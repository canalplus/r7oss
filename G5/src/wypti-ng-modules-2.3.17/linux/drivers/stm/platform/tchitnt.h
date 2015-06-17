/*
 * Dorade Platform registration
 *
 * Copyright (C) 2009 Wyplay
 * Author: Laurent Fazio <lfazio@wyplay.com>
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 */
#ifndef __TCHITNT_H
#define __TCHITNT_H

#ifdef CONFIG_SH_ST_C275

#include <linux/stm/stm-frontend.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/stm/pio.h>

#include <../../media/dvb/frontends/tda10048.h>
#include <linux/dvb/tda18219/tda18219.h>

static struct dvb_frontend* demod_tda10048_attach(struct fe_config *config, struct i2c_adapter *i2c) {
	struct dvb_frontend * new_fe = NULL;
	struct tda10048_config *tmm = (struct tda10048_config *)kmalloc(sizeof(struct tda10048_config), GFP_KERNEL);

	printk("DVB-T: DEMOD: TDA10048\n");

	memset(tmm, 0, sizeof(struct tda10048_config));
	
	tmm->dtv6_if_freq_khz = TDA10048_IF_3300;

	if(config->demod_section == 0)
		tmm->dtv7_if_freq_khz = TDA10048_IF_3550;
	else if(config->demod_section == 1)
		tmm->dtv7_if_freq_khz = TDA10048_IF_3450;
	else
		tmm->dtv7_if_freq_khz = TDA10048_IF_3500;

	if(config->demod_section == 0)
		tmm->dtv8_if_freq_khz = TDA10048_IF_4050;
	else if(config->demod_section == 1)
		tmm->dtv8_if_freq_khz = TDA10048_IF_3950;
	else
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

static struct dvb_frontend* tuner_tda18219_attach(struct fe_config *config, struct dvb_frontend *fe, struct i2c_adapter *i2c) {
	struct dvb_frontend * new_fe = NULL;
    struct tda18219_config *tda18219_dvb_config = (struct tda10048_config *)kmalloc(sizeof(struct tda18219_config), GFP_KERNEL);

    tda18219_dvb_config->i2c_address = config->tuner_address;
	tda18219_dvb_config->id =  config->demod_section;
	
    printk("DVB-T: TUNER: TDA18219\n");

	new_fe = tda18219_attach(fe, i2c, tda18219_dvb_config);
	if (new_fe == NULL) {
		printk(KERN_ERR"DVB-T: TUNER: TDA18219: not attached!!!\n");
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
                                .demod_address = 0x09,
								.demod_section = 0,
                                .tuner_id      = 0,
                                .tuner_address = 0x60
                            }
                        },

                        .demod_attach   = demod_tda10048_attach,
                        .demod_i2c_bus  = 0,

                        .tuner_attach   = tuner_tda18219_attach,
                        .tuner_i2c_bus  = 0,

                        .lock = 1,
                        .drop = 1,

                        .pio_reset_bank = 0x06,
                        .pio_reset_pin  = 0x07,
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
                                .demod_address = 0x08,
								.demod_section = 1,
                                .tuner_id      = 0,
                                .tuner_address = 0x63
                            }
                        },

                        .demod_attach   = demod_tda10048_attach,
                        .demod_i2c_bus  = 0,

                        .tuner_attach   = tuner_tda18219_attach,
                        .tuner_i2c_bus  = 0,

                        .lock = 1,
                        .drop = 1,

                        .pio_reset_bank = 0x7,
                        .pio_reset_pin  = 0x2,
                        .pio_reset_n = 1,
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
	.tsm_sram_buffer_size = 0x0c00,
	.tsm_num_channels = 8,
	.tsm_swts_channel = 4,
	.tsm_num_pti_alt_out  = 1,
	.tsm_num_1394_alt_out = 1,
      }
    }
  }
};

static struct platform_device *board_dorade[] __initdata = {
	&tsm_device,
    &pti1_device,
    &pti2_device,
};

#define BOARD_SPECIFIC_CONFIG
static int register_board_drivers(void)
{
    return platform_add_devices(board_dorade, 
				sizeof(board_dorade) / sizeof(struct platform_device*));
}

#endif

#endif /* __TCHITNT_H */
