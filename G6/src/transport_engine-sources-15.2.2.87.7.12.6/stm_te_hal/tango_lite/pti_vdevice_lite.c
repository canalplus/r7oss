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
   @file   pti_vdevice_lite.c
   @brief  Virtual Device Allocation, Deallocation functions.

   This file implements the functions for creating and destroying virtual devices.  This allows us
   to have appearance of multiple PTI's on one physical PTI.

   Lite version without any functionality for TP
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
#include "pti_pdevice_lite.h"
#include "pti_vdevice_lite.h"
#include "pti_dbg_lite.h"
#include "pti_slot_lite.h"
#include "pti_swinjector_lite.h"

/* MACROS ------------------------------------------------------------------ */
/* Private Constants ------------------------------------------------------- */
/* Private Variables ------------------------------------------------------- */
/* Private Function Prototypes --------------------------------------------- */

/* Although these prototypes are not exported directly they are exported through the API constant below. */
/* Add the definition to pti_hal.h */
ST_ErrorCode_t stptiHAL_vDeviceAllocatorLite(FullHandle_t vDeviceHandle, void *params_p);
ST_ErrorCode_t stptiHAL_vDeviceDeallocatorLite(FullHandle_t vDeviceHandle);

ST_ErrorCode_t stptiHAL_vDeviceDebugLite(FullHandle_t vDeviceHandle, char *DebugClass, char *String, int *StringSize_p,
					 int MaxStringSize, int Offset, int *EOF_p);
ST_ErrorCode_t stptiHAL_vDeviceLookupSlotForPIDLite(FullHandle_t vDeviceHandle, U16 PIDtoFind,
						    FullHandle_t *SlotHandleArray_p, int SlotArraySize,
						    int *SlotsMatching);
ST_ErrorCode_t stptiHAL_vDeviceLookupPIDsLite(FullHandle_t vDeviceHandle, U16 *PIDArray_p, U16 PIDArraySize,
					      U16 *PIDsFound);
ST_ErrorCode_t stptiHAL_vDeviceGetStreamIDLite(FullHandle_t vDeviceHandle, U32 *StreamID_p, BOOL *UseTimerTag_p);
ST_ErrorCode_t stptiHAL_vDeviceSetStreamIDLite(FullHandle_t vDeviceHandle, U32 StreamID, BOOL UseTimerTag);
ST_ErrorCode_t stptiHAL_vDeviceSetEventLite(FullHandle_t vDeviceHandle, stptiHAL_EventType_t Event, BOOL Enable);
ST_ErrorCode_t stptiHAL_vDeviceGetTSProtocolLite(FullHandle_t vDeviceHandle, stptiHAL_TransportProtocol_t *Protocol_p,
						 U8 *PacketSize_p);
ST_ErrorCode_t stptiHAL_vDeviceGetStreamStatisticsLite(FullHandle_t vDeviceHandle,
						       stptiHAL_vDeviceStreamStatistics_t *Statistics_p);
ST_ErrorCode_t stptiHAL_vDeviceResetStreamStatisticsLite(FullHandle_t vDeviceHandle);
ST_ErrorCode_t stptiHAL_vDeviceGetCapability(FullHandle_t vDeviceHandle,
					     stptiHAL_vDeviceConfigStatus_t *ConfigStatus_p);

ST_ErrorCode_t stptiHAL_vDeviceIndexesEnableLite(FullHandle_t vDeviceHandle, BOOL Enable);
ST_ErrorCode_t stptiHAL_vDeviceFirmwareResetLite(FullHandle_t vDeviceHandle);
ST_ErrorCode_t stptiHAL_vDevicePowerDownLite(FullHandle_t vDeviceHandle);
ST_ErrorCode_t stptiHAL_vDevicePowerUpLite(FullHandle_t vDeviceHandle);
ST_ErrorCode_t stptiHAL_vDeviceFeatureEnableLite(FullHandle_t vDeviceHandle, stptiHAL_vDeviceFeature_t Feature,
						 BOOL Enable);
ST_ErrorCode_t stptiHAL_vDeviceGetTimerLite(FullHandle_t vDeviceHandle, U32 *TimerValue_p, U64 *SystemTime_p);

static ST_ErrorCode_t stptiHAL_vDeviceUpdateStreamIDLite(FullHandle_t vDeviceHandle, U32 StreamID);

/* Public Constants ------------------------------------------------------- */

/* Export the API */
const stptiHAL_vDeviceAPI_t stptiHAL_vDeviceAPI_Lite = {
	{
		/* Allocator                   Associator Disassociator Deallocator */
		stptiHAL_vDeviceAllocatorLite, NULL,      NULL,         stptiHAL_vDeviceDeallocatorLite,
		NULL, NULL
	},
	stptiHAL_vDeviceDebugLite,
	stptiHAL_vDeviceLookupSlotForPIDLite,
	stptiHAL_vDeviceLookupPIDsLite,
	stptiHAL_vDeviceGetStreamIDLite,
	stptiHAL_vDeviceSetStreamIDLite,
	stptiHAL_vDeviceSetEventLite,
	stptiHAL_vDeviceGetTSProtocolLite,
	stptiHAL_vDeviceGetStreamStatisticsLite,
	stptiHAL_vDeviceResetStreamStatisticsLite,
	stptiHAL_vDeviceGetCapability,
	stptiHAL_vDeviceIndexesEnableLite,
	stptiHAL_vDeviceFirmwareResetLite,
	stptiHAL_vDevicePowerDownLite,
	stptiHAL_vDevicePowerUpLite,
	stptiHAL_vDeviceFeatureEnableLite,
	stptiHAL_vDeviceGetTimerLite,
};

/* Object Management Functions ------------------------------------------------------------------ */

/**
   @brief  Allocator for vDevice

   This function sees if it is the first vDevice on this pDevice and if so will then initialise the
   pDevice, start the tp).   It also allocates space for slots and filters.

   This lite version is without TP functionality.

   Upon Entry we will be write locked.  Upon exit we must be write locked.

   @param  vDeviceHandle  The handle for the new vDevice.
   @param  params_p       (a pointer to stptiHAL_vDeviceAllocationParams_t) The initialisation
                          parameters for this vDevice.

   @return                A standard st error type...
                          - ST_NO_ERROR
                          - ST_ERROR_BAD_PARAMETER
                          - ST_ERROR_NO_MEMORY       (no space to allocate slots, filters)
 */
ST_ErrorCode_t stptiHAL_vDeviceAllocatorLite(FullHandle_t vDeviceHandle, void *params_p)
{
	ST_ErrorCode_t Error = ST_NO_ERROR;
	stptiHAL_vDeviceAllocationParams_t *vDeviceAllocationParams_p = (stptiHAL_vDeviceAllocationParams_t *) params_p;
	stptiHAL_vDevice_lite_t *vDevice_p = stptiHAL_GetObjectvDevice_lite_p(vDeviceHandle);
	stptiHAL_pDevice_lite_t *pDevice_p = stptiHAL_GetObjectpDevice_lite_p(vDeviceHandle);

	BOOL ListsAllocated = FALSE;

	/* Already write locked */

	if (params_p != NULL) {
		stpti_printf("Allocating ObjectType=%u Handle=%08x using vDevice allocator.",
			     (unsigned)stptiOBJMAN_GetObjectType(vDeviceHandle), (unsigned)vDeviceHandle.word);

		/* Setup TP structure pointers and the list pointers for managing TP resources */
		if (ST_NO_ERROR == Error) {
			/* Copy over the whole block for simplicity */
			memcpy(&vDevice_p->TP_Interface, &pDevice_p->TP_Interface, sizeof(stptiTP_Interface_lite_t));

			/* And update the structures to indication sizes for just this vdevice */
			vDevice_p->TP_Interface.NumberOfSlots = vDeviceAllocationParams_p->NumberOfSlots;
			vDevice_p->TP_Interface.NumberOfSectionFilters =
			    vDeviceAllocationParams_p->NumberOfSectionFilters;

			if (ST_NO_ERROR != stptiOBJMAN_AllocateList(&vDevice_p->PidIndexes)) {
				STPTI_PRINTF_ERROR("Unable to allocate list for PidIndexes");
				Error = ST_ERROR_NO_MEMORY;
			}

			ListsAllocated = TRUE;
		}

		if (ST_NO_ERROR == Error) {
			vDevice_p->TSProtocol = vDeviceAllocationParams_p->TSProtocol;
			vDevice_p->PacketSize = vDeviceAllocationParams_p->PacketSize;
			vDevice_p->StreamID = vDeviceAllocationParams_p->StreamID;
			vDevice_p->TSInputHandle = stptiOBJMAN_NullHandle;

			/* Set initial power state */
			vDevice_p->PoweredOn = TRUE;
		}

		/* Mechanism to tidy up should errors have occured */
		if (ST_NO_ERROR != Error) {
			if (ListsAllocated) {
				stptiOBJMAN_DeallocateList(&vDevice_p->PidIndexes);
			}
		}

	} else {
		Error = ST_ERROR_BAD_PARAMETER;
	}

	/* Must leave write locked */
	return (Error);
}

/**
   @brief  Deallocator for vDevice

   This function see if it is the last vDevice and if so terminate the  pDevice and if so will then initialise the
   pDevice.

   Upon Entry we will be write locked.  Upon exit we must be write locked.

   @param  vDeviceHandle  The handle for the vDevice to be deallocated (terminated).

   @return                A standard st error type...
                          - ST_NO_ERROR
                          - ST_ERROR_INTERRUPT_UNINSTALL
 */
ST_ErrorCode_t stptiHAL_vDeviceDeallocatorLite(FullHandle_t vDeviceHandle)
{
	ST_ErrorCode_t Error = ST_NO_ERROR;
	stptiHAL_vDevice_lite_t *vDevice_p = stptiHAL_GetObjectvDevice_lite_p(vDeviceHandle);

	/* Already write locked */

	stpti_printf("Deallocating ObjectType=%u Handle=%08x using vDevice deallocator.",
		     (unsigned)stptiOBJMAN_GetObjectType(vDeviceHandle), (unsigned)vDeviceHandle.word);

	if (stptiOBJMAN_GetNumberOfItemsInList(&vDevice_p->PidIndexes) > 0) {
		/* PID Table should be empty, if it isn't then this is indication of a memory leak.  This memory is recovered
		   below during the DeallocateList, but is a fault worth reporting.  The setting and clearing of pids should
		   be strongly correlated to Adding and Removing from the PidIndexes list. */
		STPTI_PRINTF_ERROR("PIDTable not empty when deallocating vDevice %08x - Please Report.",
				   (unsigned)stptiOBJMAN_GetObjectType(vDeviceHandle));
	}

	/* Deallocate the lists */
	stptiOBJMAN_DeallocateList(&vDevice_p->PidIndexes);

	/* Must leave write locked */
	return (Error);
}

/* Object HAL functions ------------------------------------------------------------------------- */

/**
   @brief  Debug Interface for procfs and STPTI_Debug.

   This function provide textual status information about the device (usually pDevice rather than
   vDevice)

   @param  vDeviceHandle  The handle for the vDevice to be examined
   @param  DebugClass     A string indicating the class of information required
   @param  String         A buffer to put the textual information in.  The buffer will be null
                          terminated.
   @param  StringSize_p   A pointer to a int which will hold the size of the text put into the
                          buffer (excluding null termination).
   @param  MaxStringSize  Size of the buffer
   @param  Offset         If not all the information was output last time, this is way for the
                          caller to indicate the amount of information to skip over before starting
                          to put text into the buffer.
   @param  EOF_p          A pointer to an int which will be 1 if all the information was output, or
                          zero if there is more information to come.

   @return                A standard st error type...
                          - ST_NO_ERROR
                          - ST_ERROR_BAD_PARAMETER          (bad DebugClass, NULL buffer)
 */
ST_ErrorCode_t stptiHAL_vDeviceDebugLite(FullHandle_t vDeviceHandle, char *DebugClass, char *String, int *StringSize_p,
					 int MaxStringSize, int Offset, int *EOF_p)
{
	ST_ErrorCode_t Error = ST_NO_ERROR;
	int chars_output = 0;

	if (String == NULL) {
		Error = ST_ERROR_BAD_PARAMETER;
	} else {
		/* Terminate the string so it can be printed out even if we get an error */
		if (MaxStringSize > 0) {
			String[0] = 0;
		}

		/* String size must be a minimum of 128 characters before we can report anything! */
		if (MaxStringSize < 128) {
			Error = ST_ERROR_BAD_PARAMETER;
		}
	}

	if (Error == ST_NO_ERROR) {
		stptiSupport_AccessLockState_t LocalLockState = stptiSupport_UNLOCKED;

		stptiOBJMAN_ReadLock(vDeviceHandle, &LocalLockState);
		{
			chars_output =
			    stptiHAL_DebugAsFileLite(vDeviceHandle, DebugClass, String, MaxStringSize, Offset, EOF_p);
		}
		stptiOBJMAN_Unlock(vDeviceHandle, &LocalLockState);
	}

	*StringSize_p = chars_output;
	return Error;

}

/**
   @brief  Lookup slot(s) for a specified PID

   This function returns the slots on this vDevice that have the specified PID.

   @param  vDeviceHandle  The handle for the vDevice to be examined
   @param  PIDtoFind      The PID to find
   @param  SlotArray      A pointer to an array of SlotHandles (for returning matches)
   @param  SlotArraySize  The size of the array of SlotHandles
   @param  SlotsMatching  A pointer to return the number of matches found

   @return                A standard st error type...
                          - ST_NO_ERROR
 */
ST_ErrorCode_t stptiHAL_vDeviceLookupSlotForPIDLite(FullHandle_t vDeviceHandle, U16 PIDtoFind,
						    FullHandle_t *SlotHandleArray_p, int SlotArraySize,
						    int *SlotsMatching)
{
	stptiSupport_AccessLockState_t LocalLockState = stptiSupport_UNLOCKED;

	stptiOBJMAN_ReadLock(vDeviceHandle, &LocalLockState);
	{
		*SlotsMatching = stptiHAL_SlotLookupPIDLite(vDeviceHandle, PIDtoFind, SlotHandleArray_p, SlotArraySize);
	}
	stptiOBJMAN_Unlock(vDeviceHandle, &LocalLockState);

	return (ST_NO_ERROR);
}

/**
   @brief  Lookup all the PIDs set on a specific vDevice

   This function returns the Pids set on this vDevice.

   @param  vDeviceHandle  The handle for the vDevice to be examined
   @param  PIDtoFind      The PID to find
   @param  SlotArray      A pointer to an array of SlotHandles (for returning matches)
   @param  SlotArraySize  The size of the array of SlotHandles
   @param  SlotsMatching  A pointer to return the number of matches found

   @return                A standard st error type...
                          - ST_NO_ERROR
 */
ST_ErrorCode_t stptiHAL_vDeviceLookupPIDsLite(FullHandle_t vDeviceHandle, U16 *PIDArray_p, U16 PIDArraySize,
					      U16 *PIDsFound)
{
	stptiSupport_AccessLockState_t LocalLockState = stptiSupport_UNLOCKED;

	stptiOBJMAN_ReadLock(vDeviceHandle, &LocalLockState);
	{
		/* We need to go through all the slot handles and then get the pid */
		*PIDsFound = stptiHAL_LookupPIDsOnSlotsLite(vDeviceHandle, PIDArray_p, PIDArraySize);
	}
	stptiOBJMAN_Unlock(vDeviceHandle, &LocalLockState);

	return (ST_NO_ERROR);
}

/**
   @brief  Lookup the StreamID on a specific vDevice

   This function returns the streamID on this vDevice.

   @param  vDeviceHandle  The handle for the vDevice to be examined

   @return                A standard st error type...
                          - ST_NO_ERROR
 */
ST_ErrorCode_t stptiHAL_vDeviceGetStreamIDLite(FullHandle_t vDeviceHandle, U32 *StreamID_p, BOOL *UseTimerTag_p)
{
	stptiSupport_AccessLockState_t LocalLockState = stptiSupport_UNLOCKED;

	stptiOBJMAN_ReadLock(vDeviceHandle, &LocalLockState);
	{
		stptiHAL_vDevice_lite_t *vDevice_p = stptiHAL_GetObjectvDevice_lite_p(vDeviceHandle);
		*StreamID_p = vDevice_p->StreamID;
		*UseTimerTag_p = FALSE;
	}
	stptiOBJMAN_Unlock(vDeviceHandle, &LocalLockState);

	return (ST_NO_ERROR);
}

/**
   @brief  Set the StreamID for a specific vDevice

   This function set the streamID on this vDevice.

   @param  vDeviceHandle  The handle for the vDevice to be configured.
   @param  StreamID       StreamID to configure vDevice to.

   @return                A standard st error type...
                          - ST_NO_ERROR
 */
ST_ErrorCode_t stptiHAL_vDeviceSetStreamIDLite(FullHandle_t vDeviceHandle, U32 StreamID, BOOL UseTimerTag)
{
	ST_ErrorCode_t Error = ST_NO_ERROR;
	stptiSupport_AccessLockState_t LocalLockState = stptiSupport_UNLOCKED;

	stptiOBJMAN_WriteLock(vDeviceHandle, &LocalLockState);
	{
		Error = stptiHAL_vDeviceUpdateStreamIDLite(vDeviceHandle, StreamID);
	}
	stptiOBJMAN_Unlock(vDeviceHandle, &LocalLockState);

	return (Error);
}

/**
   @brief  Enable/Disable a specific event vDevice

   This is the lite version without TP functionality

   @param  vDeviceHandle  The handle for the vDevice to be configured.
   @param  StreamID       StreamID to configure vDevice to.

   @return                A standard st error type...
                          - ST_ERROR_BAD_PARAMETER
                          - ST_NO_ERROR
 */
ST_ErrorCode_t stptiHAL_vDeviceSetEventLite(FullHandle_t vDeviceHandle, stptiHAL_EventType_t Event, BOOL Enable)
{
	return (ST_NO_ERROR);
}

/**
   @brief  Lookup the StreamID on a specific vDevice

   This function returns the transport stream protocol and packet size set for this vDevice.

   @param  vDeviceHandle  The handle for the vDevice to be examined
   @param  Protocol_p     (return) A pointer to the variable where the protocol will be set
   @param  PacketSize _p   (return) A pointer to the variable where the PacketSize will be set

   @return                A standard st error type...
                          - ST_ERROR_BAD_PARAMETER
                          - ST_NO_ERROR
 */
ST_ErrorCode_t stptiHAL_vDeviceGetTSProtocolLite(FullHandle_t vDeviceHandle, stptiHAL_TransportProtocol_t *Protocol_p,
						 U8 *PacketSize_p)
{
	stptiSupport_AccessLockState_t LocalLockState = stptiSupport_UNLOCKED;

	stptiOBJMAN_WriteLock(vDeviceHandle, &LocalLockState);
	{
		stptiHAL_vDevice_lite_t *vDevice_p = stptiHAL_GetObjectvDevice_lite_p(vDeviceHandle);

		if (vDevice_p == NULL) {
			stptiOBJMAN_Unlock(vDeviceHandle, &LocalLockState);
			return (ST_ERROR_BAD_PARAMETER);
		}

		if (Protocol_p != NULL) {
			*Protocol_p = vDevice_p->TSProtocol;
		}

		if (PacketSize_p != NULL) {
			switch (vDevice_p->TSProtocol) {
			case stptiHAL_DSS_TS_PROTOCOL:
				*PacketSize_p = 130;
				break;

			case stptiHAL_A3_TS_PROTOCOL:
			case stptiHAL_DVB_TS_PROTOCOL:
				*PacketSize_p = 188;
				break;

			default:
				STPTI_PRINTF_ERROR("Error unknown protocol.  Assuming packet size is 188.");
				*PacketSize_p = 188;
				break;
			}
		}
	}
	stptiOBJMAN_Unlock(vDeviceHandle, &LocalLockState);

	return (ST_NO_ERROR);
}

/**
   @brief  Get Statistics about the Input Stream

   This is the lite version without TP functionality so it returns zero.

   This function returns the number of packets, the number of packets with the transport error bit
   set and the number of packets with a cc error.

   @param  vDeviceHandle  The handle for the vDevice to be examined
   @param  Statistics_p   (return) A pointer to a structure to return the various counts

   @return                A standard st error type...
                          - ST_NO_ERROR
 */
ST_ErrorCode_t stptiHAL_vDeviceGetStreamStatisticsLite(FullHandle_t vDeviceHandle,
						       stptiHAL_vDeviceStreamStatistics_t *Statistics_p)
{
	Statistics_p->PacketCount = 0;
	Statistics_p->TransportErrorCount = 0;
	Statistics_p->CCErrorCount = 0;
	Statistics_p->Utilization = 0;

	return (ST_NO_ERROR);
}

ST_ErrorCode_t stptiHAL_vDeviceResetStreamStatisticsLite(FullHandle_t vDeviceHandle)
{
	return (ST_NO_ERROR);
}

/**
   @brief  Get capabilities of the (virtual) device

   This function returns the capabilities of the vdevice.

   @param  vDeviceHandle            The handle for the vDevice to be examined
   @param  ConfigStatus_p           (return) A pointer to where to put the capabilities

   @return                          A standard st error type...
                                    - ST_NO_ERROR
 */
ST_ErrorCode_t stptiHAL_vDeviceGetCapabilityLite(FullHandle_t vDeviceHandle,
						 stptiHAL_vDeviceConfigStatus_t *ConfigStatus_p)
{
	stptiSupport_AccessLockState_t LocalLockState = stptiSupport_UNLOCKED;

	stptiOBJMAN_WriteLock(vDeviceHandle, &LocalLockState);
	{
		stptiHAL_vDevice_lite_t *vDevice_p = stptiHAL_GetObjectvDevice_lite_p(vDeviceHandle);

		ConfigStatus_p->NumberSlots = vDevice_p->TP_Interface.NumberOfSlots;
		ConfigStatus_p->NumberDMAs = vDevice_p->TP_Interface.NumberOfDMAStructures;
		ConfigStatus_p->NumberHWSectionFilters = vDevice_p->TP_Interface.NumberOfSectionFilters;

		ConfigStatus_p->AlternateOutputSupport = FALSE;
		ConfigStatus_p->PidWildcardingSupport = TRUE;
		ConfigStatus_p->RawStreamDescrambling = TRUE;

		ConfigStatus_p->Protocol = vDevice_p->TSProtocol;
	}
	stptiOBJMAN_Unlock(vDeviceHandle, &LocalLockState);

	return (ST_NO_ERROR);
}

/**
   @brief  Enable (or Disable) Raw slot Indexing

   This is the lite version without TP functionality

   @param  vDeviceHandle            The handle for the vDevice
   @param  Enable                   TRUE if enabling (starting) indexing, FALSE if disabling
                                    (stopping)

   @return                          A standard st error type...
                                    - ST_NO_ERROR
 */
ST_ErrorCode_t stptiHAL_vDeviceIndexesEnableLite(FullHandle_t vDeviceHandle, BOOL Enable)
{
	return (ST_NO_ERROR);
}

ST_ErrorCode_t stptiHAL_vDeviceFirmwareResetLite(FullHandle_t vDeviceHandle)
{
	return (ST_NO_ERROR);
}

/**
   @brief Power-down this vDevice

   Set this vDevice into a power-down state, where it is safe to power-down any
   underlying pDevice

   @param  vDeviceHandle            The handle for the vDevice

   @return                          A standard st error type...
                                    - ST_NO_ERROR
 */
ST_ErrorCode_t stptiHAL_vDevicePowerDownLite(FullHandle_t vDeviceHandle)
{
	ST_ErrorCode_t Error = ST_NO_ERROR;
	stptiSupport_AccessLockState_t LocalLockState = stptiSupport_UNLOCKED;
	stptiHAL_vDevice_lite_t *vDevice_p = stptiHAL_GetObjectvDevice_lite_p(vDeviceHandle);

	/* First we need to disable any inputs */
	stptiOBJMAN_WriteLock(vDeviceHandle, &LocalLockState);

	if (vDevice_p->PoweredOn) {
		int index;
		unsigned int SlotCount = 0;
		stptiHAL_Slot_lite_t *Slot_p;
		FullHandle_t SlotHandleArray[64];

		/* Decouple from any associated TSInput */
		vDevice_p->SavedStreamID = vDevice_p->StreamID;
		Error = stptiHAL_vDeviceUpdateStreamIDLite(vDeviceHandle, (U32) stptiHAL_StreamIDNone);
		if (ST_NO_ERROR != Error) {
			STPTI_PRINTF_ERROR("Unable to update StreamID for vDevice [Handle: %08x]", vDeviceHandle.word);
			Error = ST_NO_ERROR;	/* Carry on even if error - not much we can do about it */
		}

		/* Reset all slots on this vDevice.  Unfortunately this has to be done in
		 * blocks to minimise delay caused by TP synchronisation */
		stptiOBJMAN_FirstInList(&vDevice_p->PidIndexes, (void *)&Slot_p, &index);
		while (index >= 0) {
			SlotHandleArray[SlotCount++] = stptiOBJMAN_ObjectPointerToHandle(Slot_p);

			stptiOBJMAN_NextInList(&vDevice_p->PidIndexes, (void *)&Slot_p, &index);
			if (SlotCount >= sizeof(SlotHandleArray) / sizeof(FullHandle_t) || index < 0) {
				SlotCount = 0;
			}
		}

		vDevice_p->PoweredOn = FALSE;
	}
	stptiOBJMAN_Unlock(vDeviceHandle, &LocalLockState);

	return Error;
}

/**
   @brief Power-up this vDevice

   Set this vDevice into a power-up state. The underlying pDevice must already
   be powered-up.

   @param  vDeviceHandle            The handle for the vDevice

   @return                          A standard st error type...
                                    - ST_NO_ERROR
 */
ST_ErrorCode_t stptiHAL_vDevicePowerUpLite(FullHandle_t vDeviceHandle)
{
	stptiSupport_AccessLockState_t LocalLockState = stptiSupport_UNLOCKED;
	ST_ErrorCode_t Error = ST_NO_ERROR;
	stptiHAL_vDevice_lite_t *vDevice_p = stptiHAL_GetObjectvDevice_lite_p(vDeviceHandle);

	stptiOBJMAN_WriteLock(vDeviceHandle, &LocalLockState);

	if (!vDevice_p->PoweredOn) {
		/* Restore saved stream id */
		Error = stptiHAL_vDeviceUpdateStreamIDLite(vDeviceHandle, vDevice_p->SavedStreamID);
		vDevice_p->SavedStreamID = (U32) stptiHAL_StreamIDNone;

		vDevice_p->PoweredOn = TRUE;
	}

	stptiOBJMAN_Unlock(vDeviceHandle, &LocalLockState);

	return Error;
}

/**
   @brief  Set specific features for the vDevice

   No TP functionality in this verions

   @param  vDeviceHandle            The handle for the vDevice
   @param  Feature                  Feature to adjust
   @param  Enable                   TRUE if feature should be Enabled

   @return                          A standard st error type...
                                    - ST_NO_ERROR
 */
ST_ErrorCode_t stptiHAL_vDeviceFeatureEnableLite(FullHandle_t vDeviceHandle, stptiHAL_vDeviceFeature_t Feature,
						 BOOL Enable)
{
	return (ST_NO_ERROR);
}

/**
   @brief  Return the vDevice parent pDevice timer value

   No TP support on this version.

   @param  vDeviceHandle            The handle for the vDevice
   @param  TimerValue_p             Place to store timer value
   @param  SystemTime_p             Place to store current time

   @return                          A standard st error type...
                                    - ST_NO_ERROR
 */
ST_ErrorCode_t stptiHAL_vDeviceGetTimerLite(FullHandle_t vDeviceHandle, U32 *TimerValue_p, U64 *SystemTime_p)
{
	return (ST_ERROR_FEATURE_NOT_SUPPORTED);
}

/* Supporting Functions ------------------------------------------------------------------------- */

/**
   @brief  Set the StreamID for a specific vDevice

   This function set the streamID on this vDevice.

   This is the lite version without TP functionality

   @param  vDeviceHandle  The handle for the vDevice to be configured.
   @param  StreamID       StreamID to configure vDevice to.

   @return                A standard st error type...
                          - ST_NO_ERROR
 */
ST_ErrorCode_t stptiHAL_vDeviceUpdateStreamIDLite(FullHandle_t vDeviceHandle, U32 StreamID)
{
	ST_ErrorCode_t Error = ST_NO_ERROR;

	stptiHAL_vDevice_lite_t *vDevice_p = stptiHAL_GetObjectvDevice_lite_p(vDeviceHandle);

	/* Do we need to change the streamID */
	if (vDevice_p->StreamID != StreamID) {
		U16 Count = 0;
		U16 Pids[256];
		U16 PIDsFound = 0;

		stpti_printf("StreamIDs different - looking up pids....");

		/* Get the current Pids */
		PIDsFound = stptiHAL_LookupPIDsOnSlotsLite(vDeviceHandle, Pids, 256);

		stpti_printf("%d pids found", PIDsFound);

		if (PIDsFound > 0) {
			if (stptiHAL_StreamIDNone != vDevice_p->StreamID) {
				/* Always call the TSInput clear/set pid funtions as the hardware will figure out if it needs to set memory or not */
				/* Clear Pids in the TSInput Pid Filter */
				for (Count = 0; Count < PIDsFound; Count++) {
					stpti_printf("Clearing PID 0x%x", Pids[Count]);
					Error =
					    stptiTSHAL_call(TSInput.TSHAL_TSInputSetClearPid, 0,
							    stptiOBJMAN_GetpDeviceIndex(vDeviceHandle),
							    vDevice_p->StreamID, Pids[Count], FALSE);
					if (ST_NO_ERROR != Error) {
						/* If there is an error it is likely because this input is not configured and Pid memory does not exist yet.
						   This will be taken care of in the configure call later */
						break;
					}
				}
			}

			if (stptiHAL_StreamIDNone != StreamID) {
				/* Clear Pids in the TSInput Pid Filter */
				for (Count = 0; Count < PIDsFound; Count++) {
					stpti_printf("Setting PID 0x%x", Pids[Count]);
					Error =
					    stptiTSHAL_call(TSInput.TSHAL_TSInputSetClearPid, 0,
							    stptiOBJMAN_GetpDeviceIndex(vDeviceHandle), StreamID,
							    Pids[Count], TRUE);
					if (ST_NO_ERROR != Error) {
						/* If there is an error it is likely because this input is not configured and Pid memory does not exist yet.
						   This will be taken care of in the configure call later */
						break;
					}
				}
			}
		}

		/* Update the new streamID */
		vDevice_p->StreamID = StreamID;
	}

	return (ST_NO_ERROR);
}
