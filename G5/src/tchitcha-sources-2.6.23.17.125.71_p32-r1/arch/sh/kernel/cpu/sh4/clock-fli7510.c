/*
 * Copyright (C) 2009 STMicroelectronics Limited
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 * Clocking framework stub.
 */

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/pm.h>
#include <linux/stm/clk.h>

#include "clock-common.h"



/* SH4 generic clocks ----------------------------------------------------- */

static struct clk generic_module_clk = {
	.name = "module_clk",
	.rate = 100000000,
};

static struct clk generic_comms_clk = {
	.name = "comms_clk",
	.rate = 100000000,
};



/* Audio clockgen --------------------------------------------------------- */

#define CLKGEN_AUDIO_REF_RATE		30000000

#define CTL_EN(base)			((base) + 0x00)
#define EN_CLK_512FS_FREE_RUN		0
#define EN_CLK_512FS_FREE_RUN__DISABLED	(0 << EN_CLK_512FS_FREE_RUN)
#define EN_CLK_512FS_FREE_RUN__ENABLED	(1 << EN_CLK_512FS_FREE_RUN)
#define EN_CLK_256FS_FREE_RUN		1
#define EN_CLK_256FS_FREE_RUN__DISABLED (0 << EN_CLK_256FS_FREE_RUN)
#define EN_CLK_256FS_FREE_RUN__ENABLED	(1 << EN_CLK_256FS_FREE_RUN)
#define EN_CLK_FS_FREE_RUN		2
#define EN_CLK_FS_FREE_RUN__DISABLED	(0 << EN_CLK_FS_FREE_RUN)
#define EN_CLK_FS_FREE_RUN__ENABLED	(1 << EN_CLK_FS_FREE_RUN)
#define EN_CLK_256FS_DEC_1		3
#define EN_CLK_256FS_DEC_1__DISABLED	(0 << EN_CLK_256FS_DEC_1)
#define EN_CLK_256FS_DEC_1__ENABLED	(1 << EN_CLK_256FS_DEC_1)
#define EN_CLK_FS_DEC_1			4
#define EN_CLK_FS_DEC_1__DISABLED	(0 << EN_CLK_FS_DEC_1)
#define EN_CLK_FS_DEC_1__ENABLED	(1 << EN_CLK_FS_DEC_1)
#define EN_CLK_SPDIF_RX			5
#define EN_CLK_SPDIF_RX__DISABLED	(0 << EN_CLK_SPDIF_RX)
#define EN_CLK_SPDIF_RX__ENABLED	(1 << EN_CLK_SPDIF_RX)
#define EN_CLK_256FS_DEC_2		6
#define EN_CLK_256FS_DEC_2__DISABLED	(0 << EN_CLK_256FS_DEC_2)
#define EN_CLK_256FS_DEC_2__ENABLED	(1 << EN_CLK_256FS_DEC_2)
#define EN_CLK_FS_DEC_2			7
#define EN_CLK_FS_DEC_2__DISABLED	(0 << EN_CLK_FS_DEC_2)
#define EN_CLK_FS_DEC_2__ENABLED	(1 << EN_CLK_FS_DEC_2)

#define CTL_SYNTH4X_AUD(base)			((base) + 0x04)
#define SYNTH4X_AUD_NDIV			0
#define SYNTH4X_AUD_NDIV__30_MHZ		(0 << SYNTH4X_AUD_NDIV)
#define SYNTH4X_AUD_NDIV__60_MHZ		(1 << SYNTH4X_AUD_NDIV)
#define SYNTH4X_AUD_SELCLKIN			1
#define SYNTH4X_AUD_SELCLKIN__CLKIN2V5		(0 << SYNTH4X_AUD_SELCLKIN)
#define SYNTH4X_AUD_SELCLKIN__CLKIN1V2		(1 << SYNTH4X_AUD_SELCLKIN)
#define SYNTH4X_AUD_SELBW			2
#define SYNTH4X_AUD_SELBW__VERY_GOOD_REFERENCE	(0 << SYNTH4X_AUD_SELBW)
#define SYNTH4X_AUD_SELBW__GOOD_REFERENCE	(1 << SYNTH4X_AUD_SELBW)
#define SYNTH4X_AUD_SELBW__BAD_REFERENCE	(2 << SYNTH4X_AUD_SELBW)
#define SYNTH4X_AUD_SELBW__VERY_BAD_REFERENCE	(3 << SYNTH4X_AUD_SELBW)
#define SYNTH4X_AUD_NPDA			4
#define SYNTH4X_AUD_NPDA__POWER_DOWN		(0 << SYNTH4X_AUD_NPDA)
#define SYNTH4X_AUD_NPDA__ACTIVE		(1 << SYNTH4X_AUD_NPDA)
#define SYNTH4X_AUD_NRST			5
#define SYNTH4X_AUD_NRST__MASK			(1 << SYNTH4X_AUD_NRST)
#define SYNTH4X_AUD_NRST__RESET			(0 << SYNTH4X_AUD_NRST)
#define SYNTH4X_AUD_NRST__NORMAL		(1 << SYNTH4X_AUD_NRST)

/* Warning! Registers spec defines these registers as 1 ... 4,
 * instead of 0 ... 3! */
#define CTL_SYNTH4X_AUD_N(base, n)	((base) + 0x08 + ((n - 1) * 0x4))
#define MD				0
#define MD__MASK			(0x1f << MD)
#define MD__(value)			(((value) << MD) & MD__MASK)
#define SDIV				5
#define SDIV__MASK			(0x7 << SDIV)
#define SDIV__(value)			(((value) << SDIV) & SDIV__MASK)
#define PE				8
#define PE__MASK			(0xffff << PE)
#define PE__(value)			(((value) << PE) & PE__MASK)
#define SEL_CLK_OUT			24
#define SEL_CLK_OUT__EXTCLK		(0 << SEL_CLK_OUT)
#define SEL_CLK_OUT__FSYNTH		(1 << SEL_CLK_OUT)
#define NSB				25
#define NSB__MASK			(1 << NSB)
#define NSB__STANDBY			(0 << NSB)
#define NSB__ACTIVE			(1 << NSB)
#define NSDIV3				26
#define NSDIV3__ACTIVE			(0 << NSDIV3)
#define NSDIV3__BYPASSED		(1 << NSDIV3)

static void *clkgen_audio_base;

static int clkgen_audio_enable(struct clk *clk)
{
	unsigned long value = readl(CTL_SYNTH4X_AUD_N(clkgen_audio_base,
				clk->id));

	value &= ~NSB__MASK;
	value |= NSB__ACTIVE;

	writel(value, CTL_SYNTH4X_AUD_N(clkgen_audio_base, clk->id));

	return 0;
}

static int clkgen_audio_disable(struct clk *clk)
{
	unsigned long value = readl(CTL_SYNTH4X_AUD_N(clkgen_audio_base,
				clk->id));

	value &= ~NSB__MASK;
	value |= NSB__STANDBY;

	writel(value, CTL_SYNTH4X_AUD_N(clkgen_audio_base, clk->id));

	return 0;
}

static int clkgen_audio_set_rate(struct clk *clk, unsigned long rate)
{
	unsigned int pe, md, sdiv;
	unsigned long value;
	int divider = (int)clk->private_data;

	if (divider)
		rate *= divider;

	if (clk_fsyn_get_params(CLKGEN_AUDIO_REF_RATE, rate,
				&md, &pe, &sdiv) != 0)
		return -EINVAL;

	value = readl(CTL_SYNTH4X_AUD_N(clkgen_audio_base, clk->id));

	value &= ~MD__MASK;
	value |= MD__(md);

	value &= ~PE__MASK;
	value |= PE__(pe);

	value &= ~SDIV__MASK;
	value |= SDIV__(sdiv);

	writel(value, CTL_SYNTH4X_AUD_N(clkgen_audio_base, clk->id));

	clk->nominal_rate = rate;
	clk->rate = clk_fsyn_get_rate(CLKGEN_AUDIO_REF_RATE, pe, md, sdiv);

	if (divider) {
		clk->nominal_rate /= divider;
		clk->rate /= divider;
	}

	return 0;
}

static struct clk_ops clkgen_audio_clk_ops = {
	.enable = clkgen_audio_enable,
	.disable = clkgen_audio_disable,
	.set_rate = clkgen_audio_set_rate,
};

static struct clk clkgen_audio_clks[] = {
	{
		/* fs_aud_1 / 2 -> clk_256fs_free_run */
		.name = "clk_256fs_free_run",
		.ops = &clkgen_audio_clk_ops,
		.id = 1,
		.private_data = (void *)2, /* divider on used output */
	}, {
		/* fs_aud_2 -> clk_256fs_dec_1 */
		.name = "clk_256fs_dec_1",
		.ops = &clkgen_audio_clk_ops,
		.id = 2,
	}, {
		/* fs_aud_3 -> clk_spdif_rx (not usable for players) */
		.name = "clk_spdif_rx",
		.ops = &clkgen_audio_clk_ops,
		.id = 3,
	}, {
		/* fs_aud_4 -> clk_256fs_dec_2 */
		.name = "clk_256fs_dec_2",
		.ops = &clkgen_audio_clk_ops,
		.id = 4,
	}
};

static int __init clkgen_audio_init(void)
{
	int err = 0;
	unsigned long value;
	int i;

	if (cpu_data->type == CPU_FLI7510)
		clkgen_audio_base = ioremap(0xfdee0000, 0x30);
	else
		clkgen_audio_base = ioremap(0xfe0e0000, 0x30);

	if (!clkgen_audio_base)
		return -EFAULT;

	/* Configure clkgen */

	value = EN_CLK_256FS_FREE_RUN__ENABLED;
	value |= EN_CLK_256FS_DEC_1__ENABLED;
	value |= EN_CLK_SPDIF_RX__ENABLED;
	value |= EN_CLK_256FS_DEC_2__ENABLED;
	writel(value, CTL_EN(clkgen_audio_base));

	value = SYNTH4X_AUD_NDIV__30_MHZ;
	value |= SYNTH4X_AUD_SELCLKIN__CLKIN1V2;
	value |= SYNTH4X_AUD_SELBW__VERY_GOOD_REFERENCE;
	value |= SYNTH4X_AUD_NPDA__ACTIVE;
	value |= SYNTH4X_AUD_NRST__RESET;
	writel(value, CTL_SYNTH4X_AUD(clkgen_audio_base));

	value = SEL_CLK_OUT__FSYNTH;
	value |= NSB__STANDBY;
	value |= NSDIV3__BYPASSED;
	for (i = 0; i < ARRAY_SIZE(clkgen_audio_clks); i++) {
		writel(value, CTL_SYNTH4X_AUD_N(clkgen_audio_base,
				i + 1));
		/* Set "safe" rate */
		clkgen_audio_set_rate(&clkgen_audio_clks[i], 32000 * 128);
	}

	/* Unreset ;-) it now */

	value = readl(CTL_SYNTH4X_AUD(clkgen_audio_base));
	value &= ~SYNTH4X_AUD_NRST__MASK;
	value |= SYNTH4X_AUD_NRST__NORMAL;
	writel(value, CTL_SYNTH4X_AUD(clkgen_audio_base));

	/* Register clocks */

	for (i = 0; i < ARRAY_SIZE(clkgen_audio_clks) && !err; i++)
		err = clk_register(&clkgen_audio_clks[i]);

	return err;
}




/* ------------------------------------------------------------------------ */

int __init clk_init(void)
{
	int err;

	/* Generic SH-4 clocks */

	err = clk_register(&generic_module_clk);
	if (err)
		goto error;

	err = clk_register(&generic_comms_clk);
	if (err)
		goto error;

	err = clkgen_audio_init();

error:
	return err;
}

