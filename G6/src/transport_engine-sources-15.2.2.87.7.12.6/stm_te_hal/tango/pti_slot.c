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
   @file   pti_slot.c
   @brief  Slot Object Initialisation, Termination and Manipulation Functions.

   This file implements the functions for creating, destroying and accessing PTI slots.

   Slots are pid collectors, and the govern the way in which a packet is parsed and output.

   With TANGO you can now chain slots together to allow you to collect a  pid on multiple slots.
   It is important to recognise that the TP does pid matching to match the slot, and then follows
   the slot chain.  So changing the pid of a slot futher down the chain will have no effect.

   Slot Chains are always kept in order of the pid table, i.e. a slot will be inserted in the
   middle of a chain if the slot number is in between two other slots with the same pid.  This
   helps ease the complexity (i.e. the head of the chain, the lowest numbered slot, has the pid
   that matters).

   Slots have attributes (slot flags).  These usually decided at the time of allocation, but there
   are a few exceptions (see stptiHAL_SlotFeatureEnable).

   Slots also contain information that you might expect to be relevent to other things.  The big
   example here is filtering.  The filtering state and setup is contained within the slot structure,
   this makes the filter object and the slot object very coupled, and so there are a lot of helper
   functions to do the information exchange.

 */

#if 0
#define STPTI_PRINT
#endif

/* Includes ---------------------------------------------------------------- */
#include "linuxcommon.h"

/* Includes from API level */
#include "../pti_debug.h"
#include "../pti_hal_api.h"
#include "../pti_tshal_api.h"

/* Includes from the HAL / ObjMan level */
#include "pti_vdevice.h"
#include "pti_slot.h"
#include "pti_buffer.h"
#include "pti_filter.h"
#include "pti_index.h"

/* MACROS ------------------------------------------------------------------ */

/* Private Constants ------------------------------------------------------- */
/* Private Variables ------------------------------------------------------- */
/* Private Function Prototypes --------------------------------------------- */
ST_ErrorCode_t stptiHAL_SlotAllocator(FullHandle_t SlotHandle, void *params_p);
ST_ErrorCode_t stptiHAL_SlotAssociator(FullHandle_t SlotHandle, FullHandle_t AssocObjectHandle);
ST_ErrorCode_t stptiHAL_SlotDisassociator(FullHandle_t SlotHandle, FullHandle_t AssocObjectHandle);
ST_ErrorCode_t stptiHAL_SlotDeallocator(FullHandle_t SlotHandle);
ST_ErrorCode_t stptiHAL_SlotSetPID(FullHandle_t SlotHandle, U16 Pid, BOOL ResetSlot);
ST_ErrorCode_t stptiHAL_SlotGetPID(FullHandle_t SlotHandle, U16 *Pid_p);
ST_ErrorCode_t stptiHAL_SlotGetState(FullHandle_t SlotHandle, U32 *TSPacketCount_p,
				     stptiHAL_ScrambledState_t *CurrentScramblingState_p);
ST_ErrorCode_t stptiHAL_SlotEnableEvent(FullHandle_t SlotHandle, stptiHAL_EventType_t Event, BOOL Enable);
ST_ErrorCode_t stptiHAL_SlotRemapScramblingBits(FullHandle_t SlotHandle, U8 SCBitsToMatch, U8 ReplacementSCBits);
ST_ErrorCode_t stptiHAL_SlotSetCorruption(FullHandle_t SlotHandle, BOOL EnableCorruption, U8 CorruptionOffset,
					  U8 CorruptionValue);
ST_ErrorCode_t stptiHAL_SlotFeatureEnable(FullHandle_t SlotHandle, stptiHAL_SlotFeature_t Feature, BOOL Enable);
ST_ErrorCode_t stptiHAL_SlotSetSecurePathOutputNode(FullHandle_t SlotHandle, stptiHAL_SecureOutputNode_t OutputNode);
ST_ErrorCode_t stptiHAL_SlotSetSecurePathID(FullHandle_t SlotHandle, U32 PathID);
ST_ErrorCode_t stptiHAL_SlotGetMode(FullHandle_t SlotHandle, U16 *Mode_p);
ST_ErrorCode_t stptiHAL_SlotSetSecondaryPid(FullHandle_t SecondarySlotHandle, FullHandle_t PrimarySlotHandle,
					    stptiHAL_SecondaryPidMode_t Mode);
ST_ErrorCode_t stptiHAL_SlotGetSecondaryPid(FullHandle_t SlotHandle, stptiHAL_SecondaryPidMode_t *Mode);
ST_ErrorCode_t stptiHAL_SlotClearSecondaryPid(FullHandle_t SecondarySlotHandle, FullHandle_t PrimarySlotHandle);
ST_ErrorCode_t stptiHAL_SlotSetRemapPID(FullHandle_t SlotHandle, U16 Pid);

static ST_ErrorCode_t stptiHAL_SlotTPSync(FullHandle_t SlotHandle);
static void stptiHAL_SlotUnchainClearPID(stptiHAL_vDevice_t *vDevice_p, stptiHAL_Slot_t *SlotToBeUnchained_p);
static ST_ErrorCode_t stptiHAL_SlotChainSetPID(stptiHAL_vDevice_t *vDevice_p, stptiHAL_Slot_t *SlotToBeChained_p,
					       U16 NewPid);
static ST_ErrorCode_t stptiHAL_SlotSetPIDWorkerFn(FullHandle_t SlotHandle, U16 Pid, BOOL SuppressReset);

/* Public Constants -------------------------------------------------------- */

/* Export the API */
const stptiHAL_SlotAPI_t stptiHAL_SlotAPI = {
	{
		/* Allocator            Associator               Disassociator               Deallocator */
		stptiHAL_SlotAllocator, stptiHAL_SlotAssociator, stptiHAL_SlotDisassociator, stptiHAL_SlotDeallocator,
		NULL, NULL
	},
	stptiHAL_SlotSetPID,
	stptiHAL_SlotGetPID,
	stptiHAL_SlotGetState,
	stptiHAL_SlotEnableEvent,
	stptiHAL_SlotRemapScramblingBits,
	stptiHAL_SlotSetCorruption,
	stptiHAL_SlotFeatureEnable,
	stptiHAL_SlotSetSecurePathOutputNode,
	stptiHAL_SlotSetSecurePathID,
	stptiHAL_SlotGetMode,
	stptiHAL_SlotSetSecondaryPid,
	stptiHAL_SlotGetSecondaryPid,
	stptiHAL_SlotClearSecondaryPid,
	stptiHAL_SlotSetRemapPID
};

/* Object Management Functions ------------------------------------------------------------------ */

/**
   @brief  Allocate this slot

   This function allocates the slot, reserving an entry in the SlotInfo and PID tables on the TP.

   @param  SlotHandle           The handle of the slot.
   @param  params_p             The parameters for this slot.

   @return                      A standard st error type...
                                - ST_NO_ERROR
 */
ST_ErrorCode_t stptiHAL_SlotAllocator(FullHandle_t SlotHandle, void *params_p)
{
	/* Already write locked */
	stptiHAL_Slot_t *Slot_p = stptiHAL_GetObjectSlot_p(SlotHandle);
	stptiHAL_pDevice_t *pDevice_p = stptiHAL_GetObjectpDevice_p(SlotHandle);
	stptiHAL_vDevice_t *vDevice_p = stptiHAL_GetObjectvDevice_p(SlotHandle);
	stptiHAL_SlotConfigParams_t *Parameters_p = (stptiHAL_SlotConfigParams_t *) params_p;

	ST_ErrorCode_t Error = ST_NO_ERROR;

	/* DataEntry facilities */
	if (Parameters_p->DataEntryReplacement && Parameters_p->DataEntryInsertion) {
		Error = ST_ERROR_BAD_PARAMETER;	/* It can be either or neither, but not both */
	} else {
		/* Assign a slot index for this slot */
		if (ST_NO_ERROR == stptiOBJMAN_AddToList(vDevice_p->SlotHandles, Slot_p, FALSE, &Slot_p->SlotIndex)) {
			if (Slot_p->SlotIndex >= (int)pDevice_p->TP_Interface.NumberOfSlots) {
				/* Too many slots for the physical Device */
				stptiOBJMAN_RemoveFromList(vDevice_p->SlotHandles, Slot_p->SlotIndex);
				Error = ST_ERROR_NO_MEMORY;
			}
		}
	}

	if (ST_NO_ERROR == Error) {
		vDevice_p->SlotsAllocated++;

		Slot_p->SlotMode = Parameters_p->SlotMode;
		Slot_p->PID = stptiHAL_InvalidPID;
		Slot_p->PidIndex = -1;
		Slot_p->PreviousSlotInChain = stptiOBJMAN_NullHandle;
		Slot_p->NextSlotInChain = stptiOBJMAN_NullHandle;
		Slot_p->DataEntryHandle = stptiOBJMAN_NullHandle;
		Slot_p->SecurityPathID = 0;
		Slot_p->SecondarySlotType = stptiHAL_SECONDARY_TYPE_NONE;

		stptiHAL_SlotTPEntryInitialise(&vDevice_p->TP_Interface.SlotInfo_p[Slot_p->SlotIndex]);
		stpti_printf("Allocating Slot at Index 0x%02x", Slot_p->SlotIndex);

		if (stptiHAL_SLOT_TYPE_PARTIAL_PES == Parameters_p->SlotMode) {
			stptiHAL_pDeviceXP70Write(&vDevice_p->TP_Interface.SlotInfo_p[Slot_p->SlotIndex].slot_mode,
						  SLOTTYPE_RAW);
			/* but note that Slot_p->SlotMode remains stptiHAL_SLOT_TYPE_PARTIAL_PES */
		} else {
			stptiHAL_pDeviceXP70Write(&vDevice_p->TP_Interface.SlotInfo_p[Slot_p->SlotIndex].slot_mode,
						  (stptiTP_SlotMode_t) Slot_p->SlotMode);
		}

		/* SuppressMetaData stops output of metadata that is output alongside PES or Sections */
		if (Parameters_p->SuppressMetaData) {
			stptiHAL_pDeviceXP70BitSet(&vDevice_p->TP_Interface.SlotInfo_p[Slot_p->SlotIndex].slot_flags,
						   SLOT_FLAGS_SUPPRESS_METADATA);
			Slot_p->SuppressMetaData = TRUE;
		} else {
			Slot_p->SuppressMetaData = FALSE;
		}

		/* SoftwareCDFifo (implies SuppressMetaData) forces update of writeq at the end of every packet (important for non-deterministic
		   PES where a PES packet might be larger than your buffer). */
		if (Parameters_p->SoftwareCDFifo) {
			stptiHAL_pDeviceXP70BitSet(&vDevice_p->TP_Interface.SlotInfo_p[Slot_p->SlotIndex].slot_flags,
						   SLOT_FLAGS_NO_WINDBACK_ON_ERROR | SLOT_FLAGS_SW_CD_FIFO);
			Slot_p->SoftwareCDFifo = TRUE;

			/* Software CD Fifo implys no Metadata, so force this to be true */
			stptiHAL_pDeviceXP70BitSet(&vDevice_p->TP_Interface.SlotInfo_p[Slot_p->SlotIndex].slot_flags,
						   SLOT_FLAGS_SUPPRESS_METADATA);
			Slot_p->SuppressMetaData = TRUE;
		} else {
			Slot_p->SoftwareCDFifo = FALSE;
		}

		switch (Slot_p->SlotMode) {
		case stptiHAL_SLOT_TYPE_ECM:	/* purposefully drop into Slot Type Section */
		case stptiHAL_SLOT_TYPE_EMM:	/* purposefully drop into Slot Type Section */
		case stptiHAL_SLOT_TYPE_SECTION:
			stptiHAL_pDeviceXP70Write(&vDevice_p->TP_Interface.SlotInfo_p[Slot_p->SlotIndex].u.section_info.
						  section_params, SF_MODE_NONE);
			break;

		case stptiHAL_SLOT_TYPE_PES:
			stptiHAL_pDeviceXP70Write(&vDevice_p->TP_Interface.SlotInfo_p[Slot_p->SlotIndex].u.pes_fsm.
						  pes_state, PES_STATE_IDLE);
			stptiHAL_pDeviceXP70Write(&vDevice_p->TP_Interface.SlotInfo_p[Slot_p->SlotIndex].u.pes_fsm.
						  pes_streamid_filterdata, 0);
			stptiHAL_pDeviceXP70Write(&vDevice_p->TP_Interface.SlotInfo_p[Slot_p->SlotIndex].u.pes_fsm.
						  pes_marker_index, 0xFF);
			break;

		case stptiHAL_SLOT_TYPE_PARTIAL_PES:
		case stptiHAL_SLOT_TYPE_RAW:
			/* Set the scramble control bit mapping to a "don't change them" state */
			stptiHAL_pDeviceXP70Write(&vDevice_p->TP_Interface.SlotInfo_p[Slot_p->SlotIndex].u.raw_slot_info.SCRemappingBits, 0xE4);	/* 11.10.01.00 in binary = 0xE4 */

			/* Set default remap to invalid pid */
			stptiHAL_pDeviceXP70Write(&vDevice_p->TP_Interface.SlotInfo_p[Slot_p->SlotIndex].u.
						  raw_slot_info.RemapPid, 0xE000);

			Slot_p->DataEntryReplacement = Parameters_p->DataEntryReplacement;
			Slot_p->DataEntryInsertion = Parameters_p->DataEntryInsertion;

			stptiHAL_pDeviceXP70Write(&vDevice_p->TP_Interface.SlotInfo_p[Slot_p->SlotIndex].u.
						  raw_slot_info.DataEntry.EntryState, DATA_ENTRY_STATE_EMPTY);
			stptiHAL_pDeviceXP70BitSet(&vDevice_p->TP_Interface.SlotInfo_p[Slot_p->SlotIndex].slot_flags,
						   ((Parameters_p->
						     DataEntryReplacement) ? SLOT_FLAGS_ENTRY_REPLACEMENT : 0));
			stptiHAL_pDeviceXP70BitSet(&vDevice_p->TP_Interface.SlotInfo_p[Slot_p->SlotIndex].slot_flags,
						   ((Parameters_p->
						     DataEntryInsertion) ? SLOT_FLAGS_ENTRY_INSERTION : 0));

			stptiHAL_pDeviceXP70Write(&vDevice_p->TP_Interface.SlotInfo_p[Slot_p->SlotIndex].u.
						  raw_slot_info.DataEntry.EntryAddress, 0);
			stptiHAL_pDeviceXP70Write(&vDevice_p->TP_Interface.SlotInfo_p[Slot_p->SlotIndex].u.
						  raw_slot_info.DataEntry.EntrySize, 0);
			stptiHAL_pDeviceXP70Write(&vDevice_p->TP_Interface.SlotInfo_p[Slot_p->SlotIndex].u.
						  raw_slot_info.CCFixup, 0);

			break;

		case stptiHAL_SLOT_TYPE_PCR:
		default:
			break;
		}
	}

	if (ST_NO_ERROR == Error) {
		/* Activate the slot (by setting the mode) - however a pid will still need to be set */
		stptiHAL_SlotActivate(SlotHandle);
	}

	return (Error);
}

/**
   @brief  Assocate this slot

   This function assocate the slot, from either another slot or a buffer.  When associating slots
   to each other, they become a group which follow each other when their settings are changed
   (such as SlotSetPID).

   @param  SlotHandle           The handle of the slot.
   @param  AssocObjectHandle    The handle of the object to be associated.

   @return                      A standard st error type...
                                - ST_NO_ERROR
 */
ST_ErrorCode_t stptiHAL_SlotAssociator(FullHandle_t SlotHandle, FullHandle_t AssocObjectHandle)
{
	/* Already write locked */
	ST_ErrorCode_t Error = ST_NO_ERROR;

	switch (stptiOBJMAN_GetObjectType(AssocObjectHandle)) {
	case OBJECT_TYPE_SLOT:
		{
			stptiHAL_Slot_t *Slot_p = stptiHAL_GetObjectSlot_p(SlotHandle);
			stptiHAL_Slot_t *AssocSlot_p = stptiHAL_GetObjectSlot_p(AssocObjectHandle);
			if (Slot_p->PID != stptiHAL_InvalidPID ||
				AssocSlot_p->PID != stptiHAL_InvalidPID) {
				Error = STPTI_ERROR_NOT_ALLOWED_WHILST_PID_SET;
			}
			if (Slot_p->PID != 0 ||
				AssocSlot_p->PID != 0) {
				Error = STPTI_ERROR_DIFFERING_SECURITY_PATH_IDS;
			}
		}
		break;

	case OBJECT_TYPE_DATA_ENTRY:
		{
			stptiHAL_Slot_t *Slot_p = stptiHAL_GetObjectSlot_p(SlotHandle);

			if (Slot_p->SecondarySlotType != stptiHAL_SECONDARY_TYPE_NONE) {
				Error = STPTI_ERROR_SLOT_SECONDARY_PID_IN_USE;
			}
		}
		break;

	case OBJECT_TYPE_INDEX:
		if (stptiOBJMAN_CountAssociatedObjects(SlotHandle, OBJECT_TYPE_INDEX) > 1) {
			Error = STPTI_ERROR_INDEX_SLOT_ALREADY_ASSOCIATED;
		} else {
			stptiHAL_vDevice_t *vDevice_p = stptiHAL_GetObjectvDevice_p(SlotHandle);
			stptiHAL_Slot_t *Slot_p = stptiHAL_GetObjectSlot_p(SlotHandle);
			stptiHAL_Index_t *Index_p = stptiHAL_GetObjectIndex_p(AssocObjectHandle);
			stptiHAL_pDeviceXP70Write(&vDevice_p->TP_Interface.SlotInfo_p[Slot_p->SlotIndex].indexer,
						  Index_p->IndexerIndex);
		}
		break;

	case OBJECT_TYPE_BUFFER:
		if (stptiOBJMAN_CountAssociatedObjects(SlotHandle, OBJECT_TYPE_BUFFER) > 1) {
			Error = STPTI_ERROR_SLOT_ALREADY_LINKED;
		} else {
			stptiHAL_vDevice_t *vDevice_p = stptiHAL_GetObjectvDevice_p(SlotHandle);
			stptiHAL_Slot_t *Slot_p = stptiHAL_GetObjectSlot_p(SlotHandle);
			stptiHAL_Buffer_t *Buffer_p = stptiHAL_GetObjectBuffer_p(AssocObjectHandle);

			stptiHAL_pDeviceXP70Write(&vDevice_p->TP_Interface.SlotInfo_p[Slot_p->SlotIndex].dma_record,
						  Buffer_p->BufferIndex);

			if (stptiHAL_SLOT_TYPE_PES == Slot_p->SlotMode) {
				stptiHAL_pDeviceXP70Write(&vDevice_p->TP_Interface.SlotInfo_p[Slot_p->SlotIndex].u.
							  pes_fsm.pes_state, PES_STATE_IDLE);
			}
		}
		break;

	case OBJECT_TYPE_FILTER:
		{
			stptiHAL_Slot_t *Slot_p = stptiHAL_GetObjectSlot_p(SlotHandle);
			stptiHAL_Filter_t *Filter_p = stptiHAL_GetObjectFilter_p(AssocObjectHandle);

			switch (Slot_p->SlotMode) {
			case stptiHAL_SLOT_TYPE_PES:
				if (stptiHAL_PES_STREAMID_FILTER == Filter_p->FilterType) {
					if (stptiOBJMAN_CountAssociatedObjects(SlotHandle, OBJECT_TYPE_FILTER) > 1) {
						/* Only one PES filter allowed on a SLOT */
						Error = STPTI_ERROR_INVALID_FILTER_OPERATING_MODE;
					} else {
						stptiHAL_SlotUpdateFilterState(SlotHandle, AssocObjectHandle,
									       !Filter_p->EnableOnAssociation,
									       Filter_p->EnableOnAssociation);
					}
				} else {
					Error = STPTI_ERROR_INVALID_SLOT_TYPE;
				}
				break;

			case stptiHAL_SLOT_TYPE_ECM:
			case stptiHAL_SLOT_TYPE_EMM:
			case stptiHAL_SLOT_TYPE_SECTION:
				if ((stptiHAL_TINY_FILTER == Filter_p->FilterType)
				    && stptiOBJMAN_CountAssociatedObjects(SlotHandle, OBJECT_TYPE_FILTER) > 1) {
					/* Only one TINY filter allowed on a SLOT */
					Error = STPTI_ERROR_INVALID_FILTER_OPERATING_MODE;
				} else if (stptiHAL_PROPRIETARY_FILTER == Filter_p->FilterType) {
					stptiHAL_vDevice_t *vDevice_p = stptiHAL_GetObjectvDevice_p(SlotHandle);

					if (vDevice_p->ProprietaryFilterAllocated) {
						/* Only one Proprietary filter allowed per vDevice */
						Error = STPTI_ERROR_INVALID_FILTER_OPERATING_MODE;
					} else {
						vDevice_p->ProprietaryFilterAllocated = TRUE;
						Error =
						    stptiHAL_SlotUpdateFilterState(SlotHandle, AssocObjectHandle,
										   !Filter_p->EnableOnAssociation,
										   Filter_p->EnableOnAssociation);
					}
				} else if ((stptiHAL_TINY_FILTER == Filter_p->FilterType)
					   && stptiOBJMAN_CountAssociatedObjects(SlotHandle, OBJECT_TYPE_FILTER) == 1) {
					Error =
					    stptiHAL_SlotUpdateFilterState(SlotHandle, AssocObjectHandle,
									   !Filter_p->EnableOnAssociation,
									   Filter_p->EnableOnAssociation);
				} else {
					/* Need to check that associated filters are compatible as only certain
					   types of filters can be mixed on one slot.  Note that the it will also
					   unnecessarily check it is compatible with itself. */
					int index;
					Object_t *AssociatedObject_p;
					stptiTP_SectionParams_t SectionParams =
					    stptiHAL_FilterConvertToTPType(Filter_p->FilterType);
					stptiTP_SectionParams_t mixtest =
					    (SectionParams & (SF_MODE_LONG | SF_MODE_PNMM)) ? SF_MODE_LONG |
					    SF_MODE_PNMM : 0;

					/* The restriction under tango is that we cannot mix h/w section filter
					   types (LONG, SHORT, PNMM).  SectionParams is the h/w section filter
					   type or SF_MODE_NONE if it is not relevent. */

					/* Changes to the firmware now allow the mixing of
					   LONG and PNM modes only. SHORT mode not supported */

					stptiOBJMAN_FirstInList(&Slot_p->ObjectHeader.AssociatedObjects,
								(void *)&AssociatedObject_p, &index);
					while (index >= 0) {
						if (AssociatedObject_p->Handle.member.ObjectType == OBJECT_TYPE_FILTER) {
							stptiHAL_Filter_t *AssocFilter_p =
							    (stptiHAL_Filter_t *) AssociatedObject_p;
							stptiTP_SectionParams_t TestSectionParams =
							    stptiHAL_FilterConvertToTPType(AssocFilter_p->FilterType);

							/* New filter   Exisiting associated  mixtest & TestSectionParams
							   mixtest      TestSectionParams
							   TINY=0       TINY=4                0=ERROR ** Should not execute this part of the code
							   TINY=0       PROP=6                0=ERROR ** Should not execute this part of the code
							   TINY=0       LONG=1                0=ERROR ** Should not execute this part of the code
							   TINY=0       PNMM=2                0=ERROR ** Should not execute this part of the code
							   LONG=3       TINY=4                0=ERROR
							   LONG=3       PROP=8                0=ERROR
							   LONG=3       LONG=1                1=OK
							   LONG=3       PNMM=2                2=OK
							   PNMM=3       TINY=4                0=ERROR
							   PNMM=3       PROP=8                0=ERROR
							   PNMM=3       LONG=1                1=OK
							   PNMM=3       PNMM=2                2=OK */
							if (!(mixtest & TestSectionParams)) {
								Error = STPTI_ERROR_INVALID_FILTER_TYPE;
								break;
							}
						}
						stptiOBJMAN_NextInList(&Slot_p->ObjectHeader.AssociatedObjects,
								       (void *)&AssociatedObject_p, &index);
					}
					if (ST_NO_ERROR == Error) {
						Error =
						    stptiHAL_SlotUpdateFilterState(SlotHandle, AssocObjectHandle,
										   !Filter_p->EnableOnAssociation,
										   Filter_p->EnableOnAssociation);
					}
				}
				break;

			default:
				/* Can't associate a filter to this slot type */
				Error = STPTI_ERROR_INVALID_SLOT_TYPE;
				break;
			}
		}
		break;

	default:
		STPTI_PRINTF_ERROR("Slot cannot associate to this kind of object %d",
				   stptiOBJMAN_GetObjectType(AssocObjectHandle));
		Error = ST_ERROR_BAD_PARAMETER;
		break;
	}

	return (Error);
}

/**
   @brief  Disassociate this slot from another object

   This function disassociates the slot, from either another slot or a buffer.

   @param  SlotHandle           The handle of the slot.
   @param  AssocObjectHandle    The handle of the object to be disassociated.

   @return                      A standard st error type...
                                - ST_NO_ERROR
 */
ST_ErrorCode_t stptiHAL_SlotDisassociator(FullHandle_t SlotHandle, FullHandle_t AssocObjectHandle)
{
	/* Already write locked */
	ST_ErrorCode_t Error = ST_NO_ERROR;

	stptiHAL_Slot_t *Slot_p = stptiHAL_GetObjectSlot_p(SlotHandle);
	switch (stptiOBJMAN_GetObjectType(AssocObjectHandle)) {
	case OBJECT_TYPE_SLOT:
	case OBJECT_TYPE_DATA_ENTRY:
		/* Nothing to do, we just want to allow the object manager to draw up a list */
		break;

	case OBJECT_TYPE_INDEX:
		{
			stptiHAL_vDevice_t *vDevice_p = stptiHAL_GetObjectvDevice_p(SlotHandle);
			stptiHAL_pDeviceXP70Write(&vDevice_p->TP_Interface.SlotInfo_p[Slot_p->SlotIndex].indexer,
						  0xFFFF);
			if (Slot_p->PID != stptiHAL_InvalidPID) {
				stptiHAL_SlotTPSync(SlotHandle);	/* Wait for Slot not to be in use (purposefully ignore error) */
			}
		}
		break;

	case OBJECT_TYPE_BUFFER:
		{
			stptiHAL_vDevice_t *vDevice_p = stptiHAL_GetObjectvDevice_p(SlotHandle);
			stptiHAL_pDeviceXP70Write(&vDevice_p->TP_Interface.SlotInfo_p[Slot_p->SlotIndex].dma_record,
						  0xFFFF);
		}
		break;

	case OBJECT_TYPE_FILTER:
		{
			stptiHAL_Filter_t *Filter_p = stptiHAL_GetObjectFilter_p(AssocObjectHandle);
			stptiHAL_vDevice_t *vDevice_p = stptiHAL_GetObjectvDevice_p(SlotHandle);

			stptiHAL_SlotUpdateFilterState(SlotHandle, AssocObjectHandle, TRUE, FALSE);

			if (Slot_p->PID != stptiHAL_InvalidPID) {
				stptiHAL_SlotTPSync(SlotHandle);	/* Wait for Slot not to be in use (purposefully ignore error) */
			}

			switch (Slot_p->SlotMode) {
			case stptiHAL_SLOT_TYPE_ECM:
			case stptiHAL_SLOT_TYPE_EMM:
			case stptiHAL_SLOT_TYPE_SECTION:
				if (stptiOBJMAN_CountAssociatedObjects(SlotHandle, OBJECT_TYPE_FILTER) == 1) {
					/* Last filter being disassociated */
					stptiHAL_pDeviceXP70Write(&vDevice_p->TP_Interface.SlotInfo_p[Slot_p->SlotIndex].u.section_info.section_params,
								  SF_MODE_NONE);
				}

				if (stptiHAL_PROPRIETARY_FILTER == Filter_p->FilterType)
					vDevice_p->ProprietaryFilterAllocated = FALSE;

				break;

			default:	/* Don't need to do anything for slot base filters such as TINY */
				break;
			}
		}
		break;

	default:
		/* Allow disassociation, even from invalid types, else we create a clean up problem */
		STPTI_PRINTF_ERROR("Slot disassociating from invalid type %d.",
				   stptiOBJMAN_GetObjectType(AssocObjectHandle));
		break;
	}

	return (Error);
}

/**
   @brief  Deallocate this slot

   This function deallocates the slot, and unchains it from any other slots.

   @param  SlotHandle           The Handle of the slot.

   @return                      A standard st error type...
                                - ST_NO_ERROR
 */
ST_ErrorCode_t stptiHAL_SlotDeallocator(FullHandle_t SlotHandle)
{
	ST_ErrorCode_t Error = ST_NO_ERROR;
	/* Already write locked */
	stptiHAL_Slot_t *Slot_p = stptiHAL_GetObjectSlot_p(SlotHandle);
	stptiHAL_vDevice_t *vDevice_p = stptiHAL_GetObjectvDevice_p(SlotHandle);

	/* If we are deallocating a slot we must check to see if its a secondary pid slot - if it is we must also clear the secondary_pid_info structure on the linked slot */
	if (Slot_p != NULL) {
		U16 OriginalPID = Slot_p->PID;

		if (stptiHAL_SECONDARY_TYPE_NONE != Slot_p->SecondarySlotType) {
			/* Despite the name SecondaryHandle - This could be a primary or secondary slot */
			if (!stptiOBJMAN_IsNullHandle(Slot_p->SecondaryHandle)) {
				FullHandle_t SecondaryHandle = Slot_p->SecondaryHandle;
				stptiHAL_Slot_t *SecondarySlot_p = stptiHAL_GetObjectSlot_p(SecondaryHandle);

				/* Clear the secondary pid linked slot, the slot passed in will be cleared below in stptiHAL_SlotTPEntryInitialise */
				stptiHAL_pDeviceXP70Write(&vDevice_p->TP_Interface.SlotInfo_p[SecondarySlot_p->SlotIndex].secondary_pid_info, 0);

				/* Also need to find the object pointer to clear the linked secondary id flag */
				SecondarySlot_p->SecondarySlotType = stptiHAL_SECONDARY_TYPE_NONE;
				stptiHAL_SlotResetState(&SecondaryHandle, 1, FALSE);
			}
			/* Clear the current slot ASAP even though it will be cleared below in stptiHAL_SlotTPEntryInitialise */
			stptiHAL_pDeviceXP70Write(&vDevice_p->TP_Interface.SlotInfo_p[Slot_p->SlotIndex].secondary_pid_info, 0);
		}

		/* Unchain this slot if it is */
		stptiHAL_SlotUnchainClearPID(vDevice_p, Slot_p);

		if (stptiHAL_InvalidPID != OriginalPID) {
			if (stptiHAL_StreamIDNone != vDevice_p->StreamID) {
				(void)stptiTSHAL_call(TSInput.TSHAL_TSInputSetClearPid, 0,
						      stptiOBJMAN_GetpDeviceIndex(SlotHandle), vDevice_p->StreamID,
						      OriginalPID, FALSE);
				/* Ignore error here, as we can error if the TSInput has be "unconfigured" */
			}

			stptiHAL_SlotResetState(&SlotHandle, 1, TRUE);	/* This delays to make sure it is no longer in use */
		}

		/* Put entry back to an initialised state (strictly unnecessary but helps debugging) */
		stptiHAL_SlotTPEntryInitialise(&vDevice_p->TP_Interface.SlotInfo_p[Slot_p->SlotIndex]);

		stptiOBJMAN_RemoveFromList(vDevice_p->SlotHandles, Slot_p->SlotIndex);
		vDevice_p->SlotsAllocated--;
	}

	return (Error);
}

/* Object HAL functions ------------------------------------------------------------------------- */

/**
   @brief  Get the Slot mode for this slot

   This function gets the slotmode set for this slot.

   @param  SlotHandle           The Handle of the slot.
   @param  Mode_p               Pointer to the returned mode

   @return                      A standard st error type...
                                - ST_NO_ERROR
 */
ST_ErrorCode_t stptiHAL_SlotGetMode(FullHandle_t SlotHandle, U16 * Mode_p)
{
	ST_ErrorCode_t Error = ST_NO_ERROR;
	stptiSupport_AccessLockState_t LocalLockState = stptiSupport_UNLOCKED;

	stptiOBJMAN_ReadLock(SlotHandle, &LocalLockState);
	{
		stptiHAL_Slot_t *Slot_p = stptiHAL_GetObjectSlot_p(SlotHandle);
		if (NULL != Slot_p) {
			*Mode_p = Slot_p->SlotMode;
		} else {
			Error = ST_ERROR_BAD_PARAMETER;
		}
	}
	stptiOBJMAN_Unlock(SlotHandle, &LocalLockState);

	return (Error);
}

/**
   @brief  Set the PID for this slot

   This function set the PID set for this slot.

   @param  SlotHandle           The Handle of the slot.
   @param  Pid                  The PID to be set (or stptiHAL_InvalidPID if clearing it)
   @param  SuppressReset        TRUE if you which to prevent reset of the slot state and any
                                associated buffer's pointers.

   @return                      A standard st error type...
                                - ST_NO_ERROR
 */
ST_ErrorCode_t stptiHAL_SlotSetPID(FullHandle_t SlotHandle, U16 Pid, BOOL SuppressReset)
{
	ST_ErrorCode_t Error = ST_NO_ERROR;
	stptiSupport_AccessLockState_t LocalLockState = stptiSupport_UNLOCKED;

	/* Slot set pid is more complex, than you might expect!!!!!

	   There are TWO independent concepts here...

	   1/ Slots with the same pid are "chained together".  They need not be associated.  This
	   means the user can have two (or more) slots collecting the same pid, and effectively
	   "duplicate" the data.

	   2/ If one or more slots are ASSOCIATED then all associated slots are changed to the
	   new pid as well.  This makes it easier to group slots together, and is particularly
	   useful for "hidden" slots which the user might not know about (used for record buffer
	   in the STPTI API).

	   AS SETTING A PID IS NO LONGER A CONCEPT FOR A SINGLE SLOT IT IS REALLY IMPORTANT TO CHECK
	   THAT WHAT IS GOING TO BE DONE IS ACHIEVABLE BEFORE STARTING TO DO THE CHANGES (i.e.
	   preValidate the changes before committing to do them).

	 */

	/* Additional rules for Secondary pid feature.
	   - Only one Secondary Pid Primary slot is allowed on a slot chain.
	   - No slot chaining allowed on a Secondary Pid Secondary slot.
	 */
	stptiOBJMAN_WriteLock(SlotHandle, &LocalLockState);
	{
		stptiHAL_Slot_t *Slot_p = stptiHAL_GetObjectSlot_p(SlotHandle);

		FullHandle_t AssocdSlotHandles[MAX_CHAINED_SLOTS + 1];
		int CountOfAssocdSlotHandles = 0;
		int index;
		BOOL SecondarySlotDetected = FALSE;
		BOOL PrimarySlotDetected = FALSE;
		BOOL UpdatePathID = FALSE;

		CountOfAssocdSlotHandles =
		    stptiOBJMAN_ReturnAssociatedObjects(SlotHandle, AssocdSlotHandles,
							sizeof(AssocdSlotHandles) / sizeof(AssocdSlotHandles[0]),
							OBJECT_TYPE_SLOT);

		/* Not allowed to set the PID on an DataEntry Insertion Slot */
		if (Slot_p->DataEntryInsertion) {
			Error = STPTI_ERROR_INVALID_SLOT_HANDLE;
		}

		if (ST_NO_ERROR == Error) {

			if (Pid != stptiHAL_InvalidPID) {
				int CountOfSlotsWithRequestedPID, CountOfAssocdWithSamePID =
				    0, CountOfSlotsAfterPidChange;

				/* Perform a check to see that we can actually do this request for changing the pid.  It is
				   important to do this in advance as it is difficult to "unwind" the change once we have
				   started setting pids. */

				if (Pid > stptiHAL_WildCardPID) {	/* 0x0000-0x1fff are normal PID definitions, 0x2000 is the wildcard pid */
					Error = ST_ERROR_BAD_PARAMETER;
				} else {
					FullHandle_t SlotHandlesWithRequestedPID[MAX_CHAINED_SLOTS + 1];

					/* Lookup any slots already set to the new pid */
					CountOfSlotsWithRequestedPID =
					    stptiHAL_SlotLookupPID(SlotHandle, Pid, SlotHandlesWithRequestedPID,
								   sizeof(SlotHandlesWithRequestedPID) /
								   sizeof(SlotHandlesWithRequestedPID[0]));

					for (index = 0; index < CountOfSlotsWithRequestedPID; index++) {
						stptiHAL_Slot_t *AssocdSlot_p =
						    stptiHAL_GetObjectSlot_p(SlotHandlesWithRequestedPID[index]);

						/* Can't have a different pathid on the same chain as there is only one packet buffer in the TP */
						if (AssocdSlot_p->SecurityPathID != Slot_p->SecurityPathID) {
							Slot_p->SecurityPathID = AssocdSlot_p->SecurityPathID;
							UpdatePathID = TRUE;
						}
						if (AssocdSlot_p->SecondarySlotType ==
							stptiHAL_SECONDARY_TYPE_PRIM) {
							stpti_printf("Primary Detected %d", __LINE__);
							PrimarySlotDetected = TRUE;
						} else if (AssocdSlot_p->SecondarySlotType ==
							   stptiHAL_SECONDARY_TYPE_SEC) {
							/* If any slot handle is a Secondary Pid secondary slot, abort */
							stpti_printf("SecondarySlotDetected %d", __LINE__);
							SecondarySlotDetected = TRUE;
							break;
						}
					}

					if (ST_NO_ERROR == Error) {
						/* Now check all the associated slots handles to see which have the pid already set */
						for (index = 0; index < CountOfAssocdSlotHandles; index++) {
							stptiHAL_Slot_t *AssocdSlot_p =
							    stptiHAL_GetObjectSlot_p(AssocdSlotHandles[index]);
							if (AssocdSlot_p->PID == Pid) {
								CountOfAssocdWithSamePID++;
							}
							if (AssocdSlot_p->SecondarySlotType ==
								stptiHAL_SECONDARY_TYPE_PRIM) {
								stpti_printf("PrimarySlotDetected %d", __LINE__);
								PrimarySlotDetected = TRUE;
							}
						}

						if (ST_NO_ERROR == Error) {
							/* Total CountOfSlotsAfterPidChange = assocd slots that will change pid + all the slots with the same pid */
							CountOfSlotsAfterPidChange =
							    CountOfAssocdSlotHandles - CountOfAssocdWithSamePID +
							    CountOfSlotsWithRequestedPID;

							/* Most importantly stop a Secondary slot being added to a chain */
							if (Slot_p->SecondarySlotType == stptiHAL_SECONDARY_TYPE_SEC
							    && CountOfSlotsAfterPidChange) {
								Error = STPTI_ERROR_SLOT_SECONDARY_PID_IN_USE;
							} else if (Slot_p->SecondarySlotType ==
								   stptiHAL_SECONDARY_TYPE_NONE
								   && SecondarySlotDetected) {
								STPTI_PRINTF_ERROR
								    ("STPTI_ERROR_SLOT_SECONDARY_PID_IN_USE %d",
								     __LINE__);
								/* Secondary slot found on the new pid */
								Error = STPTI_ERROR_SLOT_SECONDARY_PID_IN_USE;
							} else if (Slot_p->SecondarySlotType ==
								   stptiHAL_SECONDARY_TYPE_PRIM
								   && PrimarySlotDetected) {
								STPTI_PRINTF_ERROR
								    ("STPTI_ERROR_SLOT_SECONDARY_PID_IN_USE %d",
								     __LINE__);
								/* Primary slot found on the new chain */
								Error = STPTI_ERROR_SLOT_SECONDARY_PID_IN_USE;
							} else if (ST_NO_ERROR == Error) {
								/* Finally add the actual slot we are changing */
								if (Slot_p->PID != Pid) {
									CountOfSlotsAfterPidChange++;
								}
								if (CountOfSlotsAfterPidChange > MAX_CHAINED_SLOTS) {
									Error =
									    STPTI_ERROR_TOO_MANY_SLOTS_WITH_SAME_PID;
								}
							}
						}
					}
				}
			}
		}

		if (ST_NO_ERROR == Error) {
			/* We shouldn't get errors here as the validation stage above should have caught these */
			stptiHAL_vDevice_t *vDevice_p = stptiHAL_GetObjectvDevice_p(SlotHandle);
			volatile stptiTP_SlotInfo_t *SlotInfo_p;

			/* Set PID for this slot */
			Error = stptiHAL_SlotSetPIDWorkerFn(SlotHandle, Pid, SuppressReset);
			if (ST_NO_ERROR == Error && UpdatePathID) {
				SlotInfo_p =
					&vDevice_p->TP_Interface.SlotInfo_p[Slot_p->SlotIndex];

				/* On TANGO the key index (secure path id) is only 16bits (discard upper 16bits) */
				stptiHAL_pDeviceXP70Write(&SlotInfo_p->key_index, (U16) Slot_p->SecurityPathID);

			}
			if (ST_NO_ERROR == Error) {
				/* Now set PID for all associated slots */
				for (index = 0; index < CountOfAssocdSlotHandles; index++) {
					stptiHAL_Slot_t *AssocSlot_p = stptiHAL_GetObjectSlot_p(AssocdSlotHandles[index]);
					SlotInfo_p =
						&vDevice_p->TP_Interface.SlotInfo_p[AssocSlot_p->SlotIndex];
					Error =
					    stptiHAL_SlotSetPIDWorkerFn(AssocdSlotHandles[index], Pid, SuppressReset);
					if (ST_NO_ERROR == Error && UpdatePathID) {
						/* On TANGO the key index (secure path id) is only 16bits (discard upper 16bits) */
						stptiHAL_pDeviceXP70Write(&SlotInfo_p->key_index, (U16) Slot_p->SecurityPathID);
					}
					if (ST_NO_ERROR == Error) {
						break;
					}
				}
			}
		}
	}
	stptiOBJMAN_Unlock(SlotHandle, &LocalLockState);

	return (Error);
}

/**
   @brief  Return the PID for this slot

   This function returns the PID set for this slot.

   @param  SlotHandle           The Handle of the slot.
   @param  Pid_p                A pointer to where to put the PID value to be returned.

   @return                      A standard st error type...
                                - ST_NO_ERROR
 */
ST_ErrorCode_t stptiHAL_SlotGetPID(FullHandle_t SlotHandle, U16 * Pid_p)
{
	ST_ErrorCode_t Error = ST_NO_ERROR;
	stptiSupport_AccessLockState_t LocalLockState = stptiSupport_UNLOCKED;

	stptiOBJMAN_ReadLock(SlotHandle, &LocalLockState);
	{
		stptiHAL_Slot_t *Slot_p = stptiHAL_GetObjectSlot_p(SlotHandle);
		if (NULL != Slot_p)
			*Pid_p = Slot_p->PID;
		else
			Error =	ST_ERROR_BAD_PARAMETER;
	}
	stptiOBJMAN_Unlock(SlotHandle, &LocalLockState);

	return (Error);
}

/**
   @brief  Return the slot count and scrambling state for this slot.

   This function returns the slot count and scrambling state for this slot.

   @param  SlotHandle                The Handle of the slot.
   @param  TSPacketCount_p           (return) A pointer to where to put the count.
   @param  CurrentScramblingState_p  (return) A pointer to where to put the current scrambling state

   @return                           A standard st error type...
                                     - ST_NO_ERROR
 */
ST_ErrorCode_t stptiHAL_SlotGetState(FullHandle_t SlotHandle, U32 * TSPacketCount_p,
				     stptiHAL_ScrambledState_t * CurrentScramblingState_p)
{
	ST_ErrorCode_t Error = ST_NO_ERROR;
	stptiSupport_AccessLockState_t LocalLockState = stptiSupport_UNLOCKED;

	stptiOBJMAN_ReadLock(SlotHandle, &LocalLockState);
	{
		stptiHAL_vDevice_t *vDevice_p = stptiHAL_GetObjectvDevice_p(SlotHandle);
		stptiHAL_Slot_t *Slot_p = stptiHAL_GetObjectSlot_p(SlotHandle);
		volatile stptiTP_SlotInfo_t *SlotInfo_p = &vDevice_p->TP_Interface.SlotInfo_p[Slot_p->SlotIndex];

		if (TSPacketCount_p != NULL) {
			*TSPacketCount_p =
			    stptiHAL_pDeviceXP70Read(&vDevice_p->TP_Interface.SlotInfo_p[Slot_p->SlotIndex].packet_count);
		}

		if (CurrentScramblingState_p != NULL) {
			/* Detect PES Level Scrambling */
			if (SlotInfo_p->slot_state & STATUS_PES_LEVEL_SC_MASK) {
				*CurrentScramblingState_p = stptiHAL_PES_SCRAMBLED;
			} else {	/* Not PES level - see if it is Transport level */

				/* Currently this only indicates Transport Level Scrambling State */
				*CurrentScramblingState_p =
				    (stptiHAL_ScrambledState_t) ((SlotInfo_p->
								  slot_state & STATUS_TS_LEVEL_SC_MASK) >>
								 STATUS_TS_LEVEL_SC_OFFSET);
			}
		}
	}
	stptiOBJMAN_Unlock(SlotHandle, &LocalLockState);

	return (Error);
}

/**
   @brief  Set an Event for this slot

   This function set an event for this slot.

   @param  SlotHandle           The Handle of the slot.
   @param  Event                Event enum of interest
   @param  Enable               Enable or Disable?
   @return                      A standard st error type...
                                - ST_NO_ERROR
 */
ST_ErrorCode_t stptiHAL_SlotEnableEvent(FullHandle_t SlotHandle, stptiHAL_EventType_t Event, BOOL Enable)
{
	ST_ErrorCode_t Error = ST_NO_ERROR;
	stptiSupport_AccessLockState_t LocalLockState = stptiSupport_UNLOCKED;

	stptiOBJMAN_WriteLock(SlotHandle, &LocalLockState);
	{
		stptiHAL_vDevice_t *vDevice_p = stptiHAL_GetObjectvDevice_p(SlotHandle);
		stptiHAL_Slot_t *Slot_p = stptiHAL_GetObjectSlot_p(SlotHandle);
		stptiTP_StatusBlk_Flags_t Flag = 0;

		switch (Event) {
		case (stptiHAL_SCRAMBLE_TOCLEAR_EVENT):
			Flag = STATUS_BLK_SCRAMBLE_TOCLEAR;
			break;
		case (stptiHAL_CLEAR_TOSCRAM_EVENT):
			Flag = STATUS_BLK_CLEAR_TOSCRAMBLE;
			break;
		case (stptiHAL_PCR_RECEIVED_EVENT):
			Flag = STATUS_BLK_PCR_RECEIVED;
			break;

		default:
			Error = ST_ERROR_BAD_PARAMETER;
			break;
		}
		if (Enable) {
			stptiHAL_pDeviceXP70BitSet(&vDevice_p->TP_Interface.SlotInfo_p[Slot_p->SlotIndex].event_mask,
						   Flag);
		} else {
			stptiHAL_pDeviceXP70BitClear(&vDevice_p->TP_Interface.SlotInfo_p[Slot_p->SlotIndex].event_mask,
						     Flag);
		}
	}
	stptiOBJMAN_Unlock(SlotHandle, &LocalLockState);

	return (Error);
}

/**
   @brief  Alter transport_scramble_control bits to new values.

   This function allows the user to remap the transport_scramble_control to new values in DVB
   packets.  The Scramble control bits come as a pair.

   In DVB... 00 means the transport packet in the clear, 10 means the transport packet is scrambled
   with the even key, 11 means the transport packet is scrambled with the odd key, and 01 is
   "default" (implementation dependent).

   Typically the user will call this function 3 times to cover all 3 combinations of the SC bits
   (01 "default" being left as is).

   Only works for RAW slots and for DVB.

   @param  SlotHandle                The Handle of the RAW slot.
   @param  SCBitsToMatch             The 2bits of the transport_scramble_control field to match
   @param  ReplacementSCBits         The 2bits to put in the transport_scramble_control field when
                                     the above bits match

   @return                           A standard st error type...
                                     - ST_NO_ERROR
 */
ST_ErrorCode_t stptiHAL_SlotRemapScramblingBits(FullHandle_t SlotHandle, U8 SCBitsToMatch, U8 ReplacementSCBits)
{
	ST_ErrorCode_t Error = ST_NO_ERROR;
	stptiSupport_AccessLockState_t LocalLockState = stptiSupport_UNLOCKED;

	stptiOBJMAN_WriteLock(SlotHandle, &LocalLockState);
	{
		stptiHAL_vDevice_t *vDevice_p = stptiHAL_GetObjectvDevice_p(SlotHandle);
		stptiHAL_Slot_t *Slot_p = stptiHAL_GetObjectSlot_p(SlotHandle);

		if (Slot_p->SlotMode != stptiHAL_SLOT_TYPE_RAW) {
			Error = ST_ERROR_BAD_PARAMETER;
		} else {
			volatile stptiTP_SlotInfo_t *SlotInfo_p =
			    &vDevice_p->TP_Interface.SlotInfo_p[Slot_p->SlotIndex];
			U8 SCRemappingBits = stptiHAL_pDeviceXP70Read(&SlotInfo_p->u.raw_slot_info.SCRemappingBits);

			/* B1:0 is replacement SC bits when SC=0, B3:2 is replacement SC bits when SC=1,
			   B5:4 is replacement SC bits when SC=2, B7:6 is replacement SC bits when SC=3 */
			SCRemappingBits &= 3 << (SCBitsToMatch * 2);
			SCRemappingBits |= ReplacementSCBits << (SCBitsToMatch * 2);

			stptiHAL_pDeviceXP70Write(&SlotInfo_p->u.raw_slot_info.SCRemappingBits, SCRemappingBits);
		}
	}
	stptiOBJMAN_Unlock(SlotHandle, &LocalLockState);

	return (Error);
}

/**
   @brief  Setup post processed transport packet corruption.

   This function allows the user to set the corruption value and offset for transport packet
   corruption on RAW slots.  It will overwrite bytes with the CorruptionValue from the
   CorruptionOffset until the end of the packet.

   @param  SlotHandle                The Handle of the RAW slot.
   @param  CorruptOffset             Offset to start overwriting packet. Must be 1-188.  0 turns
                                     off transport packet corruption
   @param  CorruptionValue           Value to overwrite with.

   @return                           A standard st error type...
                                     - ST_NO_ERROR
 */
ST_ErrorCode_t stptiHAL_SlotSetCorruption(FullHandle_t SlotHandle, BOOL EnableCorruption, U8 CorruptionOffset,
					  U8 CorruptionValue)
{
	ST_ErrorCode_t Error = ST_NO_ERROR;
	stptiSupport_AccessLockState_t LocalLockState = stptiSupport_UNLOCKED;

	stptiOBJMAN_WriteLock(SlotHandle, &LocalLockState);
	{
		stptiHAL_vDevice_t *vDevice_p = stptiHAL_GetObjectvDevice_p(SlotHandle);
		stptiHAL_Slot_t *Slot_p = stptiHAL_GetObjectSlot_p(SlotHandle);
		volatile stptiTP_SlotInfo_t *SlotInfo_p = &vDevice_p->TP_Interface.SlotInfo_p[Slot_p->SlotIndex];

		if (Slot_p->SlotMode != stptiHAL_SLOT_TYPE_RAW) {
			Error = ST_ERROR_BAD_PARAMETER;
		} else {
			if (EnableCorruption) {
				stptiHAL_pDeviceXP70Write(&SlotInfo_p->u.raw_slot_info.CorruptionOffset,
							  CorruptionOffset);
				stptiHAL_pDeviceXP70Write(&SlotInfo_p->u.raw_slot_info.CorruptionValue,
							  CorruptionValue);
			} else {
				stptiHAL_pDeviceXP70Write(&SlotInfo_p->u.raw_slot_info.CorruptionOffset, 0);
				stptiHAL_pDeviceXP70Write(&SlotInfo_p->u.raw_slot_info.CorruptionValue, 0);
			}
		}
	}
	stptiOBJMAN_Unlock(SlotHandle, &LocalLockState);

	return (Error);
}

/**
   @brief  Enable or Disable a specific feature.

   This function allows the user to enable and disable CC checking (on any slot) and clear output
   (for RAW slots).

   @param  SlotHandle                The Handle of the slot.
   @param  Feature
   @param  Enable

   @return                           A standard st error type...
                                     - ST_NO_ERROR
 */
ST_ErrorCode_t stptiHAL_SlotFeatureEnable(FullHandle_t SlotHandle, stptiHAL_SlotFeature_t Feature, BOOL Enable)
{
	ST_ErrorCode_t Error = ST_NO_ERROR;
	stptiSupport_AccessLockState_t LocalLockState = stptiSupport_UNLOCKED;

	stptiOBJMAN_WriteLock(SlotHandle, &LocalLockState);
	{
		stptiHAL_vDevice_t *vDevice_p = stptiHAL_GetObjectvDevice_p(SlotHandle);
		stptiHAL_Slot_t *Slot_p = stptiHAL_GetObjectSlot_p(SlotHandle);
		volatile stptiTP_SlotInfo_t *SlotInfo_p = &vDevice_p->TP_Interface.SlotInfo_p[Slot_p->SlotIndex];
		stptiTP_SlotFlags_t slot_flag = 0;

		/* Slot mode validation */
		switch (Feature) {
		case stptiHAL_SLOT_OUTPUT_DNLA_TS_TAG:
		case stptiHAL_SLOT_CC_FIXUP:
			if (Slot_p->SlotMode != stptiHAL_SLOT_TYPE_RAW) {
				Error = STPTI_ERROR_INVALID_SLOT_TYPE;
			}
			break;

		case stptiHAL_SLOT_OUTPUT_BUFFER_COUNT:
			if (Slot_p->SlotMode != stptiHAL_SLOT_TYPE_ECM) {
				Error = STPTI_ERROR_INVALID_SLOT_TYPE;
			}
			break;

		default:
			break;
		}

		if (ST_NO_ERROR == Error) {
			switch (Feature) {
			case stptiHAL_SLOT_SUPPRESS_CC:
				slot_flag = SLOT_FLAGS_SUPPRESS_CC_CHECK;
				break;

			case stptiHAL_SLOT_OUTPUT_DNLA_TS_TAG:
				slot_flag = SLOT_FLAGS_PREFIX_DNLA_TS_TAG;
				break;

			case stptiHAL_SLOT_CC_FIXUP:
				slot_flag = SLOT_FLAGS_CC_FIXUP;
				break;

			case stptiHAL_SLOT_OUTPUT_BUFFER_COUNT:
				slot_flag = SLOT_FLAGS_COUNT_METADATA;
				break;

			default:
				Error = ST_ERROR_BAD_PARAMETER;
				break;
			}
		}

		if (ST_NO_ERROR == Error) {
			if (Enable) {
				stptiHAL_pDeviceXP70BitSet(&SlotInfo_p->slot_flags, slot_flag);
			} else {
				stptiHAL_pDeviceXP70BitClear(&SlotInfo_p->slot_flags, slot_flag);
			}
		}
	}
	stptiOBJMAN_Unlock(SlotHandle, &LocalLockState);

	return (Error);
}

/**
   @brief  Set the Secure Path Output Node.

   This function sets the Secure Path Output Node for RAW slots (all others are assumed clear).

   @param  SlotHandle               The Handle of the slot.
   @param  OutputNode               A choice between...
                                    - STPTI_SECURITY_OUTPUT_NODE_CLEAR
                                    - STPTI_SECURITY_OUTPUT_NODE_SCRAMBLED

   @return                          A standard st error type...
                                    - ST_NO_ERROR
 */
ST_ErrorCode_t stptiHAL_SlotSetSecurePathOutputNode(FullHandle_t SlotHandle, stptiHAL_SecureOutputNode_t OutputNode)
{
	ST_ErrorCode_t Error = ST_NO_ERROR;
	stptiSupport_AccessLockState_t LocalLockState = stptiSupport_UNLOCKED;

	stptiOBJMAN_WriteLock(SlotHandle, &LocalLockState);
	{
		stptiHAL_vDevice_t *vDevice_p = stptiHAL_GetObjectvDevice_p(SlotHandle);
		stptiHAL_Slot_t *Slot_p = stptiHAL_GetObjectSlot_p(SlotHandle);
		volatile stptiTP_SlotInfo_t *SlotInfo_p = &vDevice_p->TP_Interface.SlotInfo_p[Slot_p->SlotIndex];

		switch (OutputNode) {
		case stptiHAL_SECUREPATH_OUTPUT_NODE_CLEAR:
			stptiHAL_pDeviceXP70BitClear(&SlotInfo_p->slot_flags, SLOT_FLAGS_OUTPUT_SCR_NODE);	/* clear OUTPUT_SCR_NODE flag */
			break;

		case stptiHAL_SECUREPATH_OUTPUT_NODE_SCRAMBLED:
			if (Slot_p->SlotMode == stptiHAL_SLOT_TYPE_RAW) {
				stptiHAL_pDeviceXP70BitSet(&SlotInfo_p->slot_flags, SLOT_FLAGS_OUTPUT_SCR_NODE);	/* set OUTPUT_SCR_NODE flag */
			} else {
				Error = STPTI_ERROR_INVALID_SLOT_TYPE;
			}
			break;

		default:
			Error = ST_ERROR_BAD_PARAMETER;
			break;
		}
	}
	stptiOBJMAN_Unlock(SlotHandle, &LocalLockState);

	return (Error);
}

/**
   @brief  Set the Descrambler/Scrambler Path ID for this slot.

   This function sets the descrambler path id for this slot.

   @param  SlotHandle               The Handle of the slot.
   @param  PathID                   The path id.

   @return                          A standard st error type...
                                    - ST_NO_ERROR
 */
ST_ErrorCode_t stptiHAL_SlotSetSecurePathID(FullHandle_t SlotHandle, U32 PathID)
{
	ST_ErrorCode_t Error = ST_NO_ERROR;
	stptiSupport_AccessLockState_t LocalLockState = stptiSupport_UNLOCKED;

	stptiOBJMAN_WriteLock(SlotHandle, &LocalLockState);
	{
		stptiHAL_Slot_t *Slot_p = stptiHAL_GetObjectSlot_p(SlotHandle), *SlotAssoc_p;
		int index;
		stptiHAL_vDevice_t *vDevice_p = stptiHAL_GetObjectvDevice_p(SlotHandle);

		/* Set the Path ID for associated slots - associated
		   slots are used for record buffers only.
		   Only do this when no valid pid is set as the pid lookup
		   code afterwards will pickup all valid pid slots */
		if (Slot_p->PID == stptiHAL_InvalidPID) {
			volatile stptiTP_SlotInfo_t *SlotInfo_p =
			    &vDevice_p->TP_Interface.SlotInfo_p[Slot_p->SlotIndex];

			/* Set the Path ID for this slot */
			/* On TANGO the key index (secure path id) is only 16bits (discard upper 16bits) */
			stptiHAL_pDeviceXP70Write(&SlotInfo_p->key_index, (U16) PathID);
			Slot_p->SecurityPathID = PathID;

			stptiOBJMAN_FirstInList(
				&Slot_p->ObjectHeader.AssociatedObjects,
				(void *)&SlotAssoc_p,
				&index);
			while (index >= 0) {
				FullHandle_t ObjectHandle = stptiOBJMAN_ObjectPointerToHandle(SlotAssoc_p);
				if (stptiOBJMAN_GetObjectType(ObjectHandle) == OBJECT_TYPE_SLOT) {
					stptiHAL_Slot_t *AssocSlot_p = stptiHAL_GetObjectSlot_p(ObjectHandle);
					SlotInfo_p =
						&vDevice_p->TP_Interface.SlotInfo_p[AssocSlot_p->SlotIndex];

					/* On TANGO the key index (secure path id) is only 16bits (discard upper 16bits) */
					stptiHAL_pDeviceXP70Write(&SlotInfo_p->key_index, (U16) PathID);
					AssocSlot_p->SecurityPathID = PathID;
				}
				stptiOBJMAN_NextInList(&Slot_p->ObjectHeader.AssociatedObjects, (void *)&SlotAssoc_p,
					&index);
			}
		} else {
			/* Look up slots with the pid of interest new path ID
			   superceeds all previous ones on the slot chain. */
			FullHandle_t SlotHandles[MAX_CHAINED_SLOTS + 1];
			int NumSlotsFound;
			/* Lookup any slots already set to the new pid */
			NumSlotsFound =
				stptiHAL_SlotLookupPID(SlotHandle,
					Slot_p->PID,
					SlotHandles,
					sizeof(SlotHandles) /
					sizeof(SlotHandles[0]));

			for (index = 0; index < NumSlotsFound; index++) {
				stptiHAL_Slot_t *LookupSlot_p =
					stptiHAL_GetObjectSlot_p(SlotHandles[index]);
				volatile stptiTP_SlotInfo_t *SlotInfo_p =
					&vDevice_p->TP_Interface.SlotInfo_p[LookupSlot_p->SlotIndex];
				stptiHAL_pDeviceXP70Write(&SlotInfo_p->key_index, (U16) PathID);
				LookupSlot_p->SecurityPathID = PathID;
			}
		}
	}
	stptiOBJMAN_Unlock(SlotHandle, &LocalLockState);

	return (Error);
}

/**
   @brief  Set the secondary pid feature for the orimary and secondary slot

   This function sets up the TP secondary slot info

   @param  SecondarySlotHandle      Secondary pid secondary slot handle
   @param  PrimarySlotHandle        Secondary pid primary slot handle
   @param  Mode                     Operational mode

   @return                          A standard st error type...
                                    - ST_NO_ERROR
 */
ST_ErrorCode_t stptiHAL_SlotSetSecondaryPid(FullHandle_t SecondarySlotHandle, FullHandle_t PrimarySlotHandle,
					    stptiHAL_SecondaryPidMode_t Mode)
{
	ST_ErrorCode_t Error = ST_NO_ERROR;
	stptiSupport_AccessLockState_t LocalLockState = stptiSupport_UNLOCKED;

	stptiOBJMAN_WriteLock(SecondarySlotHandle, &LocalLockState);
	{
		stptiHAL_Slot_t *SecondarySlot_p = stptiHAL_GetObjectSlot_p(SecondarySlotHandle);
		stptiHAL_Slot_t *PrimarySlot_p = stptiHAL_GetObjectSlot_p(PrimarySlotHandle);
		stptiHAL_vDevice_t *vDevice_p = stptiHAL_GetObjectvDevice_p(SecondarySlotHandle);
		volatile stptiTP_SlotInfo_t *SecondarySlotInfo_p =
		    &vDevice_p->TP_Interface.SlotInfo_p[SecondarySlot_p->SlotIndex];
		volatile stptiTP_SlotInfo_t *PrimarySlotInfo_p =
		    &vDevice_p->TP_Interface.SlotInfo_p[PrimarySlot_p->SlotIndex];
		FullHandle_t AssocDataEntryObject;

		/* We need to sync before we write the data else */
		if ((stptiHAL_SECONDARY_TYPE_NONE != PrimarySlot_p->SecondarySlotType)
		    || (stptiHAL_SECONDARY_TYPE_NONE != SecondarySlot_p->SecondarySlotType)) {
			Error = STPTI_ERROR_SLOT_ALREADY_LINKED;
		} else if ((SecondarySlot_p->PID != stptiHAL_InvalidPID)
			   || (SecondarySlot_p->PID != stptiHAL_InvalidPID)) {
			/* You must set the pids after you link - this ensures the slots are not part of any slot chains */
			Error = STPTI_ERROR_SLOT_NOT_ASSOCIATED;
		} else
		    if ((stptiOBJMAN_ReturnAssociatedObjects
			 (SecondarySlotHandle, &AssocDataEntryObject, 1, OBJECT_TYPE_DATA_ENTRY) > 0)
			||
			(stptiOBJMAN_ReturnAssociatedObjects
			 (PrimarySlotHandle, &AssocDataEntryObject, 1, OBJECT_TYPE_DATA_ENTRY) > 0)) {
			/* Data Entry and Secondary Pid are incompatible features */
			Error = STPTI_ERROR_SLOT_NOT_ASSOCIATED;
		} else {
			U16 SecondaryMode = 0;
			/* No pids are set so we don't need to worry about the order we write the slot info */
			switch (Mode) {
			case stptiHAL_SECONDARY_PID_MODE_INSERTION:
				SecondaryMode = SECONDARY_PID_INSERTION_MODE;
				break;
			case stptiHAL_SECONDARY_PID_MODE_INSERTDELETE:
				SecondaryMode = SECONDARY_PID_INSERT_DELETE_MODE;
				break;
			case stptiHAL_SECONDARY_PID_MODE_SUBSTITUTION:
			default:
				Mode = stptiHAL_SECONDARY_PID_MODE_SUBSTITUTION;
				SecondaryMode = SECONDARY_PID_SUBSTITUTION_MODE;
				break;
			}
			PrimarySlot_p->SecondarySlotType = stptiHAL_SECONDARY_TYPE_PRIM;
			SecondarySlot_p->SecondarySlotType = stptiHAL_SECONDARY_TYPE_SEC;
			PrimarySlot_p->SecondaryHandle = SecondarySlotHandle;
			SecondarySlot_p->SecondaryHandle = PrimarySlotHandle;
			PrimarySlot_p->SecondarySlotMode = Mode;
			SecondarySlot_p->SecondarySlotMode = Mode;
			stptiHAL_pDeviceXP70Write(&PrimarySlotInfo_p->secondary_pid_info,
						  SECONDARY_PID_PRIMARY_SLOT | SecondaryMode);
			stptiHAL_pDeviceXP70Write(&SecondarySlotInfo_p->secondary_pid_info,
						  SECONDARY_PID_SECONDARY_SLOT | PrimarySlot_p->PID);
		}
	}
	stptiOBJMAN_Unlock(SecondarySlotHandle, &LocalLockState);
	return Error;
}

/**
   @brief  Get the secondary pid mode for the slot

   This function reads the object manage setting for this slot to obtain the secondary pid mode.

   @param  SlotHandle               Slot handle
   @param  Mode_p                   Pointer to return operational mode of the slot.

   @return                          A standard st error type...
                                    - ST_NO_ERROR
 */
ST_ErrorCode_t stptiHAL_SlotGetSecondaryPid(FullHandle_t SlotHandle, stptiHAL_SecondaryPidMode_t *Mode_p)
{
	stptiSupport_AccessLockState_t LocalLockState = stptiSupport_UNLOCKED;

	stptiOBJMAN_ReadLock(SlotHandle, &LocalLockState);
	{
		stptiHAL_Slot_t *Slot_p = stptiHAL_GetObjectSlot_p(SlotHandle);
		if (Slot_p->SecondarySlotType == stptiHAL_SECONDARY_TYPE_NONE) {
			*Mode_p = stptiHAL_SECONDARY_PID_MODE_NONE;
		} else {
			*Mode_p = Slot_p->SecondarySlotMode;
		}
	}
	stptiOBJMAN_Unlock(SlotHandle, &LocalLockState);
	return ST_NO_ERROR;
}

/**
   @brief  Disable the secondary pid feature for the primary and secondary slot

   This function disables the TP secondary pid feature on primary and secondary slots

   @param  SecondarySlotHandle      Secondary pid secondary slot handle
   @param  PrimarySlotHandle        Secondary pid primary slot handle

   @return                          A standard st error type...
                                    - ST_NO_ERROR
 */
ST_ErrorCode_t stptiHAL_SlotClearSecondaryPid(FullHandle_t SecondarySlotHandle, FullHandle_t PrimarySlotHandle)
{
	ST_ErrorCode_t Error = ST_NO_ERROR;
	stptiSupport_AccessLockState_t LocalLockState = stptiSupport_UNLOCKED;

	stptiOBJMAN_WriteLock(SecondarySlotHandle, &LocalLockState);
	{
		stptiHAL_Slot_t *SecondarySlot_p = stptiHAL_GetObjectSlot_p(SecondarySlotHandle);
		stptiHAL_Slot_t *PrimarySlot_p = stptiHAL_GetObjectSlot_p(PrimarySlotHandle);
		stptiHAL_vDevice_t *vDevice_p = stptiHAL_GetObjectvDevice_p(SecondarySlotHandle);
		volatile stptiTP_SlotInfo_t *SecondarySlotInfo_p =
		    &vDevice_p->TP_Interface.SlotInfo_p[SecondarySlot_p->SlotIndex];
		volatile stptiTP_SlotInfo_t *PrimarySlotInfo_p =
		    &vDevice_p->TP_Interface.SlotInfo_p[PrimarySlot_p->SlotIndex];

		/* We need to make sure the both of the slot handles passed in, are indeed linked to each other */
		if ((stptiHAL_SECONDARY_TYPE_NONE == PrimarySlot_p->SecondarySlotType)
		    || (stptiHAL_SECONDARY_TYPE_NONE == SecondarySlot_p->SecondarySlotType)) {
			/* Either one or both slots are not being used for secondary pid feature */
			Error = STPTI_ERROR_SLOT_NOT_ASSOCIATED;
		} else if (stptiOBJMAN_IsHandlesEqual(PrimarySlot_p->SecondaryHandle, SecondarySlotHandle)
			   && stptiOBJMAN_IsHandlesEqual(SecondarySlot_p->SecondaryHandle, PrimarySlotHandle)) {
			/* Slots are linked so proceed */
			PrimarySlot_p->SecondarySlotType = stptiHAL_SECONDARY_TYPE_NONE;
			SecondarySlot_p->SecondarySlotType = stptiHAL_SECONDARY_TYPE_NONE;
			PrimarySlot_p->SecondaryHandle = stptiOBJMAN_NullHandle;
			SecondarySlot_p->SecondaryHandle = stptiOBJMAN_NullHandle;

			stptiHAL_pDeviceXP70Write(&SecondarySlotInfo_p->secondary_pid_info, 0);
			stptiHAL_pDeviceXP70Write(&PrimarySlotInfo_p->secondary_pid_info, 0);
			stptiHAL_SlotResetState(&PrimarySlotHandle, 1, FALSE);
		} else {
			/* Slots are used for the secondary pid feature but not linked to each other */
			Error = STPTI_ERROR_SLOT_NOT_ASSOCIATED;
		}
	}
	stptiOBJMAN_Unlock(SecondarySlotHandle, &LocalLockState);
	return Error;
}

/**
   @brief  Set an Remap pid for this slot

   This function set a remap pid for this slot.

   @param  SlotHandle           The Handle of the slot.
   @param  Pid                  Remap pid value
   @return                      A standard st error type...
                                - ST_NO_ERROR
 */
ST_ErrorCode_t stptiHAL_SlotSetRemapPID(FullHandle_t SlotHandle, U16 Pid)
{
	stptiSupport_AccessLockState_t LocalLockState = stptiSupport_UNLOCKED;

	stptiOBJMAN_WriteLock(SlotHandle, &LocalLockState);
	{
		stptiHAL_vDevice_t *vDevice_p = stptiHAL_GetObjectvDevice_p(SlotHandle);
		stptiHAL_Slot_t *Slot_p = stptiHAL_GetObjectSlot_p(SlotHandle);

		if (Slot_p->SlotMode == stptiHAL_SLOT_TYPE_RAW) {
			stptiHAL_pDeviceXP70Write(&vDevice_p->TP_Interface.SlotInfo_p[Slot_p->SlotIndex].u.
						  raw_slot_info.RemapPid, Pid);
		}
	}
	stptiOBJMAN_Unlock(SlotHandle, &LocalLockState);

	return (ST_NO_ERROR);
}

/* Supporting Functions ------------------------------------------------------------------------- */

/**
   @brief  Resets the state of a slot or slots

   This function resets one or more slots. Note this function blocks if any of the supplied slots
   is active, for upto a packet period.

   @param  SlotHandleList     A list of slots to reset
   @param  NumberOfHandles    The size of the above list
   @param  KeepDeactivated    FALSE reactivate the slots afterwards, TRUE keep deactivated (and
                              you will need to call stptiHAL_SlotActivate() to activate it later.

 */
void stptiHAL_SlotResetState(FullHandle_t *SlotHandleList, unsigned int NumberOfHandles, BOOL KeepDeactivated)
{
	unsigned int i;
	BOOL ActiveSlotFound = FALSE;

	for (i = 0; i < NumberOfHandles; i++) {
		stptiHAL_vDevice_t *vDevice_p = stptiHAL_GetObjectvDevice_p(SlotHandleList[i]);	/* potential optimisation: they should all be from the same vDevice */
		stptiHAL_Slot_t *Slot_p = stptiHAL_GetObjectSlot_p(SlotHandleList[i]);

		/* Setting the SlotMode to NULL will deactivate it */
		stptiHAL_pDeviceXP70Write(&vDevice_p->TP_Interface.SlotInfo_p[Slot_p->SlotIndex].slot_mode,
					  SLOTTYPE_NULL);

		if (Slot_p->PID != stptiHAL_InvalidPID) {
			ActiveSlotFound = TRUE;
		}
	}

	/* If we have made an active slot inactive, we must synchronise with the TP to make
	   sure it is no longer in use when we reset the slot states. */
	if (ActiveSlotFound) {
		stptiHAL_SlotTPSync(SlotHandleList[0]);
	}

	/* Reset the slots states */
	for (i = 0; i < NumberOfHandles; i++) {
		stptiHAL_vDevice_t *vDevice_p = stptiHAL_GetObjectvDevice_p(SlotHandleList[i]);
		stptiHAL_Slot_t *Slot_p = stptiHAL_GetObjectSlot_p(SlotHandleList[i]);
		FullHandle_t AssocIndexObject;

		/* Reset the slot state (TP header status ) */
		stptiHAL_pDeviceXP70Write(&vDevice_p->TP_Interface.SlotInfo_p[Slot_p->SlotIndex].slot_state,
					  STATUS_SLOT_STATE_CLEAR | STATUS_TS_LEVEL_SC_NOT_SYNCED);
		stptiHAL_pDeviceXP70Write(&vDevice_p->TP_Interface.SlotInfo_p[Slot_p->SlotIndex].remaining_pes_header_length, 0);

		/* Reset slot specific states */
		switch (Slot_p->SlotMode) {
		case stptiHAL_SLOT_TYPE_ECM:
		case stptiHAL_SLOT_TYPE_EMM:
		case stptiHAL_SLOT_TYPE_SECTION:
			stptiHAL_pDeviceXP70Write(&vDevice_p->TP_Interface.SlotInfo_p[Slot_p->SlotIndex].u.section_info.stage, 0);
			break;

		case stptiHAL_SLOT_TYPE_PES:
			stptiHAL_pDeviceXP70Write(&vDevice_p->TP_Interface.SlotInfo_p[Slot_p->SlotIndex].u.pes_fsm.pes_state, 0);

			/* If there are markers buffers allocated then free them */
			if (stptiHAL_pDeviceXP70Read
			    (&vDevice_p->TP_Interface.SlotInfo_p[Slot_p->SlotIndex].u.pes_fsm.pes_marker_index) != 0xFF) {
				U8 index = stptiHAL_pDeviceXP70Read(&vDevice_p->TP_Interface.SlotInfo_p[Slot_p->SlotIndex].u.pes_fsm.pes_marker_index);
				U8 next_index = 0xFF;

				do {
					stptiHAL_pDeviceXP70Write(&vDevice_p->TP_Interface.PesMarkerBuffers_p[index].allocated, 0);
					next_index =
					    stptiHAL_pDeviceXP70Read(&vDevice_p->TP_Interface.PesMarkerBuffers_p[index].next_index);
					stptiHAL_pDeviceXP70Write(&vDevice_p->TP_Interface.PesMarkerBuffers_p[index].next_index, 0xFF);
					index = next_index;
				} while (index != 0xFF);
			}
			break;
		default:
			break;
		}

		/* Check for associated indexes */
		if (stptiOBJMAN_ReturnAssociatedObjects(SlotHandleList[i], &AssocIndexObject, 1, OBJECT_TYPE_INDEX) > 0) {
			/* Index associated, we need to reset the slot to point to this index in case index chaining
			   is used and we have advanced down the chain. */
			stptiHAL_Index_t *Index_p = stptiHAL_GetObjectIndex_p(AssocIndexObject);
			stptiHAL_pDeviceXP70Write(&vDevice_p->TP_Interface.SlotInfo_p[Slot_p->SlotIndex].indexer,
						  Index_p->IndexerIndex);
		}

		/* Windback the write pointer of any associated buffers to discard any partial units */
		if (Slot_p->SlotMode != stptiHAL_SLOT_TYPE_RAW) {
			/* For all slot types other than RAW we must reset the associated buffer */
			FullHandle_t BufferHandle;
			int CountOfAssocdBufferHandles =
			    stptiOBJMAN_ReturnAssociatedObjects(SlotHandleList[i], &BufferHandle, 1,
								OBJECT_TYPE_BUFFER);
			if (CountOfAssocdBufferHandles > 0) {
				stptiHAL_BufferWindbackQWrite(BufferHandle);
			}
		}

		if (!KeepDeactivated) {
			stptiHAL_SlotActivate(SlotHandleList[i]);
		}

	}
}

/**
   @brief  Activates a slot

   This function activates a slot, by setting its mode (from NULL).  A pid will still need to be
   set for the slot to receive any data.

   @param  SlotHandle           A list of slots to reset

 */
void stptiHAL_SlotActivate(FullHandle_t SlotHandle)
{
	stptiHAL_vDevice_t *vDevice_p = stptiHAL_GetObjectvDevice_p(SlotHandle);
	stptiHAL_Slot_t *Slot_p = stptiHAL_GetObjectSlot_p(SlotHandle);

	if (stptiHAL_SLOT_TYPE_PARTIAL_PES == Slot_p->SlotMode) {
		stptiHAL_pDeviceXP70Write(&vDevice_p->TP_Interface.SlotInfo_p[Slot_p->SlotIndex].slot_mode,
					  SLOTTYPE_RAW);
		/* but note that Slot_p->SlotMode remains stptiHAL_SLOT_TYPE_PARTIAL_PES */
	} else {
		stptiHAL_pDeviceXP70Write(&vDevice_p->TP_Interface.SlotInfo_p[Slot_p->SlotIndex].slot_mode,
					  (stptiTP_SlotMode_t) Slot_p->SlotMode);
	}
}

/**
   @brief  Wait for the Slot not be in use by the TP

   This function waits until slot processing has completed.  If you disable a slot (set an invalid pid)
   and call this function then you are guaranteed that the slot is no longer in use.

   @param  SlotHandle       A Handle to the slot to wait on (can be any slot on the pDevice).

   @return                  A standard st error type...
                            - ST_NO_ERROR if no errors
 */
static ST_ErrorCode_t stptiHAL_SlotTPSync(FullHandle_t SlotHandle)
{
	ST_ErrorCode_t Error = ST_NO_ERROR;
	stptiHAL_vDevice_t *vDevice_p = stptiHAL_GetObjectvDevice_p(SlotHandle);

	/* Make sure we have no TP Sync's pending */
	stptiSupport_SubMilliSecondWait(stptiHAL_pDeviceXP70Read(&vDevice_p->TP_Interface.pDeviceInfo_p->SyncTP) !=
					SYNC_TP_SYNCHRONISED, &Error);

	if (Error != ST_ERROR_TIMEOUT) {
		/* Wait for Slot Sync */
		stptiHAL_pDeviceXP70Write(&vDevice_p->TP_Interface.pDeviceInfo_p->SyncTP, SYNC_TP_SLOT_WAIT);
		stptiSupport_SubMilliSecondWait(stptiHAL_pDeviceXP70Read(&vDevice_p->TP_Interface.pDeviceInfo_p->SyncTP)
						!= SYNC_TP_SYNCHRONISED, &Error);
	}

	if (Error == ST_ERROR_TIMEOUT) {
		STPTI_PRINTF_ERROR("Unable to obtain Slot SYNC point - TP stalled?");
	}

	stptiHAL_pDeviceXP70Write(&vDevice_p->TP_Interface.pDeviceInfo_p->SyncTP, SYNC_TP_SYNCHRONISED);	/* force it to be set (in case of a timeout) */

	return (Error);
}

/**
   @brief  Synchronise the Slot DiscardOnCRCError setting with all the filters DiscardOnCRCError settings.

   This function updates the slots DiscardOnCRCError appropriately for this slot (DiscardOnCRCError
   is only allowed to be true if all the filters are true - i.e. bad sections are not discarded by
   a slot if any filter on that slot wants to see them).

   @param  SlotHandle           A handle to the slot to update the filter state for.

   @return                      A standard st error type...
                                - ST_ERROR_BAD_PARAMETER   (invalid slot type)
                                - ST_NO_ERROR
 */
ST_ErrorCode_t stptiHAL_SlotSyncDiscardOnCRCError(FullHandle_t SlotHandle)
{
	ST_ErrorCode_t Error = ST_NO_ERROR;

	stptiHAL_vDevice_t *vDevice_p = stptiHAL_GetObjectvDevice_p(SlotHandle);
	stptiHAL_Slot_t *Slot_p = stptiHAL_GetObjectSlot_p(SlotHandle);
	stptiHAL_Filter_t *AssocFilter_p;
	BOOL DiscardOnCRCError = TRUE;
	int index;

	stptiOBJMAN_FirstInList(&Slot_p->ObjectHeader.AssociatedObjects, (void *)&AssocFilter_p, &index);
	while (index >= 0) {
		if (stptiOBJMAN_GetObjectType(AssocFilter_p->ObjectHeader.Handle) == OBJECT_TYPE_FILTER) {
			if (!AssocFilter_p->DiscardOnCRCError) {
				DiscardOnCRCError = FALSE;
				break;
			}
		}
		stptiOBJMAN_NextInList(&Slot_p->ObjectHeader.AssociatedObjects, (void *)&AssocFilter_p, &index);
	}

	switch (Slot_p->SlotMode) {
	case stptiHAL_SLOT_TYPE_ECM:
	case stptiHAL_SLOT_TYPE_EMM:
	case stptiHAL_SLOT_TYPE_SECTION:
		if (DiscardOnCRCError) {
			/* Windback on CRC error */
			stptiHAL_pDeviceXP70BitSet(&vDevice_p->TP_Interface.SlotInfo_p[Slot_p->SlotIndex].u.section_info.filter_flags, DISCARD_ON_CRC);
		} else {
			/* Do NOT Windback on CRC error */
			stptiHAL_pDeviceXP70BitClear(&vDevice_p->TP_Interface.SlotInfo_p[Slot_p->SlotIndex].u.section_info.filter_flags, DISCARD_ON_CRC);
		}
		break;

	default:
		Error = ST_ERROR_BAD_PARAMETER;
		break;
	}

	return (Error);
}

/**
   @brief  Update filter state

   This function updates the filter state, and sets the DiscardOnCRCError appropriately for this slot
   (DiscardOnCRCError is only allowed to be true if all the filters are true).  It is possible to call
   this function to synchronise the filter state without updating the filter enable/disable state.

   @param  SlotHandle           A handle to the slot to update the filter state for.
   @param  FilterHandle         A handle to the filter being enabled or disabled.
   @param  DisableFilter        TRUE if disabling.
   @param  DisableEnable        TRUE if enabling.

   @return                      A standard st error type...
                                - ST_NO_ERROR
 */
ST_ErrorCode_t stptiHAL_SlotUpdateFilterState(FullHandle_t SlotHandle, FullHandle_t FilterHandle, BOOL DisableFilter,
					      BOOL EnableFilter)
{
	ST_ErrorCode_t Error = ST_NO_ERROR;

	stptiHAL_vDevice_t *vDevice_p = stptiHAL_GetObjectvDevice_p(SlotHandle);
	stptiHAL_Slot_t *Slot_p = stptiHAL_GetObjectSlot_p(SlotHandle);
	stptiHAL_Filter_t *Filter_p = stptiHAL_GetObjectFilter_p(FilterHandle);

	int FilterIndex = Filter_p->FilterIndex;
	stptiHAL_FilterType_t FilterType = Filter_p->FilterType;
	stptiTP_SectionParams_t SectionParams = stptiHAL_FilterConvertToTPType(FilterType);

	if (EnableFilter) {
		Filter_p->FilterEnabled = TRUE;
	} else if (DisableFilter) {
		Filter_p->FilterEnabled = FALSE;
	}

	/* for ECM slots we reset them to make sure that the key change detection is also reset */
	if (Slot_p->SlotMode == stptiHAL_SLOT_TYPE_ECM) {
		stptiHAL_SlotResetState(&SlotHandle, 1, TRUE);
	}

	/* Work out which filter structure to use in slot_info */
	switch (Slot_p->SlotMode) {
	case stptiHAL_SLOT_TYPE_ECM:
	case stptiHAL_SLOT_TYPE_EMM:
	case stptiHAL_SLOT_TYPE_SECTION:
		/* If the slot is a LONG or PNMM we can just inclusive OR the type. */
		if (SectionParams & (SF_MODE_LONG | SF_MODE_PNMM)) {
			char val =
			    stptiHAL_pDeviceXP70Read(&vDevice_p->TP_Interface.SlotInfo_p[Slot_p->SlotIndex].u.section_info.section_params);

			if (SF_MODE_NONE == val) {
				stptiHAL_pDeviceXP70Write(&vDevice_p->TP_Interface.SlotInfo_p[Slot_p->SlotIndex].u.section_info.section_params, SectionParams);
			} else {
				stptiHAL_pDeviceXP70ReadModifyWrite(&vDevice_p->TP_Interface.
								    SlotInfo_p[Slot_p->SlotIndex].u.section_info.section_params,
								    (char)(~(SF_MODE_LONG | SF_MODE_PNMM)),
								    SectionParams);
			}
		} else {
			stptiHAL_pDeviceXP70Write(&vDevice_p->TP_Interface.SlotInfo_p[Slot_p->SlotIndex].u.section_info.section_params,
						  SectionParams);
		}
		if (FilterType == stptiHAL_TINY_FILTER) {
			/* Tiny Section filter uses the filters_associate field to store the filter values */
			/* The filter is always active, so to disable it we have to use known non-matching section values (TID is not allowed to be 0xFF) */
			if (Filter_p->FilterEnabled) {
				stptiHAL_pDeviceXP70Write(&vDevice_p->TP_Interface.SlotInfo_p[Slot_p->SlotIndex].u.section_info.u.tiny_filter.FilterData,
							  Filter_p->u.TinyFilter.FilterData);
				stptiHAL_pDeviceXP70Write(&vDevice_p->TP_Interface.SlotInfo_p[Slot_p->SlotIndex].u.section_info.u.tiny_filter.FilterMask,
							  Filter_p->u.TinyFilter.FilterMask);
				stptiHAL_pDeviceXP70Write(&vDevice_p->TP_Interface.SlotInfo_p[Slot_p->SlotIndex].u.section_info.u.tiny_filter.FilterAnyMatchMask,
							  Filter_p->u.TinyFilter.FilterAnyMatchMask);
			} else {
				stptiHAL_pDeviceXP70Write(&vDevice_p->TP_Interface.SlotInfo_p[Slot_p->SlotIndex].u.section_info.u.tiny_filter.FilterData, 0xFFFF);
				stptiHAL_pDeviceXP70Write(&vDevice_p->TP_Interface.SlotInfo_p[Slot_p->SlotIndex].u.section_info.u.tiny_filter.FilterMask, 0xFFFF);
				stptiHAL_pDeviceXP70Write(&vDevice_p->TP_Interface.SlotInfo_p[Slot_p->SlotIndex].u.section_info.u.tiny_filter.FilterAnyMatchMask, 0);
			}
		} else if (FilterType == stptiHAL_PROPRIETARY_FILTER) {
			if (Filter_p->FilterEnabled) {
				stptiHAL_pDeviceXP70MemcpyTo((void *)vDevice_p->TP_Interface.ProprietaryFilters_p,
							     &Filter_p->u.ProprietaryFilter,
							     sizeof(stptiTP_ProprietaryFilter_t));
				stptiHAL_pDeviceXP70Write(&vDevice_p->TP_Interface.SlotInfo_p[Slot_p->SlotIndex].u.section_info.u.proprietary_filter_index,
							  vDevice_p->ProprietaryFilterIndex);
			} else {
				/* Disable filtering */
				stptiHAL_pDeviceXP70Write(&vDevice_p->TP_Interface.SlotInfo_p[Slot_p->SlotIndex].u.section_info.u.proprietary_filter_index,
							  0xFFFFFFFF);
			}
			if (ST_NO_ERROR == Error) {
				Error = stptiHAL_SlotSyncDiscardOnCRCError(SlotHandle);
			}
		} else {
			if (Filter_p->FilterEnabled) {
				if (FilterIndex < 64) {
					stptiHAL_pDeviceXP70BitSet(&vDevice_p->TP_Interface.SlotInfo_p[Slot_p->SlotIndex].u.section_info.u.filters_associated,
								   (1 << FilterIndex));
				} else {
					Filter_p->FilterEnabled = FALSE;
					Error = STPTI_ERROR_INVALID_FILTER_HANDLE;
				}
			} else {	/* Disable Filter */

				if (FilterIndex < 64) {
					stptiHAL_pDeviceXP70BitClear(&vDevice_p->TP_Interface.SlotInfo_p[Slot_p->SlotIndex].u.section_info.u.filters_associated,
								     (1 << FilterIndex));
				} else {
					Error = STPTI_ERROR_INVALID_FILTER_HANDLE;
				}
			}
		}
		if (ST_NO_ERROR == Error) {
			Error = stptiHAL_SlotSyncDiscardOnCRCError(SlotHandle);
		}
		break;

	case stptiHAL_SLOT_TYPE_PES:
		STPTI_ASSERT(FilterType == stptiHAL_PES_STREAMID_FILTER, "Must be PES filter for Slot type PES");	/* This should be already checked by the filter associate function */
		if (Filter_p->FilterEnabled) {
			stptiHAL_pDeviceXP70Write(&vDevice_p->TP_Interface.SlotInfo_p[Slot_p->SlotIndex].u.pes_fsm.pes_streamid_filterdata,
						  Filter_p->u.PESStreamIDMask);
		} else {
			stptiHAL_pDeviceXP70Write(&vDevice_p->TP_Interface.SlotInfo_p[Slot_p->SlotIndex].u.pes_fsm.pes_streamid_filterdata, 0);
		}
		break;

	default:
		Filter_p->FilterEnabled = FALSE;
		Error = ST_ERROR_FEATURE_NOT_SUPPORTED;
		break;
	}

	/* re-enable ECM slots from where we reset it above */
	if (Slot_p->SlotMode == stptiHAL_SLOT_TYPE_ECM) {
		stptiHAL_SlotActivate(SlotHandle);
	}

	return (Error);
}

/**
   @brief  Check the list of slots and unchain this slot if necessary.

   This function manages slot chaining for removing slots.  Its complement is stptiHAL_SlotChainSetPID
   used when setting pids on slots.

   The aim of this function is to maintain...
     - vDevice_p->PidIndexes to point to the head slot (or remove entry if this is the last)
     - maintain the Slot_t PreviousSlotInChain, NextSlotInChain references
     - clear the pid in the TP's pid table (if this is the last)
     - maintain the slot chain in the TP slot info table

   @param  vDevice_p            A pointer to the vDevice
   @param  SlotToBeUnchained_p  A pointer to the the Slot to be unchained

   @return                      A standard st error type...
                                - ST_NO_ERROR
 */
static void stptiHAL_SlotUnchainClearPID(stptiHAL_vDevice_t *vDevice_p, stptiHAL_Slot_t *SlotToBeUnchained_p)
{
	stptiHAL_Slot_t *PreviousSlotInChain_p = stptiHAL_GetObjectSlot_p(SlotToBeUnchained_p->PreviousSlotInChain);
	stptiHAL_Slot_t *NextSlotInChain_p = stptiHAL_GetObjectSlot_p(SlotToBeUnchained_p->NextSlotInChain);

	if (PreviousSlotInChain_p == NULL) {
		/* No previous slot, so this is the head slot */
		stpti_printf("This slot is the head and so not referenced.");

		if (SlotToBeUnchained_p->PID == stptiHAL_WildCardPID) {
			if (NextSlotInChain_p == NULL) {
				/* Slot being unchained is WildCard, and there are no other Slots in the chain,
				   so we should disable WildCard feature. */
				stptiHAL_pDeviceXP70Write(&vDevice_p->TP_Interface.vDeviceInfo_p->wildcard_slot_index,
							  (U16) 0xFFFF);
			} else {
				/* More wildcard head slot index down to the next slot index */
				stptiHAL_pDeviceXP70Write(&vDevice_p->TP_Interface.vDeviceInfo_p->wildcard_slot_index,
							  (U16) NextSlotInChain_p->SlotIndex);
			}
		}

		if (NextSlotInChain_p != NULL) {
			/* This is a head slot, so we need to make the slot mapping entry now points to the next slot in the chain */
			stptiOBJMAN_ReplaceItemInList(&vDevice_p->PidIndexes, SlotToBeUnchained_p->PidIndex,
						      NextSlotInChain_p);
			stptiHAL_pDeviceXP70Write(&vDevice_p->TP_Interface.
						  PIDSlotMappingTable_p[NextSlotInChain_p->PidIndex],
						  (U16) (NextSlotInChain_p->SlotIndex));
			NextSlotInChain_p->PreviousSlotInChain = stptiOBJMAN_NullHandle;
		} else {
			/* Last Slot in the Chain so we deallocate the PidIndex */
			/* Now we dealt with the "head" slot case, we can now clear the pid disabling this slot. */
			if (SlotToBeUnchained_p->PidIndex >= 0) {
				stptiHAL_pDeviceXP70Write(&vDevice_p->TP_Interface.
							  PIDTable_p[SlotToBeUnchained_p->PidIndex],
							  stptiHAL_InvalidPID);
				stptiOBJMAN_RemoveFromList(&vDevice_p->PidIndexes, SlotToBeUnchained_p->PidIndex);
			}
		}
	} else {		/* ( PreviousSlotInChain_p != NULL ) */

		/* There is a previous slot (slot being removed is not the head of the chain) */
		stpti_printf("This slot %d was not the head and will need to be skipped.",
			     SlotToBeUnchained_p->SlotIndex);
		if (NextSlotInChain_p != NULL) {
			/* There is a next slot (slot being removed is not the tail of the chain) */
			stpti_printf
			    ("This slot was not the tail and so the previous Slot %d will need to link to Slot %d.",
			     PreviousSlotInChain_p->SlotIndex, NextSlotInChain_p->SlotIndex);
			PreviousSlotInChain_p->NextSlotInChain = SlotToBeUnchained_p->NextSlotInChain;
			NextSlotInChain_p->PreviousSlotInChain = SlotToBeUnchained_p->PreviousSlotInChain;
			stptiHAL_pDeviceXP70Write(&vDevice_p->TP_Interface.SlotInfo_p[PreviousSlotInChain_p->SlotIndex].
						  next_slot, NextSlotInChain_p->SlotIndex);
		} else {
			/* There is no next slot (slot being removed is the tail of the chain) */
			stpti_printf("This slot is the tail, so the previous slot %d is now the tail.",
				     PreviousSlotInChain_p->SlotIndex);
			PreviousSlotInChain_p->NextSlotInChain = stptiOBJMAN_NullHandle;
			stptiHAL_pDeviceXP70Write(&vDevice_p->TP_Interface.SlotInfo_p[PreviousSlotInChain_p->SlotIndex].
						  next_slot, 0xFFFF);
		}
	}

	/* Unlink this slot */
	SlotToBeUnchained_p->PreviousSlotInChain = stptiOBJMAN_NullHandle;
	SlotToBeUnchained_p->NextSlotInChain = stptiOBJMAN_NullHandle;
	stptiHAL_pDeviceXP70Write(&vDevice_p->TP_Interface.SlotInfo_p[SlotToBeUnchained_p->SlotIndex].next_slot,
				  0xFFFF);
	SlotToBeUnchained_p->PID = stptiHAL_InvalidPID;
	SlotToBeUnchained_p->PidIndex = -1;
}

/**
   @brief  Check the list of slots and Chain this slot if necessary.

   This function manages slot chaining when setting slots.  Its complement is
   stptiHAL_SlotUnchainClearPID used when clearing pids on slots / deallocating slots.

   The aim of this function is to maintain...
     - maintain the Slot_t PreviousSlotInChain, NextSlotInChain references
     - add new slots with the same pid to the end of the chain
     - Set the pid in the TP's pid table (if this is the first)
     - maintain the slot chain in the TP slot info table

   @param  vDevice_p            A pointer to the vDevice
   @param  SlotToBeUnchained_p  A pointer to the the Slot to be unchained

   @return                      Nothing
 */
static ST_ErrorCode_t stptiHAL_SlotChainSetPID(stptiHAL_vDevice_t *vDevice_p, stptiHAL_Slot_t *SlotToBeChained_p,
					       U16 NewPid)
{
	ST_ErrorCode_t Error = ST_NO_ERROR;

	stptiHAL_Slot_t *HeadSlot_p = NULL, *TailSlot_p = NULL;
	BOOL NoOtherSlotsWithThisPID = TRUE;
	int PidIndex;

	/* Work through the PidIndexes looking for the PID we are setting.  The PidIndex list
	 * always points to the head slot (first slot in a slot chain). */
	stptiOBJMAN_FirstInList(&vDevice_p->PidIndexes, (void *)&HeadSlot_p, &PidIndex);
	while (PidIndex >= 0) {
		if (HeadSlot_p->PID == NewPid) {
			/* Found a Slot with this PID, now find the tail... */
			NoOtherSlotsWithThisPID = FALSE;
			TailSlot_p = HeadSlot_p;
			while (!stptiOBJMAN_IsNullHandle(TailSlot_p->NextSlotInChain)) {
				TailSlot_p = stptiHAL_GetObjectSlot_p(TailSlot_p->NextSlotInChain);
			}
			break;
		}
		stptiOBJMAN_NextInList(&vDevice_p->PidIndexes, (void *)&HeadSlot_p, &PidIndex);
	}

	if (NoOtherSlotsWithThisPID) {
		/* No slots with this pid */

		/* Is there room in the pid table for this new pid?  If not grow the pid table */
		if (stptiOBJMAN_GetNumberOfItemsInList(&vDevice_p->PidIndexes) >=
		    (int)vDevice_p->TP_Interface.NumberOfSlots) {
			Error = stptiHAL_vDeviceGrowPIDTable(stptiOBJMAN_ObjectPointerToHandle(vDevice_p));
		}

		if (ST_NO_ERROR == Error) {
			Error =
			    stptiOBJMAN_AddToList(&vDevice_p->PidIndexes, SlotToBeChained_p, FALSE,
						  &SlotToBeChained_p->PidIndex);
			if (ST_NO_ERROR == Error) {
				if (SlotToBeChained_p->PidIndex >= (int)vDevice_p->TP_Interface.NumberOfSlots) {
					stptiOBJMAN_RemoveFromList(&vDevice_p->PidIndexes, SlotToBeChained_p->PidIndex);
					Error = ST_ERROR_NO_MEMORY;
				}
			}
		}
	}

	if (ST_NO_ERROR == Error) {
		/* This slot is going to be the new tail (should be 0xFFFF already, so unnecessary but defensive) */
		stptiHAL_pDeviceXP70Write(&vDevice_p->TP_Interface.SlotInfo_p[SlotToBeChained_p->SlotIndex].next_slot,
					  0xFFFF);

		if (NoOtherSlotsWithThisPID) {
			/* No previous slots, so this is the only slot with this pid.  Define it as the head slot. */
			HeadSlot_p = SlotToBeChained_p;

			/* If it is a WildCardPID we need to reference it */
			if (NewPid == stptiHAL_WildCardPID) {
				stptiHAL_pDeviceXP70Write(&vDevice_p->TP_Interface.vDeviceInfo_p->wildcard_slot_index,
							  (U16) SlotToBeChained_p->SlotIndex);
			}
		} else {
			/* There are one or more slots with this pid so we add to the tail of the chain */
			TailSlot_p->NextSlotInChain = stptiOBJMAN_ObjectPointerToHandle(SlotToBeChained_p);
			stptiHAL_pDeviceXP70Write(&vDevice_p->TP_Interface.SlotInfo_p[TailSlot_p->SlotIndex].next_slot,
						  SlotToBeChained_p->SlotIndex);
			SlotToBeChained_p->PreviousSlotInChain = stptiOBJMAN_ObjectPointerToHandle(TailSlot_p);
			SlotToBeChained_p->PidIndex = HeadSlot_p->PidIndex;
		}

		if (SlotToBeChained_p->SecondarySlotType == stptiHAL_SECONDARY_TYPE_PRIM) {
			/* Lookup secondary slot */
			stptiHAL_Slot_t *SecondarySlot_p = stptiHAL_GetObjectSlot_p(SlotToBeChained_p->SecondaryHandle);
			if (SecondarySlot_p != NULL) {
				volatile stptiTP_SlotInfo_t *SecondarySlotInfo_p =
				    &vDevice_p->TP_Interface.SlotInfo_p[SecondarySlot_p->SlotIndex];
				stptiHAL_pDeviceXP70Write(&SecondarySlotInfo_p->secondary_pid_info,
							  SECONDARY_PID_SECONDARY_SLOT | NewPid);
			}
		}

		if (SlotToBeChained_p->SecondarySlotType == stptiHAL_SECONDARY_TYPE_SEC) {
			/* Lookup primary slot */
			stptiHAL_Slot_t *PrimarySlot_p = stptiHAL_GetObjectSlot_p(SlotToBeChained_p->SecondaryHandle);
			if (PrimarySlot_p != NULL) {
				volatile stptiTP_SlotInfo_t *SecondarySlotInfo_p =
				    &vDevice_p->TP_Interface.SlotInfo_p[SlotToBeChained_p->SlotIndex];
				stptiHAL_pDeviceXP70Write(&SecondarySlotInfo_p->secondary_pid_info,
							  SECONDARY_PID_SECONDARY_SLOT | PrimarySlot_p->PID);
			}
		}

		if (NoOtherSlotsWithThisPID) {
			/* Set the pid index to point to the head slot */
			stptiHAL_pDeviceXP70Write(&vDevice_p->TP_Interface.
						  PIDSlotMappingTable_p[SlotToBeChained_p->PidIndex],
						  (U16) (HeadSlot_p->SlotIndex));

			/* Do last to make sure chain is setup before you alter the actual pid table */
			stptiHAL_pDeviceXP70Write(&vDevice_p->TP_Interface.PIDTable_p[SlotToBeChained_p->PidIndex],
						  NewPid);
		}
	}

	return (Error);
}

/**
   @brief  Set the PID for a given single slot.

   This function does the work of setting a PID.  It is called by stptiHAL_SlotSetPID (which handles
   slots that have associations, and so need to be changed en mass).

   @param  SlotHandle     A handle to the slot to be changed.
   @param  Pid            The Packet ID or stptiHAL_InvalidPID if to be "unset".

   @return                A standard st error type...
                          - ST_NO_ERROR
 */
static ST_ErrorCode_t stptiHAL_SlotSetPIDWorkerFn(FullHandle_t SlotHandle, U16 Pid, BOOL SuppressReset)
{
	/* ERROR HANDLING MUST BE VERY CAREFULLY CONTROLLED IN THIS FUNCTION!!!
	   If you are not careful you can leave the chain in a broken state.  Before this function is
	   run, checks should have been done to make sure that no errors should occur during execution.

	   The complexity is caused by the requirement to modify the slot chain whilst the slots are in
	   use.  You have to be very careful in the order you do things.  Dealing with all the errors
	   as they occur could increase the complexity to unmaintainable levels. */

	ST_ErrorCode_t Error = ST_NO_ERROR;

	stptiHAL_vDevice_t *vDevice_p = stptiHAL_GetObjectvDevice_p(SlotHandle);
	stptiHAL_Slot_t *Slot_p = stptiHAL_GetObjectSlot_p(SlotHandle);

	U16 OriginalPID = Slot_p->PID;

	/* Unchain it if the PID was already in use */
	if (OriginalPID != stptiHAL_InvalidPID) {
		/* If the PID is already in use, we must make sure that this pid is not matched in case it
		   is at the top of the pid table (the head of the chain) and it would prevent other slots
		   in the chain from running when it is unchained. */
		stptiHAL_SlotUnchainClearPID(vDevice_p, Slot_p);
	}

	/* Need to look for a frontend on this vDevice.  If it is "live" (TSIN) then we need
	   to update the PID table for it. */
	if (stptiHAL_StreamIDNone != vDevice_p->StreamID && OriginalPID != Pid) {
		/* If changing the PID, stop the current PID being filtered (function checks no other slots are requesting it) */
		if (stptiHAL_InvalidPID != OriginalPID) {
			(void)stptiTSHAL_call(TSInput.TSHAL_TSInputSetClearPid, 0,
					      stptiOBJMAN_GetpDeviceIndex(SlotHandle), vDevice_p->StreamID, OriginalPID,
					      FALSE);
			/* Ignore the error as it could return error if we are clearing a pid on a TSInput that has not yet been configured */
		}
		if (stptiHAL_InvalidPID != Pid) {
			/* Enable the new PID */
			(void)stptiTSHAL_call(TSInput.TSHAL_TSInputSetClearPid, 0,
					      stptiOBJMAN_GetpDeviceIndex(SlotHandle), vDevice_p->StreamID, Pid, TRUE);
			/* Ignore the error as it could return error if we are setting a pid on a TSInput that has not yet been configured */
		}
	}

	/* If the slot is active (not an invalid PID) then chain it. */
	if (Pid != stptiHAL_InvalidPID) {
		/* Now chain the slot.  As we are committed to chaining to keep the slot chain integral, we
		   must always call stptiHAL_SlotChainSetPID() even on TSInput errors. */
		Error = stptiHAL_SlotChainSetPID(vDevice_p, Slot_p, Pid);
		if (ST_NO_ERROR == Error) {
			stptiHAL_SlotActivate(SlotHandle);
		} else {
			Pid = stptiHAL_InvalidPID;
		}
	} else {		/* an InvalidPID - i.e disabling the slot */

		/* Obscure usage - sometimes people want to clear the pid but maintain the state */
		if (!SuppressReset) {
			/* If clearing the PID we also reset the slot so that it can be freely used again */
			stptiHAL_SlotResetState(&SlotHandle, 1, TRUE);	/* TRUE means keep disabled (pid invalidated) */
		}
	}

	Slot_p->PID = Pid;

	return (Error);
}

/**
   @brief  Initialise a SlotInfo entry in the Transport Processor

   This function initialises a specified entry in the Transport Processors SlotInfo table.

   @param  SlotInfo_p       Pointer to the slot info entry to be initialised.
   @param  NumberOfEntries  Number of Entries to clear.

   @return                  A standard st error type...
                            - ST_NO_ERROR
 */
void stptiHAL_SlotTPEntryInitialise(volatile stptiTP_SlotInfo_t * SlotInfo_p)
{
	if (SlotInfo_p != NULL) {
		/* Initialise SlotInfo */
		stptiHAL_pDeviceXP70Memset((void *)SlotInfo_p, 0, sizeof(stptiTP_SlotInfo_t));

		stptiHAL_pDeviceXP70Write(&SlotInfo_p->slot_state,
					  STATUS_SLOT_STATE_CLEAR | STATUS_TS_LEVEL_SC_NOT_SYNCED);
		stptiHAL_pDeviceXP70Write(&SlotInfo_p->next_slot, 0xFFFF);
		stptiHAL_pDeviceXP70Write(&SlotInfo_p->dma_record, 0xFFFF);
		stptiHAL_pDeviceXP70Write(&SlotInfo_p->indexer, 0xFFFF);

		/* The slot's event_mask is used to gate events, we allow (enable) all events which are not
		   slot based events so that they are not prevented from being raised */
		stptiHAL_pDeviceXP70Write(&SlotInfo_p->event_mask,
					  EVENT_MASK_ALL_EVENTS & (~EVENT_MASK_SLOT_ENABLED_EVENTS));
	}
}

/**
   @brief  Initialise the PID Table in the Transport Processor

   This function initialises the Transport Processors PID table / PID Mapping Table. To a known
   state.

   @param  PIDTable_p             Pointer to the base of the PID table to be initialised.
   @param  PIDSlotMappingTable_p  Pointer to the base of the PID Slot Mapping table to be
                                  initialised.
   @param  NumberOfEntries        Size of the tables.

   @return                        A standard st error type...
                                  - ST_NO_ERROR
 */
void stptiHAL_SlotPIDTableInitialise(volatile U16 *PIDTable_p, volatile U16 *PIDSlotMappingTable_p,
				     int NumberOfEntries)
{
	int i;
	for (i = 0; i < NumberOfEntries; i++) {
		/* Initialise PIDTable Entry */
		stptiHAL_pDeviceXP70Write(&PIDTable_p[i], stptiHAL_InvalidPID);

		/* Initialise corresponding PIDSlotMappingTable Entry */
		stptiHAL_pDeviceXP70Write(&PIDSlotMappingTable_p[i], 0xFFFF);
	}
}

/**
   @brief  Search for the slots which match a specified PID.

   This function returns the slots which match a specified PID.

   @param  vDeviceHandle        The handle of the vDevice to check.
   @param  PIDtoFind            The PID to search for
   @param  SlotArray            A pointer to array of FullHandles (the matching slots to be returned)
   @param  SlotArraySize        The size of the array of FullHandles

   @return                      The number of Slots found with this PID.

 */
int stptiHAL_SlotLookupPID(FullHandle_t vDeviceHandle, U16 PIDtoFind, FullHandle_t *SlotArray, int SlotArraySize)
{
	stptiHAL_vDevice_t *vDevice_p = stptiHAL_GetObjectvDevice_p(vDeviceHandle);
	stptiHAL_Slot_t *Slot_p;
	int index, SlotsFound = 0;

	stptiOBJMAN_FirstInList(&vDevice_p->PidIndexes, (void *)&Slot_p, &index);
	while (index >= 0 && SlotsFound < SlotArraySize) {
		if (Slot_p->PID == PIDtoFind) {
			/* Slots with the same PID are chained together, so we need to follow the chain */
			do {
				SlotArray[SlotsFound++] = stptiOBJMAN_ObjectPointerToHandle(Slot_p);
				Slot_p = stptiHAL_GetObjectSlot_p(Slot_p->NextSlotInChain);
			} while (Slot_p != NULL && SlotsFound < SlotArraySize);
		}
		stptiOBJMAN_NextInList(&vDevice_p->PidIndexes, (void *)&Slot_p, &index);
	}

	return (SlotsFound);
}

/**
   @brief  Search for the Pids set on a vdevice.

   This function returns the Pids on a vDevice

   @param  vDeviceHandle        The handle of the vDevice to check.
   @param  PidArray_p           A pointer to array of Pid
   @param  PidArraySize         The size of the array of Pids

   @return                      The number of Pids
 */
U16 stptiHAL_LookupPIDsOnSlots(FullHandle_t vDeviceHandle, U16 *PIDArray_p, U16 PIDArraySize)
{
	stptiHAL_vDevice_t *vDevice_p = stptiHAL_GetObjectvDevice_p(vDeviceHandle);
	stptiHAL_Slot_t *Slot_p;
	int index, SlotsFound = 0;

	stptiOBJMAN_FirstInList(&vDevice_p->PidIndexes, (void *)&Slot_p, &index);
	while (index >= 0 && (SlotsFound != PIDArraySize)) {
		if (Slot_p->PID != stptiHAL_InvalidPID) {
			PIDArray_p[SlotsFound++] = Slot_p->PID;
		}
		stptiOBJMAN_NextInList(&vDevice_p->PidIndexes, (void *)&Slot_p, &index);
	}

	return (SlotsFound);
}
