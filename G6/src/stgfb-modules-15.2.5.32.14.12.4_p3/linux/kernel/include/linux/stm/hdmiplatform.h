/***********************************************************************
 *
 * File: linux/kernel/include/linux/stm/hdmiplatform.h
 * Copyright (c) 2012 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef HDMIPLATFORM_H_
#define HDMIPLATFORM_H_

struct stm_hdmi_platform_data {
  uint32_t              device_id;
  uint32_t              pipeline_id;
#ifndef CONFIG_ARCH_STI
  struct stm_pad_config hpd_pad_config;
  uint16_t              hpd_poll_pio;
#endif
};

#endif /* HDMIPLATFORM_H_ */
