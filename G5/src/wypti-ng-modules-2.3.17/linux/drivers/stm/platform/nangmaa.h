/*
 * Nangmaa Platform registration
 *
 * Copyright (C) 2010 Wyplay
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 */
#ifndef __NANGMAA_H
#define __NANGMAA_H

#include <linux/stm/stm-frontend.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/stm/pio.h>
#include "../../media/dvb/frontends/intel/mxl5007-ce6353.h"

#define DVBFE_NONE	0xFF

/**
 * demod_set_adapter - Just stores a pointer on the dmod adapater, to pass it
 * to tuner_mxl5007_demod_ce6353_attach later.
 * @config Channel configuration.
 * @i2c I2C adapter for demod.
 */
static struct dvb_frontend* demod_set_adapter(void *config, struct i2c_adapter* i2c);

/**
 * tuner_mxl5007_demod_ce6353_attach - Tuner and demod attach routine. Most of
 * the work is done here.
 * @config Channel configuration.
 * @fe DVB front end. 
 * @i2c I2C adapter for tuner.
 */
static struct dvb_frontend* tuner_mxl5007_demod_ce6353_attach(void *config,
							      struct dvb_frontend *fe,
							      struct i2c_adapter *i2c);

static struct i2c_adapter* demod_adap = NULL;

static struct platform_device pti_device = {
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
                        .option         = STM_TSM_CHANNEL_1,

                        .config         = (struct fe_config[]) {
                            {
                                .demod_address = 0x0d,
                                .tuner_id      = 0,
                                .tuner_address = 0x60,
                            }
                        },

                        .demod_attach   = demod_set_adapter,
                        .demod_i2c_bus  = 2,

                        .tuner_attach   = tuner_mxl5007_demod_ce6353_attach,
                        .tuner_i2c_bus  = 2,

                        .lock = 1,
                        .drop = 1,
						
                        .pio_reset_bank = 0x0b,
                        .pio_reset_pin  = 0x02,
			.pio_reset_n = 1
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


static struct platform_device *board_nangmaa[] __initdata = {
	&tsm_device,
	&pti_device,
};

#define BOARD_SPECIFIC_CONFIG
static int register_board_drivers(void)
{
	return platform_add_devices(board_nangmaa, sizeof(board_nangmaa) /
				    sizeof(struct platform_device*));
}

static struct dvb_frontend* tuner_mxl5007_demod_ce6353_attach(void *config,
							      struct dvb_frontend *fe,
							      struct i2c_adapter *i2c)
{
	struct dvb_frontend *new_fe;
	struct intel_config *fe_config;
	
	if ((config == NULL) || (demod_adap == NULL) || (fe != (struct dvb_frontend *)DVBFE_NONE)) {
		printk(KERN_ERR "%s EINVAL\n", __FUNCTION__);
		return NULL;
	}
	fe_config = (struct intel_config*) kmalloc(sizeof(struct intel_config), GFP_KERNEL);
	if (fe_config == NULL) {
		printk(KERN_ERR "%s Out of memory\n", __FUNCTION__);
		return NULL;
	}
	fe_config->tuner_address = ((struct fe_config *)config)->tuner_address,
	fe_config->tuner_i2c = i2c;
	fe_config->serial_not_parallel = 0;
	tuner_adapter = i2c;
	new_fe = intel_attach(fe_config, demod_adap, ((struct fe_config *)config)->demod_address);
	if (new_fe == NULL) {
		printk(KERN_ERR"DVB-T: DEMOD CE6353 TUNER MXL5007T: not attached!!!\n");
	}
	return new_fe;
}

static struct dvb_frontend* demod_set_adapter(void *config, struct i2c_adapter* i2c)
{
	demod_adap = i2c;
	// do nothing with the demod for the moment
	return (struct dvb_frontend*)DVBFE_NONE;
}

#endif /* __NANGMAA_H */

