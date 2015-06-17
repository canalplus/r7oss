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

//{{{  note
////////////////////////////////////////////////////////////////////////////
/// \class Codec_MmeAudioStream_c
///
/// The stream audio codec proxy.
///
/// When it is difficult for the host processor to efficiently determine the
/// frame boundaries the player makes no attempt to discover the frame boundaries.
/// There is not a one-to-one relationship between input buffers and output buffers
/// so the decoder is operated with streaming input (MME_SEND_BUFFERS) extracting
/// frame based output whenever we believe the decoder to be capable of
/// providing data (MME_TRANSFORM).
///
///
//}}}

#include "osinline.h"
#include "ksound.h"
#include "st_relayfs_se.h"
#include "player_threads.h"

#include "codec_mme_audio_stream.h"
#include "auto_lock_mutex.h"

#undef TRACE_TAG
#define TRACE_TAG "Codec_MmeAudioStream_c"

#define BUFFER_AUDIO_CODEC_TRANSFORM_CONTEXT   "AudioCodecTransformContext"
#define BUFFER_AUDIO_CODEC_TRANSFORM_CONTEXT_TYPE {BUFFER_AUDIO_CODEC_TRANSFORM_CONTEXT,        \
                                                   BufferDataTypeBase,                          \
                                                   AllocateFromOSMemory,                        \
                                                   32,                                          \
                                                   0,                                           \
                                                   true,                                        \
                                                   true,                                        \
                                                   sizeof(StreamAudioCodecTransformContext_t)}

static BufferDataDescriptor_t AudioCodecTransformContextDescriptor = BUFFER_AUDIO_CODEC_TRANSFORM_CONTEXT_TYPE;

static OS_TaskEntry(TransformThreadStub)
{
    Codec_MmeAudioStream_c     *Codec = (Codec_MmeAudioStream_c *)Parameter;
    Codec->TransformThread();
    OS_TerminateThread();
    return NULL;
}

///
Codec_MmeAudioStream_c::Codec_MmeAudioStream_c(void)
    : Codec_MmeAudio_c()
    , SendbufTriggerTransformCount(0)
    , NeedToMarkStreamUnplayable(false)
    , TransformContext(NULL)
    , DecodeContextPoolMutex()
    , DecoderId(ACC_LAST_DECODER_ID)
    , TransformThreadRunning(false)
    , TransformThreadTerminated()
    , InputMutex()
    , IssueTransformCommandEvent()
    , IssueSendBufferEvent()
    , mTransformContextDescriptor(NULL)
    , mTransformContextType(0)
    , TransformContextPool(NULL)
    , TransformContextBuffer(NULL)
    , TransformCodedFrameMemoryDevice(0)
    , TransformCodedFrameMemory()
    , TransformCodedFramePool(NULL)
    , CurrentDecodeFrameIndex(0)
    , SavedParsedFrameParameters()
    , SavedSequenceNumberStructure()
    , MarkerFrameSavedSequenceNumberStructure()
    , InPossibleMarkerStallState(false)
    , TimeOfEntryToPossibleMarkerStallState(0)
    , StallStateSendBuffersCommandsIssued(0)
    , StallStateTransformCommandsCompleted(0)
    , SendBuffersCommandsIssued(0)
    , SendBuffersCommandsCompleted(0)
    , TransformCommandsIssued(0)
    , TransformCommandsCompleted(0)
    , LastNormalizedPlaybackTime(UNSPECIFIED_TIME)
    , EofMarkerFrameReceived(false)
    , DontIssueEof(false)
    , PutMarkerFrameToTheRing(false)
    , TransformCommandsToIssueBeforeAutoEof(0)
    , SendBuffersCommandsIssuedTillMarkerFrame(0)
{
    Configuration.CodecName  = "Stream audio";
    SE_VERBOSE(group_decoder_audio, "this = %p\n", this);

    OS_InitializeMutex(&InputMutex);
    OS_InitializeMutex(&DecodeContextPoolMutex);

    OS_InitializeEvent(&TransformThreadTerminated);
    OS_InitializeEvent(&IssueTransformCommandEvent);
    OS_InitializeEvent(&IssueSendBufferEvent);
}

///
Codec_MmeAudioStream_c::~Codec_MmeAudioStream_c(void)
{
    SE_VERBOSE(group_decoder_audio, "this = %p\n", this);
    Halt();

    // Release the decoded frame context buffer pool
    if (TransformContextPool != NULL)
    {
        BufferManager->DestroyPool(TransformContextPool);
    }

    // Release the coded frame buffer pool
    if (TransformCodedFramePool != NULL)
    {
        BufferManager->DestroyPool(TransformCodedFramePool);
    }

    // Free the coded frame memory
    AllocatorClose(&TransformCodedFrameMemoryDevice);

    OS_TerminateMutex(&DecodeContextPoolMutex);
    OS_TerminateMutex(&InputMutex);

    OS_TerminateEvent(&TransformThreadTerminated);
    OS_TerminateEvent(&IssueTransformCommandEvent);
    OS_TerminateEvent(&IssueSendBufferEvent);
}

///
CodecStatus_t   Codec_MmeAudioStream_c::Halt(void)
{
    SE_VERBOSE(group_decoder_audio, "this=%p\n", this);

    if (TransformThreadRunning)
    {
        // Ask thread to terminate
        OS_ResetEvent(&TransformThreadTerminated);
        OS_Smp_Mb(); // Write memory barrier: wmb_for_MmeAudioStream_Terminating coupled with: rmb_for_MmeAudioStream_Terminating
        TransformThreadRunning = false;

        // set any events the thread may be blocked waiting for
        OS_SetEvent(&IssueTransformCommandEvent);
        OS_SetEvent(&IssueSendBufferEvent);

        // wait for the thread to terminate
        OS_WaitForEventAuto(&TransformThreadTerminated, OS_INFINITE);
    }

    return Codec_MmeAudio_c::Halt();
}


// /////////////////////////////////////////////////////////////////////////
//
//      Function to initialize all the types with the buffer manager
//

CodecStatus_t   Codec_MmeAudioStream_c::InitializeDataTypes(void)
{
    SE_VERBOSE(group_decoder_audio, "DataTypesInitialized:%s", DataTypesInitialized ? "true" : "false");

    if (DataTypesInitialized)
    {
        return CodecNoError;
    }

    //
    // Transform context type
    //
    CodecStatus_t Status = InitializeDataType(&AudioCodecTransformContextDescriptor,
                                              &mTransformContextType,
                                              &mTransformContextDescriptor);
    if (Status != CodecNoError)
    {
        SE_ERROR("Failed to initialize TransformContext data type");
        return Status;
    }

    return Codec_MmeAudio_c::InitializeDataTypes();
}
//{{{  Connect
// /////////////////////////////////////////////////////////////////////////
//
//      Connect output port
//
CodecStatus_t   Codec_MmeAudioStream_c::Connect(Port_c *Port)
{
    PlayerStatus_t      Status;
    OS_Thread_t         Thread;

    Status = Codec_MmeAudio_c::Connect(Port);
    if (Status != CodecNoError)
    {
        return Status;
    }

    //{{{  Create transform thread and mutexes
    // Create Transform thread and events and mutexes it uses
    if (!TransformThreadRunning)
    {
        TransformThreadRunning = true;

        if (OS_CreateThread(&Thread, TransformThreadStub, this, &player_tasks_desc[SE_TASK_AUDIO_STREAMT]) != OS_NO_ERROR)
        {
            SE_ERROR("Unable to create Transform playback thread\n");
            TransformThreadRunning = false;
            return CodecError;
        }
    }

    //}}}

    // Create the coded frame buffer pool if necessary
    if (TransformCodedFramePool == NULL)
    {
        BufferType_t    CodedFrameBufferType;
        CodedFrameBufferPool->GetType(&CodedFrameBufferType);

        unsigned int    CodedFrameCount;
        unsigned int    CodedMemorySize;
        CodedFrameBufferPool->GetPoolUsage(&CodedFrameCount, NULL, &CodedMemorySize, NULL, NULL);

        // Get the memory and Create the pool with it
        Status = PartitionAllocatorOpen(&TransformCodedFrameMemoryDevice,
                                        Configuration.TranscodedMemoryPartitionName,
                                        CodedMemorySize,
                                        MEMORY_DEFAULT_ACCESS);
        if (Status != allocator_ok)
        {
            SE_ERROR("(%s) - Failed to allocate memory\n", Configuration.CodecName);
            return PlayerInsufficientMemory;
        }

        TransformCodedFrameMemory[CachedAddress]        = AllocatorUserAddress(TransformCodedFrameMemoryDevice);
        TransformCodedFrameMemory[PhysicalAddress]      = AllocatorPhysicalAddress(TransformCodedFrameMemoryDevice);

        Status = BufferManager->CreatePool(&TransformCodedFramePool, CodedFrameBufferType,
                                           MAX_DECODE_BUFFERS, CodedMemorySize, TransformCodedFrameMemory);
        if (Status != BufferNoError)
        {
            SE_ERROR("(%s) - Failed to create the coded frame pool\n", Configuration.CodecName);
            return PlayerInsufficientMemory;
        }

        Status = TransformCodedFramePool->AttachMetaData(Player->MetaDataParsedFrameParametersType);
        if (Status != PlayerNoError)
        {
            SE_ERROR("(%s) - Failed to attach ParsedFrameParameters to the coded buffer pool\n", Configuration.CodecName);
            return Status;
        }

        Status = TransformCodedFramePool->AttachMetaData(Player->MetaDataSequenceNumberType);
        if (Status != PlayerNoError)
        {
            SE_ERROR("(%s) - Failed to attach sequence numbers to the coded buffer pool\n", Configuration.CodecName);
            return Status;
        }
    }

    // Create the decode context buffers
    Player->GetBufferManager(&BufferManager);

    if (TransformContextPool == NULL)
    {
        Status = BufferManager->CreatePool(&TransformContextPool, mTransformContextType, Configuration.DecodeContextCount);
        if (Status != BufferNoError)
        {
            SE_ERROR("(%s) - Failed to create a pool of decode context buffers\n", Configuration.CodecName);
            return Status;
        }

        //Set this up here too - this will be stamped onto the ParsedFrameParameters of the CodedFrames
        CurrentDecodeFrameIndex         = 0;
    }

    TransformContextBuffer              = NULL;
    return CodecNoError;
}
//}}}
//{{{  Input
// /////////////////////////////////////////////////////////////////////////
//
//      The Input function - receive chunks of data parsed by the frame parser
//
CodecStatus_t   Codec_MmeAudioStream_c::Input(Buffer_t          CodedBuffer)
{
    unsigned int  waiting_for_buffer = 0;
#define CODEC_STREAM_WAIT_TIMEOUT 1000
    OS_ResetEvent(&IssueSendBufferEvent);

    while ((SendBuffersCommandsIssued - SendBuffersCommandsCompleted) >= Configuration.DecodeContextCount)
    {
        if (waiting_for_buffer == 0)
        {
            SE_DEBUG(group_decoder_audio, "No buffer available to send a SendBuffer command : Waiting until a buffer is released\n");
        }

        OS_WaitForEventAuto(&IssueSendBufferEvent, CODEC_STREAM_WAIT_TIMEOUT);

        waiting_for_buffer += CODEC_STREAM_WAIT_TIMEOUT;
    }

    if (waiting_for_buffer > CODEC_STREAM_WAIT_TIMEOUT)
    {
        SE_DEBUG(group_decoder_audio, "No buffer available to send a SendBuffer command : has been waiting until a buffer be released for %d ms\n", waiting_for_buffer);
    }

    OS_LockMutex(&InputMutex);

    //! get the coded frame params
    ParsedFrameParameters_t *ParsedFrameParameters;
    CodedBuffer->ObtainMetaDataReference(Player->MetaDataParsedFrameParametersType, (void **)(&ParsedFrameParameters));
    SE_ASSERT(ParsedFrameParameters != NULL);

    memcpy(&SavedParsedFrameParameters, ParsedFrameParameters, sizeof(ParsedFrameParameters_t));

    PlayerSequenceNumber_t *SequenceNumberStructure;
    CodedBuffer->ObtainMetaDataReference(Player->MetaDataSequenceNumberType, (void **)(&SequenceNumberStructure));
    SE_ASSERT(SequenceNumberStructure != NULL);

    /* When marker frame is of type DrainPlayOutMarker then consider it as a EofMarkerFrame.
       In this case copy the  Sequence Number Structure into the different Structure.
       So that Marker to be putted in the the ring when Eof received from the FW.
       Don't issue the EofMarkerFrame when flag DontIssueEof is set (it is set in case of input jump)
       also issue EofMarkerFrame only if we had atleast a input buffer in the processing */
    if ((SequenceNumberStructure->MarkerFrame) && (SequenceNumberStructure->MarkerType == DrainPlayOutMarker) && (!DontIssueEof) && (SendBuffersCommandsIssued != 0))
    {
        memcpy(&MarkerFrameSavedSequenceNumberStructure, SequenceNumberStructure, sizeof(PlayerSequenceNumber_t));
        ParsedFrameParameters->EofMarkerFrame    = true;
        EofMarkerFrameReceived = true;
        TransformCommandsToIssueBeforeAutoEof = WAIT_FOR_TRANSFORM_COMMANDS_BEFORE_AUTO_EOF + Configuration.DecodeContextCount;
        SendBuffersCommandsIssuedTillMarkerFrame = SendBuffersCommandsIssued;
        SE_DEBUG(group_decoder_audio, "Got Input with EOF Marker Frame : SendBuffersCommandsIssuedTillMarkerFrame =  %d\n", SendBuffersCommandsIssuedTillMarkerFrame);
    }
    else
    {
        memcpy(&SavedSequenceNumberStructure, SequenceNumberStructure, sizeof(PlayerSequenceNumber_t));
    }

    // Perform base operations, on return we may need to mark the stream as unplayable
    CodecStatus_t Status = Codec_MmeAudio_c::Input(CodedBuffer);

    OS_UnLockMutex(&InputMutex);

    if (NeedToMarkStreamUnplayable)
    {
        SE_ERROR("(%s) Marking stream unplayable\n", Configuration.CodecName);
        Stream->MarkUnPlayable();
        NeedToMarkStreamUnplayable      = false;
    }

    return Status;
}
//}}}

//{{{  FillOutTransformerInitializationParameters
////////////////////////////////////////////////////////////////////////////
///
/// Populate the AUDIO_DECODER's initialization parameters for Stream audio.
///
/// When this method completes Codec_MmeAudio_c::AudioDecoderInitializationParameters
/// will have been filled out with valid values sufficient to initialize an
/// Stream audio decoder
///
CodecStatus_t   Codec_MmeAudioStream_c::FillOutTransformerInitializationParameters(void)
{
    CodecStatus_t                       Status;
    MME_LxAudioDecoderInitParams_t     &Params  = AudioDecoderInitializationParameters;
    unsigned int                        Blocksize;
    MMEInitializationParameters.TransformerInitParamsSize       = sizeof(Params);
    MMEInitializationParameters.TransformerInitParams_p         = &Params;
    Status              = Codec_MmeAudio_c::FillOutTransformerInitializationParameters();
#if DRV_MULTICOM_AUDIO_DECODER_VERSION >= 0x090128
    // Limit the BlockSize of stream-base codecs that may return variable output blocksize.
    // The AudioFW enables to limit this transform size to 512, 1024 or 1536 samples per transform
    // If  set to '0' , the FW would decode a predefined amount of samples as per codec
    //    variable from frame to frame that could be as large as 8k samples.
    Blocksize =  Configuration.MaximumSampleCount;

    if ((Blocksize <= 1536) && (Blocksize == ((Blocksize / 512) * 512)))
    {
        Params.BlockWise.Bits.BlockSize = Blocksize / 512;
    }

#endif

    if (Status != CodecNoError)
    {
        return Status;
    }

    return FillOutTransformerGlobalParameters(&Params.GlobalParams);
}
//}}}
//{{{  FillOutSetStreamParametersCommand
////////////////////////////////////////////////////////////////////////////
///
/// Populate the AUDIO_DECODER's MME_SET_GLOBAL_TRANSFORMER_PARAMS parameters for stream based audio.
///
CodecStatus_t   Codec_MmeAudioStream_c::FillOutSetStreamParametersCommand(void)
{
    CodecStatus_t                               Status;
    StreamAudioCodecStreamParameterContext_t   *Context = (StreamAudioCodecStreamParameterContext_t *)StreamParameterContext;
    SE_VERBOSE(group_decoder_audio, "\n");
    // Fill out the structure
    memset(&(Context->StreamParameters), 0, sizeof(Context->StreamParameters));
    Status              = FillOutTransformerGlobalParameters(&(Context->StreamParameters));

    if (Status != CodecNoError)
    {
        return Status;
    }

    // Fill out the actual command
    Context->BaseContext.MMECommand.CmdStatus.AdditionalInfoSize        = 0;
    Context->BaseContext.MMECommand.CmdStatus.AdditionalInfo_p          = NULL;
    Context->BaseContext.MMECommand.ParamSize                           = sizeof(Context->StreamParameters);
    Context->BaseContext.MMECommand.Param_p                             = (MME_GenericParams_t)(&Context->StreamParameters);
    return CodecNoError;
}
//}}}

//{{{  FillOutDecodeContext
////////////////////////////////////////////////////////////////////////////
///
/// Override the superclass version to suit MME_SEND_BUFFERS.
///
/// Populate the DecodeContext structure with parameters for a single buffer
///
CodecStatus_t Codec_MmeAudioStream_c::FillOutDecodeContext(void)
{
    //
    // Provide default values for the input and output buffers (the sub-class can change this if it wants to).
    //
    memset(&DecodeContext->MMECommand, 0, sizeof(MME_Command_t));
    DecodeContext->MMECommand.NumberInputBuffers                = 1;
    DecodeContext->MMECommand.NumberOutputBuffers               = 0;
    DecodeContext->MMECommand.DataBuffers_p = DecodeContext->MMEBufferList;
    // plumbing
    DecodeContext->MMEBufferList[0]                             = &DecodeContext->MMEBuffers[0];
    memset(&DecodeContext->MMEBuffers[0], 0, sizeof(MME_DataBuffer_t));
    DecodeContext->MMEBuffers[0].StructSize                     = sizeof(MME_DataBuffer_t);
    DecodeContext->MMEBuffers[0].NumberOfScatterPages           = 1;
    DecodeContext->MMEBuffers[0].ScatterPages_p                 = &DecodeContext->MMEPages[0];
    memset(&DecodeContext->MMEPages[0], 0, sizeof(MME_ScatterPage_t));
    // input
    DecodeContext->MMEBuffers[0].TotalSize                      = CodedDataLength;
    DecodeContext->MMEPages[0].Page_p                           = CodedData;
    DecodeContext->MMEPages[0].Size                             = CodedDataLength;
    return CodecNoError;
}
//}}}
//{{{  FillOutDecodeCommand
////////////////////////////////////////////////////////////////////////////
///
/// Populate the AUDIO_DECODER's -=<< !! MME_SEND_BUFFERS !! >>=- parameters for Stream audio.
///1572864
CodecStatus_t   Codec_MmeAudioStream_c::FillOutDecodeCommand(void)
{
    StreamAudioCodecDecodeContext_t    *Context = (StreamAudioCodecDecodeContext_t *)DecodeContext;
    Context->BaseContext.MMECommand.CmdCode                             = MME_SEND_BUFFERS;
    // Initialize the frame parameters
    memset(&Context->DecodeParameters, 0, sizeof(Context->DecodeParameters));
    // Zero the reply structure
    memset(&Context->DecodeStatus, 0, sizeof(Context->DecodeStatus));
    // Fill out the actual command
    Context->BaseContext.MMECommand.CmdStatus.AdditionalInfoSize        = sizeof(Context->DecodeStatus);
    Context->BaseContext.MMECommand.CmdStatus.AdditionalInfo_p          = (MME_GenericParams_t)(&Context->DecodeStatus);
    Context->BaseContext.MMECommand.ParamSize                           = sizeof(Context->DecodeParameters);
    Context->BaseContext.MMECommand.Param_p                             = (MME_GenericParams_t)(&Context->DecodeParameters);
    return CodecNoError;
}
//}}}
//{{{  SendMMEDecodeCommand
CodecStatus_t   Codec_MmeAudioStream_c::SendMMEDecodeCommand(void)
{
    StreamAudioCodecDecodeContext_t    *Context         = (StreamAudioCodecDecodeContext_t *)DecodeContext;
    unsigned int                        PRL             = (unsigned int)Player->PolicyValue(Playback, Stream, PolicyAudioProgramReferenceLevel);
    Buffer_t                            AttachedCodedDataBuffer;
    ParsedFrameParameters_t            *ParsedFrameParams;
    unsigned int                        PTSFlag         = ACC_NO_PTS_DTS;
    unsigned long long int              PTS             = INVALID_TIME;

    //Limit prl to valid range if it is outside the supported range
    PRL = (PRL > 31) ? 31 : PRL;

    AutoLockMutex mutex(&DecodeContextPoolMutex);

    //DecodeContext->DecodeContextBuffer->Dump (DumpAll);
    // pass the PTS to the firmware...
    DecodeContext->DecodeContextBuffer->ObtainAttachedBufferReference(CodedFrameBufferType, &AttachedCodedDataBuffer);
    SE_ASSERT(AttachedCodedDataBuffer != NULL);

    AttachedCodedDataBuffer->ObtainMetaDataReference(Player->MetaDataParsedFrameParametersType,
                                                     (void **)(&ParsedFrameParams));
    SE_ASSERT(ParsedFrameParams != NULL);
    SE_DEBUG(group_decoder_audio, " (%llx,%llx)\n", ParsedFrameParams->NormalizedPlaybackTime, ParsedFrameParams->NativePlaybackTime);

    if (ValidTime(ParsedFrameParams->NormalizedPlaybackTime))
    {
        PTS                                             = ParsedFrameParams->NativePlaybackTime;
        // inform the firmware a pts is present
        PTSFlag                                         = ACC_PTS_PRESENT;
    }
    else
    {
        SE_DEBUG(group_decoder_audio, "(%s) PTS = INVALID_TIME\n", Configuration.CodecName);
        PTS                                             = 0;
    }

    SE_DEBUG(group_decoder_audio, " PTSFlag %x, PTS %llx\n", PTSFlag, PTS);
#if DRV_MULTICOM_AUDIO_DECODER_VERSION >= 0x090128
    Context->DecodeParameters.StructSize                        = sizeof(MME_StreamingBufferParams_t);
    Context->DecodeParameters.BufferFlags.Bits.PTS_DTS_FLAG     = PTSFlag;
    Context->DecodeParameters.BufferFlags.Bits.PtsTimeFormat    = StmSeConvertPlayerTimeFormatToFwTimeFormat(ParsedFrameParameters->NativeTimeFormat);
    Context->DecodeParameters.PTS                               = PTS;
    Context->DecodeParameters.BufferFlags.Bits.DialogNorm       = PRL;

    if (ParsedFrameParameters->EofMarkerFrame)
    {
        Context->DecodeParameters.BufferFlags.Bits.FrameType    = STREAMING_DEC_EOF; // For the EofMarkerFrame TAG Buffer as Eof
    }

#else
    MME_ADBufferParams_t       *BufferParams                   = (MME_ADBufferParams_t *) & (Context->DecodeParameters.BufferParams[0]);
    BufferParams->PTSflags.Bits.PTS_DTS_FLAG                   = PTSFlag;
    BufferParams->PTSflags.Bits.PTS_Bit32                      = PTS >> 32;
    BufferParams->PTS                                          = PTS;
    BufferParams->PTSflags.Bits.DialogNorm                     = PRL;

    if (ParsedFrameParameters->EofMarkerFrame)
    {
        BufferParams->PTSflags.Bits.FrameType                   = STREAMING_DEC_EOF; // For the EofMarkerFrame TAG Buffer as Eof
    }

#endif
    SendBuffersCommandsIssued++;
    OS_SetEvent(&IssueTransformCommandEvent);
    CodecStatus_t Status = Codec_MmeBase_c::SendMMEDecodeCommand();
    return Status;
}
//}}}
//{{{  FinishedDecode
////////////////////////////////////////////////////////////////////////////
///
/// Clear up - do nothing, as actual decode done elsewhere
///
void Codec_MmeAudioStream_c::FinishedDecode(void)
{
    // We have NOT finished decoding into this buffer
    // This is to replace the cleanup after an MME_TRANSFORM from superclass Input function
    // But as we over-ride this with an MME_SEND_BUFFERS, and do the transform from a separate thread
    // we don't want to clean up anything here
}
//}}}
//{{{  ValidateDecodeContext
////////////////////////////////////////////////////////////////////////////
///
/// Validate the ACC status structure and squawk loudly if problems are found.
/// in spite of the messages this method unconditionally returns success. This is
/// because the firmware will already have concealed the decode problems by
/// performing a soft mute.
///
/// \return CodecSuccess
///
CodecStatus_t   Codec_MmeAudioStream_c::ValidateDecodeContext(CodecBaseDecodeContext_t *Context)
{
    bool TranscodeOutputBufAvailable = false;
    bool CompressedOutputBufAvailable = false;
    AssertComponentState(ComponentRunning);

    if (Context == NULL)
    {
        SE_ERROR("(%s) - CodecContext is NULL\n", Configuration.CodecName);
        return CodecError;
    }

    if (AudioOutputSurface == NULL)
    {
        SE_ERROR("(%s) - AudioOutputSurface is NULL\n", Configuration.CodecName);
        return CodecError;
    }

    StreamAudioCodecTransformContext_t *DecodeContext   = (StreamAudioCodecTransformContext_t *)Context;
    MME_LxAudioDecoderFrameStatus_t    *Status          = &(DecodeContext->DecodeStatus.DecStatus);
    SE_DEBUG(group_decoder_audio, "DecStatus %d\n", Status->DecStatus);

    // Incase of EOF we amy receive muted frame in callback of transfrom command when no Input is available
    if ((Status->DecStatus != ACC_HXR_OK) && (Status->PTSflag.Bits.FrameType != STREAMING_DEC_EOF))
    {
        SE_WARNING("(%s) - Audio decode error (muted frame): %d\n", Configuration.CodecName, Status->DecStatus);
        // don't report an error to the higher levels (because the frame is muted)
    }

    // sysfs
    AudioDecoderStatus = *Status;
    SetAudioCodecDecStatistics();
    //
    // Attach any codec derived metadata to the output buffer (or verify the
    // frame analysis if the frame analyser already filled everything in for
    // us).
    //
    /* To determine if we must handle a transcoded buffer,
     * let's check if a transcode buffer has been attached to the CodedDataBuffer*/
    Buffer_t codedDataBuffer;
    Context->DecodeContextBuffer->ObtainAttachedBufferReference(CodedFrameBufferType, &codedDataBuffer);
    if (codedDataBuffer != NULL)
    {
        Buffer_t TranscodeBuffer;
        codedDataBuffer->ObtainAttachedBufferReference(TranscodedFrameBufferType, &TranscodeBuffer);
        if (TranscodeBuffer != NULL)
        {
            MME_Command_t *Cmd = &DecodeContext->BaseContext.MMECommand;
            int32_t TranscodedBufferSize = Cmd->DataBuffers_p[STREAM_BASE_TRANSCODE_BUFFER_INDEX]->ScatterPages_p[0].BytesUsed;
            unsigned int datasize = Cmd->DataBuffers_p[STREAM_BASE_TRANSCODE_BUFFER_INDEX]->ScatterPages_p[0].BytesUsed;
            TranscodedBuffers[DecodeContext->TranscodeBufferIndex].Buffer->SetUsedDataSize(datasize);

            st_relayfs_write_se(ST_RELAY_TYPE_AUDIO_TRANSCODE, ST_RELAY_SOURCE_SE,
                                (uint8_t *) Cmd->DataBuffers_p[STREAM_BASE_TRANSCODE_BUFFER_INDEX]->ScatterPages_p[0].Page_p,
                                TranscodedBufferSize, 0);

            TranscodeOutputBufAvailable = true;
        }
        Buffer_t CompressedBuffer;
        codedDataBuffer->ObtainAttachedBufferReference(CompressedFrameBufferType, &CompressedBuffer);
        // if CompressedFrame is present , then check the compressed frame buffer size and set compressed buffer used data size
        if (CompressedBuffer != NULL)
        {
            MME_Command_t *Cmd = &DecodeContext->BaseContext.MMECommand;
            // When the transcoding is enabled along with CompressedFrame then CompressedFrame index will be TranscodeBufferIndex + 1.
            int32_t CompressedFrameScatterPageIndex = (TranscodeOutputBufAvailable == true) ? STREAM_BASE_TRANSCODE_BUFFER_INDEX + 1 : STREAM_BASE_TRANSCODE_BUFFER_INDEX;
            for (uint32_t i = 0; i < Cmd->DataBuffers_p[CompressedFrameScatterPageIndex]->NumberOfScatterPages; i++)
            {
                int32_t CompressedFrameBufferSize       = Cmd->DataBuffers_p[CompressedFrameScatterPageIndex]->ScatterPages_p[i].BytesUsed;
                CompressedFrameBuffers[i][DecodeContext->CompressedFrameBufferIndex[i]].Buffer->SetUsedDataSize(Cmd->DataBuffers_p[CompressedFrameScatterPageIndex]->ScatterPages_p[i].BytesUsed);
                st_relayfs_write_se(ST_RELAY_TYPE_AUDIO_COMPRESSED_FRAME, ST_RELAY_SOURCE_SE, (uint8_t *) Cmd->DataBuffers_p[CompressedFrameScatterPageIndex]->ScatterPages_p[i].Page_p,
                                    CompressedFrameBufferSize, 0);
            }
            CompressedOutputBufAvailable = true;
        }
    }

    OS_LockMutex(&Lock);

    if (AuxOutputEnable)
    {
        MME_Command_t *Cmd = &DecodeContext->BaseContext.MMECommand;
        int32_t AuxFrameBufferIndex = (TranscodeOutputBufAvailable) ?
                                      STREAM_BASE_TRANSCODE_BUFFER_INDEX + 1 : STREAM_BASE_TRANSCODE_BUFFER_INDEX;
        AuxFrameBufferIndex = (CompressedOutputBufAvailable) ? AuxFrameBufferIndex + 1 : AuxFrameBufferIndex;

        int32_t  AuxBufferSize = Cmd->DataBuffers_p[AuxFrameBufferIndex]->ScatterPages_p[0].BytesUsed;

        st_relayfs_write_se(ST_RELAY_TYPE_DECODED_AUDIO_AUX_BUFFER0 + RelayfsIndex, ST_RELAY_SOURCE_SE,
                            (uint8_t *) Cmd->DataBuffers_p[AuxFrameBufferIndex]->ScatterPages_p[0].Page_p, AuxBufferSize, 0);

    }

    ParsedAudioParameters_t *AudioParameters = BufferState[DecodeContext->BaseContext.BufferIndex].ParsedAudioParameters;
    if (AudioParameters == NULL)
    {
        SE_ERROR("(%s) - AudioParameters are NULL\n", Configuration.CodecName);
        OS_UnLockMutex(&Lock);
        return CodecError;
    }

    AudioParameters->Source.BitsPerSample       = AudioOutputSurface->BitsPerSample;
    AudioParameters->Source.ChannelCount        = AudioOutputSurface->ChannelCount;
    AudioParameters->Organisation               = Status->AudioMode;
    AudioParameters->SampleCount                = Status->NbOutSamples;
    enum eAccFsCode  SamplingFreqCode           = Status->SamplingFreq;

    if (SamplingFreqCode < ACC_FS_reserved)
    {
        AudioParameters->Source.SampleRateHz    = StmSeTranslateDiscreteSamplingFrequencyToInteger(SamplingFreqCode);
    }
    else
    {
        AudioParameters->Source.SampleRateHz    = 48000;
        SE_ERROR("(%s) - Audio decode bad sampling freq returned setting default sampling frequency = 48000 Hz : 0x%x\n",
                 Configuration.CodecName, (int) SamplingFreqCode);
    }

    if (SE_IS_DEBUG_ON(group_decoder_audio))
    {
        SE_DEBUG(group_decoder_audio, "AudioParameters                %p\n", AudioParameters);
        SE_DEBUG(group_decoder_audio, "  Source.BitsPerSample         %d\n", AudioParameters->Source.BitsPerSample);
        SE_DEBUG(group_decoder_audio, "  Source.ChannelCount          %d\n", AudioParameters->Source.ChannelCount);
        SE_DEBUG(group_decoder_audio, "  Organisation                 %d\n", AudioParameters->Organisation);
        SE_DEBUG(group_decoder_audio, "  SampleCount                  %d\n", AudioParameters->SampleCount);
        SE_DEBUG(group_decoder_audio, "  Source.SampleRateHz          %d\n", AudioParameters->Source.SampleRateHz);
    }

    //! This is probably the right time to synthesise a PTS
    Buffer_t TheCurrentDecodeBuffer = BufferState[DecodeContext->BaseContext.BufferIndex].Buffer; // CurrentDecodeBuffer;

    ParsedFrameParameters_t *DecodedFrameParsedFrameParameters;
    TheCurrentDecodeBuffer->ObtainMetaDataReference(Player->MetaDataParsedFrameParametersReferenceType, (void **)(&DecodedFrameParsedFrameParameters));
    SE_ASSERT(DecodedFrameParsedFrameParameters != NULL);

    long long       CalculatedDelta         = ((unsigned long long)AudioParameters->SampleCount * 1000000ull) /
                                              ((unsigned long long)AudioParameters->Source.SampleRateHz);
    // DTS is used post decode to handle startup delay, so we preserve the PTS DTS normalized delta
    DecodedFrameParsedFrameParameters->NativeDecodeTime              = INVALID_TIME;

    if (ValidTime(DecodedFrameParsedFrameParameters->NormalizedDecodeTime) &&
        ValidTime(DecodedFrameParsedFrameParameters->NormalizedPlaybackTime))
    {
        DecodedFrameParsedFrameParameters->NormalizedDecodeTime     -= DecodedFrameParsedFrameParameters->NormalizedPlaybackTime;
    }
    else
    {
        DecodedFrameParsedFrameParameters->NormalizedDecodeTime      = INVALID_TIME;
    }

    //
    // suck out the PTS from the codec's reply
    //
    DecodedFrameParsedFrameParameters->NativeTimeFormat = StmSeConvertFwTimeFormatToPlayerTimeFormat((enum ePtsTimeFormat)(Status->PTSflag.Bits.PtsTimeFormat));
    stm_se_time_format_t NativeTimeFormat = (stm_se_time_format_t) DecodedFrameParsedFrameParameters->NativeTimeFormat;
    if (ACC_isPTS_PRESENT(Status->PTSflag.Bits.PTS_DTS_FLAG))
    {
        TimeStamp_c FwPts = TimeStamp_c(Status->PTS, NativeTimeFormat);
        DecodedFrameParsedFrameParameters->NativePlaybackTime       = FwPts.Value(NativeTimeFormat);
        DecodedFrameParsedFrameParameters->NormalizedPlaybackTime   = FwPts.uSecValue();
        SE_DEBUG(group_decoder_audio, "Status.PTS %llx\n", Status->PTS);
    }
    else if (ValidTime(LastNormalizedPlaybackTime))
    {
        // synthesise a PTS
        TimeStamp_c  SynthesisePts    = TimeStamp_c(LastNormalizedPlaybackTime, TIME_FORMAT_US);
        DecodedFrameParsedFrameParameters->NativePlaybackTime       = SynthesisePts.Value(NativeTimeFormat);
        DecodedFrameParsedFrameParameters->NormalizedPlaybackTime   = SynthesisePts.uSecValue();
        SE_DEBUG(group_decoder_audio, "Synthesized PTS uSec %llx\n", SynthesisePts.uSecValue());
    }
    else
    {
        DecodedFrameParsedFrameParameters->NativePlaybackTime       = 0;
        DecodedFrameParsedFrameParameters->NormalizedPlaybackTime   = 0;
    }

    // Restore DTS values

    if (ValidTime(DecodedFrameParsedFrameParameters->NormalizedDecodeTime))
    {
        DecodedFrameParsedFrameParameters->NormalizedDecodeTime += DecodedFrameParsedFrameParameters->NormalizedPlaybackTime;
    }

    // Squawk if time does not progress quite as expected.
    if (ValidTime(LastNormalizedPlaybackTime))
    {
        // measure the delta with the previously computed buffer
        long long   RealDelta                       = DecodedFrameParsedFrameParameters->NormalizedPlaybackTime - LastNormalizedPlaybackTime;
        long long   PtsJitterTollerenceThreshold    = 1000; // some file formats specify pts times to 1 ms accuracy

        // Check that the predicted and actual times deviate by no more than the threshold
        if ((RealDelta < -PtsJitterTollerenceThreshold || RealDelta > PtsJitterTollerenceThreshold) &&
            (Player->PolicyValue(Playback, Stream, PolicyAVDSynchronization) == PolicyValueApply))
        {
            SE_WARNING("(%s)Unexpected change in playback time. Expected %lldus, got %lldus (deltas: %lld )\n", \
                       Configuration.CodecName, \
                       LastNormalizedPlaybackTime, \
                       DecodedFrameParsedFrameParameters->NormalizedPlaybackTime, \
                       RealDelta);
        }
    }

    // Record the PlaybackTime of next expected buffer
    LastNormalizedPlaybackTime      = DecodedFrameParsedFrameParameters->NormalizedPlaybackTime + CalculatedDelta;

    /* Check if we have EOF TAG from the FW.
       In that case put the Marker Frame SequenceNumberStructure into the ring  */

    if (Status->PTSflag.Bits.FrameType == STREAMING_DEC_EOF)
    {
        if (!PutMarkerFrameToTheRing)
        {
            PutMarkerFrameToTheRing = true;
            /* When we use attched buffer reference of "CodedFrameBuffer" with Current
            Transfom Buffer to send the Marker buffer in the ring then the last decoded
            frame will be discarded because we play the buffers till *marker sequence
            number* excluding marker frame. To overcome this we need to put the marker frame to the ring with the *next
            buffer* after the EOF received from the FW.
            So put the marker frame into the ring only from the second decoded buffer marked with EOF*/
        }
        else
        {
            Buffer_t AttachedCodedFrameBuffer;
            TheCurrentDecodeBuffer->ObtainAttachedBufferReference(CodedFrameBufferType, &AttachedCodedFrameBuffer);
            SE_ASSERT(AttachedCodedFrameBuffer != NULL);

            /* Get the CodedFrameBuffer of the current running Transfrom */
            PlayerSequenceNumber_t *SequenceNumberStructure;
            AttachedCodedFrameBuffer->ObtainMetaDataReference(Player->MetaDataSequenceNumberType, (void **)(&SequenceNumberStructure));
            SE_ASSERT(SequenceNumberStructure != NULL);

            memcpy(SequenceNumberStructure, &MarkerFrameSavedSequenceNumberStructure, sizeof(PlayerSequenceNumber_t));
            // EOF received from FW. Marker Frame put into the ring. Reset the flags used for EofMarker Frame.
            EofMarkerFrameReceived = false;
            SendBuffersCommandsIssuedTillMarkerFrame = 0;
            TransformCommandsToIssueBeforeAutoEof    = 0;
            DontIssueEof                             = true; // Eof is received for the FW. Now no need to issue any more Eof flag
        }
    }

    // Fill the parsed parameters with the metadata available in the stream : reported by the FW
    Codec_MmeAudioStream_c::FillStreamMetadata(AudioParameters, Status);
    OS_UnLockMutex(&Lock);
    // Validate the extended status (without propagating errors)
    (void) ValidatePcmProcessingExtendedStatus(Context,
                                               (MME_PcmProcessingFrameExtStatus_t *) &DecodeContext->DecodeStatus.PcmStatus);
    //SYSFS
    SetAudioCodecDecAttributes();
    // Check if a new AudioParameter event occurs
    PlayerEventRecord_t       myEvent;
    stm_se_play_stream_audio_parameters_t   newAudioParametersValues;
    memset(&newAudioParametersValues, 0, sizeof(stm_se_play_stream_audio_parameters_t));
    // no codec specific parameters => do not need to update myEvent and newAudioParametersValues
    CheckAudioParameterEvent(Status, (MME_PcmProcessingFrameExtStatus_t *) &DecodeContext->DecodeStatus.PcmStatus, &newAudioParametersValues, &myEvent);
    return CodecNoError;
}
//}}}

//{{{  DiscardQueuedDecodes
// /////////////////////////////////////////////////////////////////////////
//
//      When discarding queued decodes, poke the monitor task
//

CodecStatus_t   Codec_MmeAudioStream_c::DiscardQueuedDecodes(void)
{
    CodecStatus_t       Status;
    DontIssueEof                        = true; // We are discarding the buffers. In this cases no need to issue the Eof
    OS_LockMutex(&InputMutex);
    Status              = Codec_MmeBase_c::DiscardQueuedDecodes();
    OS_UnLockMutex(&InputMutex);
    OS_SetEvent(&IssueTransformCommandEvent);
    return Status;
}
//}}}
//{{{  CheckForMarkerFrameStall
// /////////////////////////////////////////////////////////////////////////
//
//      This function checks for a marker frame stall, where we have 1
//      or more coded frame buffers in the transformer, and 1 or more
//      decode buffers, and a marker frame waiting, but nothing is going
//      in or out of the transform.
//

CodecStatus_t   Codec_MmeAudioStream_c::CheckForMarkerFrameStall(void)
{
    Buffer_t                    LocalMarkerBuffer;
    unsigned long long          Now;

    if (MarkerBuffer != NULL)
    {
        Now     = OS_GetTimeInMicroSeconds();

        if (!InPossibleMarkerStallState ||
            (SendBuffersCommandsIssued  != StallStateSendBuffersCommandsIssued) ||
            (TransformCommandsCompleted != StallStateTransformCommandsCompleted))
        {
            TimeOfEntryToPossibleMarkerStallState       = Now;
            StallStateSendBuffersCommandsIssued         = SendBuffersCommandsIssued;
            StallStateTransformCommandsCompleted        = TransformCommandsCompleted;
            InPossibleMarkerStallState                  = true;
        }
        else if ((Now - TimeOfEntryToPossibleMarkerStallState) >= MAXIMUM_STALL_PERIOD)
        {
            LocalMarkerBuffer                           = TakeMarkerBuffer();

            if (LocalMarkerBuffer)
            {
                mOutputPort->Insert((uintptr_t)LocalMarkerBuffer);
            }

            InPossibleMarkerStallState                  = false;
        }
    }
    else
    {
        InPossibleMarkerStallState      = false;
    }

    return CodecNoError;
}
//}}}
//{{{  AbortMMECommands
////////////////////////////////////////////////////////////////////////////
///
/// Abort any pending MME_TRANSFORM / MME_SEND_BUFFER commands.
///
CodecStatus_t Codec_MmeAudioStream_c::AbortMMECommands(BufferPool_t CommandContextPool)
{
    BufferStatus_t              Status;
    Buffer_t                    AllocatedBuffers[DEFAULT_COMMAND_CONTEXT_COUNT] = { 0 };
    CodecBaseDecodeContext_t   *OutstandingCommandContexts[DEFAULT_COMMAND_CONTEXT_COUNT] = { 0 };
    unsigned int                NumOutstandingCommands = 0;
    unsigned long long          TimeOut;

    // Check the  the local context pool to cancel MME_TRANSFORM/MME_SEND_BUFFER commands
    if (CommandContextPool == NULL)
    {
        SE_DEBUG(group_decoder_audio, "(%s) No context pool - no commands to abort\n", Configuration.CodecName);
        return CodecNoError;
    }

    // Scan the local context pool to cancel MME_TRANSFORM/MME_SEND_BUFFERS commands
    Status = CommandContextPool->GetAllUsedBuffers(DEFAULT_COMMAND_CONTEXT_COUNT, AllocatedBuffers, 0);
    if (Status != BufferNoError)
    {
        SE_ERROR("(%s) Could not get handles for in-use buffers\n", Configuration.CodecName);
        return CodecError;
    }

    for (int i = 0; i < DEFAULT_COMMAND_CONTEXT_COUNT; i++)
    {
        if (AllocatedBuffers[i])
        {
            AllocatedBuffers[i]->ObtainDataReference(NULL, NULL, (void **)&OutstandingCommandContexts[i]);
            if (OutstandingCommandContexts[i] == NULL)
            {
                SE_ERROR("(%s) Could not get data reference for in-use buffer at %p\n", Configuration.CodecName,
                         AllocatedBuffers[i]);
                return CodecError;
            }

            // Having got the context, we can now release the hold we acquired through GetAllUsedBuffers()
            AllocatedBuffers[i]->DecrementReferenceCount();
            SE_DEBUG(group_decoder_audio, "Selected command %08x\n", OutstandingCommandContexts[i]->MMECommand.CmdStatus.CmdId);
            NumOutstandingCommands++;
        }
    }

    SE_DEBUG(group_decoder_audio, "Waiting for %d MME command(s) to abort\n", NumOutstandingCommands);
    TimeOut             = OS_GetTimeInMicroSeconds() + 5000000;

    while (NumOutstandingCommands && OS_GetTimeInMicroSeconds() < TimeOut)
    {
        // Scan for any completed commands
        for (unsigned int i = 0; i < NumOutstandingCommands; /* no iterator */)
        {
            MME_Command_t      &Command = OutstandingCommandContexts[i]->MMECommand;
            SE_DEBUG(group_decoder_audio, "Command %08x  State %d\n", Command.CmdStatus.CmdId, Command.CmdStatus.State);

            // It might, perhaps, looks a little odd to check for a zero command identifier here. Basically
            // the callback action calls ReleaseDecodeContext() which will zero the structures. We really
            // ought to use a better technique to track in-flight commands (and possible move the call to
            // ReleaseDecodeContext() into the Stream playback thread.
            if (0 == Command.CmdStatus.CmdId ||
                MME_COMMAND_COMPLETED == Command.CmdStatus.State ||
                MME_COMMAND_FAILED == Command.CmdStatus.State)
            {
                SE_DEBUG(group_decoder_audio, "Retiring command %08x\n", Command.CmdStatus.CmdId);
                OutstandingCommandContexts[i]           = OutstandingCommandContexts[NumOutstandingCommands - 1];
                NumOutstandingCommands--;
            }
            else
            {
                i++;
            }
        }

        // Issue the aborts to the co-processor
        for (unsigned int i = 0; i < NumOutstandingCommands; i++)
        {
            MME_Command_t      &Command = OutstandingCommandContexts[i]->MMECommand;
            SE_DEBUG(group_decoder_audio, "Aborting command %08x\n", Command.CmdStatus.CmdId);
            MME_ERROR   Error   = MME_AbortCommand(MMEHandle, Command.CmdStatus.CmdId);

            if (MME_SUCCESS != Error)
            {
                if ((Error == MME_INVALID_ARGUMENT) &&
                    ((Command.CmdStatus.State == MME_COMMAND_COMPLETED) || (Command.CmdStatus.State == MME_COMMAND_FAILED)))
                    SE_DEBUG(group_decoder_audio,  "Ignored error during abort (command %08x already complete)\n",
                             Command.CmdStatus.CmdId);
                else
                    SE_ERROR("(%s) Cannot issue abort on command %08x (error = %d) (state = %d) (Command error = %d)\n", Configuration.CodecName,
                             Command.CmdStatus.CmdId, Error, Command.CmdStatus.State, Command.CmdStatus.Error);
            }
        }

        // Allow a little time for the co-processor to react
        OS_SleepMilliSeconds(100);
    }

    if (NumOutstandingCommands > 0)
    {
        SE_ERROR("(%s) Timed out waiting for %d commands to abort\n", Configuration.CodecName, NumOutstandingCommands);
        return CodecError;
    }

    return CodecNoError;
}
//}}}

//{{{  DumpSetStreamParameters
// /////////////////////////////////////////////////////////////////////////
//
//      Function to dump out the set stream
//      parameters from an mme command.
//

CodecStatus_t   Codec_MmeAudioStream_c::DumpSetStreamParameters(void    *Parameters)
{
    SE_ERROR("(%s) Not implemented\n", Configuration.CodecName);
    return CodecNoError;
}
//}}}
//{{{  DumpDecodeParameters
// /////////////////////////////////////////////////////////////////////////
//
//      Function to dump out the decode
//      parameters from an mme command.
//

CodecStatus_t   Codec_MmeAudioStream_c::DumpDecodeParameters(void    *Parameters)
{
    SE_VERBOSE(group_decoder_audio, " TotalSize[0]                  %d\n", DecodeContext->MMEBuffers[0].TotalSize);
    SE_VERBOSE(group_decoder_audio, " Page_p[0]                     %p\n", DecodeContext->MMEPages[0].Page_p);
    SE_VERBOSE(group_decoder_audio, " TotalSize[1]                  %d\n", DecodeContext->MMEBuffers[1].TotalSize);
    SE_VERBOSE(group_decoder_audio, " Page_p[1]                     %p\n", DecodeContext->MMEPages[1].Page_p);
    return CodecNoError;
}
//}}}

//{{{  CallbackFromMME
// /////////////////////////////////////////////////////////////////////////
//
//      Callback function from MME
//
//

void   Codec_MmeAudioStream_c::CallbackFromMME(MME_Event_t Event, MME_Command_t *CallbackData)
{
    CodecBaseDecodeContext_t           *DecodeContext;
    SE_VERBOSE(group_decoder_audio, "CmdId %x  CmdCode %s  State %d  Error %d\n",
               CallbackData->CmdStatus.CmdId,
               CallbackData->CmdCode == MME_SET_GLOBAL_TRANSFORM_PARAMS ? "MME_SET_GLOBAL_TRANSFORM_PARAMS" :
               CallbackData->CmdCode == MME_SEND_BUFFERS ? "MME_SEND_BUFFERS" :
               CallbackData->CmdCode == MME_TRANSFORM ? "MME_TRANSFORM" : "UNKNOWN",
               CallbackData->CmdStatus.State,
               CallbackData->CmdStatus.Error);
    Codec_MmeBase_c::CallbackFromMME(Event, CallbackData);

    //
    // Switch to perform appropriate actions per command
    //

    switch (CallbackData->CmdCode)
    {
    case MME_SET_GLOBAL_TRANSFORM_PARAMS:
        break;

    case MME_TRANSFORM:
        if (Event == MME_COMMAND_COMPLETED_EVT)
        {
            TransformCommandsCompleted++;
            OS_SetEvent(&IssueTransformCommandEvent);
        }

        break;

    case MME_SEND_BUFFERS:
        DecodeContext               = (CodecBaseDecodeContext_t *)CallbackData;
        OS_LockMutex(&Lock);
        ReleaseDecodeContext(DecodeContext);
        OS_UnLockMutex(&Lock);
        OS_SetEvent(&IssueSendBufferEvent);
        SendBuffersCommandsCompleted++;
        break;

    default:
        break;
    }
}
//}}}

////////////////////////////////////////////////////////////////////////////
///
/// Attach the Input buffer (coded frame buffer) .
/// Note that in the stream base decoding transcoded buffer belong to Transfom command
/// but here we are attaching input buffer (SendBuffer command) so no transcoded buffer should be attached.
/// Transcoded buffer will be attched in the transform thread
///
void Codec_MmeAudioStream_c::AttachCodedFrameBuffer(void)
{
    DecodeContextBuffer->AttachBuffer(CodedFrameBuffer);
}

////////////////////////////////////////////////////////////////////////////
///
///  Set Default StreamBase style TRANSFORM command IOs
///  Populate TransfomContext with 0 Input and 1 output Buffer
///  Populate I/O MME_DataBuffers
//
//   This function must be mutex-locked by caller
//
void Codec_MmeAudioStream_c::PresetIOBuffers(void)
{
    CodecBufferState_t      *State;
    OS_AssertMutexHeld(&Lock);
    // plumbing
    TransformContext->MMEBufferList[0]                  = &TransformContext->MMEBuffers[0];
    memset(&TransformContext->MMEBuffers[0], 0, sizeof(MME_DataBuffer_t));
    TransformContext->MMEBuffers[0].StructSize          = sizeof(MME_DataBuffer_t);
    TransformContext->MMEBuffers[0].NumberOfScatterPages = 1;
    TransformContext->MMEBuffers[0].ScatterPages_p      = &TransformContext->MMEPages[0];
    memset(&TransformContext->MMEPages[0], 0, sizeof(MME_ScatterPage_t));
    // output
    State                                               = &BufferState[CurrentDecodeBufferIndex];
    TransformContext->MMEBuffers[0].TotalSize          = Stream->GetDecodeBufferManager()->ComponentSize(State->Buffer, PrimaryManifestationComponent);
    TransformContext->MMEPages[0].Page_p               = Stream->GetDecodeBufferManager()->ComponentBaseAddress(State->Buffer, PrimaryManifestationComponent);
    TransformContext->MMEPages[0].Size                 = Stream->GetDecodeBufferManager()->ComponentSize(State->Buffer, PrimaryManifestationComponent);
}


////////////////////////////////////////////////////////////////////////////
///
///  Set Default StreamBase style TRANSFORM command for AudioDecoder MT
///  with 0 Input Buffer and 1 Output Buffer.
//
//   This function must be mutex-locked by caller
//
void Codec_MmeAudioStream_c::SetCommandIO(void)
{
    OS_AssertMutexHeld(&Lock);
    PresetIOBuffers();
    // StreamBase Transformer :: 0 Input Buffer / 1 Output Buffer sent through 1 MME_TRANSFORM
    TransformContext->MMECommand.NumberInputBuffers     = 0;
    TransformContext->MMECommand.NumberOutputBuffers    = 1;
    TransformContext->MMECommand.DataBuffers_p          = TransformContext->MMEBufferList;
}

/// Functions below are all used by the transform thread to init, issue and abort
/// MME_TRANSFORM commands.  They all correspond to similar functions used for
/// managing MME_SEND_BUFFERS commands above or in super classes.

//{{{  FillOutTransformContext
////////////////////////////////////////////////////////////////////////////
///
/// Fill out class contextual variable required to issue an MME_TRANSFORM command.
///
/// This is the corollary of FillOutDecodeContext although it has significantly more code
/// in order to deal with the Stream buffer pool.
///
CodecStatus_t   Codec_MmeAudioStream_c::FillOutTransformContext(void)
{
    CodecStatus_t               Status;
    OS_LockMutex(&Lock);

    // Obtain a new buffer if needed
    if (CurrentDecodeBufferIndex == INVALID_INDEX)
    {
        BufferStatus_t BufferStatus = TransformCodedFramePool->GetBuffer(&CodedFrameBuffer, UNSPECIFIED_OWNER, 4, false);
        if (BufferStatus != BufferNoError)
        {
            SE_ERROR("(%s) Failed to get a coded buffer\n", Configuration.CodecName);
            //CurrentDecodeBuffer->DecrementReferenceCount();
            ReleaseDecodeContext(TransformContext);
            OS_UnLockMutex(&Lock);
            return CodecError;
        }

        // Set up base parsed frame params pointer to point at our coded frame buffer
        CodedFrameBuffer->ObtainMetaDataReference(Player->MetaDataParsedFrameParametersType, (void **)(&ParsedFrameParameters));
        SE_ASSERT(ParsedFrameParameters != NULL);

        memcpy(ParsedFrameParameters, &SavedParsedFrameParameters, sizeof(ParsedFrameParameters_t));
        Status = GetDecodeBuffer();
        CodedFrameBuffer->DecrementReferenceCount();            // Now attached to decode buffer or failed

        if (Status != CodecNoError)
        {
            SE_ERROR("(%s) Failed to get decode buffer\n", Configuration.CodecName);
            ReleaseDecodeContext(TransformContext);
            OS_UnLockMutex(&Lock);
            return CodecError;
        }

        // attach coded frame (with copied params) to decode buffer
        BufferStatus = TransformContextBuffer->AttachBuffer(CodedFrameBuffer);
        if (BufferStatus != CodecNoError)
        {
            SE_ERROR("(%s) Failed to attach new coded frame to decode buffer\n", Configuration.CodecName);
            ReleaseDecodeContext(TransformContext);
            OS_UnLockMutex(&Lock);
            return CodecError;
        }

        ParsedFrameParameters->DisplayFrameIndex        = CurrentDecodeFrameIndex++;

        // copy the sequence structure across here
        PlayerSequenceNumber_t *SequenceNumberStructure;
        CodedFrameBuffer->ObtainMetaDataReference(Player->MetaDataSequenceNumberType, (void **)(&SequenceNumberStructure));
        SE_ASSERT(SequenceNumberStructure != NULL);

        memcpy(SequenceNumberStructure, &SavedSequenceNumberStructure, sizeof(PlayerSequenceNumber_t));
    }
    else
    {
        SE_ERROR("(%s) Already have valid CurrentDecodeBufferIndex\n", Configuration.CodecName);
        ReleaseDecodeContext(TransformContext);
        OS_UnLockMutex(&Lock);
        return CodecError;
    }

    // Now back as the corollary of FillOutDecodeContext (the code above is the buffer pool management)
    // Record the buffer being used in the decode context
    TransformContext->BufferIndex                       = CurrentDecodeBufferIndex;
    // We need to initialise the Decode Quality
    TransformContext->DecodeQuality = 1;
    memset(&TransformContext->MMECommand, 0, sizeof(MME_Command_t));
    SetCommandIO();
    TransformContext->DecodeInProgress                                  = true;
    BufferState[CurrentDecodeBufferIndex].DecodesInProgress++;
    BufferState[CurrentDecodeBufferIndex].OutputOnDecodesComplete       = true;
    OS_UnLockMutex(&Lock);
    return CodecNoError;
}
//}}}
//{{{  FillOutTransformCommand
////////////////////////////////////////////////////////////////////////////
///
/// Populate an MME_TRANSFORM command.
///
/// This is the corollary of Codec_MmeAudioStream_c::FillOutDecodeCommand .
///
CodecStatus_t   Codec_MmeAudioStream_c::FillOutTransformCommand(void)
{
    StreamAudioCodecTransformContext_t *Context = (StreamAudioCodecTransformContext_t *)TransformContext;
    // Initialize the frame parameters (we don't actually have much to say here)
    memset(&Context->DecodeParameters, 0, sizeof(Context->DecodeParameters));

    bool mute = false;
    Player->GetControl(Playback, Stream, STM_SE_CTRL_PLAY_STREAM_MUTE, &mute);
    Context->DecodeParameters.Cmd = mute ? ACC_CMD_MUTE : ACC_CMD_PLAY;

    // Zero the reply structure
    memset(&Context->DecodeStatus, 0, sizeof(Context->DecodeStatus));
    // Fill out the actual command
    Context->BaseContext.MMECommand.StructSize                          = sizeof(MME_Command_t);
    Context->BaseContext.MMECommand.CmdCode                             = MME_TRANSFORM;
    Context->BaseContext.MMECommand.CmdEnd                              = MME_COMMAND_END_RETURN_NOTIFY;
    Context->BaseContext.MMECommand.DueTime                             = 0;
    Context->BaseContext.MMECommand.CmdStatus.AdditionalInfoSize        = sizeof(Context->DecodeStatus);
    Context->BaseContext.MMECommand.CmdStatus.AdditionalInfo_p          = (MME_GenericParams_t)(&Context->DecodeStatus);
    Context->BaseContext.MMECommand.ParamSize                           = sizeof(Context->DecodeParameters);
    Context->BaseContext.MMECommand.Param_p                             = (MME_GenericParams_t)(&Context->DecodeParameters);
    return CodecNoError;
}
//}}}
//{{{  SendMMETransformCommand
////////////////////////////////////////////////////////////////////////////
///
/// Issue an MME_TRANSFORM command.
///
/// This is the corollary of Codec_MmeBase_c::SendMMEDecodeCommand from
/// which a substantial quantity of code has been copied.
///
/// \todo Verify the correct action if the component is halted
///
CodecStatus_t   Codec_MmeAudioStream_c::SendMMETransformCommand(void)
{
    MMECommandPreparedCount++;

    // Check that we have not commenced shutdown.
    if (TestComponentState(ComponentHalted))
    {
        SE_ERROR("(%s) Attempting to send Stream transform command when component is halted\n", Configuration.CodecName);
        MMECommandAbortedCount++;
        // XXX: This code was refactored from Codec_MmeAudioStream_c::StreamThread(), it used to cause the
        //      Immediate death of the thread (instant return), this felt wrong so instead report success and
        //      rely on other components to examine the component state. Once this path has been tested
        //      the SE_ERROR() can probably be removed.
        return CodecNoError;
    }

#ifdef DUMP_COMMANDS
    DumpMMECommand(&TransformContext->MMECommand);
#endif
    // Perform the mme transaction
    TransformContextBuffer                      = NULL;
    TransformContext->DecodeCommenceTime        = OS_GetTimeInMicroSeconds();
    MME_ERROR MMEStatus                         = MME_SendCommand(MMEHandle, &TransformContext->MMECommand);

    if (MMEStatus != MME_SUCCESS)
    {
        SE_ERROR("(%s) Unable to send decode command (%08x)\n", Configuration.CodecName, MMEStatus);
        return CodecError;
    }

    TransformContext                        = NULL;
    TransformCommandsIssued++;

    return CodecNoError;
}
//}}}
//{{{  FinishedTransform
////////////////////////////////////////////////////////////////////////////
///
/// Tidy up the contextual variables after issuing an MME_TRANSFORM command.
///
/// This is the corollary of Codec_MmeAudio_c::FinishedDecode .
///
void Codec_MmeAudioStream_c::FinishedTransform(void)
{
    OS_LockMutex(&Lock);
    //
    // We have finished decoding into this buffer
    //
    CurrentDecodeBufferIndex        = INVALID_INDEX;
    CurrentDecodeIndex              = INVALID_INDEX;
    OS_UnLockMutex(&Lock);
}
//}}}

//{{{  TransformThread
////////////////////////////////////////////////////////////////////////////
///
/// Stream transform management thread.
///
/// \b WARNING: This method is public only to allow it to be called from a
///             C linkage callback. Do not call it directly.
///
/// This is basically an adaptation of Codec_MmeAudio_c::Input to dispatch
/// the TRANSFORM commands to match the SEND_BUFFERS done by the main code
/// that will be driven directly by the Codec_MmeAudio_c::Input.
/// Various items called by Codec_MmeAudio_c::Input are duplicated here to
/// be customised to the requirements or to detach them from member variables
/// that we have duplicated for this half of the process.
///
void Codec_MmeAudioStream_c::TransformThread(void)
{
    CodecStatus_t       Status;
    unsigned int        HighWatermarkOfDiscardDecodesUntil = 0;
    int                 BuffersAvailable        = 0;
    int                 TransformsActive        = 0;
    Buffer_t            CodedDataBuffer;
    //unsigned int        ContextsInTransformPool, TransformContextsInUse;
    //unsigned int        ContextsInDecodePool, DecodeContextsInUse;
    SE_INFO(group_decoder_audio, "Starting Transform Thread\n");

    while (TransformThreadRunning)
    {
        OS_WaitForEventAuto(&IssueTransformCommandEvent, 1000);

        OS_ResetEvent(&IssueTransformCommandEvent);
        //TransformContextPool->GetPoolUsage (&ContextsInTransformPool, &TransformContextsInUse, NULL, NULL, NULL);
        //DecodeContextPool->GetPoolUsage    (&ContextsInDecodePool, &DecodeContextsInUse, NULL, NULL, NULL);
        //SE_INFO(group_decoder_audio, " SendBuffers %d, %d, Contexts %d, %d Transforms %d, %d, Contexts %d, %d\n",
        //         SendBuffersCommandsIssued, SendBuffersCommandsCompleted,
        //         ContextsInDecodePool, DecodeContextsInUse,
        //         TransformCommandsIssued, TransformCommandsCompleted,
        //         ContextsInTransformPool, TransformContextsInUse);
        OS_LockMutex(&InputMutex);

        //{{{  Check if need to discard current data.
        if (DiscardDecodesUntil > HighWatermarkOfDiscardDecodesUntil)
        {
            SE_DEBUG(group_decoder_audio, "Commands prepared %d, CommandsCompleted %d)\n", MMECommandPreparedCount, MMECommandCompletedCount);
            SE_DEBUG(group_decoder_audio, "SendBuffers %d, %d, Transforms %d, %d,\n",
                     SendBuffersCommandsIssued, SendBuffersCommandsCompleted,
                     TransformCommandsIssued, TransformCommandsCompleted);
            DontIssueEof                        = true; // We are discarding the buffers (issuing the Abort). In this cases no need to issue the Eof
            HighWatermarkOfDiscardDecodesUntil      = DiscardDecodesUntil;
            Status = AbortMMECommands(TransformContextPool);
            if (Status != CodecNoError)
            {
                SE_ERROR("(%s) Could not abort all pending MME_TRANSFORM commands\n", Configuration.CodecName);
                // no recovery possible
            }

            OS_LockMutex(&DecodeContextPoolMutex);
            Status = AbortMMECommands(DecodeContextPool);
            OS_UnLockMutex(&DecodeContextPoolMutex);

            if (Status != CodecNoError)
            {
                SE_ERROR("(%s) Could not abort all pending MME_SEND_BUFFERS commands\n", Configuration.CodecName);
                // no recovery possible
            }

            ForceStreamParameterReload          = true;
            HighWatermarkOfDiscardDecodesUntil  = 0;
            DiscardDecodesUntil                 = 0;
            // We now expect a jump in the pts time, so we avoid any unsavoury errors, by invalidating our record
            LastNormalizedPlaybackTime          = UNSPECIFIED_TIME;
        }

        //}}}
        //{{{  Conditionally issue a new MME_TRANSFORM command
        BuffersAvailable            = SendBuffersCommandsIssued - SendBuffersCommandsCompleted;
        TransformsActive            = TransformCommandsIssued - TransformCommandsCompleted;

        /* If EofMarkerFrameReceived is true then we need to send transfrom command even if we have 0 active SendBuffers.
           So that FW can flush the stored samples.
           EofMarkerFrameReceived will make false by ValidateDecodeContext after receiving Eof TAG from the FW */

        while ((((BuffersAvailable - TransformsActive) >= SendbufTriggerTransformCount) || (EofMarkerFrameReceived)) &&
               (!TestComponentState(ComponentHalted)) && TransformThreadRunning &&
               TransformCodedFramePool && (Configuration.DecodeContextCount - TransformsActive > 0))
        {
            if (TransformContextBuffer != NULL)
            {
                SE_ERROR("(%s) Already have a decode context\n", Configuration.CodecName);
                break;
            }

            BufferStatus_t BufferStatus = TransformContextPool->GetBuffer(&TransformContextBuffer);
            if (BufferStatus != BufferNoError)
            {
                SE_ERROR("(%s) Fail to get decode context\n", Configuration.CodecName);
                break;
            }

            unsigned int TransformContextSize;
            TransformContextBuffer->ObtainDataReference(&TransformContextSize, NULL, (void **)&TransformContext);
            SE_ASSERT(TransformContext != NULL); // not expected to be empty
            memset(TransformContext, 0, TransformContextSize);
            TransformContext->DecodeContextBuffer   = TransformContextBuffer;
            TransformContext->DecodeQuality = 1;  // rational

            Status = FillOutTransformContext();
            if (Status != CodecNoError)
            {
                SE_ERROR("(%s) Cannot fill out transform context\n", Configuration.CodecName);
                break;
            }

            Status = FillOutTransformCommand();
            if (Status != CodecNoError)
            {
                SE_ERROR("(%s) Cannot fill out transform command\n", Configuration.CodecName);
                break;
            }

            // Ensure that the coded frame will be available throughout the
            // life of the decode by attaching the coded frame to the decode
            // context prior to launching the decode.
            TransformContextBuffer->AttachBuffer(CodedFrameBuffer);

            if (AuxOutputEnable)
            {
                TransformContextBuffer->AttachBuffer(CurrentAuxBuffer);
                CurrentAuxBuffer->DecrementReferenceCount();
                CurrentAuxBuffer = NULL;
            }

            // Attach the transcoded buffer if transcoding is enabled
            if ((TranscodeEnable && TranscodeNeeded) || (CompressedFrameEnable && CompressedFrameNeeded))
            {
                CurrentDecodeBuffer->ObtainAttachedBufferReference(CodedFrameBufferType, &CodedDataBuffer);
                if (CodedDataBuffer == NULL)
                {
                    SE_ERROR("Could not get the attached coded data buffer (%d)\n", BufferStatus);
                    return;
                }
            }

            if (TranscodeEnable && TranscodeNeeded)
            {
                CodedDataBuffer->AttachBuffer(CurrentTranscodeBuffer);
                CurrentTranscodeBuffer->DecrementReferenceCount();
                CurrentTranscodeBuffer = NULL;
                // the transcoded buffer is now only referenced by its attachment to the coded buffer
            }

            // Attach the compressed buffer if CompressedFrame is enabled
            if (CompressedFrameEnable && CompressedFrameNeeded)
            {
                for (uint32_t i = 0; i < NoOfCompressedFrameBuffers; i++)
                {
                    CodedDataBuffer->AttachBuffer(CurrentCompressedFrameBuffer[i]);
                    CurrentCompressedFrameBuffer[i]->DecrementReferenceCount();
                    CurrentCompressedFrameBuffer[i] = NULL;
                }

                // the CompressedFrame buffer is now only referenced by its attachment to the coded buffer
            }

            Status = SendMMETransformCommand();
            if (Status != CodecNoError)
            {
                SE_ERROR("(%s) Failed to send a transform command\n", Configuration.CodecName);
                ReleaseDecodeContext(TransformContext);
                //! NULL them here, as ReleaseDecodeContext will only do for the MME_SEND_BUFFERS done with DecodeContext/etc
                TransformContext                    = NULL;
                TransformContextBuffer              = NULL;
                break;
            }

            FinishedTransform();
            BuffersAvailable                        = SendBuffersCommandsIssued - SendBuffersCommandsCompleted;
            TransformsActive                        = TransformCommandsIssued - TransformCommandsCompleted;

            /* It may be possible (may be in trick mode) that FW not generate Eof flag.
               To drain out we need to send the Marker Frame.
               So wait for *TransformCommandsToIssueBeforeAutoEof* transfom command and then put the Marker Frame  to unblock the drain */
            if ((EofMarkerFrameReceived) && (SendBuffersCommandsCompleted >= SendBuffersCommandsIssuedTillMarkerFrame))
            {
                TransformCommandsToIssueBeforeAutoEof--;
                DontIssueEof = true; // We have already issue the Eof frame now no need to issue for more buffers.

                if (TransformCommandsToIssueBeforeAutoEof <= 0)
                {
                    PlayerSequenceNumber_t *SequenceNumberStructure;
                    CodedFrameBuffer->ObtainMetaDataReference(Player->MetaDataSequenceNumberType, (void **)(&SequenceNumberStructure));
                    SE_ASSERT(SequenceNumberStructure != NULL);

                    memcpy(SequenceNumberStructure, &MarkerFrameSavedSequenceNumberStructure, sizeof(PlayerSequenceNumber_t));
                    EofMarkerFrameReceived = false;
                    SendBuffersCommandsIssuedTillMarkerFrame = 0;
                    TransformCommandsToIssueBeforeAutoEof    = 0;
                    // EOF not received from FW but we are generating preventive EOF.
                    SE_DEBUG(group_decoder_audio, "EOF Tag not received from the FW possible FW error. Auto generating the EOF\n");
                }
            }
        }

        //}}}
        CheckForMarkerFrameStall();
        OS_UnLockMutex(&InputMutex);
    } //while (TransformThreadRunning)

    OS_Smp_Mb(); // Read memory barrier: rmb_for_MmeAudioStream_Terminating coupled with: wmb_for_MmeAudioStream_Terminating

    // About to terminate, make sure there are no pending commands.
    Status = AbortMMECommands(TransformContextPool);
    if (Status != CodecNoError)
    {
        SE_ERROR("(%s)Could not abort all pending MME_TRANSFORM commands (resources will leak)\n", Configuration.CodecName);
    }

    OS_LockMutex(&DecodeContextPoolMutex);
    Status = AbortMMECommands(DecodeContextPool);
    OS_UnLockMutex(&DecodeContextPoolMutex);

    if (Status != CodecNoError)
    {
        SE_ERROR("(%s)Could not abort all pending MME_SEND_BUFFERS commands (resources will leak)\n", Configuration.CodecName);
    }

    SE_INFO(group_decoder_audio, "Terminating transform thread\n");
    OS_SetEvent(&TransformThreadTerminated);
}
//}}}

//{{{  OutputPartialDecodeBuffers
////////////////////////////////////////////////////////////////////////////
///
/// We override CodecMmeBase version as Drain() calls this, and for
/// audio_stream we need to protect against TransformThread as well
/// this is currenly achieved using InputMutex
///
CodecStatus_t    Codec_MmeAudioStream_c::OutputPartialDecodeBuffers(void)
{
    CodecStatus_t Status = CodecNoError;
    AssertComponentState(ComponentRunning);
    OS_LockMutex(&InputMutex);
    Status = Codec_MmeBase_c::OutputPartialDecodeBuffers();
    OS_UnLockMutex(&InputMutex);
    return Status;
}
//}}}
// /////////////////////////////////////////////////////////////////////////
//
//      Function to set the stream metadata according to what is contained
//      in the steam bitstream (returned by the codec firmware status)
//

void Codec_MmeAudioStream_c::FillStreamMetadata(ParsedAudioParameters_t *AudioParameters, MME_LxAudioDecoderFrameStatus_t *Status)
{
    StreamMetadata_t *Metadata = &AudioParameters->StreamMetadata;
    tMME_BufferFlags *Flags = (tMME_BufferFlags *) &Status->PTSflag;
    // direct mapping
    Metadata->FrontMatrixEncoded = Flags->FrontMatrixEncoded;
    Metadata->RearMatrixEncoded  = Flags->RearMatrixEncoded;
    Metadata->DialogNorm = Flags->DialogNorm;
}
