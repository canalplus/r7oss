/*
   ST Microelectronics BDispII driver - state handling

   (c) Copyright 2008-2010  STMicroelectronics Ltd.

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

#include <linux/types.h>

#include "bdisp2/bdisp2_os.h"

#include "bdisp2/bdispII_driver_features.h"

#include "linux/stm/blitter.h"
#include "state.h"
#include "surface.h"
#include "strings.h"

#include "bdisp2/bdisp_tables.h"
#include "bdisp2/bdisp_accel.h"
#include "bdisp2/bdispII_aq_state.h"

#ifdef __KERNEL__
#include <linux/errno.h>
#include <linux/string.h>
#else /* __KERNEL__ */
#include <string.h>
#include <stdint.h>
#include <errno.h>
#endif /* __KERNEL__ */

#include "blit_debug.h"
#include "bdisp2/bdispII_debug.h"

STM_BLITTER_DEBUG_DOMAIN (BDisp_State,      "BDisp/State",      "BDispII state handling");
STM_BLITTER_DEBUG_DOMAIN (BDisp_StateCheck, "BDisp/StateCheck", "BDispII state testing");
STM_BLITTER_DEBUG_DOMAIN (BDisp_StateSet,   "BDisp/StateSet",   "BDispII state setting");


/* state validation flags */
enum {
  DESTINATION        = 0x00000001,
  CLIP               = 0x00000002,
  FILLCOLOR          = 0x00000004,
  BLITCOLOR          = 0x00000008,
  DST_COLORKEY       = 0x00000010,

  SOURCE             = 0x00000020,
  SOURCE_SRC1_MODE   = 0x00000400, /* source 1 mode */
  SRC_COLORKEY       = 0x00000040,
  PALETTE_DRAW       = 0x00000080,
  PALETTE_BLIT       = 0x00000100,

  DRAWFLAGS          = 0x00000200,

  BLITFLAGS_CKEY     = 0x00010000,
  ROTATION           = 0x00020000,
  FILTERING          = 0x00040000,
  BLITFLAGS          = 0x00080000,
  MATRIXCONVERSIONS  = 0x00100000,
  INPUTMATRIX        = 0x00200000,

  ALL                = 0x003f07ff
};

/* state handling macros */
#define BDISP_STATE_VALIDATED(flags)       ( { stdev->v_flags |=  (flags); } )
#define BDISP_STATE_INVALIDATE(flags)      ( { stdev->v_flags &= ~(flags); } )
#define BDISP_STATE_FLAG_NEEDS_VALIDATE(flag)     (!(stdev->v_flags & flag))
#define BDISP_STATE_CHECK_N_VALIDATE(flag) ( {                                                    \
                                             if (BDISP_STATE_FLAG_NEEDS_VALIDATE(flag))           \
                                               bdisp_state_validate_##flag (stdev, state); \
                                           } )
#define BDISP_STATE_CHECK_N_VALIDATE2(flag) ( {                                                   \
                                             if (BDISP_STATE_FLAG_NEEDS_VALIDATE(flag))           \
                                               bdisp_state_validate_##flag (stdrv, stdev, state); \
                                           } )
#define BDISP_STATE_CHECK_N_VALIDATE3(flag) ( {                                                   \
                                             if (BDISP_STATE_FLAG_NEEDS_VALIDATE(flag))           \
                                               bdisp_state_validate_##flag (stdev, state, accel); \
                                           } )


/******** SetState() ********************************************************/
#if defined(BDISP2_DUMP_CHECK_STATE) \
    || defined(BDISP2_DUMP_CHECK_STATE_FAILED) \
    || defined(BDISP2_DUMP_SET_STATE)

static void
_dump_state (const char                     * const prefix,
             const struct stm_blitter_state * const state,
             enum stm_blitter_accel          accel)
{
  char * const accel_str = stm_blitter_get_accel_string (accel);
  char * const modified_str = stm_blitter_get_modified_string (state->modified);
  char * const drawflags_str = stm_blitter_get_drawflags_string (state->drawflags);
  char * const blitflags_str = stm_blitter_get_blitflags_string (state->blitflags);

  stm_blit_printi(
         "%s: accel: %s\n"
          "    state: %p modified: %s, flags draw/blit: %s/%s\n"
          "    pd rule: %u(%s)\n"
          "    format src/dst/src2: '%s'/'%s'/'%s'\n"
          "    color/index[0]: %.2x%.2x%.2x%.2x %.2x\n"
          "    color[1]:       %.2x%.2x%.2x%.2x\n"
          "    colorkey src mode/min/max: 0x%x %.2x%.2x%.2x%.2x %.2x / %.2x%.2x%.2x%.2x %.2x\n"
          "    colorkey dst mode min/max: 0x%x %.2x%.2x%.2x%.2x %.2x / %.2x%.2x%.2x%.2x %.2x\n"
          "    clip: %lld,%lld-%lld,%lld\n",
          prefix,
          accel_str, state, modified_str, drawflags_str, blitflags_str,
          state->pd, stm_blitter_get_porter_duff_name(state->pd),
          state->src ? stm_blitter_get_surface_format_name(state->src->format) : "no source",
          state->dst ? stm_blitter_get_surface_format_name(state->dst->format) : "no dest",
          state->src2 ? stm_blitter_get_surface_format_name(state->src2->format) : "no source2",
          state->colors[0].a, state->colors[0].r, state->colors[0].g, state->colors[0].b, state->color_index,
          state->colors[1].a, state->colors[1].r, state->colors[1].g, state->colors[1].b,
          state->src_ckey_mode,
          state->src_ckey[0].a, state->src_ckey[0].r, state->src_ckey[0].g, state->src_ckey[0].b, state->src_ckey[0].index,
          state->src_ckey[1].a, state->src_ckey[1].r, state->src_ckey[1].g, state->src_ckey[1].b, state->src_ckey[1].index,
          state->dst_ckey_mode,
          state->dst_ckey[0].a, state->dst_ckey[0].r, state->dst_ckey[0].g, state->dst_ckey[0].b, state->dst_ckey[0].index,
          state->dst_ckey[1].a, state->dst_ckey[1].r, state->dst_ckey[1].g, state->dst_ckey[1].b, state->dst_ckey[1].index,
          (long long) state->clip.position1.x, (long long) state->clip.position1.y,
          (long long) state->clip.position2.x, (long long) state->clip.position2.x);

  stm_blitter_free_mem(blitflags_str);
  stm_blitter_free_mem(drawflags_str);
  stm_blitter_free_mem(modified_str);
  stm_blitter_free_mem(accel_str);
}
#endif /* BDISP2_DUMP_CHECK_STATE || BDISP2_DUMP_CHECK_STATE_FAILED || BDISP2_DUMP_SET_STATE */
#ifdef BDISP2_DUMP_CHECK_STATE
  #define _dump_check_state(p, s, a) _dump_state(p, s, a)
#else /* BDISP2_DUMP_CHECK_STATE */
  #define _dump_check_state(p, s, a)
#endif /* BDISP2_DUMP_CHECK_STATE */

#ifdef BDISP2_DUMP_CHECK_STATE_FAILED
  #ifndef BDISP2_DUMP_CHECK_STATE
    #define _dump_check_state_failed(p, s, a) ( { char t[100];               \
                                                  snprintf (t, sizeof (t),   \
                                                            "%s: failed", p);\
                                                  _dump_state(t, s, a);      \
                                              } )
  #else /* BDISP2_DUMP_CHECK_STATE_FAILED */
    #define _dump_check_state_failed(p, s, a) printf("... %s: failed\n", p);
  #endif /* BDISP2_DUMP_CHECK_STATE_FAILED */
#else /* BDISP2_DUMP_CHECK_STATE */
  #define _dump_check_state_failed(p, s, a)
#endif /* BDISP2_DUMP_CHECK_STATE */

#ifdef BDISP2_DUMP_SET_STATE
  #define _dump_set_state(p, s, a) _dump_state(p, s, a)
#else /* BDISP2_DUMP_SET_STATE */
  #define _dump_set_state(p, s, a)
#endif /* BDISP2_DUMP_SET_STATE */




static stm_blitter_porter_duff_rule_t
__attribute__((warn_unused_result))
bdisp2_aq_check_porter_duff (const struct stm_bdisp2_device_data * const stdev,
                             enum stm_blitter_porter_duff_rule_e  pd)
{
  /* the blending maths we can support */
  if (stdev->features.hw.porterduff)
    return 0;

  switch (pd)
    {
    case STM_BLITTER_PD_CLEAR:
    case STM_BLITTER_PD_SOURCE:
    case STM_BLITTER_PD_DEST:
    case STM_BLITTER_PD_SOURCE_OVER:
    case STM_BLITTER_PD_DEST_OVER:
    case STM_BLITTER_PD_DEST_IN:
    case STM_BLITTER_PD_DEST_OUT:
    case STM_BLITTER_PD_NONE:
      return 0;

    case STM_BLITTER_PD_SOURCE_IN:
    case STM_BLITTER_PD_SOURCE_OUT:
    case STM_BLITTER_PD_SOURCE_ATOP:
    case STM_BLITTER_PD_DEST_ATOP:
    case STM_BLITTER_PD_XOR:
    default:
      break;
    }

  return (stm_blitter_porter_duff_rule_t) -EOPNOTSUPP;
}

static bool
__attribute__((warn_unused_result))
bdisp2_aq_multipass_porter_duff_rule (const struct stm_bdisp2_device_data * const stdev,
                                      enum stm_blitter_porter_duff_rule_e  pd)
{
  if (stdev->features.hw.porterduff)
    /* if we have full support for PorterDuff blending in the hardware, we
       only need one node for the operation. */
    return false;

  switch (pd)
    {
    case STM_BLITTER_PD_DEST_IN:
    case STM_BLITTER_PD_DEST_OUT:
    case STM_BLITTER_PD_SOURCE_IN:
    case STM_BLITTER_PD_SOURCE_OUT:
    case STM_BLITTER_PD_SOURCE_ATOP:
    case STM_BLITTER_PD_DEST_ATOP:
      return true;

    case STM_BLITTER_PD_CLEAR:
    case STM_BLITTER_PD_SOURCE:
    case STM_BLITTER_PD_DEST:
    case STM_BLITTER_PD_SOURCE_OVER:
    case STM_BLITTER_PD_DEST_OVER:
    case STM_BLITTER_PD_XOR:
    case STM_BLITTER_PD_NONE:
    default:
      break;
    }

  return false;
}

static bool
__attribute__((warn_unused_result))
bdisp2_aq_porter_duff_rule_needs_lut (const struct stm_bdisp2_device_data * const stdev,
                                      enum stm_blitter_porter_duff_rule_e  pd)
{
  if (stdev->features.hw.porterduff)
    /* No need to use a LUT and multi-pass if the IP has native support. */
    return false;

  switch (pd)
    {
    case STM_BLITTER_PD_CLEAR:
    case STM_BLITTER_PD_SOURCE:
    case STM_BLITTER_PD_DEST:
    case STM_BLITTER_PD_SOURCE_OVER:
    case STM_BLITTER_PD_DEST_OVER:
    case STM_BLITTER_PD_NONE:
      return false;

    case STM_BLITTER_PD_SOURCE_IN:
    case STM_BLITTER_PD_DEST_IN:
    case STM_BLITTER_PD_SOURCE_OUT:
    case STM_BLITTER_PD_DEST_OUT:
    case STM_BLITTER_PD_SOURCE_ATOP:
    case STM_BLITTER_PD_DEST_ATOP:
    case STM_BLITTER_PD_XOR: /* this one is not supported anyway */
    default:
      break;
    }

  return true;
}

static bool
__attribute__((warn_unused_result))
bdisp2_aq_check_need_native_porter_duff (const struct stm_bdisp2_device_data * const stdev,
                                         const struct stm_blitter_state * const state)
{
  if (!stdev->features.hw.porterduff)
    return false;

  if (state->pd == STM_BLITTER_PD_NONE)
    /* PD_NONE is non-native anyway and is only implemented in the
       non-native path */
    return false;

  if ((state->pd == STM_BLITTER_PD_CLEAR
      || state->pd == STM_BLITTER_PD_SOURCE
      || (state->pd == STM_BLITTER_PD_DEST
          && !(state->blitflags & (STM_BLITTER_SBF_DST_PREMULTIPLY
                                   | STM_BLITTER_SBF_DST_COLORKEY))))
          && !stdev->no_blend_optimisation)
    return false;

  return true;
}

bool
bdisp2_aq_check_src_format (const struct stm_bdisp2_device_data * const stdev,
                            stm_blitter_surface_format_t         format)
{
  stm_blitter_surface_format_t    fmt = STM_BLITTER_SF_MASK (format);
  const struct bdisp2_pixelformat_info * const tbl
    = stdev->features.stm_blit_to_bdisp;

  if (format <= STM_BLITTER_SF_INVALID
      || fmt >= STM_BLITTER_SF_COUNT)
    return -EINVAL;

  return tbl[fmt].supported_as_src ? 0 : -EINVAL;
}

static bool
bdisp2_aq_check_dst_format (const struct stm_bdisp2_device_data * const stdev,
                            stm_blitter_surface_format_t         format)
{
  stm_blitter_surface_format_t    fmt = STM_BLITTER_SF_MASK (format);
  const struct bdisp2_pixelformat_info * const tbl
    = stdev->features.stm_blit_to_bdisp;

  if (format <= STM_BLITTER_SF_INVALID
      || fmt >= STM_BLITTER_SF_COUNT)
    return -EINVAL;

  return tbl[fmt].supported_as_dst ? 0 : -EINVAL;
}


/**************************************************************************/
/* Check that the current state is ok for the operation indicated by accel*/
/**************************************************************************/
enum stm_blitter_accel
bdisp2_state_supported (const struct stm_bdisp2_device_data * const stdev,
                        const struct stm_blitter_state      * const state,
                        const enum stm_blitter_accel         accel)
{
  enum stm_blitter_accel ret_accel = (enum stm_blitter_accel) 0;

  _dump_check_state (__func__, state, accel);

  /* return if the desired function is not supported at all. */
  if (accel & ~(STM_BLITTER_ACCEL_FILLRECT | STM_BLITTER_ACCEL_DRAWRECT
                | STM_BLITTER_ACCEL_BLIT | STM_BLITTER_ACCEL_BLIT2
                | STM_BLITTER_ACCEL_STRETCHBLIT))
    goto out;

  if (bdisp2_aq_check_dst_format (stdev, state->dst->format))
    goto out; /* unsupported destination format */

  if (STM_BLITTER_ACCEL_IS_DRAW (accel))
    {
      bool dst_is_ycbcr;

      /* check there are no other drawing flags than those supported */
      if (state->drawflags & ~stdev->features.drawflags)
        goto out;

      /* YUY2 not supported for filling operation as not supported by hardware */
      if (state->dst->format == STM_BLITTER_SF_YUY2)
        goto out;

      /* check blend restrictions */
      if (state->drawflags & STM_BLITTER_SDF_BLEND)
        {
          if (bdisp2_aq_check_porter_duff (stdev, state->pd))
            goto out; /* unsupported porter duff rule */

          if (bdisp2_aq_multipass_porter_duff_rule (stdev, state->pd))
            /* can't support multi node drawing ops */
            goto out;

          if (!stdev->features.hw.rgb32
              && state->dst->format == STM_BLITTER_SF_RGB32)
            {
              if (bdisp2_aq_porter_duff_rule_needs_lut (stdev, state->pd))
                /* we can support a few operations involving RGB32 without much
                   effort. Fortunately, those are the ones that are used by
                   graphics libraries like cairo. */
                goto out;
            }

          /* YCbCr and STM_BLITTER_SDF_SRC_PREMULTIPLY isn't a valid combination,
           * as premultiplication is only defined for RGB */
          if (state->color_colorspace == STM_BLITTER_COLOR_YUV
              && state->drawflags & STM_BLITTER_SDF_SRC_PREMULTIPLY)
            goto out;

          /* DST_PREMULITPLY can only be supported on IPs that natively
             support full PorterDuff blending (but only for real PorterDuff
             modes). */
          if ((state->drawflags & STM_BLITTER_SDF_DST_PREMULTIPLY)
              && (!stdev->features.hw.porterduff
                  || state->pd == STM_BLITTER_PD_NONE))
            goto out;

          /* don't allow blends to LUT surfaces, but we do allow fast fills in
             order to be able to do a "clear" to a specific index. */
          if (state->dst->format & STM_BLITTER_SF_INDEXED)
            goto out;

          /* can not support blend and xor at the same time */
          if (state->drawflags & STM_BLITTER_SDF_XOR)
            goto out;

          /* STM_BLITTER_PD_NONE can only be accelerated when the destination
             does not have an alpha channel, because the correct calculation of
             the destination alpha cannot be achieved in the hardware. We go to
             the trouble of supporting it for pure RGB surfaces as the mode
             is used in the blended fill df_dok performance test. */
          if ((state->dst->format & STM_BLITTER_SF_ALPHA)
              && state->pd == STM_BLITTER_PD_NONE)
            goto out;
        }
      else if (state->drawflags & STM_BLITTER_SDF_DST_PREMULTIPLY)
        {
          /* We only implement support for DST_PREMULITPLY when doing a
             blend, this could be changed if needed (for IPs that
             natively support full PorterDuff blending). */
          goto out;
        }

      /* FIXME: We need to use OVMX instead of IVMX for color conversion
         operations to ABGR destination formats. */
      if ((state->dst->format == STM_BLITTER_SF_ABGR)
          && (state->color_colorspace == STM_BLITTER_COLOR_YUV))
        goto out;

      /* This is an RGB modulation so fail if done on YCbCr. All planar
         formats are also YCbCr formats, but not all YCbCr formats are
         planar formats! */
      dst_is_ycbcr = !!(state->dst->format & STM_BLITTER_SF_YCBCR);
      if (dst_is_ycbcr
          && state->drawflags & STM_BLITTER_SDF_DST_PREMULTIPLY)
        goto out;

      if (state->dst->format & STM_BLITTER_SF_PLANAR)
        {
          /* planar destinations are only supported for simple fill, as for
             read-modify-write to planar surfaces we would either
             1) need all 3 sources, and we wouldn't be able to configure one
             of them for colour fill anymore,
             2) or need 2 sources and the operation would need to be
             configured to using SOURCE2+SOURCE3 for reading the destination,
             and SOURCE1 for the colour. We can't do that for blend, though,
             because SOURCE1 is the background, and SOURCE2(+SOURCE3) the
             foreground, which is the other way around to how we need it (for
             most types of blend). We could swap the PorterDuff rule (SRC <->
             DST) or use the SWAP_FG_BG bit, but this seems too much
             bother!

             Also note that all the set-up code writes the colour to
             SOURCE2 and expects SOURCE1==destination for read-modify-write
             operations. Supporting read-modify-write with planar formats
             would necessitate changing all that as well. */
          if (state->drawflags & STM_BLITTER_SDF_BLEND)
            {
              switch (state->pd)
                {
                case STM_BLITTER_PD_CLEAR:
                case STM_BLITTER_PD_SOURCE:
                case STM_BLITTER_PD_DEST:
                  /* SOURCE and DEST without additional flags and CLEAR are
                  special, as these are simple colour fills. */
                  break;

                case STM_BLITTER_PD_SOURCE_OVER:
                case STM_BLITTER_PD_DEST_OVER:
                case STM_BLITTER_PD_SOURCE_IN:
                case STM_BLITTER_PD_DEST_IN:
                case STM_BLITTER_PD_SOURCE_OUT:
                case STM_BLITTER_PD_DEST_OUT:
                case STM_BLITTER_PD_SOURCE_ATOP:
                case STM_BLITTER_PD_DEST_ATOP:
                case STM_BLITTER_PD_XOR:
                case STM_BLITTER_PD_NONE:
                default:
                  goto out;
                }
            }

          /* the same goes for destination colour keying */
          if (state->drawflags & STM_BLITTER_SDF_DST_COLORKEY)
            goto out;

          /* the same goes for XOR */
          if (state->drawflags & STM_BLITTER_SDF_XOR)
            goto out;
        }

      ret_accel |= STM_BLITTER_ACCEL_FILLRECT | STM_BLITTER_ACCEL_DRAWRECT;
    }
  else if (STM_BLITTER_ACCEL_IS_BLIT (accel))
    {
      bool canUseHWInputMatrix;
      bool isRotate;
      stm_blitter_surface_blitflags_t blitflags_norotate;
      bool src_is_ycbcr, dst_is_ycbcr;
      bool src_premultcolor;
      bool needs_blend;

      /* check there are no other drawing flags than those supported */
      if (state->blitflags & ~stdev->features.blitflags)
        goto out;

      if (bdisp2_aq_check_src_format (stdev, state->src->format))
        goto out; /* unsupported source format */

      if (accel & STM_BLITTER_ACCEL_BLIT2)
      {
          if (bdisp2_aq_check_src_format (stdev, state->src2->format))
            goto out; /* unsupported source format */

          /* Planar formats can never be supported on S1 */
          if (state->src2->format & STM_BLITTER_SF_PLANAR)
            goto out;

          /* FIXME: BLIT2 management should be reworked to use OVMX instead of IVMX.
           * In fact, the current implementation use IVMX that permits color conversion
           * only for S2 and S3.
           * This implmentation doesn't permit to perform color conversion on S1.
           * So, if Src1 colorspace is different to destination colorspace
           * feature should be considered as not supported. Same thing for ABGR destinations
           * that requires a conversion matrix.
           */
          if ((state->src2->colorspace != state->dst->colorspace)
              || (state->dst->format == STM_BLITTER_SF_ABGR))
            goto out;
      }

      isRotate = !!(state->blitflags & (STM_BLITTER_SBF_ROTATE90
                                        | STM_BLITTER_SBF_ROTATE270));
      /* rotation is supported only when no other flag is requested */
      blitflags_norotate = state->blitflags & ~(STM_BLITTER_SBF_ROTATE90
                                                | STM_BLITTER_SBF_ROTATE270);

      /* some pixelformat checks */
      switch (state->src->format)
        {
        case STM_BLITTER_SF_RGB565:
        case STM_BLITTER_SF_RGB24:
        case STM_BLITTER_SF_BGR24:
        case STM_BLITTER_SF_RGB32:
        case STM_BLITTER_SF_ARGB1555:
        case STM_BLITTER_SF_ARGB4444:
        case STM_BLITTER_SF_ARGB8565:
        case STM_BLITTER_SF_AlRGB8565:
        case STM_BLITTER_SF_ARGB:
        case STM_BLITTER_SF_AlRGB:
        case STM_BLITTER_SF_BGRA:
        case STM_BLITTER_SF_BGRAl:
        case STM_BLITTER_SF_ABGR:
        case STM_BLITTER_SF_LUT8:
        case STM_BLITTER_SF_LUT4:
        case STM_BLITTER_SF_LUT2:
        case STM_BLITTER_SF_LUT1:
        case STM_BLITTER_SF_ALUT88:
        case STM_BLITTER_SF_AlLUT88:
        case STM_BLITTER_SF_ALUT44:
        case STM_BLITTER_SF_A8:
        case STM_BLITTER_SF_Al8:
        case STM_BLITTER_SF_A1:
          /* YCbCr formats */
        case STM_BLITTER_SF_AVYU:
        case STM_BLITTER_SF_AlVYU:
        case STM_BLITTER_SF_VYU:
          break;

        case STM_BLITTER_SF_YUY2:
           /* Resize not supported with this color conversion. */
           if ((state->dst->format == STM_BLITTER_SF_UYVY)
               && (accel & STM_BLITTER_ACCEL_STRETCHBLIT))
             goto out;
           break;

        case STM_BLITTER_SF_UYVY:
           /* Resize not supported with this color conversion. */
           if ((state->dst->format == STM_BLITTER_SF_YUY2)
               && (accel & STM_BLITTER_ACCEL_STRETCHBLIT))
             goto out;
           break;

        case STM_BLITTER_SF_YCBCR420MB:
        case STM_BLITTER_SF_YCBCR422MB:
           if ((state->src->size.w & 0xf)
               || (state->src->size.h & 0xf))
             {
               /* must be an even multiple of 16. */
               stm_blit_printw ("%s(): Macroblock surface width and height "
                                "must be multiples of 16. Got %lldx%lld.\n",
                                __func__,
                                (long long) state->src->size.w,
                                (long long) state->src->size.h);
               goto out;
             }
           /* fall through */
        case STM_BLITTER_SF_NV12:
        case STM_BLITTER_SF_NV21:
        case STM_BLITTER_SF_NV16:
        case STM_BLITTER_SF_NV61:
          if (state->blitflags & ~(STM_BLITTER_SBF_ALL_IN_FIXED_POINT
                                   | STM_BLITTER_SBF_DEINTERLACE_TOP
                                   | STM_BLITTER_SBF_DEINTERLACE_BOTTOM
                                   | STM_BLITTER_SBF_STRICT_INPUT_RECT
                                   | STM_BLITTER_SBF_NO_FILTER
                                   | STM_BLITTER_SBF_BLEND_ALPHACHANNEL
                                   | STM_BLITTER_SBF_BLEND_COLORALPHA
                                   | STM_BLITTER_SBF_SRC_PREMULTIPLY
                                   | STM_BLITTER_SBF_SRC_PREMULTCOLOR))
            goto out;

          /* we don't support format conversion to RGB32, support could be
             added, though... */
          if (!stdev->features.hw.rgb32
              && state->dst->format == STM_BLITTER_SF_RGB32)
            goto out;
          break;
        case STM_BLITTER_SF_YV12:
        case STM_BLITTER_SF_I420:
        case STM_BLITTER_SF_YV16:
        case STM_BLITTER_SF_YV61:
        case STM_BLITTER_SF_YCBCR444P:
        case STM_BLITTER_SF_NV24:
          /* we don't (can't) support YUV formats w/ any blitflags set,
             we could support SOURCE colorkeying and SOURCE2 probably */
          if (state->blitflags & ~(STM_BLITTER_SBF_ALL_IN_FIXED_POINT
                                   | STM_BLITTER_SBF_DEINTERLACE_TOP
                                   | STM_BLITTER_SBF_DEINTERLACE_BOTTOM
                                   | STM_BLITTER_SBF_STRICT_INPUT_RECT
                                   | STM_BLITTER_SBF_NO_FILTER))
            goto out;

          /* we don't support format conversion to RGB32, support could be
             added, though... */
          if (!stdev->features.hw.rgb32
              && state->dst->format == STM_BLITTER_SF_RGB32)
            goto out;
          break;

        /* these are not supported via the normal API */
        case STM_BLITTER_SF_RLD_BD:
        case STM_BLITTER_SF_RLD_H2:
        case STM_BLITTER_SF_RLD_H8:

        case STM_BLITTER_SF_INVALID:
        case STM_BLITTER_SF_COUNT:
        default:
          goto out;
        }

      src_is_ycbcr = !!(state->src->format & STM_BLITTER_SF_YCBCR);
      dst_is_ycbcr = !!(state->dst->format & STM_BLITTER_SF_YCBCR);

      canUseHWInputMatrix = (state->src->colorspace == state->dst->colorspace);

      /* We can never support blending or any other blit flags that
         need SOURCE1 with source formats that need all 3 source
         plugs themselves. */
      if ((state->src->format & STM_BLITTER_SF_PLANAR3)
          == STM_BLITTER_SF_PLANAR3)
        {
          if ((state->blitflags & (STM_BLITTER_SBF_DST_COLORKEY
                                   | STM_BLITTER_SBF_XOR
                                   | STM_BLITTER_SBF_BLEND_ALPHACHANNEL
                                   | STM_BLITTER_SBF_DST_PREMULTIPLY
                                   | STM_BLITTER_SBF_READ_SOURCE2))
              || (accel & STM_BLITTER_ACCEL_BLIT2))
            goto out;
        }

      /* deal with a special case of LUT->LUT blits that do not go through a
         colour lookup, but instead do an index translation mapping. Note
         that we do not actually support anything other than a 1:1 mapping
         of the LUT.
         We do support blitting to a LUT surface in case of rotation (see below
         for explanation on rotation). */
      if (state->dst->format & STM_BLITTER_SF_INDEXED)
        {
          /* note that blits are not supported to LUT destinations other than
             in the index translation case. */
          if (state->src->format == state->dst->format
              && !(blitflags_norotate & ~STM_BLITTER_SBF_NO_FILTER))
            {
              ret_accel |= STM_BLITTER_ACCEL_BLIT;
              if (state->blitflags & STM_BLITTER_SBF_NO_FILTER)
                ret_accel |= STM_BLITTER_ACCEL_STRETCHBLIT;
              return ret_accel;
            }

          goto out;
        }

      /* we don't (can't) support 3 plane YUV formats w/ any blitflags set*/
      if ((state->dst->format & STM_BLITTER_SF_PLANAR3)
          == STM_BLITTER_SF_PLANAR3)
        {
          if (state->blitflags & ~(STM_BLITTER_SBF_ALL_IN_FIXED_POINT
                                   | STM_BLITTER_SBF_DEINTERLACE_TOP
                                   | STM_BLITTER_SBF_DEINTERLACE_BOTTOM
                                   | STM_BLITTER_SBF_STRICT_INPUT_RECT
                                   | STM_BLITTER_SBF_NO_FILTER))
            goto out;
        }

      /* rotation is supported only when no other flag is requested */
      if (unlikely (isRotate))
        {
          if ((blitflags_norotate & ~(STM_BLITTER_SBF_NO_FILTER
                                      | STM_BLITTER_SBF_STRICT_INPUT_RECT
                                      | STM_BLITTER_SBF_ANTIALIAS))
              || !canUseHWInputMatrix)
            goto out;

          if ((state->src->format & STM_BLITTER_SF_PLANAR)
              || (state->dst->format & STM_BLITTER_SF_PLANAR)
              || (state->src->format == STM_BLITTER_SF_UYVY)
              || (state->dst->format == STM_BLITTER_SF_UYVY)
              || (state->src->format == STM_BLITTER_SF_YUY2)
              || (state->dst->format == STM_BLITTER_SF_YUY2))
                goto out;

          if ((state->src->format & STM_BLITTER_SF_INDEXED)
              && state->src->format != state->dst->format)
            /* this is different to the test above in that we deny LUT->(A)RGB
               expansion! */
            goto out;

          ret_accel |= STM_BLITTER_ACCEL_BLIT;
          return ret_accel;
        }

      /* FIXME: FF with BLIT2 operation should be supported but it wasn't
         verified */
     if ((state->blitflags & STM_BLITTER_SBF_FLICKER_FILTER)
         && (accel & (STM_BLITTER_ACCEL_STRETCHBLIT
                      | STM_BLITTER_ACCEL_BLIT2)))
        /* FF and Vertical resize are in mutual exclusion (Hw limitation).
           So, using FF with STRETCHBLIT will be considered as not supported */
        goto out;

     if ((state->blitflags & STM_BLITTER_SBF_FLICKER_FILTER)
         && (state->src->format & STM_BLITTER_SF_INDEXED))
        /* FIXME: FF with LUT src formats doesn't work correctly.
           In fact, color expansion was not performed during blit ops*/
        goto out;

      /* can not accelerate simultaneous source and destination colour key */
      if ((state->blitflags & (STM_BLITTER_SBF_SRC_COLORKEY
                               | STM_BLITTER_SBF_DST_COLORKEY))
          == (STM_BLITTER_SBF_SRC_COLORKEY | STM_BLITTER_SBF_DST_COLORKEY))
        goto out;

      if (state->blitflags & STM_BLITTER_SBF_SRC_COLORKEY)
        {
          /*application of SRC_COLORKEY on CLUT sources for PLANAR destinations
            formats is not supported. Chroma will be shifted compared
            to Luma due to hardware limitation */
          if ((state->src->format & STM_BLITTER_SF_INDEXED)
              && (state->dst->format & STM_BLITTER_SF_PLANAR))
            goto out;
        }

      src_premultcolor = !!(state->blitflags
                            & STM_BLITTER_SBF_SRC_PREMULTCOLOR);
      needs_blend = false;

      if (state->blitflags & (STM_BLITTER_SBF_BLEND_ALPHACHANNEL
                              | STM_BLITTER_SBF_BLEND_COLORALPHA))
        {
          bool src_premultiply;

          if (bdisp2_aq_check_porter_duff (stdev, state->pd))
            goto out; /* unsupported porter duff rule */

          /* we cannot blend and XOR at the same time, what a silly idea */
          if (state->blitflags & STM_BLITTER_SBF_XOR)
            /* XOR needs the ROP unit and blending needs the blend unit - they
               can not be used simultaneously. */
            goto out;

          if (!stdev->features.hw.rgb32
              && state->dst->format == STM_BLITTER_SF_RGB32)
            {
              if (state->src->format == STM_BLITTER_SF_RGB32
                  || (state->src->format & STM_BLITTER_SF_INDEXED))
                /* can't do this for:
                   RGB32 because we can only use the LUT to correct the alpha
                         either on S1 or S2, but not on both.
                   INDEXED because we only have one LUT, either to use for color
                           lookup or color correction */
                goto out;
            }

          /* blend operations are not supported with planar destinations */
          if (state->dst->format & STM_BLITTER_SF_PLANAR)
            goto out;

          if (!bdisp2_aq_check_need_native_porter_duff(stdev, state))
            {
              src_premultiply = !!(state->blitflags
                                   & STM_BLITTER_SBF_SRC_PREMULTIPLY);

              switch (state->blitflags & (STM_BLITTER_SBF_BLEND_ALPHACHANNEL
                                          | STM_BLITTER_SBF_BLEND_COLORALPHA))
                {
                default:
                case STM_BLITTER_SBF_NONE:
                case STM_BLITTER_SBF_BLEND_ALPHACHANNEL:
                  break;

                case STM_BLITTER_SBF_BLEND_COLORALPHA:
                  switch (state->pd)
                    {
                    case STM_BLITTER_PD_DEST:
                      if (state->blitflags & STM_BLITTER_SBF_DST_PREMULTIPLY)
                        break;
                      /* fall through */

                    case STM_BLITTER_PD_CLEAR:
                      break;

                    case STM_BLITTER_PD_SOURCE:
                    case STM_BLITTER_PD_SOURCE_OVER:
                      switch (state->blitflags & (STM_BLITTER_SBF_SRC_PREMULTCOLOR
                                                  | STM_BLITTER_SBF_SRC_PREMULTIPLY))
                        {
                        case STM_BLITTER_SBF_NONE:
                          break;

                        case (STM_BLITTER_SBF_SRC_PREMULTCOLOR
                              | STM_BLITTER_SBF_SRC_PREMULTIPLY):
                          if (!canUseHWInputMatrix)
                            break;
                          /* fall through */

                        case STM_BLITTER_SBF_SRC_PREMULTCOLOR:
                          if (!src_premultiply)
                            src_premultcolor = false;
                          /* fall through */

                        case STM_BLITTER_SBF_SRC_PREMULTIPLY:
                          needs_blend = true;
                          src_premultiply = false;
                          break;
                        }
                      break;

                    case STM_BLITTER_PD_DEST_OVER:
                    case STM_BLITTER_PD_DEST_IN:
                    case STM_BLITTER_PD_DEST_OUT:
                    case STM_BLITTER_PD_NONE:
                      break;

                    case STM_BLITTER_PD_SOURCE_IN:
                    case STM_BLITTER_PD_SOURCE_OUT:
                    case STM_BLITTER_PD_SOURCE_ATOP:
                    case STM_BLITTER_PD_DEST_ATOP:
                    case STM_BLITTER_PD_XOR:
                    default:
                      /* not reached */
                      goto out;
                    }
                  break;

                case (STM_BLITTER_SBF_BLEND_ALPHACHANNEL
                      | STM_BLITTER_SBF_BLEND_COLORALPHA):
                  switch (state->pd)
                    {
                    case STM_BLITTER_PD_DEST_OVER:
                    case STM_BLITTER_PD_DEST_IN:
                    case STM_BLITTER_PD_DEST_OUT:
                      break;

                    case STM_BLITTER_PD_DEST:
                    case STM_BLITTER_PD_CLEAR:
                      /* no ops. */
                      break;

                    case STM_BLITTER_PD_SOURCE:
                    case STM_BLITTER_PD_SOURCE_OVER:
                      if (!(state->blitflags & (STM_BLITTER_SBF_SRC_PREMULTCOLOR
                                                | STM_BLITTER_SBF_SRC_PREMULTIPLY)))
                        break;
                      /* fall through */

                    case STM_BLITTER_PD_NONE:
                      if ((state->blitflags & STM_BLITTER_SBF_SRC_PREMULTCOLOR)
                          && !canUseHWInputMatrix)
                        {
                          if ((state->blitflags & STM_BLITTER_SBF_SRC_PREMULTIPLY)
                              || state->pd == STM_BLITTER_PD_NONE)
                            break;
                        }

                      needs_blend = true;
                      if (src_premultcolor
                          && !src_premultiply
                          && state->pd != STM_BLITTER_PD_NONE)
                        {
                          src_premultcolor = false;
                        }
                      break;

                    case STM_BLITTER_PD_SOURCE_IN:
                    case STM_BLITTER_PD_SOURCE_OUT:
                    case STM_BLITTER_PD_SOURCE_ATOP:
                    case STM_BLITTER_PD_DEST_ATOP:
                    case STM_BLITTER_PD_XOR:
                    default:
                      /* not reached */
                      goto out;
                    }
                  break;
                }

              switch (state->pd)
                {
                case STM_BLITTER_PD_SOURCE:
                  if (src_premultiply)
                    {
                      needs_blend = true;
                      if (state->blitflags & (STM_BLITTER_SBF_DST_COLORKEY
                                              | STM_BLITTER_SBF_READ_SOURCE2))
                        /* the SRC_PREMULTIPLY is done using a blend with 0.0.0.0
                           coming from S1 color fill, so we can not DST_COLORKEY at
                           the same time */
                        goto out;
                    }
                  /* fall through */

                case STM_BLITTER_PD_DEST:
                  if (state->blitflags & STM_BLITTER_SBF_DST_PREMULTIPLY)
                    needs_blend = true;
                  break;

                case STM_BLITTER_PD_SOURCE_OVER:
                  if (state->blitflags & STM_BLITTER_SBF_DST_PREMULTIPLY)
                    goto out;
                  needs_blend = true;
                  break;

                case STM_BLITTER_PD_DEST_OVER:
                  if (state->blitflags & STM_BLITTER_SBF_SRC_PREMULTIPLY)
                    /* in this case we end up with a destination premultiply, because
                       we swap FG/BG in the ALU, whereas DirectFB (correctly) still
                       does a source premultiply.
                       FIXME: this probably means we could support
                       (STM_BLITTER_SBF_DST_PREMULTIPLY
                        | STM_BLITTER_SBF_DST_PREMULTCOLOR in this case. */
                    goto out;
                  needs_blend = true;
                  break;

                case STM_BLITTER_PD_DEST_IN:
                case STM_BLITTER_PD_DEST_OUT:
                  if (state->blitflags & STM_BLITTER_SBF_DST_PREMULTIPLY)
                    goto out;
                  needs_blend = true;
                  break;

                case STM_BLITTER_PD_NONE:
                  if (state->blitflags & STM_BLITTER_SBF_SRC_PREMULTIPLY)
                    /* We can only do source premultiplication if the
                       src func != DSBF_SRCALPHA.
                       We only have one premultiplication unit, and need it for
                       DSBF_SRCALPHA already. */
                    goto out;
                  /* We can do src func = DSBF_SRCALPHA as long as the destination
                     does not have an alpha channel.

                     This is because the hardware cannot calculate the correct alpha
                     for the destination but can calculate the correct RGB values.
                     This is supported to accelerate the blit-blend performance test
                     in df_dok for the standard 16bit destination. */
                  if (state->dst->format & STM_BLITTER_SF_ALPHA)
                    goto out;
                  needs_blend = true;
                  break;

                case STM_BLITTER_PD_CLEAR:
                  if (state->blitflags & STM_BLITTER_SBF_DST_PREMULTIPLY)
                    goto out;
                  break;

                case STM_BLITTER_PD_SOURCE_IN:
                case STM_BLITTER_PD_SOURCE_OUT:
                case STM_BLITTER_PD_SOURCE_ATOP:
                case STM_BLITTER_PD_DEST_ATOP:
                case STM_BLITTER_PD_XOR:
                default:
                  /* not reached */
                  goto out;
                }
            }
          else
            {
              if ((state->blitflags & STM_BLITTER_SBF_SRC_PREMULTCOLOR)
                  && !canUseHWInputMatrix)
                goto out;
            }
        }
      else /* not a blend operation */
        {
          if (state->blitflags & STM_BLITTER_SBF_SRC_PREMULTIPLY)
            {
              /* 1) we cannot XOR and premultiply by src alpha at the same time
                 2) we cannot DST colourkey and SRC premultiply at the same time
                 3) we cannot DST and SRC premultiply at the same time.

                 XOR needs the ROP unit and premultiply the blend unit - they
                 can not be used simultaneously.
                 We do the SRC_PREMULTIPLY using a blend with 0.0.0.0 coming
                 from S1 color fill, so we can not read the destination.
                 SRC and DST premultiply are exclusive - we can only premultiply
                 on S2, which can be either SRC or DST, depending on
                 BLIT_ACK_SWAP_FG_BG. */
              if (state->blitflags & (STM_BLITTER_SBF_XOR
                                      | STM_BLITTER_SBF_DST_COLORKEY
                                      | STM_BLITTER_SBF_DST_PREMULTIPLY
                                      | STM_BLITTER_SBF_READ_SOURCE2))
                goto out;

              needs_blend = true;
            }
        }

      /* colorize operation with YCbCr colors is not supported */
      if ((state->blitflags & STM_BLITTER_SBF_SRC_COLORIZE)
          && (state->color_colorspace == STM_BLITTER_COLOR_YUV))
        goto out;

      if ((state->blitflags & STM_BLITTER_SBF_SRC_COLORIZE)
          || src_premultcolor)
        {
          /* this works only if formats are equal or if we don't convert
             YCbCr <-> RGB, as we need the input matrix on the BDisp to do
             premultcolor / colorize. */
          if (!src_is_ycbcr && dst_is_ycbcr)
            {
              /* We can still do this if the target is YUV, by doing the
                 colourspace conversion in the output matrix, but only if the
                 ALU is not being used to blend or xor source and destination.
                 That needs the conversion to be done in the input matrix, but
                 we want to use that matrix to do the RGB modulation, we can't
                 do both! */
              if ((state->blitflags & STM_BLITTER_SBF_XOR)
                  || needs_blend)
                goto out;
            }
        }

      /* De-Interlacing is not supported simultaneously on TOP and BOTTOM */
      if ((state->blitflags & (STM_BLITTER_SBF_DEINTERLACE_TOP
                               | STM_BLITTER_SBF_DEINTERLACE_BOTTOM))
          == (STM_BLITTER_SBF_DEINTERLACE_TOP | STM_BLITTER_SBF_DEINTERLACE_BOTTOM))
        goto out;

      ret_accel |= (STM_BLITTER_ACCEL_BLIT
                    | STM_BLITTER_ACCEL_BLIT2
                    | STM_BLITTER_ACCEL_STRETCHBLIT);
    }

  return ret_accel;

out:
  _dump_check_state_failed (__func__, state, accel);

  return (enum stm_blitter_accel) 0;
}



/******** SetState() related things -> helpers ******************************/
int
_bdisp_state_set_buffer_type (const struct stm_bdisp2_device_data * const stdev,
                              uint32_t                            * const ty,
                              stm_blitter_surface_format_t         format,
                              uint16_t                             pitch)
{
  stm_blitter_surface_format_t    fmt = STM_BLITTER_SF_MASK (format);
  const struct bdisp2_pixelformat_info * const tbl
    = stdev->features.stm_blit_to_bdisp;

  stm_blit_printd (BDisp_State,
                   "  -> %s format: %s pitch: %hu\n", __func__,
                   stm_blitter_get_surface_format_name (format), pitch);

  if (format <= STM_BLITTER_SF_INVALID
      || fmt >= STM_BLITTER_SF_COUNT)
    return -EINVAL;

  *ty = tbl[fmt].bdisp_type;
  switch (*ty)
    {
    case -1:
      STM_BLITTER_ASSERT ("BDisp/BLT: _bdisp_state_set_buffer_type() "
                          "unsupported pixelformat\n" == NULL);
      return -EINVAL;

    default:
      *ty |= pitch;
      break;
    }

  return 0;
}

static void
_bdisp_state_set_key_color (stm_blitter_surface_format_t    format,
                            const stm_blitter_color_t      * const key,
                            uint32_t                       * const reg)
{
  unsigned long tmp;

  stm_blit_printd (BDisp_State, "  -> %s colorkey a rgb ycbcr idx: %.2x %.2x%.2x%.2x %.2x%.2x%.2x %.2x\n",
                   __func__, key->a, key->r, key->g, key->b,
                   key->y, key->cb, key->cr, key->index);

  /* convert colorkey into a 32bit RGB as required by the hardware. We do
     this by padding the missing LSBs with the MSBs to match the hardware
     expansion mode we use for subbyte color formats. */
  switch (format)
  {
    /* these are not supported via the normal API */
    case STM_BLITTER_SF_RLD_BD:
    case STM_BLITTER_SF_RLD_H2:
    case STM_BLITTER_SF_RLD_H8:
    case STM_BLITTER_SF_INVALID:
    case STM_BLITTER_SF_COUNT:
    default:
      STM_BLITTER_ASSERT ("BDisp/BLT: bdisp_aq_SetKeyColor(): unknown"
                          " pixelformat?!\n" == NULL);
      /* fall through */
    case STM_BLITTER_SF_RGB565:
    case STM_BLITTER_SF_RGB24:
    case STM_BLITTER_SF_RGB32:
    case STM_BLITTER_SF_ARGB1555:
    case STM_BLITTER_SF_ARGB4444:
    case STM_BLITTER_SF_ARGB8565:
    case STM_BLITTER_SF_AlRGB8565:
    case STM_BLITTER_SF_ARGB:
    case STM_BLITTER_SF_AlRGB:
      tmp = (key->r << 16) | (key->g << 8) | key->b;
      break;

    case STM_BLITTER_SF_BGR24:
    case STM_BLITTER_SF_BGRA:
    case STM_BLITTER_SF_BGRAl:
      tmp = (key->b << 16) | (key->g << 8) | key->r;
      break;

    case STM_BLITTER_SF_ABGR:
      tmp = (key->b << 16) | (key->g << 8) | key->r;
      break;

    case STM_BLITTER_SF_A8:
    case STM_BLITTER_SF_Al8:
    case STM_BLITTER_SF_A1:
      /* fixme: don't know! */

    case STM_BLITTER_SF_LUT8:
    case STM_BLITTER_SF_LUT4:
    case STM_BLITTER_SF_LUT2:
    case STM_BLITTER_SF_LUT1:
    case STM_BLITTER_SF_ALUT88:
    case STM_BLITTER_SF_AlLUT88:
    case STM_BLITTER_SF_ALUT44:
      tmp = key->index;
      break;

    /* YCbCr formats */
    case STM_BLITTER_SF_AVYU:
    case STM_BLITTER_SF_AlVYU:
    case STM_BLITTER_SF_VYU:
    case STM_BLITTER_SF_YUY2:
    case STM_BLITTER_SF_UYVY:
    case STM_BLITTER_SF_YV12:
    case STM_BLITTER_SF_I420:
    case STM_BLITTER_SF_YV16:
    case STM_BLITTER_SF_YV61:
    case STM_BLITTER_SF_YCBCR444P:
    case STM_BLITTER_SF_NV12:
    case STM_BLITTER_SF_NV21:
    case STM_BLITTER_SF_NV16:
    case STM_BLITTER_SF_NV61:
    case STM_BLITTER_SF_YCBCR420MB:
    case STM_BLITTER_SF_YCBCR422MB:
    case STM_BLITTER_SF_NV24:
      /* fixme: don't know */
      tmp = (key->y << 16) | (key->cb << 8) | key->cr;
      break;
  }

  /* preserve the alpha in the destination as it may be significant for 1bit
     alpha modes. */
  *reg = (*reg & 0xff000000) | tmp;

  if (tmp != *reg)
    stm_blit_printd (BDisp_State, "    -> %.8x\n", *reg);
}

static void
bdisp_state_validate_PALETTE_DRAW (struct stm_bdisp2_device_data  * const stdev,
                                   const struct stm_blitter_state * const state)
{
  stm_blit_printd (BDisp_State, "%s\n", __func__);

  if (!stdev->features.hw.rgb32
      && state->dst->format == STM_BLITTER_SF_RGB32)
    {
      /* need a palette to make RGB32 usable */
      stdev->setup->drawstate.ConfigGeneral.BLT_CIC |= BLIT_CIC_NODE_CLUT;
      stdev->setup->drawstate.ConfigGeneral.BLT_INS |= BLIT_INS_ENABLE_CLUT;
      /* use a palette to 'correct' all x in xRGB32 to 0xff */
      stdev->setup->drawstate.ConfigClut.BLT_CCO = BLIT_CCO_CLUT_CORRECT;
      /* palette works on source 1, as we read from there */
      stdev->setup->drawstate.ConfigClut.BLT_CCO |= BLIT_CCO_CLUT_NS2_S1_ON_S1;
      stdev->setup->drawstate.ConfigClut.BLT_CCO |= BLIT_CCO_CLUT_UPDATE_EN;
      stdev->setup->drawstate.ConfigClut.BLT_CML
        = stdev->clut_phys[SG2C_ONEALPHA_RGB];
    }
  else
    {
      stdev->setup->drawstate.ConfigGeneral.BLT_CIC &= ~BLIT_CIC_NODE_CLUT;
      stdev->setup->drawstate.ConfigGeneral.BLT_INS &= ~BLIT_INS_ENABLE_CLUT;
    }

  /* done */
  BDISP_STATE_VALIDATED (PALETTE_DRAW);
}

/*********************************************************************/
/* Store the destination surface state in the device state structure */
/*********************************************************************/

#define set_matrix(dst, src) \
  ({ \
    dst ## 0 = src[0]; \
    dst ## 1 = src[1]; \
    dst ## 2 = src[2]; \
    dst ## 3 = src[3]; \
  })

static void
bdisp_state_validate_DESTINATION (struct stm_bdisp2_device_data  * const stdev,
                                  const struct stm_blitter_state * const state)
{
  unsigned int pitch_adjustment = 1;
  uint32_t blt_cic = stdev->setup->ConfigGeneral.BLT_CIC;
  uint32_t blt_ins = stdev->setup->ConfigGeneral.BLT_INS;

  stm_blit_printd (BDisp_State,
                   "%s phys+offset: %.8lx\n",
                   __func__, state->dst->buffer_address.base);

  _bdisp_state_set_buffer_type (stdev,
                                &stdev->setup->ConfigTarget.BLT_TTY,
                                state->dst->format,
                                state->dst->stride);

  blt_cic &= ~BLIT_CIC_NODE_OVMX;
  blt_ins &= ~BLIT_INS_ENABLE_OVMX;

  stdev->setup->all_states.extra_blt_ins = 0;
  stdev->setup->all_states.extra_blt_cic = 0;

  stdev->setup->upsampled_dst_nbpix_min = stdev->features.hw.upsampled_nbpix_min;
  if (state->dst->format & STM_BLITTER_SF_PLANAR)
    stdev->setup->upsampled_dst_nbpix_min = stdev->features.hw.upsampled_nbpix_min*2;

  stdev->setup->n_target_extra_loops = 0;
  if (state->dst->format & STM_BLITTER_SF_PLANAR)
    {
      pitch_adjustment = 1;
      stdev->setup->target_loop[0].coordinate_divider.h
        = stdev->setup->target_loop[0].coordinate_divider.v
        = stdev->setup->target_loop[1].coordinate_divider.h
        = stdev->setup->target_loop[1].coordinate_divider.v
        = 1;
    }

  stdev->setup->ConfigTarget.BLT_TBA = state->dst->buffer_address.base;
  switch (state->dst->format)
    {
    case STM_BLITTER_SF_YV12:
    case STM_BLITTER_SF_I420:
      stdev->setup->target_loop[0].coordinate_divider.v = 2;
      stdev->setup->target_loop[1].coordinate_divider.v = 2;
      /* fall through */
    case STM_BLITTER_SF_YV16:
    case STM_BLITTER_SF_YV61:
      stdev->setup->target_loop[0].coordinate_divider.h = 2;
      stdev->setup->target_loop[1].coordinate_divider.h = 2;
      pitch_adjustment = 2;
      /* fall through */
    case STM_BLITTER_SF_YCBCR444P:
      {
      struct target_loop *target_loop;

      stdev->setup->n_target_extra_loops = 2;
      /* Cb */
      target_loop = &stdev->setup->target_loop[0];
      target_loop->BLT_TBA = (state->dst->buffer_address.base
                              + state->dst->buffer_address.cb_offset);
      _bdisp_state_set_buffer_type (stdev,
                                    &target_loop->BLT_TTY,
                                    state->dst->format,
                                    state->dst->stride / pitch_adjustment);
      target_loop->BLT_TTY |= BLIT_TTY_CHROMA_NOTLUMA;
      /* Cr */
      target_loop = &stdev->setup->target_loop[1];
      target_loop->BLT_TBA = (state->dst->buffer_address.base
                              + state->dst->buffer_address.cr_offset);
      _bdisp_state_set_buffer_type (stdev,
                                    &target_loop->BLT_TTY,
                                    state->dst->format,
                                    state->dst->stride / pitch_adjustment);
      target_loop->BLT_TTY |= BLIT_TTY_CHROMA_NOTLUMA | BLIT_TTY_CR_NOT_CB;
      }
      break;

    case STM_BLITTER_SF_NV12:
    case STM_BLITTER_SF_NV21:
    case STM_BLITTER_SF_YCBCR420MB:
      stdev->setup->target_loop[0].coordinate_divider.v = 2;
      /* fall through */
    case STM_BLITTER_SF_NV16:
    case STM_BLITTER_SF_NV61:
    case STM_BLITTER_SF_YCBCR422MB:
      stdev->setup->target_loop[0].coordinate_divider.h = 2;
      pitch_adjustment = 1;
      /* fall through */
    case STM_BLITTER_SF_NV24:
      {
      struct target_loop *target_loop;

      if (state->dst->format == STM_BLITTER_SF_NV24)
        pitch_adjustment = 2;

      stdev->setup->n_target_extra_loops = 1;
      /* Cb/Cr */
      target_loop = &stdev->setup->target_loop[0];
      target_loop->BLT_TBA = (state->dst->buffer_address.base
                              + state->dst->buffer_address.cbcr_offset);
      _bdisp_state_set_buffer_type (stdev,
                                    &target_loop->BLT_TTY,
                                    state->dst->format,
                                    state->dst->stride * pitch_adjustment);
      target_loop->BLT_TTY |= BLIT_TTY_CHROMA_NOTLUMA;
      }
      break;

    case STM_BLITTER_SF_RGB32:
      if (!stdev->features.hw.rgb32)
        {
          stdev->setup->all_states.extra_blt_ins = BLIT_INS_ENABLE_PLANEMASK;
          stdev->setup->all_states.extra_blt_cic = BLIT_CIC_NODE_FILTERS;
        }
      break;

    case STM_BLITTER_SF_ARGB8565:
    case STM_BLITTER_SF_AlRGB8565:
    case STM_BLITTER_SF_RGB565:
      /* workarround for rounding issue */
      set_matrix (stdev->setup->ConfigOVMX.BLT_OVMX, bdisp_aq_RGB565);
      blt_cic |= BLIT_CIC_NODE_OVMX;
      blt_ins |= BLIT_INS_ENABLE_OVMX;
      break;

    case STM_BLITTER_SF_ARGB1555:
      /* workarround for rounding issue */
      set_matrix (stdev->setup->ConfigOVMX.BLT_OVMX, bdisp_aq_RGB555);
      blt_cic |= BLIT_CIC_NODE_OVMX;
      blt_ins |= BLIT_INS_ENABLE_OVMX;
      break;

    case STM_BLITTER_SF_ARGB4444:
      /* workarround for rounding issue */
      set_matrix (stdev->setup->ConfigOVMX.BLT_OVMX, bdisp_aq_RGB444);
      blt_cic |= BLIT_CIC_NODE_OVMX;
      blt_ins |= BLIT_INS_ENABLE_OVMX;
      break;

    case STM_BLITTER_SF_RGB24:
    case STM_BLITTER_SF_BGR24:
    case STM_BLITTER_SF_ARGB:
    case STM_BLITTER_SF_AlRGB:
    case STM_BLITTER_SF_BGRA:
    case STM_BLITTER_SF_BGRAl:
    case STM_BLITTER_SF_ABGR:
    case STM_BLITTER_SF_LUT8:
    case STM_BLITTER_SF_LUT4:
    case STM_BLITTER_SF_LUT2:
    case STM_BLITTER_SF_LUT1:
    case STM_BLITTER_SF_ALUT88:
    case STM_BLITTER_SF_AlLUT88:
    case STM_BLITTER_SF_ALUT44:
    case STM_BLITTER_SF_A8:
    case STM_BLITTER_SF_Al8:
    case STM_BLITTER_SF_A1:
    case STM_BLITTER_SF_AVYU:
    case STM_BLITTER_SF_AlVYU:
    case STM_BLITTER_SF_VYU:
    case STM_BLITTER_SF_YUY2:
    case STM_BLITTER_SF_UYVY:
      break;

    /* these are not supported via the normal API */
    case STM_BLITTER_SF_RLD_BD:
    case STM_BLITTER_SF_RLD_H2:
    case STM_BLITTER_SF_RLD_H8:
    case STM_BLITTER_SF_INVALID:
    case STM_BLITTER_SF_COUNT:
    default:
      /* not reached */
      STM_BLITTER_ASSERT ("should not be reached" == NULL);
      break;
    }

  stdev->setup->ConfigGeneral.BLT_CIC = blt_cic;
  stdev->setup->ConfigGeneral.BLT_INS = blt_ins;

  /* need to invalidate the source flag, which will cause S1 to be updated
     correctly */
  BDISP_STATE_INVALIDATE (SOURCE_SRC1_MODE);
  BDISP_STATE_INVALIDATE (ROTATION); /* configure copy direction */
  BDISP_STATE_INVALIDATE (INPUTMATRIX);
  BDISP_STATE_INVALIDATE (MATRIXCONVERSIONS);

  /* done */
  BDISP_STATE_VALIDATED (DESTINATION);
}

#ifdef BDISP2_SUPPORT_HW_CLIPPING
static bool
bdisp_state_validate_CLIP (struct stm_bdisp2_device_data  * const stdev,
                           const struct stm_blitter_state * const state)
{
  stm_blit_printd (BDisp_State, "%s %lld,%lld - %lldx%lld (%s)\n",
                   __func__,
                   (long long) state->clip.position1.x,
                   (long long) state->clip.position1.y,
                   (long long) state->clip.position2.x,
                   (long long) state->clip.position2.y,
                   ((state->clip_mode == STM_BLITTER_CM_CLIP_INSIDE)
                    ? "inside" : "outside"));

  /* FIXME: clip_mode inside outside */
  stdev->setup->ConfigClip.BLT_CWO = ((0 << 31)
                                      | (state->clip.position1.y << 16)
                                      | state->clip.position1.x);
  stdev->setup->ConfigClip.BLT_CWS = ((state->clip.position2.y << 16)
                                      | state->clip.position2.x);

  /* done */
  BDISP_STATE_VALIDATED (CLIP);

  return true;
}
#endif /* BDISP2_SUPPORT_HW_CLIPPING */

/**************************************************************/
/* Set the source surface state in the device state structure */
/**************************************************************/
static void
bdisp_state_validate_PALETTE_BLIT (struct stm_bdisp2_device_data  * const stdev,
                                   const struct stm_blitter_state * const state)
{
  stm_blit_printd (BDisp_State, "%s\n", __func__);

  /* for blitting op */
  stdev->setup->palette_type = SG2C_NORMAL;

  stdev->setup->blitstate.bIndexTranslation =
    ((state->src->format & STM_BLITTER_SF_INDEXED)
     && (state->dst->format & STM_BLITTER_SF_INDEXED));

  if (state->src->format & STM_BLITTER_SF_INDEXED)
    {
      STM_BLITTER_ASSUME (state->src->buffer_address.palette.num_entries != 0);
      stdev->setup->palette = &state->src->buffer_address.palette;
    }
  else
    stdev->setup->palette = NULL;

  /* done */
  BDISP_STATE_VALIDATED (PALETTE_BLIT);
}

static void
bdisp_state_validate_SOURCE_SRC1_MODE (struct stm_bdisp2_device_data  * const stdev,
                                       const struct stm_blitter_state * const state,
                                       enum stm_blitter_accel          accel)
{
  stm_blit_printd (BDisp_State, "%s\n", __func__);

  stdev->setup->ConfigGeneral.BLT_CIC |= BLIT_CIC_NODE_SOURCE1;
  stdev->setup->ConfigGeneral.BLT_INS &= ~BLIT_INS_SRC1_MODE_MASK;

  switch (state->src->format)
    {
    case STM_BLITTER_SF_YV12:
    case STM_BLITTER_SF_I420:
    case STM_BLITTER_SF_YV16:
    case STM_BLITTER_SF_YV61:
    case STM_BLITTER_SF_YCBCR444P:
      stdev->setup->ConfigGeneral.BLT_INS |= BLIT_INS_SRC1_MODE_MEMORY;
      stdev->setup->ConfigSource1.BLT_S1BA
        = (state->src->buffer_address.base
           + state->src->buffer_address.cr_offset);
      stdev->setup->ConfigSource1.BLT_S1TY
        = stdev->setup->ConfigSource2.BLT_S2TY;
      break;

    case STM_BLITTER_SF_RGB565:
    case STM_BLITTER_SF_RGB24:
    case STM_BLITTER_SF_BGR24:
    case STM_BLITTER_SF_RGB32:
    case STM_BLITTER_SF_ARGB1555:
    case STM_BLITTER_SF_ARGB4444:
    case STM_BLITTER_SF_ARGB8565:
    case STM_BLITTER_SF_AlRGB8565:
    case STM_BLITTER_SF_ARGB:
    case STM_BLITTER_SF_AlRGB:
    case STM_BLITTER_SF_BGRA:
    case STM_BLITTER_SF_BGRAl:
    case STM_BLITTER_SF_ABGR:
    case STM_BLITTER_SF_LUT8:
    case STM_BLITTER_SF_LUT4:
    case STM_BLITTER_SF_LUT2:
    case STM_BLITTER_SF_LUT1:
    case STM_BLITTER_SF_ALUT88:
    case STM_BLITTER_SF_AlLUT88:
    case STM_BLITTER_SF_ALUT44:
    case STM_BLITTER_SF_A8:
    case STM_BLITTER_SF_Al8:
    case STM_BLITTER_SF_A1:
    case STM_BLITTER_SF_AVYU:
    case STM_BLITTER_SF_AlVYU:
    case STM_BLITTER_SF_VYU:
    case STM_BLITTER_SF_YUY2:
    case STM_BLITTER_SF_UYVY:
    case STM_BLITTER_SF_YCBCR420MB:
    case STM_BLITTER_SF_YCBCR422MB:
    case STM_BLITTER_SF_NV12:
    case STM_BLITTER_SF_NV21:
    case STM_BLITTER_SF_NV16:
    case STM_BLITTER_SF_NV61:
    case STM_BLITTER_SF_NV24:
      /* for the formats that don't need S1 (BLIT_INS_SRC1_MODE_DISABLED) we
         obviously still need to set correct data into ConfigSource1 as it might
         get enabled due to some blitting flags, e.g. blend. */
      if (!(state->blitflags & STM_BLITTER_SBF_READ_SOURCE2)
          && !(accel & STM_BLITTER_ACCEL_BLIT2))
        {
          stdev->setup->ConfigGeneral.BLT_INS
            |= (BLIT_INS_SRC1_MODE_DISABLED
                | stdev->setup->blitstate.blt_ins_src1_mode);
          stdev->setup->ConfigSource1.BLT_S1BA
            = stdev->setup->ConfigTarget.BLT_TBA;
          stdev->setup->ConfigSource1.BLT_S1TY
            = (stdev->setup->ConfigTarget.BLT_TTY
               | BLIT_STY_COLOR_EXPAND_MSB);
        }
      else
        {
          STM_BLITTER_ASSERT((stdev->setup->blitstate.blt_ins_src1_mode
                              == BLIT_INS_SRC1_MODE_MEMORY)
                             || (stdev->setup->blitstate.blt_ins_src1_mode
                                 == BLIT_INS_SRC1_MODE_DISABLED));

          stdev->setup->ConfigGeneral.BLT_INS |= BLIT_INS_SRC1_MODE_MEMORY;
          stdev->setup->ConfigSource1.BLT_S1BA
            = state->src2->buffer_address.base;
          _bdisp_state_set_buffer_type (stdev,
                                        &stdev->setup->ConfigSource1.BLT_S1TY,
                                        state->src2->format,
                                        state->src2->stride);
          stdev->setup->ConfigSource1.BLT_S1TY |= BLIT_STY_COLOR_EXPAND_MSB;
        }
      break;

    /* these are not supported via the normal API */
    case STM_BLITTER_SF_RLD_BD:
    case STM_BLITTER_SF_RLD_H2:
    case STM_BLITTER_SF_RLD_H8:
    case STM_BLITTER_SF_INVALID:
    case STM_BLITTER_SF_COUNT:
    default:
      /* not reached */
      STM_BLITTER_ASSERT ("should not be reached" == NULL);
      break;
    }

  /* done */
  BDISP_STATE_VALIDATED (SOURCE_SRC1_MODE);
}

#define TO_FIXED_POINT(x)         ((x) << 16) /* convert x to fixed point */

static void
bdisp_state_validate_SOURCE (struct stm_bdisp2_device_data  * const stdev,
                             const struct stm_blitter_state * const state)
{
  STM_BLITTER_ASSERT (state->src != NULL);

  stm_blit_printd (BDisp_State, "%s phys: %.8lx\n",
                   __func__, state->src->buffer_address.base);

  STM_BLITTER_ASSERT (state->src->buffer_address.base != 0);

  stdev->setup->upsampled_src_nbpix_min = stdev->features.hw.upsampled_nbpix_min;
  if (state->src->format & STM_BLITTER_SF_PLANAR)
    stdev->setup->upsampled_src_nbpix_min = stdev->features.hw.upsampled_nbpix_min*2;

  stdev->setup->blitstate.source_w = TO_FIXED_POINT (state->src->size.w);
  stdev->setup->blitstate.source_h = TO_FIXED_POINT (state->src->size.h);

  stdev->setup->ConfigGeneral.BLT_CIC &= ~(BLIT_CIC_NODE_SOURCE1
                                           | BLIT_CIC_NODE_SOURCE2
                                           | BLIT_CIC_NODE_SOURCE3);
  stdev->setup->ConfigGeneral.BLT_INS &= ~(BLIT_INS_SRC1_MODE_MASK
                                           | BLIT_INS_SRC2_MODE_MASK
                                           | BLIT_INS_SRC3_MODE_MASK);
  /* previously, NV12 and NV16 did not have CIC_NODE_SOURCE1 set. As we don't
     allow any blending involving these two formats that should not have
     mattered, but setting it shouldn't do any harm either. */
  stdev->setup->ConfigGeneral.BLT_CIC |= BLIT_CIC_NODE_SOURCE2;
  stdev->setup->ConfigGeneral.BLT_INS |= BLIT_INS_SRC2_MODE_MEMORY;

  stdev->setup->blitstate.srcFactorH = stdev->setup->blitstate.srcFactorV = 1;

  switch (state->src->format)
    {
    case STM_BLITTER_SF_YV12:
    case STM_BLITTER_SF_I420:
      stdev->setup->blitstate.srcFactorV = 2;
      /* fall through */
    case STM_BLITTER_SF_YV16:
    case STM_BLITTER_SF_YV61:
      stdev->setup->blitstate.srcFactorH = 2;
      /* fall through */
    case STM_BLITTER_SF_YCBCR444P:
      stdev->setup->ConfigGeneral.BLT_CIC |= BLIT_CIC_NODE_SOURCE3;
      stdev->setup->ConfigGeneral.BLT_INS |= BLIT_INS_SRC3_MODE_MEMORY;

      stdev->setup->ConfigSource3.BLT_S3BA = state->src->buffer_address.base;
      stdev->setup->ConfigSource2.BLT_S2BA
        = (state->src->buffer_address.base
           + state->src->buffer_address.cb_offset);

      _bdisp_state_set_buffer_type (stdev,
                                    &stdev->setup->ConfigSource3.BLT_S3TY,
                                    state->src->format,
                                    state->src->stride);
      stdev->setup->ConfigSource3.BLT_S3TY |= BLIT_STY_COLOR_EXPAND_MSB;
      stdev->setup->ConfigSource2.BLT_S2TY
        = ((stdev->setup->ConfigSource3.BLT_S3TY
            & ~BLIT_TY_PIXMAP_PITCH_MASK)
           | (state->src->stride / stdev->setup->blitstate.srcFactorH));
      break;

    case STM_BLITTER_SF_NV12:
    case STM_BLITTER_SF_NV21:
    case STM_BLITTER_SF_YCBCR420MB:
      stdev->setup->blitstate.srcFactorV = 2;
      /* fall through */
    case STM_BLITTER_SF_NV16:
    case STM_BLITTER_SF_NV61:
    case STM_BLITTER_SF_YCBCR422MB:
      stdev->setup->blitstate.srcFactorH = 2;
      /* fall through */
    case STM_BLITTER_SF_NV24:
      stdev->setup->ConfigGeneral.BLT_CIC |= BLIT_CIC_NODE_SOURCE3;
      stdev->setup->ConfigGeneral.BLT_INS |= BLIT_INS_SRC3_MODE_MEMORY;

      stdev->setup->ConfigSource3.BLT_S3BA = state->src->buffer_address.base;
      stdev->setup->ConfigSource2.BLT_S2BA
        = (state->src->buffer_address.base
           + state->src->buffer_address.cbcr_offset);

      _bdisp_state_set_buffer_type (stdev,
                                    &stdev->setup->ConfigSource3.BLT_S3TY,
                                    state->src->format,
                                    state->src->stride);
      stdev->setup->ConfigSource3.BLT_S3TY |= BLIT_STY_COLOR_EXPAND_MSB;
      stdev->setup->ConfigSource2.BLT_S2TY
        = stdev->setup->ConfigSource3.BLT_S3TY;
      if (state->src->format == STM_BLITTER_SF_NV24) {
        stdev->setup->ConfigSource2.BLT_S2TY &= ~BLIT_TY_PIXMAP_PITCH_MASK;
        stdev->setup->ConfigSource2.BLT_S2TY
          |= (stdev->setup->ConfigSource3.BLT_S3TY
              & BLIT_TY_PIXMAP_PITCH_MASK) * 2;
      }
      break;

    case STM_BLITTER_SF_RGB565:
    case STM_BLITTER_SF_RGB24:
    case STM_BLITTER_SF_BGR24:
    case STM_BLITTER_SF_RGB32:
    case STM_BLITTER_SF_ARGB1555:
    case STM_BLITTER_SF_ARGB4444:
    case STM_BLITTER_SF_ARGB8565:
    case STM_BLITTER_SF_AlRGB8565:
    case STM_BLITTER_SF_ARGB:
    case STM_BLITTER_SF_AlRGB:
    case STM_BLITTER_SF_BGRA:
    case STM_BLITTER_SF_BGRAl:
    case STM_BLITTER_SF_ABGR:
    case STM_BLITTER_SF_LUT8:
    case STM_BLITTER_SF_LUT4:
    case STM_BLITTER_SF_LUT2:
    case STM_BLITTER_SF_LUT1:
    case STM_BLITTER_SF_ALUT88:
    case STM_BLITTER_SF_AlLUT88:
    case STM_BLITTER_SF_ALUT44:
    case STM_BLITTER_SF_A8:
    case STM_BLITTER_SF_Al8:
    case STM_BLITTER_SF_A1:
    case STM_BLITTER_SF_AVYU:
    case STM_BLITTER_SF_AlVYU:
    case STM_BLITTER_SF_VYU:
    case STM_BLITTER_SF_YUY2:
    case STM_BLITTER_SF_UYVY:
      stdev->setup->ConfigGeneral.BLT_INS |= BLIT_INS_SRC3_MODE_DISABLED;

      stdev->setup->ConfigSource3.BLT_S3BA = 0xdeadba5e;
      stdev->setup->ConfigSource2.BLT_S2BA = state->src->buffer_address.base;

      stdev->setup->ConfigSource3.BLT_S3TY = 0xf0011eef;
      _bdisp_state_set_buffer_type (stdev,
                                    &stdev->setup->ConfigSource2.BLT_S2TY,
                                    state->src->format,
                                    state->src->stride);
      stdev->setup->ConfigSource2.BLT_S2TY |= BLIT_STY_COLOR_EXPAND_MSB;
      break;

    /* these are not supported via the normal API */
    case STM_BLITTER_SF_RLD_BD:
    case STM_BLITTER_SF_RLD_H2:
    case STM_BLITTER_SF_RLD_H8:
    case STM_BLITTER_SF_INVALID:
    case STM_BLITTER_SF_COUNT:
    default:
      /* not reached */
      STM_BLITTER_ASSERT ("should not be reached" == NULL);
      break;
    }

  /* as we touch the source1 mode, make sure it stays correct. */
  BDISP_STATE_INVALIDATE (SOURCE_SRC1_MODE);
  BDISP_STATE_INVALIDATE (BLITFLAGS);
  BDISP_STATE_INVALIDATE (ROTATION); /* configure copy direction */
  BDISP_STATE_INVALIDATE (INPUTMATRIX);
  BDISP_STATE_INVALIDATE (MATRIXCONVERSIONS);

  /* done */
  BDISP_STATE_VALIDATED (SOURCE);
}

static void
bdisp_state_validate_FILLCOLOR (struct stm_bdisp2_device_data  * const stdev,
                                const struct stm_blitter_state * const state)
{
  stm_blitter_color_colorspace_t colorspace = state->color_colorspace;
  uint32_t a = state->colors[0].a;
  uint32_t r = state->colors[0].r;
  uint32_t g = state->colors[0].g;
  uint32_t b = state->colors[0].b;

  bool tty_is_ycbcr, cty_is_ycbcr;

  stm_blit_printd (BDisp_State, "%s αrgb %.2x%s %.2x%.2x%.2x idx %.2x%s %s\n",
                   __func__,
                   a,
                   ((state->drawflags & STM_BLITTER_SDF_SRC_PREMULTIPLY)
                    ? " *" : ""),
                   r, g, b,
                   state->color_index,
                   ((state->drawflags & STM_BLITTER_SDF_BLEND)
                    ? " (blend)" : ""),
                   ((state->drawflags & STM_BLITTER_SDF_BLEND)
                    ? stm_blitter_get_porter_duff_name(state->pd) : ""));

  if (state->drawflags & STM_BLITTER_SDF_SRC_PREMULTIPLY)
    {
      /* we do source alpha pre-multiplication now, rather in the hardware
         so we can support it regardless of the blend mode. */
      r = (r * (a + 1)) >> 8;
      g = (g * (a + 1)) >> 8;
      b = (b * (a + 1)) >> 8;
    }

  stdev->setup->drawstate.ConfigGeneral.BLT_CIC &= ~BLIT_CIC_NODE_OVMX;
  stdev->setup->drawstate.ConfigGeneral.BLT_INS &= ~BLIT_INS_ENABLE_OVMX;

  if (state->drawflags & STM_BLITTER_SDF_BLEND)
    {

      /* Activate matrix for case of slow path to workarround rounding issue.
         The CLEAR PD rule is accelerated as direct fill operation and should
         use the fast path. Enabling OVMx with CLEAR PD will maintain S1CF
         un-cleared and results in unexpected behavior. */
      if ((state->dst->format == STM_BLITTER_SF_RGB565   ||
           state->dst->format == STM_BLITTER_SF_ARGB1555 ||
           state->dst->format == STM_BLITTER_SF_ARGB4444 ||
           state->dst->format == STM_BLITTER_SF_ARGB8565 ||
           state->dst->format == STM_BLITTER_SF_AlRGB8565 )
           && state->pd != STM_BLITTER_PD_CLEAR)
        {
          stdev->setup->drawstate.ConfigGeneral.BLT_CIC |= BLIT_CIC_NODE_OVMX;
          stdev->setup->drawstate.ConfigGeneral.BLT_INS |= BLIT_INS_ENABLE_OVMX;
        }

      switch (state->pd)
        {
        case STM_BLITTER_PD_CLEAR:
          /* Just fill with 0. Set the source format to the dest format to get
             a fast fill. */
          stdev->setup->drawstate.color = 0;
          switch (state->dst->format)
            {
            case STM_BLITTER_SF_YUY2:
            case STM_BLITTER_SF_UYVY:
            case STM_BLITTER_SF_AVYU:
            case STM_BLITTER_SF_AlVYU:
            case STM_BLITTER_SF_VYU:
            case STM_BLITTER_SF_YV12:
            case STM_BLITTER_SF_I420:
            case STM_BLITTER_SF_YV16:
            case STM_BLITTER_SF_YV61:
            case STM_BLITTER_SF_YCBCR444P:
            case STM_BLITTER_SF_NV12:
            case STM_BLITTER_SF_NV21:
            case STM_BLITTER_SF_NV16:
            case STM_BLITTER_SF_NV61:
            case STM_BLITTER_SF_YCBCR420MB:
            case STM_BLITTER_SF_YCBCR422MB:
            case STM_BLITTER_SF_NV24:
              /* full alpha ange doesn't matter here as it's 0 anyway */
              stdev->setup->drawstate.color_ty = BLIT_COLOR_FORM_ARGB8888;
              break;

            case STM_BLITTER_SF_RGB32:
              if (!stdev->features.hw.rgb32)
                stdev->setup->drawstate.color = ((0xff << 24)
                                                 | (0x00 << 16)
                                                 | (0x00 <<  8)
                                                 | (0x00 <<  0));
              /* fall through */
            case STM_BLITTER_SF_RGB565:
            case STM_BLITTER_SF_RGB24:
            case STM_BLITTER_SF_BGR24:
            case STM_BLITTER_SF_ARGB1555:
            case STM_BLITTER_SF_ARGB4444:
            case STM_BLITTER_SF_ARGB8565:
            case STM_BLITTER_SF_AlRGB8565:
            case STM_BLITTER_SF_ARGB:
            case STM_BLITTER_SF_AlRGB:
            case STM_BLITTER_SF_ABGR:
            case STM_BLITTER_SF_LUT8:
            case STM_BLITTER_SF_LUT4:
            case STM_BLITTER_SF_LUT2:
            case STM_BLITTER_SF_LUT1:
            case STM_BLITTER_SF_ALUT88:
            case STM_BLITTER_SF_AlLUT88:
            case STM_BLITTER_SF_ALUT44:
            case STM_BLITTER_SF_A8:
            case STM_BLITTER_SF_Al8:
            case STM_BLITTER_SF_A1:
              /* FIXME - unsetting big endian here means that further down
                 when the function pointers are updated a clear to a big
                 endian surface (BGRA8888) will end up in the slow path, as
                 BLT_TTY and color_ty will not be identical anymore. That's
                 not an issue right now, as BGRA8888 is not a supported
                 DirectFB format at the moment, and it is the only non YUV
                 format with the big endian flag set. */
              stdev->setup->drawstate.color_ty =
                (bdisp_ty_get_format_from_ty (stdev->setup->ConfigTarget.BLT_TTY)
                 & ~BLIT_TY_BIG_ENDIAN);
              break;

            case STM_BLITTER_SF_BGRA:
            case STM_BLITTER_SF_BGRAl:
            /* these are not supported via the normal API */
            case STM_BLITTER_SF_RLD_BD:
            case STM_BLITTER_SF_RLD_H2:
            case STM_BLITTER_SF_RLD_H8:
            case STM_BLITTER_SF_INVALID:
            case STM_BLITTER_SF_COUNT:
            default:
              /* unsupported (for now) */
              break;
            }
          break;

        case STM_BLITTER_PD_SOURCE:
          if (!stdev->features.hw.rgb32
              && unlikely (state->dst->format == STM_BLITTER_SF_RGB32))
            a = 0xff;
          /* fall through */
        case STM_BLITTER_PD_DEST: /* This leaves the destination as is, i.e.
                                     a nop, because we do not support
                                     destination pre-multiplication. */
        case STM_BLITTER_PD_DEST_OVER:
        case STM_BLITTER_PD_SOURCE_OVER:
        case STM_BLITTER_PD_SOURCE_IN:
        case STM_BLITTER_PD_DEST_IN:
        case STM_BLITTER_PD_SOURCE_OUT:
        case STM_BLITTER_PD_DEST_OUT:
        case STM_BLITTER_PD_SOURCE_ATOP:
        case STM_BLITTER_PD_DEST_ATOP:
        case STM_BLITTER_PD_XOR:
        case STM_BLITTER_PD_NONE:
          /* for blended fills we use ARGB8888, to get full alpha precision in
             the blend, the hardware does color conversion. */
          stdev->setup->drawstate.color = ((a << 24)
                                           | (r << 16)
                                           | (g <<  8)
                                           | (b <<  0));

          switch (state->dst->format)
            {
            case STM_BLITTER_SF_YUY2:
            case STM_BLITTER_SF_UYVY:
            case STM_BLITTER_SF_AVYU:
            case STM_BLITTER_SF_AlVYU:
            case STM_BLITTER_SF_VYU:
            case STM_BLITTER_SF_YV12:
            case STM_BLITTER_SF_I420:
            case STM_BLITTER_SF_YV16:
            case STM_BLITTER_SF_YV61:
            case STM_BLITTER_SF_YCBCR444P:
            case STM_BLITTER_SF_NV12:
            case STM_BLITTER_SF_NV21:
            case STM_BLITTER_SF_NV16:
            case STM_BLITTER_SF_NV61:
            case STM_BLITTER_SF_YCBCR420MB:
            case STM_BLITTER_SF_YCBCR422MB:
            case STM_BLITTER_SF_NV24:
            case STM_BLITTER_SF_RGB32:
            case STM_BLITTER_SF_RGB565:
            case STM_BLITTER_SF_RGB24:
            case STM_BLITTER_SF_BGR24:
            case STM_BLITTER_SF_ARGB1555:
            case STM_BLITTER_SF_ARGB4444:
            case STM_BLITTER_SF_ARGB8565:
            case STM_BLITTER_SF_AlRGB8565:
            case STM_BLITTER_SF_ARGB:
            case STM_BLITTER_SF_AlRGB:
            case STM_BLITTER_SF_LUT8:
            case STM_BLITTER_SF_LUT4:
            case STM_BLITTER_SF_LUT2:
            case STM_BLITTER_SF_LUT1:
            case STM_BLITTER_SF_ALUT88:
            case STM_BLITTER_SF_AlLUT88:
            case STM_BLITTER_SF_ALUT44:
            case STM_BLITTER_SF_A8:
            case STM_BLITTER_SF_Al8:
            case STM_BLITTER_SF_A1:
            case STM_BLITTER_SF_BGRA:
            case STM_BLITTER_SF_BGRAl:
              break;

            case STM_BLITTER_SF_ABGR:
              stdev->setup->drawstate.color = ((a << 24)
                                               | (r << 0)
                                               | (g << 8)
                                               | (b << 16));
              break;

            case STM_BLITTER_SF_RLD_BD:
            case STM_BLITTER_SF_RLD_H2:
            case STM_BLITTER_SF_RLD_H8:
            case STM_BLITTER_SF_INVALID:
            case STM_BLITTER_SF_COUNT:
            default:
              /* unsupported (for now) */
              break;
            }

          if (colorspace == STM_BLITTER_COLOR_RGB)
            stdev->setup->drawstate.color_ty = BLIT_COLOR_FORM_ARGB8888;
          else
            stdev->setup->drawstate.color_ty = BLIT_COLOR_FORM_AYCBCR8888;
          stdev->setup->drawstate.color_ty |= BLIT_TY_FULL_ALPHA_RANGE;
          break;

        default:
          /* not reached */
          STM_BLITTER_ASSERT ("should not be reached" == NULL);
          break;
        }
    }
  else
    {
      if ((state->dst->format & STM_BLITTER_SF_ALPHA_LIMITED)
          == STM_BLITTER_SF_ALPHA_LIMITED)
        /* Note that the alpha range is 0-128 (not 127!) */
        a = (a + 1) / 2;

      if (colorspace == STM_BLITTER_COLOR_RGB)
        {
          if (!BLIT_TY_COLOR_FORM_IS_YUV (stdev->setup->ConfigTarget.BLT_TTY))
            {
              /* fast path */
              stdev->setup->drawstate.color_ty
                = bdisp_ty_get_format_from_ty (stdev->setup->ConfigTarget.BLT_TTY);
            }
          else
            {
              stdev->setup->drawstate.color = ((a << 24)
                                               | (r << 16)
                                               | (g <<  8)
                                               | (b <<  0));
              stdev->setup->drawstate.color_ty = BLIT_COLOR_FORM_ARGB8888;

              if ((state->dst->format & STM_BLITTER_SF_ALPHA_LIMITED)
                   != STM_BLITTER_SF_ALPHA_LIMITED)
                stdev->setup->drawstate.color_ty |= BLIT_TY_FULL_ALPHA_RANGE;
            }
        }
      else
        {
          if (BLIT_TY_COLOR_FORM_IS_YUV (stdev->setup->ConfigTarget.BLT_TTY)
              && !(state->dst->format & STM_BLITTER_SF_PLANAR)
              && !(state->dst->format == STM_BLITTER_SF_UYVY)
              && !(state->dst->format == STM_BLITTER_SF_YUY2))
            {
              /* fast path */
              stdev->setup->drawstate.color_ty
                = bdisp_ty_get_format_from_ty (stdev->setup->ConfigTarget.BLT_TTY);
            }
          else
            {
              stdev->setup->drawstate.color = ((a << 24)
                                               | (r << 16)
                                               | (g <<  8)
                                               | (b <<  0));

              stdev->setup->drawstate.color_ty = BLIT_COLOR_FORM_AYCBCR8888;

              if ((state->dst->format & STM_BLITTER_SF_ALPHA_LIMITED)
                   != STM_BLITTER_SF_ALPHA_LIMITED)
                stdev->setup->drawstate.color_ty |= BLIT_TY_FULL_ALPHA_RANGE;
            }
        }

      /* fast path will be used only if the color format is the same as for
         the destination format. Otherwise, slow path will be used for the fill
         color operation. */
      if (bdisp_ty_formats_identical (stdev->setup->drawstate.color_ty,
                                      stdev->setup->ConfigTarget.BLT_TTY))
        {
          switch (state->dst->format)
            {
            case STM_BLITTER_SF_ARGB1555:
              stdev->setup->drawstate.color = (((a & 0x80) << 8)
                                               | ((r & 0xf8) << 7)
                                               | ((g & 0xf8) << 2)
                                               | ((b & 0xf8) >> 3));
              break;

            case STM_BLITTER_SF_ARGB4444:
              stdev->setup->drawstate.color = (((a & 0xf0) << 8)
                                               | ((r & 0xf0) << 4)
                                               | ((g & 0xf0) << 0)
                                               | ((b & 0xf0) >> 4));
              break;

            case STM_BLITTER_SF_RGB565:
              stdev->setup->drawstate.color = (((r & 0xf8) << 8)
                                               | ((g & 0xfc) << 3)
                                               | ((b & 0xf8) >> 3));
              break;

            case STM_BLITTER_SF_VYU:
            case STM_BLITTER_SF_RGB24:
            case STM_BLITTER_SF_BGR24:
            case STM_BLITTER_SF_RGB32:
              stdev->setup->drawstate.color = ((0xff << 24)
                                               | (r << 16)
                                               | (g <<  8)
                                               | (b <<  0));
              break;

            case STM_BLITTER_SF_ARGB8565:
            case STM_BLITTER_SF_AlRGB8565:
              stdev->setup->drawstate.color = ((a << 16)
                                               | ((r & 0xf8) << 8)
                                               | ((g & 0xfc) << 3)
                                               | ((b & 0xf8) >> 3));
              break;

            case STM_BLITTER_SF_AVYU:
            case STM_BLITTER_SF_AlVYU:
            case STM_BLITTER_SF_ARGB:
            case STM_BLITTER_SF_AlRGB:
            case STM_BLITTER_SF_BGRA:
            case STM_BLITTER_SF_BGRAl:
              stdev->setup->drawstate.color = ((a << 24)
                                               | (r << 16)
                                               | (g <<  8)
                                               | (b <<  0));
              break;

            case STM_BLITTER_SF_ABGR:
              stdev->setup->drawstate.color = ((a << 24)
                                               | (r << 0)
                                               | (g << 8)
                                               | (b << 16));
              break;

            case STM_BLITTER_SF_LUT8:
              stdev->setup->drawstate.color = state->color_index;
              break;

            case STM_BLITTER_SF_LUT4:
              stdev->setup->drawstate.color = state->color_index & 0x0f;
              break;

            case STM_BLITTER_SF_LUT2:
              stdev->setup->drawstate.color = state->color_index & 0x03;
              break;

            case STM_BLITTER_SF_LUT1:
              stdev->setup->drawstate.color = state->color_index & 0x01;
              break;

            case STM_BLITTER_SF_ALUT88:
            case STM_BLITTER_SF_AlLUT88:
              stdev->setup->drawstate.color = (a << 8) | state->color_index;
              break;

            case STM_BLITTER_SF_ALUT44:
              stdev->setup->drawstate.color = (a & 0xf0) | (state->color_index & 0x0f);
              break;

            case STM_BLITTER_SF_A8:
            case STM_BLITTER_SF_Al8:
              stdev->setup->drawstate.color = a;
              break;

            case STM_BLITTER_SF_A1:
              stdev->setup->drawstate.color = a & 0x01;
              break;

            case STM_BLITTER_SF_RLD_BD:
            case STM_BLITTER_SF_RLD_H2:
            case STM_BLITTER_SF_RLD_H8:
            case STM_BLITTER_SF_YUY2:
            case STM_BLITTER_SF_UYVY:
            case STM_BLITTER_SF_NV12:
            case STM_BLITTER_SF_NV21:
            case STM_BLITTER_SF_NV16:
            case STM_BLITTER_SF_NV61:
            case STM_BLITTER_SF_YCBCR420MB:
            case STM_BLITTER_SF_YCBCR422MB:
            case STM_BLITTER_SF_NV24:
            case STM_BLITTER_SF_YV12:
            case STM_BLITTER_SF_I420:
            case STM_BLITTER_SF_YV16:
            case STM_BLITTER_SF_YV61:
            case STM_BLITTER_SF_YCBCR444P:
              /* these are unsupported destination formats, fall through */
            case STM_BLITTER_SF_INVALID:
            case STM_BLITTER_SF_COUNT:
            default:
              stm_blit_printe ("stgfx2/state: Unexpected color format %x\n",
                               state->dst->format);
              break;
            }
        }
    }


  stdev->setup->drawstate.ConfigGeneral.BLT_CIC &= ~BLIT_CIC_NODE_IVMX;
  stdev->setup->drawstate.ConfigGeneral.BLT_INS &= ~BLIT_INS_ENABLE_IVMX;

  tty_is_ycbcr = !!BLIT_TY_COLOR_FORM_IS_YUV (stdev->setup->ConfigTarget.BLT_TTY);
  cty_is_ycbcr = !!BLIT_TY_COLOR_FORM_IS_YUV (stdev->setup->drawstate.color_ty);
  if (tty_is_ycbcr != cty_is_ycbcr)
    {
      stm_blit_printd (BDisp_State,
                       "%s(): color conversion YCbCr/RGB\n", __func__);

      stdev->setup->drawstate.ConfigGeneral.BLT_CIC |= BLIT_CIC_NODE_IVMX;
      stdev->setup->drawstate.ConfigGeneral.BLT_INS |= BLIT_INS_ENABLE_IVMX;
    }

  /* done */
  BDISP_STATE_VALIDATED (FILLCOLOR);
}

static int
bdisp_state_setup_draw_blend (struct stm_bdisp2_device_data  * const stdev,
                              const struct stm_blitter_state * const state,
                              uint32_t                       * const blt_ins,
                              uint32_t                       * const blt_ack)
{
  static const uint32_t global_alpha = 128;

  switch (state->pd)
    {
    case STM_BLITTER_PD_DEST:
      stm_blit_printd (BDisp_State, "  -> PD dest\n");
      break;

    case STM_BLITTER_PD_DEST_OVER:
      /* src func = 1-dst alpha, dest func = 1.0   (STM_BLITTER_PD_DEST_OVER) */
      *blt_ack |= BLIT_ACK_SWAP_FG_BG;
      /* fall through */
    case STM_BLITTER_PD_SOURCE_OVER:
      /* src func = 1.0, dest func = 1-src alpha (STM_BLITTER_PD_SOURCE_OVER) */
      *blt_ack |= ((global_alpha << BLIT_ACK_GLOBAL_ALPHA_SHIFT)
                   | BLIT_ACK_BLEND_SRC2_PREMULT);
      *blt_ins |= BLIT_INS_SRC1_MODE_MEMORY | BLIT_INS_SRC2_MODE_COLOR_FILL;
      stm_blit_printd (BDisp_State, "  -> PD %s over\n",
                       ((*blt_ack & BLIT_ACK_SWAP_FG_BG)
                        ? "dest"
                        : "source"));
      break;

    case STM_BLITTER_PD_NONE:
      /* src func = src alpha, dest func = 1-src alpha (STM_BLITTER_PD_NONE)
         We can only support this when the destination format has no alpha
         channel. This is because the hardware always calculates the
         destination alpha as Asrc + Adst*(1-Asrc) but we would require
         Asrc*Asrc + Adst*(1-Asrc). */
      stm_blit_printd (BDisp_State, "  -> PD none\n");
      stm_blit_printd (BDisp_State,
                       "    -> setting src as NOT pre-multiplied\n");
      *blt_ack |= ((global_alpha << BLIT_ACK_GLOBAL_ALPHA_SHIFT)
                   | BLIT_ACK_BLEND_SRC2_N_PREMULT);
      *blt_ins |= BLIT_INS_SRC1_MODE_MEMORY | BLIT_INS_SRC2_MODE_COLOR_FILL;
      break;

    case STM_BLITTER_PD_CLEAR:
    case STM_BLITTER_PD_SOURCE:
      /* for clear, we would normally just use a direct (fast) fill, but
         we can't for YUV formats as the blitter needs to convert ARGB
         0x00000000 into YUV space */
      if (bdisp_ty_formats_identical (stdev->setup->drawstate.color_ty,
                                      stdev->setup->ConfigTarget.BLT_TTY)
          && !(state->drawflags & STM_BLITTER_SDF_DST_COLORKEY))
        {
          *blt_ins |= BLIT_INS_SRC1_MODE_DIRECT_FILL;
          if (!stdev->features.hw.rgb32
              && state->dst->format == STM_BLITTER_SF_RGB32)
            {
              stdev->setup->drawstate.ConfigGeneral.BLT_CIC &= ~BLIT_CIC_NODE_CLUT;
              *blt_ins &= ~BLIT_INS_ENABLE_CLUT;
            }
          else
            {
              STM_BLITTER_ASSUME ((*blt_ins & BLIT_INS_ENABLE_CLUT) == 0);
              STM_BLITTER_ASSUME ((stdev->setup->drawstate.ConfigGeneral.BLT_CIC
                                   & BLIT_CIC_NODE_CLUT) == 0);
            }
          stm_blit_printd (BDisp_State,
                           "  -> PD %s (fastpath)\n",
                           ((state->pd == STM_BLITTER_PD_CLEAR)
                            ? "clear"
                            : "source"));
        }
      else
        {
          /* let the blitter do the color conversion (on the slow path) */
          *blt_ack |= BLIT_ACK_BYPASSSOURCE2;
          *blt_ins |= BLIT_INS_SRC1_MODE_DISABLED | BLIT_INS_SRC2_MODE_COLOR_FILL;
          stm_blit_printd (BDisp_State,
                           "  -> PD %s\n",
                           ((state->pd == STM_BLITTER_PD_CLEAR)
                            ? "clear"
                            : "source"));
        }
      break;

    case STM_BLITTER_PD_SOURCE_IN:
    case STM_BLITTER_PD_DEST_IN:
    case STM_BLITTER_PD_SOURCE_OUT:
    case STM_BLITTER_PD_DEST_OUT:
    case STM_BLITTER_PD_SOURCE_ATOP:
    case STM_BLITTER_PD_DEST_ATOP:
    case STM_BLITTER_PD_XOR:
    default:
      stm_blit_printw ("%s: unexpected Porter/Duff rule: %d\n",
                       __func__, state->pd);
      return -EINVAL;
    }

  return 0;
}

static int
bdisp_state_setup_native_blending_rules (const struct stm_blitter_state * const state,
                                         uint32_t                       * const blt_ack)
{
      switch (state->pd)
        {
        case STM_BLITTER_PD_CLEAR:
          stm_blit_printd (BDisp_State,
                           "  -> PD clear (native)\n");
          *blt_ack |= BLIT_ACK_BLEND_MODE_CLEAR;
          break;
        case STM_BLITTER_PD_SOURCE:
          stm_blit_printd (BDisp_State,
                           "  -> PD source (native)\n");
          *blt_ack |= BLIT_ACK_BLEND_MODE_S2;
          break;
        case STM_BLITTER_PD_DEST:
          stm_blit_printd (BDisp_State,
                           "  -> PD destination (native)\n");
          *blt_ack |= BLIT_ACK_BLEND_MODE_S1;
          break;
        case STM_BLITTER_PD_SOURCE_OVER:
          stm_blit_printd (BDisp_State,
                           "  -> PD source over (native)\n");
          *blt_ack |= BLIT_ACK_BLEND_MODE_S2_OVER_S1;
          break;
        case STM_BLITTER_PD_DEST_OVER:
          stm_blit_printd (BDisp_State,
                           "  -> PD dest over (native)\n");
          *blt_ack |= BLIT_ACK_BLEND_MODE_S1_OVER_S2;
          break;
        case STM_BLITTER_PD_SOURCE_IN:
          stm_blit_printd (BDisp_State,
                           "  -> PD source in (native)\n");
          *blt_ack |= BLIT_ACK_BLEND_MODE_S2_IN_S1;
          break;
        case STM_BLITTER_PD_DEST_IN:
          stm_blit_printd (BDisp_State,
                           "  -> PD dest in (native)\n");
          *blt_ack |= BLIT_ACK_BLEND_MODE_S1_IN_S2;
          break;
        case STM_BLITTER_PD_SOURCE_OUT:
          stm_blit_printd (BDisp_State,
                           "  -> PD source out (native)\n");
          *blt_ack |= BLIT_ACK_BLEND_MODE_S2_OUT_S1;
          break;
        case STM_BLITTER_PD_DEST_OUT:
          stm_blit_printd (BDisp_State,
                           "  -> PD dest out (native)\n");
          *blt_ack |= BLIT_ACK_BLEND_MODE_S1_OUT_S2;
          break;
        case STM_BLITTER_PD_SOURCE_ATOP:
          stm_blit_printd (BDisp_State,
                           "  -> PD source atop (native)\n");
          *blt_ack |= BLIT_ACK_BLEND_MODE_S2_ATOP_S1;
          break;
        case STM_BLITTER_PD_DEST_ATOP:
          stm_blit_printd (BDisp_State,
                           "  -> PD dest atop (native)\n");
          *blt_ack |= BLIT_ACK_BLEND_MODE_S1_ATOP_S2;
          break;
        case STM_BLITTER_PD_XOR:
          stm_blit_printd (BDisp_State,
                           "  -> PD xor (native)\n");
          *blt_ack |= BLIT_ACK_BLEND_MODE_S2_XOR_S1;
          break;

        case STM_BLITTER_PD_NONE: /* not reached */
        default:
          stm_blit_printw ("%s: unexpected Porter/Duff rule: %d\n",
                           __func__, state->pd);
          return -EINVAL;
        }
	return true;
}

static int
bdisp_state_setup_draw_blend_native (struct stm_bdisp2_device_data  * const stdev,
                                     const struct stm_blitter_state * const state,
                                     uint32_t                       * const blt_ins,
                                     uint32_t                       * const blt_ack)
{
  static const uint32_t global_alpha = 128;

  if (!stdev->features.hw.porterduff)
    return bdisp_state_setup_draw_blend (stdev, state, blt_ins, blt_ack);

  /* We handle some cases in the non-native PorterDuff support branch
     because... */
  if (state->pd == STM_BLITTER_PD_NONE)
    /* ... 1) PD_NONE is non-native anyway and is only implemented in the
       non-native path */
    return bdisp_state_setup_draw_blend (stdev, state, blt_ins, blt_ack);

  if ((state->pd == STM_BLITTER_PD_CLEAR
       || state->pd == STM_BLITTER_PD_SOURCE
       || (state->pd == STM_BLITTER_PD_DEST
           && !(state->drawflags & (STM_BLITTER_SDF_DST_PREMULTIPLY
                                    | STM_BLITTER_SDF_DST_COLORKEY))))
      && !stdev->no_blend_optimisation)
    /* ... 2) CLEAR and SOURCE are optimised to use the fast direct fill
              feature in case formats are identical.
       ... 3) Clear in the native case does the wrong thing when
              applied to a YCbCr destination. The ALU inserts 0, but it sits
              after the IVMX, so we end up with green, not black. In the
              non-native case we manually do a color fill with 0, but the
              IVMX is used to convert this to YCbCr.
       ... 4) CLEAR in the non-native case supports IPs that don't support
              RGB32 natively.

       DST_PREMULTIPLY has no effect on CLEAR and SOURCE, while DST_COLORKEY
       is correctly handled, too. */
    return bdisp_state_setup_draw_blend (stdev, state, blt_ins, blt_ack);

  switch (state->pd)
    {
    case STM_BLITTER_PD_CLEAR:
    case STM_BLITTER_PD_SOURCE:
    case STM_BLITTER_PD_DEST:
    case STM_BLITTER_PD_SOURCE_OVER:
    case STM_BLITTER_PD_DEST_OVER:
    case STM_BLITTER_PD_SOURCE_IN:
    case STM_BLITTER_PD_DEST_IN:
    case STM_BLITTER_PD_SOURCE_OUT:
    case STM_BLITTER_PD_DEST_OUT:
    case STM_BLITTER_PD_SOURCE_ATOP:
    case STM_BLITTER_PD_DEST_ATOP:
    case STM_BLITTER_PD_XOR:
      *blt_ack |= ((global_alpha << BLIT_ACK_GLOBAL_ALPHA_SHIFT)
                   | BLIT_ACK_PORTER_DUFF);
      /* Note: we set the premultiplied flag on S2 (the
         source) as the colour will have been pre-multiplied
         manually in bdisp_state_validate_FILLCOLOR(). */
      *blt_ack |= BLIT_ACK_PREMULT_PREMULTIPLIED_S2;
      *blt_ack |= ((state->drawflags & STM_BLITTER_SDF_DST_PREMULTIPLY)
                   ? BLIT_ACK_PREMULT_NOTPREMULTIPLIED_S1
                   : BLIT_ACK_PREMULT_PREMULTIPLIED_S1);
      *blt_ins |= BLIT_INS_SRC1_MODE_MEMORY | BLIT_INS_SRC2_MODE_COLOR_FILL;

      /* set BLT_ACK register for native porter duff rules */
      if (!bdisp_state_setup_native_blending_rules(state, blt_ack))
        return -EINVAL;

      stm_blit_printd (BDisp_State,
                       "  -> %spremultiply destination\n",
                       ((state->drawflags & STM_BLITTER_SDF_DST_PREMULTIPLY)
                        ? ""
                        : "don't "));
      break;

    case STM_BLITTER_PD_NONE: /* this is handled in the non-native case! */
    default:
      stm_blit_printw ("%s: unexpected Porter/Duff rule: %d\n",
                       __func__, state->pd);
      return -EINVAL;
    }

  return 0;
}

static int
bdisp_state_setup_blit_blend_native (struct stm_bdisp2_device_data  * const stdev,
                                     const struct stm_blitter_state * const state,
                                     uint32_t                       * const blt_ins,
                                     uint32_t                       * const blt_ack)
{
  static const uint32_t global_alpha = 128;

  switch (state->pd)
    {
    case STM_BLITTER_PD_CLEAR:
    case STM_BLITTER_PD_SOURCE:
    case STM_BLITTER_PD_DEST:
    case STM_BLITTER_PD_SOURCE_OVER:
    case STM_BLITTER_PD_DEST_OVER:
    case STM_BLITTER_PD_SOURCE_IN:
    case STM_BLITTER_PD_DEST_IN:
    case STM_BLITTER_PD_SOURCE_OUT:
    case STM_BLITTER_PD_DEST_OUT:
    case STM_BLITTER_PD_SOURCE_ATOP:
    case STM_BLITTER_PD_DEST_ATOP:
    case STM_BLITTER_PD_XOR:
      *blt_ack |= ((global_alpha << BLIT_ACK_GLOBAL_ALPHA_SHIFT)
                   | BLIT_ACK_PORTER_DUFF);
      /* set premulticalication flags on S2 */
      *blt_ack |= ((state->blitflags & STM_BLITTER_SBF_SRC_PREMULTIPLY)
                   ? BLIT_ACK_PREMULT_NOTPREMULTIPLIED_S2
                   : BLIT_ACK_PREMULT_PREMULTIPLIED_S2);
      /* set premulticalication flags on S1 */
      *blt_ack |= ((state->blitflags & STM_BLITTER_SBF_DST_PREMULTIPLY)
                   ? BLIT_ACK_PREMULT_NOTPREMULTIPLIED_S1
                   : BLIT_ACK_PREMULT_PREMULTIPLIED_S1);
      *blt_ins |= BLIT_INS_SRC2_MODE_MEMORY;
      stdev->setup->blitstate.blt_ins_src1_mode = BLIT_INS_SRC1_MODE_MEMORY;

      /* set BLT_ACK register for native porter duff rules */
      if (!bdisp_state_setup_native_blending_rules(state, blt_ack))
        return -EINVAL;

      stm_blit_printd (BDisp_State,
                       "  -> %spremultiply source\n",
                       ((state->blitflags & STM_BLITTER_SBF_SRC_PREMULTIPLY)
                        ? ""
                        : "don't "));
      stm_blit_printd (BDisp_State,
                       "  -> %spremultiply destination\n",
                       ((state->blitflags & STM_BLITTER_SBF_DST_PREMULTIPLY)
                        ? ""
                        : "don't "));
      break;

    case STM_BLITTER_PD_NONE: /* this is handled in the non-native case! */
    default:
      stm_blit_printw ("%s: unexpected Porter/Duff rule: %d\n",
                       __func__, state->pd);
      return -EINVAL;
    }

  return 0;
}

static void
bdisp_state_validate_DRAWFLAGS (struct stm_bdisp2_device_data  * const stdev,
                                const struct stm_blitter_state * const state)
{
  uint32_t blt_ins;
  uint32_t blt_ack;

  stm_blit_printd (BDisp_State, "%s drawflags %.2x\n",
                   __func__, state->drawflags);

  blt_ins = stdev->setup->drawstate.ConfigGeneral.BLT_INS;
  blt_ack = stdev->setup->drawstate.ConfigGeneral.BLT_ACK;

  blt_ins &= ~(BLIT_INS_SRC1_MODE_MASK | BLIT_INS_SRC2_MODE_MASK);
  blt_ack &= ~BLIT_ACK_SWAP_FG_BG;
  blt_ack &= ~(BLIT_ACK_MODE_MASK
               | BLIT_ACK_GLOBAL_ALPHA_MASK | BLIT_ACK_ROP_MASK
               | BLIT_ACK_CKEY_MASK | BLIT_ACK_COLORKEYING_MASK
               | BLIT_ACK_BLEND_MODE_MASK
               | BLIT_ACK_S2_PREMULT_MASK | BLIT_ACK_S1_PREMULT_MASK
              );

  if (state->drawflags & STM_BLITTER_SDF_BLEND)
    {
      if (bdisp_state_setup_draw_blend_native (stdev, state,
                                               &blt_ins, &blt_ack))
        return;
    }
  else if (state->drawflags & STM_BLITTER_SDF_XOR)
    {
      stm_blit_printd (BDisp_State, "%s(): XOR\n", __func__);
      blt_ack |= (0
                  | BLIT_ACK_ROP
                  | BLIT_ACK_ROP_XOR
                 );
      blt_ins |= BLIT_INS_SRC1_MODE_MEMORY | BLIT_INS_SRC2_MODE_COLOR_FILL;
    }
  else
    {
      /* fast path */
      if (!stdev->force_slow_path
          && bdisp_ty_formats_identical (stdev->setup->drawstate.color_ty,
                                         stdev->setup->ConfigTarget.BLT_TTY)
          && !(blt_ins & BLIT_INS_ENABLE_CLUT)
          && !(state->drawflags & STM_BLITTER_SDF_DST_COLORKEY))
        {
          stm_blit_printd (BDisp_State, "%s(): fast path\n", __func__);
          blt_ins |= BLIT_INS_SRC1_MODE_DIRECT_FILL;
        }
      else
        {
          blt_ack |= BLIT_ACK_BYPASSSOURCE2;
          blt_ins |= BLIT_INS_SRC1_MODE_DISABLED | BLIT_INS_SRC2_MODE_COLOR_FILL;
        }
    }

  if (state->drawflags & STM_BLITTER_SDF_DST_COLORKEY)
    {
      stdev->setup->drawstate.ConfigGeneral.BLT_CIC |= BLIT_CIC_NODE_COLORKEY;
      blt_ins |= BLIT_INS_ENABLE_COLORKEY;

      blt_ack |= BLIT_ACK_CKEY_RGB_ENABLE;
      blt_ack |= BLIT_ACK_COLORKEYING_DEST;

      /* we should have made sure to never set direct fill if dst colorkey
         was requested. */
      STM_BLITTER_ASSUME (((blt_ins & BLIT_INS_SRC1_MODE_MASK)
                           == BLIT_INS_SRC1_MODE_MEMORY)
                          || ((blt_ins & BLIT_INS_SRC1_MODE_MASK)
                              == BLIT_INS_SRC1_MODE_DISABLED));
      STM_BLITTER_ASSUME ((blt_ins & BLIT_INS_SRC2_MODE_MASK)
                          == BLIT_INS_SRC2_MODE_COLOR_FILL);

      if ((blt_ins & BLIT_INS_SRC1_MODE_MASK) == BLIT_INS_SRC1_MODE_DISABLED)
        blt_ack |= (BLIT_ACK_ROP
                    | BLIT_ACK_ROP_COPY);

      /* clear potential BLIT_INS_SRC1_MODE_DIRECT_FILL */
      blt_ins &= ~BLIT_INS_SRC1_MODE_MASK;
      blt_ins |= BLIT_INS_SRC1_MODE_MEMORY;
    }
  else
    {
      stdev->setup->drawstate.ConfigGeneral.BLT_CIC &= ~BLIT_CIC_NODE_COLORKEY;
      blt_ins &= ~BLIT_INS_ENABLE_COLORKEY;
    }

  stdev->setup->drawstate.ConfigGeneral.BLT_INS = blt_ins;
  stdev->setup->drawstate.ConfigGeneral.BLT_ACK = blt_ack;

  /* done */
  BDISP_STATE_VALIDATED (DRAWFLAGS);
}

#define CLUT_SIZE (256 * sizeof (uint32_t))
#define SET_ARGB_PIXEL(a,r,g,b) (((a) << 24) | \
                                 ((r) << 16) | \
                                 ((g) << 8)  | \
                                  (b))
static void
bdisp_state_validate_BLITCOLOR (struct stm_bdisp2_driver_data  * const stdrv,
                                struct stm_bdisp2_device_data  * const stdev,
                                const struct stm_blitter_state * const state)
{
  stm_blit_printd (BDisp_State,
                   "%s αrgb %.2x%.2x%.2x%.2x\n",
                   __func__, state->colors[0].a, state->colors[0].r,
                   state->colors[0].g, state->colors[0].b);

  if (state->blitflags & (STM_BLITTER_SBF_BLEND_COLORALPHA
                          | STM_BLITTER_SBF_BLEND_ALPHACHANNEL))
    {
      uint32_t                  palette1[256];
      uint32_t                 *palette_pass1 = palette1;
      enum bdisp2_palette_type  palette_type1 = SG2C_NORMAL;
      int                       i;
      /* scale 0..255 to 0..128 (not 127!) */
      uint32_t                  alpha = (state->colors[0].a + 1) / 2;

      switch (state->blitflags & (STM_BLITTER_SBF_BLEND_COLORALPHA
                                  | STM_BLITTER_SBF_BLEND_ALPHACHANNEL))
        {
        case STM_BLITTER_SBF_NONE:
        case STM_BLITTER_SBF_BLEND_ALPHACHANNEL:
          break;

        case STM_BLITTER_SBF_BLEND_COLORALPHA:
          stm_blit_printd (BDisp_State,
                           "  -> BLEND_COLORALPHA: α %.2x\n",
                           state->colors[0].a);
          if (stdev->setup->blitstate.isOptimisedModulation)
            {
              stm_blit_printd (BDisp_State,
                               "    -> optimised using global α\n");

              if (state->src->format & STM_BLITTER_SF_ALPHA)
                stdev->setup->palette_type = SG2C_ONEALPHA_RGB;

              stdev->setup->ConfigGeneral.BLT_ACK &= ~BLIT_ACK_GLOBAL_ALPHA_MASK;
              stdev->setup->ConfigGeneral.BLT_ACK |= ((alpha & 0xff)
                                                      << BLIT_ACK_GLOBAL_ALPHA_SHIFT);
            }
          else
            {
              /* replace alpha using clut */
              stm_blit_printd (BDisp_State,
                               "    -> slow (palette replacement)\n");

              for (i = 0; i <= 0x80; ++i)
                palette1[i] = SET_ARGB_PIXEL (alpha, i, i, i);
              for (; i < 256; ++i)
                palette1[i] = SET_ARGB_PIXEL (0, i, i, i);
              palette_type1 = SG2C_COLORALPHA;
            }
          break;

        case (STM_BLITTER_SBF_BLEND_ALPHACHANNEL
              | STM_BLITTER_SBF_BLEND_COLORALPHA):
          stm_blit_printd (BDisp_State,
                           "  -> BLEND_COLORALPHA | BLEND_ALPHACHANNEL: α %.2x\n",
                           state->colors[0].a);
          if (stdev->setup->blitstate.isOptimisedModulation)
            {
              stm_blit_printd (BDisp_State,
                               "    -> optimised using global α\n");

              stdev->setup->ConfigGeneral.BLT_ACK &= ~BLIT_ACK_GLOBAL_ALPHA_MASK;
              stdev->setup->ConfigGeneral.BLT_ACK |= ((alpha & 0xff)
                                                      << BLIT_ACK_GLOBAL_ALPHA_SHIFT);
            }
          else
            {
              /* replace alpha using clut */
              stm_blit_printd (BDisp_State,
                               "    -> slow (palette replacement)\n");

              for (i = 0; i <= 0x80; ++i)
                palette1[i] = SET_ARGB_PIXEL ((i * alpha / 0x80), i, i, i);
              for (; i < 256; ++i)
                palette1[i] = SET_ARGB_PIXEL (0, i, i, i);
              palette_type1 = SG2C_ALPHA_MUL_COLORALPHA;
            }
          break;
        }
      if (palette_type1 != SG2C_NORMAL)
        {
          uint32_t                  palette2[256];
          uint32_t                 *palette_pass2 = palette2;
          enum bdisp2_palette_type  palette_type2 = SG2C_NORMAL;

          stm_blit_printd (BDisp_State, "    -> palette1: %s\n",
                           bdisp2_get_palette_type_name (palette_type1));

          if (!bdisp2_aq_check_need_native_porter_duff(stdev, state))
            {
              if (state->pd == STM_BLITTER_PD_DEST_IN
                  || state->pd == STM_BLITTER_PD_DEST_OUT)
                {
                  stm_blit_printd (BDisp_State,
                                   "  -> STM_BLITTER_PD_DEST_IN/OUT/ATOP: mod'ing palette(s)\n");

                  for (i = 0; i <= 0x80; ++i)
                    palette2[i] = (0x80 - (palette1[i] >> 24)) << 24;
                  memset (&palette2[i], 0x00, CLUT_SIZE - (i * sizeof (uint32_t)));

                  if (state->pd == STM_BLITTER_PD_DEST_IN)
                    {
                      stm_blit_printd (BDisp_State,
                                       "  -> STM_BLITTER_PD_DEST_IN: palette1 before: %s\n",
                                       bdisp2_get_palette_type_name (palette_type1));

                      if (palette_type1 == SG2C_COLORALPHA)
                        {
                          palette_type1 = SG2C_INVCOLORALPHA;
                          palette_type2 = SG2C_COLORALPHA;
                        }
                      else if (palette_type1 == SG2C_ALPHA_MUL_COLORALPHA)
                        {
                          palette_type1 = SG2C_INV_ALPHA_MUL_COLORALPHA;
                          palette_type2 = SG2C_ALPHA_MUL_COLORALPHA;
                        }

                      palette_pass1 = palette2;
                      palette_pass2 = palette1;

                      stm_blit_printd (BDisp_State,
                                       "  -> STM_BLITTER_PD_DEST_IN: palettes after: %s/%s\n",
                                       bdisp2_get_palette_type_name (palette_type1),
                                       bdisp2_get_palette_type_name (palette_type2));
                    }
                  else if (state->pd == STM_BLITTER_PD_DEST_OUT)
                    {
                      for (i = 0; i <= 0x80; ++i)
                        palette1[i] &= 0xff000000;
                      memset (&palette1[i], 0x00,
                              CLUT_SIZE - (i * sizeof (uint32_t)));

                      stm_blit_printd (BDisp_State,
                                       "  -> STM_BLITTER_PD_DEST_OUT: palette1 before: %s\n",
                                       bdisp2_get_palette_type_name (palette_type1));

                      if (palette_type1 == SG2C_COLORALPHA)
                        palette_type2 = SG2C_INVCOLORALPHA;
                      else if (palette_type1 == SG2C_ALPHA_MUL_COLORALPHA)
                        palette_type2 = SG2C_INV_ALPHA_MUL_COLORALPHA;

                      stm_blit_printd (BDisp_State,
                                       "  -> STM_BLITTER_PD_DEST_OUT: palettes after: %s/%s\n",
                                       bdisp2_get_palette_type_name (palette_type1),
                                       bdisp2_get_palette_type_name (palette_type2));
                    }
                }
            }

          /* upload palette(s) into hardware. We must make sure they are not
             used at the moment, which is why we have to wait until the
             hardware is idle. */
          bdisp2_prepare_upload_palette_hw (stdrv, stdev);

          memcpy (stdrv->clut_virt[palette_type1],
                  palette_pass1, CLUT_SIZE);
          stdev->setup->palette_type = palette_type1;

          if (palette_type2 != SG2C_NORMAL)
            {
              memcpy (stdrv->clut_virt[palette_type2],
                      palette_pass2, CLUT_SIZE);
              stdev->setup->blitstate.extra_passes[0].palette_type = palette_type2;
            }
        }
    }
  /* done */
  BDISP_STATE_VALIDATED (BLITCOLOR);
}

static void
bdisp_state_validate_DST_COLORKEY (struct stm_bdisp2_device_data  * const stdev,
                                   const struct stm_blitter_state * const state)
{
  stm_blit_printd (BDisp_State, "%s\n", __func__);

  _bdisp_state_set_key_color (state->dst->format,
                              &state->dst_ckey[0],
                              &stdev->setup->all_states.dst_ckey[0]);
  _bdisp_state_set_key_color (state->dst->format,
                              &state->dst_ckey[1],
                              &stdev->setup->all_states.dst_ckey[1]);

  if (state->blitflags & STM_BLITTER_SBF_DST_COLORKEY)
    {
      stdev->setup->ConfigColorkey.BLT_KEY1 = stdev->setup->all_states.dst_ckey[0];
      stdev->setup->ConfigColorkey.BLT_KEY2 = stdev->setup->all_states.dst_ckey[1];
    }

  /* done */
  BDISP_STATE_VALIDATED (DST_COLORKEY);
}

static void
bdisp_state_validate_SRC_COLORKEY (struct stm_bdisp2_device_data  * const stdev,
                                   const struct stm_blitter_state * const state)
{
  stm_blit_printd (BDisp_State, "%s\n", __func__);

  _bdisp_state_set_key_color (state->src->format,
                              &state->src_ckey[0],
                              &stdev->setup->blitstate.src_ckey[0]);
  _bdisp_state_set_key_color (state->src->format,
                              &state->src_ckey[1],
                              &stdev->setup->blitstate.src_ckey[1]);

  if (state->blitflags & STM_BLITTER_SBF_SRC_COLORKEY)
    {
      stdev->setup->ConfigColorkey.BLT_KEY1 = stdev->setup->blitstate.src_ckey[0];
      stdev->setup->ConfigColorkey.BLT_KEY2 = stdev->setup->blitstate.src_ckey[1];
    }

  /* done */
  BDISP_STATE_VALIDATED (SRC_COLORKEY);
}

static void
bdisp_state_validate_ROTATION (struct stm_bdisp2_device_data  * const stdev,
                               const struct stm_blitter_state * const state)
{
  uint32_t blt_ins  = stdev->setup->ConfigGeneral.BLT_INS;
  uint32_t blt_tty  = bdisp_ty_sanitise_direction (stdev->setup->ConfigTarget.BLT_TTY);
  uint32_t blt_s1ty = bdisp_ty_sanitise_direction (stdev->setup->ConfigSource1.BLT_S1TY);
  uint32_t blt_s2ty = bdisp_ty_sanitise_direction (stdev->setup->ConfigSource2.BLT_S2TY);

  stm_blit_printd (BDisp_State, "%s\n", __func__);

  stdev->setup->blitstate.rotate = 0;

  if ((state->blitflags & STM_BLITTER_SBF_ROTATE180)
      == STM_BLITTER_SBF_ROTATE180)
    stdev->setup->blitstate.rotate = 180;
  else if (state->blitflags & STM_BLITTER_SBF_FLIP_VERTICAL)
    stdev->setup->blitstate.rotate = 181;
  else if (state->blitflags & STM_BLITTER_SBF_FLIP_HORIZONTAL)
    stdev->setup->blitstate.rotate = 182;
  else if (state->blitflags & STM_BLITTER_SBF_ROTATE90)
    stdev->setup->blitstate.rotate = 90;
  else if (state->blitflags & STM_BLITTER_SBF_ROTATE270)
    stdev->setup->blitstate.rotate = 270;

  stm_blit_printd (BDisp_State, "  -> %d°\n", stdev->setup->blitstate.rotate);

  switch (stdev->setup->blitstate.rotate)
    {
    case 90: /* rotate 90° counter clock wise */
      blt_ins |= BLIT_INS_ENABLE_ROTATION;
      /* read from top left to bottom right */
      blt_s2ty |= (BLIT_TY_COPYDIR_TOPBOTTOM | BLIT_TY_COPYDIR_LEFTRIGHT);
      /* write from bottom left to top right */
      blt_tty |= (BLIT_TY_COPYDIR_BOTTOMTOP | BLIT_TY_COPYDIR_LEFTRIGHT);
      blt_s1ty |= (BLIT_TY_COPYDIR_BOTTOMTOP | BLIT_TY_COPYDIR_LEFTRIGHT);
      break;

    case 270: /* rotate 270° counter clock wise */
      blt_ins |= BLIT_INS_ENABLE_ROTATION;
      /* read from top left to bottom right */
      blt_s2ty |= (BLIT_TY_COPYDIR_TOPBOTTOM | BLIT_TY_COPYDIR_LEFTRIGHT);
      /* write from top right to bottom left */
      blt_tty |= (BLIT_TY_COPYDIR_TOPBOTTOM | BLIT_TY_COPYDIR_RIGHTLEFT);
      blt_s1ty |= (BLIT_TY_COPYDIR_TOPBOTTOM | BLIT_TY_COPYDIR_RIGHTLEFT);
      break;

    default:
      blt_ins &= ~BLIT_INS_ENABLE_ROTATION;
      /* the rest will be updated later in _bdisp_aq_Blit_setup_directions() */
      break;
    }

  stdev->setup->ConfigGeneral.BLT_INS = blt_ins;
  stdev->setup->ConfigTarget.BLT_TTY = blt_tty;
  stdev->setup->ConfigSource1.BLT_S1TY = blt_s1ty;
  stdev->setup->ConfigSource2.BLT_S2TY = blt_s2ty;
  /* rotation for 3 buffer formats is never supported, so always sanitise it */
  stdev->setup->ConfigSource3.BLT_S3TY
    = bdisp_ty_sanitise_direction (stdev->setup->ConfigSource3.BLT_S3TY);

  /* done */
  BDISP_STATE_VALIDATED (ROTATION);
}

static void
bdisp_state_validate_FILTERING (struct stm_bdisp2_device_data  * const stdev,
                                const struct stm_blitter_state * const state)
{
  bool disable_filtering;
  bool forced __attribute__((unused)) = false;

  stm_blit_printd (BDisp_State, "%s\n", __func__);

  disable_filtering = !!(state->blitflags & STM_BLITTER_SBF_NO_FILTER);
  /* We can't disable filtering for (downsampled) YCbCr 4:2:x formats, as
     the chroma upscaling would look very ugly. */
  if (disable_filtering
      && ((state->src->format & STM_BLITTER_SF_YCBCR)
          && (state->src->format != STM_BLITTER_SF_AVYU)
          && (state->src->format != STM_BLITTER_SF_AlVYU)
          && (state->src->format != STM_BLITTER_SF_VYU)
          && (state->src->format != STM_BLITTER_SF_YCBCR444P)))
    {
      disable_filtering = false;
      forced = true;
    }
  /* We have to disable filtering for 1:1 LUT blits as it wouldn't be
     correct to interpolate indexes. */
  if (!disable_filtering
      && (state->dst->format & STM_BLITTER_SF_INDEXED))
    {
      STM_BLITTER_ASSUME (state->src->format == state->dst->format);
      disable_filtering = true;
      forced = true;
    }

  stm_blit_printd (BDisp_State, "  -> %s%s rescale\n",
                   forced ? "forced " : "",
                   disable_filtering ? "resize only" : "smooth");

  stdev->setup->ConfigFilters.BLT_FCTL_RZC &= ~(BLIT_RZC_HF2D_MODE_MASK
                                                | BLIT_RZC_VF2D_MODE_MASK
                                                | BLIT_RZC_Y_HF2D_MODE_MASK
                                                | BLIT_RZC_Y_VF2D_MODE_MASK);
//  stdev->setup->ConfigFilters.BLT_FCTL_RZC &= ~BLIT_RZC_BOUNDARY_BYPASS;
  if (!disable_filtering)
    {
      stdev->setup->ConfigFilters.BLT_FCTL_RZC |= (0
                                                   | BLIT_RZC_HF2D_MODE_FILTER_BOTH
                                                   | BLIT_RZC_VF2D_MODE_FILTER_BOTH
                                                  );
      if (state->src->format & STM_BLITTER_SF_PLANAR)
        stdev->setup->ConfigFilters.BLT_FCTL_RZC |= (0
                                                     | BLIT_RZC_Y_HF2D_MODE_FILTER_BOTH
                                                     | BLIT_RZC_Y_VF2D_MODE_FILTER_BOTH
                                                    );
//      stdev->setup->ConfigFilters.BLT_FCTL_RZC |= BLIT_RZC_BOUNDARY_BYPASS;
    }
  else
    {
      stdev->setup->ConfigFilters.BLT_FCTL_RZC |= (0
                                                   | BLIT_RZC_HF2D_MODE_RESIZE_ONLY
                                                   | BLIT_RZC_VF2D_MODE_RESIZE_ONLY
                                                  );
      if (state->src->format & STM_BLITTER_SF_PLANAR)
        stdev->setup->ConfigFilters.BLT_FCTL_RZC |= (0
                                                     | BLIT_RZC_Y_HF2D_MODE_RESIZE_ONLY
                                                     | BLIT_RZC_Y_VF2D_MODE_RESIZE_ONLY
                                                    );
    }

  /* Flicker Filter mode configuration */
  stdev->setup->blitstate.flicker_filter = false;
  if (state->blitflags & STM_BLITTER_SBF_FLICKER_FILTER)
    {
      stdev->setup->blitstate.flicker_filter = true;
      stdev->setup->ConfigFilters.BLT_FCTL_RZC = 0;

      if (state->ff_mode & STM_BLITTER_FF_ADAPTATIVE)
        stdev->setup->ConfigFilters.BLT_FCTL_RZC |= BLIT_RZC_FF_MODE_ADAPTIVE;

      if (state->ff_mode & STM_BLITTER_FF_FIELD_NOT_FRAME)
        stdev->setup->ConfigFilters.BLT_FCTL_RZC |= BLIT_RZC_FF_FIELD;
    }

  /* strict input rect mode configuration */
  stdev->setup->blitstate.strict_input_rect = false;
  if (state->blitflags & STM_BLITTER_SBF_STRICT_INPUT_RECT)
    {
      stdev->setup->blitstate.strict_input_rect = true;
    }

  /* done */
  BDISP_STATE_VALIDATED (FILTERING);
}

static void
bdisp_state_validate_BLITFLAGS_CKEY (struct stm_bdisp2_device_data  * const stdev,
                                     const struct stm_blitter_state * const state)
{
  uint32_t blt_cic = stdev->setup->ConfigGeneral.BLT_CIC;
  uint32_t blt_ins = stdev->setup->ConfigGeneral.BLT_INS;
  uint32_t blt_ack = stdev->setup->ConfigGeneral.BLT_ACK;

  stm_blit_printd (BDisp_State, "%s\n", __func__);

  blt_ack &= ~(BLIT_ACK_CKEY_MASK
               | BLIT_ACK_COLORKEYING_MASK);

  if (state->blitflags & (STM_BLITTER_SBF_DST_COLORKEY
                          | STM_BLITTER_SBF_SRC_COLORKEY))
    {
      /* can't have both, source and destination color key set */
      STM_BLITTER_ASSERT ((state->blitflags & (STM_BLITTER_SBF_DST_COLORKEY
                                               | STM_BLITTER_SBF_SRC_COLORKEY))
                          != (STM_BLITTER_SBF_DST_COLORKEY
                              | STM_BLITTER_SBF_SRC_COLORKEY));

      blt_cic |= BLIT_CIC_NODE_COLORKEY;
      blt_ins |= BLIT_INS_ENABLE_COLORKEY;

      if (state->blitflags & STM_BLITTER_SBF_SRC_COLORKEY)
        {
          stm_blit_printd (BDisp_State, "  -> SRC_COLORKEY\n");

          /* for source color keying, we have to set up the mode (before or
             after the CLUT translation) depending on the pixelformat.
             Normally, we wouldn't have to worry, but in CLUT correction
             mode, which we use for supporting RGB32, color keying only works
             if done after the CLUT translation. Don't know why, maybe I just
             didn't see the note about that in the documentation. */
          if (!(state->src->format & STM_BLITTER_SF_INDEXED))
            {
              if (state->src_ckey_mode & STM_BLITTER_CKM_B_ENABLE)
                {
                  if ((state->src_ckey_mode & STM_BLITTER_CKM_B_INVERTED)
                      == STM_BLITTER_CKM_B_INVERTED)
                    blt_ack |= BLIT_ACK_CKEY_BLUE_INV_ENABLE;
                  else
                    blt_ack |= BLIT_ACK_CKEY_BLUE_ENABLE;
                }

              if (state->src_ckey_mode & STM_BLITTER_CKM_G_ENABLE)
                {
                  if ((state->src_ckey_mode & STM_BLITTER_CKM_G_INVERTED)
                      == STM_BLITTER_CKM_G_INVERTED)
                    blt_ack |= BLIT_ACK_CKEY_GREEN_INV_ENABLE;
                  else
                    blt_ack |= BLIT_ACK_CKEY_GREEN_ENABLE;
                }

              if (state->src_ckey_mode & STM_BLITTER_CKM_R_ENABLE)
                {
                  if ((state->src_ckey_mode & STM_BLITTER_CKM_R_INVERTED)
                      == STM_BLITTER_CKM_R_INVERTED)
                    blt_ack |= BLIT_ACK_CKEY_RED_INV_ENABLE;
                  else
                    blt_ack |= BLIT_ACK_CKEY_RED_ENABLE;
                }

              blt_ack |= BLIT_ACK_COLORKEYING_SRC_AFTER;
            }
          else
            {
              if (state->src_ckey_mode & STM_BLITTER_CKM_B_ENABLE)
                {
                  /* for LUT formats, the color key (i.e. the index) is specified
                     in the 'blue' bits of the color key register. */
                  if ((state->src_ckey_mode & STM_BLITTER_CKM_B_INVERTED)
                      == STM_BLITTER_CKM_B_INVERTED)
                    blt_ack |= BLIT_ACK_CKEY_BLUE_INV_ENABLE;
                  else
                    blt_ack |= BLIT_ACK_CKEY_BLUE_ENABLE;

                  blt_ack |= BLIT_ACK_COLORKEYING_SRC_BEFORE;
                }
            }

          stdev->setup->ConfigColorkey.BLT_KEY1 = stdev->setup->blitstate.src_ckey[0];
          stdev->setup->ConfigColorkey.BLT_KEY2 = stdev->setup->blitstate.src_ckey[1];
        }
      else
        {
          stm_blit_printd (BDisp_State, "  -> DST_COLORKEY??\n");

          blt_ack |= BLIT_ACK_COLORKEYING_DEST;
          blt_ack |= BLIT_ACK_CKEY_RGB_ENABLE;

          blt_ack &= ~(BLIT_ACK_ROP_MASK | BLIT_ACK_MODE_MASK);
          blt_ack |= BLIT_ACK_ROP_COPY | BLIT_ACK_ROP;
//          blt_ins &= ~BLIT_INS_SRC1_MODE_MASK;
//          blt_ins |= BLIT_INS_SRC1_MODE_MEMORY;
          blt_ins &= ~BLIT_INS_SRC2_MODE_MASK;
          blt_ins |= BLIT_INS_SRC2_MODE_MEMORY;

          stdev->setup->ConfigColorkey.BLT_KEY1 = stdev->setup->all_states.dst_ckey[0];
          stdev->setup->ConfigColorkey.BLT_KEY2 = stdev->setup->all_states.dst_ckey[1];
        }
    }
  else
    {
      blt_cic &= ~BLIT_CIC_NODE_COLORKEY;
      blt_ins &= ~BLIT_INS_ENABLE_COLORKEY;
    }

  stdev->setup->ConfigGeneral.BLT_CIC = blt_cic;
  stdev->setup->ConfigGeneral.BLT_INS = blt_ins;
  stdev->setup->ConfigGeneral.BLT_ACK = blt_ack;

  /* done */
  BDISP_STATE_VALIDATED (BLITFLAGS_CKEY);
}

static void
bdisp_state_validate_BLITFLAGS (struct stm_bdisp2_driver_data  * const stdrv,
                                struct stm_bdisp2_device_data  * const stdev,
                                const struct stm_blitter_state * const state)
{
  uint32_t blt_cic = stdev->setup->ConfigGeneral.BLT_CIC;
  uint32_t blt_ins = stdev->setup->ConfigGeneral.BLT_INS;
  uint32_t blt_ack = stdev->setup->ConfigGeneral.BLT_ACK;

  static const stm_blitter_surface_blitflags_t MODULATION_FLAGS =
    (0
     | STM_BLITTER_SBF_BLEND_ALPHACHANNEL
     | STM_BLITTER_SBF_BLEND_COLORALPHA
     | STM_BLITTER_SBF_DST_PREMULTIPLY /* Might be possible for BLIT_ACK_SWAP_FG_BG */
     | STM_BLITTER_SBF_SRC_PREMULTIPLY
     | STM_BLITTER_SBF_SRC_PREMULTCOLOR
    );
  stm_blitter_surface_blitflags_t modulation;

  uint32_t msk_blt_cic[2];
  uint32_t msk_blt_ins[2];
  uint32_t msk_blt_ack[2];

  int i;

  stm_blit_printd (BDisp_State, "%s\n", __func__);

//  stdev->setup->blitstate.bIndexTranslation = !!(state->blitflags
//                                                 & DSBLIT_INDEX_TRANSLATION);
  stdev->setup->blitstate.bFixedPoint = 0;
  stdev->setup->blitstate.srcxy_fixed_point = 0;
  if ((state->blitflags & STM_BLITTER_SBF_ALL_IN_FIXED_POINT)
      == STM_BLITTER_SBF_ALL_IN_FIXED_POINT)
    stdev->setup->blitstate.bFixedPoint = 1;
  else if (state->blitflags & STM_BLITTER_SBF_SRC_XY_IN_FIXED_POINT)
    stdev->setup->blitstate.srcxy_fixed_point = 1;

  blt_cic &= ~BLIT_CIC_NODE_COLOR;
  blt_ack &= (BLIT_ACK_CKEY_MASK
              | BLIT_ACK_COLORKEYING_MASK);
  blt_ack &= ~BLIT_ACK_SWAP_FG_BG;

  stdev->setup->blitstate.blt_ins_src1_mode = BLIT_INS_SRC1_MODE_DISABLED;
  BDISP_STATE_INVALIDATE (SOURCE_SRC1_MODE);

  blt_cic &= ~BLIT_CIC_NODE_FILTERS;
  blt_ins &= ~BLIT_INS_ENABLE_PLANEMASK;

  blt_cic &= ~BLIT_CIC_NODE_IVMX;
  blt_ins &= ~BLIT_INS_ENABLE_IVMX;

  modulation = state->blitflags & MODULATION_FLAGS;

  stdrv->funcs.blit         = NULL;
  stdrv->funcs.stretch_blit = NULL;
  stdrv->funcs.blit2        = NULL;

  stdev->setup->ConfigClut.BLT_CCO &= ~BLIT_CCO_CLUT_NS2_S1_MASK;

  stdev->setup->blitstate.n_passes = 1;

  stdev->setup->blitstate.src_premultcolor = !!(state->blitflags
                                                & STM_BLITTER_SBF_SRC_PREMULTCOLOR);

  if (modulation)
    {
      bool needs_blend = false;
      bool src_premultiply = !!(state->blitflags
                                & STM_BLITTER_SBF_SRC_PREMULTIPLY);

      blt_ack &= ~BLIT_ACK_GLOBAL_ALPHA_MASK;
      blt_ack |= ((0x80 & 0xff) << BLIT_ACK_GLOBAL_ALPHA_SHIFT);

      if (state->blitflags & (STM_BLITTER_SBF_BLEND_ALPHACHANNEL
                              | STM_BLITTER_SBF_BLEND_COLORALPHA))
        {
          stdev->setup->blitstate.isOptimisedModulation = 1;
          if (bdisp2_aq_check_need_native_porter_duff(stdev, state))
            {
              if (state->blitflags & STM_BLITTER_SBF_BLEND_COLORALPHA
                  && !((state->blitflags & STM_BLITTER_SBF_BLEND_ALPHACHANNEL)
                        || state->blitflags & STM_BLITTER_SBF_SRC_PREMULTIPLY))
                      stdev->setup->blitstate.isOptimisedModulation = 0;

              if (bdisp_state_setup_blit_blend_native (stdev, state,
                                                       &blt_ins, &blt_ack))
                return;
            }
          else
            {
              switch (state->blitflags & (STM_BLITTER_SBF_BLEND_ALPHACHANNEL
                                          | STM_BLITTER_SBF_BLEND_COLORALPHA))
                {
                default:
                case STM_BLITTER_SBF_NONE:
                case STM_BLITTER_SBF_BLEND_ALPHACHANNEL:
                  break;

                case STM_BLITTER_SBF_BLEND_COLORALPHA:
                  /* If we do a premultiply (with colour or source alpha, but not
                     both!), we can simply use the global alpha if the source alpha
                     is 1.0f. It is 1.0f for all surface formats that do not contain
                     an alpha channel as the hardware fills in the alpha for us
                     and for those formats that do have an alpha channel, we can
                     use a (static, hence fast) palette to replace the source alpha
                     by 1.0f. We can also do the optimisation for those obscure
                     blend modes where the source is discarded. */
                  switch (state->pd)
                    {
                    case STM_BLITTER_PD_DEST:
                      if (state->blitflags & STM_BLITTER_SBF_DST_PREMULTIPLY)
                        {
                          stdev->setup->blitstate.isOptimisedModulation = 0;
                          break;
                        }
                      /* fall through */

                    case STM_BLITTER_PD_CLEAR:
                      break;

                    case STM_BLITTER_PD_SOURCE:
                    case STM_BLITTER_PD_SOURCE_OVER:
                      switch (state->blitflags & (STM_BLITTER_SBF_SRC_PREMULTCOLOR
                                                  | STM_BLITTER_SBF_SRC_PREMULTIPLY))
                        {
                        case STM_BLITTER_SBF_NONE:
                          stdev->setup->blitstate.isOptimisedModulation = 0;
                          break;

                        case (STM_BLITTER_SBF_SRC_PREMULTCOLOR
                              | STM_BLITTER_SBF_SRC_PREMULTIPLY):
                          if (!stdev->setup->blitstate.canUseHWInputMatrix)
                            {
                              stdev->setup->blitstate.isOptimisedModulation = 0;
                              break;
                            }
                          /* fall through */

                        case STM_BLITTER_SBF_SRC_PREMULTCOLOR:
                          if (!src_premultiply)
                            {
                              stdev->setup->blitstate.src_premultcolor = 0;
                              BDISP_STATE_INVALIDATE (MATRIXCONVERSIONS);
                            }
                          /* fall through */

                        case STM_BLITTER_SBF_SRC_PREMULTIPLY:
                          needs_blend = true;
                          src_premultiply = false;
                          break;
                        }
                      break;

                    case STM_BLITTER_PD_NONE:
                    case STM_BLITTER_PD_DEST_OVER:
                    case STM_BLITTER_PD_DEST_IN:
                    case STM_BLITTER_PD_DEST_OUT:
                    default:
                      stdev->setup->blitstate.isOptimisedModulation = 0;
                      break;

                    case STM_BLITTER_PD_SOURCE_IN:
                    case STM_BLITTER_PD_SOURCE_OUT:
                    case STM_BLITTER_PD_SOURCE_ATOP:
                    case STM_BLITTER_PD_DEST_ATOP:
                    case STM_BLITTER_PD_XOR:
                      STM_BLITTER_ASSERT ("this should not be reached!\n" == NULL);
                      break;
                    }

                  /* this will set up blt_ack or the palette as needed */
                  BDISP_STATE_INVALIDATE (BLITCOLOR);
                  break;

                case (STM_BLITTER_SBF_BLEND_ALPHACHANNEL
                      | STM_BLITTER_SBF_BLEND_COLORALPHA):
                  /* as an optimisation, we can use the global alpha if we do
                     SRC_PREMULTIPLY or SRC_PREMULTCOLOR or if the source blend
                     function is DSBF_SRCALPHA. */
                  switch (state->pd)
                    {
                    case STM_BLITTER_PD_DEST_OVER:
                    case STM_BLITTER_PD_DEST_IN:
                    case STM_BLITTER_PD_DEST_OUT:
                      stdev->setup->blitstate.isOptimisedModulation = 0;
                      break;

                    case STM_BLITTER_PD_CLEAR:
                    case STM_BLITTER_PD_SOURCE:
                    case STM_BLITTER_PD_DEST:
                    case STM_BLITTER_PD_SOURCE_OVER:
                      if (!(state->blitflags & (STM_BLITTER_SBF_SRC_PREMULTCOLOR
                                                | STM_BLITTER_SBF_SRC_PREMULTIPLY)))
                        {
                          stdev->setup->blitstate.isOptimisedModulation = 0;
                          break;
                        }
                      /* fall through */

                    case STM_BLITTER_PD_NONE:
                      if (stdev->setup->blitstate.src_premultcolor
                          && !stdev->setup->blitstate.canUseHWInputMatrix)
                        {
                          if (src_premultiply
                              || state->pd == STM_BLITTER_PD_NONE)
                            {
                              stdev->setup->blitstate.isOptimisedModulation = 0;
                              break;
                            }
                        }

                      needs_blend = true;
                      if (stdev->setup->blitstate.src_premultcolor
                          && !src_premultiply
                          && state->pd != STM_BLITTER_PD_NONE)
                        {
                          stdev->setup->blitstate.src_premultcolor = 0;
                          BDISP_STATE_INVALIDATE (MATRIXCONVERSIONS);
                        }
                      break;

                    default:
                    case STM_BLITTER_PD_SOURCE_IN:
                    case STM_BLITTER_PD_SOURCE_OUT:
                    case STM_BLITTER_PD_SOURCE_ATOP:
                    case STM_BLITTER_PD_DEST_ATOP:
                    case STM_BLITTER_PD_XOR:
                      STM_BLITTER_ASSERT ("this should not be reached!\n" == NULL);
                      break;
                    }

                  /* this will set up blt_ack or the palette as needed */
                  BDISP_STATE_INVALIDATE (BLITCOLOR);
                  break;
                }

              switch (state->pd)
                {
                case STM_BLITTER_PD_CLEAR:
                  stm_blit_printd (BDisp_State,
                                   "  -> fast direct fill (STM_BLITTER_PD_CLEAR)\n");
                  /* This is a clear, just fill with 0 */
                  stdrv->funcs.blit         = bdisp2_blit_shortcut;
                  stdrv->funcs.stretch_blit = bdisp2_stretch_blit_shortcut;
                  stdrv->funcs.blit2        = bdisp2_blit2_shortcut;
                  goto out;

                case STM_BLITTER_PD_DEST:
                  if (state->blitflags & STM_BLITTER_SBF_DST_PREMULTIPLY)
                    {
                      /* FIXME: untested! */
                      blt_ack |= BLIT_ACK_SWAP_FG_BG;

                      stm_blit_printd (BDisp_State,
                                       "  -> blend dst with src colour zero (STM_BLITTER_PD_DEST)\n");
                      blt_ins &= ~BLIT_INS_SRC2_MODE_MASK;
                      blt_ins |= BLIT_INS_SRC2_MODE_COLOR_FILL;
                      BDISP_STATE_INVALIDATE (SOURCE);

                      stdev->setup->blitstate.blt_ins_src1_mode = BLIT_INS_SRC1_MODE_MEMORY;

                      needs_blend = true;
                      /* FIXME: don't overwrite palette type here? */
                      stdev->setup->palette_type = SG2C_NORMAL;
                    }
                  else
                    {
                      stdrv->funcs.blit         = bdisp2_blit_nop;
                      stdrv->funcs.stretch_blit = bdisp2_stretch_blit_nop;
                      stdrv->funcs.blit2        = bdisp2_blit2_nop;
                      goto out;
                    }
                  break;

                case STM_BLITTER_PD_SOURCE:
                  if (src_premultiply
                      || needs_blend)
                    {
                      /* This is a copy (blend with a destination of (0,0,0,0))
                         after premultiplication with the colour alpha channel. */
                      stm_blit_printd (BDisp_State,
                                       "  -> blend src with dst colour zero (STM_BLITTER_PD_SOURCE)\n");
                      blt_cic |= BLIT_CIC_NODE_COLOR;
                      stdev->setup->blitstate.blt_ins_src1_mode = BLIT_INS_SRC1_MODE_COLOR_FILL;
                      needs_blend = true;
                    }
                  break;

                case STM_BLITTER_PD_DEST_OVER:
                  blt_ack |= BLIT_ACK_SWAP_FG_BG;
                case STM_BLITTER_PD_SOURCE_OVER:
                  stm_blit_printd (BDisp_State,
                                   "  -> blend with dst memory (STM_BLITTER_PD_%s_OVER)\n",
                                   ((state->pd == STM_BLITTER_PD_SOURCE_OVER)
                                    ? "SRC" : "DST"));
                  stdev->setup->blitstate.blt_ins_src1_mode = BLIT_INS_SRC1_MODE_MEMORY;
                  needs_blend = true;
                  break;

                case STM_BLITTER_PD_DEST_IN:
                  stm_blit_printd (BDisp_State,
                                   "  -> blend with dst memory (STM_BLITTER_PD_DEST_IN)\n");
                  stdev->setup->blitstate.blt_ins_src1_mode = BLIT_INS_SRC1_MODE_MEMORY;
                  if (!stdev->setup->blitstate.isOptimisedModulation)
                    BDISP_STATE_INVALIDATE (BLITCOLOR);
                  else
                    {
                      stm_blit_printd (BDisp_State,
                                       "    -> SG2C_INVALPHA_ZERORGB\n");
                      if (stdev->setup->palette_type == SG2C_ONEALPHA_RGB)
                        {
                          stdev->setup->palette_type = SG2C_ZEROALPHA_ZERORGB;
                          stdev->setup->blitstate.extra_passes[0].palette_type = SG2C_ONEALPHA_RGB;
                        }
                      else
                        {
                          stdev->setup->palette_type = SG2C_INVALPHA_ZERORGB;
                          stdev->setup->blitstate.extra_passes[0].palette_type = SG2C_NORMAL;
                        }
                    }
                  needs_blend = true;
                  /* need the plane mask in the first pass */
                  stdev->setup->blitstate.extra_passes[0].ConfigGeneral.BLT_CIC = 0;
                  stdev->setup->blitstate.extra_passes[0].ConfigGeneral.BLT_INS = 0;
                  stdev->setup->blitstate.extra_passes[0].ConfigGeneral.BLT_ACK = BLIT_ACK_BLEND_CLIPMASK_BLEND;
                  blt_cic |= BLIT_CIC_NODE_FILTERS;
                  blt_ins |= BLIT_INS_ENABLE_PLANEMASK;
                  msk_blt_cic[0] = ~BLIT_CIC_NODE_FILTERS;
                  msk_blt_ins[0] = ~BLIT_INS_ENABLE_PLANEMASK;
                  msk_blt_ack[0] = ~BLIT_ACK_MODE_MASK;
                  stdev->setup->blitstate.n_passes = 2;
                  break;

                case STM_BLITTER_PD_DEST_OUT:
                  stm_blit_printd (BDisp_State,
                                   "  -> blend with dst memory (STM_BLITTER_PD_DEST_OUT)\n");
                  stdev->setup->blitstate.blt_ins_src1_mode = BLIT_INS_SRC1_MODE_MEMORY;
                  if (!stdev->setup->blitstate.isOptimisedModulation)
                    BDISP_STATE_INVALIDATE (BLITCOLOR);
                  else
                    {
                      stm_blit_printd (BDisp_State, "    -> normal palettes\n");
                      if (stdev->setup->palette_type == SG2C_ONEALPHA_RGB)
                        {
                          stdev->setup->palette_type = SG2C_ONEALPHA_ZERORGB;
                          stdev->setup->blitstate.extra_passes[0].palette_type = SG2C_ZEROALPHA_ZERORGB;
                        }
                      else
                        {
                          stdev->setup->palette_type = SG2C_ALPHA_ZERORGB;
                          stdev->setup->blitstate.extra_passes[0].palette_type = SG2C_INVALPHA_ZERORGB;
                        }
                    }
                  needs_blend = true;
                  /* need the plane mask in the first pass */
                  stdev->setup->blitstate.extra_passes[0].ConfigGeneral.BLT_CIC = 0;
                  stdev->setup->blitstate.extra_passes[0].ConfigGeneral.BLT_INS = 0;
                  stdev->setup->blitstate.extra_passes[0].ConfigGeneral.BLT_ACK = BLIT_ACK_BLEND_CLIPMASK_BLEND;
                  blt_cic |= BLIT_CIC_NODE_FILTERS;
                  blt_ins |= BLIT_INS_ENABLE_PLANEMASK;
                  msk_blt_cic[0] = ~BLIT_CIC_NODE_FILTERS;
                  msk_blt_ins[0] = ~BLIT_INS_ENABLE_PLANEMASK;
                  msk_blt_ack[0] = ~BLIT_ACK_MODE_MASK;
                  stdev->setup->blitstate.n_passes = 2;
                  break;

                case STM_BLITTER_PD_NONE:
                  /* Must be premulitply off, dest no alpha channel. */
                  stm_blit_printd (BDisp_State,
                                   "  -> blend with dst memory (STM_BLITTER_PD_NONE)\n");
                  stdev->setup->blitstate.blt_ins_src1_mode = BLIT_INS_SRC1_MODE_MEMORY;
                  needs_blend = true;
                  break;

                case STM_BLITTER_PD_SOURCE_IN:
                case STM_BLITTER_PD_SOURCE_OUT:
                case STM_BLITTER_PD_SOURCE_ATOP:
                case STM_BLITTER_PD_DEST_ATOP:
                case STM_BLITTER_PD_XOR:
                default:
                  stm_blit_printw ("unsupported Porter/Duff rule: %d flags: %.8x",
                                   state->pd, state->blitflags);
                  needs_blend = true;
                  break;
                }
            }
        }
      else if (state->blitflags & (STM_BLITTER_SBF_SRC_PREMULTIPLY
                                   | STM_BLITTER_SBF_DST_PREMULTIPLY))
        {
          /* This is a copy (blend with a destination of (0,0,0,0))
             after premultiplication with the colour alpha channel. */
          if (state->blitflags & STM_BLITTER_SBF_DST_PREMULTIPLY)
            /* FIXME: see STM_BLITTER_PD_DEST case. */
            blt_ack |= BLIT_ACK_SWAP_FG_BG;

          stm_blit_printd (BDisp_State,
                           "  -> %s premultiply\n",
                           ((state->blitflags & STM_BLITTER_SBF_SRC_PREMULTIPLY)
                            ? "src" : "dst"));
          blt_cic |= BLIT_CIC_NODE_COLOR;
          stdev->setup->blitstate.blt_ins_src1_mode = BLIT_INS_SRC1_MODE_COLOR_FILL;
          needs_blend = true;
        }

      if (needs_blend)
        {
          /* CheckState guarantees that only one of SRC_PREMULTIPLY
             DST_PREMULTIPLY is set, depending on BLIT_ACK_SWAP_FG_BG */
          blt_ack |= ((src_premultiply
                       || (state->blitflags & STM_BLITTER_SBF_DST_PREMULTIPLY)
                       || (state->pd == STM_BLITTER_PD_NONE))
                       ? BLIT_ACK_BLEND_SRC2_N_PREMULT
                       : BLIT_ACK_BLEND_SRC2_PREMULT);
        }
    }

  if (state->blitflags & STM_BLITTER_SBF_XOR)
    {
      stm_blit_printd (BDisp_State, "  -> XOR\n");
      blt_ack &= ~BLIT_ACK_ROP_MASK;
      blt_ack |= (0
                  | BLIT_ACK_ROP
                  | BLIT_ACK_ROP_XOR
                 );
      stdev->setup->blitstate.blt_ins_src1_mode = BLIT_INS_SRC1_MODE_MEMORY;
    }

  if (state->blitflags & STM_BLITTER_SBF_READ_SOURCE2)
    {
      stm_blit_printd (BDisp_State, "  -> SOURCE2\n");

      STM_BLITTER_ASSERT (stdev->setup->blitstate.blt_ins_src1_mode !=
                          BLIT_INS_SRC1_MODE_COLOR_FILL);
      stdev->setup->blitstate.blt_ins_src1_mode = BLIT_INS_SRC1_MODE_MEMORY;
    }

  stdev->setup->blitstate.bDeInterlacingTop
    = !!(state->blitflags & STM_BLITTER_SBF_DEINTERLACE_TOP);

  stdev->setup->blitstate.bDeInterlacingBottom
    = !!(state->blitflags & STM_BLITTER_SBF_DEINTERLACE_BOTTOM);

  if (stdev->setup->blitstate.bDeInterlacingBottom)
     stm_blit_printd (BDisp_State, "  -> DEINTERLACE_BOTTOM\n");

  if (stdev->setup->blitstate.bDeInterlacingTop)
     stm_blit_printd (BDisp_State, "  -> DEINTERLACE_TOP\n");

  if (!(blt_ack & BLIT_ACK_MODE_MASK))
    blt_ack |= BLIT_ACK_BYPASSSOURCE2;
  stdev->setup->ConfigGeneral.BLT_CIC = blt_cic;
  stdev->setup->ConfigGeneral.BLT_INS = blt_ins;
  stdev->setup->ConfigGeneral.BLT_ACK = blt_ack;

  stdev->setup->do_expand = false;
  if (((state->src->format & STM_BLITTER_SF_INDEXED)
       && !(state->dst->format & STM_BLITTER_SF_INDEXED)))
    stdev->setup->do_expand = true;

  for (i = 0; i < (stdev->setup->blitstate.n_passes - 1); ++i)
    {
      blt_cic &= msk_blt_cic[i];
      blt_ins &= msk_blt_ins[i];
      blt_ack &= msk_blt_ack[i];

      blt_cic &= ~stdev->setup->blitstate.extra_passes[i].ConfigGeneral.BLT_CIC;
      blt_ins &= ~stdev->setup->blitstate.extra_passes[i].ConfigGeneral.BLT_INS;
      blt_ack &= ~stdev->setup->blitstate.extra_passes[i].ConfigGeneral.BLT_ACK;

      stdev->setup->blitstate.extra_passes[i].ConfigGeneral.BLT_CIC |= blt_cic;
      if (stdev->setup->blitstate.blt_ins_src1_mode != BLIT_INS_SRC1_MODE_DISABLED)
        stdev->setup->blitstate.extra_passes[i].ConfigGeneral.BLT_CIC |= BLIT_CIC_NODE_SOURCE1;
      stdev->setup->blitstate.extra_passes[i].ConfigGeneral.BLT_INS |= blt_ins;
      stdev->setup->blitstate.extra_passes[i].ConfigGeneral.BLT_INS |= stdev->setup->blitstate.blt_ins_src1_mode;
      stdev->setup->blitstate.extra_passes[i].ConfigGeneral.BLT_ACK |= blt_ack;
    }

out:
  /* done */
  BDISP_STATE_VALIDATED (BLITFLAGS);
}

static void
bdisp_state_validate_INPUTMATRIX (struct stm_bdisp2_device_data  * const stdev,
                                  const struct stm_blitter_state * const state)
{
  bool t_is_YCbCr = !!BLIT_TY_COLOR_FORM_IS_YUV (stdev->setup->ConfigTarget.BLT_TTY);
  bool s2_is_YCbCr = !!BLIT_TY_COLOR_FORM_IS_YUV (stdev->setup->ConfigSource2.BLT_S2TY);

  stm_blit_printd (BDisp_State, "%s\n", __func__);

  stdev->setup->blitstate.canUseHWInputMatrix = !!(t_is_YCbCr == s2_is_YCbCr);

  /* done */
  BDISP_STATE_VALIDATED (INPUTMATRIX);
}

static void
bdisp_state_validate_MATRIXCONVERSIONS (struct stm_bdisp2_device_data  * const stdev,
                                        const struct stm_blitter_state * const state)
{
  uint32_t blt_cic = stdev->setup->ConfigGeneral.BLT_CIC;
  uint32_t blt_ins = stdev->setup->ConfigGeneral.BLT_INS;

  stm_blit_printd (BDisp_State, "%s\n", __func__);

  blt_cic &= ~BLIT_CIC_NODE_IVMX;
  blt_ins &= ~BLIT_INS_ENABLE_IVMX;

  if (BLIT_TY_COLOR_FORM_IS_ALPHA (stdev->setup->ConfigSource2.BLT_S2TY)
      || (state->blitflags & STM_BLITTER_SBF_SRC_COLORIZE)
      || stdev->setup->blitstate.src_premultcolor)
    {
      uint32_t red, green, blue;
      uint32_t extra = 0;

      if (!BLIT_TY_COLOR_FORM_IS_YUV (stdev->setup->ConfigSource2.BLT_S2TY))
        if (BLIT_TY_COLOR_FORM_IS_YUV (stdev->setup->ConfigTarget.BLT_TTY))
          {
            stm_blit_printd (BDisp_State, "  -> Output RGB->YCbCr601\n");

            blt_cic |= BLIT_CIC_NODE_OVMX;
            blt_ins |= BLIT_INS_ENABLE_OVMX;
	    set_matrix (stdev->setup->ConfigOVMX.BLT_OVMX, bdisp_aq_RGB_2_VideoYCbCr601);
          }

      blt_cic |= BLIT_CIC_NODE_IVMX;
      blt_ins |= BLIT_INS_ENABLE_IVMX;

      if (state->blitflags & STM_BLITTER_SBF_SRC_COLORIZE)
        {
          stm_blit_printd (BDisp_State, "  -> COLORIZE: %.2x%.2x%.2x\n",
                           state->colors[0].r, state->colors[0].g,
                           state->colors[0].b);

          if (state->dst->format == STM_BLITTER_SF_ABGR)
            {
              blue  = (state->colors[0].r * 256) / 255;
              green = (state->colors[0].g * 256) / 255;
              red   = (state->colors[0].b * 256) / 255;
            }
          else
            {
              red   = (state->colors[0].r * 256) / 255;
              green = (state->colors[0].g * 256) / 255;
              blue  = (state->colors[0].b * 256) / 255;
            }

          if (BLIT_TY_COLOR_FORM_IS_YUV (stdev->setup->ConfigSource2.BLT_S2TY))
            {
              int cr = red;
              int y  = green;
              int cb = blue;

              /* this is for brightness, contrast and saturation:
                 - brigthness is adjusted by passing 0 <= brighness <= 255,
                   with Y (green) = brightness and R = B = 255
                 - contrast by passing in 0 <= contrast <= 255,
                   with R = G = B = contrast
                 - saturation by passing in 0 <= saturation <= 255,
                   with G = 255 and R = B = saturation
                 This does not allow to increase any of these above the
                 original values.

                 For contrast, what we need is
                 Y  = Contrast * (Y  - 128) + 128
                 Cb = Contrast * (Cb - 128) + 128
                 Cr = Contrast * (Cr - 128) + 128

                 which can be re-written as
                 Cb = Contrast * Cb - 128 * Contrast + 128 etc.

                 This is what happens here - the Contrast * Cb is done using
                 the xVMX2, prepared above, and -128 * Contrast + 128 we
                 prepare here (xVMX3).

                 The same goes for saturation and brightness.
              */
              int extra_cr = (((-128 * cr / 256) + 128) & 0x3ff) << 20;
              int extra_y  = (((-128 *  y / 256) + 128) & 0x3ff) << 10;
              int extra_cb = (((-128 * cb / 256) + 128) & 0x3ff) <<  0;

              extra = extra_cr | extra_y | extra_cb;
            }
        }
      else
        red = green = blue = 256;

      if (stdev->setup->blitstate.src_premultcolor)
        {
          /* scale 0..255 to 0..256 */
          uint32_t alpha = (state->colors[0].a * 256) / 255;

          stm_blit_printd (BDisp_State,
                           "  -> SRC_PREMULTCOLOR α: %.2x\n", alpha);

          red   = (red   * alpha) / 256;
          green = (green * alpha) / 256;
          blue  = (blue  * alpha) / 256;
        }

      if (BLIT_TY_COLOR_FORM_IS_ALPHA (stdev->setup->ConfigSource2.BLT_S2TY))
        {
          /* A8 and A1 surface types, we need to add the calculated colour as
             the hardware always expands to black with alpha (what use is
             that?!) */
          stm_blit_printd (BDisp_State,
                           "  -> Input α x RGB replacement (%.2x%.2x%.2x)\n",
                           red, green, blue);

          stdev->setup->ConfigIVMX.BLT_IVMX0 = 0;
          stdev->setup->ConfigIVMX.BLT_IVMX1 = 0;
          stdev->setup->ConfigIVMX.BLT_IVMX2 = 0;
          stdev->setup->ConfigIVMX.BLT_IVMX3 = (red << 20) | (green << 10) | blue;
        }
      else /* if (!BLIT_TY_COLOR_FORM_IS_MISC (stdev->setup->ConfigSource2.BLT_S2TY)) */
        {
          /* RGB surface types, do a real modulation */
          stm_blit_printd (BDisp_State,
                           "  -> Input RGB modulation (%.2x%.2x%.2x)\n",
                           red, green, blue);

          stdev->setup->ConfigIVMX.BLT_IVMX0 = red   << 21;
          stdev->setup->ConfigIVMX.BLT_IVMX1 = green << 10;
          stdev->setup->ConfigIVMX.BLT_IVMX2 = blue;
          stdev->setup->ConfigIVMX.BLT_IVMX3 = extra;
        }
      /* else RLE - not reached (hopefully :-) */
    }
  else if (state->dst->format == STM_BLITTER_SF_ABGR)
    {
      if (!BLIT_TY_COLOR_FORM_IS_YUV (stdev->setup->ConfigSource2.BLT_S2TY))
        {
          stm_blit_printd (BDisp_State, "  -> Input RGB -> BGR\n");

          blt_cic |= BLIT_CIC_NODE_IVMX;
          blt_ins |= BLIT_INS_ENABLE_IVMX;

          set_matrix (stdev->setup->ConfigIVMX.BLT_IVMX, bdisp_aq_RGB_2_GfxBGR);
        }
      else
        {
          stm_blit_printd (BDisp_State, "  -> Input YUV -> BGR\n");

          blt_cic |= BLIT_CIC_NODE_IVMX;
          blt_ins |= BLIT_INS_ENABLE_IVMX;

          set_matrix (stdev->setup->ConfigIVMX.BLT_IVMX, bdisp_aq_YCbCr601_2_GfxBGR);
        }
    }
  else
    {
      if (BLIT_TY_COLOR_FORM_IS_YUV (stdev->setup->ConfigTarget.BLT_TTY)
          && !BLIT_TY_COLOR_FORM_IS_YUV (stdev->setup->ConfigSource2.BLT_S2TY))
        {
          stm_blit_printd (BDisp_State, "  -> Input RGB->YCbCr601\n");

          blt_cic |= BLIT_CIC_NODE_IVMX;
          blt_ins |= BLIT_INS_ENABLE_IVMX;

          set_matrix (stdev->setup->ConfigIVMX.BLT_IVMX, bdisp_aq_RGB_2_VideoYCbCr601);
        }
      else if (!BLIT_TY_COLOR_FORM_IS_YUV (stdev->setup->ConfigTarget.BLT_TTY)
               && BLIT_TY_COLOR_FORM_IS_YUV (stdev->setup->ConfigSource2.BLT_S2TY))
        {
          stm_blit_printd (BDisp_State, "  -> Input YCbCr601->RGB\n");

          blt_cic |= BLIT_CIC_NODE_IVMX;
          blt_ins |= BLIT_INS_ENABLE_IVMX;

          set_matrix (stdev->setup->ConfigIVMX.BLT_IVMX,
                      bdisp_aq_VideoYCbCr601_2_RGB);
        }
    }

  stdev->setup->ConfigGeneral.BLT_CIC = blt_cic;
  stdev->setup->ConfigGeneral.BLT_INS = blt_ins;

  BDISP_STATE_VALIDATED (MATRIXCONVERSIONS);
}


void
bdisp2_state_update (struct stm_bdisp2_driver_data * const stdrv,
                     struct stm_bdisp2_device_data * const stdev,
                     struct stm_blitter_state      * const state,
                     enum stm_blitter_accel         accel)
{
  enum stm_blitter_state_modification_flags modified = state->modified;

  bool support_hw_clip = false;

  _dump_set_state (__func__, state, accel);

  stm_blit_printd (BDisp_State,
                   "%s: accel %d %.8x\n", __func__,
                   accel, state->modified);

  /* Simply invalidate all? */
  if (modified == STM_BLITTER_SMF_ALL)
    BDISP_STATE_INVALIDATE (ALL);
  else if (likely (modified))
    {
      /* Invalidate destination registers. */
      if (modified & STM_BLITTER_SMF_DST)
        {
          /* make sure that the color gets updated when the destination
             surface format changes, as we have to modify the color for
             fills. */
          /* FIXME: we can be more intelligent about the destination
             address, only. */
          BDISP_STATE_INVALIDATE (DESTINATION | SOURCE_SRC1_MODE
                                  | FILLCOLOR | DST_COLORKEY
                                  | PALETTE_DRAW | PALETTE_BLIT
                                  | DRAWFLAGS
                                  | BLITFLAGS);
        }
      else
        {
          if (modified & (STM_BLITTER_SMF_DRAWFLAGS
                          | STM_BLITTER_SMF_PORTER_DUFF))
            /* make sure that the color gets updated when the drawing
               flags change, as we have to modify the color for fills. */
            BDISP_STATE_INVALIDATE (FILLCOLOR | DRAWFLAGS | PALETTE_DRAW);

          if (modified & STM_BLITTER_SMF_DST_COLORKEY)
            BDISP_STATE_INVALIDATE (DST_COLORKEY);

          if (modified & (STM_BLITTER_SMF_BLITFLAGS
                          | STM_BLITTER_SMF_PORTER_DUFF))
            BDISP_STATE_INVALIDATE (PALETTE_BLIT);
        }

      if (modified & STM_BLITTER_SMF_COLOR)
        BDISP_STATE_INVALIDATE (FILLCOLOR | BLITCOLOR | MATRIXCONVERSIONS);
      else if (modified & STM_BLITTER_SMF_BLITFLAGS)
        BDISP_STATE_INVALIDATE (BLITCOLOR | MATRIXCONVERSIONS);

      /* Invalidate clipping registers. */
      if (modified & STM_BLITTER_SMF_CLIP)
        BDISP_STATE_INVALIDATE (CLIP);

      if (modified & STM_BLITTER_SMF_SRC)
        BDISP_STATE_INVALIDATE (SOURCE | SOURCE_SRC1_MODE
                                | SRC_COLORKEY
                                | PALETTE_BLIT
                                | BLITFLAGS | BLITFLAGS_CKEY
                                | FILTERING);
      else
        {
          if (modified & STM_BLITTER_SMF_SRC_COLORKEY)
            BDISP_STATE_INVALIDATE (SRC_COLORKEY);

          if (modified & STM_BLITTER_SMF_FILTERING)
            BDISP_STATE_INVALIDATE (FILTERING);

          if (modified & STM_BLITTER_SMF_BLITFLAGS)
            BDISP_STATE_INVALIDATE (SOURCE_SRC1_MODE
                                    | BLITFLAGS | BLITFLAGS_CKEY);
          else if (modified & STM_BLITTER_SMF_PORTER_DUFF)
            BDISP_STATE_INVALIDATE (BLITFLAGS | BLITFLAGS_CKEY);

          if (modified & STM_BLITTER_SMF_SRC2)
            BDISP_STATE_INVALIDATE (SOURCE_SRC1_MODE);
        }

      if (modified & (STM_BLITTER_SMF_BLITFLAGS
                      | STM_BLITTER_SMF_MATRIX))
        BDISP_STATE_INVALIDATE (ROTATION);
    }

  BDISP_STATE_CHECK_N_VALIDATE (DESTINATION);

  BDISP_STATE_CHECK_N_VALIDATE (FILLCOLOR);

  BDISP_STATE_CHECK_N_VALIDATE (DST_COLORKEY);

  switch (accel)
    {
    case STM_BLITTER_ACCEL_FILLRECT:
    case STM_BLITTER_ACCEL_DRAWRECT:
      stm_blit_printd (BDisp_State,
                       "%s(): fill/draw op\n", __func__);

      state->set &= ~(STM_BLITTER_ACCEL_BLIT | STM_BLITTER_ACCEL_STRETCHBLIT
                      | STM_BLITTER_ACCEL_BLIT2);

      BDISP_STATE_CHECK_N_VALIDATE (PALETTE_DRAW);
      BDISP_STATE_CHECK_N_VALIDATE (DRAWFLAGS);

      if (stdev->setup->drawstate.ConfigGeneral.BLT_INS == BLIT_INS_SRC1_MODE_DIRECT_FILL)
        {
          /* this is a simple solid fill that can use the 64bit fast path
             in the blitter, which supports the output of multiple pixels
             per blitter clock cycle. */
          stm_blit_printd (BDisp_State,
                           "%s(): fast direct fill/draw\n", __func__);

          stdrv->funcs.fill_rect = bdisp2_fill_rect_simple;
          stdrv->funcs.draw_rect = bdisp2_draw_rect_simple;
        }
      else if (unlikely (((stdev->setup->drawstate.ConfigGeneral.BLT_INS & BLIT_INS_SRC2_MODE_MASK)
                          == BLIT_INS_SRC2_MODE_DISABLED)
                         && ((stdev->setup->drawstate.ConfigGeneral.BLT_INS & BLIT_INS_SRC1_MODE_MASK)
                             == BLIT_INS_SRC1_MODE_DISABLED)))
        {
          /* We don't need to do the operation as it would be a nop... */
          stm_blit_printd (BDisp_State,
                           "%s(): fill/draw nop\n", __func__);

          stdrv->funcs.fill_rect = bdisp2_fill_draw_nop;
          stdrv->funcs.draw_rect = bdisp2_fill_draw_nop;

          /* ...hence we might as well announce support for clipping. */
          support_hw_clip = true;
        }
      else
        {
          /* this is a fill which needs to use the source 2 data path for
             the fill color. This path is limited to one pixel output per
             blitter clock cycle... */
          stm_blit_printd (BDisp_State,
                           "%s(): complex fill/draw\n", __func__);

          STM_BLITTER_ASSUME ((stdev->setup->drawstate.ConfigGeneral.BLT_INS
                               & BLIT_INS_SRC1_MODE_MASK)
                              != BLIT_INS_SRC1_MODE_DIRECT_FILL);

          stdrv->funcs.fill_rect = bdisp2_fill_rect;
          stdrv->funcs.draw_rect = bdisp2_draw_rect;

          /* ...but it supports clipping. */
          support_hw_clip = true;
        }

      state->set |= (STM_BLITTER_ACCEL_FILLRECT | STM_BLITTER_ACCEL_DRAWRECT);
      break;

    case STM_BLITTER_ACCEL_BLIT:
    case STM_BLITTER_ACCEL_BLIT2:
    case STM_BLITTER_ACCEL_STRETCHBLIT:
      stm_blit_printd (BDisp_State,
                       "%s(): blit/stretch/blit2 op\n", __func__);

      state->set &= ~(STM_BLITTER_ACCEL_FILLRECT | STM_BLITTER_ACCEL_DRAWRECT);

      /* SRC1 mode depends on the acceleration function in addition to the
         blitting flags, therefore we need to invalidate this every time! */
      BDISP_STATE_INVALIDATE (SOURCE_SRC1_MODE);
      BDISP_STATE_CHECK_N_VALIDATE (SOURCE);
      BDISP_STATE_CHECK_N_VALIDATE (INPUTMATRIX);
      BDISP_STATE_CHECK_N_VALIDATE (SRC_COLORKEY);
      BDISP_STATE_CHECK_N_VALIDATE (FILTERING);
      BDISP_STATE_CHECK_N_VALIDATE (PALETTE_BLIT);
      BDISP_STATE_CHECK_N_VALIDATE2 (BLITFLAGS);
      BDISP_STATE_CHECK_N_VALIDATE (BLITFLAGS_CKEY);
      BDISP_STATE_CHECK_N_VALIDATE2 (BLITCOLOR);
      BDISP_STATE_CHECK_N_VALIDATE3 (SOURCE_SRC1_MODE);
      BDISP_STATE_CHECK_N_VALIDATE (MATRIXCONVERSIONS);
      BDISP_STATE_CHECK_N_VALIDATE (ROTATION);

      if (unlikely (stdrv->funcs.blit == bdisp2_blit_nop))
        {
          /* STM_BLITTER_PD_DEST blend was requested. */
          STM_BLITTER_ASSERT (stdrv->funcs.stretch_blit == bdisp2_stretch_blit_nop);
          STM_BLITTER_ASSERT (stdrv->funcs.blit2 == bdisp2_blit2_nop);
          state->set |= (STM_BLITTER_ACCEL_BLIT
                         | STM_BLITTER_ACCEL_STRETCHBLIT
                         | STM_BLITTER_ACCEL_BLIT2);
        }
      else if (unlikely (stdrv->funcs.blit == bdisp2_blit_shortcut))
        {
          /* STM_BLITTER_PD_CLEAR 'blend' was requested. */
          STM_BLITTER_ASSERT (stdrv->funcs.stretch_blit == bdisp2_stretch_blit_shortcut);
          STM_BLITTER_ASSERT (stdrv->funcs.blit2 == bdisp2_blit2_shortcut);
          switch (state->dst->format)
            {
            case STM_BLITTER_SF_YUY2:
            case STM_BLITTER_SF_UYVY:
              stdrv->funcs.blit         = bdisp2_blit_shortcut_YCbCr422r;
              stdrv->funcs.stretch_blit = bdisp2_stretch_blit_shortcut_YCbCr422r;
              stdrv->funcs.blit2        = bdisp2_blit2_shortcut_YCbCr422r;
              break;

            case STM_BLITTER_SF_RGB32:
              if (stdev->features.hw.rgb32)
                break;
              stdrv->funcs.blit         = bdisp2_blit_shortcut_rgb32;
              stdrv->funcs.stretch_blit = bdisp2_stretch_blit_shortcut_rgb32;
              stdrv->funcs.blit2        = bdisp2_blit2_shortcut_rgb32;
              break;

            case STM_BLITTER_SF_RGB565:
            case STM_BLITTER_SF_RGB24:
            case STM_BLITTER_SF_BGR24:
            case STM_BLITTER_SF_ARGB1555:
            case STM_BLITTER_SF_ARGB4444:
            case STM_BLITTER_SF_ARGB8565:
            case STM_BLITTER_SF_AlRGB8565:
            case STM_BLITTER_SF_ARGB:
            case STM_BLITTER_SF_AlRGB:
            case STM_BLITTER_SF_BGRA:
            case STM_BLITTER_SF_BGRAl:
            case STM_BLITTER_SF_ABGR:
            case STM_BLITTER_SF_LUT8:
            case STM_BLITTER_SF_LUT4:
            case STM_BLITTER_SF_LUT2:
            case STM_BLITTER_SF_LUT1:
            case STM_BLITTER_SF_ALUT88:
            case STM_BLITTER_SF_AlLUT88:
            case STM_BLITTER_SF_ALUT44:
            case STM_BLITTER_SF_A8:
            case STM_BLITTER_SF_Al8:
            case STM_BLITTER_SF_A1:
            case STM_BLITTER_SF_AVYU:
            case STM_BLITTER_SF_AlVYU:
            case STM_BLITTER_SF_VYU:
            case STM_BLITTER_SF_YV12:
            case STM_BLITTER_SF_I420:
            case STM_BLITTER_SF_YV16:
            case STM_BLITTER_SF_YV61:
            case STM_BLITTER_SF_YCBCR444P:
            case STM_BLITTER_SF_NV12:
            case STM_BLITTER_SF_NV21:
            case STM_BLITTER_SF_NV16:
            case STM_BLITTER_SF_NV61:
            case STM_BLITTER_SF_YCBCR420MB:
            case STM_BLITTER_SF_YCBCR422MB:
            case STM_BLITTER_SF_NV24:
              break;

            /* these are not supported via the normal API */
            case STM_BLITTER_SF_RLD_BD:
            case STM_BLITTER_SF_RLD_H2:
            case STM_BLITTER_SF_RLD_H8:
            case STM_BLITTER_SF_INVALID:
            case STM_BLITTER_SF_COUNT:
            default:
              STM_BLITTER_ASSERT ("this should not be reached" == NULL);
              break;
            }
          state->set |= (STM_BLITTER_ACCEL_BLIT
                         | STM_BLITTER_ACCEL_STRETCHBLIT
                         | STM_BLITTER_ACCEL_BLIT2);
        }
      else if (unlikely (stdev->setup->blitstate.rotate == 90
                         || stdev->setup->blitstate.rotate == 270))
        {
          stm_blit_printd (BDisp_State,
                           "%s(): rotated blit\n", __func__);
          state->set |= STM_BLITTER_ACCEL_BLIT;
          state->set &= ~(STM_BLITTER_ACCEL_STRETCHBLIT
                          | STM_BLITTER_ACCEL_BLIT2);
          stdrv->funcs.blit = bdisp2_blit_rotate_90_270;
          bdisp_aq_setup_blit_operation (stdrv, stdev);
        }
      else
        {
          state->set |= (STM_BLITTER_ACCEL_BLIT
                         | STM_BLITTER_ACCEL_STRETCHBLIT);
          /* if this is a stretchblit, or a blit of a subbyte format, or
             a blit with format conversion, or a blit with any blitting
             flags set, we have to use the slow path... */
          if (!stdev->force_slow_path
              && state->src->format == state->dst->format
              /* note that STM_BLITTER_SBF_NO_FILTER has no effect on copy
                 (i.e. when not stretching), so it must be ignored when
                 determining which blit function to use! */
              && ((state->blitflags & ~STM_BLITTER_SBF_NO_FILTER)
                  == STM_BLITTER_SBF_NONE)
              && state->src->colorspace == state->dst->colorspace
              /* this caters for 180 degrees and mirroring */
              && !stdev->setup->blitstate.rotate)
            {
              stm_blit_printd (BDisp_State,
                               "%s(): fast blit\n", __func__);

              switch (state->dst->format)
                {
                case STM_BLITTER_SF_YUY2:
                case STM_BLITTER_SF_UYVY:
                  /* for some reason a fast UYVY -> UYVY blit has problems, we
                     can use the fast path, but have to change the pixelformat!
                     RGB16 just works fine, and it doesn't matter as the BDisp
                     does not care about the actual data. */
                  stdrv->funcs.blit = bdisp2_blit_simple_YCbCr422r;
                  break;

                case STM_BLITTER_SF_YV12:
                case STM_BLITTER_SF_I420:
                  stdrv->funcs.blit = bdisp2_blit_simple_YV420;
                  break;
                case STM_BLITTER_SF_YV16:
                case STM_BLITTER_SF_YV61:
                  stdrv->funcs.blit = bdisp2_blit_simple_YV422;
                  break;
                case STM_BLITTER_SF_YCBCR444P:
                  stdrv->funcs.blit = bdisp2_blit_simple_YV444;
                  break;
                case STM_BLITTER_SF_NV12:
                case STM_BLITTER_SF_NV21:
                  stdrv->funcs.blit = bdisp2_blit_simple_NV420;
                  break;
                case STM_BLITTER_SF_NV16:
                case STM_BLITTER_SF_NV61:
                  stdrv->funcs.blit = bdisp2_blit_simple_NV422;
                  break;

                case STM_BLITTER_SF_NV24:
                case STM_BLITTER_SF_RGB565:
                case STM_BLITTER_SF_RGB24:
                case STM_BLITTER_SF_BGR24:
                case STM_BLITTER_SF_RGB32:
                case STM_BLITTER_SF_ARGB1555:
                case STM_BLITTER_SF_ARGB4444:
                case STM_BLITTER_SF_ARGB8565:
                case STM_BLITTER_SF_AlRGB8565:
                case STM_BLITTER_SF_ARGB:
                case STM_BLITTER_SF_AlRGB:
                case STM_BLITTER_SF_BGRA:
                case STM_BLITTER_SF_BGRAl:
                case STM_BLITTER_SF_ABGR:
                case STM_BLITTER_SF_LUT8:
                case STM_BLITTER_SF_LUT4:
                case STM_BLITTER_SF_LUT2:
                case STM_BLITTER_SF_LUT1:
                case STM_BLITTER_SF_ALUT88:
                case STM_BLITTER_SF_AlLUT88:
                case STM_BLITTER_SF_ALUT44:
                case STM_BLITTER_SF_A8:
                case STM_BLITTER_SF_Al8:
                case STM_BLITTER_SF_A1:
                case STM_BLITTER_SF_AVYU:
                case STM_BLITTER_SF_AlVYU:
                case STM_BLITTER_SF_VYU:
                  stdrv->funcs.blit = bdisp2_blit_simple;
                  break;

                case STM_BLITTER_SF_YCBCR420MB:
                case STM_BLITTER_SF_YCBCR422MB:
                  stdrv->funcs.blit = bdisp2_blit_as_stretch_MB4xx;
                  break;

                /* these are not supported via the normal API */
                case STM_BLITTER_SF_RLD_BD:
                case STM_BLITTER_SF_RLD_H2:
                case STM_BLITTER_SF_RLD_H8:
                case STM_BLITTER_SF_INVALID:
                case STM_BLITTER_SF_COUNT:
                default:
                  STM_BLITTER_ASSERT ("this should not be reached!\n" == NULL);
                  break;
                }
            }
          else if ((state->src->format ==  STM_BLITTER_SF_YUY2
                    && state->dst->format == STM_BLITTER_SF_UYVY)
                 || (state->src->format ==  STM_BLITTER_SF_UYVY
                    && state->dst->format == STM_BLITTER_SF_YUY2))
            {
              stm_blit_printd (BDisp_State,
                               "%s(): YC Swap blit\n", __func__);
              stdrv->funcs.blit = bdisp2_blit_swap_YCbCr422r;
              state->set &= ~STM_BLITTER_ACCEL_STRETCHBLIT;
            }
          else if (state->dst->format & STM_BLITTER_SF_PLANAR)
            {
              stm_blit_printd (BDisp_State,
                               "%s(): complex planar destinations blit\n", __func__);
              /* If the destination is a planar format, the blit operation
               * is completely indepenent from the source format.*/
              switch (state->dst->format)
                {
                case STM_BLITTER_SF_NV12:
                case STM_BLITTER_SF_NV21:
                case STM_BLITTER_SF_YV12:
                case STM_BLITTER_SF_I420:
                case STM_BLITTER_SF_YCBCR420MB:
                case STM_BLITTER_SF_NV16:
                case STM_BLITTER_SF_NV61:
                case STM_BLITTER_SF_YV16:
                case STM_BLITTER_SF_YV61:
                case STM_BLITTER_SF_YCBCR422MB:
                case STM_BLITTER_SF_YCBCR444P:
                  if (state->src->format == STM_BLITTER_SF_YCBCR420MB
                      || state->src->format == STM_BLITTER_SF_YCBCR422MB)
                     stdrv->funcs.blit = bdisp2_blit_as_stretch_MB4xx;
                   else
                     stdrv->funcs.blit = bdisp2_blit_as_stretch_planar_4xx;
                  break;
                case STM_BLITTER_SF_NV24:
                case STM_BLITTER_SF_YUY2:
                case STM_BLITTER_SF_UYVY:
                case STM_BLITTER_SF_RGB565:
                case STM_BLITTER_SF_RGB24:
                case STM_BLITTER_SF_BGR24:
                case STM_BLITTER_SF_RGB32:
                case STM_BLITTER_SF_ARGB1555:
                case STM_BLITTER_SF_ARGB4444:
                case STM_BLITTER_SF_ARGB8565:
                case STM_BLITTER_SF_AlRGB8565:
                case STM_BLITTER_SF_ARGB:
                case STM_BLITTER_SF_AlRGB:
                case STM_BLITTER_SF_BGRA:
                case STM_BLITTER_SF_BGRAl:
                case STM_BLITTER_SF_ABGR:
                case STM_BLITTER_SF_LUT8:
                case STM_BLITTER_SF_LUT4:
                case STM_BLITTER_SF_LUT2:
                case STM_BLITTER_SF_LUT1:
                case STM_BLITTER_SF_ALUT88:
                case STM_BLITTER_SF_AlLUT88:
                case STM_BLITTER_SF_ALUT44:
                case STM_BLITTER_SF_A8:
                case STM_BLITTER_SF_Al8:
                case STM_BLITTER_SF_A1:
                case STM_BLITTER_SF_AVYU:
                case STM_BLITTER_SF_AlVYU:
                case STM_BLITTER_SF_VYU:
                /* these are not supported via the normal API */
                case STM_BLITTER_SF_RLD_BD:
                case STM_BLITTER_SF_RLD_H2:
                case STM_BLITTER_SF_RLD_H8:
                case STM_BLITTER_SF_INVALID:
                case STM_BLITTER_SF_COUNT:
                default:
                  break;
                }
            }
          else if (state->src->format & STM_BLITTER_SF_PLANAR)
            {
              stm_blit_printd (BDisp_State,
                               "%s(): planar blit\n", __func__);

              if (state->src->format == STM_BLITTER_SF_YCBCR420MB
                  || state->src->format == STM_BLITTER_SF_YCBCR422MB)
                stdrv->funcs.blit = bdisp2_blit_as_stretch_MB;
              else
                stdrv->funcs.blit = bdisp2_blit_as_stretch;

              support_hw_clip = true;
            }
          else
            {
              stm_blit_printd (BDisp_State,
                               "%s(): complex blit\n", __func__);
              stdrv->funcs.blit = bdisp2_blit;
              support_hw_clip = true;
            }

          if (state->blitflags & (STM_BLITTER_SBF_DEINTERLACE_TOP
                                  | STM_BLITTER_SBF_DEINTERLACE_BOTTOM))
            {
              stdrv->funcs.blit = bdisp2_blit_as_stretch;
              state->set |= STM_BLITTER_ACCEL_BLIT;
            }

          if (accel & STM_BLITTER_ACCEL_BLIT2)
            {
              if (state->src->format & STM_BLITTER_SF_PLANAR)
                {
                  if (state->src->format == STM_BLITTER_SF_YCBCR420MB
                      || state->src->format == STM_BLITTER_SF_YCBCR422MB)
                    stdrv->funcs.blit2 = bdisp2_blit2_as_stretch_MB;
                  else
                    stdrv->funcs.blit2 = bdisp2_blit2_as_stretch;
                }
              else
                stdrv->funcs.blit2 = bdisp2_blit2;
              state->set |= STM_BLITTER_ACCEL_BLIT2;
            }
          else
            {
              stdrv->funcs.blit2 = bdisp2_blit2_nop;
              state->set &= ~STM_BLITTER_ACCEL_BLIT2;
            }

          switch (state->dst->format)
            {
            case STM_BLITTER_SF_NV12:
            case STM_BLITTER_SF_NV21:
            case STM_BLITTER_SF_YV12:
            case STM_BLITTER_SF_I420:
            case STM_BLITTER_SF_YCBCR420MB:
            case STM_BLITTER_SF_NV16:
            case STM_BLITTER_SF_NV61:
            case STM_BLITTER_SF_YV16:
            case STM_BLITTER_SF_YV61:
            case STM_BLITTER_SF_YCBCR422MB:
            case STM_BLITTER_SF_YCBCR444P:
              if (state->src->format == STM_BLITTER_SF_YCBCR420MB
                  || state->src->format == STM_BLITTER_SF_YCBCR422MB)
                stdrv->funcs.stretch_blit = bdisp2_stretch_blit_MB4xx;
              else
                stdrv->funcs.stretch_blit = bdisp2_stretch_blit_planar_4xx;
              break;
            case STM_BLITTER_SF_NV24:
            case STM_BLITTER_SF_YUY2:
            case STM_BLITTER_SF_UYVY:
            case STM_BLITTER_SF_RGB565:
            case STM_BLITTER_SF_RGB24:
            case STM_BLITTER_SF_BGR24:
            case STM_BLITTER_SF_RGB32:
            case STM_BLITTER_SF_ARGB1555:
            case STM_BLITTER_SF_ARGB4444:
            case STM_BLITTER_SF_ARGB8565:
            case STM_BLITTER_SF_AlRGB8565:
            case STM_BLITTER_SF_ARGB:
            case STM_BLITTER_SF_AlRGB:
            case STM_BLITTER_SF_BGRA:
            case STM_BLITTER_SF_BGRAl:
            case STM_BLITTER_SF_ABGR:
            case STM_BLITTER_SF_LUT8:
            case STM_BLITTER_SF_LUT4:
            case STM_BLITTER_SF_LUT2:
            case STM_BLITTER_SF_LUT1:
            case STM_BLITTER_SF_ALUT88:
            case STM_BLITTER_SF_AlLUT88:
            case STM_BLITTER_SF_ALUT44:
            case STM_BLITTER_SF_A8:
            case STM_BLITTER_SF_Al8:
            case STM_BLITTER_SF_A1:
            case STM_BLITTER_SF_AVYU:
            case STM_BLITTER_SF_AlVYU:
            case STM_BLITTER_SF_VYU:
              if (state->src->format == STM_BLITTER_SF_YCBCR420MB
                  || state->src->format == STM_BLITTER_SF_YCBCR422MB)
                stdrv->funcs.stretch_blit = bdisp2_stretch_blit_MB;
              else
                stdrv->funcs.stretch_blit = bdisp2_stretch_blit;
              /* fall through */

            /* these are not supported via the normal API */
            case STM_BLITTER_SF_RLD_BD:
            case STM_BLITTER_SF_RLD_H2:
            case STM_BLITTER_SF_RLD_H8:
            case STM_BLITTER_SF_INVALID:
            case STM_BLITTER_SF_COUNT:
            default:
                break;
            }

          bdisp_aq_setup_blit_operation (stdrv, stdev);
        }
      break;

    default:
      /* Should never happen as we have told DirectFB which functions we can
         support */
      stm_blit_printe ("bdisp_state: unexpected acceleration function");
      state->set &= ~(STM_BLITTER_ACCEL_BLIT
                      | STM_BLITTER_ACCEL_STRETCHBLIT
                      | STM_BLITTER_ACCEL_BLIT2);
      break;
  }


  if (support_hw_clip)
    {
#ifdef BDISP2_SUPPORT_HW_CLIPPING
      BDISP_STATE_CHECK_N_VALIDATE (CLIP);

      stdev->setup->ConfigGeneral.BLT_CIC |= BLIT_CIC_NODE_CLIP;
      stdev->setup->ConfigGeneral.BLT_INS |= BLIT_INS_ENABLE_RECTCLIP;

      /* multi pass operations */
      stdev->setup->blitstate.extra_passes[0].ConfigGeneral.BLT_CIC |= BLIT_CIC_NODE_CLIP;
      stdev->setup->blitstate.extra_passes[0].ConfigGeneral.BLT_INS |= BLIT_INS_ENABLE_RECTCLIP;
      stdev->setup->blitstate.extra_passes[1].ConfigGeneral.BLT_CIC |= BLIT_CIC_NODE_CLIP;
      stdev->setup->blitstate.extra_passes[1].ConfigGeneral.BLT_INS |= BLIT_INS_ENABLE_RECTCLIP;
    }
  else
    {
      /* fast path, hence no HW clipping */
      stdev->setup->ConfigGeneral.BLT_CIC &= ~BLIT_CIC_NODE_CLIP;
      stdev->setup->ConfigGeneral.BLT_INS &= ~BLIT_INS_ENABLE_RECTCLIP;

      /* multi pass operations */
      stdev->setup->blitstate.extra_passes[0].ConfigGeneral.BLT_CIC &= ~BLIT_CIC_NODE_CLIP;
      stdev->setup->blitstate.extra_passes[0].ConfigGeneral.BLT_INS &= ~BLIT_INS_ENABLE_RECTCLIP;
      stdev->setup->blitstate.extra_passes[1].ConfigGeneral.BLT_CIC &= ~BLIT_CIC_NODE_CLIP;
      stdev->setup->blitstate.extra_passes[1].ConfigGeneral.BLT_INS &= ~BLIT_INS_ENABLE_RECTCLIP;
#endif  /* BDISP2_SUPPORT_HW_CLIPPING */
    }


  state->modified = 0;
}
