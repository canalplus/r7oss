/*
 * ST Thermal Sensor Driver for syscfg based sensors.
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

#include <linux/of.h>
#include <linux/module.h>
#include <linux/mfd/syscon.h>

#include "st_thermal.h"

/* STiH415 */
#define STIH415_SYSCFG_FRONT(num)		((num - 100) * 4)
#define STIH415_SYSCFG_MPE(num)			((num - 600) * 4)
#define STIH415_SAS_THSENS_CONF			STIH415_SYSCFG_FRONT(178)
#define STIH415_SAS_THSENS_STATUS		STIH415_SYSCFG_FRONT(198)
#define STIH415_MPE_THSENS_CONF			STIH415_SYSCFG_MPE(607)
#define STIH415_MPE_THSENS_STATUS		STIH415_SYSCFG_MPE(667)

/* STiH416 */
#define STIH416_SYSCFG_FRONT(num)		((num - 1000) * 4)
#define STIH416_SAS_THSENS_CONF			STIH416_SYSCFG_FRONT(1552)
#define STIH416_SAS_THSENS_STATUS1		STIH416_SYSCFG_FRONT(1554)
#define STIH416_SAS_THSENS_STATUS2		STIH416_SYSCFG_FRONT(1594)

/* STiD127 */
#define STID127_SYSCFG_CPU(num)			((num - 700) * 4)
#define STID127_THSENS_CONF			STID127_SYSCFG_CPU(743)
#define STID127_THSENS_STATUS			STID127_SYSCFG_CPU(767)

static const struct reg_field st_415sas_regfields[MAX_REGFIELDS] = {
	[TEMP_PWR] = REG_FIELD(STIH415_SAS_THSENS_CONF, 9, 9),
	[DCORRECT] = REG_FIELD(STIH415_SAS_THSENS_CONF, 4, 8),
	[OVERFLOW] = REG_FIELD(STIH415_SAS_THSENS_STATUS, 8, 8),
	[DATA] = REG_FIELD(STIH415_SAS_THSENS_STATUS, 10, 16),
};

static const struct reg_field st_415mpe_regfields[MAX_REGFIELDS] = {
	[TEMP_PWR] = REG_FIELD(STIH415_MPE_THSENS_CONF, 8, 8),
	[DCORRECT] = REG_FIELD(STIH415_MPE_THSENS_CONF, 3, 7),
	[OVERFLOW] = REG_FIELD(STIH415_MPE_THSENS_STATUS, 9, 9),
	[DATA] = REG_FIELD(STIH415_MPE_THSENS_STATUS, 11, 18),
};

static const struct reg_field st_416sas_regfields[MAX_REGFIELDS] = {
	[TEMP_PWR] = REG_FIELD(STIH416_SAS_THSENS_CONF, 9, 9),
	[DCORRECT] = REG_FIELD(STIH416_SAS_THSENS_CONF, 4, 8),
	[OVERFLOW] = REG_FIELD(STIH416_SAS_THSENS_STATUS1, 8, 8),
	[DATA] = REG_FIELD(STIH416_SAS_THSENS_STATUS2, 10, 16),
};

static const struct reg_field st_127_regfields[MAX_REGFIELDS] = {
	[TEMP_PWR] = REG_FIELD(STID127_THSENS_CONF, 7, 7),
	[DCORRECT] = REG_FIELD(STID127_THSENS_CONF, 2, 6),
	[OVERFLOW] = REG_FIELD(STID127_THSENS_STATUS, 9, 9),
	[DATA] = REG_FIELD(STID127_THSENS_STATUS, 11, 18),
};

/* Private OPs for SYSCFG based thermal sensors */
static int st_syscfg_power_ctrl(struct st_thermal_sensor *sensor,
				enum st_thermal_power_state power_state)
{
	return regmap_field_write(sensor->pwr, power_state);
}

static int st_syscfg_alloc_regfields(struct st_thermal_sensor *sensor)
{
	struct device *dev = sensor_to_dev(sensor);
	int ret;

	ret = st_thermal_common_alloc_regfields(sensor);
	if (ret)
		return ret;

	sensor->pwr = devm_regmap_field_alloc(dev, sensor->regmap,
				sensor->data->reg_fields[TEMP_PWR]);
	if (IS_ERR(sensor->pwr)) {
		dev_err(dev, "%s,failed to alloc reg field\n", __func__);
		return PTR_ERR(sensor->pwr);
	}

	return 0;
}

static int st_syscfg_do_memmap_regmap(struct st_thermal_sensor *sensor)
{
	sensor->regmap =
		syscon_regmap_lookup_by_compatible(sensor->data->sys_compat);
	if (IS_ERR(sensor->regmap)) {
		dev_err(sensor_to_dev(sensor), "could find regmap\n");
		return PTR_ERR(sensor->regmap);
	}

	return 0;
}

static struct st_thermal_sensor_ops st_syscfg_sensor_ops = {
	.power_ctrl = st_syscfg_power_ctrl,
	.alloc_regfields = st_syscfg_alloc_regfields,
	.do_memmap_regmap = st_syscfg_do_memmap_regmap,
};

/* Compatible device data for stih415 sas thermal sensor */
struct st_thermal_compat_data st_415sas_data = {
	.reg_fields = st_415sas_regfields,
	.sys_compat = "st,stih415-front-syscfg",
	.ops = &st_syscfg_sensor_ops,
	.calibration_val = 16,
	.temp_adjust_val = 20,
	.passive_temp = 80,
	.crit_temp = 125,
};

/* Compatible device data for stih415 mpe thermal sensor */
struct st_thermal_compat_data st_415mpe_data = {
	.reg_fields = st_415mpe_regfields,
	.sys_compat = "st,stih415-system-syscfg",
	.ops = &st_syscfg_sensor_ops,
	.calibration_val = 16,
	.temp_adjust_val = -103,
	.passive_temp = 80,
	.crit_temp = 125,
};

/* Compatible device data for stih416 sas thermal sensor */
struct st_thermal_compat_data st_416sas_data = {
	.reg_fields = st_416sas_regfields,
	.sys_compat = "st,stih416-front-syscfg",
	.ops = &st_syscfg_sensor_ops,
	.calibration_val = 16,
	.temp_adjust_val = 20,
	.passive_temp = 80,
	.crit_temp = 125,
};

/* Compatible device data for stid127 thermal sensor */
struct st_thermal_compat_data st_127_data = {
	.reg_fields = st_127_regfields,
	.sys_compat = "st,stid127-cpu-syscfg",
	.ops = &st_syscfg_sensor_ops,
	.calibration_val = 8,
	.temp_adjust_val = -103,
	.passive_temp = 80,
	.crit_temp = 125,
};
