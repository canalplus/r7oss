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

#include "report.h"
#include "allocator_simple.h"

#undef TRACE_TAG
#define TRACE_TAG "AllocatorSimple_c"

// ------------------------------------------------------------------------
// Macro

#define On64MBBoundary( addr ) ((!mAllowCross64MbBoundary) && ((((uintptr_t)(addr) + (uintptr_t)mPhysicalAddress) & 0x03ffffff) == 0))

/* report if waiting for memory more then 30s */
#define MAX_EXPECTED_WAIT_FOR_MEM_PERIOD  30000

/* report if waiting for Allocate exit more then 500ms */
#define MAX_EXPECTED_WAIT_FOR_ALLOCATE_EXIT  500


// ------------------------------------------------------------------------
// Constructor function
AllocatorSimple_c::AllocatorSimple_c(unsigned int     BufferSize,
                                     unsigned int     SegmentSize,
                                     unsigned char   *PhysicalAddress,
                                     bool             AllowCross64MbBoundary)
    : Allocator_c()
    , mBeingDeleted(false)
    , mInAllocate(false)
    , mBufferSize(BufferSize)
    , mSegmentSize(SegmentSize)
    , mPhysicalAddress(PhysicalAddress)
    , mHighestUsedBlockIndex(0)
    , mBlocks()
    , mMaxPossibleBlockSize(0)
    , mAllowCross64MbBoundary(AllowCross64MbBoundary)
    , mLock()
    , mEntryFreed()
    , mExitAllocate()
{
    OS_InitializeMutex(&mLock);
    OS_InitializeEvent(&mEntryFreed);
    OS_InitializeEvent(&mExitAllocate);

    SE_VERBOSE(group_buffer, "allocator 0x%p BufferSize %d SegmentSize %d\n",
               this, mBufferSize, mSegmentSize);

    Free();
}

// ------------------------------------------------------------------------
// Destructor function

AllocatorSimple_c::~AllocatorSimple_c(void)
{
    SE_VERBOSE(group_buffer, ">allocator 0x%p\n", this);

    OS_LockMutex(&mLock);
    mBeingDeleted = true;
    OS_SetEvent(&mEntryFreed);
    OS_UnLockMutex(&mLock);

    if (mInAllocate)
    {
        unsigned long long EntryTime = OS_GetTimeInMicroSeconds();
        SE_INFO(group_buffer, "allocator 0x%p Waiting to exit allocate\n", this);

        OS_Status_t WaitStatus;
        do
        {
            WaitStatus = OS_WaitForEventAuto(&mExitAllocate, MAX_EXPECTED_WAIT_FOR_ALLOCATE_EXIT);
            if (WaitStatus == OS_TIMED_OUT)
            {
                SE_WARNING("allocator 0x%p still waiting to exit allocate\n", this);
            }
        }
        while (WaitStatus == OS_TIMED_OUT);

        SE_INFO(group_buffer, "allocator 0x%p Time to exit allocate %llu\n", this, OS_GetTimeInMicroSeconds() - EntryTime);
    }

    OS_TerminateEvent(&mEntryFreed);
    OS_TerminateEvent(&mExitAllocate);
    OS_TerminateMutex(&mLock);
    SE_VERBOSE(group_buffer, "<allocator 0x%p\n", this);
}

// ------------------------------------------------------------------------
// Allocate function

AllocatorStatus_t   AllocatorSimple_c::Allocate(
    unsigned int     *Size,
    unsigned char   **Block,
    bool              AllocateLargest,
    bool              NonBlocking)
{
    if (*Size > mMaxPossibleBlockSize)
    {
        SE_ERROR("allocator 0x%p Could not allocate %d bytes from buffer of %d bytes with MaxPossibleBlockSize %d\n",
                 this, *Size, mBufferSize, mMaxPossibleBlockSize);
        return AllocatorNoMemory;
    }

    OS_LockMutex(&mLock);
    SE_VERBOSE(group_buffer, ">allocator 0x%p Block 0x%p Size %d HighestUsedBlockIndex %d\n",
               this, *Block, *Size, mHighestUsedBlockIndex);
    OS_ResetEvent(&mExitAllocate);
    mInAllocate  = true;

    if (!AllocateLargest)
    {
        if (mSegmentSize == 0)
        {
            SE_ERROR("Can not compute block size (mSegmentSize == 0)\n");
            goto exit_no_error;
        }
        *Size = (((*Size + mSegmentSize - 1) / mSegmentSize) * mSegmentSize);
    }

    do
    {
        OS_ResetEvent(&mEntryFreed);

        if (mBeingDeleted)
        {
            SE_INFO(group_buffer, "allocator 0x%p is being deleted\n", this);
            // Need to unblock destructor waiting for Allocate exit
            OS_SetEvent(&mExitAllocate);
            break;
        }

        if (AllocateLargest)
        {
            int i   = LargestFreeBlock();

            if ((i >= 0) && (mBlocks[i].Size >= (*Size)))
            {
                *Size  = mBlocks[i].Size;
                *Block = mBlocks[i].Base;
                mBlocks[i].Size  = 0;
                mBlocks[i].InUse = false;
                SE_VERBOSE(group_buffer, "<allocator 0x%p Size %d LargestFreeBlock block[%d]\n",
                           this, *Size, i);
                goto exit_no_error;
            }
        }
        else
        {
            for (unsigned int i = 0; i < (mHighestUsedBlockIndex + 1); i++)
                if (mBlocks[i].InUse && (mBlocks[i].Size >= *Size))
                {
                    *Block         = mBlocks[i].Base;
                    mBlocks[i].Base += *Size;
                    mBlocks[i].Size -= *Size;

                    if (mBlocks[i].Size == 0)
                    {
                        mBlocks[i].InUse = false;
                    }

                    SE_VERBOSE(group_buffer, "<allocator 0x%p Size %d HighestUsedBlockIndex %d block[%d].Size %d\n",
                               this, *Size, mHighestUsedBlockIndex, i, mBlocks[i].Size);
                    goto exit_no_error;
                }
        }

        SE_VERBOSE(group_buffer, "allocator 0x%p Waiting for memory size %d\n", this, *Size);
        OS_UnLockMutex(&mLock);

        if (!NonBlocking)
        {
            OS_Status_t WaitStatus;
            do
            {
                WaitStatus = OS_WaitForEventAuto(&mEntryFreed, MAX_EXPECTED_WAIT_FOR_MEM_PERIOD);
                if (WaitStatus == OS_TIMED_OUT)
                {
                    SE_WARNING("allocator 0x%p still waiting to allocate %d bytes\n", this, *Size);
                    TraceBlocks();
                }
            }
            while (WaitStatus == OS_TIMED_OUT && mBeingDeleted == false);
        }

        OS_LockMutex(&mLock);
    }
    while (!NonBlocking);

    mInAllocate   = false;
    SE_INFO(group_buffer, "<allocator 0x%p AllocatorUnableToAllocate\n", this);
    OS_UnLockMutex(&mLock);
    return AllocatorUnableToAllocate;

exit_no_error:
    mInAllocate       = false;
    OS_UnLockMutex(&mLock);
    return AllocatorNoError;
}

// ------------------------------------------------------------------------
// Extend an allocation to its largest available size (either adding at the end or beginning)
// NOTE we practically split this fn into two separate implementations depending on which
// way we want to extend.

AllocatorStatus_t   AllocatorSimple_c::ExtendToLargest(
    unsigned int     *Size,
    unsigned char   **Block,
    bool          ExtendUpwards)
{
    unsigned int     i;
    unsigned char   *BoundaryToExtend;
    SE_VERBOSE(group_buffer, ">allocator 0x%p Block 0x%p Size %d HighestUsedBlockIndex %d\n",
               this, *Block, *Size, mHighestUsedBlockIndex);

    if (ExtendUpwards)
    {
        //
        // First check to see if extension is even possible
        //
        BoundaryToExtend = *Block + *Size;

        if (On64MBBoundary(BoundaryToExtend) || (BoundaryToExtend >= (mPhysicalAddress + mBufferSize)))
        {
            SE_DEBUG(group_buffer, "allocator 0x%p Not enough memory to extend boundary\n", this);
            return AllocatorNoMemory;
        }

        //
        // Now scan for an adjacent block
        //
        OS_LockMutex(&mLock);

        for (i = 0; i < (mHighestUsedBlockIndex + 1); i++)
            if (mBlocks[i].InUse && (BoundaryToExtend == mBlocks[i].Base))
            {
                *Size       += mBlocks[i].Size;
                mBlocks[i].Size   = 0;
                mBlocks[i].InUse  = false;
                OS_UnLockMutex(&mLock);
                return AllocatorNoError;
            }
    }
    else
    {
        //
        // First check to see if extension is even possible
        //
        BoundaryToExtend    = *Block;

        if (On64MBBoundary(BoundaryToExtend) || (BoundaryToExtend == mPhysicalAddress))
        {
            SE_DEBUG(group_buffer, "allocator 0x%p Not enough memory to extend boundary\n", this);
            return AllocatorNoMemory;
        }

        //
        // Now scan for an adjacent block
        //
        OS_LockMutex(&mLock);

        for (i = 0; i < (mHighestUsedBlockIndex + 1); i++)
            if (mBlocks[i].InUse && (BoundaryToExtend == (mBlocks[i].Base + mBlocks[i].Size)))
            {
                *Size       += mBlocks[i].Size;
                *Block       = mBlocks[i].Base;
                mBlocks[i].Size   = 0;
                mBlocks[i].InUse  = false;
                OS_UnLockMutex(&mLock);
                return AllocatorNoError;
            }
    }

    SE_DEBUG(group_buffer, "<allocator 0x%p AllocatorUnableToAllocate\n", this);
    OS_UnLockMutex(&mLock);
    return AllocatorUnableToAllocate;
}

// ------------------------------------------------------------------------
// Free the whole block, note we partition this into blocks that do not
// cross a 64Mb boundary in the physical address space.

AllocatorStatus_t   AllocatorSimple_c::Free(void)
{
    int              i;
    unsigned int     LocalBufferSize;
    unsigned int     MaxBlockSize;
    unsigned char    *LocalOffset;

    OS_LockMutex(&mLock);

    memset(mBlocks, 0, sizeof(mBlocks));
    LocalBufferSize     = mBufferSize;
    LocalOffset         = NULL;
    MaxBlockSize        = 0x4000000 - ((uintptr_t)mPhysicalAddress & 0x03ffffff);
    mHighestUsedBlockIndex   = 0;
    mMaxPossibleBlockSize    = 0;

    if (mAllowCross64MbBoundary == true)
    {
        mBlocks[0].InUse      = true;
        mBlocks[0].Size       = mBufferSize;
        mBlocks[0].Base       = NULL;
        mHighestUsedBlockIndex = 0;
        mMaxPossibleBlockSize  = mBlocks[0].Size;
    }
    else
    {
        for (i = 0; ((LocalBufferSize != 0) && (i < ALLOCATOR_SIMPLE_MAX_BLOCKS)); i++)
        {
            mBlocks[i].InUse      = true;
            mBlocks[i].Size       = min(LocalBufferSize, MaxBlockSize);
            mBlocks[i].Base       = LocalOffset;
            LocalBufferSize     -= mBlocks[i].Size;
            LocalOffset         += mBlocks[i].Size;
            MaxBlockSize         = 0x4000000;
            mHighestUsedBlockIndex = i;
            mMaxPossibleBlockSize  = max(mMaxPossibleBlockSize, mBlocks[i].Size);
        }
    }

    SE_VERBOSE(group_buffer, "allocator 0x%p BufferSize %d HighestUsedBlockIndex %d MaxPossibleBlockSize: %d mBlocks[%d].Size:%d\n",
               this, mBufferSize, mHighestUsedBlockIndex, mMaxPossibleBlockSize, mHighestUsedBlockIndex, mBlocks[mHighestUsedBlockIndex].Size);

    OS_SetEvent(&mEntryFreed);
    OS_UnLockMutex(&mLock);

    return AllocatorNoError;
}

// ------------------------------------------------------------------------
// Free range of entries

AllocatorStatus_t   AllocatorSimple_c::Free(unsigned int      Size,
                                            unsigned char    *Block)
{
    unsigned int    i;
    unsigned int    LowestFreeBlock;
    unsigned int    NextHighestUsedBlockIndex;
    //

    OS_LockMutex(&mLock);
    SE_VERBOSE(group_buffer, ">allocator 0x%p Block 0x%p Size %d HighestUsedBlockIndex %d\n",
               this, Block, Size, mHighestUsedBlockIndex);
    if (mSegmentSize == 0)
    {
        OS_UnLockMutex(&mLock);
        SE_ERROR("Can not compute block size (mSegmentSize == 0)\n");
        return AllocatorError;
    }
    Size                    = (((Size + mSegmentSize - 1) / mSegmentSize) * mSegmentSize);
    LowestFreeBlock         = mHighestUsedBlockIndex + 1;
    NextHighestUsedBlockIndex   = 0;

    //
    // Note by adding adjacent block records to the one we are trying to free,
    // this loop does concatenation of free blocks as well as freeing the
    // current block.
    //

    for (i = 0; i < (mHighestUsedBlockIndex + 1); i++)
    {
        if (mBlocks[i].InUse)
        {
            if (((Block + Size) == mBlocks[i].Base) &&
                !On64MBBoundary(mBlocks[i].Base))
            {
                Size            += mBlocks[i].Size;
                mBlocks[i].InUse  = false;
            }
            else if ((Block == (mBlocks[i].Base + mBlocks[i].Size)) &&
                     !On64MBBoundary(Block))
            {
                Size            += mBlocks[i].Size;
                Block            = mBlocks[i].Base;
                mBlocks[i].InUse  = false;
            }
            else
            {
                NextHighestUsedBlockIndex   = i;
            }
        }

        if (!mBlocks[i].InUse)
        {
            LowestFreeBlock      = min(LowestFreeBlock, i);
        }
    }

    //

    if (LowestFreeBlock == ALLOCATOR_SIMPLE_MAX_BLOCKS)
    {
        SE_ERROR("allocator 0x%p Unable to create a free block - too many\n", this);
        TraceBlocks();
        OS_SetEvent(&mEntryFreed);
        OS_UnLockMutex(&mLock);
        return AllocatorError;
    }

    //
    mBlocks[LowestFreeBlock].InUse       = true;
    mBlocks[LowestFreeBlock].Size        = Size;
    mBlocks[LowestFreeBlock].Base        = Block;
    mHighestUsedBlockIndex       = max(LowestFreeBlock, NextHighestUsedBlockIndex);
    SE_VERBOSE(group_buffer, "<allocator 0x%p Size %d HighestUsedBlockIndex %d mBlocks[LowestFreeBlock %d].Size %d\n",
               this, Size, mHighestUsedBlockIndex, LowestFreeBlock, mBlocks[LowestFreeBlock].Size);
    OS_SetEvent(&mEntryFreed);
    OS_UnLockMutex(&mLock);
    return AllocatorNoError;
}


// ------------------------------------------------------------------------
// Inform the caller of the size of the largest free block

AllocatorStatus_t   AllocatorSimple_c::LargestFreeBlock(unsigned int     *Size)
{
    int Index;
    OS_LockMutex(&mLock);
    Index   = LargestFreeBlock();
    *Size   = (Index < 0) ? 0 : mBlocks[Index].Size;
    OS_UnLockMutex(&mLock);
    return AllocatorNoError;
}


// ------------------------------------------------------------------------
// Private - internal function to find the largest free block

int   AllocatorSimple_c::LargestFreeBlock(void)
{
    unsigned int    i;
    int     Index;
    //
    Index   = -1;

    for (i = 0; i < (mHighestUsedBlockIndex + 1); i++)
        if (mBlocks[i].InUse && ((Index < 0) || (mBlocks[i].Size > mBlocks[Index].Size)))
        {
            Index = i;
        }

    return Index;
}

void AllocatorSimple_c::TraceBlocks(void)
{
    unsigned int    i;
    SE_INFO(group_buffer, "allocator 0x%p HighestUsedBlockIndex %d\n", this, mHighestUsedBlockIndex);

    for (i = 0; i < ALLOCATOR_SIMPLE_MAX_BLOCKS; i++)
        if (mBlocks[i].InUse)
        {
            SE_INFO(group_buffer, "mBlocks[%d] Base 0x%p Size %d\n", i, mBlocks[i].Base, mBlocks[i].Size);
        }
}


