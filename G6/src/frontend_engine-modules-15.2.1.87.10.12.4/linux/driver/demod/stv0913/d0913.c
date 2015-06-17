/************************************************************************
Copyright(C) 2013 STMicroelectronics. All Rights Reserved.

This file is part of the stm_fe Library.

stm_fe is free software ; you can redistribute it and/or modify it under
the terms of the GNU General Public License version 2 as published by the
Free Software Foundation.

stm_fe is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY ; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with stm_fe ; see the file COPYING.  If not, write to the Free Software
Foundation, 59 Temple Place - Suite 330, Boston, MA 02111 - 1307, USA.

The stm_fe Library may alternatively be licensed under a proprietary
license from ST.

Source file name : d0913.c
Author :	   Hemendra Singh

Wrapper for STV0913 LLA

Date		Modification		Name
---------	------------		--------
3-June-13	Created			 HS

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
#include "stv0913_drv.h"
#include "d0913.h"
#include <fe_sat_tuner.h>
#include "stv0913_init.h"

#define STV0913_CURRENT_LLA_REVISION "STV0913-LLA_REL_2.7"

#define LLA_DEFAULT 0

static int lla_wrapper_debug;
module_param_named(lla_wrapper_debug, lla_wrapper_debug, int, 0644);
MODULE_PARM_DESC(lla_wrapper_debug,
		 "Turn on/off stv0913 lla wrapper debugging (default:off).");
#define dpr_info(x...) do { if (lla_wrapper_debug) pr_info(x); } while (0)
#define dpr_err(x...) do { if (lla_wrapper_debug) pr_err(x); } while (0)
#define THIS_PATH(p) (FE_SAT_DEMOD_1)

static int stv0913_init(struct stm_fe_demod_s *priv);
static int stv0913_tune(struct stm_fe_demod_s *priv, bool *lock);
static int stv0913_scan(struct stm_fe_demod_s *priv, bool *lock);
static int stv0913_status(struct stm_fe_demod_s *priv, bool *locked);
static int stv0913_tracking(struct stm_fe_demod_s *priv);
static int stv0913_unlock(struct stm_fe_demod_s *priv);
static int stv0913_term(struct stm_fe_demod_s *demod_priv);
static int stv0913_abort(struct stm_fe_demod_s *priv, bool abort);
static int stv0913_standby(struct stm_fe_demod_s *priv, bool standby);
static int stv0913_restore(struct stm_fe_demod_s *priv);

/*
 * Name: stm_fe_stv0913_attach()
 *
 * Description: Installed the wrapper function pointers for LLA
 *
 */
int stm_fe_stv0913_attach(struct stm_fe_demod_s *priv)
{
	if (!priv->ops)
		return -EFAULT;

	priv->ops->init = stv0913_init;
	priv->ops->tune = stv0913_tune;
	priv->ops->scan = stv0913_scan;
	priv->ops->tracking = stv0913_tracking;
	priv->ops->status = stv0913_status;
	priv->ops->unlock = stv0913_unlock;
	priv->ops->term = stv0913_term;
	priv->ops->abort = stv0913_abort;
	priv->ops->standby = stv0913_standby;
	priv->ops->detach = stm_fe_stv0913_detach;
	priv->ops->restore = stv0913_restore;
	priv->ops->tsclient_init = NULL;
	priv->ops->tsclient_term = NULL;
	priv->ops->tsclient_pid_configure = NULL;
	priv->ops->tsclient_pid_deconfigure = NULL;

	dpr_info("%s: demod attached\n", __func__);

	return 0;
}
EXPORT_SYMBOL(stm_fe_stv0913_attach);

/*
 * Name: stm_fe_stv0913_detach()
 *
 * Description: Uninstalled the wrapper function pointers for LLA
 *
 */
int stm_fe_stv0913_detach(struct stm_fe_demod_s *priv)
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

	dpr_info("%s: demod detached\n", __func__);

	return 0;
}
EXPORT_SYMBOL(stm_fe_stv0913_detach);


static
void config_ts(enum demod_tsout_config_e tsout, FE_STV0913_TSConfig_t *conf)
{
	if (tsout & DEMOD_TS_SERIAL_PUNCT_CLOCK)
		conf->TSMode = FE_TS_SERIAL_PUNCT_CLOCK;
	else if (tsout & DEMOD_TS_SERIAL_CONT_CLOCK)
		conf->TSMode = FE_TS_SERIAL_CONT_CLOCK;
	else if (tsout & DEMOD_TS_PARALLEL_PUNCT_CLOCK)
		conf->TSMode = FE_TS_PARALLEL_PUNCT_CLOCK;
	else if (tsout & DEMOD_TS_PARALLEL_PUNCT_CLOCK)
		conf->TSMode = FE_TS_PARALLEL_PUNCT_CLOCK;
	else if (tsout & DEMOD_TS_DVBCI_CLOCK)
		conf->TSMode = FE_TS_DVBCI_CLOCK;

	conf->TSSpeedControl = (tsout & DEMOD_TS_MANUAL_SPEED) ?
		FE_TS_MANUAL_SPEED : FE_TS_AUTO_SPEED;

	conf->TSClockPolarity = (tsout & DEMOD_TS_RISINGEDGE_CLOCK) ?
		FE_TS_RISINGEDGE_CLOCK : FE_TS_FALLINGEDGE_CLOCK;

	conf->TSSyncByteEnable = (tsout & DEMOD_TS_SYNCBYTE_ON) ?
		FE_TS_SYNCBYTE_ON : FE_TS_SYNCBYTE_OFF;

	conf->TSParityBytes = (tsout & DEMOD_TS_PARITYBYTES_ON) ?
		FE_TS_PARITYBYTES_ON : FE_TS_PARITYBYTES_OFF;

	conf->TSSwap = (tsout & DEMOD_TS_SWAP_ON) ?
		FE_TS_SWAP_ON : FE_TS_SWAP_OFF;

	conf->TSSmoother = (tsout & DEMOD_TS_SMOOTHER_ON) ?
		FE_TS_SMOOTHER_ON : FE_TS_SMOOTHER_OFF;
}

/*
 * Name: stv0913_init()
 *
 * Description: Providing Initiliazing parameters to LLA
 *
 */
static int stv0913_init(struct stm_fe_demod_s *priv)
{
	FE_STV0913_InternalParams_t *p = NULL;
	FE_Sat_InitParams_t ip;
	struct demod_config_s *conf = priv->config;
	FE_STV0913_TSConfig_t ts_conf;
	uint32_t ts_speed;
	int ret = 0;
	FE_STV0913_Error_t err;
	size_t size;
	bool is_auto_tuner = (conf->custom_flags & DEMOD_CUSTOM_USE_AUTO_TUNER);
	bool is_iq_inv = (conf->custom_flags & DEMOD_CUSTOM_IQ_WIRING_INVERTED);
	bool is_vglna = (conf->custom_flags & DEMOD_CUSTOM_ADD_ON_VGLNA);

	dpr_info("%s: auto_tuner = %d, iq_inv = %d, vglna = %d\n", __func__,
					   is_auto_tuner, is_iq_inv, is_vglna);
	/* LLA init p from config */
	memset(&ip, 0, sizeof(FE_Sat_InitParams_t));
	strncpy(ip.DemodName, "STV0913", sizeof(ip.DemodName));

	ip.DemodI2cAddr = conf->demod_io.address;
	ip.DemodRefClock = conf->clk_config.demod_clk;
	ip.DemodType = FE_SAT_SINGLE;
	ip.RollOff = stmfe_set_sat_roll_off(conf->roll_off);
	dpr_info("%s: DemodI2Caddress = 0x%x DemodRefclock = %d\n", __func__,
							ip.DemodI2cAddr,
							ip.DemodRefClock);
	ip.TunerHWControlType = stmfe_set_sat_tuner_type(conf->tuner_name,
								is_auto_tuner);
	ip.PathTSClock = stmfe_set_ts_output_mode(conf->ts_out);
	ip.TunerModel = stmfe_set_tuner_model(conf->tuner_name);
	strncpy(ip.TunerName, "Tuner1", sizeof(ip.TunerName));
	ip.Tuner_I2cAddr = conf->tuner_io.address;
	ip.TunerRefClock = conf->clk_config.tuner_clk;
	ip.TunerOutClkDivider = conf->clk_config.tuner_clkout_divider;
	ip.TunerIQ_Inversion = stmfe_set_tuner_iq_inv(is_iq_inv);
	ip.TunerBasebandGain = stmfe_set_tuner_bbgain(is_vglna, ip.TunerModel);
	ip.TunerBandSelect = stmfe_set_tuner_band(ip.TunerModel);
	ip.TunerRF_Source = FE_RF_SOURCE_C;
	ip.TunerInputSelect = TUNER_INPUT2;

	dpr_info("%s: tuner p for demod:\n"
			   "Tuner model = %s\nTuner I2C Address = 0x%x\n"
			   "Tuner Refclock = %d\nTunerRF_Source = %d\n"
			   "TunerInputSelect = %d\n", __func__,
			   conf->tuner_name, ip.Tuner_I2cAddr,
			   ip.TunerRefClock, ip.TunerRF_Source,
			   ip.TunerInputSelect);

	p = stm_fe_malloc(sizeof(FE_STV0913_InternalParams_t));
	if (!p) {
		pr_err("%s: mem alloc failed for stv0913 params\n", __func__);
		return -ENOMEM;
	}

	config_ts(conf->ts_out, &ts_conf);

	priv->demod_i2c = chip_open(conf->demod_io.bus);
	if (!priv->demod_i2c) {
		pr_err("%s: failed to get I2C adaptor\n", __func__);
		stm_fe_free((void **) &p);
		return -EINVAL;
	}

	size = STV0913_NBREGS * sizeof(STCHIP_Register_t);
	priv->demod_h->pRegMapImage = stm_fe_malloc(size);
	if (!priv->demod_h->pRegMapImage) {
		priv->demod_i2c = NULL;
		stm_fe_free((void **) &p);
		pr_err("%s: mem alloc failed for Demod reg map\n", __func__);
		return -ENOMEM;
	}

	/*To enable the auto repeater mode */
	priv->demod_h->IsAutoRepeaterOffEnable = true;
	priv->demod_h->Abort = false;
	priv->tuner_h->Abort = false;
	priv->demod_params = (void *)p;

	/*initialise demod/tuner chip structures */
	p->hDemod = priv->demod_h;
	p->hDemod->dev_i2c = priv->demod_i2c;
	p->hDemod->I2cAddr = conf->demod_io.address;
	p->hDemod->IORoute = conf->demod_io.route;
	p->hDemod->IODriver = conf->demod_io.io;

	p->hTuner = priv->tuner_h;
	p->hTuner->dev_i2c = priv->demod_i2c;
	p->hTuner->I2cAddr = conf->tuner_io.address;
	p->hTuner->IORoute = conf->tuner_io.route;
	p->hTuner->IODriver = conf->tuner_io.io;

	err = FE_STV0913_Init(&ip, &priv->demod_params);
	if (err != FE_LLA_NO_ERROR) {
		pr_err("%s: FE_STV0913_Init failed: %d\n", __func__, err);
		ret = -EINVAL;
		goto err_ret;
	}
	dpr_info("%s: TS config for stv0913:\n", __func__);
	dpr_info("%s: TSClockPolarity = %d\nTSSyncByteEnable = %d\n"
			 "TSSwap = %d\n", __func__, ts_conf.TSClockPolarity,
			 ts_conf.TSSyncByteEnable, ts_conf.TSSwap);

	err = FE_STV0913_SetTSoutput(p, &ts_conf);
	if (err != FE_LLA_NO_ERROR) {
		pr_err("%s: STV0913_SetTSoutput failed: %d\n", __func__, err);
		ret = -EINVAL;
		goto err_ret;
	}

	err = FE_STV0913_SetTSConfig(p, &ts_conf, &ts_speed);
	if (err != FE_LLA_NO_ERROR) {
		pr_err("%s: STV0913_SetTSConfig failed: %d\n", __func__, err);
		ret = -EINVAL;
		goto err_ret;
	}

	dpr_info("%s: successful\n", __func__);

	return 0;

err_ret:
	stm_fe_free((void **) &priv->demod_h->pRegMapImage);
	priv->demod_i2c = NULL;
	priv->demod_params = NULL;
	stm_fe_free((void **) &p);
	return ret;
}

/*
 * Name: stv0913_term()
 *
 * Description: terminating the stv0913 LLA
 *
 */
static int stv0913_term(struct stm_fe_demod_s *priv)
{
	int ret = -1;

	priv->demod_i2c = NULL;
	priv->demod_h->dev_i2c = NULL;
	priv->tuner_h->dev_i2c = NULL;
	ret = stm_fe_free((void **) &priv->demod_h->pRegMapImage);
	if (ret) {
		pr_err("%s: tried to free NULL ptr RegImage\n", __func__);
		return ret;
	}

	ret = stm_fe_free((void **) &priv->demod_params);
	if (ret) {
		pr_err("%s: tried to free NULL ptr demod_params\n", __func__);
		return ret;
	}

	dpr_info("%s: lla terminated\n", __func__);
	return ret;
}

/*
 * Name: stv0913_tune()
 *
 * Description: Provided tuning parameters to LLA and perfomed tuning
 *
 */
static int stv0913_tune(struct stm_fe_demod_s *priv, bool *lock)
{
	FE_STV0913_Error_t err;
	FE_STV0913_SearchParams_t sp;
	FE_STV0913_SearchResult_t rp;
	FE_STV0913_Handle_t hdl;
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

	memset(&rp, '\0', sizeof(FE_STV0913_SearchResult_t));
	memset(&sp, '\0', sizeof(FE_STV0913_SearchParams_t));

	hdl = priv->demod_params;

	FILL_SAT_SEARCH_PARAMS_S1_S2(priv, std_in, sp, psat)

	dpr_info("%s: Search params filled are:\n", __func__);
	dpr_info("%s: Freq = %d KHz\nSymbolRate = %d bds\n"
			  "SearchRange = %d Hz\n", __func__,
			  sp.Frequency, sp.SymbolRate, sp.SearchRange);

	if (std_in & STM_FE_DEMOD_TX_STD_S1_S2)
		RESET_INFO_SAT_S1_S2(isat)

	err = FE_STV0913_Search((FE_STV0913_Handle_t) hdl, &sp, &rp, 0);
	if (!err && rp.Locked) {
		*lock = rp.Locked;
		dpr_info("%s: demod %s locked\n", __func__, priv->demod_name);
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
		dpr_info("%s: Result params after lock :\n", __func__);
		dpr_info("%s: Freq = %d KHz\nSymbolRate = %d bds\n",
				   __func__, rp.Frequency, rp.SymbolRate);

	} else if (err == FE_LLA_SEARCH_FAILED) {
		if (info->std & STM_FE_DEMOD_TX_STD_S1_S2)
			isat->status = STM_FE_DEMOD_NO_SIGNAL;
		dpr_info("%s: tune fail as lla search failed\n", __func__);
	} else {
		if (info->std & STM_FE_DEMOD_TX_STD_S1_S2)
			isat->status = STM_FE_DEMOD_STATUS_UNKNOWN;
		ret = -EINVAL;
		dpr_err("%s: tune failed status unknown\n", __func__);
	}

	return ret;
}

/*
 * Name: stv0913_scan()
 *
 * Description: Provided scanning parameters to LLA and perfomed tuning
 *
 */
static int stv0913_scan(struct stm_fe_demod_s *priv, bool *lock)
{
	FE_STV0913_Error_t err;
	FE_STV0913_SearchParams_t sp;
	FE_STV0913_SearchResult_t rp;
	void *hdl;
	int ret = 0;
	stm_fe_demod_channel_info_t *info =
		&priv->c_tinfo.u_info.tune.demod_info;
	/*
	 * This is to exploit the characteristic of "union" for
	 * stm_fe_demod_dvbs_channel_info_t and
	 * stm_fe_demod_dvbs2_channel_info_t
	 */
	stm_fe_demod_dvbs2_channel_info_t *isat = &info->u_channel.dvbs2;

	memset(&rp, '\0', sizeof(FE_STV0913_SearchResult_t));
	memset(&sp, '\0', sizeof(FE_STV0913_SearchParams_t));

	hdl = priv->demod_params;
	sp.Frequency = priv->fe_scan.search_freq;
	sp.SearchRange = 2 * 1000 * 1000;
	sp.Modulation = FE_SAT_MOD_UNKNOWN;
	sp.Modcode = FE_SAT_MODCODE_UNKNOWN;
	sp.SymbolRate = 100000;
	sp.Standard = FE_SAT_AUTO_SEARCH;
	sp.SearchAlgo = FE_SAT_BLIND_SEARCH;

	sp.PunctureRate =
	     stmfe_set_sat_fec_rate(priv->scanparams.u_config.dvb_sat.fecs);
	sp.IQ_Inversion = FE_SAT_IQ_AUTO;

	dpr_info("%s: scan params are:\n", __func__);
	dpr_info("%s: Freq = %d KHz\nSearchRange = %d Hz\n"
			  "SymbolRate = %d bds\n", __func__,
			  sp.Frequency, sp.SearchRange, sp.SymbolRate);

	err = FE_STV0913_Search((FE_STV0913_Handle_t) hdl, &sp, &rp, 0);
	if (!err && rp.Locked) {
		*lock = rp.Locked;
		dpr_info("%s: scan successful with scan params\n", __func__);
		dpr_info("%s: Freq locked = %d KHz\n"
				  "SymbolRate = %d bds\n", __func__,
				  rp.Frequency, rp.SymbolRate);

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
	} else if (err == FE_LLA_SEARCH_FAILED) {
		if (info->std & STM_FE_DEMOD_TX_STD_S1_S2)
			isat->status = STM_FE_DEMOD_NO_SIGNAL;
		 dpr_info("%s: scan fail as lla search failed\n", __func__);
	} else {
		if (info->std & STM_FE_DEMOD_TX_STD_S1_S2)
			isat->status = STM_FE_DEMOD_STATUS_UNKNOWN;
		dpr_err("%s: scan failed status unknown\n", __func__);
		ret = -EINVAL;
	}

	return ret;
}

/*
 * Name: stv0913_tracking()
 *
 * Description: Checking the lock status of demodulator
 *
 */
static int stv0913_tracking(struct stm_fe_demod_s *priv)
{
	FE_STV0913_Error_t err;
	FE_STV0913_Handle_t hdl;
	FE_STV0913_SignalInfo_t sinfo;
	uint32_t packetcount_err, packetscount_total;
	stm_fe_demod_channel_info_t *info =
		&priv->c_tinfo.u_info.tune.demod_info;
	stm_fe_demod_dvbs2_channel_info_t *isat = &info->u_channel.dvbs2;

	hdl = priv->demod_params;
	err = FE_LLA_NO_ERROR;
	err |= FE_STV0913_GetSignalInfo(hdl, &sinfo, 1);
	if (!(err)) {
		if (sinfo.Standard == FE_SAT_DVBS1_STANDARD) {
			info->std = STM_FE_DEMOD_TX_STD_DVBS;
			isat->per = sinfo.BER;
		} else if (sinfo.Standard == FE_SAT_DVBS2_STANDARD) {
			info->std = STM_FE_DEMOD_TX_STD_DVBS2;
			FE_STV913_GetPacketErrorRate(hdl, &packetcount_err,
							 &packetscount_total);
			isat->per = packetcount_err;
			isat->roll_off = stmfe_get_sat_roll_off(sinfo.RollOff);
		}
		if (info->std & STM_FE_DEMOD_TX_STD_S1_S2) {
			isat->freq = sinfo.Frequency;
			isat->sr = sinfo.SymbolRate;
			isat->fec = stmfe_get_sat_fec_rate(sinfo.Standard,
					sinfo.PunctureRate, sinfo.ModCode);
			isat->inv = sinfo.Spectrum;
			isat->mod = stmfe_get_sat_modulation(info->std,
						  sinfo.ModCode);
			/* Correction done to provide final snr in (db*10) */
			isat->snr = sinfo.C_N;
			isat->signal_strength = sinfo.Power;
			isat->status = STM_FE_DEMOD_SYNC_OK;
			dpr_info("%s: Freq = %d KHz\nSymbolRate = %d bds\n",
					__func__, isat->freq, isat->sr);
			dpr_info("%s: SNR = %d SignalStrength = %d\n",
						__func__, isat->snr,
						isat->signal_strength);
		}
	} else {
		if (info->std & STM_FE_DEMOD_TX_STD_S1_S2)
			RESET_INFO_SAT_S1_S2(isat)
	}

	return 0;
}

static int stv0913_status(struct stm_fe_demod_s *priv, bool *locked)
{

	FE_STV0913_Handle_t hdl;
	stm_fe_demod_channel_info_t *info;
	stm_fe_demod_status_t *status;
	uint8_t i = 0;

	hdl = priv->demod_params;
	info = &priv->c_tinfo.u_info.tune.demod_info;
	do {
		*locked = FE_STV0913_Status(hdl);
		WAIT_N_MS(20);
		i++;
	} while ((i < 10) && (*locked == FALSE));

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


static int stv0913_unlock(struct stm_fe_demod_s *priv)
{
	FE_STV0913_Error_t err;
	FE_STV0913_Handle_t hdl;
	int ret = 0;
	stm_fe_demod_channel_info_t *info;

	hdl = priv->demod_params;
	info = &priv->c_tinfo.u_info.tune.demod_info;

	err = FE_STV0913_Unlock(hdl);
	if (err == FE_LLA_INVALID_HANDLE) {
		dpr_err("%s: unlock failed due to invalid handle\n", __func__);
		ret = -EINVAL;
	} else if (err == FE_LLA_I2C_ERROR) {
		dpr_err("%s: unlock failed due to I2C err\n", __func__);
		ret = -EFAULT;
	}

	if (info->std == STM_FE_DEMOD_TX_STD_DVBS)
		info->u_channel.dvbs.status = STM_FE_DEMOD_STATUS_UNKNOWN;
	else
		info->u_channel.dvbs2.status = STM_FE_DEMOD_STATUS_UNKNOWN;

	return ret;
}

static int stv0913_abort(struct stm_fe_demod_s *priv, bool abort)
{
	FE_STV0913_Error_t err;
	FE_STV0913_Handle_t hdl;
	int ret = 0;
	hdl = priv->demod_params;

	err = FE_STV0913_SetAbortFlag(hdl, abort);
	if (err != FE_LLA_NO_ERROR) {
		pr_err("%s: failed\n", __func__);
		ret = -EINVAL;
	} else {
		dpr_info("%s: successful\n", __func__);
	}
	return ret;
}

static int stv0913_standby(struct stm_fe_demod_s *priv, bool standby)
{
	FE_STV0913_Error_t err;
	FE_STV0913_Handle_t hdl;
	int ret = 0;

	hdl = priv->demod_params;
	err = FE_STV0913_SetStandby(hdl, standby);
	if (err != FE_LLA_NO_ERROR) {
		pr_err("%s: failed\n", __func__);
		ret = -EINVAL;
	} else {
		dpr_info("%s: successful\n", __func__);
	}

	return ret;
}

/*
 * Name: stv0913_restore()
 *
 * Description: Re - Initiliazing LLA after restoring from CPS
 *
 */
static int stv0913_restore(struct stm_fe_demod_s *priv)
{
	FE_Sat_InitParams_t ip;
	struct demod_config_s *conf = priv->config;
	FE_STV0913_TSConfig_t ts_conf;
	uint32_t p1_ts_speed;
	int ret = 0;
	FE_STV0913_Error_t err;
	bool is_auto_tuner = (conf->custom_flags & DEMOD_CUSTOM_USE_AUTO_TUNER);
	bool is_iq_inv = (conf->custom_flags & DEMOD_CUSTOM_IQ_WIRING_INVERTED);
	bool is_vglna = (conf->custom_flags & DEMOD_CUSTOM_ADD_ON_VGLNA);

	dpr_info("%s: auto_tuner = %d, iq_inv = %d, vglna =%d\n", __func__,
					is_auto_tuner, is_iq_inv, is_vglna);
	/* LLA init params from config */
	memset(&ip, 0, sizeof(FE_Sat_InitParams_t));
	strncpy(ip.DemodName, "STV0913", sizeof(ip.DemodName));

	dpr_info("%s: Reinitializing LLA after restring from CPS\n", __func__);

	ip.DemodI2cAddr = conf->demod_io.address;
	ip.DemodRefClock = conf->clk_config.demod_clk;
	ip.DemodType = FE_SAT_SINGLE;
	ip.RollOff = stmfe_set_sat_roll_off(conf->roll_off);

	dpr_info("%s: DemodI2Caddress is = 0x%x\nDemodRefclock is = %d\n",
				__func__, ip.DemodI2cAddr, ip.DemodRefClock);

	ip.TunerHWControlType = stmfe_set_sat_tuner_type(conf->tuner_name,
								is_auto_tuner);
	ip.PathTSClock = stmfe_set_ts_output_mode(conf->ts_out);
	ip.TunerModel = stmfe_set_tuner_model(conf->tuner_name);
	strncpy(ip.TunerName, "Tuner1", sizeof(ip.TunerName));
	ip.Tuner_I2cAddr = conf->tuner_io.address;
	ip.TunerRefClock = conf->clk_config.tuner_clk;
	ip.TunerOutClkDivider = conf->clk_config.tuner_clkout_divider;
	ip.TunerIQ_Inversion = stmfe_set_tuner_iq_inv(is_iq_inv);
	ip.TunerBasebandGain = stmfe_set_tuner_bbgain(is_vglna, ip.TunerModel);
	ip.TunerBandSelect = stmfe_set_tuner_band(ip.TunerModel);
	ip.TunerRF_Source = FE_RF_SOURCE_C;
	ip.TunerInputSelect = TUNER_INPUT2;
	dpr_info("%s: tuner params for demod:\n"
			  "Tuner model = %s\n Tuner I2C Address = %d\n"
			  "Tuner Refclock = %d\n TunerRF_Source = %d\n"
			  "TunerInputSelect = %d\n", __func__,
			  conf->tuner_name, ip.Tuner_I2cAddr,
			  ip.TunerRefClock, ip.TunerRF_Source,
			  ip.TunerInputSelect);

	config_ts(conf->ts_out, &ts_conf);


	err = FE_STV0913_Init(&ip, &priv->demod_params);
	if (err != FE_LLA_NO_ERROR) {
		pr_err("%s: FE_STV0913_Init failed: %d\n", __func__, err);
		ret = -EINVAL;
		goto err_ret;
	}

	dpr_info("%s: TS config for Demod:\n", __func__);
	dpr_info("%s: TSClockPolarity = %d\nTSSyncByteEnable = %d\n"
			  "TSSwap = %d\n", __func__, ts_conf.TSClockPolarity,
			  ts_conf.TSSyncByteEnable, ts_conf.TSSwap);

	err = FE_STV0913_SetTSoutput(priv->demod_params, &ts_conf);
	if (err != FE_LLA_NO_ERROR) {
		pr_err("%s: STV0913_SetTSoutput failed: %d\n", __func__, err);
		ret = -EINVAL;
		goto err_ret;
	}

	err = FE_STV0913_SetTSConfig(priv->demod_params, &ts_conf,
			&p1_ts_speed);
	if (err != FE_LLA_NO_ERROR) {
		pr_err("%s: STV0913_SetTSConfig failed: %d\n", __func__, err);
		ret = -EINVAL;
		goto err_ret;
	}
	dpr_info("%s: init successful after restoring from CPS\n", __func__);

	return 0;

err_ret:
	return ret;
}
static int32_t __init stv0913_lla_init(void)
{
	pr_info("Loading stv0913 driver module...\n");
	return 0;
}
module_init(stv0913_lla_init);

static void __exit stv0913_lla_term(void)
{
	pr_info("Removing stv0913 driver module ...\n");
}
module_exit(stv0913_lla_term);

MODULE_DESCRIPTION("Low level driver for STV0913 satellite demodulator");
MODULE_AUTHOR("STMicroelectronics Limited");
MODULE_VERSION(STV0913_CURRENT_LLA_REVISION);
MODULE_LICENSE("GPL");
