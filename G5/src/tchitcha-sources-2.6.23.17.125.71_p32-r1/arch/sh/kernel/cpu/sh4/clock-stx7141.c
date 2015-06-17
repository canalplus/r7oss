/*************************************************************************
 * CLOCK driver
 * Low Level API - 7141 specific implementation
 *************************************************************************
 *************************************************************************
 *
 * File name   :clock-stx7141.c
 *
 * Description :low level api for 7141 specific implementation
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 ************************************************************************/

/* ----- Modification history (most recent first)----
 - 10 July 2009 - Review by Francesco Virlinzi
   - applyed all the LLA rules
   - Linux compliant

 -  8 July 2009 - First Draft - ...
 *****************/

#include <linux/stm/sysconf.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/stm/clk.h>
#include "clock-regs-stx7141.h"
#include "clock-stx7141.h"

#include "clock-oslayer.h"
#include "clock-common.h"

static int clkgena_observe(clk_t *clk_p, unsigned long *div_p);
static int clkgenb_observe(clk_t *clk_p, unsigned long *div_p);
static int clkgena_set_parent(clk_t *clk_p, clk_t *src_p);
static int clkgenb_set_parent(clk_t *clk_p, clk_t *src_p);
static int clkgenc_set_parent(clk_t *clk_p, clk_t *src_p);
static int clkgend_set_parent(clk_t *clk_p, clk_t *src_p);
static int clkgene_set_parent(clk_t *clk_p, clk_t *src_p);
static int clkgena_set_freq(clk_t *clk_p, U32 freq);
static int clkgenb_set_freq(clk_t *clk_p, U32 freq);
static int clkgena_set_div(clk_t *clk_p, U32 *div_p);
static int clkgenb_set_div(clk_t *clk_p, U32 *div_p);
static int clkgenb_set_fsclock(clk_t *clk_p, U32 freq);
static int clkgena_recalc(clk_t *clk_p);
static int clkgenb_recalc(clk_t *clk_p);
static int clkgenc_recalc(clk_t *clk_p);
static int clkgend_recalc(clk_t *clk_p);
static int clkgene_recalc(clk_t *clk_p);	/* Added to get infos for USB */
static int clkgena_enable(clk_t *clk_p);
static int clkgenb_enable(clk_t *clk_p);
static int clkgenc_enable(clk_t *clk_p);
static int clkgena_disable(clk_t *clk_p);
static int clkgenb_disable(clk_t *clk_p);
static int clkgenc_disable(clk_t *clk_p);
static unsigned long clkgena_get_measure(clk_t *clk_p);
static int clktop_init(clk_t *clk_p);
static int clkgena_init(clk_t *clk_p);
static int clkgenb_init(clk_t *clk_p);
static int clkgenc_init(clk_t *clk_p);
static int clkgenc_set_rate(clk_t *clk_p, U32 freq);
static int clkgend_init(clk_t *clk_p);
static int clkgene_init(clk_t *clk_p);

/* Per boards top input clocks. mb618 & mb636 currently identical */
static U32 OSC_CLKOSC = 30;	/* USB/lp osc */

typedef struct fsyn_regs {
	unsigned long md, pe, sdiv, prog;
} fsyn_regs_t;

static U32 clkgena_offset_regs[] = {
	CKGA_OSC_DIV0_CFG,
	CKGA_PLL0HS_DIV0_CFG,
	CKGA_PLL0LS_DIV0_CFG,
	CKGA_PLL1_DIV0_CFG
};

static struct fsyn_regs clkgenb_regs[] = {
	{CKGB_FS0_MD1, CKGB_FS0_PE1, CKGB_FS0_SDIV1, CKGB_FS0_EN_PRG1},
	{CKGB_FS0_MD2, CKGB_FS0_PE2, CKGB_FS0_SDIV2, CKGB_FS0_EN_PRG2},
	{CKGB_FS0_MD3, CKGB_FS0_PE3, CKGB_FS0_SDIV3, CKGB_FS0_EN_PRG3},
	{CKGB_FS0_MD4, CKGB_FS0_PE4, CKGB_FS0_SDIV4, CKGB_FS0_EN_PRG4},
	{CKGB_FS1_MD1, CKGB_FS1_PE1, CKGB_FS1_SDIV1, CKGB_FS1_EN_PRG1},
	{CKGB_FS1_MD2, CKGB_FS1_PE2, CKGB_FS1_SDIV2, CKGB_FS1_EN_PRG2},
	{CKGB_FS1_MD3, CKGB_FS1_PE3, CKGB_FS1_SDIV3, CKGB_FS1_EN_PRG3},
	{CKGB_FS1_MD4, CKGB_FS1_PE4, CKGB_FS1_SDIV4, CKGB_FS1_EN_PRG4},
};

static struct fsyn_regs clkgenc_regs[] = {
	{CKGC_FS0_MD1, CKGC_FS0_PE1, CKGC_FS0_SDIV1, CKGC_FS0_EN_PRG1},
	{CKGC_FS0_MD2, CKGC_FS0_PE2, CKGC_FS0_SDIV2, CKGC_FS0_EN_PRG2},
	{CKGC_FS0_MD3, CKGC_FS0_PE3, CKGC_FS0_SDIV3, CKGC_FS0_EN_PRG3},
	{CKGC_FS0_MD4, CKGC_FS0_PE4, CKGC_FS0_SDIV4, CKGC_FS0_EN_PRG4}
};

/* Possible operations registration.
   Operations are usually grouped by clockgens due to specific HW implementation

   Name, Desc, init, set_parent, Setfreq, recalc, enable, disable, Observe, Meas

   where
     Name: MUST be the same one declared with REGISTER_CLK (ops field).
     Desc: Clock group short description. Ex: "clockgen A", "USB", "LMI"
     init: Clock init function (read HW to identify parent & compute rate).
     set_parent: Parent/src setup function.
     Setfreq: Clock frequency setup function.
     enable: Clock enable function.
     disable: Clock disable function.
     Observe: Clock observation function.
     recalc: Clock frequency recompute function. Called when parent clock change
     Measure: Clock measure function (when HW available).

   Note: If no capability, put NULL instead of function name.
   Note: All functions should return 'clk_err_t'. */

REGISTER_OPS(Top, "Top clocks",
	clktop_init,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,	/* No measure function */
	NULL
);
REGISTER_OPS(ClockGenA,
	"clockgen A",
	clkgena_init,
	clkgena_set_parent,
	clkgena_set_freq,
	clkgena_recalc,
	clkgena_enable,
	clkgena_disable,
	clkgena_observe,
	clkgena_get_measure,
	NULL
);
REGISTER_OPS(ClockGenB,
	"clockgen B",
	clkgenb_init,
	clkgenb_set_parent,
	clkgenb_set_freq,
	clkgenb_recalc,
	clkgenb_enable,
	clkgenb_disable,
	clkgenb_observe,
	NULL,	/* No measure function */
	NULL
);
REGISTER_OPS(ClockGenC,
	"C/Audio",
	clkgenc_init,
	clkgenc_set_parent,
	clkgenc_set_rate,
	clkgenc_recalc,
	clkgenc_enable,
	clkgenc_disable,
	NULL,
	NULL,	/* No measure function */
	NULL
);
REGISTER_OPS(ClockGenD,
	"D/LMI",
	clkgend_init,
	clkgend_set_parent,
	NULL,
	clkgend_recalc,
	NULL,
	NULL,
	NULL,
	NULL,	/* No measure function */
	NULL
);
REGISTER_OPS(ClockGenE,
	"USB",
	clkgene_init,
	clkgene_set_parent,
	NULL,
	clkgene_recalc,
	NULL,
	NULL,
	NULL,
	NULL,	/* No measure function */
	NULL
);

/* Physical clocks description */
clk_t clk_clocks[] = {
	/*	    ClkID	       Ops	 Nominalfreq   Flags */

	/* Top level clocks */
	REGISTER_CLK(CLK_SATA_OSC, &Top, 0,
		     CLK_RATE_PROPAGATES | CLK_ALWAYS_ENABLED),
	REGISTER_CLK(CLK_SYSALT, &Top, 0,
		     CLK_RATE_PROPAGATES | CLK_ALWAYS_ENABLED),

	/* Clockgen A */
	REGISTER_CLK(CLKA_REF, &ClockGenA, 0,
		     CLK_RATE_PROPAGATES | CLK_ALWAYS_ENABLED),
	REGISTER_CLK(CLKA_PLL0HS, &ClockGenA, 900000000, CLK_RATE_PROPAGATES),
	REGISTER_CLK(CLKA_PLL0LS, &ClockGenA, 450000000, CLK_RATE_PROPAGATES),
	REGISTER_CLK(CLKA_PLL1, &ClockGenA, 800000000, CLK_RATE_PROPAGATES),
	REGISTER_CLK(CLKA_OSC, &ClockGenA, 30000000, CLK_RATE_PROPAGATES),

	REGISTER_CLK(CLKA_IC_STNOC, &ClockGenA, 400000000, 0),
	REGISTER_CLK(CLKA_FDMA0, &ClockGenA, 400000000, 0),
	REGISTER_CLK(CLKA_FDMA1, &ClockGenA, 400000000, 0),
	REGISTER_CLK(CLKA_FDMA2, &ClockGenA, 400000000, 0),
	REGISTER_CLK(CLKA_SH4_ICK, &ClockGenA, 450000000, 0),
	REGISTER_CLK(CLKA_SH4_ICK_498, &ClockGenA, 450000000, 0),
	REGISTER_CLK(CLKA_LX_DMU_CPU, &ClockGenA, 450000000, 0),
	REGISTER_CLK(CLKA_LX_AUD_CPU, &ClockGenA, 450000000, 0),
	REGISTER_CLK(CLKA_IC_BDISP_200, &ClockGenA, 200000000, 0),
	REGISTER_CLK(CLKA_IC_DISP_200, &ClockGenA, 200000000, 0),
	REGISTER_CLK(CLKA_IC_IF_100, &ClockGenA, 100000000, 0),
	REGISTER_CLK(CLKA_DISP_PIPE_200, &ClockGenA, 200000000, 0),
	REGISTER_CLK(CLKA_BLIT_PROC, &ClockGenA, 266666666, 0),
	REGISTER_CLK(CLKA_ETH_PHY, &ClockGenA, 25000000, 0),
	REGISTER_CLK(CLKA_PCI, &ClockGenA, 66666666, 0),
	REGISTER_CLK(CLKA_EMI_MASTER, &ClockGenA, 100000000, 0),
	REGISTER_CLK(CLKA_IC_COMPO_200, &ClockGenA, 200000000, 0),
	REGISTER_CLK(CLKA_IC_IF_200, &ClockGenA, 200000000, 0),

	/* Clockgen B */
	REGISTER_CLK(CLKB_REF, &ClockGenB, 0,
		     CLK_RATE_PROPAGATES | CLK_ALWAYS_ENABLED),
	REGISTER_CLK(CLKB_FS0_CH1, &ClockGenB, 0, CLK_RATE_PROPAGATES),
	REGISTER_CLK(CLKB_FS0_CH2, &ClockGenB, 0, CLK_RATE_PROPAGATES),
	REGISTER_CLK(CLKB_FS0_CH3, &ClockGenB, 0, CLK_RATE_PROPAGATES),
	REGISTER_CLK(CLKB_FS0_CH4, &ClockGenB, 0, CLK_RATE_PROPAGATES),
	REGISTER_CLK(CLKB_FS1_CH1, &ClockGenB, 0, CLK_RATE_PROPAGATES),
	REGISTER_CLK(CLKB_FS1_CH2, &ClockGenB, 0, CLK_RATE_PROPAGATES),
	REGISTER_CLK(CLKB_FS1_CH3, &ClockGenB, 0, CLK_RATE_PROPAGATES),
	REGISTER_CLK(CLKB_FS1_CH4, &ClockGenB, 0, CLK_RATE_PROPAGATES),

	REGISTER_CLK(CLKB_TMDS_HDMI, &ClockGenB, 0, 0),
	REGISTER_CLK(CLKB_PIX_HD, &ClockGenB, 0, 0),
	REGISTER_CLK(CLKB_DISP_HD, &ClockGenB, 0, 0),
	REGISTER_CLK(CLKB_656, &ClockGenB, 0, 0),
	REGISTER_CLK(CLKB_GDP3, &ClockGenB, 0, 0),
	REGISTER_CLK(CLKB_DISP_ID, &ClockGenB, 0, 0),
	REGISTER_CLK(CLKB_PIX_SD, &ClockGenB, 0, 0),

	REGISTER_CLK(CLKB_DSS, &ClockGenB, 0, 0),
	REGISTER_CLK(CLKB_PP, &ClockGenB, 0, 0),
	REGISTER_CLK(CLKB_150, &ClockGenB, 0, 0),
	REGISTER_CLK(CLKB_LPC, &ClockGenB, 0, 0),

	/* Clockgen C (CKGCIO) */
	REGISTER_CLK(CLKC_REF, &ClockGenC, 0,
		     CLK_RATE_PROPAGATES | CLK_ALWAYS_ENABLED),
	REGISTER_CLK(CLKC_FS0_CH1, &ClockGenC, 0, 0),
	REGISTER_CLK(CLKC_FS0_CH2, &ClockGenC, 0, 0),
	REGISTER_CLK(CLKC_FS0_CH3, &ClockGenC, 0, 0),
	REGISTER_CLK(CLKC_FS0_CH4, &ClockGenC, 0, 0),

	/* Clockgen D (LMI) */
	REGISTER_CLK(CLKD_REF, &ClockGenD, 30000000,
		     CLK_RATE_PROPAGATES | CLK_ALWAYS_ENABLED),
	REGISTER_CLK(CLKD_LMI2X, &ClockGenD, 800000000, 0),

	/* Clockgen E (USB), not really a clockgen */
	REGISTER_CLK(CLKE_REF, &ClockGenE, 30000000, CLK_ALWAYS_ENABLED),
};

GENERIC_LINUX_CLKS(clk_clocks[CLKA_SH4_ICK],	/* sh4_clk parent    */
		   clk_clocks[CLKA_IC_IF_100],	/* module_clk parent */
		   clk_clocks[CLKA_IC_IF_100]);	/* comms_clk parent  */

SYSCONF(SYS_STA, 1, 0, 0);
SYSCONF(SYS_CFG, 6, 0, 0);
SYSCONF(SYS_CFG, 11, 1, 8);
SYSCONF(SYS_CFG, 11, 9, 11);
SYSCONF(SYS_CFG, 40, 0, 0);
SYSCONF(SYS_CFG, 40, 2, 2);

/*
 * The Linux clk_init function
 */
int __init clk_init(void)
{
	int i;

	SYSCONF_CLAIM(SYS_STA, 1, 0, 0);
	SYSCONF_CLAIM(SYS_CFG, 6, 0, 0);
	SYSCONF_CLAIM(SYS_CFG, 11, 1, 8);
	SYSCONF_CLAIM(SYS_CFG, 11, 9, 11);
	SYSCONF_CLAIM(SYS_CFG, 40, 0, 0);
	SYSCONF_CLAIM(SYS_CFG, 40, 2, 2);

	for (i = 0; i < ARRAY_SIZE(clk_clocks); ++i)
		if (clk_clocks[i].name && !clk_register(&clk_clocks[i]))
			clk_enable(&clk_clocks[i]);

	REGISTER_GENERIC_LINUX_CLKS();

	return 0;
}

/******************************************************************************
Top level clocks group
******************************************************************************/

/* ========================================================================
   Name:	clkgena_init
   Description: Read HW status to initialize 'clk_t' structure.
   Returns:     'clk_err_t' error code.
   ======================================================================== */

static int clktop_init(clk_t *clk_p)
{
	if (!clk_p)
		return (CLK_ERR_BAD_PARAMETER);

	/* Top recalc function */
	switch (clk_p->id) {
	case CLK_SATA_OSC:
		clk_p->rate = OSC_CLKOSC * 1000000;
		break;
	case CLK_SYSALT:
		clk_p->rate = OSC_CLKOSC * 1000000;
		break;

	default:
		clk_p->rate = 0;
		break;
	}

	return (CLK_ERR_NONE);
}

/******************************************************************************
CLOCKGEN A (CPU+interco+comms) clocks group
******************************************************************************/

/* ========================================================================
Name:	clkgena_get_index
Description: Returns index of given clockgenA clock
Returns:     idx==-1 if error, else >=0
======================================================================== */

static int clkgena_get_index(clk_id_t clkid, U32 *srcreg, int *shift)
{
	int idx;
	/* Warning: This functions assumes clock IDs are perfectly
	   following real implementation order. Each "hole" has therefore
	   to be filled with "CLKx_NOT_USED" */
	if (clkid < CLKA_IC_STNOC || clkid > CLKA_IC_IF_200)
		return -1;

	if (clkid >= CLKA_BLIT_PROC)
		idx = clkid - CLKA_IC_STNOC - 1;
	else
		idx = clkid - CLKA_IC_STNOC;

	if (idx <= 15) {
		*srcreg = CKGA_CLKOPSRC_SWITCH_CFG;
		*shift = idx * 2;
	} else {
		*srcreg = CKGA_CLKOPSRC_SWITCH_CFG2;
		*shift = (idx - 16) * 2;
	}

	return (idx);
}

/* ========================================================================
   Name:	clkgena_set_parent
   Description: Set clock source for clockgenA when possible
   Returns:     0=NO error
   ======================================================================== */

static int clkgena_set_parent(clk_t *clk_p, clk_t *src_p)
{
	U32 clk_src, val;
	int idx;
	U32 srcreg;
	int shift;

	if (!clk_p || !src_p)
		return CLK_ERR_BAD_PARAMETER;

	switch (src_p->id) {
	case CLKA_OSC:
		clk_src = 0;
		break;
	case CLKA_PLL0LS:
	case CLKA_PLL0HS:
		clk_src = 1;
		break;
	case CLKA_PLL1:
		clk_src = 2;
		break;
	default:
		return CLK_ERR_BAD_PARAMETER;
	}

	idx = clkgena_get_index(clk_p->id, &srcreg, &shift);
	if (idx == -1)
		return (CLK_ERR_BAD_PARAMETER);

	val = CLK_READ(CKGA_BASE_ADDRESS + srcreg) & ~(0x3 << shift);
	val = val | (clk_src << shift);
	CLK_WRITE(CKGA_BASE_ADDRESS + srcreg, val);

	clk_p->parent = &clk_clocks[src_p->id];
	clkgena_recalc(clk_p);

	return 0;
}

/* ========================================================================
   Name:	clkgena_identify_parent
   Description: Identify parent clock for clockgen A clocks.
   Returns:     Pointer on parent 'clk_t' structure.
   ======================================================================== */

static int clkgena_identify_parent(clk_t *clk_p)
{
	int idx;
	U32 src_sel;
	U32 srcreg;
	int shift;

	if (!clk_p)
		return (CLK_ERR_BAD_PARAMETER);

	clk_p->parent = NULL;

	switch (clk_p->id) {
	case CLKA_REF:
		src_sel = SYSCONF_READ(SYS_STA, 1, 0, 0);
		switch (src_sel) {
		case 0:
			clk_p->parent = &clk_clocks[CLK_SATA_OSC];
			break;
		case 1:
			clk_p->parent = &clk_clocks[CLK_SYSALT];
			break;
		default:
			break;
		}
		return (0);

	case CLKA_PLL0LS:/* PLLs of clockgen A all sourced from CLKA_REF */
	case CLKA_PLL0HS:
	case CLKA_PLL1:
		clk_p->parent = &clk_clocks[CLKA_REF];
		return (0);
	}

	/* Which divider to setup ? */
	idx = clkgena_get_index(clk_p->id, &srcreg, &shift);
	if (idx == -1)
		return (CLK_ERR_BAD_PARAMETER);

	/* Identifying source */
	src_sel = (CLK_READ(CKGA_BASE_ADDRESS + srcreg) >> shift) & 0x3;
	switch (src_sel) {
	case 0:
		clk_p->parent = &clk_clocks[CLKA_OSC];
		break;
	case 1:
		if (idx <= 3)
			clk_p->parent = &clk_clocks[CLKA_PLL0HS];
		else
			clk_p->parent = &clk_clocks[CLKA_PLL0LS];
		break;
	case 2:
		clk_p->parent = &clk_clocks[CLKA_PLL1];
		break;
	case 3:
		clk_p->parent = NULL;
		clk_p->rate = 0;
		break;
	}

	return (0);
}

/* ========================================================================
   Name:	clkgena_enable
   Description: enable/disable clock
   Returns:     O=NO error
   ======================================================================== */

static int clkgena_enable(clk_t *clk_p)
{

	if (!clk_p)
		return CLK_ERR_BAD_PARAMETER;
	if (!clk_p->parent)
		return CLK_ERR_BAD_PARAMETER;

	return clkgena_set_parent(clk_p, clk_p->parent);
}

/* ========================================================================
   Name:	clkgena_disable
   Description: disable clock
   Returns:     'clk_err_t' error code
   ======================================================================== */

static int clkgena_disable(clk_t *clk_p)
{
	U32 clk_src, val;
	int idx;
	U32 srcreg;
	int shift;

	if (!clk_p)
		return CLK_ERR_BAD_PARAMETER;
	if (!clk_p->parent)
		/* Unsupported. Need to use set_parent() first. */
		return CLK_ERR_BAD_PARAMETER;

	/* IC_IF_100, IC_IF_200, and CPU cannot be disabled */
	if ((clk_p->id == CLKA_IC_IF_100) || (clk_p->id == CLKA_IC_IF_200)
	    || (clk_p->id == CLKA_SH4_ICK))
		return (0);

	idx = clkgena_get_index(clk_p->id, &srcreg, &shift);
	if (idx == -1)
		return CLK_ERR_BAD_PARAMETER;

	/* Disabling clock */
	val = CLK_READ(CKGA_BASE_ADDRESS + srcreg) & ~(0x3 << shift);
	val = val | (3 << shift);	/* 3 = STOP clock */
	CLK_WRITE(CKGA_BASE_ADDRESS + srcreg, val);
	clk_p->rate = 0;

	return (0);
}

/* ========================================================================
   Name:	clkgena_set_div
   Description: Set divider ratio for clockgenA when possible
   ======================================================================== */

static int clkgena_set_div(clk_t *clk_p, U32 *div_p)
{
	int idx;
	U32 div_cfg = 0;
	U32 srcreg, offset;
	int shift;

	if (!clk_p || !clk_p->parent)
		return CLK_ERR_BAD_PARAMETER;

	/* Computing divider config */
	if (*div_p == 1)
		div_cfg = 0x00;
	else
		div_cfg = (*div_p - 1) & 0x1F;

	/* Which divider to setup ? */
	idx = clkgena_get_index(clk_p->id, &srcreg, &shift);
	if (idx == -1)
		return CLK_ERR_BAD_PARAMETER;

	/* Now according to parent, let's write divider ratio */
	offset = clkgena_offset_regs[clk_p->parent->id - CLKA_REF];
	CLK_WRITE(CKGA_BASE_ADDRESS + offset + (4 * idx), div_cfg);
	return (0);
}

/* ========================================================================
   Name:	clkgena_set_freq
   Description: Set clock frequency
   ======================================================================== */

static int clkgena_set_freq(clk_t *clk_p, U32 freq)
{
	U32 div;
	int err;

	if (!clk_p)
		return CLK_ERR_BAD_PARAMETER;

	if (clk_p->id < CLKA_IC_STNOC)
		return 0;
	if (clk_p->id <= CLKA_IC_IF_200)
		if (!clk_p->parent)
			return CLK_ERR_INTERNAL;
	div = clk_p->parent->rate / freq;
	err = clkgena_set_div(clk_p, &div);
	if (!err)
		clk_p->rate = clk_p->parent->rate / div;
	return 0;
}

/* ========================================================================
   Name:	clkgena_recalc
   Description: Get CKGA programmed clocks frequencies
   Returns:     0=NO error
   ======================================================================== */

static int clkgena_recalc(clk_t *clk_p)
{
	U32 data, ratio;
	int idx;
	U32 srcreg, offset;
	int shift;

	if (!clk_p || !clk_p->parent)
		return (CLK_ERR_BAD_PARAMETER);

	/* Cleaning structure first */
	clk_p->rate = 0;

	/* Reading clock programmed value */
	switch (clk_p->id) {
	case CLKA_REF:		/* Clockgen A reference clock */
		clk_p->rate = clk_p->parent->rate;
		break;

	case CLKA_PLL0HS:
	case CLKA_PLL0LS:
		data = CLK_READ(CKGA_BASE_ADDRESS + CKGA_PLL0_CFG);
		clk_p->rate = clk_pll1600_freq(clk_p->parent->rate, data);
		if (clk_p->id == CLKA_PLL0LS)
			clk_p->rate = clk_p->rate / 2;
		break;

	case CLKA_PLL1:
		data = CLK_READ(CKGA_BASE_ADDRESS + CKGA_PLL1_CFG);
		clk_p->rate = clk_pll800_freq(clk_p->parent->rate, data);
		break;

	default:
		if (!clk_p->parent) {
			clk_p->rate = 0;
			return (0);
		}

		idx = clkgena_get_index(clk_p->id, &srcreg, &shift);
		if (idx == -1)
			return (CLK_ERR_BAD_PARAMETER);

		/* Now according to source, let's get divider ratio */
		offset = clkgena_offset_regs[clk_p->parent->id - CLKA_OSC];
		data = CLK_READ(CKGA_BASE_ADDRESS + offset + (4 * idx));

		if ((data & 0x1F) == 0)	/* Div by 1 ? */
			ratio = 1;
		else
			ratio = (data & 0x1F) + 1;

		clk_p->rate = clk_p->parent->rate / ratio;
		break;
	}

	return (0);
}

/* ========================================================================
   Name:	clkgena_observe
   Description: allows to observe a clock on a SYSACLK_OUT
   Returns:     0=NO error
   ======================================================================== */

static int clkgena_observe(clk_t *clk_p, unsigned long *div_p)
{
	U32 src = 0;
	U32 divcfg;
	static U32 obs_table[] = { 8, 9, 0xa, 0xb, 0xc, 0xd, 0xe, 0xf,
		0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19
	};
	if (!clk_p || !div_p)
		return (CLK_ERR_BAD_PARAMETER);

	if (clk_p->id < CLKA_IC_STNOC || clk_p->id > CLKA_IC_IF_200)
		return CLK_ERR_FEATURE_NOT_SUPPORTED;

	src = obs_table[clk_p->id - CLKA_IC_STNOC];

	switch (*div_p) {
	case 2:
		divcfg = 0;
		break;
	case 4:
		divcfg = 1;
		break;
	default:
		divcfg = 2;
		*div_p = 1;
		break;
	}
	CLK_WRITE((CKGA_BASE_ADDRESS + CKGA_CLKOBS_MUX1_CFG),
		  (divcfg << 6) | src);

	return 0;
}

/* ========================================================================
   Name:	clkgena_get_measure
   Description: Use internal HW feature (when avail.) to measure clock
   Returns:     'clk_err_t' error code.
   ======================================================================== */

static unsigned long clkgena_get_measure(clk_t *clk_p)
{
	U32 src, data;
	U32 measure;
	static U32 measure_table[] = {
		8, 9, 0xa, 0xb, 0xc, 0xd, 0xe, 0xf, 0x10, 0x11, 0x12,
		0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19
	};
	if (!clk_p)
		return (0);
	if (clk_p->id < CLKA_IC_STNOC || clk_p->id > CLKA_IC_IF_200)
		return 0;

	src = measure_table[clk_p->id - CLKA_IC_STNOC];
	measure = 0;

	/* Loading the MAX Count 1000 in 30MHz Oscillator Counter */
	/* poke #FE213034 #3E8     */
	CLK_WRITE(CKGA_BASE_ADDRESS + CKGA_CLKOBS_MASTER_MAXCOUNT, 0x3E8);
	CLK_WRITE(CKGA_BASE_ADDRESS + CKGA_CLKOBS_CMD, 3);

	/* Selecting clock to observe */
	CLK_WRITE(CKGA_BASE_ADDRESS + CKGA_CLKOBS_MUX1_CFG, (1 << 7) | src);

	/* Start counting */
	CLK_WRITE(CKGA_BASE_ADDRESS + CKGA_CLKOBS_CMD, 0);

	while (1) {
		task_delay(time_ticks_per_sec() / 100);	/* Every 10ms */

		data = CLK_READ(CKGA_BASE_ADDRESS + CKGA_CLKOBS_STATUS);
		if (data & 1)
			break;	/* IT */
	}

	/* Reading value */
	data = CLK_READ(CKGA_BASE_ADDRESS + CKGA_CLKOBS_SLAVE0_COUNT);
	measure = 30 * data * 1000;

	/* poke #FE213038 #3 */
	CLK_WRITE(CKGA_BASE_ADDRESS + CKGA_CLKOBS_CMD, 3);

	return measure;
}

/* ========================================================================
   Name:	clkgena_init
   Description: Read HW status to initialize 'clk_t' structure.
   Returns:     'clk_err_t' error code.
   ======================================================================== */

static int clkgena_init(clk_t *clk_p)
{
	if (!clk_p)
		return CLK_ERR_BAD_PARAMETER;

	clkgena_identify_parent(clk_p);
	clkgena_recalc(clk_p);

	return 0;
}

/******************************************************************************
CLOCKGEN B
******************************************************************************/

void clkgenb_unlock(void)
{
	CLK_WRITE(CKGB_BASE_ADDRESS + CKGB_LOCK, 0xc0de);
}

void clkgenb_lock(void)
{
	CLK_WRITE(CKGB_BASE_ADDRESS + CKGB_LOCK, 0xc1a0);
}

/* ========================================================================
   Name:	clkgenb_enable
   Description: enable/disable clock
   Returns:     0=NO error
   ======================================================================== */
struct gen_utility {
	U32 clk_id;
	U32 info;
};

static int clkgenb_xxable(clk_t *clk_p, U32 en)
{
	U32 bit, power;
	U32 i;
	struct gen_utility enable_clock[] = {
		{CLKB_DSS, 1},
		{CLKB_PIX_HD, 3},
		{CLKB_DISP_HD, 4},
		{CLKB_TMDS_HDMI, 5},
		{CLKB_656, 6},
		{CLKB_GDP3, 7},
		{CLKB_DISP_ID, 8},
		{CLKB_PIX_SD, 9},
		{CLKB_150, 11},
		{CLKB_PP, 12},
		{CLKB_LPC, 13}
	};

	if (!clk_p)
		return CLK_ERR_BAD_PARAMETER;
	for (i = 0; i < 11; ++i)
		if (enable_clock[i].clk_id == clk_p->id)
			break;
	if (i == 11)
		return CLK_ERR_BAD_PARAMETER;
	power = CLK_READ(CKGB_BASE_ADDRESS + CKGB_POWER_ENABLE);
	bit = enable_clock[i].info;
	clkgenb_unlock();
	if (en) {
		CLK_WRITE(CKGB_BASE_ADDRESS + CKGB_POWER_ENABLE,
			  power | (1 << bit));
	} else {
		CLK_WRITE(CKGB_BASE_ADDRESS + CKGB_POWER_ENABLE,
			  power & ~(1 << bit));
	}
	clkgenb_lock();

	return 0;
}

static int clkgenb_enable(clk_t *clk_p)
{
	return clkgenb_xxable(clk_p, 1);
}

/* ========================================================================
   Name:	clkgenb_disable
   Description: disable clock
   Returns:     O=NO error
   ======================================================================== */

static int clkgenb_disable(clk_t *clk_p)
{
	return clkgenb_xxable(clk_p, 0);
}

/* ========================================================================
   Name:	clkgenb_set_parent
   Description: Set clock source for clockgenB when possible
   Returns:     0=NO error
   ======================================================================== */

static int clkgenb_set_parent(clk_t *clk_p, clk_t *parent_p)
{
	U32 set = 0;		/* Each bit set to 1 will be SETTED */
	U32 reset = 0;		/* Each bit set to 1 will be RESETTED */
	U32 reg;		/* Register address */
	U32 val;

	if (!clk_p || !parent_p)
		return CLK_ERR_BAD_PARAMETER;

	switch (clk_p->id) {
	case CLKB_REF:
		switch (parent_p->id) {
		case CLK_SATA_OSC:
			reset = 1;
			break;
		case CLK_SYSALT:
			set = 1;
			break;
		}
		reg = CKGB_BASE_ADDRESS + CKGB_CRISTAL_SEL;
		break;

		/*  case CLKB_PIX_HD:
		   if ( parent_p->id == CLKB_FS0_CH1 )
		   reset = 1 << 14;
		   else
		   set = 1 << 14;
		   reg = CKGB_BASE_ADDRESS+CKGB_DISPLAY_CFG;
		   break; */
	case CLKB_GDP3:
		if ((parent_p->id == CLKB_DISP_HD)
		    || (parent_p->id == CLKB_FS0_CH1))
			reset = 1 << 0;
		else
			set = 1 << 0;
		reg = CKGB_BASE_ADDRESS + CKGB_FS_SELECT;
		break;

	case CLKB_PIX_SD:
		if (parent_p->id == CLKB_FS0_CH1)
			reset = 1 << 1;
		else
			set = 1 << 1;
		reg = CKGB_BASE_ADDRESS + CKGB_FS_SELECT;
		break;

	case CLKB_PIP:
		/* In fact NOT a clockgen B clock but closely linked to it */
		if (parent_p->id == CLKB_DISP_ID)
			val = 0;
		else if (parent_p->id == CLKB_DISP_HD)
			val = 1;
		else
			return CLK_ERR_BAD_PARAMETER;
		SYSCONF_WRITE(SYS_CFG, 6, 0, 0, val);
		clk_p->parent = parent_p;
		/* Special case since config done thru sys_conf register */
		return 0;

	default:
		return CLK_ERR_BAD_PARAMETER;
	}

	clkgenb_unlock();
	val = CLK_READ(reg);
	val = val & ~reset;
	val = val | set;
	CLK_WRITE(reg, val);
	clkgenb_lock();
	clk_p->parent = parent_p;

	return 0;
}

/* ========================================================================
   Name:	clkgenb_set_freq
   Description: Set clock frequency
   ======================================================================== */

static int clkgenb_set_freq(clk_t *clk_p, U32 freq)
{
	U32 div;

	if (!clk_p)
		return CLK_ERR_BAD_PARAMETER;

	switch (clk_p->id) {
	case CLKB_FS0_CH1:
	case CLKB_FS0_CH2:
	case CLKB_FS0_CH3:
	case CLKB_FS0_CH4:
	case CLKB_FS1_CH1:
	case CLKB_FS1_CH2:
	case CLKB_FS1_CH3:
	case CLKB_FS1_CH4:
		return clkgenb_set_fsclock(clk_p, freq);
	default:
		if (!clk_p->parent)
			/* A parent is expected to these clocks */
			return CLK_ERR_INTERNAL;
		div = clk_p->parent->rate / freq;
		return clkgenb_set_div(clk_p, &div);
	}

	return 0;
}

/* ========================================================================
   Name:	clkgenb_set_fsclock
   Description: Set FS clock
   Returns:     0=NO error
   ======================================================================== */

static int clkgenb_set_fsclock(clk_t *clk_p, U32 freq)
{
	int md, pe, sdiv;
	struct fsyn_regs *regs;
	if (!clk_p || !clk_p->parent)
		return (CLK_ERR_BAD_PARAMETER);

	/* Computing FSyn params. Should be common function with FSyn type */
	if ((clk_fsyn_get_params
	     (clk_p->parent->rate, freq, &md, &pe, &sdiv)) != 0)
		return CLK_ERR_BAD_PARAMETER;

	if (clk_p->id < CLKB_FS0_CH1 || clk_p->id > CLKB_FS1_CH4)
		return CLK_ERR_BAD_PARAMETER;

	regs = &clkgenb_regs[clk_p->id - CLKB_FS0_CH1];

	clkgenb_unlock();
	CLK_WRITE(CKGB_BASE_ADDRESS + regs->md, md);
	CLK_WRITE(CKGB_BASE_ADDRESS + regs->pe, pe);
	CLK_WRITE(CKGB_BASE_ADDRESS + regs->sdiv, sdiv);
	CLK_WRITE(CKGB_BASE_ADDRESS + regs->prog, 0x1);
	clkgenb_lock();
	clk_p->rate = freq;
	return 0;
}

/* ========================================================================
   Name:	clkgenb_set_div
   Description: Set divider ratio for clockgenB when possible
   Returns:     0=NO error
   ======================================================================== */

static int clkgenb_set_div(clk_t *clk_p, U32 * div_p)
{
	U32 set = 0;		/* Each bit set to 1 will be SETTED */
	U32 reset = 0;		/* Each bit set to 1 will be RESETTED */
	U32 reg;
	U32 val;
	if (!clk_p)
		return CLK_ERR_BAD_PARAMETER;

	reg = CKGB_DISPLAY_CFG;
	switch (clk_p->id) {
	case CLKB_TMDS_HDMI:
		switch (*div_p) {
		case 1:
			reset = 0x1;
			set = 0x3;
			break;
		case 2:
			reset = 0x3;
			set = 0x3;
			break;
		case 4:
			reset = 1 << 1;
			set = 1 << 0;
			break;
		case 1024:
			reg = CKGB_POWER_DOWN;
			set = 1 << 1;
			break;
		}
		break;
	case CLKB_DISP_HD:
		switch (*div_p) {
		case 1:
			reset = 0x3 << 4;
			set = 0x3 << 4;
			break;
		case 2:
			reset = 0x3 << 4;
			set = 0x0;
			break;
		case 4:
			reset = 1 << 5;
			set = 1 << 4;
			break;
		case 8:
			set = 1 << 5;
			reset = 1 << 4;
			break;
		case 1024:
			reg = CKGB_POWER_DOWN;
			set = 1 << 4;
			break;
		}
		break;
	case CLKB_656:
		switch (*div_p) {
		case 2:
			reset = 0x3 << 6;
			set = 0x3 << 6;
			break;
		case 4:
			reset = 1 << 7;
			set = 1 << 6;
			break;
		case 1024:
			reg = CKGB_POWER_DOWN;
			set = 1 << 5;
			break;
		}
		break;
	case CLKB_DISP_ID:
		switch (*div_p) {
		case 2:
			reset = 0x3 << 8;
			set = 0x3 << 8;
			break;
		case 4:
			reset = 0x1 << 9;
			set = 0x1 << 8;
			break;
		case 8:
			reset = 0x3 << 8;
			set = 0x2 << 8;
			break;
		case 1024:
			reg = CKGB_POWER_DOWN;
			set = 1 << 6;
			break;
		}
		break;
	case CLKB_PIX_SD:

		if (clk_p->parent->id == CLKB_FS0_CH1) {
			switch (*div_p) {/* clk_pix_sd sourced from Fsyn0 */
			case 2:
				reset = 0x3 << 10;
				set = 0;
				break;
			case 4:
				reset = 1 << 11;
				set = 1 << 10;
				break;
			case 1024:
				reg = CKGB_POWER_DOWN;
				set = 1 << 7;
				break;
			}

		} else {
			switch (*div_p) {/* clk_pix_sd sourced from Fsyn1 */
			case 1:
				reset = 0x1 << 13;
				set = 0x3 << 12;
				break;
			case 2:
				reset = 0x3 < 12;
				set = 0;
				break;
			case 4:
				reset = 1 << 13;
				set = 1 << 12;
				break;
			case 1024:
				reg = CKGB_POWER_DOWN;
				set = 1 << 7;
				break;
			}
		}
		break;
	default:
		return CLK_ERR_BAD_PARAMETER;
	}

	val = CLK_READ(CKGB_BASE_ADDRESS + reg);
	val = val & ~reset;
	val = val | set;
	clkgenb_unlock();
	CLK_WRITE(CKGB_BASE_ADDRESS + reg, val);
	clkgenb_lock();

	return 0;
}

/* ========================================================================
   Name:	clkgenb_observe
   Description: Allows to observe a clock on a PIO5_2
   Returns:     0=NO error
   ======================================================================== */

static int clkgenb_observe(clk_t *clk_p, unsigned long *div_p)
{
	static U32 observe_table[] = {
		1,		/* CLKB_TMDS_HDMI    */
		3,		/* CLKB_PIX_HD       */
		4,		/* CLKB_DISP_HD      */
		5,		/* CLKB_656	  */
		6,		/* CLKB_GDP3	 */
		7,		/* CLKB_DISP_ID      */
		8,		/* CLKB_PIX_SD       */
		12,		/* CLKB_PP	   */
		-1,		/* CLKB_150	  */
		13,		/* CLKB_LPC	  */
		9,		/* CLKB_DSS	  */
		-1 /* CLKB_PIP	  */
	};

	U32 out0, out1 = 0;

	if (!clk_p)
		return CLK_ERR_BAD_PARAMETER;

	if (clk_p->id == CLKB_PIP || clk_p->id == CLKB_150)
		return CLK_ERR_BAD_PARAMETER;

	out0 = observe_table[clk_p->id - CLKB_TMDS_HDMI];
	if (clk_p->id == CLKB_PP)
		out1 = 11;

	clkgenb_unlock();
	CLK_WRITE((CKGB_BASE_ADDRESS + CKGB_OUT_CTRL), (out1 << 4) | out0);
	clkgenb_lock();

	/* Set PIO5_2 for observation (alternate function output mode) */
	PIO_SET_MODE(5, 2, STPIO_ALT_OUT);

	/* No possible predivider on clockgen B */
	*div_p = 1;

	return 0;
}

/* ========================================================================
   Name:	clkgenb_recalc
   Description: Get CKGB clocks frequencies function
   Returns:     0=NO error
   ======================================================================== */

/* Check clock enable value for clockgen B.
   Returns: 1=RUNNING, 0=DISABLED */
static int clkgenb_is_running(U32 power, int bit)
{
	if (power & (1 << bit))
		return 1;

	return 0;
}

static int clkgenb_recalc(clk_t *clk_p)
{

	U32 displaycfg, powerdown, fs_sel, power;
	U32 ref;		/* Ref clock in Mhz */
	U32 pe, md, sdiv;
	struct fsyn_regs *regs;

	if (!clk_p || !clk_p->parent)
		return (CLK_ERR_BAD_PARAMETER);

	/* Cleaning structure first */
	clk_p->rate = 0;

	/* Required clocks */
	ref = clk_p->parent->rate / 1000000;

	/* Read mux */
	displaycfg = CLK_READ(CKGB_BASE_ADDRESS + CKGB_DISPLAY_CFG);
	powerdown = CLK_READ(CKGB_BASE_ADDRESS + CKGB_POWER_DOWN);
	fs_sel = CLK_READ(CKGB_BASE_ADDRESS + CKGB_FS_SELECT);
	power = CLK_READ(CKGB_BASE_ADDRESS + CKGB_POWER_ENABLE);

	switch (clk_p->id) {
	case CLKB_REF:		/* What is clockgen B ref clock ? */
		clk_p->rate = clk_p->parent->rate;
		break;

	case CLKB_FS0_CH1...CLKB_FS1_CH4:
		regs = &clkgenb_regs[clk_p->id - CLKB_FS0_CH1];
		md = CLK_READ(CKGB_BASE_ADDRESS + regs->md);
		pe = CLK_READ(CKGB_BASE_ADDRESS + regs->pe);
		sdiv = CLK_READ(CKGB_BASE_ADDRESS + regs->sdiv);
		clk_p->rate =
		    clk_fsyn_get_rate(clk_p->parent->rate, pe, md, sdiv);
		break;

	case CLKB_TMDS_HDMI:	/* tmds_hdmi_clk */
		if (powerdown & (1 << 1)) {
			clk_p->rate = clk_p->parent->rate / 1024;
		} else {
			switch (displaycfg & 0x3) {
			case 0:
			case 3:
				clk_p->rate = clk_p->parent->rate / 2;
				break;
			case 1:
				clk_p->rate = clk_p->parent->rate / 4;
				break;
			default:
				break;
			}
		}
		if (!clkgenb_is_running(power, 5))
			clk_p->rate = 0;
		break;

	case CLKB_PIX_HD:	/* pix_hd */
		if (displaycfg & (1 << 14)) {	/* pix_hd source = FSYN1 */
			clk_p->rate = clk_p->parent->rate;
		} else {	/* pix_hd source = FSYN0 */
			clk_p->rate = clk_p->parent->rate;
		}
		if (powerdown & (1 << 3))
			clk_p->rate = clk_p->rate / 1024;
		if (!clkgenb_is_running(power, 3))
			clk_p->rate = 0;
		break;

	case CLKB_DISP_HD:	/* disp_hd */
		if (powerdown & (1 << 4))
			clk_p->rate = clk_p->parent->rate / 1024;
		else {
			switch ((displaycfg >> 4) & 0x3) {
			case 0:
			case 3:
				clk_p->rate = clk_p->parent->rate / 2;
				break;
			case 1:
				clk_p->rate = clk_p->parent->rate / 4;
				break;
			case 2:
				clk_p->rate = clk_p->parent->rate / 8;
				break;
			default:
				break;
			}
		}
		if (!clkgenb_is_running(power, 4))
			clk_p->rate = 0;
		break;

	case CLKB_656:		/* ccir656_clk */
		if (powerdown & (1 << 5))
			clk_p->rate = clk_p->parent->rate / 1024;
		else {
			switch ((displaycfg >> 6) & 0x3) {
			case 0:
			case 3:
				clk_p->rate = clk_p->parent->rate / 2;
				break;
			case 1:
				clk_p->rate = clk_p->parent->rate / 4;
				break;
			default:
				break;
			}
		}
		if (!clkgenb_is_running(power, 6))
			clk_p->rate = 0;
		break;

	case CLKB_DISP_ID:	/* disp_id */
		if (powerdown & (1 << 6))
			clk_p->rate = clk_p->parent->rate / 1024;
		else {
			switch ((displaycfg >> 8) & 0x3) {
			case 0:
			case 3:
				clk_p->rate = clk_p->parent->rate / 2;
				break;
			default:
				break;
			}
		}
		if (!clkgenb_is_running(power, 8))
			clk_p->rate = 0;
		break;

	case CLKB_PIX_SD:	/* pix_sd */
		/* source is FS0 */
		if (powerdown & (1 << 7))
			clk_p->rate = clk_p->parent->rate / 1024;
		else {
			switch ((displaycfg >> 10) & 0x3) {
			case 1:
				clk_p->rate = clk_p->parent->rate / 4;
				break;
			default:
				break;
			}
		}
		if (!clkgenb_is_running(power, 9))
			clk_p->rate = 0;
		break;

	case CLKB_GDP3:	/* gdp3_clk */
		if (fs_sel & 0x1) {
			/* source is FS1 */
			clk_p->rate = clk_p->parent->rate;
		} else {
			/* source is FS0 */
			clk_p->rate = clk_p->parent->rate;
		}
		if (!clkgenb_is_running(power, 7))
			clk_p->rate = 0;
		break;

	case CLKB_DSS:
		clk_p->rate = clk_p->parent->rate;
		if (!clkgenb_is_running(power, 0))
			clk_p->rate = 0;
		break;

	case CLKB_PP:
		clk_p->rate = clk_p->parent->rate;
		if (!clkgenb_is_running(power, 12))
			clk_p->rate = 0;
		break;

	case CLKB_LPC:
		clk_p->rate = clk_p->parent->rate / 1024;
		if (!clkgenb_is_running(power, 13))
			clk_p->rate = 0;
		break;

	default:
		return CLK_ERR_BAD_PARAMETER;
	}

	return (0);
}

/* ========================================================================
   Name:	clkgenb_get_parent
   Description: Identify parent clock
   Returns:     Pointer on parent 'clk_t' structure, or NULL (none or error)
   ======================================================================== */

static clk_t *clkgenb_get_parent(clk_t *clk_p)
{
	U32 sel, fs_sel;
	U32 displaycfg;

	if (!clk_p)
		return NULL;

	fs_sel = CLK_READ(CKGB_BASE_ADDRESS + CKGB_FS_SELECT);

	switch (clk_p->id) {
	case CLKB_REF:		/* What is clockgen B ref clock ? */
		sel = CLK_READ(CKGB_BASE_ADDRESS + CKGB_CRISTAL_SEL);
		switch (sel & 0x1) {
		case 0:
			clk_p->parent = &clk_clocks[CLK_SATA_OSC];
			break;
		case 1:
			clk_p->parent = &clk_clocks[CLK_SYSALT];
			break;
		default:
			clk_p->parent = NULL;
			break;
		}
		break;

	case CLKB_FS0_CH1:	/* FS0 clock 1 */
	case CLKB_FS0_CH2:	/* FS0 clock 2 */
	case CLKB_FS0_CH3:	/* FS0 clock 3 */
	case CLKB_FS0_CH4:	/* FS0 clock 4 */
	case CLKB_FS1_CH1:	/* FS1 clock 1 */
	case CLKB_FS1_CH2:	/* FS1 clock 2 */
	case CLKB_FS1_CH3:	/* FS1 clock 3 */
	case CLKB_FS1_CH4:	/* FS1 clock 4 */
		clk_p->parent = &clk_clocks[CLKB_REF];
		break;

	case CLKB_TMDS_HDMI:	/* tmds_hdmi_clk */
	case CLKB_DISP_HD:	/* disp_hd */
	case CLKB_656:		/* ccir656_clk */
		clk_p->parent = &clk_clocks[CLKB_FS0_CH1];
		break;

	case CLKB_PIX_HD:	/* pix_hd */
		displaycfg = CLK_READ(CKGB_BASE_ADDRESS + CKGB_DISPLAY_CFG);
		if (displaycfg & (1 << 14)) {	/* pix_hd source = FSYN1 */
			clk_p->parent = &clk_clocks[CLKB_FS1_CH1];
		} else {	/* pix_hd source = FSYN0 */
			clk_p->parent = &clk_clocks[CLKB_FS0_CH1];
		}
		break;

	case CLKB_DISP_ID:	/* disp_id */
		clk_p->parent = &clk_clocks[CLKB_FS1_CH1];
		break;

	case CLKB_PIX_SD:	/* pix_sd */
		if (fs_sel & 0x2)
			/* source is FS1 */
			clk_p->parent = &clk_clocks[CLKB_FS1_CH1];
		else
			/* source is FS0 */
			clk_p->parent = &clk_clocks[CLKB_FS0_CH1];
		break;

	case CLKB_GDP3:	/* gdp3_clk */
		if (fs_sel & 0x1)
			/* source is FS1 */
			clk_p->parent = &clk_clocks[CLKB_FS1_CH1];
		else
			/* source is FS0 */
			clk_p->parent = &clk_clocks[CLKB_FS0_CH1];
		break;

	case CLKB_DSS:
		clk_p->parent = &clk_clocks[CLKB_FS0_CH2];
		break;

	case CLKB_PP:
		clk_p->parent = &clk_clocks[CLKB_FS1_CH3];
		break;

	case CLKB_LPC:
		clk_p->parent = &clk_clocks[CLKB_FS1_CH4];
		break;

	default:
		return NULL;
	}

	return clk_p->parent;
}

/* ========================================================================
   Name:	clkgenb_init
   Description: Read HW status to initialize 'clk_t' structure.
   Returns:     'clk_err_t' error code.
   ======================================================================== */

static int clkgenb_init(clk_t *clk_p)
{
	if (!clk_p)
		return CLK_ERR_BAD_PARAMETER;

	clk_p->parent = clkgenb_get_parent(clk_p);
	clkgenb_recalc(clk_p);

	return 0;
}

/******************************************************************************
CLOCKGEN C (audio)
******************************************************************************/

/* ========================================================================
   Name:	clkgenc_recalc
   Description: Get CKGC clocks frequencies function
   Returns:     0=NO error
   ======================================================================== */

static int clkgenc_recalc(clk_t *clk_p)
{
	int pe, md, sdiv;
	struct fsyn_regs *regs;
	/* Cleaning structure first */
	clk_p->rate = 0;

	/* What is clockgen C ref clock ? */
	/* refclockC = Clk->parent->rate / 1000000; */

	if (clk_p->id == CLKC_REF) {
		clk_p->rate = clk_p->parent->rate;
		return 0;
	}

	regs = &clkgenc_regs[clk_p->id - CLKC_FS0_CH1];

	pe = CLK_READ(CKGC_BASE_ADDRESS + regs->pe);
	md = CLK_READ(CKGC_BASE_ADDRESS + regs->md);
	sdiv = CLK_READ(CKGC_BASE_ADDRESS + regs->sdiv);
	clk_p->rate = clk_fsyn_get_rate(clk_p->parent->rate, pe, md, sdiv);
	return (0);
}

/* ========================================================================
   Name:	clkgenc_identify_parent
   Description: Identify parent clock
   Returns:     Pointer on parent 'clk_t' structure, or NULL (none or error)
   ======================================================================== */

static clk_t *clkgenc_identify_parent(clk_t *clk_p)
{
	U32 sel;

	if (!clk_p)
		return (NULL);

	switch (clk_p->id) {
	case CLKC_REF:
		sel = CLK_READ(CKGC_BASE_ADDRESS + CKGC_FS0_CFG) >> 23;
		switch (sel & 0x1) {
		case 0:
			clk_p->parent = &clk_clocks[CLK_SATA_OSC];
			break;
		case 1:
			clk_p->parent = &clk_clocks[CLK_SYSALT];
			break;
		default:
			clk_p->parent = NULL;
			break;
		}
		break;

	case CLKC_FS0_CH1:
	case CLKC_FS0_CH2:
	case CLKC_FS0_CH3:
	case CLKC_FS0_CH4:
		clk_p->parent = &clk_clocks[CLKC_REF];
		break;

	default:
		clk_p->parent = NULL;
		break;
	}

	return (clk_p->parent);
}

/* ========================================================================
   Name:	clkgenc_set_parent
   Description: Set parent clock
   Returns:     'clk_err_t' error code.
   ======================================================================== */

static int clkgenc_set_parent(clk_t *clk_p, clk_t *parent_p)
{
	U32 sel, data;

	if (!clk_p || !parent_p)
		return (CLK_ERR_BAD_PARAMETER);

	if (clk_p->id == CLKC_REF) {
		switch (parent_p->id) {
		case CLK_SATA_OSC:
			sel = 0;
			break;
		case CLK_SYSALT:
			sel = 1;
			break;
		default:
			return CLK_ERR_BAD_PARAMETER;
		}
		data =
		    CLK_READ(CKGC_BASE_ADDRESS + CKGC_FS0_CFG) & ~(0x3 << 23);
		CLK_WRITE(CKGC_BASE_ADDRESS + CKGC_FS0_CFG, data | (sel << 23));
		clk_p->parent = parent_p;
	} else
		clk_p->parent = &clk_clocks[CLKC_REF];

	return CLK_ERR_BAD_PARAMETER;
}

/* ========================================================================
   Name:	clkgenc_init
   Description: Read HW status to initialize 'clk_t' structure.
   Returns:     'clk_err_t' error code.
   ======================================================================== */

static int clkgenc_init(clk_t *clk_p)
{
	if (!clk_p)
		return CLK_ERR_BAD_PARAMETER;

	clk_p->parent = clkgenc_identify_parent(clk_p);
	clkgenc_recalc(clk_p);

	return 0;
}

/******************************************************************************
CLOCKGEN D (LMI)
******************************************************************************/

/* ========================================================================
   Name:	clkgend_recalc
   Description: Get CKGD (LMI) clocks frequencies (in Hz)
   Returns:     0=NO error
   ======================================================================== */

static int clkgend_recalc(clk_t *clk_p)
{
	U32 data, rdiv, ddiv;

	/* Cleaning structure first */
	clk_p->rate = 0;

	if (clk_p->id == CLKD_REF) {
		clk_p->rate = clk_p->parent->rate;
	} else if (clk_p->id == CLKD_LMI2X) {
		rdiv = SYSCONF_READ(SYS_CFG, 11, 9, 11);
		ddiv = SYSCONF_READ(SYS_CFG, 11, 1, 8);
		clk_p->rate =
		    (((clk_p->parent->rate / 1000000) * ddiv) / rdiv) * 1000000;
	} else
		return CLK_ERR_BAD_PARAMETER;	/* Unknown clock */

	return 0;
}

/* ========================================================================
   Name:	clkgend_get_parent
   Description: Identify parent clock
   Returns:     Pointer on parent 'clk_t' structure, or NULL (none or error)
   ======================================================================== */

static clk_t *clkgend_get_parent(clk_t *clk_p)
{
	U32 sel;

	if (!clk_p)
		return (NULL);

	if (clk_p->id == CLKD_REF) {
		sel = SYSCONF_READ(SYS_CFG, 40, 0, 0);
		if (sel)
			clk_p->parent = &clk_clocks[CLK_SYSALT];
		else
			clk_p->parent = &clk_clocks[CLK_SATA_OSC];
	} else
		clk_p->parent = &clk_clocks[CLKD_REF];

	return clk_p->parent;
}

/* ========================================================================
   Name:	clkgend_set_parent
   Description: Set parent clock
   Returns:     'clk_err_t' error code.
   ======================================================================== */

static int clkgend_set_parent(clk_t *clk_p, clk_t *parent_p)
{
	U32 sel, data;

	if (!clk_p || !parent_p)
		return CLK_ERR_BAD_PARAMETER;

	if (clk_p->id == CLKD_REF) {
		switch (parent_p->id) {
		case CLK_SATA_OSC:
			sel = 0;
			break;
		case CLK_SYSALT:
			sel = 1;
			break;
		default:
			return CLK_ERR_BAD_PARAMETER;
		}

		SYSCONF_WRITE(SYS_CFG, 40, 0, 0, sel);
		clk_p->parent = parent_p;
	} else
		clk_p->parent = &clk_clocks[CLKD_REF];

	return 0;
}

/* ========================================================================
   Name:	clkgend_init
   Description: Read HW status to initialize 'clk_t' structure.
   Returns:     'clk_err_t' error code.
   ======================================================================== */

static int clkgend_init(clk_t *clk_p)
{
	if (!clk_p)
		return CLK_ERR_BAD_PARAMETER;

	clk_p->parent = clkgend_get_parent(clk_p);
	clkgend_recalc(clk_p);

	return 0;
}

/******************************************************************************
CLOCKGEN E (USB)
******************************************************************************/

/* ========================================================================
   Name:	clkgene_recalc
   Description: Get CKGE (USB) clocks frequencies (in Hz)
   Returns:     0=NO error
   ======================================================================== */

static int clkgene_recalc(clk_t *clk_p)
{
	/* Cleaning structure first */
	clk_p->rate = 0;

	if (clk_p->id == CLKE_REF)
		clk_p->rate = clk_p->parent->rate;
	else
		return CLK_ERR_BAD_PARAMETER;	/* Unknown clock */

	return 0;
}

/* ========================================================================
   Name:	clkgene_get_parent
   Description: Identify parent clock
   Returns:     Pointer on parent 'clk_t' structure, or NULL (none or error)
   ======================================================================== */

static clk_t *clkgene_get_parent(clk_t *clk_p)
{
	U32 sel;

	if (!clk_p)
		return (NULL);

	if (clk_p->id == CLKE_REF) {
		sel = SYSCONF_READ(SYS_CFG, 40, 2, 2);
		if (sel)
			clk_p->parent = &clk_clocks[CLK_SYSALT];
		else
			clk_p->parent = &clk_clocks[CLK_SATA_OSC];
	} else
		clk_p->parent = &clk_clocks[CLKE_REF];

	return (clk_p->parent);
}

/* ========================================================================
   Name:	clkgene_set_parent
   Description: Change parent clock
   Returns:     Pointer on parent 'clk_t' structure, or NULL (none or error)
   ======================================================================== */

static int clkgene_set_parent(clk_t *clk_p, clk_t *parent_p)
{
	U32 data, sel;

	if (!clk_p)
		return CLK_ERR_BAD_PARAMETER;

	if (clk_p->id == CLKE_REF) {
		switch (parent_p->id) {
		case CLK_SATA_OSC:
			sel = 0;
			break;
		case CLK_SYSALT:
			sel = 1;
			break;
		default:
			return CLK_ERR_BAD_PARAMETER;
		}

		SYSCONF_WRITE(SYS_CFG, 40, 2, 2, sel);
		clk_p->parent = parent_p;
	} else
		return CLK_ERR_BAD_PARAMETER;

	return (0);
}

/* ========================================================================
   Name:	clkgene_init
   Description: Read HW status to initialize 'clk_t' structure.
   Returns:     'clk_err_t' error code.
   ======================================================================== */

static int clkgene_init(clk_t *clk_p)
{
	if (!clk_p)
		return (CLK_ERR_BAD_PARAMETER);

	clk_p->parent = clkgene_get_parent(clk_p);
	clkgene_recalc(clk_p);

	return (0);
}

static int clkgenc_set_rate(clk_t *clk_p, U32 freq)
{
	int md, pe, sdiv;
	U32 ref;
	struct fsyn_regs *regs;
	U32 reg_value = 0;
	static U32 set_rate_table[] = { 0x06, 0x0A, 0x012, 0x022 };

	if (!clk_p)
		return CLK_ERR_BAD_PARAMETER;

	ref = clk_p->parent->rate;

	/* Computing FSyn params. Should be common function with FSyn type */
	if ((clk_fsyn_get_params(ref, freq, &md, &pe, &sdiv)) != 0)
		return CLK_ERR_BAD_PARAMETER;

	reg_value = CLK_READ(CKGC_BASE_ADDRESS + CKGC_FS0_CFG);

	regs = &clkgenc_regs[clk_p->id - CLKC_FS0_CH1];

	reg_value |= set_rate_table[clk_p->id - CLKC_FS0_CH1];

	/* Select FS clock only for the clock specified */
	CLK_WRITE(CKGC_BASE_ADDRESS + CKGC_FS0_CFG, reg_value);

	CLK_WRITE(CKGC_BASE_ADDRESS + regs->pe, pe);
	CLK_WRITE(CKGC_BASE_ADDRESS + regs->md, md);
	CLK_WRITE(CKGC_BASE_ADDRESS + regs->sdiv, sdiv);
	CLK_WRITE(CKGC_BASE_ADDRESS + regs->prog, 0x01);
	CLK_WRITE(CKGC_BASE_ADDRESS + regs->prog, 0x00);

	clk_p->rate = freq;
	return 0;

}

static U32 clkgenc_enable_mask[] = { 0x440, 0x880, 0x1100, 0x2200 };

static int clkgenc_enable(clk_t *clk_p)
{
	U32 reg_value = 0;

	if (!clk_p)
		return CLK_ERR_BAD_PARAMETER;

	reg_value = CLK_READ(CKGC_BASE_ADDRESS);

	reg_value |= clkgenc_enable_mask[clk_p->id - CLKC_FS0_CH1];

	CLK_WRITE(CKGC_BASE_ADDRESS, reg_value);
	if ((reg_value & 0x1DC0) && !(reg_value & 0x4000)) {
		reg_value = reg_value | 0x4000;
		CLK_WRITE(CKGC_BASE_ADDRESS, reg_value);
	}
	return 0;

}

static int clkgenc_disable(clk_t *clk_p)
{
	U32 reg_value = 0;

	reg_value = CLK_READ(CKGC_BASE_ADDRESS);
	reg_value &= ~clkgenc_enable_mask[clk_p->id - CLKC_FS0_CH1];

	CLK_WRITE(CKGC_BASE_ADDRESS, reg_value);
	if (!(reg_value & 0x1DC0)) {
		reg_value = reg_value & 0xFFFFBFFF;
		CLK_WRITE(CKGC_BASE_ADDRESS, reg_value);
	}
	return 0;

}
