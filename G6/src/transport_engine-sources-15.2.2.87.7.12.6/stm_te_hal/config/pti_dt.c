/************************************************************************
Copyright (C) 2013 STMicroelectronics. All Rights Reserved.

This file is part of the Transport Engine Library.

The Transport Engine is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

Transport Engine is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with Transport Engine; see the file COPYING.  If not, write to the Free
Software Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307,
USA.

The Transport Engine Library may alternatively be licensed under a
proprietary license from ST.

************************************************************************/
/**
   @file   pti_dt.c
   @brief  Defines pti driver device tree functions
 */
#include "linuxcommon.h"
#include <linux/of.h>
#include "pti_dt.h"
#include "pti_platform.h"

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

#define MATCH_TP { .compatible = "st,tango-tp" }
#define MATCH_STFE { .compatible = "st,tango-fe" }
#define MATCH_TANGO_LITE { .compatible = "st,tango-dummy" }

struct of_device_id stpti_tp_match[] = {
	MATCH_TP,
	{},
};

struct of_device_id stpti_stfe_match[] = {
	MATCH_STFE,
	{},
};

struct of_device_id stpti_tango_lite_match[] = {
	MATCH_TANGO_LITE,
	{},
};

struct of_device_id stpti_all_match[] = {
	MATCH_TP,
	MATCH_STFE,
	MATCH_TANGO_LITE,
	{},
};

void stptiDriver_dt_dump(void)
{
	struct device_node *dn;
	uint32_t matches = 0;

	for_each_matching_node(dn, stpti_all_match) {
		struct property *prop;
		pr_info("Device node: %s\n", dn->name);

		for_each_property_of_node(dn, prop) {
			pr_info("\tProperty: %s\n", prop->name);
		}

		matches++;
	}

	pr_info("Found %u matching DT nodes for stm_te\n", matches);
}

int stptiDriver_dt_check(void)
{
	struct device_node *dn;

	dn = of_find_matching_node(NULL, stpti_all_match);
	if (!dn)
		return -ENODEV;

	return 0;
}

static int copy_dt_clocks(struct device_node *dn, struct platform_device *pdev,
		struct stpti_clk **ret_clk, uint32_t *nb_clk)
{
	struct device_node *clock_node;
	struct stpti_clk *clk = NULL;
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
		clk[clk_index].clk = devm_clk_get(&pdev->dev, clk[clk_index].name);
#endif
		clk[clk_index].enable_count = 0;

		clk_index++;
	}

	*ret_clk = clk;
	*nb_clk = clk_index;

	return 0;
}

int stpti_dt_get_tp_pdata(struct platform_device *pdev)
{
	struct device_node *dn = pdev->dev.of_node;
	struct device_node *cfg;
	struct stpti_tp_config *data = NULL;
	const char *power_state;
	int ret;

	data = devm_kzalloc(&pdev->dev, sizeof(*data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	/* Copy properties */
	cfg = of_parse_phandle(dn, "tp_config", 0);
	if (!cfg)
		return -EINVAL;

	ret = of_property_read_u32(cfg, "ddem-offset", &data->ddem_offset);
	if (ret)
		return -EINVAL;

	ret = of_property_read_u32(cfg, "idem-offset", &data->idem_offset);
	if (ret)
		return -EINVAL;

	ret = of_property_read_u32(cfg, "st-bux-plug-write-offset",
			&data->st_bux_plug_write_offset);
	if (ret)
		return -EINVAL;

	ret = of_property_read_u32(cfg, "st-bux-plug-read-offset",
			&data->st_bux_plug_read_offset);
	if (ret)
		return -EINVAL;

	ret = of_property_read_u32(cfg, "core-ctrl-offset",
			&data->core_ctrl_offset);
	if (ret)
		return -EINVAL;

	ret = of_property_read_u32(cfg, "mailbox0-to-xp70-offset",
			&data->mailbox0_to_xp70_offset);
	if (ret)
		return -EINVAL;

	ret = of_property_read_u32(cfg, "mailbox0-to-host-offset",
			&data->mailbox0_to_host_offset);
	if (ret)
		return -EINVAL;

	ret = of_property_read_u32(cfg, "writelock-error-offset",
			&data->writelock_error_offset);
	if (ret)
		return -EINVAL;

	ret = of_property_read_u32(cfg, "t3-addr-filter-offset",
			&data->t3_addr_filter_offset);
	if (ret)
		return -EINVAL;

	ret = of_property_read_u32(cfg, "timer-counter-offset",
			&data->timer_counter_offset);
	if (ret)
		return -EINVAL;

	ret = of_property_read_string(cfg, "firmware", &data->firmware);
	if (ret)
		return -EINVAL;

	ret = of_property_read_u32(cfg, "nb-vdevice", &data->nb_vdevice);
	if (ret)
		return -EINVAL;

	ret = of_property_read_u32(cfg, "nb-slot", &data->nb_slot);
	if (ret)
		return -EINVAL;

	ret = of_property_read_u32(cfg, "nb-section-filter",
			&data->nb_section_filter);
	if (ret)
		return -EINVAL;

	ret = of_property_read_u32(cfg, "nb-dma-structure",
			&data->nb_dma_structure);
	if (ret)
		return -EINVAL;

	ret = of_property_read_u32(cfg, "nb-indexer", &data->nb_indexer);
	if (ret)
		return -EINVAL;

	ret = of_property_read_u32(cfg, "nb-status-blk", &data->nb_status_blk);
	if (ret)
		return -EINVAL;

	ret = of_property_read_u32(cfg, "timer-counter-divider",
			&data->timer_counter_divider);
	if (ret)
		return -EINVAL;

	/* check if device is disabled by DT node */
	/* In case DT node is not defined, mark device as active*/
	ret = of_property_read_string(dn, "power-down",
			&power_state);
	if (ret) {
		pr_debug("power-status not defined\n");
	} else if (strcmp("yes" , power_state) == 0) {
		pr_debug("power disabled\n");
		data->power_down = 1;
	}
	/* Optional properties */
	of_property_read_u32(cfg, "software-leaky-pid-timeout",
			&data->software_leaky_pid_timeout);
	data->sc_bypass = of_property_read_bool(cfg, "sc-bypass");
	data->permit_powerdown = of_property_read_bool(cfg,
			"permit-powerdown");
	data->software_leaky_pid = of_property_read_bool(cfg,
			"software-leaky-pid");

	/* Alloc and copy clks */
	cfg = of_get_child_by_name(dn, "clocks");
	if (cfg) {
		ret = copy_dt_clocks(cfg, pdev, &data->clk, &data->nb_clk);
		if (ret)
			return ret;
		get_clks(pdev->name, data->clk, data->nb_clk);
	}

	pdev->dev.platform_data = data;

	return 0;
}

int stpti_dt_put_tp_pdata(struct platform_device *pdev)
{
	struct stpti_tp_config *data = pdev->dev.platform_data;

	/* Free allocated pdata and clks */
	if (data) {
		devm_kfree(&pdev->dev, data->clk);
		devm_kfree(&pdev->dev, data);
	}
	return 0;
}

/* Table mapping default_dest strings used in DT to
 * stptiTSHAL_TSInputDestination_t enum values */
static struct stpti_dt_stfe_dest {
	const char dest_str[16];
	const stptiTSHAL_TSInputDestination_t dest;
} stfe_dest_list[] = {
	{ "demux",      stptiTSHAL_TSINPUT_DEST_DEMUX },
	{ "tsout0",     stptiTSHAL_TSINPUT_DEST_TSOUT0 },
	{ "tsout1",     stptiTSHAL_TSINPUT_DEST_TSOUT1 },
	{ "ext_tsout0", stptiTSHAL_TSINPUT_DEST_EXT_TSOUT0 },
	{ "ext_tsout1", stptiTSHAL_TSINPUT_DEST_EXT_TSOUT1 },
};

int stpti_dt_get_stfe_pdata(struct platform_device *pdev)
{
	struct device_node *dn = pdev->dev.of_node;
	struct device_node *cfg;
	struct stpti_stfe_config *data = NULL;
	int ret;
	uint32_t stfe_version = 0;
	const char *default_dest;
	int i;
	const char *power_state;

	data = devm_kzalloc(&pdev->dev, sizeof(*data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	/* Copy properties */
	cfg = of_parse_phandle(dn, "stfe_config", 0);
	if (!cfg)
		return -EINVAL;

	ret = of_property_read_u32(cfg, "stfe-version", &stfe_version);
	if (ret)
		return -EINVAL;
	if (stfe_version >= 2) {
		data->stfe_version = STFE_V2;
	} else {
		data->stfe_version = STFE_V1;
	}

	ret = of_property_read_u32(cfg, "nb-ib", &data->nb_ib);
	if (ret)
		return -EINVAL;

	ret = of_property_read_u32(cfg, "nb-mib", &data->nb_mib);
	if (ret)
		return -EINVAL;

	ret = of_property_read_u32(cfg, "nb-swts", &data->nb_swts);
	if (ret)
		return -EINVAL;

	ret = of_property_read_u32(cfg, "nb-tsdma", &data->nb_tsdma);
	if (ret)
		return -EINVAL;

	ret = of_property_read_u32(cfg, "nb-ccsc", &data->nb_ccsc);
	if (ret)
		return -EINVAL;

	ret = of_property_read_u32(cfg, "nb-tag", &data->nb_tag);
	if (ret)
		return -EINVAL;

	ret = of_property_read_u32(cfg, "ib-offset", &data->ib_offset);
	if (ret)
		return -EINVAL;

	ret = of_property_read_u32(cfg, "tag-offset", &data->tag_offset);
	if (ret)
		return -EINVAL;

	ret = of_property_read_u32(cfg, "pid-filter-offset",
			&data->pid_filter_offset);
	if (ret)
		return -EINVAL;

	ret = of_property_read_u32(cfg, "system-regs-offset",
			&data->system_regs_offset);
	if (ret)
		return -EINVAL;

	ret = of_property_read_u32(cfg, "memdma-offset", &data->memdma_offset);
	if (ret)
		return -EINVAL;

	ret = of_property_read_u32(cfg, "tsdma-offset", &data->tsdma_offset);
	if (ret)
		return -EINVAL;

	ret = of_property_read_u32(cfg, "ccsc-offset", &data->ccsc_offset);
	if (ret)
		return -EINVAL;

	/* check if device is disabled by DT node */
	/* In case DT node is not defined, mark device as active*/
	ret = of_property_read_string(dn, "power-down", &power_state);
	if (ret) {
		pr_debug("power status not defined\n");
	} else if (strcmp("yes" , power_state) == 0) {
		pr_debug("power-disabled\n");
		data->power_down = 1;
	}

	/* STFE v2 devices required firmware */
	if (stfe_version >= 2) {
		ret  = of_property_read_string(cfg, "firmware",
				&data->firmware);
		if (ret)
			return -EINVAL;
	}

	/* Translate optional default_dest property to TSInput enum */
	ret  = of_property_read_string(dn, "default_dest", &default_dest);
	if (!ret) {
		data->default_dest = stptiTSHAL_TSINPUT_DEST_END;
		for (i = 0 ; i < ARRAY_SIZE(stfe_dest_list); i++) {
			if (strncmp(stfe_dest_list[i].dest_str, default_dest,
						sizeof(stfe_dest_list[0].dest_str)) == 0) {
				data->default_dest = stfe_dest_list[i].dest;
				break;
			}
		}
		if (data->default_dest == stptiTSHAL_TSINPUT_DEST_END) {
			pr_err("Invalid default dest: %s\n", default_dest);
			return -EINVAL;
		}
	} else {
		data->default_dest =  stptiTSHAL_TSINPUT_DEST_DEMUX;
	}
	data->stfe_ccsc_clk_enabled = false;
	/* Alloc and copy clks */
	cfg = of_get_child_by_name(dn, "clocks");
	if (cfg) {
		ret = copy_dt_clocks(cfg, pdev, &data->clk, &data->nb_clk);
		if (ret)
			return ret;
		get_clks(pdev->name, data->clk, data->nb_clk);
	}

	pdev->dev.platform_data = data;

	return 0;
}

int stpti_dt_put_stfe_pdata(struct platform_device *pdev)
{
	struct stpti_stfe_config *data = pdev->dev.platform_data;
	int i = 0;
	/* Free allocated pdata and clks */
	if (data) {
		for (i = 0; i < data->nb_clk; i++)
			data->clk[i].enable_count = 0;

		data->stfe_ccsc_clk_enabled = false;
		devm_kfree(&pdev->dev, data->clk);
		devm_kfree(&pdev->dev, data);
	}
	return 0;
}
