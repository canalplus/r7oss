/************************************************************************
Copyright (C) 2003-2014 STMicroelectronics. All Rights Reserved.

This file is part of the Streaming Engine.

Streaming Engine is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

Streaming Engine is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with Streaming Engine; see the file COPYING.  If not, write to the Free
Software Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307,
USA.

The Streaming Engine Library may alternatively be licensed under a
proprietary license from ST.
************************************************************************/

#ifndef H_OSDEV_CACHE
#define H_OSDEV_CACHE

#ifdef __cplusplus
#error "included from player cpp code"
#endif

/* warning suppression - use the definitions from <linux/kernel.h> */
#ifndef _LINUX_KERNEL_H
// We should only undefine these if we haven't already seen linux/kernel.h
#undef min
#undef max
#endif

#include <linux/ptrace.h>
#include <asm/cacheflush.h>
#include <linux/dma-direction.h>

static inline void OSDEV_FlushCacheAll(void)
{
	flush_cache_all();
#if defined(CONFIG_ARM) && defined(CONFIG_OUTER_CACHE)
	outer_flush_all();
#endif
}

static inline void OSDEV_FlushCacheRange(void *start, phys_addr_t start_phys, int size)
{
#if defined(CONFIG_ARM)
	//do a writeback (dmac_flush_range does writeback & invalidate!)
	dmac_map_area((const void *)start, (size_t)size, DMA_TO_DEVICE);
	outer_clean_range(start_phys, start_phys + size);
	mb();

#else
#error "Unknown architecture - Cache implementation required!"
#endif
}

static inline void OSDEV_PurgeCacheRange(void *start, phys_addr_t start_phys, int size)
{
#if defined (CONFIG_ARM)
	//dmac_flush_range does a writeback and invalidate
	dmac_flush_range(start, start + size);
	outer_flush_range(start_phys, start_phys + size);
	mb();

#else
#error "Unknown architecture - Cache implementation required!"
#endif
}

static inline void OSDEV_InvCacheRange(void *start, phys_addr_t start_phys, int size)
{
#if defined (CONFIG_ARM)
	//dmac_flush_range does a writeback and invalidate
	dmac_unmap_area((const void *)start, (size_t)size, DMA_FROM_DEVICE);
	outer_inv_range(start_phys, start_phys + size);
	mb();

#else
#error "Unknown architecture - Cache implementation required!"
#endif
}

#endif // H_OSDEV_CACHE
