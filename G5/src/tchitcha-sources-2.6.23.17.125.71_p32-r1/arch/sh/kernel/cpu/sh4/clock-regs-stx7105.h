/*****************************************************************************
 *
 * File name   : clock-regs-stx7105.h
 * Description : Low Level API - Base addresses & register definitions.
 *
 * COPYRIGHT (C) 2009 STMicroelectronics - All Rights Reserved
 * May be copied or modified under the terms of the GNU General Public
 * License V2 __ONLY__.  See linux/COPYING for more information.
 *
 *****************************************************************************/

#ifndef __CLOCK_LLA_REGS_H
#define __CLOCK_LLA_REGS_H

#define CKGA_BASE_ADDRESS		0xFE213000
#define CKGB_BASE_ADDRESS		0xFE000000
#define CKGC_BASE_ADDRESS		0xFE210000

/* --- CKGA registers (hardware specific) ----------------------------------- */
#define CKGA_PLL0_CFG			0x000
#define CKGA_PLL1_CFG			0x004
#define CKGA_POWER_CFG			0x010
#define CKGA_CLKOPSRC_SWITCH_CFG	0x014
#define CKGA_OSC_ENABLE_FB		0x018
#define CKGA_PLL0_ENABLE_FB		0x01c
#define CKGA_PLL1_ENABLE_FB		0x020
#define CKGA_CLKOPSRC_SWITCH_CFG2	0x024

#define CKGA_CLKOBS_MUX1_CFG		0x030
#define CKGA_CLKOBS_MASTER_MAXCOUNT	0x034
#define CKGA_CLKOBS_CMD			0x038
#define CKGA_CLKOBS_STATUS		0x03c
#define CKGA_CLKOBS_SLAVE0_COUNT	0x040
#define CKGA_OSCMUX_DEBUG		0x044
#define CKGA_CLKOBS_MUX2_CFG		0x048
#define CKGA_LOW_POWER_CTRL		0x04C

#define CKGA_OSC_DIV0_CFG		0x800
#define CKGA_OSC_DIV_CFG(x)		(CKGA_OSC_DIV0_CFG + (x) * 4)

#define CKGA_PLL0HS_DIV0_CFG		0x900
#define CKGA_PLL0HS_DIV_CFG(x)		(CKGA_PLL0HS_DIV0_CFG + (x) * 4)

#define CKGA_PLL0LS_DIV0_CFG		0xA00
#define CKGA_PLL0LS_DIV_CFG(x)		(CKGA_PLL0LS_DIV0_CFG + (x) * 4)

#define CKGA_PLL1_DIV0_CFG		0xB00
#define CKGA_PLL1_DIV_CFG(x)		(CKGA_PLL1_DIV0_CFG + (x) * 4)


/* --- CKGB registers (hardware specific) ----------------------------------- */
#define CKGB_LOCK			0x010
#define CKGB_FS0_CTRL			0x014
#define CKGB_FS1_CTRL			0x05c
#define CKGB_FS0_CLKOUT_CTRL		0x058
#define CKGB_FS1_CLKOUT_CTRL		0x0a0
#define CKGB_DISPLAY_CFG		0x0a4
#define CKGB_FS_SELECT			0x0a8
#define CKGB_POWER_DOWN			0x0ac
#define CKGB_POWER_ENABLE		0x0b0
#define CKGB_OUT_CTRL			0x0b4
#define CKGB_CRISTAL_SEL		0x0b8

/*
 * both bank and channel counts from _zero_
 */
#define CKGB_FS_MD(_bank, _channel)		\
	(0x18 + (_channel) * 0x10 + (_bank) * 0x48)
#define CKGB_FS_PE(bk, ch)	(0x4 + CKGB_FS_MD(bk, ch))
#define CKGB_FS_EN_PRG(bk, ch)	(0x4 + CKGB_FS_PE(bk, ch))
#define CKGB_FS_SDIV(bk, ch)	(0x4 + CKGB_FS_EN_PRG(bk, ch))


/* --- Audio CFG registers (0xfd601000) ------------------------------------- */
#define CKGC_FS0_CFG        0x00000000
#define CKGC_FS1_CFG        0x00000100

#define CKGC_FS_MD(_bank, _channel)		\
		(0x10 + 0x100 * (_bank) + 0x10 * (_channel))
#define CKGC_FS_PE(bk, ch)	(0x4 + CKGC_FS_MD(bk, ch))
#define CKGC_FS_SDIV(bk, ch)	(0x8 + CKGC_FS_MD(bk, ch))
#define CKGC_FS_EN_PRG(bk, ch)	(0xc + CKGC_FS_MD(bk, ch))

#define AUDCFG_IOMUX_CTRL		0x00000200
#define AUDCFG_HDMI_CTRL		0x00000204
#define AUDCFG_RECOVERY_CTRL		0x00000208
#define AUDCFG_ADAC0_CTRL		0x00000400
#define AUDCFG_ADAC1_CTRL		0x00000500

#endif  /* End __CLOCK_LLA_REGS_H */

