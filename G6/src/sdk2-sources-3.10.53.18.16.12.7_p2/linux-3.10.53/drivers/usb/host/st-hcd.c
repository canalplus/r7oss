/*
 * STMicroelectronics HCD (Host Controller Driver) for USB 2.0 and 1.1.
 *
 * Copyright (c) 2013 STMicroelectronics (R&D) Ltd.
 * Author: Stephen Gallimore <stephen.gallimore@st.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2, as
 * published by the Free Software Foundation.
 */

#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/dma-mapping.h>
#include <linux/of.h>
#include <linux/pm_runtime.h>
#include <linux/pm_clock.h>
#include <linux/delay.h>
#include <linux/usb.h>
#include <linux/usb/hcd.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/reset.h>
#include <linux/regmap.h>
#include <linux/st_amba_bridge.h>
#include <linux/phy/phy.h>
#include <linux/mfd/syscon.h>
#include <linux/usb/ohci_pdriver.h>
#include <linux/usb/ehci_pdriver.h>

#include "ohci.h"

#define EHCI_CAPS_SIZE 0x10
#define AHB2STBUS_INSREG01 (EHCI_CAPS_SIZE + 0x84)

struct st_hcd_dev {
	int port_nr;
	struct platform_device *ehci_device;
	struct platform_device *ohci_device;
	struct clk *ic_clk; /* Interconnect clock to the controller block */
	struct clk *ohci_clk; /* 48MHz clock for OHCI */
	struct reset_control *pwr;
	struct reset_control *rst;
	struct phy *phy;
};

static inline void st_ehci_configure_bus(void __iomem *regs)
{
	/* Set EHCI packet buffer IN/OUT threshold to 128 bytes */
	u32 threshold = 128 | (128 << 16);
	writel(threshold, regs + AHB2STBUS_INSREG01);
}

static const struct usb_ehci_pdata ehci_pdata = {
};

static const struct usb_ohci_pdata ohci_pdata = {
};

static int st_hcd_remove(struct platform_device *pdev)
{
	struct st_hcd_dev *hcd_dev = platform_get_drvdata(pdev);

	platform_device_unregister(hcd_dev->ehci_device);
	platform_device_unregister(hcd_dev->ohci_device);

	phy_power_off(hcd_dev->phy);

	reset_control_assert(hcd_dev->rst);
	reset_control_assert(hcd_dev->pwr);

	if (!IS_ERR(hcd_dev->ohci_clk))
		clk_disable_unprepare(hcd_dev->ohci_clk);

	clk_disable_unprepare(hcd_dev->ic_clk);

	return 0;
}

static struct platform_device *st_hcd_device_create(const char *name, int id,
		struct platform_device *parent)
{
	struct platform_device *pdev;
	const char *platform_name;
	struct resource *res;
	struct resource hcd_res[2];
	int ret;

	res = platform_get_resource_byname(parent, IORESOURCE_MEM, name);
	if (!res)
		return ERR_PTR(-ENODEV);

	hcd_res[0] = *res;

	res = platform_get_resource_byname(parent, IORESOURCE_IRQ, name);
	if (!res)
		return ERR_PTR(-ENODEV);

	hcd_res[1] = *res;

	platform_name = kasprintf(GFP_KERNEL, "%s-platform", name);
	if (!platform_name)
		return ERR_PTR(-ENOMEM);

	pdev = platform_device_alloc(platform_name, id);

	kfree(platform_name);

	if (!pdev)
		return ERR_PTR(-ENOMEM);

	pdev->dev.parent = &parent->dev;
	pdev->dev.dma_mask = &pdev->dev.coherent_dma_mask;
	pdev->dev.coherent_dma_mask = DMA_BIT_MASK(32);

	ret = platform_device_add_resources(pdev, hcd_res, ARRAY_SIZE(hcd_res));
	if (ret)
		goto error;

	if (!strcmp(name, "ohci")) {
		ret = platform_device_add_data(pdev, &ohci_pdata,
					       sizeof(ohci_pdata));
	} else {
		ret = platform_device_add_data(pdev, &ehci_pdata,
					       sizeof(ehci_pdata));
	}

	if (ret)
		goto error;

	ret = platform_device_add(pdev);
	if (ret)
		goto error;

	return pdev;

error:
	platform_device_put(pdev);
	return ERR_PTR(ret);
}

#ifdef CONFIG_PM_SLEEP
static int st_hcd_resume(struct device *dev)
{
	struct st_hcd_dev *hcd_dev = dev_get_drvdata(dev);
	struct usb_hcd *ehci_hcd = platform_get_drvdata(hcd_dev->ehci_device);
	int err;

	pinctrl_pm_select_default_state(dev);

	clk_prepare_enable(hcd_dev->ic_clk);
	/*
	 * This assumes that the re-enable will cause the correct
	 * clock frequency to be reprogrammed after a PM_SUSPEND_MEM
	 * or hibernate.
	 */
	if (!IS_ERR(hcd_dev->ohci_clk))
		clk_prepare_enable(hcd_dev->ohci_clk);

	phy_init(hcd_dev->phy);

	err = phy_power_on(hcd_dev->phy);
	if (err && (err != -ENOTSUPP))
		return err;

	err = reset_control_deassert(hcd_dev->rst);
	if (err)
		return err;

	err = reset_control_deassert(hcd_dev->pwr);
	if (err)
		return err;

	st_ehci_configure_bus(ehci_hcd->regs);

	return 0;
}

static int st_hcd_suspend(struct device *dev)
{
	struct st_hcd_dev *hcd_dev = dev_get_drvdata(dev);
	int err;


	err = reset_control_assert(hcd_dev->pwr);
	if (err)
		return err;

	err = reset_control_assert(hcd_dev->rst);
	if (err)
		return err;

	err = phy_power_off(hcd_dev->phy);
	if (err && (err != -ENOTSUPP))
		return err;

	phy_exit(hcd_dev->phy);

	if (!IS_ERR(hcd_dev->ohci_clk))
		clk_disable_unprepare(hcd_dev->ohci_clk);

	clk_disable_unprepare(hcd_dev->ic_clk);

	pinctrl_pm_select_sleep_state(dev);

	return 0;
}
static SIMPLE_DEV_PM_OPS(st_hcd_pm, st_hcd_suspend, st_hcd_resume);
#define ST_HCD_PM	(&st_hcd_pm)
#else
#define ST_HCD_PM	NULL
#endif


static struct of_device_id st_hcd_match[] = {
	{ .compatible = "st,usb-300x" },
	{},
};
MODULE_DEVICE_TABLE(of, st_hcd_match);

static int st_hcd_probe_clocks(struct platform_device *pdev,
				struct st_hcd_dev *hcd_dev)
{
	hcd_dev->ic_clk = devm_clk_get(&pdev->dev, "ic");
	if (IS_ERR(hcd_dev->ic_clk)) {
		dev_err(&pdev->dev, "ic clk not found\n");
		return PTR_ERR(hcd_dev->ic_clk);
	}

	hcd_dev->ohci_clk = devm_clk_get(&pdev->dev, "ohci");
	if (IS_ERR(hcd_dev->ohci_clk))
		dev_info(&pdev->dev, "48MHz ohci clk not found\n");

	/*
	 * The interconnect input clock have either fixed
	 * rate or the rate is defined on boot, so we are only concerned about
	 * enabling any gates for this clock.
	 */
	clk_prepare_enable(hcd_dev->ic_clk);
	/*
	 * The 48MHz OHCI clock is usually provided by a programmable
	 * frequency synthesizer, which is often not programmed on boot/chip
	 * reset, so we set its rate here to ensure it is correct.
	 */
	if (!IS_ERR(hcd_dev->ohci_clk)) {
		clk_set_rate(hcd_dev->ohci_clk, 48000000);
		clk_prepare_enable(hcd_dev->ohci_clk);
	}

	return 0;
}

static int st_hcd_probe_resets(struct platform_device *pdev,
				struct st_hcd_dev *hcd_dev)
{
	int err;

	hcd_dev->pwr = devm_reset_control_get(&pdev->dev, "power");
	if (IS_ERR(hcd_dev->pwr)) {
		dev_err(&pdev->dev, "power reset control not found\n");
		return PTR_ERR(hcd_dev->pwr);
	}

	err = reset_control_deassert(hcd_dev->pwr);
	if (err) {
		dev_err(&pdev->dev, "unable to bring out of powerdown\n");
		return err;
	}

	hcd_dev->rst = devm_reset_control_get(&pdev->dev, "softreset");
	if (IS_ERR(hcd_dev->rst)) {
		dev_err(&pdev->dev, "Soft reset control not found\n");
		return PTR_ERR(hcd_dev->rst);
	}

	err = reset_control_deassert(hcd_dev->rst);
	if (err) {
		dev_err(&pdev->dev, "unable to bring out of softreset\n");
		return err;
	}

	return 0;
}

static int st_hcd_probe_ehci_setup(struct platform_device *pdev)
{
	struct resource *res;
	struct st_amba_bridge *amba_bridge;
	struct st_amba_bridge_config *amba_config;
	void __iomem *amba_base;
	void __iomem *ehci_regs;

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "protocol");
	if (!res)
		return -ENODEV;

	amba_base = devm_ioremap_nocache(&pdev->dev, res->start,
					 resource_size(res));
	if (!amba_base)
		return -ENOMEM;

	amba_config = st_of_get_amba_config(&pdev->dev);
	amba_bridge = st_amba_bridge_create(&pdev->dev, amba_base, amba_config);

	if (IS_ERR(amba_bridge))
		return PTR_ERR(amba_bridge);

	st_amba_bridge_init(amba_bridge);

	/*
	 * We need to do some integration specific setup in the EHCI
	 * controller, which the EHCI platform driver does not provide any
	 * hooks to allow us to do during it's initialisation.
	 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "ehci");
	if (!res)
		return -ENODEV;

	ehci_regs = devm_ioremap(&pdev->dev, res->start, resource_size(res));
	if (!ehci_regs)
		return -ENOMEM;

	st_ehci_configure_bus(ehci_regs);
	devm_iounmap(&pdev->dev, ehci_regs);

	return 0;
}

static int st_hcd_probe(struct platform_device *pdev)
{
	struct st_hcd_dev *hcd_dev;
	int id;
	int err;

	if (!pdev->dev.of_node)
		return -ENODEV;

	id = of_alias_get_id(pdev->dev.of_node, "usb");

	if (id < 0) {
		dev_err(&pdev->dev, "No ID specified via DT alias\n");
		return -ENODEV;
	}

	hcd_dev = devm_kzalloc(&pdev->dev, sizeof(*hcd_dev), GFP_KERNEL);
	if (!hcd_dev)
		return -ENOMEM;

	hcd_dev->port_nr = id;

	err = st_hcd_probe_clocks(pdev, hcd_dev);
	if (err)
		return err;

	err = st_hcd_probe_resets(pdev, hcd_dev);
	if (err)
		return err;

	err = st_hcd_probe_ehci_setup(pdev);
	if (err)
		return err;

	hcd_dev->phy = devm_phy_get(&pdev->dev, "usb2-phy");
	if (IS_ERR(hcd_dev->phy)) {
		dev_err(&pdev->dev, "no PHY configured\n");
		return PTR_ERR(hcd_dev->phy);
	}

	phy_init(hcd_dev->phy);

	err = phy_power_on(hcd_dev->phy);
	if (err && (err != -ENOTSUPP))
		return err;

	hcd_dev->ehci_device = st_hcd_device_create("ehci", id, pdev);
	if (IS_ERR(hcd_dev->ehci_device))
		return PTR_ERR(hcd_dev->ehci_device);

	hcd_dev->ohci_device = st_hcd_device_create("ohci", id, pdev);
	if (IS_ERR(hcd_dev->ohci_device)) {
		platform_device_del(hcd_dev->ehci_device);
		return PTR_ERR(hcd_dev->ohci_device);
	}

	platform_set_drvdata(pdev, hcd_dev);

	return 0;
}

static struct platform_driver st_hcd_driver = {
	.probe = st_hcd_probe,
	.remove = st_hcd_remove,
	/*
	 * No shutdown required, the EHCI and OHCI device shutdowns will
	 * stop the controllers ready for the kexec use case.
	 */
	.driver = {
		.name = "st-hcd",
		.owner = THIS_MODULE,
		.pm = ST_HCD_PM,
		.of_match_table = st_hcd_match,
	},
};

module_platform_driver(st_hcd_driver);

MODULE_DESCRIPTION("STMicroelectronics On-Chip USB Host Controller");
MODULE_AUTHOR("Stephen Gallimore <stephen.gallimore@st.com>");
MODULE_LICENSE("GPL v2");
