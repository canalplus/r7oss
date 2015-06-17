/***********************************************************************
 *
 * File: linux/kernel/drivers/stm/pixel_capture/device_features.h
 * Copyright (c) 2012 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef __BDISPII_DEVICE_FEATURES_H__
#define __BDISPII_DEVICE_FEATURES_H__

#include <linux/types.h>
#include <stm_pixel_capture.h>
#include <capture_device_priv.h>

struct _stm_capture_hw_features {
  char                     *name;
  stm_pixel_capture_device_type_t type;
  stm_pixel_capture_hw_features_t hw_features;
};

#endif /* __BDISPII_DEVICE_FEATURES_H__ */
