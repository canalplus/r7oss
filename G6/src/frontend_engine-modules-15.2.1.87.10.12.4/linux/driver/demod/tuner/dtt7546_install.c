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

Source file name :dtt7546_install.c
Author :          Ankur

wrapper for dtt7546 LLA

Date        Modification                                    Name
----        ------------                                    --------
21-Sep-11   Created                                         Ankur

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
#include "tuner_wrapper.h"
#include <stm_fe_demod.h>
#include <fecab_commlla_str.h>
#include "fe_tc_tuner.h"
#include "dtt7546_tuner.h"

/*-------------------------------------------------------------------------
					Private Function prototypes
----------------------------------------------------------------------- */


/*
 * Name: stm_fe_dtt7546_attach()
 *
 * Description: installed the wrapper function pointers for tuner LLA
 *
 */
int stm_fe_dtt7546_attach(struct stm_fe_demod_s *priv)
{
	size_t size;
	TUNER_Params_t tuner_params;
	struct tuner_ops_s *tuner_ops;
	size = DTT7546_NBREGS * sizeof(STCHIP_Register_t);

	priv->tuner_h->pRegMapImage = stm_fe_malloc(size);
	if (!priv->tuner_h->pRegMapImage) {
		pr_err("%s: mem alloc failed for Tunerregmap\n", __func__);
		return -ENOMEM;
	}

	/*Tuner internal params */

	tuner_params = stm_fe_malloc(sizeof(TUNER_InitParams_t));
	if (!tuner_params) {
		stm_fe_free((void **)&priv->tuner_h->pRegMapImage);
		pr_err("%s: mem alloc failed for tuner params\n", __func__);
		return -ENOMEM;
	}

	priv->tuner_h->pData = (void *)tuner_params;

	if (!priv->tuner_h->tuner_ops) {
		stm_fe_free((void **)&tuner_params);
		stm_fe_free((void **)&priv->tuner_h->pRegMapImage);
		pr_err("%s: failed due to NULL tuner_ops\n", __func__);
		return -EFAULT;
	}

	tuner_ops = priv->tuner_h->tuner_ops;

	tuner_ops->init = DTT7546_TunerInit;
	tuner_ops->set_frequency = DTT7546_TunerSetFrequency;
	tuner_ops->set_referenceFreq = DTT7546_TunerSetReferenceFreq;
	tuner_ops->set_bandWidth = DTT7546_TunerSetBandWidth;
	tuner_ops->get_status = DTT7546_TunerGetStatus;
	tuner_ops->get_frequency = DTT7546_TunerGetFrequency;
	tuner_ops->switch_to_dvbc = DTT7546_SwitchTunerToDVBC;
	tuner_ops->switch_to_dvbt = DTT7546_SwitchTunerToDVBT;
	tuner_ops->term = DTT7546_TunerTerm;
	tuner_ops->detach = stm_fe_dtt7546_detach;

	return 0;

}
EXPORT_SYMBOL(stm_fe_dtt7546_attach);

/*
 * Name: stm_fe_dtt7546_detach()
 *
 * Description: uninstalled the function pointers for tuner LLA
 *
 */

int stm_fe_dtt7546_detach(STCHIP_Handle_t tuner_h)
{
	TUNER_Params_t tuner_param;
	struct tuner_ops_s *tuner_ops = tuner_h->tuner_ops;

	if (tuner_ops) {
		tuner_ops->detach = NULL;
		tuner_ops->term = NULL;
		tuner_ops->switch_to_dvbt = NULL;
		tuner_ops->switch_to_dvbc = NULL;
		tuner_ops->get_frequency = NULL;
		tuner_ops->get_status = NULL;
		tuner_ops->set_bandWidth = NULL;
		tuner_ops->set_referenceFreq = NULL;
		tuner_ops->set_frequency = NULL;
		tuner_ops->init = NULL;
	}

	tuner_param = (TUNER_Params_t) (tuner_h->pData);
	stm_fe_free((void **)&tuner_param);
	stm_fe_free((void **)&tuner_h->pRegMapImage);

	return 0;
}
EXPORT_SYMBOL(stm_fe_dtt7546_detach);

static int32_t __init dtt7546_lla_init(void)
{
	pr_info("Loading dtt7546 tuner module ...\n");

	return 0;
}

module_init(dtt7546_lla_init);

static void __exit dtt7546_lla_term(void)
{
	pr_info("Removing dtt7546 tuner module ...\n");

}

module_exit(dtt7546_lla_term);

MODULE_DESCRIPTION("Low level driver for DTT7546 Thomson terrestrial and cable tuner");
MODULE_AUTHOR("STMicroelectronics Limited");
MODULE_LICENSE("GPL");
