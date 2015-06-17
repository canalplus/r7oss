/************************************************************************
Copyright (C) 2011 STMicroelectronics. All Rights Reserved.

This file is part of the STLinuxTV Library.

STLinuxTV is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License version 2 as published by the
Free Software Foundation.

STLinuxTV is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with STLinuxTV; see the file COPYING.  If not, write to the Free Software
Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

The STLinuxTV Library may alternatively be licensed under a proprietary
license from ST.

Source file name : dvb_tuner.c
Author :           Rahul.V

Specific tuner device attach functions

Date        Modification                                    Name
----        ------------                                    --------
16-Jun-11   Created                                         Rahul.V

************************************************************************/
#include "dvb_tuner.h"
#include <stv6110x.h>
#include <stv090x.h>
#include <dvb-pll.h>

#include "dvb_util.h"
#include <frontend/dvb_stm_fe.h>

static void *dvb_stv6110x_attach(struct stm_dvb_demod_s *dvb_demod)
{
	struct stv6110x_devctl *devctl = NULL;
	struct stv6110x_config *config;

	config = kzalloc(sizeof(struct stv6110x_config), GFP_KERNEL);
	if (config == NULL)
		return NULL;

	config->addr = dvb_demod->demod_config->tuner_io.address;
	config->refclk = dvb_demod->demod_config->clk_config.tuner_clk;

	devctl = dvb_attach(stv6110x_attach,
			    dvb_demod->demod, config, dvb_demod->tuner_i2c);
	if (devctl) {
		struct stv090x_config *config = dvb_demod->dvb_demod_config;
		config->tuner_init = devctl->tuner_init;
		config->tuner_set_mode = devctl->tuner_set_mode;
		config->tuner_set_frequency = devctl->tuner_set_frequency;
		config->tuner_get_frequency = devctl->tuner_get_frequency;
		config->tuner_set_bandwidth = devctl->tuner_set_bandwidth;
		config->tuner_get_bandwidth = devctl->tuner_get_bandwidth;
		config->tuner_set_bbgain = devctl->tuner_set_bbgain;
		config->tuner_get_bbgain = devctl->tuner_get_bbgain;
		config->tuner_set_refclk = devctl->tuner_set_refclk;
		config->tuner_get_status = devctl->tuner_get_status;

	}
	dvb_demod->dvb_tuner_config = config;
	return devctl;
}

static void *dvb_dvb_pll_attach(struct stm_dvb_demod_s *dvb_demod,
				unsigned int type)
{
	return dvb_attach(dvb_pll_attach,
			  dvb_demod->demod,
			  dvb_demod->demod_config->tuner_io.address,
			  dvb_demod->tuner_i2c, DVB_PLL_THOMSON_DTT7546X);
}

int tuner_attach(struct stm_dvb_demod_s *dvb_demod)
{
	int ret = 0;
	char *name = dvb_demod->demod_config->tuner_name;
	int config = 0;

	if (!name || !strlen(name)) {
		printk(KERN_NOTICE "No tuner field in dtb for demod %s",
			dvb_demod->demod_config->demod_name);
		return ret;
	}

	if (ATTACH_STM_FE & set_conf(name, NULL, &config))
		return 0;
	else if (DEVICE_MATCH & set_conf(name, "STV6110", &config)) {
		if (!dvb_stv6110x_attach(dvb_demod))
			ret = 1;
	} else if (DEVICE_MATCH & set_conf(name, "DTT7546", &config)) {
		if (!dvb_dvb_pll_attach(dvb_demod, DVB_PLL_THOMSON_DTT7546X))
			ret = 1;
	} else
		printk(KERN_ERR "Unknown tuner device: %s\n", name);

	return ret;
}
