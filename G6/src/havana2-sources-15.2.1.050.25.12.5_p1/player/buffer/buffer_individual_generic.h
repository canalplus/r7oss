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

#ifndef H_BUFFER_INDIVIDUAL_GENERIC
#define H_BUFFER_INDIVIDUAL_GENERIC

/*
 * Control block managing a piece of memory.
 *
 * This class is conditionally thread safe: some of its methods should not be
 * called concurrently.  See method comments for details.
 */
class Buffer_Generic_c : public Buffer_c
{
public:
    Buffer_Generic_c(BufferManager_Generic_t   Manager,
                     BufferPool_Generic_t      Pool,
                     BufferDataDescriptor_t   *Descriptor);
    BufferStatus_t FinalizeInit(BufferDataDescriptor_t *Descriptor);

    //
    // Meta data activities
    //

    BufferStatus_t   AttachMetaData(MetaDataType_t  Type,
                                    unsigned int    Size                      = UNSPECIFIED_SIZE,
                                    void           *MemoryBlock               = NULL,
                                    char           *DeviceMemoryPartitionName = NULL);
    void             DetachMetaData(MetaDataType_t    Type);

    void             ObtainMetaDataReference(MetaDataType_t Type, void **Pointer);

    //
    // Buffer manipulators
    //

    BufferStatus_t   SetUsedDataSize(unsigned int  DataSize);

    BufferStatus_t   ShrinkBuffer(unsigned int     NewSize);
    BufferStatus_t   ExtendBuffer(unsigned int    *NewSize,
                                  bool             ExtendUpwards     = true);

    BufferStatus_t   PartitionBuffer(unsigned int  LeaveInFirstPartitionSize,
                                     bool          DuplicateMetaData,
                                     Buffer_t     *SecondPartition,
                                     unsigned int  SecondOwnerIdentifier = UNSPECIFIED_OWNER,
                                     bool          NonBlocking       = false);

    //
    // Reference manipulation, and ownership control
    //

    BufferStatus_t   RegisterDataReference(unsigned int  BlockSize,
                                           void         *Pointer,
                                           AddressType_t AddressType = CachedAddress);

    BufferStatus_t   RegisterDataReference(unsigned int  BlockSize,
                                           void         *Pointers[3]);

    BufferStatus_t   ObtainDataReference(unsigned int  *BlockSize,
                                         unsigned int  *UsedDataSize,
                                         void         **Pointer,
                                         AddressType_t  AddressType = CachedAddress);

    void TransferOwnership(unsigned int OwnerIdentifier0,
                           unsigned int OwnerIdentifier1  = UNSPECIFIED_OWNER);

    void IncrementReferenceCount(unsigned int OwnerIdentifier  = UNSPECIFIED_OWNER);
    void DecrementReferenceCount(unsigned int OwnerIdentifier  = UNSPECIFIED_OWNER);

    //
    // linking of other buffers to this buffer - for increment/decrement management
    //

    BufferStatus_t   AttachBuffer(Buffer_t Buffer);
    void             DetachBuffer(Buffer_t Buffer);

    void             ObtainAttachedBufferReference(BufferType_t Type, Buffer_t *Buffer);

    //
    // Usage/query/debug methods
    //

    void GetType(BufferType_t    *Type);
    void GetIndex(unsigned int   *Index);

    void GetOwnerCount(unsigned int  *Count);
    void GetOwnerList(unsigned int  ArraySize,
                      unsigned int *ArrayOfOwnerIdentifiers);

    //
    // Status dump/reporting
    //

    void        Dump(unsigned int Flags = DumpAll);
    void        DumpViaRelay(unsigned int id, unsigned int type);

    friend class BufferPool_Generic_c;

private:
    // Owning buffer manager.  Accessed without locking (ptr set during
    // construction, never changed and pointed object provides its own locking).
    BufferManager_Generic_t   Manager;

    // Owning pool.  Accessed without locking (ptr set during construction,
    // never changed and pointed object provides its own locking).
    BufferPool_Generic_t      Pool;

    // Next buffer in Pool->ListOfBuffers.  Protected by Pool->PoolLock.
    Buffer_Generic_t          Next;

    // Accessed without locking (set during construction, never changed).
    unsigned int              Index;

    // Number of references to this buffer.  0 for unused pre-allocated buffers
    // and buffers about to die.  Protected by this->BufferLock.
    unsigned int              ReferenceCount;

    // Keep track for debugging purpose of entities (e.g. edge) having
    // references to this buffer.  Protected by this->BufferLock.
    unsigned int              OwnerIdentifier[MAX_BUFFER_OWNER_IDENTIFIERS];

    // Describe actual memory managed by this buffer.  Protected by
    // Pool->PoolLock except BufferBlock->Descriptor that is immutable and is
    // accessed without locking.
    BlockDescriptor_t         BufferBlock;

    // Linked list of meta data attached to this buffer.  Protected by
    // this->BufferLock.
    BlockDescriptor_t         ListOfMetaData;

    // All buffers this buffer keeps references to.  Protected by
    // this->BufferLock.
    Buffer_t                  AttachedBuffers[MAX_ATTACHED_BUFFERS];

    // Size in bytes of payload.  Protected by Pool->PoolLock because changing
    // it almost always impacts Pool.
    unsigned int              DataSize;

    // Protect some fields of this class.  See per-member comments for details.
    // See BufferPool_Generic_c::PoolLock for lock ordering.
    OS_Mutex_t                BufferLock;

    // Private to prevent direct destruction by clients.
    ~Buffer_Generic_c(void);

    enum LookupMetadataFilter_t
    {
        NOT_ATTACHED_METADATA,  // look up only blocks where AttachedToPool is false
        ALL_METADATA            // look up all blocks
    };
    BlockDescriptor_t  *LookupMetadataPtr(MetaDataType_t Type, LookupMetadataFilter_t Filter);
    BlockDescriptor_t   LookupMetadata(MetaDataType_t Type, LookupMetadataFilter_t Filter);

    bool DetachBufferIfAttached(Buffer_t Buffer);
    bool DetachBufferIfAttachedLocked(Buffer_t Buffer);
    void DumpLocked(unsigned int Flags);

    const char     *GetTypeNameInternal() const;
    BufferType_t    GetTypeInternal() const;

    DISALLOW_COPY_AND_ASSIGN(Buffer_Generic_c);
};

#endif
