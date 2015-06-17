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

#undef TRACE_TAG
#define TRACE_TAG "BufferManager_Generic_c"

BufferManager_Generic_c::BufferManager_Generic_c(void)
    : Lock()
    , TypeDescriptorCount(0)
    , TypeDescriptors()
    , ListOfBufferPools(NULL)
{
    SE_VERBOSE(group_buffer, "0x%p\n", this);

    OS_InitializeMutex(&Lock);
}

BufferManager_Generic_c::~BufferManager_Generic_c(void)
{
    SE_VERBOSE(group_buffer, "0x%p\n", this);

    while (ListOfBufferPools != NULL)
    {
        DestroyPool(ListOfBufferPools);
    }

    for (unsigned int i = 0; i < TypeDescriptorCount; i++)
    {
        OS_Free(const_cast<char *>(TypeDescriptors[i].TypeName));
    }

    OS_TerminateMutex(&Lock);
}

// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Add to defined types
//

BufferStatus_t   BufferManager_Generic_c::CreateBufferDataType(
    BufferDataDescriptor_t   *Descriptor,
    BufferType_t         *Type)
{
    SE_VERBOSE(group_buffer, "0x%p Type %d Typename %s\n",
               this, Descriptor->Type, (Descriptor->TypeName == NULL) ? "Unnamed" : Descriptor->TypeName);
    //
    // Initialize the return parameter
    //
    *Type   = 0xffffffff;

    if (Descriptor->Type == MetaDataTypeBase)
    {
        if (Descriptor->AllocationSource == AllocateIndividualSuppliedBlocks)
        {
            SE_ERROR("0x%p Cannot allocate meta data from supplied block list\n", this);
            return BufferUnsupportedAllocationSource;
        }

        if (Descriptor->AllocateOnPoolCreation)
        {
            SE_ERROR("0x%p Cannot allocate meta data on pool creation\n", this);
            return BufferUnsupportedAllocationSource;
        }
    }
    else if (Descriptor->Type != BufferDataTypeBase)
    {
        SE_ERROR("0x%p Invalid type value\n", this);
        return BufferInvalidDescriptor;
    }

    //

    if (Descriptor->HasFixedSize && (Descriptor->FixedSize == NOT_SPECIFIED))
    {
        SE_ERROR("0x%p Fixed size not specified\n", this);
        return BufferInvalidDescriptor;
    }

    //
    // From here on in we need to lock access to the data structure, since we are now allocating an entry
    //
    OS_LockMutex(&Lock);

    if (TypeDescriptorCount >= MAX_BUFFER_DATA_TYPES)
    {
        OS_UnLockMutex(&Lock);
        SE_ERROR("0x%p Insufficient space to hold new data type descriptor\n", this);
        return BufferTooManyDataTypes;
    }

    //
    memcpy(&TypeDescriptors[TypeDescriptorCount], Descriptor, sizeof(BufferDataDescriptor_t));

    if (Descriptor->TypeName != NULL)
    {
        TypeDescriptors[TypeDescriptorCount].TypeName   = static_cast<const char *>(OS_StrDup(Descriptor->TypeName));

        if (TypeDescriptors[TypeDescriptorCount].TypeName == NULL)
        {
            OS_UnLockMutex(&Lock);
            SE_ERROR("0x%p Insufficient memory to copy type name\n", this);
            return BufferInsufficientMemoryGeneral;
        }
    }

    TypeDescriptors[TypeDescriptorCount].Type   = Descriptor->Type | TypeDescriptorCount;
    *Type                   = TypeDescriptors[TypeDescriptorCount].Type;
    //
    TypeDescriptorCount++;
    OS_UnLockMutex(&Lock);
    return BufferNoError;
}

// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Retrieve the descriptor for a defined type
//

BufferStatus_t   BufferManager_Generic_c::FindBufferDataType(
    const char       *TypeName,
    BufferType_t     *Type)
{
    unsigned int    i;

    //
    // Initialize the return parameter
    //

    if (TypeName == NULL)
    {
        SE_ERROR("0x%p Cannot find an unnamed type\n", this);
        return BufferDataTypeNotFound;
    }

    OS_LockMutex(&Lock);

    for (i = 0; i < TypeDescriptorCount; i++)
        if ((TypeDescriptors[i].TypeName != NULL) &&
            (strcmp(TypeDescriptors[i].TypeName, TypeName) == 0))
        {
            *Type   = TypeDescriptors[i].Type;
            OS_UnLockMutex(&Lock);
            return BufferNoError;
        }

    OS_UnLockMutex(&Lock);
    return BufferDataTypeNotFound;
}

// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Get descriptor
//

BufferStatus_t   BufferManager_Generic_c::GetDescriptor(BufferType_t          Type,
                                                        BufferPredefinedType_t   RequiredKind,
                                                        BufferDataDescriptor_t **Descriptor)
{
    if ((Type & TYPE_TYPE_MASK) != (unsigned int)RequiredKind)
    {
        SE_ERROR("0x%p Wrong kind of data type (%08x)\n", this, RequiredKind);
        return BufferDataTypeNotFound;
    }

    OS_LockMutex(&Lock);

    if ((Type & TYPE_INDEX_MASK) > TypeDescriptorCount)
    {
        OS_UnLockMutex(&Lock);
        SE_ERROR("0x%p Attempt to create buffer with unregisted type (%08x)\n", this, Type);
        return BufferDataTypeNotFound;
    }

    *Descriptor = &TypeDescriptors[Type & TYPE_INDEX_MASK];
    OS_UnLockMutex(&Lock);

    if ((*Descriptor)->Type != Type)
    {
        SE_ERROR("0x%p Type descriptor does not match type\n", this);
        return BufferInvalidDescriptor;
    }

    return BufferNoError;
}


// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Create a pool of buffers - common implementation
//

BufferStatus_t   BufferManager_Generic_c::CreatePool(
    BufferPool_t     *Pool,
    BufferType_t      Type,
    unsigned int      NumberOfBuffers,
    unsigned int      Size,
    void             *MemoryPool[3],
    void             *ArrayOfMemoryBlocks[][3],
    char             *DeviceMemoryPartitionName,
    bool              AllowCross64MbBoundary,
    unsigned int      MemoryAccessType)
{
    BufferStatus_t          Status;
    BufferDataDescriptor_t *Descriptor;
    BufferPool_Generic_t    NewPool;

    //
    // Initialize the return parameter
    //

    if (Pool == NULL)
    {
        SE_ERROR("0x%p Null supplied as place to return Pool pointer\n", this);
        return BufferError;
    }

    *Pool   = NULL;
    //
    // Get the descriptor
    //
    Status  = GetDescriptor(Type, BufferDataTypeBase, &Descriptor);

    if (Status != BufferNoError)
    {
        return Status;
    }

    SE_DEBUG(group_buffer, "0x%p Type %d Typename %s NumberOfBuffers %d Size %d partition %s AllowCross64MbBoundary:%d\n",
             this, Descriptor->Type, (Descriptor->TypeName == NULL) ? "Unnamed" : Descriptor->TypeName,
             NumberOfBuffers, Size, (DeviceMemoryPartitionName == NULL) ? "Unnamed" : DeviceMemoryPartitionName,
             AllowCross64MbBoundary);

    //
    // Perform simple parameter checks
    //

    if (Descriptor->AllocateOnPoolCreation &&
        (NumberOfBuffers == NOT_SPECIFIED))
    {
        SE_ERROR("0x%p Unable to allocate on creation, NumberOfBuffers not specified\n", this);
        return BufferParametersIncompatibleWithAllocationSource;
    }

    //
    // Perform the creation
    //
    NewPool = new BufferPool_Generic_c(this, Descriptor, NumberOfBuffers, Size, MemoryPool,
                                       ArrayOfMemoryBlocks, DeviceMemoryPartitionName, AllowCross64MbBoundary, MemoryAccessType);
    if ((NewPool == NULL) || (NewPool->InitializationStatus != BufferNoError))
    {
        BufferStatus_t  Status;
        Status      = BufferInsufficientMemoryForPool;

        if (NewPool != NULL)
        {
            Status    = NewPool->InitializationStatus;
        }

        SE_ERROR("0x%p Failed to create pool (%08x)\n", this, Status);
        return Status;
    }

    //
    // Insert the pool into our list
    //
    OS_LockMutex(&Lock);
    NewPool->Next   = ListOfBufferPools;
    ListOfBufferPools   = NewPool;
    OS_UnLockMutex(&Lock);

    *Pool = NewPool;

    SE_DEBUG(group_buffer, "0x%p Created pool 0x%p\n", this, NewPool);
    return BufferNoError;
}


// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Destroy a pool of buffers
//

void BufferManager_Generic_c::DestroyPool(BufferPool_t    Pool)
{
    BufferPool_Generic_t     LocalPool  = (BufferPool_Generic_t)Pool;
    BufferPool_Generic_t    *LocationOfPointer;

    SE_DEBUG(group_buffer, "0x%p Destroy pool 0x%p\n", this, Pool);

    //
    // First find and remove the pool from our list
    //
    OS_LockMutex(&Lock);

    for (LocationOfPointer    = &ListOfBufferPools;
         *LocationOfPointer  != NULL;
         LocationOfPointer    = &((*LocationOfPointer)->Next))
        if (*LocationOfPointer == LocalPool)
        {
            break;
        }

    if (*LocationOfPointer == NULL)
    {
        OS_UnLockMutex(&Lock);
        SE_ERROR("0x%p Pool not found\n", this);
        return;
    }

    *LocationOfPointer  = LocalPool->Next;
    OS_UnLockMutex(&Lock);
    //
    // Then destroy the actual pool
    //
    delete LocalPool;
}

// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Detach a buffer from all pools
//

void BufferManager_Generic_c::DetachBuffer(Buffer_Generic_t Buffer)
{
    SE_VERBOSE(group_buffer, "0x%p Buffer 0x%p\n", this, Buffer);
    OS_LockMutex(&Lock);

    BufferPool_Generic_t PoolSearch;
    for (PoolSearch = ListOfBufferPools;
         (PoolSearch != NULL); PoolSearch = PoolSearch->Next)
    {
        PoolSearch->DetachBuffer(Buffer);
    }

    OS_UnLockMutex(&Lock);
}

// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Status dump/reporting
//

static const char *AllocationSources[] =
{
    "NoAllocation",
    "AllocateFromOSMemory",
    "AllocateFromSuppliedBlock",
    "AllocateIndividualSuppliedBlocks",
    "AllocateFromNamedDeviceMemory"
};


void   BufferManager_Generic_c::Dump(unsigned int Flags)
{
    unsigned int        i;
    BufferPool_Generic_t    Pool;
    OS_LockMutex(&Lock);

    for (i = 0; i < TypeDescriptorCount; i++)
        if ((((TypeDescriptors[i].Type & TYPE_TYPE_MASK) == BufferDataTypeBase) && (Flags & DumpBufferTypes) != 0) ||
            (((TypeDescriptors[i].Type & TYPE_TYPE_MASK) == MetaDataTypeBase) && (Flags & DumpMetaDataTypes) != 0))
        {
            SE_DEBUG(group_buffer, "0x%p Buffer Type %04x - '%s' AllocationSource = %s\n", this, TypeDescriptors[i].Type,
                     (TypeDescriptors[i].TypeName == NULL) ? "Unnamed" : TypeDescriptors[i].TypeName,
                     (TypeDescriptors[i].AllocationSource <= AllocateIndividualSuppliedBlocks) ?
                     AllocationSources[TypeDescriptors[i].AllocationSource] : "Invalid");
            SE_DEBUG(group_buffer, "0x%p RequiredAlignment %08x AllocationUnitSize %08x AllocateOnPoolCreation %d "
                     "HasFixedSize %d FixedSize %08x\n",
                     this, TypeDescriptors[i].RequiredAlignment, TypeDescriptors[i].AllocationUnitSize, TypeDescriptors[i].AllocateOnPoolCreation,
                     TypeDescriptors[i].HasFixedSize, TypeDescriptors[i].FixedSize);
        }

    if ((Flags & DumpListPools) != 0)
    {
        SE_DEBUG(group_buffer, "0x%p Dump of Buffer Pools\n", this);

        for (Pool  = ListOfBufferPools;
             Pool != NULL;
             Pool  = Pool->Next)
        {
            Pool->Dump(Flags);
        }
    }

    OS_UnLockMutex(&Lock);
}

void BufferManager_Generic_c:: CreateIcsMap()
{
    OS_LockMutex(&Lock);

    BufferPool_Generic_t PoolSearch;
    for (PoolSearch = ListOfBufferPools;
         (PoolSearch != NULL); PoolSearch = PoolSearch->Next)
    {
        if (PoolSearch->BufferBlock && PoolSearch->BufferBlock->MemoryAllocatorDevice && PoolSearch->BufferBlock->MemoryAllocatorDevice->UnderlyingDevice)
        {
            AllocatorCreateMapEx(PoolSearch->BufferBlock->MemoryAllocatorDevice->UnderlyingDevice);
        }
    }
    OS_UnLockMutex(&Lock);
}

void BufferManager_Generic_c:: ResetIcsMap()
{
    OS_LockMutex(&Lock);

    BufferPool_Generic_t PoolSearch;
    for (PoolSearch = ListOfBufferPools;
         (PoolSearch != NULL); PoolSearch = PoolSearch->Next)
    {
        if (PoolSearch->BufferBlock && PoolSearch->BufferBlock->MemoryAllocatorDevice && PoolSearch->BufferBlock->MemoryAllocatorDevice->UnderlyingDevice)
        {
            AllocatorRemoveMapEx(PoolSearch->BufferBlock->MemoryAllocatorDevice->UnderlyingDevice);
        }
    }
    OS_UnLockMutex(&Lock);
}
