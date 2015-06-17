/***********************************************************************
 *
 * File: display/ip/hdf/stmhdfreg.h
 * Copyright (c) 2011 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef _STM_HDFREG_H
#define _STM_HDFREG_H

/* HD formatter registers --------------------------------------------*/
#define HDF_ANA_ANCILIARY_CTRL   0x010
#define HDF_LUMA_SRC_CFG         0x014
#define HDF_LUMA_COEFF_P1_T123   0x018
#define HDF_LUMA_COEFF_P1_T456   0x01C
#define HDF_LUMA_COEFF_P2_T123   0x020
#define HDF_LUMA_COEFF_P2_T456   0x024
#define HDF_LUMA_COEFF_P3_T123   0x028
#define HDF_LUMA_COEFF_P3_T456   0x02C
#define HDF_LUMA_COEFF_P4_T123   0x030
#define HDF_LUMA_COEFF_P4_T456   0x034
#define HDF_CHROMA_SRC_CFG       0x040
#define HDF_CHROMA_COEFF_P1_T123 0x044
#define HDF_CHROMA_COEFF_P1_T456 0x048
#define HDF_CHROMA_COEFF_P2_T123 0x04C
#define HDF_CHROMA_COEFF_P2_T456 0x050
#define HDF_CHROMA_COEFF_P3_T123 0x054
#define HDF_CHROMA_COEFF_P3_T456 0x058
#define HDF_CHROMA_COEFF_P4_T123 0x05C
#define HDF_CHROMA_COEFF_P4_T456 0x060

#define HDF_DIG1_CFG      0x100
#define HDF_DIG1_YC_DELAY 0x104

#define HDF_DIG2_CFG      0x180
#define HDF_DIG2_YC_DELAY 0x184

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

#endif // _STM_HDFREG_H
