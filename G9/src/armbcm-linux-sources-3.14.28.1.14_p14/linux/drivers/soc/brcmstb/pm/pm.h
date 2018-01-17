/*
 * Definitions for Broadcom STB power management / Always ON (AON) block
 *
 * Copyright © 2014 Broadcom Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef __BRCMSTB_PM_H__
#define __BRCMSTB_PM_H__

#define AON_CTRL_RESET_CTRL		0x00
#define AON_CTRL_PM_CTRL		0x04
#define AON_CTRL_PM_STATUS		0x08
#define AON_CTRL_PM_CPU_WAIT_COUNT	0x10
#define AON_CTRL_PM_INITIATE		0x88
#define AON_CTRL_HOST_MISC_CMDS		0x8c
#define AON_CTRL_SYSTEM_DATA_RAM_OFS	0x200

/* RESET_CTRL bitfield */
#define FP_RESET_ENABLE_LOCK  (1 << 3)
#define FP_RESET_ENABLE       (1 << 2)
#define FP_RESET_POLARITY     (1 << 1)
#define FP_RESET_HISTORY      (1 << 0)

/* PM_CTRL bitfield */
#define PM_FAST_PWRDOWN			(1 << 6)
#define PM_WARM_BOOT			(1 << 5)
#define PM_DEEP_STANDBY			(1 << 4)
#define PM_CPU_PWR			(1 << 3)
#define PM_USE_CPU_RDY			(1 << 2)
#define PM_PLL_PWRDOWN			(1 << 1)
#define PM_PWR_DOWN			(1 << 0)

#define PM_S2_COMMAND	(PM_PLL_PWRDOWN | PM_USE_CPU_RDY | PM_PWR_DOWN)
#define PM_COLD_CONFIG	(PM_PLL_PWRDOWN | PM_DEEP_STANDBY)
#define PM_WARM_CONFIG	(PM_COLD_CONFIG | PM_USE_CPU_RDY | PM_WARM_BOOT)

#endif /* __BRCMSTB_PM_H__ */
