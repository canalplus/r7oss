/*
 *   STMicroelectronics Uniperipheral TDM driver
 *
 *   Copyright (c) 2012,2013 STMicroelectronics Limited
 *
 *   Author: John Boddie <john.boddie@st.com>
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

#include <linux/of.h>
#include <linux/init.h>
#include <linux/bpa2.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/platform_data/dma-st-fdma.h>
#include <sound/core.h>
#include "common.h"
#include "reg_aud_uniperif.h"

#ifndef CONFIG_BPA2
#error "BPA2 must be configured for Uniperif TDM driver"
#endif

/*
 * Defines.
 */

#define UNIPERIF_TDM_MIN_HANDSETS	1
#define UNIPERIF_TDM_MAX_HANDSETS	10

#define UNIPERIF_TDM_MIN_PERIODS	4
#define UNIPERIF_TDM_DEF_PERIODS	10

#define UNIPERIF_TDM_MIN_TIMESLOTS	1
#define UNIPERIF_TDM_MAX_TIMESLOTS	128

#define UNIPERIF_TDM_FREQ_8KHZ		8000
#define UNIPERIF_TDM_FREQ_16KHZ		16000
#define UNIPERIF_TDM_FREQ_32KHZ		32000

#define UNIPERIF_TDM_HW_INFO		(SNDRV_PCM_INFO_MMAP | \
					 SNDRV_PCM_INFO_MMAP_VALID | \
					 SNDRV_PCM_INFO_INTERLEAVED | \
					 SNDRV_PCM_INFO_BLOCK_TRANSFER)

#define UNIPERIF_TDM_HW_FORMAT_CNB	(SNDRV_PCM_FMTBIT_S8 | \
					SNDRV_PCM_FMTBIT_U8)
#define UNIPERIF_TDM_HW_FORMAT_LNB	SNDRV_PCM_FMTBIT_S16_LE
#define UNIPERIF_TDM_HW_FORMAT_CWB	SNDRV_PCM_FMTBIT_S16_LE
#define UNIPERIF_TDM_HW_FORMAT_LWB	SNDRV_PCM_FMTBIT_S32_LE

#define UNIPERIF_TDM_BUF_OFF_MAX	((1UL << (14 + 3)) - 1)

#define UNIPERIF_TDM_DMA_MAXBURST	35  /* FIFO is 70 words, so half */

#define UNIPERIF_TDM_FIFO_ERROR_LIMIT	2  /* FIFO errors allowed */

#define SND_STM_TELSS_HANDSET_INFO(fs, s1, s2, v, dup, dat, cn, ln, cw, lw)\
	{ \
		.fsync = fs, \
		.slot1 = s1, \
		.slot2 = s2, \
		.slot2_valid = v, \
		.duplicate = dup, \
		.data16 = dat, \
		.cnb = cn, \
		.lnb = ln, \
		.cwb = cw, \
		.lwb = lw, \
	}

#define UNIPERIF_TDM_DUMP_REGISTER(r) \
		snd_iprintf(buffer, "AUD_UNIPERIF_%-23s (offset 0x%04x) = " \
				"0x%08x\n", __stringify(r), \
				offset__AUD_UNIPERIF_##r(tdm), \
				get__AUD_UNIPERIF_##r(tdm))

/*
 * Types.
 */

struct snd_stm_telss_handset_info {
	unsigned int fsync;
	unsigned int slot1;
	unsigned int slot2;
	unsigned int slot2_valid;
	unsigned int duplicate;
	unsigned int data16;
	unsigned int cnb;
	unsigned int lnb;
	unsigned int cwb;
	unsigned int lwb;
};

struct snd_stm_telss_word_pos_info {
	unsigned int msb;
	unsigned int lsb;
};

struct snd_stm_telss_timeslot_info {
	int word_num;
	struct snd_stm_telss_word_pos_info *word_pos;
};

struct snd_stm_uniperif_tdm_info {
	const char *name;
	int ver;
	int card_device;

	const char *fdma_name;
	struct device_node *dma_np;
	unsigned int fdma_channel;
	unsigned int fdma_initiator;
	enum dma_transfer_direction fdma_direction;
	unsigned int fdma_direct_conn;
	unsigned int fdma_request_line;

	unsigned int rising_edge;		/* Data on rising edge */
	unsigned int clk_rate;			/* Clock rate in Hz */
	unsigned int pclk_rate;			/* PCLK rate in Hz */
	unsigned int fs_rate;			/* fs ref rate in Hz */
	unsigned int timeslots;			/* Time slots per fs ref */
	unsigned int fs01_rate;			/* fs01 rate in Hz */
	unsigned int fs02_rate;			/* fs02 rate in Hz */
	unsigned int fs02_delay_clock;		/* fs02 delay from fs01 */
	unsigned int fs02_delay_timeslot;	/* fs02 delay from fs01 */
	unsigned int msbit_start;		/* Timeslot start position */
	struct snd_stm_telss_timeslot_info *timeslot_info;

	unsigned int frame_size;		/* In 32-bit words */
	unsigned int frame_count;		/* Frames per period */
	unsigned int max_periods;		/* Max audio periods */
	unsigned int handset_count;		/* One per I2S: 2/4/6/8/10 */
	struct snd_stm_telss_handset_info *handset_info;
};

struct uniperif_tdm;

struct telss_handset {
	unsigned int id;			/* Handset ID */
	unsigned int ctl;			/* Control ID for handset */
	struct snd_pcm *pcm;			/* PCM device for handset */
	struct uniperif_tdm *tdm;		/* TDM handset is part of */
	struct snd_pcm_hardware hw;		/* Supported hw info */
	struct snd_pcm_substream *substream;	/* Substream for handset */

	unsigned int hw_xrun;			/* HW is in xrun */
	unsigned int call_xrun;			/* Call is in xrun */
	unsigned int call_valid;		/* Call is valid */
	unsigned int period_max_sz;		/* Handset period max size */
	unsigned int buffer_max_sz;		/* Handset buffer max size */
	unsigned int buffer_act_sz;		/* Handset buffer actual size */
	unsigned int buffer_offset;		/* Handset buffer offset */
	unsigned int dma_period;		/* Handset cached dma period */

	struct snd_stm_telss_handset_info *info;
	struct st_dma_telss_handset_config config;

	snd_stm_magic_field;
};

struct uniperif_tdm {
	struct device *dev;
	struct snd_stm_uniperif_tdm_info *info;
	int ver; /* IP version, used by register access macros */

	struct resource *mem_region;
	void __iomem *base;
	unsigned int irq;
	struct snd_stm_clk *clk;		/* Clock */
	struct snd_stm_clk *pclk;		/* PCLK */
	spinlock_t lock;			/* Lock */

	struct snd_info_entry *proc_entry;

	int open_ref;				/* No handsets opened */
	int start_ref;				/* No handsets started */

	struct bpa2_part *buffer_bpa2;		/* BPA2 partition */
	dma_addr_t buffer_phys;			/* Physical address */
	unsigned char *buffer_area;		/* Uncached address */
	size_t buffer_max_sz;			/* Maximum buffer size */
	size_t buffer_act_sz;			/* Actual buffer size */

	size_t period_max_sz;			/* Maximum period size */
	int period_max_cnt;			/* Maximum number of periods */
	int period_act_cnt;			/* Actual number of periods */

	struct mutex dma_mutex;			/* Request dma channel lock */
	dma_cookie_t dma_cookie;
	unsigned int dma_maxburst;
	struct dma_chan *dma_channel;
	struct st_dma_telss_config dma_config;
	struct dma_async_tx_descriptor *dma_descriptor;
	unsigned int dma_fifo_errors;		/* Count of fifo errors */
	s64 dma_last_callback_time;		/* Time of last dma callback */

	struct telss_handset handsets[UNIPERIF_TDM_MAX_HANDSETS];

	snd_stm_magic_field;
};

/*
 * Uniperipheral TDM globals.
 */

static unsigned int uniperif_tdm_handset_pcm_ctl;

struct uniperif_tdm_mem_fmt_map {
	unsigned int frame_size;
	unsigned int channels;
	unsigned int mem_fmt;
};

/*
 * Uniperipheral TDM function prototypes.
 */

static void uniperif_tdm_hw_reset(struct uniperif_tdm *tdm);
static void uniperif_tdm_hw_enable(struct uniperif_tdm *tdm);
static void uniperif_tdm_hw_disable(struct uniperif_tdm *tdm);
static void uniperif_tdm_hw_start(struct uniperif_tdm *tdm);
static void uniperif_tdm_hw_stop(struct uniperif_tdm *tdm);

/*
 * Uniperipheral TDM implementation.
 */

static void uniperif_tdm_xrun(struct uniperif_tdm *tdm, const char *error)
{
	unsigned long flags;
	int i;

	/* Output error message */
	dev_err(tdm->dev, error);

	spin_lock_irqsave(&tdm->lock, flags);

	/* Put each handset into xrun */
	for (i = 0; i < tdm->info->handset_count; ++i)
		if (tdm->handsets[i].substream)
			if (tdm->handsets[i].call_valid) {
				/* Indicate hardware xrun */
				tdm->handsets[i].hw_xrun = 1;

				/* Stop the substream */
				spin_unlock_irqrestore(&tdm->lock, flags);
				snd_pcm_stop(tdm->handsets[i].substream,
						SNDRV_PCM_STATE_XRUN);
				spin_lock_irqsave(&tdm->lock, flags);
			}

	spin_unlock_irqrestore(&tdm->lock, flags);
}

static irqreturn_t uniperif_tdm_irq_handler(int irq, void *dev_id)
{
	struct uniperif_tdm *tdm = dev_id;
	irqreturn_t result = IRQ_NONE;
	unsigned int status;

	BUG_ON(!tdm);
	BUG_ON(!snd_stm_magic_valid(tdm));

	/* Get the interrupt status and clear immediately */
	preempt_disable();
	status = get__AUD_UNIPERIF_ITS(tdm);
	set__AUD_UNIPERIF_ITS_BCLR(tdm, status);
	preempt_enable();

	if (unlikely(status & mask__AUD_UNIPERIF_ITS__FIFO_ERROR(tdm))) {
		/* Allow some fifo errors as we leave underflow state */
		if (++tdm->dma_fifo_errors > UNIPERIF_TDM_FIFO_ERROR_LIMIT) {
			/* Disable interrupt so doesn't continually fire */
			set__AUD_UNIPERIF_ITM_BCLR__FIFO_ERROR(tdm);

			/* Stop each uniperipheral pcm device that is running */
			uniperif_tdm_xrun(tdm, "FIFO error!");
		}

		result = IRQ_HANDLED;

	} else if (unlikely(status & mask__AUD_UNIPERIF_ITS__DMA_ERROR(tdm))) {
		/* Disable interrupt so doesn't continually fire */
		set__AUD_UNIPERIF_ITM_BCLR__DMA_ERROR(tdm);

		/* Stop each uniperipheral pcm device that is running */
		uniperif_tdm_xrun(tdm, "DMA error!");

		result = IRQ_HANDLED;
	}

	if (result != IRQ_HANDLED)
		dev_err(tdm->dev, "Unhandled IRQ: %08x", status);

	return result;
}

static bool uniperif_tdm_dma_filter_fn(struct dma_chan *chan, void *fn_param)
{
	struct uniperif_tdm *tdm = fn_param;
	struct st_dma_telss_config *config = &tdm->dma_config;

	/* check if FDMA handle has been specified */
	if (tdm->info->dma_np)
		if (tdm->info->dma_np != chan->device->dev->of_node)
			return false;

	/* If fdma channel has been specified, attempt to match channel to it */
	if (tdm->info->fdma_channel)
		if (!st_dma_is_fdma_channel(chan, tdm->info->fdma_channel))
			return false;

	/* Setup basic dma type and buffer address */
	config->type = ST_DMA_TYPE_TELSS;
	config->dma_addr = tdm->mem_region->start +
			offset__AUD_UNIPERIF_FIFO_DATA(tdm);

	/* Setup dma dreq */
	config->dreq_config.request_line = tdm->info->fdma_request_line;
	config->dreq_config.direct_conn = tdm->info->fdma_direct_conn;
	config->dreq_config.initiator = tdm->info->fdma_initiator;
	config->dreq_config.increment = 0;
	config->dreq_config.hold_off = 0;
	config->dreq_config.maxburst = tdm->dma_maxburst;
	config->dreq_config.buswidth = DMA_SLAVE_BUSWIDTH_4_BYTES;
	config->dreq_config.direction = tdm->info->fdma_direction;

	/* Setup telss information */
	config->frame_size = tdm->info->frame_size - 1; /* 1=2, 2=3, etc */
	config->frame_count = tdm->info->frame_count;
	config->handset_count = tdm->info->handset_count;

	/* Save the channel config inside the channel structure */
	chan->private = config;

	dev_dbg(tdm->dev, "Using '%s' channel %d",
			dev_name(chan->device->dev), chan->chan_id);

	return true;
}

static int uniperif_tdm_dma_request(struct uniperif_tdm *tdm)
{
	dma_cap_mask_t mask;
	int result;
	int i;

	BUG_ON(!tdm);
	BUG_ON(!snd_stm_magic_valid(tdm));

	dev_dbg(tdm->dev, "%s(tdm=%p)", __func__, tdm);

	mutex_lock(&tdm->dma_mutex);

	/* Only request the dma channel if not already done so */
	if (tdm->dma_channel) {
		mutex_unlock(&tdm->dma_mutex);
		return 0;
	}

	/* Set the dma channel capabilities we want */
	dma_cap_zero(mask);
	dma_cap_set(DMA_SLAVE, mask);
	dma_cap_set(DMA_CYCLIC, mask);

	/* Request a matching dma channel */
	tdm->dma_channel = dma_request_channel(mask,
			uniperif_tdm_dma_filter_fn, tdm);
	if (!tdm->dma_channel) {
		dev_err(tdm->dev, "Failed to request DMA channel");
		mutex_unlock(&tdm->dma_mutex);
		return -ENODEV;
	}

	/* Set the handset configuration */
	for (i = 0; i < tdm->info->handset_count; ++i) {
		struct telss_handset *handset = &tdm->handsets[i];
		struct st_dma_telss_handset_config *config = &handset->config;

		/* Set the buffer offset and period offset to start from */
		config->buffer_offset = handset->buffer_offset;
		config->period_offset = 0;
		config->period_stride = handset->period_max_sz;

		/* Set the remainder of the handset configuration */
		config->first_slot_id = handset->info->slot1;
		config->second_slot_id = handset->info->slot2;
		config->second_slot_id_valid = handset->info->slot2_valid;
		config->duplicate_enable = handset->info->duplicate;
		config->data_length = handset->info->data16;
		config->call_valid = false;

		/* Set the default handset configuration */
		result = dma_telss_handset_config(tdm->dma_channel, handset->id,
				config);
		if (result) {
			dev_err(tdm->dev, "Failed to configure handset %d", i);
			dma_release_channel(tdm->dma_channel);
			mutex_unlock(&tdm->dma_mutex);
			return result;
		}
	}

	mutex_unlock(&tdm->dma_mutex);
	return 0;
}

static void uniperif_tdm_dma_release(struct uniperif_tdm *tdm)
{
	BUG_ON(!tdm);
	BUG_ON(!snd_stm_magic_valid(tdm));

	dev_dbg(tdm->dev, "%s(tdm=%p)", __func__, tdm);

	mutex_lock(&tdm->dma_mutex);

	if (tdm->dma_channel) {
		/* Terminate any dma */
		dmaengine_terminate_all(tdm->dma_channel);

		/* Release the dma channel */
		dma_release_channel(tdm->dma_channel);
		tdm->dma_channel = NULL;
	}

	mutex_unlock(&tdm->dma_mutex);
}

static int uniperif_tdm_open(struct snd_pcm_substream *substream)
{
	struct telss_handset *handset = snd_pcm_substream_chip(substream);
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct uniperif_tdm *tdm;
	unsigned long flags;
	int result;

	BUG_ON(!handset);
	BUG_ON(!snd_stm_magic_valid(handset));
	BUG_ON(!handset->tdm);
	BUG_ON(!snd_stm_magic_valid(handset->tdm));

	tdm = handset->tdm;

	dev_dbg(tdm->dev, "%s(substream=%p)", __func__, substream);

	/* The substream should not be linked to the pcm device yet */
	BUG_ON(handset->substream);

	/* Set the pcm sync identifier for sound card */
	snd_pcm_set_sync(substream);

	/* Request dma channel and set default handset configuration */
	result = uniperif_tdm_dma_request(tdm);
	if (result) {
		dev_err(tdm->dev, "Failed to request DMA channel");
		return result;
	}

	/* Ensure application uses same period size in frames as driver */
	result = snd_pcm_hw_constraint_minmax(runtime,
			SNDRV_PCM_HW_PARAM_PERIOD_SIZE,
			tdm->info->frame_count, tdm->info->frame_count);
	if (result < 0) {
		dev_err(tdm->dev, "Failed to constrain period size");
		return result;
	}

	/* Set the basic hardware info */
	handset->hw.info = UNIPERIF_TDM_HW_INFO;

	/* C/NB: 8-bit at 8kHz = 8-bit */
	if (handset->info->cnb)
		handset->hw.formats |= UNIPERIF_TDM_HW_FORMAT_CNB;
	/* L/NB: 16-bit at 8kHz = 16-bit */
	if (handset->info->lnb)
		handset->hw.formats |= UNIPERIF_TDM_HW_FORMAT_LNB;
	/* C/WB: 8-bit at 16kHz = 16-bit */
	if (handset->info->cwb)
		handset->hw.formats |= UNIPERIF_TDM_HW_FORMAT_CWB;
	/* L/WB: 16-bit at 16kHz = 32-bit */
	if (handset->info->lwb)
		handset->hw.formats |= UNIPERIF_TDM_HW_FORMAT_LWB;

	/* Handsets only support a single fsync */
	handset->hw.rates = SNDRV_PCM_RATE_CONTINUOUS,
	handset->hw.rate_min = handset->info->fsync;
	handset->hw.rate_max = handset->info->fsync;

	/* Handsets only support a single channel */
	handset->hw.channels_min = 1;
	handset->hw.channels_max = 1;

	spin_lock_irqsave(&tdm->lock, flags);

	if (++tdm->open_ref == 1) {
		/* Allow application to adjust period count within range */
		handset->hw.periods_min	= UNIPERIF_TDM_MIN_PERIODS;
		handset->hw.periods_max	= tdm->period_max_cnt;

		/* Allow application to use maximum size buffer */
		handset->hw.buffer_bytes_max = handset->buffer_max_sz;
	} else {
		/* Ensure application uses same number of periods as driver */
		handset->hw.periods_min	= tdm->period_act_cnt;
		handset->hw.periods_max	= tdm->period_act_cnt;

		/* Allow application to use up to same size buffer as driver */
		handset->hw.buffer_bytes_max = handset->buffer_act_sz;
	}

	spin_unlock_irqrestore(&tdm->lock, flags);

	/* Allow application to adjust period bytes depending on frame size */
	handset->hw.period_bytes_min = 1;
	handset->hw.period_bytes_max = handset->period_max_sz;

	/* Link the hardware parameters to the runtime */
	substream->runtime->hw = handset->hw;

	/* Link the substream to pcm device */
	handset->substream = substream;

	return 0;
}

static int uniperif_tdm_close(struct snd_pcm_substream *substream)
{
	struct telss_handset *handset = snd_pcm_substream_chip(substream);
	struct uniperif_tdm *tdm;
	unsigned long flags;
	int i;

	BUG_ON(!handset);
	BUG_ON(!snd_stm_magic_valid(handset));
	BUG_ON(!handset->tdm);
	BUG_ON(!snd_stm_magic_valid(handset->tdm));

	tdm = handset->tdm;

	dev_dbg(tdm->dev, "%s(substream=%p)", __func__, substream);

	spin_lock_irqsave(&tdm->lock, flags);

	/* Clean up after an xrun */
	if (handset->call_xrun) {
		/* Is this the last started call? */
		if (--tdm->start_ref == 0)
			uniperif_tdm_hw_stop(tdm);

		/* The handset is no longer in xrun */
		handset->call_xrun = 0;
	}

	/* Unlink the substream from the pcm device */
	handset->substream = NULL;

	/*
	 * If only a single handset is open, then it can now change the number
	 * of periods used. So find the open handset and update it with the
	 * min/max periods and the max buffer size.
	 * Note that this will not change the current settings!
	 */

	/* Check if only a single handset open */
	if (--tdm->open_ref == 1) {
		/* Find open handset */
		for (i = 0; i < tdm->info->handset_count; ++i) {
			/* Skip over closed handsets */
			if (tdm->handsets[i].substream == NULL)
				continue;

			/* Set periods range (doesn't change actual used) */
			tdm->handsets[i].hw.periods_min =
					UNIPERIF_TDM_MIN_PERIODS;
			tdm->handsets[i].hw.periods_max	= tdm->period_max_cnt;

			/* Set max buffer size (doesn't change actual used) */
			tdm->handsets[i].hw.buffer_bytes_max =
					tdm->handsets[i].buffer_max_sz;
		}
	}

	spin_unlock_irqrestore(&tdm->lock, flags);

	return 0;
}

static int uniperif_tdm_hw_params(struct snd_pcm_substream *substream,
		struct snd_pcm_hw_params *hw_params)
{
	struct telss_handset *handset = snd_pcm_substream_chip(substream);
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct uniperif_tdm *tdm;
	unsigned long flags;
	int buffer_new_sz;
	int period_new_cnt;
	int i;

	BUG_ON(!handset);
	BUG_ON(!snd_stm_magic_valid(handset));
	BUG_ON(!runtime);
	BUG_ON(!handset->tdm);
	BUG_ON(!snd_stm_magic_valid(handset->tdm));

	dev_dbg(handset->tdm->dev, "%s(substream=%p, hw_params=%p)", __func__,
		   substream, hw_params);

	/* Only process once regardless of how many times called */
	if ((runtime->dma_addr != 0) && (runtime->dma_area != NULL))
		return 0;

	tdm = handset->tdm;

	/* Get the new buffer size in frames */
	buffer_new_sz = params_buffer_size(hw_params);

	/* Calculate how many periods in the buffer */
	period_new_cnt = buffer_new_sz / tdm->info->frame_count;

	/* The new buffer size must be a multiple of period size */
	if (buffer_new_sz % tdm->info->frame_count) {
		dev_err(tdm->dev, "Buffer size not multiple of period size");
		return -EINVAL;
	}

	spin_lock_irqsave(&tdm->lock, flags);

	/* Check for a change in the number of periods */
	if (period_new_cnt != tdm->period_act_cnt) {
		/* Only allow buffer size change when a single handset open */
		if (tdm->open_ref > 1) {
			dev_err(tdm->dev, "More than 1 handset open");
			spin_unlock_irqrestore(&tdm->lock, flags);
			return -EINVAL;
		}

		/* Period count should never exceed maximum number of periods */
		BUG_ON(tdm->period_act_cnt > tdm->period_max_cnt);

		/* Ensure period count is not too small */
		if (tdm->period_act_cnt < UNIPERIF_TDM_MIN_PERIODS) {
			dev_err(tdm->dev, "Buffer size must be > 2 periods");
			spin_unlock_irqrestore(&tdm->lock, flags);
			return -EINVAL;
		}

		/* Set actual period count and calculate actual buffer size */
		tdm->period_act_cnt = period_new_cnt;
		tdm->buffer_act_sz = tdm->period_max_sz * tdm->period_act_cnt;

		/* Update each handset with new actual buffer size */
		for (i = 0; i < tdm->info->handset_count; ++i)
			tdm->handsets[i].buffer_act_sz =
				tdm->buffer_act_sz / tdm->info->handset_count;
	}

	spin_unlock_irqrestore(&tdm->lock, flags);

	/* Buffer is already allocated, just set handset dma pointers */
	runtime->dma_addr = handset->tdm->buffer_phys + handset->buffer_offset;
	runtime->dma_area = handset->tdm->buffer_area + handset->buffer_offset;
	runtime->dma_bytes = handset->buffer_act_sz;

	return 0;
}

static int uniperif_tdm_hw_free(struct snd_pcm_substream *substream)
{
	struct telss_handset *handset = snd_pcm_substream_chip(substream);
	struct snd_pcm_runtime *runtime = substream->runtime;

	BUG_ON(!handset);
	BUG_ON(!snd_stm_magic_valid(handset));
	BUG_ON(!runtime);
	BUG_ON(!handset->tdm);
	BUG_ON(!snd_stm_magic_valid(handset->tdm));

	dev_dbg(handset->tdm->dev, "%s(substream=%p)", __func__, substream);

	/* Only process once regardless of how many times called */
	if ((runtime->dma_addr == 0) && (runtime->dma_area == NULL))
		return 0;

	/* Buffer is never freed, just null handset dma pointers */
	runtime->dma_addr = 0;
	runtime->dma_area = NULL;
	runtime->dma_bytes = 0;

	return 0;
}

static int uniperif_tdm_prepare(struct snd_pcm_substream *substream)
{
	struct telss_handset *handset = snd_pcm_substream_chip(substream);
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct uniperif_tdm *tdm;
	int result;

	BUG_ON(!handset);
	BUG_ON(!snd_stm_magic_valid(handset));
	BUG_ON(!runtime);
	BUG_ON(!handset->tdm);
	BUG_ON(!snd_stm_magic_valid(handset->tdm));

	tdm = handset->tdm;

	dev_dbg(tdm->dev, "%s(substream=%p)", __func__, substream);

	/* Update handset configuration with format specific parameters */
	switch (substream->runtime->format) {
	case SNDRV_PCM_FORMAT_S8:
	case SNDRV_PCM_FORMAT_U8:
		dev_dbg(tdm->dev, "SNDRV_PCM_FORMAT_S8/U8");
		handset->config.second_slot_id_valid = false;
		handset->config.duplicate_enable = false;
		handset->config.data_length = false;
		handset->config.period_stride = tdm->info->frame_count;
		break;

	case SNDRV_PCM_FORMAT_S16_LE:
		dev_dbg(tdm->dev, "SNDRV_PCM_FORMAT_S16_LE");
		handset->config.second_slot_id_valid = false;
		handset->config.duplicate_enable = false;
		handset->config.data_length = true;
		handset->config.period_stride = tdm->info->frame_count * 2;
		break;

	case SNDRV_PCM_FORMAT_S32_LE:
		dev_dbg(tdm->dev, "SNDRV_PCM_FORMAT_S32_LE");
		handset->config.second_slot_id_valid = true;
		handset->config.duplicate_enable = false;
		handset->config.data_length = true;
		handset->config.period_stride = tdm->info->frame_count * 4;
		break;

	default:
		dev_err(tdm->dev, "Unsupported audio format %d",
				substream->runtime->format);
		BUG();
		return -EINVAL;
	}

	/* Set the handset configuration */
	result = dma_telss_handset_config(tdm->dma_channel, handset->id,
			&handset->config);
	if (result) {
		dev_err(tdm->dev, "Failed to configure handset");
		return result;
	}

	return 0;
}

static void uniperif_tdm_dma_callback(void *param)
{
	struct uniperif_tdm *tdm = param;
	unsigned long flags;
	int period;
	s64 now;
	int i;

	BUG_ON(!tdm);
	BUG_ON(!snd_stm_magic_valid(tdm));

	/* Get the current ktime in microseconds */
	now = ktime_to_us(ktime_get());

	/* Cache currently being processed dma period for use by pointer fn */
	period = dma_telss_get_period(tdm->dma_channel);

	/*dev_dbg(tdm->dev, "dma period %d, delta %5lld us", period,
			now - tdm->dma_last_callback_time);*/

	/* Save last callback time */
	tdm->dma_last_callback_time = now;

	/* Update buffer pointers for every handset that is running */
	for (i = 0; i < tdm->info->handset_count; ++i) {
		struct telss_handset *handset = &tdm->handsets[i];

		spin_lock_irqsave(&tdm->lock, flags);

		/* Skip null substreams */
		if (!handset->substream) {
			spin_unlock_irqrestore(&tdm->lock, flags);
			continue;
		}

		/* Skip non-valid calls (i.e. not started or in xrun) */
		if (!handset->call_valid) {
			spin_unlock_irqrestore(&tdm->lock, flags);
			continue;
		}

		/* Calculate adjusted period based on handset period offset */
		handset->dma_period = period;
		handset->dma_period -= handset->config.period_offset;
		handset->dma_period += tdm->period_act_cnt;
		handset->dma_period %= tdm->period_act_cnt;

		spin_unlock_irqrestore(&tdm->lock, flags);

		/* Update the hwptr */
		snd_pcm_period_elapsed(handset->substream);
	}
}

static int uniperif_tdm_start(struct snd_pcm_substream *substream)
{
	struct telss_handset *handset = snd_pcm_substream_chip(substream);
	struct uniperif_tdm *tdm;
	int result;
	int new_period_offset;

	BUG_ON(!handset);
	BUG_ON(!snd_stm_magic_valid(handset));
	BUG_ON(!handset->tdm);
	BUG_ON(!snd_stm_magic_valid(handset->tdm));

	tdm = handset->tdm;

	dev_dbg(tdm->dev, "%s(substream=%p)", __func__, substream);

	/* The handset call must be stopped or in xrun (i.e. not valid) */
	BUG_ON(handset->call_valid);

	/* Clear old data from reader buffer */
	if (tdm->info->fdma_direction == DMA_DEV_TO_MEM)
		memset(substream->runtime->dma_area, 0, handset->buffer_act_sz);

	/* Reset the handset cached period */
	handset->dma_period = 0;

	/* Is this the first call? */
	if (tdm->start_ref == 0) {
		/* The fdma always starts at offset 0 */
		handset->config.period_offset = 0;

		/* Set call valid for reader only (player will set it later) */
		if (tdm->info->fdma_direction == DMA_DEV_TO_MEM)
			handset->config.call_valid = true;

		/* Set the configuration */
		result = dma_telss_handset_config(tdm->dma_channel, handset->id,
				&handset->config);
		if (result) {
			dev_err(tdm->dev, "Failed to set handset config");
			goto error_handset_config;
		}

		/* Prepare the dma descriptor */
		tdm->dma_descriptor = dma_telss_prep_dma_cyclic(
				tdm->dma_channel,
				tdm->buffer_phys,
				tdm->buffer_act_sz,
				tdm->period_max_sz,
				tdm->info->fdma_direction);
		if (!tdm->dma_descriptor) {
			dev_err(tdm->dev, "Failed to prepare dma descriptor");
			result = -ENOMEM;
			goto error_prep_dma_cyclic;
		}

		/* Set the dma callback */
		tdm->dma_descriptor->callback = uniperif_tdm_dma_callback;
		tdm->dma_descriptor->callback_param = tdm;

		/* Start the hardware */
		uniperif_tdm_hw_start(tdm);

		/* Set player first call starting period offset to 1 */
		if (tdm->info->fdma_direction == DMA_MEM_TO_DEV)
			handset->config.period_offset = 1;
	} else {
		/* Second handset should start at next period offset */
		handset->config.period_offset =
				dma_telss_get_period(tdm->dma_channel) + 1;

		/* Ensure period offset is in range */
		handset->config.period_offset %= tdm->period_act_cnt;
	}

	new_period_offset =  handset->config.period_offset;

	/* If call not valid (player 1st call or either 2nd call), do so now */
	if (!handset->config.call_valid) {
		/* Mark the call as valid */
		handset->config.call_valid = true;

		/*
		 * This is to ensure that handset's offset
		 * is correctly programmed
		 */
		do {
			handset->config.period_offset = new_period_offset;

			/* Set the configuration */
			result = dma_telss_handset_config(tdm->dma_channel,
					handset->id, &handset->config);
			if (result) {
				dev_err(tdm->dev, "Failed to set handset config");
				goto error_handset_config;
			}

			/* break if this is first handset */
			 if (tdm->start_ref == 0)
				break;

			/* get period offset after configuring handset */
			new_period_offset =
				dma_telss_get_period(tdm->dma_channel) + 1;

			/* Ensure period offset is in range */
			new_period_offset %= tdm->period_act_cnt;

		} while (new_period_offset != handset->config.period_offset);

	}

	/* Only increment start ref when handset not recovering from xrun */
	if (!handset->call_xrun)
		tdm->start_ref++;

	/* Indicate the handset call is valid and clear xrun flag */
	handset->call_xrun = 0;
	handset->call_valid = 1;

	return 0;

error_prep_dma_cyclic:
	dma_telss_handset_control(tdm->dma_channel, handset->id, 0);
error_handset_config:
	handset->config.call_valid = false;
	return result;
}

static int uniperif_tdm_stop(struct snd_pcm_substream *substream)
{
	struct telss_handset *handset = snd_pcm_substream_chip(substream);
	struct uniperif_tdm *tdm;
	int result;

	BUG_ON(!handset);
	BUG_ON(!snd_stm_magic_valid(handset));
	BUG_ON(!handset->tdm);
	BUG_ON(!snd_stm_magic_valid(handset->tdm));

	tdm = handset->tdm;

	dev_dbg(tdm->dev, "%s(substream=%p)", __func__, substream);

	/* When start fails, stop is called so call valid may not be set */
	if (!handset->call_valid) {
		dev_err(tdm->dev, "Stop called when call not valid!");
		return 0;
	}

	/* Reset handset call valid bit and period offset */
	handset->config.period_offset = 0;
	handset->config.call_valid = false;

	/* Set the configuration */
	result = dma_telss_handset_config(tdm->dma_channel, handset->id,
			&handset->config);
	if (result) {
		dev_err(tdm->dev, "Failed to set handset config");
		return result;
	}

	/* Clear playback buffer to silence remaining data in current period */
	if (tdm->info->fdma_direction == DMA_MEM_TO_DEV)
		memset(substream->runtime->dma_area, 0, handset->buffer_act_sz);

	/*
	 * The runtime state is usually updated _after_ stop is called. This
	 * means we cannot use it to determine an xrun state. So, when called
	 * in running state, we check for a hardware xrun, and if not asserted
	 * we always assume a normal xrun. Any other state will result in a
	 * normal stop.
	 *
	 * For a player, a normal stop will be when in the draining state, but
	 * for a reader, it will always be in the running state. So to fully
	 * stop a reader (including the dma) you must also issue a close.
	 */

	if (substream->runtime->status->state == SNDRV_PCM_STATE_RUNNING) {
		/* Hardware xrun should still cause a stop */
		if (!handset->hw_xrun) {
			handset->call_xrun = 1;
			handset->call_valid = 0;
			return 0;
		}
	}

	/* Is this the last started call? */
	if (--tdm->start_ref == 0)
		uniperif_tdm_hw_stop(tdm);

	/* Indicate the handset call is stopped and clear any xrun */
	handset->hw_xrun = 0;
	handset->call_xrun = 0;
	handset->call_valid = 0;

	return 0;
}

static int uniperif_tdm_trigger(struct snd_pcm_substream *substream,
		int command)
{
	struct telss_handset *handset = snd_pcm_substream_chip(substream);
	unsigned long flags;
	int result;

	BUG_ON(!handset);
	BUG_ON(!snd_stm_magic_valid(handset));
	BUG_ON(!handset->tdm);
	BUG_ON(!snd_stm_magic_valid(handset->tdm));

	spin_lock_irqsave(&handset->tdm->lock, flags);

	switch (command) {
	case SNDRV_PCM_TRIGGER_START:
		result = uniperif_tdm_start(substream);
		break;
	case SNDRV_PCM_TRIGGER_STOP:
		result = uniperif_tdm_stop(substream);
		break;
	default:
		result = -EINVAL;
		BUG();
	}

	spin_unlock_irqrestore(&handset->tdm->lock, flags);

	return result;
}

static snd_pcm_uframes_t uniperif_tdm_pointer(
		struct snd_pcm_substream *substream)
{
	struct telss_handset *handset = snd_pcm_substream_chip(substream);

	BUG_ON(!handset);
	BUG_ON(!snd_stm_magic_valid(handset));
	BUG_ON(!handset->tdm);
	BUG_ON(!snd_stm_magic_valid(handset->tdm));

	/*dev_dbg(tdm->dev, "%s(substream=%p)", __func__, substream);*/

	/* We should only be here when handset call is valid */
	BUG_ON(!handset->call_valid);

	/* Just return cached dma frame offset */
	return handset->dma_period * handset->tdm->info->frame_count;
}

static struct snd_pcm_ops uniperif_tdm_pcm_ops = {
	.open		= uniperif_tdm_open,
	.close		= uniperif_tdm_close,
	.mmap		= snd_stm_buffer_mmap,
	.ioctl		= snd_pcm_lib_ioctl,
	.hw_params	= uniperif_tdm_hw_params,
	.hw_free	= uniperif_tdm_hw_free,
	.prepare	= uniperif_tdm_prepare,
	.trigger	= uniperif_tdm_trigger,
	.pointer	= uniperif_tdm_pointer,
};

/*
 * ALSA low-level device functions.
 */

static int uniperif_tdm_lookup_mem_fmt(struct uniperif_tdm *tdm,
		unsigned int *mem_fmt, unsigned int *channels)
{
	static struct uniperif_tdm_mem_fmt_map mem_fmt_table[] = {
		/* 16/16 memory formats for frame size (prefer over 16/0) */
		{1, 2, value__AUD_UNIPERIF_CONFIG__MEM_FMT_16_16(NULL)},
		{2, 4, value__AUD_UNIPERIF_CONFIG__MEM_FMT_16_16(NULL)},
		{3, 6, value__AUD_UNIPERIF_CONFIG__MEM_FMT_16_16(NULL)},
		{4, 8, value__AUD_UNIPERIF_CONFIG__MEM_FMT_16_16(NULL)},
		/* 16/0 memory formats for frame size */
		{2, 2, value__AUD_UNIPERIF_CONFIG__MEM_FMT_16_0(NULL)},
		{4, 4, value__AUD_UNIPERIF_CONFIG__MEM_FMT_16_0(NULL)},
		{6, 6, value__AUD_UNIPERIF_CONFIG__MEM_FMT_16_0(NULL)},
		{8, 8, value__AUD_UNIPERIF_CONFIG__MEM_FMT_16_0(NULL)},
	};
	static int mem_fmt_table_size = ARRAY_SIZE(mem_fmt_table);
	int i;

	BUG_ON(!tdm);
	BUG_ON(!snd_stm_magic_valid(tdm));
	BUG_ON(!mem_fmt);
	BUG_ON(!channels);

	/* Search table based on frame size */
	for (i = 0; i < mem_fmt_table_size; ++i) {
		/* Skip table entry if frame size doesn't match */
		if (tdm->info->frame_size != mem_fmt_table[i].frame_size)
			continue;

		/* Frame size matches, return memory format and channel count */
		*mem_fmt = mem_fmt_table[i].mem_fmt;
		*channels = mem_fmt_table[i].channels / 2;
		return 0;
	}

	return -EINVAL;
}

static int uniperif_tdm_set_freqs(struct uniperif_tdm *tdm)
{
	dev_dbg(tdm->dev, "%s(tdm=%p)", __func__, tdm);

	BUG_ON(!tdm);
	BUG_ON(!snd_stm_magic_valid(tdm));

	/* We only set the clock frequencies for players */
	if (tdm->info->fdma_direction == DMA_DEV_TO_MEM)
		return 0;

	/* Set the fs ref */
	switch (tdm->info->fs_rate) {
	case UNIPERIF_TDM_FREQ_8KHZ:
		set__AUD_UNIPERIF_TDM_FS_REF_FREQ__8KHZ(tdm);
		break;

	case UNIPERIF_TDM_FREQ_16KHZ:
		set__AUD_UNIPERIF_TDM_FS_REF_FREQ__16KHZ(tdm);
		break;

	case UNIPERIF_TDM_FREQ_32KHZ:
		set__AUD_UNIPERIF_TDM_FS_REF_FREQ__32KHZ(tdm);
		break;

	default:
		dev_err(tdm->dev, "Invalid fs_ref (%d)", tdm->info->fs_rate);
		return -EINVAL;
	}

	/* Set the number of timeslots required */
	BUG_ON(tdm->info->timeslots < UNIPERIF_TDM_MIN_TIMESLOTS);
	BUG_ON(tdm->info->timeslots > UNIPERIF_TDM_MAX_TIMESLOTS);

	set__AUD_UNIPERIF_TDM_FS_REF_DIV__NUM_TIMESLOT(tdm,
			tdm->info->timeslots);

	/* Ensure fs01 is less than or equal to fs ref */
	BUG_ON(tdm->info->fs01_rate > tdm->info->fs_rate);

	/* Set the fs01 frequency parameters */
	switch (tdm->info->fs01_rate) {
	case UNIPERIF_TDM_FREQ_8KHZ:
		set__AUD_UNIPERIF_TDM_FS01_FREQ__8KHZ(tdm);
		set__AUD_UNIPERIF_TDM_FS01_WIDTH__1BIT(tdm);
		break;

	case UNIPERIF_TDM_FREQ_16KHZ:
		set__AUD_UNIPERIF_TDM_FS_REF_FREQ__16KHZ(tdm);
		set__AUD_UNIPERIF_TDM_FS01_FREQ__16KHZ(tdm);
		set__AUD_UNIPERIF_TDM_FS01_WIDTH__1BIT(tdm);
		break;

	default:
		dev_err(tdm->dev, "Invalid fs01 (%d)", tdm->info->fs01_rate);
		return -EINVAL;
	}

	/* Ensure fs02 is less than or equal to fs ref */
	BUG_ON(tdm->info->fs02_rate > tdm->info->fs_rate);

	/* Set the fs02 frequency parameters */
	switch (tdm->info->fs02_rate) {
	case UNIPERIF_TDM_FREQ_8KHZ:
		set__AUD_UNIPERIF_TDM_FS02_FREQ__8KHZ(tdm);
		set__AUD_UNIPERIF_TDM_FS02_WIDTH__1BIT(tdm);
		break;

	case UNIPERIF_TDM_FREQ_16KHZ:
		set__AUD_UNIPERIF_TDM_FS02_FREQ__16KHZ(tdm);
		set__AUD_UNIPERIF_TDM_FS02_WIDTH__1BIT(tdm);
		break;

	default:
		dev_err(tdm->dev, "Invalid fs02 (%d)", tdm->info->fs02_rate);
		return -EINVAL;
	}

	/* Set the frame sync 2 timeslot delay */
	set__AUD_UNIPERIF_TDM_FS02_TIMESLOT_DELAY__PCM_CLOCK(tdm,
			tdm->info->fs02_delay_clock);
	set__AUD_UNIPERIF_TDM_FS02_TIMESLOT_DELAY__TIMESLOT(tdm,
			tdm->info->fs02_delay_timeslot);

	return 0;
}

static void uniperif_tdm_configure_timeslots(struct uniperif_tdm *tdm)
{
	dev_dbg(tdm->dev, "%s(tdm=%p)", __func__, tdm);

	BUG_ON(!tdm);
	BUG_ON(!snd_stm_magic_valid(tdm));

	/* Set the timeslot start position */
	set__AUD_UNIPERIF_TDM_DATA_MSBIT_START__DELAY(tdm,
			tdm->info->msbit_start);
}

static int uniperif_tdm_configure(struct uniperif_tdm *tdm)
{
	unsigned int mem_fmt;
	unsigned int channels;
	int result;

	dev_dbg(tdm->dev, "%s(tdm=%p)", __func__, tdm);

	BUG_ON(!tdm);
	BUG_ON(!snd_stm_magic_valid(tdm));

	/* Look up what memory format to use based on frame size & channels */
	result = uniperif_tdm_lookup_mem_fmt(tdm, &mem_fmt, &channels);
	if (result) {
		dev_err(tdm->dev, "Failed to lookup mem fmt");
		return result;
	}

	/* Set the input memory format */
	set__AUD_UNIPERIF_CONFIG__MEM_FMT(tdm, mem_fmt);
	/* Set the fdma trigger limit */
	set__AUD_UNIPERIF_CONFIG__FDMA_TRIGGER_LIMIT(tdm, tdm->dma_maxburst);

	/*
	 * When TDM_ENABLE is set to 1, the i2s format should be automatically
	 * configured. Unfortunately this is not the case! We must manually
	 * set the MSB to get input/output and in order for loopback at the
	 * TELSS glue to function correctly we must set the number of bits per
	 * subframe.
	 */

	/* Set the number of bits per subframe  */
	set__AUD_UNIPERIF_I2S_FMT__NBIT_16(tdm);
	/* Set the output data size for 32-bit i2s */
	set__AUD_UNIPERIF_I2S_FMT__DATA_SIZE_32(tdm);
	/* Set the output data lr clock polarity to high for i2s */
	set__AUD_UNIPERIF_I2S_FMT__LR_POL_HIG(tdm);
	/* Set the output data alignment to left for i2s */
	set__AUD_UNIPERIF_I2S_FMT__ALIGN_LEFT(tdm);
	/* Set the output data format to MSB for i2s */
	set__AUD_UNIPERIF_I2S_FMT__ORDER_MSB(tdm);
	/* Set the output data channel count */
	set__AUD_UNIPERIF_I2S_FMT__NUM_CH(tdm, channels);
	/* Set the output data padding to Sony mode */
	set__AUD_UNIPERIF_I2S_FMT__PADDING_SONY_MODE(tdm);

	/* Set the clock edge (usually Tx = rising edge, Rx = falling edge) */
	if (tdm->info->rising_edge)
		set__AUD_UNIPERIF_I2S_FMT__SCLK_EDGE_RISING(tdm);
	else
		set__AUD_UNIPERIF_I2S_FMT__SCLK_EDGE_FALLING(tdm);

	/* We don't use the memory block read interrupt, so set this to zero */
	set__AUD_UNIPERIF_I2S_FMT__NO_OF_SAMPLES_TO_READ(tdm, 0);

	/* Set the clock frequencies */
	result = uniperif_tdm_set_freqs(tdm);
	if (result) {
		dev_err(tdm->dev, "Failed to set freqs");
		return result;
	}

	/* Configure the time slots */
	uniperif_tdm_configure_timeslots(tdm);

	return 0;
}

static int uniperif_tdm_clk_get(struct uniperif_tdm *tdm)
{
	struct snd_card *card = snd_stm_card_get(SND_STM_CARD_TYPE_TELSS);
	int result;

	dev_dbg(tdm->dev, "%s(tdm=%p)", __func__, tdm);

	BUG_ON(!tdm);
	BUG_ON(!snd_stm_magic_valid(tdm));

	/* Only configure clk for player */
	if (tdm->info->fdma_direction == DMA_DEV_TO_MEM)
		return 0;

	/* Get the tdm clock */
	tdm->clk = snd_stm_clk_get(tdm->dev, "uniperif_tdm_clk", card,
			tdm->info->card_device);
	if (IS_ERR(tdm->clk)) {
		dev_err(tdm->dev, "Failed to get clk");
		return -EINVAL;
	}

	/* Enable clock here as it must be permanently on */
	result = snd_stm_clk_enable(tdm->clk);
	if (result) {
		dev_err(tdm->dev, "Failed to enable clk");
		goto error_clk_enable;
	}

	/* Set clock rate (rate at which telss devices run) */
	result = snd_stm_clk_set_rate(tdm->clk, tdm->info->clk_rate);
	if (result) {
		dev_err(tdm->dev, "Failed to set clk rate");
		goto error_clk_set_rate;
	}

	return 0;

error_clk_set_rate:
	snd_stm_clk_disable(tdm->clk);
error_clk_enable:
	snd_stm_clk_put(tdm->clk);
	return result;
}

static void uniperif_tdm_clk_put(struct uniperif_tdm *tdm)
{
	dev_dbg(tdm->dev, "%s(tdm=%p)", __func__, tdm);

	BUG_ON(!tdm);
	BUG_ON(!snd_stm_magic_valid(tdm));

	if (!IS_ERR(tdm->clk)) {
		/* Disable the clock */
		snd_stm_clk_disable(tdm->clk);
		/* Relinquish the clock */
		snd_stm_clk_put(tdm->clk);
		/* Null the clock pointer */
		tdm->clk = NULL;
	}
}

static int uniperif_tdm_pclk_get(struct uniperif_tdm *tdm)
{
	struct snd_card *card = snd_stm_card_get(SND_STM_CARD_TYPE_TELSS);
	int result;

	dev_dbg(tdm->dev, "%s(tdm=%p)", __func__, tdm);

	BUG_ON(!tdm);
	BUG_ON(!snd_stm_magic_valid(tdm));

	/* Only configure pclk for player */
	if (tdm->info->fdma_direction == DMA_DEV_TO_MEM)
		return 0;

	/* Get the tdm pclk */
	tdm->pclk = snd_stm_clk_get(tdm->dev, "uniperif_tdm_pclk", card,
			tdm->info->card_device);
	if (IS_ERR(tdm->pclk)) {
		dev_err(tdm->dev, "Failed to get pclk");
		return -EINVAL;
	}

	/* Enable pclk here as it must be permanently on */
	result = snd_stm_clk_enable(tdm->pclk);
	if (result) {
		dev_err(tdm->dev, "Failed to enable pclk");
		goto error_pclk_enable;
	}

	/* Set pclk rate (rate at which telss devices run) */
	result = snd_stm_clk_set_rate(tdm->pclk, tdm->info->pclk_rate);
	if (result) {
		dev_err(tdm->dev, "Failed to set pclk rate");
		goto error_pclk_set_rate;
	}

	return 0;

error_pclk_set_rate:
	snd_stm_clk_disable(tdm->pclk);
error_pclk_enable:
	snd_stm_clk_put(tdm->pclk);
	return result;
}

static void uniperif_tdm_pclk_put(struct uniperif_tdm *tdm)
{
	dev_dbg(tdm->dev, "%s(tdm=%p)", __func__, tdm);

	BUG_ON(!tdm);
	BUG_ON(!snd_stm_magic_valid(tdm));

	if (!IS_ERR(tdm->pclk)) {
		/* Disable the clock */
		snd_stm_clk_disable(tdm->pclk);
		/* Relinquish the clock */
		snd_stm_clk_put(tdm->pclk);
		/* Null the clock pointer */
		tdm->pclk = NULL;
	}
}

static void uniperif_tdm_hw_reset(struct uniperif_tdm *tdm)
{
	dev_dbg(tdm->dev, "%s(tdm=%p)", __func__, tdm);

	BUG_ON(!tdm);
	BUG_ON(!snd_stm_magic_valid(tdm));

	/* Set the soft reset */
	set__AUD_UNIPERIF_SOFT_RST__SOFT_RST(tdm);

	/* Issue a soft reset to ensure consistent IP internal state */
	while (get__AUD_UNIPERIF_SOFT_RST__SOFT_RST(tdm))
		udelay(5);
}

static void uniperif_tdm_hw_enable(struct uniperif_tdm *tdm)
{
	int i;

	dev_dbg(tdm->dev, "%s(tdm=%p)", __func__, tdm);

	BUG_ON(!tdm);
	BUG_ON(!snd_stm_magic_valid(tdm));

	/* Enable the tdm functionality */
	set__AUD_UNIPERIF_TDM_ENABLE__TDM_ENABLE(tdm);

	/* Prime the FIFO to start internal clocking (player only) */
	if (tdm->info->fdma_direction == DMA_MEM_TO_DEV)
		for (i = 0; i < tdm->info->frame_size; ++i)
			set__AUD_UNIPERIF_FIFO_DATA(tdm, 0x00000000);

	/* The uniperipheral must be on to supply a clock to telss hardware */
	set__AUD_UNIPERIF_CTRL__OPERATION_PCM_DATA(tdm);
}

static void uniperif_tdm_hw_disable(struct uniperif_tdm *tdm)
{
	dev_dbg(tdm->dev, "%s(tdm=%p)", __func__, tdm);

	BUG_ON(!tdm);
	BUG_ON(!snd_stm_magic_valid(tdm));

	/* Disable the tdm functionality */
	set__AUD_UNIPERIF_TDM_ENABLE__TDM_DISABLE(tdm);

	/* Set operating mode to off */
	set__AUD_UNIPERIF_CTRL__OPERATION_OFF(tdm);

	/* Issue a soft reset to ensure consistent IP internal state */
	uniperif_tdm_hw_reset(tdm);
}

static void uniperif_tdm_hw_start(struct uniperif_tdm *tdm)
{
	dev_dbg(tdm->dev, "%s(tdm=%p)", __func__, tdm);

	BUG_ON(!tdm);
	BUG_ON(!snd_stm_magic_valid(tdm));

	/* Start dma transfer */
	tdm->dma_cookie = dmaengine_submit(tdm->dma_descriptor);

	/* Reset the fifo error count */
	tdm->dma_fifo_errors = 0;

	/* Enable interrupts */
	set__AUD_UNIPERIF_ITS_BCLR__FIFO_ERROR(tdm);
	set__AUD_UNIPERIF_ITM_BSET__FIFO_ERROR(tdm);
	set__AUD_UNIPERIF_ITS_BCLR__DMA_ERROR(tdm);
	set__AUD_UNIPERIF_ITM_BSET__DMA_ERROR(tdm);
	enable_irq(tdm->irq);

	/* Turn uniperipheral on (reader only - player always on) */
	if (tdm->info->fdma_direction == DMA_DEV_TO_MEM) {
		uniperif_tdm_hw_reset(tdm);
		uniperif_tdm_hw_enable(tdm);
	}
}

static void uniperif_tdm_hw_stop(struct uniperif_tdm *tdm)
{
	dev_dbg(tdm->dev, "%s(tdm=%p)", __func__, tdm);

	BUG_ON(!tdm);
	BUG_ON(!snd_stm_magic_valid(tdm));

	/* Disable interrupts */
	disable_irq_nosync(tdm->irq);
	set__AUD_UNIPERIF_ITM_BCLR__FIFO_ERROR(tdm);
	set__AUD_UNIPERIF_ITM_BCLR__DMA_ERROR(tdm);

	/* Turn uniperipheral off (reader only - player always on) */
	if (tdm->info->fdma_direction == DMA_DEV_TO_MEM)
		uniperif_tdm_hw_disable(tdm);

	/* Terminate the dma */
	dmaengine_terminate_all(tdm->dma_channel);
}

static void uniperif_tdm_dump_registers(struct snd_info_entry *entry,
		struct snd_info_buffer *buffer)
{
	struct uniperif_tdm *tdm = entry->private_data;

	BUG_ON(!tdm);
	BUG_ON(!snd_stm_magic_valid(tdm));

	snd_iprintf(buffer, "--- %s (0x%p) ---\n", dev_name(tdm->dev),
			tdm->base);

	UNIPERIF_TDM_DUMP_REGISTER(SOFT_RST);
	UNIPERIF_TDM_DUMP_REGISTER(STA);
	UNIPERIF_TDM_DUMP_REGISTER(ITS);
	UNIPERIF_TDM_DUMP_REGISTER(ITM);
	UNIPERIF_TDM_DUMP_REGISTER(CONFIG);
	UNIPERIF_TDM_DUMP_REGISTER(CTRL);
	UNIPERIF_TDM_DUMP_REGISTER(I2S_FMT);
	UNIPERIF_TDM_DUMP_REGISTER(STATUS_1);
	UNIPERIF_TDM_DUMP_REGISTER(DFV0);
	UNIPERIF_TDM_DUMP_REGISTER(CONTROLABILITY);
	UNIPERIF_TDM_DUMP_REGISTER(CRC_CTRL);
	UNIPERIF_TDM_DUMP_REGISTER(CRC_WINDOW);
	UNIPERIF_TDM_DUMP_REGISTER(CRC_VALUE_IN);
	UNIPERIF_TDM_DUMP_REGISTER(CRC_VALUE_OUT);
	UNIPERIF_TDM_DUMP_REGISTER(TDM_ENABLE);

	/* Only output frequency registers if a player */
	if (tdm->info->fdma_direction == DMA_MEM_TO_DEV) {
		UNIPERIF_TDM_DUMP_REGISTER(TDM_FS_REF_FREQ);
		UNIPERIF_TDM_DUMP_REGISTER(TDM_FS_REF_DIV);
		UNIPERIF_TDM_DUMP_REGISTER(TDM_FS01_FREQ);
		UNIPERIF_TDM_DUMP_REGISTER(TDM_FS01_WIDTH);
		UNIPERIF_TDM_DUMP_REGISTER(TDM_FS02_FREQ);
		UNIPERIF_TDM_DUMP_REGISTER(TDM_FS02_WIDTH);
		UNIPERIF_TDM_DUMP_REGISTER(TDM_FS02_TIMESLOT_DELAY);
	}

	UNIPERIF_TDM_DUMP_REGISTER(TDM_DATA_MSBIT_START);
	UNIPERIF_TDM_DUMP_REGISTER(TDM_WORD_POS_1_2);
	UNIPERIF_TDM_DUMP_REGISTER(TDM_WORD_POS_3_4);
	UNIPERIF_TDM_DUMP_REGISTER(TDM_WORD_POS_5_6);
	UNIPERIF_TDM_DUMP_REGISTER(TDM_WORD_POS_7_8);

	snd_iprintf(buffer, "\n");
}

static int uniperif_tdm_register(struct snd_device *snd_device)
{
	struct uniperif_tdm *tdm = snd_device->device_data;
	int result;

	dev_dbg(tdm->dev, "%s(snd_device=%p)", __func__, snd_device);

	BUG_ON(!tdm);
	BUG_ON(!snd_stm_magic_valid(tdm));

	/* Get the tdm clock */
	result = uniperif_tdm_clk_get(tdm);
	if (result) {
		dev_err(tdm->dev, "Failed to get clock");
		goto error_clk_get;
	}

	/* Get the tdm pclk */
	result = uniperif_tdm_pclk_get(tdm);
	if (result) {
		dev_err(tdm->dev, "Failed to get pclk");
		goto error_pclk_get;
	}

	/* Configure the tdm device */
	result = uniperif_tdm_configure(tdm);
	if (result) {
		dev_err(tdm->dev, "Failed to configure");
		goto error_configure;
	}

	/* Add procfs info entry */
	result = snd_stm_info_register(&tdm->proc_entry, dev_name(tdm->dev),
			uniperif_tdm_dump_registers, tdm);
	if (result) {
		dev_err(tdm->dev, "Failed to register with procfs");
		goto error_info_register;
	}

	/* Enable the tdm device immediately (player only) */
	if (tdm->info->fdma_direction == DMA_MEM_TO_DEV)
		uniperif_tdm_hw_enable(tdm);

	return 0;

error_info_register:
error_configure:
	uniperif_tdm_pclk_put(tdm);
error_pclk_get:
	uniperif_tdm_clk_put(tdm);
error_clk_get:
	return result;
}

static int uniperif_tdm_disconnect(struct snd_device *snd_device)
{
	struct uniperif_tdm *tdm = snd_device->device_data;

	dev_dbg(tdm->dev, "%s(snd_device=%p)", __func__, snd_device);

	BUG_ON(!tdm);
	BUG_ON(!snd_stm_magic_valid(tdm));

	/* Release any dma channel */
	uniperif_tdm_dma_release(tdm);

	/* Disable the tdm device */
	uniperif_tdm_hw_disable(tdm);

	/* Relinquish the clocks */
	uniperif_tdm_pclk_put(tdm);
	uniperif_tdm_clk_put(tdm);

	/* Remove procfs info entry */
	snd_stm_info_unregister(tdm->proc_entry);

	return 0;
}

static struct snd_device_ops uniperif_tdm_snd_device_ops = {
	.dev_register	= uniperif_tdm_register,
	.dev_disconnect	= uniperif_tdm_disconnect,
};


/*
 * Platform driver initialisation.
 */

static int uniperif_tdm_handset_init(struct uniperif_tdm *tdm, int id)
{
	struct snd_card *card = snd_stm_card_get(SND_STM_CARD_TYPE_TELSS);
	struct telss_handset *handset;
	int direction;
	int playback;
	int capture;
	char *name;
	int result;

	dev_dbg(tdm->dev, "%s(tdm=%p, id=%d)", __func__, tdm, id);

	BUG_ON(!tdm);
	BUG_ON(!snd_stm_magic_valid(tdm));

	/* Get a pointer to the handset structure */
	handset = &tdm->handsets[id];

	/* Initialise handset structure */
	handset->id = id;
	handset->ctl = uniperif_tdm_handset_pcm_ctl++;
	handset->tdm = tdm;
	handset->info = &tdm->info->handset_info[id];
	handset->period_max_sz = tdm->period_max_sz / tdm->info->handset_count;
	handset->buffer_max_sz = tdm->buffer_max_sz / tdm->info->handset_count;
	handset->buffer_act_sz = tdm->buffer_act_sz / tdm->info->handset_count;

	/* Align start of this handset buffer to page size for mmap */
	handset->buffer_offset = (handset->buffer_max_sz + PAGE_SIZE - 1);
	handset->buffer_offset /= PAGE_SIZE;
	handset->buffer_offset *= PAGE_SIZE * id;
	snd_stm_magic_set(handset);

	/* Ensure handset buffer offset is within the fdma node range */
	BUG_ON(handset->buffer_offset > UNIPERIF_TDM_BUF_OFF_MAX);

	switch (tdm->info->fdma_direction) {
	case DMA_MEM_TO_DEV:
		name = "TELSS Player #%d";
		capture = 0;
		playback = 1;
		direction = SNDRV_PCM_STREAM_PLAYBACK;
		break;

	case DMA_DEV_TO_MEM:
		name = "TELSS Reader #%d";
		capture = 1;
		playback = 0;
		direction = SNDRV_PCM_STREAM_CAPTURE;
		break;

	default:
		dev_err(tdm->dev, "Cannot determine if player/reader");
		return -EINVAL;
	}

	/* Create a new ALSA playback pcm device for handset */
	result = snd_pcm_new(card, NULL, handset->ctl, playback, capture,
				&handset->pcm);
	if (result) {
		dev_err(tdm->dev, "Failed to create pcm for handset %d", id);
		return result;
	}

	/* Set the pcm device name */
	snprintf(handset->pcm->name, sizeof(handset->pcm->name), name, id);
	dev_notice(tdm->dev, "'%s'", handset->pcm->name);

	/* Link the pcm device to the handset */
	handset->pcm->private_data = handset;

	/* Set the pcm device ops */
	snd_pcm_set_ops(handset->pcm, direction, &uniperif_tdm_pcm_ops);

	return 0;
}

static void uniperif_tdm_parse_dt_handsets(struct platform_device *pdev,
		struct device_node *pnode,
		struct snd_stm_uniperif_tdm_info *info)
{
	struct device_node *hnode, *child;
	struct snd_stm_telss_handset_info *hs_info;
	int i = 0;

	BUG_ON(!pdev);
	BUG_ON(!pnode);
	BUG_ON(!info);

	/* Read the handset count property */
	of_property_read_u32(pnode, "handset-count", &info->handset_count);

	/* Get a pointer to the handset info node */
	hnode = of_parse_phandle(pnode, "handset-info", 0);

	/* Calculate how many handset info entries we have */
	for_each_child_of_node(hnode, child)
		i++;

	BUG_ON(i != info->handset_count);

	/* Allocate the handset info structure array */
	hs_info = devm_kzalloc(&pdev->dev,
			sizeof(*hs_info) * info->handset_count,
			GFP_KERNEL);
	BUG_ON(!hs_info);
	info->handset_info = hs_info;

	/* Read each handset properties */
	for_each_child_of_node(hnode, child) {
		of_property_read_u32(child, "fsync", &hs_info->fsync);
		of_property_read_u32(child, "slot1", &hs_info->slot1);
		of_property_read_u32(child, "slot2", &hs_info->slot2);
		of_property_read_u32(child, "slot2-valid",
				&hs_info->slot2_valid);
		of_property_read_u32(child, "duplicate", &hs_info->duplicate);
		of_property_read_u32(child, "data16", &hs_info->data16);
		of_property_read_u32(child, "cnb", &hs_info->cnb);
		of_property_read_u32(child, "lnb", &hs_info->lnb);
		of_property_read_u32(child, "cwb", &hs_info->cwb);
		of_property_read_u32(child, "lwb", &hs_info->lwb);
		hs_info++;
	}
}

static void uniperif_tdm_parse_dt_hw_config(struct platform_device *pdev,
		struct snd_stm_uniperif_tdm_info *info)
{
	struct device_node *pnode;

	BUG_ON(!pdev);
	BUG_ON(!info);

	/* Get a pointer to the handset info node */
	pnode = of_parse_phandle(pdev->dev.of_node, "hw-config", 0);

	of_property_read_u32(pnode, "pclk-rate", &info->pclk_rate);
	of_property_read_u32(pnode, "fs-rate", &info->fs_rate);
	of_property_read_u32(pnode, "fs-divider", &info->timeslots);
	of_property_read_u32(pnode, "fs01-rate", &info->fs01_rate);
	of_property_read_u32(pnode, "fs02-rate", &info->fs02_rate);
	of_property_read_u32(pnode, "fs02-delay-clock",
			&info->fs02_delay_clock);
	of_property_read_u32(pnode, "fs02-delay-timeslot",
			&info->fs02_delay_timeslot);
	of_property_read_u32(pnode, "msbit-start", &info->msbit_start);
	of_property_read_u32(pnode, "frame-size", &info->frame_size);

	/* Read the handset configuration properties */
	uniperif_tdm_parse_dt_handsets(pdev, pnode, info);
}

static int uniperif_tdm_parse_dt(struct platform_device *pdev,
		struct uniperif_tdm *tdm)
{
	struct snd_stm_uniperif_tdm_info *info;
	struct device_node *pnode;
	const char *direction;
	int val;

	BUG_ON(!pdev);
	BUG_ON(!tdm);

	/* Allocate memory for the info structure */
	info = devm_kzalloc(&pdev->dev, sizeof(*info), GFP_KERNEL);
	BUG_ON(!info);

	/* Read the device properties */
	pnode = pdev->dev.of_node;

	of_property_read_string(pnode, "description", &info->name);
	of_property_read_u32(pnode, "version", &info->ver);
	of_property_read_u32(pnode, "card-device", &info->card_device);

	/* Read the fdma properties */
	val = get_property_hdl(&pdev->dev, pnode, "dmas", 0);
	if (!val) {
		dev_err(&pdev->dev, "unable to find DT dma node");
		return -EINVAL;
	}
	info->dma_np = of_find_node_by_phandle(val);
	of_property_read_string(pnode, "fdma-name", &info->fdma_name);
	of_property_read_u32(pnode, "fdma-channel", &info->fdma_channel);
	of_property_read_u32(pnode, "fdma-initiator", &info->fdma_initiator);
	of_property_read_string(pnode, "fdma-direction", &direction);
	of_property_read_u32(pnode, "fdma-direct-conn",
			&info->fdma_direct_conn);
	of_property_read_u32(pnode, "fdma-request-line",
			&info->fdma_request_line);

	/* Map the dma direction */
	if (strcasecmp(direction, "DMA_MEM_TO_DEV") == 0)
		info->fdma_direction = DMA_MEM_TO_DEV;
	else if (strcasecmp(direction, "DMA_DEV_TO_MEM") == 0)
		info->fdma_direction = DMA_DEV_TO_MEM;
	else
		info->fdma_direction = DMA_TRANS_NONE;

	/* Read the driver properties */
	of_property_read_u32(pnode, "rising-edge", &info->rising_edge);
	of_property_read_u32(pnode, "clk-rate", &info->clk_rate);
	of_property_read_u32(pnode, "frame-count", &info->frame_count);
	of_property_read_u32(pnode, "max-periods", &info->max_periods);

	/* Read the hardware configuration properties */
	uniperif_tdm_parse_dt_hw_config(pdev, info);

	/* Save the info structure */
	tdm->info = info;

	return 0;
}

static int uniperif_tdm_probe(struct platform_device *pdev)
{
	struct snd_card *card = snd_stm_card_get(SND_STM_CARD_TYPE_TELSS);
	struct uniperif_tdm *tdm;
	int result;
	int pages;
	int h;

	BUG_ON(!pdev);
	BUG_ON(!card);

	dev_dbg(&pdev->dev, "%s(pdev=%p)\n", __func__, pdev);

	/* Allocate device structure */
	tdm = devm_kzalloc(&pdev->dev, sizeof(*tdm), GFP_KERNEL);
	if (!tdm) {
		dev_err(&pdev->dev, "Failed to allocate device structure");
		return -ENOMEM;
	}

	/* Initialise device structure */
	tdm->dev = &pdev->dev;
	snd_stm_magic_set(tdm);

	if (!pdev->dev.of_node) {
		dev_err(&pdev->dev, "device not in dt");
		return -EINVAL;
	}

	if (uniperif_tdm_parse_dt(pdev, tdm)) {
		dev_err(&pdev->dev, "invalid dt");
		return -EINVAL;
	}

	/* Set the version */
	tdm->ver = tdm->info->ver;
	BUG_ON(tdm->ver < 0);

	spin_lock_init(&tdm->lock);
	mutex_init(&tdm->dma_mutex);

	dev_notice(&pdev->dev, "'%s'", tdm->info->name);

	/* Request memory region */
	result = snd_stm_memory_request(pdev, &tdm->mem_region, &tdm->base);
	if (result < 0) {
		dev_err(&pdev->dev, "Failed memory request");
		return result;
	}

	/* Request irq */
	result = snd_stm_irq_request(pdev, &tdm->irq, uniperif_tdm_irq_handler,
			tdm);
	if (result < 0) {
		dev_err(&pdev->dev, "Failed IRQ request");
		return result;
	}

	/* Link device to sound card (sound card will manage device) */
	result = snd_device_new(card, SNDRV_DEV_LOWLEVEL, tdm,
			&uniperif_tdm_snd_device_ops);
	if (result < 0) {
		dev_err(&pdev->dev, "Failed to create ALSA sound device");
		goto error_device_new;
	}

	/* Bounds check the number of handsets (handsets != I2S channels) */
	BUG_ON(tdm->info->handset_count < UNIPERIF_TDM_MIN_HANDSETS);
	BUG_ON(tdm->info->handset_count > UNIPERIF_TDM_MAX_HANDSETS);

	/*
	 * Here we calculate various period and buffer sizes. Each handset
	 * buffer is contiguous and aligned to 8-bytes as the offset to each
	 * buffer must be shifted down 3-bits when programmed into the FDMA
	 * handset node parameters. All handset buffers must fit within the
	 * programmable handset offset address range (i.e. 0 - 131,064). To
	 * ensure buffer remains contiguous at 8/16/32-bit sample sizes we
	 * must have a frame count that is a multiple of 8.
	 */

	/* Ensure frame count is a multiple of 8 */
	BUG_ON(tdm->info->frame_count % 8);

	/* Calculate maximum period size for 32-bit sample (for all handsets) */
	tdm->period_max_sz = tdm->info->frame_size * 4 * tdm->info->frame_count;

	/* Calculate number of periods that we can fit within handset offset */
	tdm->period_act_cnt = UNIPERIF_TDM_BUF_OFF_MAX;
	tdm->period_act_cnt /= tdm->period_max_sz;
	tdm->period_act_cnt /= tdm->info->handset_count;

	/* Ensure we do not have less than the minimum number of periods */
	BUG_ON(tdm->period_act_cnt < UNIPERIF_TDM_MIN_PERIODS);

	/* Ensure max period config is larger than or equal to the minimum */
	if (tdm->info->max_periods < UNIPERIF_TDM_MIN_PERIODS)
		tdm->info->max_periods = UNIPERIF_TDM_DEF_PERIODS;

	/* Bounds check against the maximum number of periods */
	if (tdm->period_act_cnt > tdm->info->max_periods)
		tdm->period_act_cnt = tdm->info->max_periods;

	/* Output a message if we don't have as many periods as expected */
	if (tdm->period_act_cnt != tdm->info->max_periods)
		dev_notice(&pdev->dev, "Using %d periods not %d",
				tdm->period_act_cnt, tdm->info->max_periods);

	/* Set the maximum periods */
	tdm->period_max_cnt = tdm->period_act_cnt;

	/* Calculate maximum buffer size (32-bit samples) and set as actual */
	tdm->buffer_max_sz = tdm->period_max_sz * tdm->period_act_cnt;
	tdm->buffer_act_sz = tdm->buffer_max_sz;

	/* Calculate dma maxburst/trigger limit as a multiple of frame size */
	tdm->dma_maxburst = UNIPERIF_TDM_DMA_MAXBURST / tdm->info->frame_size;
	tdm->dma_maxburst *= tdm->info->frame_size;

	/*
	 * We can't use the standard core buffer functions as they associate
	 * with a given pcm device and substream. We are in essence sharing a
	 * single buffer with multiple pcm devices so need something special.
	 */

	tdm->buffer_bpa2 = bpa2_find_part(CONFIG_SND_STM_BPA2_PARTITION_NAME);
	if (!tdm->buffer_bpa2) {
		dev_err(&pdev->dev, "Failed to find BPA2 partition");
		result = -ENOMEM;
		goto error_bpa2_find;
	}
	dev_notice(&pdev->dev, "BPA2 '%s' at %p",
			CONFIG_SND_STM_BPA2_PARTITION_NAME, tdm->buffer_bpa2);

	/*
	 * This driver determines the number of periods and the period size in
	 * frames that the user application may use. As such we only need to
	 * allocate the dma buffer once.
	 */

	/* Round handset buffer to full page and calculate total buffer size */
	pages = (tdm->buffer_max_sz / tdm->info->handset_count) + PAGE_SIZE - 1;
	pages /= PAGE_SIZE;
	pages *= tdm->info->handset_count;

	/* Allocate the physical pages for internal buffer */
	tdm->buffer_phys = bpa2_alloc_pages(tdm->buffer_bpa2, pages,
				0, GFP_KERNEL);
	if (!tdm->buffer_phys) {
		dev_err(tdm->dev, "Failed to allocate BPA2 pages");
		result = -ENOMEM;
		goto error_bpa2_alloc;
	}

	/* do not allocate memory inside the kernel address space */
	if (pfn_valid(__phys_to_pfn(tdm->buffer_phys))) {
		dev_err(tdm->dev,
			"BPA2 page 0x%08x is inside the kernel address space",
			tdm->buffer_phys);
		result = -ENOMEM;
		goto error_bpa2_alloc;
	}

	/* Map the physical pages to uncached memory */
	tdm->buffer_area = ioremap_nocache(tdm->buffer_phys, pages * PAGE_SIZE);
	BUG_ON(!tdm->buffer_area);

	dev_notice(tdm->dev, "Buffer at %08x (phys)", tdm->buffer_phys);
	dev_notice(tdm->dev, "Buffer at %p (virt)", tdm->buffer_area);

	/* Process each handset to be supported */
	for (h = 0; h < tdm->info->handset_count; ++h) {
		/* Create a new pcm device for the handset */
		result = uniperif_tdm_handset_init(tdm, h);
		if (result) {
			dev_err(&pdev->dev, "Failed to init handset %d", h);
			goto error_handset_init;
		}
	}

	/* Register the telss sound card */
	result = snd_card_register(card);
	if (result) {
		dev_err(&pdev->dev, "Failed to register card (%d)", result);
		goto error_card_register;
	}

	platform_set_drvdata(pdev, tdm);

	return 0;

error_card_register:
error_handset_init:
	iounmap(tdm->buffer_area);
	bpa2_free_pages(tdm->buffer_bpa2, tdm->buffer_phys);
error_bpa2_alloc:
error_bpa2_find:
	snd_device_free(card, tdm);
error_device_new:
	/*if (tdm->pads)
		stm_pad_release(tdm->pads);*/

	return result;
}

static int uniperif_tdm_remove(struct platform_device *pdev)
{
	struct uniperif_tdm *tdm = platform_get_drvdata(pdev);

	dev_dbg(&pdev->dev, "%s(pdev=%p)\n", __func__, pdev);

	BUG_ON(!tdm);
	BUG_ON(!snd_stm_magic_valid(tdm));

	/* Unmap and free the internal buffer */
	iounmap(tdm->buffer_area);
	bpa2_free_pages(tdm->buffer_bpa2, tdm->buffer_phys);

	snd_stm_magic_clear(tdm);

	return 0;
}

/*
 * Power management
 */

#ifdef CONFIG_PM
static int uniperif_tdm_suspend(struct device *dev)
{
	dev_dbg(dev, "%s(dev=%p)", __func__, dev);
	dev_err(dev, "PM not supported!");
	return -ENOTSUPP;
}

static int uniperif_tdm_resume(struct device *dev)
{
	dev_dbg(dev, "%s(dev=%p)", __func__, dev);
	dev_err(dev, "PM not supported!");
	return -ENOTSUPP;
}

static const struct dev_pm_ops uniperif_tdm_pm_ops = {
	.suspend = uniperif_tdm_suspend,
	.resume	 = uniperif_tdm_resume,

	.freeze	 = uniperif_tdm_suspend,
	.thaw	 = uniperif_tdm_resume,
	.restore = uniperif_tdm_resume,
};
#endif

/*
 * Module initialisation.
 */

static struct of_device_id uniperif_tdm_match[] = {
	{
		.compatible = "st,snd_uni_tdm",
	},
	{},
};

MODULE_DEVICE_TABLE(of, uniperif_tdm_match);

static struct platform_driver uniperif_tdm_platform_driver = {
	.driver.name	= "snd_uni_tdm",
	.driver.of_match_table = uniperif_tdm_match,
#ifdef CONFIG_PM
	.driver.pm	= &uniperif_tdm_pm_ops,
#endif
	.probe		= uniperif_tdm_probe,
	.remove		= uniperif_tdm_remove,
};

module_platform_driver(uniperif_tdm_platform_driver);

MODULE_AUTHOR("John Boddie <john.boddie@st.com>");
MODULE_DESCRIPTION("STMicroelectronics Uniperipheral TDM driver");
MODULE_LICENSE("GPL");
