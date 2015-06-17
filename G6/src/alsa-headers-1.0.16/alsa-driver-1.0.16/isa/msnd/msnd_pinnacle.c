/*********************************************************************
 *
 * Linux multisound pinnacle/fiji driver for alsa 0.9rc2CVS
 *
 * 2002/06/30 Karsten Wiese:
 *	for now this is only used to build a pinnacle / fiji driver.
 *	the OSS parent of this code is designed to also support the multisound classic
 *	via the file msnd_classic.c.
 *	to make it easier for some brave heart to implemt classic support in alsa, i left
 *	all the MSND_CLASSIC tokens in this file. but for now this untested & undone.
 *
 *
 * ripped from linux kernel 2.4.18 by Karsten Wiese.
 *
 * the following is a copy of the 2.4.18 OSS FREE file-heading comment:
 *
 * Turtle Beach MultiSound Sound Card Driver for Linux
 * msnd_pinnacle.c / msnd_classic.c
 *
 * -- If MSND_CLASSIC is defined:
 *
 *     -> driver for Turtle Beach Classic/Monterey/Tahiti
 *
 * -- Else
 *
 *     -> driver for Turtle Beach Pinnacle/Fiji
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
 * $Id: msnd_pinnacle.c,v 1.8 2000/12/30 00:33:21 sycamore Exp $
 *
 * 12-3-2000  Modified IO port validation  Steve Sycamore
 *
 *
 * $$$: msnd_pinnacle.c,v 1.75 1999/03/21 16:50:09 andrewtv $$$ $
 *
 ********************************************************************/

#include "adriver.h"
#include <sound/core.h>
#include <sound/initval.h>
#include <sound/asound.h>
#include <sound/pcm.h>
#include <linux/kernel.h>
#include <linux/config.h>
#include <linux/version.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/smp_lock.h>
#include <linux/vmalloc.h>
#include <linux/ioport.h>
#include <linux/firmware.h>
#include <asm/irq.h>
#include <asm/io.h>


#ifdef MSND_CLASSIC
# ifndef __alpha__
#  define SLOWIO
# endif
#endif
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


#define chip_t snd_msndpinnacle_pcm_t

typedef struct snd_msndpinnacle_pcm {
	struct snd_card *		card;
	spinlock_t lock;
	struct timer_list timer;
	unsigned int pcm_size;
	unsigned int pcm_count;
	unsigned int pcm_periods;
	unsigned int pcm_bps;		/* bytes per second */
	unsigned int pcm_jiffie;	/* bytes per one jiffie */
	unsigned int pcm_irq_pos;	/* IRQ position */
	unsigned int pcm_buf_pos;	/* position in buffer */
	struct snd_pcm_substream *substream;
} snd_msndpinnacle_pcm_t;


static int index[SNDRV_CARDS] = SNDRV_DEFAULT_IDX;	/* Index 0-MAX */
static char *id[SNDRV_CARDS] = SNDRV_DEFAULT_STR;	/* ID for this card */

module_param_array(index, int, NULL, S_IRUGO);
MODULE_PARM_DESC(index, "Index value for msnd_pinnacle soundcard.");
module_param_array(id, charp, NULL, S_IRUGO);
MODULE_PARM_DESC(id, "ID string for msnd_pinnacle soundcard.");

#ifndef CONFIG_MSND_WRITE_NDELAY
#  define CONFIG_MSND_WRITE_NDELAY	1
#endif

#define get_play_delay_jiffies(size)	((size) * HZ *			\
					 dev.play_sample_size / 8 /	\
					 dev.play_sample_rate /		\
					 dev.play_channels)

#define get_rec_delay_jiffies(size)	((size) * HZ *			\
					 dev.rec_sample_size / 8 /	\
					 dev.rec_sample_rate /		\
					 dev.rec_channels)

static multisound_dev_t			dev;

snd_msndpinnacle_pcm_t*			Dpcm;


#ifndef HAVE_DSPCODEH
static char				*dspini, *permini;
static int				sizeof_dspini, sizeof_permini;
#endif

static int				snd_msnd_dsp_full_reset(void);
static void				dsp_write_flush(void);


int snd_msnd_send_dsp_cmd_chk(multisound_dev_t *dev, register BYTE cmd)
{
	if (snd_msnd_send_dsp_cmd(dev, cmd) == 0)
		return 0;
	snd_msnd_dsp_full_reset();
	return snd_msnd_send_dsp_cmd(dev, cmd);
}

static void snd_msnd_play_reset_queue( snd_msndpinnacle_pcm_t* dpcm)
{
	int	n;
	LPDAQD	lpDAQ;

	dev.last_playbank = -1;
	dev.playLimit = dpcm->pcm_count * ( dpcm->pcm_periods - 1);
	dev.playPeriods = dpcm->pcm_periods;
	isa_writew(PCTODSP_OFFSET(0 * DAQDS__size), dev.DAPQ + JQS_wHead);
	isa_writew(PCTODSP_OFFSET(0 * DAQDS__size), dev.DAPQ + JQS_wTail);

	dev.play_period_bytes = dpcm->pcm_count;

	for (n = 0, lpDAQ = dev.base + DAPQ_DATA_BUFF; n < 3; ++n, lpDAQ += DAQDS__size) {
		isa_writew(PCTODSP_BASED((DWORD)( dpcm->pcm_count * (n % dpcm->pcm_periods))), lpDAQ + DAQDS_wStart);
		isa_writew(0, lpDAQ + DAQDS_wSize);
		isa_writew(1, lpDAQ + DAQDS_wFormat);
		isa_writew(dev.play_sample_size, lpDAQ + DAQDS_wSampleSize);
		isa_writew(dev.play_channels, lpDAQ + DAQDS_wChannels);
		isa_writew(dev.play_sample_rate, lpDAQ + DAQDS_wSampleRate);
		isa_writew(HIMT_PLAY_DONE * 0x100 + n, lpDAQ + DAQDS_wIntMsg);
		isa_writew(n, lpDAQ + DAQDS_wFlags);
	}
}

static void snd_msnd_capture_reset_queue( snd_msndpinnacle_pcm_t* dpcm)
{
	int		n;
	LPDAQD		lpDAQ;
	//unsigned long	flags;

//	snd_msnd_init_queue(dev.DARQ, DARQ_DATA_BUFF, DARQ_BUFF_SIZE);

	dev.last_recbank = 2;
	dev.captureLimit = dpcm->pcm_count * ( dpcm->pcm_periods - 1);
	dev.capturePeriods = dpcm->pcm_periods;
	isa_writew(PCTODSP_OFFSET(0 * DAQDS__size), dev.DARQ + JQS_wHead);
	isa_writew(PCTODSP_OFFSET(dev.last_recbank * DAQDS__size), dev.DARQ + JQS_wTail);

	/* Critical section: bank 1 access. this is how the OSS driver does it:
	spin_lock_irqsave(&dev.lock, flags);
	outb(HPBLKSEL_1, dev.io + HP_BLKS);
	isa_memset_io(dev.base, 0, DAR_BUFF_SIZE * 3);
	outb(HPBLKSEL_0, dev.io + HP_BLKS);
	spin_unlock_irqrestore(&dev.lock, flags);*/

	dev.capturePeriodBytes = dpcm->pcm_count;
	//snd_printd( "snd_msnd_capture_reset_queue() %i\n", dpcm->pcm_count);

	for (n = 0, lpDAQ = dev.base + DARQ_DATA_BUFF; n < 3/*dpcm->pcm_periods*/; ++n, lpDAQ += DAQDS__size) {
		isa_writew(PCTODSP_BASED((DWORD)(dpcm->pcm_count * (n % dpcm->pcm_periods)) + 0x3000), lpDAQ + DAQDS_wStart);
		isa_writew(dpcm->pcm_count, lpDAQ + DAQDS_wSize);
		isa_writew(1, lpDAQ + DAQDS_wFormat);
		isa_writew(dev.capture_sample_size, lpDAQ + DAQDS_wSampleSize);
		isa_writew(dev.capture_channels, lpDAQ + DAQDS_wChannels);
		isa_writew(dev.capture_sample_rate, lpDAQ + DAQDS_wSampleRate);
		isa_writew(HIMT_RECORD_DONE * 0x100 + n, lpDAQ + DAQDS_wIntMsg);
		isa_writew(n, lpDAQ + DAQDS_wFlags);
	}
}

#ifdef NO0
static void reset_queues(void)
{
	if (dev.mode & FMODE_WRITE) {
		msnd_fifo_make_empty(&dev.DAPF);
		snd_msnd_reset_play_queue();
	}
	if (dev.mode & FMODE_READ) {
		msnd_fifo_make_empty(&dev.DARF);
		snd_msnd_reset_capture_queue();
	}
}
#endif


static void dsp_write_flush(void)
{
	if (!(dev.mode & FMODE_WRITE) || !test_bit(F_WRITING, &dev.flags))
		return;
	set_bit(F_WRITEFLUSH, &dev.flags);
/*	interruptible_sleep_on_timeout(
		&dev.writeflush,
		get_play_delay_jiffies(dev.DAPF.len));*/
	clear_bit(F_WRITEFLUSH, &dev.flags);
	if (!signal_pending(current)) 
		schedule_timeout_interruptible(get_play_delay_jiffies(dev.play_period_bytes));
	clear_bit(F_WRITING, &dev.flags);
}

static void dsp_halt(struct file *file)
{
	if ((file ? file->f_mode : dev.mode) & FMODE_READ) {
		clear_bit(F_READING, &dev.flags);
		snd_msnd_send_dsp_cmd_chk(&dev, HDEX_RECORD_STOP);
		snd_msnd_disable_irq(&dev);
		if (file) {
			printk(KERN_DEBUG LOGNAME ": Stopping read for %p\n", file);
			dev.mode &= ~FMODE_READ;
		}
		clear_bit(F_AUDIO_READ_INUSE, &dev.flags);
	}
	if ((file ? file->f_mode : dev.mode) & FMODE_WRITE) {
		if (test_bit(F_WRITING, &dev.flags)) {
			dsp_write_flush();
			snd_msnd_send_dsp_cmd_chk(&dev, HDEX_PLAY_STOP);
		}
		snd_msnd_disable_irq(&dev);
		if (file) {
			printk(KERN_DEBUG LOGNAME ": Stopping write for %p\n", file);
			dev.mode &= ~FMODE_WRITE;
		}
		clear_bit(F_AUDIO_WRITE_INUSE, &dev.flags);
	}
}

#ifdef NO0
static int dsp_release(struct file *file)
{
	dsp_halt(file);
	return 0;
}
#endif


static void set_default_play_audio_parameters(void)
{
	dev.play_sample_size = DEFSAMPLESIZE;
	dev.play_sample_rate = DEFSAMPLERATE;
	dev.play_channels = DEFCHANNELS;
}

static void set_default_rec_audio_parameters(void)
{
	dev.capture_sample_size = DEFSAMPLESIZE;
	dev.capture_sample_rate = DEFSAMPLERATE;
	dev.capture_channels = DEFCHANNELS;
}

static void set_default_audio_parameters(void)
{
	set_default_play_audio_parameters();
	set_default_rec_audio_parameters();
}


static __inline__ int snd_msnd_DARQ( register int bank)
{
	register int /*size, n,*/ timeout = 3;
	register WORD wTmp;
	//LPDAQD DAQD;

	/* Increment the tail and check for queue wrap */
	wTmp = isa_readw(dev.DARQ + JQS_wTail) + PCTODSP_OFFSET(DAQDS__size);
	//printk( "%iR wTmp = %i", bank, (int)wTmp);
	if (wTmp > isa_readw(dev.DARQ + JQS_wSize))
		wTmp = 0;
	//printk( " %i\n", (int)wTmp);
	while (wTmp == isa_readw(dev.DARQ + JQS_wHead) && timeout--)
		udelay(1);

	if( dev.capturePeriods == 2){
		LPDAQD	lpDAQ = dev.base + DARQ_DATA_BUFF + bank * DAQDS__size + DAQDS_wStart;
		unsigned short offset = isa_readw( lpDAQ);
		isa_writew( offset == PCTODSP_BASED( 0x3000) ? PCTODSP_BASED( 0x3000 + dev.capturePeriodBytes): PCTODSP_BASED( 0x3000), lpDAQ);
	}

	isa_writew(wTmp, dev.DARQ + JQS_wTail);

	/* Get our digital audio queue struct
	DAQD = bank * DAQDS__size + dev.base + DARQ_DATA_BUFF;*/

	/* Get length of data */
	//size = isa_readw(DAQD + DAQDS_wSize);

	/* Read data from the head (unprotected bank 1 access okay
           since this is only called inside an interrupt) */
//	outb(HPBLKSEL_1, dev.io + HP_BLKS);
/*	if ((n = msnd_fifo_write(
		&dev.DARF,
		(char *)(dev.base + bank * DAR_BUFF_SIZE),
		size, 0)) <= 0) {
		outb(HPBLKSEL_0, dev.io + HP_BLKS);
		return n;
	}*/
//	outb(HPBLKSEL_0, dev.io + HP_BLKS);

	return 1;
}

static __inline__ int  snd_msnd_DAPQ( register int start)
{
	register WORD	DAPQ_tail;
	register int	protect = start, nbanks = 0;
	LPDAQD		DAQD;
	static int play_banks_submitted;
	//unsigned long flags;
	//spin_lock_irqsave( &dev.lock, flags); not necessary

	DAPQ_tail = isa_readw(dev.DAPQ + JQS_wTail);
	while (DAPQ_tail != isa_readw(dev.DAPQ + JQS_wHead) || start) {
		register int bank_num = DAPQ_tail / PCTODSP_OFFSET(DAQDS__size);

		if (start){
			start = 0;
			play_banks_submitted = 0;
		}

		/* Get our digital audio queue struct */
		DAQD = bank_num * DAQDS__size + dev.base + DAPQ_DATA_BUFF;

		/* Write size of this bank */
		isa_writew( dev.play_period_bytes, DAQD + DAQDS_wSize);
		if( play_banks_submitted < 3)
			++play_banks_submitted;
		else{
			if( dev.playPeriods == 2){
				unsigned short offset = isa_readw( DAQD + DAQDS_wStart);
				isa_writew( offset == PCTODSP_BASED( 0x0) ? PCTODSP_BASED( 0x0 + dev.play_period_bytes): PCTODSP_BASED( 0x0), DAQD + DAQDS_wStart);
			}
		}
		++nbanks;

		/* Then advance the tail */
		/*
		if( protect)
			snd_printd(  "B %X %lX\n", bank_num, xtime.tv_usec);
		*/

		DAPQ_tail = (++bank_num % 3) * PCTODSP_OFFSET(DAQDS__size);
		isa_writew(DAPQ_tail, dev.DAPQ + JQS_wTail);
		/* Tell the DSP to play the bank */
		snd_msnd_send_dsp_cmd(&dev, HDEX_PLAY_START);
		if( protect)
			if( 2 == bank_num)
	break;
	}
	/*
	if( protect)
		snd_printd(  "%lX\n", xtime.tv_usec);
	*/
	//spin_unlock_irqrestore( &dev.lock, flags); not necessary
	return nbanks;
}


static int InTrigger = 0;               // interrupt diagnostic, comment this out later
static int banksPlayed = 0;
//static int playPosQueriesSinceInt = 0;
//static int play_bytes_remaining_last;
//static int play_bytes_jiffies_last;

static __inline__ void snd_msnd_eval_dsp_msg(register WORD wMessage)
{
	switch (HIBYTE(wMessage)) {
	case HIMT_PLAY_DONE: {
        	snd_msndpinnacle_pcm_t *dpcm = Dpcm;//(void*)data;
		//snd_printd( "snd_msnd_eval_dsp_msg( %i)\n", wMessage);
		#ifdef CONFIG_SND_DEBUG0	
		if(banksPlayed < 3)
			printk(  "%08X: HIMT_PLAY_DONE: %i\n",  (unsigned)jiffies, LOBYTE( wMessage));
		#endif
		#ifdef CONFIG_SND_DEBUG0
		{
			int xx = *(short int*)(__ISA_IO_base + 0x7F40 + dev.base);
			printk(  "%08X: P %X\n", (unsigned)jiffies, xx);
		}
		#endif

		if (dev.last_playbank == LOBYTE(wMessage)){
			snd_printd(  "dev.last_playbank == LOBYTE(wMessage)\n");
			break;
		}
		banksPlayed++;

		if( test_bit( F_WRITING, &dev.flags))
			snd_msnd_DAPQ( 0);

		dev.last_playbank = LOBYTE(wMessage);
                if( ( dev.playDMAPos += dev.play_period_bytes) > dev.playLimit)
			dev.playDMAPos = 0;
		//playPosQueriesSinceInt = 0;
		snd_pcm_period_elapsed( dpcm->substream);
		//play_bytes_remaining_last += dev.play_period_bytes;

		break;
	}
	case HIMT_RECORD_DONE:
		if (dev.last_recbank == LOBYTE(wMessage))
			break;
		dev.last_recbank = LOBYTE(wMessage);
                if( ( dev.captureDMAPos += dev.capturePeriodBytes) > (dev.captureLimit))
			dev.captureDMAPos = 0;

		if( test_bit( F_READING, &dev.flags))
			snd_msnd_DARQ(dev.last_recbank);

		snd_pcm_period_elapsed( dev.captureSubstream);
		break;

	case HIMT_DSP:
		switch (LOBYTE(wMessage)) {
#ifndef MSND_CLASSIC
		case HIDSP_PLAY_UNDER:
#endif
		case HIDSP_INT_PLAY_UNDER:
			printk(KERN_DEBUG LOGNAME ": Play underflow %i\n", banksPlayed);
			if( banksPlayed > 2)
				clear_bit(F_WRITING, &dev.flags);
			break;

		case HIDSP_INT_RECORD_OVER:
			printk(KERN_DEBUG LOGNAME ": Record overflow\n");
			clear_bit(F_READING, &dev.flags);
			break;

		default:
			printk(KERN_DEBUG LOGNAME ": DSP message %d 0x%02x\n",
			LOBYTE(wMessage), LOBYTE(wMessage));
			break;
		}
		break;

        case HIMT_MIDI_IN_UCHAR:
//printk( "msnd midi int");
		if( dev.msndmidi_mpu)
			snd_msndmidi_input_read( dev.msndmidi_mpu);
		break;

	default:
		printk(KERN_DEBUG LOGNAME ": HIMT message %d 0x%02x\n", HIBYTE(wMessage), HIBYTE(wMessage));
		break;
	}
}

//static int InInterrupt = 0;
irqreturn_t snd_msnd_interrupt(int irq, void *dev_id)
{
	/*if( InInterrupt){
		printk(  "INTERRUPT in InInterrupt\n");
		return IRQ_NONE;
	}
	InInterrupt = 1;*/
#ifdef CONFIG_SND_DEBUG
	// interrupt diagnostic, comment this out later
	if(InTrigger)
		printk(  "INTERRUPT in InTrigger %i\n", InTrigger);        // should never happen
#endif
	/* Send ack to DSP */
//	inb(dev.io + HP_RXL);

	/* Evaluate queued DSP messages */
	while (isa_readw(dev.DSPQ + JQS_wTail) != isa_readw(dev.DSPQ + JQS_wHead)) {
		register WORD wTmp;

		snd_msnd_eval_dsp_msg(isa_readw(dev.pwDSPQData + 2*isa_readw(dev.DSPQ + JQS_wHead)));

		if ((wTmp = isa_readw(dev.DSPQ + JQS_wHead) + 1) > isa_readw(dev.DSPQ + JQS_wSize))
			isa_writew(0, dev.DSPQ + JQS_wHead);
		else
			isa_writew(wTmp, dev.DSPQ + JQS_wHead);
	}
	/* Send ack to DSP */
	inb(dev.io + HP_RXL);
	//InInterrupt = 0;
	return IRQ_HANDLED;
}


static int snd_msnd_reset_dsp(void)
{
	int timeout = 100;

	outb(HPDSPRESET_ON, dev.io + HP_DSPR);
	mdelay(1);
#ifndef MSND_CLASSIC
	dev.info = inb(dev.io + HP_INFO);
#endif
	outb(HPDSPRESET_OFF, dev.io + HP_DSPR);
	mdelay(1);
	while (timeout-- > 0) {
		if (inb(dev.io + HP_CVR) == HP_CVR_DEF)
			return 0;
		mdelay(1);
	}
	printk(KERN_ERR LOGNAME ": Cannot reset DSP\n");

	return -EIO;
}

static int __init snd_msnd_probe(void)
{
#ifndef MSND_CLASSIC
	char *xv, *rev = NULL;
	char *pin = "Pinnacle", *fiji = "Fiji";
	char *pinfiji = "Pinnacle/Fiji";
#endif

	if (!request_region(dev.io, dev.numio, "probing")) {
		printk(KERN_ERR LOGNAME ": I/O port conflict\n");
		return -ENODEV;
	}

	if ( snd_msnd_reset_dsp() < 0) {
		release_region(dev.io, dev.numio);
		return -ENODEV;
	}

#ifdef MSND_CLASSIC
	dev.name = "Classic/Tahiti/Monterey";
	printk(KERN_INFO LOGNAME ": %s, "
#else
	switch (dev.info >> 4) {
	case 0xf: xv = "<= 1.15"; break;
	case 0x1: xv = "1.18/1.2"; break;
	case 0x2: xv = "1.3"; break;
	case 0x3: xv = "1.4"; break;
	default: xv = "unknown"; break;
	}

	switch (dev.info & 0x7) {
	case 0x0: rev = "I"; dev.name = pin; break;
	case 0x1: rev = "F"; dev.name = pin; break;
	case 0x2: rev = "G"; dev.name = pin; break;
	case 0x3: rev = "H"; dev.name = pin; break;
	case 0x4: rev = "E"; dev.name = fiji; break;
	case 0x5: rev = "C"; dev.name = fiji; break;
	case 0x6: rev = "D"; dev.name = fiji; break;
	case 0x7:
		rev = "A-B (Fiji) or A-E (Pinnacle)";
		dev.name = pinfiji;
		break;
	}
	printk(KERN_INFO LOGNAME ": %s revision %s, Xilinx version %s, "
#endif /* MSND_CLASSIC */
	       "I/O 0x%x-0x%x, IRQ %d, memory mapped to 0x%lX-0x%lX\n",
	       dev.name,
#ifndef MSND_CLASSIC
	       rev, xv,
#endif
	       dev.io, dev.io + dev.numio - 1,
	       dev.irq,
	       dev.base, dev.base + 0x7fff);

	release_region(dev.io, dev.numio);
	return 0;
}

static int snd_msnd_init_sma(void)
{
	static int initted;
	WORD mastVolLeft, mastVolRight;
	unsigned long flags;

#ifdef MSND_CLASSIC
	outb(dev.memid, dev.io + HP_MEMM);
#endif
	outb(HPBLKSEL_0, dev.io + HP_BLKS);
	if (initted) {
		mastVolLeft = isa_readw(dev.SMA + SMA_wCurrMastVolLeft);
		mastVolRight = isa_readw(dev.SMA + SMA_wCurrMastVolRight);
	} else
		mastVolLeft = mastVolRight = 0;
	isa_memset_io(dev.base, 0, 0x8000);

	/* Critical section: bank 1 access */
	spin_lock_irqsave(&dev.lock, flags);
	outb(HPBLKSEL_1, dev.io + HP_BLKS);
	isa_memset_io(dev.base, 0, 0x8000);
	outb(HPBLKSEL_0, dev.io + HP_BLKS);
	spin_unlock_irqrestore(&dev.lock, flags);

	dev.pwDSPQData = (dev.base + DSPQ_DATA_BUFF);
	dev.pwMODQData = (dev.base + MODQ_DATA_BUFF);
	dev.pwMIDQData = (dev.base + MIDQ_DATA_BUFF);

	/* Motorola 56k shared memory base */
	dev.SMA = dev.base + SMA_STRUCT_START;

	/* Digital audio play queue */
	dev.DAPQ = dev.base + DAPQ_OFFSET;
	snd_msnd_init_queue(dev.DAPQ, DAPQ_DATA_BUFF, DAPQ_BUFF_SIZE);

	/* Digital audio record queue */
	dev.DARQ = dev.base + DARQ_OFFSET;
	snd_msnd_init_queue(dev.DARQ, DARQ_DATA_BUFF, DARQ_BUFF_SIZE);

	/* MIDI out queue */
	dev.MODQ = dev.base + MODQ_OFFSET;
	snd_msnd_init_queue(dev.MODQ, MODQ_DATA_BUFF, MODQ_BUFF_SIZE);

	/* MIDI in queue */
	dev.MIDQ = dev.base + MIDQ_OFFSET;
	snd_msnd_init_queue(dev.MIDQ, MIDQ_DATA_BUFF, MIDQ_BUFF_SIZE);

	/* DSP -> host message queue */
	dev.DSPQ = dev.base + DSPQ_OFFSET;
	snd_msnd_init_queue(dev.DSPQ, DSPQ_DATA_BUFF, DSPQ_BUFF_SIZE);

	/* Setup some DSP values */
#ifndef MSND_CLASSIC
	isa_writew(1, dev.SMA + SMA_wCurrPlayFormat);
	isa_writew(dev.play_sample_size, dev.SMA + SMA_wCurrPlaySampleSize);
	isa_writew(dev.play_channels, dev.SMA + SMA_wCurrPlayChannels);
	isa_writew(dev.play_sample_rate, dev.SMA + SMA_wCurrPlaySampleRate);
#endif
	isa_writew(dev.play_sample_rate, dev.SMA + SMA_wCalFreqAtoD);
	isa_writew(mastVolLeft, dev.SMA + SMA_wCurrMastVolLeft);
	isa_writew(mastVolRight, dev.SMA + SMA_wCurrMastVolRight);
#ifndef MSND_CLASSIC
	isa_writel(0x00010000, dev.SMA + SMA_dwCurrPlayPitch);
	isa_writel(0x00000001, dev.SMA + SMA_dwCurrPlayRate);
#endif
	isa_writew(0x303, dev.SMA + SMA_wCurrInputTagBits);

	initted = 1;

	return 0;
}


////////////////////////////////////////////////////////////////////////////////

static int __init snd_msnd_calibrate_adc( WORD srate)
{
	snd_printd( "snd_msnd_calibrate_adc( %i)\n", srate);
	isa_writew(srate, dev.SMA + SMA_wCalFreqAtoD);
	if (dev.calibrate_signal == 0)
		isa_writew(isa_readw(dev.SMA + SMA_wCurrHostStatusFlags)
		       | 0x0001, dev.SMA + SMA_wCurrHostStatusFlags);
	else
		isa_writew(isa_readw(dev.SMA + SMA_wCurrHostStatusFlags)
		       & ~0x0001, dev.SMA + SMA_wCurrHostStatusFlags);
	if (snd_msnd_send_word(&dev, 0, 0, HDEXAR_CAL_A_TO_D) == 0 &&
	    snd_msnd_send_dsp_cmd_chk(&dev, HDEX_AUX_REQ) == 0) {
		schedule_timeout_interruptible(msecs_to_jiffies(333));
		return 0;
	}
	printk(KERN_WARNING LOGNAME ": ADC calibration failed\n");
	return -EIO;
}

static int upload_dsp_code(void)
{
#ifndef HAVE_DSPCODEH
	const struct firmware *init_fw, *perm_fw;
#endif
	int err;

	outb(HPBLKSEL_0, dev.io + HP_BLKS);
#ifndef HAVE_DSPCODEH
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 5, 0)
	err = request_firmware(&init_fw, INITCODEFILE, dev.card->dev);
#else
	err = request_firmware(&init_fw, INITCODEFILE, "msnd");
#endif
	if (err < 0) {
		printk(KERN_ERR LOGNAME ": Error loading " INITCODEFILE);
		goto cleanup1;
	}
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 5, 0)
	err = request_firmware(&perm_fw, PERMCODEFILE, dev.card->dev);
#else
	err = request_firmware(&perm_fw, PERMCODEFILE, "msnd");
#endif
	if (err < 0) {
		printk(KERN_ERR LOGNAME ": Error loading " PERMCODEFILE);
		goto cleanup;
	}
	INITCODE = init_fw->data;
	INITCODESIZE = init_fw->size;
	PERMCODE = perm_fw->data;
	PERMCODESIZE = perm_fw->size;
#endif
	isa_memcpy_toio(dev.base, PERMCODE, PERMCODESIZE);
	if (snd_msnd_upload_host(&dev, INITCODE, INITCODESIZE) < 0) {
		printk(KERN_WARNING LOGNAME ": Error uploading to DSP\n");
		err = -ENODEV;
		goto cleanup;
	}
#ifdef HAVE_DSPCODEH
	printk(KERN_INFO LOGNAME ": DSP firmware uploaded (resident)\n");
#else
	printk(KERN_INFO LOGNAME ": DSP firmware uploaded\n");
#endif
	err = 0;

cleanup:
#ifndef HAVE_DSPCODEH
	release_firmware(perm_fw);
cleanup1:
	release_firmware(init_fw);
#endif
	return err;
}

#ifdef MSND_CLASSIC
static void reset_proteus(void)
{
	outb(HPPRORESET_ON, dev.io + HP_PROR);
	mdelay(TIME_PRO_RESET);
	outb(HPPRORESET_OFF, dev.io + HP_PROR);
	mdelay(TIME_PRO_RESET_DONE);
}
#endif

static int snd_msnd_initialize(void)
{
	int err, timeout;
	//snd_printd( "snd_msnd_initialize( void)\n");

#ifdef MSND_CLASSIC
	outb(HPWAITSTATE_0, dev.io + HP_WAIT);
	outb(HPBITMODE_16, dev.io + HP_BITM);

	reset_proteus();
#endif
	if ((err = snd_msnd_init_sma()) < 0) {
		printk(KERN_WARNING LOGNAME ": Cannot initialize SMA\n");
		return err;
	}

	if ((err = snd_msnd_reset_dsp()) < 0)
		return err;

	if ((err = upload_dsp_code()) < 0) {
		printk(KERN_WARNING LOGNAME ": Cannot upload DSP code\n");
		return err;
	}

	timeout = 200;

	//snd_printd( "%li\n", dev.base);
	while (isa_readw(dev.base)) {
		mdelay(1);
		if (!timeout--) {
			printk(KERN_DEBUG LOGNAME ": DSP reset timeout\n");
			return -EIO;
		}
	}

	snd_msndmix_setup( &dev);
	return 0;
}

static int snd_msnd_dsp_full_reset(void)
{
	int rv;

	if (test_bit(F_RESETTING, &dev.flags) || ++dev.nresets > 10)
		return 0;

	set_bit(F_RESETTING, &dev.flags);
	printk(KERN_INFO LOGNAME ": DSP reset\n");
	dsp_halt(NULL);			/* Unconditionally halt */
	if ((rv = snd_msnd_initialize()))
		printk(KERN_WARNING LOGNAME ": DSP reset failed\n");
	snd_msndmix_force_recsrc( &dev, 0);
	clear_bit( F_RESETTING, &dev.flags);
	return rv;
}

static int snd_msnd_dev_free( struct snd_device *device){
	snd_printd( "snd_msnd_dev_free()\n");
	return 0;
}

static struct snd_pcm_hardware snd_msnd_playback =
{
	.info =			( SNDRV_PCM_INFO_MMAP | SNDRV_PCM_INFO_INTERLEAVED | SNDRV_PCM_INFO_MMAP_VALID),
	.formats =		( SNDRV_PCM_FMTBIT_U8 | SNDRV_PCM_FMTBIT_S16_LE),
	.rates =			SNDRV_PCM_RATE_KNOT | SNDRV_PCM_RATE_8000_48000,
	.rate_min =		8000,
	.rate_max =		48000,
	.channels_min =		1,
	.channels_max =		2,
	.buffer_bytes_max =	0x3000,
	.period_bytes_min =	0x40,
	.period_bytes_max =	0x1800,
	.periods_min =		2,
	.periods_max =		3,
	.fifo_size =		0,
};

static struct snd_pcm_hardware snd_msnd_capture =
{
	.info =			(SNDRV_PCM_INFO_MMAP | SNDRV_PCM_INFO_INTERLEAVED |
				 SNDRV_PCM_INFO_MMAP_VALID),
	.formats =		( SNDRV_PCM_FMTBIT_U8 | SNDRV_PCM_FMTBIT_S16_LE),
	.rates =		SNDRV_PCM_RATE_KNOT | SNDRV_PCM_RATE_8000_48000,
	.rate_min =		8000,
	.rate_max =		48000,
	.channels_min =		1,
	.channels_max =		2,
	.buffer_bytes_max =	0x3000,
	.period_bytes_min =	0x40,
	.period_bytes_max =	0x1800,
	.periods_min =		2,
	.periods_max =		3,
	.fifo_size =		0,
};


static unsigned int rates[7] = {
	8000, 11025, 16000, 22050,
	32000, 44100, 48000
};

static struct snd_pcm_hw_constraint_list hw_constraints_rates = {
	.count = 7,
	.list = rates,
	.mask = 0,
};



static void snd_msnd_pcm_timer_function(unsigned long data)
{
//this is a dummy caused by lack of knowledge

	snd_printd( "snd_msnd_pcm_timer_function()\n");

/*	
	dpcm->timer.expires = 1 + jiffies;
	add_timer( &dpcm->timer);
	spin_lock_irq( &dpcm->lock);
	dpcm->pcm_irq_pos += dpcm->pcm_jiffie;
	dpcm->pcm_buf_pos += dpcm->pcm_jiffie;
	dpcm->pcm_buf_pos %= dpcm->pcm_size;
	while (dpcm->pcm_irq_pos >= dpcm->pcm_count) {
		dpcm->pcm_irq_pos -= dpcm->pcm_count;
		snd_pcm_period_elapsed( dpcm->substream);
	}
	spin_unlock_irq( &dpcm->lock);	*/
}

static void snd_msnd_runtime_free( struct snd_pcm_runtime *runtime)
{
	snd_msndpinnacle_pcm_t* dpcm = runtime->private_data;
	kfree( dpcm);
}

#ifdef SND_MSNDPI_GENSIN
	// diagnostic code i used to make it work
	static short int sinTable[ 0x480] = {
		#include "sintable.h"
		};

static void snd_msnd_generate_sine(signed short *samples, int channels,
			  snd_pcm_uframes_t offset,
			  int count, double *_phase, int freq, int rate)
{
	double phase = *_phase;
	double max_phase = 1.0 / freq;
	double step = 1.0 / (double)rate;
	double res;

//	int steps[channels];
	int chn, ires, sto = 0;

	/* verify and prepare the contents of areas */
	for (chn = 0; chn < channels; chn++) {
	}
	/* fill the channel areas */
	while (count > 0) {
		ires = sinTable[ sto];
		if( (sto += freq) >= 0x480)
			sto = 0;
		for (chn = 0; chn < channels; chn++) {
			*samples = ires;
			samples ++;
			count -= 2;
		}
		phase += step;
		if (phase >= max_phase)
			phase -= max_phase;
	}
	*_phase = phase;
}
#endif


static int snd_msnd_playback_open( struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *	runtime = substream->runtime;
	snd_msndpinnacle_pcm_t*	dpcm;

	//snd_printd( "snd_msnd_playback_open()\n");

	Dpcm = dpcm = kcalloc(1, sizeof(*dpcm), GFP_KERNEL);
	if( dpcm == NULL)
		return -ENOMEM;

	//snd_printd( " dpcm = %X\n", (unsigned)dpcm);

	set_bit( F_AUDIO_WRITE_INUSE, &dev.flags);
	clear_bit( F_WRITING, &dev.flags);
	snd_msnd_enable_irq( &dev);

	runtime->dma_area =	dev.mappedbase;
	//memset( __ISA_IO_base + dev.base, 0, 3*0x2400);
	runtime->dma_bytes =	0x3000;

	dpcm->timer.data =	(unsigned long) dpcm;
	dpcm->timer.function =	snd_msnd_pcm_timer_function;
	spin_lock_init( &dpcm->lock);
	dpcm->substream =	substream;
	runtime->private_data =	dpcm;
	runtime->private_free =	snd_msnd_runtime_free;
	runtime->hw =		snd_msnd_playback;
	snd_pcm_hw_constraint_list( runtime, 0, SNDRV_PCM_HW_PARAM_RATE, &hw_constraints_rates);
	return 0;
}

static int snd_msnd_playback_close( struct snd_pcm_substream *substream)
{
	//snd_printd( "snd_msnd_playback_close()\n");
	snd_msnd_disable_irq( &dev);
	clear_bit( F_AUDIO_WRITE_INUSE, &dev.flags);
	return 0;
}


static int snd_msnd_playback_hw_params( struct snd_pcm_substream *substream, struct snd_pcm_hw_params *params)
{
	/*	snd_printd( " dpcm = %X M=%X\n", (unsigned)substream->runtime->private_data,
			*((unsigned*)substream->runtime->private_data-1));
	*/
	//snd_msndpinnacle_pcm_t*	chip = snd_pcm_substream_chip( substream);
	int 		i;
	LPDAQD		lpDAQ =	dev.base + DAPQ_DATA_BUFF;

	//this results in 8 with the 2 synts amsynth & bristol so for now i hardcode it:
	//dev.play_sample_size =	snd_pcm_format_width( substream->runtime->format);
	//printk( "dev.play_sample_size %i\n", dev.play_sample_size);
	dev.play_sample_size =	16;
	dev.play_channels =	params_channels( params);
	dev.play_sample_rate =	params_rate( params);

	//snd_printd( "snd_msnd_playback_hw_params()\n");
	//snd_printd( "f: %i; c: %i; r: %i\n", dev.play_sample_size, dev.play_channels, dev.play_sample_rate);

	for( i = 0; i < 3; ++i, lpDAQ += DAQDS__size) {
		isa_writew( dev.play_sample_size,	lpDAQ + DAQDS_wSampleSize);
		isa_writew( dev.play_channels,		lpDAQ + DAQDS_wChannels);
		isa_writew( dev.play_sample_rate,	lpDAQ + DAQDS_wSampleRate);
	}
	// dont do this here:	snd_msnd_calibrate_adc( dev.play_sample_rate);

	/* stuff i used to get it basically making noise:
	{
	double phase = 0.0;
	generate_sine((short int*)(__ISA_IO_base + dev.base), dev.play_channels,
				  0,
				  0x2400, &phase, 4, dev.play_sample_rate) ;
	generate_sine((short int*)(__ISA_IO_base + dev.base + 1*0x2400), dev.play_channels,
				  0,
				  0x2400, &phase, 4, dev.play_sample_rate);
	generate_sine((short int*)(__ISA_IO_base + dev.base + 2*0x2400), dev.play_channels,
				  0,
				  0x2400, &phase, 4, dev.play_sample_rate);
	}*/
	return 0;
}



/*static int snd_msnd_playback_hw_free( struct snd_pcm_substream *substream)
{
	snd_printd( "snd_msnd_playback_hw_free()\n");
	return 0;
} */

static int snd_msnd_playback_prepare( struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	snd_msndpinnacle_pcm_t*	dpcm = runtime->private_data;
	//snd_printd( "snd_msnd_playback_prepare()\n");

	dpcm->pcm_size = snd_pcm_lib_buffer_bytes( substream);
	dpcm->pcm_count = snd_pcm_lib_period_bytes( substream);
	dpcm->pcm_periods = dpcm->pcm_size / dpcm->pcm_count;
	//snd_printd( "buffer_bytes=%i; period_bytes=%i\n", dpcm->pcm_size, dpcm->pcm_count);
	snd_msnd_play_reset_queue( dpcm);
	dev.playDMAPos = 0;
	return 0;
}

//static int played_bytes;
static int snd_msnd_playback_trigger( struct snd_pcm_substream *substream, int cmd)
{
	int	result = 0;
	//spin_lock(&chip->reg_lock);
	if( cmd == SNDRV_PCM_TRIGGER_START) {
		//snd_printd( "snd_msnd_playback_trigger( START)\n");
		InTrigger = 1;           // interrupt diagnostic, comment this out later
		//play_bytes_remaining_last = 0;
		banksPlayed = 0;
		//playPosQueriesSinceInt = 0;
		//played_bytes = -1;
		set_bit( F_WRITING, &dev.flags);
		// this gives looong timeouts, so dont do it here: snd_msnd_calibrate_adc( dev.play_sample_rate);
		snd_msnd_DAPQ( 1);
	} else if( cmd == SNDRV_PCM_TRIGGER_STOP) {
		//snd_printd( "snd_msnd_playback_trigger( STop)\n");
		InTrigger = 2;            // interrupt diagnostic, comment this out later
		clear_bit( F_WRITING, &dev.flags);
		snd_msnd_send_dsp_cmd(&dev, HDEX_PLAY_STOP);
	} else {
		snd_printd( "snd_msnd_playback_trigger( ?????)\n");
		result = -EINVAL;
	}
	InTrigger = 0;             // interrupt diagnostic, comment this out later
	//	spin_unlock(&chip->reg_lock);

	//snd_printd( "snd_msnd_playback_trigger() ENDE\n");
	return result;
}

static snd_pcm_uframes_t snd_msnd_playback_pointer( struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *	runtime =	substream->runtime;
	//snd_printd( "snd_msnd_playback_pointer()\n");
	//snd_msndpinnacle_pcm_t*	dpcm =		runtime->private_data;

	/*	with the following mess i tried to generate a more precise pointer position
	 *	it generated errors with the alsa framework i could not resolve.....
	//int pos = dev.playDMAPos;
	unsigned	remaining = *(short int*)(__ISA_IO_base + 0x7F40 + dev.base),
	 		ljiffies = (unsigned)jiffies;
	int		diff;
	if( -1 == played_bytes){
		if( 0 == remaining)
			played_bytes = 0;
		else
			played_bytes = dev.play_period_bytes - remaining;
	//	played_bytes += 0x60;
	}else{
	int 	ref =		( ljiffies - play_bytes_jiffies_last)
			*	( runtime->frame_bits / 16)
			*	( runtime->rate /HZ);
		diff = 	play_bytes_remaining_last - remaining;
		//printk( "pb %i diff %i ref %i",  played_bytes, diff, ref);
		if( diff < ref){
			diff += dev.play_period_bytes;
			//printk( " newdiff %i ",  diff);
		}
		played_bytes += diff;
		//printk( "\n");
	}

	play_bytes_remaining_last =	remaining;
	play_bytes_jiffies_last =	ljiffies;
	played_bytes %=			snd_msnd_playback.buffer_bytes_max;

	//	if( playPosQueriesSinceInt++)
	//		pos += dev.play_period_bytes - remaining;

	{
		snd_printd( "%08X: remaining %04X\n", (unsigned)jiffies, remaining);
		snd_printd(  "snd_msnd_playback_pointer() %X\n", played_bytes);
	}

	diff = dev.playDMAPos - played_bytes;
	if( diff < 0)
		diff += snd_msndpinnacle_playback.buffer_bytes_max;

	return bytes_to_frames( runtime,
				diff < ( snd_msndpinnacle_playback.buffer_bytes_max / 2)
			?       dev.playDMAPos : played_bytes);
	// end of mess
	*/
	return bytes_to_frames( runtime, dev.playDMAPos);
}


static struct snd_pcm_ops snd_msnd_playback_ops = {
	.open =		snd_msnd_playback_open,
	.close =	snd_msnd_playback_close,
	.ioctl =	snd_pcm_lib_ioctl,
	.hw_params =	snd_msnd_playback_hw_params,
//	.hw_free =	snd_msnd_playback_hw_free,
	.prepare =	snd_msnd_playback_prepare,
	.trigger =	snd_msnd_playback_trigger,
	.pointer =	snd_msnd_playback_pointer,
};

static int snd_msnd_capture_open(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *	runtime = substream->runtime;
	snd_msndpinnacle_pcm_t*	dpcm;
	//snd_printd( "snd_msnd_capture_open()\n");

	dpcm = kcalloc(1, sizeof(*dpcm), GFP_KERNEL);
	if( dpcm == NULL)
		return -ENOMEM;

	dev.captureSubstream = substream;
	set_bit( F_AUDIO_READ_INUSE, &dev.flags);
	snd_msnd_enable_irq( &dev);
	runtime->dma_area = dev.mappedbase + 0x3000;
	runtime->dma_bytes = 0x3000;
	memset( runtime->dma_area, 0, runtime->dma_bytes);
	dpcm->timer.data = (unsigned long) dpcm;
	dpcm->timer.function = snd_msnd_pcm_timer_function;
	spin_lock_init(&dpcm->lock);
	dpcm->substream = substream;
	runtime->private_data = dpcm;
	runtime->private_free = snd_msnd_runtime_free;
	runtime->hw = snd_msnd_capture;
	snd_pcm_hw_constraint_list( runtime, 0, SNDRV_PCM_HW_PARAM_RATE, &hw_constraints_rates);
	return 0;
}

static int snd_msnd_capture_close( struct snd_pcm_substream *substream)
{
	//snd_printd( "snd_msnd_capture_close()\n");
	snd_msnd_disable_irq( &dev);
	clear_bit( F_AUDIO_READ_INUSE, &dev.flags);
	return 0;
}

static int snd_msnd_capture_prepare( struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	snd_msndpinnacle_pcm_t*	dpcm = runtime->private_data;
	//snd_printd( "snd_msnd_capture_prepare()\n");

	dpcm->pcm_size = snd_pcm_lib_buffer_bytes( substream);
	dpcm->pcm_count = snd_pcm_lib_period_bytes( substream);
	dpcm->pcm_periods = dpcm->pcm_size / dpcm->pcm_count;
	//snd_printd( "buffer_bytes=%i; period_bytes=%i\n", dpcm->pcm_size, dpcm->pcm_count);
	snd_msnd_capture_reset_queue( dpcm);
	dev.captureDMAPos = 0;
	return 0;
}

static int snd_msnd_capture_trigger( struct snd_pcm_substream *substream, int cmd)
{
	if( cmd == SNDRV_PCM_TRIGGER_START){
		//snd_card_dummy_pcm_timer_start(substream);
		dev.last_recbank = -1;
		set_bit( F_READING, &dev.flags);
		if (snd_msnd_send_dsp_cmd_chk(&dev, HDEX_RECORD_START) == 0)
			return 0;

		clear_bit( F_READING, &dev.flags);
	}else if( cmd == SNDRV_PCM_TRIGGER_STOP){
		//snd_card_dummy_pcm_timer_stop(substream);
		clear_bit( F_READING, &dev.flags);
		snd_msnd_send_dsp_cmd(&dev, HDEX_RECORD_STOP);
		return 0;
	}
	return -EINVAL;
}


static snd_pcm_uframes_t snd_msnd_capture_pointer( struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
//	snd_card_dummy_pcm_t *dpcm = runtime->private_data;

	return bytes_to_frames( runtime, dev.captureDMAPos);
}


static int snd_msnd_capture_hw_params( struct snd_pcm_substream *substream, struct snd_pcm_hw_params *params)
{
	int		i;
	LPDAQD		lpDAQ =		dev.base + DARQ_DATA_BUFF;

	//this results in 8 with the 2 synts amsynth & bristol so for now i hardcode it:
	//dev.play_sample_size =	snd_pcm_format_width( substream->runtime->format);
	//printk( "dev.play_sample_size %i\n", dev.play_sample_size);
	dev.capture_sample_size =	16;
	dev.capture_channels =		params_channels( params);
	dev.capture_sample_rate =	params_rate( params);

	//snd_printd( "snd_msnd_capture_hw_params()\n");
	//snd_printd( "f: %i; c: %i; r: %i\n", dev.capture_sample_size, dev.capture_channels, dev.capture_sample_rate);

	for( i = 0; i < 3; ++i, lpDAQ += DAQDS__size) {
		isa_writew( dev.capture_sample_size,	lpDAQ + DAQDS_wSampleSize);
		isa_writew( dev.capture_channels,	lpDAQ + DAQDS_wChannels);
		isa_writew( dev.capture_sample_rate,	lpDAQ + DAQDS_wSampleRate);
	}
	return 0;
}


static struct snd_pcm_ops snd_msnd_capture_ops = {
	.open =		snd_msnd_capture_open,
	.close =	snd_msnd_capture_close,
	.ioctl =	snd_pcm_lib_ioctl,
	.hw_params =	snd_msnd_capture_hw_params,
//	.hw_free =	snd_msnd_capture_hw_free,
	.prepare =	snd_msnd_capture_prepare,
	.trigger =	snd_msnd_capture_trigger,
	.pointer =	snd_msnd_capture_pointer,
};


int snd_msnd_pcm( multisound_dev_t *chip, int device, struct snd_pcm **rpcm)
{
	struct snd_pcm *	pcm;
	int		err;

	if( ( err = snd_pcm_new( chip->card, "MSNDPINNACLE", 0/*device*/, 1, 1, &pcm)) < 0)
		return err;

	snd_pcm_set_ops( pcm, SNDRV_PCM_STREAM_PLAYBACK, &snd_msnd_playback_ops);
	snd_pcm_set_ops( pcm, SNDRV_PCM_STREAM_CAPTURE, &snd_msnd_capture_ops);

	pcm->private_free = NULL;//snd_msnd_pcm_free;
	pcm->private_data = chip;
	pcm->info_flags = 0;//SNDRV_PCM_INFO_HALF_DUPLEX;
	strcpy(pcm->name, "Hurricane");


	chip->pcm = pcm;
	if (rpcm)
		*rpcm = pcm;
	return 0;
}

static int __init snd_msnd_attach(void)
{
	int err;
	struct snd_card *card;
	static struct snd_device_ops ops = {
		.dev_free =      snd_msnd_dev_free,
		};

	//snd_printd(  "snd_msnd_attach(void)\n");
	if ((err = request_irq(dev.irq, snd_msnd_interrupt, 0, dev.name, &dev)) < 0) {
		printk(KERN_ERR LOGNAME ": Couldn't grab IRQ %d\n", dev.irq);
		return err;
	}
	request_region(dev.io, dev.numio, dev.name);

	dev.mappedbase = (void __force *)( __ISA_IO_base + dev.base);

	/* geht nich!!!!!
		useless, don't do this at home.
		the multisound has shared isa memory that is by default already maped.

	if( ( dev.mappedbase = request_mem_region( (unsigned)(__ISA_IO_base + dev.base), BUFFSIZE, "MSNDPINNACLE")) == NULL) {
		printk( KERN_ERR LOGNAME ": unable to grab memory region 0x%lx-0x%lx\n",
			   dev.base, dev.base + BUFFSIZE - 1);
		release_region( dev.io, dev.numio);
		free_irq( dev.irq, &dev);
		return -EBUSY;
	}
	*/

	//snd_printd(  "dev.mappedbase = 0x%08X\n", (unsigned)dev.mappedbase);

	if( ( err = snd_msnd_dsp_full_reset()) < 0){
		release_region( dev.io, dev.numio);
		free_irq( dev.irq, &dev);
		return err;
	}

	if ((err = snd_msnd_register(&dev)) < 0) {
		printk(KERN_ERR LOGNAME ": Unable to register MultiSound\n");
		release_region(dev.io, dev.numio);
		free_irq(dev.irq, &dev);
		return err;
	}

	card = snd_card_new(index[ 0/*dev*/], id[0/*dev*/], THIS_MODULE, 0);
	if( card == NULL){
		snd_msnd_unregister(&dev);
		release_region(dev.io, dev.numio);
		free_irq(dev.irq, &dev);
		return -ENOMEM;
	}

	strcpy( card->shortname, "TB Pinnacle");
	strcpy( card->longname, "Turtle Beach Multisound Pinnacle");
	dev.card = card;

	/* Register device */
	if( ( err = snd_device_new(card, SNDRV_DEV_LOWLEVEL, &dev, &ops)) < 0){
		snd_card_free( card);
		snd_msnd_unregister( &dev);
		release_region( dev.io, dev.numio);
		free_irq( dev.irq, &dev);
		return err;
	}

	if( ( err = snd_msnd_pcm( &dev, 0, NULL)) < 0){
		printk(KERN_ERR LOGNAME ": error creating new PCM device\n");
		snd_card_free(card);
		snd_msnd_unregister( &dev);
		release_region( dev.io, dev.numio);
		free_irq( dev.irq, &dev);
		return err;
	}

	if( ( err = snd_msndmix_new( &dev)) < 0){
		printk(KERN_ERR LOGNAME ": error creating new Mixer device\n");
		snd_card_free(card);
		snd_msnd_unregister( &dev);
		release_region( dev.io, dev.numio);
		free_irq( dev.irq, &dev);
		return err;
	}


	if( ( err = snd_msndmidi_new( card, 0, &dev)) < 0){
		printk(KERN_ERR LOGNAME ": error creating new Midi device\n");
		snd_card_free(card);
		snd_msnd_unregister( &dev);
		release_region( dev.io, dev.numio);
		free_irq( dev.irq, &dev);
		return err;
	}

	disable_irq(dev.irq);
	snd_msnd_calibrate_adc( dev.play_sample_rate);
	snd_msndmix_force_recsrc( &dev, 0);

	if( ( err = snd_card_register(card)) < 0) {
		snd_card_free(card);
		return err;
	}

	//snd_printd(  "attach_multisound(void) end\n");
	return 0;
}


static void __exit snd_msnd_unload(void)
{
	//snd_printd(  "snd_msnd_unload(void)\n");
	release_region(dev.io, dev.numio);
	free_irq(dev.irq, &dev);
	snd_msnd_unregister(&dev);
	snd_card_free( dev.card);
}

#ifndef MSND_CLASSIC

/* Pinnacle/Fiji Logical Device Configuration */

static int __init snd_msnd_write_cfg(int cfg, int reg, int value)
{
	outb(reg, cfg);
	outb(value, cfg + 1);
	if (value != inb(cfg + 1)) {
		printk(KERN_ERR LOGNAME ": snd_msnd_write_cfg: I/O error\n");
		return -EIO;
	}
	return 0;
}

static int __init snd_msnd_write_cfg_io0(int cfg, int num, WORD io)
{
	if (snd_msnd_write_cfg(cfg, IREG_LOGDEVICE, num))
		return -EIO;
	if (snd_msnd_write_cfg(cfg, IREG_IO0_BASEHI, HIBYTE(io)))
		return -EIO;
	if (snd_msnd_write_cfg(cfg, IREG_IO0_BASELO, LOBYTE(io)))
		return -EIO;
	return 0;
}

static int __init snd_msnd_write_cfg_io1(int cfg, int num, WORD io)
{
	if (snd_msnd_write_cfg(cfg, IREG_LOGDEVICE, num))
		return -EIO;
	if (snd_msnd_write_cfg(cfg, IREG_IO1_BASEHI, HIBYTE(io)))
		return -EIO;
	if (snd_msnd_write_cfg(cfg, IREG_IO1_BASELO, LOBYTE(io)))
		return -EIO;
	return 0;
}

static int __init snd_msnd_write_cfg_irq(int cfg, int num, WORD irq)
{
	if (snd_msnd_write_cfg(cfg, IREG_LOGDEVICE, num))
		return -EIO;
	if (snd_msnd_write_cfg(cfg, IREG_IRQ_NUMBER, LOBYTE(irq)))
		return -EIO;
	if (snd_msnd_write_cfg(cfg, IREG_IRQ_TYPE, IRQTYPE_EDGE))
		return -EIO;
	return 0;
}

static int __init snd_msnd_write_cfg_mem(int cfg, int num, int mem)
{
	WORD wmem;

	mem >>= 8;
	mem &= 0xfff;
	wmem = (WORD)mem;
	if (snd_msnd_write_cfg(cfg, IREG_LOGDEVICE, num))
		return -EIO;
	if (snd_msnd_write_cfg(cfg, IREG_MEMBASEHI, HIBYTE(wmem)))
		return -EIO;
	if (snd_msnd_write_cfg(cfg, IREG_MEMBASELO, LOBYTE(wmem)))
		return -EIO;
	if (wmem && snd_msnd_write_cfg(cfg, IREG_MEMCONTROL, (MEMTYPE_HIADDR | MEMTYPE_16BIT)))
		return -EIO;
	return 0;
}

static int __init snd_msnd_activate_logical(int cfg, int num)
{
	if (snd_msnd_write_cfg(cfg, IREG_LOGDEVICE, num))
		return -EIO;
	if (snd_msnd_write_cfg(cfg, IREG_ACTIVATE, LD_ACTIVATE))
		return -EIO;
	return 0;
}

static int __init snd_msnd_write_cfg_logical(int cfg, int num, WORD io0, WORD io1, WORD irq, int mem)
{
	if (snd_msnd_write_cfg(cfg, IREG_LOGDEVICE, num))
		return -EIO;
	if (snd_msnd_write_cfg_io0(cfg, num, io0))
		return -EIO;
	if (snd_msnd_write_cfg_io1(cfg, num, io1))
		return -EIO;
	if (snd_msnd_write_cfg_irq(cfg, num, irq))
		return -EIO;
	if (snd_msnd_write_cfg_mem(cfg, num, mem))
		return -EIO;
	if (snd_msnd_activate_logical(cfg, num))
		return -EIO;
	return 0;
}

typedef struct msnd_pinnacle_cfg_device {
		WORD io0, io1, irq;
		int mem;
	} msnd_pinnacle_cfg_t[4];

static int __init snd_msnd_pinnacle_cfg_devices(int cfg, int reset, msnd_pinnacle_cfg_t device)
{
	int i;

	/* Reset devices if told to */
	if (reset) {
		printk(KERN_INFO LOGNAME ": Resetting all devices\n");
		for (i = 0; i < 4; ++i)
			if (snd_msnd_write_cfg_logical(cfg, i, 0, 0, 0, 0))
				return -EIO;
	}

	/* Configure specified devices */
	for (i = 0; i < 4; ++i) {

		switch (i) {
		case 0:		/* DSP */
			if (!(device[i].io0 && device[i].irq && device[i].mem))
				continue;
			break;
		case 1:		/* MPU */
			if (!(device[i].io0 && device[i].irq))
				continue;
			printk(KERN_INFO LOGNAME
			       ": Configuring MPU to I/O 0x%x IRQ %d\n",
			       device[i].io0, device[i].irq);
			break;
		case 2:		/* IDE */
			if (!(device[i].io0 && device[i].io1 && device[i].irq))
				continue;
			printk(KERN_INFO LOGNAME
			       ": Configuring IDE to I/O 0x%x, 0x%x IRQ %d\n",
			       device[i].io0, device[i].io1, device[i].irq);
			break;
		case 3:		/* Joystick */
			if (!(device[i].io0))
				continue;
			printk(KERN_INFO LOGNAME
			       ": Configuring joystick to I/O 0x%x\n",
			       device[i].io0);
			break;
		}

		/* Configure the device */
		if (snd_msnd_write_cfg_logical(cfg, i, device[i].io0, device[i].io1, device[i].irq, device[i].mem))
			return -EIO;
	}

	return 0;
}
#endif

#ifdef MODULE
MODULE_AUTHOR			("Karsten Wiese <annabellesgarden@yahoo.de>");
MODULE_DESCRIPTION		("Turtle Beach " LONGNAME " Linux Driver");
MODULE_LICENSE("GPL");
#ifndef HAVE_DSPCODEH
MODULE_FIRMWARE(INITCODEFILE);
MODULE_FIRMWARE(PERMCODEFILE);
#endif

static int io __initdata =		-1;
static int irq __initdata =		-1;
static int mem __initdata =		-1;
static int write_ndelay __initdata =	-1;

#ifndef MSND_CLASSIC
//static int play_period_bytes __initdata =	DAP_BUFF_SIZE;

/* Pinnacle/Fiji non-PnP Config Port */
static int cfg __initdata =		-1;

/* Extra Peripheral Configuration */
static int reset __initdata = 0;
static int mpu_io __initdata = 0;
static int mpu_irq __initdata = 0;
static int ide_io0 __initdata = 0;
static int ide_io1 __initdata = 0;
static int ide_irq __initdata = 0;
static int joystick_io __initdata = 0;

/* If we have the digital daugherboard... */
static int digital __initdata = 0;
#endif

static int calibrate_signal __initdata = 0;

module_param(io, int, S_IRUGO);
module_param(irq, int, S_IRUGO);
module_param(mem, int, S_IRUGO);
module_param(write_ndelay, int, S_IRUGO);
module_param(calibrate_signal, int, S_IRUGO);
#ifndef MSND_CLASSIC
module_param(digital, int, S_IRUGO);
module_param(cfg, int, S_IRUGO);
module_param(reset, int, S_IRUGO);
module_param(mpu_io, int, S_IRUGO);
module_param(mpu_irq, int, S_IRUGO);
module_param(ide_io0, int, S_IRUGO);
module_param(ide_io1, int, S_IRUGO);
module_param(ide_irq, int, S_IRUGO);
module_param(joystick_io, int, S_IRUGO);

#endif

#else /* not a module */

static int write_ndelay __initdata =	-1;

#ifdef MSND_CLASSIC
static int io __initdata =		CONFIG_MSNDCLAS_IO;
static int irq __initdata =		CONFIG_MSNDCLAS_IRQ;
static int mem __initdata =		CONFIG_MSNDCLAS_MEM;
#else /* Pinnacle/Fiji */

static int io __initdata =		CONFIG_MSNDPIN_IO;
static int irq __initdata =		CONFIG_MSNDPIN_IRQ;
static int mem __initdata =		CONFIG_MSNDPIN_MEM;

/* Pinnacle/Fiji non-PnP Config Port */
#ifdef CONFIG_MSNDPIN_NONPNP
#  ifndef CONFIG_MSNDPIN_CFG
#    define CONFIG_MSNDPIN_CFG		0x250
#  endif
#else
#  ifdef CONFIG_MSNDPIN_CFG
#    undef CONFIG_MSNDPIN_CFG
#  endif
#  define CONFIG_MSNDPIN_CFG		-1
#endif
static int cfg __initdata =		CONFIG_MSNDPIN_CFG;
/* If not a module, we don't need to bother with reset=1 */
static int reset;

/* Extra Peripheral Configuration (Default: Disable) */
#ifndef CONFIG_MSNDPIN_MPU_IO
#  define CONFIG_MSNDPIN_MPU_IO		0
#endif
static int mpu_io __initdata =		CONFIG_MSNDPIN_MPU_IO;

#ifndef CONFIG_MSNDPIN_MPU_IRQ
#  define CONFIG_MSNDPIN_MPU_IRQ	0
#endif
static int mpu_irq __initdata =		CONFIG_MSNDPIN_MPU_IRQ;

#ifndef CONFIG_MSNDPIN_IDE_IO0
#  define CONFIG_MSNDPIN_IDE_IO0	0
#endif
static int ide_io0 __initdata =		CONFIG_MSNDPIN_IDE_IO0;

#ifndef CONFIG_MSNDPIN_IDE_IO1
#  define CONFIG_MSNDPIN_IDE_IO1	0
#endif
static int ide_io1 __initdata =		CONFIG_MSNDPIN_IDE_IO1;

#ifndef CONFIG_MSNDPIN_IDE_IRQ
#  define CONFIG_MSNDPIN_IDE_IRQ	0
#endif
static int ide_irq __initdata =		CONFIG_MSNDPIN_IDE_IRQ;

#ifndef CONFIG_MSNDPIN_JOYSTICK_IO
#  define CONFIG_MSNDPIN_JOYSTICK_IO	0
#endif
static int joystick_io __initdata =	CONFIG_MSNDPIN_JOYSTICK_IO;

/* Have SPDIF (Digital) Daughterboard */
#ifndef CONFIG_MSNDPIN_DIGITAL
#  define CONFIG_MSNDPIN_DIGITAL	0
#endif
static int digital __initdata =		CONFIG_MSNDPIN_DIGITAL;

#endif /* MSND_CLASSIC */

#ifndef CONFIG_MSND_CALSIGNAL
#  define CONFIG_MSND_CALSIGNAL		0
#endif
static int
calibrate_signal __initdata =		CONFIG_MSND_CALSIGNAL;
#endif /* MODULE */


static int __init snd_msnd_init(void)
{
	int err;
#ifndef MSND_CLASSIC
	static msnd_pinnacle_cfg_t pinnacle_devs;
#endif /* MSND_CLASSIC */

	printk(KERN_INFO LOGNAME ": Turtle Beach " LONGNAME " Linux Driver Version "
	       VERSION ", Copyright (C) 2002 Karsten Wiese 1998 Andrew Veliath\n");

	if (io == -1 || irq == -1 || mem == -1)
		printk(KERN_WARNING LOGNAME ": io, irq and mem must be set\n");

#ifdef MSND_CLASSIC
	if (io == -1 ||
	    !(io == 0x290 ||
	      io == 0x260 ||
	      io == 0x250 ||
	      io == 0x240 ||
	      io == 0x230 ||
	      io == 0x220 ||
	      io == 0x210 ||
	      io == 0x3e0)) {
		printk(KERN_ERR LOGNAME ": \"io\" - DSP I/O base must be set to 0x210, 0x220, 0x230, 0x240, 0x250, 0x260, 0x290, or 0x3E0\n");
		return -EINVAL;
	}
#else
	if (io == -1 ||
		io < 0x100 ||
		io > 0x3e0 ||
		(io % 0x10) != 0) {
			printk(KERN_ERR LOGNAME ": \"io\" - DSP I/O base must within the range 0x100 to 0x3E0 and must be evenly divisible by 0x10\n");
			return -EINVAL;
	}
#endif /* MSND_CLASSIC */

	if (irq == -1 ||
	    !(irq == 5 ||
	      irq == 7 ||
	      irq == 9 ||
	      irq == 10 ||
	      irq == 11 ||
	      irq == 12)) {
		printk(KERN_ERR LOGNAME ": \"irq\" - must be set to 5, 7, 9, 10, 11 or 12\n");
		return -EINVAL;
	}

	if (mem == -1 ||
	    !(mem == 0xb0000 ||
	      mem == 0xc8000 ||
	      mem == 0xd0000 ||
	      mem == 0xd8000 ||
	      mem == 0xe0000 ||
	      mem == 0xe8000)) {
		printk(KERN_ERR LOGNAME ": \"mem\" - must be set to "
		       "0xb0000, 0xc8000, 0xd0000, 0xd8000, 0xe0000 or 0xe8000\n");
		return -EINVAL;
	}

#ifdef MSND_CLASSIC
	switch (irq) {
	case 5: dev.irqid = HPIRQ_5; break;
	case 7: dev.irqid = HPIRQ_7; break;
	case 9: dev.irqid = HPIRQ_9; break;
	case 10: dev.irqid = HPIRQ_10; break;
	case 11: dev.irqid = HPIRQ_11; break;
	case 12: dev.irqid = HPIRQ_12; break;
	}

	switch (mem) {
	case 0xb0000: dev.memid = HPMEM_B000; break;
	case 0xc8000: dev.memid = HPMEM_C800; break;
	case 0xd0000: dev.memid = HPMEM_D000; break;
	case 0xd8000: dev.memid = HPMEM_D800; break;
	case 0xe0000: dev.memid = HPMEM_E000; break;
	case 0xe8000: dev.memid = HPMEM_E800; break;
	}
#else
	if (cfg == -1) {
		printk(KERN_INFO LOGNAME ": Assuming PnP mode\n");
	} else if (cfg != 0x250 && cfg != 0x260 && cfg != 0x270) {
		printk(KERN_INFO LOGNAME ": Config port must be 0x250, 0x260 or 0x270 (or unspecified for PnP mode)\n");
		return -EINVAL;
	} else {
		printk(KERN_INFO LOGNAME ": Non-PnP mode: configuring at port 0x%x\n", cfg);

		/* DSP */
		pinnacle_devs[0].io0 = io;
		pinnacle_devs[0].irq = irq;
		pinnacle_devs[0].mem = mem;

		/* The following are Pinnacle specific */

		/* MPU */
		pinnacle_devs[1].io0 = mpu_io;
		pinnacle_devs[1].irq = mpu_irq;

		/* IDE */
		pinnacle_devs[2].io0 = ide_io0;
		pinnacle_devs[2].io1 = ide_io1;
		pinnacle_devs[2].irq = ide_irq;

		/* Joystick */
		pinnacle_devs[3].io0 = joystick_io;

		if (!request_region(cfg, 2, "Pinnacle/Fiji Config")) {
			printk(KERN_ERR LOGNAME ": Config port 0x%x conflict\n", cfg);
			return -EIO;
		}

		if (snd_msnd_pinnacle_cfg_devices(cfg, reset, pinnacle_devs)) {
			printk(KERN_ERR LOGNAME ": Device configuration error\n");
			release_region(cfg, 2);
			return -EIO;
		}
		release_region(cfg, 2);
	}
	
#endif /* MSND_CLASSIC */

	set_default_audio_parameters();
#ifdef MSND_CLASSIC
	dev.type = msndClassic;
#else
	dev.type = msndPinnacle;
#endif
/*	snd_msnd_playback.buffer_bytes_max = 3 * (
		snd_msnd_playback.period_bytes_max =
			snd_msnd_playback.period_bytes_min =
				dev.play_period_bytes = play_period_bytes
	);
	printk(KERN_INFO LOGNAME ": play_period_bytes=0x%X\n", dev.play_period_bytes);
*/
	dev.io = io;
	dev.numio = DSP_NUMIO;
	dev.irq = irq;
	dev.base = mem;

	dev.calibrate_signal = calibrate_signal ? 1 : 0;
	dev.recsrc = 0;
	dev.dspq_data_buff = DSPQ_DATA_BUFF;
	dev.dspq_buff_size = DSPQ_BUFF_SIZE;
	if (write_ndelay == -1)
		write_ndelay = CONFIG_MSND_WRITE_NDELAY;
	if (write_ndelay)
		clear_bit(F_DISABLE_WRITE_NDELAY, &dev.flags);
	else
		set_bit(F_DISABLE_WRITE_NDELAY, &dev.flags);
#ifndef MSND_CLASSIC
	if (digital)
		set_bit(F_HAVEDIGITAL, &dev.flags);
#endif
	spin_lock_init(&dev.lock);
	if ((err = snd_msnd_probe()) < 0) {
		printk(KERN_ERR LOGNAME ": Probe failed\n");
		return err;
	}

	if ((err = snd_msnd_attach()) < 0) {
		printk(KERN_ERR LOGNAME ": Attach failed\n");
		return err;
	}

	return 0;
}

static void __exit snd_msnd_cleanup(void)
{
	snd_msnd_unload();
//	snd_free_pages( dev.mappedbase, 0x8000);
}

module_init(snd_msnd_init);
module_exit(snd_msnd_cleanup);

EXPORT_NO_SYMBOLS;
