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
   @file   pti_index.c
   @brief  Index Object Initialisation, Termination and Manipulation Functions.

   This file implements the functions for creating, destroying and accessing PTI indexes.

 */

#if 0
#define STPTI_PRINT
#endif

/* Includes ---------------------------------------------------------------- */
#include "linuxcommon.h"

/* Includes from API level */
#include "../pti_debug.h"
#include "../pti_hal_api.h"

/* Includes from the HAL / ObjMan level */
#include "pti_vdevice.h"
#include "pti_slot.h"
#include "pti_index.h"
#include "pti_buffer.h"

/* MACROS ------------------------------------------------------------------ */

/* Private Constants ------------------------------------------------------- */
#define MAX_INDEX_SLOTS (32)

/* Private Variables ------------------------------------------------------- */
/* Private Function Prototypes --------------------------------------------- */
ST_ErrorCode_t stptiHAL_IndexAllocator(FullHandle_t IndexHandle, void *params_p);
ST_ErrorCode_t stptiHAL_IndexAssociator(FullHandle_t IndexHandle, FullHandle_t AssocObjectHandle);
ST_ErrorCode_t stptiHAL_IndexDisassociator(FullHandle_t IndexHandle, FullHandle_t AssocObjectHandle);
ST_ErrorCode_t stptiHAL_IndexDeallocator(FullHandle_t IndexHandle);
ST_ErrorCode_t stptiHAL_IndexTransportEvents(FullHandle_t IndexHandle, stptiHAL_EventFlags_t EventFlags,
					     stptiHAL_AdditionalTransportFlags_t AdditionalFlags, BOOL Enable);
ST_ErrorCode_t stptiHAL_IndexOutputStartCodes(FullHandle_t IndexHandle, U32 *StartCodeMask,
					      stptiHAL_IndexerStartCodeMode_t Mode);
ST_ErrorCode_t stptiHAL_IndexReset(FullHandle_t IndexHandle);
ST_ErrorCode_t stptiHAL_IndexChain(FullHandle_t IndexHandle, FullHandle_t *Indexes2Chain, int NumberOfIndexes);

/* Public Constants -------------------------------------------------------- */

/* Export the API */
const stptiHAL_IndexAPI_t stptiHAL_IndexAPI = {
	{
		/* Allocator             Associator                Disassociator                Deallocator */
		stptiHAL_IndexAllocator, stptiHAL_IndexAssociator, stptiHAL_IndexDisassociator, stptiHAL_IndexDeallocator,
		NULL, NULL
	},
	stptiHAL_IndexTransportEvents,
	stptiHAL_IndexOutputStartCodes,
	stptiHAL_IndexReset,
	stptiHAL_IndexChain
};

/* Object Management Functions ------------------------------------------------------------------ */

/**
   @brief  Allocate this Index

   This function allocates the Index Object

   @param  IndexHandle          The handle of the Index.
   @param  params_p             The parameters for this Index.

   @return                      A standard st error type...
                                - ST_NO_ERROR
 */
ST_ErrorCode_t stptiHAL_IndexAllocator(FullHandle_t IndexHandle, void *params_p)
{
	ST_ErrorCode_t Error = ST_NO_ERROR;

	/* Already write locked */
	stptiHAL_Index_t *Index_p = stptiHAL_GetObjectIndex_p(IndexHandle);
	stptiHAL_vDevice_t *vDevice_p = stptiHAL_GetObjectvDevice_p(IndexHandle);

	params_p = params_p;

	if (ST_NO_ERROR == stptiOBJMAN_AddToList(vDevice_p->IndexHandles, Index_p, FALSE, &Index_p->IndexerIndex)) {
		if (Index_p->IndexerIndex >= (int)vDevice_p->TP_Interface.NumberOfIndexers) {
			/* Too many indexes */
			stptiOBJMAN_RemoveFromList(vDevice_p->IndexHandles, Index_p->IndexerIndex);
			Error = ST_ERROR_NO_MEMORY;
		} else {
			stptiHAL_IndexerTPEntryInitialise(&vDevice_p->TP_Interface.IndexerInfo_p[Index_p->IndexerIndex],
							  Index_p->IndexerIndex);
		}
	}

	return (Error);
}

/**
   @brief  Assocate this Index

   This function assocate the Index, to either slot or a pid.

   @param  IndexHandle           The handle of the Index.
   @param  AssocObjectHandle     The handle of the object to be associated.

   @return                      A standard st error type...
                                - ST_NO_ERROR
 */
ST_ErrorCode_t stptiHAL_IndexAssociator(FullHandle_t IndexHandle, FullHandle_t AssocObjectHandle)
{
	/* Already write locked */
	ST_ErrorCode_t Error = ST_NO_ERROR;

	switch (stptiOBJMAN_GetObjectType(AssocObjectHandle)) {
	case OBJECT_TYPE_SLOT:
		break;

	case OBJECT_TYPE_BUFFER:
		{
			stptiHAL_vDevice_t *vDevice_p = stptiHAL_GetObjectvDevice_p(IndexHandle);
			stptiHAL_Index_t *Index_p = stptiHAL_GetObjectIndex_p(IndexHandle);
			stptiHAL_Buffer_t *Buffer_p = stptiHAL_GetObjectBuffer_p(AssocObjectHandle);
			stptiHAL_pDeviceXP70Write(&vDevice_p->TP_Interface.IndexerInfo_p[Index_p->IndexerIndex].output_dma_record, Buffer_p->BufferIndex);	/* Buffer assoc fn will have been called before this one */
		}
		break;

	default:
		STPTI_PRINTF_ERROR("Index cannot associate to this kind of object %d",
				   stptiOBJMAN_GetObjectType(AssocObjectHandle));
		Error = ST_ERROR_BAD_PARAMETER;
		break;
	}

	return (Error);
}

/**
   @brief  Disassociate this Index from another object

   This function disassociates the index, from either a slot or a pid.

   @param  IndexHandle          The handle of the index.
   @param  AssocObjectHandle    The handle of the object to be disassociated.

   @return                      A standard st error type...
                                - ST_NO_ERROR
 */
ST_ErrorCode_t stptiHAL_IndexDisassociator(FullHandle_t IndexHandle, FullHandle_t AssocObjectHandle)
{
	/* Already write locked */
	ST_ErrorCode_t Error = ST_NO_ERROR;

	switch (stptiOBJMAN_GetObjectType(AssocObjectHandle)) {
	case OBJECT_TYPE_SLOT:
		break;

	case OBJECT_TYPE_BUFFER:
		{
			stptiHAL_vDevice_t *vDevice_p = stptiHAL_GetObjectvDevice_p(IndexHandle);
			stptiHAL_Index_t *Index_p = stptiHAL_GetObjectIndex_p(IndexHandle);
			stptiHAL_pDeviceXP70Write(&vDevice_p->TP_Interface.IndexerInfo_p[Index_p->IndexerIndex].output_dma_record, 0xFFFF);
		}
		break;

	default:
		/* Allow disassociation, even from invalid types, else we create a clean up problem */
		STPTI_PRINTF_ERROR("Index disassociating from invalid type %d.",
				   stptiOBJMAN_GetObjectType(AssocObjectHandle));
		break;
	}

	return (Error);
}

/**
   @brief  Deallocate this index

   This function deallocates the index

   @param  IndexHandle          The Handle of the index.

   @return                      A standard st error type...
                                - ST_NO_ERROR
 */
ST_ErrorCode_t stptiHAL_IndexDeallocator(FullHandle_t IndexHandle)
{
	/* Already write locked */
	stptiHAL_Index_t *Index_p = stptiHAL_GetObjectIndex_p(IndexHandle);
	stptiHAL_vDevice_t *vDevice_p = stptiHAL_GetObjectvDevice_p(IndexHandle);

	stptiOBJMAN_RemoveFromList(vDevice_p->IndexHandles, Index_p->IndexerIndex);

	return (ST_NO_ERROR);
}

/* Object HAL functions ------------------------------------------------------------------------- */

/**
   @brief  Enables and Disable Index Triggers

   This function sets or clears the triggers on the specified events.

   @param  IndexHandle          The handle of the index.
   @param  EventFlags           Which events trigger an index event
   @param  AdditionalFlags      Which flags trigger an index event in the Transport Packet's Adaption Field
   @param  Enable               TRUE set specified events, FALSE clear specified events

   @return                      A standard st error type...
                                - ST_NO_ERROR
 */
ST_ErrorCode_t stptiHAL_IndexTransportEvents(FullHandle_t IndexHandle, stptiHAL_EventFlags_t EventFlags,
					     stptiHAL_AdditionalTransportFlags_t AdditionalFlags, BOOL Enable)
{
	ST_ErrorCode_t Error = ST_NO_ERROR;
	stptiSupport_AccessLockState_t LocalLockState = stptiSupport_UNLOCKED;

	stptiOBJMAN_WriteLock(IndexHandle, &LocalLockState);
	{
		stptiHAL_vDevice_t *vDevice_p = stptiHAL_GetObjectvDevice_p(IndexHandle);
		stptiHAL_Index_t *Index_p = stptiHAL_GetObjectIndex_p(IndexHandle);
		stptiTP_StatusBlk_Flags_t events = 0;

		if (EventFlags & stptiHAL_INDEX_PUSI)
			events |= STATUS_BLK_PUSI_FLAG;
		if (EventFlags & stptiHAL_INDEX_SCRAMBLE_TO_CLEAR)
			events |= STATUS_BLK_SCRAMBLE_TOCLEAR;
		if (EventFlags & stptiHAL_INDEX_SCRAMBLE_TO_EVEN)
			events |= STATUS_BLK_SCRAMBLE_TOEVEN;
		if (EventFlags & stptiHAL_INDEX_SCRAMBLE_TO_ODD)
			events |= STATUS_BLK_SCRAMBLE_TOODD;
		if (EventFlags & stptiHAL_INDEX_CLEAR_TO_SCRAMBLE)
			events |= STATUS_BLK_CLEAR_TOSCRAMBLE;
		if (EventFlags & stptiHAL_INDEX_PES_PTS)
			events |= STATUS_BLK_PES_PTS;
		if (EventFlags & stptiHAL_INDEX_FIRST_RECORDED_PKT)
			events |= STATUS_BLK_FIRST_RECORD_PKT;

		if (Enable) {
			stptiHAL_pDeviceXP70BitSet(&vDevice_p->TP_Interface.IndexerInfo_p[Index_p->IndexerIndex].
						   index_on_event_mask, events);

			/* The AdditionalFlags matches the stptiTP_Indexer_AdditionalTransportIndexMask_t structure so
			   we can just use a cast. */
			stptiHAL_pDeviceXP70BitSet(&vDevice_p->TP_Interface.IndexerInfo_p[Index_p->IndexerIndex].additional_transport_index_mask,
						   (stptiTP_Indexer_AdditionalTransportIndexMask_t) AdditionalFlags);
		} else {
			stptiHAL_pDeviceXP70BitClear(&vDevice_p->TP_Interface.IndexerInfo_p[Index_p->IndexerIndex].index_on_event_mask, events);

			/* The AdditionalFlags matches the stptiTP_Indexer_AdditionalTransportIndexMask_t structure so
			   we can just use a cast. */
			stptiHAL_pDeviceXP70BitClear(&vDevice_p->TP_Interface.IndexerInfo_p[Index_p->IndexerIndex].additional_transport_index_mask,
						     (stptiTP_Indexer_AdditionalTransportIndexMask_t) AdditionalFlags);
		}

		/* Enable/Disable output of Index Event Parcels */
		if (stptiHAL_pDeviceXP70Read
		    (&vDevice_p->TP_Interface.IndexerInfo_p[Index_p->IndexerIndex].index_on_event_mask) > 0) {
			stptiHAL_pDeviceXP70BitSet(&vDevice_p->TP_Interface.IndexerInfo_p[Index_p->IndexerIndex].parcel_selection,
						   INDEXER_OUTPUT_EVENT);
		} else {
			stptiHAL_pDeviceXP70BitClear(&vDevice_p->TP_Interface.IndexerInfo_p[Index_p->IndexerIndex].parcel_selection,
						     INDEXER_OUTPUT_EVENT);
		}

		/* Enable/Disable output of Index AF Event Parcels */
		if (stptiHAL_pDeviceXP70Read
		    (&vDevice_p->TP_Interface.IndexerInfo_p[Index_p->IndexerIndex].additional_transport_index_mask) >
		    0) {
			stptiHAL_pDeviceXP70BitSet(&vDevice_p->TP_Interface.IndexerInfo_p[Index_p->IndexerIndex].parcel_selection,
						   INDEXER_OUTPUT_ADDITIONAL_TRANSPORT_EVENT);
		} else {
			stptiHAL_pDeviceXP70BitClear(&vDevice_p->TP_Interface.IndexerInfo_p[Index_p->IndexerIndex].parcel_selection,
						     INDEXER_OUTPUT_ADDITIONAL_TRANSPORT_EVENT);
		}
	}
	stptiOBJMAN_Unlock(IndexHandle, &LocalLockState);

	return (Error);
}

/**
   @brief  Sets up Start Code Indexing.

   This function allows you to specify which start codes should be indexed, and what mode to run
   the start code indexer in.

   @param  IndexHandle          The handle of the index.
   @param  StartCodeMask        An 8 U32 array indicating which of the 256 MPEG start codes to index on.
   @param  Mode                 Should be set to...
                                - stptiHAL_INDEX_STARTCODES_WITH_CONTEXT for outputing section with 8bytes of context
                                - stptiHAL_NO_STARTCODE_INDEXING for disabling start code indexing

   @return                      A standard st error type...
                                - ST_NO_ERROR
 */
ST_ErrorCode_t stptiHAL_IndexOutputStartCodes(FullHandle_t IndexHandle, U32 *StartCodeMask,
					      stptiHAL_IndexerStartCodeMode_t Mode)
{
	ST_ErrorCode_t Error = ST_NO_ERROR;
	stptiSupport_AccessLockState_t LocalLockState = stptiSupport_UNLOCKED;

	stptiOBJMAN_WriteLock(IndexHandle, &LocalLockState);
	{
		stptiHAL_vDevice_t *vDevice_p = stptiHAL_GetObjectvDevice_p(IndexHandle);
		stptiHAL_Index_t *Index_p = stptiHAL_GetObjectIndex_p(IndexHandle);

		if (StartCodeMask != NULL) {
			int i;
			for (i = 0; i < 256 / 32; i++) {
				stptiHAL_pDeviceXP70Write(&vDevice_p->TP_Interface.IndexerInfo_p[Index_p->IndexerIndex].
							  mpeg_sc_mask[i], StartCodeMask[i]);
			}
		}

		if (Mode == stptiHAL_NO_STARTCODE_INDEXING) {
			stptiHAL_pDeviceXP70BitClear(&vDevice_p->TP_Interface.IndexerInfo_p[Index_p->IndexerIndex].parcel_selection,
						     INDEXER_OUTPUT_START_CODE_EVENT);
		} else if (Mode == stptiHAL_INDEX_STARTCODES_WITH_CONTEXT) {
			stptiHAL_pDeviceXP70BitSet(&vDevice_p->TP_Interface.IndexerInfo_p[Index_p->IndexerIndex].parcel_selection,
						   INDEXER_OUTPUT_START_CODE_EVENT);
		} else {
			Error = ST_ERROR_BAD_PARAMETER;
		}
	}
	stptiOBJMAN_Unlock(IndexHandle, &LocalLockState);

	return (Error);
}

/**
   @brief  Disables all Indexing.

   This function clears an index, disabling any index triggers set.

   @param  IndexHandle          The handle of the index.

   @return                      A standard st error type...
                                - ST_NO_ERROR
 */
ST_ErrorCode_t stptiHAL_IndexReset(FullHandle_t IndexHandle)
{
	ST_ErrorCode_t Error = ST_NO_ERROR;
	stptiSupport_AccessLockState_t LocalLockState = stptiSupport_UNLOCKED;

	stptiOBJMAN_WriteLock(IndexHandle, &LocalLockState);
	{
		stptiHAL_vDevice_t *vDevice_p = stptiHAL_GetObjectvDevice_p(IndexHandle);
		stptiHAL_Index_t *Index_p = stptiHAL_GetObjectIndex_p(IndexHandle);
		int i;

		stptiHAL_pDeviceXP70Write(&vDevice_p->TP_Interface.IndexerInfo_p[Index_p->IndexerIndex].parcel_selection, 0);
		stptiHAL_pDeviceXP70Write(&vDevice_p->TP_Interface.IndexerInfo_p[Index_p->IndexerIndex].indexer_config,0);
		stptiHAL_pDeviceXP70Write(&vDevice_p->TP_Interface.IndexerInfo_p[Index_p->IndexerIndex].index_on_event_mask, 0);
		stptiHAL_pDeviceXP70Write(&vDevice_p->TP_Interface.IndexerInfo_p[Index_p->IndexerIndex].additional_transport_index_mask, 0);
		for (i = 0; i < 256 / 32; i++) {
			stptiHAL_pDeviceXP70Write(&vDevice_p->TP_Interface.IndexerInfo_p[Index_p->IndexerIndex].mpeg_sc_mask[i], 0);
		}
		stptiHAL_pDeviceXP70Write(&vDevice_p->TP_Interface.IndexerInfo_p[Index_p->IndexerIndex].index_count, 0);
	}
	stptiOBJMAN_Unlock(IndexHandle, &LocalLockState);

	return (Error);
}

/**
   @brief  Chain together indexes to form an index state machine

   This function allows the user to chain together indexes.  This allows the user to chain together
   index so that you can detect a sequence of start codes.

   The first index in the chain must be associated with the slot and a buffer.

   It is valid to call this function with NumberOfIndexes=0 (e.g. if you wanted to disable chaining)

   @param  IndexHandle          The handle of the "head" index (first in the chain).
   @param  Indexes2Chain        A pointer to an array of indexes to chain together in order from
                                the head index. The last one is chained back to the head (making the
                                chain circular).
   @param  NumberOfIndexes      Number of indexes to chain (size of array above), excluding the head index.

   @return                      A standard st error type...
                                - ST_NO_ERROR
 */
ST_ErrorCode_t stptiHAL_IndexChain(FullHandle_t IndexHandle, FullHandle_t * Indexes2Chain, int NumberOfIndexes)
{
	ST_ErrorCode_t Error = ST_NO_ERROR;
	stptiSupport_AccessLockState_t LocalLockState = stptiSupport_UNLOCKED;

	stptiOBJMAN_WriteLock(IndexHandle, &LocalLockState);
	{
		/* Note this function must cope with NumberOfIndexes equalling 0 */

		stptiHAL_vDevice_t *vDevice_p = stptiHAL_GetObjectvDevice_p(IndexHandle);
		stptiHAL_Index_t *HeadIndex_p = stptiHAL_GetObjectIndex_p(IndexHandle);
		stptiHAL_Index_t *Current_p = HeadIndex_p, *Next_p;
		FullHandle_t AssocSlotHandle;
		int i;

		/* In case the chain is active we disable the chain, by making each index refer to itself */
		stptiHAL_pDeviceXP70Write(&vDevice_p->TP_Interface.IndexerInfo_p[HeadIndex_p->IndexerIndex].next_chained_indexer,
					  HeadIndex_p->IndexerIndex);
		for (i = 0; i < NumberOfIndexes; i++) {
			Current_p = stptiHAL_GetObjectIndex_p(Indexes2Chain[i]);
			stptiHAL_pDeviceXP70Write(&vDevice_p->TP_Interface.IndexerInfo_p[Current_p->IndexerIndex].next_chained_indexer,
						  Current_p->IndexerIndex);
			if (stptiOBJMAN_CountAssociatedObjects(Indexes2Chain[i], OBJECT_TYPE_ANY) > 0) {
				Error = STPTI_ERROR_INDEX_INVALID_ASSOCIATION;
			}
		}

		if (ST_NO_ERROR == Error) {
			/* Check for an associated Slot (to reset it) */
			if (stptiOBJMAN_ReturnAssociatedObjects(IndexHandle, &AssocSlotHandle, 1, OBJECT_TYPE_SLOT) > 0) {
				/* Slot is associated to this head index, we must reset the slot_info to refer to head index */
				stptiHAL_Slot_t *Slot_p = stptiHAL_GetObjectSlot_p(AssocSlotHandle);
				stptiHAL_pDeviceXP70Write(&vDevice_p->TP_Interface.SlotInfo_p[Slot_p->SlotIndex].indexer,
							  HeadIndex_p->IndexerIndex);
			}

			/* Now chain them together */
			Current_p = HeadIndex_p;
			for (i = 0; i < NumberOfIndexes; i++) {
				Next_p = stptiHAL_GetObjectIndex_p(Indexes2Chain[i]);
				stptiHAL_pDeviceXP70Write(&vDevice_p->TP_Interface.IndexerInfo_p[Current_p->IndexerIndex].next_chained_indexer,
							  Next_p->IndexerIndex);
				Current_p = Next_p;
			}
			stptiHAL_pDeviceXP70Write(&vDevice_p->TP_Interface.IndexerInfo_p[Current_p->IndexerIndex].next_chained_indexer,
						  HeadIndex_p->IndexerIndex);
		}
	}
	stptiOBJMAN_Unlock(IndexHandle, &LocalLockState);

	return (Error);
}

/* Supporting Functions ------------------------------------------------------------------------- */

/**
   @brief  Initialise a IndexerInfo entry in the Transport Processor

   This function initialises a specified entry in the Transport Processors IndexerInfo table.

   @param  IndexerInfo_p  Pointer to the Indexer info entry to be initialised.

   @return                A standard st error type...
                          - ST_NO_ERROR
 */
void stptiHAL_IndexerTPEntryInitialise(volatile stptiTP_IndexerInfo_t * IndexerInfo_p, int IndexerIndex)
{
	stptiHAL_pDeviceXP70Memset((void *)IndexerInfo_p, 0, sizeof(stptiTP_IndexerInfo_t));

	/* Self reference for chaining */
	stptiHAL_pDeviceXP70Write(&IndexerInfo_p->next_chained_indexer, IndexerIndex);

	/* unassign dma record */
	stptiHAL_pDeviceXP70Write(&IndexerInfo_p->output_dma_record, 0xFFFF);
}
