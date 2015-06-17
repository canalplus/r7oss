/*
 * Copyright (C) 2014 STMicroelectronics
 *
 * STMicroelectronics Generic PHY driver for STiH407 USB2.
 *
 * Author: Giuseppe Cavallaro <peppe.cavallaro@st.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2, as
 * published by the Free Software Foundation.
 *
 */
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/clk.h>
#include <linux/regmap.h>
#include <linux/reset.h>
#include <linux/mfd/syscon.h>
#include <linux/phy/phy.h>

/* Default PHY_SEL and REFCLKSEL configuration */
#define STIH407_USB_PICOPHY_CTRL_PORT_CONF	0x6
#define STIH407_USB_PICOPHY_CTRL_PORT_MASK	0x1f

/* ports parameters overriding */
#define STIH407_USB_PICOPHY_PARAM_MASK		0xffffffff
#define ST_PICOPHY_COMPDISTUNE(n)		((n & 0x7) << 0)
#define ST_PICOPHY_SQRXTUNE(n)			((n & 0x7) << 3)
#define ST_PICOPHY_TXFSLSTUNE(n)		((n & 0xf) << 6)
#define ST_PICOPHY_TXPREEMPAMPTUNE(n)		((n & 0x3) << 10)
#define ST_PICOPHY_TXPREEMPPULSETUNE(n)		((n & 0x1) << 12)
#define ST_PICOPHY_TXRISETUNE(n)		((n & 0x3) << 13)
#define ST_PICOPHY_TXVREFTUNE(n)		((n & 0xf) << 15)
#define ST_PICOPHY_TXHSXVTUNE(n)		((n & 0x3) << 19)
#define ST_PICOPHY_TXRESTUNE(n)			((n & 0x3) << 21)

struct st_picophy_tune {
	u32 picophy_tune;
	u32 compdistune;
	u32 sqrxtune;
	u32 txfslstune;
	u32 txpreempamptune;
	u32 txpreemppulsetune;
	u32 txrisetune;
	u32 txvreftune;
	u32 txhsxvtune;
	u32 txrestune;
};

/*default design picophy config params*/
static struct st_picophy_tune st_phy_config = {
	.compdistune = 0x4,	/* Disconnect Threshold Adjustment */
	.sqrxtune = 0x3,	/* Squelch Threshold Adjustment */
	.txfslstune = 0x3,	/* FS/LS Source Impedance Adjustment */
	.txpreempamptune = 0x1,	/* HS Pre-Emphasis Current Control */
	.txpreemppulsetune = 0x0, /* HS Pre-Emphasis Duration Control */
	.txrisetune = 0x1,	/* HS Tx Rise/Fall Time Adjustment */
	.txvreftune = 0x3,	/* HS DC Voltage Level Adjustment */
	.txhsxvtune = 0x3,	/* HS Tx Crossover Adjustment */
	.txrestune = 0x1,	/* USB Source Impedance Adjustment */
};

struct stih407_usb2_picophy {
	struct phy *phy;
	struct regmap *regmap;
	struct device *dev;
	struct reset_control *rstc;
	struct reset_control *rstport;
	int ctrl;
	int param;
	u32 picophy_tune;
};

static int st_of_get_picophy_config(struct stih407_usb2_picophy *phy_dev)
{
	struct device *dev = phy_dev->dev;
	struct device_node *np = dev->of_node;
	struct st_picophy_tune *phy_tune = &st_phy_config;
	int value = 0;

	if (!np)
		return -EINVAL;

	of_property_read_u32(np, "st,compdistune", &phy_tune->compdistune);
	value |= ST_PICOPHY_COMPDISTUNE(phy_tune->compdistune);
	of_property_read_u32(np, "st,sqrxtune", &phy_tune->sqrxtune);
	value |= ST_PICOPHY_SQRXTUNE(phy_tune->sqrxtune);
	of_property_read_u32(np, "st,txfslstune", &phy_tune->txfslstune);
	value |= ST_PICOPHY_TXFSLSTUNE(phy_tune->txfslstune);
	of_property_read_u32(np, "st,txpreempamptune",
			     &phy_tune->txpreempamptune);
	value |= ST_PICOPHY_TXPREEMPAMPTUNE(phy_tune->txpreempamptune);
	of_property_read_u32(np, "st,txpreemppulsetune",
			     &phy_tune->txpreemppulsetune);
	value |= ST_PICOPHY_TXPREEMPPULSETUNE(phy_tune->txpreemppulsetune);
	of_property_read_u32(np, "st,txrisetune", &phy_tune->txrisetune);
	value |= ST_PICOPHY_TXRISETUNE(phy_tune->txrisetune);
	of_property_read_u32(np, "st,txvreftune", &phy_tune->txvreftune);
	value |= ST_PICOPHY_TXVREFTUNE(phy_tune->txvreftune);
	of_property_read_u32(np, "st,txhsxvtune", &phy_tune->txhsxvtune);
	value |= ST_PICOPHY_TXHSXVTUNE(phy_tune->txhsxvtune);
	of_property_read_u32(np, "st,txrestune", &phy_tune->txrestune);
	value |= ST_PICOPHY_TXRESTUNE(phy_tune->txrestune);

	return value;
}

static int stih407_usb2_pico_ctrl(struct stih407_usb2_picophy *phy_dev)
{
	/* In case global reset must be deasserted */
	if (reset_control_is_asserted(phy_dev->rstc)) {
		dev_info(phy_dev->dev, "found picoPHY in rst state\n");
		reset_control_deassert(phy_dev->rstc);
	}

	return regmap_update_bits(phy_dev->regmap, phy_dev->ctrl,
				  STIH407_USB_PICOPHY_CTRL_PORT_MASK,
				  STIH407_USB_PICOPHY_CTRL_PORT_CONF);
}

static int stih407_usb2_init_port(struct phy *phy)
{
	struct stih407_usb2_picophy *phy_dev = phy_get_drvdata(phy);
	int ret;

	dev_info(phy_dev->dev, "Generic picoPHY usb port init.\n");

	stih407_usb2_pico_ctrl(phy_dev);
	ret = regmap_update_bits(phy_dev->regmap,
				 phy_dev->param,
				 STIH407_USB_PICOPHY_PARAM_MASK,
				 phy_dev->picophy_tune);

	if (ret)
		return ret;

	return reset_control_deassert(phy_dev->rstport);
}

static int stih407_usb2_exit_port(struct phy *phy)
{
	struct stih407_usb2_picophy *phy_dev = phy_get_drvdata(phy);

	dev_info(phy_dev->dev, "Generic picoPHY power off\n");

	/*
	 * Only port reset is asserted, phy global reset is kept untouched
	 * as other ports may still be active. When all ports are in reset
	 * state, assumption is made that power will be cut off on the phy, in
	 * case of suspend for instance. Theoretically, asserting individual
	 * reset (like here) or global reset should be equivalent.
	 */
	return reset_control_assert(phy_dev->rstport);
}

static const struct phy_ops stih407_usb2_picophy_data = {
	.init = stih407_usb2_init_port,
	.exit = stih407_usb2_exit_port,
	.owner = THIS_MODULE,
};

static const struct of_device_id stih407_usb2_picophy_of_match[];

static int stih407_usb2_picophy_probe(struct platform_device *pdev)
{
	const struct of_device_id *match;
	struct stih407_usb2_picophy *phy_dev;
	struct device *dev = &pdev->dev;
	struct device_node *np = dev->of_node;
	struct phy_provider *phy_provider;
	struct phy *phy;
	struct resource *res;

	phy_dev = devm_kzalloc(dev, sizeof(*phy_dev), GFP_KERNEL);
	if (!phy_dev)
		return -ENOMEM;

	match = of_match_device(stih407_usb2_picophy_of_match, dev);
	if (!match)
		return -ENODEV;

	phy_dev->dev = dev;
	dev_set_drvdata(dev, phy_dev);

	phy_dev->rstc = devm_reset_control_get(dev, "global");
	if (IS_ERR(phy_dev->rstc)) {
		dev_err(dev, "failed to ctrl picoPHY reset\n");
		return PTR_ERR(phy_dev->rstc);
	}

	phy_dev->rstport = devm_reset_control_get(dev, "port");
	if (IS_ERR(phy_dev->rstport)) {
		dev_err(dev, "failed to ctrl picoPHY reset\n");
		return PTR_ERR(phy_dev->rstport);
	}

	/* Reset port by default: only deassert it in phy init */
	reset_control_assert(phy_dev->rstport);

	phy_dev->regmap = syscon_regmap_lookup_by_phandle(np, "st,syscfg");
	if (IS_ERR(phy_dev->regmap)) {
		dev_err(dev, "No syscfg phandle specified\n");
		return PTR_ERR(phy_dev->regmap);
	}

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "ctrl");
	if (!res) {
		dev_err(dev, "No ctrl reg found\n");
		return -ENXIO;
	}
	phy_dev->ctrl = res->start;

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "param");
	if (!res) {
		dev_err(dev, "No param reg found\n");
		return -ENXIO;
	}
	phy_dev->param = res->start;
	phy_dev->picophy_tune = st_of_get_picophy_config(phy_dev);

	phy_provider = devm_of_phy_provider_register(dev, of_phy_simple_xlate);
	if (IS_ERR(phy_provider))
		return PTR_ERR(phy_provider);

	phy = devm_phy_create(dev, match->data, NULL);
	if (IS_ERR(phy)) {
		dev_err(dev, "failed to create Display Port PHY\n");
		return PTR_ERR(phy);
	}

	phy_dev->phy = phy;
	phy_set_drvdata(phy, phy_dev);

	dev_info(dev, "STiH407 USB Generic picoPHY driver probed!");

	return 0;
}

static const struct of_device_id stih407_usb2_picophy_of_match[] = {
	{.compatible = "st,stih407-usb2-phy",
	 .data = &stih407_usb2_picophy_data},
	{},
};

MODULE_DEVICE_TABLE(of, stih407_usb2_picophy_of_match);

static struct platform_driver stih407_usb2_picophy_driver = {
	.probe = stih407_usb2_picophy_probe,
	.driver = {
		   .name = "stih407-usb-genphy",
		   .owner = THIS_MODULE,
		   .of_match_table = stih407_usb2_picophy_of_match,
		   }
};

module_platform_driver(stih407_usb2_picophy_driver);

MODULE_AUTHOR("Giuseppe Cavallaro <peppe.cavallaro@st.com>");
MODULE_DESCRIPTION("STMicroelectronics Generic picoPHY driver for STiH407");
MODULE_LICENSE("GPL v2");
