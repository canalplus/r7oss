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

Source file name : lnbh24_wrapper.c
Author :          Shobhit

Low level function installtion for lnbh24

Date        Modification                                    Name
----        ------------                                    --------
16-Jun-11   Created                                         Shobhit
11-Apr-12   Keeping copy of lnbconfig in lnbh24_setconf   Ankur Tyagi
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
#include <stm_fe_lnb.h>
#include <stm_fe_diseqc.h>
#include <stm_fe_install.h>
#include <fesat_commlla_str.h>
#include "lnbh24.h"

static int lnbh24_init(struct stm_fe_lnb_s *priv);
static int lnbh24_setconf(struct stm_fe_lnb_s *priv, struct lnb_config_t *conf);
static int lnbh24_getconf(struct stm_fe_lnb_s *priv, struct lnb_config_t *conf);
static int lnbh24_term(struct stm_fe_lnb_s *priv);

static uint8_t conf_to_regval(struct lnb_config_t *conf);
static void regval_to_conf(uint8_t regval, struct lnb_config_t *conf);

static int lnbh24_suspend(struct stm_fe_lnb_s *priv);
static int lnbh24_resume(struct stm_fe_lnb_s *priv);
/*
 * Name: stm_fe_lnbh24_attach()
 *
 * Description: Installed the wrapper function pointers for LNBH24 driver
 *
 */
int stm_fe_lnbh24_attach(struct stm_fe_lnb_s *priv)
{
	if (!priv->ops)
		return -EFAULT;

	priv->ops->init = lnbh24_init;
	priv->ops->setconfig = lnbh24_setconf;
	priv->ops->getconfig = lnbh24_getconf;
	priv->ops->term = lnbh24_term;
	priv->ops->suspend = lnbh24_suspend;
	priv->ops->resume = lnbh24_resume;
	priv->ops->detach = stm_fe_lnbh24_detach;

	return 0;
}
EXPORT_SYMBOL(stm_fe_lnbh24_attach);

/*
 * Name: stm_fe_lnbh24_detach()
 *
 * Description: uninstall the function pointers of LNBH24 driver
 *
 */
int stm_fe_lnbh24_detach(struct stm_fe_lnb_s *priv)
{
	if (!priv->ops)
		return -EFAULT;

	priv->ops->detach = NULL;
	priv->ops->term = NULL;
	priv->ops->suspend = NULL;
	priv->ops->resume = NULL;
	priv->ops->getconfig = NULL;
	priv->ops->setconfig = NULL;
	priv->ops->init = NULL;

	return 0;
}
EXPORT_SYMBOL(stm_fe_lnbh24_detach);

/*
 * Name: lnbh24_init()
 *
 * Description: Initilialization of LNBH24
 *
 */
int lnbh24_init(struct stm_fe_lnb_s *priv)
{

	uint8_t regval;
	size_t size;
	int ret = 0;
	struct lnb_config_t *lnbparams_p;
	STCHIP_Error_t err = CHIPERR_NO_ERROR;
	priv->lnb_i2c = chip_open(priv->config->lnb_io.bus);

	if (!priv->lnb_i2c)
		return -EINVAL;
	memcpy(&(priv->lnb_h->dev_i2c), &(priv->lnb_i2c),
			sizeof(priv->lnb_i2c));

	lnbparams_p = stm_fe_malloc(sizeof(struct lnb_config_t));

	if (!lnbparams_p) {
		pr_err("%s: mem alloc failed lnb internal struct\n", __func__);
		ret = -ENOMEM;
		goto lnb_config_t_alloc_fail;
	}
	priv->lnb_h->pData = (void *)lnbparams_p;
	size = sizeof(STCHIP_Register_t);
	priv->lnb_h->pRegMapImage = stm_fe_malloc(size);
	if (!priv->lnb_h->pRegMapImage) {
		pr_err("%s: mem alloc failed for lnb reg map\n", __func__);
		ret = -ENOMEM;
		goto lnb_reg_map_alloc_fail;
	}
	priv->prev_lnb_status_on = false;
	priv->lnb_h->RepeaterHost = NULL;	/* Repeater host */
	priv->lnb_h->RepeaterFn = NULL;	/* Repeater enable/disable function */
	priv->lnb_h->Repeater = FALSE;	/* Tuner need to enable repeater */
	priv->lnb_h->NbRegs = LNBH24_NBREGS;
	priv->lnb_h->NbFields = LNBH24_NBFIELDS;
	priv->lnb_h->ChipMode = STCHIP_MODE_NOSUBADR;
	priv->lnb_h->WrStart = RLNBH24_REGS;
	priv->lnb_h->WrSize = 1;
	priv->lnb_h->RdStart = RLNBH24_REGS;
	priv->lnb_h->RdSize = 1;
	priv->lnb_h->IORoute = priv->config->lnb_io.route;
	priv->lnb_h->IODriver = priv->config->lnb_io.io;
	priv->lnb_h->I2cAddr = priv->config->lnb_io.address;
	priv->lnb_h->Error = 0;
	priv->lnb_h->Abort = false;
	/* Set the default values */
	lnbparams_p->status = LNB_STATUS_OFF;
	lnbparams_p->polarization = STM_FE_LNB_PLR_HORIZONTAL;
	lnbparams_p->tonestate = STM_FE_LNB_TONE_OFF;
	lnbparams_p->txmode = LNB_TX;

	regval = conf_to_regval(lnbparams_p);

	/*set LNB Register */

	err = ChipSetOneRegister(priv->lnb_h, (uint16_t) RLNBH24_REGS, regval);

	if (err != CHIPERR_NO_ERROR) {
		ret = -EFAULT;
		goto set_lnb_reg_fail;
	}

	return ret;

set_lnb_reg_fail:
	stm_fe_free((void **)&priv->lnb_h->pRegMapImage);
lnb_reg_map_alloc_fail:
	stm_fe_free((void **)&priv->lnb_h->pData);
lnb_config_t_alloc_fail:
	priv->lnb_i2c = NULL;
	return ret;
}

/*
 * Name: lnbh24_term()
 *
 * Description: termination of LNBH24
 *
 */
int lnbh24_term(struct stm_fe_lnb_s *priv)
{
	int ret = -1;

	priv->lnb_i2c = NULL;
	priv->lnb_h->dev_i2c = NULL;
	ret = stm_fe_free((void **)&priv->lnb_h->pRegMapImage);
	if (ret) {
		pr_err("%s: tried to free NULL ptr pRegMapImage\n", __func__);
		return ret;
	}
	priv->lnb_h->pRegMapImage = NULL;
	ret = stm_fe_free((void **)&priv->lnb_h->pData);
	if (ret) {
		pr_err("%s: tried to free NULL ptr pData\n", __func__);
		return ret;
	}
	priv->lnb_h->pData = NULL;
	return ret;
}

/*
  * Name: lnbh24_suspend()
  *
  * Description: suspends LNBH24 device
  *
  */
int lnbh24_suspend(struct stm_fe_lnb_s *priv)
{
	int ret = 0;
	struct lnb_config_t *conf;

	conf = &priv->lnbparams;
	ret = lnbh24_setconf(priv, conf);
	return ret;
}
/*
  * Name: lnbh24_resume()
  *
  * Description: resume power status of LNBH24 device back to normal
  *
  */
int lnbh24_resume(struct stm_fe_lnb_s *priv)
{
	int ret = 0;
	struct lnb_config_t *conf;

	conf = &priv->lnbparams;
	if (priv->prev_lnb_status_on == false)
		return ret;

	ret = lnbh24_setconf(priv, conf);
	return ret;
}
 /*
  * Name: lnbh24_setconf()
  *
  * Description: Configure the LNBH24 device according to configuartion values
  *
  */
int lnbh24_setconf(struct stm_fe_lnb_s *priv, struct lnb_config_t *conf)
{

	uint8_t regval;
	struct lnb_config_t *config;

	config = &priv->lnbparams;
	regval = conf_to_regval(config);
	/*set LNB Register */
	regval = ChipSetOneRegister(priv->lnb_h, (uint16_t) RLNBH24_REGS,
									regval);
	if (regval != CHIPERR_NO_ERROR) {
		pr_err("%s: LNBH24 i/o write error occurred\n", __func__);
		return -EIO;
	}
	memcpy(&priv->lnbparams, config, sizeof(struct lnb_config_t));

	if (priv->lnbparams.status == LNB_STATUS_ON)
		priv->prev_lnb_status_on = true;
	else
		priv->prev_lnb_status_on = false;

	return 0;
}

/*
  * Name: lnbh24_getconf()
  *
  * Description: get the status of LNBH24 device
  *
  */
int lnbh24_getconf(struct stm_fe_lnb_s *priv, struct lnb_config_t *config)
{

	uint8_t regval = 0;

	/*get LNB Register */
	regval = ChipGetOneRegister(priv->lnb_h, (uint16_t) RLNBH24_REGS);
	if (regval != CHIPERR_NO_ERROR) {
		pr_err("%s: LNBH24 i/o read error occurred\n", __func__);
		return -EIO;
	}
	regval_to_conf(regval, config);
	return 0;
}

/*
 * Name: regval_to_conf()
 *
 * Description: preparing configuration values after reading LNBH24 register
 *
 */
void regval_to_conf(uint8_t regval, struct lnb_config_t *config)
{

	if (regval & FLNBH24_OLF)
		config->status = LNB_STATUS_SHORT_CIRCUIT;
	else if (regval & FLNBH24_OTF)
		config->status = LNB_STATUS_OVER_TEMPERATURE;
	if ((regval & FLNBH24_OLF) && (regval & FLNBH24_OTF))
		config->status = LNB_STATUS_SHORTCIRCUIT_OVERTEMPERATURE;
}

/*
 * Name: conf_to_regval()
 *
 * Description: preparing configuration values for writing LNBH24 register
 *
 */

uint8_t conf_to_regval(struct lnb_config_t *config)
{

	uint8_t regval = 0;

	/* Default value of Bit 0 = 0 => Normal operation */
	/* Default value of Bit 1 = 0 => Minimum output currrent
	 *diagnostic threshold = 6mA */

	/* Bit 2 */
	regval |= ((config->status == LNB_STATUS_ON)
		   && (config->polarization != STM_FE_LNB_PLR_OFF)) ?
							FLNBH24_EN : 0;

	/* Bit 3 */
	regval |= (config->polarization == STM_FE_LNB_PLR_HORIZONTAL) ?
							FLNBH24_VSEL : 0;

	/* Bit 4 */
	/*regval |= (Config->CableLossCompensation == TRUE) ? FLNBH24_LLC : 0;*/

	/* Bit 5 */
	regval |=
	    (config->tonestate == STM_FE_LNB_TONE_22KHZ) ? FLNBH24_TEN : 0;

	/* Bit 6 */

	regval |= (config->txmode == LNB_TX) ? FLNBH24_TTX : 0;

	return regval;

}

static int32_t __init stm_fe_lnbh24(void)
{

	pr_info("Loading lnbh24 driver module...\n");

	return 0;

}

module_init(stm_fe_lnbh24);

static void __exit stm_fe_lnbh24_term(void)
{

	pr_info("Removing lnbh24 driver module ...\n");

}

module_exit(stm_fe_lnbh24_term);

MODULE_DESCRIPTION("Low level driver for LNBH24 LNB controller");
MODULE_AUTHOR("STMicroelectronics Limited");
MODULE_LICENSE("GPL");
