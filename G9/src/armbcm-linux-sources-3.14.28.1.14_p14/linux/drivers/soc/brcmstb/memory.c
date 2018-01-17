/*
 * Copyright © 2015 Broadcom Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * A copy of the GPL is available at
 * http://www.broadcom.com/licenses/GPLv2.php or from the Free Software
 * Foundation at https://www.gnu.org/licenses/ .
 */

#include <asm/page.h>
#include <asm/setup.h>  /* for meminfo */
#include <linux/device.h>
#include <linux/io.h>
#include <linux/libfdt.h>
#include <linux/memblock.h>
#include <linux/mm.h>   /* for high_memory */
#include <linux/of_address.h>
#include <linux/of_fdt.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/vme.h>
#include <linux/brcmstb/bmem.h>
#include <linux/brcmstb/cma_driver.h>
#include <linux/brcmstb/memory_api.h>

/* -------------------- Constants -------------------- */

#define DEFAULT_LOWMEM_PCT	20  /* used if only one membank */

/* Macros to help extract property data */
#define U8TOU32(b, offs) \
	((((u32)b[0+offs] << 0)  & 0x000000ff) | \
	 (((u32)b[1+offs] << 8)  & 0x0000ff00) | \
	 (((u32)b[2+offs] << 16) & 0x00ff0000) | \
	 (((u32)b[3+offs] << 24) & 0xff000000))

#define DT_PROP_DATA_TO_U32(b, offs) (fdt32_to_cpu(U8TOU32(b, offs)))

/* Constants used when retrieving memc info */
#define NUM_BUS_RANGES 10
#define BUS_RANGE_ULIMIT_SHIFT 4
#define BUS_RANGE_LLIMIT_SHIFT 4
#define BUS_RANGE_PA_SHIFT 12

enum {
	BUSNUM_MCP0 = 0x4,
	BUSNUM_MCP1 = 0x5,
	BUSNUM_MCP2 = 0x6,
};

/* -------------------- Shared and local vars -------------------- */

const enum brcmstb_reserve_type brcmstb_default_reserve = BRCMSTB_RESERVE_BMEM;
bool brcmstb_memory_override_defaults = false;

static struct {
	struct brcmstb_range range[MAX_BRCMSTB_RESERVED_RANGE];
	int count;
} reserved_init;

/* -------------------- Functions -------------------- */

/*
 * If the DT nodes are handy, determine which MEMC holds the specified
 * physical address.
 */
int brcmstb_memory_phys_addr_to_memc(phys_addr_t pa)
{
	int memc = -1;
	int i;
	struct device_node *np;
	void __iomem *cpubiuctrl = NULL;
	void __iomem *curr;

	np = of_find_compatible_node(NULL, NULL, "brcm,brcmstb-cpu-biu-ctrl");
	if (!np)
		goto cleanup;

	cpubiuctrl = of_iomap(np, 0);
	if (!cpubiuctrl)
		goto cleanup;

	for (i = 0, curr = cpubiuctrl; i < NUM_BUS_RANGES; i++, curr += 8) {
		const u64 ulimit_raw = readl(curr);
		const u64 llimit_raw = readl(curr + 4);
		const u64 ulimit =
			((ulimit_raw >> BUS_RANGE_ULIMIT_SHIFT)
			 << BUS_RANGE_PA_SHIFT) | 0xfff;
		const u64 llimit = (llimit_raw >> BUS_RANGE_LLIMIT_SHIFT)
				   << BUS_RANGE_PA_SHIFT;
		const u32 busnum = (u32)(ulimit_raw & 0xf);

		if (pa >= llimit && pa <= ulimit) {
			if (busnum >= BUSNUM_MCP0 && busnum <= BUSNUM_MCP2) {
				memc = busnum - BUSNUM_MCP0;
				break;
			}
		}
	}

cleanup:
	if (cpubiuctrl)
		iounmap(cpubiuctrl);

	of_node_put(np);

	return memc;
}

static int populate_memc(struct brcmstb_memory *mem, int addr_cells,
		int size_cells)
{
	const void *fdt = initial_boot_params;
	const int mem_offset = fdt_path_offset(fdt, "/memory");
	const struct fdt_property *prop;
	int proplen, cellslen;
	int i;

	if (mem_offset < 0) {
		pr_err("No memory node?\n");
		return -EINVAL;
	}

	prop = fdt_get_property(fdt, mem_offset, "reg", &proplen);
	cellslen = (int)sizeof(u32) * (addr_cells + size_cells);
	if ((proplen % cellslen) != 0) {
		pr_err("Invalid length of reg prop: %d\n", proplen);
		return -EINVAL;
	}

	for (i = 0; i < proplen / cellslen; ++i) {
		u64 addr = 0;
		u64 size = 0;
		int memc_idx;
		int range_idx;
		int j;

		for (j = 0; j < addr_cells; ++j) {
			int offset = (cellslen * i) + (sizeof(u32) * j);
			addr |= (u64)DT_PROP_DATA_TO_U32(prop->data, offset) <<
				((addr_cells - j - 1) * 32);
		}
		for (j = 0; j < size_cells; ++j) {
			int offset = (cellslen * i) +
				(sizeof(u32) * (j + addr_cells));
			size |= (u64)DT_PROP_DATA_TO_U32(prop->data, offset) <<
				((size_cells - j - 1) * 32);
		}

		if ((phys_addr_t)addr != addr) {
			pr_err("phys_addr_t is smaller than provided address 0x%llx!\n",
					addr);
			return -EINVAL;
		}

		memc_idx = brcmstb_memory_phys_addr_to_memc((phys_addr_t)addr);
		if (memc_idx == -1) {
			pr_err("address 0x%llx does not appear to be in any memc\n",
					addr);
			return -EINVAL;
		}

		range_idx = mem->memc[memc_idx].count;
		if (mem->memc[memc_idx].count >= MAX_BRCMSTB_RANGE)
			pr_warn("%s: Exceeded max ranges for memc%d\n",
					__func__, memc_idx);
		else {
			mem->memc[memc_idx].range[range_idx].addr = addr;
			mem->memc[memc_idx].range[range_idx].size = size;
		}
		++mem->memc[memc_idx].count;
	}

	return 0;
}

static int populate_lowmem(struct brcmstb_memory *mem)
{
#ifdef CONFIG_ARM
	mem->lowmem.range[0].addr = __pa(PAGE_OFFSET);
	mem->lowmem.range[0].size = (unsigned long)high_memory - PAGE_OFFSET;
	++mem->lowmem.count;
	return 0;
#else
	return -ENOSYS;
#endif
}

static int populate_bmem(struct brcmstb_memory *mem)
{
#ifdef CONFIG_BRCMSTB_BMEM
	phys_addr_t addr, size;
	int i;

	for (i = 0; i < MAX_BRCMSTB_RANGE; ++i) {
		if (bmem_region_info(i, &addr, &size))
			break;  /* no more regions */
		mem->bmem.range[i].addr = addr;
		mem->bmem.range[i].size = size;
		++mem->bmem.count;
	}
	if (i >= MAX_BRCMSTB_RANGE) {
		while (bmem_region_info(i, &addr, &size) == 0) {
			pr_warn("%s: Exceeded max ranges\n", __func__);
			++mem->bmem.count;
		}
	}

	return 0;
#else
	return -ENOSYS;
#endif
}

static int populate_cma(struct brcmstb_memory *mem)
{
#ifdef CONFIG_BRCMSTB_CMA
	int i;

	for (i = 0; i < CMA_NUM_RANGES; ++i) {
		struct cma_dev *cdev = cma_dev_get_cma_dev(i);
		if (cdev == NULL)
			break;
		if (i >= MAX_BRCMSTB_RANGE)
			pr_warn("%s: Exceeded max ranges\n", __func__);
		else {
			mem->cma.range[i].addr = cdev->range.base;
			mem->cma.range[i].size = cdev->range.size;
		}
		++mem->cma.count;
	}

	return 0;
#else
	return -ENOSYS;
#endif
}

static int populate_reserved(struct brcmstb_memory *mem)
{
#ifdef CONFIG_HAVE_MEMBLOCK
	memcpy(&mem->reserved, &reserved_init, sizeof(reserved_init));
	return 0;
#else
	return -ENOSYS;
#endif
}

/**
 * brcmstb_memory_get_default_reserve() - find default reservation for given ID
 * @bank_nr: bank index
 * @pstart: pointer to the start address (output)
 * @psize: pointer to the size address (output)
 *
 * NOTE: This interface will change in future kernels that do not have meminfo
 *
 * This takes in the bank number and determines the size and address of the
 * default region reserved for refsw within the bank.
 */
int __init brcmstb_memory_get_default_reserve(int bank_nr,
		phys_addr_t *pstart, phys_addr_t *psize)
{
	/* min alignment for mm core */
	const phys_addr_t alignment =
		PAGE_SIZE << max(MAX_ORDER - 1, pageblock_order);
	struct membank *bank = &meminfo.bank[bank_nr];
	phys_addr_t start = bank->start;
	phys_addr_t size = 0, adj = 0;
	phys_addr_t newstart, newsize;
	int i;

	if (!pstart || !psize)
		return -EFAULT;

	if (bank_nr == 0) {
		if (meminfo.nr_banks == 1) {
			u32 rem;
			u64 tmp;

			BUG_ON(bank->highmem);
			if (bank->size < SZ_32M) {
				pr_err("low memory too small for default bmem\n");
				return -EINVAL;
			}

			if (brcmstb_default_reserve == BRCMSTB_RESERVE_BMEM) {
				if (bank->size <= SZ_128M)
					return -EINVAL;

				adj = SZ_128M;
			}

			/* kernel reserves X percent, bmem gets the rest */
			tmp = ((u64)(bank->size - adj)) * (100 - DEFAULT_LOWMEM_PCT);
			rem = do_div(tmp, 100);
			size = tmp + rem;
			start = bank->start + bank->size - size;
		} else {
			/* If more than one bank, don't use first bank */
			return -EINVAL;
		}
	} else if (bank->start >= VME_A32_MAX && bank->size > SZ_64M) {
		/*
		 * Nexus doesn't use the address extension range yet, just
		 * reserve 64 MiB in these areas until we have a firmer
		 * specification
		 */
		size = SZ_64M;
	} else {
		size = bank->size;
	}

	/*
	 * To keep things simple, we only handle the case where reserved memory
	 * is at the start or end of a region.
	 */
	i = 0;
	while (i < memblock.reserved.cnt) {
		struct memblock_region *region = &memblock.reserved.regions[i];
		newstart = start;
		newsize = size;

		if (start >= region->base &&
				start < region->base + region->size) {
			/* adjust for reserved region at beginning */
			newstart = region->base + region->size;
			newsize = size - (newstart - start);
		} else if (start < region->base) {
			if (start + size >
					region->base + region->size) {
				/* unhandled condition */
				pr_err("%s: Split region %pa@%pa, reserve will fail\n",
						__func__, &size, &start);
				/* enable 'memblock=debug' for dump output */
				memblock_dump_all();
				return -EINVAL;
			}
			/* adjust for reserved region at end */
			newsize = min(region->base - start, size);
		}
		/* see if we had any modifications */
		if (newsize != size || newstart != start) {
			pr_debug("%s: moving default region from %pa@%pa to %pa@%pa\n",
					__func__, &size, &start, &newsize,
					&newstart);
			size = newsize;
			start = newstart;
			i = 0; /* start over */
		} else {
			++i;
		}
	}

	/* Fix up alignment */
	newstart = ALIGN(start, alignment);
	if (newstart != start) {
		pr_debug("adjusting start from %pa to %pa\n",
				&start, &newstart);
		start = newstart;
	}
	newsize = round_down(size, alignment);
	if (newsize != size) {
		pr_debug("adjusting size from %pa to %pa\n",
				&size, &newsize);
		size = newsize;
	}

	if (size == 0) {
		pr_debug("size available in bank was 0 - skipping\n");
		return -EINVAL;
	}

	*pstart = start;
	*psize = size;

	return 0;
}

/**
 * brcmstb_memory_reserve() - fill in static brcmstb_memory structure
 *
 * This is a boot-time initialization function used to copy the information
 * stored in the memblock reserve function that is discarded after boot.
 */
void __init brcmstb_memory_reserve(void)
{
#ifdef CONFIG_HAVE_MEMBLOCK
	struct memblock_type *type = &memblock.reserved;
	int i;

	for (i = 0; i < type->cnt; ++i) {
		struct memblock_region *region = &type->regions[i];

		if (i >= MAX_BRCMSTB_RESERVED_RANGE)
			pr_warn_once("%s: Exceeded max ranges\n", __func__);
		else {
			reserved_init.range[i].addr = region->base;
			reserved_init.range[i].size = region->size;
		}
		++reserved_init.count;
	}
#else
	pr_err("No memblock, cannot get reserved range\n");
#endif
}

/*
 * brcmstb_memory_get() - fill in brcmstb_memory structure
 * @mem: pointer to allocated struct brcmstb_memory to fill
 *
 * The brcmstb_memory struct is required by the brcmstb middleware to
 * determine how to set up its memory heaps.  This function expects that the
 * passed pointer is valid.  The struct does not need to have be zeroed
 * before calling.
 */
int brcmstb_memory_get(struct brcmstb_memory *mem)
{
	const void *fdt = initial_boot_params;
	const struct fdt_property *prop;
	int addr_cells = 1, size_cells = 1;
	int proplen;
	int ret;

	if (!mem)
		return -EFAULT;

	if (!fdt) {
		pr_err("No device tree?\n");
		return -EINVAL;
	}

	/* Get root size and address cells if specified */
	prop = fdt_get_property(fdt, 0, "#size-cells", &proplen);
	if (prop)
		size_cells = DT_PROP_DATA_TO_U32(prop->data, 0);
	pr_debug("size_cells = %x\n", size_cells);

	prop = fdt_get_property(fdt, 0, "#address-cells", &proplen);
	if (prop)
		addr_cells = DT_PROP_DATA_TO_U32(prop->data, 0);
	pr_debug("address_cells = %x\n", addr_cells);

	memset(mem, 0, sizeof(*mem));

	ret = populate_memc(mem, addr_cells, size_cells);
	if (ret)
		return ret;

	ret = populate_lowmem(mem);
	if (ret)
		return ret;

	ret = populate_bmem(mem);
	if (ret)
		pr_debug("bmem is disabled\n");

	ret = populate_cma(mem);
	if (ret)
		pr_debug("cma is disabled\n");

	ret = populate_reserved(mem);
	if (ret)
		return ret;

	return 0;
}
EXPORT_SYMBOL(brcmstb_memory_get);

static int pte_callback(pte_t *pte, unsigned long x, unsigned long y,
			struct mm_walk *walk)
{
	const pgprot_t pte_prot = __pgprot(*pte);
	const pgprot_t req_prot = *((pgprot_t *)walk->private);
	const pgprot_t prot_msk = L_PTE_MT_MASK | L_PTE_VALID;
	return (((pte_prot ^ req_prot) & prot_msk) == 0) ? 0 : -1;
}

static void *page_to_virt_contig(const struct page *page, unsigned int pg_cnt,
					pgprot_t pgprot)
{
	int rc;
	struct mm_walk walk;
	unsigned long pfn;
	unsigned long pfn_start;
	unsigned long pfn_end;
	unsigned long va_start;
	unsigned long va_end;

	if ((page == NULL) || !pg_cnt)
		return ERR_PTR(-EINVAL);

	pfn_start = page_to_pfn(page);
	pfn_end = pfn_start + pg_cnt;
	for (pfn = pfn_start; pfn < pfn_end; pfn++) {
		const struct page *cur_pg = pfn_to_page(pfn);
		phys_addr_t pa;

		/* Verify range is in low memory only */
		if (PageHighMem(cur_pg))
			return NULL;

		/* Must be mapped */
		pa = page_to_phys(cur_pg);
		if (page_address(cur_pg) == NULL)
			return NULL;
	}

	/*
	 * Aliased mappings with different cacheability attributes on ARM can
	 * lead to trouble!
	 */
	memset(&walk, 0, sizeof(walk));
	walk.pte_entry = &pte_callback;
	walk.private = (void *)&pgprot;
	walk.mm = current->mm;
	va_start = (unsigned long)page_address(page);
	va_end = (unsigned long)(page_address(page) + (pg_cnt << PAGE_SHIFT));
	rc = walk_page_range(va_start,
			     va_end,
			     &walk);
	if (rc)
		pr_debug("cacheability mismatch\n");

	return rc ? NULL : page_address(page);
}

static struct page **get_pages(struct page *page, int num_pages)
{
	struct page **pages;
	long pfn;
	int i;

	if (num_pages == 0) {
		pr_err("bad count\n");
		return NULL;
	}

	if (page == NULL) {
		pr_err("bad page\n");
		return NULL;
	}

	pages = vmalloc(sizeof(struct page *) * num_pages);
	if (pages == NULL)
		return NULL;

	pfn = page_to_pfn(page);
	for (i = 0; i < num_pages; i++) {
		/*
		 * pfn_to_page() should resolve to simple arithmetic for the
		 * FLATMEM memory model.
		 */
		pages[i] = pfn_to_page(pfn++);
	}

	return pages;
}

/*
 * Basically just vmap() without checking that count < totalram_pages,
 * since we want to be able to map pages that aren't managed by Linux
 */
static void *brcmstb_memory_vmap(struct page **pages, unsigned int count,
		unsigned long flags, pgprot_t prot)
{
	struct vm_struct *area;

	might_sleep();

	area = get_vm_area_caller((count << PAGE_SHIFT), flags,
					__builtin_return_address(0));
	if (!area)
		return NULL;

	if (map_vm_area(area, prot, &pages)) {
		vunmap(area->addr);
		return NULL;
	}

	return area->addr;
}

/**
 * brcmstb_memory_kva_map() - Map page(s) to a kernel virtual address
 *
 * @page: A struct page * that points to the beginning of a chunk of physical
 * contiguous memory.
 * @num_pages: Number of pages
 * @pgprot: Page protection bits
 *
 * Return: pointer to mapping, or NULL on failure
 */
void *brcmstb_memory_kva_map(struct page *page, int num_pages, pgprot_t pgprot)
{
	void *va;

	/* get the virtual address for this range if it exists */
	va = page_to_virt_contig(page, num_pages, pgprot);
	if (IS_ERR(va)) {
		pr_debug("page_to_virt_contig() failed (%ld)\n", PTR_ERR(va));
		return NULL;
	} else if (va == NULL || is_vmalloc_addr(va)) {
		struct page **pages;

		pages = get_pages(page, num_pages);
		if (pages == NULL) {
			pr_err("couldn't get pages\n");
			return NULL;
		}

		va = brcmstb_memory_vmap(pages, num_pages, 0, pgprot);

		vfree(pages);

		if (va == NULL) {
			pr_err("vmap failed (num_pgs=%d)\n", num_pages);
			return NULL;
		}
	}

	return va;
}
EXPORT_SYMBOL(brcmstb_memory_kva_map);

/**
 * brcmstb_memory_kva_map_phys() - map phys range to kernel virtual address
 *
 * @phys: physical address base
 * @size: size of range to map
 * @cached: whether to use cached or uncached mapping
 *
 * Return: NULL on failure, err on success
 */
void *brcmstb_memory_kva_map_phys(phys_addr_t phys, size_t size, bool cached)
{
	void *addr = NULL;
	unsigned long pfn = PFN_DOWN(phys);

	if (!cached) {
		/*
		 * This could be supported for MIPS by using ioremap instead,
		 * but that cannot be done on ARM if you want O_DIRECT support
		 * because having multiple mappings to the same memory with
		 * different cacheability will result in undefined behavior.
		 */
		return NULL;
	}

	if (pfn_valid(pfn)) {
		addr = brcmstb_memory_kva_map(pfn_to_page(pfn),
				size / PAGE_SIZE, PAGE_KERNEL);
	}
	return addr;
}
EXPORT_SYMBOL(brcmstb_memory_kva_map_phys);

/**
 * brcmstb_memory_kva_unmap() - Unmap a kernel virtual address associated
 * to physical pages mapped by brcmstb_memory_kva_map()
 *
 * @kva: Kernel virtual address previously mapped by brcmstb_memory_kva_map()
 *
 * Return: 0 on success, negative on failure.
 */
int brcmstb_memory_kva_unmap(const void *kva)
{
	if (kva == NULL)
		return -EINVAL;

	if (!is_vmalloc_addr(kva)) {
		/* unmapping not necessary for low memory VAs */
		return 0;
	}

	vunmap(kva);

	return 0;
}
EXPORT_SYMBOL(brcmstb_memory_kva_unmap);
