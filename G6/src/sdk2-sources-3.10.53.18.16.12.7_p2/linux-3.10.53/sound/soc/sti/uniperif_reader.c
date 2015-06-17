/*
 *   STMicroelectronics System-on-Chips' Uniperipheral reader driver
 *
 *   Copyright (c) 2014 STMicroelectronics
 *
 *   Author: Arnaud Pouliquen <arnaud.pouliquen@st.com>,
 *
 *         Based on the early work done by:
 *           John Boddie <john.boddie@st.com>
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

#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/clk.h>

#include <sound/pcm.h>
#include <sound/info.h>

#include "uniperif.h"

#define DEFAULT_FORMAT (SND_ST_FORMAT_I2S | SND_ST_FORMAT_SUBFRAME_32_BITS)

#define DUMP_REGISTER(r) \
		snd_iprintf(buffer, "AUD_UNIPERIF_%-23s (offset 0x%04x) = " \
			    "0x%08x\n", __stringify(r), \
			    OFFSET_UNIPERIF_##r(reader), \
			    GET_UNIPERIF_##r(reader))

const struct snd_pcm_hardware uniperif_reader_pcm_hw = {
	.info = (SNDRV_PCM_INFO_INTERLEAVED |
		 SNDRV_PCM_INFO_BLOCK_TRANSFER |
		 SNDRV_PCM_INFO_PAUSE),

	.formats = (SNDRV_PCM_FMTBIT_S32_LE |
		    SNDRV_PCM_FMTBIT_S16_LE),

	.rates = SNDRV_PCM_RATE_CONTINUOUS,
	.rate_min = UNIPERIF_MIN_RATE,
	.rate_max = UNIPERIF_MAX_RATE,

	.channels_min = UNIPERIF_MIN_CHANNELS,
	.channels_max = UNIPERIF_MAX_CHANNELS,

	.periods_min = UNIPERIF_PERIODS_MIN,
	.periods_max = UNIPERIF_PERIODS_MAX,

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
 * Uniperipheral reader implementation
 */

static irqreturn_t uniperif_reader_irq_handler(int irq, void *dev_id)
{
	irqreturn_t ret = IRQ_NONE;
	struct uniperif *reader = dev_id;
	unsigned int status;
	unsigned int tmp;

	if (!reader) {
		dev_dbg(reader->dev, "invalid pointer(s)");
		return ret;
	}

	/* Get interrupt status & clear them immediately */
	preempt_disable();
	status = GET_UNIPERIF_ITS(reader);
	/* treat ghost IRQ */
	if (!status)
		return ret;

	SET_UNIPERIF_ITS_BCLR(reader, status);

	preempt_enable();

	/* Check for fifo error (under run) */
	if (unlikely(status & MASK_UNIPERIF_ITS_FIFO_ERROR(reader))) {
		dev_err(reader->dev, "FIFO underflow error detected");

		/* Interrupt is just for information when underflow recovery */
		if (reader->info->underflow_enabled) {
			/* Update state to underflow */
			reader->state = UNIPERIF_STATE_UNDERFLOW;

		} else {
			/* Disable interrupt so doesn't continually fire */
			SET_UNIPERIF_ITM_BCLR_FIFO_ERROR(reader);

			/* Update state to xrun and stop the reader */
			reader->state = UNIPERIF_STATE_XRUN;
		}

		ret = IRQ_HANDLED;
	}

	/* Check for dma error (over run) */
	if (unlikely(status & MASK_UNIPERIF_ITS_DMA_ERROR(reader))) {
		dev_err(reader->dev, "DMA error detected");

		/* Disable interrupt so doesn't continually fire */
		SET_UNIPERIF_ITM_BCLR_DMA_ERROR(reader);

		/* Update state to xrun and stop the reader */
		reader->state = UNIPERIF_STATE_XRUN;

		ret = IRQ_HANDLED;
	}

	/* Check for underflow recovery done */
	if (unlikely(status &
	    MASK_UNIPERIF_ITM_UNDERFLOW_REC_DONE(reader))) {
		if (!reader->info->underflow_enabled) {
			dev_err(reader->dev, "unexpected Underflow recovering");
			return -EPERM;
		}
		/* Read the underflow recovery duration */
		tmp = GET_UNIPERIF_STATUS_1_UNDERFLOW_DURATION(reader);
		dev_dbg(reader->dev, "Underflow recovered (%d LR clocks)", tmp);

		/* Clear the underflow recovery duration */
		SET_UNIPERIF_BIT_CONTROL_CLR_UNDERFLOW_DURATION(reader);

		/* Update state to started */
		reader->state = UNIPERIF_STATE_STARTED;

		ret = IRQ_HANDLED;
	}

	if (unlikely(status &
		MASK_UNIPERIF_ITS_MEM_BLK_READ(reader))) {
		ret = IRQ_HANDLED;
	}

	/* Checkf or underflow recovery failed */
	if (unlikely(status &
		MASK_UNIPERIF_ITM_UNDERFLOW_REC_FAILED(reader))) {

		dev_err(reader->dev, "Underflow recovery failed");

		/* Update state to xrun and stop the reader */
		reader->state = UNIPERIF_STATE_XRUN;

		ret = IRQ_HANDLED;
	}

	/* error  on unhandled interrupt */
	if (ret != IRQ_HANDLED)
		dev_err(reader->dev, "IRQ status : %#x", status);

	return ret;
}

#ifdef CONFIG_DEBUG_FS
static void uniperif_reader_dump_registers(struct snd_info_entry *entry,
					   struct snd_info_buffer *buffer)
{
	struct uniperif *reader = entry->private_data;
	char *state[] = {
		"STOPPED",
		"STARTED",
		"STANDBY",
		"UNDERFLOW",
		"XRUN"
	};

	dev_dbg(reader->dev, "%s: enter", __func__);

	if (!reader) {
		dev_err(reader->dev, "invalid pointer(s)");
		return;
	}

	snd_iprintf(buffer, "--- %s (0x%p) %s ---\n", dev_name(reader->dev),
		    reader->base, state[reader->state]);

	DUMP_REGISTER(SOFT_RST);
	DUMP_REGISTER(STA);
	DUMP_REGISTER(ITS);
	DUMP_REGISTER(ITM);
	DUMP_REGISTER(CONFIG);
	DUMP_REGISTER(CTRL);
	DUMP_REGISTER(I2S_FMT);
	DUMP_REGISTER(STATUS_1);
	DUMP_REGISTER(DFV0);
	DUMP_REGISTER(CONTROLABILITY);
	DUMP_REGISTER(CRC_CTRL);
	DUMP_REGISTER(CRC_WINDOW);
	DUMP_REGISTER(CRC_VALUE_IN);
	DUMP_REGISTER(CRC_VALUE_OUT);

	snd_iprintf(buffer, "\n");
}
static int uniperif_reader_info_register(struct snd_info_entry *root,
					 const char *name,
					 struct uniperif *reader)
{
	int ret = 0;

	dev_dbg(reader->dev, "%s: enter", __func__);

	reader->entry = snd_info_create_module_entry(THIS_MODULE, name, root);

	if (reader->entry) {
		reader->entry->c.text.read = uniperif_reader_dump_registers;
		reader->entry->private_data = reader;

		if (snd_info_register(reader->entry) < 0) {
			ret = -EINVAL;
			snd_info_free_entry(reader->entry);
		}
	} else
		ret = -EINVAL;

	return ret;
}
#endif

static int uniperif_reader_prepare_pcm(struct uniperif *reader,
				       struct snd_pcm_runtime *runtime)
{
	int lr_pol;

	dev_dbg(reader->dev, "%s(reader=%p, runtime=%p)", __func__,
		reader, runtime);

	if (!reader || !runtime) {
		dev_err(reader->dev, "%s: invalid pointer(s)", __func__);
		return -EINVAL;
	}

	/* Set the default format for later */
	reader->format = DEFAULT_FORMAT;

	/* Number of bits per subframe (i.e one channel sample) on input. */
	switch (reader->format & SND_ST_FORMAT_SUBFRAME_MASK) {
	case SND_ST_FORMAT_SUBFRAME_32_BITS:
		dev_dbg(reader->dev, "32-bit subframe");
		SET_UNIPERIF_I2S_FMT_NBIT_32(reader);
		SET_UNIPERIF_I2S_FMT_DATA_SIZE_32(reader);
		break;
	case SND_ST_FORMAT_SUBFRAME_16_BITS:
		dev_dbg(reader->dev, "16-bit subframe");
		SET_UNIPERIF_I2S_FMT_NBIT_16(reader);
		SET_UNIPERIF_I2S_FMT_DATA_SIZE_16(reader);
		break;
	default:
		dev_err(reader->dev, "subframe format not supported");
		return -EINVAL;
	}

	switch (reader->format & SND_ST_FORMAT_MASK) {
	case SND_ST_FORMAT_I2S:
		dev_dbg(reader->dev, "I2S format");
		SET_UNIPERIF_I2S_FMT_ALIGN_LEFT(reader);
		SET_UNIPERIF_I2S_FMT_PADDING_I2S_MODE(reader);
		lr_pol = VALUE_UNIPERIF_I2S_FMT_LR_POL_LOW(reader);
		break;
	case SND_ST_FORMAT_LEFT_JUSTIFIED:
		dev_dbg(reader->dev, "Left Justified format");
		SET_UNIPERIF_I2S_FMT_ALIGN_LEFT(reader);
		SET_UNIPERIF_I2S_FMT_PADDING_SONY_MODE(reader);
		lr_pol = VALUE_UNIPERIF_I2S_FMT_LR_POL_HIG(reader);
		break;
	case SND_ST_FORMAT_RIGHT_JUSTIFIED:
		dev_dbg(reader->dev, "Right Justified format");
		SET_UNIPERIF_I2S_FMT_ALIGN_RIGHT(reader);
		SET_UNIPERIF_I2S_FMT_PADDING_SONY_MODE(reader);
		lr_pol = VALUE_UNIPERIF_I2S_FMT_LR_POL_HIG(reader);
		break;
	default:
		dev_err(reader->dev, "justified format not supported");
		return -EINVAL;
	}

	SET_UNIPERIF_I2S_FMT_LR_POL(reader, lr_pol);

	/* Configure data memory format */
	switch (runtime->format) {
	case SNDRV_PCM_FORMAT_S16_LE:
		/* One data word contains two samples */
		SET_UNIPERIF_CONFIG_MEM_FMT_16_16(reader);
		break;

	case SNDRV_PCM_FORMAT_S32_LE:
		/*
		 * Actually "16 bits/0 bits" means "32/28/24/20/18/16 bits
		 * on the left than zeros (if less than 32 bytes)"... ;-)
		 */
		SET_UNIPERIF_CONFIG_MEM_FMT_16_0(reader);
		break;

	default:
		dev_err(reader->dev, "format not supported");
		return -EINVAL;
	}

	SET_UNIPERIF_CONFIG_MSTR_CLKEDGE_RISING(reader);
	SET_UNIPERIF_CTRL_READER_OUT_SEL_IN_MEM(reader);

	/*
	 * Serial audio interface format - for detailed explanation
	 * see ie.:
	 * http: www.cirrus.com/en/pubs/appNote/AN282REV1.pdf
	 */

	SET_UNIPERIF_I2S_FMT_ORDER_MSB(reader);

	/* Data clocking (changing) on the rising edge */
	SET_UNIPERIF_I2S_FMT_SCLK_EDGE_RISING(reader);


	/* Number of channels... */

	if ((runtime->channels % 2) || (runtime->channels < 2) ||
		(runtime->channels > 10)) {
		dev_err(reader->dev, "%s: invalid nb of channels", __func__);
		return -EINVAL;
	}

	SET_UNIPERIF_I2S_FMT_NUM_CH(reader, runtime->channels / 2);

	return 0;
}

static int uniperif_reader_open(struct uniperif *reader)
{
	if (!reader) {
		dev_err(reader->dev, "%s: invalid pointer(s)", __func__);
		return -EINVAL;
	}

	dev_dbg(reader->dev, "%s: enter", __func__);

	SET_UNIPERIF_CONFIG_BACK_STALL_REQ_DISABLE(reader);
	SET_UNIPERIF_CTRL_ROUNDING_OFF(reader);
	SET_UNIPERIF_CTRL_SPDIF_LAT_OFF(reader);
	SET_UNIPERIF_CONFIG_IDLE_MOD_DISABLE(reader);

	return 0;
}

static int uniperif_reader_prepare(struct uniperif *reader,
				   struct snd_pcm_runtime *runtime)
{
	int transfer_size, trigger_limit;
	int ret;

	dev_dbg(reader->dev, "%s: enter", __func__);

	if (!reader) {
		dev_err(reader->dev, "%s: invalid pointer(s)", __func__);
		return -EINVAL;
	}

	/* The reader should be stopped */
	if (reader->state != UNIPERIF_STATE_STOPPED) {
		dev_err(reader->dev, "%s: invalid reader state %d", __func__,
			reader->state);
		return -EINVAL;
	}

	/* Calculate transfer size (in fifo cells and bytes) for frame count */
	transfer_size = runtime->channels * UNIPERIF_FIFO_FRAMES;

	/* Calculate number of empty cells available before asserting DREQ */
	if (reader->ver < SND_ST_UNIPERIF_VERSION_UNI_PLR_TOP_1_0)
		trigger_limit = UNIPERIF_FIFO_SIZE - transfer_size;
	else
		/*
		 * Since SND_ST_UNIPERIF_VERSION_UNI_PLR_TOP_1_0
		 * FDMA_TRIGGER_LIMIT also controls when the state switches
		 * from OFF or STANDBY to AUDIO DATA.
		 */
		trigger_limit = transfer_size;

	/* Trigger limit must be an even number */
	if ((!trigger_limit % 2) ||
	    (trigger_limit != 1 && transfer_size % 2) ||
	    (trigger_limit > MASK_UNIPERIF_CONFIG_FDMA_TRIGGER_LIMIT(reader))) {
		dev_err(reader->dev, "invalid trigger limit %d", trigger_limit);
		return -EINVAL;
	}

	dev_dbg(reader->dev, "FDMA trigger limit %d", trigger_limit);

	SET_UNIPERIF_CONFIG_FDMA_TRIGGER_LIMIT(reader, trigger_limit);

	ret = uniperif_reader_prepare_pcm(reader, runtime);

	/* Clear any pending interrupts */
	SET_UNIPERIF_ITS_BCLR(reader, GET_UNIPERIF_ITS(reader));

	SET_UNIPERIF_I2S_FMT_NO_OF_SAMPLES_TO_READ(reader, 0);

	/* Set the interrupt mask */
	SET_UNIPERIF_ITM_BSET_DMA_ERROR(reader);
	SET_UNIPERIF_ITM_BSET_FIFO_ERROR(reader);
	SET_UNIPERIF_ITM_BSET_MEM_BLK_READ(reader);

	/* Enable underflow recovery interrupts */
	if (reader->info->underflow_enabled) {
		SET_UNIPERIF_ITM_BSET_UNDERFLOW_REC_DONE(reader);
		SET_UNIPERIF_ITM_BSET_UNDERFLOW_REC_FAILED(reader);
	}

	/* Reset uniperipheral reader */
	SET_UNIPERIF_SOFT_RST_SOFT_RST(reader);

	while (GET_UNIPERIF_SOFT_RST_SOFT_RST(reader))
		udelay(5);

	return 0;
}

static int uniperif_reader_start(struct uniperif *reader)
{
	int ret;

	dev_dbg(reader->dev, "%s : enter\n", __func__);

	if (!reader) {
		dev_err(reader->dev, "%s: invalid pointer(s)", __func__);
		return -EINVAL;
	}

	/* The reader should be stopped ot in standby */
	if ((reader->state != UNIPERIF_STATE_STOPPED) &&
	    (reader->state != UNIPERIF_STATE_STANDBY)) {
		dev_err(reader->dev, "%s: invalid reader state", __func__);
		return -EINVAL;
	}

	/* Check if we are moving from standby state to started */
	if (reader->state == UNIPERIF_STATE_STANDBY) {

		dev_dbg(reader->dev, "%s: reader in standby", __func__);
		/* Set the reader to audio/pcm data mode */
		SET_UNIPERIF_CTRL_OPERATION_AUDIO_DATA(reader);

		/* Update state to started and return */
		reader->state = UNIPERIF_STATE_STARTED;
		return 0;
	}

	ret = clk_prepare_enable(reader->clk);

	if (ret) {
		dev_err(reader->dev, "%s: Failed to enable clock", __func__);
		return ret;
	}

	/* Enable reader interrupts (and clear possible stalled ones) */
	enable_irq(reader->irq);
	SET_UNIPERIF_ITS_BCLR_FIFO_ERROR(reader);
	SET_UNIPERIF_ITM_BSET_FIFO_ERROR(reader);

	/* Launch the reader */
	SET_UNIPERIF_CTRL_OPERATION_PCM_DATA(reader);

	/* Update state to started */
	reader->state = UNIPERIF_STATE_STARTED;
	return 0;
}

static int uniperif_reader_stop(struct uniperif *reader)
{
	dev_dbg(reader->dev, "%s: enter", __func__);

	if (!reader) {
		dev_err(reader->dev, "%s: invalid pointer(s)", __func__);
		return -EINVAL;
	}

	/* The reader should not be in stopped state */
	if (reader->state == UNIPERIF_STATE_STOPPED) {
		dev_err(reader->dev, "%s: invalid reader state", __func__);
		return -EINVAL;
	}

	/* Turn the reader off */
	SET_UNIPERIF_CTRL_OPERATION_OFF(reader);

	/* Disable interrupts */
	SET_UNIPERIF_ITM_BCLR(reader, GET_UNIPERIF_ITM(reader));
	disable_irq_nosync(reader->irq);

	/* Disable clock */
	clk_disable_unprepare(reader->clk);

	/* Update state to stopped and return */
	reader->state = UNIPERIF_STATE_STOPPED;

	return 0;
}

static void uniperif_reader_close(struct uniperif *reader)
{
	if (!reader) {
		dev_err(reader->dev, "%s: invalid pointer(s)", __func__);
		return;
	}

	if (reader->state != UNIPERIF_STATE_STOPPED) {
		/* Stop the reader */
		uniperif_reader_stop(reader);
	}
}

/*
 * Platform driver routines
 */

static int uniperif_reader_parse_dt(struct platform_device *pdev,
				    struct uniperif *reader)
{
	struct uniperif_info *info;
	struct device_node *pnode;
	struct device *dev = &pdev->dev;
	int val;

	if (!reader) {
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

	of_property_read_u32(pnode, "channels", &info->channels);

	/* Save the info structure */
	reader->info = info;

	return 0;
}

const struct uniperif_ops uniperif_reader_ops = {
	.open = uniperif_reader_open,
	.close = uniperif_reader_close,
	.prepare = uniperif_reader_prepare,
	.start = uniperif_reader_start,
	.stop = uniperif_reader_stop,
};

int uniperif_reader_init(struct platform_device *pdev,
			 struct uniperif **uni_reader,
			 struct snd_info_entry *snd_info_root)
{
	struct uniperif *reader;
	static unsigned int channels_2_10[] = { 2, 4, 6, 8, 10 };
	int ret = 0;

	dev_dbg(&pdev->dev, "%s: enter", __func__);

	reader = devm_kzalloc(&pdev->dev, sizeof(*reader), GFP_KERNEL);

	if (!reader) {
		dev_err(&pdev->dev, "Failed to allocate device structure");
		return -ENOMEM;
	}

	reader->dev = &pdev->dev;
	reader->state = UNIPERIF_STATE_STOPPED;
	reader->hw = &uniperif_reader_pcm_hw;
	reader->ops = &uniperif_reader_ops;
	ret = uniperif_reader_parse_dt(pdev, reader);

	if (ret < 0) {
		dev_err(reader->dev, "Failed to parse DeviceTree");
		return ret;
	}

	/* Get resources */

	reader->mem_region = platform_get_resource(pdev, IORESOURCE_MEM, 0);

	if (!reader->mem_region) {
		dev_err(&pdev->dev, "Failed to get memory resource");
		return -ENODEV;
	}

	reader->base = devm_request_and_ioremap(&pdev->dev,
						reader->mem_region);

	if (!reader->base) {
		dev_err(&pdev->dev, "Failed to ioremap memory region");
		return -ENXIO;
	}

	reader->fifo_phys_address = reader->mem_region->start +
				     OFFSET_UNIPERIF_FIFO_DATA(reader);

	reader->irq = platform_get_irq(pdev, 0);

	if (reader->irq < 0) {
		dev_err(&pdev->dev, "Failed to get IRQ resource");
		return -ENXIO;
	}

	dev_dbg(&pdev->dev, "IRQ number for device %s : %d",
		dev_name(&pdev->dev), reader->irq);

	ret = devm_request_irq(&pdev->dev, reader->irq,
			       uniperif_reader_irq_handler, IRQF_SHARED,
			       dev_name(&pdev->dev), reader);

	if (ret < 0) {
		dev_err(&pdev->dev, "Failed to request IRQ");
		return -EBUSY;
	}

	/*
	 * request_irq() enables the interrupt immediately; as it is
	 * lethal in concurrent audio environment, we want to have
	 * it disabled for most of the time...
	 */
	disable_irq(reader->irq);

	/* Get reader capabilities */

	if ((reader->info->channels % 2) || (reader->info->channels < 2) ||
	    (reader->info->channels > 10)) {
		dev_err(reader->dev, "%s: invalid nb of channels", __func__);
		return -EINVAL;
	}

	reader->channels_constraint.list = channels_2_10;
	reader->channels_constraint.count = reader->info->channels / 2;
	reader->channels_constraint.mask = 0;

	dev_dbg(reader->dev, "%d-channel PCM",
		reader->channels_constraint.count);

	*uni_reader = reader;

	/* ensure that disable by default */
	SET_UNIPERIF_CONFIG_BACK_STALL_REQ_DISABLE(reader);
	SET_UNIPERIF_CTRL_ROUNDING_OFF(reader);
	SET_UNIPERIF_CTRL_SPDIF_LAT_OFF(reader);
	SET_UNIPERIF_CONFIG_IDLE_MOD_DISABLE(reader);

	/* Registers view in ALSA's procfs */

#ifdef CONFIG_DEBUG_FS
	ret = uniperif_reader_info_register(snd_info_root,
					    dev_name(reader->dev), reader);
	if (ret < 0)
		dev_err(reader->dev, "Failed to register with procfs");
#endif

	return 0;
}

int uniperif_reader_remove(struct platform_device *pdev)
{
	struct uniperif *reader = platform_get_drvdata(pdev);

	dev_dbg(&pdev->dev, "%s(pdev=%p)", __func__, pdev);

	if (!reader) {
		dev_dbg(reader->dev, "invalid pointer(s)");
		return -EINVAL;
	}
	snd_info_free_entry(reader->entry);

	return 0;
}

MODULE_DESCRIPTION("uniperipheral reader IP management");
MODULE_AUTHOR("Arnaud Pouliquen <arnaud.pouliquen@st.com>");
MODULE_LICENSE("GPL v2");
