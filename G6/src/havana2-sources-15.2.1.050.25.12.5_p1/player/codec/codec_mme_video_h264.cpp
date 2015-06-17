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

#include "player_threads.h"

#include "ring_generic.h"
#include "player_generic.h"
#include "player_playback.h"
#include "player_stream.h"
#include "parse_to_decode_edge.h"

#include "codec_mme_video_h264.h"
#include "h264.h"

#undef TRACE_TAG
#define TRACE_TAG "Codec_MmeVideoH264_c"

//      Support Older MME APIs
#if (H264_MME_VERSION < 57)
// These Enumerations were renamed at Version 57
#define H264_DISP_AUX_EN       AUXOUT_EN
#define H264_DISP_MAIN_EN      MAINOUT_EN
#define H264_DISP_AUX_MAIN_EN  AUX_MAIN_OUT_EN
#endif

#define MAX_EVENT_WAIT          100             // ms

typedef struct H264CodecStreamParameterContext_s
{
    CodecBaseStreamParameterContext_t   BaseContext;

    H264_SetGlobalParam_t               StreamParameters;
} H264CodecStreamParameterContext_t;

#define BUFFER_H264_CODEC_STREAM_PARAMETER_CONTEXT             "H264CodecStreamParameterContext"
#define BUFFER_H264_CODEC_STREAM_PARAMETER_CONTEXT_TYPE        {BUFFER_H264_CODEC_STREAM_PARAMETER_CONTEXT, BufferDataTypeBase, AllocateFromOSMemory, 32, 0, true, true, sizeof(H264CodecStreamParameterContext_t)}

static BufferDataDescriptor_t H264CodecStreamParameterContextDescriptor = BUFFER_H264_CODEC_STREAM_PARAMETER_CONTEXT_TYPE;

// --------

typedef struct H264CodecDecodeContext_s
{
    CodecBaseDecodeContext_t            BaseContext;

    H264_TransformParam_fmw_t           DecodeParameters;
    H264_CommandStatus_fmw_t            DecodeStatus;
} H264CodecDecodeContext_t;

#define BUFFER_H264_CODEC_DECODE_CONTEXT       "H264CodecDecodeContext"
#define BUFFER_H264_CODEC_DECODE_CONTEXT_TYPE  {BUFFER_H264_CODEC_DECODE_CONTEXT, BufferDataTypeBase, AllocateFromOSMemory, 32, 0, true, true, sizeof(H264CodecDecodeContext_t)}

static BufferDataDescriptor_t H264CodecDecodeContextDescriptor = BUFFER_H264_CODEC_DECODE_CONTEXT_TYPE;

// --------

#define BUFFER_H264_PRE_PROCESSOR_BUFFER        "H264PreProcessorBuffer"

// /////////////////////////////////////////////////////////////////////////
//
//      Constructor function, fills in the codec specific parameter values
//

Codec_MmeVideoH264_c::Codec_MmeVideoH264_c(void)
    : Codec_MmeVideo_c()
    , H264TransformCapability()
    , H264InitializationParameters()
    , Terminating(false)
    , StartStopEvent()
    , SD_MaxMBStructureSize(0)
    , HD_MaxMBStructureSize(0)
    , DiscardFramesInPreprocessorChain(false)
    , FramesInPreprocessorChain()
    , H264Lock()
    , FramesInPreprocessorChainRing(NULL)
    , mPreProcessorBufferType()
    , mPreProcessorBufferMaxSize()
    , mH264PreProcessorBufferDescriptor()
    , PreProcessorBufferPool(NULL)
    , PreProcessorDevice(H264_PP_INVALID_DEVICE)
    , ReferenceFrameSlotUsed()
    , RecordedHostData()
    , OutstandingSlotAllocationRequest(INVALID_INDEX)
    , NumberOfUsedDescriptors(0)
    , DescriptorIndices()
    , LocalReferenceFrameList()
    , RasterOutput(false)
{
    Configuration.CodecName                             = "H264 video";
    Configuration.StreamParameterContextCount           = 4;                    // H264 tests use stream param change per frame
    Configuration.StreamParameterContextDescriptor      = &H264CodecStreamParameterContextDescriptor;
    Configuration.DecodeContextCount                    = 4;
    Configuration.DecodeContextDescriptor               = &H264CodecDecodeContextDescriptor;
    Configuration.MaxDecodeIndicesPerBuffer             = 2;
    Configuration.SliceDecodePermitted                  = false;
    Configuration.DecimatedDecodePermitted              = true;
    Configuration.TransformName[0]                      = H264_MME_TRANSFORMER_NAME "0";
    Configuration.TransformName[1]                      = H264_MME_TRANSFORMER_NAME "1";
    Configuration.AvailableTransformers                 = 2;
    Configuration.SizeOfTransformCapabilityStructure    = sizeof(H264_TransformerCapability_fmw_t);
    Configuration.TransformCapabilityStructurePointer   = (void *)(&H264TransformCapability);

    //
    // The video firmware violates the MME spec. and passes data buffer addresses
    // as parametric information. For this reason it requires physical addresses
    // to be used.
    //
    Configuration.AddressingMode                        = PhysicalAddress;
    //
    // Since we pre-process the data, we shrink the coded data buffers
    // before entering the generic code. as a consequence we do
    // not require the generic code to shrink buffers on our behalf,
    // and we do not want the generic code to find the coded data buffer
    // for us.
    //
    Configuration.ShrinkCodedDataBuffersAfterDecode = false;
    Configuration.IgnoreFindCodedDataBuffer         = true;
    //
    // My trick mode parameters
    //
    Configuration.TrickModeParameters.EmpiricalMaximumDecodeFrameRateShortIntegration   = 60;
    Configuration.TrickModeParameters.EmpiricalMaximumDecodeFrameRateLongIntegration    = 60;
    Configuration.TrickModeParameters.DecodeFrameRateShortIntegrationForIOnly           = 60;
    Configuration.TrickModeParameters.DecodeFrameRateLongIntegrationForIOnly            = 60;
    Configuration.TrickModeParameters.SubstandardDecodeSupported                        = false;
    Configuration.TrickModeParameters.SubstandardDecodeRateIncrease                     = Rational_t(4, 3);
    Configuration.TrickModeParameters.DefaultGroupSize                                  = 12;
    Configuration.TrickModeParameters.DefaultGroupReferenceFrameCount                   = 4;

    OS_InitializeMutex(&H264Lock);
    OS_InitializeEvent(&StartStopEvent);

    // TODO(pht) move FinalizeInit to a factory method
    InitializationStatus = FinalizeInit();
}

// /////////////////////////////////////////////////////////////////////////
//
//      FinalizeInit finalizes init work by doing operations that may fail
//      (and such not doable in ctor)
//
CodecStatus_t Codec_MmeVideoH264_c::FinalizeInit(void)
{
    // Create the ring used to inform the intermediate process of a datum on the way
    FramesInPreprocessorChainRing = new class RingGeneric_c(H264_CODED_FRAME_COUNT);
    if ((FramesInPreprocessorChainRing == NULL) ||
        (FramesInPreprocessorChainRing->InitializationStatus != RingNoError))
    {
        SE_ERROR("Failed to create ring to hold frames in pre-processor chain\n");
        return CodecError;
    }

    // Create the intermediate process
    OS_ResetEvent(&StartStopEvent);

    OS_Thread_t Thread;
    if (OS_CreateThread(&Thread, Codec_MmeVideoH264_IntermediateProcess, this, &player_tasks_desc[SE_TASK_VIDEO_H264INT]) != OS_NO_ERROR)
    {
        SE_ERROR("Failed to create intermediate process\n");
        return CodecError;
    }

    // Wait for intermediate process to run
    OS_Status_t WaitStatus;
    do
    {
        WaitStatus = OS_WaitForEventAuto(&StartStopEvent, MAX_EVENT_WAIT);
        if (WaitStatus == OS_TIMED_OUT)
        {
            SE_WARNING("Still waiting for intermediate process to run\n");
        }
    }
    while (WaitStatus == OS_TIMED_OUT);

    OS_ResetEvent(&StartStopEvent);

    return CodecNoError;
}

// /////////////////////////////////////////////////////////////////////////
//
//      Destructor function, ensures a full halt and reset
//      are executed for all levels of the class.
//

Codec_MmeVideoH264_c::~Codec_MmeVideoH264_c(void)
{
    Halt();

    //
    // Make sure the pre-processor device is closed (note this may be done in halt,
    // but we check again, since you can enter reset without halt during a failed
    // start up).
    //

    if (PreProcessorDevice != H264_PP_INVALID_DEVICE)
    {
        H264ppClose(PreProcessorDevice);
        PreProcessorDevice = H264_PP_INVALID_DEVICE;  // reset for intermediate process
    }

    //
    // Free buffer pools
    //
    if (PreProcessorBufferPool != NULL)
    {
        BufferManager->DestroyPool(PreProcessorBufferPool);
    }

    // lock & reset for intermediate process
    OS_LockMutex(&H264Lock);
    memset(FramesInPreprocessorChain, 0, H264_CODED_FRAME_COUNT * sizeof(FramesInPreprocessorChain_t));
    OS_UnLockMutex(&H264Lock);

    // Shutdown the intermediate process
    OS_ResetEvent(&StartStopEvent);
    OS_Smp_Mb(); // Write memory barrier: wmb_for_CodecH264_Terminating coupled with: rmb_for_CodecH264_Terminating
    Terminating = true;

    // Wait for intermediate process to terminate
    OS_Status_t WaitStatus;
    do
    {
        // FramesInPreprocessorChainRing->Extract use MAX_EVENT_WAIT timeout
        // let's use a wider timeout here to ensure Intermediate process have enough time to terminate
        WaitStatus = OS_WaitForEventAuto(&StartStopEvent, MAX_EVENT_WAIT * 2);
        if (WaitStatus == OS_TIMED_OUT)
        {
            SE_WARNING("Still waiting for intermediate process to stop\n");
        }
    }
    while (WaitStatus == OS_TIMED_OUT);

    //
    // Delete the frames in pre-processor chain ring
    //
    delete FramesInPreprocessorChainRing;

    //
    // Destroy the locking mutex
    //
    OS_TerminateEvent(&StartStopEvent);
    OS_TerminateMutex(&H264Lock);
}

// /////////////////////////////////////////////////////////////////////////
//
//      Halt function
//

CodecStatus_t   Codec_MmeVideoH264_c::Halt(void)
{
    return Codec_MmeVideo_c::Halt();
}

// /////////////////////////////////////////////////////////////////////////
//
//      Connect output port, we take this opportunity to
//      create the buffer pools for use in the pre-processor, and the
//      macroblock structure buffers
//

CodecStatus_t   Codec_MmeVideoH264_c::Connect(Port_c *Port)
{
    h264pp_status_t     PPStatus;
    int                 MemProfilePolicy;
    //
    // Let the ancestor classes setup the framework
    //
    CodecStatus_t Status = Codec_MmeVideo_c::Connect(Port);
    if (Status != CodecNoError)
    {
        SetComponentState(ComponentInError);
        return Status;
    }

    //
    // Create the pre-processor buffer pool
    //
    MemProfilePolicy = Player->PolicyValue(Playback,
                                           PlayerAllStreams, PolicyVideoPlayStreamMemoryProfile);

    if (MemProfilePolicy == PolicyValueVideoPlayStreamMemoryProfile4K2K)
    {
        mPreProcessorBufferMaxSize = PLAYER_VIDEO_4K2K_PP_BUFFER_MAXIMUM_SIZE;
    }
    else if (MemProfilePolicy == PolicyValueVideoPlayStreamMemoryProfileSD)
    {
        mPreProcessorBufferMaxSize = PLAYER_VIDEO_SD_PP_BUFFER_MAXIMUM_SIZE;
    }
    else
    {
        mPreProcessorBufferMaxSize = PLAYER_VIDEO_HD_PP_BUFFER_MAXIMUM_SIZE;
    }

    mH264PreProcessorBufferDescriptor.TypeName = BUFFER_H264_PRE_PROCESSOR_BUFFER;
    mH264PreProcessorBufferDescriptor.Type = BufferDataTypeBase;
    mH264PreProcessorBufferDescriptor.AllocationSource = AllocateFromNamedDeviceMemory;
    mH264PreProcessorBufferDescriptor.RequiredAlignment = 64;
    mH264PreProcessorBufferDescriptor.AllocationUnitSize = 0;
    mH264PreProcessorBufferDescriptor.HasFixedSize = false;
    mH264PreProcessorBufferDescriptor.AllocateOnPoolCreation = true;
    //FixedSize should not be there as size should change depending on memory profile selected,
    mH264PreProcessorBufferDescriptor.FixedSize = 0;

    BufferStatus_t BufferStatus = BufferManager->FindBufferDataType(BUFFER_H264_PRE_PROCESSOR_BUFFER, &mPreProcessorBufferType);
    if (BufferStatus != BufferNoError)
    {
        BufferStatus = BufferManager->CreateBufferDataType(&mH264PreProcessorBufferDescriptor, &mPreProcessorBufferType);
        if (BufferStatus != BufferNoError)
        {
            SE_ERROR("Failed to create the %s buffer type (%08x)\n", BUFFER_H264_PRE_PROCESSOR_BUFFER, BufferStatus);
            SetComponentState(ComponentInError);
            return CodecError;
        }
    }

    BufferStatus = BufferManager->CreatePool(&PreProcessorBufferPool, mPreProcessorBufferType,
                                             Configuration.DecodeContextCount, mPreProcessorBufferMaxSize,
                                             NULL, NULL, Configuration.AncillaryMemoryPartitionName);
    if (BufferStatus != BufferNoError)
    {
        SE_ERROR("Failed to create pre-processor buffer pool (%08x)\n", BufferStatus);
        SetComponentState(ComponentInError);
        return CodecError;
    }

    PPStatus = H264ppOpen(&PreProcessorDevice);
    if (PPStatus != h264pp_ok)
    {
        SE_ERROR("Failed to open the pre-processor device\n");
        SetComponentState(ComponentInError);
        return CodecError;
    }

    return CodecNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
//      Output partial decode buffers - not entirely sure what to do here,
//      it may well be necessary to fire off the pre-processing of any
//      accumulated slices.
//

CodecStatus_t   Codec_MmeVideoH264_c::OutputPartialDecodeBuffers(void)
{
    uintptr_t    i;
    AssertComponentState(ComponentRunning);
    OS_LockMutex(&H264Lock);

    for (i = 0; i < H264_CODED_FRAME_COUNT; i++)
        if (!FramesInPreprocessorChain[i].Used)
        {
            memset(&FramesInPreprocessorChain[i], 0, sizeof(FramesInPreprocessorChain_t));
            FramesInPreprocessorChain[i].Used                                   = true;
            FramesInPreprocessorChain[i].Action                                 = ActionCallOutputPartialDecodeBuffers;
            FramesInPreprocessorChainRing->Insert(i);
            OS_UnLockMutex(&H264Lock);
            return CodecNoError;
        }

    OS_UnLockMutex(&H264Lock);
    SE_ERROR("Failed to find an entry in the \"FramesInPreprocessorChain\" list\n");
    return CodecError;
}


// /////////////////////////////////////////////////////////////////////////
//
//      Discard queued decodes, this will include any between here
//      and the preprocessor.
//

CodecStatus_t   Codec_MmeVideoH264_c::DiscardQueuedDecodes(void)
{
    uintptr_t    i;

    OS_LockMutex(&H264Lock);

    for (i = 0; i < H264_CODED_FRAME_COUNT; i++)
        if (!FramesInPreprocessorChain[i].Used)
        {
            DiscardFramesInPreprocessorChain            = true;
            memset(&FramesInPreprocessorChain[i], 0, sizeof(FramesInPreprocessorChain_t));
            FramesInPreprocessorChain[i].Used           = true;
            FramesInPreprocessorChain[i].Action         = ActionCallDiscardQueuedDecodes;
            FramesInPreprocessorChainRing->Insert(i);
            OS_UnLockMutex(&H264Lock);
            return CodecNoError;
        }

    OS_UnLockMutex(&H264Lock);
    SE_ERROR("Failed to find an entry in the \"FramesInPreprocessorChain\" list\n");
    return CodecError;
}


// /////////////////////////////////////////////////////////////////////////
//
//      Release reference frame, this must be capable of releasing a
//      reference frame that has not yet exited the pre-processor.
//

CodecStatus_t   Codec_MmeVideoH264_c::ReleaseReferenceFrame(unsigned int              ReferenceFrameDecodeIndex)
{
    OS_LockMutex(&H264Lock);

    for (int i = 0; i < H264_CODED_FRAME_COUNT; i++)
        if (!FramesInPreprocessorChain[i].Used)
        {
            memset(&FramesInPreprocessorChain[i], 0, sizeof(FramesInPreprocessorChain_t));
            FramesInPreprocessorChain[i].Used                   = true;
            FramesInPreprocessorChain[i].DecodeFrameIndex       = ReferenceFrameDecodeIndex;
            FramesInPreprocessorChain[i].Action                 = ActionCallReleaseReferenceFrame;
            FramesInPreprocessorChainRing->Insert(i);
            OS_UnLockMutex(&H264Lock);
            return CodecNoError;
        }

    OS_UnLockMutex(&H264Lock);

    SE_ERROR("Failed to find an entry in the \"FramesInPreprocessorChain\" list\n");
    return CodecError;
}


// /////////////////////////////////////////////////////////////////////////
//
//      Check reference frame list needs to account for those currently
//      in the pre-processor chain.
//

CodecStatus_t   Codec_MmeVideoH264_c::CheckReferenceFrameList(
    unsigned int              NumberOfReferenceFrameLists,
    ReferenceFrameList_t      ReferenceFrameList[])
{
    unsigned int  i, j, k;

    //
    // Check we can cope
    //

    if (NumberOfReferenceFrameLists > H264_NUM_REF_FRAME_LISTS)
    {
        SE_ERROR("H264 has only %d reference frame lists, requested to check %d\n", H264_NUM_REF_FRAME_LISTS, NumberOfReferenceFrameLists);
        return CodecUnknownFrame;
    }

    //
    // Construct local lists consisting of those elements we do not know about
    //
    OS_LockMutex(&H264Lock);

    for (i = 0; i < NumberOfReferenceFrameLists; i++)
    {
        LocalReferenceFrameList[i].EntryCount   = 0;

        for (j = 0; j < ReferenceFrameList[i].EntryCount; j++)
        {
            for (k = 0; k < H264_CODED_FRAME_COUNT; k++)
                if (FramesInPreprocessorChain[k].Used &&
                    (FramesInPreprocessorChain[k].DecodeFrameIndex == ReferenceFrameList[i].EntryIndicies[j]))
                {
                    break;
                }

            if (k == H264_CODED_FRAME_COUNT)
            {
                LocalReferenceFrameList[i].EntryIndicies[LocalReferenceFrameList[i].EntryCount++]       = ReferenceFrameList[i].EntryIndicies[j];
            }
        }
    }

    OS_UnLockMutex(&H264Lock);
    return Codec_MmeVideo_c::CheckReferenceFrameList(NumberOfReferenceFrameLists, LocalReferenceFrameList);
}


// /////////////////////////////////////////////////////////////////////////
//
//      Input, must feed the data to the preprocessor chain.
//      This function needs to replicate a considerable amount of the
//      ancestor classes, because this function is operating
//      in a different process to the ancestors, and because the
//      ancestor data, and the data used here, will be in existence
//      at the same time.
//

CodecStatus_t   Codec_MmeVideoH264_c::Input(Buffer_t                  CodedBuffer)
{
    uintptr_t                 i;
    h264pp_status_t           PPStatus;
    unsigned int              CodedDataLength;
    ParsedFrameParameters_t  *ParsedFrameParameters;
    ParsedVideoParameters_t  *ParsedVideoParameters;
    Buffer_t                  PreProcessorBuffer;
    H264FrameParameters_t    *FrameParameters;

    // Sanity check here to avoid filling in the FramesInPreprocessorChain
    if (PreProcessorBufferPool == NULL)
    {
        SE_ERROR("About to dereference PreProcessorBufferPool with NULL value\n");
        return CodecError;
    }

    BufferStatus_t Status;
    //
    // First extract the useful pointers from the buffer all held locally
    //
    CodedBuffer->ObtainMetaDataReference(Player->MetaDataParsedFrameParametersType, (void **)(&ParsedFrameParameters));
    SE_ASSERT(ParsedFrameParameters != NULL);

    CodedBuffer->ObtainMetaDataReference(Player->MetaDataParsedVideoParametersType, (void **)(&ParsedVideoParameters));
    SE_ASSERT(ParsedVideoParameters != NULL);

    //
    // If this does not contain any frame data, then we simply wish
    // to slot it into place for the intermediate process.
    //

    if (!ParsedFrameParameters->NewFrameParameters ||
        (ParsedFrameParameters->DecodeFrameIndex == INVALID_INDEX))
    {
        OS_LockMutex(&H264Lock);

        for (i = 0; i < H264_CODED_FRAME_COUNT; i++)
            if (!FramesInPreprocessorChain[i].Used)
            {
                memset(&FramesInPreprocessorChain[i], 0, sizeof(FramesInPreprocessorChain_t));
                FramesInPreprocessorChain[i].Used                               = true;
                FramesInPreprocessorChain[i].Action                             = ActionPassOnFrame;
                FramesInPreprocessorChain[i].CodedBuffer                        = CodedBuffer;
                FramesInPreprocessorChain[i].ParsedFrameParameters              = ParsedFrameParameters;
                FramesInPreprocessorChain[i].DecodeFrameIndex                   = ParsedFrameParameters->DecodeFrameIndex;
                CodedBuffer->IncrementReferenceCount(IdentifierH264Intermediate);
                FramesInPreprocessorChainRing->Insert(i);
                OS_UnLockMutex(&H264Lock);
                return CodecNoError;
            }

        SE_ERROR("Failed to find an entry in the \"FramesInPreprocessorChain\" list\n");
        OS_UnLockMutex(&H264Lock);
        return CodecError;
    }

    //
    // We believe we have a frame - check that this is marked as a first slice
    //
    FrameParameters     = (H264FrameParameters_t *)ParsedFrameParameters->FrameParameterStructure;

    if (!ParsedVideoParameters->FirstSlice)
    {
        SE_ERROR("Non first slice, when one is expected\n");
        return CodecError;
    }

    //
    // Get a pre-processor buffer and attach it to the coded frame.
    // Note since it is attached to the coded frame, we can release our
    // personal hold on it.
    //
    Status = PreProcessorBufferPool->GetBuffer(&PreProcessorBuffer);
    if (Status != BufferNoError)
    {
        SE_ERROR("Failed to get a pre-processor buffer (%08x)\n", Status);
        return CodecError;
    }

    CodedBuffer->AttachBuffer(PreProcessorBuffer);
    PreProcessorBuffer->DecrementReferenceCount();
    //
    // Now get an entry into the frames list, and fill in the relevant fields
    //
    OS_LockMutex(&H264Lock);

    for (i = 0; i < H264_CODED_FRAME_COUNT; i++)
        if (!FramesInPreprocessorChain[i].Used)
        {
            break;
        }

    if (i == H264_CODED_FRAME_COUNT)
    {
        SE_ERROR("Failed to find an entry in the \"FramesInPreprocessorChain\" list - Implementation error\n");
        CodedBuffer->DetachBuffer(PreProcessorBuffer);
        OS_UnLockMutex(&H264Lock);
        return PlayerImplementationError;
    }

    memset(&FramesInPreprocessorChain[i], 0, sizeof(FramesInPreprocessorChain_t));
    FramesInPreprocessorChain[i].Used                                   = true;
    FramesInPreprocessorChain[i].Action                                 = ActionPassOnPreProcessedFrame;
    FramesInPreprocessorChain[i].CodedBuffer                            = CodedBuffer;
    FramesInPreprocessorChain[i].PreProcessorBuffer                     = PreProcessorBuffer;
    FramesInPreprocessorChain[i].ParsedFrameParameters                  = ParsedFrameParameters;
    FramesInPreprocessorChain[i].DecodeFrameIndex                       = ParsedFrameParameters->DecodeFrameIndex;
    CodedBuffer->ObtainDataReference(NULL, &CodedDataLength, (void **)(&FramesInPreprocessorChain[i].InputBufferCachedAddress), CachedAddress);
    SE_ASSERT(FramesInPreprocessorChain[i].InputBufferCachedAddress != NULL); // not expected to be empty ? TBC
    CodedBuffer->ObtainDataReference(NULL, NULL, (void **)(&FramesInPreprocessorChain[i].InputBufferPhysicalAddress), PhysicalAddress);
    SE_ASSERT(FramesInPreprocessorChain[i].InputBufferPhysicalAddress != NULL); // not expected to be empty ? TBC

    /* it could happen in corrupted stream */
    if (CodedDataLength < ParsedFrameParameters->DataOffset + FrameParameters->SlicesLength)
    {
        SE_ERROR("error DataOffset=%u + SlicesLength %u > CodedDataLength=%u\n",
                 ParsedFrameParameters->DataOffset, FrameParameters->SlicesLength, CodedDataLength);
        FramesInPreprocessorChain[i].Used               = false;
        CodedBuffer->DetachBuffer(PreProcessorBuffer);
        OS_UnLockMutex(&H264Lock);
        return CodecError;
    }

    FramesInPreprocessorChain[i].InputBufferCachedAddress    = (void *)((unsigned int)FramesInPreprocessorChain[i].InputBufferCachedAddress + ParsedFrameParameters->DataOffset);
    FramesInPreprocessorChain[i].InputBufferPhysicalAddress  = (void *)((unsigned int)FramesInPreprocessorChain[i].InputBufferPhysicalAddress + ParsedFrameParameters->DataOffset);
//ik    CodedDataLength                 -= ParsedFrameParameters->DataOffset;
    PreProcessorBuffer->ObtainDataReference(NULL, NULL, (void **)(&FramesInPreprocessorChain[i].OutputBufferCachedAddress), CachedAddress);
    SE_ASSERT(FramesInPreprocessorChain[i].OutputBufferCachedAddress != NULL); // not expected to be empty ? TBC
    PreProcessorBuffer->ObtainDataReference(NULL, NULL, (void **)(&FramesInPreprocessorChain[i].OutputBufferPhysicalAddress), PhysicalAddress);
    SE_ASSERT(FramesInPreprocessorChain[i].OutputBufferPhysicalAddress != NULL); // not expected to be empty ? TBC
    CodedBuffer->IncrementReferenceCount(IdentifierH264Intermediate);

    //
    // Process the buffer
    //
    PPStatus = H264ppProcessBuffer(PreProcessorDevice,
                                   &FrameParameters->SliceHeader,
                                   i,
                                   mPreProcessorBufferMaxSize,
                                   FrameParameters->SlicesLength,
                                   FramesInPreprocessorChain[i].InputBufferCachedAddress,
                                   FramesInPreprocessorChain[i].InputBufferPhysicalAddress,
                                   FramesInPreprocessorChain[i].OutputBufferCachedAddress,
                                   FramesInPreprocessorChain[i].OutputBufferPhysicalAddress);
    if (PPStatus != h264pp_ok)
    {
        SE_ERROR("Failed to process a buffer, Junking frame\n");
        FramesInPreprocessorChain[i].Used               = false;
        CodedBuffer->DetachBuffer(PreProcessorBuffer);
        CodedBuffer->DecrementReferenceCount(IdentifierH264Intermediate);
        OS_UnLockMutex(&H264Lock);
        return CodecError;
    }

    FramesInPreprocessorChainRing->Insert(i);
    OS_UnLockMutex(&H264Lock);

    return CodecNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
//      Function to deal with the returned capabilities
//      structure for an H264 mme transformer.
//

CodecStatus_t   Codec_MmeVideoH264_c::HandleCapabilities(void)
{
// Default to using Omega2 unless Capabilities tell us otherwise
    BufferFormat_t  DisplayFormat = FormatVideo420_MacroBlock;
// Default elements to produce in the buffers
    DecodeBufferComponentElementMask_t Elements = PrimaryManifestationElement |
                                                  DecimatedManifestationElement |
                                                  VideoMacroblockStructureElement;
    RasterOutput = false;
    SE_INFO(group_decoder_video, "MME Transformer '%s' capabilities are :-\n", H264_MME_TRANSFORMER_NAME);
    SE_DEBUG(group_decoder_video, "  SD_MaxMBStructureSize             = %d bytes\n", H264TransformCapability.SD_MaxMBStructureSize);
    SE_DEBUG(group_decoder_video, "  HD_MaxMBStructureSize             = %d bytes\n", H264TransformCapability.HD_MaxMBStructureSize);
    SE_DEBUG(group_decoder_video, "  MaximumFieldDecodingLatency90KHz  = %d\n", H264TransformCapability.MaximumFieldDecodingLatency90KHz);
    //
    // Unless things have changed, the firmware always
    // reported zero for the sizes, so we use our own
    // known values.
    //
    // Accordingly to max partition size (cf. havana_stream.cpp)
    SD_MaxMBStructureSize       = 110;
    HD_MaxMBStructureSize       = 48;
#if H264_MME_VERSION >= 57
#ifdef DELTATOP_MME_VERSION
    DeltaTop_TransformerCapability_t *DeltaTopCapabilities = (DeltaTop_TransformerCapability_t *) Configuration.DeltaTopCapabilityStructurePointer;

    if ((DeltaTopCapabilities != NULL) && (DeltaTopCapabilities->DisplayBufferFormat == DELTA_OUTPUT_RASTER))
    {
        RasterOutput = true;
    }

#endif
#endif

    if (RasterOutput)
    {
        DisplayFormat = FormatVideo420_Raster2B;
        Elements |= VideoDecodeCopyElement;
        SD_MaxMBStructureSize   = H264_DUALHD_STORED_MBSTRUCT_SIZE_IN_PPB_BELOW_LEVEL31;
        HD_MaxMBStructureSize   = H264_DUALHD_STORED_MBSTRUCT_SIZE_IN_PPB_LEVEL31_OR_ABOVE;
    }

    SE_INFO(group_decoder_video, "  Using default MB structure sizes   = %d, %d bytes\n", SD_MaxMBStructureSize, HD_MaxMBStructureSize);
    Stream->GetDecodeBufferManager()->FillOutDefaultList(DisplayFormat, Elements,
                                                         Configuration.ListOfDecodeBufferComponents);
    return CodecNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
//      Function to extend the buffer request to incorporate the macroblock sizing
//

CodecStatus_t   Codec_MmeVideoH264_c::FillOutDecodeBufferRequest(DecodeBufferRequest_t    *Request)
{
    H264SequenceParameterSetHeader_t        *SPS;
    CodecStatus_t ret = CodecNoError;

    // Fill out the standard fields first
    ret = Codec_MmeVideo_c::FillOutDecodeBufferRequest(Request);

    // and now the extended field
    if (ParsedFrameParameters && ParsedFrameParameters->FrameParameterStructure)
    {
        SPS = ((H264FrameParameters_t *)ParsedFrameParameters->FrameParameterStructure)->SliceHeader.SequenceParameterSet;
        Request->PerMacroblockMacroblockStructureSize = (SPS->level_idc <= 30) ? SD_MaxMBStructureSize : HD_MaxMBStructureSize;
    }
    else
    {
        SE_DEBUG(group_decoder_video, "Unable to retrieve sps header\n");
        Request->PerMacroblockMacroblockStructureSize  = HD_MaxMBStructureSize;
        ret = CodecError;
    }

    Request->MacroblockSize                             = 256;
    Request->PerMacroblockMacroblockStructureFifoSize   = 512; // Fifo length is 256 bytes for 7108 but 512 are requested for MPE42c1, let's use widest value it's easier
    return ret;
}


// /////////////////////////////////////////////////////////////////////////
//
//      Function to deal with the returned capabilities
//      structure for an H264 mme transformer.
//

CodecStatus_t   Codec_MmeVideoH264_c::FillOutTransformerInitializationParameters(void)
{
    //
    // Fill out the command parameters
    //
    H264InitializationParameters.CircularBinBufferBeginAddr_p   = (U32 *)0x00000000;
    H264InitializationParameters.CircularBinBufferEndAddr_p     = (U32 *)0xffffffff;
    //
    // Fill out the actual command
    //
    MMEInitializationParameters.TransformerInitParamsSize       = sizeof(H264_InitTransformerParam_fmw_t);
    MMEInitializationParameters.TransformerInitParams_p         = (MME_GenericParams_t)(&H264InitializationParameters);

    return CodecNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
//      Function to prepare the stream parameters
//      structure for an H264 mme transformer,
//      from the current PPS and SPS
//

CodecStatus_t Codec_MmeVideoH264_c::PrepareStreamParametersCommand(void                    *GenericContext,
                                                                   H264SequenceParameterSetHeader_t   *SPS,
                                                                   H264PictureParameterSetHeader_t    *PPS)
{
    H264CodecStreamParameterContext_t *Context = (H264CodecStreamParameterContext_t *)GenericContext;

    memset(&Context->StreamParameters, 0, sizeof(H264_SetGlobalParam_t));
    Context->StreamParameters.H264SetGlobalParamSPS.DecoderProfileConformance          = H264_HIGH_PROFILE;
    // Context->StreamParameters.H264SetGlobalParamSPS.DecoderProfileConformance          = (H264_DecoderProfileComformance_t)SPS->profile_idc;
    Context->StreamParameters.H264SetGlobalParamSPS.level_idc                          = SPS->level_idc;
    Context->StreamParameters.H264SetGlobalParamSPS.log2_max_frame_num_minus4          = SPS->log2_max_frame_num_minus4;
    Context->StreamParameters.H264SetGlobalParamSPS.pic_order_cnt_type                 = SPS->pic_order_cnt_type;
    Context->StreamParameters.H264SetGlobalParamSPS.log2_max_pic_order_cnt_lsb_minus4  = SPS->log2_max_pic_order_cnt_lsb_minus4;
    Context->StreamParameters.H264SetGlobalParamSPS.delta_pic_order_always_zero_flag   = SPS->delta_pic_order_always_zero_flag;
    Context->StreamParameters.H264SetGlobalParamSPS.pic_width_in_mbs_minus1            = SPS->pic_width_in_mbs_minus1;
    Context->StreamParameters.H264SetGlobalParamSPS.pic_height_in_map_units_minus1     = SPS->pic_height_in_map_units_minus1;
    Context->StreamParameters.H264SetGlobalParamSPS.frame_mbs_only_flag                = SPS->frame_mbs_only_flag;
    Context->StreamParameters.H264SetGlobalParamSPS.mb_adaptive_frame_field_flag       = SPS->mb_adaptive_frame_field_flag;
#if defined(CONFIG_STM_VIRTUAL_PLATFORM) && (H264_MME_VERSION >= 62) // VSOC only trick - used by SystemC model of Delta Decoders for DPB management. Not relevant for SVC firmware
    Context->StreamParameters.H264SetGlobalParamSPS.max_num_ref_frames                 = SPS->num_ref_frames;
#endif
    // In case of LEVEL >= 3.0, we force direct_8x8_inference_flag to support streams with an invalid number of MV's : (Bug 12245)
    Context->StreamParameters.H264SetGlobalParamSPS.direct_8x8_inference_flag          = SPS->level_idc >= 30 ? true : SPS->direct_8x8_inference_flag;
    Context->StreamParameters.H264SetGlobalParamSPS.chroma_format_idc                  = SPS->chroma_format_idc;
    Context->StreamParameters.H264SetGlobalParamPPS.entropy_coding_mode_flag           = PPS->entropy_coding_mode_flag;
    Context->StreamParameters.H264SetGlobalParamPPS.pic_order_present_flag             = PPS->pic_order_present_flag;
    Context->StreamParameters.H264SetGlobalParamPPS.num_ref_idx_l0_active_minus1       = PPS->num_ref_idx_l0_active_minus1;
    Context->StreamParameters.H264SetGlobalParamPPS.num_ref_idx_l1_active_minus1       = PPS->num_ref_idx_l1_active_minus1;
    Context->StreamParameters.H264SetGlobalParamPPS.weighted_pred_flag                 = PPS->weighted_pred_flag;
    Context->StreamParameters.H264SetGlobalParamPPS.weighted_bipred_idc                = PPS->weighted_bipred_idc;
    Context->StreamParameters.H264SetGlobalParamPPS.pic_init_qp_minus26                = PPS->pic_init_qp_minus26;
    Context->StreamParameters.H264SetGlobalParamPPS.chroma_qp_index_offset             = PPS->chroma_qp_index_offset;
    Context->StreamParameters.H264SetGlobalParamPPS.deblocking_filter_control_present_flag = PPS->deblocking_filter_control_present_flag;
    Context->StreamParameters.H264SetGlobalParamPPS.constrained_intra_pred_flag        = PPS->constrained_intra_pred_flag;
    Context->StreamParameters.H264SetGlobalParamPPS.transform_8x8_mode_flag            = PPS->transform_8x8_mode_flag;
    Context->StreamParameters.H264SetGlobalParamPPS.second_chroma_qp_index_offset      = PPS->second_chroma_qp_index_offset;
    Context->StreamParameters.H264SetGlobalParamPPS.ScalingList.ScalingListUpdated     = 1;

    if (PPS->pic_scaling_matrix_present_flag)
    {
        memcpy(Context->StreamParameters.H264SetGlobalParamPPS.ScalingList.FirstSixScalingList, PPS->ScalingList4x4, sizeof(PPS->ScalingList4x4));
        memcpy(Context->StreamParameters.H264SetGlobalParamPPS.ScalingList.NextTwoScalingList, PPS->ScalingList8x8, sizeof(PPS->ScalingList8x8));
    }
    else
    {
        memcpy(Context->StreamParameters.H264SetGlobalParamPPS.ScalingList.FirstSixScalingList, SPS->ScalingList4x4, sizeof(PPS->ScalingList4x4));
        memcpy(Context->StreamParameters.H264SetGlobalParamPPS.ScalingList.NextTwoScalingList, SPS->ScalingList8x8, sizeof(PPS->ScalingList8x8));
    }

    // Needed for SVCDecoder firmware
    Context->StreamParameters.SvcSpecificSetGlobalParamSPS.slice_header_restriction_flag        = 1;
    Context->StreamParameters.SvcSpecificSetGlobalParamSPS.chroma_phase_x_plus1_flag            = 1;
    Context->StreamParameters.SvcSpecificSetGlobalParamSPS.chroma_phase_y_plus1             = 1;
    Context->StreamParameters.SvcSpecificSetGlobalParamSPS.seq_ref_layer_chroma_phase_x_plus1_flag  = 1;
    Context->StreamParameters.SvcSpecificSetGlobalParamSPS.seq_ref_layer_chroma_phase_y_plus1       = 1;
    //
    // Fill out the actual command
    //
    memset(&Context->BaseContext.MMECommand, 0, sizeof(MME_Command_t));
    Context->BaseContext.MMECommand.CmdStatus.AdditionalInfoSize        = 0;
    Context->BaseContext.MMECommand.CmdStatus.AdditionalInfo_p          = NULL;
    Context->BaseContext.MMECommand.ParamSize                           = sizeof(H264_SetGlobalParam_t);
    Context->BaseContext.MMECommand.Param_p                             = (MME_GenericParams_t)(&Context->StreamParameters);

    return CodecNoError;
}

// /////////////////////////////////////////////////////////////////////////
//
//      Function to fill out the stream parameters
//      structure for an H264 mme transformer.
//

CodecStatus_t   Codec_MmeVideoH264_c::FillOutSetStreamParametersCommand(void)
{
    H264StreamParameters_t                  *Parsed         = (H264StreamParameters_t *)ParsedFrameParameters->StreamParameterStructure;
    H264CodecStreamParameterContext_t       *Context        = (H264CodecStreamParameterContext_t *)StreamParameterContext;
    H264PictureParameterSetHeader_t         *PPS;
    H264SequenceParameterSetHeader_t        *SPS;
    //
    // Fill out the command parameters
    //
    SPS         = Parsed->SequenceParameterSet;
    PPS         = Parsed->PictureParameterSet;
    return PrepareStreamParametersCommand((void *)Context, SPS, PPS);
}


// /////////////////////////////////////////////////////////////////////////
//
//      Function to fill out the decode parameters
//      structure for an H264 mme transformer.
//

CodecStatus_t   Codec_MmeVideoH264_c::FillOutDecodeCommand(void)
{
    H264FrameParameters_t  *Parsed = (H264FrameParameters_t *)ParsedFrameParameters->FrameParameterStructure;
    //
    // Detach the pre-processor buffer and re-attach it to the decode context
    // this will make its release automatic on the freeing of the decode context.
    //

    Buffer_t PreProcessorBuffer;
    CodedFrameBuffer->ObtainAttachedBufferReference(mPreProcessorBufferType, &PreProcessorBuffer);
    SE_ASSERT(PreProcessorBuffer != NULL);

    DecodeContextBuffer->AttachBuffer(PreProcessorBuffer);      // Must be ordered, otherwise the pre-processor
    CodedFrameBuffer->DetachBuffer(PreProcessorBuffer);         // buffer will get released in the middle
    return PrepareDecodeCommand(&Parsed->SliceHeader, PreProcessorBuffer, ParsedFrameParameters->ReferenceFrame);
}


CodecStatus_t   Codec_MmeVideoH264_c::PrepareDecodeCommand(H264SliceHeader_t   *SliceHeader,
                                                           Buffer_t        PreProcessorBuffer,
                                                           bool            ReferenceFrame)
{
    CodecStatus_t                     Status;
    H264CodecDecodeContext_t         *Context                       = (H264CodecDecodeContext_t *)DecodeContext;
    H264_TransformParam_fmw_t        *Param;
    H264SequenceParameterSetHeader_t *SPS;
    unsigned int                      InputBuffer;
    unsigned int                      PreProcessorBufferSize;
    bool                  DowngradeDecode;
    unsigned int              MaxAllocatableBuffers;
    Buffer_t              DecodeBuffer;
    DecodeBufferComponentType_t   DisplayComponent;
    DecodeBufferComponentType_t   DecimatedComponent;
    DecodeBufferComponentType_t   DecodeComponent;
    //
    // For H264 we do not do slice decodes.
    //
    KnownLastSliceInFieldFrame                  = true;
    OS_LockMutex(&Lock);
    DecodeBuffer            = BufferState[CurrentDecodeBufferIndex].Buffer;
    DisplayComponent            = PrimaryManifestationComponent;
    DecimatedComponent          = DecimatedManifestationComponent;
    DecodeComponent         = RasterOutput ? VideoDecodeCopy : PrimaryManifestationComponent;

    //
    // If this is a reference frame, and no macroblock structure has been attached
    // to this decode buffer, obtain a reference frame slot, and obtain a macroblock
    // structure buffer and attach it.
    //

    if (ReferenceFrame && (BufferState[CurrentDecodeBufferIndex].ReferenceFrameSlot == INVALID_INDEX))
    {
        //
        // Obtain a reference frame slot
        //
        Status = ReferenceFrameSlotAllocate(CurrentDecodeBufferIndex);
        if (Status != CodecNoError)
        {
            SE_ERROR("Failed to obtain a reference frame slot\n");
            OS_UnLockMutex(&Lock);
            return Status;
        }

        //
        // Obtain a macroblock structure buffer
        //    First calculate the size,
        //    Second verify that there are enough decode buffers for the reference frame count
        //    Third verify that the macroblock structure memory is large enough
        //    Fourth allocate the actual buffer.
        //
        SPS = SliceHeader->SequenceParameterSet;

        Stream->GetDecodeBufferManager()->GetEstimatedBufferCount(&MaxAllocatableBuffers, ForManifestation);
        // 3 buffers may be in display pipe
        if (SPS->num_ref_frames > (MaxAllocatableBuffers - 3))
        {
            PlayerEventRecord_t                UnPlayableEvent;
            SE_ERROR("Insufficient memory allocated to cope with manifested frames (num_ref_frames:%d > (%d-3) max)\n",
                     SPS->num_ref_frames, MaxAllocatableBuffers);
            // raise the event to signal this to the user
            UnPlayableEvent.Code      = EventStreamUnPlayable;
            UnPlayableEvent.Playback  = Stream->GetPlayback();
            UnPlayableEvent.Stream    = Stream;
            UnPlayableEvent.PlaybackTime  = TIME_NOT_APPLICABLE;
            UnPlayableEvent.UserData  = NULL;
            UnPlayableEvent.Reason    = STM_SE_PLAY_STREAM_MSG_REASON_CODE_INSUFFICIENT_MEMORY;

            Stream->SignalEvent(&UnPlayableEvent);
            Stream->SetUnPlayable();
            OS_UnLockMutex(&Lock);
            return CodecError;
        }

        Stream->GetDecodeBufferManager()->GetEstimatedBufferCount(&MaxAllocatableBuffers, ForReference);
        if (SPS->num_ref_frames > MaxAllocatableBuffers)
        {
            PlayerEventRecord_t                UnPlayableEvent;
            SE_ERROR("Insufficient memory allocated to cope with reference frames (num_ref_frames:%d > %d max)\n",
                     SPS->num_ref_frames, MaxAllocatableBuffers);

            UnPlayableEvent.Code      = EventStreamUnPlayable;
            UnPlayableEvent.Playback  = Stream->GetPlayback();
            UnPlayableEvent.Stream    = Stream;
            UnPlayableEvent.PlaybackTime  = TIME_NOT_APPLICABLE;
            UnPlayableEvent.UserData  = NULL;
            UnPlayableEvent.Reason    = STM_SE_PLAY_STREAM_MSG_REASON_CODE_INSUFFICIENT_MEMORY;

            Stream->SignalEvent(&UnPlayableEvent);
            Stream->SetUnPlayable();
            OS_UnLockMutex(&Lock);
            return CodecError;
        }
    }

    //
    // Fill out the sub-components of the command data
    //
    memset(&Context->DecodeParameters, 0, sizeof(H264_TransformParam_fmw_t));
    memset(&Context->DecodeStatus, 0xa5, sizeof(H264_CommandStatus_fmw_t));
    FillOutDecodeCommandHostData(SliceHeader);
    FillOutDecodeCommandRefPicList(SliceHeader);
    //
    // Fill out the remaining fields in the transform parameters structure.
    //
    Param                                       = &Context->DecodeParameters;
    PreProcessorBuffer->ObtainDataReference(NULL, &PreProcessorBufferSize, (void **)&InputBuffer, PhysicalAddress);
    SE_ASSERT(InputBuffer != NULL); // not expected to be empty ? TBC
    Param->PictureStartAddrBinBuffer            = (unsigned int *)InputBuffer;                            // Start address in the bin buffer
    Param->PictureStopAddrBinBuffer             = (unsigned int *)(InputBuffer + PreProcessorBufferSize); // Stop address in the bin buffer
    Param->MaxSESBSize                          = H264_PP_SESB_SIZE;

    //
    // Here we only support decimation by 2 in the vertical,
    // if the decimated buffer is present, then we decimate 2 vertical

    if (!Stream->GetDecodeBufferManager()->ComponentPresent(DecodeBuffer, DecimatedComponent))
    {
        // Normal Case
#if (H264_MME_VERSION >= 57)
        if (RasterOutput && ParsedFrameParameters->ReferenceFrame)
        {
            Param->MainAuxEnable = H264_REF_MAIN_DISP_MAIN_EN;
        }
        else
        {
            Param->MainAuxEnable = H264_DISP_MAIN_EN;
        }
#else
        Param->MainAuxEnable                        = H264_DISP_MAIN_EN;
#endif
        Param->HorizontalDecimationFactor           = HDEC_1;
        Param->VerticalDecimationFactor             = VDEC_1;
    }
    else
    {
#if (H264_MME_VERSION >= 57)
        if (RasterOutput && ParsedFrameParameters->ReferenceFrame)
        {
            Param->MainAuxEnable = H264_REF_MAIN_DISP_MAIN_AUX_EN;
        }
        else
        {
            Param->MainAuxEnable = H264_DISP_AUX_MAIN_EN;
        }
#else
        Param->MainAuxEnable                        = H264_DISP_AUX_MAIN_EN;
#endif
        Param->HorizontalDecimationFactor           = (Stream->GetDecodeBufferManager()->DecimationFactor(DecodeBuffer, 0) == 2) ?
                                                      HDEC_ADVANCED_2 :
                                                      HDEC_ADVANCED_4;
        Param->VerticalDecimationFactor             = VDEC_ADVANCED_2_INT;
    }

    //
    DowngradeDecode             = ParsedFrameParameters->ApplySubstandardDecode &&
                                  SLICE_TYPE_IS(((H264FrameParameters_t *)ParsedFrameParameters->FrameParameterStructure)->SliceHeader.slice_type, H264_SLICE_TYPE_B);
    Param->DecodingMode                         = DowngradeDecode ? H264_DOWNGRADED_DECODE_LEVEL1 :
                                                  (Player->PolicyValue(Playback, Stream, PolicyErrorDecodingLevel) == PolicyValuePolicyErrorDecodingLevelMaximum ?
                                                   H264_ERC_MAX_DECODE : H264_NORMAL_DECODE);
    Param->AdditionalFlags                      = H264_ADDITIONAL_FLAG_NONE;                            // No future use yet identified
    //
    // Omega2 Reference Buffers are stored in Decoded Buffer
    //
    Param->DecodedBufferAddress.Luma_p          = (H264_LumaAddress_t)Stream->GetDecodeBufferManager()->Luma(DecodeBuffer, DecodeComponent);
    Param->DecodedBufferAddress.Chroma_p        = (H264_ChromaAddress_t)Stream->GetDecodeBufferManager()->Chroma(DecodeBuffer, DecodeComponent);
    Param->DecodedBufferAddress.MBStruct_p      = (H264_MBStructureAddress_t)Stream->GetDecodeBufferManager()->ComponentBaseAddress(DecodeBuffer, VideoMacroblockStructure);
#if H264_MME_VERSION >= 57
    Param->DisplayBufferAddress.DisplayLuma_p   = (H264_LumaAddress_t)Stream->GetDecodeBufferManager()->Luma(DecodeBuffer, DisplayComponent);
    Param->DisplayBufferAddress.DisplayChroma_p = (H264_ChromaAddress_t)Stream->GetDecodeBufferManager()->Chroma(DecodeBuffer, DisplayComponent);

    if (Stream->GetDecodeBufferManager()->ComponentPresent(DecodeBuffer, DecimatedComponent))
    {
        Param->DisplayBufferAddress.DisplayDecimatedLuma_p   = (H264_LumaAddress_t)Stream->GetDecodeBufferManager()->Luma(DecodeBuffer, DecimatedComponent);
        Param->DisplayBufferAddress.DisplayDecimatedChroma_p = (H264_ChromaAddress_t)Stream->GetDecodeBufferManager()->Chroma(DecodeBuffer, DecimatedComponent);
    }

#else

    if (Player->PolicyValue(Playback, Stream, PolicyDecimateDecoderOutput) != PolicyValueDecimateDecoderOutputDisabled)
    {
        Param->DecodedBufferAddress.LumaDecimated_p   = (H264_LumaAddress_t)Stream->GetDecodeBufferManager()->Luma(DecodeBuffer, DecimatedComponent);
        Param->DecodedBufferAddress.ChromaDecimated_p = (H264_ChromaAddress_t)Stream->GetDecodeBufferManager()->Chroma(DecodeBuffer, DecimatedComponent);
    }

#endif
    OS_UnLockMutex(&Lock);
    // Bug 22786 : base_view_flag must be set for AVC Streams.
    // For MVC streams, this flag must be set to 1 for the base view and 0 for others.
    Param->base_view_flag = 1;
    // Needed for SVCDecoder firmware
    Param->OutputLayerParams.no_inter_layer_pred_flag = 1;
    Param->CurrentLayerParams.no_inter_layer_pred_flag = 1;
    Param->CurrentLayerParams.TargetLayerFlag = 1;

    //
    // Fill out the actual command
    //
    memset(&Context->BaseContext.MMECommand, 0, sizeof(MME_Command_t));
    Context->BaseContext.MMECommand.CmdStatus.AdditionalInfoSize        = sizeof(H264_CommandStatus_fmw_t);
    Context->BaseContext.MMECommand.CmdStatus.AdditionalInfo_p          = (MME_GenericParams_t)(&Context->DecodeStatus);
    Context->BaseContext.MMECommand.ParamSize                           = sizeof(H264_TransformParam_fmw_t);
    Context->BaseContext.MMECommand.Param_p                             = (MME_GenericParams_t)(&Context->DecodeParameters);

    return CodecNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
//      Function to fill out the host data segment of the h264 decode
//      parameters structure for the H264 mme transformer.
//
//      This function must be mutex-locked by caller
//
CodecStatus_t   Codec_MmeVideoH264_c::FillOutDecodeCommandHostData(H264SliceHeader_t *SliceHeader)
{
    H264CodecDecodeContext_t                 *Context               = (H264CodecDecodeContext_t *)DecodeContext;
    H264SequenceParameterSetHeader_t         *SPS;
    H264_HostData_t                          *HostData;

    OS_AssertMutexHeld(&Lock);
    SPS         = SliceHeader->SequenceParameterSet;
    HostData    = &Context->DecodeParameters.HostData;

    HostData->PictureNumber             = 0;
    HostData->PicOrderCntTop            = SliceHeader->PicOrderCntTop;
    HostData->PicOrderCntBot            = SliceHeader->PicOrderCntBot;

    if (SPS->frame_mbs_only_flag || !SliceHeader->field_pic_flag)
    {
        HostData->PicOrderCnt           = min(SliceHeader->PicOrderCntTop, SliceHeader->PicOrderCntBot);
        HostData->DescriptorType        = SPS->mb_adaptive_frame_field_flag ? H264_PIC_DESCRIPTOR_AFRAME : H264_PIC_DESCRIPTOR_FRAME;
    }
    else
    {
        HostData->PicOrderCnt           = SliceHeader->bottom_field_flag ? SliceHeader->PicOrderCntBot : SliceHeader->PicOrderCntTop;
        HostData->DescriptorType        = SliceHeader->bottom_field_flag ? H264_PIC_DESCRIPTOR_FIELD_BOTTOM : H264_PIC_DESCRIPTOR_FIELD_TOP;
    }

    HostData->ReferenceType             = (SliceHeader->nal_ref_idc != 0) ?                             // Its a reference frame
                                          (SliceHeader->dec_ref_pic_marking.long_term_reference_flag ?
                                           H264_LONG_TERM_REFERENCE :
                                           H264_SHORT_TERM_REFERENCE) :
                                          H264_UNUSED_FOR_REFERENCE;
    HostData->IdrFlag                   = SliceHeader->nal_unit_type == NALU_TYPE_IDR;
    HostData->NonExistingFlag           = 0;

    BufferState[CurrentDecodeBufferIndex].DecodedAsFrames       = (SliceHeader->field_pic_flag == 0);
    BufferState[CurrentDecodeBufferIndex].DecodedWithMbaff      = (SPS->mb_adaptive_frame_field_flag != 0);

    //
    // Save the host data only if this is a reference frame
    //

    if (ParsedFrameParameters->ReferenceFrame)
    {
        memcpy(&RecordedHostData[CurrentDecodeBufferIndex], HostData, sizeof(H264_HostData_t));
    }

    return CodecNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
//      Function to fill out the reference picture list segment of the
//      h264 decode parameters structure for the H264 mme transformer.
//
//      This function must be mutex-locked by caller
//
CodecStatus_t   Codec_MmeVideoH264_c::FillOutDecodeCommandRefPicList(H264SliceHeader_t *SliceHeader)
{
    unsigned int                              i;
    H264CodecDecodeContext_t                 *Context               = (H264CodecDecodeContext_t *)DecodeContext;
    H264_RefPictListAddress_t                *RefPicLists;
    unsigned int                              ReferenceId;
    unsigned int                  Slot;
    unsigned int                              DescriptorIndex;
    Buffer_t                  ReferenceBuffer;
    DecodeBufferComponentType_t       DecodeComponent;
    OS_AssertMutexHeld(&Lock);
    DecodeComponent         = RasterOutput ? VideoDecodeCopy : PrimaryManifestationComponent;

    // H264SequenceParameterSetHeader_t *SPS = SliceHeader->SequenceParameterSet;
    RefPicLists = &Context->DecodeParameters.InitialRefPictList;

    //
    // Setup the frame address array
    //

    for (i = 0; i < DecodeBufferCount; i++)
        if ((BufferState[i].ReferenceFrameCount != 0) && (BufferState[i].ReferenceFrameSlot != INVALID_INDEX))
        {
            Slot        = BufferState[i].ReferenceFrameSlot;
            ReferenceBuffer = BufferState[i].Buffer;
            RefPicLists->ReferenceFrameAddress[Slot].DecodedBufferAddress.Luma_p     = (H264_LumaAddress_t)Stream->GetDecodeBufferManager()->Luma(ReferenceBuffer, DecodeComponent);;
            RefPicLists->ReferenceFrameAddress[Slot].DecodedBufferAddress.Chroma_p   = (H264_ChromaAddress_t)Stream->GetDecodeBufferManager()->Chroma(ReferenceBuffer, DecodeComponent);;
            RefPicLists->ReferenceFrameAddress[Slot].DecodedBufferAddress.MBStruct_p = (H264_MBStructureAddress_t)Stream->GetDecodeBufferManager()->ComponentBaseAddress(ReferenceBuffer,
                                                                                       VideoMacroblockStructure);
        }

    //
    // Create the reference picture lists, NOTE this is a fudge, the
    // delta-phi requires reference picture lists for B and P pictures
    // to be supplied for all pictures (thats I B or P), so we have to
    // make up reference picture lists for a frame that this isn't.
    //
    NumberOfUsedDescriptors     = 0;
    memset(DescriptorIndices, 0xff, sizeof(DescriptorIndices));
    RefPicLists->ReferenceDescriptorsBitsField  = 0;                            // Default to all being frame descriptors
    //
    // For each entry in the lists we check if the descriptor is already available
    // and point to it. If it is not available we create a new descriptor.
    //
    RefPicLists->InitialPList0.RefPiclistSize   = DecodeContext->ReferenceFrameList[P_REF_PIC_LIST].EntryCount;

    for (i = 0; i < RefPicLists->InitialPList0.RefPiclistSize; i++)
    {
        unsigned int    UsageCode                       = DecodeContext->ReferenceFrameList[P_REF_PIC_LIST].ReferenceDetails[i].UsageCode;
        unsigned int    BufferIndex                     = DecodeContext->ReferenceFrameList[P_REF_PIC_LIST].EntryIndicies[i];
        Slot                        = BufferState[BufferIndex].ReferenceFrameSlot;
        ReferenceId                                     = (3 * Slot) + UsageCode;
        DescriptorIndex                                 = DescriptorIndices[ReferenceId];

        if (DescriptorIndex == 0xff)
        {
            DescriptorIndex                             = FillOutNewDescriptor(ReferenceId, BufferIndex, &DecodeContext->ReferenceFrameList[P_REF_PIC_LIST].ReferenceDetails[i]);
        }

        RefPicLists->InitialPList0.RefPicList[i]        = DescriptorIndex;
    }

    RefPicLists->InitialBList0.RefPiclistSize   = DecodeContext->ReferenceFrameList[B_REF_PIC_LIST_0].EntryCount;

    for (i = 0; i < RefPicLists->InitialBList0.RefPiclistSize; i++)
    {
        unsigned int    UsageCode                       = DecodeContext->ReferenceFrameList[B_REF_PIC_LIST_0].ReferenceDetails[i].UsageCode;
        unsigned int    BufferIndex                     = DecodeContext->ReferenceFrameList[B_REF_PIC_LIST_0].EntryIndicies[i];
        Slot                        = BufferState[BufferIndex].ReferenceFrameSlot;
        ReferenceId                                     = (3 * Slot) + UsageCode;
        DescriptorIndex                                 = DescriptorIndices[ReferenceId];

        if (DescriptorIndex == 0xff)
        {
            DescriptorIndex                             = FillOutNewDescriptor(ReferenceId, BufferIndex, &DecodeContext->ReferenceFrameList[B_REF_PIC_LIST_0].ReferenceDetails[i]);
        }

        RefPicLists->InitialBList0.RefPicList[i]        = DescriptorIndex;
    }

    RefPicLists->InitialBList1.RefPiclistSize   = DecodeContext->ReferenceFrameList[B_REF_PIC_LIST_1].EntryCount;

    for (i = 0; i < RefPicLists->InitialBList1.RefPiclistSize; i++)
    {
        unsigned int    UsageCode                       = DecodeContext->ReferenceFrameList[B_REF_PIC_LIST_1].ReferenceDetails[i].UsageCode;
        unsigned int    BufferIndex                     = DecodeContext->ReferenceFrameList[B_REF_PIC_LIST_1].EntryIndicies[i];
        Slot                        = BufferState[BufferIndex].ReferenceFrameSlot;
        ReferenceId                                     = (3 * Slot) + UsageCode;
        DescriptorIndex                                 = DescriptorIndices[ReferenceId];

        if (DescriptorIndex == 0xff)
        {
            DescriptorIndex                             = FillOutNewDescriptor(ReferenceId, BufferIndex, &DecodeContext->ReferenceFrameList[B_REF_PIC_LIST_1].ReferenceDetails[i]);
        }

        RefPicLists->InitialBList1.RefPicList[i]        = DescriptorIndex;
    }

    return CodecNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
//      Function to fill out a single descriptor for a h264 reference frame
//
//      This function must be mutex-locked by caller
//
unsigned int   Codec_MmeVideoH264_c::FillOutNewDescriptor(
    unsigned int             ReferenceId,
    unsigned int             BufferIndex,
    ReferenceDetails_t  *Details)
{
    H264CodecDecodeContext_t                 *Context               = (H264CodecDecodeContext_t *)DecodeContext;
    H264_RefPictListAddress_t                *RefPicLists;
    unsigned int                              DescriptorIndex;
    unsigned int                  SlotIndex;
    H264_PictureDescriptor_t                 *Descriptor;

    OS_AssertMutexHeld(&Lock);
    RefPicLists = &Context->DecodeParameters.InitialRefPictList;
    DescriptorIndex                     = NumberOfUsedDescriptors++;
    SlotIndex               = BufferState[BufferIndex].ReferenceFrameSlot;
    DescriptorIndices[ReferenceId]      = DescriptorIndex;
    Descriptor                          = &RefPicLists->PictureDescriptor[DescriptorIndex];
    Descriptor->FrameIndex              = SlotIndex;
    //
    // Setup the host data
    //
    memcpy(&Descriptor->HostData, &RecordedHostData[BufferIndex], sizeof(H264_HostData_t));
    Descriptor->HostData.PictureNumber  = Details->PictureNumber;
    Descriptor->HostData.PicOrderCnt    = Details->PicOrderCnt;
    Descriptor->HostData.PicOrderCntTop = Details->PicOrderCntTop;
    Descriptor->HostData.PicOrderCntBot = Details->PicOrderCntBot;
    Descriptor->HostData.ReferenceType  = Details->LongTermReference ? H264_LONG_TERM_REFERENCE : H264_SHORT_TERM_REFERENCE;

    //
    // The nature of the frame being referenced is in its original descriptor type
    //

    if ((Descriptor->HostData.DescriptorType != H264_PIC_DESCRIPTOR_FRAME) &&
        (Descriptor->HostData.DescriptorType != H264_PIC_DESCRIPTOR_AFRAME))
    {
        RefPicLists->ReferenceDescriptorsBitsField      |= 1 << SlotIndex;
    }

    //
    // We now modify the descriptor type to reflect the way in which we reference the frame
    //

    if (Details->UsageCode == REF_PIC_USE_FRAME)
    {
        if ((Descriptor->HostData.DescriptorType != H264_PIC_DESCRIPTOR_AFRAME) &&
            (Descriptor->HostData.DescriptorType != H264_PIC_DESCRIPTOR_FRAME))
        {
            // REMINDER: remember to mutex lock/unlock BufferState[] access if you are going to use that following line
            // Descriptor->HostData.DescriptorType = BufferState[BufferIndex].DecodedWithMbaff ? H264_PIC_DESCRIPTOR_AFRAME : H264_PIC_DESCRIPTOR_FRAME;
            Descriptor->HostData.DescriptorType = H264_PIC_DESCRIPTOR_FRAME;
        }
    }
    else if (Details->UsageCode == REF_PIC_USE_FIELD_TOP)
    {
        Descriptor->HostData.DescriptorType     = H264_PIC_DESCRIPTOR_FIELD_TOP;
    }
    else if (Details->UsageCode == REF_PIC_USE_FIELD_BOT)
    {
        Descriptor->HostData.DescriptorType     = H264_PIC_DESCRIPTOR_FIELD_BOTTOM;
    }

    //
    // And return the index
    //
    return DescriptorIndex;
}

////////////////////////////////////////////////////////////////////////////
///
/// Unconditionally return success.
///
/// Success and failure codes are located entirely in the generic MME structures
/// allowing the super-class to determine whether the decode was successful. This
/// means that we have no work to do here.
///
/// \return CodecNoError
///
CodecStatus_t   Codec_MmeVideoH264_c::ValidateDecodeContext(CodecBaseDecodeContext_t *Context)
{
    H264CodecDecodeContext_t  *H264Context = (H264CodecDecodeContext_t *)Context;
#ifdef MULTICOM
    SE_EXTRAVERB(group_decoder_video, "Decode took %6dus\n", H264Context->DecodeStatus.DecodeTimeInMicros);

    if (H264Context->DecodeStatus.ErrorCode != H264_DECODER_NO_ERROR)
    {
        SE_INFO(group_decoder_video, "H264 decode status:%08x\n", H264Context->DecodeStatus.ErrorCode);
#if 0
        DumpMMECommand(&Context->MMECommand);
#endif
    }

#endif
    return CodecNoError;
}

// /////////////////////////////////////////////////////////////////////////
//
//      Function to dump out the set stream
//      parameters from an mme command.
//

CodecStatus_t   Codec_MmeVideoH264_c::DumpSetStreamParameters(void    *Parameters)
{
    unsigned int             i;
    H264_SetGlobalParam_t   *StreamParams;
    StreamParams    = (H264_SetGlobalParam_t *)Parameters;
    SE_VERBOSE(group_decoder_video, "AZA - STREAM PARAMS %p\n", StreamParams);
    SE_VERBOSE(group_decoder_video, "AZA -    SPS\n");
    SE_VERBOSE(group_decoder_video, "AZA -       DecoderProfileConformance            = %d\n", StreamParams->H264SetGlobalParamSPS.DecoderProfileConformance);
    SE_VERBOSE(group_decoder_video, "AZA -       level_idc                            = %d\n", StreamParams->H264SetGlobalParamSPS.level_idc);
    SE_VERBOSE(group_decoder_video, "AZA -       log2_max_frame_num_minus4            = %d\n", StreamParams->H264SetGlobalParamSPS.log2_max_frame_num_minus4);
    SE_VERBOSE(group_decoder_video, "AZA -       pic_order_cnt_type                   = %d\n", StreamParams->H264SetGlobalParamSPS.pic_order_cnt_type);
    SE_VERBOSE(group_decoder_video, "AZA -       log2_max_pic_order_cnt_lsb_minus4    = %d\n", StreamParams->H264SetGlobalParamSPS.log2_max_pic_order_cnt_lsb_minus4);
    SE_VERBOSE(group_decoder_video, "AZA -       delta_pic_order_always_zero_flag     = %d\n", StreamParams->H264SetGlobalParamSPS.delta_pic_order_always_zero_flag);
    SE_VERBOSE(group_decoder_video, "AZA -       pic_width_in_mbs_minus1              = %d\n", StreamParams->H264SetGlobalParamSPS.pic_width_in_mbs_minus1);
    SE_VERBOSE(group_decoder_video, "AZA -       pic_height_in_map_units_minus1       = %d\n", StreamParams->H264SetGlobalParamSPS.pic_height_in_map_units_minus1);
    SE_VERBOSE(group_decoder_video, "AZA -       frame_mbs_only_flag                  = %d\n", StreamParams->H264SetGlobalParamSPS.frame_mbs_only_flag);
    SE_VERBOSE(group_decoder_video, "AZA -       mb_adaptive_frame_field_flag         = %d\n", StreamParams->H264SetGlobalParamSPS.mb_adaptive_frame_field_flag);
    SE_VERBOSE(group_decoder_video, "AZA -       direct_8x8_inference_flag            = %d\n", StreamParams->H264SetGlobalParamSPS.direct_8x8_inference_flag);
    SE_VERBOSE(group_decoder_video, "AZA -       chroma_format_idc                    = %d\n", StreamParams->H264SetGlobalParamSPS.chroma_format_idc);
    SE_VERBOSE(group_decoder_video, "AZA -    PPS\n");
    SE_VERBOSE(group_decoder_video, "AZA -       entropy_coding_mode_flag             = %d\n", StreamParams->H264SetGlobalParamPPS.entropy_coding_mode_flag);
    SE_VERBOSE(group_decoder_video, "AZA -       pic_order_present_flag               = %d\n", StreamParams->H264SetGlobalParamPPS.pic_order_present_flag);
    SE_VERBOSE(group_decoder_video, "AZA -       num_ref_idx_l0_active_minus1         = %d\n", StreamParams->H264SetGlobalParamPPS.num_ref_idx_l0_active_minus1);
    SE_VERBOSE(group_decoder_video, "AZA -       num_ref_idx_l1_active_minus1         = %d\n", StreamParams->H264SetGlobalParamPPS.num_ref_idx_l1_active_minus1);
    SE_VERBOSE(group_decoder_video, "AZA -       weighted_pred_flag                   = %d\n", StreamParams->H264SetGlobalParamPPS.weighted_pred_flag);
    SE_VERBOSE(group_decoder_video, "AZA -       weighted_bipred_idc                  = %d\n", StreamParams->H264SetGlobalParamPPS.weighted_bipred_idc);
    SE_VERBOSE(group_decoder_video, "AZA -       pic_init_qp_minus26                  = %d\n", StreamParams->H264SetGlobalParamPPS.pic_init_qp_minus26);
    SE_VERBOSE(group_decoder_video, "AZA -       chroma_qp_index_offset               = %d\n", StreamParams->H264SetGlobalParamPPS.chroma_qp_index_offset);
    SE_VERBOSE(group_decoder_video, "AZA -       deblocking_filter_control_present_flag = %d\n", StreamParams->H264SetGlobalParamPPS.deblocking_filter_control_present_flag);
    SE_VERBOSE(group_decoder_video, "AZA -       constrained_intra_pred_flag          = %d\n", StreamParams->H264SetGlobalParamPPS.constrained_intra_pred_flag);
    SE_VERBOSE(group_decoder_video, "AZA -       transform_8x8_mode_flag              = %d\n", StreamParams->H264SetGlobalParamPPS.transform_8x8_mode_flag);
    SE_VERBOSE(group_decoder_video, "AZA -       second_chroma_qp_index_offset        = %d\n", StreamParams->H264SetGlobalParamPPS.second_chroma_qp_index_offset);
    SE_VERBOSE(group_decoder_video, "AZA -       ScalingListUpdated                   = %d\n", StreamParams->H264SetGlobalParamPPS.ScalingList.ScalingListUpdated);

    for (i = 0; i < 6; i++)
        SE_VERBOSE(group_decoder_video, "AZA -       ScalingList4x4[%d]                   = %02x %02x %02x %02x %02x %02x %02x %02x\n", i,
                   StreamParams->H264SetGlobalParamPPS.ScalingList.FirstSixScalingList[i][0], StreamParams->H264SetGlobalParamPPS.ScalingList.FirstSixScalingList[i][1],
                   StreamParams->H264SetGlobalParamPPS.ScalingList.FirstSixScalingList[i][2], StreamParams->H264SetGlobalParamPPS.ScalingList.FirstSixScalingList[i][3],
                   StreamParams->H264SetGlobalParamPPS.ScalingList.FirstSixScalingList[i][4], StreamParams->H264SetGlobalParamPPS.ScalingList.FirstSixScalingList[i][5],
                   StreamParams->H264SetGlobalParamPPS.ScalingList.FirstSixScalingList[i][6], StreamParams->H264SetGlobalParamPPS.ScalingList.FirstSixScalingList[i][7]);

    for (i = 0; i < 2; i++)
        SE_VERBOSE(group_decoder_video, "AZA -       ScalingList8x8[%d]                   = %02x %02x %02x %02x %02x %02x %02x %02x\n", i,
                   StreamParams->H264SetGlobalParamPPS.ScalingList.NextTwoScalingList[i][0], StreamParams->H264SetGlobalParamPPS.ScalingList.NextTwoScalingList[i][1],
                   StreamParams->H264SetGlobalParamPPS.ScalingList.NextTwoScalingList[i][2], StreamParams->H264SetGlobalParamPPS.ScalingList.NextTwoScalingList[i][3],
                   StreamParams->H264SetGlobalParamPPS.ScalingList.NextTwoScalingList[i][4], StreamParams->H264SetGlobalParamPPS.ScalingList.NextTwoScalingList[i][5],
                   StreamParams->H264SetGlobalParamPPS.ScalingList.NextTwoScalingList[i][6], StreamParams->H264SetGlobalParamPPS.ScalingList.NextTwoScalingList[i][7]);

    return CodecNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
//      Function to dump out the decode
//      parameters from an mme command.
//

CodecStatus_t   Codec_MmeVideoH264_c::DumpDecodeParameters(void    *Parameters)
{
    unsigned int                      i;
    unsigned int                      MaxDescriptor;
    H264_TransformParam_fmw_t        *FrameParams;
    H264_HostData_t                  *HostData;
    H264_RefPictListAddress_t        *RefPicLists;

    FrameParams = (H264_TransformParam_fmw_t *)Parameters;
    HostData    = &FrameParams->HostData;
    RefPicLists = &FrameParams->InitialRefPictList;
    SE_VERBOSE(group_decoder_video, "AZA - FRAME PARAMS %p\n", FrameParams);
    SE_VERBOSE(group_decoder_video, "AZA -       PictureStartAddrBinBuffer            = %p\n", FrameParams->PictureStartAddrBinBuffer);
    SE_VERBOSE(group_decoder_video, "AZA -       PictureStopAddrBinBuffer             = %p\n", FrameParams->PictureStopAddrBinBuffer);
    SE_VERBOSE(group_decoder_video, "AZA -       MaxSESBSize                          = %d bytes\n", FrameParams->MaxSESBSize);
    SE_VERBOSE(group_decoder_video, "AZA -       MainAuxEnable                        = %08x\n", FrameParams->MainAuxEnable);
    SE_VERBOSE(group_decoder_video, "AZA -       HorizontalDecimationFactor           = %08x\n", FrameParams->HorizontalDecimationFactor);
    SE_VERBOSE(group_decoder_video, "AZA -       VerticalDecimationFactor             = %08x\n", FrameParams->VerticalDecimationFactor);
    SE_VERBOSE(group_decoder_video, "AZA -       DecodingMode                         = %d\n", FrameParams->DecodingMode);
    SE_VERBOSE(group_decoder_video, "AZA -       AdditionalFlags                      = %08x\n", FrameParams->AdditionalFlags);
    SE_VERBOSE(group_decoder_video, "AZA -       Luma_p                               = %p\n", FrameParams->DecodedBufferAddress.Luma_p);
    SE_VERBOSE(group_decoder_video, "AZA -       Chroma_p                             = %p\n", FrameParams->DecodedBufferAddress.Chroma_p);
    SE_VERBOSE(group_decoder_video, "AZA -       MBStruct_p                           = %p\n", FrameParams->DecodedBufferAddress.MBStruct_p);
    SE_VERBOSE(group_decoder_video, "AZA -       HostData\n");
    SE_VERBOSE(group_decoder_video, "AZA -           PictureNumber                    = %d\n", HostData->PictureNumber);
    SE_VERBOSE(group_decoder_video, "AZA -           PicOrderCntTop                   = %d\n", HostData->PicOrderCntTop);
    SE_VERBOSE(group_decoder_video, "AZA -           PicOrderCntBot                   = %d\n", HostData->PicOrderCntBot);
    SE_VERBOSE(group_decoder_video, "AZA -           PicOrderCnt                      = %d\n", HostData->PicOrderCnt);
    SE_VERBOSE(group_decoder_video, "AZA -           DescriptorType                   = %d\n", HostData->DescriptorType);
    SE_VERBOSE(group_decoder_video, "AZA -           ReferenceType                    = %d\n", HostData->ReferenceType);
    SE_VERBOSE(group_decoder_video, "AZA -           IdrFlag                          = %d\n", HostData->IdrFlag);
    SE_VERBOSE(group_decoder_video, "AZA -           NonExistingFlag                  = %d\n", HostData->NonExistingFlag);
    SE_VERBOSE(group_decoder_video, "AZA -       InitialRefPictList\n");
    SE_VERBOSE(group_decoder_video, "AZA -           ReferenceFrameAddresses\n");
    SE_VERBOSE(group_decoder_video, "AZA -                 Luma    Chroma   MBStruct\n");

    for (i = 0; i < DecodeBufferCount; i++)
        SE_VERBOSE(group_decoder_video, "AZA -               %p %p %p\n",
                   RefPicLists->ReferenceFrameAddress[i].DecodedBufferAddress.Luma_p,
                   RefPicLists->ReferenceFrameAddress[i].DecodedBufferAddress.Chroma_p,
                   RefPicLists->ReferenceFrameAddress[i].DecodedBufferAddress.MBStruct_p);

    MaxDescriptor       = 0;
    SE_VERBOSE(group_decoder_video, "AZA -           InitialPList0 Descriptor Indices\n");
    SE_VERBOSE(group_decoder_video, "AZA -               ");

    for (i = 0; i < RefPicLists->InitialPList0.RefPiclistSize; i++)
    {
        SE_VERBOSE(group_decoder_video, "%02d  ", RefPicLists->InitialPList0.RefPicList[i]);
        MaxDescriptor   = max(MaxDescriptor, (RefPicLists->InitialPList0.RefPicList[i] + 1));
    }

    SE_VERBOSE(group_decoder_video, "\n");
    SE_VERBOSE(group_decoder_video, "AZA -           InitialBList0 Descriptor Indices\n");
    SE_VERBOSE(group_decoder_video, "AZA -               ");

    for (i = 0; i < RefPicLists->InitialBList0.RefPiclistSize; i++)
    {
        SE_VERBOSE(group_decoder_video, "%02d  ", RefPicLists->InitialBList0.RefPicList[i]);
        MaxDescriptor   = max(MaxDescriptor, (RefPicLists->InitialBList0.RefPicList[i] + 1));
    }

    SE_VERBOSE(group_decoder_video, "\n");
    SE_VERBOSE(group_decoder_video, "AZA -           InitialBList1 Descriptor Indices\n");
    SE_VERBOSE(group_decoder_video, "AZA -               ");

    for (i = 0; i < RefPicLists->InitialBList1.RefPiclistSize; i++)
    {
        SE_VERBOSE(group_decoder_video, "%02d  ", RefPicLists->InitialBList1.RefPicList[i]);
        MaxDescriptor   = max(MaxDescriptor, (RefPicLists->InitialBList1.RefPicList[i] + 1));
    }

    SE_VERBOSE(group_decoder_video, "\n");
    SE_VERBOSE(group_decoder_video, "AZA -           Picture descriptors (ReferenceDescriptorsBitsField = %08x)\n", RefPicLists->ReferenceDescriptorsBitsField);

    for (i = 0; i < MaxDescriptor; i++)
        SE_VERBOSE(group_decoder_video, "AZA -               Desc %d - Buff %2d, PN = %2d, Cnt T %2d B %2d X %2d, DT = %d, RT = %d, IDR = %d, NEF = %d\n",
                   i, RefPicLists->PictureDescriptor[i].FrameIndex,
                   RefPicLists->PictureDescriptor[i].HostData.PictureNumber,
                   RefPicLists->PictureDescriptor[i].HostData.PicOrderCntTop,
                   RefPicLists->PictureDescriptor[i].HostData.PicOrderCntBot,
                   RefPicLists->PictureDescriptor[i].HostData.PicOrderCnt,
                   RefPicLists->PictureDescriptor[i].HostData.DescriptorType,
                   RefPicLists->PictureDescriptor[i].HostData.ReferenceType,
                   RefPicLists->PictureDescriptor[i].HostData.IdrFlag,
                   RefPicLists->PictureDescriptor[i].HostData.NonExistingFlag);

    return CodecNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
//      A local release reference frame, the external one inserts into our
//  processing list, and the parent class one is generic, this version
//  of the function releases the reference list slot allocation, then
//  passes control to the generic function.
//

CodecStatus_t   Codec_MmeVideoH264_c::H264ReleaseReferenceFrame(unsigned int     ReferenceFrameDecodeIndex)
{
    CodecStatus_t   Status;
    unsigned int    Index;
    OS_LockMutex(&Lock);

    //
    // Release the appropriate entries in the reference list slot array
    //

    if (ReferenceFrameDecodeIndex == CODEC_RELEASE_ALL)
    {
        memset(ReferenceFrameSlotUsed, 0, H264_MAX_REFERENCE_FRAMES * sizeof(bool));
        OutstandingSlotAllocationRequest    = INVALID_INDEX;
    }
    else
    {
        Status = TranslateDecodeIndex(ReferenceFrameDecodeIndex, &Index);
        if ((Status == CodecNoError) && (BufferState[Index].ReferenceFrameCount == 1) &&
            (BufferState[Index].ReferenceFrameSlot != INVALID_INDEX))
        {
            ReferenceFrameSlotUsed[BufferState[Index].ReferenceFrameSlot] = false;
        }
    }

    //
    // Can we satisfy an outstanding reference frame slot request
    //
    ReferenceFrameSlotAllocate(INVALID_INDEX);
    //
    // And pass on down the generic chain
    //
    OS_UnLockMutex(&Lock);
    return Codec_MmeVideo_c::ReleaseReferenceFrame(ReferenceFrameDecodeIndex);
}


// /////////////////////////////////////////////////////////////////////////
//
//      A function to handle allocation of reference frame slots.
//  Split out since it can be called on a decode where the
//  allocation may be deferred, and on release where a deferred
//  allocation may be enacted. Only one allocation may be deferred
//  at any one time.
//
//      This function must be mutex-locked by caller
//
CodecStatus_t   Codec_MmeVideoH264_c::ReferenceFrameSlotAllocate(unsigned int        BufferIndex)
{
    unsigned int    i;
    OS_AssertMutexHeld(&Lock);

    //
    // Is there any outstanding request (that is still outstanding).
    //

    if (OutstandingSlotAllocationRequest != INVALID_INDEX)
    {
        if (BufferState[OutstandingSlotAllocationRequest].ReferenceFrameCount != 0)
        {
            for (i = 0; (i < H264_MAX_REFERENCE_FRAMES) && ReferenceFrameSlotUsed[i]; i++);

            // Did we find one, if not just fall through
            if (i < H264_MAX_REFERENCE_FRAMES)
            {
                ReferenceFrameSlotUsed[i]                       = true;
                BufferState[OutstandingSlotAllocationRequest].ReferenceFrameSlot    = i;
                OutstandingSlotAllocationRequest                    = INVALID_INDEX;
            }
        }
        else
        {
            OutstandingSlotAllocationRequest    = INVALID_INDEX;
        }
    }

    //
    // Do we have a new request
    //

    if (BufferIndex != INVALID_INDEX)
    {
        //
        // If there is an outstanding request we have a problem
        //
        if (OutstandingSlotAllocationRequest != INVALID_INDEX)
        {
            SE_ERROR("Outstanding request when new request is received\n");
            return CodecError;
        }

        //
        // Attempt to allocate, should we fail then make the request outstanding
        //

        for (i = 0; (i < H264_MAX_REFERENCE_FRAMES) && ReferenceFrameSlotUsed[i]; i++);

        if (i < H264_MAX_REFERENCE_FRAMES)
        {
            ReferenceFrameSlotUsed[i]           = true;
            BufferState[BufferIndex].ReferenceFrameSlot = i;
        }
        else
        {
            BufferState[BufferIndex].ReferenceFrameSlot = INVALID_INDEX;
            OutstandingSlotAllocationRequest        = BufferIndex;
        }
    }

    return CodecNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
//      The intermediate process, taking output from the pre-processor
//      and feeding it to the lower level of the codec.
//

OS_TaskEntry(Codec_MmeVideoH264_IntermediateProcess)
{
    Codec_MmeVideoH264_c    *Codec = (Codec_MmeVideoH264_c *)Parameter;
    Codec->IntermediateProcess();
    OS_TerminateThread();
    return NULL;
}

// ----------------------

void Codec_MmeVideoH264_c::UnuseCodedBuffer(Buffer_t CodedBuffer, FramesInPreprocessorChain_t *PPentry)
{
    CodedBuffer->SetUsedDataSize(0);

    BufferStatus_t Status = CodedBuffer->ShrinkBuffer(0);
    if (Status != BufferNoError)
    {
        SE_INFO(group_decoder_video, "Failed to shrink buffer\n");
    }
}

void Codec_MmeVideoH264_c::IntermediateProcess(void)
{
    PlayerStatus_t  Status;
    h264pp_status_t PPStatus;
    RingStatus_t    RingStatus;
    uintptr_t   Entry;
    unsigned int    PPEntry = 0;
    unsigned int    PPSize = 0;
    unsigned int    PPStatusMask = 0;
    bool        PromoteNextStreamParametersToNew;
    bool        MovingToDiscardUntilNewFrame;
    bool        DiscardUntilNewFrame;
    Buffer_t    PreProcessorBuffer;
    SE_INFO(group_decoder_video, "Starting Intermediate Process Thread\n");

    //
    // Signal we have started
    //
    OS_SetEvent(&StartStopEvent);

    //
    // Main loop
    //
    PromoteNextStreamParametersToNew    = false;
    MovingToDiscardUntilNewFrame    = false;
    DiscardUntilNewFrame        = false;

    while (!Terminating)
    {
        //
        // Anything coming through the ring
        //
        RingStatus      = FramesInPreprocessorChainRing->Extract(&Entry, MAX_EVENT_WAIT);

        if (RingStatus == RingNothingToGet)
        {
            continue;
        }

        //
        // Make sure we have exclusive access to FramesInPreprocessorChain[]
        //
        OS_LockMutex(&H264Lock);

        //
        // If SE is in low power state, process should keep inactive, as we are not allowed to issue any MME commands
        //
        if (Stream && Stream->IsLowPowerState())
        {
            FramesInPreprocessorChain[Entry].Action     = ActionNull;
        }

        //
        // Is frame aborted
        //

        if (FramesInPreprocessorChain[Entry].AbortedFrame || Terminating)
        {
            FramesInPreprocessorChain[Entry].Action     = ActionNull;
        }

        //
        // Process activity - note aborted activity differs in consequences for each action.
        //

        switch (FramesInPreprocessorChain[Entry].Action)
        {
        case ActionNull:
            break;

        case ActionCallOutputPartialDecodeBuffers:
            if (Stream)
            {
                Codec_MmeVideo_c::OutputPartialDecodeBuffers();
            }
            break;

        case ActionCallDiscardQueuedDecodes:
            DiscardFramesInPreprocessorChain        = false;
            Codec_MmeVideo_c::DiscardQueuedDecodes();
            break;

        case ActionCallReleaseReferenceFrame:
            H264ReleaseReferenceFrame(FramesInPreprocessorChain[Entry].DecodeFrameIndex);
            break;

        case ActionPassOnPreProcessedFrame:
            PPStatus = H264ppGetPreProcessedBuffer(PreProcessorDevice, &PPEntry, &PPSize, &PPStatusMask);
            if (PPStatus != h264pp_ok)
            {
                SE_ERROR("Failed to retrieve a buffer from the pre-processor\n");
                break;
            }
            else
            {
                if (Stream)
                {
                    if (PPStatusMask & PP_ITM__ERROR_SC_DETECTED)
                    {
                        Stream->Statistics().H264PreprocErrorSCDetected++;
                    }

                    if (PPStatusMask & PP_ITM__ERROR_BIT_INSERTED)
                    {
                        Stream->Statistics().H264PreprocErrorBitInserted++;
                    }

                    if (PPStatusMask & PP_ITM__INT_BUFFER_OVERFLOW)
                    {
                        Stream->Statistics().H264PreprocIntBufferOverflow++;
                    }

                    if (PPStatusMask & PP_ITM__BIT_BUFFER_UNDERFLOW)
                    {
                        Stream->Statistics().H264PreprocBitBufferUnderflow++;
                    }

                    if (PPStatusMask & PP_ITM__BIT_BUFFER_OVERFLOW)
                    {
                        Stream->Statistics().H264PreprocBitBufferOverflow++;
                    }

                    if (PPStatusMask & PP_ITM__READ_ERROR)
                    {
                        Stream->Statistics().H264PreprocReadErrors++;
                    }

                    if (PPStatusMask & PP_ITM__WRITE_ERROR)
                    {
                        Stream->Statistics().H264PreprocWriteErrors++;
                    }
                }
            }

            // Coded buffer is not useful anymore, all the data is in the intermediate buffer
            UnuseCodedBuffer(FramesInPreprocessorChain[Entry].CodedBuffer, &FramesInPreprocessorChain[Entry]);

            if (PPStatusMask != 0)
            {
                int AllowBadPPFrames  = Player->PolicyValue(Playback, Stream, PolicyAllowBadPreProcessedFrames);

                if (AllowBadPPFrames != PolicyValueApply)
                {
                    SE_ERROR("Pre-processing failed\n");
                    MovingToDiscardUntilNewFrame    = true;
                }
            }

            if ((PPEntry != Entry) && !MovingToDiscardUntilNewFrame)
            {
                SE_ERROR("Retrieved wrong identifier from pre-processor (%u %lu)\n", PPEntry, Entry);
                MovingToDiscardUntilNewFrame    = true;
            }

            if (FramesInPreprocessorChain[Entry].ParsedFrameParameters->FirstParsedParametersForOutputFrame)
            {
                DiscardUntilNewFrame  = false;
            }

            if (DiscardFramesInPreprocessorChain || MovingToDiscardUntilNewFrame || DiscardUntilNewFrame)
            {
                if (FramesInPreprocessorChain[Entry].ParsedFrameParameters->NewStreamParameters)
                {
                    PromoteNextStreamParametersToNew  = true;
                }

                if (Stream)
                {
                    if (FramesInPreprocessorChain[Entry].ParsedFrameParameters->FirstParsedParametersForOutputFrame)
                    {
                        Stream->ParseToDecodeEdge->RecordNonDecodedFrame(FramesInPreprocessorChain[Entry].CodedBuffer, FramesInPreprocessorChain[Entry].ParsedFrameParameters);
                    }
                    else
                    {
                        Codec_MmeVideo_c::OutputPartialDecodeBuffers();
                    }
                }

                FramesInPreprocessorChain[Entry].CodedBuffer->DetachBuffer(FramesInPreprocessorChain[Entry].PreProcessorBuffer);         // This should release the pre-processor buffer

                if (MovingToDiscardUntilNewFrame)
                {
                    DiscardUntilNewFrame        = true;
                    MovingToDiscardUntilNewFrame    = false;
                }

                break;
            }

            FramesInPreprocessorChain[Entry].PreProcessorBuffer->SetUsedDataSize(PPSize);

        case ActionPassOnFrame:
            if (Terminating)
            {
                break;
            }

            //
            // Now mimic the input procedure as done in the player process
            //

            if (PromoteNextStreamParametersToNew && (FramesInPreprocessorChain[Entry].ParsedFrameParameters->StreamParameterStructure != NULL))
            {
                FramesInPreprocessorChain[Entry].ParsedFrameParameters->NewStreamParameters = true;
                PromoteNextStreamParametersToNew                        = false;
            }

            Status  = Codec_MmeVideo_c::Input(FramesInPreprocessorChain[Entry].CodedBuffer);
            if (Status != CodecNoError)
            {
                if (FramesInPreprocessorChain[Entry].ParsedFrameParameters->NewStreamParameters)
                {
                    PromoteNextStreamParametersToNew  = true;
                }

                if (Stream && FramesInPreprocessorChain[Entry].ParsedFrameParameters->FirstParsedParametersForOutputFrame)
                {
                    Stream->ParseToDecodeEdge->RecordNonDecodedFrame(FramesInPreprocessorChain[Entry].CodedBuffer, FramesInPreprocessorChain[Entry].ParsedFrameParameters);
                    Codec_MmeVideo_c::OutputPartialDecodeBuffers();
                }

                // If the pre processor buffer is still attached top the coded buffer, make sure it gets free
                FramesInPreprocessorChain[Entry].CodedBuffer->ObtainAttachedBufferReference(mPreProcessorBufferType, &PreProcessorBuffer);
                if (PreProcessorBuffer != NULL)
                {
                    FramesInPreprocessorChain[Entry].CodedBuffer->DetachBuffer(PreProcessorBuffer);
                }

                DiscardUntilNewFrame = true;
            }

            break;
        }

        //
        // Release this entry
        //

        if (FramesInPreprocessorChain[Entry].CodedBuffer != NULL)
        {
            FramesInPreprocessorChain[Entry].CodedBuffer->DecrementReferenceCount(IdentifierH264Intermediate);
        }

        FramesInPreprocessorChain[Entry].Used   = false;
        OS_UnLockMutex(&H264Lock);
    }

    //
    // Clean up the ring
    //
    OS_LockMutex(&H264Lock);

    while (FramesInPreprocessorChainRing->NonEmpty())
    {
        FramesInPreprocessorChainRing->Extract(&Entry);

        if (FramesInPreprocessorChain[Entry].CodedBuffer != NULL)
        {
            FramesInPreprocessorChain[Entry].CodedBuffer->DecrementReferenceCount(IdentifierH264Intermediate);
        }

        FramesInPreprocessorChain[Entry].Used   = false;
    }

    OS_UnLockMutex(&H264Lock);

    SE_INFO(group_decoder_video, "Terminating Intermediate Process Thread\n");

    //
    // Signal we have terminated
    //
    OS_Smp_Mb(); // Read memory barrier: rmb_for_CodecH264_Terminating coupled with: wmb_for_CodecH264_Terminating
    OS_SetEvent(&StartStopEvent);
}

//{{{  CheckCodecReturnParameters
// Convert the return code into human readable form.
static const char *LookupError(unsigned int Error)
{
#define E(e) case e: return #e

    switch (Error)
    {
        E(H264_DECODER_ERROR_MB_OVERFLOW);
        E(H264_DECODER_ERROR_RECOVERED);
        E(H264_DECODER_ERROR_NOT_RECOVERED);
        E(H264_DECODER_ERROR_TASK_TIMEOUT);

    default: return "H264_DECODER_UNKNOWN_ERROR";
    }

#undef E
}

CodecStatus_t   Codec_MmeVideoH264_c::CheckCodecReturnParameters(CodecBaseDecodeContext_t *Context)
{
    unsigned int                 i, j;
    unsigned int                 ErroneousMacroBlocks;
    unsigned int                 TotalMacroBlocks;
    unsigned long long           DecodeDeltaLxTime;
    Buffer_t                     DecodeBuffer;
    DecodeBufferComponentType_t  DecodeComponent;
    H264CodecDecodeContext_t    *H264Context      = (H264CodecDecodeContext_t *)Context;
    MME_Command_t               *MMECommand       = (MME_Command_t *)(&Context->MMECommand);
    MME_CommandStatus_t         *CmdStatus        = (MME_CommandStatus_t *)(&MMECommand->CmdStatus);
    H264_CommandStatus_fmw_t    *AdditionalInfo_p = (H264_CommandStatus_fmw_t *)CmdStatus->AdditionalInfo_p;

    if (AdditionalInfo_p != NULL)
    {
        DecodeDeltaLxTime = AdditionalInfo_p->DecodeTimeInMicros;

        if ((H264Context->DecodeParameters.HostData.DescriptorType != H264_PIC_DESCRIPTOR_FRAME) &&
            (H264Context->DecodeParameters.HostData.DescriptorType != H264_PIC_DESCRIPTOR_AFRAME))
        {
            DecodeDeltaLxTime *= 2;
        }

        Stream->Statistics().MaxVideoHwDecodeTime = max(DecodeDeltaLxTime , Stream->Statistics().MaxVideoHwDecodeTime);
        Stream->Statistics().MinVideoHwDecodeTime = min(DecodeDeltaLxTime, Stream->Statistics().MinVideoHwDecodeTime);

        if (!Stream->Statistics().MinVideoHwDecodeTime)
        {
            Stream->Statistics().MinVideoHwDecodeTime = DecodeDeltaLxTime;
        }

        if (Stream->Statistics().FrameCountDecoded)
        {
            Stream->Statistics().AvgVideoHwDecodeTime = (DecodeDeltaLxTime +
                                                         ((Stream->Statistics().FrameCountDecoded - 1) * Stream->Statistics().AvgVideoHwDecodeTime))
                                                        / (Stream->Statistics().FrameCountDecoded);
        }
        else
        {
            Stream->Statistics().AvgVideoHwDecodeTime = 0;
        }

        if (AdditionalInfo_p->ErrorCode != H264_DECODER_NO_ERROR)
        {
            SE_INFO(group_decoder_video, "%s  %x\n", LookupError(AdditionalInfo_p->ErrorCode), AdditionalInfo_p->ErrorCode);
            //
            // Calculate decode quality from error status fields
            //
            OS_LockMutex(&Lock);
            DecodeBuffer        = BufferState[Context->BufferIndex].Buffer;
            DecodeComponent     = RasterOutput ? VideoDecodeCopy : PrimaryManifestationComponent;
            TotalMacroBlocks        = (Stream->GetDecodeBufferManager()->ComponentDimension(DecodeBuffer, DecodeComponent, 0) *
                                       Stream->GetDecodeBufferManager()->ComponentDimension(DecodeBuffer, DecodeComponent, 1)) / 256;
            OS_UnLockMutex(&Lock);

            if ((H264Context->DecodeParameters.HostData.DescriptorType != H264_PIC_DESCRIPTOR_FRAME) &&
                (H264Context->DecodeParameters.HostData.DescriptorType != H264_PIC_DESCRIPTOR_AFRAME))
            {
                TotalMacroBlocks  /= 2;
            }

            ErroneousMacroBlocks    = 0;

            for (i = 0; i < H264_STATUS_PARTITION; i++)
                for (j = 0; j < H264_STATUS_PARTITION; j++)
                {
                    ErroneousMacroBlocks  += H264Context->DecodeStatus.Status[i][j];
                }

            Context->DecodeQuality  = (ErroneousMacroBlocks < TotalMacroBlocks) ?
                                      Rational_t((TotalMacroBlocks - ErroneousMacroBlocks), TotalMacroBlocks) : 0;
#if 0
            SE_VERBOSE(group_decoder_video, "AZAAZA - %4d %4d - %d.%09d\n", TotalMacroBlocks, ErroneousMacroBlocks,
                       Context->DecodeQuality.IntegerPart(),  Context->DecodeQuality.RemainderDecimal(9));

            for (unsigned int i = 0; i < H264_STATUS_PARTITION; i++)
                SE_VERBOSE(group_decoder_video, "  %02x %02x %02x %02x %02x %02x\n",
                           H264Context->DecodeStatus.Status[0][i], H264Context->DecodeStatus.Status[1][i],
                           H264Context->DecodeStatus.Status[2][i], H264Context->DecodeStatus.Status[3][i],
                           H264Context->DecodeStatus.Status[4][i], H264Context->DecodeStatus.Status[5][i]);

#endif

//

            switch (AdditionalInfo_p->ErrorCode)
            {
            case H264_DECODER_ERROR_MB_OVERFLOW:
                Stream->Statistics().FrameDecodeMBOverflowError++;
                break;

            case H264_DECODER_ERROR_RECOVERED:
                Stream->Statistics().FrameDecodeRecoveredError++;
                break;

            case H264_DECODER_ERROR_NOT_RECOVERED:
                Stream->Statistics().FrameDecodeNotRecoveredError++;
                break;

            case H264_DECODER_ERROR_TASK_TIMEOUT:
                Stream->Statistics().FrameDecodeErrorTaskTimeOutError++;
                break;

            default:
                Stream->Statistics().FrameDecodeError++;
                break;
            }
        }
    }

    return CodecNoError;
}
//}}}
