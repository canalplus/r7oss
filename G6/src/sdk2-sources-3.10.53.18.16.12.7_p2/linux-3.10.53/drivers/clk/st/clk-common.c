/*****************************************************************************
 *
 * File name   : clk-common.c
 * Description : Low Level API - Common functions (SOC independant)
 *
 * COPYRIGHT (C) 2009 STMicroelectronics - All Rights Reserved
 * May be copied or modified under the terms of the GNU General Public
 * License v2.  See linux/COPYING for more information.
 *
 *****************************************************************************/

/* ----- Modification history (most recent first)----
29/jan/14 fabrice.charpentier@st.com
	  Added PLL4600C28 support. clk_pll3200c32_get_rate() fix for IDF=0.
18/sep/13 fabrice.charpentier@st.com
	  clk_fs660c32_vco_get_rate() out of range checks.
03/jul/13 fabrice.charpentier@st.com
	  clk_fs216c65_get_params() bug fix; checks if output is 0
30/oct/12 fabrice.charpentier@st.com
	  FS660C32 migrated to new API.
29/oct/12 fabrice.charpentier@st.com
	  FS216C65 & FS432C65 migrated to new API. FS216 enhanced to support
	  variable NSDIV.
21/sep/12 fabrice.charpentier@st.com, francesco.virlinzi@st.com
	  New API for 800C65, 1200C32, 1600C45, 1600C65 & 3200C32 PLLs.
04/sep/12 fabrice.charpentier@st.com
	  clk_pll1600c65_get_rate(); now support IDF=0
28/aug/12 fabrice.charpentier@st.com
	  PLL1600C45: get_rate() & get_phi_rate() now using 64bit div.
21/aug/12 fabrice.charpentier@st.com
	  PLL800C65 revisited for better precision & win32 compilation support.
	  Using 64bit div now.
07/aug/12 fabrice.charpentier@st.com
	  FS660C32 enhancement. Better result for low freqs.
	  FS660C32 & FS432C65 NSDIV search loop & order change.
	  clk_pll1200c32_get_rate(): added support for "odf==0" & "idf==0".
30/jul/12 fabrice.charpentier@st.com
	  clk_pll3200c32_get_params() & clk_pll1600c45_get_params()
	    charge pump computation bug fixes.
28/jun/12 fabrice.charpentier@st.com
	  clk_fs432c65_get_params() bug fix for 64bits division under Linux
28/jun/12 fabrice.charpentier@st.com
	  clk_fs216c65_get_params() bug fix for 64bits division under Linux.
19/jun/12 Ravinder SINGH
	  clk_pll1600c45_get_phi_params() fix.
30/apr/12 fabrice.charpentier@st.com
	  FS660C32 fine tuning to get better result.
26/apr/12 fabrice.charpentier@st.com
	  FS216 & FS432 fine tuning to get better result.
24/apr/12 fabrice.charpentier@st.com
	  FS216, FS432 & FS660: changed sdiv search order from highest to lowest
	  as recommended by Anand K.
18/apr/12 fabrice.charpentier@st.com
	  Added FS432C65 algo.
13/apr/12 fabrice.charpentier@st.com
	  FS216C65 MD order changed to recommended -16 -> -1 then -17.
05/apr/12 fabrice.charpentier@st.com
	  FS216C65 fully revisited to have 1 algo only for Linux & OS21.
28/mar/12 fabrice.charpentier@st.com
	  FS660C32 algos merged from Liege required improvements.
25/nov/11 fabrice.charpentier@st.com
	  Functions rename to support several algos for a same PLL/FS.
28/oct/11 fabrice.charpentier@st.com
	  Added PLL1600 CMOS045 support for Lille
27/oct/11 fabrice.charpentier@st.com
	  PLL1200 functions revisited. API changed.
27/jul/11 fabrice.charpentier@st.com
	  FS660 algo enhancement.
14/mar/11 fabrice.charpentier@st.com
	  Added PLL1200 functions.
07/mar/11 fabrice.charpentier@st.com
	  clk_pll3200c32_get_params() revisited.
11/mar/10 fabrice.charpentier@st.com
	  clk_pll800c65_get_params() fully revisited.
10/dec/09 francesco.virlinzi@st.com
	  clk_pll1600c65_get_params() now same code for OS21 & Linux.
13/oct/09 fabrice.charpentier@st.com
	  clk_fs216c65_get_rate() API changed. Now returns error code.
30/sep/09 fabrice.charpentier@st.com
	  Introducing clk_pll800c65_get_rate() & clk_pll1600c65_get_rate() to
	  replace clk_pll800_freq() & clk_pll1600c65_freq().
*/

#ifdef REMOVED
#include <linux/stm/clk.h>
#include <linux/clkdev.h>
#include "clock-oslayer.h"
#else
#include <linux/err.h>
#include <linux/math64.h>

/* Low level API errors */
enum clk_err {
	CLK_ERR_NONE = 0,
	CLK_ERR_FEATURE_NOT_SUPPORTED = -EPERM,
	CLK_ERR_BAD_PARAMETER = -EINVAL,
	CLK_ERR_INTERNAL = -EFAULT /* Internal & fatal error */
};
#endif
#include "clk-common.h"

#ifdef REMOVED
struct sysconf_field *(*platform_sys_claim)(int nr, int lsb, int msb);
int __init clk_register_table(struct clk *clks, int num, int enable)
{
	int i;

	for (i = 0; i < num; i++) {
		struct clk *clk = &clks[i];
		int ret;
		struct clk_lookup *cl;

		/*
		 * Some devices have clockgen outputs which are unused.
		 * In this case the LLA may still have an entry in its
		 * tables for that clock, and try and register that clock,
		 * so we need some way to skip it.
		 */
		if (!clk->name)
			continue;

		ret = clk_register(clk);
		if (ret)
			return ret;

		/*
		 * We must ignore the result of clk_enables as some of
		 * the LLA enables functions claim to support an
		 * enables function, but then fail if you call it!
		 */
		if (enable || clk->flags & CLK_ALWAYS_ENABLED) {
			ret = clk_enable(clk);
			if (ret)
				pr_warn("Failed to enable clk %s, ignoring\n",
					clk->name);
		}

		cl = clkdev_alloc(clk, clk->name, NULL);
		if (!cl)
			return -ENOMEM;
		clkdev_add(cl);
	}

	return 0;
}
#endif

/*
 * Prototypes for local functions
 */

int clk_fs216c65_get_rate(unsigned long input, struct stm_fs *fs,
		unsigned long *rate);
int clk_fs432c65_get_rate(unsigned long input, struct stm_fs *fs,
		unsigned long *rate);
int clk_fs660c32_dig_get_rate(unsigned long input, struct stm_fs *fs,
			   unsigned long *rate);
/*
 * PLL800
 */

/* ========================================================================
   Name:	clk_pll800c65_get_params()
   Description: Freq to parameters computation for PLL800 CMOS65
   Input:       input & output freqs (Hz)
   Output:      updated *mdiv, *ndiv & *pdiv (register values)
   Return:      'clk_err_t' error code
   ======================================================================== */

/*
 * PLL800 in FS mode computation algo
 *
 *           2 * NDIV * Fin
 * Fout = -----------------		using FS MODE
 *            MDIV * PDIV
 *
 * Rules:
 *   6.25Mhz <= output <= 800Mhz
 *   FS mode means 3 <= NDIV <= 255
 *   1 <= MDIV <= 255
 *   PDIV: 1, 2, 4, 8, 16 or 32
 *   1Mhz <= INFIN (input/MDIV) <= 50Mhz
 *   200Mhz <= FVCO (input*2*N/M) <= 800Mhz
 *   For better long term jitter select M minimum && P maximum
 */

static int clk_pll800c65_get_params(unsigned long input,
	unsigned long output, struct stm_pll *pll)
{
	unsigned long m, n, p, pi, infin;
	unsigned long deviation = 0xffffffff;
	unsigned long new_freq;
	long new_deviation;
	uint64_t res;
	struct stm_pll temp_pll = {
		.type = pll->type,
	};

	/* Output clock range: 6.25Mhz to 800Mhz */
	if (output < 6250000 || output > 800000000)
		return CLK_ERR_BAD_PARAMETER;

	for (m = 1; (m < 255) && deviation; m++) {
		infin = input / m; /* 1Mhz <= INFIN <= 50Mhz */
		if (infin < 1000000 || infin > 50000000)
			continue;

		for (pi = 5; pi <= 5 && deviation; pi--) {
			p = 1 << pi;
			res = (uint64_t)m * (uint64_t)p * (uint64_t)output;
			n = (unsigned long)div64_u64(res, input * 2);

			/* Checks */
			if (n < 3)
				break;
			if (n > 255)
				continue;

			/* 200Mhz <= FVCO <= 800Mhz */
			res = div64_u64(res, m);
			if (res > 800000000)
				continue;
			if (res < 200000000)
				break;

			temp_pll.mdiv = m;
			temp_pll.ndiv = n;
			temp_pll.pdiv = pi;
			stm_clk_pll_get_rate(input, &temp_pll, &new_freq);
			new_deviation = new_freq - output;
			if (new_deviation < 0)
				new_deviation = -new_deviation;
			if (new_deviation < deviation) {
				pll->mdiv = m;
				pll->ndiv = n;
				pll->pdiv = pi;
				deviation = new_deviation;
			}
		}
	}

	if (deviation == 0xffffffff) /* No solution found */
		return CLK_ERR_BAD_PARAMETER;
	return 0;
}

/* ========================================================================
   Name:	clk_pll800c65_get_rate()
   Description: Convert input/mdiv/ndiv/pvid values to frequency for PLL800
   Params:      'input' freq (Hz), mdiv/ndiv/pvid values
   Output:      '*rate' updated
   Return:      Error code.
   ======================================================================== */

int
clk_pll800c65_get_rate(unsigned long input, struct stm_pll *pll,
		       unsigned long *rate)
{
	uint64_t res;
	unsigned long mdiv = pll->mdiv;

	if (!mdiv)
		mdiv++; /* mdiv=0 or 1 => MDIV=1 */

	res = (uint64_t)2 * (uint64_t)input * (uint64_t)pll->ndiv;
	*rate = (unsigned long)div64_u64(res, mdiv * (1 << pll->pdiv));

	return 0;
}

/*
 * PLL1200
 */

/* ========================================================================
   Name:	clk_pll1200c32_get_params()
   Description: PHI freq to parameters computation for PLL1200.
   Input:       input=input freq (Hz),output=output freq (Hz)
		WARNING: Output freq is given for PHI (FVCO/ODF).
   Output:      updated *idf, *ldf, & *odf
   Return:      'clk_err_t' error code
   ======================================================================== */

/* PLL output structure
 *   FVCO >> Divider (ODF) >> PHI
 *
 * PHI = (INFF * LDF) / (ODF * IDF) when BYPASS = L
 *
 * Rules:
 *   9.6Mhz <= input (INFF) <= 350Mhz
 *   600Mhz <= FVCO <= 1200Mhz
 *   9.52Mhz <= PHI output <= 1200Mhz
 *   1 <= i (register value for IDF) <= 7
 *   8 <= l (register value for LDF) <= 127
 *   1 <= odf (register value for ODF) <= 63
 */

static int
clk_pll1200c32_get_params(unsigned long input, unsigned long output,
			  struct stm_pll *pll)
{
	unsigned long i, l, o; /* IDF, LDF, ODF values */
	unsigned long deviation = 0xffffffff;
	unsigned long new_freq;
	long new_deviation;

	/* Output clock range: 9.52Mhz to 1200Mhz */
	if (output < 9520000 || output > 1200000000)
		return CLK_ERR_BAD_PARAMETER;

	/* Computing Output Division Factor */
	if (output < 600000000) {
		o = 600000000 / output;
		if (600000000 % output)
			o = o + 1;
	} else {
		o = 1;
	}

	input /= 1000;
	output /= 1000;

	for (i = 1; (i <= 7) && deviation; i++) {
		l = i * output * o / input;

		/* Checks */
		if (l < 8)
			continue;
		if (l > 127)
			break;

		new_freq = (input * l) / (i * o);
		new_deviation = new_freq - output;
		if (new_deviation < 0)
			new_deviation = -new_deviation;
		if (!new_deviation || new_deviation < deviation) {
			pll->idf = i;
			pll->ldf = l;
			pll->odf = o;
			deviation = new_deviation;
		}
	}

	if (deviation == 0xffffffff) /* No solution found */
		return CLK_ERR_BAD_PARAMETER;
	return 0;
}

/* ========================================================================
   Name:	clk_pll1200c32_get_rate()
   Description: Convert input/idf/ldf/odf values to PHI output freq.
		WARNING: Assuming NOT BYPASS.
   Params:      'input' freq (Hz), idf/ldf/odf REGISTERS values
   Output:      '*rate' updated with value of PHI output (FVCO/ODF).
   Return:      Error code.
   ======================================================================== */

int
clk_pll1200c32_get_rate(unsigned long input, struct stm_pll *pll,
			unsigned long *rate)
{
	unsigned long idf = pll->idf, odf = pll->odf;

	if (!idf) /* idf==0 means 1 */
		idf = 1;
	if (!odf) /* odf==0 means 1 */
		odf = 1;

	/* Note: input is divided by 1000 to avoid overflow */
	*rate = (((input / 1000) * pll->ldf) / (odf * idf)) * 1000;

	return 0;
}

/*
 * PLL1600
 * WARNING: 2 types currently supported; CMOS065 & CMOS045
 */

/* ========================================================================
   Name:	clk_pll1600c45_get_params(), PL1600 CMOS45
   Description: FVCO output freq to parameters computation function.
   Input:       input,output=input/output freqs (Hz)
   Output:      updated *idf, *ndiv and *cp
   Return:      'clk_err_t' error code
   ======================================================================== */

/*
 * Spec used: CMOS045_PLL_PG_1600X_A_SSCG_FR_LSHOD25_7M4X0Y2Z_SPECS_1.1.pdf
 *
 * Rules:
 *   4Mhz <= input (INFF) <= 350Mhz
 *   800Mhz <= VCO freq (FVCO) <= 1800Mhz
 *   6.35Mhz <= output (PHI) <= 900Mhz
 *   1 <= IDF (Input Div Factor) <= 7
 *   8 <= NDIV (Loop Div Factor) <= 225
 *   1 <= ODF (Output Div Factor) <= 63
 *
 * PHI = (INFF*LDF) / (2*IDF*ODF)
 * FVCO = (INFF*LDF) / (IDF)
 * LDF = 2*NDIV (if FRAC_CONTROL=L)
 * => FVCO = INFF * 2 * NDIV / IDF
 */

int clk_pll1600c45_get_params(unsigned long input, unsigned long output,
			   struct stm_pll *pll)
{
	unsigned long i, n; /* IDF, NDIV values */
	unsigned long deviation = 0xffffffff;
	unsigned long new_freq;
	long new_deviation;
	/* Charge pump table: highest ndiv value for cp=7 to 27 */
	static const unsigned char cp_table[] = {
		71, 79, 87, 95, 103, 111, 119, 127, 135, 143,
		151, 159, 167, 175, 183, 191, 199, 207, 215,
		223, 225
	};

	/* Output clock range: 800Mhz to 1800Mhz */
	if (output < 800000000 || output > 1800000000)
		return CLK_ERR_BAD_PARAMETER;

	input /= 1000;
	output /= 1000;

	for (i = 1; (i <= 7) && deviation; i++) {
		n = (i * output) / (2 * input);

		/* Checks */
		if (n < 8)
			continue;
		if (n > 225)
			break;

		new_freq = (input * 2 * n) / i;
		new_deviation = new_freq - output;
		if (new_deviation < 0)
			new_deviation = -new_deviation;
		if (!new_deviation || new_deviation < deviation) {
			pll->idf	= i;
			pll->ndiv	= n;
			deviation = new_deviation;
		}
	}

	if (deviation == 0xffffffff) /* No solution found */
		return CLK_ERR_BAD_PARAMETER;

	/* Computing recommended charge pump value */
	for (pll->cp = 7; pll->ndiv > cp_table[pll->cp - 7]; (pll->cp)++)
		;

	return 0;
}

/* ========================================================================
   Name:	clk_pll1600c45_get_phi_params()
   Description: PLL1600 C45 PHI freq computation function
   Input:       input,output=input/output freqs (Hz)
   Output:      updated *idf, *ndiv, *odf and *cp
   Return:      'clk_err_t' error code
   ======================================================================== */

int clk_pll1600c45_get_phi_params(unsigned long input, unsigned long output,
			   struct stm_pll *pll)
{
	unsigned long o; /* ODF value */

	/* Output clock range: 6.35Mhz to 900Mhz */
	if (output < 6350000 || output > 900000000)
		return CLK_ERR_BAD_PARAMETER;

	/* Computing Output Division Factor */
	if (output < 400000000) {
		o = 400000000 / output;
		if (400000000 % output)
			o = o + 1;
	} else {
		o = 1;
	}

	pll->odf = o;

	/* Computing FVCO freq*/
	output = 2 * output * o;

	return clk_pll1600c45_get_params(input, output, pll);
}

/* ========================================================================
   Name:	clk_pll1600c45_get_rate()
   Description: Convert input/idf/ndiv REGISTERS values to FVCO frequency
   Params:      'input' freq (Hz), idf/ndiv REGISTERS values
   Output:      '*rate' updated with value of FVCO output.
   Return:      Error code.
   ======================================================================== */

int clk_pll1600c45_get_rate(unsigned long input, struct stm_pll *pll,
			    unsigned long *rate)
{
	unsigned long idf = pll->idf;
	uint64_t res;

	if (!idf)
		idf = 1;

	/* FVCO = (INFF*LDF) / (IDF)
	   LDF = 2*NDIV (if FRAC_CONTROL=L)
	   => FVCO = INFF * 2 * NDIV / IDF */

	res = (uint64_t)input * (uint64_t)2 * (uint64_t)pll->ndiv;
	*rate = (unsigned long)div64_u64(res, idf);

	return 0;
}

/* ========================================================================
   Name:	clk_pll1600c45_get_phi_rate()
   Description: Convert input/idf/ndiv/odf REGISTERS values to frequency
   Params:      'input' freq (Hz), idf/ndiv/odf REGISTERS values
   Output:      '*rate' updated with value of PHI output.
   Return:      Error code.
   ======================================================================== */

int clk_pll1600c45_get_phi_rate(unsigned long input, struct stm_pll *pll,
			    unsigned long *rate)
{
	unsigned long idf = pll->idf, odf = pll->odf;
	uint64_t res;

	if (!idf)
		idf = 1;
	if (!odf)
		odf = 1;

	/* PHI = (INFF*LDF) / (2*IDF*ODF)
	   LDF = 2*NDIV (if FRAC_CONTROL=L)
	   => PHI = (INFF*NDIV) / (IDF*ODF) */

	res = (uint64_t)input * (uint64_t)pll->ndiv;
	*rate = (unsigned long)div64_u64(res, idf * odf);

	return 0;
}

/* ========================================================================
   Name:	clk_pll1600c65_get_params()
   Description: Freq to parameters computation for PLL1600 CMOS65
   Input:       input,output=input/output freqs (Hz)
   Output:      updated *mdiv (rdiv) & *ndiv (ddiv)
   Return:      'clk_err_t' error code
   ======================================================================== */

/*
 * Spec used: PLL_PG_1600x_CMOS065LP_SPECS_1.4.pdf
 *
 * Rules:
 *   600Mhz <= output (FVCO) <= 1800Mhz
 *   1 <= M (also called R) <= 7
 *   4 <= N <= 255
 *   4Mhz <= PFDIN (input/M) <= 75Mhz
 *
 * FVCO = (INFF*LDF) / IDF
 * LDF = 2*NDIV
 * => FVCO = (INFF*2*NDIV) / IDF
 */

static int
clk_pll1600c65_get_params(unsigned long input, unsigned long output,
			  struct stm_pll *pll)
{
	unsigned long m, n, pfdin;
	unsigned long deviation = 0xffffffff;
	unsigned long new_freq;
	long new_deviation;

	/* FVCO output clock range: 600Mhz to 1800Mhz */
	if (output < 600000000 || output > 1800000000)
		return CLK_ERR_BAD_PARAMETER;

	input /= 1000;
	output /= 1000;

	for (m = 1; (m <= 7) && deviation; m++) {
		n = m * output / (input * 2);

		/* Checks */
		if (n < 4)
			continue;
		if (n > 255)
			break;
		pfdin = input / m; /* 4Mhz <= PFDIN <= 75Mhz */
		if (pfdin < 4000 || pfdin > 75000)
			continue;

		new_freq = (input * 2 * n) / m;
		new_deviation = new_freq - output;
		if (new_deviation < 0)
			new_deviation = -new_deviation;
		if (!new_deviation || new_deviation < deviation) {
			pll->mdiv = m;
			pll->ndiv = n;
			deviation = new_deviation;
		}
	}

	if (deviation == 0xffffffff) /* No solution found */
		return CLK_ERR_BAD_PARAMETER;
	return 0;
}

/* ========================================================================
   Name:	clk_pll1600c65_get_rate()
   Description: Convert input/mdiv/ndiv values to frequency for PLL1600
   Params:      'input' freq (Hz), mdiv/ndiv values
		Info: mdiv also called rdiv, ndiv also called ddiv
   Output:      '*rate' updated with value of HS output.
   Return:      Error code.
   ======================================================================== */

int
clk_pll1600c65_get_rate(unsigned long input, struct stm_pll *pll,
			unsigned long *rate)
{
	unsigned long mdiv = pll->mdiv;

	if (!mdiv)
		mdiv = 1; /* BIN to FORMULA conversion */

	/* Note: input is divided by 1000 to avoid overflow */
	*rate = ((2 * (input/1000) * pll->ndiv) / mdiv) * 1000;

	return 0;
}

/*
 * PLL3200
 */

/* ========================================================================
   Name:	clk_pll3200c32_get_params()
   Description: Freq to parameters computation for PLL3200 CMOS32
   Input:       input=input freq (Hz), output=FVCOBY2 freq (Hz)
   Output:      updated *idf & *ndiv, plus *cp value (charge pump)
   Return:      'clk_err_t' error code
   ======================================================================== */

/* PLL output structure
 * VCO >> /2 >> FVCOBY2
 *                 |> Divider (ODF0) >> PHI0
 *                 |> Divider (ODF1) >> PHI1
 *                 |> Divider (ODF2) >> PHI2
 *                 |> Divider (ODF3) >> PHI3
 *
 * FVCOby2 output = (input*4*NDIV) / (2*IDF) (assuming FRAC_CONTROL==L)
 *
 * Rules:
 *   4Mhz <= input <= 350Mhz
 *   800Mhz <= output (FVCOby2) <= 1600Mhz
 *   1 <= i (register value for IDF) <= 7
 *   8 <= n (register value for NDIV) <= 200
 */

int
clk_pll3200c32_get_params(unsigned long input, unsigned long output,
			  struct stm_pll *pll)
{
	unsigned long i, n;
	unsigned long deviation = 0xffffffff;
	unsigned long new_freq;
	long new_deviation;
	/* Charge pump table: highest ndiv value for cp=6 to 25 */
	static const unsigned char cp_table[] = {
		48, 56, 64, 72, 80, 88, 96, 104, 112, 120,
		128, 136, 144, 152, 160, 168, 176, 184, 192
	};

	/* Output clock range: 800Mhz to 1600Mhz */
	if (output < 800000000 || output > 1600000000)
		return CLK_ERR_BAD_PARAMETER;

	input /= 1000;
	output /= 1000;

	for (i = 1; (i <= 7) && deviation; i++) {
		n = i * output / (2 * input);

		/* Checks */
		if (n < 8)
			continue;
		if (n > 200)
			break;

		new_freq = (input * 2 * n) / i;
		new_deviation = new_freq - output;
		if (new_deviation < 0)
			new_deviation = -new_deviation;
		if (!new_deviation || new_deviation < deviation) {
			pll->idf  = i;
			pll->ndiv = n;
			deviation = new_deviation;
		}
	}

	if (deviation == 0xffffffff) /* No solution found */
		return CLK_ERR_BAD_PARAMETER;

	/* Computing recommended charge pump value */
	for (pll->cp = 6; pll->ndiv > cp_table[pll->cp-6]; (pll->cp)++)
		;

	return 0;
}

/* ========================================================================
   Name:	clk_pll3200c32_get_rate()
   Description: Convert input/idf/ndiv values to FVCOby2 frequency for PLL3200
   Params:      'input' freq (Hz), idf/ndiv values
   Output:      '*rate' updated with value of FVCOby2 output (PHIx / 1).
   Return:      Error code.
   ======================================================================== */

int
clk_pll3200c32_get_rate(unsigned long input, struct stm_pll *pll,
			unsigned long *rate)
{
	if (!pll->idf)
		pll->idf = 1;

	/* Note: input is divided to avoid overflow */
	*rate = ((2 * (input/1000) * pll->ndiv) / pll->idf) * 1000;

	return 0;
}

/*
 * PLL4600 C28
 */

/* ========================================================================
   Name:	clk_pll4600c28_get_params()
   Description: Freq to parameters computation for PLL4600 CMOS28
   Input:       input=input freq (Hz), output=FVCOBY2 freq (Hz)
   Output:      updated pll->idf & pll->ndiv
   Return:      'clk_err_t' error code
   ======================================================================== */

/* PLL output structure
 * FVCO >> /2 >> FVCOBY2 (no output)
 *                 |> Divider (ODF) >> PHI
 *
 * FVCOby2 output = (input * 2 * NDIV) / IDF (assuming FRAC_CONTROL==L)
 *
 * Rules:
 *   4Mhz <= INFF input <= 350Mhz
 *   4Mhz <= INFIN (INFF / IDF) <= 50Mhz
 *   19.05Mhz <= FVCOby2 output (PHI w ODF=1) <= 3000Mhz
 *   1 <= i (register/dec value for IDF) <= 7
 *   8 <= n (register/dec value for NDIV) <= 246
 */

int
clk_pll4600c28_get_params(unsigned long input, unsigned long output,
			  struct stm_pll *pll)
{
	unsigned long i, infin, n;
	unsigned long deviation = 0xffffffff;
	unsigned long new_freq, new_deviation;

	/* Output clock range: 19Mhz to 3000Mhz */
	if (output < 19000000u || output > 3000000000u)
		return CLK_ERR_BAD_PARAMETER;

	/* For better jitter, IDF should be smallest
	   and NDIV must be maximum */
	for (i = 1; (i <= 7) && deviation; i++) {
		/* INFIN checks */
		infin = input / i;
		if ((infin < 4000000) || (infin > 50000000))
			continue;	/* Invalid case */

		n = output / (infin * 2);
		if ((n < 8) || (n > 246)) {
			continue;	/* Invalid case */
		if (n < 246)
			n++;	/* To work around 'y' when n=x.y */
		}
		for (; (n >= 8) && deviation; n--) {
			new_freq = infin * 2 * n;
			if (new_freq < output)
				break;	/* Optimization: shorting loop */

			new_deviation = new_freq - output;
			if (!new_deviation || new_deviation < deviation) {
				pll->idf  = i;
				pll->ndiv = n;
				deviation = new_deviation;
			}
		}
	}

	if (deviation == 0xffffffff) /* No solution found */
		return CLK_ERR_BAD_PARAMETER;

	return 0;
}

/* ========================================================================
   Name:	clk_pll4600c28_get_rate()
   Description: Convert input/idf/ndiv values to FVCOby2 frequency
   Params:      'input' freq (Hz), idf/ndiv values
   Output:      '*rate' updated with value of FVCOby2 output (PHIx / 1).
   Return:      Error code.
   ======================================================================== */

int
clk_pll4600c28_get_rate(unsigned long input, struct stm_pll *pll,
			unsigned long *rate)
{
	if (!pll->idf)
		pll->idf = 1;

	*rate = (input / pll->idf) * 2 * pll->ndiv;

	return 0;
}

/*
 * FS216
 */

/* ========================================================================
   Name:	clk_fs216c65_get_params()
   Description: Freq to parameters computation for FS216 C65
   Input:       input=input freq (Hz), output=output freq (Hz)
   Output:      updated 'struct stm_fs *fs'
   Return:      'clk_err_t' error code
   ======================================================================== */

#define P15			(uint64_t)(1 << 15)
#define FS216_SCALING_FACTOR	4096LL

int clk_fs216c65_get_params(unsigned long input, unsigned long output,
			    struct stm_fs *fs)
{
	unsigned long nd = 8; /* ndiv value: bin stuck at 0 => value = 8 */
	unsigned long ns; /* nsdiv3 value: 3=0.bin, 1=1.bin */
	unsigned long sd; /* sdiv value = 1 << (sdiv_bin_value + 1) */
	long m; /* md value (-17 to -1) */
	uint64_t p, p2; /* pe value */
	int si;
	unsigned long new_freq, new_deviation;
	/* initial condition to say: "infinite deviation" */
	unsigned long deviation = 0xffffffff;
	int stop, ns_search_loop;
	struct stm_fs fs_tmp = {
		.type = stm_fs216c65,
	};

	/* Checks */
	if (!output)
		return CLK_ERR_BAD_PARAMETER;

	/*
	 * fs->nsdiv is a register value ('BIN') which is translated
	 * to a decimal value according to following rules.
	 * In case nsdiv is hardwired, it must be set to 0xff before calling.
	 *
	 * fs->nsdiv      ns.dec
	 *   0xff	  computed by this algo
	 *     0	  3
	 *     1	  1
	 */
	if (fs->nsdiv != 0xff) {
		ns = (fs->nsdiv ? 1 : 3);
		ns_search_loop = 1;
	} else {
		ns = 3;
		ns_search_loop = 2;
	}

	for (; (ns < 4) && ns_search_loop; ns -= 2, ns_search_loop--)
		for (si = 7; (si >= 0) && deviation; si--) {
			sd = (1 << (si + 1));
			/* Recommended search order: -16 to -1, then -17 */
			for (m = -16, stop = 0; !stop && deviation; m++) {
				if (!m) {
					m = -17; /* 0 is -17 */
					stop = 1;
				}
				p = P15 * 32 * nd * input
					* FS216_SCALING_FACTOR;
				p = div64_u64(p, sd * ns * output
					* FS216_SCALING_FACTOR);
				p2 = P15 * (uint64_t)(m + 33);
				if (p2 < p)
					continue; /* p must be >= 0 */
				p = p2 - p;

				if (p > 32768LL)
					/* Already too high.
					Let's move to next sdiv */
					break;

				fs_tmp.mdiv = (unsigned long)(m + 32);
				/* pe fine tuning: ± 2
				around computed pe value */
				if (p > 2)
					p2 = p - 2;
				else
					p2 = 0;
				for (; p2 < 32768ll && (p2 < (p + 2)); p2++) {
					fs_tmp.pe = p2;
					fs_tmp.sdiv = si;
					fs_tmp.nsdiv = (ns == 1) ? 1 : 0;
					clk_fs216c65_get_rate(input,
							      &fs_tmp,
							      &new_freq);

					if (new_freq < output)
						new_deviation = output
								- new_freq;
					else
						new_deviation = new_freq
								- output;
					/* Check if this is a better solution */
					if (new_deviation < deviation) {
						fs->mdiv = (unsigned long)
								(m + 32);
						fs->pe = (unsigned long)p2;
						fs->sdiv = si;
						fs->nsdiv = (ns == 1) ? 1 : 0;
						deviation = new_deviation;
					}
				}
			}
		}

	if (deviation == 0xffffffff) /* No solution found */
		return CLK_ERR_BAD_PARAMETER;

	return 0;
}

/* ========================================================================
   Name:	clk_fs216c65_get_rate()
   Description: Parameters to freq computation for frequency synthesizers.
   ======================================================================== */

int clk_fs216c65_get_rate(unsigned long input, struct stm_fs *fs,
		unsigned long *rate)
{
	uint64_t res;
	unsigned long ns; /* nsdiv3 value: 3=0.bin, 1=1.bin */
	unsigned long nd = 8; /* ndiv stuck at 0 => val = 8 */
	unsigned long s; /* sdiv value = 1 << (sdiv_bin + 1) */
	long m; /* md value (-17 to -1) */

	/* BIN to VAL */
	m = fs->mdiv - 32;
	s = 1 << (fs->sdiv + 1);
	ns = (fs->nsdiv ? 1 : 3);

	res = (uint64_t)(s * ns * P15 * (uint64_t)(m + 33));
	res = res - (s * ns * fs->pe);
	*rate = div64_u64(P15 * nd * input * 32, res);

	return 0;
}

/*
 * FS432 C65
 * Based on "C65_4FS432_25_um.pdf" spec
 */

/* ========================================================================
   Name:	clk_fs432c65_get_params()
   Description: Freq to parameters computation for FS432 C65
   Input:       input=input freq (Hz), output=output freq (Hz)
   Output:      updated *md, *pe, *sdiv, & *nsdiv3
   Return:      'clk_err_t' error code
   ======================================================================== */

#define FS432_SCALING_FACTOR	4096LL

int clk_fs432c65_get_params(unsigned long input, unsigned long output,
			    struct stm_fs *fs)
{
	unsigned long nd = 16; /* ndiv value; stuck at 0 (30Mhz input) */
	unsigned long ns; /* nsdiv3 value */
	unsigned long sd; /* sdiv value = 1 << (sdiv_bin_value + 1) */
	long m; /* md value (-17 to -1) */
	uint64_t p, p2; /* pe value */
	int si;
	unsigned long new_freq, new_deviation;
	/* initial condition to say: "infinite deviation" */
	unsigned long deviation = 0xffffffff;
	int stop, ns_search_loop;
	struct stm_fs fs_tmp = {
		.type = stm_fs432c65,
	};

	/*
	 * *nsdiv3 is a register value ('BIN') which is translated
	 * to a decimal value according to following rules.
	 * In case nsdiv is hardwired, it must be set to 0xff before calling.
	 *
	 *    *nsdiv   ns.dec
	 *    ff       computed by this algo
	 *    0        3
	 *    1        1
	 */
	if (fs->nsdiv != 0xff) {
		ns = (fs->nsdiv ? 1 : 3);
		ns_search_loop = 1;
	} else {
		ns = 3;
		ns_search_loop = 2;
	}

	for (; (ns < 4) && ns_search_loop; ns -= 2, ns_search_loop--)
		for (si = 7; (si >= 0) && deviation; si--) {
			sd = (1 << (si + 1));
			/* Recommended search order: -16 to -1, then -17
			 * (if 24Mhz<input<27Mhz ONLY)
			 */
			for (m = -16, stop = 0; !stop && deviation; m++) {
				if (!m) {
					/* -17 forbidden with 30Mhz */
					if (input > 27000000)
						break;
					m = -17; /* 0 is -17 */
					stop = 1;
				}
				p = P15 * 32 * nd * input
					* FS432_SCALING_FACTOR;
				p = div64_u64(p, sd * ns * output
					* FS432_SCALING_FACTOR);
				p2 = P15 * (uint64_t)(m + 33);
				if (p2 < p)
					continue; /* p must be >= 0 */
				p = p2 - p;

				if (p > 32768LL)
					/* Already too high.
					Let's move to next sdiv */
					break;

				fs_tmp.mdiv = (unsigned long)(m + 32);
				fs_tmp.pe = (unsigned long)p;
				fs_tmp.sdiv = si;
				fs_tmp.nsdiv = (ns == 1) ? 1 : 0;
				if (clk_fs432c65_get_rate(input,
							  &fs_tmp,
							  &new_freq) != 0)
					continue;

				if (new_freq < output)
					new_deviation = output - new_freq;
				else
					new_deviation = new_freq - output;
				/* Check if this is a better solution */
				if (new_deviation < deviation) {
					fs->mdiv = (unsigned long)(m + 32);
					fs->pe = (unsigned long)p;
					fs->sdiv = si;
					fs->nsdiv = (ns == 1) ? 1 : 0;
					deviation = new_deviation;
				}
			}
		}

	if (deviation == 0xffffffff) /* No solution found */
		return CLK_ERR_BAD_PARAMETER;

	return 0;
}

/* ========================================================================
   Name:	clk_fs432c65_get_rate()
   Description: Parameters to freq computation for frequency synthesizers.
   ======================================================================== */

int clk_fs432c65_get_rate(unsigned long input, struct stm_fs *fs,
		unsigned long *rate)
{
	uint64_t res;
	unsigned long nd = 16; /* ndiv value; stuck at 0 (30Mhz input) */
	long m; /* md value (-17 to -1) */
	unsigned long sd; /* sdiv value = 1 << (sdiv_bin + 1) */
	unsigned long ns; /* nsdiv3 value */

	/* BIN to VAL */
	m = fs->mdiv - 32;
	sd = 1 << (fs->sdiv + 1);
	ns = (fs->nsdiv ? 1 : 3);

	res = (uint64_t)(sd * ns * P15 * (uint64_t)(m + 33));
	res = res - (sd * ns * fs->pe);
	*rate = div64_u64(P15 * nd * input * 32, res);

	return 0;
}

/*
   FS660
   Based on C32_4FS_660MHZ_LR_EG_5U1X2T8X_um spec.

   This FSYN embed a programmable PLL which then serve the 4 digital blocks

   clkin => PLL660 => DIG660_0 => clkout0
		   => DIG660_1 => clkout1
		   => DIG660_2 => clkout2
		   => DIG660_3 => clkout3
   For this reason the PLL660 is programmed separately from digital parts.
*/

/* ========================================================================
   Name:	clk_fs660c32_vco_get_params()
   Description: Compute params for embeded PLL660
   Input:       input=input freq (Hz), output=output freq (Hz)
   Output:      updated *ndiv (register value). Note that PDIV is frozen to 1.
   Return:      'clk_err_t' error code
   ======================================================================== */

int clk_fs660c32_vco_get_params(unsigned long input,
				unsigned long output, struct stm_fs *fs)
{
/* Formula
   VCO frequency = (fin x ndiv) / pdiv
   ndiv = VCOfreq * pdiv / fin
   */
	unsigned long pdiv = 1, n;

	/* Output clock range: 384Mhz to 660Mhz */
	if (output < 384000000 || output > 660000000)
		return CLK_ERR_BAD_PARAMETER;

	if (input > 40000000)
		/* This means that PDIV would be 2 instead of 1.
		   Not supported today. */
		return CLK_ERR_BAD_PARAMETER;

	input /= 1000;
	output /= 1000;

	n = output * pdiv / input;
	if (n < 16)
		n = 16;
	fs->ndiv = n - 16; /* Converting formula value to reg value */

	return 0;
}

/* ========================================================================
   Name:	clk_fs660c32_vco_get_rate()
   Description: Compute VCO frequency of FS660 embedded PLL (PLL660)
   Input: ndiv & pdiv registers values
   Output: updated *rate (Hz)
   Returns: 0=OK, -1=can't compute with given input+ndiv
   ======================================================================== */

int clk_fs660c32_vco_get_rate(unsigned long input, struct stm_fs *fs,
			   unsigned long *rate)
{
	unsigned long nd = fs->ndiv + 16; /* ndiv value */
	unsigned long pdiv = 1; /* Frozen. Not configurable so far */

	*rate = (input * nd) / pdiv;
	if (*rate < 384000000 || *rate > 660000000)
		return -1; /* Out of range */

	return 0;
}

/* ========================================================================
   Name:	clk_fs660c32_dig_get_params()
   Description: Compute params for digital part of FS660
   Input:       input=VCO freq, output=requested freq (Hz), *nsdiv
		(0/1 if silicon frozen, or 0xff if to be computed).
   Output:      updated *nsdiv, *md, *pe & *sdiv registers values.
   Return:      'clk_err_t' error code
   ======================================================================== */

#define P20		(uint64_t)(1 << 20)

/* We use Fixed-point arithmetic in order to avoid "float" functions.*/
#define SCALING_FACTOR	2048LL

int clk_fs660c32_dig_get_params(unsigned long input,
				unsigned long output, struct stm_fs *fs)
{
	int si; /* sdiv_reg (8 downto 0) */
	unsigned long ns; /* nsdiv value (1 or 3) */
	unsigned long s; /* sdiv value = 1 << sdiv_reg */
	unsigned long m; /* md value */
	unsigned long new_freq, new_deviation;
	/* initial condition to say: "infinite deviation" */
	unsigned long deviation = 0xffffffff;
	uint64_t p, p2; /* pe value */
	int ns_search_loop; /* How many ns search trials */
	struct stm_fs fs_tmp = {
		.type = stm_fs660c32
	};

	/*
	 * *nsdiv is a register value ('BIN') which is translated
	 * to a decimal value according to following rules.
	 * In case nsdiv is hardwired, it must be set to 0xff before calling.
	 *
	 *    *nsdiv      ns.dec
	 *	ff	  computed by this algo
	 *       0        3
	 *       1        1
	 */
	if (fs->nsdiv != 0xff) {
		ns = (fs->nsdiv ? 1 : 3);
		ns_search_loop = 1;
	} else {
		ns = 3;
		ns_search_loop = 2;
	}

	for (; (ns < 4) && ns_search_loop; ns -= 2, ns_search_loop--)
		for (si = 8; (si >= 0) && deviation; si--) {
			s = (1 << si);
			for (m = 0; (m < 32) && deviation; m++) {
				p = (uint64_t)input * SCALING_FACTOR;
				p = p - SCALING_FACTOR * ((uint64_t)s
					* (uint64_t)ns * (uint64_t)output) -
					((uint64_t)s * (uint64_t)ns
					* (uint64_t)output)
					* ((uint64_t)m
					* (SCALING_FACTOR / 32LL));
				p = p * (P20 / SCALING_FACTOR);
				p = div64_u64(p, (uint64_t)((uint64_t)s
					* (uint64_t)ns * (uint64_t)output));

				if (p > 32767LL)
					continue;

				fs_tmp.mdiv = m;
				fs_tmp.pe = (unsigned long)p;
				fs_tmp.sdiv = si;
				fs_tmp.nsdiv = (ns == 1) ? 1 : 0;
				if (clk_fs660c32_dig_get_rate(input, &fs_tmp,
							      &new_freq) != 0)
					continue;
				if (new_freq < output)
					new_deviation = output - new_freq;
				else
					new_deviation = new_freq - output;
				if (new_deviation < deviation) {
					fs->mdiv = m;
					fs->pe = (unsigned long)p;
					fs->sdiv = si;
					fs->nsdiv = (ns == 1) ? 1 : 0;
					deviation = new_deviation;
				}
			}
		}

	if (deviation == 0xffffffff) /* No solution found */
		return CLK_ERR_BAD_PARAMETER;

	/* pe fine tuning if deviation not 0: ± 2 around computed pe value */
	if (deviation) {
		fs_tmp.mdiv = fs->mdiv;
		fs_tmp.sdiv = fs->sdiv;
		fs_tmp.nsdiv = fs->nsdiv;
		if (fs->pe > 2)
			p2 = fs->pe - 2;
		else
			p2 = 0;
		for (; p2 < 32768ll && (p2 <= (fs->pe + 2)); p2++) {
			fs_tmp.pe = (unsigned long)p2;
			if (clk_fs660c32_dig_get_rate(input,
						      &fs_tmp, &new_freq) != 0)
				continue;
			if (new_freq < output)
				new_deviation = output - new_freq;
			else
				new_deviation = new_freq - output;

			/* Check if this is a better solution */
			if (new_deviation < deviation) {
				fs->pe = (unsigned long)p2;
				deviation = new_deviation;
			}
		}
	}

	return 0;
}

/* ========================================================================
   Name:	clk_fs660c32_dig_get_rate()
   Description: Parameters to freq computation for frequency synthesizers.
   Inputs:	input=VCO frequency, nsdiv, md, pe, & sdiv 'BIN' values.
   Outputs:	*rate updated
   ======================================================================== */

int clk_fs660c32_dig_get_rate(unsigned long input,
				struct stm_fs *fs, unsigned long *rate)
{
	 /* sdiv value = 1 << sdiv_reg_value */
	unsigned long s = (1 << fs->sdiv);
	  /* nsdiv value (1 or 3) */
	unsigned long ns;
	uint64_t res;

	/*
	 * 'nsdiv' is a register value ('BIN') which is translated
	 * to a decimal value according to following rules.
	 *
	 *     nsdiv      ns.dec
	 *       0        3
	 *       1        1
	 */
	ns = (fs->nsdiv == 1) ? 1 : 3;

	res = (P20 * (32 + fs->mdiv) + 32 * fs->pe) * s * ns;
	*rate = (unsigned long)div64_u64(input * P20 * 32, res);
	return 0;
}

/* ========================================================================
   Name:	stm_clk_pll_get_params
   Description: Generic freq to parameters computation function
   Returns:     0=OK
   ======================================================================== */

int stm_clk_pll_get_params(unsigned long input, unsigned long output,
		struct stm_pll *pll)
{
	switch (pll->type) {
	case stm_pll800c65:
		return clk_pll800c65_get_params(input, output, pll);
	case stm_pll1200c32:
		return clk_pll1200c32_get_params(input, output, pll);
	case stm_pll1600c45:
		return clk_pll1600c45_get_params(input, output, pll);
	case stm_pll1600c45phi:
		return clk_pll1600c45_get_phi_params(input, output, pll);
	case stm_pll1600c65:
		return clk_pll1600c65_get_params(input, output, pll);
	case stm_pll3200c32:
		return clk_pll3200c32_get_params(input, output, pll);
	case stm_pll4600c28:
		return clk_pll4600c28_get_params(input, output, pll);
	};

	return CLK_ERR_BAD_PARAMETER;
}

/* ========================================================================
   Name:	stm_clk_pll_get_params
   Description: Generic parameters to freq computation function
   Returns:     0=OK
   ======================================================================== */

int stm_clk_pll_get_rate(unsigned long input, struct stm_pll *pll,
		unsigned long *output)
{
	switch (pll->type) {
	case stm_pll800c65:
		return clk_pll800c65_get_rate(input, pll, output);
	case stm_pll1200c32:
		return clk_pll1200c32_get_rate(input, pll, output);
	case stm_pll1600c45:
		return clk_pll1600c45_get_rate(input, pll, output);
	case stm_pll1600c45phi:
		return clk_pll1600c45_get_phi_rate(input, pll, output);
	case stm_pll1600c65:
		return clk_pll1600c65_get_rate(input, pll, output);
	case stm_pll3200c32:
		return clk_pll3200c32_get_rate(input, pll, output);
	case stm_pll4600c28:
		return clk_pll4600c28_get_rate(input, pll, output);
	};

	return CLK_ERR_BAD_PARAMETER;
}

/* ========================================================================
   Name:	stm_clk_fs_get_params
   Description: Generic freq to parameters computation function
   Returns:     0=OK
   ======================================================================== */

int stm_clk_fs_get_params(unsigned long input, unsigned long output,
		struct stm_fs *fs)
{
	switch (fs->type) {
	case stm_fs216c65:
		return clk_fs216c65_get_params(input, output, fs);
	case stm_fs432c65:
		return clk_fs432c65_get_params(input, output, fs);
	case stm_fs660c32vco:
		return clk_fs660c32_vco_get_params(input, output, fs);
	case stm_fs660c32:
		return clk_fs660c32_dig_get_params(input, output, fs);
	};

	return CLK_ERR_BAD_PARAMETER;
}

/* ========================================================================
   Name:	stm_clk_fs_get_params
   Description: Generic parameters to freq computation function
   Returns:     0=OK
   ======================================================================== */

int stm_clk_fs_get_rate(unsigned long input, struct stm_fs *fs,
		unsigned long *output)
{
	switch (fs->type) {
	case stm_fs216c65:
		return clk_fs216c65_get_rate(input, fs, output);
	case stm_fs432c65:
		return clk_fs432c65_get_rate(input, fs, output);
	case stm_fs660c32vco:
		return clk_fs660c32_vco_get_rate(input, fs, output);
	case stm_fs660c32:
		return clk_fs660c32_dig_get_rate(input, fs, output);
	};

	return CLK_ERR_BAD_PARAMETER;
}


