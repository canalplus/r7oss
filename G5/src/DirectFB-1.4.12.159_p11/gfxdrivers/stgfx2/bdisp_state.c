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

#include <asm/types.h>
#include <direct/types.h>
#include <core/coretypes.h>
#include <core/state.h>
#include <core/surface.h>

#include <gfx/convert.h>


#include "stgfx2_features.h"
#include "stm_types.h"

#include "bdisp_state.h"

#include "bdisp_features.h"
#include "bdisp_tables.h"
#include "bdisp_registers.h"
#include "stm_gfxdriver.h"
#include "bdisp_accel.h"


D_DEBUG_DOMAIN (BDisp_State,      "BDisp/State",      "BDispII state handling");
D_DEBUG_DOMAIN (BDisp_StateCheck, "BDisp/StateCheck", "BDispII state testing");
D_DEBUG_DOMAIN (BDisp_StateSet,   "BDisp/StateSet",   "BDispII state setting");


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

  DRAWINGFLAGS       = 0x00000200,

  BLITTINGFLAGS_CKEY = 0x00010000,
  ROTATION           = 0x00020000,
  RENDEROPTS         = 0x00040000,
  BLITTINGFLAGS      = 0x00080000,
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
                                               bdisp_state_validate_##flag (stdrv, stdev, state); \
                                           } )
#define BDISP_STATE_CHECK_N_VALIDATE2(flag) ( {                                                   \
                                             if (BDISP_STATE_FLAG_NEEDS_VALIDATE(flag))           \
                                               bdisp_state_validate_##flag (stdrv, stdev, state, funcs); \
                                           } )
#define BDISP_STATE_CHECK_N_VALIDATE3(flag) ( {                                                   \
                                             if (BDISP_STATE_FLAG_NEEDS_VALIDATE(flag))           \
                                               bdisp_state_validate_##flag (stdrv, stdev, state, accel); \
                                           } )


/******** SetState() ********************************************************/
#if defined(STGFX2_DUMP_CHECK_STATE) \
    || defined(STGFX2_DUMP_CHECK_STATE_FAILED) \
    || defined(STGFX2_DUMP_SET_STATE)
static const char *_state_strs[] = {
  "DSBF_UNKNOWN",
  "DSBF_ZERO",
  "DSBF_ONE",
  "DSBF_SRCCOLOR",
  "DSBF_INVSRCCOLOR",
  "DSBF_SRCALPHA",
  "DSBF_INVSRCALPHA",
  "DSBF_DESTALPHA",
  "DSBF_INVDESTALPHA",
  "DSBF_DESTCOLOR",
  "DSBF_INVDESTCOLOR",
  "DSBF_SRCALPHASAT"
};

static char *
_get_accel_string (DFBAccelerationMask accel)
{
  char *str = malloc (220);
  int pos = 0;

  pos += sprintf (str + pos, "%.8x ->", accel);
  if (accel == DFXL_NONE)
    pos += sprintf (str + pos, " NONE");
  else if (accel == DFXL_ALL)
    pos += sprintf (str + pos, " ALL");
  else if (accel == DFXL_ALL_DRAW)
    pos += sprintf (str + pos, " ALL_DRAW");
  else if (accel == DFXL_ALL_BLIT)
    pos += sprintf (str + pos, " ALL_BLIT");
  else
    {
      if (accel & DFXL_FILLRECTANGLE)
        pos += sprintf (str + pos, " FILLRECTANGLE");
      if (accel & DFXL_DRAWRECTANGLE)
        pos += sprintf (str + pos, " DRAWRECTANGLE");
      if (accel & DFXL_DRAWLINE)
        pos += sprintf (str + pos, " DRAWLINE");
      if (accel & DFXL_FILLTRIANGLE)
        pos += sprintf (str + pos, " FILLTRIANGLE");
      if (accel & DFXL_BLIT)
        pos += sprintf (str + pos, " BLIT");
      if (accel & DFXL_STRETCHBLIT)
        pos += sprintf (str + pos, " STRETCHBLIT");
      if (accel & DFXL_TEXTRIANGLES)
        pos += sprintf (str + pos, " TEXTRIANGLES");
      if (accel & DFXL_BLIT2)
        pos += sprintf (str + pos, " BLIT2");
      if (accel & DFXL_DRAWSTRING)
        pos += sprintf (str + pos, " DRAWSTRING");
    }

  return str;
}

static char *
_get_modified_string (StateModificationFlags modified)
{
  char *str = malloc (220);
  int pos = 0;

  pos += sprintf (str + pos, "%.8x ->", modified);
  if (!modified)
    pos += sprintf (str + pos, " none");
  else if (modified == SMF_ALL)
    pos += sprintf (str + pos, " ALL");
  else
    {
      if (modified & SMF_DRAWING_FLAGS)
        pos += sprintf (str + pos, " DRAWING_FLAGS");
      if (modified & SMF_BLITTING_FLAGS)
        pos += sprintf (str + pos, " BLITTING_FLAGS");
      if (modified & SMF_CLIP)
        pos += sprintf (str + pos, " CLIP");
      if (modified & SMF_COLOR)
        pos += sprintf (str + pos, " COLOR");
      if (modified & SMF_SRC_BLEND)
        pos += sprintf (str + pos, " SRC_BLEND");
      if (modified & SMF_DST_BLEND)
        pos += sprintf (str + pos, " DST_BLEND");
      if (modified & SMF_SRC_COLORKEY)
        pos += sprintf (str + pos, " SRC_COLORKEY");
      if (modified & SMF_DST_COLORKEY)
        pos += sprintf (str + pos, " DST_COLORKEY");
      if (modified & SMF_DESTINATION)
        pos += sprintf (str + pos, " DESTINATION");
      if (modified & SMF_SOURCE)
        pos += sprintf (str + pos, " SOURCE");
      if (modified & SMF_SOURCE_MASK)
        pos += sprintf (str + pos, " SOURCE_MASK");
      if (modified & SMF_SOURCE_MASK_VALS)
        pos += sprintf (str + pos, " SOURCE_MASK_VALS");
      if (modified & SMF_INDEX_TRANSLATION)
        pos += sprintf (str + pos, " INDEX_TRANSLATION");
      if (modified & SMF_COLORKEY)
        pos += sprintf (str + pos, " COLORKEY");
      if (modified & SMF_RENDER_OPTIONS)
        pos += sprintf (str + pos, " RENDER_OPTIONS");
      if (modified & SMF_MATRIX)
        pos += sprintf (str + pos, " MATRIX");
      if (modified & SMF_SOURCE2)
        pos += sprintf (str + pos, " SOURCE2");
    }

  return str;
}

static char *
_get_drawflags_string (DFBSurfaceDrawingFlags flags)
{
  char *str = malloc (220);
  int pos = 0;

  pos += sprintf (str + pos, "%.8x ->", flags);
  if (flags == DSDRAW_NOFX)
    pos += sprintf (str + pos, " NOFX");
  else
    {
      if (flags & DSDRAW_BLEND)
        pos += sprintf (str + pos, " BLEND");
      if (flags & DSDRAW_DST_COLORKEY)
        pos += sprintf (str + pos, " DST_COLORKEY");
      if (flags & DSDRAW_SRC_PREMULTIPLY)
        pos += sprintf (str + pos, " SRC_PREMULTIPLY");
      if (flags & DSDRAW_DST_PREMULTIPLY)
        pos += sprintf (str + pos, " DST_PREMULTIPLY");
      if (flags & DSDRAW_DEMULTIPLY)
        pos += sprintf (str + pos, " DEMULTIPLY");
      if (flags & DSDRAW_XOR)
        pos += sprintf (str + pos, " XOR");
    }

  return str;
}

static char *
_get_blitflags_string (DFBSurfaceBlittingFlags flags)
{
  char *str = malloc (220);
  int pos = 0;

  pos += sprintf (str + pos, "%.8x ->", flags);
  if (flags == DSBLIT_NOFX)
    pos += sprintf (str + pos, " NOFX");
  else
    {
      if (flags & DSBLIT_BLEND_ALPHACHANNEL)
        pos += sprintf (str + pos, " BLEND_ALPHACHANNEL");
      if (flags & DSBLIT_BLEND_COLORALPHA)
        pos += sprintf (str + pos, " BLEND_COLORALPHA");
      if (flags & DSBLIT_COLORIZE)
        pos += sprintf (str + pos, " COLORIZE");
      if (flags & DSBLIT_SRC_COLORKEY)
        pos += sprintf (str + pos, " SRC_COLORKEY");
      if (flags & DSBLIT_DST_COLORKEY)
        pos += sprintf (str + pos, " DST_COLORKEY");
      if (flags & DSBLIT_SRC_PREMULTIPLY)
        pos += sprintf (str + pos, " SRC_PREMULTIPLY");
      if (flags & DSBLIT_DST_PREMULTIPLY)
        pos += sprintf (str + pos, " DST_PREMULTIPLY");
      if (flags & DSBLIT_DEMULTIPLY)
        pos += sprintf (str + pos, " DEMULTIPLY");
      if (flags & DSBLIT_DEINTERLACE)
        pos += sprintf (str + pos, " DEINTERLACE");
      if (flags & DSBLIT_SRC_PREMULTCOLOR)
        pos += sprintf (str + pos, " SRC_PREMULTCOLOR");
      if (flags & DSBLIT_XOR)
        pos += sprintf (str + pos, " XOR");
      if (flags & DSBLIT_INDEX_TRANSLATION)
        pos += sprintf (str + pos, " INDEX_TRANSLATION");
      if (flags & DSBLIT_ROTATE90)
        pos += sprintf (str + pos, " ROTATE90");
      if (flags & DSBLIT_ROTATE180)
        pos += sprintf (str + pos, " ROTATE180");
      if (flags & DSBLIT_ROTATE270)
        pos += sprintf (str + pos, " ROTATE270");
      if (flags & DSBLIT_COLORKEY_PROTECT)
        pos += sprintf (str + pos, " COLORKEY_PROTECT");
      if (flags & DSBLIT_SRC_MASK_ALPHA)
        pos += sprintf (str + pos, " SRC_MASK_ALPHA");
      if (flags & DSBLIT_SRC_MASK_COLOR)
        pos += sprintf (str + pos, " SRC_MASK_COLOR");
      if (flags & DSBLIT_SOURCE2)
        pos += sprintf (str + pos, " SOURCE2");
      if (flags & DSBLIT_FLIP_HORIZONTAL)
        pos += sprintf (str + pos, " FLIP_HORIZONTAL");
      if (flags & DSBLIT_FLIP_VERTICAL)
        pos += sprintf (str + pos, " FLIP_VERTICAL");
      if (flags & DSBLIT_FIXEDPOINT)
        pos += sprintf (str + pos, " FIXEDPOINT");
    }

  return str;
}

static char *
_get_renderoptions_string (DFBSurfaceRenderOptions opts)
{
  char *str = malloc (220);
  int pos = 0;

  pos += sprintf (str + pos, "%.8x ->", opts);
  if (opts == DSRO_NONE)
    pos += sprintf (str + pos, " NONE");
  else
    {
      if (opts & DSRO_SMOOTH_UPSCALE)
        pos += sprintf (str + pos, " SMOOTH_UPSCALE");
      if (opts & DSRO_SMOOTH_DOWNSCALE)
        pos += sprintf (str + pos, " SMOOTH_DOWNSCALE");
      if (opts & DSRO_MATRIX)
        pos += sprintf (str + pos, " MATRIX");
      if (opts & DSRO_ANTIALIAS)
        pos += sprintf (str + pos, " ANTIALIAS");
    }

  return str;
}

static char *
_get_matrix_string (const s32 *matrix)
{
  char *str = malloc (220);
  int pos = 0, i;

  for (i = 0; i < 3; ++i, ++matrix)
    pos += sprintf (str + pos, "[%.3x.%.3x] ", (*matrix) >> 16, (*matrix) & 0xffff);
  pos += sprintf (str + pos, "\n            ");
  for (i = 0; i < 3; ++i, ++matrix)
    pos += sprintf (str + pos, "[%.3x.%.3x] ", (*matrix) >> 16, (*matrix) & 0xffff);
  pos += sprintf (str + pos, "\n            ");
  for (i = 0; i < 3; ++i, ++matrix)
    pos += sprintf (str + pos, "[%.3x.%.3x] ", (*matrix) >> 16, (*matrix) & 0xffff);
  pos += sprintf (str + pos, "\n");

  return str;
}

static void
_dump_state (const char          * const prefix,
             const CardState     * const state,
             DFBAccelerationMask  accel)
{
  char * const accel_str = _get_accel_string (accel);
  char * const modified_str = _get_modified_string (state->mod_hw);
  char * const drawflags_str = _get_drawflags_string (state->drawingflags);
  char * const blitflags_str = _get_blitflags_string (state->blittingflags);
  char * const renderopts_str = _get_renderoptions_string (state->render_options);
  char * const matrix_str = _get_matrix_string (state->matrix);

  printf ("%s: accel: %s\n"
          "    state: modified: %s, flags draw/blit: %s/%s\n"
          "    blendfunc src/dst: %u(%s)/%u(%s)\n"
          "    format src/dst: '%s'/'%s'\n"
          "    color/index: %.2x%.2x%.2x%.2x/%.8x colorkey src/dst: %.8x/%.8x   clip: %d,%d-%d,%d\n"
          "    render_opts: %s\n"
          "    matrix: %s\n",
          prefix,
          accel_str, modified_str, drawflags_str, blitflags_str,
          state->src_blend, (state->src_blend <= DSBF_SRCALPHASAT) ? _state_strs[state->src_blend] : _state_strs[0],
          state->dst_blend, (state->dst_blend <= DSBF_SRCALPHASAT) ? _state_strs[state->dst_blend] : _state_strs[0],
          state->source ? dfb_pixelformat_name (state->source->config.format) : "no source",
          state->destination ? dfb_pixelformat_name (state->destination->config.format) : "no dest",
          state->color.a, state->color.r, state->color.g, state->color.b, state->color_index,
          state->src_colorkey, state->dst_colorkey, DFB_REGION_VALS (&state->clip),
          renderopts_str, matrix_str);

  free (matrix_str);
  free (renderopts_str);
  free (blitflags_str);
  free (drawflags_str);
  free (modified_str);
  free (accel_str);
}
#endif /* STGFX2_DUMP_CHECK_STATE || STGFX2_DUMP_CHECK_STATE_FAILED || STGFX2_DUMP_SET_STATE */
#ifdef STGFX2_DUMP_CHECK_STATE
  #define _dump_check_state(p, s, a) _dump_state(p, s, a)
#else /* STGFX2_DUMP_CHECK_STATE */
  #define _dump_check_state(p, s, a)
#endif /* STGFX2_DUMP_CHECK_STATE */

#ifdef STGFX2_DUMP_CHECK_STATE_FAILED
  #ifndef STGFX2_DUMP_CHECK_STATE
    #define _dump_check_state_failed(p, s, a) ( { char t[100];               \
                                                  snprintf (t, sizeof (t),   \
                                                            "%s: failed", p);\
                                                  _dump_state(t, s, a);      \
                                              } )
  #else /* STGFX2_DUMP_CHECK_STATE_FAILED */
    #define _dump_check_state_failed(p, s, a) printf("... %s: failed\n", p);
  #endif /* STGFX2_DUMP_CHECK_STATE_FAILED */
#else /* STGFX2_DUMP_CHECK_STATE */
  #define _dump_check_state_failed(p, s, a)
#endif /* STGFX2_DUMP_CHECK_STATE */

#ifdef STGFX2_DUMP_SET_STATE
  #define _dump_set_state(p, s, a) _dump_state(p, s, a)
#else /* STGFX2_DUMP_SET_STATE */
  #define _dump_set_state(p, s, a)
#endif /* STGFX2_DUMP_SET_STATE */




#define DSPD_SATURATE          (DSPD_DST + 1)
#define DSPD_UNSUPPORTED       (DSPD_SATURATE + 1)
static DFBSurfacePorterDuffRule
__attribute__((warn_unused_result))
bdisp_aq_convert_to_supported_porterduff (const CardState * const state)
{
  /* the blending maths we can support */
  switch (state->src_blend)
    {
    case DSBF_ZERO:
      if (state->dst_blend == DSBF_ZERO)
        return DSPD_CLEAR;
      if (state->dst_blend == DSBF_SRCALPHA)
        return DSPD_DST_IN;
      if (state->dst_blend == DSBF_INVSRCALPHA)
        return DSPD_DST_OUT;
      if (state->dst_blend == DSBF_ONE)
        return DSPD_DST;
      break;

    case DSBF_ONE:
      if (state->dst_blend == DSBF_ZERO)
        return DSPD_SRC;
      if (state->dst_blend == DSBF_INVSRCALPHA)
        return DSPD_SRC_OVER;
      break;

    case DSBF_INVDESTALPHA:
      if (state->dst_blend == DSBF_ONE)
        return DSPD_DST_OVER;
      break;

    case DSBF_SRCALPHA:
      if (state->dst_blend == DSBF_INVSRCALPHA)
        return DSPD_NONE;
      break;

    default:
      break;
    }

  return DSPD_UNSUPPORTED;
}

static bool
stgfx2SupportedSrcPixelFormat (const STGFX2DeviceData * const stdev,
                               DFBSurfacePixelFormat   dfbFmt)
{
  const struct _bdisp_pixelformat_table * const dspf_to_bdisp
    = stdev->features.dfb_dspf_to_bdisp;

  D_ASSERT (dfbFmt != DSPF_UNKNOWN);
  if (unlikely (!dfbFmt))
    return false;

  return dspf_to_bdisp[DFB_PIXELFORMAT_INDEX (dfbFmt)].supported_as_src;
}

static bool
stgfx2SupportedDstPixelFormat (const STGFX2DeviceData * const stdev,
                               DFBSurfacePixelFormat   dfbFmt)
{
  const struct _bdisp_pixelformat_table * const dspf_to_bdisp
    = stdev->features.dfb_dspf_to_bdisp;

  D_ASSERT (dfbFmt != DSPF_UNKNOWN);
  if (unlikely (!dfbFmt))
    return false;

  return dspf_to_bdisp[DFB_PIXELFORMAT_INDEX (dfbFmt)].supported_as_dst;
}

static int
bdisp_aq_convert_to_supported_rotation (const CardState     * const state,
                                        DFBAccelerationMask  accel,
                                        const s32 *matrix)
{
  if (state)
    _dump_check_state (__FUNCTION__, state, accel);

  /* we (can) only support a handful of matrices:
     - 1:1 translation
     - rotation 90 180 270 degrees
     - reflection on X or Y axis

     in theory, support for scaling and rotation could be added as well, I
     guess. But we deny that for complexity reasons.

     matrix formula:
     X' = (X * v0 + Y * v1 + v2) / (X * v6 + Y * v7 + v8)
     Y' = (X * v3 + Y * v4 + v5) / (X * v6 + Y * v7 + v8) */
#define fixedpointONE  (1L << 16)           /* 1 in 16.16 fixed point */
#define fixedpointHALF (fixedpointONE / 2)  /* 1/2 in 16.16 fixed point */
#define fractionMASK   (fixedpointONE - 1)
  if (state && !state->affine_matrix)
    return -1;

  if (matrix[2] != 0 || matrix[5] != 0)
    return -1;

  if (matrix[0] == 1 * fixedpointONE && matrix[1] == 0
      && matrix[3] == 0 && matrix[4] == -1 * fixedpointONE)
    return 181; /* reflection against x */

  if (matrix[0] == -1 * fixedpointONE && matrix[1] == 0
      && matrix[3] == 0 && matrix[4] == 1 * fixedpointONE)
    return 182; /* reflection against y */

  if (matrix[0] == -1 * fixedpointONE && matrix[1] == 0
      && matrix[3] == 0 && matrix[4] == -1 * fixedpointONE)
    return 180; /* counter clock wise */

  if (matrix[0] == 0 && matrix[1] == 1 * fixedpointONE
      && matrix[3] == -1 * fixedpointONE && matrix[4] == 0)
    return 270; /* counter clock wise */

  if (matrix[0] == 0 && matrix[1] == -1 * fixedpointONE
      && matrix[3] == 1 * fixedpointONE && matrix[4] == 0)
    return 90; /* counter clock wise */

  if (matrix[0] == 1 * fixedpointONE && matrix[1] == 0
      && matrix[3] == 0 && matrix[4] == 1 * fixedpointONE)
    return 0; /* nothing */

  return -1;
}


/**************************************************************************/
/* Check that the current state is ok for the operation indicated by accel*/
/**************************************************************************/
  #define BDISP_AQ_DRAW_PIXELFORMAT_HAS_ALPHA(fmt) (DFB_PIXELFORMAT_HAS_ALPHA (fmt))

  #define BDISP_AQ_BLIT_PIXELFORMAT_HAS_ALPHA(fmt) (DFB_PIXELFORMAT_HAS_ALPHA (fmt))

#define BDISP_AQ_MULTIPASS_PORTER_DUFF_RULE(pd) \
         (((pd) == DSPD_DST_IN)         \
          || ((pd) == DSPD_DST_OUT)     \
/*          || ((pd) == DSPD_SRC_IN)      \
          || ((pd) == DSPD_SRC_OUT)     \
          || ((pd) == DSPD_SRC_ATOP)    \
          || ((pd) == DSPD_DST_ATOP)    \
*/         )

#define BDISP_AQ_PORTER_DUFF_RULE_NEEDS_LUT(pd) \
         (((pd) == DSPD_DST_IN)         \
          || ((pd) == DSPD_DST_OUT)     \
/*          || ((pd) == DSPD_SRC_IN)      \
          || ((pd) == DSPD_SRC_OUT)     \
          || ((pd) == DSPD_SRC_ATOP)    \
          || ((pd) == DSPD_DST_ATOP)    \
*/         )

void
bdisp_aq_CheckState (void                * const drv,
                     void                * const dev,
                     CardState           * const state,
                     DFBAccelerationMask  accel)
{
  const STGFX2DeviceData * const stdev = dev;

  _dump_check_state (__FUNCTION__, state, accel);

  /* return if the desired function is not supported at all. */
  if (D_FLAGS_INVALID (accel, (STGFX2_VALID_DRAWINGFUNCTIONS
                               | STGFX2_VALID_BLITTINGFUNCTIONS)))
    return;

  if (!stgfx2SupportedDstPixelFormat (stdev, state->destination->config.format))
    goto out; /* unsupported destination format */

  if (D_FLAGS_INVALID (state->render_options,
                       stdev->features.dfb_renderopts))
    goto out;

  int rotation = 0;
  if (D_FLAGS_IS_SET (state->render_options, DSRO_MATRIX))
    {
      rotation = bdisp_aq_convert_to_supported_rotation (state,
                                                         accel,
                                                         state->matrix);
      if (rotation == -1)
        goto out;
    }

  if (DFB_DRAWING_FUNCTION (accel))
    {
      /* check there are no other drawing flags than those supported */
      if (D_FLAGS_INVALID (state->drawingflags, stdev->features.dfb_drawflags))
        goto out;

      /* check blend restrictions */
      if (D_FLAGS_IS_SET (state->drawingflags, DSDRAW_BLEND))
        {
          DFBSurfacePorterDuffRule pd =
            bdisp_aq_convert_to_supported_porterduff (state);

          if (pd == DSPD_UNSUPPORTED
              /* can't support multi node drawing ops */
              || BDISP_AQ_MULTIPASS_PORTER_DUFF_RULE (pd)
             )
            goto out;

          if (state->destination->config.format == DSPF_RGB32)
            {
              switch (pd)
                {
                /* we can support a few operations involving RGB32 without much
                   effort. Fortunately, those are the ones that are used by
                   graphics libraries like cairo. */
                case DSPD_DST_OVER:
                case DSPD_SRC_OVER:
                case DSPD_CLEAR:
                case DSPD_DST:
                case DSPD_SRC:
                case DSPD_NONE:
                  /* supported */
                  break;
                default:
                  goto out;
                }
            }

          /* don't allow blends to LUT surfaces, but we do allow fast fills in
             order to be able to do a "clear" to a specific index. */
          if (DFB_PIXELFORMAT_IS_INDEXED (state->destination->config.format))
            goto out;

          /* can not support blend and xor at the same time */
          if (state->drawingflags & DSDRAW_XOR)
            goto out;

          /* DSPD_NONE can only be accelerated when the destination does not
             have an alpha channel, because the correct calculation of the
             destination alpha cannot be achieved in the hardware. We go to
             the trouble of supporting it for pure RGB surfaces as the mode
             is used in the blended fill df_dok performance test. */
          if (BDISP_AQ_DRAW_PIXELFORMAT_HAS_ALPHA (state->destination->config.format)
              && pd == DSPD_NONE)
            goto out;
        }

      state->accel |= STGFX2_VALID_DRAWINGFUNCTIONS;
    }
  else /* if (DFB_BLITTING_FUNCTION (accel)) */
    {
      bool canUseHWInputMatrix;
      bool isRotate;
      DFBSurfaceBlittingFlags blitflags_norotate;

      /* check there are no other drawing flags than those supported */
      if (D_FLAGS_INVALID (state->blittingflags, stdev->features.dfb_blitflags))
        goto out;

      if (!stgfx2SupportedSrcPixelFormat (stdev, state->source->config.format))
        goto out; /* unsupported source format */

      isRotate = (D_FLAGS_IS_SET (state->blittingflags, (DSBLIT_ROTATE90
                                                         | DSBLIT_ROTATE270))
                  || rotation == 90
                  || rotation == 270);
      /* rotation is supported only when no other flag is requested (apart
         from DSBLIT_INDEX_TRANSLATION, which just tells us to do a 1:1 copy
         of LUT surfaces) */
      blitflags_norotate = state->blittingflags & ~(DSBLIT_ROTATE90
                                                    | DSBLIT_ROTATE270);

      /* some pixelformat checks */
      switch (state->source->config.format)
        {
        case DSPF_NV12:
        case DSPF_NV16:
           if ((state->source->config.size.w & 0xf)
               || (state->source->config.size.h & 0xf)
               || ((state->source->config.size.h >> 4) & 1L))
             {
               /* must be an even multiple of 16. */
               D_WARN ("%s(): nv12 and nv16 width and height must be "
                       "multiples (even for the height) of 16. Got %dx%d.\n",
                       __FUNCTION__,
                       state->source->config.size.w, state->source->config.size.h);
               goto out;
             }
        case DSPF_I420: case DSPF_YV12: case DSPF_YV16: case DSPF_YUV444P:
          /* we don't (can't) support YUV formats w/ any blittingflags set,
             we could support SOURCE colorkeying and SOURCE2 probably */
          if (state->blittingflags)
            goto out;

          /* we don't support format conversion to RGB32, support could be
             added, though... */
          if (state->destination->config.format == DSPF_RGB32)
            goto out;

        default:
          break;
        }

      bool src_is_ycbcr = false, dst_is_ycbcr = false;
      switch (state->source->config.format)
        {
        case DSPF_I420:
        case DSPF_YV12:
        case DSPF_YV16:
        case DSPF_YUV444P:
          if (D_FLAGS_IS_SET (state->blittingflags, DSBLIT_SOURCE2)
              || D_FLAGS_IS_SET (accel, DFXL_BLIT2))
            goto out;
          /* fall through */
        case DSPF_UYVY:
        case DSPF_YUY2:
        case DSPF_AVYU:
        case DSPF_VYU:
        case DSPF_NV12:
        case DSPF_NV16:
          if (D_FLAGS_IS_SET (state->blittingflags, (DSBLIT_SRC_PREMULTCOLOR
                                                     | DSBLIT_SRC_PREMULTIPLY)))
            /* These are RGB source modulations so fail if the source is YUV. */
            goto out;
          src_is_ycbcr = true;
          /* fall through */

        default:
          break;
        }

      switch (state->destination->config.format)
        {
        case DSPF_UYVY:
        case DSPF_YUY2:
        case DSPF_AVYU:
        case DSPF_VYU:
        case DSPF_I420:
        case DSPF_YV12:
        case DSPF_YV16:
        case DSPF_YUV444P:
        case DSPF_NV12:
        case DSPF_NV16:
          dst_is_ycbcr = true;
          /* fall through */

        default:
          break;
        }

      canUseHWInputMatrix = (src_is_ycbcr == dst_is_ycbcr);

      /* deal with a special case of LUT->LUT blits that do not go through a
         colour lookup, but instead do an index translation mapping. Note
         that we do not actually support anything other than a 1:1 mapping
         of the LUT.
         We do support blitting to a LUT surface in case of rotation (see below
         for explanation on rotation). */
      if (DFB_PIXELFORMAT_IS_INDEXED (state->destination->config.format))
        {
          /* note that blits are not supported to LUT destinations other than
             in the index translation case. */
          if (state->source->config.format == state->destination->config.format
              && blitflags_norotate == DSBLIT_INDEX_TRANSLATION
              && !D_FLAGS_IS_SET (state->render_options, DSRO_ANTIALIAS))
            {
              state->accel |= DFXL_BLIT;
              if (!D_FLAGS_IS_SET (state->render_options, (DSRO_SMOOTH_UPSCALE
                                                           | DSRO_SMOOTH_DOWNSCALE)))
                state->accel |= DFXL_STRETCHBLIT;
              return;
            }

          goto out;
        }

      /* rotation is supported only when no other flag is requested (apart
         from DSBLIT_INDEX_TRANSLATION, which just tells us to do a 1:1 copy
         of LUT surfaces) */
      if (unlikely (isRotate))
        {
          if (blitflags_norotate || !canUseHWInputMatrix)
            goto out;

          switch (state->source->config.format)
            {
            case DSPF_YUY2: case DSPF_UYVY: case DSPF_AVYU: case DSPF_VYU:
            case DSPF_I420: case DSPF_YV12: case DSPF_YV16: case DSPF_YUV444P:
            case DSPF_NV12: case DSPF_NV16:
              goto out;

            default:
              break;
            }
          switch (state->destination->config.format)
            {
            case DSPF_YUY2: case DSPF_UYVY: case DSPF_AVYU: case DSPF_VYU:
              goto out;

            default:
              break;
            }

          if (DFB_PIXELFORMAT_IS_INDEXED (state->source->config.format)
              && state->source->config.format != state->destination->config.format)
            goto out;

          state->accel |= DFXL_BLIT;
          return;
        }


      /* can not accelerate simultaneous source and destination colour key */
      if (D_FLAGS_ARE_SET (state->blittingflags, (DSBLIT_SRC_COLORKEY
                                                  | DSBLIT_DST_COLORKEY)))
        goto out;

      /* smooth upscale and smooth downscale must be set the same */
      if (D_FLAGS_IS_SET (state->render_options, (DSRO_SMOOTH_UPSCALE
                                                  | DSRO_SMOOTH_DOWNSCALE)))
        {
          bool u = !!(state->render_options & DSRO_SMOOTH_UPSCALE);
          bool d = !!(state->render_options & DSRO_SMOOTH_DOWNSCALE);
          if (u != d)
            goto out;
        }

      bool src_premultcolor = D_FLAGS_IS_SET (state->blittingflags,
                                              DSBLIT_SRC_PREMULTCOLOR);
      bool needs_blend = false;

      if (D_FLAGS_IS_SET (state->blittingflags, (DSBLIT_BLEND_COLORALPHA
                                                 | DSBLIT_BLEND_ALPHACHANNEL)))
        {
          DFBSurfacePorterDuffRule pd =
            bdisp_aq_convert_to_supported_porterduff (state);

          bool src_premultiply;

          if (pd == DSPD_UNSUPPORTED)
            goto out;

          /* we cannot blend and XOR at the same time, what a silly idea */
          if (D_FLAGS_IS_SET (state->blittingflags, DSBLIT_XOR))
            /* XOR needs the ROP unit and blending needs the blend unit - they
               can not be used simultaneously. */
            goto out;

          if (state->destination->config.format == DSPF_RGB32)
            {
              if (state->source->config.format == DSPF_RGB32
                  || DFB_PIXELFORMAT_IS_INDEXED (state->source->config.format))
                /* can't do this for:
                   RGB32 because we can only use the LUT to correct the alpha
                         either on S1 or S2, but not on both.
                   INDEXED because we only have one LUT, either to use for color
                           lookup or color correction */
                goto out;
            }

          src_premultiply = D_FLAGS_IS_SET (state->blittingflags,
                                            DSBLIT_SRC_PREMULTIPLY);

          switch (state->blittingflags & (DSBLIT_BLEND_ALPHACHANNEL
                                          | DSBLIT_BLEND_COLORALPHA))
            {
            default:
            case DSBLIT_NOFX:
            case DSBLIT_BLEND_ALPHACHANNEL:
              break;

            case DSBLIT_BLEND_COLORALPHA:
              switch (pd)
                {
                case DSPD_DST:
                  if (D_FLAGS_IS_SET (state->blittingflags,
                                      DSBLIT_DST_PREMULTIPLY))
                    break;
                  /* fall through */

                case DSPD_CLEAR:
                  break;

                case DSPD_SRC:
                case DSPD_SRC_OVER:
                  switch (state->blittingflags & (DSBLIT_SRC_PREMULTCOLOR
                                                  | DSBLIT_SRC_PREMULTIPLY))
                    {
                    case DSBLIT_NOFX:
                      break;

                    case (DSBLIT_SRC_PREMULTCOLOR | DSBLIT_SRC_PREMULTIPLY):
                      if (!canUseHWInputMatrix)
                        break;
                      /* fall through */

                    case DSBLIT_SRC_PREMULTCOLOR:
                      if (!src_premultiply)
                        src_premultcolor = false;
                      /* fall through */

                    case DSBLIT_SRC_PREMULTIPLY:
                      needs_blend = true;
                      src_premultiply = false;
                      break;
                    }
                  break;

                case DSPD_NONE:
                case DSPD_DST_OVER:
                case DSPD_DST_IN:
                case DSPD_DST_OUT:
                  break;

                default:
                  /* not reached */
                  break;
                }
              break;

            case (DSBLIT_BLEND_ALPHACHANNEL | DSBLIT_BLEND_COLORALPHA):
              switch (pd)
                {
                case DSPD_DST_OVER:
                case DSPD_DST_IN:
                case DSPD_DST_OUT:
                  break;

                case DSPD_DST:
                case DSPD_CLEAR:
                  /* no ops. */
                  break;

                case DSPD_SRC:
                case DSPD_SRC_OVER:
                  if (!D_FLAGS_IS_SET (state->blittingflags,
                                       (DSBLIT_SRC_PREMULTCOLOR
                                        | DSBLIT_SRC_PREMULTIPLY)))
                    break;
                  /* fall through */

                case DSPD_NONE:
                  if (D_FLAGS_IS_SET (state->blittingflags,
                                      DSBLIT_SRC_PREMULTCOLOR)
                      && !canUseHWInputMatrix)
                    {
                      if (D_FLAGS_IS_SET (state->blittingflags,
                                          DSBLIT_SRC_PREMULTIPLY)
                          || pd == DSPD_NONE)
                        break;
                    }

                  needs_blend = true;
                  if (src_premultcolor
                      && !src_premultiply
                      && pd != DSPD_NONE)
                    {
                      src_premultcolor = false;
                    }
                  break;

                default:
                  /* not reached */
                  break;
                }
              break;
            }

          switch (pd)
            {
            case DSPD_SRC:
              if (src_premultiply)
                {
                  needs_blend = true;
                  if (D_FLAGS_IS_SET (state->blittingflags,
                                      DSBLIT_DST_COLORKEY
                                      | DSBLIT_SOURCE2))
                    /* the SRC_PREMULTIPLY is done using a blend with 0.0.0.0
                       coming from S1 color fill, so we can not DST_COLORKEY at
                       the same time */
                    goto out;
                }

            case DSPD_DST:
              if (D_FLAGS_IS_SET (state->blittingflags, DSBLIT_DST_PREMULTIPLY))
                needs_blend = true;
              break;

            case DSPD_SRC_OVER:
              if (D_FLAGS_IS_SET (state->blittingflags, DSBLIT_DST_PREMULTIPLY))
                goto out;
              needs_blend = true;
              break;

            case DSPD_DST_OVER:
              if (D_FLAGS_IS_SET (state->blittingflags, DSBLIT_SRC_PREMULTIPLY))
                /* in this case we end up with a destination premultiply, because
                   we swap FG/BG in the ALU, whereas DirectFB (correctly) still
                   does a source premultiply.
                   FIXME: this probably means we could support
                   DSBLIT_DST_PREMULTIPLY | DSBLIT_DST_PREMULTCOLOR in this
                   case. */
                goto out;
              needs_blend = true;
              break;

            case DSPD_DST_IN:
            case DSPD_DST_OUT:
              if (D_FLAGS_IS_SET (state->blittingflags, DSBLIT_DST_PREMULTIPLY))
                goto out;
              needs_blend = true;
              break;

            case DSPD_NONE:
              if (D_FLAGS_IS_SET (state->blittingflags, DSBLIT_SRC_PREMULTIPLY))
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
              if (DFB_PIXELFORMAT_HAS_ALPHA (state->destination->config.format))
                goto out;
              needs_blend = true;
              break;

            default:
              if (D_FLAGS_IS_SET (state->blittingflags, DSBLIT_DST_PREMULTIPLY))
                goto out;
              break;
            }
        }
      else /* not a blend operation */
        {
          if (D_FLAGS_IS_SET (state->blittingflags, DSBLIT_SRC_PREMULTIPLY))
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
              if (D_FLAGS_IS_SET (state->blittingflags,
                                  (DSBLIT_XOR | DSBLIT_DST_COLORKEY
                                   | DSBLIT_DST_PREMULTIPLY
                                   | DSBLIT_SOURCE2)))
                goto out;

              needs_blend = true;
            }
        }

      if (D_FLAGS_IS_SET (state->blittingflags, DSBLIT_COLORIZE)
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
              if (D_FLAGS_IS_SET (state->blittingflags, DSBLIT_XOR)
                  || needs_blend)
                goto out;
            }
        }


      state->accel |= STGFX2_VALID_BLITTINGFUNCTIONS;
    }

  return;

out:
  _dump_check_state_failed (__FUNCTION__, state, accel);
#if 0
  if (!(DFB_BLITTING_FUNCTION (accel)
        && state->src_blend == DSBF_ZERO
        && state->dst_blend == DSBF_SRCALPHA))
    raise (SIGINT);
#endif

  return;
}



/******** SetState() related things -> helpers ******************************/
bool
_bdisp_state_set_buffer_type (const STGFX2DeviceData * const stdev,
                              u32                    * const ty,
                              DFBSurfacePixelFormat   format,
                              u16                     pitch)
{
  const struct _bdisp_pixelformat_table * const dspf_to_bdisp
    = stdev->features.dfb_dspf_to_bdisp;

  D_ASSERT (format != DSPF_UNKNOWN);

  D_DEBUG_AT (BDisp_State, "  -> %s format: %s pitch: %hu\n", __FUNCTION__,
              dfb_pixelformat_name (format), pitch);

  if (!format)
    return 0;

  *ty = dspf_to_bdisp[DFB_PIXELFORMAT_INDEX (format)].bdisp_type;
  switch (*ty)
    {
    case -1:
      D_ASSERT ("BDisp/BLT: _bdisp_state_set_buffer_type() unsupported pixelformat\n" == NULL);
      return false;

    default:
      *ty |= pitch;
      break;
    }

  return true;
}

static void
_bdisp_state_set_key_color (DFBSurfacePixelFormat  format,
                            u32                    colorkey,
                            u32                   * const reg)
{
  unsigned long tmp;

  D_DEBUG_AT (BDisp_State, "  -> %s colorkey: %.8x\n", __FUNCTION__,
              colorkey);

  /* convert colorkey into a 32bit RGB as required by the hardware. We do
     this by padding the missing LSBs with the MSBs to match the hardware
     expansion mode we use for subbyte color formats. */
  switch (format)
  {
    case DSPF_RGB16:
    case DSPF_ARGB8565:
      tmp = RGB16_TO_RGB32 (colorkey) | ((colorkey & 0xe000) << 3)
                                      | ((colorkey & 0x0600) >> 1)
                                      | ((colorkey & 0x001c) >> 2);
      break;

    case DSPF_ARGB1555:
      tmp = ARGB1555_TO_RGB32 (colorkey) | ((colorkey & 0x7000) << 4)
                                         | ((colorkey & 0x0380) << 1)
                                         | ((colorkey & 0x001c) >> 2);
      break;

    case DSPF_ARGB4444:
      /* ARGB4444_TO_RGB32() already color expands for us */
      tmp = ARGB4444_TO_RGB32 (colorkey);
      break;

    case DSPF_RGB24:
    case DSPF_RGB32:
    case DSPF_ARGB:
    case DSPF_A8:
    case DSPF_YUY2:
    case DSPF_UYVY:
    case DSPF_LUT8:
    case DSPF_ALUT44:
    case DSPF_A1:
    case DSPF_LUT2:
    case DSPF_AVYU:
    case DSPF_VYU:
    case DSPF_NV12:
    case DSPF_NV16:
    case DSPF_I420:
    case DSPF_YV12:
    case DSPF_YV16:
    case DSPF_YUV444P:
      tmp = colorkey & 0x00ffffff;
      break;

    default:
      D_ASSERT ("BDisp/BLT: bdisp_aq_SetKeyColor(): unknown pixelformat?!\n" == NULL);
      tmp = colorkey & 0x00ffffff;
      break;
  }

  /* preserve the alpha in the destination as it may be significant for 1bit
     alpha modes. */
  *reg = (*reg & 0xff000000) | tmp;

  if (colorkey != *reg)
    D_DEBUG_AT (BDisp_State, "    -> %.8x\n", *reg);
}

static void
bdisp_state_validate_PALETTE_DRAW (const STGFX2DriverData * const stdrv,
                                   STGFX2DeviceData       * const stdev,
                                   const CardState        * const state)
{
  D_DEBUG_AT (BDisp_State, "%s\n", __FUNCTION__);

  if (state->dst.buffer->format == DSPF_RGB32)
    {
      /* need a palette to make RGB32 usable */
      stdev->drawstate.ConfigGeneral.BLT_CIC |= CIC_NODE_CLUT;
      stdev->drawstate.ConfigGeneral.BLT_INS |= BLIT_INS_ENABLE_CLUT;
      /* use a palette to 'correct' all x in xRGB32 to 0xff */
      stdev->drawstate.ConfigClut.BLT_CCO = BLIT_CCO_CLUT_CORRECT;
      /* palette works on source 1, as we read from there */
      stdev->drawstate.ConfigClut.BLT_CCO |= BLIT_CCO_CLUT_NS2_S1_ON_S1;
      stdev->drawstate.ConfigClut.BLT_CCO |= BLIT_CCO_CLUT_UPDATE_EN;
      stdev->drawstate.ConfigClut.BLT_CML = stdev->clut_phys[SG2C_ONEALPHA_RGB];
    }
  else
    {
      stdev->drawstate.ConfigGeneral.BLT_CIC &= ~CIC_NODE_CLUT;
      stdev->drawstate.ConfigGeneral.BLT_INS &= ~BLIT_INS_ENABLE_CLUT;
    }

  /* done */
  BDISP_STATE_VALIDATED (PALETTE_DRAW);
}

/*********************************************************************/
/* Store the destination surface state in the device state structure */
/*********************************************************************/
static void
bdisp_state_validate_DESTINATION (const STGFX2DriverData * const stdrv,
                                  STGFX2DeviceData       * const stdev,
                                  const CardState        * const state)
{
  D_DEBUG_AT (BDisp_State, "%s phys+offset: %.8lx + %.8lx = %.8lx\n",
              __FUNCTION__, state->dst.phys - state->dst.offset,
              state->dst.offset, state->dst.phys);

  _bdisp_state_set_buffer_type (stdev, &stdev->ConfigTarget.BLT_TTY,
                                state->dst.buffer->format, state->dst.pitch);
  stdev->ConfigTarget.BLT_TBA = state->dst.phys;

  stdev->all_states.extra_blt_ins = 0;
  stdev->all_states.extra_blt_cic = 0;
  if (state->dst.buffer->format == DSPF_RGB32)
    {
      stdev->all_states.extra_blt_ins = BLIT_INS_ENABLE_PLANEMASK;
      stdev->all_states.extra_blt_cic = CIC_NODE_FILTERS;
    }

  /* need to invalidate the source flag, which will cause S1 to be updated
     correctly */
  BDISP_STATE_INVALIDATE (SOURCE_SRC1_MODE);
  BDISP_STATE_INVALIDATE (ROTATION); /* configure copy direction */
  BDISP_STATE_INVALIDATE (INPUTMATRIX);
  BDISP_STATE_INVALIDATE (MATRIXCONVERSIONS);

  /* done */
  BDISP_STATE_VALIDATED (DESTINATION);
}

#ifdef STGFX2_SUPPORT_HW_CLIPPING
static bool
bdisp_state_validate_CLIP (const STGFX2DriverData * const stdrv,
                           STGFX2DeviceData       * const stdev,
                           const CardState        * const state)
{
  D_DEBUG_AT (BDisp_State, "%s %d,%d - %dx%d\n",
              __FUNCTION__, DFB_REGION_VALS (&state->clip));

  stdev->ConfigClip.BLT_CWO = (0 << 31) | (state->clip.y1 << 16) | state->clip.x1;
  stdev->ConfigClip.BLT_CWS = (state->clip.y2 << 16) | state->clip.x2;

  /* done */
  BDISP_STATE_VALIDATED (CLIP);

  return true;
}
#endif

/**************************************************************/
/* Set the source surface state in the device state structure */
/**************************************************************/
static void
bdisp_state_validate_PALETTE_BLIT (const STGFX2DriverData * const stdrv,
                                   STGFX2DeviceData       * const stdev,
                                   const CardState        * const state)
{
  D_DEBUG_AT (BDisp_State, "%s\n", __FUNCTION__);

  /* for blitting op */
  switch (state->source->config.format)
    {
    case DSPF_LUT2:
    case DSPF_LUT8:
    case DSPF_ALUT44:
      stdev->palette = state->source->palette;
      stdev->palette_type = SG2C_NORMAL;
      break;

    default:
      stdev->palette = NULL;
      stdev->palette_type = SG2C_NORMAL;
      break;
    }

  /* done */
  BDISP_STATE_VALIDATED (PALETTE_BLIT);
}

static void
bdisp_state_validate_SOURCE_SRC1_MODE (const STGFX2DriverData * const stdrv,
                                       STGFX2DeviceData       * const stdev,
                                       const CardState        * const state,
                                       DFBAccelerationMask     accel)
{
  D_DEBUG_AT (BDisp_State, "%s\n", __FUNCTION__);

  stdev->ConfigGeneral.BLT_CIC |= CIC_NODE_SOURCE1;
  stdev->ConfigGeneral.BLT_INS &= ~BLIT_INS_SRC1_MODE_MASK;

  switch (state->source->config.format)
    {
    case DSPF_I420:
      stdev->ConfigGeneral.BLT_INS |= BLIT_INS_SRC1_MODE_MEMORY;
      stdev->ConfigSource1.BLT_S1BA = (stdev->ConfigSource2.BLT_S2BA
                                       + ((state->src.pitch
                                           / stdev->srcFactorH)
                                          * (state->source->config.size.h
                                             / stdev->srcFactorV))
                                      );
      stdev->ConfigSource1.BLT_S1TY = stdev->ConfigSource2.BLT_S2TY;
      break;

    case DSPF_YV12:
      /* fall through */
    case DSPF_YV16:
      stdev->ConfigGeneral.BLT_INS |= BLIT_INS_SRC1_MODE_MEMORY;
      stdev->ConfigSource1.BLT_S1BA = (stdev->ConfigSource2.BLT_S2BA
                                       - ((state->src.pitch
                                           / stdev->srcFactorH)
                                          * (state->source->config.size.h
                                             / stdev->srcFactorV))
                                      );
      stdev->ConfigSource1.BLT_S1TY = stdev->ConfigSource2.BLT_S2TY;
      break;

    case DSPF_YUV444P:
      stdev->ConfigGeneral.BLT_INS |= BLIT_INS_SRC1_MODE_MEMORY;
      stdev->ConfigSource1.BLT_S1BA = (stdev->ConfigSource2.BLT_S2BA
                                       + (state->src.pitch
                                          * state->source->config.size.h)
                                      );
      stdev->ConfigSource1.BLT_S1TY = stdev->ConfigSource2.BLT_S2TY;
      break;

    case DSPF_NV12:
    case DSPF_NV16:
    default:
      /* for the formats that don't need S1 (BLIT_INS_SRC1_MODE_DISABLED) we
         obviously still need to set correct data into ConfigSource1 as it might
         get enabled due to some blitting flags, e.g. blend. */
      if (!D_FLAGS_IS_SET (state->blittingflags, DSBLIT_SOURCE2)
          && !D_FLAGS_IS_SET (accel, DFXL_BLIT2))
        {
          stdev->ConfigGeneral.BLT_INS |= (BLIT_INS_SRC1_MODE_DISABLED
                                           | stdev->blitstate.blt_ins_src1_mode);
          stdev->ConfigSource1.BLT_S1BA = stdev->ConfigTarget.BLT_TBA;
          stdev->ConfigSource1.BLT_S1TY = (stdev->ConfigTarget.BLT_TTY
                                           | BLIT_TY_COLOR_EXPAND_MSB);
        }
      else
        {
          D_ASSERT ((stdev->blitstate.blt_ins_src1_mode == BLIT_INS_SRC1_MODE_MEMORY)
                    || (stdev->blitstate.blt_ins_src1_mode == BLIT_INS_SRC1_MODE_DISABLED));

          stdev->ConfigGeneral.BLT_INS |= BLIT_INS_SRC1_MODE_MEMORY;
          stdev->ConfigSource1.BLT_S1BA = state->src2.phys;
          _bdisp_state_set_buffer_type (stdev, &stdev->ConfigSource1.BLT_S1TY,
                                        state->src2.buffer->format,
                                        state->src2.pitch);
          stdev->ConfigSource1.BLT_S1TY |= BLIT_TY_COLOR_EXPAND_MSB;
        }
      break;
    }

  /* done */
  BDISP_STATE_VALIDATED (SOURCE_SRC1_MODE);
}

static void
bdisp_state_validate_SOURCE (const STGFX2DriverData * const stdrv,
                             STGFX2DeviceData       * const stdev,
                             const CardState        * const state)
{
  D_DEBUG_AT (BDisp_State, "%s phys+offset: %.8lx + %.8lx = %.8lx\n",
              __FUNCTION__, state->src.phys - state->src.offset,
              state->src.offset, state->src.phys);

  D_ASSERT (state->source != NULL);
  stdev->source_w = DFB_FIXED_POINT_VAL (state->source->config.size.w);
  stdev->source_h = DFB_FIXED_POINT_VAL (state->source->config.size.h);

  stdev->ConfigGeneral.BLT_CIC &= ~(CIC_NODE_SOURCE2
                                    | CIC_NODE_SOURCE3);
  stdev->ConfigGeneral.BLT_INS &= ~(BLIT_INS_SRC2_MODE_MASK
                                    | BLIT_INS_SRC3_MODE_MASK);
  /* previously, NV12 and NV16 did not have CIC_NODE_SOURCE1 set. As we don't
     allow any blending involving these two formats that should not have
     mattered, but setting it shouldn't do any harm either. */
  stdev->ConfigGeneral.BLT_CIC |= CIC_NODE_SOURCE2;
  stdev->ConfigGeneral.BLT_INS |= BLIT_INS_SRC2_MODE_MEMORY;

  stdev->srcFactorH = stdev->srcFactorV = 1;

  switch (state->source->config.format)
    {
    case DSPF_I420:
      stdev->srcFactorH = stdev->srcFactorV = 2;
      stdev->ConfigGeneral.BLT_CIC |= CIC_NODE_SOURCE3;
      stdev->ConfigGeneral.BLT_INS |= BLIT_INS_SRC3_MODE_MEMORY;

      stdev->ConfigSource3.BLT_S3BA = state->src.phys;
      stdev->ConfigSource2.BLT_S2BA = (stdev->ConfigSource3.BLT_S3BA
                                       + (state->src.pitch
                                          * state->source->config.size.h));

      _bdisp_state_set_buffer_type (stdev, &stdev->ConfigSource3.BLT_S3TY,
                                    state->src.buffer->format,
                                    state->src.pitch);
      stdev->ConfigSource3.BLT_S3TY |= BLIT_TY_COLOR_EXPAND_MSB;
      stdev->ConfigSource2.BLT_S2TY
        = ((stdev->ConfigSource3.BLT_S3TY & ~BLIT_TY_PIXMAP_PITCH_MASK)
           | (state->src.pitch / stdev->srcFactorH));
      break;

    case DSPF_YV12:
      stdev->srcFactorV = 2;
      /* fall through */
    case DSPF_YV16:
      stdev->srcFactorH = 2;
      stdev->ConfigGeneral.BLT_CIC |= CIC_NODE_SOURCE3;
      stdev->ConfigGeneral.BLT_INS |= BLIT_INS_SRC3_MODE_MEMORY;

      stdev->ConfigSource3.BLT_S3BA = state->src.phys;
      stdev->ConfigSource2.BLT_S2BA = (stdev->ConfigSource3.BLT_S3BA
                                       + (state->src.pitch
                                          * state->source->config.size.h)
                                       + ((state->src.pitch
                                           / stdev->srcFactorH)
                                          * (state->source->config.size.h
                                             / stdev->srcFactorV))
                                      );

      _bdisp_state_set_buffer_type (stdev, &stdev->ConfigSource3.BLT_S3TY,
                                    state->src.buffer->format,
                                    state->src.pitch);
      stdev->ConfigSource3.BLT_S3TY |= BLIT_TY_COLOR_EXPAND_MSB;
      stdev->ConfigSource2.BLT_S2TY
        = ((stdev->ConfigSource3.BLT_S3TY & ~BLIT_TY_PIXMAP_PITCH_MASK)
           | (state->src.pitch / stdev->srcFactorH));
      break;

    case DSPF_YUV444P:
      stdev->ConfigGeneral.BLT_CIC |= CIC_NODE_SOURCE3;
      stdev->ConfigGeneral.BLT_INS |= BLIT_INS_SRC3_MODE_MEMORY;

      stdev->ConfigSource3.BLT_S3BA = state->src.phys;
      stdev->ConfigSource2.BLT_S2BA = (stdev->ConfigSource3.BLT_S3BA
                                       + (state->src.pitch
                                          * state->source->config.size.h));

      _bdisp_state_set_buffer_type (stdev, &stdev->ConfigSource3.BLT_S3TY,
                                    state->src.buffer->format,
                                    state->src.pitch);
      stdev->ConfigSource3.BLT_S3TY |= BLIT_TY_COLOR_EXPAND_MSB;
      stdev->ConfigSource2.BLT_S2TY
        = stdev->ConfigSource3.BLT_S3TY;
      break;

    case DSPF_NV12:
      stdev->srcFactorV = 2;
      /* fall through */
    case DSPF_NV16:
      stdev->srcFactorH = 2;
      stdev->ConfigGeneral.BLT_CIC |= CIC_NODE_SOURCE3;
      stdev->ConfigGeneral.BLT_INS |= BLIT_INS_SRC3_MODE_MEMORY;

      stdev->ConfigSource3.BLT_S3BA = state->src.phys;
      stdev->ConfigSource2.BLT_S2BA = (stdev->ConfigSource3.BLT_S3BA
                                       + (state->src.pitch
                                          * state->source->config.size.h));

      _bdisp_state_set_buffer_type (stdev, &stdev->ConfigSource3.BLT_S3TY,
                                    state->src.buffer->format,
                                    state->src.pitch);
      stdev->ConfigSource3.BLT_S3TY |= BLIT_TY_COLOR_EXPAND_MSB;
      stdev->ConfigSource2.BLT_S2TY = stdev->ConfigSource3.BLT_S3TY;
      break;

    default:
      stdev->ConfigGeneral.BLT_INS |= BLIT_INS_SRC3_MODE_DISABLED;

      stdev->ConfigSource3.BLT_S3BA = 0xdeadba5e;
      stdev->ConfigSource2.BLT_S2BA = state->src.phys;

      stdev->ConfigSource3.BLT_S3TY = 0xf0011eef;
      _bdisp_state_set_buffer_type (stdev, &stdev->ConfigSource2.BLT_S2TY,
                                    state->src.buffer->format,
                                    state->src.pitch);
      stdev->ConfigSource2.BLT_S2TY |= BLIT_TY_COLOR_EXPAND_MSB;
      break;
    }

  /* as we touch the source1 mode, make sure it stays correct. */
  BDISP_STATE_INVALIDATE (SOURCE_SRC1_MODE);
  BDISP_STATE_INVALIDATE (BLITTINGFLAGS);
  BDISP_STATE_INVALIDATE (ROTATION); /* configure copy direction */
  BDISP_STATE_INVALIDATE (INPUTMATRIX);
  BDISP_STATE_INVALIDATE (MATRIXCONVERSIONS);

  /* done */
  BDISP_STATE_VALIDATED (SOURCE);
}

static void
bdisp_state_validate_FILLCOLOR (const STGFX2DriverData * const stdrv,
                                STGFX2DeviceData       * const stdev,
                                const CardState        * const state)
{
  u32 alpha = state->color.a;
  u32 red   = state->color.r;
  u32 green = state->color.g;
  u32 blue  = state->color.b;

  D_DEBUG_AT (BDisp_State, "%s αrgb %.2x%s%.2x%.2x%.2x%s\n",
              __FUNCTION__,
              alpha,
              (state->drawingflags & DSDRAW_SRC_PREMULTIPLY) ? "*" : "",
              red, green, blue,
              (state->drawingflags & DSDRAW_BLEND) ? " (blend)" : "");

  if (state->drawingflags & DSDRAW_SRC_PREMULTIPLY)
    {
      /* we do source alpha pre-multiplication now, rather in the hardware
         so we can support it regardless of the blend mode. */
      red   = (red   * (alpha + 1)) >> 8;
      green = (green * (alpha + 1)) >> 8;
      blue  = (blue  * (alpha + 1)) >> 8;
    }

  if (state->drawingflags & DSDRAW_BLEND)
    {
      switch (stdev->porter_duff_rule)
        {
        case DSPD_CLEAR:
          /* Just fill with 0. Set the source format to the dest format to get
             a fast fill. */
          stdev->drawstate.color = 0;
          switch (state->destination->config.format)
            {
            case DSPF_YUY2:
            case DSPF_UYVY:
            case DSPF_AVYU:
            case DSPF_VYU:
              /* full alpha ange doesn't matter here as it's 0 anyway */
              stdev->drawstate.color_ty = BLIT_COLOR_FORM_ARGB8888;
              break;

            case DSPF_RGB32:
              stdev->drawstate.color = PIXEL_ARGB (0xff, 0x00, 0x00, 0x00);
              /* fall through */
            default:
              /* FIXME - unsetting big endian here means that further down
                 when the function pointers are updated a clear to a big
                 endian surface (BGRA8888) will end up in the slow path, as
                 BLT_TTY and color_ty will not be identical anymore. That's
                 not an issue right now, as BGRA8888 is not a supported
                 DirectFB format at the moment, and it is the only non YUV
                 format with the big endian flag set. */
              stdev->drawstate.color_ty =
                (bdisp_ty_get_format_from_ty (stdev->ConfigTarget.BLT_TTY)
                 & ~BLIT_TY_BIG_ENDIAN);
              break;
            }
          break;

        case DSPD_SRC: /* FIXME: why not the destination format??? */
          if (unlikely (state->destination->config.format == DSPF_RGB32))
            alpha = 0xff;
        case DSPD_DST_OVER:
        case DSPD_SRC_OVER:
        case DSPD_NONE:
          /* for blended fills we use ARGB8888, to get full alpha precision in the
             blend, the hardware does color conversion. */
          stdev->drawstate.color = PIXEL_ARGB (alpha, red, green, blue);
          stdev->drawstate.color_ty = (BLIT_COLOR_FORM_ARGB8888
                                       | BLIT_TY_FULL_ALPHA_RANGE);
          break;

        case DSPD_DST:
          /* This leaves the destination as is, i.e. a nop, because we do not
             support destination pre-multiplication. */
        default:
          /* not supported */
          break;
        }
    }
  else
    {
      stdev->drawstate.color_ty
        = bdisp_ty_get_format_from_ty (stdev->ConfigTarget.BLT_TTY);

      switch (state->destination->config.format)
        {
        case DSPF_ARGB1555:
          stdev->drawstate.color = PIXEL_ARGB1555 (alpha, red, green, blue);
          break;

        case DSPF_ARGB4444:
          stdev->drawstate.color = PIXEL_ARGB4444 (alpha, red, green, blue);
          break;

        case DSPF_RGB16:
          stdev->drawstate.color = PIXEL_RGB16 (red, green, blue);
          break;

        case DSPF_RGB24:
          stdev->drawstate.color = PIXEL_RGB32 (red, green, blue);
          break;

        case DSPF_RGB32:
          stdev->drawstate.color = PIXEL_ARGB (0xff, red, green, blue);
          break;

        case DSPF_ARGB8565:
          stdev->drawstate.color = PIXEL_ARGB8565 (alpha, red, green, blue);
          break;

        /* For 422R we let the hardware do the conversion, as direct fast fill
           in 422R has problems (probably due to two pixels being mixed up in
           a 32bit word). The other reason we are using the hardware is that
           this way the conversion RGB->YCbCr will be consistent (DirectFB
           has no way to specify the color space). */
        case DSPF_YUY2:
        case DSPF_UYVY:
        case DSPF_AVYU:
        case DSPF_VYU:
          /* let the blitter do the color conversion (on the slow path) */
          stdev->drawstate.color_ty = (BLIT_COLOR_FORM_ARGB8888
                                       | BLIT_TY_FULL_ALPHA_RANGE);
          /* fall through */
        case DSPF_ARGB:
          stdev->drawstate.color = PIXEL_ARGB (alpha, red, green, blue);
          break;

        case DSPF_LUT2:
        case DSPF_LUT8:
          stdev->drawstate.color = state->color_index;
          break;

        case DSPF_ALUT44:
          stdev->drawstate.color = (alpha & 0xf0) | (state->color_index & 0x0f);
          break;

        case DSPF_A8:
          stdev->drawstate.color = alpha;
          break;

        default:
          D_ERROR ("stgfx2/state: Unexpected color format %x\n",
                   state->destination->config.format);
          break;
        }
    }


  stdev->drawstate.ConfigGeneral.BLT_CIC &= ~CIC_NODE_IVMX;
  stdev->drawstate.ConfigGeneral.BLT_INS &= ~BLIT_INS_ENABLE_IVMX;
  if (BLIT_TY_COLOR_FORM_IS_YUV (stdev->ConfigTarget.BLT_TTY)
      && !BLIT_TY_COLOR_FORM_IS_YUV (stdev->drawstate.color_ty))
    {
      D_DEBUG ("%s(): Input RGB->YUV\n", __FUNCTION__);

      stdev->drawstate.ConfigGeneral.BLT_CIC |= CIC_NODE_IVMX;
      stdev->drawstate.ConfigGeneral.BLT_INS |= BLIT_INS_ENABLE_IVMX;
    }

  /* done */
  BDISP_STATE_VALIDATED (FILLCOLOR);
}

static void
bdisp_state_validate_DRAWINGFLAGS (STGFX2DriverData * const stdrv,
                                   STGFX2DeviceData * const stdev,
                                   const CardState  * const state)
{
  static const u32 global_alpha = 128;
  u32              blt_ins;
  u32              blt_ack;

  D_DEBUG_AT (BDisp_State, "%s drawflags %.2x\n",
              __FUNCTION__, state->drawingflags);

  blt_ins = stdev->drawstate.ConfigGeneral.BLT_INS;
  blt_ack = stdev->drawstate.ConfigGeneral.BLT_ACK;

  blt_ins &= ~(BLIT_INS_SRC1_MODE_MASK | BLIT_INS_SRC2_MODE_MASK);
  blt_ack &= ~BLIT_ACK_SWAP_FG_BG;
  blt_ack &= ~(BLIT_ACK_MODE_MASK
               | BLIT_ACK_GLOBAL_ALPHA_MASK | BLIT_ACK_ROP_MASK
               | BLIT_ACK_CKEY_MASK | BLIT_ACK_COLORKEYING_MASK
              );

  if (state->drawingflags & DSDRAW_BLEND)
    {
      switch (stdev->porter_duff_rule)
        {
        case DSPD_DST:
          return;

        case DSPD_DST_OVER:
          /* src func = 1-dst alpha, dest func = 1.0   (DSPD_DST_OVER) */
          blt_ack |= BLIT_ACK_SWAP_FG_BG;
          /* fall through */
        case DSPD_SRC_OVER:
          /* src func = 1.0  , dest func = 1-src alpha (DSPD_SRC_OVER) */
          blt_ack |= ((global_alpha << BLIT_ACK_GLOBAL_ALPHA_SHIFT)
                      | BLIT_ACK_BLEND_SRC2_PREMULT);
          blt_ins |= BLIT_INS_SRC1_MODE_MEMORY | BLIT_INS_SRC2_MODE_COLOR_FILL;
          break;
        case DSPD_NONE:
          /* src func = src alpha, dest func = 1-src alpha (DSPD_NONE)
             We can only support this when the destination format has no alpha
             channel. This is because the hardware always calculates the
             destination alpha as Asrc + Adst*(1-Asrc) but we would require
             Asrc*Asrc + Adst*(1-Asrc). */
          D_DEBUG ("%s(): setting src as NOT pre-multiplied\n", __FUNCTION__);
          blt_ack |= ((global_alpha << BLIT_ACK_GLOBAL_ALPHA_SHIFT)
                      | BLIT_ACK_BLEND_SRC2_N_PREMULT);
          blt_ins |= BLIT_INS_SRC1_MODE_MEMORY | BLIT_INS_SRC2_MODE_COLOR_FILL;
          break;

        case DSPD_CLEAR:
          /* for clear, we would normally just use a direct (fast) fill, but
             we can't for YUV formats as the blitter needs to convert ARGB
             0x00000000 into YUV space */
          if (bdisp_ty_formats_identical (stdev->drawstate.color_ty,
                                          stdev->ConfigTarget.BLT_TTY))
            {
              blt_ins |= BLIT_INS_SRC1_MODE_DIRECT_FILL;
              if (state->destination->config.format == DSPF_RGB32)
                {
                  stdev->drawstate.ConfigGeneral.BLT_CIC &= ~CIC_NODE_CLUT;
                  blt_ins &= ~BLIT_INS_ENABLE_CLUT;
                }
              else
                {
                  D_ASSUME ((blt_ins & BLIT_INS_ENABLE_CLUT) == 0);
                  D_ASSUME ((stdev->drawstate.ConfigGeneral.BLT_CIC
                             & CIC_NODE_CLUT) == 0);
                }
            }
          else
            {
              /* let the blitter do the color conversion (on the slow path) */
              blt_ack |= BLIT_ACK_BYPASSSOURCE2;
              blt_ins |= BLIT_INS_SRC1_MODE_DISABLED | BLIT_INS_SRC2_MODE_COLOR_FILL;
            }
          break;

        case DSPD_SRC:
          /* color is in ARGB, see FIXME above */
          if (bdisp_ty_formats_identical (stdev->drawstate.color_ty,
                                          stdev->ConfigTarget.BLT_TTY))
            {
              blt_ins |= BLIT_INS_SRC1_MODE_DIRECT_FILL;
              if (state->destination->config.format == DSPF_RGB32)
                {
                  stdev->drawstate.ConfigGeneral.BLT_CIC &= ~CIC_NODE_CLUT;
                  blt_ins &= ~BLIT_INS_ENABLE_CLUT;
                }
              else
                {
                  D_ASSUME ((blt_ins & BLIT_INS_ENABLE_CLUT) == 0);
                  D_ASSUME ((stdev->drawstate.ConfigGeneral.BLT_CIC
                             & CIC_NODE_CLUT) == 0);
                }
            }
          else
            {
              /* need the slow path for a blend */
              blt_ack |= BLIT_ACK_BYPASSSOURCE2;
              blt_ins |= BLIT_INS_SRC1_MODE_DISABLED | BLIT_INS_SRC2_MODE_COLOR_FILL;
            }
          break;

        default:
          D_WARN ("%s: unexpected Porter/Duff rule: %d\n", __FUNCTION__,
                  stdev->porter_duff_rule);
          return;
        }
    }
  else if (state->drawingflags & DSDRAW_XOR)
    {
      D_DEBUG ("%s(): XOR\n", __FUNCTION__);
      blt_ack |= (0
                  | BLIT_ACK_ROP
                  | BLIT_ACK_ROP_XOR
                 );
      blt_ins |= BLIT_INS_SRC1_MODE_MEMORY | BLIT_INS_SRC2_MODE_COLOR_FILL;
    }
  else if (!(state->drawingflags & DSDRAW_DST_COLORKEY))
    {
      /* fast path */
      blt_ack |= BLIT_ACK_BYPASSSOURCE2;
      if (bdisp_ty_formats_identical (stdev->drawstate.color_ty,
                                      stdev->ConfigTarget.BLT_TTY)
          && !(blt_ins & BLIT_INS_ENABLE_CLUT))

        {
          D_DEBUG ("%s(): fast path\n", __FUNCTION__);
          blt_ins |= BLIT_INS_SRC1_MODE_DIRECT_FILL;
        }
      else
        {
          blt_ack |= BLIT_ACK_BYPASSSOURCE2;
          blt_ins |= BLIT_INS_SRC1_MODE_DISABLED | BLIT_INS_SRC2_MODE_COLOR_FILL;
        }
    }

  if (state->drawingflags & DSDRAW_DST_COLORKEY)
    {
      stdev->drawstate.ConfigGeneral.BLT_CIC |= CIC_NODE_COLORKEY;
      blt_ins |= BLIT_INS_ENABLE_COLORKEY;

      blt_ack |= BLIT_ACK_CKEY_RGB_ENABLE;
      if (blt_ins & BLIT_INS_SRC1_MODE_MEMORY)
        blt_ack |= BLIT_ACK_COLORKEYING_DEST_KEY_ZEROS_SRC_ALPHA;
      else
        blt_ack |= (0
                    | BLIT_ACK_ROP
                    | BLIT_ACK_ROP_COPY
                    | BLIT_ACK_COLORKEYING_DEST
                   );

      /* clear potential BLIT_INS_SRC1_MODE_DIRECT_FILL */
      blt_ins &= ~BLIT_INS_SRC1_MODE_MASK;
      blt_ins |= BLIT_INS_SRC1_MODE_MEMORY;
    }
  else
    {
      stdev->drawstate.ConfigGeneral.BLT_CIC &= ~CIC_NODE_COLORKEY;
      blt_ins &= ~BLIT_INS_ENABLE_COLORKEY;
    }

  stdev->drawstate.ConfigGeneral.BLT_INS = blt_ins;
  stdev->drawstate.ConfigGeneral.BLT_ACK = blt_ack;

  /* done */
  BDISP_STATE_VALIDATED (DRAWINGFLAGS);
}

#define CLUT_SIZE (256 * sizeof (u32))
static void
bdisp_state_validate_BLITCOLOR (STGFX2DriverData * const stdrv,
                                STGFX2DeviceData * const stdev,
                                const CardState  * const state)
{
  D_DEBUG_AT (BDisp_State, "%s αrgb %.2x%.2x%.2x%.2x\n",
              __FUNCTION__, state->color.a, state->color.r, state->color.g,
              state->color.b);

  if (D_FLAGS_IS_SET (state->blittingflags, (DSBLIT_BLEND_COLORALPHA
                                             | DSBLIT_BLEND_ALPHACHANNEL)))
    {
      u32         palette1[256];
      u32        *palette_pass1 = palette1;
      Stgfx2Clut  palette_type1 = SG2C_NORMAL;
      int         i;
      /* scale 0..255 to 0..128 (not 127!) */
      u32 alpha = (state->color.a + 1) / 2;

      switch (state->blittingflags & (DSBLIT_BLEND_COLORALPHA
                                      | DSBLIT_BLEND_ALPHACHANNEL))
        {
        case DSBLIT_NOFX:
        case DSBLIT_BLEND_ALPHACHANNEL:
          break;

        case DSBLIT_BLEND_COLORALPHA:
          D_DEBUG_AT (BDisp_State,
                      "  -> BLEND_COLORALPHA: α %.2x\n", state->color.a);
          if (stdev->blitstate.isOptimisedModulation)
            {
              D_DEBUG_AT (BDisp_State, "    -> optimised using global α\n");

              if (DFB_PIXELFORMAT_HAS_ALPHA (state->source->config.format))
                stdev->palette_type = SG2C_ONEALPHA_RGB;

              stdev->ConfigGeneral.BLT_ACK &= ~BLIT_ACK_GLOBAL_ALPHA_MASK;
              stdev->ConfigGeneral.BLT_ACK |= ((alpha & 0xff) << BLIT_ACK_GLOBAL_ALPHA_SHIFT);
            }
          else
            {
              /* replace alpha using clut */
              D_DEBUG_AT (BDisp_State, "    -> slow (palette replacement)\n");

              for (i = 0; i <= 0x80; ++i)
                palette1[i] = PIXEL_ARGB (alpha, i, i, i);
              for (; i < 256; ++i)
                palette1[i] = PIXEL_ARGB (0, i, i, i);
              palette_type1 = SG2C_COLORALPHA;
            }
          break;

        case (DSBLIT_BLEND_ALPHACHANNEL | DSBLIT_BLEND_COLORALPHA):
          D_DEBUG_AT (BDisp_State,
                      "  -> BLEND_COLORALPHA | BLEND_ALPHACHANNEL: α %.2x\n",
                      state->color.a);
          if (stdev->blitstate.isOptimisedModulation)
            {
              D_DEBUG_AT (BDisp_State, "    -> optimised using global α\n");

              stdev->ConfigGeneral.BLT_ACK &= ~BLIT_ACK_GLOBAL_ALPHA_MASK;
              stdev->ConfigGeneral.BLT_ACK |= ((alpha & 0xff) << BLIT_ACK_GLOBAL_ALPHA_SHIFT);
            }
          else
            {
              /* replace alpha using clut */
              D_DEBUG_AT (BDisp_State, "    -> slow (palette replacement)\n");

              for (i = 0; i <= 0x80; ++i)
                palette1[i] = PIXEL_ARGB ((i * alpha / 0x80), i, i, i);
              for (; i < 256; ++i)
                palette1[i] = PIXEL_ARGB (0, i, i, i);
              palette_type1 = SG2C_ALPHA_MUL_COLORALPHA;
            }
          break;
        }

      if (palette_type1 != SG2C_NORMAL)
        {
          u32         palette2[256];
          u32        *palette_pass2 = palette2;
          Stgfx2Clut  palette_type2 = SG2C_NORMAL;

#if D_DEBUG_ENABLED
          const char *
          palette_to_string (Stgfx2Clut palette)
          {
            #define _CMP(p)     \
              if (palette == p) \
                return #p;

            _CMP(SG2C_NORMAL);
            _CMP(SG2C_COLORALPHA);
            _CMP(SG2C_INVCOLORALPHA);
            _CMP(SG2C_ALPHA_MUL_COLORALPHA);
            _CMP(SG2C_INV_ALPHA_MUL_COLORALPHA);
            _CMP(SG2C_ONEALPHA_RGB);
            _CMP(SG2C_INVALPHA_ZERORGB);
            _CMP(SG2C_ALPHA_ZERORGB);
            _CMP(SG2C_ZEROALPHA_RGB);
            _CMP(SG2C_ZEROALPHA_ZERORGB);
            _CMP(SG2C_ONEALPHA_ZERORGB);
            _CMP(SG2C_COUNT);

            #undef _CMP

            return "unknown";
          }
#endif

          D_DEBUG_AT (BDisp_State, "    -> palette1: %s\n",
                      palette_to_string (palette_type1));

          if (stdev->porter_duff_rule == DSPD_DST_IN
              || stdev->porter_duff_rule == DSPD_DST_OUT)
            {
              D_DEBUG_AT (BDisp_State,
                          "  -> DSPD_DST_IN/OUT/ATOP: mod'ing palette(s)\n");

              for (i = 0; i <= 0x80; ++i)
                palette2[i] = (0x80 - (palette1[i] >> 24)) << 24;
              memset (&palette2[i], 0x00, CLUT_SIZE - (i * sizeof (u32)));

              if (stdev->porter_duff_rule == DSPD_DST_IN)
                {
                  D_DEBUG_AT (BDisp_State,
                              "  -> DSPD_DST_IN: palette1 before: %s\n",
                              palette_to_string (palette_type1));

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

                  D_DEBUG_AT (BDisp_State,
                              "  -> DSPD_DST_IN: palettes after: %s/%s\n",
                              palette_to_string (palette_type1),
                              palette_to_string (palette_type2));
                }
              else if (stdev->porter_duff_rule == DSPD_DST_OUT)
                {
                  for (i = 0; i <= 0x80; ++i)
                    palette1[i] &= 0xff000000;
                  memset (&palette1[i], 0x00, CLUT_SIZE - (i * sizeof (u32)));

                  D_DEBUG_AT (BDisp_State,
                              "  -> DSPD_DST_OUT: palette1 before: %s\n",
                              palette_to_string (palette_type1));

                  if (palette_type1 == SG2C_COLORALPHA)
                    palette_type2 = SG2C_INVCOLORALPHA;
                  else if (palette_type1 == SG2C_ALPHA_MUL_COLORALPHA)
                    palette_type2 = SG2C_INV_ALPHA_MUL_COLORALPHA;

                  D_DEBUG_AT (BDisp_State,
                              "  -> DSPD_DST_OUT: palettes after: %s/%s\n",
                              palette_to_string (palette_type1),
                              palette_to_string (palette_type2));
                }
            }

          /* upload palette(s) into hardware. We must make sure they are not
             used at the moment, which is why we have to wait until the
             hardware is idle. */
          _bdisp_aq_prepare_upload_palette_hw (stdrv, stdev);

          memcpy (stdrv->clut_virt[palette_type1], palette_pass1, CLUT_SIZE);
          stdev->palette_type = palette_type1;

          if (palette_type2 != SG2C_NORMAL)
            {
              memcpy (stdrv->clut_virt[palette_type2], palette_pass2, CLUT_SIZE);
              stdev->blitstate.extra_passes[0].palette_type = palette_type2;
            }
        }
    }

  /* done */
  BDISP_STATE_VALIDATED (BLITCOLOR);
}

static void
bdisp_state_validate_DST_COLORKEY (STGFX2DriverData * const stdrv,
                                   STGFX2DeviceData * const stdev,
                                   const CardState  * const state)
{
  D_DEBUG_AT (BDisp_State, "%s %.8x\n", __FUNCTION__, state->dst_colorkey);

  _bdisp_state_set_key_color (state->destination->config.format,
                              state->dst_colorkey, &stdev->all_states.dst_ckey);

  if (D_FLAGS_IS_SET (state->blittingflags, DSBLIT_DST_COLORKEY))
    {
      stdev->ConfigColorkey.BLT_KEY1 = stdev->all_states.dst_ckey;
      stdev->ConfigColorkey.BLT_KEY2 = stdev->all_states.dst_ckey;
    }

  /* done */
  BDISP_STATE_VALIDATED (DST_COLORKEY);
}

static void
bdisp_state_validate_SRC_COLORKEY (STGFX2DriverData * const stdrv,
                                   STGFX2DeviceData * const stdev,
                                   const CardState  * const state)
{
  D_DEBUG_AT (BDisp_State, "%s %.8x\n", __FUNCTION__, state->src_colorkey);

  _bdisp_state_set_key_color (state->source->config.format,
                              state->src_colorkey,
                              &stdev->blitstate.src_ckey);

  if (D_FLAGS_IS_SET (state->blittingflags, DSBLIT_SRC_COLORKEY))
    {
      stdev->ConfigColorkey.BLT_KEY1 = stdev->blitstate.src_ckey;
      stdev->ConfigColorkey.BLT_KEY2 = stdev->blitstate.src_ckey;
    }

  /* done */
  BDISP_STATE_VALIDATED (SRC_COLORKEY);
}

static void
bdisp_state_validate_ROTATION (STGFX2DriverData    * const stdrv,
                               STGFX2DeviceData    * const stdev,
                               const CardState     * const state,
                               GraphicsDeviceFuncs * const funcs)
{
  D_DEBUG_AT (BDisp_State, "%s\n", __FUNCTION__);

  u32 blt_ins  = stdev->ConfigGeneral.BLT_INS;
  u32 blt_tty  = bdisp_ty_sanitise_direction (stdev->ConfigTarget.BLT_TTY);
  u32 blt_s1ty = bdisp_ty_sanitise_direction (stdev->ConfigSource1.BLT_S1TY);
  u32 blt_s2ty = bdisp_ty_sanitise_direction (stdev->ConfigSource2.BLT_S2TY);

  stdev->rotate = 0;

  if (D_FLAGS_ARE_SET (state->blittingflags, (DSBLIT_FLIP_HORIZONTAL
                                              | DSBLIT_FLIP_VERTICAL))
      || D_FLAGS_IS_SET (state->blittingflags, DSBLIT_ROTATE180))
    stdev->rotate = 180;
  else if (D_FLAGS_IS_SET (state->blittingflags, DSBLIT_FLIP_VERTICAL))
    stdev->rotate = 181;
  else if (D_FLAGS_IS_SET (state->blittingflags, DSBLIT_FLIP_HORIZONTAL))
    stdev->rotate = 182;
  else if (D_FLAGS_IS_SET (state->blittingflags, DSBLIT_ROTATE90))
    stdev->rotate = 90;
  else if (D_FLAGS_IS_SET (state->blittingflags, DSBLIT_ROTATE270))
    stdev->rotate = 270;
  else if (D_FLAGS_IS_SET (state->render_options, DSRO_MATRIX))
    stdev->rotate = bdisp_aq_convert_to_supported_rotation (NULL, 0, state->matrix);

  D_DEBUG_AT (BDisp_State, "  -> %d°\n", stdev->rotate);

  switch (stdev->rotate)
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

  stdev->ConfigGeneral.BLT_INS = blt_ins;
  stdev->ConfigTarget.BLT_TTY = blt_tty;
  stdev->ConfigSource1.BLT_S1TY = blt_s1ty;
  stdev->ConfigSource2.BLT_S2TY = blt_s2ty;
  /* rotation for 3 buffer formats is never supported, so always sanitise it */
  stdev->ConfigSource3.BLT_S3TY = bdisp_ty_sanitise_direction (stdev->ConfigSource3.BLT_S3TY);

  /* done */
  BDISP_STATE_VALIDATED (ROTATION);
}

static void
bdisp_state_validate_RENDEROPTS (STGFX2DriverData * const stdrv,
                                 STGFX2DeviceData * const stdev,
                                 const CardState  * const state)
{
  D_DEBUG_AT (BDisp_State, "%s\n", __FUNCTION__);

  /* smooth value must be the same for up- and downscale */
  stdev->ConfigFilters.BLT_FCTL_RZC &= ~(BLIT_RZC_2DHF_MODE_MASK
                                         | BLIT_RZC_2DVF_MODE_MASK
                                         | BLIT_RZC_Y_2DHF_MODE_MASK
                                         | BLIT_RZC_Y_2DVF_MODE_MASK);
//  stdev->ConfigFilters.BLT_FCTL_RZC &= ~BLIT_RZC_BOUNDARY_BYPASS;

#ifndef STGFX2_FORCE_SMOOTH_SCALE
  if (D_FLAGS_IS_SET (state->render_options, (DSRO_SMOOTH_UPSCALE
                                              | DSRO_SMOOTH_DOWNSCALE)))
    {
      D_DEBUG_AT (BDisp_State, "  -> smooth rescale\n");

      D_ASSERT (D_FLAGS_ARE_SET (state->render_options,
                                 (DSRO_SMOOTH_UPSCALE | DSRO_SMOOTH_DOWNSCALE)));

      stdev->ConfigFilters.BLT_FCTL_RZC |= (0
                                            | BLIT_RZC_2DHF_MODE_FILTER_BOTH
                                            | BLIT_RZC_2DVF_MODE_FILTER_BOTH
                                           );
      if (D_FLAGS_IS_SET (stdev->ConfigGeneral.BLT_CIC, CIC_NODE_SOURCE3))
        stdev->ConfigFilters.BLT_FCTL_RZC |= (0
                                              | BLIT_RZC_Y_2DHF_MODE_FILTER_BOTH
                                              | BLIT_RZC_Y_2DVF_MODE_FILTER_BOTH
                                             );
//      stdev->ConfigFilters.BLT_FCTL_RZC |= BLIT_RZC_BOUNDARY_BYPASS;
    }
  else
    {
      D_DEBUG_AT (BDisp_State, "  -> resize only rescale\n");

      D_ASSERT (!D_FLAGS_IS_SET (state->render_options,
                                 (DSRO_SMOOTH_UPSCALE | DSRO_SMOOTH_DOWNSCALE)));

      stdev->ConfigFilters.BLT_FCTL_RZC |= (0
                                            | BLIT_RZC_2DHF_MODE_RESIZE_ONLY
                                            | BLIT_RZC_2DVF_MODE_RESIZE_ONLY
                                           );
      if (D_FLAGS_IS_SET (stdev->ConfigGeneral.BLT_CIC, CIC_NODE_SOURCE3))
        stdev->ConfigFilters.BLT_FCTL_RZC |= (0
                                              | BLIT_RZC_Y_2DHF_MODE_RESIZE_ONLY
                                              | BLIT_RZC_Y_2DVF_MODE_RESIZE_ONLY
                                             );
    }
#else /* STGFX2_FORCE_SMOOTH_SCALE */
  stdev->ConfigFilters.BLT_FCTL_RZC |= (0
                                        | BLIT_RZC_2DHF_MODE_FILTER_BOTH
                                        | BLIT_RZC_2DVF_MODE_FILTER_BOTH
                                       );
  if (D_FLAGS_IS_SET (stdev->ConfigGeneral.BLT_CIC, CIC_NODE_SOURCE3))
    stdev->ConfigFilters.BLT_FCTL_RZC |= (0
                                          | BLIT_RZC_Y_2DHF_MODE_FILTER_BOTH
                                          | BLIT_RZC_Y_2DVF_MODE_FILTER_BOTH
                                         );
#endif /* STGFX2_FORCE_SMOOTH_SCALE */

  /* done */
  BDISP_STATE_VALIDATED (RENDEROPTS);
}

static void
bdisp_state_validate_BLITTINGFLAGS_CKEY (STGFX2DriverData * const stdrv,
                                         STGFX2DeviceData * const stdev,
                                         const CardState  * const state)
{
  D_DEBUG_AT (BDisp_State, "%s\n", __FUNCTION__);

  u32 blt_cic = stdev->ConfigGeneral.BLT_CIC;
  u32 blt_ins = stdev->ConfigGeneral.BLT_INS;
  u32 blt_ack = stdev->ConfigGeneral.BLT_ACK;

  blt_ack &= ~(BLIT_ACK_CKEY_MASK
               | BLIT_ACK_COLORKEYING_MASK);

  if (D_FLAGS_IS_SET (state->blittingflags, (DSBLIT_DST_COLORKEY
                                             | DSBLIT_SRC_COLORKEY)))
    {
      /* can't have both, source and destination color key set */
      D_ASSERT (!D_FLAGS_ARE_SET (state->blittingflags,
                                  (DSBLIT_DST_COLORKEY | DSBLIT_SRC_COLORKEY)));

      blt_cic |= CIC_NODE_COLORKEY;
      blt_ins |= BLIT_INS_ENABLE_COLORKEY;

      if (D_FLAGS_IS_SET (state->blittingflags, DSBLIT_SRC_COLORKEY))
        {
          D_DEBUG_AT (BDisp_State, "  -> SRC_COLORKEY\n");

          /* for source color keying, we have to set up the mode (before or
             after the CLUT translation) depending on the pixelformat.
             Normally, we wouldn't have to worry, but in CLUT correction
             mode, which we use for supporting RGB32, color keying only works
             if done after the CLUT translation. Don't know why, maybe I just
             didn't see the note about that in the documentation. */
          if (!DFB_PIXELFORMAT_IS_INDEXED (state->source->config.format))
            blt_ack |= (BLIT_ACK_CKEY_RGB_ENABLE
                        | BLIT_ACK_COLORKEYING_SRC_AFTER);
          else
            /* for LUT formats, the color key (i.e. the index) is specified
               in the 'blue' bits of the color key register. */
            blt_ack |= (BLIT_ACK_CKEY_BLUE_ENABLE
                        | BLIT_ACK_COLORKEYING_SRC_BEFORE);

          stdev->ConfigColorkey.BLT_KEY1 = stdev->blitstate.src_ckey;
          stdev->ConfigColorkey.BLT_KEY2 = stdev->blitstate.src_ckey;
        }
      else
        {
          D_DEBUG_AT (BDisp_State, "  -> DST_COLORKEY??\n");

          blt_ack |= BLIT_ACK_COLORKEYING_DEST;
          blt_ack |= BLIT_ACK_CKEY_RGB_ENABLE;

          blt_ack &= ~(BLIT_ACK_ROP_MASK | BLIT_ACK_MODE_MASK);
          blt_ack |= BLIT_ACK_ROP_COPY | BLIT_ACK_ROP;
//          blt_ins &= ~BLIT_INS_SRC1_MODE_MASK;
//          blt_ins |= BLIT_INS_SRC1_MODE_MEMORY;
          blt_ins &= ~BLIT_INS_SRC2_MODE_MASK;
          blt_ins |= BLIT_INS_SRC2_MODE_MEMORY;

          stdev->ConfigColorkey.BLT_KEY1 = stdev->all_states.dst_ckey;
          stdev->ConfigColorkey.BLT_KEY2 = stdev->all_states.dst_ckey;
        }
    }
  else
    {
      blt_cic &= ~CIC_NODE_COLORKEY;
      blt_ins &= ~BLIT_INS_ENABLE_COLORKEY;
    }

  stdev->ConfigGeneral.BLT_CIC = blt_cic;
  stdev->ConfigGeneral.BLT_INS = blt_ins;
  stdev->ConfigGeneral.BLT_ACK = blt_ack;

  /* done */
  BDISP_STATE_VALIDATED (BLITTINGFLAGS_CKEY);
}

static void
bdisp_state_validate_BLITTINGFLAGS (STGFX2DriverData    * const stdrv,
                                    STGFX2DeviceData    * const stdev,
                                    const CardState     * const state,
                                    GraphicsDeviceFuncs * const funcs)
{
  D_DEBUG_AT (BDisp_State, "%s\n", __FUNCTION__);

  stdev->bIndexTranslation = !!(state->blittingflags & DSBLIT_INDEX_TRANSLATION);
  stdev->bFixedPoint       = !!(state->blittingflags & DSBLIT_FIXEDPOINT);

  u32 blt_cic = stdev->ConfigGeneral.BLT_CIC;
  u32 blt_ins = stdev->ConfigGeneral.BLT_INS;
  u32 blt_ack = stdev->ConfigGeneral.BLT_ACK;

  u32 msk_blt_cic[2];
  u32 msk_blt_ins[2];
  u32 msk_blt_ack[2];

  blt_cic &= ~CIC_NODE_COLOR;
  blt_ack &= (BLIT_ACK_CKEY_MASK
              | BLIT_ACK_COLORKEYING_MASK);
  blt_ack &= ~BLIT_ACK_SWAP_FG_BG;

  stdev->blitstate.blt_ins_src1_mode = BLIT_INS_SRC1_MODE_DISABLED;
  BDISP_STATE_INVALIDATE (SOURCE_SRC1_MODE);

  blt_cic &= ~CIC_NODE_FILTERS;
  blt_ins &= ~BLIT_INS_ENABLE_PLANEMASK;

  blt_cic &= ~(CIC_NODE_IVMX | CIC_NODE_OVMX);
  blt_ins &= ~(BLIT_INS_ENABLE_IVMX | BLIT_INS_ENABLE_OVMX);

  stdev->blitstate.flags = 0;

  static const DFBSurfaceBlittingFlags MODULATION_FLAGS =
    (0
     | DSBLIT_BLEND_ALPHACHANNEL
     | DSBLIT_BLEND_COLORALPHA
     | DSBLIT_DST_PREMULTIPLY /* Might be possible for BLIT_ACK_SWAP_FG_BG */
     | DSBLIT_SRC_PREMULTIPLY
     | DSBLIT_SRC_PREMULTCOLOR
    );
  DFBSurfaceBlittingFlags modulation = state->blittingflags & MODULATION_FLAGS;

  funcs->Blit        = NULL;
  funcs->StretchBlit = NULL;
  funcs->Blit2       = NULL;

  stdev->ConfigClut.BLT_CCO &= ~BLIT_CCO_CLUT_NS2_S1_MASK;

  stdev->blitstate.n_passes = 1;

  stdev->blitstate.src_premultcolor = D_FLAGS_IS_SET (state->blittingflags,
                                                      DSBLIT_SRC_PREMULTCOLOR);

  if (modulation)
    {
      blt_ack &= ~BLIT_ACK_GLOBAL_ALPHA_MASK;
      blt_ack |= ((0x80 & 0xff) << BLIT_ACK_GLOBAL_ALPHA_SHIFT);

      bool needs_blend = false;
      bool src_premultiply = D_FLAGS_IS_SET (state->blittingflags,
                                             DSBLIT_SRC_PREMULTIPLY);

      if (D_FLAGS_IS_SET (state->blittingflags, (DSBLIT_BLEND_COLORALPHA
                                                 | DSBLIT_BLEND_ALPHACHANNEL)))
        {
          stdev->blitstate.isOptimisedModulation = true;

          switch (state->blittingflags & (DSBLIT_BLEND_ALPHACHANNEL
                                          | DSBLIT_BLEND_COLORALPHA))
            {
            default:
            case DSBLIT_NOFX:
            case DSBLIT_BLEND_ALPHACHANNEL:
              break;

            case DSBLIT_BLEND_COLORALPHA:
              /* If we do a premultiply (with colour or source alpha, but not
                 both!), we can simply use the global alpha if the source alpha
                 is 1.0f. It is 1.0f for all surface formats that do not contain
                 an alpha channel as the hardware fills in the alpha for us
                 and for those formats that do have an alpha channel, we can
                 use a (static, hence fast) palette to replace the source alpha
                 by 1.0f. We can also do the optimisation for those obscure
                 blend modes where the source is discarded. */
              switch (stdev->porter_duff_rule)
                {
                case DSPD_DST:
                  if (D_FLAGS_IS_SET (state->blittingflags,
                                      DSBLIT_DST_PREMULTIPLY))
                    {
                      stdev->blitstate.isOptimisedModulation = false;
                      break;
                    }
                  /* fall through */

                case DSPD_CLEAR:
                  break;

                case DSPD_SRC:
                case DSPD_SRC_OVER:
                  switch (state->blittingflags & (DSBLIT_SRC_PREMULTCOLOR
                                                  | DSBLIT_SRC_PREMULTIPLY))
                    {
                    case DSBLIT_NOFX:
                      stdev->blitstate.isOptimisedModulation = false;
                      break;

                    case (DSBLIT_SRC_PREMULTCOLOR | DSBLIT_SRC_PREMULTIPLY):
                      if (!stdev->blitstate.canUseHWInputMatrix)
                        {
                          stdev->blitstate.isOptimisedModulation = false;
                          break;
                        }
                      /* fall through */

                    case DSBLIT_SRC_PREMULTCOLOR:
                      if (!src_premultiply)
                        {
                          stdev->blitstate.src_premultcolor = false;
                          BDISP_STATE_INVALIDATE (MATRIXCONVERSIONS);
                        }
                      /* fall through */

                    case DSBLIT_SRC_PREMULTIPLY:
                      needs_blend = true;
                      src_premultiply = false;
                      break;
                    }
                  break;

                case DSPD_NONE:
                case DSPD_DST_OVER:
                case DSPD_DST_IN:
                case DSPD_DST_OUT:
                default:
                  stdev->blitstate.isOptimisedModulation = false;
                  break;
                }

              /* this will set up blt_ack or the palette as needed */
              BDISP_STATE_INVALIDATE (BLITCOLOR);
              break;

            case (DSBLIT_BLEND_ALPHACHANNEL | DSBLIT_BLEND_COLORALPHA):
              /* as an optimisation, we can use the global alpha if we do
                 SRC_PREMULTIPLY or SRC_PREMULTCOLOR or if the source blend
                 function is DSBF_SRCALPHA. */
              switch (stdev->porter_duff_rule)
                {
                case DSPD_DST_OVER:
                case DSPD_DST_IN:
                case DSPD_DST_OUT:
                  stdev->blitstate.isOptimisedModulation = false;
                  break;

                default:
                  if (!D_FLAGS_IS_SET (state->blittingflags,
                                       (DSBLIT_SRC_PREMULTCOLOR
                                        | DSBLIT_SRC_PREMULTIPLY)))
                    {
                      stdev->blitstate.isOptimisedModulation = false;
                      break;
                    }
                  /* fall through */

                case DSPD_NONE:
                  if (stdev->blitstate.src_premultcolor
                      && !stdev->blitstate.canUseHWInputMatrix)
                    {
                      if (src_premultiply
                          || stdev->porter_duff_rule == DSPD_NONE)
                        {
                          stdev->blitstate.isOptimisedModulation = false;
                          break;
                        }
                    }

                  needs_blend = true;
                  if (stdev->blitstate.src_premultcolor
                      && !src_premultiply
                      && stdev->porter_duff_rule != DSPD_NONE)
                    {
                      stdev->blitstate.src_premultcolor = false;
                      BDISP_STATE_INVALIDATE (MATRIXCONVERSIONS);
                    }
                  break;
                }

              /* this will set up blt_ack or the palette as needed */
              BDISP_STATE_INVALIDATE (BLITCOLOR);
              break;
            }

          switch (stdev->porter_duff_rule)
            {
            case DSPD_CLEAR:
              D_DEBUG_AT (BDisp_State, "  -> fast direct fill (DSPD_CLEAR)\n");
              /* This is a clear, just fill with 0 */
              funcs->Blit        = bdisp_aq_Blit_shortcut;
              funcs->StretchBlit = bdisp_aq_StretchBlit_shortcut;
              funcs->Blit2       = bdisp_aq_Blit2_shortcut;
              goto out;

            case DSPD_DST:
              if (D_FLAGS_IS_SET (state->blittingflags, DSBLIT_DST_PREMULTIPLY))
                {
                  /* FIXME: untested! */
                  blt_ack |= BLIT_ACK_SWAP_FG_BG;

                  D_DEBUG_AT (BDisp_State,
                              "  -> blend dst with src colour zero (DSPD_DST)\n");
                  blt_ins &= ~BLIT_INS_SRC2_MODE_MASK;
                  blt_ins |= BLIT_INS_SRC2_MODE_COLOR_FILL;
                  BDISP_STATE_INVALIDATE (SOURCE);

                  stdev->blitstate.blt_ins_src1_mode = BLIT_INS_SRC1_MODE_MEMORY;

                  needs_blend = true;
                  /* FIXME: don't overwrite palette type here? */
                  stdev->palette_type = SG2C_NORMAL;
                }
              else
                {
                  funcs->Blit        = bdisp_aq_Blit_nop;
                  funcs->StretchBlit = bdisp_aq_StretchBlit_nop;
                  funcs->Blit2       = bdisp_aq_Blit2_nop;
                  goto out;
                }
              break;

            case DSPD_SRC:
              if (src_premultiply
                  || needs_blend)
                {
                  /* This is a copy (blend with a destination of (0,0,0,0))
                     after premultiplication with the colour alpha channel. */
                  D_DEBUG_AT (BDisp_State,
                              "  -> blend src with dst colour zero (DSPD_SRC)\n");
                  blt_cic |= CIC_NODE_COLOR;
                  stdev->blitstate.blt_ins_src1_mode = BLIT_INS_SRC1_MODE_COLOR_FILL;
                  needs_blend = true;
                }
              break;

            case DSPD_DST_OVER:
              blt_ack |= BLIT_ACK_SWAP_FG_BG;
            case DSPD_SRC_OVER:
              D_DEBUG_AT (BDisp_State, "  -> blend with dst memory (DSPD_%s_OVER)\n",
                          (stdev->porter_duff_rule == DSPD_SRC_OVER) ? "SRC" : "DST");
              stdev->blitstate.blt_ins_src1_mode = BLIT_INS_SRC1_MODE_MEMORY;
              needs_blend = true;
              break;

            case DSPD_DST_IN:
              D_DEBUG_AT (BDisp_State, "  -> blend with dst memory (DSPD_DST_IN)\n");
              stdev->blitstate.blt_ins_src1_mode = BLIT_INS_SRC1_MODE_MEMORY;
              if (!stdev->blitstate.isOptimisedModulation)
                BDISP_STATE_INVALIDATE (BLITCOLOR);
              else
                {
                  D_DEBUG_AT (BDisp_State, "    -> SG2C_INVALPHA_ZERORGB\n");
                  if (stdev->palette_type == SG2C_ONEALPHA_RGB)
                    {
                      stdev->palette_type = SG2C_ZEROALPHA_ZERORGB;
                      stdev->blitstate.extra_passes[0].palette_type = SG2C_ONEALPHA_RGB;
                    }
                  else
                    {
                      stdev->palette_type = SG2C_INVALPHA_ZERORGB;
                      stdev->blitstate.extra_passes[0].palette_type = SG2C_NORMAL;
                    }
                }
              needs_blend = true;
              /* need the plane mask in the first pass */
              stdev->blitstate.extra_passes[0].ConfigGeneral.BLT_CIC = 0;
              stdev->blitstate.extra_passes[0].ConfigGeneral.BLT_INS = 0;
              stdev->blitstate.extra_passes[0].ConfigGeneral.BLT_ACK = BLIT_ACK_BLEND_CLIPMASK_BLEND;
              blt_cic |= CIC_NODE_FILTERS;
              blt_ins |= BLIT_INS_ENABLE_PLANEMASK;
              msk_blt_cic[0] = ~CIC_NODE_FILTERS;
              msk_blt_ins[0] = ~BLIT_INS_ENABLE_PLANEMASK;
              msk_blt_ack[0] = ~BLIT_ACK_MODE_MASK;
              stdev->blitstate.n_passes = 2;
              break;

            case DSPD_DST_OUT:
              D_DEBUG_AT (BDisp_State, "  -> blend with dst memory (DSPD_DST_OUT)\n");
              stdev->blitstate.blt_ins_src1_mode = BLIT_INS_SRC1_MODE_MEMORY;
              if (!stdev->blitstate.isOptimisedModulation)
                BDISP_STATE_INVALIDATE (BLITCOLOR);
              else
                {
                  D_DEBUG_AT (BDisp_State, "    -> normal palettes\n");
                  if (stdev->palette_type == SG2C_ONEALPHA_RGB)
                    {
                      stdev->palette_type = SG2C_ONEALPHA_ZERORGB;
                      stdev->blitstate.extra_passes[0].palette_type = SG2C_ZEROALPHA_ZERORGB;
                    }
                  else
                    {
                      stdev->palette_type = SG2C_ALPHA_ZERORGB;
                      stdev->blitstate.extra_passes[0].palette_type = SG2C_INVALPHA_ZERORGB;
                    }
                }
              needs_blend = true;
              /* need the plane mask in the first pass */
              stdev->blitstate.extra_passes[0].ConfigGeneral.BLT_CIC = 0;
              stdev->blitstate.extra_passes[0].ConfigGeneral.BLT_INS = 0;
              stdev->blitstate.extra_passes[0].ConfigGeneral.BLT_ACK = BLIT_ACK_BLEND_CLIPMASK_BLEND;
              blt_cic |= CIC_NODE_FILTERS;
              blt_ins |= BLIT_INS_ENABLE_PLANEMASK;
              msk_blt_cic[0] = ~CIC_NODE_FILTERS;
              msk_blt_ins[0] = ~BLIT_INS_ENABLE_PLANEMASK;
              msk_blt_ack[0] = ~BLIT_ACK_MODE_MASK;
              stdev->blitstate.n_passes = 2;
              break;

            case DSPD_NONE:
              /* Must be premulitply off, dest no alpha channel. */
              D_DEBUG_AT (BDisp_State, "  -> blend with dst memory (DSPD_NONE)\n");
              stdev->blitstate.blt_ins_src1_mode = BLIT_INS_SRC1_MODE_MEMORY;
              needs_blend = true;
              break;

            default:
              D_WARN ("unsupported Porter/Duff rule: %d flags: %.8x",
                      stdev->porter_duff_rule, state->blittingflags);
              needs_blend = true;
              break;
            }
        }
      else if (D_FLAGS_IS_SET (state->blittingflags,
                               (DSBLIT_SRC_PREMULTIPLY
                                | DSBLIT_DST_PREMULTIPLY)))
        {
          /* This is a copy (blend with a destination of (0,0,0,0))
             after premultiplication with the colour alpha channel. */
          if (D_FLAGS_IS_SET (state->blittingflags, DSBLIT_DST_PREMULTIPLY))
            /* FIXME: see DSPD_DST case. */
            blt_ack |= BLIT_ACK_SWAP_FG_BG;

          D_DEBUG_AT (BDisp_State, "  -> %s premultiply\n",
                      (state->blittingflags & DSBLIT_SRC_PREMULTIPLY) ? "src" : "dst");
          blt_cic |= CIC_NODE_COLOR;
          stdev->blitstate.blt_ins_src1_mode = BLIT_INS_SRC1_MODE_COLOR_FILL;
          needs_blend = true;
        }

      if (needs_blend)
        {
          /* CheckState guarantees that only one of SRC_PREMULTIPLY
             DST_PREMULTIPLY is set, depending on BLIT_ACK_SWAP_FG_BG */
          blt_ack |= ((src_premultiply
                       || (state->blittingflags & DSBLIT_DST_PREMULTIPLY)
                       || (stdev->porter_duff_rule == DSPD_NONE))
                       ? BLIT_ACK_BLEND_SRC2_N_PREMULT
                       : BLIT_ACK_BLEND_SRC2_PREMULT);
        }
    }

  if (D_FLAGS_IS_SET (state->blittingflags, DSBLIT_XOR))
    {
      D_DEBUG_AT (BDisp_State, "  -> XOR\n");
      blt_ack &= ~BLIT_ACK_ROP_MASK;
      blt_ack |= (0
                  | BLIT_ACK_ROP
                  | BLIT_ACK_ROP_XOR
                 );
      stdev->blitstate.blt_ins_src1_mode = BLIT_INS_SRC1_MODE_MEMORY;
    }

  if (D_FLAGS_IS_SET (state->blittingflags, DSBLIT_SOURCE2))
    {
      D_DEBUG_AT (BDisp_State, "  -> SOURCE2\n");

      D_ASSERT (stdev->blitstate.blt_ins_src1_mode !=
                BLIT_INS_SRC1_MODE_COLOR_FILL);
      stdev->blitstate.blt_ins_src1_mode = BLIT_INS_SRC1_MODE_MEMORY;
    }

  if (!(blt_ack & BLIT_ACK_MODE_MASK))
    blt_ack |= BLIT_ACK_BYPASSSOURCE2;
  stdev->ConfigGeneral.BLT_CIC = blt_cic;
  stdev->ConfigGeneral.BLT_INS = blt_ins;
  stdev->ConfigGeneral.BLT_ACK = blt_ack;

  int i;
  for (i = 0; i < (stdev->blitstate.n_passes - 1); ++i)
    {
      blt_cic &= msk_blt_cic[i];
      blt_ins &= msk_blt_ins[i];
      blt_ack &= msk_blt_ack[i];

      blt_cic &= ~stdev->blitstate.extra_passes[i].ConfigGeneral.BLT_CIC;
      blt_ins &= ~stdev->blitstate.extra_passes[i].ConfigGeneral.BLT_INS;
      blt_ack &= ~stdev->blitstate.extra_passes[i].ConfigGeneral.BLT_ACK;

      stdev->blitstate.extra_passes[i].ConfigGeneral.BLT_CIC |= blt_cic;
      stdev->blitstate.extra_passes[i].ConfigGeneral.BLT_INS |= blt_ins;
      stdev->blitstate.extra_passes[i].ConfigGeneral.BLT_INS |= stdev->blitstate.blt_ins_src1_mode;
      stdev->blitstate.extra_passes[i].ConfigGeneral.BLT_ACK |= blt_ack;
    }

out:
  /* done */
  BDISP_STATE_VALIDATED (BLITTINGFLAGS);
}

#define set_matrix(dst, src) \
  ({ \
    dst ## 0 = src[0]; \
    dst ## 1 = src[1]; \
    dst ## 2 = src[2]; \
    dst ## 3 = src[3]; \
  })


static void
bdisp_state_validate_INPUTMATRIX (STGFX2DriverData * const stdrv,
                                  STGFX2DeviceData * const stdev,
                                  CardState        * const state)
{
  bool t_is_YCbCr = !!BLIT_TY_COLOR_FORM_IS_YUV (stdev->ConfigTarget.BLT_TTY);
  bool s2_is_YCbCr = !!BLIT_TY_COLOR_FORM_IS_YUV (stdev->ConfigSource2.BLT_S2TY);

  D_DEBUG_AT (BDisp_State, "%s\n", __FUNCTION__);

  stdev->blitstate.canUseHWInputMatrix = (t_is_YCbCr == s2_is_YCbCr);

  /* done */
  BDISP_STATE_VALIDATED (INPUTMATRIX);
}

static void
bdisp_state_validate_MATRIXCONVERSIONS (STGFX2DriverData * const stdrv,
                                        STGFX2DeviceData * const stdev,
                                        CardState        * const state)
{
  u32 blt_cic = stdev->ConfigGeneral.BLT_CIC;
  u32 blt_ins = stdev->ConfigGeneral.BLT_INS;

  D_DEBUG_AT (BDisp_State, "%s\n", __FUNCTION__);

  blt_cic &= ~(CIC_NODE_IVMX | CIC_NODE_OVMX);
  blt_ins &= ~(BLIT_INS_ENABLE_IVMX | BLIT_INS_ENABLE_OVMX);

  if (BLIT_TY_COLOR_FORM_IS_ALPHA (stdev->ConfigSource2.BLT_S2TY)
      || D_FLAGS_IS_SET (state->blittingflags, DSBLIT_COLORIZE)
      || stdev->blitstate.src_premultcolor)
    {
      if (!BLIT_TY_COLOR_FORM_IS_YUV (stdev->ConfigSource2.BLT_S2TY))
        if (BLIT_TY_COLOR_FORM_IS_YUV (stdev->ConfigTarget.BLT_TTY))
          {
            D_DEBUG_AT (BDisp_State, "  -> Output RGB->YCbCr601\n");

            blt_cic |= CIC_NODE_OVMX;
            blt_ins |= BLIT_INS_ENABLE_OVMX;
          }

      u32 red, green, blue;
      u32 extra = 0;

      blt_cic |= CIC_NODE_IVMX;
      blt_ins |= BLIT_INS_ENABLE_IVMX;

      if (D_FLAGS_IS_SET (state->blittingflags, DSBLIT_COLORIZE))
        {
          D_DEBUG_AT (BDisp_State, "  -> COLORIZE: %.2x%.2x%.2x\n",
                      state->color.r, state->color.g, state->color.b);

          red   = (state->color.r * 256) / 255;
          green = (state->color.g * 256) / 255;
          blue  = (state->color.b * 256) / 255;

          if (BLIT_TY_COLOR_FORM_IS_YUV (stdev->ConfigSource2.BLT_S2TY))
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

      if (stdev->blitstate.src_premultcolor)
        {
          /* scale 0..255 to 0..256 */
          u32 alpha = (state->color.a * 256) / 255;

          D_DEBUG_AT (BDisp_State, "  -> SRC_PREMULTCOLOR α: %.2x\n", alpha);

          red   = (red   * alpha) / 256;
          green = (green * alpha) / 256;
          blue  = (blue  * alpha) / 256;
        }

      if (BLIT_TY_COLOR_FORM_IS_ALPHA (stdev->ConfigSource2.BLT_S2TY))
        {
          /* A8 and A1 surface types, we need to add the calculated colour as
             the hardware always expands to black with alpha (what use is
             that?!) */
          D_DEBUG_AT (BDisp_State,
                      "  -> Input α x RGB replacement (%.2x%.2x%.2x)\n",
                      red, green, blue);

          stdev->ConfigIVMX.BLT_IVMX0 = 0;
          stdev->ConfigIVMX.BLT_IVMX1 = 0;
          stdev->ConfigIVMX.BLT_IVMX2 = 0;
          stdev->ConfigIVMX.BLT_IVMX3 = (red << 20) | (green << 10) | blue;
        }
      else /* if (!BLIT_TY_COLOR_FORM_IS_MISC (stdev->ConfigSource2.BLT_S2TY)) */
        {
          /* RGB surface types, do a real modulation */
          D_DEBUG_AT (BDisp_State,
                      "  -> Input RGB modulation (%.2x%.2x%.2x)\n",
                      red, green, blue);

          stdev->ConfigIVMX.BLT_IVMX0 = red   << 21;
          stdev->ConfigIVMX.BLT_IVMX1 = green << 10;
          stdev->ConfigIVMX.BLT_IVMX2 = blue;
          stdev->ConfigIVMX.BLT_IVMX3 = extra;
        }
      /* else RLE - not reached (hopefully :-) */
    }
  else
    {
      if (BLIT_TY_COLOR_FORM_IS_YUV (stdev->ConfigTarget.BLT_TTY)
          && !BLIT_TY_COLOR_FORM_IS_YUV (stdev->ConfigSource2.BLT_S2TY))
        {
          D_DEBUG_AT (BDisp_State, "  -> Input RGB->YCbCr601\n");

          blt_cic |= CIC_NODE_IVMX;
          blt_ins |= BLIT_INS_ENABLE_IVMX;

          set_matrix (stdev->ConfigIVMX.BLT_IVMX, bdisp_aq_RGB_2_VideoYCbCr601);
        }
      else if (!BLIT_TY_COLOR_FORM_IS_YUV (stdev->ConfigTarget.BLT_TTY)
               && BLIT_TY_COLOR_FORM_IS_YUV (stdev->ConfigSource2.BLT_S2TY))
        {
          D_DEBUG_AT (BDisp_State, "  -> Input YCbCr601->RGB\n");

          blt_cic |= CIC_NODE_IVMX;
          blt_ins |= BLIT_INS_ENABLE_IVMX;

          set_matrix (stdev->ConfigIVMX.BLT_IVMX, bdisp_aq_VideoYCbCr601_2_RGB);
        }
    }

  stdev->ConfigGeneral.BLT_CIC = blt_cic;
  stdev->ConfigGeneral.BLT_INS = blt_ins;

  BDISP_STATE_VALIDATED (MATRIXCONVERSIONS);
}


void
bdisp_aq_SetState (void                * const drv,
                   void                * const dev,
                   GraphicsDeviceFuncs * const funcs,
                   CardState           * const state,
                   DFBAccelerationMask   accel)
{
  STGFX2DriverData       * const stdrv = drv;
  STGFX2DeviceData       * const stdev = dev;
  StateModificationFlags  modified = state->mod_hw;

  bool support_hw_clip = false;

  _dump_set_state (__FUNCTION__, state, accel);

  D_DEBUG_AT (BDisp_State, "%s: %.8x\n", __FUNCTION__, state->mod_hw);

  /* Simply invalidate all? */
  if (modified == SMF_ALL)
    BDISP_STATE_INVALIDATE (ALL);
  else if (likely (modified))
    {
      /* Invalidate destination registers. */
      if (modified & SMF_DESTINATION)
        {
          /* make sure that the color gets updated when the destination
             surface format changes, as we have to modify the color for
             fills. */
          BDISP_STATE_INVALIDATE (DESTINATION | SOURCE_SRC1_MODE
                                  | FILLCOLOR | DST_COLORKEY
                                  | PALETTE_DRAW | PALETTE_BLIT
                                  | DRAWINGFLAGS
                                  | BLITTINGFLAGS);
        }
      else
        {
          if (modified & (SMF_DRAWING_FLAGS
                          | SMF_SRC_BLEND | SMF_DST_BLEND))
            /* make sure that the color gets updated when the drawing
               flags change, as we have to modify the color for fills. */
            BDISP_STATE_INVALIDATE (FILLCOLOR | DRAWINGFLAGS | PALETTE_DRAW);

          if (modified & SMF_DST_COLORKEY)
            BDISP_STATE_INVALIDATE (DST_COLORKEY);

          if (modified & (SMF_BLITTING_FLAGS
                          | SMF_SRC_BLEND | SMF_DST_BLEND))
            BDISP_STATE_INVALIDATE (PALETTE_BLIT);
        }

      if (modified & SMF_COLOR)
        BDISP_STATE_INVALIDATE (FILLCOLOR | BLITCOLOR | MATRIXCONVERSIONS);
      else if (modified & SMF_BLITTING_FLAGS)
        BDISP_STATE_INVALIDATE (BLITCOLOR | MATRIXCONVERSIONS);

      /* Invalidate clipping registers. */
      if (modified & SMF_CLIP)
        BDISP_STATE_INVALIDATE (CLIP);

      if (modified & SMF_SOURCE)
        BDISP_STATE_INVALIDATE (SOURCE | SOURCE_SRC1_MODE
                                | SRC_COLORKEY
                                | PALETTE_BLIT
                                | BLITTINGFLAGS | BLITTINGFLAGS_CKEY
                                | RENDEROPTS);
      else
        {
          if (modified & SMF_SRC_COLORKEY)
            BDISP_STATE_INVALIDATE (SRC_COLORKEY);

          if (modified & SMF_RENDER_OPTIONS)
            BDISP_STATE_INVALIDATE (RENDEROPTS);

          if (modified & SMF_BLITTING_FLAGS)
            BDISP_STATE_INVALIDATE (SOURCE_SRC1_MODE
                                    | BLITTINGFLAGS | BLITTINGFLAGS_CKEY);
          else if (modified & (SMF_SRC_BLEND | SMF_DST_BLEND))
            BDISP_STATE_INVALIDATE (BLITTINGFLAGS | BLITTINGFLAGS_CKEY);

          if (modified & SMF_SOURCE2)
            BDISP_STATE_INVALIDATE (SOURCE_SRC1_MODE);
        }

      if (modified & (SMF_BLITTING_FLAGS | SMF_MATRIX | SMF_RENDER_OPTIONS))
        BDISP_STATE_INVALIDATE (ROTATION);
    }

  if (modified & (SMF_SRC_BLEND | SMF_DST_BLEND))
    stdev->porter_duff_rule = bdisp_aq_convert_to_supported_porterduff (state);

  BDISP_STATE_CHECK_N_VALIDATE (DESTINATION);

  BDISP_STATE_CHECK_N_VALIDATE (FILLCOLOR);

  BDISP_STATE_CHECK_N_VALIDATE (DST_COLORKEY);

  switch (accel)
    {
    case DFXL_FILLRECTANGLE:
    case DFXL_DRAWRECTANGLE:
      BDISP_STATE_CHECK_N_VALIDATE (PALETTE_DRAW);
      BDISP_STATE_CHECK_N_VALIDATE (DRAWINGFLAGS);

      if (stdev->drawstate.ConfigGeneral.BLT_INS == BLIT_INS_SRC1_MODE_DIRECT_FILL)
        {
          /* this is a simple solid fill that can use the 64bit fast path
             in the blitter, which supports the output of multiple pixels
             per blitter clock cycle. */
          D_DEBUG_AT (BDisp_State, "%s(): fast direct fill/draw\n", __FUNCTION__);

          funcs->FillRectangle = bdisp_aq_FillRectangle_simple;
          funcs->DrawRectangle = bdisp_aq_DrawRectangle_simple;
        }
      else if (unlikely (((stdev->drawstate.ConfigGeneral.BLT_INS & BLIT_INS_SRC2_MODE_MASK)
                          == BLIT_INS_SRC2_MODE_DISABLED)
                         && ((stdev->drawstate.ConfigGeneral.BLT_INS & BLIT_INS_SRC1_MODE_MASK)
                             == BLIT_INS_SRC1_MODE_DISABLED)))
        {
          /* We don't need to do the operation as it would be a nop... */
          funcs->FillRectangle = bdisp_aq_FillDraw_nop;
          funcs->DrawRectangle = bdisp_aq_FillDraw_nop;

          /* ...hence we might as well announce support for clipping. */
          support_hw_clip = true;
        }
      else
        {
          /* this is a fill which needs to use the source 2 data path for
             the fill color. This path is limited to one pixel output per
             blitter clock cycle... */
          D_DEBUG_AT (BDisp_State, "%s(): complex fill/draw\n", __FUNCTION__);

          D_ASSUME ((stdev->drawstate.ConfigGeneral.BLT_INS & BLIT_INS_SRC1_MODE_MASK) != BLIT_INS_SRC1_MODE_DIRECT_FILL);

          funcs->FillRectangle = bdisp_aq_FillRectangle;
          funcs->DrawRectangle = bdisp_aq_DrawRectangle;

          /* ...but it supports clipping. */
          support_hw_clip = true;
        }

      state->set |= (DFXL_FILLRECTANGLE | DFXL_DRAWRECTANGLE);
      break;

    case DFXL_BLIT:
    case DFXL_BLIT2:
    case DFXL_STRETCHBLIT:
      /* SRC1 mode depends on the acceleration function in addition to the
         blitting flags, therefore we need to invalidate this every time! */
      BDISP_STATE_INVALIDATE (SOURCE_SRC1_MODE);
      BDISP_STATE_CHECK_N_VALIDATE (SOURCE);
      BDISP_STATE_CHECK_N_VALIDATE (INPUTMATRIX);
      BDISP_STATE_CHECK_N_VALIDATE (SRC_COLORKEY);
      BDISP_STATE_CHECK_N_VALIDATE (RENDEROPTS);
      BDISP_STATE_CHECK_N_VALIDATE (PALETTE_BLIT);
      BDISP_STATE_CHECK_N_VALIDATE2 (BLITTINGFLAGS);
      BDISP_STATE_CHECK_N_VALIDATE (BLITTINGFLAGS_CKEY);
      BDISP_STATE_CHECK_N_VALIDATE (BLITCOLOR);
      BDISP_STATE_CHECK_N_VALIDATE3 (SOURCE_SRC1_MODE);
      BDISP_STATE_CHECK_N_VALIDATE (MATRIXCONVERSIONS);
      BDISP_STATE_CHECK_N_VALIDATE2 (ROTATION);

      if (unlikely (funcs->Blit == bdisp_aq_Blit_nop))
        {
          /* DSPD_DST blend was requested. */
          D_ASSERT (funcs->StretchBlit == bdisp_aq_StretchBlit_nop);
          D_ASSERT (funcs->Blit2 == bdisp_aq_Blit2_nop);
          state->set |= (DFXL_BLIT | DFXL_STRETCHBLIT | DFXL_BLIT2);
        }
      else if (unlikely (funcs->Blit == bdisp_aq_Blit_shortcut))
        {
          /* DSPD_CLEAR 'blend' was requested. */
          D_ASSERT (funcs->StretchBlit == bdisp_aq_StretchBlit_shortcut);
          D_ASSERT (funcs->Blit2 == bdisp_aq_Blit2_shortcut);
          switch (state->destination->config.format)
            {
            case DSPF_YUY2:
            case DSPF_UYVY:
              funcs->Blit = bdisp_aq_Blit_shortcut_YCbCr422r;
              funcs->StretchBlit = bdisp_aq_StretchBlit_shortcut_YCbCr422r;
              funcs->Blit2 = bdisp_aq_Blit2_shortcut_YCbCr422r;
              break;

            case DSPF_RGB32:
              funcs->Blit = bdisp_aq_Blit_shortcut_rgb32;
              funcs->StretchBlit = bdisp_aq_StretchBlit_shortcut_rgb32;
              funcs->Blit2 = bdisp_aq_Blit2_shortcut_rgb32;
              break;

            default:
              /* nothing */
              break;
            }
          state->set |= (DFXL_BLIT | DFXL_STRETCHBLIT | DFXL_BLIT2);
        }
      else if (unlikely (stdev->rotate == 90
                         || stdev->rotate == 270))
        {
          D_DEBUG_AT (BDisp_State, "%s(): rotated blit\n", __FUNCTION__);
          state->set |= DFXL_BLIT;
          state->set &= ~(DFXL_STRETCHBLIT | DFXL_BLIT2);
          funcs->Blit = bdisp_aq_Blit_rotate_90_270;
          bdisp_aq_setup_blit_operation (stdrv, stdev);
        }
      else
        {
          state->set |= (DFXL_BLIT | DFXL_STRETCHBLIT);
          /* if this is a stretchblit, or a blit of a subbyte format, or
             a blit with format conversion, or a blit with any blitting
             flags set, we have to use the slow path... */
          if (state->source->config.format == state->destination->config.format
              && D_FLAGS_ARE_IN (state->blittingflags,
                                 DSBLIT_INDEX_TRANSLATION)
              /* this caters for 180 degrees and mirroring */
              && !stdev->rotate)
            {
              D_DEBUG_AT (BDisp_State, "%s(): fast blit\n", __FUNCTION__);

              switch (state->destination->config.format)
                {
                case DSPF_YUY2:
                case DSPF_UYVY:
                  /* for some reason a fast UYVY -> UYVY blit has problems, we
                     can use the fast path, but have to change the pixelformat!
                     RGB16 just works fine, and it doesn't matter as the BDisp
                     does not care about the actual data. */
                  funcs->Blit = bdisp_aq_Blit_simple_YCbCr422r;
                  break;

                default:
                  funcs->Blit = bdisp_aq_Blit_simple;
                  break;
                }
            }
          else if (DFB_PLANAR_PIXELFORMAT (state->source->config.format))
            {
              D_DEBUG_AT (BDisp_State, "%s(): planar blit\n", __FUNCTION__);
              funcs->Blit = bdisp_aq_Blit_as_stretch;
              support_hw_clip = true;
            }
          else
            {
              D_DEBUG_AT (BDisp_State, "%s(): complex blit\n", __FUNCTION__);
              funcs->Blit = bdisp_aq_Blit;
              support_hw_clip = true;
            }

          if (D_FLAGS_IS_SET (accel, DFXL_BLIT2))
            {
              funcs->Blit2 = bdisp_aq_Blit2;
              state->set |= DFXL_BLIT2;
            }
          else
            {
              funcs->Blit2 = bdisp_aq_Blit2_nop;
              state->set &= ~DFXL_BLIT2;
            }

          funcs->StretchBlit = bdisp_aq_StretchBlit;
          bdisp_aq_setup_blit_operation (stdrv, stdev);
        }
      break;

    default:
      /* Should never happen as we have told DirectFB which functions we can
         support */
      D_ERROR ("DirectFB/gfxdrivers/stgfx2: unexpected acceleration function");
      state->set &= ~(DFXL_BLIT | DFXL_STRETCHBLIT | DFXL_BLIT2);
      break;
  }


#ifdef STGFX2_SUPPORT_HW_CLIPPING
  if (support_hw_clip)
    {
      BDISP_STATE_CHECK_N_VALIDATE (CLIP);
      *flags |= CCF_CLIPPING;

      stdev->ConfigGeneral.BLT_CIC |= CIC_NODE_CLIP;
      stdev->ConfigGeneral.BLT_INS |= BLIT_INS_ENABLE_RECTCLIP;

      /* multi pass operations */
      stdev->blitstate.extra_passes[0].ConfigGeneral.BLT_CIC |= CIC_NODE_CLIP;
      stdev->blitstate.extra_passes[0].ConfigGeneral.BLT_INS |= BLIT_INS_ENABLE_RECTCLIP;
      stdev->blitstate.extra_passes[1].ConfigGeneral.BLT_CIC |= CIC_NODE_CLIP;
      stdev->blitstate.extra_passes[1].ConfigGeneral.BLT_INS |= BLIT_INS_ENABLE_RECTCLIP;
    }
  else
    {
      /* fast path, hence no HW clipping */
      *flags &= ~CCF_CLIPPING;

      stdev->ConfigGeneral.BLT_CIC &= ~CIC_NODE_CLIP;
      stdev->ConfigGeneral.BLT_INS &= ~BLIT_INS_ENABLE_RECTCLIP;

      /* multi pass operations */
      stdev->blitstate.extra_passes[0].ConfigGeneral.BLT_CIC &= ~CIC_NODE_CLIP;
      stdev->blitstate.extra_passes[0].ConfigGeneral.BLT_INS &= ~BLIT_INS_ENABLE_RECTCLIP;
      stdev->blitstate.extra_passes[1].ConfigGeneral.BLT_CIC &= ~CIC_NODE_CLIP;
      stdev->blitstate.extra_passes[1].ConfigGeneral.BLT_INS &= ~BLIT_INS_ENABLE_RECTCLIP;
    }
#endif


  state->mod_hw = 0;
}
