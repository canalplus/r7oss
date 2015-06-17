/************************************************************************
 * Copyright (C) 2011 STMicroelectronics. All Rights Reserved.

This file is part of the STLinuxTV Library.

STLinuxTV is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License version 2 as published by the
Free Software Foundation.

STLinuxTV is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with STLinuxTV; see the file COPYING.  If not, write to the Free Software
Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

The STLinuxTV Library may alternatively be licensed under a proprietary
license from ST.

Source file name : dvb_init.c

Main entry point for the STLinuxTv module

Date        Modification                                    Name
----        ------------                                    --------

************************************************************************/

#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/device.h>
#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_address.h>
#endif
#include "dvb_demod.h"
#include "dvb_tuner.h"
#include "dvb_lnb.h"
#include "dvb_ip.h"
#include "dvb_ipfec.h"
#include "dvb_diseqc.h"
#include "dvb_rf_matrix.h"
#include "dvb_util.h"
#include "dvb_data.h"
#include "dvb_adapt_demux.h"
#ifdef CONFIG_STLINUXTV_CRYPTO
#include "dvb_adapt_ca.h"
#endif
#include <frontend/dvb_stm_fe.h>
#include <frontend/dvb_ipfe.h>

#ifdef CONFIG_STLINUXTV_DECODE_DISPLAY
#include "dvb_module.h"
#include "dvb_audio.h"
#include "dvb_video.h"
#endif
#include "stat.h"

int ctrl_via_stm_fe = FE_ADAP_NO_OPTIONS_DEFAULT;
module_param(ctrl_via_stm_fe, int, 0660);

static int packet_count = 0;
module_param(packet_count, int, 0660);

static int hw_path = 1;
module_param(hw_path, int, 0660);

static int audio_device_nb = 25;
module_param(audio_device_nb, int, 0660);

static int video_device_nb = 25;
module_param(video_device_nb, int, 0660);

static struct dvb_adapter stm_dvb_adapter = {
	.num = -1,
};

#ifdef CONFIG_STLINUXTV_DECODE_DISPLAY
static struct DvbContext_s DvbContext;
#endif
static int dev_id;
static int lnb_dev_id;
static int diseqc_dev_id;
static int rf_matrix_dev_id;
static int ip_dev_id;
static int ipfec_dev_id;
#ifdef CONFIG_OF

int dvb_check_i2c_device_id(unsigned int *base_address, int num)
{
	int ret = -1;
	struct i2c_adapter *adap;
	struct platform_device *pdev;

	adap = i2c_get_adapter(num);
	if (adap == NULL) {
		printk(KERN_ERR "%s: incorrect i2c device id %d\n",
								__func__, num);
		return ret;
	}
	pdev = (struct platform_device *) to_platform_device(adap->dev.parent);
	if (!pdev) {
		pr_err("%s: pdev NULL (i2c id:%d)\n", __func__, num);
		i2c_put_adapter(adap);
		return ret;
	}
	if (pdev->resource->start != (unsigned int) base_address) {
		printk(KERN_ERR"%s:base address not matched %u",
			__func__, (unsigned int) base_address);
		i2c_put_adapter(adap);
		return ret;
	}
	i2c_put_adapter(adap);
	return 0;
}

int demod_config_pdata(struct device_node *iter,
					struct demod_config_s *demod_config_p)
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
		goto demod_pdata_err;
	strlcpy(demod_config_p->demod_name, name, DEMOD_NAME_MAX_LEN);
	/*For remote and PI - I2C adapter is not required*/
	if ((!strncmp(demod_config_p->demod_name, "DUMMY", 5)) ||
			(!strncmp(demod_config_p->demod_name, "REMOTE", 6)))
		goto i2c_adapter_bypass;

	i2c  = of_get_parent(iter);
	if (i2c == NULL) {
		demod_config_p->demod_io.io = DEMOD_IO_MEMORY_MAPPED;
		goto i2c_adapter_bypass;
	} else
		demod_config_p->demod_io.io = DEMOD_IO_I2C;
	i2c_id = of_alias_get_id(i2c, "i2c");
	if (i2c_id < 0) {
		ret = -ENODEV;
		goto demod_pdata_err;
	}

	ret = of_address_to_resource(i2c, 0, &res);
	if (ret < 0)
		pr_err("%s: Fail to get i2c base address\n", __func__);

	ret = dvb_check_i2c_device_id((uint32_t *)res.start, i2c_id);
	if (!ret) {
		demod_config_p->demod_io.bus = i2c_id;
		pr_debug("%s: demod i2c bus id = %d", __func__, i2c_id);
	} else {
		pr_err("%s: wrong demod i2c bus id", __func__);
		goto demod_pdata_err;
	}

	addr = of_get_property(iter, "reg", NULL);
	if (addr) {/*only 8 bits of address are useful*/
		demod_config_p->demod_io.address = (*addr) >> 24;
	} else {
		pr_err("%s: Fail to get demod address\n", __func__);
		goto demod_pdata_err;
	}

	of_property_read_u32(iter,
			"demod_i2c_route", &demod_config_p->demod_io.route);
	of_property_read_u32(iter,
			"demod_i2c_speed", &demod_config_p->demod_io.baud);

i2c_adapter_bypass:
	ret = of_property_read_u32(iter,
			"lla_selection", &demod_config_p->demod_lla_sel);
	if (ret) {
		pr_err("%s: invalid lla_selection", __func__);
		goto demod_pdata_err;
	}

	if (demod_config_p->demod_lla_sel != DEMOD_LLA_AUTO
			&& demod_config_p->demod_lla_sel != DEMOD_LLA_FORCED) {
		pr_err("%s: wrong value of lla_selection", __func__);
		goto demod_pdata_err;
	}
	if (demod_config_p->demod_lla_sel == DEMOD_LLA_FORCED) {
		ret = of_property_read_string(iter, "lla_driver", &lla);
		if (ret)
			goto demod_pdata_err;
		if ((strncmp(lla, "st,lla", sizeof("st,lla")))
			&& (strncmp(lla, "custom,lla", sizeof("custom,lla")))) {
			pr_err("%s: lla_driver not correctly set\n", __func__);
			goto demod_pdata_err;
		}
		strlcpy(demod_config_p->demod_lla, lla, DEMOD_NAME_MAX_LEN);
	}

	if (demod_config_p->ctrl == STM_FE)
		strcat(demod_config_p->demod_name, POST_FIX);

	ret = of_property_read_u32(iter,
		    "demod_clock_type", &demod_config_p->clk_config.clk_type);
	if (ret)
		goto demod_pdata_err;

	of_property_read_u32(iter,
		    "demod_clock_freq", &demod_config_p->clk_config.demod_clk);
	ret = of_property_read_u32(iter,
			"demod_ts_config", &demod_config_p->ts_out);
	if (ret)
		goto demod_pdata_err;

	of_property_read_u32(iter,
			"demod_ts_clock", &demod_config_p->ts_clock);
	of_property_read_u32(iter,
			"demod_ts_tag", &demod_config_p->ts_tag);
	ret = of_property_read_u32(iter,
			"demod_tsin_id", &demod_config_p->demux_tsin_id);
	if (ret) {
		pr_debug("%s: invalid demod_tsin_id", __func__);
		goto demod_pdata_err;
	}

	of_property_read_u32(iter,
			"demod_custom_option", &demod_config_p->custom_flags);
	of_property_read_u32(iter,
			"demod_tuner_if", &demod_config_p->tuner_if);
	of_property_read_u32(iter,
			"demod_roll_off", &demod_config_p->roll_off);
	of_property_read_u32(iter,
			"remote_ip", &demod_config_p->remote_ip);
	of_property_read_u32(iter,
			"remote_port", &demod_config_p->remote_port);
demod_pdata_err:
	if (ret)
		pr_err("%s: demod config not set properly\n", __func__);

	return ret;
}

int tuner_config_pdata(struct device_node *iter,
					struct demod_config_s *demod_config_p)
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

	strlcpy(demod_config_p->tuner_name, name, DEMOD_NAME_MAX_LEN);

	i2c  = of_get_parent(iter);
	if (i2c == NULL) {
		demod_config_p->tuner_io.io = DEMOD_IO_MEMORY_MAPPED;
		goto tuner_i2c_bypass;
	} else
		demod_config_p->tuner_io.io = DEMOD_IO_I2C;

	i2c_id = of_alias_get_id(i2c, "i2c");
	if (i2c_id < 0) {
		ret = -EINVAL;
		goto tuner_pdata_err;
	}

	ret = of_address_to_resource(i2c, 0, &res);
	if (ret < 0)
		pr_err("%s: Fail to get i2c bus address\n", __func__);

	ret = dvb_check_i2c_device_id((uint32_t *)res.start, i2c_id);
	if (!ret) {
		demod_config_p->tuner_io.bus = i2c_id;
		pr_debug("%s: tuner i2c bus id = %d", __func__, i2c_id);
	} else {
		pr_err("%s: wrong tuner i2c bus id ", __func__);
		goto tuner_pdata_err;
	}

	addr = of_get_property(iter, "reg", NULL);
	if (addr) {
		demod_config_p->tuner_io.address = (*addr) >> 24;
	} else {
		pr_err("%s: Fail to get tuner address\n", __func__);
		goto tuner_pdata_err;
	}
	of_property_read_u32(iter,
			"tuner_i2c_route" , &demod_config_p->tuner_io.route);
	of_property_read_u32(iter,
			"tuner_i2c_speed" , &demod_config_p->tuner_io.baud);

tuner_i2c_bypass:
	ret = of_property_read_u32(iter,
		"lla_selection", &demod_config_p->tuner_lla_sel);
	if (ret) {
		pr_err("%s: invalid lla_selection", __func__);
		goto tuner_pdata_err;
	}

	if (demod_config_p->tuner_lla_sel != DEMOD_LLA_AUTO
			&& demod_config_p->tuner_lla_sel != DEMOD_LLA_FORCED) {
		pr_err("%s: wrong value of lla_selection", __func__);
		goto tuner_pdata_err;
	}

	if (demod_config_p->tuner_lla_sel == DEMOD_LLA_FORCED) {
		ret = of_property_read_string(iter, "lla_driver", &lla);
		if (ret)
			goto tuner_pdata_err;
		if ((strncmp(lla, "st,lla", sizeof("st,lla")))
			&& (strncmp(lla, "custom,lla", sizeof("custom,lla")))) {
			pr_err("%s: lla_driver not correctly set\n", __func__);
			goto tuner_pdata_err;
		}
		strlcpy(demod_config_p->tuner_lla, lla, DEMOD_NAME_MAX_LEN);
	}

	if (demod_config_p->ctrl == STM_FE)
		strcat(demod_config_p->tuner_name, POST_FIX);

	of_property_read_u32(iter,
		"tuner_clock_freq" , &demod_config_p->clk_config.tuner_clk);
	of_property_read_u32(iter,
	 "tuner_clock_div" , &demod_config_p->clk_config.tuner_clkout_divider);

tuner_pdata_err:
	if (ret)
		pr_err("%s: tuner config not set properly\n", __func__);

	return ret;
}

static void *stm_dvb_get_pdata(struct platform_device *pdev)
{
	int ret = 0;
	struct device_node *np = pdev->dev.of_node;
	struct device_node *fe;	/*parent node*/
	struct device_node *iter;
	struct demod_config_s *demod_config_p;

	demod_config_p =
		devm_kzalloc(&pdev->dev, sizeof(*demod_config_p), GFP_KERNEL);
	if (!demod_config_p) {
		printk(KERN_ERR "%s : demod paltform data alloaction fail",
								    __func__);
		ret = -ENOMEM;
		goto out;
	}

	fe = of_get_parent(np);
	if (fe == NULL) {
		ret = -EINVAL;
		goto out;
	}
	ret = of_property_read_u32(fe, "control", &demod_config_p->ctrl);
	if (ret)
		goto out;
	iter = of_parse_phandle(np, "dvb_demod", 0);
	if (iter == NULL) {
		ret = -EINVAL;
		goto out;
	}
	iter = of_parse_phandle(iter, "demod", 0);
	if (iter == NULL) {
		ret = -EINVAL;
		goto out;
	}
	ret = demod_config_pdata(iter, demod_config_p);
	if (ret)
		goto out;

	iter = of_parse_phandle(np, "dvb_demod", 0);
	if (iter == NULL) {
		ret = -EINVAL;
		goto out;
	}
	iter = of_parse_phandle(iter, "tuner", 0);
	if (!iter)
		goto out;
	ret = tuner_config_pdata(iter, demod_config_p);
	if (ret)
		goto out;

out:
	if (ret)
		return ERR_PTR(ret);

	return demod_config_p;
}

#else
static void *stm_dvb_get_pdata(struct platform_device *stm_fe_demod_data)
{
	return NULL;
}
#endif
static int stm_dvb_demod_attach(struct platform_device *dvb_demod_data)
{
	struct stm_platdev_data *plat_dev_data_p;
	struct stm_dvb_demod_s *demod_p;
	struct demod_config_s *demod_config_p;
	int ret = 0;

	demod_p = kzalloc(sizeof(struct stm_dvb_demod_s), GFP_KERNEL);
	if (!demod_p) {
		printk(KERN_ERR "%s(): Unable to initiate demod context\n",
		       __func__);
		ret = -ENOMEM;
		goto demod_attach_done;
	}

	if (dvb_demod_data->dev.of_node) {
		demod_config_p = stm_dvb_get_pdata(dvb_demod_data);
		 if (IS_ERR_OR_NULL(demod_config_p)) {
			kfree(demod_p);
			return PTR_ERR(demod_config_p);
		}
		demod_p->demod_id = dev_id;
		dvb_demod_data->id = dev_id;
		dev_id++;
		demod_p->demod_config = demod_config_p;
	} else {
		plat_dev_data_p = dvb_demod_data->dev.platform_data;
		demod_p->demod_config = plat_dev_data_p->config_data;
		demod_p->demod_id = dvb_demod_data->id;
	}

	/* Prepare the demod context */
	addObjectToList(&demod_p->list, DEMOD);

	sprintf(demod_p->dvb_demod_name, "stm_dvb_demod.%d", demod_p->demod_id);

	printk(KERN_INFO "%s(): Configuring demod %s Demod name %s ... ",
	       __func__, demod_p->demod_config->demod_name,
	       dvb_demod_data->name);

	/* Get the I2C adapter for demod/tuner access  */
	/*For dummy and PI I2C adapter is not required*/
	if ((!strncmp(demod_p->demod_config->demod_name, "DUMMY", 5)) ||
		(!strncmp(demod_p->demod_config->demod_name, "REMOTE", 6))) {
		printk(KERN_INFO "%s(): No need to get i2c adapter\n",
								__func__);
		goto i2c_adapter_bypass;
	}
	demod_p->demod_i2c =
	    i2c_get_adapter(demod_p->demod_config->demod_io.bus);
	if (!demod_p->demod_i2c) {
		printk(KERN_ERR
		       "%s(): i2c_get_adapter failed for demod i2c bus %d\n",
		       __func__, demod_p->demod_config->demod_io.bus);
		ret = -EBUSY;
		goto demod_attach_failed;
	}
	demod_p->tuner_i2c =
	    i2c_get_adapter(demod_p->demod_config->tuner_io.bus);
	if (demod_p->tuner_i2c == NULL) {
		printk(KERN_ERR
		       "%s(): i2c_get_adapter failed for tuner i2c bus %d\n",
		       __func__, demod_p->demod_config->tuner_io.bus);
		ret = -EBUSY;
		goto demod_attach_failed;
	}

i2c_adapter_bypass:
	demod_p->demod = demod_attach(demod_p);
	if (!demod_p->demod) {
		printk(KERN_ERR "%s(): demod_attach failed!\n", __func__);
		ret = -ENODEV;
		goto demod_attach_failed;
	}

	ret = tuner_attach(demod_p);
	if (ret) {
		printk(KERN_ERR "%s(): tuner_attach failed!\n", __func__);
		ret = -ENODEV;
		goto demod_attach_failed;
	}

	ret = dvb_register_frontend(&stm_dvb_adapter, demod_p->demod);
	if (ret) {
		printk(KERN_ERR "%s() dvb_register_frontend failed ret = %d!\n",
		       __func__, ret);
		goto demod_attach_failed;
	}

	goto demod_attach_done;

demod_attach_failed:
	if (demod_p->tuner_i2c)
		i2c_put_adapter(demod_p->tuner_i2c);
	if (demod_p->demod_i2c)
		i2c_put_adapter(demod_p->demod_i2c);

	removeObjectFromList(&demod_p->list, DEMOD);
	kfree(demod_p);

demod_attach_done:
	return ret;
}

static int stm_dvb_demod_remove(struct platform_device *dvb_demod_data)
{
	struct stm_dvb_demod_s *dvb_demod;

	GET_DEMOD_OBJECT(dvb_demod, dvb_demod_data->id);
	if (unlikely(!dvb_demod)) {
		printk(KERN_ERR "%s: dvb_demod not registered, nothing to do\n",
								      __func__);
		goto demod_remove_done;
	}

	/*
	 * Unregister frontend device
	 */
	dvb_unregister_frontend(dvb_demod->demod);

	if (demod_detach(dvb_demod))
		printk(KERN_ERR "%s: Failed to remove demod configuration\n",
								      __func__);
	/*
	 * Free the memory allocated during the native attach
	 */

	if (dvb_demod->dvb_demod_config)
		kfree(dvb_demod->dvb_demod_config);
	if (dvb_demod->dvb_tuner_config)
		kfree(dvb_demod->dvb_tuner_config);

	/*
	 * Put down the i2c adapters
	 */
	if (dvb_demod->tuner_i2c)
		i2c_put_adapter(dvb_demod->tuner_i2c);
	if (dvb_demod->demod_i2c)
		i2c_put_adapter(dvb_demod->demod_i2c);

	list_del(&dvb_demod->list);

	/*
	 * Release lnb, tuner, frontend. ST specific tuners are not
	 * released from this call, as they aren't registered
	 */
	dvb_frontend_detach(dvb_demod->demod);

	kfree(dvb_demod);

demod_remove_done:
	return 0;
}
#ifdef CONFIG_OF
static struct of_device_id stm_dvb_match[] = {
	{
		.compatible = "st,dvb_demod",
	},
		{},
};
#endif
static struct platform_driver stm_dvb_demod_driver = {
	.driver = {
		   .name = "stm_dvb_demod",
		   .owner = THIS_MODULE,
#ifdef CONFIG_OF
		   .of_match_table = of_match_ptr(stm_dvb_match),
#endif
		   },
	.probe = stm_dvb_demod_attach,
	.remove = stm_dvb_demod_remove
};

#ifdef CONFIG_OF
static void *stm_dvb_get_lnb_pdata(struct platform_device *pdev)
{
	int ret = 0;
	struct device_node *np = pdev->dev.of_node;
	struct device_node *fe;
	struct device_node *iter;
	struct device_node *i2c;
	struct lnb_config_s *lnb_config_p;
	const char *name;
	const char *lla;
	const unsigned int *addr;
	struct resource res;
	int i2c_id;

	lnb_config_p =
		devm_kzalloc(&pdev->dev, sizeof(*lnb_config_p), GFP_KERNEL);
	if (!lnb_config_p) {
		printk(KERN_ERR "%s : lnb platform data allocation fail",
								   __func__);
		ret = -ENOMEM;
		goto lnb_pdata_err;
	}
	fe = of_get_parent(np);
	if (fe == NULL) {
		ret = -EINVAL;
		goto lnb_pdata_err;
	}
	ret = of_property_read_u32(fe, "control", &lnb_config_p->ctrl);
	if (ret)
		goto lnb_pdata_err;
	iter = of_parse_phandle(np, "dvb_lnb", 0);
	if (iter == NULL) {
		ret = -EINVAL;
		goto lnb_pdata_err;
	}
	iter = of_parse_phandle(iter, "lnb", 0);
	if (iter == NULL) {
		ret = -EINVAL;
		goto lnb_pdata_err;
	}
	ret = of_property_read_string(iter, "model", &name);
	if (ret)
		goto lnb_pdata_err;
	strlcpy(lnb_config_p->lnb_name, name, LNB_NAME_MAX_LEN);

	i2c  = of_get_parent(iter);
	if (i2c == NULL) {
		lnb_config_p->lnb_io.io = LNB_IO_MEMORY_MAPPED;
		goto lnb_i2c_bypass;
	} else
		lnb_config_p->lnb_io.io = LNB_IO_I2C;
	i2c_id = of_alias_get_id(i2c, "i2c");
	if (i2c_id < 0) {
		ret = -ENODEV;
		goto lnb_pdata_err;
	}
	ret = of_address_to_resource(i2c, 0, &res);
	if (ret < 0)
		pr_err("%s: Fail to get i2c base address\n", __func__);

	ret = dvb_check_i2c_device_id((unsigned int *) res.start, i2c_id);
	if (!ret) {
		lnb_config_p->lnb_io.bus = i2c_id;
		pr_debug("%s: lnb i2c bus id = %d", __func__, i2c_id);
	} else {
		pr_err("%s: wrong lnb i2c bus id", __func__);
		goto lnb_pdata_err;
	}

	addr = of_get_property(iter, "reg", NULL);
	if (addr)
		lnb_config_p->lnb_io.address = (*addr) >> 24;
	else {
		pr_err("%s: Fail to get lnb address\n", __func__);
		goto lnb_pdata_err;
	}
	of_property_read_u32(iter,
			"lnb_i2c_route", &lnb_config_p->lnb_io.route);
	of_property_read_u32(iter,
			"lnb_i2c_speed", &lnb_config_p->lnb_io.baud);
lnb_i2c_bypass:
	ret = of_property_read_u32(iter,
		"lla_selection", &lnb_config_p->lla_sel);
	if (ret) {
		pr_err("%s: invalid lla_selection", __func__);
		goto lnb_pdata_err;
	}
	if (lnb_config_p->lla_sel != LNB_LLA_AUTO
			&& lnb_config_p->lla_sel != LNB_LLA_FORCED) {
		pr_err("%s: wrong value of lla_selection", __func__);
		goto lnb_pdata_err;
	}
	if (lnb_config_p->lla_sel == LNB_LLA_FORCED) {
		ret = of_property_read_string(iter, "lla_driver", &lla);
		if (ret)
			goto lnb_pdata_err;

		if ((strncmp(lla, "st,lla", sizeof("st,lla")))
			&& (strncmp(lla, "custom,lla", sizeof("custom,lla")))) {
			pr_err("%s: lla_driver not correctly set\n", __func__);
			goto lnb_pdata_err;
		}
		strlcpy(lnb_config_p->lla, lla, LNB_NAME_MAX_LEN);
	}

	if (lnb_config_p->ctrl == STM_FE)
		strcat(lnb_config_p->lnb_name, POST_FIX);

	of_property_read_u32(iter,
		    "lnb_custom_option", &lnb_config_p->cust_flags);
	ret = of_property_read_u32(iter,
			"lnb_vol_select", &lnb_config_p->be_pio.volt_sel);
	if (ret)
		goto lnb_pdata_err;
	ret = of_property_read_u32(iter,
			"lnb_vol_enable", &lnb_config_p->be_pio.volt_en);
	if (ret)
		goto lnb_pdata_err;
	ret = of_property_read_u32(iter,
			"lnb_tone_select", &lnb_config_p->be_pio.tone_sel);

lnb_pdata_err:
	if (ret) {
		pr_err("%s: lnb config not properly set\n", __func__);
		return ERR_PTR(ret);
	}
	return lnb_config_p;
}
#else
static void *stm_dvb_get_lnb_pdata(struct platform_device *stm_fe_demod_data)
{
	return NULL;
}
#endif

static int stm_dvb_lnb_attach(struct platform_device *dvb_lnb_data)
{
	struct stm_platdev_data *plat_dev_data_p;
	struct stm_dvb_lnb_s *lnb;
	int32_t ret = 0;
	struct lnb_config_s *lnb_config_p;

	lnb = kzalloc(sizeof(struct stm_dvb_lnb_s), GFP_KERNEL);
	if (lnb == NULL) {
		printk(KERN_ERR
		       "%s(): Unable to allocate memory for lnb context\n",
		       __func__);
		ret = -ENOMEM;
		goto lnb_attach_done;
	}
	if (dvb_lnb_data->dev.of_node) {
		lnb_config_p = stm_dvb_get_lnb_pdata(dvb_lnb_data);
		if (IS_ERR_OR_NULL(lnb_config_p)) {
			kfree(lnb);
			return PTR_ERR(lnb_config_p);
		}
		lnb->lnb_id = lnb_dev_id;
		dvb_lnb_data->id = lnb_dev_id;
		lnb_dev_id++;
		lnb->lnb_config = lnb_config_p;
	} else {
		plat_dev_data_p = dvb_lnb_data->dev.platform_data;
		lnb->lnb_config = plat_dev_data_p->config_data;
		lnb->lnb_id = dvb_lnb_data->id;
	}

	addObjectToList(&lnb->list, LNB);

	sprintf(lnb->dvb_lnb_name, "stm_dvb_lnb.%d", lnb->lnb_id);

	GET_DEMOD_OBJECT(lnb->dvb_demod, lnb->lnb_id);
	if (lnb->dvb_demod == NULL) {
		printk(KERN_ERR
		       "%s(): Invalid demod object, cannot attach lnb\n",
		       __func__);
		ret = -ENXIO;
		goto lnb_attach_failed;
	}

	lnb->i2c = i2c_get_adapter(lnb->lnb_config->lnb_io.bus);
	if (lnb->i2c == NULL) {
		printk(KERN_ERR
		       "%s(): i2c_get_adapter failed for lnb i2c bus %d\n",
		       __func__, lnb->lnb_config->lnb_io.bus);
		ret = -EBUSY;
		goto lnb_attach_failed;
	}

	if (!lnb_attach(lnb)) {
		printk(KERN_ERR "%s(): Lnb_attach failed!\n", __func__);
		ret = -ENODEV;
		goto lnb_attach_failed;
	}

	printk(KERN_INFO "%s(): Attached lnb %s to demod %s ..\n", __func__,
	       lnb->dvb_lnb_name, lnb->dvb_demod->dvb_demod_name);

	goto lnb_attach_done;

lnb_attach_failed:
	if (lnb->i2c)
		i2c_put_adapter(lnb->i2c);

	removeObjectFromList(&lnb->list, LNB);
	kfree(lnb);

lnb_attach_done:
	return ret;
}

static int stm_dvb_lnb_remove(struct platform_device *dvb_lnb_data)
{
	struct stm_dvb_lnb_s *dvb_lnb;

	GET_LNB_OBJECT(dvb_lnb, dvb_lnb_data->id);
	if (unlikely(!dvb_lnb)) {
		printk(KERN_ERR "%s: dvb_lnb not registered, nothing to do\n",
								      __func__);
		goto lnb_remove_done;
	}

	if (lnb_detach(dvb_lnb))
		printk(KERN_ERR "%s: Failed to remove LNB configuration\n",
								      __func__);

	if (dvb_lnb->i2c)
		i2c_put_adapter(dvb_lnb->i2c);

	list_del(&dvb_lnb->list);

	kfree(dvb_lnb);

lnb_remove_done:
	return 0;
}

#ifdef CONFIG_OF
static struct of_device_id stm_lnb_match[] = {
	{
		.compatible = "st,dvb_lnb",
	},
		{},
};
#endif
static struct platform_driver stm_dvb_lnb_driver = {
	.driver = {
		   .name = "stm_dvb_lnb",
		   .owner = THIS_MODULE,
#ifdef CONFIG_OF
		   .of_match_table = of_match_ptr(stm_lnb_match),
#endif
		   },
	.probe = stm_dvb_lnb_attach,
	.remove = stm_dvb_lnb_remove
};
#ifdef CONFIG_OF
static void *stm_dvb_get_diseqc_pdata(struct platform_device *pdev)
{
	int ret = 0;
	struct device_node *np = pdev->dev.of_node;
	struct device_node *fe;
	struct device_node *iter;
	struct diseqc_config_s *diseqc_config_p;
	const char *name;
	const char *lla;

	diseqc_config_p =
		devm_kzalloc(&pdev->dev, sizeof(*diseqc_config_p), GFP_KERNEL);
	if (!diseqc_config_p) {
		pr_err("%s : diseqc platform data allocation fail", __func__);
		ret = -ENOMEM;
		goto diseqc_pdata_err;
	}
	fe = of_get_parent(np);
	if (fe == NULL) {
		ret = -EINVAL;
		goto diseqc_pdata_err;
	}
	ret = of_property_read_u32(fe, "control", &diseqc_config_p->ctrl);
	if (ret)
		goto diseqc_pdata_err;
	iter = of_parse_phandle(np, "dvb_diseqc", 0);
	if (iter == NULL) {
		ret = -EINVAL;
		goto diseqc_pdata_err;
	}
	iter = of_parse_phandle(iter, "diseqc", 0);
	if (iter == NULL) {
		ret = -EINVAL;
		goto diseqc_pdata_err;
	}
	ret = of_property_read_string(iter, "model", &name);
	if (ret)
		goto diseqc_pdata_err;
	strlcpy(diseqc_config_p->diseqc_name, name, DISEQC_NAME_MAX_LEN);

	ret = of_property_read_string(iter, "lla_driver", &lla);
	if (ret)
		goto diseqc_pdata_err;
	if ((strncmp(lla, "st,lla", sizeof("st,lla")))
			&& (strncmp(lla, "custom,lla", sizeof("custom,lla")))) {
		pr_err("%s: Fail to select diseqc lla\n", __func__);
		goto diseqc_pdata_err;
	}
	strlcpy(diseqc_config_p->lla, lla, DISEQC_NAME_MAX_LEN);

	if (diseqc_config_p->ctrl == STM_FE)
		strcat(diseqc_config_p->diseqc_name, POST_FIX);

	ret = of_property_read_u32(iter,
			"diseqc_ver", &diseqc_config_p->ver);
	if (ret)
		goto diseqc_pdata_err;
	of_property_read_u32(iter,
			"diseqc_custom_option", &diseqc_config_p->cust_flags);

diseqc_pdata_err:
	if (ret) {
		pr_err("%s: diseqc config not properly set\n", __func__);
		return ERR_PTR(ret);
	}
	return diseqc_config_p;
}
#else
static void *stm_dvb_get_diseqc_pdata(struct platform_device *stm_fe_demod_data)
{
	return NULL;
}
#endif

static int stm_dvb_diseqc_attach(struct platform_device *dvb_diseqc_data)
{
	struct stm_platdev_data *plat_dev_data_p;
	struct stm_dvb_diseqc_s *diseqc;
	struct diseqc_config_s *diseqc_config_p;
	int32_t ret = 0;

	diseqc = kzalloc(sizeof(struct stm_dvb_diseqc_s), GFP_KERNEL);
	if (diseqc == NULL) {
		printk(KERN_ERR
		       "%s(): Unable to allocate memory for diseqc context\n",
		       __func__);
		ret = -ENOMEM;
		goto diseqc_attach_done;
	}
	if (dvb_diseqc_data->dev.of_node) {
		diseqc_config_p =
				stm_dvb_get_diseqc_pdata(dvb_diseqc_data);
		if (IS_ERR_OR_NULL(diseqc_config_p)) {
			kfree(diseqc);
			return PTR_ERR(diseqc_config_p);
		}
		diseqc->diseqc_id = diseqc_dev_id;
		dvb_diseqc_data->id = diseqc_dev_id;
		diseqc_dev_id++;
		diseqc->diseqc_config = diseqc_config_p;
	} else {
		plat_dev_data_p = dvb_diseqc_data->dev.platform_data;
		diseqc->diseqc_config = plat_dev_data_p->config_data;
		diseqc->diseqc_id = dvb_diseqc_data->id;
	}
	addObjectToList(&diseqc->list, DISEQC);

	sprintf(diseqc->dvb_diseqc_name, "stm_dvb_diseqc.%d",
		diseqc->diseqc_id);

	GET_DEMOD_OBJECT(diseqc->dvb_demod, diseqc->diseqc_id);
	if (diseqc->dvb_demod == NULL) {
		printk(KERN_ERR
		       "%s() Invalid demod object, cannot attach diseqc\n",
		       __func__);
		ret = -ENXIO;
		goto diseqc_attach_failed;
	}

	if (!diseqc_attach(diseqc)) {
		printk(KERN_ERR "%s(): diseqc attached failed\n", __func__);
		ret = -ENODEV;
		goto diseqc_attach_failed;
	}
	printk(KERN_INFO "%s(): Attached diseqc %s to demod %s ..\n", __func__,
	       diseqc->dvb_diseqc_name, diseqc->dvb_demod->dvb_demod_name);

	goto diseqc_attach_done;

diseqc_attach_failed:
	removeObjectFromList(&diseqc->list, DISEQC);
	kfree(diseqc);

diseqc_attach_done:
	return ret;
}

static int stm_dvb_diseqc_remove(struct platform_device *dvb_diseqc_data)
{
	struct stm_dvb_diseqc_s *diseqc;

	GET_DISEQC_OBJECT(diseqc, dvb_diseqc_data->id);
	if (unlikely(!diseqc)) {
		printk(KERN_ERR "%s: dvb_diseqc not registered, nothing to do\n",
								      __func__);
		goto diseqc_remove_done;
	}

	if (diseqc_detach(diseqc))
		printk(KERN_ERR "%s: Failed to remove DISEQC configuration\n",
								      __func__);
	list_del(&diseqc->list);

	kfree(diseqc);

diseqc_remove_done:
	return 0;
}
#ifdef CONFIG_OF
static struct of_device_id stm_diseqc_match[] = {
	{
		.compatible = "st,dvb_diseqc",
	},
		{},
};
#endif
static struct platform_driver stm_dvb_diseqc_driver = {
	.driver = {
		   .name = "stm_dvb_diseqc",
		   .owner = THIS_MODULE,
#ifdef CONFIG_OF
		   .of_match_table = of_match_ptr(stm_diseqc_match),
#endif
		   },
	.probe = stm_dvb_diseqc_attach,
	.remove = stm_dvb_diseqc_remove
};
#ifdef CONFIG_OF
static void *stm_dvb_get_rf_matrix_pdata(struct platform_device *pdev)
{
	int ret = 0;
	struct device_node *np = pdev->dev.of_node;
	struct device_node *fe;
	struct device_node *iter;
	struct rf_matrix_config_s *rf_matrix_config_p;
	const char *name;
	const char *lla;

	rf_matrix_config_p = devm_kzalloc(&pdev->dev,
				       sizeof(*rf_matrix_config_p), GFP_KERNEL);
	if (!rf_matrix_config_p) {
		pr_err("%s : rf_matrix paltform data allocation fail",
								   __func__);
		ret = -ENOMEM;
		goto rfm_pdata_err;
	}
	fe = of_get_parent(np);
	if (fe == NULL) {
		ret = -EINVAL;
		goto rfm_pdata_err;
	}
	ret = of_property_read_u32(fe, "control", &rf_matrix_config_p->ctrl);
	if (ret)
		goto rfm_pdata_err;
	ret = of_property_read_u32(fe, "group_id", &rf_matrix_config_p->grp_id);
	if (ret)
		goto rfm_pdata_err;
	iter = of_parse_phandle(np, "dvb_rf_matrix", 0);
	if (iter == NULL) {
		ret = -EINVAL;
		goto rfm_pdata_err;
	}
	iter = of_parse_phandle(iter, "rf_matrix", 0);
	if (iter == NULL) {
		ret = -EINVAL;
		goto rfm_pdata_err;
	}
	ret = of_property_read_string(iter, "model", &name);
	if (ret)
		goto rfm_pdata_err;
	strlcpy(rf_matrix_config_p->name, name, RF_MATRIX_NAME_MAX_LEN);
	ret = of_property_read_u32(iter,
		"lla_selection", &rf_matrix_config_p->lla_sel);
	if (ret) {
		pr_err("%s: invalid lla_selection", __func__);
		goto rfm_pdata_err;
	}
	if (rf_matrix_config_p->lla_sel != RF_MATRIX_LLA_AUTO
			&& rf_matrix_config_p->lla_sel !=
							RF_MATRIX_LLA_FORCED) {
		pr_err("%s: wrong value of lla_selection", __func__);
		goto rfm_pdata_err;
	}

	if (rf_matrix_config_p->lla_sel == RF_MATRIX_LLA_FORCED) {
		ret = of_property_read_string(iter, "lla_driver", &lla);
		if (ret)
			goto rfm_pdata_err;
		if ((strncmp(lla, "st,lla", sizeof("st,lla"))) &&
			(strncmp(lla, "custom,lla", sizeof("custom,lla")))) {
			pr_err("%s: lla is not correctly set in dt\n",
					__func__);
			goto rfm_pdata_err;
		}
		strlcpy(rf_matrix_config_p->lla, lla, RF_MATRIX_NAME_MAX_LEN);
	}

	if (rf_matrix_config_p->ctrl == STM_FE)
		strcat(rf_matrix_config_p->name, POST_FIX);

	of_property_read_u32(iter, "max_input",
					&rf_matrix_config_p->max_input);
	ret = of_property_read_u32(iter, "input_id",
					&rf_matrix_config_p->input_id);
	if (ret)
		goto rfm_pdata_err;

	/*
	 * Presently rf matrix device contol is via tuner device , it is not
	 * required to parse i2c node information. Incase some future device
	 * (i2c/memory mapped) with rfmatrix capability needs to be supported ,
	 * IO information will need to be fecthed as done for demod devices
	 *
	 * */
rfm_pdata_err:
	if (ret) {
		pr_err("%s: rfm config not properly set\n", __func__);
		return ERR_PTR(ret);
	}

	return rf_matrix_config_p;

}
#else
static void *
stm_dvb_get_rf_matrix_pdata(struct platform_device *stm_fe_demod_data)
{
	return NULL;
}
#endif

static int stm_dvb_rf_matrix_attach(struct platform_device *dvb_rf_matrix_data)
{
	struct stm_platdev_data *plat_dev_data_p;
	struct stm_dvb_rf_matrix_s *rf_matrix;
	struct rf_matrix_config_s *rf_matrix_config_p;
	int32_t ret = 0;

	rf_matrix = kzalloc(sizeof(struct stm_dvb_rf_matrix_s), GFP_KERNEL);
	if (rf_matrix == NULL) {
		printk(KERN_ERR
		       "%s: Unable to allocate memory for rf_matrix context\n",
		       __func__);
		ret = -ENOMEM;
		goto rf_matrix_attach_done;
	}
	if (dvb_rf_matrix_data->dev.of_node) {
		rf_matrix_config_p =
				stm_dvb_get_rf_matrix_pdata(dvb_rf_matrix_data);
		if (IS_ERR_OR_NULL(rf_matrix_config_p)) {
			kfree(rf_matrix);
			return PTR_ERR(rf_matrix_config_p);
		}
		rf_matrix->id = rf_matrix_dev_id;
		dvb_rf_matrix_data->id = rf_matrix_dev_id;
		rf_matrix_dev_id++;
		rf_matrix->config = rf_matrix_config_p;
	} else {
		plat_dev_data_p = dvb_rf_matrix_data->dev.platform_data;
		rf_matrix->config = plat_dev_data_p->config_data;
		rf_matrix->id = dvb_rf_matrix_data->id;
	}
	addObjectToList(&rf_matrix->list, RF_MATRIX);

	sprintf(rf_matrix->dvb_rf_matrix_name, "stm_dvb_rf_matrix.%d",
		rf_matrix->id);

	GET_DEMOD_OBJECT(rf_matrix->dvb_demod, rf_matrix->id);
	if (rf_matrix->dvb_demod == NULL) {
		printk(KERN_ERR
		       "%s: Invalid demod object, cannot attach rf_matrix\n",
		       __func__);
		ret = -ENXIO;
		goto rf_matrix_attach_failed;
	}

	if (!rf_matrix_attach(rf_matrix)) {
		printk(KERN_ERR "%s: rf_matrix attached failed\n", __func__);
		ret = -ENODEV;
		goto rf_matrix_attach_failed;
	}
	printk(KERN_INFO "%s: Attached rf_matrix %s to demod %s ..\n", __func__,
					rf_matrix->dvb_rf_matrix_name,
					rf_matrix->dvb_demod->dvb_demod_name);

	goto rf_matrix_attach_done;

rf_matrix_attach_failed:
	removeObjectFromList(&rf_matrix->list, RF_MATRIX);
	kfree(rf_matrix);

rf_matrix_attach_done:
	return ret;
}

static int stm_dvb_rf_matrix_remove(struct platform_device *dvb_rf_matrix_data)
{
	struct stm_dvb_rf_matrix_s *dvb_rf_matrix;

	GET_RF_MATRIX_OBJECT(dvb_rf_matrix, dvb_rf_matrix_data->id);
	if (unlikely(!dvb_rf_matrix)) {
		printk(KERN_ERR "%s: dvb_rf_matrix not registered,"
				"nothing to do\n", __func__);
		goto rf_matrix_remove_done;
	}

	if (rf_matrix_detach(dvb_rf_matrix))
		printk(KERN_ERR "%s: Failed to remove RF_MATRIX config\n",
								      __func__);

	list_del(&dvb_rf_matrix->list);

	kfree(dvb_rf_matrix);

rf_matrix_remove_done:
	return 0;
}
#ifdef CONFIG_OF
static struct of_device_id stm_rf_matrix_match[] = {
	{
		.compatible = "st,dvb_rf_matrix",
	},
		{},
};
#endif
static struct platform_driver stm_dvb_rf_matrix_driver = {
	.driver = {
		   .name = "stm_dvb_rf_matrix",
		   .owner = THIS_MODULE,
#ifdef CONFIG_OF
		   .of_match_table = of_match_ptr(stm_rf_matrix_match),
#endif
		   },
	.probe = stm_dvb_rf_matrix_attach,
	.remove = stm_dvb_rf_matrix_remove
};
#ifdef CONFIG_OF
static void *stm_dvb_ip_get_pdata(struct platform_device *pdev)
{
	int ret = 0;
	struct device_node *np = pdev->dev.of_node;
	struct ip_config_s *ip_config_p;
	const char *addr;
	const char *eth;
	struct device_node *iter;

	ip_config_p =
		devm_kzalloc(&pdev->dev, sizeof(*ip_config_p), GFP_KERNEL);
	if (!ip_config_p) {
		printk(KERN_ERR "%s : ip platform data alloaction fail",
								    __func__);
		ret = -ENOMEM;
		goto ip_pdata_err;
	}
	strlcpy(ip_config_p->ip_name, "IPFE-STM_FE", IP_NAME_MAX_LEN);
	iter = of_parse_phandle(np, "dvb_ip", 0);
	if (iter == NULL) {
		ret = -EINVAL;
		goto ip_pdata_err;
	}
	ret = of_property_read_string(iter, "ip-addr", &addr);
	if (ret)
		goto ip_pdata_err;
	strlcpy(ip_config_p->ip_addr, addr, IP_ADDR_LEN);
	ret = of_property_read_u32(iter, "ip-port", &ip_config_p->ip_port);
	if (ret)
		goto ip_pdata_err;
	ret = of_property_read_u32(iter, "protocol", &ip_config_p->protocol);
	if (ret)
		goto ip_pdata_err;
	ret = of_property_read_string(iter, "ethdev", &eth);
	if (ret)
		goto ip_pdata_err;
	strlcpy(ip_config_p->ethdev, eth, IP_NAME_MAX_LEN);

ip_pdata_err:
	if (ret) {
		printk(KERN_ERR "%s: ip configuration is not properly set\n",
								     __func__);
		return ERR_PTR(ret);
	}
	return ip_config_p;
}
#else
static void *stm_dvb_ip_get_pdata(struct platform_device *stm_fe_demod_data)
{
	return NULL;
}
#endif
static int stm_dvb_ip_attach(struct platform_device *dvb_ip_data)
{
	struct stm_platdev_data *plat_dev_data_p;
	struct stm_dvb_ip_s *ip_p;
	struct ip_config_s *ip_config_p;
	int ret = 0;

	ip_p = kzalloc(sizeof(struct stm_dvb_ip_s), GFP_KERNEL);
	if (ip_p == NULL) {
		printk(KERN_ERR "Unable to allocate memory for ip context\n");
		ret = -ENOMEM;
		goto ip_attach_done;
	}
	if (dvb_ip_data->dev.of_node) {
		ip_config_p = stm_dvb_ip_get_pdata(dvb_ip_data);
		if (IS_ERR_OR_NULL(ip_config_p)) {
			kfree(ip_p);
			return PTR_ERR(ip_config_p);
		}
		ip_p->ip_id = ip_dev_id;
		dvb_ip_data->id = ip_dev_id;
		ip_dev_id++;
		ip_p->ip_config = ip_config_p;
	} else {
		plat_dev_data_p = dvb_ip_data->dev.platform_data;
		ip_p->ip_config = plat_dev_data_p->config_data;
		ip_p->ip_id = dvb_ip_data->id;
	}
	addObjectToList(&ip_p->list, IP);

	sprintf(ip_p->dvb_ip_name, "stm_dvb_ip.%d", ip_p->ip_id);

	printk(KERN_INFO "Configuring ip %s IP name %s\n",
	       ip_p->ip_config->ip_name, dvb_ip_data->name);

	ret = dvb_stm_ipfe_attach(&ip_p->ip, ip_p->ip_config);
	if (ret) {
		printk(KERN_ERR "ip_attach failed!\n");
		goto ip_attach_failed;
	}

	ret = dvb_register_ipfe(&stm_dvb_adapter, ip_p->ip);
	if (ret) {
		printk(KERN_ERR "dvb_register_ipfe failed ret = 0x%d!\n", ret);
		goto ip_attach_failed;
	}

	goto ip_attach_done;

ip_attach_failed:
	dvb_stm_ipfe_detach(ip_p->ip);
	removeObjectFromList(&ip_p->list, IP);
	kfree(ip_p);

ip_attach_done:
	return ret;
}

static int stm_dvb_ip_remove(struct platform_device *dvb_ip_data)
{
	struct stm_dvb_ip_s *dvb_ip;
	GET_IP_OBJECT(dvb_ip, dvb_ip_data->id);
	if (!dvb_ip) {
		printk(KERN_ERR "No IP object, nothing to do\n");
		goto ip_remove_done;
	}

	dvb_unregister_ipfe(dvb_ip->ip);

	dvb_stm_ipfe_detach(dvb_ip->ip);

	list_del(&dvb_ip->list);

	kfree(dvb_ip);

ip_remove_done:
	return 0;
}
#ifdef CONFIG_OF
static struct of_device_id stm_dvb_ip_match[] = {
	{
		.compatible = "st,dvb_ip",
	},
		{},
};
#endif
static struct platform_driver stm_dvb_ip_driver = {
	.driver = {
		   .name = "stm_dvb_ip",
		   .owner = THIS_MODULE,
#ifdef CONFIG_OF
		   .of_match_table = of_match_ptr(stm_dvb_ip_match),
#endif
		   },
	.probe = stm_dvb_ip_attach,
	.remove = stm_dvb_ip_remove
};

#ifdef CONFIG_OF
static void *stm_dvb_ipfec_get_pdata(struct platform_device *pdev)
{
	int ret = 0;
	struct device_node *np = pdev->dev.of_node;
	struct ipfec_config_s *ipfec_config_p;
	const char *addr;
	struct device_node *iter;

	ipfec_config_p =
		devm_kzalloc(&pdev->dev, sizeof(*ipfec_config_p), GFP_KERNEL);
	if (!ipfec_config_p) {
		printk(KERN_ERR "%s : ipfec platform data alloaction fail",
								    __func__);
		ret = -ENOMEM;
		goto ipfec_pdata_err;
	}
	strlcpy(ipfec_config_p->ipfec_name, "IPFEC-STM_FE", IP_NAME_MAX_LEN);
	iter = of_parse_phandle(np, "dvb_ip", 0);
	if (iter == NULL) {
		ret = -EINVAL;
		goto ipfec_pdata_err;
	}
	ret = of_property_read_string(iter, "ipfec-addr", &addr);
	if (ret)
		goto ipfec_pdata_err;
	strlcpy(ipfec_config_p->ipfec_addr, addr, IP_ADDR_LEN);
	ret = of_property_read_u32(iter, "ipfec-port",
						&ipfec_config_p->ipfec_port);
	if (ret)
		goto ipfec_pdata_err;
	ret = of_property_read_u32(iter, "ipfec-scheme",
						&ipfec_config_p->ipfec_scheme);
	if (ret)
		goto ipfec_pdata_err;

ipfec_pdata_err:
	if (ret) {
		printk(KERN_ERR "%s: ipfec configuration is not properly set\n",
								     __func__);
		return ERR_PTR(ret);
	}
	return ipfec_config_p;
}
#else
static void *stm_dvb_ipfec_get_pdata(struct platform_device *stm_fe_demod_data)
{
	return NULL;
}
#endif

static int stm_dvb_ipfec_attach(struct platform_device *dvb_ipfec_data)
{
	struct stm_platdev_data *plat_dev_data_p;
	struct stm_dvb_ipfec_s *ipfec;
	struct ipfec_config_s *ipfec_config_p;
	int32_t ret = 0;

	ipfec = kzalloc(sizeof(struct stm_dvb_ipfec_s), GFP_KERNEL);
	if (ipfec == NULL) {
		printk(KERN_ERR
		       "%s(): Unable to allocate memory for diseqc context\n",
		       __func__);
		ret = -ENOMEM;
		goto ipfec_attach_done;
	}
	if (dvb_ipfec_data->dev.of_node) {
		ipfec_config_p = stm_dvb_ipfec_get_pdata(dvb_ipfec_data);
		if (IS_ERR_OR_NULL(ipfec_config_p)) {
			kfree(ipfec);
			return PTR_ERR(ipfec_config_p);
		}
		ipfec->ipfec_id = ipfec_dev_id;
		dvb_ipfec_data->id = ipfec_dev_id;
		ipfec_dev_id++;
		ipfec->ipfec_config = ipfec_config_p;
	} else {
		plat_dev_data_p = dvb_ipfec_data->dev.platform_data;
		ipfec->ipfec_config = plat_dev_data_p->config_data;
		ipfec->ipfec_id = dvb_ipfec_data->id;
	}
	addObjectToList(&ipfec->list, IPFEC);

	sprintf(ipfec->dvb_ipfec_name, "stm_dvb_ipfec.%d",
		ipfec->ipfec_id);

	GET_IP_OBJECT(ipfec->dvb_ip, ipfec->ipfec_id);
	if (ipfec->dvb_ip == NULL) {
		printk(KERN_ERR
		       "%s() Invalid demod object, cannot attach diseqc\n",
		       __func__);
		ret = -ENXIO;
		goto ipfec_attach_failed;
	}

	ret = ipfec_attach(ipfec);
	if (ret) {
		printk(KERN_ERR "%s(): ipfec attached failed\n", __func__);
		goto ipfec_attach_failed;
	}

	printk(KERN_INFO "%s(): Attached ipfec %s to ip %s ..\n", __func__,
	       ipfec->dvb_ipfec_name, ipfec->dvb_ip->dvb_ip_name);

	goto ipfec_attach_done;

ipfec_attach_failed:
	removeObjectFromList(&ipfec->list, IPFEC);
	kfree(ipfec);

ipfec_attach_done:
	return ret;
}

static int stm_dvb_ipfec_remove(struct platform_device *dvb_ipfec_data)
{
	struct stm_dvb_ipfec_s *dvb_ipfec;

	GET_IPFEC_OBJECT(dvb_ipfec, dvb_ipfec_data->id);
	if (dvb_ipfec == NULL) {
		printk(KERN_ERR "No dvb_ipfec object, nothing to be done\n");
		goto ipfec_remove_done;
	}

	dvb_stm_fe_ipfec_detach(dvb_ipfec->dvb_ip->ip);

	list_del(&dvb_ipfec->list);

	kfree(dvb_ipfec);

ipfec_remove_done:
	return 0;
}
#ifdef CONFIG_OF
static struct of_device_id stm_dvb_ipfec_match[] = {
	{
		.compatible = "st,dvb_ipfec",
	},
		{},
};
#endif
static struct platform_driver stm_dvb_ipfec_driver = {
	.driver = {
		   .name = "stm_dvb_ipfec",
		   .owner = THIS_MODULE,
#ifdef CONFIG_OF
		   .of_match_table = of_match_ptr(stm_dvb_ipfec_match),
#endif
		   },
	.probe = stm_dvb_ipfec_attach,
	.remove = stm_dvb_ipfec_remove
};

int32_t stm_dvb_init(void)
{
	struct stm_dvb_demod_s *dvb_demod = NULL;
	struct stm_dvb_lnb_s *dvb_lnb = NULL;
	struct stm_dvb_diseqc_s *dvb_diseqc = NULL;
	struct stm_dvb_rf_matrix_s *dvb_rf_matrix = NULL;
	stm_fe_rf_matrix_h rf_matrix_object = NULL;
	struct stm_dvb_ip_s *dvb_ip = NULL;
	struct stm_dvb_ipfec_s *dvb_ipfec = NULL;
	struct stm_dvb_demux_s *dvb_demux = NULL;
#ifdef CONFIG_STLINUXTV_CRYPTO
	struct stm_dvb_ca_s *dvb_ca = NULL;
#endif
	stm_fe_object_info_t *fe_objs;
	stm_te_caps_t te_caps;
	uint32_t obj_cnt = 0;
	uint32_t cnt = 0;
	uint32_t inst = 0;
	uint32_t lnb_id = 0;
	uint32_t diseqc_id = 0;
	uint32_t rf_matrix_id = 0;
	uint32_t ip_id = 0;
	uint32_t ipfec_id = 0;
	int32_t  ret;
	short int ids[DVB_MAX_ADAPTERS] = { 0 };

	printk(KERN_INFO "Loading module ctrl_via_stm_fe = %d\n",
	       ctrl_via_stm_fe);

	if ((ret = stm_fe_discover(&fe_objs, &obj_cnt))) {
		printk(KERN_ERR
		       "Error getting STM_FE objects from stm_fe_discover : %d\n",
		       ret);
		return 0;
	}

	if (obj_cnt) {
		printk(KERN_INFO "%d stm_fe objects discovered\n", obj_cnt);
	} else {
		printk(KERN_INFO
		       "NO stm_fe objects discovered is stm_fe_platform loaded\n");
	}

	initDataSets();

	if (0 > (ret = dvb_register_adapter(&stm_dvb_adapter,
					    "ST DVB frontend driver",
					    THIS_MODULE, NULL, ids))) {
		printk(KERN_ERR "Error registering adapter %d : %d\n", *ids,
		       ret);
		return 0;
	}

	platform_driver_register(&stm_dvb_demod_driver);
	platform_driver_register(&stm_dvb_lnb_driver);
	platform_driver_register(&stm_dvb_diseqc_driver);
	platform_driver_register(&stm_dvb_rf_matrix_driver);
	platform_driver_register(&stm_dvb_ip_driver);
	platform_driver_register(&stm_dvb_ipfec_driver);

	for (cnt = 0; cnt < obj_cnt; cnt++) {
		if (fe_objs[cnt].type == STM_FE_DEMOD) {
			dvb_demod = stm_dvb_object_get_by_index(DEMOD, inst++);
			if (!dvb_demod)
				continue;
			printk(KERN_INFO "dvb_demod = %p, stm_fe_obj = %p\n",
			       dvb_demod, fe_objs[cnt].stm_fe_obj);

			ret = stm_fe_demod_new(fe_objs[cnt].stm_fe_obj,
					       &dvb_demod->demod_object);
			if (ret) {
				printk(KERN_ERR
				       "Failed stm_fe_demod_new() for %s ret = 0x%d\n",
				       fe_objs[cnt].stm_fe_obj, ret);
			} else {
				printk(KERN_INFO
				       "Created %-20s for parent %-20s\n",
				       fe_objs[cnt].stm_fe_obj,
				       fe_objs[cnt].member_of[0]);
				if (dvb_demod->ctrl_via_stm_fe)
					dvb_stm_fe_demod_link(dvb_demod->demod,
							      dvb_demod->demod_object,
							      &fe_objs[cnt]);
			}
		}

		if (fe_objs[cnt].type == STM_FE_LNB) {
			dvb_lnb = stm_dvb_object_get_by_index(LNB, lnb_id++);
			if (!dvb_lnb)
				continue;
			if (dvb_lnb->dvb_demod->demod_object) {
				ret = stm_fe_lnb_new(fe_objs[cnt].stm_fe_obj,
						     dvb_lnb->
						     dvb_demod->demod_object,
						     &dvb_lnb->lnb_object);
				if (!ret)
					printk(KERN_INFO
					       "Created %-20s for parent %-20s\n",
					       fe_objs[cnt].stm_fe_obj,
					       fe_objs[cnt].member_of[0]);
				if (dvb_lnb->ctrl_via_stm_fe)
					dvb_stm_fe_lnb_link(dvb_lnb->
							    dvb_demod->demod,
							    dvb_lnb->lnb_object);
			}
		}

		if (fe_objs[cnt].type == STM_FE_DISEQC) {
			dvb_diseqc = stm_dvb_object_get_by_index(DISEQC, diseqc_id++);
			if (!dvb_diseqc)
				continue;
			if (dvb_diseqc->dvb_demod->demod_object) {
				ret = stm_fe_diseqc_new(fe_objs[cnt].stm_fe_obj,
							dvb_diseqc->dvb_demod->
							demod_object,
							&dvb_diseqc->diseqc_object);
				if (!ret)
					printk(KERN_INFO
					       "Created %-20s for parent %-20s\n",
					       fe_objs[cnt].stm_fe_obj,
					       fe_objs[cnt].member_of[0]);
				if (dvb_diseqc->ctrl_via_stm_fe)
					dvb_stm_fe_diseqc_link
					    (dvb_diseqc->dvb_demod->demod,
					     dvb_diseqc->diseqc_object);
			}
		}

		if (fe_objs[cnt].type == STM_FE_RF_MATRIX) {
			dvb_rf_matrix = stm_dvb_object_get_by_index(RF_MATRIX,
								rf_matrix_id++);
			if (!dvb_rf_matrix)
				continue;
			rf_matrix_object = dvb_rf_matrix->rf_matrix_object;
			if (dvb_rf_matrix->dvb_demod->demod_object) {
				ret =
				   stm_fe_rf_matrix_new(fe_objs[cnt].stm_fe_obj,
						   dvb_rf_matrix->dvb_demod->
						   demod_object,
						   &rf_matrix_object);
				if (!ret)
					printk(KERN_INFO
					       "Created %-20s for parent %-20s\n",
					       fe_objs[cnt].stm_fe_obj,
					       fe_objs[cnt].member_of[0]);
				if (dvb_rf_matrix->ctrl_via_stm_fe)
					dvb_stm_fe_rf_matrix_link
					    (dvb_rf_matrix->dvb_demod->demod,
					     rf_matrix_object,
					     &fe_objs[cnt]);
			}
		}

		if (fe_objs[cnt].type == STM_FE_IP) {
			dvb_ip = stm_dvb_object_get_by_index(IP, ip_id++);
			if (!dvb_ip)
				continue;
			ret = stm_fe_ip_new(fe_objs[cnt].stm_fe_obj,
					    NULL, &dvb_ip->ip_object);
			if (!ret) {
				printk(KERN_INFO
				       "Created %-20s for parent %-20s\n",
				       fe_objs[cnt].stm_fe_obj,
				       fe_objs[cnt].member_of[0]);
			}
			dvb_stm_fe_ip_link(dvb_ip->ip, dvb_ip->ip_object,
					   &fe_objs[cnt]);
		}

		if (fe_objs[cnt].type == STM_FE_IPFEC) {
			dvb_ipfec = stm_dvb_object_get_by_index(IPFEC, ipfec_id++);
			if (!dvb_ipfec)
				continue;
			ret = stm_fe_ip_fec_new(fe_objs[cnt].stm_fe_obj,
					    dvb_ipfec->dvb_ip->ip_object,
					    NULL, &dvb_ipfec->ipfec_object);
			if (!ret) {
				printk(KERN_INFO
				       "Created %-20s for parent %-20s\n",
				       fe_objs[cnt].stm_fe_obj,
				       fe_objs[cnt].member_of[0]);
			}
			dvb_stm_fe_ipfec_link(dvb_ipfec->dvb_ip->ip,
						dvb_ipfec->ipfec_object);
		}
	}

	if ((ret = stm_te_get_capabilities(&te_caps))) {
		printk(KERN_ERR "Failed to get capabilities of Demux\n");
		return ret;
	}

	/* Filter numbers are hard coded at the moment until the te_caps is filled correctly */

	printk(KERN_INFO
	       "Found %u demuxes, %d pid filters, %d output filters\n",
	       te_caps.max_demuxes, te_caps.max_pid_filters,
	       te_caps.max_output_filters);

#ifdef CONFIG_STLINUXTV_DECODE_DISPLAY
	/* Initialize the DvbContext structure */
	memset(&DvbContext, 0, sizeof(struct DvbContext_s));
	mutex_init(&DvbContext.Lock);

	/* Store the max demux available, the same number of
	 * audio/video devices to be created and released */
	DvbContext.demux_dev_nb = te_caps.max_demuxes;
	DvbContext.audio_dev_off = 0;
	DvbContext.audio_dev_nb = audio_device_nb;
	DvbContext.video_dev_off = audio_device_nb;
	DvbContext.video_dev_nb = video_device_nb;

	DvbContext.AudioDeviceContext =
	    kzalloc(sizeof(struct AudioDeviceContext_s) * audio_device_nb,
		    GFP_KERNEL);
	if (DvbContext.AudioDeviceContext == NULL) {
		printk(KERN_ERR
		       "%s: failed to init memory for Audio device context\n",
		       __func__);
		return -ENOMEM;
	}
	DvbContext.VideoDeviceContext =
	    kzalloc(sizeof(struct VideoDeviceContext_s) * video_device_nb,
		    GFP_KERNEL);
	if (DvbContext.VideoDeviceContext == NULL) {
		printk(KERN_ERR
		       "%s: failed to init memory for Video device context\n",
		       __func__);
		return -ENOMEM;
	}
	DvbContext.PlaybackDeviceContext =
	    kzalloc(sizeof(struct PlaybackDeviceContext_s) *\
			(audio_device_nb + video_device_nb) , GFP_KERNEL);
	if (DvbContext.PlaybackDeviceContext == NULL) {
		printk(KERN_ERR
		       "%s: failed to init memory for Playback device context\n",
		       __func__);
		return -ENOMEM;
	}
	for (obj_cnt = 0; obj_cnt < (audio_device_nb + video_device_nb); obj_cnt++ ){
		DvbContext.PlaybackDeviceContext[obj_cnt].Id = obj_cnt;
		mutex_init(&DvbContext.PlaybackDeviceContext[obj_cnt].DemuxWriteLock);
		mutex_init(&DvbContext.PlaybackDeviceContext[obj_cnt].Playback_alloc_mutex);
	}

	DvbContext.DvbAdapter = &stm_dvb_adapter;
#endif

	for (obj_cnt = 0; obj_cnt < te_caps.max_demuxes; obj_cnt++) {
		stm_dvb_demux_attach(&stm_dvb_adapter,
				     &dvb_demux,
				     te_caps.max_output_filters,
				     te_caps.max_output_filters);
		if (dvb_demux) {
			addObjectToList(&dvb_demux->list, DEMUX);
		} else {
			printk(KERN_ERR "Failed to add demux.%d\n", obj_cnt);
			goto demux_err;
		}
#ifdef CONFIG_STLINUXTV_CRYPTO
		stm_dvb_ca_attach(&stm_dvb_adapter, dvb_demux, &dvb_ca);
		if (dvb_ca) {
			addObjectToList(&dvb_ca->list, CA);
		} else {
			printk(KERN_ERR "Failed to add CA.%d\n", obj_cnt);
			goto demux_err;
		}
#endif

#ifdef CONFIG_STLINUXTV_DECODE_DISPLAY
		dvb_demux->PlaybackContext = NULL;
		dvb_demux->DvbContext = &DvbContext;
#endif
	}

#ifdef CONFIG_STLINUXTV_DECODE_DISPLAY
	for (obj_cnt = 0; obj_cnt < audio_device_nb; obj_cnt++) {
		struct AudioDeviceContext_s *audio;
		char InitialName[32];
		/* Create audio/video devices and initialize them */
		audio = &DvbContext.AudioDeviceContext[obj_cnt];
		audio->Id = obj_cnt;
		audio->StreamType = STREAM_TYPE_TRANSPORT;
		audio->DvbContext = &DvbContext;
		audio->PlaySpeed = DVB_SPEED_NORMAL_PLAY;

		mutex_init(&audio->auddev_mutex);
		mutex_init(&audio->audops_mutex);
		mutex_init(&audio->audwrite_mutex);
		mutex_init(&audio->AudioEvents.Lock);

		dvb_register_device(&stm_dvb_adapter,
				    &audio->AudioDevice,
				    AudioInit(audio),
				    audio, DVB_DEVICE_AUDIO);
		AudioInitSubdev(audio);
		stlinuxtv_stat_init_device(&audio->AudioClassDevice);
		snprintf(InitialName, sizeof(InitialName), "dvb%d.audio%d",
			 DvbContext.DvbAdapter->num, obj_cnt);
		audio->AudioClassDevice.init_name = InitialName;
		ret = device_register(&audio->AudioClassDevice);
		if (ret != 0) {
			printk(KERN_ERR "Unable to register %s stat device\n",
			       InitialName);
		}
#if defined (SDK2_ENABLE_STLINUXTV_STATISTICS)
		ret = AudioInitSysfsAttributes(audio);
		if (ret != 0) {
			printk(KERN_ERR "Uanbel to create attributes for %s\n",
			       InitialName);
		}
#endif
	}

	for (obj_cnt = 0; obj_cnt < video_device_nb; obj_cnt++) {
		struct VideoDeviceContext_s *video;
		char InitialName[32];
		video = &DvbContext.VideoDeviceContext[obj_cnt];
		video->Id = obj_cnt;
		video->StreamType = STREAM_TYPE_TRANSPORT;
		video->DvbContext = &DvbContext;
		video->PlaySpeed = DVB_SPEED_NORMAL_PLAY;

		mutex_init(&video->viddev_mutex);
		mutex_init(&video->vidops_mutex);
		mutex_init(&video->vidwrite_mutex);
		mutex_init(&video->VideoEvents.Lock);

		dvb_register_device(&stm_dvb_adapter,
				    &video->VideoDevice,
				    VideoInit(video),
				    video, DVB_DEVICE_VIDEO);
		VideoInitSubdev(video);

		stlinuxtv_stat_init_device(&video->VideoClassDevice);
		snprintf(InitialName, sizeof(InitialName), "dvb%d.video%d",
			 DvbContext.DvbAdapter->num, obj_cnt);
		video->VideoClassDevice.init_name = InitialName;
		ret = device_register(&video->VideoClassDevice);
		if (ret != 0) {
			printk(KERN_ERR "Unable to register %s stat device\n",
			       InitialName);
		}

#if defined(SDK2_ENABLE_STLINUXTV_STATISTICS)
		ret = VideoInitSysfsAttributes(video);
		if (ret != 0) {
			printk(KERN_ERR "Uanbel to create attributes for %s\n",
			       InitialName);
		}
#endif
	}
#endif
	return 0;

demux_err:
	printk(KERN_ERR
	       "Conguration called for %d demux devices, only %d could be created\n",
	       te_caps.max_demuxes, obj_cnt);
	return 0;
}

void stm_dvb_exit(void)
{
	struct stm_dvb_demod_s *dvb_demod;
	struct stm_dvb_lnb_s *dvb_lnb;
	struct stm_dvb_diseqc_s *dvb_diseqc;
	struct stm_dvb_rf_matrix_s *dvb_rf_matrix;
	stm_fe_rf_matrix_h rf_matrix_object;
	struct stm_dvb_demux_s *dvb_demux;
#ifdef CONFIG_STLINUXTV_CRYPTO
	struct stm_dvb_ca_s *dvb_ca;
#endif
	struct stm_dvb_ip_s *dvb_ip;
	struct stm_dvb_ipfec_s *dvb_ipfec;
#ifdef CONFIG_STLINUXTV_DECODE_DISPLAY
	struct AudioDeviceContext_s *AudioDeviceContext;
	struct VideoDeviceContext_s *VideoDeviceContext;
	int i;
#endif
	int ret, inst = 0;

#ifdef CONFIG_STLINUXTV_DECODE_DISPLAY
	/* Remove the Audio subdev context */
	for (i = 0; i < DvbContext.audio_dev_nb; i++) {
		AudioDeviceContext = &DvbContext.AudioDeviceContext[i];

#if defined(SDK2_ENABLE_STLINUXTV_STATISTICS)
		/* Remove Audio subdev registrations */
		AudioRemoveSysfsAttributes(AudioDeviceContext);
#endif

		device_unregister(&AudioDeviceContext->AudioClassDevice);

		AudioReleaseSubdev(AudioDeviceContext);

		dvb_unregister_device(AudioDeviceContext->AudioDevice);
	}

	/* Remove the Video subdev context */
	for (i = 0; i < DvbContext.video_dev_nb; i++) {
		VideoDeviceContext = &DvbContext.VideoDeviceContext[i];

#if defined(SDK2_ENABLE_STLINUXTV_STATISTICS)
		/* Remove Video subdev registrations */
		VideoRemoveSysfsAttributes(VideoDeviceContext);
#endif

		device_unregister(&VideoDeviceContext->VideoClassDevice);

		VideoReleaseSubdev(VideoDeviceContext);

		dvb_unregister_device(VideoDeviceContext->VideoDevice);
	}

	kfree(DvbContext.AudioDeviceContext);

	kfree(DvbContext.VideoDeviceContext);

	kfree(DvbContext.PlaybackDeviceContext);
#endif

#ifdef CONFIG_STLINUXTV_CRYPTO
	do {
		GET_CA_OBJECT(dvb_ca, inst++);

		if (dvb_ca) {
			list_del(&dvb_ca->list);
			ret = stm_dvb_ca_delete(dvb_ca);
			if (ret)
				printk(KERN_ERR
				       "Failed stm_dvb_ca_delete %d\n", ret);
		} else
			break;
	} while (1);
#endif

	inst = 0;

	do {
		GET_DEMUX_OBJECT(dvb_demux, inst++);

		if (dvb_demux) {
			list_del(&dvb_demux->list);
			ret = stm_dvb_demux_delete(dvb_demux);
			if (ret)
				printk(KERN_ERR
				       "Failed stm_te_demux_delete %d\n", ret);
		} else
			break;
	} while (1);

	inst = 0;

	do {
		dvb_demod = stm_dvb_object_get_by_index(DEMOD, inst);
		dvb_lnb = stm_dvb_object_get_by_index(LNB, inst);
		dvb_diseqc = stm_dvb_object_get_by_index(DISEQC, inst);
		dvb_rf_matrix = stm_dvb_object_get_by_index(RF_MATRIX, inst);

		if (!dvb_demod) {
			/*
			 * No more DEMOD objects implies, no more LNB or DISEQC
			 */
			break;
		}

		if (!dvb_demod->demod_object) {
			inst++;
			continue;
		}

		if (dvb_rf_matrix) {
			rf_matrix_object = dvb_rf_matrix->rf_matrix_object;
			ret = stm_fe_rf_matrix_delete(rf_matrix_object);
			if (ret)
				printk(KERN_ERR
				       "Failed stm_fe_rf_matrix_delete %d\n",
				       ret);
		}
		if (dvb_diseqc) {
			ret = stm_fe_diseqc_delete(dvb_diseqc->diseqc_object);
			if (ret)
				printk(KERN_ERR
				       "Failed stm_fe_diseqc_delete %d\n", ret);
		}
		if (dvb_lnb) {
			ret = stm_fe_lnb_delete(dvb_lnb->lnb_object);
			if (ret)
				printk(KERN_ERR "Failed stm_fe_lnb_delete %d\n",
				       ret);
		}
		ret = stm_fe_demod_delete(dvb_demod->demod_object);
		if (ret)
			printk(KERN_ERR "Failed stm_fe_demod_delete %d\n", ret);
		inst++;
	} while (1);

	inst = 0;
	do {
		dvb_ip = stm_dvb_object_get_by_index(IP, inst);
		dvb_ipfec = stm_dvb_object_get_by_index(IPFEC, inst);

		if (!dvb_ip) {
			/*
			 * No more IP/IPFEC objects
			 */
			break;
		}

		if (!dvb_ip->ip_object) {
			inst++;
			continue;
		}

		if (dvb_ip) {
			ret = stm_fe_ip_delete(dvb_ip->ip_object);
			if (ret)
				printk(KERN_ERR
				       "Failed stm_fe_ip_delete %d\n", ret);
		}

		if (dvb_ipfec) {
			ret = stm_fe_ip_fec_delete(dvb_ipfec->ipfec_object);
			if (ret)
				printk(KERN_ERR
				       "Failed stm_fe_ip_fec_delete %d\n", ret);
		}

		inst++;
	} while (1);

	platform_driver_unregister(&stm_dvb_ipfec_driver);
	platform_driver_unregister(&stm_dvb_ip_driver);
	platform_driver_unregister(&stm_dvb_rf_matrix_driver);
	platform_driver_unregister(&stm_dvb_diseqc_driver);
	platform_driver_unregister(&stm_dvb_lnb_driver);
	platform_driver_unregister(&stm_dvb_demod_driver);

	if (0 <= stm_dvb_adapter.num)
		dvb_unregister_adapter(&stm_dvb_adapter);

	ctrl_via_stm_fe = FE_ADAP_NO_OPTIONS_DEFAULT;
}
