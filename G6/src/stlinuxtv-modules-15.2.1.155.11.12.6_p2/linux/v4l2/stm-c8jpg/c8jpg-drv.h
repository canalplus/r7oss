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

#ifndef __STM_C8JPG_DRV_H
#define __STM_C8JPG_DRV_H

/* external symbols */

extern struct v4l2_m2m_ops c8jpg_v4l2_m2m_ops;
extern struct v4l2_ioctl_ops stm_c8jpg_vidioc_ioctls_ops;
extern struct v4l2_file_operations stm_c8jpg_file_ops;

extern irqreturn_t c8jpg_irq_handler(int irq, void *priv);

extern struct platform_driver stm_c8jpg_driver;

extern int stm_c8jpg_v4l2_pm_prepare(struct device *dev);

int stm_c8jpg_device_init(void);

void stm_c8jpg_device_exit(void);

#endif /* __STM_C8JPG_DRV_H */
