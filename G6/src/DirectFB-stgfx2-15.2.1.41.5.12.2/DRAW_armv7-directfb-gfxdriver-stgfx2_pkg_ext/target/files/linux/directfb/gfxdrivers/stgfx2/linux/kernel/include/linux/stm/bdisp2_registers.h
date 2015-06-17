/*
   ST Microelectronics BDispII driver - register & bit definitions

   (c) Copyright 2007/2008/2011  STMicroelectronics Ltd.

   All rights reserved.
*/

#ifndef __BDISP2_REGISTERS_H__
#define __BDISP2_REGISTERS_H__

#include <linux/types.h>


#define BDISP_CTL      0x00
#  define BDISP_CTL_BIG_NOT_LITTLE    (1 << 29)
#  define BDISP_CTL_STEP_BY_STEP      (1 << 30)
#  define BDISP_CTL_GLOBAL_SOFT_RESET (1 << 31)
#define BDISP_ITS      0x04
#  define BDISP_ITS_AQ_LNA_REACHED    (1 << 0)
#  define BDISP_ITS_AQ_STOPPED        (1 << 1)
#  define BDISP_ITS_AQ_NODE_REPEAT    (1 << 2)
#  define BDISP_ITS_AQ_NODE_NOTIFY    (1 << 3)
#  define BDISP_ITS_AQ_MASK           (0xf)
#  define BDISP_ITS_AQ1_SHIFT         (12)
#  define BDISP_ITS_AQ2_SHIFT         (16)
#  define BDISP_ITS_AQ3_SHIFT         (20)
#  define BDISP_ITS_AQ4_SHIFT         (24)
#define BDISP_STA      0x08
#  define BDISP_STA_IDLE  0x01
#define BDISP_SSBA1    0x10
#define BDISP_SSBA2    0x14
#define BDISP_SSBA3    0x18
#define BDISP_SSBA4    0x1C
#define BDISP_SSBA5    0x20
#define BDISP_SSBA6    0x24
#define BDISP_SSBA7    0x28
#define BDISP_SSBA8    0x2C
#define BDISP_STBA1    0x30
#define BDISP_STBA2    0x34
#define BDISP_STBA3    0x38
#define BDISP_STBA4    0x3C
#define BDISP_CQ1_BASE 0x40
#define BDISP_CQ2_BASE 0x50
/* application queue offsets and defines */
#define BDISP_AQ1_BASE 0x60
#define BDISP_AQ2_BASE 0x70
#define BDISP_AQ3_BASE 0x80
#define BDISP_AQ4_BASE 0x90
#  define BDISP_AQ_CTL                0x0
#    define BDISP_AQ_CTL_PRIORITY_MASK      (0x3 <<  0)
#    define BDISP_AQ_CTL_IRQ_MASK           (0xf << 20)
#      define BDISP_AQ_CTL_IRQ_NODE_REPEAT    (0x1 << 20)
#      define BDISP_AQ_CTL_IRQ_QUEUE_STOPPED  (0x2 << 20)
#      define BDISP_AQ_CTL_IRQ_LNA_REACHED    (0x4 << 20)
#      define BDISP_AQ_CTL_IRQ_NODE_COMPLETED (0x8 << 20)
#    define BDISP_AQ_CTL_EVENT_MASK         (0x7 << 24)
#      define BDISP_AQ_CTL_EVENT_SUSPEND      (0x0 << 24)
#      define BDISP_AQ_CTL_EVENT_ABORT_REPEAT (0x2 << 24)
#      define BDISP_AQ_CTL_EVENT_STOP         (0x4 << 24)
#    define BDISP_AQ_CTL_QUEUE_EN           (0x1 << 31)
#  define BDISP_AQ_IP                 0x4
#  define BDISP_AQ_LNA                0x8
#  define BDISP_AQ_STA                0xc
#define BDISP_SSBA9    0xA0
#define BDISP_SSBA10   0xA4
#define BDISP_SSBA11   0xA8
#define BDISP_SSBA12   0xAC
#define BDISP_SSBA13   0xB0
#define BDISP_SSBA14   0xB4
#define BDISP_SSBA15   0xB8
#define BDISP_SSBA16   0xBC
#define BDISP_SGA1     0xC0
#define BDISP_SGA2     0xC4
#define BDISP_ITM0     0xD0
#define BDISP_ITM1     0xD4
#define BDISP_ITM2     0xD8
#define BDISP_ITM3     0xDC
#define BDISP_PRI      0xF8


/* node registers */
/* CICs - BLT_CIC */
#define BLIT_CIC_NODE_COLOR         (1 <<  2)
#define BLIT_CIC_NODE_SOURCE1       (1 <<  3)
#define BLIT_CIC_NODE_SOURCE2       (1 <<  4)
#define BLIT_CIC_NODE_SOURCE3       (1 <<  5)
#ifdef BDISP2_SUPPORT_HW_CLIPPING
  #define BLIT_CIC_NODE_CLIP        (1 <<  6)
#else
  #define BLIT_CIC_NODE_CLIP        (0 <<  6)
#endif
#define BLIT_CIC_NODE_CLUT          (1 <<  7)
#define BLIT_CIC_NODE_FILTERS       (1 <<  8)
#define BLIT_CIC_NODE_2DFILTERSCHR  (1 <<  9)
#define BLIT_CIC_NODE_2DFILTERSLUMA (1 << 10)
#define BLIT_CIC_NODE_FLICKER       (1 << 11)
#define BLIT_CIC_NODE_COLORKEY      (1 << 12)
/* 13 doesn't exist */
#define BLIT_CIC_NODE_STATIC_real   (1 << 14)
#ifdef BDISP2_IMPLEMENT_WAITSERIAL
  #define BLIT_CIC_NODE_STATIC      BLIT_CIC_NODE_STATIC_real
#else
  #define BLIT_CIC_NODE_STATIC      (0 << 14)
#endif
#define BLIT_CIC_NODE_IVMX          (1 << 15)
#define BLIT_CIC_NODE_OVMX          (1 << 16)
/* 17 doesn't exist */
#define BLIT_CIC_NODE_VC1R          (1 << 18)
#define BLIT_CIC_NODE_GRADIENT_FILL (1 << 19)

/* Blit instruction control - BLT_INS */
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
#define BLIT_INS_ENABLE_GRAD_FILL             (0x00080000)
#define BLIT_INS_ENABLE_BLITCOMPIRQ           (0x80000000)


/* ALU and Colour key control - BLT_ACK */
#define BLIT_ACK_MODE_MASK                    0x0f
#  define BLIT_ACK_BYPASSSOURCE1                0x00
#  define BLIT_ACK_ROP                          0x01
#  define BLIT_ACK_BLEND_SRC2_N_PREMULT         0x02
#  define BLIT_ACK_BLEND_SRC2_PREMULT           0x03
#  define BLIT_ACK_BLEND_CLIPMASK_LOG_1ST       0x04
#  define BLIT_ACK_BLEND_CLIPMASK_LOG_2ND       0x08
#  define BLIT_ACK_BLEND_CLIPMASK_BLEND         0x05
#  define BLIT_ACK_BYPASSSOURCE2                0x07
#  define BLIT_ACK_PORTER_DUFF                  0x0c
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
#define BLIT_ACK_COLORKEYING_MASK             (0x03 << 22)
#  define BLIT_ACK_COLORKEYING_DEST               (0L << 22)
#  define BLIT_ACK_COLORKEYING_SRC_BEFORE         (1L << 22)
#  define BLIT_ACK_COLORKEYING_SRC_AFTER          (2L << 22)
#  define BLIT_ACK_COLORKEYING_DEST_KEY_ZEROS_SRC_ALPHA  (3L << 22)
#define BLIT_ACK_BLEND_MODE_MASK              (0x0f << 24)
#  define BLIT_ACK_BLEND_MODE_CLEAR             (0x00 << 24)
#  define BLIT_ACK_BLEND_MODE_S2                (0x01 << 24)
#  define BLIT_ACK_BLEND_MODE_S1                (0x02 << 24)
#  define BLIT_ACK_BLEND_MODE_S2_OVER_S1        (0x03 << 24)
#  define BLIT_ACK_BLEND_MODE_S1_OVER_S2        (0x04 << 24)
#  define BLIT_ACK_BLEND_MODE_S2_IN_S1          (0x05 << 24)
#  define BLIT_ACK_BLEND_MODE_S1_IN_S2          (0x06 << 24)
#  define BLIT_ACK_BLEND_MODE_S2_OUT_S1         (0x07 << 24)
#  define BLIT_ACK_BLEND_MODE_S1_OUT_S2         (0x08 << 24)
#  define BLIT_ACK_BLEND_MODE_S2_ATOP_S1        (0x09 << 24)
#  define BLIT_ACK_BLEND_MODE_S1_ATOP_S2        (0x0a << 24)
#  define BLIT_ACK_BLEND_MODE_S2_XOR_S1         (0x0b << 24)
#define BLIT_ACK_S2_PREMULT_MASK              (0x01 << 28)
#  define BLIT_ACK_PREMULT_NOTPREMULTIPLIED_S2  (0x00 << 28)
#  define BLIT_ACK_PREMULT_PREMULTIPLIED_S2     (0x01 << 28)
#define BLIT_ACK_S1_PREMULT_MASK              (0x01 << 29)
#  define BLIT_ACK_PREMULT_NOTPREMULTIPLIED_S1  (0x00 << 29)
#  define BLIT_ACK_PREMULT_PREMULTIPLIED_S1     (0x01 << 29)


/* Source/Dest type - BLT_TTY BLT_S1TY BLT_S2TY BLT_S3TY */
#define BLIT_TY_PIXMAP_PITCH_MASK             (0x0000ffff)
#define BLIT_TY_COLOR_FORM_MASK               (0x001f0000)
#  define BLIT_TY_COLOR_FORM_IS_RGB(ty)         (((ty) & (0x18 << 16)) == (0x00 << 16))
#    define BLIT_COLOR_FORM_RGB565                (0x00 << 16)
#    define BLIT_COLOR_FORM_RGB888                (0x01 << 16)
#    define BLIT_COLOR_FORM_RGB888_32             (0x02 << 16)
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
#    define BLIT_COLOR_FORM_BYTE                  (0x1f << 16)
#  define BLIT_TY_COLOR_FORM_IS_2_BUFFER(ty)    ((((ty) & (0x1f << 16)) == BLIT_COLOR_FORM_YCBCR42XMB) || (((ty) & (0x1f << 16)) == BLIT_COLOR_FORM_YCBCR42XR2B) || (((ty) & (0x1f << 16)) == BLIT_COLOR_FORM_YCBCR42XMBN))
#  define BLIT_TY_COLOR_FORM_IS_3_BUFFER(ty)    (((ty) & (0x1f << 16)) == BLIT_COLOR_FORM_YUV444P)
#  define BLIT_TY_COLOR_FORM_IS_UP_SAMPLED(ty)  (BLIT_TY_COLOR_FORM_IS_2_BUFFER(ty) || BLIT_TY_COLOR_FORM_IS_3_BUFFER(ty) || (((ty) & (0x1f << 16)) == BLIT_COLOR_FORM_YCBCR422R))
#define BLIT_TY_FULL_ALPHA_RANGE              (0x01 << 21) /* T, S1 and S2 */
#define BLIT_TTY_CR_NOT_CB                    (0x01 << 22) /* T */
#define BLIT_STY_A1_SUBST                     (0x01 << 22) /* S1 and S2 */
#define BLIT_TY_MB_FIELD                      (0x01 << 23) /* T, S2 and S3 */
#define BLIT_TY_COPYDIR_MASK                  (0x03 << 24)
#  define BLIT_TY_COPYDIR_MASK_HORI               (1 << 24)
#    define BLIT_TY_COPYDIR_LEFTRIGHT             (0 << 24)
#    define BLIT_TY_COPYDIR_RIGHTLEFT             (1 << 24)
#  define BLIT_TY_COPYDIR_MASK_VERT               (1 << 25)
#    define BLIT_TY_COPYDIR_TOPBOTTOM             (0 << 25)
#    define BLIT_TY_COPYDIR_BOTTOMTOP             (1 << 25)
#define BLIT_TTY_ENABLE_DITHER                (0x01 << 26)
#define BLIT_STY_CHROMA_EXTEND                (0x01 << 26) /* S1 and S2 */
#define BLIT_S3TY_BLANK_ACC                   (0x01 << 26) /* S3 */
#define BLIT_TTY_CHROMA_NOTLUMA               (0x01 << 27) /* T */
#define BLIT_STY_SUBBYTE_LSB                  (0x01 << 28) /* S1 and S2 */
#define BLIT_STY_COLOR_EXPAND_MASK            (0x01 << 29) /* S1 and S2 */
#  define BLIT_STY_COLOR_EXPAND_MSB             (0x00 << 29)
#  define BLIT_STY_COLOR_EXPAND_ZEROS           (0x01 << 29)
#define BLIT_TY_BIG_ENDIAN                    (0x01 << 30) /* T,  S1 and S2 */

/* the various BLT_xTY registers hold the type and pitch + some other flags
   as can be seen above, add some helper macros to extract useful
   information */
/* these are the necessary bits to describe a pixelformat exactly */
static inline __u32
__attribute__((const))
bdisp_ty_get_format_from_ty (__u32 ty)
{
  return ty & (BLIT_TY_COLOR_FORM_MASK | BLIT_TY_FULL_ALPHA_RANGE | BLIT_TY_BIG_ENDIAN);
}

/* when switching from blit to draw without any other state changes,
   BLT_TTY might (and will!) still contain the copy direction, we have to
   clear it for normal fills. */
static inline __u32
__attribute__((const))
bdisp_ty_sanitise_direction (__u32 ty)
{
  return ty & ~BLIT_TY_COPYDIR_MASK;
}

static inline bool
__attribute__((const))
bdisp_ty_formats_identical (__u32 ty1, __u32 ty2)
{
  return bdisp_ty_get_format_from_ty (ty1) == bdisp_ty_get_format_from_ty (ty2);
}


/* colorspace conversion and CLUT control - BLT_CCO */
#define BLIT_CCO_GFILL_EN_H_GFILL             (1 <<  0)
#define BLIT_CCO_GFILL_EN_V_GFILL             (1 <<  1)
#define BLIT_CCO_GFILL_NS2_S1                 (1 <<  2)
#define BLIT_CCO_CLUT_NS2_S1_MASK             (1 << 15)
#  define BLIT_CCO_CLUT_NS2_S1_ON_S2            (0 << 15)
#  define BLIT_CCO_CLUT_NS2_S1_ON_S1            (1 << 15)
#define BLIT_CCO_CLUT_MODE_MASK               (3 << 16)
#  define BLIT_CCO_CLUT_EXPAND                  (0 << 16)
#  define BLIT_CCO_CLUT_CORRECT                 (1 << 16)
#define BLIT_CCO_CLUT_UPDATE_EN               (1 << 18)


/* common resize and flicker filter control - BLT_FCTL */
/* chroma 2D filter (horizontal) */
#define BLIT_RZC_HF2D_MODE_MASK               (0x07 << 0)
#  define BLIT_RZC_HF2D_MODE_DISABLED           (0x00 << 0)
#  define BLIT_RZC_HF2D_MODE_RESIZE_ONLY        (0x04 << 0)
#  define BLIT_RZC_HF2D_MODE_FILTER_COLOUR      (0x05 << 0)
#  define BLIT_RZC_HF2D_MODE_FILTER_ALPHA       (0x06 << 0)
#  define BLIT_RZC_HF2D_MODE_FILTER_BOTH        (0x07 << 0)
/* chroma 2D filter (vertical) */
#define BLIT_RZC_VF2D_MODE_MASK               (0x07 << 4)
#  define BLIT_RZC_VF2D_MODE_DISABLED           (0x00 << 4)
#  define BLIT_RZC_VF2D_MODE_RESIZE_ONLY        (0x04 << 4)
#  define BLIT_RZC_VF2D_MODE_FILTER_COLOUR      (0x05 << 4)
#  define BLIT_RZC_VF2D_MODE_FILTER_ALPHA       (0x06 << 4)
#  define BLIT_RZC_VF2D_MODE_FILTER_BOTH        (0x07 << 4)
/* flicker filter */
#define BLIT_RZC_FF_MODE_MASK                 (0x01 << 8)
#  define BLIT_RZC_FF_MODE_FILTER0              (0x00 << 8)
#  define BLIT_RZC_FF_MODE_ADAPTIVE             (0x01 << 8)
#define BLIT_RZC_FF_FRAME2FIELD_MASK          (1 << 10)
#  define BLIT_RZC_FF_FRAME                     (0 << 10)
#  define BLIT_RZC_FF_FIELD                     (1 << 10)
#define BLIT_RZC_FF_NS2_S1_MASK               (1 << 11)
#  define BLIT_RZC_FF_NS2_S1_ON_S2              (0 << 11)
#  define BLIT_RZC_FF_NS2_S1_ON_S1              (1 << 11)
/* alpha border control */
#define BLIT_RZC_AB_MODE_MASK                 (0x0f << 12)
#  define BLIT_RZC_AB_MODE_DISABLED             (0x00 << 12)
#  define BLIT_RZC_AB_MODE_RIGHT                (0x01 << 12)
#  define BLIT_RZC_AB_MODE_LEFT                 (0x02 << 12)
#  define BLIT_RZC_AB_MODE_BOTTOM               (0x04 << 12)
#  define BLIT_RZC_AB_MODE_TOP                  (0x08 << 12)
#  define BLIT_RZC_AB_MODE_LEFT_AND_RIGHT       (0x03 << 12)
#  define BLIT_RZC_AB_MODE_TOP_AND_BOTTOM       (0x0c << 12)
#  define BLIT_RZC_AB_MODE_ALL                  (0x0f << 12)
/* flicker filter repeat line */
#define BLIT_RZC_FF_REPEAT_MASK               (0x03 << 18)
#  define BLIT_RZC_FF_REPEAT_LL                 (0x01 << 18)
#  define BLIT_RZC_FF_REPEAT_FL                 (0x02 << 18)
#  define BLIT_RZC_FF_REPEAT_LL_AND_FL          (0x03 << 18)
/* luma 2D filter (horizontal) */
#define BLIT_RZC_Y_HF2D_MODE_MASK             (0x03 << 24)
#  define BLIT_RZC_Y_HF2D_MODE_DISABLED         (0x00 << 24)
#  define BLIT_RZC_Y_HF2D_MODE_RESIZE_ONLY      (0x02 << 24)
#  define BLIT_RZC_Y_HF2D_MODE_FILTER_BOTH      (0x03 << 24)
/* luma 2D filter (vertical) */
#define BLIT_RZC_Y_VF2D_MODE_MASK             (0x03 << 28)
#  define BLIT_RZC_Y_VF2D_MODE_DISABLED         (0x00 << 28)
#  define BLIT_RZC_Y_VF2D_MODE_RESIZE_ONLY      (0x02 << 28)
#  define BLIT_RZC_Y_VF2D_MODE_FILTER_BOTH      (0x03 << 28)
#define BLIT_RZC_BOUNDARY_BYPASS              (0x01 << 31)


/* resizer initialisation - BLT_RZI BLT_Y_RZI */
#define BLIT_RZI_H_INIT_SHIFT           (0)
#define BLIT_RZI_H_INIT_MASK            (0x3FFL << BLIT_RZI_H_INIT_SHIFT)
#define BLIT_RZI_H_REPEAT_SHIFT         (12)
#define BLIT_RZI_H_REPEAT_MASK          (0x7L << BLIT_RZI_H_REPEAT_SHIFT)
#define BLIT_RZI_V_INIT_SHIFT           (16)
#define BLIT_RZI_V_INIT_MASK            (0x3FFL << BLIT_RZI_V_INIT_SHIFT)
#define BLIT_RZI_V_REPEAT_SHIFT         (28)
#define BLIT_RZI_V_REPEAT_MASK          (0x7L << BLIT_RZI_V_REPEAT_SHIFT)


/* VC1 range register - BLT_VC1R */
#define BLIT_VC1R_LUMA_COEFF_MASK       (0x7 << 0)
#define BLIT_VC1R_LUMA_MAP              (0x1 << 3)
#define BLIT_VC1R_CHROMA_COEFF_MASK     (0x7 << 4)
#define BLIT_VC1R_CHROMA_MAP            (0x1 << 7)


/* gradient fill register - BLT_HGF BLT_VGF in 3.5 notation */
#define BLIT_GF_BLUE_MASK               (0xff <<  0)
#define BLIT_GF_GREEN_MASK              (0xff <<  8)
#define BLIT_GF_RED_MASK                (0xff << 16)
#define BLIT_GF_ALPHA_MASK              (0xff << 24)


#endif /* __BDISP2_REGISTERS_H__ */
