/*
 * Copyright (C) 2013 STMicroelectronics (R&D) Limited
 * Author: Alexandre Torgue <alexandre.torgue@st.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>

#include <dt-bindings/reset-controller/stid127-resets.h>

#include "reset-syscfg.h"

/*
 * STiD127 Peripheral powerdown definitions.
 */
static const char stid127_hd[] = "st,stid127-hd-syscfg";
#define SYSCFG_925	0x64 /* Powerdown request for USB0 */
#define SYSSTAT_937	0x94 /* Powerdown status for USB0 */

#define STID127_PDN_USB0\
	_SYSCFG_RST_CH(stid127_hd, SYSCFG_925, 0, SYSSTAT_937, 0)

static const char stid127_south[] = "st,stid127-south-syscfg";
#define SYSCFG_263	0xfc /* Powerdown request for PCIE1 */
#define SYSSTAT_278	0x138 /* Powerdown status for PCIE1 */

#define STID127_PDN_PCIE1\
	_SYSCFG_RST_CH(stid127_south, SYSCFG_263, 4, SYSSTAT_278, 3)

#define SYSCFG_267	0x10c /* Powerdown request for PCIE0 */
#define SYSSTAT_280	0x140 /* Powerdown status for PCIE0 */

#define STID127_PDN_PCIE0\
	_SYSCFG_RST_CH(stid127_south, SYSCFG_267, 4, SYSSTAT_280, 3)

#define SYSCFG_284	0x150 /* Powerdown request for KEYSCAN */
#define SYSSTAT_285	0x154 /* Powerdown status for KEYSCAN */

#define STID127_PDN_KEYSCAN\
	_SYSCFG_RST_CH(stid127_south, SYSCFG_284, 0, SYSSTAT_285, 0)

#define SYSCFG_286	0x158 /* Powerdown request for ETH0 */
#define SYSSTAT_287	0x15c /* Powerdown status for ETH0 */

#define STID127_PDN_ETH0\
	_SYSCFG_RST_CH(stid127_south, SYSCFG_286, 0, SYSSTAT_287, 2)

static const char stid127_cpu[] = "st,stid127-cpu-syscfg";

#define SYSCFG706	0x18
#define SYSCFG707	0x1c
#define SYSCFG_731	0x7c /* CPU soft reset */

static const struct syscfg_reset_channel_data stid127_powerdowns[] = {
	[STID127_USB0_POWERDOWN]	= STID127_PDN_USB0,
	[STID127_PCIE0_POWERDOWN]	= STID127_PDN_PCIE0,
	[STID127_PCIE1_POWERDOWN]	= STID127_PDN_PCIE1,
	[STID127_KEYSCAN_POWERDOWN]	= STID127_PDN_KEYSCAN,
	[STID127_ETH0_POWERDOWN]	= STID127_PDN_ETH0,
};

static const struct syscfg_reset_channel_data stid127_softresets[] = {
	[STID127_ETH0_SOFTRESET] = SYSCFG_SRST(stid127_south, SYSCFG_286, 1),
	[STID127_USB0_SOFTRESET] = SYSCFG_SRST(stid127_cpu, SYSCFG707, 24),
	[STID127_ST40_MANRESET] = SYSCFG_SRST(stid127_cpu, SYSCFG_731, 4),
	[STID127_ST40_PWRRESET] = SYSCFG_SRST(stid127_cpu, SYSCFG_731, 2),
	[STID127_PCIE0_SOFTRESET] = SYSCFG_SRST(stid127_cpu, SYSCFG707, 18),
	[STID127_PCIE1_SOFTRESET] = SYSCFG_SRST(stid127_cpu, SYSCFG707, 20),
	[STID127_FP_SOFTRESET] = SYSCFG_SRST(stid127_cpu, SYSCFG706, 28),
};

static struct syscfg_reset_controller_data stid127_powerdown_controller = {
	.wait_for_ack	= true,
	.nr_channels	= ARRAY_SIZE(stid127_powerdowns),
	.channels	= stid127_powerdowns,
};

static struct syscfg_reset_controller_data stid127_softreset_controller = {
	.wait_for_ack = false,
	.active_low = true,
	.nr_channels = ARRAY_SIZE(stid127_softresets),
	.channels = stid127_softresets,
};

static struct of_device_id stid127_reset_match[] = {
	{ .compatible = "st,stid127-powerdown",
	  .data = &stid127_powerdown_controller, },
	{ .compatible = "st,stid127-softreset",
	  .data = &stid127_softreset_controller, },
	{},
};

static struct platform_driver stid127_reset_driver = {
	.probe = syscfg_reset_probe,
	.driver = {
		.name = "reset-stid127",
		.owner = THIS_MODULE,
		.of_match_table = stid127_reset_match,
	},
};

static int __init stid127_reset_init(void)
{
	return platform_driver_register(&stid127_reset_driver);
}
arch_initcall(stid127_reset_init);
