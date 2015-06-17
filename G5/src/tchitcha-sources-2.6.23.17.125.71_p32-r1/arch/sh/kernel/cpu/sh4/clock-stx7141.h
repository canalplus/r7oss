/************************************************************************
 * CLOCK driver
 * Low Level API - 7141 specific implementation
 * (C) F. Charpentier, 2008-09
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
************************************************************************/

/* Clocks identifier list */
typedef enum
{
    /* Top level clocks */
    CLK_SATA_OSC,	    /* SATA OSC */
    CLK_SYSALT,	      /* SYSCLKINALT Optional alternate input clock */

    /* Clockgen A */
    CLKA_REF,	   /* Clockgen A reference clock */
    CLKA_OSC,
    CLKA_PLL0HS,	/* PLL0 HS output */
    CLKA_PLL0LS,	/* PLL0 LS output */
    CLKA_PLL1,

    CLKA_IC_STNOC,
    CLKA_FDMA0,
    CLKA_FDMA1,
    CLKA_FDMA2,
    CLKA_SH4_ICK,
    CLKA_SH4_ICK_498,
    CLKA_LX_DMU_CPU,
    CLKA_LX_AUD_CPU,
    CLKA_IC_BDISP_200,
    CLKA_IC_DISP_200,
    CLKA_IC_IF_100,
    CLKA_DISP_PIPE_200,
    CLKA_BLIT_PROC,
    CLKA_ETH_PHY,
    CLKA_PCI,
    CLKA_EMI_MASTER,
    CLKA_IC_COMPO_200,
    CLKA_IC_IF_200,

    /* Clockgen B */
    CLKB_REF,	   /* Clockgen B reference clock */
    CLKB_FS0_CH1,
    CLKB_FS0_CH2,
    CLKB_FS0_CH3,
    CLKB_FS0_CH4,
    CLKB_FS1_CH1,
    CLKB_FS1_CH2,
    CLKB_FS1_CH3,
    CLKB_FS1_CH4,

    CLKB_TMDS_HDMI,

    CLKB_PIX_HD,
    CLKB_DISP_HD,
    CLKB_656,
    CLKB_GDP3,
    CLKB_DISP_ID,
    CLKB_PIX_SD,

    CLKB_PP,
    CLKB_150,
    CLKB_LPC,
    CLKB_DSS,
    CLKB_PIP,	   /* NOT in clockgenB.
		    * Sourced from clk_disp_sd or clk_disp_hd
		    */

    /* Clockgen C (Audio) */
    CLKC_REF,
    CLKC_FS0_CH1,
    CLKC_FS0_CH2,
    CLKC_FS0_CH3,
    CLKC_FS0_CH4,

    /* Clockgen D */
    CLKD_REF,	   /* Clockgen D reference clock */
    CLKD_LMI2X,

    /* Clockgen E = USB PHY */
    CLKE_REF,	   /* Clockgen E reference clock */
/*
 * Not used in Linux
 *   CLK_LAST	    * Keep this as last clock ID. Used to get number of clocks
 */
} clk_id_t;
