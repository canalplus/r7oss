/*
 * xhci-brcm.c - xHCI host controller driver platform Bus Glue.
 *
 * Copyright (C) 2012 Texas Instruments Incorporated - http://www.ti.com
 * Author: Sebastian Andrzej Siewior <bigeasy@linutronix.de>
 *
 * A lot of code borrowed from the Linux xHCI driver.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 */

#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <linux/dma-mapping.h>
#include <linux/clk.h>

#include "xhci.h"
#include "usb-brcm.h"

static struct hc_driver __read_mostly xhci_brcm_hc_driver;


#define BRCM_DRIVER_DESC "xHCI BRCM driver"
#define BRCM_DRIVER_NAME "xhci-brcm"

struct brcm_hcd {
	/*
	 * xhci_hcd_ptr must always be the first entry
	 * because the XHCI driver expects this pointer
	 * to be at the beginning of the hcd_priv area
	 * in the usb_hcd structure.
	 */
	struct xhci_hcd *xhci_hcd_ptr;
	struct clk *hcd_clk;
};

static struct brcm_hcd *hcd_to_brcm(struct usb_hcd *hcd)
{
	return (struct brcm_hcd *)hcd->hcd_priv;
}

static void xhci_brcm_quirks(struct device *dev, struct xhci_hcd *xhci)
{
	/*
	 * As of now platform drivers don't provide MSI support so we ensure
	 * here that the generic code does not try to make a pci_dev from our
	 * dev struct in order to setup MSI
	 */
	xhci->quirks |= XHCI_PLAT;

	/*
	 * The Broadcom XHCI core does not support save/restore state
	 * so we need to reset on resume.
	 */
	xhci->quirks |= XHCI_RESET_ON_RESUME;
}

/* called during probe() after chip reset completes */
static int xhci_brcm_setup(struct usb_hcd *hcd)
{
	return xhci_gen_setup(hcd, xhci_brcm_quirks);
}

static int xhci_brcm_probe(struct platform_device *pdev)
{
	struct xhci_hcd		*xhci;
	struct usb_hcd		*hcd;
	struct clk		*usb_clk;
	struct brcm_hcd		*brcm_hcd_ptr;
	int			ret;

	ret = dma_coerce_mask_and_coherent(&pdev->dev, DMA_BIT_MASK(64));
	if (ret)
		return ret;

	ret = brcm_usb_probe(pdev, &xhci_brcm_hc_driver, &hcd, &usb_clk);
	if (ret)
		return ret;
	brcm_hcd_ptr = hcd_to_brcm(hcd);
	brcm_hcd_ptr->hcd_clk = usb_clk;

	/* USB 2.0 roothub is stored in the platform_device now. */
	hcd = platform_get_drvdata(pdev);
	xhci = hcd_to_xhci(hcd);
	xhci->shared_hcd = usb_create_shared_hcd(&xhci_brcm_hc_driver,
			&pdev->dev, dev_name(&pdev->dev), hcd);
	if (!xhci->shared_hcd) {
		ret = -ENOMEM;
		goto dealloc_usb2_hcd;
	}

	/*
	 * Set the xHCI pointer before xhci_brcm_setup() (aka hcd_driver.reset)
	 * is called by usb_add_hcd().
	 */
	*((struct xhci_hcd **) xhci->shared_hcd->hcd_priv) = xhci;

	ret = usb_add_hcd(xhci->shared_hcd, hcd->irq, IRQF_SHARED);
	if (ret)
		goto put_usb3_hcd;
	brcm_hcd_ptr = hcd_to_brcm(hcd);
	brcm_hcd_ptr->hcd_clk = usb_clk;

	return 0;

put_usb3_hcd:
	usb_put_hcd(xhci->shared_hcd);

dealloc_usb2_hcd:
	usb_remove_hcd(hcd);
	usb_put_hcd(hcd);

	return ret;
}

static int xhci_brcm_remove(struct platform_device *dev)
{
	struct usb_hcd	*hcd = platform_get_drvdata(dev);
	struct xhci_hcd	*xhci = hcd_to_xhci(hcd);
	struct brcm_hcd *brcm_hcd_ptr = hcd_to_brcm(hcd);

	usb_remove_hcd(xhci->shared_hcd);
	usb_put_hcd(xhci->shared_hcd);

	brcm_usb_remove(dev, brcm_hcd_ptr->hcd_clk);
	kfree(xhci);

	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int xhci_brcm_suspend(struct device *dev)
{
	int ret;
	struct usb_hcd	*hcd = dev_get_drvdata(dev);
	struct xhci_hcd	*xhci = hcd_to_xhci(hcd);
	struct brcm_hcd *brcm_hcd_ptr = hcd_to_brcm(hcd);

	ret = xhci_suspend(xhci, device_may_wakeup(dev));
	clk_disable(brcm_hcd_ptr->hcd_clk);
	return ret;
}

static int xhci_brcm_resume(struct device *dev)
{
	struct usb_hcd	*hcd = dev_get_drvdata(dev);
	struct xhci_hcd	*xhci = hcd_to_xhci(hcd);
	struct brcm_hcd *brcm_hcd_ptr = hcd_to_brcm(hcd);
	int err;

	err = clk_enable(brcm_hcd_ptr->hcd_clk);
	if (err)
		return err;
	return xhci_resume(xhci, 0);
}

#endif /* CONFIG_PM_SLEEP */

static SIMPLE_DEV_PM_OPS(xhci_brcm_pm_ops, xhci_brcm_suspend,
		xhci_brcm_resume);

#ifdef CONFIG_OF
static const struct of_device_id brcm_xhci_of_match[] = {
	{ .compatible = "xhci-platform" },
	{ .compatible = "brcm,xhci-brcm" },
	{ .compatible = "brcm,xhci-brcm-v2" },
	{ },
};
MODULE_DEVICE_TABLE(of, brcm_xhci_of_match);
#endif

static struct platform_driver xhci_brcm_driver = {
	.probe	= xhci_brcm_probe,
	.remove	= xhci_brcm_remove,
	.driver	= {
		.name = BRCM_DRIVER_NAME,
		.pm = &xhci_brcm_pm_ops,
		.of_match_table = of_match_ptr(brcm_xhci_of_match),
	},
};

static int __init xhci_brcm_init(void)
{
	if (usb_disabled())
		return -ENODEV;
	pr_info("%s: " BRCM_DRIVER_DESC "\n", BRCM_DRIVER_NAME);
	xhci_init_driver(&xhci_brcm_hc_driver, xhci_brcm_setup);
	xhci_brcm_hc_driver.hcd_priv_size = sizeof(struct brcm_hcd);
	return platform_driver_register(&xhci_brcm_driver);
}
module_init(xhci_brcm_init);

static void __exit xhci_brcm_cleanup(void)
{
	platform_driver_unregister(&xhci_brcm_driver);
}
module_exit(xhci_brcm_cleanup);

MODULE_DESCRIPTION(BRCM_DRIVER_DESC);
MODULE_AUTHOR("Al Cooper");
MODULE_LICENSE("GPL");
