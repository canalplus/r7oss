/************************************************************************
Copyright (C) 2011 STMicroelectronics. All Rights Reserved.

This file is part of the Transport Engine Library.

The Transport Engine is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

Transport Engine is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with Transport Engine; see the file COPYING.  If not, write to the Free
Software Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307,
USA.

The Transport Engine Library may alternatively be licensed under a
proprietary license from ST.

************************************************************************/
/**
   @file   pti_buffer.h
   @brief  Buffer Object Initialisation, Termination and Manipulation Functions.

   This file declares the object structure and access mechanism for the Buffer
   Object.

   This file should ONLY contain function prototypes and constants that are
   INTERNAL to the stptiHAL.  i.e. with the exception of the HAL object
   registration function in pti_driver.c, NOTHING outside of the tango
   directory should be referring to this file.

   Defines and enums relevent to the API of the HAL must be put into
   pti_hal_api.h and will be shared across all HALs.

 */

#ifndef _PTI_BUFFER_H_
#define _PTI_BUFFER_H_

/* Includes ---------------------------------------------------------------- */

/* ANSI C includes */

/* STAPI includes */

/* Includes from API level */
#include "../pti_osal.h"
#include "../pti_hal_api.h"

/* Includes from the HAL / ObjMan level */
#include "../objman/pti_object.h"
#include "firmware/pti_tp_api.h"

/* Exported Types ---------------------------------------------------------- */

typedef struct {
	Object_t ObjectHeader;	/* Must be first in structure */
	BOOL ManuallyAllocatedBuffer;			/**< Is this buffer a user (manually) allocated buffer */
	BOOL DriverHasMappedBuffer;			/**< Has the driver mapped the buffer? */
	void *RealBufferStart;				/**< The start of the buffer */
	U8 *Buffer_p;					/**< The aligned start of the buffer (virtual address) */
	U32 PhysicalBuffer_p;				/**< The aligned start of the buffer (physical address) */
	U32 BufferSize;					/**< The size of the buffer */
	U32 ReadOffset;					/**< The (cached) offset of the Read pointer into the buffer (always kept in sync with read pointer) */
	U32 QWriteOffset;				/**< The (cached) offset of the QWrite pointer into the buffer (only to used when disassoc'd from a slot) */
	U32 BufferUnitCount;				/**< The (cached) Buffer Unit Count (only to used when disassoc'd from a slot) */
	int BufferIndex;				/**< Which TP DMA channel */
	int SlotCount;					/**< Number of Slots associated (-1 means no slots have ever been associated) */
	int IndexerCount;				/**< Number of Indexers associated (-1 means no indexers have ever been associated) */
	stptiHAL_SlotMode_t AssociatedSlotsMode;	/**< Associated slots mode (to save time looking up) */
	int AssociatedSlotsWithMetaDataSuppressed;	/**< a count of the slot which have suppressed metadata (used to block read fn's) */
	int AssociatedSlotsWithSoftwareCDFifo;		/**< a count of the slots which have software CD Fifo set (used to block read fn's) */
	U32 SignallingUpperThreshold;			/**< The "fullness" at which the TP will signal (TP signals when moving from below to above/equal the threshold) */
	BOOL DiscardInputOnOverflow;			/**< Set to TRUE (normally), if TRUE HOST must used ReadOffset above to determin read pointer */
	U8 DMAOverflowFlag;				/**< The cache copy of TP dma overflow flag. (copy back into TP on Associate or cache on disassociate) */
	void *FiltersFlushArray_p;			/**< A pointer to the FilterFlush Structure Array (holding any active FilterFlush jobs) */

	stptiSupport_DMAMemoryStructure_t CachedMemoryStructure;  /**< A structure holding the range of cached memory */
	stptiSupport_MessageQueue_t *SignallingMessageQ;          /**< A MessageQ to hold all the handles of buffers signalled (NULL if no signal object associated) */

} stptiHAL_Buffer_t;

/* Exported Function Prototypes -------------------------------------------- */
extern const stptiHAL_BufferAPI_t stptiHAL_BufferAPI;

void stptiHAL_BufferTPEntryInitialise(volatile stptiTP_DMAInfo_t * DMAInfo_p);
ST_ErrorCode_t stptiHAL_BufferWindbackQWrite(FullHandle_t BufferHandle);
ST_ErrorCode_t stptiHAL_BufferState(FullHandle_t BufferHandle, U32 * BytesInBuffer_p, U32 * BufferUnitCount_p,
				    U32 * FreeSpace_p, U32 * UnitsInBuffer_p, U32 * NonUnitBytesInBuffer_p,
				    BOOL * OverflowedFlag_p);

#define stptiHAL_GetObjectBuffer_p(ObjectHandle)   ((stptiHAL_Buffer_t *)stptiOBJMAN_HandleToObjectPointer(ObjectHandle,OBJECT_TYPE_BUFFER))

/* Buffer(overwrite mode) is in overflow when overflow_overwrite flag is set AND reset flag is *not* set (which is done by host
when it moves the ReadOffset pointer) */
#define BufferInOverflowOverwrite(x) (x ^ (DMA_INFO_MARK_OVERFLOWED_OVERWRITE & ~DMA_INFO_MARK_RESET_OVERFLOW))

#endif /* _PTI_BUFFER_H_ */
