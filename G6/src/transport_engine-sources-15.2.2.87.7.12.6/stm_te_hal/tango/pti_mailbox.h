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
   @file   pti_mailbox.h
   @brief  Mailbox handling

   This file implements the mailbox handling for (mainly interrupt related)
   comms between the HOST and STxP70s.  It intentionally makes the interface very
   clear as to which way the information is flowing.

   This file should ONLY contain function prototypes and constants that are
   INTERNAL to the stptiHAL.  i.e. with the exception of the HAL object
   registration function in pti_driver.c, NOTHING outside of the tango
   directory should be referring to this file.

   Defines and enums relevent to the API of the HAL must be put into
   pti_hal_api.h and will be shared across all HALs.

 */

#ifndef _PTI_MAILBOX_H_
#define _PTI_MAILBOX_H_

/* Includes ---------------------------------------------------------------- */

/* ANSI C includes */

/* STAPI includes */

/* Includes from API level */

/* Includes from the HAL / ObjMan level */
#include "../objman/pti_object.h"

/* Exported Types ---------------------------------------------------------- */

/* The mailbox enum is split into two so that lint/coverity checking will detect
   if the wrong typedef is used with the wrong function (it is also checked at
   runtime).  Shame that gcc doesn't deploy this kind of type checking */

typedef enum {
	/* Items for writing to TP Mailboxes - stptiHAL_MailboxPost() */
	MAILBOX_TP_PRINTF_COMPLETE = 0,
	MAILBOX_START_PLAYBACK,
	MAILBOX_STOP_PLAYBACK,
	MAILBOX_UPDATING_NEW_PID_TABLE,
	MAILBOX_SUPPLYING_INIT_PARAMS,
	MAILBOX_START_LIVE,
	MAILBOX_STOP_LIVE,
	MAILBOX_ALL_OUTGOING_TP_ITEMS,

	MAILBOX_LAST_OUTGOING_ITEM /* leave as last item */
} stptiHAL_MailboxOutgoingItem_t;

typedef enum {
	/* Items for reading from HOST Mailboxes - stptiHAL_MailboxQuery() */
	MAILBOX_IS_TP_PRINTF_WAITING = MAILBOX_LAST_OUTGOING_ITEM + 1,
	MAILBOX_CHECK_FOR_PLAYBACK_COMPLETION,
	MAILBOX_HAS_A_BUFFER_SIGNALLED,
	MAILBOX_TP_ACKNOWLEDGE,
	MAILBOX_TP_STOP_STFE_FLUSH,
	MAILBOX_TP_START_STFE_FLUSH,
	MAILBOX_CHECK_FOR_STATUS_BLOCK_OVERFLOW,
	MAILBOX_CHECK_FOR_STATUS_BLOCK,
	MAILBOX_ALL_INCOMING_TP_ITEMS
} stptiHAL_MailboxIncomingItem_t;

/* Exported Function Prototypes -------------------------------------------- */

ST_ErrorCode_t stptiHAL_MailboxPost(FullHandle_t HandleForDevice, stptiHAL_MailboxOutgoingItem_t MailboxItemType,
				    U32 Value);
ST_ErrorCode_t stptiHAL_MailboxQueryAndClear(FullHandle_t HandleForDevice,
					     stptiHAL_MailboxIncomingItem_t MailboxItemType, U32 *Value_p);
ST_ErrorCode_t stptiHAL_MailboxInterruptCtrl(FullHandle_t HandleForDevice,
					     stptiHAL_MailboxIncomingItem_t MailboxItemType, BOOL Enable);

U32 stptiHAL_MailboxInterruptTP(FullHandle_t HandleForDevice);

#endif /* _PTI_MAILBOX_H_ */
