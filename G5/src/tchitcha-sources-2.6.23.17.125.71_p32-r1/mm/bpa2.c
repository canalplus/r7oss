/*
 * Copyright (c) 2007 STMicroelectronics Limited
 * Author: Stuart Menefy <stuart.menefy@st.com>
 *
 * Derived from mm/bigphysarea.c which was:
 * Copyright (c) 1996 by Matt Welsh.
 * Extended by Roger Butenuth (butenuth@uni-paderborn.de), October 1997
 * Extended for linux-2.1.121 till 2.4.0 (June 2000)
 *     by Pauline Middelink <middelink@polyware.nl>
 *
 * 17th Jan 2007 Carl Shaw <carl.shaw@st.com>
 * 	Added kernel command line "bpa2parts=" parameter support.
 *
 * 9th Sep 2008 Pawel Moll <pawel.moll@st.com>
 *      Added name aliases to "bpa2parts=" syntax.
 *      Added allocation tracing features.
 *
 * 11th Apr 2010 Peter Bennett <peter.bennett@st.com>
 *      Added userspace mmap interface.
 *      Added interface to search
 *
 * This is a set of routines which allow you to reserve a large (?)
 * amount of physical memory at boot-time, which can be allocated/deallocated
 * by drivers. This memory is intended to be used for devices such as
 * video framegrabbers which need a lot of physical RAM (above the amount
 * allocated by kmalloc). This is by no means efficient or recommended;
 * to be used only in extreme circumstances.
 *
 * Partitions can be defined by a BSP (bpa2_init() shall be called somewhere
 * on setup_arch() level) or by a "bpa2parts=" kernel command line parameter:
 *
 *	bpa2parts=<partdef>[,<partdef>]
 *
 * 	<partdef> := <names>:<size>[:<base physical address>][:<flags>]
 * 	<names> := <name>[|<names>] (name and aliases separated by '|')
 * 	<name> := partition name string (20 characters max.)
 * 	<size> := standard linux memory size (e.g. 4M or 0x400000)
 * 	<base physical address> := physical address the partition should
 * 	                            start from (e.g. 32M or 0x02000000)
 *      <flags> := currently unused
 *
 * Examples:
 *
 * 	bpa2parts=audio:1M,video:10M
 *
 * 	bpa2parts=LMI_VID|video|gfx:0x03000000:0x19000000,\
 * 			LMI_SYS|audio:0x05000000:\
 * 			bigphyarea:5M
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#include <linux/ptrace.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/pci.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/bootmem.h>
#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/pfn.h>
#include <linux/bpa2.h>
#include <linux/uaccess.h>
#include <linux/cdev.h>



#define BPA2_MAX_NAME_LEN 20
#define BPA2_RES_PREFIX "bpa2:"
#define BPA2_RES_PREFIX_LEN 5

#define BPA2_MAX_MINORS 1


struct bpa2_range {
	struct bpa2_range *next;
	unsigned long base; /* base of allocated block */
	unsigned long size; /* size in bytes */
#if defined(CONFIG_BPA2_ALLOC_TRACE)
	const char *trace_file;
	int trace_line;
#endif
};

struct bpa2_part {
	struct resource res;
	struct bpa2_range initial_free_list;
	struct bpa2_range *free_list;
	struct bpa2_range *used_list;
	int flags;
	int low_mem;
	struct list_head list;
	int names_cnt;
	/* Do not separate two following fields! */
	char res_name_prefix[BPA2_RES_PREFIX_LEN]; /* resource name prefix */
	char names[1]; /* will be expanded during allocation */
};

struct bpa2_info {
	struct bpa2_part *part;
	off_t             physical_base;
	size_t            size;
	char              flags;
	atomic_t          usage_count;
};



static LIST_HEAD(bpa2_parts);
static struct bpa2_part *bpa2_bigphysarea_part;
static DEFINE_SPINLOCK(bpa2_lock);

static struct cdev          bpa2_char_device;


/* Names form one looong string of fixed-size slots */
static int bpa2_names_size(int names_cnt)
{
	return names_cnt * (BPA2_MAX_NAME_LEN + 1);
}

static const char *bpa2_get_name(struct bpa2_part *part, int n)
{
	BUG_ON(n >= part->names_cnt);

	return part->names + n * (BPA2_MAX_NAME_LEN + 1);
}

static void bpa2_set_name(struct bpa2_part *part, int n, const char *name)
{
	BUG_ON(n >= part->names_cnt);

	strlcpy(part->names + n * (BPA2_MAX_NAME_LEN + 1), name,
			BPA2_MAX_NAME_LEN + 1);
}

static int bpa2_check_name(struct bpa2_part *part, const char *name)
{
	int i;

	for (i = 0; i < part->names_cnt; i++)
		if (strcmp(name, bpa2_get_name(part, i)) == 0)
			return 0;

	return -1;
}

struct bpa2_range *bpa2_check_address(struct bpa2_part *part,
				      unsigned long address)
{
	struct bpa2_range *range;

	for (range = part->used_list; range; range = range->next)
		if (address >= range->base &&
		    address < (range->base + range->size))
			return range;

	return NULL;
}

static int __init bpa2_alloc_low(struct bpa2_part *part, unsigned long size,
		unsigned long *start)
{
	void *addr = alloc_bootmem_low_pages(size);

	if (addr == NULL) {
		printk(KERN_ERR "bpa2: could not allocate low memory\n");
		return -ENOMEM;
	}

	part->res.start = virt_to_phys(addr);
	part->res.end = virt_to_phys(addr) + size - 1;
	part->low_mem = 1;

	if (start)
		*start = part->res.start;

	return 0;
}

static int __init bpa2_reserve_low(struct bpa2_part *part, unsigned long start,
		unsigned long size)
{
	void *addr;

	/* Can't use reserve_bootmem() because there is no return code to
	 * indicate success or failure. So use __alloc_bootmem_core(),
	 * specifying a goal, which must be available. */
	addr = __alloc_bootmem_core(NODE_DATA(0)->bdata, size, PAGE_SIZE,
			start, 0);

	if (addr != phys_to_virt(start)) {
		printk(KERN_ERR "bpa2: could not allocate boot memory\n");
		if (addr)
			free_bootmem((unsigned long)addr, size);
		return -ENOMEM;
	}

	part->res.start = start;
	part->res.end = start + size - 1;
	part->low_mem = 1;

	return 0;
}

static int __init bpa2_init_high(struct bpa2_part *part, unsigned long start,
		unsigned long size)
{
	part->res.start = start;
	part->res.end = start + size - 1;
	part->low_mem = 0;

	return 0;
}

static int __init bpa2_add_part(const char **names, int names_cnt,
		unsigned long start, unsigned long size, unsigned long flags)
{
	int result;
	struct bpa2_part *part;
	unsigned long start_pfn, end_pfn;
	int i;

	part = alloc_bootmem(sizeof(*part) + bpa2_names_size(names_cnt) - 1);
	if (!part) {
		printk(KERN_ERR "bpa2: can't allocate '%s' partition "
				"structure\n", *names);
		result = -ENOMEM;
		goto fail;
	}

	if (start != PAGE_ALIGN(start)) {
		printk(KERN_WARNING "bpa2: '%s' partition start address not "
				"page aligned - fixed\n", *names);
		start = PAGE_ALIGN(start);
	}

	if (size != PAGE_ALIGN(size)) {
		printk(KERN_WARNING "bpa2: '%s' partition size not page "
				"aligned - fixed\n", *names);
		size = PAGE_ALIGN(size);
	}

	part->flags = flags;
	part->names_cnt = names_cnt;

	for (i = 0; i < names_cnt; i++)
		bpa2_set_name(part, i, names[i]);

	memcpy(part->res_name_prefix, BPA2_RES_PREFIX, BPA2_RES_PREFIX_LEN);
	part->res.name = part->res_name_prefix; /* merged with the first name */
	part->res.flags = IORESOURCE_BUSY | IORESOURCE_MEM;

	/* Allocate/reserve/initialize requested memory area */
	start_pfn = PFN_DOWN(start);
	end_pfn = PFN_DOWN(start + size);
	if (start == 0) {
		result = bpa2_alloc_low(part, size, &start);
	} else if ((start_pfn >= min_low_pfn) && (end_pfn <= max_low_pfn)) {
		result = bpa2_reserve_low(part, start, size);
	} else if ((start_pfn > max_low_pfn) || (end_pfn < min_low_pfn)) {
		result = bpa2_init_high(part, start, size);
	} else {
		printk(KERN_ERR "bpa2: partition spans low memory boundary\n");
		result = -EFAULT;
	}
	if (result != 0) {
		printk(KERN_ERR "bpa2: failed to create '%s' partition\n",
				*names);
		goto fail;
	}

	/* Declare the resource */
	result = insert_resource(&iomem_resource, &part->res);
	if (result != 0) {
		printk(KERN_ERR "bpa2: could not reserve '%s' partition "
				"resource\n", *names);
		goto fail;
	}

	/* Initialize ranges */
	part->initial_free_list.next = NULL;
	part->initial_free_list.base = start;
	part->initial_free_list.size = size;
	part->free_list = &part->initial_free_list;
	part->used_list = NULL;

	/* And finally... */
	list_add_tail(&part->list, &bpa2_parts);
	printk(KERN_INFO "bpa2: partition '%s' created at 0x%08lx, size %ld kB"
			" (0x%08lx B)\n", *names, start, size / 1024, size);

	/* Assign the legacy partition pointer, if that's the one */
	if (bpa2_bigphysarea_part == NULL &&
			bpa2_check_name(part, "bigphysarea") == 0) {
		if (part->low_mem)
			bpa2_bigphysarea_part = part;
		else
			printk(KERN_ERR "bpa2: bigphysarea ('%s') not in "
					"logical memory\n", *names);
	}

	return 0;

fail:
	if (part)
		free_bootmem(virt_to_phys(part), sizeof(*part) +
				bpa2_names_size(names_cnt) - 1);
	return result;
}

/**
 * bpa2_init - initialize bpa2 partitions
 * @partdescs: description of the partitions
 * @nparts: number of partitions
 *
 * This function initialises the bpa2 internal data structures
 * based on the partition descriptions which are passed in.
 *
 * This must be called from early in the platform initialisation
 * sequence, while bootmem is still active.
 */
void __init bpa2_init(struct bpa2_partition_desc *partdescs, int nparts)
{
	for (; nparts; nparts--, partdescs++) {
		int names_cnt = 1;
		const char **names;
		int i;

		if (!partdescs->name || !*partdescs->name) {
			printk(KERN_ERR "bpa2: no partition name given!\n");
			continue;
		}

		/* Count aliases given in description */
		names = partdescs->aka;
		while (names && *names) {
			names_cnt++;
			names++;
		}

		/* Create names pointer array */
		names = alloc_bootmem(sizeof(*names) * names_cnt);
		names[0] = partdescs->name;
		for (i = 1; i < names_cnt; i++)
			names[i] = partdescs->aka[i - 1];

		/* Finally create the partition */
		if (bpa2_add_part(names, names_cnt, partdescs->start,
				partdescs->size, partdescs->flags) != 0)
			printk(KERN_ERR "bpa2: '%s' partition skipped\n",
					*names);

		free_bootmem(virt_to_phys(names), sizeof(*names) * names_cnt);
	}
}

/*
 * Create legacy "bigphysarea" partition (if kindly asked ;-)
 */
static int __init bpa2_bigphys_setup(char *str)
{
	const char *name = "bigphysarea";
	int pages;

	if (get_option(&str, &pages) != 1) {
		printk(KERN_ERR "bpa2: wrong 'bigphysarea' parameter\n");
		return -EINVAL;
	}

	bpa2_add_part(&name, 1, 0, pages << PAGE_SHIFT, BPA2_NORMAL);

	return 1;
}
__setup("bigphysarea=", bpa2_bigphys_setup);

/*
 * Create "bpa2parts"-defined partitions
 */
static int __init bpa2_parts_setup(char *str)
{
	char *desc;

	if (!str || !*str)
		return -EINVAL;

	while ((desc = strsep(&str, ",")) != NULL) {
		unsigned long start = 0;
		unsigned long size = 0;
		int names_cnt = 1;
		const char **names;
		char *token;
		int i;

		/* Get '|'-separated partition names token */
		token = strsep(&desc, ":");
		if (!token || !*token) {
			printk(KERN_ERR "bpa2: partition name(s) not given!\n");
			continue;
		}

		/* Check how many names we have... */
		for (i = 0; token[i]; i++)
			if (token[i] == '|')
				names_cnt++;

		/* Separate names & create pointers table */
		names = alloc_bootmem(sizeof(*names) * names_cnt);
		for (i = 0; i < names_cnt; i++)
			names[i] = strsep(&token, "|");

		/* Get partition size */
		token = strsep(&desc, ":");
		if (token) {
			size = memparse(token, &token);
			if (*token)
				size = 0;
		}
		if (size == 0) {
			printk(KERN_ERR "bpa2: partition size not given\n");
			free_bootmem(virt_to_phys(names),
					sizeof(*names) * names_cnt);
			continue;
		}

		/* Get partition start address (optional) */
		token = strsep(&desc, ":");
		if (token && *token) {
			start = memparse(token, &token);
			if (*token)
				start = 0;
			if (start == 0) {
				printk(KERN_ERR "bpa2: Invalid base "
						"address!\n");
				free_bootmem(virt_to_phys(names),
						sizeof(*names) * names_cnt);
				continue;
			}
		}

		/* Get partition flags (not implemented yet) */

		/* Finally add it to the list... */
		if (bpa2_add_part(names, names_cnt, start, size,
					BPA2_NORMAL) != 0)
			printk(KERN_ERR "bpa2: '%s' partition skipped\n",
					*names);

		free_bootmem(virt_to_phys(names), sizeof(*names) * names_cnt);
	}

	return 1;
}
__setup("bpa2parts=", bpa2_parts_setup);



/**
 * bpa2_find_part - find a bpa2 partition based on its name
 * @name: name of the partition to find
 *
 * Return the bpa2 partition corresponding to the requested name.
 */
struct bpa2_part *bpa2_find_part(const char *name)
{
	struct bpa2_part *part;

	list_for_each_entry(part, &bpa2_parts, list)
		if (bpa2_check_name(part, name) == 0)
			return part;

	return NULL;
}
EXPORT_SYMBOL(bpa2_find_part);

/**
 * bpa2_find_range - find a bpa2 partition based on its physical
 * address.
 * @address: address range to find.
 * @range:   pointer to the range information.
 *
 * Return the bpa2 partition corresponding to the physical address
 */
struct bpa2_part *bpa2_find_range(unsigned long address,
				  struct bpa2_range **range)
{
	struct bpa2_part  *part;
	struct bpa2_range *the_range;

	list_for_each_entry(part, &bpa2_parts, list) {
		the_range = bpa2_check_address(part, address);
		if (the_range) {
			if (range) *range = the_range;
			return part;
		}
	}

	return NULL;
}
EXPORT_SYMBOL(bpa2_find_range);

/**
 * bpa2_low_part - return whether a partition resides in low memory
 * @part: partition to query
 *
 * Return whether the specified partition resides in low (that is,
 * kernel logical) memory. If it does, then functions such as
 * phys_to_virt() can be used to convert the allocated memory into
 * a virtual address which can be directly dereferenced.
 *
 * If this is not true, then the region will not be mapped into
 * the kernel's address space, and so if access is required it will
 * need to be mapped using ioremap() and accessed using readl() etc.
 */
int bpa2_low_part(struct bpa2_part *part)
{
	return part->low_mem;
}
EXPORT_SYMBOL(bpa2_low_part);



/**
 * __bpa2_alloc_pages - allocate pages from a bpa2 partition
 * @part: partition to allocate from
 * @count: number of pages to allocate
 * @align: required alignment
 * @priority: GFP_* flags to use
 *
 * Allocate `count' pages from the partition. Pages are aligned to
 * a multiple of `align'. `priority' has the same meaning in kmalloc, and
 * is used for partition management information, it does not influence the
 * memory returned.
 *
 * This function may not be called from an interrupt.
 */
unsigned long __bpa2_alloc_pages(struct bpa2_part *part, int count, int align,
		int priority, const char *trace_file, int trace_line)
{
	struct bpa2_range *range, **range_ptr;
	struct bpa2_range *new_range, *align_range, *used_range;
	unsigned long aligned_base = 0;
	unsigned long result = 0;

	if (count == 0)
		return 0;

	/* Allocate the data structures we might need here so that we
	 * don't have problems inside the spinlock.
	 * Free at the end if not used. */
	new_range = kmalloc(sizeof(*new_range), priority);
	align_range = kmalloc(sizeof(*align_range), priority);
	if ((new_range == NULL) || (align_range == NULL))
		goto fail;

	if (align == 0)
		align = PAGE_SIZE;
	else
		align = align * PAGE_SIZE;

	spin_lock(&bpa2_lock);

	/* Search a free block which is large enough, even with alignment. */
	range_ptr = &part->free_list;
	while (*range_ptr != NULL) {
		range = *range_ptr;
		aligned_base = ((range->base + align - 1) / align) * align;
		if (aligned_base + count * PAGE_SIZE <=
				range->base + range->size)
			break;
		range_ptr = &range->next;
	}
	if (*range_ptr == NULL)
		goto fail_unlock;
	range = *range_ptr;

	/* When we have to align, the pages needed for alignment can
	 * be put back to the free pool. */
	if (aligned_base != range->base) {
		align_range->base = range->base;
		align_range->size = aligned_base - range->base;
		range->base = aligned_base;
		range->size -= align_range->size;
		align_range->next = range;
		*range_ptr = align_range;
		range_ptr = &align_range->next;
		align_range = NULL;
	}

	if (count * PAGE_SIZE < range->size) {
		/* Range is larger than needed, create a new list element for
		 * the used list and shrink the element in the free list. */
		new_range->base = range->base;
		new_range->size = count * PAGE_SIZE;
		range->base = new_range->base + new_range->size;
		range->size = range->size - new_range->size;
		used_range = new_range;
		new_range = NULL;
	} else {
		/* Range fits perfectly, remove it from free list. */
		*range_ptr = range->next;
		used_range = range;
	}
#if defined(CONFIG_BPA2_ALLOC_TRACE)
	/* Save the caller data */
	used_range->trace_file = trace_file;
	used_range->trace_line = trace_line;
#endif
	/* Insert block into used list */
	used_range->next = part->used_list;
	part->used_list = used_range;
	result = used_range->base;

fail_unlock:
	spin_unlock(&bpa2_lock);
fail:
	if (new_range)
		kfree(new_range);
	if (align_range)
		kfree(align_range);

	return result;
}
EXPORT_SYMBOL(__bpa2_alloc_pages);

/**
 * bpa2_free_pages - free pages allocated from a bpa2 partition
 * @part: partition to free pages back to
 * @base:
 * @align: required alinment
 * @priority: GFP_* flags to use
 *
 * Free pages allocated with `bigphysarea_alloc_pages'. `base' must be an
 * address returned by `bigphysarea_alloc_pages'.
 * This function my not be called from an interrupt!
 */
void bpa2_free_pages(struct bpa2_part *part, unsigned long base)
{
	struct bpa2_range *prev, *next, *range, **range_ptr;

	spin_lock(&bpa2_lock);

	/* Search the block in the used list. */
	for (range_ptr = &part->used_list;
			*range_ptr != NULL;
			range_ptr = &(*range_ptr)->next)
		if ((*range_ptr)->base == base)
			break;
	if (*range_ptr == NULL) {
		printk(KERN_ERR "%s: 0x%08lx not allocated!\n",
				__func__, base);
		spin_unlock(&bpa2_lock);
		return;
	}
	range = *range_ptr;

	/* Remove range from the used list: */
	*range_ptr = (*range_ptr)->next;

	/* The free-list is sorted by address, search insertion point
	 * and insert block in free list. */
	for (range_ptr = &part->free_list, prev = NULL;
			*range_ptr != NULL;
			prev = *range_ptr, range_ptr = &(*range_ptr)->next)
		if ((*range_ptr)->base >= base)
			break;
	range->next  = *range_ptr;
	*range_ptr   = range;

	/* Concatenate free range with neighbors, if possible.
	 * Try for upper neighbor (next in list) first, then
	 * for lower neighbor (predecessor in list). */
	next = NULL;
	if (range->next != NULL &&
			range->base + range->size == range->next->base) {
		next = range->next;
		range->size += next->size;
		range->next = next->next;
	}
	if (prev != NULL &&
			prev->base + prev->size == range->base) {
		prev->size += range->size;
		prev->next = range->next;
	} else {
		range = NULL;
	}

	spin_unlock(&bpa2_lock);

	if (next && (next != &part->initial_free_list))
		kfree(next);
	if (range && (range != &part->initial_free_list))
		kfree(range);
}
EXPORT_SYMBOL(bpa2_free_pages);



caddr_t	__bigphysarea_alloc_pages(int count, int align, int priority,
		const char *trace_file, int trace_line)
{
	unsigned long addr;

	if (!bpa2_bigphysarea_part)
		return NULL;

	addr = __bpa2_alloc_pages(bpa2_bigphysarea_part, count,
			align, priority, trace_file, trace_line);

	if (addr == 0)
		return NULL;

	return phys_to_virt(addr);
}
EXPORT_SYMBOL(__bigphysarea_alloc_pages);

void bigphysarea_free_pages(caddr_t mapped_addr)
{
	unsigned long addr = virt_to_phys(mapped_addr);

	bpa2_free_pages(bpa2_bigphysarea_part, addr);
}
EXPORT_SYMBOL(bigphysarea_free_pages);



#ifdef CONFIG_PROC_FS

static void *bpa2_seq_start(struct seq_file *s, loff_t *pos)
{
	struct list_head *node;
	loff_t i;

	spin_lock(&bpa2_lock);

	for (i = 0, node = bpa2_parts.next;
			i < *pos && node != &bpa2_parts;
			i++, node = node->next)
		;

	if (node == &bpa2_parts)
		return NULL;

	return node;
}

static void bpa2_seq_stop(struct seq_file *s, void *v)
{
	spin_unlock(&bpa2_lock);
}

static void *bpa2_seq_next(struct seq_file *s, void *v, loff_t *pos)
{
	struct list_head *node = v;

	(*pos)++;
	node = node->next;

	if (node == &bpa2_parts)
		return NULL;

	seq_printf(s, "\n");

	return node;
}

static int bpa2_seq_show(struct seq_file *s, void *v)
{
	struct bpa2_part *part = list_entry(v, struct bpa2_part, list);
	struct bpa2_range *range;
	int free_count, free_total, free_max;
	int used_count, used_total, used_max;
	int i;

	free_count = 0;
	free_total = 0;
	free_max = 0;
	for (range = part->free_list; range != NULL; range = range->next) {
		free_count++;
		free_total += range->size;
		if (range->size > free_max)
			free_max = range->size;
	}

	used_count = 0;
	used_total = 0;
	used_max = 0;
	for (range = part->used_list; range != NULL; range = range->next) {
		used_count++;
		used_total += range->size;
		if (range->size > used_max)
			used_max = range->size;
	}

	seq_printf(s, "Partition: ");
	for (i = 0; i < part->names_cnt; i++)
		seq_printf(s, "%s'%s'", i > 0 ? " aka " : "",
				bpa2_get_name(part, i));
	seq_printf(s, "\n");
	seq_printf(s, "Size: %d kB, base address: 0x%08x\n",
			(part->res.end - part->res.start + 1) / 1024,
			part->res.start);
	seq_printf(s, "Statistics:                  free       "
			"    used\n");
	seq_printf(s, "- number of blocks:      %8d       %8d\n",
			free_count, used_count);
	seq_printf(s, "- size of largest block: %8d kB    %8d kB\n",
			free_max / 1024, used_max / 1024);
	seq_printf(s, "- total:                 %8d kB    %8d kB\n",
			free_total / 1024, used_total / 1024);

	if (used_count) {
		seq_printf(s, "Allocations:\n");
		for (range = part->used_list; range != NULL;
				range = range->next) {
			seq_printf(s, "- %lu B at 0x%.8lx",
					range->size, range->base);
#if defined(CONFIG_BPA2_ALLOC_TRACE)
			if (range->trace_file)
				seq_printf(s, " (%s:%d)", range->trace_file,
						range->trace_line);
#endif
			seq_printf(s, "\n");
		}
	}

	return 0;
}

static struct seq_operations bpa2_seq_ops = {
	.start = bpa2_seq_start,
	.next = bpa2_seq_next,
	.stop = bpa2_seq_stop,
	.show = bpa2_seq_show,
};

static int bpa2_proc_open(struct inode *inode, struct file *file)
{
	return seq_open(file, &bpa2_seq_ops);
}

static struct file_operations bpa2_proc_ops = {
	.owner = THIS_MODULE,
	.open = bpa2_proc_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = seq_release,
};

/* Called from late in the kernel initialisation sequence, once the
 * normal memory allocator is available. */
static int __init bpa2_proc_init(void)
{
	struct proc_dir_entry *entry = create_proc_entry("bpa2", 0, NULL);

	if (entry)
		entry->proc_fops = &bpa2_proc_ops;

	return 0;
}
__initcall(bpa2_proc_init);

#endif /* CONFIG_PROC_FS */

void bpa2_memory(struct bpa2_part *part, unsigned long *base,
		 unsigned long *size)
{
	if (base)
		*base = part?
			(unsigned long)phys_to_virt(part->res.start)
			: 0;
	if (size)
		*size = part?
			part->res.end - part->res.start + 1
			: 0;
}
EXPORT_SYMBOL(bpa2_memory);

void bigphysarea_memory(unsigned long *base, unsigned long *size)
{
	bpa2_memory(bpa2_bigphysarea_part, base, size);
}
EXPORT_SYMBOL(bigphysarea_memory);

static void bpa2_vm_open(struct vm_area_struct *vma)
{
	struct bpa2_info *info = (struct bpa2_info *)vma->vm_private_data;

	atomic_inc(&info->usage_count);
}

static void bpa2_vm_close(struct vm_area_struct *vma)
{
	struct bpa2_info *info = (struct bpa2_info *)vma->vm_private_data;

	atomic_dec(&info->usage_count);
}

static struct vm_operations_struct bpa2_vm_ops_memory = {
	.open     = bpa2_vm_open,
	.close    = bpa2_vm_close,
};

static int bpa2_open(struct inode *inode, struct file *file)
{
	struct bpa2_info *info;

	info = kzalloc(sizeof(info), GFP_KERNEL);

	if (!info)
		return -ENOMEM;

	file->private_data = info;
	atomic_set(&info->usage_count, 0);

	return 0;
}

static int bpa2_ioctl(struct inode *inode,
		      struct file *file,
		      unsigned int cmd,
		      unsigned long parg)
{
	struct bpa2_info *info = (struct bpa2_info *)file->private_data;

	switch (cmd) {
	case BPA2_ALLOC:
	{
		struct bpa2_alloc alloc;

		if (copy_from_user(&alloc, parg, sizeof(alloc)))
			return -EFAULT;

		info->part = bpa2_find_part(alloc.name);

		if (!info->part)
			return -EINVAL;

		info->size = (alloc.size + (PAGE_SIZE-1)) / PAGE_SIZE;

		info->physical_base =
			bpa2_alloc_pages(info->part, info->size, 0, 0);

		if (!info->physical_base)
			return -ENOMEM;

		info->flags = alloc.flags;

		return 0;
	}
	case BPA2_FREE:
		if (!info->part || !info->physical_base)
			return -EINVAL;

		if (atomic_read(&info->usage_count))
			return -EBUSY;

		bpa2_free_pages(info->part, info->physical_base);
		info->part          = NULL;
		info->physical_base = 0;

		return 0;
	};

  return -ENOIOCTLCMD;
}

static int bpa2_close(struct inode *inode, struct file *file)
{
	struct bpa2_info *info = (struct bpa2_info *)file->private_data;

	if (info->part && info->physical_base)
		bpa2_free_pages(info->part, info->physical_base);

	if (info) kfree(info);

	return 0;
}

static int bpa2_mmap(struct file *file,
		     struct vm_area_struct *vma)
{
	struct bpa2_info *info = (struct bpa2_info *)file->private_data;

	if ((info->size << PAGE_SHIFT) != (vma->vm_end - vma->vm_start))
		return -EINVAL;

	if (!(vma->vm_flags & VM_SHARED))
		return -EINVAL;

	vma->vm_flags       |= VM_RESERVED | VM_DONTEXPAND | VM_LOCKED;

	/* If it's not below high_memory it's IO memory. */
	if (info->physical_base >= __pa(high_memory))
		vma->vm_flags       |= VM_IO;
	else
		vma->vm_flags       &= ~VM_IO;

	vma->vm_flags       |= VM_IO;

	if (info->flags & BPA2_FLAGS_UNCACHED)
		vma->vm_page_prot    = pgprot_noncached(vma->vm_page_prot);

	vma->vm_private_data = info;

	if (remap_pfn_range(vma,
			    vma->vm_start,
			    info->physical_base >> PAGE_SHIFT,
			    info->size << PAGE_SHIFT,
			    vma->vm_page_prot) < 0)
		return -EIO;

	vma->vm_ops = &bpa2_vm_ops_memory;
	bpa2_vm_open(vma);

	return 0;
}

static struct file_operations bpa2_fops = {
	.owner   = THIS_MODULE,
	.open    = bpa2_open,
	.release = bpa2_close,
	.mmap    = bpa2_mmap,
	.ioctl   = bpa2_ioctl
};

static int __init bpa2_char_init(void)
{
	struct class        *bpa2_class;
	struct class_device *bpa2_class_dev;
	dev_t dev;
	int   err;

	err = alloc_chrdev_region(&dev, 0, BPA2_MAX_MINORS, "bpa2");
	if (err) return err;

	cdev_init(&bpa2_char_device, &bpa2_fops);
	err = cdev_add(&bpa2_char_device, dev, BPA2_MAX_MINORS);

	if (err) {
		unregister_chrdev_region(dev, BPA2_MAX_MINORS);
		return err;
	}

	bpa2_class     = class_create(THIS_MODULE, "bpa2");
	bpa2_class_dev = class_device_create(bpa2_class,
					     NULL, dev,
					     NULL, "bpa2");

	return 0;
}

#ifdef CONFIG_BPA2_USERSPACE_ALLOCATION
__initcall(bpa2_char_init);
#endif
