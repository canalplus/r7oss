/*****************************************************************************
 *
 * File name   : clock-common.c
 * Description : Low Level API - Common LLA functions (SOC independant)
 *
 * COPYRIGHT (C) 2009 STMicroelectronics - All Rights Reserved
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 *****************************************************************************/

/* ----- Modification history (most recent first)----
13/oct/09 fabrice.charpentier@st.com
	  clk_fsyn_get_rate() API changed. Now returns error code.
30/sep/09 fabrice.charpentier@st.com
	  Introducing clk_pll800_get_rate() & clk_pll1600_get_rate() to
	  replace clk_pll800_freq() & clk_pll1600_freq().
*/

#include <linux/clk.h>
#include <asm-generic/div64.h>

/*
 * Linux specific function
 */

/* Return the number of set bits in x. */
static unsigned int population(unsigned int x)
{
	/* This is the traditional branch-less algorithm for population count */
	x = x - ((x >> 1) & 0x55555555);
	x = (x & 0x33333333) + ((x >> 2) & 0x33333333);
	x = (x + (x >> 4)) & 0x0f0f0f0f;
	x = x + (x << 8);
	x = x + (x << 16);

	return x >> 24;
}

/* Return the index of the most significant set in x.
 * The results are 'undefined' is x is 0 (0xffffffff as it happens
 * but this is a mere side effect of the algorithm. */
static unsigned int most_significant_set_bit(unsigned int x)
{
	/* propagate the MSSB right until all bits smaller than MSSB are set */
	x = x | (x >> 1);
	x = x | (x >> 2);
	x = x | (x >> 4);
	x = x | (x >> 8);
	x = x | (x >> 16);

	/* now count the number of set bits [clz is population(~x)] */
	return population(x) - 1;
}

#include "clock-oslayer.h"
#include "clock-common.h"

int clk_info_get_index(unsigned long clk_id, struct clk_info *table,
		    unsigned long t_size)
{
	int i;

	for (i = 0; i < t_size; ++i)
		if (table[i].clk_id == clk_id)
			return i;
	return -1;
}

/* ========================================================================
   Name:	clk_pll800_freq()
   Description: Convert PLLx_CFG to freq for PLL800
   Params:   'input' freq (Hz), 'cfg'=PLLx_CFG register value
   ======================================================================== */
/*
 * OBSOLETE FUNCTION !! clk_pll800_get_rate() to use instead !!!!
 */

unsigned long clk_pll800_freq(unsigned long input, unsigned long cfg)
{
	unsigned long freq, ndiv, pdiv, mdiv;

	mdiv = (cfg >> 0) & 0xff;
	ndiv = (cfg >> 8) & 0xff;
	pdiv = (cfg >> 16) & 0x7;
	freq = (((2 * (input / 1000) * ndiv) / mdiv) / (1 << pdiv)) * 1000;

	return freq;
}

/* ========================================================================
   Name:	clk_pll800_get_rate()
   Description: Convert input/mdiv/ndiv/pvid values to frequency for PLL800
   Params:   'input' freq (Hz), mdiv/ndiv/pvid values
   Output:   '*rate' updated
   Return:   Error code.
   ======================================================================== */

int clk_pll800_get_rate(unsigned long input, unsigned long mdiv,
			unsigned long ndiv, unsigned long pdiv,
			unsigned long *rate)
{
	if (!mdiv)
		return CLK_ERR_BAD_PARAMETER;

	/* Note: input is divided by 1000 to avoid overflow */
	*rate = (((2 * (input / 1000) * ndiv) / mdiv) / (1 << pdiv)) * 1000;

	return 0;
}

/* ========================================================================
   Name:	clk_pll1600_freq()
   Description: Convert PLLx_CFG to freq for PLL1600
	  Always returns HS output value.
   Params:   'input' freq (Hz), 'cfg'=PLLx_CFG register value
   ======================================================================== */

/*
 * OBSOLETE function !!!
 * clk_pll1600_get_rate() to be used instead  !!!
 */

unsigned long clk_pll1600_freq(unsigned long input, unsigned long cfg)
{
	unsigned long freq, ndiv, mdiv;

	mdiv = (cfg >> 0) & 0x7;
	ndiv = (cfg >> 8) & 0xff;
	freq = ((2 * (input / 1000) * ndiv) / mdiv) * 1000;

	return freq;
}

/* ========================================================================
   Name:	clk_pll1600_get_rate()
   Description: Convert input/mdiv/ndiv values to frequency for PLL1600
   Params:   'input' freq (Hz), mdiv/ndiv values
	  Info: mdiv also called rdiv, ndiv also called ddiv
   Output:   '*rate' updated with value of HS output.
   Return:   Error code.
   ======================================================================== */

int clk_pll1600_get_rate(unsigned long input, unsigned long mdiv,
			 unsigned long ndiv, unsigned long *rate)
{
	if (!mdiv)
		return CLK_ERR_BAD_PARAMETER;

	/* Note: input is divided by 1000 to avoid overflow */
	*rate = ((2 * (input / 1000) * ndiv) / mdiv) * 1000;

	return 0;
}

/* ========================================================================
   Name:	clk_fsyn_get_rate()
   Description: Parameters to freq computation for frequency synthesizers
   ======================================================================== */

/* This has to be enhanced to support several Fsyn types */

unsigned long clk_fsyn_get_rate(unsigned long input, unsigned long pe,
				unsigned long md, unsigned long sd)
{
	int md2 = md;
	long long p, q, r, s, t, u;
	if (md & 0x10)
		md2 = md | 0xfffffff0;	/* adjust the md sign */

	input *= 8;

	p = 1048576ll * input;
	q = 32768 * md2;
	r = 1081344 - pe;
	s = r + q;
	t = (1 << (sd + 1)) * s;
	u = div64_64(p, t);

	return u;
}

/* ========================================================================
   Name:	clk_pll800_get_params()
   Description: Freq to parameters computation for PLL800
   Input: input,output=input/output freqs (Hz)
   Output:   updated *mdiv, *ndiv & *pdiv
   Return:   'clk_err_t' error code
   ======================================================================== */

/*
 * The PLL_800 equation is:
 *
 *	  2 * N * Fin Mhz
 * Fout Mhz = -----------------		[1]
 *	  M * (2 ^ P)
 *
 * The algorithm sets:
 * M = Fin  / 500000			[2]
 * N = Fout / 500000			[3]
 *
 * Thefore [1][2][3] becomes
 *
 *	2 * (Fout / 500000) * Fin
 * Fout = --------------------------	[4]
 *	(2 ^ P) * (Fin  / 500000)
 *
 * Now the algorithm search the right P value
 * during an interation in [0, 32]
 */

int clk_pll800_get_params(unsigned long input, unsigned long output,
			  unsigned long *mdiv, unsigned long *ndiv,
			  unsigned long *pdiv)
{
	int ret = 0;
	unsigned long input_scaled = input / 500000;
	unsigned long output_scaled = output / 500000;

	*mdiv = input_scaled;
	*ndiv = output_scaled;
	*pdiv = 0;

	for (*pdiv = 0; *pdiv < 33; ++*pdiv)
		if (((2 * input_scaled * *ndiv) / (*mdiv * (2 << *pdiv)))
		 == output_scaled)
			break;
	if (*pdiv == 33)
		return -1;

	return 0;
}

/* ========================================================================
   Name:	clk_pll1600_get_params()
   Description: Freq to parameters computation for PLL1600
   Input: input,output=input/output freqs (Hz)
   Output:   updated *mdiv & *ndiv
   Return:   'clk_err_t' error code
   ======================================================================== */

/*
 * The PLL equation is:
 *
 *	   2 * N * Fin Mhz
 * Fout Mhz = -------------------- [1]
 *		 M
 *
 *	2 * N * Fin
 * Fout = -----------
 *	 M
 *
 * The algorithm sets:
 * M = 4 			[2]
 *
 * and N becomes
 *
 * Fount * 2 = N * Fin		[3]
 *
 * 2 * Fout
 * N =  ---------
 *	Fin
 */

int clk_pll1600_get_params(unsigned long input, unsigned long output,
			   unsigned long *mdiv, unsigned long *ndiv)
{
	*mdiv = 4;		/* as medium value allowed between [1, 7] */

	input /= 1000;
	output /= 1000;

	*ndiv = (output * 2) / input;

	return (*mdiv <= 7 && *mdiv > 0 && *ndiv > 0 && *ndiv <= 255) ? 0 : -1;
}

/* ========================================================================
   Name:	clk_fsyn_get_params()
   Description: Freq to parameters computation for frequency synthesizers
   Input: input=input freq (Hz), output=output freq (Hz)
   Output:   updated *md, *pe & *sdiv
   Return:   'clk_err_t' error code
   ======================================================================== */

/* This has to be enhanced to support several Fsyn types.
   Currently based on C090_4FS216_25. */

int clk_fsyn_get_params(int input, int output, int *md, int *pe, int *sdiv)
{
	unsigned long long p, q;
	unsigned int predivide;
	int preshift;		/* always +ve but used in subtraction */
	unsigned int lsdiv;
	int lmd;
	unsigned int lpe = 1 << 14;

	/* pre-divide the frequencies */
	p = 1048576ull * input * 8;	/* <<20? */
	q = output;

	predivide = (unsigned int)div64_64(p, q);

	/* determine an appropriate value for the output divider using eqn. #4
	 * with md = -16 and pe = 32768 (and round down) */
	lsdiv = predivide / 524288;
	if (lsdiv > 1) {
		/* sdiv = fls(sdiv) - 1; // this doesn't work
		 * for some unknown reason */
		lsdiv = most_significant_set_bit(lsdiv);
	} else
		lsdiv = 1;

	/* pre-shift a common sub-expression of later calculations */
	preshift = predivide >> lsdiv;

	/* determine an appropriate value for the coarse selection using eqn. #5
	 * with pe = 32768 (and round down which for signed values means away
	 * from zero) */
	lmd = ((preshift - 1048576) / 32768) - 1;	/* >>15? */

	/* calculate a value for pe that meets the output target */
	lpe = -1 * (preshift - 1081344 - (32768 * lmd));	/* <<15? */

	/* finally give sdiv its true hardware form */
	lsdiv--;
	/* special case for 58593.75Hz and harmonics...
	 * can't quite seem to get the rounding right */
	if (lmd == -17 && lpe == 0) {
		lmd = -16;
		lpe = 32767;
	}

	/* update the outgoing arguments */
	*sdiv = lsdiv;
	*md = lmd;
	*pe = lpe;

	/* return 0 if all variables meet their contraints */
	return (lsdiv <= 7 && -16 <= lmd && lmd <= -1 && lpe <= 32767) ? 0 : -1;
}

/* ========================================================================
   Name:	clk_err_string
   Description: Convert LLA error code to string.
   Returns:  const char *ErrMessage
   ======================================================================== */

const char *clk_err_string(int err)
{
	static const char *errors[] = { "unknown error",
		"feature not supported",
		"bad parameter",
		"fatal error"
	};

	if (err > CLK_ERR_INTERNAL)
		return errors[0];

	return errors[err];
}
