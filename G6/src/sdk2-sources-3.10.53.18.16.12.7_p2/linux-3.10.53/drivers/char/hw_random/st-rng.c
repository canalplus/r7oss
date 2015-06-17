/*
* ST Random Number Generator Driver for STix series of SoCs
* Author: Pankaj Dev (pankaj.dev@st.com)
*
* Copyright (C) 2007-2013 STMicroelectronics (R&D) Limited
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
*
*/

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/hw_random.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/slab.h>

/* Registers */
#define ST_RNG_STATUS_REG 0x20
#define ST_RNG_DATA_REG   0x24

/* Registers fields */
#define ST_RNG_STATUS_BAD_SEQUENCE   BIT(0)
#define ST_RNG_STATUS_BAD_ALTERNANCE BIT(1)
#define ST_RNG_STATUS_FIFO_FULL		 BIT(5)

#define ST_RNG_FIFO_SIZE 8

#define ST_RNG_NAME "st-rng:%s"

struct st_rng_data {
	struct hwrng st_rng_ops;
	struct clk *clk_input;
	u16 last;
};

static u32 st_rng_read_reg(struct hwrng *rng, int reg)
{
	return readl_relaxed(((void __iomem *)rng->priv) + reg);
}

static int st_rng_read(struct hwrng *rng, void *data, size_t max, bool wait)
{
	int i;
	u32 status;

	if (max < sizeof(u16))
		return -EINVAL;

	status = st_rng_read_reg(rng, ST_RNG_STATUS_REG);
	if (!wait && ((status & (ST_RNG_STATUS_BAD_ALTERNANCE |
				 ST_RNG_STATUS_BAD_SEQUENCE)) ||
		      !(status & (ST_RNG_STATUS_FIFO_FULL))))
		return 0;

	/*
	 * Wait till FIFO is Full
	 * We put a timeout = 0.667us*(ST_RNG_FIFO_SIZE/sizeof(u16)).
	 */

	if (!(st_rng_read_reg(rng, ST_RNG_STATUS_REG) &
			 (ST_RNG_STATUS_FIFO_FULL))) {
		udelay(1 * (ST_RNG_FIFO_SIZE / sizeof(u16)));
		if (!(st_rng_read_reg(rng, ST_RNG_STATUS_REG) &
				 (ST_RNG_STATUS_FIFO_FULL)))
			return 0;
	}

	for (i = 0; i < ST_RNG_FIFO_SIZE && i < max; i = i + 2)
		*(u16 *)(data+i) = st_rng_read_reg(rng, ST_RNG_DATA_REG);

	return i-2;	/* No of bytes read */
}

static int st_rng_probe(struct platform_device *pdev)
{
	struct resource *res;
	struct st_rng_data *rng_drvdata;
	struct hwrng *st_rng_ops;
	int ret;
	void __iomem *rng_base;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);

	rng_drvdata =
	    devm_kzalloc(&pdev->dev, sizeof(*rng_drvdata), GFP_KERNEL);
	if (!rng_drvdata)
		return -ENOMEM;

	st_rng_ops = &(rng_drvdata->st_rng_ops);

	rng_base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(rng_base))
		return PTR_ERR(rng_base);

	st_rng_ops->name = pdev->name;
	st_rng_ops->priv = (unsigned long)rng_base;
	st_rng_ops->read = st_rng_read;

	rng_drvdata->clk_input = devm_clk_get(&pdev->dev, NULL);
	if (WARN_ON(IS_ERR(rng_drvdata->clk_input)))
		return -EINVAL;
	/* ensure that clk rate is correct by enabling the clk */
	clk_prepare_enable(rng_drvdata->clk_input);

	ret = hwrng_register(st_rng_ops);
	if (ret) {
		dev_err(&pdev->dev,
			"ST Random Number Generator Device Probe Failed\n");
		return ret;
	}

	dev_set_drvdata(&pdev->dev, st_rng_ops);

	dev_info(&pdev->dev,
		 "ST Random Number Generator Device Probe Successful\n");

	return 0;
}

static int __exit st_rng_remove(struct platform_device *pdev)
{
	struct st_rng_data *rng_drvdata = dev_get_drvdata(&pdev->dev);
	struct hwrng *st_rng_ops = &(rng_drvdata->st_rng_ops);

	hwrng_unregister(st_rng_ops);

	clk_disable_unprepare(rng_drvdata->clk_input);

	return 0;
}

static struct of_device_id st_rng_match[] = {
	{.compatible = "st,rng",},
	{},
};

MODULE_DEVICE_TABLE(of, st_rng_match);

static struct platform_driver st_rng_driver = {
	.driver = {
		   .name = "st-hwrandom",
		   .owner = THIS_MODULE,
		   .of_match_table = of_match_ptr(st_rng_match),
		   },
	.probe = st_rng_probe,
	.remove = __exit_p(st_rng_remove),
};

module_platform_driver(st_rng_driver);

MODULE_AUTHOR("ST Microelectronics <pankaj.dev@st.com>");
MODULE_LICENSE("GPLv2");
