/*
 * Copyright (C) 2010 Broadcom Corporation
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
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include <linux/usb.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_platform.h>
#include <linux/usb/hcd.h>

#include "usb-brcm-common-init.h"

struct brcm_usb_instance {
	void __iomem		*ctrl_regs;
	void __iomem		*xhci_ec_regs;
	int			ioc;
	int			ipp;
	int			has_xhci;
	int			device_mode;
	struct clk		*usb_clk;
};

static const char msg_clk_not_found[] = "Clock not found in Device Tree\n";

/***********************************************************************
 * Library functions
 ***********************************************************************/

int brcm_usb_probe(struct platform_device *pdev,
		const struct hc_driver *hc_driver,
		struct usb_hcd **hcdptr,
		struct clk **hcd_clk_ptr)
{
	struct resource *res_mem;
	int irq;
	struct usb_hcd *hcd;
	struct device_node *dn = pdev->dev.of_node;
	struct clk *usb_clk;
	int err;

	if (usb_disabled())
		return -ENODEV;

	res_mem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res_mem) {
		dev_err(&pdev->dev, "platform_get_resource error.\n");
		return -ENODEV;
	}

	irq = platform_get_irq(pdev, 0);
	if (irq < 0) {
		dev_err(&pdev->dev, "platform_get_irq error.\n");
		return -ENODEV;
	}

	usb_clk = of_clk_get_by_name(dn, "sw_usb");
	if (IS_ERR(usb_clk)) {
		dev_err(&pdev->dev, msg_clk_not_found);
		usb_clk = NULL;
	}
	err = clk_prepare_enable(usb_clk);
	if (err)
		return err;
	*hcd_clk_ptr = usb_clk;

	/* initialize hcd */
	hcd = usb_create_hcd(hc_driver, &pdev->dev, dev_name(&pdev->dev));
	if (!hcd) {
		dev_err(&pdev->dev, "Failed to create hcd\n");
		clk_disable(usb_clk);
		return -ENOMEM;
	}
	*hcdptr = hcd;
	hcd->rsrc_start = res_mem->start;
	hcd->rsrc_len = resource_size(res_mem);

	hcd->regs = devm_ioremap_resource(&pdev->dev, res_mem);
	if (IS_ERR(hcd->regs)) {
		err = PTR_ERR(hcd->regs);
		goto err_put_hcd;
	}
	err = usb_add_hcd(hcd, irq, IRQF_SHARED);
	if (err)
		goto err_put_hcd;

	device_wakeup_enable(hcd->self.controller);
	platform_set_drvdata(pdev, hcd);
	return err;

err_put_hcd:
	clk_disable(usb_clk);
	usb_put_hcd(hcd);

	return err;
}
EXPORT_SYMBOL(brcm_usb_probe);

int brcm_usb_remove(struct platform_device *pdev, struct clk *hcd_clk)
{
	struct usb_hcd *hcd = platform_get_drvdata(pdev);
	usb_remove_hcd(hcd);
	usb_put_hcd(hcd);
	clk_disable(hcd_clk);

	return 0;
}
EXPORT_SYMBOL(brcm_usb_remove);


#ifdef CONFIG_OF

/***********************************************************************
 * DT support for USB instances
 ***********************************************************************/

static int brcm_usb_instance_probe(struct platform_device *pdev)
{
	struct device_node *dn = pdev->dev.of_node;
	struct resource ctrl_res;
	struct resource xhci_ec_res;
	const u32 *prop;
	struct brcm_usb_instance *priv;
	struct device_node *node;
	struct brcm_usb_common_init_params params;
	int err;
	const char *device_mode;

	/*
	 * if there is an alias for "usbphy0", it means we have the new
	 * style USB device tree without the USB wrapper.
	 */
	node = of_find_node_by_path("/aliases");
	if (node) {
		const char *usbphy = of_get_property(node, "usbphy0", NULL);
		if (usbphy)
			return -ENODEV;
	}

	priv = devm_kzalloc(&pdev->dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;
	dev_set_drvdata(&pdev->dev, priv);

	if (of_address_to_resource(dn, 0, &ctrl_res)) {
		dev_err(&pdev->dev, "can't get USB_CTRL base address\n");
		return -EINVAL;
	}
	priv->ctrl_regs = devm_request_and_ioremap(&pdev->dev, &ctrl_res);
	if (!priv->ctrl_regs) {
		dev_err(&pdev->dev, "can't map register space\n");
		return -EINVAL;
	}
	if (!of_address_to_resource(dn, 1, &xhci_ec_res))
		priv->xhci_ec_regs =
			devm_request_and_ioremap(&pdev->dev, &xhci_ec_res);
	prop = of_get_property(dn, "ipp", NULL);
	if (prop)
		priv->ipp = be32_to_cpup(prop);
	prop = of_get_property(dn, "ioc", NULL);
	if (prop)
		priv->ioc = be32_to_cpup(prop);
	err = of_property_read_string(dn, "device", &device_mode);
	priv->device_mode = USB_CTLR_DEVICE_OFF;
	if (err == 0) {
		if (strcmp(device_mode, "on") == 0)
			priv->device_mode = USB_CTLR_DEVICE_ON;
		if (strcmp(device_mode, "dual") == 0)
			priv->device_mode = USB_CTLR_DEVICE_DUAL;
	}
	node = of_find_compatible_node(dn, NULL, "xhci-platform");
	of_node_put(node);
	priv->has_xhci = node != NULL;
	priv->usb_clk = of_clk_get_by_name(dn, "sw_usb");
	if (IS_ERR(priv->usb_clk)) {
		dev_err(&pdev->dev, msg_clk_not_found);
		priv->usb_clk = NULL;
	}
	err = clk_prepare_enable(priv->usb_clk);
	if (err)
		return err;
	params.ctrl_regs = (uintptr_t)priv->ctrl_regs;
	params.ioc = priv->ioc;
	params.ipp = priv->ipp;
	params.has_xhci = priv->has_xhci;
	params.xhci_ec_regs = (uintptr_t)priv->xhci_ec_regs;
	params.device_mode = priv->device_mode;
	brcm_usb_common_init(&params);
	return of_platform_populate(dn, NULL, NULL, NULL);
}

#ifdef CONFIG_PM_SLEEP
static int brcm_usb_instance_suspend(struct device *dev)
{
	struct brcm_usb_instance *priv = dev_get_drvdata(dev);

	clk_disable(priv->usb_clk);
	return 0;
}

static int brcm_usb_instance_resume(struct device *dev)
{
	struct brcm_usb_instance *priv = dev_get_drvdata(dev);
	struct brcm_usb_common_init_params params;

	clk_enable(priv->usb_clk);
	params.ctrl_regs = (uintptr_t)priv->ctrl_regs;
	params.ioc = priv->ioc;
	params.ipp = priv->ipp;
	params.has_xhci = priv->has_xhci;
	params.xhci_ec_regs = (uintptr_t)priv->xhci_ec_regs;
	params.device_mode = priv->device_mode;
	brcm_usb_common_init(&params);
	return 0;
}
#endif /* CONFIG_PM_SLEEP */

static SIMPLE_DEV_PM_OPS(brcm_usb_instance_pm_ops, brcm_usb_instance_suspend,
		brcm_usb_instance_resume);

static const struct of_device_id brcm_usb_instance_match[] = {
	{ .compatible = "brcm,usb-instance" },
	{},
};

static struct platform_driver brcm_usb_instance_driver = {
	.driver = {
		.name = "usb-brcm",
		.bus = &platform_bus_type,
		.of_match_table = of_match_ptr(brcm_usb_instance_match),
		.pm = &brcm_usb_instance_pm_ops,
	}
};

/*
 * We really don't want to try to undo of_platform_populate(), so it
 * is not possible to unbind/deregister this driver.
 */
static int __init brcm_usb_instance_init(void)
{
	return platform_driver_probe(&brcm_usb_instance_driver,
				     brcm_usb_instance_probe);
}
module_init(brcm_usb_instance_init);

#endif

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Broadcom Corporation");
MODULE_DESCRIPTION("Broadcom USB common functions");
