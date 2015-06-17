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
 * Implementation of compo subdev
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

/*
 * Definitions to find out the exact input of compo capture
 */
struct compo_mapping {
	char *disp_name;
	char *grab_name;
};

static const struct compo_mapping map_output_to_input[] = {
	{.disp_name = "Main-VID", .grab_name = "stm_input_video1"},
	{.disp_name = "Main-PIP", .grab_name = "stm_input_video2"},
	{.disp_name = "analog_sdout0", .grab_name = "stm_input_mix2"},
	{.disp_name = "analog_hdout0", .grab_name = "stm_input_mix1"},
	{.disp_name = NULL, .grab_name = NULL},
};

/**
 * compo_capture_video_s_stream() - enable/disable pixel capture
 */
static int compo_capture_video_s_stream(struct v4l2_subdev *sd, int enable)
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
 * compo_capture_subdev_s_dv_timings() - video subdev s_dv_timings callback
 * @sd     : compo subdev
 * @timings: timings as filled in by the caller
 * Sets up the input parameters for pixel capture. This has to be issued
 * before the video callback for s_mbus_fmt, as we store the dv_timings
 * first then setup the pixel capture params with a call to s_mbus_fmt.
 */
static int compo_capture_video_s_dv_timings(struct v4l2_subdev *sd,
					struct v4l2_dv_timings *timings)
{
	struct capture_context *cap_ctx = v4l2_get_subdevdata(sd);
	stm_pixel_capture_input_params_t *params = &cap_ctx->params;
	struct v4l2_bt_timings *bt = &timings->bt;
	uint64_t total_lines;

	pr_debug("%s(): <<IN setting the compo subdev timings\n", __func__);

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

	pr_debug("%s(): OUT>> setting the compo subdev timings\n", __func__);

	return 0;
}

/**
 * compo_capture_s_mbus_fmt() - s_mbus_fmt callback from hdmirx
 * @sd : compo subdev
 * @fmt: v4l2_mbus_framefmt
 * This will use the dv_timings set earlier and configure compo
 * input based on the remaining information propagated with this.
 * The source pad of compo which is used for capture configuration
 * will also be configured with the same properties now, but,
 * application can anytime over-ride it.
 */
static int compo_capture_video_s_mbus_fmt(struct v4l2_subdev *sd,
				struct v4l2_mbus_framefmt *fmt)
{
	int ret = 0;
	struct capture_context *cap_ctx = v4l2_get_subdevdata(sd);
	struct stmedia_v4l2_subdev *dec_stm_sd = NULL;
	stm_pixel_capture_input_params_t *params = &cap_ctx->params;
	stm_pixel_capture_buffer_format_t *buf_fmt = &cap_ctx->format;
	stm_pixel_capture_rect_t input_rect;

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

	params->color_space = map_v4l2_to_capture_colorspace(fmt->colorspace);
	/*
	 * FIXME: hard-coded for the moment
	 * params->pixel_format = map_v4l2_to_capture_pixelcode(fmt->code);
	 */
	params->pixel_format = STM_PIXEL_FORMAT_YCbCr422R;
	if (params->pixel_format < 0) {
		pr_err("%s(): this pixel code is not supported\n", __func__);
		goto s_mbus_fmt_done;
	}

	/*
	 * Program compo with input parameters
	 */
	ret = stm_pixel_capture_set_input_params(cap_ctx->pixel_capture, *params);
	if (ret) {
		pr_err("%s(): failed to s_fmt compo capture\n", __func__);
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
	 * Configure output pad now with the same properties
	 */
	buf_fmt->width = params->active_window.width;
	buf_fmt->height = params->active_window.height;
	buf_fmt->flags = params->flags;
	buf_fmt->stride = cap_ctx->format.stride;
	buf_fmt->color_space = params->color_space;
	buf_fmt->format = params->pixel_format;
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
 * compo_capture_pad_get_fmt() - pad get_fmt subdev callback
 * @sd    : compo subdev
 * @fh    : NULL for internal callback, non-NULL for app call
 * @format: format passed by app to fill in
 * Configure the capture output format
 */
static int compo_capture_pad_get_fmt(struct v4l2_subdev *sd,
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

get_fmt_done:
	mutex_unlock(&cap_ctx->lock);
	return ret;
}

/**
 * compo_capture_pad_set_fmt() - pad set_fmt subdev callback
 * @sd    : compo subdev
 * @fh    : NULL for internal callback, non-NULL for app call
 * @format: format as set in by the caller
 * Configure the capture output format
 */
static int compo_capture_pad_set_fmt(struct v4l2_subdev *sd,
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

/*
 * compo capture video ops
 */
static struct v4l2_subdev_video_ops compo_capture_subdev_video_ops = {
	.s_stream = compo_capture_video_s_stream,
	.s_dv_timings = compo_capture_video_s_dv_timings,
	.s_mbus_fmt = compo_capture_video_s_mbus_fmt,
};

/*
 * compo capture pad ops
 */
static struct v4l2_subdev_pad_ops compo_capture_subdev_pad_ops = {
	.get_fmt = compo_capture_pad_get_fmt,
	.set_fmt = compo_capture_pad_set_fmt,
};

/*
 * compo capture subdev operations
 */
static struct v4l2_subdev_ops compo_capture_subdev_ops = {
	.video = &compo_capture_subdev_video_ops,
	.pad = &compo_capture_subdev_pad_ops,
};

/**
 * compo_capture_link_setup() - link setup function for compo entity
 */
static int compo_capture_link_setup(struct media_entity *entity,
				const struct media_pad *local,
				const struct media_pad *remote, u32 flags)
{
	int ret = 0;
	struct capture_context *cap_ctx;

	cap_ctx = v4l2_get_subdevdata((media_entity_to_v4l2_subdev(local->entity)));

	MUTEX_INTERRUPTIBLE(&cap_ctx->lock);

	switch (remote->entity->type) {
	case MEDIA_ENT_T_V4L2_SUBDEV_PLANE_VIDEO:
	case MEDIA_ENT_T_V4L2_SUBDEV_OUTPUT:
	{
		int i;
		uint32_t id;
		char *name;

		pr_debug("%s(): hdmirx -> compo link setup okay\n", __func__);

		/*
		 * Map the input to compo to the particular input id
		 */
		for (i = 0; map_output_to_input[i].disp_name; i++) {
			if (!strcmp(remote->entity->name, map_output_to_input[i].disp_name))
				break;
		}

		if (!map_output_to_input[i].disp_name) {
			pr_err("%s(): unsupported input to compo found\n", __func__);
			ret = -EINVAL;
			break;
		}

		for (id = 0; ; id++) {
			ret = stm_pixel_capture_enum_inputs(cap_ctx->pixel_capture, id, (const char **)&name);
			if (ret || !strcmp(map_output_to_input[i].grab_name, name))
				break;
		}
		if (ret) {
			pr_err("%s(): no valid input found\n", __func__);
			break;
		}

		ret = stm_pixel_capture_set_input(cap_ctx->pixel_capture, id);
		if (ret)
			pr_err("%s(): set input to pixel capture\n", __func__);

		break;
	}

	case MEDIA_ENT_T_V4L2_SUBDEV_VIDEO_DECODER:
	{
		struct stmedia_v4l2_subdev *dec_stm_sd;
		struct v4l2_subdev *compo_sd;

		dec_stm_sd = entity_to_stmedia_v4l2_subdev(remote->entity);
		compo_sd = media_entity_to_v4l2_subdev(entity);

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

		break;
	}

	default:
		ret = -EINVAL;
	}

	mutex_unlock(&cap_ctx->lock);
	return ret;
}

/*
 * compo media link setup operation
 */
static const struct media_entity_operations compo_capture_media_ops = {
	.link_setup = compo_capture_link_setup,
};

/**
 * compo_capture_create_disabled_links() - create disabled links from compo
 * @src_entity : source entity type
 * @src_pad    : source pad number
 * @sink_entity: remote entity type
 * @sink_pad   : sink pad number
 * Create disabled links between different entities
 */
static int compo_capture_create_disabled_links(u32 src_entity, u16 src_pad,
					u32 sink_entity, u16 sink_pad)
{
	struct media_entity *src, *sink;
	int ret = 0;

	/*
	 * Create all possible disabled links from all sources to all sinks.
	 * a. Find the source and connect to all sinks
	 * b. Do this for all sources
	 */

	/*
	 * FIXME: This must be moved to stmvout as sink is the best to know
	 * which source to connect to, but, for the moment, it requires a
	 * module reordering which will be taken later
	 */
	src = stm_media_find_entity_with_type_first(src_entity);

	if (src_entity == MEDIA_ENT_T_V4L2_SUBDEV_OUTPUT) {
		if (src && strcmp(src->name, "analog_hdout0")) {
			do {
				src = stm_media_find_entity_with_type_next(src, src_entity);

			} while (src && strcmp(src->name, "analog_hdout0"));
		}

		sink = stm_media_find_entity_with_type_first(sink_entity);
		if (sink && src) {
			ret = media_entity_create_link(src, src_pad,
					sink, sink_pad, !MEDIA_LNK_FL_ENABLED);
			if (ret)
				pr_err("%s(): failed to create %s->%s\n", __func__, src->name, sink->name);
		}

		goto link_setup_failed;
	}

	while (src) {

		sink = stm_media_find_entity_with_type_first(sink_entity);
		while(sink) {

			ret = media_entity_create_link(src, src_pad,
					sink, sink_pad, !MEDIA_LNK_FL_ENABLED);
			if (ret) {
				pr_err("%s(): failed to create %s->%s\n", __func__, src->name, sink->name);
				goto link_setup_failed;
			}

			sink = stm_media_find_entity_with_type_next(sink, sink->type);
		}

		src = stm_media_find_entity_with_type_next(src, src_entity);
	}

link_setup_failed:
	return ret;
}

/**
 * compo_capture_subdev_init() - initialize the compo subdev
 * @cap_ctx: capture context for compo subdev
 */
int compo_capture_subdev_init(struct capture_context *cap_ctx)
{
	struct v4l2_subdev *sd = &cap_ctx->subdev;
	struct media_entity *me = &sd->entity;
	struct media_pad *pads;
	int ret = 0;

	mutex_init(&cap_ctx->lock);

	/*
	 * init subdev with ops
	 */
	v4l2_subdev_init(sd, &compo_capture_subdev_ops);
	snprintf(sd->name, sizeof(sd->name), "compo%d", cap_ctx->id);
	v4l2_set_subdevdata(sd, cap_ctx);
	sd->flags = V4L2_SUBDEV_FL_HAS_DEVNODE;

	pads = (struct media_pad *)kzalloc(sizeof(struct media_pad) *
					(COMPO_SRC_PAD + 1), GFP_KERNEL);
	if (!pads) {
		pr_err("%s(): out of memory for compo pads\n", __func__);
		goto pad_alloc_failed;
	}
	cap_ctx->pads = pads;

	pads[COMPO_SNK_PAD].flags = MEDIA_PAD_FL_SINK;
	pads[COMPO_SRC_PAD].flags = MEDIA_PAD_FL_SOURCE;

	/*
	 * Register with media entitty
	 */
	ret = media_entity_init(me, (COMPO_SRC_PAD + 1), pads, 0);
	if (ret < 0) {
		pr_err("%s(): compo entity init failed\n", __func__);
		goto entity_init_failed;
	}
	me->ops = &compo_capture_media_ops;
	me->type = MEDIA_ENT_T_V4L2_SUBDEV_COMPO;

	/*
	 * Register v4l2 subdev
	 */
	ret = stm_media_register_v4l2_subdev(sd);
	if (ret) {
		pr_err("%s() : failed to register compo subdev\n", __func__);
		goto entity_init_failed;
	}

	/*
	 * Open the compo capture
	 */
	ret = stm_pixel_capture_open(STM_PIXEL_CAPTURE_COMPO, 0, &cap_ctx->pixel_capture);
	if (ret) {
		pr_err("%s(): failed to open compo capture\n", __func__);
		goto compo_open_failed;
	}

	/* FIXME: Need to clarify the role of lock. Locking the compo
	 * here break the non-tunneled compo capture. Since, breakage
	 * not tested, so, better to leave out lock for the moment
	 */
#if 0
	/* Lock Capture for our usage */
	ret = stm_pixel_capture_lock(cap_ctx->pixel_capture);
	if (ret) {
		printk(KERN_ERR "%s: failed to lock the pixel capture(%d)\n",
				__func__, ret);
		goto compo_set_input_failed;
	}
#endif

	/*
	 * Create disabled links with planes
	 * TODO: src_pad = 1, and make a common module for these helpers
	 */
	ret = compo_capture_create_disabled_links(MEDIA_ENT_T_V4L2_SUBDEV_PLANE_VIDEO, 1,
					me->type, COMPO_SNK_PAD);
	if (ret) {
		pr_err("%s(): failed to create entity links to video\n", __func__);
		goto compo_link_create_failed;
	}

	pr_err("%s(): created disabled links to video plane\n", __func__);

	/*
	 * Create disabled links with graphic planes
	 */
	ret = compo_capture_create_disabled_links(MEDIA_ENT_T_V4L2_SUBDEV_PLANE_GRAPHIC, 1,
					me->type, COMPO_SNK_PAD);
	if (ret) {
		pr_err("%s(): failed to create entity links to graphic\n", __func__);
		goto compo_link_create_failed;
	}

	pr_err("%s(): created disabled links to gfx plane\n", __func__);

	/*
	 * Create disabled links with output
	 */
	ret = compo_capture_create_disabled_links(MEDIA_ENT_T_V4L2_SUBDEV_OUTPUT, 1,
					me->type, COMPO_SNK_PAD);
	if (ret) {
		pr_err("%s(): failed to create entity links to output\n", __func__);
		goto compo_link_create_failed;
	}

	pr_err("%s(): created disabled links to output\n", __func__);

	pr_debug("%s(): OUT>> create subdev for compo capture\n", __func__);

	return 0;

compo_link_create_failed:
#if 0
	stm_pixel_capture_unlock(cap_ctx->pixel_capture);
compo_set_input_failed:
#endif
	stm_pixel_capture_close(cap_ctx->pixel_capture);
compo_open_failed:
	stm_media_unregister_v4l2_subdev(sd);
entity_init_failed:
	kfree(cap_ctx->pads);
pad_alloc_failed:
	return ret;
}

/**
 * compo_capture_subdev_exit() - exit the compo subdev
 */
void compo_capture_subdev_exit(struct capture_context *cap_ctx)
{
	struct v4l2_subdev *sd = &cap_ctx->subdev;
	struct media_entity *me = &sd->entity;

#if 0
	stm_pixel_capture_unlock(cap_ctx->pixel_capture);
#endif

	stm_pixel_capture_close(cap_ctx->pixel_capture);

	stm_media_unregister_v4l2_subdev(sd);

	media_entity_cleanup(me);

	kfree(cap_ctx->pads);
}
