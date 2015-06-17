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

Source file name : stm_fe_engine.c
Author :           Rahul.V

stm_fe engine initialisation and discovery functions

Date        Modification                                    Name
----        ------------                                    --------
16-Jun-11   Created                                         Rahul.V

************************************************************************/
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/list.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#ifdef CONFIG_OF
#include <linux/of.h>
#endif
#include <linux/stm/plat_dev.h>
#include <linux/stm/demod.h>
#include <linux/stm/lnb.h>
#include <linux/stm/diseqc.h>
#include <linux/stm/rf_matrix.h>
#include <linux/stm/ip.h>
#include <stm_registry.h>
#include <stm_fe.h>
#include <stfe_utilities.h>
#include <stm_fe_os.h>
#include <i2c_wrapper.h>
#include "demod_caps.h"
#include "diseqc_caps.h"
#include "rf_matrix_caps.h"
#include "ip_caps.h"
#include "ipfec_caps.h"
#include "stm_fe_demod.h"
#include "stm_fe_lnb.h"
#include "stm_fe_diseqc.h"
#include "stm_fe_rf_matrix.h"
#include "stm_fe_ip.h"
#include "stm_fe_ipfec.h"
#include "stm_fe_engine.h"
#include "stm_fe_version.h"
#include <debugfs.h>

#ifdef CONFIG_PM
static const struct dev_pm_ops stm_fe_demod_pm_ops = {
#ifdef CONFIG_PM_RUNTIME
	.runtime_suspend = stm_fe_demod_rt_suspend,
	.runtime_resume = stm_fe_demod_rt_resume,
#endif
	.suspend = stm_fe_demod_suspend,
	.resume = stm_fe_demod_resume,
#ifndef CONFIG_ARCH_STI
	.freeze = stm_fe_demod_freeze,
	.restore = stm_fe_demod_restore,
#endif
};

static const struct dev_pm_ops stm_fe_lnb_pm_ops = {
#ifdef CONFIG_PM_RUNTIME
	.runtime_suspend = stm_fe_lnb_suspend,
	.runtime_resume = stm_fe_lnb_resume,
#endif
	.suspend = stm_fe_lnb_suspend,
	.resume = stm_fe_lnb_resume,
#ifndef CONFIG_ARCH_STI
	.freeze = stm_fe_lnb_freeze,
	.restore = stm_fe_lnb_restore,
#endif
};

static const struct dev_pm_ops stm_fe_diseqc_pm_ops = {
#ifdef CONFIG_PM_RUNTIME
	.runtime_suspend = stm_fe_diseqc_suspend,
	.runtime_resume = stm_fe_diseqc_resume,
#endif
	.suspend = stm_fe_diseqc_suspend,
	.resume = stm_fe_diseqc_resume,
#ifndef CONFIG_ARCH_STI
	.freeze = stm_fe_diseqc_freeze,
	.restore = stm_fe_diseqc_restore,
#endif
};

static const struct dev_pm_ops stm_fe_rf_matrix_pm_ops = {
#ifdef CONFIG_PM_RUNTIME
	.runtime_suspend = stm_fe_rf_matrix_suspend,
	.runtime_resume = stm_fe_rf_matrix_resume,
#endif
	.suspend = stm_fe_rf_matrix_suspend,
	.resume = stm_fe_rf_matrix_resume,
#ifndef CONFIG_ARCH_STI
	.freeze = stm_fe_rf_matrix_freeze,
	.restore = stm_fe_rf_matrix_restore,
#endif
};
#endif
#ifdef CONFIG_OF
static struct of_device_id stm_demod_match[] = {
	{
		.compatible = "st,demod",
	},
		{},
};

static struct of_device_id stm_lnb_match[] = {
	{
		.compatible = "st,lnb",
	},
		{},
};

static struct of_device_id stm_diseqc_match[] = {
	{
		.compatible = "st,diseqc",
	},
		{},
};

static struct of_device_id stm_rf_matrix_match[] = {
	{
		.compatible = "st,rf_matrix",
	},
		{},
};
#endif

static void get_demod_caps(stm_fe_demod_caps_t *demod_caps, char *demod_name);
static void get_ip_caps(stm_fe_ip_caps_t *ip_fe_caps, char *ip_name);
static void get_ipfec_caps(stm_fe_ip_fec_caps_t *ipfec_fe_caps,
							char *ipfec_name);
static void get_diseqc_caps(stm_fe_diseqc_caps_t *diseqc_caps,
							char *diseqc_name);
static void get_rf_matrix_caps(stm_fe_rf_matrix_caps_t *rf_matrix_caps,
					struct stm_fe_rf_matrix_s *priv);
static struct platform_driver stm_fe_demod_driver = {
	.driver = {
		   .name = STM_FE_DEMOD_NAME,
		   .owner = THIS_MODULE,
#ifdef CONFIG_PM
		   .pm = &stm_fe_demod_pm_ops,
#endif
#ifdef CONFIG_OF
		   .of_match_table = of_match_ptr(stm_demod_match),
#endif
		   },
	.probe = stm_fe_demod_probe,
	.remove = stm_fe_demod_remove,
    .shutdown = stm_fe_demod_shutdown
};

static struct platform_driver stm_fe_lnb_driver = {
	.driver = {
		   .name = STM_FE_LNB_NAME,
		   .owner = THIS_MODULE,
#ifdef CONFIG_PM
		   .pm = &stm_fe_lnb_pm_ops,
#endif
#ifdef CONFIG_OF
		   .of_match_table = of_match_ptr(stm_lnb_match),
#endif
		   },
	.probe = stm_fe_lnb_probe,
	.remove = stm_fe_lnb_remove
};

static struct platform_driver stm_fe_diseqc_driver = {
	.driver = {
		   .name = STM_FE_DISEQC_NAME,
		   .owner = THIS_MODULE,
#ifdef CONFIG_PM
		   .pm = &stm_fe_diseqc_pm_ops,
#endif
#ifdef CONFIG_OF
		   .of_match_table = of_match_ptr(stm_diseqc_match),
#endif
		   },
	.probe = stm_fe_diseqc_probe,
	.remove = stm_fe_diseqc_remove
};

static struct platform_driver stm_fe_rf_matrix_driver = {
	.driver = {
		   .name = STM_FE_RF_MATRIX_NAME,
		   .owner = THIS_MODULE,
#ifdef CONFIG_PM
		   .pm = &stm_fe_rf_matrix_pm_ops,
#endif
#ifdef CONFIG_OF
		   .of_match_table = of_match_ptr(stm_rf_matrix_match),
#endif
		   },
	.probe = stm_fe_rf_matrix_probe,
	.remove = stm_fe_rf_matrix_remove
};

#ifdef CONFIG_OF
static struct of_device_id stm_ipfe_match[] = {
	{
		.compatible = "st,ipfe",
	},
		{},
};

static struct of_device_id stm_ipfec_match[] = {
	{
		.compatible = "st,ipfec",
	},
		{},
};
#endif

static struct platform_driver stm_fe_ip_driver = {
	.driver = {
		   .name = STM_FE_IP_NAME,
		   .owner = THIS_MODULE,
#ifdef CONFIG_OF
		   .of_match_table = of_match_ptr(stm_ipfe_match),
#endif
		   },
	.probe = stm_fe_ip_probe,
	.remove = stm_fe_ip_remove
};

struct stm_fe_engine_s stm_fe_engine = {
	.obj_cnt = 0,
	.fe = 0
};

static struct platform_driver stm_fe_ipfec_driver = {
	.driver = {
		   .name = STM_FE_IPFEC_NAME,
		   .owner = THIS_MODULE,
#ifdef CONFIG_OF
		   .of_match_table = of_match_ptr(stm_ipfec_match),
#endif
		   },
	.probe = stm_fe_ipfec_probe,
	.remove = stm_fe_ipfec_remove
};

extern int stm_fe_native;

#define OBJ_NAME_SIZE_MAX 32
#define OBJ_NAME_NONE_SIZE 4
#define DEFAULT_PARENT 0
int32_t stm_fe_discover(stm_fe_object_info_t **fe, uint32_t *cnt)
{
	struct list_head *pos;
	struct stm_fe_demod_s *demod_priv;
	struct stm_fe_lnb_s *lnb_priv;
	struct stm_fe_diseqc_s *diseqc_priv;
	struct stm_fe_rf_matrix_s *rf_matrix_priv;
	struct stm_fe_ip_s *ip_priv;
	struct stm_fe_ipfec_s *ipfec_priv;
	uint32_t obj_cnt = 0;
	uint8_t str_len;

	if (!stm_fe_engine.fe) {
		stm_fe_engine.fe = kzalloc(stm_fe_engine.obj_cnt *
					   sizeof(struct stm_fe_object_info_s)
						, GFP_KERNEL);
		if (!stm_fe_engine.fe) {
			pr_err("%s: mem alloc obj info failed\n", __func__);
			return -ENOMEM;
		}
	}

	/* Discover all demodulation objects */
	list_for_each_prev(pos, &stm_fe_engine.demod_list) {
		demod_priv = list_entry(pos, struct stm_fe_demod_s, list);
		str_len = strlen(demod_priv->demod_name);
		if (str_len >= OBJ_NAME_SIZE_MAX) {
			pr_err("%s: fail.--> invalid name\n", __func__);
			return -EINVAL;
		}

		strncpy(stm_fe_engine.fe[obj_cnt].stm_fe_obj,
					demod_priv->demod_name,	str_len);
		stm_fe_engine.fe[obj_cnt].stm_fe_obj[str_len] = '\0';
		snprintf(stm_fe_engine.fe[obj_cnt].member_of[DEFAULT_PARENT],
		    sizeof(stm_fe_engine.fe[obj_cnt].member_of[DEFAULT_PARENT]),
			"stm_fe_demod.%d", demod_priv->demod_id);
		stm_fe_engine.fe[obj_cnt].type = STM_FE_DEMOD;
		get_demod_caps(&stm_fe_engine.fe[obj_cnt].u_caps.demod,
			       demod_priv->config->demod_name);
		pr_info("%s: Discovered %s\n",
					      __func__, demod_priv->demod_name);
		obj_cnt++;
	}
	/* Discover all lnb objects */
	list_for_each_prev(pos, &stm_fe_engine.lnb_list) {
		lnb_priv = list_entry(pos, struct stm_fe_lnb_s, list);
		str_len = strlen(lnb_priv->lnb_name);
		if (str_len >= OBJ_NAME_SIZE_MAX) {
			pr_err("%s: fail.--> invalid name\n", __func__);
			return -EINVAL;
		}
		strncpy(stm_fe_engine.fe[obj_cnt].stm_fe_obj,
					lnb_priv->lnb_name, str_len);
		stm_fe_engine.fe[obj_cnt].stm_fe_obj[str_len] = '\0';
		snprintf(stm_fe_engine.fe[obj_cnt].member_of[DEFAULT_PARENT],
		    sizeof(stm_fe_engine.fe[obj_cnt].member_of[DEFAULT_PARENT]),
			"stm_fe_demod.%d", lnb_priv->lnb_id);
		stm_fe_engine.fe[obj_cnt].type = STM_FE_LNB;
		pr_info("%s: Discovered %s ...\n",
						  __func__, lnb_priv->lnb_name);
		obj_cnt++;
	}
	/* Discover all diseqc objects */
	list_for_each_prev(pos, &stm_fe_engine.diseqc_list) {
		diseqc_priv = list_entry(pos, struct stm_fe_diseqc_s, list);
		str_len = strlen(diseqc_priv->diseqc_name);
		if (str_len >= OBJ_NAME_SIZE_MAX) {
			pr_err("%s: fail.--> invalid name\n", __func__);
			return -EINVAL;
		}
		strncpy(stm_fe_engine.fe[obj_cnt].stm_fe_obj,
					diseqc_priv->diseqc_name, str_len);
		stm_fe_engine.fe[obj_cnt].stm_fe_obj[str_len] = '\0';
		snprintf(stm_fe_engine.fe[obj_cnt].member_of[DEFAULT_PARENT],
		    sizeof(stm_fe_engine.fe[obj_cnt].member_of[DEFAULT_PARENT]),
			"stm_fe_demod.%d", diseqc_priv->diseqc_id);
		stm_fe_engine.fe[obj_cnt].type = STM_FE_DISEQC;
		get_diseqc_caps(&stm_fe_engine.fe[obj_cnt].u_caps.diseqc,
				diseqc_priv->config->diseqc_name);
		pr_info("%s: Discovered %s ...\n",
					    __func__, diseqc_priv->diseqc_name);
		obj_cnt++;
	}
	/* Discover all rf_matrix objects */
	list_for_each_prev(pos, &stm_fe_engine.rf_matrix_list) {
		rf_matrix_priv =
			list_entry(pos, struct stm_fe_rf_matrix_s, list);
		str_len = strlen(rf_matrix_priv->name);
		if (str_len >= OBJ_NAME_SIZE_MAX) {
			pr_err("%s: fail.--> invalid name\n", __func__);
			return -EINVAL;
		}
		strncpy(stm_fe_engine.fe[obj_cnt].stm_fe_obj,
						rf_matrix_priv->name, str_len);
		stm_fe_engine.fe[obj_cnt].stm_fe_obj[str_len] = '\0';
		snprintf(stm_fe_engine.fe[obj_cnt].member_of[DEFAULT_PARENT],
		    sizeof(stm_fe_engine.fe[obj_cnt].member_of[DEFAULT_PARENT]),
					 "stm_fe_demod.%d", rf_matrix_priv->id);
		stm_fe_engine.fe[obj_cnt].type = STM_FE_RF_MATRIX;
		get_rf_matrix_caps(&stm_fe_engine.fe[obj_cnt].u_caps.rf_matrix,
								rf_matrix_priv);
		pr_info("%s: Discovered %s ...\n",
						__func__, rf_matrix_priv->name);
		obj_cnt++;
	}
	/* Discover all ip objects */
	list_for_each_prev(pos, &stm_fe_engine.ip_list) {
		ip_priv =
		    list_entry(pos, struct stm_fe_ip_s, list);
		str_len = strlen(ip_priv->ip_name);
		if (str_len >= OBJ_NAME_SIZE_MAX) {
			pr_err("%s: fail.--> invalid name\n", __func__);
			return -EINVAL;
		}
		strncpy(stm_fe_engine.fe[obj_cnt].stm_fe_obj, ip_priv->ip_name,
			str_len);
		stm_fe_engine.fe[obj_cnt].stm_fe_obj[str_len] = '\0';
		snprintf(stm_fe_engine.fe[obj_cnt].member_of[DEFAULT_PARENT],
		    sizeof(stm_fe_engine.fe[obj_cnt].member_of[DEFAULT_PARENT]),
			"stm_fe_ip.%d", ip_priv->ip_id);
		stm_fe_engine.fe[obj_cnt].type = STM_FE_IP;
		get_ip_caps(&stm_fe_engine.fe[obj_cnt].u_caps.ip,
				ip_priv->config->ip_name);
		pr_info("%s: Discovered %s ...\n", __func__, ip_priv->ip_name);
		obj_cnt++;
	}
	/* Discover all ip fec objects */
	list_for_each_prev(pos, &stm_fe_engine.ipfec_list) {
		ipfec_priv =
		    list_entry(pos, struct stm_fe_ipfec_s, list);
		str_len = strlen(ipfec_priv->ipfec_name);
		if (str_len >= OBJ_NAME_SIZE_MAX) {
			pr_err("%s: fail.--> invalid name\n", __func__);
			return -EINVAL;
		}
		strncpy(stm_fe_engine.fe[obj_cnt].stm_fe_obj,
					ipfec_priv->ipfec_name, str_len);
		stm_fe_engine.fe[obj_cnt].stm_fe_obj[str_len] = '\0';
		snprintf(stm_fe_engine.fe[obj_cnt].member_of[DEFAULT_PARENT],
		    sizeof(stm_fe_engine.fe[obj_cnt].member_of[DEFAULT_PARENT]),
				       "stm_fe_ipfec.%d", ipfec_priv->ipfec_id);
		stm_fe_engine.fe[obj_cnt].type = STM_FE_IPFEC;
		get_ipfec_caps(&stm_fe_engine.fe[obj_cnt].u_caps.ipfec,
				ipfec_priv->config->ipfec_name);
		pr_info("%s: Discovered %s ...\n",
					      __func__, ipfec_priv->ipfec_name);
		obj_cnt++;
	}
	*cnt = obj_cnt;
	*fe = stm_fe_engine.fe;

	return 0;
}

static void get_demod_caps(stm_fe_demod_caps_t *demod_caps, char *demod_name)
{
	if (IS_NAME_MATCH(demod_name, "STV090x"))
		*demod_caps = stv0903_caps;
	else if (IS_NAME_MATCH(demod_name, "STV0910"))
		*demod_caps = stv0903_caps;
	else if (IS_NAME_MATCH(demod_name, "STV0913"))
		*demod_caps = stv0903_caps;
	else if (IS_NAME_MATCH(demod_name, "STV0900"))
		*demod_caps = stv0903_caps;
	else if (IS_NAME_MATCH(demod_name, "STV0367T"))
		*demod_caps = stv0367t_caps;
	else if (IS_NAME_MATCH(demod_name, "STV0367C"))
		*demod_caps = stv0367c_caps;
	else if (IS_NAME_MATCH(demod_name, "STV0367"))
		*demod_caps = stv0367_caps;
	else if (IS_NAME_MATCH(demod_name, "SASC1_REMOTE"))
		*demod_caps = remote_anx_b_caps;
	else if (IS_NAME_MATCH(demod_name, "REMOTE_ANNEX_B"))
		*demod_caps = remote_anx_b_caps;
	else if (IS_NAME_MATCH(demod_name, "REMOTE_DVB_C"))
		*demod_caps = remote_dvb_c_caps;
	else if (IS_NAME_MATCH(demod_name, "REMOTE"))
		*demod_caps = remote_caps;
	else if (IS_NAME_MATCH(demod_name, "MXL584"))
		*demod_caps = stv0903_caps;
	else if (IS_NAME_MATCH(demod_name, "MXL582"))
		*demod_caps = stv0903_caps;
	else if (IS_NAME_MATCH(demod_name, "MXL214"))
		*demod_caps = stv0367c_caps;
	/* TCH_FIX - Begin - Add SILABS */
	else if (IS_NAME_MATCH(demod_name, "SILABS"))
		*demod_caps = SiLabs_2xSi2147_Si21652_caps;
	/* TCH_FIX - End - Add SILABS */
	else if (IS_NAME_MATCH(demod_name, "DUMMY"))
		*demod_caps = dummy_demod_caps;
	else
		pr_err("%s: caps not set for %s\n", __func__, demod_name);
}

static void get_diseqc_caps(stm_fe_diseqc_caps_t *diseqc_caps,
							char *diseqc_name)
{
	if (IS_NAME_MATCH(diseqc_name, "DISEQC_STV090x"))
		*diseqc_caps = diseqc_stv0903_caps;
	else if (IS_NAME_MATCH(diseqc_name, "DISEQC_STV0910"))
		*diseqc_caps = diseqc_stv0903_caps;
	else if (IS_NAME_MATCH(diseqc_name, "DISEQC_STV0913"))
		*diseqc_caps = diseqc_stv0903_caps;
	else if (IS_NAME_MATCH(diseqc_name, "DISEQC_STV0900"))
		*diseqc_caps = diseqc_stv0903_caps;
	else
		pr_err("%s: caps not set for %s\n", __func__, diseqc_name);
}

static void get_rf_matrix_caps(stm_fe_rf_matrix_caps_t *rf_matrix_caps,
						struct stm_fe_rf_matrix_s *priv)
{
	char *name = priv->config->name;

	if (IS_NAME_MATCH(name, "RF_MATRIX_STV6120"))
		*rf_matrix_caps = rf_matrix_stv6120_caps;
	else {
		pr_err("%s: caps not set for %s\n", __func__, name);
		return;
	}
	if (priv->config->max_input < rf_matrix_caps->max_input)
		rf_matrix_caps->max_input = priv->config->max_input;
}

static void get_ip_caps(stm_fe_ip_caps_t *ip_fe_caps, char *ip_name)
{
	if (IS_NAME_MATCH(ip_name, "IPFE"))
		*ip_fe_caps = ipfe_caps;
	else
		pr_err("%s: caps not set for %s\n", __func__, ip_name);
}

static void get_ipfec_caps(stm_fe_ip_fec_caps_t *ipfec_fe_caps,
							char *ipfec_name)
{
	if (IS_NAME_MATCH(ipfec_name, "IPFEC"))
		*ipfec_fe_caps = ipfec_caps;
	else
		pr_err("%s: caps not set for %s\n", __func__, ipfec_name);
}
EXPORT_SYMBOL(stm_fe_discover);

struct list_head *get_demod_list(void)
{
	return &stm_fe_engine.demod_list;
}
EXPORT_SYMBOL(get_demod_list);

struct list_head *get_diseqc_list(void)
{
	return &stm_fe_engine.diseqc_list;
}

struct list_head *get_rf_matrix_list(void)
{
	return &stm_fe_engine.rf_matrix_list;
}

struct list_head *get_lnb_list(void)
{
	return &stm_fe_engine.lnb_list;
}

void incr_obj_cnt(void)
{
	stm_fe_engine.obj_cnt++;
}

void decr_obj_cnt(void)
{
	stm_fe_engine.obj_cnt--;
}

stm_object_h *get_type_for_registry(void)
{
	return (stm_object_h *)&stm_fe_engine.demod_obj_type;
}

static int32_t __init stm_fe_init(void)
{
	int ret = 0;
	int demod_reg_ret = 0;
	int reg_ret = 0;
	pr_info("Loading module stm_fe ...\n");

	INIT_LIST_HEAD(&stm_fe_engine.demod_list);
	INIT_LIST_HEAD(&stm_fe_engine.lnb_list);
	INIT_LIST_HEAD(&stm_fe_engine.diseqc_list);
	INIT_LIST_HEAD(&stm_fe_engine.rf_matrix_list);
	INIT_LIST_HEAD(&stm_fe_engine.ip_list);
	INIT_LIST_HEAD(&stm_fe_engine.ipfec_list);
	if (stmfe_debugfs_create())
		pr_err("%s: debugfs creation failed\n", __func__);

	ret = stm_registry_add_object(STM_REGISTRY_TYPES, "stm_fe_demod",
				 (stm_object_h) &stm_fe_engine.demod_obj_type);
	if (ret) {
		pr_err("%s: reg add_object failed: %d\n", __func__, ret);
		return ret;
	}

	ret = stm_registry_add_object(STM_REGISTRY_TYPES, "stm_fe_ip",
				    (stm_object_h) &stm_fe_engine.ip_obj_type);

	if (ret) {
		pr_err("%s: reg add_object failed: %d\n", __func__, ret);
		reg_ret = stm_registry_remove_object(
				(stm_object_h)&stm_fe_engine.demod_obj_type);
		if (reg_ret)
			pr_err("%s: reg remove_object failed\n", __func__);

		return ret;
	}

	demod_reg_ret = platform_driver_register(&stm_fe_demod_driver);
	if (demod_reg_ret) {
		pr_info("%s: demod pltf driver register failed\n", __func__);
		reg_ret = stm_registry_remove_object(
				(stm_object_h)&stm_fe_engine.demod_obj_type);
		if (reg_ret)
			pr_err("%s: reg remove_object failed\n", __func__);

		goto demod_reg_fail;
	}

	ret = platform_driver_register(&stm_fe_lnb_driver);
	if (ret)
		pr_info("%s: lnb pltf driver register failed\n", __func__);

	ret = platform_driver_register(&stm_fe_diseqc_driver);
	if (ret)
		pr_info("%s: diseqc pltf driver register failed\n", __func__);

	ret = platform_driver_register(&stm_fe_rf_matrix_driver);
	if (ret)
		pr_info("%s: rfmatrix pltf driver register failed\n", __func__);

demod_reg_fail:
	ret = platform_driver_register(&stm_fe_ip_driver);
	if (ret) {
		pr_info("%s: ip pltf driver register failed\n", __func__);
		reg_ret = stm_registry_remove_object(
				(stm_object_h)&stm_fe_engine.ip_obj_type);
		if (reg_ret)
			pr_err("%s: reg_remove_object failed\n", __func__);

		if (demod_reg_ret)
			goto err;
		else
			goto ip_reg_fail_demod_pass;
	}
	ret = platform_driver_register(&stm_fe_ipfec_driver);
	if (ret)
		pr_info("%s: ipfec pltf driver register failed\n", __func__);
ip_reg_fail_demod_pass:
	return 0;
err:
	pr_err("%s: pltf driver register failed for ip & demod\n", __func__);

	return ret;
}

module_init(stm_fe_init);

static void __exit stm_fe_term(void)
{
	int ret;
	pr_info("Unloading module stm_fe...\n");

	ret = stm_registry_remove_object((stm_object_h)
						&stm_fe_engine.demod_obj_type);
	if (ret)
		pr_err("%s: reg remove_object failed: %d\n", __func__, ret);
	ret = stm_registry_remove_object((stm_object_h)
						&stm_fe_engine.ip_obj_type);
	if (ret)
		pr_err("%s: reg remove_object failed: %d\n", __func__, ret);

	if (stmfe_debugfs_remove())
		pr_err("%s: debugfs deletion failed\n", __func__);
	kfree(stm_fe_engine.fe);

	stm_fe_native = STM_FE_NO_OPTIONS_DEFAULT;
	platform_driver_unregister(&stm_fe_ipfec_driver);
	platform_driver_unregister(&stm_fe_rf_matrix_driver);
	platform_driver_unregister(&stm_fe_diseqc_driver);
	platform_driver_unregister(&stm_fe_lnb_driver);
	platform_driver_unregister(&stm_fe_demod_driver);
	platform_driver_unregister(&stm_fe_ip_driver);
}

module_exit(stm_fe_term);

MODULE_DESCRIPTION("Frontend engine device manager");
MODULE_AUTHOR("STMicroelectronics Limited");
MODULE_VERSION(STM_FE_VERSION);
MODULE_LICENSE("GPL");
