/*******************************************************************************
 *
 * File name   : clock-stx5197.h
 * Description : Low Level API - 5197 clocks identifiers
 *
 * COPYRIGHT (C) 2009 STMicroelectronics - All Rights Reserved
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 ******************************************************************************/


/* Clocks identifier list */
typedef enum
{
    OSC_REF,	/* PLLs reference clock */

    PLLA,	   /* PLLA */
    PLLB,	   /* PLLB */

    PLL_CPU,
    PLL_LMI,
    PLL_BIT,
    PLL_SYS,
    PLL_FDMA,
    PLL_DDR,
    PLL_AV,
    PLL_SPARE,
    PLL_ETH,
    PLL_ST40_ICK,
    PLL_ST40_PCK,

    /* FSs clocks */
    FSA_SPARE,
    FSA_PCM,
    FSA_SPDIF,
    FSA_DSS,
    FSB_PIX,
    FSB_FDMA_FS,
    FSB_AUX,
    FSB_USB,
} clk_id_t;
