/*
 * -------------------------------------------------------------------------
 * <linux_root>/arch/sh/kernel/cpu/sh4/suspend-stx7111.c
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

#include "./soc-stx7111.h"

#define _SYS_STA4			(5)
#define _SYS_STA4_MASK			(6)
#define _SYS_STA3			(11)
#define _SYS_STA3_MASK			(12)
#define _SYS_STA3_VALUE			(13)

#define _SYS_CFG11			(7)
#define _SYS_CFG11_MASK			(8)

#define _SYS_CFG38			(9)
#define _SYS_CFG38_MASK			(10)

/* *************************
 * STANDBY INSTRUCTION TABLE
 * *************************
 */
static unsigned long stx7111_standby_table[] __cacheline_aligned = {
/* 1. Move all the clock on OSC */
CLK_POKE(CKGA_CLKOPSRC_SWITCH_CFG(0x0), 0x0),
CLK_POKE(CKGA_OSC_DIV_CFG(5), 29), /* clk_ic_if_100 @ 1 MHz to be safe for lirc */

IMMEDIATE_DEST(0x1f),
/* reduces the st40 frequency */
CLK_STORE(CKGA_OSC_DIV_CFG(4)),
/* reduces the clk_STNoc_ic */
CLK_STORE(CKGA_OSC_DIV_CFG(0x0)),

 /* END. */
_END(),

IMMEDIATE_DEST(0x10000),
CLK_STORE(CKGA_PLL0LS_DIV_CFG(4)),
DATA_LOAD(0x0),
CLK_STORE(CKGA_CLKOPSRC_SWITCH_CFG(0x0)),
DATA_LOAD(0x1),
CLK_STORE(CKGA_OSC_DIV_CFG(0x0)),
DATA_LOAD(0x2),
CLK_STORE(CKGA_OSC_DIV_CFG(5)),

 /* END. */
_END()
};

/* *********************
 * MEM INSTRUCTION TABLE
 * *********************
 */
static unsigned long stx7111_mem_table[] __cacheline_aligned = {
/* 1. Enables the DDR self refresh mode */
DATA_OR_LONG(_SYS_CFG38, _SYS_CFG38_MASK),
/* waits until the ack bit is zero */
DATA_WHILE_NEQ(_SYS_STA4, _SYS_STA4_MASK, _SYS_STA4_MASK),
/* 1.1 Turn-off the ClockGenD */
DATA_OR_LONG(_SYS_CFG11, _SYS_CFG11_MASK),

IMMEDIATE_DEST(0x1f),
CLK_STORE(CKGA_OSC_DIV_CFG(4)),   /* reduces the st40 frequency */
CLK_STORE(CKGA_OSC_DIV_CFG(17)),  /* reduces the clk_ic_if_200  */
CLK_STORE(CKGA_OSC_DIV_CFG(0x0)), /* reduces the clk_STNoc_ic   */

CLK_POKE(CKGA_OSC_DIV_CFG(5), 29), /* clk_ic_if_100 @ 1 MHz to be safe for lirc */

/* 2. Move all the clock on OSC */
IMMEDIATE_DEST(0x0),
CLK_STORE(CKGA_CLKOPSRC_SWITCH_CFG(0x0)),
CLK_STORE(CKGA_CLKOPSRC_SWITCH_CFG(0x1)),

/* PLLs in power down */
CLK_OR_LONG(CKGA_POWER_CFG, 0x3),
 /* END. */
_END(),

/* Turn-on the PLLs */
CLK_AND_LONG(CKGA_POWER_CFG, ~3),
/* Wait PLLs lock */
CLK_WHILE_NEQ(CKGA_PLL0_CFG, CKGA_PLL0_CFG_LOCK, CKGA_PLL0_CFG_LOCK),
CLK_WHILE_NEQ(CKGA_PLL1_CFG, CKGA_PLL1_CFG_LOCK, CKGA_PLL1_CFG_LOCK),

/* 1. Turn-on the LMI ClocksGenD */
DATA_AND_NOT_LONG(_SYS_CFG11, _SYS_CFG11_MASK),
/* Wait LMI ClocksGenD lock */
DATA_WHILE_NEQ(_SYS_STA3, _SYS_STA3_MASK, _SYS_STA3_VALUE),

/* 2. Disables the DDR self refresh mode */
DATA_AND_NOT_LONG(_SYS_CFG38, _SYS_CFG38_MASK),
/* waits until the ack bit is zero */
DATA_WHILE_EQ(_SYS_STA4, _SYS_STA4_MASK, _SYS_STA4_MASK),


IMMEDIATE_DEST(0x10000),
/* 3. Restore the previous clocks setting */
CLK_STORE(CKGA_OSC_DIV_CFG(4)),
DATA_LOAD(0x0),
CLK_STORE(CKGA_CLKOPSRC_SWITCH_CFG(0x0)),
DATA_LOAD(0x1),
CLK_STORE(CKGA_CLKOPSRC_SWITCH_CFG(0x1)),
DATA_LOAD(0x2),
CLK_STORE(CKGA_OSC_DIV_CFG(0x0)),
DATA_LOAD(0x3),
CLK_STORE(CKGA_OSC_DIV_CFG(5)),
DATA_LOAD(0x4),
CLK_STORE(CKGA_OSC_DIV_CFG(17)),

_DELAY(),
_DELAY(),
_DELAY(),
_END()
};

static unsigned long stx7111_wrt_table[16] __cacheline_aligned;

static int stx7111_suspend_prepare(suspend_state_t state)
{
	if (state == PM_SUSPEND_STANDBY) {
		stx7111_wrt_table[0] = /* swith config */
		   ioread32(CLOCKGENA_BASE_ADDR + CKGA_CLKOPSRC_SWITCH_CFG(0));
		stx7111_wrt_table[1] = /* clk_STNoc_ic */
		    ioread32(CLOCKGENA_BASE_ADDR + CKGA_OSC_DIV_CFG(0));
		stx7111_wrt_table[2] = /* clk_ic_if_100 */
		    ioread32(CLOCKGENA_BASE_ADDR + CKGA_OSC_DIV_CFG(5));
	} else {
		stx7111_wrt_table[0] = /* swith config */
		   ioread32(CLOCKGENA_BASE_ADDR + CKGA_CLKOPSRC_SWITCH_CFG(0));
		stx7111_wrt_table[1] = /* swith config 1 */
		   ioread32(CLOCKGENA_BASE_ADDR + CKGA_CLKOPSRC_SWITCH_CFG(1));
		stx7111_wrt_table[2] = /* clk_STNoc_ic */
		    ioread32(CLOCKGENA_BASE_ADDR + CKGA_OSC_DIV_CFG(0));
		stx7111_wrt_table[3] = /* clk_ic_if_100 */
		    ioread32(CLOCKGENA_BASE_ADDR + CKGA_OSC_DIV_CFG(5));
		stx7111_wrt_table[4] = /* clk_ic_if_200 */
		    ioread32(CLOCKGENA_BASE_ADDR + CKGA_OSC_DIV_CFG(17));
	}
	return 0;
}

static unsigned long stx7111_iomem[2] __cacheline_aligned = {
		stx7111_wrt_table,
		CLOCKGENA_BASE_ADDR};

static int stx7111_evttoirq(unsigned long evt)
{
	return ((evt < 0x400) ? ilc2irq(evt) : evt2irq(evt));
}

static struct sh4_suspend_t st40data __cacheline_aligned = {
	.iobase = stx7111_iomem,
	.ops.prepare = stx7111_suspend_prepare,
	.evt_to_irq = stx7111_evttoirq,

	.stby_tbl = (unsigned long)stx7111_standby_table,
	.stby_size = DIV_ROUND_UP(ARRAY_SIZE(stx7111_standby_table) *
			sizeof(long), L1_CACHE_BYTES),

	.mem_tbl = (unsigned long)stx7111_mem_table,
	.mem_size = DIV_ROUND_UP(ARRAY_SIZE(stx7111_mem_table) * sizeof(long),
			L1_CACHE_BYTES),
	.wrt_tbl = (unsigned long)stx7111_wrt_table,
	.wrt_size = DIV_ROUND_UP(ARRAY_SIZE(stx7111_wrt_table) * sizeof(long),
			L1_CACHE_BYTES),
};

static int __init suspend_platform_setup()
{
	struct sysconf_field* sc;
#ifdef CONFIG_PM_DEBUG
/* route the sh4/2 clock frequenfy */
	iowrite32(0xc, CLOCKGENA_BASE_ADDR + CKGA_CLKOBS_MUX1_CFG);
#endif

	sc = sysconf_claim(SYS_CFG, 38, 20, 20, "pm");
	stx7111_wrt_table[_SYS_CFG38]      = (unsigned long)sysconf_address(sc);
	stx7111_wrt_table[_SYS_CFG38_MASK] = sysconf_mask(sc);

	sc = sysconf_claim(SYS_CFG, 11, 12, 12, "pm");
	stx7111_wrt_table[_SYS_CFG11]      = (unsigned long)sysconf_address(sc);
	stx7111_wrt_table[_SYS_CFG11_MASK] = sysconf_mask(sc);

	sc = sysconf_claim(SYS_STA, 4, 0, 0, "pm");
	stx7111_wrt_table[_SYS_STA4]      = (unsigned long)sysconf_address(sc);
	stx7111_wrt_table[_SYS_STA4_MASK] = sysconf_mask(sc);

	sc = sysconf_claim(SYS_STA, 3, 0, 0, "pm");
	stx7111_wrt_table[_SYS_STA3] = (unsigned long)sysconf_address(sc);
	stx7111_wrt_table[_SYS_STA3_MASK] = sysconf_mask(sc);
	stx7111_wrt_table[_SYS_STA3_VALUE] = 0;
	return sh4_suspend_register(&st40data);
}

late_initcall(suspend_platform_setup);
