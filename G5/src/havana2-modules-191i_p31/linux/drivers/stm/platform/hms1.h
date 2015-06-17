/*
 * Player2 Platform registration
 *
 * Copyright (C) 2006 STMicroelectronics Limited
 * Author: Peter Bennett <peter.bennett@st.com>
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 */
#ifndef _HMS1_H_
#define _HMS1_H_


#ifdef CONFIG_SH_ST_HMS1_no

static struct plat_frontend_channel frontend_channels[] = {
  { STM_TSM_CHANNEL_0 | STM_DISEQC_STONCHIP | STM_SERIAL_NOT_PARALLEL,
    .demod_attach      = stv0360_attach,
    .demod_config      = NULL,
    .demod_i2c_bus     = 1,

    .tuner_id          = 0,
    .tuner_i2c_bus     = 1,
    .tuner_i2c_address = 0xff,

    .lnb_i2c_bus       = 1,
    .lnb_i2c_address   = 0x0b,

    .lock = 0,
    .drop = 0,

    .pio_reset_bank    = 0x1,
    .pio_reset_pin     = 0x4
  }};

static struct plat_frontend_config frontend_config = {
  .diseqc_address       = 0x18068000,
  .diseqc_irq           = 0x81,
  .diseqc_pio_bank      = 5,
  .diseqc_pio_pin       = 5,

  .tsm_base_address     = 0x19242000,

  .tsm_sram_buffer_size = 0xc00,
  .tsm_num_pti_alt_out  = 1,
  .tsm_num_1394_alt_out = 1,

  .nr_channels          = 1,
  .channels             = frontend_channels
};

static struct resource  pti_resource = {
  .start                = 0x19230000,
  .end                  = 0x19240000 - 1,
  .flags                = IORESOURCE_MEM,
};

static struct platform_device frontend_device = {
  .name    = "pti",
  .id      = -1,
  .dev          = {
    .platform_data      = &frontend_config,
  },

  .num_resources        = 1,
  .resource             = &pti_resource,
};

static struct platform_device *board_7109[] __initdata = {
        &frontend_device,
};

#define BOARD_SPECIFIC_CONFIG
static int register_board_drivers(void)
{
	return platform_add_devices(board_7109,sizeof(board_7109)/sizeof(struct platform_device*));
}

#endif

#endif /* _HMS1_H_ */
