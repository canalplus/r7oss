/*
 * arch/sh/kernel/cpu/sh4/cpufreq-stx7108.c
 *
 * Cpufreq driver for the ST40 processors.
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
#include <linux/cpumask.h>
#include <linux/smp.h>
#include <linux/sched.h>	/* set_cpus_allowed() */
#include <linux/stm/clk.h>

static struct clk *cpu_clk;	/* the real clock as defined in the lla */

static void st_cpufreq_update_clocks(unsigned int set,
				     int not_used_on_this_platform)
{
	static unsigned int current_set;
	int right = 1, shift = 1;
	unsigned long rate = clk_get_rate(cpu_clk);

	if (set == current_set)
		return;

	if ((set + current_set) == 2)
		shift = 2;

	if (set > current_set)
		right = 0;

	if (right)
		rate >>= shift;
	else
		rate <<= shift;

	clk_set_rate(cpu_clk, rate);
	sh4_clk->rate = rate;
	current_set = set;
}

#ifdef CONFIG_STM_CPU_FREQ_OBSERVE
static void __init st_cpufreq_observe_init(void)
{
	unsigned long div = 2;
	clk_observe(cpu_clk, &div);
}
#endif

static int __init st_cpufreq_platform_init(void)
{
	cpu_clk = clk_get(NULL, "CLKA_SH4_ICK");

	if (!cpu_clk) {
		printk(KERN_ERR" CPU clock not fund!\n");
		return -EINVAL;
	}

	return 0;
}
