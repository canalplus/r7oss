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
   @file   pti_pdevice_lite.h
   @brief  Physical Device Initialisation and Termination functions

   This file implements the functions for initialising and terminating a physical device.  A pDevice
   is not stictly an object, but it is referenced by a vDevice and its is a way of sharing
   information between vDevices.

   This file should ONLY contain function prototypes and constants that are
   INTERNAL to the stptiHAL.  i.e. with the exception of the HAL object
   registration function in pti_driver.c, NOTHING outside of the tango
   directory should be referring to this file.

   Defines and enums relevent to the API of the HAL must be put into
   pti_hal_api.h and will be shared across all HALs.

 */

#ifndef _PTI_PDEVICE_LITE_H_
#define _PTI_PDEVICE_LITE_H_

/* Includes ---------------------------------------------------------------- */
/* Includes from API level */
#include "../pti_hal_api.h"

#include "../objman/pti_object.h"

/* Exported Types ---------------------------------------------------------- */
typedef struct {
    volatile U32 NumberOfvDevices;                          /* will be reset to 0 if there isn't enough memory in the shared memory interface */
    volatile U32 NumberOfPids;                              /* must be at least the number of slots (a few more helps pid table allocation) */
    volatile U32 NumberOfSlots;                             /* there is a hard limit of 256 */
    volatile U32 NumberOfSectionFilters;
    volatile U32 NumberOfDMAStructures;
    volatile U32 NumberOfIndexers;
    volatile U32 NumberOfStatusBlks;                        /* there is a hard limit of 256 */
    volatile U32 SlowRateStreamTimeout;                     /* GNBvd50868_SLOW_RATE_STREAM_WA - not conditionally compiled to avoid f/w compatibility issue, 0xFFFFFFFF means disabled */
} stptiTP_Interface_lite_t;

typedef struct {
	Object_t ObjectHeader;	/* Must be first in structure                                  */

	stptiTP_Interface_lite_t TP_Interface;			/**< A copy of the TP_Interface (TP Memory Map with any pointers converted to HOST addresses) */
} stptiHAL_pDevice_lite_t;
/* Exported Variables ------------------------------------------------------ */
/* Exported Function Prototypes -------------------------------------------- */

extern const stptiHAL_pDeviceAPI_t stptiHAL_pDeviceAPI_Lite;

void stptiHAL_pDeviceDefragmentPidTablePartitionsLite(FullHandle_t pDeviceHandle);
void stptiHAL_pDeviceCompactPidTablePartitionsLite(FullHandle_t pDeviceHandle);

#define stptiHAL_GetObjectpDevice_lite_p(pDeviceHandle)     ((stptiHAL_pDevice_lite_t *)stptiOBJMAN_HandleToObjectPointer(pDeviceHandle,OBJECT_TYPE_PDEVICE))

#endif /* _PTI_PDEVICE_LITE_H_ */
