/************************************************************************
Copyright (C) 2013 STMicroelectronics. All Rights Reserved.

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
   @file   pti_buffer_lite.c
   @brief  Buffer Object Initialisation, Termination and Manipulation Functions.

   Dummy functions without TP functionality.

 */

#if 0
#define STPTI_PRINT
#endif

/* Includes ---------------------------------------------------------------- */

/* ANSI C includes */
#include "linuxcommon.h"

/* Includes from API level */
#include "../pti_debug.h"

/* Includes from the HAL / ObjMan level */
//#include "pti_vdevice_lite.h"
#include "pti_buffer_lite.h"

/* MACROS ------------------------------------------------------------------ */
/* Private Constants ------------------------------------------------------- */
/* Private Variables ------------------------------------------------------- */
/* Private Function Prototypes --------------------------------------------- */
ST_ErrorCode_t stptiHAL_BufferAllocatorLite(FullHandle_t BufferHandle, void *params_p);
ST_ErrorCode_t stptiHAL_BufferAssociatorLite(FullHandle_t BufferHandle, FullHandle_t AssocObjectHandle);
ST_ErrorCode_t stptiHAL_BufferDisassociatorLite(FullHandle_t BufferHandle, FullHandle_t AssocObjectHandle);
ST_ErrorCode_t stptiHAL_BufferDeallocatorLite(FullHandle_t BufferHandle);
ST_ErrorCode_t stptiHAL_BufferSetThresholdLite(FullHandle_t BufferHandle, U32 UpperThreshold);
ST_ErrorCode_t stptiHAL_BufferFlushLite(FullHandle_t BufferHandle);
ST_ErrorCode_t stptiHAL_BufferFiltersFlushLite(FullHandle_t BufferHandle, FullHandle_t *FilterHandles,
					       int NumberOfHandles);
ST_ErrorCode_t stptiHAL_BufferGetWriteOffsetLite(FullHandle_t BufferHandle, U32 *WriteOffset_p);

ST_ErrorCode_t stptiHAL_BufferReadLite(FullHandle_t BufferHandle, stptiHAL_BufferReadQuantisationRule_t ReadAsUnits,
				       U32 *ReadOffset_p, U32 LeadingBytesToDiscard, void *DestBuffer1_p,
				       U32 DestSize1, void *DestBuffer2_p, U32 DestSize2, void *MetaData_p,
				       U32 MetaDataSize, ST_ErrorCode_t(*CopyFunction) (void **, const void *, size_t),
				       U32 *BytesCopied);
ST_ErrorCode_t stptiHAL_BufferSetReadOffsetLite(FullHandle_t BufferHandle, U32 ReadOffset);

ST_ErrorCode_t stptiHAL_BufferStatusLite(FullHandle_t BufferHandle, U32 *BufferSize_p, U32 *BytesInBuffer_p,
					 U32 *BufferUnitCount_p, U32 *FreeSpace_p, U32 *UnitsInBuffer_p,
					 U32 *NonUnitBytesInBuffer_p, BOOL *OverflowedFlag_p);
ST_ErrorCode_t stptiHAL_BufferTypeLite(FullHandle_t BufferHandle, stptiHAL_SlotMode_t *InheritedSlotType);
ST_ErrorCode_t stptiHAL_BufferSetOverflowControlLite(FullHandle_t BufferHandle, BOOL DiscardInputOnOverflow);

/* Public Constants -------------------------------------------------------- */

/* Export the API */
const stptiHAL_BufferAPI_t stptiHAL_BufferAPI_Lite = {
	{
		/* Allocator                  Associator                     Disassociator                     Deallocator */
		stptiHAL_BufferAllocatorLite, stptiHAL_BufferAssociatorLite, stptiHAL_BufferDisassociatorLite, stptiHAL_BufferDeallocatorLite,
		NULL, NULL
	},
	stptiHAL_BufferSetThresholdLite,
	stptiHAL_BufferFlushLite,
	stptiHAL_BufferFiltersFlushLite,
	stptiHAL_BufferGetWriteOffsetLite,
	stptiHAL_BufferReadLite,
	stptiHAL_BufferSetReadOffsetLite,
	stptiHAL_BufferStatusLite,
	stptiHAL_BufferTypeLite,
	stptiHAL_BufferSetOverflowControlLite,
};

/* Object Management Functions ------------------------------------------------------------------ */

/**
   @brief  Allocate a Buffer

   Dummy function without TP functionality.

   @param  BufferHandle         A handle for the buffer
   @param  params_p             Parameters for the buffer

   @return                      A standard st error type...
                                - ST_ERROR_BAD_PARAMETER
                                - ST_ERROR_NO_MEMORY
                                - ST_NO_ERROR
 */
ST_ErrorCode_t stptiHAL_BufferAllocatorLite(FullHandle_t BufferHandle, void *params_p)
{
	return (ST_NO_ERROR);
}

/**
   @brief  Associate a Buffer to a Slot or Signal

   This function allows the user to associate slots and signals.  When associating a slot, if this
   is the first slot associated to this buffer it allocates TP DMA resources.

   @param  BufferHandle         A handle for the buffer
   @param  AssocObjectHandle    A handle for the object being associated (a slot or signal)

   @return                      A standard st error type...
                                - ST_ERROR_BAD_PARAMETER
                                - STPTI_ERROR_INVALID_SLOT_TYPE  (all assoc'd slots must be the same type)
                                - ST_ERROR_NO_MEMORY
                                - ST_NO_ERROR
 */
ST_ErrorCode_t stptiHAL_BufferAssociatorLite(FullHandle_t BufferHandle, FullHandle_t AssocObjectHandle)
{
	return (ST_NO_ERROR);
}

/**
   @brief  Disassociate a Buffer from a Slot or Signal.

   This function allows the user to disassociate slots and signals.  When disassociating a slot, if
   there are no other slots left then the TP's resources are cleared for this buffer (but it can still
   be read using the buffer read functions).

   @param  BufferHandle         A handle for the buffer
   @param  AssocObjectHandle    A handle for the object being disassociated (a slot or signal)

   @return                      A standard st error type...
                                - ST_NO_ERROR
 */
ST_ErrorCode_t stptiHAL_BufferDisassociatorLite(FullHandle_t BufferHandle, FullHandle_t AssocObjectHandle)
{
	return (ST_NO_ERROR);
}

/**
   @brief  Deallocate a Buffer.

   Dummy function with no TP functionality.

   @param  BufferHandle         A handle for the buffer

   @return                      A standard st error type...
                                - ST_NO_ERROR
 */
ST_ErrorCode_t stptiHAL_BufferDeallocatorLite(FullHandle_t BufferHandle)
{
	return (ST_NO_ERROR);
}

/* Object HAL functions ------------------------------------------------------------------------- */

/**
   @brief  Set the threshold for a buffer.

   Dummy function without TP functionality.

   @param  BufferHandle         The Handle of the Buffer
   @param  UpperThreshold       The threshold at which signalling will occur (if above or equal to)

   @return                      A standard st error type...
                                - ST_NO_ERROR
 */
ST_ErrorCode_t stptiHAL_BufferSetThresholdLite(FullHandle_t BufferHandle, U32 UpperThreshold)
{
	return (ST_ERROR_FEATURE_NOT_SUPPORTED);
}

/**
   @brief  Flush the buffer (move read pointer to write pointer)

   Dummy function without TP functionality.

   @param  BufferHandle         The Handle of the Buffer

   @return                      A standard st error type...
                                - ST_NO_ERROR
 */
ST_ErrorCode_t stptiHAL_BufferFlushLite(FullHandle_t BufferHandle)
{
	return (ST_NO_ERROR);
}

/**
   @brief  Remove filter matches from the buffer

   Dummy function without TP functionality.

   @param  BufferHandle         The Handle of the Buffer
   @param  FilterHandles        A pointer to an array of Filter Handles to "flush"
   @param  NumberOfHandles      The number of handle in the above array

   @return                      A standard st error type...
                                - ST_NO_ERROR
 */
ST_ErrorCode_t stptiHAL_BufferFiltersFlushLite(FullHandle_t BufferHandle, FullHandle_t *FilterHandles,
					       int NumberOfHandles)
{
	return (ST_NO_ERROR);
}

/**
   @brief  Return the write offset

   Dummy function without TP functionality.

   @param  BufferHandle         The Handle of the Buffer

   @return                      A standard st error type...
                                - ST_NO_ERROR
 */
ST_ErrorCode_t stptiHAL_BufferGetWriteOffsetLite(FullHandle_t BufferHandle, U32 *WriteOffset_p)
{
	*WriteOffset_p = 0;

	return ST_NO_ERROR;
}

/**
   @brief  Read data from a buffer and place into specified memory regions.

   Dummy function without TP functionality.

   @param  BufferHandle             The Handle of the Buffer
   @param  ReadAsUnits              Set to TRUE if to read data a quantisation units
   @param  ReadOffset_p             A pointer to a U32 holding the ReadOffset to start reading from.
                                    The ReadOffset should be stptiHAL_CURRENT_READ_OFFSET to start
                                    reading from the current read pointer.  Updated with the revised
                                    ReadOffset after the read.
   @param  LeadingBytesToDiscard    Offset into unit to start return data from
   @param  DestBuffer1              The start address of where to copy data to (first region).
   @param  DestSize1                The size of first region.
   @param  DestBuffer2              The start address of where to copy data to (second region) used
                                    after the first region is full.  Can be NULL.
   @param  DestSize2                The size of second region.
   @param  MetaData_p               A pointer where to put metadata (the form of which is dependent on
                                    the slot associated).
   @param  MetaDataSize             Size (in bytes) of the Metadata region.
   @param  CopyFunction             The function to use for copying.
   @param  BytesCopied              (return) the number of bytes read from the buffer.

   @return                          A standard st error type...
                                    - STPTI_ERROR_BUFFER_HAS_NO_METADATA            (one or more slots outputing data without the necessary metadata)
                                    - STPTI_ERROR_CORRUPT_DATA_IN_BUFFER            (driver fault - metadata/section/pes alignment issue)
                                    - ST_ERROR_BAD_PARAMETER                        (Metadata buffer NULL or too small)
                                    - ST_NO_ERROR
 */
ST_ErrorCode_t stptiHAL_BufferReadLite(FullHandle_t BufferHandle, stptiHAL_BufferReadQuantisationRule_t ReadAsUnits,
				       U32 *ReadOffset_p, U32 LeadingBytesToDiscard, void *DestBuffer1_p,
				       U32 DestSize1, void *DestBuffer2_p, U32 DestSize2, void *MetaData_p,
				       U32 MetaDataSize, ST_ErrorCode_t(*CopyFunction) (void **, const void *, size_t),
				       U32 *BytesCopied)
{
	*BytesCopied = 0;

	return (ST_NO_ERROR);
}

/**
   @brief  Sets the read offset.

   Dummy function without TP functionality.

   @param  BufferHandle         The Handle of the Buffer
   @param  ReadOffset           Read Offset to set

   @return                      A standard st error type...
                                - ST_ERROR_BAD_PARAMETER  (invalid buffer handle, read pointer outside of buffer)
                                - ST_NO_ERROR
 */
ST_ErrorCode_t stptiHAL_BufferSetReadOffsetLite(FullHandle_t BufferHandle, U32 ReadOffset)
{
	return (ST_NO_ERROR);
}

/**
   @brief  Return the Filled status of the buffer.

   Dummy function without TP functionality.

   @param  BufferHandle      A Handle to the buffer.

   @param  BufferSize_p      Size of the Buffer in bytes.

   @param  BytesInBuffer_p   Bytes in the buffer that can be read (only complete units of information).

   @param  BufferUnitCount_p Number of units of information received in the buffer.

   @param  FreeSpace_p       Amount of free space in the buffer (takes into account incomplete units).

   @param  UnitsInBuffer_p   Number of units of information in the buffer.

   @param  NonUnitBytesInBuffer_p Number of additional bytes of data not contained in typicial units.

   @return                   A standard st error type...
                             - ST_NO_ERROR if no errors
 */
ST_ErrorCode_t stptiHAL_BufferStatusLite(FullHandle_t BufferHandle, U32 *BufferSize_p,
					 U32 *BytesInBuffer_p, U32 *BufferUnitCount_p,
					 U32 *FreeSpace_p, U32 *UnitsInBuffer_p,
					 U32 *NonUnitBytesInBuffer_p, BOOL *OverflowedFlag_p)
{
	if (BufferSize_p)
		*BufferSize_p = 0;

	if (BytesInBuffer_p)
		*BytesInBuffer_p = 0;

	if (BufferUnitCount_p)
		*BufferUnitCount_p = 0;

	if (FreeSpace_p)
		*FreeSpace_p = 0;

	if (UnitsInBuffer_p)
		*UnitsInBuffer_p = 0;

	if (NonUnitBytesInBuffer_p)
		*NonUnitBytesInBuffer_p = 0;

	if (OverflowedFlag_p != NULL)
		*OverflowedFlag_p = 0;

	return ST_NO_ERROR;
}

/**
   @brief  Return the Buffer type.

   Dummy function without TP functionality.

   @param  BufferHandle         The Handle of the Buffer
   @param  InheritedSlotType    The buffers type.

   @return                      A standard st error type...
                                - ST_NO_ERROR
 */
ST_ErrorCode_t stptiHAL_BufferTypeLite(FullHandle_t BufferHandle, stptiHAL_SlotMode_t *InheritedSlotType)
{
	*InheritedSlotType = stptiHAL_SLOT_TYPE_NULL;
	return (ST_NO_ERROR);
}

/**
   @brief  Set Overflow Control on and off

   This function control whether data is discarded when the buffer is full or whether unread data
   is overwritten.

   @param  BufferHandle            The Handle of the Buffer
   @param  DiscardInputOnOverflow  TRUE if we should discard the incoming data upon overflow (the default)

   @return                         A standard st error type...
                                   - ST_NO_ERROR
 */
ST_ErrorCode_t stptiHAL_BufferSetOverflowControlLite(FullHandle_t BufferHandle, BOOL DiscardInputOnOverflow)
{
	return (ST_NO_ERROR);
}
