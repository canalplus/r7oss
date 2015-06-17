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
   @file   pti_swinjector_lite.c
   @brief  The Software Injector for playback streams.

   This file implements dummy hal functions devices without TP functionality.
   This will likely be removed later on.

 */

#if 0
#define STPTI_PRINT
#endif

/* Includes ---------------------------------------------------------------- */

/* Includes from API level */
#include "../pti_debug.h"

/* Includes from the HAL / ObjMan level */
#include "pti_swinjector_lite.h"

/* MACROS ------------------------------------------------------------------ */
/* Private Constants ------------------------------------------------------- */
/* Private Variables ------------------------------------------------------- */
/* Private Function Prototypes --------------------------------------------- */

/* Although these prototypes are not exported directly they are exported through the API constant below. */
/* Add the definition to pti_hal.h */
ST_ErrorCode_t stptiHAL_SoftwareInjectorAllocatorLite(FullHandle_t SoftwareInjectorHandle, void *params_p);
ST_ErrorCode_t stptiHAL_SoftwareInjectorDeallocatorLite(FullHandle_t SoftwareInjectorHandle);
ST_ErrorCode_t stptiHAL_SoftwareInjectorAddInjectNodeLite(FullHandle_t SoftwareInjectorHandle, U8 *Data, U32 Size,
							  U32 Ctrl);
ST_ErrorCode_t stptiHAL_SoftwareInjectorStartLite(FullHandle_t SoftwareInjectorHandle);
ST_ErrorCode_t stptiHAL_SoftwareInjectorWaitForCompletionLite(FullHandle_t SoftwareInjectorHandle, int TimeoutInMS);
ST_ErrorCode_t stptiHAL_SoftwareInjectorAbortLite(FullHandle_t SoftwareInjectorHandle);
ST_ErrorCode_t stptiHAL_SoftwareInjectorFlushLite(FullHandle_t SoftwareInjectorHandle);

/* Public Constants ------------------------------------------------------- */

/* Export the API */
const stptiHAL_SoftwareInjectorAPI_t stptiHAL_SoftwareInjectorAPI_Lite = {
	{
		/* Allocator                            Associator Disassociator Deallocator */
		stptiHAL_SoftwareInjectorAllocatorLite, NULL,      NULL,         stptiHAL_SoftwareInjectorDeallocatorLite,
		NULL, NULL
	},
	stptiHAL_SoftwareInjectorAddInjectNodeLite,
	stptiHAL_SoftwareInjectorStartLite,
	stptiHAL_SoftwareInjectorWaitForCompletionLite,
	stptiHAL_SoftwareInjectorAbortLite,
	stptiHAL_SoftwareInjectorFlushLite
};

/* Object Management Functions ------------------------------------------------------------------ */

/**
   @brief  Allocator for a software injector.

   This function allocate a software injector.

   @param  SoftwareInjectorHandle  Handle to the Software Injector Object.

   @param  params_p                A pointer to the stptiHAL_SoftwareInjectorConfigParams_t struct
                                   populated with the initialisation.

   @return                         A standard st error type...
                                   - ST_ERROR_NO_MEMORY if no channels free, or no memory.
                                   - ST_NO_ERROR if no errors
 */
ST_ErrorCode_t stptiHAL_SoftwareInjectorAllocatorLite(FullHandle_t SoftwareInjectorHandle, void *params_p)
{
	return (ST_NO_ERROR);
}

/**
   @brief  Deallocator for a software injector.

   This function deallocate a software injector.

   @param  SoftwareInjectorHandle  Handle to the Software Injector Object.

   @return                         A standard st error type...
                                   - ST_NO_ERROR if no errors
 */
ST_ErrorCode_t stptiHAL_SoftwareInjectorDeallocatorLite(FullHandle_t SoftwareInjectorHandle)
{
	return (ST_NO_ERROR);
}

/* Object HAL functions ------------------------------------------------------------------------- */

/**
   @brief  Add an injection node to a software injector.

   Dummy function without TP functionality.

   @param  SoftwareInjectorHandle  Handle to the Software Injector Object.
   @param  Data                    A pointer to the start of the memory region for this node.
   @param  Size                    The size of the memory region for this node.
   @param  Ctrl                    Always 0.

   @return                         A standard st error type...
                                   - ST_ERROR_DEVICE_BUSY if already injecting
                                   - ST_NO_ERROR if no errors
 */
ST_ErrorCode_t stptiHAL_SoftwareInjectorAddInjectNodeLite(FullHandle_t SoftwareInjectorHandle, U8 *Data, U32 Size,
							  stptiHAL_InjectionFlags_t InjectionFlags)
{
	return (ST_ERROR_FEATURE_NOT_SUPPORTED);
}

/**
   @brief  Start a Software Injector Injecting the Node Chain.

   Dummy function without TP functionality.

   @param  SoftwareInjectorHandle  Handle to the Software Injector Object.

   @return                         A standard st error type...
                                   - ST_ERROR_TIMEOUT if unable to start the injection
                                   - ST_NO_ERROR if no errors
 */
ST_ErrorCode_t stptiHAL_SoftwareInjectorStartLite(FullHandle_t SoftwareInjectorHandle)
{
	return (ST_ERROR_FEATURE_NOT_SUPPORTED);
}

/**
   @brief  Abort Software Injector Injection.

   Dummy function without TP functionality.

   @param  SoftwareInjectorHandle  Handle to the Software Injector Object.

   @return                         A standard st error type...
                                   - ST_NO_ERROR
 */
ST_ErrorCode_t stptiHAL_SoftwareInjectorAbortLite(FullHandle_t SoftwareInjectorHandle)
{
	return (ST_NO_ERROR);
}

/**
   @brief  Wait for the current software injection to complete

   Dummy function without TP functionality.

   @param  SoftwareInjectorHandle  Handle to the Software Injector Object.
   @param  TimeoutInMS             Specifies how long to wait (in miliseconds) before returning.  To
                                   poll if an injection has completed use 0 (zero).  To wait forever
                                   use a negative value (-1).

   @return                         A standard st error type...
                                   - ST_ERROR_TIMEOUT if still waiting for an injection to finish after the timeout
                                   - ST_NO_ERROR if not injecting (idle)
 */
ST_ErrorCode_t stptiHAL_SoftwareInjectorWaitForCompletionLite(FullHandle_t SoftwareInjectorHandle, int TimeoutInMS)
{
	return (ST_NO_ERROR);
}

/**
   @brief  Flush out any inactive nodes, and packet align next injection.

   Dummy function without TP functionality.

   @param  SoftwareInjectorHandle  Handle to the Software Injector Object.

   @return                         A standard st error type...
                                   - ST_NO_ERROR
 */
ST_ErrorCode_t stptiHAL_SoftwareInjectorFlushLite(FullHandle_t SoftwareInjectorHandle)
{
	return (ST_NO_ERROR);
}

/**
   @brief  Abort activity on the Software Injector.

   This function performs the same task as stptiHAL_SoftwareInjectorAbort() but is provided for
   internal HAL use.  It is used during powerdown to make sure the software injector is inactive.

   NOTE!  It must be called outside of a HAL access lock.

   @param  SoftwareInjectorHandle  Handle to the Software Injector Object

   @return                         A standard st error type...
                                   - ST_NO_ERROR
 */
ST_ErrorCode_t stptiHAL_SoftwareInjectorAbortInternalLite(FullHandle_t SoftwareInjectorHandle)
{
	return (ST_NO_ERROR);
}
