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

#include "buffer_generic.h"
#include "st_relayfs_se.h"
#include "player_generic.h"
#include "player_playback.h"
#include "player_stream.h"

#undef TRACE_TAG
#define TRACE_TAG "Buffer_Generic_c"

// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Constructor function - Initialize our data
//

Buffer_Generic_c::Buffer_Generic_c(BufferManager_Generic_t   Manager,
                                   BufferPool_Generic_t      Pool,
                                   BufferDataDescriptor_t   *Descriptor)
    : Manager(Manager)
    , Pool(Pool)
    , Next(NULL)
    , Index(0)
    , ReferenceCount(0)
    , OwnerIdentifier()
    , BufferBlock(NULL)
    , ListOfMetaData(NULL)
    , AttachedBuffers()
    , DataSize(0)
    , BufferLock()
{
    SE_VERBOSE(group_buffer, "0x%p Pool 0x%p TypeName %s Type %d\n", this, Pool,
               (Descriptor->TypeName == NULL) ? "Unnamed" : Descriptor->TypeName, Descriptor->Type);

    OS_InitializeMutex(&BufferLock);

    // TODO(pht) move FinalizeInit to a factory method
    InitializationStatus = FinalizeInit(Descriptor);
}

BufferStatus_t Buffer_Generic_c::FinalizeInit(BufferDataDescriptor_t *Descriptor)
{
    // Get a memory block descriptor, and initialize it
    BufferBlock = new struct BlockDescriptor_s(NULL, Descriptor, false, 0, NULL);
    if (BufferBlock == NULL)
    {
        SE_ERROR("0x%p Pool 0x%p TypeName %s Type %d Unable to create a block descriptor record\n",
                 this, Pool, (Descriptor->TypeName == NULL) ? "Unnamed" : Descriptor->TypeName, Descriptor->Type);
        return BufferError;
    }

    return BufferNoError;
}

// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Destructor function - close down
//

Buffer_Generic_c::~Buffer_Generic_c(void)
{
    SE_VERBOSE(group_buffer, "0x%p Pool 0x%p\n", this , Pool);

    delete BufferBlock;

    // make sure lock is not used before terminating.. W.A. for bz52878 - TODO check usage
    OS_LockMutex(&BufferLock);
    OS_UnLockMutex(&BufferLock);
    OS_TerminateMutex(&BufferLock);
}

// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Attach meta data structure
//

BufferStatus_t Buffer_Generic_c::AttachMetaData(
    MetaDataType_t Type,
    unsigned int   Size,
    void          *MemoryBlock,
    char          *DeviceMemoryPartitionName)
{
    //
    // Get the descriptor
    //
    BufferDataDescriptor_t *Descriptor;
    BufferStatus_t Status = Manager->GetDescriptor(Type, MetaDataTypeBase, &Descriptor);
    if (Status != BufferNoError)
    {
        return Status;
    }

    SE_VERBOSE(group_buffer, "0x%p Pool 0x%p Type %d Size %d TypeName %s\n",
               this , Pool, Type, Size, (Descriptor->TypeName == NULL) ? "Unnamed" : Descriptor->TypeName);

    //
    // Check the parameters and associated information to see if we can do this
    //
    unsigned int ItemSize = Size;

    if (Descriptor->AllocationSource != NoAllocation)
    {
        Status = Pool->CheckMemoryParameters(Descriptor, false, Size, MemoryBlock, NULL, DeviceMemoryPartitionName,
                                             "Buffer_Generic_c::AttachMetaData", &ItemSize);
        if (Status != BufferNoError)
        {
            return Status;
        }
    }
    else if (MemoryBlock == NULL)
    {
        SE_ERROR("0x%p Pool 0x%p TypeName %s Type %d Pointer not supplied for NoAllocation meta data\n",
                 this, Pool, (Descriptor->TypeName == NULL) ? "Unnamed" : Descriptor->TypeName, Descriptor->Type);
        return BufferParametersIncompatibleWithAllocationSource;
    }

    OS_LockMutex(&BufferLock);
    SE_ASSERT(ReferenceCount > 0);

    //
    // Don't allow several attach for same type => will leak otherwise
    //
    BlockDescriptor_t Block = LookupMetadata(Type, NOT_ATTACHED_METADATA);
    if (Block != NULL)
    {
        // TODO(pht) handle changed size : detach / reattach metadata
        if (Block->Size != Size)
        {
            SE_ERROR("trying to attach metadata with different size:%d (was:%d) is not supported\n",
                     Size, Block->Size);
            Status = BufferError;
        }
        else
        {
            Status = BufferNoError;
        }
        OS_UnLockMutex(&BufferLock);
        SE_INFO(group_buffer, "0x%p metadata already attached\n", this);
        return Status;
    }

    //
    // Create a new block descriptor record
    //
    Block = new BlockDescriptor_s(ListOfMetaData, Descriptor, false, ItemSize, NULL);
    if (Block == NULL)
    {
        OS_UnLockMutex(&BufferLock);
        SE_ERROR("0x%p Pool 0x%p TypeName %s Type %d Unable to create a block descriptor record\n",
                 this, Pool, (Descriptor->TypeName == NULL) ? "Unnamed" : Descriptor->TypeName, Descriptor->Type);
        return BufferInsufficientMemoryForMetaData;
    }

    //
    // Associate the memory
    //
    if (Descriptor->AllocationSource != NoAllocation)
    {
        Status = Pool->AllocateMemoryBlock(Block, false, 0, NULL, MemoryBlock, NULL, DeviceMemoryPartitionName,
                                           "Buffer_Generic_c::AttachMetaData");
        if (Status != BufferNoError)
        {
            OS_UnLockMutex(&BufferLock);
            SE_ERROR("0x%p Pool 0x%p TypeName %s Type %d AllocateMemoryBlock() failed: %d\n",
                     this, Pool, (Descriptor->TypeName == NULL) ? "Unnamed" : Descriptor->TypeName, Descriptor->Type,
                     Status);
            delete Block;
            return Status;
        }
    }
    else
    {
        Block->Address[CachedAddress] = MemoryBlock;
    }

    //
    // The block is fully initialized so we can register it.
    //
    ListOfMetaData = Block;

    OS_UnLockMutex(&BufferLock);

    return BufferNoError;
}


// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Detach meta data
//

void Buffer_Generic_c::DetachMetaData(MetaDataType_t Type)
{
    BlockDescriptor_t   Block;

    //
    // First find the descriptor block in the buffer
    //
    SE_VERBOSE(group_buffer, "0x%p Pool 0x%p Type %d\n", this , Pool, Type);

    OS_LockMutex(&BufferLock);
    SE_ASSERT(ReferenceCount > 0);

    BlockDescriptor_t  *LocationOfBlockPointer = LookupMetadataPtr(Type, NOT_ATTACHED_METADATA);
    if (LocationOfBlockPointer == NULL)
    {
        OS_UnLockMutex(&BufferLock);
        SE_ERROR("0x%p Pool 0x%p No metadata of type %d is attached to the buffer\n", this , Pool, Type);
        return;
    }

    //
    // Get a local block pointer, and unthread the block from the list
    //
    Block = *LocationOfBlockPointer;
    *LocationOfBlockPointer = Block->Next;
    OS_UnLockMutex(&BufferLock);

    //
    // Free up the memory, and delete the block record.
    //
    Pool->DeAllocateMemoryBlock(Block);

    delete Block;
}

// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Obtain a meta data reference
//

// Return pointer to metadata block of specified type or NULL if not found.
// The returned block is neither locked against concurrent accesses nor
// reference counted.  Callers must ensure the metadata is not destroyed or
// changed while accessed.
void Buffer_Generic_c::ObtainMetaDataReference(MetaDataType_t Type, void **Pointer)
{
    // same as for ObtainDataReference
    // in case type not found, pointer still set to null

    //
    // Find the descriptor block
    //
    OS_LockMutex(&BufferLock);
    SE_ASSERT(ReferenceCount > 0);

    BlockDescriptor_t Block = LookupMetadata(Type, ALL_METADATA);
    if (Block == NULL)
    {
        OS_UnLockMutex(&BufferLock);
        SE_ERROR("0x%p Pool 0x%p No metadata of type %d is attached to the buffer\n", this , Pool, Type);
        if (Pointer)
        {
            *Pointer = NULL;
        }
        return;
    }

    //
    // Return the appropriate value
    //
    if (Pointer)
    {
        *Pointer = Block->Address[CachedAddress];
    }

    OS_UnLockMutex(&BufferLock);
}

// Return pointer to metadata block matching Type and Filter or NULL if not found.
// Caller must hold BufferLock.
BlockDescriptor_t Buffer_Generic_c::LookupMetadata(MetaDataType_t Type, LookupMetadataFilter_t Filter)
{
    BlockDescriptor_t *p = LookupMetadataPtr(Type, Filter);
    return p != NULL ? *p : NULL;
}

// Return pointer to linked list link pointing to metadata block matching Type
// and Filter or NULL if not found.  The added indirection level allows to
// remove the looked up element in O(1).
// Caller must hold BufferLock.
BlockDescriptor_t *Buffer_Generic_c::LookupMetadataPtr(MetaDataType_t Type, LookupMetadataFilter_t Filter)
{
    OS_AssertMutexHeld(&BufferLock);

    for (BlockDescriptor_t *LocationOfBlockPointer = &ListOfMetaData;
         *LocationOfBlockPointer != NULL;
         LocationOfBlockPointer = &((*LocationOfBlockPointer)->Next))
    {
        if ((*LocationOfBlockPointer)->Descriptor->Type == Type)
        {
            switch (Filter)
            {
            case ALL_METADATA:
                return LocationOfBlockPointer;
            case NOT_ATTACHED_METADATA:
                if ((*LocationOfBlockPointer)->AttachedToPool == false)
                {
                    return LocationOfBlockPointer;
                }
            }
        }
    }

    return NULL;
}

// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Set the amount of the buffer that is used
//

BufferStatus_t  Buffer_Generic_c::SetUsedDataSize(unsigned int Size)
{
    OS_LockMutex(&BufferLock);
    SE_ASSERT(ReferenceCount > 0);
    OS_UnLockMutex(&BufferLock);

    OS_LockMutex(&Pool->PoolLock);

    if (Size > BufferBlock->Size)
    {
        SE_ERROR("0x%p Pool 0x%p Attempt to set Size %d larger than Buffer size %d\n",
                 this, Pool, Size, BufferBlock->Size);
        OS_UnLockMutex(&Pool->PoolLock);
        return BufferError;
    }

    SE_VERBOSE(group_buffer, "0x%p Pool 0x%p Type %d DataSize %d\n",
               this , Pool, GetTypeInternal(), DataSize);

    // TODO(pht) change this; shall not access directly pool members
    Pool->TotalUsedMemory = Pool->TotalUsedMemory - this->DataSize + Size;

    this->DataSize = Size;

    OS_UnLockMutex(&Pool->PoolLock);

    return BufferNoError;
}


// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Shrink the buffer
//

BufferStatus_t   Buffer_Generic_c::ShrinkBuffer(unsigned int NewSize)
{
    OS_LockMutex(&BufferLock);
    SE_ASSERT(ReferenceCount > 0);
    OS_UnLockMutex(&BufferLock);

    OS_LockMutex(&Pool->PoolLock);

    SE_VERBOSE(group_buffer, "0x%p Pool 0x%p DataSize %d NewSize %d\n", this , Pool, DataSize, NewSize);
    BufferStatus_t Status;
    if (NewSize < DataSize)
    {
        SE_ERROR("0x%p Pool 0x%p The new buffer size will be less than the content size\n",
                 this, Pool);
        Status = BufferError;
    }
    else
    {
        Status = Pool->ShrinkMemoryBlock(BufferBlock, NewSize);
    }

    OS_UnLockMutex(&Pool->PoolLock);

    return Status;
}


// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Expand the buffer
//

BufferStatus_t   Buffer_Generic_c::ExtendBuffer(unsigned int *NewSize,
                                                bool          ExtendUpwards)
{
    SE_VERBOSE(group_buffer, "0x%p Pool 0x%p\n", this , Pool);
    OS_LockMutex(&BufferLock);
    SE_ASSERT(ReferenceCount > 0);
    OS_UnLockMutex(&BufferLock);

    OS_LockMutex(&Pool->PoolLock);
    BufferStatus_t Status = Pool->ExtendMemoryBlock(BufferBlock, NewSize, ExtendUpwards);
    OS_UnLockMutex(&Pool->PoolLock);
    return Status;
}

// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Partition a buffer into separate buffers
//

BufferStatus_t   Buffer_Generic_c::PartitionBuffer(
    unsigned int  LeaveInFirstPartitionSize,
    bool          DuplicateMetaData,
    Buffer_t     *SecondPartition,
    unsigned int  SecondOwnerIdentifier,
    bool          NonBlocking)
{
    unsigned int            i;
    BufferStatus_t          Status;
    BufferDataDescriptor_t *Descriptor;
    Buffer_Generic_c       *NewBuffer;
    BlockDescriptor_t       Block;
    BlockDescriptor_t       Datum;
    MetaDataType_t          Type;
    unsigned int            Size;
    void                   *MemoryBlock;
    //
    // Can we partition this buffer
    //
    Descriptor  = BufferBlock->Descriptor;
    SE_VERBOSE(group_buffer, "0x%p Pool 0x%p TypeName %s Type %d "
               "LeaveInFirstPartitionSize %d DuplicateMetaData %d SecondOwnerIdentifier %d\n",
               this , Pool, (Descriptor->TypeName == NULL) ? "Unnamed" : Descriptor->TypeName, Descriptor->Type,
               LeaveInFirstPartitionSize, DuplicateMetaData, SecondOwnerIdentifier);

    if (Descriptor->AllocateOnPoolCreation)
    {
        SE_ERROR("All buffer memory allocated on pool creation, partitioning not supported\n");
        return BufferOperationNotSupportedByThisDescriptor;
    }

    switch (Descriptor->AllocationSource)
    {
    case AllocateFromOSMemory:
    case AllocateFromNamedDeviceMemory:
    case AllocateIndividualSuppliedBlocks:
        SE_ERROR("0x%p Pool 0x%p TypeName %s Type %d Partitioning not supported for this allocation source (%d)\n",
                 this , Pool, (Descriptor->TypeName == NULL) ? "Unnamed" : Descriptor->TypeName,
                 Descriptor->Type, Descriptor->AllocationSource);
        return BufferOperationNotSupportedByThisDescriptor;
        break;

    case AllocateFromSuppliedBlock:
        break;

    case NoAllocation:
        break;
    }

    //
    // Get a new buffer of zero size (specially forces no allocation)
    //
    Status  = Pool->GetBuffer(SecondPartition, SecondOwnerIdentifier, 0, NonBlocking);
    if (Status != BufferNoError)
    {
        return Status;
    }

    NewBuffer  = (Buffer_Generic_t)(*SecondPartition);

    OS_LockMutex(&Pool->PoolLock);

    //
    // Copy first buffer block into new one, and adjust memory pointers
    //
    Block      = NewBuffer->BufferBlock;

    memcpy(Block, BufferBlock, sizeof(struct BlockDescriptor_s));

    for (i = 0; i < 3; i++)
        if (Block->Address[i] != NULL)
        {
            Block->Address[i] = (unsigned char *)BufferBlock->Address[i] + LeaveInFirstPartitionSize;
        }

    Block->PoolAllocatedOffset = BufferBlock->PoolAllocatedOffset + LeaveInFirstPartitionSize;
    Block->Size                = BufferBlock->Size - LeaveInFirstPartitionSize;
    BufferBlock->Size          = LeaveInFirstPartitionSize;

    if (DataSize > LeaveInFirstPartitionSize)
    {
        NewBuffer->DataSize = DataSize - LeaveInFirstPartitionSize;
        DataSize            = LeaveInFirstPartitionSize;
    }

    OS_UnLockMutex(&Pool->PoolLock);

    OS_LockMutex(&BufferLock);
    SE_ASSERT(ReferenceCount > 0);

    //
    // Do we need to copy the metadata
    //
    if (DuplicateMetaData)
    {
        // TODO(pht) remove this section since never used/tested
        for (Datum  = ListOfMetaData;
             Datum != NULL;
             Datum  = Datum->Next)
        {
            Type    = Datum->Descriptor->Type;
            Size    = Datum->Size;

            switch (Datum->Descriptor->AllocationSource)
            {
            case NoAllocation:
                MemoryBlock = Datum->Address[CachedAddress];
                break;
            case AllocateFromOSMemory:
            case AllocateFromNamedDeviceMemory:
            case AllocateFromSuppliedBlock:
            default:
                MemoryBlock = NULL;
                break;
            }

            Status  = NewBuffer->AttachMetaData(Type, Size, MemoryBlock);
            if (Status != BufferNoError)
            {
                OS_UnLockMutex(&BufferLock);
                SE_ERROR("0x%p Pool 0x%p TypeName %s Type %d Unable to duplicate meta data block\n",
                         this, Pool, (Descriptor->TypeName == NULL) ? "Unnamed" : Descriptor->TypeName, Descriptor->Type);
                return BufferInsufficientMemoryForMetaData;
            }

            if (MemoryBlock == NULL)
            {
                NewBuffer->ObtainMetaDataReference(Type, &MemoryBlock);
                SE_ASSERT(MemoryBlock != NULL);

                memcpy(MemoryBlock, Datum->Address[CachedAddress], Size);
            }
        }
    }

    OS_UnLockMutex(&BufferLock);

    return BufferNoError;
}


// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Register data reference functions
//

BufferStatus_t   Buffer_Generic_c::RegisterDataReference(
    unsigned int   BlockSize,
    void          *Pointer,
    AddressType_t  AddressType)
{
    OS_LockMutex(&BufferLock);
    SE_ASSERT(ReferenceCount > 0);
    OS_UnLockMutex(&BufferLock);

    void *Pointers[3];
    Pointers[0]             = NULL;
    Pointers[1]             = NULL;
    Pointers[2]             = NULL;
    Pointers[AddressType]   = Pointer;
    return RegisterDataReference(BlockSize, Pointers);
}


BufferStatus_t   Buffer_Generic_c::RegisterDataReference(
    unsigned int  BlockSize,
    void         *Pointers[3])
{
    OS_LockMutex(&BufferLock);
    SE_ASSERT(ReferenceCount > 0);
    OS_UnLockMutex(&BufferLock);

    //
    // We can only specify the buffer address when the no-allocation method is used
    //

    if (BufferBlock->Descriptor->AllocationSource != NoAllocation)
    {
        SE_ERROR("0x%p Pool 0x%p TypeName %s Type %d Attempt to register a data reference pointer on an allocated buffer\n",
                 this, Pool, GetTypeNameInternal(), GetTypeInternal());
        return BufferOperationNotSupportedByThisDescriptor;
    }

    OS_LockMutex(&Pool->PoolLock);

    BufferBlock->Size       = BlockSize;
    BufferBlock->Address[0] = Pointers[0];
    BufferBlock->Address[1] = Pointers[1];
    BufferBlock->Address[2] = Pointers[2];

    DataSize            = 0;

    OS_UnLockMutex(&Pool->PoolLock);

    return BufferNoError;
}


// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//
//

BufferStatus_t  Buffer_Generic_c::ObtainDataReference(
    unsigned int   *BlockSize,
    unsigned int   *UsedDataSize,
    void          **Pointer,
    AddressType_t   AddressType)

{
    OS_LockMutex(&BufferLock);
    SE_ASSERT(ReferenceCount > 0);
    OS_UnLockMutex(&BufferLock);

    OS_LockMutex(&Pool->PoolLock);

    // all provided output parameters will be set
    // whatever the return status;
    // e.g. even in error case, pointer will be set to NULL
    if (BlockSize != NULL)
    {
        *BlockSize = BufferBlock->Size;
    }

    if (BufferBlock->Size == 0)
    {
        // This is not an exceptional behaviour (we call this from the audio compressed data bypass code
        // to discover if the buffer *has* compressed data attached in the first place). Since we do this
        // every frame we could really do not to report each occasion this happens.

        if (UsedDataSize != NULL)
        {
            *UsedDataSize = 0;
        }

        if (Pointer != NULL)
        {
            *Pointer = NULL;
        }

        OS_UnLockMutex(&Pool->PoolLock);
        return BufferNoDataAttached;
    }

    if ((BufferBlock->Address[AddressType] == NULL) && (Pointer != NULL))
    {
        OS_UnLockMutex(&Pool->PoolLock);
        // Address translation not supported
        SE_ERROR("0x%p Pool 0x%p TypeName %s Type %d Attempt to obtain a data reference pointer in an address type that is unavailable for the buffer\n",
                 this, Pool, GetTypeNameInternal(), GetTypeInternal());
        *Pointer = NULL;
        return BufferError;
    }

    if (UsedDataSize != NULL)
    {
        *UsedDataSize = DataSize;
    }

    if (Pointer != NULL)
    {
        *Pointer     = BufferBlock->Address[AddressType];
    }

    OS_UnLockMutex(&Pool->PoolLock);

    return BufferNoError;
}


// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//
//

void Buffer_Generic_c::TransferOwnership(
    unsigned int      OwnerIdentifier0,
    unsigned int      OwnerIdentifier1)
{
    SE_VERBOSE(group_buffer, "0x%p Pool 0x%p TypeName %s Type %d OwnerIdentifier0 %d OwnerIdentifier1 %d\n",
               this, Pool, GetTypeNameInternal(), GetTypeInternal(), OwnerIdentifier0, OwnerIdentifier1);

    OS_LockMutex(&BufferLock);
    SE_ASSERT(ReferenceCount > 0);

    if (OwnerIdentifier1 != UNSPECIFIED_OWNER)
    {
        unsigned int      i;
        for (i = 0; i < MAX_BUFFER_OWNER_IDENTIFIERS; i++)
            if (OwnerIdentifier[i] == OwnerIdentifier0)
            {
                OwnerIdentifier[i] = OwnerIdentifier1;
                OS_UnLockMutex(&BufferLock);
                return;
            }

        SE_WARNING("0x%p Pool 0x%p TypeName %s Type %d specified current owner not found\n",
                   this, Pool, GetTypeNameInternal(), GetTypeInternal());
    }
    else
    {
        OwnerIdentifier[0] = OwnerIdentifier0;
    }

    OS_UnLockMutex(&BufferLock);
}

// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//
//

void Buffer_Generic_c::IncrementReferenceCount(unsigned int NewOwnerIdentifier)
{
    OS_LockMutex(&BufferLock);
    SE_ASSERT(ReferenceCount > 0);
    ReferenceCount++;

    if (NewOwnerIdentifier != UNSPECIFIED_OWNER)
    {
        unsigned int i;
        for (i = 0; i < MAX_BUFFER_OWNER_IDENTIFIERS; i++)
            if (OwnerIdentifier[i] == UNSPECIFIED_OWNER)
            {
                OwnerIdentifier[i]  = NewOwnerIdentifier;
                break;
            }

        if (i >= MAX_BUFFER_OWNER_IDENTIFIERS)
        {
            SE_WARNING("0x%p Pool 0x%p More than expected references, new owner not recorded\n", this , Pool);
        }
    }

    SE_VERBOSE(group_buffer, "0x%p Pool 0x%p NewOwnerIdentifier %d ReferenceCount %d\n",
               this , Pool, NewOwnerIdentifier, ReferenceCount);
    OS_UnLockMutex(&BufferLock);
}


// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Decrement a reference count, and release the buffer if we get to zero.
//  We take a copy (during the locked period) of the reference to ensure
//  that if two processes are decrementing the count, only one will do the
//  actual release (without having to extend the period of locking).
//

void Buffer_Generic_c::DecrementReferenceCount(unsigned int  OldOwnerIdentifier)
{
    unsigned int ReferenceCountAfterDecrement;

    OS_LockMutex(&BufferLock);
    SE_ASSERT(ReferenceCount > 0);
    ReferenceCountAfterDecrement = --ReferenceCount;

    if (OldOwnerIdentifier != UNSPECIFIED_OWNER)
    {
        unsigned int  i;
        for (i = 0; i < MAX_BUFFER_OWNER_IDENTIFIERS; i++)
            if (OwnerIdentifier[i] == OldOwnerIdentifier)
            {
                OwnerIdentifier[i]  = UNSPECIFIED_OWNER;
                break;
            }
    }

    SE_VERBOSE(group_buffer, "0x%p Pool 0x%p OldOwnerIdentifier %d ReferenceCount %d\n",
               this , Pool, OldOwnerIdentifier, ReferenceCount);
    OS_UnLockMutex(&BufferLock);

    if (ReferenceCountAfterDecrement == 0)
    {
        Pool->ReleaseBuffer(this);
    }
}


// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Attach a buffer to this one
//
BufferStatus_t   Buffer_Generic_c::AttachBuffer(Buffer_t Buffer)
{
    SE_VERBOSE(group_buffer, "0x%p Pool 0x%p TypeName %s Type %d Buffer 0x%p\n",
               this , Pool, GetTypeNameInternal(), GetTypeInternal(), Buffer);

    if (Buffer == NULL)
    {
        return BufferError;
    }

    OS_LockMutex(&BufferLock);
    SE_ASSERT(ReferenceCount > 0);

    unsigned int i;
    // shall forbid multiple attachbuffer for same buffer ..
    // might leak if detachbuffer not called proper number of times
    for (i = 0; i < MAX_ATTACHED_BUFFERS; i++)
    {
        if (AttachedBuffers[i] == Buffer)
        {
            SE_DEBUG(group_buffer, "0x%p Pool 0x%p TypeName %s Type %d buffer already attached.. attaching again anyway\n",
                     this , Pool, GetTypeNameInternal(), GetTypeInternal());
            // OS_UnLockMutex(&BufferLock);
            // return BufferError; just output warning and keep going FTTB
        }
    }

    for (i = 0; i < MAX_ATTACHED_BUFFERS; i++)
    {
        if (AttachedBuffers[i] == NULL)
        {
            AttachedBuffers[i]  = Buffer;
            OS_UnLockMutex(&BufferLock);
            Buffer->IncrementReferenceCount();
            return BufferNoError;
        }
    }

    SE_ERROR("0x%p Pool 0x%p TypeName %s Type %d Too many buffers attached to this one\n",
             this , Pool, GetTypeNameInternal(), GetTypeInternal());

    OS_UnLockMutex(&BufferLock);

    return BufferTooManyAttachments;
}


// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Detach a buffer from this one
//

void Buffer_Generic_c::DetachBuffer(Buffer_t Buffer)
{
    if (Buffer != NULL && DetachBufferIfAttached(Buffer) == false)
    {
        SE_ERROR("0x%p Pool 0x%p TypeName %s Type %d Attached buffer not found\n", this , Pool,
                 GetTypeNameInternal(), GetTypeInternal());
    }
}

/*
 * Try to detach specified buffer.  Return true if detached, false if not
 * attached in first place.
 */
bool Buffer_Generic_c::DetachBufferIfAttached(Buffer_t Buffer)
{
    OS_LockMutex(&BufferLock);
    bool Detached = DetachBufferIfAttachedLocked(Buffer);
    OS_UnLockMutex(&BufferLock);
    return Detached;
}

/*
 * Same as DetachBufferIfAttached() but caller must hold this->BufferLock.
 */
bool Buffer_Generic_c::DetachBufferIfAttachedLocked(Buffer_t Buffer)
{
    SE_VERBOSE(group_buffer, "0x%p Pool 0x%p TypeName %s Type %d Buffer 0x%p\n",
               this , Pool, GetTypeNameInternal(), GetTypeInternal(), Buffer);

    OS_AssertMutexHeld(&BufferLock);
    SE_ASSERT(ReferenceCount > 0);

    if (Buffer == NULL)
    {
        return false;
    }

    unsigned int    i;
    for (i = 0; i < MAX_ATTACHED_BUFFERS; i++)
    {
        if (AttachedBuffers[i] == Buffer)
        {
            AttachedBuffers[i]  = NULL;
            Buffer->DecrementReferenceCount(IdentifierAttachedToOtherBuffer);
            return true;
        }
    }

    return false;
}

// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Obtain a meta data reference
//

// Return pointer to attached buffer of specified type or NULL if not found.
// The reference count of the attached buffer is not incremented.
void Buffer_Generic_c::ObtainAttachedBufferReference(
    BufferType_t  Type,
    Buffer_t     *Buffer)
{
    // same as for ObtainDataReference
    // in case type not found, pointer still set to null

    OS_LockMutex(&BufferLock);
    SE_ASSERT(ReferenceCount > 0);

    unsigned int i;
    for (i = 0; i < MAX_ATTACHED_BUFFERS; i++)
        if (AttachedBuffers[i] != NULL)
        {
            BufferType_t AttachedType;
            AttachedBuffers[i]->GetType(&AttachedType);

            if (AttachedType == Type)
            {
                if (Buffer)
                {
                    *Buffer = AttachedBuffers[i];
                }
                OS_UnLockMutex(&BufferLock);
                return;
            }
        }

    OS_UnLockMutex(&BufferLock);

    if (Buffer)
    {
        *Buffer = NULL;
    }
}

// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Discover the type of buffer supported by the pool
//

// Return type of this buffer.
// Internal version that can be called with or without lock.
inline BufferType_t Buffer_Generic_c::GetTypeInternal() const
{
    // Descriptor immutable so no locking needed.
    return BufferBlock->Descriptor->Type;
}


// Return type name of this buffer.
// Internal version that can be called with or without lock.
inline const char *Buffer_Generic_c::GetTypeNameInternal() const
{
    // Descriptor immutable so no locking needed.
    return (BufferBlock->Descriptor->TypeName == NULL) ? "Unnamed" : BufferBlock->Descriptor->TypeName;
}

void Buffer_Generic_c::GetType(BufferType_t *Type)
{
    OS_LockMutex(&BufferLock);
    SE_ASSERT(ReferenceCount > 0);
    OS_UnLockMutex(&BufferLock);

    *Type = GetTypeInternal();
}

// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Discover the type of buffer supported by the pool
//
void Buffer_Generic_c::GetIndex(unsigned int     *Index)
{
    OS_LockMutex(&BufferLock);
    SE_ASSERT(ReferenceCount > 0);
    OS_UnLockMutex(&BufferLock);

    *Index  = this->Index;
}

// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//
//
void Buffer_Generic_c::GetOwnerCount(unsigned int    *Count)
{
    OS_LockMutex(&BufferLock);
    SE_ASSERT(ReferenceCount > 0);
    *Count = ReferenceCount;
    OS_UnLockMutex(&BufferLock);
}

// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//
//

// Fill ArrayOfOwnerIdentifiers with up to ArraySize identifiers of the entities
// having references to this buffer.  Note that the returned information may
// change at any time due to concurrent activities in other threads.
void Buffer_Generic_c::GetOwnerList(unsigned int      ArraySize,
                                    unsigned int     *ArrayOfOwnerIdentifiers)
{
    unsigned int          i, j;

    OS_LockMutex(&BufferLock);
    SE_ASSERT(ReferenceCount > 0);

    i   = 0;
    for (j = 0; j < MAX_BUFFER_OWNER_IDENTIFIERS; j++)
        if (OwnerIdentifier[j] != UNSPECIFIED_OWNER)
        {
            if (i >= ArraySize)
            {
                break;
            }

            ArrayOfOwnerIdentifiers[i++]    = OwnerIdentifier[j];
        }

    OS_UnLockMutex(&BufferLock);

    if (j < MAX_BUFFER_OWNER_IDENTIFIERS)
    {
        SE_ERROR("0x%p Pool 0x%p Too many owner identifiers for array\n", this, Pool);
    }
}

// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//
//

void Buffer_Generic_c::Dump(unsigned int Flags)
{
    OS_LockMutex(&Pool->PoolLock);
    OS_LockMutex(&BufferLock);
    DumpLocked(Flags);
    OS_UnLockMutex(&BufferLock);
    OS_UnLockMutex(&Pool->PoolLock);
}

// Dump state to kernel log.
// Caller must hold both PoolLock and BufferLock.
void Buffer_Generic_c::DumpLocked(unsigned int Flags)
{
    unsigned int             i;
    unsigned int             LocalMetaCount;
    BlockDescriptor_t        MetaData;
    BufferDataDescriptor_t  *Descriptor;

    OS_AssertMutexHeld(&Pool->PoolLock);
    OS_AssertMutexHeld(&BufferLock);

    if ((Flags & DumpBufferStates) != 0)
    {
        SE_DEBUG(group_buffer, "0x%p Pool 0x%p (index %2d) of type %04x - '%s'\n",
                 this, Pool, Index, GetTypeInternal(), GetTypeNameInternal());
        SE_DEBUG(group_buffer, "0x%p Pool 0x%p Current values  Size %06x Occupied %06x References %d\n",
                 this, Pool, BufferBlock->Size, DataSize, ReferenceCount);

        if (ReferenceCount != 0)
        {
            for (i = 0; i < MAX_BUFFER_OWNER_IDENTIFIERS; i++)
                if (OwnerIdentifier[i] != UNSPECIFIED_OWNER)
                    SE_DEBUG(group_buffer, "0x%p Pool 0x%p OwnerIdentifier[%d] %08x\n",
                             this, Pool, i, OwnerIdentifier[i]);
        }

        LocalMetaCount  = 0;

        for (MetaData  = ListOfMetaData;
             MetaData != NULL;
             MetaData  = MetaData->Next)
            if (!MetaData->AttachedToPool)
            {
                LocalMetaCount++;
            }

        SE_DEBUG(group_buffer, "0x%p Pool 0x%p metadata attached to this buffer: %s\n",
                 this, Pool, (LocalMetaCount == 0) ? "None" : "Yes");

        for (MetaData  = ListOfMetaData;
             MetaData != NULL;
             MetaData  = MetaData->Next)
            if (!MetaData->AttachedToPool)
                SE_DEBUG(group_buffer, "0x%p Pool 0x%p %04x - %s\n", this, Pool, MetaData->Descriptor->Type,
                         (MetaData->Descriptor->TypeName == NULL) ? "Unnamed" : MetaData->Descriptor->TypeName);

        for (i = 0; i < MAX_ATTACHED_BUFFERS; i++)
            if (AttachedBuffers[i] != NULL)
            {
                Descriptor  = ((Buffer_Generic_c *)AttachedBuffers[i])->Pool->mBufDataDescriptor;  // TODO(pht) fixme
                SE_DEBUG(group_buffer, "0x%p  Pool 0x%p AttachedBuffers[%d] 0x%p type %04x - %s\n",
                         this, Pool, i, AttachedBuffers[i], Descriptor->Type,
                         (Descriptor->TypeName == NULL) ? "Unnamed" : Descriptor->TypeName);
            }
    }
}

void Buffer_Generic_c::DumpViaRelay(unsigned int id, unsigned int source)
{
    OS_LockMutex(&Pool->PoolLock);
    if (BufferBlock->Address[CachedAddress] && DataSize)
    {
        st_relayfs_write_se(id, source, (unsigned char *) BufferBlock->Address[CachedAddress], DataSize, 0);
    }
    OS_UnLockMutex(&Pool->PoolLock);
}

