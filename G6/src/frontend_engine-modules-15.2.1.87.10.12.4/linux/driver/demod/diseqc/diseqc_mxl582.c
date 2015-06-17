/************************************************************************
Copyright(C) 2012 - 2013 STMicroelectronics. All Rights Reserved.

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

Source file name :diseqc_mxl582.c
Author :		  HS

Low level function installation for diseqc

Date		Modification			Name
----		------------			--------
13 - May - 13	Created				HS
************************************************************************/
#include <linux/kernel.h>
#include <linux/module.h>
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
#include <stm_fe_lnb.h>
#include <stm_fe_diseqc.h>
#include <stm_fe_install.h>
#include "diseqc_mxl582.h"
#include <fesat_commlla_str.h>
#include "mxl582_drv.h"
#include "mxlware_hydra_diseqcfskapi.h"

static int diseqc_mxl582_init(struct stm_fe_diseqc_s *priv);
static int diseqc_mxl582_transfer(struct stm_fe_diseqc_s *priv,
	stm_fe_diseqc_mode_t mode, stm_fe_diseqc_msg_t *msg_d, uint32_t num);
static int diseqc_mxl582_term(struct stm_fe_diseqc_s *priv);

static int diseqc_mxl582_get_id(uint32_t d_id, uint16_t *diseqc_id);


#define MXL582_DEV_ID 0


/*
 * Name: stm_fe_diseqc_mxl582_attach()
 *
 * Description: Installed the diseqc_mxl582 wrapper function pointers for LLA
 *
 */
int stm_fe_diseqc_mxl582_attach(struct stm_fe_diseqc_s *priv)
{
	if (!priv->ops)
		return -EFAULT;

	priv->ops->init = diseqc_mxl582_init;
	priv->ops->transfer = diseqc_mxl582_transfer;
	priv->ops->term = diseqc_mxl582_term;
	priv->ops->detach = stm_fe_diseqc_mxl582_detach;

	return 0;
}
EXPORT_SYMBOL(stm_fe_diseqc_mxl582_attach);

/*
 * Name: stm_fe_diseqc_mxl582_detach()
 *
 * Description: uninstall the diseqc_mxl582 wrapper function
 *
 */
int stm_fe_diseqc_mxl582_detach(struct stm_fe_diseqc_s *priv)
{
	if (!priv->ops)
		return -EFAULT;

	priv->ops->detach = NULL;
	priv->ops->term = NULL;
	priv->ops->transfer = NULL;
	priv->ops->init = NULL;

	return 0;
}
EXPORT_SYMBOL(stm_fe_diseqc_mxl582_detach);


static int diseqc_mxl582_get_id(uint32_t d_id, uint16_t *diseqc_id)
{
	switch (d_id) {
	case MXL_HYDRA_DEMOD_ID_0:
	case MXL_HYDRA_DEMOD_ID_1:
	case MXL_HYDRA_DEMOD_ID_2:
	case MXL_HYDRA_DEMOD_ID_3:
		*diseqc_id = MXL_HYDRA_DISEQC_ID_0;
		break;
	case MXL_HYDRA_DEMOD_ID_4:
	case MXL_HYDRA_DEMOD_ID_5:
	case MXL_HYDRA_DEMOD_ID_6:
	case MXL_HYDRA_DEMOD_ID_7:
		*diseqc_id = MXL_HYDRA_DISEQC_ID_2;
		break;
	default:
		*diseqc_id = MXL_HYDRA_DISEQC_ID_0;
	}
	return *diseqc_id;
}
/*
 * Name: diseqc_mxl582_init()
 *
 * Description: initialization of diseqc
 *
 */
static int diseqc_mxl582_init(struct stm_fe_diseqc_s *priv)
{
	struct stm_fe_demod_s *demod_priv;
	FE_MXL582_InternalParams_t *params = NULL;

	demod_priv = (struct stm_fe_demod_s *)priv->demod_h;
	params = (FE_MXL582_InternalParams_t *)demod_priv->demod_params;

	if (priv->config->cust_flags & DISEQC_CUSTOM_ENVELOPE_MODE)
		params->DiSEqC_ENV_Mode_Selection = true;

	return 0;
}

/*
 * Name: diseqc_mxl582_term()
 *
 * Description: termination of diseqc
 *
 */
static int diseqc_mxl582_term(struct stm_fe_diseqc_s *priv)
{
	struct stm_fe_demod_s *demod_priv;
	FE_MXL582_InternalParams_t *params = NULL;

	demod_priv = (struct stm_fe_demod_s *)priv->demod_h;
	params = (FE_MXL582_InternalParams_t *)demod_priv->demod_params;
	if (params)
		params->DiSEqC_ENV_Mode_Selection = false;
	else
		pr_err("%s: invalid params\n", __func__);

	return 0;
}

/*
 * Name: diseqc_mxl582_get_mode()
 *
 * Description: retreive the diseqc mode
 *
 */
FE_Sat_DiseqC_TxMode diseqc_mxl582_get_mode(FE_MXL582_Handle_t hdl,
					 stm_fe_diseqc_mode_t mode,
					 uint8_t *tx_data, uint32_t *nbdata,
					 MXL_HYDRA_DISEQC_OPMODE_E mxl_mod,
					 MXL_HYDRA_DISEQC_TONE_CTRL_E t_ctrl)
{
	FE_MXL582_InternalParams_t *params = NULL;
	FE_Sat_DiseqC_TxMode mode_d = FE_SAT_22KHZ_Continues;

	params = (FE_MXL582_InternalParams_t *) hdl;

	switch (mode) {
		/*Equivalent to continous mode */
	case STM_FE_DISEQC_TONE_BURST_CONTINUOUS:
		mode_d = FE_SAT_22KHZ_Continues;
		tx_data[0] = 2;
		*nbdata = 1;
		break;
		/*0 -- 12.5 ms of continuous tone
		 *Equivalent to MODE SA */
	case STM_FE_DISEQC_TONE_BURST_SEND_0_UNMODULATED:
		if (params->DiSEqC_ENV_Mode_Selection)
			mode_d = FE_SAT_DISEQC_3_3_ENVELOP;
		else		/*Writing 0x011 in DISTX_MODE-- */
			mode_d = FE_SAT_DISEQC_3_3_PWM;
		tx_data[0] = 0; /*Writing one byte of value 0x00 */
		*nbdata = 1;
		t_ctrl = MXL_HYDRA_DISEQC_TONE_SA;
		break;
		/*binary 0 - a 2/3duty cycle tone*/
	case STM_FE_DISEQC_TONE_BURST_SEND_0_MODULATED:
		tx_data[0] = 0;
		if (params->DiSEqC_ENV_Mode_Selection)
			mode_d = FE_SAT_DISEQC_2_3_ENVELOP;
		else
			mode_d = FE_SAT_DISEQC_2_3_PWM;
		*nbdata = 1;
		break;
		/*binary 1 - a 1/3 duty cycle tone modulated
		 *Equivalent to MODE SB */
	case STM_FE_DISEQC_TONE_BURST_SEND_1_MODULATED:
		if (params->DiSEqC_ENV_Mode_Selection)
			mode_d = FE_SAT_DISEQC_2_3_ENVELOP;
		else
			mode_d = FE_SAT_DISEQC_2_3_PWM;
		tx_data[0] = 0xff; /*One byte of 0xff */
		*nbdata = 1;
		t_ctrl = MXL_HYDRA_DISEQC_TONE_SB;
		break;
	case STM_FE_DISEQC_COMMAND:	/*DiSEqC(1.2/2)command */
		if (params->DiSEqC_ENV_Mode_Selection)
			mode_d = FE_SAT_DISEQC_2_3_ENVELOP;
		else
			mode_d = FE_SAT_DISEQC_2_3_PWM;

		t_ctrl = MXL_HYDRA_DISEQC_TONE_NONE;
		break;

	default:
		break;

	}

	if (params->DiSEqC_ENV_Mode_Selection)
		mxl_mod = MXL_HYDRA_DISEQC_ENVELOPE_MODE;
	else
		mxl_mod = MXL_HYDRA_DISEQC_TONE_MODE;

	return mode_d;
}

/*
 * Name: diseqc_mxl582_transfer()
 *
 * Description: Implementation of diseqc send receive command
 *
 */
int diseqc_mxl582_transfer(struct stm_fe_diseqc_s *priv,
			stm_fe_diseqc_mode_t mode,
			stm_fe_diseqc_msg_t *msg_d,
			uint32_t num_msg)
{
	FE_MXL582_Handle_t hdl;
	struct stm_fe_demod_s *demod_priv;
	struct stm_fe_lnb_s *lnb_priv;
	FE_Sat_DiseqC_TxMode mode_d = FE_SAT_22KHZ_Continues;
	FE_MXL582_InternalParams_t *p = NULL;

	MXL_STATUS_E result = MXL_FAILURE;
	uint16_t  diseqc_id;
	MXL_HYDRA_DISEQC_TX_MSG_T disq_tx_msg;
	MXL_HYDRA_DISEQC_RX_MSG_T disq_rx_msg;
	MXL_HYDRA_DISEQC_OPMODE_E opMode = 1;
	MXL_HYDRA_DISEQC_TONE_CTRL_E t_ctrl = MXL_HYDRA_DISEQC_TONE_NONE;
	struct lnb_config_t lnbconfig;
	int ret = 0;
	int i = 0, t = 0, w = 0;
	unsigned char tx_data[8] = {0};
	uint32_t nbdata = 1;
	bool enable_setconfig = true;
	uint32_t stat = 0;

	demod_priv = (struct stm_fe_demod_s *)priv->demod_h;
	lnb_priv = (struct stm_fe_lnb_s *)demod_priv->lnb_h;
	hdl = demod_priv->demod_params;
	p = (FE_MXL582_InternalParams_t *) hdl;

	memcpy(&lnbconfig, &lnb_priv->lnbparams, sizeof(struct lnb_config_t));

	if (!lnb_priv->ops->setconfig) {
		pr_err("%s: FP lnb_setconfig is NULL\n", __func__);
		return -EFAULT;
	}
	if (lnb_priv->lnbparams.txmode == LNB_TTX_NONE)
		enable_setconfig = false;

	if (enable_setconfig && num_msg)
		lnbconfig.txmode = LNB_TX;

	/*mode_d decides if 2/3 PWM Modulated OR  Envelope*/
	if (mode & STM_FE_DISEQC_COMMAND) /*DiSEqC Command*/
		mode_d = diseqc_mxl582_get_mode(hdl, mode, msg_d[i].msg,
					&msg_d[i].msg_len, opMode, t_ctrl);
	else	/*for SAT - A/SAT - B*/
		mode_d = diseqc_mxl582_get_mode(hdl, mode, tx_data,
						 &nbdata, opMode, t_ctrl);

	diseqc_mxl582_get_id(demod_priv->demod_id, &diseqc_id);

	p->status = MxLWare_HYDRA_API_CfgDiseqcOpMode(MXL582_DEV_ID, diseqc_id,
					opMode, priv->config->ver,
					MXL_HYDRA_DISEQC_CARRIER_FREQ_22KHZ);

	if (p->status != MXL_SUCCESS) {
		pr_err("%s:MxLWare_HYDRA_API_CfgDiseqcOpMode %d\n",
				__func__, result);
		return -EFAULT;
	}

	if (!(mode & STM_FE_DISEQC_COMMAND)) {/*for SAT-A/SAT-B/cont T burst*/
		if (mode == STM_FE_DISEQC_TONE_BURST_CONTINUOUS) {
			p->status =
			 MxLWare_HYDRA_API_CfgDiseqcContinuousToneCtrl(
								MXL582_DEV_ID,
								diseqc_id,
								TRUE);
			if (p->status != MXL_SUCCESS) {
				pr_err(
				"%s:Error CfgDiseqcContinuousToneCtrl =%d\n",
				__func__, p->status);
				return -EFAULT;
			}
		} else if (mode == STM_FE_DISEQC_TONE_BURST_OFF) {
			p->status =
			MxLWare_HYDRA_API_CfgDiseqcContinuousToneCtrl(
								MXL582_DEV_ID,
								diseqc_id,
								FALSE);
			if (p->status != MXL_SUCCESS) {
				pr_err(
				"%s:Error CfgDiseqcContinuousToneCtrl =%d\n",
						__func__, p->status);
				return -EFAULT;
			}
		} else {
			disq_tx_msg.diseqcId = diseqc_id;
			disq_tx_msg.nbyte = nbdata;
			disq_tx_msg.bufMsg[0] = tx_data[0];
			disq_tx_msg.toneBurst = t_ctrl;
			p->status =
			MxLWare_HYDRA_API_CfgDiseqcWrite(MXL582_DEV_ID,
							&disq_tx_msg);
			if (p->status != MXL_SUCCESS) {
				pr_err(
				"%s:Error MXL_API_CfgDiseqcWrite = %d\n",
				__func__, p->status);
				return -EFAULT;
			}
		}
		return 0;
	}

	while (i < num_msg) {
		if (msg_d[i].op == STM_FE_DISEQC_TRANSFER_SEND) {

			disq_tx_msg.diseqcId = diseqc_id,
			disq_tx_msg.nbyte = msg_d[i].msg_len;
			for (t = 0; t <= disq_tx_msg.nbyte; t++)
				disq_tx_msg.bufMsg[t] = msg_d[i].msg[t];

			lnbconfig.txmode = LNB_TX;
			lnbconfig.tonestate = STM_FE_LNB_TONE_22KHZ;
			ret = lnb_priv->ops->setconfig(lnb_priv, &lnbconfig);

			disq_tx_msg.toneBurst = MXL_HYDRA_DISEQC_TONE_NONE;
			p->status =
				MxLWare_HYDRA_API_CfgDiseqcWrite(MXL582_DEV_ID,
								&disq_tx_msg);
			if (p->status != MXL_SUCCESS) {
				pr_err(
				"%s:Error HYDRA_API_CfgDiseqcWrite = %d\n",
					__func__, p->status);
				return -EFAULT;
			}
			w = 0;
			stat = 0;
			while ((stat != MXL_HYDRA_DISEQC_STATUS_TX_DONE)
				&& (w < 10)) {
				/*wait until the end of diseqc send operation*/
				p->status =
					MxLWare_HYDRA_API_ReqDiseqcStatus(
								MXL582_DEV_ID,
								diseqc_id,
								&stat);
					WAIT_N_MS(10);
					w++;
				}
			lnbconfig.txmode = LNB_TX;
			lnbconfig.tonestate = STM_FE_LNB_TONE_OFF;
			ret = lnb_priv->ops->setconfig(lnb_priv, &lnbconfig);
		} else { /* (msg_d[i].op == STM_FE_DISEQC_TRANSFER_RECEIVE)*/
			/* wait for reception ms */
			WAIT_N_MS(msg_d[i].timeout);
			w = 0;
			stat = 0;
			while ((stat != MXL_HYDRA_DISEQC_STATUS_RX_DATA_AVAIL)
				&& (w < 10)) {
				/*wait until the end of diseqc send operation*/
				p->status =
					MxLWare_HYDRA_API_ReqDiseqcStatus(
								MXL582_DEV_ID,
								diseqc_id,
								&stat);

				WAIT_N_MS(10);
				w++;
			}
			p->status = MxLWare_HYDRA_API_ReqDiseqcRead(
								MXL582_DEV_ID,
								diseqc_id,
								&disq_rx_msg);
			if (p->status != MXL_SUCCESS) {
				pr_err(
				"%s:Err in MXL_API_ReqDiseqcRead = %d\n"
					, __func__, p->status);
				return -EFAULT;
			} else {
				msg_d[i].msg_len = disq_rx_msg.nbyte;
				for (t = 0; t <= msg_d[i].msg_len; t++)
					msg_d[i].msg[t] = disq_rx_msg.bufMsg[t];
			}
		}
		WAIT_N_MS(msg_d[i].timegap); /*delay between two transfer */
		i++;
	}
	return 0;
}

static int32_t __init stm_fe_diseqc_mxl582_init(void)
{
	pr_info("Loading diseqc_mxl582 driver module...\n");

	return 0;
}
module_init(stm_fe_diseqc_mxl582_init);

static void __exit stm_fe_diseqc_mxl582_term(void)
{
	pr_info("Removing diseqc_mxl582 driver module...\n");
}
module_exit(stm_fe_diseqc_mxl582_term);

MODULE_DESCRIPTION("Low level driver for mxl582 diseqc controller");
MODULE_AUTHOR("STMicroelectronics Limited");
MODULE_LICENSE("GPL");
