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
   @file   pti_slot.h
   @brief  Slot Object Initialisation, Termination and Manipulation Functions.

   This file declares the object structure and access mechanism for the Slot
   Object.

   This file should ONLY contain function prototypes and constants that are
   INTERNAL to the stptiHAL.  i.e. with the exception of the HAL object
   registration function in pti_driver.c, NOTHING outside of the tango
   directory should be referring to this file.

   Defines and enums relevent to the API of the HAL must be put into
   pti_hal_api.h and will be shared across all HALs.


 */

#ifndef _PTI_SLOT_H_
#define _PTI_SLOT_H_

/* Includes ---------------------------------------------------------------- */

/* ANSI C includes */

/* STAPI includes */

/* Includes from API level */
#include "../pti_hal_api.h"

/* Includes from the HAL / ObjMan level */
#include "../objman/pti_object.h"
#include "firmware/pti_tp_api.h"

/* Exported Types ---------------------------------------------------------- */

typedef struct {
	Object_t ObjectHeader;				/* Must be first in structure */
	int SlotIndex;					/**< The index of the slot relative to the start of the region for this vDevice */
	int PidIndex;					/**< The index of the slot's pid entry in the pid table */
	U16 PID;					/**< The PID (to save looking it up on the TP) */
	stptiHAL_SlotMode_t SlotMode;			/**< The SlotMode (to save looking it up on the TP) */
	BOOL SuppressMetaData;				/**< TRUE if metadata should not be output to any assoc'd buffers (read fn's won't work) */
	BOOL SoftwareCDFifo;				/**< TRUE if swcdfifo mode, meaning no windback upon error, and return write not qwrite (in GetWritePtr fn)*/
	BOOL DataEntryInsertion;			/**< TRUE if dataentry insertion slot mode */
	BOOL DataEntryReplacement;			/**< TRUE if dataentry replacement slot mode */
	FullHandle_t PreviousSlotInChain;		/**< Set if slot is chained to one before (otherwise NullHandle) */
	FullHandle_t NextSlotInChain;			/**< Set if slot is chained to one after (otherwise NullHandle) */
	FullHandle_t DataEntryHandle;			/**< Set if slot is queued with data entries (otherwise NullHandle) */
	U32 SecurityPathID;				/**< SecurityPathID (to save looking itup on the TP) */
	stptiHAL_SecondaryType_t SecondarySlotType;	/**< Whether the slot is a being used for Secondary Pid Feature, rather than look up on TP */
	FullHandle_t SecondaryHandle;			/**< The associated slot handle for secondary pid */
	stptiHAL_SecondaryPidMode_t SecondarySlotMode;	/**< The associated slot handle for secondary pid */
} stptiHAL_Slot_t;

/* Exported Function Prototypes -------------------------------------------- */
extern const stptiHAL_SlotAPI_t stptiHAL_SlotAPI;

void stptiHAL_SlotResetState(FullHandle_t *SlotHandleList, unsigned int NumberOfHandles, BOOL KeepDeactivated);
void stptiHAL_SlotActivate(FullHandle_t SlotHandle);
ST_ErrorCode_t stptiHAL_SlotSyncDiscardOnCRCError(FullHandle_t SlotHandle);
ST_ErrorCode_t stptiHAL_SlotUpdateFilterState(FullHandle_t SlotHandle, FullHandle_t FilterHandle, BOOL DisableFilter,
					      BOOL EnableFilter);
void stptiHAL_SlotTPEntryInitialise(volatile stptiTP_SlotInfo_t *SlotInfo_p);
void stptiHAL_SlotPIDTableInitialise(volatile U16 *PIDTable_p, volatile U16 *PIDSlotMappingTable_p,
				     int NumberOfEntries);
int stptiHAL_SlotLookupPID(FullHandle_t vDeviceHandle, U16 PIDtoFind, FullHandle_t *SlotArray, int SlotArraySize);
U16 stptiHAL_LookupPIDsOnSlots(FullHandle_t vDeviceHandle, U16 *PIDArray_p, U16 PIDArraySize);

#define stptiHAL_GetObjectSlot_p(ObjectHandle)   ((stptiHAL_Slot_t *)stptiOBJMAN_HandleToObjectPointer(ObjectHandle,OBJECT_TYPE_SLOT))

#endif /* _PTI_SLOT_H_ */
