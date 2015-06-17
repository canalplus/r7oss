/************************************************************************
File  : Low Level clock API
	Common LLA functions (SOC independant)

Author: F. Charpentier <fabrice.charpentier@st.com>

Copyright (C) 2008-12 STMicroelectronics
************************************************************************/

#ifndef __CLKLLA_COMMON_H
#define __CLKLLA_COMMON_H

enum stm_pll_type {
	stm_pll800c65,
	stm_pll1200c32,
	stm_pll1600c45,
	stm_pll1600c45phi,
	stm_pll1600c65,
	stm_pll3200c32,
	stm_pll4600c28,
};

struct stm_pll {
	enum stm_pll_type type;

	unsigned long mdiv;
	unsigned long ndiv;
	unsigned long pdiv;

	unsigned long odf;
	unsigned long idf;
	unsigned long ldf;
	unsigned long cp;
};

enum stm_fs_type {
	stm_fs216c65,
	stm_fs432c65,
	stm_fs660c32vco,	/* VCO out */
	stm_fs660c32	/* DIG out */
};

struct stm_fs {
	enum stm_fs_type type;

	unsigned long ndiv;
	unsigned long mdiv;
	unsigned long pe;
	unsigned long sdiv;
	unsigned long nsdiv;
};

/* ========================================================================
   Name:	stm_clk_pll_get_params()
   Description: Freq to parameters computation for PLLs
   Input:       input=input freq (Hz), output=output freq (Hz)
   Output:      updated 'struct stm_pll *pll'
   Return:      'clk_err_t' error code
   ======================================================================== */

int stm_clk_pll_get_params(unsigned long input, unsigned long output,
		struct stm_pll *pll);

int stm_clk_pll_get_rate(unsigned long input, struct stm_pll *pll,
		unsigned long *output);

/* ========================================================================
   Name:	stm_clk_fs_get_params()
   Description: Freq to parameters computation for FSs
   Input:       input=input freq (Hz), output=output freq (Hz)
   Output:      updated 'struct stm_fs *fs'
   Return:      'clk_err_t' error code
   ======================================================================== */

int stm_clk_fs_get_params(unsigned long input, unsigned long output,
		struct stm_fs *fs);

int stm_clk_fs_get_rate(unsigned long input, struct stm_fs *fs,
		unsigned long *output);

#ifdef REMOVED
/* ========================================================================
   Name:        clk_register_table
   Description: ?
   Returns:     'clk_err_t' error code
   ======================================================================== */

int clk_register_table(struct clk *clks, int num, int enable);
#endif

/* ========================================================================
   Name:        clk_best_div
   Description: Returned closest div factor
   Returns:     Best div factor
   ======================================================================== */

static inline unsigned long
clk_best_div(unsigned long parent_rate, unsigned long rate)
{
	unsigned long best_div;

	if (rate >= parent_rate)
		best_div = 1;
	else
		best_div = parent_rate / rate +
			((rate > (2 * (parent_rate % rate))) ? 0 : 1);

	return best_div;
}

extern int clk_pll3200c32_get_params(unsigned long input, unsigned long output,
			struct stm_pll *pll);
extern int clk_pll1600c45_get_phi_params(unsigned long input,
			unsigned long output, struct stm_pll *pll);

extern int clk_pll800c65_get_rate(unsigned long input, struct stm_pll *pll,
		       unsigned long *rate);
extern int clk_pll1200c32_get_rate(unsigned long input, struct stm_pll *pll,
		       unsigned long *rate);
extern int clk_pll1600c45_get_rate(unsigned long input, struct stm_pll *pll,
		       unsigned long *rate);
extern int clk_pll1600c45_get_phi_rate(unsigned long input, struct stm_pll *pll,
		       unsigned long *rate);
extern int clk_pll1600c65_get_rate(unsigned long input, struct stm_pll *pll,
		       unsigned long *rate);
extern int clk_pll3200c32_get_rate(unsigned long input, struct stm_pll *pll,
		       unsigned long *rate);
extern int clk_pll4600c28_get_rate(unsigned long input, struct stm_pll *pll,
		       unsigned long *rate);

extern int clk_fs660c32_vco_get_rate(unsigned long, struct stm_fs *,
		unsigned long *);
extern int clk_fs216c65_get_params(unsigned long, unsigned long,
		struct stm_fs *);
extern int clk_fs432c65_get_params(unsigned long, unsigned long,
		struct stm_fs *);
extern int clk_fs660c32_vco_get_params(unsigned long, unsigned long,
		struct stm_fs *);
extern int clk_fs660c32_dig_get_params(unsigned long, unsigned long,
		struct stm_fs *);

int clk_fs216c65_get_rate(unsigned long input, struct stm_fs *fs,
                unsigned long *rate);
int clk_fs432c65_get_rate(unsigned long input, struct stm_fs *fs,
                unsigned long *rate);
int clk_fs660c32_dig_get_rate(unsigned long input, struct stm_fs *fs,
                           unsigned long *rate);

#endif /* #ifndef __CLKLLA_COMMON_H */
