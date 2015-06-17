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

Source file name : stm_fe_install.c
Author :          Shobhit

attach and detach the device lla

Date        Modification                                    Name
----        ------------                                    --------
16-Jun-11   Created                                         Shobhit
27-Sep-11   Added support for DTT7546 tuner                 Ankur
21-May-11   Added support for stv0900 demod                     Ankur
************************************************************************/
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/list.h>
#include <linux/platform_device.h>
#include <linux/stm/plat_dev.h>
#include <linux/stm/demod.h>
#include <linux/stm/lnb.h>
#include <linux/stm/diseqc.h>
#include <linux/stm/rf_matrix.h>
#include <stm_registry.h>
#include <stm_fe.h>
#include <stfe_utilities.h>
#include <stm_fe_os.h>
#include <i2c_wrapper.h>
#include <tuner_wrapper.h>
#include "stm_fe_demod.h"
#include "stm_fe_lnb.h"
#include "stm_fe_diseqc.h"
#include "stm_fe_rf_matrix.h"
#include "stm_fe_install.h"
#include <fesat_commlla_str.h>
#include <fecab_commlla_str.h>
#include <feter_commlla_str.h>
#include <d0900.h>
#include <d0903.h>
#include <d0910.h>
#include <d0913.h>
#include <d0367.h>
#include <d0dummy_demod.h>
#include <d0remote.h>
#include <tsclient.h>
#include <stv6110_install.h>
#include <stv6111_install.h>
#include <stv6120_install.h>
#include <fe_tc_tuner.h>
#include <lnbh24.h>
#include <lnbh26.h>
#include <diseqc_stv090x.h>
#include <diseqc_stv0910.h>
#include <diseqc_stv0913.h>
#include <diseqc_mxl582.h>
#include <diseqc_stv0900.h>
#include <rf_matrix_stv6120.h>
#include "stm_fe_custom.h"

int stm_fe_native = STM_FE_NO_OPTIONS_DEFAULT;
module_param(stm_fe_native, int, 0660);

#define POST_FIX "-STM_FE"
#define LLA_NAME(x) (#x  POST_FIX)

int check_device_name(char *input_name, char *driver_name, int full_check)
{
	int driver_len = strlen(driver_name);
	int input_len = strlen(input_name);
	int check_len = input_len;

	/* either check based on the length of the largest string
	 * of if requested remove the STM_FE postfix len and test that remaider
	 * only */

	if (full_check) {
		if (driver_len > check_len)
			check_len = driver_len;
	} else {
		int offset = driver_len - strlen(POST_FIX);
		if (offset > 0)
			check_len = offset;
	}

	if (!strncmp(input_name, driver_name, check_len))
		return TRUE;
	else
		return FALSE;
}

/*
 * Name: tuner_install()
 *
 * Description: low level function pointer installation according to tuner type
 *
 */
int tuner_install(struct stm_fe_demod_s *priv)
{
	int ret = 0;
	int check;
	char *name = priv->config->tuner_name;
	struct tuner_interface_s tuner_interface;
	uint32_t fe_instance;
	bool custom_fe = false;

	if (!strlen(name))
		return 0;

	if (STM_FE_BYPASS_LLA_DRIVERS == stm_fe_native) {
		pr_info("%s: skipping, to be done by adapt layer\n", __func__);
		return 0;
	}

	if (STM_FE_USE_LLA_DRIVERS == stm_fe_native) {
		check = FALSE;
	} else {
		check = TRUE;

		fe_instance = priv->demod_id;
		tuner_interface.ops = priv->tuner_h->tuner_ops;
		ret = stm_fe_external_attach(stm_fe_custom_tuner_attach,
					&fe_instance, &tuner_interface, NULL);
		if (!ret) {
			custom_fe = true;
			pr_info("%s: Attached custom tuner\n", __func__);
		}
	}

	if (!custom_fe) {
		ret = -ENODEV;
		if (check_device_name(name, LLA_NAME(STV6110), check) ||
			check_device_name(name,	LLA_NAME(STV6110_1), check) ||
			check_device_name(name,	LLA_NAME(STV6110_2), check))
				ret = stm_fe_external_attach(
					stm_fe_stv6110_attach, priv);
		else if (check_device_name(name, LLA_NAME(STV6120_1), check) ||
			 check_device_name(name, LLA_NAME(STV6120_2), check))
				ret = stm_fe_external_attach(
					stm_fe_stv6120_attach, priv);
		else if (check_device_name(name, LLA_NAME(STV6111), check))
			ret = stm_fe_external_attach(
					stm_fe_stv6111_attach, priv);
		else if (check_device_name(name, LLA_NAME(STV4100), check))
				ret = stm_fe_external_attach(
					stm_fe_stv4100_attach, priv);
		else if (check_device_name(name, LLA_NAME(TDA18212), check))
				ret = stm_fe_external_attach(
					stm_fe_tda18212_attach, priv);
		else if (check_device_name(name, LLA_NAME(DTT7546), check))
				ret = stm_fe_external_attach(
					stm_fe_dtt7546_attach, priv);
	}

	if (ret) {
		pr_err("%s: %s Attach failed err: %d\n", __func__,
				name, ret);
	}

	return ret;
}




/*
 * Name: tuner_uninstall()
 *
 * Description: uninstall the wrapper function pointer
 *
 */
int tuner_uninstall(struct stm_fe_demod_s *priv)
{
	int ret = 0;
	struct tuner_ops_s *tuner_ops = priv->tuner_h->tuner_ops;

	if (!tuner_ops->detach) {
		pr_err("%s: FP tuner_ops->detach is NULL\n", __func__);
		return -EFAULT;
	}

	tuner_ops->detach(priv->tuner_h);

	return ret;
}

/*
 * Name: demod_install()
 *
 * Description: low level function pointer installation according to demod type
 *
 */
int demod_install(struct stm_fe_demod_s *priv)
{
	int ret = -ENODEV;
	int check;
	char *name = priv->config->demod_name;
	int offset = strlen(name) - strlen(POST_FIX);
	struct demod_interface_s demod_interface;
	uint32_t fe_instance;
	bool custom_fe = false;

	memset(&demod_interface, '\0', sizeof(struct demod_interface_s));

	if (STM_FE_BYPASS_LLA_DRIVERS == stm_fe_native) {
		pr_info("%s: skipping, to be done by adapt layer\n", __func__);
		priv->bypass_control = true;
		return 0;
	}

	if (STM_FE_USE_LLA_DRIVERS == stm_fe_native) {
		check = FALSE;
	} else {
		check = TRUE;

		fe_instance = priv->demod_id;
		demod_interface.ops = priv->ops;
		ret = stm_fe_external_attach(stm_fe_custom_demod_attach,
				&fe_instance, &demod_interface, NULL);
		if (!ret) {
			custom_fe = true;
			priv->config->ts_out = demod_interface.ts_out;
			priv->config->demux_tsin_id =
						demod_interface.demux_tsin_id;
			priv->config->ts_tag = demod_interface.ts_tag;
			priv->config->ts_clock = demod_interface.ts_clock;
			pr_info("%s: Attached custom demod\n", __func__);
		}
	}

	if (!custom_fe) {
		ret = -ENODEV;
		if (check_device_name(name, LLA_NAME(STV090x), check))
			ret = stm_fe_external_attach(stm_fe_stv0903_attach,
									priv);
		else if (check_device_name(name, LLA_NAME(STV0910P1), check) ||
			 check_device_name(name, LLA_NAME(STV0910P2), check))
			ret = stm_fe_external_attach(stm_fe_stv0910_attach,
									priv);
		else if (check_device_name(name, LLA_NAME(STV0913), check))
			ret = stm_fe_external_attach(stm_fe_stv0913_attach,
									priv);
		else if (check_device_name(name, LLA_NAME(STV0900P1), check) ||
			 check_device_name(name, LLA_NAME(STV0900P2), check))
			ret = stm_fe_external_attach(stm_fe_stv0900_attach,
									priv);
		else if (check_device_name(name, LLA_NAME(STV0367), check) ||
			 check_device_name(name, LLA_NAME(STV0367T), check) ||
			 check_device_name(name, LLA_NAME(STV0367C), check))
			ret = stm_fe_external_attach(stm_fe_stv0367_attach,
									priv);
		else if (check_device_name(name, LLA_NAME(SASC1_REMOTE), check))
			ret = stm_fe_external_attach(stm_fe_remote_attach,
									priv);
		else if (check_device_name(name, LLA_NAME(DUMMY), check))
			ret = stm_fe_external_attach(stm_fe_dummy_demod_attach,
									priv);
	}
	if (ret) {
		if (!check ||
		    (offset && !(strcmp((name + offset), POST_FIX)))) {
			pr_err("%s: %s Attach failed err = %d\n", __func__,
								     name, ret);
		} else {
			priv->bypass_control = true;
			ret = 0;
		}
	}

	if (offset > 0 && IS_NAME_MATCH(&name[offset], POST_FIX))
		name[offset] = '\0';

	return ret;
}

/*
 * Name: demod_uninstall()
 *
 * Description: uninstall the wrapper function pointer for demod
 *
 */
int demod_uninstall(struct stm_fe_demod_s *priv)
{
	int ret = 0;
	if (!priv->ops->detach) {
		pr_err("%s: FP demod_priv->ops->detach is NULL\n", __func__);
		return -EFAULT;
	}

	priv->ops->detach(priv);
	return ret;
}

/*
 * Name: lnb_install()
 *
 * Description: low level function pointer installation according to demod type
 *
 */
int lnb_install(struct stm_fe_lnb_s *priv)
{
	int ret = -ENODEV;
	int check;
	char *name = priv->config->lnb_name;
	struct lnb_interface_s lnb_interface;
	uint32_t lnb_instance;
	bool custom_fe = false;

	if (STM_FE_BYPASS_LLA_DRIVERS == stm_fe_native) {
		pr_info("%s: skipping, to be done by adapt layer\n", __func__);
		priv->bypass_control = true;
		return 0;
	}

	if (STM_FE_USE_LLA_DRIVERS == stm_fe_native) {
		check = FALSE;
	} else {
		check = TRUE;

		lnb_instance = priv->lnb_id;
		lnb_interface.ops = priv->ops;

		ret = stm_fe_external_attach(stm_fe_custom_lnb_attach,
				&lnb_instance, &lnb_interface, NULL);
		if (!ret) {
			custom_fe = true;
			pr_info("%s: Attached custom lnb\n", __func__);
		}
	}

	if (!custom_fe) {
		ret = -ENODEV;
		if (check_device_name(name, LLA_NAME(LNBH24), check))
			ret = stm_fe_external_attach(
					stm_fe_lnbh24_attach, priv);
		else if (check_device_name(name, LLA_NAME(LNBH26P1), check))
			ret = stm_fe_external_attach(
					stm_fe_lnbh26_attach, priv);
		else if (check_device_name(name, LLA_NAME(LNBH26P2), check))
			ret = stm_fe_external_attach(
					stm_fe_lnbh26_attach, priv);
	}
	if (ret) {
		int offset = strlen(name) - strlen(POST_FIX);
		if (!check ||
		    (offset && !(strcmp((name + offset), POST_FIX)))) {
			pr_err("%s: %s Attach failed err: %d\n", __func__,
								     name, ret);
		} else {
			priv->bypass_control = true;
			ret = 0;
		}
	}

	return ret;
}

/*
 * Name: lnb_uninstall()
 *
 * Description: uninstall the wrapper function pointer for lnb
 *
 */
int lnb_uninstall(struct stm_fe_lnb_s *priv)
{
	int ret = 0;
	if (!priv->ops->detach) {
		pr_err("%s: FP lnb_priv->ops->detach is NULL\n", __func__);
		return -EFAULT;
	}
	priv->ops->detach(priv);

	return ret;
}

/*
 * Name: diseqc_install()
 *
 * Description: low level function pointer installation according to demod type
 *
 */
int diseqc_install(struct stm_fe_diseqc_s *priv)
{
	int ret = -ENODEV;
	int check;
	char *name = priv->config->diseqc_name;
	struct diseqc_interface_s diseqc_interface;
	uint32_t diseqc_instance;
	bool custom_fe = false;

	if (STM_FE_BYPASS_LLA_DRIVERS == stm_fe_native) {
		pr_info("%s: skipping, to be done by adapt layer\n", __func__);
		priv->bypass_control = true;
		return 0;
	}

	if (STM_FE_USE_LLA_DRIVERS == stm_fe_native) {
		check = FALSE;
	} else {
		check = TRUE;

		diseqc_instance = priv->diseqc_id;
		diseqc_interface.ops = priv->ops;

		ret = stm_fe_external_attach(stm_fe_custom_diseqc_attach,
				&diseqc_instance, &diseqc_interface, NULL);
		if (!ret) {
			custom_fe = true;
			pr_info("%s: Attached custom diseqc\n", __func__);
		}
	}

	if (!custom_fe) {
		ret = -ENODEV;
		if (check_device_name(name, LLA_NAME(DISEQC_STV090x), check))
			ret = stm_fe_external_attach(
				stm_fe_diseqc_stv090x_attach, priv);
		else if (check_device_name(name,
					   LLA_NAME(DISEQC_STV0910P1), check) ||
			 check_device_name(name,
					   LLA_NAME(DISEQC_STV0910P2), check))
				ret = stm_fe_external_attach(
				    stm_fe_diseqc_stv0910_attach, priv);
		else if (check_device_name(name,
					   LLA_NAME(DISEQC_STV0900P1), check) ||
			 check_device_name(name,
					   LLA_NAME(DISEQC_STV0900P2), check))
				ret = stm_fe_external_attach(
				    stm_fe_diseqc_stv0900_attach, priv);
		else if (check_device_name(name,
					   LLA_NAME(DISEQC_STV0913), check))
			ret = stm_fe_external_attach(
				stm_fe_diseqc_stv0913_attach, priv);
		else if (check_device_name(name,
			   LLA_NAME(DISEQC_MXL582P1), check) ||
			  check_device_name(name,
			   LLA_NAME(DISEQC_MXL582P2), check))
			ret = stm_fe_external_attach(
				stm_fe_diseqc_mxl582_attach, priv);
	}
	if (ret) {
		int offset = strlen(name) - strlen(POST_FIX);
		if (!check ||
		    (offset && !(strcmp((name + offset), POST_FIX)))) {
			pr_err("%s: %s Attach failed err: %d\n", __func__,
								     name, ret);
		} else {
			priv->bypass_control = true;
			ret = 0;
		}
	}

	return ret;
}

/*
 * Name: diseqc_uninstall()
 *
 * Description: uninstall the wrapper function pointer for diseqc
 *
 */
int diseqc_uninstall(struct stm_fe_diseqc_s *priv)
{
	int ret = 0;
	if (!priv->ops->detach) {
		pr_err("%s: FP diseqc_priv->ops->detach is NULL\n", __func__);
		return -EFAULT;
	}
	priv->ops->detach(priv);

	return ret;
}

/*
 * Name: rf_matrix_install()
 *
 * Description: low level function pointer installation acc to rf matrix type
 *
 */
int rf_matrix_install(struct stm_fe_rf_matrix_s *priv)
{
	int ret = -ENODEV;
	int check;
	char *name = priv->config->name;
	struct rf_matrix_interface_s interface;
	uint32_t inst;
	bool custom_fe = false;

	if (STM_FE_BYPASS_LLA_DRIVERS == stm_fe_native) {
		pr_info("%s: skipping, to be done by adapt layer\n", __func__);
		priv->bypass_control = true;
		ret = 0;
	} else {

		if (STM_FE_USE_LLA_DRIVERS == stm_fe_native) {
			check = FALSE;
		} else {
			check = TRUE;

			inst = priv->id;
			interface.ops = priv->ops;

			ret =
			  stm_fe_external_attach(stm_fe_custom_rf_matrix_attach,
						       &inst, &interface, NULL);
			if (!ret) {
				custom_fe = true;
				pr_info("%s: Attached custom module\n",
								      __func__);
			}
		}

		if (!custom_fe) {
			ret = -ENODEV;
			if (check_device_name(name,
					   LLA_NAME(RF_MATRIX_STV06120), check))
				ret = stm_fe_external_attach(
					 stm_fe_rf_matrix_stv6120_attach, priv);
		}
		if (ret) {
			int offset = strlen(name) - strlen(POST_FIX);
			if (!check ||
			    (offset && !(strcmp((name + offset), POST_FIX)))) {
				pr_err("%s: %s attach failed err: %d\n",
							   __func__, name, ret);
			} else {
				priv->bypass_control = true;
				ret = 0;
			}
		}
	}

	return ret;
}

/*
 * Name: rf_matrix_uninstall()
 *
 * Description: uninstall the wrapper function pointer for rf_matrix
 *
 */
int rf_matrix_uninstall(struct stm_fe_rf_matrix_s *priv)
{
	int ret = 0;
	if (priv->ops->detach) {
		priv->ops->detach(priv);
	} else {
		pr_err("%s: FP demod_priv->ops->detach is NULL\n", __func__);
		return -EFAULT;
	}
	return ret;
}
