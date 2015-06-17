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
 * Header containing the structure definitions
************************************************************************/
#ifndef __STM_V4L2_VIDEO_CAPTURE_H__
#define __STM_V4L2_VIDEO_CAPTURE_H__

#include <media/media-entity.h>
#include <media/videobuf2-core.h>

#include "stm_pixel_capture.h"
#include "stmedia.h"

/* Not yet working */
/* #define PIXEL_CAPTURE_EVENT_SUPPORT 1 */

struct capture_context {
	struct mutex lock;
	unsigned int id;
	struct v4l2_subdev subdev;
	struct media_pad *pads, *dec_pad;

	int mapping_index;
	unsigned int flags;

	unsigned int vq_set;
	struct vb2_queue vq;

	stm_pixel_capture_h pixel_capture;
	stm_pixel_capture_buffer_format_t format;
	stm_pixel_capture_input_params_t params;

	unsigned long chroma_offset;
	unsigned long buffer_size;

#ifdef PIXEL_CAPTURE_EVENT_SUPPORT
	struct list_head active;
	spinlock_t slock;

	stm_event_subscription_h events;
#endif
};

/*
 * Indexes to find out the input context
 */
#define COMPO_INDEX			0
#define DVP_INDEX			1
#define MAX_CAPTURE_DEVICES		2

/*
 * Compo Pads indexing
 */
#define COMPO_SNK_PAD			0
#define COMPO_SRC_PAD			1

/*
 * DVP pads indexing
 */
#define DVP_SNK_PAD			0
#define DVP_SRC_PAD			1

#define VIDIOC_STI_S_VIDEO_INPUT_STABLE		\
	_IOW('V', BASE_VIDIOC_PRIVATE + 1, int )

/*
 * Function declaration/definitions
 */
int compo_capture_subdev_init(struct capture_context *cap_ctx);
void compo_capture_subdev_exit(struct capture_context *cap_ctx);

#ifdef CONFIG_STLINUXTV_DVP
int dvp_capture_subdev_init(struct capture_context *cap_ctx);
void dvp_capture_subdev_exit(struct capture_context *cap_ctx);
#else
static inline int dvp_capture_subdev_init(struct capture_context *cap_ctx)
{
	return 0;
}

static inline void dvp_capture_subdev_exit(struct capture_context *cap_ctx)
{
	return;
}
#endif

/*
 * Common function declarations for compo and dvp
 */
stm_pixel_capture_color_space_t
	map_v4l2_to_capture_colorspace(enum v4l2_colorspace v4l2_colorspace);
stm_pixel_capture_format_t
	map_v4l2_to_capture_pixelcode(enum v4l2_mbus_pixelcode pixel_code);
stm_pixel_capture_flags_t
	map_v4l2_to_capture_field(enum v4l2_field field);
enum v4l2_mbus_pixelcode
	map_capture_to_v4l2_pixelcode(stm_pixel_capture_format_t pixel_fmt);
enum v4l2_field
	map_capture_to_v4l2_field(stm_pixel_capture_flags_t flags);
enum v4l2_colorspace
	map_capture_to_v4l2_colorspace(stm_pixel_capture_color_space_t stm_color_space);
bool stm_capture_streaming(struct capture_context *cap_ctx);
int stm_capture_streamoff(struct capture_context *cap_ctx,
				struct stmedia_v4l2_subdev *dec_stm_sd);
int stm_capture_streamon(struct capture_context *cap_ctx,
				struct stmedia_v4l2_subdev *dec_stm_sd);

#endif
