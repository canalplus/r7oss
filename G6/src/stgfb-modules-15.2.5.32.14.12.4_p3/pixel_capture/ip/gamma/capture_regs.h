/***********************************************************************
 *
 * File: pixel_capture/ip/gamma/capture_regs.h
 * Copyright (c) 2012 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef GAMMA_CAPTURE_REGS_H
#define GAMMA_CAPTURE_REGS_H

/***********************************************************************/
/** Gamma Compositor Capture Registers.                               **/
/***********************************************************************/

/*
 * The CAP Control register provides the operating mode of the capture
 * pipeline.
 */
#define GAM_CAPn_CTL    0x00

/*
 * XDO and XDS parameters are given with respect to the first pixel of
 * the active area.
 *
 * The YDO and YDS value are specified in reference to a frame line-numbering,
 * even in an interlaced display. To capture the complete active height of an
 * object, YDO/YDS must be programmed as in the source pipeline (for example
 * as VID1_VPO and VID1_VPS, if capturing VID1 ouptput).
 */
#define GAM_CAPn_CWO    0x04
#define GAM_CAPn_CWS    0x08

/*
 * The CAP Video Top field Pointer register is a 32-bit register containing
 * the memory location for the top field first pixel to be stored (top-left corner).
 */
#define GAM_CAPn_VTP    0x14

/*
 * The CAP Video Bottom field Pointer register is a 32-bit register containing
 * the memory location for the bottom field first pixel to be stored (top-left corner).
 */
#define GAM_CAPn_VBP    0x18

/*
 * The CAP Video Memory Pitch register contains the memory pitch for
 * the captured video, as stored in the memory.
 */
#define GAM_CAPn_PMP    0x1c

/*
 * The CAP Captured memory window register provides the size of the captured picture,
 * after the horizontal sample rate converter. Vertically, as there is no sample rate
 * conversion, the top height (respectively the bottom height) always corresponds to
 * CAP_TFO / CAP_TFS registers (respectively to CAP_BFO / CAP_BFS registers).
 */
#define GAM_CAPn_CMW    0x20

/*
 * The CAP Horizontal Sample Rate Converter register provides the configuration for
 * the horizontal sample rate converter.
 */
#define GAM_CAPn_HSRC   0x30
#define GAM_CAPn_HFC0   0x34
#define GAM_CAPn_HFC1   0x38
#define GAM_CAPn_HFC2   0x3c
#define GAM_CAPn_HFC3   0x40
#define GAM_CAPn_HFC4   0x44
#define GAM_CAPn_HFC5   0x48
#define GAM_CAPn_HFC6   0x4c
#define GAM_CAPn_HFC7   0x50
#define GAM_CAPn_HFC8   0x54
#define GAM_CAPn_HFC9   0x58

/*
 * The CAP Vertical Sample Rate Converter register provides the configuration for the
 * vertical sample rate converter.
 */
#define GAM_CAPn_VSRC   0x5c
#define GAM_CAPn_VFC0   0x60
#define GAM_CAPn_VFC1   0x64
#define GAM_CAPn_VFC2   0x68
#define GAM_CAPn_VFC3   0x6c
#define GAM_CAPn_VFC4   0x70
#define GAM_CAPn_VFC5   0x74

/*
 * Modern CAPTURE implementations use these instead of the PKZ register
 * to configure memory bus transactions.
 */
#define GAM_CAPn_PAS    0xec
#define GAM_CAPn_MAOS   0xf0
#define GAM_CAPn_MACS   0xf8
#define GAM_CAPn_MAMS   0xfc

#endif //GAMMA_CAPTURE_REGS_H
