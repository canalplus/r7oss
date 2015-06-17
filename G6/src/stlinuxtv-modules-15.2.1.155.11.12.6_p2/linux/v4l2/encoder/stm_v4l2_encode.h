/************************************************************************
Copyright (C) 2007, 2009, 2010 STMicroelectronics. All Rights Reserved.

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
 * Implementation of v4l2 control device
************************************************************************/
#ifndef __STM_V4L2_ENCODE_H__
#define __STM_V4L2_ENCODE_H__

#include <media/v4l2-subdev.h>
#include <media/videobuf2-core.h>
#include <media/v4l2-ctrls.h>

#include "stmedia.h"

#include "stm_se.h"
#include "stm_event.h"

#include "stm_v4l2_encode_video.h"
#include "stm_v4l2_encode_audio.h"

#define STM_V4L2_ENCODE_CONNECT_NONE   0
#define STM_V4L2_ENCODE_CONNECT_INJECT 1
#define STM_V4L2_ENCODE_CONNECT_DECODE 2

struct stm_v4l2_encoder_param_fmt {
	u32 type;		/* buffer type */
	u32 sizebuf;	/* buffer size */
};

/*
 * These macros are used as index inside the encoder pads
 */
#define STM_ENCODE_SINK_PAD	0
#define STM_ENCODE_SOURCE_PAD	1
#define STM_ENCODE_MAX_PADS	2

struct stm_v4l2_encoder_buf {
	struct vb2_buffer vb;
	struct list_head list;
	stm_se_capture_buffer_t metadata;
};

struct stm_v4l2_encoder_device {

	int users;

	int enc_mode; /* audio or video encoding */
	struct v4l2_ctrl_handler ctrl_handler;

	/* private use */
	union {
		stm_v4l2_videnc_device_t video;
		stm_v4l2_audenc_device_t audio;
	} enc_dev;

	/* video buffer queue */
	struct vb2_queue src_vq;
	int              src_vq_set;
	struct vb2_queue dst_vq;
	int              dst_vq_set;

	/* list head to track queued capture buffer */
	struct list_head active_cap;

	/* encode object */
	void*  enc_obj;
	stm_se_encode_h		encode_obj;

	/* encoding parameters */
	stm_se_encode_stream_h	encode_stream;
	stm_object_h		    encode_sink;
	stm_event_subscription_h evt_subs;
	stm_event_subscription_entry_t *evt_subs_entry;

	union {
		video_encode_params_t	video;
		audio_encode_params_t	audio;
	} encode_params;

	/* encoder format */
	struct stm_v4l2_encoder_param_fmt src_fmt;
	struct stm_v4l2_encoder_param_fmt dst_fmt;

	struct mutex lock;	/* encoder device lock */

	struct semaphore sema;

	int audenc_offset;
	int encoder_id;

	struct v4l2_subdev encoder_subdev;
	struct media_pad encoder_pad[STM_ENCODE_MAX_PADS];

	int   src_connect_type;

	void				*alloc_ctx;

#define STM_ENCODER_FLAG_EOS_PENDING	(1<<0)
	unsigned int flags;
};

struct stm_v4l2_enc_fh {
	struct v4l2_fh fh;
	struct stm_v4l2_encoder_device *dev_p;
};

#define file_to_stm_v4l2_encoder_dev(_e) \
	((struct stm_v4l2_enc_fh*)container_of(_e->private_data, struct stm_v4l2_enc_fh, fh))->dev_p;

#define encoder_sd_to_encoder_device(encoder_sd) \
        container_of(encoder_sd, struct stm_v4l2_encoder_device, encoder_subdev)

/*
 * stm_media_entity_to_encoder_context
 * @me: media entity pointer
 * This macro returns the encoder device context for the given media entity
 */
#define stm_media_entity_to_encoder_context(me)			\
		container_of(media_entity_to_v4l2_subdev(me),	\
		struct stm_v4l2_encoder_device, encoder_subdev)

struct stm_v4l2_encoder_device *encoder_entity_to_EncoderDevice(struct media_entity *me);

/*
 * Functions to manage connections between encoder and decoder
 */
int stm_encoder_link_setup(const struct media_pad *src_pad,
				unsigned long type, u32 flags);
int stm_encoder_connect_decoder(const struct media_pad *src_pad,
				const struct media_pad *dst_pad,
				unsigned long entity_type);
int stm_encoder_disconnect_decoder(const struct media_pad *src_pad,
				const struct media_pad *dst_pad,
				unsigned long entity_type);
int stm_encoder_event_init(struct stm_v4l2_encoder_device *dev_p);
void stm_encoder_event_remove(struct stm_v4l2_encoder_device *dev_p);

#endif
