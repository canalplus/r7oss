/*
 *   STMicroelectronics System-on-Chips' Uniperipheral player driver
 *
 *   Copyright (c) 2014 STMicroelectronics
 *
 *   Author: Arnaud Pouliquen <arnaud.pouliquen@st.com>,
 *
 *   Based on the early work done by:
 *      John Boddie <john.boddie@st.com>
 *      Sevanand Singh <sevanand.singh@st.com>
 *      Mark Glaisher
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

#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/clk.h>

#include <sound/pcm.h>
#include <sound/info.h>
#include <sound/control.h>
#include <sound/asoundef.h>

#include "uniperif.h"

/*
 * Some hardware-related definitions
 */

#define DEFAULT_FORMAT (SND_ST_FORMAT_I2S | SND_ST_FORMAT_SUBFRAME_32_BITS)

#define DEFAULT_OVERSAMPLING 128 /* make all ip's running at same rate*/

#define MIN_IEC958_SAMPLE_RATE	32000

/*
 * Driver specific types.
 */

#define UNIPERIF_PLAYER_TYPE_IS_HDMI(p) \
	((p)->info->player_type == SND_ST_UNIPERIF_PLAYER_TYPE_HDMI)
#define UNIPERIF_PLAYER_TYPE_IS_PCM(p) \
	((p)->info->player_type == SND_ST_UNIPERIF_PLAYER_TYPE_PCM)
#define UNIPERIF_PLAYER_TYPE_IS_SPDIF(p) \
	((p)->info->player_type == SND_ST_UNIPERIF_PLAYER_TYPE_SPDIF)
#define UNIPERIF_PLAYER_TYPE_IS_IEC958(p) \
	(UNIPERIF_PLAYER_TYPE_IS_HDMI(p) || \
		UNIPERIF_PLAYER_TYPE_IS_SPDIF(p))

#define DUMP_REGISTER(r) \
		snd_iprintf(buffer, "AUD_UNIPERIF_%-23s (offset 0x%04x) = " \
				"0x%08x\n", __stringify(r), \
				OFFSET_UNIPERIF_##r(player), \
				GET_UNIPERIF_##r(player))

const struct snd_pcm_hardware uniperif_player_pcm_hw = {
	.info		= (
	SNDRV_PCM_INFO_INTERLEAVED |
	SNDRV_PCM_INFO_BLOCK_TRANSFER |
	SNDRV_PCM_INFO_PAUSE),
	.formats	= (SNDRV_PCM_FMTBIT_S32_LE |
	SNDRV_PCM_FMTBIT_S16_LE),

	.rates		= SNDRV_PCM_RATE_CONTINUOUS,
	.rate_min	= UNIPERIF_MIN_RATE,
	.rate_max	= UNIPERIF_MAX_RATE,

	.channels_min	= UNIPERIF_MIN_CHANNELS,
	.channels_max	= UNIPERIF_MAX_CHANNELS,

	.periods_min	= UNIPERIF_PERIODS_MIN,
	.periods_max	= UNIPERIF_PERIODS_MAX,

	.period_bytes_min = UNIPERIF_PERIODS_BYTES_MIN,
	.period_bytes_max = UNIPERIF_PERIODS_BYTES_MAX,
	.buffer_bytes_max = UNIPERIF_BUFFER_BYTES_MAX
};

static inline int get_property_hdl(struct device *dev, struct device_node *np,
					const char *prop, int idx)
{
	int sz = 0;
	const __be32 *phandle;

	phandle = of_get_property(np, prop, &sz);

	if (!phandle) {
		dev_err(dev, "%s: ERROR: DT-property '%s' missing or invalid!",
				__func__, prop);
		return -EINVAL;
	}

	if (idx >= sz) {
		dev_err(dev, "%s: ERROR: Array-index (%u) >= array-size (%u)!",
				__func__, idx, sz);
		return -EINVAL;
	}

	return be32_to_cpup(phandle + idx);
}

/*
 * Uniperipheral player implementation
 */

static irqreturn_t uniperif_player_irq_handler(int irq, void *dev_id)
{
	irqreturn_t ret = IRQ_NONE;
	struct uniperif *player = dev_id;
	unsigned int status;
	unsigned int tmp;

	if (!player) {
		dev_dbg(player->dev, "invalid pointer(s)");
		return 0;
	}

	/* Get interrupt status & clear them immediately */
	preempt_disable();
	status = GET_UNIPERIF_ITS(player);
	/* treat ghost IRQ */
	if (!status)
		return 0;

	SET_UNIPERIF_ITS_BCLR(player, status);

	preempt_enable();

	/* Check for fifo error (under run) */
	if (unlikely(status & MASK_UNIPERIF_ITS_FIFO_ERROR(player))) {
		dev_err(player->dev, "FIFO underflow error detected");

		/* Interrupt is just for information when underflow recovery */
		if (player->info->underflow_enabled) {
			/* Update state to underflow */
			player->state = UNIPERIF_STATE_UNDERFLOW;

		} else {
			/* Disable interrupt so doesn't continually fire */
			SET_UNIPERIF_ITM_BCLR_FIFO_ERROR(player);

			/* Update state to xrun and stop the player */
			player->state = UNIPERIF_STATE_XRUN;
		}

		ret = IRQ_HANDLED;
	}

	/* Check for dma error (over run) */
	if (unlikely(status & MASK_UNIPERIF_ITS_DMA_ERROR(player))) {
		dev_err(player->dev, "DMA error detected");

		/* Disable interrupt so doesn't continually fire */
		SET_UNIPERIF_ITM_BCLR_DMA_ERROR(player);

		/* Update state to xrun and stop the player */
		player->state = UNIPERIF_STATE_XRUN;

		ret = IRQ_HANDLED;
	}

	/* Check for underflow recovery done */
	if (unlikely(status &
			MASK_UNIPERIF_ITM_UNDERFLOW_REC_DONE(player))) {
		if (!player->info->underflow_enabled) {
			dev_err(player->dev, "unexpected Underflow recovering");
			return -EPERM;
		}
		/* Read the underflow recovery duration */
		tmp = GET_UNIPERIF_STATUS_1_UNDERFLOW_DURATION(player);
		dev_dbg(player->dev, "Underflow recovered (%d LR clocks)",
									tmp);

		/* Clear the underflow recovery duration */
		SET_UNIPERIF_BIT_CONTROL_CLR_UNDERFLOW_DURATION(player);

		/* Update state to started */
		player->state = UNIPERIF_STATE_STARTED;

		ret = IRQ_HANDLED;
	}

	if (unlikely(status &
		MASK_UNIPERIF_ITS_MEM_BLK_READ(player))) {
		ret = IRQ_HANDLED;
	}

	/* Checkf or underflow recovery failed */
	if (unlikely(status &
		MASK_UNIPERIF_ITM_UNDERFLOW_REC_FAILED(player))) {

		dev_err(player->dev, "Underflow recovery failed");

		/* Update state to xrun and stop the player */
		player->state = UNIPERIF_STATE_XRUN;

		ret = IRQ_HANDLED;
	}

	/* error  on unhandled interrupt */
	if (ret != IRQ_HANDLED)
		dev_err(player->dev, "IRQ status : %#x", status);

	return ret;
}

#ifdef CONFIG_DEBUG_FS
static void uniperif_player_dump_registers(struct snd_info_entry *entry,
			struct snd_info_buffer *buffer)
{
	struct uniperif *player = entry->private_data;
	char *state[] = {
		"STOPPED",
		"STARTED",
		"STANDBY",
		"UNDERFLOW",
		"XRUN"
	};

	pr_debug("%s: enter", __func__);

	if (!player) {
		dev_err(player->dev, "invalid pointer(s)");
		return;
	}

	snd_iprintf(buffer, "--- %s (0x%p) %s ---\n", dev_name(player->dev),
			player->base, state[player->state]);

	DUMP_REGISTER(SOFT_RST);
	DUMP_REGISTER(STA);
	DUMP_REGISTER(ITS);
	DUMP_REGISTER(ITM);

	if (player->ver < SND_ST_UNIPERIF_VERSION_UNI_PLR_TOP_1_0) {
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

static int uniperif_player_info_register(struct snd_info_entry *root,
					const char *name,
					struct uniperif *player)
{
	int ret = 0;

	pr_debug("%s : enter\n", __func__);

	player->entry = snd_info_create_module_entry(THIS_MODULE, name, root);

	if (player->entry) {
		player->entry->c.text.read = uniperif_player_dump_registers;
		player->entry->private_data = player;

		if (snd_info_register(player->entry) < 0) {
			ret = -EINVAL;
			snd_info_free_entry(player->entry);
		}
	} else
		ret = -EINVAL;

	return ret;
}

#endif

static void uniperif_player_set_channel_status(struct uniperif *player,
			struct snd_pcm_runtime *runtime)
{
	int n;
	unsigned int status;

	dev_dbg(player->dev, "%s(player=%p, runtime=%p)", __func__,
			player, runtime);

	if (!player) {
		dev_err(player->dev, "invalid pointer(s)");
		return;
	}

	/*
	 * Some AVRs and TVs require the channel status to contain a correct
	 * sampling frequency. If no sample rate is already specified, then
	 * set one.
	 */

	spin_lock(&player->lock);
	if (runtime && (player->stream_settings.iec958.status[3]
					== IEC958_AES3_CON_FS_NOTID)) {
		switch (runtime->rate) {
		case 22050:
			player->stream_settings.iec958.status[3] =
						IEC958_AES3_CON_FS_22050;
			break;
		case 44100:
			player->stream_settings.iec958.status[3] =
						IEC958_AES3_CON_FS_44100;
			break;
		case 88200:
			player->stream_settings.iec958.status[3] =
						IEC958_AES3_CON_FS_88200;
			break;
		case 176400:
			player->stream_settings.iec958.status[3] =
						IEC958_AES3_CON_FS_176400;
			break;
		case 24000:
			player->stream_settings.iec958.status[3] =
						IEC958_AES3_CON_FS_24000;
			break;
		case 48000:
			player->stream_settings.iec958.status[3] =
						IEC958_AES3_CON_FS_48000;
			break;
		case 96000:
			player->stream_settings.iec958.status[3] =
						IEC958_AES3_CON_FS_96000;
			break;
		case 192000:
			player->stream_settings.iec958.status[3] =
						IEC958_AES3_CON_FS_192000;
			break;
		case 32000:
			player->stream_settings.iec958.status[3] =
						IEC958_AES3_CON_FS_32000;
			break;
		case 768000:
			player->stream_settings.iec958.status[3] =
						IEC958_AES3_CON_FS_768000;
			break;
		default:
			/* Mark as sampling frequency not indicated */
			player->stream_settings.iec958.status[3] =
						IEC958_AES3_CON_FS_NOTID;
			break;
		}
	}

	/* audio mode
	 * use audio mode status to select PCM or encoded mode
	 */
	if (player->stream_settings.iec958.status[0] & IEC958_AES0_NONAUDIO)
		player->stream_settings.encoding_mode =
			UNIPERIF_IEC958_ENCODING_MODE_ENCODED;
	else
		player->stream_settings.encoding_mode =
			UNIPERIF_IEC958_ENCODING_MODE_PCM;

	/* Program the new channel status */
	for (n = 0; n < 6; ++n) {
		status  =
		player->stream_settings.iec958.status[0 + (n * 4)] & 0xf;
		status |=
		player->stream_settings.iec958.status[1 + (n * 4)] << 8;
		status |=
		player->stream_settings.iec958.status[2 + (n * 4)] << 16;
		status |=
		player->stream_settings.iec958.status[3 + (n * 4)] << 24;
		dev_dbg(player->dev, "Channel Status Regsiter %d: %08x",
				n, status);
		SET_UNIPERIF_CHANNEL_STA_REGN(player, n, status);
	}
	spin_unlock(&player->lock);

	/* Update the channel status */
	if (player->ver < SND_ST_UNIPERIF_VERSION_UNI_PLR_TOP_1_0)
		SET_UNIPERIF_CONFIG_CHL_STS_UPDATE(player);
	else
		SET_UNIPERIF_BIT_CONTROL_CHL_STS_UPDATE(player);
}

static int uniperif_player_prepare_iec958(struct uniperif *player,
			struct snd_pcm_runtime *runtime)
{
	dev_dbg(player->dev, "%s(player=%p, runtime=%p)", __func__,
			player, runtime);

	if (!player || !runtime) {
		dev_err(player->dev, "%s: invalid pointer(s)", __func__);
		return -EINVAL;
	}

	/* The player should be stopped ot in standby */
	if ((player->state != UNIPERIF_STATE_STOPPED) &&
		(player->state != UNIPERIF_STATE_STANDBY)) {
		dev_err(player->dev, "%s: invalid player state", __func__);
		return -EINVAL;
	}

	/* Set the default format for later */
	player->format = DEFAULT_FORMAT;

	dev_dbg(player->dev, "Sample frequency %d (oversampling %d)",
			runtime->rate, player->info->oversampling);

	/* Oversampling must be multiple of 128 as iec958 frame is 32-bits */
	if ((player->info->oversampling % 128) ||
		(player->info->oversampling <= 0)) {
		dev_err(player->dev, "%s: invalid oversampling", __func__);
		return -EINVAL;
	}

	/* No sample rates below 32kHz are supported for iec958 */
	if (runtime->rate < MIN_IEC958_SAMPLE_RATE) {
		dev_err(player->dev, "Invalid rate (%d)", runtime->rate);
		return -EINVAL;
	}

	switch (runtime->format) {
	case SNDRV_PCM_FORMAT_S16_LE:
		dev_dbg(player->dev, "16-bit subframe");
		/* 16/16 memory format */
		SET_UNIPERIF_CONFIG_MEM_FMT_16_16(player);
		/* 16-bits per sub-frame */
		SET_UNIPERIF_I2S_FMT_NBIT_32(player);
		/* Set 16-bit sample precision */
		SET_UNIPERIF_I2S_FMT_DATA_SIZE_16(player);
		break;
	case SNDRV_PCM_FORMAT_S32_LE:
		dev_dbg(player->dev, "32-bit subframe");
		/* 16/0 memory format */
		SET_UNIPERIF_CONFIG_MEM_FMT_16_0(player);
		/* 32-bits per sub-frame */
		SET_UNIPERIF_I2S_FMT_NBIT_32(player);
		/* Set 24-bit sample precision */
		SET_UNIPERIF_I2S_FMT_DATA_SIZE_24(player);
		break;
	default:
		dev_dbg(player->dev, "format not supported");
		return -EINVAL;
	}

	/* Set parity to be calculated by the hardware */
	SET_UNIPERIF_CONFIG_PARITY_CNTR_BY_HW(player);

	/* Set channel status bits to be inserted by the hardware */
	SET_UNIPERIF_CONFIG_CHANNEL_STA_CNTR_BY_HW(player);

	/* Set user data bits to be inserted by the hardware */
	SET_UNIPERIF_CONFIG_USER_DAT_CNTR_BY_HW(player);

	/* Set validity bits to be inserted by the hardware */
	SET_UNIPERIF_CONFIG_VALIDITY_DAT_CNTR_BY_HW(player);

	/* Set full software control to disabled */
	SET_UNIPERIF_CONFIG_SPDIF_SW_CTRL_DISABLE(player);

	SET_UNIPERIF_CTRL_ZERO_STUFF_HW(player);

	if (player->stream_settings.encoding_mode ==
		UNIPERIF_IEC958_ENCODING_MODE_PCM) {

		dev_dbg(player->dev, "Encoding Mode: Linear PCM");

		/* Clear user validity bits */
		SET_UNIPERIF_USER_VALIDITY_VALIDITY_LEFT(player,
					0);
		SET_UNIPERIF_USER_VALIDITY_VALIDITY_RIGHT(player,
					0);
	} else {
		dev_dbg(player->dev, "Encoding Mode: Encoded");

		/* Set user validity bits */
		SET_UNIPERIF_USER_VALIDITY_VALIDITY_LEFT(player,
					1);
		SET_UNIPERIF_USER_VALIDITY_VALIDITY_RIGHT(player,
					1);
	}

	/* Update the channel status */
	uniperif_player_set_channel_status(player, runtime);

	/* Clear the user validity user bits */
	SET_UNIPERIF_USER_VALIDITY_USER_LEFT(player, 0);
	SET_UNIPERIF_USER_VALIDITY_USER_RIGHT(player, 0);

	/* Disable one-bit audio mode */
	SET_UNIPERIF_CONFIG_ONE_BIT_AUD_DISABLE(player);

	/* Enable consecutive frames repetition of Z preamble (not for HBRA) */
	SET_UNIPERIF_CONFIG_REPEAT_CHL_STS_ENABLE(player);

	/* Change to SUF0_SUBF1 and left/right channels swap! */
	SET_UNIPERIF_CONFIG_SUBFRAME_SEL_SUBF1_SUBF0(player);

	/* Set lr clock polarity and i2s mode using platform configuration */
	SET_UNIPERIF_I2S_FMT_LR_POL(player, player->info->iec958_lr_pol);
	SET_UNIPERIF_I2S_FMT_PADDING(player,
					player->info->iec958_i2s_mode);

	/* Set data output on rising edge */
	SET_UNIPERIF_I2S_FMT_SCLK_EDGE_RISING(player);

	/* Set data aligned to left with respect to left-right clock polarity */
	SET_UNIPERIF_I2S_FMT_ALIGN_LEFT(player);

	/* Set data output as MSB first */
	SET_UNIPERIF_I2S_FMT_ORDER_MSB(player);

	/* Set the number of channels (maximum supported by spdif is 2) */
	if (UNIPERIF_PLAYER_TYPE_IS_SPDIF(player) &&
		(runtime->channels != 2)) {
		dev_err(player->dev, "invalid nb of channels");
		return -EINVAL;
	}

	SET_UNIPERIF_I2S_FMT_NUM_CH(player, runtime->channels / 2);

	/* Set rounding to off */
	SET_UNIPERIF_CTRL_ROUNDING_OFF(player);

	/* Set clock divisor */
	SET_UNIPERIF_CTRL_DIVIDER(player,
					player->info->oversampling / 128);

	/* Set the spdif latency to not wait before starting player */
	SET_UNIPERIF_CTRL_SPDIF_LAT_OFF(player);

	/*
	 * Ensure iec958 formatting is off. It will be enabled in function
	 * uniperif_player_start() at the same time as the operation
	 * mode is set to work around a silicon issue.
	 */
	if (player->ver < SND_ST_UNIPERIF_VERSION_UNI_PLR_TOP_1_0)
		SET_UNIPERIF_CTRL_SPDIF_FMT_OFF(player);
	else
		SET_UNIPERIF_CTRL_SPDIF_FMT_ON(player);

	return 0;
}

static int uniperif_player_prepare_pcm(struct uniperif *player,
					struct snd_pcm_runtime *runtime)
{
	int bits_in_output_frame;
	int lr_pol;

	dev_dbg(player->dev, "%s(player=%p, runtime=%p)", __func__,
			player, runtime);

	if (!player || !runtime) {
		dev_err(player->dev, "%s: invalid pointer(s)", __func__);
		return -EINVAL;
	}

	/* Set the default format for later */
	player->format = DEFAULT_FORMAT;

	if (player->info->oversampling <= 0) {
		dev_err(player->dev, "%s: invalid oversampling", __func__);
		return -EINVAL;
	}

	/* For 32 bits subframe oversampling must be a multiple of 128,
	 * for 16 bits - of 64 */
	if ((player->format & SND_ST_FORMAT_SUBFRAME_32_BITS) &&
		(player->info->oversampling % 128)) {
		dev_err(player->dev, "%s: invalid oversampling", __func__);
		return -EINVAL;
	}

	if ((player->format & SND_ST_FORMAT_SUBFRAME_16_BITS) &&
		(player->info->oversampling % 64)) {
		dev_err(player->dev, "%s: invalid oversampling", __func__);
		return -EINVAL;
	}

	/* Number of bits per subframe (which is one channel sample)
	 * on output - it determines serial clock frequency, which is
	 * 64 times sampling rate for 32 bits subframe (2 channels 32
	 * bits each means 64 bits per frame) and 32 times sampling
	 * rate for 16 bits subframe
	 * (you know why, don't you? :-) */
	switch (player->format & SND_ST_FORMAT_SUBFRAME_MASK) {
	case SND_ST_FORMAT_SUBFRAME_32_BITS:
		dev_dbg(player->dev, "32-bit subframe");
		SET_UNIPERIF_I2S_FMT_NBIT_32(player);
		SET_UNIPERIF_I2S_FMT_DATA_SIZE_32(player);
		bits_in_output_frame = 64; /* frame = 2 * subframe */
		break;
	case SND_ST_FORMAT_SUBFRAME_16_BITS:
		dev_dbg(player->dev, "16-bit subframe");
		SET_UNIPERIF_I2S_FMT_NBIT_16(player);
		SET_UNIPERIF_I2S_FMT_DATA_SIZE_16(player);
		bits_in_output_frame = 32; /* frame = 2 * subframe */
		break;
	default:
		dev_err(player->dev, "subframe format not supported");
		return -EINVAL;
	}

	switch (player->format & SND_ST_FORMAT_MASK) {
	case SND_ST_FORMAT_I2S:
		dev_dbg(player->dev, "I2S format");
		SET_UNIPERIF_I2S_FMT_ALIGN_LEFT(player);
		SET_UNIPERIF_I2S_FMT_PADDING_I2S_MODE(player);
		lr_pol = VALUE_UNIPERIF_I2S_FMT_LR_POL_LOW(player);
		break;
	case SND_ST_FORMAT_LEFT_JUSTIFIED:
		dev_dbg(player->dev, "Left Justified format");
		SET_UNIPERIF_I2S_FMT_ALIGN_LEFT(player);
		SET_UNIPERIF_I2S_FMT_PADDING_SONY_MODE(player);
		lr_pol = VALUE_UNIPERIF_I2S_FMT_LR_POL_HIG(player);
		break;
	case SND_ST_FORMAT_RIGHT_JUSTIFIED:
		dev_dbg(player->dev, "Right Justified format");
		SET_UNIPERIF_I2S_FMT_ALIGN_RIGHT(player);
		SET_UNIPERIF_I2S_FMT_PADDING_SONY_MODE(player);
		lr_pol = VALUE_UNIPERIF_I2S_FMT_LR_POL_HIG(player);
		break;
	default:
		dev_err(player->dev, "justified format not supported");
		return -EINVAL;
	}

	/* Configure data memory format */

	switch (runtime->format) {
	case SNDRV_PCM_FORMAT_S16_LE:
		/* One data word contains two samples */
		SET_UNIPERIF_CONFIG_MEM_FMT_16_16(player);
		break;

	case SNDRV_PCM_FORMAT_S32_LE:
		/* Actually "16 bits/0 bits" means "32/28/24/20/18/16 bits
		 * on the left than zeros (if less than 32 bytes)"... ;-) */
		SET_UNIPERIF_CONFIG_MEM_FMT_16_0(player);
		break;

	default:
		dev_err(player->dev, "format not supported");
		return -EINVAL;
	}

	/* Set the L/R polarity */
	SET_UNIPERIF_I2S_FMT_LR_POL(player, lr_pol);

	/* Set rounding to off */
	SET_UNIPERIF_CTRL_ROUNDING_OFF(player);

	/* Set clock divisor */
	SET_UNIPERIF_CTRL_DIVIDER(player,
		     player->info->oversampling / (2 * bits_in_output_frame));

	/* Number of channels... */

	if ((runtime->channels % 2) || (runtime->channels < 2) ||
		(runtime->channels > 10)) {
		dev_err(player->dev, "%s: invalid nb of channels", __func__);
		return -EINVAL;
	}

	SET_UNIPERIF_I2S_FMT_NUM_CH(player, runtime->channels / 2);

	/* Set 1-bit audio format to disabled */
	SET_UNIPERIF_CONFIG_ONE_BIT_AUD_DISABLE(player);

	SET_UNIPERIF_I2S_FMT_ORDER_MSB(player);
	SET_UNIPERIF_I2S_FMT_SCLK_EDGE_FALLING(player);

	/* No iec958 formatting as outputting to DAC  */
	SET_UNIPERIF_CTRL_SPDIF_FMT_OFF(player);

	return 0;
}

/*
 * ALSA uniperipheral iec958 controls
 */
static int  uniperif_player_ctl_iec958_info(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_info *uinfo)
{
	uinfo->type = SNDRV_CTL_ELEM_TYPE_IEC958;
	uinfo->count = 1;

	return 0;
}

static int uniperif_player_ctl_iec958_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct uniperif *player = snd_kcontrol_chip(kcontrol);
	struct snd_aes_iec958 *iec958 = &player->stream_settings.iec958;

	dev_dbg(player->dev, "%s(kcontrol=%p, ucontrol=%p)", __func__,
			kcontrol, ucontrol);
	spin_lock(&player->lock);
	ucontrol->value.iec958.status[0] = iec958->status[0];
	ucontrol->value.iec958.status[1] = iec958->status[1];
	ucontrol->value.iec958.status[2] = iec958->status[2];
	ucontrol->value.iec958.status[3] = iec958->status[3];
	spin_unlock(&player->lock);
	return 0;
}

static int uniperif_player_ctl_iec958_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct uniperif *player = snd_kcontrol_chip(kcontrol);
	struct snd_aes_iec958 *iec958 =  &player->stream_settings.iec958;

	dev_dbg(player->dev, "%s(kcontrol=%p, ucontrol=%p)", __func__,
			kcontrol, ucontrol);

	spin_lock(&player->lock);
	/* If user settings differ from the current, update current */
	iec958->status[0] = ucontrol->value.iec958.status[0];
	iec958->status[1] = ucontrol->value.iec958.status[1];
	iec958->status[2] = ucontrol->value.iec958.status[2];
	iec958->status[3] = ucontrol->value.iec958.status[3];
	spin_unlock(&player->lock);

	uniperif_player_set_channel_status(player, NULL);
	return 0;
}

const struct snd_kcontrol_new uniperif_player_iec958_ctls = {
		.iface = SNDRV_CTL_ELEM_IFACE_PCM,
		.name = SNDRV_CTL_NAME_IEC958("", PLAYBACK, DEFAULT),
		.info = uniperif_player_ctl_iec958_info,
		.get = uniperif_player_ctl_iec958_get,
		.put = uniperif_player_ctl_iec958_put,
};

static int uniperif_player_open(struct uniperif *player)
{
	if (!player) {
		pr_err("%s: invalid pointer(s)", __func__);
		return -EINVAL;
	}

	pr_debug("%s: enter", __func__);

	SET_UNIPERIF_CONFIG_BACK_STALL_REQ_DISABLE(player);
	SET_UNIPERIF_CTRL_ROUNDING_OFF(player);
	SET_UNIPERIF_CTRL_SPDIF_LAT_OFF(player);
	SET_UNIPERIF_CONFIG_IDLE_MOD_DISABLE(player);

	return 0;
}

static int uniperif_player_prepare(struct uniperif *player,
				struct snd_pcm_runtime *runtime)
{
	int transfer_size, trigger_limit;
	int ret;

	pr_debug("%s: enter", __func__);

	if (!player) {
		pr_err("%s: invalid pointer(s)", __func__);
		return -EINVAL;
	}

	/* The player should be stopped ot in standby */
	if ((player->state != UNIPERIF_STATE_STOPPED) &&
		(player->state != UNIPERIF_STATE_STANDBY)) {
		dev_err(player->dev, "%s: invalid player state %d", __func__,
			player->state);
		return -EINVAL;
	}

	/* Calculate transfer size (in fifo cells and bytes) for frame count */
	transfer_size = runtime->channels * UNIPERIF_FIFO_FRAMES;

	/* Calculate number of empty cells available before asserting DREQ */
	if (player->ver < SND_ST_UNIPERIF_VERSION_UNI_PLR_TOP_1_0)
		trigger_limit = UNIPERIF_FIFO_SIZE - transfer_size;
	else
		/* Since SND_ST_UNIPERIF_VERSION_UNI_PLR_TOP_1_0
		 * FDMA_TRIGGER_LIMIT also controls when the state switches
		 * from OFF or STANDBY to AUDIO DATA.*/
		trigger_limit = transfer_size;

	/* Trigger limit must be an even number */
	if ((!trigger_limit % 2) || (trigger_limit != 1 && transfer_size % 2)
		|| (trigger_limit >
		MASK_UNIPERIF_CONFIG_FDMA_TRIGGER_LIMIT(player))) {
		dev_err(player->dev, "invalid trigger limit %d", trigger_limit);
		return -EINVAL;
	}

	dev_dbg(player->dev, "FDMA trigger limit %d", trigger_limit);

	SET_UNIPERIF_CONFIG_FDMA_TRIGGER_LIMIT(player, trigger_limit);

	SET_UNIPERIF_CONFIG_BACK_STALL_REQ_DISABLE(player);
	SET_UNIPERIF_CTRL_ROUNDING_OFF(player);
	SET_UNIPERIF_CTRL_SPDIF_LAT_OFF(player);
	SET_UNIPERIF_CONFIG_IDLE_MOD_DISABLE(player);

	/* Uniperipheral setup is dependent on player type */
	switch (player->info->player_type) {
	case SND_ST_UNIPERIF_PLAYER_TYPE_HDMI:
		ret = uniperif_player_prepare_iec958(player, runtime);
		break;
	case SND_ST_UNIPERIF_PLAYER_TYPE_PCM:
		ret = uniperif_player_prepare_pcm(player, runtime);
		break;
	case SND_ST_UNIPERIF_PLAYER_TYPE_SPDIF:
		ret = uniperif_player_prepare_iec958(player, runtime);
		break;
	default:
		dev_err(player->dev, "invalid player type");
		return -EINVAL;
	}

	if (ret)
		return ret;

	/* Check if we are moving from standby state to started */
	if (player->state == UNIPERIF_STATE_STANDBY) {
		/* Synchronise transition out of standby */
		if (UNIPERIF_PLAYER_TYPE_IS_IEC958(player))
			SET_UNIPERIF_CTRL_EXIT_STBY_ON_EOBLOCK_ON(player);
		else
			SET_UNIPERIF_CTRL_EXIT_STBY_ON_EOBLOCK_OFF(
					player);

		dev_warn(player->dev, "standby return 0");
		return 0;
	}

	/* We are moving from stopped state to started */
	if (player->state != UNIPERIF_STATE_STOPPED) {
		dev_err(player->dev, "%s: invalid player state", __func__);
		return -EINVAL;
	}

	/* Set clock rate */
	dev_dbg(player->dev, "rate %d  oversampling %d",
			runtime->rate, player->info->oversampling);

	ret = clk_set_rate(player->clk,
			runtime->rate * player->info->oversampling);

	if (ret) {
		dev_err(player->dev, "Failed to set clock rate");
		return ret;
	}

	/* Clear any pending interrupts */
	SET_UNIPERIF_ITS_BCLR(player, GET_UNIPERIF_ITS(player));

	SET_UNIPERIF_I2S_FMT_NO_OF_SAMPLES_TO_READ(player, 0);

	/* Set the interrupt mask */
	SET_UNIPERIF_ITM_BSET_DMA_ERROR(player);
	SET_UNIPERIF_ITM_BSET_FIFO_ERROR(player);
	SET_UNIPERIF_ITM_BSET_MEM_BLK_READ(player);

	/* Enable underflow recovery interrupts */
	if (player->info->underflow_enabled) {
		SET_UNIPERIF_ITM_BSET_UNDERFLOW_REC_DONE(player);
		SET_UNIPERIF_ITM_BSET_UNDERFLOW_REC_FAILED(player);
	}

	/* Reset uniperipheral player */
	SET_UNIPERIF_SOFT_RST_SOFT_RST(player);

	if (player->ver < SND_ST_UNIPERIF_VERSION_UNI_PLR_TOP_1_0)
		while (GET_UNIPERIF_SOFT_RST_SOFT_RST(player))
			udelay(5);

	return 0;
}

static int uniperif_player_start(struct uniperif *player)
{
	int ret;

	pr_debug("%s : enter\n", __func__);

	if (!player) {
		pr_err("%s: invalid pointer(s)", __func__);
		return -EINVAL;
	}

	/* The player should be stopped ot in standby */
	if ((player->state != UNIPERIF_STATE_STOPPED) &&
		(player->state != UNIPERIF_STATE_STANDBY)) {
		pr_err("%s: invalid player state", __func__);
		return -EINVAL;
	}

	/* Check if we are moving from standby state to started */
	if (player->state == UNIPERIF_STATE_STANDBY) {

		pr_debug("%s: player in standby", __func__);
		/* Set the player to audio/pcm data mode */
		SET_UNIPERIF_CTRL_OPERATION_AUDIO_DATA(player);

		/* Update state to started and return */
		player->state = UNIPERIF_STATE_STARTED;
		return 0;
	}

	ret = clk_prepare_enable(player->clk);

	if (ret) {
		pr_err("%s: Failed to enable clock", __func__);
		return ret;
	}

	/* Enable player interrupts */
	enable_irq(player->irq);

	/* Reset uniperipheral player */
	SET_UNIPERIF_SOFT_RST_SOFT_RST(player);

	if (player->ver < SND_ST_UNIPERIF_VERSION_UNI_PLR_TOP_1_0)
		while (GET_UNIPERIF_SOFT_RST_SOFT_RST(player))
			udelay(5);

	/*
	 * Does not use IEC61937 features of the uniperipheral hardware.
	 * Instead it performs IEC61937 in software and inserts it directly
	 * into the audio data stream. As such, when encoded mode is selected,
	 * linear pcm mode is still used, but with the differences of the
	 * channel status bits set for encoded mode and the validity bits set.
	 */

	SET_UNIPERIF_CTRL_OPERATION_PCM_DATA(player);

	/*
	 * If iec958 formatting is required for hdmi or spdif, then it must be
	 * enabled after the operation mode is set. If set prior to this, it
	 * will not take affect and hang the player.
	 */

	if (player->ver < SND_ST_UNIPERIF_VERSION_UNI_PLR_TOP_1_0)
		if (UNIPERIF_PLAYER_TYPE_IS_IEC958(player))
				SET_UNIPERIF_CTRL_SPDIF_FMT_ON(player);

	/* Update state to started */
	player->state = UNIPERIF_STATE_STARTED;

	return 0;
}

static int uniperif_player_stop(struct uniperif *player)
{

	pr_debug("%s: enter", __func__);

	if (!player) {
		pr_err("%s: invalid pointer(s)", __func__);
		return -EINVAL;
	}

	/* The player should not be in stopped state */
	if (player->state == UNIPERIF_STATE_STOPPED) {
		pr_err("%s: invalid player state", __func__);
		return -EINVAL;
	}

	/* Turn the player off */
	SET_UNIPERIF_CTRL_OPERATION_OFF(player);

	/* Soft reset the player */
	SET_UNIPERIF_SOFT_RST_SOFT_RST(player);

	if (player->ver < SND_ST_UNIPERIF_VERSION_UNI_PLR_TOP_1_0)
		while (GET_UNIPERIF_SOFT_RST_SOFT_RST(player))
			udelay(5);

	/* Disable interrupts */
	SET_UNIPERIF_ITM_BCLR(player, GET_UNIPERIF_ITM(player));
	disable_irq_nosync(player->irq);

	/* Disable clock */
	clk_disable_unprepare(player->clk);

	/* Update state to stopped and return */
	player->state = UNIPERIF_STATE_STOPPED;
	return 0;
}

static void uniperif_player_close(struct uniperif *player)
{
	if (!player) {
		pr_err("%s: invalid pointer(s)", __func__);
		return;
	}

	if (player->state != UNIPERIF_STATE_STOPPED) {
		/* Stop the player */
		uniperif_player_stop(player);
	}
}

/*
 * Platform driver routines
 */

static int uniperif_player_parse_dt(struct platform_device *pdev,
					struct uniperif *player)
{
	struct uniperif_info *info;
	struct device_node *pnode;
	struct device *dev = &pdev->dev;
	const char *mode;
	int val;

	if (!player) {
		dev_err(&pdev->dev, "%s: invalid pointer", __func__);
		return -EINVAL;
	}

	/* Allocate memory for the info structure */
	info = devm_kzalloc(&pdev->dev, sizeof(*info), GFP_KERNEL);
	if (!info) {
		dev_err(&pdev->dev, "Failed to allocate device structure");
		return -ENOMEM;
	}

	/* Read the device properties */
	pnode = pdev->dev.of_node;

	if (!pnode) {
		dev_err(&pdev->dev, "%s: invalid pnode", __func__);
		return -EINVAL;
	}

	of_property_read_u32(pnode, "version", &info->ver);

	val = get_property_hdl(dev, dev->of_node, "dmas", 0);

	if (!val) {
		dev_err(&pdev->dev, "unable to find DT dma node");
		return -1;
	}

	info->dma_np = of_find_node_by_phandle(val);
	of_property_read_string(pnode, "dma-names", &info->fdma_name);
	of_property_read_u32(pnode, "fdma-initiator", &info->fdma_initiator);
	of_property_read_u32(pnode, "fdma-direct-conn",
				&info->fdma_direct_conn);
	of_property_read_u32(pnode, "fdma-request-line",
				&info->fdma_request_line);

	/* info->pad_config = stm_of_get_pad_config(&pdev->dev);*/

	of_property_read_string(pnode, "description", &info->name);

	/* Read the device mode property */
	of_property_read_string(pnode, "mode", &mode);

	if (strcasecmp(mode, "hdmi") == 0)
		info->player_type = SND_ST_UNIPERIF_PLAYER_TYPE_HDMI;
	else if (strcasecmp(mode, "pcm") == 0)
		info->player_type = SND_ST_UNIPERIF_PLAYER_TYPE_PCM;
	else if (strcasecmp(mode, "spdif") == 0)
		info->player_type = SND_ST_UNIPERIF_PLAYER_TYPE_SPDIF;
	else
		info->player_type = SND_ST_UNIPERIF_PLAYER_TYPE_NONE;

	/* Read any iec958 (hdmi/spdif) lr polarity and i2s mode settings */
	of_property_read_u32(pnode, "iec958-lr-pol", &info->iec958_lr_pol);
	of_property_read_u32(pnode, "iec958-i2s-mode", &info->iec958_i2s_mode);

	/* Read the device features properties */
	of_property_read_u32(pnode, "channels", &info->channels);
	of_property_read_u32(pnode, "underflow", &info->underflow_enabled);

	of_property_read_string(pnode, "clock-names", &info->clk_name);
	of_property_read_u32(pnode, "mclk_oversampling", &info->oversampling);
	if (!info->oversampling)
		info->oversampling = DEFAULT_OVERSAMPLING;

	/* Save the info structure */
	player->info = info;
	return 0;
}

const struct uniperif_ops uniperif_player_ops = {
	.open = uniperif_player_open,
	.close = uniperif_player_close,
	.prepare = uniperif_player_prepare,
	.start = uniperif_player_start,
	.stop = uniperif_player_stop,
};

int uniperif_player_init(struct platform_device *pdev,
				struct uniperif **uni_player,
				struct snd_info_entry *snd_info_root)
{
	struct uniperif *player;
	static unsigned int channels_2_10[] = { 2, 4, 6, 8, 10 };
	struct snd_kcontrol_new *ctrl;
	int ret = 0;

	dev_dbg(&pdev->dev, "%s: enter", __func__);

	player = devm_kzalloc(&pdev->dev, sizeof(*player), GFP_KERNEL);

	if (!player) {
		dev_err(&pdev->dev, "Failed to allocate device structure");
		return -ENOMEM;
	}

	player->dev = &pdev->dev;
	player->state = UNIPERIF_STATE_STOPPED;
	player->hw = &uniperif_player_pcm_hw;
	player->ops = &uniperif_player_ops;

	ret = uniperif_player_parse_dt(pdev, player);

	if (ret < 0) {
		dev_err(player->dev, "Failed to parse DeviceTree");
		return ret;
	}

	if (player->info->clk_name)
		player->clk = devm_clk_get(player->dev, player->info->clk_name);

	if (IS_ERR(player->clk)) {
		ret = (int)PTR_ERR(player->clk);
		dev_err(player->dev, "%s: ERROR: clk_get failed (%d)!\n",
				__func__, ret);
		return -EINVAL;
	}

	if (player->info->ver == SND_ST_UNIPERIF_VERSION_UNKOWN) {
		dev_err(player->dev, "Unknown uniperipheral version ");
		return -EINVAL;
	}
	player->ver = player->info->ver;

	/* Check for underflow recovery being enabled */
	if (player->info->underflow_enabled) {
		/* Underflow recovery is only supported on later ip revisions */
		if (player->ver < SND_ST_UNIPERIF_VERSION_UNI_PLR_TOP_1_0) {
			dev_err(player->dev,
				"underflow recovery not supported");
			player->info->underflow_enabled = 0;
		} else
			dev_dbg(&pdev->dev, "Underflow:     Enabled");
	}

	/* Get resources */

	player->mem_region = platform_get_resource(pdev, IORESOURCE_MEM, 0);

	if (!player->mem_region) {
		dev_err(&pdev->dev, "Failed to get memory resource");
		return -ENODEV;
	}

	player->base = devm_request_and_ioremap(&pdev->dev,
							player->mem_region);

	if (!player->base) {
		dev_err(&pdev->dev, "Failed to ioremap memory region");
		return -ENXIO;
	}

	player->fifo_phys_address = player->mem_region->start +
					OFFSET_UNIPERIF_FIFO_DATA(player);

	player->irq = platform_get_irq(pdev, 0);

	if (player->irq < 0) {
		dev_err(&pdev->dev, "Failed to get IRQ resource");
		return -ENXIO;
	}

	dev_dbg(&pdev->dev, "IRQ number for device %s : %d",
			dev_name(&pdev->dev), player->irq);

	ret = devm_request_irq(&pdev->dev, player->irq,
				uniperif_player_irq_handler, IRQF_SHARED,
				dev_name(&pdev->dev), player);

	if (ret < 0) {
		dev_err(&pdev->dev, "Failed to request IRQ");
		return -EBUSY;
	}

	/* request_irq() enables the interrupt immediately; as it is
	 * lethal in concurrent audio environment, we want to have
	 * it disabled for most of the time...
	 */
	disable_irq(player->irq);

	/* Get player capabilities */

	if ((player->info->channels % 2) || (player->info->channels < 2) ||
		(player->info->channels > 10)) {
		dev_err(player->dev, "%s: invalid nb of channels", __func__);
		return -EINVAL;
	}

	player->channels_constraint.list = channels_2_10;
	player->channels_constraint.count = player->info->channels / 2;
	player->channels_constraint.mask = 0;

	dev_dbg(player->dev, "%d-channel PCM",
			player->channels_constraint.count);

	*uni_player = player;

	/* ensure that disable by default */
	SET_UNIPERIF_CONFIG_BACK_STALL_REQ_DISABLE(player);
	SET_UNIPERIF_CTRL_ROUNDING_OFF(player);
	SET_UNIPERIF_CTRL_SPDIF_LAT_OFF(player);
	SET_UNIPERIF_CONFIG_IDLE_MOD_DISABLE(player);

	if (UNIPERIF_PLAYER_TYPE_IS_IEC958(player)) {
		/* Set default iec958 status bits  */

		/* Consumer, PCM, copyright, 2ch, mode 0 */
		player->stream_settings.iec958.status[0] = 0x00;
		/* Broadcast reception category */
		player->stream_settings.iec958.status[1] =
					IEC958_AES1_CON_GENERAL;
		/* Do not take into account source or channel number */
		player->stream_settings.iec958.status[2] =
					IEC958_AES2_CON_SOURCE_UNSPEC;
		/* Sampling frequency not indicated */
		player->stream_settings.iec958.status[3] =
					IEC958_AES3_CON_FS_NOTID;
		/* Max sample word 24-bit, sample word length not indicated */
		player->stream_settings.iec958.status[4] =
					IEC958_AES4_CON_MAX_WORDLEN_24 |
					IEC958_AES4_CON_WORDLEN_24_20;

		spin_lock_init(&player->lock);

		/* create the iec958 uniperipheral ALSA controls */
		player->snd_ctrls =
			devm_kzalloc(player->dev, sizeof(*ctrl), GFP_KERNEL);

		memcpy(player->snd_ctrls, &uniperif_player_iec958_ctls,
		       sizeof(struct snd_kcontrol_new));

		dev_dbg(player->dev, "dev ID =%d", pdev->dev.id);
		player->snd_ctrls->name =
			SNDRV_CTL_NAME_IEC958("",	PLAYBACK, DEFAULT);
			player->snd_ctrls->index = pdev->id;
		player->num_ctrls = 1;

	}
	/* Registers view in ALSA's procfs */
#ifdef CONFIG_DEBUG_FS
	ret = uniperif_player_info_register(snd_info_root,
				dev_name(player->dev), player);
	if (ret < 0)
		dev_err(player->dev, "Failed to register with procfs");
#endif

	return 0;
}

int uniperif_player_remove(struct platform_device *pdev)
{
	struct uniperif *player = platform_get_drvdata(pdev);

	dev_dbg(&pdev->dev, "%s(pdev=%p)", __func__, pdev);

	if (!player) {
		dev_dbg(player->dev, "invalid pointer(s)");
		return -EINVAL;
	}
	snd_info_free_entry(player->entry);

	return 0;
}

MODULE_DESCRIPTION("uniperipheral player IP management");
MODULE_AUTHOR("Arnaud Pouliquen <arnaud.pouliquen@st.com>");
MODULE_LICENSE("GPL v2");
