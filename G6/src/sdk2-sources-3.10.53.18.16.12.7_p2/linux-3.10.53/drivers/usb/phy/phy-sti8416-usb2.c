/*
 * Copyright (C) 2014 STMicroelectronics
 *
 * STMicroelectronics PHY driver for STi8416 USB2.
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
#include <linux/usb/phy.h>

#define phy_to_priv(x)	container_of((x), struct sti8416_usb2_phy, phy)

struct sti8416_usb2_phy {
	struct usb_phy phy;
	struct reset_control *rstc;
};

static int sti8416_usb2_phy_init(struct usb_phy *phy)
{
	struct sti8416_usb2_phy *phy_dev = phy_to_priv(phy);

	reset_control_deassert(phy_dev->rstc);
	return 0;
}


static void sti8416_usb2_phy_shutdown(struct usb_phy *phy)
{
	struct sti8416_usb2_phy *phy_dev = phy_to_priv(phy);

	reset_control_assert(phy_dev->rstc);
}

static const struct of_device_id sti8416_usb2_phy_of_match[];

static int sti8416_usb2_phy_probe(struct platform_device *pdev)
{
	const struct of_device_id *match;
	struct sti8416_usb2_phy *phy_dev;
	struct device *dev = &pdev->dev;
	struct usb_phy *phy;

	phy_dev = devm_kzalloc(dev, sizeof(*phy_dev), GFP_KERNEL);
	if (!phy_dev)
		return -ENOMEM;

	match = of_match_device(sti8416_usb2_phy_of_match, &pdev->dev);
	if (!match)
		return -ENODEV;

	phy_dev->rstc = devm_reset_control_get(dev, NULL);
	if (IS_ERR(phy_dev->rstc)) {
		dev_err(dev, "failed to ctrl PHY reset\n");
		return PTR_ERR(phy_dev->rstc);
	}

	dev_info(dev, "reset PHY\n");

	phy = &phy_dev->phy;
	phy->dev = dev;
	phy->label = "STi8416 USB2 PHY";
	phy->init = sti8416_usb2_phy_init;
	phy->shutdown = sti8416_usb2_phy_shutdown;
	phy->type = USB_PHY_TYPE_USB2;

	usb_add_phy_dev(phy);

	platform_set_drvdata(pdev, phy_dev);

	dev_info(dev, "STi8416 USB2 PHY probed\n");

	return 0;
}

static int sti8416_usb2_phy_remove(struct platform_device *pdev)
{
	struct sti8416_usb2_phy *phy_dev = platform_get_drvdata(pdev);

	reset_control_assert(phy_dev->rstc);

	usb_remove_phy(&phy_dev->phy);

	return 0;
}

static const struct of_device_id sti8416_usb2_phy_of_match[] = {
	{ .compatible = "st,sti8416-usb2phy", },
	{},
};

MODULE_DEVICE_TABLE(of, sti8416_usb2_phy_of_match);

static struct platform_driver sti8416_usb2_phy_driver = {
	.probe = sti8416_usb2_phy_probe,
	.remove = sti8416_usb2_phy_remove,
	.driver = {
		   .name = "sti8416-usb2-phy",
		   .owner = THIS_MODULE,
		   .of_match_table = sti8416_usb2_phy_of_match,
		   }
};

module_platform_driver(sti8416_usb2_phy_driver);

MODULE_AUTHOR("Giuseppe Cavallaro <peppe.cavallaro@st.com>");
MODULE_DESCRIPTION("STMicroelectronics USB2 PHY driver for STi8416 SoC");
MODULE_LICENSE("GPL v2");
