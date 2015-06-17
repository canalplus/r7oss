/****************************
*************************************************
 *
 * File name   : clock-stx5206.c
 * Description : Low Level API - Chip specific implementation
 *
 * COPYRIGHT (C) 2009 STMicroelectronics - All Rights Reserved
 * May be copied or modified under the terms of the GNU General Public
 * License  Version 2 _ONLY_.  See linux/COPYING for more information.
 *
 *****************************************************************************/

/* ----- Modification history (most recent first)----
12/nov/09 francesco.virlinzi@st,com
	  Several update to improve the code
23/sep/09 fabrice.charpentier@st.com
	  clkgenb_recalc() bug fix + clkgenb_fsyn_recalc() indent.
17/sep/09 fabrice.charpentier@st.com
	  Realignment on 7111+indent.
01/sep/09 fabrice.charpentier@st.com
	  Bug fixes + indentation changes.
24/aug/09 fabrice.charpentier@st.com
	  PLL power down/up update.
05/aug/09 fabrice.charpentier@st.com
	  Updates towards Linux requirements.
26/jun/09 fabrice.charpentier@st.com
	  Revisited, completed and aligned on latest LLA rules.
07/may/09 Suvrat Jain
	  Original version
*/

/* Includes ----------------------------------------------------------------- */

#include <linux/clk.h>
#include <linux/stm/clk.h>
#include <linux/delay.h>
#include <linux/io.h>
#include "clock-stx5206.h"
#include "clock-regs-stx5206.h"

#include "clock-oslayer.h"
#include "clock-common.h"

static int clkgena_observe(clk_t *clk_p, unsigned long *div_p);
static int clkgenb_observe(clk_t *clk_p, unsigned long *div_p);
static int clkgena_set_parent(clk_t *clk_p, clk_t *src_p);
static int clkgenb_set_parent(clk_t *clk_p, clk_t *src_p);
static int clkgenc_set_parent(clk_t *clk_p, clk_t *src_p);
static int clkgend_set_parent(clk_t *clk_p, clk_t *src_p);
static int clkgene_set_parent(clk_t *clk_p, clk_t *src_p);
static int clkgena_set_rate(clk_t *clk_p, unsigned long freq);
static int clkgenb_set_rate(clk_t *clk_p, unsigned long freq);
static int clkgenc_set_rate(clk_t *clk_p, unsigned long freq);
static int clkgena_set_div(clk_t *clk_p, unsigned long *div_p);
static int clkgenb_set_div(clk_t *clk_p, unsigned long *div_p);
static int clkgenb_set_fsclock(clk_t *clk_p, unsigned long freq);
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
static int clkgentop_init(clk_t *clk_p);
static int clkgena_init(clk_t *clk_p);
static int clkgenb_init(clk_t *clk_p);
static int clkgenc_init(clk_t *clk_p);
static int clkgend_init(clk_t *clk_p);
static int clkgene_init(clk_t *clk_p);

/* Per boards top input clocks. mb680 currently identical */
#define SYSCLKIN	30	/* osc */
#define SYSCLKALT 	30	/* Alternate  osc */

static const unsigned long clkgena_offset_regs[] = {
	CKGA_OSC_DIV0_CFG,
	0, /* dummy */
	CKGA_PLL0HS_DIV0_CFG,
	CKGA_PLL0LS_DIV0_CFG,
	CKGA_PLL1_DIV0_CFG
};

/* Possible operations registration.
   Operations are usually grouped by clockgens due to specific HW
   implementation.

   Name, Desc, init, set_parent, set_rate, recalc, enable, disable,
   observe, measure

   where
	Name: MUST be the same one declared with REGISTER_CLK (ops field).
	Desc: Clock group short description. Ex: "clockgen A", "USB", "LMI"
	Init: Clock init function (read HW to identify parent & compute rate).
	SetParent: Parent/src setup function.
	Setfreq: Clock frequency setup function.
	Enable: Clock enable function.
	Disable: Clock disable function.
	Observe: Clock observation function.
	Recalc: Clock frequency recompute function. Called when parent clock
		changed.
	Measure: Clock measure function (when HW available).

   Note: If no capability, put NULL instead of function name.
   Note: All functions should return 'clk_err_t'. */

REGISTER_OPS(Top,
	"Top clocks",
	clkgentop_init,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,	/* No measure function */
	NULL	/* No observation point */
);

REGISTER_OPS(clkgena,
	"Clockgen A",
	clkgena_init,
	clkgena_set_parent,
	clkgena_set_rate,
	clkgena_recalc,
	clkgena_enable,
	clkgena_disable,
	clkgena_observe,
	clkgena_get_measure,
	NULL	/* No observation point */
);

REGISTER_OPS(clkgenb,
	"Clockgen B/Video",
	clkgenb_init,
	clkgenb_set_parent,
	clkgenb_set_rate,
	clkgenb_recalc,
	clkgenb_enable,
	clkgenb_disable,
	clkgenb_observe,
	NULL,	/* No measure function */
	"PIO2[5]"
);
REGISTER_OPS(clkgenc,
	"Clockgen C/Audio",
	clkgenc_init,
	clkgenc_set_parent,
	clkgenc_set_rate,
	clkgenc_recalc,
	clkgenc_enable,
	clkgenc_disable,
	NULL,
	NULL,	/* No measure function */
	NULL	/* No observation point */
);
REGISTER_OPS(clkgend,
	"Clockgen D/LMI",
	clkgend_init,
	clkgend_set_parent,
	NULL,
	clkgend_recalc,
	NULL,
	NULL,
	NULL,
	NULL,	/* No measure function */
	NULL	/* No observation point */
);
REGISTER_OPS(clkgene,
	"USB",
	clkgene_init,
	clkgene_set_parent,
	NULL,
	clkgene_recalc,
	NULL,
	NULL,
	NULL,
	NULL,	/* No measure function */
	NULL	/* No observation point */
);

/* Physical clocks description */
clk_t clk_clocks[] = {
	/*	  ClkID		Ops	Nominalfreq   Flags */
/* Top level clocks */
REGISTER_CLK(CLK_SYS, &Top, 0,
	  CLK_RATE_PROPAGATES | CLK_ALWAYS_ENABLED),
REGISTER_CLK(CLK_SYSALT, &Top, 0,
	  CLK_RATE_PROPAGATES | CLK_ALWAYS_ENABLED),

/* Clockgen A */
REGISTER_CLK(CLKA_REF, &clkgena, 0,
		CLK_RATE_PROPAGATES | CLK_ALWAYS_ENABLED),
REGISTER_CLK_P(CLKA_PLL0, &clkgena, 0,
	       CLK_RATE_PROPAGATES, &clk_clocks[CLKA_REF]),
REGISTER_CLK_P(CLKA_PLL0HS, &clkgena, 900000000,
		CLK_RATE_PROPAGATES, &clk_clocks[CLKA_PLL0]),
REGISTER_CLK_P(CLKA_PLL0LS, &clkgena, 450000000,
		CLK_RATE_PROPAGATES, &clk_clocks[CLKA_PLL0]),
REGISTER_CLK_P(CLKA_PLL1, &clkgena, 800000000,
		CLK_RATE_PROPAGATES, &clk_clocks[CLKA_REF]),

REGISTER_CLK(CLKA_IC_STNOC, &clkgena, 400000000, 0),
REGISTER_CLK(CLKA_FDMA0, &clkgena, 400000000, 0),

REGISTER_CLK(CLKA_PP, &clkgena, 160000000, 0),
REGISTER_CLK(CLKA_SH4_ICK, &clkgena, 450000000, CLK_ALWAYS_ENABLED),
REGISTER_CLK(CLKA_IC_IF_100, &clkgena, 100000000, CLK_ALWAYS_ENABLED),

REGISTER_CLK(CLKA_LX_DMU_CPU, &clkgena, 450000000, 0),
REGISTER_CLK(CLKA_LX_AUD_CPU, &clkgena, 450000000, 0),
REGISTER_CLK(CLKA_IC_BDISP_200, &clkgena, 200000000, 0),
REGISTER_CLK(CLKA_IC_DISP_200, &clkgena, 200000000, 0),
REGISTER_CLK(CLKA_IC_TS_200, &clkgena, 200000000, 0),
REGISTER_CLK(CLKA_DISP_PIPE_200, &clkgena, 200000000, 0),
REGISTER_CLK(CLKA_BLIT_PROC, &clkgena, 266666666, 0),
REGISTER_CLK(CLKA_IC_DELTA_200, &clkgena, 266666666, 0),
REGISTER_CLK(CLKA_ETH_PHY, &clkgena, 50000000, 0),
REGISTER_CLK(CLKA_PCI, &clkgena, 66666666, 0),
REGISTER_CLK(CLKA_EMI_MASTER, &clkgena, 100000000, 0),
REGISTER_CLK(CLKA_IC_COMPO_200, &clkgena, 200000000, 0),
REGISTER_CLK(CLKA_IC_IF_200, &clkgena, 200000000, CLK_ALWAYS_ENABLED),

/* Clockgen B */
REGISTER_CLK(CLKB_REF, &clkgenb, 0,
	CLK_RATE_PROPAGATES | CLK_ALWAYS_ENABLED),
REGISTER_CLK_P(CLKB_FS0, &clkgenb, 0,
	       CLK_RATE_PROPAGATES, &clk_clocks[CLKB_REF]),
REGISTER_CLK_P(CLKB_FS1, &clkgenb, 0,
	       CLK_RATE_PROPAGATES, &clk_clocks[CLKB_REF]),

REGISTER_CLK_P(CLKB_FS0_CH1, &clkgenb, 0,
	       CLK_RATE_PROPAGATES, &clk_clocks[CLKB_FS0]),
REGISTER_CLK_P(CLKB_FS0_CH2, &clkgenb, 0,
	       CLK_RATE_PROPAGATES, &clk_clocks[CLKB_FS0]),
REGISTER_CLK_P(CLKB_FS0_CH3, &clkgenb, 0,
	       CLK_RATE_PROPAGATES, &clk_clocks[CLKB_FS0]),
REGISTER_CLK_P(CLKB_FS0_CH4, &clkgenb, 0,
	       CLK_RATE_PROPAGATES, &clk_clocks[CLKB_FS0]),

REGISTER_CLK_P(CLKB_FS1_CH1, &clkgenb, 0,
	       CLK_RATE_PROPAGATES, &clk_clocks[CLKB_FS1]),
REGISTER_CLK_P(CLKB_FS1_CH2, &clkgenb, 0,
	       CLK_RATE_PROPAGATES, &clk_clocks[CLKB_FS1]),
REGISTER_CLK_P(CLKB_FS1_CH3, &clkgenb, 0,
	       CLK_RATE_PROPAGATES, &clk_clocks[CLKB_FS1]),
REGISTER_CLK_P(CLKB_FS1_CH4, &clkgenb, 0,
	       CLK_RATE_PROPAGATES, &clk_clocks[CLKB_FS1]),

REGISTER_CLK_P(CLKB_SRC_ED, &clkgenb, 0,
		CLK_RATE_PROPAGATES, &clk_clocks[CLKB_FS0_CH1]),/*HD->ED */
REGISTER_CLK_P(CLKB_DSS, &clkgenb, 0, 0, &clk_clocks[CLKB_FS0_CH2]),
REGISTER_CLK_P(CLKB_DAA, &clkgenb, 0, 0, &clk_clocks[CLKB_FS0_CH3]),
	/*spare04->27_0 *//*enable code needs to be written */
REGISTER_CLK_P(CLKB_27_0, &clkgenb, 0, 0, &clk_clocks[CLKB_FS0_CH4]),

REGISTER_CLK(CLKB_SRC_SD, &clkgenb, 0, CLK_RATE_PROPAGATES),
/* Name "clk_pp" on previous chips */
REGISTER_CLK_P(CLKB_27_1, &clkgenb, 0, 0, &clk_clocks[CLKB_FS1_CH2]),
REGISTER_CLK_P(CLKB_MMC, &clkgenb, 0, 0, &clk_clocks[CLKB_FS1_CH3]),
REGISTER_CLK_P(CLKB_LPC, &clkgenb, 0, 0, &clk_clocks[CLKB_FS1_CH4]),

REGISTER_CLK(CLKB_PIX_ED, &clkgenb, 0, 0),
REGISTER_CLK(CLKB_DISP_ED, &clkgenb, 0, 0),
REGISTER_CLK(CLKB_PIX_SD, &clkgenb, 0, 0),
REGISTER_CLK(CLKB_DISP_SD, &clkgenb, 0, 0),
REGISTER_CLK(CLKB_656, &clkgenb, 0, 0),
REGISTER_CLK(CLKB_GDP1, &clkgenb, 0, 0),
REGISTER_CLK(CLKB_PIP, &clkgenb, 0, 0),

/* Clockgen C (AUDIO) */
REGISTER_CLK(CLKC_REF, &clkgenc, 0,
		CLK_RATE_PROPAGATES | CLK_ALWAYS_ENABLED),
REGISTER_CLK_P(CLKC_FS0, &clkgenc, 0,
		CLK_RATE_PROPAGATES, &clk_clocks[CLKC_REF]),
REGISTER_CLK_P(CLKC_FS0_CH1, &clkgenc, 0, 0, &clk_clocks[CLKC_FS0]),
REGISTER_CLK_P(CLKC_FS0_CH2, &clkgenc, 0, 0, &clk_clocks[CLKC_FS0]),
REGISTER_CLK_P(CLKC_FS0_CH3, &clkgenc, 0, 0, &clk_clocks[CLKC_FS0]),
REGISTER_CLK_P(CLKC_FS0_CH4, &clkgenc, 0, 0, &clk_clocks[CLKC_FS0]),

/* Clockgen D (LMI) */
REGISTER_CLK(CLKD_REF, &clkgend, 30000000,
		  CLK_RATE_PROPAGATES | CLK_ALWAYS_ENABLED),
REGISTER_CLK_P(CLKD_LMI2X, &clkgend, 800000000, 0, &clk_clocks[CLKD_REF]),

/* Clockgen E (USB), not really a clockgen */
REGISTER_CLK(CLKE_REF, &clkgene, 30000000, CLK_ALWAYS_ENABLED),

};

GENERIC_LINUX_CLKS(clk_clocks[CLKA_SH4_ICK],	/* sh4_clk parent */
		   clk_clocks[CLKA_IC_IF_100],	/* module_clk parent */
		   clk_clocks[CLKA_IC_IF_100]);	/* comms_clk parent  */

SYSCONF(SYS_STA, 1, 0, 1);
SYSCONF(SYS_CFG, 11, 1, 8);
SYSCONF(SYS_CFG, 11, 9, 11);

SYSCONF(SYS_CFG, 15, 0, 15);	/* div mode: 2 bits per clock */
SYSCONF(SYS_CFG, 15, 16, 23);	/* enable: 1 bit per clock */
SYSCONF(SYS_CFG, 15, 24, 31);	/* source: 1 bit per clock */

SYSCONF(SYS_CFG, 40, 0, 1);
SYSCONF(SYS_CFG, 40, 2, 3);
/*
 * The Linux clk_init function
 */
int __init clk_init(void)
{
	int i;

	SYSCONF_CLAIM(SYS_STA, 1, 0, 1);
	SYSCONF_CLAIM(SYS_CFG, 11, 1, 8);
	SYSCONF_CLAIM(SYS_CFG, 11, 9, 11);
	SYSCONF_CLAIM(SYS_CFG, 15, 0, 15);
	SYSCONF_CLAIM(SYS_CFG, 15, 16, 23);
	SYSCONF_CLAIM(SYS_CFG, 15, 24, 31);
	SYSCONF_CLAIM(SYS_CFG, 40, 0, 1);
	SYSCONF_CLAIM(SYS_CFG, 40, 2, 3);

	for (i = 0; i < ARRAY_SIZE(clk_clocks); ++i)
		if (!clk_register(&clk_clocks[i]))
			clk_enable(&clk_clocks[i]);

	REGISTER_GENERIC_LINUX_CLKS();

	return 0;
}

/******************************************************************************
Top level clocks group
******************************************************************************/

/* ========================================================================
   Name:	clkgentop_init
   Description: Read HW status to initialize 'clk_t' structure.
   Returns:  'clk_err_t' error code.
   ======================================================================== */

static int clkgentop_init(clk_t *clk_p)
{
	if (!clk_p)
		return CLK_ERR_BAD_PARAMETER;

	/* Top recalc function */
	switch (clk_p->id) {
	case CLK_SYS:
		clk_p->rate = SYSCLKIN * 1000000;
		break;
	case CLK_SYSALT:
		clk_p->rate = SYSCLKALT * 1000000;
		break;
	default:
		clk_p->rate = 0;
		break;
	}

	return 0;
}

/******************************************************************************
CLOCKGEN A (CPU+interco+comms) clocks group
******************************************************************************/

/* ========================================================================
Name:	clkgena_get_index
Description: Returns index of given clockgenA clock and source reg infos
Returns:  idx==-1 if error, else >=0
======================================================================== */

static int clkgena_get_index(int clkid, unsigned long *srcreg, int *shift)
{
	int idx;

	/* Warning: This function assumes clock IDs are perfectly
	   following real implementation order. Each "hole" has therefore
	   to be filled with "CLKx_NOT_USED" */
	if (clkid < CLKA_IC_STNOC || clkid > CLKA_IC_IF_200)
		return -1;

	/* Warning: CLKA_BLIT_PROC and CLKA_IC_DELTA_200 have the same index */
	if (clkid >= CLKA_IC_DELTA_200)
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

	return idx;
}

/* ========================================================================
   Name:	clkgena_set_parent
   Description: Set clock source for clockgenA when possible
   Returns:  0=NO error
   ======================================================================== */

static int clkgena_set_parent(clk_t *clk_p, clk_t *src_p)
{
	unsigned long clk_src, val;
	int idx;
	unsigned long srcreg;
	int shift;

	if (!clk_p || !src_p)
		return CLK_ERR_BAD_PARAMETER;

	if (clk_p->id < CLKA_IC_STNOC)
		/* they can not change parent */
		return CLK_ERR_BAD_PARAMETER;

	switch (src_p->id) {
	case CLKA_REF:
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
		return CLK_ERR_BAD_PARAMETER;

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
   Returns:  Pointer on parent 'clk_t' structure.
   ======================================================================== */

static int clkgena_identify_parent(clk_t *clk_p)
{
	int idx;
	unsigned long src_sel;
	unsigned long srcreg;
	int shift;

	if (!clk_p)
		return CLK_ERR_BAD_PARAMETER;


	if (clk_p->id == CLKA_REF) {
		src_sel = SYSCONF_READ(SYS_STA, 1, 0, 1);
		switch (src_sel) {
		case 0:
			clk_p->parent = &clk_clocks[CLK_SYS];	/* OSC FE */
			clk_p->rate = clk_p->parent->rate;
			break;
		case 1:
			clk_p->parent = &clk_clocks[CLK_SYSALT];/* OSC USB */
			clk_p->rate = clk_p->parent->rate;
			break;
		default:
			break;
		}
		return 0;
	} else if (clk_p->id < CLKA_IC_STNOC)
			/* already done statically */
			return 0;

	/* Which divider to setup ? */
	idx = clkgena_get_index(clk_p->id, &srcreg, &shift);
	if (idx == -1)
		return CLK_ERR_BAD_PARAMETER;

	/* Identifying source */
	src_sel = (CLK_READ(CKGA_BASE_ADDRESS + srcreg) >> shift) & 0x3;
	switch (src_sel) {
	case 0:
		clk_p->parent = &clk_clocks[CLKA_REF];
		break;
	case 1:
		if (clk_p->id < CLKA_SH4_ICK)
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

	return 0;
}

/* ========================================================================
   Name:	clkgena_xxxable
   Description: Enable/disable PLL
   Returns:  'clk_err_t' error code
   ======================================================================== */

static int clkgena_xxxable(clk_t *clk_p, int enable)
{
	unsigned long val;
	int bit;
	unsigned long data;

	if (!clk_p)
		return CLK_ERR_BAD_PARAMETER;

	bit = (clk_p->id == CLKA_PLL0 ? 0 : 1);
	val = CLK_READ(CKGA_BASE_ADDRESS + CKGA_POWER_CFG);

	if (enable) {
		CLK_WRITE(CKGA_BASE_ADDRESS + CKGA_POWER_CFG,
			  val & ~(1 << bit));
		clk_p->rate = clk_p->parent->rate; /* PLL0 */
		if (clk_p->id == CLKA_PLL1) {
			data = CLK_READ(CKGA_BASE_ADDRESS + CKGA_PLL1_CFG);
			clk_p->rate =
			clk_pll800_freq(clk_p->parent->rate, data);
		}
	} else {
		CLK_WRITE(CKGA_BASE_ADDRESS + CKGA_POWER_CFG, val | (1 << bit));
		clk_p->rate = 0;
	}

	return 0;
}

/* ========================================================================
   Name:	clkgena_enable
   Description: Enable clock
   Returns:  'clk_err_t' error code
   ======================================================================== */

static int clkgena_enable(clk_t *clk_p)
{
	int err = 0;
	unsigned long data;

	if (!clk_p || !clk_p->parent)
		return CLK_ERR_BAD_PARAMETER;

	if (clk_p->id == CLKA_REF)
		return 0;

	/* PLL power up */
	if (clk_p->id == CLKA_PLL0 || clk_p->id == CLKA_PLL1)
		return clkgena_xxxable(clk_p, 1);

	if (clk_p->id == CLKA_PLL0HS || clk_p->id == CLKA_PLL0LS) {
		data = CLK_READ(CKGA_BASE_ADDRESS + CKGA_PLL0_CFG);
		clk_p->rate = clk_pll1600_freq(clk_p->parent->rate, data);
		if (clk_p->id == CLKA_PLL0LS)
			clk_p->rate /= 2;
		return 0;

	}
	err = clkgena_set_parent(clk_p, clk_p->parent);

	return err;
}

/* ========================================================================
   Name:	clkgena_disable
   Description: Disable clock
   Returns:  'clk_err_t' error code
   ======================================================================== */

static int clkgena_disable(clk_t *clk_p)
{
	unsigned long val;
	int idx;
	unsigned long srcreg;
	int shift;

	if (!clk_p)
		return CLK_ERR_BAD_PARAMETER;

	/* PLL power down */
	if (clk_p->id == CLKA_PLL0 || clk_p->id == CLKA_PLL1)
		return clkgena_xxxable(clk_p, 0);

	idx = clkgena_get_index(clk_p->id, &srcreg, &shift);
	if (idx == -1)
		return CLK_ERR_BAD_PARAMETER;

	/* Disabling clock */
	val = CLK_READ(CKGA_BASE_ADDRESS + srcreg) & ~(0x3 << shift);
	val = val | (3 << shift);	/* 3 = STOP clock */
	CLK_WRITE(CKGA_BASE_ADDRESS + srcreg, val);
	clk_p->rate = 0;

	return 0;
}

/* ========================================================================
   Name:	clkgena_set_div
   Description: Set divider ratio for clockgenA when possible
   ======================================================================== */

static int clkgena_set_div(clk_t *clk_p, unsigned long *div_p)
{
	int idx;
	unsigned long div_cfg = 0;
	unsigned long srcreg, offset;
	int shift;

	if (!clk_p || !clk_p->parent)
		return CLK_ERR_BAD_PARAMETER;

	/* Computing divider config */
	div_cfg = (*div_p - 1) & 0x1F;

	/* Which divider to setup ? */
	idx = clkgena_get_index(clk_p->id, &srcreg, &shift);
	if (idx == -1)
		return CLK_ERR_BAD_PARAMETER;

	/* Now according to parent, let's write divider ratio */
	offset = clkgena_offset_regs[clk_p->parent->id - CLKA_REF];
	CLK_WRITE(CKGA_BASE_ADDRESS + offset + (4 * idx), div_cfg);

	return 0;
}

/* ========================================================================
   Name:	clkgena_set_rate
   Description: Set clock frequency
   ======================================================================== */

static int clkgena_set_rate(clk_t *clk_p, unsigned long freq)
{
	unsigned long div;
	int err;

	if (!clk_p)
		return CLK_ERR_BAD_PARAMETER;

	if (clk_p->id < CLKA_PLL0HS || clk_p->id > CLKA_IC_IF_200)
		return CLK_ERR_BAD_PARAMETER;

	/* PLL set rate: to be completed */
	if (clk_p->id >= CLKA_PLL0HS && clk_p->id <= CLKA_PLL1)
		return CLK_ERR_BAD_PARAMETER;

	/* We need a parent for these clocks */
	if (!clk_p->parent)
		return CLK_ERR_INTERNAL;

	div = clk_p->parent->rate / freq;
	err = clkgena_set_div(clk_p, &div);
	if (!err)
		clk_p->rate = clk_p->parent->rate / div;

	return err;
}

/* ========================================================================
   Name:	clkgena_recalc
   Description: Get CKGA programmed clocks frequencies
   Returns:  0=NO error
   ======================================================================== */

static int clkgena_recalc(clk_t *clk_p)
{
	unsigned long data, ratio;
	int idx;
	unsigned long srcreg, offset;
	int shift;

	if (!clk_p)
		return CLK_ERR_BAD_PARAMETER;
	if (!clk_p->parent)
		return CLK_ERR_INTERNAL;

	/* Cleaning structure first */
	clk_p->rate = 0;

	/* Reading clock programmed value */
	switch (clk_p->id) {
	case CLKA_REF:		/* Clockgen A reference clock */
	case CLKA_PLL0:	/* Not a clock but a PLL id. Therefore out=in */
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
		idx = clkgena_get_index(clk_p->id, &srcreg, &shift);
		if (idx == -1)
			return CLK_ERR_BAD_PARAMETER;

		/* Now according to source, let's get divider ratio */
		offset = clkgena_offset_regs[clk_p->parent->id - CLKA_REF];
		data = CLK_READ(CKGA_BASE_ADDRESS + offset + (4 * idx));

		ratio = (data & 0x1F) + 1;

		clk_p->rate = clk_p->parent->rate / ratio;
		break;
	}

	return 0;
}

/* ========================================================================
   Name:	clkgena_observe
   Description: allows to observe a clock on a SYSACLK_OUT
   Returns:  0=NO error
   ======================================================================== */

static int clkgena_observe(clk_t *clk_p, unsigned long *div_p)
{
	unsigned long src = 0;
	unsigned long divcfg;
	/* WARNING: the obs_table[] must strictly follows clockgen enum order
	 * taking into account any "holes" (CLKA_NOTUSED) filled with
	 * 0xffffffff */
	static const int obs_table[] = {
		0x8, 0x9, 0xa, 0xb, 0xc, 0xd, 0xe, 0xf, 0x10, 0x11,
		0x12, 0x13, 0x14, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19
	};

	if (!clk_p || !div_p)
		return CLK_ERR_BAD_PARAMETER;

	if (clk_p->id < CLKA_IC_STNOC || clk_p->id > CLKA_IC_IF_200)
		return CLK_ERR_BAD_PARAMETER;

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
   Returns:  'clk_err_t' error code.
   ======================================================================== */

static unsigned long clkgena_get_measure(clk_t *clk_p)
{
	unsigned long src, data;
	unsigned long measure;
	/* WARNING: the measure_table[] must strictly follows clockgen enum
	 * order taking into account any "holes" (CLKA_NOT_USED) filled with
	 * 0xffffffff */
	static const int measure_table[] = {
		0x8, 0x9, 0xffffffff, 0xb, 0xc, 0xd, 0xe, 0xf, 0x10, 0x11,
		0x12, 0x13, 0x14, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19
	};

	if (!clk_p)
		return 0;
	if (clk_p->id < CLKA_IC_STNOC || clk_p->id > CLKA_IC_IF_200)
		return 0;

	src = measure_table[clk_p->id - CLKA_IC_STNOC];
	measure = 0;

	/* Loading the MAX Count 1000 in 30MHz Oscillator Counter */
	CLK_WRITE(CKGA_BASE_ADDRESS + CKGA_CLKOBS_MASTER_MAXCOUNT, 0x3E8);
	CLK_WRITE(CKGA_BASE_ADDRESS + CKGA_CLKOBS_CMD, 3);

	/* Selecting clock to observe */
	CLK_WRITE(CKGA_BASE_ADDRESS + CKGA_CLKOBS_MUX1_CFG, (1 << 7) | src);

	/* Start counting */
	CLK_WRITE(CKGA_BASE_ADDRESS + CKGA_CLKOBS_CMD, 0);

	while (1) {
		mdelay(10);
		data = CLK_READ(CKGA_BASE_ADDRESS + CKGA_CLKOBS_STATUS);
		if (data & 1)
			break;	/* IT */
	}

	/* Reading value */
	data = CLK_READ(CKGA_BASE_ADDRESS + CKGA_CLKOBS_SLAVE0_COUNT);
	measure = 30 * data * 1000;

	CLK_WRITE(CKGA_BASE_ADDRESS + CKGA_CLKOBS_CMD, 3);

	return measure;
}

/* ========================================================================
   Name:	clkgena_init
   Description: Read HW status to initialize 'clk_t' structure.
   Returns:  'clk_err_t' error code.
   ======================================================================== */

static int clkgena_init(clk_t *clk_p)
{
	int err;

	if (!clk_p)
		return CLK_ERR_BAD_PARAMETER;

	err = clkgena_identify_parent(clk_p);
/*
	if (!err)
		err = clkgena_recalc(clk_p);
*/
	return err;
}

/******************************************************************************
CLOCKGEN B
******************************************************************************/

static void clkgenb_unlock(void)
{
	CLK_WRITE(CKGB_BASE_ADDRESS + CKGB_LOCK, 0xc0de);
}

static void clkgenb_lock(void)
{
	CLK_WRITE(CKGB_BASE_ADDRESS + CKGB_LOCK, 0xc1a0);
}

/* ========================================================================
   Name:	clkgenb_xable_fsyn
   Description: enable/disable FSYN
   Returns:  0=NO error
   ======================================================================== */

static int clkgenb_xable_fsyn(clk_t *clk_p, unsigned long enable)
{
	unsigned long val, clkout, ctrl;

	if (!clk_p)
		return CLK_ERR_BAD_PARAMETER;
	if (clk_p->id == CLKB_FS0) {
		clkout = CKGB_FS0_CLKOUT_CTRL;
		ctrl = CKGB_FS0_CTRL;
	} else if (clk_p->id == CLKB_FS1) {
		clkout = CKGB_FS1_CLKOUT_CTRL;
		ctrl = CKGB_FS1_CTRL;
	} else
		return CLK_ERR_BAD_PARAMETER;

	clkgenb_unlock();

	/* Powering down/up digital part */
	val = CLK_READ(CKGB_BASE_ADDRESS + clkout);
	if (enable)
		CLK_WRITE(CKGB_BASE_ADDRESS + clkout, val | 0xF);
	else
		CLK_WRITE(CKGB_BASE_ADDRESS + clkout, val & ~0xF);

	/* Powering down/up analog part */
	val = CLK_READ(CKGB_BASE_ADDRESS + ctrl);
	if (enable) {
		CLK_WRITE(CKGB_BASE_ADDRESS + ctrl, val | (1 << 4));
		clk_p->rate = clk_p->parent->rate;
	} else {
		CLK_WRITE(CKGB_BASE_ADDRESS + ctrl, val & ~(1 << 4));
		clk_p->rate = 0;
	}

	clkgenb_lock();
	return 0;
}

/* ========================================================================
   Name:	clkgenb_xable_clock
   Description: Enable/disable clock (Clockgen B)
   Returns:  0=NO error
   ======================================================================== */

struct gen_utility {
	unsigned long clk_id;
	unsigned long info;
};

static const int clock_b_pwr_table[] = { 3, 1, 0, 12, 9, 11, 12, 13};

static int clkgenb_xable_clock(clk_t *clk_p, unsigned long enable)
{
	unsigned long bit, power;

	if (!clk_p)
		return CLK_ERR_BAD_PARAMETER;

	clkgenb_unlock();

	if (clk_p->id >= CLKB_SRC_ED && clk_p->id <= CLKB_LPC) {
		/* these clock are managed via clock_Gen */
		bit = clock_b_pwr_table[clk_p->id - CLKB_SRC_ED];
		power = CLK_READ(CKGB_BASE_ADDRESS + CKGB_POWER_ENABLE);
		if (enable)
			power |= (1 << bit);
		else
			power &= ~(1 << bit);
		CLK_WRITE(CKGB_BASE_ADDRESS + CKGB_POWER_ENABLE, power);
	} else {
		/* these clock are managed via sysconf */
		bit = clk_p->id - CLKB_PIX_ED;
		power = SYSCONF_READ(SYS_CFG, 15, 16, 23);
		if (enable)
			power |= (1 << bit);
		else
			power &= ~(1 << bit);
		SYSCONF_WRITE(SYS_CFG, 15, 16, 23, power);
	}

	clkgenb_lock();

	if (enable)
		clkgenb_recalc(clk_p); /* to set the rate */
	else
		clk_p->rate = 0;
	return 0;
}

/* ========================================================================
   Name:	clkgenb_enable
   Description: enable clock or FSYN (clockgen B)
   Returns:  O=NO error
   ======================================================================== */

static int clkgenb_enable(clk_t *clk_p)
{
	if (!clk_p)
		return CLK_ERR_BAD_PARAMETER;

	if (clk_p->id == CLKB_FS0 || clk_p->id == CLKB_FS1)
		return clkgenb_xable_fsyn(clk_p, 1);

	return clkgenb_xable_clock(clk_p, 1);
}

/* ========================================================================
   Name:	clkgenb_disable
   Description: Disable clock
   Returns:  O=NO error
   ======================================================================== */

static int clkgenb_disable(clk_t *clk_p)
{
	if (!clk_p)
		return CLK_ERR_BAD_PARAMETER;

	if (clk_p->id == CLKB_FS0 || clk_p->id == CLKB_FS1)
		return clkgenb_xable_fsyn(clk_p, 0);

	return clkgenb_xable_clock(clk_p, 0);
}

/* ========================================================================
   Name:	clkgenb_set_parent
   Description: Set clock source for clockgenB when possible
   Returns:  0=NO error
   ======================================================================== */

static int clkgenb_set_parent(clk_t *clk_p, clk_t *parent_p)
{
	unsigned long set = 0;	/* Each bit set to 1 will be SETTED */
	unsigned long reset = 0;	/* Each bit set to 1 will be RESETTED */
	unsigned long val;

	if (!clk_p || !parent_p)
		return CLK_ERR_BAD_PARAMETER;

	if (clk_p->id == CLKB_REF) {
		if (parent_p->id != CLK_SYS && parent_p->id != CLK_SYSALT)
			return CLK_ERR_BAD_PARAMETER;

		reset = 3;
		set = (parent_p->id == CLK_SYS ? 0 : 2);
	} else if (clk_p->id == CLKB_SRC_SD) {
		if (parent_p->id != CLKB_FS0_CH1 &&
		    parent_p->id != CLKB_FS1_CH1)
			return CLK_ERR_BAD_PARAMETER;

		if (parent_p->id == CLKB_FS0_CH1)
			reset = 1 << 1;
		else
			set = 1 << 1;

	} else if (clk_p->id >= CLKB_PIX_ED && clk_p->id <= CLKB_PIP) {
		if (parent_p->id != CLKB_SRC_ED &&
		    parent_p->id != CLKB_SRC_SD)
			return CLK_ERR_BAD_PARAMETER;

		if (parent_p->id == CLKB_SRC_ED)
			reset = 1 << (clk_p->id - CLKB_PIX_SD);
		else
			set = 1 << (clk_p->id - CLKB_PIX_SD);
	} else
		return CLK_ERR_BAD_PARAMETER;

	clkgenb_unlock();
	if (clk_p->id == CLKB_REF || clk_p->id == CLKB_SRC_SD) {
		unsigned long reg;
		/* set via clock gen */
		reg = CKGB_BASE_ADDRESS + CKGB_CRISTAL_SEL;
		if (clk_p->id == CLKB_SRC_SD)
			reg = CKGB_BASE_ADDRESS + CKGB_FS_SELECT;
		val = CLK_READ(reg);
		val = val & ~reset;
		val = val | set;
		CLK_WRITE(reg, val);
	} else {
		/* set via sysconf */
		val = SYSCONF_READ(SYS_CFG, 15, 24, 31);
		val = val & ~reset;
		val = val | set;
		SYSCONF_WRITE(SYS_CFG, 15, 24, 31, val);
	}
	clkgenb_lock();
	clk_p->parent = parent_p;
	clkgenb_recalc(clk_p);

	return 0;
}

/* ========================================================================
   Name:	clkgenb_set_rate
   Description: Set clock frequency
   ======================================================================== */

static int clkgenb_set_rate(clk_t *clk_p, unsigned long freq)
{
	unsigned long div;
	int err;

	if (!clk_p)
		return CLK_ERR_BAD_PARAMETER;

	if (!clk_p->parent)
		/* A parent is expected to these clocks */
		return CLK_ERR_INTERNAL;

	if (clk_p->id >= CLKB_FS0_CH1 && clk_p->id <= CLKB_FS1_CH4)
		/* clkgenb_set_fsclock() is updating clk_p->rate */
		return clkgenb_set_fsclock(clk_p, freq);

	div = clk_p->parent->rate / freq;
	err = clkgenb_set_div(clk_p, &div);
	if (err == 0)
		clk_p->rate = freq;

	return err;
}

/* ========================================================================
   Name:	clkgenb_set_fsclock
   Description: Set FS clock
   Returns:  0=NO error
   ======================================================================== */

static int clkgenb_set_fsclock(clk_t *clk_p, unsigned long freq)
{
	int md, pe, sdiv;
	int bank, channel;

	if (!clk_p)
		return CLK_ERR_BAD_PARAMETER;

	if (!clk_p->parent)
		return CLK_ERR_INTERNAL;

	if (clk_p->id < CLKB_FS0_CH1 || clk_p->id > CLKB_FS1_CH4)
		return CLK_ERR_BAD_PARAMETER;

	/* Computing FSyn params. Should be common function with FSyn type */
	if (clk_fsyn_get_params(clk_p->parent->rate, freq, &md, &pe, &sdiv))
		return CLK_ERR_BAD_PARAMETER;

	bank = (clk_p->id - CLKB_FS0_CH1) / 4;
	channel = (clk_p->id - CLKB_FS0_CH1) % 4;

	clkgenb_unlock();
	CLK_WRITE(CKGB_BASE_ADDRESS + CKGB_FS_MD(bank, channel), md);
	CLK_WRITE(CKGB_BASE_ADDRESS + CKGB_FS_PE(bank, channel), pe);
	CLK_WRITE(CKGB_BASE_ADDRESS + CKGB_FS_SDIV(bank, channel), sdiv);
	CLK_WRITE(CKGB_BASE_ADDRESS + CKGB_FS_EN_PRG(bank, channel), 0x1);
	CLK_WRITE(CKGB_BASE_ADDRESS + CKGB_FS_EN_PRG(bank, channel), 0x0);
	clkgenb_lock();
	clk_p->rate = freq;

	return 0;
}

/* ========================================================================
   Name:	clkgenb_set_div
   Description: Set divider ratio for clockgenB when possible
   Returns:  0=NO error
   ======================================================================== */

static int clkgenb_set_div(clk_t *clk_p, unsigned long *div_p)
{
	unsigned long set = 0;	/* Each bit set to 1 will be SETTED */
	unsigned long reset = 0;	/* Each bit set to 1 will be RESETTED */
	unsigned long reg;
	unsigned long val;
	static const int shift_table[] = {0, 2, 4, 8, 10, 12};
				/* 	  0, 1, 2,  3, 4,  5,  6,  7, 8 */
	static const int div_table[] = { -1, 0, 1, -1, 2, -1, -1, -1, 3};

	if (!clk_p)
		return CLK_ERR_BAD_PARAMETER;

	switch (clk_p->id) {
	case CLKB_SRC_ED:
		reg = CKGB_BASE_ADDRESS + CKGB_POWER_DOWN;
		if (*div_p == 1024)
			set = 1 << 3;
		else
			reset = 1 << 3;
		break;
	case CLKB_SRC_SD:
		reg = CKGB_BASE_ADDRESS + CKGB_DISPLAY_CFG;
		/* clk_pix_sd sourced from Fsyn1 */
		if (*div_p == 1024) {
			reg = CKGB_BASE_ADDRESS + CKGB_POWER_DOWN;
			set = 1 << 7;
		}

		if (CLK_READ(CKGB_BASE_ADDRESS + CKGB_FS_SELECT) & 0x2)
			switch (*div_p) {
			case 4:
				reset = 1 << 13;
				set = 1 << 12;
				break;
			case 1:
				set = 0x3 << 12;
				break;
			}
		else	/* clk_pix_sd sourced from Fsyn0 */
			switch (*div_p) {
			case 2:
				reset = 0x3 << 10;
				break;
			case 4:
				reset = 1 << 11;
				set = 1 << 10;
				break;
			}
		break;

	case CLKB_PIX_ED:
	case CLKB_DISP_ED:
	case CLKB_PIX_SD:
	case CLKB_DISP_SD:
	case CLKB_656:
	case CLKB_GDP1:
	case CLKB_PIP:
		reset = 3 << shift_table[clk_p->id - CLKB_PIX_ED];
		if (*div_p > 8)
			return CLK_ERR_BAD_PARAMETER;
		set = div_table[*div_p];
		if (set == -1)
			return CLK_ERR_BAD_PARAMETER;
		set <<= shift_table[clk_p->id - CLKB_PIX_ED];
		break;
	default:
		return CLK_ERR_BAD_PARAMETER;
	}

	clkgenb_unlock();
	if (clk_p->id < CLKB_PIX_ED) { /* CLKB_SRC_ED and CLKB_SRC_SD */
		/* Managed via clock gen */
		val = CLK_READ(reg);
		val = (val & ~reset) | set;
		CLK_WRITE(reg, val);
	} else {
		/* Managed via sysconf */
		val = SYSCONF_READ(SYS_CFG, 15, 0, 15);
		val = (val & ~reset) | set;
		SYSCONF_WRITE(SYS_CFG, 15, 0, 15, val);
	}
	clkgenb_lock();

	return 0;
}

/* ========================================================================
   Name:	clkgenb_observe
   Description: Allows to observe a clock on a PIO5_2
   Returns:  0=NO error
   ======================================================================== */

static int clkgenb_observe(clk_t *clk_p, unsigned long *div_p)
{
	unsigned long i, out0, out1 = 0;

	static const struct gen_utility observe_table[] = {
		{CLKB_SRC_ED, 3},
		{CLKB_SRC_SD, 8},
		{CLKB_DSS, 9}, {CLKB_DAA, 10},
		{CLKB_27_1, 11},
		{CLKB_MMC, 11}, {CLKB_27_0, 12},	/* On mux 1 */
		{CLKB_LPC, 13}
	};

	if (!clk_p)
		return CLK_ERR_BAD_PARAMETER;

	for (i = 0; i < ARRAY_SIZE(observe_table); ++i)
		if (observe_table[i].clk_id == clk_p->id)
			break;

	if (i == ARRAY_SIZE(observe_table))
		return CLK_ERR_BAD_PARAMETER;

	/* MMC and 27_0 are taken from mux1 */
	if (clk_p->id == CLKB_MMC || clk_p->id == CLKB_27_0) {
		out1 = observe_table[i].info;
		out0 = 12;
	} else
		out0 = observe_table[i].info;

	clkgenb_unlock();
	CLK_WRITE((CKGB_BASE_ADDRESS + CKGB_OUT_CTRL), (out1 << 4) | out0);
	clkgenb_lock();

	/* Set PIO2_5 for observation (alternate function output mode) */
	PIO_SET_MODE(2, 5, STPIO_ALT_OUT);

	/* No possible predivider on clockgen B */
	*div_p = 1;

	return 0;
}

/* ========================================================================
   Name:	clkgenb_fsyn_recalc
   Description: Check FSYN & channels status... active, disabled, standbye
		'clk_p->rate' is updated accordingly.
   Returns:  Error code.
   ======================================================================== */

static int clkgenb_fsyn_recalc(clk_t *clk_p)
{
	unsigned long val, clkout, ctrl, bit;
	unsigned long pe, md, sdiv;
	int bank, channel;

	if (!clk_p)
		return CLK_ERR_BAD_PARAMETER;

	/* Which FSYN control registers to use ? */
	switch (clk_p->id) {
	case CLKB_FS0:
	case CLKB_FS0_CH1...CLKB_FS0_CH4:
		clkout = CKGB_FS0_CLKOUT_CTRL;
		ctrl = CKGB_FS0_CTRL;
		break;
	case CLKB_FS1:
	case CLKB_FS1_CH1...CLKB_FS1_CH4:
		clkout = CKGB_FS1_CLKOUT_CTRL;
		ctrl = CKGB_FS1_CTRL;
		break;
	default:
		return CLK_ERR_BAD_PARAMETER;
	}

	/* Is FSYN analog part UP ? */
	val = CLK_READ(CKGB_BASE_ADDRESS + ctrl);
	if ((val & (1 << 4)) == 0) {	/* NO. Analog part is powered down */
		clk_p->rate = 0;
		return 0;
	}

	/* Is FSYN digital part UP ? */
	if (clk_p->id !=  CLKB_FS0 && clk_p->id != CLKB_FS1) {

		bit = (clk_p->id - CLKB_FS0_CH1) % 4;
		val = CLK_READ(CKGB_BASE_ADDRESS + clkout);
		if ((val & (1 << bit)) == 0) {
			/* Digital standbye */
			clk_p->rate = 0;
			return 0;
		}
	}

	/* FSYN is up and running.
	   Now computing frequency */
	if (clk_p->id < CLKB_FS0_CH1) {
		clk_p->rate = clk_p->parent->rate;
		return 0;
	}

	bank = (clk_p->id - CLKB_FS0_CH1) / 4;
	channel = (clk_p->id - CLKB_FS0_CH1) % 4;

	pe = CLK_READ(CKGB_BASE_ADDRESS + CKGB_FS_PE(bank, channel));
	md = CLK_READ(CKGB_BASE_ADDRESS + CKGB_FS_MD(bank, channel));
	sdiv = CLK_READ(CKGB_BASE_ADDRESS + CKGB_FS_SDIV(bank, channel));
	clk_p->rate = clk_fsyn_get_rate(clk_p->parent->rate, pe, md, sdiv);

	return 0;
}

/* ========================================================================
   Name:	clkgenb_recalc
   Description: Get CKGB clocks frequencies function
   Returns:  0=NO error
   ======================================================================== */

/* Check clock enable value for clockgen B.
   Returns: 1=RUNNING, 0=DISABLED */
static int clkgenb_is_running(unsigned long power, int bit)
{
	if (power & (1 << bit))
		return 1;

	return 0;
}

static int clkgenb_recalc(clk_t *clk_p)
{
	unsigned long displaycfg, powerdown, fs_sel, power;
	static const int tab1248[] = { 1, 2, 4, 8 };
	static const int tab2481[] = { 2, 4, 8, 1 };
	static const int tab2482[] = { 2, 4, 8, 2 };


	if (!clk_p)
		return CLK_ERR_BAD_PARAMETER;

	if (!clk_p->parent)
		return CLK_ERR_INTERNAL; /* parent_p clock is unknown */

	/* Read mux */
	displaycfg = CLK_READ(CKGB_BASE_ADDRESS + CKGB_DISPLAY_CFG);
	powerdown = CLK_READ(CKGB_BASE_ADDRESS + CKGB_POWER_DOWN);
	fs_sel = CLK_READ(CKGB_BASE_ADDRESS + CKGB_FS_SELECT);
	power = CLK_READ(CKGB_BASE_ADDRESS + CKGB_POWER_ENABLE);

	switch (clk_p->id) {
	case CLKB_REF:
	case CLKB_27_0:
		clk_p->rate = clk_p->parent->rate;
		return ;

	case CLKB_FS0  ... CLKB_FS1_CH4:
		return clkgenb_fsyn_recalc(clk_p);

	case CLKB_SRC_ED:	/* pix_hd */
		if (!clkgenb_is_running(power,
			clock_b_pwr_table[clk_p->id - CLKB_SRC_ED]))
			clk_p->rate = 0;
		else if (powerdown & (1 << 3))
			clk_p->rate = clk_p->parent->rate / 1024;
		else
			clk_p->rate = clk_p->parent->rate;
		return 0;

	case CLKB_SRC_SD:	/* pix_sd */
		if (!clkgenb_is_running(power,
			clock_b_pwr_table[clk_p->id - CLKB_SRC_ED]))
			clk_p->rate = 0;
		else if (powerdown & (1 << 7))
			clk_p->rate = clk_p->parent->rate / 1024;
		else if (fs_sel & 0x2)	/* source is FS1 */
			clk_p->rate = clk_p->parent->rate /
				tab2481[(displaycfg >> 12) & 0x3];
		else		/* source is FS0 */
			clk_p->rate = clk_p->parent->rate /
				tab2482[(displaycfg >> 10) & 0x3];
		return 0;

	case CLKB_DSS:
	case CLKB_DAA:
	case CLKB_27_1:
	case CLKB_MMC:
		if (!clkgenb_is_running(power,
			clock_b_pwr_table[clk_p->id - CLKB_SRC_ED]))
			clk_p->rate = 0;
		else
			clk_p->rate = clk_p->parent->rate;
		return 0;

	case CLKB_LPC:
		if (!clkgenb_is_running(power,
			clock_b_pwr_table[clk_p->id - CLKB_SRC_ED]))
			clk_p->rate = 0;
		else if (powerdown & (1 << 9))
			clk_p->rate = clk_p->parent->rate / 1024;
		else
			clk_p->rate = clk_p->parent->rate;
		return 0;

	case CLKB_PIX_ED ... CLKB_PIP:
		power = SYSCONF_READ(SYS_CFG, 15, 16, 23);
		if (!clkgenb_is_running(power, clk_p->id - CLKB_PIX_ED))
			clk_p->rate = 0;
		else {
			unsigned long div;
			div = SYSCONF_READ(SYS_CFG, 15, 0, 15);
			clk_p->rate = clk_p->parent->rate / tab1248
			[(div >> ((clk_p->id - CLKB_PIX_ED) * 2)) & 0x3];
		}
		break;

	}

	return 0;
}

/* ========================================================================
   Name:	clkgenb_identify_parent
   Description: Identify parent clock
   Returns:  clk_err_t
   ======================================================================== */

static int clkgenb_identify_parent(clk_t *clk_p)
{
	unsigned long sel;

	if (!clk_p)
		return CLK_ERR_BAD_PARAMETER;

	switch (clk_p->id) {
	case CLKB_REF:		/* What is clockgen B ref clock ? */
		sel = CLK_READ(CKGB_BASE_ADDRESS + CKGB_CRISTAL_SEL);
		clk_p->parent = &clk_clocks
			[(sel & 0x2) ? CLK_SYSALT : CLK_SYS];
		break;

	case CLKB_SRC_SD:	/* pix_sd */
		sel = CLK_READ(CKGB_BASE_ADDRESS + CKGB_FS_SELECT);
		clk_p->parent = &clk_clocks[
			((sel & 0x2) ? CLKB_FS1_CH1 : CLKB_FS0_CH1)];
		break;

	case CLKB_PIX_ED...CLKB_PIP:
		sel = SYSCONF_READ(SYS_CFG, 15, 24, 31);
		clk_p->parent = &clk_clocks[
			((sel & (1 << (clk_p->id - CLKB_PIX_ED))) ?
				CLKB_SRC_SD : CLKB_SRC_ED)];
		break;
	}

	return 0;
}

/* ========================================================================
   Name:	clkgenb_init
   Description: Read HW status to initialize 'clk_t' structure.
   Returns:  'clk_err_t' error code.
   ======================================================================== */

static int clkgenb_init(clk_t *clk_p)
{
	int err;

	if (!clk_p)
		return CLK_ERR_BAD_PARAMETER;

	err = clkgenb_identify_parent(clk_p);
/*	if (err == 0)i
		err = clkgenb_recalc(clk_p);
*/
	return err;
}

/******************************************************************************
CLOCKGEN C (audio)
******************************************************************************/

/* ========================================================================
   Name:	clkgenc_fsyn_recalc
   Description: Get CKGC FSYN clocks frequencies function
   Returns:  0=NO error
   ======================================================================== */

static int clkgenc_fsyn_recalc(clk_t *clk_p)
{
	unsigned long cfg, dig_bit, en_bit;
	unsigned long pe, md, sdiv;
	int channel;

	if (!clk_p)
		return CLK_ERR_BAD_PARAMETER;

	/* Is FSYN analog UP ? */
	cfg = CLK_READ(CKGC_BASE_ADDRESS + CKGC_FS_CFG(0));
	if (!(cfg & (1 << 14))) {	/* Analog power down */
		clk_p->rate = 0;
		return 0;
	}

	/* Is FSYN digital part UP & enabled ? */
	if (clk_p->id == CLKC_FS0) {
		clk_p->rate = clk_p->parent->rate;
		return 0;
	}

	if (clk_p->id < CLKC_FS0_CH1 || clk_p->id > CLKC_FS0_CH4)
		return CLK_ERR_BAD_PARAMETER;

	dig_bit = 10 + (clk_p->id - CLKC_FS0_CH1);
	en_bit = 6 + (clk_p->id - CLKC_FS0_CH1);

	channel = (clk_p->id - CLKC_FS0_CH1) % 4;

	if ((cfg & (1 << dig_bit)) == 0) {	/* digital part in standbye */
		clk_p->rate = 0;
		return 0;
	}
	if ((cfg & (1 << en_bit)) == 0) {	/* disabled */
		clk_p->rate = 0;
		return 0;
	}

	/* FSYN up & running.
	   Computing frequency */

	pe = CLK_READ(CKGC_BASE_ADDRESS + CKGC_FS_PE(0, channel));
	md = CLK_READ(CKGC_BASE_ADDRESS + CKGC_FS_MD(0, channel));
	sdiv = CLK_READ(CKGC_BASE_ADDRESS + CKGC_FS_SDIV(0, channel));
	clk_p->rate = clk_fsyn_get_rate(clk_p->parent->rate, pe, md, sdiv);

	return 0;
}

/* ========================================================================
   Name:	clkgenc_recalc
   Description: Get CKGC clocks frequencies function
   Returns:  0=NO error
   ======================================================================== */

static int clkgenc_recalc(clk_t *clk_p)
{
	if (!clk_p)
		return CLK_ERR_BAD_PARAMETER;

	switch (clk_p->id) {
	case CLKC_REF:
		clk_p->rate = clk_p->parent->rate;
		break;
	case CLKC_FS0:		/* FSyn 0 power control => output=input */
	case CLKC_FS0_CH1...CLKC_FS0_CH4:	/* FS0 clocks */
		return clkgenc_fsyn_recalc(clk_p);
	default:
		return CLK_ERR_BAD_PARAMETER;
	}

	return 0;
}

/* ========================================================================
   Name:	clkgenc_set_rate
   Description: Set CKGC clocks frequencies
   Returns:  0=NO error
   ======================================================================== */

static int clkgenc_set_rate(clk_t *clk_p, unsigned long freq)
{
	int md, pe, sdiv;
	unsigned long reg_value = 0;
	int channel;
	static const int table_enable[] = { 0x06, 0x0A, 0x012, 0x022 };

	if (!clk_p)
		return CLK_ERR_BAD_PARAMETER;

	if ((clk_p->id < CLKC_FS0_CH1) || (clk_p->id > CLKC_FS0_CH4))
		return CLK_ERR_BAD_PARAMETER;

	/* Computing FSyn params. Should be common function with FSyn type */
	if (clk_fsyn_get_params(clk_p->parent->rate, freq, &md, &pe, &sdiv))
		return CLK_ERR_BAD_PARAMETER;

	if (clk_p->id < CLKC_FS0_CH1 || clk_p->id > CLKC_FS0_CH4)
		return CLK_ERR_BAD_PARAMETER;

	reg_value = CLK_READ(CKGC_BASE_ADDRESS + CKGC_FS_CFG(0));
	reg_value |= table_enable[clk_p->id - CLKC_FS0_CH1];

	/* Select FS clock only for the clock specified */
	CLK_WRITE(CKGC_BASE_ADDRESS + CKGC_FS_CFG(0), reg_value);

	channel = (clk_p->id - CLKC_FS0_CH1) % 4;
	CLK_WRITE(CKGC_BASE_ADDRESS + CKGC_FS_PE(0, channel), pe);
	CLK_WRITE(CKGC_BASE_ADDRESS + CKGC_FS_MD(0, channel), md);
	CLK_WRITE(CKGC_BASE_ADDRESS + CKGC_FS_SDIV(0, channel), sdiv);
	CLK_WRITE(CKGC_BASE_ADDRESS + CKGC_FS_EN_PRG(0, channel), 0x01);
	CLK_WRITE(CKGC_BASE_ADDRESS + CKGC_FS_EN_PRG(0, channel), 0x00);

	clk_p->rate = freq;

	return 0;
}

/* ========================================================================
   Name:	clkgenc_identify_parent
   Description: Identify parent clock
   Returns:  clk_err_t
   ======================================================================== */

static int clkgenc_identify_parent(clk_t *clk_p)
{
	unsigned long sel;

	if (!clk_p)
		return CLK_ERR_BAD_PARAMETER;

	if (clk_p->id != CLKC_REF)
		return 0;

	sel = CLK_READ(CKGC_BASE_ADDRESS + CKGC_FS_CFG(0)) >> 23;
	switch (sel & 0x3) {
	case 0:
	case 1:
		clk_p->parent = &clk_clocks[CLK_SYS];
		clk_p->rate = clk_p->parent->rate;
		break;
	case 2:
		clk_p->parent = &clk_clocks[CLK_SYSALT];
		clk_p->rate = clk_p->parent->rate;
		break;
	default:
		clk_p->parent = NULL;
		break;
	}
	return 0;
}

/* ========================================================================
   Name:	clkgenc_set_parent
   Description: Set parent clock
   Returns:  'clk_err_t' error code.
   ======================================================================== */

static int clkgenc_set_parent(clk_t *clk_p, clk_t *parent_p)
{
	unsigned long sel, data;

	if (!clk_p || !parent_p)
		return CLK_ERR_BAD_PARAMETER;

	if (clk_p->id != CLKC_REF)
		return CLK_ERR_BAD_PARAMETER;

	switch (parent_p->id) {
	case CLK_SYS:
		sel = 0;
		break;
	case CLK_SYSALT:
		sel = 2;
		break;
	default:
		return CLK_ERR_BAD_PARAMETER;
	}
	data = CLK_READ(CKGC_BASE_ADDRESS + CKGC_FS_CFG(0))
		& ~(0x3 << 23);
	CLK_WRITE(CKGC_BASE_ADDRESS + CKGC_FS_CFG(0),
		data | (sel << 23));
	clk_p->parent = parent_p;

	clkgenc_recalc(clk_p);
	return 0;
}

/* ========================================================================
   Name:	clkgenc_init
   Description: Read HW status to initialize 'clk_t' structure.
   Returns:  'clk_err_t' error code.
   ======================================================================== */

static int clkgenc_init(clk_t *clk_p)
{
	int err;

	if (!clk_p)
		return CLK_ERR_BAD_PARAMETER;

	err = clkgenc_identify_parent(clk_p);
	if (!err)
		err = clkgenc_recalc(clk_p);

	return err;
}

/* ========================================================================
   Name:	clkgenc_enable
   Description: enable clock
   Returns:  'clk_err_t' error code.
   ======================================================================== */

static const int clkgenc_enable_mask[] = { 0x440, 0x880, 0x1100, 0x2200 };

static int clkgenc_enable(clk_t *clk_p)
{
	unsigned long reg_value = 0;

	if (!clk_p)
		return CLK_ERR_BAD_PARAMETER;

	if (clk_p->id == CLKC_REF)
		return 0;

	if (clk_p->id == CLKC_FS0) {
		reg_value = CLK_READ(CKGC_BASE_ADDRESS + CKGC_FS_CFG(0));
		CLK_WRITE(CKGC_BASE_ADDRESS + CKGC_FS_CFG(0),
				reg_value | 0x4000);
		clk_p->rate = clk_p->parent->rate;
		return 0;
	}
	reg_value = CLK_READ(CKGC_BASE_ADDRESS + CKGC_FS_CFG(0));
	reg_value |= clkgenc_enable_mask[clk_p->id - CLKC_FS0_CH1];

	CLK_WRITE(CKGC_BASE_ADDRESS, reg_value);
	if ((reg_value & 0x1DC0) && !(reg_value & 0x4000)) {
		reg_value = reg_value | 0x4000;
		CLK_WRITE(CKGC_BASE_ADDRESS + CKGC_FS_CFG(0), reg_value);
	}

	return clkgenc_recalc(clk_p);
}

/* ========================================================================
   Name:	clkgenc_disable
   Description: disable clock
   Returns:  'clk_err_t' error code.
   ======================================================================== */

static int clkgenc_disable(clk_t *clk_p)
{
	unsigned long reg_value = 0;

	reg_value = CLK_READ(CKGC_BASE_ADDRESS);
	reg_value &= ~clkgenc_enable_mask[clk_p->id - CLKC_FS0_CH1];

	if (clk_p->id == CLKC_REF)
		return CLK_ERR_BAD_PARAMETER;

	CLK_WRITE(CKGC_BASE_ADDRESS, reg_value);
	if (!(reg_value & 0x1DC0)) {
		reg_value = reg_value & 0xFFFFBFFF;
		CLK_WRITE(CKGC_BASE_ADDRESS + CKGC_FS_CFG(0), reg_value);
	}

	clk_p->rate = 0;

	return 0;
}

/******************************************************************************
CLOCKGEN D (LMI)
******************************************************************************/

/* ========================================================================
   Name:	clkgend_recalc
   Description: Get CKGD (LMI) clocks frequencies (in Hz)
   Returns:  0=NO error
   ======================================================================== */

static int clkgend_recalc(clk_t *clk_p)
{
	unsigned long rdiv, ddiv;

	if (!clk_p)
		return CLK_ERR_BAD_PARAMETER;

	/* Cleaning structure first */
	clk_p->rate = 0;

	if (clk_p->id == CLKD_REF)
		clk_p->rate = clk_p->parent->rate;
	else if (clk_p->id == CLKD_LMI2X) {
		rdiv = SYSCONF_READ(SYS_CFG, 11, 9, 11);
		ddiv = SYSCONF_READ(SYS_CFG, 11, 1, 8);
		clk_p->rate =
		 (((clk_p->parent->rate / 1000000) * ddiv) / rdiv) * 1000000;
	} else
		return CLK_ERR_BAD_PARAMETER;	/* Unknown clock */

	return 0;
}

/* ========================================================================
   Name:	clkgend_identify_parent
   Description: Identify parent clock
   Returns:  clk_err_t
   ======================================================================== */

static int clkgend_identify_parent(clk_t *clk_p)
{
	unsigned long sel;

	if (!clk_p)
		return CLK_ERR_BAD_PARAMETER;

	if (clk_p->id != CLKD_REF)
		return 0;

	sel = SYSCONF_READ(SYS_CFG, 40, 0, 1);
	switch (sel) {
	case 0:
	case 1:
		clk_p->parent = &clk_clocks[CLK_SYS];
		break;
	case 2:
		clk_p->parent = &clk_clocks[CLK_SYSALT];
		break;
	default:
		clk_p->parent = NULL;
		break;
	}

	return 0;
}

/* ========================================================================
   Name:	clkgend_set_parent
   Description: Set parent clock
   Returns:  'clk_err_t' error code.
   ======================================================================== */

static int clkgend_set_parent(clk_t *clk_p, clk_t *parent_p)
{
	unsigned long sel;

	if (!clk_p || !parent_p)
		return CLK_ERR_BAD_PARAMETER;

	if (clk_p->id != CLKD_REF)
		return CLK_ERR_BAD_PARAMETER;

	switch (parent_p->id) {
	case CLK_SYS:
		sel = 0;
		break;
	case CLK_SYSALT:
		sel = 1;
		break;
	default:
		return CLK_ERR_BAD_PARAMETER;
	}

	SYSCONF_WRITE(SYS_CFG, 40, 0, 1, sel);
	clk_p->parent = parent_p;

	return clkgend_recalc(clk_p);
}

/* ========================================================================
   Name:	clkgend_init
   Description: Read HW status to initialize 'clk_t' structure.
   Returns:  'clk_err_t' error code.
   ======================================================================== */

static int clkgend_init(clk_t *clk_p)
{
	int err;

	if (!clk_p)
		return CLK_ERR_BAD_PARAMETER;

	err = clkgend_identify_parent(clk_p);
	if (!err)
		err = clkgend_recalc(clk_p);

	return err;
}

/******************************************************************************
CLOCKGEN E (USB)
******************************************************************************/

/* ========================================================================
   Name:	clkgene_recalc
   Description: Get CKGE (USB) clocks frequencies (in Hz)
   Returns:  0=NO error
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
   Name:	clkgene_identify_parent
   Description: Identify parent clock
   Returns:  clk_err_t
   ======================================================================== */

static int clkgene_identify_parent(clk_t *clk_p)
{
	unsigned long sel;

	if (!clk_p)
		return CLK_ERR_BAD_PARAMETER;

	if (clk_p->id == CLKE_REF) {
		sel = SYSCONF_READ(SYS_CFG, 40, 2, 3);
		switch (sel) {
		case 0:
		case 1:
			clk_p->parent = &clk_clocks[CLK_SYS];
			break;
		case 2:
			clk_p->parent = &clk_clocks[CLK_SYSALT];
			break;
		default:
			clk_p->parent = NULL;
			break;
		}
	} else
		clk_p->parent = &clk_clocks[CLKE_REF];

	return 0;
}

/* ========================================================================
   Name:	clkgene_set_parent
   Description: Change parent clock
   Returns:  Pointer on parent 'clk_t' structure, or NULL (none or error)
   ======================================================================== */

static int clkgene_set_parent(clk_t *clk_p, clk_t *parent_p)
{
	unsigned long sel;

	if (!clk_p)
		return CLK_ERR_BAD_PARAMETER;

	if (clk_p->id == CLKE_REF) {
		switch (parent_p->id) {
		case CLK_SYS:
			sel = 0;
			break;
		case CLK_SYSALT:
			sel = 1;
			break;
		default:
			return CLK_ERR_BAD_PARAMETER;
		}
		SYSCONF_WRITE(SYS_CFG, 40, 2, 3, sel);
		clk_p->parent = parent_p;
	} else
		return CLK_ERR_BAD_PARAMETER;

	return clkgene_recalc(clk_p);
}

/* ========================================================================
   Name:	clkgene_init
   Description: Read HW status to initialize 'clk_t' structure.
   Returns:  'clk_err_t' error code.
   ======================================================================== */

static int clkgene_init(clk_t *clk_p)
{
	int err;

	if (!clk_p)
		return CLK_ERR_BAD_PARAMETER;

	err = clkgene_identify_parent(clk_p);
	if (!err)
		err = clkgene_recalc(clk_p);

	return err;
}
