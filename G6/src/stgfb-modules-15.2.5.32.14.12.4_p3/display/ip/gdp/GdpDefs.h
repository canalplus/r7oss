/***********************************************************************
 *
 * File: display/ip/gdp/GdpDefs.h
 * Copyright (c) 2000, 2004, 2005, 2014 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef GDPDEFS_H
#define GDPDEFS_H

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

#define GDP_VPS_XDS_MASK     (0x7FFF)
#define GDP_VPS_YDS_MASK     (0x7FFF<<16)

#define GDP_PMP_PITCH_VALUE_MASK     (0xFFFF)

#define GDP_SIZE_WIDTH_MASK    (0x07FF)
#define GDP_SIZE_HEIGHT_MASK   (0x07FF<<16)

#define GDP_VSRC_INITIAL_PHASE_SHIFT (16)
#define GDP_VSRC_NO_SKIP             (1L<<23) /* not implemented yet */
#define GDP_VSRC_FILTER_EN           (1L<<24)
#define GDP_VSRC_ADAPTIVE_FLICKERFIL (1L<<25) /* 7109Cut3 */
#define GDP_VSRC_VSRC_INC_XTN_SHIFT  (27)
#define GDP_VSRC_VSRC_INCREMENT_MASK (0x1FFF)

#define GDP_PPT_IGNORE_ON_MIX1  (1L<<0)
#define GDP_PPT_IGNORE_ON_MIX2  (1L<<1)
#define GDP_PPT_FORCE_ON_MIX1   (1L<<2)
#define GDP_PPT_FORCE_ON_MIX2   (1L<<3)

#endif //GDPDEFS_H
