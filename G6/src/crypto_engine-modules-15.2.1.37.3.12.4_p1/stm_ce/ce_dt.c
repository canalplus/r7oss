/************************************************************************
Copyright (C) 2014 STMicroelectronics. All Rights Reserved.

This file is part of the Crypto_engine Library.

Crypto_engine is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License version 2 as published by the
Free Software Foundation.

Crypto_engine is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with Crypto_engine; see the file COPYING.  If not, write to the Free Software
Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

The Crypto_engine Library may alternatively be licensed under a proprietary
license from ST.

Source file name : ce_dt.c

CryptoEngine HAL Device Tree handling
************************************************************************/
#include "ce_hal.h"
#include "ce_dt.h"

#include <linux/of.h>
#include <linux/version.h>

#if (LINUX_VERSION_CODE < KERNEL_VERSION(3, 5, 0))
static inline int of_get_child_count(const struct device_node *np)
{
	struct device_node *child;
	int num = 0;

	for_each_child_of_node(np, child)
		num++;

	return num;
}
#endif

#define MATCH_SC { .compatible = "st,tango-sc" }

struct of_device_id ce_dt_sc_match[] = {
	MATCH_SC,
	{},
};

int ce_hal_dt_check(void)
{
	struct device_node *dn;

	dn = of_find_matching_node(NULL, ce_dt_sc_match);
	if (!dn)
		return -ENODEV;

	return 0;
}

/* Helper function to initialise clk pointers in ce_hal_clk array */
static inline void get_clks(struct device *dev, const char *devname,
				struct ce_hal_clk *clks, uint32_t nb_clk)
{
	int i;

	for (i = 0; i < nb_clk; i++) {
#ifndef CONFIG_ARCH_STI
		clks[i].clk = clk_get_sys(devname, clks[i].name);
#endif
		if (IS_ERR(clks[i].clk)) {
			dev_warn(dev, "%s: Clock %s not found", devname,
					clks[i].name);
			clks[i].clk = NULL;
		}
		if (clks[i].parent_name) {
			clks[i].parent_clk = clk_get_sys(devname,
					clks[i].parent_name);
			if (IS_ERR(clks[i].parent_clk)) {
				dev_warn(dev, "%s: parent %s not found\n",
					devname, clks[i].parent_name);
				clks[i].parent_clk = NULL;
			}
		}
	}
}

static inline void put_clks(struct ce_hal_clk *clks, uint32_t nb_clk)
{
	int i;

	for (i = 0; i < nb_clk; i++) {
		if (clks[i].clk) {
			clk_put(clks[i].clk);
			clks[i].clk = NULL;
			if (clks[i].parent_clk) {
				clk_put(clks[i].parent_clk);
				clks[i].parent_clk = NULL;
			}
		}
	}
}

/* Helper function to get the default_session_name */
inline const char *get_default_session_name(struct platform_device *pdev)
{
	struct ce_hal_dt_config *conf =
		(struct ce_hal_dt_config *)pdev->dev.platform_data;

	return conf->default_session_name;
}

static int copy_dt_clocks(struct device_node *dn, struct platform_device *pdev,
		struct ce_hal_clk **ret_clk, uint32_t *nb_clk)
{
	struct device_node *clock_node;
	struct ce_hal_clk *clk = NULL;
	int clk_index = 0;

	clk = devm_kzalloc(&pdev->dev, sizeof(*clk) * of_get_child_count(dn),
			GFP_KERNEL);
	if (!clk)
		return -ENOMEM;

	for_each_child_of_node(dn, clock_node) {
		int err;

		err = of_property_read_string(clock_node, "clk-name",
				&clk[clk_index].name);
		if (err)
			/* Skip nodes that don't have clk-name */
			continue;
		of_property_read_u32(clock_node, "clk-rate",
				&clk[clk_index].freq);
		of_property_read_string(clock_node, "clk-parent",
				&clk[clk_index].parent_name);

#ifdef CONFIG_ARCH_STI
		clk[clk_index].clk = devm_clk_get(&pdev->dev,
						  clk[clk_index].name);
#endif
		clk_index++;
	}

	*ret_clk = clk;
	*nb_clk = clk_index;

	return 0;
}

int ce_hal_dt_get_pdata(struct platform_device *pdev)
{
	struct device_node *dn = pdev->dev.of_node;
	struct device_node *cfg_clk, *cfg;
	struct ce_hal_dt_config *data = NULL;
	int ret;

	/* Allocate the platform data */
	data = devm_kzalloc(&pdev->dev, sizeof(*data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	/* Alloc and copy clks */
	cfg_clk = of_get_child_by_name(dn, "clocks");
	if (cfg_clk) {
		ret = copy_dt_clocks(cfg_clk, pdev, &data->clk, &data->nb_clk);
		if (ret)
			goto error;
		get_clks(&pdev->dev, pdev->name, data->clk, data->nb_clk);
	}

	/* Copy properties */
	cfg = of_parse_phandle(dn, "cesc_config", 0);
	if (!cfg) {
		ret = -EINVAL;
		goto error;
	}

	/* Get the default session name */
	ret = of_property_read_string(cfg, "default-session-name",
				&data->default_session_name);
	if (ret)
		goto error;

	pdev->dev.platform_data = data;

	return 0;

error:
	devm_kfree(&pdev->dev, data);
	return ret;
}

int ce_hal_dt_put_pdata(struct platform_device *pdev)
{
	struct ce_hal_dt_config *data = pdev->dev.platform_data;

	/* Free allocate pdata and clks */
	if (data) {
		devm_kfree(&pdev->dev, data->clk);
		devm_kfree(&pdev->dev, data);
	}
	return 0;
}

int ce_hal_clk_enable(struct platform_device *pdev)
{
	struct ce_hal_dt_config *dt_conf = pdev->dev.platform_data;
	int i;
	int err;

	for (i = 0; i < dt_conf->nb_clk; i++) {
		if (dt_conf->clk[i].clk) {
			dev_info(&pdev->dev, "Enabling clock %s\n",
					dt_conf->clk[i].name);
			err = clk_prepare_enable(dt_conf->clk[i].clk);
			if (err)
				dev_err(&pdev->dev,
					"Unable to enable clock %s (%d)\n",
					dt_conf->clk[i].name, err);

			if (dt_conf->clk[i].parent_clk) {
				clk_set_parent(dt_conf->clk[i].clk,
					dt_conf->clk[i].parent_clk);
			}

			if (dt_conf->clk[i].freq) {
				dev_info(&pdev->dev,
					 "Setting clock %s to %uHz\n",
					 dt_conf->clk[i].name,
					 dt_conf->clk[i].freq);
				err = clk_set_rate(dt_conf->clk[i].clk,
							dt_conf->clk[i].freq);
				if (err)
					dev_err(&pdev->dev,
						"Unable to set rate (%d)\n",
							err);
			}
			dev_info(&pdev->dev, "Clock %s @ %luHz\n",
				 dt_conf->clk[i].name,
				 clk_get_rate(dt_conf->clk[i].clk));
		} else {
			dev_warn(&pdev->dev, "Clock %s unavailable\n",
					dt_conf->clk[i].name);
		}
	}

	return 0;
}

int ce_hal_clk_disable(struct platform_device *pdev)
{
	struct ce_hal_dt_config *dt_conf = pdev->dev.platform_data;
	int i;

	for (i = 0; i < dt_conf->nb_clk; i++) {
		if (dt_conf->clk[i].clk) {
			dev_info(&pdev->dev, "Disabling clock %s\n",
					dt_conf->clk[i].name);
			clk_disable_unprepare(dt_conf->clk[i].clk);
		}
	}
	return 0;
}
