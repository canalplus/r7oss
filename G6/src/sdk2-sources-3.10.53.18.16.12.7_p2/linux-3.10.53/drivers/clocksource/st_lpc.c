/*
 * This driver implements a Clocksource using the Low Power Timer in
 * the Low Power Controller (LPC) in some STMicroelectronics
 * STi series SoCs
 *
 * Copyright (C) 2014 STMicroelectronics Limited
 * Author: Francesco Virlinzi <francesco.virlinzi@st.com>
 * Author: Ajit Pal Singh <ajitpal.singh@st.com>
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 */

#include <linux/clocksource.h>
#include <linux/init.h>
#include <linux/of_address.h>
#include <linux/slab.h>
#include <linux/clk.h>
#include <asm/sched_clock.h>

/* Low Power Timer */
#define LPC_LPT_LSB_OFF		0x400
#define LPC_LPT_MSB_OFF		0x404
#define LPC_LPT_START_OFF	0x408

struct st_lpc {
	struct clk *clk;
	void __iomem *iomem_cs;
};

static struct st_lpc *st_lpc;

static u64 notrace st_lpc_counter_read(void)
{
	u64 counter;
	u32 lower;
	u32 upper, old_upper;

	upper = readl_relaxed(st_lpc->iomem_cs + LPC_LPT_MSB_OFF);
	do {
		old_upper = upper;
		lower = readl_relaxed(st_lpc->iomem_cs + LPC_LPT_LSB_OFF);
		upper = readl_relaxed(st_lpc->iomem_cs + LPC_LPT_MSB_OFF);
	} while (upper != old_upper);

	counter = upper;
	counter <<= 32;
	counter |= lower;
	return counter;
}

static cycle_t st_lpc_clocksource_read(struct clocksource *cs)
{
	return st_lpc_counter_read();
}

static void st_lpc_clocksource_reset(struct clocksource *cs)
{
	writel(0, st_lpc->iomem_cs + LPC_LPT_START_OFF);
	writel(0, st_lpc->iomem_cs + LPC_LPT_MSB_OFF);
	writel(0, st_lpc->iomem_cs + LPC_LPT_LSB_OFF);
	writel(1, st_lpc->iomem_cs + LPC_LPT_START_OFF);
}

static struct clocksource st_lpc_clocksource = {
	.name   = "st-lpc clocksource",
	.rating = 301,
	.read   = st_lpc_clocksource_read,
	.mask   = CLOCKSOURCE_MASK(64),
	.flags  = CLOCK_SOURCE_IS_CONTINUOUS,
};

#ifdef CONFIG_CLKSRC_LPC_TIMER_SCHED_CLOCK
static u32 notrace st_lpc_sched_clock_read(void)
{
	return st_lpc_counter_read();
}
#endif

static int st_lpc_clocksource_init(void)
{
	unsigned long rate;

	st_lpc_clocksource_reset(&st_lpc_clocksource);

	rate = clk_get_rate(st_lpc->clk);
#ifdef CONFIG_CLKSRC_LPC_TIMER_SCHED_CLOCK
	setup_sched_clock(st_lpc_sched_clock_read, 32, rate);
#endif
	clocksource_register_hz(&st_lpc_clocksource, rate);

	return 0;
}

static int st_lpc_setup_clk(struct device_node *np)
{
	char *clk_name = "lpc_clk";
	struct clk *clk;
	int ret;

	clk = of_clk_get_by_name(np, clk_name);
	if (IS_ERR(clk)) {
		pr_err("st-lpc: unable to get lpc clock\n");
		ret = PTR_ERR(clk);
		return ret;
	}

	if (clk_prepare_enable(clk)) {
		pr_err("st-lpc: %s could not be enabled\n", clk_name);
		return -EINVAL;
	}

	if (!clk_get_rate(clk)) {
		pr_err("st-lpc: Unable to get clock rate\n");
		clk_disable_unprepare(clk);
		return -EINVAL;
	}

	pr_info("st-lpc: %s running @ %lu Hz\n",
		clk_name, clk_get_rate(clk));

	st_lpc->clk = clk;

	return 0;
}

static void __init st_lpc_of_register(struct device_node *np)
{
	st_lpc = kzalloc(sizeof(*st_lpc), GFP_KERNEL);
	if (!st_lpc) {
		pr_err("st-lpc: No memory available\n");
		return;
	}

	st_lpc->iomem_cs = of_iomap(np, 0);
	if (!st_lpc->iomem_cs) {
		pr_err("st-lpc: Unable to map iomem\n");
		goto err_0;
	}

	if (st_lpc_setup_clk(np))
		goto err_1;

	st_lpc_clocksource_init();

	pr_info("st-lpc: clocksource initialised: iomem: %p\n",
		st_lpc->iomem_cs);
	return;
err_1:
	iounmap(st_lpc->iomem_cs);
err_0:
	kfree(st_lpc);
	return;
}

CLOCKSOURCE_OF_DECLARE(st_lpc, "st,st_lpc_timer", st_lpc_of_register);
