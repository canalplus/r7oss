/************************************************************************
Copyright (C) 2005 STMicroelectronics. All Rights Reserved.

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

Source file name : dvb_v4l2_ctrls.c
Author :   Soby Mathew

Implementation of linux dvb v4l2 control framework
************************************************************************/
#include <linux/module.h>
#include <linux/dvb/video.h>
#include <linux/dvb/audio.h>
#include <linux/version.h>

#include "stmedia.h"
#include "stmedia_export.h"

#include "dvb_module.h"
#include "dvb_audio.h"
#include "dvb_video.h"
#include "display.h"
#include "dvb_v4l2_export.h"
#include "stmedia_export.h"

#define stv_err pr_err


static int stm_v4l2_capture_video_s_ctrl(struct v4l2_ctrl *ctrl)
{
	int ret = 0;
	struct VideoDeviceContext_s *video_dec = container_of(ctrl->handler,
			struct VideoDeviceContext_s, ctrl_handler);

	switch (ctrl->id) {
	case V4L2_CID_MPEG_STM_VIDEO_DECODER_BLANK:
		ret = DvbStreamEnable(video_dec->VideoStream, ctrl->val);
		if (ret < 0) {
			DVB_ERROR
			    ("DvbStreamEnable failed (ctrlvalue = %lu)\n",
			     (long unsigned int)
			     ctrl->val);
			ret = -EINVAL;
			break;
		}
		break;

	case V4L2_CID_STM_ASPECT_RATIO_CONV_MODE:
		ret = dvb_stm_display_set_aspect_ratio_conversion_mode
							(video_dec, ctrl->val);
		if (ret != 0)
			DVB_ERROR
			("dvb_stm_display_set_aspect_ratio_conversion_mode fail %d\n",
			     ret);
		break;

	case V4L2_CID_DV_STM_TX_ASPECT_RATIO:
		ret = dvb_stm_display_set_output_display_aspect_ratio
							(video_dec, ctrl->val);
		if (ret != 0)
			DVB_ERROR
			("dvb_stm_display_set_output_display_aspect_ratio fail %d\n",
			     ret);

		break;

	case V4L2_CID_STM_INPUT_WINDOW_MODE:
		ret = VideoSetInputWindowMode(video_dec, ctrl->val);
		if (ret != 0)
			DVB_ERROR("VideoSetInputWindowMode fail %d\n", ret);

		break;

	case V4L2_CID_STM_OUTPUT_WINDOW_MODE:
		ret = VideoSetOutputWindowMode(video_dec, ctrl->val);
		if (ret != 0)
			DVB_ERROR("VideoSetOutputWindowMode fail %d\n", ret);
		break;

	default:
		DVB_ERROR("Control Id not handled. \n");
		ret = -EINVAL;
		break;
	}

	return ret;
}

static int stm_v4l2_capture_video_g_ctrl(struct v4l2_ctrl *ctrl)
{
	int ret = 0;
	struct VideoDeviceContext_s *video_dec = container_of(ctrl->handler,
			struct VideoDeviceContext_s, ctrl_handler);

	switch (ctrl->id) {
	case V4L2_CID_STM_ASPECT_RATIO_CONV_MODE:
		ret = dvb_stm_display_get_aspect_ratio_conversion_mode(video_dec,
						&(ctrl->val));
		if (ret != 0)
			DVB_ERROR
			("dvb_stm_display_get_aspect_ratio_conversion_mode fail %d\n",
			     ret);
		break;

	case V4L2_CID_DV_STM_TX_ASPECT_RATIO:
		ret = dvb_stm_display_get_output_display_aspect_ratio(video_dec,
						     &(ctrl->val));
		if (ret != 0)
			DVB_ERROR
			("dvb_stm_display_get_output_display_aspect_ratio fail %d\n",
			     ret);
		break;

	case V4L2_CID_STM_INPUT_WINDOW_MODE:
		ret = VideoGetInputWindowMode(video_dec, &(ctrl->val));
		if (ret != 0)
			DVB_ERROR("VideoGetInputWindowMode fail %d\n", ret);

		break;

	case V4L2_CID_STM_OUTPUT_WINDOW_MODE:
		ret = VideoGetOutputWindowMode(video_dec, &(ctrl->val));
		if (ret != 0)
			DVB_ERROR("VideoGetOutputWindowMode fail %d\n", ret);

		break;
	case V4L2_CID_MIN_BUFFERS_FOR_CAPTURE:
	case V4L2_CID_MPEG_STM_FRAME_SIZE:
	{
		stm_se_play_stream_h  play_stream;
		stm_se_play_stream_decoded_frame_buffer_info_t  se_video_frames;

		if(video_dec->VideoStream && video_dec->VideoStream->Handle){
			play_stream = video_dec->VideoStream->Handle;
		}else {
			DVB_ERROR("Need a play_stream (VIDEO_PLAY miss ?)\n");
			return -EINVAL;
		}

		memset(&se_video_frames, '\0', sizeof(se_video_frames));

		ret = stm_se_play_stream_get_compound_control(
				play_stream,
				STM_SE_CTRL_NEGOTIATE_VIDEO_DECODE_BUFFERS,
				(void *)&se_video_frames);
		if (ret) {
			DVB_ERROR("stm_se_play_stream_get_compound_control failed!\n");
			return -EINVAL;
		}

		ctrl->val = 0;
		if (ctrl->id == V4L2_CID_MIN_BUFFERS_FOR_CAPTURE){
			ctrl->val = se_video_frames.number_of_buffers;
		} else if (ctrl->id == V4L2_CID_MPEG_STM_FRAME_SIZE) {
			ctrl->val = se_video_frames.buffer_size;
		}
		break;
	}
	case V4L2_CID_MPEG_STM_VIDEO_ELEMENTARY_BUFFER_SIZE:
	case V4L2_CID_MPEG_STM_VIDEO_ELEMENTARY_BUFFER_USED:
	{
		stm_se_play_stream_h  play_stream;
		stm_se_ctrl_play_stream_elementary_buffer_level_t es_buf_level;

		if(video_dec->VideoStream && video_dec->VideoStream->Handle){
			play_stream = video_dec->VideoStream->Handle;
		}else {
			stv_err("Need a play_stream (VIDEO_PLAY miss ?)\n");
			return -EINVAL;
		}

		memset(&es_buf_level, '\0', sizeof(es_buf_level));

		ret = stm_se_play_stream_get_compound_control(
				play_stream,
				STM_SE_CTRL_PLAY_STREAM_ELEMENTARY_BUFFER_LEVEL,
				(void *)&es_buf_level);
		if (ret) {
			stv_err("stm_se_play_stream_get_compound_control failed!\n");
			return -EINVAL;
		}

		ctrl->val = 0;
		if (ctrl->id == V4L2_CID_MPEG_STM_VIDEO_ELEMENTARY_BUFFER_SIZE){
			ctrl->val = es_buf_level.actual_size;
		} else if (ctrl->id == V4L2_CID_MPEG_STM_VIDEO_ELEMENTARY_BUFFER_USED) {
			ctrl->val = es_buf_level.bytes_used;
		}
		break;
	}
	default:
		stv_err("Control Id not handled. \n");
		ret = -EINVAL;
	}

	return ret;
}

static int stm_v4l2_capture_try_ctrl(struct v4l2_ctrl *ctrl)
{
	int ret = 0;
	union {
			s32 val;
			s64 val64;
			char *string;
		} temp_val;

	/* since the values in v4l2 layer are not in sync with those in the kpi, we update
	the current values by querying them here. This is required for the ctrl framework to
	decide to invoke s_ctrl */
	if(ctrl->flags & V4L2_CTRL_FLAG_VOLATILE){

		switch (ctrl->type) {
		case V4L2_CTRL_TYPE_STRING:
			temp_val.string = kzalloc(ctrl->maximum, GFP_KERNEL);
			/* strings are always 0-terminated */
			strcpy(temp_val.string, ctrl->string);
			break;
		case V4L2_CTRL_TYPE_INTEGER64:
			temp_val.val64 = ctrl->val64;
			break;
		default:
			temp_val.val = ctrl->val;
			break;
		}

		ret = ctrl->ops->g_volatile_ctrl(ctrl);
		switch (ctrl->type) {
		case V4L2_CTRL_TYPE_STRING:
			/* strings are always 0-terminated */
			strcpy(ctrl->cur.string, ctrl->string);
			strcpy(ctrl->string, temp_val.string);
			kfree(temp_val.string);
			break;
		case V4L2_CTRL_TYPE_INTEGER64:
			ctrl->cur.val64 = ctrl->val64;
			ctrl->val64 = temp_val.val64;
			break;
		default:
			ctrl->cur.val = ctrl->val;
			ctrl->val = temp_val.val;
			break;
		}
	}
	return ret;
}

static int stm_v4l2_capture_audio_g_ctrl(struct v4l2_ctrl *ctrl)
{
	int ret = 0;
	struct AudioDeviceContext_s *aud_ctx = container_of(ctrl->handler,
			struct AudioDeviceContext_s, ctrl_handler);

	switch (ctrl->id) {
	case V4L2_CID_MPEG_STM_AUDIO_ELEMENTARY_BUFFER_SIZE:
	case V4L2_CID_MPEG_STM_AUDIO_ELEMENTARY_BUFFER_USED:
	{
		stm_se_play_stream_h  play_stream;
		stm_se_ctrl_play_stream_elementary_buffer_level_t es_buf_level;

		if(aud_ctx->AudioStream && aud_ctx->AudioStream->Handle){
			play_stream = aud_ctx->AudioStream->Handle;
		}else {
			stv_err("Need a play_stream (AUDIO_PLAY miss ?)\n");
			return -EINVAL;
		}

		memset(&es_buf_level, '\0', sizeof(es_buf_level));

		ret = stm_se_play_stream_get_compound_control(
				play_stream,
				STM_SE_CTRL_PLAY_STREAM_ELEMENTARY_BUFFER_LEVEL,
				(void *)&es_buf_level);
		if (ret) {
			stv_err("stm_se_play_stream_get_compound_control failed!\n");
			return -EINVAL;
		}

		ctrl->val = 0;
		if (ctrl->id == V4L2_CID_MPEG_STM_AUDIO_ELEMENTARY_BUFFER_SIZE){
			ctrl->val = es_buf_level.actual_size;
		} else if (ctrl->id == V4L2_CID_MPEG_STM_AUDIO_ELEMENTARY_BUFFER_USED) {
			ctrl->val = es_buf_level.bytes_used;
		}
		break;
	}
	default:
		stv_err("Control Id not handled. \n");
		ret = -EINVAL;
	}

	return ret;
}

static const struct v4l2_ctrl_ops video_dec_ctrl_ops = {
	.try_ctrl = stm_v4l2_capture_try_ctrl,
	.s_ctrl = stm_v4l2_capture_video_s_ctrl,
	.g_volatile_ctrl = stm_v4l2_capture_video_g_ctrl,
};

static const struct v4l2_ctrl_ops audio_dec_ctrl_ops = {
	.try_ctrl = stm_v4l2_capture_try_ctrl,
	.g_volatile_ctrl = stm_v4l2_capture_audio_g_ctrl,
};

/*{{{  AudioInitCtrlHandler */
int AudioInitCtrlHandler(struct AudioDeviceContext_s *aud_ctx)
{
	int ret=0,i;

	struct v4l2_ctrl_config cfg[] = {
		{
			.ops = &audio_dec_ctrl_ops,
			.id = V4L2_CID_MPEG_STM_AUDIO_ELEMENTARY_BUFFER_SIZE,
			.name = "audio coded buffer size",
			.type = V4L2_CTRL_TYPE_INTEGER,
			.min = 0,
			.max = INT_MAX, /* signed integer max */
			.step = 1,
			.def = 0,
			.flags = V4L2_CTRL_FLAG_VOLATILE | V4L2_CTRL_FLAG_READ_ONLY,
		},
		{
			.ops = &audio_dec_ctrl_ops,
			.id = V4L2_CID_MPEG_STM_AUDIO_ELEMENTARY_BUFFER_USED,
			.name = "audio coded buffer in use",
			.type = V4L2_CTRL_TYPE_INTEGER,
			.min = 0,
			.max = INT_MAX, /* signed integer max */
			.step = 1,
			.def = 0,
			.flags = V4L2_CTRL_FLAG_VOLATILE | V4L2_CTRL_FLAG_READ_ONLY,
		},
	};

	ret = v4l2_ctrl_handler_init(&aud_ctx->ctrl_handler, ARRAY_SIZE(cfg));
	if(ret){
		stv_err("Control registration failed for audio ret = %d",ret);
		goto set_handler_failed;
	}

	for(i = 0; i <  ARRAY_SIZE(cfg); i++) {
		v4l2_ctrl_new_custom(&aud_ctx->ctrl_handler, &cfg[i], NULL);

		if (aud_ctx->ctrl_handler.error) {
			ret = aud_ctx->ctrl_handler.error;
			stv_err("Control registration failed for audio ret = %d",ret);
			goto new_custom_ctrl_failed;
		}
	}

	return 0;
new_custom_ctrl_failed:
	v4l2_ctrl_handler_free(&aud_ctx->ctrl_handler);
set_handler_failed:
	return ret;
}

/*{{{  VideoInitCtrlHandler */
int VideoInitCtrlHandler(struct VideoDeviceContext_s *Context)
{
	int ret = 0, i;
	struct v4l2_ctrl *ctrl = NULL;
	static const char * const dv_tx_aspect_ratio[] = {
		"Aspect ratio 16:9",
		"Aspect ratio 4:3",
		"Aspect ratio 14:9",
		NULL
	};

		static const char * const aspect_ratio_conv_mode[] = {
		"Aspect ratio conv letter box",
		"Aspect ratio conv pan & scan",
		"Aspect ratio conv combined",
		"Aspect ratio conv ignore",
		NULL
	};

	struct v4l2_ctrl_config cfg[] = {
		{
			.ops = &video_dec_ctrl_ops,
			.id = V4L2_CID_MPEG_STM_VIDEO_DECODER_BLANK,
			.name = "Decoder output to blank",
			.type = V4L2_CTRL_TYPE_BOOLEAN,
			.min = 0, /* 0 means hide */
			.max = 1, /* 1 means show */
			.step = 1,
			.def = 1, /* show for initial state, by default */
			.flags = 0,/* the read values are stored by ctrl framework*/
		},
		{
			.ops = &video_dec_ctrl_ops,
			.id = V4L2_CID_STM_ASPECT_RATIO_CONV_MODE,
			.name = "Plane aspect ratio conversion_mode",
			.type = V4L2_CTRL_TYPE_MENU,
			.min = VCSASPECT_RATIO_CONV_LETTER_BOX,
			.max = VCSASPECT_RATIO_CONV_IGNORE,
			.qmenu = aspect_ratio_conv_mode,
			.menu_skip_mask = 0,
			.def = 0, /* default does not matter for volatile*/
			.flags = V4L2_CTRL_FLAG_VOLATILE,
		},
		{
			.ops = &video_dec_ctrl_ops,
			.id = V4L2_CID_STM_INPUT_WINDOW_MODE,
			.name = "Input window mode",
			.type = V4L2_CTRL_TYPE_BOOLEAN,
			.min = 0,
			.max = 1,
			.step = 1,
			.def = 1, /* Auto mode is default */
			.flags = V4L2_CTRL_FLAG_VOLATILE,
		},
		{
			.ops = &video_dec_ctrl_ops,
			.id = V4L2_CID_STM_OUTPUT_WINDOW_MODE,
			.name = "Output window mode",
			.type = V4L2_CTRL_TYPE_BOOLEAN,
			.min = 0,
			.max = 1,
			.step = 1,
			.def = 1, /* Auto mode is default */
			.flags = V4L2_CTRL_FLAG_VOLATILE,
		},
		{
			.ops = &video_dec_ctrl_ops,
			.id = V4L2_CID_DV_STM_TX_ASPECT_RATIO,
			.name = "Output aspect ratio",
			.type = V4L2_CTRL_TYPE_MENU,
			.min = VCSOUTPUT_DISPLAY_ASPECT_RATIO_16_9,
			.max = VCSOUTPUT_DISPLAY_ASPECT_RATIO_14_9,
			.qmenu = dv_tx_aspect_ratio,
			.menu_skip_mask = 0,
			.def = 0,
			.flags = V4L2_CTRL_FLAG_VOLATILE,
		},
		{
			.ops = &video_dec_ctrl_ops,
			.id = V4L2_CID_MPEG_STM_FRAME_SIZE,
			.name = "Decoder frame size",
			.type = V4L2_CTRL_TYPE_INTEGER,
			.min = 0,
			.max = INT_MAX, /* signed integer max */
			.step = 1,
			.def = 0,
			.flags = V4L2_CTRL_FLAG_VOLATILE | V4L2_CTRL_FLAG_READ_ONLY,
		},
		{
			.ops = &video_dec_ctrl_ops,
			.id = V4L2_CID_MPEG_STM_VIDEO_ELEMENTARY_BUFFER_SIZE,
			.name = "coded buffer size",
			.type = V4L2_CTRL_TYPE_INTEGER,
			.min = 0,
			.max = INT_MAX, /* signed integer max */
			.step = 1,
			.def = 0,
			.flags = V4L2_CTRL_FLAG_VOLATILE | V4L2_CTRL_FLAG_READ_ONLY,
		},
		{
			.ops = &video_dec_ctrl_ops,
			.id = V4L2_CID_MPEG_STM_VIDEO_ELEMENTARY_BUFFER_USED,
			.name = "coded buffer in use",
			.type = V4L2_CTRL_TYPE_INTEGER,
			.min = 0,
			.max = INT_MAX, /* signed integer max */
			.step = 1,
			.def = 0,
			.flags = V4L2_CTRL_FLAG_VOLATILE | V4L2_CTRL_FLAG_READ_ONLY,
		},
	};


	ret = v4l2_ctrl_handler_init(&Context->ctrl_handler, ARRAY_SIZE(cfg));
	if(ret){
		printk(KERN_ERR"Control registration failed for video ret = %d",ret);
		return ret;
	}

	for(i = 0; i <  ARRAY_SIZE(cfg); i++)
		v4l2_ctrl_new_custom(&Context->ctrl_handler, &cfg[i], NULL);

	if (Context->ctrl_handler.error) {
		ret = Context->ctrl_handler.error;
		v4l2_ctrl_handler_free(&Context->ctrl_handler);
		printk(KERN_ERR"Control registration failed for video ret = %d",ret);
		return ret;
	}

	ctrl = v4l2_ctrl_new_std(&Context->ctrl_handler, &video_dec_ctrl_ops,
		V4L2_CID_MIN_BUFFERS_FOR_CAPTURE, 0, INT_MAX, 1, 0);

	if (Context->ctrl_handler.error) {
		ret = Context->ctrl_handler.error;
		printk(KERN_ERR"Control registration failed for video ret = %d",ret);
		v4l2_ctrl_handler_free(&Context->ctrl_handler);
		return ret;
	}

	/* strangely the framwework does not set volatile for read only controls.*/
	ctrl->flags |= V4L2_CTRL_FLAG_VOLATILE;



	return 0;
}




/*}}}*/
