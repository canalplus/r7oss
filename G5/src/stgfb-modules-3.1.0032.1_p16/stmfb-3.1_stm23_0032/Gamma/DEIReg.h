/***********************************************************************
 *
 * File: Gamma/DEIReg.h
 * Copyright (c) 2006-2009 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/
#ifndef _DEI_REG_H
#define _DEI_REG_H

#define DEI_CTL           0x000
#define DEI_VP_ORIGIN     0x004 // Source image origin
#define DEI_VP_SIZE       0x008 // Source image size
#define DEI_VF_SIZE       0x00C // Macroblock stride
#define DEI_T3I_CTL       0x010 // Type3 interface control (MB/Raster/Planar/Interleaved)
#define DEI_PYF_BA        0x014 // Luma base addresses
#define DEI_CYF_BA        0x018
#define DEI_NYF_BA        0x01C
#define DEI_PCF_BA        0x020 // Chroma base addresses
#define DEI_CCF_BA        0x024
#define DEI_NCF_BA        0x028
#define DEI_PMF_BA        0x02C // Motion base addresses
#define DEI_CMF_BA        0x030
#define DEI_NMF_BA        0x034
#define DEI_YF_FORMAT     0x038 // Luma endianess control
#define DEI_YF_STACK_L0   0x03C // Luma Line addressing loops
#define DEI_YF_STACK_L1   0x040
#define DEI_YF_STACK_L2   0x044
#define DEI_YF_STACK_L3   0x048
#define DEI_YF_STACK_L4   0x04C
#define DEI_YF_STACK_P0   0x050 // Luma Pixel addressing loops
#define DEI_YF_STACK_P1   0x054
#define DEI_YF_STACK_P2   0x058
#define DEI_CF_FORMAT     0x05C // Chroma endianess and reordering control
#define DEI_CF_STACK_L0   0x060 // Chroma Line addressing loops
#define DEI_CF_STACK_L1   0x064
#define DEI_CF_STACK_L2   0x068
#define DEI_CF_STACK_L3   0x06C
#define DEI_CF_STACK_L4   0x070
#define DEI_CF_STACK_P0   0x074 // Chroma Pixel addressing loops
#define DEI_CF_STACK_P1   0x078
#define DEI_CF_STACK_P2   0x07C
#define DEI_MF_STACK_L0   0x080 // Motion Line addressing loop
#define DEI_MF_STACK_P0   0x084 // Motion Pixel addressing loop

#define FMD_THRESHOLD_SCD  0x090
#define FMD_THRESHOLD_RFD  0x094
#define FMD_THRESHOLD_MOVE 0x098
#define FMD_THRESHOLD_CFD  0x09C
#define FMD_BLOCK_NUMBER   0x0A0
#define FMD_CFD_SUM        0x0A4
#define FMD_FIELD_SUM      0x0A8
#define FMD_STATUS         0x0AC
#  define FMD_STATUS_SCENE_COUNT_MASK        (0x1ff)
#  define FMD_STATUS_MOVE_STATUS_MASK        (1 <<  9)
#  define FMD_STATUS_REPEAT_STATUS_MASK      (1 << 10)
#  define FMD_STATUS_C2_SCENE_COUNT_MASK     (0x7ff)
#  define FMD_STATUS_C2_MOVE_STATUS_MASK     (1 << 11)
#  define FMD_STATUS_C2_REPEAT_STATUS_MASK   (1 << 12)

#define DEI_T3I_MOTION_CTL 0x0B0 // Plug control for motion data, 7111 onwards

#define VHSRC_CTL          0x100
#define VHSRC_LUMA_HSRC    0x104
#define VHSRC_LUMA_VSRC    0x108
#define VHSRC_CHR_HSRC     0x10c
#define VHSRC_CHR_VSRC     0x110
#define VHSRC_TARGET_SIZE  0x114
#define VHSRC_NLZZD_Y      0x118
#define VHSRC_NLZZD_C      0x11c
#define VHSRC_PDELTA       0x120
#define VHSRC_LUMA_SIZE    0x124
#define VHSRC_CHR_SIZE     0x128
#define VHSRC_HFP          0x12C
#define VHSRC_VFP          0x130

#define P2I_PXF_IT_STATUS  0x300
#define P2I_PXF_CONF       0x308

#define Y_CRC_CHECK_SUM    0x400
#define UV_CRC_CHECK_SUM   0x404


/* common */
#define DEI_CTL_NOT_ACTIVE                             (1 << 0)
#define DEI_CTL_BYPASS                                 (1 << 1)
#define DEI_CTL_DD_MODE_MASK                           (3 << 10)
#  define DEI_CTL_DD_MODE_DIAG5                          (0)
#  define DEI_CTL_DD_MODE_DIAG7                          (1 << 10)
#  define DEI_CTL_DD_MODE_DIAG9                          (2 << 10)
#define DEI_CTL_F_MODE_LMU_NOT_MLD                     (1 << 12)
#define DEI_CTL_CLAMP_MODE_DIRECTIONAL_NOT_VERTICAL    (1 << 13)
#define DEI_CTL_DIR3_ENABLE                            (1 << 14)
#define DEI_CTL_DIR_RECURSIVE_ENABLE                   (1 << 15)
#define DEI_CTL_KCORRECTION_SHIFT                      (16)
#define DEI_CTL_KCORRECTION_MASK                       (0xF << DEI_CTL_KCORRECTION_SHIFT)
#define DEI_CTL_T_DETAIL_SHIFT                         (20)
#define DEI_CTL_T_DETAIL_MASK                          (0xF<< DEI_CTL_T_DETAIL_SHIFT)
#define DEI_CTL_FMD_FIELD_SWAP                         (1 << 26)
#define DEI_CTL_CVF_TYPE_MASK                          (7 << 28)
#  define DEI_CTL_CVF_TYPE_420_EVEN                      (0)
#  define DEI_CTL_CVF_TYPE_420_ODD                       (2 << 28)
#  define DEI_CTL_CVF_TYPE_420_PROGRESSIVE               (4 << 28)
#  define DEI_CTL_CVF_TYPE_422_EVEN                      (1 << 28)
#  define DEI_CTL_CVF_TYPE_422_ODD                       (3 << 28)
#  define DEI_CTL_CVF_TYPE_422_PROGRESSIVE               (5 << 28)

/*
 * values for both luma and chroma interpolation control, the shifts in the
 * register are defined below as they are different for different versions
 * of the IP.
 */
#define DEI_CTL_INTERP_VERTICAL                (0)
#define DEI_CTL_INTERP_DIRECTIONAL             (1)
#define DEI_CTL_INTERP_MEDIAN                  (2)
#define DEI_CTL_INTERP_3D                      (3)
#define DEI_CTL_INTERP_MASK                    (3)

/* 7109c3, 7200cut1 */
#define C1_DEI_CTL_MODE_MASK                           (3 << 2)
#  define C1_DEI_CTL_MODE_FIELD_MERGING                  (1 << 2)
#  define C1_DEI_CTL_MODE_DEINTERLACING                  (2 << 2)
#  define C1_DEI_CTL_MODE_TIME_UPCONV                    (3 << 2)
#define C1_DEI_CTL_LUMA_INTERP_SHIFT                   (4)
#define C1_DEI_CTL_CHROMA_INTERP_SHIFT                 (6)
#define C1_DEI_CTL_MD_MODE_SHIFT                       (8)
#define C1_DEI_CTL_MD_MODE_MASK                        (3 << C1_DEI_CTL_MD_MODE_SHIFT)
#  define C1_DEI_CTL_MD_MODE_OFF                         (0)
#  define C1_DEI_CTL_MD_MODE_INIT                        (1 << C1_DEI_CTL_MD_MODE_SHIFT)
#  define C1_DEI_CTL_MD_MODE_LOW_CONF                    (2 << C1_DEI_CTL_MD_MODE_SHIFT)
#  define C1_DEI_CTL_MD_MODE_FULL_CONF                   (3 << C1_DEI_CTL_MD_MODE_SHIFT)
#define C1_DEI_CTL_KMOV_FACTOR_14_NOT_15               (1 << 24)
#define C1_DEI_CTL_FMD_ENABLE                          (1 << 25)
/* 7200c2,7111,7105,7141 */
#define C2_DEI_CTL_MAIN_MODE                           (1 << 2) /* 0: DEI, 1: field merging */
#define C2_DEI_CTL_LUMA_INTERP_SHIFT                   (3)
#define C2_DEI_CTL_CHROMA_INTERP_SHIFT                 (5)
#define C2_DEI_CTL_MD_MODE_SHIFT                       (7)
#define C2_DEI_CTL_MD_MODE_MASK                        (7 << C2_DEI_CTL_MD_MODE_SHIFT)
#  define C2_DEI_CTL_MD_MODE_OFF                         (0 << C2_DEI_CTL_MD_MODE_SHIFT)
#  define C2_DEI_CTL_MD_MODE_INIT                        (1 << C2_DEI_CTL_MD_MODE_SHIFT)
#  define C2_DEI_CTL_MD_MODE_LOW_CONF                    (2 << C2_DEI_CTL_MD_MODE_SHIFT)
#  define C2_DEI_CTL_MD_MODE_FULL_CONF                   (3 << C2_DEI_CTL_MD_MODE_SHIFT)
#  define C2_DEI_CTL_MD_MODE_NO_RECURSE                  (4 << C2_DEI_CTL_MD_MODE_SHIFT)
#define C2_DEI_CTL_KMOV_FACTOR_MASK                    (3 << 24)
#  define C2_DEI_CTL_KMOV_FACTOR_12                      (0 << 24)
#  define C2_DEI_CTL_KMOV_FACTOR_14                      (1 << 24)
#  define C2_DEI_CTL_KMOV_FACTOR_10                      (2 << 24)
#  define C2_DEI_CTL_KMOV_FACTOR_08                      (3 << 24)
#define C2_DEI_CTL_REDUCE_MOTION                       (1 << 27) /* 0: 8bit motion, 1: 4bit motion */
#define C2_DEI_CTL_FMD_ENABLE                          (1 << 31)

#define DEI_T3I_CTL_CHUNK_SIZE_SHIFT                (0)
#define DEI_T3I_CTL_CHUNK_SIZE_MASK                 (0x1F<<DEI_T3I_CTL_CHUNK_SIZE_SHIFT)
#define DEI_T3I_CTL_OPCODE_SIZE_8                   (3L<<5)
#define DEI_T3I_CTL_OPCODE_SIZE_16                  (4L<<5)
#define DEI_T3I_CTL_OPCODE_SIZE_32                  (5L<<5)
#define DEI_T3I_CTL_OPCODE_SIZE_MASK                (7L<<5)
#define DEI_T3I_CTL_WAIT_AFTER_VSYNC_SHIFT          (8)
#define DEI_T3I_CTL_WAIT_AFTER_VSYNC_MASK           (0x1F<<DEI_T3I_CTL_WAIT_AFTER_VSYNC_SHIFT)
#define DEI_T3I_CTL_MACRO_BLOCK_ENABLE              (1L<<13)
#define DEI_T3I_CTL_422R_YUYV_NOT_UYVY              (1L<<14)
#define DEI_T3I_CTL_422R_ENABLE                     (1L<<15)
#define DEI_T3I_CTL_MIN_SPACE_BETWEEN_REQS_SHIFT    (16)
#define DEI_T3I_CTL_MIN_SPACE_BETWEEN_REQS_MASK     (0x3FF<<DEI_T3I_CTL_MIN_SPACE_BETWEEN_REQS_SHIFT)
#define DEI_T3I_CTL_PRIORITY_SHIFT                  (26)
#define DEI_T3I_CTL_PRIORITY_MASK                   (0xF<<DEI_T3I_CTL_PRIORITY_SHIFT)

#define DEI_FORMAT_REVERSE                          (0x0L)    /* input bytes 12345678 -> output bytes 87654321     */
#define DEI_FORMAT_IDENTITY                         (0x7L)    /* input bytes = output bytes                        */

#define DEI_REORDER_IDENTITY                        (0x0L)    /* input bytes = output bytes                        */
#define DEI_REORDER_13245768                        (0x1L<<4) /* input 12345678 -> 13245768                        */
#define DEI_REORDER_15263748                        (0x2L<<4) /* input 12345678 -> 15263748 (half word interleave) */
#define DEI_REORDER_WORD_INTERLEAVE                 (0x3L<<4) /* input 12345678,abcdefghi -> 1a2b3c4d,5e6f7g8h     */

#define DEI_STACK(offset,ceil)                      (((offset) & 0x3FFFFF)<<10 | ((ceil) & 0x3FF))

#define VHSRC_CTL__OUTPUT_422NOT444                 (1L<<0)
#define VHSRC_CTL__LUMA_HFILTER_ENABLE              (1L<<1)
#define VHSRC_CTL__CHROMA_HFILTER_ENABLE            (1L<<2)
#define VHSRC_CTL__LOAD_VFILTER_COEFFS              (1L<<3)
#define VHSRC_CTL__LOAD_HFILTER_COEFFS              (1L<<4)
#define VHSRC_CTL__OUTPUT_ENABLE                    (1L<<5)
#define VHSRC_CTL__COEF_LOAD_LINE_NUM(line)         ((line & 0x1f)<<16)
#define VHSRC_CTL__PIX_LOAD_LINE_NUM(line)          ((line & 0x1f)<<21)

#define P2I_STATUS_END_PROCESSING                   (1L<<0)
#define P2I_STATUS_FIFO_EMPTY                       (1L<<1)

#define P2I_CONF_BYPASS_EN                          (1L<<0)
#define P2I_CONF_TOP_NOT_BOT                        (1L<<1)

#endif // _DEI_REG_H
