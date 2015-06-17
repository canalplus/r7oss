/************************************************************************
Copyright (C) 2003-2014 STMicroelectronics. All Rights Reserved.

This file is part of the Streaming Engine.

Streaming Engine is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

Streaming Engine is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with Streaming Engine; see the file COPYING.  If not, write to the Free
Software Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307,
USA.

The Streaming Engine Library may alternatively be licensed under a
proprietary license from ST.
************************************************************************/

//#define DUMP_COMMANDS

#include "player_threads.h"

#include "codec_mme_base.h"
#include "player_generic.h"
#include "player_playback.h"
#include "player_stream.h"
#include "parse_to_decode_edge.h"

#undef TRACE_TAG
#define TRACE_TAG "Codec_MmeBase_c"

// //////////////////////////////////////////////////////////////////////////
// Debugging support
// Action : register tuneable debug controls
// Input  : /debug/havana/audio_decoder_enable_crc
// Input  : /debug/havana/audio_decoder_upmix_mono2stereo
// ////////////////////////////////////////////////////////////////////////
unsigned int volatile Codec_c::EnableAudioCRC = 0;
unsigned int volatile Codec_c::UpmixMono2Stereo = 0;

// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      C wrapper for the MME callback
//

typedef void (*MME_GenericCallback_t)(MME_Event_t Event, MME_Command_t *CallbackData, void *UserData);

static void MMECallbackStub(MME_Event_t      Event,
                            MME_Command_t   *CallbackData,
                            void            *UserData)
{
    Codec_MmeBase_c         *Self = (Codec_MmeBase_c *)UserData;
    Self->CallbackFromMME(Event, CallbackData);
}

//
Codec_MmeBase_c::Codec_MmeBase_c(void)
    : Lock()
    , Configuration()
    , SelectedTransformer(0)
    , ForceStreamParameterReload(false)
    , BufferManager(NULL)
    , DataTypesInitialized(false)
    , MMEInitialized(false)
    , MMEHandle(NULL)
    , MMEInitializationParameters()
    , MMECommandPreparedCount(0)
    , MMECommandAbortedCount(0)
    , MMECommandCompletedCount(0)
    , MMECallbackPriorityBoosted(false)
    , CodedFrameBufferPool(NULL)
    , CodedFrameBufferType(0)
    , DecodeBufferPool(NULL)
    , DecodeBufferCount(0)
    , mOutputPort(NULL)
    , StreamParameterContextPool(NULL)
    , StreamParameterContextDescriptor(NULL)
    , StreamParameterContextType(0)
    , StreamParameterContextBuffer(NULL)
    , DecodeContextPool(NULL)
    , DecodeContextDescriptor(NULL)
    , DecodeContextType(0)
    , DecodeContextBuffer(NULL)
    , IndexBufferMapSize(0)
    , IndexBufferMap(NULL)
    , CodedFrameBuffer(NULL)
    , CodedDataLength(0)
    , CodedData(NULL)
    , CodedData_Cp(NULL)
    , ParsedFrameParameters(NULL)
    , CurrentDecodeBufferIndex(INVALID_INDEX)
    , CurrentDecodeBuffer(NULL)
    , CurrentDecodeIndex(INVALID_INDEX)
    , StreamParameterContext(NULL)
    , DecodeContext(NULL)
    , MarkerBuffer(NULL)
    , PassOnMarkerBufferAt(0)
    , DiscardDecodesUntil(0)
    , DecodeTimeShortIntegrationPeriod(0)
    , DecodeTimeLongIntegrationPeriod(0)
    , NextDecodeTime(0)
    , NextDecodeTimeForIOnly(0)
    , LastDecodeCompletionTime(INVALID_TIME)
    , DecodeTimes()
    , DecodeTimesForIOnly()
    , ShortTotalDecodeTime(0)
    , LongTotalDecodeTime(0)
    , ShortTotalDecodeTimeForIOnly(0)
    , LongTotalDecodeTimeForIOnly(0)
    , ReportedMissingReferenceFrame(false)
    , ReportedUsingReferenceFrameSubstitution(false)
    , IsLowPowerMMEInitialized(false)
{
    OS_InitializeMutex(&Lock);

    //
    // Fill out non trivial inits for the configuration record
    // (for which default ctor was already called)
    //
    strncpy(Configuration.TranscodedMemoryPartitionName, Configuration.CodecName,
            sizeof(Configuration.TranscodedMemoryPartitionName));
    Configuration.TranscodedMemoryPartitionName[sizeof(Configuration.TranscodedMemoryPartitionName) - 1] = '\0';

    strncpy(Configuration.AncillaryMemoryPartitionName, Configuration.CodecName,
            sizeof(Configuration.AncillaryMemoryPartitionName));
    Configuration.AncillaryMemoryPartitionName[sizeof(Configuration.AncillaryMemoryPartitionName) - 1] = '\0';

    Configuration.ListOfDecodeBufferComponents[0].Type                                = UndefinedComponent;
    Configuration.StreamParameterContextCount                                         = 1;
    Configuration.DecodeContextCount                                                  = 4;
    Configuration.AudioVideoDataParsedParametersType                                  = NOT_SPECIFIED;
    Configuration.SizeOfAudioVideoDataParsedParameters                                = NOT_SPECIFIED;
    Configuration.AddressingMode                                                      = PhysicalAddress;
    Configuration.ShrinkCodedDataBuffersAfterDecode                                   = true;
    Configuration.TrickModeParameters.EmpiricalMaximumDecodeFrameRateShortIntegration = 1024;
    Configuration.TrickModeParameters.EmpiricalMaximumDecodeFrameRateLongIntegration  = 1024;
    Configuration.TrickModeParameters.DecodeFrameRateShortIntegrationForIOnly         = 1024;
    Configuration.TrickModeParameters.DecodeFrameRateLongIntegrationForIOnly          = 1024;
    Configuration.TrickModeParameters.SubstandardDecodeSupported                      = false;
    Configuration.TrickModeParameters.SubstandardDecodeRateIncrease                   = 1;
    Configuration.TrickModeParameters.DefaultGroupSize                                = 1;
    Configuration.TrickModeParameters.DefaultGroupReferenceFrameCount                 = 0;

    for (int i = 0; i < MAX_DECODE_BUFFERS; i++)
    {
        BufferState[i].BufferQuality = 1;    // rational
    }
}

//
Codec_MmeBase_c::~Codec_MmeBase_c(void)
{
    // TODO(pht) move halt here: shall not rely on callers

    //
    // Delete the decode and stream parameter contexts
    //
    if (DecodeContextPool != NULL)
    {
        BufferManager->DestroyPool(DecodeContextPool);
    }

    if (StreamParameterContextPool != NULL)
    {
        BufferManager->DestroyPool(StreamParameterContextPool);
    }

    //
    // Free the indexing map
    //
    if (IndexBufferMap != NULL)
    {
        delete[] IndexBufferMap;
    }

    OS_TerminateMutex(&Lock);
}

// /////////////////////////////////////////////////////////////////////////
//
//      The Halt function, give up access to any registered resources
//
//      NOTE for some calls we ignore the return statuses, this is because
//      we will proceed with the halt even if we fail (what else can we do)
//

CodecStatus_t   Codec_MmeBase_c::Halt(void)
{
    Buffer_t LocalMarkerBuffer;

    if (TestComponentState(ComponentRunning))
    {
        //
        // Move the base state to halted early, to ensure
        // no activity can be queued once we start halting
        //
        BaseComponentClass_c::Halt();
        //
        // Terminate any partially accumulated buffers
        //
        Codec_MmeBase_c::OutputPartialDecodeBuffers();
        //
        // Terminate the MME transform, this will involve waiting
        // for all the currently queued transactions to complete.
        //
        TerminateMMETransformer();
        //
        // Pass on any marker buffer
        //
        LocalMarkerBuffer = TakeMarkerBuffer();
        OS_LockMutex(&Lock);

        if (LocalMarkerBuffer != NULL)
        {
            MarkerBuffer        = NULL;
        }

        OS_UnLockMutex(&Lock);
        //
        // Lose the output ring
        //
        DecodeBufferPool                = NULL;
        CodedFrameBufferPool            = NULL;
        mOutputPort                     = NULL;
    }

    return CodecNoError;
}

// /////////////////////////////////////////////////////////////////////////
//
//      SetModuleParameters function
//      Action  : Allows external user to set up important environmental parameters
//      Input   :
//      Output  :
//      Result  :
//

CodecStatus_t   Codec_MmeBase_c::SetModuleParameters(
    unsigned int   ParameterBlockSize,
    void          *ParameterBlock)
{
    struct CodecParameterBlock_s  *CodecParameterBlock = (struct CodecParameterBlock_s *)ParameterBlock;

    if (ParameterBlockSize != sizeof(struct CodecParameterBlock_s))
    {
        SE_ERROR("Invalid parameter block\n");
        return CodecError;
    }

    switch (CodecParameterBlock->ParameterType)
    {
    case CodecSelectTransformer:
    {
        if (CodecParameterBlock->Transformer >= Configuration.AvailableTransformers)        // TODO ask transformers
        {
            SE_ERROR("Invalid transformer id (%d >= %d)\n", CodecParameterBlock->Transformer, Configuration.AvailableTransformers);
            return CodecError;
        }

        SelectedTransformer         = CodecParameterBlock->Transformer;
        // Adjust SelectedTransformer if required
        // that is if selected CPU doesn't support the codec but another does.
        CodecStatus_t Status = GloballyVerifyMMECapabilities();
        if (Status != CodecNoError)
        {
            return Status;    // sub-routine puts errors to console
        }

        SE_INFO(GetGroupTrace(), "Setting selected transformer to %d\n", SelectedTransformer);
        break;
    }

    default:
        SE_ERROR("Unrecognised parameter block (%d)\n", CodecParameterBlock->ParameterType);
        return CodecError;
    }

    return  CodecNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
//      Retrieve trickmode parameters
//

CodecStatus_t   Codec_MmeBase_c::GetTrickModeParameters(CodecTrickModeParameters_t     *TrickModeParameters)
{
    memcpy(TrickModeParameters, &Configuration.TrickModeParameters, sizeof(CodecTrickModeParameters_t));
    return CodecNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
//      Connect output port function
//

CodecStatus_t   Codec_MmeBase_c::Connect(Port_c *Port)
{
    // TODO(pht) proper unallocations rollback on errors

    if (Port == NULL)
    {
        SE_ERROR("Incorrect parameter\n");
        return CodecError;
    }
    if (mOutputPort != NULL)
    {
        SE_WARNING("Port already connected\n");
    }

    mOutputPort  = Port;
    //
    // Obtain the class list
    //
    Player->GetBufferManager(&BufferManager);
    //
    // Initialize the data types we use.
    //
    CodecStatus_t CodecStatus = InitializeDataTypes();
    if (CodecStatus != CodecNoError)
    {
        return CodecStatus;
    }

    CodedFrameBufferPool = Stream->GetCodedFrameBufferPool();

    CodedFrameBufferPool->GetType(&CodedFrameBufferType);
    //
    // Obtain the decode buffer pool
    //
    Player->GetDecodeBufferPool(Stream, &DecodeBufferPool);

    if (DecodeBufferPool == NULL)
    {
        SE_ERROR("(%s) - Unable to obtain decode buffer pool\n", Configuration.CodecName);
        SetComponentState(ComponentInError);
        return PlayerError;
    }

    DecodeBufferPool->GetPoolUsage(&DecodeBufferCount, NULL, NULL, NULL, NULL);
    //
    // Create the mapping between decode indices and decode buffers
    //
    OS_LockMutex(&Lock);
    IndexBufferMapSize  = DecodeBufferCount * Configuration.MaxDecodeIndicesPerBuffer;
    IndexBufferMap      = new CodecIndexBufferMap_t[IndexBufferMapSize];
    if (IndexBufferMap == NULL)
    {
        SE_ERROR("(%s) - Failed to allocate DecodeIndex <=> Buffer map\n", Configuration.CodecName);
        SetComponentState(ComponentInError);
        OS_UnLockMutex(&Lock);
        return CodecError;
    }

    memset(IndexBufferMap, 0xff, IndexBufferMapSize * sizeof(CodecIndexBufferMap_t));
    OS_UnLockMutex(&Lock);
    //
    // Attach the stream specific (audio|video|data)
    // parsed frame parameters to the decode buffer pool.
    //
    BufferStatus_t BufStatus = DecodeBufferPool->AttachMetaData(Configuration.AudioVideoDataParsedParametersType);
    if (BufStatus != BufferNoError)
    {
        SE_ERROR("(%s) - Failed to attach stream specific parsed parameters to all decode buffers\n", Configuration.CodecName);
        SetComponentState(ComponentInError);
        return CodecError;
    }

    //
    // Create the stream parameters context buffers
    //
    BufStatus = BufferManager->CreatePool(&StreamParameterContextPool, StreamParameterContextType, Configuration.StreamParameterContextCount);
    if (BufStatus != BufferNoError)
    {
        SE_ERROR("(%s) - Failed to create a pool of stream parameter context buffers\n", Configuration.CodecName);
        SetComponentState(ComponentInError);
        return CodecError;
    }

    StreamParameterContextBuffer        = NULL;
    //
    // Now create the decode context buffers
    //
    BufStatus = BufferManager->CreatePool(&DecodeContextPool, DecodeContextType, Configuration.DecodeContextCount);
    if (BufStatus != BufferNoError)
    {
        SE_ERROR(" (%s) - Failed to create a pool of decode context buffers\n", Configuration.CodecName);
        SetComponentState(ComponentInError);
        return CodecError;
    }

    DecodeContextBuffer         = NULL;
    //
    // Start the mme transformer
    //
    CodecStatus_t Status = InitializeMMETransformer();
    if (Status != CodecNoError)
    {
        SE_ERROR(" (%s) - Failed to start the mme transformer\n", Configuration.CodecName);
        SetComponentState(ComponentInError);
        return Status;
    }

    // NOTE: the implementation taken is the one in codec mme audio spdif class!
    Status = CreateAttributeEvents();
    if (Status != CodecNoError)
    {
        SE_ERROR(" (%s) - Failed to create attribute events\n", Configuration.CodecName);
        SetComponentState(ComponentInError);
        return Status;
    }

    //
    // Initialize the decode buffer manager to the configuration list of buffer components
    //
    DecodeBufferManagerStatus_t DbmStatus = Stream->GetDecodeBufferManager()->InitializeComponentList(Configuration.ListOfDecodeBufferComponents);
    if (DbmStatus != PlayerNoError)
    {
        SE_ERROR(" (%s) - Failed to initialize decode buffer component list\n", Configuration.CodecName);
        SetComponentState(ComponentInError);
        return DbmStatus;
    }

    //
    // Go live
    //
    SetComponentState(ComponentRunning);
    return CodecNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
//      Release any partially decoded buffers
//

CodecStatus_t   Codec_MmeBase_c::OutputPartialDecodeBuffers(void)
{
    if (ComponentState == ComponentHalted)
    {
        if (CurrentDecodeBufferIndex != INVALID_INDEX)
        {
            SE_WARNING("halted state with valid DecodeBufferIndex:%d  dip:%d\n",
                       CurrentDecodeBufferIndex, BufferState[CurrentDecodeBufferIndex].DecodesInProgress);
        }
        return PlayerError; // as would do the AssertComponentState below
    }

    AssertComponentState(ComponentRunning);
    OS_LockMutex(&Lock);

    if (CurrentDecodeBufferIndex != INVALID_INDEX)
    {
        // Operation cannot fail
        SetOutputOnDecodesComplete(CurrentDecodeBufferIndex, true);
        CurrentDecodeBufferIndex        = INVALID_INDEX;
    }

    OS_UnLockMutex(&Lock);
    return CodecNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
//      Set the discard pointer to discard any queued decodes
//

CodecStatus_t   Codec_MmeBase_c::DiscardQueuedDecodes(void)
{

    CurrentDecodeIndex = INVALID_INDEX;
    DiscardDecodesUntil = MMECommandPreparedCount;
    return CodecNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
//      Release a reference frame
//

CodecStatus_t   Codec_MmeBase_c::ReleaseReferenceFrame(unsigned int    ReferenceFrameDecodeIndex)
{
    unsigned int    i;
    unsigned int    Index;

    OS_LockMutex(&Lock);

    //
    // Deal with the generic case (release all)
    //

    if (ReferenceFrameDecodeIndex == CODEC_RELEASE_ALL)
    {
        for (i = 0; i < DecodeBufferCount; i++)
        {
            if (BufferState[i].ReferenceFrameCount != 0)
            {
                while (BufferState[i].ReferenceFrameCount != 0)
                {
                    BufferState[i].ReferenceFrameCount--;
                    DecrementAccessCount(i, true);
                }
            }
        }
    }
    //
    // Deal with the specific case
    //
    else
    {
        CodecStatus_t Status = TranslateDecodeIndex(ReferenceFrameDecodeIndex, &Index);
        if ((Status != CodecNoError) || (BufferState[Index].ReferenceFrameCount == 0))
        {
            // It is legal to receive a release command for a frame we have never seen
            // this is because a frame may be discarded or dropped, between the frame
            // parser and us.
            OS_UnLockMutex(&Lock);
            return CodecNoError;
        }

        BufferState[Index].ReferenceFrameCount--;
        DecrementAccessCount(Index, true);
    }

    OS_UnLockMutex(&Lock);

    return CodecNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
//      Check that we have all of the reference frames mentioned
//      in a reference frame list.
//

CodecStatus_t   Codec_MmeBase_c::CheckReferenceFrameList(
    unsigned int              NumberOfReferenceFrameLists,
    ReferenceFrameList_t      ReferenceFrameList[])
{
    unsigned int      i, j;
    unsigned int      BufferIndex;

    OS_LockMutex(&Lock);

    for (i = 0; i < NumberOfReferenceFrameLists; i++)
    {
        for (j = 0; j < ReferenceFrameList[i].EntryCount; j++)
        {
            CodecStatus_t Status = TranslateDecodeIndex(ReferenceFrameList[i].EntryIndicies[j], &BufferIndex);
            if (Status != CodecNoError)
            {
                OS_UnLockMutex(&Lock);
                return CodecUnknownFrame;
            }
        }
    }

    OS_UnLockMutex(&Lock);

    return CodecNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
//      Release a decode buffer
//

CodecStatus_t   Codec_MmeBase_c::ReleaseDecodeBuffer(Buffer_t Buffer)
{
    OS_LockMutex(&Lock);
    CodecStatus_t Status = ReleaseDecodeBufferLocked(Buffer);
    OS_UnLockMutex(&Lock);
    return Status;
}

//      This function must be mutex-locked by caller
CodecStatus_t   Codec_MmeBase_c::ReleaseDecodeBufferLocked(Buffer_t          Buffer)
{
    unsigned int    Index;
    CodecStatus_t   Status;

    OS_AssertMutexHeld(&Lock);

    Buffer->GetIndex(&Index);

    Status = DecrementAccessCount(Index, false);

    //
    // If the mapping was no longer extant, then we just release the buffer
    //

    if (Status != CodecNoError)
    {
        Buffer->DecrementReferenceCount();
    }

//
    return CodecNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
//      The get coded frame buffer pool fn
//

CodecStatus_t   Codec_MmeBase_c::Input(Buffer_t          CodedBuffer)
{
    CodecStatus_t             Status;
    unsigned int              StreamParameterContextSize;
    unsigned int              DecodeContextSize;
    Buffer_t                  OldMarkerBuffer;
    Buffer_t                  LocalMarkerBuffer;
    DecodeBufferRequest_t     BufferRequest;
    //
    // Initialize context pointers
    //
    CodedFrameBuffer                                            = NULL;
    CodedData                                                   = NULL;
    CodedDataLength                                             = 0;
    ParsedFrameParameters                                       = NULL;
    *Configuration.AudioVideoDataParsedParametersPointer        = NULL;
    //
    // Obtain pointers to data associated with the buffer.
    //
    CodedFrameBuffer    = CodedBuffer;
    Stream->DecodeCommenceTime = OS_GetTimeInMicroSeconds();

    if (!Configuration.IgnoreFindCodedDataBuffer)
    {
        CodedDataLength     = 0;
        CodedData       = NULL;

        BufferStatus_t BufStatus = CodedFrameBuffer->ObtainDataReference(NULL, &CodedDataLength, (void **)(&CodedData), Configuration.AddressingMode);
        if ((BufStatus != BufferNoError) && (BufStatus != BufferNoDataAttached))
        {
            SE_ERROR("(%s) - Unable to obtain data reference\n", Configuration.CodecName);
            return CodecError;
        }

        BufStatus = CodedFrameBuffer->ObtainDataReference(NULL, &CodedDataLength, (void **)(&CodedData_Cp), CachedAddress);
        if ((BufStatus != BufferNoError) && (BufStatus != BufferNoDataAttached))
        {
            SE_ERROR("(%s) - Unable to obtain data reference for Cached CodedData buffer @\n", Configuration.CodecName);
            return CodecError;
        }
    }

    CodedFrameBuffer->ObtainMetaDataReference(Player->MetaDataParsedFrameParametersType, (void **)(&ParsedFrameParameters));
    SE_ASSERT(ParsedFrameParameters != NULL);

    CodedFrameBuffer->ObtainMetaDataReference(Configuration.AudioVideoDataParsedParametersType, Configuration.AudioVideoDataParsedParametersPointer);
    SE_ASSERT(Configuration.AudioVideoDataParsedParametersPointer != NULL);

    //
    // Handle the special case of a marker frame
    //

    /* In StreamBase decoder EofMarker buffer need to be send to the FW.
       So that FW can return Eof Marker after consuming entire input */

    if ((CodedDataLength == 0) && !ParsedFrameParameters->NewStreamParameters && !ParsedFrameParameters->NewFrameParameters && !ParsedFrameParameters->EofMarkerFrame)
    {
        //
        // Test we don't already have one
        //
        OldMarkerBuffer = TakeMarkerBuffer();

        if (OldMarkerBuffer != NULL)
        {
            SE_ERROR("(%s) - Marker frame recognized when holding a marker frame already\n", Configuration.CodecName);
            OldMarkerBuffer->DecrementReferenceCount(IdentifierCodec);
        }

        //
        // Get a marker buffer
        //
        FillOutDecodeBufferRequest(&BufferRequest);
        BufferRequest.MarkerFrame   = true;
        Status = Stream->GetDecodeBufferManager()->GetDecodeBuffer(&BufferRequest, &LocalMarkerBuffer);
        if (Status != CodecNoError)
        {
            SE_ERROR("(%s) - Failed to get marker decode buffer from decode buffer manager\n", Configuration.CodecName);
            return Status;
        }

        LocalMarkerBuffer->TransferOwnership(IdentifierCodec);
        BufferStatus_t BufStatus = LocalMarkerBuffer->AttachMetaData(Player->MetaDataParsedFrameParametersReferenceType, UNSPECIFIED_SIZE, (void *)ParsedFrameParameters);
        if (BufStatus != BufferNoError)
        {
            SE_ERROR("(%s) - Unable to attach a reference to \"ParsedFrameParameters\" to the marker buffer\n", Configuration.CodecName);
            return CodecError;
        }

        LocalMarkerBuffer->AttachBuffer(CodedFrameBuffer);
        //
        // Queue/pass on the buffer
        //
        PassOnMarkerBufferAt    = MMECommandPreparedCount;
        MarkerBuffer            = LocalMarkerBuffer;
        TestMarkerFramePassOn();
        return CodecNoError;
    }

    //
    // Adjust the coded data to take account of the offset
    //

    if (!Configuration.IgnoreFindCodedDataBuffer)
    {
        CodedData               += ParsedFrameParameters->DataOffset;
        CodedDataLength         -= ParsedFrameParameters->DataOffset;
    }

    //
    // Check that the decode index is monotonically increasing
    //

    if ((CurrentDecodeIndex != INVALID_INDEX) &&
        ((ParsedFrameParameters->DecodeFrameIndex < CurrentDecodeIndex) ||
         (!Configuration.SliceDecodePermitted && (ParsedFrameParameters->DecodeFrameIndex == CurrentDecodeIndex))))
    {
        SE_ERROR("(%s) - Decode indices must be monotonically increasing (%d vs %d)\n", Configuration.CodecName, ParsedFrameParameters->DecodeFrameIndex, CurrentDecodeIndex);
        return PlayerImplementationError;
    }

    //
    // If new stream parameters, the obtain a stream parameters context
    //

    if (ParsedFrameParameters->NewStreamParameters || ForceStreamParameterReload)
    {
        ParsedFrameParameters->NewStreamParameters = true;
        ForceStreamParameterReload      = false;

        if (StreamParameterContextBuffer != NULL)
        {
            SE_ERROR("(%s) - We already have a stream parameter context\n", Configuration.CodecName);
        }
        else
        {
            BufferStatus_t BufStatus = StreamParameterContextPool->GetBuffer(&StreamParameterContextBuffer);
            if (BufStatus != BufferNoError)
            {
                SE_ERROR("(%s) - Failed to get stream parameter context\n", Configuration.CodecName);
                return CodecError;
            }

            StreamParameterContextBuffer->ObtainDataReference(&StreamParameterContextSize, NULL, (void **)&StreamParameterContext);
            SE_ASSERT(StreamParameterContext != NULL); // not expected to be empty
            memset(StreamParameterContext, 0, StreamParameterContextSize);
            StreamParameterContext->StreamParameterContextBuffer        = StreamParameterContextBuffer;
        }
    }

    //
    // If a new frame is present obtain a decode context
    //
    /* In StreamBase decoder EofMarker buffer need to be send to the FW.
       So that FW can return Eof Marker after consuming entire input */

    if (ParsedFrameParameters->NewFrameParameters || ParsedFrameParameters->EofMarkerFrame)
    {
        if (DecodeContextBuffer != NULL)
        {
            SE_ERROR("(%s) - We already have a decode context\n", Configuration.CodecName);
        }
        else
        {
            BufferStatus_t BufStatus = DecodeContextPool->GetBuffer(&DecodeContextBuffer);
            if (BufStatus != BufferNoError)
            {
                SE_ERROR("(%s) - Failed to get decode context\n", Configuration.CodecName);
                return CodecError;
            }

            DecodeContextBuffer->ObtainDataReference(&DecodeContextSize, NULL, (void **)&DecodeContext);
            SE_ASSERT(DecodeContext != NULL); // not expected to be empty

            memset(DecodeContext, 0, DecodeContextSize);
            DecodeContext->BufferIndex = INVALID_INDEX;
            DecodeContext->DecodeContextBuffer  = DecodeContextBuffer;
            DecodeContext->DecodeQuality    = 1;
        }
    }

    //
    // If a new frame is present and its key, reset reported flags
    //

    if (ParsedFrameParameters->NewFrameParameters &&
        ParsedFrameParameters->FirstParsedParametersForOutputFrame &&
        ParsedFrameParameters->KeyFrame)
    {
        ReportedMissingReferenceFrame       = false;
        ReportedUsingReferenceFrameSubstitution = false;
    }

    Stream->Statistics().FrameCountToCodec++;
    return CodecNoError;
}

// /////////////////////////////////////////////////////////////////////////
//
// Low power enter method
// For CPS mode, all MME transformers must be terminated
//
CodecStatus_t   Codec_MmeBase_c::LowPowerEnter(void)
{
    CodecStatus_t CodecStatus = CodecNoError;
    // Terminate MME transformer if needed
    IsLowPowerMMEInitialized = MMEInitialized;

    if (IsLowPowerMMEInitialized)
    {
        CodecStatus = TerminateMMETransformer();
        SE_DEBUG(GetGroupTrace(), "  Calling ics_region_remove at low power enter\n");
        AllocatorRemoveMapEx((Stream->CodedFrameMemoryDevice)->UnderlyingDevice);
        Stream->GetDecodeBufferManager()->ResetDecoderMap();
        BufferManager->ResetIcsMap();
    }

    return CodecStatus;
}

// /////////////////////////////////////////////////////////////////////////
//
// Low power exit method
// For CPS mode, all MME transformers must be re-initialized
//
CodecStatus_t   Codec_MmeBase_c::LowPowerExit(void)
{
    CodecStatus_t CodecStatus = CodecNoError;

    // Re-initialize MME transformer if needed
    if (IsLowPowerMMEInitialized)
    {
        MME_ERROR MMEStatus;

        SE_DEBUG(GetGroupTrace(), "  Forcing reload of stream parameters\n");
        ForceStreamParameterReload      = true;
        SE_DEBUG(GetGroupTrace(), "  Calling ics_region_add at low power exit\n");
        AllocatorCreateMapEx((Stream->CodedFrameMemoryDevice)->UnderlyingDevice);
        Stream->GetDecodeBufferManager()->CreateDecoderMap();
        BufferManager->CreateIcsMap();
        // We cannot use the generic InitializeMMETransformer() method for 2 reasons:
        // - for some codecs (FLV1, DIVX, DIVX_HD), this method is overloaded and do not call MME_InitTransformer(), but only MME_GetTransformerCapability()
        // - for some other codecs (VP6, VP8, RMV), MME transformer term/init is called afterward on SendMMEStreamParameters(), overriding the initial config
        // Instead we make direct call to MME_InitTransformer() with the already filled MMEInitializationParameters struct.
        MMEStatus = MME_InitTransformer(Configuration.TransformName[SelectedTransformer], &MMEInitializationParameters, &MMEHandle);

        if (MMEStatus != MME_SUCCESS)
        {
            SE_ERROR("Error returned by MME_InitTransformer()\n");
            CodecStatus = CodecError;
        }
        else
        {
            MMEInitialized = true;
        }
    }

    return CodecStatus;
}

// /////////////////////////////////////////////////////////////////////////
//
//      Protected - The mapping/translation functions for decode indices
//
//      This function must be mutex-locked by caller
//
CodecStatus_t   Codec_MmeBase_c::MapBufferToDecodeIndex(
    unsigned int      DecodeIndex,
    unsigned int      BufferIndex)
{
    unsigned int    i;
    OS_AssertMutexHeld(&Lock);

    for (i = 0; i < IndexBufferMapSize; i++)
        if (IndexBufferMap[i].DecodeIndex == INVALID_INDEX)
        {
            IndexBufferMap[i].DecodeIndex       = DecodeIndex;
            IndexBufferMap[i].BufferIndex       = BufferIndex;
            return CodecNoError;
        }

//
    SE_ERROR("(%s) - Map table full, implementation error\n", Configuration.CodecName);
    return PlayerImplementationError;
}

// ---------------
//
//      This function must be mutex-locked by caller
//
CodecStatus_t   Codec_MmeBase_c::UnMapBufferIndex(unsigned int      BufferIndex)
{
    unsigned int    i;
    OS_AssertMutexHeld(&Lock);

    //
    // Do not break out on finding the buffer, because it is perfectly legal
    // to have more than one decode index associated with a buffer.
    //

    for (i = 0; i < IndexBufferMapSize; i++)
        if (IndexBufferMap[i].BufferIndex == BufferIndex)
        {
            IndexBufferMap[i].DecodeIndex       = INVALID_INDEX;
            IndexBufferMap[i].BufferIndex       = INVALID_INDEX;
        }

    return CodecNoError;
}

// ---------------
//
//      This function must be mutex-locked by caller
//
CodecStatus_t   Codec_MmeBase_c::TranslateDecodeIndex(
    unsigned int      DecodeIndex,
    unsigned int     *BufferIndex)
{
    unsigned int    i;
    OS_AssertMutexHeld(&Lock);

    for (i = 0; i < IndexBufferMapSize; i++)
        if (IndexBufferMap[i].DecodeIndex == DecodeIndex)
        {
            *BufferIndex        = IndexBufferMap[i].BufferIndex;
            return CodecNoError;
        }

    *BufferIndex        = INVALID_INDEX;
    return CodecUnknownFrame;
}


// /////////////////////////////////////////////////////////////////////////
//
//      Function control moving of buffers to output ring, sets the flag
//      indicating there will be no further decodes into this buffer,
//      and optionally checks to see if it should be immediately moved
//      to the output ring.
//
//      This function must be mutex-locked by caller
//
CodecStatus_t   Codec_MmeBase_c::SetOutputOnDecodesComplete(
    unsigned int      BufferIndex,
    bool              TestForImmediateOutput)
{
    OS_AssertMutexHeld(&Lock);

    if (!TestForImmediateOutput)
    {
        BufferState[BufferIndex].OutputOnDecodesComplete        = true;
    }
    else
    {
        BufferState[BufferIndex].OutputOnDecodesComplete        = true;

        if (BufferState[BufferIndex].DecodesInProgress == 0)
        {
            Stream->GetDecodeBufferManager()->ReleaseBufferComponents(
                BufferState[BufferIndex].Buffer, ForDecode);
            mOutputPort->Insert((uintptr_t)BufferState[BufferIndex].Buffer);
            Stream->Statistics().FrameCountFromCodec++;
        }
    }

    return CodecNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
//      Function to obtain a new decode buffer.
//
//      This function must be mutex-locked by caller
//
CodecStatus_t   Codec_MmeBase_c::GetDecodeBuffer(void)
{
    DecodeBufferRequest_t    BufferRequest;
    OS_AssertMutexHeld(&Lock);
    //
    // Get a buffer
    //
    CodecStatus_t Status = FillOutDecodeBufferRequest(&BufferRequest);
    if (Status != CodecNoError)
    {
        SE_ERROR("(%s) - Failed to fill out a buffer request structure\n", Configuration.CodecName);
        return Status;
    }

    OS_UnLockMutex(&Lock);   // FIXME: UGLY.
    BufferStatus_t BufStatus = Stream->GetDecodeBufferManager()->GetDecodeBuffer(&BufferRequest, &CurrentDecodeBuffer);
    OS_LockMutex(&Lock);   // FIXME: UGLY.

    if (BufStatus != BufferNoError)
    {
        SE_ERROR("(%s) - Failed to obtain a decode buffer from the decode buffer manager\n", Configuration.CodecName);
        return CodecError;
    }

    CurrentDecodeBuffer->TransferOwnership(IdentifierCodec);
    //
    // Map it and initialize the mapped entry.
    //
    CurrentDecodeBuffer->GetIndex(&CurrentDecodeBufferIndex);

    if (CurrentDecodeBufferIndex >= MAX_DECODE_BUFFERS)
    {
        SE_FATAL(" (%s) - Decode buffer index %d >= MAX_DECODE_BUFFERS - Implementation error\n", Configuration.CodecName, CurrentDecodeBufferIndex);
        return CodecError;
    }

    Status = MapBufferToDecodeIndex(ParsedFrameParameters->DecodeFrameIndex, CurrentDecodeBufferIndex);
    if (Status != CodecNoError)
    {
        return Status;
    }

    memset(&BufferState[CurrentDecodeBufferIndex], 0, sizeof(CodecBufferState_t));
    BufferState[CurrentDecodeBufferIndex].Buffer                        = CurrentDecodeBuffer;
    BufferState[CurrentDecodeBufferIndex].OutputOnDecodesComplete       = false;
    BufferState[CurrentDecodeBufferIndex].DecodesInProgress             = 0;
    BufferState[CurrentDecodeBufferIndex].BufferQualityWeight           = 0;
    BufferState[CurrentDecodeBufferIndex].BufferQuality                 = 1; // rational
    BufferState[CurrentDecodeBufferIndex].ReferenceFrameSlot            = INVALID_INDEX;
    //
    // Obtain the interesting references to the buffer
    //
    BufStatus = CurrentDecodeBuffer->AttachMetaData(Player->MetaDataParsedFrameParametersReferenceType, UNSPECIFIED_SIZE, (void *)ParsedFrameParameters);
    if (BufStatus != BufferNoError)
    {
        SE_ERROR("(%s) - Unable to attach a reference to \"ParsedFrameParameters\" to the decode buffer\n", Configuration.CodecName);
        return CodecError;
    }

    CurrentDecodeBuffer->ObtainMetaDataReference(Configuration.AudioVideoDataParsedParametersType,
                                                 (void **)(&BufferState[CurrentDecodeBufferIndex].AudioVideoDataParsedParameters));
    SE_ASSERT(BufferState[CurrentDecodeBufferIndex].AudioVideoDataParsedParameters != NULL);

    //
    // We attached a reference to the parsed frame parameters for the first coded frame
    // We actually copy over the parsed stream specific parameters, because we modify these
    // as several decodes affect one buffer.
    // After doing the memcpy we attach the coded frame to to the decode
    // buffer, this extends the lifetime of any data pointed to in the
    // parsed parameters to the lifetime of this decode buffer.
    //
    // We do the attachment because we have copied pointers to the original
    // (mpeg2/H264/mp3...) headers, and we wish these pointers to be valid so
    // long as the decode buffer is around.
    //
    memcpy(BufferState[CurrentDecodeBufferIndex].AudioVideoDataParsedParameters, *Configuration.AudioVideoDataParsedParametersPointer,
           Configuration.SizeOfAudioVideoDataParsedParameters);
    CurrentDecodeBuffer->AttachBuffer(CodedFrameBuffer);
//
    return CodecNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
//      Function to send a set stream parameters
//
//      This function must be mutex-locked by caller
//
CodecStatus_t   Codec_MmeBase_c::ReleaseDecodeContext(CodecBaseDecodeContext_t *Context)
{
    unsigned int i, j;

    if (Context == NULL)
    {
        return CodecError;
    }

    OS_AssertMutexHeld(&Lock);

    //
    // If the decodes in progress flag has already been
    // incremented we decrement it and test for buffer release
    //

    if (Context->DecodeInProgress)
    {
        Context->DecodeInProgress     = false;
        BufferState[Context->BufferIndex].DecodesInProgress--;

        if (BufferState[Context->BufferIndex].OutputOnDecodesComplete)
        {
            if (BufferState[Context->BufferIndex].DecodesInProgress == 0)
            {
                //
                // Buffer is complete, check for discard or place on output ring
                //
                int DiscardForDisplayQualityThreshold   = Player->PolicyValue(Playback, Stream, PolicyDiscardForManifestationQualityThreshold);
                int DiscardForReferenceQualityThreshold = Player->PolicyValue(Playback, Stream, PolicyDiscardForReferenceQualityThreshold);

                if (BufferState[Context->BufferIndex].BufferQuality < Rational_t(DiscardForReferenceQualityThreshold, 255))
                    SE_INFO(GetGroupTrace(), "Discarding frame for reference (Quality %d.%03d < %d.%03d)\n",
                            BufferState[Context->BufferIndex].BufferQuality.IntegerPart(), BufferState[Context->BufferIndex].BufferQuality.RemainderDecimal(3),
                            Rational_t(DiscardForReferenceQualityThreshold, 255).IntegerPart(), Rational_t(DiscardForReferenceQualityThreshold, 255).RemainderDecimal(3));

                if (((MMECommandAbortedCount + MMECommandCompletedCount) < DiscardDecodesUntil) ||
                    (BufferState[Context->BufferIndex].BufferQuality < Rational_t(DiscardForDisplayQualityThreshold, 255)))
                {
                    if (BufferState[Context->BufferIndex].BufferQuality < Rational_t(DiscardForDisplayQualityThreshold, 255))
                        SE_INFO(GetGroupTrace(), "Discarding frame for manifestation (Quality %d.%03d < %d.%03d)\n",
                                BufferState[Context->BufferIndex].BufferQuality.IntegerPart(), BufferState[Context->BufferIndex].BufferQuality.RemainderDecimal(3),
                                Rational_t(DiscardForDisplayQualityThreshold, 255).IntegerPart(), Rational_t(DiscardForDisplayQualityThreshold, 255).RemainderDecimal(3));

                    Buffer_t AttachedCodedDataBuffer;
                    BufferState[Context->BufferIndex].Buffer->ObtainAttachedBufferReference(CodedFrameBufferType, &AttachedCodedDataBuffer);
                    if (AttachedCodedDataBuffer != NULL)
                    {
                        ParsedFrameParameters_t *AttachedParsedFrameParameters;
                        AttachedCodedDataBuffer->ObtainMetaDataReference(Player->MetaDataParsedFrameParametersType, (void **)(&AttachedParsedFrameParameters));
                        SE_ASSERT(AttachedParsedFrameParameters != NULL);

                        // Record non decoded frame in case of no error reported so far
                        Stream->ParseToDecodeEdge->RecordNonDecodedFrame(AttachedCodedDataBuffer, AttachedParsedFrameParameters);
                    }
                    else
                    {
                        SE_ERROR("ReleaseDecodeContext - Cannot obtain attached buffer for buffer %d\n", Context->BufferIndex);
                    }

                    ReleaseDecodeBufferLocked(BufferState[Context->BufferIndex].Buffer);
                }
                else
                {
                    Stream->GetDecodeBufferManager()->ReleaseBufferComponents(
                        BufferState[Context->BufferIndex].Buffer, ForDecode);
                    mOutputPort->Insert((uintptr_t)BufferState[Context->BufferIndex].Buffer);
                    Stream->Statistics().FrameCountFromCodec++;
                }
            }

            //
            // Special case, if we are releasing a decode context due to a failure,
            // then it is possible this can result in invalidation of the current decode index
            //

            if (Context->BufferIndex == CurrentDecodeBufferIndex)
            {
                CurrentDecodeBufferIndex        = INVALID_INDEX;
            }
        }
    }

    //
    // If requested we shrink any attached coded buffer to zero size.
    //

    if (Configuration.ShrinkCodedDataBuffersAfterDecode)
    {
        Buffer_t AttachedCodedDataBuffer;
        do
        {
            Context->DecodeContextBuffer->ObtainAttachedBufferReference(CodedFrameBufferType, &AttachedCodedDataBuffer);
            if (AttachedCodedDataBuffer != NULL)
            {
                AttachedCodedDataBuffer->SetUsedDataSize(0);
                BufferStatus_t ShrinkStatus  = AttachedCodedDataBuffer->ShrinkBuffer(0);
                if (ShrinkStatus != BufferNoError)
                {
                    SE_INFO(GetGroupTrace(), "Failed to shrink buffer\n");
                }

                Context->DecodeContextBuffer->DetachBuffer(AttachedCodedDataBuffer);
            }
        }
        while (AttachedCodedDataBuffer != NULL);
    }

    for (i = 0; i < Context->NumberOfReferenceFrameLists; i++)
        for (j = 0; j < Context->ReferenceFrameList[i].EntryCount; j++)
        {
            unsigned int Index = Context->ReferenceFrameList[i].EntryIndicies[j];

            if (Index != INVALID_INDEX)
            {
                DecrementAccessCount(Index, true);
            }
        }

    //
    // Are we about to release the current context
    //

    if (Context == DecodeContext)
    {
        DecodeContextBuffer     = NULL;
        DecodeContext           = NULL;
    }

    //
    // Release the decode context
    //
    Context->DecodeContextBuffer->DecrementReferenceCount();

    return CodecNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
//      Function to translate reference frame indices,
//      and increment usage counts appropriately.
//
//      This function must be mutex-locked by caller
//
CodecStatus_t   Codec_MmeBase_c::TranslateSetOfReferenceFrameLists(bool         IncrementUseCountForReferenceFrame,
                                                                   bool            ParsedReferenceFrame,
                                                                   ReferenceFrameList_t    ParsedReferenceFrameList[MAX_REFERENCE_FRAME_LISTS])
{
    unsigned int      i, j, k;
    unsigned int      BufferIndex;
    CodecStatus_t     Status;
    unsigned int      AlternateReferenceFrameBufferIndex;
    unsigned int      AlternateReferenceFrameDecodeIndex;
    Rational_t    DiscardForReferenceQualityThreshold;
    bool          ReferenceFrameMissing;
    bool          ReferenceFrameUnusable;

    OS_AssertMutexHeld(&Lock);
    DiscardForReferenceQualityThreshold = Rational_t(Player->PolicyValue(Playback, Stream, PolicyDiscardForReferenceQualityThreshold), 255);
    DecodeContext->UsedReferenceFrameSubstitution   = false;
    DecodeContext->IndependentFrame         = ParsedFrameParameters->IndependentFrame;


    if (IncrementUseCountForReferenceFrame && ParsedReferenceFrame)
    {
        BufferState[CurrentDecodeBufferIndex].ReferenceFrameCount++;
        Stream->GetDecodeBufferManager()->IncrementReferenceUseCount(BufferState[CurrentDecodeBufferIndex].Buffer);
    }

//
    DecodeContext->NumberOfReferenceFrameLists  = 0;
    memcpy(DecodeContext->ReferenceFrameList, ParsedReferenceFrameList, ParsedFrameParameters->NumberOfReferenceFrameLists * sizeof(ReferenceFrameList_t));
    AlternateReferenceFrameDecodeIndex      = INVALID_INDEX;
    AlternateReferenceFrameBufferIndex      = INVALID_INDEX;

    for (i = 0; i < ParsedFrameParameters->NumberOfReferenceFrameLists; i++)
    {
        DecodeContext->NumberOfReferenceFrameLists++;
        DecodeContext->ReferenceFrameList[i].EntryCount = 0;

        for (j = 0; j < ParsedReferenceFrameList[i].EntryCount; j++)
        {
            Status = TranslateDecodeIndex(ParsedReferenceFrameList[i].EntryIndicies[j], &BufferIndex);
            // Do we have a usable reference frame
            ReferenceFrameMissing   = false;
            ReferenceFrameUnusable  = false;
            if (Status != CodecNoError)
            {
                ReferenceFrameMissing   = true;

                if (!ReportedMissingReferenceFrame)
                {
                    SE_ERROR("(%s) - Unable to translate reference\n", Configuration.CodecName);
                    SE_DEBUG(GetGroupTrace(), "  Missing index is %d (%d %d)\n", ParsedFrameParameters->ReferenceFrameList[i].EntryIndicies[j], ParsedFrameParameters->NumberOfReferenceFrameLists,
                             ParsedFrameParameters->ReferenceFrameList[i].EntryCount);
                    SE_DEBUG(GetGroupTrace(), "  Missing %d %d\n", ParsedFrameParameters->DecodeFrameIndex, ParsedFrameParameters->ReferenceFrameList[i].EntryIndicies[j]);
                    SE_DEBUG(GetGroupTrace(), "  Missing %d - %2d %2d %2d %2d %2d %2d %2d %2d\n", ParsedFrameParameters->ReferenceFrameList[0].EntryCount,
                             ParsedFrameParameters->ReferenceFrameList[0].EntryIndicies[0], ParsedFrameParameters->ReferenceFrameList[0].EntryIndicies[1],
                             ParsedFrameParameters->ReferenceFrameList[0].EntryIndicies[2], ParsedFrameParameters->ReferenceFrameList[0].EntryIndicies[3],
                             ParsedFrameParameters->ReferenceFrameList[0].EntryIndicies[4], ParsedFrameParameters->ReferenceFrameList[0].EntryIndicies[5],
                             ParsedFrameParameters->ReferenceFrameList[0].EntryIndicies[6], ParsedFrameParameters->ReferenceFrameList[0].EntryIndicies[7]);
                    SE_DEBUG(GetGroupTrace(), "  Missing %d - %2d %2d %2d %2d %2d %2d %2d %2d\n", ParsedFrameParameters->ReferenceFrameList[1].EntryCount,
                             ParsedFrameParameters->ReferenceFrameList[1].EntryIndicies[0], ParsedFrameParameters->ReferenceFrameList[1].EntryIndicies[1],
                             ParsedFrameParameters->ReferenceFrameList[1].EntryIndicies[2], ParsedFrameParameters->ReferenceFrameList[1].EntryIndicies[3],
                             ParsedFrameParameters->ReferenceFrameList[1].EntryIndicies[4], ParsedFrameParameters->ReferenceFrameList[1].EntryIndicies[5],
                             ParsedFrameParameters->ReferenceFrameList[1].EntryIndicies[6], ParsedFrameParameters->ReferenceFrameList[1].EntryIndicies[7]);
                    ReportedMissingReferenceFrame   = true;
                }
            }
            else if (Status == CodecNoError)
            {
                ReferenceFrameUnusable  = BufferState[BufferIndex].BufferQuality < DiscardForReferenceQualityThreshold;
            }

            if (ReferenceFrameMissing || ReferenceFrameUnusable)
            {
                //
                // Do we seek an alternate, or just get out of here
                //
                if (Player->PolicyValue(Playback, Stream, PolicyAllowReferenceFrameSubstitution) == PolicyValueDisapply)
                {
                    return CodecError;
                }

                //
                // Seek an alternate reference frame
                //

                if (AlternateReferenceFrameDecodeIndex == INVALID_INDEX)
                    for (k = 0; k < IndexBufferMapSize; k++)
                        if ((IndexBufferMap[k].DecodeIndex != INVALID_INDEX) &&
                            (BufferState[IndexBufferMap[k].BufferIndex].ReferenceFrameCount != 0) &&
                            ((AlternateReferenceFrameDecodeIndex == INVALID_INDEX) || (AlternateReferenceFrameDecodeIndex < IndexBufferMap[k].DecodeIndex)))
                        {
                            AlternateReferenceFrameDecodeIndex  = IndexBufferMap[k].DecodeIndex;
                            AlternateReferenceFrameBufferIndex  = IndexBufferMap[k].BufferIndex;
                        }

                if (AlternateReferenceFrameDecodeIndex == INVALID_INDEX)         // If we didn't find any, then use current and report error.
                {
                    if (!ReportedUsingReferenceFrameSubstitution)
                    {
                        SE_ERROR("(%s) - Alternate reference frame not found - skipping frame\n", Configuration.CodecName);
                        ReportedUsingReferenceFrameSubstitution = true;
                    }

                    return CodecError;
                }

                // FIXME shall be compared to MAX_ENTRIES_IN_REFERENCE_FRAME_LIST
                if (AlternateReferenceFrameBufferIndex >= MAX_DECODE_BUFFERS)
                {
                    SE_ERROR("(%s) - Alternate reference frame buffer index is INVALID (idx=%d, max=%d) - skipping frame\n", Configuration.CodecName,
                             AlternateReferenceFrameBufferIndex, MAX_DECODE_BUFFERS);
                    return CodecError;
                }
                if (AlternateReferenceFrameBufferIndex >= MAX_ENTRIES_IN_REFERENCE_FRAME_LIST)
                {
                    SE_WARNING("(%s) - Alternate reference frame buffer index is likely INVALID (idx=%d, maxref=%d) fixing it to max\n", Configuration.CodecName,
                               AlternateReferenceFrameBufferIndex, MAX_ENTRIES_IN_REFERENCE_FRAME_LIST);
                    AlternateReferenceFrameBufferIndex = MAX_ENTRIES_IN_REFERENCE_FRAME_LIST - 1;
                }

                if (!ReportedUsingReferenceFrameSubstitution)
                {
                    SE_ERROR("Using alternate reference frame\n");
                    ReportedUsingReferenceFrameSubstitution = true;
                }

                DecodeContext->UsedReferenceFrameSubstitution   = true;
                BufferIndex     = AlternateReferenceFrameBufferIndex;
                memcpy(&DecodeContext->ReferenceFrameList[i].ReferenceDetails[j],
                       &DecodeContext->ReferenceFrameList[i].ReferenceDetails[BufferIndex],
                       sizeof(ReferenceDetails_t));
            }

            DecodeContext->ReferenceFrameList[i].EntryCount++;
            DecodeContext->ReferenceFrameList[i].EntryIndicies[j]   = BufferIndex;
            Stream->GetDecodeBufferManager()->IncrementReferenceUseCount(BufferState[BufferIndex].Buffer);
        }
    }

//
    return CodecNoError;
}

//
//      This function must be mutex-locked by caller
//
CodecStatus_t   Codec_MmeBase_c::TranslateReferenceFrameLists(bool    IncrementUseCountForReferenceFrame)
{
    OS_AssertMutexHeld(&Lock);
    // NOTE: it locks Mutex
    return   TranslateSetOfReferenceFrameLists(IncrementUseCountForReferenceFrame,
                                               ParsedFrameParameters->ReferenceFrame,
                                               ParsedFrameParameters->ReferenceFrameList);
}

// /////////////////////////////////////////////////////////////////////////
//
//      Function to initialize all the types with the buffer manager
//

CodecStatus_t   Codec_MmeBase_c::InitializeDataType(BufferDataDescriptor_t   *InitialDescriptor,
                                                    BufferType_t             *Type,
                                                    BufferDataDescriptor_t  **ManagedDescriptor)
{
    BufferStatus_t  BufStatus;
    BufStatus = BufferManager->FindBufferDataType(InitialDescriptor->TypeName, Type);
    if (BufStatus != BufferNoError)
    {
        BufStatus  = BufferManager->CreateBufferDataType(InitialDescriptor, Type);
        if (BufStatus != BufferNoError)
        {
            SE_ERROR("(%s) - Failed to create the '%s' buffer type\n", Configuration.CodecName, InitialDescriptor->TypeName);
            return CodecError;
        }
    }

    BufferManager->GetDescriptor(*Type, (BufferPredefinedType_t)InitialDescriptor->Type, ManagedDescriptor);
    return CodecNoError;
}

// /////////////////////////////////////////////////////////////////////////
//
//      Function to initialize all the types with the buffer manager
//

CodecStatus_t   Codec_MmeBase_c::InitializeDataTypes(void)
{
    if (DataTypesInitialized)
    {
        return CodecNoError;
    }

    //
    // Stream parameters context type
    //
    CodecStatus_t Status = InitializeDataType(Configuration.StreamParameterContextDescriptor,
                                              &StreamParameterContextType, &StreamParameterContextDescriptor);
    if (Status != CodecNoError)
    {
        return Status;
    }

    //
    // Decode context type
    //
    Status = InitializeDataType(Configuration.DecodeContextDescriptor, &DecodeContextType, &DecodeContextDescriptor);
    if (Status != CodecNoError)
    {
        return Status;
    }

    DataTypesInitialized        = true;
    return CodecNoError;
}

///////////////////////////////////////////////////////////////////////////
///
/// Verify that the transformer is capable of correct operation.
///
/// This method is always called before a transformer is initialized. It may
/// also, optionally, be called from the sub-class constructor. Such classes
/// typically fail to initialize properly if their needs are not met. By failing
/// early an alternative codec (presumably with a reduced feature set) may be
/// substituted.
///
CodecStatus_t Codec_MmeBase_c::VerifyMMECapabilities(unsigned int ActualTransformer)
{
    if (Configuration.SizeOfTransformCapabilityStructure != 0)
    {
        MME_TransformerCapability_t Capability;
        memset(&Capability, 0, sizeof(MME_TransformerCapability_t));
        memset(Configuration.TransformCapabilityStructurePointer, 0, Configuration.SizeOfTransformCapabilityStructure);
        Capability.StructSize           = sizeof(MME_TransformerCapability_t);
        Capability.TransformerInfoSize  = Configuration.SizeOfTransformCapabilityStructure;
        Capability.TransformerInfo_p    = Configuration.TransformCapabilityStructurePointer;
        MME_ERROR MMEStatus = MME_GetTransformerCapability(Configuration.TransformName[ActualTransformer], &Capability);

        if (MMEStatus != MME_SUCCESS)
        {
            SE_ERROR("(%s:%s) - Unable to read capabilities (%08x)\n", Configuration.CodecName, Configuration.TransformName[ActualTransformer], MMEStatus);
            return CodecError;
        }

        SE_INFO(GetGroupTrace(), "Found %s transformer (version %x)\n", Configuration.TransformName[ActualTransformer], Capability.Version);
    }

    // Parse Capabilities (in case the selected transformer is coping with several codec)
    {
        CodecStatus_t Status = ParseCapabilities(ActualTransformer);
        if (Status != CodecNoError)
        {
            return Status;
        }
    }

    // DecodeBufferManager is still unset when GetCapability is called just after creation of the codec component.
    if (Stream && Stream->GetDecodeBufferManager())
    {
        //
        // Unless we had a positive error we always call handle capabilities,
        // so that codecs can initialize their decode buffer structures.
        //
        CodecStatus_t Status = HandleCapabilities(ActualTransformer);
        if (Status != CodecNoError)
        {
            return Status;
        }
    }

//
    return CodecNoError;
}

///////////////////////////////////////////////////////////////////////////
///
/// Verify that at least one transformer is capable of correct operation.
///
/// This method is a more relaxed version of Codec_MmeBase_c::VerifyMMECapabilities().
/// Basically we work through all the possible transformer names and report success
/// if at least one can support our operation.
///
CodecStatus_t   Codec_MmeBase_c::GloballyVerifyMMECapabilities(void)
{
    CodecStatus_t Status = CodecNoError;
    unsigned int  CapableTransformer      = 0;                      // Bit-field capturing all CPUs capable of the requested codec.
    unsigned int  FirstCapableTransformer = CODEC_MAX_TRANSFORMERS; // Which is the first CPU capable of handling requested codec

    for (unsigned int i = 0; i < Configuration.AvailableTransformers; i++)
    {
        Status = VerifyMMECapabilities(i);

        if (Status == CodecNoError)
        {
            if (FirstCapableTransformer == CODEC_MAX_TRANSFORMERS)
            {
                FirstCapableTransformer = i;
            }

            CapableTransformer     |= (1 << i);

            if (SelectedTransformer == i)
            {
                return Status;
            }
        }
    }

    // There is a transformer capable of handling the requested codec
    if (CapableTransformer)
    {
        // Confirm there is a capable transformer
        Status = CodecNoError;
        // but it is not the SelectedTransformer
        // Update the SelectedTransformer to point to the one that can.
        SelectedTransformer = FirstCapableTransformer;
    }

    return Status;
}

///////////////////////////////////////////////////////////////////////////
///
///     Function to initialize an mme transformer
///
CodecStatus_t   Codec_MmeBase_c::InitializeMMETransformer(void)
{
    MMECommandPreparedCount     = 0;
    MMECommandAbortedCount      = 0;
    MMECommandCompletedCount    = 0;
    DiscardDecodesUntil     = 0;
    //
    // Is there any capability information the caller is interested in
    //
    CodecStatus_t Status = VerifyMMECapabilities();
    if (Status != CodecNoError)
    {
        return Status;    // sub-routine puts errors to console
    }

    //
    // Handle the transformer initialization
    //
    memset(&MMEInitializationParameters, 0, sizeof(MME_TransformerInitParams_t));
    MMEInitializationParameters.Priority                        = MME_PRIORITY_NORMAL;
    MMEInitializationParameters.StructSize                      = sizeof(MME_TransformerInitParams_t);
    MMEInitializationParameters.Callback                        = &MMECallbackStub;
    MMEInitializationParameters.CallbackUserData                = this;

    Status = FillOutTransformerInitializationParameters();
    if (Status != CodecNoError)
    {
        SE_ERROR("(%s) - Failed to fill out transformer initialization parameters (%08x)\n", Configuration.CodecName, Status);
        return Status;
    }

    MME_ERROR MMEStatus = MME_InitTransformer(Configuration.TransformName[SelectedTransformer], &MMEInitializationParameters, &MMEHandle);
    if (MMEStatus != MME_SUCCESS)
    {
        SE_ERROR("(%s,%s) - Failed to initialize mme transformer (%08x)\n",
                 Configuration.CodecName, Configuration.TransformName[SelectedTransformer], MMEStatus);
        return CodecError;
    }

    MMEInitialized      = true;
    MMECallbackPriorityBoosted = false;
    return CodecNoError;
}


// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Terminate the mme transformer system
//

CodecStatus_t   Codec_MmeBase_c::TerminateMMETransformer(void)
{
    unsigned int            i;
    unsigned int            MaxTimeToWait;

    if (MMEInitialized)
    {
        MMEInitialized  = false;
        //
        // Wait a reasonable time for all mme transactions to terminate
        //
        MaxTimeToWait   = (Configuration.StreamParameterContextCount + Configuration.DecodeContextCount) *
                          CODEC_MAX_WAIT_FOR_MME_COMMAND_COMPLETION;

        for (i = 0; ((i < MaxTimeToWait / 10) && (MMECommandPreparedCount > (MMECommandCompletedCount + MMECommandAbortedCount))); i++)
        {
            OS_SleepMilliSeconds(10);
        }

        if (MMECommandPreparedCount > (MMECommandCompletedCount + MMECommandAbortedCount))
        {
            SE_ERROR("(%s) - Transformer failed to complete %d commands in %dms\n", Configuration.CodecName, (MMECommandPreparedCount - MMECommandCompletedCount - MMECommandAbortedCount), MaxTimeToWait);
        }

        //
        // Terminate the transformer
        //
        MME_ERROR Status = MME_TermTransformer(MMEHandle);
        if (Status != MME_SUCCESS)
        {
            SE_ERROR("(%s) - Failed to terminate mme transformer (%08x)\n", Configuration.CodecName, Status);
            return CodecError;
        }

        MMEHandle = 0;
    }

    return CodecNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
//      Function to send a set stream parameters
//

CodecStatus_t   Codec_MmeBase_c::SendMMEStreamParameters(void)
{
    //
    // Perform the generic portion of the setup
    //
    StreamParameterContext->MMECommand.StructSize                       = sizeof(MME_Command_t);
    StreamParameterContext->MMECommand.CmdCode                          = MME_SET_GLOBAL_TRANSFORM_PARAMS;
    StreamParameterContext->MMECommand.CmdEnd                           = MME_COMMAND_END_RETURN_NOTIFY;
    StreamParameterContext->MMECommand.DueTime                          = 0;                    // No Idea what to put here, I just want it done in sequence.
    StreamParameterContext->MMECommand.NumberInputBuffers               = 0;
    StreamParameterContext->MMECommand.NumberOutputBuffers              = 0;
    StreamParameterContext->MMECommand.DataBuffers_p                    = NULL;
    //
    // Check that we have not commenced shutdown.
    //
    MMECommandPreparedCount++;

    if (TestComponentState(ComponentHalted))
    {
        MMECommandAbortedCount++;
        return CodecNoError;
    }

    //
    // Do we wish to dump the mme command
    //
    DumpMMECommand(&StreamParameterContext->MMECommand);
    // Perform the mme transaction
    MME_ERROR Status = MME_SendCommand(MMEHandle, &StreamParameterContext->MMECommand);
    if (Status != MME_SUCCESS)
    {
        SE_ERROR("(%s) - Unable to send stream parameters (%08x)\n", Configuration.CodecName, Status);
        return CodecError;
    }

    StreamParameterContext              = NULL;
    StreamParameterContextBuffer        = NULL;

    return CodecNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
//      Function to send a set stream parameters
//

CodecStatus_t   Codec_MmeBase_c::SendMMEDecodeCommand(void)
{
    //
    // Perform the generic portion of the setup
    //
    DecodeContext->MMECommand.StructSize                        = sizeof(MME_Command_t);
    DecodeContext->MMECommand.CmdEnd                            = MME_COMMAND_END_RETURN_NOTIFY;
    DecodeContext->MMECommand.DueTime                           = 0;                    // No Idea what to put here, I just want it done in sequence.
    //
    // Check that we have not commenced shutdown.
    //
    MMECommandPreparedCount++;

    if (TestComponentState(ComponentHalted))
    {
        MMECommandAbortedCount++;
        return CodecNoError;
    }

    //
    // Do we wish to dump the mme command
    //
    DumpMMECommand(&DecodeContext->MMECommand);
    //
    // Perform the mme transaction
    /* In StreamBase decoder we are now sending the Marker buffer for Eof handling.
       This marker buffer has no input (zero size input buffer) so noting to do with the FlushCache */
    DecodeContextBuffer                 = NULL;
    DecodeContext->DecodeCommenceTime   = OS_GetTimeInMicroSeconds();

#ifdef LOWMEMORYBANDWIDTH
    if ((DecodeContext->MMECommand.CmdCode == MME_TRANSFORM) &&
        ((Stream->GetEncoding() == STM_SE_STREAM_ENCODING_VIDEO_HEVC) ||
         (Stream->GetEncoding() == STM_SE_STREAM_ENCODING_VIDEO_H264)))
    {
        unsigned long long t = 0;
        while (OS_TIMED_OUT == Player->getSemMemBWLimiter(15))  //TBC timeout -> half a vsync (+- sw latency)
        {
            SE_INFO(GetGroupTrace(), "(%s) - getSemMemBWLimiter timeout\n", Configuration.CodecName);
        }
        t = OS_GetTimeInMicroSeconds() - DecodeContext->DecodeCommenceTime;
        if (t > 8000)
        {
            SE_INFO(GetGroupTrace(), "(%s) - getSemMemBWLimiter > %lld us\n", Configuration.CodecName, t);
        }
    }
#endif

    MME_ERROR Status = MME_SendCommand(MMEHandle, &DecodeContext->MMECommand);
    if (Status != MME_SUCCESS)
    {
        SE_ERROR("(%s) - Unable to send decode command (%08x)\n", Configuration.CodecName, Status);
        return CodecError;
    }

    Stream->Statistics().FrameCountLaunchedDecode++;

    if (ParsedFrameParameters->KeyFrame)
    {
        Stream->Statistics().VidFrameCountLaunchedDecodeI++;
    }

    if (ParsedFrameParameters->ReferenceFrame && !ParsedFrameParameters->KeyFrame)
    {
        Stream->Statistics().VidFrameCountLaunchedDecodeP++;
    }

    if (!ParsedFrameParameters->ReferenceFrame && !ParsedFrameParameters->KeyFrame)
    {
        Stream->Statistics().VidFrameCountLaunchedDecodeB++;
    }

    DecodeContext       = NULL;
//
    return CodecNoError;
}

void Codec_MmeBase_c::GlobalParamsCommandCompleted(CodecBaseStreamParameterContext_t *StreamParameterContext)
{
    SE_VERBOSE(GetGroupTrace(), "\n");
}

// /////////////////////////////////////////////////////////////////////////
//
//      Callback function from MME
//
//      NOTE we forced the command to be the first element in each of the
//      command contexts, this allows us to find the context by simply
//      casting the command address.
//

void   Codec_MmeBase_c::CallbackFromMME(MME_Event_t Event, MME_Command_t *CallbackData)
{
    CodecBaseStreamParameterContext_t       *StreamParameterContext;
    CodecBaseDecodeContext_t                *DecodeContext;

    SE_VERBOSE(GetGroupTrace(), "\n");

    if (CallbackData == NULL)
    {
        SE_ERROR("(%s) - No CallbackData\n", Configuration.CodecName);
        return;
    }

    //
    // Switch to perform appropriate actions per command
    //

    switch (CallbackData->CmdCode)
    {
    case MME_SET_GLOBAL_TRANSFORM_PARAMS:
        //
        //
        //
        StreamParameterContext  = (CodecBaseStreamParameterContext_t *)CallbackData;
        StreamParameterContext->StreamParameterContextBuffer->DecrementReferenceCount();
        MMECommandCompletedCount++;

        //
        // boost the callback priority to be the same as the DecodeToManifest process
        //
        if (!MMECallbackPriorityBoosted)
        {
            OS_SetPriority(&player_tasks_desc[SE_TASK_AUDIO_DTOM]);
            MMECallbackPriorityBoosted = true;
        }
        GlobalParamsCommandCompleted(StreamParameterContext);

        break;

    case MME_TRANSFORM:
#ifdef LOWMEMORYBANDWIDTH
        if ((Stream->GetEncoding() == STM_SE_STREAM_ENCODING_VIDEO_HEVC) ||
            (Stream->GetEncoding() == STM_SE_STREAM_ENCODING_VIDEO_H264))
        {
            Player->releaseSemMemBWLimiter();
        }
#endif
        //
        // in case of StreamBase decoders the transform may be return with NOT_ENOUGH_MEMORY event so... incomplete
        //
        if (Event == MME_COMMAND_COMPLETED_EVT)
        {
            DecodeContext           = (CodecBaseDecodeContext_t *)CallbackData;

            Stream->Statistics().FrameCountDecoded++;

            if (Stream->Statistics().FrameCountDecoded == 0)
            {
                Stream->Statistics().FrameCountDecoded++;
            }

            if (CallbackData->CmdStatus.Error != MME_COMMAND_ABORTED) // No need to check DecodeContext for the aborted command
            {
                CodecStatus_t Status = ValidateDecodeContext(DecodeContext);
                if (Status != CodecNoError)
                {
                    SE_ERROR("(%s) - Failed to validate the decode(%p) context\n", Configuration.CodecName, DecodeContext);
                }

                if (CallbackData->CmdStatus.Error != MME_SUCCESS)
                {
                    SE_ERROR("MME Transform Callback %x\n", CallbackData->CmdStatus.Error);
                    DecodeContext->DecodeQuality    = 0;
                }

                Status = CheckCodecReturnParameters(DecodeContext);
                if (Status != CodecNoError)
                {
                    SE_ERROR("(%s) - Failed to check codec return parameters\n", Configuration.CodecName);
                }

                //
                // Update the buffer quality, based on this decode and reference frame list.
                //
                UpdateBufferQuality(DecodeContext);
                //
                // Calculate the decode time
                //
                Status  = CalculateMaximumFrameRate(DecodeContext);
                if (Status != CodecNoError)
                {
                    SE_ERROR("(%s) - Failed to adjust maximum frame rate\n", Configuration.CodecName);
                }

#if 0
                SE_INFO(GetGroupTrace(), "Decode took %6lldus\n", OS_GetTimeInMicroSeconds() - DecodeContext->DecodeCommenceTime);
                {
                    static unsigned long long LastCommence  = 0;
                    static unsigned long long LastComplete  = 0;
                    unsigned long long Now          = OS_GetTimeInMicroSeconds();

                    if ((Now - DecodeContext->DecodeCommenceTime) > 30000)
                        SE_INFO(GetGroupTrace(), "Decode times - CommenceInterval %6lld, CompleteInterval %6lld, DurationInterval %6lld\n",
                                DecodeContext->DecodeCommenceTime - LastCommence,
                                Now - LastComplete,
                                Now - DecodeContext->DecodeCommenceTime);

                    LastCommence        = DecodeContext->DecodeCommenceTime;
                    LastComplete        = Now;
                }
#endif
            }

            OS_LockMutex(&Lock);
            ReleaseDecodeContext(DecodeContext);
            OS_UnLockMutex(&Lock);
            {
                PlayerEventRecord_t          DecodeEvent;
                DecodeEvent.Code           = EventNewFrameDecoded;
                DecodeEvent.Playback       = Playback;
                DecodeEvent.Stream         = Stream;
                PlayerStatus_t StatusEvt = Stream->SignalEvent(&DecodeEvent);
                if (StatusEvt != PlayerNoError)
                {
                    SE_ERROR("(%s) - Failed to signal decode event\n", Configuration.CodecName);
                }
            }
            MMECommandCompletedCount++;
        }
        else
        {
            SE_DEBUG(GetGroupTrace(), " (%s) - Transform Command returned INCOMPLETE\n", Configuration.CodecName);
        }

        break;

    case MME_SEND_BUFFERS:
        MMECommandCompletedCount++;
        break;

    default:
        break;
    }

    //
    // Test for a marker buffer to be passed on
    //

    if (MarkerBuffer != NULL)
    {
        TestMarkerFramePassOn();
    }
}

// /////////////////////////////////////////////////////////////////////////
//
//      Function to decrement the reference count on a buffer, also
//      wraps the cleaning of the associated data in the codec.
//
//      This function must be mutex-locked by caller
//
CodecStatus_t   Codec_MmeBase_c::DecrementAccessCount(unsigned int BufferIndex,
                                                      bool         ForReference)
{
    Buffer_t        Buffer;
    unsigned int    Count;

    OS_AssertMutexHeld(&Lock);
    Buffer      = BufferState[BufferIndex].Buffer;

    if (Buffer == NULL)
    {
        SE_ERROR("(%s) NULL buffer index %d\n", Configuration.CodecName, BufferIndex);
        return CodecError;
    }

    Buffer->GetOwnerCount(&Count);

    if (Count == 1)
    {
        if ((BufferState[BufferIndex].ReferenceFrameCount != 0) ||
            (BufferState[BufferIndex].DecodesInProgress != 0))
        {
            SE_ERROR("(%s) - BufferState inconsistency (%d %d), implementation error\n", Configuration.CodecName,
                     BufferState[BufferIndex].ReferenceFrameCount, BufferState[BufferIndex].DecodesInProgress);
        }

        UnMapBufferIndex(BufferIndex);
        memset(&BufferState[BufferIndex], 0, sizeof(CodecBufferState_t));
        BufferState[BufferIndex].BufferQuality = 1;  // rational
    }

    if ((Stream == NULL) || (Stream->GetDecodeBufferManager() == NULL))
    {
        SE_ERROR("Stream 0x%p Buffer can't be released", Stream);
        return CodecError;
    }

    Stream->GetDecodeBufferManager()->ReleaseBuffer(Buffer, ForReference);

    return CodecNoError;
}

// /////////////////////////////////////////////////////////////////////////
//
//      Function to adjust the maximum frame rate based on actual decode times
//

CodecStatus_t   Codec_MmeBase_c::CalculateMaximumFrameRate(CodecBaseDecodeContext_t   *DecodeContext)
{
    unsigned long long           Now;
    unsigned long long           DecodeTime;
    unsigned long long           DecodeTimeForIOnly;
    Now         = OS_GetTimeInMicroSeconds();
    DecodeTime  = min(Now - DecodeContext->DecodeCommenceTime, Now - LastDecodeCompletionTime);
    // Store video decode time
    // Average decode time is computed for video only. This time is used to compute
    // I only decode domain limit.

    long long stat = Stream->Statistics().AvgVideoHwDecodeTime;
    if (stat != 0)
    {
        SE_VERBOSE(group_avsync, "Stream 0x%p Using hw decode time statistic : %lld\n", Stream, stat);
        DecodeTimeForIOnly = stat;
    }
    else
    {
        DecodeTimeForIOnly = DecodeTime /*Now - Stream->DecodeCommenceTime*/;
    }
    OS_LockMutex(&Lock);

    if (BufferState[DecodeContext->BufferIndex].FieldDecode)
    {
        DecodeTime  *= 2;
        DecodeTimeForIOnly *= 2;
    }

    if (DecodeTimeShortIntegrationPeriod == 0)
    {
        Stream->GetDecodeBufferManager()->GetEstimatedBufferCount(&DecodeTimeLongIntegrationPeriod);
        // Setting short integration period to 4 means we will compute the average decode time on 4 sliding GOPs.
        // It seems to give enough pictures to have a reliable average decode value.
        // This computation is common to audio and video.
        DecodeTimeShortIntegrationPeriod     = 4;

        if (BufferState[DecodeContext->BufferIndex].FieldDecode)
        {
            DecodeTimeLongIntegrationPeriod  *= 2;
            DecodeTimeShortIntegrationPeriod *= 2;
        }
    }

    OS_UnLockMutex(&Lock);

    if (!DecodeTimeLongIntegrationPeriod)
    {
        SE_WARNING("DecodeTimeLongIntegrationPeriod 0; forcing 1\n");
        DecodeTimeLongIntegrationPeriod++;
    }

    ShortTotalDecodeTime                        = ShortTotalDecodeTime - DecodeTimes[(NextDecodeTime + DecodeTimeLongIntegrationPeriod - DecodeTimeShortIntegrationPeriod) %
                                                                                     DecodeTimeLongIntegrationPeriod] + DecodeTime;
    LongTotalDecodeTime                         = LongTotalDecodeTime - DecodeTimes[NextDecodeTime % DecodeTimeLongIntegrationPeriod] + DecodeTime;
    DecodeTimes[NextDecodeTime % DecodeTimeLongIntegrationPeriod]   = DecodeTime;
    NextDecodeTime++;

    if (Stream->GetStreamType() == StreamTypeVideo)
    {
        // Integrate decode times for short and long period.
        ShortTotalDecodeTimeForIOnly = ShortTotalDecodeTimeForIOnly - DecodeTimesForIOnly[(NextDecodeTimeForIOnly + DecodeTimeLongIntegrationPeriod - DecodeTimeShortIntegrationPeriod) %
                                                                                          DecodeTimeLongIntegrationPeriod] + DecodeTimeForIOnly;
        LongTotalDecodeTimeForIOnly = LongTotalDecodeTimeForIOnly - DecodeTimesForIOnly[NextDecodeTimeForIOnly % DecodeTimeLongIntegrationPeriod] + DecodeTimeForIOnly;
        DecodeTimesForIOnly[NextDecodeTimeForIOnly % DecodeTimeLongIntegrationPeriod] = DecodeTimeForIOnly;
        NextDecodeTimeForIOnly++;
    }

    LastDecodeCompletionTime                        = Now;

    if ((NextDecodeTime % DecodeTimeLongIntegrationPeriod) == 0)
    {
        if (!LongTotalDecodeTime)
        {
            SE_WARNING("LongTotalDecodeTime 0; forcing 1\n");
            LongTotalDecodeTime++;
        }

        SE_VERBOSE(GetGroupTrace(), "Decode Times(%s) - LongIntegrationAverage = %lld (I-Only=%lld) (over %d) , ShortIntegrationAverage = %lld (I-Only=%lld) (over %d) Max Framerate = %d (short : %d)\n",
                   Configuration.CodecName,
                   LongTotalDecodeTime / DecodeTimeLongIntegrationPeriod,
                   LongTotalDecodeTimeForIOnly / DecodeTimeLongIntegrationPeriod,
                   DecodeTimeLongIntegrationPeriod,
                   ShortTotalDecodeTime / DecodeTimeShortIntegrationPeriod,
                   ShortTotalDecodeTimeForIOnly / DecodeTimeShortIntegrationPeriod,
                   DecodeTimeShortIntegrationPeriod,
                   (unsigned int)((1000000ull * DecodeTimeShortIntegrationPeriod) / ShortTotalDecodeTime),
                   (unsigned int)((1000000ull * DecodeTimeShortIntegrationPeriod) / ShortTotalDecodeTime)
                  );
    }

    if (!ShortTotalDecodeTime)
    {
        ShortTotalDecodeTime++;
    }

    if (!LongTotalDecodeTime)
    {
        LongTotalDecodeTime++;
    }

    if (!ShortTotalDecodeTimeForIOnly)
    {
        ShortTotalDecodeTimeForIOnly++;
    }

    if (!LongTotalDecodeTimeForIOnly)
    {
        LongTotalDecodeTimeForIOnly++;
    }

    Configuration.TrickModeParameters.EmpiricalMaximumDecodeFrameRateShortIntegration   = Rational_t((1000000ull * DecodeTimeShortIntegrationPeriod), ShortTotalDecodeTime);
    Configuration.TrickModeParameters.EmpiricalMaximumDecodeFrameRateLongIntegration    = Rational_t((1000000ull * DecodeTimeLongIntegrationPeriod), LongTotalDecodeTime);

    if (NextDecodeTimeForIOnly > max(DecodeTimeLongIntegrationPeriod, DecodeTimeShortIntegrationPeriod))
    {
        Configuration.TrickModeParameters.DecodeFrameRateShortIntegrationForIOnly = Rational_t((1000000ull * DecodeTimeShortIntegrationPeriod), ShortTotalDecodeTimeForIOnly);
        Configuration.TrickModeParameters.DecodeFrameRateLongIntegrationForIOnly = Rational_t((1000000ull * DecodeTimeLongIntegrationPeriod), LongTotalDecodeTimeForIOnly);
    }

    return CodecNoError;
}


///////////////////////////////////////////////////////////////////////////
///
/// Take ownership of Codec_MmeBase_c::MarkerBuffer in a thread-safe way.
///
/// Atomically read the marker buffer and set it to NULL. Nobody except
/// this method is permitted to set MarkerBuffer
/// to NULL.
///
Buffer_t Codec_MmeBase_c::TakeMarkerBuffer(void)
{
    Buffer_t LocalMarkerBuffer;
    OS_LockMutex(&Lock);
    LocalMarkerBuffer = MarkerBuffer;
    MarkerBuffer = NULL;
    OS_UnLockMutex(&Lock);
    return LocalMarkerBuffer;
}

// /////////////////////////////////////////////////////////////////////////
//
//      Test whether or not we have to pass on a marker frame.
//

CodecStatus_t   Codec_MmeBase_c::TestMarkerFramePassOn(void)
{
    Buffer_t                  LocalMarkerBuffer;

    if ((MarkerBuffer != NULL) &&
        ((MMECommandAbortedCount + MMECommandCompletedCount) >= PassOnMarkerBufferAt))
    {
        LocalMarkerBuffer       = TakeMarkerBuffer();

        if (LocalMarkerBuffer)
        {
            mOutputPort->Insert((uintptr_t)LocalMarkerBuffer);
        }
    }

    return CodecNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
//      Function to update the buffer quality based on the
//  reference frame list and the current decode quality.
//

void   Codec_MmeBase_c::UpdateBufferQuality(CodecBaseDecodeContext_t *Context)
{
    unsigned int    i, j;
    unsigned int    ReferenceCount;
    Rational_t  ReferenceFrameQuality;

    OS_LockMutex(&Lock);
    ReferenceCount      = 0;
    ReferenceFrameQuality   = 0;

    for (i = 0; i < Context->NumberOfReferenceFrameLists; i++)
        for (j = 0; j < Context->ReferenceFrameList[i].EntryCount; j++)
        {
            ReferenceFrameQuality   += BufferState[Context->ReferenceFrameList[i].EntryIndicies[j]].BufferQuality;
            ReferenceCount++;
        }

    ReferenceFrameQuality   = (ReferenceCount == 0) ? 1 : (ReferenceFrameQuality / ReferenceCount);

    if (Context->IndependentFrame)
    {
        ReferenceFrameQuality = 1;
    }
    else if (Context->UsedReferenceFrameSubstitution)
    {
        ReferenceFrameQuality = ReferenceFrameQuality / 2;
    }

    BufferState[Context->BufferIndex].BufferQuality = (BufferState[Context->BufferIndex].BufferQuality * BufferState[Context->BufferIndex].BufferQualityWeight) +
                                                      (ReferenceFrameQuality * Context->DecodeQuality);
    BufferState[Context->BufferIndex].BufferQualityWeight++;

    if (!BufferState[Context->BufferIndex].BufferQualityWeight)
    {
        BufferState[Context->BufferIndex].BufferQualityWeight++;
    }

    BufferState[Context->BufferIndex].BufferQuality /= BufferState[Context->BufferIndex].BufferQualityWeight;
    OS_UnLockMutex(&Lock);
}


// /////////////////////////////////////////////////////////////////////////
//
//      Function to dump out an mme command
//

void   Codec_MmeBase_c::DumpMMECommand(MME_Command_t *CmdInfo_p)
{
    if (!SE_IS_VERBOSE_ON(GetGroupTrace()))
    {
        return;
    }

    SE_VERBOSE(GetGroupTrace(), "AZA - COMMAND (%d)\n", ParsedFrameParameters->DecodeFrameIndex);
    SE_VERBOSE(GetGroupTrace(), "AZA -     StructSize                             = %d\n", CmdInfo_p->StructSize);
    SE_VERBOSE(GetGroupTrace(), "AZA -     CmdCode                                = %d\n", CmdInfo_p->CmdCode);
    SE_VERBOSE(GetGroupTrace(), "AZA -     CmdEnd                                 = %d\n", CmdInfo_p->CmdEnd);
    SE_VERBOSE(GetGroupTrace(), "AZA -     DueTime                                = %d\n", CmdInfo_p->DueTime);
    SE_VERBOSE(GetGroupTrace(), "AZA -     NumberInputBuffers                     = %d\n", CmdInfo_p->NumberInputBuffers);
    SE_VERBOSE(GetGroupTrace(), "AZA -     NumberOutputBuffers                    = %d\n", CmdInfo_p->NumberOutputBuffers);
    SE_VERBOSE(GetGroupTrace(), "AZA -     DataBuffers_p                          = %p\n", CmdInfo_p->DataBuffers_p);
    SE_VERBOSE(GetGroupTrace(), "AZA -     CmdStatus\n");
    SE_VERBOSE(GetGroupTrace(), "AZA -         CmdId                              = %d\n", CmdInfo_p->CmdStatus.CmdId);
    SE_VERBOSE(GetGroupTrace(), "AZA -         State                              = %d\n", CmdInfo_p->CmdStatus.State);
    SE_VERBOSE(GetGroupTrace(), "AZA -         ProcessedTime                      = %d\n", CmdInfo_p->CmdStatus.ProcessedTime);
    SE_VERBOSE(GetGroupTrace(), "AZA -         Error                              = %d\n", CmdInfo_p->CmdStatus.Error);
    SE_VERBOSE(GetGroupTrace(), "AZA -         AdditionalInfoSize                 = %d\n", CmdInfo_p->CmdStatus.AdditionalInfoSize);
    SE_VERBOSE(GetGroupTrace(), "AZA -         AdditionalInfo_p                   = %p\n", CmdInfo_p->CmdStatus.AdditionalInfo_p);
    SE_VERBOSE(GetGroupTrace(), "AZA -     ParamSize                              = %d\n", CmdInfo_p->ParamSize);
    SE_VERBOSE(GetGroupTrace(), "AZA -     Param_p                                = %p\n", CmdInfo_p->Param_p);

    switch (CmdInfo_p->CmdCode)
    {
    case MME_SET_GLOBAL_TRANSFORM_PARAMS:
        DumpSetStreamParameters(CmdInfo_p->Param_p);
        break;

    case MME_TRANSFORM:
        DumpDecodeParameters(CmdInfo_p->Param_p);
        break;

    default: break;
    }
}

////////////////////////////////////////////////////////////////////////////
/// \fn CodecStatus_t Codec_MmeBase_c::FillOutTransformerInitializationParameters( void )
///
/// Populate the transformers initialization parameters.
///
/// This method is required the fill out the TransformerInitParamsSize and TransformerInitParams_p
/// members of Codec_MmeBase_c::MMEInitializationParameters and initialize and data pointed to
/// as a result.
///
/// This method may, optionally, change the default values held in other members of
/// Codec_MmeBase_c::MMEInitializationParameters .
///


////////////////////////////////////////////////////////////////////////////
/// \fn CodecStatus_t   FillOutSendBufferCommand(   void );
/// This function may only implemented for stream base inheritors.
/// Populates the SEND_BUFFER Command structure

CodecStatus_t   Codec_MmeBase_c::FillOutSendBufferCommand(void)
{
    // TODO :: default implementation
    return CodecNoError;
}


/// /////////////////////////////////////////////////////////////////////////
///
///      UpdateSpeed function
///      Action  : Allows caller to inform a new playback speed has been set
///      Input   :
///      Output  :
///      Result  :
///

CodecStatus_t   Codec_MmeBase_c::UpdatePlaybackSpeed(void)
{
    ForceStreamParameterReload = true;
    return CodecNoError;
}
