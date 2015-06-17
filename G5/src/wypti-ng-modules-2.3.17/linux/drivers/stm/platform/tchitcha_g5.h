/*
 * tchitcha_g5 Platform registration
 *
 * Copyright (C) 2009 STMicroelectronics Limited
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 */

#ifndef _TCHITCHA_G5_H
#define _TCHITCHA_G5_H

#include <linux/platform_device.h>
#include <linux/i2c.h>
#include <linux/dvb/frontend.h>
#include <linux/stm/stm-frontend.h>

#include "stv6110x.h"
#include "stv090x.h"   /* depends on stv6110x.h */
#include "lnbh24.h"

static struct stv090x_config stv090x_config = {
	.device                 = STV0900,
	.demod_mode             = STV090x_DUAL,
	.clk_mode               = STV090x_CLK_EXT,

	.xtal                   = 16000000,

	.ts1_mode               = STV090x_TSMODE_SERIAL_CONTINUOUS,
	.ts2_mode               = STV090x_TSMODE_SERIAL_CONTINUOUS,

	.repeater_level         = STV090x_RPTLEVEL_16,

	/* select modulated mode (vs envelope mode) for the signals output on
	 * STV0900:DISEQCOUTx pins and fed to the respective LNBH24L:EXTM-x
	 * analogic input pins, which take the complete, modulated DiSEqC
	 * signals */
	.diseqc_envelope_mode   = false,

	.tuner_init             = NULL,
	.tuner_set_mode         = NULL,
	.tuner_set_frequency    = NULL,
	.tuner_get_frequency    = NULL,
	.tuner_set_bandwidth    = NULL,
	.tuner_get_bandwidth    = NULL,
	.tuner_set_bbgain       = NULL,
	.tuner_get_bbgain       = NULL,
	.tuner_set_refclk       = NULL,
	.tuner_get_status       = NULL,
};

/* both tuners are assigned the same HW I2C address
 * therefore they can share this single config */
static struct stv6110x_config stv6110x_config = {
	.refclk                 = 16000000,
};

extern void *stv090x_fe_state;

static struct dvb_frontend* demod_900_attach(void *config, struct i2c_adapter *i2c)
{
	struct dvb_frontend* ret_value;
	const struct fe_config* const fe_cfg = (struct fe_config *)config;
	const enum stv090x_demodulator section = fe_cfg->demod_section == 1 ?
		STV090x_DEMODULATOR_1 : STV090x_DEMODULATOR_0;

	stv090x_config.address = fe_cfg->demod_address;
	ret_value = stv090x_attach(&stv090x_config, i2c, section);
	stv090x_fe_state = ret_value->demodulator_priv;
	return ret_value;
}

static struct dvb_frontend* tuner_stv6110_attach(void *config, struct dvb_frontend *fe, struct i2c_adapter *i2c)
{
	const struct fe_config* const fe_cfg = (struct fe_config *)config;
	struct stv6110x_devctl *fe2 = NULL;

	stv6110x_config.addr = fe_cfg->tuner_address;
	fe2 = stv6110x_attach(fe, &stv6110x_config, i2c);

	stv090x_config.tuner_init          = fe2->tuner_init;
	stv090x_config.tuner_set_mode      = fe2->tuner_set_mode;
	stv090x_config.tuner_set_frequency = fe2->tuner_set_frequency;
	stv090x_config.tuner_get_frequency = fe2->tuner_get_frequency;
	stv090x_config.tuner_set_bandwidth = fe2->tuner_set_bandwidth;
	stv090x_config.tuner_get_bandwidth = fe2->tuner_get_bandwidth;
	stv090x_config.tuner_set_bbgain    = fe2->tuner_set_bbgain;
	stv090x_config.tuner_get_bbgain    = fe2->tuner_get_bbgain;
	stv090x_config.tuner_set_refclk    = fe2->tuner_set_refclk;
	stv090x_config.tuner_get_status    = fe2->tuner_get_status;

	printk(KERN_DEBUG "%s: %p %p\n", __FUNCTION__, fe2->tuner_init, fe2->tuner_set_mode);

	return fe;
}

static struct dvb_frontend *lnb_lnbh24_attach(void *config, struct dvb_frontend *fe, struct i2c_adapter *i2c)
{
	const struct fe_config* const fe_cfg = (struct fe_config *)config;

	/* 22kHz tone is input on EXTM-x pins, hence TTX-x, DSQIN-x and TEN-x
	 * controls shall remain inactive */
	return lnbh24_attach(fe, i2c, 0, LNBH24_TTX | LNBH24_TEN, fe_cfg->lnb_address);
}

static struct platform_device pti1_device = {
	.name = "stm-pti",
	.id = 0,
	.num_resources = 1,

	.resource = (struct resource[]) {
		{
			.start = 0xfe230000,
			.end = 0xfe240000 - 1,
			.flags = IORESOURCE_MEM,
		}
	},

	.dev = {
		.platform_data = (struct plat_frontend_config[]) {
			{
				.nr_channels = 1,
				.channels = (struct plat_frontend_channel[]) {
					{
						/* demodulator 2 is wired to STi7105:TSin1 */
						.option = STM_TSM_CHANNEL_1 | STM_PACKET_CLOCK_VALID | STM_SERIAL_NOT_PARALLEL,
						.config = (struct fe_config[]) {
							{
								.demod_address = (0xd4 >> 1),
								.demod_section = 1,	/* demodulator 2 = "ENTREE SAT" port */
								.tuner_address = (0xc0 >> 1),
								.tuner_id = DVB_PLL_THOMSON_DTT759X,
								.lnb_address = (0x10 >> 1),	/* LNBR-A = "ENTREE SAT" port */
							}
						},

						.demod_attach = demod_900_attach,
						.demod_i2c_bus = 0,	/* i2c adapter 0 */
						.tuner_attach = tuner_stv6110_attach,
						.tuner_i2c_bus = 0,	/* i2c adapter 0 */
						.lnb_attach = lnb_lnbh24_attach,
						.lnb_i2c_bus = 0,	/* i2c adapter 0 */

						.lock = 1,
						.drop = 1,

						.pio_reset_bank = 0x0,	/* rely on board init */
						.pio_reset_pin = 0x0
					},
				}
			}
		}
	}
};

static struct platform_device pti2_device = {
	.name = "stm-pti",
	.id = 1,
	.num_resources = 1,

	.resource = (struct resource[]) {
		{
			.start = 0xfe260000,
			.end   = 0xfe270000 - 1,
			.flags = IORESOURCE_MEM,
		}
	},

	.dev = {
		.platform_data = (struct plat_frontend_config[]) {
			{
				.nr_channels = 1,
				.channels = (struct plat_frontend_channel[]) {
					{
						/* demodulator 1 is wired to STi7105:TSin0 */
						.option = STM_TSM_CHANNEL_0 | STM_PACKET_CLOCK_VALID | STM_SERIAL_NOT_PARALLEL,
						.config = (struct fe_config[]) {
							{
								.demod_address = (0xd4 >> 1),
								.demod_section = 0,	/* demodulator 1 = "SAT AUX" port */
								.tuner_address = (0xc0 >> 1),
								.tuner_id = DVB_PLL_THOMSON_DTT759X,
								.lnb_address = (0x14 >> 1),	/* LNBR-B = "SAT AUX" port */
							}
						},

						.demod_attach = demod_900_attach,
						.demod_i2c_bus = 0,	/* i2c adapter 0 */
						.tuner_attach = tuner_stv6110_attach,
						.tuner_i2c_bus = 0,	/* i2c adapter 0 */
						.lnb_attach = lnb_lnbh24_attach,
						.lnb_i2c_bus = 0,	/* i2c adapter 0 */

						.lock = 1,
						.drop = 1,

						.pio_reset_bank = 0x0,	/* rely on board init */
						.pio_reset_pin = 0x0
					},
				}
			}
		}
	}
};

static struct platform_device tsm_device = {
	.name = "stm-tsm",
	.id = -1,

	.num_resources = 3,
	.resource = (struct resource[]) {
		{	/* TSM Config Registers */
			.start = 0xfe242000,
			.end = 0xfe243000 - 1,
			.flags = IORESOURCE_MEM,
		},
		{	/* SWTS Data Injection Registers */
			.start = 0xfe900000,
			.end = 0xfe901000 - 1,
			.flags = IORESOURCE_MEM,
		},
		{	/* FDMA Request Line */
			.start = 7,	/* could be 11 */
			.end = 7,
			.flags = IORESOURCE_DMA,
		}
	},

	.dev = {
		.platform_data = (struct plat_tsm_config[]) {
			{
				.tsm_sram_buffer_size = 0x0c00,
				.tsm_num_channels = 8,
				.tsm_swts_channel = 4,
				.tsm_num_pti_alt_out = 1,
				.tsm_num_1394_alt_out = 1,
			}
		}
	}
};

static struct platform_device *board_tchitcha_g5[] __initdata = {
	&tsm_device,
	&pti1_device,
	&pti2_device,
};

#define BOARD_SPECIFIC_CONFIG

static int register_board_drivers(void)
{
	return platform_add_devices(board_tchitcha_g5, sizeof(board_tchitcha_g5) / sizeof(struct platform_device *));
}

#endif	/* _TCHITCHA_G5_H */
