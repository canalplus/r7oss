/*
 * WyMediabox Platform registration
 *
 * Copyright (C) 2009 Wyplay
 * Author: Laurent Fazio <laurent.fazio@wyplay.com>
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 */
#ifndef _WYMEDIABOX_H
#define _WYMEDIABOX_H

#ifdef CONFIG_SH_WYMDBOX_01

#include <linux/stm/stm-frontend.h>


static struct platform_device pti1_device = {
	.name    = "stm-pti",
	.id      = 0,
	.num_resources        = 1,

	.resource             = (struct resource[]) {
		{
			.start = 0x19230000,
			.end   = 0x19240000 - 1,
			.flags = IORESOURCE_MEM,
		}
	},

	.dev          = {
		.platform_data      = (struct plat_frontend_config[]) {
			{
				.nr_channels      = 1,
				.channels         = (struct plat_frontend_channel[]) {
                    {
						.option         = STM_TSM_CHANNEL_0  | STM_DISEQC_STONCHIP,

						.config         = (struct fe_config[]) {
							{
								.demod_address = 0x1d,
								.tuner_id      = DVB_PLL_THOMSON_DTT759X,
								.tuner_address = 0x63
							}
						},

						.demod_attach   = demod_362_attach,
						.demod_i2c_bus  = 0,

						.tuner_attach   = tuner_attach,
						.tuner_i2c_bus  = 0,

						.lock = 0,
						.drop = 0,

						.pio_reset_bank = 0x02,
						.pio_reset_pin  = 0x05,
						.pio_reset_n    = 1,
					},
                }
			}
		}
	}
};

static struct platform_device pti2_device = {
    .name    = "stm-pti",
    .id      = 1,
    .num_resources        = 1,

    .resource             = (struct resource[]) {
        {
            .start = 0x19260000,
            .end   = 0x19270000 - 1,
            .flags = IORESOURCE_MEM,
        }
    },

    .dev          = {
        .platform_data      = (struct plat_frontend_config[]) {
            {
                .nr_channels      = 1,
                .channels         = (struct plat_frontend_channel[]) {
                    {
 {
                        .option         = STM_TSM_CHANNEL_2 | STM_DISEQC_STONCHIP,

                        .config         = (struct fe_config[]) {
                            {
                                .demod_address = 0x1c,
                                .tuner_id      = DVB_PLL_THOMSON_DTT759X,
                                .tuner_address = 0x60,
                            }
                        },

                        .demod_attach   = demod_362_attach,
                        .demod_i2c_bus  = 0,

                        .tuner_attach   = tuner_attach,
                        .tuner_i2c_bus  = 0,

                        .lock = 0,
                        .drop = 0,

                        .pio_reset_bank = 0x02,
                        .pio_reset_pin  = 0x04,
                        .pio_reset_n    = 1,
                    },
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
            .start                = 0x19242000,
            .end                  = 0x19243000 - 1,
            .flags                = IORESOURCE_MEM,
        },
        { /* SWTS Data Injection Registers */
            .start                = 0x1A300000,
            .end                  = 0x1A301000 - 1,
            .flags                = IORESOURCE_MEM,
        },  
        { /* FDMA Request Line */
            .start = 28,
            .end   = 28,
            .flags = IORESOURCE_DMA,
        }
    },

    .dev          = {
        .platform_data      = (struct plat_tsm_config[]) {
            {
                .tsm_sram_buffer_size = 0xc00,
                .tsm_swts_channel = 3,
                .tsm_num_pti_alt_out  = 1,
                .tsm_num_1394_alt_out = 1,
            }
        }
    }
};


static struct platform_device *board_wymdbox01[] __initdata = {
	&tsm_device,
    &pti1_device,
    &pti2_device,
};

#define BOARD_SPECIFIC_CONFIG
static int register_board_drivers(void)
{
	return platform_add_devices(board_wymdbox01,sizeof(board_wymdbox01)/sizeof(struct platform_device*));
}

#endif /* ! CONFIG_SH_WYMDBOX_01 */

#endif /* ! _WYMEDIABOX_H */
