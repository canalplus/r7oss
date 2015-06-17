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

Source file name : stm_fe_demod.c
Author :           Rahul.V

API dedinitions for demodulation device

Date        Modification                                    Name
----        ------------                                    --------
16-Jun-11   Created                                         Rahul.V
11-April-12 Added PM support				    Ankur Tyagi
22-May-13   Added CPS support				    Ankur Tyagi
************************************************************************/
#include <linux/kernel.h>
#include <linux/version.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 2, 2)
#include <linux/export.h>
#else
#include <linux/module.h>
#endif
#include <linux/gpio.h>
#include <linux/list.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/of_address.h>
#endif
#include <linux/err.h>
#include <linux/stm/plat_dev.h>
#ifdef CONFIG_ARCH_STI
#include <linux/pinctrl/consumer.h>
#include <linux/mfd/syscon.h>
#include <linux/regmap.h>
#else
#include <linux/stm/device.h>
#include <linux/stm/pad.h>
#endif
#include <linux/stm/demod.h>
#include <linux/stm/lnb.h>
#include <linux/stm/diseqc.h>
#include <stm_fe.h>
#include <stfe_utilities.h>
#include <stm_fe_os.h>
#include <i2c_wrapper.h>
#include <stats.h>
#include <tuner_wrapper.h>
#include "stm_fe_demod.h"
#include "stm_fe_ip.h"
#include "stm_fe_engine.h"
#include "stm_fe_lnb.h"
#include "stm_fe_diseqc.h"
#include "stm_fe_install.h"
#include <pm.h>
#include <debugfs.h>
#include <linux/module.h>

static bool disable_ts_via_cablecard;
module_param_named(disable_ts_via_cablecard, disable_ts_via_cablecard, bool,
									0644);
MODULE_PARM_DESC(disable_ts_via_cablecard,
				"Disable cable card routing Default:OFF");
static int demod_dev_id;
#ifdef CONFIG_OF
static int demod_reset(uint32_t reset_pio)
{
	/* Reset the PIO lines connected the demod */
	if (reset_pio != DEMOD_RESET_PIO_NONE) {
		gpio_request(reset_pio, "Nim Reset");
		gpio_direction_output(reset_pio, 0);
		msleep(10);
		gpio_direction_output(reset_pio, 1);
	}

	return 0;
}

#ifdef CONFIG_ARCH_STI
static void demod_setup_tsin(struct demod_config_s *conf, int freeze)
{
	if (!conf->pinctrl) {
		pr_err("%s: pinctrl is not declared\n", __func__);
		return;
	}

	if (freeze) {
		devm_pinctrl_put(conf->pinctrl);
		return;
	}
	conf->pins_default = pinctrl_lookup_state(conf->pinctrl,
						  PINCTRL_STATE_DEFAULT);
	if (IS_ERR(conf->pins_default))
		pr_err("%s: could not get default pinstate\n", __func__);
	else
		pinctrl_select_state(conf->pinctrl, conf->pins_default);

	return;

}
#else
static void demod_setup_tsin(struct demod_config_s *conf, int freeze)
{
	if (!conf->pad_cfg) {
		pr_err("%s: pad configs are not declared\n", __func__);
		return;
	}

	if (freeze) {
		if (conf->pad_state)
			stm_pad_release(conf->pad_state);
		conf->pad_state = NULL;
	} else {
		conf->pad_state = stm_pad_claim(conf->pad_cfg, "tsin");
	}
}
#endif

int demod_config_pdata(struct device_node *iter, struct demod_config_s *conf)
{
	int ret = 0;
	struct device_node *i2c; /*parent i2c node*/
	const char *name;
	const char *lla;
	const unsigned int *addr;
	struct resource res;
	int i2c_id;
	const char *eth;
	unsigned int pio;

	ret = of_property_read_string(iter, "model", &name);
	if (ret)
		goto demod_pdata_err;

	strlcpy(conf->demod_name, name, DEMOD_NAME_MAX_LEN);
	/*For remote and PI - I2C adapter is not required*/
	if ((!strncmp(conf->demod_name, "DUMMY", 5)) ||
			(!strncmp(conf->demod_name, "REMOTE", 6)))
		goto i2c_adapter_bypass;

	i2c  = of_get_parent(iter);
	if (!i2c) {
		conf->demod_io.io = DEMOD_IO_MEMORY_MAPPED;
		goto i2c_adapter_bypass;
	} else
		conf->demod_io.io = DEMOD_IO_I2C;

	i2c_id = of_alias_get_id(i2c, "i2c");
	if (i2c_id < 0) {
		ret = -ENODEV;
		goto demod_pdata_err;
	}

	ret = of_address_to_resource(i2c, 0, &res);
	if (ret < 0)
		pr_err("%s: get i2c base address failed\n", __func__);

	ret = stm_fe_check_i2c_device_id((uint32_t *)res.start, i2c_id);
	if (!ret) {
		conf->demod_io.bus = i2c_id;
		pr_debug("%s: demod i2c bus id = %d\n", __func__, i2c_id);
	} else {
		pr_err("%s: wrong demod i2c bus id\n", __func__);
		goto demod_pdata_err;
	}

	addr = of_get_property(iter, "reg", NULL);
	if (addr) {/*only 8 bits of address are useful*/
		conf->demod_io.address = (*addr) >> 24;
	} else {
		pr_err("%s: get demod address failed\n", __func__);
		goto demod_pdata_err;
	}

	of_property_read_u32(iter, "demod_i2c_route", &conf->demod_io.route);
	of_property_read_u32(iter, "demod_i2c_speed", &conf->demod_io.baud);
	if (of_property_read_u32(iter, "demod_pio_reset", &pio) != 0) {
		pr_info("%s: no reset pio defined\n", __func__);
		goto i2c_adapter_bypass;
	}
	conf->reset_pio = of_get_named_gpio(iter, "demod_pio_reset", 0);
	/*PIO reset*/
	demod_reset(conf->reset_pio);

i2c_adapter_bypass:
	ret = of_property_read_u32(iter, "lla_selection", &conf->demod_lla_sel);
	if (ret) {
		pr_err("%s: invalid lla_selection\n", __func__);
		goto demod_pdata_err;
	}

	if (conf->demod_lla_sel != DEMOD_LLA_AUTO
			&& conf->demod_lla_sel != DEMOD_LLA_FORCED) {
		pr_err("%s: wrong value of lla_selection\n", __func__);
		goto demod_pdata_err;
	}
	if (conf->demod_lla_sel == DEMOD_LLA_FORCED) {
		ret = of_property_read_string(iter, "lla_driver", &lla);
		if (ret)
			goto demod_pdata_err;
		if ((strncmp(lla, "st,lla", sizeof("st,lla")))
			&& (strncmp(lla, "custom,lla", sizeof("custom,lla")))) {
			pr_err("%s: lla_driver not correctly set\n", __func__);
			goto demod_pdata_err;
		}
		strlcpy(conf->demod_lla, lla, DEMOD_NAME_MAX_LEN);
	}

	ret = of_property_read_u32(iter,
		    "demod_clock_type", &conf->clk_config.clk_type);
	if (ret)
		goto demod_pdata_err;

	of_property_read_u32(iter,
		    "demod_clock_freq", &conf->clk_config.demod_clk);
	ret = of_property_read_u32(iter, "demod_ts_config", &conf->ts_out);
	if (ret)
		goto demod_pdata_err;

	of_property_read_u32(iter, "demod_ts_clock", &conf->ts_clock);
	of_property_read_u32(iter, "demod_ts_tag", &conf->ts_tag);
	ret = of_property_read_u32(iter, "demod_tsin_id", &conf->demux_tsin_id);
	if (ret) {
		pr_err("%s: invalid demod_tsin_id\n", __func__);
		goto demod_pdata_err;
	}

	of_property_read_u32(iter, "demod_custom_option", &conf->custom_flags);
	of_property_read_u32(iter, "demod_tuner_if", &conf->tuner_if);
	of_property_read_u32(iter, "demod_roll_off", &conf->roll_off);
	of_property_read_u32(iter, "remote_ip", &conf->remote_ip);
	of_property_read_u32(iter, "remote_port", &conf->remote_port);
	ret = of_property_read_string(iter, "eth_name", &eth);
	if (ret) {
		if (!strncmp(conf->demod_name, "REMOTE", 6))
			pr_err("%s: invalid eth interface name\n", __func__);
		else
			pr_debug("%s: invalid eth interface name\n", __func__);
		return 0;
	}
	strlcpy(conf->eth_name, eth, ETH_NAME_MAX_LEN);
demod_pdata_err:
	if (ret)
		pr_err("%s: demod config not set properly\n", __func__);

	return ret;
}

int tuner_config_pdata(struct device_node *iter, struct demod_config_s *conf)
{
	int ret = 0;
	struct device_node *i2c; /*parent i2c node*/
	const char *name;
	const char *lla;
	const unsigned int *addr;
	struct resource res;
	int i2c_id;

	ret = of_property_read_string(iter, "model", &name);
	if (ret)
		goto tuner_pdata_err;
	strlcpy(conf->tuner_name, name, DEMOD_NAME_MAX_LEN);

	i2c  = of_get_parent(iter);
	if (!i2c) {
		conf->tuner_io.io = DEMOD_IO_MEMORY_MAPPED;
		goto tuner_i2c_bypass;
	} else
		conf->tuner_io.io = DEMOD_IO_I2C;

	i2c_id = of_alias_get_id(i2c, "i2c");
	if (i2c_id < 0) {
		ret = -EINVAL;
		goto tuner_pdata_err;
	}

	ret = of_address_to_resource(i2c, 0, &res);
	if (ret < 0)
		pr_err("%s: get i2c bus address failed\n", __func__);

	ret = stm_fe_check_i2c_device_id((uint32_t *)res.start, i2c_id);
	if (!ret) {
		conf->tuner_io.bus = i2c_id;
		pr_debug("%s: tuner i2c bus id = %d\n", __func__, i2c_id);
	} else {
		pr_err("%s: wrong tuner i2c bus id\n", __func__);
		goto tuner_pdata_err;
	}

	addr = of_get_property(iter, "reg", NULL);
	if (addr) {
		conf->tuner_io.address = (*addr) >> 24;
	} else {
		pr_err("%s: get tuner address failed\n", __func__);
		goto tuner_pdata_err;
	}
	of_property_read_u32(iter, "tuner_i2c_route" , &conf->tuner_io.route);
	of_property_read_u32(iter, "tuner_i2c_speed" , &conf->tuner_io.baud);

tuner_i2c_bypass:
	ret = of_property_read_u32(iter, "lla_selection", &conf->tuner_lla_sel);
	if (ret) {
		pr_err("%s: invalid lla_selection\n", __func__);
		goto tuner_pdata_err;
	}

	if (conf->tuner_lla_sel != DEMOD_LLA_AUTO
			&& conf->tuner_lla_sel != DEMOD_LLA_FORCED) {
		pr_err("%s: wrong value of lla_selection\n", __func__);
		goto tuner_pdata_err;
	}

	if (conf->tuner_lla_sel == DEMOD_LLA_FORCED) {
		ret = of_property_read_string(iter, "lla_driver", &lla);
		if (ret)
			goto tuner_pdata_err;
		if ((strncmp(lla, "st,lla", sizeof("st,lla")))
			&& (strncmp(lla, "custom,lla", sizeof("custom,lla")))) {
			pr_err("%s: lla_driver not correctly set\n", __func__);
			goto tuner_pdata_err;
		}
		strlcpy(conf->tuner_lla, lla, DEMOD_NAME_MAX_LEN);
	}

	of_property_read_u32(iter, "tuner_clock_freq" ,
			&conf->clk_config.tuner_clk);
	of_property_read_u32(iter, "tuner_clock_div" ,
			&conf->clk_config.tuner_clkout_divider);

tuner_pdata_err:
	if (ret)
		pr_err("%s: tuner config not set properly\n", __func__);

	return ret;
}

int vglna_config_pdata(struct device_node *iter, struct demod_config_s *conf)
{
	int ret = 0;
	struct device_node *i2c; /*parent i2c node*/
	const char *name;
	const unsigned int *addr;
	struct resource res;
	int i2c_id;

	ret = of_property_read_string(iter, "model", &name);
	if (ret)
		goto vglna_pdata_err;
	strlcpy(conf->vglna_config.name, name, DEMOD_NAME_MAX_LEN);
	i2c  = of_get_parent(iter);
	if (!i2c) {
		conf->vglna_config.vglna_io.io = DEMOD_IO_MEMORY_MAPPED;
		goto vglna_i2c_bypass;
	} else
		conf->vglna_config.vglna_io.io = DEMOD_IO_I2C;
	i2c_id = of_alias_get_id(i2c, "i2c");
	if (i2c_id < 0) {
		ret = -EINVAL;
		goto vglna_pdata_err;
	}
	ret = of_address_to_resource(i2c, 0, &res);
	if (ret < 0)
		pr_err("%s: get i2c base address failed\n", __func__);
	ret = stm_fe_check_i2c_device_id((uint32_t *)res.start, i2c_id);
	if (!ret) {
		conf->vglna_config.vglna_io.bus = i2c_id;
		pr_debug("%s: vglna i2c bus id = %d\n", __func__, i2c_id);
	} else {
		pr_err("%s: wrong vglna i2c bus id\n", __func__);
		goto vglna_pdata_err;
	}

	addr = of_get_property(iter, "reg", NULL);
	if (addr) {/*only 8 bits of address are useful*/
		conf->vglna_config.vglna_io.address = (*addr) >> 24;
	} else {
		pr_err("%s: get vglna address failed\n", __func__);
		goto vglna_pdata_err;
	}
	of_property_read_u32(iter, "vglna_i2c_route",
			&conf->vglna_config.vglna_io.route);
	of_property_read_u32(iter, "vglna_i2c_speed",
			&conf->vglna_config.vglna_io.baud);
vglna_i2c_bypass:
	of_property_read_u32(iter, "vglna_type", &conf->vglna_config.type);
	of_property_read_u32(iter, "vglna_rep_bus",
			&conf->vglna_config.rep_bus);
vglna_pdata_err:
	if (ret)
		pr_err("%s: vglna config not set properly\n", __func__);

	return ret;
}

static void *stm_demod_get_pdata(struct platform_device *pdev)
{
	int ret = 0;
	struct device_node *np = pdev->dev.of_node;
	struct device_node *fe;	/*parent node*/
	struct device_node *iter;
	struct demod_config_s *conf;

	conf = devm_kzalloc(&pdev->dev, sizeof(*conf), GFP_KERNEL);
	if (!conf) {
		pr_err("%s: demod paltform data alloc failed\n", __func__);
		ret = -ENOMEM;
		goto out;
	}
	/*pad config*/

#ifdef CONFIG_ARCH_STI
	conf->pinctrl = devm_pinctrl_get(&pdev->dev);
	if (IS_ERR(conf->pinctrl)) {
		pr_err("%s: pinctrl is not declared\n", __func__);
		ret = -EINVAL;
		goto out;
	}

	conf->regmap_ts_map = syscon_regmap_lookup_by_phandle(np, "st,syscfg");

	if (IS_ERR(conf->regmap_ts_map)) {
		pr_info("%s: No syscfg phandle specified\n", __func__);
		goto get_parent;
	}

	conf->res = platform_get_resource_byname(pdev, IORESOURCE_MEM,
						"ts-mapping-ctrl");
	if (!conf->res) {
		pr_err("%s: No res for sysconf ts pio map\n", __func__);
		goto get_parent;
	} else {
		conf->syscfg_ts_map = conf->res->start;
	}

	ret = of_property_read_u32(np, "st,ctrl-mask", &(conf->ts_map_mask));
	if (ret) {
		pr_err("%s: No ctrl-mask for ts mapping <%d>\n", __func__, ret);
		goto get_parent;
	}

	ret = of_property_read_u32(np, "st,ctrl-value", &(conf->ts_map_val));
	if (ret) {
		pr_err("%s: No ctrl-val for ts mapping <%d>\n", __func__, ret);
		goto get_parent;
	}

	ret = regmap_update_bits(conf->regmap_ts_map, conf->syscfg_ts_map,
					conf->ts_map_mask, conf->ts_map_val);
	if (ret)
		pr_err("%s: Fail:ts mapping control syscfg update\n", __func__);
	else
		pr_info("%s: ts mapping control syscfg update ok\n", __func__);
get_parent:
#else
	conf->pad_cfg = stm_of_get_pad_config(&pdev->dev);
	if (!conf->pad_cfg) {
		pr_err("%s: pad configs are not declared", __func__);
		ret = -EINVAL;
		goto out;
	}

	conf->device_config = stm_of_get_dev_config(&pdev->dev);
	if (conf->device_config)
		devm_stm_device_init(&pdev->dev, conf->device_config);

#endif

	fe = of_get_parent(np);
	if (!fe) {
		ret = -EINVAL;
		goto out;
	}
	ret = of_property_read_u32(fe, "control", &conf->ctrl);
	if (ret)
		goto out;
	iter = of_parse_phandle(np, "demod", 0);
	if (!iter) {
		ret = -EINVAL;
		goto out;
	}
	ret = demod_config_pdata(iter, conf);
	if (ret)
		goto out;

	iter = of_parse_phandle(np, "tuner", 0);
	if (!iter)
		goto vglna;
	ret = tuner_config_pdata(iter, conf);
	if (ret)
		goto out;

vglna:
	iter = of_parse_phandle(np, "vglna", 0);
	if (!iter)
		goto out;
	ret = vglna_config_pdata(iter, conf);

out:
	if (ret)
		return ERR_PTR(ret);

	return conf;
}

#else

static void demod_setup_tsin(struct demod_config_s *conf, int freeze)
{
	/* Does not do anything for non DT platform */
	/* Kernel is reponsible for claiming tsin pads */
	return;
}

static void *stm_demod_get_pdata(struct platform_device *pdev)
{
	return NULL;
}
#endif

/*
 * Name: stm_fe_demod_probe()
 *
 * Description:get the configuration information of demod
 */
int stm_fe_demod_probe(struct platform_device *pdev)
{
	struct stm_platdev_data *plat_dev_data_p;
	struct demod_config_s *conf;
	struct stm_fe_demod_s *priv;

	priv = stm_fe_malloc(sizeof(struct stm_fe_demod_s));
	if (!priv)
		return -ENOMEM;
	/* Get the configuration information about the tuners */
	if (pdev->dev.of_node) {
		conf = stm_demod_get_pdata(pdev);
		if (IS_ERR_OR_NULL(conf)) {
			stm_fe_free((void **)&priv);
			return PTR_ERR(conf);
		}
		demod_setup_tsin(conf, 0);
		priv->demod_id = demod_dev_id;
		pdev->id = demod_dev_id;
		demod_dev_id++;
		priv->dt_enable = true;
		priv->first_tunerinfo = false;
	} else {
		plat_dev_data_p = pdev->dev.platform_data;
		conf = plat_dev_data_p->config_data;
		priv->demod_id = pdev->id;
		priv->dt_enable = false;
	}
	list_add(&priv->list, get_demod_list());

	priv->config = conf;
	priv->demod_data = pdev;
	priv->list_h = get_demod_list();
	snprintf(priv->demod_name, sizeof(priv->demod_name),
					     "stm_fe_demod.%d", priv->demod_id);

	incr_obj_cnt();

	pr_info("%s: Configuring demod %s\n", __func__, priv->demod_name);

	return 0;
}

/*
 * Name: stm_fe_demod_remove()
 *
 * Description:remove the configuration information of demod
 */
int stm_fe_demod_remove(struct platform_device *pdev)
{
	struct stm_fe_demod_s *priv = demod_from_id(pdev->id);

	if (priv) {
		list_del(&priv->list);
		if (priv->demod_data->dev.of_node)
			demod_setup_tsin(priv->config, 1);
		stm_fe_free((void **)&priv);
		decr_obj_cnt();
	} else {
		pr_err("%s: demod obj is not initialised\n", __func__);
		return -ENODEV;
	}

	return 0;
}

/*
 * Name: stm_fe_demod_new()
 *
 * Description:allocates a new demod source
 */
int32_t stm_fe_demod_new(const char *name, stm_fe_demod_h *obj)
{
	int ret = -1;
	int err = -1;
	struct stm_fe_demod_s *priv = demod_from_name(name);
	*obj = NULL;

	if (!priv) {
		pr_err("%s: '%s' not present in demod list\n", __func__, name);
		return -ENODEV;
	}

	if (priv->ops) {
		pr_err("%s: '%s' already exists\n", __func__, name);
		return -EEXIST;
	}

	/*initial value assignment for stat attributes */
	priv->stat_attrs.last_acq_time_ticks = 0;

	/*Memory Allocations for STMFE internal structures */

	priv->demod_h = stm_fe_malloc(sizeof(STCHIP_Info_t));
	if (!priv->demod_h) {
		pr_err("%s: mem alloc for demod_h failed\n", __func__);
		return -ENOMEM;
	}
	priv->tuner_h = stm_fe_malloc(sizeof(STCHIP_Info_t));
	if (!priv->tuner_h) {
		pr_err("%s: mem alloc for tuner_h failed\n", __func__);
		err = -ENOMEM;
		goto tuner_h_malloc_fail;
	}

	priv->tuner_h->tuner_ops = stm_fe_malloc(sizeof(struct tuner_ops_s));
	if (!priv->tuner_h->tuner_ops) {
		pr_err("%s: mem alloc for tuner FP failed\n", __func__);
		err = -ENOMEM;
		goto tuner_ops_malloc_fail;
	}
	priv->ops = stm_fe_malloc(sizeof(struct demod_ops_s));
	if (!priv->ops) {
		pr_err("%s: mem alloc for Demod FP failed\n", __func__);
		err = -ENOMEM;
		goto demod_ops_malloc_fail;
	}
	if (priv->dt_enable)
		ret = demod_install_dt(priv);
	else
		ret = demod_install(priv);
	if (ret) {
		pr_err("%s: No demod is available to install\n", __func__);
		err = -ENODEV;
		goto demod_install_fail;
	}

	/* Control through Linux DVB; Not necessary to proceed further */
	if (priv->bypass_control) {
		pr_info("%s: STM_FE Demod control bypassed ...\n", __func__);
		goto ret_ok;
	}

	if (priv->dt_enable)
		ret = tuner_install_dt(priv);
	else
		ret = tuner_install(priv);
	if (ret) {
		pr_err("%s: No tuner is available to install\n", __func__);
		err = -ENODEV;
		goto tuner_install_fail;
	}

	/*Initialization of demod and tuner */
	if (priv->ops->init) {
		ret = priv->ops->init(priv);
		if (ret) {
			pr_err("%s: demod_init failed\n", __func__);
			err = ret;
			goto demod_init_fail;
		}
	} else {
		pr_err("%s: FP demod_init is NULL\n", __func__);
		err = -EFAULT;
		goto demod_init_fail;
	}

	/*start  the demod thread */
	ret = demod_task_open(priv);
	if (ret) {
		pr_err("%s: demod_task_open failed\n", __func__);
		err = ret;
		goto task_open_fail;
	}
	priv->tune_enable = false;
	priv->scan_enable = false;
ret_ok:
	ret = stm_registry_add_instance(STM_REGISTRY_INSTANCES,
					get_type_for_registry(),
					name, (stm_object_h)priv);
	if (ret) {
		pr_err("%s: reg add_instance failed: %d\n", __func__, ret);
		if (demod_task_close(priv))
			pr_err("%s: demod task close failed\n", __func__);
		err = ret;
		goto task_open_fail;
	}
	priv->rpm_suspended = false;
	*obj = priv;

	if (stmfe_stats_init(priv))
		pr_err("%s: stmfe_stats_init failed: %d\n", __func__, ret);

	if (stmfe_demod_debugfs_create(priv))
		pr_err("%s: debugfs set up failed for RF FE\n", __func__);

#ifdef CONFIG_PM_RUNTIME
	/* Increment device usage but not more than 1 */
	if (!atomic_read(&priv->demod_data->dev.power.usage_count))
		pm_runtime_get_noresume(&priv->demod_data->dev);

	pm_runtime_set_active(&priv->demod_data->dev);
	pm_runtime_enable(&priv->demod_data->dev);
	pm_runtime_put_sync_suspend(&priv->demod_data->dev);
	pm_runtime_set_suspended(&priv->demod_data->dev);
	priv->rpm_suspended = true;
#endif
	return 0;

task_open_fail:
	ret = priv->ops->term(priv);
	if (ret)
		pr_err("%s: demod->term failed\n", __func__);
demod_init_fail:
	ret = tuner_uninstall(priv);
	if (ret)
		pr_err("%s: tuner uninstall failed\n", __func__);
tuner_install_fail:
	ret = demod_uninstall(priv);
	if (ret)
		pr_err("%s: demod uninstall failed\n", __func__);
demod_install_fail:
	stm_fe_free((void **)&priv->ops);
demod_ops_malloc_fail:
	stm_fe_free((void **)&priv->tuner_h->tuner_ops);
tuner_ops_malloc_fail:
	stm_fe_free((void **)&priv->tuner_h);
tuner_h_malloc_fail:
	stm_fe_free((void **)&priv->demod_h);

	return err;
}
EXPORT_SYMBOL(stm_fe_demod_new);

/*
 * Name: stm_fe_demod_delete()
 *
 * Description: delete a demod source object
 */
int32_t stm_fe_demod_delete(stm_fe_demod_h obj)
{
	int ret = -1, err = 0;
	struct stm_fe_demod_s *priv;

	if (!obj) {
		pr_err("%s: demod obj is not initialised\n", __func__);
		return -EINVAL;
	}

	priv = (struct stm_fe_demod_s *)obj;

	if (!priv->ops) {
		pr_err("%s: demod source is not allocated\n", __func__);
		return -ENODEV;
	}
#ifdef CONFIG_PM_RUNTIME
	/* Decrement device usage but not less than 0 */
	if (atomic_read(&priv->demod_data->dev.power.usage_count) > 0) {
		pm_runtime_put_sync_suspend(&priv->demod_data->dev);
		pm_runtime_put_noidle(&priv->demod_data->dev);
		priv->rpm_suspended = true;
	}
		pm_runtime_set_suspended(&priv->demod_data->dev);
		pm_runtime_disable(&priv->demod_data->dev);
#endif
	ret = stmfe_stats_term(priv);
	if (ret)
		pr_err("%s: stmfe_stats_term failed: %d\n", __func__, ret);

	ret = stm_registry_remove_object((stm_object_h) obj);
	if (ret) {
		pr_err("%s: reg remove_object failed: %d\n", __func__, ret);
		err = ret;
	}

	if (!priv->bypass_control) {
		ret = demod_task_close(priv);
		if (ret) {
			pr_err("%s: task_close failed: %d\n", __func__, ret);
			err = ret;
		}
	}
	if (priv->ops->term) {
		ret = priv->ops->term(priv);
		if (ret) {
			pr_err("%s: unable to terminate demod\n", __func__);
			err = ret;
		}
	} else {
		pr_err("%s: FP demod_priv->ops->term is NULL\n", __func__);
		err = -EFAULT;
	}

	if (priv->tuner_h->tuner_ops->term) {
		ret = priv->tuner_h->tuner_ops->term(priv->tuner_h);
		if (ret) {
			pr_err("%s: unable to terminate tuner\n", __func__);
			err = ret;
		}
	}
	ret = tuner_uninstall(priv);
	if (ret) {
		pr_err("%s: tuner uninstall failed\n", __func__);
		err = ret;
	}
	ret = demod_uninstall(priv);
	if (ret) {
		pr_err("%s: demod uninstall failed\n", __func__);
		err = ret;
	}
	stm_fe_free((void **)&priv->ops);
	stm_fe_free((void **)&priv->tuner_h->tuner_ops);
	stm_fe_free((void **)&priv->tuner_h);
	stm_fe_free((void **)&priv->demod_h);

	return err;
}
EXPORT_SYMBOL(stm_fe_demod_delete);

/*
 * Name: stm_fe_demod_attach()
 *
 * Description:connect the TS output or CVBS output of the frontend
 * to the input of the demux
 */
int32_t stm_fe_demod_attach(stm_fe_demod_h obj, stm_object_h sink_object)
{
	int ret;
	stm_object_h sink_obj_type;
	size_t actual_size;
	char connect_tag[STM_REGISTRY_MAX_TAG_SIZE];
	char data_type[STM_REGISTRY_MAX_TAG_SIZE];
	char sink_name[STM_REGISTRY_MAX_TAG_SIZE];
	stm_fe_te_sink_interface_t demux_interface;
	stm_te_tsinhw_config_t ts_params = {0};
	struct stm_fe_demod_s *priv;
	enum demod_tsout_config_e tsconfig;

	if (obj) {
		priv = ((struct stm_fe_demod_s *)obj);
	} else {
		pr_err("%s: demod obj is not initialised\n", __func__);
		return -EINVAL;
	}

	if (!priv->ops) {
		pr_err("%s: demod source is not allocated\n", __func__);
		return -ENODEV;
	}
	tsconfig = priv->config->ts_out;

	ret = stm_registry_get_object_type(sink_object, &sink_obj_type);
	if (ret) {
		pr_err("%s: reg get_object_type failed: %d\n", __func__, ret);
		return ret;
	}

	ret = stm_registry_get_attribute(sink_obj_type,
					 STM_FE_TE_SINK_INTERFACE,
					 data_type,
					 sizeof(stm_fe_te_sink_interface_t),
					 &demux_interface, &actual_size);
	if (ret) {
		pr_err("%s: reg get_attribute failed: %d\n", __func__, ret);
		return ret;
	}

	ret = stm_registry_get_object_tag(sink_object, sink_name);

	if (ret) {
		pr_err("%s: get sink object tag failed: %d\n", __func__, ret);
		return ret;
	}

	if (priv->ops->tsclient_init) {
		ret = priv->ops->tsclient_init(priv, sink_name);
		if (ret) {
			pr_err("%s: tsclient init failed: %d\n", __func__, ret);
			return ret;
		}
	} else {
		pr_debug("%s: FP ops->tsclient_init is NULL\n", __func__);
	}
	ts_params.stm_fe_bc_pid_set = priv->ops->tsclient_pid_configure;
	ts_params.stm_fe_bc_pid_clear = priv->ops->tsclient_pid_deconfigure;
	ts_params.async_not_sync = (bool) (tsconfig & DEMOD_TS_SYNCBYTE_OFF);
	ts_params.invert_ts_clk =
			(bool) (tsconfig & DEMOD_TS_FALLINGEDGE_CLOCK);
	ts_params.serial_not_parallel =
			(bool) ((tsconfig & DEMOD_TS_SERIAL_PUNCT_CLOCK)
			|| (tsconfig & DEMOD_TS_SERIAL_CONT_CLOCK));
	ts_params.tsin_number = priv->config->demux_tsin_id;
	ts_params.ts_tag = priv->config->ts_tag;
	if ((tsconfig & DEMOD_TS_THROUGH_CABLE_CARD) &&
					!(disable_ts_via_cablecard))
		ts_params.ts_thru_cablecard = true;
	else
		ts_params.ts_thru_cablecard = false;

	if (tsconfig & DEMOD_TS_MERGED)
		ts_params.ts_merged = true;
	else
		ts_params.ts_merged = false;
	ts_params.use_timer_tag = (tsconfig & DEMOD_TS_TIMER_TAG);

	if (!demux_interface.stm_te_tsinhw_connect_handler)
		return -EINVAL;

	ret = demux_interface.stm_te_tsinhw_connect_handler(obj, sink_object,
								&ts_params);
	if (ret) {
		pr_err("%s: hw_ts_connect_handler failed: %d\n", __func__, ret);
		return ret;
	}

	snprintf(connect_tag, STM_REGISTRY_MAX_TAG_SIZE, "%s->%s",
			priv->demod_name, sink_name);

	ret = stm_registry_add_connection(obj, connect_tag, sink_object);
	if (ret) {
		pr_err("%s: reg add_connection failed: %d\n", __func__, ret);
		return ret;
	}

	if (stmfe_ts_conf_debugfs_create(priv))
		pr_err("%s: debugfs entry for ts_conf failed\n", __func__);

	return 0;
}
EXPORT_SYMBOL(stm_fe_demod_attach);

/*
 * Name: stm_fe_demod_detach()
 *
 * Description: disconnect the TS output or CVBS output of the frontend
 * from the input of the demux
 */
int32_t stm_fe_demod_detach(stm_fe_demod_h obj, stm_object_h peer_object)
{
	int ret;
	stm_object_h sink_obj_type;
	stm_object_h sink_object;
	size_t actual_size;
	char connect_tag[STM_REGISTRY_MAX_TAG_SIZE];
	char data_type[STM_REGISTRY_MAX_TAG_SIZE];
	char sink_name[STM_REGISTRY_MAX_TAG_SIZE];
	stm_fe_te_sink_interface_t demux_interface;
	struct stm_fe_demod_s *priv;

	if (obj && peer_object) {
		priv = ((struct stm_fe_demod_s *)obj);
	} else {
		pr_err("%s: demod obj is not initialised\n", __func__);
		return -EINVAL;
	}

	if (!priv->ops) {
		pr_err("%s: demod source is not allocated\n", __func__);
		return -ENODEV;
	}

	ret = stm_registry_get_object_tag(peer_object, sink_name);

	if (ret) {
		pr_err("%s: get sink object tag failed: %d\n", __func__, ret);
		return ret;
	}

	snprintf(connect_tag, STM_REGISTRY_MAX_TAG_SIZE, "%s->%s",
			priv->demod_name, sink_name);

	ret = stm_registry_get_connection(obj, connect_tag, &sink_object);
	if (ret) {
		pr_err("%s: reg get_connection failed: %d\n", __func__, ret);
		return ret;
	}

	ret = stm_registry_get_object_type(sink_object, &sink_obj_type);
	if (ret) {
		pr_err("%s: reg get_object_type failed: %d\n", __func__, ret);
		return ret;
	}

	ret = stm_registry_get_attribute(sink_obj_type,
					 STM_FE_TE_SINK_INTERFACE, data_type,
					 sizeof(stm_fe_te_sink_interface_t),
					 &demux_interface, &actual_size);
	if (ret) {
		pr_err("%s: reg get_attribute failed: %d\n", __func__, ret);
		return ret;
	}

	ret = demux_interface.stm_te_tsinhw_disconnect_handler(obj,
							       sink_object);
	if (ret) {
		pr_err("%s: ts_disconnect_handler failed: %d\n", __func__, ret);
		return ret;
	}

	if (priv->ops->tsclient_term) {
		ret = priv->ops->tsclient_term(priv, sink_name);
		if (ret) {
			pr_err("%s: tsclient term failed: %d\n", __func__, ret);
			return ret;
		}
	}
	ret = stm_registry_remove_connection(obj, connect_tag);
	if (ret)
		pr_err("%s: reg remove_connection failed: %d\n", __func__, ret);

	return ret;
}
EXPORT_SYMBOL(stm_fe_demod_detach);

/*
 * Name: stm_fe_demod_tune()
 *
 * Description: initiates tuning operation of the tuner and demod
 */
int32_t stm_fe_demod_tune(stm_fe_demod_h obj, stm_fe_demod_tune_params_t *tp)
{
	int ret = -1;
	struct stm_fe_demod_s *priv;
	struct platform_device *demod_data;

	priv = (struct stm_fe_demod_s *)obj;
	if (!priv) {
		pr_err("%s: demod internal db is not initialized\n", __func__);
		return -EINVAL;
	}

	if (!priv->ops) {
		pr_err("%s: demod source is not allocated\n", __func__);
		return -ENODEV;
	}

	demod_data = priv->demod_data;
#ifdef CONFIG_PM_RUNTIME
	if (demod_data->dev.power.runtime_status == RPM_SUSPENDED) {
		if (priv->rpm_suspended == true)
			priv->rpm_suspended = false;
		else
			return -EPERM;
	}
#endif

	if (priv->scan_enable == true) {
		pr_err("%s: scan_enable is true\n", __func__);
		return -EBUSY;
	}

#ifdef CONFIG_PM_RUNTIME
	/* Increment device usage but not more than 1 */
	if (!atomic_read(&demod_data->dev.power.usage_count))
		pm_runtime_get_sync(&demod_data->dev);
#endif

	/*Abort demod and tuner operations to get the semaphore quickly*/
	if (priv->ops->abort) {
		ret = priv->ops->abort(priv, TRUE);
		if (ret) {
			pr_err("%s: demod_abort failed\n", __func__);
			return ret;
		}
	} else {
		pr_err("%s: FP demod_abort is NULL\n", __func__);
		return -EFAULT;
	}
	/* Wait for guardSemaphore to release */
	ret = stm_fe_sema_wait(priv->fe_task.guard_sem);
	if (ret)
		pr_err("%s: Failed stm_fe_sema_wait= %d\n", __func__, ret);
	/*Undo the Abort before proceeding to acquisition*/
	if (priv->ops->abort) {
		ret = priv->ops->abort(priv, FALSE);
		if (ret) {
			pr_err("%s: demod_abort failed\n", __func__);
			stm_fe_sema_signal(priv->fe_task.guard_sem);
			return ret;
		}
	} else {
		pr_err("%s: FP demod_abort is NULL\n", __func__);
		stm_fe_sema_signal(priv->fe_task.guard_sem);
		return -EFAULT;
	}
	priv->fe_task.delay_in_ms = DEMOD_WAKE_UP;
	fe_wake_up_interruptible(&priv->fe_task.waitq);
	memcpy(&priv->tp, tp, sizeof(stm_fe_demod_tune_params_t));

	priv->fe_task.nextstate = FE_TASKSTATE_TUNE;
	priv->event_disable = false;

	/*signal to guardSemaphore to continue task machine */
	stm_fe_sema_signal(priv->fe_task.guard_sem);

	return ret;
}
EXPORT_SYMBOL(stm_fe_demod_tune);

/*
 * Name: stm_fe_demod_scan()
 *
 * Description: initiates scanning operation of the tuner and demod
 */
int32_t stm_fe_demod_scan(stm_fe_demod_h obj,
			  stm_fe_demod_scan_context_t context,
			  stm_fe_demod_scan_configure_t *config)
{
	int ret = -1;
	struct stm_fe_demod_s *priv;
	struct platform_device *demod_data;

	priv = (struct stm_fe_demod_s *)obj;
	if (!priv) {
		pr_err("%s: demod internal db is not initialized\n", __func__);
		return -EINVAL;
	}

	if (!priv->ops) {
		pr_err("%s: demod source is not allocated\n", __func__);
		return -ENODEV;
	}

	demod_data = priv->demod_data;
#ifdef CONFIG_PM_RUNTIME
	if (demod_data->dev.power.runtime_status == RPM_SUSPENDED) {
		if (priv->rpm_suspended == true)
			priv->rpm_suspended = false;
		else
			return -EPERM;
	}
#endif

	if (priv->tune_enable == true) {
		pr_err("%s: tune_enable is true\n", __func__);
		return -EBUSY;
	}

#ifdef CONFIG_PM_RUNTIME
	/* Increment device usage but not more than 1 */
	if (!atomic_read(&demod_data->dev.power.usage_count))
		pm_runtime_get_sync(&demod_data->dev);
#endif

	priv->scan_enable = true;
	priv->context = STM_FE_DEMOD_SCAN;
	priv->scan_context = context;
	if (context == STM_FE_DEMOD_SCAN_NEW) {
		memcpy(&priv->scanparams, config,
			       sizeof(stm_fe_demod_scan_configure_t));
	}

	/*Wait for guardSemaphore to release*/
	ret = stm_fe_sema_wait(priv->fe_task.guard_sem);
	if (ret)
		pr_err("%s: stm_fe_sema_wait failed: %d\n", __func__, ret);

	priv->fe_task.delay_in_ms = DEMOD_WAKE_UP;
	fe_wake_up_interruptible(&priv->fe_task.waitq);
	priv->fe_task.nextstate = FE_TASKSTATE_SCAN;
	priv->event_disable = false;

	/*signal to guardSemaphore to continue task machine */
	stm_fe_sema_signal(priv->fe_task.guard_sem);

	return ret;
}
EXPORT_SYMBOL(stm_fe_demod_scan);
/*
 * Name: stm_fe_demod_get_info()
 *
 * Description:get detail information on currently acquired channel
 */
int32_t stm_fe_demod_get_info(stm_fe_demod_h obj, stm_fe_demod_info_t *info)
{
	struct stm_fe_demod_s *priv;
	struct platform_device *demod_data;

	priv = (struct stm_fe_demod_s *)obj;

	if (!priv) {
		pr_err("%s: demod internal db is not initialized\n", __func__);
		return -EINVAL;
	}

	if (!priv->ops) {
		pr_err("%s: demod source is not allocated\n", __func__);
		return -ENODEV;
	}

	demod_data = priv->demod_data;
#ifdef CONFIG_PM_RUNTIME
	if (demod_data->dev.power.runtime_status == RPM_SUSPENDED) {
		if (priv->rpm_suspended == true)
			priv->rpm_suspended = false;
		else
			return -EPERM;
	}
	/* Increment device usage but not more than 1 */
	if (!atomic_read(&demod_data->dev.power.usage_count))
		pm_runtime_get_sync(&demod_data->dev);
#endif

	if (!info)
		return -EINVAL;

	*info = priv->info;

	return 0;
}
EXPORT_SYMBOL(stm_fe_demod_get_info);

/*
 * Name: stm_fe_demod_stop()
 *
 * Description:Unlock the currently locked channel if already acquired
 */
int32_t stm_fe_demod_stop(stm_fe_demod_h obj)
{
	int ret = -1;
	struct stm_fe_demod_s *priv;
	struct platform_device *demod_data;
#ifdef CONFIG_PM_RUNTIME
	struct platform_device *diseqc_data;
	struct platform_device *lnb_data;
#endif
	struct stm_fe_diseqc_s *diseqc_priv;
	struct stm_fe_lnb_s *lnb_priv;

	priv = (struct stm_fe_demod_s *)obj;
	if (!priv) {
		pr_err("%s: demod internal db is not initialized\n", __func__);
		return -EINVAL;
	}

	if (!priv->ops) {
		pr_err("%s: demod source is not allocated\n", __func__);
		return -ENODEV;
	}

	diseqc_priv = priv->diseqc_h;
	lnb_priv = priv->lnb_h;
	demod_data = priv->demod_data;

#ifdef CONFIG_PM_RUNTIME
	if (demod_data->dev.power.runtime_status == RPM_SUSPENDED) {
		if (priv->rpm_suspended == true)
			return 0;
		else
			return -EPERM;
	}
#endif

	priv->demod_h->Abort = true;
	priv->tuner_h->Abort = true;
	priv->event_disable = true;

	/* Wait for guardSemaphore to release */
	ret = stm_fe_sema_wait(priv->fe_task.guard_sem);
	if (ret)
		pr_err("%s: stm_fe_sema_wait failed: %d\n", __func__, ret);

	priv->demod_h->Abort = false;
	priv->tuner_h->Abort = false;
	if (!priv->ops->unlock) {
		pr_err("%s: FP (demod_unlock) is NULL\n", __func__);
		stm_fe_sema_signal(priv->fe_task.guard_sem);
		return -EFAULT;
	} else {
		ret = priv->ops->unlock(priv);
		if (ret) {
			pr_err("%s: demod unlock failed: %d\n", __func__, ret);
			stm_fe_sema_signal(priv->fe_task.guard_sem);
			return ret;
		}

	}

	/* Update prevstate as state machine might not run before demod suspend
	 * is called. Otherwise while resuming, we may go in other state (tune)
	 * instead of IDLE.
	 */
	priv->fe_task.prevstate = FE_TASKSTATE_IDLE;
	priv->fe_task.nextstate = FE_TASKSTATE_IDLE;
	priv->tune_enable = false;
	priv->scan_enable = false;
	memset(&priv->info.u_info.tune.demod_info, 0,
			sizeof(stm_fe_demod_channel_info_t));
	/*signal to Guard semaphore to continue task machine */
	priv->fe_task.delay_in_ms = DEMOD_IDLE_PERIOD_MS;
	stm_fe_sema_signal(priv->fe_task.guard_sem);

#ifdef CONFIG_PM_RUNTIME
	if (diseqc_priv) {
		diseqc_data = diseqc_priv->diseqc_data;
	/* Decrement device usage but not less than 0 */
		if (atomic_read(&diseqc_data->dev.power.usage_count) > 0) {
			pm_runtime_put_sync_suspend(&diseqc_data->dev);
			diseqc_priv->rpm_suspended = true;
		}
	}
	if (lnb_priv) {
		lnb_data = lnb_priv->lnb_data;
	/* Decrement device usage but not less than 0 */
		if (atomic_read(&lnb_data->dev.power.usage_count) > 0) {
			pm_runtime_put_sync_suspend(&lnb_data->dev);
			lnb_priv->rpm_suspended = true;
		}
	}
	/* Decrement device usage but not less than 0 */
	if (atomic_read(&demod_data->dev.power.usage_count) > 0) {
		pm_runtime_put_sync_suspend(&demod_data->dev);
		priv->rpm_suspended = true;
	}
#endif

	return ret;
}
EXPORT_SYMBOL(stm_fe_demod_stop);

#ifdef CONFIG_PM

/*
 * Name: Fe_PowerONandReset()
 *
 * Description: Power ON and reset the frontend devices
 *
 * Platform dependent: the fe-power-en GPIO and fe-reset GPIO are common to both FE instances
 *
 * return:  0 if OK, -1 otherwise
 */
static int Fe_PowerONandReset(void)
{
    int  rc = -1;
    struct device_node *np = NULL;
	int                 gpio_power_pin = -1, gpio_reset_pin = -1, ret;

	if (of_machine_is_compatible("st,custom001303"))
	{
		np = of_find_compatible_node(NULL, NULL, "custom001303,gpio");
		if (IS_ERR_OR_NULL(np))
        {
            pr_err("%s: Failed to get node!\n",__func__);
            goto err;
        }

        gpio_power_pin = of_get_named_gpio(np, "fe-power-en", 0);
        ret = gpio_request(gpio_power_pin, "fe-power-en");
        if (ret != 0)
        {
            gpio_power_pin = -1;
            of_node_put(np);
            pr_err("%s: Failed to get fe-power-en gpio!\n",__func__);
            goto err;
        }

        gpio_reset_pin = of_get_named_gpio(np, "fe-reset", 0);
        ret = gpio_request(gpio_reset_pin, "fe-reset");
        if (ret != 0)
        {
            gpio_reset_pin = -1;
            of_node_put(np);
            pr_err("%s: Failed to get fe-reset gpio!\n",__func__);
            goto err;
        }

        /* ---- Disable the FE power and assert the RESETN pin ---- */
        /* Remark: at the time, the power is ON only when coming from cold boot
         * (as it is enabled at system start-up)
         */
        ret  = gpio_direction_output(gpio_power_pin, 0);
        ret |= gpio_direction_output(gpio_reset_pin, 0);
        mdelay(20); /* Wait the FE power ramp-down */

        /* ---- Enable the FE power ---- */
        ret |= gpio_direction_output(gpio_power_pin, 1);
        mdelay(15); /* Wait for power supply to be stabilized */

        /* ---- Release the reset signal ---- */
        ret |= gpio_direction_output(gpio_reset_pin, 1);

        if (ret != 0)
        {
            pr_err("%s: Failed to set fe gpios!\n",__func__);
            goto err;
        }
        else {
            pr_info("%s: OK\n",__func__);
        }

        rc = 0;
	}
    else {
        rc = 0; /* Nothing to do */
    }

err: /* The following is to be done in all the cases */
    if (gpio_power_pin != -1) {
        gpio_free(gpio_power_pin);
    }
    if (gpio_reset_pin != -1) {
        gpio_free(gpio_reset_pin);
    }

    return rc;
}

/*
 * Name: Fe_CtrlPowerOn()
 *
 * Description: if required, power ON and reset this FE instance (and maybe other FE instance(s))
 *
 * Platform dependent: both FE instances are controlled by a unique fe-power-en GPIO and fe-reset GPIO
 *
 * Remark: the stm_fe_demod_resume function is always called in the same order 
 *         than the order of registry of the FE instances to the power management infrastructure
 *         => first call is for instance 0
 */
static void Fe_CtrlPowerOn(struct stm_fe_demod_s *priv)
{
    /* Instance 0 is always the 1rst FE instance restored when leaving CPS */
    if (0 == priv->demod_id)
    {
        Fe_PowerONandReset();
    }
}

/*
 * Name: Fe_PowerDown()
 *
 * Description: set fe-power-en gpio to 0
 *
 * Platform dependent: power down both FE instances
 */
static void Fe_PowerDown(void)
{
    struct device_node *np = NULL;
    int                 gpio_power_pin = -1, ret;

    if (of_machine_is_compatible("st,custom001303"))
    {
        np = of_find_compatible_node(NULL, NULL, "custom001303,gpio");
        if (IS_ERR_OR_NULL(np))
        {
            pr_err("%s: Failed to get node!\n",__func__);
            goto err;
        }

        gpio_power_pin = of_get_named_gpio(np, "fe-power-en", 0);
        ret = gpio_request(gpio_power_pin, "fe-power-en");
        if (ret != 0)
        {
            gpio_power_pin = -1;
            of_node_put(np);
            pr_err("%s: Failed to get fe-power-en gpio!\n\n",__func__);
            goto err;
        }

        ret  = gpio_direction_output(gpio_power_pin, 0);

        if (ret != 0)
        {
            pr_err("%s: Failed to set fe gpios!\n",__func__);
            goto err;
        }
        else {
            pr_info("%s: fe-power-en GPIO set to 0 : OK\n",__func__);
        }
    }

err: /* The following is to be done in all the cases */
    if (gpio_power_pin != -1) {
        gpio_free(gpio_power_pin);
    }
}

/*
 * Name: Fe_CtrlPowerDown()
 *
 * Description: if required, power down this FE instance (and maybe other FE instance(s))
 *
 * Platform dependent: both FE instances are controlled by a unique fe-power-en GPIO
 *
 * Remark: the stm_fe_demod_suspend and stm_fe_demod_shutdown functions are always called in the
 *         reverse order than the order of registry of the FE instances to the power management infrastructure
 *         => last call is for instance 0
 */
static void Fe_CtrlPowerDown(struct stm_fe_demod_s *priv)
{
    /* Instance 0 is always the last FE instance put in suspend (enter CPS) or shutdown (enter DCPS) */
    if (0 == priv->demod_id)
    {
        Fe_PowerDown();
    }
}

/*
 * Name: stm_fe_demod_shutdown()
 *
 * Description: function called when entering DCPS
 *
 * Call stm_fe_demod_suspend in order to:
 * - suspend the demod state machine by blocking demod_task on its guard_sem
 *   (then no more I2C access to FE chips of this instance).
 * - put the instance in standby (transiently)
 * - power down the FE, depending on platform architecture
 *
 */
void stm_fe_demod_shutdown(struct platform_device *pdev)
{
    struct device *dev = NULL;
    int32_t        ret = 0;

    if (pdev != NULL)
    {
        dev = &pdev->dev;

        if (dev != NULL)
        {
            ret = stm_fe_demod_suspend(dev);

            if (ret)
            {
                pr_err("%s: stm_fe_demod_suspend failed!\n", __func__);
            }
        }
        else {
            pr_err("%s: demod obj uninitialiazed....skipping\n", __func__);
        }
    }
    else {
        pr_err("%s: demod obj is not initialised\n", __func__);
    }
}

/*
 * Name: stm_fe_demod_suspend()
 *
 * Description: put tuner and demod in (standby)low power state.
 */
int32_t stm_fe_demod_suspend(struct device *dev)
{
	int32_t ret = 0;
	int32_t id;
	struct stm_fe_demod_s *priv = NULL;

	/* Get the configuration information */
	id = to_platform_device(dev)->id;
	priv = demod_from_id(id);

	if (!priv) {
		pr_warn("%s: demod obj uninitialiazed....skipping\n", __func__);
		return ret;
	}

	if (!priv->ops) {
		pr_warn("%s: demod source is unallocated,skipping\n", __func__);
		return ret;
	}

	if ((priv->bypass_control) || (priv->rpm_suspended)) {
		pr_info("%s: demod suspended in low power\n", __func__);
#ifdef CONFIG_ARCH_STI
		if (priv->demod_data->dev.of_node)
			demod_setup_tsin(priv->config, 1);
#endif
        Fe_CtrlPowerDown(priv); /* If required power down the instance */
		return ret;
	}

	ret = demod_suspend(priv);
    Fe_CtrlPowerDown(priv); /* If required power down the instance */

	if (ret) {
		pr_err("%s: standby operation failed: %d\n", __func__, ret);
		return ret;
	}

#ifdef CONFIG_ARCH_STI
	if (priv->demod_data->dev.of_node)
		demod_setup_tsin(priv->config, 1);
#endif
	return ret;
}

/*
 * Name: stm_fe_demod_resume()
 *
 * Description: resume tuner and demod in normal power state.
 */
int32_t stm_fe_demod_resume(struct device *dev)
{
	int32_t ret = 0;
	int32_t id;
	struct stm_fe_demod_s *priv = NULL;

	/* Get the configuration information */
	id = to_platform_device(dev)->id;
	priv = demod_from_id(id);

	if (!priv) {
		pr_warn("%s: demod obj uninitialized.....skipping\n", __func__);
		return ret;
	}

	if (!priv->ops) {
		pr_warn("%s: demod source is unallocated,skipping\n", __func__);
		return ret;
	}

#ifdef CONFIG_ARCH_STI
	priv->config->pinctrl = devm_pinctrl_get(dev);
	if (IS_ERR(priv->config->pinctrl)) {
		pr_err("%s: pinctrl is not declared\n", __func__);
		ret = -EINVAL;
		return ret;
	}

	if (priv->demod_data->dev.of_node)
		demod_setup_tsin(priv->config, 0);
#endif

	if (priv->bypass_control) {
		pr_info("%s: demod resuming in normal power\n", __func__);
		return ret;
	}
#ifdef CONFIG_ARCH_STI
	if (priv->config->res) {
		ret = regmap_update_bits(priv->config->regmap_ts_map,
			priv->config->syscfg_ts_map, priv->config->ts_map_mask,
			priv->config->ts_map_val);
		if (ret)
			pr_err("%s: Fail:ts mapping control syscfg update\n",
								__func__);
		else
			pr_info("%s: ts mapping control syscfg update ok\n",
								__func__);
	}

    Fe_CtrlPowerOn(priv); /* If necessary power ON and reset the instance */
	ret = demod_restore(priv);
	if (ret) {
		pr_err("%s: wake-up operation failed: %d\n", __func__, ret);
		return ret;
	}
#else
	ret = demod_resume(priv);
	if (ret) {
		pr_err("%s: wake-up operation failed: %d\n", __func__, ret);
		return ret;
	}
#endif
	if (priv->rpm_suspended == true) {
		ret = demod_suspend(priv);
		if (ret) {
			pr_err("%s: demod suspend failed: %d\n", __func__, ret);
			return ret;
		}
	}

	return ret;
}

int32_t stm_fe_demod_freeze(struct device *dev)
{
	int32_t ret = 0;
	int32_t id;
	struct stm_fe_demod_s *priv = NULL;

	/* Get the configuration information */
	id = to_platform_device(dev)->id;
	priv = demod_from_id(id);

	if (!priv) {
		pr_warn("%s: demod obj uninitialized...skipping\n", __func__);
		return ret;
	}

	if (!priv->ops) {
		pr_warn("%s: demod source is unallocated,skipping\n", __func__);
		return ret;
	}

	if ((priv->bypass_control) ||
	    (priv->rpm_suspended)) {
		pr_info("%s: demod suspended in low power\n", __func__);
		if (priv->demod_data->dev.of_node)
			demod_setup_tsin(priv->config, 1);

		return ret;
	}

	ret = demod_suspend(priv);
	if (ret) {
		pr_err("%s: standby operation failed: %d\n", __func__, ret);
		return ret;
	}

	if (priv->demod_data->dev.of_node)
		demod_setup_tsin(priv->config, 1);

	return ret;
}

int32_t stm_fe_demod_restore(struct device *dev)
{
	int32_t ret = 0;
	int32_t id;
	struct stm_fe_demod_s *priv = NULL;

	/* Get the configuration information */
	id = to_platform_device(dev)->id;
	priv = demod_from_id(id);

	if (!priv) {
		pr_warn("%s: demod obj uninitialized...skipping\n", __func__);
		return ret;
	}

	if (!priv->ops) {
		pr_warn("%s: demod source is unallocated,skipping\n", __func__);
		return ret;
	}

	if (priv->demod_data->dev.of_node)
		demod_setup_tsin(priv->config, 0);

	if (priv->bypass_control) {
		pr_info("%s: demod resuming in normal power\n", __func__);
		return ret;
	}

	ret = demod_restore(priv);
	if (ret) {
		pr_err("%s: wake-up operation failed: %d\n", __func__, ret);
		return ret;
	}

	return ret;
}
#ifdef CONFIG_PM_RUNTIME
/*
 * Name: stm_fe_demod_rt_suspend()
 *
 * Description: put tuner and demod in (standby)low power state dynamically (
 * runtime pm).
 */
int32_t stm_fe_demod_rt_suspend(struct device *dev)
{
	int32_t ret = 0;
	int32_t id;
	struct stm_fe_demod_s *priv = NULL;

	/* Get the configuration information */
	id = to_platform_device(dev)->id;
	priv = demod_from_id(id);

	if (!priv) {
		pr_warn("%s: demod obj uninitialiazed....skipping\n", __func__);
		return ret;
	}

	if (!priv->ops) {
		pr_warn("%s: demod source is unallocated,skipping\n", __func__);
		return ret;
	}

	if ((priv->bypass_control) || (priv->rpm_suspended)) {
		pr_info("%s: demod suspended in low power\n", __func__);
		return ret;
	}

	ret = demod_suspend(priv);
	if (ret) {
		pr_err("%s: standby operation failed: %d\n", __func__, ret);
		return ret;
	}

	return ret;
}
/*
 * Name: stm_fe_demod_rt_resume()
 *
 * Description: resume tuner and demod in normal power state dynamically
 * (runtime pm).
 */
int32_t stm_fe_demod_rt_resume(struct device *dev)
{
	int32_t ret = 0;
	int32_t id;
	struct stm_fe_demod_s *priv = NULL;

	/* Get the configuration information */
	id = to_platform_device(dev)->id;
	priv = demod_from_id(id);

	if (!priv) {
		pr_warn("%s: demod obj uninitialized.....skipping\n", __func__);
		return ret;
	}

	if (!priv->ops) {
		pr_warn("%s: demod source is unallocated,skipping\n", __func__);
		return ret;
	}


	if (priv->bypass_control) {
		pr_info("%s: demod resuming in normal power\n", __func__);
		return ret;
	}

	ret = demod_resume(priv);
	if (ret) {
		pr_err("%s: wake-up operation failed: %d\n", __func__, ret);
		return ret;
	}

	return ret;
}

#endif /* CONFIG_PM_RUNTIME */
#endif

struct stm_fe_demod_s *demod_from_name(const char *name)
{
	struct list_head *pos;
	list_for_each(pos, get_demod_list()) {
		struct stm_fe_demod_s *priv;
		priv = list_entry(pos, struct stm_fe_demod_s, list);
		if (!strcmp(priv->demod_name, name))
			return priv;
	}

	return NULL;
}

struct stm_fe_demod_s *demod_from_id(uint32_t id)
{
	struct list_head *pos;
	list_for_each(pos, get_demod_list()) {
		struct stm_fe_demod_s *priv;
		priv = list_entry(pos, struct stm_fe_demod_s, list);
		if (priv->demod_id == id)
			return priv;
	}

	return NULL;
}
