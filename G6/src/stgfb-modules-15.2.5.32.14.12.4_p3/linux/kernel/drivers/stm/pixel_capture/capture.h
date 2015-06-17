/***********************************************************************
 *
 * File: linux/kernel/drivers/stm/pixel_capture/capture.h
 * Copyright (c) 2012 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef __STM_PIXEL_CAPTURE_H__
#define __STM_PIXEL_CAPTURE_H__

#include <linux/device.h>
#include <linux/types.h>
#include <stm_display.h>
#include <linux/stm/stmcoredisplay.h>

#include "class.h"
#include "device_features.h"

#define STM_CAPTURE_MAX_DEVICES   4
#define STM_CAPTURE_MAX_INSTANCES 4
#define STM_CAPTURE_MAX_CALLBACKS 2

struct stm_capture_config {
  int                             device_id;
  stm_pixel_capture_h             pixel_capture;
  stm_pixel_capture_device_type_t device_type;
  char                            name[20];
  stm_pixel_capture_hw_features_t hw_features;
};

int stm_capture_register_device (struct device                     *dev,
         struct device_attribute           *attrs,
         void                              *data);
int stm_capture_unregister_device (void *data);

#endif /* __STM_BDISPII_AQ_H__ */
