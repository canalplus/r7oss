/*
 * ST Thermal Sensor Driver common routines
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

#include <linux/clk.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/cpu_cooling.h>

#include "st_thermal.h"

/*
 * Function to allocate regfields which are common
 * between syscfg and memory mapped based sensors
 */
int st_thermal_common_alloc_regfields(struct st_thermal_sensor *sensor)
{
	struct device *dev = sensor_to_dev(sensor);
	struct regmap *regmap = sensor->regmap;
	const struct reg_field *reg_fields = sensor->data->reg_fields;

	sensor->dcorrect = devm_regmap_field_alloc(dev, regmap,
					reg_fields[DCORRECT]);

	sensor->overflow = devm_regmap_field_alloc(dev, regmap,
					reg_fields[OVERFLOW]);

	sensor->temp_data = devm_regmap_field_alloc(dev, regmap,
					reg_fields[DATA]);

	if (IS_ERR(sensor->dcorrect) || IS_ERR(sensor->overflow) ||
	    IS_ERR(sensor->temp_data)) {
		return -EINVAL;
	}

	return 0;
}

static int st_thermal_probe_dt(struct st_thermal_sensor *sensor)
{
	struct device *dev = sensor_to_dev(sensor);
	struct device_node *np = dev->of_node;
	int ret;

	sensor->data = of_match_node(dev->driver->of_match_table, np)->data;
	if (!sensor->data || !sensor->data->ops)
		return -EINVAL;

	sensor->ops = sensor->data->ops;

	ret = sensor->ops->do_memmap_regmap(sensor);
	if (ret)
		return ret;

	return sensor->ops->alloc_regfields(sensor);
}

static int st_thermal_sensor_on(struct st_thermal_sensor *sensor)
{
	int ret;
	struct device *dev = sensor_to_dev(sensor);

	ret = clk_prepare_enable(sensor->clk);
	if (ret) {
		dev_err(dev, "unable to enable clk\n");
		goto out;
	}

	ret = sensor->ops->power_ctrl(sensor, POWER_ON);
	if (ret) {
		dev_err(dev, "unable to power on sensor\n");
		goto clk_dis;
	}

	return 0;
clk_dis:
	clk_disable_unprepare(sensor->clk);
out:
	return ret;
}

static int st_thermal_sensor_off(struct st_thermal_sensor *sensor)
{
	int ret;

	ret = sensor->ops->power_ctrl(sensor, POWER_OFF);
	if (ret)
		return ret;

	clk_disable_unprepare(sensor->clk);

	return 0;
}

static int st_thermal_calibration(struct st_thermal_sensor *sensor)
{
	int ret;
	struct device *dev = sensor_to_dev(sensor);

	/* TODO : Read calibration data from SAFEMEM */

	ret = regmap_field_write(sensor->dcorrect,
				 sensor->data->calibration_val);
	if (ret)
		dev_err(dev, "unable to set calibration data\n");

	return ret;
}

/* Callback to get temperature from HW*/
static int st_thermal_get_temp(struct thermal_zone_device *th,
		unsigned long *temperature)
{
	struct st_thermal_sensor *sensor = thzone_to_sensor(th);
	struct device *dev = sensor_to_dev(sensor);
	unsigned int data;
	unsigned int overflow;
	int ret;

	ret = regmap_field_read(sensor->overflow, &overflow);
	if (ret)
		return ret;
	if (overflow)
		return -EIO;

	ret = regmap_field_read(sensor->temp_data, &data);
	if (ret)
		return ret;

	data += sensor->data->temp_adjust_val;
	data = mcelsius(data);
	if (sensor->ops->clear_irq)
		sensor->ops->clear_irq(th, data);

	dev_dbg(dev, "%s:temperature:%d\n", __func__, data);

	*temperature = data;

	return 0;
}

static int st_thermal_get_trip_type(struct thermal_zone_device *th,
				int trip, enum thermal_trip_type *type)
{
	struct st_thermal_sensor *sensor = thzone_to_sensor(th);
	struct device *dev = sensor_to_dev(sensor);

	switch (trip) {
	case 0:
		*type = THERMAL_TRIP_CRITICAL;
		break;
	case 1:
		*type = THERMAL_TRIP_PASSIVE;
		break;
	default:
		dev_err(dev, "invalid trip point\n");
		return -EINVAL;
	}

	return 0;
}

static int st_thermal_get_trip_temp(struct thermal_zone_device *th,
				    int trip, unsigned long *temp)
{
	struct st_thermal_sensor *sensor = thzone_to_sensor(th);
	struct device *dev = sensor_to_dev(sensor);

	switch (trip) {
	case 0:
		*temp = mcelsius(sensor->data->crit_temp);
		break;
	case 1:
		*temp = mcelsius(sensor->data->passive_temp);
		break;
	default:
		dev_err(dev, "Invalid trip point\n");
		return -EINVAL;
	}

	return 0;
}

static int st_thermal_bind(struct thermal_zone_device *th,
			   struct thermal_cooling_device *cdev)
{
	struct st_thermal_sensor *sensor = thzone_to_sensor(th);
	struct device *dev = sensor_to_dev(sensor);
	int ret = 0;

	ret = thermal_zone_bind_cooling_device(th, 1, cdev, THERMAL_NO_LIMIT,
					       THERMAL_NO_LIMIT);
	if (ret)
		dev_err(dev, "binding thermal zone with cdev failed:%d\n", ret);

	return ret;
}

static int st_thermal_unbind(struct thermal_zone_device *th,
			     struct thermal_cooling_device *cdev)
{
	struct st_thermal_sensor *sensor = thzone_to_sensor(th);
	struct device *dev = sensor_to_dev(sensor);
	int ret = 0;

	ret = thermal_zone_unbind_cooling_device(th, 1, cdev);
	if (ret)
		dev_err(dev, "unbinding thermal zone with cdev failed:%d\n",
			ret);

	return ret;
}

static struct thermal_zone_device_ops st_tz_ops = {
	.get_temp	= st_thermal_get_temp,
	.get_trip_type	= st_thermal_get_trip_type,
	.get_trip_temp	= st_thermal_get_trip_temp,
	.bind		= st_thermal_bind,
	.unbind		= st_thermal_unbind,
};

static int st_thermal_probe(struct platform_device *pdev)
{
	struct st_thermal_sensor *sensor;
	struct device *dev = &pdev->dev;
	int ret;
	struct cpumask mask_val;

	if (!dev->of_node) {
		dev_err(dev, "device node not found\n");
		ret = -EINVAL;
		goto out;
	}

	sensor = devm_kzalloc(dev, sizeof(*sensor), GFP_KERNEL);
	if (!sensor) {
		ret = -ENOMEM;
		goto out;
	}

	sensor->dev = dev;
	ret = st_thermal_probe_dt(sensor);
	if (ret)
		goto out;

	sensor->clk = devm_clk_get(dev, "thermal");
	if (IS_ERR(sensor->clk)) {
		dev_err(dev, "unable to get clk\n");
		ret = PTR_ERR(sensor->clk);
		goto out;
	}
	/* Activate thermal sensor */
	ret = st_thermal_sensor_on(sensor);
	if (ret) {
		dev_err(dev, "unable to power on sensor\n");
		goto out;
	}

	ret = st_thermal_calibration(sensor);
	if (ret)
		goto turn_off_exit;

	cpumask_set_cpu(0, &mask_val);
	sensor->cdev = cpufreq_cooling_register(&mask_val);
	if (IS_ERR(sensor->cdev)) {
		dev_err(dev, "Failed to register cpufreq cooling device\n");
		ret = -EINVAL;
		goto turn_off_exit;
	}

	sensor->th_dev = thermal_zone_device_register(
		(char *)dev_name(dev), 2,
		0, (void *)sensor, &st_tz_ops,
		NULL, 1000, sensor->ops->register_irq ? 0 : 1000);

	if (IS_ERR(sensor->th_dev)) {
		dev_err(dev, "unable to register thermal zone device\n");
		ret = PTR_ERR(sensor->th_dev);
		goto turn_off_exit;
	}

	if (sensor->ops->register_irq) {
		ret = sensor->ops->register_irq(sensor);
		if (ret) {
			dev_err(dev, "unable to register irq\n");
			goto unregister_thzone;
		}
	}

	platform_set_drvdata(pdev, sensor);
	dev_info(dev, "STMicroelectronics thermal sensor initialised\n");

	return 0;
unregister_thzone:
	thermal_zone_device_unregister(sensor->th_dev);
turn_off_exit:
	st_thermal_sensor_off(sensor);
out:
	return ret;
}

static int st_thermal_remove(struct platform_device *pdev)
{
	struct st_thermal_sensor *sensor = platform_get_drvdata(pdev);

	st_thermal_sensor_off(sensor);
	thermal_zone_device_unregister(sensor->th_dev);

	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int st_thermal_suspend(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct st_thermal_sensor *sensor = platform_get_drvdata(pdev);

	return st_thermal_sensor_off(sensor);
}

static int st_thermal_resume(struct device *dev)
{
	int ret;
	struct platform_device *pdev = to_platform_device(dev);
	struct st_thermal_sensor *sensor = platform_get_drvdata(pdev);

	ret = st_thermal_sensor_on(sensor);
	if (ret)
		return ret;

	ret = st_thermal_calibration(sensor);
	if (ret)
		return ret;

	if (sensor->ops->enable_irq) {
		ret = sensor->ops->enable_irq(sensor);
		if (ret)
			return ret;
	}

	return 0;
}
#endif

static struct of_device_id st_thermal_of_match[] = {
#ifdef CONFIG_ST_THERMAL_SYSCFG
	{ .compatible = "st,stih415-sas-thermal",
		.data = &st_415sas_data },
	{ .compatible = "st,stih415-mpe-thermal",
		.data = &st_415mpe_data },
	{ .compatible = "st,stih416-sas-thermal",
		.data = &st_416sas_data },
	{ .compatible = "st,stid127-thermal",
		.data = &st_127_data },
#endif
#ifdef CONFIG_ST_THERMAL_MEMMAP
	{ .compatible = "st,stih416-mpe-thermal",
		.data = &st_416mpe_data },
	{ .compatible = "st,stih407-thermal",
		.data = &st_407_data },
#endif
	{ /* sentinel */ }
};

static SIMPLE_DEV_PM_OPS(st_thermal_pm_ops, st_thermal_suspend,
		st_thermal_resume);

MODULE_DEVICE_TABLE(of, st_thermal_of_match);
static struct platform_driver st_thermal_driver = {
	.driver = {
		.name	= "st_thermal",
		.owner  = THIS_MODULE,
		.of_match_table = st_thermal_of_match,
		.pm     = &st_thermal_pm_ops,
	},
	.probe		= st_thermal_probe,
	.remove		= st_thermal_remove,
};

module_platform_driver(st_thermal_driver);

MODULE_AUTHOR("STMicroelectronics (R&D) Limited <ajitpal.singh@st.com>");
MODULE_DESCRIPTION("STMicroelectronics STi SOC thermal sensor driver");
MODULE_LICENSE("GPL v2");
