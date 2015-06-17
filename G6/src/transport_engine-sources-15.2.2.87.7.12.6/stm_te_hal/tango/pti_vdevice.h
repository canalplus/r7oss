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
   @file   pti_vdevice.h
   @brief  Virtual Device Allocation, Deallocation functions.

   This file implements the functions for creating and destroying virtual devices.  This allows us
   to have appearance of multiple PTI's on one physical PTI.

   This file should ONLY contain function prototypes and constants that are
   INTERNAL to the stptiHAL.  i.e. with the exception of the HAL object
   registration function in pti_driver.c, NOTHING outside of the tango
   directory should be referring to this file.

   Defines and enums relevent to the API of the HAL must be put into
   pti_hal_api.h and will be shared across all HALs.

 */

#ifndef _PTI_VDEVICE_H_
#define _PTI_VDEVICE_H_

/* Includes ---------------------------------------------------------------- */

/* ANSI C includes */

/* STAPI includes */
#include "stddefs.h"

/* Includes from API level */
#include "../pti_hal_api.h"

/* Includes from the HAL / ObjMan level */
#include "../objman/pti_object.h"
#include "firmware/pti_tp_api.h"
#include "pti_pdevice.h"

/* Exported Types ---------------------------------------------------------- */

/* One of these per Virtual PTI */
typedef struct {
	Object_t ObjectHeader;						/* Must be first in structure                                  */

	stptiHAL_TransportProtocol_t TSProtocol;			/**< The transport stream protocol                             */
	int PacketSize;							/**< The size of a transport packet (for DVB this is 188bytes) */
	U32 StreamID;							/**< TSInput Tag assoicated with the vdevice                   */
	FullHandle_t TSInputHandle;					/**< The handle of the linked  TSInput/STFE interface          */
	BOOL ForceDiscardSectionOnCRCError;				/**< If TRUE forces DiscardOnCRC to true for all filters       */
	int PIDTablePartitionID;					/**< PIDTable partition ID                                     */
	int FilterPartitionID;						/**< Filter partition ID                                       */
	int SlotsAllocated;						/**< Number of slots allocated just for this vDevice           */
	List_t *TSInputHandles;						/**< Shared Resource                                           */
	List_t *SoftwareInjectorHandles;				/**< Shared Resource                                           */
	List_t *BufferHandles;						/**< Shared Resource                                           */
	List_t *IndexHandles;						/**< Shared Resource                                           */
	List_t *SlotHandles;						/**< Shared Resource                                           */
	List_t PidIndexes;						/**< Individual Resource                                       */
	List_t SectionFilterHandles;					/**< Individual Resource                                       */
	List_t PESStreamIDFilterHandles;				/**< Individual Resource                                       */
	stptiTP_Interface_t TP_Interface;				/**< Host pointers to the TP interface structures              */
	U32 ProprietaryFilterIndex;					/**< Proprietary Filter Index                                  */
	BOOL ProprietaryFilterAllocated;				/**< Set if the Proprietary Filter is allocated                */
	stptiHAL_EventFuncPtrs_t EventPtrs;				/**< Function pointers for event notification abstraction      */
	BOOL PoweredOn;							/**< Power Management state of the virtual device              */
	U32 SavedStreamID;						/**< TSInput Tag to re-associate when leaving low power mode.  */
	stptiHAL_vDeviceStreamStatistics_t StreamStatisticsUponReset;	/**< Saved Statistics when reset called (this avoids implementing a reset in the f/w) */
	BOOL UseTimerTag;						/**< Timer Counter behaviour for TP on this vdevice            */
} stptiHAL_vDevice_t;

/* Exported Function Prototypes -------------------------------------------- */
extern const stptiHAL_vDeviceAPI_t stptiHAL_vDeviceAPI;

void stptiHAL_vDeviceTPEntryInitialise(volatile stptiTP_vDeviceInfo_t *vDeviceInfo_p);
void stptiHAL_vDeviceTranslateTag(U32 TagWord0, U32 TagWord1, U32 *Clk27MHzDiv300Bit32_p,
				  U32 *Clk27MHzDiv300Bit31to0_p, U16 *Clk27MHzModulus300_p);
ST_ErrorCode_t stptiHAL_vDeviceGrowPIDTable(FullHandle_t vDeviceHandle);

#define stptiHAL_GetObjectvDevice_p(ObjectHandle)   ((stptiHAL_vDevice_t *)stptiOBJMAN_HandleToObjectPointer(ObjectHandle,OBJECT_TYPE_VDEVICE))

#endif /* _PTI_VDEVICE_H_ */
