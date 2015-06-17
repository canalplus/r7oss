/*
 *  Copyright (c) by Jaroslav Kysela <perex@perex.cz>
 *  Routines for control of MPU-401 in UART mode
 *
 *  MPU-401 supports UART mode which is not capable generate transmit
 *  interrupts thus output is done via polling. Also, if irq < 0, then
 *  input is done also via polling. Do not expect good performance.
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

#define __NO_VERSION__
#include "adriver.h"
#include <asm/io.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/ioport.h>
#include <linux/sched.h>
#include <linux/errno.h>
#include <sound/core.h>
#include <sound/rawmidi.h>

#include "msnd.h"
#ifdef MSND_CLASSIC
#  ifdef CONFIG_MSNDCLAS_HAVE_BOOT
#    define HAVE_DSPCODEH
#  endif
#  include "msnd_classic.h"
#  define LOGNAME			"msnd_classic"
#else
#  ifdef CONFIG_MSNDPIN_HAVE_BOOT
#    define HAVE_DSPCODEH
#  endif
#  include "msnd_pinnacle.h"
#  define LOGNAME			"snd_msnd_pinnacle"
#endif


#define MSNDMIDI_MODE_BIT_INPUT		0
#define MSNDMIDI_MODE_BIT_OUTPUT		1
#define MSNDMIDI_MODE_BIT_INPUT_TRIGGER	2
#define MSNDMIDI_MODE_BIT_OUTPUT_TRIGGER	3

#define MSNDMIDI_MODE_INPUT		(1<<MSNDMIDI_MODE_BIT_INPUT)
#define MSNDMIDI_MODE_OUTPUT		(1<<MSNDMIDI_MODE_BIT_OUTPUT)
#define MSNDMIDI_MODE_INPUT_TRIGGER	(1<<MSNDMIDI_MODE_BIT_INPUT_TRIGGER)
#define MSNDMIDI_MODE_OUTPUT_TRIGGER	(1<<MSNDMIDI_MODE_BIT_OUTPUT_TRIGGER)

#define MSNDMIDI_MODE_INPUT_TIMER		(1<<0)
#define MSNDMIDI_MODE_OUTPUT_TIMER	(1<<1)


#ifdef SND_MSNDMIDI_OUTPUT
static void snd_msndmidi_output_write(msndmidi_t * mpu);
#endif
/*

 */

#define snd_msndmidi_input_avail(mpu)	(!(inb(MPU401C(mpu)) & 0x80))
#define snd_msndmidi_output_ready(mpu)	(!(inb(MPU401C(mpu)) & 0x40))

#define MPU401_RESET		0xff
#define MPU401_ENTER_UART	0x3f
#define MPU401_ACK		0xfe


typedef struct _snd_msndmidi msndmidi_t;

struct _snd_msndmidi {
	struct snd_rawmidi *rmidi;
	multisound_dev_t *dev;

	unsigned long mode;		/* MSNDMIDI_MODE_XXXX */
	int timer_invoked;

	void *private_data;

	struct snd_rawmidi_substream *substream_input;
	struct snd_rawmidi_substream *substream_output;

	spinlock_t input_lock;
	spinlock_t output_lock;
	spinlock_t timer_lock;

	struct timer_list timer;
};


/* not used static void snd_msnd_midi_clear_rx(msndmidi_t *mpu)
{/ *	int timeout = 100000;
	for (; timeout > 0 && snd_msndmidi_input_avail(mpu); timeout--)
		inb(MPU401D(mpu));
#ifdef CONFIG_SND_DEBUG
	if (timeout <= 0)
		snd_printk("cmd: clear rx timeout (status = 0x%x)\n", inb(MPU401C(mpu)));
#endif
* /
}           */

/*
static void _snd_msndmidi_interrupt(msndmidi_t *mpu)
{
	if (test_bit(MPU401_MODE_BIT_INPUT, &mpu->mode))
		snd_msndmidi_input_read(mpu);
	else
		snd_msndmidi_clear_rx(mpu);
	/ * ok. for better Tx performance try do some output when input is done * /
	if (test_bit(MPU401_MODE_BIT_OUTPUT, &mpu->mode))
		snd_msndmidi_output_write(mpu);
}

void snd_msndmidi_interrupt(int irq, void *dev_id, struct pt_regs *regs)
{
	msndmidi_t *mpu = dev_id;
	
	if (mpu == NULL)
		return;
	_snd_msndmidi_interrupt(mpu);
}*/
/*
static void snd_msndmidi_timer(unsigned long data)
{
	unsigned long flags;
	msndmidi_t *mpu = (void *)data;

	spin_lock_irqsave(&mpu->timer_lock, flags);
	/ * mpu->mode |= MPU401_MODE_TIMER;* /
	mpu->timer.expires = 1 + jiffies;
	add_timer(&mpu->timer);
	spin_unlock_irqrestore(&mpu->timer_lock, flags);
	if (mpu->rmidi)
		_snd_msndmidi_interrupt(mpu);
}

static void snd_msndmidi_add_timer (msndmidi_t *mpu, int input)
{
	unsigned long flags;

	spin_lock_irqsave (&mpu->timer_lock, flags);
	if (mpu->timer_invoked == 0) {
		mpu->timer.data = (unsigned long)mpu;
		mpu->timer.function = snd_msndmidi_timer;
		mpu->timer.expires = 1 + jiffies;
		add_timer(&mpu->timer);
	}
	mpu->timer_invoked |= input ? MPU401_MODE_INPUT_TIMER : MPU401_MODE_OUTPUT_TIMER;
	spin_unlock_irqrestore (&mpu->timer_lock, flags);
}

static void snd_msndmidi_remove_timer (msndmidi_t *mpu, int input)
{
	unsigned long flags;

	spin_lock_irqsave (&mpu->timer_lock, flags);
	if (mpu->timer_invoked) {
		mpu->timer_invoked &= input ? ~MPU401_MODE_INPUT_TIMER : ~MPU401_MODE_OUTPUT_TIMER;
		if (! mpu->timer_invoked)
			del_timer(&mpu->timer);
	}
	spin_unlock_irqrestore (&mpu->timer_lock, flags);
} */

/*

 */

/* not used so far .... ?
static void snd_msndmidi_cmd(msndmidi_t * mpu, unsigned char cmd, int ack)
{
	unsigned long flags;
	int timeout, ok;

	spin_lock_irqsave(&mpu->input_lock, flags);
/ *	if (mpu->hardware != MPU401_HW_TRID4DWAVE) {
		outb(0x00, MPU401D(mpu));
		/ *snd_msndmidi_clear_rx(mpu);* /
	}
	/ * ok. standard MPU-401 initialization * /
	if (mpu->hardware != MPU401_HW_SB) {
		for (timeout = 1000; timeout > 0 && !snd_msndmidi_output_ready(mpu); timeout--)
			udelay(10);
#ifdef CONFIG_SND_DEBUG
		if (!timeout)
			snd_printk("cmd: tx timeout (status = 0x%x)\n", inb(MPU401C(mpu)));
#endif
	}
	outb(cmd, MPU401C(mpu));
	if (ack) {
		ok = 0;
		timeout = 10000;
		while (!ok && timeout-- > 0) {
			if (snd_msndmidi_input_avail(mpu)) {
				if (inb(MPU401D(mpu)) == MPU401_ACK)
					ok = 1;
			}
		}
		if (!ok && inb(MPU401D(mpu)) == MPU401_ACK)
			ok = 1;
	} else {
		ok = 1;
	}* /
	spin_unlock_irqrestore(&mpu->input_lock, flags);
//	if (! ok)
//		snd_printk("cmd: 0x%x failed at 0x%lx (status = 0x%x, data = 0x%x)\n", cmd, mpu->port, inb(MPU401C(mpu)), inb(MPU401D(mpu)));
	// snd_printk("cmd: 0x%x at 0x%lx (status = 0x%x, data = 0x%x)\n", cmd, mpu->port, inb(MPU401C(mpu)), inb(MPU401D(mpu)));
}          */

/*
 * input/output open/close - protected by open_mutex in rawmidi.c
 */
static int snd_msndmidi_input_open(struct snd_rawmidi_substream *substream)
{
	msndmidi_t *mpu;
//	int err;
#ifdef CONFIG_SND_DEBUG0
	printk( "snd_msndmidi_input_open(struct snd_rawmidi_substream *substream)\n");
#endif

	mpu = substream->rmidi->private_data;
/*	if (mpu->open_input && (err = mpu->open_input(mpu)) < 0)
		return err;
	if (! test_bit(MPU401_MODE_BIT_OUTPUT, &mpu->mode)) {
		snd_msndmidi_cmd(mpu, MPU401_RESET, 1);
		snd_msndmidi_cmd(mpu, MPU401_ENTER_UART, 1);
	}*/

	mpu->substream_input = substream;

	//  SetMidiInPort( EXTIN_MIP);
	snd_msnd_enable_irq( mpu->dev);

	snd_msnd_send_dsp_cmd( mpu->dev, HDEX_MIDI_IN_START);
	set_bit(MSNDMIDI_MODE_BIT_INPUT, &mpu->mode);
	return 0;
}

#ifdef SND_MSNDMIDI_OUTPUT
static int snd_msndmidi_output_open(struct snd_rawmidi_substream *substream)
{
	msndmidi_t *mpu;
	int err;

	mpu = substream->rmidi->private_data;
/*	if (mpu->open_output && (err = mpu->open_output(mpu)) < 0)
		return err;
	if (! test_bit(MPU401_MODE_BIT_INPUT, &mpu->mode)) {
		snd_msndmidi_cmd(mpu, MPU401_RESET, 1);
		snd_msndmidi_cmd(mpu, MPU401_ENTER_UART, 1);
	}*/
	mpu->substream_output = substream;
	set_bit(MSNDMIDI_MODE_BIT_OUTPUT, &mpu->mode);
	return 0;
}
#endif

static int snd_msndmidi_input_close(struct snd_rawmidi_substream *substream)
{
	msndmidi_t *mpu;

#ifdef CONFIG_SND_DEBUG0
	printk( "snd_msndmidi_input_close(struct snd_rawmidi_substream *substream)\n");
#endif

	mpu = substream->rmidi->private_data;
	snd_msnd_send_dsp_cmd( mpu->dev, HDEX_MIDI_IN_STOP);
	clear_bit(MSNDMIDI_MODE_BIT_INPUT, &mpu->mode);
	mpu->substream_input = NULL;
	snd_msnd_disable_irq( mpu->dev);
/*	if (! test_bit(MPU401_MODE_BIT_OUTPUT, &mpu->mode))
		snd_msndmidi_cmd(mpu, MPU401_RESET, 0);
	if (mpu->close_input)
		mpu->close_input(mpu);*/
	return 0;
}

#ifdef SND_MSNDMIDI_OUTPUT
static int snd_msndmidi_output_close(struct snd_rawmidi_substream *substream)
{
	msndmidi_t *mpu;

	mpu = substream->rmidi->private_data;
	clear_bit(MSNDMIDI_MODE_BIT_OUTPUT, &mpu->mode);
	mpu->substream_output = NULL;
/*	if (! test_bit(MPU401_MODE_BIT_INPUT, &mpu->mode))
		snd_msndmidi_cmd(mpu, MPU401_RESET, 0);
	if (mpu->close_output)
		mpu->close_output(mpu);*/
	return 0;
}
#endif

static void inline snd_msndmidi_input_drop(msndmidi_t * mpu)
{
	WORD tail;

	tail = isa_readw(mpu->dev->MIDQ + JQS_wTail);
	isa_writew(tail, mpu->dev->MIDQ + JQS_wHead);
}

/*
 * trigger input
 */
static void snd_msndmidi_input_trigger(struct snd_rawmidi_substream *substream, int up)
{
	unsigned long flags;
	msndmidi_t *mpu;
//	int max = 64;
#ifdef CONFIG_SND_DEBUG0
	printk( "snd_msndmidi_input_trigger(, %i)\n", up);
#endif

	mpu = substream->rmidi->private_data;
	spin_lock_irqsave(&mpu->input_lock, flags);
	if (up) {
		if (! test_and_set_bit(MSNDMIDI_MODE_BIT_INPUT_TRIGGER, &mpu->mode))
			snd_msndmidi_input_drop(mpu);
	} else {
		clear_bit(MSNDMIDI_MODE_BIT_INPUT_TRIGGER, &mpu->mode);
	}
	spin_unlock_irqrestore(&mpu->input_lock, flags);
	if (up)
		snd_msndmidi_input_read(mpu);
}

void snd_msndmidi_input_read( void* mpuv)
{
//unsigned char byte;
	unsigned long flags;
msndmidi_t * mpu = mpuv;

	spin_lock_irqsave(&mpu->input_lock, flags);
	while (isa_readw( mpu->dev->MIDQ + JQS_wTail) != isa_readw( mpu->dev->MIDQ + JQS_wHead)) {
	WORD wTmp, val;
		val = isa_readw( mpu->dev->pwMIDQData + 2*isa_readw( mpu->dev->MIDQ + JQS_wHead));

			if (test_bit( MSNDMIDI_MODE_BIT_INPUT_TRIGGER, &mpu->mode)) {
//		printk( "MID: 0x%04X\n", (unsigned)val);
				snd_rawmidi_receive(mpu->substream_input, (unsigned char*)&val, 1);
			}

		if ((wTmp = isa_readw( mpu->dev->MIDQ + JQS_wHead) + 1) > isa_readw( mpu->dev->MIDQ + JQS_wSize))
			isa_writew(0,  mpu->dev->MIDQ + JQS_wHead);
		else
			isa_writew(wTmp,  mpu->dev->MIDQ + JQS_wHead);
	}
	spin_unlock_irqrestore(&mpu->input_lock, flags);
}

/*
 *  Tx FIFO sizes:
 *    CS4237B			- 16 bytes
 *    AudioDrive ES1688         - 12 bytes
 *    S3 SonicVibes             -  8 bytes
 *    SoundBlaster AWE 64       -  2 bytes (ugly hardware)
 */
#ifdef SND_MSNDMIDI_OUTPUT
static void snd_msndmidi_output_write(msndmidi_t * mpu)
{
	unsigned long flags;
	unsigned char byte;
	int max = 256, timeout;

	if (!test_bit(MSNDMIDI_MODE_BIT_OUTPUT_TRIGGER, &mpu->mode))
		return;
	do {
		spin_lock_irqsave(&mpu->output_lock, flags);
		if (snd_rawmidi_transmit_peek(mpu->substream_output, &byte, 1) == 1) {
			for (timeout = 100; timeout > 0; timeout--) {
				if (snd_msndmidi_output_ready(mpu)) {
//					outb(byte, MPU401D(mpu));
					snd_rawmidi_transmit_ack(mpu->substream_output, 1);
					break;
				}
			}
		} else {
			snd_msndmidi_remove_timer (mpu, 0);
			max = 1; /* no other data - leave the tx loop */
		}
		spin_unlock_irqrestore(&mpu->output_lock, flags);
	} while (--max > 0);
}

static void snd_msndmidi_output_trigger(struct snd_rawmidi_substream *substream, int up)
{
	unsigned long flags;
	msndmidi_t *mpu;

	mpu = substream->rmidi->private_data;
	spin_lock_irqsave(&mpu->output_lock, flags);
	if (up) {
		set_bit(MSNDMIDI_MODE_BIT_OUTPUT_TRIGGER, &mpu->mode);
		snd_msndmidi_add_timer(mpu, 0);
	} else {
		snd_msndmidi_remove_timer(mpu, 0);
		clear_bit(MSNDMIDI_MODE_BIT_OUTPUT_TRIGGER, &mpu->mode);
	}
	spin_unlock_irqrestore(&mpu->output_lock, flags);
	if (up)
		snd_msndmidi_output_write(mpu);
}

/*

 */

static struct snd_rawmidi_ops snd_msndmidi_output =
{
	.open =		snd_msndmidi_output_open,
	.close =	snd_msndmidi_output_close,
	.trigger =	snd_msndmidi_output_trigger,
};
#endif

static struct snd_rawmidi_ops snd_msndmidi_input =
{
	.open =		snd_msndmidi_input_open,
	.close =	snd_msndmidi_input_close,
	.trigger =	snd_msndmidi_input_trigger,
};

static void snd_msndmidi_free(struct snd_rawmidi *rmidi)
{
	msndmidi_t *mpu = rmidi->private_data;
/*	if (mpu->irq_flags && mpu->irq >= 0)
		free_irq(mpu->irq, (void *) mpu);
	if (mpu->res) {
		release_resource(mpu->res);
		kfree_nocheck(mpu->res);
	}*/
	kfree(mpu);
}

int snd_msndmidi_new(struct snd_card *card, int device, multisound_dev_t *dev)
{
	msndmidi_t *mpu;
	struct snd_rawmidi *rmidi;
	int err;

	if ((err = snd_rawmidi_new(card, "MSND-MIDI", device, 1, 1, &rmidi)) < 0)
		return err;
	mpu = kcalloc(1, sizeof(*mpu), GFP_KERNEL);
	if (mpu == NULL) {
		snd_device_free(card, rmidi);
		return -ENOMEM;
	}
	mpu->dev = dev;
	dev->msndmidi_mpu = mpu;
	rmidi->private_data = mpu;
	rmidi->private_free = snd_msndmidi_free;
	spin_lock_init(&mpu->input_lock);
	spin_lock_init(&mpu->output_lock);
	spin_lock_init(&mpu->timer_lock);
/*	if (!integrated) {
		if ((mpu->res = request_region(port, 2, "MPU401 UART")) == NULL) {
			snd_device_free(card, rmidi);
			return -EBUSY;
		}
	}
	mpu->port = port;
	if (irq >= 0 && irq_flags) {
		if (request_irq(irq, snd_msndmidi_interrupt, irq_flags, "MPU401 UART", (void *) mpu)) {
			snd_printk("unable to grab IRQ %d\n", irq);
			snd_device_free(card, rmidi);
			return -EBUSY;
		}
		mpu->irq = irq;
		mpu->irq_flags = irq_flags;
	}*/
	strcpy(rmidi->name, "MSND MIDI");
#ifdef SND_MSNDMIDI_OUTPUT
	snd_rawmidi_set_ops(rmidi, SNDRV_RAWMIDI_STREAM_OUTPUT, &snd_msndmidi_output);
#endif
	snd_rawmidi_set_ops(rmidi, SNDRV_RAWMIDI_STREAM_INPUT, &snd_msndmidi_input);
	rmidi->info_flags |=
#ifdef SND_MSNDMIDI_OUTPUT
				SNDRV_RAWMIDI_INFO_OUTPUT |
	                     SNDRV_RAWMIDI_INFO_DUPLEX |
#endif
	                     SNDRV_RAWMIDI_INFO_INPUT ;
	mpu->rmidi = rmidi;
	return 0;
}
