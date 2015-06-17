/***********************************************************************
 *
 * File: hdmirx/include/hdmirx.h
 * Copyright (c) 2012 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef __HDMIRX_H__
#define __HDMIRX_H__
typedef enum stm_hdmirx_operational_mode_e
{
  STM_HDMIRX_ROUTE_OP_MODE_DVI = 0x01,
  STM_HDMIRX_ROUTE_OP_MODE_HDMI = 0x04,
  STM_HDMIRX_ROUTE_OP_MODE_AUTO = 0x08,

} stm_hdmirx_operational_mode_t;

typedef enum stm_hdmirx_equalization_mode_e
{
  STM_HDMIRX_ROUTE_EQ_MODE_DISABLE,
  STM_HDMIRX_ROUTE_EQ_MODE_AUTO,
  STM_HDMIRX_ROUTE_EQ_MODE_LOW_GAIN,
  STM_HDMIRX_ROUTE_EQ_MODE_MEDIUM_GAIN,
  STM_HDMIRX_ROUTE_EQ_MODE_HIGH_GAIN,
  STM_HDMIRX_ROUTE_EQ_MODE_CUSTOM
} stm_hdmirx_equalization_mode_t;
typedef enum stm_hdmirx_output_pixel_width_e
{
  STM_HDMIRX_ROUTE_OUTPUT_PIXEL_WIDTH_8_BITS = 0x01,
  STM_HDMIRX_ROUTE_OUTPUT_PIXEL_WIDTH_10_BITS = 0x02,
  STM_HDMIRX_ROUTE_OUTPUT_PIXEL_WIDTH_12_BITS = 0x04,
  STM_HDMIRX_ROUTE_OUTPUT_PIXEL_WIDTH_16_BITS = 0x08,

} stm_hdmirx_output_pixel_width_t;
typedef enum
{
  HDMIRX_INPUT_PORT_1,
  HDMIRX_INPUT_PORT_2,
  HDMIRX_INPUT_PORT_3,
  HDMIRX_INPUT_PORT_4
} hdmirx_port_t;

typedef struct stm_hdmirx_equalization_config_s
{
  uint32_t low_freq_gain;
  uint32_t high_freq_gain;
  uint32_t rterm_val;

} stm_hdmirx_equalization_config_t;

#endif
