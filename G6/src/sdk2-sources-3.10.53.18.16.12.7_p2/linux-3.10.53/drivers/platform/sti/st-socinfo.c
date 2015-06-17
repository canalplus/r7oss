/*
 * Copyright (C) 2014 STMicroelectronics (R&D) Limited
 *
 * Author: Seraphin Bonnaffe <seraphin.bonnaffe@st.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 */

#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/mfd/syscon.h>
#include <linux/regmap.h>
#include <linux/sys_soc.h>
#include <linux/of_fdt.h>
#include <linux/slab.h>
#include <linux/st-socinfo.h>

#define STIH416_SYSCFG_9516	0x810
#define STIH407_SYSCFG_5568	0x8e0
#define STID127_SYSCFG_772	0x120
#define SOCID_SHIFT		28
#define SOCID_MASK		0xf

long st_socinfo_version_major = -1;
EXPORT_SYMBOL(st_socinfo_version_major);

long st_socinfo_version_minor = -1;
EXPORT_SYMBOL(st_socinfo_version_minor);

struct st_socinfo_dev {
	struct regmap *regmap;
	unsigned int syscfg;
};

static const struct of_device_id st_socinfo_of_match[] = {
	{
		.compatible = "st,stih416-socinfo",
		.data = (void *) STIH416_SYSCFG_9516,
	},
	{
		.compatible = "st,stih407-socinfo",
		.data = (void *) STIH407_SYSCFG_5568,
	},
	{
		.compatible = "st,stid127-socinfo",
		.data = (void *) STID127_SYSCFG_772,
	},
	{}
};

static int st_socinfo_probe(struct platform_device *pdev)
{
	struct soc_device *soc_dev;
	struct soc_device_attribute *soc_dev_attr;
	const struct of_device_id *match;
	struct st_socinfo_dev *st_socinfo;
	struct device *dev = &pdev->dev;
	struct device *parent;
	struct device_node *np = pdev->dev.of_node;
	struct device_node *npp = np->parent;
	int socid;
	char *family;
	const char *model;
	char *pch;
	int family_length;
	int ret;

	/* Identify SoC family from "model" DT entry  */
	model = of_get_property(npp->parent, "model", NULL);
	if (!model)
		return -EINVAL;

	pch = strchr(model, ' ');
	if (!pch)
		family_length = strlen(model);
	else
		family_length = pch - model;

	family = devm_kzalloc(dev, (family_length + 1) * sizeof(char),
			      GFP_KERNEL);
	if (!family)
		return -ENOMEM;

	strncat(family, model, family_length);

	/* Get SoC ID and revision from syscfg register */
	st_socinfo = devm_kzalloc(dev, sizeof(struct st_socinfo_dev),
				  GFP_KERNEL);
	if (!st_socinfo)
		return -ENOMEM;

	match = of_match_device(st_socinfo_of_match, dev);
	if (!match)
		return -ENODEV;

	st_socinfo->syscfg = (unsigned int)match->data;
	st_socinfo->regmap = syscon_regmap_lookup_by_phandle(np, "st,syscfg");
	if (IS_ERR(st_socinfo->regmap)) {
		dev_err(dev, "No syscfg phandle specified\n");
		return PTR_ERR(st_socinfo->regmap);
	}

	ret = regmap_read(st_socinfo->regmap,
			  st_socinfo->syscfg, &socid);

	if (ret)
		return ret;

	/* Populate and register SoC device attribute */
	soc_dev_attr = devm_kzalloc(dev, sizeof(*soc_dev_attr), GFP_KERNEL);
	if (!soc_dev_attr)
		return -ENOMEM;

	soc_dev_attr->machine = "STi";
	soc_dev_attr->family = kasprintf(GFP_KERNEL, "%s", family);
	st_socinfo_version_major = (socid >> SOCID_SHIFT) & SOCID_MASK;
	soc_dev_attr->revision = kasprintf(GFP_KERNEL, "0x%lx",
					   st_socinfo_version_major);
	soc_dev_attr->soc_id = kasprintf(GFP_KERNEL, "0x%lx",
					 socid & BITS_MASK(0, SOCID_SHIFT));

	soc_dev = soc_device_register(soc_dev_attr);
	if (IS_ERR(soc_dev))
		return -ENOMEM;

	parent = soc_device_to_device(soc_dev);

	ret = of_platform_populate(np, st_socinfo_of_match, NULL, parent);

	if (ret)
		dev_err(dev, "Failed to add st-socinfo device\n");

	return ret;
}

static struct platform_driver st_socinfo_driver = {
	.driver = {
		.name = "st-socinfo",
		.owner = THIS_MODULE,
		.of_match_table = st_socinfo_of_match,
	},
	.probe = st_socinfo_probe,
};


static int __init st_socinfo_init(void)
{
	return platform_driver_register(&st_socinfo_driver);
}
late_initcall(st_socinfo_init);
