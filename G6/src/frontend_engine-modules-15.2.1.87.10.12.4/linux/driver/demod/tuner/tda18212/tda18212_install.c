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

Source file name :tda18212_install.c
Author :          Manu

wrapper for TDA18212 LLA

Date        Modification                                    Name
----        ------------                                    --------
09-July-13   Created                                         Manu

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
#include "tuner_wrapper.h"
#include <stm_fe_demod.h>
#include <fecab_commlla_str.h>
#include "fe_tc_tuner.h"
#include "tda18212_tuner.h"

/*
 * Name: stm_fe_tda18212_attach()
 *
 * Description: installed the wrapper function pointers for tuner LLA
 *
 */
int stm_fe_tda18212_attach(struct stm_fe_demod_s *priv)
{
	size_t size;
	TUNER_Params_t tuner_params;
	struct tuner_ops_s *tuner_ops;
	size = TDA18212_NBREGS * sizeof(STCHIP_Register_t);

	priv->tuner_h->pRegMapImage = stm_fe_malloc(size);
	if (!priv->tuner_h->pRegMapImage) {
		pr_err("%s: mem alloc failed for reg map\n", __func__);
		return -ENOMEM;
	}

	/*Tuner internal params */

	tuner_params = stm_fe_malloc(sizeof(TUNER_InitParams_t));
	if (!tuner_params) {
		stm_fe_free((void **)&priv->tuner_h->pRegMapImage);
		pr_err("%s: mem alloc failed for tuner params\n", __func__);
		return -ENOMEM;
	}
	tuner_params->pAddParams = stm_fe_malloc(sizeof(TDA18212_TUNER_tds));
	if (!tuner_params->pAddParams) {
		stm_fe_free((void **)&tuner_params);
		stm_fe_free((void **)&priv->tuner_h->pRegMapImage);
		pr_err("%s: mem alloc failed pAddParams\n", __func__);
		return -ENOMEM;
	}
	priv->tuner_h->pData = (void *)tuner_params;
	if (!priv->tuner_h->tuner_ops) {
		stm_fe_free((void **)&tuner_params->pAddParams);
		stm_fe_free((void **)&tuner_params);
		stm_fe_free((void **)&priv->tuner_h->pRegMapImage);
		pr_err("%s: failed due to NULL tuner_ops\n", __func__);
		return -EFAULT;
	}

	tuner_ops = priv->tuner_h->tuner_ops;

	tuner_ops->init = TDA18212_TunerInit;
	tuner_ops->set_frequency = TDA18212_TunerSetFrequency;
	tuner_ops->set_bandWidth = TDA18212_TunerSetBandWidth;
	tuner_ops->set_standby = TDA18212_TunerSetStandby;
	tuner_ops->get_status = TDA18212_TunerGetStatus;
	tuner_ops->get_frequency = TDA18212_TunerGetFrequency;
	tuner_ops->get_rflevel = TDA18212_TunerEstimateRfPower;
	tuner_ops->switch_to_dvbc = TDA18212_SwitchTunerToDVBC;
	tuner_ops->switch_to_dvbt = TDA18212_SwitchTunerToDVBT;
	tuner_ops->term = TDA18212_TunerTerm;
	tuner_ops->detach = stm_fe_tda18212_detach;

	return 0;

}
EXPORT_SYMBOL(stm_fe_tda18212_attach);

/*
 * Name: stm_fe_tda18212_detach()
 *
 * Description: uninstalled the function pointers for tuner LLA
 *
 */
int stm_fe_tda18212_detach(STCHIP_Handle_t tuner_h)
{
	int err = 0;

	TUNER_Params_t tuner_param;
	struct tuner_ops_s *tuner_ops = tuner_h->tuner_ops;

	if (tuner_ops) {
		tuner_ops->detach = NULL;
		tuner_ops->term = NULL;
		tuner_ops->switch_to_dvbt = NULL;
		tuner_ops->switch_to_dvbc = NULL;
		tuner_ops->get_rflevel = NULL;
		tuner_ops->get_frequency = NULL;
		tuner_ops->get_status = NULL;
		tuner_ops->set_standby = NULL;
		tuner_ops->set_bandWidth = NULL;
		tuner_ops->set_frequency = NULL;
		tuner_ops->init = NULL;
	}

	tuner_param = (TUNER_Params_t) (tuner_h->pData);
	stm_fe_free((void **)&tuner_param->pAddParams);
	stm_fe_free((void **)&tuner_param);
	stm_fe_free((void **)&tuner_h->pRegMapImage);

	return err;
}
EXPORT_SYMBOL(stm_fe_tda18212_detach);

static int32_t __init tda18212_lla_init(void)
{
	pr_info("Loading tda18212 tuner module ...\n");

	return 0;
}

module_init(tda18212_lla_init);

static void __exit tda18212_lla_term(void)
{
	pr_info("Removing tda18212 tuner module ...\n");

}

module_exit(tda18212_lla_term);

MODULE_DESCRIPTION("Low level driver for TDA18212 terrestrial tuner");
MODULE_AUTHOR("STMicroelectronics Limited");
MODULE_LICENSE("GPL");
