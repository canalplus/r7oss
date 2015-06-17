/******************************************************************************
 *
 * File name   : clock-stx5197.c
 * Description : Low Level API - 5197 specific implementation
 *
 * COPYRIGHT (C) 2009 STMicroelectronics - All Rights Reserved
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 *****************************************************************************/

/* ----- Modification history (most recent first)----
24/jul/09 francesco.virlinzi@st.com
	  Redesigned all the implementation
09/jul/09 fabrice.charpentier@st.com
	  Revisited for LLA & Linux compliancy.
*/

/* Includes --------------------------------------------------------------- */

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/stm/clk.h>
#include "clock-stx5197.h"
#include "clock-regs-stx5197.h"

#include "clock-oslayer.h"
#include "clock-common.h"

#define CRYSTAL  30000000

/* Private Function prototypes -------------------------------------------- */
static void sys_service_lock(int lock_enable);

static int clkgen_xtal_init(clk_t *clk);

static int clkgen_pll_init(clk_t *clk);
static int clkgen_pll_set_rate(clk_t *clk, unsigned long rate);
static int clkgen_pll_recalc(clk_t *clk);
static int clkgen_pll_enable(clk_t *clk);
static int clkgen_pll_enable(clk_t *clk);
static int clkgen_pll_disable(clk_t *clk);
static int clkgen_pll_observe(clk_t *clk, unsigned long *divd);

static int clkgen_fs_set_rate(clk_t *clk, unsigned long rate);
static int clkgen_fs_enable(clk_t *clk);
static int clkgen_fs_disable(clk_t *clk);
static int clkgen_fs_observe(clk_t *clk, unsigned long *freq);
static int clkgen_fs_init(clk_t *clk);
static int clkgen_fs_recalc(clk_t *clk);

/*---------------------------------------------------------------------*/

#define FRAC(whole, half)	((whole << 1) | (half ? 1 : 0))

#define DIVIDER(depth, seq, hno, even)				\
	((hno << 25) | (even << 24) | (depth << 20) | (seq << 0))

#define COMBINE_DIVIDER(depth, seq, hno, even)			\
	.value = DIVIDER(depth, seq, hno, even),		\
	.cfg_0 = (seq & 0xffff),				\
	.cfg_1 = (seq >> 16),					\
	.cfg_2 = (depth | (even << 5) | (hno << 6))

static const struct {
	unsigned long ratio, value;
	unsigned short cfg_0;
	unsigned char cfg_1, cfg_2;
} divide_table[] = {
	{
	FRAC(2, 0), COMBINE_DIVIDER(0x01, 0x00AAA, 0x1, 0x1)}, {
	FRAC(2, 5), COMBINE_DIVIDER(0x04, 0x05AD6, 0x1, 0x0)}, {
	FRAC(3, 0), COMBINE_DIVIDER(0x01, 0x00DB6, 0x0, 0x0)}, {
	FRAC(3, 5), COMBINE_DIVIDER(0x03, 0x0366C, 0x1, 0x0)}, {
	FRAC(4, 0), COMBINE_DIVIDER(0x05, 0x0CCCC, 0x1, 0x1)}, {
	FRAC(4, 5), COMBINE_DIVIDER(0x07, 0x3399C, 0x1, 0x0)}, {
	FRAC(5, 0), COMBINE_DIVIDER(0x04, 0x0739C, 0x0, 0x0)}, {
	FRAC(5, 5), COMBINE_DIVIDER(0x00, 0x0071C, 0x1, 0x0)}, {
	FRAC(6, 0), COMBINE_DIVIDER(0x01, 0x00E38, 0x1, 0x1)}, {
	FRAC(6, 5), COMBINE_DIVIDER(0x02, 0x01C78, 0x1, 0x0)}, {
	FRAC(7, 0), COMBINE_DIVIDER(0x03, 0x03C78, 0x0, 0x0)}, {
	FRAC(7, 5), COMBINE_DIVIDER(0x04, 0x07878, 0x1, 0x0)}, {
	FRAC(8, 0), COMBINE_DIVIDER(0x05, 0x0F0F0, 0x1, 0x1)}, {
	FRAC(8, 5), COMBINE_DIVIDER(0x06, 0x1E1F0, 0x1, 0x0)}, {
	FRAC(9, 0), COMBINE_DIVIDER(0x07, 0x3E1F0, 0x0, 0x0)}, {
	FRAC(9, 5), COMBINE_DIVIDER(0x08, 0x7C1F0, 0x1, 0x0)}, {
	FRAC(10, 0), COMBINE_DIVIDER(0x09, 0xF83E0, 0x1, 0x1)}, {
	FRAC(11, 0), COMBINE_DIVIDER(0x00, 0x007E0, 0x0, 0x0)}, {
	FRAC(12, 0), COMBINE_DIVIDER(0x01, 0x00FC0, 0x1, 0x1)}, {
	FRAC(13, 0), COMBINE_DIVIDER(0x02, 0x01FC0, 0x0, 0x0)}, {
	FRAC(14, 0), COMBINE_DIVIDER(0x03, 0x03F80, 0x1, 0x1)}, {
	FRAC(15, 0), COMBINE_DIVIDER(0x04, 0x07F80, 0x0, 0x0)}, {
	FRAC(16, 0), COMBINE_DIVIDER(0x05, 0x0FF00, 0x1, 0x1)}, {
	FRAC(17, 0), COMBINE_DIVIDER(0x06, 0x1FF00, 0x0, 0x0)}, {
	FRAC(18, 0), COMBINE_DIVIDER(0x07, 0x3FE00, 0x1, 0x1)}, {
	FRAC(19, 0), COMBINE_DIVIDER(0x08, 0x7FE00, 0x0, 0x0)}, {
	FRAC(20, 0), COMBINE_DIVIDER(0x09, 0xFFC00, 0x1, 0x1)}
};

/* Possible operations registration.
   Operations are usually grouped by clockgens due to specific HW implementation

   Name, Desc, Init, SetParent, Setrate, recalc, enable, disable, observe, Measu

   where
     Name: MUST be the same one declared with REGISTER_CLK (ops field).
     Desc: Clock group short description. Ex: "clockgen A", "USB", "LMI"
     Init: Clock init function (read HW to identify parent & compute rate).
     SetParent: Parent/src setup function.
     Setrate: Clock frequency setup function.
     enable: Clock enable function.
     disable: Clock disable function.
     observe: Clock observation function.
     recalc: Clock frequency recompute function. Called when parent clock change
     Measure: Clock measure function (when HW available).

   Note: If no capability, put NULL instead of function name.
   Note: All functions should return 'clk_err_t'. */

REGISTER_OPS(Top,
	"Top clocks",
	clkgen_xtal_init,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,	/* No measure function */
	NULL
);
REGISTER_OPS(PLL,
	"PLL",
	clkgen_pll_init,
	NULL,
	clkgen_pll_set_rate,
	clkgen_pll_recalc,
	clkgen_pll_enable,
	clkgen_pll_disable,
	clkgen_pll_observe,
	NULL,
	NULL
);

REGISTER_OPS(FS,
	"FS",
	clkgen_fs_init,
	NULL,
	clkgen_fs_set_rate,
	clkgen_fs_recalc,
	clkgen_fs_enable,
	clkgen_fs_disable,
	clkgen_fs_observe,
	NULL,
	NULL
);

/* Clocks identifier list */

/* Physical clocks description */
clk_t clk_clocks[] = {
/*	    clkID	       Ops	 Nominalrate   Flags */
REGISTER_CLK(OSC_REF, &Top, CRYSTAL,
		CLK_RATE_PROPAGATES | CLK_ALWAYS_ENABLED),

/* PLLs */
REGISTER_CLK(PLLA,	&PLL, 700000000, CLK_RATE_PROPAGATES),
REGISTER_CLK(PLLB,	&PLL, 800000000, CLK_RATE_PROPAGATES),
/* PLL child */
REGISTER_CLK(PLL_CPU,	&PLL, 800000000, 0),
REGISTER_CLK(PLL_LMI,	&PLL, 200000000, 0),
REGISTER_CLK(PLL_BIT,	&PLL, 200000000, 0),
REGISTER_CLK(PLL_SYS,	&PLL, 133000000, CLK_RATE_PROPAGATES |
					CLK_ALWAYS_ENABLED),
REGISTER_CLK(PLL_FDMA,	&PLL, 350000000, CLK_RATE_PROPAGATES),
REGISTER_CLK(PLL_DDR,	&PLL, 0, 0),
REGISTER_CLK(PLL_AV,	&PLL, 100000000, 0),
REGISTER_CLK(PLL_SPARE, &PLL, 50000000, 0),
REGISTER_CLK(PLL_ETH,	&PLL, 100000000, 0),
REGISTER_CLK(PLL_ST40_ICK, &PLL, 350000000, CLK_RATE_PROPAGATES |
					CLK_ALWAYS_ENABLED),
REGISTER_CLK(PLL_ST40_PCK, &PLL, 133000000, CLK_RATE_PROPAGATES |
					CLK_ALWAYS_ENABLED),
/* FS A */
REGISTER_CLK(FSA_SPARE,	&FS, 36000000, 0),
REGISTER_CLK(FSA_PCM,	&FS, 72000000, 0),
REGISTER_CLK(FSA_SPDIF,	&FS, 36000000, 0),
REGISTER_CLK(FSA_DSS,	&FS, 13800000, 0),
/* FS B */
REGISTER_CLK(FSB_PIX,	&FS, 13800000, 0),
REGISTER_CLK(FSB_FDMA_FS, &FS, 13800000, 0),
REGISTER_CLK(FSB_AUX,	&FS, 27000000, 0),
REGISTER_CLK(FSB_USB,	&FS, 48000000, 0),
};

GENERIC_LINUX_CLKS(clk_clocks[PLL_ST40_ICK],	/* sh4_clk parent    */
		   clk_clocks[PLL_ST40_PCK],	/* module_clk parent */
		   clk_clocks[PLL_SYS]);	/* comms_clk parent  */

/*
 * The Linux clk_init function
 */
int __init clk_init(void)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(clk_clocks); ++i)
		if (clk_clocks[i].name && !clk_register(&clk_clocks[i]))
			clk_enable(&clk_clocks[i]);

	REGISTER_GENERIC_LINUX_CLKS();

	return 0;
}

static const unsigned long pll_cfg0_offset[] = {
	CLKDIV0_CONFIG0,	/* cpu		*/
	CLKDIV1_CONFIG0,	/* lmi		*/
	CLKDIV2_CONFIG0,	/* blit		*/
	CLKDIV3_CONFIG0,	/* sys		*/
	CLKDIV4_CONFIG0,	/* fdma		*/
	-1,	/* no configuration for ddr clk	*/
	CLKDIV6_CONFIG0,	/* av		*/
	CLKDIV7_CONFIG0,	/* spare	*/
	CLKDIV8_CONFIG0,	/* eth		*/
	CLKDIV9_CONFIG0,	/* st40_ick	*/
	CLKDIV10_CONFIG0	/* st40_pck	*/
};

static void sys_service_lock(int lock_enable)
{
	if (lock_enable) {
		CLK_WRITE(SYS_SERVICE_ADDR + CLK_LOCK_CFG, 0x100);
		return;
	}
	CLK_WRITE(SYS_SERVICE_ADDR + CLK_LOCK_CFG, 0xF0);
	CLK_WRITE(SYS_SERVICE_ADDR + CLK_LOCK_CFG, 0x0F);
}

static unsigned long pll_hw_evaluate(unsigned long input, U32 div_num)
{
	unsigned long offset;
	unsigned long config0, config1, config2;
	unsigned long seq, depth, hno, even;
	unsigned long combined, i;

	offset = pll_cfg0_offset[div_num];

	config0 = CLK_READ(SYS_SERVICE_ADDR + offset);
	config1 = CLK_READ(SYS_SERVICE_ADDR + offset + 0x4);
	config2 = CLK_READ(SYS_SERVICE_ADDR + offset + 0x8);

	seq = (config0 & 0xffff) | ((config1 & 0xf) << 16);
	depth = config2 & 0xf;
	hno = (config2 & (1 << 6)) ? 1 : 0;
	even = (config2 & (1 << 5)) ? 1 : 0;
	combined = DIVIDER(depth, seq, hno, even);

	for (i = 0; i < ARRAY_SIZE(divide_table); i++)
		if (divide_table[i].value == combined)
			return (input * 2) / divide_table[i].ratio;

	return 0;
}

static int clkgen_pll_recalc(clk_t *clk)
{
	if (clk->id < PLL_CPU || clk->id > PLL_ST40_PCK)
		return CLK_ERR_BAD_PARAMETER;

	if (clk->id == PLL_DDR) {
		clk->rate = clk->parent->rate;
		return 0;
	}
	clk->rate = pll_hw_evaluate(clk->parent->rate, clk->id - PLL_CPU);
	return 0;
}

/*==========================================================
Name:		clkgen_xtal_init
description	Top Level System Lock
===========================================================*/
static int clkgen_xtal_init(clk_t *clk)
{
	if (!clk)
		return CLK_ERR_BAD_PARAMETER;

	/* Top recalc function */
	if (clk->id == OSC_REF)
		clk->rate = CRYSTAL;

	return 0;
}

static unsigned long clkgen_pll_eval(unsigned long input, int id)
{
	unsigned long config0, config1;
	unsigned long pll_num = id - PLLA;

	config0 = CLK_READ(SYS_SERVICE_ADDR + 8 * pll_num);
	config1 = CLK_READ(SYS_SERVICE_ADDR + 8 * pll_num + 4);

#define  CLK_PLL_CONFIG1_POFF   (1<<13)
	if (config1 & CLK_PLL_CONFIG1_POFF)
		return 0;

	return clk_pll800_freq(input, config0 | ((config1 & 0x7) << 16));
}

/*==========================================================
Name:		clkgen_pll_init
description	PLL clocks init
===========================================================*/

static int clkgen_pll_init(clk_t *clk)
{
	unsigned long data;

	if (!clk || clk->id < PLLA || clk->id > PLL_ST40_PCK)
		return CLK_ERR_BAD_PARAMETER;

	/* 1. set the right parent */
	if (clk->id < PLL_CPU)	/* PLLA and PLLB */
		clk->parent = &clk_clocks[OSC_REF];
	else {
		data = CLK_READ(SYS_SERVICE_ADDR + PLL_SELECT_CFG) >> 1;
		clk->parent =
		    &clk_clocks[PLLA +
				(data & (1 << (clk->id - PLL_CPU)) ? 1 : 0)];
	}

	/* 2. evaluate the rate */
	if (clk->id < PLL_CPU)
		clk->rate = clkgen_pll_eval(clk->parent->rate, clk->id);
	else
		clkgen_pll_recalc(clk);

	return 0;
}

static void pll_hw_set(unsigned long addr, U32 cfg0, U32 cfg1, U32 cfg2)
{
	unsigned long flags;
	addr += SYS_SERVICE_ADDR;

	sys_service_lock(0);

/*
 * On the 5197 platform it's mandatory change the clock setting with an
 * asm code because in X1 mode all the clocks are routed on Xtal
 * and it could be dangerous a memory access
 *
 * All the code is self-contained in a single icache line
 */
	local_irq_save(flags);

#define   CLK_MODE_CTRL_NULL    0x0
#define   CLK_MODE_CTRL_X1      0x1
#define   CLK_MODE_CTRL_PROG    0x2
#define   CLK_MODE_CTRL_STDB    0x3
	asm volatile (".balign  32      \n"
		      "mov.l    %5, @%4 \n"	/* in X1 mode */
		      "mov.l    %1, @(0,%0)\n"	/* set     */
		      "mov.l    %2, @(4,%0)\n"	/*  the    */
		      "mov.l    %3, @(8,%0)\n"	/*   ratio */
		      "mov.l    %6, @%4 \n"	/* in Prog mode */
		      "tst      %7, %7  \n"	/* wait stable signal */
		      "2:		\n"
		      "bf/s     2b      \n"
		      " dt      %7      \n"
		::	"r" (addr),
			"r" (cfg0),
			"r" (cfg1),
			"r" (cfg2),	/* with enable */
			"r" (SYS_SERVICE_ADDR + MODE_CONTROL),
			"r" (CLK_MODE_CTRL_X1),
			"r" (CLK_MODE_CTRL_PROG),
			"r" (1000000)
		:	"memory");
	local_irq_restore(flags);

	sys_service_lock(1);
}

static int clkgen_pll_set_rate(clk_t *clk, unsigned long rate)
{
	unsigned long i, offset;

	if (clk->id < PLL_CPU)
		return CLK_ERR_BAD_PARAMETER;

	for (i = 0; i < ARRAY_SIZE(divide_table); i++)
		if (((clk_get_rate(clk->parent) * 2) /
		     divide_table[i].ratio) == rate)
			break;

	if (i == ARRAY_SIZE(divide_table))	/* not found! */
		return CLK_ERR_BAD_PARAMETER;

	offset = pll_cfg0_offset[clk->id - PLL_CPU];

	if (offset == -1) /* ddr case */
		return CLK_ERR_BAD_PARAMETER;

	pll_hw_set(offset, divide_table[i].cfg_0,
			  divide_table[i].cfg_1,
			  divide_table[i].cfg_2 | (1 << 4));

	clk->rate = rate;
	return 0;
}


static int clkgen_pll_xxable(clk_t *clk, unsigned long enable)
{
	unsigned long offset, reg_cfg0, reg_cfg1, reg_cfg2;

	if (!clk || clk->id > PLL_ST40_PCK)
		return CLK_ERR_BAD_PARAMETER;

	if (clk->id < PLL_CPU)
		return 0;

	offset = pll_cfg0_offset[clk->id - PLL_CPU];

	if (offset == -1) { /* ddr case */
		clk->rate = clk->parent->rate;
		return 0;
	}

	reg_cfg0 = CLK_READ(SYS_SERVICE_ADDR + offset);
	reg_cfg1 = CLK_READ(SYS_SERVICE_ADDR + offset + 4);
	reg_cfg2 = CLK_READ(SYS_SERVICE_ADDR + offset + 8);

	if (enable)
		reg_cfg2 |= (1 << 4);
	else
		reg_cfg2 &= ~(1 << 4);

	pll_hw_set(offset, reg_cfg0, reg_cfg1, reg_cfg2);

	clk->rate =
	    (enable ? pll_hw_evaluate(clk->parent->rate, clk->id - PLL_CPU) :
	     0);
	return 0;
}

static int clkgen_pll_enable(clk_t *clk)
{
	return clkgen_pll_xxable(clk, 1);
}

static int clkgen_pll_disable(clk_t *clk)
{
	return clkgen_pll_xxable(clk, 0);
}

static const unsigned long fs_cfg_offset[] = {
	CLK_SPARE_SETUP0,
	CLK_PCM_SETUP0,
	CLK_SPDIF_SETUP0,
	CLK_DSS_SETUP0,
	CLK_PIX_SETUP0,
	CLK_FDMA_FS_SETUP0,
	CLK_AUX_SETUP0,
	CLK_USB_SETUP0,
};

/*==========================================================
Name:		clkgen_fs_nit
description	Sets the parent of the FS channel and recalculates its freq
===========================================================*/
static int clkgen_fs_init(clk_t *clk)
{
	if (!clk || clk->id < FSA_SPARE || clk->id > FSB_USB)
		return CLK_ERR_BAD_PARAMETER;

	clk->parent = &clk_clocks[OSC_REF];
	if (clk->id == FSA_SPARE || clk->id == FSB_PIX) {
		unsigned long fs_setup;
/* really horrible but it seems the FS requires a
 * initialization sequence....
 * Info acquired from target_pack
 */
		fs_setup = (clk->id < FSB_PIX ? FSA_SETUP : FSB_SETUP);
		sys_service_lock(0);
		CLK_WRITE(SYS_SERVICE_ADDR + fs_setup, (1 << 3));
		CLK_WRITE(SYS_SERVICE_ADDR + fs_setup,
			(1 << 3) | (1 << 4) | (0xF << 8));
		sys_service_lock(1);
	}
	clkgen_fs_recalc(clk);
	return 0;
}

/*==========================================================
Name:		clkgen_fs_set_rate
description	Sets the freq of the FS channels
===========================================================*/
static int clkgen_fs_set_rate(clk_t *clk, unsigned long rate)
{
	int md, pe, sdiv, val;
	unsigned long setup0, used_dco = 0;

	if (!clk || clk->id < FSA_SPARE || clk->id > FSB_USB)
		return CLK_ERR_BAD_PARAMETER;

	if ((clk_fsyn_get_params(clk->parent->rate, rate, &md, &pe, &sdiv)) !=
	    0)
		return CLK_ERR_BAD_PARAMETER;

	setup0 = fs_cfg_offset[clk->id - FSA_SPARE];

	md &= 0x1f;	/* fix sign */
	sdiv &= 0x7;	/* fix sign */
	pe &= 0xffff;	/* fix sign */

	val = md | (sdiv << 6);/* set [md, sdiv] */
	val |= FS_SEL_OUT | FS_OUT_ENABLED;

	sys_service_lock(0);

	if (clk->id == FSA_PCM || clk->id == FSA_SPDIF || clk->id == FSB_PIX) {
		CLK_WRITE(SYS_SERVICE_ADDR + DCO_MODE_CFG, 0);
		used_dco++;
		}

	CLK_WRITE(SYS_SERVICE_ADDR + setup0 + 4, pe);
	CLK_WRITE(SYS_SERVICE_ADDR + setup0, val);
	CLK_WRITE(SYS_SERVICE_ADDR + setup0, val | FS_PROG_EN);
	mdelay(10);
	if (used_dco) {
		CLK_WRITE(SYS_SERVICE_ADDR + DCO_MODE_CFG, 1 | FS_PROG_EN);
		CLK_WRITE(SYS_SERVICE_ADDR + DCO_MODE_CFG, 0);
		}

	CLK_WRITE(SYS_SERVICE_ADDR + setup0, val);
	sys_service_lock(1);

	clk->rate = rate;
	return 0;
}

/*==========================================================
Name:		clkgen_fs_clk_enable
description	enables the FS channels
===========================================================*/
static int clkgen_fs_enable(clk_t *clk)
{
	unsigned long fs_setup, fs_value;
	unsigned long setup0, setup0_value;

	if (!clk || clk->id < FSA_SPARE || clk->id > FSB_USB)
		return CLK_ERR_BAD_PARAMETER;

	fs_setup = (clk->id < FSB_PIX ? FSA_SETUP : FSB_SETUP);
	fs_value = CLK_READ(SYS_SERVICE_ADDR + fs_setup);

	/* not reset */
	fs_value |= FS_NOT_RESET;

	/* enable analog part in fbX_setup */
	fs_value &=  ~FS_ANALOG_POFF;

	/* enable i-th digital part in fbX_setup */
	fs_value |= FS_DIGITAL_PON((clk->id - FSA_SPARE) % 4);

	setup0 = fs_cfg_offset[clk->id - FSA_SPARE];
	setup0_value = CLK_READ(SYS_SERVICE_ADDR + setup0);
	setup0_value |= FS_SEL_OUT | FS_OUT_ENABLED;

	sys_service_lock(0);
	CLK_WRITE(SYS_SERVICE_ADDR + fs_setup, fs_value);
	CLK_WRITE(SYS_SERVICE_ADDR + setup0, setup0_value);
	sys_service_lock(1);

	clkgen_fs_recalc(clk);
	return 0;
}

/*==========================================================
Name:		clkgen_fs_clk_disable
description	disables the individual channels of the FSA
===========================================================*/
static int clkgen_fs_disable(clk_t *clk)
{
	unsigned long setup0, tmp, fs_setup;

	if (!clk || clk->id < FSA_SPARE || clk->id > FSB_USB)
		return CLK_ERR_BAD_PARAMETER;

	setup0 = fs_cfg_offset[clk->id - FSA_SPARE];

	sys_service_lock(1);

	tmp = CLK_READ(SYS_SERVICE_ADDR + setup0);
	/* output disabled */
	CLK_WRITE(SYS_SERVICE_ADDR + setup0, tmp & ~FS_OUT_ENABLED);

	fs_setup = (clk->id < FSB_PIX ? FSA_SETUP : FSB_SETUP);
	tmp = CLK_READ(SYS_SERVICE_ADDR + fs_setup);
	/* disable the i-th digital part */
	tmp &= ~FS_DIGITAL_PON((clk->id - FSA_SPARE) % 4);

	if ((tmp & (0xf << 8)) == (0xf << 8))
		/* disable analog and digital parts */
		CLK_WRITE(SYS_SERVICE_ADDR + fs_setup, tmp | FS_ANALOG_POFF);
	else
		/* disable only digital part */
		CLK_WRITE(SYS_SERVICE_ADDR + fs_setup, tmp);

	sys_service_lock(1);
	clk->rate = 0;
	return 0;
}

/*==========================================================
Name:		clkgen_fs_recalc
description	Tells the programmed freq of the FS channel
===========================================================*/
static int clkgen_fs_recalc(clk_t *clk)
{
	unsigned long md = 0x1f;
	unsigned long sdiv = 0x1C0;
	unsigned long pe = 0xFFFF;
	unsigned long setup0, val;

	if (!clk || clk->id < FSA_SPARE || clk->id > FSB_USB)
		return CLK_ERR_BAD_PARAMETER;

	setup0 = fs_cfg_offset[clk->id - FSA_SPARE];

	val = CLK_READ(SYS_SERVICE_ADDR + setup0);
	md &= val;	/* 0-4 bits */
	sdiv &= val;	/* 6-8 bits */
	sdiv >>= 6;
	pe &= CLK_READ(SYS_SERVICE_ADDR + setup0 + 4);
	clk->rate = clk_fsyn_get_rate(clk->parent->rate, pe, md, sdiv);
	return 0;
}

/*********************************************************************

Functions to observe the clk on the test point provided on the
	board-Debug Functions

**********************************************************************/
static int clkgen_fs_observe(clk_t *clk, unsigned long *freq)
{
	static const unsigned long obs_table[] =
		{ 11, 12, 13, 14, 15, 16, -1, 17 };
	unsigned long clk_out_sel = 0;

	if (!clk || clk->id < FSA_SPARE || clk->id > FSB_USB)
		return CLK_ERR_BAD_PARAMETER;

	clk_out_sel = obs_table[clk->id - FSA_SPARE];

	sys_service_lock(0);
	if (clk_out_sel == -1)	/* o_f_synth_6 */
		clk_out_sel = 0;
	else
		clk_out_sel |= (1 << 5);
	CLK_WRITE(SYS_SERVICE_ADDR + CLK_OBS_CFG, clk_out_sel);
	sys_service_lock(1);
	return 0;

}

static int clkgen_pll_observe(clk_t *clk, unsigned long *divd)
{
	unsigned long clk_out_sel;

	if (!clk)
		return CLK_ERR_BAD_PARAMETER;

	if (clk->id < PLL_CPU || clk->id > PLL_ST40_PCK)
		return CLK_ERR_FEATURE_NOT_SUPPORTED;

	clk_out_sel = clk->id - PLL_CPU;

	sys_service_lock(0);
	CLK_WRITE(SYS_SERVICE_ADDR + CLK_OBS_CFG, clk_out_sel | (1 << 5));
	sys_service_lock(1);
	return 0;
}
