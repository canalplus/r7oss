/*
 * -------------------------------------------------------------------------
 * <linux_root>/arch/sh/kernel/cpu/sh4/cpu.c
 * -------------------------------------------------------------------------
 * Copyright (C) 2009  STMicroelectronics
 * Author: Francesco M. Virlinzi  <francesco.virlinzi@st.com>
 *
 * May be copied or modified under the terms of the GNU General Public
 * License V.2 ONLY.  See linux/COPYING for more information.
 *
 * ------------------------------------------------------------------------- */

#include <linux/init.h>
#include <linux/suspend.h>
#include <linux/errno.h>
#include <linux/time.h>
#include <linux/irqflags.h>
#include <linux/irq.h>
#include <asm/system.h>
#include <asm/io.h>
#include <asm/mmu.h>
#include <asm/cacheflush.h>

#undef  dbg_print

#ifdef CONFIG_PM_DEBUG
#define dbg_print(fmt, args...)			\
		printk(KERN_DEBUG "%s: " fmt, __FUNCTION__ , ## args)
#else
#define dbg_print(fmt, args...)
#endif

/*
 * saved registers:
 * - 8 gpr
 * -   pr
 * -   sr
 * -   r6_bank
 * -   r7_bank
 */
unsigned long saved_context_reg[12] __cacheline_aligned;

/* References to section boundaries */
int pfn_is_nosave(unsigned long pfn)
{
	unsigned long nosave_begin_pfn = __pa(&__nosave_begin) >> PAGE_SHIFT;
	unsigned long nosave_end_pfn =
	    PAGE_ALIGN(__pa(&__nosave_end)) >> PAGE_SHIFT;
	return (pfn >= nosave_begin_pfn) && (pfn < nosave_end_pfn);

}
