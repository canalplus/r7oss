/*
 * c8jpg.h, a driver for the C8JPG1 JPEG decoder IP
 *
 * Copyright (C) 2012, STMicroelectronics R&D
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 */

#ifndef __STM_C8JPG_H
#define __STM_C8JPG_H

#include <linux/hrtimer.h>

#include <linux/videodev2.h>

#include <media/v4l2-device.h>
#include <media/v4l2-mem2mem.h>
#include <media/v4l2-ioctl.h>
#include <media/v4l2-fh.h>

#include "c8jpg-hw.h"

struct stm_c8jpg_device {
	struct device *dev;
	struct v4l2_device v4l2dev;
	struct video_device videodev;
	struct v4l2_m2m_dev *m2mdev;
	void * __iomem iomem;
	struct clk *clk_ip;
#ifndef CONFIG_ARCH_STI
	struct clk *clk_ic;
#endif
	int irq;
	spinlock_t lock;
	void *vb2_alloc_ctx;
	struct mutex mutex_lock;
	struct c8jpg_resizer_state state;
#ifdef CONFIG_DEBUG_FS
	struct dentry *debugfs;
#endif
	__u32 nframes; /* total decoded frames so far */
#ifdef CONFIG_VIDEO_STM_C8JPG_DEBUG
	/* crc32c checksum on the last decoded luma buffer */
	unsigned int luma_crc32c;
	/* crc32c checksum on the last decoded chroma buffer */
	unsigned int chroma_crc32c;
	/* time spent decoding the last picture */
	ktime_t hwdecode_start;
	ktime_t hwdecode_end;
	struct timeval hwdecode_time;
#endif
};

#define v4l2_dev_to_c8jpg_dev(dev) \
	container_of(dev, struct stm_c8jpg_device, v4l2dev)

#define video_dev_to_c8jpg_dev(vdev) \
	container_of(vdev, struct stm_c8jpg_device, videodev)

struct stm_c8jpg_format {
	unsigned int fourcc;
	unsigned int nplanes;
	unsigned int bpp;
	struct v4l2_fract uvsampling;
	enum v4l2_buf_type type;
	const char *description;
};

struct stm_c8jpg_buf_fmt {
	struct stm_c8jpg_format *fmt;
	unsigned int width;
	unsigned int height;
	unsigned int pitch;
	unsigned int size;
	unsigned int mcu_height;
};

struct stm_c8jpg_ctx {
	struct v4l2_fh fh;
	struct stm_c8jpg_device *dev;
	struct v4l2_m2m_ctx *ctx;
	struct stm_c8jpg_buf_fmt srcfmt; /* internal variable */
	struct stm_c8jpg_buf_fmt dstfmt; /* only accessible by s_fmt capture */
	struct stm_c8jpg_buf_fmt resfmt; /* only accessible by g_fmt */
	struct v4l2_crop crop; /* accessible by s_crop and g_crop */
	unsigned int DxT_offset; /* offset to the DHT/DQT tables */
};

#define fh_to_c8jpg_ctx(fh) \
	container_of(fh, struct stm_c8jpg_ctx, fh)

struct stm_c8jpg_m2m_buffer {
	struct v4l2_m2m_buffer buf;
	struct stm_c8jpg_buf_fmt srcfmt;
	struct stm_c8jpg_buf_fmt dstfmt;
	struct v4l2_crop crop;
};

#define v4l2_to_c8jpg_m2m_buf(buf) \
	container_of(buf, struct stm_c8jpg_m2m_buffer, buf)

#endif /* __STM_C8JPG_H */
