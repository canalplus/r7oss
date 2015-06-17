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
   @file   pti_pdevice_lite.c
   @brief  Physical Device Initialisation and Termination functions

   This version of the hal
   This file implements the functions for initialising and terminating a physical device.  A pDevice
   is not stictly an object, but it is referenced by a vDevice and its is a way of sharing
   information between vDevices.

   As such most of the APIs should not be

 */

#if 0
#define STPTI_PRINT
#endif

/* Includes ---------------------------------------------------------------- */
#include "linuxcommon.h"

/* Includes from API level */
#include "../pti_debug.h"
#include "../pti_osal.h"
#include "../pti_driver.h"

/* Includes from the HAL / ObjMan level */
#include "../objman/pti_object.h"
#include "pti_pdevice_lite.h"
#include "pti_vdevice_lite.h"
/* MACROS ------------------------------------------------------------------ */

/* Private Constants ------------------------------------------------------- */

/* Private Variables ------------------------------------------------------- */

/* Private Function Prototypes --------------------------------------------- */

/* Although these prototypes are not exported directly they are exported through the API constant below. */
/* Add the definition to pti_hal.h */
ST_ErrorCode_t stptiHAL_pDeviceInitLite(FullHandle_t pDeviceHandle, void *params);
ST_ErrorCode_t stptiHAL_pDeviceTermLite(FullHandle_t pDeviceHandle);

ST_ErrorCode_t stptiHAL_pDeviceConfigureLiveLite(FullHandle_t pDeviceHandle,
						 stptiHAL_pDeviceConfigLiveParams_t *LiveParams_p);
ST_ErrorCode_t stptiHAL_pDeviceEnableSWLeakyPIDLite(FullHandle_t pDeviceHandle, unsigned int LiveChannel,
						    BOOL StreamIsActive);
ST_ErrorCode_t stptiHAL_pDeviceSetPowerStateLite(FullHandle_t pDeviceHandle, stptiHAL_pDevicePowerState_t NewState);
ST_ErrorCode_t stptiHAL_pDeviceGetCapabilityLite(FullHandle_t pDeviceHandle,
						stptiHAL_pDeviceConfigStatus_t *ConfigStatus_p);

/* Public Constants ------------------------------------------------------- */

/* Public Variables ------------------------------------------------------- */

/* Export the API */
const stptiHAL_pDeviceAPI_t stptiHAL_pDeviceAPI_Lite = {
	{
		/* Allocator              Associator  Disassociator Deallocator */
		stptiHAL_pDeviceInitLite, NULL,       NULL,         stptiHAL_pDeviceTermLite,
		NULL, NULL
	},
	NULL,			/* stptiHAL_pDeviceEventTask */
	NULL,			/* stptiHAL_pDeviceEventTaskAbort */
	NULL,			/* stptiHAL_pDeviceLiveTask, */

	stptiHAL_pDeviceConfigureLiveLite,	/* Must be exposed for TSINPUT (STFE) HAL */
	stptiHAL_pDeviceEnableSWLeakyPIDLite,	/* Must be exposed for TSINPUT (STFE) HAL */
	NULL,
	stptiHAL_pDeviceGetCapabilityLite,
	NULL,
	stptiHAL_pDeviceSetPowerStateLite,
};

/**
   @brief  Initialise a pDevice.

   Lite version with TP functionality removed.

   @param  pDeviceHandle  The (full) handle of the pDevice.
   @param  params         Pointer to platform device to initialise

   @return                A standard st error type...
                          - ST_NO_ERROR
                          - ST_ERROR_NO_MEMORY           (unable to map registers)
                          - ST_ERROR_INTERRUPT_INSTALL   (unable to install interrupt)
                          - ST_ERROR_TIMEOUT             (timedout waiting for TP ack)
 */
ST_ErrorCode_t stptiHAL_pDeviceInitLite(FullHandle_t pDeviceHandle, void *params)
{
	ST_ErrorCode_t Error = ST_NO_ERROR;
	stptiHAL_pDevice_lite_t *pDevice_p = stptiHAL_GetObjectpDevice_lite_p(pDeviceHandle);

	STPTI_PRINTF("Starting TANGO Stubbed HAL");

	/* This code imitates the function of parameters returned from the platform
	   configuration file as there are no TP device parameters currently for
	   Alicante */
	pDevice_p->TP_Interface.NumberOfvDevices = 16;
	pDevice_p->TP_Interface.NumberOfSlots = 256;
	pDevice_p->TP_Interface.NumberOfPids = 256 + (256 / 2);
	pDevice_p->TP_Interface.NumberOfSectionFilters = 256;
	pDevice_p->TP_Interface.NumberOfDMAStructures = 96;
	pDevice_p->TP_Interface.NumberOfIndexers = 32;
	pDevice_p->TP_Interface.NumberOfStatusBlks = 32;

	stptiHAL_debug_add_object_lite(pDeviceHandle);

	return (Error);
}

/**
   @brief  Terminate a pDevice.

   Lite version with no TP functionality

   @param  pDeviceHandle  The (full) handle of the pDevice.

   @return                A standard st error type...
                          - ST_NO_ERROR
                          - ST_ERROR_INTERRUPT_UNINSTALL   (unable to uninstall interrupt)
 */
ST_ErrorCode_t stptiHAL_pDeviceTermLite(FullHandle_t pDeviceHandle)
{
	return (ST_NO_ERROR);
}

/**
   @brief  Configure a Live channel on this physical device

   This function dummy function with no TP functionality

   @param  pDeviceHandle  The handle for the pDevice to be examined
   @param  LiveParams_p   A pointer to a structure containing the params

   @return                A standard st error type...
                          - ST_NO_ERROR
 */
ST_ErrorCode_t stptiHAL_pDeviceConfigureLiveLite(FullHandle_t pDeviceHandle,
						 stptiHAL_pDeviceConfigLiveParams_t *LiveParams_p)
{
	/* This call has been stubbed as there is no transport processor to
	   communicate to */

	return ST_NO_ERROR;
}

ST_ErrorCode_t stptiHAL_pDeviceEnableSWLeakyPIDLite(FullHandle_t pDeviceHandle, unsigned int LiveChannel,
						    BOOL StreamIsActive)
{
	/* This call has been stubbed as there is no transport processor to
	   communicate to */

	return ST_NO_ERROR;
}

/**
   @brief  Get capabilities of the (physical) device

   This function returns the capabilities of the pdevice.

   @param  pDeviceHandle            The handle for the pDevice to be examined
   @param  ConfigStatus_p           (return) A pointer to where to put the capabilities

   @return                          A standard st error type...
					- ST_NO_ERROR
 */
ST_ErrorCode_t stptiHAL_pDeviceGetCapabilityLite(FullHandle_t pDeviceHandle,
						stptiHAL_pDeviceConfigStatus_t *ConfigStatus_p)
{
	stptiSupport_AccessLockState_t LocalLockState = stptiSupport_UNLOCKED;

	stptiHAL_pDevice_lite_t *pDevice_p = stptiHAL_GetObjectpDevice_lite_p(pDeviceHandle);

	stptiOBJMAN_WriteLock(pDeviceHandle, &LocalLockState);

	ConfigStatus_p->NumbervDevice = pDevice_p->TP_Interface.NumberOfvDevices;
	ConfigStatus_p->NumberSlots = pDevice_p->TP_Interface.NumberOfSlots;
	ConfigStatus_p->NumberDMAs = pDevice_p->TP_Interface.NumberOfDMAStructures;
	ConfigStatus_p->NumberHWSectionFilters = pDevice_p->TP_Interface.NumberOfSectionFilters;
	ConfigStatus_p->NumberIndexer = pDevice_p->TP_Interface.NumberOfIndexers;
	ConfigStatus_p->NumberStatusBlk = pDevice_p->TP_Interface.NumberOfStatusBlks;

	stptiOBJMAN_Unlock(pDeviceHandle, &LocalLockState);


	return ST_NO_ERROR;
}


/**
   @brief Puts the pDevice into the requested power state

   @param  pDeviceHandle  The handle of the pDevice whose power state to change
   @param  NewState       The requested power state

   @return                A standard st error type...
                          - ST_NO_ERROR
 */
ST_ErrorCode_t stptiHAL_pDeviceSetPowerStateLite(FullHandle_t pDeviceHandle, stptiHAL_pDevicePowerState_t NewState)
{
	/* Nothing to do for tango lite pDevice since there is no hardware to
	 * manage here */
	return ST_NO_ERROR;
}
