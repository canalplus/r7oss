/*
 * Copyright (C) 2007 STMicroelectronics Limited
 * Author: Stuart Menefy <stuart.menefy@st.com>
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 */

#ifndef __LINUX_STM_SYSCONF_H
#define __LINUX_STM_SYSCONF_H

#include <linux/types.h>

struct sysconf_field;

/**
 * sysconf_claim - Claim ownership of a field of a sysconfig register
 * @group: register group (ie. SYS_CFG, SYS_STA); SOC-specific, see below
 * @num: register number
 * @lsb: the LSB of the register we are claiming
 * @msb: the MSB of the register we are claiming
 * @devname: device claiming the field
 *
 * This function claims ownership of a field from a sysconfig register.
 * The part of the sysconfig register being claimed is from bit @lsb
 * through to bit @msb inclusive. To claim the whole register, @lsb
 * should be 0, @msb 31 (or 63 for systems with 64 bit sysconfig registers).
 *
 * It returns a &struct sysconf_field which can be used in subsequent
 * operations on this field.
 */
struct sysconf_field *sysconf_claim(int group, int num, int lsb, int msb,
		const char *devname);

/**
 * sysconf_release - Release ownership of a field of a sysconfig register
 * @field: the sysconfig field to write to
 *
 * Release ownership of a field from a sysconf register.
 * @field must have been claimed using sysconf_claim().
 */
void sysconf_release(struct sysconf_field *field);

/**
 * sysconf_write - Write a value into a field of a sysconfig register
 * @field: the sysconfig field to write to
 * @value: the value to write into the field
 *
 * This writes @value into the field of the sysconfig register @field.
 * @field must have been claimed using sysconf_claim().
 */
void sysconf_write(struct sysconf_field *field, unsigned long value);

/**
 * sysconf_read - Read a field of a sysconfig register
 * @field: the sysconfig field to read
 *
 * This reads a field of the sysconfig register @field.
 * @field must have been claimed using sysconf_claim().
 */
unsigned long sysconf_read(struct sysconf_field *field);

/**
 * sysconf_address - Return the address memory of sysconfig register
 * @field: the sysconfig field to return
 *
 * This returns the address memory of sysconfig register
 * @field must have been claimed using sysconf_claim().
 */
void *sysconf_address(struct sysconf_field *field);

/**
 * sysconf_mask: Return the bitmask of sysconfig register
 * @field: the sysconfig field to return
 *
 * This returns the bitmask of sysconfig register
 * @field must have been claimed using sysconf_claim().
 */
unsigned long sysconf_mask(struct sysconf_field *field);



/**
 * Available register types:
 */

#if defined(CONFIG_CPU_SUBTYPE_FLI7510)

#define PRB_PU_CFG_1			0
#define PRB_PU_CFG_2			1
#define TRS_PU_CFG_0			2 /* 7510 spec */
#define TRS_SPARE_REGS_0		2 /* 75[234] spec */
#define TRS_PU_CFG_1			3 /* 7510 spec */
#define TRS_SPARE_REGS_1		3 /* 75[234] spec */
#define VDEC_PU_CFG_0			4
#define VDEC_PU_CFG_1			5
#define VOUT_PU_CFG_1			6 /* 7510 spec */
#define VOUT_SPARE_REGS			6 /* 75[234] spec */

#define CFG_RESET_CTL			PRB_PU_CFG_1, (0x00 / 4)
#define CFG_BOOT_CTL			PRB_PU_CFG_1, (0x04 / 4)
#define CFG_SYS1			PRB_PU_CFG_1, (0x08 / 4)
#define CFG_MPX_CTL			PRB_PU_CFG_1, (0x0c / 4)
#define CFG_PWR_DWN_CTL			PRB_PU_CFG_1, (0x10 / 4)
#define CFG_SYS2			PRB_PU_CFG_1, (0x14 / 4)
#define CFG_MODE_PIN_STATUS		PRB_PU_CFG_1, (0x18 / 4)
#define CFG_PCI_ROPC_STATUS		PRB_PU_CFG_1, (0x1c / 4)

#define CFG_ST40_HOST_BOOT_ADDR		PRB_PU_CFG_2, (0x00 / 4)
#define CFG_ST40_CTL_BOOT_ADDR		PRB_PU_CFG_2, (0x04 / 4)
#define CFG_SYS10			PRB_PU_CFG_2, (0x08 / 4)
#define CFG_RNG_BIST_CTL		PRB_PU_CFG_2, (0x0c / 4)
#define CFG_SYS12			PRB_PU_CFG_2, (0x10 / 4)
#define CFG_SYS13			PRB_PU_CFG_2, (0x14 / 4)
#define CFG_SYS14			PRB_PU_CFG_2, (0x18 / 4)
#define CFG_EMI_ROPC_STATUS		PRB_PU_CFG_2, (0x1c / 4)

#define CFG_COMMS_CONFIG_1		TRS_SPARE_REGS_0, (0x00 / 4)
#define CFG_TRS_CONFIG			TRS_SPARE_REGS_0, (0x04 / 4)
#define CFG_COMMS_CONFIG_2		TRS_SPARE_REGS_0, (0x08 / 4)
#define CFG_USB_SOFT_JTAG		TRS_SPARE_REGS_0, (0x0c / 4)
#define CFG_TRS_SPARE_REG5_NOTUSED_0	TRS_SPARE_REGS_0, (0x10 / 4)
#define CFG_TRS_CONFIG_2		TRS_SPARE_REGS_0, (0x14 / 4)
#define CFG_COMMS_TRS_STATUS		TRS_SPARE_REGS_0, (0x18 / 4)
#define CFG_EXTRA_ID1_LSB		TRS_SPARE_REGS_0, (0x1c / 4)

#define CFG_SPARE_1			TRS_SPARE_REGS_1, (0x00 / 4)
#define CFG_SPARE_2			TRS_SPARE_REGS_1, (0x04 / 4)
#define CFG_SPARE_3			TRS_SPARE_REGS_1, (0x08 / 4)
#define CFG_TRS_SPARE_REG4_NOTUSED	TRS_SPARE_REGS_1, (0x0c / 4)
#define CFG_TRS_SPARE_REG5_NOTUSED_1	TRS_SPARE_REGS_1, (0x10 / 4)
#define CFG_TRS_SPARE_REG6_NOTUSED	TRS_SPARE_REGS_1, (0x14 / 4)
#define CFG_DEVICE_ID			TRS_SPARE_REGS_1, (0x18 / 4)
#define CFG_EXTRA_ID1_MSB		TRS_SPARE_REGS_1, (0x1c / 4)

#define CFG_TOP_SPARE_REG1		VDEC_PU_CFG_0, (0x00 / 4)
#define CFG_TOP_SPARE_REG2		VDEC_PU_CFG_0, (0x04 / 4)
#define CFG_TOP_SPARE_REG3		VDEC_PU_CFG_0, (0x08 / 4)
#define CFG_ST231_DRA2_DEBUG		VDEC_PU_CFG_0, (0x0c / 4)
#define CFG_ST231_AUD1_DEBUG		VDEC_PU_CFG_0, (0x10 / 4)
#define CFG_ST231_AUD2_DEBUG		VDEC_PU_CFG_0, (0x14 / 4)
#define CFG_REG7_0			VDEC_PU_CFG_0, (0x18 / 4)
#define CFG_INTERRUPT			VDEC_PU_CFG_0, (0x1c / 4)

#define CFG_ST231_DRA2_PERIPH_REG1	VDEC_PU_CFG_1, (0x00 / 4)
#define CFG_ST231_DRA2_BOOT_REG2	VDEC_PU_CFG_1, (0x04 / 4)
#define CFG_ST231_AUD1_PERIPH_REG3	VDEC_PU_CFG_1, (0x08 / 4)
#define CFG_ST231_AUD1_BOOT_REG4	VDEC_PU_CFG_1, (0x0c / 4)
#define CFG_ST231_AUD2_PERIPH_REG5	VDEC_PU_CFG_1, (0x10 / 4)
#define CFG_ST231_AUD2_BOOT_REG6	VDEC_PU_CFG_1, (0x14 / 4)
#define CFG_REG7_1			VDEC_PU_CFG_1, (0x18 / 4)
#define CFG_INTERRUPT_REG8		VDEC_PU_CFG_1, (0x1c / 4)

#define CFG_REG1_VOUT_PIO_ALT_SEL	VOUT_SPARE_REGS, (0x00 / 4)
#define CFG_REG2_VOUT_PIO_ALT_SEL	VOUT_SPARE_REGS, (0x04 / 4)
#define CFG_VOUT_SPARE_REG3		VOUT_SPARE_REGS, (0x08 / 4)
#define CFG_REG4_DAC_CTRL		VOUT_SPARE_REGS, (0x0c / 4)
#define CFG_REG5_VOUT_DEBUG_PAD_CTL	VOUT_SPARE_REGS, (0x10 / 4)
#define CFG_REG6_TVOUT_DEBUG_CTL	VOUT_SPARE_REGS, (0x14 / 4)
#define CFG_REG7_UNUSED			VOUT_SPARE_REGS, (0x18 / 4)

#elif defined(CONFIG_CPU_SUBTYPE_STX5197)

#define HS_CFG 			0
#define HD_CFG 			1

#define CFG_CTRL_A		HS_CFG, (0x00 / 4)
#define CFG_CTRL_B		HS_CFG, (0x04 / 4)

#define CFG_CTRL_C		HD_CFG, (0x00 / 4)
#define CFG_CTRL_D		HD_CFG, (0x04 / 4)
#define CFG_CTRL_E		HD_CFG, (0x08 / 4)
#define CFG_CTRL_F		HD_CFG, (0x0c / 4)
#define CFG_CTRL_G		HD_CFG, (0x10 / 4)
#define CFG_CTRL_H		HD_CFG, (0x14 / 4)
#define CFG_CTRL_I		HD_CFG, (0x18 / 4)
#define CFG_CTRL_J		HD_CFG, (0x1c / 4)

#define CFG_CTRL_K		HD_CFG, (0x40 / 4)
#define CFG_CTRL_L		HD_CFG, (0x44 / 4)
#define CFG_CTRL_M		HD_CFG, (0x48 / 4)
#define CFG_CTRL_N		HD_CFG, (0x4c / 4)
#define CFG_CTRL_O		HD_CFG, (0x50 / 4)
#define CFG_CTRL_P		HD_CFG, (0x54 / 4)
#define CFG_CTRL_Q		HD_CFG, (0x58 / 4)
#define CFG_CTRL_R		HD_CFG, (0x5c / 4)

#define CFG_MONITOR_A		HS_CFG, (0x08 / 4)
#define CFG_MONITOR_B		HS_CFG, (0x0c / 4)

#define CFG_MONITOR_C		HD_CFG, (0x20 / 4)
#define CFG_MONITOR_D		HD_CFG, (0x24 / 4)
#define CFG_MONITOR_E		HD_CFG, (0x28 / 4)
#define CFG_MONITOR_F		HD_CFG, (0x2c / 4)
#define CFG_MONITOR_G		HD_CFG, (0x30 / 4)
#define CFG_MONITOR_H		HD_CFG, (0x34 / 4)
#define CFG_MONITOR_I		HD_CFG, (0x38 / 4)
#define CFG_MONITOR_J		HD_CFG, (0x3c / 4)

#define CFG_MONITOR_K		HD_CFG, (0x60 / 4)
#define CFG_MONITOR_L		HD_CFG, (0x64 / 4)
#define CFG_MONITOR_M		HD_CFG, (0x68 / 4)
#define CFG_MONITOR_N		HD_CFG, (0x6c / 4)
#define CFG_MONITOR_O		HD_CFG, (0x70 / 4)
#define CFG_MONITOR_P		HD_CFG, (0x74 / 4)
#define CFG_MONITOR_Q		HD_CFG, (0x78 / 4)
#define CFG_MONITOR_R		HD_CFG, (0x7c / 4)

#elif defined(CONFIG_CPU_SUBTYPE_STX7108)

#define SYS_STA_BANK0		0
#define SYS_CFG_BANK0		1
#define SYS_STA_BANK1		2
#define SYS_CFG_BANK1		3
#define SYS_STA_BANK2		4
#define SYS_CFG_BANK2		5
#define SYS_STA_BANK3		6
#define SYS_CFG_BANK3		7
#define SYS_STA_BANK4		8
#define SYS_CFG_BANK4		9

#else

#define SYS_DEV			0
#define SYS_STA			1
#define SYS_CFG			2

#endif

#endif
