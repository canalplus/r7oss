/*
 * Copyright (C) 2013 STMicroelectronics
 *
 * PMU syscfg driver, used in STMicroelectronics devices.
 *
 * Author: Christophe Kerello <christophe.kerello@st.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2, as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/mfd/syscon.h>
#include <linux/regmap.h>

/* bit2: pmuirq(0) enable */
/* bit3: pmuirq(1) enable */
/* bit[16..14]: pmuirq(0) selection */
/* bit[19..17]: pmuirq(1) selection */
#define ST_PMU_SYSCFG_MASK	0xfc0ff
#define ST_PMU_SYSCFG_VAL	0xd400c

/* STiH415 */
#define STIH415_SYSCFG_642	0xa8

/* STiH416 */
#define STIH416_SYSCFG_7543	0x87c

/* STiH407 */
#define STIH407_SYSCFG_5102	0x198

/* STiD127 */
#define STID127_SYSCFG_734	0x88

struct st_pmu_syscfg_dev {
	struct regmap *regmap;
	unsigned int syscfg;
};

static int st_pmu_syscfg_enable_irq(struct device *dev)
{
	struct platform_device *pdev =
		container_of(dev, struct platform_device, dev);
	struct st_pmu_syscfg_dev *st_pmu_syscfg = platform_get_drvdata(pdev);

	regmap_update_bits(st_pmu_syscfg->regmap,
				st_pmu_syscfg->syscfg,
				ST_PMU_SYSCFG_MASK,
				ST_PMU_SYSCFG_VAL);

	return 0;
}

static struct of_device_id st_pmu_syscfg_of_match[] = {
	{
		.compatible = "st,stih415-pmu-syscfg",
		.data = (void *) STIH415_SYSCFG_642,
	},
	{
		.compatible = "st,stih416-pmu-syscfg",
		.data = (void *) STIH416_SYSCFG_7543,
	},
	{
		.compatible = "st,stih407-pmu-syscfg",
		.data = (void *) STIH407_SYSCFG_5102,
	},
	{
		.compatible = "st,stid127-pmu-syscfg",
		.data = (void *) STID127_SYSCFG_734,
	},
	{}
};

static int st_pmu_syscfg_probe(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	const struct of_device_id *match;
	struct device *dev = &pdev->dev;
	struct st_pmu_syscfg_dev *st_pmu_syscfg;
	int ret;

	if (!np) {
		dev_err(dev, "Device tree node not found.\n");
		return -ENODEV;
	}

	st_pmu_syscfg = devm_kzalloc(dev, sizeof(struct st_pmu_syscfg_dev),
					GFP_KERNEL);
	if (!st_pmu_syscfg)
		return -ENOMEM;

	match = of_match_device(st_pmu_syscfg_of_match, dev);
	if (!match)
		return -ENODEV;

	st_pmu_syscfg->syscfg = (unsigned int)match->data;

	st_pmu_syscfg->regmap = syscon_regmap_lookup_by_phandle(np,
					"st,syscfg");
	if (IS_ERR(st_pmu_syscfg->regmap)) {
		dev_err(dev, "No syscfg phandle specified\n");
		return PTR_ERR(st_pmu_syscfg->regmap);
	}

	ret = of_platform_populate(np, NULL, NULL, dev);
	if (ret) {
		dev_err(dev, "Failed to add pmu device\n");
		return ret;
	}

	platform_set_drvdata(pdev, st_pmu_syscfg);

	return st_pmu_syscfg_enable_irq(dev);
}

#ifdef CONFIG_PM_SLEEP
static int st_pmu_syscfg_resume(struct device *dev)
{
	/* restore syscfg register */
	return st_pmu_syscfg_enable_irq(dev);
}

static SIMPLE_DEV_PM_OPS(st_pmu_syscfg_pm_ops, NULL, st_pmu_syscfg_resume);
#endif /* CONFIG_PM_SLEEP */

static struct platform_driver st_pmu_syscfg_driver = {
	.probe = st_pmu_syscfg_probe,
	.driver = {
		.name = "st_pmu_config",
#ifdef CONFIG_PM_SLEEP
		.pm = &st_pmu_syscfg_pm_ops,
#endif /* CONFIG_PM_SLEEP */
		.of_match_table = st_pmu_syscfg_of_match,
	},
};

static int __init st_pmu_syscfg_init(void)
{
	return platform_driver_register(&st_pmu_syscfg_driver);
}

device_initcall(st_pmu_syscfg_init);

MODULE_AUTHOR("Christophe Kerello <christophe.kerello@st.com>");
MODULE_DESCRIPTION("STMicroelectronics PMU syscfg driver");
MODULE_LICENSE("GPL v2");
