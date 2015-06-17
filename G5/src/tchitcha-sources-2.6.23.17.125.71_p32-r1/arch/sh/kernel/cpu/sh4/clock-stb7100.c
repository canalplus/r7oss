/*
 * Copyright (C) 2005 STMicroelectronics Limited
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 * Code to handle the clockgen hardware on the STb7100.
 */

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/gfp.h>
#include <linux/slab.h>
#include <linux/pm.h>
#include <linux/stm/clk.h>
#include <asm/freq.h>
#include <asm/io.h>
#include <asm-generic/div64.h>

#include "./clock-common.h"
#include "./soc-stb7100.h"

#ifdef CONFIG_CLK_LOW_LEVEL_DEBUG
#include <linux/stm/pio.h>
#define dgb_print(fmt, args...)  	\
	printk(KERN_DEBUG "%s: " fmt, __FUNCTION__ , ## args)
#else
#define dgb_print(fmt, args...)
#endif

void __iomem *clkgena_base;

#define CLOCKGEN_PLL0_CFG	0x08
#define CLOCKGEN_PLL0_CLK1_CTRL	0x14
#define CLOCKGEN_PLL0_CLK2_CTRL	0x18
#define CLOCKGEN_PLL0_CLK3_CTRL	0x1c
#define CLOCKGEN_PLL0_CLK4_CTRL	0x20
#define CLOCKGEN_PLL1_CFG	0x24

/* to enable/disable and reduce the coprocessor clock*/
#define CLOCKGEN_CLK_DIV	0x30
#define CLOCKGEN_CLK_EN		0x34

			       /* 0  1  2  3  4  5  6  7  */
static unsigned char ratio1[] = { 1, 2, 3, 4, 6, 8 , NO_MORE_RATIO};
static unsigned char ratio2[] = { 1, 2, 3, 4, 6, 8 , NO_MORE_RATIO};
static unsigned char ratio3[] = { 4, 2, 4, 4, 6, 8 , NO_MORE_RATIO};
static unsigned char ratio4[] = { 1, 2, 3, 4, 6, 8 , NO_MORE_RATIO};

static int pll_freq(unsigned long addr)
{
	unsigned long freq, data, ndiv, pdiv, mdiv;

	data = readl(clkgena_base+addr);
	mdiv = (data >>  0) & 0xff;
	ndiv = (data >>  8) & 0xff;
	pdiv = (data >> 16) & 0x7;
	freq = (((2 * (CONFIG_SH_EXTERNAL_CLOCK / 1000) * ndiv) / mdiv) /
		(1 << pdiv)) * 1000;

	return freq;
}

static int pll0_clk_init(struct clk *clk)
{
	clk->rate = pll_freq(CLOCKGEN_PLL0_CFG);

	return 0;
}

static struct clk_ops pll0_clk_ops = {
	.init = pll0_clk_init,
};

static struct clk pll0_clk = {
	.name		= "pll0_clk",
	.flags		= CLK_ALWAYS_ENABLED | CLK_RATE_PROPAGATES,
	.ops		= &pll0_clk_ops,
};

static int pll1_clk_init(struct clk *clk)
{
	clk->rate = pll_freq(CLOCKGEN_PLL1_CFG);

	return 0;
}

static struct clk_ops pll1_clk_ops = {
	.init = pll1_clk_init,
};

static struct clk pll1_clk = {
	.name		= "pll1_clk",
	.flags		= CLK_ALWAYS_ENABLED | CLK_RATE_PROPAGATES,
	.ops		= &pll1_clk_ops,
};

struct clokgenA
{
	unsigned long ctrl_reg;
	unsigned int div;
	unsigned char *ratio;
};


enum clockgenA_ID {
	SH4_CLK_ID = 0,
	SH4IC_CLK_ID,
	MODULE_ID,
	SLIM_ID,
	LX_AUD_ID,
	LX_VID_ID,
	LMISYS_ID,
	LMIVID_ID,
	IC_ID,
	IC_100_ID,
	EMI_ID
};

static void clockgenA_clk_recalc(struct clk *clk)
{
	struct clokgenA *cga = (struct clokgenA *)clk->private_data;
	clk->rate = clk->parent->rate / cga->div;
	return;
}

static int clockgenA_clk_set_rate(struct clk *clk, unsigned long value)
{
	unsigned long data = readl(clkgena_base + CLOCKGEN_CLK_DIV);
	unsigned long val = 1 << (clk->id -5);

	if (clk->id != LMISYS_ID && clk->id != LMIVID_ID)
		return -1;
	writel(0xc0de, clkgena_base);
	if (clk->rate > value) {/* downscale */
		writel(data | val, clkgena_base + CLOCKGEN_CLK_DIV);
		clk->rate /= 1024;
	} else {/* upscale */
		writel(data & ~val, clkgena_base + CLOCKGEN_CLK_DIV);
		clk->rate *= 1024;
	}
	writel(0x0, clkgena_base);
	return 0;
}

static int clockgenA_clk_init(struct clk *clk)
{
	struct clokgenA *cga = (struct clokgenA *)clk->private_data;
	if (cga->ratio) {
		unsigned long data = readl(clkgena_base + cga->ctrl_reg) & 0x7;
		cga->div = 2*cga->ratio[data];
	}
	clk->rate = clk->parent->rate / cga->div;

	return 0;
}

static void clockgenA_clk_XXable(struct clk *clk, int enable)
{
	unsigned long tmp, value;
	struct clokgenA *cga = (struct clokgenA *)clk->private_data;

	if (clk->id != LMISYS_ID && clk->id != LMIVID_ID)
		return ;

	tmp   = readl(clkgena_base+cga->ctrl_reg) ;
	value = 1 << (clk->id -5);
	writel(0xc0de, clkgena_base);
	if (enable) {
		writel(tmp | value, clkgena_base + cga->ctrl_reg);
		clockgenA_clk_init(clk); /* to evaluate the rate */
	} else {
		writel(tmp & ~value, clkgena_base + cga->ctrl_reg);
		clk->rate = 0;
	}
	writel(0x0, clkgena_base);
}
static int clockgenA_clk_enable(struct clk *clk)
{
	clockgenA_clk_XXable(clk, 1);

	return 0;
}

static int clockgenA_clk_disable(struct clk *clk)
{
	clockgenA_clk_XXable(clk, 0);

	return 0;
}

static struct clk_ops clokgenA_ops = {
	.init		= clockgenA_clk_init,
	.recalc		= clockgenA_clk_recalc,
	.set_rate	= clockgenA_clk_set_rate,
	.enable		= clockgenA_clk_enable,
	.disable	= clockgenA_clk_disable,
};

#define DEF_FLAG	CLK_ALWAYS_ENABLED | CLK_RATE_PROPAGATES
#define PM_1024		(10<<CLK_PM_EXP_SHIFT)

#define CLKGENA(_id, clock, pll, _ctrl_reg, _div, _ratio, _flags)\
[_id] = {							\
	.name	= #clock "_clk",				\
	.flags	= (_flags),					\
	.parent	= &(pll),					\
	.ops	= &clokgenA_ops,				\
	.id	= (_id),					\
	.private_data = &(struct clokgenA){			\
		.div = (_div),					\
		.ctrl_reg = (_ctrl_reg),			\
		.ratio = (_ratio)				\
		},						\
	}

struct clk clkgena_clks[] = {
CLKGENA(SH4_CLK_ID,	sh4, pll0_clk, CLOCKGEN_PLL0_CLK1_CTRL,
	1, ratio1, DEF_FLAG),
CLKGENA(SH4IC_CLK_ID, sh4_ic, pll0_clk, CLOCKGEN_PLL0_CLK2_CTRL,
	1, ratio2, DEF_FLAG),
CLKGENA(MODULE_ID,	module,   pll0_clk, CLOCKGEN_PLL0_CLK3_CTRL,
	1, ratio3, DEF_FLAG),
CLKGENA(SLIM_ID,	slim,     pll0_clk, CLOCKGEN_PLL0_CLK4_CTRL,
	1, ratio4, DEF_FLAG),
CLKGENA(LX_AUD_ID,	st231aud, pll1_clk, CLOCKGEN_CLK_EN,
	1, NULL, DEF_FLAG | PM_1024),
CLKGENA(LX_VID_ID,	st231vid, pll1_clk, CLOCKGEN_CLK_EN,
	1, NULL, DEF_FLAG | PM_1024),
CLKGENA(LMISYS_ID,	lmisys,   pll1_clk, 0,
	1, NULL, DEF_FLAG),
CLKGENA(LMIVID_ID,	lmivid,   pll1_clk, 0,
	1, NULL, DEF_FLAG),
CLKGENA(IC_ID,	ic,	  pll1_clk, 0,
	2, NULL, DEF_FLAG),
CLKGENA(IC_100_ID,	ic_100,   pll1_clk, 0,
	4, NULL, DEF_FLAG),
CLKGENA(EMI_ID,	emi,      pll1_clk, 0,
	4, NULL, DEF_FLAG)
};

static void comms_clk_recalc(struct clk *clk)
{
	clk->rate = clk->parent->rate;
}

static int comms_clk_init(struct clk *clk)
{
	comms_clk_recalc(clk);

	return 0;
}

static struct clk_ops comms_clk_ops = {
	.init	= comms_clk_init,
	.recalc	= comms_clk_recalc,
};

struct clk comms_clk = {
	.name		= "comms_clk",
	.parent		= &clkgena_clks[IC_100_ID],
	.flags		= CLK_ALWAYS_ENABLED,
	.ops		= &comms_clk_ops
};

static struct clk *onchip_clocks[] = {
	&pll0_clk,
	&pll1_clk,
};



/* Clockgen C */

#define CLKGENC_REF_RATE		30000000

#define FSYN_CFG(base)			((base) + 0x00)

#define RSTP				0
#define RSTP__MASK			(1 << RSTP)
#define RSTP__NORMAL			(0 << RSTP)
#define RSTP__RESET			(1 << RSTP)

#define PCM_CLK(n)			(2 + (n))
#define PCM_CLK_SEL__EXTERNAL(n)	(0 << PCM_CLK(n))
#define PCM_CLK_SEL__FSYN(n)		(1 << PCM_CLK(n))

#define FS_EN(n)			(6 + (n))
#define FS_EN__DISABLED(n)		(0 << FS_EN(n))
#define FS_EN__ENABLED(n)		(1 << FS_EN(n))

#define NSB(n)				(10 + (n))
#define NSB__MASK(n)			(1 << NSB(n))
#define NSB__STANDBY(n)			(0 << NSB(n))
#define NSB__ACTIVE(n)			(1 << NSB(n))

#define NPDA				14
#define NPDA__POWER_DOWN		(0 << NPDA)
#define NPDA__NORMAL			(1 << NPDA)

#define NDIV				15
#define NDIV__27_30_MHZ			(0 << NDIV)
#define NDIV__54_60_MHZ			(1 << NDIV)

#define BW_SEL				16
#define BW_SEL__VERY_GOOD_REFERENCE	(0 << BW_SEL)
#define BW_SEL__GOOD_REFERENCE		(1 << BW_SEL)
#define BW_SEL__BAD_REFERENCE		(2 << BW_SEL)
#define BW_SEL__VERY_BAD_REFERENCE	(3 << BW_SEL)

#define REF_CLK_IN			23
#define REF_CLK_IN__SATA_PHY_30MHZ	(0 << REF_CLK_IN)
#define REF_CLK_IN__SYSBCLKINALT	(1 << REF_CLK_IN)

#define FSYN_MD(base, n)		((base) + ((n) + 1) * 0x10 + 0x00)
#define FSYN_PE(base, n)		((base) + ((n) + 1) * 0x10 + 0x04)
#define FSYN_SDIV(base, n)		((base) + ((n) + 1) * 0x10 + 0x08)

#define FSYN_PROGEN(base, n)		((base) + ((n) + 1) * 0x10 + 0x0c)
#define FSYN_PROGEN__IGNORED		0
#define FSYN_PROGEN__USED		1

static void *clkgenc_base;

static int clkgenc_enable(struct clk *clk)
{
	unsigned long value = readl(FSYN_CFG(clkgenc_base));

	value &= ~NSB__MASK(clk->id);
	value |= NSB__ACTIVE(clk->id);

	writel(value, FSYN_CFG(clkgenc_base));

	return 0;
}

static int clkgenc_disable(struct clk *clk)
{
	unsigned long value = readl(FSYN_CFG(clkgenc_base));

	value &= ~NSB__MASK(clk->id);
	value |= NSB__STANDBY(clk->id);

	writel(value, FSYN_CFG(clkgenc_base));

	return 0;
}

static int clkgenc_set_rate(struct clk *clk, unsigned long rate)
{
	unsigned int pe, md, sdiv;

	if (clk_fsyn_get_params(CLKGENC_REF_RATE, rate, &md, &pe, &sdiv) != 0)
		return -EINVAL;

	writel(FSYN_PROGEN__IGNORED, FSYN_PROGEN(clkgenc_base, clk->id));

	writel(md, FSYN_MD(clkgenc_base, clk->id));
	writel(pe, FSYN_PE(clkgenc_base, clk->id));
	writel(sdiv, FSYN_SDIV(clkgenc_base, clk->id));

	writel(FSYN_PROGEN__USED, FSYN_PROGEN(clkgenc_base, clk->id));

	clk->nominal_rate = rate;
	clk->rate = clk_fsyn_get_rate(CLKGENC_REF_RATE, pe, md, sdiv);

	return 0;
}

static struct clk_ops clkgenc_clk_ops = {
	.enable = clkgenc_enable,
	.disable = clkgenc_disable,
	.set_rate = clkgenc_set_rate,
};

static struct clk clkgenc_clks[] = {
	{
		.name = "pcm0_clk",
		.ops = &clkgenc_clk_ops,
		.id = 0,
	}, {
		.name = "pcm1_clk",
		.ops = &clkgenc_clk_ops,
		.id = 1,
	}, {
		.name = "spdif_clk",
		.ops = &clkgenc_clk_ops,
		.id = 2,
	}
};

static int __init clkgenc_init(void)
{
	int err = 0;
	unsigned long value;
	int i;

	clkgenc_base = ioremap(CLOCKGENC_BASE_ADDR, 0x40);
	if (!clkgenc_base)
		return -EFAULT;

	value = RSTP__RESET;
	for (i = 0; i < ARRAY_SIZE(clkgenc_clks); i++) {
		value |= PCM_CLK_SEL__FSYN(i);
		value |= FS_EN__ENABLED(i);
		value |= NSB__STANDBY(i);
		/* Set "safe" rate */
		clkgenc_set_rate(&clkgenc_clks[i], 32000 * 128);
	}
	value |= NPDA__NORMAL;
	value |= NDIV__27_30_MHZ;
	value |= BW_SEL__GOOD_REFERENCE;
	value |= REF_CLK_IN__SATA_PHY_30MHZ;
	writel(value, FSYN_CFG(clkgenc_base));

	/* Unreset ;-) it now */

	value &= ~RSTP__MASK;
	value |= RSTP__NORMAL;
	writel(value, FSYN_CFG(clkgenc_base));

	/* Register clocks */

	for (i = 0; i < ARRAY_SIZE(clkgenc_clks) && !err; i++)
		err = clk_register(&clkgenc_clks[i]);

	return err;
}



/* Main init */

int __init clk_init(void)
{
	int i, ret = 0;

	/**************/
	/* Clockgen A */
	/**************/
	clkgena_base = ioremap(CLOCKGEN_BASE_ADDR, 0x100);

	for (i = 0; i < ARRAY_SIZE(onchip_clocks); i++) {
		struct clk *clk = onchip_clocks[i];

		ret |= clk_register(clk);
		clk_enable(clk);
	}

	for (i = 0; i < ARRAY_SIZE(clkgena_clks); i++) {
		struct clk *clk = &clkgena_clks[i];
		ret |= clk_register(clk);
		clk_enable(clk);
	}
	clk_register(&comms_clk);
	clk_enable(&comms_clk);

#ifdef CONFIG_CLK_LOW_LEVEL_DEBUG
	iowrite32(0xc0de, clkgena_base);
	iowrite32(13, clkgena_base + 0x38);   /* routed on SYSCLK_OUT */
	iowrite32(0, clkgena_base);
	stpio_request_set_pin(5, 2, "clkB dbg", STPIO_ALT_OUT, 1);
#endif

	ret |= clkgenc_init();

	return ret;
}
