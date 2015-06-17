/*
 * clk-flexgen.c
 *
 * Copyright (C) ST-Microelectronics SA 2013
 * Author:  Maxime Coquelin <maxime.coquelin@st.com> for ST-Microelectronics.
 * License terms:  GNU General Public License (GPL), version 2  */

#include <linux/clk-provider.h>
#include "clk-common.h"
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/err.h>
#include <linux/string.h>
#include <linux/of.h>
#include <linux/of_address.h>

#include "clkgen.h"

#define FLEXGEN_MAX_OUTPUTS 48
#define FLEXGEN_MAX_PARENT 4

struct clkgen_data_parent_clk {
	struct clk *clkp;
	bool sync;
};

struct clkgen_data_clk {
	struct clk *clkp;
	bool sync;
};

struct clkgen_data {
	unsigned long clk_flags;
	bool clk_mode;
	struct clkgen_data_parent_clk parent_clks[FLEXGEN_MAX_PARENT];
	struct clkgen_field beatcnt[FLEXGEN_MAX_PARENT];
	struct clkgen_data_clk clks[FLEXGEN_MAX_OUTPUTS];
};

struct flexgen {
	struct clk_hw hw;

	/* Crossbar */
	struct clk_mux mux;
	/* Pre-divisor's gate */
	struct clk_gate pgate;
	/* Pre-divisor */
	struct clk_divider pdiv;
	/* Final divisor's gate */
	struct clk_gate fgate;
	/* Final divisor */
	struct clk_divider fdiv;
	/* Asynchronous mode control */
	struct clk_gate sync;
	/* hw control flags */
	bool control_mode;
	/* Beatcnt divisor */
	struct clk_divider beatdiv[FLEXGEN_MAX_PARENT];
	struct clkgen_data_parent_clk *parents_clks;
	struct clkgen_data_clk *clks;
};

#define to_flexgen(_hw) container_of(_hw, struct flexgen, hw)
#define to_clk_gate(_hw) container_of(_hw, struct clk_gate, hw)

static int flexgen_enable(struct clk_hw *hw)
{
	struct flexgen *flexgen = to_flexgen(hw);
	struct clk_hw *pgate_hw = &flexgen->pgate.hw;
	struct clk_hw *fgate_hw = &flexgen->fgate.hw;

	pgate_hw->clk = hw->clk;
	fgate_hw->clk = hw->clk;

	clk_gate_ops.enable(pgate_hw);

	clk_gate_ops.enable(fgate_hw);

	pr_debug("%s: flexgen output enabled\n", __clk_get_name(hw->clk));
	return 0;
}

static void flexgen_disable(struct clk_hw *hw)
{
	struct flexgen *flexgen = to_flexgen(hw);
	struct clk_hw *fgate_hw = &flexgen->fgate.hw;

	/* disable only the final gate */
	fgate_hw->clk = hw->clk;

	clk_gate_ops.disable(fgate_hw);

	pr_debug("%s: flexgen output disabled\n", __clk_get_name(hw->clk));
}

static int flexgen_is_enabled(struct clk_hw *hw)
{
	struct flexgen *flexgen = to_flexgen(hw);
	struct clk_hw *fgate_hw = &flexgen->fgate.hw;

	fgate_hw->clk = hw->clk;

	if (!clk_gate_ops.is_enabled(fgate_hw))
		return 0;

	return 1;
}

static u8 flexgen_get_parent(struct clk_hw *hw)
{
	struct flexgen *flexgen = to_flexgen(hw);
	struct clk_hw *mux_hw = &flexgen->mux.hw;

	mux_hw->clk = hw->clk;

	return clk_mux_ops.get_parent(mux_hw);
}

static int flexgen_set_parent(struct clk_hw *hw, u8 index)
{
	struct flexgen *flexgen = to_flexgen(hw);
	struct clk_hw *mux_hw = &flexgen->mux.hw;

	mux_hw->clk = hw->clk;

	return clk_mux_ops.set_parent(mux_hw, index);
}

static long flexgen_round_rate(struct clk_hw *hw, unsigned long rate,
				   unsigned long *prate)
{
	unsigned long div;

	/* Round div according to exact prate and wished rate */
	div = clk_best_div(*prate, rate);

	if (__clk_get_flags(hw->clk) & CLK_SET_RATE_PARENT) {
		*prate = rate * div;
		return rate;
	} else {
		return *prate / div;
	}
}

unsigned long flexgen_recalc_rate(struct clk_hw *hw,
		unsigned long parent_rate)
{
	struct flexgen *flexgen = to_flexgen(hw);
	struct clk_hw *pdiv_hw = &flexgen->pdiv.hw;
	struct clk_hw *fdiv_hw = &flexgen->fdiv.hw;
	unsigned long mid_rate;

	pdiv_hw->clk = hw->clk;
	fdiv_hw->clk = hw->clk;

	mid_rate = clk_divider_ops.recalc_rate(pdiv_hw, parent_rate);

	return clk_divider_ops.recalc_rate(fdiv_hw, mid_rate);
}

static bool flexgen_set_max_beatcnt(struct clk_hw *hw, unsigned long rate,
				unsigned long parent_rate)
{
	struct flexgen *flexgen = to_flexgen(hw);
	unsigned long final_div = 0;
	int i, j;
	u8 beatdiv = 0;
	bool beatdiv_update = false;

	final_div = clk_best_div(parent_rate, rate);

	for (i = 0; i < FLEXGEN_MAX_PARENT; i++)
		if (flexgen->parents_clks[i].sync &&
		    __clk_get_parent(hw->clk) ==
		    flexgen->parents_clks[i].clkp) {
			u8 beatdiv_max = 0;
			struct clk_hw *beatdiv_hw = &flexgen->beatdiv[i].hw;

			beatdiv = parent_rate /
				  clk_divider_ops.recalc_rate(beatdiv_hw,
							      parent_rate);

			if (final_div > beatdiv) {
				clk_divider_ops.set_rate(beatdiv_hw, rate,
							 rate * final_div);
				beatdiv_update = true;
				break;
			}

			if (final_div == beatdiv)
				break;

			/* if (final_div < beatdiv) */
			beatdiv_max = final_div;
			for (j = 0; j < FLEXGEN_MAX_OUTPUTS; j++) {
				struct clk *clkp = flexgen->clks[j].clkp;

				if (hw->clk == clkp)
					continue;

				if (flexgen->clks[j].sync &&
				    __clk_get_parent(hw->clk) ==
				    __clk_get_parent(clkp)) {
					u8 div = parent_rate /
						 flexgen_recalc_rate
						 (__clk_get_hw(clkp),
						  parent_rate);

					if (div > beatdiv_max)
						beatdiv_max = div;
				}
			}

			if (beatdiv_max != beatdiv) {
				clk_divider_ops.set_rate(beatdiv_hw, rate,
							 rate * beatdiv_max);
				beatdiv_update = true;
			}
			break;
		}

	return beatdiv_update;
}

static int flexgen_set_rate(struct clk_hw *hw, unsigned long rate,
				unsigned long parent_rate)
{
	struct flexgen *flexgen = to_flexgen(hw);
	struct clk_hw *pdiv_hw = &flexgen->pdiv.hw;
	struct clk_hw *fdiv_hw = &flexgen->fdiv.hw;
	struct clk_hw *sync_hw = &flexgen->sync.hw;
	struct clk_gate *config = to_clk_gate(sync_hw);
	bool control = flexgen->control_mode;
	unsigned long final_div = 0;
	int ret = 0, i;
	bool beatdiv_update = false;
	u32 reg;

	if (rate == flexgen_recalc_rate(hw, parent_rate))
		return ret;

	pdiv_hw->clk = hw->clk;
	fdiv_hw->clk = hw->clk;

	final_div = clk_best_div(parent_rate, rate);

	if (control) {
		reg = readl(config->reg);
		reg &= ~BIT(config->bit_idx);
		reg |= !control << config->bit_idx;
		writel(reg, config->reg);

		beatdiv_update = flexgen_set_max_beatcnt(hw, rate, parent_rate);
	}

	/*
	 * Prediv is mainly targeted for low freq results, while final_div is
	 * the first to use for divs < 64.
	 * The other way could lead to 'duty cycle' issues.
	 */
	clk_divider_ops.set_rate(pdiv_hw, parent_rate, parent_rate);
	ret = clk_divider_ops.set_rate(fdiv_hw, rate, rate * final_div);

	if (!control)
		return ret;

	if (beatdiv_update) {
		for (i = 0; i < FLEXGEN_MAX_OUTPUTS; i++) {
			struct clk *clkp = flexgen->clks[i].clkp;

			if (flexgen->clks[i].sync &&
			    (__clk_get_parent(hw->clk) ==
			    __clk_get_parent(clkp)) &&
			    flexgen_is_enabled(__clk_get_hw(clkp))) {
					flexgen_disable(__clk_get_hw(clkp));
					flexgen_enable(__clk_get_hw(clkp));
			}
		}
	} else {
		if (flexgen_is_enabled(hw)) {
				flexgen_disable(hw);
				flexgen_enable(hw);
		}
	}

	return ret;
}

static const struct clk_ops flexgen_ops = {
	.enable = flexgen_enable,
	.disable = flexgen_disable,
	.is_enabled = flexgen_is_enabled,
	.get_parent = flexgen_get_parent,
	.set_parent = flexgen_set_parent,
	.round_rate = flexgen_round_rate,
	.recalc_rate = flexgen_recalc_rate,
	.set_rate = flexgen_set_rate,
};

struct clk *clk_register_flexgen(const char *name,
				const char **parent_names, u8 num_parents,
				void __iomem *reg, spinlock_t *lock, u32 idx,
				unsigned long flexgen_flags,
				struct clkgen_data *data) {
	struct flexgen *fgxbar;
	struct clk *clk;
	struct clk_init_data init;
	u32  xbar_shift, i;
	void __iomem *xbar_reg, *fdiv_reg;

	fgxbar = kzalloc(sizeof(struct flexgen), GFP_KERNEL);
	if (!fgxbar)
		return ERR_PTR(-ENOMEM);

	init.name = name;
	init.ops = &flexgen_ops;
	init.flags = CLK_IS_BASIC | flexgen_flags;
	init.parent_names = parent_names;
	init.num_parents = num_parents;

	xbar_reg = reg + 0x18 + (idx & ~0x3);
	xbar_shift = (idx % 4) * 0x8;
	fdiv_reg = reg + 0x164 + idx * 4;

	/* Crossbar element config */
	fgxbar->mux.lock = lock;
	fgxbar->mux.mask = BIT(6) - 1;
	fgxbar->mux.reg = xbar_reg;
	fgxbar->mux.shift = xbar_shift;
	fgxbar->mux.table = NULL;


	/* Pre-divider's gate config (in xbar register)*/
	fgxbar->pgate.lock = lock;
	fgxbar->pgate.reg = xbar_reg;
	fgxbar->pgate.bit_idx = xbar_shift + 6;

	/* Pre-divider config */
	fgxbar->pdiv.lock = lock;
	fgxbar->pdiv.reg = reg + 0x58 + idx * 4;
	fgxbar->pdiv.width = 10;

	/* Final divider's gate config */
	fgxbar->fgate.lock = lock;
	fgxbar->fgate.reg = fdiv_reg;
	fgxbar->fgate.bit_idx = 6;

	/* Final divider config */
	fgxbar->fdiv.lock = lock;
	fgxbar->fdiv.reg = fdiv_reg;
	fgxbar->fdiv.width = 6;

	/* Final divider sync config */
	fgxbar->sync.lock = lock;
	fgxbar->sync.reg = fdiv_reg;
	fgxbar->sync.bit_idx = 7;

	fgxbar->control_mode = (data) ? data->clks[idx].sync : false;

	if (fgxbar->control_mode) {
		/* beatcnt divider config */
		for (i = 0; i < FLEXGEN_MAX_PARENT; i++) {
			fgxbar->beatdiv[i].lock = lock;
			fgxbar->beatdiv[i].reg = reg + data->beatcnt[i].offset;
			fgxbar->beatdiv[i].width = 8;
			fgxbar->beatdiv[i].shift = 8 * i;
		}
		fgxbar->parents_clks = data->parent_clks;
		fgxbar->clks = data->clks;
	}

	fgxbar->hw.init = &init;

	clk = clk_register(NULL, &fgxbar->hw);
	if (IS_ERR(clk))
		kfree(fgxbar);
	else
		pr_debug("%s: parent %s rate %u\n",  __clk_get_name(clk),
			 __clk_get_name(clk_get_parent(clk)),
			 (unsigned int)clk_get_rate(clk));
	return clk;
}

static const char ** __init flexgen_get_parents(struct device_node *np,
						       int *num_parents)
{
	const char **parents;
	int nparents, i;

	nparents = of_count_phandle_with_args(np, "clocks", "#clock-cells");
	if (WARN_ON(nparents <= 0))
		return NULL;

	parents = kzalloc(nparents * sizeof(const char *), GFP_KERNEL);
	if (!parents)
		return NULL;

	for (i = 0; i < nparents; i++)
		parents[i] = of_clk_get_parent_name(np, i);

	*num_parents = nparents;
	return parents;
}

static struct clkgen_data clkgen_d0 = {
	.clk_flags = CLK_SET_RATE_PARENT,
	.clk_mode = 0,
};

static struct clkgen_data clkgen_d2 = {
	.clk_flags = 0,
	.clk_mode = 1,
	.parent_clks = { {NULL, 1},
			 {NULL, 1},
			 {NULL, 0},
			 {NULL, 0}, },
	.beatcnt = { CLKGEN_FIELD(0x26c, 0xff, 0),
		     CLKGEN_FIELD(0x26c, 0xff, 8),
		     CLKGEN_FIELD(0x26c, 0xff, 16),
		     CLKGEN_FIELD(0x26c, 0xff, 24) },
	.clks = { {NULL, 1},
		  {NULL, 1},
		  {NULL, 1},
		  {NULL, 1},
		  {NULL, 1},
		  {NULL, 1},
		  {NULL, 1},
		  {NULL, 1},
		  {NULL, 1},
		  {NULL, 0},
		  {NULL, 0},
		  {NULL, 1},
		  {NULL, 0},
		  {NULL, 1},
		  {NULL, 0},
		  {NULL, 0}, },
};

static struct of_device_id flexgen_of_match[] = {
	{
		.compatible = "st,stih407-clkgend0",
		.data = &clkgen_d0,
	},
	{
		.compatible = "st,stih407-clkgend2",
		.data = &clkgen_d2,
	},
	{}
};

struct clkgen_flex_conf {
	unsigned long flex_flags[FLEXGEN_MAX_OUTPUTS];
};

static struct clkgen_flex_conf st_flexgen_conf_407_c0 = {
	.flex_flags = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
			0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
			0, 0, 0, 0, 0, 0, 0, 0, 0, CLK_IGNORE_UNUSED,
			CLK_IGNORE_UNUSED, CLK_IGNORE_UNUSED },
};

static struct clkgen_flex_conf st_flexgen_conf_407_d2 = {
	.flex_flags = { CLK_IGNORE_UNUSED, CLK_IGNORE_UNUSED, CLK_IGNORE_UNUSED,
			CLK_IGNORE_UNUSED, CLK_IGNORE_UNUSED, CLK_IGNORE_UNUSED,
			CLK_IGNORE_UNUSED, CLK_IGNORE_UNUSED, CLK_IGNORE_UNUSED,
			CLK_IGNORE_UNUSED, CLK_IGNORE_UNUSED, CLK_IGNORE_UNUSED,
			CLK_IGNORE_UNUSED, CLK_IGNORE_UNUSED, CLK_IGNORE_UNUSED,
			CLK_IGNORE_UNUSED },
};

static struct of_device_id flexconf_of_match[] = {
	{
		.compatible = "st,stih407-flexgen-conf-c0",
		.data = &st_flexgen_conf_407_c0,
	},
	{
		.compatible = "st,stih407-flexgen-conf-d2",
		.data = &st_flexgen_conf_407_d2,
	},
	{}
};

void __init st_of_flexgen_setup(struct device_node *np)
{
	struct device_node *pnode;
	void __iomem *reg;
	struct clk_onecell_data *clk_data = NULL;
	const char **parents;
	int num_parents, i;
	spinlock_t *rlock;
	const struct of_device_id *match;
	struct clkgen_data *data = NULL;
	struct clkgen_flex_conf *conf_data = NULL;
	unsigned long clk_flags = 0;

	rlock = kzalloc(sizeof(spinlock_t), GFP_KERNEL);

	pnode = of_get_parent(np);
	if (!pnode)
		return;

	reg = of_iomap(pnode, 0);
	if (!reg)
		return;

	match = of_match_node(flexconf_of_match, np);
	if (match)
		conf_data = (struct clkgen_flex_conf *)match->data;

	parents = flexgen_get_parents(np, &num_parents);
	if (!parents)
		return;

	match = of_match_node(flexgen_of_match, np);
	if (match) {
		data = (struct clkgen_data *)match->data;
		clk_flags = data->clk_flags;
		if (data->clk_mode)
			/* get parent clks and set beat-cnt to 0*/
			for (i = 0; i < FLEXGEN_MAX_PARENT; i++)
				if (data->parent_clks[i].sync) {
					if (i >= num_parents)
						goto err;
					data->parent_clks[i].clkp =
					__clk_lookup(parents[i]);
					if (IS_ERR(data->parent_clks[i].clkp))
						goto err;
					clkgen_write(reg, &data->beatcnt[i], 0);
				}
	}

	clk_data = kzalloc(sizeof(*clk_data), GFP_KERNEL);
	if (!clk_data)
		goto err;

	clk_data->clk_num = of_property_count_strings(np ,
			"clock-output-names");
	if (clk_data->clk_num <= 0) {
		pr_err("%s: Failed to get number of output clocks (%d)",
		       __func__, clk_data->clk_num);
		goto err;
	}

	clk_data->clks = kzalloc(clk_data->clk_num * sizeof(struct clk *),
			GFP_KERNEL);
	if (!clk_data->clks)
		goto err;

	for (i = 0; i < clk_data->clk_num; i++) {
		struct clk *clk;
		const char *clk_name;
		unsigned long flags = ((conf_data) ?
				       conf_data->flex_flags[i] : 0);

		if (of_property_read_string_index(np, "clock-output-names",
						  i, &clk_name)) {
			break;
		}

		/*
		 * If we read an empty clock name then the output is unused
		 */
		if (*clk_name == '\0')
			continue;

		clk = clk_register_flexgen(clk_name, parents, num_parents,
					   reg, rlock, i, clk_flags | flags,
					   data);

		if (IS_ERR(clk))
			goto err;

		clk_data->clks[i] = clk;
		if (data && data->clks[i].sync)
			data->clks[i].clkp = clk;
	}

	kfree(parents);
	of_clk_add_provider(np, of_clk_src_onecell_get, clk_data);

	return;

err:
	if (clk_data)
		kfree(clk_data->clks);
	kfree(clk_data);
	kfree(parents);
	return;
}
CLK_OF_DECLARE(flexgen, "st,flexgen", st_of_flexgen_setup);
