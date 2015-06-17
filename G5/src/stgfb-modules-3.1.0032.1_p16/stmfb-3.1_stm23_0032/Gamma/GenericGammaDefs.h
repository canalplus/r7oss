/***********************************************************************
 *
 * File: Gamma/GenericGammaDefs.h
 * Copyright (c) 2000, 2004, 2005 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef GENERICGAMMADEFS_H
#define GENERICGAMMADEFS_H

// Generic GDP Register Definitions although not all
// functionality is available on "GDP Lite"
#define GDP_CTL_RGB_565         0L
#define GDP_CTL_RGB_888         0x01L
#define GDP_CTL_ARGB_8565       0x04L
#define GDP_CTL_ARGB_8888       0x05L
#define GDP_CTL_ARGB_1555       0x06L
#define GDP_CTL_ARGB_4444       0x07L
#define GDP_CTL_CLUT8           0x0BL     /* 7109Cut3 */
#define GDP_CTL_ACLUT88         0x0DL     /* 7109Cut3 */
#define GDP_CTL_YCbCr888        0x10L
#define GDP_CTL_YCbCr422R       0x12L
#define GDP_CTL_AYCbCr8888      0x15L
#define GDP_CTL_FORMAT_MASK     0x1FL

#define GDP_CTL_ALPHA_RANGE     (1L<<5)
#define GDP_CTL_COLOUR_FILL     (1L<<8)
#define GDP_CTL_EN_FLICKERFIL   (1L<<9)
#define GDP_CTL_EN_H_RESIZE     (1L<<10)
#define GDP_CTL_EN_V_RESIZE     (1L<<11)
#define GDP_CTL_EN_ALPHA_HBOR   (1L<<12)
#define GDP_CTL_EN_ALPHA_VBOR   (1L<<13)
#define GDP_CTL_EN_COLOR_KEY    (1L<<14)
#define GDP_CTL_BCB_COL_KEY_1   (1L<<16)
#define GDP_CTL_BCB_COL_KEY_3   (3L<<16)
#define GDP_CTL_GY_COL_KEY_1    (1L<<18)
#define GDP_CTL_GY_COL_KEY_3    (3L<<18)
#define GDP_CTL_RCR_COL_KEY_1   (1L<<20)
#define GDP_CTL_RCR_COL_KEY_3   (3L<<20)
#define GDP_CTL_BIGENDIAN       (1L<<23)
#define GDP_CTL_PREMULT_FORMAT  (1L<<24)
#define GDP_CTL_709_SELECT      (1L<<25)
#define GDP_CTL_CHROMA_SIGNED   (1L<<26)
#define GDP_CTL_EN_CLUT_UPDATE  (1L<<27)
#define GDP_CTL_EN_VFILTER_UPD  (1L<<28)
#define GDP_CTL_LSB_STUFF       (1L<<29)
#define GDP_CTL_EN_HFILTER_UPD  (1L<<30)
#define GDP_CTL_WAIT_NEXT_VSYNC (1L<<31)

#define GDP_AGC_GAIN_SHIFT      (16)
#define GDP_AGC_GAIN_MASK       (0xFF<<GDP_AGC_GAIN_SHIFT)
#define GDP_AGC_CONSTANT_SHIFT  (24)
#define GDP_AGC_CONSTANT_MASK   (0xFF<<GDP_AGC_CONSTANT_SHIFT)

#define GDP_HSRC_INITIAL_PHASE_SHIFT (16)
#define GDP_HSRC_FILTER_EN           (1L<<24)
#define GDP_HSRC_HSRC_INC_XTN_SHIFT  (27)

#define GDP_VSRC_INITIAL_PHASE_SHIFT (16)
#define GDP_VSRC_NO_SKIP             (1L<<23) /* not implemented yet */
#define GDP_VSRC_FILTER_EN           (1L<<24)
#define GDP_VSRC_ADAPTIVE_FLICKERFIL (1L<<25) /* 7109Cut3 */
#define GDP_VSRC_VSRC_INC_XTN_SHIFT  (27)

#define GDP_PPT_IGNORE_ON_MIX1  (1L<<0)
#define GDP_PPT_IGNORE_ON_MIX2  (1L<<1)
#define GDP_PPT_FORCE_ON_MIX1   (1L<<2)
#define GDP_PPT_FORCE_ON_MIX2   (1L<<3)

//Video display register definitions
#define DISP_CTL__OUTPUT_ENABLE                     0x80000000
#define DISP_CTL__LOAD_HFILTER_COEFFS               0x40000000
#define DISP_CTL__LOAD_VFILTER_COEFFS               0x20000000
#define DISP_CTL__BIG_ENDIAN_INPUT                  0x00800000
#define DISP_CTL__CHROMA_HFILTER_ENABLE             0x00400000
#define DISP_CTL__LUMA_HFILTER_ENABLE               0x00200000
#define DISP_CTL__OUTPUT_422NOT444                  0x00100000

#define DISP_MA_CTL__CHROMA_AS_FIELD                0x04000000
#define DISP_MA_CTL__COEF_LOAD_LINE_NUM(line)       ((line     & 0x1f)  <<6 )
#define DISP_MA_CTL__PIX_LOAD_LINE_NUM(line)        ((line     & 0x1f)  <<11)
#define DISP_MA_CTL__MIN_MEM_REQ_INTERVAL(t2Cycles) ((t2Cycles & 0x3ff) <<16)
#define DISP_MA_CTL__LUMA_AS_FIELD                  0x00000020
#define DISP_MA_CTL__FORMAT_YCBCR_420_MB            0x00000014
#define DISP_MA_CTL__FORMAT_YCBCR_422_MB            0x00000013
#define DISP_MA_CTL__FORMAT_YCBCR_422_R             0x00000012

//Mixer crossbar register definitions
#define MIX_CRB_NOTHING      0x00
#define MIX_CRB_VID1         0x01
#define MIX_CRB_VID2         0x02
#define MIX_CRB_GDP1         0x03
#define MIX_CRB_GDP2         0x04
#define MIX_CRB_GDP3         0x05
#define MIX_CRB_GDP4         0x06
#define MIX_CRB_DEPTH1_SHIFT 0
#define MIX_CRB_DEPTH2_SHIFT 3
#define MIX_CRB_DEPTH3_SHIFT 6
#define MIX_CRB_DEPTH4_SHIFT 9
#define MIX_CRB_DEPTH5_SHIFT 12
#define MIX_CRB_DEPTH6_SHIFT 15

#define CUR_CTL_CLUT_UPDATE  (1L<<1)

#endif //GENERICGAMMADEFS_H
