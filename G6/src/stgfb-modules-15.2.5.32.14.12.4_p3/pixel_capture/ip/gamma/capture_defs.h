/***********************************************************************
 *
 * File: pixel_capture/ip/gamma/capture_defs.h
 * Copyright (c) 2012 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef GAMMA_CAPTURE_DEFS_H
#define GAMMA_CAPTURE_DEFS_H

/***********************************************************************/
/** Generic CAP Register Definitions.                                 **/
/***********************************************************************/


#define CAP_CTL_SOURCE_RESERVED           0x0
#define CAP_CTL_SOURCE_VID1               0x01
#define CAP_CTL_SOURCE_VID2               0x02
#define CAP_CTL_SOURCE_EXT_AYCbCr         0x03
#define CAP_CTL_SOURCE_MIX2_YCbCr         0x0A
#define CAP_CTL_SOURCE_MIX2_RGB           0x0B
#define CAP_CTL_SOURCE_MIX1_YCbCr         0x0E
#define CAP_CTL_SOURCE_MIX1_RGB           0x0F
#define CAP_CTL_SEL_SOURCE_OFFSET         0
#define CAP_CTL_SEL_SOURCE_MASK           0x0F

/*
 * The effective register update is synchronised on the Vsync event of
 * the selected VTG (CAP_CTL.VTG).
 */
#define CAP_CTL_VTG_SELECT                (1L<<4)
#define CAP_CTL_BFCAP_ENA                 (1L<<5)
#define CAP_CTL_TFCAP_ENA                 (1L<<6)

/*
 * Two capture modes are supported : a continuous mode and a single-shot mode.
 *
 * * Continous mode :
 *   * Interlaced display :
 *      The capture always starts on the first TOP field following the
 *      CAP_CTL.CAPTURE register update. When the CAP_CTL.CAPTURE register is
 *      reset by the CPU, the capture process ends at the end of the current
 *      field (top or bottom). During the capture process, it is possible to
 *      select the top field capture and the bottom field capture individually
 *      (CAP_CTL.TFCAP and CAP_CTL.BFCAP).
 *   * Progressive display :
 *      The capture starts on the next frame following the CAP_CTL.CAPTURE
 *      register update. When the CAP_CTL.CAPTURE register is reset by the CPU,
 *      the capture process ends at the end of the current frame.CAP_CTL.TFCAP
 *      and CAP_CTL.BFCAP must be set to 1 to capture a frame, if they are set
 *      to zero, nothing is captured.
 * * Single-shot mode :
 *   * Interlaced display :
 *      The capture process starts on the first TOP field following the
 *      CAP_CTL.CAPTURE register update, and ends automatically 2 fields later.
 *      The top field is captured if CAP_CTL.TFCAP is 1, and the bottom field
 *      is captured if CAP_CTL.BFCAP is 1.
 *   * Progressive display :
 *      The capture starts on the next frame following the CAP_CTL.CAPTURE
 *      register update, and ends automatically 1 frame later. The frame is
 *      captured if CAP_CTL.TFCAP is 1.
 */
#define CAP_CTL_SSCAP_SINGLE_SHOT         (1L<<7)

/*
 * The CAP_CTL.CAPTURE register must be set to launch the capture process.
 */
#define CAP_CTL_CAPTURE_ENA               (1L<<8)
#define CAP_CTL_EN_V_RESIZE               (1L<<9)
#define CAP_CTL_EN_H_RESIZE               (1L<<10)

#define CAP_CTL_RGB_565                   0x00
#define CAP_CTL_RGB_888                   0x01
#define CAP_CTL_ARGB_8565                 0x04
#define CAP_CTL_ARGB_8888                 0x05
#define CAP_CTL_ARGB_1555                 0x06
#define CAP_CTL_ARGB_4444                 0x07
#define CAP_CTL_YCbCr888                  0x10
#define CAP_CTL_YCbCr422R                 0x12
/*
 * The 32BIT_WORD format must be used when capturing 30-bit MIXn outputs.
 * In that case, the 30 bits are right-justified. In other words, this
 * test format can be considered eithe as RGB10-10-10- or CrYCb10-10-10.
 */
#define CAP_CTL_32_BITS_WORD              0x1E
#define CAP_CTL_FORMAT_OFFSET             16
#define CAP_CTL_FORMAT_MASK               0x1F

#define CAP_CTL_BIGENDIAN                 (1L<<23)
#define CAP_CTL_YCBCR2RGB_ENA             (1L<<24)
#define CAP_CTL_BF_709_SELECT             (1L<<25)
#define CAP_CTL_CHROMA_SIGN_IN            (1L<<26)
#define CAP_CTL_CHROMA_SIGN_OUT           (1L<<27)

#define CAP_CMW_BOT_HEIGHT_SHIFT          (30)
#define CAP_CMW_BOT_EQ_TOP_HEIGHT         (0x0<<CAP_CMW_BOT_HEIGHT_SHIFT)
#define CAP_CMW_BOT_EQ_TOP_PLUS_HEIGHT    (0x1<<CAP_CMW_BOT_HEIGHT_SHIFT)
#define CAP_CMW_BOT_EQ_TOP_MINUS_HEIGHT   (0x2<<CAP_CMW_BOT_HEIGHT_SHIFT)

#define CAP_HSRC_INITIAL_PHASE_SHIFT      (16)
#define CAP_HSRC_INITIAL_PHASE_MASK       0x7
#define CAP_HSRC_FILTER_EN                (1L<<24)

#define CAP_VSRC_TOP_INITIAL_PHASE_SHIFT  (16)
#define CAP_VSRC_BOT_INITIAL_PHASE_SHIFT  (20)
#define CAP_VSRC_INITIAL_PHASE_MASK       0x7
#define CAP_VSRC_FILTER_EN                (1L<<24)

#endif //GAMMA_CAPTURE_DEFS_H
