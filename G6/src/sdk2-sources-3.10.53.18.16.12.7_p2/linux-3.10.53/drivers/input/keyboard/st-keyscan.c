/*
 * STMicroelectronics Key Scanning driver
 *
 * Copyright (c) 2012 STMicroelectonics Ltd.
 * Author: Stuart Menefy <stuart.menefy@st.com>
 *
 * Based on sh_keysc.c, copyright 2008 Magnus Damm
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/input.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/input.h>
#include <linux/input/matrix_keypad.h>

#define ST_KEYSCAN_MAXKEYS 16

#define KEYSCAN_CONFIG_OFF		0x000
#define KEYSCAN_CONFIG_ENABLE		1
#define KEYSCAN_DEBOUNCE_TIME_OFF	0x004
#define KEYSCAN_MATRIX_STATE_OFF	0x008
#define KEYSCAN_MATRIX_DIM_OFF		0x00c
#define KEYSCAN_MATRIX_DIM_X_SHIFT	0
#define KEYSCAN_MATRIX_DIM_Y_SHIFT	2

struct keypad_platform_data {
	const struct matrix_keymap_data *keymap_data;
	unsigned int num_out_pads;
	unsigned int num_in_pads;
	unsigned int debounce_us;
};

struct keyscan_priv {
	void __iomem *base;
	int irq;
	struct clk *clk;
	struct input_dev *input_dev;
	struct keypad_platform_data *config;
	unsigned int last_state;
	u32 keycodes[ST_KEYSCAN_MAXKEYS];
};

static irqreturn_t keyscan_isr(int irq, void *dev_id)
{
	struct platform_device *pdev = dev_id;
	struct keyscan_priv *priv = platform_get_drvdata(pdev);
	int state;
	int change;

	state = readl(priv->base + KEYSCAN_MATRIX_STATE_OFF) & 0xffff;
	change = priv->last_state ^ state;

	while (change) {
		int scancode = __ffs(change);
		int down = state & (1 << scancode);

		input_report_key(priv->input_dev, priv->keycodes[scancode],
				 down);
		change ^= 1 << scancode;
	};

	input_sync(priv->input_dev);

	priv->last_state = state;

	return IRQ_HANDLED;
}

static void keyscan_start(struct keyscan_priv *priv)
{
	clk_enable(priv->clk);

	writel(priv->config->debounce_us * (clk_get_rate(priv->clk) / 1000000),
	       priv->base + KEYSCAN_DEBOUNCE_TIME_OFF);

	writel(((priv->config->num_in_pads - 1) << KEYSCAN_MATRIX_DIM_X_SHIFT) |
	       ((priv->config->num_out_pads - 1) << KEYSCAN_MATRIX_DIM_Y_SHIFT),
	       priv->base + KEYSCAN_MATRIX_DIM_OFF);

	writel(KEYSCAN_CONFIG_ENABLE, priv->base + KEYSCAN_CONFIG_OFF); }

static void keyscan_stop(struct keyscan_priv *priv)
{
	writel(0, priv->base + KEYSCAN_CONFIG_OFF);

	clk_disable(priv->clk);
}


static int keypad_matrix_key_parse_dt(struct keyscan_priv *st_kp)
{
	struct device *dev = st_kp->input_dev->dev.parent;
	struct device_node *np = dev->of_node;
	struct keypad_platform_data *pdata;
	int error;

	pdata = devm_kzalloc(dev, sizeof(*pdata), GFP_KERNEL);
	if (!pdata) {
		dev_err(dev, "failed to allocate driver pdata\n");
		return -ENOMEM;
	}

	error = matrix_keypad_parse_of_params(dev, &pdata->num_out_pads,
			&pdata->num_in_pads);
	if (error) {
		dev_err(dev, "failed to parse keypad params\n");
		return error;
	}

	of_property_read_u32(np, "st,debounce_us", &pdata->debounce_us);

	st_kp->config = pdata;

	dev_info(dev, "rows=%d col=%d debounce=%d\n",
			pdata->num_out_pads,
			pdata->num_in_pads,
			pdata->debounce_us);

	error = of_property_read_u32_array(np, "linux,keymap",
					st_kp->keycodes, ST_KEYSCAN_MAXKEYS);
	if (error) {
		dev_err(dev, "failed to parse keymap\n");
		return error;
	}

	return 0;
}

static int __init keyscan_probe(struct platform_device *pdev)
{
	struct keypad_platform_data *pdata =
		dev_get_platdata(&pdev->dev);
	struct keyscan_priv *st_kp;
	struct input_dev *input_dev;
	struct device *dev = &pdev->dev;
	struct resource *res;
	int len;
	int error;
	int i;

	if (!pdata && !pdev->dev.of_node) {
		dev_err(&pdev->dev, "no keymap defined\n");
		return -EINVAL;
	}

	st_kp = devm_kzalloc(dev, sizeof(*st_kp), GFP_KERNEL);
	if (!st_kp) {
		dev_err(dev, "failed to allocate driver data\n");
		return -ENOMEM;
	}
	st_kp->config = pdata;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(dev, "no I/O memory specified\n");
		return -ENXIO;
	}

	len = resource_size(res);
	if (!devm_request_mem_region(dev, res->start, len, pdev->name)) {
		dev_err(dev, "failed to reserve I/O memory\n");
		return -EBUSY;
	}

	st_kp->base = devm_ioremap_nocache(dev, res->start, len);
	if (st_kp->base == NULL) {
		dev_err(dev, "failed to remap I/O memory\n");
		return -ENXIO;
	}

	st_kp->irq = platform_get_irq(pdev, 0);
	if (st_kp->irq < 0) {
		dev_err(dev, "no IRQ specified\n");
		return -ENXIO;
	}

	error = devm_request_irq(dev, st_kp->irq, keyscan_isr, 0,
				 pdev->name, pdev);
	if (error) {
		dev_err(dev, "failed to request IRQ\n");
		return error;
	}

	input_dev = devm_input_allocate_device(&pdev->dev);
	if (!input_dev) {
		dev_err(&pdev->dev, "failed to allocate the input device\n");
		return -ENOMEM;
	}

	st_kp->clk = devm_clk_get(dev, NULL);
	if (IS_ERR(st_kp->clk)) {
		dev_err(dev, "cannot get clock");
		return PTR_ERR(st_kp->clk);
	}

	input_dev = input_allocate_device();
	if (!input_dev) {
		dev_err(dev, "failed to allocate input device\n");
		return -ENOMEM;
	}
	st_kp->input_dev = input_dev;

	input_dev->name = pdev->name;
	input_dev->phys = "keyscan-keys/input0";
	input_dev->dev.parent = dev;

	input_dev->id.bustype = BUS_HOST;
	input_dev->id.vendor = 0x0001;
	input_dev->id.product = 0x0001;
	input_dev->id.version = 0x0100;

	if (!pdata) {
		error = keypad_matrix_key_parse_dt(st_kp);
		if (error)
			return error;
		pdata = st_kp->config;
	}

	input_dev->keycode = st_kp->keycodes;
	input_dev->keycodesize = sizeof(st_kp->keycodes[0]);
	input_dev->keycodemax = ARRAY_SIZE(st_kp->keycodes);

	for (i = 0; i < ST_KEYSCAN_MAXKEYS; i++)
		input_set_capability(input_dev, EV_KEY, st_kp->keycodes[i]);
	__clear_bit(KEY_RESERVED, input_dev->keybit);

	error = input_register_device(input_dev);
	if (error) {
		dev_err(dev, "failed to register input device\n");
		input_free_device(input_dev);
		platform_set_drvdata(pdev, NULL); /* @@ ? */
		return error;
	}

	platform_set_drvdata(pdev, st_kp);

	keyscan_start(st_kp);

	device_set_wakeup_capable(&pdev->dev, 1);

	return 0;
}

static int __exit keyscan_remove(struct platform_device *pdev)
{
	struct keyscan_priv *priv = platform_get_drvdata(pdev);

	keyscan_stop(priv);

	input_unregister_device(priv->input_dev);
	platform_set_drvdata(pdev, NULL);

	return 0;
}

static int keyscan_suspend(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct keyscan_priv *priv = platform_get_drvdata(pdev);

	if (device_may_wakeup(dev))
		enable_irq_wake(priv->irq);
	else
		keyscan_stop(priv);

	return 0;
}

static int keyscan_resume(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct keyscan_priv *priv = platform_get_drvdata(pdev);

	if (device_may_wakeup(dev))
		disable_irq_wake(priv->irq);
	else
		keyscan_start(priv);

	return 0;
}

static const struct dev_pm_ops keyscan_dev_pm_ops = {
	.suspend = keyscan_suspend,
	.resume = keyscan_resume,
};

static const struct of_device_id keyscan_of_match[] = {
	{ .compatible = "st,keypad" },
	{ },
};
MODULE_DEVICE_TABLE(of, keyscan_of_match);

__refdata struct platform_driver keyscan_device_driver = {
	.probe		= keyscan_probe,
	.remove		= __exit_p(keyscan_remove),
	.driver		= {
		.name	= "st-keyscan",
		.pm	= &keyscan_dev_pm_ops,
		.of_match_table = of_match_ptr(keyscan_of_match),
	}
};

static int __init keyscan_init(void)
{
	return platform_driver_register(&keyscan_device_driver);
}

static void __exit keyscan_exit(void)
{
	platform_driver_unregister(&keyscan_device_driver);
}

module_init(keyscan_init);
module_exit(keyscan_exit);

MODULE_AUTHOR("Stuart Menefy <stuart.menefy@st.com>");
MODULE_DESCRIPTION("STMicroelectronics keyscan device driver");
MODULE_LICENSE("GPL");
