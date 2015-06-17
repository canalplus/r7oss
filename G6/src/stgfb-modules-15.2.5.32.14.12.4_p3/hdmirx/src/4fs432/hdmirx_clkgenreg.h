/***********************************************************************
 *
 * File: hdmirx/src/clkgen/hdmirx_clkgenreg.h
 * Copyright (c) 2012 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef __HDMIRX_CLOCKGENREG_H__
#define __HDMIRX_CLOCKGENREG_H__

/*Includes------------------------------------------------------------------------------*/
#include <stddefs.h>
#include <InternalTypes.h>

#ifdef __cplusplus
extern "C" {
#endif

//DOUBLE U16
#define     HAL_CLKGEN_READ_REG_DWORD                HDMI_READ_REG_DWORD
#define     HAL_CLKGEN_WRITE_REG_DWORD               HDMI_WRITE_REG_DWORD
#define     HAL_CLKGEN_SET_BITS_DWORD                HDMI_SET_REG_BITS_DWORD
#define     HAL_CLKGEN_CLEAR_BITS_DWORD              HDMI_CLEAR_REG_BITS_DWORD
#define     HAL_CLKGEN_ClearAndSet_BITS_DWORD        HDMI_CLEAR_AND_SET_REG_BITS_DWORD

  /****************************************************************************
     R E G I S T E R S   D E F I N I T I O N S
  ****************************************************************************/
/* 4FS432 Registers */
#define         CLOCK_STATUS                0x0A00
#define         CLOCK_IRQ_CTRL              0x0A04
/* Video Clock DDS0 */
#define         VDDS0_CONTROL               0x0A08
#define         VDDS0_INIT                  0x0A0C
#define         VDDS0_INI_FREQ              0x0A10
#define         VDDS0_FREQ_DELTA            0x0A14
#define         VDDS0_MUL                   0x0A18
#define         VDDS0_DIV                   0x0A1C
#define         VDDS0_CORR_THRESHOLD        0x0A20
#define         VDDS0_TRACK_ERR             0x0A24
#define         VDDS0_FREQ                  0x0A28
/* AUDIO Clock DDS0 */
#define         AUDDS0_CONTROL              0x0A50
#define         AUDDS0_INIT                 0x0A54
#define         AUDDS0_INI_FREQ             0x0A58
#define         AUDDS0_FREQ_DELTA           0x0A5C
#define         AUDDS0_MUL                  0x0A60
#define         AUDDS0_DIV                  0x0A64
#define         AUDDS0_CORR_THRESHOLD       0x0A68
#define         AUDDS0_TRACK_ERR            0x0A6C
#define         AUDDS0_FREQ                 0x0A70
/* Test Bus Control */
#define         DPRX_4FS432_TEST_BUS_SEL    0x0A98
/* DDDS Configuration Registers */
#define         PLL_CONTROL                 0x0A9C
#define         THRESHOLD_SETTINGS_VDDS0    0x0AA0
#define         THRESHOLD_SETTINGS_AUDDS0   0x0AA4
#define         DDDS_STATUS                 0x0AA8
#define         FINE_CORRECTION_FIFO_DEPTH  0x0AAC
#define         DDDS_STATUS_CRO             0x0AB0
#define         VDDS_THRESHOLD_MUL_DIFF     0x0ACC
#define         VDDS_THRESHOLD_DIV_DIFF     0x0AD0
#define         AUDDS_THRESHOLD_MUL_DIFF    0x0AD4
#define         AUDDS_THRESHOLD_DIV_DIFF    0x0AD8


  /* CLOCK_STATUS */
#define     VDDS0_STATUS                    BIT0   /* 0 = Normal Closed loop operation.  1 = DDS switched to open loop. */
#define     VDDS1_STATUS                    BIT1   /* 0 = Normal Closed loop operation.  1 = DDS switched to open loop. */
#define     AUDDS0_STATUS                   BIT2   /* 0 = Normal Closed loop operation.  1 = DDS switched to open loop. */
#define     AUDDS1_STATUS                   BIT3   /* 0 = Normal Closed loop operation.  1 = DDS switched to open loop. */

  /* CLOCK_IRQ_CTRL */
#define     VDDS0_IRQ_EN                    BIT0   /* 0 = IRQ Disabled		1 = IRQ Enabled */
#define     VDDS1_IRQ_EN                    BIT1   /* 0 = IRQ Disabled		1 = IRQ Enabled */
#define     AUDDS0_IRQ_EN                   BIT2   /* 0 = IRQ Disabled		1 = IRQ Enabled */
#define     AUDDS1_IRQ_EN                   BIT3   /* 0 = IRQ Disabled		1 = IRQ Enabled */

  /* VDDS0_CONTROL */
#define     VDDS0_FORCE_OPLOOP              BIT0   /* 0 = Video DDS operates in closed loop 1 = Forces Video DDS to open loop */
#define     VDDS0_SP_EN                     BIT10  /* 0 = Spread Spectrum disabled 1 = Spread spectrum Enabled */
#define     VDDS0_BUF_ERR_POL               BIT11  /* 0=Error is used as is; 1=Error polarity is inverted */
#define     VDDS0_FREQ_AVG_EN               BIT28  /* Reserved */

  /* VDDS0_INIT */
#define     VDDS0_INIT_TRIGGER              BIT0
#define     VDDS0_MUL_DIV_WR                BIT4

  /* AUDDS0_CONTROL */
#define     AUDDS0_FORCE_OPLOOP             BIT0   /* 0 = Audio DDS operates in closed loop 1 = Forces Audio DDS to open loop */
#define     AUDDS0_SP_EN                    BIT10  /* 0 = Spread Spectrum disabled 1 = Spread spectrum Enabled */
#define     AUDDS0_BUF_ERR_POL              BIT11  /* 0=Error is used as is; 1=Error polarity is inverted. */
#define     AUDDS0_FREQ_AVG_EN              BIT28  /* Reserved */
#define     AUDDS0_LOAD_MAX_EN              BIT29  /* Reserved */
#define     VID_PLL_SEL4_AUD                BIT30

  /* AUDDS0_INIT */
#define     AUDDS0_INIT_TRIGGER             BIT0
#define     AUDDS0_MUL_DIV_WR               BIT4

  /* AUDDS0_CORR_THRESHOLD */
#define     AUDDS0_CORR_TH_AUTO             BIT7   /* 0 – AUDDS0_CORR_TH[6:0] is used as the correction threshold 1 - Correction threshold value is generated by hardware */

  /* DDS0_SHIFT */
#define     DDS0_CLKMUL_SHIFT               1
#define     DDS0_MUL_SEL_SHIFT              4
#define     DDS0_DIV_SEL_SHIFT              7
#define     DDS0_SP_AMPLITUDE_SHIFT         16
#define     DDS0_SP_PERIOD_SHIFT            19
#define     DDS0_CLK_COR_K_SHIFT            24

  /* DDS0_MASKS */
#define     DDS0_CLKMUL_MSK                 0x0000000E /* VDDS0_CONTROL/AUDDS0_CONTROL */
#define     DDS0_MUL_SEL_MSK                0x00000070 /* VDDS0_CONTROL/AUDDS0_CONTROL */
#define     DDS0_DIV_SEL_MSK                0x00000380 /* VDDS0_CONTROL/AUDDS0_CONTROL */
#define     DDS0_SP_AMPLITUDE_MSK           0x00070000 /* VDDS0_CONTROL/AUDDS0_CONTROL */
#define     DDS0_SP_PERIOD_MSK              0x00F80000 /* VDDS0_CONTROL/AUDDS0_CONTROL */
#define     DDS0_CLK_COR_K_MSK              0x0F000000 /* VDDS0_CONTROL/AUDDS0_CONTROL */
#define     DDS0_INI_FREQ_MSK               0x00FFFFFF /* VDDS0_INI_FREQ/AUDDS0_INI_FREQ_MSK */
#define     DDS0_FREQ_DELTA_MSK             0x000000FF /* VDDS0_FREQ_DELTA/AUDDS0_FREQ_DELTA */
#define     DDS0_MUL_MSK                    0x000FFFFF /* VDDS0_MUL/AUDDS0_MUL */
#define     DDS0_DIV_MSK                    0x000FFFFF /* VDDS0_DIV/AUDDS0_DIV */
#define     DDS0_CORR_TH_MSK                0x0000007F /* VDDS0_CORR_THRESHOLD/AUDDS0_CORR_THRESHOLD */
#define     DDS0_TRACK_ERR_MSK              0x000000FF /* VDDS0_TRACK_ERR/AUDDS0_TRACK_ERR */
#define     DDS0_FREQ_MSK                   0x00FFFFFF /* VDDS0_FREQ/AUDDS0_FREQ */
#define     VDDS0_FIFO_DEPTH                0x0000000F /* total error one can accommodate before applying correction */
#define     AUDDS0_FIFO_DEPTH               0x000000F0 /* total error one can accommodate before applying correction */

  /* PLL_CONTROL */
#define     INPUTFREQSEL                    BIT0      /* "0" the CLKIN2V5 is the PLL input clock. "1" the CLKIN1V0 is the PLL input clock. */
#define     NPDPLL                          BIT1      /* Analog Power down mode, active low */
#define     NRST                            (0x3<<3)  /* Reset control */
#define     NSB                             (0x3<<5)  /* Independent Digital Stand-Bye, active low. */
#define     SELCLKOUT                       (0x3<<7)  /* Select the output clock */
#define     NDIV                            (0x0<<9)
#define     CLOCK_SEL                       BIT13     /* 0 External Clock is Selected as Input 1 VCO clock from PLL is selected as Input  */
#define     EXT_AUDIO_CLOSEDLOOP_CTRL       BIT15     /* When set to 1 will give control of Audio Clock closed loop behavior to HDMI hardware. */
#define     EXT_VIDEO_CLOSEDLOOP_CTRL       BIT16     /* When set to 1 will give control of Video Clock closed loop behavior to HDMI hardware.  */
#define     PLL0_CLOSELOOP_IRQ_EN           BIT17     /* Value of 1 will enable interrupt while 0 will disable the interrupt. */
#define     PLL1_CLOSELOOP_IRQ_EN           BIT18     /* Value of 1 will enable interrupt while 0 will disable the interrupt. */
#define     PLL0_OPENLOOP_IRQ_EN            BIT19     /* Value of 1 will enable interrupt while 0 will disable the interrupt. */
#define     PLL1_OPENLOOP_IRQ_EN            BIT20     /* Value of 1 will enable interrupt while 0 will disable the interrupt. */

  /* DDDS_STATUS */
#define     PLL0_LOCK                       BIT0	/* Lock status for individual PLL */
#define     PLL1_LOCK                       BIT1	/* Lock status for individual PLL */
#define     LOCK_P                          BIT4	/* From FS specifies Stable Clock output */

  /* DDDS_STATUS_CRO */
#define     PLL0_CLOSEDLOOP                 BIT0	/* Closed Loop Lock status for VDDS PLL */
#define     PLL1_CLOSEDLOOP                 BIT1	/* Closed Loop Lock status for AUDDS PLL */
#define     PLL0_OPENLOOP                   BIT2	/* Open Loop Status for VDDS PLL */
#define     PLL1_OPENLOOP                   BIT3	/* Open Loop Status for AUDDS PLL */

#ifdef __cplusplus
}
#endif
#endif				/*end of __HDMIRX_CLOCKGENREG_H__ */
