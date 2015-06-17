/*
 * Bostona Platform registration
 *
 * Copyright (C) 2009 Wyplay
 * Author: Laurent Fazio <lfazio@wyplay.com>
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 */
#ifndef __BOSTONA_H
#define __BOSTONA_H

#ifdef CONFIG_SH_BOSTONA

#include <linux/stm/stm-frontend.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/stm/pio.h>

static struct dvb_frontend* bostona_tda10048_attach(struct fe_config *config, struct i2c_adapter *i2c) {
	struct dvb_frontend * new_fe = NULL;
	struct tda10048_config *tmm = (struct tda10048_config *)kmalloc(sizeof(struct tda10048_config), GFP_KERNEL);

	printk("DVB-T: DEMOD: TDA10048\n");

	memset(tmm, 0, sizeof(struct tda10048_config));
	tmm->dtv6_if_freq_khz = TDA10048_IF_3300;
	tmm->dtv7_if_freq_khz = TDA10048_IF_3500;
	tmm->dtv8_if_freq_khz = TDA10048_IF_4000;
	tmm->clk_freq_khz     = TDA10048_CLK_16000;
	tmm->demod_address    = config->demod_address;
	tmm->inversion        = TDA10048_INVERSION_OFF;
	tmm->output_mode      = TDA10048_SERIAL_OUTPUT;

	new_fe = tda10048_attach(tmm, i2c);
	if (new_fe == NULL){
		printk(KERN_ERR"DVB-T: DEMOD: TDA10048: not attached!!!\n");
	}

	return new_fe;
}

static struct dvb_frontend* bostona_tda18218_attach(struct fe_config *config, struct dvb_frontend *fe, struct i2c_adapter *i2c) {
	struct dvb_frontend * new_fe = NULL;
	struct tda18218_config tda18218_dvb_config = {
		.role      = TDA18218_MASTER,
		.lt        = TDA18218_WITH_LT,
		.config    = 0,
		.small_i2c = 0,
	};

	printk("DVB-T: TUNER: TDA18218\n");

	new_fe = tda18218_attach(fe, config->tuner_address, i2c, &tda18218_dvb_config);
	if (new_fe == NULL) {
		printk(KERN_ERR"DVB-T: TUNER: TDA18218: not attached!!!\n");
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
                        .option         = STM_TSM_CHANNEL_0  | STM_SERIAL_NOT_PARALLEL | STM_PACKET_CLOCK_VALID,

                        .config         = (struct fe_config[]) {
                            {
                                .demod_address = 0x10,
                                .tuner_id      = 0,
                                .tuner_address = 0xC0,
                            }
                        },

                        .demod_attach   = bostona_tda10048_attach,
                        .demod_i2c_bus  = 0,

                        .tuner_attach   = bostona_tda18218_attach,
                        .tuner_i2c_bus  = 0,

                        .lock = 1,
                        .drop = 1,

                        .pio_reset_bank = 0x16,
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
                        .option         = STM_TSM_CHANNEL_1  | STM_SERIAL_NOT_PARALLEL | STM_PACKET_CLOCK_VALID,

                        .config         = (struct fe_config[]) {
                            {
                                .demod_address = 0x12,
                                .tuner_id      = 0,
                                .tuner_address = 0xC6,
                            }
                        },

                        .demod_attach   = bostona_tda10048_attach,
                        .demod_i2c_bus  = 0,

                        .tuner_attach   = bostona_tda18218_attach,
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
                .nr_channels      = 0,
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

static struct platform_device *board_bostona[] __initdata = {
	&tsm_device,
    &pti1_device,
    &pti2_device,
};

#define BOARD_SPECIFIC_CONFIG
static int register_board_drivers(void)
{
    return platform_add_devices(board_bostona,
                                sizeof(board_bostona) / sizeof(struct platform_device*));
}

#endif

#endif /* __BOSTONA_H */
