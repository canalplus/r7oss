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

#define         RCLKFORM_CFG                0x0000
#define         PHY_CLK_SEL                 0x0004
#define         SELECT_CLK_FROM_DDS         0x0008

  /* Output Clock Controls*/
#define         DSTCLK_POWER_DOWN           0x000C
#define         BYPASS_CLK_EN               0x0010
  /*Input Clock Controls*/
#define         DDS_FORCE_FREE_RUN          0x0014

#define         FC_PD_CTRL                  0x0100
#define         DDS_TST_CTRL                0x0104

#define         AVDDS1_CONTROL0             0x0174
#define         AVDDS1_CONTROL1             0x0178
#define         AVDDS1_INIT_FREQ            0x017C
#define         AVDDS1_INIT                 0x0180
#define         AVDDS1_MUL                  0x0184
#define         AVDDS1_DIV                  0x0188
#define         AVDDS1_ESM_CTRL             0x018C
#define         AVDDS1_FREQ_DELTA           0x0190
#define         AVDDS1_TRACK_ERR            0x0194
#define         AVDDS1_CUR_FREQ             0x0198
#define         AVDDS1_CUR_TRACK_ERR        0x019C

#define         AVDDS2_CONTROL0             0x01A0
#define         AVDDS2_CONTROL1             0x01A4
#define         AVDDS2_INIT_FREQ            0x01A8
#define         AVDDS2_INIT                 0x01AC
#define         AVDDS2_MUL                  0x01B0
#define         AVDDS2_DIV                  0x01B4
#define         AVDDS2_ESM_CTRL             0x01B8
#define         AVDDS2_FREQ_DELTA           0x01BC
#define         AVDDS2_TRACK_ERR            0x01C0
#define         AVDDS2_CUR_FREQ             0x01C4
#define         AVDDS2_CUR_TRACK_ERR        0x01C8

#define         AVDDS3_CONTROL0             0x01CC
#define         AVDDS3_CONTROL1             0x01D0
#define         AVDDS3_INIT_FREQ            0x01D4
#define         AVDDS3_INIT                 0x01D8
#define         AVDDS3_MUL                  0x01DC
#define         AVDDS3_DIV                  0x01E0
#define         AVDDS3_ESM_CTRL             0x01E4
#define         AVDDS3_FREQ_DELTA           0x01E8
#define         AVDDS3_TRACK_ERR            0x01EC
#define         AVDDS3_CUR_FREQ             0x01F0
#define         AVDDS3_CUR_TRACK_ERR        0x01F4

#define         AVDDS4_CONTROL0             0x01F8
#define         AVDDS4_CONTROL1             0x01FC
#define         AVDDS4_INIT_FREQ            0x0200
#define         AVDDS4_INIT                 0x0204
#define         AVDDS4_MUL                  0x0208
#define         AVDDS4_DIV                  0x020C
#define         AVDDS4_ESM_CTRL             0x0210
#define         AVDDS4_FREQ_DELTA           0x0214
#define         AVDDS4_TRACK_ERR            0x0218
#define         AVDDS4_CUR_FREQ             0x021C
#define         AVDDS4_CUR_TRACK_ERR        0x0220

  /*RCLKFORM_CFG*/
#define    RCLKFORM_CLKIN2V5_SEL            BIT0
#define    RCLKFORM_INPUT_FREQ_SELECT       BIT1
#define    RCLKFORM_ASYNCHRONOUS_RESET      BIT4
#define    RCLKFORM_ANALOG_POWER_DOWN       BIT5

  /* BIT 1 =0( 30 MHz) or 1 ( NDIV select the input frequency)*/

  /*PHY_CLK_SEL*/
#define     HDMI_PIX_DVI_CLK_SEL            BIT3	/* 1-extracted from link clock, 0- AVDDS1 */
#define     HDMI_AUD_DVI_CLK_SEL            BIT4	/* 1- extracted from link clock 0- AVDDS2 */
#define     HDMI2_PIX_DVI_CLK_SEL           BIT5	/* 1-extracted from link clock, 0- AVDDS3 */
#define     HDMI2_AUD_DVI_CLK_SEL           BIT6	/* 1- extracted from link clock 0- AVDDS4 */

  /*SELECT_CLK_FROM_DDS*/
#define     SEL_HDMI_PIX_AVDDS1             BIT3	/* 1- clock from DDS, 0 - Default TCLK */
#define     SEL_HDMI_AUD_AVDDS2             BIT4	/* 1- clock from DDS, 0 - Default TCLK */
#define     SEL_HDMI2_PIX_AVDDS1            BIT6	/* 1- clock from DDS, 0 - Default TCLK */
#define     SEL_HDMI2_AUD_AVDDS2            BIT7	/* 1- clock from DDS, 0 - Default TCLK */

  /*DSTCLK_POWER_DOWN*/
#define     POWER_DOWN_HDMI_PIX_CLK_N           BIT5	/* 1- Enables, 0- gated */
#define     POWER_DOWN_HDMI_AUD_CLK_N           BIT6	/* 1- Enables, 0- gated */
#define     POWER_DOWN_IFM_CLK_N                BIT8
#define     POWER_DOWN_TCLK_N                   BIT9
#define     POWER_DOWN_HDMI2_PIX_CLK_N          BIT10	/* 1- Enables, 0- gated */
#define     POWER_DOWN_HDMI2_AUD_CLK_N          BIT11	/* 1- Enables, 0- gated */

  /*BYPASS_CLK_EN*/
#define     BYPASS_EXT_HDMI_PIX_CLK_EN          BIT5	/* 1- Enables external clock, 0- DDS */
#define     BYPASS_EXT_HDMI_AUD_CLK_EN          BIT6	/* 1- Enables external clock, 0- DDS */
#define     BYPASS_EXT_IFM_CLK_EN               BIT8	/* 1- Enables external clock, 0- DDS */
#define     BYPASS_EXT_HDMI2_PIX_CLK_EN         BIT9	/* 1- Enables external clock, 0- DDS */
#define     BYPASS_EXT_HDMI2_AUD_CLK_EN         BIT10	/* 1- Enables external clock, 0- DDS */

  /*DDS_FORCE_FREE_RUN*/
#define     FREE_RUN_HDMI_PIX_AVDDS1_N      BIT3	/* 1- force to open loop mode. */
#define     FREE_RUN_HDMI_AUD_AVDDS2_N      BIT4	/* 1- force to open loop mode. */

  /*FC_PD_CTRL*/
#define     FC_PD_CTRL_AVDDS_EN_SHIT        19
#define     FC_PD_CTRL_AVDDS1_EN_MSK	    BIT19
#define     FC_PD_CTRL_AVDDS2_EN_MSK	    BIT20
#define     FC_PD_CTRL_AVDDS3_EN_MSK	    BIT21
#define     FC_PD_CTRL_AVDDS4_EN_MSK	    BIT22

  /*AVDDS2_INIT*/
#define     AVDDS1_INIT_TRIGGER             BIT0
#define     AVDDS1_MULDIV_WR                BIT4

  /*AVDDS1_CONTROL0*/
#define     AVDDS1_FORCE_OPLOOP             BIT0
#define     AVDDS1_K_MAIN_MASK              0x000E
#define     AVDDS1_K_MAIN_SHIFT             1
#define     AVDDS1_K_DIFF_MASK              0x0070
#define     AVDDS1_K_DIFF_SHIFT             4

  /*AVDDS1_CONTROL1*/
#define     AVDDS1_MULDIV_SEL               BIT0
#define     AVDDS1_CLKMUL_MASK              0x000E
#define     AVDDS1_CLKMUL_SHIFT             1
#define     AVDDS1_COR_K_P_MASk             0x00f0
#define     AVDDS1_COR_K_P_SHIFT            4
#define     AVDDS1_COR_K_I_MASK             0x0f00
#define     AVDDS1_COR_K_I_SHIFT            8

  /*AVDDS2_INIT*/
#define     AVDDS2_INIT_TRIGGER             BIT0
#define     AVDDS2_MULDIV_WR                BIT4

  /*AVDDS2_CONTROL0*/
#define     AVDDS2_FORCE_OPLOOP             BIT0
#define     AVDDS2_K_MAIN_MASK              0x000E
#define     AVDDS2_K_MAIN_SHIFT             1
#define     AVDDS2_K_DIFF_MASK              0x0070
#define     AVDDS2_K_DIFF_SHIFT             4

  /*AVDDS2_CONTROL1*/
#define     AVDDS2_MULDIV_SEL               BIT0
#define     AVDDS2_CLKMUL_MASK              0x000E
#define     AVDDS2_CLKMUL_SHIFT             1
#define     AVDDS2_COR_K_P_MASk             0x00f0
#define     AVDDS2_COR_K_P_SHIFT            4
#define     AVDDS2_COR_K_I_MASK             0x0f00
#define     AVDDS2_COR_K_I_SHIFT            8

  /*AVDDS2_INIT*/
#define     AVDDS2_INIT_TRIGGER             BIT0
#define     AVDDS2_MULDIV_WR                BIT4

  /*AVDDS3_INIT*/
#define     AVDDS3_INIT_TRIGGER             BIT0
#define     AVDDS3_MULDIV_WR                BIT4

  /*AVDDS1_CONTROL0*/
#define     AVDDS3_FORCE_OPLOOP             BIT0
#define     AVDDS3_K_MAIN_MASK              0x000E
#define     AVDDS3_K_MAIN_SHIFT             1
#define     AVDDS3_K_DIFF_MASK              0x0070
#define     AVDDS3_K_DIFF_SHIFT             4

  /*AVDDS1_CONTROL1*/
#define     AVDDS3_MULDIV_SEL               BIT0
#define     AVDDS3_CLKMUL_MASK              0x000E
#define     AVDDS3_CLKMUL_SHIFT             1
#define     AVDDS3_COR_K_P_MASk             0x00f0
#define     AVDDS3_COR_K_P_SHIFT            4
#define     AVDDS3_COR_K_I_MASK             0x0f00
#define     AVDDS3_COR_K_I_SHIFT            8

  /*AVDDS3_INIT*/
#define     AVDDS4_INIT_TRIGGER             BIT0
#define     AVDDS3_MULDIV_WR                BIT4

  /*AVDDS1_CONTROL0*/
#define     AVDDS4_FORCE_OPLOOP             BIT0
#define     AVDDS4_K_MAIN_MASK              0x000E
#define     AVDDS4_K_MAIN_SHIFT             1
#define     AVDDS4_K_DIFF_MASK              0x0070
#define     AVDDS4_K_DIFF_SHIFT             4

  /*AVDDS1_CONTROL1*/
#define     AVDDS4_MULDIV_SEL               BIT0
#define     AVDDS4_CLKMUL_MASK              0x000E
#define     AVDDS4_CLKMUL_SHIFT             1
#define     AVDDS4_COR_K_P_MASk             0x00f0
#define     AVDDS4_COR_K_P_SHIFT            4
#define     AVDDS4_COR_K_I_MASK             0x0f00
#define     AVDDS4_COR_K_I_SHIFT            8

#ifdef __cplusplus
}
#endif
#endif				/*end of __HDMIRX_CLOCKGENREG_H__ */
