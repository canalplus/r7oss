/*
 * c8jpg.c, a driver for the C8JPG1 JPEG decoder IP
 *
 * Copyright (C) 2012-2013, STMicroelectronics R&D
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 */

#include <linux/platform_device.h>
#include <linux/kernel.h>
#include <linux/module.h>

#include <linux/stm/device.h>
#include <linux/stm/stih416.h>

#include "c8jpg-drv.h"

static void c8jpg_device_release(struct device *dev)
{
}

struct platform_device c8jpg_device = {
	.name = "stm-c8jpg",
	.id = 0,
	.num_resources = 2,
	.resource = (struct resource[]) {
		STM_PLAT_RESOURCE_MEM_NAMED("c8jpg-io", 0xfd6e2000, 0x1000),
		STIH416_RESOURCE_IRQ_NAMED("c8jpg-int", 95),
	},
	.dev.platform_data = 0,
	.dev.release = c8jpg_device_release
};


int stm_c8jpg_device_init(void)
{
	int ret;

	ret = platform_device_register(&c8jpg_device);

	return ret;
}

void stm_c8jpg_device_exit(void)
{

	platform_device_unregister(&c8jpg_device);

}

