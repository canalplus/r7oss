/************************************************************************
 * Copyright (C) 2011 STMicroelectronics. All Rights Reserved.
 *
 * This file is part of the STLinuxTV Library.
 *
 * STLinuxTV is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License version 2 as published by the
 * Free Software Foundation.
 *
 * STLinuxTV is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with STLinuxTV; see the file COPYING.  If not, write to the Free Software
 * Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * The STLinuxTV Library may alternatively be licensed under a proprietary
 * license from ST.
 *
 * Source file name : cec_internal.h
 * Author :           Nidhi Chadha
 *
 * Header for cec user layer adaptation
 * *
 * Date        Modification                                    Name
 * ----        ------------                                    --------
 * 22-10-2012   Created                                         nidhic
 *
 *   ************************************************************************/

#ifndef _CEC_INTERNAL_H
#define  _CEC_INTERNAL_H

#include <linux/cdev.h>		/* cdev_alloc */
#include "linux/stmcec.h"		/* cec io file */
#include "stm_cec.h"		/* stkpi cec driver interface file */
#include "stm_event.h"

/* Enable/disable debug-level macros.
 *
 * This really should be off by default.
 * If you *really* need your message to hit the console every time
 * then that is what CEC_TRACE() is for.
 */

/*Uncomment in final release */

#ifndef ENABLE_CEC_DEBUG
#define ENABLE_CEC_DEBUG                0
#endif

/* Comment in final release */
/* #define ENABLE_CEC_DEBUG      1 */

#define CEC_DEBUG_MSG(fmt, args...)\
	((void) (ENABLE_CEC_DEBUG &&\
	(printk(KERN_INFO "%s: " fmt, __func__, ##args), 0)))

/* Output trace information off the critical path */
#define CEC_TRACE_MSG(fmt, args...)\
	(printk(KERN_NOTICE "%s: " fmt, __func__, ##args))

/* Output errors, should never be output in 'normal' operation */
#define CEC_ERROR_MSG(fmt, args...)\
	printk(KERN_CRIT "ERROR in %s: " fmt, __func__, ##args)

#define CEC_ASSERT(x)\
	do { if (!(x))\
	printk(KERN_CRIT "%s: Assertion '%s' failed at %s:%d\n", \
	__func__, #x, __FILE__, __LINE__);\
	} while (0);

struct stm_cec {
	stm_event_subscription_h subscription;
	stm_cec_h cec_device;
	struct cdev cdev;
	struct class *cdev_class;
	struct device *cdev_class_dev;
	uint8_t cec_address[4];
	uint8_t logical_addr;
	stm_cec_msg_t cur_msg;
	stm_cec_msg_t cur_reply;
	uint8_t cur_phy_addr[4];
	uint8_t exit;
	/*
	 * @cecdev_mutex: This mutex is to synchronize open/closure of device.
	 *                This also allows to have ioctls/rw ops to be
	 *                independent of another open. At the moment, only one
	 *                open is allowed, but a second open which would be
	 *                unsuccessful will not block any operation
	 * @cecops_mutex: This mutex is to synchronize the ioctls on the device
	 * @cerw_mutex  : This mutex is to synchronize the ioctls which could be
	 *                affected by read/write operations and is also used to
	 *                synchronize read/write operations
	 */
	struct mutex cecdev_mutex;
	struct mutex cecops_mutex;
	struct mutex cecrw_mutex;
	wait_queue_head_t wait_queue;
};

long stmcec_ioctl(struct file *filp, unsigned int cmd, unsigned long arg);

#endif /* _CEC_INTERNAL_H */
