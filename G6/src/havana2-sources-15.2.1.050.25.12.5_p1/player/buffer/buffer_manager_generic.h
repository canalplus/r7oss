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

#ifndef H_BUFFER_MANAGER_GENERIC
#define H_BUFFER_MANAGER_GENERIC

class BufferManager_Generic_c : public BufferManager_c
{
public:
    BufferManager_Generic_c(void);
    ~BufferManager_Generic_c(void);

    //
    // Add to the defined types
    //

    BufferStatus_t  CreateBufferDataType(BufferDataDescriptor_t  *Descriptor,
                                         BufferType_t            *Type);

    BufferStatus_t  FindBufferDataType(const char          *TypeName,
                                       BufferType_t        *Type);

    BufferStatus_t  GetDescriptor(BufferType_t              Type,
                                  BufferPredefinedType_t    RequiredKind,
                                  BufferDataDescriptor_t  **Descriptor);

    //
    // Create/destroy a pool of buffers overloaded creation function
    //

    BufferStatus_t   CreatePool(BufferPool_t  *Pool,
                                BufferType_t   Type,
                                unsigned int   NumberOfBuffers           = UNRESTRICTED_NUMBER_OF_BUFFERS,
                                unsigned int   Size                      = UNSPECIFIED_SIZE,
                                void          *MemoryPool[3]             = NULL,
                                void          *ArrayOfMemoryBlocks[][3]  = NULL,
                                char          *DeviceMemoryPartitionName = NULL,
                                bool           AllowCross64MbBoundary    = true,
                                unsigned int   MemoryAccessType          = MEMORY_DEFAULT_ACCESS);

    void             DestroyPool(BufferPool_t         Pool);

    void             DetachBuffer(Buffer_Generic_t Buffer);

    void             ResetIcsMap(void);

    void             CreateIcsMap(void);

    //
    // Status dump/reporting
    //

    void         Dump(unsigned int    Flags = DumpAll);

    friend class BufferPool_Generic_c;
    friend class Buffer_Generic_c;

private:
    OS_Mutex_t              Lock;

    unsigned int            TypeDescriptorCount;
    BufferDataDescriptor_t  TypeDescriptors[MAX_BUFFER_DATA_TYPES];

    BufferPool_Generic_t    ListOfBufferPools;

    DISALLOW_COPY_AND_ASSIGN(BufferManager_Generic_c);
};

#endif
