/*
 * sti_backend.c -- audio back end for STMicroelectronics sti platforms
 *
 * Copyright (C) ST-Microelectonics 2014
 *  Author: Arnaud Pouliquen <arnaud.pouliquen@st.com>,
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 */

#include <linux/module.h>
#include <linux/of.h>
#include <linux/delay.h>

#include <sound/soc.h>
#include <sound/pcm_params.h>
#include <sound/dmaengine_pcm.h>

#include "uniperif.h"

#define SLOT_MONO	0x0001
#define SLOT_STEREO	0x0003
#define SLOT_4CH	0x000F
#define SLOT_6CH	0x003F
#define SLOT_8CH	0x00FF

struct sti_backend_data {
	int stream;
	int (*init)(struct platform_device *pdev, struct uniperif **uni,
		    struct snd_info_entry *snd_info_root);
	int (*remove)(struct platform_device *pdev);
};

const struct sti_backend_data uni_player = {
	.stream = SNDRV_PCM_STREAM_PLAYBACK,
	.init = uniperif_player_init,
	.remove = uniperif_player_remove,
};

const struct sti_backend_data uni_reader = {
	.stream = SNDRV_PCM_STREAM_CAPTURE,
	.init = uniperif_reader_init,
	.remove = uniperif_reader_remove,
};

struct audio_backend {
	struct snd_soc_dai_driver *dai;
	struct uniperif *uni;
	const struct snd_pcm_hardware *hardware;
	struct snd_dmaengine_dai_dma_data dma_data;
	struct sti_backend_data dev_data;
};
#ifdef CONFIG_DEBUG_FS
static struct snd_info_entry *sti_info_root;
#endif

static const struct of_device_id snd_soc_sti_be_match[];

/*
 * PCM stream
 */
static bool sti_backend_dma_filter_fn(struct dma_chan *chan, void *fn_param)
{
	struct audio_backend *be = fn_param;
	struct uniperif *uni = be->uni;
	struct st_dma_audio_config *config = &uni->dma_config;

	if (!uni)
		return false;

	/* Check FDMA used fit with expected FDMA */
	if (uni->info->dma_np != chan->device->dev->of_node)
		return false;

	/* Setup this channel for audio operation */
	dev_dbg(uni->dev, "request_line %x",
		(unsigned int)uni->info->fdma_request_line);
	config->type = ST_DMA_TYPE_AUDIO;
	config->dma_addr = uni->fifo_phys_address;
	config->dreq_config.request_line = uni->info->fdma_request_line;
	config->dreq_config.direct_conn = uni->info->fdma_direct_conn;
	config->dreq_config.initiator = uni->info->fdma_initiator;
	config->dreq_config.increment = 0;
	config->dreq_config.hold_off = 0;
	config->dreq_config.maxburst = 1;
	config->dreq_config.buswidth = DMA_SLAVE_BUSWIDTH_4_BYTES;

	if (be->dev_data.stream == SNDRV_PCM_STREAM_PLAYBACK)
		config->dreq_config.direction = DMA_MEM_TO_DEV;
	else
		config->dreq_config.direction = DMA_DEV_TO_MEM;

	/* TODO to suppress when DMA standard */
	/* Set the default parking configuration */
	config->park_config.sub_periods = 1;
	config->park_config.buffer_size = 4;

	/* Save the channel config inside the channel structure */
	chan->private = config;
	dev_dbg(uni->dev, "chan->private %x", (unsigned int)config);

	dev_dbg(uni->dev, "Using FDMA '%s' channel %d",
		dev_name(chan->device->dev), chan->chan_id);

	return true;
}

/*
 * sti_backend_pcm_request_chan
 * This function is used to request dma channel associated to
 * the uniperipheral
 *
 */
struct dma_chan *sti_backend_pcm_request_chan(struct snd_soc_pcm_runtime *rtd)
{
	struct snd_soc_dai *dai = rtd->cpu_dai;
	struct audio_backend *be = snd_soc_dai_get_drvdata(dai);

	return snd_dmaengine_pcm_request_channel(sti_backend_dma_filter_fn, be);
}

/*
 * sti_backend_pcm_request_chan
 * This function is used to get the uniperipheral hardware constraint
 *
 */
const struct snd_pcm_hardware *sti_backend_get_pcm_hardware(
							struct snd_soc_dai *dai)
{
	struct audio_backend *be = snd_soc_dai_get_drvdata(dai);

	return be->hardware;
}

/*
 * sti_backend_pcm_create_dai_ctrl
 * This function is used to create Ctrl associated to DAI but also pcm device.
 * Request is done by front end to associate ctrl with pcm device id
 */
int sti_backend_pcm_create_dai_ctrl(struct snd_soc_pcm_runtime *rtd)
{
	struct snd_soc_dai *dai = rtd->cpu_dai;
	struct audio_backend *be = snd_soc_dai_get_drvdata(dai);
	struct uniperif *player	= be->uni;
	int i, ret = 0;

	dev_dbg(dai->dev, "%s: enter for pcm device %d\n",
		__func__, rtd->pcm->device);

	for (i = 0; i < player->num_ctrls; i++) {
		struct snd_kcontrol_new *ctrl = &player->snd_ctrls[i];
		ctrl->index = rtd->pcm->device;
		ctrl->device = rtd->pcm->device;

		ret = snd_ctl_add(dai->card->snd_card,
				snd_ctl_new1(ctrl, player));
		if (ret < 0) {
			dev_err(dai->dev, "%s: Failed to add %s: %d\n",
				__func__, ctrl->name, ret);
			return ret;
		}
	}

	return 0;
}

/*
 * DAI
 */
static int sti_backend_start(struct snd_pcm_substream *substream,
			     struct snd_soc_dai *dai)
{
	struct audio_backend *be = snd_soc_dai_get_drvdata(dai);
	struct uniperif *uni	= be->uni;

	dev_dbg(dai->dev, "%s: (substream=%p)", __func__, substream);

	/* start peripheral */
	if (uni->ops->start)
		return uni->ops->start(uni);

	return 0;
}

static int sti_backend_stop(struct snd_pcm_substream *substream,
			    struct snd_soc_dai *dai)
{
	struct audio_backend *be = snd_soc_dai_get_drvdata(dai);
	struct uniperif *uni = be->uni;

	dev_dbg(dai->dev, "%s: (substream=%p)", __func__, substream);

	if (!be || !uni) {
		dev_err(dai->dev, "%s: invalid context (be=%p) (uniperif=%p) ",
			__func__, be, uni);
		return -EINVAL;
	}
	if (uni->ops->stop)
		uni->ops->stop(uni);

	return 0;
}

static int sti_backend_dai_startup(struct snd_pcm_substream *substream,
				   struct snd_soc_dai *dai)
{
	struct audio_backend *be = snd_soc_dai_get_drvdata(dai);
	struct uniperif *uni = be->uni;

	if (!be || !dai || !be || !uni)
		return -EINVAL;

	dev_dbg(dai->dev, "%s: Enter for substream %x.\n", __func__,
		(unsigned int)substream);

	if (uni->ops->open)
		return uni->ops->open(uni);

	return 0;
}

static void sti_backend_dai_shutdown(struct snd_pcm_substream *substream,
				     struct snd_soc_dai *dai)
{
	struct audio_backend *be = snd_soc_dai_get_drvdata(dai);
	struct uniperif *uni = be->uni;

	if (!dai || !dai->dev) {
		dev_err(dai->dev, "%s: invalid pointer(s)", __func__);
		return;
	}
	dev_dbg(dai->dev, "%s: Enter for substream %x.\n", __func__,
		(unsigned int)substream);
	if (uni->ops->close)
		uni->ops->close(uni);
}

static int sti_backend_dai_prepare(struct snd_pcm_substream *substream,
				   struct snd_soc_dai *dai)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct audio_backend *be = snd_soc_dai_get_drvdata(dai);
	struct uniperif *uni = be->uni;

	dev_dbg(dai->dev, "%s: Enter for substream %x.\n", __func__,
		(unsigned int)substream);

	if (uni->ops->prepare)
		return uni->ops->prepare(uni, runtime);

	return 0;
}

static int sti_backend_dai_trigger(struct snd_pcm_substream *substream,
				   int cmd, struct snd_soc_dai *dai)
{
	dev_dbg(dai->dev, "%s: Enter for substream %x.\n", __func__,
		(unsigned int)substream);
	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		return sti_backend_start(substream, dai);
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
		return sti_backend_stop(substream, dai);
	default:
		return -EINVAL;
	}

	return 0;
}

static int sti_backend_dai_hw_params(struct snd_pcm_substream *substream,
				     struct snd_pcm_hw_params *params,
				     struct snd_soc_dai *dai)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct audio_backend *be = snd_soc_dai_get_drvdata(dai);
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	struct snd_dmaengine_dai_dma_data *dma_data = &be->dma_data;
	unsigned int fmt, pcm_format, tx_slots, rx_slots, freq;
	int channels, transfer_size, ret = 0, slots;
	bool is_playback;

	dev_dbg(dai->dev, "%s: Enter for substream %x.\n", __func__,
		(unsigned int)substream);

	/* Setup format */
	fmt = SND_SOC_DAIFMT_I2S | SND_SOC_DAIFMT_RIGHT_J |
	      SND_SOC_DAIFMT_CBS_CFS | SND_SOC_DAIFMT_CBS_CFS |
	      SND_SOC_DAIFMT_GATED;

	ret = snd_soc_dai_set_fmt(codec_dai, fmt);

	if (ret < 0) {
		dev_err(dai->dev,
			"%s: ERROR: failed for codec_dai (ret = %d)!\n",
			__func__, ret);
		return ret;
	}

	/* Setup I2S -slots */
	channels = params_channels(params);

	is_playback = (substream->stream == SNDRV_PCM_STREAM_PLAYBACK);

	switch (channels) {
	case 1:
		slots = 2;
		tx_slots = (is_playback) ? SLOT_MONO : 0;
		rx_slots = (is_playback) ? 0 : SLOT_MONO;
		break;
	case 2:
		slots = 2;
		tx_slots = (is_playback) ? SLOT_STEREO : 0;
		rx_slots = (is_playback) ? 0 : SLOT_STEREO;
		break;
	case 4:
		slots = 4;
		tx_slots = (is_playback) ? SLOT_4CH : 0;
		rx_slots = (is_playback) ? 0 : SLOT_4CH;
		break;
	case 6:
		slots = 6;
		tx_slots = (is_playback) ? SLOT_6CH : 0;
		rx_slots = (is_playback) ? 0 : SLOT_6CH;
		break;
	case 8:
		slots = 8;
		tx_slots = (is_playback) ? SLOT_8CH : 0;
		rx_slots = (is_playback) ? 0 : SLOT_8CH;
		break;
	default:
		return -EINVAL;
	}

	switch (params_format(params)) {
	case SNDRV_PCM_FORMAT_S32_LE:
		pcm_format = 32;
		break;

	case SNDRV_PCM_FORMAT_S16_LE:
		pcm_format = 16;
		break;

	default:
		return -EINVAL;
	}
	dev_dbg(dai->dev, "%s: CODEC-DAI TDM: TX=0x%04X RX=0x%04x\n", __func__,
		tx_slots, rx_slots);
	ret = snd_soc_dai_set_tdm_slot(codec_dai, tx_slots, rx_slots, slots,
				       pcm_format);
	if (ret < 0) {
		dev_err(dai->dev,
			"%s: ERROR: failed for codec tdm slot (ret = %d)!\n",
			__func__, ret);
		return ret;
	}
	/* set up rate */
	freq = params_rate(params);
	ret = snd_soc_dai_set_sysclk(codec_dai, 0, freq,
				substream->stream);
	if (ret < 0)
		dev_err(dai->dev,
			"%s: ERROR: failed for codec sysclk (ret = %d)!\n",
			__func__, ret);

	transfer_size = params_channels(params) * UNIPERIF_FIFO_FRAMES;

	dma_data = snd_soc_dai_get_dma_data(dai, substream);
	dma_data->maxburst = transfer_size;

	return ret;
}

static int sti_backend_dai_probe(struct snd_soc_dai *dai)
{
	struct audio_backend *be = snd_soc_dai_get_drvdata(dai);

	dev_dbg(dai->dev, "%s: Enter.\n", __func__);

	if (be->dev_data.stream == SNDRV_PCM_STREAM_PLAYBACK)
		dai->playback_dma_data = &be->dma_data;
	else
		dai->capture_dma_data = &be->dma_data;

	return 0;
}

static struct snd_soc_dai_ops sti_backend_dai_ops[] = {
	{
		.startup = sti_backend_dai_startup,
		.shutdown = sti_backend_dai_shutdown,
		.prepare = sti_backend_dai_prepare,
		.trigger = sti_backend_dai_trigger,
		.hw_params = sti_backend_dai_hw_params,
	}
};

static const struct snd_soc_dai_driver sti_backend_dai_template = {
	.probe = sti_backend_dai_probe,
	.ops = sti_backend_dai_ops,
};

static const struct snd_soc_component_driver sti_backend_component = {
	.name = "sti_backend_dai",
};


/*
 * Back end driver
 */

static int sti_backend_engine_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct device_node *pnode;
	struct audio_backend *be;
	struct snd_soc_pcm_stream *stream;

	dev_dbg(&pdev->dev, "%s: Enter.\n", __func__);

	/* Allocate memory for the info structure */
	be = devm_kzalloc(&pdev->dev, sizeof(struct audio_backend), GFP_KERNEL);
	if (!be)
		return -ENOMEM;

	/* Read the device properties */
	pnode = pdev->dev.of_node;
	if (!pnode)
		return -EINVAL;

	/* populate data structure depending on compatibility */
	if (!of_match_node(snd_soc_sti_be_match, pnode)->data) {
		dev_err(&pdev->dev, "%s: context data missing.\n", __func__);
		return -EINVAL;
	}

	memcpy(&be->dev_data, of_match_node(snd_soc_sti_be_match, pnode)->data,
	       sizeof(struct sti_backend_data));

	/* initialise uniperif */
#ifdef CONFIG_DEBUG_FS
	if (!sti_info_root) {
		/* first instance -> create procfs directory */
		sti_info_root = snd_info_create_module_entry(THIS_MODULE,
							     "uniperif", NULL);
		if (sti_info_root) {
			sti_info_root->mode = S_IFDIR | S_IRUGO | S_IXUGO;

			if (snd_info_register(sti_info_root) < 0) {
				snd_info_free_entry(sti_info_root);
				return -EINVAL;
			}
		} else
			return -ENOMEM;
	}
#endif
	if (!be->dev_data.init) {
		dev_err(&pdev->dev, "%s: context data missing.\n", __func__);
		return -EINVAL;
	}
	ret = be->dev_data.init(pdev, &be->uni, sti_info_root);

	if (ret < 0) {
		dev_err(&pdev->dev, "%s: ERROR: Fail to init uniperif ( %d)!\n",
			__func__, ret);
		return ret;
	}

	be->hardware = be->uni->hw;
	be->dai = devm_kzalloc(&pdev->dev,
			       sizeof(struct snd_soc_dai_driver),
			       GFP_KERNEL);
	*be->dai = sti_backend_dai_template;

	if (be->dev_data.stream == SNDRV_PCM_STREAM_PLAYBACK)
		stream = &be->dai->playback;
	else
		stream = &be->dai->capture;

	stream->stream_name = be->uni->info->name;
	stream->channels_min = be->hardware->channels_min;
	stream->channels_max = be->hardware->channels_max;
	stream->rates = be->hardware->rates;
	stream->formats = be->hardware->formats;

	be->dai->name = be->uni->info->name;

	/* DMA settings*/
	be->dma_data.addr = be->uni->fifo_phys_address;
	be->dma_data.addr_width = DMA_SLAVE_BUSWIDTH_4_BYTES;
	dev_dbg(&pdev->dev, "dma_data.addr '%#x'.\n", be->dma_data.addr);

	dev_dbg(&pdev->dev, "Register backend platform-DAI '%s'.\n",
		be->dai->name);

	dev_set_drvdata(&pdev->dev, be);

	ret = snd_soc_register_component(&pdev->dev, &sti_backend_component,
					 be->dai, 1);

	if (ret < 0) {
		dev_err(&pdev->dev,
			"ERROR: Failed to register DAI '%s' (ret= %d)!\n",
			be->dai->name, ret);
		return ret;
	}

	return 0;
}

static int sti_backend_engine_remove(struct platform_device *pdev)
{
	struct audio_backend *be = dev_get_drvdata(&pdev->dev);

	dev_dbg(&pdev->dev, "%s: Enter.\n", __func__);
	if (be->dev_data.remove)
		be->dev_data.remove(pdev);
	snd_soc_unregister_component(&pdev->dev);
#ifdef CONFIG_DEBUG_FS
	if (sti_info_root)
		snd_info_free_entry(sti_info_root);
#endif

	return 0;
}

static const struct of_device_id snd_soc_sti_be_match[] = {
	{
		.compatible = "st,snd_uni_player",
		.data = &uni_player,
	},
	{
		.compatible = "st,snd_uni_reader",
		.data = &uni_reader,
	},
	{},
};

static struct platform_driver sti_backend_driver = {
	.driver = {
		.name = "snd_uniperipheral",
		.owner = THIS_MODULE,
		.of_match_table = snd_soc_sti_be_match,
	},
	.probe = sti_backend_engine_probe,
	.remove = sti_backend_engine_remove
};
module_platform_driver(sti_backend_driver);

MODULE_DESCRIPTION("audio soc back end for STMicroelectronics platforms");
MODULE_AUTHOR("Arnaud Pouliquen <arnaud.pouliquen@st.com>");
MODULE_LICENSE("GPL v2");
