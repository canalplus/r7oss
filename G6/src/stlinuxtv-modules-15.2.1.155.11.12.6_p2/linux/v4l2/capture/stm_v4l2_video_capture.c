/************************************************************************
Copyright (C) 2012 STMicroelectronics. All Rights Reserved.

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
 * Implementation of V4L2 output control video device
 * First initial driver by Akram Ben Belgacem
 * Updated for V4L2 vb2/video_ioctl2 and cleanup by Alain Volmat
************************************************************************/

#include <linux/module.h>
#include <linux/slab.h>

#include <media/v4l2-ioctl.h>
#include <media/videobuf2-core.h>
#include <media/videobuf2-vmalloc.h>
#include <asm/tlb.h>

#include "linux/stmvout.h"

#include <stm_display.h>

#include <stm_pixel_capture.h>

#include "linux/dvb/dvb_v4l2_export.h"
#include "linux/stm/stmedia_export.h"

#include "stmedia.h"
#include "stm_v4l2_common.h"
#include "stm_v4l2_video_capture.h"

struct capture_context_list {
	char name[30];
	unsigned int is_main;
} capture_context_list[] = {	{"stm_input_video1", 1},
				{"stm_input_video2", 0},
				{"stm_input_mix2", 0},
				{"stm_input_mix1", 1} };
struct compo_buffer {
	struct vb2_buffer vb;
	struct list_head list;
	stm_pixel_capture_buffer_descr_t descr;
};

#define	FORMATTED		(1<<1)

struct output_fh {
	struct v4l2_fh fh;
	struct capture_context *cap_ctx;
};

struct stm_v4l2_compo_device {
	struct video_device viddev;
	struct media_pad *pads;
};
static struct stm_v4l2_compo_device stm_v4l2_compo_dev;

static struct capture_context *g_capture_contexts;

struct format_info {
	struct v4l2_fmtdesc fmt;
	int depth;
	stm_pixel_capture_format_t cap_format;
};

static const struct format_info format_mapping[STM_PIXEL_FORMAT_COUNT] = {
	/*
	 * This isn't strictly correct as the V4L RGB565 format has red
	 * starting at the least significant bit.
	 * Unfortunately there is no V4L2 16bit format with blue starting
	 * at the LSB. It is recognised in the V4L2 API documentation that
	 * this is strange and that drivers may well lie for pragmatic reasons.
	 */
	{{0, V4L2_BUF_TYPE_VIDEO_CAPTURE, 0, "RGB-16 (5-6-5)",
	  V4L2_PIX_FMT_RGB565},
	 16,
	 STM_PIXEL_FORMAT_RGB565},

	{{0, V4L2_BUF_TYPE_VIDEO_CAPTURE, 0, "RGBA-16 (5-5-5-1)",
	  V4L2_PIX_FMT_BGRA5551},
	 16,
	 STM_PIXEL_FORMAT_ARGB1555},

	{{0, V4L2_BUF_TYPE_VIDEO_CAPTURE, 0, "RGBA-16 (4-4-4-4)",
	  V4L2_PIX_FMT_BGRA4444},
	 16,
	 STM_PIXEL_FORMAT_ARGB4444},

	{{0, V4L2_BUF_TYPE_VIDEO_CAPTURE, 0, "RGB-24 (B-G-R)",
	  V4L2_PIX_FMT_BGR24},
	 24,
	 STM_PIXEL_FORMAT_RGB888},

	{{0, V4L2_BUF_TYPE_VIDEO_CAPTURE, 0, "ARGB-32 (8-8-8-8)",
	  V4L2_PIX_FMT_BGR32},
	 32,
	 STM_PIXEL_FORMAT_ARGB8888},

	{{0, V4L2_BUF_TYPE_VIDEO_CAPTURE, 0, "BGRA-32 (8-8-8-8)",
	  V4L2_PIX_FMT_RGB32},
	 32,
	 STM_PIXEL_FORMAT_ARGB8888},

	{{0, V4L2_BUF_TYPE_VIDEO_CAPTURE, 0, "YUV 4:2:2MB",
	  V4L2_PIX_FMT_STM422MB},
	 8,			/* bits per pixel for luma only */
	 STM_PIXEL_FORMAT_YUV_NV16},

	{{0, V4L2_BUF_TYPE_VIDEO_CAPTURE, 0, "YUV 4:2:0MB",
	  V4L2_PIX_FMT_STM420MB},
	 8,			/* bits per pixel for luma only */
	 STM_PIXEL_FORMAT_YUV_NV12},

	{{0, V4L2_BUF_TYPE_VIDEO_CAPTURE, 0, "YUV 4:2:2 (YUV)",
	  V4L2_PIX_FMT_YUV422P},
	 8,			/* bits per pixel for luma only */
	 STM_PIXEL_FORMAT_YUV},

	{{0, V4L2_BUF_TYPE_VIDEO_CAPTURE, 0, "YUV 4:2:2 (U-Y-V-Y)",
	  V4L2_PIX_FMT_UYVY},
	 16,			/* bits per pixel for luma only */
	 STM_PIXEL_FORMAT_YCbCr422R},
};

#ifdef PIXEL_CAPTURE_EVENT_SUPPORT
static void compo_events(unsigned int events_nb, stm_event_info_t *ev)
{
	struct capture_context *cap_ctx;
	struct compo_buffer *buf, *dequeue_buf = NULL;
	unsigned long flags = 0;
	int i, ret;

	cap_ctx = ev->cookie;

	for (i = 0; i < events_nb; i++) {
		if (ev->event.event_id != STM_PIXEL_CAPTURE_EVENT_NEW_BUFFER)
			continue;

		spin_lock_irqsave(&cap_ctx->slock, flags);
		list_for_each_entry(buf, &cap_ctx->active, list) {
			if (ev->event.object == &buf->descr) {
				dequeue_buf = buf;
				break;
			}
		}
		list_del(&buf->list);
		spin_unlock_irqrestore(&cap_ctx->slock, flags);
	}

	if (dequeue_buf) {
		ret = stm_pixel_capture_dequeue_buffer(cap_ctx->pixel_capture,
							&dequeue_buf->descr);
		if (ret) {
			printk(KERN_ERR "%s: failed to dequeue buffer (%d)\n",
				__func__, ret);
			vb2_buffer_done(&dequeue_buf->vb, VB2_BUF_STATE_ERROR);
		}
		vb2_buffer_done(&dequeue_buf->vb, VB2_BUF_STATE_DONE);
	}
}

static int compo_subscribe_events(struct capture_context *cap_ctx)
{
	int ret;
	stm_event_subscription_entry_t event_entry = {
		.object = (stm_object_h) cap_ctx->pixel_capture,
		.event_mask = STM_PIXEL_CAPTURE_EVENT_NEW_BUFFER,
		.cookie = cap_ctx
	};

	if (cap_ctx->events)
		return 0;

	ret = stm_event_subscription_create(&event_entry, 1, &cap_ctx->events);
	if (ret) {
		printk(KERN_ERR "%s: failed event subscription (%d)\n",
			__func__, ret);
		goto end;
	}

	ret = stm_event_set_handler(cap_ctx->events, &compo_events);
	if (ret) {
		printk(KERN_ERR "%s: failed event handler setting (%d)\n",
			__func__, ret);
		goto failed_set_handler;
	}

	return 0;

failed_set_handler:
	if (stm_event_subscription_delete(cap_ctx->events))
		printk(KERN_ERR "%s: unsubscribe events failed\n", __func__);
end:
	return ret;
}

static void compo_unsubscribe_events(struct capture_context *cap_ctx)
{
	int ret;

	if (!cap_ctx->events)
		return;

	ret = stm_event_subscription_delete(cap_ctx->events);
	if (ret)
		printk(KERN_ERR "%s: unsubscribe events failed (%d)\n",
				__func__, ret);

	cap_ctx->events = NULL;
}
#endif

#define OUTPUT_MAIN	0
#define OUTPUT_AUX	1
static stm_display_output_h video_get_output(stm_display_device_h dev,
						    unsigned int output)
{
	stm_display_output_h hOutput = 0;
	unsigned int outputID[10], num_ids;
	int i;

	stm_display_output_capabilities_t outputCapsValue, outputCapsMask;

	if (output == OUTPUT_MAIN) {
		outputCapsValue = (OUTPUT_CAPS_SD_ANALOG |
				   OUTPUT_CAPS_ED_ANALOG |
				   OUTPUT_CAPS_HD_ANALOG |
				   OUTPUT_CAPS_EXTERNAL_SYNC_SIGNALS |
				   OUTPUT_CAPS_CVBS_YC_EXCLUSIVE |
				   OUTPUT_CAPS_YPbPr_EXCLUSIVE |
				   OUTPUT_CAPS_RGB_EXCLUSIVE |
				   OUTPUT_CAPS_SD_RGB_CVBS_YC |
				   OUTPUT_CAPS_SD_YPbPr_CVBS_YC);
	} else {
		outputCapsValue = (OUTPUT_CAPS_SD_ANALOG |
				   OUTPUT_CAPS_EXTERNAL_SYNC_SIGNALS |
				   OUTPUT_CAPS_SD_RGB_CVBS_YC |
				   OUTPUT_CAPS_SD_YPbPr_CVBS_YC);
	}

	/* Consider only Master outputs no slaves */
	outputCapsValue |= OUTPUT_CAPS_DISPLAY_TIMING_MASTER;

	outputCapsMask = outputCapsValue;

	num_ids =
	     stm_display_device_find_outputs_with_capabilities(dev,
							       outputCapsValue,
							       outputCapsMask,
							       outputID,
							       10);
	if (num_ids <= 0) {
		printk(KERN_ERR "Failed to find a suitable output!\n");
		return 0;
	}

	for (i = 0; i < num_ids; i++) {
		unsigned int caps;
		if (stm_display_device_open_output(dev, outputID[i], &hOutput)
		    != 0) {
			printk(KERN_ERR "Failed to get an output handle!\n");
			return 0;
		}
		stm_display_output_get_capabilities(hOutput, &caps);
		if (output == OUTPUT_MAIN) {
			if ((caps &
			     (OUTPUT_CAPS_HD_ANALOG | OUTPUT_CAPS_ED_ANALOG)) !=
			    0)
				break;
		} else {
			if ((caps &
			     (OUTPUT_CAPS_HD_ANALOG | OUTPUT_CAPS_ED_ANALOG)) ==
			    0)
				break;
		}
		stm_display_output_close(hOutput);
		hOutput = NULL;
	}

	return hOutput;
}

static int video_get_capture_input_params(stm_pixel_capture_h pixel_capture,
					  stm_pixel_capture_input_params_t *
					  params)
{
	stm_display_device_h hDev = 0;
	stm_display_output_h hOutput = 0;
	int device = 0;
	stm_display_mode_t mode_line = { STM_TIMING_MODE_RESERVED };
	unsigned int input;
	int ret;
	const char *name;
	unsigned int i;

	ret = stm_pixel_capture_get_input(pixel_capture, &input);
	if (ret) {
		printk(KERN_ERR "%s: failed to get input (%d)\n",
				__func__, ret);
		return -ENODEV;
	}

	ret = stm_pixel_capture_enum_inputs(pixel_capture, input, &name);
	if (ret) {
		printk(KERN_ERR "%s: failed to enum input (%d)\n",
				__func__, ret);
		return -ENODEV;
	}

	for (i = 0; i < ARRAY_SIZE(capture_context_list); i++) {
		if (!strcmp(name, capture_context_list[i].name))
			break;
	}

	if (i >= ARRAY_SIZE(capture_context_list)) {
		printk(KERN_ERR "%s: input name not found\n", __func__);
		ret = -EIO;
		return -ENODEV;
	}

	if (stm_display_open_device(device, &hDev) != 0) {
		printk(KERN_ERR "Display device not found\n");
		return -ENODEV;
	}

	if (capture_context_list[i].is_main)
		hOutput = video_get_output(hDev, OUTPUT_MAIN);
	else
		hOutput = video_get_output(hDev, OUTPUT_AUX);

	if (!hOutput) {
		printk(KERN_ERR "Unable to get output\n");
		return -ENODEV;
	}

	if (stm_display_output_get_current_display_mode(hOutput, &mode_line)
								 < 0) {
		printk(KERN_ERR "Unable to use requested display mode\n");
		return -EINVAL;
	}

	/* Setup capture input parameters according to current input mode */
	params->active_window.x =
	    mode_line.mode_params.active_area_start_pixel;
	params->active_window.y =
	    mode_line.mode_params.active_area_start_line;
	params->active_window.width =
	    mode_line.mode_params.active_area_width;
	params->active_window.height =
	    mode_line.mode_params.active_area_height;
	if (mode_line.mode_params.pixel_aspect_ratios[STM_AR_INDEX_16_9].
	    denominator == 0) {
		params->pixel_aspect_ratio.numerator = 16;
		params->pixel_aspect_ratio.denominator = 9;
	} else {
		params->pixel_aspect_ratio.numerator =
		    mode_line.mode_params.pixel_aspect_ratios[STM_AR_INDEX_16_9].
		    numerator;
		params->pixel_aspect_ratio.denominator =
		    mode_line.mode_params.pixel_aspect_ratios[STM_AR_INDEX_16_9].
		    denominator;
	}
	params->src_frame_rate =
	    mode_line.mode_params.vertical_refresh_rate;
	params->vtotal = mode_line.mode_timing.vsync_width;
	params->htotal = mode_line.mode_timing.hsync_width;
	if (mode_line.mode_params.scan_type == STM_PROGRESSIVE_SCAN)
		params->flags = 0;
	else
		params->flags = STM_PIXEL_CAPTURE_BUFFER_INTERLACED;

	stm_display_output_close(hOutput);
	stm_display_device_close(hDev);

	return 0;
}

/* VideoBuf2 handlers */
static int compo_queue_setup(struct vb2_queue *vq,
			     const struct v4l2_format *f,
			     unsigned int *nbuffers,
			     unsigned int *nplanes,
			     unsigned int sizes[], void *alloc_ctxs[])
{
	struct capture_context *cap_ctx = vb2_get_drv_priv(vq);

	*nplanes = 1;
	*nbuffers = 1;
	sizes[0] = cap_ctx->buffer_size;

	/*
	 * videobuf2-vmalloc allocator is context-less so no need to set
	 * alloc_ctxs array.
	 */

	return 0;
}

static int compo_buf_prepare(struct vb2_buffer *vb)
{
	vb2_set_plane_payload(vb, 0, vb->v4l2_buf.length);

	return 0;
}

/*
 * TODO: This function should be generic to our drivers
 */
static unsigned long get_physical_contiguous(unsigned long ptr, size_t size)
{
	struct mm_struct *mm = current->mm;
	unsigned virt_base = (ptr / PAGE_SIZE) * PAGE_SIZE;
	unsigned phys_base = 0;

	pgd_t *pgd;
	pmd_t *pmd;
	pte_t *ptep, pte;

	spin_lock(&mm->page_table_lock);

	pgd = pgd_offset(mm, virt_base);
	if (pgd_none(*pgd) || pgd_bad(*pgd))
		goto out;

	pmd = pmd_offset((pud_t *) pgd, virt_base);
	if (pmd_none(*pmd) || pmd_bad(*pmd))
		goto out;

	ptep = pte_offset_map(pmd, virt_base);

	pte = *ptep;

	if (pte_present(pte))
		phys_base = __pa(page_address(pte_page(pte)));

	if (!phys_base)
		goto out;

	spin_unlock(&mm->page_table_lock);
	return phys_base + (ptr - virt_base);

out:
	spin_unlock(&mm->page_table_lock);
	return 0;
}

static int fill_in_buffer_descr(stm_pixel_capture_buffer_format_t format,
				unsigned long chroma_offset,
				struct v4l2_buffer *p, struct v4l2_plane *plane,
				stm_pixel_capture_buffer_descr_t *descr)
{
	unsigned long long temporary_time;
	unsigned long phys = 0;

	/* build a pixel capture buffer from v4l2 buffer */
	memset(descr, 0, sizeof(*descr));

	phys = get_physical_contiguous(plane->m.userptr, plane->length);
	if (unlikely(!phys)) {
		printk(KERN_ERR "User space pointer is a bit wonky\n");
		return -EINVAL;
	}

	descr->cap_format = format;

	descr->length = p->length;
	descr->bytesused = p->length;

	/* update to the new captured buffer */
	switch (format.format) {
	case STM_PIXEL_FORMAT_RGB565:
	case STM_PIXEL_FORMAT_RGB888:
	case STM_PIXEL_FORMAT_ARGB1555:
	case STM_PIXEL_FORMAT_ARGB4444:
	case STM_PIXEL_FORMAT_ARGB8565:
	case STM_PIXEL_FORMAT_ARGB8888:
		descr->rgb_address = phys;
		break;
	case STM_PIXEL_FORMAT_YUV_NV12:
	case STM_PIXEL_FORMAT_YUV_NV16:
	case STM_PIXEL_FORMAT_YUV:
		descr->luma_address = phys;
		descr->chroma_offset = chroma_offset;
		break;
	default:
		descr->rgb_address = phys;
		break;
	}

	/* update to the new captured buffer */
	if (p->flags & V4L2_BUF_FLAG_FULLRANGE) {
		switch (descr->cap_format.color_space) {
		case STM_PIXEL_CAPTURE_BT601:
			descr->cap_format.color_space =
			    STM_PIXEL_CAPTURE_BT601_FULLRANGE;
			break;
		case STM_PIXEL_CAPTURE_BT709:
			descr->cap_format.color_space =
			    STM_PIXEL_CAPTURE_BT709_FULLRANGE;
			break;
		case STM_PIXEL_CAPTURE_RGB_VIDEORANGE:
			descr->cap_format.color_space = STM_PIXEL_CAPTURE_RGB;
			break;
		default:
			BUG();
			return -EINVAL;
		}
	}

	/* setup the new captured buffer scan type */
	switch (p->field) {
	case V4L2_FIELD_INTERLACED:
		descr->cap_format.flags = STM_PIXEL_CAPTURE_BUFFER_INTERLACED;
		break;
	case V4L2_FIELD_TOP:
		descr->cap_format.flags = STM_PIXEL_CAPTURE_BUFFER_TOP_ONLY;
		break;
	case V4L2_FIELD_BOTTOM:
		descr->cap_format.flags = STM_PIXEL_CAPTURE_BUFFER_BOTTOM_ONLY;
		break;
	case V4L2_FIELD_INTERLACED_TB:
		descr->cap_format.flags = STM_PIXEL_CAPTURE_BUFFER_TOP_BOTTOM;
		break;
	case V4L2_FIELD_INTERLACED_BT:
		descr->cap_format.flags = STM_PIXEL_CAPTURE_BUFFER_BOTTOM_TOP;
		break;
	case V4L2_FIELD_NONE:
	case V4L2_FIELD_ANY:
	default:
		descr->cap_format.flags = 0;
		break;
	}

	/* Convert sec/usec into PTS */
	temporary_time = p->timestamp.tv_usec * USEC_PER_SEC;
	temporary_time = temporary_time + p->timestamp.tv_sec;
	descr->captured_time = temporary_time;

	return 0;
}

static void compo_buf_queue(struct vb2_buffer *vb)
{
	struct vb2_queue *vq = vb->vb2_queue;
	struct compo_buffer *buf = container_of(vb, struct compo_buffer, vb);
	struct capture_context *cap_ctx = vb2_get_drv_priv(vq);
#ifdef PIXEL_CAPTURE_EVENT_SUPPORT
	unsigned long flags = 0;
#endif
	int ret;

	ret = fill_in_buffer_descr(cap_ctx->format,
				   cap_ctx->chroma_offset,
				   &vb->v4l2_buf,
				   &vb->v4l2_planes[0], &buf->descr);
	if (ret) {
		printk(KERN_ERR "%s: something went wrong ... (%d)\n",
		       __func__, ret);
		vb2_buffer_done(vb, VB2_BUF_STATE_ERROR);
		return;
	}

	ret = stm_pixel_capture_queue_buffer(cap_ctx->pixel_capture,
					       &buf->descr);
	if (ret) {
		printk(KERN_ERR "%s: failed to queue buffer (%d)\n",
				__func__, ret);
		vb2_buffer_done(vb, VB2_BUF_STATE_ERROR);
		return;
	}

#ifdef PIXEL_CAPTURE_EVENT_SUPPORT
	spin_lock_irqsave(&cap_ctx->slock, flags);
	list_add_tail(&buf->list, &cap_ctx->active);
	spin_unlock_irqrestore(&cap_ctx->slock, flags);
#else
	vb2_buffer_done(vb, VB2_BUF_STATE_DONE);
#endif
	return;
}

static int compo_buf_finish(struct vb2_buffer *vb)
{
	struct compo_buffer *buf = container_of(vb, struct compo_buffer, vb);
	unsigned long long temporary_time;
#ifndef PIXEL_CAPTURE_EVENT_SUPPORT
	struct vb2_queue *vq = vb->vb2_queue;
	struct capture_context *cap_ctx = vb2_get_drv_priv(vq);
	int ret;
#endif

#ifndef PIXEL_CAPTURE_EVENT_SUPPORT
	ret = stm_pixel_capture_dequeue_buffer(cap_ctx->pixel_capture,
							&buf->descr);
	if (ret) {
		printk(KERN_ERR "%s: failed to dequeue buffer (%d)\n",
				__func__, ret);
		return ret;
	}
#endif

	/* Convert PTS into sec/usec.
	 * do_div() modifies its first argument (it is a macro, not
	 * a function), therefore using a temporary is good practice.
	 */
	temporary_time = buf->descr.captured_time;
	vb->v4l2_buf.timestamp.tv_usec = do_div(temporary_time, USEC_PER_SEC);
	vb->v4l2_buf.timestamp.tv_sec = temporary_time;

	return 0;
}

static struct vb2_ops compo_vq_ops = {
	.queue_setup = compo_queue_setup,
	.buf_prepare = compo_buf_prepare,
	.buf_queue = compo_buf_queue,
	.buf_finish = compo_buf_finish,
};

static int vq_init(struct vb2_queue *vq, void *priv)
{
	memset(vq, 0, sizeof(*vq));
	vq->type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	vq->io_modes = VB2_USERPTR;
	vq->drv_priv = priv;
	vq->buf_struct_size = sizeof(struct compo_buffer);
	vq->ops = &compo_vq_ops;
	vq->mem_ops = &vb2_vmalloc_memops;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 9, 0))
	vq->timestamp_type = V4L2_BUF_FLAG_TIMESTAMP_MONOTONIC;
#endif

	return vb2_queue_init(vq);
}

static int vidioc_querycap(struct file *file, void *priv,
			   struct v4l2_capability *cap)
{
	strlcpy(cap->driver, "Compo", sizeof(cap->driver));
	strlcpy(cap->card, "STMicroelectronics", sizeof(cap->card));
	strlcpy(cap->bus_info, "platform: ST SoC", sizeof(cap->bus_info));

	cap->version = LINUX_VERSION_CODE;
	cap->device_caps = V4L2_CAP_VIDEO_CAPTURE |
				V4L2_CAP_STREAMING;
	cap->capabilities = cap->device_caps | V4L2_CAP_DEVICE_CAPS;

	return 0;
}

static int vidioc_enum_fmt_vid_cap(struct file *file, void *priv,
				   struct v4l2_fmtdesc *f)
{
	int index;

	if (f->index >= STM_PIXEL_FORMAT_COUNT)

		return -EINVAL;

	/* Copy the v4l2_fmtdesc structure content */
	index = f->index;
	memcpy(f, &(format_mapping[f->index].fmt),
	       sizeof(struct v4l2_fmtdesc));
	f->index = index;
	f->type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

	return 0;
}

static int vidioc_g_fmt_vid_cap(struct file *file, void *priv,
				struct v4l2_format *f)
{
	struct output_fh *fh = file->private_data;
	struct capture_context *cap_ctx = fh->cap_ctx;

	/* Fill in the v4l2_format structure */
	f->fmt.pix.width = cap_ctx->format.width;
	f->fmt.pix.height = cap_ctx->format.height;
	f->fmt.pix.pixelformat = format_mapping[cap_ctx->mapping_index]
							.fmt.pixelformat;
	f->fmt.pix.bytesperline = cap_ctx->format.stride;
	f->fmt.pix.sizeimage = f->fmt.pix.bytesperline * f->fmt.pix.height;
	f->fmt.pix.priv = cap_ctx->chroma_offset;

	switch (cap_ctx->format.color_space) {
	case STM_PIXEL_CAPTURE_BT601:
		f->fmt.pix.colorspace = V4L2_COLORSPACE_SMPTE170M;
		break;
	case STM_PIXEL_CAPTURE_BT709:
		f->fmt.pix.colorspace = V4L2_COLORSPACE_REC709;
		break;
	case STM_PIXEL_CAPTURE_RGB_VIDEORANGE:
		f->fmt.pix.colorspace = V4L2_COLORSPACE_SRGB;
		break;
	default:
		printk(KERN_WARNING
		       "%s: capture destination color space not setted properly\n",
		       __func__);
		return -EINVAL;
	}

	switch (cap_ctx->format.flags) {
	case STM_PIXEL_CAPTURE_BUFFER_INTERLACED:
		f->fmt.pix.field = V4L2_FIELD_INTERLACED;
		break;
	case STM_PIXEL_CAPTURE_BUFFER_TOP_ONLY:
		f->fmt.pix.field = V4L2_FIELD_TOP;
		break;
	case STM_PIXEL_CAPTURE_BUFFER_BOTTOM_ONLY:
		f->fmt.pix.field = V4L2_FIELD_BOTTOM;
		break;
	case STM_PIXEL_CAPTURE_BUFFER_TOP_BOTTOM:
		f->fmt.pix.field = V4L2_FIELD_SEQ_TB;
		break;
	case STM_PIXEL_CAPTURE_BUFFER_BOTTOM_TOP:
		f->fmt.pix.field = V4L2_FIELD_SEQ_BT;
		break;
	default:
		f->fmt.pix.field = V4L2_FIELD_NONE;
		break;
	}

	return 0;
}

static int vidioc_try_fmt_vid_cap(struct file *file, void *priv,
				  struct v4l2_format *f)
{
	int n, surface = -1;

	for (n = 0; n < ARRAY_SIZE(format_mapping); n++)
		if (format_mapping[n].fmt.pixelformat ==
		    f->fmt.pix.pixelformat)
			surface = n;

	if (surface == -1)
		return -EINVAL;

	return 0;
}

static int vidioc_s_fmt_vid_cap(struct file *file, void *priv,
				struct v4l2_format *f)
{
	struct output_fh *fh = file->private_data;
	struct capture_context *cap_ctx = fh->cap_ctx;
	const struct format_info *info;
	int n, mapping_index = 0, ret;
	stm_pixel_capture_input_params_t input_params;

	if (!cap_ctx)
		return -EIO;

	ret = vidioc_try_fmt_vid_cap(file, priv, f);
	if (ret < 0)
		return ret;

	if (vb2_is_streaming(&cap_ctx->vq))
		return -EBUSY;

	info = NULL;
	for (n = 0; n < ARRAY_SIZE(format_mapping); ++n)
		if (format_mapping[n].fmt.pixelformat ==
			f->fmt.pix.pixelformat) {
			mapping_index = n;
			info = &format_mapping[n];
			break;
		}

	if (!info)
		return -EIO;

	if (!f->fmt.pix.bytesperline)
		f->fmt.pix.bytesperline = (info->depth * f->fmt.pix.width / 8);

	cap_ctx->mapping_index = mapping_index;
	memset(&cap_ctx->format, 0, sizeof(stm_pixel_capture_buffer_format_t));
	cap_ctx->format.format = info->cap_format;

	switch (f->fmt.pix.colorspace) {
	case V4L2_COLORSPACE_SMPTE170M:
		cap_ctx->format.color_space = STM_PIXEL_CAPTURE_BT601;
		break;
	case V4L2_COLORSPACE_REC709:
		cap_ctx->format.color_space = STM_PIXEL_CAPTURE_BT709;
		break;
	case V4L2_COLORSPACE_SRGB:
	default:
		cap_ctx->format.color_space = STM_PIXEL_CAPTURE_RGB_VIDEORANGE;
		break;
	}

	cap_ctx->format.width = f->fmt.pix.width;
	cap_ctx->format.height = f->fmt.pix.height;
	cap_ctx->format.stride = f->fmt.pix.bytesperline;
	cap_ctx->chroma_offset = f->fmt.pix.priv;

	switch (f->fmt.pix.field) {
	case V4L2_FIELD_INTERLACED:
		cap_ctx->format.flags = STM_PIXEL_CAPTURE_BUFFER_INTERLACED;
		break;
	case V4L2_FIELD_TOP:
		cap_ctx->format.flags = STM_PIXEL_CAPTURE_BUFFER_TOP_ONLY;
		break;
	case V4L2_FIELD_BOTTOM:
		cap_ctx->format.flags = STM_PIXEL_CAPTURE_BUFFER_BOTTOM_ONLY;
		break;
	case V4L2_FIELD_SEQ_TB:
		cap_ctx->format.flags = STM_PIXEL_CAPTURE_BUFFER_TOP_BOTTOM;
		break;
	case V4L2_FIELD_SEQ_BT:
		cap_ctx->format.flags = STM_PIXEL_CAPTURE_BUFFER_BOTTOM_TOP;
		break;
	/* If Field ANY or NONE is set, we set in progressive */
	case V4L2_FIELD_ANY:
	case V4L2_FIELD_NONE:
	default:
		/* default scan type is PROGRESSIVE */
		cap_ctx->format.flags = 0;
		break;
	}

	/* Setup Capture Input Params */
	ret =
	    video_get_capture_input_params(cap_ctx->pixel_capture,
					   &input_params);
	if (ret)
		return -EIO;

	/* Setup input color space avoiding any hw color space convertion */
	input_params.color_space = cap_ctx->format.color_space;

	ret =
	    stm_pixel_capture_set_input_params(cap_ctx->pixel_capture,
					       input_params);

	if (ret) {
		printk(KERN_ERR "%s: failed to set cap_ctx params(%d)\n",
				__func__, ret);
		return -EIO;
	}

	cap_ctx->flags |= FORMATTED;

	return 0;
}

static int vidioc_reqbufs(struct file *file, void *priv,
			  struct v4l2_requestbuffers *p)
{
	struct output_fh *fh = file->private_data;
	struct capture_context *cap_ctx = fh->cap_ctx;

	return vb2_reqbufs(&cap_ctx->vq, p);
}

static int vidioc_querybuf(struct file *file, void *priv, struct v4l2_buffer *p)
{
	struct output_fh *fh = file->private_data;
	struct capture_context *cap_ctx = fh->cap_ctx;

	return vb2_querybuf(&cap_ctx->vq, p);
}

static int vidioc_qbuf(struct file *file, void *priv, struct v4l2_buffer *p)
{
	struct output_fh *fh = file->private_data;
	struct capture_context *cap_ctx = fh->cap_ctx;

	return vb2_qbuf(&cap_ctx->vq, p);
}

static int vidioc_dqbuf(struct file *file, void *priv, struct v4l2_buffer *p)
{
	struct output_fh *fh = file->private_data;
	struct capture_context *cap_ctx = fh->cap_ctx;

	return vb2_dqbuf(&cap_ctx->vq, p, file->f_flags & O_NONBLOCK);
}

static int vidioc_streamon(struct file *file, void *priv, enum v4l2_buf_type i)
{
	struct output_fh *fh = file->private_data;
	struct capture_context *cap_ctx = fh->cap_ctx;
	int ret;

	if (!(cap_ctx->flags & FORMATTED)) {
		printk(KERN_INFO "%s: need call first VIDIOC_S_FMT\n",
		       __func__);
		return -EINVAL;
	}

	/* Start Capture */
	ret = stm_pixel_capture_start(cap_ctx->pixel_capture);
	if (ret) {
		printk(KERN_ERR "%s: failed to start pixel capture (%d)\n",
				__func__, ret);
		goto end;
	}

	return vb2_streamon(&cap_ctx->vq, i);

end:
	return ret;
}

static int vidioc_streamoff(struct file *file, void *priv, enum v4l2_buf_type i)
{
	struct output_fh *fh = file->private_data;
	struct capture_context *cap_ctx = fh->cap_ctx;
	int ret;

	ret = vb2_streamoff(&cap_ctx->vq, i);
	if (ret) {
		printk(KERN_ERR "%s: streamoff failed (%d)\n",
				__func__, ret);
		goto end;
	}

	/* Stop Capture */
	ret = stm_pixel_capture_stop(cap_ctx->pixel_capture);
	if (ret) {
		printk(KERN_ERR "%s: failed to stop pixel capture (%d)\n",
				__func__, ret);
		goto end;
	}

	return 0;

end:
	return ret;
}

static int vidioc_enum_input(struct file *file, void *priv,
			     struct v4l2_input *inp)
{
	struct capture_context *cap_ctx = g_capture_contexts;
	int index = inp->index;
	const char *name;
	int ret;

	if (!cap_ctx->pixel_capture)
		return -ENODEV;

	memset(inp, 0, sizeof(struct v4l2_input));
	inp->index = index;
	inp->type = V4L2_INPUT_TYPE_CAMERA;

	ret =
	    stm_pixel_capture_enum_inputs(cap_ctx->pixel_capture,
						inp->index, &name);
	if (ret) {
		ret = -EINVAL;
		goto end;
	}

	strlcpy(inp->name, name, sizeof(inp->name));

end:
	return ret;
}

static int vidioc_g_input(struct file *file, void *priv, unsigned int *i)
{
	struct output_fh *fh = file->private_data;
	struct capture_context *cap_ctx = fh->cap_ctx;
	unsigned int param;
	int ret;

	ret = stm_pixel_capture_get_input(cap_ctx->pixel_capture, &param);
	if (ret) {
		printk(KERN_ERR "%s: failed to get input (%d)\n",
				__func__, ret);
		goto end;
	}

	*i = param;
end:
	return ret;
}

static int vidioc_s_input(struct file *file, void *priv, unsigned int i)
{
	struct output_fh *fh = file->private_data;
	struct capture_context *cap_ctx = g_capture_contexts;
	int ret;

	fh->cap_ctx = cap_ctx;
	if (cap_ctx->vq_set == 0) {
		ret = vq_init(&cap_ctx->vq, (void *)cap_ctx);
		if (ret) {
			printk(KERN_ERR "%s: failed to init queue (%d)\n",
			       __func__, ret);
			return ret;
		}
		cap_ctx->vq_set = 1;
	}

	ret = stm_pixel_capture_set_input(cap_ctx->pixel_capture, i);
	if (ret) {
		printk(KERN_ERR "%s: failed to set input (%d)\n",
				__func__, ret);
		goto end;
	}

	/* Lock Capture for our usage */
	ret = stm_pixel_capture_lock(cap_ctx->pixel_capture);
	if (ret) {
		printk(KERN_ERR "%s: failed to lock the pixel capture(%d)\n",
				__func__, ret);
		goto failed_lock;
	}

	cap_ctx->id = i;

	return 0;

failed_lock:
end:
	return ret;
}

static int STM_V4L2_FUNC(vidioc_s_crop,
		struct file *file, void *priv, struct v4l2_crop *c)
{
	struct output_fh *fh = file->private_data;
	struct capture_context *cap_ctx = fh->cap_ctx;
	stm_pixel_capture_rect_t input_window;
	int ret;

	switch (c->type) {
	case V4L2_BUF_TYPE_VIDEO_CAPTURE:
		input_window.x = c->c.left;
		input_window.y = c->c.top;
		input_window.width = c->c.width;
		input_window.height = c->c.height;

		ret =
		    stm_pixel_capture_set_input_window(cap_ctx->pixel_capture,
						       input_window);
		if (ret)
			printk(KERN_ERR "%s:failed to set the IO window(%d)\n",
				__func__, ret);
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

static int vidioc_g_crop(struct file *file, void *priv, struct v4l2_crop *c)
{
	struct output_fh *fh = file->private_data;
	struct capture_context *cap_ctx = fh->cap_ctx;
	stm_pixel_capture_rect_t input_window = { 0 };
	int ret = -EINVAL;

	switch (c->type) {
	case V4L2_BUF_TYPE_VIDEO_CAPTURE:
		ret =
		    stm_pixel_capture_get_input_window(cap_ctx->pixel_capture,
						       &input_window);
		if (ret) {
			printk(KERN_ERR "%s:failed to get the IO window(%d)\n",
				__func__, ret);
			goto end;
		}

		c->c.left = input_window.x;
		c->c.top = input_window.y;
		c->c.width = input_window.width;
		c->c.height = input_window.height;
		break;
	default:
		ret = -EINVAL;
		break;
	}

end:
	return ret;
}

static int get_default_window_rect(struct capture_context *cap_ctx, struct v4l2_rect *rect)
{
	stm_pixel_capture_input_window_capabilities_t input_window_capability;
	stm_pixel_capture_input_params_t input_params;
	int ret;

	memset(&input_params, 0, sizeof(stm_pixel_capture_input_params_t));

	ret =
	    stm_pixel_capture_get_input_window_capabilities(cap_ctx->
						pixel_capture,
						&input_window_capability);
	if (ret) {
		printk(KERN_ERR "%s: failed to get the IO window capabilities(%d)\n",
			__func__, ret);
		goto end;
	}

	ret = video_get_capture_input_params(cap_ctx->pixel_capture,
						   &input_params);
	if (ret)
		goto end;

	rect->left = rect->top = 0;
	rect->width = min(input_window_capability.max_input_window_area.width,
				input_params.active_window.width);
	rect->height = min(input_window_capability.max_input_window_area.height,
				input_params.active_window.height);

end:
	return ret;
}

static int vidioc_cropcap(struct file *file, void *priv, struct v4l2_cropcap *c)
{
	struct output_fh *fh = file->private_data;
	struct capture_context *cap_ctx = fh->cap_ctx;
	int ret = 0;

	switch (c->type) {
	case V4L2_BUF_TYPE_VIDEO_CAPTURE:
		ret = get_default_window_rect(cap_ctx, &c->bounds);
		if (ret)
			goto end;

		c->defrect = c->bounds;

		c->pixelaspect.numerator = 1;
		c->pixelaspect.denominator = 1;
		break;
	default:
		ret = -EINVAL;
		break;
	}

end:
	return ret;
}

static long output_dev_ioctl(struct file *file,
			     unsigned int cmd, unsigned long arg)
{
	int ret;
	struct output_fh *fh = file->private_data;
	struct capture_context *cap_ctx = fh->cap_ctx;

	if (cap_ctx)
		if (mutex_lock_interruptible(&cap_ctx->lock))
			return -EINTR;

	ret = video_ioctl2(file, cmd, arg);

	if (cap_ctx)
		mutex_unlock(&cap_ctx->lock);

	return ret;
}

static int output_dev_close(struct file *file)
{
	struct output_fh *fh = file->private_data;
	struct capture_context *cap_ctx = fh->cap_ctx;
	int ret;

	if (v4l2_fh_is_singular_file(file)) {
		if (cap_ctx->vq_set) {
			vb2_queue_release(&cap_ctx->vq);
			cap_ctx->vq_set = 0;
		}

		/* Unlock Capture before exiting */
		ret = stm_pixel_capture_unlock(cap_ctx->pixel_capture);
		if (ret)
			printk(KERN_ERR "%s: pixel capture unlock failed(%d)\n",
					__func__, ret);
	}

	return v4l2_fh_release(file);
}

static int output_dev_open(struct file *file)
{
	struct video_device *viddev = &stm_v4l2_compo_dev.viddev;
	struct output_fh *fh;
	struct v4l2_cropcap cropcap;
	struct v4l2_crop crop;
	int ret;

	/* Allocate memory */
	fh = kzalloc(sizeof(struct output_fh), GFP_KERNEL);
	if (!fh) {
		printk(KERN_ERR "%s: nomem on v4l2 open\n", __func__);
		return -ENOMEM;
	}
	v4l2_fh_init(&fh->fh, viddev);
	file->private_data = fh;
	fh->cap_ctx = NULL;
	v4l2_fh_add(&fh->fh);

	/* Default to the first input */
	ret = vidioc_s_input(file, NULL, 0);
	if (ret) {
		printk(KERN_ERR "%s: failed to select input 0 as default (%d)\n",
				__func__, ret);
		goto end;
	}

	/* Set the default input window */
	cropcap.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	ret = vidioc_cropcap(file, NULL, &cropcap);
	if (ret) {
		printk(KERN_ERR "%s: failed to get default crop (%d)\n",
				__func__, ret);
		goto end;
	}

	crop.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	crop.c = cropcap.defrect;
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3, 7, 0))
	ret = vidioc_s_crop(file, NULL, &crop);
#else
	ret = vidioc_s_crop(file, NULL, (const struct v4l2_crop *)&crop);
#endif
	if (ret) {
		printk(KERN_ERR "%s: failed to set crop (%d)\n",
				__func__, ret);
		goto end;
	}

end:
	return 0;
}

static struct v4l2_file_operations output_dev_fops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = output_dev_ioctl,
	.open = output_dev_open,
	.release = output_dev_close,
};

static void output_dev_vdev_release(struct video_device *vdev)
{
	/* Nothing to do, but need by V4L2 stack */
}

static const struct v4l2_ioctl_ops output_ioctl_ops = {
	.vidioc_querycap = vidioc_querycap,
	.vidioc_enum_fmt_vid_cap = vidioc_enum_fmt_vid_cap,
	.vidioc_g_fmt_vid_cap = vidioc_g_fmt_vid_cap,
	.vidioc_try_fmt_vid_cap = vidioc_try_fmt_vid_cap,
	.vidioc_s_fmt_vid_cap = vidioc_s_fmt_vid_cap,
	.vidioc_reqbufs = vidioc_reqbufs,
	.vidioc_querybuf = vidioc_querybuf,
	.vidioc_qbuf = vidioc_qbuf,
	.vidioc_dqbuf = vidioc_dqbuf,
	.vidioc_streamon = vidioc_streamon,
	.vidioc_streamoff = vidioc_streamoff,
	.vidioc_enum_input = vidioc_enum_input,
	.vidioc_g_input = vidioc_g_input,
	.vidioc_s_input = vidioc_s_input,
	.vidioc_s_crop = vidioc_s_crop,
	.vidioc_g_crop = vidioc_g_crop,
	.vidioc_cropcap = vidioc_cropcap,
};

/**
 * output_device_init() - initialize video capture device and subdevs(compo/dvp)
 */
static int __init output_device_init(void)
{
	struct video_device *viddev = &stm_v4l2_compo_dev.viddev;
	struct media_pad *pads;
	struct capture_context *cap_ctx;
	int ret;

	/* Initialize the capture_context table */
	cap_ctx = kzalloc((sizeof(struct capture_context)
				 * MAX_CAPTURE_DEVICES), GFP_KERNEL);
	if (!cap_ctx) {
		printk(KERN_ERR
		       "%s: failed to allocate memory for capture_input\n",
		       __func__);
		ret = -ENOMEM;
		goto failed_input_table_alloc;
	}
	g_capture_contexts = cap_ctx;

	/*
	 * Initialize compo subdev
	 */
	ret = compo_capture_subdev_init(&cap_ctx[COMPO_INDEX]);
	if (ret) {
		pr_err("%s(): compo subdev init failed\n", __func__);
		goto failed_compo_subdev_init;
	}

	/*
	 * Initialize dvp subdev
	 */
	ret = dvp_capture_subdev_init(&cap_ctx[DVP_INDEX]);
	if (ret) {
		pr_err("%s(): dvp subdev init failed\n", __func__);
		goto failed_dvp_subdev_init;
	}

#ifdef PIXEL_CAPTURE_EVENT_SUPPORT
	INIT_LIST_HEAD(&cap_ctx->active);
	spin_lock_init(&cap_ctx->slock);
	ret = compo_subscribe_events(cap_ctx);
	if (ret)
		goto failed_subscribe;
#endif

	/*
	 * Initialize the media entity for the video capture video device
	 */
	pads = (struct media_pad *)kzalloc(sizeof(*pads), GFP_KERNEL);
	if (!pads) {
		printk(KERN_ERR "Out of memory for video capture pads\n");
		ret = -ENOMEM;
		goto failed_pad_alloc;
	}
	stm_v4l2_compo_dev.pads = pads;

	ret = media_entity_init(&viddev->entity, 1, pads, 0);
	if (ret) {
		printk(KERN_ERR "Failed to init compo entity\n");
		goto failed_entity_init;
	}

	/*
	 * Initialize the video capture v4l2 video device
	 */
	strlcpy(viddev->name, "STM Video Capture", sizeof(viddev->name));
	viddev->fops = &output_dev_fops;
	viddev->ioctl_ops = &output_ioctl_ops;
	viddev->release = output_dev_vdev_release;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 7, 0))
        viddev->vfl_dir = VFL_DIR_RX;
#endif

	/* Register the output device */
	ret =
	    stm_media_register_v4l2_video_device(viddev, VFL_TYPE_GRABBER, -1);
	if (ret < 0) {
		printk(KERN_ERR
		       "%s: failed to register the overlay driver (%d)\n",
		       __func__, ret);
		ret = -EIO;
		goto failed_register_device;
	}

	return 0;

failed_register_device:
	media_entity_cleanup(&viddev->entity);
failed_entity_init:
	kfree(pads);
failed_pad_alloc:
#ifdef PIXEL_CAPTURE_EVENT_SUPPORT
	compo_unsubscribe_events(cap_ctx);
failed_subscribe:
#endif
	dvp_capture_subdev_exit(&cap_ctx[DVP_INDEX]);
failed_dvp_subdev_init:
	compo_capture_subdev_exit(&cap_ctx[COMPO_INDEX]);
failed_compo_subdev_init:
	kfree(g_capture_contexts);
failed_input_table_alloc:
	return ret;
}

/**
 * output_device_exit() - exits the video capture device
 */
static void __exit output_device_exit(void)
{
	stm_media_unregister_v4l2_video_device(&stm_v4l2_compo_dev.viddev);

	media_entity_cleanup(&stm_v4l2_compo_dev.viddev.entity);

	kfree(stm_v4l2_compo_dev.pads);

#ifdef PIXEL_CAPTURE_EVENT_SUPPORT
	compo_unsubscribe_events(g_capture_contexts);
#endif

	dvp_capture_subdev_exit(&g_capture_contexts[DVP_INDEX]);

	compo_capture_subdev_exit(&g_capture_contexts[COMPO_INDEX]);

	kfree(g_capture_contexts);
}

module_init(output_device_init);
module_exit(output_device_exit);
MODULE_DESCRIPTION("ST Microelectronics - V4L2 Video Capture device from Compositor/Planes");
MODULE_AUTHOR("ST Microelectronics");
MODULE_LICENSE("GPL");
