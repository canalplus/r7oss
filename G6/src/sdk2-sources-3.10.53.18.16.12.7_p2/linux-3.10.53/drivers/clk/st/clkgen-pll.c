/*
 * Copyright (C) 2003-2013 STMicroelectronics (R&D) Limited
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 */

/*
 * Authors:
 * Stephen Gallimore <stephen.gallimore@st.com>,
 * Pankaj Dev <pankaj.dev@st.com>.
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

static DEFINE_SPINLOCK(clkgena_c32_odf_lock);
DEFINE_SPINLOCK(clkgen_a9_lock);

/*
 * Common PLL configuration register bits for PLL800 and PLL1600 C65
 */
#define C65_MDIV_PLL800_MASK	(0xff)
#define C65_MDIV_PLL1600_MASK	(0x7)
#define C65_NDIV_MASK		(0xff)
#define C65_PDIV_MASK		(0x7)

/*
 * PLL configuration register bits for PLL3200 C32
 */
#define C32_NDIV_MASK (0xff)
#define C32_IDF_MASK (0x7)
#define C32_ODF_MASK (0x3f)
#define C32_LDF_MASK (0x7f)
#define C32_CP_MASK (0x1f)

#define C32_MAX_ODFS (4)

/*
 * PLL configuration register bits for PLL1600 C45
 */
#define C45_NDIV_MASK (0xff)
#define C45_IDF_MASK (0x7)
#define C45_ODF_MASK (0x3f)
#define C45_CP_MASK (0x1f)

struct clkgen_pll_data {
	struct clkgen_field pdn_status;
	bool pdn_polarity;
	struct clkgen_field pdn_ctrl;
	struct clkgen_field locked_status;
	struct clkgen_field mdiv;
	struct clkgen_field ndiv;
	struct clkgen_field pdiv;
	struct clkgen_field idf;
	struct clkgen_field ldf;
	struct clkgen_field cp;
	unsigned int num_odfs;
	struct clkgen_field odf[C32_MAX_ODFS];
	struct clkgen_field odf_gate[C32_MAX_ODFS];
	bool endisable;
	struct clkgen_field pre_disable;
	unsigned int pre_disable_val;
	struct clkgen_field post_enable;
	unsigned int post_enable_val;
	/* lock */
	spinlock_t *lock;
	const struct clk_ops *ops;
	unsigned long pll_clkflags, odf_clkflags[C32_MAX_ODFS];
};

static const struct clk_ops st_pll1600c65_ops;
static const struct clk_ops st_pll800c65_ops;
static const struct clk_ops stm_pll3200c32_ops;
static const struct clk_ops st_pll1200c32_ops;
static const struct clk_ops stm_pll1600c45_phi_ops;
static const struct clk_ops stm_pll1600c45_ops;

static struct clkgen_pll_data st_pll1600c65_ax = {
	.pdn_status	= CLKGEN_FIELD(0x0, 0x1,			19),
	.pdn_polarity = 1,
	.pdn_ctrl	= CLKGEN_FIELD(0x10,	0x1,			0),
	.locked_status	= CLKGEN_FIELD(0x0, 0x1,			31),
	.mdiv		= CLKGEN_FIELD(0x0, C65_MDIV_PLL1600_MASK,	0),
	.ndiv		= CLKGEN_FIELD(0x0, C65_NDIV_MASK,		8),
	.ops		= &st_pll1600c65_ops
};

static struct clkgen_pll_data st_pll800c65_ax = {
	.pdn_status	= CLKGEN_FIELD(0x0,	0x1,			19),
	.pdn_polarity = 1,
	.pdn_ctrl	= CLKGEN_FIELD(0xC,	0x1,			1),
	.locked_status	= CLKGEN_FIELD(0x0,	0x1,			31),
	.mdiv		= CLKGEN_FIELD(0x0,	C65_MDIV_PLL800_MASK,	0),
	.ndiv		= CLKGEN_FIELD(0x0,	C65_NDIV_MASK,		8),
	.pdiv		= CLKGEN_FIELD(0x0,	C65_PDIV_MASK,		16),
	.ops		= &st_pll800c65_ops
};

static struct clkgen_pll_data st_pll3200c32_a1x_0 = {
	.pdn_status	= CLKGEN_FIELD(0x0,	0x1,			31),
	.pdn_polarity = 1,
	.pdn_ctrl	= CLKGEN_FIELD(0x18,	0x1,			0),
	.locked_status	= CLKGEN_FIELD(0x4,	0x1,			31),
	.ndiv		= CLKGEN_FIELD(0x0,	C32_NDIV_MASK,		0x0),
	.idf		= CLKGEN_FIELD(0x4,	C32_IDF_MASK,		0x0),
	.num_odfs = 4,
	.odf =	{	CLKGEN_FIELD(0x54,	C32_ODF_MASK,		4),
			CLKGEN_FIELD(0x54,	C32_ODF_MASK,		10),
			CLKGEN_FIELD(0x54,	C32_ODF_MASK,		16),
			CLKGEN_FIELD(0x54,	C32_ODF_MASK,		22) },
	.odf_gate = {	CLKGEN_FIELD(0x54,	0x1,			0),
			CLKGEN_FIELD(0x54,	0x1,			1),
			CLKGEN_FIELD(0x54,	0x1,			2),
			CLKGEN_FIELD(0x54,	0x1,			3) },
	.ops		= &stm_pll3200c32_ops,
};

static struct clkgen_pll_data st_pll3200c32_a1x_1 = {
	.pdn_status	= CLKGEN_FIELD(0xC,	0x1,			31),
	.pdn_polarity = 1,
	.pdn_ctrl	= CLKGEN_FIELD(0x18,	0x1,			1),
	.locked_status	= CLKGEN_FIELD(0x10,	0x1,			31),
	.ndiv		= CLKGEN_FIELD(0xC,	C32_NDIV_MASK,		0x0),
	.idf		= CLKGEN_FIELD(0x10,	C32_IDF_MASK,		0x0),
	.num_odfs = 4,
	.odf = {	CLKGEN_FIELD(0x58,	C32_ODF_MASK,		4),
			CLKGEN_FIELD(0x58,	C32_ODF_MASK,		10),
			CLKGEN_FIELD(0x58,	C32_ODF_MASK,		16),
			CLKGEN_FIELD(0x58,	C32_ODF_MASK,		22) },
	.odf_gate = {	CLKGEN_FIELD(0x58,	0x1,			0),
			CLKGEN_FIELD(0x58,	0x1,			1),
			CLKGEN_FIELD(0x58,	0x1,			2),
			CLKGEN_FIELD(0x58,	0x1,			3) },
	.ops		= &stm_pll3200c32_ops,
};

static struct clkgen_pll_data st_pll1600c45_ax_0 = {
	.pdn_status	= CLKGEN_FIELD(0x0, 0x1,			31),
	.pdn_polarity = 1,
	.pdn_ctrl	= CLKGEN_FIELD(0x18,	0x1,			0),
	.locked_status	= CLKGEN_FIELD(0x4, 0x1,			31),
	.ndiv		= CLKGEN_FIELD(0x0, C45_NDIV_MASK,		0),
	.idf		= CLKGEN_FIELD(0x4,	C45_IDF_MASK,		0x0),
	.ops		= &stm_pll1600c45_ops
};

static struct clkgen_pll_data st_pll1600c45_ax_1 = {
	.pdn_status	= CLKGEN_FIELD(0xC, 0x1,			31),
	.pdn_polarity = 1,
	.pdn_ctrl	= CLKGEN_FIELD(0x18,	0x1,			1),
	.locked_status	= CLKGEN_FIELD(0x10, 0x1,			31),
	.ndiv		= CLKGEN_FIELD(0xC, C45_NDIV_MASK,		0),
	.idf		= CLKGEN_FIELD(0x10,	C45_IDF_MASK,		0x0),
	.ops		= &stm_pll1600c45_ops
};

/* 415 specific */
static struct clkgen_pll_data st_pll3200c32_a9_415 = {
	.pdn_status	= CLKGEN_FIELD(0x0,	0x1,			0),
	.pdn_polarity = 1,
	.pdn_ctrl	= CLKGEN_FIELD(0x0,	0x1,			0),
	.locked_status	= CLKGEN_FIELD(0x6C,	0x1,			0),
	.ndiv		= CLKGEN_FIELD(0x0,	C32_NDIV_MASK,		9),
	.idf		= CLKGEN_FIELD(0x0,	C32_IDF_MASK,		22),
	.num_odfs = 1,
	.odf =		{ CLKGEN_FIELD(0x0,	C32_ODF_MASK,		3) },
	.odf_gate =	{ CLKGEN_FIELD(0x0,	0x1,			28) },
	.ops		= &stm_pll3200c32_ops,
};

static struct clkgen_pll_data st_pll3200c32_ddr_415 = {
	.pdn_status	= CLKGEN_FIELD(0x0,	0x1,			0),
	.pdn_polarity = 1,
	.pdn_ctrl	= CLKGEN_FIELD(0x0,	0x1,			0),
	.locked_status	= CLKGEN_FIELD(0x100,	0x1,			0),
	.ndiv		= CLKGEN_FIELD(0x8,	C32_NDIV_MASK,		0),
	.idf		= CLKGEN_FIELD(0x0,	C32_IDF_MASK,		25),
	.num_odfs = 2,
	.odf		= { CLKGEN_FIELD(0x8,	C32_ODF_MASK,		8),
			    CLKGEN_FIELD(0x8,	C32_ODF_MASK,		14) },
	.odf_gate	= { CLKGEN_FIELD(0x4,	0x1,			28),
			    CLKGEN_FIELD(0x4,	0x1,			29) },
	.ops		= &stm_pll3200c32_ops,
};

static struct clkgen_pll_data st_pll1200c32_gpu_415 = {
	.pdn_status	= CLKGEN_FIELD(0x4,	0x1,			0),
	.pdn_polarity = 0,
	.pdn_ctrl	= CLKGEN_FIELD(0x4,	0x1,			0),
	.locked_status	= CLKGEN_FIELD(0x168,	0x1,			0),
	.ldf		= CLKGEN_FIELD(0x0,	C32_LDF_MASK,		3),
	.idf		= CLKGEN_FIELD(0x0,	C32_IDF_MASK,		0),
	.num_odfs = 0,
	.odf		= { CLKGEN_FIELD(0x0,	C32_ODF_MASK,		10) },
	.ops		= &st_pll1200c32_ops,
};

/* 416 specific */
static struct clkgen_pll_data st_pll3200c32_a9_416 = {
	.pdn_status	= CLKGEN_FIELD(0x0,	0x1,			0),
	.pdn_polarity = 1,
	.pdn_ctrl	= CLKGEN_FIELD(0x0,	0x1,			0),
	.locked_status	= CLKGEN_FIELD(0x6C,	0x1,			0),
	.ndiv		= CLKGEN_FIELD(0x8,	C32_NDIV_MASK,		0),
	.idf		= CLKGEN_FIELD(0x0,	C32_IDF_MASK,		25),
	.num_odfs = 1,
	.odf		= { CLKGEN_FIELD(0x8,	C32_ODF_MASK,		8) },
	.odf_gate	= { CLKGEN_FIELD(0x4,	0x1,			28) },
	.ops		= &stm_pll3200c32_ops,
};

static struct clkgen_pll_data st_pll3200c32_ddr_416 = {
	.pdn_status	= CLKGEN_FIELD(0x0,	0x1,			0),
	.pdn_polarity = 1,
	.pdn_ctrl	= CLKGEN_FIELD(0x0,	0x1,			0),
	.locked_status	= CLKGEN_FIELD(0x10C,	0x1,			0),
	.ndiv		= CLKGEN_FIELD(0x8,	C32_NDIV_MASK,		0),
	.idf		= CLKGEN_FIELD(0x0,	C32_IDF_MASK,		25),
	.num_odfs = 2,
	.odf		= { CLKGEN_FIELD(0x8,	C32_ODF_MASK,		8),
			    CLKGEN_FIELD(0x8,	C32_ODF_MASK,		14) },
	.odf_gate	= { CLKGEN_FIELD(0x4,	0x1,			28),
			    CLKGEN_FIELD(0x4,	0x1,			29) },
	.ops		= &stm_pll3200c32_ops,
};

static struct clkgen_pll_data st_pll1200c32_gpu_416 = {
	.pdn_status	= CLKGEN_FIELD(0x8E4,	0x1,			3),
	.pdn_polarity = 0,
	.pdn_ctrl	= CLKGEN_FIELD(0x8E4,	0x1,			3),
	.locked_status	= CLKGEN_FIELD(0x90C,	0x1,			0),
	.ldf		= CLKGEN_FIELD(0x0,	C32_LDF_MASK,		3),
	.idf		= CLKGEN_FIELD(0x0,	C32_IDF_MASK,		0),
	.num_odfs = 0,
	.odf		= { CLKGEN_FIELD(0x0,	C32_ODF_MASK,		10) },
	.ops		= &st_pll1200c32_ops,
};

static struct clkgen_pll_data st_pll3200c32_407_a0 = {
	/* 407 A0 */
	.pdn_status	= CLKGEN_FIELD(0x2a0,	0x1,			8),
	.pdn_polarity = 1,
	.pdn_ctrl	= CLKGEN_FIELD(0x2a0,	0x1,			8),
	.locked_status	= CLKGEN_FIELD(0x2a0,	0x1,			24),
	.ndiv		= CLKGEN_FIELD(0x2a4,	C32_NDIV_MASK,		16),
	.idf		= CLKGEN_FIELD(0x2a4,	C32_IDF_MASK,		0x0),
	.num_odfs = 1,
	.odf		= { CLKGEN_FIELD(0x2b4, C32_ODF_MASK,		0) },
	.odf_gate	= { CLKGEN_FIELD(0x2b4,	0x1,			6) },
	.ops		= &stm_pll3200c32_ops,
};

static struct clkgen_pll_data st_pll3200c32_cx_0 = {
	/* C0 PLL0 */
	.pdn_status	= CLKGEN_FIELD(0x2a0,	0x1,			8),
	.pdn_polarity = 1,
	.pdn_ctrl	= CLKGEN_FIELD(0x2a0,	0x1,			8),
	.locked_status	= CLKGEN_FIELD(0x2a0,	0x1,			24),
	.ndiv		= CLKGEN_FIELD(0x2a4,	C32_NDIV_MASK,		16),
	.idf		= CLKGEN_FIELD(0x2a4,	C32_IDF_MASK,		0x0),
	.num_odfs = 1,
	.odf		= { CLKGEN_FIELD(0x2b4, C32_ODF_MASK,		0) },
	.odf_gate	= { CLKGEN_FIELD(0x2b4, 0x1,			6) },
	.ops		= &stm_pll3200c32_ops,
};

static struct clkgen_pll_data st_pll3200c32_cx_1 = {
	/* C0 PLL1 */
	.pdn_status	= CLKGEN_FIELD(0x2c8,	0x1,			8),
	.pdn_polarity = 1,
	.pdn_ctrl	= CLKGEN_FIELD(0x2c8,	0x1,			8),
	.locked_status	= CLKGEN_FIELD(0x2c8,	0x1,			24),
	.ndiv		= CLKGEN_FIELD(0x2cc,	C32_NDIV_MASK,		16),
	.idf		= CLKGEN_FIELD(0x2cc,	C32_IDF_MASK,		0x0),
	.num_odfs = 1,
	.odf		= { CLKGEN_FIELD(0x2dc, C32_ODF_MASK,		0) },
	.odf_gate	= { CLKGEN_FIELD(0x2dc, 0x1,			6) },
	.ops		= &stm_pll3200c32_ops,
};

static struct clkgen_pll_data st_pll3200c32_407_a9 = {
	/* 407 A9 */
	.pdn_status	= CLKGEN_FIELD(0x1a8,	0x1,			0),
	.pdn_polarity = 1,
	.pdn_ctrl	= CLKGEN_FIELD(0x1a8,	0x1,			0),
	.locked_status	= CLKGEN_FIELD(0x87c,	0x1,			0),
	.ndiv		= CLKGEN_FIELD(0x1b0,	C32_NDIV_MASK,		0),
	.idf		= CLKGEN_FIELD(0x1a8,	C32_IDF_MASK,		25),
	.cp		= CLKGEN_FIELD(0x1a8,	C32_CP_MASK,		1),
	.num_odfs = 1,
	.odf		= { CLKGEN_FIELD(0x1b0, C32_ODF_MASK,		8) },
	.odf_gate	= { CLKGEN_FIELD(0x1ac, 0x1,			28) },
	.endisable = true,
	.pre_disable = CLKGEN_FIELD(0x1a4,	0x1,			1),
	.pre_disable_val = 1,
	.post_enable = CLKGEN_FIELD(0x1a4,	0x1,			1),
	.post_enable_val = 0,
	.lock = &clkgen_a9_lock,
	.ops		= &stm_pll3200c32_ops,
};

/* 127 specific */
static struct clkgen_pll_data st_pll1600c45_a9_127 = {
	.pdn_status	= CLKGEN_FIELD(0x0,	0x1,			0),
	.pdn_polarity = 1,
	.pdn_ctrl	= CLKGEN_FIELD(0x0,	0x1,			0),
	.locked_status	= CLKGEN_FIELD(0x98,	0x1,			0),
	.ndiv		= CLKGEN_FIELD(0x0,	C45_NDIV_MASK,		9),
	.idf		= CLKGEN_FIELD(0x0,	C45_IDF_MASK,		22),
	.cp		= CLKGEN_FIELD(0x0,	C45_CP_MASK,		17),
	.num_odfs = 0,
	.odf =		{ CLKGEN_FIELD(0x0,	C45_ODF_MASK,		3) },
	.endisable = true,
	.pre_disable = CLKGEN_FIELD(0x0,	0x1,			2),
	.pre_disable_val = 1,
	.post_enable = CLKGEN_FIELD(0x0,	0x1,			2),
	.post_enable_val = 0,
	.lock = &clkgen_a9_lock,
	.ops		= &stm_pll1600c45_phi_ops,
};

static struct clkgen_pll_data st_pll3200c32_bx_8416 = {
	/* 8416 B0 PLL */
	.pdn_status	= CLKGEN_FIELD(0x2a0,	0x1,			8),
	.pdn_polarity = 1,
	.pdn_ctrl	= CLKGEN_FIELD(0x2a0,	0x1,			8),
	.locked_status	= CLKGEN_FIELD(0x2a0,	0x1,			24),
	.ndiv		= CLKGEN_FIELD(0x2a4,	C32_NDIV_MASK,		16),
	.idf		= CLKGEN_FIELD(0x2a4,	C32_IDF_MASK,		0),
	.num_odfs = 1,
	.odf		= { CLKGEN_FIELD(0x2b4, C32_ODF_MASK,		0) },
	.odf_gate	= { CLKGEN_FIELD(0x2b4, 0x1,			6) },
	.ops		= &stm_pll3200c32_ops,
};

struct clkgen_pll_conf {
	unsigned long pll_clkflags, odf_clkflags[C32_MAX_ODFS];
};

static struct clkgen_pll_conf st_pll3200c32_conf_407_c0_1 = {
	/* 407 C0 PLL1 */
	.pll_clkflags = CLK_IGNORE_UNUSED,
	.odf_clkflags = { CLK_IGNORE_UNUSED },
};

/**
 * DOC: Clock Generated by PLL, rate set and enabled by bootloader
 *
 * Traits of this clock:
 * prepare - clk_(un)prepare only ensures parent is (un)prepared
 * enable - clk_enable/disable only ensures parent is enabled
 * rate - rate is fixed. No clk_set_rate support
 * parent - fixed parent.  No clk_set_parent support
 */

/**
 * PLL clock that is integrated in the ClockGenA instances on the STiH415
 * and STiH416.
 *
 * @hw: handle between common and hardware-specific interfaces.
 *
 * @type: PLL instance type.
 *
 * @regs_base: base of the PLL configuration register(s).
 *
 */
struct clkgen_pll {
	struct clk_hw		hw;
	struct clkgen_pll_data	*data;
	void __iomem		*regs_base;
	/* lock */
	spinlock_t	*lock;

	u32 ndiv;
	u32 idf;
	u32 odf;
	u32 cp;
};

#define to_clkgen_pll(_hw) container_of(_hw, struct clkgen_pll, hw)

static int clkgen_pll_is_locked(struct clk_hw *hw)
{
	struct clkgen_pll *pll = to_clkgen_pll(hw);
	u32 locked = CLKGEN_READ(pll, locked_status);

	return !!locked;
}

static int clkgen_pll_is_enabled(struct clk_hw *hw)
{
	struct clkgen_pll *pll = to_clkgen_pll(hw);
	u32 poweroff = CLKGEN_READ(pll, pdn_status);
	return (pll->data->pdn_polarity) ? !poweroff : !!poweroff;
}

static int clkgen_pll_enable(struct clk_hw *hw)
{
	struct clkgen_pll *pll = to_clkgen_pll(hw);
	unsigned long flags = 0, timeout;

	if (clkgen_pll_is_enabled(hw))
		return 0;

	if (pll->lock)
		spin_lock_irqsave(pll->lock, flags);

	CLKGEN_WRITE(pll, pdn_ctrl, !pll->data->pdn_polarity);

	timeout = jiffies + msecs_to_jiffies(10);

	while (!clkgen_pll_is_locked(hw)) {
		if (time_after(jiffies, timeout))
			return -ETIMEDOUT;
		cpu_relax();
	}

	if (pll->data->endisable)
		CLKGEN_WRITE(pll, post_enable, pll->data->post_enable_val);

	if (pll->lock)
		spin_unlock_irqrestore(pll->lock, flags);

	pr_debug("%s:%s enabled\n", __clk_get_name(hw->clk), __func__);

	return 0;
}

void clkgen_pll_disable(struct clk_hw *hw)
{
	struct clkgen_pll *pll = to_clkgen_pll(hw);
	unsigned long flags = 0;

	if (!clkgen_pll_is_enabled(hw))
		return;

	if (pll->lock)
		spin_lock_irqsave(pll->lock, flags);

	if (pll->data->endisable)
		CLKGEN_WRITE(pll, pre_disable, pll->data->pre_disable_val);

	CLKGEN_WRITE(pll, pdn_ctrl, pll->data->pdn_polarity);

	if (pll->lock)
		spin_unlock_irqrestore(pll->lock, flags);

	pr_debug("%s:%s disabled\n", __clk_get_name(hw->clk), __func__);
}

unsigned long recalc_stm_pll800c65(struct clk_hw *hw,
		unsigned long parent_rate)
{
	struct clkgen_pll *pll = to_clkgen_pll(hw);
	struct stm_pll params;
	unsigned long rate;

	if (!clkgen_pll_is_enabled(hw) || !clkgen_pll_is_locked(hw))
		return 0;

	params.pdiv = CLKGEN_READ(pll, pdiv);
	params.mdiv = CLKGEN_READ(pll, mdiv);
	params.ndiv = CLKGEN_READ(pll, ndiv);

	clk_pll800c65_get_rate(parent_rate, &params, &rate);

	pr_debug("%s:%s rate %lu\n", __clk_get_name(hw->clk), __func__, rate);

	return rate;
}

unsigned long recalc_stm_pll1600c65(struct clk_hw *hw,
		unsigned long parent_rate)
{
	struct clkgen_pll *pll = to_clkgen_pll(hw);
	struct stm_pll params;
	unsigned long rate;

	if (!clkgen_pll_is_enabled(hw) || !clkgen_pll_is_locked(hw))
		return 0;

	params.mdiv = CLKGEN_READ(pll, mdiv);
	params.ndiv = CLKGEN_READ(pll, ndiv);

	clk_pll1600c65_get_rate(parent_rate, &params, &rate);

	pr_debug("%s:%s rate %lu\n", __clk_get_name(hw->clk), __func__, rate);

	return rate;
}

unsigned long recalc_stm_pll3200c32(struct clk_hw *hw,
		unsigned long parent_rate)
{
	struct clkgen_pll *pll = to_clkgen_pll(hw);
	struct stm_pll params;
	unsigned long rate;

	if (!clkgen_pll_is_enabled(hw) || !clkgen_pll_is_locked(hw))
		return 0;

	params.ndiv = CLKGEN_READ(pll, ndiv);
	params.idf = CLKGEN_READ(pll, idf);

	clk_pll3200c32_get_rate(parent_rate, &params, &rate);

	pr_debug("%s:%s rate %lu\n", __clk_get_name(hw->clk), __func__, rate);

	return rate;
}

static long round_rate_stm_pll3200c32(struct clk_hw *hw, unsigned long rate,
		unsigned long *prate)
{
	struct stm_pll params;

	if (!clk_pll3200c32_get_params(*prate, rate, &params)) {
		clk_pll3200c32_get_rate(*prate, &params, &rate);
	} else {
		pr_debug("%s: %s rate %ld Invalid\n", __func__,
			 __clk_get_name(hw->clk), rate);
		return 0;
	}

	pr_debug("%s: %s new rate %ld [ndiv=%u] [idf=%u]\n",
		 __func__, __clk_get_name(hw->clk),
		 rate, (unsigned int)params.ndiv,
		 (unsigned int)params.idf);

	return rate;
}

static int set_rate_stm_pll3200c32(struct clk_hw *hw, unsigned long rate,
				unsigned long parent_rate)
{
	struct clkgen_pll *pll = to_clkgen_pll(hw);
	struct stm_pll params;
	long hwrate;
	unsigned long flags = 0;

	if (!rate || !parent_rate)
		return -EINVAL;

	if (!clk_pll3200c32_get_params(parent_rate, rate, &params)) {
		clk_pll3200c32_get_rate(parent_rate, &params, &hwrate);
	} else {
		pr_debug("%s: %s rate %ld Invalid\n", __func__,
			 __clk_get_name(hw->clk), rate);
		return -EINVAL;
	}

	pr_debug("%s: %s new rate %ld [ndiv=0x%x] [idf=0x%x]\n",
		 __func__, __clk_get_name(hw->clk),
		 hwrate, (unsigned int)params.ndiv,
		 (unsigned int)params.idf);

	if (!hwrate)
		return -EINVAL;

	pll->ndiv = params.ndiv;
	pll->idf = params.idf;
	pll->cp = params.cp;

	clkgen_pll_disable(hw);

	if (pll->lock)
		spin_lock_irqsave(pll->lock, flags);

	CLKGEN_WRITE(pll, ndiv, pll->ndiv);
	CLKGEN_WRITE(pll, idf, pll->idf);
	CLKGEN_WRITE(pll, cp, pll->cp);

	if (pll->lock)
		spin_unlock_irqrestore(pll->lock, flags);

	clkgen_pll_enable(hw);

	return 0;
}

unsigned long recalc_stm_pll1200c32(struct clk_hw *hw,
		unsigned long parent_rate)
{
	struct clkgen_pll *pll = to_clkgen_pll(hw);
	struct stm_pll params;
	unsigned long rate;

	if (!clkgen_pll_is_enabled(hw) || !clkgen_pll_is_locked(hw))
		return 0;

	params.odf = CLKGEN_READ(pll, odf[0]);
	params.ldf = CLKGEN_READ(pll, ldf);
	params.idf = CLKGEN_READ(pll, idf);

	clk_pll1200c32_get_rate(parent_rate, &params, &rate);

	pr_debug("%s:%s rate %lu\n", __clk_get_name(hw->clk), __func__, rate);

	return rate;
}

unsigned long recalc_stm_pll1600c45(struct clk_hw *hw,
		unsigned long parent_rate)
{
	struct clkgen_pll *pll = to_clkgen_pll(hw);
	struct stm_pll params;
	unsigned long rate;

	if (!clkgen_pll_is_enabled(hw) || !clkgen_pll_is_locked(hw))
		return 0;

	params.ndiv = CLKGEN_READ(pll, ndiv);
	params.idf = CLKGEN_READ(pll, idf);

	clk_pll1600c45_get_rate(parent_rate, &params, &rate);

	pr_debug("%s:%s rate %lu\n", __clk_get_name(hw->clk), __func__, rate);

	return rate;
}

unsigned long recalc_stm_pll1600c45_phi(struct clk_hw *hw,
		unsigned long parent_rate)
{
	struct clkgen_pll *pll = to_clkgen_pll(hw);
	struct stm_pll params;
	unsigned long rate;

	if (!clkgen_pll_is_enabled(hw) || !clkgen_pll_is_locked(hw))
		return 0;

	params.odf = CLKGEN_READ(pll, odf[0]);
	params.ndiv = CLKGEN_READ(pll, ndiv);
	params.idf = CLKGEN_READ(pll, idf);

	clk_pll1600c45_get_phi_rate(parent_rate, &params, &rate);

	pr_debug("%s:%s rate %lu\n", __clk_get_name(hw->clk), __func__, rate);

	return rate;
}

static long round_rate_stm_pll1600c45_phi(struct clk_hw *hw, unsigned long rate,
		unsigned long *prate)
{
	struct stm_pll params;

	if (!clk_pll1600c45_get_phi_params(*prate, rate, &params)) {
		clk_pll1600c45_get_phi_rate(*prate, &params, &rate);
	} else {
		pr_debug("%s: %s rate %ld Invalid\n", __func__,
			 __clk_get_name(hw->clk), rate);
		return 0;
	}

	pr_debug("%s: %s new rate %ld [ndiv=%u] [idf=%u] [odf=%u]\n",
		 __func__, __clk_get_name(hw->clk),
		 rate, (unsigned int)params.ndiv,
		 (unsigned int)params.idf, (unsigned int)params.odf);

	return rate;
}

static int set_rate_stm_pll1600c45_phi(struct clk_hw *hw, unsigned long rate,
				unsigned long parent_rate)
{
	struct clkgen_pll *pll = to_clkgen_pll(hw);
	struct stm_pll params;
	long hwrate;
	unsigned long flags = 0;

	if (!rate || !parent_rate)
		return -EINVAL;

	if (!clk_pll1600c45_get_phi_params(parent_rate, rate, &params)) {
		clk_pll1600c45_get_phi_rate(parent_rate, &params, &hwrate);
	} else {
		pr_debug("%s: %s rate %ld Invalid\n", __func__,
			 __clk_get_name(hw->clk), rate);
		return -EINVAL;
	}

	pr_debug("%s: %s new rate %ld [ndiv=0x%x] [idf=0x%x] [odf=0x%x]\n",
		 __func__, __clk_get_name(hw->clk),
		 hwrate, (unsigned int)params.ndiv,
		 (unsigned int)params.idf, (unsigned int)params.odf);

	if (!hwrate)
		return -EINVAL;

	pll->ndiv = params.ndiv;
	pll->idf = params.idf;
	pll->odf = params.odf;
	pll->cp = params.cp;

	clkgen_pll_disable(hw);

	if (pll->lock)
		spin_lock_irqsave(pll->lock, flags);

	CLKGEN_WRITE(pll, ndiv, pll->ndiv);
	CLKGEN_WRITE(pll, idf, pll->idf);
	CLKGEN_WRITE(pll, odf[0], pll->odf);
	CLKGEN_WRITE(pll, cp, pll->cp);

	if (pll->lock)
		spin_unlock_irqrestore(pll->lock, flags);

	clkgen_pll_enable(hw);

	return 0;
}

static const struct clk_ops st_pll1600c65_ops = {
	.enable		= clkgen_pll_enable,
	.disable	= clkgen_pll_disable,
	.is_enabled	= clkgen_pll_is_enabled,
	.recalc_rate	= recalc_stm_pll1600c65,
};

static const struct clk_ops st_pll800c65_ops = {
	.enable		= clkgen_pll_enable,
	.disable	= clkgen_pll_disable,
	.is_enabled	= clkgen_pll_is_enabled,
	.recalc_rate	= recalc_stm_pll800c65,
};

static const struct clk_ops stm_pll3200c32_ops = {
	.enable		= clkgen_pll_enable,
	.disable	= clkgen_pll_disable,
	.is_enabled	= clkgen_pll_is_enabled,
	.recalc_rate	= recalc_stm_pll3200c32,
	.round_rate	= round_rate_stm_pll3200c32,
	.set_rate	= set_rate_stm_pll3200c32,
};

static const struct clk_ops st_pll1200c32_ops = {
	.enable		= clkgen_pll_enable,
	.disable	= clkgen_pll_disable,
	.is_enabled	= clkgen_pll_is_enabled,
	.recalc_rate	= recalc_stm_pll1200c32,
};

static const struct clk_ops stm_pll1600c45_ops = {
	.enable		= clkgen_pll_enable,
	.disable	= clkgen_pll_disable,
	.is_enabled	= clkgen_pll_is_enabled,
	.recalc_rate	= recalc_stm_pll1600c45,
};

static const struct clk_ops stm_pll1600c45_phi_ops = {
	.enable		= clkgen_pll_enable,
	.disable	= clkgen_pll_disable,
	.is_enabled	= clkgen_pll_is_enabled,
	.recalc_rate	= recalc_stm_pll1600c45_phi,
	.round_rate	= round_rate_stm_pll1600c45_phi,
	.set_rate	= set_rate_stm_pll1600c45_phi,
};

static struct clk * __init clkgen_pll_register(const char *parent_name,
				struct clkgen_pll_data	*pll_data,
				struct clkgen_pll_conf *conf_data,
				void __iomem *reg, const char *clk_name,
				spinlock_t *lock)
{
	struct clkgen_pll *pll;
	struct clk *clk;
	struct clk_init_data init;
	unsigned long flags = ((conf_data) ? conf_data->pll_clkflags : 0);

	pll = kzalloc(sizeof(*pll), GFP_KERNEL);
	if (!pll)
		return ERR_PTR(-ENOMEM);

	init.name  = clk_name;
	init.ops   = pll_data->ops;

	init.flags = CLK_IS_BASIC | flags;
	init.parent_names = &parent_name;
	init.num_parents  = 1;

	pll->data      = pll_data;
	pll->regs_base = reg;
	pll->hw.init   = &init;

	clk = clk_register(NULL, &pll->hw);
	if (IS_ERR(clk)) {
		kfree(pll);
		return clk;
	}

	pr_debug("%s: parent %s rate %lu\n", __clk_get_name(clk),
		 __clk_get_name(clk_get_parent(clk)), clk_get_rate(clk));

	return clk;
}

static struct clk * __init clkgen_lsdiv_register(const char *parent_name,
						     const char *clk_name)
{
	struct clk *clk;

	clk = clk_register_fixed_factor(NULL, clk_name, parent_name, 0, 1, 2);
	if (IS_ERR(clk))
		return clk;

	pr_debug("%s: parent %s rate %lu\n", __clk_get_name(clk),
		 __clk_get_name(clk_get_parent(clk)), clk_get_rate(clk));
	return clk;
}

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

#define CLKGENAx_PLL0_OFFSET 0x0
#define CLKGENAx_PLL1_OFFSET 0x4

static void __init clkgena_c65_pll_setup(struct device_node *np)
{
	const int num_pll_outputs = 3;
	struct clk_onecell_data *clk_data;
	const char *parent_name;
	void __iomem *reg;
	const char *clk_name;

	parent_name = of_clk_get_parent_name(np, 0);
	if (!parent_name)
		return;

	reg = clkgen_get_register_base(np);
	if (!reg)
		return;

	clk_data = kzalloc(sizeof(*clk_data), GFP_KERNEL);
	if (!clk_data)
		return;

	clk_data->clk_num = num_pll_outputs;
	clk_data->clks = kzalloc(clk_data->clk_num * sizeof(struct clk *),
				 GFP_KERNEL);

	if (!clk_data->clks)
		goto err;

	if (of_property_read_string_index(np, "clock-output-names",
					  0, &clk_name))
		goto err;

	/*
	 * PLL0 HS (high speed) output
	 */
	clk_data->clks[0] = clkgen_pll_register(parent_name,
						&st_pll1600c65_ax, NULL,
						reg + CLKGENAx_PLL0_OFFSET,
						clk_name, NULL);

	if (IS_ERR(clk_data->clks[0]))
		goto err;

	if (of_property_read_string_index(np, "clock-output-names",
					  1, &clk_name))
		goto err;

	/*
	 * PLL0 LS (low speed) output, which is a fixed divide by 2 of the
	 * high speed output.
	 */
	clk_data->clks[1] = clkgen_lsdiv_register(__clk_get_name
						      (clk_data->clks[0]),
						      clk_name);

	if (IS_ERR(clk_data->clks[1]))
		goto err;

	if (of_property_read_string_index(np, "clock-output-names",
					  2, &clk_name))
		goto err;

	/*
	 * PLL1 output
	 */
	clk_data->clks[2] = clkgen_pll_register(parent_name,
						&st_pll800c65_ax, NULL,
						reg + CLKGENAx_PLL1_OFFSET,
						clk_name, NULL);

	if (IS_ERR(clk_data->clks[2]))
		goto err;

	of_clk_add_provider(np, of_clk_src_onecell_get, clk_data);
	return;

err:
	kfree(clk_data->clks);
	kfree(clk_data);
}
CLK_OF_DECLARE(clkgena_c65_plls,
	       "st,clkgena-plls-c65", clkgena_c65_pll_setup);

static struct clk * __init clkgen_odf_register(const char *parent_name,
					       void * __iomem reg,
					       struct clkgen_pll_data *pll_data,
					       struct clkgen_pll_conf
					       *conf_data, int odf,
					       spinlock_t *odf_lock,
					       const char *odf_name)
{
	struct clk *clk;
	unsigned long flags;
	struct clk_gate *gate;
	struct clk_divider *div;

	flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT
		| ((conf_data) ? conf_data->odf_clkflags[odf] : 0);

	gate = kzalloc(sizeof(*gate), GFP_KERNEL);
	if (!gate)
		return ERR_PTR(-ENOMEM);

	gate->flags   = CLK_GATE_SET_TO_DISABLE;
	gate->reg     = reg + pll_data->odf_gate[odf].offset;
	gate->bit_idx = pll_data->odf_gate[odf].shift;
	gate->lock    = odf_lock;

	div = kzalloc(sizeof(*div), GFP_KERNEL);
	if (!div)
		return ERR_PTR(-ENOMEM);

	div->flags = CLK_DIVIDER_ONE_BASED | CLK_DIVIDER_ALLOW_ZERO;
	div->reg   = reg + pll_data->odf[odf].offset;
	div->shift = pll_data->odf[odf].shift;
	div->width = fls(pll_data->odf[odf].mask);
	div->lock  = odf_lock;

	clk = clk_register_composite(NULL, odf_name, &parent_name, 1,
				     NULL, NULL,
				     &div->hw, &clk_divider_ops,
				     &gate->hw, &clk_gate_ops,
				     flags);
	if (IS_ERR(clk))
		return clk;

	pr_debug("%s: parent %s rate %lu\n", __clk_get_name(clk),
		 __clk_get_name(clk_get_parent(clk)), clk_get_rate(clk));
	return clk;
}

static struct of_device_id c32_pllconf_of_match[] = {
	{
		.compatible = "st,stih407-plls-conf-c32-c0_1",
		.data = &st_pll3200c32_conf_407_c0_1,
	},
	{}
};

static struct of_device_id c32_pll_of_match[] = {
	{
		.compatible = "st,plls-c32-a1x-0",
		.data = &st_pll3200c32_a1x_0,
	},
	{
		.compatible = "st,plls-c32-a1x-1",
		.data = &st_pll3200c32_a1x_1,
	},
	{
		.compatible = "st,stih415-plls-c32-a9",
		.data = &st_pll3200c32_a9_415,
	},
	{
		.compatible = "st,stih415-plls-c32-ddr",
		.data = &st_pll3200c32_ddr_415,
	},
	{
		.compatible = "st,stih416-plls-c32-a9",
		.data = &st_pll3200c32_a9_416,
	},
	{
		.compatible = "st,stih416-plls-c32-ddr",
		.data = &st_pll3200c32_ddr_416,
	},
	{
		.compatible = "st,stih407-plls-c32-a0",
		.data = &st_pll3200c32_407_a0,
	},
	{
		.compatible = "st,plls-c32-cx_0",
		.data = &st_pll3200c32_cx_0,
	},
	{
		.compatible = "st,plls-c32-cx_1",
		.data = &st_pll3200c32_cx_1,
	},
	{
		.compatible = "st,stih407-plls-c32-a9",
		.data = &st_pll3200c32_407_a9,
	},
	{
		.compatible = "st,sti8416-plls-c32-bx",
		.data = &st_pll3200c32_bx_8416,
	},
	{}
};

static void __init clkgen_c32_pll_setup(struct device_node *np)
{
	const struct of_device_id *match;
	struct clk *clk;
	const char *parent_name, *pll_name;
	void __iomem *pll_base;
	int num_odfs, odf;
	struct clk_onecell_data *clk_data;
	struct clkgen_pll_data	*data;
	struct clkgen_pll_conf  *conf_data = NULL;

	match = of_match_node(c32_pll_of_match, np);
	if (!match) {
		pr_err("%s: No matching data\n", __func__);
		return;
	}

	data = (struct clkgen_pll_data *)match->data;

	match = of_match_node(c32_pllconf_of_match, np);
	if (match)
		conf_data = (struct clkgen_pll_conf *)match->data;

	parent_name = of_clk_get_parent_name(np, 0);
	if (!parent_name)
		return;

	pll_base = clkgen_get_register_base(np);
	if (!pll_base)
		return;

	clk = clkgen_pll_register(parent_name, data, conf_data, pll_base,
				  np->name, data->lock);
	if (IS_ERR(clk))
		return;

	pll_name = __clk_get_name(clk);

	num_odfs = data->num_odfs;

	clk_data = kzalloc(sizeof(*clk_data), GFP_KERNEL);
	if (!clk_data)
		return;

	clk_data->clk_num = num_odfs;
	clk_data->clks = kzalloc(clk_data->clk_num * sizeof(struct clk *),
				 GFP_KERNEL);

	if (!clk_data->clks)
		goto err;

	for (odf = 0; odf < num_odfs; odf++) {
		struct clk *clk;
		const char *clk_name;

		if (of_property_read_string_index(np, "clock-output-names",
						  odf, &clk_name))
			return;

		clk = clkgen_odf_register(pll_name, pll_base, data, conf_data,
				odf, &clkgena_c32_odf_lock, clk_name);
		if (IS_ERR(clk))
			goto err;

		clk_data->clks[odf] = clk;
	}

	of_clk_add_provider(np, of_clk_src_onecell_get, clk_data);
	return;

err:
	kfree(pll_name);
	kfree(clk_data->clks);
	kfree(clk_data);
}
CLK_OF_DECLARE(clkgen_c32_pll, "st,clkgen-plls-c32", clkgen_c32_pll_setup);

static struct of_device_id pll_of_match[] = {
	{
		.compatible = "st,stih415-gpu-pll-c32",
		.data = &st_pll1200c32_gpu_415,
	},
	{
		.compatible = "st,stih416-gpu-pll-c32",
		.data = &st_pll1200c32_gpu_416,
	},
	{
		.compatible = "st,stid127-a9-plls-c45",
		.data = &st_pll1600c45_a9_127,
	},
};

static void __init clkgen_pll_setup(struct device_node *np)
{
	const struct of_device_id *match;
	struct clk *clk;
	const char *parent_name;
	void __iomem *reg;
	const char *clk_name;
	struct clkgen_pll_data	*data;

	match = of_match_node(pll_of_match, np);
	if (!match) {
		pr_err("%s: No matching data\n", __func__);
		return;
	}

	data = (struct clkgen_pll_data *)match->data;

	parent_name = of_clk_get_parent_name(np, 0);
	if (!parent_name)
		return;

	reg = clkgen_get_register_base(np);
	if (!reg)
		return;

	if (of_property_read_string_index(np, "clock-output-names",
					  0, &clk_name))
		return;

	clk = clkgen_pll_register(parent_name, data, NULL, reg, clk_name,
				  data->lock);

	if (!IS_ERR(clk))
		of_clk_add_provider(np, of_clk_src_simple_get, clk);

	return;
}
CLK_OF_DECLARE(clkgen_pll,
	       "st,clkgen-pll", clkgen_pll_setup);

static struct of_device_id c45_pll_of_match[] = {
	{
		.compatible = "st,clkgena-plls-c45-ax-0",
		.data = &st_pll1600c45_ax_0,
	},
	{
		.compatible = "st,clkgena-plls-c45-ax-1",
		.data = &st_pll1600c45_ax_1,
	},
	{}
};

static void __init clkgena_c45_pll_setup(struct device_node *np)
{
	const struct of_device_id *match;
	const int num_pll_outputs = 2;
	struct clk_onecell_data *clk_data;
	const char *parent_name;
	void __iomem *reg;
	const char *clk_name;
	struct clkgen_pll_data	*data;

	match = of_match_node(c45_pll_of_match, np);
	if (!match) {
		pr_err("%s: No matching data\n", __func__);
		return;
	}

	data = (struct clkgen_pll_data *)match->data;

	parent_name = of_clk_get_parent_name(np, 0);
	if (!parent_name)
		return;

	reg = clkgen_get_register_base(np);
	if (!reg)
		return;

	clk_data = kzalloc(sizeof(*clk_data), GFP_KERNEL);
	if (!clk_data)
		return;

	clk_data->clk_num = num_pll_outputs;
	clk_data->clks = kzalloc(clk_data->clk_num * sizeof(struct clk *),
				 GFP_KERNEL);

	if (!clk_data->clks)
		goto err;

	if (of_property_read_string_index(np, "clock-output-names",
					  0, &clk_name))
		goto err;

	/*
	 * PLL0 HS (high speed) output
	 */
	clk_data->clks[0] = clkgen_pll_register(parent_name,
						data, NULL, reg, clk_name,
						data->lock);

	if (IS_ERR(clk_data->clks[0]))
		goto err;

	if (of_property_read_string_index(np, "clock-output-names",
					  1, &clk_name))
		goto err;

	/*
	 * PLL0 LS (low speed) output, which is a fixed divide by 2 of the
	 * high speed output.
	 */
	clk_data->clks[1] = clkgen_lsdiv_register(__clk_get_name
						      (clk_data->clks[0]),
						      clk_name);

	if (IS_ERR(clk_data->clks[1]))
		goto err;

	of_clk_add_provider(np, of_clk_src_onecell_get, clk_data);
	return;

err:
	kfree(clk_data->clks);
	kfree(clk_data);
}
CLK_OF_DECLARE(clkgena_c45_plls,
	       "st,clkgena-plls-c45", clkgena_c45_pll_setup);
