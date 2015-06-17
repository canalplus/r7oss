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

#ifndef H_BUFFER_INDIVIDUAL
#define H_BUFFER_INDIVIDUAL

#include "osinline.h"

class Buffer_c : public BaseComponentClass_c
{
public:
    //
    // Meta data activities
    //

    virtual BufferStatus_t   AttachMetaData(MetaDataType_t    Type,
                                            unsigned int      Size                      = UNSPECIFIED_SIZE,
                                            void             *MemoryBlock               = NULL,
                                            char             *DeviceMemoryPartitionName = NULL) = 0;

    virtual void             DetachMetaData(MetaDataType_t    Type) = 0;

    virtual void             ObtainMetaDataReference(MetaDataType_t Type, void **Pointer) = 0;

    //
    // Buffer manipulators
    //

    virtual BufferStatus_t   SetUsedDataSize(unsigned int DataSize) = 0;

    virtual BufferStatus_t   ShrinkBuffer(unsigned int    NewSize) = 0;

    virtual BufferStatus_t   ExtendBuffer(unsigned int   *NewSize,
                                          bool            Upwards  = true) = 0;

    virtual BufferStatus_t   PartitionBuffer(unsigned int  LeaveInFirstPartitionSize,
                                             bool          DuplicateMetaData, // TODO(pht) remove
                                             Buffer_t     *SecondPartition,
                                             unsigned int  SecondOwnerIdentifier = UNSPECIFIED_OWNER,
                                             bool          NonBlocking = false) = 0;

    //
    // Reference manipulation, and ownership control
    //

    virtual BufferStatus_t   RegisterDataReference(unsigned int  BlockSize,
                                                   void         *Pointer,
                                                   AddressType_t AddressType = CachedAddress) = 0;

    virtual BufferStatus_t   RegisterDataReference(unsigned int  BlockSize,
                                                   void         *Pointers[3]) = 0;

    virtual BufferStatus_t   ObtainDataReference(unsigned int    *BlockSize,
                                                 unsigned int    *UsedDataSize,
                                                 void           **Pointer,
                                                 AddressType_t    AddressType = CachedAddress) = 0;

    virtual void             TransferOwnership(unsigned int   OwnerIdentifier0,
                                               unsigned int   OwnerIdentifier1  = UNSPECIFIED_OWNER) = 0;

    virtual void IncrementReferenceCount(unsigned int OwnerIdentifier = UNSPECIFIED_OWNER) = 0;

    virtual void DecrementReferenceCount(unsigned int OwnerIdentifier = UNSPECIFIED_OWNER) = 0;

    //
    // linking of other buffers to this buffer - for increment/decrement management
    //

    virtual BufferStatus_t  AttachBuffer(Buffer_t    Buffer) = 0;
    virtual void            DetachBuffer(Buffer_t    Buffer) = 0;

    virtual void            ObtainAttachedBufferReference(BufferType_t  Type,
                                                          Buffer_t     *Buffer) = 0;

    //
    // Usage/query/debug methods
    //

    virtual void GetType(BufferType_t  *Type) = 0;
    virtual void GetIndex(unsigned int *Index) = 0;

    virtual void GetOwnerCount(unsigned int  *Count) = 0;
    virtual void GetOwnerList(unsigned int    ArraySize,
                              unsigned int   *ArrayOfOwnerIdentifiers) = 0;

    //
    // Status dump/reporting
    //

    virtual void  Dump(unsigned int    Flags = DumpAll) = 0;
    virtual void  DumpViaRelay(unsigned int id, unsigned int type) = 0;
};
#endif
