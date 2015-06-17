/***********************************************************************
 *
 * File: linux/kernel/drivers/stm/stmcec.h
 * Copyright (c) 2007-2012 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef _STMCEC_H
#define _STMCEC_H

#include <linux/types.h>
/*
 * The following structure contents delivered on the cec bus using the read and write fops
 *
 */
struct stmcecio_msg {
	unsigned char data[16];
	unsigned char len;
};

/*
 * The following structure contents passed using STMCECIO_SET_LOGICAL_ADDRESS_ID will
 * change the Device's logical address used by the driver to recieve msgs intended for it.
 *
 */
struct stmcecio_logical_addr {
	unsigned char addr;
	unsigned char enable;	/* 1: enable/adds the given logical address,  0:disable the given logical address  */
};

/*
 * The following structure describes the event delivered by the device.
 *
 */

struct stmcec_event {
	__u32 type;
	union {
		__u32 id;
		__u32 reserved[8];
	};
};

#define STMCEC_EVENT_MSG_RECEIVED          	(1 << 0)
#define STMCEC_EVENT_MSG_ERROR_RECEIVED     (1 << 1)

/*
 * The following structure describes the event subscription for a given type.
 *
 */

struct stmcec_event_subscription {
	__u32 type;
	__u32 id;
};

#define STMCECIO_SET_LOGICAL_ADDRESS     _IOW ('C', 0x1, struct stmcecio_logical_addr)
#define STMCECIO_GET_LOGICAL_ADDRESS     _IOR ('C', 0x2, struct stmcecio_logical_addr)

#define STMCECIO_DQEVENT                 _IOR('C', 0x3, struct stmcec_event)
#define STMCECIO_SUBSCRIBE_EVENT         _IOW('C', 0x4, struct stmcec_event_subscription)
#define STMCECIO_UNSUBSCRIBE_EVENT       _IOW('C', 0x5, struct stmcec_event_subscription)

#endif /* _STMCEC_H */
