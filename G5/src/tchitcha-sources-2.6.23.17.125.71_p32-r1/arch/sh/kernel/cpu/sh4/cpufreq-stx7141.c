/*
 * arch/sh/kernel/cpu/sh4/cpufreq-stx7141.c
 *
 * Cpufreq driver for the ST40 processors.
 * Version: 0.1 (Aug 26 2008)
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
#include <linux/io.h>
#include <linux/stm/clk.h>

#include <asm/processor.h>
#include <asm/freq.h>

#ifdef CONFIG_STM_CPU_FREQ_OBSERVE
#include <linux/stm/sysconf.h>
#include <linux/stm/pio.h>
#endif

#define clk_iomem		0xfe213000 /* Clockgen A */
#define CKGA_PLL0LS_DIV_CFG(x)	(0xa10 + (((x) -4) * 4))
#define ST40_CLK 		(clk_iomem + CKGA_PLL0LS_DIV_CFG(4))
#define CKGA_CLKOBS_MUX1_CFG	0x030

/*				1:1,	 1:2, 1:4	*/
unsigned long st40_ratios[] = { 0x10000, 0x1, 0x3 };

static void st_cpufreq_update_clocks(unsigned int set,
				     int not_used_on_this_platform)
{
	static unsigned int current_set = 0;
	unsigned long flag;
	unsigned long st40_clk = ST40_CLK;
	unsigned long l_p_j = _1_ms_lpj();

	cpufreq_debug_printk(CPUFREQ_DEBUG_DRIVER,
		"st_cpufreq_update_clocks", "\n");
	l_p_j >>= 3;	/* l_p_j = 125 usec (for each HZ) */

	local_irq_save(flag);

	if (set > current_set) {	/* down scaling... */
		/* it scales l_p_j based on the new frequency */
		l_p_j >>= 1;	/* 450 -> 225 or 225 -> 112.5 */
		if ((set + current_set) == 2)
			l_p_j >>= 1;	/* 450 -> 112.5 */
	} else {
		/* it scales l_p_j based on the new frequency */
		l_p_j <<= 1;	/* 225   -> 450 or 112.5 -> 225 */
		if ((set + current_set) == 2)
			l_p_j <<= 1;	/* 112.5 -> 450 */
	}

	asm volatile (".balign  32      \n"
		      "mov.l    %1, @%0\n"
		      "tst      %2, %2  \n"
		      "2:               \n"
		      "bf/s     2b      \n"
		      " dt      %2      \n"
		::    "r" (st40_clk),
		      "r" (st40_ratios[set]),
		      "r" (l_p_j)
		:     "memory", "t");

	current_set = set;
	sh4_clk->rate = (cpu_freqs[set].frequency << 3) * 125;
	local_irq_restore(flag);
}

#ifdef CONFIG_STM_CPU_FREQ_OBSERVE
static void __init st_cpufreq_observe_init(void)
{
	static struct sysconf_field *sc;
	/* route the sh4/2  clock frequenfy */
	iowrite32(0xc, clk_iomem + CKGA_CLKOBS_MUX1_CFG);
	stpio_request_set_pin(3, 2, "clkA dbg", STPIO_ALT_OUT, 1);
	sc = sysconf_claim(SYS_CFG, 19, 22, 23, "clkA dbg");
	sysconf_write(sc, 11);
}
#endif

static int __init st_cpufreq_platform_init(void)
{
	return 0;
}
