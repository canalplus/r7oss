/*
 * PXA2xx i2Sound: support for Intel PXA2xx I2S audio
 *
 * Copyright (c) 2004,2005 Giorgio Padrin <giorgio@mandarinlogiq.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * History:
 *
 * 2005-03	Initial release; Audio engine ((c) 2004) derived from pxa-uda1380.c
 *		-- Giorgio Padrin
 * 2005-06	Power management -- Giorgio Padrin
 * 2005-10	Some finishing -- Giorgio Padrin
 */

#include "adriver.h"

#include <linux/module.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/string.h>
#include <linux/delay.h>
#include <linux/pm.h>
#include <linux/errno.h>
#include <linux/kmod.h>
#include <linux/dma-mapping.h>
#include <linux/platform_device.h>

#include <asm/semaphore.h>
#include <asm/hardware.h>
#include <asm/dma.h>
#include <asm/system.h>

#include <asm/arch/pxa-regs.h>

#include "pxa2xx-i2sound.h"
#include <sound/pcm_params.h>

struct snd_pxa2xx_i2sound_engine snd_pxa2xx_i2sound_engine;

struct snd_pxa2xx_i2sound_stream {
	int id;				/* SNDRV_PCM_STREAM_(PLAYBACK|CAPTURE) */

	struct snd_pxa2xx_i2sound_engine *engine;

	int dma_ch;			/* DMA channel */

	dma_addr_t buffer;		/* buffer (physical address) */
	size_t buffer_size;

	size_t period_size;

	pxa_dma_desc *dma_dring;	/* DMA descriptors ring */
	char *dma_dring_store;		/* storage space for DMA descriptors ring */
	dma_addr_t dma_dring_p;		/* (physical address) */
	dma_addr_t dma_dring_store_p;
	size_t dma_dring_size;

	volatile int dma_running;	/* DMA running? */

  	struct snd_pcm_substream *asubs;	/* attached ALSA substream */

	struct snd_pcm_hardware ahw;		/* ALSA hw description */
};

struct snd_pxa2xx_i2sound_engine {
	unsigned int rate;			/* sample rate */
	unsigned int rate_in_use;
	spinlock_t rate_lock;

	struct snd_pxa2xx_i2sound_stream streams[2];
};

struct snd_pxa2xx_i2sound_i2slink {
	unsigned int usage_count;
	u32 sadiv;
	spinlock_t lock;
	int suspended;
};

struct snd_pxa2xx_i2sound_card {
	const char *name;

	struct snd_pxa2xx_i2sound_engine engine;

	struct snd_pxa2xx_i2sound_i2slink i2slink;

	struct snd_pxa2xx_i2sound_board *board;

	struct snd_card *acard;		/* ALSA card */
	struct snd_pcm *apcm;		/* ALSA pcm */

	struct platform_driver pdriver;
	struct platform_device pdev;
};

static struct snd_pxa2xx_i2sound_card card;

#define flag_stream(s)		(1 << (s)->id)

#define info_clock_from_pxa	((card.board->info & SND_PXA2xx_I2SOUND_INFO_CLOCK_FROM_PXA) ? 1 : 0)
#define info_can_capture	((card.board->info & SND_PXA2xx_I2SOUND_INFO_CAN_CAPTURE) ? 1 : 0)
#define info_half_duplex	((card.board->info & SND_PXA2xx_I2SOUND_INFO_HALF_DUPLEX) ? 1 : 0)

/* PXA 25x or 27x kind detection
   PXA21x and PXA26x belong to the PXA 25x kind  */
static inline int pxa_25x_kind(void)
{
	return ((read_cpuid(CPUID_ID) & 0x3f0) != 0x110);
}


/* begin {{ Audio Engine }} */

#define MAX_DMA_XFER_SIZE	4096
#define MAX_BUFFER_SIZE		65536

static int snd_pxa2xx_i2sound_ngn_init(void)
{
	spin_lock_init(&card.engine.rate_lock);

	/* preallocate buffer */
	return snd_pcm_lib_preallocate_pages_for_all(card.apcm, SNDRV_DMA_TYPE_DEV, NULL,
						     MAX_BUFFER_SIZE,
						     info_can_capture ? MAX_BUFFER_SIZE : 0);
}

static void snd_pxa2xx_i2sound_ngn_free(void)
{
	/* ALSA takes care of deallocate preallocated buffer */
}

/* Constraint: buffer_size integer multiple of period_size
* A better way to assure it?
*/
static unsigned int period_sizes[]
		= { 512, 1024, 2048, 4096, 8192, 16384, 32768 };
static unsigned int buffer_sizes[]
		= { 1024, 2048, 4096, 8192, 16384, 32768, 65536 };
static struct snd_pcm_hw_constraint_list cnstr_period_sizes = {
        	.count = ARRAY_SIZE(period_sizes),
        	.list = period_sizes,
	};
static struct snd_pcm_hw_constraint_list cnstr_buffer_sizes = {
        	.count = ARRAY_SIZE(buffer_sizes),
        	.list = buffer_sizes,
	};

static struct snd_pcm_hw_constraint_list cnstr_rates;

static void snd_pxa2xx_i2sound_ngn_free_buffer(struct snd_pxa2xx_i2sound_stream *s);

static int snd_pxa2xx_i2sound_ngn_init_buffer(struct snd_pxa2xx_i2sound_stream *s,
					      size_t buffer_size, size_t period_size)
{
	int ret;

	unsigned int dma_dpp; /* dma descriptors per period */
	unsigned int periods;
	unsigned int i, j, di;
	dma_addr_t p, d_buf;
	size_t d_size = sizeof(pxa_dma_desc);

	size_t dma_xfer_size = (period_size >= MAX_DMA_XFER_SIZE) ?
			       MAX_DMA_XFER_SIZE : period_size;
	dma_dpp = (period_size - 1) / dma_xfer_size + 1;
	periods = buffer_size / period_size;

	/* free old buffer in case */
	if (s->buffer)
		snd_pxa2xx_i2sound_ngn_free_buffer(s);

	/* allocate buffer */
	ret = snd_pcm_lib_malloc_pages(s->asubs, buffer_size);
	if (ret < 0) return ret;

	s->buffer = s->asubs->runtime->dma_addr;

	/* ---- allocate and setup DMA descriptors ring ---- */

	/* allocate storage space for descriptors ring (alignment issue) */
	s->dma_dring_size = periods * dma_dpp * d_size;
	s->dma_dring_store = dma_alloc_coherent(NULL, s->dma_dring_size + 15,
		 				&s->dma_dring_store_p, GFP_KERNEL);
	if (!s->dma_dring_store) {
		s->dma_dring_size = 0;
		snd_pxa2xx_i2sound_ngn_free_buffer(s);
		return -ENOMEM;
	}

	/* setup the descriptors ring pointer, aligned on a 16 bytes boundary */
	s->dma_dring = ((dma_addr_t) s->dma_dring_store & 0xf) ?
			 (pxa_dma_desc *) (((dma_addr_t) s->dma_dring_store & ~0xf) + 16) :
			 (pxa_dma_desc *) s->dma_dring_store;
	s->dma_dring_p = s->dma_dring_store_p +
			  (dma_addr_t) s->dma_dring - (dma_addr_t) s->dma_dring_store;

	/* fill the descriptors ring */
	di = 0; /* current descriptor index */
	p = s->buffer; /* p: current period (address in buffer) */
	 /* iterate over periods */
	for (i = 0; i < periods; i++, p += period_size) {
		d_buf = p; /* d_buf: address in buffer for current dma descriptor */
		/* iterate over dma descriptors in a period */
		for (j = 0; j < dma_dpp; j++, di++, d_buf += dma_xfer_size) {
			/* link to next descriptor */
			s->dma_dring[di].ddadr = s->dma_dring_p + (di + 1) * d_size;
			if (s->id == SNDRV_PCM_STREAM_PLAYBACK) {
				s->dma_dring[di].dsadr = d_buf;
				s->dma_dring[di].dtadr = __PREG(SADR);
				s->dma_dring[di].dcmd = DCMD_INCSRCADDR | DCMD_FLOWTRG |
							DCMD_BURST32 | DCMD_WIDTH4;
			}
			else {
				s->dma_dring[di].dsadr = __PREG(SADR);
				s->dma_dring[di].dtadr = d_buf;
				s->dma_dring[di].dcmd =	DCMD_INCTRGADDR | DCMD_FLOWSRC |
							DCMD_BURST32 | DCMD_WIDTH4;
			}
			s->dma_dring[di].dcmd |=
			  ((p + period_size - d_buf) >= dma_xfer_size) ?
			  dma_xfer_size : p + period_size - d_buf; /* transfer length */
		}
		s->dma_dring[di - 1].dcmd |= DCMD_ENDIRQEN; /* period irq */
	}
	s->dma_dring[di - 1].ddadr = s->dma_dring_p; /* close the ring */

	return 0;
}

static void snd_pxa2xx_i2sound_ngn_free_buffer(struct snd_pxa2xx_i2sound_stream *s)
{
	if (s->dma_dring_store) {
		dma_free_coherent(NULL, s->dma_dring_size + 15,
				  s->dma_dring_store, s->dma_dring_store_p);
		s->dma_dring_store = NULL;
		s->dma_dring = NULL;
		s->dma_dring_store_p = 0;
		s->dma_dring_p = 0;
		s->dma_dring_size = 0;
	}
	if (s->buffer) {
		snd_pcm_lib_free_pages(s->asubs);
		s->buffer = 0;
	}
}

static int snd_pxa2xx_i2sound_ngn_set_rate(unsigned int rate);

static int snd_pxa2xx_i2sound_ngn_request_rate(struct snd_pxa2xx_i2sound_stream *s,
					       unsigned int rate)
{
	spin_lock(&card.engine.rate_lock);

	card.engine.rate_in_use &= ~flag_stream(s);
	if (rate != card.engine.rate) {
		if (!card.engine.rate_in_use) {
			if (snd_pxa2xx_i2sound_ngn_set_rate(rate) == 0)
				card.engine.rate = rate;
			else goto error;
		}
		else goto error;
	}
	card.engine.rate_in_use |= flag_stream(s);
	spin_unlock(&card.engine.rate_lock);
	return 0;

 error:	spin_unlock(&card.engine.rate_lock);
	return -EINVAL;
}

static void snd_pxa2xx_i2sound_ngn_free_rate(struct snd_pxa2xx_i2sound_stream *s)
{
	spin_lock(&card.engine.rate_lock);
	card.engine.rate_in_use &= ~flag_stream(s);
	spin_unlock(&card.engine.rate_lock);
}

static int snd_pxa2xx_i2sound_i2slink_set_rate_pxa(unsigned int rate);

static int snd_pxa2xx_i2sound_ngn_set_rate(unsigned int rate)
{
	if (info_clock_from_pxa)
		if (snd_pxa2xx_i2sound_i2slink_set_rate_pxa(rate) != 0)
			return -EINVAL;
	if (card.board->set_rate != NULL)
		if (card.board->set_rate(rate) != 0)
			return -EINVAL;
	return 0;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 19)
static void snd_pxa2xx_i2sound_ngn_dma_irq(int ch, void *data)
#else
static void snd_pxa2xx_i2sound_ngn_dma_irq(int ch, void *data, struct pt_regs *regs)
#endif
{
	struct snd_pxa2xx_i2sound_stream *s = (struct snd_pxa2xx_i2sound_stream*) data;
	u32 dcsr;

	dcsr = DCSR(ch);
	DCSR(ch) = dcsr & ~DCSR_STOPIRQEN;

	if (dcsr & DCSR_BUSERR) {
		snd_printk(KERN_ERR "%s: bus error\n", __FUNCTION__);
		return;
	}

	if (dcsr & DCSR_ENDINTR)
		return snd_pcm_period_elapsed(s->asubs);
}

static inline void snd_pxa2xx_i2sound_ngn_map_dma_reqs(struct snd_pxa2xx_i2sound_stream *s)
{
	if (s->id == SNDRV_PCM_STREAM_PLAYBACK)
		DRCMRTXSADR = s->dma_ch | DRCMR_MAPVLD;
	else
		DRCMRRXSADR = s->dma_ch | DRCMR_MAPVLD;
}

static inline void snd_pxa2xx_i2sound_ngn_unmap_dma_reqs(struct snd_pxa2xx_i2sound_stream *s)
{
	if (s->id == SNDRV_PCM_STREAM_PLAYBACK)
		DRCMRTXSADR = 0;
	else
		DRCMRRXSADR = 0;
}

static int snd_pxa2xx_i2sound_ngn_request_dma_channel(struct snd_pxa2xx_i2sound_stream *s)
{
	int ret;
	char *name = (s->id == SNDRV_PCM_STREAM_PLAYBACK) ?
		     "i2s out" :
		     "i2s in";

	ret = pxa_request_dma(name, DMA_PRIO_LOW, snd_pxa2xx_i2sound_ngn_dma_irq, s);
	if (ret < 0) return ret;
	s->dma_ch = ret;
	snd_pxa2xx_i2sound_ngn_map_dma_reqs(s);
	return 0;
}

static void snd_pxa2xx_i2sound_ngn_free_dma_channel(struct snd_pxa2xx_i2sound_stream *s)
{
	snd_pxa2xx_i2sound_ngn_unmap_dma_reqs(s);
	pxa_free_dma(s->dma_ch);
}

static int snd_pxa2xx_i2sound_ngn_attach_asubs(struct snd_pcm_substream *asubs)
{
	struct snd_pxa2xx_i2sound_stream *s = &card.engine.streams[asubs->stream];
	int ret;

	asubs->private_data = (void*) s;
	s->asubs = asubs;

	if ((ret = snd_pxa2xx_i2sound_ngn_request_dma_channel(s)) < 0) return ret;

	/* ALSA hw description */
	asubs->runtime->hw = s->ahw;

	/* ---- constraints on hw params space ---- */

	snd_pcm_hw_constraint_list(asubs->runtime, 0, SNDRV_PCM_HW_PARAM_PERIOD_BYTES,
				   		      &cnstr_period_sizes);
	snd_pcm_hw_constraint_list(asubs->runtime, 0, SNDRV_PCM_HW_PARAM_BUFFER_BYTES,
						      &cnstr_buffer_sizes);
	if (!info_clock_from_pxa && card.board->streams_hw[s->id].rates_table != NULL) {
		/* rates constraint */
		cnstr_rates.count = card.board->streams_hw[s->id].rates_num;
        	cnstr_rates.list = card.board->streams_hw[s->id].rates_table;
		snd_pcm_hw_constraint_list(asubs->runtime, 0, SNDRV_PCM_HW_PARAM_RATE,
				   		      &cnstr_rates);
	}

	/* more board specific constraints? */
	if (card.board->add_hw_constraints)
		card.board->add_hw_constraints(asubs);

	return 0;
}

static void snd_pxa2xx_i2sound_ngn_detach_asubs(struct snd_pcm_substream *asubs)
{
	struct snd_pxa2xx_i2sound_stream *s = &card.engine.streams[asubs->stream];

	snd_pxa2xx_i2sound_ngn_free_dma_channel(s);

	asubs->private_data = NULL;
	s->asubs = NULL;
}

static int snd_pxa2xx_i2sound_ngn_on(struct snd_pxa2xx_i2sound_stream *s, unsigned int rate,
				  size_t buffer_size, size_t period_size)
{
	int ret;

	/* request sample rate */
	ret = snd_pxa2xx_i2sound_ngn_request_rate(s, rate);
	if (ret) return ret;

	/* init buffer */
	ret = snd_pxa2xx_i2sound_ngn_init_buffer(s, buffer_size, period_size);
	if (ret) {
		snd_pxa2xx_i2sound_ngn_free_rate(s);
		return ret;
	}

	s->buffer_size = buffer_size;
	s->period_size = period_size;

	return 0;
}

static void snd_pxa2xx_i2sound_ngn_off(struct snd_pxa2xx_i2sound_stream *s)
{
	snd_pxa2xx_i2sound_ngn_free_rate(s);
	snd_pxa2xx_i2sound_ngn_free_buffer(s);
	s->buffer_size = 0;
	s->period_size = 0;
}

static int snd_pxa2xx_i2sound_ngn_start(struct snd_pxa2xx_i2sound_stream *s)
{
	DDADR(s->dma_ch) = s->dma_dring_p;
	DCSR(s->dma_ch) = DCSR_RUN;
	s->dma_running = 1;
	return 0;
}

static int snd_pxa2xx_i2sound_ngn_stop(struct snd_pxa2xx_i2sound_stream *s)
{
	unsigned int timeout = 100; /* safety timeout */

	DCSR(s->dma_ch) = DCSR_STOPIRQEN;

	while (!(DCSR(s->dma_ch) & DCSR_STOPSTATE) && timeout > 0) {
		 udelay(10);
		 timeout--;
	}

	s->dma_running = 0;
	return 0;
}

static u32 snd_pxa2xx_i2sound_ngn_pointer(struct snd_pxa2xx_i2sound_stream *s)
{
	return ((s->id == SNDRV_PCM_STREAM_PLAYBACK) ?
	      DSADR(s->dma_ch) : DTADR(s->dma_ch))
	      - s->buffer;
}

#ifdef CONFIG_PM
static int snd_pxa2xx_i2sound_ngn_suspend(struct snd_pxa2xx_i2sound_stream *s)
{
	if (s->asubs) snd_pxa2xx_i2sound_ngn_unmap_dma_reqs(s);
	return 0;
}

static int snd_pxa2xx_i2sound_ngn_resume(struct snd_pxa2xx_i2sound_stream *s)
{
	if (s->asubs) snd_pxa2xx_i2sound_ngn_map_dma_reqs(s);
	return 0;
}
#endif

/* end {{ Audio Engine }} */


/* begin {{ I2SLink }} */

static void snd_pxa2xx_i2sound_i2slink_init(void)
{
	spin_lock_init(&card.i2slink.lock);
	card.i2slink.sadiv = 0xd; /* rate 44.1 kHz */;
}

static void snd_pxa2xx_i2sound_i2slink_on(void)
{
	if (info_clock_from_pxa) {
		pxa_gpio_mode(GPIO28_BITCLK_OUT_I2S_MD);
		if (pxa_25x_kind()) pxa_gpio_mode(GPIO32_SYSCLK_I2S_MD);
		else pxa_gpio_mode(GPIO113_I2S_SYSCLK_MD);
	}
	else {
		pxa_gpio_mode(GPIO28_BITCLK_IN_I2S_MD);
	}
	pxa_gpio_mode(GPIO30_SDATA_OUT_I2S_MD);
	if (info_can_capture) pxa_gpio_mode(GPIO29_SDATA_IN_I2S_MD);
	pxa_gpio_mode(GPIO31_SYNC_I2S_MD);

	pxa_set_cken(CKEN8_I2S, 1);
	SACR0 = SACR0_RST;
	if (info_clock_from_pxa) {
		SADIV = card.i2slink.sadiv; /* rate 44.1 kHz */
		SACR0 = SACR0_ENB | SACR0_BCKD | SACR0_RFTH(8) | SACR0_TFTH(8);
	}
	else {
		SACR0 = SACR0_ENB | SACR0_RFTH(8) | SACR0_TFTH(8);
	}
}

static void snd_pxa2xx_i2sound_i2slink_off(void)
{
	SACR0 = SACR0_RST;
	pxa_set_cken(CKEN8_I2S, 0);

	if (info_clock_from_pxa) {
		pxa_gpio_mode(GPIO28_BITCLK | GPIO_OUT | GPIO_DFLT_LOW);
		if (pxa_25x_kind()) pxa_gpio_mode(GPIO32_SYSCLK | GPIO_OUT | GPIO_DFLT_LOW);
		else pxa_gpio_mode(GPIO113_I2S_SYSCLK | GPIO_OUT | GPIO_DFLT_LOW);
	}
	else {
		pxa_gpio_mode(GPIO28_BITCLK | GPIO_IN);
	}
	pxa_gpio_mode(GPIO30_SDATA_OUT | GPIO_OUT | GPIO_DFLT_LOW);
	if (info_can_capture) pxa_gpio_mode(GPIO29_SDATA_IN | GPIO_IN);
	pxa_gpio_mode(GPIO31_SYNC | GPIO_OUT | GPIO_DFLT_LOW);
}

void snd_pxa2xx_i2sound_i2slink_get(void)
{
	spin_lock(&card.i2slink.lock);
	if (card.i2slink.usage_count++ == 0 &&
	    !card.i2slink.suspended)
		snd_pxa2xx_i2sound_i2slink_on();
	spin_unlock(&card.i2slink.lock);
}

void snd_pxa2xx_i2sound_i2slink_free(void)
{
	spin_lock(&card.i2slink.lock);
	if (--card.i2slink.usage_count == 0 &&
	    !card.i2slink.suspended)
		snd_pxa2xx_i2sound_i2slink_off();
	spin_unlock(&card.i2slink.lock);
}

#ifdef CONFIG_PM
void snd_pxa2xx_i2sound_i2slink_suspend(void)
{
	spin_lock(&card.i2slink.lock);
	card.i2slink.suspended = 1;
	if (card.i2slink.usage_count == 0)
		snd_pxa2xx_i2sound_i2slink_off();
	spin_unlock(&card.i2slink.lock);
}

void snd_pxa2xx_i2sound_i2slink_resume(void)
{
	spin_lock(&card.i2slink.lock);
	if (card.i2slink.usage_count > 0)
		snd_pxa2xx_i2sound_i2slink_on();
	card.i2slink.suspended = 0;
	spin_unlock(&card.i2slink.lock);
}
#endif

static int snd_pxa2xx_i2sound_i2slink_set_rate_pxa(unsigned int rate)
{
	switch (rate) {
	case 8000: case 11025: case 16000: case 22050: case 44100: case 48000:
		spin_lock(&card.i2slink.lock);
		SADIV = card.i2slink.sadiv = 576000 / rate;
		spin_unlock(&card.i2slink.lock);
		return 0;
	default: return -EINVAL;
	}
}

/* end {{ I2SLink }} */


/* begin {{ ALSA PCM Ops }} */

static int snd_pxa2xx_i2sound_pcm_hw_params(struct snd_pcm_substream *asubs,
					    struct snd_pcm_hw_params * ahw_params)
{
	return snd_pxa2xx_i2sound_ngn_on((struct snd_pxa2xx_i2sound_stream*) asubs->private_data,
					 params_rate(ahw_params),
					 params_buffer_bytes(ahw_params),
					 params_period_bytes(ahw_params));
}

static int snd_pxa2xx_i2sound_pcm_hw_free(struct snd_pcm_substream *asubs)
{
	snd_pxa2xx_i2sound_ngn_off((struct snd_pxa2xx_i2sound_stream*) asubs->private_data);
	return 0;
}

static int snd_pxa2xx_i2sound_pcm_prepare(struct snd_pcm_substream *asubs)
{
	return 0;
}

static int snd_pxa2xx_i2sound_pcm_trigger(struct snd_pcm_substream *asubs, int cmd)
{
	struct snd_pxa2xx_i2sound_stream* s =
		(struct snd_pxa2xx_i2sound_stream*) asubs->private_data;
	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
		snd_pxa2xx_i2sound_ngn_start(s);
		break;
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
		snd_pxa2xx_i2sound_ngn_stop(s);
		break;
	default:
		return -EINVAL;
		break;
	}
	return 0;
}

static snd_pcm_uframes_t snd_pxa2xx_i2sound_pcm_pointer(struct snd_pcm_substream *asubs)
{
	struct snd_pxa2xx_i2sound_stream* s = 
		(struct snd_pxa2xx_i2sound_stream*) asubs->private_data;
	return bytes_to_frames(asubs->runtime,
			       snd_pxa2xx_i2sound_ngn_pointer(s));
}

static int snd_pxa2xx_i2sound_pcm_open(struct snd_pcm_substream *asubs)
{
	int ret;
	ret = snd_pxa2xx_i2sound_ngn_attach_asubs(asubs);
	if (ret == 0) {
		snd_pxa2xx_i2sound_i2slink_get();
		card.board->open_stream(asubs->stream);
	}
	return ret;
}

static int snd_pxa2xx_i2sound_pcm_close(struct snd_pcm_substream *asubs)
{
	card.board->close_stream(asubs->stream);
	snd_pxa2xx_i2sound_i2slink_free();
	snd_pxa2xx_i2sound_ngn_detach_asubs(asubs);
	return 0;
}

static struct snd_pcm_ops snd_pxa2xx_i2sound_pcm_ops = {
	.open		= snd_pxa2xx_i2sound_pcm_open,
	.close		= snd_pxa2xx_i2sound_pcm_close,
	.ioctl		= snd_pcm_lib_ioctl,
	.hw_params	= snd_pxa2xx_i2sound_pcm_hw_params,
	.hw_free	= snd_pxa2xx_i2sound_pcm_hw_free,
	.prepare	= snd_pxa2xx_i2sound_pcm_prepare,
	.trigger	= snd_pxa2xx_i2sound_pcm_trigger,
	.pointer	= snd_pxa2xx_i2sound_pcm_pointer
};

/* end {{ ALSA PCM Ops }} */


/* begin {{ Platform Device }} */

#ifdef CONFIG_PM
static int snd_pxa2xx_i2sound_card_suspend(struct snd_card *acard, pm_message_t state);
static int snd_pxa2xx_i2sound_card_resume(struct snd_card *acard);

static int snd_pxa2xx_i2sound_pdev_suspend(struct platform_device *pdev, pm_message_t state)
{
	return snd_pxa2xx_i2sound_card_suspend(card.acard, state);
}

static int snd_pxa2xx_i2sound_pdev_resume(struct platform_device *pdev)
{
	return snd_pxa2xx_i2sound_card_resume(card.acard);
}
#endif

static void snd_pxa2xx_i2sound_pdev_device_release(struct device *dev) {}

static int snd_pxa2xx_i2sound_pdev_register(void)
{
	int ret = 0;

	/* platform driver */
	card.pdriver.driver.name = card.name;
#ifdef CONFIG_PM
	card.pdriver.suspend = snd_pxa2xx_i2sound_pdev_suspend;
	card.pdriver.resume = snd_pxa2xx_i2sound_pdev_resume;
#endif
	if ((ret = platform_driver_register(&card.pdriver))) return ret;

	/* platform device */
	card.pdev.name = card.name;
	card.pdev.id = -1;
	card.pdev.dev.release = snd_pxa2xx_i2sound_pdev_device_release;
	if ((ret = platform_device_register(&card.pdev)))
		platform_driver_unregister(&card.pdriver);
	return ret;
}

static void snd_pxa2xx_i2sound_pdev_unregister(void)
{
	platform_device_unregister(&card.pdev);
	platform_driver_unregister(&card.pdriver);
}

/* end {{ Platform Device }} */


/* begin {{ Soundcard }} */

static void snd_pxa2xx_i2sound_card_free_pcm(struct snd_pcm * apcm);

static int snd_pxa2xx_i2sound_card_create_pcm(void)
{
	int ret;
	int s_id;
	struct snd_pxa2xx_i2sound_stream* s;
	struct snd_pxa2xx_i2sound_board_stream_hw* board_s_hw;

	ret = snd_pcm_new(card.acard, card.board->acard_id, 0,
			  1,
			  (info_can_capture) ? 1 : 0,
			  &card.apcm);
	if (ret < 0) return ret;

	ret = snd_pxa2xx_i2sound_ngn_init();
	if (ret < 0)	return ret;

	card.apcm->info_flags = info_half_duplex ?
				       SNDRV_PCM_INFO_HALF_DUPLEX :
				       SNDRV_PCM_INFO_JOINT_DUPLEX;
	strlcpy(card.apcm->name, card.name, sizeof(card.apcm->name));

	for (s_id = 0; s_id < 2; s_id++) {
		if (s_id == SNDRV_PCM_STREAM_CAPTURE && !info_can_capture)
			continue;
		s = &card.engine.streams[s_id];
		s->id = s_id;
		s->ahw.info		= SNDRV_PCM_INFO_INTERLEAVED |
					  SNDRV_PCM_INFO_BLOCK_TRANSFER;
		s->ahw.formats		= SNDRV_PCM_FMTBIT_S16_LE;
		s->ahw.channels_min		= 2;
		s->ahw.channels_max		= 2;
		s->ahw.buffer_bytes_max	= MAX_BUFFER_SIZE;
		s->ahw.period_bytes_min	= 512;
		s->ahw.period_bytes_max	= MAX_BUFFER_SIZE/2;
		s->ahw.periods_min	= 2;
		s->ahw.periods_max	= MAX_BUFFER_SIZE/512;
		if (!info_clock_from_pxa){
			board_s_hw = &card.board->streams_hw[s->id];
			s->ahw.rates 	= board_s_hw->rates;
			s->ahw.rate_min	= board_s_hw->rate_min;
			s->ahw.rate_max	= board_s_hw->rate_max;
		}
		else {
			s->ahw.rates = SNDRV_PCM_RATE_8000  | SNDRV_PCM_RATE_16000 |
				       SNDRV_PCM_RATE_22050 | SNDRV_PCM_RATE_44100 |
				       SNDRV_PCM_RATE_48000;
			s->ahw.rate_min	=  8000;
			s->ahw.rate_max = 48000;
		}
		snd_pcm_set_ops(card.apcm, s->id, &snd_pxa2xx_i2sound_pcm_ops);
	}

	card.apcm->private_free = snd_pxa2xx_i2sound_card_free_pcm;

	return 0;
}

static void snd_pxa2xx_i2sound_card_free_pcm(struct snd_pcm * apcm)
{
	int s_id;
	struct snd_pxa2xx_i2sound_stream* s;

	for (s_id = 0; s_id < 2; s_id++) {
		if (s_id == SNDRV_PCM_STREAM_CAPTURE && !info_can_capture)
			continue;
		s = &card.engine.streams[s_id];
		memset(s, 0, sizeof(struct snd_pxa2xx_i2sound_stream));
	}
	snd_pxa2xx_i2sound_ngn_free();
	card.apcm = NULL;
}

static int snd_pxa2xx_i2sound_board_activate(void)
{
	int ret = 0;

	if (card.board->activate) {
		if (info_clock_from_pxa) snd_pxa2xx_i2sound_i2slink_get();
		ret = card.board->activate();
		if (info_clock_from_pxa) snd_pxa2xx_i2sound_i2slink_free();
	}
	return ret;
}

static void snd_pxa2xx_i2sound_board_deactivate(void)
{
	if (card.board->deactivate) {
		if (info_clock_from_pxa) snd_pxa2xx_i2sound_i2slink_get();
		card.board->deactivate();
		if (info_clock_from_pxa) snd_pxa2xx_i2sound_i2slink_free();
	}
}

#ifdef CONFIG_PM
static int snd_pxa2xx_i2sound_card_suspend(struct snd_card *acard, pm_message_t state)
{
	int s_id;
	
	snd_power_change_state(card.acard, SNDRV_CTL_POWER_D3hot);

	snd_pcm_suspend_all(card.apcm);

	for (s_id = 0; s_id < 2; s_id++) {
		if (s_id == SNDRV_PCM_STREAM_CAPTURE && !info_can_capture)
			continue;
		snd_pxa2xx_i2sound_ngn_suspend(&card.engine.streams[s_id]);
	}

	if (info_clock_from_pxa) snd_pxa2xx_i2sound_i2slink_get();
	card.board->suspend(state);
	if (info_clock_from_pxa) snd_pxa2xx_i2sound_i2slink_free();

	snd_pxa2xx_i2sound_i2slink_suspend();

	return 0;
}

static int snd_pxa2xx_i2sound_card_resume(struct snd_card *acard)
{
	int s_id;

	snd_pxa2xx_i2sound_i2slink_resume();

	if (info_clock_from_pxa) snd_pxa2xx_i2sound_i2slink_get();
	card.board->resume();
	if (info_clock_from_pxa) snd_pxa2xx_i2sound_i2slink_free();

	for (s_id = 0; s_id < 2; s_id++) {
		if (s_id == SNDRV_PCM_STREAM_CAPTURE && !info_can_capture)
			continue;
		snd_pxa2xx_i2sound_ngn_resume(&card.engine.streams[s_id]);
	}

	snd_power_change_state(card.acard, SNDRV_CTL_POWER_D0);
	
	return 0;
}
#endif

int snd_pxa2xx_i2sound_card_activate(struct snd_pxa2xx_i2sound_board *board)
{
	int ret = 0;

	if (board == NULL) return -EINVAL;
	card.board = board;
	card.name = card.board->name;

	snd_pxa2xx_i2sound_i2slink_init();

	if ((ret = snd_pxa2xx_i2sound_board_activate()) < 0) goto failed_activate_board;

	/* new ALSA card */
	if ((card.acard = snd_card_new(-1, card.board->acard_id, THIS_MODULE, 0)) == NULL) {
		card.acard = NULL;
		ret = -ENOMEM;
		goto failed_new_acard;
	}

	strlcpy(card.acard->driver, card.board->acard_id, sizeof(card.acard->driver));
	strlcpy(card.acard->shortname, card.name, sizeof(card.acard->shortname));
	strlcpy(card.acard->longname, card.board->desc, sizeof(card.acard->longname));

	/* mixer */
	if ((ret = board->add_mixer_controls(card.acard)) < 0)
		goto failed_create_acard;

	/* PCM */
	if ((ret = snd_pxa2xx_i2sound_card_create_pcm()) < 0) goto failed_create_acard;

	/* register the card */
        if ((ret = snd_card_register(card.acard)) < 0) goto failed_create_acard;

	if ((ret = snd_pxa2xx_i2sound_pdev_register()) < 0)
		goto failed_register_pdev;

	snd_printk(KERN_INFO "PXA2xx i2Sound: %s activated\n", card.name);

	return 0;

 failed_register_pdev:
 failed_create_acard:
	snd_card_free(card.acard);
	card.acard = NULL;
 failed_new_acard:
	snd_pxa2xx_i2sound_board_deactivate();
 failed_activate_board:
	snd_pxa2xx_i2sound_i2slink_free();
	card.board = NULL;
	return ret;
}

void snd_pxa2xx_i2sound_card_deactivate(void)
{
	snd_pxa2xx_i2sound_pdev_unregister();
	snd_card_free(card.acard);
	card.acard = NULL;
	snd_pxa2xx_i2sound_board_deactivate();
	snd_pxa2xx_i2sound_i2slink_free();
	card.board = NULL;
	snd_printk(KERN_INFO "PXA2xx i2Sound: %s deactivated\n", card.name);
}

/* end {{ Soundcard }} */


/* begin {{ Module }} */

EXPORT_SYMBOL(snd_pxa2xx_i2sound_card_activate);
EXPORT_SYMBOL(snd_pxa2xx_i2sound_card_deactivate);
EXPORT_SYMBOL(snd_pxa2xx_i2sound_i2slink_get);
EXPORT_SYMBOL(snd_pxa2xx_i2sound_i2slink_free);

static int __init snd_pxa_i2Sound_module_on_load(void) { return 0; }
static void __exit snd_pxa_i2Sound_module_on_unload(void) {}
module_init(snd_pxa_i2Sound_module_on_load);
module_exit(snd_pxa_i2Sound_module_on_unload);

MODULE_AUTHOR("Giorgio Padrin");
MODULE_DESCRIPTION("PXA2xx i2Sound: support for Intel PXA2xx I2S audio");
MODULE_LICENSE("GPL");

/* end {{ Module }} */
