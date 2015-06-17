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
 * Implementation of dvp subdev
************************************************************************/
#include <media/v4l2-subdev.h>
#include <linux/videodev2.h>

#include "stm_se/types.h"
#include "stm_display_types.h"
#include "stm_display_source_pixel_stream.h"

#include "stmedia.h"
#include "linux/dvb/dvb_v4l2_export.h"
#include "linux/stm/stmedia_export.h"
#include "stm_v4l2_video_capture.h"
#include "stm_v4l2_decoder.h"

#define MUTEX_INTERRUPTIBLE(mutex)		\
	if (mutex_lock_interruptible(mutex))	\
		return -ERESTARTSYS;

/**
 * dvp_capture_video_s_stream() - enable/disable pixel capture
 * @sd    : dvp subdevice
 * @enable: enable/disable (1/0)
 * Due to play stream limitation on repeater policy, the attach
 * between play_stream and pixel capture is deferred to this call.
 * **NOTE**: This call if done from decoder, is always done after
 * connection discovery, so, the dec_pad is always valid.
 * If this call is done without discovery, then it is erroneous.
 */
static int dvp_capture_video_s_stream(struct v4l2_subdev *sd, int enable)
{
	int ret = 0;
	struct capture_context *cap_ctx = v4l2_get_subdevdata(sd);
	struct media_pad *dec_pad = cap_ctx->dec_pad;
	struct stmedia_v4l2_subdev *dec_stm_sd;

	/*
	 * Get remote stm_v4l2_subdev and access the play_stream
	 */
	dec_stm_sd = entity_to_stmedia_v4l2_subdev(dec_pad->entity);

	MUTEX_INTERRUPTIBLE(&cap_ctx->lock);

	if (enable)
		ret = stm_capture_streamon(cap_ctx, dec_stm_sd);
	else
		ret = stm_capture_streamoff(cap_ctx, dec_stm_sd);

	if (ret)
		pr_err("%s(): failed to %s pixel capture\n",
				__func__, enable ? "streamon" : "streamoff");

	mutex_unlock(&cap_ctx->lock);

	return ret;
}

/**
 * dvp_capture_subdev_s_dv_timings() - video subdev s_dv_timings callback
 * @sd     : dvp subdev
 * @timings: timings as filled in by the caller
 * Sets up the input parameters for pixel capture. This has to be issued
 * before the video callback for s_mbus_fmt, as we store the dv_timings
 * first then setup the pixel capture params with a call to s_mbus_fmt.
 */
static int dvp_capture_video_s_dv_timings(struct v4l2_subdev *sd,
					struct v4l2_dv_timings *timings)
{
	struct capture_context *cap_ctx = v4l2_get_subdevdata(sd);
	stm_pixel_capture_input_params_t *params = &cap_ctx->params;
	struct v4l2_bt_timings *bt = &timings->bt;
	uint64_t total_lines;

	pr_debug("%s(): <<IN setting the dvp subdev timings\n", __func__);

	MUTEX_INTERRUPTIBLE(&cap_ctx->lock);

	memset(params, 0, sizeof(cap_ctx->params));

	/*
	 * Calculate the htotal and vtotal for the input
	 */
	params->htotal = bt->width + bt->hsync + bt->hfrontporch + bt->hbackporch;
	params->vtotal = bt->height + bt->vsync + bt->vfrontporch + bt->vbackporch;

	/*
	 * Active area for display
	 */
	params->active_window.x = bt->hfrontporch;
	params->active_window.y = bt->vfrontporch;
	params->active_window.width = bt->width;
	params->active_window.height = bt->height;

	/*
	 * Can it really happen? no vtotal and htotal
	 */
	total_lines = (uint64_t)(params->htotal * params->vtotal);
	if (total_lines) {
		uint64_t pixelclock = bt->pixelclock * 1000;
		do_div(pixelclock, total_lines);
		params->src_frame_rate = pixelclock;
	}

	/*
	 * Interlaced or not? Which interlaced?
	 */
	params->flags = map_v4l2_to_capture_field(bt->interlaced);

	mutex_unlock(&cap_ctx->lock);

	pr_debug("%s(): OUT>> setting the dvp subdev timings\n", __func__);

	return 0;
}

/**
 * dvp_capture_s_mbus_fmt() - s_mbus_fmt callback from hdmirx
 * @sd : dvp subdev
 * @fmt: v4l2_mbus_framefmt
 * This will use the dv_timings set earlier and configure dvp
 * input based on the remaining information propagated with this.
 * The source pad of dvp which is used for capture configuration
 * will also be configured with the same properties now, but,
 * application can anytime over-ride it.
 */
static int dvp_capture_video_s_mbus_fmt(struct v4l2_subdev *sd,
				struct v4l2_mbus_framefmt *fmt)
{
	int ret = 0;
	struct capture_context *cap_ctx = v4l2_get_subdevdata(sd);
	struct stmedia_v4l2_subdev *dec_stm_sd = NULL;
	stm_pixel_capture_input_params_t *params = &cap_ctx->params;
	stm_pixel_capture_buffer_format_t *buf_fmt = &cap_ctx->format;
	stm_pixel_capture_rect_t input_rect;
	stm_pixel_capture_format_t *pix_fmt;

	MUTEX_INTERRUPTIBLE(&cap_ctx->lock);

	/*
	 * Called from link setup too and at that time, this is not set
	 */
	if (cap_ctx->dec_pad)
	        dec_stm_sd = entity_to_stmedia_v4l2_subdev(cap_ctx->dec_pad->entity);

	/*
	 * Stop pixel capture for configuring the input
	 */
	ret = stm_capture_streamoff(cap_ctx, dec_stm_sd);
	if (ret) {
		pr_err("%s(): failed to setup pixel capture for fmt change\n", __func__);
		goto s_mbus_fmt_done;
	}

	/*
	 * TODO: Square pixel as default
	 */
	params->pixel_aspect_ratio.numerator = 1;
	params->pixel_aspect_ratio.denominator = 1;

	/*
	 * Setup the input params.
	 */
	params->color_space = map_v4l2_to_capture_colorspace(fmt->colorspace);
	params->pixel_format = map_v4l2_to_capture_pixelcode(fmt->code);
	if (params->pixel_format == STM_PIXEL_FORMAT_NONE) {
		pr_err("%s(): this pixel code is not supported\n", __func__);
		goto s_mbus_fmt_done;
	}

	/*
	 * Program dvp with input parameters
	 */
	ret = stm_pixel_capture_set_input_params(cap_ctx->pixel_capture, *params);
	if (ret) {
		pr_err("%s(): failed to s_fmt dvp capture\n", __func__);
		goto s_mbus_fmt_done;
	}

	/*
	 * Configure input crop now with the same properties (no crop)
	 */
	input_rect.x = input_rect.y = 0;
	input_rect.width  = params->active_window.width;
	input_rect.height = params->active_window.height;
	ret = stm_pixel_capture_set_input_window(cap_ctx->pixel_capture, input_rect);
	if (ret) {
		pr_err("%s(): Unable to set input window (%d)\n", __func__, ret);
		goto s_mbus_fmt_done;
	}

	/*
	 * Configure dvp capture format with the first supported one
	 */
	ret = stm_pixel_capture_enum_image_formats(cap_ctx->pixel_capture,
					(const stm_pixel_capture_format_t **)&pix_fmt);
	if (ret < 0) {
		pr_err("%s(): failed to get supported formats\n", __func__);
		goto s_mbus_fmt_done;
	}

	/*
	 * Configure output pad now with the same properties
	 */
	buf_fmt->width = params->active_window.width;
	buf_fmt->height = params->active_window.height;

	/*
	 * DVP capture is always generating frames; it capture top and bottom fields
	 * then push complete frame to the play_stream interface. So we should be
	 * always configuring the capture buffer format for progressive content.
	 */
	buf_fmt->flags = 0;
	buf_fmt->stride = cap_ctx->format.stride;
	buf_fmt->color_space = params->color_space;
	buf_fmt->format = *pix_fmt;
	ret = stm_pixel_capture_set_format(cap_ctx->pixel_capture, *buf_fmt);
	if (ret) {
		pr_err("%s(): failed to set the format on capture pad\n", __func__);
		goto s_mbus_fmt_done;
	}

	/*
	 * Retrieve what has been set, which can be later pushed in pad->g_format
	 */
	ret = stm_pixel_capture_get_format(cap_ctx->pixel_capture,
							&cap_ctx->format);
	if (ret)
		pr_err("%s(): failed to get format on src pad\n", __func__);

s_mbus_fmt_done:
	mutex_unlock(&cap_ctx->lock);
	return ret;
}

/**
 * dvp_capture_pad_get_fmt() - pad get_fmt subdev callback
 * @sd    : dvp subdev
 * @fh    : NULL for internal callback, non-NULL for app call
 * @format: format passed by app to fill in
 * Configure the capture output format
 */
static int dvp_capture_pad_get_fmt(struct v4l2_subdev *sd,
		struct v4l2_subdev_fh *fh, struct v4l2_subdev_format *fmt)
{
	int ret = 0;
	struct capture_context *cap_ctx = v4l2_get_subdevdata(sd);

	if (fmt->pad != DVP_SRC_PAD) {
		pr_err("%s(): Wrong pad for output configuration\n", __func__);
		goto get_fmt_done;
	}

	MUTEX_INTERRUPTIBLE(&cap_ctx->lock);

	/*
	 * Update the application structure
	 */
	fmt->format.width = cap_ctx->format.width;
	fmt->format.height = cap_ctx->format.height;
	fmt->format.code = map_capture_to_v4l2_pixelcode(cap_ctx->format.format);
	fmt->format.field = map_capture_to_v4l2_field(cap_ctx->flags);
	fmt->format.colorspace = map_capture_to_v4l2_colorspace(cap_ctx->format.color_space);

	mutex_unlock(&cap_ctx->lock);

get_fmt_done:
	return ret;
}

/**
 * dvp_capture_pad_set_fmt() - pad set_fmt subdev callback
 * @sd    : dvp subdev
 * @fh    : NULL for internal callback, non-NULL for app call
 * @format: format as set in by the caller
 * Configure the capture output format
 */
static int dvp_capture_pad_set_fmt(struct v4l2_subdev *sd,
		struct v4l2_subdev_fh *fh, struct v4l2_subdev_format *fmt)
{
	int ret = 0;
	struct capture_context *cap_ctx = v4l2_get_subdevdata(sd);
	stm_pixel_capture_buffer_format_t buf_fmt;

	if (fmt->pad != DVP_SRC_PAD) {
		pr_err("%s(): Wrong pad for output configuration\n", __func__);
		goto set_fmt_done;
	}

	if (fmt->which == V4L2_SUBDEV_FORMAT_TRY) {
		pr_err("%s(): no fmt trying for now, only set\n", __func__);
		goto set_fmt_done;
	}

	memset(&buf_fmt, 0, sizeof(stm_pixel_capture_buffer_format_t));

	/*
	 * Get the stm pixel capture type of colorspace and pixel format info
	 */
	buf_fmt.color_space = map_v4l2_to_capture_colorspace(fmt->format.colorspace);
	buf_fmt.format = map_v4l2_to_capture_pixelcode(fmt->format.code);
	if (buf_fmt.format == STM_PIXEL_FORMAT_NONE) {
		pr_err("%s(): this pixel code is not supported\n", __func__);
		goto set_fmt_done;
	}

	buf_fmt.width = fmt->format.width;
	buf_fmt.height = fmt->format.height;
	buf_fmt.flags = map_v4l2_to_capture_field(fmt->format.field);

	MUTEX_INTERRUPTIBLE(&cap_ctx->lock);

	/*
	 * Set the format as requested by application
	 */
	ret = stm_pixel_capture_set_format(cap_ctx->pixel_capture, buf_fmt);
	if (ret) {
		pr_err("%s(): failed to set fmt of capture\n", __func__);
		goto set_fmt_unlock_done;
	}

	/*
	 * Retrieve the format set by the application and update driver state
	 */
	ret = stm_pixel_capture_get_format(cap_ctx->pixel_capture,
							&cap_ctx->format);
	if (ret) {
		pr_err("%s(): failed to get format\n", __func__);
		goto set_fmt_unlock_done;
	}

	/*
	 * Update the application structure
	 */
	fmt->format.width = cap_ctx->format.width;
	fmt->format.height = cap_ctx->format.height;
	fmt->format.reserved[0] = cap_ctx->format.stride;
	fmt->format.code = map_capture_to_v4l2_pixelcode(cap_ctx->format.format);
	fmt->format.field = map_capture_to_v4l2_field(cap_ctx->flags);
	fmt->format.colorspace = map_capture_to_v4l2_colorspace(cap_ctx->format.color_space);

set_fmt_unlock_done:
	mutex_unlock(&cap_ctx->lock);
set_fmt_done:
	return ret;
}

/**
 * dvp_capture_core_ioctl() - implements private ioctls
 */
static long dvp_capture_core_ioctl(struct v4l2_subdev *sd,
						unsigned int cmd, void *arg)
{
	long ret = 0;
	struct capture_context *cap_ctx = v4l2_get_subdevdata(sd);

	if (_IOC_DIR(cmd) != _IOC_WRITE) {
		ret = -EINVAL;
		goto core_ioctl_invalid;
	}


	MUTEX_INTERRUPTIBLE(&cap_ctx->lock);

	switch(cmd) {
	case VIDIOC_STI_S_VIDEO_INPUT_STABLE:
	{
		struct media_pad *dec_pad;
		struct stmedia_v4l2_subdev *dec_stm_sd;
		int id = 0;

		/*
		 * Find out if we have a connected decoder with dvp.
		 */
		dec_pad = stm_media_find_remote_pad_with_type
					(&cap_ctx->pads[DVP_SRC_PAD],
					MEDIA_LNK_FL_ENABLED,
					MEDIA_ENT_T_V4L2_SUBDEV_VIDEO_DECODER,
					&id);
		if (!dec_pad) {
			pr_debug("%s(): not connected to decoder, so, it is already off\n", __func__);
			break;
		}

		dec_stm_sd = entity_to_stmedia_v4l2_subdev(dec_pad->entity);

		/*
		 * Stop the capture from dvp
		 */
		ret = stm_capture_streamoff(cap_ctx, dec_stm_sd);
		if (ret)
			pr_err("%s(): failed to stop pixel capture\n", __func__);

		break;
	}

	default:
		ret = -ENOTTY;
		break;
	}

	mutex_unlock(&cap_ctx->lock);

core_ioctl_invalid:
	return ret;
}

/*
 * dvp capture core ops
 */
static struct v4l2_subdev_core_ops dvp_capture_subdev_core_ops = {
	.ioctl = dvp_capture_core_ioctl,
};

/*
 * dvp capture video ops
 */
static struct v4l2_subdev_video_ops dvp_capture_subdev_video_ops = {
	.s_stream = dvp_capture_video_s_stream,
	.s_dv_timings = dvp_capture_video_s_dv_timings,
	.s_mbus_fmt = dvp_capture_video_s_mbus_fmt,
};

/*
 * dvp capture pad ops
 */
static struct v4l2_subdev_pad_ops dvp_capture_subdev_pad_ops = {
	.get_fmt = dvp_capture_pad_get_fmt,
	.set_fmt = dvp_capture_pad_set_fmt,
};

/*
 * dvp capture subdev operations
 */
static struct v4l2_subdev_ops dvp_capture_subdev_ops = {
	.core = &dvp_capture_subdev_core_ops,
	.video = &dvp_capture_subdev_video_ops,
	.pad = &dvp_capture_subdev_pad_ops,
};

/**
 * dvp_capture_link_setup() - link setup function for dvp entity
 */
static int dvp_capture_link_setup(struct media_entity *entity,
				const struct media_pad *local,
				const struct media_pad *remote, u32 flags)
{
	int ret = 0;

	switch (remote->entity->type) {
	case MEDIA_ENT_T_V4L2_SUBDEV_HDMIRX:

		pr_debug("%s(): hdmirx -> dvp link setup okay\n", __func__);

		/*
		 * Nothing to do here for the moment, dvp has to receive
		 * the property of the signal from hdmirx and configure it's input
		 */
		break;

	case MEDIA_ENT_T_V4L2_SUBDEV_VIDEO_DECODER:
	{
		struct capture_context *cap_ctx;
		struct v4l2_subdev *dvp_sd;
		struct stmedia_v4l2_subdev *dec_stm_sd;

		dec_stm_sd = entity_to_stmedia_v4l2_subdev(remote->entity);
		dvp_sd = media_entity_to_v4l2_subdev(entity);
		cap_ctx = v4l2_get_subdevdata(dvp_sd);

		MUTEX_INTERRUPTIBLE(&cap_ctx->lock);

		/*
		 * Decoder setup is not to be done first, as the play stream
		 * creation is deferred, due to SE limitation on repeater policy
		 */
		if (flags & MEDIA_LNK_FL_ENABLED) {

			cap_ctx->dec_pad = (struct media_pad *)remote;

		} else {
			/*
			 * Streamoff and exit
			 */
			stm_capture_streamoff(cap_ctx, dec_stm_sd);
			cap_ctx->dec_pad = NULL;
		}

		mutex_unlock(&cap_ctx->lock);

		break;
	}

	default:
		ret = -EINVAL;
	}

	return ret;
}

/*
 * dvp media link setup operation
 */
static const struct media_entity_operations dvp_capture_media_ops = {
	.link_setup = dvp_capture_link_setup,
};

/**
 * dvp_capture_create_disabled_links() - create disabled links from dvp
 * @src_entity_type: source entity type
 * @src_pad        : source pad number
 * @sink           : dvp entity
 * @sink_pad       : sink pad number
 * Create disabled links from all dvp sinks to all possible dvp sources.
 * The source is identified by first parameter @src_entity_type
 */
static int dvp_capture_create_disabled_links(u32 src_entity_type, u16 src_pad,
					struct media_entity *sink, u16 sink_pad)
{
	struct media_entity *src, *next_sink = sink;
	int ret = 0;

	/*
	 * Create all possible disabled links from all sources to all sinks.
	 * a. Find the source and connect to all sinks
	 * b. Do this for all sources
	 */
	src = stm_media_find_entity_with_type_first(src_entity_type);
	while (src) {

		while(next_sink) {

			ret = media_entity_create_link(src, src_pad,
					next_sink, sink_pad, !MEDIA_LNK_FL_ENABLED);
			if (ret) {
				pr_err("%s(): failed to create hdmirx->dvp\n", __func__);
				goto link_setup_failed;
			}

			next_sink = stm_media_find_entity_with_type_next(next_sink,
								next_sink->type);
		}

		src = stm_media_find_entity_with_type_next(src, src_entity_type);
	}

link_setup_failed:
	return ret;
}

/**
 * dvp_capture_subdev_init() - initialize the dvp subdev
 * @cap_ctx: capture context for dvp subdev
 */
int dvp_capture_subdev_init(struct capture_context *cap_ctx)
{
	struct v4l2_subdev *sd = &cap_ctx->subdev;
	struct media_entity *me = &sd->entity;
	struct media_pad *pads;
	int ret = 0;

	mutex_init(&cap_ctx->lock);

	/*
	 * Init subdev with ops
	 */
	v4l2_subdev_init(sd, &dvp_capture_subdev_ops);
	snprintf(sd->name, sizeof(sd->name), "dvp%d", cap_ctx->id);
	v4l2_set_subdevdata(sd, cap_ctx);
	sd->flags = (V4L2_SUBDEV_FL_HAS_EVENTS | V4L2_SUBDEV_FL_HAS_DEVNODE);

	pads = (struct media_pad *)kzalloc(sizeof(struct media_pad) *
					(DVP_SRC_PAD + 1), GFP_KERNEL);
	if (!pads) {
		pr_err("%s(): out of memory for dvp pads\n", __func__);
		goto pad_alloc_failed;
	}
	cap_ctx->pads = pads;

	pads[DVP_SNK_PAD].flags = MEDIA_PAD_FL_SINK;
	pads[DVP_SRC_PAD].flags = MEDIA_PAD_FL_SOURCE;

	/*
	 * Register with media entitty
	 */
	ret = media_entity_init(me, (DVP_SRC_PAD + 1), pads, 0);
	if (ret < 0) {
		pr_err("%s(): dvp entity init failed\n", __func__);
		goto entity_init_failed;
	}
	me->ops = &dvp_capture_media_ops;
	me->type = MEDIA_ENT_T_V4L2_SUBDEV_DVP;

	/*
	 * Register v4l2 subdev
	 */
	ret = stm_media_register_v4l2_subdev(sd);
	if (ret) {
		pr_err("%s() : failed to register dvp subdev\n", __func__);
		goto entity_init_failed;
	}

	/*
	 * Open the dvp capture
	 */
	ret = stm_pixel_capture_open(STM_PIXEL_CAPTURE_DVP, 0, &cap_ctx->pixel_capture);
	if (ret) {
		pr_err("%s(): failed to open dvp capture\n", __func__);
		goto dvp_open_failed;
	}

	/*
	 * Setup the input
	 */
	ret = stm_pixel_capture_set_input(cap_ctx->pixel_capture, cap_ctx->id);
	if (ret) {
		printk(KERN_ERR "%s: failed to set cap_ctx (%d)\n",
				__func__, ret);
		goto dvp_set_input_failed;
	}

	/* Lock Capture for our usage */
	ret = stm_pixel_capture_lock(cap_ctx->pixel_capture);
	if (ret) {
		printk(KERN_ERR "%s: failed to lock the pixel capture(%d)\n",
				__func__, ret);
		goto dvp_set_input_failed;
	}

	/*
	 * Create disabled links with hdmirx
	 * TODO: src_pad = 1, and make a common module for these helpers
	 */
	ret = dvp_capture_create_disabled_links(MEDIA_ENT_T_V4L2_SUBDEV_HDMIRX, 1,
					me, DVP_SNK_PAD);
	if (ret) {
		pr_err("%s(): failed to create media entity links\n", __func__);
		goto dvp_link_create_failed;
	}

	pr_debug("%s(): OUT>> create subdev for dvp capture\n", __func__);


	return 0;

dvp_link_create_failed:
	stm_pixel_capture_unlock(cap_ctx->pixel_capture);
dvp_set_input_failed:
	stm_pixel_capture_close(cap_ctx->pixel_capture);
dvp_open_failed:
	stm_media_unregister_v4l2_subdev(sd);
entity_init_failed:
	kfree(cap_ctx->pads);
pad_alloc_failed:
	return ret;
}

/**
 * dvp_capture_subdev_exit() - exit the dvp subdev
 */
void dvp_capture_subdev_exit(struct capture_context *cap_ctx)
{
	struct v4l2_subdev *sd = &cap_ctx->subdev;
	struct media_entity *me = &sd->entity;

	stm_pixel_capture_unlock(cap_ctx->pixel_capture);

	stm_pixel_capture_close(cap_ctx->pixel_capture);

	stm_media_unregister_v4l2_subdev(sd);

	media_entity_cleanup(me);

	kfree(cap_ctx->pads);
}
