/*
 * ST regulator driver for flashSS vsense
 *
 * This is a small driver to manage the voltage regulator inside the ST flash
 * sub-system that is used for configuring MMC, NAND, SPI voltages.
 *
 * Copyright(C) 2014 STMicroelectronics Ltd
 * Author: Giuseppe Cavallaro <peppe.cavallaro@st.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under  the terms of the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the License, or (at your
 * option) any later version.
 */
#include <linux/module.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>

/* FlashSS voltage VSENSE TOP CONFIG register defines */
#define TOP_VSENSE_CONFIG_REG_PSW_EMMC		BIT(0)
#define TOP_VSENSE_CONFIG_ENB_REG_PSW_EMMC	BIT(1)
#define TOP_VSENSE_CONFIG_REG_PSW_NAND		BIT(8)
#define TOP_VSENSE_CONFIG_ENB_REG_PSW_NAND	BIT(9)
#define TOP_VSENSE_CONFIG_REG_PSW_SPI		BIT(16)
#define TOP_VSENSE_CONFIG_ENB_REG_PSW_SPI	BIT(17)
#define TOP_VSENSE_CONFIG_LATCHED_PSW_EMMC	BIT(24)
#define TOP_VSENSE_CONFIG_LATCHED_PSW_NAND	BIT(25)
#define TOP_VSENSE_CONFIG_LATCHED_PSW_SPI	BIT(26)

struct st_vsense {
	char *name;
	struct device *dev;
	u8 n_voltages;			/* number of supported voltages */
	void __iomem *ioaddr;		/* TOP config base address */
	unsigned int en_psw_mask;	/* Mask/enable vdd for each device */
	unsigned int psw_mask;		/* Power sel mask for VDD */
	unsigned int latched_mask;	/* Latched mask for VDD */
};

static const unsigned int st_type_voltages[] = {
	1800000,
	3300000,
};

const struct st_vsense st_vsense_data[] = {
	{
		.en_psw_mask = TOP_VSENSE_CONFIG_ENB_REG_PSW_EMMC,
		.psw_mask = TOP_VSENSE_CONFIG_REG_PSW_EMMC,
		.latched_mask = TOP_VSENSE_CONFIG_LATCHED_PSW_EMMC,
	}, {
		.en_psw_mask = TOP_VSENSE_CONFIG_ENB_REG_PSW_NAND,
		.psw_mask = TOP_VSENSE_CONFIG_REG_PSW_NAND,
		.latched_mask = TOP_VSENSE_CONFIG_LATCHED_PSW_NAND,
	}, {
		.en_psw_mask = TOP_VSENSE_CONFIG_ENB_REG_PSW_SPI,
		.psw_mask = TOP_VSENSE_CONFIG_REG_PSW_SPI,
		.latched_mask = TOP_VSENSE_CONFIG_LATCHED_PSW_SPI,
	},
};

/* Get the value of Sensed-PSW of eMMC/NAND/SPI Pads */
static int st_get_voltage_sel(struct regulator_dev *dev)
{
	struct st_vsense *vsense = rdev_get_drvdata(dev);
	void __iomem *ioaddr = vsense->ioaddr;
	int sel = 0;
	u32 value = readl_relaxed(ioaddr);

	if (value & vsense->latched_mask)
		sel = 1;

	dev_dbg(vsense->dev, "%s, selection %d (0x%08x)\n", vsense->name, sel,
		readl_relaxed(ioaddr));

	return sel;
}

static int st_set_voltage_sel(struct regulator_dev *dev, unsigned int selector)
{
	struct st_vsense *vsense = rdev_get_drvdata(dev);
	void __iomem *ioaddr = vsense->ioaddr;
	unsigned value = readl_relaxed(ioaddr);
	unsigned int voltage;

	voltage = st_type_voltages[selector];

	value |= vsense->en_psw_mask;
	if (voltage == 3300000)
		value |= vsense->psw_mask;
	else
		value &= ~vsense->psw_mask;

	writel_relaxed(value, ioaddr);

	dev_dbg(vsense->dev, "%s, required voltage %d (vsense_conf 0x%08x)\n",
		vsense->name, voltage,
		readl_relaxed(ioaddr));

	return 0;
}

static struct regulator_ops st_ops = {
	.get_voltage_sel = st_get_voltage_sel,
	.set_voltage_sel = st_set_voltage_sel,
	.list_voltage = regulator_list_voltage_table,
};

static struct of_device_id of_st_match_tbl[] = {
	{.compatible = "st,vmmc", .data = &st_vsense_data[0]},
	{.compatible = "st,vnand", .data = &st_vsense_data[1]},
	{.compatible = "st,vspi", .data = &st_vsense_data[2]},
	{ /* end */ }
};

static void st_get_satinize_powerup_voltage(struct st_vsense *vsense)
{
	void __iomem *ioaddr = vsense->ioaddr;
	u32 value = readl_relaxed(ioaddr);

	dev_dbg(vsense->dev, "Initial start-up value: (0x%08x)\n", value);

	/* Sanitize voltage values forcing what is provided from start-up */
	if (value & TOP_VSENSE_CONFIG_LATCHED_PSW_EMMC)
		value |= TOP_VSENSE_CONFIG_REG_PSW_EMMC;
	else
		value &= ~TOP_VSENSE_CONFIG_REG_PSW_EMMC;

	if (value & TOP_VSENSE_CONFIG_LATCHED_PSW_NAND)
		value |= TOP_VSENSE_CONFIG_REG_PSW_NAND;
	else
		value &= ~TOP_VSENSE_CONFIG_REG_PSW_NAND;

	if (value & TOP_VSENSE_CONFIG_LATCHED_PSW_SPI)
		value |= TOP_VSENSE_CONFIG_REG_PSW_SPI;
	else
		value &= ~TOP_VSENSE_CONFIG_REG_PSW_SPI;

	writel_relaxed(value, ioaddr);

	dev_dbg(vsense->dev, "Sanitized value: (0x%08x)\n", value);
}

static int st_vsense_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *np = dev->of_node;
	struct st_vsense *vsense;
	const struct of_device_id *device;
	struct resource *res;
	struct regulator_desc *rdesc;
	struct regulator_dev *rdev;
	struct regulator_config config = { };

	vsense = devm_kzalloc(dev, sizeof(*vsense), GFP_KERNEL);
	if (!vsense)
		return -ENOMEM;

	vsense->dev = dev;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	vsense->ioaddr = devm_ioremap_resource(dev, res);
	if (IS_ERR(vsense->ioaddr))
		return PTR_ERR(vsense->ioaddr);

	rdesc = devm_kzalloc(dev, sizeof(*rdesc), GFP_KERNEL);
	if (!rdesc)
		return -ENOMEM;

	device = of_match_device(of_st_match_tbl, &pdev->dev);
	if (!device)
		return -ENODEV;

	if (device->data) {
		const struct st_vsense *data = device->data;
		vsense->en_psw_mask = data->en_psw_mask;
		vsense->psw_mask = data->psw_mask;
		vsense->latched_mask = data->latched_mask;
	} else
		return -ENODEV;

	if (of_property_read_string(np, "regulator-name",
				    (const char **)&vsense->name))
		return -EINVAL;

	memset(rdesc, 0, sizeof(*rdesc));
	rdesc->name = vsense->name;
	rdesc->ops = &st_ops;
	rdesc->type = REGULATOR_VOLTAGE;
	rdesc->owner = THIS_MODULE;
	rdesc->n_voltages = ARRAY_SIZE(st_type_voltages);
	rdesc->volt_table = st_type_voltages;
	config.dev = &pdev->dev;
	config.driver_data = vsense;
	config.of_node = np;

	/* register regulator */
	rdev = regulator_register(rdesc, &config);
	if (IS_ERR(rdev)) {
		dev_err(dev, "failed to register %s\n", rdesc->name);
		return PTR_ERR(rdev);
	}

	platform_set_drvdata(pdev, rdev);

	dev_info(dev, "%s  vsense voltage regulator registered\n", rdesc->name);
	st_get_satinize_powerup_voltage(vsense);

	return 0;
}

static int st_vsense_remove(struct platform_device *pdev)
{
	struct regulator_dev *rdev = platform_get_drvdata(pdev);

	regulator_unregister(rdev);

	return 0;
}

static struct platform_driver st_vsense_driver = {
	.driver = {
		   .name = "st-vsense",
		   .owner = THIS_MODULE,
		   .of_match_table = of_st_match_tbl,
		   },
	.probe = st_vsense_probe,
	.remove = st_vsense_remove,
};

static int __init st_vsense_init(void)
{
	return platform_driver_register(&st_vsense_driver);
}

subsys_initcall(st_vsense_init);

static void __exit st_vsense_cleanup(void)
{
	platform_driver_unregister(&st_vsense_driver);
}

module_exit(st_vsense_cleanup);

MODULE_AUTHOR("Giuseppe Cavallaro <peppe.cavallaro@st.com>");
MODULE_DESCRIPTION("ST voltage regulator driver for vsense flashSS");
MODULE_LICENSE("GPL v2");
