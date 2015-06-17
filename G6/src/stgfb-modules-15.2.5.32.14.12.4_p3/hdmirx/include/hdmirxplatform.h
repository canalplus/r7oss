/***********************************************************************
 *
 * File: hdmirx/include/hdmirxplatform.h
 * Copyright (c) 2005-2012 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef __PLATFORM_DEV_H__
#define __PLATFORM_DEV_H_

#include <linux/device.h>
#ifndef CONFIG_ARCH_STI
#include <linux/stm/pad.h>
#else
#include <linux/pinctrl/consumer.h>
#endif
#include <hdmirx.h>
#include <stm_hdmirx.h>

typedef struct stm_hdmirx_resource_s
{
  uint32_t start_addr;
  uint32_t end_addr;
  uint8_t rterm_mode;
  uint8_t rterm_val;
} stm_hdmirx_resource_t;

typedef struct stm_hdmirx_resource_clk_s
{
  uint32_t start_addr;
  uint32_t end_addr;
  uint32_t audio_clk_gen_id;  // Id for Audio DDS
  uint32_t video_clk_gen_id;  // Id for Video DDS
} stm_hdmirx_resource_clk_t;
typedef struct stm_hdmirx_resource_i2c_s
{
  uint32_t dev_addr;
  uint32_t bus;
} stm_hdmirx_resource_i2c_t;

//-----------------------------------------------------------------------------------------------------------
// Board specific definitions
//-----------------------------------------------------------------------------------------------------------
// fn. defintion for programming external mux
typedef int (*stm_hdmirx_set_ext_mux_t) (uint32_t port_id);

typedef enum
{
  STM_HDMIRX_POWER_DETECT_OPTION_GPIO,
  STM_HDMIRX_POWER_DETECT_OPTION_LBADC
} stm_hdmirx_port_detect_option_t;

typedef struct stm_hdmirx_port_detect_config_s
{
  stm_hdmirx_port_detect_option_t option;
  union
  {
    uint32_t pd_pio;
    uint32_t pd_lbadc;
  } pin;

} stm_hdmirx_port_detect_config_t;

// Platform Port configuration

typedef struct stm_hdmirx_platform_port_s
{
  uint32_t id;    // port identifier
  uint32_t csm_port_id[3];  // hardware identifier for mapping
  stm_hdmirx_port_detect_config_t pd_config;  // power detection configuration
  uint32_t hpd_pio; // hpd gpio pin number
  uint32_t scl_pio; // i2c scl gpio pin number
  uint32_t sda_pio; // i2c sda gpio pin number
  #ifndef CONFIG_ARCH_STI
  struct stm_pad_config * Pad_Config_p;
  #else
  struct pinctrl * pinctrl_p;
  #endif
  uint32_t edid_wp;
  uint32_t route_connectivity_mask;
  bool internal_edid; // to be set to true if internal edid is chosen
  bool enable_hpd;  // to be set to true if hot plug detect is chosen
  bool enable_ddc2bi; // to be set to true if ddc2bi function is chosen
  bool ext_mux;   // to be set to true if port is defined thru external mux
  stm_hdmirx_equalization_mode_t eq_mode; // equalization to be configured
  stm_hdmirx_equalization_config_t eq_config;
  stm_hdmirx_operational_mode_t op_mode;

} stm_hdmirx_platform_port_t;

typedef struct stm_hdmirx_platform_board_s
{
  uint32_t num_ports;
  stm_hdmirx_platform_port_t *port;
  stm_hdmirx_set_ext_mux_t set_ext_mux; // fn pointer to external mux programming
  stm_hdmirx_resource_i2c_t ext_mux_i2c;

} stm_hdmirx_platform_board_t;

//-----------------------------------------------------------------------------------------------------------
// SOC specific definitions
//-----------------------------------------------------------------------------------------------------------
// Platform route configuration
typedef struct stm_hdmirx_platform_route_s
{
  uint32_t id;    // route identifier
  stm_hdmirx_resource_t phy;  // hdmirx phy resource definition
  stm_hdmirx_resource_t core; // hdmirx core resource definition
  stm_hdmirx_resource_clk_t clock_gen;
  uint32_t irq_num;
  stm_hdmirx_output_pixel_width_t output_pixel_width; // output pixel width to be selected
  uint32_t i2s_out_clk_scale_factor;  // i2s clock out scaling factor to be set
} stm_hdmirx_platform_route_t;

typedef struct stm_hdmirx_platform_soc_s
{
  uint32_t num_routes;
  stm_hdmirx_platform_route_t *route;
  stm_hdmirx_resource_t csm;
  uint32_t meas_clk_freq_hz;

} stm_hdmirx_platform_soc_t;

//-----------------------------------------------------------------------------------------------------------
// Platform Device defintion (soc + board)
//-----------------------------------------------------------------------------------------------------------
typedef struct stm_hdmirx_platform_data_s
{
  stm_hdmirx_platform_soc_t *soc; // platform data pertaining to soc
  stm_hdmirx_platform_board_t *board; // platform data pertaining to board
} stm_hdmirx_platform_data_t;

typedef struct stm_hdmirx_platform_device_s
{
  struct platform_device *device;
  bool   isPlatformDeviceRegistred;
} stm_hdmirx_platform_device_t;

stm_hdmirx_platform_data_t * stmhdmirx_get_platform_data(void);
#endif
