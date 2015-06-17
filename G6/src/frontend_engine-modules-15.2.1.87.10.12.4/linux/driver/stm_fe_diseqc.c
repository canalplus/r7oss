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

Source file name : stm_fe_diseqc.c
Author :           Rahul.V

API dedinitions for DISEQC device

Date        Modification                                    Name
----        ------------                                    --------
16-Jun-11   Created                                         Rahul.V
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
#include "stm_fe_diseqc.h"
#include "stm_fe_engine.h"
#include "stm_fe_install.h"
#include <linux/pm_runtime.h>
#include <linux/suspend.h>

static int diseqc_dev_id;
#ifdef CONFIG_OF
static void *stm_diseqc_get_pdata(struct platform_device *pdev)
{
	int ret = 0;
	struct device_node *np = pdev->dev.of_node;
	struct device_node *fe;
	struct device_node *iter;
	struct diseqc_config_s *conf;
	const char *name;
	const char *lla;

	conf = devm_kzalloc(&pdev->dev, sizeof(*conf), GFP_KERNEL);
	if (!conf) {
		pr_err("%s: diseqc platform data alloc failed\n", __func__);
		ret = -ENOMEM;
		goto diseqc_pdata_err;
	}
	fe = of_get_parent(np);
	if (!fe) {
		ret = -EINVAL;
		goto diseqc_pdata_err;
	}
	ret = of_property_read_u32(fe, "control", &conf->ctrl);
	if (ret)
		goto diseqc_pdata_err;
	iter = of_parse_phandle(np, "diseqc", 0);
	if (!iter) {
		ret = -EINVAL;
		goto diseqc_pdata_err;
	}
	ret = of_property_read_string(iter, "model", &name);
	if (ret)
		goto diseqc_pdata_err;
	strlcpy(conf->diseqc_name, name, DISEQC_NAME_MAX_LEN);

	ret = of_property_read_string(iter, "lla_driver", &lla);
	if (ret)
		goto diseqc_pdata_err;
	if ((strncmp(lla, "st,lla", sizeof("st,lla")))
			&& (strncmp(lla, "custom,lla", sizeof("custom,lla")))) {
		pr_err("%s: failed to select diseqc lla\n", __func__);
		goto diseqc_pdata_err;
	}
	strlcpy(conf->lla, lla, DISEQC_NAME_MAX_LEN);
	ret = of_property_read_u32(iter, "diseqc_ver", &conf->ver);
	if (ret)
		goto diseqc_pdata_err;
	of_property_read_u32(iter, "diseqc_custom_option", &conf->cust_flags);

diseqc_pdata_err:
	if (ret) {
		pr_err("%s: diseqc config not properly set\n", __func__);
		return ERR_PTR(ret);
	}

	return conf;
}
#else
static void *stm_diseqc_get_pdata(struct platform_device *stm_fe_demod_data)
{
	return NULL;
}
#endif

/*
 * Name: stm_fe_diseqc_probe()
 *
 * Description:get the configuration information of diseqc
 */
int stm_fe_diseqc_probe(struct platform_device *pdev)
{
	struct stm_platdev_data *plat_dev_data_p;
	struct diseqc_config_s *conf;
	struct stm_fe_diseqc_s *priv;

	priv = stm_fe_malloc(sizeof(struct stm_fe_diseqc_s));
	if (!priv)
		return -ENOMEM;
	/* Get the configuration information about the diseqc */
	if (pdev->dev.of_node) {
		conf = stm_diseqc_get_pdata(pdev);
		if (IS_ERR_OR_NULL(conf)) {
			stm_fe_free((void **)&priv);
			return PTR_ERR(conf);
		}
		priv->diseqc_id = diseqc_dev_id;
		pdev->id = diseqc_dev_id;
		diseqc_dev_id++;
		priv->dt_enable = true;
	} else {
		plat_dev_data_p = pdev->dev.platform_data;
		conf = plat_dev_data_p->config_data;
		priv->diseqc_id = pdev->id;
		priv->dt_enable = false;
	}

	list_add(&priv->list, get_diseqc_list());
	snprintf(priv->diseqc_name, sizeof(priv->diseqc_name),
					   "stm_fe_diseqc.%d", priv->diseqc_id);
	priv->config = conf;
	priv->diseqc_data = pdev;

	incr_obj_cnt();

	pr_info("%s: Config diseqc %s\n", __func__, priv->diseqc_name);

	return 0;
}

/*
 * Name: stm_fe_diseqc_remove()
 *
 * Description:remove the configuration information of diseqc
 */
int stm_fe_diseqc_remove(struct platform_device *pdev)
{
	struct stm_fe_diseqc_s *priv = diseqc_from_id(pdev->id);

	if (priv) {
		list_del(&priv->list);
		stm_fe_free((void **)&priv);
		decr_obj_cnt();
	} else {
		pr_err("%s: diseqc obj is not initialised\n", __func__);
		return -ENODEV;
	}
	return 0;
}

/*
 * Name: stm_fe_diseqc_new()
 *
 * Description:allocate a new diseqc control object
 */
int32_t stm_fe_diseqc_new(const char *name, stm_fe_demod_h demod_obj,
		stm_fe_diseqc_h *obj)
{
	int ret = -1;
	struct stm_fe_diseqc_s *priv = diseqc_from_name(name);
	struct stm_fe_demod_s *demod_priv = demod_obj;

	*obj = NULL;

	if (!demod_priv) {
		pr_err("%s: demod obj is not initialised\n", __func__);
		return -EINVAL;
	}

	if (!priv) {
		pr_err("%s: %s absent in diseqclist\n", __func__, name);
		return -ENODEV;
	}

	if (priv->ops) {
		pr_err("%s: %s already exists\n", __func__, name);
		return -EEXIST;
	}

	priv->ops = stm_fe_malloc(sizeof(struct diseqc_ops_s));
	if (!priv->ops) {
		pr_err("%s: mem alloc failed for diseqc FP\n", __func__);
		return -ENOMEM;
	}
	if (priv->dt_enable)
		ret = diseqc_install_dt(priv);
	else
		ret = diseqc_install(priv);
	if (ret) {
		pr_err("%s: No diseqc is avaialable to install\n", __func__);
		ret = -ENODEV;
		goto diseqc_install_fail;
	}

	priv->demod_h = demod_obj;

	/* Control through Linux DVB; Not necessary to proceed further */
	if (priv->bypass_control) {
		pr_info("%s: Diseqc Control bypassed..\n", __func__);
		goto ret_ok;
	}

	/*Initialization of diseqc */
	if (priv->ops->init) {
		ret = priv->ops->init(priv);
		if (ret) {
			pr_err("%s: diseqc init failed", __func__);
			goto diseqc_init_fail;
		}
	} else {
		pr_err("%s: FP diseqc_init is NULL\n", __func__);
		ret = -EFAULT;
		goto diseqc_init_fail;
	}

ret_ok:
	priv->rpm_suspended = false;

	*obj = priv;
	demod_priv->diseqc_h = priv;

#ifdef CONFIG_PM_RUNTIME
	/* Increment device usage but not more than 1 */
	if (!atomic_read(&priv->diseqc_data->dev.power.usage_count))
		pm_runtime_get_noresume(&priv->diseqc_data->dev);

	pm_runtime_set_active(&priv->diseqc_data->dev);
	pm_runtime_enable(&priv->diseqc_data->dev);
	pm_runtime_put_sync_suspend(&priv->diseqc_data->dev);
	priv->rpm_suspended = true;
#endif
	return 0;

diseqc_init_fail:
	priv->demod_h = NULL;
	diseqc_uninstall(priv);
diseqc_install_fail:
	stm_fe_free((void **)&priv->ops);
	return ret;
}
EXPORT_SYMBOL(stm_fe_diseqc_new);

/*
 * Name: stm_fe_diseqc_delete()
 *
 * Description:delete a diseqc control object
 */
int32_t stm_fe_diseqc_delete(stm_fe_diseqc_h obj)
{
	int ret = 0, error = 0;
	struct stm_fe_diseqc_s *priv = obj;
	struct stm_fe_demod_s *demod_priv;

	if (!priv) {
		pr_err("%s: diseqc obj is not initialised\n", __func__);
		return -EINVAL;
	}

	if (!priv->ops) {
		pr_err("%s: diseqc object is not allocated\n", __func__);
		return -ENODEV;
	}

	demod_priv = (struct stm_fe_demod_s *)priv->demod_h;
#ifdef CONFIG_PM_RUNTIME
	/* Decrement device usage but not less than 0 */
	if (atomic_read(&priv->diseqc_data->dev.power.usage_count) > 0)
		pm_runtime_put_noidle(&priv->diseqc_data->dev);
		pm_runtime_put_sync_suspend(&priv->diseqc_data->dev);
		pm_runtime_set_suspended(&priv->diseqc_data->dev);
		pm_runtime_disable(&priv->diseqc_data->dev);
		priv->rpm_suspended = true;
#endif
	if (priv->ops->term) {
		ret = priv->ops->term(priv);
		if (ret) {
			pr_err("%s: unable to terminate diseqc\n", __func__);
			error = ret;
		}
	} else {
		pr_err("%s: diseqc_ops->diseqc_term is NULL\n", __func__);
		error = -EFAULT;
	}
	if (ret) {
		pr_err("%s: diseqc uninstall failed\n", __func__);
		error = ret;
	}
	demod_priv->diseqc_h = NULL;
	priv->demod_h = NULL;
	ret = diseqc_uninstall(priv);
	stm_fe_free((void **)&priv->ops);
	obj = NULL;

	return ret;
}
EXPORT_SYMBOL(stm_fe_diseqc_delete);

/*
 * Name: stm_fe_diseqc_transfer()
 *
 * Description:send/receive diseqc commands and  different tone signals
 */
int32_t stm_fe_diseqc_transfer(stm_fe_diseqc_h obj, stm_fe_diseqc_mode_t mode,
		stm_fe_diseqc_msg_t *msg, uint32_t num)
{
	int ret = -1;
	int i = 0;
	struct stm_fe_diseqc_s *priv = obj;
	struct stm_fe_lnb_s *lnb_priv;
	struct platform_device *diseqc_data;
	struct stm_fe_demod_s *demod_priv;

	if (!priv) {
		pr_err("%s: diseqc object is not initialized\n", __func__);
		return -EINVAL;
	}

	demod_priv = priv->demod_h;
	if (!demod_priv) {
		pr_err("%s: demod object is not initialized\n", __func__);
		return -ENODEV;
	}
	if ((demod_priv->rf_matrix_h) &&
			(priv->diseqc_id != demod_priv->rf_src_selected)) {
		priv = diseqc_from_id(demod_priv->rf_src_selected);
		if (!priv) {
			pr_err("%s: invalid target diseqc handle\n", __func__);
			return -EFAULT;
		}
		demod_priv = priv->demod_h;
		if (!demod_priv) {
			pr_err("%s: invalid target demod handle\n", __func__);
			return -EFAULT;
		}
	}

	/* Temporary fix to prevent crash in case of NIM mismatch.
	 * Separate implementation in diseqc_stv0XXX.c or some alternate
	 * solution to fix crash */
	lnb_priv = (struct stm_fe_lnb_s *)demod_priv->lnb_h;
	if (!lnb_priv) {
		pr_err("%s: lnb object is not initialized\n", __func__);
		return -ENODEV;
	}

	if (!priv->ops) {
		pr_err("%s: diseqc object is not allocated\n", __func__);
		return -ENODEV;
	}

	diseqc_data = priv->diseqc_data;


#ifdef CONFIG_PM_RUNTIME
	if (diseqc_data->dev.power.runtime_status == RPM_SUSPENDED) {
		if (priv->rpm_suspended == true)
			priv->rpm_suspended = false;
		else
			return -EPERM;
	}

	/* Increment device usage but not more than 1 */
	if (!atomic_read(&lnb_priv->lnb_data->dev.power.usage_count)) {
		pm_runtime_get_sync(&lnb_priv->lnb_data->dev);
		lnb_priv->rpm_suspended = false;
	}
	/* Increment device usage but not more than 1 */
	if (!atomic_read(&diseqc_data->dev.power.usage_count)) {
		pm_runtime_get_sync(&diseqc_data->dev);
		priv->rpm_suspended = false;
	}
	/* need to resume demod as this is required for diseqc to be
	* functional */
	if (demod_priv->demod_data->dev.power.runtime_status ==
							RPM_SUSPENDED) {
		if (demod_priv->rpm_suspended == true)
			demod_priv->rpm_suspended = false;
		else
			return -EPERM;
	}
	/* Increment device usage but not more than 1 */
	if (!atomic_read(&demod_priv->demod_data->dev.power.usage_count)) {
		pm_runtime_get_sync(&demod_priv->demod_data->dev);
		demod_priv->rpm_suspended = false;
	}
#endif

	if (!priv->ops->transfer) {
		pr_err("%s: FP (diseqc_transfer) is NULL\n", __func__);
		return -EFAULT;
	}

	while (i < num) {
		if (msg[i].op != STM_FE_DISEQC_TRANSFER_SEND &&
			msg[i].op != STM_FE_DISEQC_TRANSFER_RECEIVE) {
			pr_err("%s: Invalid Op in diseqc msg[]\n", __func__);
			return -EINVAL;
		}
		i++;
	}
	priv->mode = mode;
	ret = priv->ops->transfer(priv, mode, msg, num);
	if (ret) {
		pr_err("%s: diseqc transfer failed\n", __func__);
		return -EFAULT;
	}

	return 0;
}
EXPORT_SYMBOL(stm_fe_diseqc_transfer);

#ifdef CONFIG_PM
/*
 * Name: stm_fe_diseqc_suspend()
 *
 * Description: put diseqc in (standby)low power state.
 */
int32_t stm_fe_diseqc_suspend(struct device *dev)
{
	int32_t ret = 0;
	int32_t id;
	struct stm_fe_diseqc_s *priv = NULL;
	/* Get the configuration information */
	id = to_platform_device(dev)->id;
	priv = diseqc_from_id(id);

	if (!priv) {
		pr_warn("%s: diseqc obj uninitialized....skipping\n", __func__);
		return ret;
	}

	if (!priv->ops) {
		pr_warn("%s: diseqc obj is unallocated...skipping\n", __func__);
		return ret;
	}

	pr_info("%s: diseqc suspending in low power mode\n", __func__);

	/* Check for bypass_control in case we have low power support
	 * for DiSEqC (just like demod, lnb)
	 */

	/* diseqc handling done, return */

	return ret;
}

/*
 * Name: stm_fe_diseqc_resume()
 *
 * Description: resume diseqc in normal power state.
 */
int32_t stm_fe_diseqc_resume(struct device *dev)
{
	int32_t ret = 0;
	int32_t id;
	struct stm_fe_diseqc_s *priv = NULL;
	/* Get the configuration information */
	id = to_platform_device(dev)->id;
	priv = diseqc_from_id(id);

	if (!priv) {
		pr_warn("%s: diseqc obj is uninitialized,skipping\n", __func__);
		return ret;
	}

	if (!priv->ops) {
		pr_warn("%s: diseqc source unallocated,skipping\n", __func__);
		return ret;
	}

	pr_info("%s: diseqc resuming in normal power mode\n", __func__);

	/* Check for bypass_control in case we have low power support
	 * for DiSEqC (just like demod, lnb)
	 */

	/* Nothing to do, return */

	return ret;
}

/*
 * Name: stm_fe_diseqc_freeze()
 *
 * Description: put diseqc in (standby)low power state.
 */
int32_t stm_fe_diseqc_freeze(struct device *dev)
{
	int32_t ret = 0;
	int32_t id;
	struct stm_fe_diseqc_s *priv = NULL;
	/* Get the configuration information */
	id = to_platform_device(dev)->id;
	priv = diseqc_from_id(id);

	if (!priv) {
		pr_warn("%s: diseqc obj uninitialized...skipping\n", __func__);
		return ret;
	}

	if (!priv->ops) {
		pr_warn("%s: diseqc src is unallocated..skipping\n", __func__);
		return ret;
	}

	pr_info("%s: diseqc suspending in low power mode..\n", __func__);

	/* Check for bypass_control in case we have low power support
	 * for DiSEqC (just like demod, lnb)
	 */

	/* diseqc handling done, return */

	return ret;
}

/*
 * Name: stm_fe_diseqc_restore()
 *
 * Description: resume diseqc in normal power state.
 */
int32_t stm_fe_diseqc_restore(struct device *dev)
{
	int32_t ret = 0;
	int32_t id;
	struct stm_fe_diseqc_s *priv = NULL;
	/* Get the configuration information */
	id = to_platform_device(dev)->id;
	priv = diseqc_from_id(id);

	if (!priv) {
		pr_warn("%s: diseqc obj uninitialized,skipping\n", __func__);
		return ret;
	}

	if (!priv->ops) {
		pr_warn("%s: diseqc src is unallocated,skipping\n", __func__);
		return ret;
	}

	pr_info("%s: diseqc resuming in normal power mode\n", __func__);

	/* Check for bypass_control in case we have low power support
	 * for DiSEqC (just like demod, lnb)
	 */

	/* Nothing to do, return */

	return ret;
}
#endif

struct stm_fe_diseqc_s *diseqc_from_name(const char *name)
{
	struct list_head *pos;
	list_for_each(pos, get_diseqc_list()) {
		struct stm_fe_diseqc_s *diseqc;
		diseqc = list_entry(pos, struct stm_fe_diseqc_s, list);
		if (!strcmp(diseqc->diseqc_name, name))
			return diseqc;
	}

	return NULL;
}

struct stm_fe_diseqc_s *diseqc_from_id(uint32_t id)
{
	struct list_head *pos;
	list_for_each(pos, get_diseqc_list()) {
		struct stm_fe_diseqc_s *diseqc;
		diseqc = list_entry(pos, struct stm_fe_diseqc_s, list);
		if (diseqc->diseqc_id == id)
			return diseqc;
	}

	return NULL;
}
