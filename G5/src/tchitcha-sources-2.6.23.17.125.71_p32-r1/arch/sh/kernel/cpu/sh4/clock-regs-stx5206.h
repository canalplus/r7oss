/*****************************************************************************
 *
 * File name   : clock-regs-stx5206.h
 * Description : Low Level API - Base addresses & register definitions.
 * Component   : STCLOCK
 * Module   :
 *
 * COPYRIGHT (C) 2009 STMicroelectronics - All Rights Reserved
 * May be copied or modified under the terms of the GNU General Public
 * License Version 2 _ONLY_.  See linux/COPYING for more information.
 *
 *****************************************************************************/

#ifndef __CLOCK_LLA_REGS_H
#define __CLOCK_LLA_REGS_H

#define CKGA_BASE_ADDRESS		0xFE213000
#define CKGB_BASE_ADDRESS		0xFE000000
#define CKGC_BASE_ADDRESS		0xFE210000
#define SYSCFG_BASE_ADDRESS		0xFE001000
#define PIO5_BASE_ADDRESS		0xFD025000

/* --- CKGA registers (hardware specific) ----------------------------------- */
#define CKGA_PLL0_CFG			0x000
#define CKGA_PLL1_CFG			0x004
#define   CKGA_PLL_CFG_LOCK		(1 << 31)

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

#define CKGA_OSC_DIV0_CFG	0x800
#define CKGA_OSC_DIV_CFG(x)	(CKGA_OSC_DIV0_CFG + (x) * 4)

#define CKGA_PLL0HS_DIV0_CFG	0x900
#define CKGA_PLL0HS_DIV_CFG(x)	(CKGA_PLL0HS_DIV0_CFG + (x) * 4)

#define CKGA_PLL0LS_DIV0_CFG	0xA00
#define CKGA_PLL0LS_DIV_CFG(x)	(CKGA_PLL0LS_DIV0_CFG + (x) * 4)


#define CKGA_PLL1_DIV0_CFG	0xB00
#define CKGA_PLL1_DIV_CFG	(CKGA_PLL1_DIV0_CFG + (x) * 4)

/* --- CKGB registers (hardware specific) ----------------------------------- */
#define CKGB_LOCK		0x010
#define CKGB_FS0_CTRL		0x014
#define CKGB_FS1_CTRL		0x05c
#define CKGB_FS0_CLKOUT_CTRL	0x058
#define CKGB_FS1_CLKOUT_CTRL	0x0a0

/*
 * both bank and channel counts from _zero_
 */
#define CKGB_FS_MD(_bank, _channel)		\
	(0x18 + (_channel) * 0x10 + (_bank) * 0x48)

#define CKGB_FS_PE(bk, ch)	(0x4 + CKGB_FS_MD(bk, ch))
#define CKGB_FS_EN_PRG(bk, ch)	(0x4 + CKGB_FS_PE(bk, ch))
#define CKGB_FS_SDIV(bk, ch)	(0x4 + CKGB_FS_EN_PRG(bk, ch))


#define CKGB_DISPLAY_CFG	0xa4
#define CKGB_FS_SELECT		0xa8
#define CKGB_POWER_DOWN		0xac
#define CKGB_POWER_ENABLE	0xb0
#define CKGB_OUT_CTRL		0xb4
#define CKGB_CRISTAL_SEL	0xb8

/* --- Audio CFG registers (0xfd601000) ------------------------------------- */
#define CKGC_FS_CFG(bank)		(0x100 * (bank))

#define CKGC_FS_MD(_bank, _channel)		\
		(0x10 + 0x100 * (_bank) + 0x10 * (_channel))
#define CKGC_FS_PE(bk, ch)	(0x4 + CKGC_FS_MD(bk, ch))
#define CKGC_FS_SDIV(bk, ch)	(0x8 + CKGC_FS_MD(bk, ch))
#define CKGC_FS_EN_PRG(bk, ch)	(0xc + CKGC_FS_MD(bk, ch))

#define AUDCFG_IOMUX_CTRL	0x00000200
#define AUDCFG_HDMI_CTRL	0x00000204

#define AUDCFG_RECOVERY_CTRL	0x00000208

#define AUDCFG_ADAC0_CTRL	0x00000400
#define AUDCFG_ADAC1_CTRL	0x00000500

/* --- PIO registers () ------------------------------- */
#define	 PIO_CLEAR_PnC0		0x28
#define	 PIO_CLEAR_PnC1		0x38
#define	 PIO_CLEAR_PnC2		0x48
#define	 PIO_PnC0		0x20
#define	 PIO_PnC1		0x30
#define	 PIO_PnC2		0x40
#define	 PIO_SET_PnC0		0x24
#define	 PIO_SET_PnC1		0x34
#define	 PIO_SET_PnC2		0x44

#endif				/* End __CLOCK_LLA_REGS_H */
