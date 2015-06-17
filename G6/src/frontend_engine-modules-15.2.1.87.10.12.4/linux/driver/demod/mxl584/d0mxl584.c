/************************************************************************
Copyright (C) 2012, 2013 STMicroelectronics. All Rights Reserved.

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

Source file name : d0mxl584.c
Author :           Hemendra

Wrapper for MXLware

Date        Modification                                    Name
----        ------------                                    --------
9-Apr-13   Created                                           HS

*******************************************************************************/

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/list.h>
#include <stm_fe.h>
#include <stm_fe_os.h>
#include <i2c_wrapper.h>
#include <stm_fe_demod.h>
#include <fesat_commlla_str.h>
#include <lla_utils.h>
#include "d0mxl584.h"
#include <mxl584_drv.h>
#include <stm_fe_custom.h>
#include <stm_fe_install.h>
#include <stm_fe.h>

static int lla_wrapper_debug;
module_param_named(lla_wrapper_debug, lla_wrapper_debug, int, 0644);
MODULE_PARM_DESC(lla_wrapper_debug,
		 "Turn on/off mxl584 lla wrapper debugging (default:off).");
#define dpr_info(x...) do { if (lla_wrapper_debug) pr_info(x); } while (0)
#define dpr_err(x...) do { if (lla_wrapper_debug) pr_err(x); } while (0)


unsigned int get_demux_tag(int fe_instance_id)
{
	/*
	* for MXL Hydra demod instance 0-3, the tagging for b2020-h416_a9 is
	* mapped to fe_instance id 0-3 respectively, for rest of the instances,
	*the mapping is to be checked
	*/
	/*if ((fe_instance_id >= 0) &&	(fe_instance_id < 4))
	*	return (unsigned int)fe_instance_id;
	* else
	*//*printk("\n\n\n******\nTSINID = %d******\n\n",fe_instance_id);*/
		return (unsigned int)fe_instance_id;

}

#if 0
unsigned int get_demux_tsin_id(int fe_instance_id)
{
	/*
	 * for MXL Hydra demod instance 0-3, the tsin id for b2020-h416_a9 is
	 * mapped to tsin id 0-3 respectively, for rest of the instances, the
	 * mapping is to be checked
	 */
	/*if ((fe_instance_id >= 0) &&  (fe_instance_id < 4))
	*	return (unsigned int)fe_instance_id;
	* else
	*//*printk("\n\n\n******\nTSINID = %d******\n\n",fe_instance_id);*/
	/*	return (unsigned int)fe_instance_id;*/

	/*When muxed mode is on.*/
	if ((fe_instance_id >= 0) &&  (fe_instance_id <= 1))
		return (unsigned int)2;
	if ((fe_instance_id >= 2) &&  (fe_instance_id <= 3))
		return (unsigned int)3;

	/*if (fe_instance_id == 0)
		return (unsigned int)0x2000;
	if (fe_instance_id == 1)
		return (unsigned int)0x2001;
	*/
	return (unsigned int)0;
}
#endif

int stm_fe_mxl584_attach(struct stm_fe_demod_s *priv)
{

	if (!priv->ops)
		return -EFAULT;

	priv->ops->init = mxl584_init;
	priv->ops->tune = mxl584_tune;
	priv->ops->scan = mxl584_scan;
	priv->ops->tracking = mxl584_tracking;
	priv->ops->status = mxl584_status;
	priv->ops->unlock = mxl584_unlock;
	priv->ops->term = mxl584_term;
	priv->ops->abort = mxl584_abort;
	priv->ops->standby = mxl584_standby;
	priv->ops->detach = stm_fe_mxl584_detach;
	priv->ops->restore = NULL/*mxl584_restore*/;
	priv->ops->tsclient_init = NULL;
	priv->ops->tsclient_term = NULL;
	priv->ops->tsclient_pid_configure = mxl584_ts_pid_start;
	priv->ops->tsclient_pid_deconfigure = mxl584_ts_pid_stop;
	/*priv->config->ts_out =  0x200A962;*/
	/*priv->config->demux_tsin_id = get_demux_tsin_id(priv->demod_id);*/
	/*priv->config->ts_tag =  get_demux_tag(priv->demod_id);
	priv->config->ts_clock = 0;
	priv->ops->tsclient_pid_configure = NULL;
	priv->ops->tsclient_pid_deconfigure = NULL;*/

	dpr_info("%s: demod attached\n", __func__);

	return 0;
}
EXPORT_SYMBOL(stm_fe_mxl584_attach);

int mxl584_ts_pid_start(stm_object_h demod_object, stm_object_h
				demux_object, uint32_t pid)
{
	FE_MXL584_Error_t err;
	FE_MXL584_Handle_t handle;
	struct stm_fe_demod_s *demod_priv;
	FE_MXL584_DEMOD_t Demod;
	int ret = 0;
	BOOL enable = 1;


	if (demod_object)
		demod_priv = ((struct stm_fe_demod_s *)demod_object);
	else
		return -EINVAL;

	handle = demod_priv->demod_params;
	Demod = demod_priv->demod_id;
	err = FE_MXL584_cfg_pid_fltr(handle, Demod,
				pid, enable);

	if (err != FE_LLA_NO_ERROR) {
		printk(KERN_ERR "%s:mxl584_ts_pid_start fail\n", __func__);
		ret = -EINVAL;
	}
	return ret;
}

int mxl584_ts_pid_stop(stm_object_h demod_object, stm_object_h
			demux_object, uint32_t pid)
{
	FE_MXL584_Error_t err;
	FE_MXL584_Handle_t handle;
	struct stm_fe_demod_s *demod_priv;
	FE_MXL584_DEMOD_t Demod;
	BOOL disable = 0;
	int ret = 0;

	if (demod_object)
		demod_priv = ((struct stm_fe_demod_s *)demod_object);
	else
		return -EINVAL;

	handle = demod_priv->demod_params;
	Demod = demod_priv->demod_id;

	err = FE_MXL584_cfg_pid_fltr(handle, Demod, pid, disable);
	if (err != FE_LLA_NO_ERROR) {
		printk(KERN_ERR "%s:mxl584_ts_pid_stop fail\n", __func__);
		ret = -EINVAL;
	}
	return ret;
}

/*
 * Name: stm_fe_mxl584_detach()
 *
 * Description: Uninstalled the wrapper function pointers for driver
 *
 */

int stm_fe_mxl584_detach(struct stm_fe_demod_s *priv)
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
EXPORT_SYMBOL(stm_fe_mxl584_detach);


struct stm_fe_demod_s *
	get_demod_from_dev_name(struct stm_fe_demod_s *demod_priv, char *name)
{
	struct list_head *pos;
	list_for_each(pos, demod_priv->list_h) {
		struct stm_fe_demod_s *assoc_demod;
		assoc_demod = list_entry(pos, struct stm_fe_demod_s, list);
		if (IS_NAME_MATCH(assoc_demod->config->demod_name, name)
				&& assoc_demod->config->demod_io.address ==
					demod_priv->config->demod_io.address
				&& assoc_demod->config->demod_io.bus ==
					demod_priv->config->demod_io.bus)
			return assoc_demod;
	}
	return NULL;
}

/*
 * Name: mxl584_init()
 *
 * Description: Providing Initiliazing parameters to LLA
 *
 */
int mxl584_init(struct stm_fe_demod_s *priv)
{
	FE_MXL584_InternalParams_t *p = NULL;
	FE_Sat_InitParams_t ip;
	FE_MXL584_Error_t err;
	size_t size;
	struct demod_config_s *conf = priv->config;
	int ret = 0;
#if 0
	struct list_head *pos;
	struct stm_fe_demod_s *demod_prev[10];
	U8 i = 7;
	U8 j = 0;
	MXL_HYDRA_TS_MUX_PREFIX_HEADER_T pfix_hdr;
#endif
	bool is_iq_inv = (conf->custom_flags & DEMOD_CUSTOM_IQ_WIRING_INVERTED);
	/* LLA init params from config */
	memset(&ip, 0, sizeof(FE_Sat_InitParams_t));
	strncpy(ip.DemodName, "MXL584", sizeof(ip.DemodName));

	ip.DemodI2cAddr = conf->demod_io.address;
	ip.DemodRefClock = conf->clk_config.demod_clk;
	/*update auto in search*/
	ip.RollOff = stmfe_set_sat_roll_off(conf->roll_off);
	dpr_info("%s: DemodI2Caddress = 0x%x DemodRefclock = %d\n", __func__,
							ip.DemodI2cAddr,
							ip.DemodRefClock);
	ip.PathTSClock = stmfe_set_ts_output_mode(conf->ts_out);
	ip.Tuner_I2cAddr = conf->tuner_io.address;
	ip.TunerRefClock = conf->clk_config.tuner_clk;
	ip.TunerOutClkDivider = conf->clk_config.tuner_clkout_divider;
	ip.TunerIQ_Inversion = stmfe_set_tuner_iq_inv(is_iq_inv);
	dpr_info(
		"%s:priv->config->demod_name = %s demod_i2c_addr =0x%x\n",
		__func__, priv->config->demod_name, ip.DemodI2cAddr);

	p = stm_fe_malloc(sizeof(FE_MXL584_InternalParams_t));
	if (!p) {
		pr_err("%s: mem alloc failed for FE_MXL584_InternalParams_t\n",
			__func__);
		return -ENOMEM;
	}

	priv->demod_i2c = chip_open(conf->demod_io.bus);
	/*pr_info("*%s:demod_i2c adapter =0x%x\n",
		__func__,(uint32_t)priv->demod_i2c);*/
	if (priv->demod_i2c == NULL) {
		stm_fe_free((void **)&p);
		return -EINVAL;
	}
	size = MXL584_HYDRA_NBREGS * sizeof(STCHIP_Register_t);
	priv->demod_h->pRegMapImage = stm_fe_malloc(size);
	if (priv->demod_h->pRegMapImage == NULL) {
		priv->demod_i2c = NULL;
		stm_fe_free((void **)&p);
		pr_info("%s: mem alloc fail for Demod reg map\n", __func__);
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

	/*No tuner with mxl584*/
	err = FE_MXL584_Init(&ip, &priv->demod_params);
	if (err != FE_LLA_NO_ERROR) {
		pr_err("%s: FE_MXL584_Init failed: %d\n", __func__, err);
		ret = -EINVAL;
		goto err_ret;
	}

	err = FE_MXL584_Taging_Muxing(priv);
	if (err != FE_LLA_NO_ERROR) {
		pr_err("%s: FE_MXL584_Taging Muxing failed: %d\n",
					__func__, err);
		ret = -EINVAL;
		goto err_ret;
	}

	return 0;

err_ret:
	stm_fe_free((void **)&priv->demod_h->pRegMapImage);
	priv->demod_i2c = NULL;
	priv->demod_params = NULL;
	stm_fe_free((void **)&p);
	return ret;
}

/*
 * Name: mxl584_term()
 *
 * Description: terminating the 584 LLA
 *
 */
int mxl584_term(struct stm_fe_demod_s *priv)
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
	/*free up the mutex*/
	 FE_MXL584_Term(priv->demod_params);

	ret = stm_fe_free((void **)&priv->demod_params);
	if (ret) {
		pr_err("%s: tried to free NULL ptr demod_params\n", __func__);
		return ret;
	}

	dpr_info("%s: lla terminated\n", __func__);
	return ret;
}

/*
 * Name: mxl584_tune()
 *
 * Description: Provided tuning parameters to LLA and performed tuning
 *
 */
int mxl584_tune(struct stm_fe_demod_s *priv, bool *lock)
{
	FE_MXL584_Error_t err;
	FE_MXL584_SearchParams_t sp;
	FE_MXL584_SearchResult_t rp;
	FE_MXL584_Handle_t hdl;
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

	memset(&rp, '\0', sizeof(FE_MXL584_SearchResult_t));
	memset(&sp, '\0', sizeof(FE_MXL584_SearchParams_t));

	hdl = priv->demod_params;

	FILL_CUSTOM_SEARCH_PARAMS_S1_S2(priv, std_in, sp, psat)

	dpr_info("%s: Search params filled are:\n", __func__);
	dpr_info("%s: Freq = %d KHz\nSymbolRate = %d bds\n"
			  "SearchRange = %d Hz\n", __func__,
			  sp.Frequency, sp.SymbolRate, sp.SearchRange);
	if (std_in & STM_FE_DEMOD_TX_STD_S1_S2)
		RESET_INFO_SAT_S1_S2(isat)

	err = FE_MXL584_Search((FE_MXL584_Handle_t) hdl,
					priv->demod_id, &sp, &rp);
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
 * Name: mxl584_scan()
 *
 * Description: Provided scanning parameters to LLA and perfomed tuning
 *
 */
int mxl584_scan(struct stm_fe_demod_s *priv, bool *lock)
{
	FE_MXL584_Error_t err;
	FE_MXL584_SearchParams_t sp;
	FE_MXL584_SearchResult_t rp;
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

	memset(&rp, '\0', sizeof(FE_MXL584_SearchResult_t));
	memset(&sp, '\0', sizeof(FE_MXL584_SearchParams_t));

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

	err = FE_MXL584_Search((FE_MXL584_Handle_t) hdl, priv->demod_id, &sp,
									&rp);
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
 * Name: mxl584_tracking()
 *
 * Description: Copying the lock parameters of demodulator
 *
 */
int mxl584_tracking(struct stm_fe_demod_s *priv)
{
	FE_MXL584_Error_t err;
	FE_MXL584_Handle_t hdl;
	FE_MXL584_SignalInfo_t sinfo;
	uint32_t packetcount_err = 0/*, packetscount_total*/;
	stm_fe_demod_channel_info_t *info =
		&priv->c_tinfo.u_info.tune.demod_info;
	stm_fe_demod_dvbs2_channel_info_t *isat = &info->u_channel.dvbs2;

	FE_MXL584_DEMOD_t Demod = priv->demod_id;
	hdl = priv->demod_params;
	err = FE_LLA_NO_ERROR;

	memset(&sinfo, '\0', sizeof(FE_MXL584_SignalInfo_t));

	err |= FE_MXL584_GetSignalInfo(hdl, Demod, &sinfo);
	if (!(err)) {
		if (sinfo.Standard == FE_SAT_DVBS1_STANDARD) {
			info->std = STM_FE_DEMOD_TX_STD_DVBS;
			isat->per = sinfo.BER;
		} else if (sinfo.Standard == FE_SAT_DVBS2_STANDARD) {
			info->std = STM_FE_DEMOD_TX_STD_DVBS2;
			isat->per = packetcount_err;
			isat->roll_off = stmfe_get_sat_roll_off(sinfo.RollOff);
		}
		if (info->std & STM_FE_DEMOD_TX_STD_S1_S2) {
			isat->freq = sinfo.Frequency;
			isat->sr = sinfo.SymbolRate;
			isat->fec =
				stmfe_get_sat_fec_rate(sinfo.Standard,
					sinfo.PunctureRate, sinfo.ModCode);
			isat->inv = sinfo.Spectrum;
			isat->mod = stmfe_get_sat_modulation(info->std,
						  sinfo.ModCode);
			/*Round off C/N*/
			isat->snr = sinfo.C_N / 10 +
						((sinfo.C_N % 10) >= 5 ? 1 : 0);
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
/*
 * Name: mxl584_status()
 * * Description: Checking the lock status of demodulator
 */

int mxl584_status(struct stm_fe_demod_s *priv, bool *locked)
{

	FE_MXL584_Handle_t hdl;
	stm_fe_demod_channel_info_t *info;
	stm_fe_demod_status_t *status;
	FE_MXL584_DEMOD_t Demod = priv->demod_id;

	hdl = priv->demod_params;
	info = &priv->c_tinfo.u_info.tune.demod_info;
	*locked = FE_MXL584_Status(hdl, Demod);

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

/*
 * Name: mxl584_unlock()
 *
 * Description: disable the demodulator
 *
 */
int mxl584_unlock(struct stm_fe_demod_s *priv)
{
	FE_MXL584_Error_t err;
	FE_MXL584_Handle_t hdl;
	int ret = 0;
	stm_fe_demod_channel_info_t *info;
	FE_MXL584_DEMOD_t Demod = priv->demod_id;

	hdl = priv->demod_params;
	info = &priv->c_tinfo.u_info.tune.demod_info;

	err = FE_MXL584_Unlock(hdl, Demod);
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
/*
 * Name: mxl584_abort()
 * Description: Set the abort flag
 */
int mxl584_abort(struct stm_fe_demod_s *priv, bool abort)
{
	FE_MXL584_Error_t err;
	FE_MXL584_Handle_t hdl;
	int ret = 0;
	FE_MXL584_DEMOD_t Demod = priv->demod_id;
	hdl = priv->demod_params;
	err = FE_MXL584_SetAbortFlag(hdl, abort, Demod);
	if (err != FE_LLA_NO_ERROR) {
		pr_err("%s: failed\n", __func__);
		ret = -EINVAL;
	} else {
		dpr_info("%s: successful\n", __func__);
	}
	return ret;
}

/*
 * Name: mxl584_standby()
 *
 * Description: Set thedevice in Standby and recover
 *
 */
int mxl584_standby(struct stm_fe_demod_s *priv, bool standby)
{
	FE_MXL584_Error_t err;
	FE_MXL584_Handle_t hdl;
	int ret = 0;
	FE_MXL584_DEMOD_t Demod = priv->demod_id;
	hdl = priv->demod_params;
	err = FE_MXL584_SetStandby(hdl, standby, Demod);
	if (err != FE_LLA_NO_ERROR) {
		pr_err("%s: failed\n", __func__);
		ret = -EINVAL;
	} else {
		dpr_info("%s: successful\n", __func__);
	}

	return ret;
}
#if 0
int mxl584_restore(struct stm_fe_demod_s *priv)
{
	FE_Sat_InitParams_t ip;
	struct demod_config_s *conf = priv->config;
	FE_MXL584_TSConfig_t ts_conf;
	uint32_t p1_ts_speed;
	int ret = 0;
	FE_MXL584_Error_t err;

	bool is_iq_inv = (conf->custom_flags & DEMOD_CUSTOM_IQ_WIRING_INVERTED);

	dpr_info("%s: auto_tuner = %d, iq_inv = %d, vglna =%d\n", __func__,
					is_auto_tuner, is_iq_inv, is_vglna);
	/* LLA init params from config */
	memset(&ip, 0, sizeof(FE_Sat_InitParams_t));
	strncpy(ip.DemodName, "MXL584", sizeof(ip.DemodName));

	dpr_info("%s: Reinitializing LLA after restring from CPS\n", __func__);

	ip.DemodI2cAddr = conf->demod_io.address;
	ip.DemodRefClock = conf->clk_config.demod_clk;
	ip.RollOff = stmfe_set_sat_roll_off(conf->roll_off);

	dpr_info("%s: DemodI2Caddress is = 0x%x\nDemodRefclock is = %d\n",
				__func__, ip.DemodI2cAddr, ip.DemodRefClock);

	err = FE_MXL584_Init(&ip, &priv->demod_params);
	if (err != FE_LLA_NO_ERROR) {
		pr_err("%s: FE_MXL584_Init failed: %d\n", __func__, err);
		ret = -EINVAL;
		goto err_ret;
	}

	dpr_info("%s: TS config for Demod:\n", __func__);
	dpr_info("%s: TSClockPolarity = %d\nTSSyncByteEnable = %d\n"
			  "TSSwap = %d\n", __func__, ts_conf.TSClockPolarity,
			  ts_conf.TSSyncByteEnable, ts_conf.TSSwap);

	dpr_info("%s: init successful after restoring from CPS\n", __func__);

	return 0;

err_ret:
	return ret;
}
#endif

static int32_t __init mxl584_lla_init(void)
{
	pr_info("Loading mxl584 driver module...\n");

	return 0;
}
module_init(mxl584_lla_init);

static void __exit mxl584_lla_term(void)
{
	pr_info("Removing mxl584 driver module ...\n");

}
module_exit(mxl584_lla_term);


MODULE_DESCRIPTION("Low level driver for MXL584 satellite demodulator");
MODULE_VERSION("1.1.1.19");
MODULE_LICENSE("GPL");
