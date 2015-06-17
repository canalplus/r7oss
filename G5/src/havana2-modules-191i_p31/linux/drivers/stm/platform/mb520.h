/*
 * Player2 Platform registration
 *
 * Copyright (C) 2006 STMicroelectronics Limited
 * Author: Peter Bennett <peter.bennett@st.com>
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 */

#ifdef CONFIG_SH_ST_MB520

#include <asm-sh/irq-ilc.h>
#include <linux/stm/sysconf.h>

#if 0
static struct plat_frontend_channel frontend_channels[] = {
  { STM_TSM_CHANNEL_0 | STM_DEMOD_AUTO | STM_TUNER_AUTO | STM_DISEQC_STONCHIP | STM_LNB_LNBH221 | STM_SERIAL_NOT_PARALLEL,
    .demod_i2c_bus     = 0xff,
    .demod_i2c_address = 0xff, /* Auto detect address */
    .demod_i2c_ad01    = 0x1,

    .tuner_i2c_bus     = 0xff,
    .tuner_i2c_address = 0xff,

    .lnb_i2c_bus       = 0xff,
    .lnb_i2c_address   = 0x0b,

    .lock = 0,
    .drop = 0,

    .pio_reset_bank    = 0x1,
    .pio_reset_pin     = 0x4
  }};

static struct plat_frontend_config frontend_config = {
	.diseqc_address       = 0xfd068000,//0x18068000,
	.diseqc_irq           = ILC_IRQ(121),//0x81,
	.diseqc_pio_bank      = 2,	//5,
	.diseqc_pio_pin       = 3,	//5,

	.tsm_base_address     = 0,//0x19242000,

	.tsm_sram_buffer_size = 0,//0xc00,
	.tsm_num_pti_alt_out  = 0,//1,
	.tsm_num_1394_alt_out = 0,//1,

  .nr_channels          = 1,
  .channels             = frontend_channels
};

static struct resource  fei_resource[] = {
	{
  .start                = 0xfdb00000,
  .end                  = 0xfdb0ffff,
  .flags                = IORESOURCE_MEM,
	},
	{
  .start                = 0xfec00000,
  .end                  = 0xfec0bfff,
  .flags                = IORESOURCE_MEM,
	},
};

static struct platform_device frontend_device = {
  .name    = "fei",
  .id      = -1,
  .dev          = {
    .platform_data      = &frontend_config,
  },

  .num_resources        = 2,
  .resource             = fei_resource,
};

static struct platform_device *board_mb519[] __initdata = {
        &frontend_device,
};
#endif

#define BOARD_SPECIFIC_CONFIG
static int __init register_board_drivers(void)
{	
  return 0;//platform_add_devices(board_mb519,sizeof(board_mb519)/sizeof(struct platform_device*));
}
#endif
