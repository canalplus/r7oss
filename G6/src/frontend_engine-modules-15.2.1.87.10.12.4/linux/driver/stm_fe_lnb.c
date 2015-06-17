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

Source file name : stm_fe_lnb.c
Author :           Rahul.V

API dedinitions for LNB device

Date        Modification                                    Name
----        ------------                                    --------
16-Jun-11   Created                                         Rahul.V
11-April-12 PM support added				    Ankur Tyagi
22-May-13   CPS support added				    Ankur Tyagi
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
#include <linux/of_address.h>
#endif
#include <linux/err.h>
#include <linux/stm/plat_dev.h>
#include <linux/stm/demod.h>
#include <linux/stm/lnb.h>
#include <linux/stm/diseqc.h>
#include <stm_registry.h>
#include <stm_fe.h>
#include <stfe_utilities.h>
#include <stm_fe_os.h>
#include <i2c_wrapper.h>
#include "stm_fe_demod.h"
#include "stm_fe_ip.h"
#include "stm_fe_lnb.h"
#include "stm_fe_engine.h"
#include "stm_fe_diseqc.h"
#include "stm_fe_install.h"
#include <linux/pm_runtime.h>
#include <linux/suspend.h>

static int lnb_dev_id;
#ifdef CONFIG_OF
static void *stm_lnb_get_pdata(struct platform_device *pdev)
{
	int ret = 0;
	struct device_node *np = pdev->dev.of_node;
	struct device_node *fe;
	struct device_node *iter;
	struct device_node *i2c;
	struct lnb_config_s *conf;
	const char *name;
	const char *lla;
	const unsigned int *addr;
	struct resource res;
	int i2c_id;

	conf = devm_kzalloc(&pdev->dev, sizeof(*conf), GFP_KERNEL);
	if (!conf) {
		pr_err("%s: lnb platform data alloc failed\n", __func__);
		ret = -ENOMEM;
		goto lnb_pdata_err;
	}
	fe = of_get_parent(np);
	if (!fe) {
		ret = -EINVAL;
		goto lnb_pdata_err;
	}
	ret = of_property_read_u32(fe, "control", &conf->ctrl);
	if (ret)
		goto lnb_pdata_err;
	iter = of_parse_phandle(np, "lnb", 0);
	if (!iter) {
		ret = -EINVAL;
		goto lnb_pdata_err;
	}
	ret = of_property_read_string(iter, "model", &name);
	if (ret)
		goto lnb_pdata_err;
	strlcpy(conf->lnb_name, name, LNB_NAME_MAX_LEN);

	i2c  = of_get_parent(iter);
	if (!i2c) {
		conf->lnb_io.io = LNB_IO_MEMORY_MAPPED;
		goto lnb_i2c_bypass;
	} else
		conf->lnb_io.io = LNB_IO_I2C;
	i2c_id = of_alias_get_id(i2c, "i2c");
	if (i2c_id < 0) {
		ret = -ENODEV;
		goto lnb_pdata_err;
	}
	ret = of_address_to_resource(i2c, 0, &res);
	if (ret < 0)
		pr_err("%s: fail to get i2c base address\n", __func__);

	ret = stm_fe_check_i2c_device_id((unsigned int *) res.start, i2c_id);
	if (!ret) {
		conf->lnb_io.bus = i2c_id;
		pr_debug("%s: lnb i2c bus id= %d\n", __func__, i2c_id);
	} else {
		pr_err("%s: wrong lnb i2c bus id\n", __func__);
		goto lnb_pdata_err;
	}

	addr = of_get_property(iter, "reg", NULL);
	if (addr)
		conf->lnb_io.address = (*addr) >> 24;
	else {
		pr_err("%s: get lnb address failed\n", __func__);
		goto lnb_pdata_err;
	}
	of_property_read_u32(iter, "lnb_i2c_route", &conf->lnb_io.route);
	of_property_read_u32(iter, "lnb_i2c_speed", &conf->lnb_io.baud);
lnb_i2c_bypass:
	ret = of_property_read_u32(iter, "lla_selection", &conf->lla_sel);
	if (ret) {
		pr_err("%s: invalid lla_selection\n", __func__);
		goto lnb_pdata_err;
	}
	if (conf->lla_sel != LNB_LLA_AUTO && conf->lla_sel != LNB_LLA_FORCED) {
		pr_err("%s: wrong value of lla_selection\n", __func__);
		goto lnb_pdata_err;
	}
	if (conf->lla_sel == LNB_LLA_FORCED) {
		ret = of_property_read_string(iter, "lla_driver", &lla);
		if (ret)
			goto lnb_pdata_err;

		if ((strncmp(lla, "st,lla", sizeof("st,lla")))
			&& (strncmp(lla, "custom,lla", sizeof("custom,lla")))) {
			pr_err("%s: lla_driver not correctly set\n", __func__);
			goto lnb_pdata_err;
		}
		strlcpy(conf->lla, lla, LNB_NAME_MAX_LEN);
	}

	of_property_read_u32(iter, "lnb_custom_option", &conf->cust_flags);
	ret = of_property_read_u32(iter,
			"lnb_vol_select", &conf->be_pio.volt_sel);
	if (ret)
		goto lnb_pdata_err;
	ret = of_property_read_u32(iter,
			"lnb_vol_enable", &conf->be_pio.volt_en);
	if (ret)
		goto lnb_pdata_err;

	ret = of_property_read_u32(iter,
			"lnb_tone_select", &conf->be_pio.tone_sel);

lnb_pdata_err:
	if (ret) {
		pr_err("%s: lnb config not properly set\n", __func__);
		return ERR_PTR(ret);
	}

	return conf;
}
#else
static void *stm_lnb_get_pdata(struct platform_device *stm_fe_demod_data)
{
	return NULL;
}
#endif

/*
 * Name: stm_fe_lnb_probe()
 *
 * Description:get the configuration information of lnb
 */
int stm_fe_lnb_probe(struct platform_device *pdev)
{
	struct stm_platdev_data *plat_dev_data_p;
	struct lnb_config_s *conf;
	struct stm_fe_lnb_s *priv;

	priv = stm_fe_malloc(sizeof(struct stm_fe_lnb_s));
	if (!priv)
		return -ENOMEM;
	/* Get the configuration information about the lnb */
	if (pdev->dev.of_node) {
		conf = stm_lnb_get_pdata(pdev);
		if (IS_ERR_OR_NULL(conf)) {
			stm_fe_free((void **)&priv);
			return PTR_ERR(conf);
		}
		priv->lnb_id = lnb_dev_id;
		pdev->id = lnb_dev_id;
		lnb_dev_id++;
		priv->dt_enable = true;
	} else {
		plat_dev_data_p = pdev->dev.platform_data;
		conf = plat_dev_data_p->config_data;
		priv->lnb_id = pdev->id;
		priv->dt_enable = false;
	}

	list_add(&priv->list, get_lnb_list());
	priv->lnb_list_p = get_lnb_list();
	snprintf(priv->lnb_name, sizeof(priv->lnb_name),
						 "stm_fe_lnb.%d", priv->lnb_id);
	priv->config = conf;
	priv->lnb_data = pdev;

	incr_obj_cnt();

	pr_info("%s: Configuring lnb %s\n", __func__, priv->lnb_name);

	return 0;
}

/*
 * Name: stm_fe_lnb_remove()
 *
 * Description:remove the configuration information of lnb
 */
int stm_fe_lnb_remove(struct platform_device *pdev)
{
	struct stm_fe_lnb_s *priv = lnb_from_id(pdev->id);

	if (priv) {
		list_del(&priv->list);
		stm_fe_free((void **)&priv);
		decr_obj_cnt();
	} else {
		pr_err("%s: lnb obj is not initialised\n", __func__);
		return -ENODEV;
	}
	return 0;
}

/*
 * Name: stm_fe_lnb_new()
 *
 * Description:allocate a new lnb control object
 */
int32_t stm_fe_lnb_new(const char *name, stm_fe_demod_h demod_obj,
							stm_fe_lnb_h *obj)
{
	int ret = -1;
	struct stm_fe_lnb_s *priv = lnb_from_name(name);
	struct stm_fe_demod_s *demod_priv;

	*obj = NULL;

	if (!demod_obj) {
		pr_err("%s: demod obj is not initialised\n", __func__);
		return -EINVAL;
	}
	demod_priv = demod_obj;

	if (!priv) {
		pr_err("%s: '%s' not present in lnb list\n", __func__, name);
		return -ENODEV;
	}

	if (priv->ops) {
		pr_err("%s: '%s' already exists\n", __func__, name);
		return -EEXIST;
	}

	/*Memory Allocations for STMFE internal structures */

	priv->lnb_h = stm_fe_malloc(sizeof(STCHIP_Info_t));
	if (!priv->lnb_h) {
		pr_err("%s: mem alloc fail for lnb_h\n", __func__);
		return -ENOMEM;
	}

	priv->ops = stm_fe_malloc(sizeof(struct lnb_ops_s));
	if (!priv->ops) {
		pr_err("%s: mem alloc fail for lnb FP\n", __func__);
		ret = -ENOMEM;
		goto lnb_ops_malloc_fail;
	}
	if (priv->dt_enable)
		ret = lnb_install_dt(priv);
	else
		ret = lnb_install(priv);
	if (ret) {
		pr_err("%s: No lnb avaialable to install\n", __func__);
		ret = -ENODEV;
		goto lnb_install_fail;
	}

	priv->demod_h = demod_obj;

	/* Control through Linux DVB; Not necessary to proceed further */
	if (priv->bypass_control) {
		pr_info("%s: stm_fe LNB Control bypassed...\n", __func__);
		goto ret_ok;
	}

	/*Initialization of lnb */
	if (priv->ops->init) {
		ret = priv->ops->init(priv);
		if (ret) {
			pr_err("%s: lnb init failed\n", __func__);
			goto lnb_init_fail;
		}
	} else {
		pr_err("%s: FP ops->lnb_init is NULL\n", __func__);
		goto lnb_init_fail;
	}
ret_ok:
	priv->rpm_suspended = false;

	*obj = priv;
	demod_priv->lnb_h = priv;

#ifdef CONFIG_PM_RUNTIME
	/* Increment device usage but not more than 1 */
	if (!atomic_read(&priv->lnb_data->dev.power.usage_count))
		pm_runtime_get_noresume(&priv->lnb_data->dev);

	pm_runtime_set_active(&priv->lnb_data->dev);
	pm_runtime_enable(&priv->lnb_data->dev);
	pm_runtime_put_sync_suspend(&priv->lnb_data->dev);
	priv->rpm_suspended = true;
#endif

	return 0;

lnb_init_fail:
	priv->demod_h = NULL;
	lnb_uninstall(priv);
lnb_install_fail:
	stm_fe_free((void **)&priv->ops);
lnb_ops_malloc_fail:
	stm_fe_free((void **)&priv->lnb_h);
	return ret;
}
EXPORT_SYMBOL(stm_fe_lnb_new);

/*
 * Name: stm_fe_lnb_delete()
 *
 * Description:deletes a lnb control object  releasing all associated resources
 */
int32_t stm_fe_lnb_delete(stm_fe_lnb_h obj)
{
	int ret = 0, error = 0;
	struct stm_fe_lnb_s *priv = obj;
	struct stm_fe_demod_s *demod_priv;

	if (!priv) {
		pr_err("%s: lnb obj is not initialised\n", __func__);
		return -EINVAL;
	}

	if (!priv->ops) {
		pr_err("%s: lnb object is  not allocated\n", __func__);
		return -ENODEV;
	}

	demod_priv = (struct stm_fe_demod_s *)priv->demod_h;
#ifdef CONFIG_PM_RUNTIME
	/* Decrement device usage but not less than 0 */
	if (atomic_read(&priv->lnb_data->dev.power.usage_count) > 0)
		pm_runtime_put_noidle(&priv->lnb_data->dev);

		pm_runtime_put_sync_suspend(&priv->lnb_data->dev);
		pm_runtime_set_suspended(&priv->lnb_data->dev);
		pm_runtime_disable(&priv->lnb_data->dev);
		priv->rpm_suspended = true;
#endif
	if (priv->ops->term) {
		ret = priv->ops->term(priv);
		if (ret) {
			pr_err("%s: unable to terminate lnb\n", __func__);
			error = ret;
		}
	} else {
		pr_err("%s: FP lnb_ops->lnb_term is NULL\n", __func__);
		error = -EFAULT;
	}

	ret = lnb_uninstall(priv);
	if (ret) {
		pr_err("%s: lnb_uninstall failed\n", __func__);
		error = ret;
	}
	stm_fe_free((void **)&priv->ops);
	stm_fe_free((void **)&priv->lnb_h);
	demod_priv->lnb_h = NULL;
	priv->demod_h = NULL;

	return error;
}
EXPORT_SYMBOL(stm_fe_lnb_delete);

/*
 * Name: stm_fe_lnb_set_config()
 *
 * Description: configure a LNB device
 */
int32_t stm_fe_lnb_set_config(stm_fe_lnb_h obj, stm_fe_lnb_config_t *lnbconfig)
{
	int ret = -1;
	struct lnb_config_t *lnbconfig_p;
	struct stm_fe_lnb_s *priv;
	struct stm_fe_demod_s *demod_priv;
	struct stm_fe_diseqc_s *diseqc_priv;

	if (!obj) {
		pr_err("%s: lnb object not initialized\n", __func__);
		return -EINVAL;
	}
	priv = (struct stm_fe_lnb_s *)obj;

	if (!priv->demod_h) {
		pr_err("%s: No valid demod handle with lnb\n", __func__);
		return -EFAULT;
	}
	demod_priv = priv->demod_h;
	diseqc_priv = demod_priv->diseqc_h;

	if ((demod_priv->rf_matrix_h) &&
			(priv->lnb_id != demod_priv->rf_src_selected)) {
		priv = lnb_from_id(demod_priv->rf_src_selected);
		if (!priv) {
			pr_err("%s: invalid target lnb handle\n", __func__);
			return -EFAULT;
		}
	}

	if (!priv->ops) {
		pr_err("%s: lnb object not allocated\n", __func__);
		return -ENODEV;
	}

	if (!priv->lnb_h) {
		pr_err("%s: lnb obj not initialized or terminated\n", __func__);
		return -EFAULT;
	}
	if (!diseqc_priv) {
		pr_err("%s: diseqc obj uninitialized\n", __func__);
		return -EFAULT;
	}

#ifdef CONFIG_PM_RUNTIME
	if (priv->lnb_data->dev.power.runtime_status == RPM_SUSPENDED) {
		if (priv->rpm_suspended == true)
			priv->rpm_suspended = false;
		else
			return -EPERM;
	}

	/* Increment device usage but not more than 1 */
	if (!atomic_read(&priv->demod_h->demod_data->dev.power.usage_count)) {
		pm_runtime_get_sync(&priv->demod_h->demod_data->dev);
		priv->demod_h->rpm_suspended = false;
	}
	/* Increment device usage but not more than 1 */
	if (!atomic_read(&diseqc_priv->diseqc_data->dev.power.usage_count)) {
		pm_runtime_get_sync(&diseqc_priv->diseqc_data->dev);
		diseqc_priv->rpm_suspended = false;
	}
	/* Increment device usage but not more than 1 */
	if (!atomic_read(&priv->lnb_data->dev.power.usage_count)) {
		pm_runtime_get_sync(&priv->lnb_data->dev);
		priv->rpm_suspended = false;
	}
#endif

	lnbconfig_p = (struct lnb_config_t *)priv->lnb_h->pData;

	if (lnbconfig->polarization != STM_FE_LNB_PLR_DEFAULT)
		lnbconfig_p->polarization = lnbconfig->polarization;

	if (lnbconfig->lnb_tone_state != STM_FE_LNB_TONE_DEFAULT)
		lnbconfig_p->tonestate = lnbconfig->lnb_tone_state;

	if (lnbconfig->polarization != STM_FE_LNB_PLR_OFF)
		lnbconfig_p->status = LNB_STATUS_ON;

	if (!priv->ops->setconfig) {
		pr_err("%s: FP lnb_setconfig is NULL\n", __func__);
		return -EFAULT;
	} else {
		ret = priv->ops->setconfig(priv, lnbconfig_p);
	}

	return ret;
}
EXPORT_SYMBOL(stm_fe_lnb_set_config);

#ifdef CONFIG_PM
/*
 * Name: stm_fe_lnb_suspend()
 *
 * Description: put lnb in (standby)low power state.
 */
int32_t stm_fe_lnb_suspend(struct device *dev)
{
	int32_t ret = 0;
	int32_t id;
	struct stm_fe_lnb_s *priv = NULL;
	char *lname;
	/* Get the configuration information */
	id = to_platform_device(dev)->id;
	priv = lnb_from_id(id);
	lname = priv->lnb_name;

	if (!priv) {
		pr_warn("%s: err obj %s not intialized\n", __func__, lname);
		return ret;
	}

	if (!priv->ops) {
		pr_warn("%s: lnb obj not allocated...skipping\n", __func__);
		return ret;
	}

	pr_info("%s: obj %s suspending in low power mode\n", __func__, lname);

	if ((priv->rpm_suspended) || (priv->bypass_control))
		return ret;

	priv->lnbparams.status = LNB_STATUS_OFF;
	if (priv->ops->suspend) {
		ret = priv->ops->suspend(priv);
		if (ret) {
			pr_err("%s: lnb suspend failed: %d\n", __func__, ret);
			return ret;
		}
	} else {
		pr_err("%s: FP lnb_suspend is NULL\n", __func__);
		return -EFAULT;
	}

	return ret;
}

/*
 * Name: stm_fe_lnb_resume()
 *
 * Description: resume lnb in normal power state.
 */
int32_t stm_fe_lnb_resume(struct device *dev)
{
	int32_t ret = 0;
	int32_t id;
	char *lname;
	struct stm_fe_lnb_s *priv = NULL;

	/* Get the configuration information */
	id = to_platform_device(dev)->id;
	priv = lnb_from_id(id);
	lname = priv->lnb_name;
	if (!priv) {
		pr_warn("%s: err object %s not intialized\n", __func__, lname);
		return ret;
	}

	if (!priv->ops) {
		pr_warn("%s: lnb obj is not allocated...skipping\n", __func__);
		return ret;
	}

	pr_info("%s: obj %s resuming in normal power mode\n", __func__, lname);

	if (priv->bypass_control)
		return ret;

	priv->lnbparams.status = LNB_STATUS_ON;
	if (priv->ops->resume) {
		ret = priv->ops->resume(priv);
		if (ret) {
			pr_err("%s: lnb resume failed: %d\n", __func__, ret);
			return ret;
		}
	} else {
		pr_err("%s: FP lnb_resume is NULL\n", __func__);
		return -EFAULT;
	}

	return ret;
}

/*
 * Name: stm_fe_lnb_freeze()
 *
 * Description: put lnb in (standby)low power state.
 */
int32_t stm_fe_lnb_freeze(struct device *dev)
{
	int32_t ret = 0;
	int32_t id;
	char *lname;
	struct stm_fe_lnb_s *priv = NULL;

	/* Get the configuration information */
	id = to_platform_device(dev)->id;
	priv = lnb_from_id(id);
	lname = priv->lnb_name;

	if (!priv) {
		pr_warn("%s: err object %s not intialized\n", __func__, lname);
		return ret;
	}

	if (!priv->ops) {
		pr_warn("%s: lnb object not allocated...skipping\n", __func__);
		return ret;
	}

	pr_info("%s: obj %s suspending in low power mode\n", __func__, lname);

	if ((priv->rpm_suspended) || (priv->bypass_control))
		return ret;

	priv->lnbparams.status = LNB_STATUS_OFF;
	if (priv->ops->setconfig) {
		ret = priv->ops->setconfig(priv, &priv->lnbparams);
		if (ret) {
			pr_err("%s: lnb_setconfig failed: %d\n", __func__, ret);
			return ret;
		}
	} else {
		pr_err("%s: FP lnb_setconfig is NULL\n", __func__);
		return -EFAULT;
	}

	return ret;
}

/*
 * Name: stm_fe_lnb_restore()
 *
 * Description: resume lnb in normal power state.
 */
int32_t stm_fe_lnb_restore(struct device *dev)
{
	int32_t ret = 0;
	int32_t id;
	char *lname;
	struct stm_fe_lnb_s *priv = NULL;

	/* Get the configuration information */
	id = to_platform_device(dev)->id;
	priv = lnb_from_id(id);
	lname = priv->lnb_name;

	if (!priv) {
		pr_warn("%s: err obj %s not intialized\n", __func__, lname);
		return ret;
	}

	if (!priv->ops) {
		pr_warn("%s: lnb object not allocated...skipping\n", __func__);
		return ret;
	}

	pr_info("%s: obj %s resuming in normal power mode\n", __func__, lname);

	if (priv->bypass_control)
		return ret;

	priv->lnbparams.status = LNB_STATUS_ON;
	if (priv->ops->setconfig) {
		ret = priv->ops->setconfig(priv, &priv->lnbparams);
		if (ret) {
			pr_err("%s: lnb_setconfig failed: %d\n", __func__, ret);
			return ret;
		}
	} else {
		pr_err("%s: FP lnb_setconfig is NULL\n", __func__);
		return -EFAULT;
	}

	return ret;
}
#endif

struct stm_fe_lnb_s *lnb_from_name(const char *name)
{
	struct list_head *pos;
	list_for_each(pos, get_lnb_list()) {
		struct stm_fe_lnb_s *priv;
		priv = list_entry(pos, struct stm_fe_lnb_s, list);
		if (!strcmp(priv->lnb_name, name))
			return priv;
	}

	return NULL;
}

struct stm_fe_lnb_s *lnb_from_id(uint32_t id)
{
	struct list_head *pos;
	list_for_each(pos, get_lnb_list()) {
		struct stm_fe_lnb_s *priv;
		priv = list_entry(pos, struct stm_fe_lnb_s, list);
		if (priv->lnb_id == id)
			return priv;
	}

	return NULL;
}
