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
// Implementation of the CRC circular buffers
//

#include "ring.h"
#include "crc.h"
#include "osdev_mem.h"

CRCRingStatus_t RingAlloc(CRCRing_t* ring, int size)
{
	 sema_init(&ring->readable, 0);
	 sema_init(&ring->writable, 0);

	 ring->crcs = OSDEV_Malloc(size * sizeof(FrameCRC_t));
	 if (ring->crcs == NULL)
		 return RING_OUT_OF_MEMORY; // FIXME semaphore leak

	 ring->stopping = 0;
	 ring->size = size;
	 ring->write_pointer = 0;
	 ring->read_pointer = 0;
	 return RING_NO_ERROR;
}

CRCRingStatus_t RingWriteCRC(CRCRing_t* ring, FrameCRC_t* crc)
{
	int was_empty = ring->write_pointer == ring->read_pointer;
	int new_write_pointer = (ring->write_pointer + 1) % ring->size;

	if (ring->stopping)
		return RING_STOPPED;

	if (new_write_pointer == ring->read_pointer) // ring is full
	{
		if (down_interruptible(&ring->writable) != 0)
			return RING_INTERNAL_ERROR;  // interrupted
		if (ring->stopping)
			return RING_STOPPED;
		if (new_write_pointer == ring->read_pointer) // still full
			return RING_INTERNAL_ERROR;
	}
	ring->crcs[ring->write_pointer] = *crc;
	ring->write_pointer = new_write_pointer;
	if (was_empty)
		up(&ring->readable);
	 return RING_NO_ERROR;
}

CRCRingStatus_t RingReadCRC (CRCRing_t* ring, FrameCRC_t* crc)
{
	int was_full = (((ring->write_pointer + 1) % ring->size) == ring->read_pointer);

	if (ring->read_pointer == ring->write_pointer) // ring empty
	{
		if (ring->stopping)
			return RING_EMPTY;
		if (down_interruptible(&ring->readable) != 0)
			return RING_INTERNAL_ERROR;  // interrupted
		if (ring->stopping)
			return RING_EMPTY;
		if (ring->read_pointer == ring->write_pointer) // ring still empty
			return RING_INTERNAL_ERROR;
	}
	*crc = ring->crcs[ring->read_pointer];
	ring->read_pointer = (ring->read_pointer + 1) % ring->size;
	if (was_full)
		up(&ring->writable);
	 return RING_NO_ERROR;
}

CRCRingStatus_t RingShutdown(CRCRing_t* ring)
{
	ring->stopping = 1;
	up(&ring->readable);
	up(&ring->writable);
	return RING_NO_ERROR;
}

CRCRingStatus_t RingFree(CRCRing_t* ring)
{
	up(&ring->readable);
	up(&ring->writable);
	OSDEV_Free(ring->crcs);
	return RING_NO_ERROR;
}
