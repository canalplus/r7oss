/***********************************************************************
 *
 * File: soc/sti7200/sti7200cut1tvoutreg.h
 * Copyright (c) 2008 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef _STI7200CUT1TVOUTREG_H
#define _STI7200CUT1TVOUTREG_H


/* STi7200 TVOut Registers ---------------------------------------------------*/
#define TVOUT_PADS_CTL          0x000
#define TVOUT_CLK_SEL           0x004
#define TVOUT_SYNC_SEL          0x008
#define TVOUT_SYNC_ACCESS       0x00C
#define TVOUT_PORT_CTL          0x010
#define TVOUT_PORT_STATUS       0x014

// For SD outputs only, teletext control
#define TVOUT_INT_ENABLE        0x018
#define TVOUT_INT_STATUS        0x01C
#define TVOUT_INT_CLR           0x020
#define TVOUT_TTXT_CTRL         0x030
#define TVOUT_TTXT_FIFO_DATA    0x034
#define TVOUT_TTXT_FIFO0_STATUS 0x038
#define TVOUT_TTXT_FIFO1_STATUS 0x03C

// Common pad control definitions
#define TVOUT_PADS_HSYNC_MUX_SHIFT (3)
#define TVOUT_PADS_HSYNC_MUX_MASK  (0x3L<<TVOUT_PADS_HSYNC_MUX_SHIFT)
#define TVOUT_PADS_HSYNC_HREF      (0)
#define TVOUT_PADS_HSYNC_HSYNC1    (1L<<TVOUT_PADS_HSYNC_MUX_SHIFT)
#define TVOUT_PADS_HSYNC_HSYNC2    (2L<<TVOUT_PADS_HSYNC_MUX_SHIFT)
#define TVOUT_PADS_HSYNC_HSYNC3    (3L<<TVOUT_PADS_HSYNC_MUX_SHIFT)

#define TVOUT_PADS_VSYNC_MUX_SHIFT (5)
#define TVOUT_PADS_VSYNC_MUX_MASK  (0x7L<<TVOUT_PADS_VSYNC_MUX_SHIFT)
#define TVOUT_PADS_VSYNC_VREF      (0)
#define TVOUT_PADS_VSYNC_VSYNC1    (1L<<TVOUT_PADS_VSYNC_MUX_SHIFT)
#define TVOUT_PADS_VSYNC_VSYNC2    (2L<<TVOUT_PADS_VSYNC_MUX_SHIFT)
#define TVOUT_PADS_VSYNC_VSYNC3    (3L<<TVOUT_PADS_VSYNC_MUX_SHIFT)
#define TVOUT_PADS_VSYNC_BNOTTREF  (4L<<TVOUT_PADS_VSYNC_MUX_SHIFT)
#define TVOUT_PADS_VSYNC_BNOTT1    (5L<<TVOUT_PADS_VSYNC_MUX_SHIFT)
#define TVOUT_PADS_VSYNC_BNOTT2    (6L<<TVOUT_PADS_VSYNC_MUX_SHIFT)
#define TVOUT_PADS_VSYNC_BNOTT3    (7L<<TVOUT_PADS_VSYNC_MUX_SHIFT)

// Common clock divide definitions
#define TVOUT_CLK_BYPASS           (0L)
#define TVOUT_CLK_DIV_2            (1L)
#define TVOUT_CLK_DIV_4            (2L)
#define TVOUT_CLK_DIV_8            (3L)
#define TVOUT_CLK_MASK             (3L)

// Common sync select definitions
#define TVOUT_SYNC_SEL_REF         (0L)
#define TVOUT_SYNC_SEL_SYNC1       (1L)
#define TVOUT_SYNC_SEL_SYNC2       (2L)
#define TVOUT_SYNC_SEL_SYNC3       (3L)

#define TVOUT_INT_TTXT_UNDERFLOW (1L<<0)

/* STi7200 Cut1 display clock glue -------------------------------------------*/ 

// Glue definitions for AUX TVOUT PADS_CTL, but used for all pipelines
#define TVOUT_AUX_PADS_FVP_DIV_SHIFT_BIT1  (15)
#define TVOUT_AUX_PADS_LOCAL_AUX_DIV_SHIFT (16)
#define TVOUT_AUX_PADS_REMOTE_DIV_SHIFT    (18)
#define TVOUT_AUX_PADS_TMDS_DIV_SHIFT      (20)
#define TVOUT_AUX_PADS_CH34_DIV_SHIFT      (22)
// Related glue definitions in one of the HDMI registers
#define HDMI_GPOUT_REMOTE_PIX_HD           (1L<<8)
#define HDMI_GPOUT_LOCAL_AUX_PIX_HD        (1L<<9)
#define HDMI_GPOUT_LOCAL_GDP3_SD           (1L<<10)
#define HDMI_GPOUT_LOCAL_VDP_SD            (1L<<11)
#define HDMI_GPOUT_LOCAL_CAP_SD            (1L<<12)
#define HDMI_GPOUT_LOCAL_SDTVO_HD          (1L<<13)
#define HDMI_GPOUT_REMOTE_SDTVO_HD         (1L<<14)
#define HDMI_GPOUT_FVP_DIV_SHIFT_BIT0      (15)

/* STi7200 Local formatter registers -----------------------------------------*/
#define TVFMT_ANA_CFG       0x000
#define TVFMT_ANA_YC_DELAY  0x004
#define TVFMT_ANA_Y_SCALE   0x008
#define TVFMT_ANA_C_SCALE   0x00C
#define TVFMT_ANA_SRC_CFG   0x014
#define TVFMT_COEFF_P1_T123 0x018
#define TVFMT_COEFF_P1_T456 0x01C
#define TVFMT_COEFF_P2_T123 0x020
#define TVFMT_COEFF_P2_T456 0x024
#define TVFMT_COEFF_P3_T123 0x028
#define TVFMT_COEFF_P3_T456 0x02C
#define TVFMT_COEFF_P4_T123 0x030
#define TVFMT_COEFF_P4_T456 0x034

#define TVFMT_DIG1_CFG      0x100
#define TVFMT_DIG1_YC_DELAY 0x104

#define TVFMT_DIG2_CFG      0x180
#define TVFMT_DIG2_YC_DELAY 0x184

/*
 * Note: the first set of definitions are also used for the Digital config
 */ 
#define ANA_CFG_INPUT_YCBCR            (0L)
#define ANA_CFG_INPUT_RGB              (1L)
#define ANA_CFG_INPUT_AUX              (2L)
#define ANA_CFG_INPUT_MASK             (3L)
#define ANA_CFG_RCTRL_G                (0L)
#define ANA_CFG_RCTRL_B                (1L)
#define ANA_CFG_RCTRL_R                (2L)
#define ANA_CFG_RCTRL_MASK             (3L)
#define ANA_CFG_REORDER_RSHIFT         (2L)
#define ANA_CFG_REORDER_GSHIFT         (4L)
#define ANA_CFG_REORDER_BSHIFT         (6L)
#define ANA_CFG_CLIP_EN                (1L<<8)
#define ANA_CFG_CLIP_GB_N_CRCB         (1L<<9)
/*
 * The rest are for analogue only
 */
#define ANA_CFG_PREFILTER_EN           (1L<<10)
#define ANA_CFG_SYNC_ON_PRPB           (1L<<11)
#define ANA_CFG_SEL_MAIN_TO_DENC       (1L<<12)
#define ANA_CFG_PRPB_SYNC_OFFSET_SHIFT (16)
#define ANA_CFG_PRPB_SYNC_OFFSET_MASK  (0x7ff<<ANA_CFG_PRPB_SYNC_OFFSET_SHIFT)
#define ANA_CFG_SYNC_EN                (1L<<27)
#define ANA_CFG_SYNC_HREF_IGNORED      (1L<<28)
#define ANA_CFG_SYNC_FIELD_N_FRAME     (1L<<29)
#define ANA_CFG_SYNC_FILTER_DEL_SD     (0)
#define ANA_CFG_SYNC_FILTER_DEL_ED     (1L<<30)
#define ANA_CFG_SYNC_FILTER_DEL_HD     (2L<<30)
#define ANA_CFG_SYNC_FILTER_DEL_BYPASS (3L<<30)
#define ANA_CFG_SYNC_FILTER_DEL_MASK   (3L<<30)

#define ANA_SCALE_OFFSET_SHIFT         (16)

#define ANA_SRC_CFG_2X           (0L)
#define ANA_SRC_CFG_4X           (1L)
#define ANA_SRC_CFG_8X           (2L)
#define ANA_SRC_CFG_DISABLE      (3L)

#define ANA_SRC_CFG_DIV_256      (0L)
#define ANA_SRC_CFG_DIV_512      (1L<<2)
#define ANA_SRC_CFG_DIV_1024     (2L<<2)
#define ANA_SRC_CFG_DIV_2048     (3L<<2)

#define DIG_CFG_CLIP_SAV_EAV     (1L<<10)

#endif // _STI7200CUT1TVOUTREG_H
