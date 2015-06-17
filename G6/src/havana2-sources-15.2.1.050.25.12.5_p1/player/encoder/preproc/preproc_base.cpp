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

#include "preproc_base.h"
#include "allocinline.h"

#undef TRACE_TAG
#define TRACE_TAG "Preproc_Base_c"

extern "C" int snprintf(char *buf, long size, const char *fmt, ...) __attribute__((format(printf, 3, 4))) ;

static BufferDataDescriptor_t PreprocContextDescriptor = BUFFER_PREPROC_CONTEXT_TYPE;

Preproc_Base_c::Preproc_Base_c(void)
    : Configuration()
    , PreprocFrameBuffer(NULL)
    , PreprocFrameBufferType(0)
    , PreprocFrameAllocType(0)
    , InputBufferType(0)
    , InputMetaDataBufferType(0)
    , EncodeCoordinatorMetaDataBufferType(0)
    , OutputMetaDataBufferType(0)
    , MetaDataSequenceNumberType(0)
    , OutputRing(NULL)
    , BufferManager(NULL)
    , PreprocFrameBufferPool(NULL)
    , PreprocFrameAllocPool(NULL)
    , PreprocFrameMemoryDevice(NULL)
    , PreprocFrameMemory()
    , PreprocMemorySize(0)        // to be set through SetFrameMemory() by derived class
    , PreprocFrameMaximumSize(0)  // to be set through SetFrameMemory() by derived class
    , PreprocMaxNbAllocBuffers(0) // to be set through SetFrameMemory() by derived class
    , PreprocMaxNbBuffers(ENCODER_STREAM_MAX_PREPROC_BUFFERS)
    , PreprocMemoryPartitionName()
    , InputMetaDataDescriptor(NULL)
    , EncodeCoordinatorMetaDataDescriptor(NULL)
    , PreprocMetaDataDescriptor(NULL)
    , PreprocFullMetaDataDescriptor(NULL)
    , PreprocContextBuffer(NULL)
    , PreprocContextBufferType(0)
    , PreprocContextBufferPool(NULL)
    , PreprocDiscontinuity(STM_SE_DISCONTINUITY_CONTINUOUS)
    , InputBufferSize(0)
{
    // Non null because not all classes use it.
    // This will be overwritten by child classes if needed
    Configuration.PreprocContextDescriptor = &PreprocContextDescriptor;
}

Preproc_Base_c::~Preproc_Base_c(void)
{
    Halt();

    AllocatorClose(&PreprocFrameMemoryDevice);

    if (PreprocFrameAllocPool != NULL)
    {
        BufferManager->DestroyPool(PreprocFrameAllocPool);
    }

    if (PreprocFrameBufferPool != NULL)
    {
        PreprocFrameBufferPool->DetachMetaData(OutputMetaDataBufferType);
        PreprocFrameBufferPool->DetachMetaData(MetaDataSequenceNumberType);
        BufferManager->DestroyPool(PreprocFrameBufferPool);
    }

    if (PreprocContextBufferPool != NULL)
    {
        BufferManager->DestroyPool(PreprocContextBufferPool);
    }
}

PreprocStatus_t Preproc_Base_c::Halt(void)
{
    BaseComponentClass_c::Halt();
    return PreprocNoError;
}

PreprocStatus_t Preproc_Base_c::SetFrameMemory(unsigned int FrameSize, unsigned int NbBuffers)
{
    SE_DEBUG(GetGroupTrace(), "FrameSize:%d NbBuffers:%d\n", FrameSize, NbBuffers);
    PreprocMemorySize        = FrameSize * NbBuffers;
    PreprocFrameMaximumSize  = FrameSize;
    PreprocMaxNbAllocBuffers = NbBuffers;
    return PreprocNoError;
}

PreprocStatus_t Preproc_Base_c::ManageMemoryProfile()
{
    return PreprocNoError;
}

PreprocStatus_t Preproc_Base_c::RegisterBufferManager(BufferManager_t   BufferManager)
{
    allocator_status_t  AStatus = allocator_ok;

    if (this->BufferManager != NULL)
    {
        SE_ERROR("Attempt to change buffer manager, not permitted\n");
        return PreprocError;
    }
    SE_ASSERT(PreprocFrameMemoryDevice == NULL);
    SE_ASSERT(PreprocFrameAllocPool == NULL);
    SE_ASSERT(PreprocFrameBufferPool == NULL);

    this->BufferManager = BufferManager;

    const EncoderBufferTypes *BufferTypes = Encoder.Encoder->GetBufferTypes();
    PreprocFrameBufferType      = BufferTypes->PreprocFrameBufferType;
    PreprocFrameAllocType       = BufferTypes->PreprocFrameAllocType;
    //input metadata preproc buffer is of type METADATA_ENCODE_FRAME_PARAMETERS_TYPE (stm_se_uncompressed_frame_metadata_t)
    InputMetaDataBufferType     = BufferTypes->InputMetaDataBufferType;
    //encode coordinator metadata preproc buffer is of type METADATA_ENCODE_COORDINATOR_FRAME_PARAMETERS_TYPE (__stm_se_encode_coordinator_metadata_t)
    EncodeCoordinatorMetaDataBufferType = BufferTypes->EncodeCoordinatorMetaDataBufferType;
    //output metadata preproc buffer is of type METADATA_INTERNAL_ENCODE_FRAME_PARAMETERS_TYPE (__stm_se_frame_metadata_t)
    OutputMetaDataBufferType    = BufferTypes->InternalMetaDataBufferType;
    InputBufferType             = BufferTypes->BufferInputBufferType;
    MetaDataSequenceNumberType  = BufferTypes->MetaDataSequenceNumberType;

    // Input meta data management
    if ((PreprocFrameBufferType == NULL) || (PreprocFrameAllocType == NULL) || (InputMetaDataBufferType == NULL) || (OutputMetaDataBufferType == NULL) ||
        (EncodeCoordinatorMetaDataBufferType == NULL) || (InputBufferType == NULL) || (MetaDataSequenceNumberType == NULL))
    {
        SE_ERROR("Invalid type registrations\n");
        return EncoderError;
    }

    //
    // Get the memory and Create the pool with it
    //
    unsigned int EncodeID = Encoder.Encode->GetEncodeID();
    snprintf(PreprocMemoryPartitionName, sizeof(PreprocMemoryPartitionName), "%s-%d", PREPROC_FRAME_BUFFER_PARTITION, (EncodeID % 2));
    PreprocMemoryPartitionName[sizeof(PreprocMemoryPartitionName) - 1] = '\0';

    SE_DEBUG(GetGroupTrace(), "to allocate memory for %s %d\n", PreprocMemoryPartitionName, PreprocMemorySize);

    AStatus = PartitionAllocatorOpen(&PreprocFrameMemoryDevice, PreprocMemoryPartitionName, PreprocMemorySize, MEMORY_DEFAULT_ACCESS);
    if (AStatus != allocator_ok)
    {
        PreprocFrameMemoryDevice = NULL;
        SE_ERROR("Failed to allocate memory for %s %d\n", PreprocMemoryPartitionName, PreprocMemorySize);
        return EncoderNoMemory;
    }

    PreprocFrameMemory[CachedAddress]         = AllocatorUserAddress(PreprocFrameMemoryDevice);
    PreprocFrameMemory[PhysicalAddress]       = AllocatorPhysicalAddress(PreprocFrameMemoryDevice);

    // Initialize preproc frame alloc pool
    BufferStatus_t BufferStatus = BufferManager->CreatePool(&PreprocFrameAllocPool, PreprocFrameAllocType,
                                                            PreprocMaxNbAllocBuffers,
                                                            PreprocMemorySize,
                                                            PreprocFrameMemory);
    if (BufferStatus != BufferNoError)
    {
        PreprocFrameAllocPool = NULL;

        AllocatorClose(&PreprocFrameMemoryDevice);

        SE_ERROR("Failed to create the alloc pool\n");
        return PreprocError;
    }

    // Initialize preproc frame buffer pool
    BufferStatus  = BufferManager->CreatePool(&PreprocFrameBufferPool, PreprocFrameBufferType, PreprocMaxNbBuffers);
    if (BufferStatus != BufferNoError)
    {
        PreprocFrameBufferPool = NULL;
        SE_ERROR("Failed to create the buffer pool\n");
        return PreprocError;
    }

    BufferStatus = PreprocFrameBufferPool->AttachMetaData(OutputMetaDataBufferType, sizeof(__stm_se_frame_metadata_t));
    if (BufferStatus != BufferNoError)
    {
        SE_ERROR("Failed to attach the output meta data to preproc buffer\n");
        return PreprocError;
    }

    BufferStatus = PreprocFrameBufferPool->AttachMetaData(MetaDataSequenceNumberType, sizeof(EncoderSequenceNumber_t));
    if (BufferStatus != BufferNoError)
    {
        SE_ERROR("Failed to attach the sequence number meta data to preproc buffer\n");
        return PreprocError;
    }

    return PreprocNoError;
}

PreprocStatus_t Preproc_Base_c::RegisterOutputBufferRing(Ring_t       Ring)
{
    OutputRing = Ring;

    BufferStatus_t BufferStatus = BufferManager->FindBufferDataType(Configuration.PreprocContextDescriptor->TypeName,
                                                                    &PreprocContextBufferType);
    if (BufferStatus != BufferNoError)
    {
        // Create buffer data type if it does not exist
        BufferStatus = BufferManager->CreateBufferDataType(Configuration.PreprocContextDescriptor, &PreprocContextBufferType);

        if (BufferStatus != BufferNoError)
        {
            SE_ERROR("Unable to create preproc context buffer type\n");
            return PreprocError;
        }
    }

    SE_ASSERT(PreprocContextBufferPool == NULL);
    BufferStatus = BufferManager->CreatePool(&PreprocContextBufferPool, PreprocContextBufferType, Configuration.PreprocContextCount);
    if (BufferStatus != BufferNoError)
    {
        PreprocContextBufferPool = NULL;
        SE_ERROR("Unable to create preproc context pool\n");
        return PreprocError;
    }

    // We have an Output Ring, and can now be considered an object with a destination
    SetComponentState(ComponentRunning);
    return PreprocNoError;
}

PreprocStatus_t Preproc_Base_c::OutputPartialPreprocBuffers(void)
{
    return PreprocNoError;
}

PreprocStatus_t Preproc_Base_c::GetNewBuffer(Buffer_t *Buffer, bool IsADiscontinuityBuffer)
{
    BufferStatus_t BufferStatus = BufferNoError;
    Buffer_t       ContentBuffer;
    unsigned    int PreprocSize = PreprocFrameMaximumSize;
    uint64_t TimeBetweenMessageUs = 5000000ULL;
    uint64_t EntryTime            = OS_GetTimeInMicroSeconds();
    uint32_t GetBufferTimeOutMs   = 100;

    // For a pure discontinuity buffer, ask for a buffer with minimum size
    // Buffer of size 0 cannot be used because not well handled by other SE objects
    if (IsADiscontinuityBuffer) { PreprocSize = 1; }

    do
    {
        BufferStatus = PreprocFrameAllocPool->GetBuffer(&ContentBuffer, IdentifierEncoderPreprocessor, PreprocSize,
                                                        false, false,
                                                        GetBufferTimeOutMs);

        // Warning message every TimeBetweenMessageUs us
        if ((OS_GetTimeInMicroSeconds() - EntryTime) > TimeBetweenMessageUs)
        {
            EntryTime = OS_GetTimeInMicroSeconds();
            SE_WARNING("Preproc(%p)->PreprocFrameAllocPool(%p)->GetBuffer still NoFreeBufferAvailable with %s\n",
                       this, PreprocFrameAllocPool,
                       TestComponentState(ComponentRunning) ? "ComponentRunning" : "!ComponentRunning");
        }
    }
    while ((BufferNoFreeBufferAvailable == BufferStatus) && TestComponentState(ComponentRunning));

    if ((BufferStatus != BufferNoError) || (ContentBuffer == NULL))
    {
        PREPROC_ERROR_RUNNING("Unable to get preproc frame alloc buffer\n");
        *Buffer = NULL;
        return PreprocError;
    }

    PreprocStatus_t Status = GetBufferClone(ContentBuffer, Buffer);

    // Release the reference to the first clone of the content buffer
    ContentBuffer->DecrementReferenceCount(IdentifierEncoderPreprocessor);

    return Status;
}

PreprocStatus_t Preproc_Base_c::GetBufferClone(Buffer_t Buffer, Buffer_t *CloneBuffer)
{
    BufferStatus_t BufferStatus;
    uint32_t        DataSize;
    void           *BufferAddr[3];
    BufferType_t    BufferType;
    Buffer_t        ContentBuffer;

    *CloneBuffer = NULL;

    // Check if buffer type is correct before cloning
    Buffer->GetType(&BufferType);

    // Retrieve the content buffer
    if (BufferType == PreprocFrameAllocType)
    {
        ContentBuffer = Buffer;
    }
    else if (BufferType == PreprocFrameBufferType)
    {
        Buffer->ObtainAttachedBufferReference(PreprocFrameAllocType, &ContentBuffer);
        SE_ASSERT(ContentBuffer != NULL);
    }
    else
    {
        SE_ERROR("Cloning of buffer type %d not supported\n", BufferType);
        return PreprocError;
    }

    // Get new preproc frame buffer
    uint64_t TimeBetweenMessageUs = 5000000ULL;
    uint64_t EntryTime            = OS_GetTimeInMicroSeconds();
    uint32_t GetBufferTimeOutMs   = 100;
    do
    {
        BufferStatus = PreprocFrameBufferPool->GetBuffer(CloneBuffer, IdentifierEncoderPreprocessor,
                                                         UNSPECIFIED_SIZE, false, false,
                                                         GetBufferTimeOutMs);

        // Warning message every TimeBetweenMessageUs us
        if ((OS_GetTimeInMicroSeconds() - EntryTime) > TimeBetweenMessageUs)
        {
            EntryTime = OS_GetTimeInMicroSeconds();
            SE_WARNING("Preproc(%p)->PreProcFrameBufferPool(%p)->GetBuffer still NoFreeBufferAvailable with %s\n",
                       this, PreprocFrameBufferPool,
                       TestComponentState(ComponentRunning) ? "ComponentRunning" : "!ComponentRunning");
        }
    }
    while ((BufferNoFreeBufferAvailable == BufferStatus) && TestComponentState(ComponentRunning));

    if ((BufferStatus != BufferNoError) || (*CloneBuffer == NULL))
    {
        PREPROC_ERROR_RUNNING("Unable to get preproc frame buffer\n");
        *CloneBuffer = NULL;
        return PreprocError;
    }

    // Initialize buffer address
    BufferAddr[0] = NULL;
    BufferAddr[1] = NULL;
    BufferAddr[2] = NULL;
    // Get physical address of buffer
    ContentBuffer->ObtainDataReference(NULL, &DataSize, (void **)(&BufferAddr[PhysicalAddress]), PhysicalAddress);
    if (BufferAddr[PhysicalAddress] == NULL)
    {
        SE_ERROR("Unable to get buffer phys reference\n");
        (*CloneBuffer)->DecrementReferenceCount(IdentifierEncoderPreprocessor);
        *CloneBuffer = NULL;
        return PreprocError;
    }

    // Get virtual address of buffer
    ContentBuffer->ObtainDataReference(NULL, &DataSize, (void **)(&BufferAddr[CachedAddress]), CachedAddress);
    if (BufferAddr[CachedAddress] == NULL)
    {
        SE_ERROR("Unable to get buffer reference\n");
        (*CloneBuffer)->DecrementReferenceCount(IdentifierEncoderPreprocessor);
        *CloneBuffer = NULL;
        return PreprocError;
    }

    // Register Input buffer pointers into the buffer copy
    (*CloneBuffer)->RegisterDataReference(PreprocFrameMaximumSize, BufferAddr);
    (*CloneBuffer)->SetUsedDataSize(DataSize);

    // Attach the content buffer to the BufferCopy so that it is kept until Preprocessing is complete.
    (*CloneBuffer)->AttachBuffer(ContentBuffer);

    return PreprocNoError;
}

PreprocStatus_t Preproc_Base_c::GetNewContext(Buffer_t *Buffer)
{
    if (NULL == Buffer)
    {
        SE_ERROR("Buffer is NULL\n");
        return PreprocError;
    }

    uint64_t TimeBetweenMessageUs = 5000000ULL;
    uint64_t EntryTime            = OS_GetTimeInMicroSeconds();
    uint32_t GetBufferTimeOutMs   = 100;
    BufferStatus_t BufferStatus;

    do
    {
        BufferStatus = PreprocContextBufferPool->GetBuffer(Buffer, IdentifierEncoderPreprocessor,
                                                           UNSPECIFIED_SIZE, false, false,
                                                           GetBufferTimeOutMs);

        // Warning message every TimeBetweenMessageUs us
        if ((OS_GetTimeInMicroSeconds() - EntryTime) > TimeBetweenMessageUs)
        {
            EntryTime = OS_GetTimeInMicroSeconds();
            SE_WARNING("Preproc(%p)->PreprocContextBufferPool(%p)->GetBuffer still NoFreeBufferAvailable with %s\n",
                       this, PreprocContextBufferPool,
                       TestComponentState(ComponentRunning) ? "ComponentRunning" : "!ComponentRunning");
        }
    }
    while ((BufferNoFreeBufferAvailable == BufferStatus) && TestComponentState(ComponentRunning));

    if (BufferStatus != BufferNoError || *Buffer == NULL)
    {
        PREPROC_ERROR_RUNNING("Unable to get preproc context buffer (BufStatus %08X, Buffer %p)\n", BufferStatus, *Buffer);
        *Buffer = NULL;
        return PreprocError;
    }

    return PreprocNoError;
}

PreprocStatus_t Preproc_Base_c::Output(Buffer_t   Buffer,
                                       bool       Marker)
{
    // Detach any input buffer now that the transform is complete
    Buffer_t InputBuffer;
    Buffer->ObtainAttachedBufferReference(InputBufferType, &InputBuffer);
    if (InputBuffer != NULL)
    {
        Buffer->DetachBuffer(InputBuffer);
    }

    RingStatus_t Status = OutputRing->Insert((uintptr_t) Buffer);
    if (Status != RingNoError)
    {
        SE_ERROR("Unable to insert buffer in Preproc Ring\n");
        Buffer->DecrementReferenceCount(IdentifierEncoderPreprocessor);
        return PreprocError;
    }
    else
    {
        Encoder.EncodeStream->EncodeStreamStatistics().BufferCountFromPreproc++;
    }

    return PreprocNoError;
}

/// This Function should only deal with common aspects regarding Input of a buffer
/// The implementation classes must deal with the actual input.
PreprocStatus_t Preproc_Base_c::Input(Buffer_t    Buffer)
{
    AssertComponentState(ComponentRunning);

    // Track all incoming buffers in statistics
    Encoder.EncodeStream->EncodeStreamStatistics().BufferCountToPreproc++;

    // Get a New 'PreprocFrameBuffer'
    PreprocStatus_t Status = GetNewBuffer(&PreprocFrameBuffer);
    if (Status != PreprocNoError)
    {
        PREPROC_ERROR_RUNNING("Unable to get new buffer, Status = %u\n", Status);
        return PreprocError;
    }

    // Retrieve input metadata
    Buffer->ObtainMetaDataReference(InputMetaDataBufferType, (void **)(&InputMetaDataDescriptor));
    SE_ASSERT(InputMetaDataDescriptor != NULL);

    // Dump input metadata
    DumpInputMetadata(InputMetaDataDescriptor);

    // Check native time format
    Status = CheckNativeTimeFormat(InputMetaDataDescriptor->native_time_format, InputMetaDataDescriptor->native_time);
    if (Status != PreprocNoError)
    {
        SE_ERROR("Bad native time format, Status = %u\n", Status);
        PreprocFrameBuffer->DecrementReferenceCount(IdentifierEncoderPreprocessor);
        return PreprocError;
    }

    // Detect Buffer Discontinuity
    Status = DetectBufferDiscontinuity(Buffer);
    if (Status != PreprocNoError)
    {
        SE_ERROR("Discontinuity detection failed, Status = %u\n", Status);
        PreprocFrameBuffer->DecrementReferenceCount(IdentifierEncoderPreprocessor);
        return PreprocError;
    }

    // Retrieve encode coordinator metadata
    Buffer->ObtainMetaDataReference(EncodeCoordinatorMetaDataBufferType, (void **)(&EncodeCoordinatorMetaDataDescriptor));
    SE_ASSERT(EncodeCoordinatorMetaDataDescriptor != NULL);

    // In case of NRT mode, this metadata struct is filled by the EncodeCoordinator
    // Otherwise, it may be not initialized: we set it to native_time for further use in preproc
    if (Encoder.Encode->IsModeNRT() == false)
    {
        memset(EncodeCoordinatorMetaDataDescriptor, 0, sizeof(__stm_se_encode_coordinator_metadata_t));
        EncodeCoordinatorMetaDataDescriptor->encoded_time = InputMetaDataDescriptor->native_time;
        EncodeCoordinatorMetaDataDescriptor->encoded_time_format = InputMetaDataDescriptor->native_time_format;
    }
    else
    {
        // Dump encode coordinator metadata
        DumpEncodeCoordinatorMetadata(EncodeCoordinatorMetaDataDescriptor);

        // Check encoded time format
        Status = CheckNativeTimeFormat(EncodeCoordinatorMetaDataDescriptor->encoded_time_format, EncodeCoordinatorMetaDataDescriptor->encoded_time);
        if (Status != PreprocNoError)
        {
            SE_ERROR("Bad encoded time format, Status = %u\n", Status);
            PreprocFrameBuffer->DecrementReferenceCount(IdentifierEncoderPreprocessor);
            return PreprocError;
        }
    }

    // Retrieve preproc metadata
    PreprocFrameBuffer->ObtainMetaDataReference(OutputMetaDataBufferType, (void **)(&PreprocFullMetaDataDescriptor));
    SE_ASSERT(PreprocFullMetaDataDescriptor != NULL);

    // Initialize preproc metadata
    PreprocMetaDataDescriptor = &PreprocFullMetaDataDescriptor->uncompressed_frame_metadata;
    memcpy(PreprocMetaDataDescriptor, InputMetaDataDescriptor, sizeof(stm_se_uncompressed_frame_metadata_t));
    memcpy(&PreprocFullMetaDataDescriptor->encode_coordinator_metadata, EncodeCoordinatorMetaDataDescriptor, sizeof(__stm_se_encode_coordinator_metadata_t));

    // Attach the Input buffer to the PreprocFrameBuffer so that it is kept until Preprocessing is complete.
    PreprocFrameBuffer->AttachBuffer(Buffer);

    //Only deal with frame stat for relevant buffers (ignore pure discontinuity)
    //NB: InputBufferSize filled by previous DetectBufferDiscontinuity() call
    if (InputBufferSize != 0)
    {
        Encoder.EncodeStream->EncodeStreamStatistics().FrameCountToPreproc++;
    }

    return PreprocNoError;
}

PreprocStatus_t Preproc_Base_c::AbortBlockingCalls(void)
{
    SE_WARNING("\n");
    // TODO(pht) check

    if (PreprocFrameBufferPool)
    {
        PreprocFrameBufferPool->AbortBlockingGetBuffer();
    }

    if (PreprocFrameAllocPool)
    {
        PreprocFrameAllocPool->AbortBlockingGetBuffer();
    }

    if (PreprocContextBufferPool)
    {
        PreprocContextBufferPool->AbortBlockingGetBuffer();
    }

    return PreprocNoError;
}

PreprocStatus_t Preproc_Base_c::GetControl(stm_se_ctrl_t    Control,
                                           void            *Data)
{
    switch (Control)
    {
    default:
        // Cannot trace this as error because it can be a valid control for another object
        SE_DEBUG(GetGroupTrace(), "Not match preproc control %u\n", Control);
        return PreprocControlNotMatch;
    }
}

PreprocStatus_t Preproc_Base_c::GetCompoundControl(stm_se_ctrl_t    Control,
                                                   void            *Data)
{
    switch (Control)
    {
    default:
        // Cannot trace this as error because it can be a valid control for another object
        SE_DEBUG(GetGroupTrace(), "Not match preproc control %u\n", Control);
        return PreprocControlNotMatch;
    }
}

PreprocStatus_t Preproc_Base_c::SetControl(stm_se_ctrl_t    Control,
                                           const void      *Data)
{
    switch (Control)
    {
    default:
        // Cannot trace this as error because it can be a valid control for another object
        SE_DEBUG(GetGroupTrace(), "Not match preproc control %u\n", Control);
        return PreprocControlNotMatch;
    }
}

PreprocStatus_t Preproc_Base_c::SetCompoundControl(stm_se_ctrl_t    Control,
                                                   const void      *Data)
{
    switch (Control)
    {
    default:
        // Cannot trace this as error because it can be a valid control for another object
        SE_DEBUG(GetGroupTrace(), "Not match preproc control %u\n", Control);
        return PreprocControlNotMatch;
    }
}

PreprocStatus_t Preproc_Base_c::CheckNativeTimeFormat(stm_se_time_format_t NativeTimeFormat, uint64_t NativeTime)
{
    switch (NativeTimeFormat)
    {
    case TIME_FORMAT_US:
        break;
    case TIME_FORMAT_PTS:
        if (NativeTime & (~PTS_MASK))
        {
            SE_ERROR("Unconsistent native time %llu, NativeTimeFormat = %u (%s)\n", NativeTime, NativeTimeFormat, StringifyTimeFormat(NativeTimeFormat));
            return EncoderNotSupported;
        }
        break;
    case TIME_FORMAT_27MHz:
        break;
    default:
        SE_ERROR("Invalid native time format %u\n", NativeTimeFormat);
        return EncoderNotSupported;
    }

    return EncoderNoError;
}

PreprocStatus_t Preproc_Base_c::CheckDiscontinuity(stm_se_discontinuity_t   Discontinuity)
{
    // Check for errors
    if (Discontinuity & STM_SE_DISCONTINUITY_OPEN_GOP_REQUEST)
    {
        return EncoderNotSupported;
    }

    if (Discontinuity & STM_SE_DISCONTINUITY_FRAME_SKIPPED)
    {
        return EncoderNotSupported;
    }

    if ((Discontinuity != STM_SE_DISCONTINUITY_CONTINUOUS) &&
        !(Discontinuity & STM_SE_DISCONTINUITY_DISCONTINUOUS) &&
        !(Discontinuity & STM_SE_DISCONTINUITY_EOS) &&
        !(Discontinuity & STM_SE_DISCONTINUITY_CLOSED_GOP_REQUEST) &&
        !(Discontinuity & STM_SE_DISCONTINUITY_MUTE) &&
        !(Discontinuity & STM_SE_DISCONTINUITY_FADEOUT) &&
        !(Discontinuity & STM_SE_DISCONTINUITY_FADEIN))
    {
        return EncoderNotSupported;
    }

    // GOP not supported in audio
    // PreprocBase does not have indication of audio/video, thus unable to check unless at leaf class
    return EncoderNoError;
}

PreprocStatus_t Preproc_Base_c::InjectDiscontinuity(stm_se_discontinuity_t  Discontinuity)
{
    PreprocStatus_t    Status;

    Encoder.EncodeStream->EncodeStreamStatistics().DiscontinuityBufferCountToPreproc++;

    Status = CheckDiscontinuity(Discontinuity);

    if (Status != EncoderNoError)
    {
        return Status;
    }

    return EncoderNoError;
}

PreprocStatus_t Preproc_Base_c::DetectBufferDiscontinuity(Buffer_t   Buffer)
{
    PreprocStatus_t                        Status;
    unsigned int                           BufferAddr;

    // Obtain MetaData from Input.
    // Retrieve input metadata
    // This operation is extra

    stm_se_uncompressed_frame_metadata_t *LocalInputMetaDataDescriptor;
    Buffer->ObtainMetaDataReference(InputMetaDataBufferType, (void **)(&LocalInputMetaDataDescriptor));
    SE_ASSERT(LocalInputMetaDataDescriptor != NULL);

    // Obtain data reference from Input
    // In case of discontinuity (InputBufferSize == 0), ObtainDataReference() might return null buffer address
    InputBufferSize = 0;
    Buffer->ObtainDataReference(NULL, &InputBufferSize, (void **)(&BufferAddr));
    if ((BufferAddr == NULL) && (InputBufferSize != 0))
    {
        SE_ERROR("Unable to obtain input data, InputBufferSize = %u\n", InputBufferSize);
        return PreprocError;
    }

    // Check the discontinuity
    Status = CheckDiscontinuity(LocalInputMetaDataDescriptor->discontinuity);
    if (Status != EncoderNoError)
    {
        SE_ERROR("Check discontinuity failed, Status %u\n", Status);
        return Status;
    }

    // Check for invalid discontinuity cases and warn
    // Invalid Case 1: No discontinuity metadata + zero datasize
    if ((LocalInputMetaDataDescriptor->discontinuity == STM_SE_DISCONTINUITY_CONTINUOUS) && (InputBufferSize == 0))
    {
        SE_ERROR("Invalid input buffer (null buffer with no discontinuity)\n");
        return PreprocError;
    }

    // Store and mask out the discontinuity
    PreprocDiscontinuity                        = LocalInputMetaDataDescriptor->discontinuity;
    LocalInputMetaDataDescriptor->discontinuity = STM_SE_DISCONTINUITY_CONTINUOUS;

    return PreprocNoError;
}

PreprocStatus_t Preproc_Base_c::GenerateBufferDiscontinuity(stm_se_discontinuity_t Discontinuity)
{
    Buffer_t                               Buffer;
    stm_se_uncompressed_frame_metadata_t *LocalPreprocMetaDataDescriptor;
    __stm_se_frame_metadata_t             *LocalPreprocFullMetaDataDescriptor;

    // Get a new discontinuity buffer
    PreprocStatus_t Status = GetNewBuffer(&Buffer, true);
    if (Status != PreprocNoError)
    {
        PREPROC_ERROR_RUNNING("Unable to get new buffer, Status = %u\n", Status);
        return PreprocError;
    }

    // Retrieve preproc metadata
    Buffer->ObtainMetaDataReference(OutputMetaDataBufferType, (void **)(&LocalPreprocFullMetaDataDescriptor));
    SE_ASSERT(LocalPreprocFullMetaDataDescriptor != NULL);

    LocalPreprocMetaDataDescriptor = &LocalPreprocFullMetaDataDescriptor->uncompressed_frame_metadata;
    // Initialize preproc metadata with discontinuity EOS
    memset(LocalPreprocFullMetaDataDescriptor, '\0', sizeof(__stm_se_frame_metadata_t));
    LocalPreprocMetaDataDescriptor->discontinuity = Discontinuity;

    Encoder.EncodeStream->EncodeStreamStatistics().DiscontinuityBufferCountFromPreproc++;

    // Output frame into ring
    return Output(Buffer);
}

PreprocStatus_t  Preproc_Base_c::LowPowerEnter(void)
{
    return PreprocNoError;
}

PreprocStatus_t  Preproc_Base_c::LowPowerExit(void)
{
    return PreprocNoError;
}

void Preproc_Base_c::DumpInputMetadata(stm_se_uncompressed_frame_metadata_t *Metadata)
{
    int i = 0;

    if (SE_IS_EXTRAVERB_ON(GetGroupTrace()))
    {
        SE_EXTRAVERB(GetGroupTrace(), "[INPUT PREPROC METADATA]\n");
        SE_EXTRAVERB(GetGroupTrace(), "|-system_time = %llu\n", Metadata->system_time);
        SE_EXTRAVERB(GetGroupTrace(), "|-native_time_format = %u (%s)\n", Metadata->native_time_format, StringifyTimeFormat(Metadata->native_time_format));
        SE_EXTRAVERB(GetGroupTrace(), "|-native_time = %llu\n", Metadata->native_time);
        SE_EXTRAVERB(GetGroupTrace(), "|-user_data_size = %u bytes\n", Metadata->user_data_size);
        SE_EXTRAVERB(GetGroupTrace(), "|-user_data_buffer_address = 0x%p\n", Metadata->user_data_buffer_address);
        SE_EXTRAVERB(GetGroupTrace(), "|-discontinuity = %u (%s)\n", Metadata->discontinuity, StringifyDiscontinuity(Metadata->discontinuity));
        SE_EXTRAVERB(GetGroupTrace(), "|-media = %u (%s)\n", Metadata->media, StringifyMedia(Metadata->media));
        if (Metadata->media == STM_SE_ENCODE_STREAM_MEDIA_AUDIO)
        {
            SE_EXTRAVERB(GetGroupTrace(), "|-[audio]\n");
            SE_EXTRAVERB(GetGroupTrace(), "  |-[core_format]\n");
            SE_EXTRAVERB(GetGroupTrace(), "  | |-sample_rate = %u Hz\n", Metadata->audio.core_format.sample_rate);
            SE_EXTRAVERB(GetGroupTrace(), "  | |-[channel_placement]\n");
            SE_EXTRAVERB(GetGroupTrace(), "  |   |-channel_count = %u\n", Metadata->audio.core_format.channel_placement.channel_count);
            for (i = 0 ; i < Metadata->audio.core_format.channel_placement.channel_count ; i++)
            {
                SE_EXTRAVERB(GetGroupTrace(), "  |   |-chan[%d] = %u\n", i, Metadata->audio.core_format.channel_placement.chan[i]);
            }
            SE_EXTRAVERB(GetGroupTrace(), "  |\n");
            SE_EXTRAVERB(GetGroupTrace(), "  |-sample_format = %u (%s)\n", Metadata->audio.sample_format, StringifyPcmFormat(Metadata->audio.sample_format));
            SE_EXTRAVERB(GetGroupTrace(), "  |-program_level = %d\n", Metadata->audio.program_level);
            SE_EXTRAVERB(GetGroupTrace(), "  |-emphasis = %u (%s)\n", Metadata->audio.emphasis, StringifyEmphasisType(Metadata->audio.emphasis));
        }
        else if (Metadata->media == STM_SE_ENCODE_STREAM_MEDIA_VIDEO)
        {
            SE_EXTRAVERB(GetGroupTrace(), "|-[video]\n");
            SE_EXTRAVERB(GetGroupTrace(), "  |-[video_parameters]\n");
            SE_EXTRAVERB(GetGroupTrace(), "  | |-width = %u pixels\n", Metadata->video.video_parameters.width);
            SE_EXTRAVERB(GetGroupTrace(), "  | |-height = %u pixels\n", Metadata->video.video_parameters.height);
            SE_EXTRAVERB(GetGroupTrace(), "  | |-aspect_ratio = %u (%s)\n", Metadata->video.video_parameters.aspect_ratio, StringifyAspectRatio(Metadata->video.video_parameters.aspect_ratio));
            SE_EXTRAVERB(GetGroupTrace(), "  | |-scan_type = %u (%s)\n", Metadata->video.video_parameters.scan_type, StringifyScanType(Metadata->video.video_parameters.scan_type));
            SE_EXTRAVERB(GetGroupTrace(), "  | |-pixel_aspect_ratio = %u/%u\n", Metadata->video.video_parameters.pixel_aspect_ratio_numerator,
                         Metadata->video.video_parameters.pixel_aspect_ratio_denominator);
            SE_EXTRAVERB(GetGroupTrace(), "  | |-format_3d = %u (%s)\n", Metadata->video.video_parameters.format_3d, StringifyFormat3d(Metadata->video.video_parameters.format_3d));
            SE_EXTRAVERB(GetGroupTrace(), "  | |-left_right_format = %u\n", Metadata->video.video_parameters.left_right_format);
            SE_EXTRAVERB(GetGroupTrace(), "  | |-colorspace = %u (%s)\n", Metadata->video.video_parameters.colorspace, StringifyColorspace(Metadata->video.video_parameters.colorspace));
            SE_EXTRAVERB(GetGroupTrace(), "  | |-frame_rate = %u fps\n", Metadata->video.video_parameters.frame_rate);
            SE_EXTRAVERB(GetGroupTrace(), "  |\n");
            SE_EXTRAVERB(GetGroupTrace(), "  |-[window_of_interest]\n");
            SE_EXTRAVERB(GetGroupTrace(), "  | |-x = %u pixels\n", Metadata->video.window_of_interest.x);
            SE_EXTRAVERB(GetGroupTrace(), "  | |-y = %u pixels\n", Metadata->video.window_of_interest.y);
            SE_EXTRAVERB(GetGroupTrace(), "  | |-width = %u pixels\n", Metadata->video.window_of_interest.width);
            SE_EXTRAVERB(GetGroupTrace(), "  | |-height = %u pixels\n", Metadata->video.window_of_interest.height);
            SE_EXTRAVERB(GetGroupTrace(), "  |\n");
            SE_EXTRAVERB(GetGroupTrace(), "  |-frame_rate = %u/%u fps\n", Metadata->video.frame_rate.framerate_num, Metadata->video.frame_rate.framerate_den);
            SE_EXTRAVERB(GetGroupTrace(), "  |-pitch = %u bytes\n", Metadata->video.pitch);
            SE_EXTRAVERB(GetGroupTrace(), "  |-vertical_alignment = %u lines of pixels\n", Metadata->video.vertical_alignment);
            SE_EXTRAVERB(GetGroupTrace(), "  |-picture_type = %u (%s)\n", Metadata->video.picture_type, StringifyPictureType(Metadata->video.picture_type));
            SE_EXTRAVERB(GetGroupTrace(), "  |-surface_format = %u (%s)\n", Metadata->video.surface_format, StringifySurfaceFormat(Metadata->video.surface_format));
            SE_EXTRAVERB(GetGroupTrace(), "  |-top_field_first = %u\n", Metadata->video.top_field_first);
        }
        SE_EXTRAVERB(GetGroupTrace(), "\n");
    }
}

void Preproc_Base_c::DumpEncodeCoordinatorMetadata(__stm_se_encode_coordinator_metadata_t *Metadata)
{
    if (SE_IS_EXTRAVERB_ON(GetGroupTrace()))
    {
        SE_EXTRAVERB(GetGroupTrace(), "[ENCODE COORDINATOR METADATA]\n");
        SE_EXTRAVERB(GetGroupTrace(), "|-encoded_time_format = %u (%s)\n", Metadata->encoded_time_format, StringifyTimeFormat(Metadata->encoded_time_format));
        SE_EXTRAVERB(GetGroupTrace(), "|-encoded_time = %llu\n", Metadata->encoded_time);
        SE_EXTRAVERB(GetGroupTrace(), "\n");
    }
}
