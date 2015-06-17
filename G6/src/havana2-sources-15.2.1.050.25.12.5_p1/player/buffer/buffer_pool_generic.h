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

#ifndef H_BUFFER_POOL_GENERIC
#define H_BUFFER_POOL_GENERIC

struct BlockDescriptor_s
{
    BlockDescriptor_t         Next;
    BufferDataDescriptor_t   *Descriptor;
    bool                      AttachedToPool;

    unsigned int              Size;
    void                     *Address[3];  // 3 address types (cached uncached physical) - only cached for meta data
    // TODO(pht) move to 2: using only cached & physical

    allocator_device_t        MemoryAllocatorDevice;
    unsigned int              PoolAllocatedOffset;

    BlockDescriptor_s(BlockDescriptor_t next,
                      BufferDataDescriptor_t *descriptor,
                      bool attachedtopool,
                      unsigned int size,
                      allocator_device_t memoryallocatordevice)
        : Next(next)
        , Descriptor(descriptor)
        , AttachedToPool(attachedtopool)
        , Size(size)
        , Address()
        , MemoryAllocatorDevice(memoryallocatordevice)
        , PoolAllocatedOffset(0)
    {}



};

class BufferPool_Generic_c : public BufferPool_c
{
public:
    BufferPool_Generic_c(BufferManager_Generic_t   Manager,
                         BufferDataDescriptor_t   *Descriptor,
                         unsigned int              NumberOfBuffers,
                         unsigned int              Size,
                         void                     *MemoryPool[3],
                         void                     *ArrayOfMemoryBlocks[][3],
                         char                     *DeviceMemoryPartitionName,
                         bool                      AllowCross64MbBoundary = true,
                         unsigned int              MemoryAccessType = MEMORY_DEFAULT_ACCESS);
    BufferStatus_t FinalizeInit(void  *MemoryPool[3],
                                void  *ArrayOfMemoryBlocks[][3],
                                bool   AllowCross64MbBoundary);
    ~BufferPool_Generic_c(void);

    //
    // Global operations on all pool members
    //
    BufferStatus_t   AttachMetaData(MetaDataType_t  Type,
                                    unsigned int    Size                      = UNSPECIFIED_SIZE,
                                    void           *MemoryPool                = NULL,
                                    void           *ArrayOfMemoryBlocks[]     = NULL,
                                    char           *DeviceMemoryPartitionName = NULL);
    void             DetachMetaData(MetaDataType_t  Type);

    //
    // Get/Release a buffer - overloaded get
    //

    /**
     * @param Buffer
     * @param OwnerIdentifier
     * @param RequiredSize
     * @param NonBlocking
     * @param RequiredSizeIsLowerBound
     * @param TimeOut in ms, 0 means blocking for ever: use
     *                NonBlocking for non-blocking
     *
     * @return BufferStatus_t
     */
    BufferStatus_t   GetBuffer(Buffer_t      *Buffer,
                               unsigned int   OwnerIdentifier          = UNSPECIFIED_OWNER,
                               unsigned int   RequiredSize             = UNSPECIFIED_SIZE,
                               bool           NonBlocking              = false,
                               bool           RequiredSizeIsLowerBound = false,
                               uint32_t       TimeOut                  = 0);

    void             AbortBlockingGetBuffer(void);

    void             SetMemoryAccessType(unsigned int);
    void             DetachBuffer(Buffer_Generic_t Buffer);

    //
    // Usage/query/debug methods
    //

    void            GetType(BufferType_t *Type);

    void            GetPoolUsage(unsigned int   *BuffersInPool,
                                 unsigned int   *BuffersWithNonZeroReferenceCount  = NULL,
                                 unsigned int   *MemoryInPool                      = NULL,
                                 unsigned int   *MemoryAllocated                   = NULL,
                                 unsigned int   *MemoryInUse                       = NULL,
                                 unsigned int   *LargestFreeMemoryBlock            = NULL);

    BufferStatus_t  GetAllUsedBuffers(unsigned int   ArraySize,
                                      Buffer_t      *ArrayOfBuffers,
                                      unsigned int   OwnerIdentifier);

    //
    // Status dump/reporting
    //

    void         Dump(unsigned int    Flags = DumpAll);

    friend class BufferManager_Generic_c;
    friend class Buffer_Generic_c;

private:
    BufferManager_Generic_t mManager;
    BufferPool_Generic_t    Next;

    // Protect members from this class and some members of Buffer_Generic_c
    // objects carved from this pool.  See per-member comments for details.
    // This lock must not be held when waiting for memory during out-of-memory
    // or a deadlock occurs with threads releasing buffers.  The lock ordering
    // is this lock before Buffer_Generic_c::BufferLock.
    OS_Mutex_t              PoolLock;

    BufferDataDescriptor_t *mBufDataDescriptor;
    unsigned int            mNumberOfBuffers;
    unsigned int            CountOfBuffers;       // In effect when NumberOfBuffers == NOT_SPECIFIED
    unsigned int            Size;

    // All buffers in this pool.  Buffers in this list may have ReferenceCount
    // == 0.  Protected by this->PoolLock.
    Buffer_Generic_t        ListOfBuffers;

    Ring_c                 *FreeBuffer;
    void                   *mMemoryPool[3];        // If memory pool is specified, its three addresses
    Allocator_c            *MemoryPoolAllocator;
    allocator_device_t      MemoryPoolAllocatorDevice;
    char                    MemoryPartitionName[ALLOCATOR_MAX_PARTITION_NAME_SIZE];
    unsigned int            mMemoryAccessType;

    BlockDescriptor_t       BufferBlock;
    BlockDescriptor_t       ListOfMetaData;

    bool                    AbortGetBuffer;       // Flag to abort a get buffer call
    bool                    BufferReleaseSignalWaitedOn;
    OS_Event_t              BufferReleaseSignal;

    unsigned int            CountOfReferencedBuffers; // Statistics (held rather than re-calculated every request)
    unsigned int            TotalAllocatedMemory;
    unsigned int            TotalUsedMemory;

    // Functions

    void            TidyUp(void);

    BufferStatus_t  CheckMemoryParameters(BufferDataDescriptor_t   *Descriptor,
                                          bool                      ArrayAllocate,
                                          unsigned int              Size,
                                          void                     *MemoryPool,
                                          void                     *ArrayOfMemoryBlocks,
                                          char                     *MemoryPartitionName,
                                          const char               *Caller,
                                          unsigned int             *ItemSize = NULL);

    BufferStatus_t  AllocateMemoryBlock(BlockDescriptor_t  Block,
                                        bool               ArrayAllocate,
                                        unsigned int       Index,
                                        Allocator_c       *PoolAllocator,
                                        void              *MemoryPool,
                                        void              *ArrayOfMemoryBlocks,
                                        char              *MemoryPartitionName,
                                        const char        *Caller,
                                        bool               RequiredSizeIsLowerBound = false,
                                        unsigned int       MemoryAccessType = MEMORY_DEFAULT_ACCESS,
                                        bool               NonBlocking = false);

    void            DeAllocateMemoryBlock(BlockDescriptor_t   Block);

    BufferStatus_t  ShrinkMemoryBlock(BlockDescriptor_t   Block,
                                      unsigned int        NewSize);
    BufferStatus_t  ExtendMemoryBlock(BlockDescriptor_t   Block,
                                      unsigned int       *NewSize,
                                      bool                ExtendUpwards);

    void DumpLocked(unsigned int Flags);

    void ReleaseBuffer(Buffer_t Buffer);

    DISALLOW_COPY_AND_ASSIGN(BufferPool_Generic_c);
};

#endif
