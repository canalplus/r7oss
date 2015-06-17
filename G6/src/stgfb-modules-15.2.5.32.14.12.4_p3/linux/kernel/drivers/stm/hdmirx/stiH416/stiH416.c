/***********************************************************************
 *
 * File: linux/kernel/drivers/stm/hdmirx/stiH416/stiH416.c
 * Copyright (c) 2011-2012 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 * HDMI Rx route platform data pertaining to ORLY soc STiH416
\***********************************************************************/

/***********************************************************************
  I N C L U D E    F I L E S
************************************************************************/
/* Standard Includes ----------------------------------------------*/
#include <linux/types.h>
#include <linux/platform_device.h>
#include <linux/stm/gpio.h>
#include <linux/stm/stih416.h>

/* Local Includes -------------------------------------------------*/
#include <hdmirxplatform.h>
#include <stddefs.h>

/**************************C O D E**************************************/
stm_hdmirx_platform_route_t stm_hdmirx_route_data[] =
{
  {
    .id = 0,
    .phy = {
      .start_addr = 0xfe878000,
      .end_addr = 0xfe87FFFF,
    },
    .core = {
      .start_addr = 0xfe870000,
      .end_addr = 0xfe877fff,
    },
    .clock_gen = {
      .start_addr = 0xfee56000,
      .end_addr = 0xfee56FFF,
      .audio_clk_gen_id = 1,
      .video_clk_gen_id = 2,
    },
    .irq_num = STIH416_IRQ(129),
    .output_pixel_width = STM_HDMIRX_ROUTE_OUTPUT_PIXEL_WIDTH_10_BITS,
    .i2s_out_clk_scale_factor = 256,
  },
};
stm_hdmirx_platform_soc_t stm_hdmirx_soc_data =
{
  .num_routes = ARRAY_SIZE(stm_hdmirx_route_data),
  .route = stm_hdmirx_route_data,
  .csm = {
    .start_addr = 0xfe4a0770,
    .end_addr = 0xfe4a476f,
  },
  .meas_clk_freq_hz = 30000000,
};

stm_hdmirx_platform_port_t stm_hdmirx_port_data[] =
{
  {
    .id = 0,
    .csm_port_id = {1, 1, 1},
    .pd_config = {
      .option = STM_HDMIRX_POWER_DETECT_OPTION_GPIO,
      .pin.pd_pio = stm_gpio(STIH416_GPIO(40), 4),
    },
    .hpd_pio = stm_gpio(STIH416_GPIO(40), 3),
    .scl_pio = stm_gpio(STIH416_GPIO(40), 6),
    .sda_pio = stm_gpio(STIH416_GPIO(40), 5),
    .route_connectivity_mask = 0x3,
    .internal_edid = FALSE,
    .enable_hpd = TRUE,
    .enable_ddc2bi = FALSE,
    .ext_mux = FALSE,
    .eq_mode = STM_HDMIRX_ROUTE_EQ_MODE_MEDIUM_GAIN,
    .eq_config = {
      .low_freq_gain = 0x7,
      .high_freq_gain = 0x0,
      .rterm_val = 0x8,
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
