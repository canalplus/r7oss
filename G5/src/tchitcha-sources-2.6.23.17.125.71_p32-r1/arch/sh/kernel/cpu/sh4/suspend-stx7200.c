/*
 * -------------------------------------------------------------------------
 * <linux_root>/arch/sh/kernel/cpu/sh4/suspend-stx7200.c
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
#include <linux/delay.h>
#include <linux/irqflags.h>
#include <linux/stm/sysconf.h>
#include <linux/stm/pm.h>
#include <asm/system.h>
#include <asm/io.h>
#include <asm/pm.h>
#include <asm/irq-ilc.h>

#include "./soc-stx7200.h"

#define _SYS_STA4			(3)
#define _SYS_STA4_MASK			(4)
#define _SYS_STA6			(5)
#define _SYS_STA6_MASK			(6)

/* To powerdown the LMIs */
#define _SYS_CFG38			(7)
#define _SYS_CFG38_MASK			(8)
#define _SYS_CFG39			(9)
#define _SYS_CFG39_MASK			(10)

/* *************************
 * STANDBY INSTRUCTION TABLE
 * *************************
 */
static unsigned long stx7200_standby_table[] __cacheline_aligned = {
/* Down scale the GenA.Pll0 and GenA.Pll2*/
CLK_OR_LONG(CLKA_PLL0, CLKA_PLL0_BYPASS),
CLK_OR_LONG(CLKA_PLL2, CLKA_PLL2_BYPASS),

CLK_OR_LONG(CLKA_PWR_CFG, PWR_CFG_PLL0_OFF | PWR_CFG_PLL2_OFF),
#if 0
CLK_AND_LONG(CLKA_PLL0, ~(0x7ffff)),
CLK_AND_LONG(CLKA_PLL2, ~(0x7ffff)),

CLK_OR_LONG(CLKA_PLL0, CLKA_PLL0_SUSPEND),
CLK_OR_LONG(CLKA_PLL2, CLKA_PLL2_SUSPEND),

CLK_AND_LONG(CLKA_PWR_CFG, ~(PWR_CFG_PLL0_OFF | PWR_CFG_PLL2_OFF)),

CLK_AND_LONG(CLKA_PLL0, ~(CLKA_PLL0_BYPASS)),
CLK_AND_LONG(CLKA_PLL2, ~(CLKA_PLL2_BYPASS)),
#endif
/* END. */
_END(),

/* Restore the GenA.Pll0 and GenA.PLL2 original frequencies */
#if 0
CLK_OR_LONG(CLKA_PLL0, CLKA_PLL0_BYPASS),
CLK_OR_LONG(CLKA_PLL2, CLKA_PLL2_BYPASS),

CLK_OR_LONG(CLKA_PWR_CFG, PWR_CFG_PLL0_OFF | PWR_CFG_PLL2_OFF),

DATA_LOAD(0x0),
IMMEDIATE_SRC0(CLKA_PLL0_BYPASS),
_OR(),
CLK_STORE(CLKA_PLL0),

DATA_LOAD(0x1),
IMMEDIATE_SRC0(CLKA_PLL2_BYPASS),
_OR(),
CLK_STORE(CLKA_PLL2),
#endif
CLK_AND_LONG(CLKA_PWR_CFG, ~(PWR_CFG_PLL0_OFF | PWR_CFG_PLL2_OFF)),
CLK_WHILE_NEQ(CLKA_PLL0, CLKA_PLL0_LOCK, CLKA_PLL0_LOCK),
CLK_WHILE_NEQ(CLKA_PLL2, CLKA_PLL2_LOCK, CLKA_PLL2_LOCK),
CLK_AND_LONG(CLKA_PLL0, ~(CLKA_PLL0_BYPASS)),
CLK_AND_LONG(CLKA_PLL2, ~(CLKA_PLL2_BYPASS)),

_DELAY(),
/* END. */
_END()
};

/* *********************
 * MEM INSTRUCTION TABLE
 * *********************
 */

static unsigned long stx7200_mem_table[] __cacheline_aligned = {
/* 1. Enables the DDR self refresh mode */
DATA_OR_LONG(_SYS_CFG38, _SYS_CFG38_MASK),
DATA_OR_LONG(_SYS_CFG39, _SYS_CFG39_MASK),
	/* waits until the ack bit is zero */
DATA_WHILE_NEQ(_SYS_STA4, _SYS_STA4_MASK, _SYS_STA4_MASK),
DATA_WHILE_NEQ(_SYS_STA6, _SYS_STA6_MASK, _SYS_STA6_MASK),

 /* waits until the ack bit is zero */
/* 2. Down scale the GenA.Pll0, GenA.Pll1 and GenA.Pll2*/
CLK_OR_LONG(CLKA_PLL0, CLKA_PLL0_BYPASS),
CLK_OR_LONG(CLKA_PLL1, CLKA_PLL1_BYPASS),
CLK_OR_LONG(CLKA_PLL2, CLKA_PLL2_BYPASS),

CLK_OR_LONG(CLKA_PWR_CFG, PWR_CFG_PLL0_OFF | PWR_CFG_PLL1_OFF | PWR_CFG_PLL2_OFF),
#if 0
CLK_AND_LONG(CLKA_PLL0, ~(0x7ffff)),
CLK_AND_LONG(CLKA_PLL1, ~(0x7ffff)),
CLK_AND_LONG(CLKA_PLL2, ~(0x7ffff)),

CLK_OR_LONG(CLKA_PLL0, CLKA_PLL0_SUSPEND),
CLK_OR_LONG(CLKA_PLL1, CLKA_PLL1_SUSPEND),
CLK_OR_LONG(CLKA_PLL2, CLKA_PLL2_SUSPEND),

CLK_AND_LONG(CLKA_PWR_CFG, ~(PWR_CFG_PLL0_OFF | PWR_CFG_PLL1_OFF | PWR_CFG_PLL2_OFF)),

CLK_WHILE_NEQ(CLKA_PLL0, CLKA_PLL0_LOCK, CLKA_PLL0_LOCK),
CLK_WHILE_NEQ(CLKA_PLL1, CLKA_PLL1_LOCK, CLKA_PLL1_LOCK),
CLK_WHILE_NEQ(CLKA_PLL2, CLKA_PLL2_LOCK, CLKA_PLL2_LOCK),

CLK_AND_LONG(CLKA_PLL0, ~(CLKA_PLL0_BYPASS)),
CLK_AND_LONG(CLKA_PLL1, ~(CLKA_PLL1_BYPASS)),
CLK_AND_LONG(CLKA_PLL2, ~(CLKA_PLL2_BYPASS)),
#endif
/* END. */
_END() ,

/* Restore the GenA.Pll0 and GenA.PLL2 original frequencies */
#if 0
CLK_OR_LONG(CLKA_PLL0, CLKA_PLL0_BYPASS),
CLK_OR_LONG(CLKA_PLL1, CLKA_PLL1_BYPASS),
CLK_OR_LONG(CLKA_PLL2, CLKA_PLL2_BYPASS),

CLK_OR_LONG(CLKA_PWR_CFG, PWR_CFG_PLL0_OFF | PWR_CFG_PLL1_OFF | PWR_CFG_PLL2_OFF),

DATA_LOAD(0x0),
IMMEDIATE_SRC0(CLKA_PLL0_BYPASS),
_OR(),
CLK_STORE(CLKA_PLL0),

DATA_LOAD(0x1),
IMMEDIATE_SRC0(CLKA_PLL1_BYPASS),
_OR(),
CLK_STORE(CLKA_PLL1),

DATA_LOAD(0x2),
IMMEDIATE_SRC0(CLKA_PLL2_BYPASS),
_OR(),
CLK_STORE(CLKA_PLL2),
#endif
CLK_AND_LONG(CLKA_PWR_CFG, ~(PWR_CFG_PLL0_OFF | PWR_CFG_PLL1_OFF | PWR_CFG_PLL2_OFF)),
/* Wait PLLs lock */
CLK_WHILE_NEQ(CLKA_PLL0, CLKA_PLL0_LOCK, CLKA_PLL0_LOCK),
CLK_WHILE_NEQ(CLKA_PLL1, CLKA_PLL1_LOCK, CLKA_PLL1_LOCK),
CLK_WHILE_NEQ(CLKA_PLL2, CLKA_PLL2_LOCK, CLKA_PLL2_LOCK),

CLK_AND_LONG(CLKA_PLL0, ~(CLKA_PLL0_BYPASS)),
CLK_AND_LONG(CLKA_PLL1, ~(CLKA_PLL1_BYPASS)),
CLK_AND_LONG(CLKA_PLL2, ~(CLKA_PLL2_BYPASS)),

DATA_AND_NOT_LONG(_SYS_CFG38, _SYS_CFG38_MASK),
DATA_AND_NOT_LONG(_SYS_CFG39, _SYS_CFG39_MASK),
DATA_WHILE_EQ(_SYS_STA4, _SYS_STA4_MASK, _SYS_STA4_MASK),

/* wait until the ack bit is high        */
DATA_WHILE_EQ(_SYS_STA6, _SYS_STA6_MASK, _SYS_STA6_MASK),

_DELAY(),
_DELAY(),
_DELAY(),
/* END. */
_END()
};

static unsigned long stx7200_wrt_table[16] __cacheline_aligned;

static int stx7200_suspend_prepare(suspend_state_t state)
{
	if (state == PM_SUSPEND_STANDBY) {
		stx7200_wrt_table[0] =
			readl(CLOCKGEN_BASE_ADDR + CLKA_PLL0) & 0x7ffff;
		stx7200_wrt_table[1] =
			readl(CLOCKGEN_BASE_ADDR + CLKA_PLL2) & 0x7ffff;
	} else {
		stx7200_wrt_table[0] =
			readl(CLOCKGEN_BASE_ADDR + CLKA_PLL0) & 0x7ffff;
		stx7200_wrt_table[1] =
			readl(CLOCKGEN_BASE_ADDR + CLKA_PLL1) & 0x7ffff;
		stx7200_wrt_table[2] =
			readl(CLOCKGEN_BASE_ADDR + CLKA_PLL2) & 0x7ffff;
	}
	return 0;
}


static unsigned long stx7200_iomem[2] __cacheline_aligned = {
		stx7200_wrt_table,	/* To access Sysconf    */
		CLOCKGEN_BASE_ADDR};	/* Clockgen A */

static int stx7200_evttoirq(unsigned long evt)
{
	return ((evt < 0x400) ? ilc2irq(evt) : evt2irq(evt));
}

static struct sh4_suspend_t st40data __cacheline_aligned = {
	.iobase = stx7200_iomem,
	.ops.prepare = stx7200_suspend_prepare,
	.evt_to_irq = stx7200_evttoirq,

	.stby_tbl = (unsigned long)stx7200_standby_table,
	.stby_size = DIV_ROUND_UP(ARRAY_SIZE(stx7200_standby_table) *
			sizeof(long), L1_CACHE_BYTES),

	.mem_tbl = (unsigned long)stx7200_mem_table,
	.mem_size = DIV_ROUND_UP(ARRAY_SIZE(stx7200_mem_table) * sizeof(long),
			L1_CACHE_BYTES),
	.wrt_tbl = (unsigned long)stx7200_wrt_table,
	.wrt_size = DIV_ROUND_UP(ARRAY_SIZE(stx7200_wrt_table) * sizeof(long),
			L1_CACHE_BYTES),
};

static int __init suspend_platform_setup()
{
	struct sysconf_field* sc;

	sc = sysconf_claim(SYS_STA, 4, 0, 0, "pm");
	stx7200_wrt_table[_SYS_STA4] = (unsigned long)sysconf_address(sc);
	stx7200_wrt_table[_SYS_STA4_MASK] = sysconf_mask(sc);

	sc = sysconf_claim(SYS_STA, 6, 0, 0, "pm");
	stx7200_wrt_table[_SYS_STA6] = (unsigned long)sysconf_address(sc);
	stx7200_wrt_table[_SYS_STA6_MASK] = sysconf_mask(sc);

	sc = sysconf_claim(SYS_CFG, 38, 20, 20, "pm");
	stx7200_wrt_table[_SYS_CFG38] = (unsigned long)sysconf_address(sc);
	stx7200_wrt_table[_SYS_CFG38_MASK] = sysconf_mask(sc);

	sc = sysconf_claim(SYS_CFG, 39, 20, 20, "pm");
	stx7200_wrt_table[_SYS_CFG39] = (unsigned long)sysconf_address(sc);
	stx7200_wrt_table[_SYS_CFG39_MASK] = sysconf_mask(sc);

#ifdef CONFIG_PM_DEBUG
	ctrl_outl(0xc, CKGA_CLKOUT_SEL +
		CLOCKGEN_BASE_ADDR); /* sh4:2 routed on SYSCLK_OUT */
#endif
	return sh4_suspend_register(&st40data);
}

late_initcall(suspend_platform_setup);
