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
 * Implementation of v4l2 audio encoder device
************************************************************************/

#include <linux/device.h>
#include <asm/io.h>
#include <media/v4l2-ioctl.h>
#include <media/videobuf2-core.h>
#include <media/videobuf2-bpa2-contig.h>

#include <linux/delay.h>

#include "stmedia.h"
#include <linux/stm/stmedia_export.h>
#include "stm_memsink.h"

#include "linux/dvb/dvb_v4l2_export.h"

#include "stm_v4l2_encode.h"
#include "stm_v4l2_encode_ctrls.h"
#include "stm_v4l2_common.h"
#include "stm_v4l2_audio.h"

#include "dvb_module.h"
#include "dvb_audio.h"

#define STM_AUDENC_DST_QUEUE_OFF_BASE	(1 << 30)

#define AUDIO_BUFFER_ONE_CHANNEL (4096*4)
#define AUDIO_IN_BUFFER_SIZE (4096*4*10)
#define AUDIO_OUT_BUFFER_SIZE 45680 /* (4096*10->43320->45680) */

struct stm_v4l2_audenc_enum_fmt {
	char *name;
	u32 codec;
	u32 types;
	u32 flags;
};

static struct stm_v4l2_audenc_enum_fmt stm_v4l2_audenc_formats[] = {
	{
	 .name = "PCM",
	 .codec = V4L2_MPEG_AUDIO_STM_ENCODING_PCM,
	 .types = V4L2_BUF_TYPE_VIDEO_OUTPUT,
	 .flags = 0,
	},
	{
	 .name = "AC3",
	 .codec = V4L2_MPEG_AUDIO_STM_ENCODING_AC3,
	 .types = V4L2_BUF_TYPE_VIDEO_CAPTURE,
	 .flags = V4L2_FMT_FLAG_COMPRESSED,
	},
	{
	 .name = "AAC",
	 .codec = V4L2_MPEG_AUDIO_STM_ENCODING_AAC,
	 .types = V4L2_BUF_TYPE_VIDEO_CAPTURE,
	 .flags = V4L2_FMT_FLAG_COMPRESSED,
	},
	{
	 .name = "MP3",
	 .codec = V4L2_MPEG_AUDIO_STM_ENCODING_LAYER_3,
	 .types = V4L2_BUF_TYPE_VIDEO_CAPTURE,
	 .flags = V4L2_FMT_FLAG_COMPRESSED,
	},
};

static const int stm_v4l2_audenc_codec_table[][2] = {
	{ V4L2_MPEG_AUDIO_STM_ENCODING_LAYER_3, STM_SE_ENCODE_STREAM_ENCODING_AUDIO_MP3 },
	{ V4L2_MPEG_AUDIO_STM_ENCODING_AC3,     STM_SE_ENCODE_STREAM_ENCODING_AUDIO_AC3 },
	{ V4L2_MPEG_AUDIO_STM_ENCODING_AAC,     STM_SE_ENCODE_STREAM_ENCODING_AUDIO_AAC },
	{ V4L2_MPEG_AUDIO_STM_ENCODING_HEAAC,   STM_SE_ENCODE_STREAM_ENCODING_AUDIO_AAC }
};

static int stm_v4l2_audenc_encoding_output(struct stm_v4l2_encoder_device *dev_p,
				struct vb2_buffer *vb);

/**
 * stm_encoder_aud_link_setup() - connect audio encoder to audio decoder
 * Callback from media-controller when any change in connection status is
 * detected. This is also called from inside decoder link_setup to verify
 * that this encoder is still available for connection.
 */
static int stm_encoder_aud_link_setup(struct media_entity *entity,
			      const struct media_pad *local,
			      const struct media_pad *remote, u32 flags)

{
	if (remote->entity->type == MEDIA_ENT_T_DVB_SUBDEV_AUDIO_DECODER)
		return stm_encoder_link_setup(local,
					remote->entity->type, flags);

	return 0;
}

static const struct media_entity_operations encoder_aud_media_ops = {
	.link_setup = stm_encoder_aud_link_setup,
};

const struct v4l2_subdev_core_ops enc_aud_core_ops = {
	.queryctrl = v4l2_subdev_queryctrl,
	.querymenu = v4l2_subdev_querymenu,
	.g_ctrl = v4l2_subdev_g_ctrl,
	.s_ctrl = v4l2_subdev_s_ctrl,
	.g_ext_ctrls = v4l2_subdev_g_ext_ctrls,
	.try_ext_ctrls = v4l2_subdev_try_ext_ctrls,
	.s_ext_ctrls = v4l2_subdev_s_ext_ctrls,
	};

static const struct v4l2_subdev_ops encoder_aud_subdev_ops = {
	.core = &enc_aud_core_ops,
};

int stm_v4l2_encoder_aud_init_subdev(void *dev)
{
	struct stm_v4l2_encoder_device *dev_p = dev;
	struct v4l2_subdev *subdev = &dev_p->encoder_subdev;
	struct media_pad *pads = dev_p->encoder_pad;
	struct media_entity *me = &subdev->entity;
	int ret;

	/* Initialize the V4L2 subdev / MC entity */
	v4l2_subdev_init(subdev, &encoder_aud_subdev_ops);
	snprintf(subdev->name, sizeof(subdev->name), "aud-encoder-%02d",
			dev_p->encoder_id - dev_p->audenc_offset);

	v4l2_set_subdevdata(subdev, dev_p);

	pads[0].flags = MEDIA_PAD_FL_SINK;
	pads[1].flags = MEDIA_PAD_FL_SOURCE;

	ret = media_entity_init(me, 2, pads, 0);
	if (ret < 0) {
		printk(KERN_ERR "%s: entity init failed(%d)\n", __func__, ret);
		return ret;
	}

	me->ops = &encoder_aud_media_ops;
	me->type = MEDIA_ENT_T_V4L2_SUBDEV_AUDIO_ENCODER;

	ret = stm_v4l2_aud_enc_init_ctrl(dev_p);
	if(ret){
		printk(KERN_ERR "%s: aud enc ctrl init failed (%d)\n",
		       __func__, ret);
	}
	subdev->ctrl_handler = &dev_p->ctrl_handler;

	ret = stm_media_register_v4l2_subdev(subdev);
	if (ret < 0) {
		media_entity_cleanup(me);
		printk(KERN_ERR "%s: stm_media register failed (%d)\n",
		       __func__, ret);
		return ret;
	}

	return 0;
}

int stm_v4l2_encoder_aud_exit_subdev(void *dev)
{
	struct stm_v4l2_encoder_device *dev_p = dev;
	struct v4l2_subdev *subdev = &dev_p->encoder_subdev;
	struct media_entity *me = &subdev->entity;

	stm_media_unregister_v4l2_subdev(subdev);
	v4l2_ctrl_handler_free(&dev_p->ctrl_handler);

	media_entity_cleanup(me);
	return 0;
}

/*-----------------------------------------------------------------*/

/* videobuf2 callbacks */

static int stm_v4l2_encoder_aud_queue_setup(struct vb2_queue *vq,
					const struct v4l2_format *fmt,
					unsigned int *nbuffers,
					unsigned int *nplanes,
					unsigned int sizes[],
					void *alloc_ctxs[])
{
	struct stm_v4l2_encoder_device *dev_p = vb2_get_drv_priv(vq);
	int count = *nbuffers;

	switch (vq->type) {
	case V4L2_BUF_TYPE_VIDEO_OUTPUT:
		*nplanes = 1;
		*nbuffers = count;
		sizes[0] = dev_p->src_fmt.sizebuf;
		alloc_ctxs[0] = dev_p->alloc_ctx;
		break;
	case V4L2_BUF_TYPE_VIDEO_CAPTURE:
		*nplanes = 1;
		*nbuffers = count;
		sizes[0] = dev_p->dst_fmt.sizebuf;
		alloc_ctxs[0] = dev_p->alloc_ctx;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int stm_v4l2_encoder_aud_buf_prepare(struct vb2_buffer *vb)
{
	struct vb2_queue *vq = vb->vb2_queue;
	struct stm_v4l2_encoder_device *dev_p = vb2_get_drv_priv(vb->vb2_queue);

	if (vq->type == V4L2_BUF_TYPE_VIDEO_CAPTURE)
		vb2_set_plane_payload(vb, 0, dev_p->dst_fmt.sizebuf);

	return 0;
}

static void stm_encoder_audio_handle_eos( struct stm_v4l2_encoder_device *dev_p);
int stm_encoder_audio_handler(struct stm_v4l2_encoder_device *dev_p);

static void stm_v4l2_encoder_aud_buf_queue(struct vb2_buffer *vb)
{
	struct vb2_queue *vq = vb->vb2_queue;
	struct stm_v4l2_encoder_buf *buf = container_of(vb,
						struct stm_v4l2_encoder_buf,
						vb);
	struct stm_v4l2_encoder_device *dev_p = vb2_get_drv_priv(vb->vb2_queue);
	int ret;

	switch(vq->type) {
	case V4L2_BUF_TYPE_VIDEO_OUTPUT:
		ret = stm_v4l2_audenc_encoding_output(dev_p, vb);
		if(ret)
			vb2_buffer_done(vb, VB2_BUF_STATE_ERROR);
		else
			vb2_buffer_done(vb, VB2_BUF_STATE_DONE);
		break;
	case V4L2_BUF_TYPE_VIDEO_CAPTURE:
		list_add_tail(&buf->list, &dev_p->active_cap);

		/* Can only proceed if we are already streaming */
		if (!vb2_is_streaming(vb->vb2_queue))
			return;

		/* We have added a new buffer - lets handle potential EOS
		 * This will (or not) take that buffer if needed */
		stm_encoder_audio_handle_eos(dev_p);

		/* Handle data as long has we can */
		while (!list_empty(&dev_p->active_cap)){
			ret = stm_encoder_audio_handler(dev_p);
			if (ret && (ret!=-EAGAIN))
				printk(KERN_ERR "%s: audio hdl failed (%d)\n",
						__func__, ret);
			else if(ret == -EAGAIN)
				break;
		}
		break;
	default :
		break;
	}
}

static void stm_v4l2_encoder_aud_wait_prepare(struct vb2_queue *q)
{
	struct stm_v4l2_encoder_device *dev_p = vb2_get_drv_priv(q);
	mutex_unlock(&dev_p->lock);
}

static void stm_v4l2_encoder_aud_wait_finish(struct vb2_queue *q)
{
	struct stm_v4l2_encoder_device *dev_p = vb2_get_drv_priv(q);

	mutex_lock(&dev_p->lock);
}

static int stm_v4l2_encoder_aud_buf_finish(struct vb2_buffer *vb)
{
	struct vb2_queue *vq = vb->vb2_queue;
	struct stm_v4l2_encoder_buf *buf = container_of(vb,
						struct stm_v4l2_encoder_buf,
						vb);
	struct v4l2_buffer* v4l2_dst_buf = &buf->vb.v4l2_buf;
	stm_se_capture_buffer_t* metadata_p = &buf->metadata;
	struct v4l2_audenc_dst_metadata dst_meta = {0};
	void*  out_metadata_p;
	uint64_t encoded_time;
	uint64_t temp_time;
	int encoded_time_format;
	int i;
	int ret;

	/* Nothing to do for OUTPUT type buffers */
	if(vq->type == V4L2_BUF_TYPE_VIDEO_OUTPUT)
		return 0;

	v4l2_dst_buf->flags = 0;

	if (metadata_p->u.compressed.discontinuity &
					STM_SE_DISCONTINUITY_DISCONTINUOUS) {
		metadata_p->u.compressed.discontinuity &=
					~STM_SE_DISCONTINUITY_DISCONTINUOUS;
		v4l2_dst_buf->flags |=
				V4L2_BUF_FLAG_STM_ENCODE_TIME_DISCONTINUITY;
	}

	encoded_time_format = metadata_p->u.compressed.encoded_time_format;
	encoded_time = metadata_p->u.compressed.encoded_time;
	if(encoded_time_format == TIME_FORMAT_US) {
		v4l2_dst_buf->timestamp.tv_usec = do_div(encoded_time, 1000000);
		v4l2_dst_buf->timestamp.tv_sec  = encoded_time;
	}
	else if(encoded_time_format == TIME_FORMAT_PTS) {
		temp_time = do_div(encoded_time, 90000);
		temp_time = temp_time * 1000;
		do_div(temp_time, 90);
		v4l2_dst_buf->timestamp.tv_usec = temp_time;
		v4l2_dst_buf->timestamp.tv_sec  = encoded_time;
	}
	else if(encoded_time_format == TIME_FORMAT_27MHz) {
		temp_time = do_div(encoded_time, 27000000);
		do_div(temp_time, 27);
		v4l2_dst_buf->timestamp.tv_usec = temp_time;
		v4l2_dst_buf->timestamp.tv_sec  = encoded_time;
	}
	else { /* check it */
		v4l2_dst_buf->timestamp.tv_sec = 0;
		v4l2_dst_buf->timestamp.tv_usec  = 0;
		printk(KERN_ERR "No time_format from aud encoder\n");
	}

	dst_meta.sample_rate = metadata_p->u.compressed.audio.core_format.sample_rate;
	dst_meta.drc_factor  = metadata_p->u.compressed.audio.drc_factor;

	dst_meta.channel_count = metadata_p->u.compressed.audio.core_format.channel_placement.channel_count;
	for(i=0; i<dst_meta.channel_count; i++) {
		dst_meta.channel[i] = metadata_p->u.compressed.audio.core_format.channel_placement.chan[i];
	}

	out_metadata_p = (void*)v4l2_dst_buf->reserved;
	if (NULL != out_metadata_p) {
		ret = copy_to_user(out_metadata_p, &dst_meta,
							sizeof(struct v4l2_audenc_dst_metadata));
		if (ret) {
			printk(KERN_ERR "can't copy to user\n");
		}
	}

	return 0;
}

static struct vb2_ops stm_v4l2_encoder_aud_qops = {
	.queue_setup	 = stm_v4l2_encoder_aud_queue_setup,
	.buf_prepare	 = stm_v4l2_encoder_aud_buf_prepare,
	.buf_queue 		 = stm_v4l2_encoder_aud_buf_queue,
	.wait_prepare	= stm_v4l2_encoder_aud_wait_prepare,
	.wait_finish	= stm_v4l2_encoder_aud_wait_finish,
	.buf_finish 	 = stm_v4l2_encoder_aud_buf_finish,
};

int stm_v4l2_encoder_aud_queue_init(int type, struct vb2_queue *vq, void *priv)
{
	struct stm_v4l2_encoder_device *dev_p = priv;
	int ret;

	memset(vq, 0, sizeof(*vq));
	vq->type = type;
	vq->io_modes = VB2_MMAP | VB2_USERPTR;
	vq->drv_priv = dev_p;
	vq->ops = &stm_v4l2_encoder_aud_qops;
	vq->mem_ops = &vb2_bpa2_contig_memops;
	vq->buf_struct_size = sizeof(struct stm_v4l2_encoder_buf);
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 10, 0))
	vq->timestamp_type = V4L2_BUF_FLAG_TIMESTAMP_MONOTONIC;
#endif

	ret = vb2_queue_init(vq);
	if (ret) {
		printk(KERN_ERR "ERROR : %s %d ret=%d  \n", __func__, __LINE__, ret);
		return ret;
	}

	return 0;
}

void stm_v4l2_encoder_aud_queue_release(struct vb2_queue *vq)
{
	vb2_queue_release(vq);
}

/*-----------------------------------------------------------------*/

/* encoding */

static int stm_v4l2_audenc_encoding_output(struct stm_v4l2_encoder_device *dev_p,
				struct vb2_buffer *vb)
{
	struct v4l2_buffer* v4l2_src_buf= &vb->v4l2_buf;
	stm_se_capture_buffer_t* metadata_p;
	stm_se_uncompressed_frame_metadata_t* meta_p;
	struct v4l2_audenc_src_metadata src_meta;
	void*  src_user_meta_p;
	int    pcm_table_length;
	int    sample_format;
	struct timeval in_time;
	void* vaddr=NULL;
	unsigned long paddr=0;
	int src_size=0;
	int ret=0;
	int i;

	if(vb2_is_streaming(&dev_p->dst_vq)==0) {
		printk(KERN_ERR "can't encode. capture buffer is off\n");
		return -EINVAL;
	}

	/*
	 * Configure audio encoder for EOS discontinuity. For a full set of
	 * discontinuites possible, please see dvb_v4l2_export.h.
	 * The last packet decoded packet pushed will be NULL, so, the payload
	 * is also zero for this packet.
	 */
	if (!vb2_get_plane_payload(vb, 0)) {
	        mutex_unlock(&dev_p->lock);

		ret = stm_se_encode_stream_inject_discontinuity
			(dev_p->encode_stream, STM_SE_DISCONTINUITY_EOS);
		if (ret)
			printk(KERN_ERR "Unable to configure audio encoder "
					"for EOS , ret: %d\n", ret);

	        mutex_lock(&dev_p->lock);

		return ret;
	}

	if(dev_p->enc_dev.audio.src_v4l2_buf.reserved==0) {
		printk(KERN_ERR "need input metadata\n");
		return -EINVAL;
	}

	src_user_meta_p = (void*)dev_p->enc_dev.audio.src_v4l2_buf.reserved;
	ret = copy_from_user(&src_meta, src_user_meta_p,
					sizeof(struct v4l2_audenc_src_metadata));
	if (ret) {
		printk(KERN_ERR "can't get input meta data\n");
		return -EINVAL;
	}

	pcm_table_length = sizeof(stm_v4l2_audio_pcm_format_table);
	sample_format = stm_v4l2_convert_v4l2_SE_define( src_meta.sample_format,
			                   stm_v4l2_audio_pcm_format_table,
	                           pcm_table_length);
	if(sample_format == -1) {
		printk(KERN_ERR "Not supported sample format\n");
		return -EINVAL;
	}

	in_time = v4l2_src_buf->timestamp;

	paddr = *(unsigned long *)vb2_plane_cookie(vb, 0);
	if (!paddr){
		printk(KERN_ERR "%s: failed to get paddr\n", __func__);
		return -EIO;
	}
	vaddr = vb2_plane_vaddr(vb, 0);

	/* process set parameters */
	src_size = dev_p->enc_dev.audio.src_v4l2_buf.bytesused;

	/* process src metadata */
	metadata_p = &dev_p->enc_dev.audio.src_metadata;
	memset(metadata_p, 0, sizeof(stm_se_capture_buffer_t));

	metadata_p->virtual_address = (void*)vaddr;
	metadata_p->buffer_length = src_size;

	meta_p = &metadata_p->u.uncompressed;

	/*
	 * Configure for Time discontinuity
	 */
	if (v4l2_src_buf->flags & V4L2_BUF_FLAG_STM_ENCODE_TIME_DISCONTINUITY)
		meta_p->discontinuity = STM_SE_DISCONTINUITY_DISCONTINUOUS;

	meta_p->system_time = stm_v4l2_get_systemtime_us();
	meta_p->native_time_format = TIME_FORMAT_US;
	meta_p->native_time = (uint64_t)in_time.tv_sec*1000000 + (uint64_t)in_time.tv_usec;

	meta_p->media = STM_SE_ENCODE_STREAM_MEDIA_AUDIO;

	meta_p->audio.core_format.sample_rate = src_meta.sample_rate;
	meta_p->audio.sample_format = sample_format;
	meta_p->audio.program_level = src_meta.program_level;
	meta_p->audio.emphasis = src_meta.emphasis;

	meta_p->audio.core_format.channel_placement.channel_count = src_meta.channel_count;

	if ((src_meta.channel_count > 0) && (src_meta.channel_count < STM_SE_MAX_NUMBER_OF_AUDIO_CHAN)) {
		for(i=0; i<src_meta.channel_count; i++) {
			meta_p->audio.core_format.channel_placement.chan[i] = src_meta.channel[i];
		}
	}
	mutex_unlock(&dev_p->lock);


	/*
	 * Inject uncompressed data for encoding
	 */
	ret = stm_se_encode_stream_inject_frame(dev_p->encode_stream,
					vaddr, paddr, src_size,
					metadata_p->u.uncompressed);
	if(ret)
		printk(KERN_ERR "audio encode error = %x\n", ret);

	mutex_lock(&dev_p->lock);

	return ret;
}

/**
 * stm_encoder_audio_handle_eos
 * This function verify if we have received a EOS discontinuity and push
 * the information into AN ALREADY AVAILABLE vb2_buffer
 */
static void stm_encoder_audio_handle_eos( struct stm_v4l2_encoder_device *dev_p)
{
	struct stm_v4l2_encoder_buf *buf;

	/* Check if we need (and can) push another buffer for the EOS */
	if (dev_p->flags & STM_ENCODER_FLAG_EOS_PENDING){
		buf = list_entry(dev_p->active_cap.next,
				 struct stm_v4l2_encoder_buf, list);
		dev_p->flags &= ~STM_ENCODER_FLAG_EOS_PENDING;
		list_del(&buf->list);
		vb2_set_plane_payload(&buf->vb, 0, 0);
		vb2_buffer_done(&buf->vb, VB2_BUF_STATE_DONE);
	}
}

int stm_encoder_audio_handler(struct stm_v4l2_encoder_device *dev_p)
{
	struct v4l2_buffer* v4l2_dst_buf;
	struct stm_v4l2_encoder_buf *buf;
	stm_se_capture_buffer_t* metadata_p;
	void* vaddr=NULL;
	unsigned long paddr=0;
	int size=0;
	int ret=0;

	/* Check if there are data to be read */
	ret = stm_memsink_test_for_data(dev_p->encode_sink, &size);
	if (ret && (ret != -EAGAIN)){
		printk(KERN_ERR "%s: failed to check data status (%d)\n",
				 __func__, ret);
		goto done;
	} else if (ret == -EAGAIN)
		goto done;

	buf = list_entry(dev_p->active_cap.next,
			 struct stm_v4l2_encoder_buf, list);
	v4l2_dst_buf = &buf->vb.v4l2_buf;

	paddr = *(unsigned long *)vb2_plane_cookie(&buf->vb, 0);
	if (!paddr){
		printk(KERN_ERR "%s: failed to get paddr\n", __func__);
		return -EIO;
	}
	vaddr = vb2_plane_vaddr(&buf->vb, 0);

	metadata_p = &buf->metadata;

	metadata_p->virtual_address = (void*)vaddr;
	metadata_p->buffer_length = dev_p->dst_fmt.sizebuf;

	ret = stm_memsink_pull_data(dev_p->encode_sink,
	                            metadata_p,
	                            metadata_p->buffer_length,
	                            &size);
	if(ret){
		printk(KERN_ERR "Error : SE aud read error=%d\n", ret);
		goto done;
	}

	/* We will have a buffer to return */
	list_del(&buf->list);

	/*
	 * (EOS.B): There is no data and EOS, it means application
	 * has injected a fake packet and we will let the reader
	 * know that EOS is there. If there's data and there's EOS
	 * we will push the data and EOS is already stored in here.
	 */
	if (!metadata_p->payload_length &&
		(metadata_p->u.compressed.discontinuity &
				STM_SE_DISCONTINUITY_EOS)) {

		metadata_p->u.compressed.discontinuity &=
					~STM_SE_DISCONTINUITY_EOS;
		vb2_set_plane_payload(&buf->vb, 0, 0);
		vb2_buffer_done(&buf->vb, VB2_BUF_STATE_DONE);
		goto done;
	} else if ( metadata_p->u.compressed.discontinuity &
			 STM_SE_DISCONTINUITY_EOS)
		dev_p->flags |= STM_ENCODER_FLAG_EOS_PENDING;

	vb2_set_plane_payload(&buf->vb, 0, metadata_p->payload_length);
	vb2_buffer_done(&buf->vb, VB2_BUF_STATE_DONE);

	/* Check if we need (and can) push another buffer for the EOS */
	if (!list_empty(&dev_p->active_cap))
		stm_encoder_audio_handle_eos(dev_p);

done:
	return ret;
}

/*-----------------------------------------------------------------*/

/* audio ioctls */

int stm_v4l2_encoder_enum_aud_output(struct v4l2_audioout *output,
				void *dev, int index)
{
	struct stm_v4l2_encoder_device *dev_p = dev;
	struct v4l2_subdev *remote_sd;

	memset(output, 0, sizeof(struct v4l2_audioout));
	remote_sd = &dev_p->encoder_subdev;
	strlcpy(output->name, remote_sd->name, sizeof(output->name));
	output->index = index;

	return 0;
}

int stm_v4l2_encoder_enum_aud_input(struct v4l2_audio *input,
				void *dev, int index)
{
	struct stm_v4l2_encoder_device *dev_p = dev;
	struct v4l2_subdev *remote_sd;

	memset(input, 0, sizeof(struct v4l2_audio));
	remote_sd = &dev_p->encoder_subdev;
	strlcpy(input->name, remote_sd->name, sizeof(input->name));
	input->index = index;

	return 0;
}

int stm_v4l2_encoder_aud_enum_fmt(struct file *file, void *fh,
				   struct v4l2_fmtdesc *f)
{
	struct stm_v4l2_audenc_enum_fmt *fmt;
	int i, num, size;
	struct stm_v4l2_encoder_device *dev_p = file_to_stm_v4l2_encoder_dev(file);

	if (dev_p == NULL)
		return -EINVAL;

	size = ARRAY_SIZE(stm_v4l2_audenc_formats);
	num = 0;

	for (i = 0; i < size; ++i) {
		if (stm_v4l2_audenc_formats[i].types == f->type) {
			/* index-th format of type type found ? */
			if (num == f->index)
				break;
			/* Correct type but haven't reached our index yet,
			 * just increment per-type index */
			++num;
		}
	}

	if (i < size) {
		/* Format found */
		fmt = &stm_v4l2_audenc_formats[i];
		f->flags = fmt->flags;
		strlcpy(f->description, fmt->name, sizeof(f->description));
		f->pixelformat = fmt->codec;
		memset(f->reserved, 0, sizeof(f->reserved));
		return 0;
	}

	/* Format not found */
	return -EINVAL;
}

static struct stm_v4l2_audenc_enum_fmt *find_aud_enum_format(struct v4l2_format *f)
{
	struct stm_v4l2_audenc_enum_fmt *fmt;
	struct v4l2_audenc_format* aud_fmt = (struct v4l2_audenc_format*)f->fmt.raw_data;
	int size;
	int k;

	size = ARRAY_SIZE(stm_v4l2_audenc_formats);

	for (k = 0; k < size; k++) {
		fmt = &stm_v4l2_audenc_formats[k];
		if (fmt->codec == aud_fmt->codec)
			break;
	}

	if (k == size)
		return NULL;

	return &stm_v4l2_audenc_formats[k];
}

int stm_v4l2_encoder_aud_try_fmt(struct file *file, void *fh, struct v4l2_format *f)
{
	struct stm_v4l2_audenc_enum_fmt *fmt;
	struct v4l2_audenc_format* aud_fmt = (struct v4l2_audenc_format*)f->fmt.raw_data;
	struct stm_v4l2_encoder_device *dev_p = file_to_stm_v4l2_encoder_dev(file);

	if (dev_p == NULL)
		return -EINVAL;

	fmt = find_aud_enum_format(f);
	if(fmt == NULL)
		return -EINVAL;

	switch (f->type) {
	case V4L2_BUF_TYPE_VIDEO_OUTPUT :
		if (aud_fmt->codec != V4L2_MPEG_AUDIO_STM_ENCODING_PCM)
			return -EINVAL;
		break;
	case V4L2_BUF_TYPE_VIDEO_CAPTURE :
		if (aud_fmt->codec != V4L2_MPEG_AUDIO_STM_ENCODING_AC3 &&
			aud_fmt->codec != V4L2_MPEG_AUDIO_STM_ENCODING_AAC &&
			aud_fmt->codec != V4L2_MPEG_AUDIO_STM_ENCODING_HEAAC &&
			aud_fmt->codec != V4L2_MPEG_AUDIO_STM_ENCODING_LAYER_3)
			return -EINVAL;
		break;
	default :
		return -EINVAL;
	}

	return 0;
}

static int stm_v4l2_encoder_aud_s_fmt_output(struct file *file, void *fh,
					     struct v4l2_format *f)
{
	struct v4l2_audenc_format* aud_fmt = (struct v4l2_audenc_format*)f->fmt.raw_data;
	audio_encode_params_t *encode_param_p;
	int codec;
	int max_channels;
	int ret;
	struct stm_v4l2_encoder_device *dev_p = file_to_stm_v4l2_encoder_dev(file);

	ret = stm_v4l2_encoder_aud_try_fmt(file, fh, f);
	if (ret)
		return ret;

	codec = aud_fmt->codec;
	max_channels = aud_fmt->max_channels;
	if(max_channels==0) max_channels=8;

	dev_p->src_fmt.sizebuf = max_channels * AUDIO_BUFFER_ONE_CHANNEL;

	encode_param_p = &dev_p->encode_params.audio;
	encode_param_p->src_codec = codec;

	return 0;
}

static int stm_v4l2_encoder_aud_s_fmt_capture(struct file *file, void *fh,
					      struct v4l2_format *f)
{
	struct v4l2_audenc_format* aud_fmt = (struct v4l2_audenc_format*)f->fmt.raw_data;
	audio_encode_params_t *encode_param_p;
	char enc_stream_name[32];
	int codec=0;
	int max_channels;
	unsigned int table_length;
	int index;
	int ret;
	struct stm_v4l2_encoder_device *dev_p = file_to_stm_v4l2_encoder_dev(file);

	ret = stm_v4l2_encoder_aud_try_fmt(file, fh, f);
	if (ret)
		return ret;

	max_channels = aud_fmt->max_channels;
	if(max_channels==0) max_channels=8;

	table_length = sizeof(stm_v4l2_audenc_codec_table);
	codec = stm_v4l2_convert_v4l2_SE_define( aud_fmt->codec,
			                   stm_v4l2_audenc_codec_table,
	                           table_length);
	if(codec == -1)
		return -EINVAL;

	index = dev_p->encoder_id - dev_p->audenc_offset;
	sprintf(enc_stream_name, "EncAudStream%02d", index);

	ret = stm_se_encode_stream_new( enc_stream_name,
									dev_p->encode_obj,
									codec,
									&dev_p->encode_stream);
	if (ret)
		return -EINVAL;

	dev_p->dst_fmt.sizebuf = max_channels * AUDIO_BUFFER_ONE_CHANNEL;

	encode_param_p = &dev_p->encode_params.audio;
	encode_param_p->codec = codec;

	return 0;
}


int stm_v4l2_encoder_aud_s_fmt(struct file *file, void *fh, struct v4l2_format *f)
{
	int ret=0;
	struct stm_v4l2_encoder_device *dev_p = file_to_stm_v4l2_encoder_dev(file);

	if (dev_p == NULL)
		return -EINVAL;

	mutex_lock(&dev_p->lock);

	if(f->type == V4L2_BUF_TYPE_VIDEO_OUTPUT)
		ret = stm_v4l2_encoder_aud_s_fmt_output(file, fh, f);
	else if(f->type == V4L2_BUF_TYPE_VIDEO_CAPTURE)
		ret = stm_v4l2_encoder_aud_s_fmt_capture(file, fh, f);

	mutex_unlock(&dev_p->lock);

	return ret;
}

int stm_v4l2_encoder_aud_s_parm(struct file *file, void *fh,
			struct v4l2_streamparm *param_p)
{
	/* does not use s_param */
	return -EINVAL;
}

int stm_v4l2_encoder_aud_reqbufs(struct file *file, void *fh,
			  struct v4l2_requestbuffers *reqbufs)
{
	struct vb2_queue *vq;
	int ret=0;
	struct stm_v4l2_encoder_device *dev_p = file_to_stm_v4l2_encoder_dev(file);

	mutex_lock(&dev_p->lock);

	if(reqbufs->type == V4L2_BUF_TYPE_VIDEO_OUTPUT)
		vq = &dev_p->src_vq;
	else if(reqbufs->type == V4L2_BUF_TYPE_VIDEO_CAPTURE)
		vq = &dev_p->dst_vq;
	else {
		ret = -EINVAL;
		goto error;
	}

	ret = vb2_reqbufs(vq, reqbufs);

error:
	mutex_unlock(&dev_p->lock);
	return ret;
}

int stm_v4l2_encoder_aud_querybuf(struct file *file, void *fh,
			   struct v4l2_buffer *buf)
{
	struct vb2_queue *vq;
	int ret=0;
	int i;
	struct stm_v4l2_encoder_device *dev_p = file_to_stm_v4l2_encoder_dev(file);

	mutex_lock(&dev_p->lock);

	if(buf->type == V4L2_BUF_TYPE_VIDEO_OUTPUT)
		vq = &dev_p->src_vq;
	else if(buf->type == V4L2_BUF_TYPE_VIDEO_CAPTURE)
		vq = &dev_p->dst_vq;
	else {
		ret = -EINVAL;
		goto error;
	}

	ret = vb2_querybuf(vq, buf);

	/* adjust MMAP memory offsets for the CAPTURE queue */
	if (buf->memory == V4L2_MEMORY_MMAP && !V4L2_TYPE_IS_OUTPUT(vq->type)) {
		if (V4L2_TYPE_IS_MULTIPLANAR(vq->type)) {
			for (i = 0; i < buf->length; ++i)
				buf->m.planes[i].m.mem_offset
					+= STM_AUDENC_DST_QUEUE_OFF_BASE;
		} else {
			buf->m.offset += STM_AUDENC_DST_QUEUE_OFF_BASE;
		}
	}

error:
	mutex_unlock(&dev_p->lock);
	return ret;
}

int stm_v4l2_encoder_aud_qbuf(struct file *file, void *fh, struct v4l2_buffer *buf)
{
	struct vb2_queue *vq;
	int ret=0;
	struct stm_v4l2_encoder_device *dev_p = file_to_stm_v4l2_encoder_dev(file);

	mutex_lock(&dev_p->lock);

	if(buf->type == V4L2_BUF_TYPE_VIDEO_OUTPUT) {
		vq = &dev_p->src_vq;
		dev_p->enc_dev.audio.src_v4l2_buf = *buf;
	}
	else if(buf->type == V4L2_BUF_TYPE_VIDEO_CAPTURE) {
		vq = &dev_p->dst_vq;
		dev_p->enc_dev.audio.dst_v4l2_buf = *buf;
	}
	else {
		ret = -EINVAL;
		goto error;
	}

	ret = vb2_qbuf(vq, buf);

error:
	mutex_unlock(&dev_p->lock);
	return ret;
}

int stm_v4l2_encoder_aud_dqbuf(struct file *file, void *fh, struct v4l2_buffer *buf)
{
	struct vb2_queue *vq;
	int ret=0;
	struct stm_v4l2_encoder_device *dev_p = file_to_stm_v4l2_encoder_dev(file);

	mutex_lock(&dev_p->lock);

	if(buf->type == V4L2_BUF_TYPE_VIDEO_OUTPUT)
		vq = &dev_p->src_vq;
	else if(buf->type == V4L2_BUF_TYPE_VIDEO_CAPTURE) {
		vq = &dev_p->dst_vq;
		dev_p->enc_dev.audio.dst_v4l2_buf = *buf;
		dev_p->enc_dev.audio.file_flag = file->f_flags;
	}
	else {
		ret = -EINVAL;
		goto error;
	}

	ret = vb2_dqbuf(vq, buf, file->f_flags & O_NONBLOCK);

error:
	mutex_unlock(&dev_p->lock);
	return ret;
}

int stm_v4l2_encoder_aud_streamon(struct file *file, void *fh,
			   enum v4l2_buf_type type)
{
	char memsink_name[32];
	struct media_pad *src_pad;
	int index, id = 0;
	int ret;
	struct stm_v4l2_encoder_device *dev_p = file_to_stm_v4l2_encoder_dev(file);

	src_pad = &dev_p->encoder_pad[STM_ENCODE_SINK_PAD];

	if(type == V4L2_BUF_TYPE_VIDEO_OUTPUT) {

		mutex_lock(&dev_p->lock);

		/*
		 * If this encoder is configured to be connected to decoder,
		 * then reject any injection from application
		 */
		if (stm_media_find_remote_pad_with_type(src_pad,
					MEDIA_LNK_FL_ENABLED,
					MEDIA_ENT_T_DVB_SUBDEV_AUDIO_DECODER,
					&id)) {
			mutex_unlock(&dev_p->lock);
			return -EBUSY;
		}
		mutex_unlock(&dev_p->lock);

		/* wait for the signal from capture streamon */
		down(&dev_p->sema);

		mutex_lock(&dev_p->lock);
		ret = vb2_streamon(&dev_p->src_vq, type);
		if(ret==0)
			dev_p->src_connect_type = STM_V4L2_ENCODE_CONNECT_INJECT;
		mutex_unlock(&dev_p->lock);
	}
	else if(type == V4L2_BUF_TYPE_VIDEO_CAPTURE) {

		mutex_lock(&dev_p->lock);

		ret = vb2_streamon(&dev_p->dst_vq, type);
		if(ret) {
			mutex_unlock(&dev_p->lock);
			return ret;
		}

		index = dev_p->encoder_id - dev_p->audenc_offset;
		sprintf(memsink_name,"EncAudSink%02d", index);
		ret = stm_memsink_new( memsink_name,
								STM_IOMODE_BLOCKING_IO,
								KERNEL,
								(stm_memsink_h*)&dev_p->encode_sink);
		if(ret) {
			dev_p->encode_sink = NULL;
			mutex_unlock(&dev_p->lock);
			return -EIO;
		}

		ret = stm_encoder_event_init(dev_p);
		if(ret) {
			printk(KERN_ERR "%s: failed to register events (%d)\n",
					 __func__, ret);
			ret = stm_memsink_delete((stm_memsink_h)dev_p->encode_sink);
			if(ret)
				printk(KERN_ERR "error : aud : stm_memsink_delete\n");
			dev_p->encode_sink = NULL;
			mutex_unlock(&dev_p->lock);
			return -EIO;
		}

		ret = stm_se_encode_stream_attach(dev_p->encode_stream, dev_p->encode_sink);
		if (ret) {
			printk(KERN_ERR "error : aud: stm_se_encode_stream_attach\n");
			stm_encoder_event_remove(dev_p);
			ret = stm_memsink_delete((stm_memsink_h)dev_p->encode_sink);
			if(ret)
				printk(KERN_ERR "error : aud : stm_memsink_delete\n");
			dev_p->encode_sink = NULL;
			mutex_unlock(&dev_p->lock);
			return -EIO;
		}

		/*
		 * Connect decoder -> encoder. Notice, here the locking.
		 * Decoder first take dec->decops_mutex (dec=aud/vid), so,
		 * we need to take lock in the same order to avoid the
		 * potential deadlock (label: encoder_locking).
		 */
		mutex_unlock(&dev_p->lock);

		ret = stm_encoder_connect_decoder(src_pad, NULL,
			MEDIA_ENT_T_DVB_SUBDEV_AUDIO_DECODER);
		if(ret && (ret != -EALREADY)) {
			printk(KERN_ERR "can't attach the encoder\n");
			stm_encoder_event_remove(dev_p);
			ret = stm_memsink_delete((stm_memsink_h)dev_p->encode_sink);
			dev_p->encode_sink = NULL;
			return -EIO;
		}

		/* singnal to ouput streamon */
		up(&dev_p->sema);

	}
	else
		ret = -EINVAL;

	return ret;
}

static int encoder_aud_stream_close(struct stm_v4l2_encoder_device *dev_p)
{
	int ret=0;
	int err=0;
	struct media_pad *src_pad;

	/*
	 * Disconnect decoder -> encoder
	 * grep encoder_locking to see the locking detail
	 */
	mutex_unlock(&dev_p->lock);

	src_pad = &dev_p->encoder_pad[STM_ENCODE_SINK_PAD];

	ret = stm_encoder_disconnect_decoder(src_pad, NULL,
				MEDIA_ENT_T_DVB_SUBDEV_AUDIO_DECODER);
	if (ret)
		printk(KERN_ERR "Failed to disconnect encoder from decoder\n");

	mutex_lock(&dev_p->lock);

	if(dev_p->src_connect_type == STM_V4L2_ENCODE_CONNECT_INJECT)
		dev_p->src_connect_type = STM_V4L2_ENCODE_CONNECT_NONE;

	if(dev_p->encode_stream && dev_p->encode_sink) {
		ret = stm_se_encode_stream_detach(dev_p->encode_stream, dev_p->encode_sink);
		if (ret) {
			printk(KERN_ERR "aud_streamoff: stream_detach error=%d\n", ret);
			err++;
		}
	}
	if(dev_p->encode_sink) {
		/* Release the lock to be sure the callback isn't blocked */
		mutex_unlock(&dev_p->lock);
		stm_encoder_event_remove(dev_p);
		mutex_lock(&dev_p->lock);

		ret = stm_memsink_delete((stm_memsink_h)dev_p->encode_sink);
		if (ret) {
			printk(KERN_ERR "aud_streamoff: memsink_delete error=%d\n", ret);
			err++;
		}
		dev_p->encode_sink = NULL;
	}

	INIT_LIST_HEAD(&dev_p->active_cap);
	dev_p->flags = 0;

	if (err)
		ret = -EIO;

	return ret;
}

int stm_v4l2_encoder_aud_streamoff(struct file *file, void *fh,
			    enum v4l2_buf_type type)
{
	int ret=0;
	int err=0;
	struct stm_v4l2_encoder_device *dev_p = file_to_stm_v4l2_encoder_dev(file);

	mutex_lock(&dev_p->lock);

	if(type == V4L2_BUF_TYPE_VIDEO_OUTPUT) {
		if(dev_p->src_connect_type == STM_V4L2_ENCODE_CONNECT_INJECT)
			dev_p->src_connect_type = STM_V4L2_ENCODE_CONNECT_NONE;
		ret = vb2_streamoff(&dev_p->src_vq, type);
	}
	else if (type == V4L2_BUF_TYPE_VIDEO_CAPTURE) {
		ret = encoder_aud_stream_close(dev_p);
		if (ret)
			err++;
		ret = vb2_streamoff(&dev_p->dst_vq, type);
		if (ret)
			err++;

		if (err)
			ret = -EIO;
	}
	else
		ret = -EINVAL;

	mutex_unlock(&dev_p->lock);
	return ret;
}

/*-----------------------------------------------------------------*/

/* file operation */

int stm_v4l2_encoder_aud_mmap(struct file *file, struct vm_area_struct *vma)
{
	unsigned long offset = vma->vm_pgoff << PAGE_SHIFT;
	struct vb2_queue *vq;
	int ret;
	struct stm_v4l2_encoder_device *dev_p = file_to_stm_v4l2_encoder_dev(file);

	mutex_lock(&dev_p->lock);

	if (offset < STM_AUDENC_DST_QUEUE_OFF_BASE) {
		vq = &dev_p->src_vq;
	} else {
		vq = &dev_p->dst_vq;
		vma->vm_pgoff -= (STM_AUDENC_DST_QUEUE_OFF_BASE >> PAGE_SHIFT);
	}

	ret = vb2_mmap(vq, vma);

	mutex_unlock(&dev_p->lock);
	return ret;
}

unsigned int stm_v4l2_aud_encoder_poll(struct file *file,
								struct poll_table_struct *wait)
{
	struct stm_v4l2_encoder_device *dev_p = file_to_stm_v4l2_encoder_dev(file);

	/* We only do poll on the destination buffer */
	if(dev_p->dst_vq_set)
		return vb2_poll(&dev_p->dst_vq, file, wait);
	else
		return 0;
}

int stm_v4l2_encoder_aud_close(struct file *file)
{
	audio_encode_params_t *encode_param_p;
	int ret=0;
	int err=0;
	struct stm_v4l2_encoder_device *dev_p = file_to_stm_v4l2_encoder_dev(file);

	ret = encoder_aud_stream_close(dev_p);
	if (ret)
		err++;

	if(dev_p->encode_stream){
		ret = stm_se_encode_stream_delete(dev_p->encode_stream);
		if(ret) {
			printk(KERN_ERR "aud_close : stream_delete error=%d\n", ret);
			err++;
		}
		dev_p->encode_stream = NULL;
	}

	if(err)
		ret = -EIO;

	encode_param_p = &dev_p->encode_params.audio;
	memset(encode_param_p, 0, sizeof(*encode_param_p));

	return ret;
}

int stm_v4l2_encoder_aud_create_connection(void)
{
	struct media_entity *src;
	struct media_entity *sink;
	int ret;

	src  = stm_media_find_entity_with_type_first(MEDIA_ENT_T_DVB_SUBDEV_AUDIO_DECODER);
	while(src) {

		sink = stm_media_find_entity_with_type_first(MEDIA_ENT_T_V4L2_SUBDEV_AUDIO_ENCODER);
		while(sink) {
			ret = media_entity_create_link(src, 0, sink, 0, 0);
			if (ret < 0) {
				printk(KERN_ERR "failed audio connection \n");
				goto aud_create_err;
			}
			sink = stm_media_find_entity_with_type_next(sink, MEDIA_ENT_T_V4L2_SUBDEV_AUDIO_ENCODER);
		}

		src = stm_media_find_entity_with_type_next(src, MEDIA_ENT_T_DVB_SUBDEV_AUDIO_DECODER);
	}

	return 0;
aud_create_err:
	return ret;
}
