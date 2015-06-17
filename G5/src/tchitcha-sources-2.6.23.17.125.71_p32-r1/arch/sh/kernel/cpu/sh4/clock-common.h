/************************************************************************
File  : Low Level clock API
	Common LLA functions (SOC independant)

Author: F. Charpentier <fabrice.charpentier@st.com>

Copyright (C) 2008 STMicroelectronics
************************************************************************/

#ifndef __CLKLLA_COMMON_H
#define __CLKLLA_COMMON_H

#define NO_MORE_RATIO		-1
#define RATIO_RESERVED		-2

int get_ratio_field(unsigned long rate, unsigned long prate, int *ratios);

struct xratio {
	unsigned long ratio;
	unsigned long field;
};

int get_xratio_field(unsigned long rate, unsigned long prate,
		  struct xratio *ratios);

/* ========================================================================
   Name:	clk_info_get_index()
   Description:
   Params:
   ======================================================================== */

struct clk_info {
	unsigned long clk_id;
	unsigned long info;
};

int clk_info_get_index(unsigned long clk_id, struct clk_info *table,
		    unsigned long t_size);

/* ========================================================================
   Name:	clk_pll800_freq()
   Description: Convert PLLx_CFG to freq for PLL800
   Params:   'input' freq (Hz), 'cfg'=PLLx_CFG register value
   ======================================================================== */

/*
 * OBSOLETE function !!!
 * clk_pll800_get_rate() to be used instead  !!!
 */
unsigned long clk_pll800_freq(unsigned long input, unsigned long cfg);

/* ========================================================================
   Name:	clk_pll800_get_rate()
   Description: Convert input/mdiv/ndiv/pvid values to frequency for PLL800
   Params:   'input' freq (Hz), mdiv/ndiv/pvid values
   Output:   '*rate' updated
   Return:   Error code.
   ======================================================================== */

int clk_pll800_get_rate(unsigned long input, unsigned long mdiv,
			unsigned long ndiv, unsigned long pdiv,
			unsigned long *rate);

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
unsigned long clk_pll1600_freq(unsigned long input, unsigned long cfg);

/* ========================================================================
   Name:	clk_pll1600_get_rate()
   Description: Convert input/mdiv/ndiv values to frequency for PLL1600
   Params:   'input' freq (Hz), mdiv/ndiv values
	  Info: mdiv also called rdiv, ndiv also called ddiv
   Output:   '*rate' updated with value of HS output
   Return:   Error code.
   ======================================================================== */

int clk_pll1600_get_rate(unsigned long input, unsigned long mdiv,
			 unsigned long ndiv, unsigned long *rate);

/* ========================================================================
   Name:	clk_fsyn_get_rate()
   Description: Parameters to freq computation for frequency synthesizers
   ======================================================================== */

/* This has to be enhanced to support several Fsyn types */

unsigned long clk_fsyn_get_rate(unsigned long input, unsigned long pe,
				unsigned long md, unsigned long sd);

/* ========================================================================
   Name:	clk_pll800_get_params()
   Description: Freq to parameters computation for PLL800
   Input: input,output=input/output freqs (Hz)
   Output:   updated *mdiv, *ndiv & *pdiv
   Return:   'clk_err_t' error code
   ======================================================================== */

int clk_pll800_get_params(unsigned long input, unsigned long output,
			  unsigned long *mdiv, unsigned long *ndiv,
			  unsigned long *pdiv);

/* ========================================================================
   Name:	clk_pll1600_get_params()
   Description: Freq to parameters computation for PLL1600
   Input: input,output=input/output freqs (Hz)
   Output:   updated *mdiv & *ndiv
   Return:   'clk_err_t' error code
   ======================================================================== */

int clk_pll1600_get_params(unsigned long input, unsigned long output,
			   unsigned long *mdiv, unsigned long *ndiv);

/* ========================================================================
   Name:	clk_fsyn_get_params()
   Description: Freq to parameters computation for frequency synthesizers
   Input: input=input freq (Hz), output=output freq (Hz)
   Output:   updated *md, *pe & *sdiv
   Return:   'clk_err_t' error code
   ======================================================================== */

/* This has to be enhanced to support several Fsyn types */

int clk_fsyn_get_params(int input, int output, int *md, int *pe, int *sdiv);

/* ========================================================================
   Name:	clk_err_string
   Description: Convert LLA error code to string.
   Returns:  const char *ErrMessage
   ======================================================================== */

const char *clk_err_string(int err);

#endif /* #ifndef __CLKLLA_COMMON_H */
