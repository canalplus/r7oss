/*
 * DDR Self-Refresh Power Down (SRPD) support for Broadcom STB SoCs
 *
 * Copyright © 2014 Broadcom Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/of_device.h>
#include <linux/io.h>

#define REG_SRPD_CONFIG			0x3c
 #define INACT_COUNT_SHIFT		0
 #define INACT_COUNT_MASK		0xffff
 #define SRPD_EN_SHIFT			16
 #define SRPD_EN_MASK			0x10000

#define REG_POWER_DOWN_STATUS		0x44
 #define SRPD_STATUS_SHIFT		1
 #define SRPD_STATUS_MASK		0x2

struct brcmstb_memc {
	struct device *dev;
	void __iomem *ddr_ctrl;
	unsigned int timeout_cycles;
	u32 frequency;
};

static int brcmstb_memc_srpd_config(struct brcmstb_memc *memc,
				    unsigned int cycles)
{
	void __iomem *cfg = memc->ddr_ctrl + REG_SRPD_CONFIG;
	u32 val;

	/* Max timeout supported in HW */
	if (cycles > INACT_COUNT_MASK)
		return -EINVAL;

	memc->timeout_cycles = cycles;

	val = (cycles << INACT_COUNT_SHIFT) & INACT_COUNT_MASK;
	if (cycles)
		val |= (1 << SRPD_EN_SHIFT);

	__raw_writel(val, cfg);
	(void)__raw_readl(cfg);

	return 0;
}

static ssize_t show_attr_freq(struct device *dev,
			      struct device_attribute *attr, char *buf)
{
	struct brcmstb_memc *memc = dev_get_drvdata(dev);

	return sprintf(buf, "%d\n", memc->frequency);
}

static ssize_t show_attr_srpd(struct device *dev,
			      struct device_attribute *attr, char *buf)
{
	struct brcmstb_memc *memc = dev_get_drvdata(dev);

	return sprintf(buf, "%d\n", memc->timeout_cycles);
}

static ssize_t store_attr_srpd(struct device *dev,
			       struct device_attribute *attr,
			       const char *buf, size_t count)
{
	struct brcmstb_memc *memc = dev_get_drvdata(dev);
	unsigned int val;
	int ret;

	ret = kstrtouint(buf, 10, &val);
	if (ret < 0)
		return ret;

	ret = brcmstb_memc_srpd_config(memc, val);
	if (ret)
		return ret;

	return count;
}

static DEVICE_ATTR(frequency, S_IRUGO, show_attr_freq, NULL);
static DEVICE_ATTR(srpd, S_IRUSR | S_IWUSR, show_attr_srpd, store_attr_srpd);

static struct attribute *dev_attrs[] = {
	&dev_attr_frequency.attr,
	&dev_attr_srpd.attr,
	NULL,
};

static struct attribute_group dev_attr_group = {
	.attrs = dev_attrs,
};

static int brcmstb_memc_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct brcmstb_memc *memc;
	struct resource *res;
	int ret;

	memc = devm_kzalloc(dev, sizeof(*memc), GFP_KERNEL);
	if (!memc)
		return -ENOMEM;

	dev_set_drvdata(dev, memc);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	memc->ddr_ctrl = devm_ioremap_resource(dev, res);
	if (IS_ERR(memc->ddr_ctrl))
		return PTR_ERR(memc->ddr_ctrl);

	of_property_read_u32(pdev->dev.of_node, "clock-frequency",
			     &memc->frequency);

	ret = sysfs_create_group(&dev->kobj, &dev_attr_group);
	if (ret) {
		dev_err(dev, "failed to create attribute group (%d)\n", ret);
		return ret;
	}

	dev_info(dev, "registered\n");

	return 0;
}

static int brcmstb_memc_remove(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;

	sysfs_remove_group(&dev->kobj, &dev_attr_group);

	return 0;
}

static const struct of_device_id brcmstb_memc_of_match[] = {
	{ .compatible = "brcm,brcmstb-memc-ddr" },
	{},
};

static struct platform_driver brcmstb_memc_driver = {
	.probe = brcmstb_memc_probe,
	.remove = brcmstb_memc_remove,
	.driver = {
		.name		= "brcmstb_memc",
		.owner		= THIS_MODULE,
		.of_match_table	= of_match_ptr(brcmstb_memc_of_match),
	},
};
module_platform_driver(brcmstb_memc_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Broadcom Corporation");
MODULE_DESCRIPTION("DDR SRPD driver for Broadcom STB chips");
