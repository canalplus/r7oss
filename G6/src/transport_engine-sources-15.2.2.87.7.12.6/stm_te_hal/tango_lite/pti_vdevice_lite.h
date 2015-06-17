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
   @file   pti_vdevice_lite.h
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

#ifndef _PTI_VDEVICE_LITE_H_
#define _PTI_VDEVICE_LITE_H_

/* Includes ---------------------------------------------------------------- */

/* ANSI C includes */

/* Includes from API level */
#include "../pti_hal_api.h"

#include "pti_pdevice_lite.h"

/* Exported Types ---------------------------------------------------------- */

/* One of these per Virtual PTI */
typedef struct {
	Object_t ObjectHeader;						/* Must be first in structure                                  */

	stptiHAL_TransportProtocol_t TSProtocol;			/**< The transport stream protocol                             */
	int PacketSize;							/**< The size of a transport packet (for DVB this is 188bytes) */
	U32 StreamID;							/**< TSInput Tag assoicated with the vdevice                   */
	FullHandle_t TSInputHandle;					/**< The handle of the linked  TSInput/STFE interface          */
	int PIDTablePartitionID;					/**< PIDTable partition ID                                     */
	int FilterPartitionID;						/**< Filter partition ID                                       */
	List_t *TSInputHandles;						/**< Shared Resource                                           */
	List_t PidIndexes;						/**< Individual Resource                                       */
	stptiTP_Interface_lite_t TP_Interface;				/**< Host pointers to the TP interface structures              */
	BOOL PoweredOn;							/**< Power Management state of the virtual device              */
	U32 SavedStreamID;						/**< TSInput Tag to re-associate when leaving low power mode.  */
} stptiHAL_vDevice_lite_t;

/* Exported Function Prototypes -------------------------------------------- */
extern const stptiHAL_vDeviceAPI_t stptiHAL_vDeviceAPI_Lite;

#define stptiHAL_GetObjectvDevice_lite_p(ObjectHandle)  ((stptiHAL_vDevice_lite_t *)stptiOBJMAN_HandleToObjectPointer(ObjectHandle,OBJECT_TYPE_VDEVICE))

ST_ErrorCode_t stptiHAL_vDeviceGrowPIDTableLite(FullHandle_t vDeviceHandle);

#endif /* _PTI_VDEVICE_LITE_H_ */
