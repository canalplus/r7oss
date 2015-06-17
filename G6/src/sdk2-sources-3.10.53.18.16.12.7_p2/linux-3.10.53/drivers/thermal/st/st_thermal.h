/*
 * ST Thermal Sensor Driver for STi series of SoCs
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
#ifndef __STI_THERMAL_SYSCFG_H
#define __STI_THERMAL_SYSCFG_H

#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/thermal.h>

/* Regfield IDs */
enum {
	/*
	 * The PWR and INTerrupt threshold regfields
	 * share the same index as they are mutually exclusive
	 */
	TEMP_PWR = 0, INT_THRESH_HI = 0,
	DCORRECT,
	OVERFLOW,
	DATA,
	INT_ENABLE,
	INT_THRESH_LOW,
	/* keep last */
	MAX_REGFIELDS
};

/* Thermal sensor power states */
enum st_thermal_power_state {
	POWER_OFF = 0,
	POWER_ON = 1
};

struct st_thermal_sensor;

/**
 * Description of private thermal sensor ops.
 *
 * @power_ctrl: Function for powering on/off a sensor.
		Clock to the sensor is also controlled from this function.
 * @alloc_regfields: Allocate regmap register fields, specific to a sensor.
 * @do_memmap_regmap: Memory map the thermal register space and init regmap
 *		instance or find regmap instance.
 * @register_irq: Register an interrupt handler for a sensor.
 * @clear_irq: Clear the interrupt status in HW and disable polling.
 */
struct st_thermal_sensor_ops {
	int (*power_ctrl)(struct st_thermal_sensor *,
		enum st_thermal_power_state);
	int (*alloc_regfields)(struct st_thermal_sensor *);
	int (*do_memmap_regmap)(struct st_thermal_sensor *);
	int (*register_irq)(struct st_thermal_sensor *);
	int (*enable_irq)(struct st_thermal_sensor *);
	void (*clear_irq)(struct thermal_zone_device *th, unsigned int);
};

/*
 * Description of thermal driver compatible data.
 *
 * @reg_fields: Pointer to the regfields array for a sensor.
 * @sys_compat: Pointer to the syscon node compatible string.
 * @ops: Pointer to private thermal ops for a sensor.
 * @calibration_val: Default calibration value to be written to the DCORRECT
 *		register field for a sensor.
 * @temp_adjust_val: Value to be added/subtracted from the data read from
 *		the sensor. If value needs to be added please provide a
 *		positive value and if it is to be subtracted please provide
 *		a negative value.
 * @crit_temp: The temperature beyond which the SoC should be shutdown to
 *		prevent damage.
 * @passive_temp: The temperature beyond which passive cooling measures
 *		come into effect.
 */
struct st_thermal_compat_data {
	const struct reg_field *reg_fields;
	char *sys_compat;
	struct st_thermal_sensor_ops *ops;
	unsigned int calibration_val;
	int temp_adjust_val;
	int crit_temp;
	int passive_temp;
};

struct st_thermal_sensor {
	struct device *dev;
	struct thermal_zone_device *th_dev;
	struct thermal_cooling_device *cdev;
	struct st_thermal_sensor_ops *ops;
	const struct st_thermal_compat_data *data;
	struct clk *clk;
	struct regmap *regmap;
	struct regmap_field *pwr;
	struct regmap_field *dcorrect;
	struct regmap_field *overflow;
	struct regmap_field *temp_data;
	struct regmap_field *int_thresh_hi;
	struct regmap_field *int_thresh_low;
	struct regmap_field *int_enable;
	int irq;
	void __iomem *mmio_base;
};

/* Helper macros */
#define thzone_to_sensor(th)			((th)->devdata)
#define sensor_to_dev(sensor)			((sensor)->dev)
#define mcelsius(temp)				((temp) * 1000)

#ifdef CONFIG_ST_THERMAL_SYSCFG
extern struct st_thermal_compat_data st_415sas_data;
extern struct st_thermal_compat_data st_415mpe_data;
extern struct st_thermal_compat_data st_416sas_data;
extern struct st_thermal_compat_data st_127_data;
#endif
#ifdef CONFIG_ST_THERMAL_MEMMAP
extern struct st_thermal_compat_data st_416mpe_data;
extern struct st_thermal_compat_data st_407_data;
#endif

/*
 * Function to allocate regfields which are common between memory mapped
 * and sysconf based sensors.
 */
int st_thermal_common_alloc_regfields(struct st_thermal_sensor *sensor);

#endif /* __STI_RESET_SYSCFG_H */
