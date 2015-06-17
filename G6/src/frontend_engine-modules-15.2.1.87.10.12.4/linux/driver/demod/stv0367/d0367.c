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

Source file name : d0367.c
Author :           Ankur

wrapper for stv0367 LLA

Date        Modification                                    Name
----        ------------                                    --------
30-Jul-11   Created                                         Ankur

************************************************************************/

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/list.h>
#include <linux/platform_device.h>
#include <linux/stm/plat_dev.h>
#include <linux/stm/demod.h>
#include <stm_registry.h>
#include <stm_fe.h>
#include <stfe_utilities.h>
#include <stm_fe_os.h>
#include <i2c_wrapper.h>
#include <stm_fe_demod.h>
#include <fe_tc_tuner.h>
#include <lla_utils.h>
#include "d0367.h"
#include "stv0367qam_init.h"
#include "stv0367ofdm_init.h"

#define STV0367_CURRENT_LLA_REVISION "OFDM : STV367ofdm-LLA_REL_3.5 &" \
					" QAM : STV0367QAM-LLA_REL_1.9"


static int lla_wrapper_debug;
module_param_named(lla_wrapper_debug, lla_wrapper_debug, int, 0644);
MODULE_PARM_DESC(lla_wrapper_debug,
		"Turn on/off stv0367 lla wrapper debugging (default:off).");

#define dpr_err(x...) do { if (lla_wrapper_debug) pr_err(x); } while (0)
#define dpr_info(x...) do { if (lla_wrapper_debug) pr_info(x); } while (0)

static int stv0367_init(struct stm_fe_demod_s *priv);
static int stv0367_tune(struct stm_fe_demod_s *priv, bool *lock);
static int stv0367_scan(struct stm_fe_demod_s *priv, bool *lock);
static int stv0367_tracking(struct stm_fe_demod_s *priv);
static int stv0367_status(struct stm_fe_demod_s *priv, bool *locked);
static int stv0367_unlock(struct stm_fe_demod_s *priv);
static int stv0367_term(struct stm_fe_demod_s *priv);
static int stv0367qam_tune(struct stm_fe_demod_s *priv, bool *lock);
static int stv0367qam_scan(struct stm_fe_demod_s *priv, bool *lock);
static int stv0367ofdm_tune(struct stm_fe_demod_s *priv, bool *lock);
static int stv0367ofdm_scan(struct stm_fe_demod_s *priv, bool *lock);
static int stv0367qam_init(struct stm_fe_demod_s *priv);
static int stv0367ofdm_init(struct stm_fe_demod_s *priv);
static int stv0367_abort(struct stm_fe_demod_s *priv, bool abort);
static int stv0367_standby(struct stm_fe_demod_s *priv, bool standby);
static int stv0367_restore(struct stm_fe_demod_s *priv);
static FE_CAB_Modulation_t modulationmap(stm_fe_demod_modulation_t mod);
static FE_TER_Guard_t guardmap(stm_fe_demod_guard_t guard);
static FE_TER_FFT_t fftmodemap(stm_fe_demod_fft_mode_t fft_mode);
static FE_TER_Hierarchy_t hierarchymap(stm_fe_demod_hierarchy_t hier);
static FE_TER_ChannelBW_t channelbwmap(uint32_t bw);
static stm_fe_demod_modulation_t get_mod(FE_CAB_Modulation_t mod);
static uint32_t get_bw(FE_TER_ChannelBW_t bw);
static stm_fe_demod_guard_t get_guard(FE_TER_Guard_t guard);
static stm_fe_demod_fft_mode_t get_fftmode(FE_TER_FFT_t fft_mode);
static stm_fe_demod_hierarchy_t get_hierarchy(FE_TER_Hierarchy_t hier);
static stm_fe_demod_fec_rate_t get_fec(FE_TER_FECRate_t fec);
static stm_fe_demod_hierarchy_alpha_t get_alpha(FE_TER_Hierarchy_Alpha_t alpha);
static stm_fe_demod_modulation_t get_ter_mod(FE_TER_Modulation_t Mod);


/*
 * Name: stm_fe_stv0367_attach()
 *
 * Description: Installed the wrapper function pointers for LLA
 *
 */
int stm_fe_stv0367_attach(struct stm_fe_demod_s *priv)
{
	if (!priv->ops)
		return -EFAULT;

	priv->ops->init = stv0367_init;
	priv->ops->tune = stv0367_tune;
	priv->ops->scan = stv0367_scan;
	priv->ops->tracking = stv0367_tracking;
	priv->ops->status = stv0367_status;
	priv->ops->unlock = stv0367_unlock;
	priv->ops->term = stv0367_term;
	priv->ops->abort = stv0367_abort;
	priv->ops->standby = stv0367_standby;
	priv->ops->detach = stm_fe_stv0367_detach;
	priv->ops->restore = stv0367_restore;
	priv->ops->tsclient_init = NULL;
	priv->ops->tsclient_term = NULL;
	priv->ops->tsclient_pid_configure = NULL;
	priv->ops->tsclient_pid_deconfigure = NULL;
	dpr_info("%s: demod attached", __func__);

	return 0;
}
EXPORT_SYMBOL(stm_fe_stv0367_attach);

/*
 * Name: stm_fe_stv0367_detach()
 *
 * Description: Uninstalled the wrapper function pointers for LLA
 *
 */
int stm_fe_stv0367_detach(struct stm_fe_demod_s *priv)
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
EXPORT_SYMBOL(stm_fe_stv0367_detach);

void configure_ts_parameter(enum demod_tsout_config_e ts_out,
					FE_STV0367_TSConfig_t *path_ts_config)
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
 * Name: stv0367qam_tune()
 *
 * Description: Provides tuning parameters to stv0367qam LLA and perform tuning
 *
 */
static int stv0367qam_tune(struct stm_fe_demod_s *priv, bool *lock)
{
	int ret = 0;
	FE_STV0367_Error_t err = FE_LLA_NO_ERROR;
	FE_STV0367_SearchParams_t sp;
	FE_STV0367_SearchResult_t rp;
	FE_367qam_InternalParams_t *ip;
	void *params = NULL;
	stm_fe_demod_channel_info_t *info;
	stm_fe_demod_dvbc_tune_params_t *tp = NULL;
	stm_fe_demod_dvbc_channel_info_t *icab = NULL;

	tp = &priv->tp.u_tune.dvbc;

	info = &priv->c_tinfo.u_info.tune.demod_info;
	icab = &info->u_channel.dvbc;
	memset(&rp, '\0', sizeof(FE_STV0367_SearchResult_t));
	memset(&sp, '\0', sizeof(FE_STV0367_SearchParams_t));

	params = priv->demod_params;
	if (!params) {
		pr_err("%s: invalid demod params\n", __func__);
		return -EINVAL;
	}
	priv->demod_h->pRegMapImage = priv->demod_h->qamregmap;
	if (IS_NAME_MATCH("STV0367", priv->config->demod_name)) {
		err = stv0367qam_init(priv);
		if (err != FE_LLA_NO_ERROR)
			return -EINVAL;

		ip = (FE_367qam_InternalParams_t *)params;
		err = FE_SwitchTunerToDVBC(ip->hTuner);
		if (err != FE_LLA_NO_ERROR) {
			pr_err("%s: FE_SwitchTunerToDVBC failed\n", __func__);
			return -EINVAL;
		}
	}
	FILL_CAB_SEARCH_PARAMS_DVBC(sp, tp)
	dpr_info("%s: tune params for cable :\n", __func__);
	dpr_info("Freq = %d kHz\nSearch Range = %d Hz\nSymbol Rate = %d bds\n",
			sp.Frequency_kHz, sp.SearchRange_Hz, sp.SymbolRate_Bds);
	err = FE_367qam_Search((FE_367qam_Handle_t)params, &sp, &rp);
	if (err) {
		icab->status = STM_FE_DEMOD_NO_SIGNAL;
		pr_err("%s: FE_367qam_Search failed\n", __func__);
		return -EINVAL;
	}
	*lock = rp.Locked;
	if (!rp.Locked) {
		icab->status = STM_FE_DEMOD_NO_SIGNAL;
		pr_err("%s: demod not locked\n", __func__);
		return ret;
	}
	dpr_info("%s: demod %s locked\n", __func__, priv->demod_name);

	FILL_DVBC_INFO(icab, rp)

	info->std = STM_FE_DEMOD_TX_STD_DVBC;
	icab->status = STM_FE_DEMOD_SYNC_OK;
	dpr_info("%s: result params after lock :\n", __func__);
	dpr_info("Freq = %d kHz\nSymbol Rate = %d bds\n", rp.Frequency_kHz,
							rp.SymbolRate_Bds);
	return ret;
}

static int stv0367qam_scan(struct stm_fe_demod_s *priv, bool *lock)
{
	int ret = 0;
	FE_STV0367_Error_t err = FE_LLA_NO_ERROR;
	FE_STV0367_SearchParams_t sp;
	FE_STV0367_SearchResult_t rp;
	FE_367qam_InternalParams_t *ip;
	stm_fe_demod_channel_info_t *info;
	stm_fe_demod_dvbc_channel_info_t *icab = NULL;
	void *params = NULL;
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
	memset(&rp, '\0', sizeof(FE_STV0367_SearchResult_t));
	memset(&sp, '\0', sizeof(FE_STV0367_SearchParams_t));

	params = priv->demod_params;
	if (!params) {
		pr_err("%s: invalid demod params\n", __func__);
		return -EINVAL;
	}
	priv->demod_h->pRegMapImage = priv->demod_h->qamregmap;
	err = stv0367qam_init(priv);
	if (err != FE_LLA_NO_ERROR) {
		dpr_err("%s: stv0367qam_init failed\n", __func__);
		return -EINVAL;
	}
	ip = (FE_367qam_InternalParams_t *) params;
	err = FE_SwitchTunerToDVBC(ip->hTuner);
	if (err != FE_LLA_NO_ERROR) {
		pr_err("%s: FE_SwitchTunerToDVBC failed\n", __func__);
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
		err = FE_367qam_Search((FE_367qam_Handle_t)params, &sp, &rp);

		dpr_info("\n%s: i=%d freq=%d sym=%d mod=%d error=%d\n",
			__func__, i, sp.Frequency_kHz, sp.SymbolRate_Bds,
			sp.Modulation, err);

		if (rp.Locked) {
			dpr_info("\n%s: locked on i=%d freq=%d sym=%d mod=%d error=%d\n",
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
		pr_err("%s: FE_367qam_Search failed\n", __func__);
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
 * Name: stv0367ofdm_tune()
 *
 * Description: Provided tuning parameters to stv0367ofdm LLA
 * and perfomed tuning
 *
 */

static int stv0367ofdm_tune(struct stm_fe_demod_s *priv, bool *lock)
{
	int ret = 0;
	FE_STV0367_Error_t err = FE_LLA_NO_ERROR;
	FE_TER_SearchParams_t sp;
	FE_TER_SearchResult_t rp;
	FE_367ofdm_InternalParams_t *ip;
	void *params = NULL;
	stm_fe_demod_dvbt_tune_params_t *tp = NULL;
	stm_fe_demod_channel_info_t *info;
	stm_fe_demod_dvbt_channel_info_t *iterr = NULL;

	tp = &priv->tp.u_tune.dvbt;

	info = &priv->c_tinfo.u_info.tune.demod_info;
	iterr = &info->u_channel.dvbt;

	memset(&rp, '\0', sizeof(FE_TER_SearchResult_t));
	memset(&sp, '\0', sizeof(FE_TER_SearchParams_t));

	params = priv->add_demod_params;
	if (!params) {
		pr_err("%s: invalid demod params\n", __func__);
		return -EINVAL;
	}
	priv->demod_h->pRegMapImage = priv->demod_h->ofdmregmap;

	if (IS_NAME_MATCH("STV0367", priv->config->demod_name)) {
		err = stv0367ofdm_init(priv);
		if (err != FE_LLA_NO_ERROR) {
			dpr_err("%s: stv0367ofdm_init failed\n", __func__);
			return -EINVAL;
		}
		ip = (FE_367ofdm_InternalParams_t *) params;
		err = FE_SwitchTunerToDVBT(ip->hTuner);
		if (err != FE_LLA_NO_ERROR) {
			pr_err("%s: FE_SwitchTunerToDVBT failed\n", __func__);
			return -EINVAL;
		}
	}
	FILL_TER_SEARCH_PARAMS_DVBT(sp, tp, priv)
	dpr_info("%s: TER search params are:\n", __func__);
	dpr_info("Freq = %d kHz\nChannel Bw = %d MHz\n", sp.Frequency_kHz,
							sp.ChannelBW_MHz);

	err = FE_367ofdm_Search((FE_367qam_Handle_t)params, &sp, &rp);
	if (err) {
		iterr->status = STM_FE_DEMOD_NO_SIGNAL;
		pr_err("%s: FE_367ofdm_Search failed\n", __func__);
		return -EINVAL;
	}
	*lock = rp.Locked;
	if (!rp.Locked) {
		iterr->status = STM_FE_DEMOD_NO_SIGNAL;
		pr_err("%s: demod not locked\n", __func__);
		return ret;
	}
	dpr_info("%s: Demod %s locked\n", __func__, priv->demod_name);
	FILL_DVBT_INFO(iterr, sp, rp)

	info->std = STM_FE_DEMOD_TX_STD_DVBT;
	iterr->status = STM_FE_DEMOD_SYNC_OK;
	dpr_info("%s: Result params after lock:\n", __func__);
	dpr_info("Freq = %d kHz BW = %d MHz\n", iterr->freq,
						channelbwmap(iterr->bw));
	return ret;
}

static int stv0367ofdm_scan(struct stm_fe_demod_s *priv, bool *lock)
{
	int ret = 0;
	FE_STV0367_Error_t err = FE_LLA_NO_ERROR;
	FE_TER_SearchParams_t sp;
	FE_TER_SearchResult_t rp;
	FE_367ofdm_InternalParams_t *ip;
	stm_fe_demod_channel_info_t *info;
	void *params = NULL;
	stm_fe_demod_dvbt_channel_info_t *iterr = NULL;

	info = &priv->c_tinfo.u_info.scan.demod_info;
	iterr = &info->u_channel.dvbt;

	memset(&rp, '\0', sizeof(FE_TER_SearchResult_t));
	memset(&sp, '\0', sizeof(FE_TER_SearchParams_t));
	params = priv->add_demod_params;
	if (!params) {
		pr_err("%s: invalid demod params\n", __func__);
		return -EINVAL;
	}
	priv->demod_h->pRegMapImage = priv->demod_h->ofdmregmap;

	err = stv0367ofdm_init(priv);
	if (err != FE_LLA_NO_ERROR)
		return -EINVAL;

	ip = (FE_367ofdm_InternalParams_t *) params;
	err = FE_SwitchTunerToDVBT(ip->hTuner);
	if (err != FE_LLA_NO_ERROR) {
		pr_err("%s: FE_SwitchTunerToDVBT failed\n", __func__);
		return -EINVAL;
	}

	sp.Frequency_kHz = priv->fe_scan.search_freq;
	sp.Optimize.Guard = STM_FE_DEMOD_GUARD_AUTO;
	sp.Optimize.FFTSize = STM_FE_DEMOD_FFT_MODE_AUTO;
	sp.Optimize.SpectInversion = FE_TER_INVERSION_Unknown;
	sp.ChannelBW_MHz = channelbwmap(priv->scanparams.u_config.dvb_ter.bw);
	if (priv->config->custom_flags == DEMOD_CUSTOM_IQ_WIRING_INVERTED)
		sp.IF_IQ_Mode = FE_TER_IQ_TUNER;
	else
		sp.IF_IQ_Mode = FE_TER_NORMAL_IF_TUNER;
	dpr_info("%s: Scan params are:\n", __func__);
	dpr_info("%s: Freq = %d kHz BW = %d MHz\n", __func__, sp.Frequency_kHz,
							sp.ChannelBW_MHz);

	err = FE_367ofdm_Search((FE_367qam_Handle_t)params, &sp, &rp);
	if (err) {
		iterr->status = STM_FE_DEMOD_NO_SIGNAL;
		/*pr_err("%s: search failed, demod is not locked\n",
		       __func__);*/
		return -EINVAL;
	}
	*lock = rp.Locked;
	if (!rp.Locked) {
		iterr->status = STM_FE_DEMOD_NO_SIGNAL;
		/*pr_err("%s: demod is not locked\n", __func__);*/
		return ret;
	}
	dpr_info("%s: Demod %s locked:\n", __func__, priv->demod_name);
	FILL_DVBT_INFO(iterr, sp, rp)

	info->std = STM_FE_DEMOD_TX_STD_DVBT;
	iterr->status = STM_FE_DEMOD_SYNC_OK;
	dpr_info("%s: Result params after lock:\n", __func__);
	dpr_info("%s: Freq = %d kHz BW = %d MHz\n", __func__, iterr->freq,
						channelbwmap(iterr->bw));
	return ret;
}

/*
 * Name: stv0367ofdm_init()
 *
 * Description: Providing Initiliazing parameters to stv0367ofdm LLA
 *
 */

static int stv0367ofdm_init(struct stm_fe_demod_s *priv)
{
	int ret = 0;
	FE_STV0367_Error_t err = FE_LLA_NO_ERROR;
	FE_TER_InitParams_t ip;

	memset(&ip, '\0', sizeof(FE_TER_InitParams_t));
	strncpy(ip.DemodName, "STV0367", sizeof(ip.DemodName));	/* Demod name
	*/
	ip.DemodI2cAddr = priv->config->demod_io.address;
	ip.TunerModel = stmfe_set_tuner_model(priv->config->tuner_name);
	strncpy(ip.TunerName, "Tuner", sizeof(ip.TunerName));
	ip.TunerI2cAddr = priv->config->tuner_io.address;

	/* Refrence clock for the Demodulator in Hz (27MHz,30MHz,....) */

	ip.Demod_Crystal_Hz = priv->config->clk_config.demod_clk;
	ip.Tuner_Crystal_Hz = priv->config->clk_config.tuner_clk;
	ip.TunerIF_kHz = priv->config->tuner_if;

	if (priv->config->ts_out & DEMOD_TS_SERIAL_PUNCT_CLOCK)
		ip.PathTSClock = FE_TS_SERIAL_PUNCT_CLOCK;
	else if (priv->config->ts_out & DEMOD_TS_SERIAL_CONT_CLOCK)
		ip.PathTSClock = FE_TS_SERIAL_CONT_CLOCK;
	else if (priv->config->ts_out & DEMOD_TS_PARALLEL_PUNCT_CLOCK)
		ip.PathTSClock = FE_TS_PARALLEL_PUNCT_CLOCK;
	else if (priv->config->ts_out & DEMOD_TS_PARALLEL_PUNCT_CLOCK)
		ip.PathTSClock = FE_TS_PARALLEL_PUNCT_CLOCK;
	else if (priv->config->ts_out & DEMOD_TS_DVBCI_CLOCK)
		ip.PathTSClock = FE_TS_DVBCI_CLOCK;

	ip.ClockPolarity = (priv->config->ts_out & DEMOD_TS_RISINGEDGE_CLOCK) ?
			FE_TS_RISINGEDGE_CLOCK : FE_TS_FALLINGEDGE_CLOCK;
	dpr_info("%s: lla init params are:\n", __func__);
	dpr_info("%s: Demod name = %s\nDemodI2Caddress = 0x%x\n"
			  "Tuner Model = %d\nTuner name = %s\n"
			  "Tuner I2C address = 0x%x\n", __func__, ip.DemodName,
			  ip.DemodI2cAddr, ip.TunerModel, ip.TunerName,
			  ip.TunerI2cAddr);
	/* assign ofdm_regmap for ofdm initialization */
	priv->demod_h->pRegMapImage = priv->demod_h->ofdmregmap;
	err = FE_367ofdm_Init(&ip, &priv->add_demod_params);

	if (err != FE_LLA_NO_ERROR) {
		pr_err("%s: LLA init failed\n", __func__);
		return -EINVAL;
	}

	priv->tp.std = STM_FE_DEMOD_TX_STD_DVBT;
	priv->c_tinfo.u_info.tune.demod_info.u_channel.dvbt.status =
		STM_FE_DEMOD_STATUS_UNKNOWN;

	return ret;
}

/*
 * Name: stv0367qam_init()
 *
 * Description: Providing Initiliazing parameters to stv0367qam LLA
 *
 */
static int stv0367qam_init(struct stm_fe_demod_s *priv)
{
	int ret = 0;
	FE_STV0367_Error_t err = FE_LLA_NO_ERROR;
	FE_CAB_InitParams_t ip;

	memset(&ip, '\0', sizeof(FE_CAB_InitParams_t));
	strncpy(ip.DemodName, "STV0367", sizeof(ip.DemodName));	/* Demod name */
	ip.DemodI2cAddr = priv->config->demod_io.address;
	ip.TunerModel = stmfe_set_tuner_model(priv->config->tuner_name);
	strncpy(ip.TunerName, "Tuner", sizeof(ip.TunerName));
	ip.TunerI2cAddr = priv->config->tuner_io.address;

	/* Refrence clock for the Demodulator in Hz (27MHz,30MHz,....) */

	ip.DemodXtal_Hz = priv->config->clk_config.demod_clk;
	ip.TunerXtal_Hz = priv->config->clk_config.tuner_clk;
	ip.TunerIF_kHz = priv->config->tuner_if;

	configure_ts_parameter(priv->config->ts_out, &ip.TS_Config);
	/* assign qam_regmap for qam initialization */
	priv->demod_h->pRegMapImage = priv->demod_h->qamregmap;
	dpr_info("%s: lla init params are:\n", __func__);
	dpr_info("%s: Demod name = %s\n DemodI2Caddress = 0x%x\n"
			  "Tuner Model = %d\n Tuner name = %s\n"
			  " Tuner I2C address = 0x%x\n",
			  __func__, ip.DemodName, ip.DemodI2cAddr,
			  ip.TunerModel, ip.TunerName, ip.TunerI2cAddr);
	err = FE_367qam_Init(&ip, &priv->demod_params);
	if (err != FE_LLA_NO_ERROR) {
		pr_err("%s: LLA init failed\n", __func__);
		return -EINVAL;
	}

	priv->tp.std = STM_FE_DEMOD_TX_STD_DVBC;
	priv->c_tinfo.u_info.tune.demod_info.u_channel.dvbc.status =
						STM_FE_DEMOD_STATUS_UNKNOWN;

	return ret;
}

/*
 * Name: stv0367_init()
 *
 * Description: Providing Initiliazing parameters to LLA
 *
 */
static int stv0367_init(struct stm_fe_demod_s *priv)
{
	FE_STV0367_InternalParams_t *qp;
	FE_367ofdm_InternalParams_t *op;
	int ret = 0;
	FE_STV0367_Error_t err = FE_LLA_NO_ERROR;
	size_t qam_size, ofdm_size;

	priv->demod_h->pData = NULL;
	/*allocate the memory for LLA internal parameters */

	priv->demod_params =
		(void *)stm_fe_malloc(sizeof(FE_STV0367_InternalParams_t));
	if (!priv->demod_params) {
		pr_err("%s: mem alloc failed for qam params\n", __func__);
		return -ENOMEM;
	}
	qp = (FE_STV0367_InternalParams_t *)priv->demod_params;

	priv->add_demod_params =
		(void *)stm_fe_malloc(sizeof(FE_367ofdm_InternalParams_t));
	if (!priv->add_demod_params) {
		stm_fe_free((void **)&qp);
		pr_err("%s: mem alloc failed for ofdm params\n", __func__);
		return -ENOMEM;
	}
	op = (FE_367ofdm_InternalParams_t *)priv->add_demod_params;

	priv->demod_i2c = chip_open(priv->config->demod_io.bus);
	if (!priv->demod_i2c) {
		dpr_err("%s: Failed to get i2c adaptor", __func__);
		stm_fe_free((void **)&op);
		stm_fe_free((void **)&qp);
		return -EINVAL;
	}

	qam_size = STV0367qam_NBREGS * sizeof(STCHIP_Register_t);
	priv->demod_h->qamregmap = stm_fe_malloc(qam_size);
	if (!priv->demod_h->qamregmap) {
		priv->demod_i2c = NULL;
		stm_fe_free((void **)&op);
		stm_fe_free((void **)&qp);
		pr_err("%s: mem alloc failed for QAM reg map\n", __func__);
		return -ENOMEM;
	}

	ofdm_size = STV0367ofdm_NBREGS * sizeof(STCHIP_Register_t);
	priv->demod_h->ofdmregmap = stm_fe_malloc(ofdm_size);
	if (!priv->demod_h->ofdmregmap) {
		priv->demod_i2c = NULL;
		stm_fe_free((void **)&priv->demod_h->qamregmap);
		stm_fe_free((void **)&op);
		stm_fe_free((void **)&qp);
		pr_err("%s: mem alloc failed for OFDM reg map\n", __func__);
		return -ENOMEM;
	}
	/*To enable the auto repeater mode */
	priv->demod_h->Abort = false;
	priv->tuner_h->Abort = false;
	/*copy demod/tuner chip structure to params */
	qp->hDemod = priv->demod_h;
	op->hDemod = priv->demod_h;
#ifdef CONFIG_ARCH_STI
	priv->demod_h->IsAutoRepeaterOffEnable = false;
#else
	priv->demod_h->IsAutoRepeaterOffEnable = true;
#endif

	qp->hTuner = (TUNER_Handle_t) priv->tuner_h;
	op->hTuner = (TUNER_Handle_t) priv->tuner_h;

	memcpy(&(qp->hDemod->dev_i2c), &(priv->demod_i2c),
			sizeof(priv->demod_i2c));
	memcpy(&(op->hDemod->dev_i2c), &(priv->demod_i2c),
			sizeof(priv->demod_i2c));
	memcpy(&(qp->hTuner->dev_i2c), &(priv->demod_i2c),
			sizeof(priv->demod_i2c));
	memcpy(&(op->hTuner->dev_i2c), &(priv->demod_i2c),
			sizeof(priv->demod_i2c));

	qp->hDemod->I2cAddr = priv->config->demod_io.address;
	qp->hDemod->IORoute = priv->config->demod_io.route;
	qp->hDemod->IODriver = priv->config->demod_io.io;

	qp->hTuner->I2cAddr = priv->config->tuner_io.address;
	qp->hTuner->IORoute = priv->config->tuner_io.route;
	qp->hTuner->IODriver = priv->config->tuner_io.io;

	op->hDemod->I2cAddr = priv->config->demod_io.address;
	op->hDemod->IORoute = priv->config->demod_io.route;
	op->hDemod->IODriver = priv->config->demod_io.io;

	op->hTuner->I2cAddr = priv->config->tuner_io.address;
	op->hTuner->IORoute = priv->config->tuner_io.route;
	op->hTuner->IODriver = priv->config->tuner_io.io;

	if (IS_NAME_MATCH("STV0367C", priv->config->demod_name) ||
		IS_NAME_MATCH("STV0367", priv->config->demod_name)) {
		/*copy pParam structure ptr to demod_params to furthur access */
		err = stv0367qam_init(priv);
		if (err != FE_LLA_NO_ERROR) {
			pr_err("%s: LLA init failed\n", __func__);
			priv->demod_i2c = NULL;
			stm_fe_free((void **)&priv->demod_h->ofdmregmap);
			stm_fe_free((void **)&priv->demod_h->qamregmap);
			stm_fe_free((void **)&op);
			stm_fe_free((void **)&qp);
			return -EINVAL;
		}
	}
	if (IS_NAME_MATCH("STV0367T", priv->config->demod_name) ||
		IS_NAME_MATCH("STV0367", priv->config->demod_name)) {
		/*copy pParam structure ptr to demod_params to furthur access */
		err = stv0367ofdm_init(priv);
		if (err != FE_LLA_NO_ERROR) {
			pr_err("%s: LLA init failed\n", __func__);
			priv->demod_i2c = NULL;
			stm_fe_free((void **)&priv->demod_h->ofdmregmap);
			stm_fe_free((void **)&priv->demod_h->qamregmap);
			stm_fe_free((void **)&op);
			stm_fe_free((void **)&qp);
			return -EINVAL;
		}
	}

	priv->c_tinfo.u_info.tune.demod_info.u_channel.dvbc.status =
						STM_FE_DEMOD_STATUS_UNKNOWN;
	priv->c_tinfo.u_info.tune.demod_info.u_channel.dvbt.status =
						STM_FE_DEMOD_STATUS_UNKNOWN;

	return ret;
}

/*
 * Name: stv0367_term()
 *
 * Description: terminating the stv0367 LLA
 *
 */

static int stv0367_term(struct stm_fe_demod_s *priv)
{
	int ret = -1;
	priv->demod_i2c = NULL;
	if (!priv->demod_h) {
		pr_err("%s: demod handle is NULL\n", __func__);
		return -EINVAL;
	}
	priv->demod_h->dev_i2c = NULL;
	priv->tuner_h->dev_i2c = NULL;
	ret = stm_fe_free((void **)&priv->demod_h->ofdmregmap);
	if (ret)
		return ret;
	ret = stm_fe_free((void **)&priv->demod_h->qamregmap);
	if (ret)
		return ret;
	ret = stm_fe_free((void **)&priv->demod_params);
	if (ret)
		return ret;
	ret = stm_fe_free((void **)&priv->add_demod_params);

	return ret;
}

/*
 * Name: stv0367_tune()
 *
 * Description: Provided tuning parameters to LLA and perfomed tuning
 *
 */
static int stv0367_tune(struct stm_fe_demod_s *priv, bool *lock)
{
	FE_STV0367_Error_t err = FE_LLA_NO_ERROR;
	int ret = 0;

	if (priv->tp.std == STM_FE_DEMOD_TX_STD_DVBC) {
		err = stv0367qam_tune(priv, lock);
	} else if (priv->tp.std == STM_FE_DEMOD_TX_STD_DVBT) {
		err = stv0367ofdm_tune(priv, lock);
	} else {
		pr_err("%s: incorrect std\n", __func__);
		ret = -EINVAL;
	}
	if (err != FE_LLA_NO_ERROR) {
		pr_err("%s: failed\n", __func__);
		ret = -EINVAL;
	}

	return ret;
}

/*
 * Name: stv0367_scan()
 *
 * Description: Provided scanning parameters to LLA and perfomed tuning
 *
 */
static int stv0367_scan(struct stm_fe_demod_s *priv, bool *lock)
{
	int ret = 0;
	stm_fe_demod_scan_tx_std_t scan_stds;

	scan_stds = priv->scanparams.scan_stds;

	if (scan_stds & STM_FE_DEMOD_SCAN_TX_STD_DVB_CAB) {
		ret = stv0367qam_scan(priv, lock);
	} else if (scan_stds & STM_FE_DEMOD_SCAN_TX_STD_DVB_TER) {
			ret = stv0367ofdm_scan(priv, lock);
	} else {
		pr_err("%s: incorrect std\n", __func__);
		ret = -EINVAL;
	}

	return ret;
}

/*
 * Name: stv0367_abort()
 *
 * Description: Abort demod and tuner operations
 *
 */
static int stv0367_abort(struct stm_fe_demod_s *priv, bool abort)
{
	FE_STV0367_Error_t err = FE_LLA_NO_ERROR;
	int ret = 0;
	void *params = NULL;
	stm_fe_demod_scan_tx_std_t scan_stds;

	scan_stds = priv->scanparams.scan_stds;

	if ((priv->tp.std == STM_FE_DEMOD_TX_STD_DVBC) ||
			(scan_stds == STM_FE_DEMOD_SCAN_TX_STD_DVB_CAB)) {
		params = priv->demod_params;
		priv->demod_h->pRegMapImage = priv->demod_h->qamregmap;
		err = FE_367qam_SetAbortFlag(params, abort);
	} else if ((priv->tp.std == STM_FE_DEMOD_TX_STD_DVBT) ||
			(scan_stds == STM_FE_DEMOD_SCAN_TX_STD_DVB_TER)) {
		params = priv->add_demod_params;
		priv->demod_h->pRegMapImage = priv->demod_h->ofdmregmap;
		err = FE_367ofdm_SetAbortFlag(params, abort);
	}

	if (err != FE_LLA_NO_ERROR) {
		pr_err("%s: failed\n", __func__);
		ret = -EINVAL;
	}

	return ret;
}

/*
 * Name: stv0367_standby()
 *
 * Description: Enter/Exit from low power state.
 *
 */
static int stv0367_standby(struct stm_fe_demod_s *priv, bool standby)
{
	FE_STV0367_Error_t err = FE_LLA_NO_ERROR;
	int ret = 0;
	void *params = NULL;
	stm_fe_demod_scan_tx_std_t scan_stds;

	scan_stds = priv->scanparams.scan_stds;

	if ((priv->tp.std == STM_FE_DEMOD_TX_STD_DVBC) ||
			(scan_stds == STM_FE_DEMOD_SCAN_TX_STD_DVB_CAB)) {
		params = priv->demod_params;
		priv->demod_h->pRegMapImage = priv->demod_h->qamregmap;
		err = FE_367qam_SetStandby(params, standby);
	} else if ((priv->tp.std == STM_FE_DEMOD_TX_STD_DVBT) ||
			(scan_stds == STM_FE_DEMOD_SCAN_TX_STD_DVB_TER)) {
		params = priv->add_demod_params;
		priv->demod_h->pRegMapImage = priv->demod_h->ofdmregmap;
		err = FE_STV0367ofdm_SetStandby(params, standby);
	} else {
		pr_err("%s: incorrect std\n", __func__);
		ret = -EINVAL;
	}
	if (err != FE_LLA_NO_ERROR) {
		pr_err("%s: failed\n", __func__);
		ret = -EINVAL;
	}

	return ret;
}

/*
 * Name: stv0367_restore()
 *
 * Description: Re-Initiliazing LLA after restoring from CPS
 *
 */
static int stv0367_restore(struct stm_fe_demod_s *priv)
{
	int ret = 0;
	FE_STV0367_Error_t err = FE_LLA_NO_ERROR;

	if (IS_NAME_MATCH("STV0367C", priv->config->demod_name) ||
		      IS_NAME_MATCH("STV0367", priv->config->demod_name)) {
		/*copy pParam structure ptr to demod_params to furthur access */
		err = stv0367qam_init(priv);
		if (err != FE_LLA_NO_ERROR) {
			pr_err("%s: LLA init failed\n", __func__);
			return -EINVAL;
		}
	}
	if (IS_NAME_MATCH("STV0367T", priv->config->demod_name) ||
		      IS_NAME_MATCH("STV0367", priv->config->demod_name)) {
		/*copy pParam structure ptr to demod_params to furthur access */
		err = stv0367ofdm_init(priv);
		if (err != FE_LLA_NO_ERROR) {
			pr_err("%s: LLA init failed\n", __func__);
			return -EINVAL;
		}
	}

	priv->c_tinfo.u_info.tune.demod_info.u_channel.dvbc.status =
						STM_FE_DEMOD_STATUS_UNKNOWN;
	priv->c_tinfo.u_info.tune.demod_info.u_channel.dvbt.status =
						STM_FE_DEMOD_STATUS_UNKNOWN;

	return ret;
}


/*
 * Name: stv0367_tracking()
 *
 * Description: Checking the lock status of demodulator
 *
 */
static int stv0367_tracking(struct stm_fe_demod_s *priv)
{
	FE_STV0367_Error_t err = FE_LLA_NO_ERROR;
	void *params;
	FE_STV0367_SignalInfo_t si;
	FE_TER_SignalInfo_t ter_si;
	stm_fe_demod_channel_info_t *info;
	stm_fe_demod_dvbc_channel_info_t *icab = NULL;
	stm_fe_demod_dvbt_channel_info_t *iterr = NULL;

	memset(&si, '\0', sizeof(FE_STV0367_SignalInfo_t));

	info = &priv->c_tinfo.u_info.tune.demod_info;

	if (priv->tp.std & STM_FE_DEMOD_TX_STD_DVBC) {
		params = priv->demod_params;
		priv->demod_h->pRegMapImage = priv->demod_h->qamregmap;
		err = FE_367qam_GetSignalInfo(params, &si);
		icab = &info->u_channel.dvbc;
		if (!err) {
			info->std = STM_FE_DEMOD_TX_STD_DVBC;
			icab->freq = si.Frequency;
			icab->sr = si.SymbolRate;
			icab->inv = si.SpectInversion;
			icab->mod = get_mod(si.Modulation);
			icab->snr = si.SNRx10dB_u32;
			icab->ber = si.BERxE7_u32;
			icab->signal_strength = si.RFLevelx10dBm_s32;
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
	}

	if (priv->tp.std & STM_FE_DEMOD_TX_STD_DVBT) {
		params = priv->add_demod_params;
		priv->demod_h->pRegMapImage = priv->demod_h->ofdmregmap;
		err = FE_367ofdm_GetSignalInfo(params, &ter_si);
		iterr = &info->u_channel.dvbt;
		if (!err) {
			info->std = STM_FE_DEMOD_TX_STD_DVBT;
			iterr->freq = ter_si.Frequency_kHz;
/*			iterr->fft_mode = get_fftmode(ter_si.FFTSize);
			iterr->guard = get_guard(ter_si.Guard); */
			iterr->hierarchy = get_hierarchy(ter_si.Hierarchy);
/*			iterr->alpha = get_alpha(ter_si.Hierarchy_Alpha);
			iterr->inv = (ter_si.SpectInversion == 1) ?
					STM_FE_DEMOD_SPECTRAL_INVERSION_OFF :
					STM_FE_DEMOD_SPECTRAL_INVERSION_ON;
			iterr->mod = get_ter_mod(ter_si.Modulation); */
			iterr->snr = ter_si.SNR_x10dB_u32;
			iterr->ber = ter_si.BER_xE7_u32;
			iterr->signal_strength = ter_si.RFLevel_x10dBm_s32;
			iterr->status = STM_FE_DEMOD_SYNC_OK;
/*			iterr->fec = get_fec(ter_si.FECRate); */
		} else {
			iterr->status = STM_FE_DEMOD_NO_SIGNAL;
			info->std = 0;
			iterr->freq = 0;
			iterr->inv = 0;
			iterr->mod = STM_FE_DEMOD_MODULATION_NONE;
			iterr->snr = 0;
			iterr->signal_strength = -100;
		}
		dpr_info("%s: Freq = %d KHz\nSNR = %d dB\nBER = %d\n"
				  "Signal strength = %d dBm\n", __func__,
				  iterr->freq, iterr->snr, iterr->ber,
				  iterr->signal_strength);
	}

	return 0;
}

static int stv0367_status(struct stm_fe_demod_s *priv, bool *locked)
{
	void *params;
	stm_fe_demod_channel_info_t *info;
	stm_fe_demod_status_t *status;

	info = &priv->c_tinfo.u_info.tune.demod_info;
	if (priv->tp.std == STM_FE_DEMOD_TX_STD_DVBC) {
		params = priv->demod_params;
		*locked = FE_367qam_Status(params);
		status = &info->u_channel.dvbc.status;
	} else if (priv->tp.std == STM_FE_DEMOD_TX_STD_DVBT) {
		params = priv->add_demod_params;
		*locked = FE_367ofdm_Status(params);
		status = &info->u_channel.dvbt.status;
	} else {
		pr_err("%s: incorrect std\n", __func__);
		return -EINVAL;
	}

	if (*locked)
		*status = STM_FE_DEMOD_SYNC_OK;
	else
		*status = STM_FE_DEMOD_NO_SIGNAL;

	return 0;
}

static int stv0367_unlock(struct stm_fe_demod_s *priv)
{
	void *params;
	int ret = 0;
	stm_fe_demod_scan_tx_std_t scan_stds;

	scan_stds = priv->scanparams.scan_stds;
	if ((priv->tp.std == STM_FE_DEMOD_TX_STD_DVBT) ||
		(scan_stds == STM_FE_DEMOD_SCAN_TX_STD_DVB_TER)) {
		FE_367qam_InternalParams_t *internal;
		params = priv->add_demod_params;
		internal = (FE_367qam_InternalParams_t *) params;
		priv->demod_h->pRegMapImage = priv->demod_h->ofdmregmap;
		priv->c_tinfo.u_info.tune.demod_info.u_channel.dvbt.
		    status = STM_FE_DEMOD_STATUS_UNKNOWN;
	} else if ((priv->tp.std == STM_FE_DEMOD_TX_STD_DVBC) ||
		(scan_stds == STM_FE_DEMOD_SCAN_TX_STD_DVB_CAB)) {
		params = priv->demod_params;
		priv->demod_h->pRegMapImage = priv->demod_h->qamregmap;
		priv->c_tinfo.u_info.tune.demod_info.u_channel.dvbc.
		    status = STM_FE_DEMOD_STATUS_UNKNOWN;
	} else {
		pr_err("%s: incorrect std\n", __func__);
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
	default:
		lla_mod = -1;
		break;
	}
	return lla_mod;
}

static FE_TER_Guard_t guardmap(stm_fe_demod_guard_t guard)
{
	FE_TER_Guard_t lla_guard;
	switch (guard) {
	case STM_FE_DEMOD_GUARD_1_4:
		lla_guard = FE_TER_GUARD_1_4;
		break;
	case STM_FE_DEMOD_GUARD_1_8:
		lla_guard = FE_TER_GUARD_1_8;
		break;
	case STM_FE_DEMOD_GUARD_1_16:
		lla_guard = FE_TER_GUARD_1_16;
		break;
	case STM_FE_DEMOD_GUARD_1_32:
		lla_guard = FE_TER_GUARD_1_32;
		break;
	case STM_FE_DEMOD_GUARD_1_128:
		lla_guard = FE_TER_GUARD_1_128;
		break;
	case STM_FE_DEMOD_GUARD_19_128:
		lla_guard = FE_TER_GUARD_19_128;
		break;
	case STM_FE_DEMOD_GUARD_19_256:
		lla_guard = FE_TER_GUARD_19_256;
		break;
	default:
		lla_guard = -1;
		break;
	}
	return lla_guard;
}

static FE_TER_FFT_t fftmodemap(stm_fe_demod_fft_mode_t fft_mode)
{
	FE_TER_FFT_t lla_mode;
	switch (fft_mode) {
	case STM_FE_DEMOD_FFT_MODE_1K:
		lla_mode = FE_TER_FFT_1K;
		break;
	case STM_FE_DEMOD_FFT_MODE_2K:
		lla_mode = FE_TER_FFT_2K;
		break;
	case STM_FE_DEMOD_FFT_MODE_4K:
		lla_mode = FE_TER_FFT_4K;
		break;
	case STM_FE_DEMOD_FFT_MODE_8K:
		lla_mode = FE_TER_FFT_8K;
		break;
	case STM_FE_DEMOD_FFT_MODE_16K:
		lla_mode = FE_TER_FFT_16K;
		break;
	case STM_FE_DEMOD_FFT_MODE_32K:
		lla_mode = FE_TER_FFT_32K;
		break;
	default:
		lla_mode = -1;
		break;
	}
	return lla_mode;
}

static FE_TER_Hierarchy_t hierarchymap(stm_fe_demod_hierarchy_t hier)
{
	FE_TER_Hierarchy_t lla_hier;
	switch (hier) {
	case STM_FE_DEMOD_HIERARCHY_NONE:
		lla_hier = FE_TER_HIER_NONE;
		break;
	case STM_FE_DEMOD_HIERARCHY_LOW:
		lla_hier = FE_TER_HIER_LOW_PRIO;
		break;
	case STM_FE_DEMOD_HIERARCHY_HIGH:
		lla_hier = FE_TER_HIER_HIGH_PRIO;
		break;
	default:
		lla_hier = -1;
		break;
	}
	return lla_hier;
}

static FE_TER_ChannelBW_t channelbwmap(stm_fe_demod_bw_t bw)
{
	FE_TER_ChannelBW_t lla_bw;
	switch (bw) {
	case STM_FE_DEMOD_BW_6_0:
		lla_bw = FE_TER_CHAN_BW_6M;
		break;
	case STM_FE_DEMOD_BW_7_0:
		lla_bw = FE_TER_CHAN_BW_7M;
		break;
	case STM_FE_DEMOD_BW_8_0:
		lla_bw = FE_TER_CHAN_BW_8M;
		break;
	default:
		lla_bw = -1;
		break;
	}
	return lla_bw;
}

static uint32_t get_bw(FE_TER_ChannelBW_t bw)
{
	uint32_t channel_bw;
	switch (bw) {
	case FE_TER_CHAN_BW_6M:
		channel_bw = STM_FE_DEMOD_BW_6_0;
		break;
	case FE_TER_CHAN_BW_7M:
		channel_bw = STM_FE_DEMOD_BW_7_0;
		break;
	case FE_TER_CHAN_BW_8M:
		channel_bw = STM_FE_DEMOD_BW_8_0;
		break;
	default:
		channel_bw = -1;
		break;
	}
	return channel_bw;

}

static stm_fe_demod_guard_t get_guard(FE_TER_Guard_t guard)
{
	stm_fe_demod_guard_t guard_t;
	switch (guard) {
	case FE_TER_GUARD_1_4:
		guard_t = STM_FE_DEMOD_GUARD_1_4;
		break;
	case FE_TER_GUARD_1_8:
		guard_t = STM_FE_DEMOD_GUARD_1_8;
		break;
	case FE_TER_GUARD_1_16:
		guard_t = STM_FE_DEMOD_GUARD_1_16;
		break;
	case FE_TER_GUARD_1_32:
		guard_t = STM_FE_DEMOD_GUARD_1_32;
		break;
	case FE_TER_GUARD_1_128:
		guard_t = STM_FE_DEMOD_GUARD_1_128;
		break;
	case FE_TER_GUARD_19_128:
		guard_t = STM_FE_DEMOD_GUARD_19_128;
		break;
	case FE_TER_GUARD_19_256:
		guard_t = STM_FE_DEMOD_GUARD_19_256;
		break;
	default:
		guard_t = -1;
		break;
	}
	return guard_t;

}

static stm_fe_demod_fft_mode_t get_fftmode(FE_TER_FFT_t fft_mode)
{
	stm_fe_demod_fft_mode_t mode;
	switch (fft_mode) {
	case FE_TER_FFT_1K:
		mode = STM_FE_DEMOD_FFT_MODE_1K;
		break;
	case FE_TER_FFT_2K:
		mode = STM_FE_DEMOD_FFT_MODE_2K;
		break;
	case FE_TER_FFT_4K:
		mode = STM_FE_DEMOD_FFT_MODE_4K;
		break;
	case FE_TER_FFT_8K:
		mode = STM_FE_DEMOD_FFT_MODE_8K;
		break;
	case FE_TER_FFT_16K:
		mode = STM_FE_DEMOD_FFT_MODE_16K;
		break;
	case FE_TER_FFT_32K:
		mode = STM_FE_DEMOD_FFT_MODE_32K;
		break;
	default:
		mode = -1;
		break;
	}
	return mode;
}

static stm_fe_demod_hierarchy_t get_hierarchy(FE_TER_Hierarchy_t hier)
{
	stm_fe_demod_hierarchy_t hierarchy;
	switch (hier) {
	case FE_TER_HIER_NONE:
		hierarchy = STM_FE_DEMOD_HIERARCHY_NONE;
		break;
	case FE_TER_HIER_LOW_PRIO:
		hierarchy = STM_FE_DEMOD_HIERARCHY_LOW;
		break;
	case FE_TER_HIER_HIGH_PRIO:
		hierarchy = STM_FE_DEMOD_HIERARCHY_HIGH;
		break;
	default:
		hierarchy = -1;
		break;
	}
	return hierarchy;
}

static stm_fe_demod_fec_rate_t get_fec(FE_TER_FECRate_t fec)
{
	stm_fe_demod_fec_rate_t fec_rate;
	switch (fec) {
	case FE_TER_Rate_1_2:
		fec_rate = STM_FE_DEMOD_FEC_RATE_1_2;
		break;
	case FE_TER_Rate_2_3:
		fec_rate = STM_FE_DEMOD_FEC_RATE_2_3;
		break;
	case FE_TER_Rate_3_4:
		fec_rate = STM_FE_DEMOD_FEC_RATE_3_4;
		break;
	case FE_TER_Rate_4_5:
		fec_rate = STM_FE_DEMOD_FEC_RATE_4_5;
		break;
	case FE_TER_Rate_5_6:
		fec_rate = STM_FE_DEMOD_FEC_RATE_5_6;
		break;
	case FE_TER_Rate_7_8:
		fec_rate = STM_FE_DEMOD_FEC_RATE_7_8;
		break;
	default:
		fec_rate = -1;
		break;
	}
	return fec_rate;
}

static stm_fe_demod_hierarchy_alpha_t get_alpha(FE_TER_Hierarchy_Alpha_t alpha)
{
	stm_fe_demod_hierarchy_alpha_t hierarchy_alpha;
	switch (alpha) {
	case FE_TER_HIER_ALPHA_NONE:
		hierarchy_alpha = STM_FE_DEMOD_HIERARCHY_ALPHA_NONE;
		break;
	case FE_TER_HIER_ALPHA_1:
		hierarchy_alpha = STM_FE_DEMOD_HIERARCHY_ALPHA_1;
		break;
	case FE_TER_HIER_ALPHA_2:
		hierarchy_alpha = STM_FE_DEMOD_HIERARCHY_ALPHA_2;
		break;
	case FE_TER_HIER_ALPHA_4:
		hierarchy_alpha = STM_FE_DEMOD_HIERARCHY_ALPHA_4;
		break;
	default:
		hierarchy_alpha = -1;
		break;
	}
	return hierarchy_alpha;
}

static stm_fe_demod_modulation_t get_ter_mod(FE_TER_Modulation_t Mod)
{
	stm_fe_demod_modulation_t fe_mod = STM_FE_DEMOD_MODULATION_NONE;
	switch (Mod) {
	case FE_TER_MOD_QPSK:
		fe_mod = STM_FE_DEMOD_MODULATION_QPSK;
		break;
	case FE_TER_MOD_16QAM:
		fe_mod = STM_FE_DEMOD_MODULATION_16QAM;
		break;
	case FE_TER_MOD_64QAM:
		fe_mod = STM_FE_DEMOD_MODULATION_64QAM;
		break;
	case FE_TER_MOD_256QAM:
		fe_mod = STM_FE_DEMOD_MODULATION_256QAM;
		break;
	default:
		fe_mod = STM_FE_DEMOD_MODULATION_NONE;
		break;
	}
	return fe_mod;
}

static int32_t __init stv0367_lla_init(void)
{
	pr_info("Loading stv0367 driver module...\n");

	return 0;
}

module_init(stv0367_lla_init);

static void __exit stv0367_lla_term(void)
{
	pr_info("Removing stv0367 driver module ...\n");

}

module_exit(stv0367_lla_term);

MODULE_DESCRIPTION("Low level driver for STV0367 terrestrial and cable demod");
MODULE_AUTHOR("STMicroelectronics Limited");
MODULE_VERSION(STV0367_CURRENT_LLA_REVISION);
MODULE_LICENSE("GPL");
