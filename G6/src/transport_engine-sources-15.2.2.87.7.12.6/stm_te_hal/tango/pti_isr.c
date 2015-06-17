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
   @file   pti_isr.c
   @brief  The interrupt service routine

   This file contains the single interrupt service routine for the system.  We
   have one interrupt service routine to facilitate debugging and to ease
   porting between platforms.

 */

/* Includes ---------------------------------------------------------------- */

#if 0
#define stpti_interrupt_print( x )    STPTI_INTERRUPT_PRINT( "ISR: ", x )
#else
#define stpti_interrupt_print( x )
#endif

/* ANSI C includes */

/* Includes from API level */
#include "../pti_osal.h"
#include "../pti_debug.h"
#include "../pti_tshal_api.h"

/* Includes from the HAL / ObjMan level */
#include "pti_pdevice.h"
#include "pti_mailbox.h"
#include "pti_buffer.h"
#include "pti_isr.h"

/* MACROS ------------------------------------------------------------------ */

/* Private Constants ------------------------------------------------------- */

/* Private Variables ------------------------------------------------------- */
static char stptiHAL_ISRstring[256];

/* Private Function Prototypes --------------------------------------------- */

/* Functions --------------------------------------------------------------- */

/**
   @brief  The Interrupt Handler

   This is called (in an interrupt context) when one of two TP->HOST Mailboxes has information to
   be processed.

   @param  pDeviceHandle  A handle of the pDevice.

 */
irqreturn_t stptiHAL_TPInterruptHandler(int irq, void *pDeviceHandle)
{
	irqreturn_t ret = IRQ_NONE;

	FullHandle_t FullpDeviceHandle;
	stptiHAL_pDevice_t *pDevice_p;
	stptiHAL_TangoDevice_t *TP_p;
	U32 TP_Status0 = 0;
	BOOL AnInterruptWasServiced = FALSE;
	U32 Signal;

	FullpDeviceHandle.word = (U32) pDeviceHandle;
	pDevice_p = stptiHAL_GetObjectpDevice_p(FullpDeviceHandle);

	if (pDevice_p != NULL) {
		TP_p = &pDevice_p->TP_MappedAddresses;
		if (pDevice_p->TP_MappedAddresses.TP_MappedAddress != NULL) {
			{
				/* Function is referenced by pointer to prevent it being inlined (it's return value is useful for debugging) */
				U32(*InterruptStatusFn) (FullHandle_t) = stptiHAL_MailboxInterruptTP;
				TP_Status0 = (*InterruptStatusFn) (FullpDeviceHandle);
			}
			pDevice_p->TP_InterruptCount++;
		}
	}

	/* if pDevice_p is NULL TP_Status0 will be 0 */
	if (TP_Status0) {
		/* Does the Transport Processor want to print something? */
		if (stptiHAL_MailboxQueryAndClear(FullpDeviceHandle, MAILBOX_IS_TP_PRINTF_WAITING, &Signal) !=
		    ST_NO_ERROR) {
			/* Raise an error */
		} else if (Signal != 0) {
			/* a printf waiting... do something */
			/* Signal holds debug address */
			snprintf(stptiHAL_ISRstring, sizeof(stptiHAL_ISRstring), "STxP70_TP%01u : ", stptiOBJMAN_GetpDeviceIndex(FullpDeviceHandle));
			STPTI_INTERRUPT_PRINT(stptiHAL_ISRstring , (char *)&TP_p->dDEM_p[Signal]);

			stptiHAL_MailboxPost(FullpDeviceHandle, MAILBOX_TP_PRINTF_COMPLETE, 0);
			AnInterruptWasServiced = TRUE;
		}

		if (stptiHAL_MailboxQueryAndClear(FullpDeviceHandle, MAILBOX_CHECK_FOR_STATUS_BLOCK_OVERFLOW, &Signal)
		    != ST_NO_ERROR) {
			/* Raise an error */
		} else if (Signal != 0) {
			stpti_interrupt_print("MAILBOX_CHECK_FOR_STATUS_BLOCK_OVERFLOW");

			/* This interrupt will fire only once and then will be effectively disabled by the TP (STATUS_BLK_STATUS_BLK_OVERFLOW cleared in &vDeviceInfo_p->event_mask) */
			pDevice_p->StatusBlockBufferOverflows++;
			snprintf(stptiHAL_ISRstring, sizeof(stptiHAL_ISRstring), "ISR%01u : ", stptiOBJMAN_GetpDeviceIndex(FullpDeviceHandle));
			STPTI_INTERRUPT_PRINT(stptiHAL_ISRstring,
					      "Status Block Buffer Overflow. ISR has not kept up with TP's pushed event status blocks.");
			AnInterruptWasServiced = TRUE;
		}

		if (stptiHAL_MailboxQueryAndClear(FullpDeviceHandle, MAILBOX_CHECK_FOR_STATUS_BLOCK, &Signal) !=
		    ST_NO_ERROR) {
			/* Raise an error */
		} else if (Signal != 0) {
			U32 NumberOfStatusBlks = pDevice_p->TP_Interface.NumberOfStatusBlks;
			volatile stptiTP_StatusBlk_t *StatBlkBase_p = pDevice_p->TP_Interface.StatusBlk_p;
			volatile stptiTP_pDeviceInfo_t *pDeviceInfo_p = pDevice_p->TP_Interface.pDeviceInfo_p;

			U32 Write_Index = stptiHAL_pDeviceXP70Read(&pDeviceInfo_p->StatusBlk_WR_index);
			U32 Read_Index = stptiHAL_pDeviceXP70Read(&pDeviceInfo_p->StatusBlk_RD_index);

			stpti_interrupt_print("MAILBOX_CHECK_FOR_STATUS_BLOCK");
			while (Read_Index != Write_Index) {
				if ((pDevice_p->TP_Status != stptiHAL_STXP70_PREPARING_TO_SLEEP) && (pDevice_p->EventQueue_p != NULL)) {	/* (Bombproofing to avoid risk of a hang) don't raise events when going to sleep, or the Event Task is not running */
					/* Ok we have a status block so we need to send a copy to the event task */
					stptiTP_StatusBlk_t StatusBlock;

					/* Copy the status block from XP70 first into DDR before passing to the message queue (this means a double
					 * memcpy, but at least the second will be from cached memory) */
					stptiHAL_pDeviceXP70MemcpyFrom(&StatusBlock, (void *)&StatBlkBase_p[Read_Index],
								       sizeof(StatusBlock));

					if (stptiSupport_MessageQueuePostTimeoutISR
					    (pDevice_p->EventQueue_p, &StatusBlock, 0)) {
						pDevice_p->ConcurrentSuccessfulEventPushes++;
					} else {
						/* Unable to post, so we delay adding the Event until the next event occurs.  If it backs up too much
						   then we will get a STATUS_BLK_STATUS_BLK_OVERFLOW. */
						if ((pDevice_p->EventQueueOverflows == 0) || (pDevice_p->ConcurrentSuccessfulEventPushes > 100)) {
							/* To limit debug: Only output this if it is the first one, or we have been able to push concurrently 100 events */
							snprintf(stptiHAL_ISRstring, sizeof(stptiHAL_ISRstring),
								 "ISR%01u: Event Queue Overflow (after %u concurrent events). Demux event task is not keep up with events. (task priority too low?)",
								 stptiOBJMAN_GetpDeviceIndex(FullpDeviceHandle),
								 (unsigned int)pDevice_p->ConcurrentSuccessfulEventPushes);
							STPTI_INTERRUPT_PRINT("", stptiHAL_ISRstring);
						}
						pDevice_p->ConcurrentSuccessfulEventPushes = 0;
						pDevice_p->EventQueueOverflows++;
						break;
					}
				}

				/* Update the read pointer */
				Read_Index++;

				/* Check for wrap condition */
				if (Read_Index >= NumberOfStatusBlks) {
					Read_Index = 0;
				}

				/* Update TP's copy */
				stptiHAL_pDeviceXP70Write(&pDeviceInfo_p->StatusBlk_RD_index, Read_Index);
			}
			AnInterruptWasServiced = TRUE;
		}

		/* Has a software injection (playback) completed? */
		if (stptiHAL_MailboxQueryAndClear(FullpDeviceHandle, MAILBOX_CHECK_FOR_PLAYBACK_COMPLETION, &Signal) != ST_NO_ERROR) {
			/* Raise an error */
		} else if (Signal != 0) {
			/* Signal holds B7:0 indicating which playback stream completed */
			int Mask = 1, Bit = 0;

			stpti_interrupt_print("MAILBOX_CHECK_FOR_PLAYBACK_COMPLETION");
			while (Signal != 0) {
				if (Signal & Mask) {
					struct semaphore *InjectionSemaphore_p = pDevice_p->InjectionSemaphore_p[Bit];
					up(InjectionSemaphore_p);
					Signal &= ~Mask;
				}
				Bit++;
				Mask <<= 1;
			}
			AnInterruptWasServiced = TRUE;
		}

		/* Has a buffer threshold been reached? */
		if (stptiHAL_MailboxQueryAndClear(FullpDeviceHandle, MAILBOX_HAS_A_BUFFER_SIGNALLED, &Signal) !=
		    ST_NO_ERROR) {
			/* Raise an error */
		} else if (Signal != 0) {
			/* addresses in TP_Interface_p will be TP virtual addresses not HOST virtual addresses so we need to convert */
			volatile stptiTP_pDeviceInfo_t *pDeviceInfo_p = pDevice_p->TP_Interface.pDeviceInfo_p;

			volatile U8 ReadIndex = stptiHAL_pDeviceXP70Read(&pDeviceInfo_p->SignallingQ_RD_index);
			volatile U8 WriteIndex = stptiHAL_pDeviceXP70Read(&pDeviceInfo_p->SignallingQ_WR_index);
			volatile U8 *SignallingQ = (U8 *) pDeviceInfo_p->SignallingQ;

			stpti_interrupt_print("MAILBOX_HAS_A_BUFFER_SIGNALLED");

			while (ReadIndex != WriteIndex) {
				U8 BufferNumber = stptiHAL_pDeviceXP70Read(&SignallingQ[ReadIndex]);

				/* Check to make sure we don't overflow the end of the list */
				if (BufferNumber <= stptiOBJMAN_GetListCapacity(&pDevice_p->BufferHandles)) {
					stptiHAL_Buffer_t *Buffer_p =
					    stptiOBJMAN_GetItemFromList(&pDevice_p->BufferHandles, BufferNumber);
					if (Buffer_p != NULL) {
						FullHandle_t BufferHandle = stptiOBJMAN_ObjectPointerToHandle(Buffer_p);
						if (Buffer_p->SignallingMessageQ != NULL) {
							if (pDevice_p->TP_Status != stptiHAL_STXP70_PREPARING_TO_SLEEP) {	/* (Bombproofing to avoid risk of a hang) don't signal when going to sleep */
								if (!stptiSupport_MessageQueuePostTimeoutISR
								    (Buffer_p->SignallingMessageQ, &BufferHandle, 0)) {
									pDevice_p->SignallingQueueOverflows++;
									if (pDevice_p->SignallingQueueOverflows <= 3) {
										snprintf(stptiHAL_ISRstring,
											 sizeof(stptiHAL_ISRstring),
											 "ISR%01u: Signalling Queue Overflow on BufferHandle=0x%08x. Signals will be lost.",
											 stptiOBJMAN_GetpDeviceIndex(FullpDeviceHandle),
											 (unsigned int)BufferHandle.word);
										STPTI_INTERRUPT_PRINT("",
											stptiHAL_ISRstring);
									}
								}
							}
						}
					}
				}

				ReadIndex = (ReadIndex + 1) % TP_SIGNALLING_QUEUE_LENGTH;
				pDeviceInfo_p->SignallingQ_RD_index = ReadIndex;
			}

			AnInterruptWasServiced = TRUE;
		}

		/* Is the TP acknowledging something? (usually a live or playback instruction, or intialisation complete) */
		if (stptiHAL_MailboxQueryAndClear(FullpDeviceHandle, MAILBOX_TP_ACKNOWLEDGE, &Signal) != ST_NO_ERROR) {
			/* Raise an error */
		} else if (Signal != 0) {
			stpti_interrupt_print("MAILBOX_TP_ACKNOWLEDGE");

			/* tp has acknowledge (or tp has intialised itself)... */
			up(&pDevice_p->TPAckSemaphore);
			AnInterruptWasServiced = TRUE;
		}

#if defined( GNBvd50868_SLOW_RATE_STREAM_WA )
		/* Start STFE Flush Workaround.  GNBvd50868/GNBvd81614 */
		if (stptiHAL_MailboxQueryAndClear(FullpDeviceHandle, MAILBOX_TP_START_STFE_FLUSH, &Signal) !=
		    ST_NO_ERROR) {
			/* Raise an error */
		} else if (Signal != 0) {
			/* addresses in TP_Interface_p will be TP virtual addresses not HOST virtual addresses so we need to convert */
			volatile stptiTP_pDeviceInfo_t *pDeviceInfo_p = pDevice_p->TP_Interface.pDeviceInfo_p;
			volatile U32 StreamBitmask = stptiHAL_pDeviceXP70Read(&pDeviceInfo_p->StartFlushSTFE);

			stpti_interrupt_print("MAILBOX_TP_START_FLUSH_STFE");

			stptiTSHAL_call(TSInput.TSHAL_TSInputOverridePIDFilter, 0, StreamBitmask, FALSE);	/* Disable PID filter */

			AnInterruptWasServiced = TRUE;
		}

		/* Stop STFE Flush Workaround. GNBvd50868/GNBvd81614
		   (it is important that this is Second in case the start and stop happens simultaneously) */
		if (stptiHAL_MailboxQueryAndClear(FullpDeviceHandle, MAILBOX_TP_STOP_STFE_FLUSH, &Signal) !=
		    ST_NO_ERROR) {
			/* Raise an error */
		} else if (Signal != 0) {
			/* addresses in TP_Interface_p will be TP virtual addresses not HOST virtual addresses so we need to convert */
			volatile stptiTP_pDeviceInfo_t *pDeviceInfo_p = pDevice_p->TP_Interface.pDeviceInfo_p;
			volatile U32 StreamBitmask = stptiHAL_pDeviceXP70Read(&pDeviceInfo_p->StopFlushSTFE);

			stpti_interrupt_print("MAILBOX_TP_STOP_FLUSH_STFE");

			stptiTSHAL_call(TSInput.TSHAL_TSInputOverridePIDFilter, 0, StreamBitmask, TRUE);	/* Re-enable PID filter */

			AnInterruptWasServiced = TRUE;
		}
#endif

		/* To prevent a lockup scenario, we check and report unhandled interrupts */
		if ((!AnInterruptWasServiced) && (TP_Status0 != 0)) {
			/* Unhandled interrupt */
			stptiSupport_WriteReg32(&TP_p->Mailbox0ToHost_p->Clear, TP_Status0);

			/* Avoid flooding the debug print buffer with unhandled interrupt messages */
			if (pDevice_p->TP_UnhandledInterrupts < 10) {
				snprintf(stptiHAL_ISRstring, sizeof(stptiHAL_ISRstring),
					 "ISR%01u: Unhandled TP Interrupt (Mbx0:%08x)",
					 stptiOBJMAN_GetpDeviceIndex(FullpDeviceHandle),
					 TP_Status0);
				STPTI_INTERRUPT_PRINT("", stptiHAL_ISRstring);
			}

			if (pDevice_p->TP_UnhandledInterrupts < 0xFFFFFFFF) {
				pDevice_p->TP_UnhandledInterrupts++;
			}

			AnInterruptWasServiced = TRUE;
		}

	}

	if (AnInterruptWasServiced) {
		ret = IRQ_HANDLED;
	}

	return ret;
}
