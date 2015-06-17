/*
 * arch/sh/mm/ioremap.c
 *
 * Re-map IO memory to kernel address space so that we can access it.
 * This is needed for high PCI addresses that aren't mapped in the
 * 640k-1MB IO memory area on PC's
 *
 * (C) Copyright 1995 1996 Linus Torvalds
 * (C) Copyright 2005, 2006 Paul Mundt
 *
 * This file is subject to the terms and conditions of the GNU General
 * Public License. See the file "COPYING" in the main directory of this
 * archive for more details.
 */
#include <linux/vmalloc.h>
#include <linux/module.h>
#include <linux/mm.h>
#include <linux/pci.h>
#include <linux/io.h>
#include <asm/page.h>
#include <asm/pgalloc.h>
#include <asm/addrspace.h>
#include <asm/cacheflush.h>
#include <asm/tlbflush.h>
#include <asm/mmu.h>

/*
 * Remap an arbitrary physical address space into the kernel virtual
 * address space.
 *
 * NOTE! We need to allow non-page-aligned mappings too: we will obviously
 * have to convert them into an offset in a page-aligned mapping, but the
 * caller shouldn't need to know that small detail.
 */
void __iomem *__ioremap_prot(unsigned long phys_addr, unsigned long size,
			     pgprot_t pgprot)
{
	unsigned long offset, last_addr, addr;
	int simple = (pgprot_val(pgprot) == pgprot_val(PAGE_KERNEL)) ||
		(pgprot_val(pgprot) == pgprot_val(PAGE_KERNEL_NOCACHE));
	int cached = pgprot_val(pgprot) & _PAGE_CACHABLE;

	/* Don't allow wraparound or zero size */
	last_addr = phys_addr + size - 1;
	if (!size || last_addr < phys_addr)
		return NULL;

	/*
	 * Don't allow anybody to remap normal RAM that we're using..
	 */
	if ((phys_addr >= __pa(memory_start)) && (last_addr < __pa(memory_end))) {
		char *t_addr, *t_end;
		struct page *page;

		t_addr = __va(phys_addr);
		t_end = t_addr + (size - 1);

		for(page = virt_to_page(t_addr); page <= virt_to_page(t_end); page++)
			if(!PageReserved(page))
				return NULL;
	}

#ifndef CONFIG_32BIT
	/*
	 * For physical mappings <29 bits, with simple cached or uncached
	 * protections, this is trivial, as everything is already mapped
	 * through P1 and P2.
	 */
	if (likely(IS_29BIT(phys_addr) && simple)) {
		if (cached)
			return (void __iomem *)P1SEGADDR(phys_addr);

		return (void __iomem *)P2SEGADDR(phys_addr);
	}
#endif

	/* Similarly, P4 uncached addresses are permanently mapped */
	if ((PXSEG(phys_addr) == P4SEG) && simple && !cached)
		return (void __iomem *)phys_addr;

#ifndef CONFIG_32BIT
	/* Prevent mapping P1/2 addresses, to improve portability */
	if (unlikely(!IS_29BIT(phys_addr)))
		return (void __iomem *)0;
#endif

	/* Simple mapping failed, so use the PMB or TLB */

	/*
	 * Mappings have to be page-aligned
	 */
	offset = phys_addr & ~PAGE_MASK;
	phys_addr &= PAGE_MASK;
	size = PAGE_ALIGN(last_addr+1) - phys_addr;

#ifdef CONFIG_32BIT
	addr = pmb_remap(phys_addr, size, cached ? _PAGE_CACHABLE : 0);
#else
	addr = 0;
#endif

	if (addr == 0) {
		struct vm_struct * area;

		area = get_vm_area(size, VM_IOREMAP);
		if (!area)
			return NULL;

		area->phys_addr = phys_addr;
		addr = (unsigned long)area->addr;

		if (ioremap_page_range(addr, addr + size, phys_addr, pgprot)) {
			vunmap((void *)addr);
			return NULL;
		}
	}

	return (void __iomem *)(offset + (char *)addr);
}
EXPORT_SYMBOL(__ioremap_prot);

void __iomem *__ioremap_mode(unsigned long phys_addr, unsigned long size,
	unsigned long flags)
{
	pgprot_t pgprot;

	if (unlikely(flags & _PAGE_CACHABLE))
		pgprot = PAGE_KERNEL;
	else
		pgprot = PAGE_KERNEL_NOCACHE;

	return __ioremap_prot(phys_addr, size, pgprot);
}
EXPORT_SYMBOL(__ioremap_mode);

void __iounmap(void __iomem *addr)
{
	unsigned long vaddr = (unsigned long __force)addr;
	struct vm_struct *p;

	if (PXSEG(vaddr) == P4SEG)
		return;

#ifndef CONFIG_32BIT
	if (PXSEG(vaddr) < P3SEG)
		return;
#endif

#ifdef CONFIG_32BIT
	if(pmb_unmap(vaddr))
	  return;
#endif

	p = remove_vm_area((void *)(vaddr & PAGE_MASK));
	if (!p) {
		printk(KERN_ERR "%s: bad address %p\n", __FUNCTION__, addr);
		return;
	}

	kfree(p);
}
EXPORT_SYMBOL(__iounmap);
