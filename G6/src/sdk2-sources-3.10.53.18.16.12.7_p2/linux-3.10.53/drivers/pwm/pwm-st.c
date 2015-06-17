/*
 * PWM device driver for ST SoCs.
 * Author: Ajit Pal Singh <ajitpal.singh@st.com>
 *
 * Copyright (C) 2003-2013 STMicroelectronics (R&D) Limited
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 */
#include <linux/bsearch.h>
#include <linux/clk.h>
#include <linux/math64.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/pwm.h>
#include <linux/regmap.h>
#include <linux/slab.h>
#include <linux/mfd/syscon.h>
#include <linux/time.h>

#define ST_PWMVAL(x)	(4 * (x))	/* Value used to define duty cycle */
#define ST_PWMCR	0x50		/* Control/Config reg */
#define ST_INTEN	0x54		/* Interrupt Enable/Disable reg */
#define ST_CNT		0x60		/* PWMCounter */

#define MAX_PWM_CNT_DEFAULT		255
#define MAX_PRESCALE_DEFAULT		0xff
#define NUM_CHAN_DEFAULT		1
#define PWM_PRESCALE_LOW_MASK		0x0f
#define PWM_PRESCALE_HIGH_MASK		0xf0

/* Regfield IDs */
enum {
	PWMCLK_PRESCALE_LOW = 0,
	PWMCLK_PRESCALE_HIGH,
	PWM_EN,
	PWM_INT_EN,
	/* keep last */
	MAX_REGFIELDS
};

struct st_pwm_chip {
	struct device *dev;
	struct clk *clk;
	unsigned long clk_rate;
	struct regmap *regmap;
	struct st_pwm_compat_data *data;
	struct regmap_field *prescale_low;
	struct regmap_field *prescale_high;
	struct regmap_field *pwm_en;
	struct regmap_field *pwm_int_en;
	unsigned long *pwm_periods;
	struct pwm_chip chip;
	void __iomem *mmio_base;
};

struct st_pwm_compat_data {
	const struct reg_field *reg_fields;
	int num_chan;
	int max_pwm_cnt;
	int max_prescale;
};

static const struct reg_field st_pwm_regfields[MAX_REGFIELDS] = {
	[PWMCLK_PRESCALE_LOW] = REG_FIELD(ST_PWMCR, 0, 3),
	[PWMCLK_PRESCALE_HIGH] = REG_FIELD(ST_PWMCR, 11, 14),
	[PWM_EN] = REG_FIELD(ST_PWMCR, 9, 9),
	[PWM_INT_EN] = REG_FIELD(ST_INTEN, 0, 0),
};

static inline struct st_pwm_chip *to_st_pwmchip(struct pwm_chip *chip)
{
	return container_of(chip, struct st_pwm_chip, chip);
}

/*
 * Calculate the period values supported by the PWM for the
 * current clock rate.
 */
static void st_pwm_calc_periods(struct st_pwm_chip *pc)
{
	struct st_pwm_compat_data *data = pc->data;
	struct device *dev = pc->dev;
	int i;
	unsigned long val;

	/*
	 * period_ns = (10^9 * (prescaler + 1) * (MAX_PWM_COUNT + 1)) / CLK_RATE
	 */
	val = NSEC_PER_SEC / pc->clk_rate;
	val *= data->max_pwm_cnt + 1;

	dev_dbg(dev, "possible periods for clkrate[HZ]:%lu\n", pc->clk_rate);
	for (i = 0; i <= data->max_prescale; i++) {
		pc->pwm_periods[i] = val * (i + 1);
		dev_dbg(dev, "prescale:%d, period[ns]:%lu\n", i,
			pc->pwm_periods[i]);
	}
}

static int st_pwm_cmp_periods(const void *key, const void *elt)
{
	unsigned long i = *(unsigned long *)key;
	unsigned long j = *(unsigned long *)elt;

	if (i < j)
		return -1;
	else
		return i == j ? 0 : 1;
}

/*
 * For STiH41x PWM IP, the pwm period is fixed to 256 local clock cycles.
 * The only way to change the period (apart from changing the pwm input clock)
 * is to change the pwm clock prescaler.
 * The prescaler is of 8 bits, so 256 prescaler values and hence
 * 256 possible period values are supported (for a particular clock rate).
 * The requested period will be applied only if it matches one of these
 * 256 values.
 */
static int st_pwm_config(struct pwm_chip *chip, struct pwm_device *pwm,
			 int duty_ns, int period_ns)
{
	struct st_pwm_chip *pc = to_st_pwmchip(chip);
	struct device *dev = pc->dev;
	struct st_pwm_compat_data *data = pc->data;
	unsigned int prescale, pwmvalx;
	int ret;
	unsigned long *found;

	/*
	 * Search for matching period value. The corresponding index is our
	 * prescale value
	 */
	found = bsearch(&period_ns, &pc->pwm_periods[0],
			data->max_prescale + 1, sizeof(unsigned long),
			st_pwm_cmp_periods);
	if (!found) {
		dev_err(dev, "matching period not found\n");
		ret = -EINVAL;
		goto out;
	}

	prescale = found - &pc->pwm_periods[0];
	/*
	 * When PWMVal == 0, PWM pulse = 1 local clock cycle.
	 * When PWMVal == max_pwm_count,
	 * PWM pulse = (max_pwm_count + 1) local cycles,
	 * that is continuous pulse: signal never goes low.
	 */
	pwmvalx = data->max_pwm_cnt * duty_ns / period_ns;
	dev_dbg(dev, "prescale:%u, period:%i, duty:%i, pwmvalx:%u\n",
		prescale, period_ns, duty_ns, pwmvalx);

	/* Enable clock before writing to PWM registers */
	ret = clk_enable(pc->clk);
	if (ret)
		goto out;

	ret = regmap_field_write(pc->prescale_low,
				 prescale & PWM_PRESCALE_LOW_MASK);
	if (ret)
		goto clk_dis;

	ret = regmap_field_write(pc->prescale_high,
				 (prescale & PWM_PRESCALE_HIGH_MASK) >> 4);
	if (ret)
		goto clk_dis;

	ret = regmap_write(pc->regmap, ST_PWMVAL(pwm->hwpwm), pwmvalx);
	if (ret)
		goto clk_dis;

	ret = regmap_field_write(pc->pwm_int_en, 0);
clk_dis:
	clk_disable(pc->clk);
out:
	return ret;
}

static int st_pwm_enable(struct pwm_chip *chip, struct pwm_device *pwm)
{
	int ret;
	struct st_pwm_chip *pc = to_st_pwmchip(chip);
	struct device *dev = pc->dev;

	ret = clk_enable(pc->clk);
	if (ret)
		return ret;

	ret = regmap_field_write(pc->pwm_en, 1);
	if (ret)
		dev_err(dev, "%s,pwm_en write failed\n", __func__);

	return ret;
}

static void st_pwm_disable(struct pwm_chip *chip, struct pwm_device *pwm)
{
	struct st_pwm_chip *pc = to_st_pwmchip(chip);
	struct device *dev = pc->dev;
	unsigned int val;

	if (pc->data->num_chan==1)
	{
	regmap_field_write(pc->pwm_en, 0);

	regmap_read(pc->regmap, ST_CNT, &val);
	dev_dbg(dev, "pwm counter :%u\n", val);

	clk_disable(pc->clk);
	}
}

static const struct pwm_ops st_pwm_ops = {
	.config = st_pwm_config,
	.enable = st_pwm_enable,
	.disable = st_pwm_disable,
	.owner = THIS_MODULE,
};

static int st_pwm_probe_dt(struct st_pwm_chip *pc)
{
	struct device *dev = pc->dev;
	const struct reg_field *reg_fields;
	struct device_node *np = dev->of_node;
	struct st_pwm_compat_data *data = pc->data;
	u32 num_chan;

	of_property_read_u32(np, "st,pwm-num-chan", &num_chan);
	if (num_chan)
		data->num_chan = num_chan;

	reg_fields = data->reg_fields;

	pc->prescale_low =
		devm_regmap_field_alloc(dev, pc->regmap,
					reg_fields[PWMCLK_PRESCALE_LOW]);
	pc->prescale_high =
		devm_regmap_field_alloc(dev, pc->regmap,
					reg_fields[PWMCLK_PRESCALE_HIGH]);
	pc->pwm_en = devm_regmap_field_alloc(dev, pc->regmap,
					     reg_fields[PWM_EN]);
	pc->pwm_int_en = devm_regmap_field_alloc(dev, pc->regmap,
						 reg_fields[PWM_INT_EN]);

	if (IS_ERR(pc->prescale_low) || IS_ERR(pc->prescale_high) ||
		IS_ERR(pc->pwm_en) || IS_ERR(pc->pwm_int_en)) {
		dev_err(dev, "unable to allocate reg_field(s)\n");
		return -EINVAL;
	}
	return 0;
}

static struct regmap_config st_pwm_regmap_config = {
	.reg_bits = 32,
	.val_bits = 32,
	.reg_stride = 4,
};

static int st_pwm_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *np = dev->of_node;
	struct st_pwm_chip *pc;
	struct st_pwm_compat_data *data;
	struct resource *res;
	int ret;

	if (!np) {
		dev_err(dev, "device node not found\n");
		return -EINVAL;
	}

	pc = devm_kzalloc(dev, sizeof(*pc), GFP_KERNEL);
	if (!pc)
		return -ENOMEM;

	data = devm_kzalloc(dev, sizeof(*data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(dev, "no memory resources defined\n");
		return -ENODEV;
	}

	pc->mmio_base = devm_ioremap_resource(dev, res);
	if (IS_ERR(pc->mmio_base))
		return PTR_ERR(pc->mmio_base);

	pc->regmap = devm_regmap_init_mmio(dev, pc->mmio_base,
				&st_pwm_regmap_config);
	if (IS_ERR(pc->regmap))
		return PTR_ERR(pc->regmap);

	/*
	 * Setup PWM data with default values: some values could be replaced
	 * with specific ones provided from device tree
	 */
	data->reg_fields = &st_pwm_regfields[0];
	data->max_pwm_cnt = MAX_PWM_CNT_DEFAULT;
	data->max_prescale = MAX_PRESCALE_DEFAULT;
	data->num_chan = NUM_CHAN_DEFAULT;

	pc->data = data;
	pc->dev = dev;

	ret = st_pwm_probe_dt(pc);
	if (ret)
		return ret;

	pc->pwm_periods = devm_kzalloc(dev,
			sizeof(unsigned long) * (pc->data->max_prescale + 1),
			GFP_KERNEL);
	if (!pc->pwm_periods)
		return -ENOMEM;

	pc->clk = of_clk_get_by_name(np, "pwm");
	if (IS_ERR(pc->clk)) {
		dev_err(dev, "unable to get pwm clock\n");
		return PTR_ERR(pc->clk);
	}

	pc->clk_rate = clk_get_rate(pc->clk);
	if (pc->clk_rate == 0) {
		dev_err(dev, "invalid clk rate\n");
		return -EINVAL;
	}

	ret = clk_prepare(pc->clk);
	if (ret) {
		dev_err(dev, "unable to prepare clk\n");
		return ret;
	}
	st_pwm_calc_periods(pc);

	pc->chip.dev = dev;
	pc->chip.ops = &st_pwm_ops;
	pc->chip.base = -1;
	pc->chip.npwm = pc->data->num_chan;
	pc->chip.can_sleep = true;
	ret = pwmchip_add(&pc->chip);
	if (ret < 0)
		goto out_clk;

	dev_info(dev, "pwm probed successfully\n");
	platform_set_drvdata(pdev, pc);

	return 0;
out_clk:
	clk_unprepare(pc->clk);
	return ret;
}

static int st_pwm_remove(struct platform_device *pdev)
{
	struct st_pwm_chip *pc = platform_get_drvdata(pdev);
	int i;

	for (i = 0; i < pc->data->num_chan; i++)
		pwm_disable(&pc->chip.pwms[i]);

	clk_unprepare(pc->clk);
	return pwmchip_remove(&pc->chip);
}

static struct of_device_id st_pwm_of_match[] = {
	{ .compatible = "st,pwm", },
	{ /* sentinel */ }
};

MODULE_DEVICE_TABLE(of, st_pwm_of_match);
static struct platform_driver st_pwm_driver = {
	.driver = {
		.name	= "st-pwm",
		.owner  = THIS_MODULE,
		.of_match_table = st_pwm_of_match,
	},
	.probe		= st_pwm_probe,
	.remove		= st_pwm_remove,
};

module_platform_driver(st_pwm_driver);
MODULE_AUTHOR("STMicroelectronics (R&D) Limited <ajitpal.singh@st.com>");
MODULE_DESCRIPTION("STMicroelectronics ST PWM driver");
MODULE_LICENSE("GPL v2");
