/************************************************************************
Copyright (C) 2013 STMicroelectronics. All Rights Reserved.

This file is part of the Streaming Engine Library.

Streaming Engine is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License version 2 as published by the
Free Software Foundation.

Streaming Engine is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with Streaming Engine; see the file COPYING.  If not, write to the Free Software
Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

The Streaming Engine Library may alternatively be licensed under a proprietary
license from ST.
************************************************************************/

//
// Specifies a CRC checker component
// One instance per checked stream
//
// Upstream   API: gets fed with reference CRCs and computed CRCs
// Downstream API: stores and reports CRC check results
//
// Internals: stores ref CRCs and computed CRCs in circular buffers
//

#ifndef H_CHECKER
#define H_CHECKER

#include <linux/types.h>
#include "crc.h"

// Opaque handle to a checker
typedef void* CheckerHandle_t;

typedef enum
{
	CHECKER_NO_ERROR = 0,
	CHECKER_CRC_MISMATCH,
	CHECKER_OUT_OF_MEMORY,
	CHECKER_STOPPED,
	CHECKER_INTERNAL_ERROR,
} CheckerStatus_t;

//
// Upstream API
//

// Allocate a new checker
CheckerStatus_t CHK_Alloc(CheckerHandle_t* handle);

// Tells a checker there won't be anymore reference CRCs
CheckerStatus_t CHK_ShutdownReferences(CheckerHandle_t handle);
// Tells a checker there won't be anymore computed CRCs
CheckerStatus_t CHK_ShutdownComputed(CheckerHandle_t handle);

// Take & Release
CheckerStatus_t CHK_Take(CheckerHandle_t handle);
CheckerStatus_t CHK_Release(CheckerHandle_t handle);

// Adds a CRC reference
// Note: blocking
CheckerStatus_t CHK_AddRefCRC(CheckerHandle_t handle, FrameCRC_t* crc);

// Checks a computed PP CRC against its ref
// Note: blocking until shutdown
CheckerStatus_t CHK_CheckPPCRC(CheckerHandle_t handle, FrameCRC_t* computed_crc);

// Checks a computed decode CRC against its ref
// Note: blocking until shutdown
CheckerStatus_t CHK_CheckDecoderCRC(CheckerHandle_t handle, FrameCRC_t* decoder_computed_crc);

//
// Downstream API
//

// Returns a computed CRC
// Note: blocking until shutdown
CheckerStatus_t CHK_GetComputedCRC(CheckerHandle_t handle, FrameCRC_t* computed_crc);

#endif // H_CHECKER
