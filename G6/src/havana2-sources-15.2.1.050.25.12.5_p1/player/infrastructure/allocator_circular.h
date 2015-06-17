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

/*
TODO this is W.I.P.: for instance all locking mechanisms to be introduced

Class implementing a circular buffer. Implicitly, there is only one consumer
and one producer. Allocations are blocking.

Note Allocations are returned as offsets from the base of the buffer
     (IE 0 == the base), this is because the allocator is used to allocate
     blocks from buffers that will have multiple address mappings (physical cached etc)
*/

//
//      <                          size in bytes                            >
//
//      +------------+---------------------------+--------------------------+
//      |            |                           |                          |
//      +------------+---------------------------+--------------------------+
//      ^ base       ^ read                       ^ write
//
// The buffer contains size bytes
// base <= read < base + size, base <= write < base + size
// The "write" pointer points to the first free byte
// read == write   means the buffer is empty
// read == write+1 means the buffer is full
// Note: the busy part of the buffer can contain at most (size - 1) bytes
// busy in bytes: (write - read) modulo size
// free in bytes: (read - write - 1) modulo size
//

#ifndef H_ALLOCATOR_CIRCULAR
#define H_ALLOCATOR_CIRCULAR

#include "osinline.h"
#include "allocator.h"

#define ALLOCATOR_CIRCULAR_MAX_BLOCKS   256

// TODO
typedef struct AllocatorCircularBlock_s
{
    unsigned int     BlockSize;
    unsigned char    BlockOffset;
} AllocatorCircularBlock_t;

//

class AllocatorCircular_c
{
public:
    explicit AllocatorCircular_c(unsigned int  BufferSize);
    ~AllocatorCircular_c(void);


    AllocatorStatus_t Allocate(unsigned int   Size,
                               unsigned int   *BlockOffset);


    AllocatorStatus_t Shrink(unsigned int    BlockOffset,
                             unsigned int    BufferSize);

    AllocatorStatus_t Free(unsigned int  BlockOffset);

private:
    bool             BeingDeleted;
    bool             InAllocate;

    unsigned int     Size;
    AllocatorCircularBlock_t Blocks[ALLOCATOR_CIRCULAR_MAX_BLOCKS];

    // Read side
    unsigned int     OldestBlock;
    unsigned int     Read;

    // Write side
    unsigned int     NewestBlock;
    unsigned int     Write;

    OS_Event_t       EntryFreed;

    unsigned int     GetFree();

    DISALLOW_COPY_AND_ASSIGN(AllocatorCircular_c);
};

#endif // H_ALLOCATOR_CIRCULAR
