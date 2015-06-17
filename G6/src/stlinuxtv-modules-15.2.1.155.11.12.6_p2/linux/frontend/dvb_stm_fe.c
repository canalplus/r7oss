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

Source file name : dvb_stm_fe.c
Author :           Rahul.V

Wrapper over stm_fe for Linux DVB

Date        Modification                                    Name
----        ------------                                    --------
16-Jun-11   Created                                         Rahul.V

 ************************************************************************/
#include <linux/platform_device.h>
#include <linux/stm/demod.h>
#include <linux/stm/ip.h>
#include <linux/stm/ipfec.h>
#include "dvb_stm_fe.h"
#include <dvb/dvb_data.h>

static int dvb_stm_fe_debug;
module_param(dvb_stm_fe_debug, int, 0664);
MODULE_PARM_DESC(dvb_stm_fe_debug, "enable stm_fe debug messages");

#define dprintk(x...) do { if (dvb_stm_fe_debug) printk(x); } while (0)

#define QPSK_STDS (STM_FE_DEMOD_TX_STD_DVBS | STM_FE_DEMOD_TX_STD_DVBS2)
#define QAM_STDS  (STM_FE_DEMOD_TX_STD_DVBC | STM_FE_DEMOD_TX_STD_DVBC2 | STM_FE_DEMOD_TX_STD_J83_AC | STM_FE_DEMOD_TX_STD_J83_B)
#define OFDM_STDS (STM_FE_DEMOD_TX_STD_DVBT | STM_FE_DEMOD_TX_STD_DVBT2 | STM_FE_DEMOD_TX_STD_ISDBT)
#define ATSC_STDS (STM_FE_DEMOD_TX_STD_ATSC)
#define ANALOG_STDS (STM_FE_DEMOD_TX_STD_PAL | STM_FE_DEMOD_TX_STD_SECAM \
                STM_FE_DEMOD_TX_STD_NTSC)

/* The unit of frequency for QPSK is kHz and for other standards it is Hz */
#define DVB_NON_QPSK_FREQ_FACTOR 1000

uint32_t get_stm_fec[12] = {
	STM_FE_DEMOD_FEC_RATE_NONE,	/* FEC_NONE = 0 */
	STM_FE_DEMOD_FEC_RATE_1_2,	/*  FEC_1_2   */
	STM_FE_DEMOD_FEC_RATE_2_3,	/*  FEC_2_3   */
	STM_FE_DEMOD_FEC_RATE_3_4,	/*  FEC_3_4   */
	STM_FE_DEMOD_FEC_RATE_4_5,	/*  FEC_4_5   */
	STM_FE_DEMOD_FEC_RATE_5_6,	/*  FEC_5_6   */
	STM_FE_DEMOD_FEC_RATE_6_7,	/*  FEC_6_7   */
	STM_FE_DEMOD_FEC_RATE_7_8,	/*  FEC_7_8   */
	STM_FE_DEMOD_FEC_RATE_8_9,	/*  FEC_8_9   */
	STM_FE_DEMOD_FEC_RATE_AUTO,	/*  FEC_AUTO   */
	STM_FE_DEMOD_FEC_RATE_3_5,	/*  FEC_3_5   */
	STM_FE_DEMOD_FEC_RATE_9_10	/*  FEC_9_10   */
};

uint32_t get_stm_inv[3] = {
	STM_FE_DEMOD_SPECTRAL_INVERSION_OFF,	/* INVERSION_OFF = 0 */
	STM_FE_DEMOD_SPECTRAL_INVERSION_ON,	/*  INVERSION_ON   */
	STM_FE_DEMOD_SPECTRAL_INVERSION_AUTO	/*  INVERSION_AUTO   */
};

uint32_t get_stm_guard[5] = {
	STM_FE_DEMOD_GUARD_1_32,	/* GUARD_INTERVAL_1_32 */
	STM_FE_DEMOD_GUARD_1_16,	/* GUARD_INTERVAL_1_16   */
	STM_FE_DEMOD_GUARD_1_8,	/* GUARD_INTERVAL_1_8   */
	STM_FE_DEMOD_GUARD_1_4,	/* GUARD_INTERVAL_1_4   */
	STM_FE_DEMOD_GUARD_AUTO	/* GUARD_INTERVAL_AUTO   */
};

uint32_t get_stm_mode[4] = {
	STM_FE_DEMOD_FFT_MODE_2K,	/* TRANSMISSION_MODE_2K */
	STM_FE_DEMOD_FFT_MODE_8K,	/* TRANSMISSION_MODE_8K   */
	STM_FE_DEMOD_FFT_MODE_AUTO,	/* TRANSMISSION_MODE_AUTO   */
	STM_FE_DEMOD_FFT_MODE_4K	/* TRANSMISSION_MODE_4K   */
};

uint32_t get_stm_mod[13] = {
	STM_FE_DEMOD_MODULATION_QPSK,	/* QPSK */
	STM_FE_DEMOD_MODULATION_16QAM,	/* QAM_16   */
	STM_FE_DEMOD_MODULATION_32QAM,	/* QAM_32   */
	STM_FE_DEMOD_MODULATION_64QAM,	/* QAM_64   */
	STM_FE_DEMOD_MODULATION_128QAM,	/* QAM_128 */
	STM_FE_DEMOD_MODULATION_256QAM,	/* QAM_256   */
	STM_FE_DEMOD_MODULATION_AUTO,	/* QAM_AUTO   */
	STM_FE_DEMOD_MODULATION_8VSB,	/* VSB_8   */
	STM_FE_DEMOD_MODULATION_16VSB,	/* VSB_16   */
	STM_FE_DEMOD_MODULATION_8PSK,	/* PSK_8 */
	STM_FE_DEMOD_MODULATION_16APSK,	/* APSK_16   */
	STM_FE_DEMOD_MODULATION_32APSK,	/* APSK_32   */
	STM_FE_DEMOD_MODULATION_QPSK	/* DQPSK   */
};

#define GET_DVB_VAL(dvb_val_p, mode, stmlookup) \
{  \
        uint32_t i=0; \
	for (i = 0; i<ARRAY_SIZE(stmlookup); i++) { \
                if(stmlookup[i] == mode)  \
                {  \
                        *dvb_val_p = i;  \
                        break;  \
                }  \
        } \
}

uint32_t get_dvb_mod[17] = {
	QPSK,			/*   STM_FE_DEMOD_MODULATION_NONE/BPSK */
	QPSK,			/*   STM_FE_DEMOD_MODULATION_QPSK   */
	PSK_8,			/*   STM_FE_DEMOD_MODULATION_8PSK   */
	APSK_16,		/*   STM_FE_DEMOD_MODULATION_16APSK   */
	APSK_32,		/*   STM_FE_DEMOD_MODULATION_32APSK   */
	QAM_16,			/*   STM_FE_DEMOD_MODULATION_4QAM   */
	QAM_16,			/*   STM_FE_DEMOD_MODULATION_16QAM   */
	QAM_32,			/*   STM_FE_DEMOD_MODULATION_32QAM   */
	QAM_64,			/*   STM_FE_DEMOD_MODULATION_64QAM   */
	QAM_128,		/*   STM_FE_DEMOD_MODULATION_128QAM   */
	QAM_256,		/*   STM_FE_DEMOD_MODULATION_256QAM   */
	QAM_256,		/*   STM_FE_DEMOD_MODULATION_512QAM   */
	QAM_256,		/*   STM_FE_DEMOD_MODULATION_1024QAM   */
	VSB_8,			/*   STM_FE_DEMOD_MODULATION_2VSB   */
	VSB_8,			/*   STM_FE_DEMOD_MODULATION_4VSB   */
	VSB_8,			/*   STM_FE_DEMOD_MODULATION_8VSB   */
	VSB_16,			/*   STM_FE_DEMOD_MODULATION_16VSB   */
};

uint32_t get_dvb_hier[3] = {
	HIERARCHY_1,		/* STM_FE_DEMOD_HIERARCHY_ALPHA_1   */
	HIERARCHY_2,		/* STM_FE_DEMOD_HIERARCHY_ALPHA_2   */
	HIERARCHY_4		/* STM_FE_DEMOD_HIERARCHY_ALPHA_4   */
};

static uint32_t __attribute_const__ lookup_stm_bw(u32 bandwidth_hz)
{
	static const uint32_t get_stm_bw[] = {
		0,		/* AUTO */
		STM_FE_DEMOD_BW_1_7,	/* 1.712MHz */
		0,		/* unsupported */
		0,		/* unsupported */
		STM_FE_DEMOD_BW_5_0,	/* 5MHz */
		STM_FE_DEMOD_BW_6_0,	/* 6Mhz */
		STM_FE_DEMOD_BW_7_0,	/* 7MHz */
		STM_FE_DEMOD_BW_8_0,	/* 8Mhz */
		0,		/* unsupported   */
		STM_FE_DEMOD_BW_10_0	/* 10MHz */
	};

	int quantized_bandwidth = bandwidth_hz >> 20;

	if (quantized_bandwidth >= ARRAY_SIZE(get_stm_bw))
		quantized_bandwidth = 0;

	return get_stm_bw[quantized_bandwidth];
}

static uint32_t __attribute_const__ lookup_bandwidth_hz(uint32_t bandwidth)
{
	switch (bandwidth) {
	case STM_FE_DEMOD_BW_1_7:
		return 1712000;
	case STM_FE_DEMOD_BW_5_0:
		return 5000000;
	case STM_FE_DEMOD_BW_6_0:
		return 6000000;
	case STM_FE_DEMOD_BW_7_0:
		return 7000000;
	case STM_FE_DEMOD_BW_8_0:
		return 8000000;
	case STM_FE_DEMOD_BW_10_0:
		return 10000000;
	}

	return 0;
}

/**
 * stm_dvb_fe_release() - Callback from dvb_frontend_detach()
 */
static void stm_dvb_fe_release(struct dvb_frontend *fe)
{
	struct dvb_stm_fe_demod_state *state;
	int ret;

	state = fe->demodulator_priv;
	if (!state)
		return;

	/*
	 * Remove the subscription entries
	 */
	ret = stm_event_subscription_modify(state->ev_subs,
				&state->evt_subs_entry,
				STM_EVENT_SUBSCRIPTION_OP_REMOVE);
	if (ret)
		printk(KERN_ERR "%s(): Failed to remove STFE subscription entries\n", __func__);


	/*
	 * Delete the subscription object
	 */
	ret = stm_event_subscription_delete(state->ev_subs);
	if (ret)
		printk(KERN_ERR "%s(): Failed to delete STFE event subscription\n", __func__);

	fe->demodulator_priv = NULL;
	kfree(state);
}

struct dvb_frontend *dvb_stm_fe_demod_attach(struct demod_config_s
					     *demod_config)
{
	struct dvb_stm_fe_demod_state *state;

	state = kzalloc(sizeof(struct dvb_stm_fe_demod_state), GFP_KERNEL);
	if (state == NULL)
		return NULL;

	state->frontend.ops.release = stm_dvb_fe_release,
	state->frontend.ops.init = dvb_stm_fe_init;
	state->frontend.ops.sleep = dvb_stm_fe_sleep;
	state->frontend.ops.get_frontend_algo = dvb_stm_fe_get_frontend_algo;
	state->frontend.ops.i2c_gate_ctrl = dvb_stm_fe_i2c_gate_ctrl;
	state->frontend.ops.diseqc_send_master_cmd =
	    dvb_stm_fe_diseqc_send_master_cmd;
	state->frontend.ops.diseqc_send_burst = dvb_stm_fe_diseqc_send_burst;
	state->frontend.ops.diseqc_recv_slave_reply =
	    dvb_stm_fe_diseqc_recv_slave_reply;
	state->frontend.ops.set_tone = dvb_stm_fe_set_tone;
	state->frontend.ops.search = dvb_stm_fe_search;
	state->frontend.ops.tune = dvb_stm_fe_tune;
	state->frontend.ops.read_status = dvb_stm_fe_read_status;
	state->frontend.ops.read_ber = dvb_stm_fe_read_ber;
	state->frontend.ops.read_signal_strength =
	    dvb_stm_fe_read_signal_strength;
	state->frontend.ops.read_snr = dvb_stm_fe_read_snr;
	state->frontend.ops.get_frontend = dvb_stm_fe_track;
	state->frontend.ops.read_ucblocks = dvb_stm_fe_read_ucblocks;

	state->demod_config = demod_config;
	state->frontend.demodulator_priv = state;
	return &state->frontend;
}

int dvb_stm_fe_demod_detach(struct dvb_frontend *fe)
{
	struct dvb_stm_fe_demod_state *state;
	if (!fe)
		return -EINVAL;

	state = fe->demodulator_priv;

	if (!state)
		return -EINVAL;

	state->frontend.ops.read_ucblocks = NULL;
	state->frontend.ops.get_frontend = NULL;
	state->frontend.ops.read_snr = NULL;
	state->frontend.ops.read_signal_strength = NULL;
	state->frontend.ops.read_ber = NULL;
	state->frontend.ops.read_status = NULL;
	state->frontend.ops.tune = NULL;
	state->frontend.ops.search = NULL;
	state->frontend.ops.set_tone = NULL;
	state->frontend.ops.diseqc_recv_slave_reply = NULL;
	state->frontend.ops.diseqc_send_burst = NULL;
	state->frontend.ops.diseqc_send_master_cmd = NULL;
	state->frontend.ops.i2c_gate_ctrl = NULL;
	state->frontend.ops.get_frontend_algo = NULL;
	state->frontend.ops.sleep = NULL;
	state->frontend.ops.init = NULL;

	return 0;
}
/**
 * stm_dvb_lnb_release() - Callback from dvb_frontend_detach()
 */
void stm_dvb_lnb_release(struct dvb_frontend *fe)
{
	struct dvb_stm_fe_lnb_state *state;

	state = fe->sec_priv;
	if (!state)
		return;

	/*
	 * Power off the LNB
	 */
	dvb_stm_fe_set_voltage(fe, SEC_VOLTAGE_OFF);

	fe->sec_priv = NULL;
	kfree(state);
}

struct dvb_frontend *dvb_stm_fe_lnb_attach(struct dvb_frontend *fe,
					   struct lnb_config_s *lnb_config)
{
	struct dvb_stm_fe_lnb_state *state =
	    kzalloc(sizeof(struct dvb_stm_fe_lnb_state), GFP_KERNEL);

	if (!state)
		return NULL;

	state->lnb_config = lnb_config;
	fe->sec_priv = state;
	fe->ops.release_sec = stm_dvb_lnb_release;
	fe->ops.set_voltage = dvb_stm_fe_set_voltage;
	fe->ops.enable_high_lnb_voltage = dvb_stm_fe_enable_high_lnb_voltage;

	printk(KERN_INFO "STM_FE LNB attached ...\n");
	return fe;
}

int dvb_stm_fe_lnb_detach(struct dvb_frontend *fe)
{
	struct dvb_stm_fe_lnb_state *state;
	if (!fe)
		return -EINVAL;

	fe->ops.set_voltage = NULL;
	fe->ops.enable_high_lnb_voltage = NULL;
	state = fe->sec_priv;

	if (!state)
		return -EINVAL;

	return 0;
}


struct dvb_frontend *dvb_stm_fe_diseqc_attach(struct dvb_frontend *fe, struct diseqc_config_s
					      *diseqc_config)
{
	struct dvb_stm_fe_demod_state *state = fe->demodulator_priv;
	if (state) {
		state->diseqc_config = diseqc_config;
		printk(KERN_INFO "STM_FE DISEQC attached ...\n");
	} else
		return NULL;
	return fe;
}

int dvb_stm_fe_diseqc_detach(struct dvb_frontend *fe)
{
	struct dvb_stm_fe_demod_state *state;

	if (fe) {
		state = fe->demodulator_priv;
		if (!state)
			return -EINVAL;
	} else
		return -EINVAL;
	state->diseqc_config = NULL;
	return 0;
}

struct dvb_frontend *dvb_stm_fe_rf_matrix_attach(struct dvb_frontend *fe,
				struct rf_matrix_config_s *rf_matrix_config)
{
	struct dvb_stm_fe_demod_state *state;
	if (!fe)
		return NULL;

	state = fe->demodulator_priv;

	if (!state)
		return NULL;
	state->rf_matrix_config = rf_matrix_config;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 3, 0))
	fe->ops.set_rf_input_src = dvb_stm_fe_set_rf_input_src;
#endif
	printk(KERN_INFO "STM_FE RF_MATRIX attached ...\n");

	return fe;
}

int dvb_stm_fe_rf_matrix_detach(struct dvb_frontend *fe)
{
	struct dvb_stm_fe_demod_state *state;

	if (!fe)
		return -EINVAL;

	state = fe->demodulator_priv;

	if (!state)
		return -EINVAL;

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 3, 0))
	fe->ops.set_rf_input_src = NULL;
#endif
	state->rf_matrix_config = NULL;

	return 0;
}

int dvb_stm_ipfe_attach(struct dvb_ipfe **dvb_ipfe, struct ip_config_s *ip_config)
{
	struct dvb_stm_fe_ip_state *state;
	int ret = 0;

	if (!ip_config) {
		ret = -EINVAL;
		goto stm_ipfe_attach_failed;
	}

	state = kzalloc(sizeof(struct dvb_stm_fe_ip_state), GFP_KERNEL);
	if (!state) {
		ret = -ENOMEM;
		goto stm_ipfe_attach_failed;
	}

	state->ipfe.ops.start = dvb_stm_fe_ip_start;
	state->ipfe.ops.fec_start = dvb_stm_fe_ip_fec_start;
	state->ipfe.ops.stop = dvb_stm_fe_ip_stop;
	state->ipfe.ops.fec_stop = dvb_stm_fe_ip_fec_stop;
	state->ipfe.ops.set_control = dvb_stm_fe_ip_set_control;
	state->ipfe.ops.set_fec = dvb_stm_fe_ip_set_fec;
	state->ipfe.ops.get_capability = dvb_stm_fe_ip_get_capability;
	state->ip_config = ip_config;
	*dvb_ipfe = &state->ipfe;


stm_ipfe_attach_failed:
	return ret;
}

void dvb_stm_ipfe_detach(struct dvb_ipfe *dvb_ipfe)
{
	struct dvb_stm_fe_ip_state *state;

	if (!dvb_ipfe)
		goto stm_ipfe_detach_done;

	state = container_of(dvb_ipfe, struct dvb_stm_fe_ip_state, ipfe);

	memset(&state->ipfe, 0, sizeof(struct dvb_ipfe));

	state->ip_config = NULL;
	state->ip_object = NULL;

	kfree(state);

stm_ipfe_detach_done:
	return;
}

int dvb_stm_fe_ipfec_attach(struct dvb_ipfe *ipfe, struct ipfec_config_s *fec_config)
{
	struct dvb_stm_fe_ip_state *state;
	int ret = 0;

	if (!ipfe) {
		ret = -EINVAL;
		goto ipfec_attach_failed;
	}

	state = container_of(ipfe, struct dvb_stm_fe_ip_state, ipfe);
	state->ipfec_config = fec_config;
	printk(KERN_INFO "STM_FE IPFEC attached ...\n");

ipfec_attach_failed:
	return ret;
}

void dvb_stm_fe_ipfec_detach(struct dvb_ipfe *ipfe)
{
	struct dvb_stm_fe_ip_state *state;

	if (!ipfe)
		return;

	state = container_of(ipfe, struct dvb_stm_fe_ip_state, ipfe);
	state->ipfec_config = NULL;
	state->ipfec_object = NULL;
}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,3,0))
static void set_dvb_ops_delsys(stm_fe_demod_tx_std_t stds,
			       struct dvb_frontend_ops *ops)
{
	/* use of temporary copy means we won't overrun ops->delsys which in
	 * linux-3.3 is only 8 long
	 */
	u8 delsys[32] = { 0 };
	int i = 0;

	if (stds & QPSK_STDS) {
		delsys[i++] = SYS_DVBS;
		delsys[i++] = SYS_DVBS2;
		delsys[i++] = SYS_TURBO;
		delsys[i++] = SYS_ISDBS;
		delsys[i++] = SYS_DSS;
	}
	if (stds & OFDM_STDS) {
		delsys[i++] = SYS_DVBT;
		delsys[i++] = SYS_DVBT2;
		delsys[i++] = SYS_ISDBT;
		delsys[i++] = SYS_DMBTH;
	}
	if (stds & QAM_STDS) {
		delsys[i++] = SYS_DVBC_ANNEX_A;
		delsys[i++] = SYS_DVBC_ANNEX_C;
	}
	if (stds & ATSC_STDS) {
		delsys[i++] = SYS_ATSC;
		delsys[i++] = SYS_DVBC_ANNEX_B;
	}

	memcpy(ops->delsys, delsys, sizeof(ops->delsys));
}
#endif

fe_type_t get_dvb_type(stm_fe_demod_tx_std_t stds)
{
	fe_type_t type = 0;

	if (stds & QPSK_STDS)
		type = FE_QPSK;
	/* If demod supports both ofdm and qam only ofdm will prevail.
	   The info structure doesn't take care of multi-standard demodulators. */
	else if (stds & OFDM_STDS)
		type = FE_OFDM;
	else if (stds & QAM_STDS)
		type = FE_QAM;
	else if (stds & ATSC_STDS)
		type = FE_ATSC;
	return type;
}

int dvb_stm_fe_demod_link(struct dvb_frontend *fe, stm_fe_demod_h demod_object,
			  stm_fe_object_info_t * stinfo)
{
	int num_std;
	struct dvb_stm_fe_demod_state *demod_state;

	if (fe) {
		demod_state = fe->demodulator_priv;
		if (!demod_state)
			return -EINVAL;
	} else {
		return -EINVAL;
	}
	demod_state->demod_object = demod_object;
	strncpy(fe->ops.info.name, stinfo->stm_fe_obj, sizeof(fe->ops.info.name));
	demod_state->stinfo = stinfo;

	/* Initialise */
	fe->ops.info.frequency_min = 0xFFFFFFFF;
	fe->ops.info.symbol_rate_min = 0xFFFFFFFF;
	fe->ops.info.caps = FE_IS_STUPID;

	for (num_std = 0; num_std < stinfo->u_caps.demod.num_stds; num_std++) {
		demod_state->supported_stds |=
		    stinfo->u_caps.demod.tune_caps[num_std].std;
		fill_dvb_info(&fe->ops.info,
			      &stinfo->u_caps.demod.tune_caps[num_std],
			      stinfo->u_caps.demod.tune_caps[num_std].std);
	}

	fe->ops.info.type = get_dvb_type(demod_state->supported_stds);
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,3,0))
	set_dvb_ops_delsys(demod_state->supported_stds, &fe->ops);
	fe->dtv_property_cache.delivery_system = fe->ops.delsys[0];
#endif

	return 0;
}

fe_caps_t get_fec_caps(stm_fe_demod_fec_rate_t fecs)
{
	fe_caps_t fec_caps = 0;

	fecs & STM_FE_DEMOD_FEC_RATE_1_2 ? (fec_caps |= FE_CAN_FEC_1_2) :
	    (fec_caps |= 0);
	fecs & STM_FE_DEMOD_FEC_RATE_2_3 ? (fec_caps |= FE_CAN_FEC_2_3) :
	    (fec_caps |= 0);
	fecs & STM_FE_DEMOD_FEC_RATE_3_4 ? (fec_caps |= FE_CAN_FEC_3_4) :
	    (fec_caps |= 0);
	fecs & STM_FE_DEMOD_FEC_RATE_4_5 ? (fec_caps |= FE_CAN_FEC_4_5) :
	    (fec_caps |= 0);
	fecs & STM_FE_DEMOD_FEC_RATE_5_6 ? (fec_caps |= FE_CAN_FEC_5_6) :
	    (fec_caps |= 0);
	fecs & STM_FE_DEMOD_FEC_RATE_6_7 ? (fec_caps |= FE_CAN_FEC_6_7) :
	    (fec_caps |= 0);
	fecs & STM_FE_DEMOD_FEC_RATE_7_8 ? (fec_caps |= FE_CAN_FEC_7_8) :
	    (fec_caps |= 0);
	fecs & STM_FE_DEMOD_FEC_RATE_8_9 ? (fec_caps |= FE_CAN_FEC_8_9) :
	    (fec_caps |= 0);

	return fec_caps;
}

fe_caps_t get_mod_caps(stm_fe_demod_modulation_t mod)
{
	fe_caps_t mod_caps = 0;

	mod & STM_FE_DEMOD_MODULATION_QPSK ? (mod_caps |= FE_CAN_QPSK) :
	    (mod_caps |= 0);
	mod & STM_FE_DEMOD_MODULATION_16QAM ? (mod_caps |= FE_CAN_QAM_16) :
	    (mod_caps |= 0);
    mod & STM_FE_DEMOD_MODULATION_32QAM ? (mod_caps |= FE_CAN_QAM_32) :
	    (mod_caps |= 0);
	mod & STM_FE_DEMOD_MODULATION_64QAM ? (mod_caps |= FE_CAN_QAM_64) :
	    (mod_caps |= 0);
	mod & STM_FE_DEMOD_MODULATION_128QAM ? (mod_caps |= FE_CAN_QAM_128) :
	    (mod_caps |= 0);
	mod & STM_FE_DEMOD_MODULATION_256QAM ? (mod_caps |= FE_CAN_QAM_256) :
	    (mod_caps |= 0);
	mod & STM_FE_DEMOD_MODULATION_8VSB ? (mod_caps |= FE_CAN_8VSB) :
	    (mod_caps |= 0);
	mod & STM_FE_DEMOD_MODULATION_16VSB ? (mod_caps |= FE_CAN_16VSB) :
	    (mod_caps |= 0);

	return mod_caps;
}

fe_caps_t get_auto_caps(stm_fe_demod_feature_t features)
{
	fe_caps_t auto_caps = 0;

	/* Supported in all */
	auto_caps |= FE_CAN_INVERSION_AUTO;

	features & STM_FE_DEMOD_FEATURE_AUTO_FEC ?
	    (auto_caps |= FE_CAN_FEC_AUTO) : (auto_caps |= 0);
	features & STM_FE_DEMOD_FEATURE_AUTO_MODULATION ?
	    (auto_caps |= FE_CAN_QAM_AUTO) : (auto_caps |= 0);
	features & STM_FE_DEMOD_FEATURE_AUTO_GUARD ?
	    (auto_caps |= FE_CAN_GUARD_INTERVAL_AUTO) : (auto_caps |= 0);
	features & STM_FE_DEMOD_FEATURE_AUTO_MODE ?
	    (auto_caps |= FE_CAN_TRANSMISSION_MODE_AUTO) : (auto_caps |= 0);
	features & STM_FE_DEMOD_FEATURE_AUTO_FEC ?
	    (auto_caps |= FE_CAN_FEC_AUTO) : (auto_caps |= 0);
	features & STM_FE_DEMOD_FEATURE_AUTO_FEC ?
	    (auto_caps |= FE_CAN_FEC_AUTO) : (auto_caps |= 0);

	return auto_caps;
}

void fill_dvb_info(struct dvb_frontend_info *dvb_info,
		   stm_fe_demod_tune_caps_t * caps, stm_fe_demod_tx_std_t std)
{
	uint32_t freq_min, freq_max, sr_min = 0, sr_max = 0;
	stm_fe_demod_feature_t features = 0;
	stm_fe_demod_fec_rate_t fecs = 0;
	stm_fe_demod_modulation_t mods = 0;

	switch (std) {
	case STM_FE_DEMOD_TX_STD_DVBS:
	default:
		freq_min = caps->u_tune.dvbs.freq_min;
		freq_max = caps->u_tune.dvbs.freq_max;
		sr_min = caps->u_tune.dvbs.sr_min;
		sr_max = caps->u_tune.dvbs.sr_max;
		fecs = caps->u_tune.dvbs.fecs;
		mods = caps->u_tune.dvbs.mods;
		features = caps->u_tune.dvbs.features;
		break;
	case STM_FE_DEMOD_TX_STD_DVBS2:
		freq_min = caps->u_tune.dvbs2.freq_min;
		freq_max = caps->u_tune.dvbs2.freq_max;
		sr_min = caps->u_tune.dvbs2.sr_min;
		sr_max = caps->u_tune.dvbs2.sr_max;
		fecs = caps->u_tune.dvbs2.fecs;
		mods = caps->u_tune.dvbs2.mods;
		features = caps->u_tune.dvbs2.features;
		break;
	case STM_FE_DEMOD_TX_STD_DVBC:
	case STM_FE_DEMOD_TX_STD_DVBC2:
		freq_min =
		    caps->u_tune.dvbc.freq_min * DVB_NON_QPSK_FREQ_FACTOR;
		freq_max =
		    caps->u_tune.dvbc.freq_max * DVB_NON_QPSK_FREQ_FACTOR;
		sr_min = caps->u_tune.dvbc.sr_min;
		sr_max = caps->u_tune.dvbc.sr_max;
		fecs = caps->u_tune.dvbc.fecs;
		mods = caps->u_tune.dvbc.mods;
		features = caps->u_tune.dvbc.features;
		break;
	case STM_FE_DEMOD_TX_STD_J83_AC:
		freq_min =
		    caps->u_tune.j83ac.freq_min * DVB_NON_QPSK_FREQ_FACTOR;
		freq_max =
		    caps->u_tune.j83ac.freq_max * DVB_NON_QPSK_FREQ_FACTOR;
		sr_min = caps->u_tune.j83ac.sr_min;
		sr_max = caps->u_tune.j83ac.sr_max;
		fecs = caps->u_tune.j83ac.fecs;
		mods = caps->u_tune.j83ac.mods;
		features = caps->u_tune.j83ac.features;
		break;
	case STM_FE_DEMOD_TX_STD_J83_B:
		freq_min =
		    caps->u_tune.j83b.freq_min * DVB_NON_QPSK_FREQ_FACTOR;
		freq_max =
		    caps->u_tune.j83b.freq_max * DVB_NON_QPSK_FREQ_FACTOR;
		sr_min = caps->u_tune.j83b.sr_min;
		sr_max = caps->u_tune.j83b.sr_max;
		fecs = caps->u_tune.j83b.fecs;
		mods = caps->u_tune.j83b.mods;
		features = caps->u_tune.j83b.features;
		break;
	case STM_FE_DEMOD_TX_STD_DVBT:
		freq_min =
		    caps->u_tune.dvbt.freq_min * DVB_NON_QPSK_FREQ_FACTOR;
		freq_max =
		    caps->u_tune.dvbt.freq_max * DVB_NON_QPSK_FREQ_FACTOR;
		fecs = caps->u_tune.dvbt.fecs;
		mods = caps->u_tune.dvbt.mods;
		features = caps->u_tune.dvbt.features;
		break;
	case STM_FE_DEMOD_TX_STD_DVBT2:
		freq_min =
		    caps->u_tune.dvbt2.freq_min * DVB_NON_QPSK_FREQ_FACTOR;
		freq_max =
		    caps->u_tune.dvbt2.freq_max * DVB_NON_QPSK_FREQ_FACTOR;
		fecs = caps->u_tune.dvbt2.fecs;
		mods = caps->u_tune.dvbt2.mods;
		features = caps->u_tune.dvbt2.features;
		break;
	case STM_FE_DEMOD_TX_STD_ATSC:
		freq_min =
		    caps->u_tune.atsc.freq_min * DVB_NON_QPSK_FREQ_FACTOR;
		freq_max =
		    caps->u_tune.atsc.freq_max * DVB_NON_QPSK_FREQ_FACTOR;
		sr_min = caps->u_tune.atsc.sr_min;
		sr_max = caps->u_tune.atsc.sr_max;
		mods = caps->u_tune.atsc.mods;
		features = caps->u_tune.atsc.features;
		break;
	case STM_FE_DEMOD_TX_STD_ISDBT:
		freq_min =
		    caps->u_tune.isdbt.freq_min * DVB_NON_QPSK_FREQ_FACTOR;
		freq_max =
		    caps->u_tune.isdbt.freq_max * DVB_NON_QPSK_FREQ_FACTOR;
		fecs = caps->u_tune.isdbt.fecs;
		mods = caps->u_tune.isdbt.mods;
		features = caps->u_tune.isdbt.features;
		break;
	case STM_FE_DEMOD_TX_STD_PAL:
	case STM_FE_DEMOD_TX_STD_SECAM:
	case STM_FE_DEMOD_TX_STD_NTSC:
		freq_min = caps->u_tune.analog.freq_min;
		freq_max = caps->u_tune.analog.freq_max;
		break;
	}

	dvb_info->caps |= get_fec_caps(fecs);
	dvb_info->caps |= get_mod_caps(mods);
	dvb_info->caps |= get_auto_caps(features);

	if (dvb_info->frequency_max < freq_max)
		dvb_info->frequency_max = freq_max;
	if (dvb_info->frequency_min > freq_min)
		dvb_info->frequency_min = freq_min;
	if (dvb_info->symbol_rate_max < sr_max)
		dvb_info->symbol_rate_max = sr_max;
	if (dvb_info->symbol_rate_min > sr_min)
		dvb_info->symbol_rate_min = sr_min;
}

int dvb_stm_fe_lnb_link(struct dvb_frontend *fe, stm_fe_lnb_h lnb_object)
{
	struct dvb_stm_fe_lnb_state *lnb_state;

	if (fe) {
		lnb_state = fe->sec_priv;
		if (!lnb_state)
			return -EINVAL;
	} else
		return -EINVAL;

	lnb_state->lnb_object = lnb_object;
	return 0;
}

int dvb_stm_fe_diseqc_link(struct dvb_frontend *fe,
			   stm_fe_diseqc_h diseqc_object)
{
	struct dvb_stm_fe_demod_state *demod_state;

	if (fe) {
		demod_state = fe->demodulator_priv;
		if (!demod_state)
			return -EINVAL;
	} else
		return -EINVAL;

	demod_state->diseqc_object = diseqc_object;
	return 0;
}

int dvb_stm_fe_rf_matrix_link(struct dvb_frontend *fe,
			   stm_fe_rf_matrix_h rf_matrix_object,
			   stm_fe_object_info_t *stinfo)
{
	struct dvb_stm_fe_demod_state *demod_state;
	struct dtv_frontend_properties *c;

	if (!fe)
		return -EINVAL;

	demod_state = fe->demodulator_priv;
	if (!demod_state)
		return -EINVAL;

	c = &fe->dtv_property_cache;

	demod_state->rf_matrix_object = rf_matrix_object;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 3, 0))
	c->rf_input_max = stinfo->u_caps.rf_matrix.max_input;
	c->rf_input_src = demod_state->rf_matrix_config->input_id;
#endif
	return 0;
}

int dvb_stm_fe_ip_link(struct dvb_ipfe *ipfe, stm_fe_ip_h ip_object,
		       stm_fe_object_info_t * stinfo)
{
	struct dvb_stm_fe_ip_state *ip_state;
	int ret = 0;

	if (!ipfe) {
		ret = -EINVAL;
		goto fe_ip_link_failed;
	}

	ip_state = container_of(ipfe, struct dvb_stm_fe_ip_state, ipfe);

	ip_state->ip_object = ip_object;
	memset(&ipfe->ops.info, 0, sizeof(struct dvb_frontend_info));
	strncpy(ipfe->ops.info.name, stinfo->stm_fe_obj, sizeof(ipfe->ops.info.name));
	ipfe->ops.info.type = FE_IPFE;
	ipfe->ops.caps.protocol = stinfo->u_caps.ip.protocol;
	ipfe->ops.caps.ec_type = stinfo->u_caps.ip.ec_types;

fe_ip_link_failed:
	return ret;
}

int dvb_stm_fe_ipfec_link(struct dvb_ipfe *ipfe,
			   stm_fe_ip_fec_h fec_object)
{
	struct dvb_stm_fe_ip_state *ipfe_state;
	int ret = 0;

	if (!ipfe) {
		ret = -EINVAL;
		goto ipfec_link_failed;
	}

	ipfe_state = container_of(ipfe, struct dvb_stm_fe_ip_state, ipfe);

	ipfe_state->ipfec_object = fec_object;

ipfec_link_failed:
	return ret;
}

/* Interface functions follows */

int dvb_stm_fe_init(struct dvb_frontend *fe)
{
	struct dvb_stm_fe_demod_state *state;
	int32_t ret;

	if (fe) {
		state = fe->demodulator_priv;
		if (!state)
			return -EINVAL;
	} else
		return -EINVAL;

	dprintk(KERN_DEBUG "%s ...\n", __FUNCTION__);

	if(state->ev_subs)
		goto done;

	state->evt_subs_entry.object = DMD_OBJ(fe);
	state->evt_subs_entry.event_mask =
	    STM_FE_DEMOD_EVENT_TUNE_LOCKED | STM_FE_DEMOD_EVENT_TUNE_UNLOCKED |
	    STM_FE_DEMOD_EVENT_TUNE_FAILED | STM_FE_DEMOD_EVENT_SCAN_LOCKED |
	    STM_FE_DEMOD_EVENT_SCAN_COMPLETE | STM_FE_DEMOD_EVENT_LNB_FAILURE |
	    STM_FE_DEMOD_EVENT_HW_FAILURE;
	state->evt_subs_entry.cookie = NULL;

	ret =
	    stm_event_subscription_create(&state->evt_subs_entry, FRONTEND_EVENTS,
					  &state->ev_subs);

	if (ret)
		printk(KERN_ERR
		       "%s: stm_event_subscription_create() failed ret = 0x%x\n",
		       __FUNCTION__, ret);

done:
	return 0;
}

int dvb_stm_fe_sleep(struct dvb_frontend *fe)
{
	int ret = 0;

	dprintk(KERN_DEBUG "%s ...\n", __FUNCTION__);
	ret = stm_fe_demod_stop(DMD_OBJ(fe));
	return ret;
}

int dvb_stm_fe_write(struct dvb_frontend *fe, u8 * buf, int len)
{
	dprintk(KERN_DEBUG "%s ...\n", __FUNCTION__);

	/* Not supported in KPI */
	return 0;
}

/* if this is set, it overrides the default swzigzag */
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,3,0))
int dvb_stm_fe_tune(struct dvb_frontend *fe,
		    bool re_tune,
		    unsigned int mode_flags,
		    unsigned int *delay, fe_status_t * status)
#else
int dvb_stm_fe_tune(struct dvb_frontend *fe,
		    struct dvb_frontend_parameters *params,
		    unsigned int mode_flags,
		    unsigned int *delay, fe_status_t * status)
#endif
{
	enum dvbfe_search result;

	dprintk(KERN_DEBUG "%s ...\n", __FUNCTION__);

	if (!fe)
		return -EINVAL;

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,3,0))
	if (!re_tune) {
		dvb_stm_fe_read_status(fe, status);
		return 0;
	}
	result = dvb_stm_fe_search(fe);
#else
	if (!params) {
		dvb_stm_fe_read_status(fe, status);
		return 0;
	}

	result = dvb_stm_fe_search(fe, params);
#endif
	if (result == DVBFE_ALGO_SEARCH_SUCCESS)
		*status = FE_HAS_LOCK;
	else
		*status = 0;

	/* To make the kdvb thread run every 1 sec to track the demod status */
	*delay = 1 * HZ;
	return 0;
}

/* get frontend tuning algorithm from the module */
enum dvbfe_algo dvb_stm_fe_get_frontend_algo(struct dvb_frontend *fe)
{
	dprintk(KERN_DEBUG "%s ...\n", __FUNCTION__);
	return DVBFE_ALGO_HW;
}

int dvb_stm_fe_read_status(struct dvb_frontend *fe, fe_status_t * tune_status)
{
	int ret = 0;
	stm_fe_demod_info_t info;
	stm_fe_demod_status_t stm_fe_status;
	uint32_t look_up[3] = {
		FE_HAS_CARRIER,	/* STM_FE_DEMOD_SIGNAL_FOUND */
		0,		/* STM_FE_DEMOD_NO_SIGNAL */
		(FE_HAS_SIGNAL | FE_HAS_CARRIER | FE_HAS_VITERBI | FE_HAS_SYNC |
		 FE_HAS_LOCK)
		    /* STM_FE_DEMOD_SYNC_OK */
	};

	dprintk(KERN_DEBUG "%s ...\n", __FUNCTION__);

	if (!fe)
		return -EINVAL;

	ret = stm_fe_demod_get_info(DMD_OBJ(fe), &info);
	if (ret) {
		return ret;
	}

	DEMOD_CHANNEL_INFO_COMMON_VALUE(info.u_info.tune.demod_info.std,
					info.u_info.tune.demod_info, status,
					&stm_fe_status);
	if (stm_fe_status != STM_FE_DEMOD_STATUS_UNKNOWN)
		*tune_status = look_up[ilog2(stm_fe_status)];
	else
		*tune_status = 0;
	return ret;
}

int dvb_stm_fe_read_ber(struct dvb_frontend *fe, u32 * ber_val)
{
	int ret = 0;
	stm_fe_demod_info_t info;

	dprintk(KERN_DEBUG "%s ...\n", __FUNCTION__);

	if (!fe)
		return -EINVAL;

	ret = stm_fe_demod_get_info(DMD_OBJ(fe), &info);
	if (ret) {
		return ret;
	}
	if (info.u_info.tune.demod_info.std & (STM_FE_DEMOD_TX_STD_PAL |
					       STM_FE_DEMOD_TX_STD_SECAM |
					       STM_FE_DEMOD_TX_STD_NTSC))
		*ber_val = 0;
	if (info.u_info.tune.demod_info.std & STM_FE_DEMOD_TX_STD_DVBS2)
		*ber_val = info.u_info.tune.demod_info.u_channel.dvbs2.per;
	else {
		DEMOD_CHANNEL_INFO_BER_VALUE(info.u_info.tune.demod_info.std,
					     info.u_info.tune.demod_info,
					     ber_val);
	}
	return ret;
}

int dvb_stm_fe_read_signal_strength(struct dvb_frontend *fe, u16 * strength)
{
	int ret = 0;
	stm_fe_demod_info_t info;

	dprintk(KERN_DEBUG "%s ...\n", __FUNCTION__);

	if (!fe)
		return -EINVAL;

	ret = stm_fe_demod_get_info(DMD_OBJ(fe), &info);
	if (ret) {
		return ret;
	}
	DEMOD_CHANNEL_INFO_COMMON_VALUE(info.u_info.tune.demod_info.std,
					info.u_info.tune.demod_info,
					signal_strength, strength);
	return ret;
}

int dvb_stm_fe_read_snr(struct dvb_frontend *fe, u16 * signal_snr)
{
	int ret = 0;
	stm_fe_demod_info_t info;

	dprintk(KERN_DEBUG "%s ...\n", __FUNCTION__);

	if (!fe)
		return -EINVAL;

	ret = stm_fe_demod_get_info(DMD_OBJ(fe), &info);
	if (ret) {
		return ret;
	}
	DEMOD_CHANNEL_INFO_COMMON_VALUE(info.u_info.tune.demod_info.std,
					info.u_info.tune.demod_info,
					snr, signal_snr);

	/* Correction done to provide final snr in (db*10) */
	(*signal_snr) = (*signal_snr) / 10 + (((*signal_snr) % 10) >= 5 ? 1 : 0);

	return ret;
}

int dvb_stm_fe_read_ucblocks(struct dvb_frontend *fe, u32 * ucblocks)
{
	int ret = 0;
	stm_fe_demod_info_t info;

	dprintk(KERN_DEBUG "%s ...\n", __FUNCTION__);

	if (!fe || !ucblocks)
		return -EINVAL;

	ret = stm_fe_demod_get_info(DMD_OBJ(fe), &info);
	if (ret) {
		return ret;
	}
	if (info.u_info.tune.demod_info.std & (STM_FE_DEMOD_TX_STD_DVBC |
                           STM_FE_DEMOD_TX_STD_DVBT |
					       STM_FE_DEMOD_TX_STD_J83_AC |
					       STM_FE_DEMOD_TX_STD_J83_B)) {
		DEMOD_CHANNEL_INFO_UCB_VALUE(info.u_info.tune.demod_info.std,
					     info.u_info.tune.demod_info,
					     ucblocks);
	} else
		*ucblocks = 0;
	return ret;
}

int dvb_stm_fe_diseqc_reset_overload(struct dvb_frontend *fe)
{
	dprintk(KERN_DEBUG "%s ...\n", __FUNCTION__);
	/* Not supported in KPI */
	return 0;
}

int dvb_stm_fe_diseqc_send_master_cmd(struct dvb_frontend *fe,
				      struct dvb_diseqc_master_cmd *cmd)
{
	stm_fe_diseqc_msg_t tx_data;
	int ret = 0;

	dprintk(KERN_DEBUG "%s ...\n", __FUNCTION__);

	if (!fe || !cmd)
		return -EINVAL;

	memcpy(tx_data.msg, cmd->msg, cmd->msg_len);
	tx_data.msg_len = cmd->msg_len;
	tx_data.op = STM_FE_DISEQC_TRANSFER_SEND;
	tx_data.timeout = 0;	/* ms */
	tx_data.timegap = 0;
	ret = stm_fe_diseqc_transfer(DSQ_OBJ(fe), STM_FE_DISEQC_COMMAND,
				     &tx_data, 1);
	return ret;
}

int dvb_stm_fe_diseqc_recv_slave_reply(struct dvb_frontend *fe,
				       struct dvb_diseqc_slave_reply *reply)
{
	stm_fe_diseqc_msg_t rx_data;
	int ret;

	dprintk(KERN_DEBUG "%s ...\n", __FUNCTION__);

	if (!fe || !reply)
		return -EINVAL;

	rx_data.op = STM_FE_DISEQC_TRANSFER_RECEIVE;
	rx_data.timeout = 150;	/* ms */
	rx_data.timegap = 0;

	ret = stm_fe_diseqc_transfer(DSQ_OBJ(fe), STM_FE_DISEQC_COMMAND,
				     &rx_data, 1);
	if (ret) {
		return ret;
	}
	memcpy(reply->msg, rx_data.msg, rx_data.msg_len);
	reply->msg_len = rx_data.msg_len;
	reply->timeout = 0;
	return ret;
}

int dvb_stm_fe_diseqc_send_burst(struct dvb_frontend *fe,
				 fe_sec_mini_cmd_t minicmd)
{
	stm_fe_diseqc_mode_t mode;
	int ret = 0;

	if (!fe)
		return -EINVAL;

	dprintk(KERN_DEBUG "%s ...\n", __FUNCTION__);

	if (minicmd == SEC_MINI_A)
		mode = STM_FE_DISEQC_TONE_BURST_SEND_0_UNMODULATED;
	else
		mode = STM_FE_DISEQC_TONE_BURST_SEND_1_MODULATED;

	ret = stm_fe_diseqc_transfer(DSQ_OBJ(fe), mode, NULL, 0);
	return ret;
}

int dvb_stm_fe_set_tone(struct dvb_frontend *fe, fe_sec_tone_mode_t tone)
{
	stm_fe_lnb_config_t lnbconfig;
	uint32_t look_up[2] = {
		STM_FE_LNB_TONE_22KHZ, STM_FE_LNB_TONE_OFF
	};

	if (!fe || !fe->sec_priv)
		return -EINVAL;

	dprintk(KERN_DEBUG "%s ...\n", __FUNCTION__);
	lnbconfig.lnb_tone_state = look_up[tone];
	lnbconfig.polarization = STM_FE_LNB_PLR_DEFAULT;
	return (stm_fe_lnb_set_config(LNB_OBJ(fe), &lnbconfig));
}

int dvb_stm_fe_set_voltage(struct dvb_frontend *fe, fe_sec_voltage_t voltage)
{
	stm_fe_lnb_config_t lnbconfig;
	uint32_t look_up[3] = {
		STM_FE_LNB_PLR_VERTICAL,
		STM_FE_LNB_PLR_HORIZONTAL, STM_FE_LNB_PLR_OFF
	};

	if (!fe || !fe->sec_priv)
		return -EINVAL;

	dprintk(KERN_DEBUG "%s ...\n", __FUNCTION__);
	lnbconfig.lnb_tone_state = STM_FE_LNB_TONE_DEFAULT;
	lnbconfig.polarization = look_up[voltage];
	return (stm_fe_lnb_set_config(LNB_OBJ(fe), &lnbconfig));
}

int dvb_stm_fe_enable_high_lnb_voltage(struct dvb_frontend *fe, long arg)
{
	dprintk(KERN_DEBUG "%s ...\n", __FUNCTION__);
	/* Not supported in KPI. It is a part of static configuration */
	return 0;
}

int dvb_stm_fe_dishnetwork_send_legacy_command(struct dvb_frontend *fe,
					       unsigned long cmd)
{
	dprintk(KERN_DEBUG "%s ...\n", __FUNCTION__);
	/* Not required now */
	return 0;
}

int dvb_stm_fe_i2c_gate_ctrl(struct dvb_frontend *fe, int enable)
{
	dprintk(KERN_DEBUG "%s ...\n", __FUNCTION__);
	/* Not required */
	return 0;
}

int dvb_stm_fe_ts_bus_ctrl(struct dvb_frontend *fe, int acquire)
{
	dprintk(KERN_DEBUG "%s ...\n", __FUNCTION__);
	/* Not required */
	return 0;
}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 3, 0))
int dvb_stm_fe_set_rf_input_src(struct dvb_frontend *fe)
{
	int ret = 0;
	struct dtv_frontend_properties *c;
	if (!fe)
		return -EINVAL;
	c = &fe->dtv_property_cache;

	dprintk(KERN_DEBUG "%s ...\n", __func__);

	ret = stm_fe_rf_matrix_select_source(RF_MATRIX_OBJ(fe),
						c->rf_input_src);
	if (ret)
		printk(KERN_ERR
		       "%s: stm_fe_rf_matrix_select_source failed ret = 0x%x\n",
		       __func__, ret);

	return ret;
}
#endif

/* These callbacks are for devices that implement their own
 * tuning algorithms, rather than a simple swzigzag
 */

#define OFDM_BW_MAX 8
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,3,0))
enum dvbfe_search dvb_stm_fe_search(struct dvb_frontend *fe)
#else
enum dvbfe_search dvb_stm_fe_search(struct dvb_frontend *fe, struct
				    dvb_frontend_parameters
				    *p)
#endif
{
	stm_fe_demod_tune_params_t tuning_params;
	struct dvb_stm_fe_demod_state *state;
	struct dtv_frontend_properties *c;
	int ret = 0;
	uint32_t num_events;
	stm_event_info_t events;
	uint32_t num_stds;

	if (!fe)
		return -EINVAL;

#if (LINUX_VERSION_CODE < KERNEL_VERSION(3,3,0))
	if (!p)
		return -EINVAL;
#endif

	state = fe->demodulator_priv;
	c = &fe->dtv_property_cache;

	dprintk(KERN_DEBUG "%s ...\n", __FUNCTION__);

	for (num_stds = 0; num_stds < state->stinfo->u_caps.demod.num_stds;
	     num_stds++) {
		stm_fe_demod_tune_caps_t *tune_caps =
		    &state->stinfo->u_caps.demod.tune_caps[num_stds];
		fe_type_t type = get_dvb_type(tune_caps->std);
		/*
		 *For multi-standard devices like 367, having capability for DVB-C and DVB-T,
		 *this loop will always try to tune for DVB-C first irrespective
		 *of the tune parameters coming from utils. There is no way to
		 *to identify whether this tune is actually for DVB-C or is the first
		 *tuning trial for DVB-T. With the following checks, first wrong tune trial
		 *for DVB-T can be bypassed.
		 */
		if (type == FE_QPSK)
			dvb_to_fe_tune_params_qpsk(c, &tuning_params,
						   tune_caps);
		else if (type == FE_QAM)
			if (c->symbol_rate >= tune_caps->u_tune.dvbc.sr_min)
				dvb_to_fe_tune_params_qam(c, &tuning_params,
							  tune_caps);
			else
				continue;
		else if (type == FE_OFDM)
			dvb_to_fe_tune_params_ofdm(c, &tuning_params,
						   tune_caps);
		else if (type == FE_ATSC)
			dvb_to_fe_tune_params_vsb(c, &tuning_params, tune_caps);
		else {
			printk(KERN_ERR "%s: Search standard %d is unknown.\n",
			       __FUNCTION__, type);
			return DVBFE_ALGO_SEARCH_INVALID;
		}
		/* Avoid setting unsupported standards */
		if (type == FE_QPSK)
			tuning_params.std &= state->supported_stds;
		else
			tuning_params.std = tune_caps->std;
		ret = stm_fe_demod_tune(DMD_OBJ(fe), &tuning_params);
		if (ret) {
			printk(KERN_ERR
			       "%s: stm_fe_demod_tune() failed ret = 0x%x\n",
			       __FUNCTION__, ret);
			return DVBFE_ALGO_SEARCH_ERROR;
		}
		ret =
		    stm_event_wait(state->ev_subs, -1, FRONTEND_EVENTS,
				   &num_events, &events);
		if (ret) {
			printk(KERN_ERR
			       "%s: stm_event_wait() failed ret = 0x%x\n",
			       __FUNCTION__, ret);
			return DVBFE_ALGO_SEARCH_ERROR;
		}

		if (events.event.event_id == STM_FE_DEMOD_EVENT_TUNE_LOCKED)
			return DVBFE_ALGO_SEARCH_SUCCESS;
	}

	return DVBFE_ALGO_SEARCH_FAILED;
}

void dvb_to_fe_tune_params_qpsk(struct dtv_frontend_properties *dvb_tune,
				stm_fe_demod_tune_params_t * fe_tune,
				stm_fe_demod_tune_caps_t * tune_caps)
{
	fe_tune->std = QPSK_STDS;
	fe_tune->u_tune.dvbs2.freq = dvb_tune->frequency;
	fe_tune->u_tune.dvbs2.sr = dvb_tune->symbol_rate;
	fe_tune->u_tune.dvbs2.fec = get_stm_fec[dvb_tune->fec_inner];
	fe_tune->u_tune.dvbs2.inv = get_stm_inv[dvb_tune->inversion];
	fe_tune->u_tune.dvbs2.mod = get_stm_mod[QPSK];
	fe_tune->u_tune.dvbs2.roll_off = STM_FE_DEMOD_ROLL_OFF_0_35;
}

void dvb_to_fe_tune_params_qam(struct dtv_frontend_properties *dvb_tune,
			       stm_fe_demod_tune_params_t * fe_tune,
			       stm_fe_demod_tune_caps_t * tune_caps)
{
	fe_tune->std = QAM_STDS;
	fe_tune->u_tune.dvbc.freq =
	    dvb_tune->frequency / DVB_NON_QPSK_FREQ_FACTOR;
	fe_tune->u_tune.dvbc.sr = dvb_tune->symbol_rate;
	fe_tune->u_tune.dvbc.inv = get_stm_inv[dvb_tune->inversion];
	fe_tune->u_tune.dvbc.mod = get_stm_mod[dvb_tune->modulation];
}

void dvb_to_fe_tune_params_ofdm(struct dtv_frontend_properties *dvb_tune,
				stm_fe_demod_tune_params_t * fe_tune,
				stm_fe_demod_tune_caps_t * tune_caps)
{
	fe_tune->std = OFDM_STDS;
	if (tune_caps->std & STM_FE_DEMOD_TX_STD_DVBT2) {
		fe_tune->u_tune.dvbt2.freq =
		    dvb_tune->frequency / DVB_NON_QPSK_FREQ_FACTOR;
		fe_tune->u_tune.dvbt2.bw =
		    lookup_stm_bw(dvb_tune->bandwidth_hz);
		fe_tune->u_tune.dvbt2.fec = get_stm_fec[dvb_tune->code_rate_HP];
		fe_tune->u_tune.dvbt2.inv = get_stm_inv[dvb_tune->inversion];
		fe_tune->u_tune.dvbt2.mod = get_stm_mod[dvb_tune->modulation];
		if (dvb_tune->hierarchy == HIERARCHY_NONE)
			fe_tune->u_tune.dvbt2.hierarchy =
			    STM_FE_DEMOD_HIERARCHY_NONE;
		else
			fe_tune->u_tune.dvbt2.hierarchy =
			    STM_FE_DEMOD_HIERARCHY_HIGH;
		fe_tune->u_tune.dvbt2.guard =
		    get_stm_guard[dvb_tune->guard_interval];
		fe_tune->u_tune.dvbt2.fft_mode =
		    get_stm_mode[dvb_tune->transmission_mode];
	} else {
		fe_tune->u_tune.dvbt.freq =
		    dvb_tune->frequency / DVB_NON_QPSK_FREQ_FACTOR;
		fe_tune->u_tune.dvbt.bw = lookup_stm_bw(dvb_tune->bandwidth_hz);
		fe_tune->u_tune.dvbt.fec = get_stm_fec[dvb_tune->code_rate_HP];
		fe_tune->u_tune.dvbt.inv = get_stm_inv[dvb_tune->inversion];
		fe_tune->u_tune.dvbt.mod = get_stm_mod[dvb_tune->modulation];
		if (dvb_tune->hierarchy == HIERARCHY_NONE)
			fe_tune->u_tune.dvbt.hierarchy =
			    STM_FE_DEMOD_HIERARCHY_NONE;
		else
			fe_tune->u_tune.dvbt.hierarchy =
			    STM_FE_DEMOD_HIERARCHY_HIGH;
		fe_tune->u_tune.dvbt.guard =
		    get_stm_guard[dvb_tune->guard_interval];
		fe_tune->u_tune.dvbt.fft_mode =
		    get_stm_mode[dvb_tune->transmission_mode];
	}
}

void dvb_to_fe_tune_params_vsb(struct dtv_frontend_properties *dvb_tune,
			       stm_fe_demod_tune_params_t * fe_tune,
			       stm_fe_demod_tune_caps_t * tune_caps)
{
	fe_tune->std = ATSC_STDS;
	fe_tune->u_tune.atsc.freq = dvb_tune->frequency;
	fe_tune->u_tune.atsc.fec = get_stm_fec[FEC_AUTO];
	fe_tune->u_tune.atsc.inv = get_stm_inv[dvb_tune->inversion];
	fe_tune->u_tune.atsc.mod = get_stm_mod[VSB_8];
}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,3,0))
int dvb_stm_fe_track(struct dvb_frontend *fe)
#else
int dvb_stm_fe_track(struct dvb_frontend *fe, struct dvb_frontend_parameters *p)
#endif
{
	int ret = 0;
	struct dtv_frontend_properties *c;
	stm_fe_demod_info_t info;

	if (!fe)
		return -EINVAL;

	dprintk(KERN_DEBUG "%s ...\n", __FUNCTION__);

#if (LINUX_VERSION_CODE < KERNEL_VERSION(3,3,0))
	if (!p)
		return -EINVAL;

	memset(p, 0, sizeof(struct dvb_frontend_parameters));
#endif

	c = &fe->dtv_property_cache;

	ret = stm_fe_demod_get_info(DMD_OBJ(fe), &info);
	if (ret) {
		return ret;
	}
	if (info.u_info.tune.demod_info.std & QPSK_STDS)
		fe_to_dvb_tune_info_qpsk(c, &info.u_info.tune.demod_info);
	if (info.u_info.tune.demod_info.std & QAM_STDS)
		fe_to_dvb_tune_info_qam(c, &info.u_info.tune.demod_info);
	if (info.u_info.tune.demod_info.std & OFDM_STDS)
		fe_to_dvb_tune_info_ofdm(c, &info.u_info.tune.demod_info);
	if (info.u_info.tune.demod_info.std & ATSC_STDS)
		fe_to_dvb_tune_info_vsb(c, &info.u_info.tune.demod_info);
	return ret;
}

void fe_to_dvb_tune_info_qpsk(struct dtv_frontend_properties *dvb_info,
			      stm_fe_demod_channel_info_t * fe_info)
{
	if (fe_info->std & STM_FE_DEMOD_TX_STD_DVBS) {
		dvb_info->frequency = fe_info->u_channel.dvbs.freq;
		if (fe_info->u_channel.dvbs.inv ==
		    STM_FE_DEMOD_SPECTRAL_INVERSION_OFF)
			dvb_info->inversion = INVERSION_OFF;
		else
			dvb_info->inversion = INVERSION_ON;
		dvb_info->symbol_rate = fe_info->u_channel.dvbs.sr;
		GET_DVB_VAL(&dvb_info->fec_inner,
			    fe_info->u_channel.dvbs.fec, get_stm_fec);
		/* cnr scale is measured in 0.001 dB steps */
		if (fe_info->u_channel.dvbs.status == STM_FE_DEMOD_SYNC_OK) {
			dvb_info->cnr.stat[0].svalue =
				(fe_info->u_channel.dvbs.snr) * 100;
			dvb_info->cnr.stat[0].scale = FE_SCALE_DECIBEL;
		} else {
			dvb_info->cnr.stat[0].scale = FE_SCALE_NOT_AVAILABLE;
		}
	} else {
		dvb_info->frequency = fe_info->u_channel.dvbs2.freq;
		if (fe_info->u_channel.dvbs2.inv ==
		    STM_FE_DEMOD_SPECTRAL_INVERSION_OFF)
			dvb_info->inversion = INVERSION_OFF;
		else
			dvb_info->inversion = INVERSION_ON;
		dvb_info->symbol_rate = fe_info->u_channel.dvbs2.sr;
		GET_DVB_VAL(&dvb_info->fec_inner,
			    fe_info->u_channel.dvbs2.fec, get_stm_fec);
		/* cnr scale is measured in 0.001 dB steps */
		if (fe_info->u_channel.dvbs2.status == STM_FE_DEMOD_SYNC_OK) {
			dvb_info->cnr.stat[0].svalue =
				(fe_info->u_channel.dvbs2.snr) * 100;
			dvb_info->cnr.stat[0].scale = FE_SCALE_DECIBEL;
		} else {
			dvb_info->cnr.stat[0].scale = FE_SCALE_NOT_AVAILABLE;
		}
	}
}

void fe_to_dvb_tune_info_qam(struct dtv_frontend_properties *dvb_info,
			     stm_fe_demod_channel_info_t * fe_info)
{
	dvb_info->frequency = fe_info->u_channel.dvbc.freq *
	    DVB_NON_QPSK_FREQ_FACTOR;
	if (fe_info->u_channel.dvbc.inv == STM_FE_DEMOD_SPECTRAL_INVERSION_OFF)
		dvb_info->inversion = INVERSION_OFF;
	else
		dvb_info->inversion = INVERSION_ON;
	dvb_info->symbol_rate = fe_info->u_channel.dvbc.sr;
	if (fe_info->u_channel.dvbc.mod != STM_FE_DEMOD_MODULATION_NONE)
		dvb_info->modulation =
		    get_dvb_mod[ilog2(fe_info->u_channel.dvbc.mod)];
	else
		dvb_info->modulation = 0;	/* 0 is also valid */
	/* cnr scale is measured in 0.001 dB steps */
	if (fe_info->u_channel.dvbc.status == STM_FE_DEMOD_SYNC_OK) {
		dvb_info->cnr.stat[0].svalue =
				(fe_info->u_channel.dvbc.snr) * 100;
		dvb_info->cnr.stat[0].scale = FE_SCALE_DECIBEL;
	} else {
		dvb_info->cnr.stat[0].scale = FE_SCALE_NOT_AVAILABLE;
	}
}

void fe_to_dvb_tune_info_ofdm(struct dtv_frontend_properties *dvb_info,
			      stm_fe_demod_channel_info_t * fe_info)
{

	if (fe_info->std & STM_FE_DEMOD_TX_STD_DVBT) {
		dvb_info->frequency = fe_info->u_channel.dvbt.freq *
		    DVB_NON_QPSK_FREQ_FACTOR;
		if (fe_info->u_channel.dvbt.inv ==
		    STM_FE_DEMOD_SPECTRAL_INVERSION_OFF)
			dvb_info->inversion = INVERSION_OFF;
		else
			dvb_info->inversion = INVERSION_ON;
		GET_DVB_VAL(&dvb_info->code_rate_HP,
			    fe_info->u_channel.dvbt.fec, get_stm_fec);
		dvb_info->code_rate_LP = dvb_info->code_rate_HP;
		if (fe_info->u_channel.dvbt.mod != STM_FE_DEMOD_MODULATION_NONE)
			dvb_info->modulation =
			    get_dvb_mod[ilog2(fe_info->u_channel.dvbt.mod)];

		dvb_info->bandwidth_hz =
		    lookup_bandwidth_hz(fe_info->u_channel.dvbt.bw);
		GET_DVB_VAL(&dvb_info->guard_interval,
			    fe_info->u_channel.dvbt.guard, get_stm_guard);
		GET_DVB_VAL(&dvb_info->transmission_mode,
			    fe_info->u_channel.dvbt.fft_mode, get_stm_mode);
		if (fe_info->u_channel.dvbt.alpha ==
		    STM_FE_DEMOD_HIERARCHY_ALPHA_NONE)
			dvb_info->hierarchy = HIERARCHY_NONE;
		else
			dvb_info->hierarchy =
			    get_dvb_hier[ilog2(fe_info->u_channel.dvbt.alpha)];
		/* cnr scale is measured in 0.001 dB steps */
		if (fe_info->u_channel.dvbt.status == STM_FE_DEMOD_SYNC_OK) {
			dvb_info->cnr.stat[0].svalue =
				(fe_info->u_channel.dvbt.snr) * 100;
			dvb_info->cnr.stat[0].scale = FE_SCALE_DECIBEL;
		} else {
			dvb_info->cnr.stat[0].scale = FE_SCALE_NOT_AVAILABLE;
		}
	} else {
		dvb_info->frequency = fe_info->u_channel.dvbt2.freq *
		    DVB_NON_QPSK_FREQ_FACTOR;
		if (fe_info->u_channel.dvbt2.inv ==
		    STM_FE_DEMOD_SPECTRAL_INVERSION_OFF)
			dvb_info->inversion = INVERSION_OFF;
		else
			dvb_info->inversion = INVERSION_ON;
		GET_DVB_VAL(&dvb_info->code_rate_HP,
			    fe_info->u_channel.dvbt2.fec, get_stm_fec);
		dvb_info->code_rate_LP = dvb_info->code_rate_HP;
		if (fe_info->u_channel.dvbt2.mod !=
		    STM_FE_DEMOD_MODULATION_NONE)
			dvb_info->modulation =
			    get_dvb_mod[ilog2(fe_info->u_channel.dvbt2.mod)];
		dvb_info->bandwidth_hz =
		    lookup_bandwidth_hz(fe_info->u_channel.dvbt2.bw);
		GET_DVB_VAL(&dvb_info->guard_interval,
			    fe_info->u_channel.dvbt2.guard, get_stm_guard);
		GET_DVB_VAL(&dvb_info->transmission_mode,
			    fe_info->u_channel.dvbt2.fft_mode, get_stm_mode);
		if (fe_info->u_channel.dvbt2.alpha ==
		    STM_FE_DEMOD_HIERARCHY_ALPHA_NONE)
			dvb_info->hierarchy = HIERARCHY_NONE;
		else
			dvb_info->hierarchy =
			    get_dvb_hier[ilog2(fe_info->u_channel.dvbt2.alpha)];
		/* cnr scale is measured in 0.001 dB steps */
		if (fe_info->u_channel.dvbt2.status == STM_FE_DEMOD_SYNC_OK) {
			dvb_info->cnr.stat[0].svalue =
				(fe_info->u_channel.dvbt2.snr) * 100;
			dvb_info->cnr.stat[0].scale = FE_SCALE_DECIBEL;
		} else {
			dvb_info->cnr.stat[0].scale = FE_SCALE_NOT_AVAILABLE;
		}
	}
}

void fe_to_dvb_tune_info_vsb(struct dtv_frontend_properties *dvb_info,
			     stm_fe_demod_channel_info_t * fe_info)
{
	dvb_info->frequency = fe_info->u_channel.atsc.freq *
	    DVB_NON_QPSK_FREQ_FACTOR;
	if (fe_info->u_channel.atsc.inv == STM_FE_DEMOD_SPECTRAL_INVERSION_OFF)
		dvb_info->inversion = INVERSION_OFF;
	else
		dvb_info->inversion = INVERSION_ON;
	if (fe_info->u_channel.atsc.mod != STM_FE_DEMOD_MODULATION_NONE)
		dvb_info->modulation =
		    get_dvb_mod[ilog2(fe_info->u_channel.atsc.mod)];
	/* cnr scale is measured in 0.001 dB steps */
	if (fe_info->u_channel.atsc.status == STM_FE_DEMOD_SYNC_OK) {
		dvb_info->cnr.stat[0].svalue =
				(fe_info->u_channel.atsc.snr) * 100;
		dvb_info->cnr.stat[0].scale = FE_SCALE_DECIBEL;
	} else {
		dvb_info->cnr.stat[0].scale = FE_SCALE_NOT_AVAILABLE;
	}
}

int dvb_stm_fe_ip_start(struct dvb_ipfe *fe)
{
	int ret = 0;
	ret = stm_fe_ip_start(IP_OBJ(fe));
	if (ret)
		printk(KERN_ERR "%s - Failed\n", __FUNCTION__);
	return ret;
}
int dvb_stm_fe_ip_fec_start(struct dvb_ipfe *fe)
{
	int ret = 0;
	ret = stm_fe_ip_fec_start(IPFEC_OBJ(fe));
	if (ret)
		printk(KERN_ERR "%s - Failed\n", __FUNCTION__);
	return ret;
}


int dvb_stm_fe_ip_stop(struct dvb_ipfe *fe)
{
	int ret = 0;
	ret = stm_fe_ip_stop(IP_OBJ(fe));
	if (ret)
		printk(KERN_ERR "%s - Failed\n", __FUNCTION__);
	return ret;
}

int dvb_stm_fe_ip_fec_stop(struct dvb_ipfe *fe)
{
	int ret = 0;
	ret = stm_fe_ip_fec_stop(IPFEC_OBJ(fe));
	if (ret)
		printk(KERN_ERR "%s - Failed\n", __FUNCTION__);
	return ret;
}

int dvb_stm_fe_ip_set_control(struct dvb_ipfe *fe, fe_ip_control_t flag,
			      void *args)
{
	int ret = 0;
	ret =
	    stm_fe_ip_set_compound_control(IP_OBJ(fe), flag,
					   (stm_fe_ip_control_t *) args);
	if (ret)
		printk(KERN_ERR "%s - Failed\n", __FUNCTION__);
	return ret;
}

int dvb_stm_fe_ip_set_fec(struct dvb_ipfe *fe, fe_ip_fec_control_t flag,
			      void *args)
{
	int ret = 0;
	ret =
	    stm_fe_ip_fec_set_compound_control(IPFEC_OBJ(fe), flag,
					   (stm_fe_ip_control_t *) args);
	if (ret)
		printk(KERN_ERR "%s - Failed\n", __FUNCTION__);
	return ret;
}

int dvb_stm_fe_ip_get_capability(struct dvb_ipfe *fe, void *args)
{
	struct fe_ip_caps * caps = (struct fe_ip_caps *)args;

	memcpy(caps, &fe->ops.caps, sizeof(struct fe_ip_caps));

	return 0;
}
