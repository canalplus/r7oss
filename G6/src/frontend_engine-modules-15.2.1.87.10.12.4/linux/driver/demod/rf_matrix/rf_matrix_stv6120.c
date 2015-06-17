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

Source file name :rf_matrix_stv6120.c
Author :          Ashish Gandhi

Low level function installtion for rf_matrix

Date        Modification                                    Name
----        ------------                                    --------
07-Aug-13   Created                                         Ashish Gandhi

************************************************************************/
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/stm/demod.h>
#include <linux/stm/lnb.h>
#include <linux/stm/rf_matrix.h>
#include <linux/stm/rf_matrix.h>
#include <stm_registry.h>
#include <stm_fe.h>
#include <stfe_utilities.h>
#include <stm_fe_os.h>
#include <i2c_wrapper.h>
#include <stm_fe_demod.h>
#include <stm_fe_lnb.h>
#include <stm_fe_rf_matrix.h>
#include <stm_fe_install.h>
#include "rf_matrix_stv6120.h"
#include <fesat_commlla_str.h>
#include <fe_sat_tuner.h>

static int32_t rf_matrix_stv6120_sel_src(struct stm_fe_rf_matrix_s *priv,
							uint32_t src_num);
static int rf_matrix_stv6120_init(struct stm_fe_rf_matrix_s *priv);
static int rf_matrix_stv6120_term(struct stm_fe_rf_matrix_s *priv);

/*
 * Name: stm_fe_rf_matrix_stv6120_attach()
 *
 * Description: Installs the rf_matrix_stv6120 wrapper fn pointers for LLA
 *
 */
int32_t stm_fe_rf_matrix_stv6120_attach(struct stm_fe_rf_matrix_s *priv)
{
	if (!priv->ops)
		return -EFAULT;

	priv->ops->init = rf_matrix_stv6120_init;
	priv->ops->term = rf_matrix_stv6120_term;
	priv->ops->select_source = rf_matrix_stv6120_sel_src;
	priv->ops->detach = stm_fe_rf_matrix_stv6120_detach;

	return 0;
}
EXPORT_SYMBOL(stm_fe_rf_matrix_stv6120_attach);

/*
 * Name: stm_fe_rf_matrix_stv6120_detach()
 *
 * Description: uninstall the rf_matrix_stv6120 wrapper function
 *
 */
int32_t stm_fe_rf_matrix_stv6120_detach(struct stm_fe_rf_matrix_s *priv)
{
	if (!priv->ops)
		return -EFAULT;

	priv->ops->detach = NULL;
	priv->ops->select_source = NULL;
	priv->ops->term = NULL;
	priv->ops->init = NULL;

	return 0;
}
EXPORT_SYMBOL(stm_fe_rf_matrix_stv6120_detach);

/*
 * Name: rf_matrix_stv6120_init()
 *
 * Description: initialization of rf_matrix
 *
 */
int32_t rf_matrix_stv6120_init(struct stm_fe_rf_matrix_s *priv)
{
	int ret = 0;
	struct stm_fe_demod_s *demod_priv;

	demod_priv = (struct stm_fe_demod_s *)priv->demod_h;

	if (!priv->config) {
		pr_err("%s: Err invalid rf_matrix config\n", __func__);
		return -EFAULT;
	}
	/*
	 * Assign default rf inputs as specified in the platform config
	 */
	ret = FE_Sat_TunerSwitchInput(demod_priv->tuner_h,
					priv->config->input_id);

	return ret;
}

/*
 * Name: rf_matrix_stv6120_sel_src()
 *
 * Description: selects an RF source from an RF matrix switch
 * for input to the tuner
 *
 */
int32_t rf_matrix_stv6120_sel_src(struct stm_fe_rf_matrix_s *priv,
							uint32_t input_id)
{
	TUNER_Error_t ret = 0;
	struct stm_fe_demod_s *demod_priv;

	demod_priv = (struct stm_fe_demod_s *)priv->demod_h;

	ret = FE_Sat_TunerSwitchInput(demod_priv->tuner_h, input_id);

	return ret;
}

/*
 * Name: rf_matrix_stv6120_term()
 *
 * Description: termination of rf_matrix
 *
 */
int rf_matrix_stv6120_term(struct stm_fe_rf_matrix_s *priv)
{
	int ret = 0;

	struct stm_fe_demod_s *demod_priv;

	demod_priv = (struct stm_fe_demod_s *)priv->demod_h;

	if (!priv->config) {
		pr_err("%s: Err invalid rf_matrix config\n", __func__);
		return -EFAULT;
	}
	/*
	 * Assign default rf inputs as specified in the platform config
	 */
	ret = FE_Sat_TunerSwitchInput(demod_priv->tuner_h,
					priv->config->input_id);

	return ret;
}

static int32_t __init stm_fe_rf_matrix_stv6120_init(void)
{
	pr_info("Loading rf_matrix_stv6120 driver module...\n");

	return 0;
}

module_init(stm_fe_rf_matrix_stv6120_init);

static void __exit stm_fe_rf_matrix_stv6120_term(void)
{
	pr_info("Removing rf_matrix_stv6120 driver module ...\n");
}

module_exit(stm_fe_rf_matrix_stv6120_term);

MODULE_DESCRIPTION("Low level driver for STV6120 rf_matrix controller");
MODULE_AUTHOR("STMicroelectronics Limited");
MODULE_LICENSE("GPL");
