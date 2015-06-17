/*
 * arch/sh/kernel/cpu/sh4/cpufreq-stb7100.c
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
#include <linux/delay.h>	/* loops_per_jiffy */
#include <linux/cpumask.h>
#include <linux/smp.h>
#include <linux/sched.h>	/* set_cpus_allowed() */
#include <linux/stm/clk.h>
#include <linux/io.h>

#include <asm/processor.h>

extern void __iomem *clkgena_base;
static struct clk *pll0_clk;
static struct clk *sh4_ic_clk;
static struct clk *module_clk;

#define CLOCKGEN_LOCK		(clkgena_base + 0x00)
#define ST40_CLK_CTRL 		(clkgena_base + 0x14)
#define CKGA_CLKOUT_SEL 	(clkgena_base + 0x38)

static struct sh4_ratio {
	long cpu, bus, per;
} ratios[] = {
	{0, 1, 0},	/* 1:1 - 1:2 - 1:4 */
	{1, 3, 0},	/* 1:2 - 1:4 - 1:4 */
	{3, 5, 5},	/* 1:4 - 1:8 - 1:8 */
};

static void st_cpufreq_update_clocks(unsigned int set, int propagate)
{
	static unsigned int current_set = 0;
	unsigned long flag;
	unsigned long st40_clk_address = ST40_CLK_CTRL;
	unsigned long l_p_j = _1_ms_lpj();

	l_p_j >>= 3;		/* l_p_j = 125 usec (for each HZ) */

	local_irq_save(flag);
	iowrite32(0xc0de, CLOCKGEN_LOCK);

	if (set > current_set) {/* down scaling... */
		/* it scales l_p_j based on the new frequency */
		l_p_j >>= 1;	/* 266 -> 133 or 133 -> 66.5 */
		if ((set + current_set) == 2)
			l_p_j >>= 1;	/* 266 -> 66.5 */

		asm volatile (".balign	32	\n"
/* sets the sh4per clock */   "mov.l	%3, @(8,%0)\n"
/* sets the sh4ic  clock */   "mov.l	%2, @(4,%0)\n"
/* sets the sh4 clock */      "mov.l	%1, @(0,%0)\n"
			      "tst	%4, %4	\n"
			      "1:		\n"
			      "bf/s	1b	\n"
			      " dt	%4	\n"
			::"r" (st40_clk_address),
			  "r"(ratios[set].cpu),
			  "r"(ratios[set].bus),
			  "r"(ratios[set].per),
			  "r"(l_p_j)
			:"memory", "t");
	} else {
		/* it scales l_p_j based on the new frequency */
		l_p_j <<= 1;	/* 133  -> 266 or 66.5 -> 133 */
		if ((set + current_set) == 2)
			l_p_j <<= 1;	/* 66.5 -> 266 */

		asm volatile (".balign	32	\n"
			      "mov.l	%1, @(0,%0)\n"
			      "mov.l	%2, @(4,%0)\n"
			      "mov.l	%3, @(8,%0)\n"
			      "tst	%4, %4	\n"
			      "2:		\n"
			      "bf/s	2b	\n"
			      " dt	%4	\n"
			::"r" (st40_clk_address),
			  "r"(ratios[set].cpu),
			  "r"(ratios[set].bus),
			  "r"(ratios[set].per),
			  "r"(l_p_j)
			:"memory", "t");
	}

	iowrite32(0, CLOCKGEN_LOCK);
	current_set = set;
	sh4_clk->rate = (cpu_freqs[set].frequency << 3) * 125;
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
	local_irq_restore(flag);
}

#ifdef CONFIG_STM_CPU_FREQ_OBSERVE
static void __init st_cpufreq_observe_init(void)
{
	/* route the sh4  clock frequency */
	iowrite8(0, CKGA_CLKOUT_SEL);
}
#endif

static int __init st_cpufreq_platform_init(void)
{
	cpufreq_debug_printk(CPUFREQ_DEBUG_DRIVER,
		"st_cpufreq_platform_init:", "\n");

	pll0_clk = clk_get(NULL, "pll0_clk");
	sh4_ic_clk = clk_get(NULL, "sh4_ic_clk");
	module_clk = clk_get(NULL, "module_clk");

	if (!pll0_clk) {
		printk(KERN_ERR "ERROR: on clk_get(pll0_clk)\n");
		return -1;
	}
	if (!sh4_ic_clk) {
		printk(KERN_ERR "ERROR: on clk_get(sh4_ic_clk)\n");
		return -1;
	}
	if (!module_clk) {
		printk(KERN_ERR "ERROR: on clk_get(module_clk)\n");
		return -1;
	}
	return 0;
}
