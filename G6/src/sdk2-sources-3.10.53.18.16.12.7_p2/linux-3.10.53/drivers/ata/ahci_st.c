/*
 * Copyright (C) 2012 STMicroelectronics Limited
 *
 * Authors: Francesco Virlinzi <francesco.virlinzi@st.com>
 *	    Alexandre Torgue <alexandre.torgue@st.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/export.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/of.h>
#include <linux/ahci_platform.h>
#include <linux/phy/phy.h>
#include <linux/reset.h>
#include <linux/io.h>
#include <linux/dma-mapping.h>

#define AHCI_OOBR		0xbc
#define AHCI_OOBR_WE		(1<<31)
#define AHCI_OOBR_CWMIN_SHIFT	24
#define AHCI_OOBR_CWMAX_SHIFT	16
#define AHCI_OOBR_CIMIN_SHIFT	8
#define AHCI_OOBR_CIMAX_SHIFT	0

struct ahci_st_drv_data {
	struct platform_device *ahci;
	struct phy *phy;
	struct reset_control *pwr;
	struct reset_control *sw_rst;
	struct reset_control *pwr_rst;
	void __iomem *mmio;
	struct clk *clk;
};

static void ahci_st_configure_oob(void __iomem *mmio)
{
	unsigned long old_val, new_val;

	new_val = (0x02 << AHCI_OOBR_CWMIN_SHIFT) |
		  (0x04 << AHCI_OOBR_CWMAX_SHIFT) |
		  (0x08 << AHCI_OOBR_CIMIN_SHIFT) |
		  (0x0C << AHCI_OOBR_CIMAX_SHIFT);

	old_val = readl(mmio + AHCI_OOBR);
	writel(old_val | AHCI_OOBR_WE, mmio + AHCI_OOBR);
	writel(new_val | AHCI_OOBR_WE, mmio + AHCI_OOBR);
	writel(new_val, mmio + AHCI_OOBR);
}

static int ahci_st_probe_resets(struct platform_device *pdev,
				struct ahci_st_drv_data *ahci_dev)
{
	int err;

	ahci_dev->pwr = devm_reset_control_get(&pdev->dev, "pwr-dwn");
	if (IS_ERR(ahci_dev->pwr)) {
		ahci_dev->pwr = NULL;
		dev_info(&pdev->dev, "power reset control not defined\n");
	} else {
		err = reset_control_deassert(ahci_dev->pwr);
		if (err) {
			dev_err(&pdev->dev, "unable to bring out of pwrdwn\n");
			return err;
		}
	}

	ahci_dev->sw_rst = devm_reset_control_get(&pdev->dev, "sw-rst");
	if (IS_ERR(ahci_dev->sw_rst)) {
		ahci_dev->sw_rst = NULL;
		dev_info(&pdev->dev, "soft reset control not defined\n");
	} else {
		err = reset_control_deassert(ahci_dev->sw_rst);
		if (err) {
			dev_err(&pdev->dev, "unable to bring out of reset\n");
			return err;
		}
	}

	ahci_dev->pwr_rst = devm_reset_control_get(&pdev->dev, "pwr-rst");
	if (IS_ERR(ahci_dev->pwr_rst)) {
		ahci_dev->pwr_rst = NULL;
		dev_dbg(&pdev->dev, "power soft reset control not defined\n");
	} else {
		err = reset_control_deassert(ahci_dev->pwr_rst);
		if (err) {
			dev_err(&pdev->dev, "unable to bring out of reset\n");
			return err;
		}
	}

	return 0;
}

static int ahci_st_init(struct device *ahci_dev, void __iomem *mmio)
{
	struct platform_device *pdev = to_platform_device(ahci_dev->parent);
	struct ahci_st_drv_data *drv_data = platform_get_drvdata(pdev);
	int ret;

	ret = clk_prepare_enable(drv_data->clk);
	if (ret)
		return ret;

	ret = phy_init(drv_data->phy);
	if (ret)
		goto fail_clk_disable;

	drv_data->mmio = mmio;
	ahci_st_configure_oob(mmio);

	return 0;

fail_clk_disable:
	clk_disable_unprepare(drv_data->clk);

	return ret;
}

static void ahci_st_exit(struct device *ahci_dev)
{
	struct platform_device *pdev = to_platform_device(ahci_dev->parent);
	struct ahci_st_drv_data *drv_data = platform_get_drvdata(pdev);
	int ret;

	if (drv_data->pwr) {
		ret = reset_control_assert(drv_data->pwr);
		if (ret)
			dev_err(&pdev->dev, "unable to bring out of pwrdwn\n");
	}
	clk_disable_unprepare(drv_data->clk);
}

#ifdef CONFIG_PM_SLEEP
static int ahci_st_suspend(struct device *ahci_dev)
{
	struct platform_device *pdev = to_platform_device(ahci_dev->parent);
	struct ahci_st_drv_data *drv_data = platform_get_drvdata(pdev);
	int ret;

	phy_exit(drv_data->phy);

	if (drv_data->pwr) {
		ret = reset_control_assert(drv_data->pwr);
		if (ret) {
			dev_err(&pdev->dev, "unable to bring out of pwrdwn");
			return ret;
		}
	}
	clk_disable_unprepare(drv_data->clk);
	return 0;
}

static int ahci_st_resume(struct device *ahci_dev)
{
	struct platform_device *pdev = to_platform_device(ahci_dev->parent);
	struct ahci_st_drv_data *drv_data = platform_get_drvdata(pdev);
	int ret;

	clk_prepare_enable(drv_data->clk);

	if (drv_data->pwr) {
		ret = reset_control_deassert(drv_data->pwr);
		if (ret) {
			dev_err(&pdev->dev, "unable to bring out of pwrdwn\n");
			return ret;
		}
	}

	if (drv_data->sw_rst) {
		ret = reset_control_deassert(drv_data->sw_rst);
		if (ret) {
			dev_err(&pdev->dev, "unable to bring out of sw-rst\n");
			return ret;
		}
	}
	if (drv_data->pwr_rst) {
		ret = reset_control_deassert(drv_data->pwr_rst);
		if (ret) {
			dev_err(&pdev->dev, "unable to bring out of pwr-rst\n");
			return ret;
		}
	}

	ret = phy_init(drv_data->phy);
	if (ret)
		return ret;

	ahci_st_configure_oob(drv_data->mmio);

	return 0;
}
#endif

static struct ahci_platform_data ahci_st_platform_data = {
	.init = ahci_st_init,
	.exit = ahci_st_exit,
#ifdef CONFIG_PM_SLEEP
	.suspend = ahci_st_suspend,
	.resume = ahci_st_resume,
#endif
};

static int ahci_st_driver_probe(struct platform_device *pdev)
{
	int err;
	struct ahci_st_drv_data *drv_data;
	struct device *dev = &pdev->dev;
	struct resource ahci_resource[2];
	struct resource *res;
	struct platform_device_info ahci_info = {
		.parent = dev,
		.name = "ahci",
		.id = pdev->id,
		.res = ahci_resource,
		.num_res = 2,
		.data = &ahci_st_platform_data,
		.size_data = sizeof(ahci_st_platform_data),
	};

	if (dev->of_node) {
		ahci_info.id = of_alias_get_id(dev->of_node, "sata");
		ahci_info.dma_mask = DMA_BIT_MASK(32);
	}
	if (ahci_info.id < 0)
		ahci_info.id =  pdev->id;


	drv_data = devm_kzalloc(dev, sizeof(*drv_data), GFP_KERNEL);
	if (!drv_data)
		return -ENOMEM;

	platform_set_drvdata(pdev, drv_data);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res)
		return -EINVAL;
	ahci_resource[0] = *res;

	res = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	if (!res)
		return -EINVAL;
	ahci_resource[1] = *res;

	err = ahci_st_probe_resets(pdev, drv_data);
	if (err)
		return err;

	drv_data->clk = devm_clk_get(&pdev->dev, "ahci_clk");
	if (IS_ERR(drv_data->clk)) {
		dev_err(&pdev->dev, "ahci_clk clk not found\n");
		return PTR_ERR(drv_data->clk);
	}

	drv_data->phy = devm_phy_get(&pdev->dev, "ahci_phy");
	if (IS_ERR(drv_data->phy)) {
		dev_err(&pdev->dev, "no PHY configured\n");
		return PTR_ERR(drv_data->phy);
	}

	platform_set_drvdata(pdev, drv_data);

	drv_data->ahci = platform_device_register_full(&ahci_info);
	if (IS_ERR(drv_data->ahci))
		return PTR_ERR(drv_data->ahci);

	return 0;
}

static struct of_device_id st_ahci_match[] = {
	{
		.compatible = "st,ahci",
	},
	{},
};

MODULE_DEVICE_TABLE(of, st_ahci_match);

static struct platform_driver ahci_st_driver = {
	.driver.name = "ahci_st",
	.driver.owner = THIS_MODULE,
	.driver.of_match_table = of_match_ptr(st_ahci_match),
	.probe = ahci_st_driver_probe,
};
module_platform_driver(ahci_st_driver);

MODULE_AUTHOR("Alexandre Torgue <alexandre.torgue@st.com>");
MODULE_AUTHOR("Francesco Virlinzi <francesco.virlinzi@st.com>");
MODULE_DESCRIPTION("STMicroelectronics Sata Ahci driver");
MODULE_LICENSE("GPL v2");
