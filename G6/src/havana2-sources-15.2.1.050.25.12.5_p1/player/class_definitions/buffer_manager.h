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

#ifndef H_BUFFER_MANAGER
#define H_BUFFER_MANAGER

typedef enum
{
    NoAllocation        = 0,
    AllocateFromOSMemory,
    AllocateFromSuppliedBlock,
    AllocateIndividualSuppliedBlocks,
    AllocateFromNamedDeviceMemory       // Allocator with memory partition name
} BufferAllocationSource_t;

// ---------------------------------------------------------------------
//
// Descriptor record, for pre-defined types, and user defined type
//

typedef enum
{
    BufferDataTypeBase      = 0x0000,
    MetaDataTypeBase        = 0x8000,
} BufferPredefinedType_t;

typedef struct BufferDataDescriptor_s
{
    const char          *TypeName;
    BufferType_t         Type;

    BufferAllocationSource_t AllocationSource;
    unsigned int             RequiredAlignment;
    unsigned int             AllocationUnitSize;

    bool             HasFixedSize;
    bool             AllocateOnPoolCreation;
    unsigned int     FixedSize;
} BufferDataDescriptor_t;

#define InitializeBufferDataDescriptor(d)   d = { NULL, BufferDataTypeBase, NoAllocation,         32, 4096, false, false, NOT_SPECIFIED }
#define InitializeMetaDataDescriptor(d)     d = { NULL, MetaDataTypeBase,   AllocateFromOSMemory, 16,    0, true,  false, NOT_SPECIFIED }

class BufferManager_c : public BaseComponentClass_c
{
public:
    virtual BufferStatus_t  CreateBufferDataType(
        BufferDataDescriptor_t *Descriptor,
        BufferType_t           *Type) = 0;

    virtual BufferStatus_t  FindBufferDataType(
        const char    *TypeName,
        BufferType_t  *Type) = 0;

    virtual BufferStatus_t  GetDescriptor(BufferType_t              Type,
                                          BufferPredefinedType_t    RequiredKind,
                                          BufferDataDescriptor_t  **Descriptor) = 0;

    //
    // Create/destroy a pool of buffers rather than overload the create function,
    // I have included multiple possible parameters, but with default values.
    //
    // For example no call should specify both a memory pool and an array of memory blocks.
    //

    virtual BufferStatus_t  CreatePool(BufferPool_t  *Pool,
                                       BufferType_t   Type,
                                       unsigned int   NumberOfBuffers           = UNRESTRICTED_NUMBER_OF_BUFFERS,
                                       unsigned int   Size                      = UNSPECIFIED_SIZE,
                                       void          *MemoryPool[3]             = NULL,
                                       void          *ArrayOfMemoryBlocks[][3]  = NULL,
                                       char          *DeviceMemoryPartitionName = NULL,
                                       bool           AllowCross64MbBoundary    = true,
                                       unsigned int   MemoryAccessType          = MEMORY_DEFAULT_ACCESS) = 0;

    virtual void            DestroyPool(BufferPool_t  Pool) = 0;

    virtual void            ResetIcsMap(void) = 0;

    virtual void            CreateIcsMap(void) = 0;

    //
    // Status dump/reporting
    //

    virtual void            Dump(unsigned int    Flags = DumpAll) = 0;
};

#endif

