/*
 * -------------------------------------------------------------------------
 * <linux_root>/arch/sh/kernel/cpu/sh4/soc-stx7200.h
 * -------------------------------------------------------------------------
 * Copyright (C) 2009  STMicroelectronics
 * Author: Francesco M. Virlinzi  <francesco.virlinzi@st.com>
 *
 * May be copied or modified under the terms of the GNU General Public
 * License V.2 ONLY.  See linux/COPYING for more information.
 *
 * ------------------------------------------------------------------------- */

#ifndef __soc_stx7200_h__
#define __soc_stx7200_h__

/* Values for mb519 */
#define SYSACLKIN	27000000
#define SYSBCLKIN	30000000

#define SYSCONF_BASE_ADDR	0xfd704000

#define CLOCKGEN_BASE_ADDR	0xfd700000	/* Clockgen A */
#define CLOCKGENB_BASE_ADDR	0xfd701000	/* Clockgen B */
#define CLOCKGENC_BASE_ADDR	0xfd601000	/* Clockgen C */

#define CLKA_PLL0			0x00
#define   CLKA_PLL0_BYPASS		(1 << 20)
#define   CLKA_PLL0_ENABLE_STATUS	(1 << 19)
#define   CLKA_PLL0_LOCK		(1 << 31)
#define   CLKA_PLL0_SUSPEND		((5 << 16) | (100 << 8) | \
	(SYSACLKIN / 1000000))

#define CLKA_PLL1			0x04
#define   CLKA_PLL1_BYPASS		(1 << 20)
#define   CLKA_PLL1_ENABLE_STATUS	(1 << 19)
#define   CLKA_PLL1_LOCK		(1 << 31)
#define   CLKA_PLL1_SUSPEND		((100 << 8) | (SYSACLKIN / 1000000))

#define CLKA_PLL2			0x08
#define   CLKA_PLL2_BYPASS		(1 << 20)
#define   CLKA_PLL2_ENABLE_STATUS	(1 << 19)
#define   CLKA_PLL2_LOCK		(1 << 31)
#define   CLKA_PLL2_SUSPEND		((5 << 16) | (100 << 8) | \
	(SYSACLKIN / 1000000))

#define CKGA_CLKOUT_SEL			0x18

#define CLKA_PWR_CFG			0x1C
#define   PWR_CFG_PLL0_OFF		0x1
#define   PWR_CFG_PLL1_OFF		0x2
#define   PWR_CFG_PLL2_OFF 		0x4

#define CLKA_DIV_CFG			0x10


#define CLKB_PLL0_CFG			0x3C
#define   CLKB_PLL0_LOCK		(1 << 31)
#define   CLKB_PLL0_BYPASS		(1 << 20)
#define   CLKB_PLL0_SUSPEND		((5 << 16) | (100 << 8) | \
	(SYSACLKIN / 1000000))

#define CLKB_PWR_CFG			0x58
#define   CLKB_PLL0_OFF			(1 << 15)

#endif
