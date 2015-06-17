/***********************************************************************
 *
 * File: linux/kernel/drivers/stm/hdmirx/stiH407/stiH407.c
 * Copyright (c) 2011-2012 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 * HDMI Rx route platform data pertaining to ORLY soc STiH407
\***********************************************************************/

/******************************************************************************
  I N C L U D E    F I L E S
******************************************************************************/
/* Standard Includes ----------------------------------------------*/
#include <linux/types.h>
#include <linux/platform_device.h>
#include <linux/stm/gpio.h>
#include <linux/stm/stih407.h>

/* Local Includes -------------------------------------------------*/
#include <hdmirxplatform.h>
#include <stddefs.h>

/**************************C O D E**************************************/
stm_hdmirx_platform_route_t stm_hdmirx_route_data[] =
{
  {
    .id = 0,
    .phy = {
      .start_addr = 0x08F80000,
      .end_addr = 0x08F87FFF,
    },
    .core = {
      .start_addr = 0x08D10000,
      .end_addr = 0x08D17fff,
    },
    .clock_gen = {
      .start_addr = 0x08D10000, //??
      .end_addr = 0x08D17fff, //??
      .audio_clk_gen_id = 1,
      .video_clk_gen_id = 0,
    },
    .irq_num = STIH407_IRQ(110),
    .output_pixel_width = STM_HDMIRX_ROUTE_OUTPUT_PIXEL_WIDTH_10_BITS,
    .i2s_out_clk_scale_factor = 256,
    .rterm_mode = 0x3,
    .rterm_val  = 0x3,
  },
};
stm_hdmirx_platform_soc_t stm_hdmirx_soc_data =
{
  .num_routes = ARRAY_SIZE(stm_hdmirx_route_data),
  .route = stm_hdmirx_route_data,
  .csm = {
    .start_addr = 0x094A0770,
    .end_addr = 0x094A476F,
  },
  .meas_clk_freq_hz = 30000000,
};

stm_hdmirx_platform_port_t stm_hdmirx_port_data[] =
{
  {
    .id = 0,
    .csm_port_id = {0, 0, 1},
    .pd_config = {
      .option = STM_HDMIRX_POWER_DETECT_OPTION_GPIO,
      .pin.pd_pio = stm_gpio(STIH407_GPIO(5), 5),
    },
    .hpd_pio = stm_gpio(STIH407_GPIO(5), 4),
    .scl_pio = stm_gpio(STIH407_GPIO(5), 7),
    .sda_pio = stm_gpio(STIH407_GPIO(5), 6),
    .edid_wp = stm_gpio(STIH407_GPIO(2), 5),
    .route_connectivity_mask = 0x3,
    .internal_edid = FALSE,
    .enable_hpd = TRUE,
    .enable_ddc2bi = FALSE,
    .ext_mux = FALSE,
    .eq_mode = STM_HDMIRX_ROUTE_EQ_MODE_MEDIUM_GAIN,
    .eq_config = {
      .low_freq_gain = 0x7,
      .high_freq_gain = 0x0,
    },
    .op_mode = STM_HDMIRX_ROUTE_OP_MODE_AUTO,
  },

};

stm_hdmirx_platform_board_t stm_hdmirx_board_data =
{
  .num_ports = ARRAY_SIZE(stm_hdmirx_port_data),
  .port = stm_hdmirx_port_data,
  .set_ext_mux = NULL,
  .ext_mux_i2c.bus = 2,
  .ext_mux_i2c.dev_addr = 0xAE,	// A0  and then right shift while assign
};

stm_hdmirx_platform_data_t stm_hdmirx_platform_data =
{
  .board = &stm_hdmirx_board_data,
  .soc = &stm_hdmirx_soc_data,
};

stm_hdmirx_platform_data_t * stmhdmirx_get_platform_data(void)
{
  return (&stm_hdmirx_platform_data);
}
