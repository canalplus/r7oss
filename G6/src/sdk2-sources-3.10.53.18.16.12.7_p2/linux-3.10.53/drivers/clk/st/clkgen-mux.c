/*
 * clk-gena-mux.c: ST GENA-MUX Clock driver
 *
 * Copyright (C) 2003-2013 STMicroelectronics (R&D) Limited
 *
 * Authors: Stephen Gallimore <stephen.gallimore@st.com>
 *	    Pankaj Dev <pankaj.dev@st.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 */

#include <linux/clk-provider.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/err.h>
#include <linux/string.h>
#include <linux/of.h>
#include <linux/of_address.h>

#include "clkgen.h"
#include "clk-common.h"

static DEFINE_SPINLOCK(clkgena_divmux_lock);
static DEFINE_SPINLOCK(clkgenf_lock);
static DEFINE_SPINLOCK(clkgen_doc_lock);

static const char ** __init clkgen_mux_get_parents(struct device_node *np,
						       int *num_parents)
{
	const char **parents;
	int nparents, i;

	nparents = of_count_phandle_with_args(np, "clocks", "#clock-cells");
	if (WARN_ON(nparents <= 0))
		return ERR_PTR(-EINVAL);

	parents = kzalloc(nparents * sizeof(const char *), GFP_KERNEL);
	if (!parents)
		return ERR_PTR(-ENOMEM);

	for (i = 0; i < nparents; i++)
		parents[i] = of_clk_get_parent_name(np, i);

	*num_parents = nparents;
	return parents;
}

/**
 * DOC: Clock mux with a programmable divider on each of its three inputs.
 *      The mux has an input setting which effectively gates its output.
 *
 * Traits of this clock:
 * prepare - clk_(un)prepare only ensures parent is (un)prepared
 * enable - clk_enable and clk_disable are functional & control gating
 * rate - set rate is supported
 * parent - set/get parent
 */

#define NUM_INPUTS 3

struct clkgena_divmux {
	struct clk_hw hw;
	/* Subclassed mux and divider structures */
	struct clk_mux mux;
	struct clk_divider div[NUM_INPUTS];
	/* Enable/running feedback register bits for each input */
	void __iomem *feedback_reg[NUM_INPUTS];
	int feedback_bit_idx;

	u8              muxsel;
};

#define to_clkgena_divmux(_hw) container_of(_hw, struct clkgena_divmux, hw)

struct clkgena_divmux_data {
	int num_outputs;
	int mux_offset;
	int mux_offset2;
	int mux_start_bit;
	int div_offsets[NUM_INPUTS];
	int fb_offsets[NUM_INPUTS];
	int fb_start_bit_idx;
};

#define CKGAX_CLKOPSRC_SWITCH_OFF 0x3

static int clkgena_divmux_is_running(struct clkgena_divmux *mux)
{
	u32 regval = readl(mux->feedback_reg[mux->muxsel]);
	u32 running = regval & BIT(mux->feedback_bit_idx);
	return !!running;
}

static int clkgena_divmux_enable(struct clk_hw *hw)
{
	struct clkgena_divmux *genamux = to_clkgena_divmux(hw);
	struct clk_hw *mux_hw = &genamux->mux.hw;
	unsigned long timeout;
	int ret = 0;

	mux_hw->clk = hw->clk;

	ret = clk_mux_ops.set_parent(mux_hw, genamux->muxsel);
	if (ret)
		return ret;

	timeout = jiffies + msecs_to_jiffies(10);

	while (!clkgena_divmux_is_running(genamux)) {
		if (time_after(jiffies, timeout))
			return -ETIMEDOUT;
		cpu_relax();
	}

	return 0;
}

static void clkgena_divmux_disable(struct clk_hw *hw)
{
	struct clkgena_divmux *genamux = to_clkgena_divmux(hw);
	struct clk_hw *mux_hw = &genamux->mux.hw;

	mux_hw->clk = hw->clk;

	clk_mux_ops.set_parent(mux_hw, CKGAX_CLKOPSRC_SWITCH_OFF);
}

static int clkgena_divmux_is_enabled(struct clk_hw *hw)
{
	struct clkgena_divmux *genamux = to_clkgena_divmux(hw);
	struct clk_hw *mux_hw = &genamux->mux.hw;

	mux_hw->clk = hw->clk;

	return (s8)(clk_mux_ops.get_parent(mux_hw) !=
		    CKGAX_CLKOPSRC_SWITCH_OFF);
}

u8 clkgena_divmux_get_parent(struct clk_hw *hw)
{
	struct clkgena_divmux *genamux = to_clkgena_divmux(hw);
	struct clk_hw *mux_hw = &genamux->mux.hw;

	mux_hw->clk = hw->clk;

	genamux->muxsel = clk_mux_ops.get_parent(mux_hw);
	if ((s8)genamux->muxsel < 0) {
		pr_debug("%s: %s: Invalid parent, setting to default.\n",
		      __func__, __clk_get_name(hw->clk));
		genamux->muxsel = 0;
	}

	return genamux->muxsel;
}

static int clkgena_divmux_set_parent(struct clk_hw *hw, u8 index)
{
	struct clkgena_divmux *genamux = to_clkgena_divmux(hw);

	if (index >= CKGAX_CLKOPSRC_SWITCH_OFF)
		return -EINVAL;

	genamux->muxsel = index;

	/*
	 * If the mux is already enabled, call enable directly to set the
	 * new mux position and wait for it to start running again. Otherwise
	 * do nothing.
	 */
	if (clkgena_divmux_is_enabled(hw))
		clkgena_divmux_enable(hw);

	return 0;
}

unsigned long clkgena_divmux_recalc_rate(struct clk_hw *hw,
		unsigned long parent_rate)
{
	struct clkgena_divmux *genamux = to_clkgena_divmux(hw);
	struct clk_hw *div_hw = &genamux->div[genamux->muxsel].hw;

	div_hw->clk = hw->clk;

	return clk_divider_ops.recalc_rate(div_hw, parent_rate);
}

static int clkgena_divmux_set_rate(struct clk_hw *hw, unsigned long rate,
				unsigned long parent_rate)
{
	struct clkgena_divmux *genamux = to_clkgena_divmux(hw);
	struct clk_hw *div_hw = &genamux->div[genamux->muxsel].hw;

	div_hw->clk = hw->clk;

	return clk_divider_ops.set_rate(div_hw, rate, parent_rate);
}

static long clkgena_divmux_round_rate(struct clk_hw *hw, unsigned long rate,
				   unsigned long *prate)
{
	struct clkgena_divmux *genamux = to_clkgena_divmux(hw);
	struct clk_hw *div_hw = &genamux->div[genamux->muxsel].hw;

	div_hw->clk = hw->clk;

	return clk_divider_ops.round_rate(div_hw, rate, prate);
}

static const struct clk_ops clkgena_divmux_ops = {
	.enable = clkgena_divmux_enable,
	.disable = clkgena_divmux_disable,
	.is_enabled = clkgena_divmux_is_enabled,
	.get_parent = clkgena_divmux_get_parent,
	.set_parent = clkgena_divmux_set_parent,
	.round_rate = clkgena_divmux_round_rate,
	.recalc_rate = clkgena_divmux_recalc_rate,
	.set_rate = clkgena_divmux_set_rate,
};

/**
 * clk_register_genamux - register a genamux clock with the clock framework
 */
struct clk *clk_register_genamux(const char *name,
				const char **parent_names, u8 num_parents,
				void __iomem *reg,
				const struct clkgena_divmux_data *muxdata,
				u32 idx)
{
	/*
	 * Fixed constants across all ClockgenA variants
	 */
	const int mux_width = 2;
	const int divider_width = 5;
	struct clkgena_divmux *genamux;
	struct clk *clk;
	struct clk_init_data init;
	int i;

	genamux = kzalloc(sizeof(*genamux), GFP_KERNEL);
	if (!genamux)
		return ERR_PTR(-ENOMEM);

	init.name = name;
	init.ops = &clkgena_divmux_ops;
	 /* CLK_IGNORE_UNUSED to be removed */
	init.flags = CLK_IS_BASIC;
	init.parent_names = parent_names;
	init.num_parents = num_parents;

	genamux->mux.lock  = &clkgena_divmux_lock;
	genamux->mux.mask = BIT(mux_width) - 1;
	genamux->mux.shift = muxdata->mux_start_bit + (idx * mux_width);
	if (genamux->mux.shift > 31) {
		/*
		 * We have spilled into the second mux register so
		 * adjust the register address and the bit shift accordingly
		 */
		genamux->mux.reg = reg + muxdata->mux_offset2;
		genamux->mux.shift -= 32;
	} else {
		genamux->mux.reg   = reg + muxdata->mux_offset;
	}

	for (i = 0; i < NUM_INPUTS; i++) {
		/*
		 * Divider config for each input
		 */
		void __iomem *divbase = reg + muxdata->div_offsets[i];
		genamux->div[i].width = divider_width;
		genamux->div[i].reg = divbase + (idx * sizeof(u32));

		/*
		 * Mux enabled/running feedback register for each input.
		 */
		genamux->feedback_reg[i] = reg + muxdata->fb_offsets[i];
	}

	genamux->feedback_bit_idx = muxdata->fb_start_bit_idx + idx;
	genamux->hw.init = &init;

	clk = clk_register(NULL, &genamux->hw);
	if (IS_ERR(clk)) {
		kfree(genamux);
		goto err;
	}

	pr_debug("%s: parent %s rate %lu\n",
			__clk_get_name(clk),
			__clk_get_name(clk_get_parent(clk)),
			clk_get_rate(clk));
err:
	return clk;
}

static struct clkgena_divmux_data st_divmux_c65hs = {
	.num_outputs = 4,
	.mux_offset = 0x14,
	.mux_start_bit = 0,
	.div_offsets = { 0x800, 0x900, 0xb00 },
	.fb_offsets = { 0x18, 0x1c, 0x20 },
	.fb_start_bit_idx = 0,
};

static struct clkgena_divmux_data st_divmux_c65ls = {
	.num_outputs = 14,
	.mux_offset = 0x14,
	.mux_offset2 = 0x24,
	.mux_start_bit = 8,
	.div_offsets = { 0x810, 0xa10, 0xb10 },
	.fb_offsets = { 0x18, 0x1c, 0x20 },
	.fb_start_bit_idx = 4,
};

static struct clkgena_divmux_data st_divmux_c32odf0 = {
	.num_outputs = 8,
	.mux_offset = 0x1c,
	.mux_start_bit = 0,
	.div_offsets = { 0x800, 0x900, 0xa60 },
	.fb_offsets = { 0x2c, 0x24, 0x28 },
	.fb_start_bit_idx = 0,
};

static struct clkgena_divmux_data st_divmux_c32odf1 = {
	.num_outputs = 8,
	.mux_offset = 0x1c,
	.mux_start_bit = 16,
	.div_offsets = { 0x820, 0x980, 0xa80 },
	.fb_offsets = { 0x2c, 0x24, 0x28 },
	.fb_start_bit_idx = 8,
};

static struct clkgena_divmux_data st_divmux_c32odf2 = {
	.num_outputs = 8,
	.mux_offset = 0x20,
	.mux_start_bit = 0,
	.div_offsets = { 0x840, 0xa20, 0xb10 },
	.fb_offsets = { 0x2c, 0x24, 0x28 },
	.fb_start_bit_idx = 16,
};

static struct clkgena_divmux_data st_divmux_c32odf3 = {
	.num_outputs = 8,
	.mux_offset = 0x20,
	.mux_start_bit = 16,
	.div_offsets = { 0x860, 0xa40, 0xb30 },
	.fb_offsets = { 0x2c, 0x24, 0x28 },
	.fb_start_bit_idx = 24,
};

static struct clkgena_divmux_data st_divmux_c45hs_0 = {
	.num_outputs = 6,
	.mux_offset = 0x1c,
	.mux_start_bit = 0,
	.div_offsets = { 0x800, 0x900, 0x980 },
	.fb_offsets = { 0x2c, 0x24, 0x28 },
	.fb_start_bit_idx = 0,
};

static struct clkgena_divmux_data st_divmux_c45hs_1 = {
	.num_outputs = 4,
	.mux_offset = 0x1c,
	.mux_start_bit = 12,
	.div_offsets = { 0x818, 0xa20, 0x998 },
	.fb_offsets = { 0x2c, 0x24, 0x28 },
	.fb_start_bit_idx = 6,
};

static struct clkgena_divmux_data st_divmux_c45ls = {
	.num_outputs = 22,
	.mux_offset = 0x1c,
	.mux_offset2 = 0x20,
	.mux_start_bit = 20,
	.div_offsets = { 0x828, 0xa30, 0xb00 },
	.fb_offsets = { 0x2c, 0x24, 0x28 },
	.fb_start_bit_idx = 10,
};

static struct of_device_id clkgena_divmux_of_match[] = {
	{
		.compatible = "st,clkgena-divmux-c65-hs",
		.data = &st_divmux_c65hs,
	},
	{
		.compatible = "st,clkgena-divmux-c65-ls",
		.data = &st_divmux_c65ls,
	},
	{
		.compatible = "st,clkgena-divmux-c32-odf0",
		.data = &st_divmux_c32odf0,
	},
	{
		.compatible = "st,clkgena-divmux-c32-odf1",
		.data = &st_divmux_c32odf1,
	},
	{
		.compatible = "st,clkgena-divmux-c32-odf2",
		.data = &st_divmux_c32odf2,
	},
	{
		.compatible = "st,clkgena-divmux-c32-odf3",
		.data = &st_divmux_c32odf3,
	},
	{
		.compatible = "st,clkgena-divmux-c45-hs-0",
		.data = &st_divmux_c45hs_0,
	},
	{
		.compatible = "st,clkgena-divmux-c45-hs-1",
		.data = &st_divmux_c45hs_1,
	},
	{
		.compatible = "st,clkgena-divmux-c45-ls",
		.data = &st_divmux_c45ls,
	},
	{}
};

static void __iomem * __init clkgen_get_register_base(
				struct device_node *np)
{
	struct device_node *pnode;
	void __iomem *reg = NULL;

	pnode = of_get_parent(np);
	if (!pnode)
		return NULL;

	reg = of_iomap(pnode, 0);

	of_node_put(pnode);
	return reg;
}

void __init st_of_clkgena_divmux_setup(struct device_node *np)
{
	const struct of_device_id *match;
	const struct clkgena_divmux_data *data;
	struct clk_onecell_data *clk_data;
	void __iomem *reg;
	const char **parents;
	int num_parents = 0, i;

	match = of_match_node(clkgena_divmux_of_match, np);
	if (WARN_ON(!match))
		return;

	data = (struct clkgena_divmux_data *)match->data;

	reg = clkgen_get_register_base(np);
	if (!reg)
		return;

	parents = clkgen_mux_get_parents(np, &num_parents);
	if (IS_ERR(parents))
		return;

	clk_data = kzalloc(sizeof(*clk_data), GFP_KERNEL);
	if (!clk_data)
		goto err;

	clk_data->clk_num = data->num_outputs;
	clk_data->clks = kzalloc(clk_data->clk_num * sizeof(struct clk *),
				 GFP_KERNEL);

	if (!clk_data->clks)
		goto err;

	for (i = 0; i < clk_data->clk_num; i++) {
		struct clk *clk;
		const char *clk_name;

		if (of_property_read_string_index(np, "clock-output-names",
						  i, &clk_name))
			break;

		/*
		 * If we read an empty clock name then the output is unused
		 */
		if (*clk_name == '\0')
			continue;

		clk = clk_register_genamux(clk_name, parents, num_parents,
					   reg, data, i);

		if (IS_ERR(clk))
			goto err;

		clk_data->clks[i] = clk;
	}

	kfree(parents);

	of_clk_add_provider(np, of_clk_src_onecell_get, clk_data);
	return;
err:
	if (clk_data)
		kfree(clk_data->clks);

	kfree(clk_data);
	kfree(parents);
}
CLK_OF_DECLARE(clkgenadivmux, "st,clkgena-divmux", st_of_clkgena_divmux_setup);

struct clkgena_prediv_data {
	u32 offset;
	u8 shift;
	struct clk_div_table *table;
};

static struct clk_div_table prediv_table16[] = {
	{ .val = 0, .div = 1 },
	{ .val = 1, .div = 16 },
	{ .div = 0 },
};

static struct clkgena_prediv_data prediv_c65_data= {
	.offset = 0x4c,
	.shift = 31,
	.table = prediv_table16,
};

static struct clkgena_prediv_data prediv_c32_data= {
	.offset = 0x50,
	.shift = 1,
	.table = prediv_table16,
};

static struct clkgena_prediv_data prediv_c45_data = {
	.offset = 0x4c,
	.shift = 31,
	.table = prediv_table16,
};

static struct of_device_id clkgena_prediv_of_match[] = {
	{ .compatible = "st,clkgena-prediv-c65", .data = &prediv_c65_data },
	{ .compatible = "st,clkgena-prediv-c32", .data = &prediv_c32_data },
	{ .compatible = "st,clkgena-prediv-c45", .data = &prediv_c45_data },
	{}
};

void __init st_of_clkgena_prediv_setup(struct device_node *np)
{
	const struct of_device_id *match;
	void __iomem *reg;
	const char *parent_name, *clk_name;
	struct clk *clk;
	struct clkgena_prediv_data *data;

	match = of_match_node(clkgena_prediv_of_match, np);
	if (!match) {
		pr_err("%s: No matching data\n", __func__);
		return;
	}

	data = (struct clkgena_prediv_data *)match->data;

	reg = clkgen_get_register_base(np);
	if (!reg)
		return;

	parent_name = of_clk_get_parent_name(np, 0);
	if (!parent_name)
		return;

	if (of_property_read_string_index(np, "clock-output-names",
					  0, &clk_name))
		return;

	clk = clk_register_divider_table(NULL, clk_name, parent_name, 0,
					 reg + data->offset, data->shift, 1,
					 0, data->table, NULL);
	if (IS_ERR(clk))
		return;

	of_clk_add_provider(np, of_clk_src_simple_get, clk);
	pr_debug("%s: parent %s rate %u\n",
		__clk_get_name(clk),
		__clk_get_name(clk_get_parent(clk)),
		(unsigned int)clk_get_rate(clk));

	return;
}
CLK_OF_DECLARE(clkgenaprediv, "st,clkgena-prediv", st_of_clkgena_prediv_setup);

struct clkgen_mux_data {
	u32 offset;
	u8 shift;
	u8 width;
	spinlock_t *lock;
	unsigned long clk_flags;
	u8 mux_flags;
};

static struct clkgen_mux_data clkgen_mux_f_vcc_fvdp_415 = {
	.offset = 0,
	.shift = 0,
	.width = 1,
};

static struct clkgen_mux_data clkgen_mux_c_vcc_hd_416 = {
	.offset = 0,
	.shift = 0,
	.width = 1,
};

static struct clkgen_mux_data clkgen_mux_f_vcc_fvdp_416 = {
	.offset = 0,
	.shift = 0,
	.width = 1,
};

static struct clkgen_mux_data clkgen_mux_f_vcc_hva_416 = {
	.offset = 0,
	.shift = 0,
	.width = 1,
};

static struct clkgen_mux_data clkgen_mux_f_vcc_hd_416 = {
	.offset = 0,
	.shift = 16,
	.width = 1,
	.lock = &clkgenf_lock,
};

static struct clkgen_mux_data clkgen_mux_c_vcc_sd_416 = {
	.offset = 0,
	.shift = 17,
	.width = 1,
	.lock = &clkgenf_lock,
};

static struct clkgen_mux_data stih415_a9_mux_data = {
	.offset = 0,
	.shift = 1,
	.width = 2,
	.lock = &clkgen_a9_lock,
};
static struct clkgen_mux_data stih416_a9_mux_data = {
	.offset = 0,
	.shift = 0,
	.width = 2,
};
static struct clkgen_mux_data stih407_a9_mux_data = {
	.offset = 0x1a4,
	.shift = 1,
	.width = 2,
	.lock = &clkgen_a9_lock,
};

static struct of_device_id mux_of_match[] = {
	{
		.compatible = "st,stih415-clkgenf-vcc-fvdp",
		.data = &clkgen_mux_f_vcc_fvdp_415,
	},
	{
		.compatible = "st,stih416-clkgenc-vcc-hd",
		.data = &clkgen_mux_c_vcc_hd_416,
	},
	{
		.compatible = "st,stih416-clkgenf-vcc-fvdp",
		.data = &clkgen_mux_f_vcc_fvdp_416,
	},
	{
		.compatible = "st,stih416-clkgenf-vcc-hva",
		.data = &clkgen_mux_f_vcc_hva_416,
	},
	{
		.compatible = "st,stih416-clkgenf-vcc-hd",
		.data = &clkgen_mux_f_vcc_hd_416,
	},
	{
		.compatible = "st,stih416-clkgenf-vcc-sd",
		.data = &clkgen_mux_c_vcc_sd_416,
	},
	{
		.compatible = "st,stih415-clkgen-a9-mux",
		.data = &stih415_a9_mux_data,
	},
	{
		.compatible = "st,stih416-clkgen-a9-mux",
		.data = &stih416_a9_mux_data,
	},
	{
		.compatible = "st,stih407-clkgen-a9-mux",
		.data = &stih407_a9_mux_data,
	},
	{}
};

void __init st_of_clkgen_mux_setup(struct device_node *np)
{
	const struct of_device_id *match;
	struct clk *clk;
	void __iomem *reg;
	const char **parents;
	int num_parents;
	struct clkgen_mux_data *data;

	match = of_match_node(mux_of_match, np);
	if (!match) {
		pr_err("%s: No matching data\n", __func__);
		return;
	}

	data = (struct clkgen_mux_data *)match->data;

	reg = of_iomap(np, 0);
	if (!reg) {
		pr_err("%s: Failed to get base address\n", __func__);
		return;
	}

	parents = clkgen_mux_get_parents(np, &num_parents);
	if (IS_ERR(parents)) {
		pr_err("%s: Failed to get parents (%ld)\n",
				__func__, PTR_ERR(parents));
		return;
	}

	clk = clk_register_mux(NULL, np->name, parents, num_parents,
				data->clk_flags | CLK_SET_RATE_PARENT,
				reg + data->offset,
				data->shift, data->width, data->mux_flags,
				data->lock);
	if (IS_ERR(clk))
		goto err;

	pr_debug("%s: parent %s rate %u\n",
		 __clk_get_name(clk),
		 __clk_get_name(clk_get_parent(clk)),
		 (unsigned int)clk_get_rate(clk));

	of_clk_add_provider(np, of_clk_src_simple_get, clk);

err:
	kfree(parents);

	return;
}
CLK_OF_DECLARE(clkgen_mux, "st,clkgen-mux", st_of_clkgen_mux_setup);

#define VCC_MAX_CHANNELS 16

#define VCC_GATE_OFFSET 0x0
#define VCC_MUX_OFFSET 0x4
#define VCC_DIV_OFFSET 0x8

struct clkgen_vcc_data {
	spinlock_t *lock;
	unsigned long clk_flags;
};

static struct clkgen_vcc_data st_clkgenf_vcc_415 = {
	.lock = NULL,
};

static struct clkgen_vcc_data st_clkgenc_vcc_41x = {
	.clk_flags = CLK_SET_RATE_PARENT,
};

static struct clkgen_vcc_data st_clkgenf_vcc_416 = {
	.lock = &clkgenf_lock,
};

static struct of_device_id vcc_of_match[] = {
	{ .compatible = "st,stih41x-clkgenc", .data = &st_clkgenc_vcc_41x },
	{ .compatible = "st,stih415-clkgenf", .data = &st_clkgenf_vcc_415 },
	{ .compatible = "st,stih416-clkgenf", .data = &st_clkgenf_vcc_416 },
	{}
};

void __init st_of_clkgen_vcc_setup(struct device_node *np)
{
	const struct of_device_id *match;
	void __iomem *reg;
	const char **parents;
	int num_parents, i;
	struct clk_onecell_data *clk_data;
	struct clkgen_vcc_data *data;

	match = of_match_node(vcc_of_match, np);
	if (WARN_ON(!match))
		return;
	data = (struct clkgen_vcc_data *)match->data;

	reg = of_iomap(np, 0);
	if (!reg)
		return;

	parents = clkgen_mux_get_parents(np, &num_parents);
	if (IS_ERR(parents))
		return;

	clk_data = kzalloc(sizeof(*clk_data), GFP_KERNEL);
	if (!clk_data)
		goto err;

	clk_data->clk_num = VCC_MAX_CHANNELS;
	clk_data->clks = kzalloc(clk_data->clk_num * sizeof(struct clk *),
				 GFP_KERNEL);

	if (!clk_data->clks)
		goto err;

	for (i = 0; i < clk_data->clk_num; i++) {
		struct clk *clk;
		const char *clk_name;
		struct clk_gate *gate;
		struct clk_divider *div;
		struct clk_mux *mux;

		if (of_property_read_string_index(np, "clock-output-names",
						  i, &clk_name))
			break;

		/*
		 * If we read an empty clock name then the output is unused
		 */
		if (*clk_name == '\0')
			continue;

		gate = kzalloc(sizeof(struct clk_gate), GFP_KERNEL);
		if (!gate)
			break;

		div = kzalloc(sizeof(struct clk_divider), GFP_KERNEL);
		if (!div) {
			kfree(gate);
			break;
		}

		mux = kzalloc(sizeof(struct clk_mux), GFP_KERNEL);
		if (!mux) {
			kfree(gate);
			kfree(div);
			break;
		}

		gate->reg = reg + VCC_GATE_OFFSET;
		gate->bit_idx = i;
		gate->flags = CLK_GATE_SET_TO_DISABLE;
		gate->lock = data->lock;

		div->reg = reg + VCC_DIV_OFFSET;
		div->shift = 2 * i;
		div->width = 2;
		div->flags = CLK_DIVIDER_POWER_OF_TWO |
			CLK_DIVIDER_ROUND_CLOSEST;

		mux->reg = reg + VCC_MUX_OFFSET;
		mux->shift = 2 * i;
		mux->mask = 0x3;

		clk = clk_register_composite(NULL, clk_name, parents,
					     num_parents,
					     &mux->hw, &clk_mux_ops,
					     &div->hw, &clk_divider_ops,
					     &gate->hw, &clk_gate_ops,
					     data->clk_flags);
		if (IS_ERR(clk)) {
			kfree(gate);
			kfree(div);
			kfree(mux);
			goto err;
		}

		pr_debug("%s: parent %s rate %u\n",
			 __clk_get_name(clk),
			 __clk_get_name(clk_get_parent(clk)),
			 (unsigned int)clk_get_rate(clk));

		clk_data->clks[i] = clk;
	}

	kfree(parents);

	of_clk_add_provider(np, of_clk_src_onecell_get, clk_data);
	return;

err:
	for (i = 0; i < clk_data->clk_num; i++) {
		struct clk_composite *composite;

		if (!clk_data->clks[i])
			continue;

		composite = container_of(__clk_get_hw(clk_data->clks[i]),
					 struct clk_composite, hw);
		kfree(container_of(composite->gate_hw, struct clk_gate, hw));
		kfree(container_of(composite->rate_hw, struct clk_divider, hw));
		kfree(container_of(composite->mux_hw, struct clk_mux, hw));
	}

	if (clk_data)
		kfree(clk_data->clks);

	kfree(clk_data);
	kfree(parents);
}
CLK_OF_DECLARE(clkgen_vcc, "st,clkgen-vcc", st_of_clkgen_vcc_setup);

/**
 * DOC: Clock with pre-divider, having 3 ouputs.
 *      The mux has an input setting which effectively gates its output.
 *
 * Traits of this clock:
 * prepare - clk_(un)prepare only ensures parent is (un)prepared
 * enable - clk_enable and clk_disable are functional & control gating
 * rate - set rate is supported
 */

#define bit_mask(d)	(BIT(d) - 1)

#define CCM_DIV1K_SHIFT 16
#define CCM_DIV1K_DIVIDER 1000
#define CCM_DIVX_DIVIDER 256

struct clkgen_ccm_data {
	u32 update_offset;
	u8 gatediv_shift, divx_shift;
	/* lock */
	spinlock_t *lock;
};

struct clkgen_ccm {
	struct clk_hw hw;
	/* Subclassed divider structures */
	struct clk_divider div;

	u8 div_val, off_val;
	u8 prog_shift;
	u32 prog_offset;
	/* lock */
	spinlock_t	*lock;
};

#define to_clkgen_ccm(_hw) container_of(_hw, struct clkgen_ccm, hw)

static int clkgen_ccm_is_enabled(struct clk_hw *hw)
{
	struct clkgen_ccm *ccm = to_clkgen_ccm(hw);
	u32 regval = (readl(ccm->div.reg) >> ccm->div.shift)
					& ~bit_mask(ccm->div.width);

	return (regval != ccm->off_val);
}

static void clkgen_ccm_xable(struct clk_hw *hw, bool enable)
{
	struct clkgen_ccm *ccm = to_clkgen_ccm(hw);
	u32 regval;
	unsigned long flags = 0;

	if (enable ? clkgen_ccm_is_enabled(hw) : !clkgen_ccm_is_enabled(hw))
		return;

	spin_lock_irqsave(ccm->lock, flags);

	regval = readl(ccm->div.reg);

	regval &= (~bit_mask(ccm->div.width) << ccm->div.shift);
	writel(regval | ((enable ? ccm->div_val : ccm->off_val)
			 << ccm->div.shift), ccm->div.reg);

	spin_unlock_irqrestore(ccm->lock, flags);
	writel(BIT(ccm->prog_shift), ccm->div.reg + ccm->prog_offset);
}

static int clkgen_ccm_enable(struct clk_hw *hw)
{
	clkgen_ccm_xable(hw, true);

	return 0;
}

static void clkgen_ccm_disable(struct clk_hw *hw)
{
	clkgen_ccm_xable(hw, false);
}

unsigned long clkgen_ccm_gatediv_recalc_rate(struct clk_hw *hw,
		unsigned long parent_rate)
{
	struct clkgen_ccm *ccm = to_clkgen_ccm(hw);
	struct clk_hw *div_hw = &ccm->div.hw;
	u32 regval = readl(ccm->div.reg);

	if (regval & BIT(CCM_DIV1K_SHIFT))
		return parent_rate / CCM_DIV1K_DIVIDER;

	if (((regval >> ccm->div.shift)
		 & bit_mask(ccm->div.width)) == 0x0)	/* Off */
		return (ccm->div_val == 0) ? 0 : (parent_rate / ccm->div_val);

	ccm->div_val = (regval >> ccm->div.shift) & bit_mask(ccm->div.width);

	return clk_divider_ops.recalc_rate(div_hw, parent_rate);
}

unsigned long clkgen_ccm_divx_recalc_rate(struct clk_hw *hw,
		unsigned long parent_rate)
{
	struct clkgen_ccm *ccm = to_clkgen_ccm(hw);
	struct clk_hw *div_hw = &ccm->div.hw;
	u32 regval = (readl(ccm->div.reg) >> ccm->div.shift)
					& bit_mask(ccm->div.width);

	if (regval == 0x0)
		return parent_rate / 256;

	if (regval == 0x1)	/* Off */
		return (ccm->div_val == 1) ? 0 :
			(parent_rate / ((ccm->div_val == 0x0) ?
					CCM_DIVX_DIVIDER : ccm->div_val));

	ccm->div_val = regval;

	return clk_divider_ops.recalc_rate(div_hw, parent_rate);
}

static int clkgen_ccm_gatediv_set_rate(struct clk_hw *hw, unsigned long rate,
				unsigned long parent_rate)
{
	struct clkgen_ccm *ccm = to_clkgen_ccm(hw);
	struct clk_hw *div_hw = &ccm->div.hw;
	u32 regval, ret = 0;
	unsigned long flags = 0;

	if (!rate || !parent_rate)
		return -EINVAL;

	spin_lock_irqsave(ccm->lock, flags);
	regval = readl(ccm->div.reg) & ~BIT(CCM_DIV1K_SHIFT);

	if (parent_rate / rate == CCM_DIV1K_DIVIDER) {
		writel(regval | BIT(CCM_DIV1K_SHIFT), ccm->div.reg);
		ccm->div_val = 1;
	} else {
		writel(regval, ccm->div.reg);
		ret = clk_divider_ops.set_rate(div_hw, rate, parent_rate);
		regval = readl(ccm->div.reg);
		if (!ret)
			ccm->div_val = (regval >> ccm->div.shift)
					& bit_mask(ccm->div.width);
	}

	spin_unlock_irqrestore(ccm->lock, flags);

	if (!ret)
		writel(BIT(ccm->prog_shift),
		       ccm->div.reg + ccm->prog_offset);

	return ret;
}

static int clkgen_ccm_divx_set_rate(struct clk_hw *hw, unsigned long rate,
				unsigned long parent_rate)
{
	struct clkgen_ccm *ccm = to_clkgen_ccm(hw);
	struct clk_hw *div_hw = &ccm->div.hw;
	u32 regval, ret = 0;
	unsigned long flags = 0;

	if (!rate || !parent_rate)
		return -EINVAL;

	spin_lock_irqsave(ccm->lock, flags);
	regval = readl(ccm->div.reg) & (~bit_mask(ccm->div.width)
					<< ccm->div.shift);

	if (parent_rate / rate == CCM_DIVX_DIVIDER)
		writel(regval, ccm->div.reg);	/* write 0x0 */
	else
		ret = clk_divider_ops.set_rate(div_hw, rate, parent_rate);
	spin_unlock_irqrestore(ccm->lock, flags);

	if (ret)
		return ret;

	regval = readl(ccm->div.reg);

	ccm->div_val = (regval >> ccm->div.shift)
					& bit_mask(ccm->div.width);

	writel((0x1 << ccm->prog_shift), ccm->div.reg + ccm->prog_offset);

	return ret;
}

static long clkgen_ccm_gatediv_round_rate(struct clk_hw *hw, unsigned long rate,
				   unsigned long *prate)
{
	struct clkgen_ccm *ccm = to_clkgen_ccm(hw);
	struct clk_hw *div_hw = &ccm->div.hw;

	if (*prate / rate > 255)
		return *prate / CCM_DIV1K_DIVIDER;

	return clk_divider_ops.round_rate(div_hw, rate, prate);
}

static long clkgen_ccm_divx_round_rate(struct clk_hw *hw, unsigned long rate,
				   unsigned long *prate)
{
	struct clkgen_ccm *ccm = to_clkgen_ccm(hw);
	struct clk_hw *div_hw = &ccm->div.hw;

	if (*prate / rate == 1)
		return 0;	/* Error */

	if (*prate / rate > 255)
		return *prate / CCM_DIVX_DIVIDER;

	return clk_divider_ops.round_rate(div_hw, rate, prate);
}

static const struct clk_ops clkgen_ccm_gatediv_ops = {
	.enable = clkgen_ccm_enable,
	.disable = clkgen_ccm_disable,
	.is_enabled = clkgen_ccm_is_enabled,
	.round_rate = clkgen_ccm_gatediv_round_rate,
	.recalc_rate = clkgen_ccm_gatediv_recalc_rate,
	.set_rate = clkgen_ccm_gatediv_set_rate,
};

static const struct clk_ops clkgen_ccm_divx_ops = {
	.enable = clkgen_ccm_enable,
	.disable = clkgen_ccm_disable,
	.is_enabled = clkgen_ccm_is_enabled,
	.round_rate = clkgen_ccm_divx_round_rate,
	.recalc_rate = clkgen_ccm_divx_recalc_rate,
	.set_rate = clkgen_ccm_divx_set_rate,
};

struct ccm_setup_data {
	u8 shift, width, off_val;
	const struct clk_ops *ops;
};

/**
 * clk_register_divx - register a divx clock with the clock framework
 */
struct clk *clk_register_ccm(const char *name, const char *parent_name,
			     struct ccm_setup_data *ccm_data, u32 prog_offset,
			     u8 prog_shift, void __iomem *reg, spinlock_t *lock)
{
	struct clkgen_ccm *ccm;
	struct clk *clk;
	struct clk_init_data init;

	ccm = kzalloc(sizeof(*ccm), GFP_KERNEL);
	if (!ccm)
		return ERR_PTR(-ENOMEM);

	init.name = name;
	init.ops = ccm_data->ops;
	 /* CLK_IGNORE_UNUSED to be removed */
	init.flags = CLK_IS_BASIC;
	init.parent_names = &parent_name;
	init.num_parents = 1;

	/*
	 * Divider config
	 */
	ccm->div.shift = ccm_data->shift;
	ccm->div.width = ccm_data->width;
	ccm->div.reg = reg;
	ccm->div.flags = CLK_DIVIDER_ONE_BASED;

	ccm->off_val = ccm_data->off_val;
	ccm->div_val = ccm_data->off_val;
	ccm->prog_offset = prog_offset;
	ccm->prog_shift = prog_shift;
	ccm->lock = lock;

	ccm->hw.init = &init;

	clk = clk_register(NULL, &ccm->hw);
	if (IS_ERR(clk)) {
		kfree(ccm);
		return clk;
	}

	ccm->div.hw.clk = clk;
	pr_debug("%s: parent %s rate %lu\n",
		 __clk_get_name(clk),
		 __clk_get_name(clk_get_parent(clk)),
		 clk_get_rate(clk));

	return clk;
}

static struct ccm_setup_data st_ccm_gatediv = {
	.shift		= 8,
	.width		= 8,
	.off_val	= 0,
	.ops		= &clkgen_ccm_gatediv_ops
};

static struct ccm_setup_data st_ccm_divx = {
	.shift		= 0,
	.width		= 8,
	.off_val	= 1,
	.ops		= &clkgen_ccm_divx_ops
};

#define CCM_MAX_CHAN 4

static void __init st_of_create_ccm_divclocks(struct device_node *np,
					      const char *parent_name,
					      struct clkgen_ccm_data *data,
					      void __iomem *reg,
					      spinlock_t *lock)
{
	struct clk_onecell_data *clk_data;
	int ccm_chan;

	clk_data = kzalloc(sizeof(*clk_data), GFP_KERNEL);
	if (!clk_data)
		return;

	clk_data->clk_num = CCM_MAX_CHAN;
	clk_data->clks = kzalloc(clk_data->clk_num * sizeof(struct clk *),
				 GFP_KERNEL);

	if (!clk_data->clks) {
		kfree(clk_data);
		return;
	}

	for (ccm_chan = 0; ccm_chan < clk_data->clk_num; ccm_chan++) {
		struct clk *clk;
		const char *clk_name;

		if (of_property_read_string_index(np, "clock-output-names",
						  ccm_chan, &clk_name)) {
			break;
		}

		/*
		 * If we read an empty clock name then the channel is unused
		 */
		if (*clk_name == '\0')
			continue;

		if (ccm_chan < 3)
			clk = clk_register_fixed_factor(NULL, clk_name,
							parent_name, 0, 1,
							(1 << ccm_chan));
		else
			clk = clk_register_ccm(clk_name, parent_name,
					       &st_ccm_divx,
					       data->update_offset,
					       data->divx_shift, reg, lock);

		/*
		 * If there was an error registering this clock output, clean
		 * up and move on to the next one.
		 */
		if (!IS_ERR(clk)) {
			clk_data->clks[ccm_chan] = clk;
			pr_debug("%s: parent %s rate %u\n",
				 __clk_get_name(clk),
				 __clk_get_name(clk_get_parent(clk)),
				 (unsigned int)clk_get_rate(clk));
		}
	}

	of_clk_add_provider(np, of_clk_src_onecell_get, clk_data);
}

static struct clkgen_ccm_data st_ccm_A_127 = {
	.update_offset = 0x14,
	.gatediv_shift = 1,
	.divx_shift = 0,
};

static struct clkgen_ccm_data st_ccm_B_127 = {
	.update_offset = 0x10,
	.gatediv_shift = 3,
	.divx_shift = 2,
};

static struct clkgen_ccm_data st_ccm_D_127 = {
	.update_offset = 0x8,
	.gatediv_shift = 7,
	.divx_shift = 6,
};

static struct clkgen_ccm_data st_ccm_DOC_A_127 = {
	.update_offset = 0x14,
	.gatediv_shift = 1,
	.divx_shift = 0,
	.lock = &clkgen_doc_lock,
};

static struct of_device_id ccm_of_match[] = {
	{
		.compatible = "st,stid127-ccm-A",
		.data = (void *)&st_ccm_A_127
	},
	{
		.compatible = "st,stid127-ccm-B",
		.data = (void *)&st_ccm_B_127
	},
	{
		.compatible = "st,stid127-ccm-D",
		.data = (void *)&st_ccm_D_127
	},
	{
		.compatible = "st,stid127-ccm-DOC-A",
		.data = (void *)&st_ccm_DOC_A_127
	},
	{}
};

static void __init clkgen_ccm_setup(struct device_node *np)
{
	const struct of_device_id *match;
	struct clk *clk;
	const char *gatediv_name, *clk_parent_name;
	void __iomem *reg;
	/* lock */
	spinlock_t *lock;
	struct clkgen_ccm_data *data;

	match = of_match_node(ccm_of_match, np);
	if (WARN_ON(!match))
		return;

	data = (struct clkgen_ccm_data *)match->data;

	reg = of_iomap(np, 0);
	if (!reg)
		return;

	/* select IFE_REF as source */
	writel(readl(reg) | (1 << 17), reg);

	clk_parent_name = of_clk_get_parent_name(np, 0);
	if (!clk_parent_name)
		return;

	gatediv_name = kasprintf(GFP_KERNEL, "%s.gatediv", np->name);
	if (!gatediv_name)
		return;

	if (!data->lock) {
		lock = kzalloc(sizeof(*lock), GFP_KERNEL);
		if (!lock)
			goto err_exit;

		spin_lock_init(lock);
	} else {
		lock = data->lock;
	}

	clk = clk_register_ccm(gatediv_name, clk_parent_name, &st_ccm_gatediv,
			       data->update_offset, data->gatediv_shift, reg,
			       lock);

	if (IS_ERR(clk)) {
		if (!data->lock)
			kfree(lock);
		goto err_exit;
	} else {
		pr_debug("%s: parent %s rate %u\n",
			 __clk_get_name(clk),
			 __clk_get_name(clk_get_parent(clk)),
			 (unsigned int)clk_get_rate(clk));
	}

	st_of_create_ccm_divclocks(np, gatediv_name,
				   (struct clkgen_ccm_data *)match->data, reg,
				   lock);

err_exit:
	kfree(gatediv_name); /* No longer need local copy of the gatediv name */
}
CLK_OF_DECLARE(clkgen_ccm,
	       "st,clkgen-ccm", clkgen_ccm_setup);

/**
 * DOC: Clock with pre-divider, having 3 ouputs.
 *      The mux has an input setting which effectively gates its output.
 *
 * Traits of this clock:
 * prepare - clk_(un)prepare only ensures parent is (un)prepared
 * enable - clk_enable and clk_disable are functional & control gating
 * rate - set rate is supported
 */

#define bit_mask(d)	(BIT(d) - 1)

#define CSM_DIV2_DIVIDER 2
#define CSM_DIV1K_DIVIDER 1000

struct clkgen_csm {
	struct clk_hw hw;

	void __iomem *reg_stop, *reg_slow;
	u8 id;
	bool stop_val, slow_val;
	/* lock */
	spinlock_t	*lock;
};

#define to_clkgen_csm(_hw) container_of(_hw, struct clkgen_csm, hw)

static int clkgen_csm_is_enabled(struct clk_hw *hw)
{
	struct clkgen_csm *csm = to_clkgen_csm(hw);

	/* stop_val = 1, slow_val = 0 ==> clock is stopped */

	return !(readl(csm->reg_stop) & BIT(csm->id)) ||
		!!(readl(csm->reg_slow) & BIT(csm->id));
}

static void clkgen_csm_xable(struct clk_hw *hw, bool enable)
{
	struct clkgen_csm *csm = to_clkgen_csm(hw);
	unsigned long flags = 0;

	if (enable ? clkgen_csm_is_enabled(hw) : !clkgen_csm_is_enabled(hw))
		return;

	spin_lock_irqsave(csm->lock, flags);

	if (enable) {
		writel((readl(csm->reg_stop) & ~BIT(csm->id)) |
			(csm->stop_val << csm->id), csm->reg_stop);
		writel((readl(csm->reg_slow) & ~BIT(csm->id)) |
			(csm->slow_val << csm->id), csm->reg_slow);
	} else {
		writel(readl(csm->reg_stop) | BIT(csm->id), csm->reg_stop);
		writel(readl(csm->reg_slow) & ~BIT(csm->id), csm->reg_slow);
	}

	spin_unlock_irqrestore(csm->lock, flags);
}

static int clkgen_csm_enable(struct clk_hw *hw)
{
	clkgen_csm_xable(hw, true);

	return 0;
}

static void clkgen_csm_disable(struct clk_hw *hw)
{
	clkgen_csm_xable(hw, false);
}

unsigned long clkgen_csm_recalc_rate(struct clk_hw *hw,
		unsigned long parent_rate)
{
	struct clkgen_csm *csm = to_clkgen_csm(hw);

	csm->stop_val = (readl(csm->reg_stop) >> csm->id) & 0x1;
	csm->slow_val = (readl(csm->reg_slow) >> csm->id) & 0x1;

	/* CSM bits config
	 * stop_val = 0, slow_val = 0 ==> bypass
	 * stop_val = 0, slow_val = 1 ==> div by 1000 mode
	 * stop_val = 1, slow_val = 0 ==> clock is stopped
	 * stop_val = 1, slow_val = 1 ==> div by 2 mode
	 */

	switch ((csm->stop_val << 1) | csm->slow_val) {
	case 0:
		return parent_rate;
	case 1:
		return parent_rate / CSM_DIV1K_DIVIDER;
	case 3:
		return parent_rate / CSM_DIV2_DIVIDER;
	default:
		return 0;
	}
}

static int clkgen_csm_set_rate(struct clk_hw *hw, unsigned long rate,
				unsigned long parent_rate)
{
	struct clkgen_csm *csm = to_clkgen_csm(hw);
	u32 ret = 0;
	unsigned long flags = 0;

	if (!rate || !parent_rate)
		return -EINVAL;

	spin_lock_irqsave(csm->lock, flags);

	switch (parent_rate / rate) {
	case 1:
		writel(readl(csm->reg_stop) & ~BIT(csm->id), csm->reg_stop);
		writel(readl(csm->reg_slow) & ~BIT(csm->id), csm->reg_slow);
		break;
	case CSM_DIV2_DIVIDER:
		writel(readl(csm->reg_stop) | BIT(csm->id), csm->reg_stop);
		writel(readl(csm->reg_slow) | BIT(csm->id), csm->reg_slow);
		break;
	case CSM_DIV1K_DIVIDER:
		writel(readl(csm->reg_stop) & ~BIT(csm->id), csm->reg_stop);
		writel(readl(csm->reg_slow) | BIT(csm->id), csm->reg_slow);
		break;
	default:
		ret = -EINVAL;
	}

	spin_unlock_irqrestore(csm->lock, flags);

	return ret;
}

static long clkgen_csm_round_rate(struct clk_hw *hw, unsigned long rate,
				   unsigned long *prate)
{
	u32 div_factor = *prate / rate;

	if (div_factor > 1) {
		if (div_factor > 2)
			return *prate / CSM_DIV1K_DIVIDER;

		return *prate / CSM_DIV2_DIVIDER;
	}

	return *prate;
}

static const struct clk_ops clkgen_csm_ops = {
	.enable = clkgen_csm_enable,
	.disable = clkgen_csm_disable,
	.is_enabled = clkgen_csm_is_enabled,
	.round_rate = clkgen_csm_round_rate,
	.recalc_rate = clkgen_csm_recalc_rate,
	.set_rate = clkgen_csm_set_rate,
};

struct clkgen_csm_data {
	u32 stop_offset, slow_offset;
	u8 start_bit;
};

/**
 * clk_register_csm - register a csm clock with the clock framework
 */
struct clk *clk_register_csm(const char *name, const char *parent_name,
			     void __iomem *reg_stop, void __iomem *reg_slow,
				 u8 id, spinlock_t *lock)
{
	struct clkgen_csm *csm;
	struct clk *clk;
	struct clk_init_data init;

	csm = kzalloc(sizeof(*csm), GFP_KERNEL);
	if (!csm)
		return ERR_PTR(-ENOMEM);

	init.name = name;
	init.ops = &clkgen_csm_ops;
	 /* CLK_IGNORE_UNUSED to be removed */
	init.flags = CLK_IS_BASIC;
	init.parent_names = &parent_name;
	init.num_parents = 1;

	/*
	 * CSM config
	 */
	csm->reg_stop = reg_stop;
	csm->reg_slow = reg_slow;
	csm->id = id;
	/* initialise with OFF val */
	csm->stop_val = 1;
	csm->slow_val = 0;
	csm->lock = lock;

	csm->hw.init = &init;

	clk = clk_register(NULL, &csm->hw);
	if (IS_ERR(clk)) {
		kfree(csm);
		return clk;
	}

	pr_debug("%s: parent %s rate %lu\n",
		 __clk_get_name(clk),
		 __clk_get_name(clk_get_parent(clk)),
		 clk_get_rate(clk));

	return clk;
}

static struct clkgen_csm_data st_csm_AB54_127 = {
	.stop_offset = 0x0,
	.slow_offset = 0x4,
	.start_bit = 20
};

static struct clkgen_csm_data st_csm_AB216_127 = {
	.stop_offset = 0x0,
	.slow_offset = 0x4,
	.start_bit = 23
};

static struct clkgen_csm_data st_csm_DE54_127 = {
	.stop_offset = 0x0,
	.slow_offset = 0x4,
	.start_bit = 0
};

static struct clkgen_csm_data st_csm_DE108_127 = {
	.stop_offset = 0x0,
	.slow_offset = 0x4,
	.start_bit = 16
};

static struct of_device_id csm_of_match[] = {
	{
		.compatible = "st,stid127-csm-AB54",
		.data = (void *)&st_csm_AB54_127
	},
	{
		.compatible = "st,stid127-csm-AB216",
		.data = (void *)&st_csm_AB216_127
	},
	{
		.compatible = "st,stid127-csm-DE54",
		.data = (void *)&st_csm_DE54_127
	},
	{
		.compatible = "st,stid127-csm-DE108",
		.data = (void *)&st_csm_DE108_127
	},
	{}
};

static void __init clkgen_csm_setup(struct device_node *np)
{
	const struct of_device_id *match;
	const char *clk_parent_name;
	void __iomem *reg;
	struct clkgen_csm_data *data;
	struct clk_onecell_data *clk_data;
	int csm_chan;

	match = of_match_node(csm_of_match, np);
	if (WARN_ON(!match))
		return;

	data = (struct clkgen_csm_data *)match->data;

	reg = of_iomap(np, 0);
	if (!reg)
		return;

	clk_parent_name = of_clk_get_parent_name(np, 0);
	if (!clk_parent_name)
		return;

	clk_data = kzalloc(sizeof(*clk_data), GFP_KERNEL);
	if (!clk_data)
		return;

	clk_data->clk_num = of_property_count_strings(np, "clock-output-names");
	if (clk_data->clk_num <= 0) {
		kfree(clk_data);
		return;
	}

	clk_data->clks = kzalloc(clk_data->clk_num * sizeof(struct clk *),
				 GFP_KERNEL);

	if (!clk_data->clks) {
		kfree(clk_data);
		return;
	}

	for (csm_chan = 0; csm_chan < clk_data->clk_num; csm_chan++) {
		struct clk *clk;
		const char *clk_name;

		if (of_property_read_string_index(np, "clock-output-names",
						  csm_chan, &clk_name)) {
			break;
		}

		/*
		 * If we read an empty clock name then the channel is unused
		 */
		if (*clk_name == '\0')
			continue;

		clk = clk_register_csm(clk_name, clk_parent_name,
					   reg + data->stop_offset,
					   reg + data->slow_offset,
					   csm_chan, &clkgen_doc_lock);

		/*
		 * If there was an error registering this clock output, clean
		 * up and move on to the next one.
		 */
		if (!IS_ERR(clk)) {
			clk_data->clks[csm_chan] = clk;
			pr_debug("%s: parent %s rate %u\n",
				 __clk_get_name(clk),
				 __clk_get_name(clk_get_parent(clk)),
				 (unsigned int)clk_get_rate(clk));
		}
	}

	of_clk_add_provider(np, of_clk_src_onecell_get, clk_data);
}
CLK_OF_DECLARE(clkgen_csm,
	       "st,clkgen-csm", clkgen_csm_setup);
