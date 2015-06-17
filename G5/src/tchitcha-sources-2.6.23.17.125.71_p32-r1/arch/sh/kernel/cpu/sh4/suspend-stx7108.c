/*
 * -------------------------------------------------------------------------
 * <linux_root>/arch/sh/kernel/cpu/sh4/suspend-stx7108.c
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
#include <linux/io.h>

#include <asm/pm.h>
#include <asm/irq-ilc.h>

#include "clock-regs-stx7108.h"

#define SYS_1_BASE_ADDRESS	0xFDE20000

#define CKGA_PLL_CFG_LOCK	(1 << 31)


/*
 * The Stx7108 doesn't use the ST gpLMI
 * it uses the Synopsys IP Dram Controller
 * For registers description see:
 * 'DesignWare Cores DDR3/2 SDRAM Protocol - Controller -
 *  Databook - Version 2.10a - February 4, 2009'
 */
#define DDR3SS0_REG		0xFDE50000
#define DDR3SS1_REG		0xFDE70000

#define DDR_SCTL		0x4
#define  DDR_SCTL_CFG			0x1
#define  DDR_SCTL_GO			0x2
#define  DDR_SCTL_SLEEP			0x3
#define  DDR_SCTL_WAKEUP		0x4

#define DDR_STAT		0x8
#define  DDR_STAT_CONFIG		0x1
#define  DDR_STAT_ACCESS		0x3
#define  DDR_STAT_LOW_POWER		0x5

/*
 * the following macros are valid only for SYSConf_Bank_1
  * where there are the ClockGen_D management registers
 */
#define SYS_BNK1_STA(x)		(0x4 * (x))
#define SYS_BNK1_CFG(x)		(0x4 * (x) + 0x3c)
/* *************************
 * STANDBY INSTRUCTION TABLE
 * *************************
 */
static unsigned long stx7108_standby_table[] __cacheline_aligned = {

CLKB_POKE(CKGA_OSC_DIV_CFG(11), 29),  /* CLKA_IC_REG_LP_ON @ 1 MHz to
				       * be safe for lirc
				       */

/* 1. Move all the clocks on OSC */
CLK_POKE(CKGA_CLKOPSRC_SWITCH_CFG, 0x0),
CLK_POKE(CKGA_CLKOPSRC_SWITCH_CFG2, 0x0),
CLK_POKE(CKGA_CLKOPSRC_SWITCH_CFG, 0x0),
CLKB_POKE(CKGA_CLKOPSRC_SWITCH_CFG2, 0x0),
/* PLLs in power down */
CLK_OR_LONG(CKGA_POWER_CFG, 0x3),
CLKB_OR_LONG(CKGA_POWER_CFG, 0x3),

/* END. */
_END(),

/* Turn-on the PLLs */
CLK_AND_LONG(CKGA_POWER_CFG, ~3),
CLKB_AND_LONG(CKGA_POWER_CFG, ~3),
/* Wait PLLs lock */
CLK_WHILE_NEQ(CKGA_PLL0_CFG, CKGA_PLL_CFG_LOCK, CKGA_PLL_CFG_LOCK),
CLK_WHILE_NEQ(CKGA_PLL1_CFG, CKGA_PLL_CFG_LOCK, CKGA_PLL_CFG_LOCK),

CLKB_WHILE_NEQ(CKGA_PLL0_CFG, CKGA_PLL_CFG_LOCK, CKGA_PLL_CFG_LOCK),
CLKB_WHILE_NEQ(CKGA_PLL1_CFG, CKGA_PLL_CFG_LOCK, CKGA_PLL_CFG_LOCK),


/* restore the original values */
DATA_LOAD(0x0), CLK_STORE(CKGA_CLKOPSRC_SWITCH_CFG),

DATA_LOAD(0x1), CLK_STORE(CKGA_CLKOPSRC_SWITCH_CFG2),

DATA_LOAD(0x2), CLKB_STORE(CKGA_CLKOPSRC_SWITCH_CFG),

DATA_LOAD(0x3), CLKB_STORE(CKGA_CLKOPSRC_SWITCH_CFG2),
 /* END. */
_END()
};

/* *********************
 * MEM INSTRUCTION TABLE
 * *********************
 */
static unsigned long stx7108_mem_table[] __cacheline_aligned = {
CLKB_POKE(CKGA_OSC_DIV_CFG(11), 29),  /* CLKA_IC_REG_LP_ON @ 1 MHz to
				       * be safe for lirc
				       */

/* 1. Enables the DDR self refresh mode based on paraghaph. 7.1.4
 *    -> from ACCESS to LowPower
 */
RAW_POKE(4, DDR_SCTL, DDR_SCTL_SLEEP),
RAW_WHILE_NEQ(4, DDR_STAT, DDR_STAT_LOW_POWER, DDR_STAT_LOW_POWER),
#if 0
RAW_POKE(5, DDR_SCTL, DDR_SCTL_SLEEP),
RAW_WHILE_NEQ(5, DDR_STAT, DDR_STAT_LOW_POWER, DDR_STAT_LOW_POWER),
#endif
CLK_POKE(CKGA_CLKOPSRC_SWITCH_CFG, 0x0),
CLK_POKE(CKGA_CLKOPSRC_SWITCH_CFG2, 0x0),
CLKB_POKE(CKGA_CLKOPSRC_SWITCH_CFG, 0x0),
CLKB_POKE(CKGA_CLKOPSRC_SWITCH_CFG2, 0x0),

#if 0
/*
 * Currently The ClockGen_D isn't supported
 * because the system hangs.
 * It will requires further investigation.
 */
/* 1.1 Turn-off the ClockGenD */
SYS_AND_LONG(SYS_BNK1_CFG(4), ~1),

#endif
/* PLLs in power down */
CLK_OR_LONG(CKGA_POWER_CFG, 0x3),
CLKB_OR_LONG(CKGA_POWER_CFG, 0x3),

 /* END. */
_END(),

/* Turn-on the PLLs */
CLK_AND_LONG(CKGA_POWER_CFG, ~3),
CLKB_AND_LONG(CKGA_POWER_CFG, ~3),
/* Wait PLLs lock */
CLK_WHILE_NEQ(CKGA_PLL0_CFG, CKGA_PLL_CFG_LOCK, CKGA_PLL_CFG_LOCK),
CLK_WHILE_NEQ(CKGA_PLL1_CFG, CKGA_PLL_CFG_LOCK, CKGA_PLL_CFG_LOCK),

CLKB_WHILE_NEQ(CKGA_PLL0_CFG, CKGA_PLL_CFG_LOCK, CKGA_PLL_CFG_LOCK),
CLKB_WHILE_NEQ(CKGA_PLL1_CFG, CKGA_PLL_CFG_LOCK, CKGA_PLL_CFG_LOCK),

#if 0
/* 1. Turn-on the LMI ClocksGenD */
SYS_OR_LONG(SYS_BNK1_CFG(4), 1),
/* Wait LMI ClocksGenD lock */
SYS_WHILE_NEQ(SYS_BNK1_STA(5), 1, 1),
#endif


DATA_LOAD(0x0), CLK_STORE(CKGA_CLKOPSRC_SWITCH_CFG),
DATA_LOAD(0x1), CLK_STORE(CKGA_CLKOPSRC_SWITCH_CFG2),
DATA_LOAD(0x2), CLKB_STORE(CKGA_CLKOPSRC_SWITCH_CFG),
DATA_LOAD(0x3), CLKB_STORE(CKGA_CLKOPSRC_SWITCH_CFG2),

/* 2. Disables the DDR self refresh mode based on paraghaph 7.1.3
 *    -> from LowPower to Access
 */
RAW_POKE(4, DDR_SCTL, DDR_SCTL_WAKEUP),
RAW_WHILE_NEQ(4, DDR_STAT, DDR_STAT_ACCESS, DDR_STAT_ACCESS),

RAW_POKE(4, DDR_SCTL, DDR_SCTL_CFG),
RAW_WHILE_NEQ(4, DDR_STAT, DDR_STAT_CONFIG, DDR_STAT_CONFIG),
RAW_POKE(4, DDR_SCTL, DDR_SCTL_GO),
RAW_WHILE_NEQ(4, DDR_STAT, DDR_STAT_ACCESS, DDR_STAT_ACCESS),
#if 0
RAW_POKE(5, DDR_SCTL, DDR_SCTL_WAKEUP),
RAW_WHILE_NEQ(5, DDR_STAT, DDR_STAT_ACCESS, DDR_STAT_ACCESS),

RAW_POKE(5, DDR_SCTL, DDR_SCTL_CFG),
RAW_WHILE_NEQ(5, DDR_STAT, DDR_STAT_CONFIG, DDR_STAT_CONFIG),
RAW_POKE(5, DDR_SCTL, DDR_SCTL_GO),
RAW_WHILE_NEQ(5, DDR_STAT, DDR_STAT_ACCESS, DDR_STAT_ACCESS),
#endif
_DELAY(),
_DELAY(),
_DELAY(),
_END()
};

static unsigned long stx7108_wrt_table[4] __cacheline_aligned;
static unsigned long *osc_regs;
static int stx7108_suspend_prepare(suspend_state_t state)
{
	int i;
	/* save CGA registers*/
	stx7108_wrt_table[0] = /* swith config */
		   ioread32(CKGA0_BASE_ADDRESS + CKGA_CLKOPSRC_SWITCH_CFG);
	stx7108_wrt_table[1] = /* swith config 1 */
		   ioread32(CKGA0_BASE_ADDRESS + CKGA_CLKOPSRC_SWITCH_CFG2);
	stx7108_wrt_table[2] = /* swith config */
		   ioread32(CKGA1_BASE_ADDRESS + CKGA_CLKOPSRC_SWITCH_CFG);
	stx7108_wrt_table[3] = /* swith config 1 */
		   ioread32(CKGA1_BASE_ADDRESS + CKGA_CLKOPSRC_SWITCH_CFG2);

	/* 16 OSC register for each bank */
	osc_regs = kmalloc(GFP_KERNEL, sizeof(long) * 2 * 18);
	if (!osc_regs)
		return -ENOMEM;
	for (i = 0; i < 18; ++i) {
		osc_regs[i] = ioread32(CKGA0_BASE_ADDRESS +
			CKGA_OSC_DIV_CFG(i));
		iowrite32(0x1f , CKGA0_BASE_ADDRESS + CKGA_OSC_DIV_CFG(i));
	}

	for (i = 0; i < 18; ++i) {
		osc_regs[i + 18] = ioread32(CKGA1_BASE_ADDRESS +
			CKGA_OSC_DIV_CFG(i));
		iowrite32(0x1f , CKGA1_BASE_ADDRESS + CKGA_OSC_DIV_CFG(i));
	}

	return 0;
}

static int stx7108_suspend_finish(suspend_state_t state)
{
	int i;
	if (!osc_regs)
		return 0;
	for (i = 0; i < 18; ++i)
		iowrite32(osc_regs[i], CKGA0_BASE_ADDRESS +
			CKGA_OSC_DIV_CFG(i));
	for (i = 0; i < 18; ++i)
		iowrite32(osc_regs[i + 18], CKGA1_BASE_ADDRESS +
			CKGA_OSC_DIV_CFG(i));
	kfree(osc_regs);
	return 0;
}

static unsigned long stx7108_iomem[] __cacheline_aligned = {
	stx7108_wrt_table,
	CKGA0_BASE_ADDRESS,
	CKGA1_BASE_ADDRESS,
	SYS_1_BASE_ADDRESS,
	DDR3SS0_REG,
	DDR3SS1_REG,
};

static int stx7108_evttoirq(unsigned long evt)
{
	return ((evt == 0xa00) ? ilc2irq(evt) : evt2irq(evt));
}

static struct sh4_suspend_t stx7108_suspend __cacheline_aligned = {
	.iobase = stx7108_iomem,
	.ops.prepare = stx7108_suspend_prepare,
	.ops.finish = stx7108_suspend_finish,
	.evt_to_irq = stx7108_evttoirq,

	.stby_tbl = (unsigned long)stx7108_standby_table,
	.stby_size = DIV_ROUND_UP(ARRAY_SIZE(stx7108_standby_table) *
			sizeof(long), L1_CACHE_BYTES),

	.mem_tbl = (unsigned long)stx7108_mem_table,
	.mem_size = DIV_ROUND_UP(ARRAY_SIZE(stx7108_mem_table) * sizeof(long),
			L1_CACHE_BYTES),
	.wrt_tbl = (unsigned long)stx7108_wrt_table,
	.wrt_size = DIV_ROUND_UP(ARRAY_SIZE(stx7108_wrt_table) * sizeof(long),
			L1_CACHE_BYTES),
};

static int __init stx7108_suspend_setup(void)
{
	struct sysconf_field *sc[2];
	int i;

	/* ClochGen_D.Pll power up/down*/
	sc[0] = sysconf_claim(SYS_CFG_BANK1, 4, 0, 0, "PM");
	/* ClochGen_D.Pll lock status */
	sc[1] = sysconf_claim(SYS_STA_BANK1, 5, 0, 0, "PM");


	for (i = 0; i < ARRAY_SIZE(sc); ++i)
		if (!sc[i])
			goto error;
#ifdef CONFIG_PM_DEBUG
	/* route the sh4/2 clock frequenfy */
	iowrite32(0xc, CKGA0_BASE_ADDRESS + CKGA_CLKOBS_MUX1_CFG);
#endif

	return sh4_suspend_register(&stx7108_suspend);

error:
	printk(KERN_ERR "[STM][PM] Error to acquire the sysconf registers\n");
	for (i = 0; i < ARRAY_SIZE(sc); ++i)
		if (sc[i])
			sysconf_release(sc[i]);

	return -1;
}

late_initcall(stx7108_suspend_setup);
