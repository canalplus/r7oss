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
#include "allocator_circular.h"

/* report if waiting for memory more then 30s */
#define MAX_EXPECTED_WAIT_FOR_MEM_PERIOD  30000

#undef TRACE_TAG
#define TRACE_TAG "AllocatorCircular_c"

// ------------------------------------------------------------------------
// Constructor function

AllocatorCircular_c::AllocatorCircular_c(unsigned int BufferSize)
{
    OS_InitializeEvent(&EntryFreed);
    this->Size = BufferSize;
    this->OldestBlock = 0;
    this->NewestBlock = 0;
    this->Read = 0;
    this->Write = 0;
    BeingDeleted = false;
    InAllocate   = false;
}

// ------------------------------------------------------------------------
// Destructor function

AllocatorCircular_c::~AllocatorCircular_c(void)
{
    BeingDeleted        = true;
    OS_SetEvent(&EntryFreed);

    while (InAllocate)
    {
        OS_SleepMilliSeconds(1);
    }

    OS_TerminateEvent(&EntryFreed);
}

// ------------------------------------------------------------------------
// Private helper function(s)

unsigned int AllocatorCircular_c::GetFree()
{
    if (Read <= Write)
    {
        return Read + Size - 1 - Write;
    }
    else
    {
        return Read - Write - 1;
    }
}

// ------------------------------------------------------------------------
// Allocate a block

AllocatorStatus_t   AllocatorCircular_c::Allocate(unsigned int    BlockSize,
                                                  unsigned int   *BlockOffset)
{
    // TODO this is W.I.P.: for instance all locking mechanisms to be introduced
    if (this == NULL)
    {
        return AllocatorError;
    }

    if (BlockSize > size - 1)
    {
        SE_ERROR("Could not allocate %u bytes from buffer of %u bytes\n", BlockSize, Size);
        return AllocatorNoMemory;
    }

//
    InAllocate  = true;

    do
    {
        OS_ResetEvent(&EntryFreed);

        if (BeingDeleted)
        {
            break;
        }

        if (BlockSize <= GetFree())
        {
            unsigned int NextBlock = (NewestBlock + 1) % ALLOCATOR_CIRCULAR_MAX_BLOCKS;

            if (NextBlock == OldestBlock)
            {
                SE_ERROR("not enough blocks\n");
                InAllocate  = false;
                return  AllocatorError;
            }

            NewestBlock = NextBlock;
            Blocks[NewestBlock].BlockSize = BlockSize;
            Blocks[NewestBlock].BlockOffset = Write;
            *BlockOffset = Write;
            Write = (Write + BlockSize) % Size;
            InAllocate = false;
            return AllocatorNoError;
        }

#if 0
        SE_INFO(group_buffer, "Waiting for memory ..\n");
#endif

        OS_Status_t WaitStatus = OS_WaitForEventAuto(&EntryFreed, MAX_EXPECTED_WAIT_FOR_MEM_PERIOD);
        if (WaitStatus == OS_TIMED_OUT)
        {
            SE_WARNING("timedout waiting to allocate %u bytes\n", BlockSize);
        }
    }
}
while (!BeingDeleted);

InAllocate   = false;
return AllocatorUnableToAllocate;
}

// ------------------------------------------------------------------------
// Shrink last allocated block

AllocatorStatus_t   AllocatorCircular_c::Shrink(unsigned int     BlockOffset,
                                                unsigned int     BufferSize)
{
    if (BlockOffset != Blocks[NewestBlock].BlockOffset || BufferSize > Blocks[NewestBlock].BufferSize)
    {
        return AllocatorError;
    }

    unsigned int staved = Blocks[NewestBlock].BufferSize - BufferSize;

    if (Write >= staved)
    {
        Write -= staved;
    }
    else
    {
        Write = Write + Size - staved;
    }

    return AllocatorNoError;
}

// ------------------------------------------------------------------------
// Free the oldest allocated block

AllocatorStatus_t   AllocatorCircular_c::Free(unsigned int   BlockOffset)
{
    if (BlockOffset != Blocks[OldestBlock].BlockOffset)
    {
        return AllocatorError;
    }

    Read = (Read + Blocks[OldestBlock].BlockSize) % Size;
    OldestBlock = (OldestBlock + 1) % ALLOCATOR_CIRCULAR_MAX_BLOCKS;
    OS_SetEvent(&EntryFreed);
    return AllocatorNoError;
}
