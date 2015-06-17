/************************************************************************
Copyright (C) 2011,2012 STMicroelectronics. All Rights Reserved.

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

Source file name : stm_fe_ip.c
Author :           SD

API definitions for ip device

Date        Modification                                    Name
----        ------------                                    --------
5-Aug-11   Created                                         SD

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
#include <linux/stm/ip.h>
#include <linux/stm/diseqc.h>
#include <stm_registry.h>
#include <stm_fe.h>
#include <stfe_utilities.h>
#include <stm_fe_os.h>
#include <i2c_wrapper.h>
#include "ip/strm/fe_ip_strm.h"
#include "stm_fe_ip.h"
#include "stm_fe_demod.h"
#include "stm_fe_engine.h"
#include "stm_fe_lnb.h"
#include "stm_fe_diseqc.h"
#include "stm_fe_ip_install.h"
#include "ip/stats/stats.h"
#include <stm_memsrc.h>

extern struct stm_fe_engine_s stm_fe_engine;
static DEFINE_MUTEX(init_term_mutex);

static int ip_dev_id;
#ifdef CONFIG_OF
static void *stm_ipfe_get_pdata(struct platform_device *pdev)
{
	int ret = 0;
	struct device_node *np = pdev->dev.of_node;
	struct ip_config_s *conf;
	const char *addr;
	const char *eth;

	conf =
		devm_kzalloc(&pdev->dev, sizeof(*conf), GFP_KERNEL);
	if (!conf) {
		pr_err("%s: ip platform data alloc failed\n", __func__);
		ret = -ENOMEM;
		goto ip_pdata_err;
	}
	strlcpy(conf->ip_name, "IPFE-STM_FE", IP_NAME_MAX_LEN);
	ret = of_property_read_string(np, "ip-addr", &addr);
	if (ret)
		goto ip_pdata_err;
	strlcpy(conf->ip_addr, addr, IP_ADDR_LEN);
	ret = of_property_read_u32(np, "ip-port", &conf->ip_port);
	if (ret)
		goto ip_pdata_err;
	ret = of_property_read_u32(np, "protocol", &conf->protocol);
	if (ret)
		goto ip_pdata_err;
	ret = of_property_read_string(np, "ethdev", &eth);
	if (ret)
		goto ip_pdata_err;
	strlcpy(conf->ethdev, eth, IP_NAME_MAX_LEN);

ip_pdata_err:
	if (ret) {
		pr_err("%s: ip configuration is not properly set\n", __func__);
		return ERR_PTR(ret);
	}

	return conf;
}
#else
static void *stm_ipfe_get_pdata(struct platform_device *pdev)
{
	return NULL;
}
#endif

/*
 * Name: stm_fe_ip_probe()
 *
 * Description:get the configuration information of fe ip
 */
int stm_fe_ip_probe(struct platform_device *pdev)
{
	struct stm_platdev_data *plat_dev_data_p;
	struct ip_config_s *conf;
	struct stm_fe_ip_s *priv;

	priv = stm_fe_malloc(sizeof(struct stm_fe_ip_s));
	if (!priv) {
		pr_err("%s: ip private date mem alloc failed\n", __func__);
		return -ENOMEM;
	}
	/* Get the configuration information about the tuners */

	if (pdev->dev.of_node) {
		conf = stm_ipfe_get_pdata(pdev);
		if (IS_ERR_OR_NULL(conf)) {
			stm_fe_free((void **)&priv);
			return PTR_ERR(conf);
		}
		priv->ip_id = ip_dev_id;
		pdev->id = ip_dev_id;
		ip_dev_id++;
	} else {
		plat_dev_data_p = pdev->dev.platform_data;
		conf = plat_dev_data_p->config_data;
		priv->ip_id = pdev->id;
	}

	list_add(&priv->list, &stm_fe_engine.ip_list);
	priv->config = conf;
	snprintf(priv->ip_name, sizeof(priv->ip_name),
						   "stm_fe_ip.%d", priv->ip_id);

	stm_fe_engine.obj_cnt++;

	pr_info("%s: Configuring ip %s\n", __func__, priv->ip_name);

	return 0;
}

/*
 * Name: stm_fe_ip_remove()
 *
 * Description:remove the configuration information of ip
 */
int stm_fe_ip_remove(struct platform_device *pdev)
{
	struct stm_fe_ip_s *priv = ip_from_id(pdev->id);

	if (!priv) {
		pr_err("%s: ip fe not initialized\n", __func__);
		return -EINVAL;
	}

	list_del(&priv->list);
	stm_fe_free((void **)&priv);
	stm_fe_engine.obj_cnt--;

	return 0;
}

/*
 * Name: stm_fe_ip_new()
 *
 * Description:allocates a new ip fe source
 */
int32_t stm_fe_ip_new(const char *name, stm_fe_ip_connection_t *connect_params,
						stm_fe_ip_h *obj)
{
	int ret = -1;
	struct stm_fe_ip_s *priv = ip_from_name(name);
	uint8_t i = 0;

	*obj = NULL;

	if (!priv) {
		pr_err("%s: '%s' not present in ip list\n", __func__, name);
		return -ENODEV;
	}

	if (priv->ops) {
		pr_err("%s: '%s' already exists\n", __func__, name);
		return -EEXIST;
	}

	/*Memory Allocations for STMFE internal structures */
	priv->ops = stm_fe_malloc(sizeof(struct ip_ops_s));

	if (!priv->ops) {
		pr_err("%s: ip_ops mem alloc failed\n", __func__);
		return -ENOMEM;
	}

	priv->ip_src = stm_fe_malloc(sizeof(struct stm_ipsrc_s));

	if (!priv->ip_src) {
		pr_err("%s: mem alloc failed for priv->ip_src\n", __func__);
		ret = -ENOMEM;
		goto ip_src_malloc_fail;
	}

	for (i = 0; i < IP_FE_SINK_OBJ_MAX; i++)
		priv->ip_src->sink_obj[i] = NULL;

	/*stored ip object address */
	ret = ip_install(priv);
	if (ret) {
		pr_err("%s: ip_install failed\n", __func__);
		goto ip_install_fail;
	}

	/*Initialization of fe ip */
	if (!priv->ops->init) {
		pr_err("%s: FP ip_init is NULL\n", __func__);
		ret = -ENODEV;
		goto ip_init_fail;
	}
	mutex_lock(&init_term_mutex);
	ret = priv->ops->init(priv, connect_params);
	mutex_unlock(&init_term_mutex);

	if (ret) {
		pr_err("%s: ip_init failed\n", __func__);
		goto ip_init_fail;
	}

	ret = stm_registry_add_instance(STM_REGISTRY_INSTANCES,
				(stm_object_h *)&stm_fe_engine.ip_obj_type,
					name, (stm_object_h)priv);
	if (ret) {
		pr_err("%s: reg add_instance failed= %d\n", __func__, ret);
		goto add_registery_fail;
	}
	*obj = priv;

	/*stats init: ref sysfs*/
	if (stm_ipfe_stats_init(priv))
		pr_err("%s: stm_ipfe_stats init failed = %d\n", __func__, ret);

	return 0;

add_registery_fail:
	if (priv->ops->term) {
		mutex_lock(&init_term_mutex);
		ret = priv->ops->term(priv);
		if (ret)
			pr_err("%s: ip term failed\n", __func__);
		mutex_unlock(&init_term_mutex);
	} else {
		pr_err("%s: ip_ops->ip_term is NULL\n", __func__);
		ret = -EFAULT;
	}

ip_init_fail:
	if (ip_uninstall(priv))
		pr_err("%s: ip uninstall failed\n", __func__);
ip_install_fail:
	stm_fe_free((void **)&priv->ip_src);
ip_src_malloc_fail:
	stm_fe_free((void **)&priv->ops);
	return ret;
}
EXPORT_SYMBOL(stm_fe_ip_new);

/*
 * Name: stm_fe_ip_delete()
 *
 * Description: delete a ip source object
 */
int32_t stm_fe_ip_delete(stm_fe_ip_h obj)
{
	int ret = 0, err = 0;

	struct stm_fe_ip_s *priv;

	if (!obj) {
		pr_err("%s: ip fe is not initialized\n", __func__);
		return -EINVAL;
	}

	priv = (struct stm_fe_ip_s *)obj;

	if (!priv->ops) {
		pr_err("%s: ip fe ops not allocated\n", __func__);
		return -ENODEV;
	}

	if (stm_ipfe_stats_term(priv))
		pr_err("%s: stmfe_stats_term failed: %d\n", __func__, ret);

	if (!priv->ip_src) {
		pr_err("%s: priv->ip_src is NULL\n", __func__);
		return -EFAULT;
	}
	ret = stm_registry_remove_object((stm_object_h) obj);
	if (ret) {
		pr_err("%s: reg remove_object failed: %d\n", __func__, ret);
		err = ret;
	}

	if (priv->ops->term) {
		mutex_lock(&init_term_mutex);
		ret = priv->ops->term(priv);
		if (ret) {
			pr_err("%s: ip_term failed\n", __func__);
			err = ret;
		}
		mutex_unlock(&init_term_mutex);
	} else {
		pr_err("%s: FP ip_ops->ip_term is NULL\n", __func__);
		err = -EFAULT;
	}

	if (ip_uninstall(priv))
		pr_err("%s: ip_uninstall failed\n", __func__);
	stm_fe_free((void **)&priv->ip_src);
	stm_fe_free((void **)&priv->ops);

	return err;
}
EXPORT_SYMBOL(stm_fe_ip_delete);

/*
 * Name: stm_fe_ip_start()
 *
 * Description: starts interception of datagram
 */
int32_t stm_fe_ip_start(stm_fe_ip_h obj)
{
	int ret = 0;
	struct stm_fe_ip_s *priv = NULL;

	if (!obj) {
		pr_err("%s: ip fe not initialized\n", __func__);
		return -EINVAL;
	}

	priv = (struct stm_fe_ip_s *)obj;
	if (!priv->ops) {
		pr_err("%s: ip fe ops not allocated\n", __func__);
		return -ENODEV;
	}

	if (!priv->ops->start) {
		pr_err("%s: FP ops->ip_start is NULL\n", __func__);
		return -EFAULT;
	}
	ret = priv->ops->start(priv);
	if (ret)
		pr_err("%s: unable to start ip\n", __func__);

	return ret;
}
EXPORT_SYMBOL(stm_fe_ip_start);

/*
 * Name: stm_fe_ip_stop()
 *
 * Description: stops interception of datagram
 */
int32_t stm_fe_ip_stop(stm_fe_ip_h obj)
{
	int ret = 0;
	struct stm_fe_ip_s *priv;

	if (!obj) {
		pr_err("%s: ip fe not initialized\n", __func__);
		return -EINVAL;
	}

	priv = (struct stm_fe_ip_s *)obj;
	if (!priv->ops) {
		pr_err("%s: ip fe src not allocated\n", __func__);
		return -ENODEV;
	}

	if (!priv->ops->stop) {
		pr_err("%s: FP ops->ip_stop is NULL\n", __func__);
		return -EFAULT;
	}
	ret = priv->ops->stop(priv);
	if (ret)
		pr_err("%s: unable to stop ip\n", __func__);

	return ret;
}
EXPORT_SYMBOL(stm_fe_ip_stop);

/*
 * Name: stm_fe_ip_set_compound_control()
 *
 * Description: sets the controlling parameters on the run time
 */
int32_t	stm_fe_ip_set_compound_control(stm_fe_ip_h obj,
				stm_fe_ip_control_t value, void *args)
{
	int ret = -1;
	struct stm_fe_ip_s *priv = NULL;

	if (!obj) {
		pr_err("%s: ip fe not initialized\n", __func__);
		return -EINVAL;
	}

	priv = (struct stm_fe_ip_s *)obj;
	if (!priv->ops) {
		pr_err("%s: ip fe src not allocated\n", __func__);
		return -ENODEV;
	}

	if (!priv->ops->set_control) {
		pr_err("%s: FP ops->ip_set_control is NULL\n", __func__);
		return -EFAULT;
	}
	ret = priv->ops->set_control(priv, value, args);
	if (ret)
		pr_err("%s: set control params fe ip failed\n", __func__);

	return ret;
}
EXPORT_SYMBOL(stm_fe_ip_set_compound_control);

/*
 * Name: stm_fe_ip_get_compound_control()
 *
 * Description: gets the controlling parameters on the run time
 */
int32_t	stm_fe_ip_get_compound_control(stm_fe_ip_h obj,
				stm_fe_ip_control_t value, void *args)
{
	int ret = -1;
	struct stm_fe_ip_s *priv;

	priv = (struct stm_fe_ip_s *)obj;

	if (!priv) {
		pr_err("%s: ip fe not initialized\n", __func__);
		return -EINVAL;
	}

	if (!priv->ops) {
		pr_err("%s: ip fe src not allocated\n", __func__);
		return -ENODEV;
	}

	if (!priv->ops->get_control) {
		pr_err("%s: FP ops->ip_get_control is NULL\n", __func__);
		return -EFAULT;
	}
	ret = priv->ops->get_control(priv, value, args);
	if (ret)
		pr_err("%s: unable to get ctrl params ip\n", __func__);

	return ret;
}
EXPORT_SYMBOL(stm_fe_ip_get_compound_control);

int ip_fe_pull_data(stm_object_h obj, struct stm_data_block
		*data_block, uint32_t block_count, uint32_t *filled_blocks)
{
	struct stm_fe_ip_s *priv = (struct stm_fe_ip_s *) obj;
	uint32_t data_remaining = 0, bytes_read = 0, total_read = 0;
	struct stm_data_block tmp_data;
	int ret = -1;

	if (!priv || !data_block) {
		pr_err("%s: ptr priv|data_block is NULL\n", __func__);
		return -EINVAL;
	}

	while (block_count) {
		bytes_read = 0;
		tmp_data.data_addr = data_block->data_addr;
		tmp_data.len = data_block->len;

		priv->ops->wait_for_data(priv, &data_remaining);

		while ((data_block->len > 0) && (data_remaining > 0)) {
			ret = priv->ops->readnbytes(priv, data_block,
					&data_block->len, &bytes_read);
			if (ret)
				pr_warn("%s: ip_readnbytes failed\n", __func__);

			data_remaining -= bytes_read;
			data_block->data_addr += bytes_read;
			data_block->len -= bytes_read;
			total_read += bytes_read;
		}

		(*filled_blocks)++;
		data_block->data_addr = tmp_data.data_addr;
		data_block->len = tmp_data.len;
		block_count--;
		if (block_count)
			data_block++;
	}

	return total_read;
}

int ip_fe_pull_test_for_data(stm_object_h obj, uint32_t *size)
{
	struct stm_fe_ip_s *priv = (struct stm_fe_ip_s *) obj;
	int ret = 0;

	if (!priv) {
		pr_err("%s: ptr priv is NULL\n", __func__);
		return -EINVAL;
	}

	ret = priv->ops->wait_for_data(priv, size);

	return ret;
}

stm_data_interface_pull_src_t stm_fe_pull_data_interface = {
	ip_fe_pull_data,
	ip_fe_pull_test_for_data
};

/*
 * Name: stm_fe_ip_attach()
 *
 * Description: This function is used to attach (or connect) the output of the
 * IP front-end to the input of the demux or a memory sink.
 */
int32_t stm_fe_ip_attach(stm_fe_ip_h obj, stm_object_h sink_object)
{
	int ret;
	stm_object_h sink_obj_type;
	size_t actual_size;
	struct stm_fe_ip_s *priv;
	char connect_tag[STM_REGISTRY_MAX_TAG_SIZE];
	char sink_name[STM_REGISTRY_MAX_TAG_SIZE];
	uint8_t idx = 0;

	if (!obj) {
		pr_err("%s: ip fe is not initialized\n", __func__);
		return -EINVAL;
	}

	priv = (struct stm_fe_ip_s *)obj;

	if (!priv->ops) {
		pr_err("%s: ip fe source not allocated\n", __func__);
		return -ENODEV;
	}

	if (!priv->ip_src) {
		pr_err("%s: priv->ip_src is NULL", __func__);
		return -EFAULT;
	}


	ret = stm_registry_get_object_type(sink_object, &sink_obj_type);
	if (ret) {
		pr_err("%s: reg get_object_type failed: %d\n", __func__, ret);
		return ret;
	}

	if (0 == stm_registry_get_attribute(sink_obj_type,
				STM_DATA_INTERFACE_PUSH, STM_REGISTRY_ADDRESS,
				sizeof(stm_data_interface_push_sink_t),
				&(priv->ip_src->push_intf), &actual_size)) {
		ret = priv->ip_src->push_intf.connect(obj, sink_object);
		pr_debug("%s: push intf connect called\n", __func__);
	} else if (0 == stm_registry_get_attribute(sink_obj_type,
				STM_DATA_INTERFACE_PULL, STM_REGISTRY_ADDRESS,
				sizeof(stm_data_interface_pull_sink_t),
				&(priv->ip_src->pull_intf) , &actual_size)) {
		ret = priv->ip_src->pull_intf.connect(obj, sink_object,
				&stm_fe_pull_data_interface);
		pr_debug("%s: pull intf connect called\n", __func__);

	} else {
		pr_err("%s: reg get_attribute failed\n", __func__);
		ret = -ENOTSUPP;
		return ret;
	}

	if (ret) {
		pr_err("%s: callback failed (%d)\n", __func__, ret);
		return -ECONNREFUSED;
	}
	ret = stm_registry_get_object_tag(sink_object, sink_name);

	if (ret) {
		pr_err("%s: failed to get sink obj tag: %d\n", __func__, ret);
		return ret;
	}

	snprintf(connect_tag, STM_REGISTRY_MAX_TAG_SIZE, "%s->%s",
			priv->ip_name, sink_name);

	ret = stm_registry_add_connection(obj, connect_tag, sink_object);

	if (ret) {
		pr_err("%s: reg add_connection failed: %d", __func__, ret);
		return ret;
	}

	for (idx = 0; idx < IP_FE_SINK_OBJ_MAX; idx++) {
		if (!priv->ip_src->sink_obj[idx])
			break;
	}

	if (idx == IP_FE_SINK_OBJ_MAX) {
		pr_err("%s: sink obj mem limit reached\n", __func__);
		ret = stm_registry_remove_connection(obj, connect_tag);
		return -ENOMEM;
	}

	priv->ip_src->sink_obj[idx] = sink_object;

	if (!priv->ops->attach_obj) {
		pr_err("%s: ip_attach_obj is NULL\n", __func__);
		ret = stm_registry_remove_connection(obj, connect_tag);
		return -EFAULT;
	}

	ret = priv->ops->attach_obj(priv);
	if (ret) {
		pr_err("%s: ops->attach_obj failed\n", __func__);
		ret = stm_registry_remove_connection(obj, connect_tag);
	}

	return ret;
}
EXPORT_SYMBOL(stm_fe_ip_attach);


/*
 * Name: stm_fe_ip_detach()
 *
 * Description: This function is used to detach (or disconnect) the output of
		the IP front-end from the input of the demux or a memory sink.
 */
int32_t stm_fe_ip_detach(stm_fe_ip_h obj, stm_object_h peer_object)
{
	int ret;
	stm_object_h sink_obj_type;
	stm_object_h sink_object;
	size_t actual_size;
	char connect_tag[STM_REGISTRY_MAX_TAG_SIZE];
	char sink_name[STM_REGISTRY_MAX_TAG_SIZE];
	struct stm_fe_ip_s *priv;
	uint8_t idx = 0;

	if (!obj || !peer_object) {
		pr_err("%s: ip OR peer_object is NULL\n", __func__);
		return -EINVAL;
	}

	priv = (struct stm_fe_ip_s *)obj;

	if (!priv->ops) {
		pr_err("%s: ip fe src is not allocated\n", __func__);
		return -ENODEV;
	}

	if (!priv->ip_src) {
		pr_err("%s: priv->ip_src is NULL", __func__);
		return -EFAULT;
	}

	ret = stm_registry_get_object_tag(peer_object, sink_name);

	if (ret) {
		pr_err("%s: get sink object tag failed: %d\n", __func__, ret);
		return ret;
	}

	snprintf(connect_tag, STM_REGISTRY_MAX_TAG_SIZE, "%s->%s",
			priv->ip_name, sink_name);

	ret = stm_registry_get_connection(obj, connect_tag, &sink_object);
	if (ret) {
		pr_err("%s: reg get_connection failed: %d\n", __func__, ret);
		return ret;
	}

	ret = stm_registry_get_object_type(sink_object, &sink_obj_type);
	if (ret)
		pr_err("%s: reg get_object_type failed: %d\n", __func__, ret);

	for (idx = 0; idx < IP_FE_SINK_OBJ_MAX; idx++) {
		if (priv->ip_src->sink_obj[idx] == peer_object)
			break;
	}

	if (idx == IP_FE_SINK_OBJ_MAX) {
		pr_err("%s: peer object is not found\n", __func__);
		ret = -EINVAL;
		return ret;
	}

	if (0 == stm_registry_get_attribute(sink_obj_type,
				STM_DATA_INTERFACE_PUSH, STM_REGISTRY_ADDRESS,
				sizeof(stm_data_interface_push_sink_t),
				&(priv->ip_src->push_intf), &actual_size)) {
		ret = priv->ip_src->push_intf.disconnect(obj, sink_object);
	} else if (0 == stm_registry_get_attribute(sink_obj_type,
				STM_DATA_INTERFACE_PULL, STM_REGISTRY_ADDRESS,
				sizeof(stm_data_interface_pull_sink_t),
				&(priv->ip_src->pull_intf), &actual_size)) {
		ret = priv->ip_src->pull_intf.disconnect(obj, sink_object);
	} else {
		pr_err("%s: reg get_attribute failed: %d\n", __func__, ret);
		return ret;
	}

	if (ret) {
		pr_err("%s: callback failed (%d)\n", __func__, ret);
		return -ECONNREFUSED;
	}

	ret = stm_registry_remove_connection(obj, connect_tag);
	if (ret)
		pr_err("%s: reg remove_connection failed: %d\n", __func__, ret);

	if (!priv->ops->detach_obj) {
		pr_err("%s: ip_detach_obj is NULL\n", __func__);
		return -EFAULT;
	}

	ret = priv->ops->detach_obj(priv);
	if (ret)
		pr_err("%s: ops->detach_obj failed\n", __func__);


	priv->ip_src->sink_obj[idx] = NULL;

	return ret;
}
EXPORT_SYMBOL(stm_fe_ip_detach);

/*
 * Name: stm_fe_ip_get_stats()
 *
 * Description: This function gives statistical information
 * (regarding packet reception).
 *
 */

int32_t stm_fe_ip_get_stats(stm_fe_ip_h obj, stm_fe_ip_stat_t *info_p)
{
	int ret = -1;
	struct stm_fe_ip_s *priv = NULL;

	if (!obj || !info_p) {
		pr_err("%s: obj OR info_p is NULL", __func__);
		return -EINVAL;
	}

	priv = (struct stm_fe_ip_s *)obj;

	if (!priv->ops) {
		pr_err("%s: source is not allocated\n", __func__);
			return -ENODEV;
	}

	if (!priv->ops->get_stats) {
		pr_err("%s: FP ip_get_stats is NULL\n", __func__);
		return -EFAULT;
	}
	ret = priv->ops->get_stats(priv, info_p);
	if (ret)
		pr_err("%s: get_stats failed\n", __func__);

	return ret;
}
EXPORT_SYMBOL(stm_fe_ip_get_stats);

/*
 * Name: stm_fe_ip_get_event_data()
 *
 * Description: This function is used to retrieve event
 * data when events are received by user if needed.
 *
 */

int32_t stm_fe_ip_get_event_data(stm_fe_ip_h fe_obj,
					stm_fe_ip_event_data_t *event_data_p)
{
	/*feature not supported*/
	pr_info("%s: feature not supported", __func__);
	return -EOPNOTSUPP;
}
EXPORT_SYMBOL(stm_fe_ip_get_event_data);

struct stm_fe_ip_s *ip_from_name(const char *name)
{
	struct list_head *pos;

	list_for_each(pos, &stm_fe_engine.ip_list) {
		struct stm_fe_ip_s *priv;
		priv = list_entry(pos, struct stm_fe_ip_s, list);
		if (!strcmp(priv->ip_name, name))
			return priv;
	}

	return NULL;
}

struct stm_fe_ip_s *ip_from_id(uint32_t id)
{
	struct list_head *pos;

	list_for_each(pos, &stm_fe_engine.ip_list) {
		struct stm_fe_ip_s *priv;
		priv = list_entry(pos, struct stm_fe_ip_s, list);
		if (priv->ip_id == id)
			return priv;
	}

	return NULL;
}
