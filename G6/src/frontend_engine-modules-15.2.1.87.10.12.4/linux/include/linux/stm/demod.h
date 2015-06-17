/************************************************************************
Copyright (C) 2011 STMicroelectronics. All Rights Reserved.

This file is part of the stm_fe Library.

stm_fe is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License version 2 as published by the
Free Software Foundation.

stm_fe is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with stm_fe; see the file COPYING.  If not, write to the Free Software
Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

The stm_fe Library may alternatively be licensed under a proprietary
license from ST.

Source file name : demod.h
Author :           Rahul.V

Configuration data types for a demodulator device

Date        Modification                                    Name
----        ------------                                    --------
16-Jun-11   Created                                         Rahul.V

************************************************************************/
#ifndef _DEMOD_H
#define _DEMOD_H

#define STM_FE_DEMOD_NAME "stm_fe_demod"


enum demod_vglna_type_e {
	DEMOD_VGLNA_STVVGLNA
};

enum demod_i2c_route_e {
	DEMOD_IO_DIRECT,
	DEMOD_IO_REPEATER
};

enum demod_repeater_bus_e {
	DEMOD_REPEATER_BUS_1,
	DEMOD_REPEATER_BUS_2
};

enum demod_io_type_e {
	DEMOD_IO_I2C,
	DEMOD_IO_MEMORY_MAPPED
};

enum demod_clk_type_e {
	DEMOD_CLK_DEMOD_CLK_IN,
	DEMOD_CLK_DEMOD_CLK_XTAL
};

enum demod_roll_off_e {
	DEMOD_ROLL_OFF_DEFAULT = 0,
	DEMOD_ROLL_OFF_0_20 = (1 << 0),
	DEMOD_ROLL_OFF_0_25 = (1 << 1),
	DEMOD_ROLL_OFF_0_35 = (1 << 2)
};

enum demod_tsout_config_e {
	DEMOD_TS_SERIAL_PUNCT_CLOCK = (1 << 0),
	DEMOD_TS_SERIAL_CONT_CLOCK = (1 << 1),
	DEMOD_TS_PARALLEL_PUNCT_CLOCK = (1 << 2),
	DEMOD_TS_DVBCI_CLOCK = (1 << 3),
	DEMOD_TS_MANUAL_SPEED = (1 << 4),
	DEMOD_TS_AUTO_SPEED = (1 << 5),
	DEMOD_TS_RISINGEDGE_CLOCK = (1 << 6),
	DEMOD_TS_FALLINGEDGE_CLOCK = (1 << 7),
	DEMOD_TS_SYNCBYTE_ON = (1 << 8),
	DEMOD_TS_SYNCBYTE_OFF = (1 << 9),
	DEMOD_TS_PARITYBYTES_ON = (1 << 10),
	DEMOD_TS_PARITYBYTES_OFF = (1 << 11),
	DEMOD_TS_SWAP_ON = (1 << 12),
	DEMOD_TS_SWAP_OFF = (1 << 13),
	DEMOD_TS_SMOOTHER_ON = (1 << 14),
	DEMOD_TS_SMOOTHER_OFF = (1 << 15),
	DEMOD_TS_PORT_0 = (1 << 16),
	DEMOD_TS_PORT_1 = (1 << 17),
	DEMOD_TS_PORT_2 = (1 << 18),
	DEMOD_TS_PORT_3 = (1 << 19),
	DEMOD_TS_MUX = (1 << 20),
	DEMOD_ADC_IN_A = (1 << 21),
	DEMOD_ADC_IN_B = (1 << 22),
	DEMOD_ADC_IN_C = (1 << 23),
	DEMOD_TS_THROUGH_CABLE_CARD = (1 << 24),
	DEMOD_TS_MERGED = (1 << 25),
	DEMOD_TS_TIMER_TAG = (1 << 26),
	DEMOD_TS_DEFAULT =
	    (DEMOD_TS_SERIAL_CONT_CLOCK | DEMOD_TS_AUTO_SPEED |
	     DEMOD_TS_RISINGEDGE_CLOCK | DEMOD_TS_SYNCBYTE_ON |
	     DEMOD_TS_PARITYBYTES_OFF | DEMOD_TS_SWAP_OFF |
	     DEMOD_TS_SMOOTHER_OFF)
};

enum demod_customisations_e {
	DEMOD_CUSTOM_NONE = 0,
	DEMOD_CUSTOM_USE_AUTO_TUNER = (1 << 0),
	DEMOD_CUSTOM_IQ_WIRING_INVERTED = (1 << 1),
	DEMOD_CUSTOM_HIGH_SYMBOL_RATE_OPTIMISED = (1 << 2),
	DEMOD_CUSTOM_ADD_ON_VGLNA = (1 << 3)
};

enum demod_lla_type_e {
	DEMOD_LLA_FORCED,
	DEMOD_LLA_AUTO
};

struct demod_io_config_s {
	enum demod_io_type_e io;
	enum demod_i2c_route_e route;
	/* char name[32]; */
	uint32_t bus;
	uint32_t baud;
	uint32_t address;
};

struct demod_clk_config_s {
	uint32_t tuner_clk;

	enum demod_clk_type_e clk_type;
	uint32_t demod_clk;
	uint32_t tuner_clkout_divider;
};

#define VGLNA_NAME_MAX_LEN 32
struct demod_vglna_config_s {
	char name[VGLNA_NAME_MAX_LEN];
	enum demod_vglna_type_e type;
	struct demod_io_config_s vglna_io;
	enum demod_repeater_bus_e rep_bus;
};

#define DEMOD_NAME_MAX_LEN 32
#define DEMOD_RESET_PIO_NONE 0xFFFFFFFF
#define ETH_NAME_MAX_LEN 10
struct demod_config_s {
	char demod_name[DEMOD_NAME_MAX_LEN];
	struct demod_io_config_s demod_io;
	char demod_lla[DEMOD_NAME_MAX_LEN];
	enum demod_lla_type_e demod_lla_sel;
	char tuner_name[DEMOD_NAME_MAX_LEN];
	struct demod_io_config_s tuner_io;
	char tuner_lla[DEMOD_NAME_MAX_LEN];
	enum demod_lla_type_e tuner_lla_sel;
	uint32_t ctrl;

	struct demod_clk_config_s clk_config;

	enum demod_tsout_config_e ts_out;

	uint32_t demux_tsin_id;

	/* to be set only if DEMOD_TS_MANUAL_SPEED is set in ts_out */
	uint32_t ts_clock;

	/* The tag value to be added to the stream */
	uint32_t ts_tag;

	enum demod_customisations_e custom_flags;

	uint32_t tuner_if;

	/* To be set only for DVB-S2 demodulator */
	enum demod_roll_off_e roll_off;

	/*PIO line connected to demodulator reset*/
	uint32_t reset_pio;

	/* to be set only if DEMOD_CUSTOM_ADD_ON_VGLNA is set in custom_flags */
	struct demod_vglna_config_s vglna_config;

	uint32_t remote_ip;
	uint32_t remote_port;
	char eth_name[ETH_NAME_MAX_LEN];
#ifdef CONFIG_ARCH_STI
	struct pinctrl *pinctrl;
	struct pinctrl_state *pins_default;
	struct regmap *regmap_ts_map;
	/* Sysconf register offset needed to configure the tsin pio map */
	uint32_t syscfg_ts_map;
	struct resource *res;
	unsigned int ts_map_mask;
	unsigned int ts_map_val;
#else
	struct stm_pad_config *pad_cfg;
	struct stm_device_config *device_config;
	struct stm_pad_state *pad_state;
#endif
};

#endif /* _DEMOD_H */
