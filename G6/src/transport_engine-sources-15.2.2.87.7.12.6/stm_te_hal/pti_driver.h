/*****************************************************************************/
/* COPYRIGHT (C) 2009 STMicroelectronics - All Rights Reserved               */
/* ST makes no warranty express or implied including but not limited to,     */
/* any warranty of                                                           */
/*                                                                           */
/*   (i)  merchantability or fitness for a particular purpose and/or         */
/*   (ii) requirements, for a particular purpose in relation to the LICENSED */
/*        MATERIALS, which is provided "AS IS", WITH ALL FAULTS. ST does not */
/*        represent or warrant that the LICENSED MATERIALS provided here     */
/*        under is free of infringement of any third party patents,          */
/*        copyrights,trade secrets or other intellectual property rights.    */
/*        ALL WARRANTIES, CONDITIONS OR OTHER TERMS IMPLIED BY LAW ARE       */
/*        EXCLUDED TO THE FULLEST EXTENT PERMITTED BY LAW                    */
/*                                                                           */
/*****************************************************************************/
/**
   @file   pti_driver.h
   @brief  Main driver file, defining the shared information structures.

   This file contains the single global variable for handling where the hardware devices are in
   memory space, and for holding key information about the physical IP blocks.

   The main reason for keeping the Physical object outside of the Object Manager is to facilitate
   speedy lookup (via macros), and to also help with debugging.

 */

#ifndef _PTI_DRIVER_H_
#define _PTI_DRIVER_H_

/* Includes ---------------------------------------------------------------- */

/* ANSI C includes */

/* STAPI includes */
#include "stddefs.h"

#if defined(STPTI_FDMA_SUPPORT)
#include "stfdma.h"
#endif

/* Includes from API level */
#include "pti_osal.h"
#include "pti_hal_api.h"
#include "pti_tshal_api.h"

/* Includes from the HAL / ObjMan level */
#include "objman/pti_object.h"

/* Maximum number of physical TP devices supported by the driver */
#define STPTI_MAX_PDEVICES (4)

/* Exported Types ---------------------------------------------------------- */

typedef struct {
	/* Object Management (object metadata) for TANGO HAL - Multiple TP's means multiple HALs so the pointers below are scaled up by [NumberOfpDevices] */
	int NumberOfpDevices;
	ObjectManagerRoot_t *ObjMan[STPTI_MAX_PDEVICES];	/* array of pointers to ObjectManagerRoot_t */
	HAL_API_t *HALFunctionPool[STPTI_MAX_PDEVICES];		/* array of pointers to HAL_API_t */

	/* Object Management (object metadata) for TSDevice HAL (STFE) - there is only ever 1 of this */
	TSHAL_API_t *TSHALFunctionPool;	/* pointer to TSHAL_API_t */

	/* For debug */
	U32 ObjManMemoryTally;		/**< Current memory usage */
	U32 ObjManMemoryHighWater;	/**< Maximum memory usage */

	/* For sychronising HAL calls + suspend/resume */
	atomic_t HALUsers;
	bool HALAllow;
	bool Power_Enabled;
	bool FE_Power_Enabled;
	wait_queue_head_t HALWaitQ;
	struct completion  HALIdle;

	const struct firmware *cachedTPFW;
	const struct firmware *cachedSTFEFW;

} STPTI_Driver_t;

extern STPTI_Driver_t STPTI_Driver;

/* Exported Function Prototypes -------------------------------------------- */

ST_ErrorCode_t stptiAPI_DriverGetNumberOfpDevices(int *NumPDevice);

int stptiAPI_register_drivers(void);
void stptiAPI_unregister_drivers(void);

#endif /* _PTI_DRIVER_H_ */
