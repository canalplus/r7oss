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

#include "osinline.h"

#include "allocator_simple.h"
#include "ring_generic.h"
#include "buffer_generic.h"

#undef TRACE_TAG
#define TRACE_TAG "BufferPool_Generic_c"

#define BUFFER_MAX_EXPECTED_WAIT_PERIOD     30000000ULL

// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Constructor function - Initialize our data
//

BufferPool_Generic_c::BufferPool_Generic_c(BufferManager_Generic_t   Manager,
                                           BufferDataDescriptor_t   *Descriptor,
                                           unsigned int              NumberOfBuffers,
                                           unsigned int              Size,
                                           void                     *MemoryPool[3],
                                           void                     *ArrayOfMemoryBlocks[][3],
                                           char                     *DeviceMemoryPartitionName,
                                           bool                      AllowCross64MbBoundary,
                                           unsigned int              MemoryAccessType)
    : mManager(Manager)
    , Next(NULL)
    , PoolLock()
    , mBufDataDescriptor(Descriptor)
    , mNumberOfBuffers(NumberOfBuffers)
    , CountOfBuffers(0)
    , Size(Size)
    , ListOfBuffers(NULL)
    , FreeBuffer(NULL)
    , mMemoryPool()
    , MemoryPoolAllocator(NULL)
    , MemoryPoolAllocatorDevice(NULL)
    , MemoryPartitionName()
    , mMemoryAccessType(MemoryAccessType)
    , BufferBlock(NULL)
    , ListOfMetaData(NULL)
    , AbortGetBuffer(false)
    , BufferReleaseSignalWaitedOn(false)
    , BufferReleaseSignal()
    , CountOfReferencedBuffers(0)
    , TotalAllocatedMemory(0)
    , TotalUsedMemory(0)
{
    SE_DEBUG(group_buffer, "0x%p BufferManager 0x%p NumberOfBuffers %d Size %d "
             "TypeName %s Type %d AllocateOnPoolCreation %d source %d PartitionName %s\n",
             this,
             mManager,
             mNumberOfBuffers,
             Size,
             (mBufDataDescriptor->TypeName == NULL) ? "Unnamed" : mBufDataDescriptor->TypeName,
             mBufDataDescriptor->Type,
             mBufDataDescriptor->AllocateOnPoolCreation,
             mBufDataDescriptor->AllocationSource,
             (DeviceMemoryPartitionName == NULL) ? "Unnamed" : DeviceMemoryPartitionName);

    if (DeviceMemoryPartitionName != NULL)
    {
        strncpy(MemoryPartitionName, DeviceMemoryPartitionName, sizeof(MemoryPartitionName));
        MemoryPartitionName[sizeof(MemoryPartitionName) - 1] = '\0';
    }
    else
    {
        MemoryPartitionName[0] = '\0';
    }

    if (MemoryPool != NULL)
    {
        mMemoryPool[0] = MemoryPool[0];
        mMemoryPool[1] = MemoryPool[1];
        mMemoryPool[2] = MemoryPool[2];
    }

    OS_InitializeMutex(&PoolLock);
    OS_InitializeEvent(&BufferReleaseSignal);

    // TODO(pht) move FinalizeInit to a factory method and get rid of TidyUp() (see TidyUp() comment)
    InitializationStatus = FinalizeInit(MemoryPool, ArrayOfMemoryBlocks, AllowCross64MbBoundary);
}

BufferStatus_t BufferPool_Generic_c::FinalizeInit(void  *MemoryPool[3],
                                                  void  *ArrayOfMemoryBlocks[][3],
                                                  bool   AllowCross64MbBoundary)
{
    unsigned int      i, j;
    BufferStatus_t    Status;
    Buffer_Generic_t  Buffer;
    unsigned int      ItemSize;

    //
    // Shall we create the buffer class instances
    //

    if (mNumberOfBuffers != NOT_SPECIFIED)
    {
        //
        // Get a ring to hold the free buffers
        //
        FreeBuffer = new RingGeneric_c(mNumberOfBuffers, mBufDataDescriptor->TypeName);
        if ((FreeBuffer == NULL) || (FreeBuffer->InitializationStatus != RingNoError))
        {
            SE_ERROR("0x%p Failed to create free buffer ring\n", this);
            TidyUp();
            return BufferError;
        }

        //
        // Can we allocate the memory for the buffers
        //

        if (mBufDataDescriptor->AllocateOnPoolCreation)
        {
            Status = CheckMemoryParameters(mBufDataDescriptor, true, Size, MemoryPool, ArrayOfMemoryBlocks, MemoryPartitionName,
                                           "FinalizeInit", &ItemSize);
            if (Status != BufferNoError)
            {
                SE_ERROR("0x%p CheckMemoryParameters failed\n", this);
                TidyUp();
                return BufferError;
            }

            //
            // Create a buffer block descriptor record
            //
            BufferBlock = new struct BlockDescriptor_s(NULL, mBufDataDescriptor, true,
                                                       ItemSize * mNumberOfBuffers, NULL);
            if (BufferBlock == NULL)
            {
                SE_ERROR("0x%p Failed to allocate block descriptor\n", this);
                TidyUp();
                return BufferError;
            }

            Status = AllocateMemoryBlock(BufferBlock, true, 0, NULL, MemoryPool,
                                         ArrayOfMemoryBlocks, MemoryPartitionName,
                                         "FinalizeInit", false, mMemoryAccessType);
            if (Status != BufferNoError)
            {
                SE_ERROR("0x%p Failed to allocate memory block\n", this);
                TidyUp();
                return BufferError;
            }
        }

        //
        // Now create the buffers
        //

        for (i = 0; i < mNumberOfBuffers; i++)
        {
            Buffer = new Buffer_Generic_c(mManager, this, mBufDataDescriptor);
            if ((Buffer == NULL) || (Buffer->InitializationStatus != BufferNoError))
            {
                InitializationStatus = BufferInsufficientMemoryForBuffer;

                if (Buffer != NULL)
                {
                    InitializationStatus = Buffer->InitializationStatus;
                    delete Buffer;
                }

                SE_ERROR("0x%p Failed to create buffer (%08x)\n", this, InitializationStatus);
                TidyUp();
                return BufferError;
            }

            Buffer->Next        = ListOfBuffers;
            Buffer->Index       = i;
            ListOfBuffers       = Buffer;
            FreeBuffer->Insert((uintptr_t)Buffer);

            //
            // Have we allocated the buffer data block
            //

            if (mBufDataDescriptor->AllocateOnPoolCreation)
            {
                Buffer->DataSize                                = 0;
                Buffer->BufferBlock->AttachedToPool             = true;
                Buffer->BufferBlock->Size                       = ItemSize;
                Buffer->BufferBlock->MemoryAllocatorDevice      = NULL;
                Buffer->BufferBlock->Address[CachedAddress]     = NULL;
                Buffer->BufferBlock->Address[PhysicalAddress]   = NULL;

                if (mBufDataDescriptor->AllocationSource == AllocateIndividualSuppliedBlocks)
                {
                    for (j = 0; j < 3; j++)
                    {
                        Buffer->BufferBlock->Address[j]         = ArrayOfMemoryBlocks[i][j];
                    }
                }
                else
                {
                    for (j = 0; j < 3; j++)
                        if (BufferBlock->Address[j] != NULL)
                        {
                            Buffer->BufferBlock->Address[j]     = (unsigned char *)BufferBlock->Address[j] + (i * ItemSize);
                        }
                }
            }
        }
    }

    //
    // If we have pool memory, and we have not used it already, then we need to initialize the allocation mechanism.
    //

    if ((MemoryPool != NULL) && !mBufDataDescriptor->AllocateOnPoolCreation)
    {
        MemoryPoolAllocator = new AllocatorSimple_c(Size, 1, (unsigned char *)MemoryPool[PhysicalAddress], AllowCross64MbBoundary);
        if ((MemoryPoolAllocator == NULL) || (MemoryPoolAllocator->InitializationStatus != AllocatorNoError))
        {
            SE_ERROR("0x%p Failed to initialize MemoryPool allocator\n", this);
            TidyUp();
            return BufferError;
        }
    }

    return BufferNoError;
}

// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Destructor function - close down
//

BufferPool_Generic_c::~BufferPool_Generic_c(void)
{
    SE_DEBUG(group_buffer, "0x%p\n", this);

    TidyUp();
    OS_TerminateMutex(&PoolLock);
    OS_TerminateEvent(&BufferReleaseSignal);
}


// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Attach meta data structures to every element of the pool
//

BufferStatus_t   BufferPool_Generic_c::AttachMetaData(
    MetaDataType_t    Type,
    unsigned int      Size,
    void             *MemoryPool,
    void             *ArrayOfMemoryBlocks[],
    char             *DeviceMemoryPartitionName)
{
    BufferStatus_t           Status;
    unsigned int             ItemSize;
    BufferDataDescriptor_t  *Descriptor;
    BlockDescriptor_t        Block;
    BlockDescriptor_t        SubBlock;
    Buffer_Generic_t         Buffer;

    //
    // Get the descriptor
    //
    Status      = mManager->GetDescriptor(Type, MetaDataTypeBase, &Descriptor);
    if (Status != BufferNoError)
    {
        return Status;
    }

    //
    // Check the parameters and associated information to see if we can do this
    //
    Status = CheckMemoryParameters(Descriptor, true, Size, MemoryPool, ArrayOfMemoryBlocks, DeviceMemoryPartitionName,
                                   "AttachMetaData", &ItemSize);
    if (Status != BufferNoError)
    {
        return Status;
    }

    //
    // Create a new block descriptor record
    //
    Block = new struct BlockDescriptor_s(NULL, Descriptor, true, ItemSize * mNumberOfBuffers, NULL);
    if (Block == NULL)
    {
        SE_ERROR("0x%p Unable to create a block descriptor record\n", this);
        return BufferInsufficientMemoryForMetaData;
    }

    Status = AllocateMemoryBlock(Block, true, 0, NULL, MemoryPool, ArrayOfMemoryBlocks, DeviceMemoryPartitionName,
                                 "AttachMetaData");
    if (Status != BufferNoError)
    {
        delete Block;
        return Status;
    }

    OS_LockMutex(&PoolLock);

    //
    // Check to see if it is already attached.  We must do this in the critical
    // section that appends the new metadata for avoiding a race between two
    // threads registering the same metadata type.
    //

    for (BlockDescriptor_t ExistingBlock = ListOfMetaData;
         ExistingBlock != NULL;
         ExistingBlock = ExistingBlock->Next)
    {
        if (ExistingBlock->Descriptor->Type == Type)
        {
            // TODO(pht) check if size changed => then detach / reattach metadata
            OS_UnLockMutex(&PoolLock);
            SE_INFO(group_buffer, "0x%p Meta data already attached\n", this);
            DeAllocateMemoryBlock(Block);
            delete Block;
            return BufferNoError;
        }
    }

    Block->Next      = ListOfMetaData;
    ListOfMetaData   = Block;

    //
    // Now loop assigning values to each buffer
    // TODO(theryn): Block and SubBlock for previous buffers in list leaked on
    // error.
    //

    for (Buffer  = ListOfBuffers;
         Buffer != NULL;
         Buffer  = Buffer->Next)
    {
        OS_LockMutex(&Buffer->BufferLock);

        SubBlock = new struct BlockDescriptor_s(Buffer->ListOfMetaData, Descriptor, true, ItemSize, NULL);
        if (SubBlock == NULL)
        {
            OS_UnLockMutex(&Buffer->BufferLock);
            OS_UnLockMutex(&PoolLock);
            SE_ERROR("0x%p Unable to create a block descriptor record\n", this);
            return BufferInsufficientMemoryForMetaData;
        }

        if (Descriptor->AllocationSource == AllocateIndividualSuppliedBlocks)
        {
            SubBlock->Address[CachedAddress] = ArrayOfMemoryBlocks[Buffer->Index];
        }
        else
        {
            SubBlock->Address[CachedAddress] = (unsigned char *)Block->Address[CachedAddress] + (Buffer->Index * ItemSize);
        }

        Buffer->ListOfMetaData = SubBlock;

        OS_UnLockMutex(&Buffer->BufferLock);
    }

    OS_UnLockMutex(&PoolLock);

    return BufferNoError;
}


// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Detach meta data from every element of the pool
//

void BufferPool_Generic_c::DetachMetaData(MetaDataType_t Type)
{
    BlockDescriptor_t  *LocationOfBlockPointer;
    BlockDescriptor_t   Block;
    BlockDescriptor_t   SubBlock;
    Buffer_Generic_t    Buffer;

    SE_VERBOSE(group_buffer, "0x%p Type %d\n", this, Type);

    //
    // First find the descriptor block in the pool
    //
    OS_LockMutex(&PoolLock);

    for (LocationOfBlockPointer   = &ListOfMetaData;
         *LocationOfBlockPointer != NULL;
         LocationOfBlockPointer   = &((*LocationOfBlockPointer)->Next))
        if ((*LocationOfBlockPointer)->Descriptor->Type == Type)
        {
            break;
        }

    if (*LocationOfBlockPointer == NULL)
    {
        OS_UnLockMutex(&PoolLock);
        SE_ERROR("0x%p No meta data of the specified type is attached to the buffer pool\n", this);
        return;
    }

    //
    // Get a local block pointer, and unthread the block from the list
    //
    Block                       = *LocationOfBlockPointer;
    *LocationOfBlockPointer     = Block->Next;

    //
    // For each buffer, find a block describing this type, unthread it, and delete the block record
    //

    for (Buffer  = ListOfBuffers;
         Buffer != NULL;
         Buffer  = Buffer->Next)
    {
        OS_LockMutex(&Buffer->BufferLock);

        if (Buffer->ReferenceCount != 0)
        {
            SE_ERROR("0x%p Detaching meta data (%s) from a live buffer (%s) - Implementation error\n",
                     this, mManager->TypeDescriptors[Type & TYPE_INDEX_MASK].TypeName, mBufDataDescriptor->TypeName);
        }

        for (LocationOfBlockPointer       = &Buffer->ListOfMetaData;
             *LocationOfBlockPointer     != NULL;
             LocationOfBlockPointer       = &((*LocationOfBlockPointer)->Next))
            if ((*LocationOfBlockPointer)->Descriptor->Type == Type)
            {
                break;
            }

        if (((*LocationOfBlockPointer) == NULL) || !(*LocationOfBlockPointer)->AttachedToPool)
        {
            OS_UnLockMutex(&Buffer->BufferLock);
            SE_ERROR("0x%p Meta data record missing from buffer, system inconsistent\n", this);
            continue;
        }

        SubBlock                = *LocationOfBlockPointer;
        *LocationOfBlockPointer = SubBlock->Next;
        delete SubBlock;

        OS_UnLockMutex(&Buffer->BufferLock);
    }

    OS_UnLockMutex(&PoolLock);

    //
    // Free up the memory, and delete the block record.
    //
    DeAllocateMemoryBlock(Block);
    delete Block;
}


// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Obtain a buffer from the pool
//

BufferStatus_t   BufferPool_Generic_c::GetBuffer(
    Buffer_t         *Buffer,
    unsigned int      OwnerIdentifier,
    unsigned int      RequiredSize,
    bool              NonBlocking,
    bool              RequiredSizeIsLowerBound,
    uint32_t          TimeOut
)
{
    unsigned int         i;
    BufferStatus_t       Status;
    RingStatus_t         RingStatus;
    unsigned int         ItemSize;
    Buffer_Generic_t     LocalBuffer;
    unsigned long long   EntryTime;
    unsigned long long   EntryTimeForTimeOut;

    SE_VERBOSE(group_buffer, "0x%p OwnerIdentifier %d RequiredSize %d NonBlocking %d\n",
               this, OwnerIdentifier, RequiredSize, NonBlocking);
    //
    // Initialize the input parameters, and clear the abort flag
    //
    *Buffer             = NULL;
    AbortGetBuffer      = false;

    //
    // Perform simple parameter checks.
    //

    if (!mBufDataDescriptor->AllocateOnPoolCreation && (mBufDataDescriptor->AllocationSource != NoAllocation))
    {
        Status = CheckMemoryParameters(mBufDataDescriptor, false, RequiredSize, mMemoryPool[0], NULL, MemoryPartitionName,
                                       "GetBuffer", &ItemSize);

        if (Status != BufferNoError)
        {
            return Status;
        }
    }

    //
    // Get a buffer - two different paths depending on whether or not there are a fixed number of buffers
    //

    OS_LockMutex(&PoolLock);

    if (mNumberOfBuffers == NOT_SPECIFIED)
    {
        LocalBuffer = new Buffer_Generic_c(mManager, this, mBufDataDescriptor);
        if ((LocalBuffer == NULL) || (LocalBuffer->InitializationStatus != BufferNoError))
        {
            OS_UnLockMutex(&PoolLock);
            delete LocalBuffer;
            SE_ERROR("0x%p Failed to create buffer\n", this);
            return BufferFailedToCreateBuffer;
        }

        LocalBuffer->Next       = ListOfBuffers;
        LocalBuffer->Index      = CountOfBuffers++;
        ListOfBuffers           = LocalBuffer;
    }
    else
    {
        EntryTime       = OS_GetTimeInMicroSeconds();
        EntryTimeForTimeOut = EntryTime;

        do
        {
            OS_ResetEvent(&BufferReleaseSignal);
            RingStatus  = FreeBuffer->Extract((uintptr_t *)(&LocalBuffer), RING_NONE_BLOCKING);

            /* OS_Status_t WaitStatus = OS_NO_ERROR; */
            if (!NonBlocking && !AbortGetBuffer && (RingStatus != RingNoError))
            {
                BufferReleaseSignalWaitedOn     = true;
                SE_DEBUG(group_buffer, "0x%p No buffer of type %04x - '%s' available start waiting\n",
                         this, mBufDataDescriptor->Type,
                         (mBufDataDescriptor->TypeName == NULL) ? "Unnamed" : mBufDataDescriptor->TypeName);
                OS_UnLockMutex(&PoolLock);
                /* WaitStatus = */ OS_WaitForEventAuto(&BufferReleaseSignal, BUFFER_MAXIMUM_EVENT_WAIT);
                OS_LockMutex(&PoolLock);
                BufferReleaseSignalWaitedOn = false;
            }

#if 0 // to be activated next -- TODO(pht)
            if (WaitStatus == OS_INTERRUPTED)
            {
                // in this case: handle interrupt
                SE_INFO(group_buffer, "wait for buffer release signal was interrupted\n");
                NonBlocking = true;   //will cause exit from loop
            }
#endif

            /* Evaluates the TimeOut exit condition */
            if (TimeOut && ((OS_GetTimeInMicroSeconds() - EntryTimeForTimeOut) > ((unsigned long long)TimeOut * 1000)))
            {
                SE_DEBUG(group_buffer, "0x%p GetBuffer of type %04x - '%s' Timed Out after %d ms\n",
                         this, mBufDataDescriptor->Type,
                         (mBufDataDescriptor->TypeName == NULL) ? "Unnamed" : mBufDataDescriptor->TypeName,
                         TimeOut);
                NonBlocking = true;   //will cause exit from loop
            }

            /* If blocked for a very long time, display warning message periodically */
            if ((OS_GetTimeInMicroSeconds() - EntryTime) > BUFFER_MAX_EXPECTED_WAIT_PERIOD)
            {
                SE_WARNING("0x%p Waiting for a buffer of type %04x - '%s'\n", this, mBufDataDescriptor->Type,
                           (mBufDataDescriptor->TypeName == NULL) ? "Unnamed" : mBufDataDescriptor->TypeName);

                if (SE_IS_DEBUG_ON(group_buffer))
                {
                    DumpLocked(DumpPoolStates | DumpBufferStates);
                }

                EntryTime   = OS_GetTimeInMicroSeconds();
            }
        }
        while (!NonBlocking && !AbortGetBuffer && (RingStatus != RingNoError));

        if (RingStatus != RingNoError)
        {
            SE_WARNING("0x%p BufferNoFreeBufferAvailable of type %04x - '%s'\n", this, mBufDataDescriptor->Type,
                       (mBufDataDescriptor->TypeName == NULL) ? "Unnamed" : mBufDataDescriptor->TypeName);
            OS_UnLockMutex(&PoolLock);
            return BufferNoFreeBufferAvailable;
        }
    }

    //
    // Deal with the memory
    //

    if (!mBufDataDescriptor->AllocateOnPoolCreation && (mBufDataDescriptor->AllocationSource != NoAllocation))
    {
        LocalBuffer->BufferBlock->AttachedToPool        = true;
        LocalBuffer->BufferBlock->Size                  = ItemSize;

        // We must release PoolLock here or, on out of memory, we will deadlock
        // with threads releasing memory we are waiting for.  This is safe
        // because LocalBuffer is not shared with anyone yet and so cannot be
        // changed under our feet.
        OS_UnLockMutex(&PoolLock);

        Status                                          = AllocateMemoryBlock(LocalBuffer->BufferBlock, false, 0, MemoryPoolAllocator,
                                                                              mMemoryPool, NULL,  MemoryPartitionName,
                                                                              "GetBuffer",
                                                                              RequiredSizeIsLowerBound, mMemoryAccessType, NonBlocking);
        if (Status != BufferNoError)
        {
            SE_ERROR("0x%p error during AllocateMemoryBlock Status %d\n", this, Status);
            return Status;
        }

        OS_LockMutex(&PoolLock);
    }

    LocalBuffer->DataSize               = 0;
    //
    // Record the owner identifier
    //
    OS_LockMutex(&LocalBuffer->BufferLock);

    LocalBuffer->OwnerIdentifier[0]     = OwnerIdentifier;
    LocalBuffer->ReferenceCount         = 1;

    for (i = 1; i < MAX_BUFFER_OWNER_IDENTIFIERS; i++)
    {
        LocalBuffer->OwnerIdentifier[i] = UNSPECIFIED_OWNER;
    }

    //
    // Initialize Attached buffers
    //

    for (i = 0; i < MAX_ATTACHED_BUFFERS; i++)
    {
        LocalBuffer->AttachedBuffers[i] = NULL;
    }

    OS_UnLockMutex(&LocalBuffer->BufferLock);

    //
    // Increment the global reference count
    //
    CountOfReferencedBuffers++;

    OS_UnLockMutex(&PoolLock);

    //
    // Set the return value
    //
    *Buffer     = LocalBuffer;
    return BufferNoError;
}


// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Abort any ongoing get buffer command
//

void BufferPool_Generic_c::AbortBlockingGetBuffer(void)
{
    SE_DEBUG(group_buffer, "0x%p\n", this);
    AbortGetBuffer = true;
    OS_SetEvent(&BufferReleaseSignal);
}


// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Set the Memory type access for buffer inside a pool
//
void BufferPool_Generic_c::SetMemoryAccessType(unsigned int MemoryAccessType)
{
    mMemoryAccessType = MemoryAccessType;
}

// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Release a buffer to the pool
//

void BufferPool_Generic_c::ReleaseBuffer(Buffer_t Buffer)
{
    unsigned int             i;
    Buffer_Generic_t         LocalBuffer;
    Buffer_Generic_t        *LocationOfBufferPointer;
    BlockDescriptor_t       *LocationOfBlockPointer;
    BlockDescriptor_t        Block;

    SE_VERBOSE(group_buffer, "0x%p Buffer 0x%p\n", this, Buffer);

    LocalBuffer = (Buffer_Generic_t)Buffer;

    //
    // Release our hold on any attached buffers.
    // We must do this without holding PoolLock in case we are recursively
    // called when releasing an attached buffer.
    //
    OS_AssertMutexNotHeld(&PoolLock);
    OS_LockMutex(&LocalBuffer->BufferLock);

    for (i = 0; i < MAX_ATTACHED_BUFFERS; i++)
    {
        Buffer_t  Temporary = LocalBuffer->AttachedBuffers[i];
        if (Temporary != NULL)
        {
            LocalBuffer->AttachedBuffers[i] = NULL;
            Temporary->DecrementReferenceCount();
        }
    }

    OS_UnLockMutex(&LocalBuffer->BufferLock);

    //
    // Release any non-persistent meta data
    //
    OS_LockMutex(&PoolLock);

    LocationOfBlockPointer = &LocalBuffer->ListOfMetaData;

    while (*LocationOfBlockPointer != NULL)
    {
        if (!((*LocationOfBlockPointer)->AttachedToPool))
        {
            //
            // Unthread the meta data item block
            //
            Block                       = *LocationOfBlockPointer;
            *LocationOfBlockPointer     = Block->Next;
            DeAllocateMemoryBlock(Block);
            delete Block;
        }
        else
        {
            LocationOfBlockPointer      = &((*LocationOfBlockPointer)->Next);
        }
    }

    //
    // If non-persistent delete the memory associated with the block
    //

    if (!mBufDataDescriptor->AllocateOnPoolCreation && (mBufDataDescriptor->AllocationSource != NoAllocation))
    {
        DeAllocateMemoryBlock(LocalBuffer->BufferBlock);
    }

    TotalUsedMemory       -= LocalBuffer->DataSize;
    LocalBuffer->DataSize  = 0;

    //
    // If there are a fixed number of buffers insert this on the ring,
    // else unthread from list and delete the buffer
    //

    if (mNumberOfBuffers != NOT_SPECIFIED)
    {
        FreeBuffer->Insert((uintptr_t)LocalBuffer);
        OS_SetEvent(&BufferReleaseSignal);
    }
    else
    {
        for (LocationOfBufferPointer     = &ListOfBuffers;
             *LocationOfBufferPointer   != NULL;
             LocationOfBufferPointer     = &((*LocationOfBufferPointer)->Next))
        {
            if ((*LocationOfBufferPointer) == LocalBuffer)
            {
                break;
            }
        }

        if (*LocationOfBufferPointer == NULL)
        {
            OS_UnLockMutex(&PoolLock);
            SE_ERROR("0x%p Buffer not found in list, internal inconsistency\n", this);
            return;
        }

        *LocationOfBufferPointer        = LocalBuffer->Next;
        delete LocalBuffer;
        CountOfBuffers--;
    }

    CountOfReferencedBuffers--;

    OS_UnLockMutex(&PoolLock);
}


// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Discover the type of buffer supported by the pool
//

void BufferPool_Generic_c::GetType(BufferType_t *Type)
{
    *Type = mBufDataDescriptor->Type;
}


// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Discover usage statistics about the pool
//

void BufferPool_Generic_c::GetPoolUsage(
    unsigned int  *BuffersInPool,
    unsigned int  *BuffersWithNonZeroReferenceCount,
    unsigned int  *MemoryInPool,
    unsigned int  *MemoryAllocated,
    unsigned int  *MemoryInUse,
    unsigned int  *LargestFreeMemoryBlock)
{
    unsigned int   TotalMemory;
    unsigned int   LargestFreeBlock;

    switch (mBufDataDescriptor->AllocationSource)
    {
    case AllocateFromSuppliedBlock:
        TotalMemory     = Size;

        if (MemoryPoolAllocator == NULL)
        {
            LargestFreeBlock  = Size;
        }
        else
        {
            MemoryPoolAllocator->LargestFreeBlock(&LargestFreeBlock);
        }

        break;

    case AllocateIndividualSuppliedBlocks:
        TotalMemory     = Size * mNumberOfBuffers;
        LargestFreeBlock    = (((mNumberOfBuffers == NOT_SPECIFIED) ? CountOfBuffers : mNumberOfBuffers)
                               != CountOfReferencedBuffers) ? Size : 0;
        break;

    case NoAllocation:
    case AllocateFromOSMemory:
    case AllocateFromNamedDeviceMemory:
    default:
        TotalMemory      = NOT_SPECIFIED;
        LargestFreeBlock = NOT_SPECIFIED;
        break;
    }

    if (BuffersInPool != NULL)
    {
        *BuffersInPool    = (mNumberOfBuffers == NOT_SPECIFIED) ? CountOfBuffers : mNumberOfBuffers;
    }

    if (BuffersWithNonZeroReferenceCount != NULL)
    {
        *BuffersWithNonZeroReferenceCount       = CountOfReferencedBuffers;
    }

    if (MemoryInPool != NULL)
    {
        *MemoryInPool     = TotalMemory;
    }

    if (MemoryAllocated != NULL)
    {
        *MemoryAllocated  = TotalAllocatedMemory;
    }

    if (MemoryInUse != NULL)
    {
        *MemoryInUse      = TotalUsedMemory;
    }

    if (LargestFreeMemoryBlock != NULL)
    {
        *LargestFreeMemoryBlock = LargestFreeBlock;
    }

    SE_VERBOSE(group_buffer, "0x%p BuffersInPool %d CountOfReferencedBuffers %d MemoryInPool %d "
               "MemoryAllocated %d MemoryInUse %d LargestFreeMemoryBlock %d\n",
               this, (mNumberOfBuffers == NOT_SPECIFIED) ? CountOfBuffers : mNumberOfBuffers, CountOfReferencedBuffers,
               TotalMemory, TotalAllocatedMemory, TotalUsedMemory, LargestFreeBlock);
}


// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Obtain a reference to all buffers that are currently allocated (a debug/reporting aid)
//  NOTE since we are returning buffers, we do take a reference for each one,
//  it is essential that the code using this function releases them.

BufferStatus_t   BufferPool_Generic_c::GetAllUsedBuffers(
    unsigned int      ArraySize,
    Buffer_t         *ArrayOfBuffers,
    unsigned int      OwnerIdentifier)
{
    unsigned int Count = 0;

    SE_VERBOSE(group_buffer, "0x%p OwnerIdentifier %d\n", this, OwnerIdentifier);

    OS_LockMutex(&PoolLock);

    Buffer_Generic_t Buffer;
    for (Buffer  = ListOfBuffers;
         Buffer != NULL;
         Buffer  = Buffer->Next)
    {
        OS_LockMutex(&Buffer->BufferLock);
        if (Buffer->ReferenceCount != 0)
        {
            if (Count >= ArraySize)
            {
                OS_UnLockMutex(&Buffer->BufferLock);
                OS_UnLockMutex(&PoolLock);
                SE_ERROR("Too many used buffers for output array\n");
                return BufferError;
            }

            Buffer->ReferenceCount++;
            ArrayOfBuffers[Count++]     = Buffer;
        }
        OS_UnLockMutex(&Buffer->BufferLock);
    }

    OS_UnLockMutex(&PoolLock);

    /* make sure to zero out the remaining entries in ArrayOfBuffers[] if we
       pushed fewer items than space is available. */
    if (Count < ArraySize)
    {
        unsigned int Remaining = ArraySize - Count;
        memset(&ArrayOfBuffers[Count], 0, Remaining * sizeof(ArrayOfBuffers[0]));
    }

    return BufferNoError;
}


// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Dump the status of the pool
//

void   BufferPool_Generic_c::Dump(unsigned int      Flags)
{
    OS_LockMutex(&PoolLock);
    DumpLocked(Flags);
    OS_UnLockMutex(&PoolLock);
}

// Dump pool state to kernel log.
// Caller must hold pool lock.
void   BufferPool_Generic_c::DumpLocked(unsigned int      Flags)
{
    unsigned int            BuffersWithNonZeroReferenceCount;
    unsigned int            MemoryInPool;
    unsigned int            MemoryAllocated;
    unsigned int            MemoryInUse;
    BlockDescriptor_t       MetaData;
    Buffer_Generic_t        Buffer;

    OS_AssertMutexHeld(&PoolLock);

    if ((Flags & DumpPoolStates) != 0)
    {
        SE_DEBUG(group_buffer, "0x%p Type %04x - '%s'\n", this, mBufDataDescriptor->Type,
                 (mBufDataDescriptor->TypeName == NULL) ? "Unnamed" : mBufDataDescriptor->TypeName);
        GetPoolUsage(NULL, &BuffersWithNonZeroReferenceCount, &MemoryInPool, &MemoryAllocated, &MemoryInUse);

        if (mNumberOfBuffers == NOT_SPECIFIED)
        {
            SE_DEBUG(group_buffer, "0x%p Dynamic buffer allocation - currently there are %d buffers\n", this, CountOfBuffers);
        }
        else
        {
            SE_DEBUG(group_buffer, "0x%p Fixed buffer allocation - %d buffers, of which %d are allocated\n",
                     this, mNumberOfBuffers, BuffersWithNonZeroReferenceCount);
        }

        SE_DEBUG(group_buffer, "0x%p There is %08x memory available to the pool (0 => Unlimited), %08x is allocated, and %08x is actually used\n",
                 this, MemoryInPool, MemoryAllocated, MemoryInUse);
        SE_DEBUG(group_buffer, "0x%p %s attached to every element of pool\n",
                 this, (ListOfMetaData == NULL) ? "There are no Meta data items" : "The following meta data items are");

        for (MetaData  = ListOfMetaData;
             MetaData != NULL;
             MetaData  = MetaData->Next)
            SE_DEBUG(group_buffer, "0x%p MetaData %04x - %s\n", this, MetaData->Descriptor->Type,
                     (MetaData->Descriptor->TypeName == NULL) ? "Unnamed" : MetaData->Descriptor->TypeName);
    }

    //

    if ((Flags & DumpBufferStates) != 0)
    {
        SE_DEBUG(group_buffer, "0x%p Dump of Buffers\n", this);

        for (Buffer  = ListOfBuffers;
             Buffer != NULL;
             Buffer  = Buffer->Next)
        {
            OS_LockMutex(&Buffer->BufferLock);
            Buffer->DumpLocked(Flags);
            OS_UnLockMutex(&Buffer->BufferLock);
        }
    }
}

// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Private - Tidy up all data function
//

// TODO(theryn): This function must be called only at destruction time.  It
// exists solely for error handling during FinalizeInit().  Inline this function
// inside the destructor and remove calls to it from FinalizeInit() when
// FinalizeInit() is moved out of the constructor and pool construction is
// wrapped in a factory function.
void   BufferPool_Generic_c::TidyUp(void)
{
    SE_DEBUG(group_buffer, "0x%p\n", this);
    Buffer_Generic_t        Buffer;

    //
    // Ensure no-one is waiting on anything.
    //

    if (BufferReleaseSignalWaitedOn)
    {
        AbortBlockingGetBuffer();
    }

    //
    // detach any globally attached meta data
    //

    while (ListOfMetaData != NULL)
    {
        DetachMetaData(ListOfMetaData->Descriptor->Type);
    }

    //
    // Before deleting buffers in this pool, we must forcibly detach them
    // from buffers in other pools.  Failing this:
    //
    // - These other buffers will have references to deleted objects.
    //
    // - Buffers will be deleted without their reference count dropping to zero
    // thus without proper cleanup and thus leaking per-buffer resources.
    //
    // Ripping out references to buffers from under the feet of their users is
    // dangerous.  This is safe if and only if:
    //
    // - The buffers are kept alive only by being attached to other buffers.
    // There is no other reference to them.
    //
    // - Nothing will ever call methods on these buffers.
    //
    // This is the case, for example, *after* stopping the pipe during a stream
    // switch or when deleting a stream.
    //
    // TODO(theryn): This design is fragile.  Consider reference counting pools
    // with buffers keeping a back reference to their owning pools to ensure
    // pools are not destructed prematurely.
    //

    if (mNumberOfBuffers != NOT_SPECIFIED)
    {
        for (Buffer  = ListOfBuffers;
             Buffer != NULL;
             Buffer  = Buffer->Next)
        {
            OS_LockMutex(&Buffer->BufferLock);
            bool alive = Buffer->ReferenceCount != 0;
            OS_UnLockMutex(&Buffer->BufferLock);
            if (alive)
            {
                mManager->DetachBuffer(Buffer);
            }
        }
    }
    else
    {
        while (ListOfBuffers != NULL)
        {
            mManager->DetachBuffer(ListOfBuffers);
        }
    }

    // Buffers still in use? This is at best a leak and at worst may cause heap
    // and BPA2 corruptions later on when buffers are accessed so raise a fatal
    // error.
    //
    // TODO(theryn): Currently, the SE must be designed so that pools outlive
    // all their buffers.  This is fragile and creates dependencies between
    // distant parts of SE.  The Right Thing is instead to reference-count pools
    // and have buffers keep a back-reference to their owning pool to prevent
    // premature pool destruction.
    if (CountOfReferencedBuffers != 0)
    {
        Dump();

        SE_FATAL("0x%p Destroying pool of type '%s' still referenced by %u buffers, MemoryInPool %d MemoryAllocated %d, MemoryInUse %d\n",
                 this, (mBufDataDescriptor->TypeName == NULL) ? "Unnamed" : mBufDataDescriptor->TypeName,
                 CountOfReferencedBuffers, this->Size, TotalAllocatedMemory, TotalUsedMemory);
    }

    while (ListOfBuffers != NULL)
    {
        Buffer          = ListOfBuffers;
        ListOfBuffers   = Buffer->Next;
        delete Buffer;
    }

    //
    // delete the ring holding free buffers
    //
    // TODO(pht) flush FreeBuffer
    delete FreeBuffer;
    FreeBuffer              = NULL;

    //
    // Delete any created allocator for the memory pool
    //
    delete MemoryPoolAllocator;
    MemoryPoolAllocator     = NULL;

    //
    // Delete any global memory allocation for the buffers
    //
    if (BufferBlock != NULL)
    {
        DeAllocateMemoryBlock(BufferBlock);
        delete BufferBlock;
        BufferBlock     = NULL;
    }
}

// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Detach a buffer from the buffers stored in pool
//

void BufferPool_Generic_c::DetachBuffer(Buffer_Generic_t Buffer)
{
    SE_VERBOSE(group_buffer, "0x%p Buffer 0x%p\n", this, Buffer);
    OS_LockMutex(&PoolLock);

    for (Buffer_Generic_t BufferSearch = ListOfBuffers;
         BufferSearch != NULL;
         BufferSearch = BufferSearch->Next)
    {
        OS_LockMutex(&BufferSearch->BufferLock);
        if (BufferSearch->ReferenceCount > 0)
        {
            BufferSearch->DetachBufferIfAttachedLocked(Buffer);
        }
        OS_UnLockMutex(&BufferSearch->BufferLock);
    }

    OS_UnLockMutex(&PoolLock);
}


// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//
//      Private - Function to check that memory parameters are appropriate for allocating data
//

BufferStatus_t   BufferPool_Generic_c::CheckMemoryParameters(
    BufferDataDescriptor_t   *Descriptor,
    bool                      ArrayAllocate,
    unsigned int              Size,
    void                     *MemoryPool,
    void                     *ArrayOfMemoryBlocks,
    char                     *MemoryPartitionName,
    const char               *Caller,
    unsigned int             *ItemSize)
{
    unsigned int    RequiredSize;
    unsigned int    ItemAlignement;
    unsigned int    PoolSize;

    // Calculate locals
    PoolSize  = Size;

    if (MemoryPool != NULL)
    {
        RequiredSize = Descriptor->HasFixedSize ? Descriptor->FixedSize : (ArrayAllocate ? (PoolSize / mNumberOfBuffers) : PoolSize);
    }
    else
    {
        RequiredSize = (Size != NOT_SPECIFIED) ? Size : (Descriptor->HasFixedSize ? Descriptor->FixedSize : NOT_SPECIFIED);
    }

    SE_VERBOSE(group_buffer, "0x%p poolsize:%d requiredsize:%d\n", this, PoolSize, RequiredSize);

    // Perform sizing check
    if (RequiredSize == NOT_SPECIFIED)
    {
        SE_ERROR("0x%p %s - Size not specified\n", this, Caller);
        return BufferError;
    }
    else if (Descriptor->HasFixedSize && (RequiredSize != Descriptor->FixedSize))
    {
        SE_ERROR("0x%p %s - Size does not match FixedSize in descriptor (%04x %04x)\n", this,
                 Caller, RequiredSize, Descriptor->FixedSize);
        return BufferError;
    }

    // Allocation of partition has to allow each of its items to be align as specified by user
    ItemAlignement = Descriptor->RequiredAlignment - 1;
    if (RequiredSize != UNSPECIFIED_SIZE)
    {
        RequiredSize = (RequiredSize + ItemAlignement) & (~ItemAlignement);
        //SE_INFO(group_buffer, "RequiredSize:%d (ItemAlignement:0x%x requiredAlignement (%d 0x%x)) \n",
        //RequiredSize, ItemAlignement,
        //Descriptor->RequiredAlignment, Descriptor->RequiredAlignment);
    }
    //
    // Perform those checks specific to array allocation
    //

    if (ArrayAllocate)
    {
        if (mNumberOfBuffers == NOT_SPECIFIED)
        {
            SE_ERROR("0x%p %s - Unknown number of buffers\n", this, Caller);
            return BufferError;
        }
    }
    else
    {
        if (ArrayOfMemoryBlocks != NULL)
        {
            SE_ERROR("0x%p %s - Attempt to use ArrayOfMemoryBlocks for single memory allocation\n", this, Caller);
            return BufferError;
        }
    }

    //
    // Now generic parameter checking
    //

    switch (Descriptor->AllocationSource)
    {
    case    NoAllocation:
        SE_ERROR("0x%p %s - No allocation allowed\n", this, Caller);
        return BufferError;
        break;

    case    AllocateFromNamedDeviceMemory:
        if ((MemoryPartitionName == NULL) ||
            (MemoryPartitionName[0] == '\0'))
        {
            SE_ERROR("0x%p %s - Parameters incompatible with buffer allocation source (No partition specified)\n", this, Caller);
            return BufferError;
        }

    // Fall through

    case    AllocateFromOSMemory:
        if ((MemoryPool != NULL) ||
            (ArrayOfMemoryBlocks != NULL))
        {
            SE_ERROR("0x%p %s - Parameters incompatible with buffer allocation source %p - %p\n", this, Caller, MemoryPool, ArrayOfMemoryBlocks);
            return BufferError;
        }

        break;

    case    AllocateFromSuppliedBlock:
        if ((PoolSize           == NOT_SPECIFIED)       ||
            (MemoryPool         == NULL))
        {
            SE_ERROR("0x%p %s - Memory block not supplied\n", this, Caller);
            return BufferError;
        }

        break;

    case    AllocateIndividualSuppliedBlocks:
        if (ArrayOfMemoryBlocks == NULL)
        {
            SE_ERROR("0x%p %s - Individual blocks of memory not supplied\n", this, Caller);
            return BufferError;
        }

        break;
    }

    if (ItemSize != NULL)
    {
        *ItemSize       = RequiredSize;
    }

    return BufferNoError;
}


// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Private - Function to allocate a block of memory
//

// Callers must not hold PoolLock or, on out of memory, there is a deadlock
// between the thread waiting for memory and threads releasing buffers.
BufferStatus_t   BufferPool_Generic_c::AllocateMemoryBlock(
    BlockDescriptor_t         Block,
    bool                      ArrayAllocate,
    unsigned int              Index,
    Allocator_c              *PoolAllocator,
    void                     *MemoryPool,
    void                     *ArrayOfMemoryBlocks,
    char                     *MemoryPartitionName,
    const char               *Caller,
    bool                     RequiredSizeIsLowerBound,
    unsigned int             MemoryAccessType,
    bool                     NonBlocking)
{
    unsigned int              i;
    BufferDataDescriptor_t   *Descriptor;
    AllocatorStatus_t         Status;
    allocator_status_t        AStatus;
    const char       *Partition = NULL;

    OS_AssertMutexNotHeld(&PoolLock);

    Block->Address[CachedAddress]       = NULL;
    Block->Address[PhysicalAddress]     = NULL;
    Descriptor                          = Block->Descriptor;
    SE_VERBOSE(group_buffer, "0x%p Block 0x%p Size %d AllocationSource %d\n",
               this, Block, Block->Size, Descriptor->AllocationSource);

    if (Block->Size == 0)
    {
        return BufferNoError;
    }

    switch (Descriptor->AllocationSource)
    {
    case AllocateFromOSMemory:
        Block->Address[CachedAddress] = new unsigned char[Block->Size];
        if (Block->Address[CachedAddress] == NULL)
        {
            SE_ERROR("0x%p %s - Failed to malloc memory\n", this, Caller);
            return BufferInsufficientMemoryGeneral;
        }

        break;

    case AllocateFromNamedDeviceMemory:
        Partition       = MemoryPartitionName;
        AStatus         = AllocatorOpenEx(&Block->MemoryAllocatorDevice, Block->Size, MemoryAccessType, Partition);
        if (AStatus != allocator_ok)
        {
            SE_ERROR("0x%p %s - Failed to allocate memory\n", this, Caller);
            return BufferInsufficientMemoryGeneral;
        }

        Block->Address[CachedAddress]   = AllocatorUserAddress(Block->MemoryAllocatorDevice);
        Block->Address[PhysicalAddress] = AllocatorPhysicalAddress(Block->MemoryAllocatorDevice);
        break;

    case AllocateFromSuppliedBlock:
        Block->PoolAllocatedOffset      = 0;

        if (!ArrayAllocate && (PoolAllocator != NULL))
        {
            if (0 == Descriptor->AllocationUnitSize)
            {
                SE_ERROR("0x%p invalid allocation unit size\n", this);
                return BufferError;
            }

            Block->Size = (((Block->Size + Descriptor->AllocationUnitSize - 1) / Descriptor->AllocationUnitSize) * Descriptor->AllocationUnitSize);

            if (RequiredSizeIsLowerBound)
            {
                Status  = PoolAllocator->Allocate(&Block->Size, (unsigned char **)&Block->PoolAllocatedOffset, true, NonBlocking);
            }
            else
            {
                Status  = PoolAllocator->Allocate(&Block->Size, (unsigned char **)&Block->PoolAllocatedOffset, false, NonBlocking);
            }

            if (Status != AllocatorNoError)
            {
                SE_ERROR("0x%p %s - Failed to allocate memory from pool\n", this, Caller);
                return BufferInsufficientMemoryGeneral;
            }
        }

        if ((Descriptor->Type & TYPE_TYPE_MASK) == MetaDataTypeBase)
        {
            Block->Address[CachedAddress] = (unsigned char *)MemoryPool + Block->PoolAllocatedOffset;
        }
        else
        {
            for (i = 0; i < 3; i++)
                if (((unsigned char **)MemoryPool)[i] != NULL)
                {
                    Block->Address[i] = ((unsigned char **)MemoryPool)[i] + Block->PoolAllocatedOffset;
                }
        }

        break;

    case AllocateIndividualSuppliedBlocks:
        break;          // Individual blocks are done externally

    default:        break;          // No action
    }

    //
    // If this is meta data we are allocating, then clear the memory.
    //
    if ((Descriptor->Type & TYPE_TYPE_MASK) == MetaDataTypeBase)
    {
        memset(Block->Address[CachedAddress], 0, Block->Size);
    }

    //
    // If this is buffer memory then update the total allocation record.
    //
    if ((Descriptor->Type & TYPE_TYPE_MASK) == BufferDataTypeBase)
    {
        TotalAllocatedMemory  += Block->Size;
    }

    return BufferNoError;
}


// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Private - Function to allocate a block of memory
//

void BufferPool_Generic_c::DeAllocateMemoryBlock(
    BlockDescriptor_t         Block)
{
    BufferDataDescriptor_t   *Descriptor;

    if (Block->Size == 0)
    {
        return;  // nothing to do
    }

    Descriptor = Block->Descriptor;
    SE_VERBOSE(group_buffer, "0x%p Block 0x%p Size %d AllocationSource %d\n",
               this, Block, Block->Size, Descriptor->AllocationSource);

    switch (Descriptor->AllocationSource)
    {
    case AllocateFromOSMemory:
        delete[](unsigned char *)Block->Address[CachedAddress];
        break;

    case AllocateFromNamedDeviceMemory:
        AllocatorClose(&Block->MemoryAllocatorDevice);
        break;

    case AllocateFromSuppliedBlock:
        if (MemoryPoolAllocator != NULL)
        {
            MemoryPoolAllocator->Free(Block->Size, (unsigned char *)Block->PoolAllocatedOffset);
        }
        break;

    default:        break;          // No action
    }

    //
    // If this is buffer memory then update the total allocation record.
    //

    if ((Descriptor->Type & TYPE_TYPE_MASK) == BufferDataTypeBase)
    {
        TotalAllocatedMemory  -= Block->Size;
    }

    Block->Address[CachedAddress]       = NULL;
    Block->Address[PhysicalAddress]     = NULL;
    Block->Size                         = 0;
    Block->MemoryAllocatorDevice        = NULL;
}


// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Private - Function to allocate a block of memory
//

BufferStatus_t   BufferPool_Generic_c::ShrinkMemoryBlock(
    BlockDescriptor_t         Block,
    unsigned int              NewSize)
{
    BufferDataDescriptor_t   *Descriptor;
    Descriptor          = Block->Descriptor;
    SE_VERBOSE(group_buffer, "0x%p Block 0x%p Size %d NewSize %d AllocationSource %d\n",
               this, Block, Block->Size, NewSize, Descriptor->AllocationSource);

    //
    // Check that this mechanism has memory allocated on a per buffer basis
    //

    if (Descriptor->AllocateOnPoolCreation)
    {
        SE_ERROR("0x%p Attempt to shrink a buffer allocated on pool creation\n", this);
        return BufferError;
    }

    //
    // Check that shrinkage is appropriate.
    //

    if (NewSize == Block->Size)
    {
        return BufferNoError;
    }

    if (NewSize > Block->Size)
    {
        SE_ERROR("0x%p Attempt to shrink a buffer to a larger size %d than Block->Size %d\n", this, NewSize, Block->Size);
        return BufferError;
    }

    //
    // Only specific allocation types can be shrunk.
    //

    switch (Descriptor->AllocationSource)
    {
    case NoAllocation:
        return BufferError;
        break;

    case AllocateFromOSMemory:
    case AllocateFromNamedDeviceMemory:
    case AllocateIndividualSuppliedBlocks:
        break;

    case AllocateFromSuppliedBlock:
        // First round up to the nearest allocation unit size
        NewSize         = (((NewSize + Descriptor->AllocationUnitSize - 1) / Descriptor->AllocationUnitSize) * Descriptor->AllocationUnitSize);

        if (NewSize < Block->Size)
        {
            MemoryPoolAllocator->Free(Block->Size - NewSize, (unsigned char *)Block->PoolAllocatedOffset + NewSize);

            if ((Descriptor->Type & TYPE_TYPE_MASK) == BufferDataTypeBase)
            {
                TotalAllocatedMemory   -= Block->Size - NewSize;
            }
        }

        Block->Size     = NewSize;
        break;
    }

    return BufferNoError;
}


// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Private - Function to allocate a block of memory
//

BufferStatus_t   BufferPool_Generic_c::ExtendMemoryBlock(
    BlockDescriptor_t Block,
    unsigned int     *NewSize,
    bool              ExtendUpwards)
{
    unsigned int i;
    AllocatorStatus_t Status;
    BufferDataDescriptor_t *Descriptor;
    Descriptor      = Block->Descriptor;
    SE_VERBOSE(group_buffer, "0x%p Block 0x%p Size %d AllocationSource %d\n",
               this, Block, Block->Size, Descriptor->AllocationSource);
    //
    // Initialize newsize consistently
    //

    if (NewSize != NULL)
    {
        *NewSize = Block->Size;
    }

    //
    // Check that this mechanism has memory allocated on a per buffer basis
    //

    if (Descriptor->AllocateOnPoolCreation)
    {
        SE_ERROR("0x%p Attempt to expand a buffer allocated on pool creation\n", this);
        return BufferError;
    }

    //
    // Only specific allocation types can be extended.
    //

    switch (Descriptor->AllocationSource)
    {
    case NoAllocation:
    case AllocateIndividualSuppliedBlocks:
    case AllocateFromOSMemory:
    case AllocateFromNamedDeviceMemory:
        return BufferError;
        break;

    case AllocateFromSuppliedBlock:
        //
        // Attempt extension
        //
        Status  = MemoryPoolAllocator->ExtendToLargest(&Block->Size, (unsigned char **)&Block->PoolAllocatedOffset, ExtendUpwards);
        if (Status != AllocatorNoError)
        {
            return BufferError;
        }

        for (i = 0; i < 3; i++)
            if (((unsigned char **)mMemoryPool)[i] != NULL)
            {
                Block->Address[i] = ((unsigned char **)mMemoryPool)[i] + Block->PoolAllocatedOffset;
            }

        if (NewSize != NULL)
        {
            *NewSize  = Block->Size;
        }

        break;
    }

    return BufferNoError;
}

