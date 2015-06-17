/***********************************************************************
 *
 * File: pixel_capture/ip/dvp/dvp_defs.h
 * Copyright (c) 2013 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef DVP_IP_DEFS_H
#define DVP_IP_DEFS_H

/***********************************************************************/
/** DVP DLL Block Register Definitions.                               **/
/***********************************************************************/

#define DLL_TOT_MASK                      0xFFFF
#define DLL_TOT_H_TOT_OFFSET              0x0
#define DLL_TOT_V_TOT_OFFSET              0x10

#define DLL_ACT_MASK                      0xFFFF
#define DLL_ACT_H_TOT_OFFSET              0x0
#define DLL_ACT_V_TOT_OFFSET              0x10

#define DLL_INFO_LOCKED_MASK              0x1
#define DLL_INFO_INTERLACED_MASK          0x2

/***********************************************************************/
/** DVP MATRIX Block Register Definitions.                            **/
/***********************************************************************/

#define MATRIX_MX0_OFFSET                 0x00
#define MATRIX_MX1_OFFSET                 0x04
#define MATRIX_MX2_OFFSET                 0x08
#define MATRIX_MX3_OFFSET                 0x0C
#define MATRIX_MX4_OFFSET                 0x10
#define MATRIX_MX5_OFFSET                 0x14
#define MATRIX_MX6_OFFSET                 0x18
#define MATRIX_MX7_OFFSET                 0x1C

#define MATRIX_MX0_C11_SHIFT              (0x00000000)
#define MATRIX_MX0_C12_SHIFT              (0x00000010)
#define MATRIX_MX1_C13_SHIFT              (0x00000000)
#define MATRIX_MX1_C21_SHIFT              (0x00000010)
#define MATRIX_MX2_C22_SHIFT              (0x00000000)
#define MATRIX_MX2_C23_SHIFT              (0x00000010)
#define MATRIX_MX3_C31_SHIFT              (0x00000000)
#define MATRIX_MX3_C32_SHIFT              (0x00000010)
#define MATRIX_MX4_C33_SHIFT              (0x00000000)

#define MATRIX_MX5_Offset1_SHIFT          (0x00000000)
#define MATRIX_MX5_Offset2_SHIFT          (0x00000010)
#define MATRIX_MX6_Offset3_SHIFT          (0x00000000)

#define MATRIX_MX6_inOff1_SHIFT           (0x00000010)
#define MATRIX_MX7_inOff2_SHIFT           (0x00000000)
#define MATRIX_MX7_inOff3_SHIFT           (0x00000010)

#define MATRIX_COEF_NUMBER                0x8

/***********************************************************************/
/** DVP MISC Block Register Definitions.                              **/
/***********************************************************************/

#define MISC_VSYNC_MASK                   0x1
#define MISC_EOCAP_MASK                   0x2
#define MISC_EOLOCK_MASK                  0x4
#define MISC_EOFIELD_MASK                 0x8
#define MISC_ALL_MASK                     0xF

/***********************************************************************/
/** DVP CAPTURE Block Register Definitions.                           **/
/***********************************************************************/

#define CAP_CTL_SOURCE_RGB888             0x00
#define CAP_CTL_SOURCE_YCBCR888           0x01
#define CAP_CTL_SOURCE_RGB101010          0x02
#define CAP_CTL_SOURCE_YCBCR101010        0x03
#define CAP_CTL_SOURCE_YCBCR422_8BITS     0x04
#define CAP_CTL_SOURCE_YCBCR422_10BITS    0x05
#define CAP_CTL_SOURCE_SB_10BITS_MASK     0x02
#define CAP_CTL_SOURCE_DB_10BITS_MASK     0x01
#define CAP_CTL_SEL_SOURCE_MASK           0x07
#define CAP_CTL_SEL_SOURCE_OFFSET         0x00

/*
 * The DVP_CTL.CAPTURE register must be set to launch the capture process.
 */
#define CAP_CTL_CAPTURE_ENA               (1L<<8)
#define CAP_CTL_EN_V_RESIZE               (1L<<9)
#define CAP_CTL_EN_H_RESIZE               (1L<<10)

#define CAP_CTL_RGB_888                   0x00
#define CAP_CTL_YCbCr888                  0x01
#define CAP_CTL_RGB_101010                0x02
#define CAP_CTL_YCbCr101010               0x03
#define CAP_CTL_YCbCr422RDB_8             0x04
#define CAP_CTL_YCbCr422RDB_10            0x05
#define CAP_CTL_FORMAT_OFFSET             16
#define CAP_CTL_FORMAT_MASK               0x7

#define CAP_CTL_BYPASS_422                (1L<<19)

#define CAP_CTL_CHROMA_SIGN_OUT           (1L<<27)

#define CAP_CMW_BOT_HEIGHT_SHIFT          (30)
#define CAP_CMW_BOT_EQ_TOP_HEIGHT         (0x0<<CAP_CMW_BOT_HEIGHT_SHIFT)
#define CAP_CMW_BOT_EQ_TOP_PLUS_HEIGHT    (0x1<<CAP_CMW_BOT_HEIGHT_SHIFT)
#define CAP_CMW_BOT_EQ_TOP_MINUS_HEIGHT   (0x2<<CAP_CMW_BOT_HEIGHT_SHIFT)

#define CAP_HSRC_INITIAL_PHASE_SHIFT      (16)
#define CAP_HSRC_INITIAL_PHASE_MASK       0x7

#define CAP_VSRC_TOP_INITIAL_PHASE_SHIFT  (16)
#define CAP_VSRC_BOT_INITIAL_PHASE_SHIFT  (20)
#define CAP_VSRC_INITIAL_PHASE_MASK       0x7

#define CAP_MAMS_MAX_MESSAGE_SIZE_SHIFT   (0x0)
#define CAP_MAMS_WRITE_POSTING_SHIFT      (0x00000003)
#define CAP_MAMS_MIN_SPACE_BW_REQ_SHIFT   (0x00000010)

#define CAP_PMP_LUMA_MASK                  0x0000FFFF
#define CAP_PMP_CHROMA_MASK                0xFFFF0000

/***********************************************************************/
/** DVP SYNCHRO Block Register Definitions.                           **/
/***********************************************************************/

#define SYNC_INTERLACED_MASK              0x1
#define SYNC_HOR_POLARITY_MASK            0x2
#define SYNC_VER_POLARITY_MASK            0x4
#define SYNC_FIELD_DETECT_MASK            0x8

#define SYNC_V_LINE_TOP_OFFSET            0x0
#define SYNC_V_LINE_BOT_OFFSET            0x10
#define SYNC_TOT_ACT_LINES_MASK           0xFF

#define SYNC_STA_FIELD_POL_MASK           0x1

/***********************************************************************/
/** MUX IP Block Register Definitions.                                **/
/***********************************************************************/

#define MUX_CTL_SOURCE_HDMI_RX            0x00
#define MUX_CTL_SOURCE_VXI                0x01

#endif //DVP_IP_DEFS_H
