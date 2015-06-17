/*****************************************************************************
 *
 * File name   : clock-regs-stx5197.h
 * Description : Low Level API - Base addresses & register definitions.
 *
 * COPYRIGHT (C) 2009 STMicroelectronics - All Rights Reserved
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 *****************************************************************************/

#ifndef __CLOCK_LLA_5197REGS_H
#define __CLOCK_LLA_5197REGS_H


/* Base addresses */
#define SYSCFG_BASE_ADDRESS	 		0xFDC00000
#define SYS_SERVICE_ADDR			0xFDC00000

/* Register offsets */
#define DCO_MODE_CFG				0x170
#define FSA_SETUP				0x010
#define FSB_SETUP		   		0x050
#define FS_DEFAULT_SETUP			0x010
#define FS_ANALOG_POFF				(1 << 3)
#define FS_NOT_RESET				(1 << 4)
#define FS_DIGITAL_PON(id)			(1 << ((id) + 8))

/* Spare clock is recovered in 5197 */
#define CLK_SPARE_SETUP0	  		0x014
#define CLK_PCM_SETUP0	 			0x020
#define CLK_SPDIF_SETUP0       			0x030
#define CLK_DSS_SETUP0       			0x040
#define CLK_PIX_SETUP0	       			0x054
#define CLK_FDMA_FS_SETUP0	      		0x060
#define CLK_AUX_SETUP0				0x070
#define CLK_USB_SETUP0				0x080
#define FS_PROG_EN				(1 << 5)
#define FS_SEL_OUT				(1 << 9)
#define FS_OUT_ENABLED				(1 << 11)

#define CAPTURE_COUNTER_PCM      		0x168

#define MODE_CONTROL				0x110
#define CLK_LOCK_CFG				0x300
#define CLK_OBS_CFG				0x188
#define FORCE_CFG				0x184
#define PLL_SELECT_CFG				0x180
#define PLLA_CONFIG0				0x000
#define PLLA_CONFIG1				0x004
#define PLLB_CONFIG0				0x008
#define PLLB_CONFIG1				0x00C
#define CLKDIV0_CONFIG0				0x090
#define CLKDIV1_CONFIG0				0x0A0
#define CLKDIV2_CONFIG0				0x0AC
#define CLKDIV3_CONFIG0				0x0B8
#define CLKDIV4_CONFIG0				0x0C4
#define CLKDIV6_CONFIG0				0x0D0
#define CLKDIV7_CONFIG0				0x0DC
#define CLKDIV8_CONFIG0				0x0E8
#define CLKDIV9_CONFIG0				0x0F4
#define CLKDIV10_CONFIG0			0x100
#define DYNAMIC_PWR_CONFIG			0x128
#define LOW_PWR_CTRL				0x118
#define LOW_PWR_CTRL1				0x11C

#endif
