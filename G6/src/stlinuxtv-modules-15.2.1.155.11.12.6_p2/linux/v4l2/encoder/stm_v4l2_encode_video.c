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
 * Implementation of v4l2 video encoder device
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
#include "stm_se.h"

#include "linux/dvb/dvb_v4l2_export.h"

#include "stm_v4l2_common.h"
#include "stm_v4l2_encode.h"
#include "stm_v4l2_encode_ctrls.h"

#include "dvb_module.h"
#include "dvb_video.h"

#include <linux/mm.h>
#include <asm/mach/map.h>

#define STM_VIDENC_DST_QUEUE_OFF_BASE      (1 << 30)

#define STM_VIDEO_ENCODER_MPLANE_NB 	2

struct stm_v4l2_videnc_enum_fmt {
	char *name;
	u32 fourcc;
	u32 types;
	u32 flags;
};

static struct stm_v4l2_videnc_enum_fmt stm_v4l2_videnc_formats[] = {
	{
	 .name = "4:2:2, UYVY",
	 .fourcc = V4L2_PIX_FMT_UYVY,
	 .types = V4L2_BUF_TYPE_VIDEO_OUTPUT,
	 .flags = 0,
	 },
	{
	 .name = "4:2:2, UYVY",
	 .fourcc = V4L2_PIX_FMT_UYVY,
	 .types = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE,
	 .flags = 0,
	 },
	{
	 .name = "4:2:0, YUV",
	 .fourcc = V4L2_PIX_FMT_NV12,
	 .types = V4L2_BUF_TYPE_VIDEO_OUTPUT,
	 .flags = 0,
	 },
	{
	 .name = "4:2:0, YUV",
	 .fourcc = V4L2_PIX_FMT_NV12,
	 .types = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE,
	 .flags = 0,
	 },
	{
	 .name = "H264",
	 .fourcc = V4L2_PIX_FMT_H264,
	 .types = V4L2_BUF_TYPE_VIDEO_CAPTURE,
	 .flags = V4L2_FMT_FLAG_COMPRESSED,
	 },
	{
	 .name = "H264",
	 .fourcc = V4L2_PIX_FMT_H264,
	 .types = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE,
	 .flags = V4L2_FMT_FLAG_COMPRESSED,
	 },
	 {
	  .name = "4:2:2, YUYV",
	  .fourcc = V4L2_PIX_FMT_YUYV,
	  .types = V4L2_BUF_TYPE_VIDEO_OUTPUT,
	  .flags = 0,
	  },
	 {
	  .name = "4:2:2, YUYV",
	  .fourcc = V4L2_PIX_FMT_YUYV,
	  .types = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE,
	  .flags = 0,
	  },
};

static const int stm_v4l2_videnc_map_colorspace[][2] = {
	{ V4L2_COLORSPACE_UNSPECIFIED,   STM_SE_COLORSPACE_UNSPECIFIED    },
	{V4L2_COLORSPACE_SMPTE170M, STM_SE_COLORSPACE_SMPTE170M},
	{V4L2_COLORSPACE_SMPTE240M, STM_SE_COLORSPACE_SMPTE240M},
	{V4L2_COLORSPACE_REC709, STM_SE_COLORSPACE_BT709},
	{V4L2_COLORSPACE_470_SYSTEM_M, STM_SE_COLORSPACE_BT470_SYSTEM_M},
	{V4L2_COLORSPACE_470_SYSTEM_BG, STM_SE_COLORSPACE_BT470_SYSTEM_BG},
	{V4L2_COLORSPACE_SRGB, STM_SE_COLORSPACE_SRGB},
};

static int stm_v4l2_videnc_encoding_output(struct stm_v4l2_encoder_device *dev_p,
							struct vb2_buffer *vb);

/**
 * stm_encoder_vid_link_setup() - connect video encoder to video decoder
 * Callback from media-controller when any change in connection status is
 * detected. This is also called from inside decoder link_setup to verify
 * that this encoder is still available for connection.
 */
static int stm_encoder_vid_link_setup(struct media_entity *entity,
			      const struct media_pad *local,
			      const struct media_pad *remote, u32 flags)
{
	if (remote->entity->type == MEDIA_ENT_T_DVB_SUBDEV_VIDEO_DECODER)
		return stm_encoder_link_setup(local,
					remote->entity->type, flags);

	return 0;
}

static const struct media_entity_operations encoder_vid_media_ops = {
	.link_setup = stm_encoder_vid_link_setup,
};

const struct v4l2_subdev_core_ops enc_vid_core_ops = {
	.queryctrl = v4l2_subdev_queryctrl,
	.querymenu = v4l2_subdev_querymenu,
	.g_ctrl = v4l2_subdev_g_ctrl,
	.s_ctrl = v4l2_subdev_s_ctrl,
	.g_ext_ctrls = v4l2_subdev_g_ext_ctrls,
	.try_ext_ctrls = v4l2_subdev_try_ext_ctrls,
	.s_ext_ctrls = v4l2_subdev_s_ext_ctrls,
	};

static const struct v4l2_subdev_ops encoder_vid_subdev_ops = {
	.core = &enc_vid_core_ops,
};

int stm_v4l2_encoder_vid_init_subdev(void *dev)
{
	struct stm_v4l2_encoder_device *dev_p = dev;
	struct v4l2_subdev *subdev = &dev_p->encoder_subdev;
	struct media_pad *pads = dev_p->encoder_pad;
	struct media_entity *me = &subdev->entity;
	int ret;

	/* Initialize the V4L2 subdev / MC entity */
	v4l2_subdev_init(subdev, &encoder_vid_subdev_ops);
	snprintf(subdev->name, sizeof(subdev->name), "vid-encoder-%02d",
			dev_p->encoder_id);

	v4l2_set_subdevdata(subdev, dev_p);

	pads[0].flags = MEDIA_PAD_FL_SINK;
	pads[1].flags = MEDIA_PAD_FL_SOURCE;

	ret = media_entity_init(me, 2, pads, 0);
	if (ret < 0) {
		printk(KERN_ERR "%s: entity init failed(%d)\n", __func__, ret);
		return ret;
	}

	me->ops = &encoder_vid_media_ops;
	me->type = MEDIA_ENT_T_V4L2_SUBDEV_VIDEO_ENCODER;

	ret = stm_v4l2_vid_enc_init_ctrl(dev_p);
	if(ret){
		printk(KERN_ERR "%s: vid enc ctrl init failed (%d)\n",
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

int stm_v4l2_encoder_vid_exit_subdev(void *dev)
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

static int stm_v4l2_encoder_vid_queue_setup(struct vb2_queue *vq,
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
	case V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE:
		*nplanes = STM_VIDEO_ENCODER_MPLANE_NB;
		*nbuffers = count;
		sizes[0] = dev_p->src_fmt.sizebuf;
		sizes[1] = dev_p->src_fmt.sizebuf;
		alloc_ctxs[0] = dev_p->alloc_ctx;
		break;
	case V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE:
		*nplanes = STM_VIDEO_ENCODER_MPLANE_NB;
		*nbuffers = count;
		sizes[0] = dev_p->dst_fmt.sizebuf;
		sizes[1] = dev_p->dst_fmt.sizebuf;
		alloc_ctxs[0] = dev_p->alloc_ctx;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int stm_v4l2_encoder_vid_buf_prepare(struct vb2_buffer *vb)
{
	struct vb2_queue *vq = vb->vb2_queue;
	struct stm_v4l2_encoder_device *dev_p = vb2_get_drv_priv(vb->vb2_queue);

	switch (vq->type) {
	case V4L2_BUF_TYPE_VIDEO_OUTPUT:
	case V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE:
		break;

	case V4L2_BUF_TYPE_VIDEO_CAPTURE:
		vb2_set_plane_payload(vb, 0, dev_p->dst_fmt.sizebuf);
		break;

	case V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE:
		vb2_set_plane_payload(vb, 0, dev_p->dst_fmt.sizebuf);
		vb2_set_plane_payload(vb, 1, dev_p->dst_fmt.sizebuf);
		break;

	default:
		return -EINVAL;
	}

	return 0;
}

static void stm_encoder_video_handle_eos( struct stm_v4l2_encoder_device *dev_p);
int stm_encoder_video_handler(struct stm_v4l2_encoder_device *dev_p);

static void stm_v4l2_encoder_vid_buf_queue(struct vb2_buffer *vb)
{
	struct vb2_queue *vq = vb->vb2_queue;
	struct stm_v4l2_encoder_buf *buf = container_of(vb,
						struct stm_v4l2_encoder_buf,
						vb);
	struct stm_v4l2_encoder_device *dev_p = vb2_get_drv_priv(vb->vb2_queue);
	int ret;

	switch (vq->type) {
	case V4L2_BUF_TYPE_VIDEO_OUTPUT:
	case V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE:
		ret = stm_v4l2_videnc_encoding_output(dev_p, vb);
		if (ret)
			vb2_buffer_done(vb, VB2_BUF_STATE_ERROR);
		else
			vb2_buffer_done(vb, VB2_BUF_STATE_DONE);
		break;
	case V4L2_BUF_TYPE_VIDEO_CAPTURE:
	case V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE:
		list_add_tail(&buf->list, &dev_p->active_cap);

		/* Can only proceed if we are already streaming */
		if (!vb2_is_streaming(vb->vb2_queue))
			return;

		/* We have added a new buffer - lets handle potential EOS
		 * This will (or not) take that buffer if needed */
		stm_encoder_video_handle_eos(dev_p);

		/* Handle data as long has we can */
		while (!list_empty(&dev_p->active_cap)){
			ret = stm_encoder_video_handler(dev_p);
			if (ret && (ret!=-EAGAIN))
				printk(KERN_ERR "%s: video hdl failed (%d)\n",
						__func__, ret);
			else if(ret == -EAGAIN)
				break;
		}
		break;
	default:
		break;
	}
}

static void stm_v4l2_encoder_vid_wait_prepare(struct vb2_queue *q)
{
	struct stm_v4l2_encoder_device *dev_p = vb2_get_drv_priv(q);

	mutex_unlock(&dev_p->lock);
}

static void stm_v4l2_encoder_vid_wait_finish(struct vb2_queue *q)
{
	struct stm_v4l2_encoder_device *dev_p = vb2_get_drv_priv(q);

	mutex_lock(&dev_p->lock);
}

static int stm_v4l2_encoder_vid_buf_finish(struct vb2_buffer *vb)
{
	struct vb2_queue *vq = vb->vb2_queue;
	struct stm_v4l2_encoder_buf *buf = container_of(vb,
						struct stm_v4l2_encoder_buf,
						vb);
	struct v4l2_buffer* v4l2_dst_buf = &buf->vb.v4l2_buf;
	stm_se_capture_buffer_t* metadata_p = &buf->metadata;
	uint64_t encoded_time;
	uint64_t temp_time;
	int encoded_time_format;

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

	if(metadata_p->u.compressed.video.new_gop)
		v4l2_dst_buf->flags |=
			V4L2_BUF_FLAG_STM_ENCODE_GOP_START;

	if(metadata_p->u.compressed.video.closed_gop)
		v4l2_dst_buf->flags |=
			V4L2_BUF_FLAG_STM_ENCODE_CLOSED_GOP;

	switch((metadata_p->u.compressed.video.picture_type)) {
	case STM_SE_PICTURE_TYPE_I:
		v4l2_dst_buf->flags |= V4L2_BUF_FLAG_KEYFRAME;
		break;
	case STM_SE_PICTURE_TYPE_P:
		v4l2_dst_buf->flags |= V4L2_BUF_FLAG_PFRAME;
		break;
	case STM_SE_PICTURE_TYPE_B:
		v4l2_dst_buf->flags |= V4L2_BUF_FLAG_BFRAME;
		break;
	default:
		break;
	}

	/* process out metadata */
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
		printk(KERN_ERR "No time_format from vid encoder\n");
	}

	return 0;
}

static struct vb2_ops stm_v4l2_encoder_vid_qops = {
	.queue_setup = stm_v4l2_encoder_vid_queue_setup,
	.buf_prepare = stm_v4l2_encoder_vid_buf_prepare,
	.buf_queue   = stm_v4l2_encoder_vid_buf_queue,
	.buf_finish   = stm_v4l2_encoder_vid_buf_finish,
	.wait_prepare = stm_v4l2_encoder_vid_wait_prepare,
	.wait_finish = stm_v4l2_encoder_vid_wait_finish,
};

int stm_v4l2_encoder_vid_queue_init(int type, struct vb2_queue *vq, void *priv)
{
	struct stm_v4l2_encoder_device *dev_p = priv;
	int ret;

	memset(vq, 0, sizeof(*vq));
	vq->type = type;
	vq->io_modes = VB2_MMAP | VB2_USERPTR;
	vq->drv_priv = dev_p;
	vq->ops = &stm_v4l2_encoder_vid_qops;
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

void stm_v4l2_encoder_vid_queue_release(struct vb2_queue *vq)
{
	vb2_queue_release(vq);
}

/*-----------------------------------------------------------------*/

/* encoding */

static int stm_v4l2_videnc_encoding_output(struct stm_v4l2_encoder_device *dev_p,
				struct vb2_buffer *vb)
{
	struct v4l2_buffer* v4l2_src_buf= &vb->v4l2_buf;
	stm_se_capture_buffer_t* metadata_p;
	stm_se_uncompressed_frame_metadata_t* meta_p;
	video_encode_params_t *video_params;
	struct timeval in_time;
	void* vaddr=NULL;
	unsigned long paddr=0;
	int src_size=0;
	int ret=0;

	if(vb2_is_streaming(&dev_p->dst_vq)==0) {
		printk(KERN_ERR "can't encode. capture buffer is off\n");
		return -EINVAL;
	}

	/*
	 * Configure video encoder for EOS discontinuity. An EOS packet
	 * is sent with 0 payload.
	 */
	if (!vb2_get_plane_payload(vb, 0)) {
	        mutex_unlock(&dev_p->lock);

		ret = stm_se_encode_stream_inject_discontinuity
			(dev_p->encode_stream, STM_SE_DISCONTINUITY_EOS);
		if (ret)
			printk(KERN_ERR "Unable to configure video encoder "
					"for EOS, ret: %d\n", ret);

	        mutex_lock(&dev_p->lock);

		return ret;
	}

	/* check src parameters */
	if(dev_p->encode_params.video.input_framerate_num==0 || dev_p->encode_params.video.input_framerate_den==0) {
		printk(KERN_ERR "Error : need to set src framerate\n");
		return -EINVAL;
	}

	in_time = v4l2_src_buf->timestamp;

	if(vb->num_planes > 1) {
		/* TODO : to support milti-plane */
	}
	else {
		paddr = *(unsigned long *)vb2_plane_cookie(vb, 0);
		if (!paddr){
			printk(KERN_ERR "%s: failed to get paddr\n", __func__);
			return -EIO;
		}
		vaddr = vb2_plane_vaddr(vb, 0);
	}

	/* process set parameters */
	src_size = dev_p->src_fmt.sizebuf;

	video_params = &dev_p->encode_params.video;

	/* process src metadata */
	metadata_p = &dev_p->enc_dev.video.src_metadata;
	memset(metadata_p, 0, sizeof(stm_se_capture_buffer_t));

	metadata_p->virtual_address = (void*)vaddr;
	metadata_p->buffer_length = src_size;

	meta_p = &metadata_p->u.uncompressed;

	/*
	 * Configure the injected data for following discontinuities
	 * a. Time discontinuity
	 * c. GOP discontinuity
	 */
	if (v4l2_src_buf->flags & V4L2_BUF_FLAG_STM_ENCODE_TIME_DISCONTINUITY)
		meta_p->discontinuity = STM_SE_DISCONTINUITY_DISCONTINUOUS;
	if (v4l2_src_buf->flags & V4L2_BUF_FLAG_STM_ENCODE_CLOSED_GOP_REQUEST)
		meta_p->discontinuity |=
				 STM_SE_DISCONTINUITY_CLOSED_GOP_REQUEST;

	meta_p->system_time = stm_v4l2_get_systemtime_us();
	meta_p->native_time_format = TIME_FORMAT_US;
	meta_p->native_time = (uint64_t)in_time.tv_sec*1000000 + (uint64_t)in_time.tv_usec;

	/* The following commented fields may be useful in the future.
	   meta_p->user_data_size = ;
	   meta_p->user_data_buffer_address = ;
	 */

	meta_p->media = STM_SE_ENCODE_STREAM_MEDIA_VIDEO;

	meta_p->video.video_parameters.width  = video_params->width;
	meta_p->video.video_parameters.height = video_params->height;
	/* Miss for now information on display aspect ratio of injected frame */
	meta_p->video.video_parameters.aspect_ratio = STM_SE_ASPECT_RATIO_UNSPECIFIED;
	meta_p->video.video_parameters.colorspace = video_params->colorspace;
	meta_p->video.video_parameters.scan_type  = video_params->scan_type;
	meta_p->video.top_field_first             = video_params->top_field_first;
	meta_p->video.video_parameters.pixel_aspect_ratio_numerator = 1;
	meta_p->video.video_parameters.pixel_aspect_ratio_denominator = 1;
	/* The following commented fields may be useful in the future.
	   meta_p->video.video_parameters.format_3d= ;
	   meta_p->video.video_parameters.left_right_format= ;
	 */
	meta_p->video.window_of_interest.x = 0;
	meta_p->video.window_of_interest.y = 0;
	meta_p->video.window_of_interest.width  = video_params->width;
	meta_p->video.window_of_interest.height = video_params->height;
	meta_p->video.frame_rate.framerate_num  = video_params->input_framerate_num;
	meta_p->video.frame_rate.framerate_den  = video_params->input_framerate_den;
	meta_p->video.pitch          = video_params->pitch;
	meta_p->video.picture_type   = STM_SE_PICTURE_TYPE_UNKNOWN;
	meta_p->video.surface_format = video_params->surface_format;

	meta_p->video.vertical_alignment = video_params->vertical_alignment;

	mutex_unlock(&dev_p->lock);

	/*
	 * Inject the decoded video frame into encoder
	 */
	ret = stm_se_encode_stream_inject_frame(dev_p->encode_stream,
					vaddr, paddr, src_size,
					metadata_p->u.uncompressed);
	if(ret)
		printk(KERN_ERR "video encode error = %x\n", ret);

	mutex_lock(&dev_p->lock);

	return ret;
}

/**
 * stm_encoder_video_handle_eos
 * This function verify if we have received a EOS discontinuity and push
 * the information into AN ALREADY AVAILABLE vb2_buffer
 */
static void stm_encoder_video_handle_eos( struct stm_v4l2_encoder_device *dev_p)
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

int stm_encoder_video_handler(struct stm_v4l2_encoder_device *dev_p)
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
	metadata_p->virtual_address = vaddr;
	metadata_p->buffer_length = dev_p->dst_fmt.sizebuf;

	ret = stm_memsink_pull_data(dev_p->encode_sink,
	                            metadata_p,
	                            metadata_p->buffer_length,
	                            &size);
	if(ret) {
		printk(KERN_ERR "Error : SE vid read error=%d\n", ret);
		goto done;
	}

	/*
	 * SKIPPED FRAME are not handled at all for now
	 * If we receive the Frame skipped discontinuity, then clear
	 * it now, so, that it is not forwarded in the next DQBUF
	 */
	if (metadata_p->u.compressed.discontinuity &
				STM_SE_DISCONTINUITY_FRAME_SKIPPED){
		printk (KERN_INFO "%s: Skipped frame detected\n", __func__);
		goto done;
	} else
		metadata_p->u.compressed.discontinuity &=
					~STM_SE_DISCONTINUITY_FRAME_SKIPPED;

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

	/*
	 * For the rest of the discontinuites, following flags are set and
	 * relevant data filled in in buf_finish handler
	 */
	vb2_set_plane_payload(&buf->vb, 0, metadata_p->payload_length);
	vb2_buffer_done(&buf->vb, VB2_BUF_STATE_DONE);

	/* Check if we need (and can) push another buffer for the EOS */
	if (!list_empty(&dev_p->active_cap))
		stm_encoder_video_handle_eos(dev_p);

done:
	return ret;
}

/*-----------------------------------------------------------------*/

/* video ioctls */

int stm_v4l2_encoder_enum_vid_output(struct v4l2_output *output,
				     void *dev, int index)
{
	struct stm_v4l2_encoder_device *dev_p = dev;
	struct v4l2_subdev *remote_sd;

	memset(output, 0, sizeof(struct v4l2_output));
	remote_sd = &dev_p->encoder_subdev;
	strlcpy(output->name, remote_sd->name, sizeof(output->name));
	output->index = index;

	return 0;
}

int stm_v4l2_encoder_enum_vid_input(struct v4l2_input *input,
				    void *dev, int index)
{
	struct stm_v4l2_encoder_device *dev_p = dev;
	struct v4l2_subdev *remote_sd;

	memset(input, 0, sizeof(struct v4l2_input));
	remote_sd = &dev_p->encoder_subdev;
	strlcpy(input->name, remote_sd->name, sizeof(input->name));
	input->index = index;

	return 0;
}

int stm_v4l2_encoder_vid_enum_fmt(struct file *file, void *fh,
				  struct v4l2_fmtdesc *f)
{
	struct stm_v4l2_videnc_enum_fmt *fmt;
	int i, num, size;
	struct stm_v4l2_encoder_device *dev_p = file_to_stm_v4l2_encoder_dev(file);

	if (dev_p == NULL)
		return -EINVAL;

	size = ARRAY_SIZE(stm_v4l2_videnc_formats);
	num = 0;

	for (i = 0; i < size; ++i) {
		if (stm_v4l2_videnc_formats[i].types == f->type) {
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
		fmt = &stm_v4l2_videnc_formats[i];
		f->flags = fmt->flags;
		strlcpy(f->description, fmt->name, sizeof(f->description));
		f->pixelformat = fmt->fourcc;
		memset(f->reserved, 0, sizeof(f->reserved));
		return 0;
	}

	/* Format not found */
	return -EINVAL;
}

static struct stm_v4l2_videnc_enum_fmt *find_vid_enum_format(struct v4l2_format *f)
{
	struct stm_v4l2_videnc_enum_fmt *fmt;
	int size;
	int k;

	size = ARRAY_SIZE(stm_v4l2_videnc_formats);

	for (k = 0; k < size; k++) {
		fmt = &stm_v4l2_videnc_formats[k];
		if (fmt->fourcc == f->fmt.pix.pixelformat)
			break;
	}

	if (k == size)
		return NULL;

	return &stm_v4l2_videnc_formats[k];
}

int stm_v4l2_encoder_vid_try_fmt(struct file *file, void *fh, struct v4l2_format *f)
{
	struct stm_v4l2_videnc_enum_fmt *fmt;
	struct stm_v4l2_encoder_device *dev_p = file_to_stm_v4l2_encoder_dev(file);

	if (dev_p == NULL)
		return -EINVAL;

	fmt = find_vid_enum_format(f);
	if (fmt == NULL)
		return -EINVAL;

	switch (f->type) {
	case V4L2_BUF_TYPE_VIDEO_OUTPUT:
		if (f->fmt.pix.pixelformat != V4L2_PIX_FMT_UYVY &&
		    f->fmt.pix.pixelformat != V4L2_PIX_FMT_NV12 &&
		    f->fmt.pix.pixelformat != V4L2_PIX_FMT_YUYV)
			return -EINVAL;
		break;
	case V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE:
		if (f->fmt.pix_mp.pixelformat != V4L2_PIX_FMT_UYVY &&
		    f->fmt.pix_mp.pixelformat != V4L2_PIX_FMT_NV12 &&
		    f->fmt.pix_mp.pixelformat != V4L2_PIX_FMT_YUYV)
			return -EINVAL;

		if (f->fmt.pix_mp.num_planes != STM_VIDEO_ENCODER_MPLANE_NB)
			return -EINVAL;
		break;
	case V4L2_BUF_TYPE_VIDEO_CAPTURE:
		if (f->fmt.pix.pixelformat != V4L2_PIX_FMT_H264)
			return -EINVAL;
		break;
	case V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE:
		if (f->fmt.pix_mp.pixelformat != V4L2_PIX_FMT_H264)
			return -EINVAL;
		if (f->fmt.pix_mp.num_planes != STM_VIDEO_ENCODER_MPLANE_NB)
			return -EINVAL;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int stm_v4l2_encoder_vid_s_fmt_output(struct file *file, void *fh,
					     struct v4l2_format *f)
{
	video_encode_params_t *encode_param_p;
	int width, buf_width;
	int height, buf_height;
	int pixelformat;
	int colorspace;
	int field;
	int table_length;
	int ret;
	struct stm_v4l2_encoder_device *dev_p = file_to_stm_v4l2_encoder_dev(file);

	ret = stm_v4l2_encoder_vid_try_fmt(file, fh, f);
	if (ret)
		return ret;

	/* parameters */
	width          = f->fmt.pix.width;
	height         = f->fmt.pix.height;
	pixelformat = f->fmt.pix.pixelformat;
	colorspace  = f->fmt.pix.colorspace;
	field            = f->fmt.pix.field;

	/* check parameters */
	if( field != V4L2_FIELD_NONE &&
	    field != V4L2_FIELD_INTERLACED_TB &&
	    field != V4L2_FIELD_INTERLACED_BT) {
	   printk(KERN_ERR "error : not supported field type\n");
	   return -EINVAL;
	}

	table_length = sizeof(stm_v4l2_videnc_map_colorspace);
	colorspace = stm_v4l2_convert_v4l2_SE_define( colorspace,
	                       stm_v4l2_videnc_map_colorspace,
	                       table_length);
	if(colorspace == -1) {
	    printk(KERN_ERR "error : not supported colorspace type\n");
	    return -EINVAL;
	}

	/* multiples of 32 */
	buf_width  = ((width + 31) >> 5) << 5;
	buf_height = ((height + 31) >> 5) << 5;

	dev_p->src_fmt.type = f->type;
	dev_p->src_fmt.sizebuf = buf_width * buf_height * 2;

	/* set parameters */
	encode_param_p = &dev_p->encode_params.video;

	/* how to set the pitch :
	 planar format : pitch = width
	 other  format : pitch = depth/8 * width
	*/
	encode_param_p->vertical_alignment = 0;

	switch(pixelformat) {

	case V4L2_PIX_FMT_UYVY :
		encode_param_p->surface_format = SURFACE_FORMAT_VIDEO_422_RASTER;
		encode_param_p->pitch = 2 * width;
		break;
	case V4L2_PIX_FMT_NV12 :
		encode_param_p->surface_format = SURFACE_FORMAT_VIDEO_420_RASTER2B;
		encode_param_p->pitch = width;
		encode_param_p->vertical_alignment = 1;
		break;
	case V4L2_PIX_FMT_YUYV :
		encode_param_p->surface_format = SURFACE_FORMAT_VIDEO_422_YUYV;
		encode_param_p->pitch = 2 * width;
		break;
	case V4L2_PIX_FMT_YUV420 :
		encode_param_p->surface_format = SURFACE_FORMAT_VIDEO_420_PLANAR;
		encode_param_p->pitch = width;
		break;
	case V4L2_PIX_FMT_YUV422P :
		encode_param_p->surface_format = SURFACE_FORMAT_VIDEO_422_PLANAR;
		encode_param_p->pitch = width;
		break;
	case V4L2_PIX_FMT_RGB565 :
		encode_param_p->surface_format = SURFACE_FORMAT_VIDEO_565_RGB;
		encode_param_p->pitch = 2 * width;
		break;
	default :
		return -EINVAL;
	}

	if(field == V4L2_FIELD_NONE) {
	   encode_param_p->scan_type = STM_SE_SCAN_TYPE_PROGRESSIVE;
	   encode_param_p->top_field_first = 0;
	}
	else if(field == V4L2_FIELD_INTERLACED_TB) {
	   encode_param_p->scan_type = STM_SE_SCAN_TYPE_INTERLACED;
	   encode_param_p->top_field_first = 1;
	}
	else if(field == V4L2_FIELD_INTERLACED_BT) {
	   encode_param_p->scan_type = STM_SE_SCAN_TYPE_INTERLACED;
	   encode_param_p->top_field_first = 0;
	}

	encode_param_p->colorspace = colorspace;

	encode_param_p->width = width;
	encode_param_p->height = height;

	ret = stm_se_encode_set_control(dev_p->encode_obj,
				STM_SE_CTRL_VIDEO_ENCODE_INPUT_COLOR_FORMAT,
				encode_param_p->surface_format);
	if (ret) {
		printk(KERN_ERR "%s: encode set control error=%d\n",
				__func__, ret);
		return -EINVAL;
	}

	return 0;
}

static int stm_v4l2_encoder_vid_s_fmt_capture(struct file *file, void *fh,
					      struct v4l2_format *f)
{
	stm_se_picture_resolution_t picture_resolution;
	char enc_stream_name[32];
	int width, buf_width;
	int height, buf_height;
	int pixelformat;
	int index;
	int ret;
	int profile;
	struct stm_v4l2_encoder_device *dev_p = file_to_stm_v4l2_encoder_dev(file);

	ret = stm_v4l2_encoder_vid_try_fmt(file, fh, f);
	if (ret)
		return ret;

	/* parameters */
	width       = f->fmt.pix.width;
	height      = f->fmt.pix.height;
	pixelformat = f->fmt.pix.pixelformat;

	/* multiples of 32 */
	buf_width  = ((width + 31) >> 5) << 5;
	buf_height = ((height + 31) >> 5) << 5;

	/* set the parameters */
	/* Create the encode stream if not existing yet */
	index = dev_p->encoder_id;
	sprintf(enc_stream_name, "EncVidStream%02d", index);


	/* set suitable memory profile taking into account encode resolution*/
	profile = stm_v4l2_encode_convert_resolution_to_profile(width, height);
	if (profile < 0) {
		printk(KERN_ERR "%s: encode res2profile error=%d\n",
				__func__, profile);
		return -EINVAL;
	}

	ret = stm_se_encode_set_control(dev_p->encode_obj,
					STM_SE_CTRL_VIDEO_ENCODE_MEMORY_PROFILE,
					profile);
	if (ret) {
		printk(KERN_ERR "%s: encode set control error=%d\n",
				__func__, ret);
		return -EINVAL;
	}

	ret = stm_se_encode_stream_new( enc_stream_name,
									dev_p->encode_obj,
									STM_SE_ENCODE_STREAM_ENCODING_VIDEO_H264,
									&dev_p->encode_stream);
	if (ret)
		return -EINVAL;

	picture_resolution.width  = width;
	picture_resolution.height = height;

	ret = stm_se_encode_stream_set_compound_control(dev_p->encode_stream,
	                          STM_SE_CTRL_VIDEO_ENCODE_STREAM_RESOLUTION,
	                          (void *)&picture_resolution);
	if (ret) {
		ret = stm_se_encode_stream_delete(dev_p->encode_stream);
		if(ret)
			printk(KERN_ERR "stm_v4l2_encoder_vid_s_fmt_capture : stream_delete error=%d\n", ret);
		dev_p->encode_stream = NULL;/* there is a chance of double delete when close is done */
		return -EIO;
	}

	dev_p->dst_fmt.type = f->type;
	/* set coded buffer size to max theoretical size = 400 x number of macroblocs (in bytes)*/
	dev_p->dst_fmt.sizebuf = 400 * (buf_width/16 * buf_height/16) + STM_V4L2_ENCODE_VIDEO_MAX_HEADER_SIZE;

	return 0;
}

int stm_v4l2_encoder_vid_s_fmt(struct file *file, void *fh, struct v4l2_format *f)
{
	int ret=0;
	struct stm_v4l2_encoder_device *dev_p = file_to_stm_v4l2_encoder_dev(file);

	if (dev_p == NULL)
		return -EINVAL;

	mutex_lock(&dev_p->lock);

	switch(f->type) {
	case V4L2_BUF_TYPE_VIDEO_OUTPUT :
	case V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE :
		ret = stm_v4l2_encoder_vid_s_fmt_output(file, fh, f);
		break;
	case V4L2_BUF_TYPE_VIDEO_CAPTURE :
	case V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE :
		ret = stm_v4l2_encoder_vid_s_fmt_capture(file, fh, f);
		break;
	default :
		ret = -EINVAL;
		break;
	}

	mutex_unlock(&dev_p->lock);

	return ret;
}

int stm_v4l2_encoder_vid_s_parm(struct file *file, void *fh,
				struct v4l2_streamparm *param_p)
{
	struct v4l2_fract *timeperframe_p = NULL;
	video_encode_params_t *encode_param_p;
	stm_se_framerate_t framerate;
	int ret = 0;
	struct stm_v4l2_encoder_device *dev_p = file_to_stm_v4l2_encoder_dev(file);

	if (dev_p == NULL)
		return -EINVAL;

	if(V4L2_TYPE_IS_OUTPUT(param_p->type))
		timeperframe_p = &param_p->parm.output.timeperframe;
	else
		timeperframe_p = &param_p->parm.capture.timeperframe;

	if (timeperframe_p == NULL)
		return -EINVAL;

	/* numerator/denominator to be inverted */
	/* and multiplied by STM_SE_PLAY_FRAME_RATE_MULTIPLIER */
	framerate.framerate_num = timeperframe_p->denominator;
	framerate.framerate_den = timeperframe_p->numerator;

	mutex_lock(&dev_p->lock);

	encode_param_p = &dev_p->encode_params.video;

	switch (param_p->type) {

	case V4L2_BUF_TYPE_VIDEO_OUTPUT:
	case V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE:
		/* framerate for input metadata */
		encode_param_p->input_framerate_num = framerate.framerate_num;
		encode_param_p->input_framerate_den = framerate.framerate_den;
		break;

	case V4L2_BUF_TYPE_VIDEO_CAPTURE:
	case V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE:
		/* target framerate */
		ret = stm_se_encode_stream_set_compound_control(dev_p->encode_stream,
		                  STM_SE_CTRL_VIDEO_ENCODE_STREAM_FRAMERATE,
		                  (void *)&framerate);
		break;

	default:
		ret = -EINVAL;
		break;
	}

	mutex_unlock(&dev_p->lock);

	return ret;
}

int stm_v4l2_encoder_vid_reqbufs(struct file *file, void *fh,
				 struct v4l2_requestbuffers *reqbufs)
{
	struct vb2_queue *vq;
	int ret=0;
	struct stm_v4l2_encoder_device *dev_p = file_to_stm_v4l2_encoder_dev(file);

	mutex_lock(&dev_p->lock);

	switch (reqbufs->type) {
	case V4L2_BUF_TYPE_VIDEO_OUTPUT:
	case V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE:
		vq = &dev_p->src_vq;
		break;
	case V4L2_BUF_TYPE_VIDEO_CAPTURE:
	case V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE:
		vq = &dev_p->dst_vq;
		break;
	default:
		ret = -EINVAL;
		goto error;
	}

	ret = vb2_reqbufs(vq, reqbufs);

error:
	mutex_unlock(&dev_p->lock);
	return ret;
}

int stm_v4l2_encoder_vid_querybuf(struct file *file, void *fh,
				  struct v4l2_buffer *buf)
{
	struct vb2_queue *vq;
	int ret=0;
	int i;
	struct stm_v4l2_encoder_device *dev_p = file_to_stm_v4l2_encoder_dev(file);

	mutex_lock(&dev_p->lock);

	switch (buf->type) {
	case V4L2_BUF_TYPE_VIDEO_OUTPUT:
	case V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE:
		vq = &dev_p->src_vq;
		break;
	case V4L2_BUF_TYPE_VIDEO_CAPTURE:
	case V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE:
		vq = &dev_p->dst_vq;
		break;
	default:
		ret = -EINVAL;
		goto error;
	}

	ret = vb2_querybuf(vq, buf);

	/* adjust MMAP memory offsets for the CAPTURE queue */
	if (buf->memory == V4L2_MEMORY_MMAP && !V4L2_TYPE_IS_OUTPUT(vq->type)) {
		if (V4L2_TYPE_IS_MULTIPLANAR(vq->type)) {
			for (i = 0; i < buf->length; ++i)
				buf->m.planes[i].m.mem_offset
					+= STM_VIDENC_DST_QUEUE_OFF_BASE;
		} else {
			buf->m.offset += STM_VIDENC_DST_QUEUE_OFF_BASE;
		}
	}

error:
	mutex_unlock(&dev_p->lock);
	return ret;
}

int stm_v4l2_encoder_vid_qbuf(struct file *file, void *fh, struct v4l2_buffer *buf)
{
	struct vb2_queue *vq;
	int ret=0;
	struct stm_v4l2_encoder_device *dev_p = file_to_stm_v4l2_encoder_dev(file);

	mutex_lock(&dev_p->lock);

	switch (buf->type) {
	case V4L2_BUF_TYPE_VIDEO_OUTPUT:
	case V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE:
		vq = &dev_p->src_vq;
		dev_p->enc_dev.video.src_v4l2_buf = *buf;
		break;
	case V4L2_BUF_TYPE_VIDEO_CAPTURE:
	case V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE:
		vq = &dev_p->dst_vq;
		dev_p->enc_dev.video.dst_v4l2_buf = *buf;
		break;
	default:
		ret = -EINVAL;
		goto error;
	}

	ret = vb2_qbuf(vq, buf);

error:
	mutex_unlock(&dev_p->lock);
	return ret;
}

int stm_v4l2_encoder_vid_dqbuf(struct file *file, void *fh, struct v4l2_buffer *buf)
{
	struct vb2_queue *vq;
	int ret=0;
	struct stm_v4l2_encoder_device *dev_p = file_to_stm_v4l2_encoder_dev(file);

	mutex_lock(&dev_p->lock);

	switch (buf->type) {
	case V4L2_BUF_TYPE_VIDEO_OUTPUT:
	case V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE:
		vq = &dev_p->src_vq;
		break;
	case V4L2_BUF_TYPE_VIDEO_CAPTURE:
	case V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE:
		vq = &dev_p->dst_vq;
		dev_p->enc_dev.video.file_flag = file->f_flags;
		break;
	default:
		ret = -EINVAL;
		goto error;
	}

	ret = vb2_dqbuf(vq, buf, file->f_flags & O_NONBLOCK);

error:
	mutex_unlock(&dev_p->lock);
	return ret;
}

int stm_v4l2_encoder_vid_streamon(struct file *file, void *fh,
				  enum v4l2_buf_type type)
{
	char memsink_name[32];
	int index, id = 0;
	int ret;
	struct media_pad *src_pad;
	struct stm_v4l2_encoder_device *dev_p = file_to_stm_v4l2_encoder_dev(file);

	src_pad = &dev_p->encoder_pad[STM_ENCODE_SINK_PAD];

	switch (type) {
	case V4L2_BUF_TYPE_VIDEO_OUTPUT:
	case V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE:

		mutex_lock(&dev_p->lock);

		if (stm_media_find_remote_pad_with_type(src_pad,
					MEDIA_LNK_FL_ENABLED,
					MEDIA_ENT_T_DVB_SUBDEV_VIDEO_DECODER,
					&id)) {
			mutex_unlock(&dev_p->lock);
			ret = -EBUSY;
			break;
		}

		mutex_unlock(&dev_p->lock);

		/* wait for the signal from capture streamon */
		down(&dev_p->sema);

		mutex_lock(&dev_p->lock);
		ret = vb2_streamon(&dev_p->src_vq, type);
		if(ret==0)
			dev_p->src_connect_type = STM_V4L2_ENCODE_CONNECT_INJECT;
		mutex_unlock(&dev_p->lock);
		break;

	case V4L2_BUF_TYPE_VIDEO_CAPTURE:
	case V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE:

		mutex_lock(&dev_p->lock);

		ret = vb2_streamon(&dev_p->dst_vq, type);
		if(ret) {
			mutex_unlock(&dev_p->lock);
			return ret;
		}

		index = dev_p->encoder_id;
		sprintf(memsink_name, "EncVidSink%02d", index);
		ret = stm_memsink_new(memsink_name,
				      STM_IOMODE_BLOCKING_IO,
				      KERNEL,
				      (stm_memsink_h *)&dev_p->encode_sink);
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
				printk(KERN_ERR "error : vid : stm_memsink_delete\n");
			dev_p->encode_sink = NULL;
			mutex_unlock(&dev_p->lock);
			return -EIO;
		}

		ret = stm_se_encode_stream_attach(dev_p->encode_stream, dev_p->encode_sink);
		if (ret) {
			printk(KERN_ERR "error : vid: stm_se_encode_stream_attach\n");
			stm_encoder_event_remove(dev_p);
			ret = stm_memsink_delete((stm_memsink_h)dev_p->encode_sink);
			if(ret)
				printk(KERN_ERR "error : vid : stm_memsink_delete\n");
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
				MEDIA_ENT_T_DVB_SUBDEV_VIDEO_DECODER);
		if(ret && (ret != -EALREADY)) {
			printk(KERN_ERR "can't attach the encoder\n");
			stm_encoder_event_remove(dev_p);
			ret = stm_memsink_delete((stm_memsink_h)dev_p->encode_sink);
			dev_p->encode_sink = NULL;
			return -EIO;
		}

		/* signal to ouput streamon */
		up(&dev_p->sema);

		break;

	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

static int encoder_vid_stream_close(struct stm_v4l2_encoder_device *dev_p)
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
				MEDIA_ENT_T_DVB_SUBDEV_VIDEO_DECODER);
	if (ret)
		printk(KERN_ERR "Failed to detach encoder from decoder\n");

	mutex_lock(&dev_p->lock);

	if(dev_p->src_connect_type == STM_V4L2_ENCODE_CONNECT_INJECT)
		dev_p->src_connect_type = STM_V4L2_ENCODE_CONNECT_NONE;

	if(dev_p->encode_stream && dev_p->encode_sink) {
		ret = stm_se_encode_stream_detach(dev_p->encode_stream, dev_p->encode_sink);
		if (ret) {
			printk(KERN_ERR "vid_streamoff: stream_detach error=%d\n", ret);
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
			printk(KERN_ERR "vid_streamoff: memsink_delete error=%d\n", ret);
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

int stm_v4l2_encoder_vid_streamoff(struct file *file, void *fh,
				   enum v4l2_buf_type type)
{
	int ret=0;
	int err=0;
	struct stm_v4l2_encoder_device *dev_p = file_to_stm_v4l2_encoder_dev(file);

	mutex_lock(&dev_p->lock);

	switch (type) {
	case V4L2_BUF_TYPE_VIDEO_OUTPUT:
	case V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE:
		if(dev_p->src_connect_type == STM_V4L2_ENCODE_CONNECT_INJECT)
			dev_p->src_connect_type = STM_V4L2_ENCODE_CONNECT_NONE;
		ret = vb2_streamoff(&dev_p->src_vq, type);
		break;
	case V4L2_BUF_TYPE_VIDEO_CAPTURE:
	case V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE:
		ret = vb2_streamoff(&dev_p->dst_vq, type);
		if (ret)
			err++;

		ret = encoder_vid_stream_close(dev_p);
		if (ret)
			err++;
		if (err)
			ret = -EIO;

		break;

	default:
		ret = -EINVAL;
		break;
	}

	mutex_unlock(&dev_p->lock);
	return ret;
}

/* --------------------------------------------------------------- */
/*  File Operations */

/* VSOC - HCE - for bpa2 buffer access from user space, we must have the      */
/* access file ops. This should also be done on SoC for access thru debugger! */
static int vid_enc_vm_access(struct vm_area_struct *vma, unsigned long addr,
			void *buf, int len, int write)
{
	void *page_addr;
	unsigned long page_frame;

	/*
	* Validate access limits
	*/
	if (addr >= vma->vm_end)
		return 0;
	if (len > vma->vm_end - addr)
		len = vma->vm_end - addr;
	if (len == 0)
		return 0;

	/*
	 * Note that this assumes an identity mapping between the page offset and
	 * the pfn of the physical address to be mapped. This will get more complex
	 * when the 32bit SH4 address space becomes available.
	*/
	page_addr = (void*)((addr - vma->vm_start) + (vma->vm_pgoff << PAGE_SHIFT));

	page_frame = ((unsigned long) page_addr >> PAGE_SHIFT);

	if (pfn_valid (page_frame)) {
		if (write)
			memcpy(__va(page_addr), buf, len);
		else
			memcpy(buf, __va(page_addr), len);
		return len;
	}

#if defined(CONFIG_ARM) || defined(CONFIG_HCE_ARCH_ARM)
	{
		void __iomem *ioaddr = __arm_ioremap((unsigned long)page_addr, len, MT_MEMORY);
		if (write)
			memcpy_toio(ioaddr, buf, len);
		else
			memcpy_fromio(buf, ioaddr, len);
		iounmap(ioaddr);
	}
	return len;
/*#elif defined(CONFIG_HAVE_IOREMAP_PROT) FIXME for stlinuxtv unknown symbol while module loading in case of hdk7108
    return generic_access_phys(vma, addr, buf, len, write);*/
#else
	return 0;
#endif
}

static struct vm_operations_struct vid_vm_ops;

int stm_v4l2_encoder_vid_mmap(struct file *file, struct vm_area_struct *vma)
{
	unsigned long offset = vma->vm_pgoff << PAGE_SHIFT;
	struct vb2_queue *vq;
	int ret;
	struct stm_v4l2_encoder_device *dev_p = file_to_stm_v4l2_encoder_dev(file);

	mutex_lock(&dev_p->lock);

	if (offset < STM_VIDENC_DST_QUEUE_OFF_BASE) {
		vq = &dev_p->src_vq;
	} else {
		vq = &dev_p->dst_vq;
		vma->vm_pgoff -= (STM_VIDENC_DST_QUEUE_OFF_BASE >> PAGE_SHIFT);
	}

	ret = vb2_mmap(vq, vma);

	memcpy(&vid_vm_ops, vma->vm_ops, sizeof(struct vm_operations_struct));

	vid_vm_ops.access = vid_enc_vm_access;

	vma->vm_ops = &vid_vm_ops;

	mutex_unlock(&dev_p->lock);

	return ret;
}

unsigned int stm_v4l2_vid_encoder_poll(struct file *file,
				       struct poll_table_struct *wait)
{
	struct stm_v4l2_encoder_device *dev_p = file_to_stm_v4l2_encoder_dev(file);

	/* We only do poll on the destination buffer */
	if(dev_p->dst_vq_set)
		return vb2_poll(&dev_p->dst_vq, file, wait);
	else
		return 0;
}

int stm_v4l2_encoder_vid_close(struct file *file)
{
	int ret=0;
	int err=0;
	struct stm_v4l2_encoder_device *dev_p = file_to_stm_v4l2_encoder_dev(file);

	ret = encoder_vid_stream_close(dev_p);
	if (ret)
		err++;

	if (dev_p->encode_stream) {
		ret = stm_se_encode_stream_delete(dev_p->encode_stream);
		if(ret) {
			printk(KERN_ERR "vid_close : stream_delete error=%d\n", ret);
			err++;
		}
		dev_p->encode_stream = NULL;
	}

	if (err)
		ret = -EIO;

	return ret;
}

int stm_v4l2_encoder_vid_create_connection(void)
{
	struct media_entity *src;
	struct media_entity *sink;
	int ret;

	src  = stm_media_find_entity_with_type_first(MEDIA_ENT_T_DVB_SUBDEV_VIDEO_DECODER);
	while(src) {

		sink = stm_media_find_entity_with_type_first(MEDIA_ENT_T_V4L2_SUBDEV_VIDEO_ENCODER);
		while(sink) {
			ret = media_entity_create_link(src, 0, sink, 0, 0);
			if (ret < 0) {
				printk(KERN_ERR "failed video connection \n");
				goto vid_create_err;
			}
			sink = stm_media_find_entity_with_type_next(sink, MEDIA_ENT_T_V4L2_SUBDEV_VIDEO_ENCODER);
		}

		src = stm_media_find_entity_with_type_next(src, MEDIA_ENT_T_DVB_SUBDEV_VIDEO_DECODER);
	}

	return 0;
vid_create_err:
	return ret;
}

int stm_v4l2_encode_convert_resolution_to_profile(int width, int height)
{
	if((width<=STM_V4L2_ENCODE_CIF_HEIGHT)&&(height<=STM_V4L2_ENCODE_CIF_WIDTH))
		return STM_SE_CTRL_VALUE_ENCODE_CIF_PROFILE;
	if((width<=STM_V4L2_ENCODE_SD_HEIGHT)&&(height<=STM_V4L2_ENCODE_SD_WIDTH))
		return STM_SE_CTRL_VALUE_ENCODE_SD_PROFILE;
	if((width<=STM_V4L2_ENCODE_720P_HEIGHT)&&(height<=STM_V4L2_ENCODE_720P_WIDTH))
		return STM_SE_CTRL_VALUE_ENCODE_720p_PROFILE;
	if((width<=STM_V4L2_ENCODE_HD_HEIGHT)&&(height<=STM_V4L2_ENCODE_HD_WIDTH))
		return STM_SE_CTRL_VALUE_ENCODE_HD_PROFILE;

	/* if does not fit in previous profiles, this is an error! */
	printk(KERN_ERR "resolution requested does not match in known profile!!! \n");
	return -1;
}
