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

Source file name : dvb_demod.c
Author :           Rahul.V

Specific demodulator device attach functions

Date        Modification                                    Name
----        ------------                                    --------
16-Jun-11   Created                                         Rahul.V

 ************************************************************************/
#include "dvb_demod.h"
#include "dvb_tuner.h"
#include <stv6110x.h>
#include <stv090x.h>
#include <stv0367.h>
#include <stv0367_priv.h>
#include <dvb_dummy_fe.h>
#include "dvb_util.h"
#include <linux/version.h>

#include <frontend/dvb_stm_fe.h>

static enum stv090x_tsmode get_stv090x_tsconfig(enum demod_tsout_config_e
						ts_out)
{
	switch (ts_out) {
	case DEMOD_TS_SERIAL_PUNCT_CLOCK:
		return STV090x_TSMODE_SERIAL_PUNCTURED;
	case DEMOD_TS_SERIAL_CONT_CLOCK:
		return STV090x_TSMODE_SERIAL_CONTINUOUS;
	case DEMOD_TS_PARALLEL_PUNCT_CLOCK:
		return STV090x_TSMODE_PARALLEL_PUNCTURED;
	case DEMOD_TS_DVBCI_CLOCK:
		return STV090x_TSMODE_DVBCI;
	default:
		return STV090x_TSMODE_SERIAL_CONTINUOUS;
	}
}

static struct dvb_frontend *dvb_stv090x_attach(struct stm_dvb_demod_s
					       *dvb_demod)
{
	struct stv090x_config *config;
	struct dvb_frontend *dvb_frontend = NULL;

	config = kzalloc(sizeof(struct stv090x_config), GFP_KERNEL);
	if (!config) {
		printk(KERN_ERR "%s(): Unable to allocate %d bytes\n", __func__,
		       sizeof(struct stv090x_config));
		goto stv090x_attach_done;
	}

	config->device = STV0903;
	config->demod_mode = STV090x_SINGLE;
	if (dvb_demod->demod_config->clk_config.clk_type ==
	    DEMOD_CLK_DEMOD_CLK_XTAL)
		config->clk_mode = STV090x_CLK_EXT;
	else
		config->clk_mode = STV090x_CLK_INT;
	config->xtal = dvb_demod->demod_config->clk_config.demod_clk;
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 34))
	config->ref_clk = dvb_demod->demod_config->clk_config.demod_clk;
#endif
	config->address = dvb_demod->demod_config->demod_io.address;
	config->ts1_mode =
	    get_stv090x_tsconfig(dvb_demod->demod_config->ts_out);
	config->ts2_mode =
	    get_stv090x_tsconfig(dvb_demod->demod_config->ts_out);
	config->repeater_level = STV090x_RPTLEVEL_64;

	dvb_frontend = dvb_attach(stv090x_attach,
				  config, dvb_demod->demod_i2c,
				  STV090x_DEMODULATOR_0);
	if (!dvb_frontend) {
		printk(KERN_ERR "%s(): Unable to attach to %s\n", __func__,
		       dvb_demod->demod_config->demod_name);
		goto stv090x_attach_failed;
	}

	printk(KERN_INFO "%s(): Attached %s ...\n", __func__,
	       dvb_frontend->ops.info.name);

	dvb_demod->dvb_demod_config = config;
	goto stv090x_attach_done;

stv090x_attach_failed:
	kfree(config);
stv090x_attach_done:
	return dvb_frontend;
}

static struct dvb_frontend *dvb_stv0367ter_attach(struct stm_dvb_demod_s
						  *dvb_demod)
{
	struct stv0367_config *config;
	struct dvb_frontend *dvb_frontend = NULL;

	config = kzalloc(sizeof(struct stv0367_config), GFP_KERNEL);
	if (!config) {
		printk(KERN_ERR "%s(): Unable to allocate %d bytes\n", __func__,
		       sizeof(sizeof(struct stv0367_config)));
		goto stv0367ter_attach_done;
	}

	config->demod_address = dvb_demod->demod_config->demod_io.address;
	config->xtal = dvb_demod->demod_config->clk_config.demod_clk;
	config->if_khz = dvb_demod->demod_config->tuner_if;

	if (dvb_demod->demod_config->custom_flags
					    == DEMOD_CUSTOM_IQ_WIRING_INVERTED)
		config->if_iq_mode = FE_TER_IQ_TUNER;
	else
		config->if_iq_mode = FE_TER_NORMAL_IF_TUNER;

	if (dvb_demod->demod_config->ts_out & DEMOD_TS_PARALLEL_PUNCT_CLOCK)
		config->ts_mode = STV0367_PARALLEL_PUNCT_CLOCK;
	else
		config->ts_mode = STV0367_SERIAL_PUNCT_CLOCK;

	/* The following change is temporary, it needs to be changed once the
	 * code in kernel/frontend_engine driver is done which is under
	 * investigation. */
	if (dvb_demod->demod_config->ts_out & DEMOD_TS_RISINGEDGE_CLOCK)
		config->clk_pol = STV0367_FALLINGEDGE_CLOCK;
	else
		config->clk_pol = STV0367_RISINGEDGE_CLOCK;

	dvb_demod->dvb_demod_config = config;

	dvb_frontend = dvb_attach(stv0367ter_attach,
				  config, dvb_demod->demod_i2c);

	if (!dvb_frontend) {
		printk(KERN_ERR "%s(): Unable to attach to %s\n", __func__,
		       dvb_demod->demod_config->demod_name);
		goto stv0367ter_attach_failed;
	}

	dvb_frontend->ops.init(dvb_frontend);
	printk(KERN_INFO "%s(): Attached %s ...\n", __func__,
	       dvb_frontend->ops.info.name);

	goto stv0367ter_attach_done;

stv0367ter_attach_failed:
	kfree(config);
stv0367ter_attach_done:
	return dvb_frontend;
}

static struct dvb_frontend *dvb_dummy_qpsk_attach(struct stm_dvb_demod_s
						  *dvb_demod)
{
	struct dvb_frontend *dvb_frontend;

	dvb_frontend = dvb_attach(dvb_dummy_fe_qpsk_attach);
	if (!dvb_frontend) {
		printk(KERN_ERR "%s(): Unable to attach to %s\n", __func__,
		       dvb_demod->demod_config->demod_name);
		goto qpsk_attach_failed;
	}

	printk(KERN_INFO "%s(): Attached %s ...\n", __func__,
	       dvb_frontend->ops.info.name);

qpsk_attach_failed:
	return dvb_frontend;
}

static struct dvb_frontend *dvb_dummy_ofdm_attach(struct stm_dvb_demod_s
						  *dvb_demod)
{
	struct dvb_frontend *dvb_frontend;

	dvb_frontend = dvb_attach(dvb_dummy_fe_ofdm_attach);
	if (!dvb_frontend) {
		printk(KERN_ERR "%s(): Unable to attach to %s\n", __func__,
		       dvb_demod->demod_config->demod_name);
		BUG();
		goto ofdm_attach_failed;
	}

	printk(KERN_INFO "%s(): Attached %s ...\n", __func__,
	       dvb_frontend->ops.info.name);

ofdm_attach_failed:
	return dvb_frontend;
}

static struct dvb_frontend *dvb_dummy_qam_attach(struct stm_dvb_demod_s
						 *dvb_demod)
{
	struct dvb_frontend *dvb_frontend;

	dvb_frontend = dvb_attach(dvb_dummy_fe_qam_attach);
	if (!dvb_frontend) {
		printk(KERN_ERR "%s(): Unable to attach to %s\n", __func__,
		       dvb_demod->demod_config->demod_name);
		goto qam_attach_failed;
	}

	printk(KERN_INFO "%s(): Attached %s ...\n", __func__,
	       dvb_frontend->ops.info.name);

qam_attach_failed:
	return dvb_frontend;
}

struct dvb_frontend *demod_attach(struct stm_dvb_demod_s *dvb_demod)
{
	struct dvb_frontend *dvb_frontend = NULL;
	char *name = dvb_demod->demod_config->demod_name;
	int config = 0;

	if (ATTACH_STM_FE & set_conf(name, NULL, &config)) {
		dvb_frontend = dvb_stm_fe_demod_attach(dvb_demod->demod_config);
		dvb_demod->ctrl_via_stm_fe = true;
		printk(KERN_INFO "%s(): Attached stm_fe driver for %s ...\n",
		       __func__, name);
	} else if (DEVICE_MATCH & set_conf(name, "STV090x", &config))
		dvb_frontend = dvb_stv090x_attach(dvb_demod);
	else if (DEVICE_MATCH & set_conf(name, "STV0367T", &config))
		dvb_frontend = dvb_stv0367ter_attach(dvb_demod);
	else if (DEVICE_MATCH & set_conf(name, "DUMMY", &config))
		dvb_frontend = dvb_dummy_ofdm_attach(dvb_demod);
	else if (DEVICE_MATCH & set_conf(name, "DUMMY_QPSK", &config))
		dvb_frontend = dvb_dummy_qpsk_attach(dvb_demod);
	else if (DEVICE_MATCH & set_conf(name, "DUMMY_OFDM", &config))
		dvb_frontend = dvb_dummy_ofdm_attach(dvb_demod);
	else if (DEVICE_MATCH & set_conf(name, "DUMMY_QAM", &config))
		dvb_frontend = dvb_dummy_qam_attach(dvb_demod);
	else
		printk(KERN_ERR "%s(): Unknown demodulator device: %s\n",
		       __func__, name);

	return dvb_frontend;
}

int demod_detach(struct stm_dvb_demod_s *dvb_demod)
{
	int ret = 0;
	char *name = NULL;

	if (!dvb_demod->demod_config) {
		printk(KERN_ERR "%s: dvb_demod does not contain valid info\n",
								      __func__);
		return -EINVAL;
	}

	name = dvb_demod->demod_config->demod_name;

	if (dvb_demod->ctrl_via_stm_fe == true) {
		ret = dvb_stm_fe_demod_detach(dvb_demod->demod);
		if (!ret)
			printk(KERN_INFO "%s: Detached stm_fe driver for %s\n",
								__func__, name);
	}

	return ret;
}
