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
   @file   pti_signal_lite.c
   @brief  Signal Object Initialisation, Termination and Manipulation Functions.

   Dummy functions without TP functionality.
   Likely to be removed later.

 */

#if 0
#define STPTI_PRINT
#endif

/* Includes ---------------------------------------------------------------- */

/* Includes from API level */
#include "../pti_debug.h"

/* Includes from the HAL / ObjMan level */
#include "pti_signal.h"
#include "pti_signal_lite.h"

/* MACROS ------------------------------------------------------------------ */
/* Private Constants ------------------------------------------------------- */
/* Private Variables ------------------------------------------------------- */
/* Private Function Prototypes --------------------------------------------- */
ST_ErrorCode_t stptiHAL_SignalAllocatorLite(FullHandle_t SignalHandle, void *params_p);
ST_ErrorCode_t stptiHAL_SignalAssociatorLite(FullHandle_t SignalHandle, FullHandle_t AssocObjectHandle);
ST_ErrorCode_t stptiHAL_SignalDisassociatorLite(FullHandle_t SignalHandle, FullHandle_t AssocObjectHandle);
ST_ErrorCode_t stptiHAL_SignalDeallocatorLite(FullHandle_t SignalHandle);
ST_ErrorCode_t stptiHAL_SignalAbortLite(FullHandle_t SignalHandle);
ST_ErrorCode_t stptiHAL_SignalWaitLite(FullHandle_t SignalHandle, FullHandle_t *BufferHandle_p, U32 TimeoutMS);

/* Public Constants -------------------------------------------------------- */

/* Export the API */
const stptiHAL_SignalAPI_t stptiHAL_SignalAPI_Lite = {
	{
		/* Allocator                  Associator                         Disassociator                 Deallocator */
		stptiHAL_SignalAllocatorLite, stptiHAL_SignalAssociatorLite, stptiHAL_SignalDisassociatorLite, stptiHAL_SignalDeallocatorLite,
		NULL, NULL
	},
	stptiHAL_SignalAbortLite,
	stptiHAL_SignalWaitLite
};

/* Object Management Functions ------------------------------------------------------------------ */

/**
   @brief  Allocate a Signal

   This function allocates a signal for a buffer,

   @param  SignalHandle         A handle for the buffer
   @param  params_p             Parameters for the buffer

   @return                      A standard st error type...
                                - ST_ERROR_BAD_PARAMETER
                                - ST_ERROR_NO_MEMORY
                                - ST_NO_ERROR
 */
ST_ErrorCode_t stptiHAL_SignalAllocatorLite(FullHandle_t SignalHandle, void *params_p)
{
	return (ST_NO_ERROR);
}

/**
   @brief  Associate Signal to a Buffer

   This function allows the user to associate slots and signals.

   @param  SignalHandle         A handle for the buffer
   @param  AssocObjectHandle    A handle for the object being associated (a slot or signal)

   @return                      A standard st error type...
                                - ST_ERROR_BAD_PARAMETER
                                - STPTI_ERROR_INVALID_SLOT_TYPE  (all assoc'd slots must be the same type)
                                - ST_ERROR_NO_MEMORY
                                - ST_NO_ERROR
 */
ST_ErrorCode_t stptiHAL_SignalAssociatorLite(FullHandle_t SignalHandle, FullHandle_t AssocObjectHandle)
{
	/* Already write locked */
	ST_ErrorCode_t Error = ST_NO_ERROR;

	SignalHandle = SignalHandle;	/* avoid compiler warning */

	switch (stptiOBJMAN_GetObjectType(AssocObjectHandle)) {
	case OBJECT_TYPE_BUFFER:
		break;

	default:
		Error = ST_ERROR_BAD_PARAMETER;
		break;
	}

	return (Error);
}

/**
   @brief  Disassociate a Signal from a Buffer.

   This function allows the user to disassociate slots and signals.

   @param  SignalHandle         A handle for the buffer
   @param  AssocObjectHandle    A handle for the object being disassociated (a slot or signal)

   @return                      A standard st error type...
                                - ST_NO_ERROR
 */
ST_ErrorCode_t stptiHAL_SignalDisassociatorLite(FullHandle_t SignalHandle, FullHandle_t AssocObjectHandle)
{
	/* Already write locked */
	ST_ErrorCode_t Error = ST_NO_ERROR;

	SignalHandle = SignalHandle;	/* avoid compiler warning */

	switch (stptiOBJMAN_GetObjectType(AssocObjectHandle)) {
	case OBJECT_TYPE_BUFFER:
		break;

	default:
		/* Allow disassociation, even from invalid types, else we create a clean up problem */
		STPTI_PRINTF_ERROR("Signal disassociating from invalid type %d.",
				   stptiOBJMAN_GetObjectType(AssocObjectHandle));
		break;
	}
	return (Error);
}

/**
   @brief  Deallocate a Signal.

   This function frees memory for the Signal.

   @param  SignalHandle         A handle for the Signal

   @return                      A standard st error type...
                                - ST_NO_ERROR
 */
ST_ErrorCode_t stptiHAL_SignalDeallocatorLite(FullHandle_t SignalHandle)
{
	return (ST_NO_ERROR);
}

/* Object HAL functions ------------------------------------------------------------------------- */

/**
   @brief  Signal Abort on a specified Signal.

   This function allows send Abort to a signal (can be used by the USER to terminate a SignalWait).

   @param  SignalHandle         A handle for the buffer

   @return                      A standard st error type...
                                - ST_NO_ERROR
 */
ST_ErrorCode_t stptiHAL_SignalAbortLite(FullHandle_t SignalHandle)
{
	return (ST_NO_ERROR);
}

/**
   @brief  Wait for a signal.

   This function waits a specified time for a Signal, and returns the BufferHandle.

   Signals can result from the Transport Processor (putting data into the buffer), and from the
   Host CPU via Buffer Read calls.  This can results in a race condition, but the race condition
   give rise to an extra signal (so we do swallow excess signals in this function).

   SignalWait is expected to be paired with a read call.  Calling SignalWait twice, without a read
   will result in a timeout.  This so we can handle two threads accessing the same signal, and
   can guarantee data is in the buffer.  It also avoids a race condition on the signal.

   @param  SignalHandle         A handle for the signal.
   @param  BufferHandle         (return) A handle for the buffer that signalled.
   @param  TimeoutMS            Timeout in ms, or 0 (immediate) or 0XFFFFFFFF if wait indefinately.

   @return                      A standard st error type...
                                - STPTI_ERROR_SIGNAL_ABORTED   (abort signal received)
                                - ST_ERROR_TIMEOUT             (timed out waiting)
                                - ST_NO_ERROR
 */
ST_ErrorCode_t stptiHAL_SignalWaitLite(FullHandle_t SignalHandle, FullHandle_t *BufferHandle_p, U32 TimeoutMS)
{
	/* Delay to prevent work queues from continually polling for data */
	msleep_interruptible(100);

	*BufferHandle_p = stptiOBJMAN_NullHandle;
	return (ST_ERROR_TIMEOUT);
}
