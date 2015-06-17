/*
 * Player2 Platform registration
 *
 * Copyright (C) 2006 STMicroelectronics Limited
 * Author: Peter Bennett <peter.bennett@st.com>
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 */
#ifndef _MB442_H_
#define _MB442_H_


#ifdef CONFIG_SH_ST_MB442

#if 0
static struct plat_frontend_channel frontend_channels[] = {
  { STM_TSM_CHANNEL_0 | STM_DEMOD_ALL | STM_TUNER_AUTO | STM_DISEQC_STONCHIP | STM_LNB_LNBH221 | STM_SERIAL_NOT_PARALLEL,
    .demod_i2c_bus     = 0,
    .demod_i2c_address = 0xff, /* Auto detect address */
    .demod_i2c_ad01    = 0x0,

    .tuner_i2c_bus     = 0,
    .tuner_i2c_address = 0xff,

    .lnb_i2c_bus       = 0,
    .lnb_i2c_address   = 0x0b,

    .lock = 0,
    .drop = 0,

    .pio_reset_bank    = 0x2,
    .pio_reset_pin     = 0x5
  },

  { STM_TSM_CHANNEL_1 | STM_DEMOD_360 | STM_TUNER_AUTO | STM_LNB_LNBH221 | STM_SERIAL_NOT_PARALLEL,
    .demod_i2c_bus     = 0,
    .demod_i2c_address = 0xff, /* Auto detect address */
    .demod_i2c_ad01    = 0x1,

    .tuner_i2c_bus     = 0,
    .tuner_i2c_address = 0xff,

    .lnb_i2c_bus       = 0,
    .lnb_i2c_address   = 0x08,

    .lock = 0,
    .drop = 0,

    .pio_reset_bank    = 0x2,
    .pio_reset_pin     = 0x5
  }
};

static struct plat_frontend_config frontend_config = {
  .diseqc_address       = 0x18068000,
  .diseqc_irq           = 0x81,
  .diseqc_pio_bank      = 5,
  .diseqc_pio_pin       = 5,

  .tsm_base_address     = 0x19242000,

  .tsm_sram_buffer_size = 0xc00,
  .tsm_num_pti_alt_out  = 1,
  .tsm_num_1394_alt_out = 1,

  .nr_channels          = 2,
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
#endif

#define BOARD_SPECIFIC_CONFIG
static int register_board_drivers(void)
{
  return 0;//platform_add_devices(board_7109,sizeof(board_7109)/sizeof(struct platform_device*));
}

#endif

#endif /* _HMS1_H_ */
