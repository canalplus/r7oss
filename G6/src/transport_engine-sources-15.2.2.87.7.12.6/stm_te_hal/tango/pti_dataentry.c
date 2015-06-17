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
   @file   pti_dataentry.c
   @brief  data entry Object Initialisation, Termination and Manipulation Functions.

   This file implements the functions for creating, destroying and accessing PTI data entries.

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
#include "pti_dataentry.h"
#include "pti_buffer.h"
#include "pti_filter.h"
#include "pti_slot.h"

/* MACROS ------------------------------------------------------------------ */

/* Private Constants ------------------------------------------------------- */

#define EXTRA_TAG_LENGTH (8) /* Warning these value must match the TP data entry buffer size in data_entry_buffer_t i.e. pkt_t * 2 */
#define RAW_TS_PKT_SIZE  (192)
#define DATA_ENTRY_TP_BUFFER_SIZE_1TS (EXTRA_TAG_LENGTH+RAW_TS_PKT_SIZE)

/* Private Variables ------------------------------------------------------- */
/* Private Function Prototypes --------------------------------------------- */
ST_ErrorCode_t stptiHAL_DataEntryAllocator(FullHandle_t DataEntryHandle, void *params_p);
ST_ErrorCode_t stptiHAL_DataEntryAssociator(FullHandle_t DataEntryHandle, FullHandle_t AssocObjectHandle);
ST_ErrorCode_t stptiHAL_DataEntryDisassociator(FullHandle_t DataEntryHandle, FullHandle_t AssocObjectHandle);
ST_ErrorCode_t stptiHAL_DataEntryDeallocator(FullHandle_t DataEntryHandle);

ST_ErrorCode_t stptiHAL_DataEntryConfigure(FullHandle_t DataEntryHandle, stptiHAL_DataEntryConfigParams_t * Params_p);

static ST_ErrorCode_t stptiHAL_DataEntryTPSync(FullHandle_t DataEntryHandle, U32 EntryClear);
stptiHAL_DataEntry_t *stptiHAL_DataEntryFindTail(FullHandle_t DataEntryHandle);
void stptiHAL_DataEntryAddToTail(FullHandle_t ListDataEntryHandle, FullHandle_t NewDataEntryHandle);
void stptiHAL_DataEntryRemoveFromList(FullHandle_t DataEntryHandle);
/* Public Constants -------------------------------------------------------- */

/* Export the API */
const stptiHAL_DataEntryAPI_t stptiHAL_DataEntryAPI = {
	{
		/* Allocator                 Associator                    Disassociator                    Deallocator */
		stptiHAL_DataEntryAllocator, stptiHAL_DataEntryAssociator, stptiHAL_DataEntryDisassociator, stptiHAL_DataEntryDeallocator,
		NULL, NULL
	},
	stptiHAL_DataEntryConfigure
};

/* Object Management Functions ------------------------------------------------------------------ */

/**
   @brief  Allocate this data entry

   This function allocates the data entry

   @param  DataEntryHandle      The handle of the data entry.
   @param  params_p             The parameters for this data entry.

   @return                      A standard st error type...
                                - ST_NO_ERROR
 */
ST_ErrorCode_t stptiHAL_DataEntryAllocator(FullHandle_t DataEntryHandle, void *params_p)
{
	/* Already write locked */
	ST_ErrorCode_t Error = ST_NO_ERROR;

	stptiHAL_DataEntryAllocateParams_t *Parameters_p = (stptiHAL_DataEntryAllocateParams_t *) params_p;

	stptiHAL_DataEntry_t *DataEntry_p = stptiHAL_GetObjectDataEntry_p(DataEntryHandle);
	stptiHAL_pDevice_t *pDevice_p = stptiHAL_GetObjectpDevice_p(DataEntryHandle);

	if (ST_NO_ERROR == Error) {
		DataEntry_p->CachedMemoryStructure.Dev = &pDevice_p->pdev->dev;
		DataEntry_p->Data_p =
		    (U8 *)
		    stptiSupport_MemoryAllocateForDMA((DATA_ENTRY_TP_BUFFER_SIZE_1TS * Parameters_p->NumTSPackets),
						      STPTI_SUPPORT_DCACHE_LINE_SIZE,
						      &DataEntry_p->CachedMemoryStructure, stptiSupport_ZONE_SMALL);
		if (DataEntry_p->Data_p == NULL) {
			Error = ST_ERROR_NO_MEMORY;
		} else {
			/* Initialise default parameters here */
			DataEntry_p->PreviousEntry_p = NULL;
			DataEntry_p->NextEntry_p = NULL;
			DataEntry_p->SrcData_p = NULL;
			DataEntry_p->RepeatCount = 1;
			DataEntry_p->FromByte = 0;
			DataEntry_p->NotifyEvent = FALSE;
			DataEntry_p->AllocSize = (DATA_ENTRY_TP_BUFFER_SIZE_1TS * Parameters_p->NumTSPackets);
		}
	}

	return (Error);
}

/**
   @brief  Assocate this data entry

   This function assocate the data entry, to a slot.

   @param  DataEntryHandle      The handle of the data entry.
   @param  AssocObjectHandle    The handle of the object to be associated.

   @return                      A standard st error type...
                                - ST_NO_ERROR
 */
ST_ErrorCode_t stptiHAL_DataEntryAssociator(FullHandle_t DataEntryHandle, FullHandle_t AssocObjectHandle)
{
	/* Already write locked */
	ST_ErrorCode_t Error = ST_NO_ERROR;
	stptiHAL_Slot_t *Slot_p = NULL;

	switch (stptiOBJMAN_GetObjectType(AssocObjectHandle)) {
	case OBJECT_TYPE_SLOT:
		{
			/* Allocate memory and copy the data and flush the cache to make sure its coherent with physical memory */
			stptiHAL_DataEntry_t *DataEntry_p = stptiHAL_GetObjectDataEntry_p(DataEntryHandle);

			if ((DataEntry_p->Data_p == NULL) || (DataEntry_p->SrcData_p == NULL)) {
				Error = ST_ERROR_NO_MEMORY;
			} else {
				if (DataEntry_p->DataSize <= (DATA_ENTRY_TP_BUFFER_SIZE_1TS * 2)) {
					U8 TagSize = 0;
					stptiHAL_vDevice_t *vDevice_p = stptiHAL_GetObjectvDevice_p(AssocObjectHandle);

					Slot_p = stptiHAL_GetObjectSlot_p(AssocObjectHandle);

					if (stptiHAL_pDeviceXP70Read
					    (&vDevice_p->TP_Interface.SlotInfo_p[Slot_p->SlotIndex].
					     slot_flags) & SLOT_FLAGS_PREFIX_DNLA_TS_TAG) {
						TagSize = 4;
					}

					/* if the size of the data entry is greater than one packet then we need to do two memcpys to allow a space for tag insertion */
					if ((DataEntry_p->DataSize > vDevice_p->PacketSize) && TagSize) {
						memcpy(DataEntry_p->Data_p, DataEntry_p->SrcData_p,
						       vDevice_p->PacketSize);

						memcpy(&DataEntry_p->Data_p[vDevice_p->PacketSize + TagSize],
						       &DataEntry_p->SrcData_p[vDevice_p->PacketSize],
						       (DataEntry_p->DataSize - vDevice_p->PacketSize));

						/* Important - adjust the memory size */
						DataEntry_p->DataSize += TagSize;
					} else {
						/* One copy required of just the packet size - TP will add the tag prefix */
						memcpy(DataEntry_p->Data_p, DataEntry_p->SrcData_p,
						       DataEntry_p->DataSize);
					}

					stptiSupport_FlushRegion(&DataEntry_p->CachedMemoryStructure,
								 DataEntry_p->Data_p, DataEntry_p->DataSize);

					/* Find the tail of the data entry list and if there is none install this entry as the head into the slot info */

					/* Is this a replacement slot or an insertion slot */
					if (Slot_p->DataEntryReplacement) {
						/* If the there is no head entry installed then install this one as the head */
						if (stptiOBJMAN_IsNullHandle(Slot_p->DataEntryHandle)) {
							Slot_p->DataEntryHandle = DataEntryHandle;

							/* This is the first entry handle associated with the slot so we have to configure the slot info in the TP */
							stptiHAL_pDeviceXP70Write(&vDevice_p->TP_Interface.SlotInfo_p[Slot_p->SlotIndex].u.raw_slot_info.DataEntry.EntryAddress,
										  (U32)stptiSupport_VirtToPhys(&DataEntry_p->CachedMemoryStructure, DataEntry_p-> Data_p));
							stptiHAL_pDeviceXP70Write(&vDevice_p->TP_Interface.SlotInfo_p[Slot_p->SlotIndex].u.raw_slot_info.DataEntry.EntrySize,
										  DataEntry_p->DataSize);
							stptiHAL_pDeviceXP70Write(&vDevice_p->TP_Interface.SlotInfo_p[Slot_p->SlotIndex].u.raw_slot_info.DataEntry.EntryFromByte,
										  DataEntry_p->FromByte);
							stptiHAL_pDeviceXP70Write(&vDevice_p->TP_Interface.SlotInfo_p[Slot_p->SlotIndex].u.raw_slot_info.DataEntry.EntryCount,
										  DataEntry_p->RepeatCount);

							/* Always set the EntryState as the last operation */
							stptiHAL_pDeviceXP70Write(&vDevice_p->TP_Interface.SlotInfo_p[Slot_p->SlotIndex].u.raw_slot_info.DataEntry.EntryState,
										  DATA_ENTRY_STATE_LOADED);
						} else {
							/* Else add the new Entry onto the end of the list */
							stptiHAL_DataEntryAddToTail(Slot_p->DataEntryHandle,
										    DataEntryHandle);
						}
					} else if (Slot_p->DataEntryInsertion) {
						if (stptiOBJMAN_IsNullHandle(Slot_p->DataEntryHandle)) {
							/* Is there no entry loaded on this vdevice then is ok to install */
							if (stptiHAL_pDeviceXP70Read
							    (&vDevice_p->TP_Interface.vDeviceInfo_p->DataEntry.EntryState) == DATA_ENTRY_STATE_EMPTY) {
								Slot_p->DataEntryHandle = DataEntryHandle;

								/* This is the first entry handle associated with the slot so we have to configure the slot info in the TP */
								stptiHAL_pDeviceXP70Write(&vDevice_p->TP_Interface.vDeviceInfo_p->DataEntry.EntryAddress,
											  (U32)stptiSupport_VirtToPhys(&DataEntry_p->CachedMemoryStructure,DataEntry_p->Data_p));
								stptiHAL_pDeviceXP70Write(&vDevice_p->TP_Interface.vDeviceInfo_p->DataEntry.EntrySize,
											  DataEntry_p->DataSize);
								stptiHAL_pDeviceXP70Write(&vDevice_p->TP_Interface.vDeviceInfo_p->DataEntry.EntrySlotIndex,
											  Slot_p->SlotIndex);
								stptiHAL_pDeviceXP70Write(&vDevice_p->TP_Interface.vDeviceInfo_p->DataEntry.EntryFromByte, 0);
								stptiHAL_pDeviceXP70Write(&vDevice_p->TP_Interface.vDeviceInfo_p->DataEntry.EntryCount,
											  DataEntry_p->RepeatCount);

								/* Always set the EntryState as the last operation */
								stptiHAL_pDeviceXP70Write(&vDevice_p->TP_Interface.vDeviceInfo_p->DataEntry.EntryState,
											  DATA_ENTRY_STATE_LOADED);
							} else {
								Error = STPTI_ERROR_DATA_ENTRY_ALREADY_LOADED;
							}
						} else {
							/* Add the new Entry onto the end of the list */
							stptiHAL_DataEntryAddToTail(Slot_p->DataEntryHandle,
										    DataEntryHandle);
						}
					} else {
						Error = STPTI_ERROR_INVALID_SLOT_HANDLE;
					}
				} else {
					Error = ST_ERROR_NO_MEMORY;
				}
			}
			break;
		}
	default:
		STPTI_PRINTF_ERROR("Data Entry cannot associate to this kind of object %d",
				   stptiOBJMAN_GetObjectType(AssocObjectHandle));
		Error = ST_ERROR_BAD_PARAMETER;
		break;
	}

	return (Error);
}

/**
   @brief  Disassociate this data entry from another object

   This function disassociates the data entry, from a slot.

   @param  DataEntryHandle      The handle of the data entry.
   @param  AssocObjectHandle    The handle of the object to be disassociated.

   @return                      A standard st error type...
                                - ST_NO_ERROR
 */
ST_ErrorCode_t stptiHAL_DataEntryDisassociator(FullHandle_t DataEntryHandle, FullHandle_t AssocObjectHandle)
{
	/* Already write locked */
	ST_ErrorCode_t Error = ST_NO_ERROR;
	stptiHAL_Slot_t *Slot_p = NULL;

	switch (stptiOBJMAN_GetObjectType(AssocObjectHandle)) {
	case OBJECT_TYPE_SLOT:
		{
			stptiHAL_DataEntry_t *DataEntry_p = stptiHAL_GetObjectDataEntry_p(DataEntryHandle);
			Slot_p = stptiHAL_GetObjectSlot_p(AssocObjectHandle);

			/* Remove the new Entry from the list - does not clear the pointers in this entry object as we need the information below. */
			stptiHAL_DataEntryRemoveFromList(DataEntryHandle);

			/* Is this Data Entry at the head of the chain? */
			if (Slot_p->DataEntryHandle.word == DataEntryHandle.word) {
				stptiHAL_vDevice_t *vDevice_p = stptiHAL_GetObjectvDevice_p(AssocObjectHandle);
				U32 EntryClear = 0;
				BOOL SkipRemove = FALSE;

				/* If we disassociate the head data entry and there is/are another previous entry/ries then we must install the next entry into the slot info ready for processing */
				if (Slot_p->DataEntryReplacement) {
					if (!(stptiHAL_pDeviceXP70Read(&vDevice_p->TP_Interface.SlotInfo_p[Slot_p->SlotIndex].u.raw_slot_info.DataEntry.EntryState) & DATA_ENTRY_STATE_COMPLETE)) {
						EntryClear = (((U32) Slot_p->SlotIndex) | SYNC_TP_ENTRY_CLEAR_SLOT);
					}
				} else
				    if (!(stptiHAL_pDeviceXP70Read(&vDevice_p->TP_Interface.vDeviceInfo_p->DataEntry.EntryState) & DATA_ENTRY_STATE_COMPLETE)) {
					EntryClear = ((U32)stptiOBJMAN_GetvDeviceIndex(AssocObjectHandle) |
					     SYNC_TP_ENTRY_CLEAR_VDEVICE);
				}

				/* Has the entry been processed or not? - if so clear */
				if (EntryClear) {
					stptiHAL_DataEntryTPSync(DataEntryHandle, EntryClear);	/* purposely ignore error */

					/* Make sure the TP doesn't process the entry if it has already started to */
					if (Slot_p->DataEntryReplacement) {
						if (stptiHAL_pDeviceXP70Read(&vDevice_p->TP_Interface.SlotInfo_p[Slot_p->SlotIndex].u.raw_slot_info.DataEntry.EntryState) != DATA_ENTRY_STATE_EMPTY) {
							/* TP is processing the packet so it will get removed in a short while anyway */
							SkipRemove = TRUE;
						}
					} else
					    if (stptiHAL_pDeviceXP70Read(&vDevice_p->TP_Interface.vDeviceInfo_p->DataEntry.EntryState) != DATA_ENTRY_STATE_EMPTY) {
						SkipRemove = TRUE;
					}
				}

				if (SkipRemove == FALSE) {
					/* Sync and clear the existing Data Entry from the slot info */
					if (DataEntry_p->PreviousEntry_p != NULL) {
						Slot_p->DataEntryHandle =
						    DataEntry_p->PreviousEntry_p->ObjectHeader.Handle;

						/* To reload we need to know if this is a replacement or insertion slot */
						if (Slot_p->DataEntryReplacement) {
							stptiHAL_pDeviceXP70Write(&vDevice_p->TP_Interface.SlotInfo_p[Slot_p->SlotIndex].u.raw_slot_info.DataEntry.EntryAddress,
										  (U32)stptiSupport_VirtToPhys(&DataEntry_p->PreviousEntry_p->CachedMemoryStructure,
													  DataEntry_p->PreviousEntry_p->Data_p));
							stptiHAL_pDeviceXP70Write(&vDevice_p->TP_Interface.SlotInfo_p[Slot_p->SlotIndex].u.raw_slot_info.DataEntry.EntrySize,
										  DataEntry_p->PreviousEntry_p->DataSize);
							stptiHAL_pDeviceXP70Write(&vDevice_p->TP_Interface.SlotInfo_p[Slot_p->SlotIndex].u.raw_slot_info.DataEntry.EntryFromByte,
										  DataEntry_p->PreviousEntry_p->FromByte);
							stptiHAL_pDeviceXP70Write(&vDevice_p->TP_Interface.SlotInfo_p[Slot_p->SlotIndex].u.raw_slot_info.DataEntry.EntrySlotIndex, 0xFFFF); /* Not used in the TP on replacement slots */
							stptiHAL_pDeviceXP70Write(&vDevice_p->TP_Interface.SlotInfo_p[Slot_p->SlotIndex].u.raw_slot_info.DataEntry.EntryCount,
										  DataEntry_p->PreviousEntry_p->RepeatCount);

							/* Always set the EntryState as the last operation to prevent */
							stptiHAL_pDeviceXP70Write(&vDevice_p->TP_Interface.SlotInfo_p[Slot_p->SlotIndex].u.raw_slot_info.DataEntry.EntryState,
										  DATA_ENTRY_STATE_LOADED);
						} else if (Slot_p->DataEntryInsertion) {
							stptiHAL_pDeviceXP70Write(&vDevice_p->TP_Interface.vDeviceInfo_p->DataEntry.EntryAddress,
										  (U32)stptiSupport_VirtToPhys(&DataEntry_p->PreviousEntry_p->CachedMemoryStructure,
													  DataEntry_p->PreviousEntry_p->Data_p));
							stptiHAL_pDeviceXP70Write(&vDevice_p->TP_Interface.vDeviceInfo_p->DataEntry.EntrySize,
										  DataEntry_p->PreviousEntry_p->DataSize);
							stptiHAL_pDeviceXP70Write(&vDevice_p->TP_Interface.vDeviceInfo_p->DataEntry.EntrySlotIndex,
										  Slot_p->SlotIndex);
							stptiHAL_pDeviceXP70Write(&vDevice_p->TP_Interface.vDeviceInfo_p->DataEntry.EntryFromByte, 0);
							stptiHAL_pDeviceXP70Write(&vDevice_p->TP_Interface.vDeviceInfo_p->DataEntry.EntryCount,
										  DataEntry_p->PreviousEntry_p->RepeatCount);

							/* Always set the EntryState as the last operation */
							stptiHAL_pDeviceXP70Write(&vDevice_p->TP_Interface.vDeviceInfo_p->DataEntry.EntryState,
										  DATA_ENTRY_STATE_LOADED);
						}
					} else {
						/* No Entries left to process so write a null handle into the slot object */
						Slot_p->DataEntryHandle = stptiOBJMAN_NullHandle;

						if (Slot_p->DataEntryReplacement) {
							stptiHAL_pDeviceXP70Write(&vDevice_p->TP_Interface.SlotInfo_p[Slot_p->SlotIndex].u.raw_slot_info.DataEntry.EntryAddress, 0);
							stptiHAL_pDeviceXP70Write(&vDevice_p->TP_Interface.SlotInfo_p[Slot_p->SlotIndex].u.raw_slot_info.DataEntry.EntrySize, 0);
							stptiHAL_pDeviceXP70Write(&vDevice_p->TP_Interface.SlotInfo_p[Slot_p->SlotIndex].u.raw_slot_info.DataEntry.EntryFromByte, 0);
							stptiHAL_pDeviceXP70Write(&vDevice_p->TP_Interface.SlotInfo_p[Slot_p->SlotIndex].u.raw_slot_info.DataEntry.EntrySlotIndex, 0xFFFF);
							stptiHAL_pDeviceXP70Write(&vDevice_p->TP_Interface.SlotInfo_p[Slot_p->SlotIndex].u.raw_slot_info.DataEntry.EntryCount, 0);
							stptiHAL_pDeviceXP70Write(&vDevice_p->TP_Interface.SlotInfo_p[Slot_p->SlotIndex].u.raw_slot_info.DataEntry.EntryState,
										  DATA_ENTRY_STATE_EMPTY);
						} else if (Slot_p->DataEntryInsertion) {
							stptiHAL_pDeviceXP70Write(&vDevice_p->TP_Interface.vDeviceInfo_p->DataEntry.EntryAddress,0);
							stptiHAL_pDeviceXP70Write(&vDevice_p->TP_Interface.vDeviceInfo_p->DataEntry.EntrySize, 0);
							stptiHAL_pDeviceXP70Write(&vDevice_p->TP_Interface.vDeviceInfo_p->DataEntry.EntrySlotIndex, 0xFFFF);
							stptiHAL_pDeviceXP70Write(&vDevice_p->TP_Interface.vDeviceInfo_p->DataEntry.EntryFromByte, 0);
							stptiHAL_pDeviceXP70Write(&vDevice_p->TP_Interface.vDeviceInfo_p->DataEntry.EntryCount, 0);
							stptiHAL_pDeviceXP70Write(&vDevice_p->TP_Interface.vDeviceInfo_p->DataEntry.EntryState,
										  DATA_ENTRY_STATE_EMPTY);
						}
					}
				}
			} else {
				/* Not the head entry so just remove it from the list */
				stptiHAL_DataEntryRemoveFromList(DataEntryHandle);
			}
		}
		break;
	default:
		/* Allow disassociation, even from invalid types, else we create a clean up problem */
		STPTI_PRINTF_ERROR("Data Entry disassociating from invalid type %d.",
				   stptiOBJMAN_GetObjectType(AssocObjectHandle));
		break;
	}
	return (Error);
}

/**
   @brief  Deallocate this data entry

   This function deallocates this data entry

   @param  DataEntryHandle      The Handle of the data entry.

   @return                      A standard st error type...
                                - ST_NO_ERROR
 */
ST_ErrorCode_t stptiHAL_DataEntryDeallocator(FullHandle_t DataEntryHandle)
{
	/* Already write locked */
	stptiHAL_DataEntry_t *DataEntry_p = stptiHAL_GetObjectDataEntry_p(DataEntryHandle);

	/* Clean up the entry and free allocated memory */
	if (DataEntry_p->Data_p != NULL) {
		stptiSupport_MemoryDeallocateForDMA(&DataEntry_p->CachedMemoryStructure);
		DataEntry_p->Data_p = NULL;
		DataEntry_p->NextEntry_p = NULL;
		DataEntry_p->PreviousEntry_p = NULL;
	}

	return (ST_NO_ERROR);
}

/* Object HAL functions ------------------------------------------------------------------------- */

/**
   @brief  Configure this data entry

   This function configure this data entry, which is used prior to an associate/load

   @param  DataEntryHandle      The Handle of the data entry.

   @return                      A standard st error type...
                                - ST_NO_ERROR
 */
ST_ErrorCode_t stptiHAL_DataEntryConfigure(FullHandle_t DataEntryHandle, stptiHAL_DataEntryConfigParams_t * Params_p)
{
	ST_ErrorCode_t Error = ST_NO_ERROR;

	/* Already write locked */
	stptiHAL_DataEntry_t *DataEntry_p = stptiHAL_GetObjectDataEntry_p(DataEntryHandle);

	if (NULL != DataEntry_p) {
		if (DataEntry_p->AllocSize < Params_p->DataSize) {
			Error = ST_ERROR_NO_MEMORY;
		} else {
			DataEntry_p->SrcData_p = Params_p->Data_p;
			DataEntry_p->DataSize = Params_p->DataSize;
			DataEntry_p->FromByte = Params_p->FromByte;
			DataEntry_p->NotifyEvent = Params_p->NotifyEvent;
			if (Params_p->RepeatCount == 0) {
				DataEntry_p->RepeatCount = TP_DATA_ENTRY_REPEAT_FOREVER;
			} else {
				DataEntry_p->RepeatCount = Params_p->RepeatCount;
			}
		}
	}

	return Error;
}

/* Supporting Functions ------------------------------------------------------------------------- */

static ST_ErrorCode_t stptiHAL_DataEntryTPSync(FullHandle_t DataEntryHandle, U32 EntryClear)
{
	ST_ErrorCode_t Error = ST_NO_ERROR;
	stptiHAL_vDevice_t *vDevice_p = stptiHAL_GetObjectvDevice_p(DataEntryHandle);

	/* Make sure we have no TP Data Entries pending */
	stptiSupport_SubMilliSecondWait(stptiHAL_pDeviceXP70Read(&vDevice_p->TP_Interface.pDeviceInfo_p->SyncTPClearEntry) !=SYNC_TP_ENTRY_DONE, &Error);

	if (Error != ST_ERROR_TIMEOUT) {
		stptiHAL_pDeviceXP70Write(&vDevice_p->TP_Interface.pDeviceInfo_p->SyncTPClearEntry, EntryClear);
		stptiSupport_SubMilliSecondWait(stptiHAL_pDeviceXP70Read(&vDevice_p->TP_Interface.pDeviceInfo_p->SyncTPClearEntry) !=
						SYNC_TP_ENTRY_DONE, &Error);
	}

	stptiHAL_pDeviceXP70Write(&vDevice_p->TP_Interface.pDeviceInfo_p->SyncTPClearEntry, SYNC_TP_ENTRY_DONE); /* force it done in case of error */

	return (Error);
}

stptiHAL_DataEntry_t *stptiHAL_DataEntryFindTail(FullHandle_t DataEntryHandle)
{
	stptiHAL_DataEntry_t *DataEntry_p = stptiHAL_GetObjectDataEntry_p(DataEntryHandle);

	while (DataEntry_p->PreviousEntry_p != NULL) {
		DataEntry_p = DataEntry_p->PreviousEntry_p;
	}

	return DataEntry_p;
}

void stptiHAL_DataEntryAddToTail(FullHandle_t ListDataEntryHandle, FullHandle_t NewDataEntryHandle)
{
	stptiHAL_DataEntry_t *ListDataEntry_p = stptiHAL_GetObjectDataEntry_p(ListDataEntryHandle);
	stptiHAL_DataEntry_t *NewDataEntry_p = stptiHAL_GetObjectDataEntry_p(NewDataEntryHandle);

	while (ListDataEntry_p->PreviousEntry_p != NULL) {
		ListDataEntry_p = ListDataEntry_p->PreviousEntry_p;
	}

	ListDataEntry_p->PreviousEntry_p = NewDataEntry_p;

	NewDataEntry_p->NextEntry_p = ListDataEntry_p;
	NewDataEntry_p->PreviousEntry_p = NULL;
}

void stptiHAL_DataEntryRemoveFromList(FullHandle_t DataEntryHandle)
{
	stptiHAL_DataEntry_t *RemovalDataEntry_p = stptiHAL_GetObjectDataEntry_p(DataEntryHandle);

	if (RemovalDataEntry_p->PreviousEntry_p != NULL) {
		RemovalDataEntry_p->PreviousEntry_p->NextEntry_p = RemovalDataEntry_p->NextEntry_p;
	}

	if (RemovalDataEntry_p->NextEntry_p != NULL) {
		RemovalDataEntry_p->NextEntry_p->PreviousEntry_p = RemovalDataEntry_p->PreviousEntry_p;
	}
}
