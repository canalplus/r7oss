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

#ifndef H_BUFFER_POOL
#define H_BUFFER_POOL

#include "base_component_class.h"
#include "buffer.h"

class BufferPool_c : public BaseComponentClass_c
{
public:
    //
    // Global operations on all pool members
    //

    virtual BufferStatus_t   AttachMetaData(MetaDataType_t Type,
                                            unsigned int   Size                      = UNSPECIFIED_SIZE,
                                            void          *MemoryPool                = NULL,
                                            void          *ArrayOfMemoryBlocks[]     = NULL,
                                            char          *DeviceMemoryPartitionName = NULL) = 0;
    virtual void             DetachMetaData(MetaDataType_t Type) = 0;

    //
    // Get/Release a buffer
    //

    virtual BufferStatus_t   GetBuffer(Buffer_t     *Buffer,
                                       unsigned int  OwnerIdentifier           = UNSPECIFIED_OWNER,
                                       unsigned int  RequiredSize              = UNSPECIFIED_SIZE,
                                       bool          NonBlocking               = false,
                                       bool          RequiredSizeIsLowerBound  = false,
                                       uint32_t      TimeOut                   = 0) = 0;

    virtual void             AbortBlockingGetBuffer(void) = 0;

    virtual void             SetMemoryAccessType(unsigned int) = 0;

    //
    // Usage/query/debug methods
    //

    virtual void             GetType(BufferType_t *Type) = 0;

    virtual void             GetPoolUsage(unsigned int *BuffersInPool,
                                          unsigned int *BuffersWithNonZeroReferenceCount = NULL,
                                          unsigned int *MemoryInPool                     = NULL,
                                          unsigned int *MemoryAllocated                  = NULL,
                                          unsigned int *MemoryInUse                      = NULL,
                                          unsigned int *LargestFreeMemoryBlock           = NULL) = 0;

    virtual BufferStatus_t   GetAllUsedBuffers(unsigned int  ArraySize,
                                               Buffer_t     *ArrayOfBuffers,
                                               unsigned int  OwnerIdentifier) = 0;

    //
    // Status dump/reporting
    //

    virtual void Dump(unsigned int Flags = DumpAll) = 0;
};
#endif
