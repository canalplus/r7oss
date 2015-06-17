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

Source file name :diseqc_stv0900.c
Author :          Ankur

Low level function installation for diseqc

Date        Modification                                    Name
----        ------------                                    --------
21-May-12   Created                                         Ankur

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
#include "diseqc_stv0900.h"
#include <fesat_commlla_str.h>
#include <stv0900_drv.h>
#include <d0900.h>

static int diseqc_stv0900_init(struct stm_fe_diseqc_s *priv);
static int diseqc_stv0900_transfer(struct stm_fe_diseqc_s *priv,
			    stm_fe_diseqc_mode_t mode,
			    stm_fe_diseqc_msg_t *msg_d, uint32_t num_msg);
static int diseqc_stv0900_term(struct stm_fe_diseqc_s *priv);

/*
 * Name: stm_fe_diseqc_stv0900_attach()
 *
 * Description: Installed the diseqc_stv0900 wrapper function pointers for LLA
 *
 */
int stm_fe_diseqc_stv0900_attach(struct stm_fe_diseqc_s *priv)
{
	if (!priv->ops)
		return -EFAULT;

	priv->ops->init = diseqc_stv0900_init;
	priv->ops->transfer = diseqc_stv0900_transfer;
	priv->ops->term = diseqc_stv0900_term;
	priv->ops->detach = stm_fe_diseqc_stv0900_detach;

	return 0;
}
EXPORT_SYMBOL(stm_fe_diseqc_stv0900_attach);

/*
 * Name: stm_fe_diseqc_stv0900_detach()
 *
 * Description: uninstall the diseqc_stv0900 wrapper function
 *
 */
int stm_fe_diseqc_stv0900_detach(struct stm_fe_diseqc_s *priv)
{
	if (!priv->ops)
		return -EFAULT;

	priv->ops->detach = NULL;
	priv->ops->term = NULL;
	priv->ops->transfer = NULL;
	priv->ops->init = NULL;

	return 0;
}
EXPORT_SYMBOL(stm_fe_diseqc_stv0900_detach);

int stv0900_diseqc_get_path(struct diseqc_config_s *config)
{
	if (IS_NAME_MATCH(config->diseqc_name, "DISEQC_STV0900P1"))
		return FE_SAT_DEMOD_1;
	else
		return FE_SAT_DEMOD_2;
}
#define THIS_PATH(p) (stv0900_diseqc_get_path(p->config))


/*
 * Name: diseqc_stv0900_init()
 *
 * Description: initialization of diseqc
 *
 */
static int diseqc_stv0900_init(struct stm_fe_diseqc_s *priv)
{
	struct stm_fe_demod_s *demod_priv;
	FE_STV0900_InternalParams_t *params = NULL;

	demod_priv = (struct stm_fe_demod_s *)priv->demod_h;
	params = (FE_STV0900_InternalParams_t *)demod_priv->demod_params;

	if (!params) {
		pr_err("%s: demod param is NULL\n", __func__);
		return -EFAULT;
	}

	if (priv->config->cust_flags & DISEQC_CUSTOM_ENVELOPE_MODE)
		params->DiSEqC_ENV_Mode_Selection = true;

	return 0;
}

/*
 * Name: diseqc_stv0900_term()
 *
 * Description: termination of diseqc
 *
 */
static int diseqc_stv0900_term(struct stm_fe_diseqc_s *priv)
{
	struct stm_fe_demod_s *demod_priv;
	FE_STV0900_InternalParams_t *params = NULL;

	demod_priv = (struct stm_fe_demod_s *)priv->demod_h;
	params = (FE_STV0900_InternalParams_t *)demod_priv->demod_params;

	if (!params) {
		pr_err("%s: demod param is NULL\n", __func__);
		return -EFAULT;
	}

	params->DiSEqC_ENV_Mode_Selection = false;

	return 0;
}

/*
 * Name: diseqc_stv0900_get_mode()
 *
 * Description: retreive the diseqc mode
 *
 */
static FE_Sat_DiseqC_TxMode diseqc_stv0900_get_mode(FE_STV0900_Handle_t hdl,
					 stm_fe_diseqc_mode_t mode,
					 uint8_t *tx_data, uint32_t *nbdata)
{
	FE_STV0900_InternalParams_t *params = NULL;
	FE_Sat_DiseqC_TxMode mode_d = FE_SAT_22KHZ_Continues;

	params = (FE_STV0900_InternalParams_t *)hdl;

	switch (mode) {
		/*Equivalent to continous mode */
	case STM_FE_DISEQC_TONE_BURST_CONTINUOUS:
		mode_d = FE_SAT_22KHZ_Continues;
		tx_data[0] = 2;
		*nbdata = 1;
		break;
		/*send of 0 for 12.5 ms ;continuous tone
		 *Equivalent to MODE SA */
	case STM_FE_DISEQC_TONE_BURST_SEND_0_UNMODULATED:
		if (params->DiSEqC_ENV_Mode_Selection)
			mode_d = FE_SAT_DISEQC_3_3_ENVELOP;
		else		/*Writing 0x011 in DISTX_MODE-- */
			mode_d = FE_SAT_DISEQC_3_3_PWM;
		tx_data[0] = 0;	/*Writing one byte of value 0x00 */
		*nbdata = 1;
		break;

	case STM_FE_DISEQC_TONE_BURST_SEND_0_MODULATED:	/*0-2/3duty cycle tone*/
		tx_data[0] = 0;
		if (params->DiSEqC_ENV_Mode_Selection)
			mode_d = FE_SAT_DISEQC_2_3_ENVELOP;
		else
			mode_d = FE_SAT_DISEQC_2_3_PWM;	/*DISTX_MODE --0x010 */
		*nbdata = 1;
		break;
		/*1-1/3 duty cycle tone modulated
		 *Equivalent to MODE SB */
	case STM_FE_DISEQC_TONE_BURST_SEND_1_MODULATED:
		/* *Mode_p  = 0x2; */
		if (params->DiSEqC_ENV_Mode_Selection)
			mode_d = FE_SAT_DISEQC_2_3_ENVELOP;
		else
			mode_d = FE_SAT_DISEQC_2_3_PWM;	/*DISTX_MODE --0x010 */
		tx_data[0] = 0xff;	/*One byte of 0xff */
		*nbdata = 1;
		break;

	case STM_FE_DISEQC_COMMAND:	/*DiSEqC (1.2/2)command */
		if (params->DiSEqC_ENV_Mode_Selection)
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
 * Name: diseqc_stv0900_transfer()
 *
 * Description: Implementation of diseqc send receive command
 *
 */
static int diseqc_stv0900_transfer(struct stm_fe_diseqc_s *priv,
			stm_fe_diseqc_mode_t mode, stm_fe_diseqc_msg_t *msg_d,
			uint32_t num_msg)
{
	FE_STV0900_Handle_t hdl;
	struct stm_fe_demod_s *demod_priv;
	struct stm_fe_lnb_s *lnb_priv;
	FE_Sat_DiseqC_TxMode mode_d = FE_SAT_22KHZ_Continues;
	FE_STV0900_Error_t err;
	struct lnb_config_t lnbconfig;
	int ret = 0;
	int i = 0;
	unsigned char tx_data[8];
	uint32_t nbdata = 1;
	bool enable_setconfig = true;

	demod_priv = (struct stm_fe_demod_s *)priv->demod_h;
	lnb_priv = (struct stm_fe_lnb_s *)demod_priv->lnb_h;
	hdl = demod_priv->demod_params;

	memcpy(&lnbconfig, &lnb_priv->lnbparams, sizeof(struct lnb_config_t));

	if (!lnb_priv->ops->setconfig) {
		pr_err("%s: FP lnb_setconfig is NULL\n", __func__);
		return -EFAULT;
	}
	if (lnb_priv->lnbparams.txmode == LNB_TTX_NONE)
		enable_setconfig = false;

	if (enable_setconfig) {
		lnbconfig.txmode = LNB_TX;
		ret = lnb_priv->ops->setconfig(lnb_priv, &lnbconfig);
	}

	if (mode & STM_FE_DISEQC_COMMAND)
		mode_d = diseqc_stv0900_get_mode(hdl, mode, msg_d[i].msg,
							&msg_d[i].msg_len);
	else
		mode_d = diseqc_stv0900_get_mode(hdl, mode, tx_data, &nbdata);

	err = FE_STV0900_DiseqcInit(hdl, mode_d, THIS_PATH(priv));
	if (err != FE_LLA_NO_ERROR) {
		pr_err("%s: Diseqc init failed\n", __func__);
		return -EFAULT;
	}

	if (!(mode & STM_FE_DISEQC_COMMAND)) {
		err = FE_STV0900_DiseqcSend(hdl, tx_data, nbdata,
						THIS_PATH(priv));
		if (err != FE_LLA_NO_ERROR) {
			pr_err("%s: Error in DiseqcSend\n", __func__);
			return -EFAULT;
		}
		return 0;
	}

	while (i < num_msg) {
		if (msg_d[i].op == STM_FE_DISEQC_TRANSFER_SEND) {
			err = FE_STV0900_DiseqcSend(hdl, msg_d[i].msg,
					msg_d[i].msg_len, THIS_PATH(priv));

			if (err != FE_LLA_NO_ERROR) {
				pr_err("%s: Error in DiseqcSend\n", __func__);
				return -EFAULT;
			}
		} else { /* (msg_d[i].op == STM_FE_DISEQC_TRANSFER_RECEIVE) */
			if (enable_setconfig) {
				lnbconfig.txmode = LNB_RX;
				ret = lnb_priv->ops->setconfig(lnb_priv,
								    &lnbconfig);
			}
			/* wait for reception ms */
			WAIT_N_MS(msg_d[i].timeout);
			err = FE_STV0900_DiseqcReceive(hdl, msg_d[i].msg,
					&msg_d[i].msg_len, THIS_PATH(priv));

			if (err != FE_LLA_NO_ERROR) {
				pr_err("%s: Err in DiseqcReceive\n", __func__);
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

static int32_t __init stm_fe_diseqc_stv0900_init(void)
{
	pr_info("Loading diseqc_stv0900 driver module...\n");

	return 0;
}

module_init(stm_fe_diseqc_stv0900_init);

static void __exit stm_fe_diseqc_stv0900_term(void)
{
	pr_info("Removing diseqc_stv0900 driver module ...\n");
}

module_exit(stm_fe_diseqc_stv0900_term);

MODULE_DESCRIPTION("Low level driver for STV0900 diseqc controller");
MODULE_AUTHOR("STMicroelectronics Limited");
MODULE_LICENSE("GPL");
