/************************************************************************
Copyright(C) 2013 STMicroelectronics. All Rights Reserved.

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

Source file name :stv6111_install.c
Author :		HS

STV6111 installation functions

Date		Modification			Name
----		------------			--------
2 - Jun - 13	Created				 HS

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
#include "stv6111_tuner.h"
#include "stv6111_install.h"

/*
 * Name: stm_fe_stv6111_attach()
 *
 * Description: installed the wrapper function pointers for tuner LLA
 *
 */
int stm_fe_stv6111_attach(struct stm_fe_demod_s *priv)
{
	size_t size;
	SAT_TUNER_Params_t p;
	struct tuner_ops_s *ops;
	size = STV6111_NBREGS * sizeof(STCHIP_Register_t);
	priv->tuner_h->pRegMapImage = stm_fe_malloc(size);
	if (!priv->tuner_h->pRegMapImage) {
		pr_err("%s: mem alloc failed for Tuner reg map\n", __func__);
		return -ENOMEM;
	}
	/*Tuner internal params */

	p = stm_fe_malloc(sizeof(SAT_TUNER_InitParams_t));
	if (!p) {
		stm_fe_free((void **) &priv->tuner_h->pRegMapImage);
		pr_err("%s: mem alloc failed for tuner params\n", __func__);
		return -ENOMEM;
	}

	priv->tuner_h->pData = (void *)p;

	if (!priv->tuner_h->tuner_ops) {
		stm_fe_free((void **) &p);
		stm_fe_free((void **) &priv->tuner_h->pRegMapImage);
		pr_err("%s: failed due to NULL tuner_ops\n", __func__);
		return -EFAULT;
	}

	ops = priv->tuner_h->tuner_ops;

	ops->init = STV6111_TunerInit;
	ops->set_referenceFreq = STV6111_TunerSetReferenceFreq;
	ops->set_gain = STV6111_TunerSetGain;
	ops->set_frequency = STV6111_TunerSetFrequency;
	ops->set_bandWidth = STV6111_TunerSetBandwidth;
	ops->set_standby = STV6111_TunerSetStandby;
	ops->get_status = STV6111_TunerGetStatus;
	ops->get_frequency = STV6111_TunerGetFrequency;
	ops->term = STV6111_TunerTerm;
	ops->detach = stm_fe_stv6111_detach;

	return 0;
}
EXPORT_SYMBOL(stm_fe_stv6111_attach);

/*
 * Name: stm_fe_stv6111_detach()
 *
 * Description: uninstalled the function pointers for tuner LLA
 *
 */

int stm_fe_stv6111_detach(STCHIP_Handle_t tuner_h)
{
	struct tuner_ops_s *ops = tuner_h->tuner_ops;
	SAT_TUNER_Params_t p;
	p = (SAT_TUNER_Params_t)(tuner_h->pData);

	if (ops) {
		ops->detach = NULL;
		ops->term = NULL;
		ops->get_frequency = NULL;
		ops->get_status = NULL;
		ops->set_standby = NULL;
		ops->set_bandWidth = NULL;
		ops->set_frequency = NULL;
		ops->set_gain = NULL;
		ops->set_referenceFreq = NULL;
		ops->init = NULL;
	}

	stm_fe_free((void **) &p);
	stm_fe_free((void **) &tuner_h->pRegMapImage);

	return 0;
}
EXPORT_SYMBOL(stm_fe_stv6111_detach);

static int32_t __init stv6111_lla_init(void)
{
	pr_info("Loading stv6111 tuner module ...\n");

	return 0;
}
module_init(stv6111_lla_init);

static void __exit stv6111_lla_term(void)
{
	pr_info("Removing stv6111 tuner module ...\n");

}
module_exit(stv6111_lla_term);

MODULE_DESCRIPTION("Low level driver for STV6111 satellite tuner");
MODULE_AUTHOR("STMicroelectronics Limited");
MODULE_LICENSE("GPL");
