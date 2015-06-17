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
   @file   pti_vdevice.c
   @brief  Virtual Device Allocation, Deallocation functions.

   This file implements the functions for creating and destroying virtual devices.  This allows us
   to have appearance of multiple PTI's on one physical PTI.
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
#include "pti_pdevice.h"
#include "pti_vdevice.h"
#include "pti_dbg.h"
#include "pti_slot.h"
#include "pti_filter.h"
#include "pti_swinjector.h"
#include "pti_mailbox.h"

/* MACROS ------------------------------------------------------------------ */
/* Private Constants ------------------------------------------------------- */
/* Private Variables ------------------------------------------------------- */
/* Private Function Prototypes --------------------------------------------- */

/* Although these prototypes are not exported directly they are exported through the API constant below. */
/* Add the definition to pti_hal.h */
ST_ErrorCode_t stptiHAL_vDeviceAllocator(FullHandle_t vDeviceHandle, void *params_p);
ST_ErrorCode_t stptiHAL_vDeviceDeallocator(FullHandle_t vDeviceHandle);
ST_ErrorCode_t stptiHAL_vDeviceDebug(FullHandle_t vDeviceHandle, char *DebugClass, char *String, int *StringSize_p,
				     int MaxStringSize, int Offset, int *EOF_p);
ST_ErrorCode_t stptiHAL_vDeviceLookupSlotForPID(FullHandle_t vDeviceHandle, U16 PIDtoFind,
						FullHandle_t *SlotHandleArray_p, int SlotArraySize,
						int *SlotsMatching);
ST_ErrorCode_t stptiHAL_vDeviceLookupPIDs(FullHandle_t vDeviceHandle, U16 *PIDArray_p, U16 PIDArraySize,
					  U16 *PIDsFound);
ST_ErrorCode_t stptiHAL_vDeviceGetStreamID(FullHandle_t vDeviceHandle, U32 *StreamID_p, BOOL *UseTimerTag_p);
ST_ErrorCode_t stptiHAL_vDeviceSetStreamID(FullHandle_t vDeviceHandle, U32 StreamID, BOOL UseTimerTag);
ST_ErrorCode_t stptiHAL_vDeviceSetEvent(FullHandle_t vDeviceHandle, stptiHAL_EventType_t Event, BOOL Enable);
ST_ErrorCode_t stptiHAL_vDeviceGetTSProtocol(FullHandle_t vDeviceHandle, stptiHAL_TransportProtocol_t *Protocol_p,
					     U8 *PacketSize_p);
ST_ErrorCode_t stptiHAL_vDeviceGetStreamStatistics(FullHandle_t vDeviceHandle,
						   stptiHAL_vDeviceStreamStatistics_t *Statistics_p);
ST_ErrorCode_t stptiHAL_vDeviceResetStreamStatistics(FullHandle_t vDeviceHandle);
ST_ErrorCode_t stptiHAL_vDeviceGetCapability(FullHandle_t vDeviceHandle,
					     stptiHAL_vDeviceConfigStatus_t *ConfigStatus_p);
ST_ErrorCode_t stptiHAL_vDeviceIndexesEnable(FullHandle_t vDeviceHandle, BOOL Enable);
ST_ErrorCode_t stptiHAL_vDeviceFirmwareReset(FullHandle_t vDeviceHandle);
ST_ErrorCode_t stptiHAL_vDevicePowerDown(FullHandle_t vDeviceHandle);
ST_ErrorCode_t stptiHAL_vDevicePowerUp(FullHandle_t vDeviceHandle);
ST_ErrorCode_t stptiHAL_vDeviceFeatureEnable(FullHandle_t vDeviceHandle, stptiHAL_vDeviceFeature_t Feature,
					     BOOL Enable);
ST_ErrorCode_t stptiHAL_vDeviceGetTimer(FullHandle_t vDeviceHandle, U32 *TimerValue_p, U64 *SystemTime_p);

static ST_ErrorCode_t stptiHAL_vDeviceUpdateStreamID(FullHandle_t vDeviceHandle, U32 StreamID, BOOL UseTimerTag);

/* Public Constants ------------------------------------------------------- */

/* Export the API */
const stptiHAL_vDeviceAPI_t stptiHAL_vDeviceAPI = {
	{
		/* Allocator               Associator  Disassociator  Deallocator */
		stptiHAL_vDeviceAllocator, NULL, NULL, stptiHAL_vDeviceDeallocator,
		NULL, NULL
	},
	stptiHAL_vDeviceDebug,
	stptiHAL_vDeviceLookupSlotForPID,
	stptiHAL_vDeviceLookupPIDs,
	stptiHAL_vDeviceGetStreamID,
	stptiHAL_vDeviceSetStreamID,
	stptiHAL_vDeviceSetEvent,
	stptiHAL_vDeviceGetTSProtocol,
	stptiHAL_vDeviceGetStreamStatistics,
	stptiHAL_vDeviceResetStreamStatistics,
	stptiHAL_vDeviceGetCapability,
	stptiHAL_vDeviceIndexesEnable,
	stptiHAL_vDeviceFirmwareReset,
	stptiHAL_vDevicePowerDown,
	stptiHAL_vDevicePowerUp,
	stptiHAL_vDeviceFeatureEnable,
	stptiHAL_vDeviceGetTimer,
};

/* Object Management Functions ------------------------------------------------------------------ */

/**
   @brief  Allocator for vDevice

   This function see if it is the first vDevice on this pDevice and if so will then initialise the
   pDevice (map addresses, start the tp).   It also allocates space for slots and filters (a
   partitioned resource on the pDevice).

   Upon Entry we will be write locked.  Upon exit we must be write locked.

   @param  vDeviceHandle  The handle for the new vDevice.
   @param  params_p       (a pointer to stptiHAL_vDeviceAllocationParams_t) The initialisation
                          parameters for this vDevice.

   @return                A standard st error type...
                          - ST_NO_ERROR
                          - ST_ERROR_BAD_PARAMETER
                          - ST_ERROR_NO_MEMORY       (no space to allocate slots, filters)
 */
ST_ErrorCode_t stptiHAL_vDeviceAllocator(FullHandle_t vDeviceHandle, void *params_p)
{
	ST_ErrorCode_t Error = ST_NO_ERROR;
	stptiHAL_vDeviceAllocationParams_t *vDeviceAllocationParams_p = (stptiHAL_vDeviceAllocationParams_t *) params_p;
	stptiHAL_vDevice_t *vDevice_p = stptiHAL_GetObjectvDevice_p(vDeviceHandle);
	stptiHAL_pDevice_t *pDevice_p = stptiHAL_GetObjectpDevice_p(vDeviceHandle);
	FullHandle_t pDeviceHandle = stptiOBJMAN_ConvertFullObjectHandleToFullpDeviceHandle(vDeviceHandle);

	BOOL ListsAllocated = FALSE;

	/* Already write locked */

	if (params_p != NULL) {
		U32 NumberOfSectionFilters = vDeviceAllocationParams_p->NumberOfSectionFilters;

		stpti_printf("Allocating ObjectType=%u Handle=%08x using vDevice allocator.",
			     (unsigned)stptiOBJMAN_GetObjectType(vDeviceHandle), (unsigned)vDeviceHandle.word);

		/* Allocate partitioned resources */
		if (ST_NO_ERROR == Error) {
			if (stptiSupport_PartitionedResourceAllocPartition
			    (&pDevice_p->PIDTablePartitioning, vDeviceAllocationParams_p->NumberOfSlots,
			     &vDevice_p->PIDTablePartitionID)) {
				STPTI_PRINTF_ERROR("Not enough room to place %d slots.",
						   vDeviceAllocationParams_p->NumberOfSlots);
				Error = ST_ERROR_NO_MEMORY;
			} else {
				stpti_printf("PIDTableOffset is %d.",
					     stptiSupport_PartitionedResourceStart(&pDevice_p->PIDTablePartitioning,
										   vDevice_p->PIDTablePartitionID));
			}

			/* Allocate CAM resource */
			if (NumberOfSectionFilters > 0) {
				if (ST_NO_ERROR == Error) {
					if (stptiSupport_PartitionedResourceAllocPartition
					    (&pDevice_p->FilterCAMPartitioning, NumberOfSectionFilters,
					     &vDevice_p->FilterPartitionID)) {
						STPTI_PRINTF_ERROR("Not enough room to place %d filters.",
								   NumberOfSectionFilters);
						Error = ST_ERROR_NO_MEMORY;
					} else {
						stpti_printf("CAM allocation for %d filters. (%d).",
							     NumberOfSectionFilters,
							     stptiSupport_PartitionedResourceStart(&pDevice_p->FilterCAMPartitioning,
												   vDevice_p->FilterPartitionID));
					}
				}
			}
		}

		/* If this is the first vDevice on this pDevice start the pDevice */
		if (ST_NO_ERROR == Error) {
			int vDeviceCount;
			FullHandle_t vDeviceHandles[MAX_NUMBER_OF_VDEVICES];

			vDeviceCount =
			    stptiOBJMAN_ReturnChildObjects(pDeviceHandle, vDeviceHandles, MAX_NUMBER_OF_VDEVICES,
							   OBJECT_TYPE_VDEVICE);

			if (vDeviceCount == 1) {
				Error = stptiHAL_pDeviceSetPowerStateInternal(pDeviceHandle, stptiHAL_PDEVICE_STARTED);
			}
		}

		/* Setup TP structure pointers and the list pointers for managing TP resources */
		if (ST_NO_ERROR == Error) {
			int vDeviceIndex = stptiOBJMAN_GetvDeviceIndex(vDeviceHandle);
			/* Copy over the whole block for simplicity */
			memcpy(&vDevice_p->TP_Interface, &pDevice_p->TP_Interface, sizeof(stptiTP_Interface_t));

			/* And update the structures to indication sizes for just this vdevice */
			vDevice_p->TP_Interface.NumberOfSlots = vDeviceAllocationParams_p->NumberOfSlots;
			vDevice_p->TP_Interface.NumberOfSectionFilters =
			    vDeviceAllocationParams_p->NumberOfSectionFilters;

			/* And update the structures that are relative */
			vDevice_p->TP_Interface.vDeviceInfo_p += vDeviceIndex;
			vDevice_p->TP_Interface.FilterCAMTables_p += vDeviceIndex;
			vDevice_p->TP_Interface.PIDTable_p +=
			    stptiSupport_PartitionedResourceStart(&pDevice_p->PIDTablePartitioning,
								  vDevice_p->PIDTablePartitionID);
			vDevice_p->TP_Interface.PIDSlotMappingTable_p +=
			    stptiSupport_PartitionedResourceStart(&pDevice_p->PIDTablePartitioning,
								  vDevice_p->PIDTablePartitionID);

			vDevice_p->TP_Interface.FilterCAMRegion_p +=
			    stptiSupport_PartitionedResourceStart(&pDevice_p->FilterCAMPartitioning,
								  vDevice_p->FilterPartitionID);

			vDevice_p->ProprietaryFilterIndex = vDeviceIndex;
			vDevice_p->ProprietaryFilterAllocated = FALSE;
			vDevice_p->TP_Interface.ProprietaryFilters_p += vDeviceIndex;

			/* Clear slots (defensive - should be unnecessary) */
			stptiHAL_SlotPIDTableInitialise(vDevice_p->TP_Interface.PIDTable_p,
							vDevice_p->TP_Interface.PIDSlotMappingTable_p,
							vDevice_p->TP_Interface.NumberOfSlots);

			/* Clear the CAMS to a default state (must be done after FilterCAMTables_p is set) */
			stptiHAL_FilterTPEntryInitialise(vDevice_p,
							 stptiSupport_PartitionedResourceStart(&pDevice_p->FilterCAMPartitioning,
											       vDevice_p->FilterPartitionID),
							 vDevice_p->TP_Interface.NumberOfSectionFilters);

			/* Setup the vDevice lists to point to the relevant lists in the pDevice */
			vDevice_p->TSInputHandles = &pDevice_p->FrontendHandles;
			vDevice_p->SoftwareInjectorHandles = &pDevice_p->SoftwareInjectorHandles;
			vDevice_p->SlotHandles = &pDevice_p->SlotHandles;
			vDevice_p->BufferHandles = &pDevice_p->BufferHandles;
			vDevice_p->IndexHandles = &pDevice_p->IndexHandles;

			/* Default for packet arrival timestamp */
			vDevice_p->UseTimerTag = FALSE;

			/* Set initial power state */
			vDevice_p->PoweredOn = TRUE;

			if (ST_NO_ERROR != stptiOBJMAN_AllocateList(&vDevice_p->SectionFilterHandles)) {
				STPTI_PRINTF_ERROR("Unable to allocate list for SectionFilterHandles");
				Error = ST_ERROR_NO_MEMORY;
			} else if (ST_NO_ERROR != stptiOBJMAN_AllocateList(&vDevice_p->PESStreamIDFilterHandles)) {
				STPTI_PRINTF_ERROR("Unable to allocate list for PESStreamIDFilterHandles");
				Error = ST_ERROR_NO_MEMORY;
			} else if (ST_NO_ERROR != stptiOBJMAN_AllocateList(&vDevice_p->PidIndexes)) {
				STPTI_PRINTF_ERROR("Unable to allocate list for PidIndexes");
				Error = ST_ERROR_NO_MEMORY;
			}

			ListsAllocated = TRUE;
		}

		if (ST_NO_ERROR == Error) {
			/* Copy accross vDevice configuration */
			volatile stptiTP_vDeviceInfo_t *vDeviceInfo_p = vDevice_p->TP_Interface.vDeviceInfo_p; /* actual TP structure to copy into */

			stptiHAL_vDeviceTPEntryInitialise(vDeviceInfo_p);

			vDevice_p->TSProtocol = vDeviceAllocationParams_p->TSProtocol;
			vDevice_p->PacketSize = vDeviceAllocationParams_p->PacketSize;
			vDevice_p->StreamID = vDeviceAllocationParams_p->StreamID;
			vDevice_p->EventPtrs = vDeviceAllocationParams_p->EventFuncPtrs;
			vDevice_p->TSInputHandle = stptiOBJMAN_NullHandle;

			vDevice_p->SlotsAllocated = 0;

			vDevice_p->ForceDiscardSectionOnCRCError =
			    vDeviceAllocationParams_p->ForceDiscardSectionOnCRCError;

			switch (vDeviceAllocationParams_p->TSProtocol) {
			default:
			case stptiHAL_DVB_TS_PROTOCOL:
				stptiHAL_pDeviceXP70Write(&vDeviceInfo_p->mode, PACKET_FORMAT_DVB);
				break;
			}

			stptiHAL_pDeviceXP70Write(&vDeviceInfo_p->pid_filter_base,
						  stptiSupport_PartitionedResourceStart(&pDevice_p->PIDTablePartitioning,
											vDevice_p->PIDTablePartitionID));
			stptiHAL_pDeviceXP70Write(&vDeviceInfo_p->pid_filter_size,
						  stptiSupport_PartitionedResourceSize(&pDevice_p->PIDTablePartitioning,
										       vDevice_p->PIDTablePartitionID));
			stptiHAL_pDeviceXP70Write(&vDeviceInfo_p->stream_tag,
						  (U16) (vDeviceAllocationParams_p->StreamID & stptiTSHAL_TAG_MASK));

			/* The vDevice's event_mask is used to gate events, we allow (enable) all events which are not
			   vDevice based events so that they are not prevented from being raised */
			stptiHAL_pDeviceXP70Write(&vDeviceInfo_p->event_mask,
						  EVENT_MASK_ALL_EVENTS & (~EVENT_MASK_VDEVICE_ENABLED_EVENTS));
			stptiHAL_pDeviceXP70BitSet(&vDeviceInfo_p->event_mask, STATUS_BLK_STATUS_BLK_OVERFLOW);	/* Always enable Status Block Overflow */

			/* reset stream statistics record */
			memset(&vDevice_p->StreamStatisticsUponReset, 0, sizeof(stptiHAL_vDeviceStreamStatistics_t));

		}

		/* Mechanism to tidy up should errors have occured */
		if (ST_NO_ERROR != Error) {
			if (ListsAllocated) {
				stptiOBJMAN_DeallocateList(&vDevice_p->SectionFilterHandles);
				stptiOBJMAN_DeallocateList(&vDevice_p->PESStreamIDFilterHandles);
				stptiOBJMAN_DeallocateList(&vDevice_p->PidIndexes);
			}

			/* Release any reserved regions in the partitioned resources */
			stptiSupport_PartitionedResourceFreePartition(&pDevice_p->PIDTablePartitioning,
								      vDevice_p->PIDTablePartitionID);
			stptiSupport_PartitionedResourceFreePartition(&pDevice_p->FilterCAMPartitioning,
								      vDevice_p->FilterPartitionID);
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
ST_ErrorCode_t stptiHAL_vDeviceDeallocator(FullHandle_t vDeviceHandle)
{
	ST_ErrorCode_t Error = ST_NO_ERROR;

	stptiHAL_pDevice_t *pDevice_p = stptiHAL_GetObjectpDevice_p(vDeviceHandle);
	stptiHAL_vDevice_t *vDevice_p = stptiHAL_GetObjectvDevice_p(vDeviceHandle);
	FullHandle_t pDeviceHandle = stptiOBJMAN_ConvertFullObjectHandleToFullpDeviceHandle(vDeviceHandle);

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
	stptiOBJMAN_DeallocateList(&vDevice_p->SectionFilterHandles);
	stptiOBJMAN_DeallocateList(&vDevice_p->PESStreamIDFilterHandles);
	stptiOBJMAN_DeallocateList(&vDevice_p->PidIndexes);

	/* Free partitioned resource regions (Slots, Filters). */
	/* Note this can create a fragmentation issue, unless you either allocate the same # of filters/slots
	   again, or work backwardsd deallocate from your last vDevice.  i.e. There is no defrag for partitioned
	   resources (as we can't move resources when the demux is active). */
	stptiSupport_PartitionedResourceFreePartition(&pDevice_p->PIDTablePartitioning, vDevice_p->PIDTablePartitionID);
	stptiSupport_PartitionedResourceFreePartition(&pDevice_p->FilterCAMPartitioning, vDevice_p->FilterPartitionID);
	/* If this is the last vDevice on this pDevice put the pDevice to sleep */
	if (ST_NO_ERROR == Error) {
		int vDeviceCount;
		FullHandle_t vDeviceHandles[MAX_NUMBER_OF_VDEVICES];

		vDeviceCount =
		    stptiOBJMAN_ReturnChildObjects(pDeviceHandle, vDeviceHandles, MAX_NUMBER_OF_VDEVICES,
						   OBJECT_TYPE_VDEVICE);

		if (vDeviceCount == 1) {
			Error = stptiHAL_pDeviceSetPowerStateInternal(pDeviceHandle, stptiHAL_PDEVICE_SLEEPING);
		}
	}

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
ST_ErrorCode_t stptiHAL_vDeviceDebug(FullHandle_t vDeviceHandle, char *DebugClass, char *String, int *StringSize_p,
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
			    stptiHAL_DebugAsFile(vDeviceHandle, DebugClass, String, MaxStringSize, Offset, EOF_p);
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
ST_ErrorCode_t stptiHAL_vDeviceLookupSlotForPID(FullHandle_t vDeviceHandle, U16 PIDtoFind,
						FullHandle_t *SlotHandleArray_p, int SlotArraySize, int *SlotsMatching)
{
	stptiSupport_AccessLockState_t LocalLockState = stptiSupport_UNLOCKED;

	stptiOBJMAN_ReadLock(vDeviceHandle, &LocalLockState);
	{
		*SlotsMatching = stptiHAL_SlotLookupPID(vDeviceHandle, PIDtoFind, SlotHandleArray_p, SlotArraySize);
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
ST_ErrorCode_t stptiHAL_vDeviceLookupPIDs(FullHandle_t vDeviceHandle, U16 *PIDArray_p, U16 PIDArraySize,
					  U16 *PIDsFound)
{
	stptiSupport_AccessLockState_t LocalLockState = stptiSupport_UNLOCKED;

	stptiOBJMAN_ReadLock(vDeviceHandle, &LocalLockState);
	{
		/* We need to go through all the slot handles and then get the pid */
		*PIDsFound = stptiHAL_LookupPIDsOnSlots(vDeviceHandle, PIDArray_p, PIDArraySize);
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
ST_ErrorCode_t stptiHAL_vDeviceGetStreamID(FullHandle_t vDeviceHandle, U32 *StreamID_p, BOOL *UseTimerTag_p)
{
	stptiSupport_AccessLockState_t LocalLockState = stptiSupport_UNLOCKED;

	stptiOBJMAN_ReadLock(vDeviceHandle, &LocalLockState);
	{
		stptiHAL_vDevice_t *vDevice_p = stptiHAL_GetObjectvDevice_p(vDeviceHandle);
		*StreamID_p = vDevice_p->StreamID;
		*UseTimerTag_p = vDevice_p->UseTimerTag;
	}
	stptiOBJMAN_Unlock(vDeviceHandle, &LocalLockState);

	return (ST_NO_ERROR);
}

/**
   @brief  Set the StreamID for a specific vDevice

   This function set the streamID on this vDevice.

   @param  vDeviceHandle  The handle for the vDevice to be configured.
   @param  StreamID       StreamID to configure vDevice to.
   @param  UseTimerTag    Select packet arrival time source for this vDevice.

   @return                A standard st error type...
                          - ST_NO_ERROR
 */
ST_ErrorCode_t stptiHAL_vDeviceSetStreamID(FullHandle_t vDeviceHandle, U32 StreamID, BOOL UseTimerTag)
{
	ST_ErrorCode_t Error = ST_NO_ERROR;
	stptiSupport_AccessLockState_t LocalLockState = stptiSupport_UNLOCKED;

	stptiOBJMAN_WriteLock(vDeviceHandle, &LocalLockState);
	{
		Error = stptiHAL_vDeviceUpdateStreamID(vDeviceHandle, StreamID, UseTimerTag);
	}
	stptiOBJMAN_Unlock(vDeviceHandle, &LocalLockState);

	return (Error);
}

/**
   @brief  Enable/Disable a specific event vDevice

   This function returns the streamID on this vDevice.

   @param  vDeviceHandle  The handle for the vDevice to be configured.
   @param  StreamID       StreamID to configure vDevice to.

   @return                A standard st error type...
                          - ST_ERROR_BAD_PARAMETER
                          - ST_NO_ERROR
 */
ST_ErrorCode_t stptiHAL_vDeviceSetEvent(FullHandle_t vDeviceHandle, stptiHAL_EventType_t Event, BOOL Enable)
{
	ST_ErrorCode_t Error = ST_NO_ERROR;
	stptiSupport_AccessLockState_t LocalLockState = stptiSupport_UNLOCKED;

	stptiOBJMAN_WriteLock(vDeviceHandle, &LocalLockState);
	{
		stptiHAL_vDevice_t *vDevice_p = stptiHAL_GetObjectvDevice_p(vDeviceHandle);

		if (Event < stptiHAL_END_EVENT) {
			stptiTP_StatusBlk_Flags_t Flag = 0;

			switch (Event) {
			case (stptiHAL_BUFFER_OVERFLOW_EVENT):
				Flag = STATUS_BLK_BUFFER_OVERFLOW;
				break;
			case (stptiHAL_CC_ERROR_EVENT):
				Flag = STATUS_BLK_CC_ERROR;
				Flag |= STATUS_BLK_SECONDARY_PID_CC_ERROR;
				break;
			case (stptiHAL_SCRAMBLE_TOCLEAR_EVENT):
				Flag = STATUS_BLK_SCRAMBLE_TOCLEAR;
				break;
			case (stptiHAL_CLEAR_TOSCRAM_EVENT):
				Flag = STATUS_BLK_CLEAR_TOSCRAMBLE;
				break;
			case (stptiHAL_INVALID_PARAMETER_EVENT):
				Flag = STATUS_BLK_INVALID_PARAMETER;
				break;
			case (stptiHAL_TRANSPORT_ERROR_EVENT):
				Flag = STATUS_BLK_TRANSPORT_ERROR_FLAG;
				break;
			case (stptiHAL_PCR_RECEIVED_EVENT):
				Flag = STATUS_BLK_PCR_RECEIVED;
				break;
			case (stptiHAL_PES_ERROR_EVENT):
				Flag = STATUS_BLK_PES_ERROR;
				break;
			case (stptiHAL_SECTIONS_DISCARDED_ON_CRC_EVENT):
				Flag = STATUS_BLK_SECTION_CRC_DISCARD;
				break;
			case (stptiHAL_INTERRUPT_FAIL_EVENT):
				Flag = STATUS_BLK_STATUS_BLK_OVERFLOW;
				break;
			case (stptiHAL_DATA_ENTRY_COMPLETE_EVENT):
				Flag = STATUS_BLK_DATA_ENTRY_COMPLETE;
				break;
			case (stptiHAL_MARKER_ERROR_EVENT):
				Flag = STATUS_BLK_MARKER_ERROR;
				break;
			case (stptiHAL_INVALID_SECONDARY_PID_PACKET_EVENT):
				Flag = STATUS_BLK_SECONDARY_PID_DISCARDED;
				break;
			case (stptiHAL_INVALID_DESCRAMBLE_KEY_EVENT):
			default:
				Error = ST_ERROR_BAD_PARAMETER;
				break;
			}

			if (ST_NO_ERROR == Error) {
				if (Enable) {
					stptiHAL_pDeviceXP70BitSet(&vDevice_p->TP_Interface.vDeviceInfo_p->event_mask,
								   Flag);
				} else {
					stptiHAL_pDeviceXP70BitClear(&vDevice_p->TP_Interface.vDeviceInfo_p->event_mask,
								     Flag);
				}
			}
		} else {
			Error = ST_ERROR_BAD_PARAMETER;
		}
	}
	stptiOBJMAN_Unlock(vDeviceHandle, &LocalLockState);

	return (Error);
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
ST_ErrorCode_t stptiHAL_vDeviceGetTSProtocol(FullHandle_t vDeviceHandle, stptiHAL_TransportProtocol_t *Protocol_p,
					     U8 *PacketSize_p)
{
	stptiSupport_AccessLockState_t LocalLockState = stptiSupport_UNLOCKED;

	stptiOBJMAN_WriteLock(vDeviceHandle, &LocalLockState);
	{
		stptiHAL_vDevice_t *vDevice_p = stptiHAL_GetObjectvDevice_p(vDeviceHandle);

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

   This function returns the number of packets, the number of packets with the transport error bit
   set and the number of packets with a cc error.

   @param  vDeviceHandle  The handle for the vDevice to be examined
   @param  Statistics_p   (return) A pointer to a structure to return the various counts

   @return                A standard st error type...
                          - ST_NO_ERROR
 */
ST_ErrorCode_t stptiHAL_vDeviceGetStreamStatistics(FullHandle_t vDeviceHandle,
						   stptiHAL_vDeviceStreamStatistics_t *Statistics_p)
{
	stptiSupport_AccessLockState_t LocalLockState = stptiSupport_UNLOCKED;

	stptiOBJMAN_WriteLock(vDeviceHandle, &LocalLockState);
	{
		stptiHAL_vDevice_t *vDevice_p = stptiHAL_GetObjectvDevice_p(vDeviceHandle);
		stptiHAL_pDevice_t *pDevice_p = stptiHAL_GetObjectpDevice_p(vDeviceHandle);

		uint32_t notidle;
		uint32_t idle;

		Statistics_p->PacketCount =
		    stptiHAL_pDeviceXP70Read(&vDevice_p->TP_Interface.vDeviceInfo_p->input_packet_count) -
		    vDevice_p->StreamStatisticsUponReset.PacketCount;
		Statistics_p->TransportErrorCount =
		    stptiHAL_pDeviceXP70Read(&vDevice_p->TP_Interface.vDeviceInfo_p->transport_error_count) -
		    vDevice_p->StreamStatisticsUponReset.TransportErrorCount;
		Statistics_p->CCErrorCount =
		    stptiHAL_pDeviceXP70Read(&vDevice_p->TP_Interface.vDeviceInfo_p->cc_error_count) -
		    vDevice_p->StreamStatisticsUponReset.CCErrorCount;
		Statistics_p->BufferOverflowCount =
		    stptiHAL_pDeviceXP70Read(&vDevice_p->TP_Interface.vDeviceInfo_p->buffer_overflow_count) -
		    vDevice_p->StreamStatisticsUponReset.BufferOverflowCount;

		notidle = stptiHAL_pDeviceXP70Read(&pDevice_p->TP_Interface.pDeviceInfo_p->AvgNotIdleCount);
		idle = stptiHAL_pDeviceXP70Read(&pDevice_p->TP_Interface.pDeviceInfo_p->AvgIdleCount);

		if ((idle + notidle) != 0) {
			Statistics_p->Utilization = (100 * notidle) / (idle + notidle);
		} else {
			Statistics_p->Utilization = 0;
		}

	}
	stptiOBJMAN_Unlock(vDeviceHandle, &LocalLockState);

	return (ST_NO_ERROR);
}

ST_ErrorCode_t stptiHAL_vDeviceResetStreamStatistics(FullHandle_t vDeviceHandle)
{
	stptiSupport_AccessLockState_t LocalLockState = stptiSupport_UNLOCKED;

	stptiOBJMAN_WriteLock(vDeviceHandle, &LocalLockState);
	{
		stptiHAL_vDevice_t *vDevice_p = stptiHAL_GetObjectvDevice_p(vDeviceHandle);

		vDevice_p->StreamStatisticsUponReset.PacketCount =
		    stptiHAL_pDeviceXP70Read(&vDevice_p->TP_Interface.vDeviceInfo_p->input_packet_count);
		vDevice_p->StreamStatisticsUponReset.TransportErrorCount =
		    stptiHAL_pDeviceXP70Read(&vDevice_p->TP_Interface.vDeviceInfo_p->transport_error_count);
		vDevice_p->StreamStatisticsUponReset.CCErrorCount =
		    stptiHAL_pDeviceXP70Read(&vDevice_p->TP_Interface.vDeviceInfo_p->cc_error_count);
		vDevice_p->StreamStatisticsUponReset.BufferOverflowCount =
		    stptiHAL_pDeviceXP70Read(&vDevice_p->TP_Interface.vDeviceInfo_p->buffer_overflow_count);
	}
	stptiOBJMAN_Unlock(vDeviceHandle, &LocalLockState);

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
ST_ErrorCode_t stptiHAL_vDeviceGetCapability(FullHandle_t vDeviceHandle,
					     stptiHAL_vDeviceConfigStatus_t *ConfigStatus_p)
{
	stptiSupport_AccessLockState_t LocalLockState = stptiSupport_UNLOCKED;

	stptiOBJMAN_WriteLock(vDeviceHandle, &LocalLockState);
	{
		stptiHAL_vDevice_t *vDevice_p = stptiHAL_GetObjectvDevice_p(vDeviceHandle);

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

   This function enables or disables indexing on raw slots.  It is the master switch which starts
   indexing on all slots.

   @param  vDeviceHandle            The handle for the vDevice
   @param  Enable                   TRUE if enabling (starting) indexing, FALSE if disabling
                                    (stopping)

   @return                          A standard st error type...
                                    - ST_NO_ERROR
 */
ST_ErrorCode_t stptiHAL_vDeviceIndexesEnable(FullHandle_t vDeviceHandle, BOOL Enable)
{
	stptiSupport_AccessLockState_t LocalLockState = stptiSupport_UNLOCKED;

	stptiOBJMAN_WriteLock(vDeviceHandle, &LocalLockState);
	{
		stptiHAL_vDevice_t *vDevice_p = stptiHAL_GetObjectvDevice_p(vDeviceHandle);
		if (Enable) {
			stptiHAL_pDeviceXP70BitSet(&vDevice_p->TP_Interface.vDeviceInfo_p->flags,
						   VDEVICE_ENABLE_RAW_SLOT_INDEXES);
		} else {
			stptiHAL_pDeviceXP70BitClear(&vDevice_p->TP_Interface.vDeviceInfo_p->flags,
						     VDEVICE_ENABLE_RAW_SLOT_INDEXES);
		}
	}
	stptiOBJMAN_Unlock(vDeviceHandle, &LocalLockState);

	return (ST_NO_ERROR);
}

ST_ErrorCode_t stptiHAL_vDeviceFirmwareReset(FullHandle_t vDeviceHandle)
{
	ST_ErrorCode_t Error = ST_NO_ERROR;
	stptiSupport_AccessLockState_t LocalLockState = stptiSupport_UNLOCKED;

	stptiOBJMAN_WriteLock(vDeviceHandle, &LocalLockState);
	{
		Error = stptiHAL_pDeviceReset(vDeviceHandle);
	}
	stptiOBJMAN_Unlock(vDeviceHandle, &LocalLockState);

	return (Error);
}

/**
   @brief Power-down this vDevice

   Set this vDevice into a power-down state, where it is safe to power-down any
   underlying pDevice

   @param  vDeviceHandle            The handle for the vDevice

   @return                          A standard st error type...
                                    - ST_NO_ERROR
 */
ST_ErrorCode_t stptiHAL_vDevicePowerDown(FullHandle_t vDeviceHandle)
{
	ST_ErrorCode_t Error = ST_NO_ERROR;
	stptiSupport_AccessLockState_t LocalLockState = stptiSupport_UNLOCKED;
	FullHandle_t SoftwareInjectorHandle = stptiOBJMAN_NullHandle;
	stptiHAL_vDevice_t *vDevice_p = stptiHAL_GetObjectvDevice_p(vDeviceHandle);

	/* First we need to disable any inputs */
	stptiOBJMAN_WriteLock(vDeviceHandle, &LocalLockState);

	if (vDevice_p->PoweredOn) {
		int index;
		Object_t *AssocdObject_p;
		unsigned int SlotCount = 0;
		stptiHAL_Slot_t *Slot_p;
		FullHandle_t SlotHandleArray[64];

		/* Decouple from any associated TSInput */
		vDevice_p->SavedStreamID = vDevice_p->StreamID;
		Error =
		    stptiHAL_vDeviceUpdateStreamID(vDeviceHandle, (U32) stptiHAL_StreamIDNone, vDevice_p->UseTimerTag);
		if (ST_NO_ERROR != Error) {
			STPTI_PRINTF_ERROR("Unable to update StreamID for vDevice [Handle: %08x]", vDeviceHandle.word);
			Error = ST_NO_ERROR;	/* Carry on even if error - not much we can do about it */
		}

		/* Check for a software injector */
		stptiOBJMAN_FirstInList(vDevice_p->SoftwareInjectorHandles, (void *)&AssocdObject_p, &index);
		while (index >= 0) {
			FullHandle_t ObjectHandle = stptiOBJMAN_ObjectPointerToHandle(AssocdObject_p);
			if (stptiOBJMAN_GetvDeviceIndex(vDeviceHandle) == stptiOBJMAN_GetvDeviceIndex(ObjectHandle)) {
				SoftwareInjectorHandle = ObjectHandle;	/* Store Software Injector Handle for checking it later */
				break;
			}
			stptiOBJMAN_NextInList(vDevice_p->SoftwareInjectorHandles, (void *)&AssocdObject_p, &index);
		}

		stptiOBJMAN_Unlock(vDeviceHandle, &LocalLockState);

		/* Abort software injection (has to be done outside an access lock) */
		if (!stptiOBJMAN_IsNullHandle(SoftwareInjectorHandle)) {
			stptiHAL_SoftwareInjectorAbortInternal(SoftwareInjectorHandle);
		}

		/* wait for 1ms for any packets to clear, events to be handled, and signals raised. */
		udelay(1000);

		stptiOBJMAN_WriteLock(vDeviceHandle, &LocalLockState);

		/* Reset all slots on this vDevice.  Unfortunately this has to be done in
		 * blocks to minimise delay caused by TP synchronisation */
		stptiOBJMAN_FirstInList(&vDevice_p->PidIndexes, (void *)&Slot_p, &index);
		while (index >= 0) {
			SlotHandleArray[SlotCount++] = stptiOBJMAN_ObjectPointerToHandle(Slot_p);

			stptiOBJMAN_NextInList(&vDevice_p->PidIndexes, (void *)&Slot_p, &index);
			if (SlotCount >= sizeof(SlotHandleArray) / sizeof(FullHandle_t) || index < 0) {
				stptiHAL_SlotResetState(SlotHandleArray, SlotCount, FALSE);
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
ST_ErrorCode_t stptiHAL_vDevicePowerUp(FullHandle_t vDeviceHandle)
{
	stptiSupport_AccessLockState_t LocalLockState = stptiSupport_UNLOCKED;
	ST_ErrorCode_t Error = ST_NO_ERROR;
	stptiHAL_vDevice_t *vDevice_p = stptiHAL_GetObjectvDevice_p(vDeviceHandle);

	stptiOBJMAN_WriteLock(vDeviceHandle, &LocalLockState);

	if (!vDevice_p->PoweredOn) {
		/* Restore saved stream id */
		Error = stptiHAL_vDeviceUpdateStreamID(vDeviceHandle, vDevice_p->SavedStreamID, vDevice_p->UseTimerTag);
		vDevice_p->SavedStreamID = (U32) stptiHAL_StreamIDNone;

		vDevice_p->PoweredOn = TRUE;
	}

	stptiOBJMAN_Unlock(vDeviceHandle, &LocalLockState);

	return Error;
}

/**
   @brief  Set specific features for the vDevice

   This function sets specific features for the vDevice as a whole.  Currently it just controls
   discarding of duplicate packets - a feature of DVB which is often undesirable as it can result
   in loss of data when splicing streams.

   @param  vDeviceHandle            The handle for the vDevice
   @param  Feature                  Feature to adjust
   @param  Enable                   TRUE if feature should be Enabled

   @return                          A standard st error type...
                                    - ST_NO_ERROR
 */
ST_ErrorCode_t stptiHAL_vDeviceFeatureEnable(FullHandle_t vDeviceHandle, stptiHAL_vDeviceFeature_t Feature, BOOL Enable)
{
	ST_ErrorCode_t Error = ST_NO_ERROR;
	stptiSupport_AccessLockState_t LocalLockState = stptiSupport_UNLOCKED;

	stptiOBJMAN_WriteLock(vDeviceHandle, &LocalLockState);
	{
		stptiHAL_vDevice_t *vDevice_p = stptiHAL_GetObjectvDevice_p(vDeviceHandle);

		switch (Feature) {
		case stptiHAL_VDEVICE_DISCARD_DUPLICATE_PKTS:
			stptiHAL_pDeviceXP70ReadModifyWrite(&vDevice_p->TP_Interface.vDeviceInfo_p->flags,
							    VDEVICE_DISCARD_DUPLICATE_PKTS,
							    (Enable ? VDEVICE_DISCARD_DUPLICATE_PKTS : 0));
			break;

		default:
			Error = ST_ERROR_BAD_PARAMETER;
			break;
		}
	}
	stptiOBJMAN_Unlock(vDeviceHandle, &LocalLockState);

	return (Error);
}

/**
   @brief  Return the vDevice parent pDevice timer value

   This function enables the use of the internal transport processor's
   TCU timer setup in the TP

   @param  vDeviceHandle            The handle for the vDevice
   @param  TimerValue_p             Place to store timer value
   @param  SystemTime_p             Place to store current system time

   @return                          A standard st error type...
                                    - ST_NO_ERROR
 */
ST_ErrorCode_t stptiHAL_vDeviceGetTimer(FullHandle_t vDeviceHandle, U32 *TimerValue_p, U64 *SystemTime_p)
{
	ST_ErrorCode_t Error = ST_NO_ERROR;
	struct timespec ts;
	ktime_t ktime;

	stptiSupport_AccessLockState_t LocalLockState = stptiSupport_UNLOCKED;
	DEFINE_SPINLOCK(LocalLock);
	unsigned long Flags;

	stptiOBJMAN_WriteLock(vDeviceHandle, &LocalLockState);
	{
		stptiHAL_pDevice_t *pDevice_p = stptiHAL_GetObjectpDevice_p(vDeviceHandle);
		stptiHAL_vDevice_t *vDevice_p = stptiHAL_GetObjectvDevice_p(vDeviceHandle);

		if (vDevice_p->UseTimerTag == FALSE) {
			Error = ST_ERROR_FEATURE_NOT_SUPPORTED;
		} else {
			/* Disable interrupts to avoid deschedule between reading device
			 * time and system time */
			spin_lock_irqsave(&LocalLock, Flags);
			*TimerValue_p = stptiHAL_pDeviceXP70Read(pDevice_p->TP_MappedAddresses.TimerCounter_p);
			getrawmonotonic(&ts);
			ktime = timespec_to_ktime(ts);
			*SystemTime_p = ktime_to_us(ktime);
			spin_unlock_irqrestore(&LocalLock, Flags);
		}
	}
	stptiOBJMAN_Unlock(vDeviceHandle, &LocalLockState);

	return (Error);
}

/* Supporting Functions ------------------------------------------------------------------------- */

/**
   @brief  Initialise a vDeviceInfo entry in the Transport Processor

   This function initialises a specified entry in the Transport Processors vDevice table.

   @param  vDeviceInfo_p  Pointer to the vDevice info entry to be initialised.

   @return                A standard st error type...
                          - ST_NO_ERROR
 */
void stptiHAL_vDeviceTPEntryInitialise(volatile stptiTP_vDeviceInfo_t *vDeviceInfo_p)
{
	stptiHAL_pDeviceXP70Memset((void *)vDeviceInfo_p, 0, sizeof(stptiTP_vDeviceInfo_t));
	stptiHAL_pDeviceXP70Write(&vDeviceInfo_p->stream_tag, 0xFFFF);	/* not matching value */
	stptiHAL_pDeviceXP70Write(&vDeviceInfo_p->wildcard_slot_index, 0xFFFF);	/* no wildcard slot */
	stptiHAL_pDeviceXP70Write(&vDeviceInfo_p->DataEntry.EntryState, DATA_ENTRY_STATE_EMPTY);
	stptiHAL_pDeviceXP70Write(&vDeviceInfo_p->DataEntry.EntrySlotIndex, 0xFFFF);
}

void stptiHAL_vDeviceTranslateTag(U32 TagWord0, U32 TagWord1, U32 * Clk27MHzDiv300Bit32_p,
				  U32 * Clk27MHzDiv300Bit31to0_p, U16 * Clk27MHzModulus300_p)
{
	if ((TagWord0 & 0xFFFF) == 0xFFFF) {
		/* DLNA TTS Tag */
		/* Need to Endian swap (TTS count is Big Endian) */
		U32 Counter =
		    (TagWord1 & 0x000000FF) << 24 | (TagWord1 & 0x0000FF00) << 8 | (TagWord1 & 0x00FF0000) >> 8 |
		    (TagWord1 & 0xFF000000) >> 24;
		*Clk27MHzDiv300Bit32_p = 0;
		*Clk27MHzDiv300Bit31to0_p = Counter / 300;
		*Clk27MHzModulus300_p = Counter % 300;
	} else {
		/* STFE Tag */
		*Clk27MHzDiv300Bit32_p = (TagWord1 & 0x200) >> 9;
		*Clk27MHzDiv300Bit31to0_p = ((TagWord1 & 0x1FF) << 23) | (TagWord0 >> 9);
		*Clk27MHzModulus300_p = (U16) (TagWord0 & 0x1FF);
	}
}

/**
   @brief  Set the StreamID for a specific vDevice

   This function set the streamID on this vDevice.

   @param  vDeviceHandle  The handle for the vDevice to be configured.
   @param  StreamID       StreamID to configure vDevice to.
   @param  UseTimerTag    Select packet arrival time source for this vDevice.

   @return                A standard st error type...
                          - ST_NO_ERROR
 */
ST_ErrorCode_t stptiHAL_vDeviceUpdateStreamID(FullHandle_t vDeviceHandle, U32 StreamID, BOOL UseTimerTag)
{
	ST_ErrorCode_t Error = ST_NO_ERROR;

	stptiHAL_vDevice_t *vDevice_p = stptiHAL_GetObjectvDevice_p(vDeviceHandle);

	/* Do we need to change the streamID */
	if (vDevice_p->StreamID != StreamID) {
		U16 Count = 0;
		U16 Pids[256];
		U16 PIDsFound = 0;

		stpti_printf("StreamIDs different - looking up pids....");

		/* Get the current Pids */
		PIDsFound = stptiHAL_LookupPIDsOnSlots(vDeviceHandle, Pids, 256);

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

		/* If required set whether the vDevice uses STFE or TCU TP packet arrival timestamps on this vDevice */
		if (vDevice_p->UseTimerTag != UseTimerTag) {
			stptiHAL_pDeviceXP70Write(&vDevice_p->TP_Interface.vDeviceInfo_p->use_tcu_timestamp,
						  (U16) UseTimerTag);
			vDevice_p->UseTimerTag = UseTimerTag;
		}

		/* Set the DMEM structure to the New tag */
		stptiHAL_pDeviceXP70Write(&vDevice_p->TP_Interface.vDeviceInfo_p->stream_tag, (U16) (StreamID & stptiTSHAL_TAG_MASK));	/* actual TP structure to copy into */
	}

	return (ST_NO_ERROR);
}

/**
   @brief  Grow the pid table partition for a specific vDevice.

   This function tries to increase the partition size for the pid and slot tables.
   This may result in the entire contents of the tables being remade.
   If this happens the TP will need to manage when the tables are updated to avoid
   any race conditions in the processing of transport packets.

   @param  vDeviceHandle  The handle for the vDevice to have its pid table grown.

   @return                A standard st error type...
                          - ST_NO_ERROR
                          - ST_ERROR_NO_MEMORY (unable to grow table)

 */
ST_ErrorCode_t stptiHAL_vDeviceGrowPIDTable(FullHandle_t vDeviceHandle)
{
	ST_ErrorCode_t Error = ST_NO_ERROR;
	stptiHAL_pDevice_t *pDevice_p = stptiHAL_GetObjectpDevice_p(vDeviceHandle);
	stptiHAL_vDevice_t *vDevice_p = stptiHAL_GetObjectvDevice_p(vDeviceHandle);

	/* First call resizepartition. This is the simplest form of increasing the
	   numbers of entries. This only works if there are gaps/space above the
	   current partition.
	   1/ It will work if this is the only demux instanciated.
	   2/ It will work if its the last demux instanciated and the maximum
	   number of entries are not used.
	   3/ Will also work if the demux above "pid table" resource has been
	   freed leaving a gap.

	   If this step works then there is nothing further to do other than
	   update the number of pidtable entries in the vdevice structure.
	 */
	if (!(stptiSupport_PartitionedResourceResizePartition(&pDevice_p->PIDTablePartitioning,
							      vDevice_p->PIDTablePartitionID,
							      vDevice_p->TP_Interface.NumberOfSlots + 1))) {
		vDevice_p->TP_Interface.NumberOfSlots++;

		/* We need to set this as we are not performing a streamer transfer */
		stptiHAL_pDeviceXP70Write(&vDevice_p->TP_Interface.vDeviceInfo_p->pid_filter_size,
					  stptiSupport_PartitionedResourceSize(&pDevice_p->PIDTablePartitioning,
									       vDevice_p->PIDTablePartitionID));

		stpti_printf("Successfully increased pid table partition to %d entries\n",
			     vDevice_p->TP_Interface.NumberOfSlots);
		return Error;
	} else {
		stpti_printf("Freeing pid table partition on vDevice 0x%x\n", vDeviceHandle.word);
		/* OK we need to try a little harder */
		stptiSupport_PartitionedResourceFreePartition(&pDevice_p->PIDTablePartitioning,
							      vDevice_p->PIDTablePartitionID);

		stptiHAL_pDeviceDefragmentPidTablePartitions(vDeviceHandle);
		if (!(stptiSupport_PartitionedResourceAllocPartition(&pDevice_p->PIDTablePartitioning,
								     vDevice_p->TP_Interface.NumberOfSlots + 1,
								     &vDevice_p->PIDTablePartitionID))) {
			stpti_printf
			    ("Successfully allocated new pid table partition on vDevice 0x%x after defragment\n",
			     vDeviceHandle.word);
			vDevice_p->TP_Interface.NumberOfSlots++;
		} else {
			stpti_printf
			    ("Failed to allocate new pid table partition on vDevice 0x%x after defragment - Performing Compact\n",
			     vDeviceHandle.word);
			/* OK we need to try even harder :) */
			stptiHAL_pDeviceCompactPidTablePartitions(stptiOBJMAN_ConvertFullObjectHandleToFullpDeviceHandle
								  (vDeviceHandle));
			if (!
			    (stptiSupport_PartitionedResourceAllocPartition
			     (&pDevice_p->PIDTablePartitioning, vDevice_p->TP_Interface.NumberOfSlots + 1,
			      &vDevice_p->PIDTablePartitionID))) {
				stpti_printf
				    ("Successfully allocated new pid table partition on vDevice 0x%x after compact\n",
				     vDeviceHandle.word);
				vDevice_p->TP_Interface.NumberOfSlots++;
			} else {
				stpti_printf
				    ("Failed to allocate new pid table partition on vDevice 0x%x after compact - Panic!",
				     vDeviceHandle.word);
				/*  OK we have run out of options - this should never happen as we should run out of total slots first. */
				STPTI_ASSERT(FALSE, "Problem with TE pid table management");
				return ST_ERROR_NO_MEMORY;
			}
		}
	}

	stptiHAL_pDeviceUpdatePidTable(stptiOBJMAN_ConvertFullObjectHandleToFullpDeviceHandle(vDeviceHandle));

	return (Error);
}
