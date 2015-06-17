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

Source file name : d0903.c
Author :           Shobhit

wrapper for stv090x LLA

Date        Modification                                    Name
----        ------------                                    --------
20-Jun-11   Created                                         Shobhit

************************************************************************/

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/list.h>
#include <linux/platform_device.h>
#include <linux/stm/plat_dev.h>
#include <linux/stm/demod.h>
#include <linux/stm/lnb.h>
#include <linux/stm/diseqc.h>
#include <stm_registry.h>
#include <stm_fe.h>
#include <stfe_utilities.h>
#include <stm_fe_os.h>
#include <i2c_wrapper.h>
#include <stm_fe_demod.h>
#include <stm_fe_diseqc.h>
#include <lla_utils.h>
#include <fesat_commlla_str.h>
#include "stv0903_drv.h"
#include "d0903.h"
#include <fe_sat_tuner.h>
#include "stv0903_init.h"

#define STV0903_CURRENT_LLA_REVISION "STV0903-LLA_REL_4.4_JUN_18_2010"
#define THIS_PATH(p) (FE_SAT_DEMOD_1)

static int stv0903_init(struct stm_fe_demod_s *priv);
static int stv0903_tune(struct stm_fe_demod_s *priv, bool *lock);
static int stv0903_scan(struct stm_fe_demod_s *priv, bool *lock);
static int stv0903_status(struct stm_fe_demod_s *priv, bool *locked);
static int stv0903_tracking(struct stm_fe_demod_s *priv);
static int stv0903_unlock(struct stm_fe_demod_s *priv);
static int stv0903_term(struct stm_fe_demod_s *priv);
static int stv0903_abort(struct stm_fe_demod_s *priv, bool abort);
static int stv0903_standby(struct stm_fe_demod_s *priv, bool standby);
static int stv0903_restore(struct stm_fe_demod_s *priv);

static void configure_ts_parameter(enum demod_tsout_config_e ts_out,
		FE_STV0903_TSConfig_t *path_ts_config);

/*
 * Name: stm_fe_stv0903_attach()
 *
 * Description: Installed the wrapper function pointers for LLA
 *
 */
int stm_fe_stv0903_attach(struct stm_fe_demod_s *priv)
{
	if (!priv->ops)
		return -EFAULT;
	priv->ops->init = stv0903_init;
	priv->ops->tune = stv0903_tune;
	priv->ops->scan = stv0903_scan;
	priv->ops->tracking = stv0903_tracking;
	priv->ops->status = stv0903_status;
	priv->ops->unlock = stv0903_unlock;
	priv->ops->term = stv0903_term;
	priv->ops->abort = stv0903_abort;
	priv->ops->standby = stv0903_standby;
	priv->ops->detach = stm_fe_stv0903_detach;
	priv->ops->restore = stv0903_restore;
	priv->ops->tsclient_init = NULL;
	priv->ops->tsclient_term = NULL;
	priv->ops->tsclient_pid_configure = NULL;
	priv->ops->tsclient_pid_deconfigure = NULL;

	return 0;
}
EXPORT_SYMBOL(stm_fe_stv0903_attach);

/*
 * Name: stm_fe_stv0903_detach()
 *
 * Description: Uninstalled the wrapper function pointers for LLA
 *
 */
int stm_fe_stv0903_detach(struct stm_fe_demod_s *priv)
{
	if (!priv->ops)
		return -EFAULT;

	priv->ops->tsclient_pid_deconfigure = NULL;
	priv->ops->tsclient_pid_configure = NULL;
	priv->ops->tsclient_term = NULL;
	priv->ops->tsclient_init = NULL;
	priv->ops->restore = NULL;
	priv->ops->detach = NULL;
	priv->ops->standby = NULL;
	priv->ops->abort = NULL;
	priv->ops->term = NULL;
	priv->ops->unlock = NULL;
	priv->ops->status = NULL;
	priv->ops->tracking = NULL;
	priv->ops->scan = NULL;
	priv->ops->tune = NULL;
	priv->ops->init = NULL;

	return 0;
}
EXPORT_SYMBOL(stm_fe_stv0903_detach);

static void configure_ts_parameter(enum demod_tsout_config_e ts_out,
		 FE_STV0903_TSConfig_t *path_ts_config)
{
	if (ts_out & DEMOD_TS_SERIAL_PUNCT_CLOCK)
		path_ts_config->TSMode = FE_TS_SERIAL_PUNCT_CLOCK;
	else if (ts_out & DEMOD_TS_SERIAL_CONT_CLOCK)
		path_ts_config->TSMode = FE_TS_SERIAL_CONT_CLOCK;
	else if (ts_out & DEMOD_TS_PARALLEL_PUNCT_CLOCK)
		path_ts_config->TSMode = FE_TS_PARALLEL_PUNCT_CLOCK;
	else if (ts_out & DEMOD_TS_PARALLEL_PUNCT_CLOCK)
		path_ts_config->TSMode = FE_TS_PARALLEL_PUNCT_CLOCK;
	else if (ts_out & DEMOD_TS_DVBCI_CLOCK)
		path_ts_config->TSMode = FE_TS_DVBCI_CLOCK;

	path_ts_config->TSSpeedControl = (ts_out & DEMOD_TS_MANUAL_SPEED) ?
	    FE_TS_MANUAL_SPEED : FE_TS_AUTO_SPEED;

	path_ts_config->TSClockPolarity = (ts_out & DEMOD_TS_RISINGEDGE_CLOCK) ?
	    FE_TS_RISINGEDGE_CLOCK : FE_TS_FALLINGEDGE_CLOCK;

	path_ts_config->TSSyncByteEnable = (ts_out & DEMOD_TS_SYNCBYTE_ON) ?
	    FE_TS_SYNCBYTE_ON : FE_TS_SYNCBYTE_OFF;

	path_ts_config->TSParityBytes = (ts_out & DEMOD_TS_PARITYBYTES_ON) ?
	    FE_TS_PARITYBYTES_ON : FE_TS_PARITYBYTES_OFF;

	path_ts_config->TSSwap = (ts_out & DEMOD_TS_SWAP_ON) ?
	    FE_TS_SWAP_ON : FE_TS_SWAP_OFF;

	path_ts_config->TSSmoother = (ts_out & DEMOD_TS_SMOOTHER_ON) ?
	    FE_TS_SMOOTHER_ON : FE_TS_SMOOTHER_OFF;
}

/*
 * Name: stv0903_init()
 *
 * Description: Providing Initiliazing parameters to LLA
 *
 */
static int stv0903_init(struct stm_fe_demod_s *priv)
{
	FE_STV0903_InternalParams_t *p;
	FE_Sat_InitParams_t ip;
	FE_STV0903_TSConfig_t ts_conf;
	uint32_t ts_speed;
	int ret = 0;
	FE_STV0903_Error_t err;
	size_t size;

	priv->demod_h->pData = NULL;

	/*allocate the memory for LLA internal parameters */
	p = stm_fe_malloc(sizeof(FE_STV0903_InternalParams_t));
	if (!p) {
		pr_err("%s: mem alloc failed stv0903 params\n", __func__);
		return -ENOMEM;
	}
	priv->demod_i2c = chip_open(priv->config->demod_io.bus);
	if (!priv->demod_i2c) {
		stm_fe_free((void **)&p);
		return -EINVAL;
	}

	size = STV0903_NBREGS * sizeof(STCHIP_Register_t);
	priv->demod_h->pRegMapImage = stm_fe_malloc(size);
	if (!priv->demod_h->pRegMapImage) {
		priv->demod_i2c = NULL;
		stm_fe_free((void **)&p);
		pr_err("%s: mem alloc failed for Demod reg map\n", __func__);
		return -ENOMEM;
	}

	/*To enable the auto repeater mode */
	priv->demod_h->IsAutoRepeaterOffEnable = true;
	priv->demod_h->Abort = false;
	priv->tuner_h->Abort = false;
	/*copy demod/tuner chip structure to p */
	p->hDemod = priv->demod_h;
	priv->demod_h->IsAutoRepeaterOffEnable = true;
	p->hTuner = (TUNER_Handle_t) priv->tuner_h;

	memcpy(&(p->hDemod->dev_i2c), &(priv->demod_i2c),
			sizeof(priv->demod_i2c));
	memcpy(&(p->hTuner->dev_i2c), &(priv->demod_i2c),
			sizeof(priv->demod_i2c));

	p->hDemod->I2cAddr = priv->config->demod_io.address;
	p->hDemod->IORoute = priv->config->demod_io.route;
	p->hDemod->IODriver = priv->config->demod_io.io;

	p->hTuner->I2cAddr = priv->config->tuner_io.address;
	p->hTuner->IORoute = priv->config->tuner_io.route;
	p->hTuner->IODriver = priv->config->tuner_io.io;

	/*copy pParam structure pointer to demod_p to furthur access */
	priv->demod_params = (void *)p;

	memset(&ip, '\0', sizeof(FE_Sat_InitParams_t));
	strncpy(ip.DemodName, "STV0903", sizeof(ip.DemodName));	/* Demod name */
	ip.DemodI2cAddr = priv->config->demod_io.address;
	/* Refrence clock for the Demodulator in Hz (27MHz,30MHz,....) */
	ip.DemodRefClock = priv->config->clk_config.demod_clk;
	/* NYQUIST Filter value (used for DVBS1/DSS, DVBS2 is automatic) */
	ip.RollOff = priv->config->roll_off == DEMOD_ROLL_OFF_0_35 ? FE_SAT_35
								: FE_SAT_20;

	/*Path1 parameters */

	if (priv->config->custom_flags & DEMOD_CUSTOM_USE_AUTO_TUNER) {
		if (IS_NAME_MATCH(priv->config->tuner_name, "STB6000")) {
			ip.TunerHWControlType = FE_SAT_AUTO_STB6000;
		} else if (IS_NAME_MATCH(priv->config->tuner_name, "STV6110")) {
			ip.TunerHWControlType = FE_SAT_AUTO_STV6110;
		} else if (IS_NAME_MATCH(priv->config->tuner_name, "STB6100")) {
			ip.TunerHWControlType = FE_SAT_AUTO_STB6100;
		} else if ((IS_NAME_MATCH(priv->config->tuner_name,
								"STV6120_1"))
		    || (IS_NAME_MATCH(priv->config->tuner_name, "STV6120_2"))) {
			ip.TunerHWControlType = FE_SAT_AUTO_STV6120;
		} else {
			ip.TunerHWControlType = FE_SAT_SW_TUNER;
		}
	} else if (priv->config->custom_flags == DEMOD_CUSTOM_NONE) {
		ip.TunerHWControlType = FE_SAT_SW_TUNER;

	}

	if (priv->config->ts_out & 0x1)
		ip.PathTSClock = FE_TS_SERIAL_PUNCT_CLOCK;
	else if ((priv->config->ts_out) & 0x2)
		ip.PathTSClock = FE_TS_SERIAL_PUNCT_CLOCK;

	ip.TunerModel = stmfe_set_tuner_model(priv->config->tuner_name);

	strncpy(ip.TunerName, "Tuner1", sizeof(ip.TunerName));
	ip.Tuner_I2cAddr = priv->config->tuner_io.address;
	ip.TunerRefClock = priv->config->clk_config.tuner_clk;
	ip.TunerOutClkDivider = priv->config->clk_config.tuner_clkout_divider;

	if (priv->config->custom_flags & DEMOD_CUSTOM_IQ_WIRING_INVERTED)
		ip.TunerIQ_Inversion = FE_SAT_IQ_SWAPPED;
	else
		ip.TunerIQ_Inversion = FE_SAT_IQ_NORMAL;

	if (priv->config->custom_flags & DEMOD_CUSTOM_ADD_ON_VGLNA)
		ip.TunerBasebandGain = 12;
	else
		ip.TunerBasebandGain = TUNER_BBGAIN(ip.TunerModel);

	if (ip.TunerModel == TUNER_STV6120_Tuner2)
		ip.TunerBandSelect = LBRFD_HBRFC;
	err = FE_STV0903_Init(&ip, &priv->demod_params);
	if (err != FE_LLA_NO_ERROR) {
		pr_err("%s: LLA initialization failed\n", __func__);
		stm_fe_free((void **)&priv->demod_h->pRegMapImage);
		priv->demod_i2c = NULL;
		stm_fe_free((void **)&p);
		return -EINVAL;
	}
	configure_ts_parameter(priv->config->ts_out, &ts_conf);

	ts_conf.TSClockRate = priv->config->ts_clock;

	err = FE_STV0903_SetTSConfig((FE_STV0903_Handle_t) p, &ts_conf,
			&ts_speed);
	if (err != FE_LLA_NO_ERROR) {
		stm_fe_free((void **)&priv->demod_h->pRegMapImage);
		priv->demod_i2c = NULL;
		stm_fe_free((void **)&p);
		ret = -EINVAL;
	}

	priv->c_tinfo.u_info.tune.demod_info.u_channel.dvbs.status =
		STM_FE_DEMOD_STATUS_UNKNOWN;
	priv->c_tinfo.u_info.tune.demod_info.u_channel.dvbs2.status =
		STM_FE_DEMOD_STATUS_UNKNOWN;

	return ret;
}

/*
 * Name: stv0903_term()
 *
 * Description: terminating the stv0903 LLA
 *
 */
static int stv0903_term(struct stm_fe_demod_s *priv)
{
	int ret = -1;

	priv->demod_i2c = NULL;
	priv->demod_h->dev_i2c = NULL;
	priv->tuner_h->dev_i2c = NULL;
	ret = stm_fe_free((void **)&priv->demod_h->pRegMapImage);
	if (ret) {
		pr_err("%s: tried to free NULL ptr RegImage\n", __func__);
		return ret;
	}

	ret = stm_fe_free((void **)&priv->demod_params);
	if (ret) {
		pr_err("%s: tried to free NULL ptr demod_param\n", __func__);
		return ret;
	}

	return ret;
}

/*
 * Name: stv0903_tune()
 *
 * Description: Provided tuning parameters to LLA and perfomed tuning
 *
 */
static int stv0903_tune(struct stm_fe_demod_s *priv, bool *lock)
{
	FE_STV0903_Error_t err;
	FE_STV0903_SearchParams_t sp;
	FE_STV0903_SearchResult_t rp;
	void *hdl;
	int ret = 0;
	stm_fe_demod_tx_std_t std_in = priv->tp.std;
	/*
	 * This is to exploit the characteristic of "union" for
	 * stm_fe_demod_dvbs_channel_info_t and
	 * stm_fe_demod_dvbs2_channel_info_t
	 */
	stm_fe_demod_dvbs2_tune_params_t *psat = &priv->tp.u_tune.dvbs2;
	stm_fe_demod_channel_info_t *info =
					&priv->c_tinfo.u_info.tune.demod_info;
	stm_fe_demod_dvbs2_channel_info_t *isat = &info->u_channel.dvbs2;

	memset(&rp, '\0', sizeof(FE_STV0903_SearchResult_t));
	memset(&sp, '\0', sizeof(FE_STV0903_SearchParams_t));

	hdl = priv->demod_params;

	FILL_SAT_SEARCH_PARAMS_S1_S2(priv, std_in, sp, psat)

	if (psat->inv == STM_FE_DEMOD_SPECTRAL_INVERSION_OFF)
		sp.IQ_Inversion = FE_SAT_IQ_FORCE_NORMAL;
	else if (psat->inv == STM_FE_DEMOD_SPECTRAL_INVERSION_ON)
		sp.IQ_Inversion = FE_SAT_IQ_FORCE_SWAPPED;
	else if (psat->inv == STM_FE_DEMOD_SPECTRAL_INVERSION_AUTO)
		sp.IQ_Inversion = FE_SAT_IQ_AUTO;

	/*err=FE_STV0903_DVBS2_SetGoldCodeX((FE_STV0903_Handle_t)
	 *DemodHandle,0);*/
	if (std_in & STM_FE_DEMOD_TX_STD_S1_S2)
		RESET_INFO_SAT_S1_S2(isat)

	err = FE_STV0903_Search((FE_STV0903_Handle_t) hdl, &sp, &rp);
	if (!err && rp.Locked) {
		*lock = rp.Locked;
		info->std = stmfe_get_sat_standard(rp.Standard);
		if (info->std & STM_FE_DEMOD_TX_STD_S1_S2) {
			isat->freq = rp.Frequency;
			isat->sr = rp.SymbolRate;
			isat->fec = stmfe_get_sat_fec_rate(rp.Standard,
					    rp.PunctureRate, rp.ModCode);
			isat->inv = rp.Spectrum;
			isat->mod = stmfe_get_sat_modulation(info->std,
					isat->fec);
			isat->status = STM_FE_DEMOD_SYNC_OK;
		}
		if (info->std == STM_FE_DEMOD_TX_STD_DVBS2)
			isat->roll_off = stmfe_get_sat_roll_off(rp.RollOff);
	} else if (err == FE_LLA_SEARCH_FAILED) {
		if (info->std & STM_FE_DEMOD_TX_STD_S1_S2)
			isat->status = STM_FE_DEMOD_NO_SIGNAL;
	} else {
		if (info->std & STM_FE_DEMOD_TX_STD_S1_S2)
			isat->status = STM_FE_DEMOD_STATUS_UNKNOWN;
		ret = -EINVAL;
	}

	return ret;
}

/*
 * Name: stv0903_scan()
 *
 * Description: Provided scanning parameters to LLA and perfomed tuning
 *
 */
static int stv0903_scan(struct stm_fe_demod_s *priv, bool *lock)
{
	FE_STV0903_Error_t err;
	FE_STV0903_SearchParams_t sp;
	FE_STV0903_SearchResult_t rp;
	void *hdl;
	int ret = 0;
	stm_fe_demod_channel_info_t *info =
					&priv->c_tinfo.u_info.scan.demod_info;
	/*
	 * This is to exploit the characteristic of "union" for
	 * stm_fe_demod_dvbs_channel_info_t and
	 * stm_fe_demod_dvbs2_channel_info_t
	 */
	stm_fe_demod_dvbs2_channel_info_t *isat = &info->u_channel.dvbs2;

	memset(&rp, '\0', sizeof(FE_STV0903_SearchResult_t));
	memset(&sp, '\0', sizeof(FE_STV0903_SearchParams_t));

	hdl = priv->demod_params;
	sp.Frequency = priv->fe_scan.search_freq;
	sp.SearchRange = 2*1000*1000;
	sp.Path = FE_SAT_DEMOD_1;
	sp.Modulation = FE_SAT_MOD_UNKNOWN;
	sp.Modcode = FE_SAT_MODCODE_UNKNOWN;
	sp.SymbolRate = 100000;
	sp.Standard = FE_SAT_AUTO_SEARCH;
	sp.SearchAlgo = FE_SAT_BLIND_SEARCH;

	sp.PunctureRate =
		stmfe_set_sat_fec_rate(priv->scanparams.u_config.dvb_sat.fecs);
	sp.IQ_Inversion = FE_SAT_IQ_AUTO;

	/*err=FE_STV0903_DVBS2_SetGoldCodeX((FE_STV0903_Handle_t)
	 *DemodHandle,0);*/
	err = FE_STV0903_Search((FE_STV0903_Handle_t) hdl, &sp, &rp);
	if (!err && rp.Locked) {
		*lock = rp.Locked;
		info->std = stmfe_get_sat_standard(rp.Standard);
		if (info->std & STM_FE_DEMOD_TX_STD_S1_S2) {
			isat->freq = rp.Frequency;
			isat->sr = rp.SymbolRate;
			isat->fec = stmfe_get_sat_fec_rate(rp.Standard,
					rp.PunctureRate, rp.ModCode);
			isat->inv = rp.Spectrum;
			isat->mod = stmfe_get_sat_modulation(info->std,
					isat->fec);
			isat->roll_off = stmfe_get_sat_roll_off(rp.RollOff);
			isat->status = STM_FE_DEMOD_SYNC_OK;
		}
		priv->first_tunerinfo = true;
	} else if (err == FE_LLA_SEARCH_FAILED) {
		if (info->std & STM_FE_DEMOD_TX_STD_S1_S2)
			isat->status = STM_FE_DEMOD_NO_SIGNAL;
	} else {
		if (info->std & STM_FE_DEMOD_TX_STD_S1_S2)
			isat->status = STM_FE_DEMOD_STATUS_UNKNOWN;
		ret = -EINVAL;
	}

	return ret;
}



/*
 * Name: stv0903_tracking()
 *
 * Description: Checking the lock status of demodulator
 *
 */
static int stv0903_tracking(struct stm_fe_demod_s *priv)
{
	FE_STV0903_Error_t err;
	FE_STV0903_Handle_t hdl;
	FE_STV0903_SignalInfo_t si;
	uint32_t packetcount_err, packetscount_total;
	stm_fe_demod_channel_info_t *info =
					&priv->c_tinfo.u_info.tune.demod_info;
	stm_fe_demod_dvbs2_channel_info_t *isat = &info->u_channel.dvbs2;

	hdl = priv->demod_params;

	err = FE_STV0903_GetSignalInfo(hdl, &si);
	if (err) {
		if (info->std & STM_FE_DEMOD_TX_STD_S1_S2)
			RESET_INFO_SAT_S1_S2(isat)
		return 0;
	}

	if (si.Standard == FE_SAT_DVBS1_STANDARD) {
		info->std = STM_FE_DEMOD_TX_STD_DVBS;
		isat->per = si.BER;
	} else if (si.Standard == FE_SAT_DVBS2_STANDARD) {
		info->std = STM_FE_DEMOD_TX_STD_DVBS2;
		FE_STV903_GetPacketErrorRate(hdl, &packetcount_err,
				&packetscount_total);
		isat->per = packetcount_err;
		isat->roll_off = stmfe_get_sat_roll_off(si.RollOff);
	}
	if (info->std & STM_FE_DEMOD_TX_STD_S1_S2) {
		isat->freq = si.Frequency;
		isat->sr = si.SymbolRate;
		isat->fec = stmfe_get_sat_fec_rate(si.Standard, si.PunctureRate,
								si.ModCode);
		isat->inv = si.Spectrum;
		isat->mod = stmfe_get_sat_modulation(info->std, si.ModCode);
		/* Correction done to provide final snr in (db*10) */
		isat->snr = si.C_N;
		isat->signal_strength = si.Power;
		isat->status = STM_FE_DEMOD_SYNC_OK;
	}

	return 0;
}

static int stv0903_status(struct stm_fe_demod_s *priv, bool *locked)
{

	FE_STV0903_Handle_t hdl;
	stm_fe_demod_channel_info_t *info;
	stm_fe_demod_status_t *status;

	hdl = priv->demod_params;
	info = &priv->c_tinfo.u_info.tune.demod_info;
	*locked = FE_STV0903_Status(hdl);
	if (info->std == STM_FE_DEMOD_TX_STD_DVBS)
		status = &info->u_channel.dvbs.status;
	else
		status = &info->u_channel.dvbs2.status;

	if (*locked)
		*status = STM_FE_DEMOD_SYNC_OK;
	else
		*status = STM_FE_DEMOD_NO_SIGNAL;

	return 0;
}

static int stv0903_unlock(struct stm_fe_demod_s *priv)
{
	FE_STV0903_Error_t err;
	FE_STV0903_Handle_t hdl;
	int ret = 0;
	hdl = priv->demod_params;

	err = FE_STV0903_Unlock(hdl);
	if (err == FE_LLA_INVALID_HANDLE)
		ret = -EINVAL;
	else if (err == FE_LLA_I2C_ERROR)
		ret = -EFAULT;
	priv->c_tinfo.u_info.tune.demod_info.u_channel.dvbs.status =
		STM_FE_DEMOD_STATUS_UNKNOWN;
	priv->c_tinfo.u_info.tune.demod_info.u_channel.dvbs2.status =
		STM_FE_DEMOD_STATUS_UNKNOWN;

	return ret;
}

static int stv0903_abort(struct stm_fe_demod_s *priv, bool abort)
{
	FE_STV0903_Error_t err;
	FE_STV0903_Handle_t hdl;
	int ret = 0;
	hdl = priv->demod_params;
	err = FE_STV0903_SetAbortFlag(hdl, abort);
	if (err != FE_LLA_NO_ERROR) {
		pr_err("%s: failed\n", __func__);
		ret = -EINVAL;
	}

	return ret;
}

static int stv0903_standby(struct stm_fe_demod_s *priv, bool standby)
{
	FE_STV0903_Error_t err;
	FE_STV0903_Handle_t hdl;
	int ret = 0;

	hdl = priv->demod_params;

	err = FE_STV0903_SetStandby(hdl, standby);
	if (err != FE_LLA_NO_ERROR) {
		pr_err("%s: failed\n", __func__);
		ret = -EINVAL;
	}

	return ret;
}

/*
 * Name: stv0903_restore()
 *
 * Description: Re-Initiliazing LLA after restoring from CPS
 *
 */
static int stv0903_restore(struct stm_fe_demod_s *priv)
{
	FE_Sat_InitParams_t ip;
	FE_STV0903_TSConfig_t ts_conf;
	uint32_t ts_speed;
	int ret = 0;
	FE_STV0903_Error_t err;

	memset(&ip, '\0', sizeof(FE_Sat_InitParams_t));
	strncpy(ip.DemodName, "STV0903", sizeof(ip.DemodName));	/* Demod name*/
	ip.DemodI2cAddr = priv->config->demod_io.address;
	/* Refrence clock for the Demodulator in Hz (27MHz,30MHz,....) */
	ip.DemodRefClock = priv->config->clk_config.demod_clk;
	/* NYQUIST Filter value (used for DVBS1/DSS, DVBS2 is automatic) */
	ip.RollOff = priv->config->roll_off == DEMOD_ROLL_OFF_0_35 ? FE_SAT_35
								: FE_SAT_20;

	/*Path1 parameters */
	if (priv->config->custom_flags & DEMOD_CUSTOM_USE_AUTO_TUNER) {
		if (IS_NAME_MATCH(priv->config->tuner_name, "STB6000"))
			ip.TunerHWControlType = FE_SAT_AUTO_STB6000;
		else if (IS_NAME_MATCH(priv->config->tuner_name, "STV6110"))
			ip.TunerHWControlType = FE_SAT_AUTO_STV6110;
		else if (IS_NAME_MATCH(priv->config->tuner_name, "STB6100"))
			ip.TunerHWControlType = FE_SAT_AUTO_STB6100;
		else if ((IS_NAME_MATCH(priv->config->tuner_name, "STV6120_1"))
		   || (IS_NAME_MATCH(priv->config->tuner_name, "STV6120_2")))
			ip.TunerHWControlType = FE_SAT_AUTO_STV6120;
		else
			ip.TunerHWControlType = FE_SAT_SW_TUNER;
	} else if (priv->config->custom_flags == DEMOD_CUSTOM_NONE) {
		ip.TunerHWControlType = FE_SAT_SW_TUNER;
	}

	if (priv->config->ts_out & 0x1)
		ip.PathTSClock = FE_TS_SERIAL_PUNCT_CLOCK;
	else if ((priv->config->ts_out) & 0x2)
		ip.PathTSClock = FE_TS_SERIAL_PUNCT_CLOCK;

	ip.TunerModel = stmfe_set_tuner_model(priv->config->tuner_name);

	strncpy(ip.TunerName, "Tuner1", sizeof(ip.TunerName));
	ip.Tuner_I2cAddr = priv->config->tuner_io.address;
	ip.TunerRefClock = priv->config->clk_config.tuner_clk;
	ip.TunerOutClkDivider = priv->config->clk_config.tuner_clkout_divider;

	if (priv->config->custom_flags & DEMOD_CUSTOM_IQ_WIRING_INVERTED)
		ip.TunerIQ_Inversion = FE_SAT_IQ_SWAPPED;
	else
		ip.TunerIQ_Inversion = FE_SAT_IQ_NORMAL;


	if (priv->config->custom_flags & DEMOD_CUSTOM_ADD_ON_VGLNA)
		ip.TunerBasebandGain = 12;
	else
		ip.TunerBasebandGain = TUNER_BBGAIN(ip.TunerModel);

	if (ip.TunerModel == TUNER_STV6120_Tuner2)
		ip.TunerBandSelect = LBRFD_HBRFC;

	err = FE_STV0903_Init(&ip, &priv->demod_params);
	if (err != FE_LLA_NO_ERROR) {
		pr_err("%s: LLA initialization failed\n", __func__);
		return -EINVAL;
	}
	configure_ts_parameter(priv->config->ts_out, &ts_conf);

	ts_conf.TSClockRate = priv->config->ts_clock;

	err = FE_STV0903_SetTSConfig((FE_STV0903_Handle_t) priv->demod_params,
					&ts_conf, &ts_speed);
	if (err != FE_LLA_NO_ERROR) {
		pr_err("%s: TS config failed\n", __func__);
		ret = -EINVAL;
	}

	priv->c_tinfo.u_info.tune.demod_info.u_channel.dvbs.status =
		STM_FE_DEMOD_STATUS_UNKNOWN;
	priv->c_tinfo.u_info.tune.demod_info.u_channel.dvbs2.status =
		STM_FE_DEMOD_STATUS_UNKNOWN;

	return ret;
}

static int32_t __init stv0903_lla_init(void)
{
	pr_info("Loading stv0903 driver module...\n");

	return 0;
}

module_init(stv0903_lla_init);

static void __exit stv0903_lla_term(void)
{
	pr_info("Removing stv0903 driver module ...\n");

}

module_exit(stv0903_lla_term);

MODULE_DESCRIPTION("Low level driver for STV0903 satellite demodulator");
MODULE_AUTHOR("STMicroelectronics Limited");
MODULE_VERSION(STV0903_CURRENT_LLA_REVISION);
MODULE_LICENSE("GPL");
