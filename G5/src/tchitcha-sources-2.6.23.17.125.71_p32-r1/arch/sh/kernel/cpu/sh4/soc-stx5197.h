/*
 * -------------------------------------------------------------------------
 * <linux_root>/arch/sh/kernel/cpu/sh4/soc-stx5197.h
 * -------------------------------------------------------------------------
 * Copyright (C) 2009  STMicroelectronics
 *
 * May be copied or modified under the terms of the GNU General Public
 * License V.2 ONLY.  See linux/COPYING for more information.
 *
 * -------------------------------------------------------------------------
 */

#ifndef __soc_stx5197_h__
#define __soc_stx5197_h__

/*
 *      STx5197 Platform
 */
enum clocks_ID {
	CLK_XTAL_ID,
	CLK_PLLA_ID,
	CLK_PLLB_ID,
	CLK_DDR_ID,     /* 0 */
	CLK_LMI_ID,
	CLK_BLT_ID,
	CLK_SYS_ID,
	CLK_FDMA_ID,
	CLK_SPARE_ID,
	CLK_AV_ID,      /* 6 */
	CLK_SPARE2_ID,
	CLK_ETH_ID,     /* 8 */
	CLK_ST40_ID,
	CLK_ST40P_ID,
};

/* Values for mb704 */
#define XTAL	30000000

#define SYS_SERV_BASE_ADDR	0xfdc00000

#define CLK_PLL_CONFIG0(x)	((x*8)+0x0)
#define CLK_PLL_CONFIG1(x)	((x*8)+0x4)
#define  CLK_PLL_CONFIG1_POFF	(1<<13)
#define  CLK_PLL_CONFIG1_LOCK	(1<<15)

#define CLKDIV0_CONFIG0		0x90
#define CLKDIV1_4_CONFIG0(n)	(0x0a0 + ((n-1)*0xc))
#define CLKDIV6_10_CONFIG0(n)	(0x0d0 + ((n-6)*0xc))

#define CLKDIV_CONF0(x)		(((x) == 0) ? CLKDIV0_CONFIG0 : ((x) < 5) ? \
			CLKDIV1_4_CONFIG0(x) : CLKDIV6_10_CONFIG0(x))

#define CLKDIV_CONF1(x)		(CLKDIV_CONF0(x) + 0x4)
#define CLKDIV_CONF2(x)		(CLKDIV_CONF0(x) + 0x8)


#define CLK_MODE_CTRL		0x110
#define   CLK_MODE_CTRL_NULL	0x0
#define   CLK_MODE_CTRL_X1	0x1
#define   CLK_MODE_CTRL_PROG	0x2
#define   CLK_MODE_CTRL_STDB	0x3

/*
 * The REDUCED_PM is used in CLK_MODE_CTRL_PROG...
 */
#define CLK_REDUCED_PM_CTRL	0x114
#define   CLK_REDUCED_ON_XTAL_MEMSTDBY	(1<<11)
#define   CLK_REDUCED_ON_XTAL_STDBY	(~(0x22))

#define CLK_LP_MODE_DIS0	0x118
#define   CLK_LP_MODE_DIS0_VALUE	(0x3 << 11)

#define CLK_LP_MODE_DIS2	0x11C

#define CLK_DYNAMIC_PWR		0x128

#define CLK_PLL_SELECT_CFG	0x180
#define CLK_DIV_FORCE_CFG	0x184
#define CLK_OBSERVE		0x188

#define CLK_LOCK_CFG		0x300

/*
 * Utility macros
 */
#define CLK_UNLOCK()	{	writel(0xf0, SYS_SERV_BASE_ADDR + CLK_LOCK_CFG); \
				writel(0x0f, SYS_SERV_BASE_ADDR + CLK_LOCK_CFG); }

#define CLK_LOCK()		writel(0x100, SYS_SERV_BASE_ADDR + CLK_LOCK_CFG);

#endif

