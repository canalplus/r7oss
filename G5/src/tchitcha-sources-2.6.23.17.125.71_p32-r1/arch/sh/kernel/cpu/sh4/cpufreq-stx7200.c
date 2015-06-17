/*
 * arch/sh/kernel/cpu/sh4/cpufreq-stx7200.c
 *
 * Cpufreq driver for the ST40 processors.
 * Version: 0.1 (7 Jan 2008)
 *
 * Copyright (C) 2008 STMicroelectronics
 * Author: Francesco M. Virlinzi <francesco.virlinzi@st.com>
 *
 * This program is under the terms of the
 * General Public License version 2 ONLY
 *
 */
#include <linux/types.h>
#include <linux/cpufreq.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/time.h>
#include <linux/delay.h>
#include <linux/cpumask.h>
#include <linux/smp.h>
#include <linux/sched.h>	/* set_cpus_allowed() */
#include <linux/io.h>
#include <linux/stm/clk.h>
#include <asm/processor.h>
#include <asm/system.h>


static struct clk *pll0_clk;
static struct clk *sh4_ic_clk;
static struct clk *module_clk;

#define clk_iomem		0xfd700000 /* Clockgen A */
#define CLKGNA_DIV_CFG		(clk_iomem + 0x10)
#define CKGA_CLKOUT_SEL 	(clk_iomem + 0x18)
#define SH4_CLK_MASK		(0x1ff << 1)
#define SH4_CLK_MASK_C2		(0x3 << 1)
/*
 *	value: 0  1  2  3  4  5  6     7
 *	ratio: 1, 2, 3, 4, 6, 8, 1024, 1
 */
static unsigned long sh4_ratio[] = {
/*	  cpu	   bus	    per */
	(0 << 1) | (1 << 4) | (3 << 7),	/* 1:1 - 1:2 - 1:4 */
	(1 << 1) | (3 << 4) | (3 << 7),	/* 1:2 - 1:4 - 1:4 */
	(3 << 1) | (5 << 4) | (5 << 7)	/* 1:4 - 1:8 - 1:8 */
};

static unsigned long sh4_ratio_c2[] = { /* ratios for Cut 2.0 */
/*        cpu   */
        (1 << 1),
        (2 << 1),
};

static unsigned long *ratio;
static void st_cpufreq_update_clocks(unsigned int set, int propagate)
{
	static unsigned int sh_current_set;
	unsigned long clks_address = CLKGNA_DIV_CFG;
	unsigned long clks_value = ctrl_inl(clks_address);
	unsigned long flag;
	unsigned long l_p_j = _1_ms_lpj();

	l_p_j >>= 3;	/* l_p_j = 125 usec (for each HZ) */

	if (set > sh_current_set) {	/* down scaling... */
		l_p_j >>= 1;
		if ((set + sh_current_set) == 2)
			l_p_j >>= 1;
	} else {		/* up scaling... */
		l_p_j <<= 1;
		if ((set + sh_current_set) == 2)
			l_p_j <<= 1;
	}
	if (cpu_data->cut_major < 2)
		clks_value &= ~SH4_CLK_MASK;
	else
		clks_value &= ~SH4_CLK_MASK_C2;

	clks_value |= ratio[set];

	local_irq_save(flag);
	asm volatile (".balign	32	\n"
		      "mov.l	%1, @%0	\n"
		      "tst	%2, %2	\n"
		      "1:		\n"
		      "bf/s	1b	\n"
		      " dt	%2	\n"
		 ::"r" (clks_address),
		      "r"(clks_value),
		      "r"(l_p_j)
		 :"t", "memory");

	cpufreq_debug_printk(CPUFREQ_DEBUG_DRIVER,
		"st_cpufreq_update_clocks:", "\n");
	sh_current_set = set;
	sh4_clk->rate = (cpu_freqs[set].frequency << 3) * 125;

	if (cpu_data->cut_major < 2) {
		sh4_ic_clk->rate = (cpu_freqs[set].frequency << 2) * 125;
		module_clk->rate = clk_get_rate(pll0_clk) >> 3;
		if (set == 2)
			module_clk->rate >>= 1;
/* The module_clk propagation can create a race condition
 * on the tmu0 during the suspend/resume...
 * The race condition basically leaves the TMU0 enabled
 * with interrupt enabled and the system immediately resume
 * after a suspend
 */
		if (propagate)
			clk_set_rate(module_clk, module_clk->rate);	/* to propagate... */
	}
	local_irq_restore(flag);
}

#ifdef CONFIG_STM_CPU_FREQ_OBSERVE
static void __init st_cpufreq_observe_init(void)
{
	/* route the sh4/2  clock frequency */
	ctrl_outl(0xc, CKGA_CLKOUT_SEL);
}
#endif

static int __init st_cpufreq_platform_init(void)
{
	pll0_clk = clk_get(NULL, "pll0_clk");
	ratio = sh4_ratio_c2;
	if (!pll0_clk) {
		printk(KERN_ERR "ERROR: on clk_get(pll0_clk)\n");
		return -ENODEV;
	}
	if (cpu_data->cut_major < 2) {
		ratio = sh4_ratio;
		sh4_ic_clk = clk_get(NULL, "sh4_ic_clk");
		module_clk = clk_get(NULL, "module_clk");
		if (!sh4_ic_clk) {
			printk(KERN_ERR "ERROR: on clk_get(sh4_ic_clk)\n");
			return -ENODEV;
		}
		if (!module_clk) {
			printk(KERN_ERR "ERROR: on clk_get(module_clk)\n");
			return -ENODEV;
		}
	}
	 else
		/* in the 7200 Cut 2 only two frequencies are supported */
		cpu_freqs[2].frequency = CPUFREQ_TABLE_END;

	return 0;
}
