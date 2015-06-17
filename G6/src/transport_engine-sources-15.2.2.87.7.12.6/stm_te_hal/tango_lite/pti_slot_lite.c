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
   @file   pti_slot_lite.c
   @brief  Slot Object Initialisation, Termination and Manipulation Functions.

   This file implements the functions for creating, destroying and accessing PTI slots.

   Dummy function without TP functionality.
 */

#if 0
#define STPTI_PRINT
#endif

/* Includes ---------------------------------------------------------------- */
#include "linuxcommon.h"

/* Includes from API level */
#include "../pti_debug.h"
#include "../pti_tshal_api.h"

/* Includes from the HAL / ObjMan level */
#include "pti_vdevice_lite.h"
#include "pti_slot_lite.h"

/* MACROS ------------------------------------------------------------------ */

/* Private Constants ------------------------------------------------------- */
/* Private Variables ------------------------------------------------------- */
/* Private Function Prototypes --------------------------------------------- */
ST_ErrorCode_t stptiHAL_SlotAllocatorLite(FullHandle_t SlotHandle, void *params_p);
ST_ErrorCode_t stptiHAL_SlotAssociatorLite(FullHandle_t SlotHandle, FullHandle_t AssocObjectHandle);
ST_ErrorCode_t stptiHAL_SlotDisassociatorLite(FullHandle_t SlotHandle, FullHandle_t AssocObjectHandle);
ST_ErrorCode_t stptiHAL_SlotDeallocatorLite(FullHandle_t SlotHandle);
ST_ErrorCode_t stptiHAL_SlotSetPIDLite(FullHandle_t SlotHandle, U16 Pid, BOOL ResetSlot);
ST_ErrorCode_t stptiHAL_SlotGetPIDLite(FullHandle_t SlotHandle, U16 *Pid_p);
ST_ErrorCode_t stptiHAL_SlotGetStateLite(FullHandle_t SlotHandle, U32 *TSPacketCount_p,
					 stptiHAL_ScrambledState_t *CurrentScramblingState_p);
ST_ErrorCode_t stptiHAL_SlotEnableEventLite(FullHandle_t SlotHandle, stptiHAL_EventType_t Event, BOOL Enable);
ST_ErrorCode_t stptiHAL_SlotRemapScramblingBitsLite(FullHandle_t SlotHandle, U8 SCBitsToMatch, U8 ReplacementSCBits);
ST_ErrorCode_t stptiHAL_SlotSetCorruptionLite(FullHandle_t SlotHandle, BOOL EnableCorruption, U8 CorruptionOffset,
					      U8 CorruptionValue);
ST_ErrorCode_t stptiHAL_SlotFeatureEnableLite(FullHandle_t SlotHandle, stptiHAL_SlotFeature_t Feature, BOOL Enable);
ST_ErrorCode_t stptiHAL_SlotSetSecurePathOutputNodeLite(FullHandle_t SlotHandle,
							stptiHAL_SecureOutputNode_t OutputNode);
ST_ErrorCode_t stptiHAL_SlotSetSecurePathIDLite(FullHandle_t SlotHandle, U32 PathID);
ST_ErrorCode_t stptiHAL_SlotGetModeLite(FullHandle_t SlotHandle, U16 *Mode_p);
ST_ErrorCode_t stptiHAL_SlotSetSecondaryPidLite(FullHandle_t SecondarySlotHandle, FullHandle_t PrimarySlotHandle,
						stptiHAL_SecondaryPidMode_t Mode);
ST_ErrorCode_t stptiHAL_SlotGetSecondaryPidLite(FullHandle_t SlotHandle, stptiHAL_SecondaryPidMode_t *Mode);
ST_ErrorCode_t stptiHAL_SlotClearSecondaryPidLite(FullHandle_t SecondarySlotHandle, FullHandle_t PrimarySlotHandle);
ST_ErrorCode_t stptiHAL_SlotSetRemapPIDLite(FullHandle_t SlotHandle, U16 Pid);

static void stptiHAL_SlotUnchainClearPIDLite(stptiHAL_vDevice_lite_t *vDevice_p, stptiHAL_Slot_lite_t *SlotToBeUnchained_p);
static ST_ErrorCode_t stptiHAL_SlotChainSetPIDLite(stptiHAL_vDevice_lite_t *vDevice_p, stptiHAL_Slot_lite_t *SlotToBeChained_p,
						   U16 NewPid);
static ST_ErrorCode_t stptiHAL_SlotSetPIDWorkerFnLite(FullHandle_t SlotHandle, U16 Pid, BOOL SuppressReset);

/* Public Constants -------------------------------------------------------- */

/* Export the API */
const stptiHAL_SlotAPI_t stptiHAL_SlotAPI_Lite = {
	{
		/* Allocator                Associator                   Disassociator                   Deallocator */
		stptiHAL_SlotAllocatorLite, stptiHAL_SlotAssociatorLite, stptiHAL_SlotDisassociatorLite, stptiHAL_SlotDeallocatorLite,
		NULL, NULL
	},
	stptiHAL_SlotSetPIDLite,
	stptiHAL_SlotGetPIDLite,
	stptiHAL_SlotGetStateLite,
	stptiHAL_SlotEnableEventLite,
	stptiHAL_SlotRemapScramblingBitsLite,
	stptiHAL_SlotSetCorruptionLite,
	stptiHAL_SlotFeatureEnableLite,
	stptiHAL_SlotSetSecurePathOutputNodeLite,
	stptiHAL_SlotSetSecurePathIDLite,
	stptiHAL_SlotGetModeLite,
	stptiHAL_SlotSetSecondaryPidLite,
	stptiHAL_SlotGetSecondaryPidLite,
	stptiHAL_SlotClearSecondaryPidLite,
	stptiHAL_SlotSetRemapPIDLite
};

/* Object Management Functions ------------------------------------------------------------------ */

/**
   @brief  Allocate this slot

   Lite version without TP functionality.

   @param  SlotHandle           The handle of the slot.
   @param  params_p             The parameters for this slot.

   @return                      A standard st error type...
                                - ST_NO_ERROR
 */
ST_ErrorCode_t stptiHAL_SlotAllocatorLite(FullHandle_t SlotHandle, void *params_p)
{
	/* Already write locked */
	stptiHAL_Slot_lite_t *Slot_p = stptiHAL_GetObjectSlot_lite_p(SlotHandle);
	stptiHAL_SlotConfigParams_t *Parameters_p = (stptiHAL_SlotConfigParams_t *) params_p;

	Slot_p->SlotMode = Parameters_p->SlotMode;
	Slot_p->PID = stptiHAL_InvalidPID;
	Slot_p->PidIndex = -1;
	Slot_p->PreviousSlotInChain = stptiOBJMAN_NullHandle;
	Slot_p->NextSlotInChain = stptiOBJMAN_NullHandle;

	return (ST_NO_ERROR);
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
ST_ErrorCode_t stptiHAL_SlotAssociatorLite(FullHandle_t SlotHandle, FullHandle_t AssocObjectHandle)
{
	return (ST_NO_ERROR);
}

/**
   @brief  Disassociate this slot from another object

   This function disassociates the slot, from either another slot or a buffer.

   @param  SlotHandle           The handle of the slot.
   @param  AssocObjectHandle    The handle of the object to be disassociated.

   @return                      A standard st error type...
                                - ST_NO_ERROR
 */
ST_ErrorCode_t stptiHAL_SlotDisassociatorLite(FullHandle_t SlotHandle, FullHandle_t AssocObjectHandle)
{
	return (ST_NO_ERROR);
}

/**
   @brief  Deallocate this slot

   This function deallocates the slot, and unchains it from any other slots.

   @param  SlotHandle           The Handle of the slot.

   @return                      A standard st error type...
                                - ST_NO_ERROR
 */
ST_ErrorCode_t stptiHAL_SlotDeallocatorLite(FullHandle_t SlotHandle)
{
	ST_ErrorCode_t Error = ST_NO_ERROR;
	/* Already write locked */
	stptiHAL_Slot_lite_t *Slot_p = stptiHAL_GetObjectSlot_lite_p(SlotHandle);
	stptiHAL_vDevice_lite_t *vDevice_p = stptiHAL_GetObjectvDevice_lite_p(SlotHandle);

	/* If we are deallocating a slot we must check to see if its a secondary pid slot - if it is we must also clear the secondary_pid_info structure on the linked slot */
	if (Slot_p != NULL) {
		U16 OriginalPID = Slot_p->PID;

		/* Unchain this slot if it is */
		stptiHAL_SlotUnchainClearPIDLite(vDevice_p, Slot_p);

		if (stptiHAL_InvalidPID != OriginalPID) {
			if (stptiHAL_StreamIDNone != vDevice_p->StreamID) {
				(void)stptiTSHAL_call(TSInput.TSHAL_TSInputSetClearPid, 0,
						      stptiOBJMAN_GetpDeviceIndex(SlotHandle), vDevice_p->StreamID,
						      OriginalPID, FALSE);
				/* Ignore error here, as we can error if the TSInput has be "unconfigured" */
			}
		}
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
ST_ErrorCode_t stptiHAL_SlotGetModeLite(FullHandle_t SlotHandle, U16 *Mode_p)
{
	ST_ErrorCode_t Error = ST_NO_ERROR;
	stptiSupport_AccessLockState_t LocalLockState = stptiSupport_UNLOCKED;

	stptiOBJMAN_ReadLock(SlotHandle, &LocalLockState);
	{
		stptiHAL_Slot_lite_t *Slot_p = stptiHAL_GetObjectSlot_lite_p(SlotHandle);
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

   This function set the PID set for this slot. With TP functionality.

   @param  SlotHandle           The Handle of the slot.
   @param  Pid                  The PID to be set (or stptiHAL_InvalidPID if clearing it)
   @param  SuppressReset        TRUE if you which to prevent reset of the slot state and any
                                associated buffer's pointers.

   @return                      A standard st error type...
                                - ST_NO_ERROR
 */
ST_ErrorCode_t stptiHAL_SlotSetPIDLite(FullHandle_t SlotHandle, U16 Pid, BOOL SuppressReset)
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
		stptiHAL_Slot_lite_t *Slot_p = stptiHAL_GetObjectSlot_lite_p(SlotHandle);

		FullHandle_t AssocdSlotHandles[MAX_CHAINED_SLOTS + 1];
		int CountOfAssocdSlotHandles = 0;
		int index;

		CountOfAssocdSlotHandles =
		    stptiOBJMAN_ReturnAssociatedObjects(SlotHandle, AssocdSlotHandles,
							sizeof(AssocdSlotHandles) / sizeof(AssocdSlotHandles[0]),
							OBJECT_TYPE_SLOT);
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
				    stptiHAL_SlotLookupPIDLite(SlotHandle, Pid, SlotHandlesWithRequestedPID,
							   sizeof(SlotHandlesWithRequestedPID) /
							   sizeof(SlotHandlesWithRequestedPID[0]));

				/* Now check all the associated slots handles to see which have the pid already set */
				for (index = 0; index < CountOfAssocdSlotHandles; index++) {
					stptiHAL_Slot_lite_t *AssocdSlot_p =
					    stptiHAL_GetObjectSlot_lite_p(AssocdSlotHandles[index]);
					if (AssocdSlot_p->PID == Pid) {
						CountOfAssocdWithSamePID++;
					}
				}

				/* Total CountOfSlotsAfterPidChange = assocd slots that will change pid + all the slots with the same pid */
				CountOfSlotsAfterPidChange =
				    CountOfAssocdSlotHandles - CountOfAssocdWithSamePID +
				    CountOfSlotsWithRequestedPID;

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
		

		if (ST_NO_ERROR == Error) {
			/* We shouldn't get errors here as the validation stage above should have caught these */

			/* Set PID for this slot */
			Error = stptiHAL_SlotSetPIDWorkerFnLite(SlotHandle, Pid, SuppressReset);
			if (ST_NO_ERROR == Error) {
				/* Now set PID for all associated slots */
				for (index = 0; index < CountOfAssocdSlotHandles; index++) {
					Error =
					    stptiHAL_SlotSetPIDWorkerFnLite(AssocdSlotHandles[index], Pid,
									    SuppressReset);
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
ST_ErrorCode_t stptiHAL_SlotGetPIDLite(FullHandle_t SlotHandle, U16 *Pid_p)
{
	ST_ErrorCode_t Error = ST_NO_ERROR;
	stptiSupport_AccessLockState_t LocalLockState = stptiSupport_UNLOCKED;

	stptiOBJMAN_ReadLock(SlotHandle, &LocalLockState);
	{
		stptiHAL_Slot_lite_t *Slot_p = stptiHAL_GetObjectSlot_lite_p(SlotHandle);
		*Pid_p = Slot_p->PID;
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
ST_ErrorCode_t stptiHAL_SlotGetStateLite(FullHandle_t SlotHandle, U32 *TSPacketCount_p,
					 stptiHAL_ScrambledState_t *CurrentScramblingState_p)
{
	if (TSPacketCount_p != NULL) {
		*TSPacketCount_p = 0;
	}
	*CurrentScramblingState_p = stptiHAL_UNSCRAMBLED;

	return (ST_NO_ERROR);
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
ST_ErrorCode_t stptiHAL_SlotEnableEventLite(FullHandle_t SlotHandle, stptiHAL_EventType_t Event, BOOL Enable)
{
	return (ST_NO_ERROR);
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
ST_ErrorCode_t stptiHAL_SlotRemapScramblingBitsLite(FullHandle_t SlotHandle, U8 SCBitsToMatch, U8 ReplacementSCBits)
{
	return (ST_NO_ERROR);
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
ST_ErrorCode_t stptiHAL_SlotSetCorruptionLite(FullHandle_t SlotHandle, BOOL EnableCorruption, U8 CorruptionOffset,
					      U8 CorruptionValue)
{
	return (ST_NO_ERROR);
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
ST_ErrorCode_t stptiHAL_SlotFeatureEnableLite(FullHandle_t SlotHandle, stptiHAL_SlotFeature_t Feature, BOOL Enable)
{
	return (ST_NO_ERROR);
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
ST_ErrorCode_t stptiHAL_SlotSetSecurePathOutputNodeLite(FullHandle_t SlotHandle, stptiHAL_SecureOutputNode_t OutputNode)
{
	return (ST_NO_ERROR);
}

/**
   @brief  Set the Descrambler/Scrambler Path ID for this slot.

   This function sets the descrambler path id for this slot.

   @param  SlotHandle               The Handle of the slot.
   @param  PathID                   The path id.

   @return                          A standard st error type...
                                    - ST_NO_ERROR
 */
ST_ErrorCode_t stptiHAL_SlotSetSecurePathIDLite(FullHandle_t SlotHandle, U32 PathID)
{
	return (ST_NO_ERROR);
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
ST_ErrorCode_t stptiHAL_SlotSetSecondaryPidLite(FullHandle_t SecondarySlotHandle, FullHandle_t PrimarySlotHandle,
						stptiHAL_SecondaryPidMode_t Mode)
{
	return ST_ERROR_FEATURE_NOT_SUPPORTED;
}

/**
   @brief  Get the secondary pid mode for the slot

   This function reads the object manage setting for this slot to obtain the secondary pid mode.

   @param  SlotHandle               Slot handle
   @param  Mode_p                   Pointer to return operational mode of the slot.

   @return                          A standard st error type...
                                    - ST_NO_ERROR
 */
ST_ErrorCode_t stptiHAL_SlotGetSecondaryPidLite(FullHandle_t SlotHandle, stptiHAL_SecondaryPidMode_t * Mode_p)
{
	return ST_ERROR_FEATURE_NOT_SUPPORTED;
}

/**
   @brief  Disable the secondary pid feature for the primary and secondary slot

   This function disables the TP secondary pid feature on primary and secondary slots

   @param  SecondarySlotHandle      Secondary pid secondary slot handle
   @param  PrimarySlotHandle        Secondary pid primary slot handle

   @return                          A standard st error type...
                                    - ST_NO_ERROR
 */
ST_ErrorCode_t stptiHAL_SlotClearSecondaryPidLite(FullHandle_t SecondarySlotHandle, FullHandle_t PrimarySlotHandle)
{
	return ST_ERROR_FEATURE_NOT_SUPPORTED;
}

/**
   @brief  Set an Remap pid for this slot

   This function set a remap pid for this slot.

   @param  SlotHandle           The Handle of the slot.
   @param  Pid                  Remap pid value
   @return                      A standard st error type...
                                - ST_NO_ERROR
 */
ST_ErrorCode_t stptiHAL_SlotSetRemapPIDLite(FullHandle_t SlotHandle, U16 Pid)
{
	return (ST_NO_ERROR);
}

/* Supporting Functions ------------------------------------------------------------------------- */

/**
   @brief  Check the list of slots and unchain this slot if necessary.

   This function manages slot chaining for removing slots.  Its complement is stptiHAL_SlotChainSetPIDLite
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
static void stptiHAL_SlotUnchainClearPIDLite(stptiHAL_vDevice_lite_t *vDevice_p, stptiHAL_Slot_lite_t *SlotToBeUnchained_p)
{
	stptiHAL_Slot_lite_t *PreviousSlotInChain_p = stptiHAL_GetObjectSlot_lite_p(SlotToBeUnchained_p->PreviousSlotInChain);
	stptiHAL_Slot_lite_t *NextSlotInChain_p = stptiHAL_GetObjectSlot_lite_p(SlotToBeUnchained_p->NextSlotInChain);

	if (PreviousSlotInChain_p == NULL) {
		/* No previous slot, so this is the head slot */
		stpti_printf("This slot is the head and so not referenced.");

		if (NextSlotInChain_p != NULL) {
			/* This is a head slot, so we need to make the slot mapping entry now points to the next slot in the chain */
			stptiOBJMAN_ReplaceItemInList(&vDevice_p->PidIndexes, SlotToBeUnchained_p->PidIndex,
						      NextSlotInChain_p);
			NextSlotInChain_p->PreviousSlotInChain = stptiOBJMAN_NullHandle;
		} else {
			/* Last Slot in the Chain so we deallocate the PidIndex */
			/* Now we dealt with the "head" slot case, we can now clear the pid disabling this slot. */
			if (SlotToBeUnchained_p->PidIndex >= 0) {
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
		} else {
			/* There is no next slot (slot being removed is the tail of the chain) */
			stpti_printf("This slot is the tail, so the previous slot %d is now the tail.",
				     PreviousSlotInChain_p->SlotIndex);
			PreviousSlotInChain_p->NextSlotInChain = stptiOBJMAN_NullHandle;
		}
	}

	/* Unlink this slot */
	SlotToBeUnchained_p->PreviousSlotInChain = stptiOBJMAN_NullHandle;
	SlotToBeUnchained_p->NextSlotInChain = stptiOBJMAN_NullHandle;
	SlotToBeUnchained_p->PID = stptiHAL_InvalidPID;
	SlotToBeUnchained_p->PidIndex = -1;
}

/**
   @brief  Check the list of slots and Chain this slot if necessary.

   This function manages slot chaining when setting slots.  Its complement is
   stptiHAL_SlotUnchainClearPIDLite used when clearing pids on slots / deallocating slots.

   The aim of this function is to maintain...
     - maintain the Slot_t PreviousSlotInChain, NextSlotInChain references
     - add new slots with the same pid to the end of the chain
     - Set the pid in the TP's pid table (if this is the first)
     - maintain the slot chain in the TP slot info table

   @param  vDevice_p            A pointer to the vDevice
   @param  SlotToBeUnchained_p  A pointer to the the Slot to be unchained

   @return                      Nothing
 */
static ST_ErrorCode_t stptiHAL_SlotChainSetPIDLite(stptiHAL_vDevice_lite_t *vDevice_p, stptiHAL_Slot_lite_t *SlotToBeChained_p,
						   U16 NewPid)
{
	ST_ErrorCode_t Error = ST_NO_ERROR;

	stptiHAL_Slot_lite_t *HeadSlot_p = NULL, *TailSlot_p = NULL;
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
				TailSlot_p = stptiHAL_GetObjectSlot_lite_p(TailSlot_p->NextSlotInChain);
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
			vDevice_p->TP_Interface.NumberOfSlots++;
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
		if (NoOtherSlotsWithThisPID) {
			/* No previous slots, so this is the only slot with this pid.  Define it as the head slot. */
			HeadSlot_p = SlotToBeChained_p;
		} else {
			/* There are one or more slots with this pid so we add to the tail of the chain */
			TailSlot_p->NextSlotInChain = stptiOBJMAN_ObjectPointerToHandle(SlotToBeChained_p);
			SlotToBeChained_p->PreviousSlotInChain = stptiOBJMAN_ObjectPointerToHandle(TailSlot_p);
			SlotToBeChained_p->PidIndex = HeadSlot_p->PidIndex;
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
static ST_ErrorCode_t stptiHAL_SlotSetPIDWorkerFnLite(FullHandle_t SlotHandle, U16 Pid, BOOL SuppressReset)
{
	/* ERROR HANDLING MUST BE VERY CAREFULLY CONTROLLED IN THIS FUNCTION!!!
	   If you are not careful you can leave the chain in a broken state.  Before this function is
	   run, checks should have been done to make sure that no errors should occur during execution.

	   The complexity is caused by the requirement to modify the slot chain whilst the slots are in
	   use.  You have to be very careful in the order you do things.  Dealing with all the errors
	   as they occur could increase the complexity to unmaintainable levels. */

	ST_ErrorCode_t Error = ST_NO_ERROR;

	stptiHAL_vDevice_lite_t *vDevice_p = stptiHAL_GetObjectvDevice_lite_p(SlotHandle);
	stptiHAL_Slot_lite_t *Slot_p = stptiHAL_GetObjectSlot_lite_p(SlotHandle);

	U16 OriginalPID = Slot_p->PID;

	/* Unchain it if the PID was already in use */
	if (OriginalPID != stptiHAL_InvalidPID) {
		/* If the PID is already in use, we must make sure that this pid is not matched in case it
		   is at the top of the pid table (the head of the chain) and it would prevent other slots
		   in the chain from running when it is unchained. */
		stptiHAL_SlotUnchainClearPIDLite(vDevice_p, Slot_p);
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
		   must always call stptiHAL_SlotChainSetPIDLite() even on TSInput errors. */
		Error = stptiHAL_SlotChainSetPIDLite(vDevice_p, Slot_p, Pid);
		if (ST_NO_ERROR != Error)
			Pid = stptiHAL_InvalidPID;
	}

	Slot_p->PID = Pid;

	return (Error);
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
int stptiHAL_SlotLookupPIDLite(FullHandle_t vDeviceHandle, U16 PIDtoFind, FullHandle_t *SlotArray, int SlotArraySize)
{
	stptiHAL_vDevice_lite_t *vDevice_p = stptiHAL_GetObjectvDevice_lite_p(vDeviceHandle);
	stptiHAL_Slot_lite_t *Slot_p;
	int index, SlotsFound = 0;

	stptiOBJMAN_FirstInList(&vDevice_p->PidIndexes, (void *)&Slot_p, &index);
	while (index >= 0 && SlotsFound < SlotArraySize) {
		if (Slot_p->PID == PIDtoFind) {
			/* Slots with the same PID are chained together, so we need to follow the chain */
			do {
				SlotArray[SlotsFound++] = stptiOBJMAN_ObjectPointerToHandle(Slot_p);
				Slot_p = stptiHAL_GetObjectSlot_lite_p(Slot_p->NextSlotInChain);
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
U16 stptiHAL_LookupPIDsOnSlotsLite(FullHandle_t vDeviceHandle, U16 *PIDArray_p, U16 PIDArraySize)
{
	stptiHAL_vDevice_lite_t *vDevice_p = stptiHAL_GetObjectvDevice_lite_p(vDeviceHandle);
	stptiHAL_Slot_lite_t *Slot_p;
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
