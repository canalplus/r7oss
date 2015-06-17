/***********************************************************************
 *
 * File: linux/kernel/drivers/stm/pixel_capture/class.h
 * Copyright (c) 2012 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef __STM_CAPTURE_CLASS_H__
#define __STM_CAPTURE_CLASS_H__

#include <linux/init.h>
#include <linux/device.h>
#include <linux/mm.h>
#include <linux/fs.h>

#include <stm_pixel_capture.h>

#include "debug.h"

struct stm_capture_class_device;

int __init stm_capture_class_init (int n_devices);
void stm_capture_class_cleanup (int n_devices);

struct stm_capture_class_device *
stm_capture_classdev_init (int                                i,
			   struct device                     *parent,
			   void                              *dev_data,
			   struct device_attribute           *dev_attrs);

int
stm_capture_classdev_deinit (struct stm_capture_class_device *dev);


#endif /* __STM_CAPTURE_CLASS_H__ */
