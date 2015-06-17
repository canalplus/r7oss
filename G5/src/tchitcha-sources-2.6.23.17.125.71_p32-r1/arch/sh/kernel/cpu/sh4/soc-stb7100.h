/*
 * -------------------------------------------------------------------------
 * <linux_root>/arch/sh/kernel/cpu/sh4/soc-stb7100.h
 * -------------------------------------------------------------------------
 * Copyright (C) 2009  STMicroelectronics
 * Author: Francesco M. Virlinzi  <francesco.virlinzi@st.com>
 *
 * May be copied or modified under the terms of the GNU General Public
 * License V.2 ONLY.  See linux/COPYING for more information.
 *
 * ------------------------------------------------------------------------- */
#ifndef __soc_stb7100_h__
#define __soc_stb7100_h__

#define CLOCKGEN_BASE_ADDR	0x19213000	/* Clockgen A */
#define CLOCKGENB_BASE_ADDR	0x19000000	/* Clockgen B */
#define CLOCKGENC_BASE_ADDR	0x19210000	/* Clockgen C */

#define CLKA_LOCK			0x00
#define CLKA_PLL0			0x08
  #define CLKA_PLL0_BYPASS		(1 << 20)
  #define CLKA_PLL0_ENABLE		(1 << 19)
  #define CLKA_PLL0_SUSPEND		((5 << 16) | (100 << 8) | \
	(CONFIG_SH_EXTERNAL_CLOCK / 1000000))

#define CLKA_PLL0_LOCK			0x10
  #define CLKA_PLL0_LOCK_LOCKED		0x01

#define CLKA_ST40			0x14
#define CLKA_ST40_IC			0x18
#define CLKA_ST40_PER			0x1c
#define CLKA_FDMA			0x20
#define CLKA_PLL1			0x24
  #define CLKA_PLL1_ENABLE		(1 << 19)
  #define CLKA_PLL1_SUSPEND		((5 << 16) | (100 << 8) | \
	(CONFIG_SH_EXTERNAL_CLOCK / 1000000))
#define CLKA_PLL1_LOCK			0x2C
  #define CLKA_PLL1_LOCK_LOCKED		0x01

#define CLKA_CLK_DIV			0x30
#define CLKA_CLK_EN			0x34
  #define CLKA_CLK_EN_ST231_AUD		(1 << 0)
  #define CLKA_CLK_EN_ST231_VID		(1 << 1)
  #define CLKA_CLK_EN_LMI_SYS		(1 << 4)
  #define CLKA_CLK_EN_LMI_VID		(1 << 5)
  #define CLKA_CLK_EN_DEFAULT		(CLKA_CLK_EN_ST231_AUD |	\
					 CLKA_CLK_EN_ST231_VID |	\
					 CLKA_CLK_EN_LMI_SYS   |	\
					 CLKA_CLK_EN_LMI_VID)
#define CLKA_PLL1_BYPASS		0x3c

#endif
