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

Source file name : lnbh26.c
Author :              Rahul

Low level driver for lnbh26 LNB controller

Date        Modification                                    Name
----        ------------                                --------
18-Apr-12   Created                                     Rahul
3-Dec-2013  Ku High band correction			HS
24-Jun-14   mxl lnb support				HS
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
#include "lnbh26.h"

static int lnbh26_init(struct stm_fe_lnb_s *priv);
static int lnbh26_setconf(struct stm_fe_lnb_s *priv, struct lnb_config_t *conf);
static int lnbh26_getconf(struct stm_fe_lnb_s *priv, struct lnb_config_t *conf);
static int lnbh26_term(struct stm_fe_lnb_s *priv);
static int lnbh26_suspend(struct stm_fe_lnb_s *priv);
static int lnbh26_resume(struct stm_fe_lnb_s *priv);

static STCHIP_Error_t conf_to_regval(struct stm_fe_lnb_s *priv,
		struct lnb_config_t *conf, uint32_t *start, uint32_t *nbregs);
static void regval_to_conf(struct stm_fe_lnb_s *priv,
						struct lnb_config_t *conf);

static int ref_counter_p1;

static int get_path(struct stm_fe_lnb_s *priv)
{
	if (IS_NAME_MATCH(priv->config->lnb_name, "LNBH26P1"))
		return 1;
	else
		return 2;
}

/*
 * Name: stm_fe_lnbh26_attach()
 *
 * Description: Installed the wrapper function pointers for LNBH26 driver
 *
 */
int stm_fe_lnbh26_attach(struct stm_fe_lnb_s *priv)
{
	if (!priv->ops)
		return -EFAULT;

	priv->ops->init = lnbh26_init;
	priv->ops->setconfig = lnbh26_setconf;
	priv->ops->getconfig = lnbh26_getconf;
	priv->ops->suspend = lnbh26_suspend;
	priv->ops->resume = lnbh26_resume;
	priv->ops->term = lnbh26_term;
	priv->ops->detach = stm_fe_lnbh26_detach;
	return 0;
}
EXPORT_SYMBOL(stm_fe_lnbh26_attach);

struct stm_fe_lnb_s *
	get_lnb_from_dev_name(struct stm_fe_lnb_s *priv, char *name)
{
	struct list_head *pos;
	list_for_each(pos, priv->lnb_list_p) {
		struct stm_fe_lnb_s *assoc_lnb;
		assoc_lnb = list_entry(pos, struct stm_fe_lnb_s, list);
		if (IS_NAME_MATCH(assoc_lnb->config->lnb_name, name)
				&& assoc_lnb->config->lnb_io.address ==
					priv->config->lnb_io.address
				&& assoc_lnb->config->lnb_io.bus ==
					priv->config->lnb_io.bus)
			return assoc_lnb;
	}
	return NULL;
}

/*
 * Name: stm_fe_lnbh26_detach()
 *
 * Description: uninstall the function pointers of LNBH26 driver
 *
 */
int stm_fe_lnbh26_detach(struct stm_fe_lnb_s *priv)
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
EXPORT_SYMBOL(stm_fe_lnbh26_detach);

/*
 * Name: lnbh26_init()
 *
 * Description: Initilialization of LNBH26
 *
 */
int lnbh26_init(struct stm_fe_lnb_s *priv)
{
	uint32_t nbregs = 0, start = 0;
	int ret = 0;
	STCHIP_Error_t err = CHIPERR_NO_ERROR;
	struct stm_fe_lnb_s *assoc_priv = NULL;
	struct list_head *pos;
	struct stm_fe_lnb_s *temp_priv = NULL;

	int path = get_path(priv);
	STCHIP_Register_t DefLNBH26Val1[LNBH26_NBREGS] = {
		{RLNBH26_STATUS1, 0x00},
		{RLNBH26_STATUS2, 0x00},
		{RLNBH26_DATA1, 0x00},/*both Paths at 18 V*/
		{RLNBH26_DATA2, 0x00},/*Tone OFF ,Low Power OFF,Modulated*/
		{RLNBH26_DATA3, 0x00},/*ISET,ISW,PCL,TON=90 ms*/
		{RLNBH26_DATA4, 0x00},
	};

	if (path == 1)
		assoc_priv = get_lnb_from_dev_name(priv, "LNBH26P2");
	else
		assoc_priv = get_lnb_from_dev_name(priv, "LNBH26P1");

	priv->lnb_i2c = chip_open(priv->config->lnb_io.bus);
	if (!priv->lnb_i2c)
		return -EINVAL;
	priv->prev_lnb_status_on = false;
	priv->lnb_h->dev_i2c = priv->lnb_i2c;
	priv->lnb_h->RepeaterHost = NULL;	/* Repeater host */
	priv->lnb_h->RepeaterFn = NULL;	/* Repeater enable/disable function */
	priv->lnb_h->Repeater = FALSE;	/* Tuner need to enable repeater */
	priv->lnb_h->NbRegs = LNBH26_NBREGS;
	priv->lnb_h->NbFields = LNBH26_NBFIELDS;
	priv->lnb_h->ChipMode = STCHIP_MODE_SUBADR_8;
	priv->lnb_h->WrStart = RLNBH26_DATA1;
	priv->lnb_h->WrSize = 4;
	priv->lnb_h->RdStart = RLNBH26_STATUS1;
	priv->lnb_h->RdSize = 2;
	priv->lnb_h->IORoute = priv->config->lnb_io.route;
	priv->lnb_h->IODriver = priv->config->lnb_io.io;
	priv->lnb_h->I2cAddr = priv->config->lnb_io.address;
	priv->lnb_h->Error = 0;
	priv->lnb_h->Abort = false;

	/*mxl582 instantiates 8 lnb;the board has a single lnbh26 that has
		2 lnb's sharing a common regmap.
		->	Memory allocation for regmap only once,
		->	all virtual lnb instances share the regmap*/
	if (assoc_priv && assoc_priv->lnb_h) {
		priv->lnb_h->pRegMapImage = assoc_priv->lnb_h->pRegMapImage;
	} else if (!strncmp(priv->demod_h->config->demod_name, "MXL582",
		/*mxl lnb case*/			sizeof("MXL582"))) {
		if (!ref_counter_p1) {
			priv->lnb_h->pRegMapImage = stm_fe_malloc(
				LNBH26_NBREGS *	sizeof(STCHIP_Register_t));
			ref_counter_p1++;
			if (!priv->lnb_h->pRegMapImage) {
				pr_err("%s: mem alloc failed for reg map\n",
					__func__);
				ret = -ENOMEM;
				goto lnb_reg_map_alloc_fail;
			}
			ChipUpdateDefaultValues(priv->lnb_h, DefLNBH26Val1);
		} else {
			list_for_each(pos, priv->lnb_list_p) {
				temp_priv = list_entry(pos,
						struct stm_fe_lnb_s, list);
				if (temp_priv->lnb_id == 0)
					break;
			}
			priv->lnb_h->pRegMapImage =
						temp_priv->lnb_h->pRegMapImage;
		}
	} else {	/*the 9xx lnb case*/
		priv->lnb_h->pRegMapImage = stm_fe_malloc(LNBH26_NBREGS *
				sizeof(STCHIP_Register_t));
		if (!priv->lnb_h->pRegMapImage) {
			pr_err("%s: mem alloc failed for reg map\n", __func__);
			ret = -ENOMEM;
			goto lnb_reg_map_alloc_fail;
		}
		ChipUpdateDefaultValues(priv->lnb_h, DefLNBH26Val1);
	}
	/* Set the default values */
	priv->lnbparams.status = LNB_STATUS_OFF;
	priv->lnbparams.polarization = STM_FE_LNB_PLR_HORIZONTAL;
	priv->lnbparams.tonestate = STM_FE_LNB_TONE_OFF;
	priv->lnbparams.txmode = LNB_TTX_NONE;

	priv->lnb_h->pData = &priv->lnbparams;
	conf_to_regval(priv, &priv->lnbparams, &start, &nbregs);

	/*set LNB Registers */
	err = ChipSetRegisters(priv->lnb_h, start, nbregs);
	if (err != CHIPERR_NO_ERROR) {
		ret = -EFAULT;
		goto set_lnb_reg_fail;
	}

	return ret;

set_lnb_reg_fail:
	if (!assoc_priv && priv->lnb_h->pRegMapImage)
		stm_fe_free((void **)&priv->lnb_h->pRegMapImage);

	if (assoc_priv && !assoc_priv->lnb_h && priv->lnb_h->pRegMapImage)
		stm_fe_free((void **)&priv->lnb_h->pRegMapImage);
lnb_reg_map_alloc_fail:
	priv->lnb_i2c = NULL;
	priv->lnb_h->pData = NULL;
	return ret;
}

/*
 * Name: lnbh26_term()
 *
 * Description: termination of LNBH26
 *
 */
int lnbh26_term(struct stm_fe_lnb_s *priv)
{
	int ret = 0;
	struct stm_fe_lnb_s *assoc_priv = NULL;
	int path = get_path(priv);

	if (path == 1)
		assoc_priv = get_lnb_from_dev_name(priv, "LNBH26P2");
	else
		assoc_priv = get_lnb_from_dev_name(priv, "LNBH26P1");

	priv->lnb_i2c = NULL;
	priv->lnb_h->dev_i2c = NULL;

	if (!assoc_priv && priv->lnb_h->pRegMapImage) {
		ret = stm_fe_free((void **)&priv->lnb_h->pRegMapImage);
		if (ret)
			pr_err("%s: tried to free NULL ptr pRegMapImage\n",
								      __func__);
	}

	if (assoc_priv && !assoc_priv->lnb_h && priv->lnb_h->pRegMapImage) {
		ret = stm_fe_free((void **)&priv->lnb_h->pRegMapImage);
		if (ret)
			pr_err("%s: tried to free NULL ptr pRegMapImage\n",
								      __func__);
	}
	priv->lnb_h->pRegMapImage = NULL;
	priv->lnb_h->pData = NULL;

	return ret;
}

 /*
  * Name: lnbh26_setconf()
  *
  * Description: Configure the LNBH26 device according to configuartion values
  *
  */
int lnbh26_setconf(struct stm_fe_lnb_s *priv, struct lnb_config_t *config)
{
	uint32_t nbregs, start;
	STCHIP_Error_t err = CHIPERR_NO_ERROR;
	int ret = 0;
	stm_fe_diseqc_mode_t mode;

	struct diseqc_ops_s *diseqc_ops;
	stm_fe_diseqc_h diseqc_h;
	struct stm_fe_diseqc_s *diseqc_priv;

	if (!(priv->demod_h->diseqc_h && priv->demod_h->diseqc_h->ops)) {
		pr_err("%s: LNBH26 uses ext modulation but no diseqc obj\n",
		       __func__);
		return -EFAULT;
	}

	diseqc_h = priv->demod_h->diseqc_h;
	diseqc_priv = (struct stm_fe_diseqc_s *)priv->demod_h->diseqc_h;
	diseqc_ops = priv->demod_h->diseqc_h->ops;

	conf_to_regval(priv, config, &start, &nbregs);

	err = ChipSetRegisters(priv->lnb_h, start, nbregs);
	if (err != CHIPERR_NO_ERROR) {
		pr_err("%s: LNBH26 i/o write err occurred\n", __func__);
		return -EIO;
	}

	if (priv->lnbparams.tonestate == STM_FE_LNB_TONE_22KHZ)
		mode = STM_FE_DISEQC_TONE_BURST_CONTINUOUS;
	else
		mode = STM_FE_DISEQC_TONE_BURST_OFF;

	if (config->status == LNB_STATUS_ON &&
				diseqc_priv->mode != STM_FE_DISEQC_COMMAND) {
		/*num_msg = 0 to ensure DiseqcSend is not
		called more than once for "generating tones"*/
		ret = diseqc_ops->transfer(diseqc_h, mode, NULL, 0);
		if (ret) {
			pr_err("%s:Diseqc tone burst send failed\n", __func__);
			return ret;
		}
	}
	priv->lnbparams = *config;

	if (priv->lnbparams.status == LNB_STATUS_ON)
		priv->prev_lnb_status_on = true;
	else
		priv->prev_lnb_status_on = false;


	return ret;
}

/*
  * Name: lnbh26_suspend()
  *
  * Description: suspends LNBH26 device
  *
  */
int lnbh26_suspend(struct stm_fe_lnb_s *priv)
{

	uint32_t nbregs, start;
	struct lnb_config_t *config;
	STCHIP_Error_t err = CHIPERR_NO_ERROR;
	int ret = 0;

	config = &priv->lnbparams;
	conf_to_regval(priv, config, &start, &nbregs);
	err = ChipSetRegisters(priv->lnb_h, start, nbregs);
	if (err != CHIPERR_NO_ERROR) {
		pr_err("%s: LNBH26 i/o write err occurred\n", __func__);
		return -EIO;
	}
	priv->lnbparams = *config;

	return ret;
}
/*
  * Name: lnbh26_resume()
  *
  * Description: resume power status of LNBH26 device back to normal
  *
  */
int lnbh26_resume(struct stm_fe_lnb_s *priv)
{
	uint32_t nbregs, start;
	struct lnb_config_t *config;
	STCHIP_Error_t err = CHIPERR_NO_ERROR;
	int ret = 0;
	config = &priv->lnbparams;

	if (priv->prev_lnb_status_on == false)
		return ret;

	conf_to_regval(priv, config, &start, &nbregs);

	err = ChipSetRegisters(priv->lnb_h, start, nbregs);
	if (err != CHIPERR_NO_ERROR) {
		pr_err("%s: LNBH26 i/o write err occurred\n", __func__);
		return -EIO;
	}
	priv->lnbparams = *config;

	return ret;
}
/*
  * Name: lnbh26_getconf()
  *
  * Description: get the status of LNBH26 device
  *
  */
int lnbh26_getconf(struct stm_fe_lnb_s *priv, struct lnb_config_t *config)
{
	int ret = 0;

	ChipGetRegisters(priv->lnb_h, priv->lnb_h->RdStart,
						priv->lnb_h->RdSize);
	if (priv->lnb_h->Error != CHIPERR_NO_ERROR) {
		pr_err("%s: LNBH26 i/o read err occurred\n", __func__);
		ret = -EIO;
	}

	regval_to_conf(priv, config);
	return ret;
}

/*
 * Name: regval_to_conf()
 *
 * Description: preparing configuration values after reading LNBH26 register
 *
 */
void regval_to_conf(struct stm_fe_lnb_s *priv,
						struct lnb_config_t *config)
{
	bool is_ol = 0;
	int path = get_path(priv);

	is_ol = ChipGetFieldImage(priv->lnb_h, FLNBH26_OLF(path));
	if (is_ol)
		config->status = LNB_STATUS_SHORT_CIRCUIT;

	if (ChipGetFieldImage(priv->lnb_h, FLNBH26_OTF)) {
		config->status = LNB_STATUS_OVER_TEMPERATURE;
		if (is_ol)
			config->status =
					LNB_STATUS_SHORTCIRCUIT_OVERTEMPERATURE;
	}
}

/*
 * Name: conf_to_regval()
 *
 * Description: preparing configuration values for writing LNBH26 register
 *
 */
STCHIP_Error_t conf_to_regval(struct stm_fe_lnb_s *priv,
	struct lnb_config_t *lnbconfig_p, uint32_t *start, uint32_t *nbregs)
{
	int path = get_path(priv);
	*start = priv->lnb_h->WrStart;
	*nbregs = priv->lnb_h->WrSize;

	if (lnbconfig_p->status == LNB_STATUS_ON) {
		ChipSetFieldImage(priv->lnb_h, FLNBH26_LPM(path), 0);
	} else {
		ChipSetFieldImage(priv->lnb_h, FLNBH26_VSEL1(path), 0);
		ChipSetFieldImage(priv->lnb_h, FLNBH26_VSEL2(path), 0);
		ChipSetFieldImage(priv->lnb_h, FLNBH26_VSEL3(path), 0);
		ChipSetFieldImage(priv->lnb_h, FLNBH26_VSEL4(path), 0);
		ChipSetFieldImage(priv->lnb_h, FLNBH26_TEN(path), 0);
		ChipSetFieldImage(priv->lnb_h, FLNBH26_LPM(path), 1);
		*start = RLNBH26_DATA1;
		*nbregs = 2;
		return priv->lnb_h->Error;
	}

	if (lnbconfig_p->polarization == STM_FE_LNB_PLR_HORIZONTAL) {
		ChipSetFieldImage(priv->lnb_h, FLNBH26_VSEL1(path), 1);
		ChipSetFieldImage(priv->lnb_h, FLNBH26_VSEL2(path), 0);
		ChipSetFieldImage(priv->lnb_h, FLNBH26_VSEL3(path), 0);
		ChipSetFieldImage(priv->lnb_h, FLNBH26_VSEL4(path), 1);
	} else if (lnbconfig_p->polarization == STM_FE_LNB_PLR_VERTICAL) {
		ChipSetFieldImage(priv->lnb_h, FLNBH26_VSEL1(path), 1);
		ChipSetFieldImage(priv->lnb_h, FLNBH26_VSEL2(path), 1);
		ChipSetFieldImage(priv->lnb_h, FLNBH26_VSEL3(path), 0);
		ChipSetFieldImage(priv->lnb_h, FLNBH26_VSEL4(path), 0);
	} else if (lnbconfig_p->polarization == STM_FE_LNB_PLR_OFF) {
		ChipSetFieldImage(priv->lnb_h, FLNBH26_VSEL1(path), 0);
		ChipSetFieldImage(priv->lnb_h, FLNBH26_VSEL2(path), 0);
		ChipSetFieldImage(priv->lnb_h, FLNBH26_VSEL3(path), 0);
		ChipSetFieldImage(priv->lnb_h, FLNBH26_VSEL4(path), 0);
	}

	if (lnbconfig_p->tonestate == STM_FE_LNB_TONE_22KHZ)
		ChipSetFieldImage(priv->lnb_h, FLNBH26_TEN(path), 1);
	else
		ChipSetFieldImage(priv->lnb_h, FLNBH26_TEN(path), 0);

	return priv->lnb_h->Error;
}

static int32_t __init stm_fe_lnbh26(void)
{
	pr_info("Loading lnbh26 driver module...\n");

	return 0;
}

module_init(stm_fe_lnbh26);

static void __exit stm_fe_lnbh26_term(void)
{
	pr_info("Removing lnbh26 driver module ...\n");
}

module_exit(stm_fe_lnbh26_term);

MODULE_DESCRIPTION("Low level driver for LNBH26 LNB controller");
MODULE_AUTHOR("STMicroelectronics Limited");
MODULE_LICENSE("GPL");
