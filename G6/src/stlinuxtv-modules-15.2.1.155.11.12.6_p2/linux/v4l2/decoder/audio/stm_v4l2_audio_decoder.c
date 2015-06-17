/************************************************************************
Copyright (C) 2014 STMicroelectronics. All Rights Reserved.

This file is part of the STLinuxTV Library.

STLinuxTV is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License version 2 as published by the
Free Software Foundation.

STLinuxTV is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with player2; see the file COPYING.  If not, write to the Free Software
Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

The STLinuxTV Library may alternatively be licensed under a proprietary
license from ST.
 * Implementation of v4l2 audio decoder subdev
************************************************************************/
#include <linux/module.h>
#include <linux/init.h>
#include <media/v4l2-subdev.h>
#include <linux/videodev2.h>

#include "stm_se.h"
#include "stmedia.h"
#include "linux/stm/stmedia_export.h"
#include "linux/stm_v4l2_export.h"
#include "stm_v4l2_audio_decoder.h"
#include "stm_v4l2_decoder.h"
#include "stm_se/audio_reader.h"
#include "player2_dvb/audio_out.h"
#include <linux/stm_v4l2_decoder_export.h>

#define MUTEX_INTERRUPTIBLE(mutex)	\
	if (mutex_lock_interruptible(mutex))	\
		return -ERESTARTSYS;

struct stm_audio_data {
	u8 num_users;
	stm_se_stream_encoding_t encoding;
	stm_se_audio_reader_h reader;
	stm_se_ctrl_audio_reader_source_info_t reader_source;
};

static struct stm_v4l2_decoder_context *stm_v4l2_auddec_ctx;
static struct stm_audio_data *stm_audio_data;

/**
 * auddec_convert_hdmirx_encoding_to_se() - convert hdmirx encoding to se equivalent
 */
static stm_se_stream_encoding_t
auddec_hdmirx_encoding_to_se(stm_hdmirx_audio_coding_type_t encoding)
{
	stm_se_stream_encoding_t se_encoding;

	switch(encoding) {
	case STM_HDMIRX_AUDIO_CODING_TYPE_NONE:
		se_encoding = STM_SE_STREAM_ENCODING_AUDIO_NONE;
		break;

	case STM_HDMIRX_AUDIO_CODING_TYPE_PCM:
		se_encoding = STM_SE_STREAM_ENCODING_AUDIO_PCM;
		break;

	case STM_HDMIRX_AUDIO_CODING_TYPE_AC3:
		se_encoding = STM_SE_STREAM_ENCODING_AUDIO_AC3;
		break;

	case STM_HDMIRX_AUDIO_CODING_TYPE_MPEG1:
		se_encoding = STM_SE_STREAM_ENCODING_AUDIO_MPEG1;
		break;

	case STM_HDMIRX_AUDIO_CODING_TYPE_MP3:
		se_encoding = STM_SE_STREAM_ENCODING_AUDIO_MP3;
		break;

	case STM_HDMIRX_AUDIO_CODING_TYPE_MPEG2:
		se_encoding = STM_SE_STREAM_ENCODING_AUDIO_MPEG2;
		break;

	case STM_HDMIRX_AUDIO_CODING_TYPE_AAC:
		se_encoding = STM_SE_STREAM_ENCODING_AUDIO_AAC;
		break;

	case STM_HDMIRX_AUDIO_CODING_TYPE_DTS:
		se_encoding = STM_SE_STREAM_ENCODING_AUDIO_DTS;
		break;

	case STM_HDMIRX_AUDIO_CODING_TYPE_ATRAC:
	case STM_HDMIRX_AUDIO_CODING_TYPE_DSD:
	case STM_HDMIRX_AUDIO_CODING_TYPE_DDPLUS:
	case STM_HDMIRX_AUDIO_CODING_TYPE_DTS_HD:
	case STM_HDMIRX_AUDIO_CODING_TYPE_MAT:
	case STM_HDMIRX_AUDIO_CODING_TYPE_DST:
	case STM_HDMIRX_AUDIO_CODING_TYPE_WMA_PRO:
	default:
		se_encoding = -EINVAL;

	}

	return se_encoding;
}

/**
 * auddec_capture_start() - create a new reader and start capture
 */
static int auddec_capture_start(struct stm_v4l2_decoder_context *aud_ctx)
{
	int ret = 0, i;
	char name[16], alsa_name[16];
	struct stm_audio_data *aud_data = aud_ctx->priv;

	/*
	 * Don't start capture again
	 */
	if (aud_data->reader) {
		pr_debug("%s(): capture already in progress\n", __func__);
		goto start_done;
	}

	/*
	 * Iterate over the list to find out the pcm capture device.
	 * FIXME: snd_find_minor() requires the information, which card
	 * and how many devices to iterate over, else, we may iterate
	 * less or over
	 */
	snprintf(name, sizeof(name), "v4l2.audreader%d", aud_ctx->id);
	for (i = 5; i >= 0; i--) {

		snprintf(alsa_name, sizeof(alsa_name), "hw,0,%d", i);

		ret = stm_se_audio_reader_new(name, alsa_name, &aud_data->reader);
		if (ret)
			pr_debug("%s(): hw,0,%d invalid alsa reader\n", __func__, i);
		else
			break;
	}

	if (ret) {
		pr_err("%s(): failed to create audio reader\n", __func__);
		goto start_done;
	}

	/*
	 * Attach reader to stream. We want to be ready before reader starts
	 * capturing any data.
	 */
	ret = stm_se_audio_reader_attach(aud_data->reader, aud_ctx->play_stream);
	if (ret) {
		pr_err("%s(): reader -> playstream attach failed\n", __func__);
		goto attach_failed;
	}

	/*
	 * Configure reader with the source info, so, it starts capturing
	 */
	ret = stm_se_audio_reader_set_compound_control(aud_data->reader,
					STM_SE_CTRL_AUDIO_READER_SOURCE_INFO,
					&aud_data->reader_source);
	if (ret) {
		pr_err("Audio reader source info set failed\n");
		goto set_source_info_failed;
	}

	/*
	 * Unmute the stream and let's roll
	 */
	ret = stm_se_play_stream_set_enable(aud_ctx->play_stream, 1);
	if (ret) {
		pr_err("%s(): failed to enable play stream\n", __func__);
		goto set_source_info_failed;
	}

	return 0;

set_source_info_failed:
	stm_se_audio_reader_detach(aud_data->reader, aud_ctx->play_stream);
attach_failed:
	stm_se_audio_reader_delete(aud_data->reader);
	aud_data->reader = NULL;
start_done:
	return ret;
}

/**
 * auddec_capture_stop() - stop capture and destroy reader
 */
static void auddec_capture_stop(struct stm_v4l2_decoder_context *aud_ctx)
{
	struct stm_audio_data *aud_data = aud_ctx->priv;

	/*
	 * If there's no audio reader, we already stopped
	 */
	if (!aud_data->reader)
		goto stop_done;

	/*
	 * Mute audio
	 */
	if (stm_se_play_stream_set_enable(aud_ctx->play_stream, 0))
		pr_err("%s(): failed to mute the play stream\n", __func__);

	/*
	 * Stop audio decode
	 */
	if (stm_se_audio_reader_detach(aud_data->reader, aud_ctx->play_stream))
		pr_err("%s(): failed to detach reader from play stream\n", __func__);

	/*
	 * Delete reader
	 */
	if (stm_se_audio_reader_delete(aud_data->reader))
		pr_err("%s(): failed to delete audio reader\n", __func__);

	aud_data->reader = NULL;

stop_done:
	return;
}

/**
 * auddec_connect_hdmirx() - link setup helper to connect to hdmirx
 */
static int auddec_connect_hdmirx(struct stm_v4l2_decoder_context *aud_ctx)
{
	int ret = 0;
	char name[24];

	/*
	 * If the play stream is already created, do not create again
	 */
	if (aud_ctx->play_stream) {
		pr_debug("%s(): audio play stream already exists\n", __func__);
		goto connect_done;
	}

	/*
	 * Create a new audio playstream
	 * FIXME: why encoding then?
	 */
	snprintf(name, sizeof(name), "v4l2.audio%d", aud_ctx->id);
	ret = stm_se_play_stream_new(name,
				aud_ctx->playback_ctx->playback,
				STM_SE_STREAM_ENCODING_AUDIO_SPDIFIN,
				&aud_ctx->play_stream);
	if (ret) {
		pr_err("%s(): failed to create video play stream\n", __func__);
		goto connect_done;
	}

	/*
	 * Enable AVSync now.
	 */
	ret = stm_se_play_stream_set_control(aud_ctx->play_stream,
					STM_SE_CTRL_ENABLE_SYNC,
					STM_SE_CTRL_VALUE_APPLY);
	if(ret) {
		pr_err("%s(): failed to set avsync on the playstream\n", __func__);
		goto set_control_failed;
	}

	/*
	 * Disable audio playstream
	 */
	ret = stm_se_play_stream_set_enable(aud_ctx->play_stream, 0);
	if (ret) {
		pr_err("%s(): failed to enable play stream\n", __func__);
		goto set_control_failed;
	}

	/*
	 * Try a deferred connect to mixer
	 */
	ret = stm_dvb_audio_connect_sink(&aud_ctx->pads[AUD_SRC_PAD],
						NULL, aud_ctx->play_stream);
	if (ret)
		pr_err("%s(): failed to connect decoder with sink\n", __func__);

	return 0;

set_control_failed:
	stm_se_play_stream_delete(aud_ctx->play_stream);
connect_done:
	return ret;
}

/**
 * auddec_disconnect_hdmirx() - disconnect from hdmirx
 */
static void auddec_disconnect_hdmirx(struct stm_v4l2_decoder_context *aud_ctx)
{
	/*
	 * Disconnect from mixer
	 */
	if (stm_dvb_audio_disconnect_sink(&aud_ctx->pads[AUD_SRC_PAD],
						NULL, aud_ctx->play_stream))
		pr_err("%s(): failed to disconnect decoder from sink\n", __func__);

	/*
	 * Stop capture
	 */
	auddec_capture_stop(aud_ctx);

	if (!aud_ctx->play_stream)
		goto done;

	/*
	 * Delete playstream
	 */
	if (stm_se_play_stream_delete(aud_ctx->play_stream))
		pr_err("%s(): failed to delete play stream\n", __func__);
	aud_ctx->play_stream = NULL;

done:
	return;
}

/**
 * auddec_subdev_core_ioctl() - subdev core ioctl op
 */
static long auddec_subdev_core_ioctl(struct v4l2_subdev *sd,
					unsigned int cmd, void *arg)
{
	long ret = 0;
	struct stm_hdmirx_audio_property_s *audio_prop;
	struct stm_v4l2_decoder_context *aud_ctx = v4l2_get_subdevdata(sd);
	struct stm_audio_data *aud_data = aud_ctx->priv;

	MUTEX_INTERRUPTIBLE(&aud_ctx->mutex);

	switch (cmd) {
	case VIDIOC_STI_S_AUDIO_FMT:

		audio_prop = (struct stm_hdmirx_audio_property_s *)arg;

		/*
		 * Get the source info for reader
		 */
		aud_data->encoding = auddec_hdmirx_encoding_to_se(audio_prop->coding_type);
		aud_data->reader_source.channel_count = audio_prop->channel_count;
		aud_data->reader_source.sampling_frequency = audio_prop->sampling_frequency;
		aud_data->reader_source.channel_alloc = audio_prop->channel_allocation;
		aud_data->reader_source.stream_type = audio_prop->stream_type;
		aud_data->reader_source.down_mix_inhibit = audio_prop->down_mix_inhibit;
		aud_data->reader_source.level_shift_value = audio_prop->level_shift;
		aud_data->reader_source.lfe_playback_level = audio_prop->lfe_pb_level;

		/*
		 * If the reader is already stopped, we don' restart.
		 * It means that there's a complete signal change, so,
		 * we let application start this.
		 */
		if (!aud_data->reader)
			break;

		/*
		 * Restart the audio capture
		 */
		auddec_capture_stop(aud_ctx);
		ret = auddec_capture_start(aud_ctx);
		if (ret)
			pr_err("%s(): failed to start audio capture\n", __func__);

		break;

	case VIDIOC_STI_S_AUDIO_INPUT_STABLE:

		/*
		 * Stop audio capture
		 */
		auddec_capture_stop(aud_ctx);

		break;

	case VIDIOC_SUBDEV_STI_STREAMON:

		/*
		 * Setup decoder
		 */
		ret = auddec_connect_hdmirx(aud_ctx);
		if (ret) {
			pr_err("%s(): failed to setup audio decoder\n", __func__);
			break;
		}

		/*
		 * Start capture
		 */
		ret = auddec_capture_start(aud_ctx);
		if (ret) {
			auddec_disconnect_hdmirx(aud_ctx);
			pr_err("%s(): failed to start audio capture\n", __func__);
		}

		break;

	case VIDIOC_SUBDEV_STI_STREAMOFF:

		/*
		 * Stop capturing
		 */
		auddec_capture_stop(aud_ctx);

		/*
		 * Clear the setup
		 */
		auddec_disconnect_hdmirx(aud_ctx);

		break;

	default:
		ret = -EINVAL;
		break;
	}

	mutex_unlock(&aud_ctx->mutex);

	return ret;
}

/*
 * Audio decoder subdev core ops
 */
static struct v4l2_subdev_core_ops auddec_subdev_core_ops = {
	.g_ctrl = v4l2_subdev_g_ctrl,
	.s_ctrl = v4l2_subdev_s_ctrl,
	.g_ext_ctrls = v4l2_subdev_g_ext_ctrls,
	.s_ext_ctrls = v4l2_subdev_s_ext_ctrls,
	.ioctl = auddec_subdev_core_ioctl,
};

/*
 * Audio decoder subdev ops
 */
static struct v4l2_subdev_ops auddec_subdev_ops = {
	.core = &auddec_subdev_core_ops,
};

/**
 * auddec_link_setup() - link setup function for audio decoder
 */
static int auddec_link_setup(struct media_entity *entity,
				const struct media_pad *local,
				const struct media_pad *remote, u32 flags)
{
	int ret = 0;
	struct v4l2_subdev *sd;
	struct stm_v4l2_decoder_context *aud_ctx;

	sd = media_entity_to_v4l2_subdev(entity);
	aud_ctx = v4l2_get_subdevdata(sd);

	switch (remote->entity->type) {
	case MEDIA_ENT_T_V4L2_SUBDEV_HDMIRX:
	{
		struct v4l2_ctrl *repeater;

		repeater = v4l2_ctrl_find(sd->ctrl_handler,
					V4L2_CID_MPEG_STI_HDMI_REPEATER_MODE);
		if (!repeater) {
			pr_err("%s(): failed to find the control for initial setup\n", __func__);
			break;
		}

		pr_debug("%s(): <<IN process link setup to hdmirx\n", __func__);


		if (flags & MEDIA_LNK_FL_ENABLED) {

			MUTEX_INTERRUPTIBLE(&aud_ctx->mutex);

			/*
			 * Create playback, we put the audio and video in the same
			 * playback for AV Sync. This is done here, to allow setting
			 * repeater policy
			 */
			aud_ctx->playback_ctx = stm_v4l2_decoder_create_playback(aud_ctx->id);
			if (!aud_ctx->playback_ctx) {
				pr_err("%s(): failed to create playback\n", __func__);
				ret = -ENOMEM;
				mutex_unlock(&aud_ctx->mutex);
				break;
			}

			/*
			 * Change the SE master clock to follow system clock
			 */
			ret = stm_se_playback_set_control(aud_ctx->playback_ctx->playback,
						STM_SE_CTRL_MASTER_CLOCK,
						STM_SE_CTRL_VALUE_SYSTEM_CLOCK_MASTER);
			if(ret) {
				pr_err("%s(): failed to set system clock as master clock of SE\n", __func__);
				mutex_unlock(&aud_ctx->mutex);
				break;
			}

			/*
			 * Set the limit to 1024 PPM for clock rates adjustment for the
			 * renderer. 10 sets the limit to 2^10 parts per million.
			 */
			ret = stm_se_playback_set_control(aud_ctx->playback_ctx->playback,
						STM_SE_CTRL_CLOCK_RATE_ADJUSTMENT_LIMIT,
						10);
			if(ret) {
				pr_err("%s(): cannot set clock rate adjustment to 2^10\n", __func__);
				mutex_unlock(&aud_ctx->mutex);
				break;
			}

			mutex_unlock(&aud_ctx->mutex);

			/*
			 * Set up the initial value of playback to non-repeater
			 */
			ret = v4l2_ctrl_s_ctrl(repeater, STM_SE_CTRL_VALUE_HDMIRX_NON_REPEATER);
			if (ret)
				pr_err("%s(): failed to set the non-repeater policy\n", __func__);

		} else {

			/*
			 * Reset the control handler to disable hdmirx capture
			 */
			ret = v4l2_ctrl_s_ctrl(repeater, STM_SE_CTRL_VALUE_HDMIRX_DISABLED);
			if (ret)
				pr_err("%s(): failed to set the non-repeater policy\n", __func__);

			MUTEX_INTERRUPTIBLE(&aud_ctx->mutex);

			stm_v4l2_decoder_delete_playback(aud_ctx->id);

			mutex_unlock(&aud_ctx->mutex);
		}

		pr_debug("%s(): OUT>> process link setup to hdmirx\n", __func__);

		break;
	}

	case MEDIA_ENT_T_ALSA_SUBDEV_PLAYER:
	case MEDIA_ENT_T_ALSA_SUBDEV_MIXER:

		pr_debug("%s(): <<IN process link setup to mixer\n", __func__);
		MUTEX_INTERRUPTIBLE(&aud_ctx->mutex);

		if (!aud_ctx->play_stream) {
			mutex_unlock(&aud_ctx->mutex);
			break;
		}

		if (flags & MEDIA_LNK_FL_ENABLED)
			ret = stm_dvb_audio_connect_sink(local,
						remote, aud_ctx->play_stream);
		else
			ret = stm_dvb_audio_disconnect_sink(local,
						remote, aud_ctx->play_stream);


		mutex_unlock(&aud_ctx->mutex);

		pr_debug("%s(): OUT>> process link setup to sink\n", __func__);

	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

static struct media_entity_operations auddec_media_ops = {
	.link_setup = auddec_link_setup,
};

/**
 * auddec_internal_open() - subdev internal open
 */
static int auddec_internal_open(struct v4l2_subdev *sd,
					struct v4l2_subdev_fh *fh)
{
	struct stm_v4l2_decoder_context *aud_ctx = v4l2_get_subdevdata(sd);
	struct stm_audio_data *aud_data = (struct stm_audio_data *)aud_ctx->priv;

	aud_data->num_users++;

	return 0;
}

/**
 * auddec_internal_close() - subdev internal close
 */
static int auddec_internal_close(struct v4l2_subdev *sd,
					struct v4l2_subdev_fh *fh)
{
	struct stm_v4l2_decoder_context *aud_ctx = v4l2_get_subdevdata(sd);
	struct stm_audio_data *aud_data = (struct stm_audio_data *)aud_ctx->priv;

	/*
	 * Subdevice ioctl is not giving in any file handle in the
	 * callback to decide the ownership of streaming. So, an open
	 * count is maintained to decide when to stop streaming
	 */
	aud_data->num_users--;
	if (!aud_data->num_users) {
		auddec_capture_stop(aud_ctx);
		auddec_disconnect_hdmirx(aud_ctx);
	}

	return 0;
}

/*
 * auddec subdev internal ops
 */
static struct v4l2_subdev_internal_ops auddec_internal_ops = {
	.open = auddec_internal_open,
	.close = auddec_internal_close,
};

/**
 * auddec_subdev_init() - subdev init for audio decoder
 */
static int auddec_subdev_init(struct stm_v4l2_decoder_context *auddec_ctx)
{
	int ret = 0;
	struct v4l2_subdev *sd;
	struct media_entity *me;
	struct media_pad *pads;

	/*
	 * Init raw audio decoder subdev with ops
	 */
	sd = &auddec_ctx->stm_sd.sdev;
	v4l2_subdev_init(sd, &auddec_subdev_ops);
	snprintf(sd->name, sizeof(sd->name), "v4l2.audio%d", auddec_ctx->id);
	v4l2_set_subdevdata(sd, auddec_ctx);
	sd->flags = V4L2_SUBDEV_FL_HAS_DEVNODE;
	sd->internal_ops = &auddec_internal_ops;

	pads = (struct media_pad *)kzalloc(sizeof(struct media_pad) *
					(AUD_SRC_PAD + 1), GFP_KERNEL);
	if (!pads) {
		pr_err("%s(): out of memory for auddec pads\n", __func__);
		ret = -ENOMEM;
		goto pad_alloc_failed;
	}
	auddec_ctx->pads = pads;

	pads[AUD_SNK_PAD].flags = MEDIA_PAD_FL_SINK;
	pads[AUD_SRC_PAD].flags = MEDIA_PAD_FL_SOURCE;

	/*
	 * Register with media controller
	 */
	me = &sd->entity;
	ret = media_entity_init(me, AUD_SRC_PAD + 1, pads, 0);
	if (ret) {
		pr_err("%s(): auddec entity init failed\n", __func__);
		goto entity_init_failed;
	}
	me->ops = &auddec_media_ops;
	me->type = MEDIA_ENT_T_V4L2_SUBDEV_AUDIO_DECODER;

	/*
	 * Init control handler for audio decoder
	 */
	sd->ctrl_handler = &auddec_ctx->ctrl_handler;
	ret = stm_v4l2_auddec_ctrl_init(sd->ctrl_handler, auddec_ctx);
	if (ret) {
		pr_err("%s(): failed to init auddec ctrl handler\n", __func__);
		goto ctrl_init_failed;
	}

	/*
	 * Register v4l2 subdev
	 */
	ret = stm_media_register_v4l2_subdev(sd);
	if (ret) {
		pr_err("%s() : failed to register audio decoder subdev\n", __func__);
		goto subdev_register_failed;
	}

	return 0;

subdev_register_failed:
	stm_v4l2_auddec_ctrl_exit(sd->ctrl_handler);
ctrl_init_failed:
	media_entity_cleanup(me);
entity_init_failed:
	kfree(auddec_ctx->pads);
pad_alloc_failed:
	return ret;
}

/*
 * auddec_subdev_exit() - cleans up the audio decoder subdevice
 */
static void auddec_subdev_exit(struct stm_v4l2_decoder_context *auddec_ctx)
{
	stm_media_unregister_v4l2_subdev(&auddec_ctx->stm_sd.sdev);

	media_entity_cleanup(&auddec_ctx->stm_sd.sdev.entity);

	kfree(auddec_ctx->pads);
}

/**
 * stm_v4l2_auddec_subdev_init() - initialize the audio decoder subdev
 */
static int __init stm_v4l2_auddec_subdev_init(void)
{
	int ret = 0, i, mixer_pad;
	struct stm_v4l2_decoder_context *auddec_ctx;

	pr_debug("%s(): <<IN create subdev for audio decoder\n", __func__);

	/*
	 * Allocate audio playback context
	 */
	auddec_ctx = kzalloc(sizeof(*auddec_ctx) * V4L2_MAX_PLAYBACKS,
					GFP_KERNEL);
	if (!auddec_ctx) {
		pr_err("%s(): out of memory for audio context\n", __func__);
		ret = -ENOMEM;
		goto ctx_alloc_failed;
	}
	stm_v4l2_auddec_ctx = auddec_ctx;

	/*
	 * Allocate stm audio private data
	 */
	stm_audio_data = kzalloc(sizeof(*stm_audio_data)
				* V4L2_MAX_PLAYBACKS, GFP_KERNEL);
	if (!stm_audio_data) {
		pr_err("%s(): out of memory for audio data\n", __func__);
		ret = -ENOMEM;
		goto data_alloc_failed;
	}

	/*
	 * Initialize the audio context
	 */
	for (i = 0; i < V4L2_MAX_PLAYBACKS; i++) {
		mutex_init(&auddec_ctx[i].mutex);
		auddec_ctx[i].id = i;
		auddec_ctx[i].priv = (void *)&stm_audio_data[i];
		ret = auddec_subdev_init(& auddec_ctx[i]);
		if (ret) {
			pr_err("%s(): subdev(%d) init failed\n", __func__, i);
			goto subdev_init_failed;
		}
	}

	/*
	 * Create disabled links with hdmirx capture as source
	 * TODO: src_pad = 2
	 */
	ret = stm_v4l2_create_links(MEDIA_ENT_T_V4L2_SUBDEV_HDMIRX, 2, NULL,
				MEDIA_ENT_T_V4L2_SUBDEV_AUDIO_DECODER, AUD_SNK_PAD,
				NULL, !MEDIA_LNK_FL_ENABLED);
	if (ret) {
		pr_err("%s(): failed to create links from hdmirx\n", __func__);
		goto subdev_init_failed;
	}

	/*
	 * Now, we have 4 input pads for mixer and 1 output, so, we create
	 * links for all input pads.
	 */
	for (mixer_pad = 0; mixer_pad < STM_SND_MIXER_PAD_OUTPUT; mixer_pad++) {

		ret = stm_v4l2_create_links(MEDIA_ENT_T_V4L2_SUBDEV_AUDIO_DECODER, AUD_SRC_PAD,
				NULL, MEDIA_ENT_T_ALSA_SUBDEV_MIXER, mixer_pad,
				NULL, !MEDIA_LNK_FL_ENABLED);
		if (ret) {
			pr_err("%s(): failed to create links to mixer\n", __func__);
			goto subdev_init_failed;
		}
	}

	/*
	 * The connection to planes is done during stlinuxtv.ko loading
	 */

	pr_debug("%s(): OUT>> create subdev for audio decoder\n", __func__);

	return 0;

subdev_init_failed:
	for (i = i - 1; i >= 0; i--)
		auddec_subdev_exit(&auddec_ctx[i]);
	kfree(stm_audio_data);
data_alloc_failed:
	kfree(stm_v4l2_auddec_ctx);
ctx_alloc_failed:
	return ret;
}

/**
 * stm_v4l2_rawvid_subdev_exit() - exit v4l2 video deocder subdev
 */
static void stm_v4l2_auddec_subdev_exit(void)
{
	int i;

	for (i = 0; i < V4L2_MAX_PLAYBACKS; i++)
		auddec_subdev_exit(&stm_v4l2_auddec_ctx[i]);

	kfree(stm_v4l2_auddec_ctx);
}

/**
 * stm_v4l2_audio_decoder_init() - init v4l2 audio decoder
 */
int __init stm_v4l2_audio_decoder_init(void)
{
	/*
	 * For the moment only subdev to init
	 */
	return stm_v4l2_auddec_subdev_init();
}

/**
 * stm_v4l2_audio_decoder_exit() - exit v4l2 audio decoder
 */
void stm_v4l2_audio_decoder_exit(void)
{
	stm_v4l2_auddec_subdev_exit();
}
