/*
 * arch/sh/kernel/cpu/sh4/cpufreq-stx5197.c
 *
 * Cpufreq driver for the ST40 processors.
 * Version: 0.1 (Jan 22 2009)
 *
 * Copyright (C) 2009 STMicroelectronics
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

#include "soc-stx5197.h"

#ifdef CONFIG_STM_CPU_FREQ_OBSERVE
#include <linux/stm/sysconf.h>
#include <linux/stm/pio.h>
#endif


struct __ratios {
	unsigned short cfg_0;
	unsigned char  cfg_1;
	unsigned char  cfg_2;
} ratios[] = {
#define COMBINE_DIVIDER(_depth, _seq, _hno, _even)	\
	.cfg_0 = (_seq) & 0xffff,			\
	.cfg_1 = (_seq) >> 16,				\
	.cfg_2 = (_depth) | ((_even) << 5) | ((_hno) << 6) | (1<<4)

	{ COMBINE_DIVIDER(0x01, 0x00AAA, 0x1, 0x1) }, /* : 2 */
	{ COMBINE_DIVIDER(0x05, 0x0CCCC, 0x1, 0x1) }, /* : 4 */
	{ COMBINE_DIVIDER(0x05, 0x0F0F0, 0x1, 0x1) }, /* : 8 */
};

static void st_cpufreq_update_clocks(unsigned int set,
				     int not_used_on_this_platform)
{
	static unsigned int current_set = 0;
	unsigned long flag;
	unsigned long addr_cfg_base = CLKDIV_CONF0(9) + SYS_SERV_BASE_ADDR;
	unsigned long addr_clk_mode = CLK_MODE_CTRL + SYS_SERV_BASE_ADDR;
	unsigned long cfg_0 = ratios[set].cfg_0;
	unsigned long cfg_1 = ratios[set].cfg_1;
	unsigned long cfg_2 = ratios[set].cfg_2;
	unsigned long l_p_j = _1_ms_lpj();

	cpufreq_debug_printk(CPUFREQ_DEBUG_DRIVER,
		"st_cpufreq_update_clocks", "\n");
	l_p_j >>= 2;	/* l_p_j = 250 usec (for each HZ) */

	local_irq_save(flag);

	if (set > current_set) {	/* down scaling... */
		/* it scales l_p_j based on the new frequency */
		l_p_j >>= 1;	/* 350 -> 175 or 175 -> 87 */
		if ((set + current_set) == 2)
			l_p_j >>= 1;	/* 350 -> 87 */
	} else {
		/* it scales l_p_j based on the new frequency */
		l_p_j <<= 1;	/* 175   -> 350 or 87 -> 175 */
		if ((set + current_set) == 2)
			l_p_j <<= 1;	/* 87 -> 350 */
	}
	CLK_UNLOCK();
	asm volatile (".balign  32	\n"
		      "mov.l	%6, @%5 \n" /* in X1 mode */
		      "mov.l	%1, @(0,%0)\n" /* set	  */
		      "mov.l	%2, @(4,%0)\n" /*  the	  */
		      "mov.l	%3, @(8,%0)\n" /*   ratio */
		      "mov.l	%7, @%5 \n" /* in Prog mode */
		      "tst	%4, %4  \n"
		      "2:		\n"
		      "bf/s	2b	\n"
		      " dt	%4	\n"
		::    "r" (addr_cfg_base),
		      "r" (cfg_0),
		      "r" (cfg_1),
		      "r" (cfg_2),
		      "r" (l_p_j),
		      "r" (addr_clk_mode),
		      "r" (CLK_MODE_CTRL_X1),
		      "r" (CLK_MODE_CTRL_PROG)
		:     "memory", "t");
	CLK_LOCK();
	current_set = set;
	sh4_clk->rate = (cpu_freqs[set].frequency << 3) * 125;
	local_irq_restore(flag);
}

#ifdef CONFIG_STM_CPU_FREQ_OBSERVE
static void __init st_cpufreq_observe_init(void)
{
	CLK_UNLOCK();
	writel(0x29, CLK_OBSERVE + SYS_SERV_BASE_ADDR);
	CLK_LOCK();
}
#endif

static int __init st_cpufreq_platform_init(void)
{
	return 0;
}
