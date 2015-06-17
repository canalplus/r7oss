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

#ifndef H_OSDEV_MEM
#define H_OSDEV_MEM

#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/mm.h>
#include <linux/semaphore.h>
#include <linux/bpa2.h>
#include <linux/io.h>
#include <linux/ptrace.h>

#ifdef __cplusplus
#error "included from player cpp code"
#endif

#if defined(SE_DEBUG_MEMORY) && (SE_DEBUG_MEMORY > 0)

enum {
	DMC_NEWK = 0,
	DMC_NEWV,
	DMC_KMEM,
	DMC_BIGPHYS,
	DMC_BPA2,
	DMC_THREAD,
	DMC_TUNE,
	DMC_LAST
};

extern struct semaphore dumpmemchk_mutex;
#define _LOCKM()   { int ret = down_interruptible(&dumpmemchk_mutex); if (ret) {}}
#define _UNLOCKM() { up(&dumpmemchk_mutex); }

extern int dmc_counts[DMC_LAST];
extern int dmc_prints[DMC_LAST];

#define _DUMPMEMCHK_UP(dmc_id, str, ptr)\
{\
    int localcounter; \
    _LOCKM(); \
    localcounter = ++dmc_counts[dmc_id]; \
    if (dmc_prints[dmc_id]) pr_info("MCHK-up %s 0x%p (%d)\n", str, ptr, localcounter); \
    _UNLOCKM(); \
}
#define _DUMPMEMCHK_DOWN(dmc_id, str, ptr)\
{\
    int localcounter; \
    _LOCKM(); \
    localcounter = --dmc_counts[dmc_id]; \
    if (dmc_prints[dmc_id]) pr_info("MCHK-down %s 0x%p (%d)\n", str, ptr, localcounter); \
    _UNLOCKM(); \
}

#else
#define _DUMPMEMCHK_UP(dmc_id, str, ptr) { }
#define _DUMPMEMCHK_DOWN(dmc_id, str, ptr) { }

#endif // SE_DEBUG_MEMORY

void OSDEV_Dump_MemCheckCounters(const char *str);

// -----------------------------------------------------------------------------------------------
//

//#define ENABLE_MALLOC_POISONING
static inline void *OSDEV_Malloc(unsigned int             Size)
{
	void *p;

	if (Size < (PAGE_SIZE * 4)) {
		p = (void *)kmalloc(Size, GFP_KERNEL);
		if (p) {
			_DUMPMEMCHK_UP(DMC_KMEM, "kmem", p);
		}
	} else {
		p = bigphysarea_alloc(Size);
		if (p) {
			_DUMPMEMCHK_UP(DMC_BIGPHYS, "bigphys", p);
		}
	}

#ifdef ENABLE_MALLOC_POISONING
	if (p) {
		memset(p, 0xca, Size);
	}
#endif

	return p;
}

static inline void *OSDEV_Zalloc(unsigned int             Size)
{
	void *p;

	if (Size < (PAGE_SIZE * 4)) {
		p = (void *)kzalloc(Size, GFP_KERNEL);
		if (p) {
			_DUMPMEMCHK_UP(DMC_KMEM, "kmem", p);
		}
	} else {
		p = bigphysarea_alloc(Size);
		if (p) {
			_DUMPMEMCHK_UP(DMC_BIGPHYS, "bigphys", p);
			memset(p, 0, Size);
		}
	}

#ifdef ENABLE_MALLOC_POISONING
	if (p) {
		memset(p, 0xca, Size);
	}
#endif

	return p;
}

static inline void OSDEV_Free(void                    *Address)
{
	unsigned long  Base, Size;
	unsigned long  Addr = (unsigned long)Address;

	if (Address == NULL) {
		// support null ptr; same as kfree does
		return;
	}

	bigphysarea_memory(&Base, &Size);
	if ((Addr >= Base) && (Addr < (Base + Size))) {
		_DUMPMEMCHK_DOWN(DMC_BIGPHYS, "bigphys", Address);
		bigphysarea_free_pages(Address);
	} else {
		_DUMPMEMCHK_DOWN(DMC_KMEM, "kmem", Address);
		kfree(Address);
	}
}

// -----------------------------------------------------------------------------------------------
//
// Partitioned versions of malloc and free
//

// ARM Integration
#if defined(CONFIG_ARM)
/*
 * ARM has mis-named this interface relative to all the other platforms
 * that define it (including x86). We are going to make life simple, given
 * that we never want to read bufferdata by using write combined.
 */
#define ioremap_cache ioremap_wc
#endif

//#define ENABLE_MALLOCPART_POISONING
static inline void *OSDEV_MallocPartitioned(char                    *Partition,
                                            unsigned int             Size)
{
	struct bpa2_part    *partition;
	unsigned int         numpages;
	unsigned long        p;

	partition   = bpa2_find_part(Partition);
	if (partition == NULL) {
		return NULL;
	}

	numpages    = (Size + PAGE_SIZE - 1) / PAGE_SIZE;
	p           = bpa2_alloc_pages(partition, numpages, 0, 0 /*priority*/);

	if (p) {
		_DUMPMEMCHK_UP(DMC_BPA2, "bpa2", ((void *)p));
#ifdef ENABLE_MALLOCPART_POISONING
		{
			void *ptr = ioremap_cached((unsigned int)p, Size);
			memset((void *)ptr, 0xca, Size);
			iounmap(ptr);
		}
#endif
	}

	return (void *)p;
}

static inline void OSDEV_FreePartitioned(char                    *Partition,
                                         void                    *Address)
{
	struct bpa2_part    *partition;

	partition = bpa2_find_part(Partition);
	if (partition == NULL) {
		pr_debug("OSDEV_FreePartitioned - Partition not found\n");
		return;
	}

	if (Address == NULL) {
		// support null ptr; same as kfree does
		return;
	}

	_DUMPMEMCHK_DOWN(DMC_BPA2, "bpa2", Address);
	bpa2_free_pages(partition, (unsigned int)Address);
}

// -----------------------------------------------------------------------------------------------

static inline void            *OSDEV_AllignedMallocHwBuffer(unsigned int             Allignment,
                                                            unsigned int             Size,
                                                            char                    *Partition,
                                                            unsigned long           *PhysicalAddress)
{
	void    *Base;
	void    *Result;
	struct bpa2_part    *partitionStruct;

	partitionStruct   = bpa2_find_part(Partition);
	if (partitionStruct == NULL) {
		pr_err("Error: %s Unknown Partition\n", __func__);
		return NULL;
	}
	//pr_info("Partition is %s\n", Partition);
	Result                      = NULL;
	Size                        = Size + sizeof(void *) + Allignment - 1;
	Base                        = OSDEV_MallocPartitioned(Partition, Size);

	if (Base != NULL) {
		*PhysicalAddress = (unsigned long)((((unsigned int)Base + sizeof(void *)) + (Allignment - 1)) & (~(Allignment - 1)));
		if (bpa2_low_part(partitionStruct)) {
			Result                  =  phys_to_virt(*PhysicalAddress);
		} else {
			Result                  = ioremap_cache(*PhysicalAddress, (unsigned long) Size);
		}
		//pr_info("Buffer virtual address in hex      = %x\n",(unsigned int)Result);
		((void **)Result)[-1]   = Base;
		//pr_info("Buffer Base in hex                 = %x\n",(unsigned int)Base);
	}

	return Result;
}

static inline void            *OSDEV_Calloc(unsigned int             Quantity,
                                            unsigned int             Size)
{
	void *Memory = OSDEV_Malloc(Quantity * Size);

	if (Memory != NULL) {
		memset(Memory, '\0', Quantity * Size);
	}

	return Memory;
}

static inline int OSDEV_AllignedFreeHwBuffer(void *Address , char *Partition)
{
	void    *Base;
	struct bpa2_part    *partitionStruct;

	partitionStruct   = bpa2_find_part(Partition);
	if (partitionStruct == NULL) {
		pr_err("Error: %s Unknown Partition\n", __func__);
		return -1;
	}

	if (Address != NULL) {
		//pr_info("Buffer virtual address to Free in hex   = %x\n",(unsigned int)Address);
		Base            = ((void **)Address)[-1];
		//pr_info("Buffer Recovered Base in hex            = %x\n",(unsigned int)Base);
		OSDEV_FreePartitioned(Partition, Base);
		if (!bpa2_low_part(partitionStruct)) {
			iounmap(Address);
		}
	}

	return 0;
}

#endif  // H_OSDEV_MEM
