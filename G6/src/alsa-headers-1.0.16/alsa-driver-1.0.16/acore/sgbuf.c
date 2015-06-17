#define __NO_VERSION__
#include <linux/config.h>
#include <linux/version.h>

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0)
#if defined(CONFIG_MODVERSIONS) && !defined(__GENKSYMS__) && !defined(__DEPEND__)
#include "sndversions.h"
#endif
#endif

#define SKIP_HIDDEN_MALLOCS
#include "adriver.h"

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 5, 0)
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,23)
#define CONFIG_HAS_DMA 1
#endif
#include "../alsa-kernel/core/sgbuf.c"
#else

/*
 * we don't have vmap/vunmap, so use vmalloc_32 and vmalloc_dma instead
 */

#include <linux/pci.h>
#include <linux/slab.h>
#include <linux/pagemap.h>
#include <linux/vmalloc.h>
#include <asm/io.h>
#include <sound/memalloc.h>

/* table entries are align to 32 */
#define SGBUF_TBL_ALIGN		32
#define sgbuf_align_table(tbl)	((((tbl) + SGBUF_TBL_ALIGN - 1) / SGBUF_TBL_ALIGN) * SGBUF_TBL_ALIGN)

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 4, 0)
/* get the virtual address of the given vmalloc'ed pointer */
static void *get_vmalloc_addr(void *pageptr)
{
	pgd_t *pgd;
	pmd_t *pmd;
	pte_t *pte;
	unsigned long lpage;

	lpage = VMALLOC_VMADDR(pageptr);
	pgd = pgd_offset_k(lpage);
	pmd = pmd_offset(pgd, lpage);
	pte = pte_offset(pmd, lpage);
	return (void *)pte_page(*pte);
}    
#endif

/* set up the page table from the given vmalloc'ed buffer pointer.
 * return a negative error if the page is out of the pci address mask.
 */
static int store_page_tables(struct snd_sg_buf *sgbuf, void *vmaddr, unsigned int pages)
{
	unsigned int i;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 4, 0)
	unsigned long rmask;
	struct pci_dev *pci = (struct pci_dev *)sgbuf->dev;
	rmask = pci->dma_mask;
	if (rmask)
		rmask = ~rmask;
	else
		rmask = ~0xffffffUL;
#endif

	sgbuf->pages = 0;
	for (i = 0; i < pages; i++) {
		struct page *page;
		void *ptr;
		dma_addr_t addr;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 4, 0)
		page = vmalloc_to_page(vmaddr + (i << PAGE_SHIFT));
		ptr = page_address(page);
		addr = virt_to_bus(ptr);
		if (addr & rmask)
			return -EINVAL;
#else
		ptr = get_vmalloc_addr(vmaddr + (i << PAGE_SHIFT));
		addr = virt_to_bus(ptr);
		page = virt_to_page(ptr);
#endif
		sgbuf->table[i].buf = ptr;
		sgbuf->table[i].addr = addr;
		sgbuf->page_table[i] = page;
		SetPageReserved(page);
		sgbuf->pages++;
	}
	return 0;
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 2, 18)
#define vmalloc_32(x) vmalloc(x)
#endif

/* remove all vmalloced pages */
static void release_vm_buffer(struct snd_sg_buf *sgbuf, void *vmaddr)
{
	int i;

	for (i = 0; i < sgbuf->pages; i++)
		if (sgbuf->page_table[i]) {
			ClearPageReserved(sgbuf->page_table[i]);
			sgbuf->page_table[i] = NULL;
		}
	sgbuf->pages = 0;
	if (vmaddr)
		vfree(vmaddr); /* don't use wrapper */
}

int snd_free_sgbuf_pages(struct snd_dma_buffer *dmab)
{
	struct snd_sg_buf *sgbuf = dmab->private_data;

	if (dmab->area)
		release_vm_buffer(sgbuf, dmab->area);
	dmab->area = NULL;
	if (sgbuf->table)
		kfree(sgbuf->table);
	sgbuf->table = NULL;
	if (sgbuf->page_table)
		kfree(sgbuf->page_table);
	kfree(sgbuf);
	dmab->private_data = NULL;
	
	return 0;
}

void *snd_malloc_sgbuf_pages(struct device *dev,
			     size_t size, struct snd_dma_buffer *dmab,
			     size_t *res_size)
{
	struct snd_sg_buf *sgbuf;
	unsigned int pages;

	dmab->area = NULL;
	dmab->addr = 0;
	dmab->private_data = sgbuf = kmalloc(sizeof(*sgbuf), GFP_KERNEL);
	if (! sgbuf)
		return NULL;
	memset(sgbuf, 0, sizeof(*sgbuf));
	sgbuf->dev = dev;
	pages = snd_sgbuf_aligned_pages(size);
	sgbuf->tblsize = sgbuf_align_table(pages);
	sgbuf->table = kmalloc(sizeof(*sgbuf->table) * sgbuf->tblsize, GFP_KERNEL);
	if (! sgbuf->table)
		goto _failed;
	memset(sgbuf->table, 0, sizeof(*sgbuf->table) * sgbuf->tblsize);
	sgbuf->page_table = kmalloc(sizeof(*sgbuf->page_table) * sgbuf->tblsize, GFP_KERNEL);
	if (! sgbuf->page_table)
		goto _failed;
	memset(sgbuf->page_table, 0, sizeof(*sgbuf->page_table) * sgbuf->tblsize);

	sgbuf->size = size;
	dmab->area = vmalloc_32(pages << PAGE_SHIFT);
	if (! dmab->area || store_page_tables(sgbuf, dmab->area, pages) < 0) {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 4, 0)
		/* reallocate with DMA flag */
		release_vm_buffer(sgbuf, dmab->area);
		dmab->area = vmalloc_dma(pages << PAGE_SHIFT);
		if (! dmab->area || store_page_tables(sgbuf, dmab->area, pages) < 0)
			goto _failed;
		
#else
		goto _failed;
#endif
	}

	memset(dmab->area, 0, size);
	return dmab->area;

 _failed:
	snd_free_sgbuf_pages(dmab); /* free the table */
	return NULL;
}

#endif /* < 2.5.0 */
