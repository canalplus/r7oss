/*********************************************************************
 *
 * 2002/06/30 Karsten Wiese:
 *	removed kernel-version dependencies.
 *	ripped from linux kernel 2.4.18 (OSS Implementation) by me.
 *	In the OSS Version, this file is compiled to a separate MODULE, that is used
 *	by the pinnacle and the classic driver.
 *	since there is no classic driver for alsa yet (i dont have a classic & writing one blindfold is difficult)
 *	this file's object is statically linked into the pinnacle-driver-module for now.
 *	look for the string
 *		"uncomment this to make this a module again"
 *	to do guess what.
 *
 * the following is a copy of the 2.4.18 OSS FREE file-heading comment:
 *
 * msnd.c - Driver Base
 *
 * Turtle Beach MultiSound Sound Card Driver for Linux
 *
 * Copyright (C) 1998 Andrew Veliath
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * $Id: msnd.c,v 1.17 1999/03/21 16:50:09 andrewtv Exp $
 *
 ********************************************************************/
#define __NO_VERSION__
#include "adriver.h"
#include <sound/core.h>
#include <sound/initval.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/types.h>
#include <linux/delay.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 10) /* enable_irq(), disable_irq() */
#include <linux/interrupt.h>
#else
#include <asm/irq.h>
#endif
#include <asm/io.h>
#include "msnd.h"

#define LOGNAME			"msnd"

#define MSND_MAX_DEVS		4

static multisound_dev_t		*devs[MSND_MAX_DEVS];
static int			num_devs;


int __init snd_msnd_register(multisound_dev_t *dev)
{
/* this is not really needed since alsa does the real (un)registration.
 * remove this later.
 */
	int i;

	for (i = 0; i < MSND_MAX_DEVS; ++i)
		if (devs[i] == NULL)
			break;

	snd_printd( "snd_msnd_register(): %i\n", i);


	if (i == MSND_MAX_DEVS)
		return -ENOMEM;

	devs[i] = dev;
	++num_devs;

//	MOD_INC_USE_COUNT;        uncomment this to make this a module again

	return 0;
}

void snd_msnd_unregister(multisound_dev_t *dev)
{
/* this is not really needed since alsa does the real (un)registration.
 * remove this later.
 */
	int i;

	for (i = 0; i < MSND_MAX_DEVS; ++i)
		if (devs[i] == dev)
			break;

	if (i == MSND_MAX_DEVS) {
		printk(KERN_WARNING LOGNAME ": Unregistering unknown device\n");
		return;
	}

	devs[i] = NULL;
	--num_devs;

//	MOD_DEC_USE_COUNT;         uncomment this to make this a module again
}

int snd_msnd_get_num_devs(void)
{
	return num_devs;
}

multisound_dev_t *snd_msnd_get_dev(int j)
{
	int i;

	for (i = 0; i < MSND_MAX_DEVS && j; ++i)
		if (devs[i] != NULL)
			--j;

	if (i == MSND_MAX_DEVS || j != 0)
		return NULL;

	return devs[i];
}

void snd_msnd_init_queue(unsigned long base, int start, int size)
{
	isa_writew(PCTODSP_BASED(start), base + JQS_wStart);
	isa_writew(PCTODSP_OFFSET(size) - 1, base + JQS_wSize);
	isa_writew(0, base + JQS_wHead);
	isa_writew(0, base + JQS_wTail);
}


int snd_msnd_wait_TXDE(multisound_dev_t *dev)
{
	register unsigned int io = dev->io;
	register int timeout = 1000;
    
	while(timeout-- > 0)
		if (inb(io + HP_ISR) & HPISR_TXDE)
			return 0;

	return -EIO;
}

int snd_msnd_wait_HC0(multisound_dev_t *dev)
{
	register unsigned int io = dev->io;
	register int timeout = 1000;

	while(timeout-- > 0)
		if (!(inb(io + HP_CVR) & HPCVR_HC))
			return 0;

	return -EIO;
}

int snd_msnd_send_dsp_cmd(multisound_dev_t *dev, BYTE cmd)
{
	unsigned long flags;

	spin_lock_irqsave(&dev->lock, flags);
	if (snd_msnd_wait_HC0(dev) == 0) {
		outb(cmd, dev->io + HP_CVR);
		spin_unlock_irqrestore(&dev->lock, flags);
		return 0;
	}
	spin_unlock_irqrestore(&dev->lock, flags);

	printk(KERN_DEBUG LOGNAME ": Send DSP command timeout\n");

	return -EIO;
}

int snd_msnd_send_word(multisound_dev_t *dev, unsigned char high,
		   unsigned char mid, unsigned char low)
{
	register unsigned int io = dev->io;

	if (snd_msnd_wait_TXDE(dev) == 0) {
		outb(high, io + HP_TXH);
		outb(mid, io + HP_TXM);
		outb(low, io + HP_TXL);
		return 0;
	}

	printk(KERN_DEBUG LOGNAME ": Send host word timeout\n");

	return -EIO;
}

int snd_msnd_upload_host(multisound_dev_t *dev, char *bin, int len)
{
	int i;

	if (len % 3 != 0) {
		printk(KERN_WARNING LOGNAME ": Upload host data not multiple of 3!\n");		
		return -EINVAL;
	}

	for (i = 0; i < len; i += 3)
		if (snd_msnd_send_word(dev, bin[i], bin[i + 1], bin[i + 2]) != 0)
			return -EIO;

	inb(dev->io + HP_RXL);
	inb(dev->io + HP_CVR);

	return 0;
}

int snd_msnd_enable_irq(multisound_dev_t *dev)
{
	unsigned long flags;

	if (dev->irq_ref++)
		return 0;

	printk(KERN_DEBUG LOGNAME ": Enabling IRQ\n");

	spin_lock_irqsave(&dev->lock, flags);
	if (snd_msnd_wait_TXDE(dev) == 0) {
		outb(inb(dev->io + HP_ICR) | HPICR_TREQ, dev->io + HP_ICR);
		if (dev->type == msndClassic)
			outb(dev->irqid, dev->io + HP_IRQM);
		outb(inb(dev->io + HP_ICR) & ~HPICR_TREQ, dev->io + HP_ICR);
		outb(inb(dev->io + HP_ICR) | HPICR_RREQ, dev->io + HP_ICR);
		enable_irq(dev->irq);
		snd_msnd_init_queue(dev->DSPQ, dev->dspq_data_buff, dev->dspq_buff_size);
		spin_unlock_irqrestore(&dev->lock, flags);
		return 0;
	}
	spin_unlock_irqrestore(&dev->lock, flags);

	printk(KERN_DEBUG LOGNAME ": Enable IRQ failed\n");

	return -EIO;
}

int snd_msnd_disable_irq(multisound_dev_t *dev)
{
	unsigned long flags;

	if (--dev->irq_ref > 0)
		return 0;

	if (dev->irq_ref < 0)
		printk(KERN_DEBUG LOGNAME ": IRQ ref count is %d\n", dev->irq_ref);

	printk(KERN_DEBUG LOGNAME ": Disabling IRQ\n");

	spin_lock_irqsave(&dev->lock, flags);
	if (snd_msnd_wait_TXDE(dev) == 0) {
		outb(inb(dev->io + HP_ICR) & ~HPICR_RREQ, dev->io + HP_ICR);
		if (dev->type == msndClassic)
			outb(HPIRQ_NONE, dev->io + HP_IRQM);
		disable_irq(dev->irq);
		spin_unlock_irqrestore(&dev->lock, flags);
		return 0;
	}
	spin_unlock_irqrestore(&dev->lock, flags);

	printk(KERN_DEBUG LOGNAME ": Disable IRQ failed\n");

	return -EIO;
}

#if 0

EXPORT_SYMBOL(snd_msnd_register);
EXPORT_SYMBOL(snd_msnd_unregister);
EXPORT_SYMBOL(snd_msnd_get_num_devs);
EXPORT_SYMBOL(snd_msnd_get_dev);

EXPORT_SYMBOL(snd_msnd_init_queue);

EXPORT_SYMBOL(snd_msnd_fifo_init);
EXPORT_SYMBOL(snd_msnd_fifo_free);
EXPORT_SYMBOL(snd_msnd_fifo_alloc);
EXPORT_SYMBOL(snd_msnd_fifo_make_empty);
EXPORT_SYMBOL(snd_msnd_fifo_write);
EXPORT_SYMBOL(snd_msnd_fifo_read);

EXPORT_SYMBOL(snd_msnd_wait_TXDE);
EXPORT_SYMBOL(snd_msnd_wait_HC0);
EXPORT_SYMBOL(snd_msnd_send_dsp_cmd);
EXPORT_SYMBOL(snd_msnd_send_word);
EXPORT_SYMBOL(snd_msnd_upload_host);

EXPORT_SYMBOL(snd_msnd_enable_irq);
EXPORT_SYMBOL(snd_msnd_disable_irq);

MODULE_AUTHOR				("Andrew Veliath <andrewtv@usa.net>");
MODULE_DESCRIPTION			("Turtle Beach MultiSound Driver Base");
MODULE_LICENSE("GPL");


int init_module(void)
{
	return 0;
}

void cleanup_module(void)
{
}

#endif
