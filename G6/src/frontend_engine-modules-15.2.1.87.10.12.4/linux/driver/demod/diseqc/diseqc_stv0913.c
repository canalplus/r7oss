/************************************************************************
Copyright (C) 2013 STMicroelectronics. All Rights Reserved.

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

Source file name :diseqc_stv0913.c
Author :          HS

Low level function installtion for diseqc

Date        Modification                                    Name
----        ------------                                    --------
5-June-13   Created                                         HS

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
#include "diseqc_stv0913.h"
#include <fesat_commlla_str.h>
#include <stv0913_drv.h>
#include <d0913.h>

static int diseqc_stv0913_init(struct stm_fe_diseqc_s *diseqc_priv);
static int diseqc_stv0913_transfer(struct stm_fe_diseqc_s *diseqc_priv,
	stm_fe_diseqc_mode_t mode, stm_fe_diseqc_msg_t *msg_d, uint32_t num);
static int diseqc_stv0913_term(struct stm_fe_diseqc_s *diseqc_priv);

/*
 * Name: stm_fe_diseqc_stv0913_attach()
 *
 * Description: Installed the diseqc_stv0913 wrapper function pointers for LLA
 *
 */
int stm_fe_diseqc_stv0913_attach(struct stm_fe_diseqc_s *priv)
{
	if (!priv->ops)
		return -EFAULT;

	priv->ops->init = diseqc_stv0913_init;
	priv->ops->transfer = diseqc_stv0913_transfer;
	priv->ops->term = diseqc_stv0913_term;
	priv->ops->detach = stm_fe_diseqc_stv0913_detach;

	return 0;
}
EXPORT_SYMBOL(stm_fe_diseqc_stv0913_attach);

/*
 * Name: stm_fe_diseqc_stv0913_detach()
 *
 * Description: uninstall the diseqc_stv0913 wrapper function
 *
 */
int stm_fe_diseqc_stv0913_detach(struct stm_fe_diseqc_s *priv)
{
	if (!priv->ops)
		return -EFAULT;

	priv->ops->detach = NULL;
	priv->ops->term = NULL;
	priv->ops->transfer = NULL;
	priv->ops->init = NULL;

	return 0;
}
EXPORT_SYMBOL(stm_fe_diseqc_stv0913_detach);


/*
 * Name: diseqc_stv0913_init()
 *
 * Description: initialization of diseqc
 *
 */
static int diseqc_stv0913_init(struct stm_fe_diseqc_s *priv)
{
	int ret = 0;
	struct stm_fe_demod_s *demod_priv;
	FE_STV0913_Handle_t hdl;
	FE_STV0913_InternalParams_t *params = NULL;

	demod_priv = (struct stm_fe_demod_s *)priv->demod_h;
	hdl = demod_priv->demod_params;
	params = (FE_STV0913_InternalParams_t *) hdl;

	if (priv->config->cust_flags & DISEQC_CUSTOM_ENVELOPE_MODE)
		params->DiSEqC_ENV_Mode_Selection = true;

	return ret;
}

/*
 * Name: diseqc_stv0913_term()
 *
 * Description: termination of diseqc
 *
 */
static int diseqc_stv0913_term(struct stm_fe_diseqc_s *priv)
{
	int ret = 0;
	struct stm_fe_demod_s *demod_priv;
	FE_STV0913_InternalParams_t *params = NULL;

	demod_priv = (struct stm_fe_demod_s *)priv->demod_h;
	params = (FE_STV0913_InternalParams_t *) demod_priv->demod_params;
	if (params)
		params->DiSEqC_ENV_Mode_Selection = false;
	else
		pr_err("%s: invalid params\n", __func__);

	return ret;
}

/*
 * Name: diseqc_stv0913_get_mode()
 *
 * Description: retreive the diseqc mode
 *
 */
static FE_Sat_DiseqC_TxMode diseqc_stv0913_get_mode(FE_STV0913_Handle_t hdl,
					 stm_fe_diseqc_mode_t mode,
					 uint8_t *tx_data, uint32_t *nbdata)
{
	FE_STV0913_InternalParams_t *p = NULL;
	FE_Sat_DiseqC_TxMode mode_d = FE_SAT_22KHZ_Continues;

	p = (FE_STV0913_InternalParams_t *) hdl;

	switch (mode) {
		/*Equivalent to continous mode */
	case STM_FE_DISEQC_TONE_BURST_CONTINUOUS:
		mode_d = FE_SAT_22KHZ_Continues;
		tx_data[0] = 0; /*Writing one byte of value 0x00 */
		*nbdata = 1;
		break;
		/*send of 0 for 12.5 ms ;continuous tone
		 *Equivalent to MODE SA */
	case STM_FE_DISEQC_TONE_BURST_SEND_0_UNMODULATED:
		if (p->DiSEqC_ENV_Mode_Selection)
			mode_d = FE_SAT_DISEQC_3_3_ENVELOP;
		else		/*Writing 0x011 in DISTX_MODE-- */
			mode_d = FE_SAT_DISEQC_3_3_PWM;
		tx_data[0] = 0;	/*Writing one byte of value 0x00 */
		*nbdata = 1;
		break;

	case STM_FE_DISEQC_TONE_BURST_SEND_0_MODULATED:	/*0-2/3duty cycle tone*/
		tx_data[0] = 0;
		if (p->DiSEqC_ENV_Mode_Selection)
			mode_d = FE_SAT_DISEQC_2_3_ENVELOP;
		else
			mode_d = FE_SAT_DISEQC_2_3_PWM;	/*DISTX_MODE --0x010 */
		*nbdata = 1;
		break;
		/*1-1/3 duty cycle tone modulated
		 *Equivalent to MODE SB */
	case STM_FE_DISEQC_TONE_BURST_SEND_1_MODULATED:
		/* *Mode_p  = 0x2; */
		if (p->DiSEqC_ENV_Mode_Selection)
			mode_d = FE_SAT_DISEQC_2_3_ENVELOP;
		else
			mode_d = FE_SAT_DISEQC_2_3_PWM;	/*DISTX_MODE --0x010 */
		tx_data[0] = 0xff;	/*One byte of 0xff */
		*nbdata = 1;
		break;

	case STM_FE_DISEQC_COMMAND:	/*DiSEqC (1.2/2)command */
		if (p->DiSEqC_ENV_Mode_Selection)
			mode_d = FE_SAT_DISEQC_2_3_ENVELOP;
		else
			mode_d = FE_SAT_DISEQC_2_3_PWM;
		break;

	default:
		break;

	}
	return mode_d;
}

/*
 * Name: diseqc_stv0913_transfer()
 *
 * Description: Implementation of diseqc send receive command
 *
 */
static int diseqc_stv0913_transfer(struct stm_fe_diseqc_s *priv,
	stm_fe_diseqc_mode_t mode, stm_fe_diseqc_msg_t *msg_d, uint32_t num)
{
	FE_STV0913_Handle_t hdl;
	struct stm_fe_demod_s *demod_priv;
	struct stm_fe_lnb_s *lnb_priv;
	FE_Sat_DiseqC_TxMode mode_d = FE_SAT_22KHZ_Continues;
	FE_STV0913_InternalParams_t *p = NULL;
	FE_STV0913_Error_t error;
	struct lnb_config_t lnbconfig;
	int ret = 0;
	int i = 0;
	unsigned char tx_data[8];
	uint32_t nbdata = 1;
	bool enable_setconfig = true;

	demod_priv = (struct stm_fe_demod_s *)priv->demod_h;
	lnb_priv = (struct stm_fe_lnb_s *)demod_priv->lnb_h;
	hdl = demod_priv->demod_params;
	p = (FE_STV0913_InternalParams_t *) hdl;

	memcpy(&lnbconfig, &lnb_priv->lnbparams, sizeof(struct lnb_config_t));

	if (!lnb_priv->ops->setconfig) {
		pr_err("%s: FP is assigned to NULL value\n", __func__);
		return -EFAULT;
	}
	if (lnb_priv->lnbparams.txmode == LNB_TTX_NONE)
		enable_setconfig = false;

	if (enable_setconfig && num) {
		lnbconfig.txmode = LNB_TX;
		ret = lnb_priv->ops->setconfig(lnb_priv, &lnbconfig);
	}

	if (mode & STM_FE_DISEQC_COMMAND)
		mode_d = diseqc_stv0913_get_mode(hdl, mode, msg_d[i].msg,
				&msg_d[i].msg_len);
	else
		mode_d = diseqc_stv0913_get_mode(hdl, mode, tx_data, &nbdata);

	error = FE_STV0913_DiseqcInit(hdl, mode_d);
	if (error != FE_LLA_NO_ERROR) {
		pr_err("%s: Diseqc initialization failed\n", __func__);
		return -EFAULT;
	}

	/*other modes than STM_FE_DISEQC_COMMAND use only FE_xxx_DISEqCSend*/
	if (!(mode & STM_FE_DISEQC_COMMAND)) {
		error = FE_STV0913_DiseqcSend(hdl, tx_data, nbdata);
		if (error != FE_LLA_NO_ERROR) {
			pr_err("%s: Err in sending Diseqc command\n", __func__);
			return -EFAULT;
		}
		return 0;
	}

	while (i < num) {
		if (msg_d[i].op == STM_FE_DISEQC_TRANSFER_SEND) {
			lnbconfig.txmode = LNB_TX;
			lnbconfig.tonestate = STM_FE_LNB_TONE_22KHZ;
			ret = lnb_priv->ops->setconfig(lnb_priv, &lnbconfig);
			error = FE_STV0913_DiseqcSend(hdl, msg_d[i].msg,
					msg_d[i].msg_len);

			if (error != FE_LLA_NO_ERROR) {
				pr_err("%s: Diseqc cmd send error\n", __func__);
				return -EFAULT;
			}
			lnbconfig.txmode = LNB_TX;
			lnbconfig.tonestate = STM_FE_LNB_TONE_OFF;
			ret = lnb_priv->ops->setconfig(lnb_priv, &lnbconfig);
		} else { /* (msg_d[i].op == STM_FE_DISEQC_TRANSFER_RECEIVE) */
			if (enable_setconfig) {
				lnbconfig.txmode = LNB_RX;
				ret = lnb_priv->ops->setconfig(lnb_priv,
								    &lnbconfig);
			}
			/* wait for reception ms */
			WAIT_N_MS(msg_d[i].timeout);
			error = FE_STV0913_DiseqcReceive(hdl, msg_d[i].msg,
					&msg_d[i].msg_len);

			if (error != FE_LLA_NO_ERROR) {
				pr_err("%s: Diseqc cmd rcv err\n" , __func__);
				return -EFAULT;
			}
			if (enable_setconfig) {
				lnbconfig.txmode = LNB_TX;
				ret = lnb_priv->ops->setconfig(lnb_priv,
								   &lnbconfig);
			}
		}
		WAIT_N_MS(msg_d[i].timegap);	/*delay between two transfer */
		i++;
	}

	return 0;
}

static int32_t __init stm_fe_diseqc_stv0913_init(void)
{
	pr_info("Loading diseqc_stv0913 driver module...\n");

	return 0;
}

module_init(stm_fe_diseqc_stv0913_init);

static void __exit stm_fe_diseqc_stv0913_term(void)
{
	pr_info("Removing diseqc_stv0913 driver module...\n");
}

module_exit(stm_fe_diseqc_stv0913_term);

MODULE_DESCRIPTION("Low level driver for STV0913 diseqc controller");
MODULE_AUTHOR("STMicroelectronics Limited");
MODULE_LICENSE("GPL");
