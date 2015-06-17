/*
 * Driver for GPIO lines capable of generating external interrupts.
 *
 * Copyright 2014 David Paris <david.paris@st.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/pm.h>
#include <linux/of_platform.h>
#include <linux/of_gpio.h>
#include <linux/of_irq.h>
#include <linux/power/st_lpm.h>

#define ST_GPIO_PINS_PER_BANK	8

struct st_wakeup_pins_drvdata {
	struct st_wkpin_data *wkpins_data;
	int nwkpins;
};

struct st_wkpin_data {
	struct st_lpm_pio_setting pio_settings;
	int irq;
};

static irqreturn_t st_gpio_wakeup_isr(int irq)
{
	return IRQ_HANDLED;
}

static int st_wakeup_get_devtree_child_data(struct device *dev,
					    struct device_node *pp)
{
	struct st_wakeup_pins_drvdata *drvdata = dev_get_drvdata(dev);
	struct st_lpm_pio_setting *pio_settings;
	int *irq, error, gpio;
	static int i=0;
	u32 reg;
	unsigned int gpio_flags;

	pio_settings = &drvdata->wkpins_data[i].pio_settings;
	irq = &drvdata->wkpins_data[i++].irq;

	if (of_property_read_u32(pp, "st,pio_use", &reg)) {
		dev_err(dev, "Error with st,pio_use property\n");
		return -ENODEV;
	}

	pio_settings->pio_use = reg;

	/* In case of EXT_IT pio use, pio_pin is the number of the
	 * ext_it line (0, 1 or 2)
	 */
	if (pio_settings->pio_use == ST_LPM_PIO_EXT_IT) {
		if (of_property_read_u32(pp, "st,ext_it_num", &reg)) {
			dev_err(dev, "st,ext_it_num to be defined (0 to 2)\n");
			return -ENODEV;
		}

		pio_settings->pio_pin = reg;

		*irq = irq_of_parse_and_map(pp, 0);
		if (!*irq) {
			dev_err(dev, "IRQ missing or invalid\n");
			return -EINVAL;
		}
	} else {
		gpio = of_get_gpio(pp, 0);
		if (gpio < 0) {
			error = gpio;
			if (error != -EPROBE_DEFER)
				dev_err(dev, "Failed to get gpio, error: %d\n",
					error);
			return error;
		}

		pio_settings->pio_bank = gpio / ST_GPIO_PINS_PER_BANK;
		pio_settings->pio_pin = gpio % ST_GPIO_PINS_PER_BANK;

		if (of_property_read_u32(pp, "st,pio_dir", &reg)) {
			dev_err(dev, "No st,pio_dir property\n");
			return -ENODEV;
		}

		pio_settings->pio_direction = reg;

		pio_settings->interrupt_enabled =
			of_property_read_bool(pp, "st,int_enabled");

		if (!pio_settings->interrupt_enabled) {
			if (of_property_read_u32(pp, "st,pio_level", &reg)) {
				dev_err(dev, "No st,pio_level property\n");
				return -ENODEV;
			}
			pio_settings->pio_level = reg;
		}

		if (!pio_settings->pio_direction == GPIOF_DIR_OUT)
			gpio_flags = pio_settings->pio_level ?
			GPIOF_OUT_INIT_HIGH : GPIOF_OUT_INIT_LOW;
		else
			gpio_flags = GPIOF_IN;

#if 0 /* MARCO - START - Conflict with gpio_keys */
		error = gpio_request_one(gpio, gpio_flags, "gpio-wakeup");
		if (error < 0) {
			dev_err(dev, "Failed to request GPIO %d, error %d\n",
				gpio, error);
			return error;
		}

		*irq = gpio_to_irq(gpio);
		if (*irq < 0) {
			error = *irq;
			dev_err(dev, "Unable to get irq number for GPIO %d, error %d\n",
				gpio, error);
			return error;
		}
#endif /* MARCO - STOP  - Conflict with gpio_keys */
	}

#if 0 /* MARCO - START - Conflict with gpio_keys */
	error = request_any_context_irq(*irq, (irq_handler_t)st_gpio_wakeup_isr,
					IRQF_TRIGGER_RISING,
					dev->kobj.name, NULL);
	if (error < 0) {
		dev_err(dev, "Unable to claim irq %d; error %d\n", *irq, error);
		return error;
	}
#endif /* MARCO - STOP  - Conflict with gpio_keys */

	return 0;
}

static int st_wakeup_pins_get_devtree_drvdata(struct device *dev)
{
	struct device_node *node, *pp;
	struct st_wakeup_pins_drvdata *drvdata = dev_get_drvdata(dev);
	int nwkpins;

	node = dev->of_node;
	if (!node)
		return -ENODEV;

	nwkpins = of_get_child_count(node);
	if (nwkpins == 0)
		return -ENODEV;

	drvdata->wkpins_data = devm_kzalloc(dev, nwkpins *
			sizeof(struct st_wkpin_data), GFP_KERNEL);
	if (!drvdata->wkpins_data)
		return -ENOMEM;

	if (pinctrl_pm_select_default_state(dev))
		return -ENODEV;

	for_each_child_of_node(node, pp)
		st_wakeup_get_devtree_child_data(dev, pp);

	drvdata->nwkpins = nwkpins;

	return 0;
}

static int st_wakeup_pins_restore_pins_config(void *dev)
{
	struct st_wakeup_pins_drvdata *drvdata;
	int i;

	drvdata = dev_get_drvdata(dev);

	for (i = 0; i < drvdata->nwkpins; i++)
		st_lpm_setup_pio(&drvdata->wkpins_data[i].pio_settings);

	return 0;
}

static struct of_device_id st_wakeup_pins_of_match[] = {
	{ .compatible = "st,wakeup-pins", },
	{ },
};
MODULE_DEVICE_TABLE(of, st_wakeup_pins_of_match);

static int st_wakeup_pins_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct st_wakeup_pins_drvdata *drvdata;
	int i, err = 0;

	drvdata = devm_kzalloc(dev, sizeof(struct st_wakeup_pins_drvdata),
			       GFP_KERNEL);

	if (drvdata) {
		platform_set_drvdata(pdev, drvdata);
	} else {
		dev_err(dev, "Error while requesting driver data memory\n");
		return PTR_ERR(drvdata);
	}

	err = st_wakeup_pins_get_devtree_drvdata(dev);
	if (err) {
		dev_err(dev, "Error while getting DT data\n");
		return err;
	}

	for (i = 0; i < drvdata->nwkpins; i++)
		st_lpm_setup_pio(&drvdata->wkpins_data[i].pio_settings);

	device_init_wakeup(&pdev->dev, true);

	st_lpm_register_callback(ST_LPM_GPIO_WAKEUP,
				 st_wakeup_pins_restore_pins_config,
				 (void *)&pdev->dev);

	return 0;
}

static int st_wakeup_pins_remove(struct platform_device *pdev)
{
	device_init_wakeup(&pdev->dev, false);

	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int st_wakeup_pins_suspend(struct device *dev)
{
/* MARCO - START - Conflict with gpio_keys */
#if 0
	struct st_wakeup_pins_drvdata *drvdata = dev_get_drvdata(dev);
	int i;

	for (i = 0; i < drvdata->nwkpins; i++)
		enable_irq_wake(drvdata->wkpins_data[i].irq);
#endif
/* MARCO - STOP  - Conflict with gpio_keys */

	return 0;
}

static int st_wakeup_pins_resume(struct device *dev)
{
/* MARCO - START - Conflict with gpio_keys */
#if 0
	struct st_wakeup_pins_drvdata *drvdata = dev_get_drvdata(dev);
	int i;

	for (i = 0; i < drvdata->nwkpins; i++)
		disable_irq_wake(drvdata->wkpins_data[i].irq);
#endif
/* MARCO - STOP  - Conflict with gpio_keys */

	return 0;
}
#endif

static SIMPLE_DEV_PM_OPS(st_wakeup_pins_pm_ops, st_wakeup_pins_suspend,
			 st_wakeup_pins_resume);

static struct platform_driver st_wakeup_pins_device_driver = {
	.probe		= st_wakeup_pins_probe,
	.remove		= st_wakeup_pins_remove,
	.driver		= {
		.name	= "st-wakeup-pins",
		.owner	= THIS_MODULE,
		.pm	= &st_wakeup_pins_pm_ops,
		.of_match_table = of_match_ptr(st_wakeup_pins_of_match),
	}
};

static int __init st_wakeup_pins_init(void)
{
	return platform_driver_register(&st_wakeup_pins_device_driver);
}

static void __exit st_wakeup_pins_exit(void)
{
	platform_driver_unregister(&st_wakeup_pins_device_driver);
}

module_init(st_wakeup_pins_init);
module_exit(st_wakeup_pins_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("David Paris <david.paris@st.com>");
MODULE_DESCRIPTION("ST driver for wakeup pins");
MODULE_ALIAS("platform:st-wakeup-pins");
