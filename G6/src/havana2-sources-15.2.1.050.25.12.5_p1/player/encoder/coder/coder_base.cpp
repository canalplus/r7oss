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

#include "coder_base.h"
#include "encoder_generic.h"
#include "allocinline.h"

extern "C" int snprintf(char *buf, long size, const char *fmt, ...) __attribute__((format(printf, 3, 4))) ;

#undef TRACE_TAG
#define TRACE_TAG "Coder_Base_c"

Coder_Base_c::Coder_Base_c(void)
    : Lock()
    , CodedFrameBuffer(NULL)
    , CodedFrameBufferType(0)
    , OutputMetaDataBufferType(0)
    , MetaDataSequenceNumberType(0)
    , InputBufferType(0)
    , InputMetaDataBufferType(0)
    , OutputRing(NULL)
    , BufferManager(NULL)
    , CodedFrameBufferPool(NULL)
    , CodedFrameMemoryDevice(NULL)
    , CodedFrameMemory()
    , CodedMemorySize(0)        // to be set through SetFrameMemory() by derived class
    , CodedFrameMaximumSize(0)  // to be set through SetFrameMemory() by derived class
    , CodedMaxNbBuffers(0)      // to be set through SetFrameMemory() by derived class
    , CodedMemoryPartitionName()
    , CoderDiscontinuity(STM_SE_DISCONTINUITY_CONTINUOUS)
{
    OS_InitializeMutex(&Lock);
}

Coder_Base_c::~Coder_Base_c(void)
{
    Halt();

    AllocatorClose(&CodedFrameMemoryDevice);

    if (CodedFrameBufferPool != NULL)
    {
        CodedFrameBufferPool->DetachMetaData(OutputMetaDataBufferType);
        CodedFrameBufferPool->DetachMetaData(MetaDataSequenceNumberType);
        BufferManager->DestroyPool(CodedFrameBufferPool);
    }

    OS_TerminateMutex(&Lock);
}

CoderStatus_t Coder_Base_c::Halt(void)
{
    BaseComponentClass_c::Halt();
    return CoderNoError;
}

CoderStatus_t Coder_Base_c::SetFrameMemory(unsigned int FrameSize, unsigned int NbBuffers)
{
    SE_DEBUG(GetGroupTrace(), "FrameSize:%d NbBuffers:%d\n", FrameSize, NbBuffers);
    CodedMemorySize       = FrameSize * NbBuffers;
    CodedFrameMaximumSize = FrameSize;
    CodedMaxNbBuffers     = NbBuffers;
    return CoderNoError;
}

CoderStatus_t Coder_Base_c::RegisterOutputBufferRing(Ring_t       Ring)
{
    OutputRing = Ring;
    // We have an Output Ring, and can now be considered an object with a destination
    SetComponentState(ComponentRunning);
    return CoderNoError;
}

CoderStatus_t Coder_Base_c::ManageMemoryProfile()
{
    return CoderNoError;
}

CoderStatus_t Coder_Base_c::RegisterBufferManager(BufferManager_t BufferManager)
{
    if (this->BufferManager != NULL)
    {
        SE_ERROR("Attempt to change buffer manager, not permitted\n");
        return EncoderError;
    }

    SE_ASSERT(CodedFrameMemoryDevice == NULL);
    SE_ASSERT(CodedFrameBufferPool == NULL);

    this->BufferManager = BufferManager;

    // Register the Buffer Types locally from the Encoder Parent
    const EncoderBufferTypes   *BufferTypes = Encoder.Encoder->GetBufferTypes();
    CodedFrameBufferType        = BufferTypes->CodedFrameBufferType;
    OutputMetaDataBufferType    = BufferTypes->InternalMetaDataBufferType;
    MetaDataSequenceNumberType  = BufferTypes->MetaDataSequenceNumberType;
    InputBufferType             = BufferTypes->PreprocFrameBufferType;
    //Input metadata buffer of coder is a preproc metadata output buffer of METADATA_INTERNAL_ENCODE_FRAME_PARAMETERS_TYPE (__stm_se_frame_metadata_t)
    InputMetaDataBufferType     = BufferTypes->InternalMetaDataBufferType;

    //
    // Get the memory and Create the pool with it
    //
    unsigned int EncodeID = Encoder.Encode->GetEncodeID();
    snprintf(CodedMemoryPartitionName, sizeof(CodedMemoryPartitionName), "%s-%d", CODED_FRAME_BUFFER_PARTITION, (EncodeID % 2));
    CodedMemoryPartitionName[sizeof(CodedMemoryPartitionName) - 1] = '\0';

    SE_DEBUG(GetGroupTrace(), "to allocate memory for %s\n", CodedMemoryPartitionName);
    SE_DEBUG(GetGroupTrace(), "to allocate memory for Coded MemorySize:%u FrameMaximumSize:%u MaxNbBuffers:%u\n",
             CodedMemorySize, CodedFrameMaximumSize, CodedMaxNbBuffers);

    allocator_status_t AStatus = PartitionAllocatorOpen(&CodedFrameMemoryDevice, CodedMemoryPartitionName, CodedMemorySize, MEMORY_DEFAULT_ACCESS);
    if (AStatus != allocator_ok)
    {
        CodedFrameMemoryDevice = NULL;
        SE_ERROR("Failed to allocate memory for %s %d\n", CodedMemoryPartitionName, CodedMemorySize);
        return EncoderNoMemory;
    }

    CodedFrameMemory[CachedAddress]   = AllocatorUserAddress(CodedFrameMemoryDevice);
    CodedFrameMemory[PhysicalAddress] = AllocatorPhysicalAddress(CodedFrameMemoryDevice);

    BufferStatus_t Status = BufferManager->CreatePool(&CodedFrameBufferPool, CodedFrameBufferType, CodedMaxNbBuffers, CodedMemorySize, CodedFrameMemory);
    if (Status != BufferNoError)
    {
        CodedFrameBufferPool     = NULL;

        AllocatorClose(&CodedFrameMemoryDevice);

        SE_ERROR("Failed to create the pool\n");
        return EncoderNoMemory;
    }

    // Output meta data management
    if (OutputMetaDataBufferType == NULL)
    {
        SE_ERROR("Output meta buffer type not initialized yet\n");
        return Status;
    }

    Status = CodedFrameBufferPool->AttachMetaData(OutputMetaDataBufferType, sizeof(__stm_se_frame_metadata_t));
    if (Status != BufferNoError)
    {
        SE_ERROR("Failed to attach the meta data to coded buffer\n");
        return Status;
    }

    // Meta data  sequence management
    if (MetaDataSequenceNumberType == NULL)
    {
        SE_ERROR("Meta data sequence type not initialized yet\n");
        return Status;
    }

    Status = CodedFrameBufferPool->AttachMetaData(MetaDataSequenceNumberType, sizeof(EncoderSequenceNumber_t));
    if (Status != BufferNoError)
    {
        SE_ERROR("Failed to attach the meta data to coded buffer\n");
        return Status;
    }

    return Status;
}

CoderStatus_t Coder_Base_c::GetBufferPoolDescriptor(Buffer_t      Buffer)
{
    return CoderNoError;
}

CoderStatus_t Coder_Base_c::Output(Buffer_t   Buffer,
                                   bool          Marker)
{
    /* Detach any input buffer now that the transform is complete */
    Buffer_t InputBuffer;
    Buffer->ObtainAttachedBufferReference(InputBufferType, &InputBuffer);
    if (InputBuffer != NULL)
    {
        Buffer->DetachBuffer(InputBuffer);
    }

    RingStatus_t Status = OutputRing->Insert((uintptr_t) Buffer);
    if (Marker == false && Status == RingNoError)
    {
        Encoder.EncodeStream->EncodeStreamStatistics().FrameCountFromCoder++;
        // Signal frame encoded after buffers are push out.
        SignalEvent(STM_SE_ENCODE_STREAM_EVENT_FRAME_ENCODED);
    }
    else if (Marker == true && Status == RingNoError)
    {
        Encoder.EncodeStream->EncodeStreamStatistics().EosBufferCountFromCoder++;
    }

    return (Status == RingNoError ? CoderNoError : CoderError);
}

CoderStatus_t Coder_Base_c::GetNewCodedBuffer(Buffer_t *Buffer, uint32_t Size)
{
    if (NULL == Buffer)
    {
        SE_ERROR("Buffer is NULL\n");
        return CoderError;
    }

    BufferStatus_t BufferStatus = BufferNoError;
    uint64_t TimeBetweenMessageUs = 5000000ULL;
    uint64_t EntryTime            = OS_GetTimeInMicroSeconds();
    uint32_t GetBufferTimeOutMs   = 100;
    do
    {
        BufferStatus = CodedFrameBufferPool->GetBuffer(Buffer, IdentifierEncoderCoder, CodedFrameMaximumSize,
                                                       false, false,
                                                       GetBufferTimeOutMs);
        // Warning message every TimeBetweenMessageUs us
        if ((OS_GetTimeInMicroSeconds() - EntryTime) > TimeBetweenMessageUs)
        {
            EntryTime = OS_GetTimeInMicroSeconds();
            SE_WARNING("Coder(%p)->CodedFrameBufferPool(%p)->GetBuffer still NoFreeBufferAvailable with %s\n",
                       this, CodedFrameBufferPool,
                       TestComponentState(ComponentRunning) ? "ComponentRunning" : "!ComponentRunning");
        }
    }
    while ((BufferNoFreeBufferAvailable == BufferStatus) && TestComponentState(ComponentRunning));

    if ((BufferNoError != BufferStatus) || (NULL == *Buffer))
    {
        CODER_ERROR_RUNNING("Unable to get coded frame buffer\n");
        *Buffer = NULL;
        return CoderError;
    }

    return CoderNoError;
}

/// This Function should only deal with common aspects regarding Input of a buffer
/// The implementation classes must deal with the actual input.
CoderStatus_t Coder_Base_c::Input(Buffer_t    Buffer)
{
    AssertComponentState(ComponentRunning);

    Encoder.EncodeStream->EncodeStreamStatistics().BufferCountToCoder++;

    // Get a New 'CodedFrameBuffer'
    CoderStatus_t Status = GetNewCodedBuffer(&CodedFrameBuffer, CodedFrameMaximumSize);
    if (Status != CoderNoError)
    {
        CODER_ERROR_RUNNING("Unable to get new buffer\n");
        return Status;
    }

    // Attach the Input buffer to the CodedFrameBuffer so that it is kept until Encoded
    CodedFrameBuffer->AttachBuffer(Buffer);

    Status = DetectBufferDiscontinuity(Buffer);
    if (Status != CoderNoError)
    {
        CodedFrameBuffer->DecrementReferenceCount(IdentifierEncoderCoder);
        return Status;
    }

    return CoderNoError;
}

/// \brief    Performs coder initialization in particular performing InitializeMMETransformer() call
/// \brief    To be overloaded by Coder specific method
CoderStatus_t Coder_Base_c::InitializeCoder(void)
{
    return CoderNoError;
}

/// \brief    Terminate coder instance in particular performing TerminateMMETransformer() call
/// \brief    To be overloaded by Coder specific method
CoderStatus_t Coder_Base_c::TerminateCoder(void)
{
    return CoderNoError;
}

CoderStatus_t Coder_Base_c::GetControl(stm_se_ctrl_t    Control,
                                       void            *Data)
{
    switch (Control)
    {
    default:
        // Cannot trace this as error because it can be a valid control for another object
        SE_DEBUG(GetGroupTrace(), "Not match coder control %u\n", Control);
        return CoderControlNotMatch;
    }
}

CoderStatus_t Coder_Base_c::GetCompoundControl(stm_se_ctrl_t    Control,
                                               void            *Data)
{
    switch (Control)
    {
    default:
        // Cannot trace this as error because it can be a valid control for another object
        SE_DEBUG(GetGroupTrace(), "Not match coder control %u\n", Control);
        return CoderControlNotMatch;
    }
}

CoderStatus_t Coder_Base_c::SetControl(stm_se_ctrl_t    Control,
                                       const void      *Data)
{
    switch (Control)
    {
    default:
        // Cannot trace this as error because it can be a valid control for another object
        SE_DEBUG(GetGroupTrace(), "Not match coder control %u\n", Control);
        return CoderControlNotMatch;
    }
}

CoderStatus_t Coder_Base_c::SetCompoundControl(stm_se_ctrl_t    Control,
                                               const void      *Data)
{
    switch (Control)
    {
    default:
        // Cannot trace this as error because it can be a valid control for another object
        SE_DEBUG(GetGroupTrace(), "Not match coder control %u\n", Control);
        return CoderControlNotMatch;
    }
}

CoderStatus_t Coder_Base_c::SignalEvent(stm_se_encode_stream_event_t Event)
{
    CoderStatus_t Status = CoderNoError;
    Status = Encoder.EncodeStream->SignalEvent(Event);

    if (Status != EncoderNoError)
    {
        return CoderError;
    }

    return CoderNoError;
}

CoderStatus_t Coder_Base_c::DetectBufferDiscontinuity(Buffer_t   Buffer)
{
    stm_se_uncompressed_frame_metadata_t *InputMetaDataDescriptor;
    __stm_se_frame_metadata_t            *FullPreProcMetaDataDescriptor;
    uint32_t                              DataSize;
    uint32_t                              BufferAddr;
    stm_se_discontinuity_t                NonContinuousDiscontinuity;
    stm_se_discontinuity_t                ContinuousDiscontinuity;

    // Obtain MetaData from Input.
    Buffer->ObtainMetaDataReference(InputMetaDataBufferType, (void **)(&FullPreProcMetaDataDescriptor));
    SE_ASSERT(FullPreProcMetaDataDescriptor != NULL);

    InputMetaDataDescriptor = &FullPreProcMetaDataDescriptor->uncompressed_frame_metadata;

    // Obtain data reference from Input
    Buffer->ObtainDataReference(NULL, &DataSize, (void **)(&BufferAddr));
    if (BufferAddr == NULL)
    {
        SE_ERROR("Unable to obtain buffer address\n");
        return CoderError;
    }

    // Check for invalid discontinuity cases and warn
    // Invalid Case 1: Continuous discontinuity metadata + zero datasize
    ContinuousDiscontinuity = (stm_se_discontinuity_t)(InputMetaDataDescriptor->discontinuity & STM_SE_DISCONTINUITY_CONTINUOUS);
    ContinuousDiscontinuity = (stm_se_discontinuity_t)((InputMetaDataDescriptor->discontinuity & STM_SE_DISCONTINUITY_MUTE)    | ContinuousDiscontinuity);
    ContinuousDiscontinuity = (stm_se_discontinuity_t)((InputMetaDataDescriptor->discontinuity & STM_SE_DISCONTINUITY_FADEOUT) | ContinuousDiscontinuity);
    ContinuousDiscontinuity = (stm_se_discontinuity_t)((InputMetaDataDescriptor->discontinuity & STM_SE_DISCONTINUITY_FADEIN)  | ContinuousDiscontinuity);
    if ((ContinuousDiscontinuity) && (DataSize == 0))
    {
        SE_ERROR("Invalid input buffer (null buffer with no discontinuity)\n");
        return CoderError;
    }

    // Invalid Case 2: discontinuity metadata + valid datasize
    NonContinuousDiscontinuity = InputMetaDataDescriptor->discontinuity;
    NonContinuousDiscontinuity = (stm_se_discontinuity_t)(NonContinuousDiscontinuity & ~STM_SE_DISCONTINUITY_CONTINUOUS);
    NonContinuousDiscontinuity = (stm_se_discontinuity_t)(NonContinuousDiscontinuity & ~STM_SE_DISCONTINUITY_MUTE);
    NonContinuousDiscontinuity = (stm_se_discontinuity_t)(NonContinuousDiscontinuity & ~STM_SE_DISCONTINUITY_FADEOUT);
    NonContinuousDiscontinuity = (stm_se_discontinuity_t)(NonContinuousDiscontinuity & ~STM_SE_DISCONTINUITY_FADEIN);
    if ((NonContinuousDiscontinuity) && (DataSize != 0))
    {
        SE_ERROR("Invalid input buffer (non-zero buffer with discontinuity)\n");
        return CoderError;
    }

    // Null buffer detection
    if (DataSize == 0)
    {
        // Accumulate Coder Discontinuity from null buffers
        CoderDiscontinuity = (stm_se_discontinuity_t)(CoderDiscontinuity | InputMetaDataDescriptor->discontinuity);
    }
    else if (STM_SE_DISCONTINUITY_CONTINUOUS == InputMetaDataDescriptor->discontinuity)
    {
        // Latch CoderDiscontinuity accumulated from previous null frames to input discontinuity
        InputMetaDataDescriptor->discontinuity = CoderDiscontinuity;
        CoderDiscontinuity = STM_SE_DISCONTINUITY_CONTINUOUS;
    }

    return CoderNoError;
}

CoderStatus_t Coder_Base_c::GenerateBufferEOS(Buffer_t   Buffer)
{
    stm_se_compressed_frame_metadata_t    *OutputMetaDataDescriptor;
    __stm_se_frame_metadata_t             *FullCoderMetaDataDescriptor;
    uint32_t                               DataSize;

    // Obtain MetaData from Output.
    Buffer->ObtainMetaDataReference(OutputMetaDataBufferType, (void **)(&FullCoderMetaDataDescriptor));
    SE_ASSERT(FullCoderMetaDataDescriptor != NULL);

    OutputMetaDataDescriptor = &FullCoderMetaDataDescriptor->compressed_frame_metadata;
    // Initialize coder metadata with discontinuity EOS
    memset(FullCoderMetaDataDescriptor, '\0', sizeof(__stm_se_frame_metadata_t));

    // What other metadata to program to be valid for coder?
    OutputMetaDataDescriptor->discontinuity = STM_SE_DISCONTINUITY_EOS;

    // Ouput EOS
    DataSize = 0;
    Buffer->SetUsedDataSize(DataSize);
    BufferStatus_t BufStatus = Buffer->ShrinkBuffer(max(DataSize, 1));
    if (BufStatus != BufferNoError)
    {
        SE_ERROR("Failed to shrink the operating buffer to size (%08x)\n", BufStatus);
        return CoderError;
    }

    //Signal through 'bool' this EOS buffer is a 'marker' buffer so that frame encoded event is not generated
    return Output(Buffer, true);
}

// /////////////////////////////////////////////////////////////////////////
//
//      Low power methods
//      Default implementation is no action
//      This implementation must be overloaded by coders using MME connection
//

CoderStatus_t  Coder_Base_c::LowPowerEnter(void)
{
    return CoderNoError;
}

CoderStatus_t  Coder_Base_c::LowPowerExit(void)
{
    return CoderNoError;
}
