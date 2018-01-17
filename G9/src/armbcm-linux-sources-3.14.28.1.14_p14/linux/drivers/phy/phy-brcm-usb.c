/*
 * phy-brcm-usb.c - Broadcom USB Phy Driver
 *
 * Copyright (C) 2015 Broadcom Corporation
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/phy/phy.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include "../drivers/usb/host/usb-brcm-common-init.h"
#include <linux/brcmstb/brcmstb.h>

enum brcm_usb_phy_id {
	BRCM_USB_PHY_2_0,
	BRCM_USB_PHY_3_0,
	BRCM_USB_PHY_ID_MAX
};

struct brcm_usb_phy_data {
	void __iomem		*ctrl_regs;
	void __iomem		*xhci_ec_regs;
	int			ioc;
	int			ipp;
	int			has_xhci;
	int			device_mode;
	struct clk		*usb_clk;
	int			num_phys;
	struct brcm_usb_phy {
		struct phy *phy;
		unsigned int index;
	} phys[BRCM_USB_PHY_ID_MAX];
};

#define to_brcm_usb_phy_data(phy) \
	container_of((phy), struct brcm_usb_phy_data, phys[(phy)->index])

static struct phy_ops brcm_usb_phy_ops = {
	.owner		= THIS_MODULE,
};

static struct phy *brcm_usb_phy_xlate(struct device *dev,
				struct of_phandle_args *args)
{
	struct brcm_usb_phy_data *data = dev_get_drvdata(dev);

	if (args->args[0] >= data->num_phys)
		return ERR_PTR(-ENODEV);

	return data->phys[args->args[0]].phy;
}


static ssize_t ipp_show(struct device *dev,
			       struct device_attribute *attr,
			       char *buf)
{
	struct brcm_usb_phy_data *priv = dev_get_drvdata(dev);
	uint32_t reg = 0;
	uintptr_t ctrl = (uintptr_t)priv->ctrl_regs;

    if (ctrl) {
        reg = (DEV_RD(ctrl) & BCHP_USB_CTRL_SETUP_IPP_MASK) >> BCHP_USB_CTRL_SETUP_IPP_SHIFT;
    }
    
    return sprintf(buf, "%d\n", reg);
}

static ssize_t ipp_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t len)
{
	struct brcm_usb_phy_data *priv = dev_get_drvdata(dev);
	uint32_t reg;
	uintptr_t ctrl = (uintptr_t)priv->ctrl_regs;
	u8 val;
    
	if (kstrtou8(buf, 0, &val))
        return -EINVAL;

    if (ctrl) {
        reg = DEV_RD(ctrl);
        reg &= ~BCHP_USB_CTRL_SETUP_IPP_MASK;

        if (val) {
            reg |= BCHP_USB_CTRL_SETUP_IPP_MASK;
        }
        DEV_WR(ctrl, reg);
    }
    
    return len;
}

static struct device_attribute usb_attrs[] = {
	__ATTR(ipp, S_IWUSR | S_IRUGO, ipp_show, ipp_store),
};


static int brcm_usb_phy_probe(struct platform_device *pdev)
{
	struct resource *res;
	struct device *dev = &pdev->dev;
	struct brcm_usb_phy_data *priv;
	struct phy *gphy;
	struct phy_provider *phy_provider;
	struct device_node *dn = pdev->dev.of_node;
	const u32 *prop;
	int i;
	int err;
	struct brcm_usb_common_init_params params;
	const char *device_mode;
	char err_msg_ioremap[] = "can't map register space\n";

	priv = devm_kzalloc(dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;
	platform_set_drvdata(pdev, priv);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (res == NULL) {
		dev_err(&pdev->dev, "can't get USB_CTRL base address\n");
		return -EINVAL;
	}
	priv->ctrl_regs = devm_request_and_ioremap(&pdev->dev, res);
	if (!priv->ctrl_regs) {
		dev_err(&pdev->dev, err_msg_ioremap);
		return -EINVAL;
	}

	/* The XHCI EC registers are optional */
	res = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	if (res != NULL) {
		priv->xhci_ec_regs =
			devm_request_and_ioremap(&pdev->dev, res);
		if (!priv->xhci_ec_regs) {
			dev_err(&pdev->dev, err_msg_ioremap);
			return -EINVAL;
		}
	}

	of_property_read_u32(dn, "ipp", &priv->ipp);
	of_property_read_u32(dn, "ioc", &priv->ioc);

	priv->device_mode = USB_CTLR_DEVICE_OFF;
	err = of_property_read_string(dn, "device", &device_mode);
	if (err == 0) {
		if (strcmp(device_mode, "on") == 0)
			priv->device_mode = USB_CTLR_DEVICE_ON;
		if (strcmp(device_mode, "dual") == 0)
			priv->device_mode = USB_CTLR_DEVICE_DUAL;
	}

	prop = of_get_property(dn, "has_xhci", NULL);
	if (prop) {
		priv->has_xhci = 1;
		priv->num_phys = 2;
	} else {
		priv->num_phys = 1;
	}

	for (i = 0; i < priv->num_phys; i++) {
		gphy = devm_phy_create(dev, &brcm_usb_phy_ops, NULL);
		if (IS_ERR(gphy)) {
			dev_err(dev, "failed to create PHY %d\n", i);
			return PTR_ERR(gphy);
		}
		priv->phys[i].phy = gphy;
		priv->phys[i].index = i;
		phy_set_drvdata(gphy, &priv->phys[i]);
	}
	phy_provider = devm_of_phy_provider_register(dev,
			brcm_usb_phy_xlate);
	if (IS_ERR(phy_provider))
		return PTR_ERR(phy_provider);

	priv->usb_clk = of_clk_get_by_name(dn, "sw_usb");
	if (IS_ERR(priv->usb_clk)) {
		dev_err(&pdev->dev, "Clock not found in Device Tree\n");
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

    /* ipp sysfs registry */
    device_create_file(&pdev->dev, &usb_attrs[0]);
    
    return 0;
}

#ifdef CONFIG_PM_SLEEP
static int brcm_usb_phy_suspend(struct device *dev)
{
	struct brcm_usb_phy_data *priv = dev_get_drvdata(dev);

	clk_disable(priv->usb_clk);
	return 0;
}

static int brcm_usb_phy_resume(struct device *dev)
{
	struct brcm_usb_common_init_params params;
	struct brcm_usb_phy_data *priv = dev_get_drvdata(dev);

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

static SIMPLE_DEV_PM_OPS(brcm_usb_phy_pm_ops, brcm_usb_phy_suspend,
		brcm_usb_phy_resume);

static const struct of_device_id brcm_usb_dt_ids[] = {
	{ .compatible = "brcm,usb-phy" },
	{ /* sentinel */ }
};

MODULE_DEVICE_TABLE(of, brcm_usb_dt_ids);

static struct platform_driver brcm_usb_driver = {
	.probe		= brcm_usb_phy_probe,
	.driver		= {
		.name	= "brcm-usb-phy",
		.owner	= THIS_MODULE,
		.pm = &brcm_usb_phy_pm_ops,
		.of_match_table = brcm_usb_dt_ids,
	},
};

module_platform_driver(brcm_usb_driver);

MODULE_ALIAS("platform:brcm-usb-phy");
MODULE_AUTHOR("Al Cooper <acooper@broadcom.com>");
MODULE_DESCRIPTION("BRCM USB PHY driver");
MODULE_LICENSE("GPL v2");
