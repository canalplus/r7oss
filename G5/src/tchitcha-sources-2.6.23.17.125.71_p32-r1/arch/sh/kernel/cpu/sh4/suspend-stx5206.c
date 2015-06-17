/*
 * -------------------------------------------------------------------------
 * <linux_root>/arch/sh/kernel/cpu/sh4/suspend-stx5206.c
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

#include "clock-regs-stx5206.h"

/* *************************
 * STANDBY INSTRUCTION TABLE
 * *************************
 */
static unsigned long stx5206_standby_table[] __cacheline_aligned = {
/* 1. Move all the clock on OSC */
CLK_POKE(CKGA_CLKOPSRC_SWITCH_CFG, 0x0),
CLK_POKE(CKGA_OSC_DIV_CFG(0x5), 29), /* ic_if_100 @ 1 MHz to be safe for Lirc*/

IMMEDIATE_DEST(0x1f),
/* reduces OSC_st40 */
CLK_STORE(CKGA_OSC_DIV_CFG(4)),
/* reduces OSC_clk_ic */
CLK_STORE(CKGA_OSC_DIV_CFG(0x0)),
/* END. */
_END(),

DATA_LOAD(0x0),
CLK_STORE(CKGA_CLKOPSRC_SWITCH_CFG),
DATA_LOAD(0x2),
CLK_STORE(CKGA_OSC_DIV_CFG(0x0)),
DATA_LOAD(0x3),
CLK_STORE(CKGA_OSC_DIV_CFG(5)),
/* END. */
_END()
};


#define SYSCONF(x)	((x * 0x4) + 0x100)
#define SYSSTA(x)	((x * 0x4) + 0x8)
/* *********************
 * MEM INSTRUCTION TABLE
 * *********************
 */
static unsigned long stx5206_mem_table[] __cacheline_aligned = {
/* 1. Enables the DDR self refresh mode */
SYS_OR_LONG(SYSCONF(38), (1 << 20)),
/* waits until the ack bit is zero */
SYS_WHILE_NEQ(SYSSTA(4), 1, 1),
/* 1.1 Turn-off the ClockGenD */
SYS_OR_LONG(SYSCONF(11), (1 << 12)),

IMMEDIATE_DEST(0x1f),
/* reduces OSC_st40 */
CLK_STORE(CKGA_OSC_DIV_CFG(4)),
/* reduces OSC_clk_ic */
CLK_STORE(CKGA_OSC_DIV_CFG(0x0)),
/* reduces OSC_clk_ic_if_200 */
CLK_STORE(CKGA_OSC_DIV_CFG(17)),
/* 2. Move all the clock on OSC */

CLK_POKE(CKGA_OSC_DIV_CFG(5), 29), /* ic_if_100 @ 1MHz to be safe for Lirc*/

IMMEDIATE_DEST(0x0),
CLK_STORE(CKGA_CLKOPSRC_SWITCH_CFG),
CLK_STORE(CKGA_CLKOPSRC_SWITCH_CFG2),
/* PLLs in power down */
CLK_OR_LONG(CKGA_POWER_CFG, 0x3),
 /* END. */
_END(),

/* Turn-on the PLLs */
CLK_AND_LONG(CKGA_POWER_CFG, ~3),
/* Wait PLLS lock */
CLK_WHILE_NEQ(CKGA_PLL0_CFG, CKGA_PLL_CFG_LOCK, CKGA_PLL_CFG_LOCK),
CLK_WHILE_NEQ(CKGA_PLL1_CFG, CKGA_PLL_CFG_LOCK, CKGA_PLL_CFG_LOCK),

/* 1. Turn-on the LMI ClocksGenD */
SYS_AND_LONG(SYSCONF(11), ~(1 << 12)),
/* Wait LMI ClocksGenD lock */
SYS_WHILE_NEQ(SYSSTA(4), 1, 1),

/* 2. Disables the DDR self refresh mode */
SYS_AND_LONG(SYSCONF(38), ~(1 << 20)),
/* waits until the ack bit is zero */
SYS_WHILE_NEQ(SYSSTA(4), 1, 0),

IMMEDIATE_DEST(0x10000),
CLK_STORE(CKGA_PLL0LS_DIV_CFG(4)),

/* 3. Restore the previous clocks setting */
DATA_LOAD(0x0),
CLK_STORE(CKGA_CLKOPSRC_SWITCH_CFG),
DATA_LOAD(0x1),
CLK_STORE(CKGA_CLKOPSRC_SWITCH_CFG2),
DATA_LOAD(0x3),
CLK_STORE(CKGA_OSC_DIV_CFG(5)),
DATA_LOAD(0x2),
CLK_STORE(CKGA_OSC_DIV_CFG(0x0)),
DATA_LOAD(0x4),
CLK_STORE(CKGA_OSC_DIV_CFG(17)),

_DELAY(),
_DELAY(),
_DELAY(),
_END()
};

static unsigned long stx5206_wrt_table[5] __cacheline_aligned;

static int stx5206_suspend_prepare(suspend_state_t state)
{
	stx5206_wrt_table[0] = /* swith config */
		   ioread32(CKGA_BASE_ADDRESS + CKGA_CLKOPSRC_SWITCH_CFG);
	stx5206_wrt_table[1] = /* swith config 1 */
		   ioread32(CKGA_BASE_ADDRESS + CKGA_CLKOPSRC_SWITCH_CFG2);
	stx5206_wrt_table[2] = /* clk_STNoc */
		   ioread32(CKGA_BASE_ADDRESS + CKGA_OSC_DIV_CFG(0));
	stx5206_wrt_table[3] = /* clk_ic_if_100 */
		   ioread32(CKGA_BASE_ADDRESS + CKGA_OSC_DIV_CFG(5));
	stx5206_wrt_table[4] = /* clk_ic_if_200 */
		   ioread32(CKGA_BASE_ADDRESS + CKGA_OSC_DIV_CFG(17));
	return 0;
}

static unsigned long stx5206_iomem[4] __cacheline_aligned = {
		stx5206_wrt_table,
		CKGA_BASE_ADDRESS,
		0, /* no clock Gen B */
		SYSCFG_BASE_ADDRESS
};

static int stx5206_evt_to_irq(unsigned long evt)
{
	return ((evt < 0x400) ? ilc2irq(evt) : evt2irq(evt));
}

static struct sh4_suspend_t stx5206_suspend_data __cacheline_aligned = {
	.iobase = stx5206_iomem,
	.ops.prepare = stx5206_suspend_prepare,
	.evt_to_irq = stx5206_evt_to_irq,

	.stby_tbl = (unsigned long)stx5206_standby_table,
	.stby_size = DIV_ROUND_UP(ARRAY_SIZE(stx5206_standby_table) *
			sizeof(long), L1_CACHE_BYTES),

	.mem_tbl = (unsigned long)stx5206_mem_table,
	.mem_size = DIV_ROUND_UP(ARRAY_SIZE(stx5206_mem_table) * sizeof(long),
			L1_CACHE_BYTES),
	.wrt_tbl = (unsigned long)stx5206_wrt_table,
	.wrt_size = DIV_ROUND_UP(ARRAY_SIZE(stx5206_wrt_table) * sizeof(long),
			L1_CACHE_BYTES),
};

static int __init suspend_platform_setup(void)
{
	int i;
	struct sysconf_field *sc[4];
	sc[0] = sysconf_claim(SYS_CFG, 38, 20, 20, "pm");
	sc[1] = sysconf_claim(SYS_CFG, 11, 12, 12, "pm");
	sc[2] = sysconf_claim(SYS_STA, 4, 0, 0, "pm");
	sc[3] = sysconf_claim(SYS_STA, 3, 0, 0, "pm");

	for (i = 0; i < ARRAY_SIZE(sc); ++i)
		if (!sc[i])
			goto error;

	return sh4_suspend_register(&stx5206_suspend_data);
error:
	printk(KERN_ERR "[STM][PM] Error to acquire the sysconf registers\n");
	for (i = 0; i > ARRAY_SIZE(sc); ++i)
		if (sc[i])
			sysconf_release(sc[i]);

	return -1;
}

late_initcall(suspend_platform_setup);
