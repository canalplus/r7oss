/***********************************************************************
 *
 * File: display/ip/vdp/VdpReg.h
 * Copyright (c) 2014 by STMicroelectronics. All rights reserved.
 *
 * This file is subject to the terms and conditions of the GNU General Public License.
 * See the file COPYING in the main directory of this archive formore details.
 *
\***********************************************************************/
#ifndef _VDP_REG_H_
#define _VDP_REG_H_

// registers
#define DEI_CTL                0x0000 // DEI control register
#define DEI_VIEWPORT_ORIG      0x0004 // output viewport origin
#define DEI_VIEWPORT_SIZE      0x0008 // output viewport size
#define DEI_VF_SIZE            0x000C // input video fields, size
#define DEI_T3I_CTL            0x0010 // stbus type 3 initiator control
#define DEI_PYF_BA_unused      0x0014
#define DEI_CYF_BA             0x0018 // current video field, luma buffer base address
#define DEI_NYF_BA_unused      0x001C
#define DEI_PCF_BA_unused      0x0020
#define DEI_CCF_BA             0x0024 // current video field, chroma buffer base address
#define DEI_NCF_BA_unused      0x0028
#define DEI_PMF_BA_unused      0x002C
#define DEI_CMF_BA_unused      0x0030
#define DEI_NMF_BA_unused      0x0034
#define DEI_YF_FORMAT          0x0038 // video fields, luma buffer memory format
#define DEI_YF_STACK_L0        0x003C // luma line buffers, address stack level i
#define DEI_YF_STACK_L1        0x0040
#define DEI_YF_STACK_L2        0x0044
#define DEI_YF_STACK_L3        0x0048
#define DEI_YF_STACK_L4        0x004C
#define DEI_YF_STACK_P0        0x0050 // luma pixel buffers, address stack level i
#define DEI_YF_STACK_P1        0x0054
#define DEI_YF_STACK_P2        0x0058
#define DEI_CF_FORMAT          0x005C // video fields, chroma buffer memory format
#define DEI_CF_STACK_L0        0x0060 // chroma line buffers, address stack level i
#define DEI_CF_STACK_L1        0x0064
#define DEI_CF_STACK_L2        0x0068
#define DEI_CF_STACK_L3        0x006C
#define DEI_CF_STACK_L4        0x0070
#define DEI_CF_STACK_P0        0x0074 // chroma pixel buffers, address stack level i
#define DEI_CF_STACK_P1        0x0078
#define DEI_CF_STACK_P2        0x007C
#define DEI_MF_STACK_L0_unused 0x0080
#define DEI_MF_STACK_P0_unused 0x0084
#define DEI_DEBUG1             0x0088 // Some Controls for Debug
#define DEI_STATUS1            0x0088 // Some flags for Debug

#define VHSRC_CTL              0x0100 // Vertical and horizontal filters Control
#define VHSRC_Y_HSRC           0x0104 // Luma horizontal SRC FSM control
#define VHSRC_Y_VSRC           0x0108 // Luma vertical SRC FSM control
#define VHSRC_C_HSRC           0x010c // Chroma horizontal SRC FSM control
#define VHSRC_C_VSRC           0x0110 // Chroma vertical SRC FSM control
#define VHSRC_TARGET_SIZE      0x0114 // Target size
#define VHSRC_NLZZD_Y          0x0118 // Non linear zoom zone definition for luma SRC
#define VHSRC_NLZZD_C          0x011c // Non linear zoom zone definition for chroma SRC
#define VHSRC_PDELTA           0x0120 // Non linear zoom increment step definition
#define VHSRC_Y_SIZE           0x0124 // Luma source pixmap size
#define VHSRC_C_SIZE           0x0128 // Chroma source pixmap size
#define VHSRC_HCOEF_BA         0x012C // horizontal filter coefficients, base address
#define VHSRC_VCOEF_BA         0x0130 // vertical filter coefficients, base address

#define P2I_PXF_IT_STATUS      0x0300 // P2I & pixel FIFO status
#define P2I_PXF_IT_MASK        0x0304 // P2I & pixel FIFO mask
#define P2I_PXF_CONF_unused    0x0308

#define Y_CRC_CHECK_SUM        0x0400 // Luma Cyclic Redundancy Checksum register
#define UV_CRC_CHECK_SUM       0x0404 // Chroma Cyclic Redundancy Checksum register

#define DEFAULT_COLOR          0x0600 // Main video default color register

// DEI_CTL register
#define DEI_CTL_INACTIVE                 ( 0x0001 << 00 )
#define DEI_CTL_CVF_TYPE_MASK            ( 0x0007 << 28)
#define DEI_CTL_CVF_TYPE_420_EVEN        ( 0x0000 << 28 )
#define DEI_CTL_CVF_TYPE_420_ODD         ( 0x0002 << 28 )
#define DEI_CTL_CVF_TYPE_420_PROGRESSIVE ( 0x0004 << 28 )
#define DEI_CTL_CVF_TYPE_422_EVEN        ( 0x0001 << 28 )
#define DEI_CTL_CVF_TYPE_422_ODD         ( 0x0003 << 28 )
#define DEI_CTL_CVF_TYPE_422_PROGRESSIVE ( 0x0005 << 28 )

// DEI_T3I_CTL register
#define DEI_T3I_CTL_CHUNK_SIZE             ( 0x001F << 00 )
#define DEI_T3I_CTL_OPCODE_SIZE_8          ( 0x0003 << 05 )
#define DEI_T3I_CTL_OPCODE_SIZE_16         ( 0x0004 << 05 )
#define DEI_T3I_CTL_OPCODE_SIZE_32         ( 0x0005 << 05 )
#define DEI_T3I_CTL_WAIT_AFTER_VSYNC       ( 0x001F << 08 )
#define DEI_T3I_CTL_MACRO_BLOCK_ENABLE     ( 0x0001 << 13 )
#define DEI_T3I_CTL_LUMA_CHROMA_ENDIANESS  ( 0x0001 << 14 )
#define DEI_T3I_CTL_LUMA_CHROMA_BUFFERS    ( 0x0001 << 15 )
#define DEI_T3I_CTL_MIN_SPACE_BETWEEN_REQS ( 0x03FF << 16 )
#define DEI_T3I_CTL_PRIORITY               ( 0x000F << 26 )

// VHSRC_CTL register
#define VHSRC_CTL_OUTPUT_444             ( 0x0000 << 00 )
#define VHSRC_CTL_OUTPUT_422             ( 0x0001 << 00 )
#define VHSRC_CTL_ENA_YHF                ( 0x0001 << 01 )
#define VHSRC_CTL_ENA_CHF                ( 0x0001 << 02 )
#define VHSRC_CTL_ENA_VFILTER_UPDATE     ( 0x0001 << 03 )
#define VHSRC_CTL_ENA_HFILTER_UPDATE     ( 0x0001 << 04 )
#define VHSRC_CTL_ENA_VHSRC              ( 0x0001 << 05 )
#define VHSRC_CTL_COEF_LOAD_LINE( line ) (( line & 0x001f ) << 16 )
#define VHSRC_CTL_PIX_LOAD_LINE( line )  (( line & 0x001f ) << 21 )

// P2I_PXF_IT_STATUS register
#define P2I_STATUS_END_PROCESSING ( 0x0001 << 00 )
#define P2I_STATUS_IT_FIFO_EMPTY  ( 0x0001 << 01 )

#define DEI_FORMAT_REVERSE          (0x0)    /* input bytes 12345678 -> output bytes 87654321     */
#define DEI_FORMAT_IDENTITY         (0x7)    /* input bytes = output bytes                        */
#define DEI_REORDER_IDENTITY        (0x0)    /* input bytes = output bytes                        */
#define DEI_REORDER_13245768        (0x1 << 4) /* input 12345678 -> 13245768                        */
#define DEI_REORDER_15263748        (0x2 << 4) /* input 12345678 -> 15263748 (half word interleave) */
#define DEI_REORDER_WORD_INTERLEAVE (0x3 << 4) /* input 12345678,abcdefghi -> 1a2b3c4d,5e6f7g8h     */

#define DEI_STACK(offset,ceil) ((( offset ) & 0x3FFFFF ) << 10 | (( ceil ) & 0x3FF ))

#endif
