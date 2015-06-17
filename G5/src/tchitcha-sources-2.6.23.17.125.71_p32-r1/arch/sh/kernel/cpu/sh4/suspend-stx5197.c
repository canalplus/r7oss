/*
 * -------------------------------------------------------------------------
 * <linux_root>/arch/sh/kernel/cpu/sh4/suspend-stx5197.c
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
#include <linux/stm/pm.h>
#include <linux/stm/sysconf.h>
#include <asm/system.h>
#include <asm/io.h>
#include <asm/pm.h>
#include <asm/irq.h>
#include <asm/irq-ilc.h>

#include "soc-stx5197.h"

/*
 * System Service Finite State Machine
 *	+-------+   +------+    +------+
 *	| reset |-->|  X1  |<-->| Prog |
 *	+-------+   +------+    +------+
 *	    	       /\	   |
 *	    		|	   \/
 *		wakeup	|       +-------+
 *		event	+-------|Standby|
 *			        +-------+
 */

/* *************************
 * STANDBY INSTRUCTION TABLE
 * *************************
 */
static unsigned long stx5197_standby_table[] __cacheline_aligned = {
CLK_POKE(CLK_LOCK_CFG, 0xf0),
CLK_POKE(CLK_LOCK_CFG, 0x0f), /* UnLock the clocks */

CLK_POKE(CLK_MODE_CTRL, CLK_MODE_CTRL_X1),

/* 1. Move all the clock on OSC */
CLK_OR_LONG(CLK_REDUCED_PM_CTRL, CLK_REDUCED_ON_XTAL_STDBY),
CLK_OR_LONG(CLK_PLL_CONFIG1(0), CLK_PLL_CONFIG1_POFF),
CLK_POKE(CLK_MODE_CTRL, CLK_MODE_CTRL_PROG),
CLK_POKE(CLK_LOCK_CFG, 0x100), /* Lock the clocks */
/* END. */
_END(),

CLK_POKE(CLK_LOCK_CFG, 0xf0),
CLK_POKE(CLK_LOCK_CFG, 0x0f), /* UnLock the clocks */
CLK_POKE(CLK_MODE_CTRL, CLK_MODE_CTRL_X1),
CLK_AND_LONG(CLK_REDUCED_PM_CTRL, ~CLK_REDUCED_ON_XTAL_STDBY),
CLK_AND_LONG(CLK_PLL_CONFIG1(0), ~CLK_PLL_CONFIG1_POFF),
CLK_WHILE_NEQ(CLK_PLL_CONFIG1(0), CLK_PLL_CONFIG1_LOCK, CLK_PLL_CONFIG1_LOCK),
CLK_POKE(CLK_MODE_CTRL, CLK_MODE_CTRL_PROG),
CLK_POKE(CLK_LOCK_CFG, 0x100), /* Lock the clocks */
_DELAY(),
_DELAY(),
_DELAY(),
_END()
};

/* *********************
 * MEM INSTRUCTION TABLE
 * *********************
 */
#define _CFG_CTRL_H		0x14
#define _CFG_MONITOR_J		0x3c

#define LMI_CONTROL		0x10
#define LMI_BURST_REFRESH	2
#define LMI_SCR_CST		100

static unsigned long stx5197_mem_table[] __cacheline_aligned = {
CLKB_POKE(LMI_CONTROL, (0x4 | (LMI_BURST_REFRESH << 4) | (LMI_SCR_CST << 16))),
SYS_OR_LONG(_CFG_CTRL_H, (1 << 26)),
SYS_WHILE_NEQ(_CFG_MONITOR_J, (1 << 24), (1 << 24)),

CLK_POKE(CLK_LOCK_CFG, 0xf0),
CLK_POKE(CLK_LOCK_CFG, 0x0f), /* UnLock the clocks */

/* disable PLLs in standby */
CLK_OR_LONG(CLK_LP_MODE_DIS0, CLK_LP_MODE_DIS0_VALUE),
CLK_POKE(CLK_MODE_CTRL, CLK_MODE_CTRL_STDB), /* IN STANDBY */

_END_NO_SLEEP(),
/*
 * On a wakeup Event the System Service goes directly in X1 mode */
CLK_AND_LONG(CLK_PLL_CONFIG1(0), ~CLK_PLL_CONFIG1_POFF),
CLK_AND_LONG(CLK_PLL_CONFIG1(1), ~CLK_PLL_CONFIG1_POFF),
/* Wait PLLs lock */
CLK_WHILE_NEQ(CLK_PLL_CONFIG1(0), CLK_PLL_CONFIG1_LOCK, CLK_PLL_CONFIG1_LOCK),
CLK_WHILE_NEQ(CLK_PLL_CONFIG1(1), CLK_PLL_CONFIG1_LOCK, CLK_PLL_CONFIG1_LOCK),

CLK_POKE(CLK_MODE_CTRL, CLK_MODE_CTRL_PROG),
CLK_POKE(CLK_LOCK_CFG, 0x100), /* Lock the clocks */

_DELAY(),
_DELAY(),
SYS_AND_LONG(_CFG_CTRL_H, ~(1 << 26)),
SYS_WHILE_NEQ(_CFG_MONITOR_J, (1 << 24), 0),


_DELAY(),
_DELAY(),
_DELAY(),
_DELAY(),
CLKB_POKE(LMI_CONTROL, 0),
_DELAY(),
_DELAY(),
_DELAY(),
_DELAY(),

_END()
};

static unsigned long stx5197_wrt_table[8] __cacheline_aligned;

static int stx5197_suspend_prepare(suspend_state_t state)
{
	return 0;
}

static unsigned long stx5197_iomem[] __cacheline_aligned = {
		stx5197_wrt_table,
		SYS_SERV_BASE_ADDR,
		0xFE000000,	/* LMI base register */
		0xfd901000,	/* High Density group */
};

static int stx5197_evt_to_irq(unsigned long evt)
{
	return ((evt < 0x400) ? ilc2irq(evt) : evt2irq(evt));
}

static struct sh4_suspend_t st40data __cacheline_aligned = {
	.iobase = stx5197_iomem,
	.ops.prepare = stx5197_suspend_prepare,
	.evt_to_irq = stx5197_evt_to_irq,

	.stby_tbl = (unsigned long)stx5197_standby_table,
	.stby_size = DIV_ROUND_UP(ARRAY_SIZE(stx5197_standby_table) *
			sizeof(long), L1_CACHE_BYTES),

	.mem_tbl = (unsigned long)stx5197_mem_table,
	.mem_size = DIV_ROUND_UP(ARRAY_SIZE(stx5197_mem_table) * sizeof(long),
			L1_CACHE_BYTES),
	.wrt_tbl = (unsigned long)stx5197_wrt_table,
	.wrt_size = DIV_ROUND_UP(ARRAY_SIZE(stx5197_wrt_table) * sizeof(long),
			L1_CACHE_BYTES),
};

static int __init suspend_platform_setup(void)
{

	struct sysconf_field* sc;

	sc = sysconf_claim(CFG_MONITOR_J, 24, 24, "LMI pwd ack");
	sc = sysconf_claim(CFG_CTRL_H, 26, 26, "LMI pwd req");

	return sh4_suspend_register(&st40data);
}

late_initcall(suspend_platform_setup);
