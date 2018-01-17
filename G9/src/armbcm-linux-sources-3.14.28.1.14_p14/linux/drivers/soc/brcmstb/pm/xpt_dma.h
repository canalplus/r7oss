/*
 * Copyright © 2014 Broadcom Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef __XPT_DMA_H__
#define __XPT_DMA_H__

#include <linux/types.h>

struct dma_region {
	dma_addr_t addr;
	size_t len;
};

struct mcpb_dma_desc {
	uint32_t buf_hi;
	uint32_t buf_lo;
	uint32_t next_offs; /* or 00001b for "last descriptor" */
	uint32_t size;
	uint32_t opts1;
	uint32_t opts2;
	uint32_t pid_channel;
	uint32_t reserved;
};

#ifdef CONFIG_BRCMSTB_XPT_HASH
int memdma_prepare_descs(struct mcpb_dma_desc *descs, dma_addr_t descs_pa,
		struct dma_region *regions, int numregions, bool channel_A);

int memdma_run(dma_addr_t desc1, dma_addr_t desc2, bool dual_channel);

void get_hash(uint32_t *hash, bool dual_channel);
#else
static inline int memdma_prepare_descs(struct mcpb_dma_desc *descs,
		dma_addr_t descs_pa, struct dma_region *regions,
		int numregions, bool channel_A)
{
	return -ENOSYS;
}

static int memdma_run(dma_addr_t desc1, dma_addr_t desc2, bool dual_channel)
{
	return -ENOSYS;
}

static inline void get_hash(uint32_t *hash, bool dual_channel)
{
}
#endif /* CONFIG_BRCMSTB_XPT_HASH */

#endif /* __XPT_DMA_H__ */
