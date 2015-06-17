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
 * Implementation of common routines between dvp and compo subdev
************************************************************************/

#include <linux/videodev2.h>
#include <linux/v4l2-mediabus.h>
#include <linux/errno.h>

#include "stm_pixel_capture.h"
#include "stm_v4l2_video_capture.h"
#include "linux/dvb/dvb_v4l2_export.h"

/**
 * stm_capture_streaming() - returns true if streaming
 */
bool stm_capture_streaming(struct capture_context *cap_ctx)
{
	bool streaming = false;
	stm_pixel_capture_status_t status;

	/*
	 * Get the status of pixel capture
	 */
	if (stm_pixel_capture_get_status(cap_ctx->pixel_capture, &status)) {
		pr_err("%s(): cannot retrieve pixel capture status\n", __func__);
		goto get_status_done;
	}

	if ((status & STM_PIXEL_CAPTURE_STARTED) == STM_PIXEL_CAPTURE_STARTED)
		streaming = true;

get_status_done:
	return streaming;
}

/**
 * stm_capture_streamoff() - disable capture and detach from play_stream
 * @cap_ctx     : capture context (dvp or compo)
 * @play_stream : decoder handle
 */
int stm_capture_streamoff(struct capture_context *cap_ctx,
					struct stmedia_v4l2_subdev *dec_stm_sd)
{
	int ret = 0;

	/*
	 * Stop the capture if streaming
	 */
	if (!stm_capture_streaming(cap_ctx))
		goto streamoff_done;

	ret = stm_pixel_capture_stop(cap_ctx->pixel_capture);
	if (ret) {
		pr_err("%s(): failed to stop pixel capture\n", __func__);
		goto streamoff_done;
	}

	/*
	 * This is only valid, in case the link setup is done or stop from decoder
	 */
	if (dec_stm_sd) {
		ret = stm_pixel_capture_detach(cap_ctx->pixel_capture,
							dec_stm_sd->stm_obj);
		if (ret)
			pr_err("%s(): failed to detach pixel capture from decoder\n", __func__);
	}

streamoff_done:
	return ret;
}

/**
 * stm_capture_streamon() - attach to play stream and start capture
 * @cap_ctx    : valid capture context
 * @dec_stm_sd : valid decoder object
 */
int stm_capture_streamon(struct capture_context *cap_ctx,
					struct stmedia_v4l2_subdev *dec_stm_sd)
{
	int ret = 0;

	/*
	 * Attach pixel capture and start
	 */
	ret = stm_pixel_capture_attach(cap_ctx->pixel_capture,
						dec_stm_sd->stm_obj);
	if (ret) {
		pr_err("%s(): failed to attach pixel capture to decoder\n", __func__);
		goto streamon_done;
	}

	ret = stm_pixel_capture_start(cap_ctx->pixel_capture);
	if (ret) {
		pr_err("%s(): failed to start pixel capture\n", __func__);
		goto streamon_failed;
	}

	return 0;

streamon_failed:
	stm_pixel_capture_detach(cap_ctx->pixel_capture,
					dec_stm_sd->stm_obj);
streamon_done:
	return ret;
}

/**
 * map_v4l2_to_capture_colorspace() - map v4l2 to pixel capture colorspace
 */
stm_pixel_capture_color_space_t
	map_v4l2_to_capture_colorspace(enum v4l2_colorspace v4l2_colorspace)
{
	stm_pixel_capture_color_space_t stm_color_space;

	/*
	 * Find out the color space mapping.
	 * TODO: STM_PIXEL_CAPTURE_BT601_FULLRANGE and
	 * other full range mapping remains
	 */
	switch (v4l2_colorspace) {
	case V4L2_COLORSPACE_SMPTE170M:
		stm_color_space = STM_PIXEL_CAPTURE_BT601;
		break;

	case V4L2_COLORSPACE_REC709:
		stm_color_space = STM_PIXEL_CAPTURE_BT709;
		break;

	/*
	 * FIXME: Is the video range same as full range?
	 */
	case V4L2_COLORSPACE_SRGB:
		stm_color_space = STM_PIXEL_CAPTURE_RGB_VIDEORANGE;
		break;

	default:
		stm_color_space = STM_PIXEL_CAPTURE_RGB;
		break;
	}

	return stm_color_space;
}

/**
 * map_v4l2_to_capture_pixelcode() - map v4l2 to pixel capture pixelcode
 */
stm_pixel_capture_format_t
	map_v4l2_to_capture_pixelcode(enum v4l2_mbus_pixelcode pixel_code)
{
	stm_pixel_capture_format_t pixel_fmt = STM_PIXEL_FORMAT_NONE;

	switch((uint32_t)pixel_code) {
	case V4L2_MBUS_FMT_RGB888_1X24:
		pixel_fmt = STM_PIXEL_FORMAT_RGB_8B8B8B_SP;
		break;

	case V4L2_MBUS_FMT_YUV8_1X24:
		pixel_fmt = STM_PIXEL_FORMAT_YUV_8B8B8B_SP;
		break;

	case V4L2_MBUS_FMT_RGB101010_1X30:
		pixel_fmt = STM_PIXEL_FORMAT_RGB_10B10B10B_SP;
		break;

	case V4L2_MBUS_FMT_YUV10_1X30:
		pixel_fmt = STM_PIXEL_FORMAT_YCbCr_10B10B10B_SP;
		break;

	case V4L2_MBUS_FMT_YUYV8_2X8:
		pixel_fmt = STM_PIXEL_FORMAT_YCbCr422_8B8B8B_DP;
		break;

	case V4L2_MBUS_FMT_YUYV10_2X10:
		pixel_fmt = STM_PIXEL_FORMAT_YCbCr422_10B10B10B_DP;
		break;

	case V4L2_MBUS_FMT_YUYV8_1X16:
		pixel_fmt = STM_PIXEL_FORMAT_YCbCr422R;
		break;

	default:
		pixel_fmt = STM_PIXEL_FORMAT_NONE;
		break;
	}

	return pixel_fmt;
}

/**
 * map_v4l2_to_capture_field() - map v4l2 to pixel capture field
 */
stm_pixel_capture_flags_t
	map_v4l2_to_capture_field(enum v4l2_field field)
{
	stm_pixel_capture_flags_t flags;

	switch (field) {
	case V4L2_FIELD_INTERLACED:
		flags = STM_PIXEL_CAPTURE_BUFFER_INTERLACED;
		break;

	case V4L2_FIELD_TOP:
		flags = STM_PIXEL_CAPTURE_BUFFER_TOP_ONLY;
		break;

	case V4L2_FIELD_BOTTOM:
		flags = STM_PIXEL_CAPTURE_BUFFER_BOTTOM_ONLY;
		break;

	case V4L2_FIELD_INTERLACED_TB:
		flags = STM_PIXEL_CAPTURE_BUFFER_TOP_BOTTOM;
		break;

	case V4L2_FIELD_INTERLACED_BT:
		flags = STM_PIXEL_CAPTURE_BUFFER_BOTTOM_TOP;
		break;

	case V4L2_FIELD_NONE:
	default:
		flags = 0;
		break;
	}

	return flags;
}

/**
 * map_capture_to_v4l2_pixelcode() - map pixel capture to v4l2 pixelcode
 */
enum v4l2_mbus_pixelcode
	map_capture_to_v4l2_pixelcode(stm_pixel_capture_format_t pixel_fmt)
{
	enum v4l2_mbus_pixelcode code = 0;

	switch (pixel_fmt) {
	case STM_PIXEL_FORMAT_RGB_8B8B8B_SP:
		code = V4L2_MBUS_FMT_RGB888_1X24;
		break;

	case STM_PIXEL_FORMAT_YUV_8B8B8B_SP:
		code = V4L2_MBUS_FMT_YUV8_1X24;
		break;

	case STM_PIXEL_FORMAT_RGB_10B10B10B_SP:
		code = V4L2_MBUS_FMT_RGB101010_1X30;
		break;

	case STM_PIXEL_FORMAT_YCbCr_10B10B10B_SP:
		code = V4L2_MBUS_FMT_YUV10_1X30;
		break;

	case STM_PIXEL_FORMAT_YCbCr422_8B8B8B_DP:
		code = V4L2_MBUS_FMT_YUYV8_2X8;
		break;

	case STM_PIXEL_FORMAT_YCbCr422_10B10B10B_DP:
		code = V4L2_MBUS_FMT_YUYV10_2X10;
		break;

	case STM_PIXEL_FORMAT_YCbCr422R:
		code = V4L2_MBUS_FMT_YUYV8_1X16;
		break;

	default:
		code = -EINVAL;
		break;
	}

	return code;
}

/**
 * map_capture_to_v4l2_field() - map pixel capture to v4l2 field
 */
enum v4l2_field
	map_capture_to_v4l2_field(stm_pixel_capture_flags_t flags)
{
	enum v4l2_field field;

	switch (flags) {
	case STM_PIXEL_CAPTURE_BUFFER_INTERLACED:
		field = V4L2_FIELD_INTERLACED;
		break;

	case STM_PIXEL_CAPTURE_BUFFER_TOP_ONLY:
	     field = V4L2_FIELD_TOP;
	     break;

	case STM_PIXEL_CAPTURE_BUFFER_BOTTOM_ONLY:
		field = V4L2_FIELD_BOTTOM;
		break;

	case STM_PIXEL_CAPTURE_BUFFER_TOP_BOTTOM:
		field = V4L2_FIELD_INTERLACED_TB;
		break;

	case STM_PIXEL_CAPTURE_BUFFER_BOTTOM_TOP:
		field = V4L2_FIELD_INTERLACED_BT;
		break;

	/*
	 * TODO: No 3D fields as of now.
	 */
	case STM_PIXEL_CAPTURE_BUFFER_3D:
		field = V4L2_FIELD_ANY;
		break;

	default:
		field = V4L2_FIELD_NONE;
		break;
	}

	return field;
}

/**
 * map_capture_to_v4l2_colorspace() - map pixel capture colorspace to v4l2
 */
enum v4l2_colorspace
	map_capture_to_v4l2_colorspace(stm_pixel_capture_color_space_t stm_color_space)
{
	enum v4l2_colorspace color_space;

	/*
	 * Find out the color space mapping.
	 * TODO: STM_PIXEL_CAPTURE_BT601_FULLRANGE and
	 * other full range mapping remains
	 */
	switch (stm_color_space) {
	case STM_PIXEL_CAPTURE_BT601:
		color_space = V4L2_COLORSPACE_SMPTE170M;
		break;

	case STM_PIXEL_CAPTURE_BT709:
		color_space = V4L2_COLORSPACE_REC709;
		break;

	/*
	 * TODO: Is this the default color space?
	 */
	default:
		color_space = V4L2_COLORSPACE_SRGB;
		break;
	}

	return color_space;
}
