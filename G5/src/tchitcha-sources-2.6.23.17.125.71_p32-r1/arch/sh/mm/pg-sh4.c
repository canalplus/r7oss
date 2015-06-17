/*
 * arch/sh/mm/pg-sh4.c
 *
 * Copyright (C) 1999, 2000, 2002  Niibe Yutaka
 * Copyright (C) 2002 - 2005  Paul Mundt
 *
 * Released under the terms of the GNU GPL v2.0.
 */
#include <linux/init.h>
#include <linux/mman.h>
#include <linux/mm.h>
#include <linux/threads.h>
#include <asm/addrspace.h>
#include <asm/page.h>
#include <asm/pgtable.h>
#include <asm/processor.h>
#include <asm/cache.h>
#include <asm/io.h>
#include <asm/uaccess.h>
#include <asm/pgalloc.h>
#include <asm/mmu_context.h>
#include <asm/cacheflush.h>

//extern struct semaphore p3map_sem[];
static atomic_t concurreny_check[16];

#define CACHE_ALIAS (current_cpu_data.dcache.alias_mask)

/*
 * clear_user_page
 * @to: kernel logical address
 * @address: U0 address to be mapped
 * @page: page (virt_to_page(to))
 */
void clear_user_page(void *to, unsigned long address, struct page *page)
{
	void __clear_page_wb(void *to);

	if (((address ^ (unsigned long)to) & CACHE_ALIAS) == 0)
		__clear_page_wb(to);
	else {
		pgprot_t pgprot = __pgprot(_PAGE_PRESENT |
					   _PAGE_RW | _PAGE_CACHABLE |
					   _PAGE_DIRTY | _PAGE_ACCESSED |
					   _PAGE_HW_SHARED | _PAGE_FLAGS_HARD);
		unsigned long phys_addr = virt_to_phys(to);
		unsigned long p3_addr = P3SEG + (address & CACHE_ALIAS);
		pgd_t *pgd = pgd_offset_k(p3_addr);
		pud_t *pud = pud_offset(pgd, p3_addr);
		pmd_t *pmd = pmd_offset(pud, p3_addr);
		pte_t *pte = pte_offset_kernel(pmd, p3_addr);
		pte_t entry;
		unsigned long flags;

		entry = pfn_pte(phys_addr >> PAGE_SHIFT, pgprot);
		inc_preempt_count();
BUG_ON(atomic_inc_return(&concurreny_check[(address & CACHE_ALIAS)>>12]) != 1);
		set_pte(pte, entry);
		local_irq_save(flags);
		flush_tlb_one(get_asid(), p3_addr);
		local_irq_restore(flags);
		update_mmu_cache(NULL, p3_addr, entry);
		__clear_user_page((void *)p3_addr, to);
		pte_clear(&init_mm, p3_addr, pte);
atomic_dec(&concurreny_check[(address & CACHE_ALIAS)>>12]);
		dec_preempt_count();
	}
}

/*
 * copy_user_page
 * @to: kernel logical address
 * @from: kernel logical address
 * @address: U0 address to be mapped
 * @page: page (virt_to_page(to))
 */
void copy_user_page(void *to, void *from, unsigned long address,
		    struct page *page)
{
	extern void __copy_page_wb(void *to, void *from);

	if (((address ^ (unsigned long)to) & CACHE_ALIAS) == 0)
		__copy_page_wb(to, from);
	else {
		pgprot_t pgprot = __pgprot(_PAGE_PRESENT |
					   _PAGE_RW | _PAGE_CACHABLE |
					   _PAGE_DIRTY | _PAGE_ACCESSED |
					   _PAGE_HW_SHARED | _PAGE_FLAGS_HARD);
		unsigned long phys_addr = virt_to_phys(to);
		unsigned long p3_addr = P3SEG + (address & CACHE_ALIAS);
		pgd_t *pgd = pgd_offset_k(p3_addr);
		pud_t *pud = pud_offset(pgd, p3_addr);
		pmd_t *pmd = pmd_offset(pud, p3_addr);
		pte_t *pte = pte_offset_kernel(pmd, p3_addr);
		pte_t entry;
		unsigned long flags;

		entry = pfn_pte(phys_addr >> PAGE_SHIFT, pgprot);
		inc_preempt_count();
BUG_ON(atomic_inc_return(&concurreny_check[(address & CACHE_ALIAS)>>12]) != 1);
		set_pte(pte, entry);
		local_irq_save(flags);
		flush_tlb_one(get_asid(), p3_addr);
		local_irq_restore(flags);
		update_mmu_cache(NULL, p3_addr, entry);
		__copy_user_page((void *)p3_addr, from, to);
		pte_clear(&init_mm, p3_addr, pte);
atomic_dec(&concurreny_check[(address & CACHE_ALIAS)>>12]);
		dec_preempt_count();
	}
}
