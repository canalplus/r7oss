/***********************************************************************
 *
 * File: pixel_capture/ip/dvp/dvp_regs.h
 * Copyright (c) 2013 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef DVP_IP_REGS_H
#define DVP_IP_REGS_H

/***********************************************************************/
/** DVP CAPTURE Block Registers (Read/Write).                         **/
/***********************************************************************/

/*
 * The CAP Control register provides the operating mode of the capture
 * pipeline.
 */
#define DVP_CAPn_CTL          0x00

/*
 * The Video Luma memory pointer.
 */
#define DVP_CAPn_VLP          0x14

/*
 * The Video Luma memory pointer.
 */
#define DVP_CAPn_VCP          0x18

/*
 * Pixmap memory pitch.
 */
#define DVP_CAPn_PMP          0x1C

/*
 * Captured memory window.
 */
#define DVP_CAPn_CMW          0x20

/*
 * The DVP CAPTURE Horizontal Sample Rate Converter register provides the
 * configuration for the horizontal Luma and Chroma sample rate converter.
 */
#define DVP_CAPn_HSRC_L       0x30  /* always same for H412 cut1.0 and cut2.0 */
#define DVP_CAPn_HSRC_C       0x60  /* Only for H412 cut2.0 */

/*
 * Horizontal Luma filter coefficients.
 */
#define DVP_CAPn_HFC0_L       0x34  /* always same for H412 cut1.0 and cut2.0 */

/*
 * Horizontal Chroma filter coefficients.
 */
#define DVP_CAPn_HFC0_C       0x64  /* Only for H412 cut2.0 */

/*
 * The DVP CAPTURE Vertical Sample Rate Converter register provides the
 * configuration for the vertical sample rate converter.
 */
#define DVP_CAPn_VSRC_HW_V1_4 0x5C  /* For H407/H412 cut1.0 DVP IP ver 1.4 */
#define DVP_CAPn_VSRC_HW_V1_6 0x90  /* For H412 cut2.0 DVP IP ver 1.6 */

/*
 * Vertical filter coefficients.
 */
#define DVP_CAPn_VFC0_HW_V1_4  0x60 /* For H407/H412 cut1.0 DVP IP ver 1.4 */
#define DVP_CAPn_VFC0_HW_V1_6  0x94 /* For H412 cut2.0 DVP IP ver 1.6 */

/*
 * Modern DVP CAPTURE memory bus transactions configuration.
 *
 *
 * CAP Page size, Chroma.
 */
#define DVP_CAPn_PAS2         0xD0

/*
 * CAP Maximum Opcode size, Chroma.
 */
#define DVP_CAPn_MAOS2        0xD4

/*
 * CAP Maximum Chunk size, Chroma.
 */
#define DVP_CAPn_MACS2        0xD8

/*
 * CAP Maximum Opcode size, Chroma.
 */
#define DVP_CAPn_MAMS2        0xDC

/*
 * CAP Page size, Luma.
 */
#define DVP_CAPn_PAS          0xEC

/*
 * CAP Maximum Opcode size, Luma.
 */
#define DVP_CAPn_MAOS         0xF0

/*
 * CAP Maximum Chunk size, Luma.
 */
#define DVP_CAPn_MACS         0xF8

/*
 * CAP Maximum Opcode size, Luma.
 */
#define DVP_CAPn_MAMS         0xFC


/***********************************************************************/
/** DVP MISC Block Registers (Read/Write).                            **/
/***********************************************************************/

/*
 * The MISC LOCK register is the information of HDMI RX locked.
 */
#define DVP_MISCn_LCK         0x00

/*
 * The MISC Status register is the information that the event occured.
 */
#define DVP_MISCn_STA         0x04

/*
 * The MISC IT Status register is the information that the event occured.
 */
#define DVP_MISCn_ITS         0x08

/*
 * The MISC IT Status Bit Clear register is used to clear the IT.
 */
#define DVP_MISCn_ITS_BCLR    0x0C

/*
 * The MISC IT Status Bit Clear register is used to set the IT.
 */
#define DVP_MISCn_ITS_BSET    0x10

/*
 * The MISC IT Mask register is used to mask the IT.
 */
#define DVP_MISCn_ITM         0x14

/*
 * The MISC IT Mask Bit Clear register is used to clear the IT.
 */
#define DVP_MISCn_ITM_BCLR    0x18

/*
 * The MISC IT Mask Bit Clear register is used to set the IT.
 */
#define DVP_MISCn_ITM_BSET    0x20


/***********************************************************************/
/** DVP DLL Block Registers (Read Only).                              **/
/***********************************************************************/

/*
 * The DLL TOT register provides the value of lines and pixels per field,
 * calculated by DLL.
 */
#define DVP_DLLn_TOT          0x00

/*
 * The DLL ACT register provides the value active window (lines and pixels),
 * calculated by DLL.
 */
#define DVP_DLLn_ACT          0x04

/*
 * The DLL INFO register provides information status of DLL.
 */
#define DVP_DLLn_INF          0x08


/***********************************************************************/
/** DVP SYNCHRO Block Registers (Read Only).                          **/
/***********************************************************************/

/*
 * The Synchro Control register provides the operating mode for the Field
 * Parity Detector.
 */
#define DVP_SYNCn_CTL         0x00

/*
 * Total number of pixel in a line including blanking.
 */
#define DVP_SYNCn_HTOT        0x04

/*
 * Total number of lines including blanking in a field.
 */
#define DVP_SYNCn_VTOT        0x08

/*
 * Total active number of pixel in a line including blanking.
 */
#define DVP_SYNCn_HACT        0x0C

/*
 * Total active number of lines including blanking in a field.
 */
#define DVP_SYNCn_VACT        0x10

/*
 * Number of blank pixel in a line before active part.
 */
#define DVP_SYNCn_HBLK        0x14

/*
 * Number of blank line in a field before active part.
 */
#define DVP_SYNCn_VBLK        0x18

/*
 * Thresholds for field parity detection.
 */
#define DVP_SYNCn_THLD        0x1C

/*
 * field parity information.
 */
#define DVP_SYNCn_STA         0x20

#endif //DVP_IP_REGS_H
