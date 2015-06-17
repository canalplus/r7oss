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

Source file name : stm_fe_rf_matrix.c
Author :           Ashish Gandhi

API dedinitions for RF MATRIX

Date        Modification                                    Name
----        ------------                                    --------
24-Jul-13   Created                                         Ashish Gandhi
************************************************************************/
#include <linux/kernel.h>
#include <linux/version.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 2, 2)
#include <linux/export.h>
#else
#include <linux/module.h>
#endif
#include <linux/list.h>
#include <linux/platform_device.h>
#ifdef CONFIG_OF
#include <linux/of.h>
#endif
#include <linux/err.h>
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
#include "stm_fe_demod.h"
#include "stm_fe_ip.h"
#include "stm_fe_rf_matrix.h"
#include "stm_fe_engine.h"
#include "stm_fe_diseqc.h"
#include "stm_fe_rf_matrix.h"
#include "stm_fe_install.h"
#include <linux/pm_runtime.h>
#include <linux/suspend.h>

static int dev_id;
static int grp_list_id;
static int grp_id;
#ifdef CONFIG_OF
static void *stm_rf_matrix_get_pdata(struct platform_device *pdev)
{
	int ret = 0;
	struct device_node *np = pdev->dev.of_node;
	struct device_node *fe;
	struct device_node *iter;
	struct rf_matrix_config_s *conf;
	const char *name;
	const char *lla;

	conf = devm_kzalloc(&pdev->dev, sizeof(*conf), GFP_KERNEL);
	if (!conf) {
		pr_err("%s: rf_matrix platform data alloc failed\n", __func__);
		ret = -ENOMEM;
		goto rfm_pdata_err;
	}
	fe = of_get_parent(np);
	if (!fe) {
		ret = -EINVAL;
		goto rfm_pdata_err;
	}
	ret = of_property_read_u32(fe, "control", &conf->ctrl);
	if (ret)
		goto rfm_pdata_err;
	ret = of_property_read_u32(fe, "group_id", &conf->grp_id);
	if (ret)
		goto rfm_pdata_err;
	iter = of_parse_phandle(np, "rf_matrix", 0);
	ret = of_property_read_string(iter, "model", &name);
	if (ret)
		goto rfm_pdata_err;
	strlcpy(conf->name, name, RF_MATRIX_NAME_MAX_LEN);
	ret = of_property_read_u32(iter, "lla_selection", &conf->lla_sel);
	if (ret) {
		pr_err("%s: invalid lla_selection\n", __func__);
		goto rfm_pdata_err;
	}
	if (conf->lla_sel != RF_MATRIX_LLA_AUTO
			&& conf->lla_sel != RF_MATRIX_LLA_FORCED) {
		pr_err("%s: wrong value of lla_selection\n", __func__);
		goto rfm_pdata_err;
	}

	if (conf->lla_sel == RF_MATRIX_LLA_FORCED) {
		ret = of_property_read_string(iter, "lla_driver", &lla);
		if (ret)
			goto rfm_pdata_err;
		if ((strncmp(lla, "st,lla", sizeof("st,lla"))) &&
			(strncmp(lla, "custom,lla", sizeof("custom,lla")))) {
			pr_err("%s: lla not correctly set in dt\n", __func__);
			goto rfm_pdata_err;
		}
		strlcpy(conf->lla, lla, RF_MATRIX_NAME_MAX_LEN);
	}

	of_property_read_u32(iter, "max_input", &conf->max_input);
	ret = of_property_read_u32(iter, "input_id", &conf->input_id);
	if (ret)
		goto rfm_pdata_err;

	/*
	 * Presently rf matrix device contol is via tuner device , it is not
	 * required to parse i2c node information. Incase some future device
	 * (i2c/memory mapped) with rfmatrix capability needs to be supported ,
	 * IO information will need to be fecthed as done for demod devices
	 * */
rfm_pdata_err:
	if (ret) {
		pr_err("%s: rfm config not properly set\n", __func__);
		return ERR_PTR(ret);
	}

	return conf;
}
#else
static void *stm_rf_matrix_get_pdata(struct platform_device *pdev)
{
	return NULL;
}
#endif

/*
 * Name: stm_fe_rf_matrix_probe()
 *
 * Description:get the configuration information of rf_matrix
 */
int stm_fe_rf_matrix_probe(struct platform_device *pdev)
{
	struct stm_platdev_data *plat_dev_data_p;
	struct rf_matrix_config_s *conf;
	struct stm_fe_rf_matrix_s *priv;

	priv = stm_fe_malloc(sizeof(struct stm_fe_rf_matrix_s));
	if (!priv) {
		pr_err("%s: mem alloc failed for rf_matrix\n", __func__);
		return -ENOMEM;
	}
	/* Get the configuration information about the rf_matrix */
	if (pdev->dev.of_node) {
		conf = stm_rf_matrix_get_pdata(pdev);
		if (IS_ERR_OR_NULL(conf)) {
			stm_fe_free((void **)&priv);
			return PTR_ERR(conf);
		}
		priv->id = dev_id;
		pdev->id = dev_id;
		dev_id++;
		priv->dt_enable = true;
	} else {
		plat_dev_data_p = pdev->dev.platform_data;
		conf = plat_dev_data_p->config_data;
		priv->id = pdev->id;
		priv->dt_enable = false;
	}
	if (conf->grp_id != grp_id) {
		grp_list_id = 0;
		grp_id = conf->grp_id;
	}
	priv->grp_list_id = grp_list_id;
	grp_list_id++;

	list_add(&priv->list, get_rf_matrix_list());
	snprintf(priv->name, sizeof(priv->name),
					       "stm_fe_rf_matrix.%d", priv->id);
	priv->config = conf;
	priv->data = pdev;

	incr_obj_cnt();

	pr_info("%s: Configuring rf_matrix %s\n", __func__, priv->name);

	return 0;
}

/*
 * Name: stm_fe_rf_matrix_remove()
 *
 * Description:remove the configuration information of rf_matrix
 */
int stm_fe_rf_matrix_remove(struct platform_device *pdev)
{
	struct stm_fe_rf_matrix_s *priv = rf_matrix_from_id(pdev->id);

	if (!priv) {
		pr_err("%s: rf matrix obj is not initialised\n", __func__);
		return -ENODEV;
	}

	list_del(&priv->list);
	stm_fe_free((void **)&priv);
	decr_obj_cnt();

	return 0;
}

/*
 * Name: stm_fe_rf_matrix_new()
 *
 * Description:allocate a new rf_matrix control object
 */
int32_t stm_fe_rf_matrix_new(const char *name, stm_fe_demod_h demod_priv,
							stm_fe_rf_matrix_h *obj)
{
	int ret = -1;
	struct stm_fe_rf_matrix_s *priv = rf_matrix_from_name(name);

	*obj = NULL;

	if (!demod_priv) {
		pr_err("%s: demod obj is not initialised\n", __func__);
		return -ENODEV;
	}
	if (!priv) {
		pr_err("%s: '%s' absent in rf_matrix list\n", __func__, name);
		return -ENODEV;
	}

	if (priv->ops) {
		pr_err("%s: '%s' already exists\n", __func__, name);
		return -EEXIST;
	}

	/*Memory Allocations for STMFE internal structures */

	priv->rf_matrix_h = stm_fe_malloc(sizeof(STCHIP_Info_t));
	if (!priv->rf_matrix_h) {
		pr_err("%s: mem alloc for rf_matrix_h failed\n", __func__);
		return -ENOMEM;
	}

	priv->ops = stm_fe_malloc(sizeof(struct rf_matrix_ops_s));
	if (!priv->ops) {
		pr_err("%s: mem alloc for rf_matrix FP failed\n", __func__);
		ret = -ENOMEM;
		goto rf_matrix_ops_malloc_fail;
	}
	if (priv->dt_enable)
		ret = rf_matrix_install_dt(priv);
	else
		ret = rf_matrix_install(priv);
	if (ret) {
		pr_err("%s: no rf_matrix avaialable to install\n", __func__);
		ret = -ENODEV;
		goto rf_matrix_install_fail;
	}

	demod_priv->rf_src_selected = priv->id;
	priv->demod_h = demod_priv;
	priv->lnb_h = demod_priv->lnb_h;

	/* Control through Linux DVB; Not necessary to proceed further */
	if (priv->bypass_control) {
		pr_info("%s: stm_fe RF_MATRIX Control bypassed...\n", __func__);
		goto ret_ok;
	}

	/*Initialization of rf_matrix */
	if (priv->ops->init) {
		ret = priv->ops->init(priv);
		if (ret) {
			pr_err("%s: init failed\n", __func__);
			goto rf_matrix_init_fail;
		}
	} else {
		pr_err("%s: FP ops->init is NULL\n", __func__);
		goto rf_matrix_init_fail;
	}

ret_ok:
	priv->rpm_suspended = false;

	*obj = priv;
	demod_priv->rf_matrix_h = priv;

#ifdef CONFIG_PM_RUNTIME
	/* Increment device usage but not more than 1 */
	if (!atomic_read(&priv->data->dev.power.usage_count))
		pm_runtime_get_noresume(&priv->data->dev);

	pm_runtime_set_active(&priv->data->dev);
	pm_runtime_enable(&priv->data->dev);
	pm_runtime_put_sync_suspend(&priv->data->dev);
#endif

	priv->rpm_suspended = true;

	return 0;

rf_matrix_init_fail:
	priv->demod_h = NULL;
	rf_matrix_uninstall(priv);
rf_matrix_install_fail:
	stm_fe_free((void **)&priv->ops);
rf_matrix_ops_malloc_fail:
	stm_fe_free((void **)&priv->rf_matrix_h);
	return ret;
}
EXPORT_SYMBOL(stm_fe_rf_matrix_new);

/*
 * Name: stm_fe_rf_matrix_select_source()
 *
 * Description: selects an RF source from an RF matrix switch
 * for input to the tuner
 */
int32_t __must_check stm_fe_rf_matrix_select_source(stm_fe_rf_matrix_h obj,
							uint32_t src_num)
{
	int ret = 0;
	struct stm_fe_rf_matrix_s *priv = obj;
	struct stm_fe_demod_s *demod_priv;
	struct stm_fe_demod_s *target_demod_priv;
	struct stm_fe_rf_matrix_s *target_priv = NULL;
	struct rf_matrix_config_s *config = NULL;

	if (!priv) {
		pr_err("%s: rf_matrix obj is not initialised\n", __func__);
		return -EINVAL;
	}
	if (!priv->ops) {
		pr_err("%s: rf_matrix object is not allocated\n", __func__);
		return -ENODEV;
	}
	demod_priv = (struct stm_fe_demod_s *)priv->demod_h;

	if (!((src_num >= 1) && (src_num <= priv->config->max_input))) {
		pr_err("%s: invalid rf input source id\n", __func__);
		return -EINVAL;
	}

	target_priv = rf_matrix_from_id_grp(--src_num, priv->config->grp_id);
	if (!target_priv) {
		pr_info("%s: invalid target_priv\n", __func__);
		return -ENODEV;
	}
	config = target_priv->config;
	if (!config) {
		pr_err("%s: invalid target_priv config\n", __func__);
		return -EFAULT;
	}
	if (priv->ops->select_source) {
		ret = priv->ops->select_source(priv, config->input_id);
	} else {
		pr_err("%s: FP select_source is NULL\n", __func__);
		return -EFAULT;
	}

	if (ret) {
		pr_err("%s: could not select source\n", __func__);
		return ret;
	}
	demod_priv->rf_src_selected = target_priv->id;

	target_demod_priv = target_priv->demod_h;
	target_demod_priv->lnb_h = target_priv->lnb_h;

	demod_priv->lnb_h = target_priv->lnb_h;

	return ret;
}
EXPORT_SYMBOL(stm_fe_rf_matrix_select_source);

/*
 * Name: stm_fe_rf_matrix_delete()
 *
 * Description: deletes a rf_matrix control object releasing all
 * associated resources
 */
int32_t stm_fe_rf_matrix_delete(stm_fe_rf_matrix_h obj)
{
	int ret = 0, error = 0;
	struct stm_fe_rf_matrix_s *priv = obj;
	struct stm_fe_demod_s *demod_priv;

	if (!priv) {
		pr_err("%s:rf_matrix obj is not initialised\n", __func__);
		return -EINVAL;
	}
	if (!priv->ops) {
		pr_err("%s: rf_matrix object not allocated\n", __func__);
		return -ENODEV;
	}

	demod_priv = priv->demod_h;

	demod_priv->rf_src_selected = priv->config->input_id;
	demod_priv->lnb_h = priv->lnb_h;

#ifdef CONFIG_PM_RUNTIME
	/* Decrement device usage but not less than 0 */
	if (atomic_read(&demod_priv->demod_data->dev.power.usage_count) > 0)
		pm_runtime_put_noidle(&demod_priv->demod_data->dev);

		pm_runtime_set_suspended(&priv->data->dev);
		pm_runtime_disable(&priv->data->dev);
#endif

	if (priv->ops->term) {
		ret = priv->ops->term(priv);
		if (ret) {
			pr_err("%s: unable to terminate\n", __func__);
			error = ret;
		}
	} else {
		pr_err("%s: FP priv->ops->term is NULL\n", __func__);
		error = -EFAULT;
	}

	ret = rf_matrix_uninstall(priv);

	if (ret) {
		pr_err("%s: rf_matrix_uninstall failed\n", __func__);
		error = ret;
	}

	stm_fe_free((void **)&priv->ops);
	stm_fe_free((void **)&priv->rf_matrix_h);
	demod_priv->rf_matrix_h = NULL;
	priv->demod_h = NULL;

	return error;
}
EXPORT_SYMBOL(stm_fe_rf_matrix_delete);

#ifdef CONFIG_PM
/*
 * Name: stm_fe_rf_matrix_suspend()
 *
 * Description: put rf_matrix in (standby)low power state.
 */
int32_t stm_fe_rf_matrix_suspend(struct device *dev)
{
	int32_t ret = 0;
	int32_t id;
	struct stm_fe_rf_matrix_s *priv = NULL;

	/* Get the configuration information */
	id = to_platform_device(dev)->id;
	priv = rf_matrix_from_id(id);

	if (!priv) {
		pr_warn("%s: object not intialized...skipping\n", __func__);
		return ret;
	}

	if (!priv->ops) {
		pr_warn("%s: object not allocated...skipping\n", __func__);
		return ret;
	}

	pr_info("%s: %s suspending in low power mode\n", __func__, priv->name);

	if ((priv->rpm_suspended) || (priv->bypass_control))
		return ret;

	return ret;
}

/*
 * Name: stm_fe_rf_matrix_resume()
 *
 * Description: resume rf_matrix in normal power state.
 */
int32_t stm_fe_rf_matrix_resume(struct device *dev)
{
	int32_t ret = 0;
	int32_t id;
	struct stm_fe_rf_matrix_s *priv = NULL;

	/* Get the configuration information */
	id = to_platform_device(dev)->id;
	priv = rf_matrix_from_id(id);

	if (!priv) {
		pr_warn("%s: object not intialized...skipping\n", __func__);
		return ret;
	}

	if (!priv->ops) {
		pr_warn("%s: object not allocated...skipping\n", __func__);
		return ret;
	}

	pr_info("%s: %s resuming in low power mode...\n", __func__, priv->name);

	if (priv->bypass_control)
		return ret;

	return ret;
}

/*
 * Name: stm_fe_rf_matrix_freeze()
 *
 * Description: put rf_matrix in (standby)low power state.
 */
int32_t stm_fe_rf_matrix_freeze(struct device *dev)
{
	int32_t ret = 0;
	int32_t id;
	struct stm_fe_rf_matrix_s *priv = NULL;

	/* Get the configuration information */
	id = to_platform_device(dev)->id;
	priv = rf_matrix_from_id(id);

	if (!priv) {
		pr_warn("%s: object not intialized...skipping\n", __func__);
		return ret;
	}

	if (!priv->ops) {
		pr_warn("%s: object not allocated...skipping\n", __func__);
		return ret;
	}

	pr_info("%s: %s suspending in low power mode\n", __func__, priv->name);

	if ((priv->rpm_suspended) || (priv->bypass_control))
		return ret;

	return ret;
}

/*
 * Name: stm_fe_rf_matrix_restore()
 *
 * Description: resume rf_matrix in normal power state.
 */
int32_t stm_fe_rf_matrix_restore(struct device *dev)
{
	int32_t ret = 0;
	int32_t id;
	struct stm_fe_rf_matrix_s *priv = NULL;

	/* Get the configuration information */
	id = to_platform_device(dev)->id;
	priv = rf_matrix_from_id(id);

	if (!priv) {
		pr_warn("%s: object not intialized...skipping\n", __func__);
		return ret;
	}

	if (!priv->ops) {
		pr_warn("%s: object not allocated...skipping\n", __func__);
		return ret;
	}

	pr_info("%s: %s resuming in low power mode...\n", __func__, priv->name);

	if (priv->bypass_control)
		return ret;

	return ret;
}
#endif

struct stm_fe_rf_matrix_s *rf_matrix_from_name(const char *name)
{
	struct list_head *pos;
	list_for_each(pos, get_rf_matrix_list()) {
		struct stm_fe_rf_matrix_s *priv;
		priv = list_entry(pos, struct stm_fe_rf_matrix_s, list);
		if (!strcmp(priv->name, name))
			return priv;
	}

	return NULL;
}

struct stm_fe_rf_matrix_s *rf_matrix_from_id(uint32_t id)
{
	struct list_head *pos;
	list_for_each(pos, get_rf_matrix_list()) {
		struct stm_fe_rf_matrix_s *priv;
		priv = list_entry(pos, struct stm_fe_rf_matrix_s, list);
		if (priv->id == id)
			return priv;
	}

	return NULL;
}

struct stm_fe_rf_matrix_s *rf_matrix_from_id_grp(uint32_t id, uint32_t grp_id)
{
	struct list_head *pos;
	list_for_each(pos, get_rf_matrix_list()) {
		struct stm_fe_rf_matrix_s *priv;
		priv = list_entry(pos, struct stm_fe_rf_matrix_s, list);
		if (!priv->config)
			continue;
		if (priv->grp_list_id == id && priv->config->grp_id == grp_id)
			return priv;
	}

	return NULL;
}
