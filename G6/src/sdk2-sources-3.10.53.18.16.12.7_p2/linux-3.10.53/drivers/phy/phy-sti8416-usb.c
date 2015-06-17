/*
 * Copyright (C) 2014 STMicroelectronics
 *
 * STMicroelectronics PHY driver for STi8416 USB.
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
#include <linux/phy/phy.h>
#include <linux/reset.h>
#include <linux/mfd/syscon.h>

struct sti8416_usb_phy {
	struct device *dev;
	struct clk *clk;
	struct reset_control *rstc;
};

static int sti8416_usb_phy_init(struct phy *phy)
{
	return 0;
}

static int sti8416_usb_phy_power_on(struct phy *phy)
{
	struct sti8416_usb_phy *phy_dev = phy_get_drvdata(phy);
	int ret;

	ret = clk_prepare_enable(phy_dev->clk);
	if (ret) {
		dev_err(phy_dev->dev, "Failed to enable osc_phy clock\n");
		return ret;
	}

	return reset_control_deassert(phy_dev->rstc);
}

static int sti8416_usb_phy_power_off(struct phy *phy)
{
	struct sti8416_usb_phy *phy_dev = phy_get_drvdata(phy);

	clk_disable_unprepare(phy_dev->clk);

	return reset_control_assert(phy_dev->rstc);
}

static struct phy_ops sti8416_usb_phy_ops = {
	.init		= sti8416_usb_phy_init,
	.power_on	= sti8416_usb_phy_power_on,
	.power_off	= sti8416_usb_phy_power_off,
	.owner		= THIS_MODULE,
};

static const struct of_device_id sti8416_usb_phy_of_match[];

static int sti8416_usb_phy_probe(struct platform_device *pdev)
{
	const struct of_device_id *match;
	struct sti8416_usb_phy *phy_dev;
	struct device *dev = &pdev->dev;
	struct phy_provider *phy_provider;
	struct phy *phy;

	phy_dev = devm_kzalloc(dev, sizeof(*phy_dev), GFP_KERNEL);
	if (!phy_dev)
		return -ENOMEM;

	match = of_match_device(sti8416_usb_phy_of_match, &pdev->dev);
	if (!match)
		return -ENODEV;

	phy_dev->rstc = devm_reset_control_get(dev, NULL);
	if (IS_ERR(phy_dev->rstc)) {
		dev_err(dev, "failed to ctrl PHY reset\n");
		return PTR_ERR(phy_dev->rstc);
	}

	phy_dev->clk = devm_clk_get(dev, "osc_phy");
	if (IS_ERR(phy_dev->clk)) {
		dev_err(dev, "osc_phy clk not found\n");
		return PTR_ERR(phy_dev->clk);
	}

	phy_provider = devm_of_phy_provider_register(dev, of_phy_simple_xlate);
	if (IS_ERR(phy_provider))
		return PTR_ERR(phy_provider);

	phy = devm_phy_create(dev, &sti8416_usb_phy_ops, NULL);
	if (IS_ERR(phy)) {
		dev_err(dev, "failed to create Display Port PHY\n");
		return PTR_ERR(phy);
	}

	phy_dev->dev = dev;

	phy_set_drvdata(phy, phy_dev);

	return 0;
}

static const struct of_device_id sti8416_usb_phy_of_match[] = {
	{ .compatible = "st,sti8416-usb2phy", },
	{ },
};
MODULE_DEVICE_TABLE(of, sti8416_usb_phy_of_match);

static struct platform_driver sti8416_usb_phy_driver = {
	.probe	= sti8416_usb_phy_probe,
	.driver = {
		.name	= "sti8416-usb-phy",
		.owner	= THIS_MODULE,
		.of_match_table	= sti8416_usb_phy_of_match,
	}
};
module_platform_driver(sti8416_usb_phy_driver);

MODULE_AUTHOR("Giuseppe Cavallaro <peppe.cavallaro@st.com>");
MODULE_DESCRIPTION("STMicroelectronics USB PHY driver for STi8416 series");
MODULE_LICENSE("GPL v2");
