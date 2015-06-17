/*
 *  Multi-Target Trace solution
 *
 *  MTT - sample software.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2
 * as published by the Free Software Foundation
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * Copyright (C) STMicroelectronics, 2011
 */
#ifndef _MTT_SAMPLE_H_
#define _MTT_SAMPLE_H_

#ifdef __KERNEL__
#include <linux/ioctl.h>
#include <linux/cdev.h>
#else
#include <sys/ioctl.h>
#endif

#define THIS_MINOR 0
#define DEVICE_NAME "mttsample"
#define CHARDEV_PATH "/tmp/mttsample"

#define BUF_LEN 80
#define SUCCESS 0
#define DELAY  (HZ/2)

/* Use 'K' as magic number */
#define DEMOAPP_IOC_MAGIC  'K'
#define DEMOAPP_IOCRESET _IO(DEMOAPP_IOC_MAGIC, 0)
#define DEMOAPP_IOCSTART _IO(DEMOAPP_IOC_MAGIC, 1)
#define DEMOAPP_IOCSTOP  _IO(DEMOAPP_IOC_MAGIC, 2)
#define DEMOAPP_IOC_MAXNR 3


/* Change this value to change the duration of the test */
#define MAX_BUFFS               200000

#define BUFF_SIZE               256
#define DMA_BUFFERS	        4
#define DMA_BUFFERS_MASK        0x3
#define SECOND                  1000000

#endif /*_MTT_EXAMPLE_H_*/
