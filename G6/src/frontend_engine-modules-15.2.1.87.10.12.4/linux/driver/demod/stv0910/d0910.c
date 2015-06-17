/************************************************************************
Copyright (C) 2012 STMicroelectronics. All Rights Reserved.

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

Source file name : d0910.c
Author :           Rahul

Wrapper for STV0910 LLA

Date        Modification                                    Name
----        ------------                                    --------
23-Apr-12   Created                                         Rahul

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
#include "stv0910_drv.h"
#include "d0910.h"
#include <fe_sat_tuner.h>
#include <stv0910_init.h>

#define STV0910_CURRENT_LLA_REVISION "STV0910-LLA_REL_2.6_July_2013"

#define THIS_PATH(p) (stv0910_get_path(p->config))

#define LLA_DEFAULT 0

static int lla_wrapper_debug;
module_param_named(lla_wrapper_debug, lla_wrapper_debug, int, 0644);
MODULE_PARM_DESC(lla_wrapper_debug,
		"Turn on/off stv0910 lla wrapper debugging (default:off).");
#define dpr_info(x...) do { if (lla_wrapper_debug) pr_info(x); } while (0)
#define dpr_err(x...) do { if (lla_wrapper_debug) pr_err(x); } while (0)

static int stv0910_init(struct stm_fe_demod_s *priv);
static int stv0910_tune(struct stm_fe_demod_s *priv, bool *lock);
static int stv0910_scan(struct stm_fe_demod_s *priv, bool *lock);
static int stv0910_status(struct stm_fe_demod_s *priv, bool *locked);
static int stv0910_tracking(struct stm_fe_demod_s *priv);
static int stv0910_unlock(struct stm_fe_demod_s *priv);
static int stv0910_term(struct stm_fe_demod_s *priv);
static int stv0910_abort(struct stm_fe_demod_s *priv, bool abort);
static int stv0910_standby(struct stm_fe_demod_s *priv, bool standby);
static int stv0910_restore(struct stm_fe_demod_s *priv);

/*
 * Name: stm_fe_stv0910_attach()
 *
 * Description: Installed the wrapper function pointers for LLA
 *
 */
int stm_fe_stv0910_attach(struct stm_fe_demod_s *priv)
{
	if (!priv->ops)
		return -EFAULT;

	priv->ops->init = stv0910_init;
	priv->ops->tune = stv0910_tune;
	priv->ops->scan = stv0910_scan;
	priv->ops->tracking = stv0910_tracking;
	priv->ops->status = stv0910_status;
	priv->ops->unlock = stv0910_unlock;
	priv->ops->term = stv0910_term;
	priv->ops->abort = stv0910_abort;
	priv->ops->standby = stv0910_standby;
	priv->ops->detach = stm_fe_stv0910_detach;
	priv->ops->restore = stv0910_restore;
	priv->ops->tsclient_init = NULL;
	priv->ops->tsclient_term = NULL;
	priv->ops->tsclient_pid_configure = NULL;
	priv->ops->tsclient_pid_deconfigure = NULL;
	dpr_info("%s: demod attached", __func__);

	return 0;
}
EXPORT_SYMBOL(stm_fe_stv0910_attach);

/*
 * Name: stm_fe_stv0910_detach()
 *
 * Description: Uninstalled the wrapper function pointers for LLA
 *
 */
int stm_fe_stv0910_detach(struct stm_fe_demod_s *priv)
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
	dpr_info("%s: demod detached", __func__);

	return 0;
}
EXPORT_SYMBOL(stm_fe_stv0910_detach);

static int stv0910_get_path(struct demod_config_s *conf)
{
	if (IS_NAME_MATCH(conf->demod_name, "STV0910P1")) {
		dpr_info("%s: path selected is STV0910P1", __func__);
		return FE_SAT_DEMOD_1;
	} else {
		dpr_info("%s: path selected is STV0910P2", __func__);
		return FE_SAT_DEMOD_2;
	}
}

static
struct stm_fe_demod_s *get_demod_from_dev_name(struct stm_fe_demod_s *priv,
								char *name)
{
	struct list_head *pos;
	list_for_each(pos, priv->list_h) {
		struct stm_fe_demod_s *assoc_demod;
		assoc_demod = list_entry(pos, struct stm_fe_demod_s, list);
		if (IS_NAME_MATCH(assoc_demod->config->demod_name, name)
		    && (assoc_demod->config->demod_io.address ==
					priv->config->demod_io.address)
		    && (assoc_demod->config->demod_io.bus ==
					priv->config->demod_io.bus))
			return assoc_demod;
	}

	return NULL;
}

/*
 * Name: stv0910_init()
 *
 * Description: Providing Initiliazing parameters to LLA
 *
 */
static int stv0910_init(struct stm_fe_demod_s *priv)
{
	FE_STV0910_InternalParams_t *p = NULL;
	FE_Sat_InitParams_t ip;
	struct demod_config_s *conf = priv->config;
	FE_STV0910_TSConfig_t p1_ts_conf, p2_ts_conf;
	uint32_t p1_ts_speed, p2_ts_speed;
	int ret = 0, path = stv0910_get_path(conf);
	FE_STV0910_Error_t err;
	size_t size;
	bool is_auto_tuner = (conf->custom_flags & DEMOD_CUSTOM_USE_AUTO_TUNER);
	bool is_iq_inv = (conf->custom_flags & DEMOD_CUSTOM_IQ_WIRING_INVERTED);
	bool is_vglna = (conf->custom_flags & DEMOD_CUSTOM_ADD_ON_VGLNA);
	struct stm_fe_demod_s *assoc_priv = NULL;
	dpr_info("%s: auto_tuner = %d, iq_inv = %d, vglna = %d\n", __func__,
					    is_auto_tuner, is_iq_inv, is_vglna);
	/* LLA init params from config */
	memset(&ip, 0, sizeof(FE_Sat_InitParams_t));
	strncpy(ip.DemodName, "STV0910", sizeof(ip.DemodName));

	ip.DemodI2cAddr = conf->demod_io.address;
	ip.DemodRefClock = conf->clk_config.demod_clk;
	ip.DemodType = FE_SAT_DUAL;
	ip.RollOff = stmfe_set_sat_roll_off(conf->roll_off);
	dpr_info("%s: DemodI2Caddress = 0x%x DemodRefclock = %d\n", __func__,
			ip.DemodI2cAddr, ip.DemodRefClock);
	if (path == FE_SAT_DEMOD_1) {
		ip.TunerHWControlType =
		      stmfe_set_sat_tuner_type(conf->tuner_name, is_auto_tuner);
		ip.PathTSClock = stmfe_set_ts_output_mode(conf->ts_out);
		ip.TunerModel = stmfe_set_tuner_model(conf->tuner_name);
		strncpy(ip.TunerName, "Tuner1", sizeof(ip.TunerName));
		ip.Tuner_I2cAddr = conf->tuner_io.address;
		ip.TunerRefClock = conf->clk_config.tuner_clk;
		ip.TunerOutClkDivider = conf->clk_config.tuner_clkout_divider;
		ip.TunerIQ_Inversion = stmfe_set_tuner_iq_inv(is_iq_inv);
		ip.TunerBasebandGain = stmfe_set_tuner_bbgain(is_vglna,
								ip.TunerModel);
		ip.TunerBandSelect = stmfe_set_tuner_band(ip.TunerModel);
		ip.TunerRF_Source = FE_RF_SOURCE_C;
		ip.TunerInputSelect = TUNER_INPUT2;
		dpr_info("%s: tuner params for demod 0:\n"
				  "Tuner model = %s\nTuner I2C Address = 0x%x\n"
				  "Tuner Refclock = %d\nTunerRF_Source = %d\n"
				  "TunerInputSelect = %d\n", __func__,
				   conf->tuner_name, ip.Tuner_I2cAddr,
				   ip.TunerRefClock, ip.TunerRF_Source,
				   ip.TunerInputSelect);
		assoc_priv = get_demod_from_dev_name(priv, "STV0910P2");
	} else {
		ip.Tuner2HWControlType =
		      stmfe_set_sat_tuner_type(conf->tuner_name, is_auto_tuner);
		ip.Path2TSClock = stmfe_set_ts_output_mode(conf->ts_out);
		ip.Tuner2Model = stmfe_set_tuner_model(conf->tuner_name);
		strncpy(ip.TunerName, "Tuner2", sizeof(ip.TunerName));
		ip.Tuner2_I2cAddr = conf->tuner_io.address;
		ip.Tuner2RefClock = conf->clk_config.tuner_clk;
		ip.Tuner2OutClkDivider = conf->clk_config.tuner_clkout_divider;
		ip.Tuner2IQ_Inversion = stmfe_set_tuner_iq_inv(is_iq_inv);
		ip.Tuner2BasebandGain = stmfe_set_tuner_bbgain(is_vglna,
								ip.Tuner2Model);
		ip.Tuner2BandSelect = stmfe_set_tuner_band(ip.Tuner2Model);
		ip.Tuner2RF_Source = FE_RF_SOURCE_B;
		ip.Tuner2InputSelect = TUNER_INPUT1;
		dpr_info("%s: tuner params for demod 1:\n"
				  "Tuner model = %s\nTuner I2C Address = 0x%x\n"
				  "Tuner Refclock = %d\nTunerRF_Source = %d\n"
				  "TunerInputSelect= %d\n", __func__,
				   conf->tuner_name, ip.Tuner2_I2cAddr,
				   ip.Tuner2RefClock, ip.Tuner2RF_Source,
				   ip.Tuner2InputSelect);
		assoc_priv = get_demod_from_dev_name(priv, "STV0910P1");
	}

	if (!assoc_priv) {
		pr_err("%s: No valid associated path found\n", __func__);
		return -EINVAL;
	}

	pr_info("%s: path =%s assoc = %s\n", __func__, priv->config->demod_name,
						assoc_priv->config->demod_name);

	p = stm_fe_malloc(sizeof(FE_STV0910_InternalParams_t));
	if (!p) {
		pr_err("%s: mem alloc failed for stv0910 params\n", __func__);
		return -ENOMEM;
	}

	if (!assoc_priv->demod_params) {
		if (path == FE_SAT_DEMOD_1) {
			stmfe_set_ts_config(conf->ts_out, conf->ts_clock,
						path, &p1_ts_conf);
			stmfe_set_ts_config(LLA_DEFAULT, conf->ts_clock,
						path, &p2_ts_conf);
		} else {
			stmfe_set_ts_config(LLA_DEFAULT, conf->ts_clock,
						path, &p1_ts_conf);
			stmfe_set_ts_config(conf->ts_out, conf->ts_clock,
						path, &p2_ts_conf);
		}
	} else {
		if (path == FE_SAT_DEMOD_1) {
			stmfe_set_ts_config(conf->ts_out, conf->ts_clock,
					path, &p1_ts_conf);
			stmfe_set_ts_config(assoc_priv->config->ts_out,
						conf->ts_clock, FE_SAT_DEMOD_2,
						&p2_ts_conf);
		} else {
			stmfe_set_ts_config(assoc_priv->config->ts_out,
						assoc_priv->config->ts_clock,
						FE_SAT_DEMOD_1, &p1_ts_conf);
			stmfe_set_ts_config(conf->ts_out, conf->ts_clock,
						path, &p2_ts_conf);
		}
	}

	priv->demod_i2c = chip_open(conf->demod_io.bus);
	if (!priv->demod_i2c) {
		dpr_err("%s: failed to get I2C adaptor\n", __func__);
		stm_fe_free((void **)&p);
		return -EINVAL;
	}

	size = STV0910_NBREGS * sizeof(STCHIP_Register_t);
	priv->demod_h->pRegMapImage = stm_fe_malloc(size);
	if (!priv->demod_h->pRegMapImage) {
		priv->demod_i2c = NULL;
		stm_fe_free((void **)&p);
		pr_err("%s: mem alloc failed for Demod reg map\n", __func__);
		return -ENOMEM;
	}

	/*To enable the auto repeater mode */
#ifdef CONFIG_ARCH_STI
	priv->demod_h->IsAutoRepeaterOffEnable = false;
#else
	priv->demod_h->IsAutoRepeaterOffEnable = true;
#endif

	priv->demod_h->Abort = false;
	priv->tuner_h->Abort = false;
	priv->demod_params = (void *)p;

	/*initialise demod/tuner chip structures */
	p->hDemod = priv->demod_h;
	p->hDemod->dev_i2c = priv->demod_i2c;
	p->hDemod->I2cAddr = conf->demod_io.address;
	p->hDemod->IORoute = conf->demod_io.route;
	p->hDemod->IODriver = conf->demod_io.io;

	if (path == FE_SAT_DEMOD_1) {
		p->hTuner1 = priv->tuner_h;
		p->hTuner1->dev_i2c = priv->demod_i2c;
		p->hTuner1->I2cAddr = conf->tuner_io.address;
		p->hTuner1->IORoute = conf->tuner_io.route;
		p->hTuner1->IODriver = conf->tuner_io.io;
	} else {
		p->hTuner2 = priv->tuner_h;
		p->hTuner2->dev_i2c = priv->demod_i2c;
		p->hTuner2->I2cAddr = conf->tuner_io.address;
		p->hTuner2->IORoute = conf->tuner_io.route;
		p->hTuner2->IODriver = conf->tuner_io.io;
	}

	dpr_info("%s: TS config for P1 Path:\n", __func__);
	dpr_info("%s: TSClockPolarity = %d\nTSSyncByteEnable = %d\n"
			  "TSSwap = %d\n", __func__, p1_ts_conf.TSClockPolarity,
			   p1_ts_conf.TSSyncByteEnable, p1_ts_conf.TSSwap);
	dpr_info("%s: TS config for P2 Path:\n", __func__);
	dpr_info("%s: TSClockPolarity = %d\nTSSyncByteEnable = %d\n"
			  "TSSwap = %d\n", __func__, p2_ts_conf.TSClockPolarity,
			   p2_ts_conf.TSSyncByteEnable, p2_ts_conf.TSSwap);
	err = FE_STV0910_Init(&ip, &priv->demod_params);
	if (err != FE_LLA_NO_ERROR) {
		pr_err("%s: FE_STV0910_Init failed: %d\n", __func__, err);
		ret = -EINVAL;
		goto err_ret;
	}

	err = FE_STV0910_SetTSoutput(p, &p1_ts_conf, &p2_ts_conf);
	if (err != FE_LLA_NO_ERROR) {
		pr_err("%s: STV0910_SetTSoutput failed: %d\n", __func__, err);
		ret = -EINVAL;
		goto err_ret;
	}

	err = FE_STV0910_SetTSConfig(p, &p1_ts_conf, &p2_ts_conf, &p1_ts_speed,
								&p2_ts_speed);
	if (err != FE_LLA_NO_ERROR) {
		pr_err("%s: STV0910_SetTSConfig failed: %d\n", __func__, err);
		ret = -EINVAL;
		goto err_ret;
	}
	dpr_info("%s: successful", __func__);
	return 0;

err_ret:
	stm_fe_free((void **)&priv->demod_h->pRegMapImage);
	priv->demod_i2c = NULL;
	priv->demod_params = NULL;
	stm_fe_free((void **)&p);
	return ret;
}

/*
 * Name: stv0910_term()
 *
 * Description: terminating the stv0910 LLA
 *
 */
static int stv0910_term(struct stm_fe_demod_s *priv)
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
		pr_err("%s: tried to free NULL ptr demod_params\n", __func__);
		return ret;
	}
	dpr_info("%s: lla terminated\n", __func__);
	return ret;
}

/*
 * Name: stv0910_tune()
 *
 * Description: Provided tuning parameters to LLA and perfomed tuning
 *
 */
static int stv0910_tune(struct stm_fe_demod_s *priv, bool *lock)
{
	FE_STV0910_Error_t err;
	FE_STV0910_SearchParams_t sp;
	FE_STV0910_SearchResult_t rp;
	FE_STV0910_Handle_t hdl;
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

	memset(&rp, '\0', sizeof(FE_STV0910_SearchResult_t));
	memset(&sp, '\0', sizeof(FE_STV0910_SearchParams_t));

	hdl = priv->demod_params;

	FILL_SAT_SEARCH_PARAMS_S1_S2(priv, std_in, sp, psat)
	dpr_info("%s: Search params filled are:\n", __func__);
	dpr_info("%s: Freq = %d KHz\nSymbolRate = %d bds\n"
			  "SearchRange = %d Hz\n", __func__, sp.Frequency,
			  sp.SymbolRate, sp.SearchRange);

	if (std_in & STM_FE_DEMOD_TX_STD_S1_S2)
		RESET_INFO_SAT_S1_S2(isat)
	err = FE_STV0910_Search((FE_STV0910_Handle_t)hdl, &sp, &rp, 0);
	if (!err && rp.Locked) {
		*lock = rp.Locked;
		dpr_info("%s: demod %s locked", __func__, priv->demod_name);
		info->std = stmfe_get_sat_standard(rp.Standard);
		if (info->std & STM_FE_DEMOD_TX_STD_S1_S2) {
			isat->freq = rp.Frequency;
			isat->sr = rp.SymbolRate;
			isat->fec = stmfe_get_sat_fec_rate(rp.Standard,
					    rp.PunctureRate, rp.ModCode);
			isat->inv = rp.Spectrum ?  FE_SAT_IQ_NORMAL :
				FE_SAT_IQ_SWAPPED;
			isat->mod = stmfe_get_sat_modulation(info->std,
					isat->fec);
			isat->status = STM_FE_DEMOD_SYNC_OK;
		}

		if (info->std == STM_FE_DEMOD_TX_STD_DVBS2)
			isat->roll_off = stmfe_get_sat_roll_off(rp.RollOff);
		priv->first_tunerinfo = true;
		dpr_info("%s: Result params after lock :", __func__);
		dpr_info("%s: Freq = %d KHz\nSymbolRate = %d bds\n",
				   __func__, rp.Frequency, rp.SymbolRate);
	} else if (err == FE_LLA_SEARCH_FAILED) {
		if (info->std & STM_FE_DEMOD_TX_STD_S1_S2)
			isat->status = STM_FE_DEMOD_NO_SIGNAL;
			dpr_info("%s: LLA SEARCH FAILED or NOSIGNAL", __func__);
	} else {
		if (info->std & STM_FE_DEMOD_TX_STD_S1_S2)
			isat->status = STM_FE_DEMOD_STATUS_UNKNOWN;
		ret = -EINVAL;
	}
	return ret;
}

/*
 * Name: stv0910_scan()
 *
 * Description: Provided scanning parameters to LLA and perfomed tuning
 *
 */
static int stv0910_scan(struct stm_fe_demod_s *priv, bool *lock)
{
	FE_STV0910_Error_t err;
	FE_STV0910_SearchParams_t sp;
	FE_STV0910_SearchResult_t rp;
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

	memset(&rp, '\0', sizeof(FE_STV0910_SearchResult_t));
	memset(&sp, '\0', sizeof(FE_STV0910_SearchParams_t));

	hdl = priv->demod_params;
	sp.Frequency = priv->fe_scan.search_freq;
	sp.SearchRange = 2 * 1000 * 1000;
	sp.Path = THIS_PATH(priv);
	sp.Modulation = FE_SAT_MOD_UNKNOWN;
	sp.Modcode = FE_SAT_MODCODE_UNKNOWN;
	sp.SymbolRate = 100000;
	sp.Standard = FE_SAT_AUTO_SEARCH;
	sp.SearchAlgo = FE_SAT_BLIND_SEARCH;

	sp.PunctureRate =
	    stmfe_set_sat_fec_rate(priv->scanparams.u_config.dvb_sat.fecs);
	sp.IQ_Inversion = FE_SAT_IQ_AUTO;

	dpr_info("%s: scan params are:\n", __func__);
	dpr_info("%s: Freq = %d KHz\nSearchRange = %d Hz\nPath = %d\n"
			  "SymbolRate = %d bds\n", __func__, sp.Frequency,
			  sp.SearchRange, sp.Path, sp.SymbolRate);
	err = FE_STV0910_Search((FE_STV0910_Handle_t) hdl, &sp, &rp, 1);
	if (!err && rp.Locked) {
		*lock = rp.Locked;
		dpr_info("%s: scan successful with scan params\n", __func__);
		dpr_info("%s: Freq locked = %d KHz\nSymbolRate = %d bds\n",
				__func__, rp.Frequency, rp.SymbolRate);

		info->std = stmfe_get_sat_standard(rp.Standard);
		if (info->std & STM_FE_DEMOD_TX_STD_S1_S2) {
			isat->freq = rp.Frequency;
			isat->sr = rp.SymbolRate;
			isat->fec = stmfe_get_sat_fec_rate(rp.Standard,
					rp.PunctureRate, rp.ModCode);
			isat->inv = rp.Spectrum ?  FE_SAT_IQ_NORMAL :
							FE_SAT_IQ_SWAPPED;
			isat->mod = stmfe_get_sat_modulation(info->std,
					isat->fec);
			isat->roll_off = stmfe_get_sat_roll_off(rp.RollOff);
			isat->status = STM_FE_DEMOD_SYNC_OK;
		}
		priv->first_tunerinfo = true;
	} else if (err == FE_LLA_SEARCH_FAILED) {
		if (info->std & STM_FE_DEMOD_TX_STD_S1_S2)
			isat->status = STM_FE_DEMOD_NO_SIGNAL;
		dpr_info("%s: scan fail as lla search failed", __func__);
	} else {
		if (info->std & STM_FE_DEMOD_TX_STD_S1_S2)
			isat->status = STM_FE_DEMOD_STATUS_UNKNOWN;
		dpr_err("%s: scan failed status unknown", __func__);
		ret = -EINVAL;
	}

	return ret;
}

/*
 * Name: stv0910_tracking()
 *
 * Description: Checking the lock status of demodulator
 *
 */
static int stv0910_tracking(struct stm_fe_demod_s *priv)
{
	FE_STV0910_Error_t err = FE_LLA_NO_ERROR;
	FE_STV0910_Handle_t hdl;
	FE_STV0910_SignalInfo_t si;
	uint32_t packetcount_err, packetscount_total;
	stm_fe_demod_channel_info_t *info =
					&priv->c_tinfo.u_info.tune.demod_info;
	stm_fe_demod_dvbs2_channel_info_t *isat = &info->u_channel.dvbs2;

	hdl = priv->demod_params;
	if (priv->first_tunerinfo == true) {
		err = FE_STV0910_STV6120_HMRFilter(hdl, THIS_PATH(priv));
		priv->first_tunerinfo = false;
	}

	err |= FE_STV0910_GetSignalInfo(hdl, THIS_PATH(priv), &si, 1);
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
		FE_STV910_GetPacketErrorRate(hdl, THIS_PATH(priv),
			    &packetcount_err, &packetscount_total);
		isat->per = packetcount_err;
		isat->roll_off = stmfe_get_sat_roll_off(si.RollOff);
	}
	if (info->std & STM_FE_DEMOD_TX_STD_S1_S2) {
		isat->freq = si.Frequency;
		isat->sr = si.SymbolRate;
		isat->fec = stmfe_get_sat_fec_rate(si.Standard, si.PunctureRate,
								si.ModCode);
		isat->inv = si.Spectrum ?  FE_SAT_IQ_NORMAL : FE_SAT_IQ_SWAPPED;
		isat->mod = stmfe_get_sat_modulation(info->std, si.ModCode);
		/* Correction done to provide final snr in (db*10) */
		isat->snr = si.C_N;
		isat->signal_strength = si.Power;
		isat->status = STM_FE_DEMOD_SYNC_OK;
		dpr_info("%s: Freq= %d KHz\nSymbolRate = %d bds\n",
				__func__, isat->freq, isat->sr);
		dpr_info("%s: SNR = %d SignalStrength = %d\n",
			__func__, isat->snr, isat->signal_strength);
	}

	return 0;
}

static int stv0910_status(struct stm_fe_demod_s *priv, bool *locked)
{

	FE_STV0910_Handle_t hdl;
	stm_fe_demod_channel_info_t *info;
	stm_fe_demod_status_t *status;
	uint8_t  i = 0;

	hdl = priv->demod_params;
	info = &priv->c_tinfo.u_info.tune.demod_info;
	do {
		*locked = FE_STV0910_Status(hdl, THIS_PATH(priv));
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

static int stv0910_unlock(struct stm_fe_demod_s *priv)
{
	FE_STV0910_Error_t err;
	FE_STV0910_Handle_t hdl;
	int ret = 0;
	stm_fe_demod_channel_info_t *info;

	hdl = priv->demod_params;
	info = &priv->c_tinfo.u_info.tune.demod_info;

	err = FE_STV0910_Unlock(hdl, THIS_PATH(priv));
	if (err == FE_LLA_INVALID_HANDLE) {
		dpr_err("%s: unlock failed due to invalid handle", __func__);
		ret = -EINVAL;
	} else if (err == FE_LLA_I2C_ERROR) {
		dpr_err("%s: unlock failed due to I2C err", __func__);
		ret = -EFAULT;
	}
	if (info->std == STM_FE_DEMOD_TX_STD_DVBS)
		info->u_channel.dvbs.status = STM_FE_DEMOD_STATUS_UNKNOWN;
	else
		info->u_channel.dvbs2.status = STM_FE_DEMOD_STATUS_UNKNOWN;

	return ret;
}

static int stv0910_abort(struct stm_fe_demod_s *priv, bool abort)
{
	FE_STV0910_Error_t err;
	FE_STV0910_Handle_t hdl;
	int ret = 0;
	hdl = priv->demod_params;

	err = FE_STV0910_SetAbortFlag(hdl, abort, THIS_PATH(priv));
	if (err != FE_LLA_NO_ERROR) {
		pr_err("%s: failed\n", __func__);
		ret = -EINVAL;
	} else {
		dpr_info("%s: successful\n", __func__);
	}

	return ret;
}

static int stv0910_standby(struct stm_fe_demod_s *priv, bool standby)
{
	FE_STV0910_Error_t err;
	FE_STV0910_Handle_t hdl;
	int ret = 0;

	hdl = priv->demod_params;
	err = FE_STV0910_SetStandby(hdl, standby, THIS_PATH(priv));
	if (err != FE_LLA_NO_ERROR) {
		pr_err("%s: failed\n", __func__);
		ret = -EINVAL;
	} else {
		dpr_info("%s: successful\n", __func__);
	}

	return ret;
}

/*
 * Name: stv0910_restore()
 *
 * Description: Re-Initiliazing LLA after restoring from CPS
 *
 */
static int stv0910_restore(struct stm_fe_demod_s *priv)
{
	FE_Sat_InitParams_t ip;
	struct demod_config_s *conf = priv->config;
	FE_STV0910_TSConfig_t p1_ts_conf, p2_ts_conf;
	uint32_t p1_ts_speed, p2_ts_speed;
	int ret = 0, path = stv0910_get_path(conf);
	FE_STV0910_Error_t err;
	bool is_auto_tuner = (conf->custom_flags & DEMOD_CUSTOM_USE_AUTO_TUNER);
	bool is_iq_inv = (conf->custom_flags & DEMOD_CUSTOM_IQ_WIRING_INVERTED);
	bool is_vglna = (conf->custom_flags & DEMOD_CUSTOM_ADD_ON_VGLNA);
	struct stm_fe_demod_s *assoc_priv = NULL;

	dpr_info("%s: auto_tuner = %d, iq_inv = %d, vglna =%d\n", __func__,
					    is_auto_tuner, is_iq_inv, is_vglna);
	/* LLA init params from config */
	memset(&ip, 0, sizeof(FE_Sat_InitParams_t));
	strncpy(ip.DemodName, "STV0910", sizeof(ip.DemodName));

	dpr_info("%s: Reinitializing LLA after restring from CPS", __func__);
	ip.DemodI2cAddr = conf->demod_io.address;
	ip.DemodRefClock = conf->clk_config.demod_clk;
	ip.DemodType = FE_SAT_DUAL;
	ip.RollOff = stmfe_set_sat_roll_off(conf->roll_off);

	dpr_info("%s: DemodI2Caddress is = 0x%x\nDemodRefclock is = %d\n"
			  "Demodtype is = FE_SAT_DUAL\n", __func__,
			   ip.DemodI2cAddr, ip.DemodRefClock);
	if (path == FE_SAT_DEMOD_1) {
		ip.TunerHWControlType =
			stmfe_set_sat_tuner_type(conf->tuner_name,
					is_auto_tuner);
		ip.PathTSClock = stmfe_set_ts_output_mode(conf->ts_out);
		ip.TunerModel = stmfe_set_tuner_model(conf->tuner_name);
		strncpy(ip.TunerName, "Tuner1", sizeof(ip.TunerName));
		ip.Tuner_I2cAddr = conf->tuner_io.address;
		ip.TunerRefClock = conf->clk_config.tuner_clk;
		ip.TunerOutClkDivider = conf->clk_config.tuner_clkout_divider;
		ip.TunerIQ_Inversion = stmfe_set_tuner_iq_inv(is_iq_inv);
		ip.TunerBasebandGain = stmfe_set_tuner_bbgain(is_vglna,
								ip.TunerModel);
		ip.TunerBandSelect = stmfe_set_tuner_band(ip.TunerModel);
		ip.TunerRF_Source = FE_RF_SOURCE_C;
		ip.TunerInputSelect = TUNER_INPUT2;
		dpr_info("%s: tuner params for demod 0:\n"
				  "Tuner model = %s\nTuner I2C Address = 0x%x\n"
				  "Tuner Refclock = %d\n TunerRF_Source = %d\n"
				  "TunerInputSelect = %d\n", __func__,
				   conf->tuner_name, ip.Tuner_I2cAddr,
				   ip.TunerRefClock, ip.TunerRF_Source,
				   ip.TunerInputSelect);
		assoc_priv = get_demod_from_dev_name(priv, "STV0910P2");
	} else {
		ip.Tuner2HWControlType =
			stmfe_set_sat_tuner_type(conf->tuner_name,
					is_auto_tuner);
		ip.Path2TSClock = stmfe_set_ts_output_mode(conf->ts_out);
		ip.Tuner2Model = stmfe_set_tuner_model(conf->tuner_name);
		strncpy(ip.TunerName, "Tuner2", sizeof(ip.TunerName));
		ip.Tuner2_I2cAddr = conf->tuner_io.address;
		ip.Tuner2RefClock = conf->clk_config.tuner_clk;
		ip.Tuner2OutClkDivider = conf->clk_config.tuner_clkout_divider;
		ip.Tuner2IQ_Inversion = stmfe_set_tuner_iq_inv(is_iq_inv);
		ip.Tuner2BasebandGain = stmfe_set_tuner_bbgain(is_vglna,
								ip.Tuner2Model);
		ip.Tuner2BandSelect = stmfe_set_tuner_band(ip.Tuner2Model);
		ip.Tuner2RF_Source = FE_RF_SOURCE_B;
		ip.Tuner2InputSelect = TUNER_INPUT1;
		dpr_info("%s: tuner params for demod 1:\n"
				  "Tuner model = %s\nTuner I2C Address = 0x%x\n"
				  "Tuner Refclock = %d\n TunerRF_Source = %d\n"
				  "Tuner InputSelect = %d\n", __func__,
				   conf->tuner_name, ip.Tuner2_I2cAddr,
				   ip.Tuner2RefClock, ip.Tuner2RF_Source,
				   ip.Tuner2InputSelect);
		assoc_priv = get_demod_from_dev_name(priv, "STV0910P1");
	}

	if (!assoc_priv) {
		pr_err("%s: No valid associated path found\n", __func__);
		return -EINVAL;
	}

	if (!assoc_priv->demod_params) {
		if (path == FE_SAT_DEMOD_1) {
			stmfe_set_ts_config(conf->ts_out, conf->ts_clock,
					path, &p1_ts_conf);
			stmfe_set_ts_config(LLA_DEFAULT, conf->ts_clock,
					path, &p2_ts_conf);
		} else {
			stmfe_set_ts_config(LLA_DEFAULT, conf->ts_clock,
					path, &p1_ts_conf);
			stmfe_set_ts_config(conf->ts_out, conf->ts_clock,
					path, &p2_ts_conf);
		}
	} else {
		if (path == FE_SAT_DEMOD_1) {
			stmfe_set_ts_config(conf->ts_out, conf->ts_clock,
					path, &p1_ts_conf);
			stmfe_set_ts_config(assoc_priv->config->ts_out,
					conf->ts_clock, FE_SAT_DEMOD_2,
					&p2_ts_conf);
		} else {
			stmfe_set_ts_config(assoc_priv->config->ts_out,
					assoc_priv->config->ts_clock,
					FE_SAT_DEMOD_1, &p1_ts_conf);
			stmfe_set_ts_config(conf->ts_out, conf->ts_clock,
					path, &p2_ts_conf);
		}
	}

	dpr_info("%s: TS config for P1 Path:\n", __func__);
	dpr_info("%s: TSClockPolarity = %d\nTSSyncByteEnable = %d\n"
			  "TSSwap = %d\n", __func__, p1_ts_conf.TSClockPolarity,
			  p1_ts_conf.TSSyncByteEnable, p1_ts_conf.TSSwap);
	dpr_info("%s: TS config for P2 Path:\n", __func__);
	dpr_info("%s: TSClockPolarity = %d\nTSSyncByteEnable = %d\n"
			  "TSSwap = %d\n", __func__, p2_ts_conf.TSClockPolarity,
			   p2_ts_conf.TSSyncByteEnable, p2_ts_conf.TSSwap);
	err = FE_STV0910_Init(&ip, &priv->demod_params);
	if (err != FE_LLA_NO_ERROR) {
		pr_err("%s: FE_STV0910_Init failed: %d\n", __func__, err);
		ret = -EINVAL;
		goto err_ret;
	}

	err = FE_STV0910_SetTSoutput(priv->demod_params, &p1_ts_conf,
			&p2_ts_conf);
	if (err != FE_LLA_NO_ERROR) {
		pr_err("%s: STV0910_SetTSoutput failed: %d\n", __func__, err);
		ret = -EINVAL;
		goto err_ret;
	}

	err = FE_STV0910_SetTSConfig(priv->demod_params, &p1_ts_conf,
				       &p2_ts_conf, &p1_ts_speed, &p2_ts_speed);
	if (err != FE_LLA_NO_ERROR) {
		pr_err("%s: STV0910_SetTSConfig failed: %d\n", __func__, err);
		ret = -EINVAL;
		goto err_ret;
	}
	dpr_info("%s: LLA init successful after restoring from CPS", __func__);
	return 0;

err_ret:
	return ret;
}

static int32_t __init stv0910_lla_init(void)
{
	pr_info("Loading stv0910 driver module...\n");

	return 0;
}

module_init(stv0910_lla_init);

static void __exit stv0910_lla_term(void)
{
	pr_info("Removing stv0910 driver module ...\n");

}

module_exit(stv0910_lla_term);

MODULE_DESCRIPTION("Low level driver for STV0910 satellite demodulator");
MODULE_AUTHOR("STMicroelectronics Limited");
MODULE_VERSION(STV0910_CURRENT_LLA_REVISION);
MODULE_LICENSE("GPL");
