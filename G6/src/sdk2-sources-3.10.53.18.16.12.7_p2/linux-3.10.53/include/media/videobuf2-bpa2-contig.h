/*
 * videobuf2-bpa2-contig.h - bpa2 contig memory allocator for videobuf2
 *
 * Copyright (C) 2012 STMicroelectronics Ltd.
 * Copyright (C) 2010 Samsung Electronics
 *
 * Based on code by:
 * Author: Pawel Osciak <pawel@osciak.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation.
 */

#ifndef _MEDIA_VIDEOBUF2_BPA2_CONTIG_H
#define _MEDIA_VIDEOBUF2_BPA2_CONTIG_H

#include <media/videobuf2-core.h>
#include <linux/bpa2.h>

static inline unsigned long
vb2_bpa2_contig_plane_dma_addr(struct vb2_buffer *vb, unsigned int plane_no)
{
	unsigned long *addr = vb2_plane_cookie(vb, plane_no);

	return *addr;
}

void *vb2_bpa2_contig_init_ctx(struct device *dev, struct bpa2_part *part);
void vb2_bpa2_contig_cleanup_ctx(void *alloc_ctx);

extern const struct vb2_mem_ops vb2_bpa2_contig_memops;

#endif
