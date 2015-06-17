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
 * Implementation of v4l2 video decoder subdev
************************************************************************/
#include <linux/videodev2.h>

#include "stm_se.h"
#include "stmedia.h"
#include "linux/stm/stmedia_export.h"
#include "linux/stm_v4l2_export.h"
#include "stm_v4l2_decoder.h"
#include "stm_v4l2_video_decoder.h"
#include <linux/stm_v4l2_decoder_export.h>

#define MUTEX_INTERRUPTIBLE(mutex)	\
	if (mutex_lock_interruptible(mutex))	\
		return -ERESTARTSYS;

struct stm_video_data {
	u8 num_users;
	u32 src_type;
};

static struct stm_v4l2_decoder_context *stm_v4l2_rawvid_ctx;
static struct stm_video_data *stm_video_data;

/**
 * rawvid_connect_planes() - connect decoder to plane
 */
static int
rawvid_connect_planes(struct stm_v4l2_decoder_context *rawvid_ctx,
					const struct media_pad *disp_pad,
					u32 entity_type)
{
	int ret = 0, id = 0;
	bool search_all_planes = false;
	struct stmedia_v4l2_subdev *st_sd;

	/*
	 * If there's no remote, then, connect was deferred and we
	 * search for all the connected planes to this decoder
	 */
	if (!disp_pad)
		search_all_planes = true;

	do {
		if (search_all_planes) {

			disp_pad = stm_media_find_remote_pad_with_type
					(&rawvid_ctx->pads[RAW_VID_SRC_PAD],
					 MEDIA_LNK_FL_ENABLED,
					 entity_type,
					 &id);

			if (!disp_pad)
				break;
		}

		/*
		 * Get the display subdev to connect to decoder
		 */
		st_sd = entity_to_stmedia_v4l2_subdev(disp_pad->entity);

		/*
		 * Connect to decoder
		 */
		ret = stm_se_play_stream_attach(rawvid_ctx->play_stream,
						st_sd->stm_obj,
						STM_SE_PLAY_STREAM_OUTPUT_PORT_DEFAULT);

		/*
		 * Reconnection request received, no problem
		 */
		if (ret == -EALREADY)
			ret = 0;
		if(ret) {
			pr_err("%s(): raw video decoder -> display link failed\n", __func__);
			break;
		}
	} while (search_all_planes);

	return ret;
}

/**
 * rawvid_disconnect_planes() - disconnect decoder from plane
 */
static int
rawvid_disconnect_planes(struct stm_v4l2_decoder_context *rawvid_ctx,
					const struct media_pad *disp_pad,
					u32 entity_type)
{
	int ret = 0, id = 0;
	bool search_all_planes = false;
	struct stmedia_v4l2_subdev *st_sd;

	/*
	 * If there's no remote, then, connect was deferred and we
	 * search for all the connected planes to this decoder
	 */
	if (!disp_pad)
		search_all_planes = true;

	do {
		if (search_all_planes) {

			disp_pad = stm_media_find_remote_pad_with_type
					(&rawvid_ctx->pads[RAW_VID_SRC_PAD],
					MEDIA_LNK_FL_ENABLED,
					entity_type,
					&id);

			if (!disp_pad)
				break;
		}

		/*
		 * Get the display subdev to connect to decoder
		 */
		st_sd = entity_to_stmedia_v4l2_subdev(disp_pad->entity);

		/*
		 * Disconnect from decoder
		 */
		ret = stm_se_play_stream_detach(rawvid_ctx->play_stream,
						st_sd->stm_obj);
		/*
		 * Already disconnected? Huh, no problem
		 */
		if (ret == -ENOTCONN)
			ret = 0;
		if(ret) {
			pr_err("%s(): raw video decoder -> display link failed\n", __func__);
			break;
		}
	} while (search_all_planes);

	return ret;
}

/**
 * rawvid_connect_grabber() - create a playback and play_stream
 * @rawvid_ctx : Uncompressed video decoder context
 * The connect is called from link_setup twice, once explicitly
 * from source (dvp/compo) and then by media controller.
 */
static int rawvid_connect_grabber(struct stm_v4l2_decoder_context *rawvid_ctx,
						struct stmedia_v4l2_subdev *stm_sd)
{
	int ret = 0;
	char name[16];

	/*
	 * No-go ahead if already created
	 */
	if (rawvid_ctx->play_stream) {
		pr_debug("%s(): raw video play stream already created\n", __func__);
		goto connect_done;
	}

	/*
	 * Create a new raw video playstream
	 */
	snprintf(name, sizeof(name), "v4l2.video%d", rawvid_ctx->id);
	ret = stm_se_play_stream_new(name, rawvid_ctx->playback_ctx->playback,
				STM_SE_STREAM_ENCODING_VIDEO_UNCOMPRESSED,
				&rawvid_ctx->play_stream);
	if (ret) {
		pr_err("%s(): failed to create video play stream\n", __func__);
		goto connect_done;
	}
	stm_sd->stm_obj = rawvid_ctx->play_stream;

	/*
	 * Connect to plane now, if it has been requested earlier. This
	 * is a deferred connect because, the play_stream may not have been
	 * created when the connection to plane was requested. Since,
	 * connection is only possible with video/gfx, try both
	 */
	ret = rawvid_connect_planes(rawvid_ctx, NULL,
				MEDIA_ENT_T_V4L2_SUBDEV_PLANE_VIDEO);
	if (ret)
		pr_err("%s(): failed to connect decoder to video planes\n", __func__);

	ret = rawvid_connect_planes(rawvid_ctx, NULL,
				MEDIA_ENT_T_V4L2_SUBDEV_PLANE_GRAPHIC);
	if (ret)
		pr_err("%s(): failed to connect decoder to gdp planes\n", __func__);

	return 0;

connect_done:
	return ret;
}

/**
 * rawvid_disconnect_grabber() - disconnect from hdmirx
 */
static void rawvid_disconnect_grabber(struct stm_v4l2_decoder_context *rawvid_ctx,
						struct stmedia_v4l2_subdev *stm_sd)
{
	/*
	 * Since play_stream is going to destroyed now, so, disconnect
	 * it from planes i.e the sink for this play stream. As we are
	 * unaware what all are we connected to, video/graphic, so,
	 * disconnect all.
	 */
	if (rawvid_disconnect_planes(rawvid_ctx, NULL,
				MEDIA_ENT_T_V4L2_SUBDEV_PLANE_VIDEO))
		pr_err("%s(): failed to disconnect video planes\n", __func__);

	if (rawvid_disconnect_planes(rawvid_ctx, NULL,
				MEDIA_ENT_T_V4L2_SUBDEV_PLANE_GRAPHIC))
		pr_err("%s(): failed to disconnect gfx planes\n", __func__);

	/*
	 * Called again, so, no play_stream, we go out
	 */
	if (!rawvid_ctx->play_stream)
		goto done;

	/*
	 * Delete play stream and reset the context
	 */
	if (stm_se_play_stream_delete(rawvid_ctx->play_stream))
		pr_err("%s(): failed to delete play stream\n", __func__);
	rawvid_ctx->play_stream = NULL;
	stm_sd->stm_obj = NULL;

done:
	return;
}

/**
 * rawvid_capture_start() - start video capturing
 */
static int rawvid_capture_start(struct stm_v4l2_decoder_context *rawvid_ctx)
{
	int ret = 0, id = 0;
	struct media_pad *grab_pad;
	struct v4l2_subdev *grab_sd;
	struct stm_video_data *vid_data = rawvid_ctx->priv;

	/*
	 * Find the remote capture subdev
	 */
	grab_pad = stm_media_find_remote_pad_with_type
					(&rawvid_ctx->pads[RAW_VID_SNK_PAD],
					MEDIA_LNK_FL_ENABLED,
					vid_data->src_type,
					&id);
	if (!grab_pad) {
		pr_err("%s(): connect capture first, capture not started\n", __func__);
		ret = -ENOTCONN;
		goto start_failed;
	}

	/*
	 * Start streaming
	 */
	grab_sd = media_entity_to_v4l2_subdev(grab_pad->entity);
	ret = v4l2_subdev_call(grab_sd, video, s_stream, 1);
	if (ret)
		pr_err("%s(): failed to streamon capture\n", __func__);

start_failed:
	return ret;
}

/**
 * rawvid_capture_stop() - stop video capturing
 */
static int rawvid_capture_stop(struct stm_v4l2_decoder_context *rawvid_ctx)
{
	int ret = 0, id = 0;
	struct media_pad *grab_pad;
	struct v4l2_subdev *grab_sd;
	struct stm_video_data *vid_data = rawvid_ctx->priv;

	/*
	 * Find the remote capture subdev
	 */
	grab_pad = stm_media_find_remote_pad_with_type
					(&rawvid_ctx->pads[RAW_VID_SNK_PAD],
					MEDIA_LNK_FL_ENABLED,
					vid_data->src_type,
					&id);
	if (!grab_pad) {
		pr_err("%s(): connect capture first, capture not started\n", __func__);
		ret = -ENOTCONN;
		goto stop_failed;
	}

	/*
	 * Stop streaming
	 */
	grab_sd = media_entity_to_v4l2_subdev(grab_pad->entity);
	ret = v4l2_subdev_call(grab_sd, video, s_stream, 0);
	if (ret)
		pr_err("%s(): failed to streamon capture\n", __func__);

stop_failed:
	return ret;
}

/**
 * rawvid_subdev_core_ioctl() - subdev core ioctl op
 */
static long rawvid_subdev_core_ioctl(struct v4l2_subdev *sd,
					unsigned int cmd, void *arg)
{
	long ret = 0;
	struct stmedia_v4l2_subdev *stm_sd;
	struct stm_v4l2_decoder_context *rawvid_ctx = v4l2_get_subdevdata(sd);

	stm_sd = container_of(sd, struct stmedia_v4l2_subdev, sdev);

	/*
	 * FIXME: Bypass for now, but, we need to do sanity on ioctl
	 */

	MUTEX_INTERRUPTIBLE(&rawvid_ctx->mutex);

	switch(cmd) {
	case VIDIOC_SUBDEV_STI_STREAMON:

		/*
		 * Configure video capture
		 */
		ret = rawvid_connect_grabber(rawvid_ctx, stm_sd);
		if (ret) {
			pr_err("%s(): failed to setup the capture\n", __func__);
			break;
		}

		/*
		 * Start video capture
		 */
		ret = rawvid_capture_start(rawvid_ctx);
		if (ret) {
			rawvid_disconnect_grabber(rawvid_ctx, stm_sd);
			pr_err("%s(): failed to start video capture\n", __func__);
		}

		break;

	case VIDIOC_SUBDEV_STI_STREAMOFF:

		/*
		 * Stop capturing
		 */
		ret = rawvid_capture_stop(rawvid_ctx);
		if (ret)
			pr_err("%s(): failed to stop capture\n", __func__);

		/*
		 * Destroy the playback and the play stream
		 */
		rawvid_disconnect_grabber(rawvid_ctx, stm_sd);

		break;

	default:
		ret = -EINVAL;
		break;
	}

	mutex_unlock(&rawvid_ctx->mutex);

	return ret;
}

/*
 * raw video decoder subdev core ops
 */
static struct v4l2_subdev_core_ops rawvid_subdev_core_ops = {
	.g_ctrl = v4l2_subdev_g_ctrl,
	.s_ctrl = v4l2_subdev_s_ctrl,
	.g_ext_ctrls = v4l2_subdev_g_ext_ctrls,
	.s_ext_ctrls = v4l2_subdev_s_ext_ctrls,
	.ioctl = rawvid_subdev_core_ioctl,
};

/*
 * raw video decoder subdev ops
 */
static struct v4l2_subdev_ops rawvid_subdev_ops = {
	.core = &rawvid_subdev_core_ops,
};

/**
 * rawvid_link_setup() - link setup function
 */
static int rawvid_link_setup(struct media_entity *entity,
				const struct media_pad *local,
				const struct media_pad *remote, u32 flags)
{
	int ret = 0;
	struct stmedia_v4l2_subdev *stm_sd;
	struct stm_v4l2_decoder_context *rawvid_ctx;
	struct stm_video_data *vid_data;

	stm_sd = entity_to_stmedia_v4l2_subdev(entity);
	rawvid_ctx = v4l2_get_subdevdata(&stm_sd->sdev);
	vid_data = rawvid_ctx->priv;

	switch (remote->entity->type) {
	case MEDIA_ENT_T_V4L2_SUBDEV_DVP:
	case MEDIA_ENT_T_V4L2_SUBDEV_COMPO:
	{
		struct v4l2_ctrl *repeater;

		pr_err("%s(): <<IN process link Setup to DVP\n", __func__);

		repeater = v4l2_ctrl_find(stm_sd->sdev.ctrl_handler,
					V4L2_CID_MPEG_STI_HDMI_REPEATER_MODE);
		if (!repeater) {
			pr_err("%s(): failed to find the control for initial setup\n", __func__);
			break;
		}

		if (flags & MEDIA_LNK_FL_ENABLED) {

			MUTEX_INTERRUPTIBLE(&rawvid_ctx->mutex);

			/*
			 * Create playback in the link setup, so, that we
			 * can apply the repeater policy.
			 */
			rawvid_ctx->playback_ctx = stm_v4l2_decoder_create_playback(rawvid_ctx->id);
			if (!rawvid_ctx->playback_ctx) {
				pr_err("%s(): failed to create playback\n", __func__);
				ret = -ENOMEM;
				mutex_unlock(&rawvid_ctx->mutex);
				break;
			}

			/*
			 * Change the SE master clock to system clock
			 */
			ret = stm_se_playback_set_control(rawvid_ctx->playback_ctx->playback,
						STM_SE_CTRL_MASTER_CLOCK,
						STM_SE_CTRL_VALUE_SYSTEM_CLOCK_MASTER);
			if(ret) {
				pr_err("%s(): failed to set system clock as master clock of SE\n", __func__);
				mutex_unlock(&rawvid_ctx->mutex);
				break;
			}

			vid_data->src_type = remote->entity->type;

			mutex_unlock(&rawvid_ctx->mutex);

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

			MUTEX_INTERRUPTIBLE(&rawvid_ctx->mutex);

			/*
			 * Delete playback
			 */
			stm_v4l2_decoder_delete_playback(rawvid_ctx->id);
			vid_data->src_type = 0;
			mutex_unlock(&rawvid_ctx->mutex);
		}

		pr_err("%s(): <<OUT process link Setup to DVP\n", __func__);

		break;
	}

	case MEDIA_ENT_T_V4L2_SUBDEV_PLANE_VIDEO:
	case MEDIA_ENT_T_V4L2_SUBDEV_PLANE_GRAPHIC:
		/*
		 * Check from the sink if it's ready for connection
		 */
		pr_err("%s(): <<IN process link Setup to Planes\n", __func__);

		if (flags & MEDIA_LNK_FL_ENABLED) {
			ret = media_entity_call(remote->entity,
					link_setup, remote, local, flags);
			if (ret)
				break;
		}

		MUTEX_INTERRUPTIBLE(&rawvid_ctx->mutex);

		/*
		 * If there's no play_stream, then we can't do anything more than
		 * return a success and defer the connect. This is most probably
		 * the case, when decoder->plane is connected first in the pipeline.
		 */
		if (!rawvid_ctx->play_stream) {
			mutex_unlock(&rawvid_ctx->mutex);
			break;
		}

		/*
		 * Now, the actual connect/disconnect
		 */
		if (flags & MEDIA_LNK_FL_ENABLED)

			ret = rawvid_connect_planes(rawvid_ctx, remote,
							remote->entity->type);
		else
			ret = rawvid_disconnect_planes(rawvid_ctx, remote,
							remote->entity->type);

		mutex_unlock(&rawvid_ctx->mutex);

		pr_err("%s(): <<OUT process link Setup to Planes\n", __func__);

		break;

	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

static struct media_entity_operations rawvid_media_ops = {
	.link_setup = rawvid_link_setup,
};

/**
 * rawvid_internal_open() - subdev internal open
 */
static int rawvid_internal_open(struct v4l2_subdev *sd,
					struct v4l2_subdev_fh *fh)
{
	struct stm_v4l2_decoder_context *rawvid_ctx = v4l2_get_subdevdata(sd);
	struct stm_video_data *vid_data = (struct stm_video_data *)rawvid_ctx->priv;

	vid_data->num_users++;

	return 0;
}

/**
 * rawvid_internal_close() - subdev internal close
 */
static int rawvid_internal_close(struct v4l2_subdev *sd,
					struct v4l2_subdev_fh *fh)
{
	struct stm_v4l2_decoder_context *rawvid_ctx = v4l2_get_subdevdata(sd);
	struct stm_video_data *vid_data = (struct stm_video_data *)rawvid_ctx->priv;
	struct stmedia_v4l2_subdev *stm_sd = container_of(sd, struct stmedia_v4l2_subdev, sdev);

	/*
	 * Subdevice ioctl is not giving in any file handle in the
	 * callback to decide the ownership of streaming. So, an open
	 * count is maintained to decide when to stop streaming in close
	 */
	vid_data->num_users--;
	if (!vid_data->num_users) {
		rawvid_capture_stop(rawvid_ctx);
		rawvid_disconnect_grabber(rawvid_ctx, stm_sd);
	}

	return 0;
}

/*
 * rawvid subdev internal ops
 */
static struct v4l2_subdev_internal_ops rawvid_internal_ops = {
	.open = rawvid_internal_open,
	.close = rawvid_internal_close,
};

/**
 * rawvid_subdev_init() - subdev init for video decoder
 */
static int rawvid_subdev_init(struct stm_v4l2_decoder_context *rawvid_ctx)
{
	int ret = 0;
	struct v4l2_subdev *sd;
	struct media_entity *me;
	struct media_pad *pads;

	/*
	 * Init raw video decoder subdev with ops
	 */
	sd = &rawvid_ctx->stm_sd.sdev;
	v4l2_subdev_init(sd, &rawvid_subdev_ops);
	snprintf(sd->name, sizeof(sd->name), "v4l2.video%d", rawvid_ctx->id);
	v4l2_set_subdevdata(sd, rawvid_ctx);
	sd->flags = V4L2_SUBDEV_FL_HAS_DEVNODE;
	sd->internal_ops = &rawvid_internal_ops;

	pads = (struct media_pad *)kzalloc(sizeof(struct media_pad) *
					(RAW_VID_SRC_PAD + 1), GFP_KERNEL);
	if (!pads) {
		pr_err("%s(): out of memory for rawvid pads\n", __func__);
		ret = -ENOMEM;
		goto pad_alloc_failed;
	}
	rawvid_ctx->pads = pads;

	pads[RAW_VID_SNK_PAD].flags = MEDIA_PAD_FL_SINK;
	pads[RAW_VID_SRC_PAD].flags = MEDIA_PAD_FL_SOURCE;

	/*
	 * Register with media controller
	 */
	me = &sd->entity;
	ret = media_entity_init(me, RAW_VID_SRC_PAD + 1, pads, 0);
	if (ret) {
		pr_err("%s(): rawvid entity init failed\n", __func__);
		goto entity_init_failed;
	}
	me->ops = &rawvid_media_ops;
	me->type = MEDIA_ENT_T_V4L2_SUBDEV_VIDEO_DECODER;

	/*
	 * Init control handler for raw video decoder
	 */
	sd->ctrl_handler = &rawvid_ctx->ctrl_handler;
	ret = stm_v4l2_rawvid_ctrl_init(sd->ctrl_handler, rawvid_ctx);
	if (ret) {
		pr_err("%s(): failed to init rawvid ctrl handler\n", __func__);
		goto ctrl_init_failed;
	}

	/*
	 * Register v4l2 subdev
	 */
	ret = stm_media_register_v4l2_subdev(sd);
	if (ret) {
		pr_err("%s() : failed to register raw viddec subdev\n", __func__);
		goto subdev_register_failed;
	}

	return 0;

subdev_register_failed:
	stm_v4l2_viddec_ctrl_exit(sd->ctrl_handler);
ctrl_init_failed:
	media_entity_cleanup(me);
entity_init_failed:
	kfree(rawvid_ctx->pads);
pad_alloc_failed:
	return ret;
}

/*
 * rawvid_subdev_exit() - cleans up the raw video subdevice
 */
static void rawvid_subdev_exit(struct stm_v4l2_decoder_context *rawvid_ctx)
{
	stm_media_unregister_v4l2_subdev(&rawvid_ctx->stm_sd.sdev);

	media_entity_cleanup(&rawvid_ctx->stm_sd.sdev.entity);

	kfree(rawvid_ctx->pads);
}

/**
 * stm_v4l2_rawvid_subdev_init() - initialize the raw video decoder subdev
 */
static int __init stm_v4l2_rawvid_subdev_init(void)
{
	int ret = 0, i;
	struct stm_v4l2_decoder_context *rawvid_ctx;

	pr_debug("%s(): <<IN create subdev for raw video decoder\n", __func__);

	/*
	 * Allocate the raw video decoder context
	 */
	rawvid_ctx = kzalloc(sizeof(*rawvid_ctx) * V4L2_MAX_PLAYBACKS,
					GFP_KERNEL);
	if (!rawvid_ctx) {
		pr_err("%s(): out of memory for video context\n", __func__);
		ret = -ENOMEM;
		goto ctx_alloc_failed;
	}
	stm_v4l2_rawvid_ctx = rawvid_ctx;

	/*
	 * Allocate stm video private data
	 */
	stm_video_data = kzalloc(sizeof(*stm_video_data)
				* V4L2_MAX_PLAYBACKS, GFP_KERNEL);
	if (!stm_video_data) {
		pr_err("%s(): out of memory for video data\n", __func__);
		ret = -ENOMEM;
		goto data_alloc_failed;
	}

	/*
	 * Set the ids for the video contexts
	 */
	for (i = 0; i < V4L2_MAX_PLAYBACKS; i++) {
		mutex_init(&rawvid_ctx[i].mutex);
		rawvid_ctx[i].id = i;
		rawvid_ctx[i].priv = (void *)&stm_video_data[i];
		ret = rawvid_subdev_init(&rawvid_ctx[i]);
		if (ret) {
			pr_err("%s(): subdev(%d) init failed\n", __func__, i);
			goto subdev_init_failed;
		}
	}

	/*
	 * Create disabled links with dvp capture as source
	 * TODO: src_pad = 1
	 */
	ret = stm_v4l2_create_links(MEDIA_ENT_T_V4L2_SUBDEV_DVP, 1, NULL,
			MEDIA_ENT_T_V4L2_SUBDEV_VIDEO_DECODER, RAW_VID_SNK_PAD,
			"v4l2.video", !MEDIA_LNK_FL_ENABLED);
	if (ret) {
		pr_err("%s(): failed to create links from dvp\n", __func__);
		goto subdev_init_failed;
	}

	/*
	 * Create disabled links with compo capture as source
	 * TODO: src_pad = 1
	 */
	ret = stm_v4l2_create_links(MEDIA_ENT_T_V4L2_SUBDEV_COMPO, 1, NULL,
			MEDIA_ENT_T_V4L2_SUBDEV_VIDEO_DECODER, RAW_VID_SNK_PAD,
			"v4l2.video", !MEDIA_LNK_FL_ENABLED);
	if (ret) {
		pr_err("%s(): failed to create links from compo\n", __func__);
		goto subdev_init_failed;
	}

	/*
	 * Create Disabled links with video planes as sink
	 */
	ret = stm_v4l2_create_links(MEDIA_ENT_T_V4L2_SUBDEV_VIDEO_DECODER,
			RAW_VID_SRC_PAD, "v4l2.video",
			MEDIA_ENT_T_V4L2_SUBDEV_PLANE_VIDEO,
			0, NULL, !MEDIA_LNK_FL_ENABLED);
	if (ret) {
		pr_err("%s(): failed to create links to vdp planes\n", __func__);
		goto subdev_init_failed;
	}

	/*
	 * Create Disabled links with video planes as sink
	 */
	ret = stm_v4l2_create_links(MEDIA_ENT_T_V4L2_SUBDEV_VIDEO_DECODER,
			RAW_VID_SRC_PAD, "v4l2.video",
			MEDIA_ENT_T_V4L2_SUBDEV_PLANE_GRAPHIC,
			0, NULL, !MEDIA_LNK_FL_ENABLED);
	if (ret) {
		pr_err("%s(): failed to create links to gdp planes\n", __func__);
		goto subdev_init_failed;
	}

	pr_debug("%s(): OUT>> create subdev for raw video decoder\n", __func__);

	return 0;

subdev_init_failed:
	for (i = i - 1; i >= 0; i--)
		rawvid_subdev_exit(&rawvid_ctx[i]);
	kfree(stm_video_data);
data_alloc_failed:
	kfree(stm_v4l2_rawvid_ctx);
ctx_alloc_failed:
	return ret;
}

/**
 * stm_v4l2_rawvid_subdev_exit() - exit v4l2 video deocder subdev
 */
static void stm_v4l2_rawvid_subdev_exit(void)
{
	int i;

	for (i = 0; i < V4L2_MAX_PLAYBACKS; i++)
		rawvid_subdev_exit(&stm_v4l2_rawvid_ctx[i]);

	kfree(stm_v4l2_rawvid_ctx);
}

/**
 * stm_v4l2_video_decoder_init() - init v4l2 video decoder
 */
int __init stm_v4l2_video_decoder_init(void)
{
	/*
	 * For the moment only subdev to init
	 */
	return stm_v4l2_rawvid_subdev_init();
}

/**
 * stm_v4l2_video_decoder_exit() - exit v4l2 video decoder
 */
void stm_v4l2_video_decoder_exit(void)
{
	stm_v4l2_rawvid_subdev_exit();
}
