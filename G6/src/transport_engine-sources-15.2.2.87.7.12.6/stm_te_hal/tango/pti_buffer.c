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
   @file   pti_buffer.c
   @brief  Buffer Object Initialisation, Termination and Manipulation Functions.
  
   This file implements the functions for creating, destroying and accessing PTI buffers.
   
   Buffers are regions of memory for data collection.  They collect data from slots.  They are 
   circular and when the read and write pointers are equal that means the buffer is empty.
   
   A buffer inherits a type from the associated slot.  You cannot associate different types of slot
   to the same buffer.
   
   Information is put into the buffer in units.  The size of a unit is dependent on the slot type
   of the slot associated, and possibly the data itself.  For example, for a buffer associated to
   a RAW slot, the size of a unit is a transport packet.  For a buffer associated to a SECTION
   slot, the size of a unit is a section (variable size).  For a buffer associated to a PES slot,
   the size of a unit is a PES unit (variable size).
   
   If a slot has metadata turned off then "units" makes no sense and the data has to be read using
   a BufferRead for N bytes.  Metadata provides the information to understand the size of a "unit"
   of information.
   
   Buffers can have signals associated.  Signals happen when a byte threshold (of "fullness") is
   crossed.  Once over the threshold, it will not signal again until you empty it below the 
   threshold and new data is put in to take it over the threshold again.
   
   For buffer collecting units of information, the threshold is set to 1.  You'll get a signal for
   every unit put into the buffer.
   
   Once of the key complications with buffer handling is the cache.  You have to make sure your
   buffer is cache aligned, so that when the host invalidations/flushes the cache you don't 
   affect neighbouring variables.  This means you end up allocating more memory than is needed
   to allow room for alignment.
   
 */

#if 0
#define STPTI_PRINT
#endif

/* Includes ---------------------------------------------------------------- */

/* ANSI C includes */
#include "linuxcommon.h"

/* Includes from API level */
#include "../pti_debug.h"
#include "../pti_hal_api.h"

/* Includes from the HAL / ObjMan level */
#include "pti_vdevice.h"
#include "pti_slot.h"
#include "pti_buffer.h"
#include "pti_filter.h"
#include "pti_signal.h"

/* MACROS ------------------------------------------------------------------ */
/* Private Constants ------------------------------------------------------- */
/* Private Variables ------------------------------------------------------- */
/* Private Function Prototypes --------------------------------------------- */
ST_ErrorCode_t stptiHAL_BufferAllocator(FullHandle_t BufferHandle, void *params_p);
ST_ErrorCode_t stptiHAL_BufferAssociator(FullHandle_t BufferHandle, FullHandle_t AssocObjectHandle);
ST_ErrorCode_t stptiHAL_BufferDisassociator(FullHandle_t BufferHandle, FullHandle_t AssocObjectHandle);
ST_ErrorCode_t stptiHAL_BufferDeallocator(FullHandle_t BufferHandle);
ST_ErrorCode_t stptiHAL_BufferSerialiser(FullHandle_t BufferHandle, int mode, void *Region_p, size_t RegionSize,
					 size_t * SerialisedObjectSize_p);
ST_ErrorCode_t stptiHAL_BufferDeserialiser(FullHandle_t BufferHandle, void *params_p, int mode);

ST_ErrorCode_t stptiHAL_BufferSetThreshold(FullHandle_t BufferHandle, U32 UpperThreshold);
ST_ErrorCode_t stptiHAL_BufferFlush(FullHandle_t BufferHandle);
ST_ErrorCode_t stptiHAL_BufferFiltersFlush(FullHandle_t BufferHandle, FullHandle_t * FilterHandles,
					   int NumberOfHandles);
ST_ErrorCode_t stptiHAL_BufferGetWriteOffset(FullHandle_t BufferHandle, U32 * WriteOffset_p);
ST_ErrorCode_t stptiHAL_BufferRead(FullHandle_t BufferHandle, stptiHAL_BufferReadQuantisationRule_t ReadAsUnits,
				   U32 * ReadOffset_p, U32 LeadingBytesToDiscard, void *DestBuffer1_p, U32 DestSize1,
				   void *DestBuffer2_p, U32 DestSize2, void *MetaData_p, U32 MetaDataSize,
				   ST_ErrorCode_t(*CopyFunction) (void **, const void *, size_t), U32 * BytesCopied);
ST_ErrorCode_t stptiHAL_BufferSetReadOffset(FullHandle_t BufferHandle, U32 ReadOffset);
ST_ErrorCode_t stptiHAL_BufferStatus(FullHandle_t BufferHandle, U32 * BufferSize_p, U32 * BytesInBuffer_p,
				     U32 * BufferUnitCount_p, U32 * FreeSpace_p, U32 * UnitsInBuffer_p,
				     U32 * NonUnitBytesInBuffer_p, BOOL * OverflowedFlag_p);
ST_ErrorCode_t stptiHAL_BufferType(FullHandle_t BufferHandle, stptiHAL_SlotMode_t * InheritedSlotType);
ST_ErrorCode_t stptiHAL_BufferSetOverflowControl(FullHandle_t BufferHandle, BOOL DiscardInputOnOverflow);

static void stptiHAL_BufferSetThresholdAndOverflow(stptiHAL_vDevice_t * vDevice_p, stptiHAL_Buffer_t * Buffer_p);
static ST_ErrorCode_t stptiHAL_BufferDMAConfigure(stptiHAL_vDevice_t * vDevice_p, stptiHAL_Buffer_t * Buffer_p);
static ST_ErrorCode_t stptiHAL_BufferTPSync(FullHandle_t BufferHandle);
static void SignalBufferIfNecessary(stptiHAL_Buffer_t * Buffer_p, stptiHAL_vDevice_t * vDevice_p);
static ST_ErrorCode_t ReturnSectionZone(U32 DestSize, void *MetaData_p, U32 MetaDataSize,
					stptiHAL_vDevice_t * vDevice_p, stptiHAL_Buffer_t * Buffer_p, U32 BytesInBuffer,
					U32 * ReadOffset_p, U32 * BytesToRead_p, U32 * BytesToSkipAtEnd_p);
static ST_ErrorCode_t ReturnPartialPESZone(U32 DestSize, void *MetaData_p, U32 MetaDataSize,
					   stptiHAL_Buffer_t * Buffer_p, U32 BytesInBuffer, U32 * ReadOffset_p,
					   U32 * BytesToRead_p, U32 * BytesToSkipAtEnd_p);
static ST_ErrorCode_t ReturnPESZone(U32 DestSize, stptiHAL_Buffer_t * Buffer_p, U32 BytesInBuffer, U32 * ReadOffset_p,
				    U32 * BytesToRead_p, U32 * BytesToSkipAtEnd_p);
static ST_ErrorCode_t ReturnFixedSizedZone(U32 DestSize, U32 PacketSize, stptiHAL_Buffer_t * Buffer_p,
					   U32 BytesInBuffer, U32 * ReadOffset_p, U32 * BytesToRead_p,
					   U32 * BytesToSkipAtEnd_p);
static ST_ErrorCode_t ReturnRAWZone(U32 DestSize, stptiHAL_vDevice_t * vDevice_p, stptiHAL_Buffer_t * Buffer_p,
				    U32 BytesInBuffer, U32 * ReadOffset_p, U32 * BytesToRead_p,
				    U32 * BytesToSkipAtEnd_p);
static ST_ErrorCode_t ReturnIndexZone(U32 DestSize, void *MetaData_p, U32 MetaDataSize, stptiHAL_vDevice_t * vDevice_p,
				      stptiHAL_Buffer_t * Buffer_p, U32 BytesInBuffer, U32 * ReadOffset_p,
				      U32 * BytesToRead_p, U32 * BytesToSkipAtEnd_p);
static ST_ErrorCode_t ReturnUnQuantisedZone(U32 DestSize, stptiHAL_Buffer_t * Buffer_p, U32 BytesInBuffer,
					    U32 * ReadOffset_p, U32 * BytesToRead_p, U32 * BytesToSkipAtEnd_p);
static ST_ErrorCode_t CopyFromCircularBuffer(stptiHAL_Buffer_t * Buffer_p, U32 * ReadOffset, U8 * DestBuffer,
					     U32 DestBytesToCopy, ST_ErrorCode_t(*CopyFunction) (void **, const void *,
												 size_t));
static void stptiSupport_InvalidateRegionCircular(stptiHAL_Buffer_t * Buffer_p, unsigned int Offset, unsigned int Size);
static ST_ErrorCode_t stptiHAL_IndexMemcpyTransfer(void **Dest_p, const void *Src_p, size_t CopySize);
static ST_ErrorCode_t ReadIndexParcel(stptiHAL_Buffer_t * Buffer_p, U32 * BytesLeftInBuffer, U32 * ParcelReadOffset_p,
				      stptiTP_IndexerParcelID_t * ParcelID_p, U32 * ParcelPayloadStartOffset_p,
				      U32 * ParcelPayloadSize_p);

/* Public Constants -------------------------------------------------------- */

/* Export the API */
const stptiHAL_BufferAPI_t stptiHAL_BufferAPI = {
	{			/* Allocator              Associator                 Disassociator                 Deallocator */
	 stptiHAL_BufferAllocator, stptiHAL_BufferAssociator, stptiHAL_BufferDisassociator, stptiHAL_BufferDeallocator,
	 stptiHAL_BufferSerialiser, stptiHAL_BufferDeserialiser},
	stptiHAL_BufferSetThreshold,
	stptiHAL_BufferFlush,
	stptiHAL_BufferFiltersFlush,
	stptiHAL_BufferGetWriteOffset,
	stptiHAL_BufferRead,
	stptiHAL_BufferSetReadOffset,
	stptiHAL_BufferStatus,
	stptiHAL_BufferType,
	stptiHAL_BufferSetOverflowControl,
};

/* Object Management Functions ------------------------------------------------------------------ */

/**
   @brief  Allocate a Buffer
   
   This function allocates memory for a buffer, but does not allocate resources from the TP until
   the buffer is associated.
   
   @param  BufferHandle         A handle for the buffer
   @param  params_p             Parameters for the buffer
   
   @return                      A standard st error type...     
                                - ST_ERROR_BAD_PARAMETER
                                - ST_ERROR_NO_MEMORY
                                - ST_NO_ERROR
 */
ST_ErrorCode_t stptiHAL_BufferAllocator(FullHandle_t BufferHandle, void *params_p)
{
	/* Already write locked */
	stptiHAL_pDevice_t *pDevice_p = stptiHAL_GetObjectpDevice_p(BufferHandle);
	stptiHAL_Buffer_t *Buffer_p = stptiHAL_GetObjectBuffer_p(BufferHandle);
	stptiHAL_BufferConfigParams_t *Parameters_p = (stptiHAL_BufferConfigParams_t *) params_p;

	ST_ErrorCode_t Error = ST_NO_ERROR;
	void *AllocatedMemory_p = NULL;

	BOOL MappedBuffer = FALSE;

	if (Parameters_p->ManuallyAllocatedBuffer) {
		Error = ST_ERROR_BAD_PARAMETER;
		STPTI_PRINTF_ERROR("Manually-allocated buffers not supported.");
	} else {
		Buffer_p->CachedMemoryStructure.Dev = &pDevice_p->pdev->dev;
		AllocatedMemory_p = stptiSupport_MemoryAllocateForDMA(Parameters_p->BufferSize,
								      STPTI_SUPPORT_DCACHE_LINE_SIZE,
								      &Buffer_p->CachedMemoryStructure,
								      stptiSupport_ZONE_LARGE_SECURE);

		if (NULL == AllocatedMemory_p) {
			Error = ST_ERROR_NO_MEMORY;
		} else {
			Parameters_p->BufferStart_p = AllocatedMemory_p;
		}
	}

	if (ST_NO_ERROR == Error) {
		/* Allocate a FiltersFlushArray in case this buffer is used for collecting sections */
		Error = stptiHAL_FiltersFlushArrayAllocate(BufferHandle, &(Buffer_p->FiltersFlushArray_p));
	}

	if (ST_NO_ERROR != Error) {
		/* An Error, so lets free any memory we might have allocated */
		if (!Parameters_p->ManuallyAllocatedBuffer && AllocatedMemory_p != NULL) {
			stptiSupport_MemoryDeallocateForDMA(&Buffer_p->CachedMemoryStructure);
		}
	} else {
		Buffer_p->ManuallyAllocatedBuffer = Parameters_p->ManuallyAllocatedBuffer;
		Buffer_p->DriverHasMappedBuffer = MappedBuffer;
		Buffer_p->Buffer_p = (U8 *) AllocatedMemory_p;
		Buffer_p->PhysicalBuffer_p =
		    (U32) stptiSupport_VirtToPhys(&Buffer_p->CachedMemoryStructure, AllocatedMemory_p);
		Buffer_p->BufferSize = Parameters_p->BufferSize;
		Buffer_p->ReadOffset = 0;
		Buffer_p->QWriteOffset = 0;
		Buffer_p->BufferUnitCount = 0;
		Buffer_p->BufferIndex = -1;	/* Marked at not TP mapped */
		Buffer_p->IndexerCount = -1;
		Buffer_p->SlotCount = -1;
		Buffer_p->AssociatedSlotsWithMetaDataSuppressed = 0;
		Buffer_p->SignallingMessageQ = NULL;
		Buffer_p->SignallingUpperThreshold = DMA_INFO_NO_SIGNALLING;
		Buffer_p->AssociatedSlotsMode = stptiHAL_SLOT_TYPE_NULL;
		Buffer_p->DiscardInputOnOverflow = TRUE;
		Buffer_p->DMAOverflowFlag = 0;

		/* If it is a manually allocated buffer it could be uncached */
		if (!Parameters_p->ManuallyAllocatedBuffer) {
			/* SAFETY AND PORTABILITY...
			   We invalidate here for architectures which don't have invalidate mechanism (such as 
			   st200).  In such cases the invalidate is replaced by purge which will flush dirty cache
			   lines (cache->mem), then invalidate.  This can work as an invalidate mechanism so long
			   as the CPU does not write to the memory to be invalidated (hence the cache is never 
			   dirty for this region and doesn't need flushing).   By invalidating/purging here we make
			   sure that the cache is invalidated BEFORE we care about the data being put into the 
			   buffer (i.e. by the PTI device itself). */

			stptiSupport_InvalidateRegion(&Buffer_p->CachedMemoryStructure, (void *)Buffer_p->Buffer_p,
						      Buffer_p->BufferSize);
		}

		stpti_printf("Allocated Buffer of size %d @ 0x%08x (0x%08x) %d",
			     Buffer_p->BufferSize, (unsigned)Buffer_p->Buffer_p, (unsigned)Buffer_p->PhysicalBuffer_p,
			     Buffer_p->ManuallyAllocatedBuffer);

	}

	return (Error);
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
ST_ErrorCode_t stptiHAL_BufferAssociator(FullHandle_t BufferHandle, FullHandle_t AssocObjectHandle)
{
	/* Already write locked */
	stptiHAL_Buffer_t *Buffer_p = stptiHAL_GetObjectBuffer_p(BufferHandle);
	stptiHAL_vDevice_t *vDevice_p = stptiHAL_GetObjectvDevice_p(BufferHandle);

	ST_ErrorCode_t Error = ST_NO_ERROR;

	switch (stptiOBJMAN_GetObjectType(AssocObjectHandle)) {
	case OBJECT_TYPE_SLOT:
		{
			stptiHAL_Slot_t *Slot_p = stptiHAL_GetObjectSlot_p(AssocObjectHandle);

			/* See if any slots or indexers are already associated */
			if (Buffer_p->SlotCount <= 0 && Buffer_p->IndexerCount <= 0) {
				/* nothing associated, so we know that there is no DMA allocated */

				/* See if the buffer is empty */
				if (Buffer_p->ReadOffset == Buffer_p->QWriteOffset) {
					/* It is empty so set the Counts to indicate that the buffer has no "type"
					   associated with it. */
					Buffer_p->SlotCount = -1;
					Buffer_p->IndexerCount = -1;
				}
			}

			/* Check to see that there are no indexers associated */
			if (Buffer_p->IndexerCount != -1) {
				Error = STPTI_ERROR_OBJECT_ALREADY_ASSOCIATED;
			} else {
				/* Setup Buffer_p->AssociatedSlotsMode */
				if (Buffer_p->SlotCount == -1) {
					Buffer_p->SlotCount = 0;
					Buffer_p->AssociatedSlotsMode = Slot_p->SlotMode;
				} else {
					if (Buffer_p->AssociatedSlotsMode != Slot_p->SlotMode) {
						Error = STPTI_ERROR_INVALID_SLOT_TYPE;
					}
				}
			}

			if (ST_NO_ERROR == Error) {
				if (Slot_p->SuppressMetaData) {
					Buffer_p->AssociatedSlotsWithMetaDataSuppressed++;
				}

				/* We need to determine if this buffer has been TP mapped yet or not */
				if (Buffer_p->BufferIndex < 0) {
					Error = stptiHAL_BufferDMAConfigure(vDevice_p, Buffer_p);
				}

				if (ST_NO_ERROR == Error) {
					stpti_printf("Associated Buffer [%08x] @%p", BufferHandle.word,
						     Buffer_p->Buffer_p);
					Buffer_p->SlotCount++;
				}
			}
		}
		break;

	case OBJECT_TYPE_INDEX:
		{
			/* See if any slots or indexers are already associated */
			if (Buffer_p->SlotCount <= 0 && Buffer_p->IndexerCount <= 0) {
				/* nothing associated, so we know that there is no DMA allocated */

				/* See if the buffer is empty */
				if (Buffer_p->ReadOffset == Buffer_p->QWriteOffset) {
					/* It is empty so set the Counts to indicate that the buffer has no "type"
					   associated with it. */
					Buffer_p->SlotCount = -1;
					Buffer_p->IndexerCount = -1;
				}
			}

			if (Buffer_p->SlotCount != -1) {
				Error = STPTI_ERROR_OBJECT_ALREADY_ASSOCIATED;
			}

			if (ST_NO_ERROR == Error) {
				if (Buffer_p->IndexerCount == -1) {
					/* first indexer to be associated to this buffer (IndexerCount will be incremented below) */
					Buffer_p->IndexerCount = 0;
				}

				/* We need to determine if this buffer has been TP mapped yet or not */
				if (Buffer_p->BufferIndex < 0) {
					Error = stptiHAL_BufferDMAConfigure(vDevice_p, Buffer_p);
				}

				if (ST_NO_ERROR == Error) {
					stpti_printf("Associated Buffer [%08x] @%p", BufferHandle.word,
						     Buffer_p->Buffer_p);
					Buffer_p->IndexerCount++;
				}
			}
		}
		break;

	case OBJECT_TYPE_SIGNAL:
		{
			stptiHAL_Signal_t *Signal_p = stptiHAL_GetObjectSignal_p(AssocObjectHandle);

			if (Buffer_p->SignallingMessageQ != NULL) {
				Error = STPTI_ERROR_ONLY_ONE_SIGNAL_PER_BUFFER;
			} else {
				Buffer_p->SignallingMessageQ = Signal_p->SignallingMessageQ;
				stptiHAL_BufferSetThresholdAndOverflow(vDevice_p, Buffer_p);
			}
		}
		break;

	default:
		Error = ST_ERROR_BAD_PARAMETER;
		break;
	}

	return (Error);
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
ST_ErrorCode_t stptiHAL_BufferDisassociator(FullHandle_t BufferHandle, FullHandle_t AssocObjectHandle)
{
	/* Already write locked */
	stptiHAL_Buffer_t *Buffer_p = stptiHAL_GetObjectBuffer_p(BufferHandle);
	stptiHAL_vDevice_t *vDevice_p = stptiHAL_GetObjectvDevice_p(BufferHandle);

	ST_ErrorCode_t Error = ST_NO_ERROR;

	switch (stptiOBJMAN_GetObjectType(AssocObjectHandle)) {
	case OBJECT_TYPE_SLOT:
		/* Slot disassociation fn will have been called first */
		{
			volatile stptiTP_DMAInfo_t *DMA_p = &vDevice_p->TP_Interface.DMAInfo_p[Buffer_p->BufferIndex];
			stptiHAL_Slot_t *Slot_p = stptiHAL_GetObjectSlot_p(AssocObjectHandle);

			/* Make sure the buffer is not currently being output to by the TP */
			stptiHAL_BufferTPSync(BufferHandle);	/* Purposefully ignore error (disassociate shouldn't fail) */

			if (Slot_p->SuppressMetaData) {
				Buffer_p->AssociatedSlotsWithMetaDataSuppressed--;
			}

			Buffer_p->ReadOffset = stptiHAL_pDeviceXP70Read(&DMA_p->read_offset);
			Buffer_p->QWriteOffset = stptiHAL_pDeviceXP70Read(&DMA_p->qwrite_offset);
			Buffer_p->BufferUnitCount = stptiHAL_pDeviceXP70Read(&DMA_p->buffer_unit_count);
			Buffer_p->DMAOverflowFlag =
			    stptiHAL_pDeviceXP70Read(&vDevice_p->TP_Interface.
						     DMAOverflowFlags_p[Buffer_p->BufferIndex]);
			Buffer_p->SlotCount--;

			if (Buffer_p->SlotCount == 0) {
				stptiOBJMAN_RemoveFromList(vDevice_p->BufferHandles, Buffer_p->BufferIndex);

				/* Reset values in the TP's DMA entry - strictly unnecessary but helps debugging */
				stptiHAL_BufferTPEntryInitialise(DMA_p);

				/* reset TP overflow entry */
				stptiHAL_pDeviceXP70Write(&vDevice_p->TP_Interface.
							  DMAOverflowFlags_p[Buffer_p->BufferIndex], 0);
				Buffer_p->BufferIndex = -1;
			}
		}
		break;

	case OBJECT_TYPE_INDEX:
		{
			volatile stptiTP_DMAInfo_t *DMA_p = &vDevice_p->TP_Interface.DMAInfo_p[Buffer_p->BufferIndex];

			Buffer_p->ReadOffset = stptiHAL_pDeviceXP70Read(&DMA_p->read_offset);
			Buffer_p->QWriteOffset = stptiHAL_pDeviceXP70Read(&DMA_p->qwrite_offset);
			Buffer_p->BufferUnitCount = stptiHAL_pDeviceXP70Read(&DMA_p->buffer_unit_count);
			Buffer_p->IndexerCount--;

			if ((Buffer_p->IndexerCount == 0) && (Buffer_p->BufferIndex >= 0)) {
				stptiOBJMAN_RemoveFromList(vDevice_p->BufferHandles, Buffer_p->BufferIndex);

				/* Reset values in the TP's DMA entry - strictly unnecessary but helps debugging */
				stptiHAL_BufferTPEntryInitialise(DMA_p);

				/* reset TP overflow entry */
				stptiHAL_pDeviceXP70Write(&vDevice_p->TP_Interface.
							  DMAOverflowFlags_p[Buffer_p->BufferIndex], 0);
				Buffer_p->BufferIndex = -1;
			}
		}
		break;

	case OBJECT_TYPE_SIGNAL:
		{
			Buffer_p->SignallingMessageQ = NULL;
			stptiHAL_BufferSetThresholdAndOverflow(vDevice_p, Buffer_p);
		}
		break;

	default:
		/* Allow disassociation, even from invalid types, else we create a clean up problem */
		STPTI_PRINTF_ERROR("Buffer disassociating from invalid type %d.",
				   stptiOBJMAN_GetObjectType(AssocObjectHandle));
		break;
	}

	return (Error);
}

/**
   @brief  Deallocate a Buffer.
   
   This function frees memory for the buffer.
   
   @param  BufferHandle         A handle for the buffer
   
   @return                      A standard st error type...     
                                - ST_NO_ERROR
 */
ST_ErrorCode_t stptiHAL_BufferDeallocator(FullHandle_t BufferHandle)
{
	/* Already write locked */
	stptiHAL_Buffer_t *Buffer_p = stptiHAL_GetObjectBuffer_p(BufferHandle);

	ST_ErrorCode_t Error = ST_NO_ERROR;

	/* Deallocate the FiltersFlushArray */
	Error = stptiHAL_FiltersFlushArrayDeallocate(BufferHandle, Buffer_p->FiltersFlushArray_p);

	/* Deallocate Buffer (if it was created by this driver) */
	if (!Buffer_p->ManuallyAllocatedBuffer) {
		stptiSupport_MemoryDeallocateForDMA(&Buffer_p->CachedMemoryStructure);
	}

	return (Error);
}

/**
   @brief  Serialise a Buffer for storage.
   
   This function serialises a buffer for the purposes of storage.  Useful for passive powerdown,
   and object cloning features.
   
   Note currently only mode 1 is supported (cloning), passive powerdown features are not yet
   implemented.
   
   @param  BufferHandle            A handle for the buffer
   @param  mode                    Mode of serialisation (normally 0 for powerdown, 1 for cloning)
   @param  Region_p                pointer to the memory to put the serialised object.
   @param  RegionSize              size of the memory set aside for the serialised object.
   @param  SerialisedObjectSize_p  pointer to a size_t to return the size of the serialised object.
   
   @return                         A standard st error type...     
                                   - ST_NO_ERROR
                                   - ST_ERROR_FEATURE_NOT_SUPPORTED
                                   - ST_BAD_PARAMETER
 */
ST_ErrorCode_t stptiHAL_BufferSerialiser(FullHandle_t BufferHandle, int mode, void *Region_p, size_t RegionSize,
					 size_t * SerialisedObjectSize_p)
{
	/* Already write locked */
	ST_ErrorCode_t Error = ST_NO_ERROR;
	stptiHAL_Buffer_t *Buffer_p = stptiHAL_GetObjectBuffer_p(BufferHandle);
	U8 *p = Region_p;

	switch (mode) {
	case 1:		/* serialise object structure but do not include the buffer, and do not deallocate the memory region (useful for cloning) */
		{
			const unsigned int bufferp_size = sizeof(*Buffer_p) - sizeof(Object_t);
			unsigned int ffa_size = stptiHAL_FiltersFlushArraySize();

			if (RegionSize < (bufferp_size + ffa_size)) {
				Error = ST_ERROR_NO_MEMORY;
			} else {
				/* copy out Buffer object but exclude the object header (the object manager handles that) */
				memcpy(p, ((U8 *) Buffer_p) + sizeof(Object_t), bufferp_size);
				p += bufferp_size;
				memcpy(p, Buffer_p->FiltersFlushArray_p, ffa_size);
				p += ffa_size;
				Buffer_p->ManuallyAllocatedBuffer = TRUE;	/* Prevents deallocation of buffer */
			}
		}
		break;

	case 0:		/* full object including actual memory buffer (useful for powerdown) */
		STPTI_PRINTF_ERROR("Mode0 Buffer Serialisation not supported");
		Error = ST_ERROR_FEATURE_NOT_SUPPORTED;
		break;

	default:
		Error = ST_ERROR_BAD_PARAMETER;
		break;
	}

	*SerialisedObjectSize_p = p - (U8 *) Region_p;
	return (Error);
}

/**
   @brief  Deserialise a buffer from storage.
   
   This function deserialises a buffer, restoring its state.  Useful for passive powerdown,
   and object cloning features.

   Note currently only mode 1 is supported (cloning), passive powerdown features are not yet
   implemented.

   @param  BufferHandle            A handle for the buffer
   @param  Region_p                pointer to the memory to put the serialised object.
   @param  mode                    Mode of serialisation (normally 0 for powerdown, 1 for cloning)
   
   @return                         A standard st error type...     
                                   - ST_NO_ERROR
                                   - ST_ERROR_FEATURE_NOT_SUPPORTED
                                   - ST_BAD_PARAMETER
 */
ST_ErrorCode_t stptiHAL_BufferDeserialiser(FullHandle_t BufferHandle, void *Region_p, int mode)
{
	/* Already write locked */
	ST_ErrorCode_t Error = ST_NO_ERROR;
	stptiHAL_Buffer_t *Buffer_p = stptiHAL_GetObjectBuffer_p(BufferHandle);
	U8 *p = Region_p;

	switch (mode) {
	case 1:		/* serialise object structure but do not include the buffer, and do not deallocate the memory region */
		{
			const unsigned int bufferp_size = sizeof(*Buffer_p) - sizeof(Object_t);
			unsigned int ffa_size = stptiHAL_FiltersFlushArraySize();

			/* copy in Buffer object but exclude the object header (the object manager handles that) */
			memcpy(((U8 *) Buffer_p) + sizeof(Object_t), p, bufferp_size);
			p += bufferp_size;

			/* Buffer is assumed to be already allocated, and unmoved! */

			Error = stptiHAL_FiltersFlushArrayAllocate(BufferHandle, &(Buffer_p->FiltersFlushArray_p));
			memcpy(Buffer_p->FiltersFlushArray_p, p, ffa_size);
			p += ffa_size;

		}
		break;

	case 0:		/* full object including actual memory buffer */
		STPTI_PRINTF_ERROR("Mode0 Buffer Serialisation not supported");
		Error = ST_ERROR_FEATURE_NOT_SUPPORTED;
		break;

	default:
		Error = ST_ERROR_BAD_PARAMETER;
		break;
	}

	return (Error);
}

/* Object HAL functions ------------------------------------------------------------------------- */

/**
   @brief  Set the threshold for a buffer.
   
   This function set the signalling threshold for the buffer.  It does not enable signalling.  This
   is done by associating a signal.
   
   @param  BufferHandle         The Handle of the Buffer
   @param  UpperThreshold       The threshold at which signalling will occur (if above or equal to)
   
   @return                      A standard st error type...     
                                - ST_NO_ERROR
 */
ST_ErrorCode_t stptiHAL_BufferSetThreshold(FullHandle_t BufferHandle, U32 UpperThreshold)
{
	ST_ErrorCode_t Error = ST_NO_ERROR;
	stptiSupport_AccessLockState_t LocalLockState = stptiSupport_UNLOCKED;

	/* Limit the UpperTheshold to 0x7fffffff, because the upper bit means something else */
	if (UpperThreshold > DMA_INFO_NO_SIGNALLING) {
		UpperThreshold = DMA_INFO_NO_SIGNALLING;
	}

	stptiOBJMAN_WriteLock(BufferHandle, &LocalLockState);
	{
		stptiHAL_vDevice_t *vDevice_p = stptiHAL_GetObjectvDevice_p(BufferHandle);
		stptiHAL_Buffer_t *Buffer_p = stptiHAL_GetObjectBuffer_p(BufferHandle);
		Buffer_p->SignallingUpperThreshold = UpperThreshold;
		stptiHAL_BufferSetThresholdAndOverflow(vDevice_p, Buffer_p);
	}
	stptiOBJMAN_Unlock(BufferHandle, &LocalLockState);

	return (Error);
}

/**
   @brief  Flush the buffer (move read pointer to write pointer)
   
   This function effectively flushes the buffer.
   
   @param  BufferHandle         The Handle of the Buffer
   
   @return                      A standard st error type...     
                                - ST_NO_ERROR
 */
ST_ErrorCode_t stptiHAL_BufferFlush(FullHandle_t BufferHandle)
{
	ST_ErrorCode_t Error = ST_NO_ERROR;
	stptiSupport_AccessLockState_t LocalLockState = stptiSupport_UNLOCKED;

	stptiOBJMAN_WriteLock(BufferHandle, &LocalLockState);
	{
		stptiHAL_Buffer_t *Buffer_p = stptiHAL_GetObjectBuffer_p(BufferHandle);
		stptiHAL_vDevice_t *vDevice_p = stptiHAL_GetObjectvDevice_p(BufferHandle);

		if (Buffer_p == NULL || vDevice_p == NULL) {
			stptiOBJMAN_Unlock(BufferHandle, &LocalLockState);
			return ST_ERROR_INVALID_HANDLE;
		}

		Buffer_p->ReadOffset = 0;
		Buffer_p->QWriteOffset = 0;
		Buffer_p->BufferUnitCount = 0;
		Buffer_p->DMAOverflowFlag = 0;

		/* buffer is associated and hence has a TP DMA allocation */
		if (Buffer_p->BufferIndex >= 0) {
			volatile stptiTP_DMAInfo_t *DMA_p = &vDevice_p->TP_Interface.DMAInfo_p[Buffer_p->BufferIndex];

			FullHandle_t AssocSlotHandleList[16];
			int index;
			Object_t *AssociatedObject;

			stptiOBJMAN_FirstInList(&Buffer_p->ObjectHeader.AssociatedObjects, (void *)&AssociatedObject, &index);

			while (index >= 0) {
				int AssocSlots=0;

				while (index >= 0 && AssocSlots < sizeof(AssocSlotHandleList) / sizeof(FullHandle_t)) {
					if (AssociatedObject->Handle.member.ObjectType == OBJECT_TYPE_SLOT) {
						AssocSlotHandleList[AssocSlots] = stptiOBJMAN_ObjectPointerToHandle(AssociatedObject);
						AssocSlots++;
					}
					stptiOBJMAN_NextInList(&Buffer_p->ObjectHeader.AssociatedObjects, (void *)&AssociatedObject, &index);
				}

				/* This is done as one call, as there is a upto a packets delay added to make sure the slots are not in use */
				stptiHAL_SlotResetState(AssocSlotHandleList, AssocSlots, TRUE);

				/* Wait for the buffer, not to be in use */
				Error = stptiHAL_BufferTPSync(BufferHandle);

				if (ST_NO_ERROR == Error) {
					int i;
					/* Now safely do the buffer flush */
					stptiHAL_pDeviceXP70Write(&DMA_p->write_offset, Buffer_p->QWriteOffset);
					stptiHAL_pDeviceXP70Write(&DMA_p->qwrite_offset_pending, Buffer_p->QWriteOffset);
					stptiHAL_pDeviceXP70Write(&DMA_p->qwrite_offset, Buffer_p->QWriteOffset);
					stptiHAL_pDeviceXP70Write(&DMA_p->buffer_unit_count, Buffer_p->BufferUnitCount);
					stptiHAL_pDeviceXP70Write(&DMA_p->read_offset, Buffer_p->ReadOffset);

					/* clear TP overflow flag */
					stptiHAL_pDeviceXP70Write(&vDevice_p->TP_Interface.
								  DMAOverflowFlags_p[Buffer_p->BufferIndex],
								  Buffer_p->DMAOverflowFlag);

					/* (re)activate the slots */
					for (i = 0; i < AssocSlots; i++) {
						stptiHAL_SlotActivate(AssocSlotHandleList[i]);
					}
				}
			}
		}
		if (ST_NO_ERROR == Error) {
			/* Purge the filters flush array (removes any pending filter flush cases) */
			Error = stptiHAL_FiltersFlushArrayPurge(Buffer_p->FiltersFlushArray_p);
		}
	}
	stptiOBJMAN_Unlock(BufferHandle, &LocalLockState);

	return (Error);
}

/**
   @brief  Remove filter matches from the buffer
   
   This function effectively marks a filter as invisible for the buffer.  The section will still 
   be read from BufferRead but the filter won't be indicated as matching.
   
   @param  BufferHandle         The Handle of the Buffer
   @param  FilterHandles        A pointer to an array of Filter Handles to "flush"
   @param  NumberOfHandles      The number of handle in the above array
   
   @return                      A standard st error type...     
                                - ST_NO_ERROR
 */
ST_ErrorCode_t stptiHAL_BufferFiltersFlush(FullHandle_t BufferHandle, FullHandle_t * FilterHandles, int NumberOfHandles)
{
	ST_ErrorCode_t Error = ST_NO_ERROR;
	stptiSupport_AccessLockState_t LocalLockState = stptiSupport_UNLOCKED;

	stptiOBJMAN_WriteLock(BufferHandle, &LocalLockState);
	{
		stptiHAL_Buffer_t *Buffer_p = stptiHAL_GetObjectBuffer_p(BufferHandle);
		U32 DiscardRegionSize;

		Error = stptiHAL_BufferState(BufferHandle, &DiscardRegionSize, NULL, NULL, NULL, NULL, NULL);
		if (ST_NO_ERROR == Error) {
			Error =
			    stptiHAL_FiltersFlushArrayAddStructure(Buffer_p->FiltersFlushArray_p, DiscardRegionSize,
								   FilterHandles, NumberOfHandles);
		}
	}
	stptiOBJMAN_Unlock(BufferHandle, &LocalLockState);

	return (Error);
}

/**
   @brief  Return the write offset
   
   This function returns the (quantised) write offset.  This is often used for direct access (manual)
   Buffers (this is termed a Software CD FIFO Buffer).
  
   @param  BufferHandle         The Handle of the Buffer
   
   @return                      A standard st error type...     
                                - ST_NO_ERROR
 */
ST_ErrorCode_t stptiHAL_BufferGetWriteOffset(FullHandle_t BufferHandle, U32 * WriteOffset_p)
{
	ST_ErrorCode_t Error = ST_NO_ERROR;
	stptiSupport_AccessLockState_t LocalLockState = stptiSupport_UNLOCKED;

	stptiOBJMAN_ReadLock(BufferHandle, &LocalLockState);
	{
		stptiHAL_vDevice_t *vDevice_p = stptiHAL_GetObjectvDevice_p(BufferHandle);
		stptiHAL_Buffer_t *Buffer_p = stptiHAL_GetObjectBuffer_p(BufferHandle);

		if (Buffer_p->BufferIndex >= 0) {
			*WriteOffset_p =
			    stptiHAL_pDeviceXP70Read(&vDevice_p->TP_Interface.DMAInfo_p[Buffer_p->BufferIndex].
						     qwrite_offset);
		} else {
			*WriteOffset_p = Buffer_p->QWriteOffset;
		}
	}
	stptiOBJMAN_Unlock(BufferHandle, &LocalLockState);

	return (Error);
}

/**
   @brief  Read data from a buffer and place into specified memory regions.
   
   This function returns data from the buffer in upto two specified memory regions.  The second
   region is used when the first one is full.  It can be set to advance the read pointer.  MetaData
   is extracted from the buffer.
  
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
ST_ErrorCode_t stptiHAL_BufferRead(FullHandle_t BufferHandle, stptiHAL_BufferReadQuantisationRule_t ReadAsUnits,
				   U32 * ReadOffset_p, U32 LeadingBytesToDiscard, void *DestBuffer1_p, U32 DestSize1,
				   void *DestBuffer2_p, U32 DestSize2, void *MetaData_p, U32 MetaDataSize,
				   ST_ErrorCode_t(*CopyFunction) (void **, const void *, size_t), U32 * BytesCopied)
{
	ST_ErrorCode_t Error = ST_NO_ERROR;
	stptiSupport_AccessLockState_t LocalLockState = stptiSupport_UNLOCKED;
	U32 ReadOffset;
	U8 DMAOverflowFlag;

	/* We do this in a read lock because copying out data can take a while */
	stptiOBJMAN_ReadLock(BufferHandle, &LocalLockState);
	{
		stptiHAL_vDevice_t *vDevice_p = stptiHAL_GetObjectvDevice_p(BufferHandle);
		stptiHAL_Buffer_t *Buffer_p = stptiHAL_GetObjectBuffer_p(BufferHandle);

		U32 BytesInBuffer, BytesToRead = 0, BytesToSkipAtEnd = 0;

		if (Buffer_p == NULL || vDevice_p == NULL) {
			stptiOBJMAN_Unlock(BufferHandle, &LocalLockState);
			return ST_ERROR_INVALID_HANDLE;
		}

		if (Buffer_p->BufferIndex >= 0) {
			/* buffer is associated and hence has a TP DMA allocation */
			Buffer_p->QWriteOffset =
			    stptiHAL_pDeviceXP70Read(&vDevice_p->TP_Interface.DMAInfo_p[Buffer_p->BufferIndex].
						     qwrite_offset);
			Buffer_p->BufferUnitCount =
			    stptiHAL_pDeviceXP70Read(&vDevice_p->TP_Interface.DMAInfo_p[Buffer_p->BufferIndex].
						     buffer_unit_count);
		}

		if (Buffer_p->BufferIndex >= 0) {
			/* buffer is associated and hence has a TP DMA allocation */
			ReadOffset =
			    stptiHAL_pDeviceXP70Read(&vDevice_p->TP_Interface.DMAInfo_p[Buffer_p->BufferIndex].
						     read_offset);

			/* get the TP overflow status */
			DMAOverflowFlag =
			    stptiHAL_pDeviceXP70Read(&vDevice_p->TP_Interface.
						     DMAOverflowFlags_p[Buffer_p->BufferIndex]);
		} else {
			/* buffer is not associated (or readpointer is not valid in the case of !DiscardInputOnOverflow) so used cached pointers */
			ReadOffset = Buffer_p->ReadOffset;

			/* and use cached overflow flag */
			DMAOverflowFlag = Buffer_p->DMAOverflowFlag;
		}

		if (*ReadOffset_p != stptiHAL_CURRENT_READ_OFFSET) {
			if (*ReadOffset_p < Buffer_p->BufferSize) {
				ReadOffset = *ReadOffset_p;
			} else {
				Error = ST_ERROR_BAD_PARAMETER;
			}
		}

		if (!Buffer_p->DiscardInputOnOverflow && BufferInOverflowOverwrite(DMAOverflowFlag)) {
			/* host has not moved the read offset since overflowed */
			BytesInBuffer = Buffer_p->BufferSize;
		} else {
			/* Normal mode (discard input when full) so pointers make sense */
			if (Buffer_p->QWriteOffset >= ReadOffset) {
				BytesInBuffer = Buffer_p->QWriteOffset - ReadOffset;
			} else {
				BytesInBuffer = Buffer_p->BufferSize - (ReadOffset - Buffer_p->QWriteOffset);
			}
		}

		if (ST_NO_ERROR == Error) {
			if (BytesInBuffer == 0) {
				Error = STPTI_ERROR_NO_PACKET;
			} else if (ReadAsUnits == stptiHAL_READ_IGNORE_QUANTISATION) {
				/* Read N bytes (DestSize1 + DestSize2) */
				Error = ReturnUnQuantisedZone(DestSize1 + DestSize2, Buffer_p, BytesInBuffer,
							      &ReadOffset, &BytesToRead, &BytesToSkipAtEnd);
			} else {
				if (Buffer_p->SlotCount >= 0) {
					/* Must be a buffer collecting data from a Slot (SlotCount would be -1 if no slots
					   had ever been associated) */

					/* We need to be quantisation unit aware.  Set BytesToRead with the number of bytes to read, 
					   Update ReadOffset and BytesToSkipAtEnd to handle skipping any meta data.  */
					switch (Buffer_p->AssociatedSlotsMode) {
					case stptiHAL_SLOT_TYPE_VIDEO_ES:
					case stptiHAL_SLOT_TYPE_AUDIO_ES:
						/* equivalent of Read N bytes (DestSize1 + DestSize2) */
						/* Copy as much as you can into the users buffer. */
						Error =
						    ReturnUnQuantisedZone(DestSize1 + DestSize2, Buffer_p,
									  BytesInBuffer, &ReadOffset, &BytesToRead,
									  &BytesToSkipAtEnd);
						break;

					case stptiHAL_SLOT_TYPE_PARTIAL_PES:
						/* Data in the buffer is raw transport packets. Manually parse the data to obtain metadata. */
						Error =
						    ReturnPartialPESZone(DestSize1 + DestSize2, MetaData_p,
									 MetaDataSize, Buffer_p, BytesInBuffer,
									 &ReadOffset, &BytesToRead, &BytesToSkipAtEnd);
						break;

					case stptiHAL_SLOT_TYPE_PES:
						/* PES data with metadata (to help sizing non-det PES) */
						Error = ReturnPESZone(DestSize1 + DestSize2, Buffer_p, BytesInBuffer,
								      &ReadOffset, &BytesToRead, &BytesToSkipAtEnd);
						break;

					case stptiHAL_SLOT_TYPE_PCR:
						/* PCR information is 12 bytes (6bytes PCR, 6byte Arrival Time) */
						Error =
						    ReturnFixedSizedZone(DestSize1 + DestSize2, 12, Buffer_p,
									 BytesInBuffer, &ReadOffset, &BytesToRead,
									 &BytesToSkipAtEnd);
						break;

					case stptiHAL_SLOT_TYPE_RAW:
						/* By default output MultiPacket units.   If there is not enough packets in the buffer to do this
						   then copy to the nearest whole packet into the users buffer. */
						Error =
						    ReturnRAWZone(DestSize1 + DestSize2, vDevice_p, Buffer_p,
								  BytesInBuffer, &ReadOffset, &BytesToRead,
								  &BytesToSkipAtEnd);
						break;

					case stptiHAL_SLOT_TYPE_ECM:
					case stptiHAL_SLOT_TYPE_EMM:
					case stptiHAL_SLOT_TYPE_SECTION:
						/* Output a section and only a section.  If there is not enough space in the user's buffer to hold
						   the full section, truncate it (loosing the extra bytes). */
						Error =
						    ReturnSectionZone(DestSize1 + DestSize2, MetaData_p, MetaDataSize,
								      vDevice_p, Buffer_p, BytesInBuffer, &ReadOffset,
								      &BytesToRead, &BytesToSkipAtEnd);
						break;

					default:
						STPTI_PRINTF_ERROR
						    ("Trying to read data from a buffer containing data of a type not supported. %d",
						     Buffer_p->AssociatedSlotsMode);
						Error = ST_ERROR_BAD_PARAMETER;
						break;
					}

					/* if no BytesToRead then there can't be a complete "packet" to return */
					if (ST_NO_ERROR == Error && BytesToRead == 0) {
						Error = STPTI_ERROR_NO_PACKET;
					}
				} else if (Buffer_p->IndexerCount >= 0) {
					/* Must be a buffer collecting indexes (IndexCount would be -1 if no indexers had
					   ever been associated) */

					Error = ReturnIndexZone(DestSize1 + DestSize2, MetaData_p, MetaDataSize,
								vDevice_p, Buffer_p, BytesInBuffer,
								&ReadOffset, &BytesToRead, &BytesToSkipAtEnd);

					/* In most cases BytesToRead will be zero, and the index information will be all 
					   metadata.  One exception to this is AF Private data where the Private data field
					   is returned as data to be copied rather than metadata. */
				} else {
					/* No Slots or Indexers have ever been associated.  Must be an uninitialised buffer. */
					Error = STPTI_ERROR_NO_PACKET;
				}

				if (ReadAsUnits == stptiHAL_READ_AS_UNITS_ALLOW_TRUNCATION
				    && STPTI_ERROR_NOT_ENOUGH_ROOM_TO_RETURN_DATA == Error) {
					Error = ST_NO_ERROR;
				}
			}
		}

		/* At this point...
		   - BytesToRead will be the number of bytes to copy and should be already have been limited by
		   DestSize1+DestSize2 (because we need take into account quantisation).  
		   - ReadOffset will be the point at which we start reading (after metadata). 
		   - BytesToSkipAtEnd will be he number of extra bytes we discard after outputing the data
		 */

		if (ST_NO_ERROR == Error
		    || (STPTI_ERROR_NOT_ENOUGH_ROOM_TO_RETURN_DATA == Error
			&& ReadAsUnits == stptiHAL_READ_AS_UNITS_ALLOW_TRUNCATION)) {
			if (BytesToRead > 0) {
				int Size1BytesToCopy, Size2BytesToCopy;

				if (LeadingBytesToDiscard < BytesToRead) {
					ReadOffset += LeadingBytesToDiscard;
					if (ReadOffset >= Buffer_p->BufferSize) {
						ReadOffset -= Buffer_p->BufferSize;
					}
					BytesToRead -= LeadingBytesToDiscard;
				}

				stptiSupport_InvalidateRegionCircular(Buffer_p, ReadOffset, BytesToRead);

				Size1BytesToCopy = BytesToRead;
				Size2BytesToCopy = 0;
				if (BytesToRead > DestSize1) {
					Size1BytesToCopy = DestSize1;
					Size2BytesToCopy = BytesToRead - DestSize1;
				}

				CopyFromCircularBuffer(Buffer_p, &ReadOffset, DestBuffer1_p, Size1BytesToCopy,
						       CopyFunction);
				CopyFromCircularBuffer(Buffer_p, &ReadOffset, DestBuffer2_p, Size2BytesToCopy,
						       CopyFunction);
			}

			/* Skip any metadata at the end */
			if (BytesToSkipAtEnd > 0) {
				ReadOffset = (ReadOffset + BytesToSkipAtEnd) % Buffer_p->BufferSize;
			}
			*BytesCopied = BytesToRead;
			*ReadOffset_p = ReadOffset;
		} else {
			*BytesCopied = 0;
		}

	}
	stptiOBJMAN_Unlock(BufferHandle, &LocalLockState);

	return (Error);
}

/**
   @brief  Sets the read offset.
   
   This function sets the read Offset.  This is often used for direct access (manual) Buffers (this
   is termed a Software CD FIFO Buffer).
   
   The ReadOffset can be relative to the existing ReadPointer, or Absolute (from the Buffer Base).
   If using relative, you must always advance the ReadOffset (it must be positive, and wrapping is
   handled).
     
   @param  BufferHandle         The Handle of the Buffer
   @param  ReadOffset           Read Offset to set
   
   @return                      A standard st error type...     
                                - ST_ERROR_BAD_PARAMETER  (invalid buffer handle, read pointer outside of buffer)
                                - ST_NO_ERROR
 */
ST_ErrorCode_t stptiHAL_BufferSetReadOffset(FullHandle_t BufferHandle, U32 ReadOffset)
{
	ST_ErrorCode_t Error = ST_NO_ERROR;
	stptiSupport_AccessLockState_t LocalLockState = stptiSupport_UNLOCKED;

	/* Only adjust the read offset and signal if necessary */
	if (ReadOffset != stptiHAL_CURRENT_READ_OFFSET) {
		stptiOBJMAN_WriteLock(BufferHandle, &LocalLockState);
		{
			stptiHAL_Buffer_t *Buffer_p = stptiHAL_GetObjectBuffer_p(BufferHandle);
			stptiHAL_vDevice_t *vDevice_p = stptiHAL_GetObjectvDevice_p(BufferHandle);

			if (ReadOffset >= Buffer_p->BufferSize) {
				/* We do not allow the user to put the read pointer outside of the buffer. */
				Error = ST_ERROR_BAD_PARAMETER;
			} else {
				Buffer_p->ReadOffset = ReadOffset;

				if (Buffer_p->BufferIndex >= 0) {
					stptiHAL_pDeviceXP70Write(&vDevice_p->TP_Interface.
								  DMAInfo_p[Buffer_p->BufferIndex].read_offset,
								  Buffer_p->ReadOffset);
				}

				if (!Buffer_p->DiscardInputOnOverflow) {
					if (Buffer_p->BufferIndex >= 0) {
						/* Check if we need to reset the overflow flag inside the TP - This controls how getbufferfreelen and testfordata behave */
						if (BufferInOverflowOverwrite
						    (stptiHAL_pDeviceXP70Read
						     (&vDevice_p->TP_Interface.
						      DMAOverflowFlags_p[Buffer_p->BufferIndex]))) {
							/* First time read offset been moved since overflow in overwrite case.
							   Set reset flag, this flag tells host read offset has since overflow and buffer is not in overflow state. TP will clear
							   overflow stat on this flag. Host must not write to DMA overflow until TP clears overflow flag (avoid race condition) */
							stptiHAL_pDeviceXP70BitSet(&vDevice_p->TP_Interface.
										   DMAOverflowFlags_p[Buffer_p->
												      BufferIndex],
										   DMA_INFO_MARK_RESET_OVERFLOW);
						}
					} else {
						/* DMA is not allocated in TP, just reset overflow flag in host */
						Buffer_p->DMAOverflowFlag = 0;
					}
				}
			}

			/* Internal signalling decision.  
			   TP only signals if fullness changes from below the threshold to above/equal to it) 
			   Host needs to (directly) signal if above the threshold (done to reduce interrupt load) */
			SignalBufferIfNecessary(Buffer_p, vDevice_p);
		}
		stptiOBJMAN_Unlock(BufferHandle, &LocalLockState);
	}

	return (Error);
}

/**
   @brief  Return the Filled status of the buffer.
   
   The parameters here should be self explanatory.  Note that *FreeSpace_p + *BytesInBuffer_p may 
   not equal *BufferSize_p.  This is because *BytesInBuffer_p reports only complete units of 
   information, and *FreeSpace_p takes into account incomplete units.
   
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
ST_ErrorCode_t stptiHAL_BufferStatus(FullHandle_t BufferHandle, U32 * BufferSize_p,
				     U32 * BytesInBuffer_p, U32 * BufferUnitCount_p,
				     U32 * FreeSpace_p, U32 * UnitsInBuffer_p,
				     U32 * NonUnitBytesInBuffer_p, BOOL * OverflowedFlag_p)
{
	ST_ErrorCode_t Error = ST_NO_ERROR;
	stptiSupport_AccessLockState_t LocalLockState = stptiSupport_UNLOCKED;

	stptiOBJMAN_ReadLock(BufferHandle, &LocalLockState);
	{
		stptiHAL_Buffer_t *Buffer_p = stptiHAL_GetObjectBuffer_p(BufferHandle);

		Error =
		    stptiHAL_BufferState(BufferHandle, BytesInBuffer_p, BufferUnitCount_p, FreeSpace_p, UnitsInBuffer_p,
					 NonUnitBytesInBuffer_p, OverflowedFlag_p);
		if (Error == ST_NO_ERROR && BufferSize_p != NULL) {
			*BufferSize_p = Buffer_p->BufferSize;
		}
	}
	stptiOBJMAN_Unlock(BufferHandle, &LocalLockState);

	return (Error);
}

/**
   @brief  Return the Buffer type.
   
   This function returns what type of buffer this is (determined by what slots are associated).
  
   @param  BufferHandle         The Handle of the Buffer
   @param  InheritedSlotType    The buffers type.
   
   @return                      A standard st error type...     
                                - ST_NO_ERROR
 */
ST_ErrorCode_t stptiHAL_BufferType(FullHandle_t BufferHandle, stptiHAL_SlotMode_t * InheritedSlotType)
{
	ST_ErrorCode_t Error = ST_NO_ERROR;
	stptiSupport_AccessLockState_t LocalLockState = stptiSupport_UNLOCKED;

	stptiOBJMAN_ReadLock(BufferHandle, &LocalLockState);
	{
		stptiHAL_Buffer_t *Buffer_p = stptiHAL_GetObjectBuffer_p(BufferHandle);
		*InheritedSlotType = Buffer_p->AssociatedSlotsMode;
	}
	stptiOBJMAN_Unlock(BufferHandle, &LocalLockState);

	return (Error);
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
ST_ErrorCode_t stptiHAL_BufferSetOverflowControl(FullHandle_t BufferHandle, BOOL DiscardInputOnOverflow)
{
	ST_ErrorCode_t Error = ST_NO_ERROR;
	stptiSupport_AccessLockState_t LocalLockState = stptiSupport_UNLOCKED;

	stptiOBJMAN_ReadLock(BufferHandle, &LocalLockState);
	{
		stptiHAL_vDevice_t *vDevice_p = stptiHAL_GetObjectvDevice_p(BufferHandle);
		stptiHAL_Buffer_t *Buffer_p = stptiHAL_GetObjectBuffer_p(BufferHandle);
		Buffer_p->DiscardInputOnOverflow = DiscardInputOnOverflow;

		stptiHAL_BufferSetThresholdAndOverflow(vDevice_p, Buffer_p);
	}
	stptiOBJMAN_Unlock(BufferHandle, &LocalLockState);

	return (Error);
}

/* Supporting Functions ------------------------------------------------------------------------- */

/**
   @brief  Initialise a DMAInfo entry in the Transport Processor
   
   This function initialises a specified entry in the Transport Processors DMAInfo table.
   
   @param  DMAInfo_p      Pointer to the DMA info entry to be initialised.
   
   @return                A standard st error type...     
                          - ST_NO_ERROR
 */
void stptiHAL_BufferTPEntryInitialise(volatile stptiTP_DMAInfo_t * DMAInfo_p)
{
	stptiHAL_pDeviceXP70Memset((void *)DMAInfo_p, 0, sizeof(stptiTP_DMAInfo_t));
	stptiHAL_pDeviceXP70Write(&DMAInfo_p->signal_threshold, DMA_INFO_NO_SIGNALLING);
}

/**
   @brief  Initialise a DMAInfo entry for signal_threshold and overflow control in the Transport Processor
   
   This function initialises the specified entry in the Transport Processors DMAInfo table.
   
   @param  BufferHandle   BufferHandle to buffer to be set.
   
   @return                A standard st error type...     
                          - ST_NO_ERROR
  */
static void stptiHAL_BufferSetThresholdAndOverflow(stptiHAL_vDevice_t * vDevice_p, stptiHAL_Buffer_t * Buffer_p)
{
	if (Buffer_p->BufferIndex >= 0) {
		U32 signal_threshold = 0;

		volatile stptiTP_DMAInfo_t *DMA_p = &vDevice_p->TP_Interface.DMAInfo_p[Buffer_p->BufferIndex];

		if (!Buffer_p->DiscardInputOnOverflow) {
			/* the start pointer is modified to turn off overflow management */
			signal_threshold = DMA_INFO_ALLOW_OVERFLOW;
		}

		/* Has a signal been associated with this buffer? ) */
		if (Buffer_p->SignallingMessageQ != NULL) {
			/* Setup Signalling */
			signal_threshold |= Buffer_p->SignallingUpperThreshold & (~DMA_INFO_ALLOW_OVERFLOW);
		} else {
			/* Turn off Signalling (by setting a very high threshold) */
			signal_threshold |= DMA_INFO_NO_SIGNALLING;
		}

		stptiHAL_pDeviceXP70Write(&DMA_p->signal_threshold, signal_threshold);
		SignalBufferIfNecessary(Buffer_p, vDevice_p);
	}
}

/**
   @brief  Assign and Configure a DMAInfo entry in the Transport Processor
   
   This function assigns and configures an entry in the Transport Processors DMAInfo table.
   
   @param  vDevice_p      Pointer to the vDevice Object
   @param  Buffer_p       Pointer to the Buffer Object
   
   @return                A standard st error type...     
                          - ST_NO_ERROR
 */
static ST_ErrorCode_t stptiHAL_BufferDMAConfigure(stptiHAL_vDevice_t * vDevice_p, stptiHAL_Buffer_t * Buffer_p)
{
	ST_ErrorCode_t Error = ST_NO_ERROR;

	if (ST_NO_ERROR == stptiOBJMAN_AddToList(vDevice_p->BufferHandles, Buffer_p, FALSE, &Buffer_p->BufferIndex)) {
		if ((U32) Buffer_p->BufferIndex >= vDevice_p->TP_Interface.NumberOfDMAStructures) {
			/* Too many buffers */
			stptiOBJMAN_RemoveFromList(vDevice_p->BufferHandles, Buffer_p->BufferIndex);
			Buffer_p->BufferIndex = -1;
			Error = ST_ERROR_NO_MEMORY;
		}
	}

	if (ST_NO_ERROR == Error) {
		volatile stptiTP_DMAInfo_t *DMA_p = &vDevice_p->TP_Interface.DMAInfo_p[Buffer_p->BufferIndex];

		stptiHAL_BufferTPEntryInitialise(DMA_p);

		stptiHAL_pDeviceXP70Write(&DMA_p->base, Buffer_p->PhysicalBuffer_p);
		stptiHAL_pDeviceXP70Write(&DMA_p->size, Buffer_p->BufferSize);
		stptiHAL_pDeviceXP70Write(&DMA_p->write_offset, Buffer_p->QWriteOffset);
		stptiHAL_pDeviceXP70Write(&DMA_p->qwrite_offset_pending, Buffer_p->QWriteOffset);
		stptiHAL_pDeviceXP70Write(&DMA_p->qwrite_offset, Buffer_p->QWriteOffset);
		stptiHAL_pDeviceXP70Write(&DMA_p->buffer_unit_count, Buffer_p->BufferUnitCount);

		stptiHAL_pDeviceXP70Write(&DMA_p->read_offset, Buffer_p->ReadOffset);

		stptiHAL_pDeviceXP70Write(&vDevice_p->TP_Interface.DMAOverflowFlags_p[Buffer_p->BufferIndex],
					  Buffer_p->DMAOverflowFlag);

		stptiHAL_BufferSetThresholdAndOverflow(vDevice_p, Buffer_p);
	}

	return (Error);
}

/**
   @brief  Windback the Write Pointer to the last quantisation point.
   
   This function windsback the write pointer for a buffer to the last quantisation point.  It is 
   typically done, when a slot is made inactive (pid cleared).  
   
   Note that it is not done for disassociating the buffer as the link the DMA disappears, and hence
   the concept of write pointer also disappears (disassociated buffers only deal in QWrite terms).
   
   @param  BufferHandle   The Handle of the Buffer to windback.
   
   @return                A standard st error type...     
                          - ST_NO_ERROR
 */
ST_ErrorCode_t stptiHAL_BufferWindbackQWrite(FullHandle_t BufferHandle)
{
	/* We make the assumption that the buffer is associated and active */
	stptiHAL_vDevice_t *vDevice_p = stptiHAL_GetObjectvDevice_p(BufferHandle);
	stptiHAL_Buffer_t *Buffer_p = stptiHAL_GetObjectBuffer_p(BufferHandle);

	if (Buffer_p->BufferIndex >= 0) {
		volatile stptiTP_DMAInfo_t *DMA_p = &vDevice_p->TP_Interface.DMAInfo_p[Buffer_p->BufferIndex];
		U32 QWrite = stptiHAL_pDeviceXP70Read(&DMA_p->qwrite_offset);

		stptiHAL_pDeviceXP70Write(&DMA_p->qwrite_offset_pending, QWrite);
		stptiHAL_pDeviceXP70Write(&DMA_p->write_offset, QWrite);
	}

	return (ST_NO_ERROR);
}

/**
   @brief  Wait for the Buffer not be in use by the TP
   
   This function waits for any transactions currently happening in the buffer to complete.  This
   avoids a race condition between the TP updating DMA pointers vs the host doing so.
   
   This function does not disable the buffer.
   
   @param  BufferHandle     A Handle to the buffer to wait for.

   @return                  A standard st error type...
                            - ST_NO_ERROR if no errors      
 */
static ST_ErrorCode_t stptiHAL_BufferTPSync(FullHandle_t BufferHandle)
{
	ST_ErrorCode_t Error = ST_NO_ERROR;
	stptiHAL_Buffer_t *Buffer_p = stptiHAL_GetObjectBuffer_p(BufferHandle);
	stptiHAL_vDevice_t *vDevice_p = stptiHAL_GetObjectvDevice_p(BufferHandle);

	/* Make sure we have no TP Sync's pending */
	stptiSupport_SubMilliSecondWait(stptiHAL_pDeviceXP70Read(&vDevice_p->TP_Interface.pDeviceInfo_p->SyncTP) !=
					SYNC_TP_SYNCHRONISED, &Error);

	if (Error != ST_ERROR_TIMEOUT) {
		/* Wait for the buffer, not to be in use */
		stptiHAL_pDeviceXP70Write(&vDevice_p->TP_Interface.pDeviceInfo_p->SyncTP, (U16) Buffer_p->BufferIndex);
		stptiSupport_SubMilliSecondWait(stptiHAL_pDeviceXP70Read(&vDevice_p->TP_Interface.pDeviceInfo_p->SyncTP)
						!= SYNC_TP_SYNCHRONISED, &Error);
	}

	stptiHAL_pDeviceXP70Write(&vDevice_p->TP_Interface.pDeviceInfo_p->SyncTP, SYNC_TP_SYNCHRONISED);	/* force it to be set (in case of a timeout) */
	return (Error);
}

/**
   @brief  Return the Filled status of the buffer.
   
   The parameters here should be self explanatory.  Note that *FreeSpace_p + *BytesInBuffer_p may 
   not equal *BufferSize_p.  This is because *BytesInBuffer_p reports only complete units of 
   information, and *FreeSpace_p takes into account incomplete units.
   
   @param  BufferHandle       A Handle to the buffer.

   @param  BytesInBuffer_p    Bytes in the buffer that can be read (only complete units of information).

   @param  BufferUnitCount_p  Number of units of information received in the buffer.

   @param  FreeSpace_p        Amount of free space in the buffer (takes into account incomplete units).

   @param  UnitsInBuffer_p    Number of units of information in the buffer. Currently only for index units but can be expanded to be made generic.

   @param NonUnitBytesInBuffer_p Number of bytes data in buffer that are not a contained within standard unit type. Currently only for index additional bytes.

   @return                  A standard st error type...
                            - ST_NO_ERROR if no errors      
 */
ST_ErrorCode_t stptiHAL_BufferState(FullHandle_t BufferHandle, U32 * BytesInBuffer_p, U32 * BufferUnitCount_p,
				    U32 * FreeSpace_p, U32 * UnitsInBuffer_p, U32 * NonUnitBytesInBuffer_p,
				    BOOL * OverflowedFlag_p)
{
	ST_ErrorCode_t Error = ST_NO_ERROR;

	stptiHAL_vDevice_t *vDevice_p = stptiHAL_GetObjectvDevice_p(BufferHandle);
	stptiHAL_Buffer_t *Buffer_p = stptiHAL_GetObjectBuffer_p(BufferHandle);
	BOOL Overflowed = FALSE;
	U8 DMAOverflowFlag;
	U32 ReadOffset, QWriteOffset, WriteOffset, BytesInBuffer;

	/* check the buffer pointer in case an invalid handle was passed in */
	if (Buffer_p != NULL && vDevice_p != NULL) {
		if (Buffer_p->BufferIndex >= 0 && (U32) Buffer_p->BufferIndex >= vDevice_p->TP_Interface.NumberOfDMAStructures) {
			pr_err("%s (%d) index %d\n",__func__, __LINE__, Buffer_p->BufferIndex);
			WARN_ON(1);
		}

		if (Buffer_p->BufferIndex >= 0 && (U32) Buffer_p->BufferIndex < vDevice_p->TP_Interface.NumberOfDMAStructures) {
			U32 QWritePtr =
			    stptiHAL_pDeviceXP70Read(&vDevice_p->TP_Interface.DMAInfo_p[Buffer_p->BufferIndex].
						     qwrite_offset);
			U32 WritePtr =
			    stptiHAL_pDeviceXP70Read(&vDevice_p->TP_Interface.DMAInfo_p[Buffer_p->BufferIndex].
						     write_offset);

			QWriteOffset = QWritePtr;
			WriteOffset = WritePtr;

			/* Update local copies from the TP memory */
			Buffer_p->QWriteOffset = QWriteOffset;
			Buffer_p->BufferUnitCount =
			    stptiHAL_pDeviceXP70Read(&vDevice_p->TP_Interface.DMAInfo_p[Buffer_p->BufferIndex].
						     buffer_unit_count);

			ReadOffset =
			    stptiHAL_pDeviceXP70Read(&vDevice_p->TP_Interface.DMAInfo_p[Buffer_p->BufferIndex].
						     read_offset);

			DMAOverflowFlag =
			    stptiHAL_pDeviceXP70Read(&vDevice_p->TP_Interface.
						     DMAOverflowFlags_p[Buffer_p->BufferIndex]);
		} else {
			QWriteOffset = Buffer_p->QWriteOffset;
			WriteOffset = Buffer_p->QWriteOffset;
			ReadOffset = Buffer_p->ReadOffset;
			DMAOverflowFlag = Buffer_p->DMAOverflowFlag;
		}

		if (!Buffer_p->DiscardInputOnOverflow) {
			if (BufferInOverflowOverwrite(DMAOverflowFlag)) {
				/* host has not set 'reset overflow' flag (done by moving the read offset), so 
				   buffer in overflow state */
				Overflowed = TRUE;
			}
		}

		if (Overflowed == TRUE) {
			BytesInBuffer = Buffer_p->BufferSize;
		} else if (QWriteOffset >= ReadOffset) {
			BytesInBuffer = QWriteOffset - ReadOffset;
		} else {
			BytesInBuffer = Buffer_p->BufferSize - (ReadOffset - QWriteOffset);
		}

		if (BytesInBuffer_p != NULL)
			*BytesInBuffer_p = BytesInBuffer;

		if (FreeSpace_p != NULL) {
			if (Overflowed == TRUE) {
				*FreeSpace_p = 0;
			} else if (ReadOffset > WriteOffset) {
				*FreeSpace_p = ReadOffset - WriteOffset;
			} else {
				*FreeSpace_p = Buffer_p->BufferSize - (WriteOffset - ReadOffset);
			}
		}

		if (BufferUnitCount_p != NULL) {
			*BufferUnitCount_p = Buffer_p->BufferUnitCount;
		}

		if (UnitsInBuffer_p != NULL && BytesInBuffer_p != NULL) {
			if (Buffer_p->IndexerCount >= 0) {
				/* Must be a buffer collecting indexes (IndexCount would be -1 if no indexers had
				   ever been associated) */
				stptiTP_IndexerParcelID_t ParcelID;
				U32 BytesLeftInBuffer = BytesInBuffer;
				U32 ParcelReadOffset = ReadOffset;
				U32 ParcelPayloadStartOffset, ParcelPayloadSize;
				U32 NonUnitBytes = 0;
				*UnitsInBuffer_p = 0;
				if (NonUnitBytesInBuffer_p != NULL) {
					while (ST_NO_ERROR ==
					       (Error =
						ReadIndexParcel(Buffer_p, &BytesLeftInBuffer, &ParcelReadOffset,
								&ParcelID, &ParcelPayloadStartOffset,
								&ParcelPayloadSize))) {
						switch (ParcelID) {
						case INDEXER_EVENT_PARCEL:
							(*UnitsInBuffer_p)++;
							if (ParcelPayloadSize > 4)
								NonUnitBytes += ParcelPayloadSize - 4;
							break;
						case INDEXER_ADDITIONAL_TRANSPORT_EVENT_PARCEL:
							(*UnitsInBuffer_p)++;
							NonUnitBytes += ParcelPayloadSize;
							break;
						case INDEXER_STARTCODE_EVENT_PARCEL:
							(*UnitsInBuffer_p)++;
							NonUnitBytes += (ParcelPayloadSize - 2);
							break;
						default:
							break;
						}
					}
					*NonUnitBytesInBuffer_p = NonUnitBytes;
				}
			}
		}

		if (OverflowedFlag_p != NULL) {
			if (DMAOverflowFlag & (DMA_INFO_MARK_OVERFLOWED_OVERWRITE | DMA_INFO_MARK_OVERFLOWED_DISCARD)) {
				*OverflowedFlag_p = TRUE;
			} else {
				*OverflowedFlag_p = FALSE;
			}
		}
	} else {
		Error = ST_ERROR_INVALID_HANDLE;
		if (!Buffer_p) {
			pr_err("%s (%d) - Error: Buffer_p=NULL\n",__func__, __LINE__);
		}
		if (!vDevice_p ) {
			pr_err("%s (%d) - Error: vDevice_p=NULL\n",__func__, __LINE__);
		}
	}

	if (ST_NO_ERROR != Error) {
		pr_err("%s (%d) - Error: %d\n",__func__, __LINE__, Error);
	}

	return (Error);
}

/**
   @brief  Signal the buffer if above or equal to the threshold.
   
   This function posts a message on the signal queue if there is enough data in the buffer.
  
   @param  Buffer_p             A pointer to the host buffer structure
   @param  vDevice_p            A pointer to the vDevice of the Buffer
   
   @return                      A standard st error type...     
                                - ST_NO_ERROR
 */
static void SignalBufferIfNecessary(stptiHAL_Buffer_t * Buffer_p, stptiHAL_vDevice_t * vDevice_p)
{
	/* Internal signalling decision. 
	   TP only signals if fullness changes from below the threshold to above/equal to it) 
	   Host needs to (directly) signal if above the threshold (done to reduce interrupt load) */

	/* Updating the QWrite AFTER updating the read pointer is essential. */
	if (Buffer_p->BufferIndex >= 0) {
		/* buffer is associated and hence has a TP DMA allocation */
		stptiHAL_pDeviceXP70Write(&Buffer_p->QWriteOffset,
					  vDevice_p->TP_Interface.DMAInfo_p[Buffer_p->BufferIndex].qwrite_offset);
		stptiHAL_pDeviceXP70Write(&Buffer_p->BufferUnitCount,
					  vDevice_p->TP_Interface.DMAInfo_p[Buffer_p->BufferIndex].buffer_unit_count);
	}

	/* Is there a signal associated? */
	if (Buffer_p->SignallingMessageQ != NULL) {
		U32 BufferFullness;

		/* Calculate how full the (circular) buffer is */
		if (Buffer_p->QWriteOffset >= Buffer_p->ReadOffset) {
			BufferFullness = Buffer_p->QWriteOffset - Buffer_p->ReadOffset;
		} else {
			BufferFullness = Buffer_p->BufferSize - (Buffer_p->ReadOffset - Buffer_p->QWriteOffset);
		}

		if (BufferFullness >= Buffer_p->SignallingUpperThreshold) {
			FullHandle_t BufferHandle = stptiOBJMAN_ObjectPointerToHandle(Buffer_p);
			stptiSupport_MessageQueuePostTimeout(Buffer_p->SignallingMessageQ, &BufferHandle, 0);
		}
	}
}

/**
   @brief  Buffer Zoning function, returns the next section header and payload zone in the buffer.
   
   This function returns the next section header and payload zone in the buffer (i.e. how to
   skip the metadata).
  
   @param  DestSize             Number of Bytes in destination buffer
   @param  MetaData_p           Pointer to users MetaData array
   @param  MetaDataSize         Pointer to Metadata array size (in bytes)
   @param  vDevice_p            A pointer to the vDevice structure
   @param  Buffer_p             A pointer to the buffer structure
   @param  BytesInBuffer        Precalculated number of bytes in the buffer (save doing it again)
   @param  ReadOffset_p         (Return) A pointer to where to put the start of the zone
   @param  BytesToRead_p        (Return) A pointer to where to put the number of bytes in the zone
   @param  BytesToSkipAtEnd_p   (Return) number of bytes to skip at the end
   
   @return                      A standard st error type...     
                                - STPTI_ERROR_BUFFER_HAS_NO_METADATA            (one or more slots outputing data without the necessary metadata)
                                - STPTI_ERROR_CORRUPT_DATA_IN_BUFFER            (driver fault - metadata/section alignment issue)
                                - STPTI_ERROR_INCOMPLETE_SECTION_IN_BUFFER      (incomplete section - called read to early)
                                - ST_ERROR_BAD_PARAMETER                        (Metadata buffer NULL or too small)
                                - ST_NO_ERROR                                   
 */
static ST_ErrorCode_t ReturnSectionZone(U32 DestSize, void *MetaData_p, U32 MetaDataSize,
					stptiHAL_vDevice_t * vDevice_p, stptiHAL_Buffer_t * Buffer_p, U32 BytesInBuffer,
					U32 * ReadOffset_p, U32 * BytesToRead_p, U32 * BytesToSkipAtEnd_p)
{
	/* Output a section and only a section.  If there is not enough space in the user's buffer to hold
	   the full section truncate it. */

	/* Format of the section with metadata...
	   |  1 byte  | 8 bytes     | ...     | 1 byte |
	   | SLOT_IDX | MATCH_BYTES | SECTION | CRC    |
	 */

	ST_ErrorCode_t Error = ST_NO_ERROR;

	MetaDataSize = MetaDataSize;	/* Avoid compiler warning */

	*BytesToRead_p = 0;

	if (Buffer_p->AssociatedSlotsWithMetaDataSuppressed > 0) {	/* Unfortunately this check only works if a buffer is associated */
		Error = STPTI_ERROR_BUFFER_HAS_NO_METADATA;
	} else if (BytesInBuffer <
		   SECTION_SLOT_INDEX_LENGTH + SECTION_MATCHBYTES_LENGTH + 3 + SECTION_CRC_METADATA_LENGTH) {
		Error = STPTI_ERROR_CORRUPT_DATA_IN_BUFFER;
	} else {
		stptiSupport_InvalidateRegionCircular(Buffer_p, *ReadOffset_p,
						      SECTION_SLOT_INDEX_LENGTH + SECTION_MATCHBYTES_LENGTH + 3);

		*BytesToRead_p =
		    ((U32) Buffer_p->
		     Buffer_p[(*ReadOffset_p + SECTION_SLOT_INDEX_LENGTH + SECTION_MATCHBYTES_LENGTH +
			       1) % Buffer_p->BufferSize] & 0x0F) << 8;
		*BytesToRead_p |=
		    (U32) Buffer_p->
		    Buffer_p[(*ReadOffset_p + SECTION_SLOT_INDEX_LENGTH + SECTION_MATCHBYTES_LENGTH +
			      2) % Buffer_p->BufferSize];
		*BytesToRead_p += 3;	/* Include the header */
		*BytesToSkipAtEnd_p = SECTION_CRC_METADATA_LENGTH;

		if (*BytesToRead_p > BytesInBuffer) {
			Error = STPTI_ERROR_INCOMPLETE_SECTION_IN_BUFFER;
		}

		if (ST_NO_ERROR == Error) {
			/* We need to output the Matched filters and the CRC validity */
			int i;
			U8 *MatchBytes = &Buffer_p->Buffer_p[*ReadOffset_p];
			U8 MatchBytesIfWrapping[SECTION_SLOT_INDEX_LENGTH + SECTION_MATCHBYTES_LENGTH];
			U32 CRCResultByteOffset =
			    (*ReadOffset_p + SECTION_SLOT_INDEX_LENGTH + SECTION_MATCHBYTES_LENGTH +
			     *BytesToRead_p) % Buffer_p->BufferSize;

			stptiSupport_InvalidateRegionCircular(Buffer_p, CRCResultByteOffset, 1);

			if (*ReadOffset_p + SECTION_SLOT_INDEX_LENGTH + SECTION_MATCHBYTES_LENGTH >
			    Buffer_p->BufferSize) {
				/* MetaData is wrapping */
				for (i = 0; i < SECTION_SLOT_INDEX_LENGTH + SECTION_MATCHBYTES_LENGTH; i++) {
					MatchBytesIfWrapping[i] =
					    Buffer_p->Buffer_p[(*ReadOffset_p + i) % Buffer_p->BufferSize];
				}
				MatchBytes = MatchBytesIfWrapping;
			}

			if (MetaData_p != NULL) {
				stptiHAL_ParseMetaData(vDevice_p, MatchBytes, Buffer_p->Buffer_p[CRCResultByteOffset],
						       (stptiHAL_SectionFilterMetaData_t *) MetaData_p,
						       SECTION_SLOT_INDEX_LENGTH + SECTION_MATCHBYTES_LENGTH +
						       *BytesToRead_p + 1, Buffer_p->FiltersFlushArray_p);
			}
		}

		if (ST_NO_ERROR == Error) {
			/* Too many bytes in the section to fit into the users supplied buffer? */
			if (*BytesToRead_p > DestSize) {
				/* If so truncate the section returned (and throw away the rest) */
				*BytesToSkipAtEnd_p += *BytesToRead_p - DestSize;
				*BytesToRead_p = DestSize;
				Error = STPTI_ERROR_NOT_ENOUGH_ROOM_TO_RETURN_DATA;
			}

			*ReadOffset_p =
			    (*ReadOffset_p + SECTION_SLOT_INDEX_LENGTH +
			     SECTION_MATCHBYTES_LENGTH) % Buffer_p->BufferSize;
		}
	}

	return (Error);
}

/**
   @brief  Buffer Zoning function, returns the next partial PES payload zone in the buffer.
   
   This function returns the next partial PES payload zone in the buffer (i.e. how to
   skip the metadata).
  
   @param  DestSize             Number of Bytes in destination buffer
   @param  MetaData_p           Pointer to users MetaData array
   @param  MetaDataSize         Pointer to Metadata array size (in bytes)
   @param  Buffer_p             A pointer to the buffer structure
   @param  BytesInBuffer        Precalculated number of bytes in the buffer (save doing it again)
   @param  ReadOffset_p         (Return) A pointer to where to put the start of the zone
   @param  BytesToRead_p        (Return) A pointer to where to put the number of bytes in the zone
   @param  BytesToSkipAtEnd_p   (Return) number of bytes to skip at the end
   
   @return                      A standard st error type...     
                                - STPTI_ERROR_NO_PACKET                         (not enough data in buffer)
                                - STPTI_ERROR_CORRUPT_DATA_IN_BUFFER            (driver fault - metadata/section alignment issue)
                                - ST_ERROR_BAD_PARAMETER                        (Metadata buffer NULL or too small)
                                - ST_NO_ERROR                                   
 */
static ST_ErrorCode_t ReturnPartialPESZone(U32 DestSize, void *MetaData_p, U32 MetaDataSize,
					   stptiHAL_Buffer_t * Buffer_p, U32 BytesInBuffer,
					   U32 * ReadOffset_p, U32 * BytesToRead_p, U32 * BytesToSkipAtEnd_p)
{
	/* Data in the buffer is raw transport packets. Manually parse the data to obtain metadata. */

	ST_ErrorCode_t Error = ST_NO_ERROR;

	MetaDataSize = MetaDataSize;	/* No Metadata */
	BytesToSkipAtEnd_p = BytesToSkipAtEnd_p;	/* No bytes to skip (leave at default of 0) */

	*BytesToRead_p = DestSize;

	if (BytesInBuffer < 188) {
		Error = STPTI_ERROR_NO_PACKET;
	} else if (DestSize < 184) {
		Error = STPTI_ERROR_NOT_ENOUGH_ROOM_TO_RETURN_DATA;
	} else {
		U8 ReadPointerAdvance = 0;

		while (*ReadOffset_p != Buffer_p->QWriteOffset) {
			stptiSupport_InvalidateRegionCircular(Buffer_p, *ReadOffset_p, 6);

			/* Check the sync */
			if (Buffer_p->Buffer_p[*ReadOffset_p] != 0x47) {
				STPTI_PRINTF_ERROR("STPTI_ERROR_CORRUPT_DATA_IN_BUFFER - invalid sync byte %x  %u",
						   Buffer_p->Buffer_p[*ReadOffset_p], *ReadOffset_p);
				Error = STPTI_ERROR_CORRUPT_DATA_IN_BUFFER;
				break;
			} else {
				/* Extract the CC etc */
				if (MetaData_p != NULL) {
					((U8 *) MetaData_p)[0] = 0;
					((U8 *) MetaData_p)[0] |=
					    ((Buffer_p->
					      Buffer_p[(*ReadOffset_p +
							1) % Buffer_p->BufferSize] & 0x40) ? 0x40 : 0x00);
					((U8 *) MetaData_p)[0] |=
					    (Buffer_p->Buffer_p[(*ReadOffset_p + 3) % Buffer_p->BufferSize] & 0x0F);

					/* Only check for the discontinuity bit if AF present */
					if ((Buffer_p->Buffer_p[(*ReadOffset_p + 3) % Buffer_p->BufferSize] == 0x20)
					    || (Buffer_p->Buffer_p[(*ReadOffset_p + 3) % Buffer_p->BufferSize] ==
						0x30)) {
						((U8 *) MetaData_p)[0] |=
						    ((Buffer_p->
						      Buffer_p[(*ReadOffset_p +
								5) % Buffer_p->BufferSize] & 0x80) ? 0x80 : 0x00);
					}
				}

				if ((Buffer_p->Buffer_p[(*ReadOffset_p + 3) % Buffer_p->BufferSize] & 0x30) == 0x10) {
					/* Advance the ReadOffset to the payload - no AF */
					ReadPointerAdvance += 4;
					*ReadOffset_p = (*ReadOffset_p + 4) % Buffer_p->BufferSize;
					*BytesToRead_p = 184;
					break;
				} else if ((Buffer_p->Buffer_p[(*ReadOffset_p + 3) % Buffer_p->BufferSize] & 0x30) ==
					   0x30) {
					/* Calculate the payload offset */
					*BytesToRead_p =
					    184 - (Buffer_p->Buffer_p[(*ReadOffset_p + 4) % Buffer_p->BufferSize] + 1);
					ReadPointerAdvance =
					    (Buffer_p->Buffer_p[(*ReadOffset_p + 4) % Buffer_p->BufferSize] + 5);
					if ((ReadPointerAdvance + *BytesToRead_p) != 188) {
						/* Not sure how to handle this one due to corrupt data so skip */
						*ReadOffset_p = (*ReadOffset_p + 188) % Buffer_p->BufferSize;
						*BytesToRead_p = 0;
					} else {
						*ReadOffset_p +=
						    (Buffer_p->Buffer_p[(*ReadOffset_p + 4) % Buffer_p->BufferSize] +
						     5);
						break;
					}
				} else {
					/* no payload to skip it */
					*ReadOffset_p = (*ReadOffset_p + 188) % Buffer_p->BufferSize;
					/* continue processing */
					*BytesToRead_p = 0;
					Error = STPTI_ERROR_NO_PACKET;
				}

			}
		}
		if (*BytesToRead_p > 184) {
			STPTI_PRINTF_ERROR("STPTI_ERROR_CORRUPT_DATA_IN_BUFFER - to many bytes %d", *BytesToRead_p);
			if (MetaData_p != NULL) {
				((U8 *) MetaData_p)[1] = 0;
			}
			Error = STPTI_ERROR_CORRUPT_DATA_IN_BUFFER;
		} else {
			if (MetaData_p != NULL) {
				((U8 *) MetaData_p)[1] = *BytesToRead_p;
			}
		}
	}
	return (Error);
}

/**
   @brief  Buffer Zoning function, returns the next PES payload zone in the buffer.
   
   This function returns the next PES payload zone in the buffer (i.e. how to skip the metadata).
  
   @param  DestSize             Number of Bytes in destination buffer
   @param  Buffer_p             A pointer to the buffer structure
   @param  BytesInBuffer        Precalculated number of bytes in the buffer (save doing it again)
   @param  ReadOffset_p         (Return) A pointer to where to put the start of the zone
   @param  BytesToRead_p        (Return) A pointer to where to put the number of bytes in the zone
   @param  BytesToSkipAtEnd_p   (Return) number of bytes to skip at the end
   
   @return                      A standard st error type...     
                                - STPTI_ERROR_BUFFER_HAS_NO_METADATA            (one or more slots outputing data without the necessary metadata)
                                - STPTI_ERROR_NO_PACKET                         (not enough data in buffer)
                                - STPTI_ERROR_CORRUPT_DATA_IN_BUFFER            (driver fault - metadata/section alignment issue)
                                - ST_ERROR_BAD_PARAMETER                        (Metadata buffer NULL or too small)
                                - ST_NO_ERROR                                   
 */
static ST_ErrorCode_t ReturnPESZone(U32 DestSize, stptiHAL_Buffer_t * Buffer_p, U32 BytesInBuffer,
				    U32 * ReadOffset_p, U32 * BytesToRead_p, U32 * BytesToSkipAtEnd_p)
{
	ST_ErrorCode_t Error = ST_NO_ERROR;

	*BytesToSkipAtEnd_p = PES_METADATA_LENGTH;

	if (Buffer_p->AssociatedSlotsWithMetaDataSuppressed > 0) {	/* Unfortunately this check only works if a buffer is associated */
		Error = STPTI_ERROR_BUFFER_HAS_NO_METADATA;
	} else if (BytesInBuffer < (PES_METADATA_LENGTH + PES_HEADER_LENGTH)) {
		/* An error has occured as we don't have enough data to contain a header + metadata */
		STPTI_PRINTF_ERROR
		    ("STPTI_ERROR_CORRUPT_DATA_IN_BUFFER - Bytes in buffer <(PES_METADATA_LENGTH + PES_HEADER_LENGTH) %u",
		     BytesInBuffer);
		Error = STPTI_ERROR_CORRUPT_DATA_IN_BUFFER;
	} else {
		*BytesToRead_p = DestSize;
		if (*BytesToRead_p > BytesInBuffer) {
			*BytesToRead_p = BytesInBuffer - PES_METADATA_LENGTH;
		}

		/* Check we have a valid PES header first */
		stptiSupport_InvalidateRegionCircular(Buffer_p, *ReadOffset_p, PES_HEADER_LENGTH);

		if ((Buffer_p->Buffer_p[(*ReadOffset_p + 0) % Buffer_p->BufferSize] != 0) ||
		    (Buffer_p->Buffer_p[(*ReadOffset_p + 1) % Buffer_p->BufferSize] != 0) ||
		    (Buffer_p->Buffer_p[(*ReadOffset_p + 2) % Buffer_p->BufferSize] != 1)) {
			STPTI_PRINTF_ERROR("STPTI_ERROR_CORRUPT_DATA_IN_BUFFER - PES header error");
			Error = STPTI_ERROR_CORRUPT_DATA_IN_BUFFER;
		} else {
			U32 PesEnd_p = (U32) & Buffer_p->Buffer_p[Buffer_p->QWriteOffset];
			S32 MetaOffset = Buffer_p->QWriteOffset;

			/* Ok we need to go to the qwrite pointer subtract 4 and read the last metadata which will point to the beginning of the last PES.
			   this process is repeated until we find the first PES packet; this enables us to know how long a non-deterministic PES is in the buffer */

			while (PesEnd_p != (U32) & Buffer_p->Buffer_p[*ReadOffset_p]) {
				MetaOffset = PesEnd_p - (U32) (Buffer_p->Buffer_p) - PES_METADATA_LENGTH;

				if ((PesEnd_p - (U32) (Buffer_p->Buffer_p)) < PES_METADATA_LENGTH) {
					MetaOffset += Buffer_p->BufferSize;
				}

				if (MetaOffset < (S32) Buffer_p->BufferSize) {
					U32 PesOffset;
					stptiSupport_InvalidateRegionCircular(Buffer_p, MetaOffset,
									      PES_METADATA_LENGTH);

					PesOffset =
					    (((U32) (Buffer_p->Buffer_p[(MetaOffset + 0) % Buffer_p->BufferSize]) << 24)
					     | ((U32) (Buffer_p->Buffer_p[(MetaOffset + 1) % Buffer_p->BufferSize]) <<
						16) | ((U32) (Buffer_p->
							      Buffer_p[(MetaOffset +
									2) %
								       Buffer_p->BufferSize]) << 8) | ((U32) (Buffer_p->
													      Buffer_p[(MetaOffset + 3) % Buffer_p->BufferSize])));

					/* Convert to virtual */
					PesEnd_p = ((U32) Buffer_p->Buffer_p) + PesOffset;
				} else {
					Error = STPTI_ERROR_CORRUPT_DATA_IN_BUFFER;
					break;
				}
			}

			if (ST_NO_ERROR == Error) {
				/* Calculate the number of bytes to read */
				*BytesToRead_p = MetaOffset - *ReadOffset_p;

				if (MetaOffset < (S32) * ReadOffset_p) {
					*BytesToRead_p += Buffer_p->BufferSize;
				}
			}

			/* If not enough room for the data, output what we can, and discard the rest */
			if (*BytesToRead_p > DestSize) {
				*BytesToSkipAtEnd_p += *BytesToRead_p - DestSize;
				*BytesToRead_p = DestSize;
				Error = STPTI_ERROR_NOT_ENOUGH_ROOM_TO_RETURN_DATA;
			}
		}
	}

	return (Error);
}

/**
   @brief  Buffer Zoning function, returns the next ts packet zone in the buffer.
   
   This function returns the next ts packet zone in the buffer (i.e. how to
   skip the metadata).
  
   @param  DestSize             Number of Bytes in destination buffer
   @param  vDevice_p            A pointer to the vDevice structure
   @param  Buffer_p             A pointer to the buffer structure
   @param  BytesInBuffer        Precalculated number of bytes in the buffer (save doing it again)
   @param  ReadOffset_p         (Return) A pointer to where to put the start of the zone
   @param  BytesToRead_p        (Return) A pointer to where to put the number of bytes in the zone
   @param  BytesToSkipAtEnd_p   (Return) number of bytes to skip at the end
   
   @return                      A standard st error type...     
                                - STPTI_ERROR_CORRUPT_DATA_IN_BUFFER            (driver fault - metadata/section alignment issue)
                                - ST_ERROR_BAD_PARAMETER                        (Metadata buffer NULL or too small)
                                - ST_NO_ERROR                                   
 */
static ST_ErrorCode_t ReturnRAWZone(U32 DestSize, stptiHAL_vDevice_t * vDevice_p, stptiHAL_Buffer_t * Buffer_p,
				    U32 BytesInBuffer, U32 * ReadOffset_p, U32 * BytesToRead_p,
				    U32 * BytesToSkipAtEnd_p)
{
	ST_ErrorCode_t Error = ST_NO_ERROR;
	U32 PacketSize = vDevice_p->PacketSize;

	ReadOffset_p = ReadOffset_p;	/* No need to adjust read offset */

	/* By default output MultiPacket units.   If there is not enough packets in the buffer to do this
	   then copy to the nearest whole packet into the users buffer. */

	if (Buffer_p->SignallingUpperThreshold == DMA_INFO_NO_SIGNALLING
	    || Buffer_p->SignallingUpperThreshold < PacketSize) {
		/* If Signalling is turned off, or Signalling threshold too low, then read a packets worth */
		*BytesToRead_p = PacketSize;
	} else {
		*BytesToRead_p = Buffer_p->SignallingUpperThreshold;
		*BytesToRead_p -= *BytesToRead_p % PacketSize;
	}

	if (*BytesToRead_p > BytesInBuffer) {
		*BytesToRead_p = BytesInBuffer;
		if ((*BytesToRead_p % PacketSize) != 0) {
			STPTI_PRINTF_ERROR("Corrupt data in buffer %d.", *BytesToRead_p);
			Error = STPTI_ERROR_CORRUPT_DATA_IN_BUFFER;
		}
	}

	/* If not enough room for the data, output what we can, and discard the rest */
	if (*BytesToRead_p > DestSize) {
		*BytesToSkipAtEnd_p += *BytesToRead_p - DestSize;
		*BytesToRead_p = DestSize;
	}

	return (Error);
}

/**
   @brief  Buffer Zoning function, returns the next fixed sized zone in the buffer.
   
   This function returns the next fixed sized zone in the buffer 
  
   @param  DestSize             Number of Bytes in destination buffer
   @param  PacketSize           Number of bytes requesting to return
   @param  Buffer_p             A pointer to the buffer structure
   @param  BytesInBuffer        Precalculated number of bytes in the buffer (save doing it again)
   @param  ReadOffset_p         (Return) A pointer to where to put the start of the zone
   @param  BytesToRead_p        (Return) A pointer to where to put the number of bytes in the zone
   @param  BytesToSkipAtEnd_p   (Return) number of bytes to skip at the end
   
   @return                      A standard st error type...     
                                - STPTI_ERROR_CORRUPT_DATA_IN_BUFFER            (driver fault - metadata/section alignment issue)
                                - ST_ERROR_BAD_PARAMETER                        (Metadata buffer NULL or too small)
                                - ST_NO_ERROR                                   
 */
static ST_ErrorCode_t ReturnFixedSizedZone(U32 DestSize, U32 PacketSize, stptiHAL_Buffer_t * Buffer_p,
					   U32 BytesInBuffer, U32 * ReadOffset_p, U32 * BytesToRead_p,
					   U32 * BytesToSkipAtEnd_p)
{
	ST_ErrorCode_t Error = ST_NO_ERROR;

	ReadOffset_p = ReadOffset_p;	/* No need to adjust read offset */
	Buffer_p = Buffer_p;

	/* If Signalling is turned off, or Signalling threshold too low, then read a packets worth */
	*BytesToRead_p = PacketSize;

	if (*BytesToRead_p > BytesInBuffer) {
		*BytesToRead_p = BytesInBuffer;
		STPTI_PRINTF_ERROR("Corrupt data in buffer %d.", *BytesToRead_p);
		Error = STPTI_ERROR_CORRUPT_DATA_IN_BUFFER;
	}

	/* If not enough room for the data, output what we can, and discard the rest */
	if (*BytesToRead_p > DestSize) {
		*BytesToSkipAtEnd_p += *BytesToRead_p - DestSize;
		*BytesToRead_p = DestSize;
		Error = STPTI_ERROR_NOT_ENOUGH_ROOM_TO_RETURN_DATA;
	}

	return (Error);
}

static ST_ErrorCode_t ReadIndexParcel(stptiHAL_Buffer_t * Buffer_p, U32 * BytesLeftInBuffer, U32 * ParcelReadOffset_p,
				      stptiTP_IndexerParcelID_t * ParcelID_p, U32 * ParcelPayloadStartOffset_p,
				      U32 * ParcelPayloadSize_p)
{
	ST_ErrorCode_t Error = ST_NO_ERROR;
	U32 ParcelPayloadSize = 0;	/* will exclude ParcelID and Size field */

	if (*BytesLeftInBuffer == 0) {
		Error = STPTI_ERROR_NO_PACKET;
	} else if (*BytesLeftInBuffer < 2) {
		Error = STPTI_ERROR_CORRUPT_DATA_IN_BUFFER;
	}

	if (ST_NO_ERROR == Error) {
		stptiSupport_InvalidateRegionCircular(Buffer_p, *ParcelReadOffset_p, 2);
		*ParcelID_p = (stptiTP_IndexerParcelID_t) Buffer_p->Buffer_p[*ParcelReadOffset_p];
		ParcelPayloadSize = Buffer_p->Buffer_p[(*ParcelReadOffset_p + 1) % Buffer_p->BufferSize];
		if (*BytesLeftInBuffer < (ParcelPayloadSize + 2)) {
			Error = STPTI_ERROR_CORRUPT_DATA_IN_BUFFER;
		} else {
			*ParcelPayloadStartOffset_p = (*ParcelReadOffset_p + 2) % Buffer_p->BufferSize;

			*BytesLeftInBuffer = *BytesLeftInBuffer - ParcelPayloadSize - 2;
			*ParcelReadOffset_p = (*ParcelReadOffset_p + 2 + ParcelPayloadSize) % Buffer_p->BufferSize;
		}
	}
	*ParcelPayloadSize_p = ParcelPayloadSize;

	if (ST_NO_ERROR != Error) {
		pr_err("%s (%d) - Error: %d\n",__func__, __LINE__, Error);
	}

	return (Error);
}

/**
   @brief  Buffer Zoning function, returns the next index in the buffer.
   
   This function returns the next index in the buffer, advancing the read pointer past the index.
   The returned information is all metadata, it does not copy any data.
  
   @param  DestSize             Number of Bytes in destination buffer
   @param  MetaData_p           Pointer to users MetaData array
   @param  MetaDataSize         Pointer to Metadata array size (in bytes)
   @param  vDevice_p            A pointer to the vDevice structure
   @param  Buffer_p             A pointer to the buffer structure
   @param  BytesInBuffer        Precalculated number of bytes in the buffer (save doing it again)
   @param  ReadOffset_p         (Return) A pointer to where to put the start of the zone
   @param  BytesToRead_p        (Return) A pointer to where to put the number of bytes in the zone
   @param  BytesToSkipAtEnd_p   (Return) number of bytes to skip at the end
   
   @return                      A standard st error type...     
                                - STPTI_ERROR_BUFFER_HAS_NO_METADATA            (one or more slots outputing data without the necessary metadata)
                                - STPTI_ERROR_CORRUPT_DATA_IN_BUFFER            (driver fault - metadata/section alignment issue)
                                - STPTI_ERROR_INCOMPLETE_SECTION_IN_BUFFER      (incomplete section - called read to early)
                                - ST_ERROR_BAD_PARAMETER                        (Metadata buffer NULL or too small)
                                - ST_NO_ERROR                                   
 */
static ST_ErrorCode_t ReturnIndexZone(U32 DestSize, void *MetaData_p, U32 MetaDataSize,
				      stptiHAL_vDevice_t * vDevice_p, stptiHAL_Buffer_t * Buffer_p, U32 BytesInBuffer,
				      U32 * ReadOffset_p, U32 * BytesToRead_p, U32 * BytesToSkipAtEnd_p)
{
	ST_ErrorCode_t Error = ST_NO_ERROR;
	stptiTP_IndexerParcelID_t ParcelID;
	U32 BytesLeftInBuffer = BytesInBuffer;
	U32 ParcelReadOffset = *ReadOffset_p;
	U32 ParcelPayloadStartOffset, ParcelPayloadSize;
	stptiHAL_IndexEventData_t *IndexEventData_p = MetaData_p;
	BOOL OutputTruncated = FALSE;

	U32 BytesToSkipAtEnd = 0;

	MetaDataSize = MetaDataSize;	/* Guard against assert removal */

	STPTI_ASSERT(MetaDataSize >= sizeof(stptiHAL_IndexEventData_t), "MetaData size mismatch");

	if (IndexEventData_p == NULL) {
		/* Makes no sense to call this function without Metadata */
		Error = ST_ERROR_BAD_PARAMETER;
	} else {
		memset(IndexEventData_p, 0x00, sizeof(stptiHAL_IndexEventData_t));

		while (ST_NO_ERROR ==
		       (Error =
			ReadIndexParcel(Buffer_p, &BytesLeftInBuffer, &ParcelReadOffset, &ParcelID,
					&ParcelPayloadStartOffset, &ParcelPayloadSize))) {
			if (ParcelID != INDEXER_INDEX_PARCEL) {
				break;
			}
		}
	}

	/* Findout what Index Parcel awaits us */
	if (ST_NO_ERROR == Error) {
		stptiSupport_InvalidateRegionCircular(Buffer_p, ParcelPayloadStartOffset, ParcelPayloadSize);

#if defined( STPTI_PRINT )
		{
			int i, offs;
			char string[256];
			const int len = sizeof(string);
			U8 *p = &Buffer_p->Buffer_p[ParcelPayloadStartOffset];

			offs = scnprintf(string, len, "%02x (%d): ", (unsigned)ParcelID, ParcelPayloadSize);
			for (i = 0; i < ParcelPayloadSize; i++) {
				offs += scnprintf(string + offs, len - offs, "%02x ", (unsigned)*(p++));
			}
			scnprintf(string + offs, len - offs, "\n");
			stpti_printf("%s", string);
		}
#endif

		/* Get index information */
		switch (ParcelID) {
			/* Rules for handling event parcels...

			   - If passing data with the event then set *ReadOffset_p to the start of the data, and
			   set *BytesToRead_p to its size.

			   - If NOT passing data with the event then set *ReadOffset_p to the start of the next
			   parcel, and set *BytesToRead_p to 0.

			   - BytesToSkipAtEnd can be set to skip bytes at the end of the parcel (default is 0).

			   The point being...
			   *ReadOffset_p + *BytesToRead_p + BytesToSkipAtEnd   _MUST_ point to the start of next parcel.

			   Failure to do this will mean that parcels will not be removed correctly, and we will
			   get misaligned.

			 */

		case INDEXER_EVENT_PARCEL:
			/* Format...  1byte PARCEL ID, 1byte Parcel size (4 or 9 for PTS), 4bytes EVENT bitmask (MSByte first) */
			{
				CopyFromCircularBuffer(Buffer_p, &ParcelPayloadStartOffset,
						       (U8 *) & IndexEventData_p->EventFlags, 4,
						       stptiHAL_IndexMemcpyTransfer);
				if (ParcelPayloadSize == 4) {
					*ReadOffset_p = ParcelReadOffset;	/* don't output any "data", it is all metadata */
					*BytesToRead_p = 0;
				} else {
					*ReadOffset_p = ParcelPayloadStartOffset;
					if ((ParcelPayloadSize - 4) > DestSize) {
						*BytesToRead_p = DestSize;
						BytesToSkipAtEnd = (ParcelPayloadSize - 4) - DestSize;
						OutputTruncated = TRUE;
					} else {
						*BytesToRead_p = ParcelPayloadSize - 4;
					}
				}
			}
			break;

		case INDEXER_ADDITIONAL_TRANSPORT_EVENT_PARCEL:
			/* Format...  1byte PARCEL ID, 1byte Parcel size (2), 1byte adaptation field flags, 1byte reserved */
			{
				IndexEventData_p->AdditionalFlags = Buffer_p->Buffer_p[ParcelPayloadStartOffset];
				*ReadOffset_p = ParcelPayloadStartOffset;
				if (ParcelPayloadSize > DestSize) {
					*BytesToRead_p = DestSize;
					BytesToSkipAtEnd = ParcelPayloadSize - DestSize;
					OutputTruncated = TRUE;
				} else {
					*BytesToRead_p = ParcelPayloadSize;
				}

				if (IndexEventData_p->AdditionalFlags & stptiHAL_AF_PCR_FLAG) {
					U8 PCRBytes[6];
					U32 PCROffset = (ParcelPayloadStartOffset + 1) % Buffer_p->BufferSize;

					/* need to get PCR value from Buffer */
					CopyFromCircularBuffer(Buffer_p, &PCROffset, PCRBytes, 6,
							       stptiHAL_IndexMemcpyTransfer);

					IndexEventData_p->PCR27MHzDiv300Bit32 = (PCRBytes[0] & 0x80) ? 1 : 0;
					IndexEventData_p->PCR27MHzDiv300Bit31to0 =
					    ((U32) PCRBytes[0] << 25) | ((U32) PCRBytes[1] << 17) | ((U32) PCRBytes[2]
												     << 9) | ((U32)
													      PCRBytes
													      [3] << 1)
					    | ((U32) PCRBytes[4] >> 7);
					IndexEventData_p->PCR27MHzModulus300 =
					    ((U16) (PCRBytes[4] & 1) << 8) | ((U16) PCRBytes[5]);
				}
			}
			break;

		case INDEXER_STARTCODE_EVENT_PARCEL:
			/* Format...  1byte PARCEL ID, 1byte Parcel size, 1byte start code value, 1byte offset, context bytes... */
			{
				U8 StartCode = Buffer_p->Buffer_p[ParcelPayloadStartOffset];
				U8 StartCodeOffsetIntoPacket =
				    188 - Buffer_p->Buffer_p[(ParcelPayloadStartOffset + 1) % Buffer_p->BufferSize];
				U8 IPB_data = Buffer_p->Buffer_p[(ParcelPayloadStartOffset + 3) % Buffer_p->BufferSize];	/* 2nd context byte is the IPB identifier for MPEG2 */
				U32 BytesToRead;

				IndexEventData_p->NumberOfMPEGStartCodes = 1;	/* We only get 1 start code per parcel */
				IndexEventData_p->MPEGStartCodeValue = StartCode;
				IndexEventData_p->MPEGStartCodeOffset = StartCodeOffsetIntoPacket;
				IndexEventData_p->MPEGStartIPB = IPB_data;

				*ReadOffset_p = ParcelPayloadStartOffset + 2;	/* Skip over startcode and offset */
				BytesToRead = ParcelPayloadSize - 2;
				if (BytesToRead > DestSize) {
					*BytesToRead_p = DestSize;
					BytesToSkipAtEnd = BytesToRead - DestSize;
					OutputTruncated = TRUE;
				} else {
					*BytesToRead_p = BytesToRead;
				}
			}
			break;

		default:
			Error = STPTI_ERROR_CORRUPT_DATA_IN_BUFFER;
			break;
		}
	}

	if (ST_NO_ERROR == Error) {
		BOOL IndexParcelImmediateFollowsEventParcel = TRUE;
		/* Got Event, need to search for index (to get timestamp and position information) */
		while (ST_NO_ERROR ==
		       (Error =
			ReadIndexParcel(Buffer_p, &BytesLeftInBuffer, &ParcelReadOffset, &ParcelID,
					&ParcelPayloadStartOffset, &ParcelPayloadSize))) {
			if (ParcelID == INDEXER_INDEX_PARCEL) {
				break;
			} else {
				IndexParcelImmediateFollowsEventParcel = FALSE;
			}
		}

		if (ST_NO_ERROR == Error) {
			/* Found index parcel */
			U8 IndexParcel[32];
			U32 ArrivalTime0, ArrivalTime1;
			U16 BufferIndex, SlotIndex;
			U32 BufferUnitCount, WriteOffset;

			if (sizeof(IndexParcel) < ParcelPayloadSize) {
				STPTI_PRINTF_ERROR("STPTI_ERROR_CORRUPT_DATA_IN_BUFFER: Index Parcel size too big");
				Error = STPTI_ERROR_CORRUPT_DATA_IN_BUFFER;
			} else {
				stptiSupport_InvalidateRegionCircular(Buffer_p, ParcelPayloadStartOffset,
								      ParcelPayloadSize);

				CopyFromCircularBuffer(Buffer_p, &ParcelPayloadStartOffset, IndexParcel,
						       ParcelPayloadSize, stptiHAL_IndexMemcpyTransfer);

				ArrivalTime0 =
				    (U32) IndexParcel[0] | ((U32) IndexParcel[1] << 8) | ((U32) IndexParcel[2] << 16) |
				    ((U32) IndexParcel[3] << 24);
				ArrivalTime1 = (U32) IndexParcel[4] | ((U32) IndexParcel[5] << 8);
				BufferIndex = (U16) IndexParcel[6] | ((U16) IndexParcel[7] << 8);
				SlotIndex = (U16) IndexParcel[8] | ((U16) IndexParcel[9] << 8);
				BufferUnitCount =
				    (U32) IndexParcel[10] | ((U32) IndexParcel[11] << 8) | ((U32) IndexParcel[12] << 16)
				    | ((U32) IndexParcel[13] << 24);
				WriteOffset =
				    (U32) IndexParcel[14] | ((U32) IndexParcel[15] << 8) | ((U32) IndexParcel[16] << 16)
				    | ((U32) IndexParcel[17] << 24);

				IndexEventData_p->IndexedSlotHandle = stptiOBJMAN_NullHandle;

				if (SlotIndex != 0xFFFF) {
					stptiHAL_Slot_t *Slot_p =
					    (stptiHAL_Slot_t *) stptiOBJMAN_GetItemFromList(vDevice_p->SlotHandles,
											    SlotIndex);
					if (NULL != Slot_p) {
						IndexEventData_p->IndexedSlotHandle = Slot_p->ObjectHeader.Handle;
					}
				}

				if (BufferIndex != 0xFFFF) {
					stptiHAL_Buffer_t *IndexedBuffer_p =
					    (stptiHAL_Buffer_t *) stptiOBJMAN_GetItemFromList(vDevice_p->BufferHandles,
											      BufferIndex);
					if (NULL != IndexedBuffer_p) {
						IndexEventData_p->IndexedBufferHandle =
						    IndexedBuffer_p->ObjectHeader.Handle;
						IndexEventData_p->BufferPacketCount = BufferUnitCount;
						IndexEventData_p->BufferOffset = WriteOffset;
					}
				} else {
					IndexEventData_p->IndexedBufferHandle = stptiOBJMAN_NullHandle;
				}

				IndexEventData_p->Clk27MHzDiv300Bit32 = (ArrivalTime1 & 0x200) >> 9;
				IndexEventData_p->Clk27MHzDiv300Bit31to0 =
				    ((ArrivalTime1 & 0x1FF) << 23) | (ArrivalTime0 >> 9);
				IndexEventData_p->Clk27MHzModulus300 = (ArrivalTime0 & 0x1FF);

				/* If IndexParcelImmediateFollowsEventParcel then we need to remove it as there are no
				   more indexes relevent for it, and make sure we signal when the next event/index is 
				   added. */
				if (IndexParcelImmediateFollowsEventParcel) {
					BytesToSkipAtEnd += ParcelPayloadSize + 2;
					*BytesToSkipAtEnd_p = BytesToSkipAtEnd;
				}
			}
		}
	}

	if (OutputTruncated) {
		Error = STPTI_ERROR_NOT_ENOUGH_ROOM_TO_RETURN_DATA;
	}

	return (Error);
}

/**
   @brief  Buffer Zoning function, but exists here only to present a consistent API.
   
   This function returns the next available series of bytes for the read function.
  
   @param  DestSize             Number of Bytes in destination buffer
   @param  Buffer_p             A pointer to the buffer structure
   @param  BytesInBuffer        Precalculated number of bytes in the buffer (save doing it again)
   @param  ReadOffset_p         (Return) A pointer to where to put the start of the zone
   @param  BytesToRead_p        (Return) A pointer to where to put the number of bytes in the zone
   @param  BytesToSkipAtEnd_p   (Return) number of bytes to skip at the end
   
   @return                      A standard st error type...     
                                - ST_NO_ERROR                                   
 */
static ST_ErrorCode_t ReturnUnQuantisedZone(U32 DestSize, stptiHAL_Buffer_t * Buffer_p, U32 BytesInBuffer,
					    U32 * ReadOffset_p, U32 * BytesToRead_p, U32 * BytesToSkipAtEnd_p)
{
	/* equivalent of Read N bytes (DestSize) */
	/* Copy as much as you can into the users buffer. */

	Buffer_p = Buffer_p;
	ReadOffset_p = ReadOffset_p;	/* no need to advance ReadOffset */
	BytesToSkipAtEnd_p = BytesToSkipAtEnd_p;	/* no need to discard bytes at the end */

	*BytesToRead_p = DestSize;
	if (*BytesToRead_p > BytesInBuffer) {
		*BytesToRead_p = BytesInBuffer;
	}
	return (ST_NO_ERROR);
}

/**
   @brief  Copy data from a Circular Buffer to a specified destination.
   
   This function copies data from a Buffer (circular) into a the specified memory region.  It uses a
   passed in function to do the copying to allow the option of having different copying mechanisms
   (e.g. kernel_userspace_copy under linux, or FDMA copy).
   
   @param  BufferBase_p         The base of the buffer to copy out from
   @param  ReadOffset           The offset into the buffer to start copying from
   @param  BufferSize           The size of the buffer
   @param  DestBuffer           The start of the destination
   @param  DestBytesToCopy      The number of bytes to copy
   @param  CopyFunction         The function to use to do the copy
   
   @return                      A standard st error type...     
                                - ST_NO_ERROR
 */
static ST_ErrorCode_t CopyFromCircularBuffer(stptiHAL_Buffer_t * Buffer_p, U32 * ReadOffset, U8 * DestBuffer,
					     U32 DestBytesToCopy, ST_ErrorCode_t(*CopyFunction) (void **, const void *,
												 size_t))
{
	ST_ErrorCode_t Error = ST_NO_ERROR;
	if (DestBytesToCopy > 0) {
		U32 part1, part2 = 0;

		part1 = Buffer_p->BufferSize - *ReadOffset;
		if (part1 > DestBytesToCopy) {
			part1 = DestBytesToCopy;	/* part1 = BytesToRead, part2 = 0 */
		} else {
			part2 = DestBytesToCopy - part1;	/* part1 = BytesToEndOfBuffer, part2 = BytesToRead-BytesToEndOfBuffer */
		}

		Error = (*CopyFunction) ((void **)&DestBuffer, &Buffer_p->Buffer_p[*ReadOffset], (size_t) part1);	/* note DestBuffer will be advanced by the CopyFunction */
		if (ST_NO_ERROR == Error) {
			*ReadOffset += part1;
			if (*ReadOffset == Buffer_p->BufferSize) {
				*ReadOffset = 0;
			}
			if (part2 > 0) {
				Error = (*CopyFunction) ((void **)&DestBuffer, Buffer_p->Buffer_p, (size_t) part2);
				if (ST_NO_ERROR == Error) {
					*ReadOffset = part2;
				}
			}
		}
	}
	return (Error);
}

/**
   @brief  Invalidate a Region of Memory
   
   This function invalidates a region of memory handling the circular nature of a buffer.
   
   @param  Buffer_p             The Buffer Object pointer
   @param  Offset               The offset into the buffer to start invalidating from
   @param  Size                 The number of bytes to invalidate
   
 */
static void stptiSupport_InvalidateRegionCircular(stptiHAL_Buffer_t * Buffer_p, unsigned int Offset, unsigned int Size)
{
	U8 *Base_p = Buffer_p->Buffer_p;
	U32 BufferSize = Buffer_p->BufferSize;

	if (Offset + Size > BufferSize) {
		stptiSupport_InvalidateRegion(&Buffer_p->CachedMemoryStructure, (void *)&Base_p[Offset],
					      BufferSize - Offset);
		stptiSupport_InvalidateRegion(&Buffer_p->CachedMemoryStructure, (void *)Base_p,
					      Size - (BufferSize - Offset));
	} else {
		stptiSupport_InvalidateRegion(&Buffer_p->CachedMemoryStructure, (void *)&Base_p[Offset], Size);
	}
}

/**
   @brief  Memcpy helper function
   
   This function performs a memcpy of data.  It is wrapped to provide a uniform interface for
   the copying functions.
   
   @param  Dest                 Destination Address.
   @param  Src                  Source Address.
   @param  CopySize             The number of bytes to copy.
   
   @return                      nothing
 */
static ST_ErrorCode_t stptiHAL_IndexMemcpyTransfer(void **Dest_pp, const void *Src_p, size_t CopySize)
{
	memcpy(*Dest_pp, Src_p, CopySize);
	*Dest_pp = (void *)((U8 *) (*Dest_pp) + CopySize);
	return (ST_NO_ERROR);
}
