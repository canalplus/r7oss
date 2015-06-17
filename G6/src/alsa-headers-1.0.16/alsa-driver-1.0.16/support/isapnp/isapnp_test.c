/*
 *  ISA Plug & Play support
 *  Copyright (c) by Jaroslav Kysela <perex@perex.cz>
 *
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 *
 */

#include <linux/config.h>
#include <linux/version.h>

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 2, 0)
#error "This driver is designed only for Linux 2.2.0 and higher."
#endif
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 3, 11)
#define NEW_RESOURCE
#endif

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/ioport.h>
#include <linux/string.h>
#include <linux/slab.h>
#include <linux/proc_fs.h>
#include <asm/io.h>
#include <asm/dma.h>
#include <asm/irq.h>
#include <linux/pci.h>
#include <linux/vmalloc.h>
#include <linux/poll.h>
#include <linux/init.h>
#include <asm/uaccess.h>
#ifndef ALSA_BUILD
#include <linux/isapnp.h>
#else
#include "isapnp.h"
#endif

#define USER_PORT_AUTO_VALUE	(~0)

static int user_port = USER_PORT_AUTO_VALUE;
MODULE_PARM(user_port, "i");

static struct isapnp_dev *dev = NULL;

int init_module(void)
{
	/* find the first game port, use standard PnP IDs */
	dev = isapnp_find_dev(NULL,
			      ISAPNP_VENDOR('P','N','P'),
			      ISAPNP_FUNCTION(0xb02f),
			      NULL);
	printk("dev = 0x%lx\n", (long)dev);
	if (!dev)
		return -ENODEV;
	if (dev->prepare(dev)<0)
		return -EAGAIN;
	if (!dev->ro) {
		/* override resource */
		if (user_port != USER_PORT_AUTO_VALUE)
			dev->resource[0].start = user_port;
	}
	if (dev->activate(dev)<0) {
		printk("isapnp configure failed (out of resources?)\n");
		return -ENOMEM;
	}
	user_port = dev->resource[0].start;		/* get real port */
	return 0;
}

void cleanup_module(void)
{
 	if (dev)
 		dev->deactivate(dev);
}
