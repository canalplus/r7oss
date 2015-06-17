/*
 * -------------------------------------------------------------------------
 * <linux_root>/arch/sh/kernel/cpu/sh4/soc-stx7105.h
 * -------------------------------------------------------------------------
 * Copyright (C) 2009  STMicroelectronics
 * Author: Francesco M. Virlinzi  <francesco.virlinzi@st.com>
 *
 * May be copied or modified under the terms of the GNU General Public
 * License V.2 ONLY.  See linux/COPYING for more information.
 *
 * -------------------------------------------------------------------------
 */
#ifndef __soc_stx7105_h__
#define __soc_stx7105_h__

/* Values for mb680 */
#define SYSCLKIN        30000000
#define SYSCLKINALT     30000000

#define SYSCONF_BASE_ADDR	0xfe001000
#define CLOCKGENA_BASE_ADDR     0xfe213000	/* Clockgen A */
#define CLOCKGENB_BASE_ADDR     0xfe000000	/* Clockgen B */

/* Definitions taken from targetpack sti7105_clockgena_regs.xml */
#define CKGA_PLL0_CFG			0x000
  #define CKGA_PLL0_CFG_DIVRES		(1 << 20)
  #define CKGA_PLL0_CFG_BYPASS		CKGA_PLL0_CFG_DIVRES
  #define CKGA_PLL0_CFG_LOCK		(1 << 31)

#define CKGA_PLL1_CFG			0x004
  #define CKGA_PLL1_CFG_DIVRES		(1 << 20)
  #define CKGA_PLL1_CFG_BYPASS		CKGA_PLL1_CFG_DIVRES
  #define CKGA_PLL1_CFG_LOCK		(1 << 31)

#define CKGA_POWER_CFG			0x010
  #define CKGA_POWER_PLL0_DISABLE	0x1
  #define CKGA_POWER_PLL1_DISABLE	0x2

#define CKGA_CLKOPSRC_SWITCH_CFG(x)	(0x014 + ((x) * 0x10))
  #define CKGA_CLKOPSRC_SFT_CLK(x)	(((x) % 16) * 2)
  #define CKGA_CLKOPSRC_OSC		0x0
  #define CKGA_CLKOPSRC_PLL0LS		0x1
  #define CKGA_CLKOPSRC_PLL0HS		0x1
  #define CKGA_CLKOPSRC_PLL1		0x2
  #define CKGA_CLKOPSRC_STOP		0x3

#define CKGA_CLKOBS_MUX1_CFG		0x030
#define CKGA_CLKOBS_MUX2_CFG		0x048
/* All the following appear to be offsets into clkgen B, despite the name */
#define CKGA_OSC_DIV_CFG(x)		(0x800 + ((x) * 4))
#define CKGA_PLL0HS_DIV_CFG(x)		(0x900 + ((x) * 4))
#define CKGA_PLL0LS_DIV_CFG(x)		(0xa10 + (((x) -4) *4))
#define CKGA_PLL1_DIV_CFG(x)		(0xb00 + ((x) *4))

#endif
