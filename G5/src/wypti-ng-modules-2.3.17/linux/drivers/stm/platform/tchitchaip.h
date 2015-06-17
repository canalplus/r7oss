/*
 * tchitchaip Platform registration
 *
 * Copyright (C) 2014 Wyplay
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 */
#ifndef __TCHITNT_H
#define __TCHITNT_H

#ifdef CONFIG_SH_ST_C275

#include <linux/stm/stm-frontend.h>

static struct platform_device pti1_device = {
	.name    = "stm-pti",
	.id      = 0,
	.num_resources        = 1,

	.resource             = (struct resource[]) {
            {
                .start = 0xfe230000,
                .end   = 0xfe240000 - 1,
                .flags = IORESOURCE_MEM,
            }
	},

	.dev          = {
            .platform_data      = (struct plat_frontend_config[]) {
            {
                .nr_channels      = 0,
                .channels         = (struct plat_frontend_channel[]) {
                }
            }
        }
    }
};


static struct platform_device tsm_device = {
  .name    = "stm-tsm",
  .id      = -1,

  .num_resources        = 3,
  .resource             = (struct resource[]) {
    { /* TSM Config Registers */
      .start                = 0xfe242000,
      .end                  = 0xfe243000 - 1,
      .flags                = IORESOURCE_MEM,
    },
    { /* SWTS Data Injection Registers */
      .start                = 0xfe900000,
      .end                  = 0xfe901000 - 1,
      .flags                = IORESOURCE_MEM,
    },  
    { /* FDMA Request Line */
      .start = 7, /* could be 11*/
      .end   = 7,
      .flags = IORESOURCE_DMA,
    }
  },

  .dev          = {
    .platform_data      = (struct plat_tsm_config[]) {
      {
	.tsm_sram_buffer_size = 0x0c00,
	.tsm_num_channels = 8,
	.tsm_swts_channel = 4,
	.tsm_num_pti_alt_out  = 1,
	.tsm_num_1394_alt_out = 1,
      }
    }
  }
};

static struct platform_device *board_tchitchaip[] __initdata = {
	&tsm_device,
	&pti1_device,
};

#define BOARD_SPECIFIC_CONFIG
static int register_board_drivers(void)
{
    return platform_add_devices(board_tchitchaip,
			sizeof(board_tchitchaip) / sizeof(struct platform_device*));
}

#endif

#endif /* __TCHITNT_H */
