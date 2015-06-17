/*
 * MAROONA Platform registration
 *
 * Copyright (C) 2010 Wyplay SAS
 * Author: Marek Skuczynski <mskuczynski@wyplay.com>
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 */
#ifndef _MAROONA_H_
#define _MAROONA_H_

#include <linux/stm/stm-frontend.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/stm/pio.h>

#include <drivers/media/common/tuners/tda18271.h>
#include <../../media/dvb/frontends/tda10048.h>

/* tuners */

static struct tda18271_config tda18271_master_config = {
	.role			= TDA18271_MASTER,
	.gate			= TDA18271_GATE_DIGITAL,
	.output_opt		= TDA18271_OUTPUT_LT_XT_ON,
	.small_i2c		= TDA18271_39_BYTE_CHUNK_INIT,
	.rf_cal_on_startup	= 0,
};

static struct tda18271_config tda18271_slave_config = {
	.role			= TDA18271_SLAVE,
	.gate			= TDA18271_GATE_DIGITAL,
	.output_opt		= TDA18271_OUTPUT_LT_OFF,
	.small_i2c		= TDA18271_39_BYTE_CHUNK_INIT,
	.rf_cal_on_startup	= 0,
};

/* demodulators */

static const struct tda10048_config tda10048_master_config = {
	.demod_address 		= 0x08,
	.output_mode		= TDA10048_SERIAL_OUTPUT,
	.inversion		= TDA10048_INVERSION_OFF,
	.clk_freq_khz		= TDA10048_CLK_16000,
	.dtv6_if_freq_khz	= TDA10048_IF_3300,
	.dtv7_if_freq_khz	= TDA10048_IF_3500,
	.dtv8_if_freq_khz	= TDA10048_IF_4000,
};

static const struct tda10048_config tda10048_slave_config = {
	.demod_address		= 0x09,
	.output_mode		= TDA10048_SERIAL_OUTPUT,
	.inversion		= TDA10048_INVERSION_OFF,
	.clk_freq_khz		= TDA10048_CLK_16000,
	.dtv6_if_freq_khz	= TDA10048_IF_3300,
	.dtv7_if_freq_khz	= TDA10048_IF_3500,
	.dtv8_if_freq_khz	= TDA10048_IF_4000,
};


static struct dvb_frontend* tda18271_master_attach(void *config, struct dvb_frontend *fe, struct i2c_adapter *i2c) {
	struct dvb_frontend * new_fe = NULL;

	printk("DVB-T: TUNER: TDA18271 - MASTER\n");

	new_fe = tda18271_attach(fe, ((struct fe_config *)config)->tuner_address, i2c, &tda18271_master_config);
	if (new_fe == NULL) {
		printk(KERN_ERR"DVB-T: TUNER: TDA18271 MASTER: not attached!!!\n");
	} 

	return new_fe;
}

static struct dvb_frontend* tda18271_slave_attach(void *config, struct dvb_frontend *fe, struct i2c_adapter *i2c) {
	struct dvb_frontend * new_fe = NULL;

	printk("DVB-T: TUNER: TDA18271 - SLAVE\n");

	new_fe = tda18271_attach(fe, ((struct fe_config *)config)->tuner_address, i2c, &tda18271_slave_config);
	if (new_fe == NULL) {
		printk(KERN_ERR"DVB-T: TUNER: TDA18271 SLAVE: not attached!!!\n");
	} 

	return new_fe;
}


static struct dvb_frontend* tda10048_master_attach(void *config, struct i2c_adapter *i2c)
{
	struct dvb_frontend * fe = NULL;

	printk("DVB-T: DEMOD: TDA10048 - MASTER\n");

	fe = tda10048_attach(&tda10048_master_config, i2c);
	if (fe == NULL) {
		printk(KERN_ERR"DVB-T: DEMOD: TDA10048 MASTER: not attached!!!\n");
	} 
	return fe;
}

static struct dvb_frontend* tda10048_slave_attach(void *config, struct i2c_adapter *i2c)
{
	struct dvb_frontend * fe = NULL;

	printk("DVB-T: DEMOD: TDA10048 - SLAVE\n");

	fe = tda10048_attach(&tda10048_slave_config, i2c);
	if (fe == NULL) {
		printk(KERN_ERR"DVB-T: DEMOD: TDA10048 SLAVE: not attached!!!\n");
	} 
	return fe;
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
			.nr_channels      = 2,
			.channels         = (struct plat_frontend_channel[]) {
				    { /* MASTER */
					.option         = STM_TSM_CHANNEL_3 | STM_SERIAL_NOT_PARALLEL | STM_PACKET_CLOCK_VALID,
					.config         = (struct fe_config[]) {
					    {
						.demod_address = 0x08,
						.tuner_id      = DVB_PLL_THOMSON_DTT759X,
						.tuner_address = 0x60
					    }
					},
					.tuner_attach   = tda18271_master_attach,
					.tuner_i2c_bus  = 2,
					.demod_attach   = tda10048_master_attach,
					.demod_i2c_bus  = 2,
					.pio_reset_bank = 11,
					.pio_reset_pin  = 3,
					.pio_reset_n    = 1,
					.lock = 1,
					.drop = 1,
				    },
				    { /* SLAVE */
					.option         = STM_TSM_CHANNEL_0 | STM_SERIAL_NOT_PARALLEL | STM_PACKET_CLOCK_VALID,
					.config         = (struct fe_config[]) {
					    {
						.demod_address = 0x09,
						.tuner_id      = DVB_PLL_THOMSON_DTT759X,
						.tuner_address = 0x63
					    }
					},
					.tuner_attach   = tda18271_slave_attach,
					.tuner_i2c_bus  = 2,
					.demod_attach   = tda10048_slave_attach,
					.demod_i2c_bus  = 2,
					.pio_reset_bank = 11,
					.pio_reset_pin  = 4,
					.pio_reset_n    = 1,
					.lock = 1,
					.drop = 1,
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
		.start = 7,
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

static struct platform_device *board_maroona[] __initdata = {
	&tsm_device,
	&pti1_device,
	&pti2_device,
};

#define BOARD_SPECIFIC_CONFIG
static __init int register_board_drivers(void)
{
    return platform_add_devices(board_maroona, ARRAY_SIZE(board_maroona));
}

#endif /* _MAROONA_H_ */
