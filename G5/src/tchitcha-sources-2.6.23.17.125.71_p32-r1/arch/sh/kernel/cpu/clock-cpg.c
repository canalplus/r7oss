/*
 * arch/sh/kernel/cpu/clock-cpg.c - SuperH clock framework for CPG style clocks
 *
 *  Copyright (C) 2005  Paul Mundt
 *
 * This clock framework is derived from the OMAP version by:
 *
 *	Copyright (C) 2004 Nokia Corporation
 *	Written by Tuukka Tikkanen <tuukka.tikkanen@elektrobit.com>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 */
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/list.h>
#include <linux/kref.h>
#include <linux/seq_file.h>
#include <linux/err.h>
#include <asm/clock.h>
#include <asm/timer.h>

/*
 * Each subtype is expected to define the init routines for these clocks,
 * as each subtype (or processor family) will have these clocks at the
 * very least. These are all provided through the CPG, which even some of
 * the more quirky parts (such as ST40, SH4-202, etc.) still have.
 *
 * The processor-specific code is expected to register any additional
 * clock sources that are of interest.
 */
static struct clk master_clk = {
	.name		= "master_clk",
	.flags		= CLK_ALWAYS_ENABLED | CLK_RATE_PROPAGATES,
	.rate		= CONFIG_SH_PCLK_FREQ,
};

static struct clk module_clk = {
	.name		= "module_clk",
	.parent		= &master_clk,
	.flags		= CLK_ALWAYS_ENABLED | CLK_RATE_PROPAGATES,
};

static struct clk bus_clk = {
	.name		= "bus_clk",
	.parent		= &master_clk,
	.flags		= CLK_ALWAYS_ENABLED | CLK_RATE_PROPAGATES,
};

static struct clk cpu_clk = {
	.name		= "cpu_clk",
	.parent		= &master_clk,
	.flags		= CLK_ALWAYS_ENABLED,
};

/*
 * The ordering of these clocks matters, do not change it.
 */
static struct clk *onchip_clocks[] = {
	&master_clk,
	&module_clk,
	&bus_clk,
	&cpu_clk,
};

void __init __attribute__ ((weak))
arch_init_clk_ops(struct clk_ops **ops, int type)
{
}

void __init __attribute__ ((weak))
arch_clk_init(void)
{
}

int __init clk_init(void)
{
	int i, ret = 0;

	BUG_ON(!master_clk.rate);

	for (i = 0; i < ARRAY_SIZE(onchip_clocks); i++) {
		struct clk *clk = onchip_clocks[i];

		arch_init_clk_ops(&clk->ops, i);
		ret |= clk_register(clk);
	}

	arch_clk_init();

	/* Kick the child clocks.. */
	clk_set_rate(&master_clk, clk_get_rate(&master_clk));
	clk_put(&master_clk);

	return ret;
}
