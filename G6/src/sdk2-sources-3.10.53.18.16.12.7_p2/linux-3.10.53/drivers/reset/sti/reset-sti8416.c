/*
 * Copyright (C) 2014 STMicroelectronics (R&D) Limited
 * Author: Giuseppe Cavallaro <peppe.cavallaro@st.com>
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

#include <dt-bindings/reset-controller/sti8416-resets.h>

#include "reset-syscfg.h"

static const char syscfg_fc8_net[] = "st,sti8416-fc8-net-syscfg";
static const char syscfg_fc11_bootdev[] = "st,sti8416-fc11-bootdev-syscfg";
static const char syscfg_fc7_hsif[] = "st,sti8416-fc7-hsif-syscfg";
static const char syscfg_vsafe_lpm[] = "st,sti8416-sdp-fc1-vsafe-syscfg";

/*
 * Powerdown is controlled by clk_en bit in the CLK_RST system config register.
 * This means that there is no powerdown_req but the powerdown_ack can be
 * available from the specific area for some IPs.
 * By default clk_en is set.
 */
#define STI8416_PDN_FC7(_bit, clk_rst_reg, specific_status_reg, _stat) \
	_SYSCFG_RST_CH(syscfg_fc7_hsif, clk_rst_reg, _bit, \
		       specific_status_reg, _stat)
#define STI8416_PDN_FC11(_bit, clk_rst_reg, specific_status_reg, _stat) \
	_SYSCFG_RST_CH(syscfg_fc11_bootdev, clk_rst_reg, _bit, \
		       specific_status_reg, _stat)

#define SYSCFG_7478	0x130138	/* POWER_DOWN_STATUS */

/*
 * SYNP MAC PDN is an exception: it is managed by using gmac_0_powerdown_req
 * and the status is taken by looking at the gmac_0_powerdown_ack.
 * It is also possible for this IP to manage the PDN using the clk_en but w/o
 * taking care about the status
 */
#define SYSCFG_8401	0x130004	/* GMAC0_CTRL */
#define SYSCFG_8450	0x138000	/* GMAC0_STATUS */
#define STI8416_PDN_GMAC0(_bit, _stat) \
	_SYSCFG_RST_CH(syscfg_fc8_net, SYSCFG_8401, _bit, SYSCFG_8450, _stat)

static const struct syscfg_reset_channel_data sti8416_powerdowns[] = {

	/* FC7 HSIF - USB3 (rear side) */
	[STI8416_FC7_USB3_CLKEN_POWERDOWN] =
	    STI8416_PDN_FC7(16, 0x0, SYSCFG_7478, 8),

	/* USB2 (rear side) */
	[STI8416_FC7_USB2_CLKEN_POWERDOWN] =
	    STI8416_PDN_FC7(16, 0x10000, SYSCFG_7478, 6),
	[STI8416_FC7_SATA_0_CLKEN_POWERDOWN] =
	    STI8416_PDN_FC7(17, 0x20000, SYSCFG_7478, 0),
	[STI8416_FC7_PCIE_0_CLKEN_POWERDOWN] =
	    STI8416_PDN_FC7(16, 0x20000, SYSCFG_7478, 1),
	[STI8416_FC7_SATA_1_CLKEN_POWERDOWN] =
	    STI8416_PDN_FC7(17, 0x30000, SYSCFG_7478, 2),
	[STI8416_FC7_PCIE_1_CLKEN_POWERDOWN] =
	    STI8416_PDN_FC7(16, 0x30000, SYSCFG_7478, 3),
	[STI8416_FC7_PCIE_2_CLKEN_POWERDOWN] =
	    STI8416_PDN_FC7(16, 0x40000, SYSCFG_7478, 5),
	[STI8416_FC7_MEDIALB_POWERDOWN] =
	    SYSCFG_SRST(syscfg_fc7_hsif, 0x50000, 16),

	/* FC8 Networking */
	[STI8416_FC8_GMAC0_POWERDOWN] = STI8416_PDN_GMAC0(0, 2),
	[STI8416_FC8_CLKEN_FP2_0_POWERDOWN] =
	    SYSCFG_SRST(syscfg_fc8_net, 0x0, 16),

	/* FC11 Bootdev */
	[STI8416_FC11_USB3_CLKEN_POWERDOWN] =
	    STI8416_PDN_FC11(16, 0x0, SYSCFG_7478, 2),
	[STI8416_FC11_USB2_CLKEN_POWERDOWN] =
	    STI8416_PDN_FC11(16, 0x10000, SYSCFG_7478, 0),
	[STI8416_FC11_FLASH_CLKEN_POWERDOWN] =
	    STI8416_PDN_FC11(16, 0x20000, SYSCFG_7478, 4),
	[STI8416_FC11_SDEMMC0_CLKEN_POWERDOWN] =
	    SYSCFG_SRST(syscfg_fc11_bootdev, 0x50000, 16),
	[STI8416_FC11_SDEMMC1_CLKEN_POWERDOWN] =
	    SYSCFG_SRST(syscfg_fc11_bootdev, 0x60000, 16),
};

/* Reset definitions. */
#define LPM_CONFIG_1    0x4	/* Softreset IRB & SBC UART */
static const struct syscfg_reset_channel_data sti8416_softresets[] = {

	/* FC7 HSIF - USB3 (rear side) */
	[STI8416_FC7_USB3_1_PHY_USB2_CALIB_RESET] =
	    SYSCFG_SRST(syscfg_fc7_hsif, 0x0, 3),
	[STI8416_FC7_USB3_1_PHY_USB2_RESET] =
	    SYSCFG_SRST(syscfg_fc7_hsif, 0x0, 2),
	[STI8416_FC7_USB3_1_RESET] = SYSCFG_SRST(syscfg_fc7_hsif, 0x0, 0),

	/* USB2 (rear side) */
	[STI8416_FC7_USB2_1_PHY_CALIB_RESET] =
	    SYSCFG_SRST(syscfg_fc7_hsif, 0x10000, 2),
	[STI8416_FC7_USB2_1_PHY_RESET] =
	    SYSCFG_SRST(syscfg_fc7_hsif, 0x10000, 1),
	[STI8416_FC7_USB2_1_RESET] = SYSCFG_SRST(syscfg_fc7_hsif, 0x10000, 0),

	[STI8416_FC7_PCIE_SATA_0_MIPHY_RESET] =
	    SYSCFG_SRST(syscfg_fc7_hsif, 0x20000, 4),
	[STI8416_FC7_SATA_0_PMALIVE_RESET] =
	    SYSCFG_SRST(syscfg_fc7_hsif, 0x20000, 3),
	[STI8416_FC7_SATA_0_RESET] = SYSCFG_SRST(syscfg_fc7_hsif, 0x20000, 2),
	[STI8416_FC7_PCIE_0_RESET] = SYSCFG_SRST(syscfg_fc7_hsif, 0x20000, 1),
	[STI8416_FC7_PCIE_SATA_0_HARDRESET] =
	    SYSCFG_SRST(syscfg_fc7_hsif, 0x20000, 0),

	[STI8416_FC7_PCIE_SATA_1_MIPHY_RESET] =
	    SYSCFG_SRST(syscfg_fc7_hsif, 0x30000, 4),
	[STI8416_FC7_SATA_1_PMALIVE_RESET] =
	    SYSCFG_SRST(syscfg_fc7_hsif, 0x30000, 3),
	[STI8416_FC7_SATA_1_RESET] = SYSCFG_SRST(syscfg_fc7_hsif, 0x30000, 2),
	[STI8416_FC7_PCIE_1_RESET] = SYSCFG_SRST(syscfg_fc7_hsif, 0x30000, 1),
	[STI8416_FC7_PCIE_SATA_1_HARDRESET] =
	    SYSCFG_SRST(syscfg_fc7_hsif, 0x30000, 0),

	[STI8416_FC7_PCIE_2_MIPHY_RESET] =
	    SYSCFG_SRST(syscfg_fc7_hsif, 0x40000, 2),
	[STI8416_FC7_PCIE_2_SOFTRESET] =
	    SYSCFG_SRST(syscfg_fc7_hsif, 0x40000, 1),
	[STI8416_FC7_PCIE_2_HARDRESET] =
	    SYSCFG_SRST(syscfg_fc7_hsif, 0x40000, 0),

	[STI8416_FC7_MEDIALB_RESET] = SYSCFG_SRST(syscfg_fc7_hsif, 0x50000, 0),

	/* FC8 Networking */
	[STI8416_FC8_FP2_SOFTRESET] = SYSCFG_SRST(syscfg_fc8_net, 0x0, 0),
	[STI8416_FC8_GMAC_SOFTRESET] = SYSCFG_SRST(syscfg_fc8_net, 0x10000, 0),

	/* FC11 Bootdev */

	/* USB3 front  - PHY reset of calibration block */
	[STI8416_FC11_USB3_1_PHY_USB2_CALIB_RESET] =
	    SYSCFG_SRST(syscfg_fc11_bootdev, 0x0, 3),
	/* USB2 PHY reset of USB3 subsys */
	[STI8416_FC11_USB3_1_PHY_USB2_RESET] =
	    SYSCFG_SRST(syscfg_fc11_bootdev, 0x0, 2),
	/* USB2 PHY reset of USB3 subsys */
	[STI8416_FC11_USB3_1_RESET] = SYSCFG_SRST(syscfg_fc11_bootdev, 0x0, 0),

	/* USB2 front */
	[STI8416_FC11_USB2_1_PHY_CALIB_RESET] =
	    SYSCFG_SRST(syscfg_fc11_bootdev, 0x10000, 2),
	[STI8416_FC11_USB2_1_PHY_RESET] =
	    SYSCFG_SRST(syscfg_fc11_bootdev, 0x10000, 1),
	[STI8416_FC11_USB2_1_RESET] =
	    SYSCFG_SRST(syscfg_fc11_bootdev, 0x10000, 0),

	/* FLASH NAND RESET */
	[STI8416_FC11_FLASH_PHY_RESET] =
	    SYSCFG_SRST(syscfg_fc11_bootdev, 0x20000, 1),
	[STI8416_FC11_FLASH_SUBSYSTEM_RESET] =
	    SYSCFG_SRST(syscfg_fc11_bootdev, 0x20000, 0),

	/* MMC0 RESET */
	[STI8416_FC11_SDEMMC0_PHY_RESET] =
	    SYSCFG_SRST(syscfg_fc11_bootdev, 0x50000, 1),
	[STI8416_FC11_SDEMMC0_HARD_RESET] =
	    SYSCFG_SRST(syscfg_fc11_bootdev, 0x50000, 0),

	/* MMC1 RESET */
	[STI8416_FC11_SDEMMC1_PHY_RESET] =
	    SYSCFG_SRST(syscfg_fc11_bootdev, 0x60000, 1),
	[STI8416_FC11_SDEMMC1_HARD_RESET] =
	    SYSCFG_SRST(syscfg_fc11_bootdev, 0x60000, 0),

	/* FC1 VSAFE WAKEUP SS (LPM CONFIG) */
	[STI8416_FC1_IRB_SOFTRESET] =
	    SYSCFG_SRST(syscfg_vsafe_lpm, LPM_CONFIG_1, 6),
	[STI8416_FC1_KEYSCAN_SOFTRESET] =
	    SYSCFG_SRST(syscfg_vsafe_lpm, LPM_CONFIG_1, 8),
	[STI8416_FC1_UART0_SOFTRESET] =
	    SYSCFG_SRST(syscfg_vsafe_lpm, LPM_CONFIG_1, 11),
	[STI8416_FC1_UART1_SOFTRESET] =
	    SYSCFG_SRST(syscfg_vsafe_lpm, LPM_CONFIG_1, 12),
};

static struct syscfg_reset_controller_data sti8416_powerdown_controller = {
	.wait_for_ack = true,
	.nr_channels = ARRAY_SIZE(sti8416_powerdowns),
	.channels = sti8416_powerdowns,
};

static struct syscfg_reset_controller_data sti8416_softreset_controller = {
	.wait_for_ack = false,
	.active_low = true,
	.nr_channels = ARRAY_SIZE(sti8416_softresets),
	.channels = sti8416_softresets,
};

static struct of_device_id sti8416_reset_match[] = {
	{.compatible = "st,sti8416-powerdown",
	 .data = &sti8416_powerdown_controller,},
	{.compatible = "st,sti8416-softreset",
	 .data = &sti8416_softreset_controller,},
	{},
};

static struct platform_driver sti8416_reset_driver = {
	.probe = syscfg_reset_probe,
	.driver = {
		   .name = "reset-sti8416",
		   .owner = THIS_MODULE,
		   .of_match_table = sti8416_reset_match,
		   },
};

static int __init sti8416_reset_init(void)
{
	return platform_driver_register(&sti8416_reset_driver);
}

arch_initcall(sti8416_reset_init);
