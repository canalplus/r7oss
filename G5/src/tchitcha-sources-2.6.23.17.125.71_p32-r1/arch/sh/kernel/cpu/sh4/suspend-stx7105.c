/*
 * -------------------------------------------------------------------------
 * <linux_root>/arch/sh/kernel/cpu/sh4/suspend-stx7105.c
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
#include <linux/wywakeup.h>
#include <linux/stm/pio.h>
#include <asm/system.h>
#include <asm/timer.h>
#include <asm/io.h>
#include <asm/pm.h>
#include <asm/irq.h>
#include <asm/irq-ilc.h>
#include <asm/cpu-sh4/timer.h>

#include "soc-stx7105.h"

extern struct timezone sys_tz;		/* from kernel/time.c */

/* ********************
 * WRITEABLE DATA TABLE
 * ********************
 */
#define _SYS_STA4		(7)
#define _SYS_STA4_MASK		(8)
#define _SYS_STA3		(11)
#define _SYS_STA3_MASK		(12)
#define _SYS_STA3_VALUE		(13)

#define _SYS_CFG11		(9)
#define _SYS_CFG11_MASK		(10)
#define _SYS_CFG38		(5)
#define _SYS_CFG38_MASK		(6)

#define _TIME_SEC		(14)	/* mind the copy in suspend-core.S */
#define _TIME_CNT		(15)	/* mind the copy in suspend-core.S */
#define _WAKE_UP_SEC		(16)	/* mind the copy in suspend-core.S */

#define _PIO_CLR_15		(17)
#define _PIO_PIN_7_MASK		(18)

#define _PIO_CLR_11		(19)
#define _PIO_PIN_2_MASK		(20)

#define _WRITE_TABLE_SIZE	(21)

static unsigned long stx7105_wrt_table[_WRITE_TABLE_SIZE] __cacheline_aligned;

/* *************************
 * STANDBY INSTRUCTION TABLE
 * *************************
 */
static unsigned long stx7105_standby_table[] __cacheline_aligned = {
/* 1. Move all the clock on OSC */
CLK_POKE(CKGA_CLKOPSRC_SWITCH_CFG(0x0), 0x0),
CLK_POKE(CKGA_OSC_DIV_CFG(0x5), 29), /* ic_if_100 @ 1 MHz to be safe for Lirc*/

IMMEDIATE_DEST(0x1f),
/* reduces OSC_st40 */
CLK_STORE(CKGA_OSC_DIV_CFG(4)),
/* reduces OSC_clk_ic */
CLK_STORE(CKGA_OSC_DIV_CFG(0x0)),
/* END. */
_END(),

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
 *
 * About the persistent, software real-time clock
 *
 * Regular timekeeping is based on channel 1 of the timer unit module, TMU1,
 * part of the ST40 core. TMU1 is configured to be the "high precision timer"
 * (see tmu_timer_init). It is clocked by the peripheral module clock
 * (Pø, "module_clk"), whose frequency is 100MHz.
 * It counts (TMU.TCNT1) on Pø/4 (TMU.TCR1.TPSC=0), hence at 25MHz.
 * It counts down from 0xffffffff (TMU.TCOR1) to 0, and restarts automatically.
 * Hence it underflows after approximately 2 min 51 s.
 *
 * Now, when the system is suspended, the operations below reduce the frequency
 * of internal clocks. The peripheral module clock is underclocked at 1MHz.
 * TMU1 then counts at 250kHz, and underflows after 4 hr 46 min.
 * CONFIG_SH_LPRTC allows timekeeping to follow this transition, and the
 * opposite one when the system is resumed.
 * Provided that the CPU is brought out of sleep at least every 4h45, the
 * counter is read again, and the delta is used to keep the wall clock time
 * up-to-date. For this purpose a Low-Power Alarm should be scheduled.
 *
 * Regarding the accuracy of this software RTC, a limited amount of time is
 * lost during the transitions (suspend, resume). Also, NTP scaling is stopped.
 * The RTC will drift, depending on the accuracy of the external crystal.
 */
static unsigned long stx7105_mem_table[] __cacheline_aligned = {
/* 1. Enables the DDR self refresh mode */
DATA_OR_LONG(_SYS_CFG38, _SYS_CFG38_MASK),
/* waits until the ack bit is zero */
DATA_WHILE_NEQ(_SYS_STA4, _SYS_STA4_MASK, _SYS_STA4_MASK),
/* 1.1 Turn-off the ClockGenD */
DATA_OR_LONG(_SYS_CFG11, _SYS_CFG11_MASK),

IMMEDIATE_DEST(0x1f),
/* reduces OSC_st40 */
CLK_STORE(CKGA_OSC_DIV_CFG(4)),
/* reduces OSC_clk_ic */
CLK_STORE(CKGA_OSC_DIV_CFG(0x0)),
/* reduces OSC_clk_ic_if_200 */
CLK_STORE(CKGA_OSC_DIV_CFG(17)),

#ifdef CONFIG_SH_LPRTC
_RTC_SAVE_TMU_HS(),
#endif

/* 2. Move all the clock on OSC */
/* Timekeeping: this operation divides peripheral module clock frequency
 * by 29 */
CLK_POKE(CKGA_OSC_DIV_CFG(5), 29), /* ic_if_100 @ 1MHz to be safe for Lirc*/

/* Timekeeping: this operation divides peripheral module clock frequency
 * by about 3.4
 */
CLK_POKE(CKGA_CLKOPSRC_SWITCH_CFG(0x0), 0xfffff0ff),

#ifdef CONFIG_SH_LPRTC
_RTC_SAVE_TMU_LS_INIT(),
#endif

CLK_POKE(CKGA_CLKOPSRC_SWITCH_CFG(0x1), 0x3),
/* PLLs in power down */
CLK_OR_LONG(CKGA_POWER_CFG, 0x3),

#if defined(CONFIG_WY_WAKEUP) && (defined(CONFIG_SH_ST_C275) || defined(CONFIG_SH_ST_CUSTOM002012))
/* LOW_PW_FE_not - cut 3V3_FE and 1V8_LMI */
DATA_OR_LONG(_PIO_CLR_15, _PIO_PIN_7_MASK),
/* RESET_ST8 - cut 1V8_LMI */
DATA_OR_LONG(_PIO_CLR_11, _PIO_PIN_2_MASK),
#endif
 /* END. */
_END(),

/* Turn-on the PLLs */
CLK_AND_LONG(CKGA_POWER_CFG, ~3),
/* Wait PLLS lock */
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
CLK_STORE(CKGA_PLL0LS_DIV_CFG(4)),

/* 3. Restore the previous clocks setting */
DATA_LOAD(0x1),
CLK_STORE(CKGA_CLKOPSRC_SWITCH_CFG(0x1)),

#ifdef CONFIG_SH_LPRTC
_RTC_SAVE_TMU_LS_EXIT(),
#endif

DATA_LOAD(0x0),
CLK_STORE(CKGA_CLKOPSRC_SWITCH_CFG(0x0)),

DATA_LOAD(0x3),
CLK_STORE(CKGA_OSC_DIV_CFG(5)),

#ifdef CONFIG_SH_LPRTC
_RTC_CLOSE(),
#endif

DATA_LOAD(0x2),
CLK_STORE(CKGA_OSC_DIV_CFG(0x0)),
DATA_LOAD(0x4),
CLK_STORE(CKGA_OSC_DIV_CFG(17)),

_DELAY(),
_DELAY(),
_DELAY(),
_END()
};

#if defined(CONFIG_WY_WAKEUP)

#define CLKB_BASE_ADDRESS	0xfe000000
#define CLKC_BASE_ADDRESS	0xfe210000
#define SYS_CONF_BASE_ADDRESS	0xfe001000

#define LMI_BASE_ADDRESS	0xFE901000
#define ETHERNET_BASE_ADDRESS	0xFD110000

#define ST7105_HD_TVOUT_MAIN_GLUE_BASE_ADDRESS	0xFE030400
#define ST7105_HD_TVOUT_AUX_GLUE_BASE_ADDRESS	0xFD104E00

static void stx7105_suspend_prepare_rest(void)
{
	uint32_t v;

	/* audio DACs off */
	v = ioread32(0xFE210100);
	v &= 0xFFFFFF97;
	iowrite32(v, 0xFE210100);

	/* audio fs down*/
	v = ioread32(CLKC_BASE_ADDRESS + 0x000);
	v &= 0xFFFF83FF;
	iowrite32(v, CLKC_BASE_ADDRESS + 0x000);

	/* SYS_CFG3 */
	v = ioread32(SYS_CONF_BASE_ADDRESS + 0x10C);
	v |= 0x3f;		/* video DACs off */
	v &= ~(1 << 12);	/* hdmi pll down  */
	iowrite32(v, SYS_CONF_BASE_ADDRESS + 0x10C);

	v = ioread32(SYS_CONF_BASE_ADDRESS + 0x108);
	v |= (1 << 26);		/* hdmi off */
	v &= ~(1 << 27);	/* HPD off */
	v &= ~(1 << 29);	/* CEC off */
	iowrite32(v, SYS_CONF_BASE_ADDRESS + 0x108);

	/* HTO_MAIN_CTL_PADS */
	v = ioread32(ST7105_HD_TVOUT_MAIN_GLUE_BASE_ADDRESS);
	v |= (1 << 11);	/* HD DACs off */
	iowrite32(v, ST7105_HD_TVOUT_MAIN_GLUE_BASE_ADDRESS);

	/* HTO_AUX_CTRL_PADS */
	v = ioread32(ST7105_HD_TVOUT_AUX_GLUE_BASE_ADDRESS);
	v |= (1 << 9);	/* SD DACs off */
	iowrite32(v, ST7105_HD_TVOUT_AUX_GLUE_BASE_ADDRESS);

	/* GMAC down */
	iowrite32(1, ETHERNET_BASE_ADDRESS + 0x02c);

	/* SYS_CFG7 */
	v = ioread32(SYS_CONF_BASE_ADDRESS + 0x11c);
	v &= ~(1 << 16);	/* ETH IF off  */
	v &= ~(1 << 11);	/* SC1 COND VCC off */
	v &= ~(1 << 7);		/* SC0 COND VCC off */
	iowrite32(v, SYS_CONF_BASE_ADDRESS + 0x11c);

	/* SYS_CFG15 */
	v = ioread32(SYS_CONF_BASE_ADDRESS + 0x13c);
	v &= ~0xf;		/* keypad scan off */
	iowrite32(v, SYS_CONF_BASE_ADDRESS + 0x13c);

	/* SYS_CFG32 */
	v = ioread32(SYS_CONF_BASE_ADDRESS + 0x180);
	v |= (1 << 11); /* sata hc */
	v |= (1 << 9); /* sata ph */
	v |= (1 << 7); /* usb ph #2 */
	v |= (1 << 6); /* usb ph #1 */
	v |= (1 << 5); /* usb hc #2 */
	v |= (1 << 4); /* usb hc #1 */
	v |= (1 << 3); /* key scan */
	v |= (1 << 2); /* pci */
	v |= (1 << 1); /* emi */

	iowrite32(v, SYS_CONF_BASE_ADDRESS + 0x180);


	v = 0;
	v |= (1 << 7); /* sata */
	v |= (1 << 5); /* usb ph #1 */
	v |= (1 << 4); /* usb hc #2 */
	v |= (1 << 3); /* key scan */
	v |= (1 << 2); /* pci */
	v |= (1 << 1); /* emi */

	/* SYS_STA15 */
	while ( (ioread32(SYS_CONF_BASE_ADDRESS + 0x044) & v) != v)
		udelay(100);

	/* SYS_CFG40 */
	v = ioread32(SYS_CONF_BASE_ADDRESS + 0x1A0);
	v |= (1 << 2); /* alt sysclkin - NC */
	v |= (1 << 3);
	iowrite32(v, SYS_CONF_BASE_ADDRESS + 0x1A0);

	/* SYS_CFG41 */
	v = ioread32(SYS_CONF_BASE_ADDRESS + 0x1A4);
	v &= ~(1 << 3); /* termal sensor off */
	iowrite32(v, SYS_CONF_BASE_ADDRESS + 0x1A4);

	/* CEC off */
	v = ioread32(0xfe030c00 + 0x18);
	v &= ~3;
	iowrite32(v, 0xfe030c00 + 0x18);

#if defined(CONFIG_SH_ST_C275) || defined(CONFIG_SH_ST_CUSTOM002012)
	do {
		struct stpio_pin *pin;

		pin = stpio_request_pin (10, 2, "HDMI_OFF", STPIO_OUT);
		if (pin) {
			stpio_set_pin(pin, 0);
			stpio_free_pin(pin);
		}

		pin = stpio_request_pin (6, 7, "RESET_DEMOD0not", STPIO_OUT);
		if (pin) {
			stpio_set_pin(pin, 0);
			udelay(10);
			stpio_set_pin(pin, 1);
			udelay(10);
			stpio_set_pin(pin, 0);
			stpio_free_pin(pin);
		}
		pin = stpio_request_pin (7, 2, "RESET_DEMOD1not", STPIO_OUT);
		if (pin) {
			stpio_set_pin(pin, 0);
			udelay(10);
			stpio_set_pin(pin, 1);
			udelay(10);
			stpio_set_pin(pin, 0);
			stpio_free_pin(pin);
		}

		/*
		 * put external USB HUB into reset state - this prevents from
		 * spinning up re-plugged HDD
		 */
		pin = stpio_request_pin (1, 4, "RESET_USBnot", STPIO_OUT);
		if (pin) {
			stpio_set_pin(pin, 0);
			stpio_free_pin(pin);
		}

		pin = stpio_request_pin (15, 0, "ETH_notRESET", STPIO_OUT);
		if (pin) {
			stpio_set_pin(pin, 0);
			stpio_free_pin(pin);
		}

		/* put Wireless module into reset state */
		pin = stpio_request_pin (7, 1, "WIFI_RESETnot", STPIO_OUT);
		if (pin) {
			stpio_set_pin(pin, 0);
			stpio_free_pin(pin);
		}

		pin = stpio_request_pin (6, 2, "RESETnot_MLC", STPIO_OUT);
		if (pin) {
			stpio_set_pin(pin, 0);
			stpio_free_pin(pin);
		}

	} while (0);
#endif
}

#endif /* defined(CONFIG_WY_WAKEUP) */
static int stx7105_suspend_prepare(suspend_state_t state)
{
	if (state == PM_SUSPEND_STANDBY) {
		stx7105_wrt_table[0] = /* swith config */
		   ioread32(CLOCKGENA_BASE_ADDR + CKGA_CLKOPSRC_SWITCH_CFG(0));
		stx7105_wrt_table[1] = /* clk_STNoc */
		   ioread32(CLOCKGENA_BASE_ADDR + CKGA_OSC_DIV_CFG(0));
		stx7105_wrt_table[2] = /* clk_ic_if_100 */
		   ioread32(CLOCKGENA_BASE_ADDR + CKGA_OSC_DIV_CFG(5));
	} else {
		unsigned long timestamp = ULONG_MAX;
#if defined(CONFIG_SH_LPRTC) && defined(CONFIG_WY_WAKEUP)
		struct timespec utc;

		if ((wywakeup_get_wakeup_time(&utc) == 0) && (utc.tv_sec != 0))
			timestamp = (unsigned long)(utc.tv_sec
			            - sys_tz.tz_minuteswest * 60);
#endif
		stx7105_wrt_table[_WAKE_UP_SEC] = timestamp;
		stx7105_wrt_table[0] = /* swith config */
		   ioread32(CLOCKGENA_BASE_ADDR + CKGA_CLKOPSRC_SWITCH_CFG(0));
		stx7105_wrt_table[1] = /* swith config 1 */
		   ioread32(CLOCKGENA_BASE_ADDR + CKGA_CLKOPSRC_SWITCH_CFG(1));
		stx7105_wrt_table[2] = /* clk_STNoc */
		   ioread32(CLOCKGENA_BASE_ADDR + CKGA_OSC_DIV_CFG(0));
		stx7105_wrt_table[3] = /* clk_ic_if_100 */
		   ioread32(CLOCKGENA_BASE_ADDR + CKGA_OSC_DIV_CFG(5));
		stx7105_wrt_table[4] = /* clk_ic_if_200 */
		   ioread32(CLOCKGENA_BASE_ADDR + CKGA_OSC_DIV_CFG(17));
		/*
		 * NOTICE:
		 * This is solution only for case when system is restarted after wakeup.
		 */
#if defined(CONFIG_WY_WAKEUP)
		stx7105_suspend_prepare_rest();
#endif
	}
	return 0;
}

static unsigned long stx7105_iomem[2] __cacheline_aligned = {
		(unsigned long)stx7105_wrt_table,
		CLOCKGENA_BASE_ADDR};

#ifdef CONFIG_SH_LPRTC
static unsigned short stx7105_saved_tcr0;
static unsigned long stx7105_saved_tcnt1;

static void stx7105_lprtc_prepare(void)
{
	struct timespec utc;

	stx7105_saved_tcr0 = ctrl_inw(TMU0_TCR);
	stx7105_saved_tcnt1 = tmu1_read_raw();

	getnstimeofday(&utc);
	/* TMU1 had been stopped by timer_suspend(); it is restarted here */
	tmu1_restart(0);
	stx7105_wrt_table[_TIME_SEC] = (unsigned long)(utc.tv_sec -
	                               sys_tz.tz_minuteswest * 60);
	/* change unit from 1nsec to 4usec, to suit TMU1 eco @ 250kHz */
	stx7105_wrt_table[_TIME_CNT] = ((unsigned long)utc.tv_nsec + 2000UL)
	                               / 4000UL;
}

static void stx7105_lprtc_finish(void)
{
	struct timespec now;

	now.tv_sec = (long)stx7105_wrt_table[_TIME_SEC] +
	             sys_tz.tz_minuteswest * 60;
	now.tv_nsec = 0;
	timespec_add_ns(&now, stx7105_wrt_table[_TIME_CNT] * 4000UL);
	timespec_add_ns(&now, (0xffffffffUL - tmu1_read_raw()) * 40UL);
	/* For consistency we should now stop TMU1, and wait for timer_resume()
	 * to restart it. But this discontinuity would affect timekeeping.
	 * Let the counter resume from the value it had
	 * upon timer_suspend(), however. */
	tmu1_restart(stx7105_saved_tcnt1);
	do_settimeofday(&now);

	/* sh4_suspend() did use TMU0, which implies the counter has been
	 * altered. Let's reset the counter to a valid value.
	 * sh4_suspend() has stopped TMU0 already. */
	ctrl_outl(ctrl_inl(TMU0_TCOR), TMU0_TCNT);
	/* sh4_suspend() has altered the TMU0 prescaler too.
	 * Restore the related register field, TMU.TCR0.TPSC. */
	ctrl_outw(stx7105_saved_tcr0 & TMU_TCR_MSK_TPSC, TMU0_TCR);
}
#endif

static int stx7105_evt_to_irq(unsigned long evt)
{
	return ((evt < 0x400) ? ilc2irq(evt) : evt2irq(evt));
}

static struct sh4_suspend_t st40data __cacheline_aligned = {
	.iobase = stx7105_iomem,
	.ops.prepare = stx7105_suspend_prepare,
	.evt_to_irq = stx7105_evt_to_irq,
#ifdef CONFIG_SH_LPRTC
	.lprtc_enter = stx7105_lprtc_prepare,
	.lprtc_exit = stx7105_lprtc_finish,
#endif

	.stby_tbl = (unsigned long)stx7105_standby_table,
	.stby_size = DIV_ROUND_UP(ARRAY_SIZE(stx7105_standby_table) *
			sizeof(long), L1_CACHE_BYTES),

	.mem_tbl = (unsigned long)stx7105_mem_table,
	.mem_size = DIV_ROUND_UP(ARRAY_SIZE(stx7105_mem_table) * sizeof(long),
			L1_CACHE_BYTES),
	.wrt_tbl = (unsigned long)stx7105_wrt_table,
	.wrt_size = DIV_ROUND_UP(ARRAY_SIZE(stx7105_wrt_table) * sizeof(long),
			L1_CACHE_BYTES),
};

static int __init suspend_platform_setup(void)
{

	struct sysconf_field* sc;
	sc = sysconf_claim(SYS_CFG, 38, 20, 20, "pm");
	stx7105_wrt_table[_SYS_CFG38]      = (unsigned long)sysconf_address(sc);
	stx7105_wrt_table[_SYS_CFG38_MASK] = sysconf_mask(sc);

	sc = sysconf_claim(SYS_CFG, 11, 12, 12, "pm");
	stx7105_wrt_table[_SYS_CFG11]      = (unsigned long)sysconf_address(sc);
	stx7105_wrt_table[_SYS_CFG11_MASK] = sysconf_mask(sc);

	sc = sysconf_claim(SYS_STA, 4, 0, 0, "pm");
	stx7105_wrt_table[_SYS_STA4]      = (unsigned long)sysconf_address(sc);
	stx7105_wrt_table[_SYS_STA4_MASK] = sysconf_mask(sc);

	sc = sysconf_claim(SYS_STA, 3, 0, 0, "pm");
	stx7105_wrt_table[_SYS_STA3] = (unsigned long)sysconf_address(sc);
	stx7105_wrt_table[_SYS_STA3_MASK] = sysconf_mask(sc);
	stx7105_wrt_table[_SYS_STA3_VALUE] = 0;

	stx7105_wrt_table[_PIO_CLR_15]     = 0xFE018008;
	stx7105_wrt_table[_PIO_PIN_7_MASK] = (1 << 7);

	stx7105_wrt_table[_PIO_CLR_11]     = 0xFE014008;
	stx7105_wrt_table[_PIO_PIN_2_MASK] = (1 << 2);

	return sh4_suspend_register(&st40data);
}

late_initcall(suspend_platform_setup);
