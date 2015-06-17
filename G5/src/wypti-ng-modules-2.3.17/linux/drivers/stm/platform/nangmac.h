/*
 * Nangmac Platform registration
 *
 * Copyright (C) 2010 Wyplay
 * Author: Laurent Fazio <lfazio@wyplay.com>
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 */
#ifndef __NANGMAC_H
#define __NANGMAC_H

#ifdef CONFIG_SH_NANGMAC

#include <linux/stm/stm-frontend.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/stm/pio.h>

#include "mxl101sf/mxl101sf.h"

static struct dvb_frontend* demod_mxl101sf_master_attach(struct fe_config *config, struct i2c_adapter *i2c)
{
	struct dvb_frontend * new_fe = NULL;
    struct mxl101sf_config mxl101sf_dvb_config = {
		.tuner_i2c           = i2c,
		.tuner_address       = config->tuner_address,
		.type                = 0, /* Master */
        .serial_not_parallel = 0,
    };

	printk("DVB-T: DEMOD: MXL101SF (master)\n");
	printk("DVB-T: TUNER: MXL101SF (master)\n");

    new_fe = mxl101sf_attach(&mxl101sf_dvb_config, i2c, config->demod_address);
    if (new_fe == NULL) {
		printk(KERN_ERR"DVB-T: FRONTEND: MXL101SF: not attached!!!\n");
	}

    return new_fe;
}

static struct dvb_frontend* demod_mxl101sf_slave_attach(struct fe_config *config, struct i2c_adapter *i2c)
{
	struct dvb_frontend * new_fe = NULL;
    struct mxl101sf_config mxl101sf_dvb_config = {
		.tuner_i2c           = i2c,
		.tuner_address       = config->tuner_address,
		.type                = 1, /* Slave */
        .serial_not_parallel = 0,
    };

	printk("DVB-T: DEMOD: MXL101SF (slave)\n");
	printk("DVB-T: TUNER: MXL101SF (slave)\n");

    new_fe = mxl101sf_attach(&mxl101sf_dvb_config, i2c, config->demod_address);
    if (new_fe == NULL) {
		printk(KERN_ERR"DVB-T: FRONTEND: MXL101SF: not attached!!!\n");
	}

    return new_fe;
}


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
                .nr_channels      = 1,
                .channels         = (struct plat_frontend_channel[]) {
                    {
                        .option         = STM_TSM_CHANNEL_0,

                        .config         = (struct fe_config[]) {
                            {
                                .demod_address = 0x60,
                                .tuner_id      = 0,
                                .tuner_address = 0x00,
                            }
                        },

                        .demod_attach   = demod_mxl101sf_master_attach,
                        .demod_i2c_bus  = 2,

                        .tuner_attach   = NULL,
                        .tuner_i2c_bus  = 0,

                        .lock = 1,
                        .drop = 1,

                        .pio_reset_bank = 0x0b,
                        .pio_reset_pin  = 0x02,
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
            .start = 0xfe260000,
            .end   = 0xfe270000 - 1,
            .flags = IORESOURCE_MEM,
        }
    },

    .dev          = {
        .platform_data      = (struct plat_frontend_config[]) {
            {
                .nr_channels      = 1,
                .channels         = (struct plat_frontend_channel[]) {
                    {
                        .option         = STM_TSM_CHANNEL_1,

                        .config         = (struct fe_config[]) {
                            {
                                .demod_address = 0x63,
                                .tuner_id      = 0,
                                .tuner_address = 0x00,
                            }
                        },

                        .demod_attach   = demod_mxl101sf_slave_attach,
                        .demod_i2c_bus  = 2,

                        .tuner_attach   = NULL,
                        .tuner_i2c_bus  = 0,

                        .lock = 1,
                        .drop = 1,

                        .pio_reset_bank = 0x00,
                        .pio_reset_pin  = 0x00,
                        .pio_reset_n    = 0,
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
                .tsm_sram_buffer_size = 0x1000,
                .tsm_num_channels = 8,
                .tsm_swts_channel = 4,
                .tsm_num_pti_alt_out  = 1,
                .tsm_num_1394_alt_out = 1,
            }
        }
    }
};


static struct platform_device *board_nangmac[] __initdata = {
	&tsm_device,
    &pti1_device,
    &pti2_device,
};

#define BOARD_SPECIFIC_CONFIG
static int register_board_drivers(void)
{
    return platform_add_devices(board_nangmac,
                                sizeof(board_nangmac) / sizeof(struct platform_device*));
}

#endif

#endif /* __NANGMAC_H */
