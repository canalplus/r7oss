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

Source file name :stv6110_wrapper.c
Author :           Shobhit

STV6110 installation functions

Date        Modification                                    Name
----        ------------                                    --------
20-Jun-11   Created                                         Shobhit

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
#include <fesat_commlla_str.h>
#include "fe_sat_tuner.h"
#include "stv6110_tuner.h"
#include "stv6110_install.h"

/*
 * Name: stm_fe_stv6110_attach()
 *
 * Description: installed the wrapper function pointers for tuner LLA
 *
 */
int stm_fe_stv6110_attach(struct stm_fe_demod_s *priv)
{
	size_t size;
	SAT_TUNER_Params_t tuner_params;
	struct tuner_ops_s *tuner_ops;
	size = STV6110_NBREGS * sizeof(STCHIP_Register_t);

	priv->tuner_h->pRegMapImage = stm_fe_malloc(size);
	if (!priv->tuner_h->pRegMapImage) {
		pr_err("%s: mem alloc failed for Tuner reg map\n", __func__);
		return -ENOMEM;
	}
	/*Tuner internal params */

	tuner_params = stm_fe_malloc(sizeof(SAT_TUNER_InitParams_t));
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

	tuner_ops->init = STV6110_TunerInit;
	tuner_ops->set_referenceFreq = STV6110_TunerSetReferenceFreq;
	tuner_ops->set_gain = STV6110_TunerSetGain;
	tuner_ops->set_frequency = STV6110_TunerSetFrequency;
	tuner_ops->set_bandWidth = STV6110_TunerSetBandwidth;
	tuner_ops->set_standby = STV6110_TunerSetStandby;
	tuner_ops->get_status = STV6110_TunerGetStatus;
	tuner_ops->get_frequency = STV6110_TunerGetFrequency;
	tuner_ops->term = STV6110_TunerTerm;
	tuner_ops->detach = stm_fe_stv6110_detach;

	return 0;
}
EXPORT_SYMBOL(stm_fe_stv6110_attach);

/*
 * Name: stm_fe_stv6110_detach()
 *
 * Description: uninstalled the function pointers for tuner LLA
 *
 */

int stm_fe_stv6110_detach(STCHIP_Handle_t tuner_h)
{
	SAT_TUNER_Params_t tuner_params;
	struct tuner_ops_s *tuner_ops = tuner_h->tuner_ops;

	if (tuner_ops) {
		tuner_ops->detach = NULL;
		tuner_ops->term = NULL;
		tuner_ops->get_frequency = NULL;
		tuner_ops->get_status = NULL;
		tuner_ops->set_standby = NULL;
		tuner_ops->set_bandWidth = NULL;
		tuner_ops->set_frequency = NULL;
		tuner_ops->set_gain = NULL;
		tuner_ops->set_referenceFreq = NULL;
		tuner_ops->init = NULL;
	}

	tuner_params = (SAT_TUNER_Params_t)(tuner_h->pData);
	stm_fe_free((void **)&tuner_params);
	stm_fe_free((void **)&tuner_h->pRegMapImage);

	return 0;
}
EXPORT_SYMBOL(stm_fe_stv6110_detach);


static int32_t __init stv6110_lla_init(void)
{
	pr_info("Loading stv6110 tuner module ...\n");

	return 0;
}

module_init(stv6110_lla_init);

static void __exit stv6110_lla_term(void)
{
	pr_info("Removing stv6110 tuner module ...\n");

}

module_exit(stv6110_lla_term);

MODULE_DESCRIPTION("Low level driver for STV6110 satellite tuner");
MODULE_AUTHOR("STMicroelectronics Limited");
MODULE_LICENSE("GPL");
