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

/************************************************************************
Note this is the simplest of implementations, it is definitely not the best
Note Allocations are returned as offsets from the base of the buffer
     (IE NULL == the base), this is because the allocator is used to allocate
     blocks from buffers that will have multiple address mappings (physical cached etc)
Note this implementation will not return a block of memory that crosses a 64Mb
     physical address boundary
************************************************************************/

#ifndef H_ALLOCATOR_SIMPLE
#define H_ALLOCATOR_SIMPLE

#include "osinline.h"
#include "allocator.h"

//#define ALLOCATOR_SIMPLE_MAX_BLOCKS   32
#define ALLOCATOR_SIMPLE_MAX_BLOCKS 256

typedef struct AllocatorSimpleBlock_s
{
    bool           InUse;
    unsigned int   Size;
    unsigned char *Base;
} AllocatorSimpleBlock_t;


class AllocatorSimple_c : public Allocator_c
{
public:
    AllocatorSimple_c(unsigned int   BufferSize,
                      unsigned int   SegmentSize,
                      unsigned char *PhysicalAddress,
                      bool           AllowCross64MbBoundary = true);
    ~AllocatorSimple_c(void);

    AllocatorStatus_t Allocate(unsigned int   *Size,
                               unsigned char **Block,
                               bool            AllocateLargest,
                               bool            NonBlocking   = false);

    AllocatorStatus_t ExtendToLargest(
        unsigned int    *Size,
        unsigned char  **Block,
        bool             ExtendUpwards = true);

    AllocatorStatus_t Free();

    AllocatorStatus_t Free(unsigned int   Size,
                           unsigned char *Block);

    AllocatorStatus_t LargestFreeBlock(unsigned int *Size);

private:
    bool             mBeingDeleted;
    bool             mInAllocate;

    unsigned int     mBufferSize;
    unsigned int     mSegmentSize;
    unsigned char   *mPhysicalAddress;
    unsigned int     mHighestUsedBlockIndex;
    AllocatorSimpleBlock_t   mBlocks[ALLOCATOR_SIMPLE_MAX_BLOCKS];
    unsigned int    mMaxPossibleBlockSize;
    bool            mAllowCross64MbBoundary;

    OS_Mutex_t      mLock;
    OS_Event_t      mEntryFreed;
    OS_Event_t      mExitAllocate;

    int  LargestFreeBlock(void);
    void TraceBlocks(void);

    DISALLOW_COPY_AND_ASSIGN(AllocatorSimple_c);
};

#endif
