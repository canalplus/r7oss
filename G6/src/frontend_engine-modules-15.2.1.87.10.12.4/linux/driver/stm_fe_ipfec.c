/************************************************************************
Copyright (C) 2012 STMicroelectronics. All Rights Reserved.

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

Source file name : stm_fe_ipfec.c
Author :           SD

API definitions for ip fec device

Date        Modification                                    Name
----        ------------                                    --------
30-Aug-12   Created                                         SD

************************************************************************/
#include <linux/kernel.h>
#include <linux/version.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 2, 2)
#include <linux/export.h>
#else
#include <linux/module.h>
#endif
#include <linux/platform_device.h>
#ifdef CONFIG_OF
#include <linux/of.h>
#endif
#include <linux/err.h>
#include <linux/stm/plat_dev.h>
#include <stm_fe.h>
#include <i2c_wrapper.h>
#include "stm_fe_ip.h"
#include "stm_fe_ipfec.h"
#include "stm_fe_demod.h"
#include "stm_fe_engine.h"
#include "stm_fe_ip_install.h"
#include "ip/ipfec/stats/stats.h"

extern struct stm_fe_engine_s stm_fe_engine;
static DEFINE_MUTEX(ipfec_init_term_mutex);

static int ipfec_dev_id;
#ifdef CONFIG_OF
static void *stm_ipfec_get_pdata(struct platform_device *pdev)
{
	int ret = 0;
	struct device_node *np = pdev->dev.of_node;
	struct ipfec_config_s *conf;
	const char *addr;

	conf = devm_kzalloc(&pdev->dev, sizeof(*conf), GFP_KERNEL);
	if (!conf) {
		pr_err("%s: ipfec platform data alloc failed\n", __func__);
		ret = -ENOMEM;
		goto ipfec_pdata_err;
	}
	strlcpy(conf->ipfec_name, "IPFEC-STM_FE", IPFEC_NAME_MAX_LEN);
	ret = of_property_read_string(np, "ipfec-addr", &addr);
	if (ret)
		goto ipfec_pdata_err;
	strlcpy(conf->ipfec_addr, addr, IPFEC_ADDR_LEN);
	ret = of_property_read_u32(np, "ipfec-port", &conf->ipfec_port);
	if (ret)
		goto ipfec_pdata_err;
	ret = of_property_read_u32(np, "ipfec-scheme", &conf->ipfec_scheme);
	if (ret)
		goto ipfec_pdata_err;

ipfec_pdata_err:
	if (ret) {
		pr_err("%s: ipfec configuration not properly set\n", __func__);
		return ERR_PTR(ret);
	}

	return conf;
}
#else
static void *stm_ipfec_get_pdata(struct platform_device *stm_fe_demod_data)
{
	return NULL;
}
#endif

/*
 * Name: stm_fe_ipfec_probe()
 *
 * Description:get the configuration information of ip fec object
 */
int stm_fe_ipfec_probe(struct platform_device *stm_fe_ipfec_data)
{
	struct stm_platdev_data *plat_dev_data_p;
	struct ipfec_config_s *conf;
	struct stm_fe_ipfec_s *priv;

	priv = stm_fe_malloc(sizeof(struct stm_fe_ipfec_s));
	if (!priv)
		return -ENOMEM;
	/* Get the configuration information about the ip fec */
	if (stm_fe_ipfec_data->dev.of_node) {
		conf = stm_ipfec_get_pdata(stm_fe_ipfec_data);
		if (IS_ERR_OR_NULL(conf)) {
			stm_fe_free((void **)&priv);
			return PTR_ERR(conf);
		}
		priv->ipfec_id = ipfec_dev_id;
		stm_fe_ipfec_data->id = ipfec_dev_id;
		ipfec_dev_id++;
	} else {
		plat_dev_data_p = stm_fe_ipfec_data->dev.platform_data;
		conf = plat_dev_data_p->config_data;
		priv->ipfec_id = stm_fe_ipfec_data->id;
	}

	list_add(&priv->list, &stm_fe_engine.ipfec_list);
	priv->config = conf;
	snprintf(priv->ipfec_name, sizeof(priv->ipfec_name),
					     "stm_fe_ipfec.%d", priv->ipfec_id);

	stm_fe_engine.obj_cnt++;

	pr_info("%s: Configuring ip %s\n", __func__, priv->ipfec_name);

	return 0;
}

/*
 * Name: stm_fe_ipfec_remove()
 *
 * Description:remove the configuration information of ipfec
 */
int stm_fe_ipfec_remove(struct platform_device *stm_fe_ipfec_data)
{
	struct stm_fe_ipfec_s *priv = ipfec_from_id(stm_fe_ipfec_data->id);

	if (priv) {
		list_del(&priv->list);
		stm_fe_free((void **)&priv);
		stm_fe_engine.obj_cnt--;
	} else {
		pr_err("%s: ipfec obj is not initialised\n", __func__);
		return -ENODEV;
	}

	return 0;
}

/*
 * Name: stm_fe_ip_fec_new()
 *
 * Description:allocates a new ip fec control object
 */
int32_t stm_fe_ip_fec_new(const char *name, stm_fe_ip_h ip_object,
		stm_fe_ip_fec_config_t *fec_params, stm_fe_ip_fec_h *obj)
{
	int ret = -1;
	struct stm_fe_ipfec_s  *priv = ipfec_from_name(name);
	struct stm_fe_ip_s *ip_priv = ip_object;

	*obj = NULL;

	if (!priv) {
		pr_err("%s: '%s' not present in ipfec list\n", __func__, name);
		return -ENODEV;
	}

	if (priv->ops) {
		pr_err("%s: '%s' already exists\n", __func__, name);
		return -EEXIST;
	}

	/*Memory Allocations for STMFE internal structures */
	priv->ops = stm_fe_malloc(sizeof(struct ipfec_ops_s));

	if (!priv->ops) {
		pr_err("%s: mem alloc for ip FE fec failed\n", __func__);
		return -ENOMEM;
	}

	/*stored ip object address */
	ret = ipfec_install(priv);
	if (ret) {
		pr_err("%s: No FE IP avaialable to install\n", __func__);
		ret = -ENODEV;
		goto ipfec_install_fail;
	}

	priv->ip_h = ip_object;

	/*Initialization of fe ip */
	if (priv->ops->init) {
		ret = priv->ops->init(priv, fec_params);
		if (ret) {
			pr_err("%s: ipfec_init failed\n", __func__);
			goto ipfec_init_fail;
		}
	} else {
		pr_err("%s: FP ipfec_init is NULL\n", __func__);
		ret = -EFAULT;
		goto ipfec_init_fail;
	}

	ret = stm_registry_add_instance(STM_REGISTRY_INSTANCES,
			(stm_object_h *)&stm_fe_engine.ip_obj_type, name,
			(stm_object_h)priv);
	if (ret) {
		pr_err("%s: registry_add_instance failed= %d\n", __func__, ret);
		goto add_registery_fail;
	}


	*obj = priv;
	ip_priv->ipfec_h = *obj;

	/*stats init: ref sysfs*/
	if (stm_ipfec_stats_init(priv))
		pr_err("%s: ipfec_stats init failed= %d\n", __func__, ret);

	return 0;

add_registery_fail:
	if (priv->ops->term) {
		mutex_lock(&ipfec_init_term_mutex);
		ret = priv->ops->term(priv);
		if (ret)
			pr_err("%s: ipfec_term failed\n", __func__);
		mutex_unlock(&ipfec_init_term_mutex);
	} else {
		pr_err("%s: FP ops->ipfec_term is NULL\n", __func__);
	}
ipfec_init_fail:
	priv->ip_h = NULL;
	ipfec_uninstall(priv);
ipfec_install_fail:
	stm_fe_free((void **)&priv->ops);
	return ret;
}
EXPORT_SYMBOL(stm_fe_ip_fec_new);

/*
 * Name: stm_fe_ip_fec_delete()
 *
 * Description: delete a ip source object
 */
int32_t stm_fe_ip_fec_delete(stm_fe_ip_fec_h obj)
{
	int ret = 0, error = 0;

	struct stm_fe_ipfec_s *priv = NULL;
	struct stm_fe_ip_s *ip_priv = NULL;

	if (!obj) {
		pr_err("%s: ip fec is not initialized\n", __func__);
		return -EINVAL;
	}

	priv = (struct stm_fe_ipfec_s *)obj;
	ip_priv = (struct stm_fe_ip_s *)priv->ip_h;

	if (!priv->ops) {
		pr_err("%s: ip fec source not allocated\n", __func__);
		return -ENODEV;
	}

	ret = stm_ipfec_stats_term(priv);
	if (ret)
		pr_err("%s: ipfec_stats_term failed: %d\n", __func__, ret);

	ret = stm_registry_remove_object((stm_object_h) obj);
	if (ret) {
		pr_err("%s: reg remove_object failed: %d\n", __func__, ret);
		error = ret;
	}

	if (priv->ops->term) {
		mutex_lock(&ipfec_init_term_mutex);
		ret = priv->ops->term(priv);
		mutex_unlock(&ipfec_init_term_mutex);
		if (ret) {
			pr_err("%s: ops->term failed\n", __func__);
			error = ret;
		}
	} else {
		pr_err("%s: FP ops->term is NULL\n", __func__);
		error = -EFAULT;
	}
	if (ipfec_uninstall(priv))
		pr_err("%s: ipfec uninstall failed\n", __func__);

	stm_fe_free((void **)&priv->ops);
	priv->ip_h = NULL;
	obj = NULL;
	ip_priv->ipfec_h = NULL;

	return error;
}
EXPORT_SYMBOL(stm_fe_ip_fec_delete);

/*
 * Name: stm_fe_ip_fec_set_compound_control()
 *
 * Description: sets the controlling parameters on the run time
 */
int32_t stm_fe_ip_fec_set_compound_control(stm_fe_ip_fec_h obj,
				stm_fe_ip_fec_control_t selector, void *args)
{
	int ret = -1;
	struct stm_fe_ipfec_s *priv = NULL;

	if (!obj) {
		pr_err("%s: ipfe_fec not initialized\n", __func__);
		return -EINVAL;
	}

	priv = (struct stm_fe_ipfec_s *)obj;
	if (!priv->ops) {
		pr_err("%s: ipfe_fec source is not allocated\n", __func__);
		return -ENODEV;
	}

	if (priv->ops->set_control) {
		ret = priv->ops->set_control(priv, selector, args);
		if (ret)
			pr_err("%s: set control params failed\n", __func__);
	} else {
		pr_err("%s: FP ipfec_set_control is NULL\n", __func__);
		ret = -EFAULT;
	}

	return ret;
}
EXPORT_SYMBOL(stm_fe_ip_fec_set_compound_control);

/*
 * Name: stm_fe_ip_fec_get_compound_control()
 *
 * Description: gets the controlling parameters on the run time
 */
int32_t stm_fe_ip_fec_get_compound_control(stm_fe_ip_fec_h obj,
				stm_fe_ip_fec_control_t selector, void *args)

{
	int ret = -1;
	struct stm_fe_ipfec_s *priv;

	if (!obj) {
		pr_err("%s: ipfe_fec not initialized\n", __func__);
		return -EINVAL;
	}

	priv = (struct stm_fe_ipfec_s *)obj;
	if (!priv->ops) {
		pr_err("%s: ipfe_fec source not allocated\n", __func__);
			return -ENODEV;
	}

	if (priv->ops->get_control) {
		ret = priv->ops->get_control(priv, selector, args);
		if (ret)
			pr_err("%s: unable to get control params\n", __func__);
	} else {
		pr_err("%s: FP ipfec_set_control is NULL\n", __func__);
		ret = -EFAULT;
	}

	return ret;
}
EXPORT_SYMBOL(stm_fe_ip_fec_get_compound_control);

/*
 * Name: stm_fe_ip_fec_get_stats()
 *
 * Description: This function gives statistical information
 *              (regarding packet reception).
 */

int32_t stm_fe_ip_fec_get_stats(stm_fe_ip_fec_h obj,
						stm_fe_ip_fec_stat_t *info_p)
{
	int ret = -1;
	struct stm_fe_ipfec_s *priv = NULL;

	if (!obj || !info_p) {
		pr_err("%s: ipfe_fec obj or info_p is NULL\n", __func__);
		return -EINVAL;
	}

	priv = (struct stm_fe_ipfec_s *)obj;

	if (!priv->ops) {
		pr_err("%s: source is not allocated\n", __func__);
		return -ENODEV;
	}

	if (!priv->ops->get_stats) {
		pr_err("%s: FP ipfec_get_stats is NULL\n", __func__);
		return -EFAULT;
	}

	ret = priv->ops->get_stats(priv, info_p);
	if (ret)
		pr_err("%s: get_stats failed\n", __func__);

	return ret;
}
EXPORT_SYMBOL(stm_fe_ip_fec_get_stats);

struct stm_fe_ipfec_s *ipfec_from_name(const char *name)
{
	struct list_head *pos;
	list_for_each(pos, &stm_fe_engine.ipfec_list) {
	struct stm_fe_ipfec_s *priv;

	priv = list_entry(pos, struct stm_fe_ipfec_s, list);
	if (!strcmp(priv->ipfec_name, name))
		return priv;
	}

	return NULL;
}

struct stm_fe_ipfec_s *ipfec_from_id(uint32_t id)
{
	struct list_head *pos;

	list_for_each(pos, &stm_fe_engine.ipfec_list) {
		struct stm_fe_ipfec_s *priv;
		priv = list_entry(pos, struct stm_fe_ipfec_s, list);
		if (priv->ipfec_id == id)
			return priv;
	}

	return NULL;
}
/*
 * Name: stm_fe_ip_fec_start()
 *
 * Description: This function enables FEC feature.
 */
int32_t stm_fe_ip_fec_start(stm_fe_ip_fec_h obj)
{
	int ret = -1;
	struct stm_fe_ipfec_s *priv = NULL;
	if (!obj) {
		pr_err("%s: ipfe_fec obj or info_p is NULL\n", __func__);
		return -EINVAL;
	}

	priv = (struct stm_fe_ipfec_s *)obj;
	if (!priv->ops) {
		pr_err("%s: source is not allocated\n", __func__);
		return -ENODEV;
	}

	if (!priv->ops->ipfec_start) {
		pr_err("%s: FP ipfec_start is NULL\n", __func__);
		return -EFAULT;
	}
	ret = priv->ops->ipfec_start(priv);
	return ret;
}
EXPORT_SYMBOL(stm_fe_ip_fec_start);

/*
 * Name: stm_fe_ip_fec_stop()
 *
 * Description: This function disables FEC feature
 */
int32_t stm_fe_ip_fec_stop(stm_fe_ip_fec_h obj)
{
	int ret = -1;
	struct stm_fe_ipfec_s *priv = NULL;

	if (!obj) {
		pr_err("%s: ipfe_fec obj or info_p is NULL\n", __func__);
		return -EINVAL;
	}

	priv = (struct stm_fe_ipfec_s *)obj;
	if (!priv->ops) {
		pr_err("%s: source is not allocated\n", __func__);
		return -ENODEV;
	}

	if (!priv->ops->ipfec_stop) {
		pr_err("%s: FP ipfec_stop is NULL\n", __func__);
		return -EFAULT;
	}
	stm_fe_mutex_lock(priv->ip_h->ip_data.start_stop_mtx);
	ret = priv->ops->ipfec_stop(priv);
	if (ret) {
		pr_err("%s: ipfec stop failed\n", __func__);
		stm_fe_mutex_unlock(priv->ip_h->ip_data.start_stop_mtx);
		return ret;
	}
	stm_fe_mutex_unlock(priv->ip_h->ip_data.start_stop_mtx);
	return 0;
}
EXPORT_SYMBOL(stm_fe_ip_fec_stop);
