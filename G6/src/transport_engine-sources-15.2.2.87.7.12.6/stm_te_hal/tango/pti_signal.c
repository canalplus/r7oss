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
   @file   pti_signal.c
   @brief  Signal Object Initialisation, Termination and Manipulation Functions.

   This file implements the functions for creating, destroying and accessing PTI signals.

   Buffers can have signals associated.  Signals happen when a byte threshold (of "fullness") is
   crossed.  Once over the threshold, it will not signal again until you empty it below the
   threshold and new data is put in to take it over the threshold again.

   For buffer collecting units of information, the threshold is set to 1.  You'll get a signal for
   every unit put into the buffer.

   Signals are a "virtual concept".  The TPs buffer structure actually holds the information about
   signalling.  The Signals purpose it to provide a messaging link between the buffer's signal
   going off and waking up a waiting task.

 */

#if 0
#define STPTI_PRINT
#endif

/* Includes ---------------------------------------------------------------- */
#include "linuxcommon.h"

/* Includes from API level */
#include "../pti_debug.h"
#include "../pti_hal_api.h"
#include "../pti_osal.h"

/* Includes from the HAL / ObjMan level */
#include "pti_pdevice.h"
#include "pti_vdevice.h"
#include "pti_signal.h"
#include "pti_buffer.h"

/* MACROS ------------------------------------------------------------------ */
/* Private Constants ------------------------------------------------------- */
/* Private Variables ------------------------------------------------------- */
/* Private Function Prototypes --------------------------------------------- */
ST_ErrorCode_t stptiHAL_SignalAllocator(FullHandle_t SignalHandle, void *params_p);
ST_ErrorCode_t stptiHAL_SignalAssociator(FullHandle_t SignalHandle, FullHandle_t AssocObjectHandle);
ST_ErrorCode_t stptiHAL_SignalDisassociator(FullHandle_t SignalHandle, FullHandle_t AssocObjectHandle);
ST_ErrorCode_t stptiHAL_SignalDeallocator(FullHandle_t SignalHandle);
ST_ErrorCode_t stptiHAL_SignalAbort(FullHandle_t SignalHandle);
ST_ErrorCode_t stptiHAL_SignalWait(FullHandle_t SignalHandle, FullHandle_t * BufferHandle_p, U32 TimeoutMS);

/* Public Constants -------------------------------------------------------- */

/* Export the API */
const stptiHAL_SignalAPI_t stptiHAL_SignalAPI = {
	{
		/* Allocator              Associator                 Disassociator                 Deallocator */
		stptiHAL_SignalAllocator, stptiHAL_SignalAssociator, stptiHAL_SignalDisassociator, stptiHAL_SignalDeallocator,
		NULL, NULL
	},
	stptiHAL_SignalAbort,
	stptiHAL_SignalWait
};

/* Object Management Functions ------------------------------------------------------------------ */

/**
   @brief  Allocate a Signal

   This function allocates memory for a buffer, but does not allocate resources from the TP until
   the buffer is associated.

   @param  SignalHandle         A handle for the buffer
   @param  params_p             Parameters for the buffer

   @return                      A standard st error type...
                                - ST_ERROR_BAD_PARAMETER
                                - ST_ERROR_NO_MEMORY
                                - ST_NO_ERROR
 */
ST_ErrorCode_t stptiHAL_SignalAllocator(FullHandle_t SignalHandle, void *params_p)
{
	/* Already write locked */
	ST_ErrorCode_t Error = ST_NO_ERROR;
	stptiHAL_Signal_t *Signal_p = stptiHAL_GetObjectSignal_p(SignalHandle);

	params_p = params_p;	/* avoid compiler warning */

	/* Allocate Signalling Queue */
	Signal_p->SignallingMessageQ = stptiSupport_MessageQueueCreate(sizeof(FullHandle_t), 16);
	Signal_p->WaitingOnSignalCount = 0;
	if (Signal_p->SignallingMessageQ == NULL) {
		Error = ST_ERROR_NO_MEMORY;
	}

	return (Error);
}

/**
   @brief  Associate Signal to a Buffer

   This function allows the user to associate slots and signals.  When associating a slot, if this
   is the first slot associated to this buffer it allocates TP DMA resources.

   @param  SignalHandle         A handle for the buffer
   @param  AssocObjectHandle    A handle for the object being associated (a slot or signal)

   @return                      A standard st error type...
                                - ST_ERROR_BAD_PARAMETER
                                - STPTI_ERROR_INVALID_SLOT_TYPE  (all assoc'd slots must be the same type)
                                - ST_ERROR_NO_MEMORY
                                - ST_NO_ERROR
 */
ST_ErrorCode_t stptiHAL_SignalAssociator(FullHandle_t SignalHandle, FullHandle_t AssocObjectHandle)
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

   This function allows the user to disassociate slots and signals.  When disassociating a slot, if
   there are no other slots left then the TP's resources are cleared for this buffer (but it can still
   be read using the buffer read functions).

   @param  SignalHandle         A handle for the buffer
   @param  AssocObjectHandle    A handle for the object being disassociated (a slot or signal)

   @return                      A standard st error type...
                                - ST_NO_ERROR
 */
ST_ErrorCode_t stptiHAL_SignalDisassociator(FullHandle_t SignalHandle, FullHandle_t AssocObjectHandle)
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
ST_ErrorCode_t stptiHAL_SignalDeallocator(FullHandle_t SignalHandle)
{
	/* Already write locked */
	ST_ErrorCode_t Error = ST_NO_ERROR;
	stptiSupport_AccessLockState_t LocalLockState = stptiSupport_WRITELOCKED;

	stptiHAL_Signal_t *Signal_p = stptiHAL_GetObjectSignal_p(SignalHandle);
	stptiSupport_MessageQueue_t *Queue_p = Signal_p->SignallingMessageQ;

	/* Prevent anything happening to the queue (new signals being added) whilst we dealloc it */
	Signal_p->SignallingMessageQ = NULL;

	if (Queue_p != NULL) {
		int counter;
		int SignalWaitCount = Signal_p->WaitingOnSignalCount;
		while (SignalWaitCount > 0) {
			FullHandle_t Handle = stptiOBJMAN_NullHandle;
			if (!stptiSupport_MessageQueuePostTimeout(Queue_p, &Handle, 1000)) {
				STPTI_PRINTF_ERROR("Unable to Send Abort Signals");
				break;
			}
			SignalWaitCount--;
		}

		/* Need to provide an opportunity for tasks to respond to the Abort Signals */
		counter = 0;
		while (Signal_p->WaitingOnSignalCount > 0) {
			stptiOBJMAN_Unlock(SignalHandle, &LocalLockState);
			if (counter++ < 3) {
				/* Start by yielding to other tasks (equal or higher priority) */
				yield();
			} else {
				/* Then wait longer so that lower priority tasks get a look in */
				msleep(1);
			}
			stptiOBJMAN_WriteLock(SignalHandle, &LocalLockState);
			if (counter > 20) {	/* threshold at which point we have to give up */
				STPTI_PRINTF_ERROR("Unable to abort Signal [%08x]", (unsigned)SignalHandle.word);
				break;
			}
		}

		while (TRUE) {
			FullHandle_t Handle;
			if (!stptiSupport_MessageQueueReceiveTimeout(Queue_p, &Handle, 0)) {
				break;
			}
		}
		stptiSupport_MessageQueueDelete(Queue_p);
	}

	return (Error);
}

/* Object HAL functions ------------------------------------------------------------------------- */

/**
   @brief  Signal Abort on a specified Signal.

   This function allows send Abort to a signal (can be used by the USER to terminate a SignalWait).

   @param  SignalHandle         A handle for the buffer

   @return                      A standard st error type...
                                - ST_NO_ERROR
 */
ST_ErrorCode_t stptiHAL_SignalAbort(FullHandle_t SignalHandle)
{
	ST_ErrorCode_t Error = ST_NO_ERROR;
	stptiSupport_AccessLockState_t LocalLockState = stptiSupport_UNLOCKED;

	stptiOBJMAN_ReadLock(SignalHandle, &LocalLockState);
	{
		stptiHAL_Signal_t *Signal_p = stptiHAL_GetObjectSignal_p(SignalHandle);

		if (Signal_p == NULL) {
			Error = ST_ERROR_BAD_PARAMETER;
		} else {
			if (Signal_p->SignallingMessageQ != NULL) {
				/* Post a null handle which signals abort */
				FullHandle_t Handle = stptiOBJMAN_NullHandle;
				if (!stptiSupport_MessageQueuePostTimeout(Signal_p->SignallingMessageQ, &Handle, 0)) {
					Error = ST_ERROR_TIMEOUT;
				}
			}
		}
	}
	stptiOBJMAN_Unlock(SignalHandle, &LocalLockState);

	return (Error);
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
ST_ErrorCode_t stptiHAL_SignalWait(FullHandle_t SignalHandle, FullHandle_t *BufferHandle_p, U32 TimeoutMS)
{
	ST_ErrorCode_t Error = ST_NO_ERROR;
	stptiSupport_AccessLockState_t LocalLockState = stptiSupport_UNLOCKED;

	int WaitTimeMS = 0;

	if (TimeoutMS == 0XFFFFFFFF) {
		WaitTimeMS = -1;
	} else if ((TimeoutMS / 1000) > (INT_MAX / HZ)) {
		Error = ST_ERROR_BAD_PARAMETER;
	} else {
		WaitTimeMS = (int)TimeoutMS;
	}

	if (Error == ST_NO_ERROR) {
		stptiSupport_MessageQueue_t *Queue_p = NULL;

		/* Obtain the queue to get the buffer signal message from */
		stptiOBJMAN_WriteLock(SignalHandle, &LocalLockState);
		{
			stptiHAL_Signal_t *Signal_p = stptiHAL_GetObjectSignal_p(SignalHandle);

			if (Signal_p == NULL) {
				Error = ST_ERROR_BAD_PARAMETER;
			} else {
				Queue_p = Signal_p->SignallingMessageQ;
				Signal_p->WaitingOnSignalCount++;
			}
		}
		stptiOBJMAN_Unlock(SignalHandle, &LocalLockState);

		while (Error == ST_NO_ERROR && Queue_p != NULL) {
			FullHandle_t BufferHandle;
			BOOL timeout;

			timeout = !stptiSupport_MessageQueueReceiveTimeout(Queue_p, &BufferHandle, WaitTimeMS);

			/* Process the message */
			stptiOBJMAN_WriteLock(SignalHandle, &LocalLockState);	/* Lock again */
			{
				/* Get the signal pointer again as it might have changed (because we unlocked above) */
				stptiHAL_Signal_t *Signal_p = stptiHAL_GetObjectSignal_p(SignalHandle);

				if (Signal_p == NULL) {
					Error = ST_ERROR_BAD_PARAMETER;
				} else {
					Signal_p->WaitingOnSignalCount--;

					if (timeout) {
						Error = ST_ERROR_TIMEOUT;
					} else {
						stptiHAL_pDevice_t *pDevice_p =
						    stptiHAL_GetObjectpDevice_p(BufferHandle);

						*BufferHandle_p = BufferHandle;

						if (stptiOBJMAN_IsNullHandle(BufferHandle) || pDevice_p->TP_Status != stptiHAL_STXP70_RUNNING) {
							/* If there is a NullBuffer Handle, or device has gone to sleep, say signal aborted */
							Error = STPTI_ERROR_SIGNAL_ABORTED;
						} else {
							/* As the host can signal as well as the TP, there is a race condition
							   when both check the amount of data in the buffer.  This can (very
							   rarely) result in a duplicate signal.  To guard against this we check
							   we have enough data in the buffer to permit the signal and if not we
							   try again. */

							stptiHAL_Buffer_t *Buffer_p = stptiHAL_GetObjectBuffer_p(BufferHandle);
							U32 BytesInBuffer;

							Error =
							    stptiHAL_BufferState(BufferHandle, &BytesInBuffer, NULL,
										 NULL, NULL, NULL, NULL);
							if ((ST_NO_ERROR == Error) && NULL != Buffer_p
							    && BytesInBuffer < Buffer_p->SignallingUpperThreshold) {
								/* Not enough data to permit signal (try again) */
								Signal_p->WaitingOnSignalCount++;
							} else {
								/* Enough data for signal (Break out of signalling loop) */
								Queue_p = NULL;	/* don't use break, because of AccessLock */
							}
						}
					}
				}
			}
			stptiOBJMAN_Unlock(SignalHandle, &LocalLockState);
		}
	}

	return (Error);
}
