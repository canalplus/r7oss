/*
 * Copyright (C) 2013 STMicroelectronics
 *
 * STMicroelectronics PHY driver for STiD127 USB.
 *
 * Author: Alexandre Torgue <alexandre.torgue@st.com>
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
#include <linux/phy/phy.h>
#include <linux/regmap.h>
#include <linux/mfd/syscon.h>

#define SYSCFG921 0x54
#define SYSCFG922 0x58

#define USB_PHY_PLL_SLP		BIT(26)
#define USB_PHY_01_COMON	BIT(24)
#define USB_PHY_PWR_MASK	(BIT(26) | BIT(24))

/**
 * struct stid127_usb_cfg - SoC specific PHY register mapping
 * @syscfg: Offset in syscfg registers bank
 * @cfg_mask: Bits mask for PHY configuration
 * @cfg: Static configuration value for PHY
 * @oscok: Notify the PHY oscillator clock is ready
 *	   Setting this bit enable the PHY
 */
struct stid127_usb_cfg {
	u32 syscfg_reg1;
	u32 cfg_mask_reg1;
	u32 cfg_reg1;
	u32 syscfg_reg2;
	u32 cfg_mask_reg2;
	u32 cfg_reg2;
};

/**
 * struct stid127_usb_phy - Private data for the PHY
 * @dev: device for this controller
 * @regmap: Syscfg registers bank in which PHY is configured
 * @cfg: SoC specific PHY register mapping
 * @clk: Oscillator used by the PHY
 */
struct stid127_usb_phy {
	struct device *dev;
	struct regmap *regmap;
	const struct stid127_usb_cfg *cfg;
	struct clk *clk;
};

static struct stid127_usb_cfg stid127_usb_phy_cfg = {
	.syscfg_reg1 = SYSCFG921,
	.cfg_mask_reg1 = 0x3c0e07,
	.cfg_reg1 = 0xc0604,
	.syscfg_reg2 = SYSCFG922,
	.cfg_mask_reg2 = 0x50c03c9,
	.cfg_reg2 = 0x40c0200,
};

static int stid127_usb_phy_init(struct phy *phy)
{
	struct stid127_usb_phy *phy_dev = phy_get_drvdata(phy);
	int ret;

	ret =  regmap_update_bits(phy_dev->regmap,
			phy_dev->cfg->syscfg_reg1, phy_dev->cfg->cfg_mask_reg1,
			phy_dev->cfg->cfg_reg1);
	if (ret)
		return ret;

	ret =  regmap_update_bits(phy_dev->regmap,
			phy_dev->cfg->syscfg_reg2, phy_dev->cfg->cfg_mask_reg2,
			phy_dev->cfg->cfg_reg2);
	return ret;
}

static int stid127_usb_phy_power_on(struct phy *phy)
{
	struct stid127_usb_phy *phy_dev = phy_get_drvdata(phy);
	int ret;

	ret = clk_prepare_enable(phy_dev->clk);
	if (ret) {
		dev_err(phy_dev->dev, "Failed to enable osc_phy clock\n");
		return ret;
	}

	ret = regmap_update_bits(phy_dev->regmap, phy_dev->cfg->syscfg_reg2,
			USB_PHY_PWR_MASK, USB_PHY_PLL_SLP);

	return ret;
}

static int stid127_usb_phy_power_off(struct phy *phy)
{
	struct stid127_usb_phy *phy_dev = phy_get_drvdata(phy);
	int ret;

	ret = regmap_update_bits(phy_dev->regmap, phy_dev->cfg->syscfg_reg2,
			USB_PHY_PWR_MASK, USB_PHY_01_COMON);
	if (ret) {
		dev_err(phy_dev->dev, "Failed to clear oscok bit\n");
		return ret;
	}

	clk_disable_unprepare(phy_dev->clk);

	return 0;
}

static struct phy_ops stid127_usb_phy_ops = {
	.init		= stid127_usb_phy_init,
	.power_on	= stid127_usb_phy_power_on,
	.power_off	= stid127_usb_phy_power_off,
	.owner		= THIS_MODULE,
};

static const struct of_device_id stid127_usb_phy_of_match[];

static int stid127_usb_phy_probe(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	const struct of_device_id *match;
	struct stid127_usb_phy *phy_dev;
	struct device *dev = &pdev->dev;
	struct phy_provider *phy_provider;
	struct phy *phy;

	phy_dev = devm_kzalloc(dev, sizeof(*phy_dev), GFP_KERNEL);
	if (!phy_dev)
		return -ENOMEM;

	match = of_match_device(stid127_usb_phy_of_match, &pdev->dev);
	if (!match)
		return -ENODEV;

	phy_dev->cfg = match->data;

	phy_dev->regmap = syscon_regmap_lookup_by_phandle(np, "st,syscfg");
	if (IS_ERR(phy_dev->regmap)) {
		dev_err(dev, "No syscfg phandle specified\n");
		return PTR_ERR(phy_dev->regmap);
	}

	phy_dev->clk = devm_clk_get(dev, "osc_phy");
	if (IS_ERR(phy_dev->clk)) {
		dev_err(dev, "osc_phy clk not found\n");
		return PTR_ERR(phy_dev->clk);
	}

	phy_provider = devm_of_phy_provider_register(dev, of_phy_simple_xlate);
	if (IS_ERR(phy_provider))
		return PTR_ERR(phy_provider);

	phy = devm_phy_create(dev, &stid127_usb_phy_ops, NULL);
	if (IS_ERR(phy)) {
		dev_err(dev, "failed to create Display Port PHY\n");
		return PTR_ERR(phy);
	}

	phy_dev->dev = dev;

	phy_set_drvdata(phy, phy_dev);

	return 0;
}

static const struct of_device_id stid127_usb_phy_of_match[] = {
	{ .compatible = "st,stid127-usb-phy", .data = &stid127_usb_phy_cfg },
	{ },
};
MODULE_DEVICE_TABLE(of, stid127_usb_phy_of_match);

static struct platform_driver stid127_usb_phy_driver = {
	.probe	= stid127_usb_phy_probe,
	.driver = {
		.name	= "stid127-usb-phy",
		.owner	= THIS_MODULE,
		.of_match_table	= stid127_usb_phy_of_match,
	}
};
module_platform_driver(stid127_usb_phy_driver);

MODULE_AUTHOR("Alexandre Torgue <alexandre.torgue@st.com>");
MODULE_DESCRIPTION("STMicroelectronics USB PHY driver for STiD127");
MODULE_LICENSE("GPL v2");
