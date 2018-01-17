/*
 * Copyright (C) 2009 - 2014 Broadcom Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 */

#include <linux/dma-mapping.h>
#include <linux/hrtimer.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/err.h>
#include <linux/platform_device.h>
#include <linux/usb/ohci_pdriver.h>
#include <linux/usb.h>
#include <linux/usb/hcd.h>
#include <linux/clk.h>
#include <linux/of.h>

#include "ohci.h"
#include "usb-brcm.h"

#define BRCM_DRIVER_DESC "OHCI BRCM driver"

static const char hcd_name[] = "ohci-brcm";

struct brcm_hcd {
	struct clk *hcd_clk;
};

static int ohci_brcm_reset(struct usb_hcd *hcd)
{
	struct ohci_hcd *ohci = hcd_to_ohci(hcd);
	int err;

	ohci->flags |= OHCI_QUIRK_BE_MMIO;
	err = ohci_setup(hcd);
	/* undo OCD (distrust_firmare) quirk forced in ohci-hcd.d file */
	ohci->flags &= ~OHCI_QUIRK_HUB_POWER;

	return err;
}

static struct hc_driver __read_mostly ohci_brcm_hc_driver;

static const struct ohci_driver_overrides brcm_overrides __initconst = {
	.product_desc =	"BRCM OHCI controller",
	.reset =	ohci_brcm_reset,
	.extra_priv_size = sizeof(struct brcm_hcd),
};

static int ohci_brcm_probe(struct platform_device *dev)
{
	int err;
	struct usb_hcd *hcd;
	struct brcm_hcd *brcm_hcd_ptr;
	struct clk *usb_clk;

	err = dma_coerce_mask_and_coherent(&dev->dev, DMA_BIT_MASK(32));
	if (err)
		return err;
	err = brcm_usb_probe(dev, &ohci_brcm_hc_driver, &hcd, &usb_clk);
	if (!err) {
		brcm_hcd_ptr = (struct brcm_hcd *)hcd_to_ohci(hcd)->priv;
		brcm_hcd_ptr->hcd_clk = usb_clk;
	}
	return err;
}

static int ohci_brcm_remove(struct platform_device *dev)
{
	struct usb_hcd *hcd = platform_get_drvdata(dev);
	struct brcm_hcd *brcm_hcd_ptr =
		(struct brcm_hcd *)hcd_to_ohci(hcd)->priv;

	return brcm_usb_remove(dev, brcm_hcd_ptr->hcd_clk);
}

#ifdef CONFIG_PM_SLEEP

static int ohci_brcm_suspend(struct device *dev)
{
	int ret;
	struct usb_hcd *hcd = dev_get_drvdata(dev);
	struct brcm_hcd *brcm_hcd_ptr =
		(struct brcm_hcd *)hcd_to_ohci(hcd)->priv;
	bool do_wakeup = device_may_wakeup(dev);

	ret = ohci_suspend(hcd, do_wakeup);
	clk_disable(brcm_hcd_ptr->hcd_clk);
	return ret;
}

static int ohci_brcm_resume(struct device *dev)
{
	struct usb_hcd *hcd = dev_get_drvdata(dev);
	struct brcm_hcd *brcm_hcd_ptr =
		(struct brcm_hcd *)hcd_to_ohci(hcd)->priv;
	int err;

	err = clk_enable(brcm_hcd_ptr->hcd_clk);
	if (err)
		return err;
	ohci_resume(hcd, false);
	return 0;
}
#endif /* CONFIG_PM_SLEEP */

static SIMPLE_DEV_PM_OPS(ohci_brcm_pm_ops, ohci_brcm_suspend,
		ohci_brcm_resume);

#ifdef CONFIG_OF
static const struct of_device_id brcm_ohci_of_match[] = {
	{ .compatible = "brcm,ohci-brcm", },
	{ .compatible = "brcm,ohci-brcm-v2", },
	{}
};

MODULE_DEVICE_TABLE(of, brcm_ohci_of_match);
#endif /* CONFIG_OF */

static struct platform_driver ohci_brcm_driver = {
	.probe		= ohci_brcm_probe,
	.remove		= ohci_brcm_remove,
	.shutdown	= usb_hcd_platform_shutdown,
	.driver		= {
		.owner	= THIS_MODULE,
		.name	= "ohci-brcm",
		.pm	= &ohci_brcm_pm_ops,
		.of_match_table = of_match_ptr(brcm_ohci_of_match),
	}
};

static int __init ohci_brcm_init(void)
{
	if (usb_disabled())
		return -ENODEV;

	pr_info("%s: " BRCM_DRIVER_DESC "\n", hcd_name);

	ohci_init_driver(&ohci_brcm_hc_driver, &brcm_overrides);
	return platform_driver_register(&ohci_brcm_driver);
}
module_init(ohci_brcm_init);

static void __exit ohci_brcm_cleanup(void)
{
	platform_driver_unregister(&ohci_brcm_driver);
}
module_exit(ohci_brcm_cleanup);

MODULE_DESCRIPTION(BRCM_DRIVER_DESC);
MODULE_AUTHOR("Al Cooper");
MODULE_LICENSE("GPL");
