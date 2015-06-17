/*
 * Copyright(c) 2014 STMicroelectronics
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
#include <linux/slab.h>
#include <linux/wait.h>
#include <linux/io.h>
#include <linux/dma-mapping.h>
#include <linux/dmaengine.h>
#include <linux/platform_data/dma-ste-dma40.h>
#include <linux/workqueue.h>
#include <linux/timer.h>

#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/dmaengine_pcm.h>

#include "sti_backend.h"

#define STI_PLATFORM_PERIODS_BYTES_MAX	30720 /* 32 bit 192khz 8ch 10 ms*/
#define STI_PLATFORM_PERIODS_MAX	4
#define STI_PLATFORM_BUFFER_BYTES_MAX	(STI_PLATFORM_PERIODS_BYTES_MAX * \
					 STI_PLATFORM_PERIODS_MAX * 2)

struct sti_frontend_pcm_dma_params {
	unsigned int pos;
	struct dma_chan *dma_channel;
	dma_cookie_t dma_cookie;
	const struct snd_pcm_hardware *hardware;
};

/*
 * pcm platform driver
 */
static void sti_frontend_dma_complete(void *arg)
{
	struct snd_pcm_substream *substream = arg;
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct sti_frontend_pcm_dma_params *fe_params = runtime->private_data;

	fe_params->pos += snd_pcm_lib_period_bytes(substream);
	if (fe_params->pos >= snd_pcm_lib_buffer_bytes(substream))
		fe_params->pos = 0;

	snd_pcm_period_elapsed(substream);
}

static int sti_frontend_pcm_open(struct snd_pcm_substream *substream)
{
	int ret = 0;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct sti_frontend_pcm_dma_params *fe_params;
	struct snd_pcm_runtime *runtime = substream->runtime;

	pr_debug("%s: Enter for substream %p(%s).\n",
		 __func__, substream,
		 substream->stream == SNDRV_PCM_STREAM_PLAYBACK ?
		 "Playback" : "Capture");

	fe_params = kzalloc(sizeof(struct sti_frontend_pcm_dma_params),
			    GFP_KERNEL);

	if (fe_params == NULL)
		return -ENOMEM;

	fe_params->hardware = sti_backend_get_pcm_hardware(rtd->cpu_dai);

	ret = snd_soc_set_runtime_hwparams(substream, fe_params->hardware);

	if (ret < 0) {
		pr_err("error on FE hw_constraint\n");
		return ret;
	}

	/* Ensure that buffer size is a multiple of period size */
	ret = snd_pcm_hw_constraint_integer(runtime,
					    SNDRV_PCM_HW_PARAM_PERIODS);

	if (ret < 0) {
		pr_err("Error: snd_pcm_hw_constraints failed (%d)\n", ret);
		return ret;
	}

	fe_params->pos = 0;
	runtime->private_data = fe_params;
	fe_params->dma_channel = sti_backend_pcm_request_chan(rtd);

	if (!fe_params->dma_channel) {
		pr_err("Failed to request DMA channel");
		return -ENODEV;
	}

	return ret;
}

static int sti_frontend_pcm_hw_params(struct snd_pcm_substream *substream,
				      struct snd_pcm_hw_params *params)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct sti_frontend_pcm_dma_params *fe = runtime->private_data;
	struct dma_slave_config slave_config;
	int ret = 0;
	int size;

	pr_debug("%s: %s\n", __func__, substream->name);

	size = params_buffer_bytes(params);
	ret = snd_pcm_lib_malloc_pages(substream, size);

	if (ret < 0) {
		pr_err("Can't allocate pages!\n");
		return -ENOMEM;
	}

	snd_dmaengine_pcm_prepare_slave_config(substream, params,
					       &slave_config);
	ret = dmaengine_slave_config(fe->dma_channel, &slave_config);

	if (ret)
		pr_err("Failed to configure DMA channel");

	return ret;
}

static int sti_frontend_pcm_trigger(struct snd_pcm_substream *substream,
				    int cmd)
{
	int ret = 0;
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct sti_frontend_pcm_dma_params *fe_params = runtime->private_data;
	enum dma_transfer_direction direction;
	struct dma_async_tx_descriptor *desc;
	unsigned long flags = DMA_CTRL_ACK;

	pr_debug("%s: %s\n", __func__,  substream->name);

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		/* check that only one backend is connected */
		direction = snd_pcm_substream_to_dma_direction(substream);

		desc = dmaengine_prep_dma_cyclic(
				fe_params->dma_channel,	runtime->dma_addr,
				snd_pcm_lib_buffer_bytes(substream),
				snd_pcm_lib_period_bytes(substream),
				direction, flags);
		if (!desc) {
			pr_err("Failed to prepare DMA descriptor");
			return -ENOMEM;
		}

		/* Set the dma callback */
		desc->callback = sti_frontend_dma_complete;
		desc->callback_param = substream;

		/* Submit dma descriptor */
		fe_params->dma_cookie = dmaengine_submit(desc);
		break;

	case SNDRV_PCM_TRIGGER_RESUME:
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
		ret = dmaengine_terminate_all(fe_params->dma_channel);
		break;

	default:
		pr_debug("%s: ERROR: Invalid command in pcm trigger!\n",
			__func__);
		return -EINVAL;
	}
	return ret;
}

static snd_pcm_uframes_t sti_frontend_pcm_pointer(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct sti_frontend_pcm_dma_params *fe_params = runtime->private_data;

	return bytes_to_frames(substream->runtime, fe_params->pos);

}
static int sti_frontend_pcm_close(struct snd_pcm_substream *substream)
{
	struct sti_frontend_pcm_dma_params *fe_params;

	pr_debug("%s: Enter.\n", __func__);
	fe_params = substream->runtime->private_data;
	dma_release_channel(fe_params->dma_channel);
	kfree(fe_params);

	return 0;
}

static struct snd_pcm_ops sti_frontend_pcm_ops = {
	.open		= sti_frontend_pcm_open,
	.close		= sti_frontend_pcm_close,
	.hw_params	= sti_frontend_pcm_hw_params,
	.hw_free	= snd_pcm_lib_free_pages,
	.trigger	= sti_frontend_pcm_trigger,
	.pointer	= sti_frontend_pcm_pointer
};

int sti_frontend_pcm_new(struct snd_soc_pcm_runtime *rtd)
{
	struct snd_pcm *pcm = rtd->pcm;
	struct snd_card *card = rtd->card->snd_card;
	int ret = 0;

	dev_dbg(rtd->dev, "%s : enter\n", __func__);
	ret = sti_backend_pcm_create_dai_ctrl(rtd);
	if (ret < 0)
		return ret;
	else
		return snd_pcm_lib_preallocate_pages_for_all(
			pcm, SNDRV_DMA_TYPE_DEV, card->dev,
			STI_PLATFORM_BUFFER_BYTES_MAX,
			STI_PLATFORM_BUFFER_BYTES_MAX);
}

static int sti_frontend_probe(struct snd_soc_platform *platform)
{
	dev_dbg(platform->dev, "%s :\n", __func__);
	return 0;
}

static struct snd_soc_platform_driver sti_frontend_platform = {
	.ops		= &sti_frontend_pcm_ops,
	.probe		= sti_frontend_probe,
	.pcm_new	= sti_frontend_pcm_new,
};


static int sti_frontend_engine_probe(struct platform_device *pdev)
{
	int ret;

	dev_dbg(&pdev->dev, "%s: Enter.\n", __func__);

	/* Driver can not use snd_dmaengine_pcm_register.
	 * Reason is that DMA needs to load a firmware.
	 * This firmware must not be loaded during the probe,
	 * while filesystem is not mounted.
	 */
	ret = snd_soc_register_platform(&pdev->dev, &sti_frontend_platform);
	if (ret < 0)
		dev_err(&pdev->dev,
			"%s: error while register platform.  err = %d\n",
			__func__, ret);
	return ret;
}

static int sti_frontend_engine_remove(struct platform_device *pdev)
{
	snd_soc_unregister_platform(&pdev->dev);

	return 0;
}

static const struct of_device_id snd_soc_sti_match[] = {
	{ .compatible = "st,sti-asoc-platform", },
	{},
};

static struct platform_driver sti_frontend_driver = {
	.driver = {
		.name = "sti-asoc-platform",
		.owner = THIS_MODULE,
		.of_match_table = snd_soc_sti_match,
	},
	.probe = sti_frontend_engine_probe,
	.remove = sti_frontend_engine_remove
};
module_platform_driver(sti_frontend_driver);

MODULE_DESCRIPTION("audio soc front end for STMicroelectronics platforms");
MODULE_AUTHOR("Arnaud Pouliquen <arnaud.pouliquen@st.com>");
MODULE_LICENSE("GPL v2");
