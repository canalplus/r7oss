/*
 * arch/sh/mm/pmb.c
 *
 * Privileged Space Mapping Buffer (PMB) Support.
 *
 * Copyright (C) 2005, 2006, 2007 Paul Mundt
 *
 * P1/P2 Section mapping definitions from map32.h, which was:
 *
 *	Copyright 2003 (c) Lineo Solutions,Inc.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 */
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/sysdev.h>
#include <linux/cpu.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/bitops.h>
#include <linux/debugfs.h>
#include <linux/fs.h>
#include <linux/seq_file.h>
#include <linux/err.h>
#include <linux/pm.h>
#include <asm/system.h>
#include <asm/uaccess.h>
#include <asm/pgtable.h>
#include <asm/mmu.h>
#include <asm/io.h>
#include <asm/mmu_context.h>
#include <asm/cacheflush.h>

#if 0
#define DPRINTK(fmt, args...) printk(KERN_ERR "%s: " fmt, __FUNCTION__, ## args)
#else
#define DPRINTK(fmt, args...) do { ; } while (0)
#endif


#define NR_PMB_ENTRIES	16
#define MIN_PMB_MAPPING_SIZE	(8*1024*1024)

struct pmb_entry {
	unsigned long vpn;
	unsigned long ppn;
	unsigned long flags;	/* Only size */
	struct pmb_entry *next;
	unsigned long size;
	int pos;
};

struct pmb_mapping {
	unsigned long phys;
	unsigned long virt;
	unsigned long size;
	unsigned long flags;	/* Only cache etc */
	struct pmb_entry *entries;
	struct pmb_mapping *next;
	int usage;
};

static DEFINE_RWLOCK(pmb_lock);
static unsigned long pmb_map;
static struct pmb_entry   pmbe[NR_PMB_ENTRIES] __attribute__ ((__section__ (".uncached.data")));
static struct pmb_mapping pmbm[NR_PMB_ENTRIES];
static struct pmb_mapping *pmb_mappings, *pmb_mappings_free;

static __always_inline unsigned long mk_pmb_entry(unsigned int entry)
{
	return (entry & PMB_E_MASK) << PMB_E_SHIFT;
}

static __always_inline unsigned long mk_pmb_addr(unsigned int entry)
{
	return mk_pmb_entry(entry) | PMB_ADDR;
}

static __always_inline unsigned long mk_pmb_data(unsigned int entry)
{
	return mk_pmb_entry(entry) | PMB_DATA;
}

static __always_inline void __set_pmb_entry(unsigned long vpn,
	unsigned long ppn, unsigned long flags, int pos)
{
#ifdef CONFIG_CACHE_WRITETHROUGH
	/*
	 * When we are in 32-bit address extended mode, CCR.CB becomes
	 * invalid, so care must be taken to manually adjust cacheable
	 * translations.
	 */
	if (likely(flags & PMB_C))
		flags |= PMB_WT;
#endif
	ctrl_outl(vpn | PMB_V, mk_pmb_addr(pos));
	ctrl_outl(ppn | flags | PMB_V, mk_pmb_data(pos));
}

static void __uses_jump_to_uncached set_pmb_entry(unsigned long vpn,
	unsigned long ppn, unsigned long flags, int pos)
{
	jump_to_uncached();
	__set_pmb_entry(vpn, ppn, flags, pos);
	back_to_cached();
}

static __always_inline void __clear_pmb_entry(int pos)
{
	ctrl_outl(0, mk_pmb_addr(pos));
}

static void __uses_jump_to_uncached clear_pmb_entry(int pos)
{
	jump_to_uncached();
	__clear_pmb_entry(pos);
	back_to_cached();
}

static int pmb_alloc(int pos)
{
	if (unlikely(pos == PMB_NO_ENTRY))
		pos = find_first_zero_bit(&pmb_map, NR_PMB_ENTRIES);

repeat:
	if (unlikely(pos > NR_PMB_ENTRIES))
		return PMB_NO_ENTRY;

	if (test_and_set_bit(pos, &pmb_map)) {
		pos = find_first_zero_bit(&pmb_map, NR_PMB_ENTRIES);
		goto repeat;
	}

	return pos;
}

static void pmb_free(int entry)
{
	clear_bit(entry, &pmb_map);
}

static struct pmb_mapping* pmb_mapping_alloc(void)
{
	struct pmb_mapping *mapping;

	if (pmb_mappings_free == NULL)
		return NULL;

	mapping = pmb_mappings_free;
	pmb_mappings_free = mapping->next;

	memset(mapping, 0, sizeof(*mapping));
        /* Set the first reference */
        mapping->usage = 1;

	return mapping;
}

static void pmb_mapping_free(struct pmb_mapping* mapping)
{
	mapping->next = pmb_mappings_free;
	pmb_mappings_free = mapping;
}

static __always_inline void __pmb_mapping_set(struct pmb_mapping *mapping)
{
	struct pmb_entry *entry = mapping->entries;

	do {
		__set_pmb_entry(entry->vpn, entry->ppn,
			      entry->flags | mapping->flags, entry->pos);
		entry = entry->next;
	} while (entry);
}

static void pmb_mapping_set(struct pmb_mapping *mapping)
{
	struct pmb_entry *entry = mapping->entries;

	do {
		set_pmb_entry(entry->vpn, entry->ppn,
			      entry->flags | mapping->flags, entry->pos);
		entry = entry->next;
	} while (entry);
}

static void pmb_mapping_clear_and_free(struct pmb_mapping *mapping)
{
	struct pmb_entry *entry = mapping->entries;

	do {
		clear_pmb_entry(entry->pos);
		pmb_free(entry->pos);
		entry = entry->next;
	} while (entry);
}

static struct {
	unsigned long size;
	int flag;
} pmb_sizes[] = {
	{ .size = 0x01000000, .flag = PMB_SZ_16M,  },
	{ .size = 0x04000000, .flag = PMB_SZ_64M,  },
	{ .size = 0x08000000, .flag = PMB_SZ_128M, },
	{ .size	= 0x20000000, .flag = PMB_SZ_512M, },
};

static struct pmb_mapping* pmb_calc(unsigned long phys, unsigned long size,
				    unsigned long req_virt, int *req_pos,
				    unsigned long pmb_flags)
{
	struct pmb_mapping *new_mapping;
	unsigned long alignment = 0;
	unsigned long virt_offset = 0;
	struct pmb_entry *prev_entry = NULL;
	unsigned long prev_end, next_start;
	struct pmb_mapping *next_mapping;
	struct pmb_mapping **prev_ptr;
	struct pmb_entry *entry;
	unsigned long start;

	new_mapping = pmb_mapping_alloc();
	if (!new_mapping)
		return NULL;

	DPRINTK("request: phys %08lx, size %08lx\n", phys, size);
	/* First work out the PMB entries to tile the physical region */
	while (size > 0) {
		int pos;
		unsigned long best_size;	/* bytes of size covered by tile */
		int best_i;
		unsigned long entry_phys;
		unsigned long entry_size;	/* total size of tile */
		int i;

		if (req_pos)
			pos = pmb_alloc(*req_pos++);
		else
			pos = pmb_alloc(PMB_NO_ENTRY);
		if (pos == PMB_NO_ENTRY)
			goto failed;

		best_size = best_i = 0;
		for (i = 0; i < ARRAY_SIZE(pmb_sizes); i++) {
			unsigned long tmp_start, tmp_end, tmp_size;
			tmp_start = phys & ~(pmb_sizes[i].size-1);
			tmp_end = tmp_start + pmb_sizes[i].size;
			tmp_size = min(phys+size, tmp_end)-max(phys, tmp_start);
			if (tmp_size > best_size) {
				best_i = i;
				best_size = tmp_size;
			}
		}

		BUG_ON(best_size == 0);

		entry_size = pmb_sizes[best_i].size;
		entry_phys = phys & ~(entry_size-1);
		DPRINTK("using PMB %d: phys %08lx, size %08lx\n", pos, entry_phys, entry_size);

		entry = &pmbe[pos];
		entry->ppn   = entry_phys;
		entry->size  = entry_size;
		entry->flags = pmb_sizes[best_i].flag;

		if (pmb_sizes[best_i].size > alignment) {
			alignment = entry_size;
			if (new_mapping->size)
				virt_offset = alignment - new_mapping->size;
		}

		if (prev_entry) {
			new_mapping->size += entry_size;
			prev_entry->next = entry;
		} else {
			new_mapping->phys = entry_phys;
			new_mapping->size = entry_size;
			new_mapping->entries = entry;
		}

		prev_entry = entry;
		size -= best_size;
		phys += best_size;
	}

	DPRINTK("mapping: phys %08lx, size %08lx\n", new_mapping->phys, new_mapping->size);
	DPRINTK("virtual alignment %08lx, offset %08lx\n", alignment, virt_offset);

	/* Do we have a conflict with the requested maping? */
	BUG_ON((req_virt & (alignment-1)) != virt_offset);

	/* Next try and find a virtual address to map this */
	prev_end = P1SEG;
	next_mapping = pmb_mappings;
	prev_ptr = &pmb_mappings;
	do {
		if (next_mapping == NULL)
			next_start = P3SEG;
		else
			next_start = next_mapping->virt;

		if (req_virt)
			start = req_virt;
		else
			start = ALIGN(prev_end, alignment) + virt_offset;

		DPRINTK("checking for virt %08lx between %08lx and %08lx\n",
			start, prev_end, next_start);

		if ((start >= prev_end) &&
		    (start + new_mapping->size <= next_start))
			break;

		if (next_mapping == NULL)
			goto failed;

		prev_ptr = &next_mapping->next;
		prev_end = next_mapping->virt + next_mapping->size;
		next_mapping = next_mapping->next;
	} while (1);

	DPRINTK("success, using %08lx\n", start);
	new_mapping->virt = start;
	new_mapping->flags = pmb_flags;
	new_mapping->next = *prev_ptr;
	*prev_ptr = new_mapping;

	/* Finally fill in the vpn's */
	for (entry = new_mapping->entries; entry; entry=entry->next) {
		entry->vpn = start;
		start += entry->size;
	}

	return new_mapping;

failed:
	/* Need to free things here */
	DPRINTK("failed\n");
	return NULL;
}

long pmb_remap(unsigned long phys,
	       unsigned long size, unsigned long flags)
{
	struct pmb_mapping *mapping;
	int pmb_flags;
	unsigned long offset;

	/* Convert typical pgprot value to the PMB equivalent */
	if (flags & _PAGE_CACHABLE) {
		if (flags & _PAGE_WT)
			pmb_flags = PMB_WT;
		else
			pmb_flags = PMB_C;
	} else
		pmb_flags = PMB_WT | PMB_UB;

	DPRINTK("phys: %08lx, size %08lx, flags %08lx->%08x\n",
		phys, size, flags, pmb_flags);

	write_lock(&pmb_lock);

	for (mapping = pmb_mappings; mapping; mapping=mapping->next) {
		DPRINTK("check against phys %08lx size %08lx flags %08lx\n",
			mapping->phys, mapping->size, mapping->flags);
		if ((phys >= mapping->phys) &&
		    (phys+size <= mapping->phys+mapping->size) &&
		    (pmb_flags == mapping->flags))
			break;
	}

	if (mapping) {
		/* If we hit an existing mapping, use it */
		mapping->usage++;
		DPRINTK("found, usage now %d\n", mapping->usage);
	} else if (size < MIN_PMB_MAPPING_SIZE) {
		/* We spit upon small mappings */
		write_unlock(&pmb_lock);
		return 0;
	} else {
		mapping = pmb_calc(phys, size, 0, NULL, pmb_flags);
		if (!mapping) {
			write_unlock(&pmb_lock);
			return 0;
		}
		pmb_mapping_set(mapping);
	}

	write_unlock(&pmb_lock);

	offset = phys - mapping->phys;
	return mapping->virt + offset;
}

static struct pmb_mapping *pmb_mapping_find(unsigned long addr,
					    struct pmb_mapping ***prev)
{
	struct pmb_mapping *mapping;
	struct pmb_mapping **prev_mapping = &pmb_mappings;

	for (mapping = pmb_mappings; mapping; mapping=mapping->next) {
		if ((addr >= mapping->virt) &&
		    (addr < mapping->virt + mapping->size))
			break;
		prev_mapping = &mapping->next;
	}

	if (prev != NULL)
		*prev = prev_mapping;

	return mapping;
}

int pmb_unmap(unsigned long addr)
{
	struct pmb_mapping *mapping;
	struct pmb_mapping **prev_mapping;

	write_lock(&pmb_lock);

	mapping = pmb_mapping_find(addr, &prev_mapping);

	if (unlikely(!mapping)) {
		write_unlock(&pmb_lock);
		return 0;
	}

	DPRINTK("mapping: phys %08lx, size %08lx, count %d\n",
		mapping->phys, mapping->size, mapping->usage);

	if (--mapping->usage == 0) {
		pmb_mapping_clear_and_free(mapping);
		*prev_mapping = mapping->next;
		pmb_mapping_free(mapping);
	}

	write_unlock(&pmb_lock);

	return 1;
}

static void noinline __uses_jump_to_uncached
apply_boot_mappings(struct pmb_mapping *uc_mapping, struct pmb_mapping *ram_mapping)
{
	extern char _start_uncached;
	register int i __asm__("r1");
	register unsigned long c2uc __asm__("r2");
	register struct pmb_entry *entry __asm__("r3");
	register unsigned long flags __asm__("r4");

	/* We can execute this directly, as the current PMB is uncached */
	__pmb_mapping_set(uc_mapping);

	cached_to_uncached = uc_mapping->virt -
		(((unsigned long)&_start_uncached) & ~(uc_mapping->entries->size-1));

	jump_to_uncached();

	/*
	 * We have to be cautious here, as we will temporarily lose access to
	 * the PMB entry which is mapping main RAM, and so loose access to
	 * data. So make sure all data is going to be in registers or the
	 * uncached region.
	 */

	c2uc = cached_to_uncached;
	entry = ram_mapping->entries;
	flags = ram_mapping->flags;

	for (i=0; i<NR_PMB_ENTRIES-1; i++)
		__clear_pmb_entry(i);

	do {
		entry = (struct pmb_entry*)(((unsigned long)entry) + c2uc);
		__set_pmb_entry(entry->vpn, entry->ppn,
				entry->flags | flags, entry->pos);
		entry = entry->next;
	} while (entry);

	/* Flush out the TLB */
	i =  ctrl_inl(MMUCR);
	i |= MMUCR_TI;
	ctrl_outl(i, MMUCR);

	back_to_cached();
}

void __init pmb_init(void)
{
	int i;
	int entry;
	extern char _start_uncached, _end_uncached;
	struct pmb_mapping *uc_mapping, *ram_mapping;

	/* Create the free list of mappings */
	pmb_mappings_free = &pmbm[0];
	for (i=0; i<NR_PMB_ENTRIES-1; i++)
		pmbm[i].next = &pmbm[i+1];
	pmbm[NR_PMB_ENTRIES-1].next = NULL;

	/* Initialise the PMB entrie's pos */
	for (i=0; i<NR_PMB_ENTRIES; i++)
		pmbe[i].pos = i;

	/* Create the initial mappings */
	entry = NR_PMB_ENTRIES-1;
	uc_mapping = pmb_calc(__pa(&_start_uncached), &_end_uncached - &_start_uncached,
		 P3SEG-0x01000000, &entry, PMB_WT | PMB_UB);
	ram_mapping = pmb_calc(__MEMORY_START, __MEMORY_SIZE, P1SEG, 0, PMB_C);
	apply_boot_mappings(uc_mapping, ram_mapping);
}

int pmb_virt_to_phys(void *addr, unsigned long *phys, unsigned long *flags)
{
	struct pmb_mapping *mapping;
	unsigned long vaddr = (unsigned long __force)addr;

	read_lock(&pmb_lock);

	mapping = pmb_mapping_find(vaddr, NULL);
	if (!mapping) {
		read_unlock(&pmb_lock);
		return EFAULT;
	}

	if (phys)
		*phys = mapping->phys + (vaddr - mapping->virt);
	if (flags)
		*flags = mapping->flags;

	read_unlock(&pmb_lock);

	return 0;
}
EXPORT_SYMBOL(pmb_virt_to_phys);

static int pmb_seq_show(struct seq_file *file, void *iter)
{
	int i;

	seq_printf(file, "V: Valid, C: Cacheable, WT: Write-Through\n"
			 "CB: Copy-Back, B: Buffered, UB: Unbuffered\n");
	seq_printf(file, "ety   vpn  ppn  size   flags\n");

	for (i = 0; i < NR_PMB_ENTRIES; i++) {
		unsigned long addr, data;
		unsigned int size;
		char *sz_str = NULL;

		addr = ctrl_inl(mk_pmb_addr(i));
		data = ctrl_inl(mk_pmb_data(i));

		size = data & PMB_SZ_MASK;
		sz_str = (size == PMB_SZ_16M)  ? " 16MB":
			 (size == PMB_SZ_64M)  ? " 64MB":
			 (size == PMB_SZ_128M) ? "128MB":
					         "512MB";

		/* 02: V 0x88 0x08 128MB C CB  B */
		seq_printf(file, "%02d: %c 0x%02lx 0x%02lx %s %c %s %s\n",
			   i, ((addr & PMB_V) && (data & PMB_V)) ? 'V' : ' ',
			   (addr >> 24) & 0xff, (data >> 24) & 0xff,
			   sz_str, (data & PMB_C) ? 'C' : ' ',
			   (data & PMB_WT) ? "WT" : "CB",
			   (data & PMB_UB) ? "UB" : " B");
	}

	return 0;
}

static int pmb_debugfs_open(struct inode *inode, struct file *file)
{
	return single_open(file, pmb_seq_show, NULL);
}

static const struct file_operations pmb_debugfs_fops = {
	.owner		= THIS_MODULE,
	.open		= pmb_debugfs_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int __init pmb_debugfs_init(void)
{
	struct dentry *dentry;

	dentry = debugfs_create_file("pmb", S_IFREG | S_IRUGO,
				     NULL, NULL, &pmb_debugfs_fops);
	if (IS_ERR(dentry))
		return PTR_ERR(dentry);

	return 0;
}
postcore_initcall(pmb_debugfs_init);

#ifdef CONFIG_PM
static int pmb_sysdev_suspend(struct sys_device *dev, pm_message_t state)
{
	static pm_message_t prev_state;
	int idx;
	switch (state.event) {
	case PM_EVENT_ON:
		/* Resumeing from hibernation */
		if (prev_state.event == PM_EVENT_FREEZE) {
			for (idx = 0; idx < NR_PMB_ENTRIES; ++idx)
				if (pmbm[idx].usage)
					pmb_mapping_set(&pmbm[idx]);
			flush_cache_all();
		}
	  break;
	case PM_EVENT_SUSPEND:
	  break;
	case PM_EVENT_FREEZE:
	  break;
	}
	prev_state = state;
	return 0;
}

static int pmb_sysdev_resume(struct sys_device *dev)
{
	return pmb_sysdev_suspend(dev, PMSG_ON);
}

static struct sysdev_driver pmb_sysdev_driver = {
	.suspend = pmb_sysdev_suspend,
	.resume = pmb_sysdev_resume,
};

static int __init pmb_sysdev_init(void)
{
	return sysdev_driver_register(&cpu_sysdev_class, &pmb_sysdev_driver);
}

subsys_initcall(pmb_sysdev_init);
#endif
