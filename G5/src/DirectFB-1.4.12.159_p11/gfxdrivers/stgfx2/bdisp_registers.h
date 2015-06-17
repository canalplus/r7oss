/*
   ST Microelectronics BDispII driver - register & bit definitions

   (c) Copyright 2007/2008  STMicroelectronics Ltd.

   All rights reserved.

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with this library; if not, write to the
   Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.
*/

#ifndef __BDISP_REGISTERS_H__
#define __BDISP_REGISTERS_H__

#include "stgfx2_features.h"


enum STM_BLITTER_FLAGS {
  STM_BLITTER_FLAGS_FLICKERFILTER           = 0x00000800,
  STM_BLITTER_FLAGS_RLE_DECODE              = 0x00080000,
};




#define BDISP_CTL      0x0
#define BDISP_STA      0x8


#define BDISP_AQ1_BASE 0x60

/*
 * Application queue offsets and defines
 */
#define BDISP_AQ_CTL      0x0
#define BDISP_AQ_IP       0x4
#define BDISP_AQ_LNA      0x8
#define BDISP_AQ_STA      0xc

#define BDISP_AQ_CTL_PRIORITY_MASK      (0x3 <<  0)
#define BDISP_AQ_CTL_IRQ_MASK           (0xf << 20)
#  define BDISP_AQ_CTL_IRQ_NODE_REPEAT    (0x1 << 20)
#  define BDISP_AQ_CTL_IRQ_QUEUE_STOPPED  (0x2 << 20)
#  define BDISP_AQ_CTL_IRQ_LNA_REACHED    (0x4 << 20)
#  define BDISP_AQ_CTL_IRQ_NODE_COMPLETED (0x8 << 20)
#define BDISP_AQ_CTL_EVENT_MASK         (0x7 << 24)
#  define BDISP_AQ_CTL_EVENT_SUSPEND      (0x0 << 24)
#  define BDISP_AQ_CTL_EVENT_ABORT_REPEAT (0x2 << 24)
#  define BDISP_AQ_CTL_EVENT_STOP         (0x4 << 24)
#define BDISP_AQ_CTL_QUEUE_EN           (0x1 << 31)



/*
 * Source/Dest type
 */
#define BLIT_TY_PIXMAP_PITCH_MASK             (0x0000ffff)
#define BLIT_TY_COLOR_FORM_MASK               (0x001f0000)
#  define BLIT_TY_COLOR_FORM_IS_RGB(ty)         (((ty) & (0x18 << 16)) == (0x00 << 16))
#    define BLIT_COLOR_FORM_RGB565                (0x00 << 16)
#    define BLIT_COLOR_FORM_RGB888                (0x01 << 16)
#    define BLIT_COLOR_FORM_ARGB8565              (0x04 << 16)
#    define BLIT_COLOR_FORM_ARGB8888              (0x05 << 16)
#    define BLIT_COLOR_FORM_ARGB1555              (0x06 << 16)
#    define BLIT_COLOR_FORM_ARGB4444              (0x07 << 16)
#  define BLIT_TY_COLOR_FORM_IS_CLUT(ty)        ((((ty) & (0x18 << 16)) == (0x08 << 16)) && (((ty) & BLIT_TY_COLOR_FORM_MASK) != BLIT_COLOR_FORM_YCBCR42XMBN))
#    define BLIT_COLOR_FORM_CLUT1                 (0x08 << 16)
#    define BLIT_COLOR_FORM_CLUT2                 (0x09 << 16)
#    define BLIT_COLOR_FORM_CLUT4                 (0x0a << 16)
#    define BLIT_COLOR_FORM_CLUT8                 (0x0b << 16)
#    define BLIT_COLOR_FORM_ACLUT44               (0x0c << 16)
#    define BLIT_COLOR_FORM_ACLUT88               (0x0d << 16)
#  define BLIT_TY_COLOR_FORM_IS_YUV(ty)         ((((ty) & (0x18 << 16)) == (0x10 << 16)) || (((ty) & (0x0f << 16)) == (0x0e << 16)))
#  define BLIT_TY_COLOR_FORM_IS_YUVr(ty)        ((((ty) & (0x1f << 16)) == BLIT_COLOR_FORM_YCBCR888) || (((ty) & (0x1f << 16)) == BLIT_COLOR_FORM_YCBCR422R) || (((ty) & (0x1f << 16)) == BLIT_COLOR_FORM_AYCBCR8888))
#    define BLIT_COLOR_FORM_YCBCR888              (0x10 << 16)
#    define BLIT_COLOR_FORM_YCBCR422R             (0x12 << 16)
#    define BLIT_COLOR_FORM_YCBCR42XMB            (0x14 << 16)
#    define BLIT_COLOR_FORM_AYCBCR8888            (0x15 << 16)
#    define BLIT_COLOR_FORM_YCBCR42XR2B           (0x16 << 16)
#    define BLIT_COLOR_FORM_YCBCR42XMBN           (0x0e << 16)
#    define BLIT_COLOR_FORM_YUV444P               (0x1e << 16)
#  define BLIT_TY_COLOR_FORM_IS_MISC(ty)        (((ty) & (0x18 << 16)) == (0x18 << 16))
#  define BLIT_TY_COLOR_FORM_IS_ALPHA(ty)       (((ty) & (0x1e << 16)) == (0x18 << 16))
#    define BLIT_COLOR_FORM_A1                    (0x18 << 16)
#    define BLIT_COLOR_FORM_A8                    (0x19 << 16)
#    define BLIT_COLOR_FORM_RLD_BD                (0x1a << 16)
#    define BLIT_COLOR_FORM_RLD_H2                (0x1b << 16)
#    define BLIT_COLOR_FORM_RLD_H8                (0x1c << 16)

#define BLIT_TY_FULL_ALPHA_RANGE              (1L << 21)
#define BLIT_TY_ENABLE_DITHER_TTYONLY         (1L << 26)
#define BLIT_TY_COPYDIR_MASK                  (3 << 24)
#  define BLIT_TY_COPYDIR_MASK_VERT               (1 << 25)
#    define BLIT_TY_COPYDIR_TOPBOTTOM             (0 << 25)
#    define BLIT_TY_COPYDIR_BOTTOMTOP             (1 << 25)
#  define BLIT_TY_COPYDIR_MASK_HORI               (1 << 24)
#    define BLIT_TY_COPYDIR_LEFTRIGHT             (0 << 24)
#    define BLIT_TY_COPYDIR_RIGHTLEFT             (1 << 24)

/*
 * NOTE: bit 29 is the opposite way around from the original Gamma blitter
 *       documentation, which appears to be incorrect. It is documented
 *       correctly for the BDisp blitter and works correctly on both.
 */
#define BLIT_TY_COLOR_EXPAND_MASK             (1L << 29)
#define BLIT_TY_COLOR_EXPAND_MSB              (0L << 29)
#define BLIT_TY_COLOR_EXPAND_ZEROS            (1L << 29)
#define BLIT_TY_BIG_ENDIAN                    (1L << 30)


/* the various BLT_xTY registers hold the type and pitch + some other flags
   as can be seen above, add some helper macros to extract useful
   information */
/* these are the necessary bits to describe a pixelformat exactly */
static inline u32
__attribute__((const))
bdisp_ty_get_format_from_ty (u32 ty)
{
  return ty & (BLIT_TY_COLOR_FORM_MASK | BLIT_TY_FULL_ALPHA_RANGE | BLIT_TY_BIG_ENDIAN);
}

/* when switching from blit to draw without any other state changes,
   BLT_TTY might (and will!) still contain the copy direction, we have to
   clear it for normal fills. */
static inline u32
__attribute__((const))
bdisp_ty_sanitise_direction (u32 ty)
{
  return ty & ~BLIT_TY_COPYDIR_MASK;
}

static inline bool
__attribute__((const))
bdisp_ty_formats_identical (u32 ty1, u32 ty2)
{
  return bdisp_ty_get_format_from_ty (ty1) == bdisp_ty_get_format_from_ty (ty2);
}


/*
 * Blit instruction control
 */
#define BLIT_INS_SRC1_MODE_MASK               (0x00000007)
#  define BLIT_INS_SRC1_MODE_DISABLED           (0 << 0)
#  define BLIT_INS_SRC1_MODE_MEMORY             (0x00000001)
#  define BLIT_INS_SRC1_MODE_COLOR_FILL         (0x00000003)
#  define BLIT_INS_SRC1_MODE_DIRECT_COPY        (0x00000004)
#  define BLIT_INS_SRC1_MODE_DIRECT_FILL        (0x00000007)
#define BLIT_INS_SRC2_MODE_MASK               (0x00000018)
#  define BLIT_INS_SRC2_MODE_DISABLED           (0 << 3)
#  define BLIT_INS_SRC2_MODE_MEMORY             (0x00000008)
#  define BLIT_INS_SRC2_MODE_COLOR_FILL         (0x00000018)
#define BLIT_INS_SRC3_MODE_MASK               (0x00000020)
#  define BLIT_INS_SRC3_MODE_DISABLED           (0 << 5)
#  define BLIT_INS_SRC3_MODE_MEMORY             (0x00000020)
#define BLIT_INS_ENABLE_IVMX                  (0x00000040)
#define BLIT_INS_ENABLE_CLUT                  (0x00000080)
#define BLIT_INS_ENABLE_2DRESCALE             (0x00000100)
#define BLIT_INS_ENABLE_FLICKERFILTER         (0x00000200)
#define BLIT_INS_ENABLE_RECTCLIP              (0x00000400)
#define BLIT_INS_ENABLE_COLORKEY              (0x00000800)
#define BLIT_INS_ENABLE_OVMX                  (0x00001000)
#define BLIT_INS_ENABLE_DEI                   (0x00002000)
#define BLIT_INS_ENABLE_PLANEMASK             (0x00004000)
#define BLIT_INS_ENABLE_VC1R                  (0x00020000)
#define BLIT_INS_ENABLE_ROTATION              (0x00040000)
#define BLIT_INS_ENABLE_BLITCOMPIRQ           (0x80000000)


/* CICs */
#define CIC_NODE_COLOR         (1 <<  2)
#define CIC_NODE_SOURCE1       (1 <<  3)
#define CIC_NODE_SOURCE2       (1 <<  4)
#define CIC_NODE_SOURCE3       (1 <<  5)
#ifdef STGFX2_SUPPORT_HW_CLIPPING
  #define CIC_NODE_CLIP          (1 <<  6)
#else
  #define CIC_NODE_CLIP          (0 <<  6)
#endif
#define CIC_NODE_CLUT          (1 <<  7)
#define CIC_NODE_FILTERS       (1 <<  8)
#define CIC_NODE_2DFILTERSCHR  (1 <<  9)
#define CIC_NODE_2DFILTERSLUMA (1 << 10)
#define CIC_NODE_FLICKER       (1 << 11)
#define CIC_NODE_COLORKEY      (1 << 12)
// 13 doesn't exist
#ifdef STGFX2_IMPLEMENT_WAITSERIAL
  #define CIC_NODE_STATIC        (1 << 14)
#else
  #define CIC_NODE_STATIC        (0 << 14)
#endif
#define CIC_NODE_IVMX          (1 << 15)
#define CIC_NODE_OVMX          (1 << 16)
// 17 doesn't exist
#define CIC_NODE_VC1R          (1 << 18)


/* ALU and Colour key control */
#define BLIT_ACK_MODE_MASK                    0x0f
#  define BLIT_ACK_BYPASSSOURCE1                0x00
#  define BLIT_ACK_ROP                          0x01
#  define BLIT_ACK_BLEND_SRC2_N_PREMULT         0x02
#  define BLIT_ACK_BLEND_SRC2_PREMULT           0x03
#  define BLIT_ACK_BLEND_CLIPMASK_LOG_1ST       0x04
#  define BLIT_ACK_BLEND_CLIPMASK_LOG_2ND       0x08
#  define BLIT_ACK_BLEND_CLIPMASK_BLEND         0x05
#  define BLIT_ACK_BYPASSSOURCE2                0x07

#define BLIT_ACK_SWAP_FG_BG                   (0x01 << 4)

#define BLIT_ACK_ROP_SHIFT                    8
#define BLIT_ACK_ROP_MASK                     (0xff << BLIT_ACK_ROP_SHIFT)
#  define BLIT_ACK_ROP_CLEAR                    ( 0 << BLIT_ACK_ROP_SHIFT) /* 0 */
#  define BLIT_ACK_ROP_AND                      ( 1 << BLIT_ACK_ROP_SHIFT) /* S2 & S1 */
#  define BLIT_ACK_ROP_ANDrev                   ( 2 << BLIT_ACK_ROP_SHIFT) /* S2 & !S1 */
#  define BLIT_ACK_ROP_COPY                     ( 3 << BLIT_ACK_ROP_SHIFT) /* S2 */
#  define BLIT_ACK_ROP_ANDinv                   ( 4 << BLIT_ACK_ROP_SHIFT) /* (!S2) & S1*/
#  define BLIT_ACK_ROP_NOOP                     ( 5 << BLIT_ACK_ROP_SHIFT) /* S1 */
#  define BLIT_ACK_ROP_XOR                      ( 6 << BLIT_ACK_ROP_SHIFT) /* S2 ^ S1 */
#  define BLIT_ACK_ROP_OR                       ( 7 << BLIT_ACK_ROP_SHIFT) /* S2 | S1 */
#  define BLIT_ACK_ROP_NOR                      ( 8 << BLIT_ACK_ROP_SHIFT) /* !S2 & !S1 */
#  define BLIT_ACK_ROP_EQUIV                    ( 9 << BLIT_ACK_ROP_SHIFT) /* !S2 ^ S1 */
#  define BLIT_ACK_ROP_INVERT                   (10 << BLIT_ACK_ROP_SHIFT) /* !S1 */
#  define BLIT_ACK_ROP_ORrev                    (11 << BLIT_ACK_ROP_SHIFT) /* S2 | !S1 */
#  define BLIT_ACK_ROP_COPYinv                  (12 << BLIT_ACK_ROP_SHIFT) /* !S2 */
#  define BLIT_ACK_ROP_ORinvert                 (13 << BLIT_ACK_ROP_SHIFT) /* !S2 | S1 */
#  define BLIT_ACK_ROP_NAND                     (14 << BLIT_ACK_ROP_SHIFT) /* !S2 | !S1 */
#  define BLIT_ACK_ROP_SET                      (15 << BLIT_ACK_ROP_SHIFT) /* 1 */

#define BLIT_ACK_GLOBAL_ALPHA_SHIFT           8
#define BLIT_ACK_GLOBAL_ALPHA_MASK            (0xff << BLIT_ACK_GLOBAL_ALPHA_SHIFT)

#define BLIT_ACK_CKEY_MASK                    (0x3f << 16)
#  define BLIT_ACK_CKEY_BLUE_IGNORE             (0x0 << 16)
#  define BLIT_ACK_CKEY_BLUE_ENABLE             (0x1 << 16)
#  define BLIT_ACK_CKEY_BLUE_INV_ENABLE         (0x3 << 16)
#  define BLIT_ACK_CKEY_GREEN_IGNORE            (0x0 << 18)
#  define BLIT_ACK_CKEY_GREEN_ENABLE            (0x1 << 18)
#  define BLIT_ACK_CKEY_GREEN_INV_ENABLE        (0x3 << 18)
#  define BLIT_ACK_CKEY_RED_IGNORE              (0x0 << 20)
#  define BLIT_ACK_CKEY_RED_ENABLE              (0x1 << 20)
#  define BLIT_ACK_CKEY_RED_INV_ENABLE          (0x3 << 20)
#  define BLIT_ACK_CKEY_RGB_ENABLE              (BLIT_ACK_CKEY_BLUE_ENABLE    \
                                                 | BLIT_ACK_CKEY_GREEN_ENABLE \
                                                 | BLIT_ACK_CKEY_RED_ENABLE   \
                                                )
#  define BLIT_ACK_CKEY_RGB_INV_ENABLE          (BLIT_ACK_CKEY_BLUE_INV_ENABLE    \
                                                 | BLIT_ACK_CKEY_GREEN_INV_ENABLE \
                                                 | BLIT_ACK_CKEY_RED_INV_ENABLE   \
                                                )

#define BLIT_ACK_COLORKEYING_MASK               (3 << 22)
#  define BLIT_ACK_COLORKEYING_DEST               (0L << 22)
#  define BLIT_ACK_COLORKEYING_SRC_BEFORE         (1L << 22)
#  define BLIT_ACK_COLORKEYING_SRC_AFTER          (2L << 22)
#  define BLIT_ACK_COLORKEYING_DEST_KEY_ZEROS_SRC_ALPHA  (3L << 22)


/* colorspace conversion and CLUT control */
#define BLIT_CCO_CLUT_NS2_S1_MASK             (1 << 15)
#  define BLIT_CCO_CLUT_NS2_S1_ON_S2            (0 << 15)
#  define BLIT_CCO_CLUT_NS2_S1_ON_S1            (1 << 15)
#define BLIT_CCO_CLUT_MODE_MASK               (3 << 16)
#  define BLIT_CCO_CLUT_EXPAND                  (0 << 16)
#  define BLIT_CCO_CLUT_CORRECT                 (1 << 16)
#define BLIT_CCO_CLUT_UPDATE_EN               (1 << 18)


/* common resize and flicker filter control */
#define BLIT_RZC_2DHF_MODE_MASK               (0x07 << 0)
#  define BLIT_RZC_2DHF_MODE_DISABLED           (0x00 << 0)
#  define BLIT_RZC_2DHF_MODE_RESIZE_ONLY        (0x04 << 0)
#  define BLIT_RZC_2DHF_MODE_FILTER_COLOUR      (0x05 << 0)
#  define BLIT_RZC_2DHF_MODE_FILTER_ALPHA       (0x06 << 0)
#  define BLIT_RZC_2DHF_MODE_FILTER_BOTH        (0x07 << 0)
#define BLIT_RZC_2DVF_MODE_MASK               (0x07 << 4)
#  define BLIT_RZC_2DVF_MODE_DISABLED           (0x00 << 4)
#  define BLIT_RZC_2DVF_MODE_RESIZE_ONLY        (0x04 << 4)
#  define BLIT_RZC_2DVF_MODE_FILTER_COLOUR      (0x05 << 4)
#  define BLIT_RZC_2DVF_MODE_FILTER_ALPHA       (0x06 << 4)
#  define BLIT_RZC_2DVF_MODE_FILTER_BOTH        (0x07 << 4)
#define BLIT_RZC_FF_MODE_MASK                 (0x01 << 8)
#  define BLIT_RZC_FF_MODE_FILTER0              (0x00 << 8)
#  define BLIT_RZC_FF_MODE_ADAPTIVE             (0x01 << 8)
#define BLIT_RZC_Y_2DHF_MODE_MASK             (0x03 << 24)
#  define BLIT_RZC_Y_2DHF_MODE_DISABLED         (0x00 << 24)
#  define BLIT_RZC_Y_2DHF_MODE_RESIZE_ONLY      (0x02 << 24)
#  define BLIT_RZC_Y_2DHF_MODE_FILTER_BOTH      (0x03 << 24)
#define BLIT_RZC_Y_2DVF_MODE_MASK             (0x03 << 28)
#  define BLIT_RZC_Y_2DVF_MODE_DISABLED         (0x00 << 28)
#  define BLIT_RZC_Y_2DVF_MODE_RESIZE_ONLY      (0x02 << 28)
#  define BLIT_RZC_Y_2DVF_MODE_FILTER_BOTH      (0x03 << 28)
#define BLIT_RZC_BOUNDARY_BYPASS              (0x01 << 31)


#define BLIT_RZI_H_INIT_SHIFT           (0)
#define BLIT_RZI_H_INIT_MASK            (0x3FFL << BLIT_RZI_H_INIT_SHIFT)
#define BLIT_RZI_H_REPEAT_SHIFT         (12)
#define BLIT_RZI_H_REPEAT_MASK          (0x7L << BLIT_RZI_H_REPEAT_SHIFT)
#define BLIT_RZI_V_INIT_SHIFT           (16)
#define BLIT_RZI_V_INIT_MASK            (0x3FFL << BLIT_RZI_V_INIT_SHIFT)
#define BLIT_RZI_V_REPEAT_SHIFT         (28)
#define BLIT_RZI_V_REPEAT_MASK          (0x7L << BLIT_RZI_V_REPEAT_SHIFT)


#endif /* __BDISP_REGISTERS_H__ */
