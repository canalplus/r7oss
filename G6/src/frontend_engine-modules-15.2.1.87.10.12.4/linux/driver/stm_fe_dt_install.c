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

Source file name : stm_fe_dt_install.c
Author :          Shobhit

attach and detach the device lla in device tree model

Date        Modification                                    Name
----        ------------                                    --------
6-Jun-13   Created                                         Shobhit
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
#include <d0mxl584.h>
#include <d0mxl582.h>
#include <d0mxl214.h>
#include <d0dummy_demod.h>
#include <d0remote.h>
#include <dsilabs.h>
#include <tsclient.h>
#include <stv6111_install.h>
#include <stv6110_install.h>
#include <stv6120_install.h>
#include <fe_tc_tuner.h>
#include <lnbh24.h>
#include <lnbh26.h>
#include <diseqc_stv090x.h>
#include <diseqc_stv0910.h>
#include <diseqc_stv0913.h>
#include <diseqc_stv0900.h>
#include <diseqc_mxl582.h>
#include <rf_matrix_stv6120.h>
#include "stm_fe_custom.h"

extern int stm_fe_native;
#define LLA_NAME_DT(x) (#x)

int check_device_name_dt(char *input_name, char *driver_name)
{
	int driver_len = strlen(driver_name);
	int input_len = strlen(input_name);
	int check_len = input_len;

	if (driver_len > check_len)
		check_len = driver_len;

	if (!strncmp(input_name, driver_name, check_len))
		return TRUE;
	else
		return FALSE;

}

int device_name_match_dt(char *input_name, char *driver_name)
{
	int len = strlen(driver_name);

	if (!strncmp(input_name, driver_name, len))
		return TRUE;
	else
		return FALSE;

}

int stm_fe_demod_match_dt(struct stm_fe_demod_s *priv, char *name)
{
	int ret = -ENODEV;

	if (check_device_name_dt(name, LLA_NAME_DT(STV090x)))
		ret = stm_fe_external_attach(stm_fe_stv0903_attach, priv);
	else if (check_device_name_dt(name, LLA_NAME_DT(STV0910P1)) ||
			check_device_name_dt(name, LLA_NAME_DT(STV0910P2)))
		ret = stm_fe_external_attach(stm_fe_stv0910_attach, priv);
	else if (check_device_name_dt(name, LLA_NAME_DT(STV0900P1)) ||
			check_device_name_dt(name, LLA_NAME_DT(STV0900P2)))
		ret = stm_fe_external_attach(stm_fe_stv0900_attach, priv);
	else if (check_device_name_dt(name, LLA_NAME_DT(STV0913)))
		ret = stm_fe_external_attach(stm_fe_stv0913_attach, priv);
	else if (check_device_name_dt(name, LLA_NAME_DT(MXL584)))
		ret = stm_fe_external_attach(stm_fe_mxl584_attach, priv);
	else if (check_device_name_dt(name, LLA_NAME_DT(MXL582)))
		ret = stm_fe_external_attach(stm_fe_mxl582_attach, priv);
	else if (check_device_name_dt(name, LLA_NAME_DT(MXL214)))
		ret = stm_fe_external_attach(stm_fe_mxl214_attach, priv);
	/* TCH_FIX - Begin */
	else if (check_device_name_dt(name, LLA_NAME_DT(SILABS)))
		ret = stm_fe_external_attach(stm_fe_silabs_attach, priv);
	/* TCH_FIX - End */
	else if (check_device_name_dt(name, LLA_NAME_DT(STV0367)) ||
			check_device_name_dt(name, LLA_NAME_DT(STV0367T)) ||
			 check_device_name_dt(name, LLA_NAME_DT(STV0367C)))
		ret = stm_fe_external_attach(stm_fe_stv0367_attach, priv);
	else if (check_device_name_dt(name, LLA_NAME_DT(SASC1_REMOTE)) ||
		     check_device_name_dt(name, LLA_NAME_DT(REMOTE)) ||
		     check_device_name_dt(name, LLA_NAME_DT(REMOTE_ANNEX_B)) ||
		     check_device_name_dt(name, LLA_NAME_DT(REMOTE_DVB_C)))
		ret = stm_fe_external_attach(stm_fe_remote_attach, priv);
	else if (check_device_name_dt(name, LLA_NAME_DT(DUMMY)))
		ret = stm_fe_external_attach(stm_fe_dummy_demod_attach, priv);

	return ret;
}

int stm_fe_tuner_match_dt(struct stm_fe_demod_s *priv, char *name)
{
	int ret = -ENODEV;

	if (check_device_name_dt(name, LLA_NAME_DT(STV6110)) ||
			check_device_name_dt(name, LLA_NAME_DT(STV6110_1)) ||
			 check_device_name_dt(name, LLA_NAME_DT(STV6110_2)))
		ret = stm_fe_external_attach(stm_fe_stv6110_attach, priv);
	else if (check_device_name_dt(name, LLA_NAME_DT(STV6120_1)) ||
			check_device_name_dt(name, LLA_NAME_DT(STV6120_2)))
		ret = stm_fe_external_attach(stm_fe_stv6120_attach, priv);
	else if (check_device_name_dt(name, LLA_NAME_DT(STV6111)))
		ret = stm_fe_external_attach(stm_fe_stv6111_attach, priv);
	else if (check_device_name_dt(name, LLA_NAME_DT(STV4100)))
		ret = stm_fe_external_attach(stm_fe_stv4100_attach, priv);
	else if (check_device_name_dt(name, LLA_NAME_DT(TDA18212)))
		ret = stm_fe_external_attach(stm_fe_tda18212_attach, priv);
	else if (check_device_name_dt(name, LLA_NAME_DT(DTT7546)))
		ret = stm_fe_external_attach(stm_fe_dtt7546_attach, priv);

	return ret;
}

int stm_fe_lnb_match_dt(struct stm_fe_lnb_s *priv, char *name)
{
	int ret = -ENODEV;

	if (check_device_name_dt(name, LLA_NAME_DT(LNBH24)))
		ret = stm_fe_external_attach(stm_fe_lnbh24_attach, priv);
	else if (check_device_name_dt(name, LLA_NAME_DT(LNBH26P1)))
		ret = stm_fe_external_attach(stm_fe_lnbh26_attach, priv);
	else if (check_device_name_dt(name, LLA_NAME_DT(LNBH26P2)))
		ret = stm_fe_external_attach(stm_fe_lnbh26_attach, priv);

	return ret;
}

int stm_fe_diseqc_match_dt(struct stm_fe_diseqc_s *priv, char *name)
{
	int ret = -ENODEV;

	if (check_device_name_dt(name, LLA_NAME_DT(DISEQC_STV090x)))
		ret = stm_fe_external_attach(stm_fe_diseqc_stv090x_attach,
								priv);
	else if (check_device_name_dt(name, LLA_NAME_DT(DISEQC_STV0910P1)) ||
		 check_device_name_dt(name, LLA_NAME_DT(DISEQC_STV0910P2)))
		ret = stm_fe_external_attach(stm_fe_diseqc_stv0910_attach,
								priv);
	else if (check_device_name_dt(name, LLA_NAME_DT(DISEQC_STV0900P1)) ||
		  check_device_name_dt(name, LLA_NAME_DT(DISEQC_STV0900P2)))
		ret = stm_fe_external_attach(stm_fe_diseqc_stv0900_attach,
								priv);
	else if (check_device_name_dt(name, LLA_NAME_DT(DISEQC_STV0913)))
		ret = stm_fe_external_attach(stm_fe_diseqc_stv0913_attach,
								priv);
	else if (check_device_name_dt(name, LLA_NAME_DT(DISEQC_MXL582P1)) ||
		check_device_name_dt(name, LLA_NAME_DT(DISEQC_MXL582P2)))
		ret = stm_fe_external_attach(stm_fe_diseqc_mxl582_attach,
							priv);
	return ret;
}

int stm_fe_rf_matrix_match_dt(struct stm_fe_rf_matrix_s *priv, char *name)
{
	int ret = -ENODEV;

	if (check_device_name_dt(name, LLA_NAME_DT(RF_MATRIX_STV6120P1)) ||
		   check_device_name_dt(name, LLA_NAME_DT(RF_MATRIX_STV6120P2)))
		ret = stm_fe_external_attach(stm_fe_rf_matrix_stv6120_attach,
									  priv);

	return ret;
}

int stm_fe_custom_demod_match_dt(struct stm_fe_demod_s *priv,
			      bool *custom_fe)
{
	int ret = -1;
	struct demod_interface_s demod_interface;
	uint32_t fe_instance;

	memset(&demod_interface, 0, sizeof(struct demod_interface_s));
	fe_instance = priv->demod_id;
	demod_interface.ops = priv->ops;
	ret = stm_fe_external_attach(stm_fe_custom_demod_attach,
				     &fe_instance, &demod_interface, NULL);
	if (!ret) {
		*custom_fe = true;
		priv->config->ts_out = demod_interface.ts_out;
		priv->config->demux_tsin_id = demod_interface.demux_tsin_id;
		priv->config->ts_tag = demod_interface.ts_tag;
		priv->config->ts_clock = demod_interface.ts_clock;
		pr_info("%s: custom demod attached\n", __func__);
	}

	return ret;
}

int stm_fe_custom_tuner_match_dt(struct stm_fe_demod_s *priv,
			      bool *custom_fe)
{
	int ret = -1;
	struct tuner_interface_s tuner_interface;
	uint32_t fe_instance;

	fe_instance = priv->demod_id;
	tuner_interface.ops = priv->tuner_h->tuner_ops;
	ret = stm_fe_external_attach(stm_fe_custom_tuner_attach,
				     &fe_instance, &tuner_interface, NULL);
	if (!ret) {
		*custom_fe = true;
		pr_info("%s: custom tuner attached\n", __func__);
	}

	return ret;
}

int stm_fe_custom_lnb_match_dt(struct stm_fe_lnb_s *priv, bool *custom_fe)
{
	int ret = -1;
	struct lnb_interface_s lnb_interface;
	uint32_t lnb_instance;

	lnb_instance = priv->lnb_id;
	lnb_interface.ops = priv->ops;
	ret = stm_fe_external_attach(stm_fe_custom_lnb_attach,
				     &lnb_instance, &lnb_interface, NULL);
	if (!ret) {
		*custom_fe = true;
		pr_info("%s: custom lnb attached\n", __func__);
	}

	return ret;
}

int stm_fe_custom_diseqc_match_dt(struct stm_fe_diseqc_s *priv,
			       bool *custom_fe)
{
	int ret = -1;
	struct diseqc_interface_s diseqc_interface;
	uint32_t diseqc_instance;

	diseqc_instance = priv->diseqc_id;
	diseqc_interface.ops = priv->ops;
	ret = stm_fe_external_attach(stm_fe_custom_diseqc_attach,
				     &diseqc_instance, &diseqc_interface, NULL);
	if (!ret) {
		*custom_fe = true;
		pr_info("%s: custom diseqc attached\n", __func__);
	}

	return ret;
}

int stm_fe_custom_rf_matrix_match_dt(struct stm_fe_rf_matrix_s *priv,
								bool *custom_fe)
{
	int ret = -1;
	struct rf_matrix_interface_s interface;
	uint32_t inst;

	inst = priv->id;
	interface.ops = priv->ops;
	ret = stm_fe_external_attach(stm_fe_custom_rf_matrix_attach, &inst,
							      &interface, NULL);
	if (!ret) {
		*custom_fe = true;
		pr_info("%s: custom rf_matrix attached\n", __func__);
	}

	return ret;
}

/*
 * Name: tuner_install_dt()
 *
 * Description: low level function pointer installation according to tuner type
 *
 */
int tuner_install_dt(struct stm_fe_demod_s *priv)
{
	int ret = 0;
	char *name = priv->config->tuner_name;
	bool custom_fe = false;

	if (!strlen(name))
		return 0;

	if (STM_FE_BYPASS_LLA_DRIVERS == stm_fe_native) {
		pr_info("%s: skipping, to be done by adapt layer\n", __func__);
		return ret;
	} else if ((STM_FE_USE_LLA_DRIVERS == stm_fe_native) ||
			(priv->config->ctrl == STM_FE)) {
		switch (priv->config->tuner_lla_sel) {
		case DEMOD_LLA_FORCED:
			if (!strncmp(priv->config->tuner_lla, "custom,lla",
						sizeof("custom,lla")))
				ret = stm_fe_custom_tuner_match_dt(priv,
						&custom_fe);
			else
			    if (!strncmp(priv->config->tuner_lla, "st,lla",
						    sizeof("st,lla")))
				ret = stm_fe_tuner_match_dt(priv, name);
			break;
		case DEMOD_LLA_AUTO:
			ret = stm_fe_custom_tuner_match_dt(priv, &custom_fe);
			if (!custom_fe)
				ret = stm_fe_tuner_match_dt(priv, name);
			break;
		default:
			pr_err("%s: Tuner lla is not selected\n", __func__);
			break;
		}
	} else if (priv->config->ctrl == KERNEL_FE) {
		pr_info("%s: skipping, to be done by adapt layer\n", __func__);
	}

	if (ret)
		pr_err("%s: %s Attach failed err: %d\n", __func__, name, ret);

	return ret;
}

/*
 * Name: demod_install_dt()
 *
 * Description: low level function pointer installation according to demod type
 *
 */
int demod_install_dt(struct stm_fe_demod_s *priv)
{
	int ret = -ENODEV;
	char *name = priv->config->demod_name;
	bool custom_fe = false;

	if (STM_FE_BYPASS_LLA_DRIVERS == stm_fe_native) {
		pr_info("%s: skipping, to be done by adapt layer\n", __func__);
		priv->bypass_control = true;
		ret = 0;
		goto end;
	} else if ((STM_FE_USE_LLA_DRIVERS == stm_fe_native) ||
			(priv->config->ctrl == STM_FE)) {
		switch (priv->config->demod_lla_sel) {
		case DEMOD_LLA_FORCED:
			if (!strncmp(priv->config->demod_lla, "custom,lla",
						sizeof("custom,lla")))
				ret = stm_fe_custom_demod_match_dt(priv,
						&custom_fe);
			else
			    if (!strncmp(priv->config->demod_lla, "st,lla",
						    sizeof("st,lla")))
				ret = stm_fe_demod_match_dt(priv, name);
			break;
		case DEMOD_LLA_AUTO:
			ret = stm_fe_custom_demod_match_dt(priv, &custom_fe);
			if (!custom_fe)
				ret = stm_fe_demod_match_dt(priv, name);
			break;
		default:
			pr_err("%s: Demod lla is not selected\n", __func__);
			break;
		}
	} else if (priv->config->ctrl == KERNEL_FE) {
		pr_info("%s: skipping, to be done by adapt layer\n", __func__);
		priv->bypass_control = true;
		ret = 0;
	}
end:
	if (ret)
		pr_err("%s: %s Attach failed err: %d\n", __func__, name, ret);

	return ret;
}

/*
 * Name: lnb_install_dt()
 *
 * Description: low level function pointer installation according to demod type
 *
 */
int lnb_install_dt(struct stm_fe_lnb_s *priv)
{
	int ret = -ENODEV;
	char *name = priv->config->lnb_name;
	bool custom_fe = false;

	if (STM_FE_BYPASS_LLA_DRIVERS == stm_fe_native) {
		pr_info("%s: skipping, to be done by adapt layer\n", __func__);
		priv->bypass_control = true;
		return 0;
	} else if ((STM_FE_USE_LLA_DRIVERS == stm_fe_native) ||
				(priv->config->ctrl == STM_FE)) {
		switch (priv->config->lla_sel) {
		case LNB_LLA_FORCED:
			if (!strncmp(priv->config->lla, "custom,lla",
						sizeof("custom,lla")))
				ret = stm_fe_custom_lnb_match_dt(priv,
						&custom_fe);
			else
			    if (!strncmp(priv->config->lla, "st,lla",
						    sizeof("st,lla")))
				ret = stm_fe_lnb_match_dt(priv, name);
			break;
		case LNB_LLA_AUTO:
			ret = stm_fe_custom_lnb_match_dt(priv, &custom_fe);
			if (!custom_fe)
				ret = stm_fe_lnb_match_dt(priv, name);
			break;
		default:
			pr_err("%s: lnb lla is not selected\n", __func__);
			break;
		}
	} else if (priv->config->ctrl == KERNEL_FE) {
		pr_info("%s: skipping, to be done by adapt layer\n", __func__);
		priv->bypass_control = true;
		ret = 0;
	}

	if (ret)
		pr_err("%s: %s Attach failed err: %d\n", __func__, name, ret);

	return ret;
}

/*
 * Name: diseqc_install_dt()
 *
 * Description: low level function pointer installation according to demod type
 *
 */
int diseqc_install_dt(struct stm_fe_diseqc_s *priv)
{
	int ret = -ENODEV;
	char *name = priv->config->diseqc_name;
	bool custom_fe = false;

	if (STM_FE_BYPASS_LLA_DRIVERS == stm_fe_native) {
		pr_info("%s: skipping, to be done by adapt layer\n", __func__);
		priv->bypass_control = true;
		return 0;
	} else if ((STM_FE_USE_LLA_DRIVERS == stm_fe_native) ||
				(priv->config->ctrl == STM_FE)) {
		switch (priv->config->lla_sel) {
		case DISEQC_LLA_FORCED:
			if (!strncmp(priv->config->lla, "custom,lla",
						sizeof("custom,lla")))
				ret = stm_fe_custom_diseqc_match_dt(priv,
						&custom_fe);
			else if (!strncmp(priv->config->lla, "st,lla",
						sizeof("st,lla")))
				ret = stm_fe_diseqc_match_dt(priv, name);
			break;
		case DISEQC_LLA_AUTO:
			ret = stm_fe_custom_diseqc_match_dt(priv, &custom_fe);
			if (!custom_fe) {
				ret = 0;
				ret = stm_fe_diseqc_match_dt(priv, name);
			}
			break;
		default:
			pr_err("%s: diseqc lla is not selected\n", __func__);
			break;
		}
	} else if (priv->config->ctrl == KERNEL_FE) {
		pr_info("%s: skipping, to be done by adapt layer\n", __func__);
		priv->bypass_control = true;
		ret = 0;
	}

	if (ret)
		pr_err("%s: %s Attach failed err: %d\n", __func__, name, ret);

	return ret;
}

/*
 * Name: rf_matrix_install_dt()
 *
 * Description: low level function pointer installation according to demod type
 *
 */
int rf_matrix_install_dt(struct stm_fe_rf_matrix_s *priv)
{
	int ret = -ENODEV;
	char *name = priv->config->name;
	bool custom_fe = false;

	if (STM_FE_BYPASS_LLA_DRIVERS == stm_fe_native) {
		pr_info("%s: skipping, to be done by adapt layer\n", __func__);
		priv->bypass_control = true;
		return 0;
	} else if ((STM_FE_USE_LLA_DRIVERS == stm_fe_native) ||
				(priv->config->ctrl == STM_FE)) {
		switch (priv->config->lla_sel) {
		case RF_MATRIX_LLA_FORCED:
			if (!strncmp(priv->config->lla, "custom,lla",
						sizeof("custom,lla")))
				ret = stm_fe_custom_rf_matrix_match_dt(priv,
							&custom_fe);
			else
			    if (!strncmp(priv->config->lla, "st,lla",
						    sizeof("st,lla")))
				ret = stm_fe_rf_matrix_match_dt(priv, name);
			break;
		case RF_MATRIX_LLA_AUTO:
			ret = stm_fe_custom_rf_matrix_match_dt(priv,
					&custom_fe);
			if (!custom_fe) {
				ret = 0;
				ret = stm_fe_rf_matrix_match_dt(priv, name);
			}
			break;
		default:
			pr_err("%s: rf_matrix lla is not selected\n", __func__);
			break;
		}
	} else if (priv->config->ctrl == KERNEL_FE) {
		pr_info("%s: skipping, to be done by adapt layer\n", __func__);
		priv->bypass_control = true;
		ret = 0;
	}

	if (ret)
		pr_err("%s: %s Attach failed err: %d\n", __func__, name, ret);

	return ret;
}

/*
 * Name: tuner_uninstall_dt()
 *
 * Description: uninstall the wrapper function pointer
 *
 */
int tuner_uninstall_dt(struct stm_fe_demod_s *priv)
{
	int ret = 0;
	struct tuner_ops_s *tuner_ops = priv->tuner_h->tuner_ops;

	if (tuner_ops->detach)
		tuner_ops->detach(priv->tuner_h);
	else {
		pr_err("%s: FP tuner_ops->tuner_detach is NULL\n", __func__);
		return -EFAULT;
	}


	return ret;
}

/*
 * Name: demod_uninstall_dt()
 *
 * Description: uninstall the wrapper function pointer for demod
 *
 */
int demod_uninstall_dt(struct stm_fe_demod_s *priv)
{
	int ret = 0;
	if (priv->ops->detach)
		priv->ops->detach(priv);
	else {
		pr_err("%s: FP demod_priv->ops->detach is NULL\n", __func__);
		return -EFAULT;
	}

	return ret;
}

/*
 * Name: lnb_uninstall_dt()
 *
 * Description: uninstall the wrapper function pointer for lnb
 *
 */
int lnb_uninstall_dt(struct stm_fe_lnb_s *priv)
{
	int ret = 0;
	if (priv->ops->detach)
		priv->ops->detach(priv);
	else {
		pr_err("%s: FP lnb_ops->detach is NULL\n", __func__);
		return -EFAULT;
	}

	return ret;
}

/*
 * Name: diseqc_uninstall_dt()
 *
 * Description: uninstall the wrapper function pointer for diseqc
 *
 */
int diseqc_uninstall_dt(struct stm_fe_diseqc_s *priv)
{
	int ret = 0;
	if (priv->ops->detach)
		priv->ops->detach(priv);
	else {
		pr_err("%s: FP diseqc_ops->detach is NULL\n", __func__);
		return -EFAULT;
	}

	return ret;
}

/*
 * Name: rf_matrix_uninstall_dt()
 *
 * Description: uninstall the wrapper function pointer for rf_matrix
 *
 */
int rf_matrix_uninstall_dt(struct stm_fe_rf_matrix_s *priv)
{
	int ret = 0;
	if (priv->ops->detach)
		priv->ops->detach(priv);
	else {
		pr_err("%s: FP rf_matrix_ops->detach is NULL\n", __func__);
		return -EFAULT;
	}

	return ret;
}
