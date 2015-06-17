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

#ifndef H_BUFFER
#define H_BUFFER
#include "allocatorio.h"
#include "player_types.h"
#include "base_component_class.h"

#ifndef NOT_SPECIFIED
#define NOT_SPECIFIED           0xffffffff
#endif

#define UNRESTRICTED_NUMBER_OF_BUFFERS  NOT_SPECIFIED
#define UNSPECIFIED_SIZE        NOT_SPECIFIED
#define UNSPECIFIED_OWNER       NOT_SPECIFIED

enum
{
    BufferNoError               = PlayerNoError,
    BufferError                 = PlayerError,

    BufferBlockingCallAborted           = BASE_BUFFER,

    BufferInsufficientMemoryGeneral,
    BufferInsufficientMemoryForPool,
    BufferInsufficientMemoryForBuffer,
    BufferInsufficientMemoryForMetaData,

    BufferUnsupportedAllocationSource,
    BufferInvalidDescriptor,

    BufferNoDataAttached,

    BufferTooManyDataTypes,
    BufferDataTypeNotFound,
    BufferMetaDataTypeNotFound,

    BufferParametersIncompatibleWithAllocationSource,
    BufferOperationNotSupportedByThisDescriptor,

    BufferSizeIncompatibleWithDescriptor,
    BufferPoolNotFound,

    BufferFailedToCreateBuffer,
    BufferNoFreeBufferAvailable,

    BufferNonZeroReferenceCount,

    BufferNotInUse,

    BufferTooManyAttachments,
    BufferAttachmentNotFound
};

typedef PlayerStatus_t  BufferStatus_t;

typedef enum
{
    CachedAddress   = 0,
    PhysicalAddress = 1
} AddressType_t;

typedef enum
{
    DumpBufferTypes     = 1,
    DumpMetaDataTypes   = 2,
    DumpListPools       = 4,
    DumpPoolStates      = 8,
    DumpBufferStates    = 16
} DumpTypeMask_t;

#define DumpAll     0xffffffff

//
// Buffer types
//

typedef unsigned int    BufferType_t;
#define MetaDataType_t  BufferType_t

typedef  class Buffer_c         *Buffer_t;
typedef  class BufferPool_c     *BufferPool_t;
typedef  class BufferManager_c  *BufferManager_t;

#include "buffer_individual.h"
#include "buffer_pool.h"
#include "buffer_manager.h"

#endif
