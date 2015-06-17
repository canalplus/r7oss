/*
 * STMicroelectronics Remote Processor driver
 *
 * Copyright (C) 2013 STMicroelectronics
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/err.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/of.h>
#include <linux/mfd/syscon.h>
#include <linux/regmap.h>
#include <linux/reset.h>
#include <linux/interrupt.h>
#include <linux/of_irq.h>
#include <linux/remoteproc.h>
#include "remoteproc_internal.h"

/**
 * struct st_rproc - ST remote processor platform data
 * See the documentation in the devicetree binding for an explanation of
 * these fields.
 */
struct st_rproc {
	struct reset_control	*man_reset;
	struct reset_control	*pwr_reset;
	struct regmap_field	*boot_addr;
	unsigned int		irq;
};

/* This is the handler for the watchdog interrupt */
static irqreturn_t st_interrupt(int irq, void *ptr)
{
	struct rproc *rproc = ptr;

	rproc_report_crash(rproc, RPROC_WATCHDOG);
	return IRQ_HANDLED;
}

static void st_enable_interrupt(struct rproc *rproc)
{
	struct st_rproc *oproc = rproc->priv;

	/*
	 * The trigger level should be low, but the GIC cannot handle that.
	 * Using rising does the trick as well it seems.
	 */
	if (oproc->irq &&
		request_irq(oproc->irq, st_interrupt,
				IRQF_NO_SUSPEND|IRQF_TRIGGER_RISING,
				rproc->name, rproc)) {
		dev_err(&rproc->dev, "cannot allocate irq.\n");
		oproc->irq = 0;
	}
}

/* Power off the remote processor */
static int st_rproc_stop(struct rproc *rproc)
{
	struct st_rproc *oproc = rproc->priv;
	int	ret;

	if (oproc->irq) {
		disable_irq(oproc->irq);
		free_irq(oproc->irq, rproc);
	}

	/* The reset bits have negated logic: 0 puts the CPU in reset. */
	ret = reset_control_assert(oproc->man_reset);
	return ret;
}

/*
 * Power up the remote processor.
 *
 * This function will be invoked only after the firmware for this rproc
 * was loaded, parsed successfully, and all of its resource requirements
 * were met.
 */
static int st_rproc_start(struct rproc *rproc)
{
	struct st_rproc *oproc = rproc->priv;
	int	ret;

	/* Stop the processor */
	ret = reset_control_assert(oproc->man_reset);

	dev_info(&rproc->dev, "starting from 0x%x.\n", rproc->bootaddr);
	ret = regmap_field_write(oproc->boot_addr, rproc->bootaddr >> 1);

	/*
	 * The power reset will force the reset even if it has any outstanding
	 * bus transactions left.
	 */
	mdelay(100);
	ret = reset_control_assert(oproc->pwr_reset);
	mdelay(100);
	ret = reset_control_deassert(oproc->man_reset);
	mdelay(100);
	ret = reset_control_deassert(oproc->pwr_reset);

	/* Give the processor some time before enabling the interrupt */
	mdelay(100);
	st_enable_interrupt(rproc);

	return ret;
}

static struct rproc_ops st_rproc_ops = {
	.start		= st_rproc_start,
	.stop		= st_rproc_stop,
};

/*
 * Determine the current state of the processor: 0 is off, 1 is on.
 */
static unsigned st_rproc_state(struct st_rproc *oproc)
{
	int		ret;
	unsigned	power_state = 0;

	/* First check the power reset. */
	ret = reset_control_is_asserted(oproc->pwr_reset);
	if (ret == 0) {

		/* Now check the manual reset. */
		ret = reset_control_is_asserted(oproc->man_reset);
		if (ret == 0)
			power_state = 1;
	}

	return power_state;
}

/*
 * This function parses an DT property that looks like one of these:
 *	name = <regmap_phandle reg lsb msb>;
 *	name = <regmap_phandle reg lsb>;
 *	name = <regmap_phandle reg>;
 * into the register map field given in "reg".
 */
static struct regmap_field *of_regmap_parse(struct platform_device *pdev,
					    const char *name)
{
	struct device_node *np = pdev->dev.of_node;
	struct property *pp;
	struct regmap *regmap;
	struct reg_field reg_field;
	struct regmap_field *reg;
	int	len, nr_props;
	const __be32 *list;

	pp = of_find_property(np, name, &len);
	if (!pp)
		return NULL;

	nr_props = pp->length/sizeof(u32);
	if (nr_props < 2) {
		dev_err(&pdev->dev, "bad property format for %s.\n", name);
		return NULL;
	}

	regmap = syscon_regmap_lookup_by_phandle(np, name);
	if (!regmap) {
		dev_err(&pdev->dev, "can not find register map for %s.\n",
			name);
		return NULL;
	}

	list = pp->value;
	/* Skip the 1st attribute: it is the phandle that was parsed above. */
	list++;
	reg_field.reg = be32_to_cpup(list++);
	if (nr_props > 2)
		reg_field.lsb = be32_to_cpup(list++);
	if (nr_props > 3)
		reg_field.msb = be32_to_cpup(list++);
	else
		reg_field.msb = 32;

	reg = devm_regmap_field_alloc(&pdev->dev, regmap, reg_field);
	return reg;
}

static int st_rproc_probe(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	struct st_rproc *oproc;
	struct rproc *rproc;
	int ret = -ENOMEM;
	unsigned power_state;

	BUG_ON(!np);
	rproc = rproc_alloc(&pdev->dev, np->name, &st_rproc_ops, NULL,
			    sizeof(struct st_rproc));
	if (!rproc)
		return ret;

	oproc = rproc->priv;

	/* Get all our device properties */
	oproc->man_reset = devm_reset_control_get(&pdev->dev, "manual_reset");
	oproc->pwr_reset = devm_reset_control_get(&pdev->dev, "power_reset");
	oproc->boot_addr = of_regmap_parse(pdev, "boot_addr");
	if (!oproc->man_reset || !oproc->pwr_reset || !oproc->boot_addr) {
		dev_err(&pdev->dev, "missing a mandatory property.\n");
		goto out_free;
	}
	/* The IRQ property is optional */
	oproc->irq = irq_of_parse_and_map(np, 0);

	/* The processor could already be running. Here we detect that. */
	power_state = st_rproc_state(oproc);
	if (power_state) {
		atomic_inc(&rproc->power);
		rproc->state = RPROC_RUNNING;
	}

	ret = rproc_add(rproc);
	if (ret)
		goto out_free;

	platform_set_drvdata(pdev, rproc);
	if (power_state)
		st_enable_interrupt(rproc);
	return ret;
out_free:
	rproc_put(rproc);
	return ret;
}

static int st_rproc_remove(struct platform_device *pdev)
{
	struct rproc *rproc = platform_get_drvdata(pdev);
	struct st_rproc *oproc = rproc->priv;

	/* Do not stop the remote processor, but do disable the interrupt */
	if (oproc->irq && (rproc->state != RPROC_OFFLINE)) {
		disable_irq(oproc->irq);
		free_irq(oproc->irq, rproc);
	}

	rproc_del(rproc);
	rproc_put(rproc);
	return 0;
}

static struct of_device_id st_rproc_match[] = {
	{
		.compatible = "st,rproc",
	},
	{},
};

static struct platform_driver st_rproc_driver = {
	.probe = st_rproc_probe,
	.remove = st_rproc_remove,
	.driver = {
		.name = "st-rproc",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(st_rproc_match),
	},
};

MODULE_DEVICE_TABLE(of, st_rproc_match);
module_platform_driver(st_rproc_driver);
MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("STMicroelectronics Remote Processor control driver");
MODULE_AUTHOR("Martin Habets");
