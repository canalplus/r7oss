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

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/device.h>

#include <media/v4l2-ioctl.h>
#include <media/videobuf2-core.h>
#include <media/videobuf2-bpa2-contig.h>

#include "stmedia.h"
#include <linux/stm/stmedia_export.h>
#include "stm_memsink.h"
#include "stm_event.h"

#include "stm_se.h"

#include "linux/dvb/dvb_v4l2_export.h"

#include "stm_v4l2_encode.h"
#include "stm_v4l2_tsmux.h"
#include "stm_v4l2_common.h"
#include "backend.h"

/******************************
 * DEFINES
 ******************************/

#define STM_V4L2_ENC_BPA2_PARTITION "BPA2_Region0"

#define STM_V4L2_MAX_ENCODER_NB			8

#define STM_V4L2_VIDEO_ENCODER_OFFSET	0
#define STM_V4L2_VIDEO_ENCODER_NB		8
#define STM_V4L2_AUDIO_ENCODER_OFFSET	8
#define STM_V4L2_AUDIO_ENCODER_NB		8

#define ENC_MODE_VIDEO 0
#define ENC_MODE_AUDIO 1

/******************************
 * GLOBAL VARIABLES
 ******************************/

static int encoder_video_device_nums = STM_V4L2_VIDEO_ENCODER_NB;
module_param(encoder_video_device_nums, int, S_IWUSR);

static int encoder_audio_device_nums = STM_V4L2_AUDIO_ENCODER_NB;
module_param(encoder_audio_device_nums, int, S_IWUSR);

static int encoder_object_nums;
static int encoder_device_nums;
static int stm_audio_enc_offset;

/******************************
 * structures and variables
 ******************************/

int stm_encoder_video_handler(struct stm_v4l2_encoder_device *dev_p);
int stm_encoder_audio_handler(struct stm_v4l2_encoder_device *dev_p);

struct stm_v4l2_encode_root {
	struct video_device viddev;
	struct media_pad *pads;
};

struct stm_v4l2_encoder_object {
	stm_se_encode_h encoder;
	int users;

	struct mutex lock;	/* encoder object lock */
};

static struct stm_v4l2_encode_root stm_v4l2_encoder_root;

static struct stm_v4l2_encoder_object  *EncoderObject;
static struct stm_v4l2_encoder_device  *EncoderDevice;

/**
 * stm_encoder_link_setup() - find if encoder is free for linking
 * @src_pad: encoder pad
 * @typ    : remote entity type (video/audio)
 * @flags  : MEDIA_LNK_*
 */
int stm_encoder_link_setup(const struct media_pad *src_pad,
				unsigned long type, u32 flags)
{
	int ret = 0, id = 0;
	struct media_pad *pad;
	struct stm_v4l2_encoder_device *enc_ctx;

	enc_ctx = stm_media_entity_to_encoder_context(src_pad->entity);

	mutex_lock(&enc_ctx->lock);

	/*
	 * If the encoder is already running in non-tunneled mode, then
	 * refuse media-controller for this connection
	 */
	if (enc_ctx->src_connect_type ==
			STM_V4L2_ENCODE_CONNECT_INJECT) {
		printk(KERN_ERR "%s is running in non-tunneled mode\n",
				src_pad->entity->name);
		ret = -EBUSY;
		goto link_setup_done;
	}

	/*
	 * As this encoder is not running in non-tunneled mode, so,
	 * let's find out if this is already connected for tunneled mode.
	 */
	pad = stm_media_find_remote_pad_with_type(src_pad,
			MEDIA_LNK_FL_ENABLED,
			type, &id);

	if (flags & MEDIA_LNK_FL_ENABLED) {

		/*
		 * This encoder is already connected
		 */
		if (pad) {
			printk(KERN_ERR "%s is already linked to %s\n",
				src_pad->entity->name, pad->entity->name);
			ret = -EBUSY;
		}

	} else {

		if (pad)
			printk(KERN_DEFAULT "%s link to %s is snapped\n",
				src_pad->entity->name, pad->entity->name);

	}

link_setup_done:
	mutex_unlock(&enc_ctx->lock);
	return ret;
}

/**
 * stm_encoder_connect_decoder() - connect encoder <- decoder
 * @src_pad    : encoder pad
 * @dst_pad    : decoder pad (can be NULL to search all decoders)
 * @entity_type: decoder type(Audio/video)
 * Connection is only possible if both play and encode stream exists.
 * The first enabled decoder for this encoder will be connected.
 */
int stm_encoder_connect_decoder(const struct media_pad *src_pad,
				const struct media_pad *dst_pad,
				unsigned long entity_type)
{
	struct VideoDeviceContext_s *vid_ctx;
	struct AudioDeviceContext_s *aud_ctx;
	struct stm_v4l2_encoder_device *enc_ctx;
	stm_se_play_stream_h play_stream;
	bool search_all_decoders = false;
	int ret = 0, id = 0;

	enc_ctx = stm_media_entity_to_encoder_context(src_pad->entity);

	/*
	 * Since, no connection exist at the moment, we need to find
	 * the decoder to which this encoder can be connected.
	 */
	if (!dst_pad)
		search_all_decoders = true;

	/*
	 * Check/Iterate over the list for the decoder.
	 */
	do {
		if (search_all_decoders) {
			dst_pad = stm_media_find_remote_pad_with_type(src_pad,
						MEDIA_LNK_FL_ENABLED,
						entity_type, &id);
			/*
			 * No more decoders present, exit
			 */
			if (!dst_pad)
				break;
		}

		/*
		 * Break-out if we find the decoder with the desired property
		 */
		if (entity_type == MEDIA_ENT_T_DVB_SUBDEV_VIDEO_DECODER) {

			vid_ctx = stm_media_entity_to_video_context
							(dst_pad->entity);

			mutex_lock(&vid_ctx->vidops_mutex);
			mutex_lock(&enc_ctx->lock);

			/*
			 * If the decoder is not started, then connection
			 * cannot be initiated at the moment. The decoder
			 * will now connect to this encoder
			 */
			if (!vid_ctx->VideoStream)
				goto video_decoder_connect_done;

			play_stream = vid_ctx->VideoStream->Handle;

			ret = stm_se_play_stream_attach(play_stream,
					enc_ctx->encode_stream,
					STM_SE_PLAY_STREAM_OUTPUT_PORT_DEFAULT);
			if (ret && (ret != -EALREADY)) {
				printk(KERN_ERR "VideoEncoder attach to stream"
						" failed (ret: %d)\n", ret);
				goto video_decoder_connect_done;
			}

			mutex_unlock(&enc_ctx->lock);
			mutex_unlock(&vid_ctx->vidops_mutex);

			break;

		} else {

			aud_ctx = stm_media_entity_to_audio_context
							(dst_pad->entity);

			mutex_lock(&aud_ctx->audops_mutex);
			mutex_lock(&enc_ctx->lock);

			if (!aud_ctx->AudioStream)
				goto audio_decoder_connect_done;

			play_stream = aud_ctx->AudioStream->Handle;

			ret = stm_se_play_stream_attach(play_stream,
					enc_ctx->encode_stream,
					STM_SE_PLAY_STREAM_OUTPUT_PORT_DEFAULT);
			if (ret && (ret != -EALREADY)) {
				printk(KERN_ERR "AudioEncoder attach to stream"
						" failed (ret: %d)\n", ret);
				goto audio_decoder_connect_done;
			}

			mutex_unlock(&enc_ctx->lock);
			mutex_unlock(&aud_ctx->audops_mutex);

			break;
		}

	} while (1);

	if (!dst_pad)
		printk(KERN_DEFAULT "Cannot connect to decoder as %s is "
			"running in non-tunneled mode\n", src_pad->entity->name);

	return 0;

video_decoder_connect_done:
	mutex_unlock(&enc_ctx->lock);
	mutex_unlock(&vid_ctx->vidops_mutex);
	return ret;

audio_decoder_connect_done:
	mutex_unlock(&enc_ctx->lock);
	mutex_unlock(&aud_ctx->audops_mutex);
	return ret;
}

/**
 * stm_encoder_disconnect_decoder() - disconnect encoder <-X- decoder
 * @src_pad    : encoder pad
 * @dst_pad    : decoder pad (can be NULL to search all decoders)
 * @entity_type: decoder type(Audio/video)
 * Disconnection is only possible if both play and encode stream exists.
 */
int stm_encoder_disconnect_decoder(const struct media_pad *src_pad,
				const struct media_pad *dst_pad,
				unsigned long entity_type)
{
	struct VideoDeviceContext_s *vid_ctx;
	struct AudioDeviceContext_s *aud_ctx;
	struct stm_v4l2_encoder_device *enc_ctx;
	stm_se_play_stream_h play_stream;
	bool search_all_decoders = false;
	int ret = 0, id = 0;

	enc_ctx = stm_media_entity_to_encoder_context(src_pad->entity);

	if (!dst_pad)
		search_all_decoders = true;

	do {
		/*
		 * Since, an encoder can be connected to a single decoder,
		 * so, this encoder pad will have only a single enabled
		 * decoder pad
		 */
		if (search_all_decoders) {
			dst_pad = stm_media_find_remote_pad_with_type(src_pad,
						MEDIA_LNK_FL_ENABLED,
						entity_type, &id);

			if (!dst_pad)
				break;
		}

		if (entity_type == MEDIA_ENT_T_DVB_SUBDEV_VIDEO_DECODER) {

			vid_ctx = stm_media_entity_to_video_context
							(dst_pad->entity);

			mutex_lock(&vid_ctx->vidops_mutex);
			mutex_lock(&enc_ctx->lock);

			/*
			 * If the decoder is stopped, then the connection is
			 * already broken
			 */
			if (!vid_ctx->VideoStream)
				goto video_decoder_disconnect_done;

			play_stream = vid_ctx->VideoStream->Handle;

			ret = stm_se_play_stream_detach(play_stream,
					enc_ctx->encode_stream);
			if (ret && (ret != -ENOTCONN)) {
				printk(KERN_ERR "VideoEncoder detach from "
					"stream failed (ret: %d)\n", ret);
				goto video_decoder_disconnect_done;
			}

			mutex_unlock(&enc_ctx->lock);
			mutex_unlock(&vid_ctx->vidops_mutex);

			break;

		} else {

			aud_ctx = stm_media_entity_to_audio_context
							(dst_pad->entity);

			mutex_lock(&aud_ctx->audops_mutex);
			mutex_lock(&enc_ctx->lock);

			if (!aud_ctx->AudioStream)
				goto audio_decoder_disconnect_done;

			play_stream = aud_ctx->AudioStream->Handle;

			ret = stm_se_play_stream_detach(play_stream,
							enc_ctx->encode_stream);
			if (ret && (ret != -ENOTCONN)) {
				printk(KERN_ERR "AudioEncoder detach from "
					"stream failed (ret: %d)\n", ret);
				goto audio_decoder_disconnect_done;
			}

			mutex_unlock(&enc_ctx->lock);
			mutex_unlock(&aud_ctx->audops_mutex);

			break;
		}

	} while (1);

	if (!dst_pad)
		printk(KERN_DEFAULT "Cannot disconnect from decoder as %s is "
			"running in non-tunneled mode\n", src_pad->entity->name);

	return 0;

video_decoder_disconnect_done:
	mutex_unlock(&enc_ctx->lock);
	mutex_unlock(&vid_ctx->vidops_mutex);
	return ret;

audio_decoder_disconnect_done:
	mutex_unlock(&enc_ctx->lock);
	mutex_unlock(&aud_ctx->audops_mutex);
	return ret;
}

/*------------------------------------------------------------------------------ */

static int stm_encoder_object_init(struct stm_v4l2_encoder_object *obj_p,
						stm_se_encode_h *encoder, int index)
{
	char encoder_name[32];
	stm_se_encode_h enc_h = NULL;
	int ret;

	mutex_lock(&obj_p->lock);
	if (obj_p->encoder == NULL) {
		snprintf(encoder_name, sizeof(encoder_name), "encoder%02d", index);
		ret = stm_se_encode_new(encoder_name, &enc_h);
		if (ret<0) {
			printk(KERN_ERR "stm_se_encode_new error: %x\n", ret);
			mutex_unlock(&obj_p->lock);
			return -EINVAL;
		}
		obj_p->encoder = enc_h;
		obj_p->users++;
	}
	else {
		enc_h = obj_p->encoder;
		obj_p->users++;
	}
	mutex_unlock(&obj_p->lock);

	*encoder = enc_h;
	return 0;
}

static int stm_encoder_object_release(struct stm_v4l2_encoder_object *obj_p)
{
	int ret=0;

	mutex_lock(&obj_p->lock);

	if (obj_p->encoder == NULL) {
		mutex_unlock(&obj_p->lock);
		return -EINVAL;
	}

	obj_p->users--;

	if(obj_p->users > 0) {
		mutex_unlock(&obj_p->lock);
		return 0;
	}

	ret = stm_se_encode_delete(obj_p->encoder);
	obj_p->encoder = NULL;

	mutex_unlock(&obj_p->lock);

	return ret;
}

/**
 * stm_encoder_event_cb
 * This function is the event callback function, called asynchronously when
 * a memsink event is generated (new encoded data available)
 */
static void stm_encoder_event_cb(unsigned int nbevent,
				 stm_event_info_t *events)
{
	struct stm_v4l2_encoder_device *dev_p = NULL;
	stm_event_info_t *eventinfo;
	unsigned int i;
	int ret;

	if (nbevent < 1)
		return;

	for (i=0; i < nbevent; i++) {
		eventinfo = &events[i];
		if (!(eventinfo->event.event_id &
		      STM_MEMSINK_EVENT_DATA_AVAILABLE)) {
			printk (KERN_INFO "%s: unexpected evt (%d)\n",
					   __func__, eventinfo->event.event_id);
			continue;
		}

		dev_p = (struct stm_v4l2_encoder_device *)eventinfo->cookie;

		mutex_lock(&dev_p->lock);

		if(!vb2_is_streaming(&dev_p->dst_vq)) {
			mutex_unlock(&dev_p->lock);
			return;
		}

		if (list_empty(&dev_p->active_cap)) {
			mutex_unlock(&dev_p->lock);
			continue;
		}

		if (dev_p->enc_mode == ENC_MODE_VIDEO)
			ret = stm_encoder_video_handler(dev_p);
		else
			ret = stm_encoder_audio_handler(dev_p);
		if (ret){
			printk(KERN_ERR "%s: failed to handle data (%d)\n",
					 __func__, ret);
			mutex_unlock(&dev_p->lock);
			continue;
		}

		mutex_unlock(&dev_p->lock);
	}
}

/**
 * stm_encoder_event_init
 * This function register in order to receive notification from memsink
 * to know when new data become available
 */
int stm_encoder_event_init(struct stm_v4l2_encoder_device *dev_p)
{
	stm_event_subscription_entry_t *evt_entry = NULL;
	int ret = 0;

	if (!dev_p->encode_sink) {
		printk(KERN_ERR "%s: No encoder sink registered\n", __func__);
		ret = -EINVAL;
		goto failed;
	}

	evt_entry = kzalloc(sizeof(stm_event_subscription_entry_t), GFP_KERNEL);
	if (!evt_entry) {
		printk(KERN_ERR "Out of memory for subscription entry\n");
		ret = -ENOMEM;
		goto failed;
	}
	dev_p->evt_subs_entry = evt_entry;

	/*
	 * Create the event subscription with the following events
	 */
	evt_entry->object = dev_p->encode_sink;
	evt_entry->event_mask = STM_MEMSINK_EVENT_DATA_AVAILABLE;
	evt_entry->cookie = dev_p;

	ret = stm_event_subscription_create(evt_entry, 1, &dev_p->evt_subs);
	if (ret) {
		printk(KERN_ERR "%s: Failed to create event sub (%d)\n",
				__func__, ret);
		goto failed_subscription_create;
	}

	ret = stm_event_set_handler(dev_p->evt_subs, &stm_encoder_event_cb);
	if (ret) {
		printk(KERN_ERR "%s: Failed to register callback fct (%d)\n",
				__func__, ret);
		goto failed_set_handler;
	}

	return ret;

failed_set_handler:
	if (stm_event_subscription_delete(dev_p->evt_subs))
		printk(KERN_ERR "%s: Failed to delete sub\n", __func__);
failed_subscription_create:
	dev_p->evt_subs = NULL;
	dev_p->evt_subs_entry = NULL;
	if(evt_entry)
		kfree(evt_entry);
failed:
	return ret;
}

/**
 * stm_encoder_event_remove
 * This function unregister event notification
 */
void stm_encoder_event_remove(struct stm_v4l2_encoder_device *dev_p)
{
	int ret;

	/*
	 * If there's no event entry, then there won't be any subcription
	 * created, so, return
	 */
	if (!dev_p->evt_subs) {
		printk(KERN_DEBUG "%s: Nothing to do\n", __func__);
		return;
	}

	/*
	 * Delete the event subscription entry
	 */
	ret = stm_event_subscription_modify(dev_p->evt_subs,
					    dev_p->evt_subs_entry,
					    STM_EVENT_SUBSCRIPTION_OP_REMOVE);
	if (ret)
		printk(KERN_ERR "%s: Failed to delete entry (%d)\n",
				__func__, ret);

	/*
	 * Delete the subscription
	 */
	ret = stm_event_subscription_delete(dev_p->evt_subs);
	if (ret)
		printk(KERN_ERR "%s: Failed to delete subscription (%d)\n",
				__func__, ret);

	dev_p->evt_subs = NULL;
	if (dev_p->evt_subs_entry)
		kfree(dev_p->evt_subs_entry);
	dev_p->evt_subs_entry = NULL;
}

/*---------------------------------------------------------------*/

/* ioctl functions */

static int stm_v4l2_encoder_query_cap(struct file *file, void *fh,
			   struct v4l2_capability *cap)
{
	strlcpy(cap->driver, "Encoder", sizeof(cap->driver));
	strlcpy(cap->card, "STMicroelectronics", sizeof(cap->card));
	cap->bus_info[0] = 0;

	cap->version = LINUX_VERSION_CODE;
	cap->capabilities = V4L2_CAP_VIDEO_CAPTURE | V4L2_CAP_VIDEO_OUTPUT
			  | V4L2_CAP_STREAMING;

	return 0;
}

static int stm_v4l2_encoder_enum_output(struct file *file, void *fh,
				  struct v4l2_output *output)
{
	struct stm_v4l2_encoder_device  *dev_p;
	int index = output->index;

	/* check consistency of index */
	if (index < 0 || index >= encoder_video_device_nums)
		return -EINVAL;

	dev_p = &EncoderDevice[index];

	return stm_v4l2_encoder_enum_vid_output(output, dev_p, index);
}

static int stm_v4l2_encoder_enum_input(struct file *file, void *fh,
		struct v4l2_input *input)
{
	struct stm_v4l2_encoder_device  *dev_p;
	int index = input->index;

	/* check consistency of index */
	if (index < 0 || index >= encoder_video_device_nums)
		return -EINVAL;

	dev_p = &EncoderDevice[index];

	return stm_v4l2_encoder_enum_vid_input(input, dev_p, index);
}

static int stm_v4l2_encoder_enum_audout(struct file *file, void *fh,
				  struct v4l2_audioout *output)
{
	struct stm_v4l2_encoder_device  *dev_p;
	int index = output->index;
	int index_dev;

	/* check consistency of index */
	if (index < 0 || index >= encoder_audio_device_nums)
		return -EINVAL;

	index_dev = index + stm_audio_enc_offset;
	dev_p = &EncoderDevice[index_dev];

	return stm_v4l2_encoder_enum_aud_output(output, dev_p, index);
}

static int stm_v4l2_encoder_enum_audio(struct file *file, void *fh,
		struct v4l2_audio *input)
{
	struct stm_v4l2_encoder_device  *dev_p;
	int index = input->index;
	int index_dev;

	/* check consistency of index */
	if (index < 0 || index >= encoder_audio_device_nums)
		return -EINVAL;

	index_dev = index + stm_audio_enc_offset;
	dev_p = &EncoderDevice[index_dev];

	return stm_v4l2_encoder_enum_aud_input(input, dev_p, index);
}

static int stm_v4l2_encoder_g_output(struct file *file, void *fh,
		unsigned int *const index )
{
	struct stm_v4l2_encoder_device  *dev_p;

	*index = -1;

	dev_p = file_to_stm_v4l2_encoder_dev(file);
	if(!dev_p)
		return -EINVAL;
	*index = dev_p->encoder_id;
	return 0;
}

static int stm_v4l2_encoder_s_output(struct file *file, void *fh,
		unsigned int index )
{
	struct stm_v4l2_encoder_object  *obj_p;
	struct stm_v4l2_encoder_device  *dev_p;
	stm_se_encode_h		encoder;
	struct stm_v4l2_enc_fh *enc_fh = container_of(fh, struct stm_v4l2_enc_fh, fh);
	int ret;

	if ( index >= encoder_video_device_nums)
		return -EINVAL;

	obj_p = &EncoderObject[index];
	dev_p = &EncoderDevice[index];

	mutex_lock(&dev_p->lock);

	if (dev_p->src_vq_set==0) {
		ret = stm_v4l2_encoder_vid_queue_init(V4L2_BUF_TYPE_VIDEO_OUTPUT,
									&dev_p->src_vq, dev_p);
		if (ret) {
			printk(KERN_ERR "stm_v4l2_encoder_vid_queue_init error : %x\n", ret);
			mutex_unlock(&dev_p->lock);
			return ret;
		}
		dev_p->src_vq_set = 1;
		memset(&dev_p->encode_params.video, 0, sizeof(dev_p->encode_params.video));
	}

	if (dev_p->encode_obj == NULL) {
		ret = stm_encoder_object_init(obj_p, &encoder, index);
		if (ret) {
			mutex_unlock(&dev_p->lock);
			return ret;
		}
		dev_p->enc_obj = (void*)obj_p;
		dev_p->encode_obj = encoder;
	}

	if (enc_fh->dev_p == NULL) {
		dev_p->users++;
		enc_fh->dev_p = dev_p;
		((struct v4l2_fh *)fh)->ctrl_handler = &dev_p->ctrl_handler;
	}

	mutex_unlock(&dev_p->lock);

	return 0;
}

static int stm_v4l2_encoder_g_input(struct file *file, void *fh,
		unsigned int *const index )
{
	struct stm_v4l2_encoder_device  *dev_p;

	*index = -1;

	dev_p = file_to_stm_v4l2_encoder_dev(file);
	if(!dev_p)
		return -EINVAL;
	*index = dev_p->encoder_id;
	return 0;
}

static int stm_v4l2_encoder_s_input(struct file *file, void *fh,
		unsigned int index)
{
	struct stm_v4l2_encoder_object  *obj_p;
	struct stm_v4l2_encoder_device  *dev_p;
	stm_se_encode_h		encoder;
	int ret;
	struct stm_v4l2_enc_fh *enc_fh = container_of(fh, struct stm_v4l2_enc_fh, fh);

	if ( index >= encoder_video_device_nums)
		return -EINVAL;

	obj_p = &EncoderObject[index];
	dev_p = &EncoderDevice[index];

	mutex_lock(&dev_p->lock);

	if (dev_p->dst_vq_set==0) {
		ret = stm_v4l2_encoder_vid_queue_init(V4L2_BUF_TYPE_VIDEO_CAPTURE,
										&dev_p->dst_vq, dev_p);
		if (ret) {
			printk(KERN_ERR "stm_v4l2_encoder_vid_queue_init error : %x\n", ret);
			mutex_unlock(&dev_p->lock);
			return ret;
		}
		dev_p->dst_vq_set = 1;
	}

	if (dev_p->encode_obj == NULL) {
		ret = stm_encoder_object_init(obj_p, &encoder, index);
		if (ret) {
			mutex_unlock(&dev_p->lock);
			return ret;
		}
		dev_p->enc_obj = (void*)obj_p;
		dev_p->encode_obj = encoder;
	}

	if (enc_fh->dev_p == NULL) {
		dev_p->users++;
		enc_fh->dev_p = dev_p;
		((struct v4l2_fh *)fh)->ctrl_handler = &dev_p->ctrl_handler;
	}

	mutex_unlock(&dev_p->lock);

	return 0;
}

static int STM_V4L2_FUNC(stm_v4l2_encoder_s_audout,
		struct file *file, void *fh, struct v4l2_audioout *output)
{
	struct stm_v4l2_encoder_device  *dev_p;
	int index = output->index;
	int index_dev;
	int ret;
	struct stm_v4l2_enc_fh *enc_fh = container_of(fh, struct stm_v4l2_enc_fh, fh);

	if ( index >= encoder_audio_device_nums)
		return -EINVAL;

	index_dev = index + stm_audio_enc_offset;

	dev_p = &EncoderDevice[index_dev];

	mutex_lock(&dev_p->lock);

	if (dev_p->src_vq_set==0) {
		ret = stm_v4l2_encoder_aud_queue_init(V4L2_BUF_TYPE_VIDEO_OUTPUT,
									&dev_p->src_vq, dev_p);
		if (ret) {
			printk(KERN_ERR "stm_v4l2_encoder_aud_queue_init error : %x\n", ret);
			mutex_unlock(&dev_p->lock);
			return ret;
		}
		dev_p->src_vq_set = 1;
		memset(&dev_p->encode_params.audio, 0, sizeof(dev_p->encode_params.audio));
		/* we need to initialize this to a same value as the ctrl value */
		dev_p->encode_params.audio.sample_rate = 4000;
		dev_p->encode_params.audio.vbr_quality_factor = 100;
		dev_p->encode_params.audio.channel_count = 2;
		dev_p->encode_params.audio.bitrate = 48000;
	}

	if (enc_fh->dev_p == NULL) {
		dev_p->users++;
		enc_fh->dev_p = dev_p;
		((struct v4l2_fh *)fh)->ctrl_handler = &dev_p->ctrl_handler;
	}

	mutex_unlock(&dev_p->lock);

	return 0;
}

static int STM_V4L2_FUNC(stm_v4l2_encoder_s_audio,
		struct file *file, void *fh, struct v4l2_audio *input)
{
	struct stm_v4l2_encoder_object  *obj_p;
	struct stm_v4l2_encoder_device  *dev_p;
	stm_se_encode_h		encoder;
	int index = input->index;
	int index_dev;
	int ret;
	struct stm_v4l2_enc_fh *enc_fh = container_of(fh, struct stm_v4l2_enc_fh, fh);

	if ( index >= encoder_audio_device_nums)
		return -EINVAL;

	index_dev = index + stm_audio_enc_offset;

	obj_p = &EncoderObject[index];
	dev_p = &EncoderDevice[index_dev];

	mutex_lock(&dev_p->lock);

	if (dev_p->dst_vq_set==0) {
		ret = stm_v4l2_encoder_aud_queue_init(V4L2_BUF_TYPE_VIDEO_CAPTURE,
									&dev_p->dst_vq, dev_p);
		if (ret) {
			printk(KERN_ERR "stm_v4l2_encoder_aud_queue_init error : %x\n", ret);
			mutex_unlock(&dev_p->lock);
			return ret;
		}
		dev_p->dst_vq_set = 1;
	}

	if (dev_p->encode_obj == NULL) {
		ret = stm_encoder_object_init(obj_p, &encoder, index);
		if (ret) {
			mutex_unlock(&dev_p->lock);
			return ret;
		}
		dev_p->enc_obj = (void*)obj_p;
		dev_p->encode_obj = encoder;
		dev_p->encode_params.audio.sample_rate = 4000;
		dev_p->encode_params.audio.vbr_quality_factor = 100;
		dev_p->encode_params.audio.channel_count = 2;
		dev_p->encode_params.audio.bitrate = 48000;
	}

	if (enc_fh->dev_p == NULL) {
		dev_p->users++;
		enc_fh->dev_p = dev_p;
		((struct v4l2_fh *)fh)->ctrl_handler = &dev_p->ctrl_handler;
	}

	mutex_unlock(&dev_p->lock);

	return 0;
}

static int stm_v4l2_encoder_enum_fmt(struct file *file, void *fh,
				   struct v4l2_fmtdesc *f)
{
	int ret;
	struct stm_v4l2_encoder_device *dev_p = file_to_stm_v4l2_encoder_dev(file);

	if (dev_p == NULL)
		return -EINVAL;

	if(dev_p->enc_mode == ENC_MODE_VIDEO)
		ret = stm_v4l2_encoder_vid_enum_fmt(file, fh, f);
	else if(dev_p->enc_mode == ENC_MODE_AUDIO)
		ret = stm_v4l2_encoder_aud_enum_fmt(file, fh, f);
	else
		return -EINVAL;

	return ret;
}

static int stm_v4l2_encoder_try_fmt(struct file *file, void *fh,
				   struct v4l2_format *f)
{
	int ret;
	struct stm_v4l2_encoder_device *dev_p = file_to_stm_v4l2_encoder_dev(file);

	if (dev_p == NULL)
		return -EINVAL;

	if(dev_p->enc_mode == ENC_MODE_VIDEO)
		ret = stm_v4l2_encoder_vid_try_fmt(file, fh, f);
	else if(dev_p->enc_mode == ENC_MODE_AUDIO)
		ret = stm_v4l2_encoder_aud_try_fmt(file, fh, f);
	else
		return -EINVAL;

	return ret;
}

static int stm_v4l2_encoder_g_fmt(struct file *file, void *fh,
							struct v4l2_format *f)
{
/*FIXME  Dummy place holder for later impl. Needed for ioctl_ops S_PARM*/
	memset(f, 0, sizeof(*f));
	return 0;
}


static int stm_v4l2_encoder_s_fmt(struct file *file, void *fh,
									struct v4l2_format *f)
{
	int ret;
	struct stm_v4l2_encoder_device *dev_p = file_to_stm_v4l2_encoder_dev(file);

	if (dev_p == NULL)
		return -EINVAL;

	if(dev_p->enc_mode == ENC_MODE_VIDEO)
		ret = stm_v4l2_encoder_vid_s_fmt(file, fh, f);
	else if(dev_p->enc_mode == ENC_MODE_AUDIO)
		ret = stm_v4l2_encoder_aud_s_fmt(file, fh, f);
	else
		return -EINVAL;

	return ret;
}

static int stm_v4l2_encoder_s_parm(struct file *file, void *fh,
			struct v4l2_streamparm *param_p)
{
	int ret;
	struct stm_v4l2_encoder_device *dev_p = file_to_stm_v4l2_encoder_dev(file);

	if (dev_p == NULL)
		return -EINVAL;

	if(dev_p->enc_mode == ENC_MODE_VIDEO)
		ret = stm_v4l2_encoder_vid_s_parm(file, fh, param_p);
	else if(dev_p->enc_mode == ENC_MODE_AUDIO)
		ret = stm_v4l2_encoder_aud_s_parm(file, fh, param_p);
	else
		return -EINVAL;

	return ret;
}

static int stm_v4l2_encoder_reqbufs(struct file *file, void *fh,
			  struct v4l2_requestbuffers *reqbufs)
{
	int ret;
	struct stm_v4l2_encoder_device *dev_p = file_to_stm_v4l2_encoder_dev(file);

	if (dev_p == NULL)
		return -EINVAL;

	if(dev_p->enc_mode == ENC_MODE_VIDEO)
		ret = stm_v4l2_encoder_vid_reqbufs(file, fh, reqbufs);
	else if(dev_p->enc_mode == ENC_MODE_AUDIO)
		ret = stm_v4l2_encoder_aud_reqbufs(file, fh, reqbufs);
	else
		return -EINVAL;

	return ret;
}

static int stm_v4l2_encoder_querybuf(struct file *file, void *fh,
			   struct v4l2_buffer *buf)
{
	int ret;
	struct stm_v4l2_encoder_device *dev_p = file_to_stm_v4l2_encoder_dev(file);

	if (dev_p == NULL)
		return -EINVAL;

	if(dev_p->enc_mode == ENC_MODE_VIDEO)
		ret = stm_v4l2_encoder_vid_querybuf(file, fh, buf);
	else if(dev_p->enc_mode == ENC_MODE_AUDIO)
		ret = stm_v4l2_encoder_aud_querybuf(file, fh, buf);
	else
		return -EINVAL;

	return ret;
}

static int stm_v4l2_encoder_qbuf(struct file *file, void *fh,
				struct v4l2_buffer *buf)
{
	int ret;
	struct stm_v4l2_encoder_device *dev_p = file_to_stm_v4l2_encoder_dev(file);

	if (dev_p == NULL)
		return -EINVAL;

	if(dev_p->enc_mode == ENC_MODE_VIDEO)
		ret = stm_v4l2_encoder_vid_qbuf(file, fh, buf);
	else if(dev_p->enc_mode == ENC_MODE_AUDIO)
		ret = stm_v4l2_encoder_aud_qbuf(file, fh, buf);
	else
		return -EINVAL;

	return ret;
}

static int stm_v4l2_encoder_dqbuf(struct file *file, void *fh,
				struct v4l2_buffer *buf)
{
	int ret;
	struct stm_v4l2_encoder_device *dev_p = file_to_stm_v4l2_encoder_dev(file);

	if (dev_p == NULL)
		return -EINVAL;

	if(dev_p->enc_mode == ENC_MODE_VIDEO)
		ret = stm_v4l2_encoder_vid_dqbuf(file, fh, buf);
	else if(dev_p->enc_mode == ENC_MODE_AUDIO)
		ret = stm_v4l2_encoder_aud_dqbuf(file, fh, buf);
	else
		return -EINVAL;

	return ret;
}

static int stm_v4l2_encoder_streamon(struct file *file, void *fh,
			   enum v4l2_buf_type type)
{
	int ret;
	struct stm_v4l2_encoder_device *dev_p = file_to_stm_v4l2_encoder_dev(file);

	if (dev_p == NULL)
		return -EINVAL;

	if(dev_p->enc_mode == ENC_MODE_VIDEO)
		ret = stm_v4l2_encoder_vid_streamon(file, fh, type);
	else if(dev_p->enc_mode == ENC_MODE_AUDIO)
		ret = stm_v4l2_encoder_aud_streamon(file, fh, type);
	else
		return -EINVAL;

	return ret;
}

static int stm_v4l2_encoder_streamoff(struct file *file, void *fh,
			   enum v4l2_buf_type type)
{
	int ret;
	struct stm_v4l2_encoder_device *dev_p = file_to_stm_v4l2_encoder_dev(file);

	if (dev_p == NULL)
		return -EINVAL;

	if(dev_p->enc_mode == ENC_MODE_VIDEO)
		ret = stm_v4l2_encoder_vid_streamoff(file, fh, type);
	else if(dev_p->enc_mode == ENC_MODE_AUDIO)
		ret = stm_v4l2_encoder_aud_streamoff(file, fh, type);
	else
		return -EINVAL;

	return ret;
}

struct v4l2_ioctl_ops stm_v4l2_encoder_ioctl_ops = {
	.vidioc_querycap = stm_v4l2_encoder_query_cap,
	.vidioc_enum_output = stm_v4l2_encoder_enum_output,
	.vidioc_enum_input = stm_v4l2_encoder_enum_input,
	.vidioc_enumaudout = stm_v4l2_encoder_enum_audout,
	.vidioc_enumaudio = stm_v4l2_encoder_enum_audio,
	.vidioc_g_output = stm_v4l2_encoder_g_output,
	.vidioc_s_output = stm_v4l2_encoder_s_output,
	.vidioc_g_input = stm_v4l2_encoder_g_input,
	.vidioc_s_input = stm_v4l2_encoder_s_input,
	.vidioc_s_audout = stm_v4l2_encoder_s_audout,
	.vidioc_s_audio = stm_v4l2_encoder_s_audio,
	.vidioc_enum_fmt_vid_cap = stm_v4l2_encoder_enum_fmt,
	.vidioc_enum_fmt_vid_out = stm_v4l2_encoder_enum_fmt,
	.vidioc_try_fmt_vid_cap = stm_v4l2_encoder_try_fmt,
	.vidioc_try_fmt_vid_out = stm_v4l2_encoder_try_fmt,
	.vidioc_g_fmt_vid_cap = stm_v4l2_encoder_g_fmt,
	.vidioc_g_fmt_vid_out = stm_v4l2_encoder_g_fmt,
	.vidioc_s_fmt_vid_cap = stm_v4l2_encoder_s_fmt,
	.vidioc_s_fmt_vid_out = stm_v4l2_encoder_s_fmt,
	.vidioc_s_parm = stm_v4l2_encoder_s_parm,
	.vidioc_reqbufs = stm_v4l2_encoder_reqbufs,
	.vidioc_querybuf = stm_v4l2_encoder_querybuf,
	.vidioc_qbuf = stm_v4l2_encoder_qbuf,
	.vidioc_dqbuf = stm_v4l2_encoder_dqbuf,
	.vidioc_streamon = stm_v4l2_encoder_streamon,
	.vidioc_streamoff = stm_v4l2_encoder_streamoff,
};

/*---------------------------------------------------------------*/

/* file operations */

static int stm_v4l2_encoder_open(struct file *file)
{
	struct stm_v4l2_enc_fh *enc_fh;

	/* Allocate memory */
	enc_fh = kzalloc(sizeof(struct stm_v4l2_enc_fh), GFP_KERNEL);
	if (NULL == enc_fh) {
		printk("%s: nomem on v4l2 open\n", __FUNCTION__);
		return -ENOMEM;
	}
	v4l2_fh_init(&enc_fh->fh, &stm_v4l2_encoder_root.viddev);
	file->private_data = &enc_fh->fh;
	enc_fh->dev_p = NULL;/* it will be set in S_INPUT, S_OUTPUT */
	v4l2_fh_add(&enc_fh->fh);

	return 0;
}

static int stm_v4l2_encoder_close(struct file *file)
{
	int ret = 0;
	struct stm_v4l2_encoder_device *dev_p;
	struct stm_v4l2_enc_fh *enc_fh = container_of(file->private_data, struct stm_v4l2_enc_fh, fh);

	dev_p = enc_fh->dev_p;

	if (dev_p == NULL)
		goto fh_cleanup;

	mutex_lock(&dev_p->lock);

	dev_p->users--;

	if (dev_p->users > 0) {
		mutex_unlock(&dev_p->lock);
		goto fh_cleanup;
	}

	if (dev_p->enc_mode == ENC_MODE_VIDEO) {

		ret = stm_v4l2_encoder_vid_close(file);

		if(dev_p->src_vq_set) {
			stm_v4l2_encoder_vid_queue_release(&dev_p->src_vq);
			dev_p->src_vq_set = 0;
		}
		if(dev_p->dst_vq_set) {
			stm_v4l2_encoder_vid_queue_release(&dev_p->dst_vq);
			dev_p->dst_vq_set = 0;
		}
		memset(&dev_p->enc_dev.video, 0,
				sizeof(stm_v4l2_videnc_device_t));
	}
	else if (dev_p->enc_mode == ENC_MODE_AUDIO)	{

		ret = stm_v4l2_encoder_aud_close(file);

		if(dev_p->src_vq_set) {
			stm_v4l2_encoder_aud_queue_release(&dev_p->src_vq);
			dev_p->src_vq_set = 0;
		}
		if(dev_p->dst_vq_set) {
			stm_v4l2_encoder_aud_queue_release(&dev_p->dst_vq);
			dev_p->dst_vq_set = 0;
		}
		memset(&dev_p->enc_dev.audio, 0,
				sizeof(stm_v4l2_audenc_device_t));
	}

	if(ret)
		printk(KERN_ERR "Error : encoder stream closing\n");

	if(dev_p->encode_obj) {
		ret = stm_encoder_object_release(dev_p->enc_obj);
		if(ret)
			printk(KERN_ERR "Error : encoder object closing\n");
		dev_p->enc_obj = NULL;
		dev_p->encode_obj = NULL;
	}

	sema_init(&dev_p->sema, 0);

	mutex_unlock(&dev_p->lock);

fh_cleanup:
	/*
	 * Remove V4L2 file handlers
	 */
	v4l2_fh_del(&enc_fh->fh);
	v4l2_fh_exit(&enc_fh->fh);

	file->private_data = NULL;
	kfree(enc_fh);

	return ret;
}


static int stm_v4l2_encoder_mmap(struct file *file, struct vm_area_struct *vma)
{
	int ret;
	struct stm_v4l2_encoder_device *dev_p = file_to_stm_v4l2_encoder_dev(file);

	if (dev_p == NULL)
		return -EINVAL;

	if(dev_p->enc_mode == ENC_MODE_VIDEO)
		ret = stm_v4l2_encoder_vid_mmap(file, vma);
	else if(dev_p->enc_mode == ENC_MODE_AUDIO)
		ret = stm_v4l2_encoder_aud_mmap(file, vma);
	else
		return -EINVAL;

	return ret;
}

static unsigned int stm_v4l2_encoder_poll(struct file *file,
								struct poll_table_struct *wait)
{
	int ret;
	struct stm_v4l2_encoder_device *dev_p = file_to_stm_v4l2_encoder_dev(file);

	if (dev_p == NULL)
		return -EINVAL;

	if(dev_p->enc_mode == ENC_MODE_VIDEO)
		ret = stm_v4l2_vid_encoder_poll(file, wait);
	else if(dev_p->enc_mode == ENC_MODE_AUDIO)
		ret = stm_v4l2_aud_encoder_poll(file, wait);
	else
		return -EINVAL;

	return ret;
}


static struct v4l2_file_operations stm_v4l2_encoder_fops = {
	.owner = THIS_MODULE,
	.open = stm_v4l2_encoder_open,
	.release = stm_v4l2_encoder_close,
	.unlocked_ioctl = video_ioctl2,
	.read = NULL,
	.write = NULL,
	.mmap = stm_v4l2_encoder_mmap,
	.poll = stm_v4l2_encoder_poll,
};

/*------------------------------------------------------------------------------ */

static void stm_v4l2_encoder_vdev_release(struct video_device *vdev)
{
	/* Nothing to do, but need by V4L2 stack */
}

/*
 * Retrieve the Encoder struct stm_v4l2_encoder_device pointer from the MC encoder entity
 */
struct stm_v4l2_encoder_device *encoder_entity_to_EncoderDevice(struct media_entity *me)
{
	struct v4l2_subdev *sd;

	sd = media_entity_to_v4l2_subdev(me);

	return (struct stm_v4l2_encoder_device *)encoder_sd_to_encoder_device(sd);
}

/*
 * Media Operations
 */
static int stm_v4l2_encoder_video_link_setup(struct media_entity *entity,
                                             const struct media_pad *local,
                                             const struct media_pad *remote,
                                             u32 flags)
{
        int ret = 0;
#ifdef TUNNELLING_SUPPORTED
        struct stm_v4l2_encoder_device *EncoderDevice;
        struct v4l2_subdev *sd;

        /* We only do something when connecting / disconnecting to/from a PLANE */
        if (remote->entity->type != MEDIA_ENT_T_V4L2_SUBDEV_TSMUXER)
                return ret;

        sd = media_entity_to_v4l2_subdev(entity);
        EncoderDevice =
            container_of(sd, struct stm_v4l2_encoder_device, encoder_subdev);

        if (!EncoderDevice)
                /* That shouldn't happen */
                return -EINVAL;

        if (remote->entity->type == MEDIA_ENT_T_V4L2_SUBDEV_TSMUXER) {
                struct v4l2_subdev *subdev;
                struct tsmux_device *tsmux_dev_p;

                subdev = media_entity_to_v4l2_subdev(remote->entity);
                tsmux_dev_p =
                    container_of(subdev, struct tsmux_device, tsmux_subdev);

                if (flags & MEDIA_LNK_FL_ENABLED) {
                        ret =
                            DvbAttachEncoderToTsmux(EncoderDevice->
                                                    encode_stream,
                                                    (void *)tsmux_dev_p);
                        if (ret == 0)
                                ret = -EINVAL;
                } else {
                        ret =
                            DvbDetachEncoderToTsmux(EncoderDevice->
                                                    encode_stream,
                                                    (void *)tsmux_dev_p);
                        if (ret == 0)
                                ret = -EINVAL;
                }
        }
#endif
        return ret;
}

static const struct media_entity_operations stm_v4l2_encoder_media_ops = {
	.link_setup = stm_v4l2_encoder_video_link_setup,
};

static void process_init_error(int err_num)
{
	int i;

	for (i=0; i<err_num; i++) {
		vb2_bpa2_contig_cleanup_ctx(EncoderDevice[i].alloc_ctx);
		if(EncoderDevice[i].enc_mode == ENC_MODE_VIDEO)
			stm_v4l2_encoder_vid_exit_subdev(&EncoderDevice[i]);
		else
			stm_v4l2_encoder_aud_exit_subdev(&EncoderDevice[i]);
	}
}

static int __init stm_v4l2_encoder_init(void)
{
	struct video_device *viddev = &stm_v4l2_encoder_root.viddev;
	struct media_pad *pads;
	struct v4l2_subdev *subdev;
	unsigned int media_flags = MEDIA_LNK_FL_IMMUTABLE | MEDIA_LNK_FL_ENABLED;
	struct bpa2_part *bpa2_part;
	int ret=0;
	int i;

	/* check max encoder number */
	if (encoder_video_device_nums > STM_V4L2_MAX_ENCODER_NB)
		return -ENOMEM;

	if (encoder_audio_device_nums > STM_V4L2_MAX_ENCODER_NB)
		return -ENOMEM;

	/* set the device number */
	encoder_device_nums =
	    encoder_video_device_nums + encoder_audio_device_nums;
	stm_audio_enc_offset = encoder_video_device_nums;

	/* allocation encoder root : video and audio */
	stm_v4l2_encoder_root.pads =
	    kzalloc(sizeof(struct media_pad) * encoder_device_nums * 2,
		    GFP_KERNEL);
	if (stm_v4l2_encoder_root.pads==NULL)
		return -ENOMEM;

	pads = stm_v4l2_encoder_root.pads;

	/* allocation encoder object */
	if (encoder_video_device_nums > encoder_audio_device_nums)
		encoder_object_nums = encoder_video_device_nums;
	else
		encoder_object_nums = encoder_audio_device_nums;

	EncoderObject =
	    kzalloc(sizeof(struct stm_v4l2_encoder_object) *
		    encoder_object_nums, GFP_KERNEL);
	if (EncoderObject == NULL) {
		kfree(pads);
		return -ENOMEM;
	}

	for (i = 0; i < encoder_object_nums; i++) {
		mutex_init(&EncoderObject[i].lock);
	}

	/* allocation encoder device */
	EncoderDevice =
	    kzalloc(sizeof(struct stm_v4l2_encoder_device) *
		    encoder_device_nums, GFP_KERNEL);
	if (EncoderDevice == NULL) {
		kfree(EncoderObject);
		kfree(pads);
		return -ENOMEM;
	}

	bpa2_part = bpa2_find_part(STM_V4L2_ENC_BPA2_PARTITION);
	if (!bpa2_part) {
		DVB_ERROR("No %s bpa2 part exist\n", STM_V4L2_ENC_BPA2_PARTITION);
		return -EINVAL;
	}

	/* Initialize the stm_v4l2_encoder_device  */
	for (i = 0; i < encoder_device_nums; i++) {
		mutex_init(&EncoderDevice[i].lock);
		sema_init(&EncoderDevice[i].sema, 0);
		EncoderDevice[i].users = 0;
		EncoderDevice[i].encode_stream = NULL;
		EncoderDevice[i].encode_sink = NULL;
		EncoderDevice[i].src_connect_type  = STM_V4L2_ENCODE_CONNECT_NONE;
		EncoderDevice[i].audenc_offset = stm_audio_enc_offset;
		EncoderDevice[i].encoder_id = i;
		INIT_LIST_HEAD(&EncoderDevice[i].active_cap);

		if (i < stm_audio_enc_offset) {
			EncoderDevice[i].enc_mode = ENC_MODE_VIDEO;
			ret = stm_v4l2_encoder_vid_init_subdev(&EncoderDevice[i]);
		}
		else {
			EncoderDevice[i].enc_mode = ENC_MODE_AUDIO;
			ret = stm_v4l2_encoder_aud_init_subdev(&EncoderDevice[i]);
		}

		if (ret) break;

		subdev = &EncoderDevice[i].encoder_subdev;

		/* memory allocation context */
		EncoderDevice[i].alloc_ctx =
			vb2_bpa2_contig_init_ctx(subdev->v4l2_dev->dev,
			bpa2_part);
		if (IS_ERR(EncoderDevice[i].alloc_ctx)) {
			printk(KERN_ERR "vb2_bpa2_init_ctx error\n");
			ret = PTR_ERR(EncoderDevice[i].alloc_ctx);
			if (EncoderDevice[i].enc_mode == ENC_MODE_VIDEO)
				stm_v4l2_encoder_vid_exit_subdev(&EncoderDevice[i]);
			else
				stm_v4l2_encoder_aud_exit_subdev(&EncoderDevice[i]);
			break;
		}
	}

	if(ret) {
		process_init_error(i);
		goto enc_init_error;
	}

	/* Initialize the encoder video device */
	strlcpy(viddev->name, "Encoder", sizeof(viddev->name));
	viddev->fops = &stm_v4l2_encoder_fops;
	viddev->minor = -1;
	viddev->release = stm_v4l2_encoder_vdev_release;
	viddev->ioctl_ops = &stm_v4l2_encoder_ioctl_ops;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 7, 0))
        viddev->vfl_dir = VFL_DIR_M2M;
#endif

	/* Initialize encoder video device pads */
	for (i = 0; i < encoder_device_nums; i++)
		pads[i].flags = MEDIA_PAD_FL_SINK;
	for (i = encoder_device_nums; i < encoder_device_nums * 2; i++)
		pads[i].flags = MEDIA_PAD_FL_SOURCE;

	viddev->entity.ops = &stm_v4l2_encoder_media_ops;
	ret =
	    media_entity_init(&viddev->entity, encoder_device_nums * 2, pads, 0);
	if (ret < 0) {
		printk(KERN_ERR "%s: failed to initialize the entity (%d)\n",
		       __func__, ret);
		process_init_error(encoder_device_nums);
		goto enc_init_error;
	}

	/* In our device model, Encoder is video2 */
	ret = stm_media_register_v4l2_video_device(viddev, VFL_TYPE_GRABBER, -1);
	if (ret < 0) {
		printk(KERN_ERR
		       "%s: failed to register the overlay driver (%d)\n",
		       __func__, ret);
		media_entity_cleanup(&viddev->entity);
		process_init_error(encoder_device_nums);
		goto enc_init_error;
	}

	/* Create links from encoders to video device */
	for (i = 0; i < encoder_device_nums; i++) {
		struct media_entity *src, *sink;

		src = &EncoderDevice[i].encoder_subdev.entity;
		sink = &viddev->entity;

		ret = media_entity_create_link(src, 1, sink, i, media_flags);
		if (ret < 0) {
			printk(KERN_ERR "%s: failed to create link (%d)\n",
			       __func__, ret);
			media_entity_cleanup(&viddev->entity);
			process_init_error(encoder_device_nums);
			goto enc_init_error;
		}
	}

	/* Create links from video device to encoders */
	for (i = 0; i < encoder_device_nums; i++) {
		struct media_entity *src, *sink;

		src = &viddev->entity;
		sink = &EncoderDevice[i].encoder_subdev.entity;

		ret =
		    media_entity_create_link(src, encoder_device_nums + i,
					     sink, 0, media_flags);
		if (ret < 0) {
			printk(KERN_ERR "%s: failed to create link (%d)\n",
			       __func__, ret);
			media_entity_cleanup(&viddev->entity);
			process_init_error(encoder_device_nums);
			goto enc_init_error;
		}
	}

	/* Create Video connection */
	stm_v4l2_encoder_vid_create_connection();
	/* Create Audio connection */
	stm_v4l2_encoder_aud_create_connection();

	return 0;

enc_init_error:
	kfree(EncoderDevice);
	kfree(EncoderObject);
	kfree(pads);
	return ret;
}

static void __exit stm_v4l2_encoder_exit(void)
{
	struct video_device *viddev = &stm_v4l2_encoder_root.viddev;
	int i;

	for (i=0; i < encoder_device_nums; i++) {
		vb2_bpa2_contig_cleanup_ctx(EncoderDevice[i].alloc_ctx);
		if(EncoderDevice[i].enc_mode == ENC_MODE_VIDEO)
			stm_v4l2_encoder_vid_exit_subdev(&EncoderDevice[i]);
		else
			stm_v4l2_encoder_aud_exit_subdev(&EncoderDevice[i]);
	}

	stm_media_unregister_v4l2_video_device(viddev);

	media_entity_cleanup(&viddev->entity);

	kfree(EncoderDevice);
	kfree(EncoderObject);
	kfree(stm_v4l2_encoder_root.pads);
}

module_init(stm_v4l2_encoder_init);
module_exit(stm_v4l2_encoder_exit);
MODULE_DESCRIPTION("STMicroelectronics V4L2 Encoder device");
MODULE_AUTHOR("STMicroelectronics Limited");
MODULE_LICENSE("GPL");
