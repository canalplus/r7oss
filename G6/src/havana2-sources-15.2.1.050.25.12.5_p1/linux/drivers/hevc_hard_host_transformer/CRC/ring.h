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
// CRC circular buffer
//

#ifndef H_RING
#define H_RING

#include <linux/semaphore.h>
#include "crc.h"

typedef struct
{
	FrameCRC_t* crcs;
	int stopping; // true after Shutdown()
	int size;
	int write_pointer;
	int read_pointer;
	struct semaphore readable;
	struct semaphore writable;
} CRCRing_t;

typedef enum
{
	RING_NO_ERROR = 0,
	RING_EMPTY,
	RING_OUT_OF_MEMORY,
	RING_STOPPED,
	RING_INTERNAL_ERROR,
} CRCRingStatus_t;

CRCRingStatus_t RingAlloc   (CRCRing_t* ring, int size);
CRCRingStatus_t RingWriteCRC(CRCRing_t* ring, FrameCRC_t* crc); // blocking
CRCRingStatus_t RingReadCRC (CRCRing_t* ring, FrameCRC_t* crc); // blocking until shutdown
CRCRingStatus_t RingShutdown(CRCRing_t* ring); // specifies that there won't be any more writes
CRCRingStatus_t RingFree    (CRCRing_t* ring);

#endif // H_RING
