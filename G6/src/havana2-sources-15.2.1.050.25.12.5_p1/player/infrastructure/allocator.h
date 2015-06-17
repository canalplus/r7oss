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

#ifndef H_ALLOCATOR
#define H_ALLOCATOR

typedef enum
{
    AllocatorNoError            = 0,
    AllocatorError,
    AllocatorNoMemory,
    AllocatorUnableToAllocate
} AllocatorStatus_t;

class Allocator_c
{
public:
    AllocatorStatus_t  InitializationStatus;

    Allocator_c(void) : InitializationStatus(AllocatorNoError) {};
    virtual ~Allocator_c(void) {};

    virtual AllocatorStatus_t Allocate(unsigned int   *Size,
                                       unsigned char **Block,
                                       bool            AllocateLargest,
                                       bool            NonBlocking   = false) = 0;

    virtual AllocatorStatus_t ExtendToLargest(
        unsigned int   *Size,
        unsigned char **Block,
        bool            ExtendUpwards = true) = 0;

    virtual AllocatorStatus_t Free(void) = 0;

    virtual AllocatorStatus_t Free(unsigned int   Size,
                                   unsigned char *Block) = 0;

    virtual AllocatorStatus_t LargestFreeBlock(unsigned int  *Size) = 0;
};
#endif
