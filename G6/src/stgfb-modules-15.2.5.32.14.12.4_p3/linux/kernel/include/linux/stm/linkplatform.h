/***********************************************************************
 *
 * File: linux/kernel/include/linux/stm/linkplatform.h
 * Copyright (c) 2013 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef LINKPLATFORM_H_
#define LINKPLATFORM_H_

struct stm_link_platform_data {
  uint32_t              device_id;
  uint32_t              pipeline_id;
  uint16_t              i2c_adapter_id;
  uint32_t              hdmi_offset;
  bool                  rxsense_support;
};

#endif /* LINKPLATFORM_H_ */
