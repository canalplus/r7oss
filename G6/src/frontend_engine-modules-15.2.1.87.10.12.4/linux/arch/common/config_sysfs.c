/*
 * arch/common/config_sysfs.c
 *
 * Copyright (C) 2012 STMicroelectronics Limited
 * Author:Rahul V
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 */
#include <linux/gpio.h>
#include <linux/string.h>
#include <linux/platform_device.h>
#include <linux/stm/demod.h>
#include <linux/stm/lnb.h>
#include <linux/stm/diseqc.h>
#include <linux/stm/ip.h>
#include <linux/stm/ipfec.h>
#include <linux/stm/plat_dev.h>
#include <stm_fe_conf.h>
#include "stm_fe_setup.h"

#define GET_DEMOD_CONF(dev) \
	((struct demod_config_s *)(((struct stm_platdev_data *) \
					dev->platform_data)->config_data))

#define GET_LNB_CONF(dev) \
	((struct lnb_config_s *)(((struct stm_platdev_data *) \
					dev->platform_data)->config_data))

#define GET_DISEQC_CONF(dev) \
	((struct diseqc_config_s *)(((struct stm_platdev_data *) \
					dev->platform_data)->config_data))

const char config_group_name[] = "conf";

#define STRING_ATTR(var, str)\
	static ssize_t var##_show(struct device *dev, \
			struct device_attribute *attr, char *buf)\
	{\
		return sprintf(buf, "%s\n", str);\
	} \
	static ssize_t var##_store(struct device *dev, \
		struct device_attribute *attr, const char *buf, size_t n)\
	{\
		char *cp;\
		int len = n;\
	\
		cp = memchr(buf, '\n', n);\
		if (cp)\
			len = cp - buf;\
	\
		strncpy(str, buf, len);\
	\
		return n;\
	} \
	static DEVICE_ATTR(var, 0644, var##_show, var##_store);

#define INT_ATTR(var, num)\
static ssize_t var##_show(struct device *dev, struct device_attribute *attr,\
			  char *buf)\
{\
	return sprintf(buf, "%d\n", num);\
} \
static ssize_t var##_store(struct device *dev, struct device_attribute *attr,\
			   const char *buf, size_t n)\
{\
	int ret;\
\
	ret = kstrtol(buf, 10, (unsigned long *)&num);\
\
	return n;\
} \
static DEVICE_ATTR(var, 0644, var##_show, var##_store);

STRING_ATTR(demod_name, (GET_DEMOD_CONF(dev))->demod_name);
STRING_ATTR(tuner_name, (GET_DEMOD_CONF(dev))->tuner_name);
INT_ATTR(demod_io_bus, (GET_DEMOD_CONF(dev))->demod_io.bus);
INT_ATTR(demod_io_address, (GET_DEMOD_CONF(dev))->demod_io.address);
INT_ATTR(tuner_io_bus, (GET_DEMOD_CONF(dev))->tuner_io.bus);
INT_ATTR(tuner_io_address, (GET_DEMOD_CONF(dev))->tuner_io.address);
INT_ATTR(clk_config_tuner_clk, (GET_DEMOD_CONF(dev))->clk_config.tuner_clk);
INT_ATTR(clk_config_demod_clk, (GET_DEMOD_CONF(dev))->clk_config.demod_clk);
INT_ATTR(clk_config_tuner_clkout_divider,
			(GET_DEMOD_CONF(dev))->clk_config.tuner_clkout_divider);
INT_ATTR(ts_out, (GET_DEMOD_CONF(dev))->ts_out);
INT_ATTR(ts_clock, (GET_DEMOD_CONF(dev))->ts_clock);
INT_ATTR(demux_tsin_id, (GET_DEMOD_CONF(dev))->demux_tsin_id);
INT_ATTR(custom_flags, (GET_DEMOD_CONF(dev))->custom_flags);
INT_ATTR(tuner_if, (GET_DEMOD_CONF(dev))->tuner_if);
INT_ATTR(roll_off, (GET_DEMOD_CONF(dev))->roll_off);
INT_ATTR(reset_pio, (GET_DEMOD_CONF(dev))->reset_pio);

static struct attribute *demod_conf_attrs[] = {
	&dev_attr_demod_name.attr,
	&dev_attr_tuner_name.attr,
	&dev_attr_demod_io_bus.attr,
	&dev_attr_demod_io_address.attr,
	&dev_attr_tuner_io_bus.attr,
	&dev_attr_tuner_io_address.attr,
	&dev_attr_clk_config_tuner_clk.attr,
	&dev_attr_clk_config_demod_clk.attr,
	&dev_attr_clk_config_tuner_clkout_divider.attr,
	&dev_attr_ts_out.attr,
	&dev_attr_ts_clock.attr,
	&dev_attr_demux_tsin_id.attr,
	&dev_attr_custom_flags.attr,
	&dev_attr_tuner_if.attr,
	&dev_attr_roll_off.attr,
	&dev_attr_reset_pio.attr,
	NULL,
};

static struct attribute_group demod_conf_attr_group = {
	.name = config_group_name,
	.attrs = demod_conf_attrs,
};

STRING_ATTR(lnb_name, (GET_LNB_CONF(dev))->lnb_name);
INT_ATTR(lnb_io_bus, (GET_LNB_CONF(dev))->lnb_io.bus);
INT_ATTR(lnb_io_address, (GET_LNB_CONF(dev))->lnb_io.address);
INT_ATTR(cust_flags, (GET_LNB_CONF(dev))->cust_flags);

static struct attribute *lnb_conf_attrs[] = {
	&dev_attr_lnb_name.attr,
	&dev_attr_lnb_io_bus.attr,
	&dev_attr_lnb_io_address.attr,
	&dev_attr_cust_flags.attr,
	NULL,
};

static struct attribute_group lnb_conf_attr_group = {
	.name = config_group_name,
	.attrs = lnb_conf_attrs,
};

STRING_ATTR(diseqc_name, (GET_DISEQC_CONF(dev))->diseqc_name);
INT_ATTR(ver, (GET_DISEQC_CONF(dev))->ver);

static struct attribute *diseqc_conf_attrs[] = {
	&dev_attr_diseqc_name.attr,
	&dev_attr_ver.attr,
	NULL,
};

static struct attribute_group diseqc_conf_attr_group = {
	.name = config_group_name,
	.attrs = diseqc_conf_attrs,
};

int stmfe_conf_sysfs_create(struct platform_device **platdev, int num)
{
	int ret, id, err;
	struct attribute_group *attr_group;

	for (id = 0, ret = 0, err = 0; id < num && !err; id++) {
		attr_group = NULL;
		if (!strcmp(platdev[id]->name, STM_FE_DEMOD_NAME))
			attr_group = &demod_conf_attr_group;
		if (!strcmp(platdev[id]->name, STM_FE_LNB_NAME))
			attr_group = &lnb_conf_attr_group;
		if (!strcmp(platdev[id]->name, STM_FE_DISEQC_NAME))
			attr_group = &diseqc_conf_attr_group;

		if (attr_group)
			err = sysfs_create_group(&platdev[id]->dev.kobj,
						 attr_group);
		if (err) {
			pr_err("%s: sysfs_create_group fail for %s = %d\n",
					     __func__, platdev[id]->name, err);
			ret = err;
		}
	}

	return ret;
}

void stmfe_conf_sysfs_remove(struct platform_device **platdev, int num)
{
	int id;
	struct attribute_group *attr_group;

	for (id = 0; id < num; id++) {
		attr_group = NULL;
		if (!strcmp(platdev[id]->name, STM_FE_DEMOD_NAME))
			attr_group = &demod_conf_attr_group;
		if (!strcmp(platdev[id]->name, STM_FE_LNB_NAME))
			attr_group = &lnb_conf_attr_group;
		if (!strcmp(platdev[id]->name, STM_FE_DISEQC_NAME))
			attr_group = &diseqc_conf_attr_group;

		if (attr_group)
			sysfs_remove_group(&platdev[id]->dev.kobj,
						 attr_group);
	}

	return;
}
