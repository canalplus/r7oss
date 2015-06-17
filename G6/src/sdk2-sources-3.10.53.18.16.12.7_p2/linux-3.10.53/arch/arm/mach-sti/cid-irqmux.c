/*
 * Copyright (C) 2014 STMicroelectronics Limited
 *
 * Author: Francesco Virlinzi <francesco.virlinzist.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/init.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/io.h>

#define IRQMUX_OUTPUT_MASK	BITS_MASK(0, 29)
#define IRQMUX_INPUT_ENABLE	BIT(31)
#define IRQMUX_INPUT_INVERT	BIT(30)

struct st_cid_irqmux_data {
	const char *name;
	u32 num_input;
	u32 num_output;
	int (*custom_mapping)(struct device *dev, long input, long *enable,
			      long *output, long *inv);
};

struct st_cid_irqmux_info {
	struct list_head list;
	void __iomem *base;
	struct device *dev;
};

static int apply_irqmux_mapping(struct device *dev)
{
	struct st_cid_irqmux_info *info = dev_get_drvdata(dev);
	struct st_cid_irqmux_data *pdata = dev_get_platdata(dev);
	int ret;

	if (pdata->custom_mapping) {
		long input, output, invert, enable;
		for (input = 0; input < pdata->num_input; ++input) {
			long mapping_value;
			output = invert = enable = 0;
			ret = pdata->custom_mapping(dev,
					input, &enable, &output, &invert);
			if (ret) {
				dev_err(dev, "Mapping failed for input %ld\n",
					input);
				return ret;
			}
			mapping_value = (output & IRQMUX_OUTPUT_MASK);
			mapping_value |= (enable ? IRQMUX_INPUT_ENABLE : 0);
			mapping_value |= (invert ? IRQMUX_INPUT_INVERT : 0);
			writel_relaxed(mapping_value, info->base + input * 4);
		}
	}

	return 0;
}

static int of_generic_irqmux_mapping(struct device *dev, long input,
				     long *enable, long *output, long *inv)
{
	struct st_cid_irqmux_data *pdata = dev_get_platdata(dev);
	struct device_node *np;

	np = dev->of_node;
	if (of_property_read_bool(np, "interrupts-enable"))
		*enable = 1;
	else
		*enable = 0;

	if (of_property_read_bool(np, "interrupts-invert"))
		*inv = 1;
	else
		*inv = 0;

	*output = input % pdata->num_output;

	return 0;
}

static int of_custom_irqmux_mapping(struct device *dev, long input,
				    long *enable, long *output, long *inv)
{
	struct device_node *np;
	union {
		u32 array[5];
		struct {
			u32 begin;
			u32 size;
			u32 output;
			u32 enabled;
			u32 inverted;
		};
	} irqmux_map_entry;
	const __be32 *list;
	const struct property *pp;
	int remaining;

	np = dev->of_node;
	pp = of_find_property(np, "irqmux-map", NULL);
	BUG_ON(!pp);

	list = pp->value;
	remaining = pp->length / sizeof(*list);
	while (remaining >= ARRAY_SIZE(irqmux_map_entry.array)) {
		int i;
		for (i = 0; i < ARRAY_SIZE(irqmux_map_entry.array); ++i)
			irqmux_map_entry.array[i] = be32_to_cpup(list++);

		if (input >= irqmux_map_entry.begin &&
		    input < (irqmux_map_entry.begin + irqmux_map_entry.size))
			goto found;

		remaining -= ARRAY_SIZE(irqmux_map_entry.array);
	}

	return -EINVAL;

found:
	*enable = irqmux_map_entry.enabled;
	*inv = irqmux_map_entry.inverted;
	*output = irqmux_map_entry.output;

	return 0;
}

static void *st_cid_irqmux_probe_dt(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	struct st_cid_irqmux_data *pdata;

	if (!np)
		return ERR_PTR(-EINVAL);

	pdata = devm_kzalloc(&pdev->dev, sizeof(*pdata), GFP_KERNEL);
	if (!pdata)
		return ERR_PTR(-ENOMEM);

	of_property_read_string(np, "irqmux-name", &pdata->name);
	of_property_read_u32(np, "irqmux-inputs", &pdata->num_input);
	of_property_read_u32(np, "irqmux-outputs", &pdata->num_output);

	if (of_find_property(np, "irqmux-map", NULL))
		pdata->custom_mapping = of_custom_irqmux_mapping;
	else
		pdata->custom_mapping = of_generic_irqmux_mapping;

	return pdev->dev.platform_data = pdata;
}

static int st_cid_irqmux_probe(struct platform_device *pdev)
{
	struct st_cid_irqmux_data *pdata;
	struct st_cid_irqmux_info *info;
	struct resource *res;
	int ret;

	pdata = st_cid_irqmux_probe_dt(pdev);
	if (IS_ERR(pdata))
		return PTR_ERR(pdata);

	info = devm_kzalloc(&pdev->dev, sizeof(*info), GFP_KERNEL);
	if (!info)
		return -ENOMEM;

	info->dev = &pdev->dev;
	platform_set_drvdata(pdev, info);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	info->base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(info->base))
		return PTR_ERR(info->base);

	ret = apply_irqmux_mapping(&pdev->dev);
	if (ret)
		return ret;

	dev_info(&pdev->dev, "%s configured\n", pdata->name);

	return 0;
}

static struct of_device_id st_cid_irqmux_match[] = {
	{ .compatible = "st,cid-irqmux",},
	{},
};

#ifdef CONFIG_PM_SLEEP
static int st_cid_irqmux_resume(struct device *dev)
{
	return apply_irqmux_mapping(dev);
}

static SIMPLE_DEV_PM_OPS(st_cid_irqmux_pm, NULL, st_cid_irqmux_resume);
#define ST_CID_IRQMUX	(&st_cid_irqmux_pm)
#else
#define ST_CID_IRQMUX	NULL
#endif

static struct platform_driver st_cid_irqmux_driver = {
	.driver = {
		.name = "st-cid-irqmux",
		.owner = THIS_MODULE,
		.of_match_table = st_cid_irqmux_match,
		.pm = ST_CID_IRQMUX,
	},
	.probe = st_cid_irqmux_probe,
};

static int __init st_cid_irqmux_init(void)
{
	return platform_driver_register(&st_cid_irqmux_driver);
}
core_initcall(st_cid_irqmux_init);
