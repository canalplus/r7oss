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
   @file   pti_evt.c
   @brief  Code for the processing status blocks

*/

/* Includes ---------------------------------------------------------------- */

#if 0
#define STPTI_PRINT
#endif

/* ANSI C includes */

/* Includes from API level */
#include "../pti_driver.h"
#include "../pti_osal.h"
#include "../pti_debug.h"
#include "../pti_hal_api.h"
#include "../pti_tshal_api.h"

/* Includes from the HAL / ObjMan level */
#include "pti_pdevice.h"
#include "pti_vdevice.h"
#include "pti_slot.h"
#include "pti_buffer.h"
#include "pti_dataentry.h"
#include "pti_evt.h"

/* MACROS ------------------------------------------------------------------ */

/* Private Constants ------------------------------------------------------- */
#define EVENT_QUEUE_SIZE        (128)

#define EVENT_TASK_QUIT_PATTERN (0xffffffff)

/* Private Variables ------------------------------------------------------- */
/* Private Function Prototypes --------------------------------------------- */
static void stptiHAL_ProcessEvent(stptiTP_StatusBlk_Flags_t ActiveStatusBlkFlags, FullHandle_t SlotHandle,
				  FullHandle_t DataEntryEventSlotHandle, FullHandle_t BufferHandle,
				  stptiTP_StatusBlk_t *StatBlk_p, stptiHAL_EventFuncPtrs_t *EventFunctionPtrs_p,
				  U32 PCRLsw, U32 PCRBit32, U16 PCRExt, U32 ArrivalLsw, U32 ArrivalBit32,
				  U16 ArrivalExt);

static void stptiHAL_ProcessDataEntryEvent(FullHandle_t SlotHandle, stptiHAL_EventFuncPtrs_t *EventFunctionPtrs_p);

static void stptiHAL_ProcessEventTimestamps(stptiTP_StatusBlk_t *StatBlk_p, U32 *PCRLsw_p, U32 *PCRBit32_p,
					    U16 *PCRExt_p, U32 *ArrivalLsw_p, U32 *ArrivalBit32_p,
					    U16 *ArrivalExt_p);

/**
    @brief  Signal the event task for termination.

    @param  pDevice_p            A Pointer the the Tango pdevice structure

    @return                      A standard st error type...
                                 - ST_ERROR_NO_MEMORY   (failed to create task)
*/
ST_ErrorCode_t stptiHAL_EventTaskAbort(FullHandle_t pDeviceHandle)
{
	ST_ErrorCode_t Error = ST_NO_ERROR;
	stptiSupport_AccessLockState_t LocalLockState = stptiSupport_UNLOCKED;

	stptiSupport_MessageQueue_t *EventQueue_p = NULL;

	stptiOBJMAN_WriteLock(pDeviceHandle, &LocalLockState);
	{
		stptiHAL_pDevice_t *pDevice_p = stptiHAL_GetObjectpDevice_p(pDeviceHandle);
		EventQueue_p = pDevice_p->EventQueue_p;
	}
	stptiOBJMAN_Unlock(pDeviceHandle, &LocalLockState);

	if (NULL != EventQueue_p) {
		stptiTP_StatusBlk_t StatBlkData;

		/* consume any pending events (must be outside of write lock!) */
		while (stptiSupport_MessageQueueReceiveTimeout(EventQueue_p, &StatBlkData, 0)) ;

		StatBlkData.Flags = EVENT_TASK_QUIT_PATTERN;
		stptiSupport_MessageQueuePostTimeout(EventQueue_p, &StatBlkData, 10);
	}

	return (Error);
}

/**
    @brief  Task for event receiption and notification.

    This task waits on the event message queue to process status blocks for notification
    to the API users.

    @param  Param                A pointer the the pDevice structure

    @return                      None
*/
ST_ErrorCode_t stptiHAL_EventTask(FullHandle_t pDeviceHandle)
{
	stptiSupport_AccessLockState_t LocalLockState = stptiSupport_UNLOCKED;
	stptiHAL_pDevice_t *pDevice_p = stptiHAL_GetObjectpDevice_p(pDeviceHandle);
	stptiTP_StatusBlk_t StatBlk;

	stptiSupport_MessageQueue_t *EventQueue_p;

	STPTI_PRINTF("Entering Event Task for %08x", (unsigned)pDeviceHandle.word);

	/* Create a msg queue for holding complete status blocks */
	EventQueue_p = stptiSupport_MessageQueueCreate(sizeof(stptiTP_StatusBlk_t), EVENT_QUEUE_SIZE);
	if (NULL == EventQueue_p) {
		return (ST_ERROR_NO_MEMORY);
	}

	stptiOBJMAN_WriteLock(pDeviceHandle, &LocalLockState);
	{
		pDevice_p->EventQueue_p = EventQueue_p;
	}
	stptiOBJMAN_Unlock(pDeviceHandle, &LocalLockState);

	/* Main task loop */
	while (TRUE) {
		FullHandle_t EventSlotHandle = stptiOBJMAN_NullHandle;
		FullHandle_t DataEntryEventSlotHandle = stptiOBJMAN_NullHandle;
		FullHandle_t EventBufferHandle = stptiOBJMAN_NullHandle;

		FullHandle_t vDeviceHandle = stptiOBJMAN_NullHandle;
		stptiHAL_vDevice_t *vDevice_p = NULL;

		stptiHAL_EventFuncPtrs_t EventFunctionPtrs;

		stptiTP_StatusBlk_Flags_t ActiveStatusBlkFlags;

		U32 PCRLsw = 0;
		U32 PCRBit32 = 0;
		U16 PCRExt = 0;
		U32 ArrivalLsw = 0;
		U32 ArrivalBit32 = 0;
		U16 ArrivalExt = 0;

		memset(&EventFunctionPtrs, 0, sizeof(EventFunctionPtrs));

		/* This is done outside of a object lock, on the basis that a pDevice and it's event queue
		   won't be destroyed until this task is terminated. */

		stptiSupport_MessageQueueReceiveTimeout(pDevice_p->EventQueue_p, &StatBlk, -1);
		if (StatBlk.Flags == EVENT_TASK_QUIT_PATTERN) {
			break;
		}

		/* Only process status blocks (events) if the xp70 is running - else we will stall when
		   accessing the shared memory interface.  In practices this means we might loose events
		   when going into a power down state. */

		if (pDevice_p->TP_Status == stptiHAL_STXP70_RUNNING) {
			/* Process the status block here */
			stptiOBJMAN_WriteLock(pDeviceHandle, &LocalLockState);
			{
				vDeviceHandle.member.ObjectType = OBJECT_TYPE_VDEVICE;
				vDeviceHandle.member.vDevice = StatBlk.vDevice;
				vDeviceHandle.member.pDevice = pDeviceHandle.member.pDevice;
				vDevice_p = stptiHAL_GetObjectvDevice_p(vDeviceHandle);

				if (StatBlk.Flags == 0) {
					stpti_printf("Invalid status block detected");
				} else {
					stpti_printf("Status block received. Flags 0x%08x", StatBlk.Flags);
					stpti_printf("Status block received. Slot    %u", StatBlk.SlotIndex);
					stpti_printf("Status block received. Pcr0  0x%x", StatBlk.Pcr0);
					stpti_printf("Status block received. Pcr1  0x%x", StatBlk.Pcr1);
					stpti_printf("Status block received. ArrivalTime0   0x%x",
						     StatBlk.ArrivalTime0);
					stpti_printf("Status block received. ArrivalTime1   0x%x",
						     StatBlk.ArrivalTime1);

					if (NULL != vDevice_p) {
						/* Compute the slot handle for the event data */
						if (StatBlk.SlotIndex != 0xFFFF) {
							int SlotIndex = StatBlk.SlotIndex;

							EventSlotHandle = stptiOBJMAN_NullHandle;
							if (SlotIndex <
							    stptiOBJMAN_GetListCapacity(vDevice_p->SlotHandles)) {
								stptiHAL_Slot_t *Slot_p =
								    (stptiHAL_Slot_t *)
								    stptiOBJMAN_GetItemFromList(vDevice_p->SlotHandles,
												SlotIndex);
								if (NULL != Slot_p) {
									EventSlotHandle = Slot_p->ObjectHeader.Handle;
								}
							}
						}

						/* Compute the data entry slot handle for the event data */
						if (StatBlk.DataEntrySlotIndex != 0xFFFF) {
							int SlotIndex = StatBlk.DataEntrySlotIndex;

							DataEntryEventSlotHandle = stptiOBJMAN_NullHandle;
							if (SlotIndex <
							    stptiOBJMAN_GetListCapacity(vDevice_p->SlotHandles)) {
								stptiHAL_Slot_t *Slot_p =
								    (stptiHAL_Slot_t *)
								    stptiOBJMAN_GetItemFromList(vDevice_p->SlotHandles,
												SlotIndex);
								if (NULL != Slot_p) {
									DataEntryEventSlotHandle =
									    Slot_p->ObjectHeader.Handle;
								}
							}

						}
						stpti_printf("StatBlk.DataEntrySlotIndex %d",
							     StatBlk.DataEntrySlotIndex);

						/* Compute the buffer handle for the event data */
						if (StatBlk.DMAIndex != 0xFFFF) {
							if (StatBlk.DMAIndex <
							    stptiOBJMAN_GetListCapacity(vDevice_p->BufferHandles)) {
								stptiHAL_Buffer_t *Buffer_p =(stptiHAL_Buffer_t *)
								    stptiOBJMAN_GetItemFromList(vDevice_p->BufferHandles,
												StatBlk.DMAIndex);
								if (NULL != Buffer_p) {
									EventBufferHandle =
									    Buffer_p->ObjectHeader.Handle;
								}
							}
						}

						/* (Potential optimisation)  This copies 12 words for every event */
						memcpy(&EventFunctionPtrs, &vDevice_p->EventPtrs,
						       sizeof(stptiHAL_EventFuncPtrs_t));

						/* Process the timestamps */
						stptiHAL_ProcessEventTimestamps(&StatBlk, &PCRLsw, &PCRBit32, &PCRExt,
										&ArrivalLsw, &ArrivalBit32,
										&ArrivalExt);
					}
				}
			}
			stptiOBJMAN_Unlock(pDeviceHandle, &LocalLockState);

			/* Find out which event bits are actually events */
			ActiveStatusBlkFlags = StatBlk.Flags & EVENT_MASK_ALL_EVENTS;

			/* This is done outside of a object lock as the ProcessEvent call below results in callbacks */
			if (ActiveStatusBlkFlags && !stptiOBJMAN_IsNullHandle(EventSlotHandle)) {
				stptiHAL_ProcessEvent(ActiveStatusBlkFlags, EventSlotHandle, DataEntryEventSlotHandle,
						      EventBufferHandle, &StatBlk, &EventFunctionPtrs, PCRLsw, PCRBit32,
						      PCRExt, ArrivalLsw, ArrivalBit32, ArrivalExt);
			}
		}
	}

	stptiSupport_MessageQueueDelete(pDevice_p->EventQueue_p);
	pDevice_p->EventQueue_p = NULL;

	STPTI_PRINTF("Leaving Event Task for %08x", (unsigned)pDeviceHandle.word);

	return (ST_NO_ERROR);
}

/**
    @brief Process status block events

    Function to process and notify events to the API users.

    @param  SlotHandle           Slothandle
    @param  BufferHandle         BufferHandle
    @param  StatBlk_p            A pointer to the stat block

    @return                      None
*/
static void stptiHAL_ProcessEvent(stptiTP_StatusBlk_Flags_t ActiveStatusBlkFlags, FullHandle_t SlotHandle,
				  FullHandle_t DataEntryEventSlotHandle, FullHandle_t BufferHandle,
				  stptiTP_StatusBlk_t *StatBlk_p, stptiHAL_EventFuncPtrs_t *EventFunctionPtrs_p,
				  U32 PCRLsw, U32 PCRBit32, U16 PCRExt, U32 ArrivalLsw, U32 ArrivalBit32,
				  U16 ArrivalExt)
{
	stptiSupport_AccessLockState_t LocalLockState = stptiSupport_UNLOCKED;

	if (STATUS_BLK_BUFFER_OVERFLOW & ActiveStatusBlkFlags) {
		stpti_printf("STATUS_BLK_BUFFER_OVERFLOW rx'ed");
		if (!stptiOBJMAN_IsNullHandle(BufferHandle)) {
			stpti_printf("buffer handle rx'ed %x", BufferHandle.word);
			if (EventFunctionPtrs_p->EventBufferOverflowNotify_p != NULL) {
				(*(EventFunctionPtrs_p->EventBufferOverflowNotify_p)) (SlotHandle, BufferHandle);
			}
		}
	}

	if (STATUS_BLK_PCR_RECEIVED & ActiveStatusBlkFlags) {
		stptiOBJMAN_WriteLock(SlotHandle, &LocalLockState);
		{
			stptiHAL_vDevice_t *vDevice_p = stptiHAL_GetObjectvDevice_p(SlotHandle);

			stpti_printf("STATUS_BLK_PCR_RECEIVED rx'ed");
			if (vDevice_p != NULL) {
				ST_ErrorCode_t Error;
				stptiTSHAL_TimerValue_t TimerValue = { 0, 0, 0, 0, 0 };
				BOOL IsTSIN = TRUE;

				/* GNBvd81759 - Create timestamp for software injected pcrs */

				/* Update the Timestamp */
				Error =
				    stptiTSHAL_call(TSInput.TSHAL_TSInputStfeGetTimer, 0, vDevice_p->StreamID,
						    &TimerValue, &IsTSIN);
				if (ST_NO_ERROR == Error && !IsTSIN) {	/* Only update if NOT a TSIN (otherwise the timestamp will already be valid) */
					ArrivalBit32 = TimerValue.Clk27MHzDiv300Bit32;
					ArrivalLsw = TimerValue.Clk27MHzDiv300Bit31to0;
					ArrivalExt = TimerValue.Clk27MHzModulus300;
				}
			}
		}
		stptiOBJMAN_Unlock(SlotHandle, &LocalLockState);

		if (EventFunctionPtrs_p->EventPCRNotify_p != NULL) {
			(*(EventFunctionPtrs_p->EventPCRNotify_p)) (SlotHandle, PCRLsw, PCRBit32, PCRExt, ArrivalLsw,
								    ArrivalBit32, ArrivalExt,
								    (BOOL)(StatBlk_p->Flags & STATUS_BLK_DISCONTINUITY_FLAG));
		}
	}

	if (STATUS_BLK_CC_ERROR & ActiveStatusBlkFlags) {
		stpti_printf("STATUS_BLK_CC_ERROR rx'ed");
		if (EventFunctionPtrs_p->EventCCErrorNotify_p != NULL) {
			(*(EventFunctionPtrs_p->EventCCErrorNotify_p)) (SlotHandle, StatBlk_p->ExpectedCC,
									StatBlk_p->ReceivedCC);
		}
	}

	if (STATUS_BLK_CLEAR_TOSCRAMBLE & ActiveStatusBlkFlags) {
		stpti_printf("STATUS_BLK_CLEAR_TOSCRAMBLE rx'ed");
		if (EventFunctionPtrs_p->EventClearToScramNotify_p != NULL) {
			(*(EventFunctionPtrs_p->EventClearToScramNotify_p)) (SlotHandle);
		}
	}

	if (STATUS_BLK_SCRAMBLE_TOCLEAR & ActiveStatusBlkFlags) {
		stpti_printf("STATUS_BLK_SCRAMBLE_TOCLEAR rx'ed");
		if (EventFunctionPtrs_p->EventScramToClearNotify_p != NULL) {
			(*(EventFunctionPtrs_p->EventScramToClearNotify_p)) (SlotHandle);
		}
	}

	if (STATUS_BLK_INVALID_PARAMETER & ActiveStatusBlkFlags) {
		stpti_printf("STATUS_BLK_INVALID_PARAMETER rx'ed");
		if (EventFunctionPtrs_p->EventInvalidParameterNotify_p != NULL) {
			(*(EventFunctionPtrs_p->EventInvalidParameterNotify_p)) (SlotHandle);
		}
	}

	if (STATUS_BLK_TRANSPORT_ERROR_FLAG & ActiveStatusBlkFlags) {
		stpti_printf("STATUS_BLK_PACKET_ERROR rx'ed");
		if (EventFunctionPtrs_p->EventPacketErrorNotify_p != NULL) {
			(*(EventFunctionPtrs_p->EventPacketErrorNotify_p)) (SlotHandle);
		}
	}

	if (STATUS_BLK_PES_ERROR & ActiveStatusBlkFlags) {
		stpti_printf("STATUS_BLK_PES_ERROR rx'ed");
		if (EventFunctionPtrs_p->EventPESErrorNotify_p != NULL) {
			(*(EventFunctionPtrs_p->EventPESErrorNotify_p)) (SlotHandle);
		}
	}

	if (STATUS_BLK_SECTION_CRC_DISCARD & ActiveStatusBlkFlags) {
		stpti_printf("STATUS_BLK_PES_ERROR rx'ed");
		if (EventFunctionPtrs_p->EventSectionCRCDiscardNotify_p != NULL) {
			(*(EventFunctionPtrs_p->EventSectionCRCDiscardNotify_p)) (SlotHandle);
		}
	}

	if (STATUS_BLK_DATA_ENTRY_COMPLETE & ActiveStatusBlkFlags) {
		stpti_printf("STATUS_BLK_DATA_ENTRY_COMPLETE rx'ed");
		stptiHAL_ProcessDataEntryEvent(DataEntryEventSlotHandle, EventFunctionPtrs_p);
	}

	if (STATUS_BLK_MARKER_ERROR & ActiveStatusBlkFlags) {
		stpti_printf("STATUS_BLK_MARKER_ERROR rx'ed");
		if (EventFunctionPtrs_p->EventMarkerErrorNotify_p != NULL) {
			(*(EventFunctionPtrs_p->EventMarkerErrorNotify_p)) (SlotHandle, StatBlk_p->MarkerType, 1,
									    StatBlk_p->MarkerID0);
		}
	}

	if (STATUS_BLK_SECONDARY_PID_DISCARDED & ActiveStatusBlkFlags) {
		stpti_printf("STATUS_BLK_SECONDARY_PID_DISCARDED rx'ed");
		if (EventFunctionPtrs_p->EventSecondaryPidPktNotify_p != NULL) {
			(*(EventFunctionPtrs_p->EventSecondaryPidPktNotify_p)) (SlotHandle);
		}
	}

	if (STATUS_BLK_SECONDARY_PID_CC_ERROR & ActiveStatusBlkFlags) {
		stptiHAL_SecondaryType_t SecondarySlotType;
		FullHandle_t SecondaryHandle;

		stpti_printf("STATUS_BLK_SECONDARY_PID_CC_ERROR rx'ed");

		/* Lock access to the objman to prevent the API modifing this slot data entry list */
		stptiOBJMAN_WriteLock(SlotHandle, &LocalLockState);
		{
			stptiHAL_Slot_t *Slot_p = stptiHAL_GetObjectSlot_p(SlotHandle);
			SecondarySlotType = Slot_p->SecondarySlotType;
			SecondaryHandle = Slot_p->SecondaryHandle;
		}
		stptiOBJMAN_Unlock(SlotHandle, &LocalLockState);

		if (SecondarySlotType == stptiHAL_SECONDARY_TYPE_PRIM) {
			if (EventFunctionPtrs_p->EventCCErrorNotify_p != NULL) {
				(*(EventFunctionPtrs_p->EventCCErrorNotify_p)) (SecondaryHandle, StatBlk_p->ExpectedCC,
										StatBlk_p->ReceivedCC);
			}
		}
	}
}

/**
    @brief Process Data Entry Complete event

    Function to process and notify of Data Entry events if required.

    @param  SlotHandle           Slothandle
    @param  StatBlk_p            A pointer to the stat block

    @return                      None
*/
static void stptiHAL_ProcessDataEntryEvent(FullHandle_t SlotHandle, stptiHAL_EventFuncPtrs_t *EventFunctionPtrs_p)
{
	FullHandle_t DataEntryHandle;
	stptiHAL_Slot_t *Slot_p = NULL;
	stptiHAL_DataEntry_t *DataEntry_p = NULL;
	BOOL NotifyEvent = FALSE;
	BOOL RemoveEntry = FALSE;

	stptiSupport_AccessLockState_t LocalLockState = stptiSupport_UNLOCKED;

	/* Lock access to the objman to prevent the API modifing this slot data entry list */
	stptiOBJMAN_WriteLock(SlotHandle, &LocalLockState);
	{
		Slot_p = stptiHAL_GetObjectSlot_p(SlotHandle);

		DataEntryHandle = Slot_p->DataEntryHandle;

		stpti_printf("stptiHAL_ProcessDataEntryEvent DataEntryHandle %x", DataEntryHandle.word);

		if (!stptiOBJMAN_IsNullHandle(DataEntryHandle)) {

			DataEntry_p = stptiHAL_GetObjectDataEntry_p(DataEntryHandle);

			if (NULL != DataEntry_p) {
				NotifyEvent = DataEntry_p->NotifyEvent;

				stpti_printf("Slot Handle %x Data Entry Handle %x", SlotHandle.word,
					     DataEntryHandle.word);

				RemoveEntry = TRUE;

				DataEntry_p->RepeatCount = 0;
			} else {
				DataEntryHandle = stptiOBJMAN_NullHandle;
			}
		}
	}
	stptiOBJMAN_Unlock(SlotHandle, &LocalLockState);

	/* A data entry has completed on this slot so we need to disassociate it if the repeat count is expired, also generate and event if the user requires one. */
	if (!stptiOBJMAN_IsNullHandle(SlotHandle) && !stptiOBJMAN_IsNullHandle(DataEntryHandle)) {

		if (NotifyEvent == TRUE) {
			if (EventFunctionPtrs_p->EventDataEntryNotify_p != NULL) {
				stpti_printf("Calling EventDataEntryNotify_p");
				(*(EventFunctionPtrs_p->EventDataEntryNotify_p)) (SlotHandle, DataEntryHandle);
			}
		} else {
			stpti_printf("Entry STEVT not required");
		}

		if (RemoveEntry == TRUE) {
			/* Using the slot handle we can get the data entry handle */
			stptiOBJMAN_DisassociateObjects(SlotHandle, DataEntryHandle);
			stpti_printf("Removing entry");
		}
	}
}

/**
    @brief Process Timestamps

    Function to process timestamps from the status block information

    @param  StatBlk_p              A pointer to the stat block
    @param  ArrivalTime_p          A pointer to to store the Arrival time
    @param  ArrivalTimeExtension_p A pointer to to store the Arrival time extension
    @param  PCRTime_p              A pointer to to store the PCR time
    @param  PCRTimeExtension_p     A pointer to to store the PCR time extension

    @return                      None
*/
static void stptiHAL_ProcessEventTimestamps(stptiTP_StatusBlk_t *StatBlk_p, U32 *PCRLsw_p, U32 *PCRBit32_p,
					    U16 *PCRExt_p, U32 *ArrivalLsw_p, U32 *ArrivalBit32_p,
					    U16 *ArrivalExt_p)
{
	/* Process the PCR value */
	*PCRBit32_p = (StatBlk_p->Pcr0 & 0x80000000) ? 1 : 0;
	*PCRLsw_p = (StatBlk_p->Pcr0 << 1) | ((StatBlk_p->Pcr1 & 0x8000) ? 1 : 0);
	*PCRExt_p = (StatBlk_p->Pcr1 & 0x1FF);

	/* Process the arrival Time (STFE or DLNA tag) */
	stptiHAL_vDeviceTranslateTag(StatBlk_p->ArrivalTime0, StatBlk_p->ArrivalTime1, ArrivalBit32_p, ArrivalLsw_p,
				     ArrivalExt_p);
}
