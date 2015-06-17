#include "config.h"

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h> /* dma area (size_t) */

#include <sys/ioctl.h>

#include <linux/stm/blitter.h>

#include "surface.h"
#include "state.h"
#include "bdisp2/bdispII_aq_state.h"

#include "bdisp2/bdisp2_os.h"

#include <direct/types.h>
#include <core/core.h>
#include <core/coretypes.h>
#include <core/state.h>
#include <core/surface.h>
#include <core/palette.h>
#include <fusion/shmalloc.h>

#include <gfx/convert.h>

#include "stm_types.h"
#include "stm_gfxdriver.h"

#include "bdisp2/bdisp_accel.h"
#include "bdisp2/bdisp2_os.h"

STM_BLITTER_DEBUG_DOMAIN (BDisp_State,      "BDisp/State",      "BDispII state handling");
STM_BLITTER_DEBUG_DOMAIN (BDisp_StateCheck, "BDisp/StateCheck", "BDispII state testing");
STM_BLITTER_DEBUG_DOMAIN (BDisp_StateSet,   "BDisp/StateSet",   "BDispII state setting");

char *
stgfx2_get_dfb_drawflags_string (DFBSurfaceDrawingFlags flags)
{
  char *str = malloc (1024);
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

char *
stgfx2_get_dfb_blitflags_string (DFBSurfaceBlittingFlags flags)
{
  char *str = malloc (1024);
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
#if HAVE_DECL_DSBLIT_SOURCE2
      if (flags & DSBLIT_SOURCE2)
        pos += sprintf (str + pos, " SOURCE2");
#endif /* HAVE_DECL_DSBLIT_SOURCE2 */
      if (flags & DSBLIT_FLIP_HORIZONTAL)
        pos += sprintf (str + pos, " FLIP_HORIZONTAL");
      if (flags & DSBLIT_FLIP_VERTICAL)
        pos += sprintf (str + pos, " FLIP_VERTICAL");
#if HAVE_DECL_DSBLIT_FIXEDPOINT
      if (flags & DSBLIT_FIXEDPOINT)
        pos += sprintf (str + pos, " FIXEDPOINT");
#endif
    }

  return str;
}

char *
stgfx2_get_dfb_renderoptions_string (DFBSurfaceRenderOptions opts)
{
  char *str = malloc (1024);
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
      if (opts & DSRO_WRITE_MASK_BITS)
        pos += sprintf (str + pos, " WRITE_MASK_BITS");
    }

  return str;
}

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
//      if (accel & DFXL_FILLTRAPEZOID)
// fixme add only after
//        pos += sprintf (str + pos, " FILLTRAPEZOID");
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
_get_matrix_string (const int32_t *matrix)
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
  char * const drawflags_str = stgfx2_get_dfb_drawflags_string (state->drawingflags);
  char * const blitflags_str = stgfx2_get_dfb_blitflags_string (state->blittingflags);
  char * const renderopts_str = stgfx2_get_dfb_renderoptions_string (state->render_options);
  char * const matrix_str = _get_matrix_string (state->matrix);

  printf ("%s: accel: %s\n"
          "    state: %p modified: %s, flags draw/blit: %s/%s\n"
          "    blendfunc src/dst: %u(%s)/%u(%s)\n"
          "    format src/dst/src2: '%s'/'%s'/'%s'\n"
          "    color/index: %.2x%.2x%.2x%.2x/%.8x colorkey src/dst: %.8x/%.8x   clip: %d,%d-%d,%d\n"
          "    render_opts: %s\n"
          "    matrix: %s\n",
          prefix,
          accel_str, state, modified_str, drawflags_str, blitflags_str,
          state->src_blend, (state->src_blend <= DSBF_SRCALPHASAT) ? _state_strs[state->src_blend] : _state_strs[0],
          state->dst_blend, (state->dst_blend <= DSBF_SRCALPHASAT) ? _state_strs[state->dst_blend] : _state_strs[0],
          state->source ? dfb_pixelformat_name (state->source->config.format) : "no source",
          state->destination ? dfb_pixelformat_name (state->destination->config.format) : "no dest",
          state->source2 ? dfb_pixelformat_name (state->source2->config.format) : "no source2",
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


const enum stm_blitter_surface_format_e
dspf_to_stm_blit[DFB_NUM_PIXELFORMATS] = {
  [DFB_PIXELFORMAT_INDEX (DSPF_ARGB1555)] = STM_BLITTER_SF_ARGB1555,
  [DFB_PIXELFORMAT_INDEX (DSPF_RGB16)]    = STM_BLITTER_SF_RGB565,
  [DFB_PIXELFORMAT_INDEX (DSPF_RGB24)]    = STM_BLITTER_SF_RGB24,
  [DFB_PIXELFORMAT_INDEX (DSPF_RGB32)]    = STM_BLITTER_SF_RGB32,
  [DFB_PIXELFORMAT_INDEX (DSPF_ARGB)]     = STM_BLITTER_SF_ARGB,
  [DFB_PIXELFORMAT_INDEX (DSPF_A8)]       = STM_BLITTER_SF_A8,
  [DFB_PIXELFORMAT_INDEX (DSPF_YUY2)]     = STM_BLITTER_SF_YUY2,
  [DFB_PIXELFORMAT_INDEX (DSPF_RGB332)]   = STM_BLITTER_SF_INVALID,
  [DFB_PIXELFORMAT_INDEX (DSPF_UYVY)]     = STM_BLITTER_SF_UYVY,
  [DFB_PIXELFORMAT_INDEX (DSPF_I420)]     = STM_BLITTER_SF_I420,
  [DFB_PIXELFORMAT_INDEX (DSPF_YV12)]     = STM_BLITTER_SF_YV12,
  [DFB_PIXELFORMAT_INDEX (DSPF_LUT8)]     = STM_BLITTER_SF_LUT8,
  [DFB_PIXELFORMAT_INDEX (DSPF_ALUT44)]   = STM_BLITTER_SF_ALUT44,
  [DFB_PIXELFORMAT_INDEX (DSPF_AiRGB)]    = STM_BLITTER_SF_INVALID,
  [DFB_PIXELFORMAT_INDEX (DSPF_A1)]       = STM_BLITTER_SF_A1,
  [DFB_PIXELFORMAT_INDEX (DSPF_NV12)]     = STM_BLITTER_SF_NV12,
  [DFB_PIXELFORMAT_INDEX (DSPF_NV16)]     = STM_BLITTER_SF_NV16,
  [DFB_PIXELFORMAT_INDEX (DSPF_ARGB2554)] = STM_BLITTER_SF_INVALID,
  [DFB_PIXELFORMAT_INDEX (DSPF_ARGB4444)] = STM_BLITTER_SF_ARGB4444,
  [DFB_PIXELFORMAT_INDEX (DSPF_RGBA4444)] = STM_BLITTER_SF_INVALID,
  [DFB_PIXELFORMAT_INDEX (DSPF_NV21)]     = STM_BLITTER_SF_NV21,
  [DFB_PIXELFORMAT_INDEX (DSPF_AYUV)]     = STM_BLITTER_SF_INVALID,
  [DFB_PIXELFORMAT_INDEX (DSPF_A4)]       = STM_BLITTER_SF_INVALID,
  [DFB_PIXELFORMAT_INDEX (DSPF_ARGB1666)] = STM_BLITTER_SF_INVALID,
  [DFB_PIXELFORMAT_INDEX (DSPF_ARGB6666)] = STM_BLITTER_SF_INVALID,
  [DFB_PIXELFORMAT_INDEX (DSPF_RGB18)]    = STM_BLITTER_SF_INVALID,
  [DFB_PIXELFORMAT_INDEX (DSPF_LUT2)]     = STM_BLITTER_SF_LUT2,
  [DFB_PIXELFORMAT_INDEX (DSPF_RGB444)]   = STM_BLITTER_SF_INVALID,
  [DFB_PIXELFORMAT_INDEX (DSPF_RGB555)]   = STM_BLITTER_SF_INVALID,
  [DFB_PIXELFORMAT_INDEX (DSPF_BGR555)]   = STM_BLITTER_SF_INVALID,
  [DFB_PIXELFORMAT_INDEX (DSPF_RGBA5551)] = STM_BLITTER_SF_INVALID,
  [DFB_PIXELFORMAT_INDEX (DSPF_YUV444P)]  = STM_BLITTER_SF_YCBCR444P,
  [DFB_PIXELFORMAT_INDEX (DSPF_ARGB8565)] = STM_BLITTER_SF_ARGB8565,
  [DFB_PIXELFORMAT_INDEX (DSPF_AVYU)]     = STM_BLITTER_SF_AVYU,
  [DFB_PIXELFORMAT_INDEX (DSPF_VYU)]      = STM_BLITTER_SF_VYU,
  [DFB_PIXELFORMAT_INDEX (DSPF_A1_LSB)]   = STM_BLITTER_SF_INVALID,
  [DFB_PIXELFORMAT_INDEX (DSPF_YV16)]     = STM_BLITTER_SF_YV16,
#if HAVE_DECL_DSPF_ABGR
  [DFB_PIXELFORMAT_INDEX (DSPF_ABGR)]       = STM_BLITTER_SF_ABGR,
#endif
#if HAVE_DECL_DSPF_RGBAF88871
  [DFB_PIXELFORMAT_INDEX (DSPF_RGBAF88871)] = STM_BLITTER_SF_INVALID,
#endif
#if HAVE_DECL_DSPF_LUT4
  [DFB_PIXELFORMAT_INDEX (DSPF_LUT4)] = STM_BLITTER_SF_LUT4,
#endif
#if HAVE_DECL_DSPF_ALUT8
  [DFB_PIXELFORMAT_INDEX (DSPF_ALUT8)] = STM_BLITTER_SF_ALUT88,
#endif
#if HAVE_DECL_DSPF_LUT1
  [DFB_PIXELFORMAT_INDEX (DSPF_LUT1)] = STM_BLITTER_SF_LUT1,
#endif /* HAVE_DECL_DSPF_LUT1 */
#if HAVE_DECL_DSPF_BGR24
  [DFB_PIXELFORMAT_INDEX (DSPF_BGR24)] = STM_BLITTER_SF_BGR24,
#endif
#if HAVE_DECL_DSPF_NV12MB
  [DFB_PIXELFORMAT_INDEX (DSPF_NV12MB)] = STM_BLITTER_SF_YCBCR420MB,
#endif
#if HAVE_DECL_DSPF_NV16MB
  [DFB_PIXELFORMAT_INDEX (DSPF_NV16MB)] = STM_BLITTER_SF_YCBCR422MB,
#endif
#if HAVE_DECL_DSPF_BYTE
  [DFB_PIXELFORMAT_INDEX (DSPF_BYTE)] = STM_BLITTER_SF_INVALID,
#endif
#if HAVE_DECL_DSPF_NV24
  [DFB_PIXELFORMAT_INDEX (DSPF_NV24)] = STM_BLITTER_SF_NV24,
#endif
};

static enum stm_blitter_porter_duff_rule_e
__attribute__((warn_unused_result, const))
dsbf_to_stm_blit (DFBSurfaceBlendFunction src_blend,
                  DFBSurfaceBlendFunction dst_blend)
{
  /* the blending maths we can support */
  switch (src_blend)
    {
    case DSBF_ZERO:
      if (dst_blend == DSBF_ZERO)
        return STM_BLITTER_PD_CLEAR;
      if (dst_blend == DSBF_SRCALPHA)
        return STM_BLITTER_PD_DEST_IN;
      if (dst_blend == DSBF_INVSRCALPHA)
        return STM_BLITTER_PD_DEST_OUT;
      if (dst_blend == DSBF_ONE)
        return STM_BLITTER_PD_DEST;
      break;

    case DSBF_ONE:
      if (dst_blend == DSBF_ZERO)
        return STM_BLITTER_PD_SOURCE;
      if (dst_blend == DSBF_INVSRCALPHA)
        return STM_BLITTER_PD_SOURCE_OVER;
      break;

    case DSBF_INVDESTALPHA:
      if (dst_blend == DSBF_ONE)
        return STM_BLITTER_PD_DEST_OVER;
      if (dst_blend == DSBF_ZERO)
        return STM_BLITTER_PD_SOURCE_OUT;
      if (dst_blend == DSBF_SRCALPHA)
        return STM_BLITTER_PD_DEST_ATOP;
      if (dst_blend == DSBF_INVSRCALPHA)
        return STM_BLITTER_PD_XOR;
      break;

    case DSBF_SRCALPHA:
      if (dst_blend == DSBF_INVSRCALPHA)
        return STM_BLITTER_PD_NONE;
      break;

    case DSBF_DESTALPHA:
      if (dst_blend == DSBF_ZERO)
        return STM_BLITTER_PD_SOURCE_IN;
      if (dst_blend == DSBF_INVSRCALPHA)
        return STM_BLITTER_PD_SOURCE_ATOP;
      break;

    case DSBF_UNKNOWN:
    case DSBF_SRCCOLOR:
    case DSBF_INVSRCCOLOR:
    case DSBF_INVSRCALPHA:
    case DSBF_DESTCOLOR:
    case DSBF_INVDESTCOLOR:
    case DSBF_SRCALPHASAT:
    default:
      break;
    }

  return (enum stm_blitter_porter_duff_rule_e) -1;
}

static enum stm_blitter_surface_colorspace_e
__attribute__((warn_unused_result, const))
dscs_to_stm_blit (DFBSurfaceColorSpace dfb_cspace)
{
  switch (dfb_cspace)
    {
    case DSCS_RGB:
      return STM_BLITTER_SCS_RGB;
    case DSCS_BT601:
      return STM_BLITTER_SCS_BT601;
    case DSCS_BT601_FULLRANGE:
      return STM_BLITTER_SCS_BT601_FULLRANGE;
    case DSCS_BT709:
      return STM_BLITTER_SCS_BT709;

    case DSCS_UNKNOWN:
    default:
      break;
    }

  return (enum stm_blitter_surface_colorspace_e) -1;
}

static void
dfb_ckey_to_stm_blit (DFBSurfacePixelFormat       format,
                      uint32_t                    pixel,
                      struct stm_blitter_color_s * const ckey)
{
  DFBColor color;

  switch (format)
    {
    case DSPF_RGB332:
    case DSPF_ARGB1555:
    case DSPF_RGB555:
    case DSPF_BGR555:
    case DSPF_ARGB2554:
    case DSPF_ARGB4444:
    case DSPF_RGB444:
    case DSPF_RGBA4444:
    case DSPF_ARGB8565:
    case DSPF_RGB16:
    case DSPF_ARGB:
#if HAVE_DECL_DSPF_ABGR
    case DSPF_ABGR:
#endif
    case DSPF_RGB24:
#if HAVE_DECL_DSPF_BGR24
    case DSPF_BGR24:
#endif
    case DSPF_RGB32:
    case DSPF_AiRGB:
    case DSPF_RGBA5551:
    case DSPF_A8:
      dfb_pixel_to_color (format, pixel, &color);
      ckey->r = color.r;
      ckey->g = color.g;
      ckey->b = color.b;
      break;

    case DSPF_LUT8:
#if HAVE_DECL_DSPF_LUT1
    case DSPF_LUT1:
#endif /* HAVE_DECL_DSPF_LUT1 */
    case DSPF_LUT2:
    case DSPF_ALUT44:
#if HAVE_DECL_DSPF_LUT4
    case DSPF_LUT4:
#endif
#if HAVE_DECL_DSPF_ALUT8
    case DSPF_ALUT8:
#endif
      /* not sure what to do with these, we can either use the LUT index, or
         the actual color... */
      ckey->index = pixel;
      dfb_pixel_to_color (format, pixel, &color);
      ckey->r = color.r;
      ckey->g = color.g;
      ckey->b = color.b;
      break;

    case DSPF_I420:
    case DSPF_YV12:
    case DSPF_YV16:
      ckey->y  = (pixel >>  0) & 0xff;
      ckey->cb = (pixel >>  8) & 0xff;
      ckey->cr = (pixel >> 16) & 0xff;
      break;

    case DSPF_AYUV:
    case DSPF_YUV444P:
      ckey->y  = (pixel >> 16) & 0xff;
      ckey->cb = (pixel >>  8) & 0xff;
      ckey->cr = (pixel >>  0) & 0xff;
      break;

    case DSPF_AVYU:
    case DSPF_VYU:
      ckey->y  = (pixel >>  8) & 0xff;
      ckey->cb = (pixel >>  0) & 0xff;
      ckey->cr = (pixel >> 16) & 0xff;
      break;

    case DSPF_YUY2:
      ckey->y  = (pixel >>  0) & 0xff;
#ifdef WORDS_BIGENDIAN
      ckey->cb = (pixel >> 24) & 0xff;
      ckey->cr = (pixel >>  8) & 0xff;
#else
      ckey->cb = (pixel >>  8) & 0xff;
      ckey->cr = (pixel >> 24) & 0xff;
#endif
      break;

    case DSPF_UYVY:
      ckey->y  = (pixel >> 24) & 0xff;
#ifdef WORDS_BIGENDIAN
      ckey->cb = (pixel >> 16) & 0xff;
      ckey->cr = (pixel >>  0) & 0xff;
#else
      ckey->cb = (pixel >>  0) & 0xff;
      ckey->cr = (pixel >> 16) & 0xff;
#endif
      break;

    case DSPF_NV12:
    case DSPF_NV16:
    case DSPF_NV21:
#if HAVE_DECL_DSPF_NV24
    case DSPF_NV24:
#endif
      dfb_pixel_to_color (format, pixel, &color);
      ckey->y  = color.g;
      ckey->cb = color.b;
      ckey->cr = color.r;
      break;

    case DSPF_A1:
    case DSPF_A1_LSB:
    case DSPF_A4:
    case DSPF_ARGB1666:
    case DSPF_ARGB6666:
    case DSPF_RGB18:
#if HAVE_DECL_DSPF_RGBAF88871
    case DSPF_RGBAF88871:
#endif

#if HAVE_DECL_DSPF_NV12MB
    case DSPF_NV12MB:
#endif
#if HAVE_DECL_DSPF_NV16MB
    case DSPF_NV16MB:
#endif

#if HAVE_DECL_DSPF_BYTE
    case DSPF_BYTE: /* not sure what to do with this one */
#endif
    case DSPF_UNKNOWN:
      break;
    }
}

static void
setup_chroma_offsets (stm_blitter_surface_t *surf)
{
  unsigned int factor_h, factor_v;

  factor_h = factor_v = 1;

  switch (surf->format)
    {
    case STM_BLITTER_SF_I420:
      factor_v = 2;
      /* fall through */
    case STM_BLITTER_SF_YV61:
      factor_h = 2;
      /* fall through */
    case STM_BLITTER_SF_YCBCR444P:
      /* three planes */
      surf->buffer_address.cb_offset = surf->stride * surf->size.h;
      surf->buffer_address.cr_offset = (surf->buffer_address.cb_offset
                                        + ((surf->stride / factor_h)
                                           * (surf->size.h / factor_v)));
      break;

    case STM_BLITTER_SF_YV12:
      factor_v = 2;
      /* fall through */
    case STM_BLITTER_SF_YV16:
      factor_h = 2;
      /* three planes */
      surf->buffer_address.cr_offset = surf->stride * surf->size.h;
      surf->buffer_address.cb_offset = (surf->buffer_address.cr_offset
                                        + ((surf->stride / factor_h)
                                           * (surf->size.h / factor_v)));
      break;

    case STM_BLITTER_SF_NV12:
    case STM_BLITTER_SF_NV21:
    case STM_BLITTER_SF_YCBCR420MB:
    case STM_BLITTER_SF_NV16:
    case STM_BLITTER_SF_NV61:
    case STM_BLITTER_SF_YCBCR422MB:
    case STM_BLITTER_SF_NV24:
      /* two planes */
      surf->buffer_address.cbcr_offset = surf->stride * surf->size.h;
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

    case STM_BLITTER_SF_RLD_BD:
    case STM_BLITTER_SF_RLD_H2:
    case STM_BLITTER_SF_RLD_H8:
      /* nothing to do */
      break;

    case STM_BLITTER_SF_INVALID:
    case STM_BLITTER_SF_COUNT:
    default:
      /* should not be reached */
      break;
    }
}

int
bdisp_aq_convert_to_supported_rotation (const CardState     * const state,
                                        DFBAccelerationMask  accel,
                                        const int32_t       *matrix)
{
//  if (state)
//    _dump_check_state (__func__, state, accel);

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
      && matrix[3] == 0 && matrix[4] == 1 * fixedpointONE)
    return 0; /* nothing */

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

  return -1;
}

void
bdisp_aq_StateInit (void      * const driver_data,
                    void      * const device_data,
                    CardState * const state)
{
  if (!state->gfxcard_data)
    {
      struct stm_blitter_state *bdisp2_state;

      FusionSHMPoolShared *pool = dfb_core_shmpool_data (NULL);

      bdisp2_state = SHCALLOC (pool, 1, sizeof (*bdisp2_state));
      if (bdisp2_state)
        {
          bdisp2_state->src = SHCALLOC (pool, 1, sizeof (*bdisp2_state->src));
          bdisp2_state->src2 = SHCALLOC (pool, 1, sizeof (*bdisp2_state->src2));
          bdisp2_state->dst = SHCALLOC (pool, 1, sizeof (*bdisp2_state->dst));

          if (!bdisp2_state->src || !bdisp2_state->src2 || !bdisp2_state->dst)
            {
              SHFREE (pool, bdisp2_state->src2);
              SHFREE (pool, bdisp2_state->src);
              SHFREE (pool, bdisp2_state->dst);
              SHFREE (pool, bdisp2_state);
              bdisp2_state = NULL;
            }
        }

      state->gfxcard_data = bdisp2_state;
    }
}

void
bdisp_aq_StateDestroy (void      * const driver_data,
                       void      * const device_data,
                       CardState * const state)
{
  if (state->gfxcard_data)
    {
      struct stm_blitter_state * const bdisp2_state = state->gfxcard_data;

      FusionSHMPoolShared *pool = dfb_core_shmpool_data (NULL);

      SHFREE (pool, bdisp2_state->src2);
      SHFREE (pool, bdisp2_state->src);
      SHFREE (pool, bdisp2_state->dst);
      SHFREE (pool, bdisp2_state);

      state->gfxcard_data = NULL;
    }
}

void
bdisp_aq_CheckState (void                * const driver_data,
                     void                * const device_data,
                     CardState           * const state,
                     DFBAccelerationMask  accel)
{
//  struct _STGFX2DriverData * const drvdata = driver_data;
  struct _STGFX2DeviceData * const devdata = device_data;

  struct stm_blitter_state *bdisp2_state = state->gfxcard_data;
  enum stm_blitter_accel    bdisp2_accel;

  bool smooth_upscale, smooth_downscale;

  if (!state->gfxcard_data)
    {
      bdisp_aq_StateInit (driver_data, device_data, state);
      bdisp2_state = state->gfxcard_data;
    }

  _dump_check_state (__func__, state, accel);

  /* return if the desired function is not supported at all. */
  bdisp2_accel = (enum stm_blitter_accel) 0;
  if (accel & DFXL_FILLRECTANGLE)
    bdisp2_accel |= STM_BLITTER_ACCEL_FILLRECT;
  if (accel & DFXL_DRAWRECTANGLE)
    bdisp2_accel |= STM_BLITTER_ACCEL_DRAWRECT;
  if (accel & DFXL_BLIT)
    bdisp2_accel |= STM_BLITTER_ACCEL_BLIT;
  if (accel & DFXL_STRETCHBLIT)
    bdisp2_accel |= STM_BLITTER_ACCEL_STRETCHBLIT;
  if (accel & DFXL_BLIT2)
    bdisp2_accel |= STM_BLITTER_ACCEL_BLIT2;

  if (!bdisp2_accel)
    goto out;

  /* convert destination format */
  if (DFB_PIXELFORMAT_INDEX (state->destination->config.format)
      >= D_ARRAY_SIZE (dspf_to_stm_blit))
    /* format exists now, but when this driver was compiled, it wasn't part
       of the DirectFB API (yet). */
    goto out;
  bdisp2_state->dst->format
    = dspf_to_stm_blit[DFB_PIXELFORMAT_INDEX (state->destination
                                                          ->config.format)];
  bdisp2_state->dst->colorspace
    = dscs_to_stm_blit (state->destination->config.colorspace);
  bdisp2_state->dst->size
    = (stm_blitter_dimension_t)
        { state->destination->config.size.w,
          state->destination->config.size.h };

  if (DFB_DRAWING_FUNCTION (accel))
    {
      bdisp2_state->drawflags = STM_BLITTER_SDF_NONE;

      if (state->drawingflags == DSDRAW_NOFX)
        bdisp2_state->drawflags |= STM_BLITTER_SDF_NONE;
      else
        {
          if (state->drawingflags & DSDRAW_BLEND)
            bdisp2_state->drawflags |= STM_BLITTER_SDF_BLEND;
          if (state->drawingflags & DSDRAW_DST_COLORKEY)
            bdisp2_state->drawflags |= STM_BLITTER_SDF_DST_COLORKEY;
          if (state->drawingflags & DSDRAW_SRC_PREMULTIPLY)
            bdisp2_state->drawflags |= STM_BLITTER_SDF_SRC_PREMULTIPLY;
          if (state->drawingflags & DSDRAW_DST_PREMULTIPLY)
            bdisp2_state->drawflags |= STM_BLITTER_SDF_DST_PREMULTIPLY;
          if (state->drawingflags & DSDRAW_XOR)
            bdisp2_state->drawflags |= STM_BLITTER_SDF_XOR;
          if (state->drawingflags & DSDRAW_DEMULTIPLY)
            goto out;
          /* STM_BLITTER_SDF_GRADIENT and STM_BLITTER_SDF_ANTIALIAS are not
             supported by DirectFB */
        }
    }
  else /* if (DFB_BLITTING_FUNCTION (accel)) */
    {
      bdisp2_state->blitflags = STM_BLITTER_SBF_NONE;

      /* convert source format */
      if (DFB_PIXELFORMAT_INDEX (state->source->config.format)
          >= D_ARRAY_SIZE (dspf_to_stm_blit))
        /* format exists now, but when this driver was compiled, it wasn't part
           of the DirectFB API (yet). */
        goto out;
      bdisp2_state->src->format
        = dspf_to_stm_blit[DFB_PIXELFORMAT_INDEX (state->source
                                                            ->config.format)];
      bdisp2_state->src->colorspace
        = dscs_to_stm_blit (state->source->config.colorspace);
      bdisp2_state->src->size
        = (stm_blitter_dimension_t)
            { state->source->config.size.w,
              state->source->config.size.h };

      /* convert source2 format */
      if (state->source2)
        {
          bdisp2_state->src2->format
            = dspf_to_stm_blit[DFB_PIXELFORMAT_INDEX (state->source2
                                                            ->config.format)];
          bdisp2_state->src2->colorspace
            = dscs_to_stm_blit (state->source2->config.colorspace);
          bdisp2_state->src2->size
            = (stm_blitter_dimension_t)
                { state->source2->config.size.w,
                  state->source2->config.size.h };
        }

      if (state->blittingflags == DSBLIT_NOFX)
        bdisp2_state->blitflags |= STM_BLITTER_SBF_NONE;
      else
        {
          if (state->blittingflags & DSBLIT_DEMULTIPLY)
            goto out;
          if (state->blittingflags & DSBLIT_DEINTERLACE)
            goto out;
          if (state->blittingflags & DSBLIT_COLORKEY_PROTECT)
            goto out;
          if (state->blittingflags & DSBLIT_SRC_MASK_ALPHA)
            goto out;
          if (state->blittingflags & DSBLIT_SRC_MASK_COLOR)
            goto out;

          if (state->blittingflags & DSBLIT_BLEND_ALPHACHANNEL)
            bdisp2_state->blitflags |= STM_BLITTER_SBF_BLEND_ALPHACHANNEL;
          if (state->blittingflags & DSBLIT_BLEND_COLORALPHA)
            bdisp2_state->blitflags |= STM_BLITTER_SBF_BLEND_COLORALPHA;
          if (state->blittingflags & DSBLIT_COLORIZE)
/* FIXME (matrix color) state->src_colormatrix */
            bdisp2_state->blitflags |= STM_BLITTER_SBF_SRC_COLORIZE;
          if (state->blittingflags & DSBLIT_SRC_COLORKEY)
            bdisp2_state->blitflags |= STM_BLITTER_SBF_SRC_COLORKEY;
          if (state->blittingflags & DSBLIT_DST_COLORKEY)
            bdisp2_state->blitflags |= STM_BLITTER_SBF_DST_COLORKEY;
          if (state->blittingflags & DSBLIT_SRC_PREMULTIPLY)
            bdisp2_state->blitflags |= STM_BLITTER_SBF_SRC_PREMULTIPLY;
          if (state->blittingflags & DSBLIT_DST_PREMULTIPLY)
            bdisp2_state->blitflags |= STM_BLITTER_SBF_DST_PREMULTIPLY;
          if (state->blittingflags & DSBLIT_SRC_PREMULTCOLOR)
            bdisp2_state->blitflags |= STM_BLITTER_SBF_SRC_PREMULTCOLOR;
          if (state->blittingflags & DSBLIT_XOR)
            bdisp2_state->blitflags |= STM_BLITTER_SBF_XOR;
          if ((state->blittingflags & DSBLIT_ROTATE180)
              || ((state->blittingflags
                   & (DSBLIT_FLIP_HORIZONTAL | DSBLIT_FLIP_VERTICAL))
                  == (DSBLIT_FLIP_HORIZONTAL | DSBLIT_FLIP_VERTICAL)))
            bdisp2_state->blitflags |= STM_BLITTER_SBF_ROTATE180;
          else if (state->blittingflags & DSBLIT_FLIP_VERTICAL)
            bdisp2_state->blitflags |= STM_BLITTER_SBF_FLIP_VERTICAL;
          else if (state->blittingflags & DSBLIT_FLIP_HORIZONTAL)
            bdisp2_state->blitflags |= STM_BLITTER_SBF_FLIP_HORIZONTAL;
          else if (state->blittingflags & DSBLIT_ROTATE90)
            bdisp2_state->blitflags |= STM_BLITTER_SBF_ROTATE90;
          else if (state->blittingflags & DSBLIT_ROTATE270)
            bdisp2_state->blitflags |= STM_BLITTER_SBF_ROTATE270;
          if (state->blittingflags & DSBLIT_INDEX_TRANSLATION)
            ; /* nothing */
#if HAVE_DECL_DSBLIT_SOURCE2
          if (state->blittingflags & DSBLIT_SOURCE2)
            bdisp2_state->blitflags |= STM_BLITTER_SBF_READ_SOURCE2;
#endif /* HAVE_DECL_DSBLIT_SOURCE2 */
#if HAVE_DECL_DSBLIT_FIXEDPOINT
          if (state->blittingflags & DSBLIT_FIXEDPOINT)
            bdisp2_state->blitflags |= STM_BLITTER_SBF_ALL_IN_FIXED_POINT;
#endif
        }

      switch (bdisp_aq_convert_to_supported_rotation (state, accel,
                                                      state->matrix))
        {
        case 0:
          break;

        case 180:
          bdisp2_state->blitflags |= STM_BLITTER_SBF_ROTATE180;
          break;

        case 181:
          bdisp2_state->blitflags |= STM_BLITTER_SBF_FLIP_VERTICAL;
          break;
        case 182:
          bdisp2_state->blitflags |= STM_BLITTER_SBF_FLIP_HORIZONTAL;
          break;

        case 270:
          bdisp2_state->blitflags |= STM_BLITTER_SBF_ROTATE270;
          break;
        case 90:
          bdisp2_state->blitflags |= STM_BLITTER_SBF_ROTATE90;
          break;

        case -1:
        default:
          goto out;
        }

      if (bdisp2_state->dst->format & STM_BLITTER_SF_INDEXED)
        {
          if (!(state->blittingflags & DSBLIT_INDEX_TRANSLATION))
            /* struct stm_blitter_s has no such flag (index translation is
               the only mode supported anyway and as such implied), but
               let's enforce it in the DirectFB glue */
            goto out;
        }
    }

  /* additional draw and blitflags */
  if (state->render_options & DSRO_ANTIALIAS)
    {
      bdisp2_state->drawflags |= STM_BLITTER_SDF_ANTIALIAS;
      bdisp2_state->blitflags |= STM_BLITTER_SBF_ANTIALIAS;
    }
  /* smooth upscale and smooth downscale must be set the same */
  smooth_upscale   = !!(state->render_options & DSRO_SMOOTH_UPSCALE);
  smooth_downscale = !!(state->render_options & DSRO_SMOOTH_DOWNSCALE);
  if (smooth_upscale != smooth_downscale)
    goto out;
  if (!smooth_upscale)
    bdisp2_state->blitflags |= STM_BLITTER_SBF_NO_FILTER;
  else
    bdisp2_state->blitflags &= ~STM_BLITTER_SBF_NO_FILTER;

  bdisp2_state->pd = dsbf_to_stm_blit (state->src_blend,
                                       state->dst_blend);
  if (bdisp2_state->pd == (stm_blitter_porter_duff_rule_t) -1)
    goto out;

  bdisp2_accel = bdisp2_state_supported (&devdata->stdev,
                                         bdisp2_state, bdisp2_accel);

  if (bdisp2_accel & STM_BLITTER_ACCEL_FILLRECT)
    state->accel |= DFXL_FILLRECTANGLE;
  if (bdisp2_accel & STM_BLITTER_ACCEL_DRAWRECT)
    state->accel |= DFXL_DRAWRECTANGLE;
  if (bdisp2_accel & STM_BLITTER_ACCEL_BLIT)
    state->accel |= DFXL_BLIT;
  if (bdisp2_accel & STM_BLITTER_ACCEL_STRETCHBLIT)
    state->accel |= DFXL_STRETCHBLIT;
  if (bdisp2_accel & STM_BLITTER_ACCEL_BLIT2)
    state->accel |= DFXL_BLIT2;

  return;

out:
  _dump_check_state_failed (__func__, state, accel);

  return;
}

void
bdisp_aq_SetState (void                *driver_data,
                   void                *device_data,
                   GraphicsDeviceFuncs *funcs,
                   CardState           *state,
                   DFBAccelerationMask  accel)
{
  struct _STGFX2DriverData * const drvdata = driver_data;
  struct _STGFX2DeviceData * const devdata = device_data;

  struct stm_blitter_state *bdisp2_state = state->gfxcard_data;
  enum stm_blitter_accel    bdisp2_accel;

  int i;

  _dump_set_state (__func__, state, accel);

  stm_blit_printd (BDisp_State, "%s: %.8x\n", __func__, state->mod_hw);

  bdisp2_state->modified = STM_BLITTER_SMF_NONE;

  if (state->mod_hw == SMF_ALL)
    bdisp2_state->modified = STM_BLITTER_SMF_ALL;
  else
    {
      if (state->mod_hw & SMF_DRAWING_FLAGS)
        {
          bdisp2_state->modified |= STM_BLITTER_SMF_DRAWFLAGS;
          state->mod_hw &= ~SMF_DRAWING_FLAGS;
        }
      if (state->mod_hw & SMF_BLITTING_FLAGS)
        {
          bdisp2_state->modified |= STM_BLITTER_SMF_BLITFLAGS;
          state->mod_hw &= ~SMF_BLITTING_FLAGS;
        }
      if (state->mod_hw & SMF_CLIP)
        {
        }
      if (state->mod_hw & SMF_COLOR)
        {
        }
      if (state->mod_hw & (SMF_SRC_BLEND | SMF_DST_BLEND))
        {
          bdisp2_state->modified |= STM_BLITTER_SMF_PORTER_DUFF;
          state->mod_hw &= ~(SMF_SRC_BLEND | SMF_DST_BLEND);
        }
      if (state->mod_hw & SMF_SRC_COLORKEY)
        {
        }
      if (state->mod_hw & SMF_DST_COLORKEY)
        {
        }
      if (state->mod_hw & SMF_DESTINATION)
        {
        }
      if (state->mod_hw & SMF_SOURCE)
        {
        }
      if (state->mod_hw & SMF_SOURCE2)
        {
        }

      if (state->mod_hw & SMF_SOURCE_MASK)
        state->mod_hw &= ~SMF_SOURCE_MASK;
      if (state->mod_hw & SMF_SOURCE_MASK_VALS)
        state->mod_hw &= ~SMF_SOURCE_MASK_VALS;
      if (state->mod_hw & SMF_INDEX_TRANSLATION)
        state->mod_hw &= ~SMF_INDEX_TRANSLATION;
      if (state->mod_hw & SMF_COLORKEY)
        state->mod_hw &= ~SMF_COLORKEY;
      if (state->mod_hw & SMF_RENDER_OPTIONS)
        {
          bdisp2_state->modified |= STM_BLITTER_SMF_FILTERING;
          state->mod_hw &= ~SMF_RENDER_OPTIONS;
        }
      if (state->mod_hw & SMF_MATRIX)
        {
          /* if matrix has changed, the rotation flag will have changed. */
          bdisp2_state->modified |= STM_BLITTER_SMF_BLITFLAGS;
          state->mod_hw &= ~SMF_MATRIX;
        }
    }

  /* smooth value must be the same for up- and downscale */

  if (state->mod_hw & SMF_CLIP)
    {
      bdisp2_state->clip.position1.x = state->clip.x1;
      bdisp2_state->clip.position1.y = state->clip.y1;
      bdisp2_state->clip.position2.x = state->clip.x2;
      bdisp2_state->clip.position2.y = state->clip.y2;
      bdisp2_state->clip_mode = STM_BLITTER_CM_CLIP_INSIDE;

      bdisp2_state->modified |= STM_BLITTER_SMF_CLIP;
      state->mod_hw &= ~SMF_CLIP;
    }

  if (state->mod_hw & SMF_COLOR)
    {
      bdisp2_state->colors[0].a = state->color.a;
      bdisp2_state->colors[0].r = state->color.r;
      bdisp2_state->colors[0].g = state->color.g;
      bdisp2_state->colors[0].b = state->color.b;
      bdisp2_state->color_colorspace = STM_BLITTER_COLOR_RGB;

      bdisp2_state->color_index = state->color_index;

      bdisp2_state->modified |= STM_BLITTER_SMF_COLOR;
      state->mod_hw &= ~SMF_COLOR;
    }

  /* dst color key */
  if (state->mod_hw & SMF_DST_COLORKEY)
    {
      dfb_ckey_to_stm_blit (state->destination->config.format,
                            state->dst_colorkey,
                            &bdisp2_state->dst_ckey[0]);
      bdisp2_state->dst_ckey[1] = bdisp2_state->dst_ckey[0];
      bdisp2_state->dst_ckey_mode = STM_BLITTER_CKM_RGB;

      bdisp2_state->modified |= STM_BLITTER_SMF_DST_COLORKEY;
      state->mod_hw &= ~SMF_DST_COLORKEY;
    }

  /* destination */
  if (state->mod_hw & SMF_DESTINATION)
    {
      D_ASSERT (bdisp2_state->dst != NULL);

      bdisp2_state->dst->buffer_address.base = state->dst.phys;
      bdisp2_state->dst->buffer_size = -1; /* FIXME */
      bdisp2_state->dst->stride = state->dst.pitch;

      setup_chroma_offsets (bdisp2_state->dst);

      bdisp2_state->modified |= STM_BLITTER_SMF_DST;
      state->mod_hw &= ~SMF_DESTINATION;
    }

  /* destination CLUT palette */
  if (state->destination
      && DFB_PIXELFORMAT_IS_INDEXED (state->destination->config.format))
    {
      if (bdisp2_state->dst->buffer_address.palette.num_entries
          != state->destination->palette->num_entries)
        {
          if (bdisp2_state->dst->buffer_address.palette.entries)
            D_FREE (bdisp2_state->dst->buffer_address.palette.entries);
          bdisp2_state->dst->buffer_address.palette.clut_base = 0;

          bdisp2_state->dst->buffer_address.palette.num_entries = state->destination->palette->num_entries;
          bdisp2_state->dst->buffer_address.palette.entries
            =  D_MALLOC (sizeof (*bdisp2_state->dst->buffer_address.palette.entries)
                         * bdisp2_state->dst->buffer_address.palette.num_entries);
        }

      for (i = 0;
           i < bdisp2_state->dst->buffer_address.palette.num_entries;
           ++i)
        {
          bdisp2_state->dst->buffer_address.palette.entries[i].a
            = state->destination->palette->entries[i].a;
          bdisp2_state->dst->buffer_address.palette.entries[i].r
            = state->destination->palette->entries[i].r;
          bdisp2_state->dst->buffer_address.palette.entries[i].g
            = state->destination->palette->entries[i].g;
          bdisp2_state->dst->buffer_address.palette.entries[i].b
            = state->destination->palette->entries[i].b;
        }
    }

  if (DFB_BLITTING_FUNCTION (accel))
    {
      /* src color key.
         We need state->source to be able to transform the ckey from its native
         format back to RGB.
         This is a bit complicated, as SMF_SRC_COLORKEY might be set (due to
         SMF_ALL) without state->source being != NULL and I have also seen
         subsequent calls to have SMF_SRC_COLORKEY unset, because */
      if (state->mod_hw & SMF_SRC_COLORKEY)
        {
          dfb_ckey_to_stm_blit (state->source->config.format,
                                state->src_colorkey,
                                &bdisp2_state->src_ckey[0]);
          bdisp2_state->src_ckey[1] = bdisp2_state->src_ckey[0];
          bdisp2_state->src_ckey_mode = STM_BLITTER_CKM_RGB;

          bdisp2_state->modified |= STM_BLITTER_SMF_SRC_COLORKEY;
          state->mod_hw &= ~SMF_SRC_COLORKEY;
        }
#if 0
      if (state->source
          && (bdisp2_state->src_ckey[0].a == 0
              || (modified & (SMF_SRC_COLORKEY | SMF_SOURCE))))
        {
          dfb_ckey_to_stm_blit (state->source->config.format,
                                state->src_colorkey,
                                &bdisp2_state->src_ckey[0]);
          bdisp2_state->src_ckey[1] = bdisp2_state->src_ckey[0];
          bdisp2_state->src_ckey_mode = STM_BLITTER_CKM_RGB;

          /* remember that we have updated this */
          bdisp2_state->src_ckey[0].a = 0xff;
        }
      else if (!state->source
               && (modified & (SMF_SRC_COLORKEY | SMF_SOURCE)))
        bdisp2_state->src_ckey[0].a = 0;
#endif

      /* source */
      if (state->mod_hw & SMF_SOURCE)
        {
          D_ASSERT (bdisp2_state->src != NULL);

          bdisp2_state->src->buffer_address.base = state->src.phys;
          bdisp2_state->src->buffer_size = -1; /* FIXME */
          bdisp2_state->src->stride = state->src.pitch;

          setup_chroma_offsets (bdisp2_state->src);

          bdisp2_state->modified |= STM_BLITTER_SMF_SRC;
          state->mod_hw &= ~SMF_SOURCE;
        }

      /* source2 */
      if (state->mod_hw & SMF_SOURCE2)
        {
          D_ASSERT (bdisp2_state->src2 != NULL);

          bdisp2_state->src2->buffer_address.base = state->src2.phys;
          bdisp2_state->src2->buffer_size = -1; /* FIXME */
          bdisp2_state->src2->stride = state->src2.pitch;

          setup_chroma_offsets (bdisp2_state->src2);

          bdisp2_state->modified |= STM_BLITTER_SMF_SRC2;
          state->mod_hw &= ~SMF_SOURCE2;
        }

      /* source CLUT palette */
      if (DFB_PIXELFORMAT_IS_INDEXED (state->source->config.format))
        {
          if (bdisp2_state->src->buffer_address.palette.num_entries
              != state->source->palette->num_entries)
            {
              if (bdisp2_state->src->buffer_address.palette.entries)
                D_FREE (bdisp2_state->src->buffer_address.palette.entries);
              bdisp2_state->src->buffer_address.palette.clut_base = 0;

              bdisp2_state->src->buffer_address.palette.num_entries = state->source->palette->num_entries;
              bdisp2_state->src->buffer_address.palette.entries
                =  D_MALLOC (sizeof (*bdisp2_state->src->buffer_address.palette.entries)
                             * bdisp2_state->src->buffer_address.palette.num_entries);
            }

          for (i = 0;
               i < bdisp2_state->src->buffer_address.palette.num_entries;
               ++i)
            {
              bdisp2_state->src->buffer_address.palette.entries[i].a
                = state->source->palette->entries[i].a;
              bdisp2_state->src->buffer_address.palette.entries[i].r
                = state->source->palette->entries[i].r;
              bdisp2_state->src->buffer_address.palette.entries[i].g
                = state->source->palette->entries[i].g;
              bdisp2_state->src->buffer_address.palette.entries[i].b
                = state->source->palette->entries[i].b;
            }
        }

      /* source2 CLUT palette */
      if (state->source2
          && DFB_PIXELFORMAT_IS_INDEXED (state->source2->config.format))
        {
          if (bdisp2_state->src2->buffer_address.palette.num_entries
              != state->source2->palette->num_entries)
            {
              if (bdisp2_state->src2->buffer_address.palette.entries)
                D_FREE (bdisp2_state->src2->buffer_address.palette.entries);
              bdisp2_state->src2->buffer_address.palette.clut_base = 0;

              bdisp2_state->src2->buffer_address.palette.num_entries = state->source2->palette->num_entries;
              bdisp2_state->src2->buffer_address.palette.entries
                =  D_MALLOC (sizeof (*bdisp2_state->src2->buffer_address.palette.entries)
                             * bdisp2_state->src2->buffer_address.palette.num_entries);
            }

          for (i = 0;
               i < bdisp2_state->src2->buffer_address.palette.num_entries;
               ++i)
            {
              bdisp2_state->src2->buffer_address.palette.entries[i].a
                = state->source2->palette->entries[i].a;
              bdisp2_state->src2->buffer_address.palette.entries[i].r
                = state->source2->palette->entries[i].r;
              bdisp2_state->src2->buffer_address.palette.entries[i].g
                = state->source2->palette->entries[i].g;
              bdisp2_state->src2->buffer_address.palette.entries[i].b
                = state->source2->palette->entries[i].b;
            }
        }
    }

  bdisp2_accel = (enum stm_blitter_accel) 0;
  if (accel & DFXL_FILLRECTANGLE)
    bdisp2_accel |= STM_BLITTER_ACCEL_FILLRECT;
  if (accel & DFXL_DRAWRECTANGLE)
    bdisp2_accel |= STM_BLITTER_ACCEL_DRAWRECT;
//  if (accel & DFXL_DRAWLINE)
//    bdisp2_accel |= 0;
//  if (accel & DFXL_FILLTRIANGLE)
//    bdisp2_accel |= 0;
//  if (accel & DFXL_FILLTRAPEZOID)
//    bdisp2_accel |= 0;
  if (accel & DFXL_BLIT)
    bdisp2_accel |= STM_BLITTER_ACCEL_BLIT;
  if (accel & DFXL_STRETCHBLIT)
    bdisp2_accel |= STM_BLITTER_ACCEL_STRETCHBLIT;
//  if (accel & DFXL_TEXTRIANGLES)
//    bdisp2_accel |= 0;
  if (accel & DFXL_BLIT2)
    bdisp2_accel |= STM_BLITTER_ACCEL_BLIT2;

  bdisp2_state->set = (enum stm_blitter_accel) 0;
  if (state->set & DFXL_FILLRECTANGLE)
    bdisp2_state->set |= STM_BLITTER_ACCEL_FILLRECT;
  if (state->set & DFXL_DRAWRECTANGLE)
    bdisp2_state->set |= STM_BLITTER_ACCEL_DRAWRECT;
  if (state->set & DFXL_BLIT)
    bdisp2_state->set |= STM_BLITTER_ACCEL_BLIT;
  if (state->set & DFXL_STRETCHBLIT)
    bdisp2_state->set |= STM_BLITTER_ACCEL_STRETCHBLIT;
  if (state->set & DFXL_BLIT2)
    bdisp2_state->set |= STM_BLITTER_ACCEL_BLIT2;

  bdisp2_state_update (&drvdata->stdrv, &devdata->stdev,
                       bdisp2_state, bdisp2_accel);

  state->set = DFXL_NONE;
  if (bdisp2_state->set & STM_BLITTER_ACCEL_FILLRECT)
    state->set |= DFXL_FILLRECTANGLE;
  if (bdisp2_state->set & STM_BLITTER_ACCEL_DRAWRECT)
    state->set |= DFXL_DRAWRECTANGLE;
  if (bdisp2_state->set & STM_BLITTER_ACCEL_BLIT)
    state->set |= DFXL_BLIT;
  if (bdisp2_state->set & STM_BLITTER_ACCEL_STRETCHBLIT)
    state->set |= DFXL_STRETCHBLIT;
  if (bdisp2_state->set & STM_BLITTER_ACCEL_BLIT2)
    state->set |= DFXL_BLIT2;
}
