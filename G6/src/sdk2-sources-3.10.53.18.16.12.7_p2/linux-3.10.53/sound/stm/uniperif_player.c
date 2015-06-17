/*
 *   STMicroelectronics System-on-Chips' Uniperipheral player driver
 *
 *   Copyright (c) 2005-2013 STMicroelectronics Limited
 *
 *   Author: John Boddie <john.boddie@st.com>
 *           Sevanand Singh <sevanand.singh@st.com>
 *           Mark Glaisher
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
#include <asm/cacheflush.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/semaphore.h>
#include <linux/delay.h>
#include <linux/pm_runtime.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/control.h>
#include <sound/info.h>
#include <sound/pcm_params.h>
#include <linux/platform_data/dma-st-fdma.h>
#include "common.h"
#include "reg_aud_uniperif.h"
#include <linux/pinctrl/consumer.h>

/*
 * Some hardware-related definitions
 */

#define DEFAULT_FORMAT (SND_STM_FORMAT__I2S | SND_STM_FORMAT__SUBFRAME_32_BITS)
#define DEFAULT_OVERSAMPLING 128 /* make all ip's running at same rate*/

/* The sample count field (NSAMPLES in CTRL register) is 19 bits wide */
#define MAX_SAMPLES_PER_PERIOD ((1 << 20) - 1)

#define MIN_IEC958_SAMPLE_RATE	32000

#define PARKING_SUBBLOCKS	4
#define PARKING_NBFRAMES	4
#define PARKING_BUFFER_SIZE	128	/* Optimal FDMA transfer is 128-bytes */

#define UNIPERIF_FIFO_SIZE	70	/* FIFO is 70 cells deep */
#define UNIPERIF_FIFO_FRAMES	4	/* FDMA trigger limit in frames */
#define UNIPERIF_PLAYER_UNDERFLOW_US	1000

#define MIN_AUTOSUPEND_DELAY_MS	100
#define UNIPERIF_PLAYER_NO_SUSPEND	(-1)
/*
 * Driver specific types.
 */

enum uniperif_iec958_input_mode {
	UNIPERIF_IEC958_INPUT_MODE_PCM,
	UNIPERIF_IEC958_INPUT_MODE_RAW
};

enum uniperif_iec958_encoding_mode {
	UNIPERIF_IEC958_ENCODING_MODE_PCM,
	UNIPERIF_IEC958_ENCODING_MODE_ENCODED
};

struct uniperif_iec958_settings {
	enum uniperif_iec958_input_mode input_mode;
	enum uniperif_iec958_encoding_mode encoding_mode;
	struct snd_aes_iec958 iec958;
};

enum uniperif_player_state {
	UNIPERIF_PLAYER_STATE_STOPPED,
	UNIPERIF_PLAYER_STATE_STARTED,
	UNIPERIF_PLAYER_STATE_PARKING,
	UNIPERIF_PLAYER_STATE_STANDBY,
	UNIPERIF_PLAYER_STATE_UNDERFLOW
};

enum snd_stm_uniperif_player_type {
	SND_STM_UNIPERIF_PLAYER_TYPE_NONE,
	SND_STM_UNIPERIF_PLAYER_TYPE_HDMI,
	SND_STM_UNIPERIF_PLAYER_TYPE_PCM,
	SND_STM_UNIPERIF_PLAYER_TYPE_SPDIF
};

struct snd_stm_uniperif_player_info {
	const char *name;
	int ver;

	int card_device;
	enum snd_stm_uniperif_player_type player_type;

	unsigned int iec958_lr_pol;		/* HDMI/SPDIF LR polarity */
	unsigned int iec958_i2s_mode;		/* HDMI/SPDIF I2S mode */

	unsigned int channels;

	int s16_swap_lr;			/* S16LE: swap left/right */
	int parking_enabled;
	int standby_enabled;
	int underflow_enabled;			/* Underflow recovery mode */

	const char *fdma_name;
	struct device_node *dma_np;
	unsigned int fdma_initiator;
	unsigned int fdma_direct_conn;
	unsigned int fdma_request_line;

	unsigned int suspend_delay;
};

struct uniperif_player {
	/* System information */
	struct snd_stm_uniperif_player_info *info;
	struct device *dev;
	struct snd_pcm *pcm;
	int ver; /* IP version, used by register access macros */

	/* Resources */
	struct resource *mem_region;
	void *base;
	unsigned long fifo_phys_address;
	unsigned int irq;
	int fdma_channel;

	/* Environment settings */
	struct snd_stm_clk *clock;
	struct snd_pcm_hw_constraint_list channels_constraint;
	struct snd_stm_conv_source *conv_source;

	/* Runtime data */
	enum uniperif_player_state state;
	struct snd_stm_conv_group *conv_group;
	struct snd_stm_buffer *buffer;
	struct snd_info_entry *proc_entry;
	struct snd_pcm_substream *substream;

	struct snd_pcm_hardware hardware;

	/* Specific to IEC958 player */
	spinlock_t default_settings_lock;
	struct uniperif_iec958_settings default_settings;
	struct uniperif_iec958_settings stream_settings;

	int dma_max_transfer_size;
	struct st_dma_audio_config dma_config;
	struct dma_chan *dma_channel;
	struct dma_async_tx_descriptor *dma_descriptor;
	dma_cookie_t dma_cookie;
	struct st_dma_park_config dma_park_config;

	/* Underflow specific */
	int underflow_us;

	int buffer_bytes;
	int period_bytes;

	unsigned int format;
	unsigned int oversampling;

	/* Configuration */
	unsigned int current_rate;
	unsigned int current_format;
	unsigned int current_channels;

	const char *clk_name;

	snd_stm_magic_field;
};

#define UNIPERIF_PLAYER_TYPE_IS_HDMI(p) \
	((p)->info->player_type == SND_STM_UNIPERIF_PLAYER_TYPE_HDMI)
#define UNIPERIF_PLAYER_TYPE_IS_PCM(p) \
	((p)->info->player_type == SND_STM_UNIPERIF_PLAYER_TYPE_PCM)
#define UNIPERIF_PLAYER_TYPE_IS_SPDIF(p) \
	((p)->info->player_type == SND_STM_UNIPERIF_PLAYER_TYPE_SPDIF)
#define UNIPERIF_PLAYER_TYPE_IS_IEC958(p) \
	(UNIPERIF_PLAYER_TYPE_IS_HDMI(p) || \
		UNIPERIF_PLAYER_TYPE_IS_SPDIF(p))

static struct snd_pcm_hardware uniperif_player_pcm_hw = {
	.info		= (SNDRV_PCM_INFO_MMAP |
				SNDRV_PCM_INFO_MMAP_VALID |
				SNDRV_PCM_INFO_INTERLEAVED |
				SNDRV_PCM_INFO_BLOCK_TRANSFER |
				SNDRV_PCM_INFO_PAUSE),
	.formats	= (SNDRV_PCM_FMTBIT_S32_LE |
				SNDRV_PCM_FMTBIT_S16_LE),

	.rates		= SNDRV_PCM_RATE_CONTINUOUS,
	.rate_min	= 8000,
	.rate_max	= 192000,

	.channels_min	= 2,
	.channels_max	= 10,

	.periods_min	= 2,
	.periods_max	= 1024,  /* TODO: sample, work out this somehow... */

	/* Values below were worked out mostly basing on ST media player
	 * requirements. They should, however, fit most "normal" cases...
	 * Note 1: that these value must be also calculated not to exceed
	 * NSAMPLE interrupt counter size (19 bits) - MAX_SAMPLES_PER_PERIOD.
	 * Note 2: for 16/16-bits data this counter is a "frames counter",
	 * not "samples counter" (two channels are read as one word).
	 * Note 3: period_bytes_min defines minimum time between period
	 * (NSAMPLE) interrupts... Keep it large enough not to kill
	 * the system... */
	.period_bytes_min = 1280, /* 320 frames @ 32kHz, 16 bits, 2 ch. */
	.period_bytes_max = 196608, /* DTSHD: 6144 frms @ 192kHz, 32 bits, 8ch */
	.buffer_bytes_max = 196608 * 3, /* 3 worst-case-periods */
};

static struct snd_pcm_hardware uniperif_player_raw_hw = {
	.info		= (SNDRV_PCM_INFO_MMAP |
				SNDRV_PCM_INFO_MMAP_VALID |
				SNDRV_PCM_INFO_INTERLEAVED |
				SNDRV_PCM_INFO_BLOCK_TRANSFER |
				SNDRV_PCM_INFO_PAUSE),
	.formats	= (SNDRV_PCM_FMTBIT_S32_LE),

	.rates		= SNDRV_PCM_RATE_CONTINUOUS,
	.rate_min	= 32000,
	.rate_max	= 192000,

	.channels_min	= 2,
	.channels_max	= 2,

	.periods_min	= 2,
	.periods_max	= 1024,  /* TODO: sample, work out this somehow... */

	/* See above... */
	.period_bytes_min = 1280, /* 320 frames @ 32kHz, 16 bits, 2 ch. */
	.period_bytes_max = 81920, /* 2048 frames @ 192kHz, 32 bits, 10 ch. */
	.buffer_bytes_max = 81920 * 3, /* 3 worst-case-periods */
};

/*
 * Uniperipheral player functions
 */

static int uniperif_player_stop(struct snd_pcm_substream *substream);

/*
 * Uniperipheral player implementation
 */

static irqreturn_t uniperif_player_irq_handler(int irq, void *dev_id)
{
	irqreturn_t result = IRQ_NONE;
	struct uniperif_player *player = dev_id;
	unsigned int status;
	unsigned int tmp;

	BUG_ON(!player);
	BUG_ON(!snd_stm_magic_valid(player));

	/* Get interrupt status & clear them immediately */
	preempt_disable();
	status = get__AUD_UNIPERIF_ITS(player);
	set__AUD_UNIPERIF_ITS_BCLR(player, status);
	preempt_enable();

	snd_pcm_stream_lock(player->substream);
	if ((player->state == UNIPERIF_PLAYER_STATE_STANDBY) ||
		(player->state == UNIPERIF_PLAYER_STATE_STOPPED)) {
		/* unexpected IRQ: do nothing */
		dev_warn(player->dev, "unexpected IRQ: status flag: %#x",
			status);
		snd_pcm_stream_unlock(player->substream);

		return IRQ_HANDLED;
	};
	/* Check for fifo error (under run) */
	if (unlikely(status & mask__AUD_UNIPERIF_ITS__FIFO_ERROR(player))) {
		dev_err(player->dev, "FIFO error detected");

		/* Interrupt is just for information when underflow recovery */
		if (player->info->underflow_enabled) {
			/* Update state to underflow */
			player->state = UNIPERIF_PLAYER_STATE_UNDERFLOW;
		} else {
			/* Disable interrupt so doesn't continually fire */
			set__AUD_UNIPERIF_ITM_BCLR__FIFO_ERROR(player);

			/* Stop the player */
			snd_pcm_stop(player->substream, SNDRV_PCM_STATE_XRUN);
		}

		result = IRQ_HANDLED;
	}

	/* Check for dma error (over run) */
	if (unlikely(status & mask__AUD_UNIPERIF_ITS__DMA_ERROR(player))) {
		dev_err(player->dev, "DMA error detected");

		/* Disable interrupt so doesn't continually fire */
		set__AUD_UNIPERIF_ITM_BCLR__DMA_ERROR(player);

		/* Stop the player */
		snd_pcm_stop(player->substream, SNDRV_PCM_STATE_XRUN);

		result = IRQ_HANDLED;
	}

	/* Check for underflow recovery done */
	if (unlikely(status &
			mask__AUD_UNIPERIF_ITM__UNDERFLOW_REC_DONE(player))) {
		BUG_ON(!player->info->underflow_enabled);

		/* Read the underflow recovery duration */
		tmp = get__AUD_UNIPERIF_STATUS_1__UNDERFLOW_DURATION(player);
		dev_dbg(player->dev, "Underflow recovered (%d LR clocks)", tmp);

		/* Clear the underflow recovery duration */
		set__AUD_UNIPERIF_BIT_CONTROL__CLR_UNDERFLOW_DURATION(player);

		/* Update state to started if still in underflow */
		if (player->state == UNIPERIF_PLAYER_STATE_UNDERFLOW)
			player->state = UNIPERIF_PLAYER_STATE_STARTED;

		result = IRQ_HANDLED;
	}

	/* Checkf or underflow recovery failed */
	if (unlikely(status &
			mask__AUD_UNIPERIF_ITM__UNDERFLOW_REC_FAILED(player))) {
		BUG_ON(!player->info->underflow_enabled);

		dev_err(player->dev, "Underflow recovery failed");

		/* Stop the player */
		snd_pcm_stop(player->substream, SNDRV_PCM_STATE_XRUN);

		result = IRQ_HANDLED;
	}


	/* Abort on unhandled interrupt */
	BUG_ON(result != IRQ_HANDLED);

	snd_pcm_stream_unlock(player->substream);
	return result;
}

static bool uniperif_player_dma_filter_fn(struct dma_chan *chan, void *fn_param)
{
	struct uniperif_player *player = fn_param;
	struct st_dma_audio_config *config = &player->dma_config;

	BUG_ON(!player);

	/* If FDMA name has been specified, attempt to match channel to it */
	if (player->info->dma_np)
		if (player->info->dma_np != chan->device->dev->of_node)
			return false;

	/* Setup this channel for audio operation */
	config->type = ST_DMA_TYPE_AUDIO;
	config->dma_addr = player->fifo_phys_address;
	config->dreq_config.request_line = player->info->fdma_request_line;
	config->dreq_config.direct_conn = player->info->fdma_direct_conn;
	config->dreq_config.initiator = player->info->fdma_initiator;
	config->dreq_config.increment = 0;
	config->dreq_config.hold_off = 0;
	config->dreq_config.maxburst = 1;
	config->dreq_config.buswidth = DMA_SLAVE_BUSWIDTH_4_BYTES;
	config->dreq_config.direction = DMA_MEM_TO_DEV;

	/* Set the default parking configuration */
	config->park_config.sub_periods = PARKING_SUBBLOCKS;
	config->park_config.buffer_size = PARKING_BUFFER_SIZE;

	/* Save the channel config inside the channel structure */
	chan->private = config;

	dev_dbg(player->dev, "Using FDMA '%s' channel %d",
			dev_name(chan->device->dev), chan->chan_id);
	return true;
}

static int uniperif_player_resources_request(struct uniperif_player *player)
{
	dma_cap_mask_t mask;

	BUG_ON(!player);
	BUG_ON(!snd_stm_magic_valid(player));

	if (!player->dma_channel) {
		/* Set the dma channel capabilities we want */
		dma_cap_zero(mask);
		dma_cap_set(DMA_SLAVE, mask);
		dma_cap_set(DMA_CYCLIC, mask);

		/* Request a matching dma channel */
		player->dma_channel = dma_request_channel(mask,
				uniperif_player_dma_filter_fn, player);
		if (!player->dma_channel) {
			dev_err(player->dev, "Failed to request DMA channel");
			return -ENODEV;
		}
	}

	if (!player->conv_group) {
		/* Get handle to any attached converter */
		player->conv_group =
			snd_stm_conv_request_group(player->conv_source);

		dev_dbg(player->dev, "Attached to converter '%s'",
			player->conv_group ?
			snd_stm_conv_get_name(player->conv_group) : "*NONE*");
	}

	return 0;
}

static void uniperif_player_resources_release(struct uniperif_player *player)
{
	BUG_ON(!player);
	BUG_ON(!snd_stm_magic_valid(player));

	/* Do not release resources when parking active */
	if (player->state == UNIPERIF_PLAYER_STATE_PARKING)
		return;

	/* Do not release resources when standby active */
	if (player->state == UNIPERIF_PLAYER_STATE_STANDBY)
		return;

	/* Player should be in the stopped state */
	BUG_ON(player->state != UNIPERIF_PLAYER_STATE_STOPPED);

	if (player->dma_channel) {
		/* Terminate any dma */
		dmaengine_terminate_all(player->dma_channel);

		/* Release the dma channel */
		dma_release_channel(player->dma_channel);
		player->dma_channel = NULL;
	}

	if (player->conv_group) {
		/* Release the converter */
		snd_stm_conv_release_group(player->conv_group);
		player->conv_group = NULL;
	}
}

static int uniperif_player_open(struct snd_pcm_substream *substream)
{
	struct uniperif_player *player = snd_pcm_substream_chip(substream);
	struct snd_pcm_runtime *runtime = substream->runtime;
	int result;

	dev_dbg(player->dev, "%s(substream=%p)", __func__, substream);

	BUG_ON(!player);
	BUG_ON(!snd_stm_magic_valid(player));
	BUG_ON(!runtime);

	/* The player should not be in started or underflow */
	BUG_ON(player->state == UNIPERIF_PLAYER_STATE_STARTED);
	BUG_ON(player->state == UNIPERIF_PLAYER_STATE_UNDERFLOW);

	snd_pcm_set_sync(substream);  /* TODO: ??? */
	pm_runtime_get(player->dev);

	/* Request the dma and converter group resources */
	result = uniperif_player_resources_request(player);
	if (result) {
		dev_err(player->dev, "Failed to request resources");
		return result;
	}

	/* Get default data */

	spin_lock(&player->default_settings_lock);
	player->stream_settings = player->default_settings;
	spin_unlock(&player->default_settings_lock);

	/* Set up channel constraints and inform ALSA */

	result = snd_pcm_hw_constraint_list(runtime, 0,
				SNDRV_PCM_HW_PARAM_CHANNELS,
				&player->channels_constraint);
	if (result < 0) {
		dev_err(player->dev, "Failed to set channel constraint");
		goto error;
	}

	/* It is better when buffer size is an integer multiple of period
	 * size... Such thing will ensure this :-O */
	result = snd_pcm_hw_constraint_integer(runtime,
			SNDRV_PCM_HW_PARAM_PERIODS);
	if (result < 0) {
		dev_err(player->dev, "Failed to constrain buffer periods");
		goto error;
	}

	/* Make the period (so buffer as well) length (in bytes) a multiple
	 * of a FDMA transfer bytes (which varies depending on channels
	 * number and sample bytes) */
	result = snd_stm_pcm_hw_constraint_transfer_bytes(runtime,
			UNIPERIF_FIFO_SIZE * 4);
	if (result < 0) {
		dev_err(player->dev, "Failed to constrain buffer bytes");
		goto error;
	}

	runtime->hw = player->hardware;

	/* Interrupt handler will need the substream pointer... */
	player->substream = substream;

	return 0;

error:
	/* Release the dma and converter group resources */
	uniperif_player_resources_release(player);
	return result;
}

static int uniperif_player_close(struct snd_pcm_substream *substream)
{
	struct uniperif_player *player = snd_pcm_substream_chip(substream);

	dev_dbg(player->dev, "%s(substream=%p)", __func__, substream);

	BUG_ON(!player);
	BUG_ON(!snd_stm_magic_valid(player));

	/* The player should not be in started or underflow */
	BUG_ON(player->state == UNIPERIF_PLAYER_STATE_STARTED);
	BUG_ON(player->state == UNIPERIF_PLAYER_STATE_UNDERFLOW);

	/* if player not stopped, schedule it "suspend_delay" ms */
	if (player->state != UNIPERIF_PLAYER_STATE_STOPPED) {
		if (player->info->suspend_delay !=
				UNIPERIF_PLAYER_NO_SUSPEND) {
			pm_runtime_mark_last_busy(player->dev);
			pm_runtime_put_autosuspend(player->dev);
		}
		return 0;
	}

	/* Release the dma and converter group resources */
	uniperif_player_resources_release(player);

	/* Clear the substream pointer */
	player->substream = NULL;

	return 0;
}

static int uniperif_player_hw_free(struct snd_pcm_substream *substream)
{
	struct uniperif_player *player = snd_pcm_substream_chip(substream);
	struct snd_pcm_runtime *runtime = substream->runtime;

	dev_dbg(player->dev, "%s(substream=%p)", __func__, substream);

	BUG_ON(!player);
	BUG_ON(!snd_stm_magic_valid(player));
	BUG_ON(!runtime);

	/* The player should not be in started or underflow */
	BUG_ON(player->state == UNIPERIF_PLAYER_STATE_STARTED);
	BUG_ON(player->state == UNIPERIF_PLAYER_STATE_UNDERFLOW);

	/* This callback may be called more than once... */

	if (snd_stm_buffer_is_allocated(player->buffer)) {
		/* Stop the dma unless player is in parking mode */
		if ((player->state != UNIPERIF_PLAYER_STATE_PARKING) &&
							(player->dma_channel))
			dmaengine_terminate_all(player->dma_channel);

		/* Free buffer */
		snd_stm_buffer_free(player->buffer);
	}

	return 0;
}

static void uniperif_player_comp_cb(void *param)
{
	struct uniperif_player *player = param;

	BUG_ON(!player);
	BUG_ON(!snd_stm_magic_valid(player));
	BUG_ON(!player->substream);

	snd_pcm_period_elapsed(player->substream);
}

static int uniperif_player_hw_params(struct snd_pcm_substream *substream,
		struct snd_pcm_hw_params *hw_params)
{
	struct uniperif_player *player = snd_pcm_substream_chip(substream);
	struct snd_pcm_runtime *runtime = substream->runtime;
	int buffer_bytes, frame_bytes, period_bytes, periods;
	int transfer_size, transfer_bytes, trigger_limit;
	struct dma_slave_config slave_config;
	int result;

	dev_dbg(player->dev, "%s(substream=%p, hw_params=%p)", __func__,
			substream, hw_params);

	BUG_ON(!player);
	BUG_ON(!snd_stm_magic_valid(player));
	BUG_ON(!runtime);

	/* The player should not be in started or underflow */
	BUG_ON(player->state == UNIPERIF_PLAYER_STATE_STARTED);
	BUG_ON(player->state == UNIPERIF_PLAYER_STATE_UNDERFLOW);

	/* This function may be called many times, so let's be prepared... */
	if (snd_stm_buffer_is_allocated(player->buffer))
		uniperif_player_hw_free(substream);

	/* Allocate buffer */

	buffer_bytes = params_buffer_bytes(hw_params);
	periods = params_periods(hw_params);
	period_bytes = buffer_bytes / periods;
	BUG_ON(periods * period_bytes != buffer_bytes);

	result = snd_stm_buffer_alloc(player->buffer, substream, buffer_bytes);
	if (result) {
		dev_err(player->dev, "Failed to allocate %d-byte buffer",
				buffer_bytes);
		result = -ENOMEM;
		goto error_buf_alloc;
	}

	/*
	 * SDK2 can perform simultaneous playback using multiple uniperipheral
	 * players. As each player can support a different number of channels,
	 * the number of frames that can be stored in the uniperipheral fifo
	 * varies. Assuming 32-bit samples, 2ch allows 35 frames, 4ch allows
	 * 17 frames, 6ch allows 11 frames, and 8ch allows 8 frames.
	 *
	 * To keep everything synchronised regardless of channel configuration,
	 * each player should store the same number of frames in it's fifo at
	 * any given time. In this case we will set the fdma trigger limit and
	 * transfer size such that we store 4 frames of data.
	 *
	 * Despite calculating for 32-bit samples this also works for 16-bit,
	 * (although number of frames obviously doubles). This only works for
	 * 16/32-bit sample sizes.
	 */

	/* Calculate transfer size (in fifo cells and bytes) for frame count */
	transfer_size = params_channels(hw_params) * UNIPERIF_FIFO_FRAMES;
	transfer_bytes = transfer_size * 4;

	/* Calculate number of empty cells available before asserting DREQ */
	if (player->ver < SND_STM_UNIPERIF_VERSION_UNI_PLR_TOP_1_0)
		trigger_limit = UNIPERIF_FIFO_SIZE - transfer_size;
	else
		/* Since SND_STM_UNIPERIF_VERSION_UNI_PLR_TOP_1_0
		 * FDMA_TRIGGER_LIMIT also controls when the state switches
		 * from OFF or STANDBY to AUDIO DATA.*/
		trigger_limit = transfer_size;

	BUG_ON(trigger_limit % 2);

	/* Fifo utilisation should not cross period or buffer boundary */
	BUG_ON(period_bytes % transfer_bytes);
	BUG_ON(buffer_bytes % transfer_bytes);

	/* Trigger limit must be an even number */
	BUG_ON(trigger_limit != 1 && transfer_size % 2);
	BUG_ON(trigger_limit >
			mask__AUD_UNIPERIF_CONFIG__FDMA_TRIGGER_LIMIT(player));

	dev_dbg(player->dev, "FDMA trigger limit %d", trigger_limit);

	set__AUD_UNIPERIF_CONFIG__FDMA_TRIGGER_LIMIT(player, trigger_limit);

	/* Save the buffer bytes and period bytes for when start dma */
	player->buffer_bytes = buffer_bytes;
	player->period_bytes = period_bytes;

	/* Request the dma and converter group resources */
	result = uniperif_player_resources_request(player);
	if (result) {
		dev_err(player->dev, "Failed to request resources");
		return result;
	}

	/* Setup the dma configuration */
	slave_config.direction = DMA_MEM_TO_DEV;
	slave_config.dst_addr = player->fifo_phys_address;
	slave_config.dst_addr_width = DMA_SLAVE_BUSWIDTH_4_BYTES;
	slave_config.dst_maxburst = transfer_size;

	result = dmaengine_slave_config(player->dma_channel, &slave_config);
	if (result) {
		dev_err(player->dev, "Failed to configure DMA channel");
		goto error_dma_config;
	}

	/* Calculate bytes per frame to keep parking buffer to minimum */
	frame_bytes = snd_pcm_format_physical_width(params_format(hw_params)) *
			params_channels(hw_params) / 8;

	/* Set the parking configuration (actually set in 'standby' function) */
	player->dma_park_config.sub_periods = PARKING_SUBBLOCKS;
	player->dma_park_config.buffer_size = transfer_bytes + (frame_bytes-1);
	player->dma_park_config.buffer_size /= frame_bytes;
	player->dma_park_config.buffer_size *= frame_bytes;

	return 0;

error_dma_config:
	snd_stm_buffer_free(player->buffer);
error_buf_alloc:
	return result;
}

static void uniperif_player_set_underflow(struct snd_pcm_substream *substream)
{
	struct uniperif_player *player = snd_pcm_substream_chip(substream);
	unsigned int window;

	dev_dbg(player->dev, "%s(substream=%p)", __func__, substream);

	BUG_ON(!player);
	BUG_ON(!snd_stm_magic_valid(player));
	BUG_ON(!substream->runtime);

	/* Do nothing if underflow is not enabled */
	if (!player->info->underflow_enabled) {
		/* Disable underflow recovery */
		set__AUD_UNIPERIF_CTRL__UNDERFLOW_REC_WINDOW(player, 0);
		return;
	}

	/*
	 * The uniperipheral functional specification contains a formula to
	 * calculate the size of the recovery window in milliseconds:
	 *
	 *     window_ms = reg_value * 1/Fs
	 *
	 * This formula can be re-arranged to calculate the value to set the
	 * recovery window to for a specified time in microseconds:
	 *
	 *     reg_value = (window_us * (Fs/1000) + 500) / 1000
	 *
	 * The maximum size of the window will vary based on sampling rate,
	 * e.g. 1.328ms @ 192kHz, 5.312ms @ 48kHz etc.
	 */

	/* Calculate window base on us and current sample rate */
	window = player->underflow_us * (substream->runtime->rate / 1000);
	/* Round window value up */
	window += 500;
	/* Scale down window range */
	window /= 1000;

	/* Cap the window value if exceeds maximum size */
	if (window > mask__AUD_UNIPERIF_CTRL__UNDERFLOW_REC_WINDOW(player))
		window = mask__AUD_UNIPERIF_CTRL__UNDERFLOW_REC_WINDOW(player);

	/* Set the new window value */
	set__AUD_UNIPERIF_CTRL__UNDERFLOW_REC_WINDOW(player, window);
}

static int uniperif_player_prepare_pcm(struct uniperif_player *player,
		struct snd_pcm_runtime *runtime)
{
	int bits_in_output_frame;
	int lr_pol;

	dev_dbg(player->dev, "%s(player=%p, runtime=%p)", __func__,
			player, runtime);

	/* The player should be stopped when setting configuration */
	BUG_ON(player->state != UNIPERIF_PLAYER_STATE_STOPPED);

	/* Set the default format and oversampling for later */
	player->format = DEFAULT_FORMAT;
	player->oversampling = DEFAULT_OVERSAMPLING;

	/* Get format & oversampling value from connected converter */
	if (player->conv_group) {
		player->format = snd_stm_conv_get_format(player->conv_group);
		player->oversampling =
			snd_stm_conv_get_oversampling(player->conv_group);
		if (player->oversampling == 0)
			player->oversampling = DEFAULT_OVERSAMPLING;
	}

	dev_dbg(player->dev, "Sample frequency %d (oversampling %d)",
			runtime->rate, player->oversampling);

	BUG_ON(player->oversampling < 0);

	/* For 32 bits subframe oversampling must be a multiple of 128,
	 * for 16 bits - of 64 */
	BUG_ON((player->format & SND_STM_FORMAT__SUBFRAME_32_BITS) &&
			(player->oversampling % 128));
	BUG_ON((player->format & SND_STM_FORMAT__SUBFRAME_16_BITS) &&
			(player->oversampling % 64));

	/* Number of bits per subframe (which is one channel sample)
	 * on output - it determines serial clock frequency, which is
	 * 64 times sampling rate for 32 bits subframe (2 channels 32
	 * bits each means 64 bits per frame) and 32 times sampling
	 * rate for 16 bits subframe
	 * (you know why, don't you? :-) */
	switch (player->format & SND_STM_FORMAT__SUBFRAME_MASK) {
	case SND_STM_FORMAT__SUBFRAME_32_BITS:
		dev_dbg(player->dev, "32-bit subframe");
		set__AUD_UNIPERIF_I2S_FMT__NBIT_32(player);
		set__AUD_UNIPERIF_I2S_FMT__DATA_SIZE_32(player);
		bits_in_output_frame = 64; /* frame = 2 * subframe */
		break;
	case SND_STM_FORMAT__SUBFRAME_16_BITS:
		dev_dbg(player->dev, "16-bit subframe");
		set__AUD_UNIPERIF_I2S_FMT__NBIT_16(player);
		set__AUD_UNIPERIF_I2S_FMT__DATA_SIZE_16(player);
		bits_in_output_frame = 32; /* frame = 2 * subframe */
		break;
	default:
		snd_BUG();
		return -EINVAL;
	}

	switch (player->format & SND_STM_FORMAT__MASK) {
	case SND_STM_FORMAT__I2S:
		dev_dbg(player->dev, "I2S format");
		set__AUD_UNIPERIF_I2S_FMT__ALIGN_LEFT(player);
		set__AUD_UNIPERIF_I2S_FMT__PADDING_I2S_MODE(player);
		lr_pol = value__AUD_UNIPERIF_I2S_FMT__LR_POL_LOW(player);
		break;
	case SND_STM_FORMAT__LEFT_JUSTIFIED:
		dev_dbg(player->dev, "Left Justified format");
		set__AUD_UNIPERIF_I2S_FMT__ALIGN_LEFT(player);
		set__AUD_UNIPERIF_I2S_FMT__PADDING_SONY_MODE(player);
		lr_pol = value__AUD_UNIPERIF_I2S_FMT__LR_POL_HIG(player);
		break;
	case SND_STM_FORMAT__RIGHT_JUSTIFIED:
		dev_dbg(player->dev, "Right Justified format");
		set__AUD_UNIPERIF_I2S_FMT__ALIGN_RIGHT(player);
		set__AUD_UNIPERIF_I2S_FMT__PADDING_SONY_MODE(player);
		lr_pol = value__AUD_UNIPERIF_I2S_FMT__LR_POL_HIG(player);
		break;
	default:
		snd_BUG();
		return -EINVAL;
	}

	/* Configure data memory format */

	switch (runtime->format) {
	case SNDRV_PCM_FORMAT_S16_LE:
		/* One data word contains two samples */
		set__AUD_UNIPERIF_CONFIG__MEM_FMT_16_16(player);
		break;

	case SNDRV_PCM_FORMAT_S32_LE:
		/* Actually "16 bits/0 bits" means "32/28/24/20/18/16 bits
		 * on the left than zeros (if less than 32 bytes)"... ;-) */
		set__AUD_UNIPERIF_CONFIG__MEM_FMT_16_0(player);
		break;

	default:
		snd_BUG();
		return -EINVAL;
	}

	/* Set the L/R polarity */
	set__AUD_UNIPERIF_I2S_FMT__LR_POL(player, lr_pol);

	/* Set rounding to off */
	set__AUD_UNIPERIF_CTRL__ROUNDING_OFF(player);

	/* Set clock divisor */
	set__AUD_UNIPERIF_CTRL__DIVIDER(player,
			player->oversampling / (2 * bits_in_output_frame));

	/* Number of channels... */

	BUG_ON(runtime->channels % 2);
	BUG_ON(runtime->channels < 2);
	BUG_ON(runtime->channels > 10);

	set__AUD_UNIPERIF_I2S_FMT__NUM_CH(player, runtime->channels / 2);

	/* Set 1-bit audio format to disabled */
	set__AUD_UNIPERIF_CONFIG__ONE_BIT_AUD_DISABLE(player);

	set__AUD_UNIPERIF_I2S_FMT__ORDER_MSB(player);
	set__AUD_UNIPERIF_I2S_FMT__SCLK_EDGE_FALLING(player);

	/* No iec958 formatting as outputting to DAC  */
	set__AUD_UNIPERIF_CTRL__SPDIF_FMT_OFF(player);

	/* Set the underflow recovery window size */
	uniperif_player_set_underflow(player->substream);

	return 0;
}

static void uniperif_player_set_channel_status(struct uniperif_player *player,
		struct snd_pcm_runtime *runtime)
{
	int n;
	unsigned int status;

	dev_dbg(player->dev, "%s(player=%p, runtime=%p)", __func__,
			player, runtime);

	BUG_ON(!player);
	BUG_ON(!snd_stm_magic_valid(player));

	/*
	 * Some AVRs and TVs require the channel status to contain a correct
	 * sampling frequency. If no sample rate is already specified, then
	 * set one.
	 */

	if (runtime && (player->stream_settings.iec958.status[3] == 0x01)) {
		switch (runtime->rate) {
		case 22050:
			player->stream_settings.iec958.status[3] = 0x04;
			break;
		case 44100:
			player->stream_settings.iec958.status[3] = 0x00;
			break;
		case 88200:
			player->stream_settings.iec958.status[3] = 0x08;
			break;
		case 176400:
			player->stream_settings.iec958.status[3] = 0x0c;
			break;
		case 24000:
			player->stream_settings.iec958.status[3] = 0x06;
			break;
		case 48000:
			player->stream_settings.iec958.status[3] = 0x02;
			break;
		case 96000:
			player->stream_settings.iec958.status[3] = 0x0a;
			break;
		case 192000:
			player->stream_settings.iec958.status[3] = 0x0e;
			break;
		case 32000:
			player->stream_settings.iec958.status[3] = 0x03;
			break;
		case 768000:
			player->stream_settings.iec958.status[3] = 0x09;
			break;
		default:
			/* Mark as sampling frequency not indicated */
			player->stream_settings.iec958.status[3] = 0x01;
			break;
		}
	}

	/* Program the new channel status */
	for (n = 0; n < 6; ++n) {
		status  = player->stream_settings.iec958.status[0+(n*4)] & 0xf;
		status |= player->stream_settings.iec958.status[1+(n*4)] << 8;
		status |= player->stream_settings.iec958.status[2+(n*4)] << 16;
		status |= player->stream_settings.iec958.status[3+(n*4)] << 24;
		dev_dbg(player->dev, "Channel Status Regsiter %d: %08x",
				n, status);
		set__AUD_UNIPERIF_CHANNEL_STA_REGn(player, n, status);
	}

	/* Update the channel status */
	if (player->ver < SND_STM_UNIPERIF_VERSION_UNI_PLR_TOP_1_0)
		set__AUD_UNIPERIF_CONFIG__CHL_STS_UPDATE(player);
	else
		set__AUD_UNIPERIF_BIT_CONTROL__CHL_STS_UPDATE(player);
}

static int uniperif_player_prepare_iec958(struct uniperif_player *player,
		struct snd_pcm_runtime *runtime)
{
	dev_dbg(player->dev, "%s(player=%p, runtime=%p)", __func__,
			player, runtime);

	BUG_ON(!player);
	BUG_ON(!snd_stm_magic_valid(player));
	BUG_ON(!runtime);

	/* The player should be stopped when setting configuration */
	BUG_ON(player->state != UNIPERIF_PLAYER_STATE_STOPPED);

	/* Set the default format and oversampling for later */
	player->format = DEFAULT_FORMAT;
	player->oversampling = DEFAULT_OVERSAMPLING;

	/* Get format & oversampling value from connected converter */
	if (player->conv_group) {
		player->format = snd_stm_conv_get_format(player->conv_group);

		if (UNIPERIF_PLAYER_TYPE_IS_SPDIF(player))
			BUG_ON((player->format & SND_STM_FORMAT__MASK) !=
				SND_STM_FORMAT__SPDIF);

		player->oversampling =
			snd_stm_conv_get_oversampling(player->conv_group);
		if (player->oversampling == 0)
			player->oversampling = DEFAULT_OVERSAMPLING;
	}

	dev_dbg(player->dev, "Sample frequency %d (oversampling %d)",
			runtime->rate, player->oversampling);

	/* Oversampling must be multiple of 128 as iec958 frame is 32-bits */
	BUG_ON(player->oversampling <= 0);
	BUG_ON(player->oversampling % 128);

	/* No sample rates below 32kHz are supported for iec958 */
	if (runtime->rate < MIN_IEC958_SAMPLE_RATE) {
		dev_err(player->dev, "Invalid sample rate (%d)", runtime->rate);
		return -EINVAL;
	}

	if (player->stream_settings.input_mode ==
			UNIPERIF_IEC958_INPUT_MODE_PCM) {

		dev_dbg(player->dev, "Input Mode: PCM");

		switch (runtime->format) {
		case SNDRV_PCM_FORMAT_S16_LE:
			dev_dbg(player->dev, "16-bit subframe");
			/* 16/16 memory format */
			set__AUD_UNIPERIF_CONFIG__MEM_FMT_16_16(player);
			/* 16-bits per sub-frame */
			set__AUD_UNIPERIF_I2S_FMT__NBIT_32(player);
			/* Set 16-bit sample precision */
			set__AUD_UNIPERIF_I2S_FMT__DATA_SIZE_16(player);
			break;
		case SNDRV_PCM_FORMAT_S32_LE:
			dev_dbg(player->dev, "32-bit subframe");
			/* 16/0 memory format */
			set__AUD_UNIPERIF_CONFIG__MEM_FMT_16_0(player);
			/* 32-bits per sub-frame */
			set__AUD_UNIPERIF_I2S_FMT__NBIT_32(player);
			/* Set 24-bit sample precision */
			set__AUD_UNIPERIF_I2S_FMT__DATA_SIZE_24(player);
			break;
		default:
			snd_BUG();
			return -EINVAL;
		}

		/* Set parity to be calculated by the hardware */
		set__AUD_UNIPERIF_CONFIG__PARITY_CNTR_BY_HW(player);

		/* Set channel status bits to be inserted by the hardware */
		set__AUD_UNIPERIF_CONFIG__CHANNEL_STA_CNTR_BY_HW(player);

		/* Set user data bits to be inserted by the hardware */
		set__AUD_UNIPERIF_CONFIG__USER_DAT_CNTR_BY_HW(player);

		/* Set validity bits to be inserted by the hardware */
		set__AUD_UNIPERIF_CONFIG__VALIDITY_DAT_CNTR_BY_HW(player);

		/* Set full software control to disabled */
		set__AUD_UNIPERIF_CONFIG__SPDIF_SW_CTRL_DISABLE(player);

		set__AUD_UNIPERIF_CTRL__ZERO_STUFF_HW(player);

		if (player->stream_settings.encoding_mode ==
				UNIPERIF_IEC958_ENCODING_MODE_PCM) {

			dev_dbg(player->dev, "Encoding Mode: Linear PCM");

			/* Set 24-bit max word size (SDK2 should do this) */
			player->stream_settings.iec958.status[4] = 0x0b;

			/* Clear user validity bits */
			set__AUD_UNIPERIF_USER_VALIDITY__VALIDITY_LR(player, 0);
		} else {
			dev_dbg(player->dev, "Encoding Mode: Encoded");

			/* Set user validity bits */
			set__AUD_UNIPERIF_USER_VALIDITY__VALIDITY_LR(player, 1);
		}

		/* Update the channel status */
		uniperif_player_set_channel_status(player, runtime);

		/* Clear the user validity user bits */
		set__AUD_UNIPERIF_USER_VALIDITY__USER_LEFT(player, 0);
		set__AUD_UNIPERIF_USER_VALIDITY__USER_RIGHT(player, 0);
	} else {

		dev_dbg(player->dev, "Input Mode: RAW");

		/* 16/0 memory format */
		set__AUD_UNIPERIF_CONFIG__MEM_FMT_16_0(player);

		/* 32-bits per sub-frame */
		set__AUD_UNIPERIF_I2S_FMT__NBIT_32(player);

		/* Set 24-bit sample precision */
		set__AUD_UNIPERIF_I2S_FMT__DATA_SIZE_32(player);

		/* Set parity to be calculated by the hardware */
		set__AUD_UNIPERIF_CONFIG__PARITY_CNTR_BY_HW(player);

		/* Set channel status bits to be inserted by the software */
		set__AUD_UNIPERIF_CONFIG__CHANNEL_STA_CNTR_BY_SW(player);

		/* Set user data bits to be inserted by the software */
		set__AUD_UNIPERIF_CONFIG__USER_DAT_CNTR_BY_SW(player);

		/* Set validity bits to be inserted by the software */
		set__AUD_UNIPERIF_CONFIG__VALIDITY_DAT_CNTR_BY_SW(player);

		/* Set full software control to enabled */
		set__AUD_UNIPERIF_CONFIG__SPDIF_SW_CTRL_ENABLE(player);

		/* Set zero stuff by hardware to stop glitch at end of audio */
		set__AUD_UNIPERIF_CTRL__ZERO_STUFF_HW(player);
	}

	/* Disable one-bit audio mode */
	set__AUD_UNIPERIF_CONFIG__ONE_BIT_AUD_DISABLE(player);

	/* Enable consecutive frames repetition of Z preamble (not for HBRA) */
	set__AUD_UNIPERIF_CONFIG__REPEAT_CHL_STS_ENABLE(player);

	/* Change to SUF0_SUBF1 and left/right channels swap! */
	set__AUD_UNIPERIF_CONFIG__SUBFRAME_SEL_SUBF1_SUBF0(player);

	/* Set lr clock polarity and i2s mode using platform configuration */
	set__AUD_UNIPERIF_I2S_FMT__LR_POL(player, player->info->iec958_lr_pol);
	set__AUD_UNIPERIF_I2S_FMT__PADDING(player,
			player->info->iec958_i2s_mode);

	/* Set data output on rising edge */
	set__AUD_UNIPERIF_I2S_FMT__SCLK_EDGE_RISING(player);

	/* Set data aligned to left with respect to left-right clock polarity */
	set__AUD_UNIPERIF_I2S_FMT__ALIGN_LEFT(player);

	/* Set data output as MSB first */
	set__AUD_UNIPERIF_I2S_FMT__ORDER_MSB(player);

	/* Set the number of channels (maximum supported by spdif is 2) */
	if (UNIPERIF_PLAYER_TYPE_IS_SPDIF(player))
		BUG_ON(runtime->channels != 2);

	set__AUD_UNIPERIF_I2S_FMT__NUM_CH(player, runtime->channels / 2);

	/* Set rounding to off */
	set__AUD_UNIPERIF_CTRL__ROUNDING_OFF(player);

	/* Set clock divisor */
	set__AUD_UNIPERIF_CTRL__DIVIDER(player, player->oversampling / 128);

	/* Set the spdif latency to not wait before starting player */
	set__AUD_UNIPERIF_CTRL__SPDIF_LAT_OFF(player);

	/*
	 * Ensure iec958 formatting is off. It will be enabled in function
	 * uniperif_player_start() at the same time as the operation
	 * mode is set to work around a silicon issue.
	 */
	if (player->ver < SND_STM_UNIPERIF_VERSION_UNI_PLR_TOP_1_0)
		set__AUD_UNIPERIF_CTRL__SPDIF_FMT_OFF(player);
	else
		set__AUD_UNIPERIF_CTRL__SPDIF_FMT_ON(player);

	/* Set the underflow recovery window size */
	uniperif_player_set_underflow(player->substream);

	return 0;
}

static int uniperif_player_prepare(struct snd_pcm_substream *substream)
{
	struct uniperif_player *player = snd_pcm_substream_chip(substream);
	struct snd_pcm_runtime *runtime = substream->runtime;
	int changed;

	dev_dbg(player->dev, "%s(substream=%p)", __func__, substream);

	BUG_ON(!player);
	BUG_ON(!snd_stm_magic_valid(player));
	BUG_ON(!runtime);
	BUG_ON(runtime->period_size * runtime->channels >=
			MAX_SAMPLES_PER_PERIOD);

	/* The player should not be in started state */
	BUG_ON(player->state == UNIPERIF_PLAYER_STATE_STARTED);

	/* We cannot prepare in underflow state */
	if (player->state == UNIPERIF_PLAYER_STATE_UNDERFLOW) {
		dev_err(player->dev, "Cannot prepare in underflow");
		return -EAGAIN;
	}

	/* Determine if output configuration has changed */
	changed  = (player->current_rate != runtime->rate);
	changed |= (player->current_format != runtime->format);
	changed |= (player->current_channels != runtime->channels);

	if (changed) {
		/* Change of configuration requires player to be stopped */
		if (player->state != UNIPERIF_PLAYER_STATE_STOPPED)
			uniperif_player_stop(substream);

		/* Player should now be stopped */
		BUG_ON(player->state != UNIPERIF_PLAYER_STATE_STOPPED);

		/* Store new configuration */
		player->current_rate = runtime->rate;
		player->current_format = runtime->format;
		player->current_channels = runtime->channels;

		/* Uniperipheral setup is dependent on player type */
		switch (player->info->player_type) {
		case SND_STM_UNIPERIF_PLAYER_TYPE_HDMI:
			return uniperif_player_prepare_iec958(player, runtime);
		case SND_STM_UNIPERIF_PLAYER_TYPE_PCM:
			return uniperif_player_prepare_pcm(player, runtime);
		case SND_STM_UNIPERIF_PLAYER_TYPE_SPDIF:
			return uniperif_player_prepare_iec958(player, runtime);
		default:
			snd_BUG();
			return -EINVAL;
		}
	}

	return 0;
}

static int uniperif_player_start(struct snd_pcm_substream *substream)
{
	struct uniperif_player *player = snd_pcm_substream_chip(substream);
	unsigned long irqflags;
	int result;

	dev_dbg(player->dev, "%s(substream=%p)", __func__, substream);

	BUG_ON(!player);
	BUG_ON(!snd_stm_magic_valid(player));

	/* The player should not be in started or underflow */
	BUG_ON(player->state == UNIPERIF_PLAYER_STATE_STARTED);
	BUG_ON(player->state == UNIPERIF_PLAYER_STATE_UNDERFLOW);

	/* Request the dma and converter group resources */
	result = uniperif_player_resources_request(player);
	if (result) {
		dev_err(player->dev, "Failed to request resources");
		return result;
	}

	/* Prepare the dma descriptor */
	player->dma_descriptor = dma_audio_prep_tx_cyclic(player->dma_channel,
			substream->runtime->dma_addr, player->buffer_bytes,
			player->period_bytes);
	if (!player->dma_descriptor) {
		dev_err(player->dev, "Failed to prepare DMA descriptor");
		return -ENOMEM;
	}

	/* Set the dma callback */
	player->dma_descriptor->callback = uniperif_player_comp_cb;
	player->dma_descriptor->callback_param = player;

	/* Check if we are moving from parking state to started */
	if (player->state == UNIPERIF_PLAYER_STATE_PARKING) {
		/* Submit dma descriptor */
		player->dma_cookie = dmaengine_submit(player->dma_descriptor);

		/* Update state to started and return */
		player->state = UNIPERIF_PLAYER_STATE_STARTED;
		return 0;
	}

	/* Check if we are moving from standby state to started */
	if (player->state == UNIPERIF_PLAYER_STATE_STANDBY) {
		/* Read the control register */
		unsigned int ctrl = get__AUD_UNIPERIF_CTRL(player);

		/* Clear the operation and exit standby bits */
		ctrl &= ~(mask__AUD_UNIPERIF_CTRL__OPERATION(player) <<
			shift__AUD_UNIPERIF_CTRL__OPERATION(player));
		ctrl &= ~(mask__AUD_UNIPERIF_CTRL__EXIT_STBY_ON_EOBLOCK(player)
			<< shift__AUD_UNIPERIF_CTRL__EXIT_STBY_ON_EOBLOCK(player));

		/* Set the exit standby on end of block if required */
		if (UNIPERIF_PLAYER_TYPE_IS_IEC958(player))
			if (player->stream_settings.encoding_mode ==
				UNIPERIF_IEC958_ENCODING_MODE_ENCODED)
				ctrl |= 1 << shift__AUD_UNIPERIF_CTRL__EXIT_STBY_ON_EOBLOCK(player);

		/* Set the player to audio/pcm data mode */
		ctrl |= value__AUD_UNIPERIF_CTRL__OPERATION_AUDIO_DATA(player)
			<< shift__AUD_UNIPERIF_CTRL__OPERATION(player);

		/* Submit dma descriptor */
		player->dma_cookie = dmaengine_submit(player->dma_descriptor);

		/* Set all control bits simultaneously (avoids ip bug) */
		set__AUD_UNIPERIF_CTRL(player, ctrl);



		/* Set the interrupt mask */
		set__AUD_UNIPERIF_ITM_BSET__DMA_ERROR(player);
		set__AUD_UNIPERIF_ITM_BSET__FIFO_ERROR(player);

		/* Enable underflow recovery interrupts */
		if (player->info->underflow_enabled) {
			set__AUD_UNIPERIF_ITM_BSET__UNDERFLOW_REC_DONE(player);
			set__AUD_UNIPERIF_ITM_BSET__UNDERFLOW_REC_FAILED(player);
		}

		/* Clear any pending interrupts */
		set__AUD_UNIPERIF_ITS_BCLR(player, get__AUD_UNIPERIF_ITS(player));

		/* Enable player interrupts */
		enable_irq(player->irq);

		/* Update state to started and return */
		player->state = UNIPERIF_PLAYER_STATE_STARTED;
		return 0;
	}

	/* We are moving from stopped state to started */
	BUG_ON(player->state != UNIPERIF_PLAYER_STATE_STOPPED);

	/* Enable clock */
	result = snd_stm_clk_enable(player->clock);
	if (result) {
		dev_err(player->dev, "Failed to enable clock");
		return result;
	}

	/* Set clock rate */
	result = snd_stm_clk_set_rate(player->clock,
		substream->runtime->rate * player->oversampling);
	if (result) {
		dev_err(player->dev, "Failed to set clock rate");
		snd_stm_clk_disable(player->clock);
		return result;
	}

	/* Enable player interrupts */
	enable_irq(player->irq);

	/* Clear any pending interrupts */
	set__AUD_UNIPERIF_ITS_BCLR(player, get__AUD_UNIPERIF_ITS(player));

	/* Set the interrupt mask */
	set__AUD_UNIPERIF_ITM_BSET__DMA_ERROR(player);
	set__AUD_UNIPERIF_ITM_BSET__FIFO_ERROR(player);

	/* Enable underflow recovery interrupts */
	if (player->info->underflow_enabled) {
		set__AUD_UNIPERIF_ITM_BSET__UNDERFLOW_REC_DONE(player);
		set__AUD_UNIPERIF_ITM_BSET__UNDERFLOW_REC_FAILED(player);
	}

	/* Reset uniperipheral player */
	set__AUD_UNIPERIF_SOFT_RST__SOFT_RST(player);
	if (player->ver < SND_STM_UNIPERIF_VERSION_UNI_PLR_TOP_1_0)
		while (get__AUD_UNIPERIF_SOFT_RST__SOFT_RST(player))
			udelay(5);

	/* Submit dma descriptor */
	player->dma_cookie = dmaengine_submit(player->dma_descriptor);

	/* Reset uniperipheral player */
	set__AUD_UNIPERIF_SOFT_RST__SOFT_RST(player);
	if (player->ver < SND_STM_UNIPERIF_VERSION_UNI_PLR_TOP_1_0)
		while (get__AUD_UNIPERIF_SOFT_RST__SOFT_RST(player))
			udelay(5);

	/*
	 * SDK2 does not use IEC61937 features of the uniperipheral hardware.
	 * Instead it performs IEC61937 in software and inserts it directly
	 * into the audio data stream. As such, when encoded mode is selected,
	 * linear pcm mode is still used, but with the differences of the
	 * channel status bits set for encoded mode and the validity bits set.
	 */

	spin_lock_irqsave(&player->default_settings_lock, irqflags);

	set__AUD_UNIPERIF_CTRL__OPERATION_PCM_DATA(player);

	/*
	 * If iec958 formatting is required for hdmi or spdif, then it must be
	 * enabled after the operation mode is set. If set prior to this, it
	 * will not take affect and hang the player.
	 */

	if (player->ver < SND_STM_UNIPERIF_VERSION_UNI_PLR_TOP_1_0)
		if (UNIPERIF_PLAYER_TYPE_IS_IEC958(player))
			set__AUD_UNIPERIF_CTRL__SPDIF_FMT_ON(player);

	spin_unlock_irqrestore(&player->default_settings_lock, irqflags);

	/* Update state to started */
	player->state = UNIPERIF_PLAYER_STATE_STARTED;

	/* Wake up & unmute converter */
	if (player->conv_group) {
		snd_stm_conv_enable(player->conv_group,
				0, substream->runtime->channels - 1);
		snd_stm_conv_unmute(player->conv_group);
	}

	return 0;
}

static int uniperif_player_stop(struct snd_pcm_substream *substream)
{
	struct uniperif_player *player = snd_pcm_substream_chip(substream);

	dev_dbg(player->dev, "%s(substream=%p)", __func__, substream);

	BUG_ON(!player);
	BUG_ON(!snd_stm_magic_valid(player));

	/* The player should not be in stopped state */
	BUG_ON(player->state == UNIPERIF_PLAYER_STATE_STOPPED);

	/* Mute & shutdown converter */
	if (player->conv_group) {
		snd_stm_conv_mute(player->conv_group);
		snd_stm_conv_disable(player->conv_group);
	}

	/* Turn the player off */
	set__AUD_UNIPERIF_CTRL__OPERATION_OFF(player);

	/* Soft reset the player */
	set__AUD_UNIPERIF_SOFT_RST__SOFT_RST(player);
	if (player->ver < SND_STM_UNIPERIF_VERSION_UNI_PLR_TOP_1_0)
		while (get__AUD_UNIPERIF_SOFT_RST__SOFT_RST(player))
			udelay(5);

	/* Disable interrupts */
	set__AUD_UNIPERIF_ITM_BCLR(player, get__AUD_UNIPERIF_ITM(player));
	disable_irq_nosync(player->irq);

	/* Disable the clock */
	snd_stm_clk_disable(player->clock);

	if (player->dma_channel) {
		/* Terminate any dma transfers */
		dmaengine_terminate_all(player->dma_channel);
	}

	/* Reset current configuration */
	player->current_rate = 0;
	player->current_format = 0;
	player->current_channels = 0;

	/* Update state to stopped and return */
	player->state = UNIPERIF_PLAYER_STATE_STOPPED;
	return 0;
}

static int uniperif_player_standby(struct snd_pcm_substream *substream)
{
	struct uniperif_player *player = snd_pcm_substream_chip(substream);
	int result;

	dev_dbg(player->dev, "%s(substream=%p)", __func__, substream);

	BUG_ON(!player);
	BUG_ON(!snd_stm_magic_valid(player));

	/* The player should not be in stopped, parking or standby */
	BUG_ON(player->state == UNIPERIF_PLAYER_STATE_STOPPED);
	BUG_ON(player->state == UNIPERIF_PLAYER_STATE_PARKING);
	BUG_ON(player->state == UNIPERIF_PLAYER_STATE_STANDBY);

	if (player->state == UNIPERIF_PLAYER_STATE_STARTED) {
		/* Check if we should enable parking mode */
		if (player->info->parking_enabled == 1) {
			dev_dbg(player->dev, "Entering parking mode");

			/* Configure parking parameters */
			result = dma_audio_parking_config(player->dma_channel,
					&player->dma_park_config);
			if (result) {
				dev_err(player->dev, "Failed to conf parking");
				goto normal_stop;
			}

			/* Enable parking */
			result = dma_audio_parking_enable(player->dma_channel);
			if (result) {
				dev_err(player->dev, "Failed to start parking");
				goto normal_stop;
			}

			/* Update state to parking and return */
			player->state = UNIPERIF_PLAYER_STATE_PARKING;
			return 0;
		}

		/* Check if we should enable standby mode */
		if (player->info->standby_enabled == 1) {
			dev_dbg(player->dev, "Entering standby mode");

			/* Disable interrupts */
			set__AUD_UNIPERIF_ITM_BCLR(player,
						get__AUD_UNIPERIF_ITM(player));
			disable_irq_nosync(player->irq);

			/* Set standby mode */
			set__AUD_UNIPERIF_CTRL__OPERATION_STANDBY(player);

			/* Stop the dma */
			dmaengine_terminate_all(player->dma_channel);

			/* Update state to standby and return */
			player->state = UNIPERIF_PLAYER_STATE_STANDBY;
			return 0;
		}
	}

normal_stop:
	/* Actually stop the uniperipheral player */
	return uniperif_player_stop(substream);
}

static int uniperif_player_trigger(struct snd_pcm_substream *substream,
		int command)
{
	struct uniperif_player *player = snd_pcm_substream_chip(substream);

	dev_dbg(player->dev, "%s(substream=%p)", __func__, substream);

	switch (command) {
	case SNDRV_PCM_TRIGGER_START:
		return uniperif_player_start(substream);
	case SNDRV_PCM_TRIGGER_RESUME:
		return uniperif_player_start(substream);
	case SNDRV_PCM_TRIGGER_STOP:
		return uniperif_player_standby(substream);
	case SNDRV_PCM_TRIGGER_SUSPEND:
		return uniperif_player_stop(substream);
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
		return uniperif_player_standby(substream);
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		return uniperif_player_start(substream);
	default:
		return -EINVAL;
	}
}

static snd_pcm_uframes_t uniperif_player_pointer(
		struct snd_pcm_substream *substream)
{
	struct uniperif_player *player = snd_pcm_substream_chip(substream);
	struct snd_pcm_runtime *runtime = substream->runtime;
	int residue, hwptr = 0;

	BUG_ON(!player);
	BUG_ON(!snd_stm_magic_valid(player));
	BUG_ON(!runtime);

	/* Pause/resume is not supported when parking active */
	if (player->state != UNIPERIF_PLAYER_STATE_PARKING) {
		struct dma_tx_state state;
		enum dma_status status;

		status = player->dma_channel->device->device_tx_status(
				player->dma_channel,
				player->dma_cookie, &state);

		residue = state.residue;
		hwptr = (runtime->dma_bytes - residue) % runtime->dma_bytes;

		trace_printk("status:0x%x residue:%d hwptr:%d\n", status, residue, hwptr); 
	}

	return bytes_to_frames(runtime, hwptr);
}

static int uniperif_player_copy(struct snd_pcm_substream *substream,
		int channel, snd_pcm_uframes_t pos,
		void __user *src, snd_pcm_uframes_t count)
{
	struct uniperif_player *player = snd_pcm_substream_chip(substream);
	struct snd_pcm_runtime *runtime = substream->runtime;
	void *dst;
	int result;
	int i;

	BUG_ON(!player);
	BUG_ON(!snd_stm_magic_valid(player));
	BUG_ON(channel != -1);
	BUG_ON(!runtime);

	/* Get a pointer to the dma area to copy the user space data to */
	dst = runtime->dma_area + frames_to_bytes(runtime, pos);

	/* Perform a normal copy from user */
	result = copy_from_user(dst, src, frames_to_bytes(runtime, count));
	BUG_ON(result);

	/* Check if we are a 16-bit format */
	if (runtime->format == SNDRV_PCM_FORMAT_S16_LE) {
		/* Check if we should swap left and right channels */
		if (player->info->s16_swap_lr) {
			short *dst16 = dst;
			short tmp;

			/* Swap channels */
			for (i = 0; i < count; ++i) {
				tmp = dst16[2 * i];
				dst16[2 * i] = dst16[2 * i + 1];
				dst16[2 * i + 1] = tmp;
			}
		}
	}

	return 0;
}

static struct snd_pcm_ops uniperif_player_pcm_ops = {
	.open =      uniperif_player_open,
	.close =     uniperif_player_close,
	.mmap =      snd_stm_buffer_mmap,
	.ioctl =     snd_pcm_lib_ioctl,
	.hw_params = uniperif_player_hw_params,
	.hw_free =   uniperif_player_hw_free,
	.prepare =   uniperif_player_prepare,
	.trigger =   uniperif_player_trigger,
	.pointer =   uniperif_player_pointer,
	.copy =      uniperif_player_copy,
};

/*
 * ALSA parking controls
 */

static int uniperif_player_parking_ctl_info(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_info *uinfo)
{
	uinfo->type = SNDRV_CTL_ELEM_TYPE_BOOLEAN;
	uinfo->count = 1;
	uinfo->value.integer.min = 0;
	uinfo->value.integer.max = 1;

	return 0;
}

static int uniperif_player_parking_ctl_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct uniperif_player *player = snd_kcontrol_chip(kcontrol);

	dev_dbg(player->dev, "%s(kcontrol=%p, ucontrol=%p)", __func__,
			kcontrol, ucontrol);

	BUG_ON(!player);
	BUG_ON(!snd_stm_magic_valid(player));

	/* Just return if the current parking enabled status */
	ucontrol->value.integer.value[0] = player->info->parking_enabled;

	return 0;
}

static int uniperif_player_parking_ctl_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct uniperif_player *player = snd_kcontrol_chip(kcontrol);

	dev_dbg(player->dev, "%s(kcontrol=%p, ucontrol=%p)", __func__,
			kcontrol, ucontrol);

	BUG_ON(!player);
	BUG_ON(!snd_stm_magic_valid(player));

	/* Get the new parking enabled value */
	player->info->parking_enabled = ucontrol->value.integer.value[0];

	/* Stop the player when parking transitions from enabled to disabled */
	if (player->state == UNIPERIF_PLAYER_STATE_PARKING)
		if (!player->info->parking_enabled)
			uniperif_player_stop(player->substream);

	return 0;
}

static struct snd_kcontrol_new uniperif_player_parking_ctls[] = {
	{
		.iface = SNDRV_CTL_ELEM_IFACE_PCM,
		.name = "Parking Control",
		.info = uniperif_player_parking_ctl_info,
		.get = uniperif_player_parking_ctl_get,
		.put = uniperif_player_parking_ctl_put,
	},
};

/*
 * ALSA standby controls
 */

static int uniperif_player_standby_ctl_info(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_info *uinfo)
{
	uinfo->type = SNDRV_CTL_ELEM_TYPE_BOOLEAN;
	uinfo->count = 1;
	uinfo->value.integer.min = 0;
	uinfo->value.integer.max = 1;

	return 0;
}

static int uniperif_player_standby_ctl_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct uniperif_player *player = snd_kcontrol_chip(kcontrol);

	dev_dbg(player->dev, "%s(kcontrol=%p, ucontrol=%p)", __func__,
			kcontrol, ucontrol);

	BUG_ON(!player);
	BUG_ON(!snd_stm_magic_valid(player));

	/* Just return if the current standby enabled status */
	ucontrol->value.integer.value[0] = player->info->standby_enabled;

	return 0;
}

static int uniperif_player_standby_ctl_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct uniperif_player *player = snd_kcontrol_chip(kcontrol);

	dev_dbg(player->dev, "%s(kcontrol=%p, ucontrol=%p)", __func__,
			kcontrol, ucontrol);

	BUG_ON(!player);
	BUG_ON(!snd_stm_magic_valid(player));

	/* Update if value has changed */
	if (player->info->standby_enabled != ucontrol->value.integer.value[0]) {
		/* Set the new control value */
		player->info->standby_enabled =
			ucontrol->value.integer.value[0];

		/* If standby is now disabled and we are in standby, stop */
		if (!player->info->standby_enabled)
			if (player->state == UNIPERIF_PLAYER_STATE_STANDBY)
				uniperif_player_stop(player->substream);

		return 1;
	}

	return 0;
}

static struct snd_kcontrol_new uniperif_player_standby_ctls[] = {
	{
		.iface = SNDRV_CTL_ELEM_IFACE_PCM,
		.name = "Standby Control",
		.info = uniperif_player_standby_ctl_info,
		.get = uniperif_player_standby_ctl_get,
		.put = uniperif_player_standby_ctl_put,
	},
};


/*
 * ALSA underflow controls
 */

static int uniperif_player_underflow_ctl_info(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_info *uinfo)
{
	uinfo->type = SNDRV_CTL_ELEM_TYPE_BOOLEAN;
	uinfo->count = 1;
	uinfo->value.integer.min = 0;
	uinfo->value.integer.max = 1;

	return 0;
}

static int uniperif_player_underflow_ctl_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct uniperif_player *player = snd_kcontrol_chip(kcontrol);

	dev_dbg(player->dev, "%s(kcontrol=%p, ucontrol=%p)", __func__,
			kcontrol, ucontrol);

	BUG_ON(!player);
	BUG_ON(!snd_stm_magic_valid(player));

	/* Just return the current underflow enabled status */
	ucontrol->value.integer.value[0] = player->info->underflow_enabled;

	return 0;
}

static int uniperif_player_underflow_ctl_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct uniperif_player *player = snd_kcontrol_chip(kcontrol);
	int old_underflow_enabled;

	dev_dbg(player->dev, "%s(kcontrol=%p, ucontrol=%p)", __func__,
			kcontrol, ucontrol);

	BUG_ON(!player);
	BUG_ON(!snd_stm_magic_valid(player));

	/* Save the old underflow enabled value */
	old_underflow_enabled = player->info->underflow_enabled;

	/* Get the new value */
	player->info->underflow_enabled = ucontrol->value.integer.value[0];

	/* Update if value has changed */
	if (old_underflow_enabled != player->info->underflow_enabled) {
		/* Update the player underflow window */
		if (player->substream)
			uniperif_player_set_underflow(player->substream);

		return 1;
	}

	return 0;
}

static int uniperif_player_underflow_us_ctl_info(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_info *uinfo)
{
	uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
	uinfo->count = 1;
	uinfo->value.integer.min = 0;
	uinfo->value.integer.max = 10000;

	return 0;
}

static int uniperif_player_underflow_us_ctl_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct uniperif_player *player = snd_kcontrol_chip(kcontrol);

	dev_dbg(player->dev, "%s(kcontrol=%p, ucontrol=%p)", __func__,
			kcontrol, ucontrol);

	BUG_ON(!player);
	BUG_ON(!snd_stm_magic_valid(player));

	/* Just return the current underflow window size in us */
	ucontrol->value.integer.value[0] = player->underflow_us;

	return 0;
}

static int uniperif_player_underflow_us_ctl_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct uniperif_player *player = snd_kcontrol_chip(kcontrol);

	dev_dbg(player->dev, "%s(kcontrol=%p, ucontrol=%p)", __func__,
			kcontrol, ucontrol);

	BUG_ON(!player);
	BUG_ON(!snd_stm_magic_valid(player));

	BUG_ON(ucontrol->value.integer.value[0] < 0);
	BUG_ON(ucontrol->value.integer.value[0] > 10000);

	/* Only process changes in value */
	if (ucontrol->value.integer.value[0] != player->underflow_us) {
		/* Set the new underflow window size in us */
		player->underflow_us = ucontrol->value.integer.value[0];

		/* Update the player underflow window */
		if (player->substream)
			uniperif_player_set_underflow(player->substream);

		return 1;
	}

	return 0;
}

static struct snd_kcontrol_new uniperif_player_underflow_ctls[] = {
	{
		.iface = SNDRV_CTL_ELEM_IFACE_PCM,
		.name = "Underflow Recovery Control",
		.info = uniperif_player_underflow_ctl_info,
		.get = uniperif_player_underflow_ctl_get,
		.put = uniperif_player_underflow_ctl_put,
	},
	{
		.iface = SNDRV_CTL_ELEM_IFACE_PCM,
		.name = "Underflow Recovery Window Adjustment",
		.info = uniperif_player_underflow_us_ctl_info,
		.get = uniperif_player_underflow_us_ctl_get,
		.put = uniperif_player_underflow_us_ctl_put,
	},
};

/*
 * ALSA uniperipheral iec958 controls
 */

static int uniperif_player_ctl_iec958_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct uniperif_player *player = snd_kcontrol_chip(kcontrol);

	dev_dbg(player->dev, "%s(kcontrol=%p, ucontrol=%p)", __func__,
			kcontrol, ucontrol);

	BUG_ON(!player);
	BUG_ON(!snd_stm_magic_valid(player));

	spin_lock(&player->default_settings_lock);
	ucontrol->value.iec958 = player->stream_settings.iec958;
	spin_unlock(&player->default_settings_lock);

	return 0;
}

static int uniperif_player_ctl_iec958_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct uniperif_player *player = snd_kcontrol_chip(kcontrol);
	int changed = 0;

	dev_dbg(player->dev, "%s(kcontrol=%p, ucontrol=%p)", __func__,
			kcontrol, ucontrol);

	BUG_ON(!player);
	BUG_ON(!snd_stm_magic_valid(player));

	spin_lock(&player->default_settings_lock);

	/* If user settings differ from the default, update default */
	if (snd_stm_iec958_cmp(&player->default_settings.iec958,
				&ucontrol->value.iec958) != 0) {
		player->default_settings.iec958 = ucontrol->value.iec958;
		changed = 1;
	}

	/* If user settings differ from the current, update current */
	if (snd_stm_iec958_cmp(&player->stream_settings.iec958,
				&ucontrol->value.iec958) != 0) {
		player->stream_settings.iec958 = ucontrol->value.iec958;
		changed = 1;

		/*
		 * Past SDK2 releases fail to set the channel status max word
		 * size to 24-bit for linear pcm mode. It is however correctly
		 * set for encoded mode. We check for the max word length being
		 * zero, and if so, assume the channel status is for linear pcm
		 * mode and set the max word length to 24-bit.
		 */

		if (player->stream_settings.iec958.status[4] == 0)
			player->stream_settings.iec958.status[4] = 0x0b;
	}

	/* If settings changed and uniperipheral in operation, update */
	if (changed)
		uniperif_player_set_channel_status(player, NULL);

	spin_unlock(&player->default_settings_lock);

	return changed;
}

/* "Raw Data" switch controls data input mode - "RAW" means that played
 * data are already properly formated (VUC bits); in "PCM" mode
 * this data will be added by driver according to setting passed in\
 * following controls So that play things in PCM mode*/

static int uniperif_player_ctl_raw_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct uniperif_player *player = snd_kcontrol_chip(kcontrol);

	dev_dbg(player->dev, "%s(kcontrol=%p, ucontrol=%p)", __func__,
			kcontrol, ucontrol);

	BUG_ON(!player);
	BUG_ON(!snd_stm_magic_valid(player));

	spin_lock(&player->default_settings_lock);
	ucontrol->value.integer.value[0] =
			(player->default_settings.input_mode ==
			UNIPERIF_IEC958_INPUT_MODE_RAW);
	spin_unlock(&player->default_settings_lock);

	return 0;
}

static int uniperif_player_ctl_raw_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct uniperif_player *player = snd_kcontrol_chip(kcontrol);
	int changed = 0;
	struct snd_pcm_hardware hardware;
	enum uniperif_iec958_input_mode input_mode;

	dev_dbg(player->dev, "%s(kcontrol=%p, ucontrol=%p)", __func__,
			kcontrol, ucontrol);

	BUG_ON(!player);
	BUG_ON(!snd_stm_magic_valid(player));

	if (ucontrol->value.integer.value[0]) {
		hardware = uniperif_player_raw_hw;
		input_mode = UNIPERIF_IEC958_INPUT_MODE_RAW;
	} else {
		hardware = uniperif_player_pcm_hw;
		input_mode = UNIPERIF_IEC958_INPUT_MODE_PCM;
	}

	spin_lock(&player->default_settings_lock);
	changed = (input_mode != player->default_settings.input_mode);
	player->hardware = hardware;
	player->default_settings.input_mode = input_mode;
	spin_unlock(&player->default_settings_lock);

	return changed;
}

/*
 * The 'Encoded Data' switch selects between linear pcm mode and encoded mode.
 * Encoded mode does not use the hardware IEC61937 features as they are buggy,
 * and have been removed from later IP revisions. Instead IEC61937 is handled
 * in software (by SDK2). This means the only real difference between linear
 * pcm mode and encoded mode is channel status and validity bit settings...
 */

static int uniperif_player_ctl_encoded_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct uniperif_player *player = snd_kcontrol_chip(kcontrol);

	dev_dbg(player->dev, "%s(kcontrol=%p, ucontrol=%p)", __func__,
			kcontrol, ucontrol);

	BUG_ON(!player);
	BUG_ON(!snd_stm_magic_valid(player));

	spin_lock(&player->default_settings_lock);
	ucontrol->value.integer.value[0] =
			(player->default_settings.encoding_mode ==
			UNIPERIF_IEC958_ENCODING_MODE_ENCODED);
	spin_unlock(&player->default_settings_lock);

	return 0;
}

static int uniperif_player_ctl_encoded_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct uniperif_player *player = snd_kcontrol_chip(kcontrol);
	int changed = 0;
	enum uniperif_iec958_encoding_mode encoding_mode;

	dev_dbg(player->dev, "%s(kcontrol=%p, ucontrol=%p)", __func__,
			kcontrol, ucontrol);

	BUG_ON(!player);
	BUG_ON(!snd_stm_magic_valid(player));

	if (ucontrol->value.integer.value[0])
		encoding_mode = UNIPERIF_IEC958_ENCODING_MODE_ENCODED;
	else
		encoding_mode = UNIPERIF_IEC958_ENCODING_MODE_PCM;

	spin_lock(&player->default_settings_lock);

	/* If user settings differ from the default, update default */
	if (encoding_mode != player->default_settings.encoding_mode) {
		player->default_settings.encoding_mode = encoding_mode;
		changed = 1;
	}

	/* If user settings differ from the current, update current */
	if (encoding_mode != player->stream_settings.encoding_mode) {
		player->stream_settings.encoding_mode = encoding_mode;
		changed = 1;
	}

	/* If settings changed and uniperipheral in operation, update */
	if (changed) {
		set__AUD_UNIPERIF_USER_VALIDITY__VALIDITY_LR(player,
				ucontrol->value.integer.value[0]);
	}

	spin_unlock(&player->default_settings_lock);

	return changed;
}

static struct snd_kcontrol_new uniperif_player_iec958_ctls[] = {
	{
		.iface = SNDRV_CTL_ELEM_IFACE_PCM,
		.name = SNDRV_CTL_NAME_IEC958("", PLAYBACK, DEFAULT),
		.info = snd_stm_ctl_iec958_info,
		.get = uniperif_player_ctl_iec958_get,
		.put = uniperif_player_ctl_iec958_put,
	}, {
		.access = SNDRV_CTL_ELEM_ACCESS_READ,
		.iface = SNDRV_CTL_ELEM_IFACE_MIXER,
		.name =	SNDRV_CTL_NAME_IEC958("", PLAYBACK, CON_MASK),
		.info =	snd_stm_ctl_iec958_info,
		.get = snd_stm_ctl_iec958_mask_get_con,
	}, {
		.access = SNDRV_CTL_ELEM_ACCESS_READ,
		.iface = SNDRV_CTL_ELEM_IFACE_MIXER,
		.name =	SNDRV_CTL_NAME_IEC958("", PLAYBACK, PRO_MASK),
		.info =	snd_stm_ctl_iec958_info,
		.get = snd_stm_ctl_iec958_mask_get_pro,
	}, {
		.iface = SNDRV_CTL_ELEM_IFACE_PCM,
		.name = SNDRV_CTL_NAME_IEC958("Raw Data ", PLAYBACK, DEFAULT),
		.info = snd_stm_ctl_boolean_info,
		.get = uniperif_player_ctl_raw_get,
		.put = uniperif_player_ctl_raw_put,
	}, {
		.iface = SNDRV_CTL_ELEM_IFACE_PCM,
		.name = SNDRV_CTL_NAME_IEC958("Encoded Data ",
				PLAYBACK, DEFAULT),
		.info = snd_stm_ctl_boolean_info,
		.get = uniperif_player_ctl_encoded_get,
		.put = uniperif_player_ctl_encoded_put,
	}
};

/*
 * ALSA lowlevel device implementation
 */

#define DUMP_REGISTER(r) \
		snd_iprintf(buffer, "AUD_UNIPERIF_%-23s (offset 0x%04x) = " \
				"0x%08x\n", __stringify(r), \
				offset__AUD_UNIPERIF_##r(player), \
				get__AUD_UNIPERIF_##r(player))

static void uniperif_player_dump_registers(struct snd_info_entry *entry,
		struct snd_info_buffer *buffer)
{
	struct uniperif_player *player = entry->private_data;
	char *state[] = {
		"STOPPED",
		"STARTED",
		"PARKING",
		"STANDBY",
		"UNDERFLOW",
	};

	BUG_ON(!player);
	BUG_ON(!snd_stm_magic_valid(player));

	snd_iprintf(buffer, "--- %s (0x%p) %s ---\n", dev_name(player->dev),
		    player->base, state[player->state]);

	DUMP_REGISTER(SOFT_RST);
	/*DUMP_REGISTER(FIFO_DATA);*/ /* Register is write-only */
	DUMP_REGISTER(STA);
	DUMP_REGISTER(ITS);
	/*DUMP_REGISTER(ITS_BCLR);*/  /* Register is write-only */
	/*DUMP_REGISTER(ITS_BSET);*/  /* Register is write-only */
	DUMP_REGISTER(ITM);
	/*DUMP_REGISTER(ITM_BCLR);*/  /* Register is write-only */
	/*DUMP_REGISTER(ITM_BSET);*/  /* Register is write-only */

	if (player->ver < SND_STM_UNIPERIF_VERSION_UNI_PLR_TOP_1_0) {
		DUMP_REGISTER(SPDIF_PA_PB);
		DUMP_REGISTER(SPDIF_PC_PD);
		DUMP_REGISTER(SPDIF_PAUSE_LAT);
		DUMP_REGISTER(SPDIF_FRAMELEN_BURST);
	}

	DUMP_REGISTER(CONFIG);
	DUMP_REGISTER(CTRL);
	DUMP_REGISTER(I2S_FMT);
	DUMP_REGISTER(STATUS_1);
	/*DUMP_REGISTER(BIT_CONTROL);*/  /* Register is write-only */
	DUMP_REGISTER(CHANNEL_STA_REG0);
	DUMP_REGISTER(CHANNEL_STA_REG1);
	DUMP_REGISTER(CHANNEL_STA_REG2);
	DUMP_REGISTER(CHANNEL_STA_REG3);
	DUMP_REGISTER(CHANNEL_STA_REG4);
	DUMP_REGISTER(CHANNEL_STA_REG5);
	DUMP_REGISTER(USER_VALIDITY);
	DUMP_REGISTER(DFV0);
	DUMP_REGISTER(CONTROLABILITY);
	DUMP_REGISTER(CRC_CTRL);
	DUMP_REGISTER(CRC_WINDOW);
	DUMP_REGISTER(CRC_VALUE_IN);
	DUMP_REGISTER(CRC_VALUE_OUT);

	snd_iprintf(buffer, "\n");
}

static int uniperif_player_add_ctls(struct snd_device *snd_device,
		struct snd_kcontrol_new *ctls, int num_ctls)
{
	struct uniperif_player *player = snd_device->device_data;
	struct snd_kcontrol *new_ctl;
	int result;
	int i;

	dev_dbg(player->dev, "%s(snd_device=%p, ctls=%p, num_ctls=%d)",
			__func__, snd_device, ctls, num_ctls);

	BUG_ON(!player);
	BUG_ON(!snd_stm_magic_valid(player));

	for (i = 0; i < num_ctls; ++i) {
		/* Set the card device the control */
		ctls[i].device = player->info->card_device;

		/* Create a new control from template */
		new_ctl = snd_ctl_new1(&ctls[i], player);
		if (!new_ctl) {
			dev_err(player->dev, "Failed to create control");
			return -ENOMEM;
		}

		/* Add the control to the sound card */
		result = snd_ctl_add(snd_device->card, new_ctl);
		if (result < 0) {
			dev_err(player->dev, "Failed to add control");
			return result;
		}

		/* Increment control template index */
		ctls[i].index++;
	}

	return 0;
}

static int uniperif_player_register(struct snd_device *snd_device)
{
	struct uniperif_player *player = snd_device->device_data;
	int result;

	dev_dbg(player->dev, "%s(snd_device=%p)", __func__, snd_device);

	BUG_ON(!player);
	BUG_ON(!snd_stm_magic_valid(player));

	/* TODO: well, hardcoded - shall anyone use it?
	 * And what it actually means? */
       /* In uniperif doc use of backstalling is avoided*/
	set__AUD_UNIPERIF_CONFIG__BACK_STALL_REQ_DISABLE(player);
	set__AUD_UNIPERIF_CTRL__ROUNDING_OFF(player);
	set__AUD_UNIPERIF_CTRL__SPDIF_LAT_OFF(player);
	set__AUD_UNIPERIF_CONFIG__IDLE_MOD_DISABLE(player);

	/* Get frequency synthesizer channel */

	player->clock = snd_stm_clk_get(player->dev, player->clk_name,
			snd_device->card, player->info->card_device);
	if (!player->clock || IS_ERR(player->clock)) {
		dev_err(player->dev, "Failed to get clock");
		return -EINVAL;
	}

	/* Registers view in ALSA's procfs */

	result = snd_stm_info_register(&player->proc_entry,
			dev_name(player->dev),
			uniperif_player_dump_registers, player);
	if (result < 0) {
		dev_err(player->dev, "Failed to register with procfs");
		return result;
	}

	/* Register the ALSA controls */

	dev_dbg(player->dev, "Adding ALSA controls");

	if (player->ver < SND_STM_UNIPERIF_VERSION_UNI_PLR_TOP_1_0) {
		/* Register the uniperipheral parking ALSA controls */
		result = uniperif_player_add_ctls(snd_device,
			uniperif_player_parking_ctls,
			ARRAY_SIZE(uniperif_player_parking_ctls));
		if (result) {
			dev_err(player->dev, "Failed to add parking ctls");
			return result;
		}
	}

	if (player->ver == SND_STM_UNIPERIF_VERSION_UNI_PLR_TOP_1_0) {
		/* Register the uniperipheral standby ALSA controls */
		result = uniperif_player_add_ctls(snd_device,
			uniperif_player_standby_ctls,
			ARRAY_SIZE(uniperif_player_standby_ctls));
		if (result) {
			dev_err(player->dev, "Failed to add standby ctls");
			return result;
		}
	}

	if (player->ver == SND_STM_UNIPERIF_VERSION_UNI_PLR_TOP_1_0) {
		/* Register the uniperipheral underflow ALSA controls */
		result = uniperif_player_add_ctls(snd_device,
			uniperif_player_underflow_ctls,
			ARRAY_SIZE(uniperif_player_underflow_ctls));
		if (result) {
			dev_err(player->dev, "Failed to add underflow ctls");
			return result;
		}
	}

	if (UNIPERIF_PLAYER_TYPE_IS_IEC958(player)) {
		/* Register the iec958 uniperipheral ALSA controls */
		result = uniperif_player_add_ctls(snd_device,
			uniperif_player_iec958_ctls,
			ARRAY_SIZE(uniperif_player_iec958_ctls));
		if (result) {
			dev_err(player->dev, "Failed to add iec958 ctls");
			return result;
		}
	}

	return result;
}

static int uniperif_player_disconnect(struct snd_device *snd_device)
{
	struct uniperif_player *player = snd_device->device_data;

	dev_dbg(player->dev, "%s(snd_device=%p)", __func__, snd_device);

	BUG_ON(!player);
	BUG_ON(!snd_stm_magic_valid(player));

	if (player->state != UNIPERIF_PLAYER_STATE_STOPPED) {
		/* Stop the player */
		uniperif_player_stop(player->substream);

		if (player->dma_channel) {
			/* Release the dma channel */
			dma_release_channel(player->dma_channel);
			player->dma_channel = NULL;
		}

		/* Release any converter */
		if (player->conv_group) {
			snd_stm_conv_release_group(player->conv_group);
			player->conv_group = NULL;
		}

		/* Clear the substream pointer */
		player->substream = NULL;
	}

	snd_stm_clk_put(player->clock);

	snd_stm_info_unregister(player->proc_entry);

	return 0;
}

static struct snd_device_ops uniperif_player_snd_device_ops = {
	.dev_register	= uniperif_player_register,
	.dev_disconnect	= uniperif_player_disconnect,
};

/*
 * Platform driver routines
 */

static int uniperif_player_parse_dt(struct platform_device *pdev,
		struct uniperif_player *player)
{
	struct snd_stm_uniperif_player_info *info;
	struct device_node *pnode;
	const char *mode;
	int val;

	BUG_ON(!pdev);
	BUG_ON(!player);

	/* Allocate memory for the info structure */
	info = devm_kzalloc(&pdev->dev, sizeof(*info), GFP_KERNEL);
	BUG_ON(!info);

	/* Read the device properties */
	pnode = pdev->dev.of_node;

	of_property_read_u32(pnode, "version", &info->ver);

	/* get dma node */
	val = get_property_hdl(&pdev->dev, pnode, "dmas", 0);
	if (!val) {
		dev_err(&pdev->dev, "unable to find DT dma node");
		return -EINVAL;
	}
	info->dma_np = of_find_node_by_phandle(val);

	of_property_read_string(pnode, "dma-names", &info->fdma_name);
	of_property_read_u32(pnode, "fdma-initiator", &info->fdma_initiator);
	of_property_read_u32(pnode, "fdma-direct-conn",
			&info->fdma_direct_conn);
	of_property_read_u32(pnode, "fdma-request-line",
			&info->fdma_request_line);

	of_property_read_string(pnode, "description", &info->name);
	of_property_read_u32(pnode, "card-device", &info->card_device);

	/* Read the device mode property */
	of_property_read_string(pnode, "mode", &mode);

	if (strcasecmp(mode, "hdmi") == 0)
		info->player_type = SND_STM_UNIPERIF_PLAYER_TYPE_HDMI;
	else if (strcasecmp(mode, "pcm") == 0)
		info->player_type = SND_STM_UNIPERIF_PLAYER_TYPE_PCM;
	else if (strcasecmp(mode, "spdif") == 0)
		info->player_type = SND_STM_UNIPERIF_PLAYER_TYPE_SPDIF;
	else
		info->player_type = SND_STM_UNIPERIF_PLAYER_TYPE_NONE;

	/* Read any iec958 (hdmi/spdif) lr polarity and i2s mode settings */
	of_property_read_u32(pnode, "iec958-lr-pol", &info->iec958_lr_pol);
	of_property_read_u32(pnode, "iec958-i2s-mode", &info->iec958_i2s_mode);

	/* Read the device features properties */
	of_property_read_u32(pnode, "channels", &info->channels);
	of_property_read_u32(pnode, "parking", &info->parking_enabled);
	of_property_read_u32(pnode, "standby", &info->standby_enabled);
	of_property_read_u32(pnode, "underflow", &info->underflow_enabled);
	of_property_read_u32(pnode, "s16-swap-lr", &info->s16_swap_lr);

	/* Save the info structure */
	player->info = info;

	of_property_read_string(pnode, "clock-names", &player->clk_name);

	if (of_property_read_u32(pnode, "auto-suspend-delay",
				 &info->suspend_delay))
		info->suspend_delay = UNIPERIF_PLAYER_NO_SUSPEND;

	return 0;
}

static int uniperif_player_probe(struct platform_device *pdev)
{
	int result = 0;
	struct uniperif_player *player;
	struct snd_card *card = snd_stm_card_get(SND_STM_CARD_TYPE_AUDIO);
	static unsigned int channels_2_10[] = { 2, 4, 6, 8, 10 };
	static unsigned char *strings_2_10[] = {
			"2", "2/4", "2/4/6", "2/4/6/8", "2/4/6/8/10"};

	dev_dbg(&pdev->dev, "%s(pdev=%p)", __func__, pdev);

	BUG_ON(!card);

	player = devm_kzalloc(&pdev->dev, sizeof(*player), GFP_KERNEL);
	if (!player) {
		dev_err(&pdev->dev, "Failed to allocate device structure");
		return -ENOMEM;
	}
	snd_stm_magic_set(player);
	player->dev = &pdev->dev;
	player->state = UNIPERIF_PLAYER_STATE_STOPPED;
	player->underflow_us = UNIPERIF_PLAYER_UNDERFLOW_US;

	if (!pdev->dev.of_node) {
		dev_err(&pdev->dev, "device not in dt");
		return -EINVAL;
	}

	if (uniperif_player_parse_dt(pdev, player)) {
		dev_err(&pdev->dev, "invalid dt");
		return -EINVAL;
	}

	if (!player->clk_name) {
		dev_err(&pdev->dev, "Failed to get clock name (not specify in DT)");
		return -EINVAL;
	}

	BUG_ON(!player->info);
	BUG_ON(player->info->ver == SND_STM_UNIPERIF_VERSION_UNKOWN);
	player->ver = player->info->ver;

	spin_lock_init(&player->default_settings_lock);

	dev_notice(&pdev->dev, "'%s'", player->info->name);

	/* Set player specific options */

	switch (player->info->player_type) {
	case SND_STM_UNIPERIF_PLAYER_TYPE_HDMI:
		player->hardware = uniperif_player_pcm_hw;
		player->stream_settings.input_mode =
				UNIPERIF_IEC958_INPUT_MODE_PCM;
		break;
	case SND_STM_UNIPERIF_PLAYER_TYPE_PCM:
		player->hardware = uniperif_player_pcm_hw;
		break;
	case SND_STM_UNIPERIF_PLAYER_TYPE_SPDIF:
		/* Default to PCM mode where hardware does everything */
		player->hardware = uniperif_player_pcm_hw;
		player->stream_settings.input_mode =
				UNIPERIF_IEC958_INPUT_MODE_PCM;
		break;
	default:
		dev_err(&pdev->dev, "Invalid player type (%d)",
				player->info->player_type);
		return -EINVAL;
	}

	/* Parking mode is only supported on older ip revisions */
	if (player->ver < SND_STM_UNIPERIF_VERSION_UNI_PLR_TOP_1_0) {
		dev_notice(&pdev->dev, "Parking mode:  Supported");

		/* Older ip does not support standby mode */
		if (player->info->standby_enabled) {
			dev_warn(&pdev->dev, "Standby unsupported on old ip");
			player->info->standby_enabled = 0;
		}

		/* Older ip does not support underflow recovery */
		if (player->info->underflow_enabled) {
			dev_warn(&pdev->dev, "Underflow unsupported on old ip");
			player->info->underflow_enabled = 0;
		}
	}

	/* Standby mode is only supported on newer ip revisions */
	if (player->ver > SND_STM_UNIPERIF_VERSION_UNI_PLR_TOP_1_0) {
		dev_notice(&pdev->dev, "Standby mode:  Supported");
		dev_notice(&pdev->dev, "Underflow:     Supported");

		/* Newer ip does not support parking mode */
		if (player->info->parking_enabled) {
			dev_warn(&pdev->dev, "Parking unsupported on new ip");
			player->info->parking_enabled = 0;
		}
	}

	/* Set default iec958 status bits (I expect user to override!) */

	/* Consumer, PCM, copyright, 2ch, mode 0 */
	player->default_settings.iec958.status[0] = 0x00;
	/* Broadcast reception category */
	player->default_settings.iec958.status[1] = 0x04;
	/* Do not take into account source or channel number */
	player->default_settings.iec958.status[2] = 0x00;
	/* Sampling frequency not indicated */
	player->default_settings.iec958.status[3] = 0x01;
	/* Max sample word 24-bit, sample word length not indicated */
	player->default_settings.iec958.status[4] = 0x01;

	/* Get resources */

	result = snd_stm_memory_request(pdev, &player->mem_region,
			&player->base);
	if (result < 0) {
		dev_err(&pdev->dev, "Failed memory request");
		return result;
	}
	player->fifo_phys_address = player->mem_region->start +
		offset__AUD_UNIPERIF_FIFO_DATA(player);

	result = snd_stm_irq_request(pdev, &player->irq,
			uniperif_player_irq_handler, player);
	if (result < 0) {
		dev_err(&pdev->dev, "Failed IRQ request");
		return result;
	}

	/* Get player capabilities */

	BUG_ON(player->info->channels % 2);
	BUG_ON(player->info->channels < 2);
	BUG_ON(player->info->channels > 10);

	player->channels_constraint.list = channels_2_10;
	player->channels_constraint.count = player->info->channels / 2;
	player->channels_constraint.mask = 0;

	dev_notice(player->dev, "%s-channel PCM",
			strings_2_10[player->channels_constraint.count-1]);

	/* Create ALSA lowlevel device */

	result = snd_device_new(card, SNDRV_DEV_LOWLEVEL, player,
			&uniperif_player_snd_device_ops);
	if (result < 0) {
		dev_err(&pdev->dev, "Failed to create ALSA sound device");
		goto error_device;
	}

	/* Create ALSA PCM device */

	result = snd_pcm_new(card, NULL, player->info->card_device, 1, 0,
			&player->pcm);
	if (result < 0) {
		dev_err(&pdev->dev, "Failed to create ALSA PCM instance");
		goto error_pcm;
	}
	player->pcm->private_data = player;
	strcpy(player->pcm->name, player->info->name);

	snd_pcm_set_ops(player->pcm, SNDRV_PCM_STREAM_PLAYBACK,
			&uniperif_player_pcm_ops);

	/* Initialize buffer */

	player->buffer = snd_stm_buffer_create(player->pcm, player->dev,
			player->hardware.buffer_bytes_max);
	if (!player->buffer) {
		dev_err(&pdev->dev, "Failed to create buffer");
		result = -ENOMEM;
		goto error_buffer_init;
	}

	/* Register in converters router */

	player->conv_source = snd_stm_conv_register_source(
			&platform_bus_type, pdev->dev.of_node,
			player->info->channels,
			card, player->info->card_device);
	if (!player->conv_source) {
		dev_err(&pdev->dev, "Failed to register converter source");
		result = -ENOMEM;
		goto error_conv_register_source;
	}

	/* Done now */

	platform_set_drvdata(pdev, player);

	if (player->info->suspend_delay != UNIPERIF_PLAYER_NO_SUSPEND) {
		if (player->info->suspend_delay < MIN_AUTOSUPEND_DELAY_MS) {
			player->info->suspend_delay = MIN_AUTOSUPEND_DELAY_MS;
			dev_info(&pdev->dev,
				 "auto suspend delay set to %u ms",
				 player->info->suspend_delay);
		}

		pm_runtime_set_autosuspend_delay(&pdev->dev,
						 player->info->suspend_delay);
		pm_runtime_use_autosuspend(&pdev->dev);
		pm_runtime_enable(&pdev->dev);
	}

	return 0;

error_conv_register_source:
error_buffer_init:
	/* snd_pcm_free() is not available - PCM device will be released
	 * during card release */
error_pcm:
	snd_device_free(card, player);
error_device:
	return result;
}

static int uniperif_player_remove(struct platform_device *pdev)
{
	struct uniperif_player *player = platform_get_drvdata(pdev);

	dev_dbg(&pdev->dev, "%s(pdev=%p)", __func__, pdev);

	BUG_ON(!player);
	BUG_ON(!snd_stm_magic_valid(player));

	pm_runtime_disable(&pdev->dev);

	snd_stm_conv_unregister_source(player->conv_source);

	snd_stm_magic_clear(player);

	return 0;
}

/*
 * Power management
 */

#ifdef CONFIG_PM
static int uniperif_player_suspend(struct device *dev)
{
	struct uniperif_player *player = dev_get_drvdata(dev);
	struct snd_card *card = snd_stm_card_get(SND_STM_CARD_TYPE_AUDIO);

	dev_dbg(dev, "%s(dev=%p)", __func__, dev);

	if (pm_runtime_suspended(dev))
		return 0;

	if (player->dma_channel &&
	    dma_audio_is_parking_active(player->dma_channel))
		uniperif_player_stop(player->substream);

	/* We must exit parking mode before entering suspend */
	if (player->state == UNIPERIF_PLAYER_STATE_PARKING)
		uniperif_player_stop(player->substream);

	/* We must exit standby mode before entering suspend */
	if (player->state == UNIPERIF_PLAYER_STATE_STANDBY)
		uniperif_player_stop(player->substream);

	/* Abort if the player is still running */
	if (get__AUD_UNIPERIF_CTRL__OPERATION(player)) {
		dev_err(player->dev, "Cannot suspend as running");
		return -EBUSY;
	}

	if (player->state == UNIPERIF_PLAYER_STATE_STOPPED) {
		if (player->dma_channel) {
			/* Release the dma channel */
			dma_release_channel(player->dma_channel);
			player->dma_channel = NULL;
		}

		if (player->conv_group) {
			/* Release the converter */
			snd_stm_conv_release_group(player->conv_group);
			player->conv_group = NULL;
		}
	}

	/* Indicate power off (with power) and suspend all streams */
	snd_power_change_state(card, SNDRV_CTL_POWER_D3hot);
	snd_pcm_suspend_all(player->pcm);

	/* pinctrl: switch pinstate to sleep */
	pinctrl_pm_select_sleep_state(dev);

	return 0;
}

static int uniperif_player_resume(struct device *dev)
{
	struct snd_card *card = snd_stm_card_get(SND_STM_CARD_TYPE_AUDIO);

	dev_dbg(dev, "%s(dev=%p)", __func__, dev);

	snd_power_change_state(card, SNDRV_CTL_POWER_D0);

	/* pinctrl: switch pinstate to default */
	pinctrl_pm_select_default_state(dev);

	return 0;
}

static int uniperif_player_runtime_suspend(struct device *dev)
{
	int result;
	struct uniperif_player *player = dev_get_drvdata(dev);

	dev_dbg(dev, "%s(dev=%p)", __func__, dev);

	snd_pcm_stream_lock(player->substream);
	result = uniperif_player_suspend(dev);
	snd_pcm_stream_unlock(player->substream);

	return result;
}

const struct dev_pm_ops uniperif_player_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(uniperif_player_suspend,
				uniperif_player_resume)

	SET_RUNTIME_PM_OPS(uniperif_player_runtime_suspend,
			   uniperif_player_resume, NULL)
};

#define UNIPERIF_PLAYER_PM_OPS	(&uniperif_player_pm_ops)
#else
#define UNIPERIF_PLAYER_PM_OPS	NULL
#endif

/*
 * Module initialization
 */

static struct of_device_id uniperif_player_match[] = {
	{
		.compatible = "st,snd_uni_player",
	},
	{},
};

MODULE_DEVICE_TABLE(of, uniperif_player_match);

static struct platform_driver uniperif_player_platform_driver = {
	.driver.name	= "snd_uni_player",
	.driver.of_match_table = uniperif_player_match,
	.driver.pm	= UNIPERIF_PLAYER_PM_OPS,
	.probe		= uniperif_player_probe,
	.remove		= uniperif_player_remove,
};

module_platform_driver(uniperif_player_platform_driver);

MODULE_AUTHOR("John Boddie <john.boddie@st.com>");
MODULE_DESCRIPTION("STMicroelectronics uniperipheral player driver");
MODULE_LICENSE("GPL");
