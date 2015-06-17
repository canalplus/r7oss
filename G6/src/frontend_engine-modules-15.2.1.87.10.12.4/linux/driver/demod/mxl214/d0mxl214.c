/************************************************************************
Copyright (C) 2014 STMicroelectronics. All Rights Reserved.

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

Source file name : d0mxl214.c
Author :           Manu

Wrapper for MXLware

Date        Modification                                    Name
----        ------------                                    --------
1-July-14   Created                                           MS

*******************************************************************************/

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/list.h>
#include <stm_fe.h>
#include <stm_fe_os.h>
#include <i2c_wrapper.h>
#include <stm_fe_demod.h>
#include <lla_utils.h>
#include "d0mxl214.h"
#include <mxl214_drv.h>
#include <stm_fe_custom.h>
#include <stm_fe_install.h>
#include <stm_fe.h>

static int lla_wrapper_debug;
module_param_named(lla_wrapper_debug, lla_wrapper_debug, int, 0644);
MODULE_PARM_DESC(lla_wrapper_debug,
		 "Turn on/off mxl214 lla wrapper debugging (default:off).");
#define dpr_info(x...) do { if (lla_wrapper_debug) pr_info(x); } while (0)
#define dpr_err(x...) do { if (lla_wrapper_debug) pr_err(x); } while (0)


static FE_CAB_Modulation_t modulationmap(stm_fe_demod_modulation_t mod);
static stm_fe_demod_modulation_t get_mod(FE_CAB_Modulation_t mod);

unsigned int get_demux_tag(int fe_instance_id)
{
		return (unsigned int)fe_instance_id;

}

int stm_fe_mxl214_attach(struct stm_fe_demod_s *priv)
{

	if (!priv->ops)
		return -EFAULT;

	priv->ops->init = mxl214_init;
	priv->ops->tune = mxl214_tune;
	priv->ops->scan = mxl214_scan;
	priv->ops->tracking = mxl214_tracking;
	priv->ops->status = mxl214_status;
	priv->ops->unlock = mxl214_unlock;
	priv->ops->term = mxl214_term;
	priv->ops->abort = mxl214_abort;
	priv->ops->standby = mxl214_standby;
	priv->ops->detach = stm_fe_mxl214_detach;
	priv->ops->restore = mxl214_restore;
	priv->ops->tsclient_init = NULL;
	priv->ops->tsclient_term = NULL;
	priv->ops->tsclient_pid_configure = NULL;
	priv->ops->tsclient_pid_deconfigure = NULL;


	dpr_info("%s: demod attached\n", __func__);

	return 0;
}
EXPORT_SYMBOL(stm_fe_mxl214_attach);

/*
 * Name: stm_fe_mxl214_detach()
 *
 * Description: Uninstalled the wrapper function pointers for driver
 *
 */

int stm_fe_mxl214_detach(struct stm_fe_demod_s *priv)
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
EXPORT_SYMBOL(stm_fe_mxl214_detach);


void configure_ts_parameter(enum demod_tsout_config_e ts_out,
					FE_MXL214_TSConfig_t *path_ts_config)
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
 * Name: mxl214_init()
 *
 * Description: Providing Initiliazing parameters to LLA
 *
 */
int mxl214_init(struct stm_fe_demod_s *priv)
{
	FE_MXL214_InternalParams_t *p = NULL;
	FE_CAB_InitParams_t ip;
	FE_MXL214_Error_t err;
	size_t size;
	struct demod_config_s *conf = priv->config;
	int ret = 0;

	/* LLA init params from config */
	memset(&ip, 0, sizeof(FE_CAB_InitParams_t));
	strncpy(ip.DemodName, "MXL214", sizeof(ip.DemodName));

	ip.DemodI2cAddr = conf->demod_io.address;
	/*update auto in search*/
	dpr_info("%s: DemodI2Caddress = 0x%x ip.DemodXtal_Hz = %d\n", __func__,
							ip.DemodI2cAddr,
							ip.DemodXtal_Hz);

	configure_ts_parameter(conf->ts_out, &ip.TS_Config);
	ip.TunerI2cAddr = conf->tuner_io.address;
	ip.DemodXtal_Hz = conf->clk_config.demod_clk;
	ip.TunerXtal_Hz = conf->clk_config.tuner_clk;
	ip.TunerIF_kHz = conf->tuner_if;

	dpr_info(
		"%s:priv->config->demod_name = %s demod_i2c_addr =0x%x\n",
		__func__, priv->config->demod_name, ip.DemodI2cAddr);

	p = stm_fe_malloc(sizeof(FE_MXL214_InternalParams_t));
	if (!p) {
		pr_err("%s: mem alloc failed for FE_MXL214_InternalParams_t\n",
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
	size = MXL214_HRCLS_NBREGS * sizeof(STCHIP_Register_t);
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

	/*No tuner with mxl214*/
	err = FE_MXL214_Init(&ip, &priv->demod_params);
	if (err != FE_LLA_NO_ERROR) {
		pr_err("%s: FE_MXL214_Init failed: %d\n", __func__, err);
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
 * Name: mxl214_term()
 *
 * Description: terminating the 214 LLA
 *
 */
int mxl214_term(struct stm_fe_demod_s *priv)
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
	 FE_MXL214_Term(priv->demod_params);

	ret = stm_fe_free((void **)&priv->demod_params);
	if (ret) {
		pr_err("%s: tried to free NULL ptr demod_params\n", __func__);
		return ret;
	}

	dpr_info("%s: lla terminated\n", __func__);
	return ret;
}

/*
 * Name: mxl214_tune()
 *
 * Description: Provided tuning parameters to LLA and performed tuning
 *
 */
int mxl214_tune(struct stm_fe_demod_s *priv, bool *lock)
{
	FE_MXL214_Error_t err;
	FE_MXL214_SearchParams_t sp;
	FE_MXL214_SearchResult_t rp;
	FE_MXL214_Handle_t hdl;
	int ret = 0;
	stm_fe_demod_dvbc_tune_params_t *tp = &priv->tp.u_tune.dvbc;
	stm_fe_demod_channel_info_t *info =
		&priv->c_tinfo.u_info.tune.demod_info;
	stm_fe_demod_dvbc_channel_info_t *icab = &info->u_channel.dvbc;
	stm_fe_demod_tx_std_t std_in = priv->tp.std;

	memset(&rp, '\0', sizeof(FE_MXL214_SearchResult_t));
	memset(&sp, '\0', sizeof(FE_MXL214_SearchParams_t));

	hdl = priv->demod_params;

	if (std_in == STM_FE_DEMOD_TX_STD_DVBC)
		sp.FECType = FE_CAB_FEC_A;
	if (std_in == STM_FE_DEMOD_TX_STD_J83_AC)
		sp.FECType = FE_CAB_FEC_C;
	if (std_in == STM_FE_DEMOD_TX_STD_J83_B)
		sp.FECType = FE_CAB_FEC_B;

	FILL_CAB_SEARCH_PARAMS_DVBC(sp, tp)

	pr_info("%s: tune params for cable :\n", __func__);
	pr_info("%s Freq = %d kHz\nSearch Range = %d Hz\nSymbol Rate = %d bds FEC_A/B/C=%d\n",
				__func__, sp.Frequency_kHz, sp.SearchRange_Hz,
				sp.SymbolRate_Bds, sp.FECType);


	err = FE_MXL214_Search((FE_MXL214_Handle_t) hdl,
					priv->demod_id, &sp, &rp);
	if (!err && rp.Locked) {
		*lock = rp.Locked;
		pr_info("%s: demod %s locked\n", __func__, priv->demod_name);

		FILL_DVBC_INFO(icab, rp)

		info->std = STM_FE_DEMOD_TX_STD_DVBC;
		icab->status = STM_FE_DEMOD_SYNC_OK;
		pr_info("%s: Result params after lock :\n", __func__);
		pr_info("%s: Freq = %d KHz\nSymbolRate = %d bds\n",
					__func__, rp.Frequency_kHz,
					rp.SymbolRate_Bds);

	} else if (err == FE_LLA_SEARCH_FAILED) {
			icab->status = STM_FE_DEMOD_NO_SIGNAL;
		 pr_info("%s: tune fail as lla search failed\n", __func__);
	} else {
			icab->status = STM_FE_DEMOD_STATUS_UNKNOWN;
			ret = -EINVAL;
			pr_err("%s: tune failed status unknown\n", __func__);
	}

	return ret;
}

/*
 * Name: mxl214_scan()
 *
 * Description: Provided scanning parameters to LLA and perfomed tuning
 *
 */
int mxl214_scan(struct stm_fe_demod_s *priv, bool *lock)
{
	int ret = 0;
	FE_MXL214_Error_t err = FE_LLA_NO_ERROR;
	FE_MXL214_SearchParams_t sp;
	FE_MXL214_SearchResult_t rp;
	stm_fe_demod_channel_info_t *info;
	stm_fe_demod_dvbc_channel_info_t *icab = NULL;
	FE_MXL214_Handle_t hdl;
	unsigned char i = 0;
	unsigned char num_trial = 0;
	FE_CAB_Modulation_t cab_mod[6] = {
		FE_CAB_MOD_QAM64,
		FE_CAB_MOD_QAM256,
		FE_CAB_MOD_QAM32,
		FE_CAB_MOD_QAM128,
		FE_CAB_MOD_QAM16,
		FE_CAB_MOD_QAM4
	};

	info = &priv->c_tinfo.u_info.scan.demod_info;
	icab = &info->u_channel.dvbc;
	memset(&rp, '\0', sizeof(FE_MXL214_SearchResult_t));
	memset(&sp, '\0', sizeof(FE_MXL214_SearchParams_t));

	hdl = priv->demod_params;
	if (!hdl) {
		pr_err("%s: invalid demod params\n", __func__);
		return -EINVAL;
	}
	sp.Frequency_kHz = priv->fe_scan.search_freq;
	sp.SearchRange_Hz = 280000;	/*280 MHz */
	sp.SymbolRate_Bds = priv->fe_scan.sr_act;
	sp.Modulation = modulationmap(priv->scanparams.u_config.dvb_cab.mods);
	dpr_info("%s: scan parameters are:\n", __func__);
	dpr_info("Freq = %d kHz\nSearch range = %d Hz\nSymbol Rate = %d Bds\n",
			sp.Frequency_kHz, sp.SearchRange_Hz, sp.SymbolRate_Bds);
	sp.SpectrumInversion = FE_CAB_SPECT_NORMAL;

	if (priv->scanparams.u_config.dvb_cab.mods ==
			STM_FE_DEMOD_MODULATION_AUTO) {
		num_trial = 6;
		sp.Modulation = cab_mod[0];
	} else {
		num_trial = 1;
	}

	while (i < num_trial) {
		err = FE_MXL214_Search((FE_MXL214_Handle_t) hdl,
						priv->demod_id, &sp, &rp);

		dpr_info("%s:i=%d freq=%d sym=%d mod=%d error=%d\n",
		__func__, i, sp.Frequency_kHz, sp.SymbolRate_Bds,
		sp.Modulation, err);

		if (rp.Locked) {
			dpr_info("%s: locked on i=%d freq=%d sym=%d mod=%d error=%d\n",
				__func__, i, sp.Frequency_kHz,
				sp.SymbolRate_Bds, sp.Modulation, err);
			break;
		}

		i++;

		if ((num_trial == 6) && (i < 6))
			sp.Modulation = cab_mod[i];

	}

	if (err) {
		icab->status = STM_FE_DEMOD_NO_SIGNAL;
		pr_err("%s: FE_MXL214_Search failed\n", __func__);
		return -EINVAL;
	}
	*lock = rp.Locked;
	if (!rp.Locked) {
		icab->status = STM_FE_DEMOD_NO_SIGNAL;
		pr_err("%s: demod not locked\n", __func__);
		return ret;
	}
	FILL_DVBC_INFO(icab, rp)
	dpr_info("%s: Demod %s locked\n", __func__, priv->demod_name);
	dpr_info("%s: Result params after successsful scan:\n", __func__);
	dpr_info("Freq = %d kHz Symbol rate = %d Bds\n", rp.Frequency_kHz,
							rp.SymbolRate_Bds);
	info->std = STM_FE_DEMOD_TX_STD_DVBC;
	icab->status = STM_FE_DEMOD_SYNC_OK;

	return ret;

}

/*
 * Name: mxl214_tracking()
 *
 * Description: Copying the lock parameters of demodulator
 *
 */
int mxl214_tracking(struct stm_fe_demod_s *priv)
{
	FE_MXL214_Error_t err;
	FE_MXL214_Handle_t hdl;
	FE_MXL214_SignalInfo_t sinfo;
	stm_fe_demod_channel_info_t *info =
		&priv->c_tinfo.u_info.tune.demod_info;
	stm_fe_demod_dvbc_channel_info_t *icab = &info->u_channel.dvbc;

	FE_MXL214_DEMOD_t Demod = priv->demod_id;
	hdl = priv->demod_params;
	err = FE_LLA_NO_ERROR;

	memset(&sinfo, '\0', sizeof(FE_MXL214_SignalInfo_t));

	err |= FE_MXL214_GetSignalInfo(hdl, Demod, &sinfo);
	if (!(err)) {
		info->std = STM_FE_DEMOD_TX_STD_DVBC;
		icab->freq = sinfo.Frequency;
		icab->sr = sinfo.SymbolRate;
		icab->inv = sinfo.SpectInversion;
		icab->mod = get_mod(sinfo.Modulation);
		icab->snr = sinfo.SNRx10dB_u32;
		icab->ber = sinfo.BERxE7_u32;
		icab->signal_strength = sinfo.RFLevelx10dBm_s32;
		icab->status = STM_FE_DEMOD_SYNC_OK;
	} else {
		icab->status = STM_FE_DEMOD_NO_SIGNAL;
		info->std = 0;
		icab->freq = 0;
		icab->sr = 0;
		icab->inv = 0;
		icab->mod = STM_FE_DEMOD_MODULATION_NONE;
		icab->snr = 0;
		icab->signal_strength = -100;
	}
	dpr_info("%s: Freq = %d KHz\nSymbol Rate = %d Bds\n"
			"SNR = %d dB\nBER = %d\n"
			"Signal strength = %d dBm\n", __func__,
			icab->freq, icab->sr, icab->snr,
			icab->ber, icab->signal_strength);

	return 0;
}
/*
 * Name: mxl214_status()
 * * Description: Checking the lock status of demodulator
 */

int mxl214_status(struct stm_fe_demod_s *priv, bool *locked)
{

	FE_MXL214_Handle_t hdl;
	stm_fe_demod_channel_info_t *info;
	stm_fe_demod_status_t *status;
	FE_MXL214_DEMOD_t Demod = priv->demod_id;

	hdl = priv->demod_params;
	info = &priv->c_tinfo.u_info.tune.demod_info;
	*locked = FE_MXL214_Status(hdl, Demod);

	if (info->std == STM_FE_DEMOD_TX_STD_DVBC)
		status = &info->u_channel.dvbc.status;
	else
		status = &info->u_channel.dvbc.status;

	if (*locked)
		*status = STM_FE_DEMOD_SYNC_OK;
	else
		*status = STM_FE_DEMOD_NO_SIGNAL;

	return 0;
}

/*
 * Name: mxl214_unlock()
 *
 * Description: disable the demodulator
 *
 */
int mxl214_unlock(struct stm_fe_demod_s *priv)
{
	FE_MXL214_Error_t err;
	FE_MXL214_Handle_t hdl;
	int ret = 0;
	stm_fe_demod_channel_info_t *info;
	FE_MXL214_DEMOD_t Demod = priv->demod_id;

	hdl = priv->demod_params;
	info = &priv->c_tinfo.u_info.tune.demod_info;

	err = FE_MXL214_Unlock(hdl, Demod);
	if (err == FE_LLA_INVALID_HANDLE) {
		dpr_err("%s: unlock failed due to invalid handle\n", __func__);
		ret = -EINVAL;
	} else if (err == FE_LLA_I2C_ERROR) {
		dpr_err("%s: unlock failed due to I2C err\n", __func__);
		ret = -EFAULT;
	}
	if (info->std == STM_FE_DEMOD_TX_STD_DVBC)
		info->u_channel.dvbc.status = STM_FE_DEMOD_STATUS_UNKNOWN;

	return ret;
}
/*
 * Name: mxl214_abort()
 * Description: Set the abort flag
 */
int mxl214_abort(struct stm_fe_demod_s *priv, bool abort)
{
	FE_MXL214_Error_t err;
	FE_MXL214_Handle_t hdl;
	int ret = 0;
	FE_MXL214_DEMOD_t Demod = priv->demod_id;
	hdl = priv->demod_params;
	err = FE_MXL214_SetAbortFlag(hdl, abort, Demod);
	if (err != FE_LLA_NO_ERROR) {
		pr_err("%s: failed\n", __func__);
		ret = -EINVAL;
	} else {
		dpr_info("%s: successful\n", __func__);
	}
	return ret;
}

/*
 * Name: mxl214_standby()
 *
 * Description: Set thedevice in Standby and recover
 *
 */
int mxl214_standby(struct stm_fe_demod_s *priv, bool standby)
{
	FE_MXL214_Error_t err;
	FE_MXL214_Handle_t hdl;
	int ret = 0;
	FE_MXL214_DEMOD_t Demod = priv->demod_id;
	hdl = priv->demod_params;
	err = FE_MXL214_SetStandby(hdl, standby, Demod);
	if (err != FE_LLA_NO_ERROR) {
		pr_err("%s: failed\n", __func__);
		ret = -EINVAL;
	} else {
		dpr_info("%s: successful\n", __func__);
	}

	return ret;
}

/*
 * Name: mxl214_restore()
 *
 * Description: Re-Initiliazing LLA after restoring from CPS
 *
 */
int mxl214_restore(struct stm_fe_demod_s *priv)
{
	int ret = 0;
	FE_MXL214_Error_t err = FE_LLA_NO_ERROR;

#if 0
	/*copy pParam structure ptr to demod_params to furthur access */
	err = mxl214_init(priv);
	if (err != FE_LLA_NO_ERROR) {
		pr_err("%s: LLA init failed\n", __func__);
		return -EINVAL;
	}

	priv->c_tinfo.u_info.tune.demod_info.u_channel.dvbc.status =
						STM_FE_DEMOD_STATUS_UNKNOWN;
#endif
	if (err != FE_LLA_NO_ERROR) {
		pr_err("%s: LLA init failed\n", __func__);
		return -EINVAL;
	}

	return ret;
}


static stm_fe_demod_modulation_t get_mod(FE_CAB_Modulation_t mod)
{
	stm_fe_demod_modulation_t fe_mod = STM_FE_DEMOD_MODULATION_NONE;
	switch (mod) {
	case FE_CAB_MOD_QAM4:
		fe_mod = STM_FE_DEMOD_MODULATION_4QAM;
		break;
	case FE_CAB_MOD_QAM16:
		fe_mod = STM_FE_DEMOD_MODULATION_16QAM;
		break;
	case FE_CAB_MOD_QAM32:
		fe_mod = STM_FE_DEMOD_MODULATION_32QAM;
		break;
	case FE_CAB_MOD_QAM64:
		fe_mod = STM_FE_DEMOD_MODULATION_64QAM;
		break;
	case FE_CAB_MOD_QAM128:
		fe_mod = STM_FE_DEMOD_MODULATION_128QAM;
		break;
	case FE_CAB_MOD_QAM256:
		fe_mod = STM_FE_DEMOD_MODULATION_256QAM;
		break;
/*	case FE_CAB_MOD_QPSK:
		fe_mod = STM_FE_DEMOD_MODULATION_QPSK;
		break;
	case FE_CAB_MOD_AUTO:
		fe_mod = STM_FE_DEMOD_MODULATION_AUTO;
		break;*/
	default:
		fe_mod = STM_FE_DEMOD_MODULATION_NONE;
		break;
	}
	return fe_mod;
}

static FE_CAB_Modulation_t modulationmap(stm_fe_demod_modulation_t mod)
{
	FE_CAB_Modulation_t lla_mod;
	switch (mod) {
	case STM_FE_DEMOD_MODULATION_4QAM:
		lla_mod = FE_CAB_MOD_QAM4;
		break;
	case STM_FE_DEMOD_MODULATION_16QAM:
		lla_mod = FE_CAB_MOD_QAM16;
		break;
	case STM_FE_DEMOD_MODULATION_32QAM:
		lla_mod = FE_CAB_MOD_QAM32;
		break;
	case STM_FE_DEMOD_MODULATION_64QAM:
		lla_mod = FE_CAB_MOD_QAM64;
		break;
	case STM_FE_DEMOD_MODULATION_128QAM:
		lla_mod = FE_CAB_MOD_QAM128;
		break;
	case STM_FE_DEMOD_MODULATION_256QAM:
		lla_mod = FE_CAB_MOD_QAM256;
		break;
/*	case STM_FE_DEMOD_MODULATION_AUTO:
		lla_mod = FE_CAB_MOD_AUTO;
		break;
	case STM_FE_DEMOD_MODULATION_QPSK:
		lla_mod = FE_CAB_MOD_QPSK;
		break;*/
	default:
		lla_mod = -1;
		break;
	}
	return lla_mod;
}

static int32_t __init mxl214_lla_init(void)
{
	pr_info("Loading mxl214 driver module...\n");

	return 0;
}
module_init(mxl214_lla_init);

static void __exit mxl214_lla_term(void)
{
	pr_info("Removing mxl214 driver module ...\n");

}
module_exit(mxl214_lla_term);


MODULE_DESCRIPTION("Low level driver for MXL214 cable demodulator");
MODULE_VERSION("4.1.5.4");
MODULE_LICENSE("GPL");
