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
   @file   pti_mailbox.c
   @brief  Mailbox handling

   This file implements the mailbox handling for (mainly interrupt related)
   comms between the HOST and STxP70s.  It intentionally makes the interface very
   clear as to which way the information is flowing.

 */

#if 0
#define STPTI_PRINT
#endif

/* Includes ---------------------------------------------------------------- */

/* ANSI C includes */

/* STAPI includes */
#include "stddefs.h"

/* Includes from API level */
#include "../pti_osal.h"

/* Includes from the HAL / ObjMan level */
#include "pti_mailbox.h"
#include "pti_pdevice.h"
#include "firmware/pti_tp_api.h"

/* MACROS ------------------------------------------------------------------ */
/* Private Constants ------------------------------------------------------- */

/*
   This describes the usage of the TP MAILBOXes.

   (I) means the driver will enable it to raise an interrupt on the Host on initialisation.

Written by host...

HOST_TO_TP_MAILBOX0     B31    DEBUG_PRINTF printed out (by host) ready for another (same as SecureCoPro)
                        B30-24 (unused)
                        B23:16 start PB_Node7:0 (sw inject)
                        B15    Supply initialisation parameters
                        B14:9  (unused)
                        B8     stop live
                        B7:1   (unused)
                        B0     start live

Read by host...

TP_TO_HOST_MAILBOX0 (I) B31    DEBUG_PRINTF ready for printing  (same as SecureCoPro)
                        B30-17 DEBUG_PRINTF offset              (same as SecureCoPro)
                        B16    (unused)
                    (I) B15-8  Playback (sw inject) node done
                        B7-6   (unused)
                    (I) B5     Stop STFE Flush
                    (I) B4     Start STFE Flush
                        B3     Status Block overflow
                    (I) B2     Status Block Received
                    (I) B1     Buffer Threshold reached (equiv. to PTI pkt signal)
                    (I) B0     TP Initial config done / Host Sync (PBnode/Node ack)

*/

/* Private Variables ------------------------------------------------------- */
/* Private Function Prototypes --------------------------------------------- */

/* Functions --------------------------------------------------------------- */

/**
   @brief  Posts a signal to a STxP70 Mailbox.

   This raises a signal (or group of signals) on the relevent Mailbox depending on MailboxItemType.
   For groups of signals the parameter value indicates which signals are enabled (Bit0 being the
   first relevent signal).

   @param  HandleForDevice  Any object handle to detect which TANGO device we are using.

   @param  MailboxItemType  One of the following...
                            - MAILBOX_TP_PRINTF_COMPLETE
                            - MAILBOX_START_PLAYBACK
                            - MAILBOX_STOP_PLAYBACK
                            - MAILBOX_SUPPLYING_INIT_PARAMS
                            - MAILBOX_START_LIVE
                            - MAILBOX_STOP_LIVE

   @param  Value            If MailboxItemType is a group indicates which signal in the group
                            (Bit0 is first Signal)

   @return                  A standard st error type...
                            - ST_ERROR_BAD_PARAMETER if an incorrect MailboxItemType supplied.
                            - ST_NO_ERROR if no errors
 */
ST_ErrorCode_t stptiHAL_MailboxPost(FullHandle_t HandleForDevice, stptiHAL_MailboxOutgoingItem_t MailboxItemType,
				    U32 Value)
{
	stptiHAL_TangoMailboxDevice_t *mbx_p =
	    stptiHAL_GetObjectpDevice_p(HandleForDevice)->TP_MappedAddresses.Mailbox0ToXP70_p;
	U32 Signal = 0;

	Value = Value;		/* Avoid compiler warning */

	if (mbx_p != NULL) {
		switch (MailboxItemType) {
			/* Items for writing to TP Mailboxes - stptiHAL_MailboxPost() */
		case MAILBOX_TP_PRINTF_COMPLETE:
			Signal = stptiTP_H2TP_MAILBOX0_TP_PRINTF_COMPLETE_MASK;
			break;

		case MAILBOX_START_PLAYBACK:
			Signal = stptiTP_H2TP_MAILBOX0_START_PLAYBACK_MASK;
			break;

		case MAILBOX_UPDATING_NEW_PID_TABLE:
			Signal = stptiTP_H2TP_MAILBOX0_UPDATING_NEW_PID_TABLE;
			break;

		case MAILBOX_STOP_PLAYBACK:
			Signal = stptiTP_H2TP_MAILBOX0_STOP_PLAYBACK_MASK;
			break;

		case MAILBOX_SUPPLYING_INIT_PARAMS:
			Signal = stptiTP_H2TP_MAILBOX0_SUPPLYING_INIT_PARAMS_MASK;
			break;

		case MAILBOX_START_LIVE:
			Signal = stptiTP_H2TP_MAILBOX0_START_LIVE_MASK;
			break;

		case MAILBOX_STOP_LIVE:
			Signal = stptiTP_H2TP_MAILBOX0_STOP_LIVE_MASK;
			break;

		default:
			return (ST_ERROR_BAD_PARAMETER);
		}

		stptiSupport_WriteReg32(&mbx_p->Set, Signal);
	}

	return (ST_NO_ERROR);
}

/**
   @brief  Queries a Host Mailbox to see if a signal has been raised by an STxP70.

   This queries the mailbox registers to see if a signal (or group of signals) on the relevent host
   has been raised (the Mailbox depending on MailboxItemType).  The U32 pointed to by Value_p is
   set according to the signals raised (Bit0 being the first signal in a group).

   @param  HandleForDevice  Any object handle to detect which TANGO device we are using.

   @param  MailboxItemType  One of the following...
                            - MAILBOX_IS_TP_PRINTF_WAITING
                            - MAILBOX_CHECK_FOR_PLAYBACK_COMPLETION
                            - MAILBOX_HAS_A_BUFFER_SIGNALLED
                            - MAILBOX_TP_START_STFE_FLUSH
                            - MAILBOX_TP_STOP_STFE_FLUSH
                            - MAILBOX_TP_ACKNOWLEDGE

   @param  Value_p          A pointer to a U32 to contain the signals raised (Bit0 is first Signal)

   @return                  A standard st error type...
                            - ST_ERROR_BAD_PARAMETER if an incorrect MailboxItemType supplied.
                            - ST_NO_ERROR if no errors
 */
ST_ErrorCode_t stptiHAL_MailboxQueryAndClear(FullHandle_t HandleForDevice,
					     stptiHAL_MailboxIncomingItem_t MailboxItemType, U32 *Value_p)
{
	stptiHAL_TangoMailboxDevice_t *mbx_p =
	    stptiHAL_GetObjectpDevice_p(HandleForDevice)->TP_MappedAddresses.Mailbox0ToHost_p;
	U32 Signal = 0;

	if (mbx_p != NULL) {
		switch (MailboxItemType) {
			/* Items for reading from HOST Mailboxes - stptiHAL_MailboxQuery() */
		case MAILBOX_IS_TP_PRINTF_WAITING:
			Signal = stptiSupport_ReadReg32(&mbx_p->Status);
			Signal &= (stptiTP_TP2H_MAILBOX0_IS_TP_PRINTF_OFFSET_MASK |
					stptiTP_TP2H_MAILBOX0_IS_TP_PRINTF_WAITING_MASK);
			*Value_p = (Signal & stptiTP_TP2H_MAILBOX0_IS_TP_PRINTF_OFFSET_MASK) >>
					stptiTP_TP2H_MAILBOX0_IS_TP_PRINTF_OFFSET_OFFSET;
			break;

		case MAILBOX_CHECK_FOR_PLAYBACK_COMPLETION:
			Signal = stptiSupport_ReadReg32(&mbx_p->Status);
			Signal &= stptiTP_TP2H_MAILBOX0_CHECK_FOR_PLAYBACK_COMPLETION_MASK;
			*Value_p = Signal >> stptiTP_TP2H_MAILBOX0_CHECK_FOR_PLAYBACK_COMPLETION_OFFSET;
			break;

		case MAILBOX_TP_START_STFE_FLUSH:
			Signal = stptiSupport_ReadReg32(&mbx_p->Status);
			Signal &= stptiTP_TP2H_MAILBOX0_START_STFE_FLUSH_MASK;
			*Value_p = Signal >> stptiTP_TP2H_MAILBOX0_START_STFE_FLUSH_OFFSET;
			break;

		case MAILBOX_TP_STOP_STFE_FLUSH:
			Signal = stptiSupport_ReadReg32(&mbx_p->Status);
			Signal &= stptiTP_TP2H_MAILBOX0_STOP_STFE_FLUSH_MASK;
			*Value_p = Signal >> stptiTP_TP2H_MAILBOX0_STOP_STFE_FLUSH_OFFSET;
			break;

		case MAILBOX_HAS_A_BUFFER_SIGNALLED:
			Signal = stptiSupport_ReadReg32(&mbx_p->Status);
			Signal &= stptiTP_TP2H_MAILBOX0_HAS_A_BUFFER_SIGNALLED_MASK;
			*Value_p = Signal >> stptiTP_TP2H_MAILBOX0_HAS_A_BUFFER_SIGNALLED_OFFSET;
			break;

		case MAILBOX_CHECK_FOR_STATUS_BLOCK_OVERFLOW:
			Signal = stptiSupport_ReadReg32(&mbx_p->Status);
			Signal &= stptiTP_TP2H_MAILBOX0_STATUS_BLOCK_OVERFLOW_MASK;
			*Value_p = Signal >> stptiTP_TP2H_MAILBOX0_STATUS_BLOCK_OVERFLOW_OFFSET;
			break;

		case MAILBOX_CHECK_FOR_STATUS_BLOCK:
			Signal = stptiSupport_ReadReg32(&mbx_p->Status);
			Signal &= stptiTP_TP2H_MAILBOX0_STATUS_BLOCK_SIGNALLED_MASK;
			*Value_p = Signal >> stptiTP_TP2H_MAILBOX0_STATUS_BLOCK_SIGNALLED_OFFSET;
			break;

		case MAILBOX_TP_ACKNOWLEDGE:
			Signal = stptiSupport_ReadReg32(&mbx_p->Status);
			Signal &= stptiTP_TP2H_MAILBOX0_TP_ACKNOWLEDGE_MASK;
			*Value_p = Signal >> stptiTP_TP2H_MAILBOX0_TP_ACKNOWLEDGE_OFFSET;
			break;

		case MAILBOX_ALL_INCOMING_TP_ITEMS:
			Signal = 0xffffffff;
			*Value_p = 0;
			break;

		default:
			*Value_p = 0;
			return (ST_ERROR_BAD_PARAMETER);
		}

		stptiSupport_WriteReg32(&mbx_p->Clear, Signal);

	}

	return (ST_NO_ERROR);
}

/**
   @brief  Enable or disable a signal (or group of signals) in a mailbox for triggering an
   interrupt.

   @param  HandleForDevice  Any object handle to detect which TANGO device we are using.

   @param  MailboxItemType  One of the following...
                            - MAILBOX_IS_TP_PRINTF_WAITING
                            - MAILBOX_CHECK_FOR_PLAYBACK_COMPLETION         (group)
                            - MAILBOX_HAS_A_BUFFER_SIGNALLED
                            - MAILBOX_TP_START_STFE_FLUSH
                            - MAILBOX_TP_STOP_STFE_FLUSH
                            - MAILBOX_TP_ACKNOWLEDGE

   @param  Enable           Indicates if the Signal (or group of signals) should be interrupt
                            enabled or disabled.

   @return                  A standard st error type...
                            - ST_ERROR_BAD_PARAMETER if an incorrect MailboxItemType supplied.
                            - ST_NO_ERROR if no errors
 */
ST_ErrorCode_t stptiHAL_MailboxInterruptCtrl(FullHandle_t HandleForDevice,
					     stptiHAL_MailboxIncomingItem_t MailboxItemType, BOOL Enable)
{
	stptiHAL_TangoMailboxDevice_t *mbx_p =
	    stptiHAL_GetObjectpDevice_p(HandleForDevice)->TP_MappedAddresses.Mailbox0ToHost_p;
	U32 SignalMask = 0;

	if (mbx_p != NULL) {
		switch (MailboxItemType) {
			/* Items for reading from HOST Mailboxes - stptiHAL_MailboxQueryAndClear() */
		case MAILBOX_IS_TP_PRINTF_WAITING:
			SignalMask = stptiTP_TP2H_MAILBOX0_IS_TP_PRINTF_WAITING_MASK;
			break;

		case MAILBOX_CHECK_FOR_PLAYBACK_COMPLETION:
			SignalMask = stptiTP_TP2H_MAILBOX0_CHECK_FOR_PLAYBACK_COMPLETION_MASK;
			break;

		case MAILBOX_TP_START_STFE_FLUSH:
			SignalMask = stptiTP_TP2H_MAILBOX0_START_STFE_FLUSH_MASK;
			break;

		case MAILBOX_TP_STOP_STFE_FLUSH:
			SignalMask = stptiTP_TP2H_MAILBOX0_STOP_STFE_FLUSH_MASK;
			break;

		case MAILBOX_HAS_A_BUFFER_SIGNALLED:
			SignalMask = stptiTP_TP2H_MAILBOX0_HAS_A_BUFFER_SIGNALLED_MASK;
			break;

		case MAILBOX_CHECK_FOR_STATUS_BLOCK:
			SignalMask = stptiTP_TP2H_MAILBOX0_STATUS_BLOCK_SIGNALLED_MASK;
			break;

		case MAILBOX_TP_ACKNOWLEDGE:
			SignalMask = stptiTP_TP2H_MAILBOX0_TP_ACKNOWLEDGE_MASK;
			break;

		case MAILBOX_ALL_INCOMING_TP_ITEMS:
			if (Enable) {
				/* We don't let people enable all mailbox interrupts at once */
				return (ST_ERROR_BAD_PARAMETER);
			} else {
				stptiSupport_WriteReg32(&mbx_p->Mask, 0);
			}
			break;

		default:
			return (ST_ERROR_BAD_PARAMETER);
		}

		if (Enable) {
			stptiSupport_WriteReg32(&mbx_p->Mask, SignalMask | stptiSupport_ReadReg32(&mbx_p->Mask));
		} else {
			stptiSupport_WriteReg32(&mbx_p->Mask, ~(SignalMask) & stptiSupport_ReadReg32(&mbx_p->Mask));
		}
	}

	return (ST_NO_ERROR);
}

/**
   @brief  Return the active interrupts for the TP

   @param  HandleForDevice  Any object handle to detect which TANGO device we are using.

   @return                  Active interrupts
 */
U32 stptiHAL_MailboxInterruptTP(FullHandle_t HandleForDevice)
{
	U32 InterruptStatus = 0;
	stptiHAL_TangoMailboxDevice_t *mbx_p =
	    stptiHAL_GetObjectpDevice_p(HandleForDevice)->TP_MappedAddresses.Mailbox0ToHost_p;

	if (mbx_p != NULL) {
		InterruptStatus = stptiSupport_ReadReg32(&mbx_p->Status);
		InterruptStatus &= stptiSupport_ReadReg32(&mbx_p->Mask);
	}

	return (InterruptStatus);
}
