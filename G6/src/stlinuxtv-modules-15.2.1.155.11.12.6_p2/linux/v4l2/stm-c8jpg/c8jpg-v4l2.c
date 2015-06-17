/*
 * c8jpg.c, a driver for the C8JPG1 JPEG decoder IP
 *
 * Copyright (C) 2012-2013, STMicroelectronics R&D
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 */

#include <linux/platform_device.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/io.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/clk.h>
#include <linux/pm_runtime.h>

#ifdef CONFIG_VIDEO_STM_C8JPG_DEBUG
#include <linux/crc32c.h>
#endif

#include <media/videobuf2-core.h>
#include <media/videobuf2-bpa2-contig.h>

#include "c8jpg.h"

#include "c8jpg-hw.h"
#include "c8jpg-macros.h"

static struct stm_c8jpg_format c8jpg_fmts[] = {
	{
		.fourcc = V4L2_PIX_FMT_NV12,
		.nplanes = 1,
		.bpp = 12,
		.uvsampling = {1, 2},
		.type = V4L2_BUF_TYPE_VIDEO_CAPTURE,
		.description = "Contiguous two planes YCbCr 4:2:0"
	},
	{
		.fourcc = V4L2_PIX_FMT_NV16,
		.nplanes = 1,
		.bpp = 16,
		.uvsampling = {1, 1},
		.type = V4L2_BUF_TYPE_VIDEO_CAPTURE,
		.description = "Contiguous two planes YCbCr 4:2:2"
	},
	{
		.fourcc = V4L2_PIX_FMT_NV24,
		.nplanes = 1,
		.bpp = 24,
		.uvsampling = {2, 1},
		.type = V4L2_BUF_TYPE_VIDEO_CAPTURE,
		.description = "Contiguous two planes YCbCr 4:4:4"
	},
	{
		.fourcc = V4L2_PIX_FMT_NV12M,
		.nplanes = 2,
		.bpp = 12,
		.uvsampling = {1, 2},
		.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE,
		.description = "Non-contiguous two planes YCbCr 4:2:0"
	},
	{
		.fourcc = V4L2_PIX_FMT_NV16M,
		.nplanes = 2,
		.bpp = 16,
		.uvsampling = {1, 1},
		.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE,
		.description = "Non-contiguous two planes YCbCr 4:2:2"
	},
	{
		.fourcc = V4L2_PIX_FMT_NV24M,
		.nplanes = 2,
		.bpp = 24,
		.uvsampling = {2, 1},
		.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE,
		.description = "Non-contiguous two planes YCbCr 4:4:4"
	},
	{
		.fourcc = V4L2_PIX_FMT_JPEG,
		.nplanes = 1,
		.bpp = 0,
		.type = V4L2_BUF_TYPE_VIDEO_OUTPUT,
		.description = "JPEG"
	}
};
#define NUM_C8JPG_FORMATS ARRAY_SIZE(c8jpg_fmts)

static struct stm_c8jpg_format *find_fmt(enum v4l2_buf_type type,
					 __u32 fourcc)
{
	int i;

	for (i = 0; i < NUM_C8JPG_FORMATS; i++)
		if (c8jpg_fmts[i].type == type
		    && c8jpg_fmts[i].fourcc == fourcc)
			return &c8jpg_fmts[i];

	return NULL;
}

#define inttofix(a)	\
	( (unsigned int) ((unsigned int)(a) << 15) )

#define fixtoint(a)	\
	( (unsigned int) ((unsigned int)(a) >> 15) )

#define fixdiv(a, b)	\
	( (unsigned int) (((unsigned int)(a) << 15) / (unsigned int)(b)) )

static inline int read_u8(struct c8jpg_jpeg_buf *buf)
{
	if (buf->pos >= buf->size)
		return -1;
	return buf->data[buf->pos++];
}

static inline int read_u16be(struct c8jpg_jpeg_buf *buf)
{
	int b1, b2;
	b1 = read_u8(buf);
	if (b1 < 0)
		return -1;
	b2 = read_u8(buf);
	if (b2 < 0)
		return -1;
	return (b1 << 8) | b2;
}

static inline void skip_bytes(struct c8jpg_jpeg_buf *buf,
			      unsigned int len)
{
	if ((buf->pos + len) >= buf->size) {
		/* Next read_u8() will fail */
		buf->pos = buf->size;
		return;
	}
	buf->pos += len;
}

static inline int read_SOI(struct c8jpg_jpeg_buf *buf)
{
	unsigned short s = read_u16be(buf);
	if ((s & 0xFF) != SOI)
		return -1;
	return 0;
}

static int read_SOF0(struct c8jpg_jpeg_buf *buf,
		     struct stm_c8jpg_buf_fmt *fmt)
{
	int p, w, h, c, i, x;
	struct c8jpg_chroma_sampling chroma;

	if (read_u16be(buf) < 0)
		return -1;

	p = read_u8(buf);
	if (p < 0)
		return -1;

	/* 8-bit per sample precision */
	if (p != 8)
		return -1;

	h = read_u16be(buf);
	if (h < 0)
		return -1;
	w = read_u16be(buf);
	if (w < 0)
		return -1;
	c = read_u8(buf);
	if (c < 0)
		return -1;

	/* 3 channels YCbCr */
	if (c != 3)
		return -1;

	for (i = 0, x = 3, chroma.sampling = 0; i < c; i++, x--) {
		int s;
		if (read_u8(buf) < 0)
			return -1;
		s = read_u8(buf);
		if (s < 0)
			return -1;
		if (read_u8(buf) < 0)
			return -1;
		chroma.sampling |= (((s & 0xF) << 16) | (s >> 4)) << (x * 4);
	}

	/* 4:2:0, 4:2:2 and 4:4:4 chroma sampling */
	switch (chroma.sampling) {
	case C8JPG_CHROMA_SAMPLING_420:
		fmt->fmt = find_fmt(V4L2_BUF_TYPE_VIDEO_CAPTURE,
				    V4L2_PIX_FMT_NV12);
		break;
	case C8JPG_CHROMA_SAMPLING_422:
		fmt->fmt = find_fmt(V4L2_BUF_TYPE_VIDEO_CAPTURE,
				    V4L2_PIX_FMT_NV16);
		break;
	case C8JPG_CHROMA_SAMPLING_444:
		fmt->fmt = find_fmt(V4L2_BUF_TYPE_VIDEO_CAPTURE,
				    V4L2_PIX_FMT_NV24);
		break;
	default:
		return -1;
	}

	fmt->width = w;
	fmt->height = h;

	return 0;
}

static int c8jpg_parse_JPEG_header(struct c8jpg_jpeg_buf *buf,
				   struct stm_c8jpg_buf_fmt *fmt)
{
	int len, ret;
	int found = -1;

	fmt->width = -1;
	fmt->height = -1;
	fmt->fmt = NULL;

	if (read_SOI(buf) < 0)
		return -1;

	while (found < 0) {
		ret = read_u8(buf);
		if (ret < 0)
			break;
		if (ret != 0xFF)
			continue;
		do
			ret = read_u8(buf);
		while (ret == 0xFF);
		if (ret < 0)
			break;
		switch (ret) {
		case SOF0:
			ret = read_SOF0(buf, fmt);
			found = 1;
			break;
		case SOF1 ... SOF1 + 2:
		case SOF5 ... SOF5 + 2:
		case JPG ... JPG + 3:
		case SOF13 ... SOF13 + 2:
			found = 0;
			break;
		case EOI:
		case TEM:
		case RST ... RST + 7:
			break;
		case DHT:
		case DAC:
		case SOS:
		case DQT:
		case DNL:
		case DRI:
		case DHP:
		case EXP:
		case APP0 ... APP0 + 15:
		case COM:
			len = read_u16be(buf);
			if (len < 0)
				break;
			skip_bytes(buf, len - 2);
			break;
		default:
			break;
		}
	}

	return (found == 1 ? ret : -1);
}

/* Bug 23312: the BH parser doesn't correctly parse APPx markers */
static int c8jpg_skip_APPx_markers(struct c8jpg_jpeg_buf *buf)
{
	int len, ret;
	int found = -1;

	if (read_SOI(buf) < 0)
		return -1;

	while (found < 0) {
		ret = read_u8(buf);
		if (ret < 0)
			break;
		if (ret != 0xFF)
			continue;
		do
			ret = read_u8(buf);
		while (ret == 0xFF);
		if (ret < 0)
			break;
		switch (ret) {
		case SOF0:
		case DHT:
		case DQT:
			found = 1;
			break;
		case APP0 ... APP0 + 15:
		/* COM markers could follow APPx */
		case COM:
			len = read_u16be(buf);
			if (len < 0)
				break;
			skip_bytes(buf, len - 2);
			break;
		default:
			/* Anything else is just an unsupported JPEG case */
			found = 0;
			break;
		}
	}

	return (found == 1 ? buf->pos - 2 : -1);
}

static int c8jpg_vb2_queue_setup(struct vb2_queue *vq,
				 const struct v4l2_format *fmt,
				 unsigned int *nbuffers,
				 unsigned int *nplanes,
				 unsigned int sizes[],
				 void *alloc_ctxs[])
{
	struct stm_c8jpg_ctx *cctx = vb2_get_drv_priv(vq);
	unsigned int size = 1;

	/* We can't queue more than one JPEG buffer because
	 * cctx->srcfmt would then be overwritten.
	 */
	*nbuffers = 1;

	*nplanes = (vq->type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE) ? 2 : 1;

	if (vq->type == V4L2_BUF_TYPE_VIDEO_OUTPUT
	    && cctx->srcfmt.size) {
		/* Must be a V4L2_MEMORY_MMAP source buffer */
		size = cctx->srcfmt.size;
	}
	if (vq->type == V4L2_BUF_TYPE_VIDEO_CAPTURE
	    && cctx->dstfmt.size) {
		/* Must be a V4L2_MEMORY_MMAP destination buffer */
		size = cctx->dstfmt.size;
	}

	sizes[0] = size;
	if (vq->type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE)
		sizes[1] = size;

	alloc_ctxs[0] = cctx->dev->vb2_alloc_ctx;
	if (vq->type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE)
		alloc_ctxs[1] = cctx->dev->vb2_alloc_ctx;

	return 0;
}

#define vb2_to_v4l2_m2m_buffer(buf) \
	container_of(buf, struct v4l2_m2m_buffer, vb)

static int c8jpg_vb2_buf_prepare(struct vb2_buffer *vb)
{
	struct stm_c8jpg_ctx *cctx = vb2_get_drv_priv(vb->vb2_queue);
	struct c8jpg_jpeg_buf buf;
	int offset;

	if (cctx->ctx) {
		if (vb->vb2_queue->type == V4L2_BUF_TYPE_VIDEO_OUTPUT) {
			buf.data = vb2_plane_vaddr(vb, 0);
			buf.size = vb2_plane_size(vb, 0);
			buf.pos = 0;
			/* Bug 25787: the T1 plug can't read past
			   16777216 bytes. */
			if (buf.size > MAX_JPEG_SIZE)
				return -EINVAL;
			/* cctx->srcfmt gets exclusively set here.
			 * c8jpg_parse_JPEG_header() will zero-out
			 * cctx->srcfmt.fmt if the incoming buffer
			 * isn't a baseline JPEG.
			 */
			if (c8jpg_parse_JPEG_header(&buf, &cctx->srcfmt) < 0) {
				dev_warn(cctx->dev->v4l2dev.dev,
					 "failed parsing JPEG header!\n");
				cctx->srcfmt.fmt = NULL;
				return -EINVAL;
			}
			dev_dbg(cctx->dev->v4l2dev.dev,
				"SOF0 size: %dx%d, fmt: %s\n",
				cctx->srcfmt.width,
				cctx->srcfmt.height,
				cctx->srcfmt.fmt->description);
			if (cctx->srcfmt.width < MIN_JPEG_WIDTH
			    || cctx->srcfmt.width > MAX_JPEG_WIDTH
			    || cctx->srcfmt.height > MAX_JPEG_HEIGHT) {
				cctx->srcfmt.fmt = NULL;
				return -EINVAL;
			}
			cctx->DxT_offset = 0;
			buf.pos = 0;
			offset = c8jpg_skip_APPx_markers(&buf);
			dev_dbg(cctx->dev->v4l2dev.dev,
				"DxT tables offset: %d\n", offset);
			if (offset < 0) {
				cctx->srcfmt.fmt = NULL;
				return -EINVAL;
			}
			cctx->DxT_offset = offset;
		}
		if (vb->vb2_queue->type == V4L2_BUF_TYPE_VIDEO_CAPTURE
		    && (!cctx->dstfmt.fmt
			|| !cctx->srcfmt.fmt
			|| (cctx->dstfmt.fmt->fourcc
			    != cctx->srcfmt.fmt->fourcc)))
			return -EINVAL;
		if (vb->vb2_queue->type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE
		    && (!cctx->dstfmt.fmt
			|| !cctx->srcfmt.fmt
			|| (cctx->srcfmt.fmt->fourcc == V4L2_PIX_FMT_NV12
			    && cctx->dstfmt.fmt->fourcc != V4L2_PIX_FMT_NV12M)
			|| (cctx->srcfmt.fmt->fourcc == V4L2_PIX_FMT_NV16
			    && cctx->dstfmt.fmt->fourcc != V4L2_PIX_FMT_NV16M)
			|| (cctx->srcfmt.fmt->fourcc == V4L2_PIX_FMT_NV24
			    && cctx->dstfmt.fmt->fourcc != V4L2_PIX_FMT_NV24M)))
			return -EINVAL;
	}

	return 0;
}

static void c8jpg_vb2_buf_queue(struct vb2_buffer *vb)
{
	struct stm_c8jpg_ctx *cctx = vb2_get_drv_priv(vb->vb2_queue);

	if (cctx->ctx) {
		struct v4l2_m2m_buffer *buf;
		struct stm_c8jpg_m2m_buffer *cbuf;

		buf = vb2_to_v4l2_m2m_buffer(vb);
		cbuf = v4l2_to_c8jpg_m2m_buf(buf);

		if (vb->vb2_queue->type == V4L2_BUF_TYPE_VIDEO_OUTPUT) {
			cbuf->srcfmt.width = cctx->srcfmt.width;
			cbuf->srcfmt.height = cctx->srcfmt.height;
			cbuf->srcfmt.fmt = cctx->srcfmt.fmt;
		}

		if (vb->vb2_queue->type == V4L2_BUF_TYPE_VIDEO_CAPTURE
		    || vb->vb2_queue->type
				== V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE) {
			cbuf->dstfmt.width = cctx->dstfmt.width;
			cbuf->dstfmt.height = cctx->dstfmt.height;
			cbuf->dstfmt.fmt = cctx->dstfmt.fmt;
			/* Invalidate the destination format causing the next
			 * call to c8jpg_vb2_buf_prepare() to fail. Userspace
			 * has to explicitly issue a new VIDIOC_S_FMT (dst)
			 * before queueing the next picture for decoding.
			 */
			cctx->dstfmt.fmt = NULL;
		}

		memcpy(&cbuf->crop, &cctx->crop, sizeof(cctx->crop));

		v4l2_m2m_buf_queue(cctx->ctx, vb);
	}
}

static void c8jpg_vb2_unlock(struct vb2_queue *vq)
{
	struct stm_c8jpg_ctx *cctx = vb2_get_drv_priv(vq);

	mutex_unlock(&cctx->dev->mutex_lock);
}

static void c8jpg_vb2_lock(struct vb2_queue *vq)
{
	struct stm_c8jpg_ctx *cctx = vb2_get_drv_priv(vq);

	mutex_lock(&cctx->dev->mutex_lock);
}

static int c8jpg_vb2_buf_finish(struct vb2_buffer *vb)
{
	struct stm_c8jpg_ctx *cctx = vb2_get_drv_priv(vb->vb2_queue);
	struct v4l2_m2m_buffer *buf;
	struct stm_c8jpg_m2m_buffer *cbuf;
	struct stm_c8jpg_device *dev = cctx->dev;
#if defined(CONFIG_VIDEO_STM_C8JPG_DEBUG) && defined(CONFIG_CRC32C)
	void *luma, *chroma;
#endif

	if (vb->vb2_queue->type != V4L2_BUF_TYPE_VIDEO_CAPTURE
	    && vb->vb2_queue->type != V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE)
		return 0;

	/* Update the shared context with the final output dimensions
	 * so that a g_fmt_vid_cap_mplane() can pick them up later on
	 */
	buf = vb2_to_v4l2_m2m_buffer(vb);
	cbuf = v4l2_to_c8jpg_m2m_buf(buf);

	/* Compute the scaled destination width (in pixels) based on
	 * the MCU resizing ratio so that the client can adequately
	 * crop the capture buffer for any subsequent processing.
	 */
	cctx->resfmt.width =
			(vb->state == VB2_BUF_STATE_DONE)
				? fixtoint(dev->state.ratio
					   * cctx->srcfmt.width)
				: -1;
	cctx->resfmt.pitch =
			(vb->state == VB2_BUF_STATE_DONE)
				? cbuf->dstfmt.pitch
				: -1;
	cctx->resfmt.height =
			(vb->state == VB2_BUF_STATE_DONE)
				? (cctx->srcfmt.height
				   / (1 << dev->state.vdec_y))
				: -1;
	cctx->resfmt.mcu_height =
			(vb->state == VB2_BUF_STATE_DONE)
				? cbuf->dstfmt.height
				: -1;
	cctx->resfmt.fmt =
			(vb->state == VB2_BUF_STATE_DONE)
				? cbuf->dstfmt.fmt
				: NULL;

#if defined(CONFIG_VIDEO_STM_C8JPG_DEBUG) && defined(CONFIG_CRC32C)
	dev->luma_crc32c = dev->chroma_crc32c = 0;
	if (vb->state != VB2_BUF_STATE_DONE)
		return 0;
	luma = vb2_plane_vaddr(vb, 0);
	BUG_ON(!luma);
	switch (vb->vb2_queue->type) {
	case V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE:
		chroma = vb2_plane_vaddr(vb, 1);
		/* crc32c seed == 0 */
		dev->luma_crc32c =
			crc32c(0, luma, vb->v4l2_planes[0].length);
		dev->chroma_crc32c =
			crc32c(0, chroma, vb->v4l2_planes[1].length);
		break;
	case V4L2_BUF_TYPE_VIDEO_CAPTURE:
		chroma = luma + cctx->resfmt.pitch * cctx->resfmt.mcu_height;
		BUG_ON(vb->v4l2_planes[0].length
		       <= (cctx->resfmt.pitch * cctx->resfmt.mcu_height));
		/* crc32c seed == 0 */
		dev->luma_crc32c =
			crc32c(0, luma,
			       cctx->resfmt.pitch * cctx->resfmt.mcu_height);
		dev->chroma_crc32c =
			crc32c(0, chroma,
			       vb->v4l2_planes[0].length -
			       cctx->resfmt.pitch * cctx->resfmt.mcu_height);
		break;
	default:
		BUG_ON(1);
		break;
	}
#endif
	return 0;
}

static int c8jpg_vb2_start_streaming(struct vb2_queue *q, unsigned int count)
{
	struct stm_c8jpg_ctx *cctx = vb2_get_drv_priv(q);
	int ret;

	/* ref count++ */
	ret = pm_runtime_get_sync(cctx->dev->dev);

	return ret > 0 ? 0 : ret;
}

static int c8jpg_vb2_stop_streaming(struct vb2_queue *q)
{
	struct stm_c8jpg_ctx *cctx = vb2_get_drv_priv(q);

	/* ref count-- */
	pm_runtime_put(cctx->dev->dev);

	return 0;
}

static struct vb2_ops c8jpg_vq_ops = {
	.queue_setup = c8jpg_vb2_queue_setup,
	.buf_prepare = c8jpg_vb2_buf_prepare,
	.buf_queue = c8jpg_vb2_buf_queue,
	.wait_prepare = c8jpg_vb2_unlock,
	.wait_finish = c8jpg_vb2_lock,
	.buf_finish = c8jpg_vb2_buf_finish,
	.start_streaming = c8jpg_vb2_start_streaming,
	.stop_streaming = c8jpg_vb2_stop_streaming
};

static int stm_c8jpg_queue_init(void *priv,
				struct vb2_queue *src_vq,
				struct vb2_queue *dst_vq)
{
	struct stm_c8jpg_ctx *cctx = priv;

	int ret;

	memset(src_vq, 0, sizeof(*src_vq));

	src_vq->type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
	src_vq->io_modes = VB2_USERPTR | VB2_MMAP;
	src_vq->drv_priv = cctx;
	src_vq->buf_struct_size = sizeof(struct stm_c8jpg_m2m_buffer);
	src_vq->ops = &c8jpg_vq_ops;
	src_vq->mem_ops = &vb2_bpa2_contig_memops;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 9, 0))
	src_vq->timestamp_type = V4L2_BUF_FLAG_TIMESTAMP_MONOTONIC;
#endif
	ret = vb2_queue_init(src_vq);
	if (ret)
		return ret;

	memset(dst_vq, 0, sizeof(*dst_vq));

	dst_vq->type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	dst_vq->io_modes = VB2_USERPTR | VB2_MMAP;
	dst_vq->drv_priv = cctx;
	dst_vq->buf_struct_size = sizeof(struct stm_c8jpg_m2m_buffer);
	dst_vq->ops = &c8jpg_vq_ops;
	dst_vq->mem_ops = &vb2_bpa2_contig_memops;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 9, 0))
	dst_vq->timestamp_type = V4L2_BUF_FLAG_TIMESTAMP_MONOTONIC;
#endif

	return vb2_queue_init(dst_vq);
}

static int stm_c8jpg_queue_mplane_init(void *priv,
				       struct vb2_queue *src_vq,
				       struct vb2_queue *dst_vq)
{
	struct stm_c8jpg_ctx *cctx = priv;

	int ret;

	memset(src_vq, 0, sizeof(*src_vq));

	src_vq->type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
	src_vq->io_modes = VB2_USERPTR;
	src_vq->drv_priv = cctx;
	src_vq->buf_struct_size = sizeof(struct stm_c8jpg_m2m_buffer);
	src_vq->ops = &c8jpg_vq_ops;
	src_vq->mem_ops = &vb2_bpa2_contig_memops;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 9, 0))
	src_vq->timestamp_type = V4L2_BUF_FLAG_TIMESTAMP_MONOTONIC;
#endif
	ret = vb2_queue_init(src_vq);
	if (ret)
		return ret;

	memset(dst_vq, 0, sizeof(*dst_vq));

	dst_vq->type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	dst_vq->io_modes = VB2_USERPTR;
	dst_vq->drv_priv = cctx;
	dst_vq->buf_struct_size = sizeof(struct stm_c8jpg_m2m_buffer);
	dst_vq->ops = &c8jpg_vq_ops;
	dst_vq->mem_ops = &vb2_bpa2_contig_memops;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 9, 0))
	dst_vq->timestamp_type = V4L2_BUF_FLAG_TIMESTAMP_MONOTONIC;
#endif
	return vb2_queue_init(dst_vq);
}

static int c8jpg_calc_output(struct stm_c8jpg_buf_fmt *srcfmt,
			     struct stm_c8jpg_buf_fmt *dstfmt,
			     struct c8jpg_output_info *output,
			     struct c8jpg_resizer_state *state)
{
	unsigned int mcu_width, mcu_height;
	unsigned int out_width, out_height;
	unsigned int rsz_ratio, ratio;
	unsigned int vdec;
	unsigned int width, hwwidth;

	const unsigned int fix_almost_one = 32764; /* 0.9999 */

	BUG_ON(!state);

	BUG_ON(!srcfmt->fmt);
	BUG_ON(!dstfmt->fmt);

	if (!state)
		return -EINVAL;

	if (!srcfmt->fmt
	    || !dstfmt->fmt)
		return -EINVAL;

	memset(state, 0, sizeof(*state));

	switch (srcfmt->fmt->fourcc) {
	case V4L2_PIX_FMT_NV12:
		mcu_width = (srcfmt->width + 15) / 16;
		mcu_height = (srcfmt->height + 15) / 16;
		state->scan_in.sampling = C8JPG_CHROMA_SAMPLING_420;
		state->scan_out.sampling = C8JPG_CHROMA_SAMPLING_420;
		break;
	case V4L2_PIX_FMT_NV16:
		mcu_width = (srcfmt->width + 15) / 16;
		mcu_height = (srcfmt->height + 7) / 8;
		state->scan_in.sampling = C8JPG_CHROMA_SAMPLING_422;
		state->scan_out.sampling = C8JPG_CHROMA_SAMPLING_422;
		break;
	case V4L2_PIX_FMT_NV24:
		mcu_width = (srcfmt->width + 7) / 8;
		mcu_height = (srcfmt->height + 7) / 8;
		state->scan_in.sampling = C8JPG_CHROMA_SAMPLING_444;
		state->scan_out.sampling = C8JPG_CHROMA_SAMPLING_444;
		break;
	default:
		BUG_ON(1);
		return -EINVAL;
	}

	/* The reconstructed buffer's width is a multiple of a pair of MCUs */
	if (dstfmt->fmt->fourcc == V4L2_PIX_FMT_NV24
	    || dstfmt->fmt->fourcc == V4L2_PIX_FMT_NV24M)
		width = (dstfmt->width + 15) & ~15L;
	else
		width = (dstfmt->width + 31) & ~31L;

	/* Take into account the rounding to the next-MCU for both the
	   source and destination buffers */
	rsz_ratio = fixdiv(width,
			   mcu_width
			   * ((srcfmt->fmt->fourcc == V4L2_PIX_FMT_NV24)
			      ? 8 : 16));

	/* Resize ratio is < 1/8 */
	if (rsz_ratio < 4096)
		return -EINVAL;

	ratio = rsz_ratio;

	/* Clamp the resize ratio to 8 if it's > 8 */
	if (rsz_ratio > inttofix(8))
		ratio = inttofix(8);

	/* Always full picture, no cropping for now */
	output->region.left = 0;
	output->region.top = 0;
	output->region.width = mcu_width;
	output->region.height = mcu_height;

	/* 1. compute the resulting output width */
	out_width =
		(fixtoint(mcu_width * ratio + fix_almost_one)
		 * state->scan_in.hy)
			/ state->scan_out.hy;

	/* The resizer always emits a pair of MCUs */
	BUG_ON(out_width & 1);

	hwwidth = out_width
		  * ((dstfmt->fmt->fourcc == V4L2_PIX_FMT_NV24
		      || dstfmt->fmt->fourcc == V4L2_PIX_FMT_NV24M)
		     ? 8 : 16);

	/* The decoder can't produce more than MAX_PICTURE_WIDTH pixels */
	if (hwwidth > MAX_PICTURE_WIDTH)
		return -EINVAL;

	BUG_ON(rsz_ratio <= inttofix(8)
	       && hwwidth != width);

	output->width = hwwidth;

	if (rsz_ratio < inttofix(8))
		/* Always request a 32 pixels aligned reconstruction buffer */
		output->pitch = (dstfmt->width + 31) & ~31L;
	else
		output->pitch = output->width;

	/* 2. compute the resulting output height */
	out_height = output->region.height
		     * ((dstfmt->fmt->fourcc == V4L2_PIX_FMT_NV12
			 || dstfmt->fmt->fourcc == V4L2_PIX_FMT_NV12M)
			? 16 : 8);

	/* 2.1 decimate vertically if possible */
	vdec = 0;
	if (dstfmt->height < out_height) {
		while (vdec < 3 && (out_height >> 1) >= dstfmt->height) {
			out_height >>= 1;
			vdec++;
		}
	}

	output->height = out_height;

	/* 3. fill in the resizer state */
	state->mcu_width = out_width;

	state->incr_y = fixdiv(state->scan_in.hy * mcu_width,
			       state->scan_out.hy * state->mcu_width);

	state->incr_uv = fixdiv(state->scan_in.hcb * mcu_width,
				state->scan_out.hcb * state->mcu_width);

	state->vdec_y = vdec;
	state->vdec_uv = vdec + (state->scan_out.vy - state->scan_in.vy);

	/* also save the mcu scaling ratio */
	state->ratio = ratio;

	c8jpg_setup_filtering_tables(state);

	return 0;
}

static void c8jpg_device_run(void *priv)
{
	struct stm_c8jpg_ctx *cctx = (struct stm_c8jpg_ctx *)priv;
	struct stm_c8jpg_device *dev = cctx->dev;

	struct vb2_buffer *src, *dst;
	struct v4l2_m2m_buffer *buf;
	struct stm_c8jpg_m2m_buffer *srcbuf, *dstbuf;
	unsigned long src_phys;
	unsigned long dst_luma_phys, dst_chroma_phys = 0;

	struct c8jpg_output_info output;
	struct c8jpg_resizer_state state;

	src = v4l2_m2m_next_src_buf(cctx->ctx);
	dst = v4l2_m2m_next_dst_buf(cctx->ctx);

	buf = vb2_to_v4l2_m2m_buffer(src);
	srcbuf = v4l2_to_c8jpg_m2m_buf(buf);

	buf = vb2_to_v4l2_m2m_buffer(dst);
	dstbuf = v4l2_to_c8jpg_m2m_buf(buf);

	c8jpg_calc_output(&srcbuf->srcfmt, &dstbuf->dstfmt, &output, &state);

	dstbuf->dstfmt.width = output.width;
	dstbuf->dstfmt.pitch = output.pitch;
	dstbuf->dstfmt.height = output.height;

	src_phys = vb2_bpa2_contig_plane_dma_addr(src, 0);
	dst_luma_phys = vb2_bpa2_contig_plane_dma_addr(dst, 0);

	switch (dst->v4l2_buf.type) {
	case V4L2_BUF_TYPE_VIDEO_CAPTURE:
		dst_chroma_phys = dst_luma_phys
				  + output.pitch * output.height;
		break;
	case V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE:
		dst_chroma_phys = vb2_bpa2_contig_plane_dma_addr(dst, 1);
		break;
	default:
		BUG_ON(1);
		break;
	}

	c8jpg_hw_init(dev);

	c8jpg_set_source(dev, src_phys + cctx->DxT_offset,
			 src->v4l2_planes[0].length - cctx->DxT_offset);
	c8jpg_set_destination(dev, dst_luma_phys, dst_chroma_phys);

	c8jpg_set_cropping(dev, &output);
	c8jpg_set_output_format(dev, &state);

	c8jpg_setup_h_resize(dev, &state);
	c8jpg_setup_v_resize(dev, &state);

	c8jpg_setup_outputstage(dev, &state);

	/* update the global device state */
	dev->state = state;

#ifdef CONFIG_VIDEO_STM_C8JPG_DEBUG
	dev->hwdecode_start = ktime_get();
#endif
	/* kick the decoder */
	c8jpg_decode_image(dev);
}

static void c8jpg_job_abort(void *priv)
{
}

struct v4l2_m2m_ops c8jpg_v4l2_m2m_ops = {
	.device_run = c8jpg_device_run,
	.job_ready = 0,
	.job_abort = c8jpg_job_abort,
};

static int enum_c8jpg_fmt(enum v4l2_buf_type type,
			  struct v4l2_fmtdesc *f)
{
	int i, idx = 0;

	for (i = 0; i < NUM_C8JPG_FORMATS; i++) {
		if (c8jpg_fmts[i].type == type) {
			if (f->index == idx)
				break;
			idx++;
		}
	}

	if (i == NUM_C8JPG_FORMATS)
		return -EINVAL;

	f->pixelformat = c8jpg_fmts[i].fourcc;
	strlcpy(f->description,
		c8jpg_fmts[i].description, sizeof(f->description));

	return 0;
}

static int stm_c8jpg_querycap(struct file *file,
			      void *fh,
			      struct v4l2_capability *cap)
{
	memset(cap, 0, sizeof(struct v4l2_capability));

	strlcpy(cap->driver, "JPEG Decoder", sizeof(cap->driver));
	strlcpy(cap->card, "STMicroelectronics", sizeof(cap->card));

	cap->capabilities = V4L2_CAP_VIDEO_OUTPUT |
			    V4L2_CAP_VIDEO_CAPTURE |
			    V4L2_CAP_VIDEO_CAPTURE_MPLANE |
			    V4L2_CAP_STREAMING;

	cap->version = LINUX_VERSION_CODE;

	return 0;
}

static int stm_c8jpg_enum_fmt_vid_out(struct file *file,
				      void *fh,
				      struct v4l2_fmtdesc *f)
{
	return enum_c8jpg_fmt(V4L2_BUF_TYPE_VIDEO_OUTPUT, f);
}

static int stm_c8jpg_enum_fmt_vid_cap_mplane(struct file *file,
					     void *fh,
					     struct v4l2_fmtdesc *f)
{
	return enum_c8jpg_fmt(V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE, f);
}

static int stm_c8jpg_enum_fmt_vid_cap(struct file *file,
				      void *fh,
				      struct v4l2_fmtdesc *f)
{
	return enum_c8jpg_fmt(V4L2_BUF_TYPE_VIDEO_CAPTURE, f);
}

static int stm_c8jpg_g_fmt_vid_out(struct file *file,
				   void *fh,
				   struct v4l2_format *f)
{
	struct stm_c8jpg_ctx *cctx = (struct stm_c8jpg_ctx *)fh;

	if (!cctx->srcfmt.fmt)
		return -EINVAL;

	f->fmt.pix.pixelformat = cctx->srcfmt.fmt->fourcc;
	f->fmt.pix.field = V4L2_FIELD_NONE;
	f->fmt.pix.width = cctx->srcfmt.width;
	f->fmt.pix.height = cctx->srcfmt.height;
	f->fmt.pix.bytesperline = 0;
	f->fmt.pix.sizeimage = 0;

	return 0;
}

static int stm_c8jpg_g_fmt_vid_cap_mplane(struct file *file,
					  void *fh,
					  struct v4l2_format *f)
{
	struct stm_c8jpg_ctx *cctx = (struct stm_c8jpg_ctx *)fh;

	/* if cctx->resfmt.fmt isn't set at this stage then
	 * the previous decode has failed.
	 */
	if (!cctx->resfmt.fmt)
		return -EINVAL;

	f->fmt.pix_mp.pixelformat = cctx->resfmt.fmt->fourcc;
	f->fmt.pix_mp.field = V4L2_FIELD_NONE;
	f->fmt.pix_mp.width = cctx->resfmt.width;
	f->fmt.pix_mp.height = cctx->resfmt.height;
	f->fmt.pix_mp.num_planes = cctx->resfmt.fmt->nplanes;

	f->fmt.pix_mp.plane_fmt[0].bytesperline = cctx->resfmt.pitch;
	f->fmt.pix_mp.plane_fmt[0].sizeimage =
			cctx->resfmt.pitch * cctx->resfmt.mcu_height;

	f->fmt.pix_mp.plane_fmt[1].bytesperline = cctx->resfmt.pitch;
	f->fmt.pix_mp.plane_fmt[1].sizeimage =
			(cctx->resfmt.pitch
			 * cctx->resfmt.mcu_height
			 * cctx->resfmt.fmt->uvsampling.numerator
			 ) / cctx->resfmt.fmt->uvsampling.denominator;

	return 0;
}

static int stm_c8jpg_g_fmt_vid_cap(struct file *file,
				   void *fh,
				   struct v4l2_format *f)
{
	struct stm_c8jpg_ctx *cctx = (struct stm_c8jpg_ctx *)fh;

	/* if cctx->resfmt.fmt isn't set at this stage then
	 * the previous decode has failed.
	 */
	if (!cctx->resfmt.fmt)
		return -EINVAL;

	f->fmt.pix.pixelformat = cctx->resfmt.fmt->fourcc;
	f->fmt.pix.field = V4L2_FIELD_NONE;
	f->fmt.pix.width = cctx->resfmt.width;
	f->fmt.pix.height = cctx->resfmt.height;
	f->fmt.pix.bytesperline = cctx->resfmt.pitch;
	f->fmt.pix.sizeimage =
			cctx->resfmt.pitch * cctx->resfmt.mcu_height
			+ (cctx->resfmt.pitch
			   * cctx->resfmt.mcu_height
			   * cctx->resfmt.fmt->uvsampling.numerator
			   ) / cctx->resfmt.fmt->uvsampling.denominator;

	return 0;
}

static int stm_c8jpg_try_fmt(struct stm_c8jpg_ctx *cctx,
			     struct v4l2_format *f,
			     enum v4l2_buf_type type,
			     bool *match)
{
	struct stm_c8jpg_buf_fmt tryfmt;
	struct c8jpg_output_info output;
	struct c8jpg_resizer_state state;
	unsigned int pixelformat = -1;

	switch (type) {
	case V4L2_BUF_TYPE_VIDEO_OUTPUT:
		if (f->fmt.pix.sizeimage > MAX_JPEG_SIZE)
			return -EINVAL;
		break;
	case V4L2_BUF_TYPE_VIDEO_CAPTURE:
		if (!f->fmt.pix.width
		    || !f->fmt.pix.height)
			return -EINVAL;
		if (f->fmt.pix.width > MAX_PICTURE_WIDTH
		    || f->fmt.pix.height > MAX_PICTURE_HEIGHT)
			return -EINVAL;
		break;
	case V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE:
		if (!f->fmt.pix_mp.width
		    || !f->fmt.pix_mp.height)
			return -EINVAL;
		if (f->fmt.pix_mp.width > MAX_PICTURE_WIDTH
		    || f->fmt.pix_mp.height > MAX_PICTURE_HEIGHT)
			return -EINVAL;
		break;
	default:
		BUG_ON(1);
	}

	if (type == V4L2_BUF_TYPE_VIDEO_CAPTURE
	    || type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE) {
		if (!cctx->srcfmt.fmt)
			return -EINVAL;

		switch (cctx->srcfmt.fmt->fourcc) {
		case V4L2_PIX_FMT_NV12:
			pixelformat = (type == V4L2_BUF_TYPE_VIDEO_CAPTURE)
				      ? V4L2_PIX_FMT_NV12 : V4L2_PIX_FMT_NV12M;
			break;
		case V4L2_PIX_FMT_NV16:
			pixelformat = (type == V4L2_BUF_TYPE_VIDEO_CAPTURE)
				      ? V4L2_PIX_FMT_NV16 : V4L2_PIX_FMT_NV16M;
			break;
		case V4L2_PIX_FMT_NV24:
			pixelformat = (type == V4L2_BUF_TYPE_VIDEO_CAPTURE)
				      ? V4L2_PIX_FMT_NV24 : V4L2_PIX_FMT_NV24M;
			break;
		default:
			BUG_ON(1);
			return -EINVAL;
		}
	}

	if (match) {
		*match = (type == V4L2_BUF_TYPE_VIDEO_CAPTURE
			  && f->fmt.pix.pixelformat == pixelformat)
			 || (type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE
			     && f->fmt.pix_mp.pixelformat == pixelformat)
			 || (type == V4L2_BUF_TYPE_VIDEO_OUTPUT
			     && f->fmt.pix.pixelformat == V4L2_PIX_FMT_JPEG);
	}

	if (type != V4L2_BUF_TYPE_VIDEO_OUTPUT)
		dev_dbg(cctx->dev->v4l2dev.dev,
			"user wants: %c%c%c%c, driver suggests: %c%c%c%c\n",
			f->fmt.pix.pixelformat & 0xff,
			(f->fmt.pix.pixelformat >> 8) & 0xff,
			(f->fmt.pix.pixelformat >> 16) & 0xff,
			f->fmt.pix.pixelformat >> 24,
			pixelformat & 0xff,
			(pixelformat >> 8) & 0xff,
			(pixelformat >> 16) & 0xff,
			pixelformat >> 24);

	switch (type) {
	case V4L2_BUF_TYPE_VIDEO_OUTPUT:
		f->fmt.pix.pixelformat = V4L2_PIX_FMT_JPEG;
		f->fmt.pix.field = V4L2_FIELD_NONE;
		f->fmt.pix.bytesperline = 0;
		/* Don't overwrite if a sizeimage is specified */
		if (!f->fmt.pix.sizeimage)
			f->fmt.pix.sizeimage = PAGE_SIZE;
		break;
	case V4L2_BUF_TYPE_VIDEO_CAPTURE:
		tryfmt.width = f->fmt.pix.width;
		tryfmt.height = f->fmt.pix.height;
		tryfmt.fmt = find_fmt(type, pixelformat);

		if (c8jpg_calc_output(&cctx->srcfmt,
				      &tryfmt, &output, &state) < 0)
			return -EINVAL;

		f->fmt.pix.width = output.width;
		f->fmt.pix.height = output.height;
		f->fmt.pix.bytesperline = output.pitch;
		f->fmt.pix.pixelformat = pixelformat;
		f->fmt.pix.sizeimage =
				output.pitch
				* output.height
				+ (output.pitch
				   * output.height
				   * tryfmt.fmt->uvsampling.numerator
				   ) / tryfmt.fmt->uvsampling.denominator;
		break;
	case V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE:
		tryfmt.width = f->fmt.pix_mp.width;
		tryfmt.height = f->fmt.pix_mp.height;
		tryfmt.fmt = find_fmt(type, pixelformat);

		if (c8jpg_calc_output(&cctx->srcfmt,
				      &tryfmt, &output, &state) < 0)
			return -EINVAL;

		f->fmt.pix_mp.width = output.width;
		f->fmt.pix_mp.height = output.height;
		f->fmt.pix_mp.pixelformat = pixelformat;
		f->fmt.pix_mp.num_planes = 2;
		f->fmt.pix_mp.plane_fmt[0].bytesperline = output.pitch;
		f->fmt.pix_mp.plane_fmt[0].sizeimage = output.pitch
						       * output.height;
		f->fmt.pix_mp.plane_fmt[1].bytesperline = output.pitch;
		f->fmt.pix_mp.plane_fmt[1].sizeimage =
				(output.pitch
				 * output.height
				 * tryfmt.fmt->uvsampling.numerator
				 ) / tryfmt.fmt->uvsampling.denominator;
		break;
	default:
		break;
	}

	return 0;
}

static int stm_c8jpg_try_fmt_vid_out(struct file *file,
				     void *fh,
				     struct v4l2_format *f)
{
	return stm_c8jpg_try_fmt(fh, f, V4L2_BUF_TYPE_VIDEO_OUTPUT, NULL);
}

static int stm_c8jpg_try_fmt_vid_cap_mplane(struct file *file,
					    void *fh,
					    struct v4l2_format *f)
{
	struct stm_c8jpg_ctx *cctx = (struct stm_c8jpg_ctx *)fh;

	if (!cctx->srcfmt.fmt)
		return -EINVAL;

	return stm_c8jpg_try_fmt(cctx, f,
				 V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE, NULL);
}

static int stm_c8jpg_try_fmt_vid_cap(struct file *file,
				     void *fh,
				     struct v4l2_format *f)
{
	struct stm_c8jpg_ctx *cctx = (struct stm_c8jpg_ctx *)fh;

	if (!cctx->srcfmt.fmt)
		return -EINVAL;

	return stm_c8jpg_try_fmt(cctx, f, V4L2_BUF_TYPE_VIDEO_CAPTURE, NULL);
}

static int stm_c8jpg_s_fmt(struct stm_c8jpg_ctx *cctx,
			   struct v4l2_format *f,
			   enum v4l2_buf_type type)
{
	struct stm_c8jpg_buf_fmt *fmt;
	struct stm_c8jpg_format *format;

	if (type == V4L2_BUF_TYPE_VIDEO_OUTPUT) {
		/* For VL42_MEMORY_MMAP buffers */
		cctx->srcfmt.size = f->fmt.pix.sizeimage;
		return 0;
	}

	fmt = &cctx->dstfmt;

	format = find_fmt(type,
			  (type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE)
			  ? f->fmt.pix_mp.pixelformat
			  : f->fmt.pix.pixelformat);
	if (!format)
		return -EINVAL;

	fmt->width = (type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE)
		     ? f->fmt.pix_mp.width : f->fmt.pix.width;
	fmt->height = (type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE)
		      ? f->fmt.pix_mp.height : f->fmt.pix.height;
	fmt->fmt = format;

	/* For VL42_MEMORY_MMAP buffers */
	fmt->size = f->fmt.pix.sizeimage;

	return 0;
}

static int stm_c8jpg_s_fmt_vid_out(struct file *file,
				   void *fh,
				   struct v4l2_format *f)
{
	struct v4l2_format format;
	bool match = true;
	int ret;

	format = *f;

	ret = stm_c8jpg_try_fmt(fh, &format,
				V4L2_BUF_TYPE_VIDEO_OUTPUT, &match);

	if (ret || !match)
		return -EINVAL;

	return stm_c8jpg_s_fmt(fh, f, V4L2_BUF_TYPE_VIDEO_OUTPUT);
}

static int stm_c8jpg_s_fmt_vid_cap_mplane(struct file *file,
					  void *fh,
					  struct v4l2_format *f)
{
	struct stm_c8jpg_ctx *cctx = (struct stm_c8jpg_ctx *)fh;
	struct stm_c8jpg_buf_fmt *fmt = &cctx->dstfmt;
	struct v4l2_format format;
	bool match = true;

	int ret;

	fmt->width = -1;
	fmt->height = -1;
	fmt->fmt = NULL;

	format = *f;

	ret = stm_c8jpg_try_fmt(cctx, &format,
				V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE, &match);
	if (ret || !match)
		return -EINVAL;

	if ((f->fmt.pix_mp.plane_fmt[0].sizeimage <
	    format.fmt.pix_mp.plane_fmt[0].sizeimage)
	    || (f->fmt.pix_mp.plane_fmt[1].sizeimage <
		format.fmt.pix_mp.plane_fmt[1].sizeimage))
		return -EINVAL;

	return stm_c8jpg_s_fmt(fh, f, V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE);
}

static int stm_c8jpg_s_fmt_vid_cap(struct file *file,
				   void *fh,
				   struct v4l2_format *f)
{
	struct stm_c8jpg_ctx *cctx = (struct stm_c8jpg_ctx *)fh;
	struct stm_c8jpg_buf_fmt *fmt = &cctx->dstfmt;
	struct v4l2_format format;
	bool match;

	int ret;

	fmt->width = -1;
	fmt->height = -1;
	fmt->fmt = NULL;

	format = *f;

	ret = stm_c8jpg_try_fmt(cctx, &format,
				V4L2_BUF_TYPE_VIDEO_CAPTURE, &match);
	if (ret || !match)
		return -EINVAL;

	if (f->fmt.pix.sizeimage < format.fmt.pix.sizeimage)
		return -EINVAL;

	return stm_c8jpg_s_fmt(fh, f, V4L2_BUF_TYPE_VIDEO_CAPTURE);
}

static int stm_c8jpg_reqbufs(struct file *file,
			     void *fh,
			     struct v4l2_requestbuffers *b)
{
	struct stm_c8jpg_device *dev = video_drvdata(file);
	struct stm_c8jpg_ctx *cctx = (struct stm_c8jpg_ctx *)fh;

	int ret;

	/* Default to a classic planar capture mode when
	 * V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE isn't specified.
	 */
	if (!cctx->ctx) {
		if (b->type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE)
			cctx->ctx =
				v4l2_m2m_ctx_init(dev->m2mdev, cctx,
						  stm_c8jpg_queue_mplane_init);
		else
			cctx->ctx =
				v4l2_m2m_ctx_init(dev->m2mdev, cctx,
						  stm_c8jpg_queue_init);
		if (IS_ERR(cctx->ctx)) {
			ret = PTR_ERR(cctx->ctx);
			return ret;
		}
	}

	return v4l2_m2m_reqbufs(file, cctx->ctx, b);
}

static int stm_c8jpg_querybufs(struct file *file,
			       void *fh,
			       struct v4l2_buffer *b)
{
	struct stm_c8jpg_ctx *cctx = (struct stm_c8jpg_ctx *)fh;

	return v4l2_m2m_querybuf(file, cctx->ctx, b);
}

static int stm_c8jpg_qbuf(struct file *file,
			  void *fh,
			  struct v4l2_buffer *b)
{
	struct stm_c8jpg_ctx *cctx = (struct stm_c8jpg_ctx *)fh;

	return v4l2_m2m_qbuf(file, cctx->ctx, b);
}

static int stm_c8jpg_dqbuf(struct file *file,
			   void *fh,
			   struct v4l2_buffer *b)
{
	struct stm_c8jpg_ctx *cctx = (struct stm_c8jpg_ctx *)fh;

	return v4l2_m2m_dqbuf(file, cctx->ctx, b);
}

static int stm_c8jpg_streamon(struct file *file,
			      void *fh,
			      enum v4l2_buf_type i)
{
	struct stm_c8jpg_ctx *cctx = (struct stm_c8jpg_ctx *)fh;

	return v4l2_m2m_streamon(file, cctx->ctx, i);
}

static int stm_c8jpg_streamoff(struct file *file,
			       void *fh,
			       enum v4l2_buf_type i)
{
	struct stm_c8jpg_ctx *cctx = (struct stm_c8jpg_ctx *)fh;

	return v4l2_m2m_streamoff(file, cctx->ctx, i);
}

struct v4l2_ioctl_ops stm_c8jpg_vidioc_ioctls_ops = {
	.vidioc_querycap = stm_c8jpg_querycap,

	.vidioc_enum_fmt_vid_cap_mplane = stm_c8jpg_enum_fmt_vid_cap_mplane,
	.vidioc_enum_fmt_vid_cap = stm_c8jpg_enum_fmt_vid_cap,
	.vidioc_enum_fmt_vid_out = stm_c8jpg_enum_fmt_vid_out,

	.vidioc_g_fmt_vid_cap_mplane = stm_c8jpg_g_fmt_vid_cap_mplane,
	.vidioc_g_fmt_vid_cap = stm_c8jpg_g_fmt_vid_cap,
	.vidioc_g_fmt_vid_out = stm_c8jpg_g_fmt_vid_out,

	.vidioc_s_fmt_vid_cap_mplane = stm_c8jpg_s_fmt_vid_cap_mplane,
	.vidioc_s_fmt_vid_cap = stm_c8jpg_s_fmt_vid_cap,
	.vidioc_s_fmt_vid_out = stm_c8jpg_s_fmt_vid_out,

	.vidioc_try_fmt_vid_cap_mplane = stm_c8jpg_try_fmt_vid_cap_mplane,
	.vidioc_try_fmt_vid_cap = stm_c8jpg_try_fmt_vid_cap,
	.vidioc_try_fmt_vid_out = stm_c8jpg_try_fmt_vid_out,

	.vidioc_reqbufs = stm_c8jpg_reqbufs,
	.vidioc_querybuf = stm_c8jpg_querybufs,

	.vidioc_qbuf = stm_c8jpg_qbuf,
	.vidioc_dqbuf = stm_c8jpg_dqbuf,

	.vidioc_streamon = stm_c8jpg_streamon,
	.vidioc_streamoff = stm_c8jpg_streamoff
};

static int stm_c8jpg_open(struct file *file)
{
	struct stm_c8jpg_device *dev = video_drvdata(file);
	struct video_device *vdev = video_devdata(file);
	struct stm_c8jpg_ctx *cctx;

	cctx = kzalloc(sizeof(struct stm_c8jpg_ctx), GFP_KERNEL);
	if (!cctx)
		return -ENOMEM;

	v4l2_fh_init(&cctx->fh, vdev);
	file->private_data = &cctx->fh;
	v4l2_fh_add(&cctx->fh);

	cctx->dev = dev;

	return 0;
}

static int stm_c8jpg_release(struct file *file)
{
	struct v4l2_fh *fh;
	struct stm_c8jpg_ctx *cctx;

	fh = (struct v4l2_fh *)file->private_data;
	cctx = fh_to_c8jpg_ctx(fh);

	if (cctx->ctx)
		v4l2_m2m_ctx_release(cctx->ctx);

	v4l2_fh_del(&cctx->fh);
	v4l2_fh_exit(&cctx->fh);

	kfree(cctx);

	return 0;
}

static int stm_c8jpg_mmap(struct file *file, struct vm_area_struct *vma)
{
	struct v4l2_fh *fh;
	struct stm_c8jpg_ctx *cctx;

	fh = (struct v4l2_fh *)file->private_data;
	BUG_ON(!fh);
	cctx = fh_to_c8jpg_ctx(fh);
	BUG_ON(!cctx);

	return v4l2_m2m_mmap(file, cctx->ctx, vma);
}

irqreturn_t c8jpg_irq_handler(int irq, void *priv)
{
	struct stm_c8jpg_device *dev = (struct stm_c8jpg_device *)priv;
	struct stm_c8jpg_ctx *cctx;
	struct vb2_buffer *src, *dst;
	enum vb2_buffer_state state = VB2_BUF_STATE_DONE;
	unsigned int status, status_its;
	unsigned long flags;

	/* Mask off any further watchdog interrupts */
	WRITE_REG(dev->iomem, MCU_ITM, MCU_ITS_watchdog_MASK);

	spin_lock_irqsave(&dev->lock, flags);

	/* A MCU_ITS read will clear the h/w interrupt status */
	status = READ_REG(dev->iomem, MCU_STA);
	status_its = READ_REG(dev->iomem, MCU_ITS);

	spin_unlock_irqrestore(&dev->lock, flags);

#ifdef CONFIG_VIDEO_STM_C8JPG_DEBUG
	dev->hwdecode_end = ktime_get();
	dev->hwdecode_end = ktime_sub(dev->hwdecode_end, dev->hwdecode_start);
	dev->hwdecode_time = ktime_to_timeval(dev->hwdecode_end);
#endif

	if (!((status_its & MCU_ITS_end_of_pic_MASK)
	      && ((status & MCU_STA_parsing_status_MASK)
		  == MCU_STA_parsing_status_OK))) {
		dev_err(dev->v4l2dev.dev,
			"h/w decode failure! (watchdog: %d)\n",
			status_its & MCU_ITS_watchdog_MASK);
		state = VB2_BUF_STATE_ERROR;
	}

	c8jpg_hw_reset(dev);

	cctx = v4l2_m2m_get_curr_priv(dev->m2mdev);
	BUG_ON(!cctx);

	if (!cctx)
		return IRQ_HANDLED;

	src = v4l2_m2m_src_buf_remove(cctx->ctx);
	dst = v4l2_m2m_dst_buf_remove(cctx->ctx);

	if (src)
		v4l2_m2m_buf_done(src, state);
	if (dst)
		v4l2_m2m_buf_done(dst, state);

	v4l2_m2m_job_finish(dev->m2mdev, cctx->ctx);

	if (state == VB2_BUF_STATE_DONE)
		dev->nframes++;

	return IRQ_HANDLED;
}

struct v4l2_file_operations stm_c8jpg_file_ops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = video_ioctl2,
	.open = stm_c8jpg_open,
	.release = stm_c8jpg_release,
	.mmap = stm_c8jpg_mmap
};

int stm_c8jpg_v4l2_pm_prepare(struct device *dev)
{
	struct stm_c8jpg_device *cdev = dev_get_drvdata(dev);
	struct stm_c8jpg_ctx *cctx;

	/* It might happen that a h/w decode is in progress with the device
	 * writing to the destination buffer (DMA) so if that's the case we'd
	 * first wait for the decode completion before continuing with the
	 * hibernation sequence.
	 */
	cctx = v4l2_m2m_get_curr_priv(cdev->m2mdev);
	if (cctx) {
		struct vb2_queue *vq;
		dev_dbg(dev, "waiting for h/w decode completion\n");
		vq = v4l2_m2m_get_vq(cctx->ctx, V4L2_BUF_TYPE_VIDEO_CAPTURE);
		if (vq)
			vb2_wait_for_all_buffers(vq);
	}

	return 0;
}
