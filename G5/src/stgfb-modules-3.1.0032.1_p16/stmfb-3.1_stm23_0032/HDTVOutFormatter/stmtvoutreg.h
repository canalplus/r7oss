/***********************************************************************
 *
 * File: HDTVOutFormatter/stmtvoutreg.h
 * Copyright (c) 2008-2009 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef _STM_TVOUTREG_H
#define _STM_TVOUTREG_H

/* TVOut Registers ---------------------------------------------------*/
#define TVOUT_PADS_CTL          0x000
#define TVOUT_CLK_SEL           0x004
#define TVOUT_SYNC_SEL          0x008

// For SD outputs only, teletext control and DAC delay for SCART
#define TVOUT_INT_ENABLE        0x018
#define TVOUT_INT_STATUS        0x01C
#define TVOUT_INT_CLR           0x020
#define TVOUT_TTXT_CTRL         0x030
#define TVOUT_TTXT_FIFO_DATA    0x034
#define TVOUT_TTXT_FIFO0_STATUS 0x038
#define TVOUT_TTXT_FIFO1_STATUS 0x03C
#define TVOUT_SD_DEL            0x040
#define TVOUT_GP_OUT            0x044

// Main TVOut specific config registers definitions
#define TVOUT_MAIN_PADS_HSYNC_MUX_SHIFT          (2)
#define TVOUT_MAIN_PADS_HSYNC_MUX_MASK           (0x7L<<TVOUT_PADS_HSYNC_MUX_SHIFT)
#define TVOUT_MAIN_PADS_HSYNC_REF               (0)
#define TVOUT_MAIN_PADS_HSYNC_SYNC1_INTERLACED  (1L<<TVOUT_MAIN_PADS_HSYNC_MUX_SHIFT)
#define TVOUT_MAIN_PADS_HSYNC_SYNC1_PROGRESSIVE (2L<<TVOUT_MAIN_PADS_HSYNC_MUX_SHIFT)
#define TVOUT_MAIN_PADS_HSYNC_SYNC2             (3L<<TVOUT_MAIN_PADS_HSYNC_MUX_SHIFT)
#define TVOUT_MAIN_PADS_HSYNC_SYNC3             (4L<<TVOUT_MAIN_PADS_HSYNC_MUX_SHIFT)

#define TVOUT_MAIN_PADS_DAC_POFF                 (1L<<11)
#define TVOUT_MAIN_PADS_AUX_N_MAIN_VTG           (1L<<15)

// Aux TVOut specific config registers definitions
#define TVOUT_AUX_PADS_HSYNC_MUX_SHIFT (3)
#define TVOUT_AUX_PADS_HSYNC_MUX_MASK  (0x3L<<TVOUT_AUX_PADS_HSYNC_MUX_SHIFT)
#define TVOUT_AUX_PADS_HSYNC_REF      (0)
#define TVOUT_AUX_PADS_HSYNC_SYNC1    (1L<<TVOUT_AUX_PADS_HSYNC_MUX_SHIFT)
#define TVOUT_AUX_PADS_HSYNC_SYNC2    (2L<<TVOUT_AUX_PADS_HSYNC_MUX_SHIFT)
#define TVOUT_AUX_PADS_HSYNC_SYNC3    (3L<<TVOUT_AUX_PADS_HSYNC_MUX_SHIFT)

#define TVOUT_AUX_PADS_DAC_POFF (1L<<9)
#define TVOUT_AUX_PADS_SD_DAC_456_N_123 (1L<<11)
#define TVOUT_AUX_PADS_HD_DAC_456_N_123 (1L<<13)

// Common pad control definitions
#define TVOUT_PADS_VSYNC_MUX_SHIFT (5)
#define TVOUT_PADS_VSYNC_MUX_MASK  (0x7L<<TVOUT_PADS_VSYNC_MUX_SHIFT)
#define TVOUT_PADS_VSYNC_REF       (0)
#define TVOUT_PADS_VSYNC_SYNC1     (1L<<TVOUT_PADS_VSYNC_MUX_SHIFT)
#define TVOUT_PADS_VSYNC_SYNC2     (2L<<TVOUT_PADS_VSYNC_MUX_SHIFT)
#define TVOUT_PADS_VSYNC_SYNC3     (3L<<TVOUT_PADS_VSYNC_MUX_SHIFT)
#define TVOUT_PADS_VSYNC_BNOTTREF  (4L<<TVOUT_PADS_VSYNC_MUX_SHIFT)
#define TVOUT_PADS_VSYNC_BNOTT1    (5L<<TVOUT_PADS_VSYNC_MUX_SHIFT)
#define TVOUT_PADS_VSYNC_BNOTT2    (6L<<TVOUT_PADS_VSYNC_MUX_SHIFT)
#define TVOUT_PADS_VSYNC_BNOTT3    (7L<<TVOUT_PADS_VSYNC_MUX_SHIFT)

// Clock divide definitions
#define TVOUT_CLK_BYPASS           (0L)
#define TVOUT_CLK_DIV_2            (1L)
#define TVOUT_CLK_DIV_4            (2L)
#define TVOUT_CLK_DIV_8            (3L)
#define TVOUT_CLK_MASK             (3L)

#define TVOUT_MAIN_CLK_PIX_SHIFT   (0)
#define TVOUT_MAIN_CLK_SEL_PIX(x)  ((x)<<TVOUT_MAIN_CLK_PIX_SHIFT)
#define TVOUT_MAIN_CLK_TMDS_SHIFT  (2)
#define TVOUT_MAIN_CLK_SEL_TMDS(x) ((x)<<TVOUT_MAIN_CLK_TMDS_SHIFT)
#define TVOUT_MAIN_CLK_FDVO_SHIFT  (6)
#define TVOUT_MAIN_CLK_SEL_FDVO(x) ((x)<<TVOUT_MAIN_CLK_FDVO_SHIFT)

#define TVOUT_AUX_CLK_SEL_PIX1X(x) ((x)<<0)
#define TVOUT_AUX_CLK_SEL_DENC(x)  ((x)<<2)
#define TVOUT_AUX_CLK_SEL_SD_N_HD  (1L<<4)

// Sync set selection
#define TVOUT_MAIN_SYNC_FDVO_SHIFT                   (0)
#define TVOUT_MAIN_SYNC_FDVO_REF                     (0L<<TVOUT_MAIN_SYNC_FDVO_SHIFT)
#define TVOUT_MAIN_SYNC_FDVO_SYNC1_INTERLACED        (1L<<TVOUT_MAIN_SYNC_FDVO_SHIFT)
#define TVOUT_MAIN_SYNC_FDVO_SYNC1_PROGRESSIVE       (5L<<TVOUT_MAIN_SYNC_FDVO_SHIFT)
#define TVOUT_MAIN_SYNC_FDVO_SYNC2                   (2L<<TVOUT_MAIN_SYNC_FDVO_SHIFT)
#define TVOUT_MAIN_SYNC_FDVO_SYNC3                   (3L<<TVOUT_MAIN_SYNC_FDVO_SHIFT)
#define TVOUT_MAIN_SYNC_FDVO_MASK                    (7L<<TVOUT_MAIN_SYNC_FDVO_SHIFT)
#define TVOUT_MAIN_SYNC_HDMI_SHIFT                   (3)
#define TVOUT_MAIN_SYNC_HDMI_REF                     (0L<<TVOUT_MAIN_SYNC_HDMI_SHIFT)
#define TVOUT_MAIN_SYNC_HDMI_SYNC1_INTERLACED        (2L<<TVOUT_MAIN_SYNC_HDMI_SHIFT)
#define TVOUT_MAIN_SYNC_HDMI_SYNC1_PROGRESSIVE       (3L<<TVOUT_MAIN_SYNC_HDMI_SHIFT)
#define TVOUT_MAIN_SYNC_HDMI_SYNC2                   (4L<<TVOUT_MAIN_SYNC_HDMI_SHIFT)
#define TVOUT_MAIN_SYNC_HDMI_SYNC3                   (6L<<TVOUT_MAIN_SYNC_HDMI_SHIFT)
#define TVOUT_MAIN_SYNC_HDMI_MASK                    (7L<<TVOUT_MAIN_SYNC_HDMI_SHIFT)
#define TVOUT_MAIN_SYNC_DCSED_SHIFT                  (6)
#define TVOUT_MAIN_SYNC_DCSED_VTG1_REF               (0L<<TVOUT_MAIN_SYNC_DCSED_SHIFT)
#define TVOUT_MAIN_SYNC_DCSED_VTG1_SYNC1_INTERLACED  (1L<<TVOUT_MAIN_SYNC_DCSED_SHIFT)
#define TVOUT_MAIN_SYNC_DCSED_VTG1_SYNC1_PROGRESSIVE (9L<<TVOUT_MAIN_SYNC_DCSED_SHIFT)
#define TVOUT_MAIN_SYNC_DCSED_VTG1_SYNC2             (2L<<TVOUT_MAIN_SYNC_DCSED_SHIFT)
#define TVOUT_MAIN_SYNC_DCSED_VTG1_SYNC3             (3L<<TVOUT_MAIN_SYNC_DCSED_SHIFT)
#define TVOUT_MAIN_SYNC_DCSED_VTG2_REF               (4L<<TVOUT_MAIN_SYNC_DCSED_SHIFT)
#define TVOUT_MAIN_SYNC_DCSED_VTG2_SYNC1             (5L<<TVOUT_MAIN_SYNC_DCSED_SHIFT)
#define TVOUT_MAIN_SYNC_DCSED_VTG2_SYNC2             (6L<<TVOUT_MAIN_SYNC_DCSED_SHIFT)
#define TVOUT_MAIN_SYNC_DCSED_VTG2_SYNC3             (7L<<TVOUT_MAIN_SYNC_DCSED_SHIFT)
#define TVOUT_MAIN_SYNC_DCSED_MASK                   (0xF<<TVOUT_MAIN_SYNC_DCSED_SHIFT)

#define TVOUT_AUX_SYNC_DENC_SHIFT                 (0)
#define TVOUT_AUX_SYNC_DENC_REF                   (0L<<TVOUT_AUX_SYNC_DENC_SHIFT)
#define TVOUT_AUX_SYNC_DENC_SYNC1                 (1L<<TVOUT_AUX_SYNC_DENC_SHIFT)
#define TVOUT_AUX_SYNC_DENC_SYNC2                 (2L<<TVOUT_AUX_SYNC_DENC_SHIFT)
#define TVOUT_AUX_SYNC_DENC_SYNC3                 (3L<<TVOUT_AUX_SYNC_DENC_SHIFT)
#define TVOUT_AUX_SYNC_DENC_PADS                  (7L<<TVOUT_AUX_SYNC_DENC_SHIFT)
#define TVOUT_AUX_SYNC_DENC_MASK                  (7L<<TVOUT_AUX_SYNC_DENC_SHIFT)
#define TVOUT_AUX_SYNC_AWG_SHIFT                  (3)
#define TVOUT_AUX_SYNC_AWG_REF                    (0L<<TVOUT_AUX_SYNC_AWG_SHIFT)
#define TVOUT_AUX_SYNC_AWG_SYNC1                  (1L<<TVOUT_AUX_SYNC_AWG_SHIFT)
#define TVOUT_AUX_SYNC_AWG_SYNC2                  (2L<<TVOUT_AUX_SYNC_AWG_SHIFT)
#define TVOUT_AUX_SYNC_AWG_SYNC3                  (3L<<TVOUT_AUX_SYNC_AWG_SHIFT)
#define TVOUT_AUX_SYNC_AWG_MASK                   (3L<<TVOUT_AUX_SYNC_AWG_SHIFT)
#define TVOUT_AUX_SYNC_VTG_SHIFT                  (5)
#define TVOUT_AUX_SYNC_VTG_SLAVE_REF              (0L<<TVOUT_AUX_SYNC_VTG_SHIFT)
#define TVOUT_AUX_SYNC_VTG_SLAVE_SYNC1            (1L<<TVOUT_AUX_SYNC_VTG_SHIFT)
#define TVOUT_AUX_SYNC_VTG_SLAVE_SYNC2            (2L<<TVOUT_AUX_SYNC_VTG_SHIFT)
#define TVOUT_AUX_SYNC_VTG_SLAVE_SYNC3            (3L<<TVOUT_AUX_SYNC_VTG_SHIFT)
#define TVOUT_AUX_SYNC_VTG_PADS                   (7L<<TVOUT_AUX_SYNC_VTG_SHIFT)
#define TVOUT_AUX_SYNC_VTG_MASK                   (7L<<TVOUT_AUX_SYNC_VTG_SHIFT)
#define TVOUT_AUX_SYNC_HDF_SHIFT                  (8)
#define TVOUT_AUX_SYNC_HDF_VTG1_REF               (0L<<TVOUT_AUX_SYNC_HDF_SHIFT)
#define TVOUT_AUX_SYNC_HDF_VTG1_SYNC1_INTERLACED  (1L<<TVOUT_AUX_SYNC_HDF_SHIFT)
#define TVOUT_AUX_SYNC_HDF_VTG1_SYNC1_PROGRESSIVE (9L<<TVOUT_AUX_SYNC_HDF_SHIFT)
#define TVOUT_AUX_SYNC_HDF_VTG1_SYNC2             (2L<<TVOUT_AUX_SYNC_HDF_SHIFT)
#define TVOUT_AUX_SYNC_HDF_VTG1_SYNC3             (3L<<TVOUT_AUX_SYNC_HDF_SHIFT)
#define TVOUT_AUX_SYNC_HDF_VTG2_REF               (4L<<TVOUT_AUX_SYNC_HDF_SHIFT)
#define TVOUT_AUX_SYNC_HDF_VTG2_SYNC1             (5L<<TVOUT_AUX_SYNC_HDF_SHIFT)
#define TVOUT_AUX_SYNC_HDF_VTG2_SYNC2             (6L<<TVOUT_AUX_SYNC_HDF_SHIFT)
#define TVOUT_AUX_SYNC_HDF_VTG2_SYNC3             (7L<<TVOUT_AUX_SYNC_HDF_SHIFT)
#define TVOUT_AUX_SYNC_HDF_MASK                   (0xF<<TVOUT_AUX_SYNC_HDF_SHIFT)


/* HD formatter registers --------------------------------------------*/
#define TVFMT_ANA_CFG              0x000
#define TVFMT_ANA_YC_DELAY         0x004
#define TVFMT_ANA_Y_SCALE          0x008
#define TVFMT_ANA_C_SCALE          0x00C // Just CB in some chips
#define TVFMT_LUMA_SRC_CFG         0x014
#define TVFMT_LUMA_COEFF_P1_T123   0x018
#define TVFMT_LUMA_COEFF_P1_T456   0x01C
#define TVFMT_LUMA_COEFF_P2_T123   0x020
#define TVFMT_LUMA_COEFF_P2_T456   0x024
#define TVFMT_LUMA_COEFF_P3_T123   0x028
#define TVFMT_LUMA_COEFF_P3_T456   0x02C
#define TVFMT_LUMA_COEFF_P4_T123   0x030
#define TVFMT_LUMA_COEFF_P4_T456   0x034
#define TVFMT_ANA_CR_SCALE         0x038
#define TVFMT_CHROMA_SRC_CFG       0x040
#define TVFMT_CHROMA_COEFF_P1_T123 0x044
#define TVFMT_CHROMA_COEFF_P1_T456 0x048
#define TVFMT_CHROMA_COEFF_P2_T123 0x04C
#define TVFMT_CHROMA_COEFF_P2_T456 0x050
#define TVFMT_CHROMA_COEFF_P3_T123 0x054
#define TVFMT_CHROMA_COEFF_P3_T456 0x058
#define TVFMT_CHROMA_COEFF_P4_T123 0x05C
#define TVFMT_CHROMA_COEFF_P4_T456 0x060

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

#define ANA_SRC_CFG_DIV_SHIFT    2
#define ANA_SRC_CFG_DIV_256      (0L<<ANA_SRC_CFG_DIV_SHIFT)
#define ANA_SRC_CFG_DIV_512      (1L<<ANA_SRC_CFG_DIV_SHIFT)
#define ANA_SRC_CFG_DIV_1024     (2L<<ANA_SRC_CFG_DIV_SHIFT)
#define ANA_SRC_CFG_DIV_2048     (3L<<ANA_SRC_CFG_DIV_SHIFT)
#define ANA_SRC_CFG_DIV_MASK     (3L<<ANA_SRC_CFG_DIV_SHIFT)

#define ANA_SRC_CFG_BYPASS       (1L<<4)

#define DIG_CFG_CLIP_SAV_EAV     (1L<<10)

#endif // _STM_TVOUTREG_H
