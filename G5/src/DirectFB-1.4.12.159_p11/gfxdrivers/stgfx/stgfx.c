/*
   (c) Copyright 2003, 2007, 2009, 2010  STMicroelectronics Ltd.

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

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <sys/mman.h>
#include <fcntl.h>
#include <sys/ioctl.h>

#include <linux/fb.h>
#include <linux/stmfb.h>

#include <directfb.h>
#include <dfb_types.h>

#include <core/coredefs.h>
#include <core/coretypes.h>

#include <core/core.h>
#include <core/state.h>
#include <core/gfxcard.h>
#include <core/surface.h>
#include <core/system.h>
#include <core/palette.h>
#include <core/layers.h>
#include <core/screens.h>

#include <fbdev/fbdev.h>
#include <stmfbdev/stmfbdev.h>

#include <gfx/convert.h>
#include <gfx/util.h>

#include <core/graphics_driver.h>

#include "stgfx.h"
#include "stgfx2/stgfx_screen.h"
#include "stgfx2/stgfx_primary.h"
#include "stgfx2/bdisp_surface_pool.h"

DFB_GRAPHICS_DRIVER( stgfx );

static const DFBSurfaceDrawingFlags STGFX_VALID_DRAWINGFLAGS_GAMMA = (
  DSDRAW_BLEND           |
  DSDRAW_DST_COLORKEY    |
  DSDRAW_SRC_PREMULTIPLY);

static const DFBSurfaceDrawingFlags STGFX_VALID_DRAWINGFLAGS_BDISP = (
  DSDRAW_BLEND           |
  DSDRAW_XOR             |
  DSDRAW_SRC_PREMULTIPLY);

static DFBSurfaceDrawingFlags STGFX_VALID_DRAWINGFLAGS;

static const DFBAccelerationMask STGFX_VALID_DRAWINGFUNCTIONS = DFXL_FILLRECTANGLE |
                                                                DFXL_DRAWRECTANGLE;

static const DFBSurfaceDrawingFlags STGFX_VALID_BLITTINGFLAGS_GAMMA = (
  DSBLIT_INDEX_TRANSLATION  |
  DSBLIT_SRC_COLORKEY       |
  DSBLIT_DST_COLORKEY       |
  DSBLIT_BLEND_COLORALPHA   |
  DSBLIT_BLEND_ALPHACHANNEL |
  DSBLIT_SRC_PREMULTIPLY    |
  DSBLIT_SRC_PREMULTCOLOR);

static const DFBSurfaceDrawingFlags STGFX_VALID_BLITTINGFLAGS_BDISP = (
  DSBLIT_INDEX_TRANSLATION  |
  DSBLIT_SRC_COLORKEY       |
  DSBLIT_BLEND_COLORALPHA   |
  DSBLIT_BLEND_ALPHACHANNEL |
  DSBLIT_SRC_PREMULTIPLY    |
  DSBLIT_XOR                |
  DSBLIT_COLORIZE           |
  DSBLIT_SRC_PREMULTCOLOR);


static DFBSurfaceDrawingFlags STGFX_VALID_BLITTINGFLAGS;

static const DFBAccelerationMask STGFX_VALID_BLITTINGFUNCTIONS = (DFXL_BLIT | DFXL_STRETCHBLIT);

static int stgfx_accel_type;

static bool stgfxCheckPixelFormat(DFBSurfacePixelFormat dfbFmt, bool bSrc, bool *hasAlpha, bool *isClut);
static unsigned long stgfxSetPixelFormat(DFBSurfacePixelFormat dfbFmt) __attribute__((pure));
/* local typedefs */


/*****************************************************************/
/* Fill in the command data that is common to all draw functions */
/*****************************************************************/
static bool stgfxDraw
(
  STGFXDriverData*  pDrv,
  STGFXDeviceData*  pDev,
  DFBRectangle*     rect,
  STMFBIO_BLT_DATA   *pBltData
)
{
  pBltData->dstMemBase = pDev->dstMemBase;
  pBltData->dstOffset  = pDev->dstOffset;
  pBltData->dstPitch   = pDev->dstPitch;

  pBltData->dstFormat  = pDev->dstFormat;

  pBltData->dst_left   = (unsigned short)rect->x;
  pBltData->dst_top    = (unsigned short)rect->y;
  pBltData->dst_right  = (unsigned short)(rect->x + rect->w);
  pBltData->dst_bottom = (unsigned short)(rect->y + rect->h);
  pBltData->colour     = pDev->drawColour;

  if (pDev->drawingFlags & DSDRAW_DST_COLORKEY)
  {
    pBltData->ulFlags   |= BLT_OP_FLAGS_DST_COLOR_KEY;
    pBltData->colourKey  = pDev->dstColourKey;
  }

  if (pDev->drawingFlags & DSDRAW_BLEND)
  {
    if(pDev->srcBlendFunc == DSBF_ZERO && pDev->dstBlendFunc == DSBF_ZERO)
    {
      /*
       * This is a clear, just fill with 0. Set the source format to the
       * dest format to get a fast fill.
       */
      pBltData->colour    = 0;
      pBltData->srcFormat = pBltData->dstFormat;
    }
    else if(pDev->srcBlendFunc == DSBF_ONE && pDev->dstBlendFunc == DSBF_ZERO)
    {
      /*
       * This is just a src fill, but the colour is in ARGB8888 not the
       * destination surface format. The hardware will convert for us.
       */
      pBltData->srcFormat = SURF_ARGB8888;
    }
    else if(pDev->srcBlendFunc == DSBF_ZERO && pDev->dstBlendFunc == DSBF_ONE)
    {
      /*
       * This leaves the destination as is, i.e. a nop, because we do not
       * support destination pre-multiplication.
       */
      return true;
    }
    else
    {
      /*
       * This must be
       *    (1) src func = 1.0  , dest func = 1-src alpha
       * or (2) src func = src alpha, dest func = 1-src alpha
       *
       * because of the restrictions imposed in check state. We can
       * only support (2) when the destination format has no alpha channel.
       * This is because the hardware always calculates the destination alpha
       * as Asrc + Adst*(1-Asrc), which is correct for (1) but for (2) we
       * would require Asrc*Asrc + Adst*(1-Asrc).
       */
      pBltData->ulFlags    |= (pDev->srcBlendFunc == DSBF_SRCALPHA) ? BLT_OP_FLAGS_BLEND_SRC_ALPHA : BLT_OP_FLAGS_BLEND_SRC_ALPHA_PREMULT;
      pBltData->ulFlags    |= BLT_OP_FLAGS_BLEND_DST_MEMORY;
      pBltData->srcFormat   = SURF_ARGB8888;
    }
  }
  else
  {
    if(pDev->drawingFlags & DSDRAW_XOR)
      pBltData->ulFlags |= BLT_OP_FLAGS_XOR;

    if(pBltData->dstFormat == SURF_YCBCR422R
       || pBltData->dstFormat == SURF_ACRYCB8888
       || pBltData->dstFormat == SURF_CRYCB888)
    {
      /*
       * For 422R we let the hardware do the conversion, as direct fast fill
       * in 422R has problems (probably due to two pixels being mixed up in a
       * 32bit word).
       */
      pBltData->srcFormat = SURF_ARGB8888;
    }
    else
    {
      /*
       * Simple fast fill, the colour is already in the destination colour format
       */
      pBltData->srcFormat = pBltData->dstFormat;
    }
  }

  if (ioctl(pDrv->fd, STMFBIO_BLT, pBltData ) < 0)
    return false;

  return true;
}

/*****************************************************************/
/* Fill in the command data that is common to all blit functions */
/*****************************************************************/
static bool stgfxCommonBlitData
(
  STGFXDriverData*  pDrv,
  STGFXDeviceData  *pDev,
  STMFBIO_BLT_DATA *pBltData
)
{
  bool set_premultcolor = true;
  pBltData->operation = BLT_OP_COPY;

  if (pDev->blittingFlags & DSBLIT_SRC_COLORKEY)
  {
    pBltData->ulFlags    |= BLT_OP_FLAGS_SRC_COLOR_KEY;
    pBltData->colourKey   = pDev->srcColourKey;
  }

  if (pDev->blittingFlags & DSBLIT_DST_COLORKEY)
  {
    pBltData->ulFlags   |= BLT_OP_FLAGS_DST_COLOR_KEY;
    pBltData->colourKey  = pDev->dstColourKey;
  }

  pBltData->srcMemBase = pDev->srcMemBase;
  pBltData->srcOffset  = pDev->srcOffset;
  pBltData->srcPitch   = pDev->srcPitch;

  pBltData->dstMemBase = pDev->dstMemBase;
  pBltData->dstOffset  = pDev->dstOffset;
  pBltData->dstPitch   = pDev->dstPitch;

  pBltData->srcFormat = pDev->srcFormat;
  pBltData->dstFormat = pDev->dstFormat;

  pBltData->colour      = pDev->blitColour;
  pBltData->globalAlpha = pDev->globalAlpha;

  if (pDev->bBlendFromColorAlpha & !pDev->bBlendFromSrcAlpha)
  {
    if(pDev->srcBlendFunc == DSBF_ZERO && pDev->dstBlendFunc == DSBF_ZERO)
    {
      /* This is a clear, just fill with 0 */
      pBltData->operation   = BLT_OP_FILL;
      pBltData->colour      = 0;
      /*
       * Change the source format to match the dest so we get a fast fill,
       * the other source surface parameters are ignored.
       */
      pBltData->srcFormat = pBltData->dstFormat;
      set_premultcolor = false;
    }
    else if(pDev->srcBlendFunc == DSBF_ZERO && pDev->dstBlendFunc == DSBF_ONE)
    {
      /*
       * This leaves the destination as is, i.e. a nop, because we do not
       * support destination pre-multiplication.
       */
      return false;
    }
    else if(pDev->srcBlendFunc == DSBF_ONE && pDev->dstBlendFunc == DSBF_ZERO)
    {
      /*
       * This is a copy (blend with a destination of (0,0,0,0)) after
       * premultiplication with the colour alpha channel.
       */
      pBltData->ulFlags |= BLT_OP_FLAGS_GLOBAL_ALPHA | BLT_OP_FLAGS_BLEND_SRC_ALPHA | BLT_OP_FLAGS_BLEND_DST_ZERO;

      if(pDev->bPremultColor && !pDev->bSrcPremultiply)
        set_premultcolor = false;
    }
    else
    {
      /*
       * Both of the real support blend combinations end up being the same
       * operation.
       */
      pBltData->ulFlags |= BLT_OP_FLAGS_GLOBAL_ALPHA | BLT_OP_FLAGS_BLEND_SRC_ALPHA | BLT_OP_FLAGS_BLEND_DST_MEMORY;

      if(pDev->bPremultColor && !pDev->bSrcPremultiply && (pDev->srcBlendFunc != DSBF_SRCALPHA))
        set_premultcolor = false;
    }
  }
  else if (pDev->bBlendFromSrcAlpha)
  {
    if(pDev->srcBlendFunc == DSBF_ZERO && pDev->dstBlendFunc == DSBF_ZERO)
    {
      /* This is a clear, just fill with 0 */
      pBltData->operation   = BLT_OP_FILL;
      pBltData->colour      = 0;
      /*
       * Change the source format to match the dest so we get a fast fill,
       * the other source surface parameters are ignored.
       */
      pBltData->srcFormat = pBltData->dstFormat;
      set_premultcolor = false;
    }
    else if(pDev->srcBlendFunc == DSBF_ONE && pDev->dstBlendFunc == DSBF_ZERO)
    {
      pBltData->ulFlags |= BLT_OP_FLAGS_BLEND_DST_ZERO;
      pBltData->ulFlags |= pDev->bSrcPremultiply ? BLT_OP_FLAGS_BLEND_SRC_ALPHA : BLT_OP_FLAGS_BLEND_SRC_ALPHA_PREMULT;
      if(pDev->bBlendFromColorAlpha)
      {
        pBltData->ulFlags |= BLT_OP_FLAGS_GLOBAL_ALPHA;
        if(pDev->bPremultColor && !pDev->bSrcPremultiply)
          set_premultcolor = false;
      }
    }
    else if(pDev->srcBlendFunc == DSBF_ZERO && pDev->dstBlendFunc == DSBF_ONE)
    {
      /*
       * This leaves the destination as is, i.e. a nop, because we do not
       * support destination pre-multiplication.
       */
      return false;
    }
    else if(pDev->srcBlendFunc == DSBF_ONE && pDev->dstBlendFunc == DSBF_INVSRCALPHA)
    {
      /*
       * The flags look the wrong way about, but they specifiy if the source
       * is _already_ pre-multiplied or not rather than the operation required.
       */
      pBltData->ulFlags |= BLT_OP_FLAGS_BLEND_DST_MEMORY;
      pBltData->ulFlags |= pDev->bSrcPremultiply ? BLT_OP_FLAGS_BLEND_SRC_ALPHA : BLT_OP_FLAGS_BLEND_SRC_ALPHA_PREMULT;
      if(pDev->bBlendFromColorAlpha)
      {
        pBltData->ulFlags |= BLT_OP_FLAGS_GLOBAL_ALPHA;
        if(pDev->bPremultColor && !pDev->bSrcPremultiply)
          set_premultcolor = false;
      }
    }
    else
    {
      /*
       * Must be srcBlendFunc == DSBF_SRCALPHA, dstBlendFunc == DSBF_INVSRCALPHA,
       * premultiply off, dest no alpha channel. Note that we use the blend flag
       * that normally enables pre-multiplication to effectively do the DSBF_SRCALPHA
       * blend maths.
       */
      pBltData->ulFlags |= BLT_OP_FLAGS_BLEND_DST_MEMORY;
      pBltData->ulFlags |= BLT_OP_FLAGS_BLEND_SRC_ALPHA;
      if(pDev->bBlendFromColorAlpha)
      {
        pBltData->ulFlags |= BLT_OP_FLAGS_GLOBAL_ALPHA;
      }
    }
  }
  else
  {
    if(pDev->blittingFlags & DSBLIT_XOR)
    {
      pBltData->ulFlags |= BLT_OP_FLAGS_XOR;
    }
    else if(pDev->blittingFlags & DSBLIT_SRC_PREMULTIPLY)
    {
      /*
       * This is a copy (blend with a destination of (0,0,0,0)) after
       * premultiplication with the colour alpha channel.
       */
      pBltData->ulFlags |= (BLT_OP_FLAGS_BLEND_SRC_ALPHA | BLT_OP_FLAGS_BLEND_DST_ZERO);
    }
  }

  if(pDev->blittingFlags & DSBLIT_COLORIZE)
    pBltData->ulFlags |= BLT_OP_FLAGS_COLORIZE;

  /*
   * If DSBLIT_SRC_PREMULTCOLOR is set, but this hasn't been done as part of an
   * alpha blend operation already, do it explicitly using the
   * input matrix transformation on hardware that has this available.
   */
  if(set_premultcolor && (pDev->blittingFlags & DSBLIT_SRC_PREMULTCOLOR))
    pBltData->ulFlags |= BLT_OP_FLAGS_PREMULT_COLOUR_ALPHA;

  return true;
}

/*********************************************************************/
/* Update palette and enable CLUT blit operation if required         */
/*********************************************************************/
static bool stgfxUpdatePalette
(
  STGFXDriverData*  pDrv,
  STGFXDeviceData  *pDev,
  STMFBIO_BLT_DATA *pBltData
)
{
  if(pDev->palette && (pDev->blittingFlags != DSBLIT_INDEX_TRANSLATION))
  {
    if(pDev->bNeedCLUTReload)
    {
      STMFBIO_PALETTE lut;
      int i;
      bzero(&lut, sizeof(STMFBIO_PALETTE));

      lut.numEntries = pDev->palette->num_entries;

      for(i=0; i < lut.numEntries; i++)
      {
        DFBColor *color = &pDev->palette->entries[i];
        /* Note that the alpha range for the lut is 0-128 (note not 127!!) */
        lut.entries[i] = PIXEL_ARGB((color->a+1)/2, color->r, color->g, color->b);
      }

      if (ioctl(pDrv->fd, STMFBIO_SET_BLITTER_PALETTE, &lut ) < 0 )
        return false;
    }

    pBltData->ulFlags |= BLT_OP_FLAGS_CLUT_ENABLE;
  }

  return true;
}


/*********************************************************************/
/* Store the destination surface state in the device state structure */
/*********************************************************************/
static void stgfxSetDestination
(
  STGFXDeviceData*  pDev,
  CardState*        pState
)
{
  int i;

  pDev->dstFormat = stgfxSetPixelFormat (pState->dst.buffer->format);

  pDev->dstPitch  = pState->dst.pitch;
  pDev->dstOffset = pState->dst.offset;
  pDev->dstPhys   = pState->dst.phys;

  pDev->dstMemBase = STMFBGP_FRAMEBUFFER;
  for (i = 0; i < D_ARRAY_SIZE (pDev->aux_pools); ++i)
    if (pDev->aux_pools[i] == pState->dst.allocation->pool)
      {
        pDev->dstMemBase = STMFBGP_GFX0 + i;
        break;
      }
}


/**************************************************************/
/* Set the source surface state in the device state structure */
/**************************************************************/
static void stgfxSetSource
(
  STGFXDriverData*  pDrv,
  STGFXDeviceData*  pDev,
  CardState*        pState
)
{
  const CoreSurface           * const srcSurf = pState->source;
  const CoreSurfaceBufferLock * const lock = &pState->src;
  const CoreSurfaceBuffer     * const srcSurfBuf = lock->buffer;
  int i;

  if (!srcSurfBuf)
  {
    pDev->srcFormat = SURF_NULL_PAD;
    pDev->srcOffset = 0;
    pDev->srcPitch  = 0;
    pDev->srcPhys   = 0;
    return;
  }

  switch (srcSurfBuf->format)
    {
    case DSPF_LUT2:
    case DSPF_LUT8:
    case DSPF_ALUT44:
      pDev->palette = srcSurf->palette;
      pDev->bNeedCLUTReload = true;
      break;

    default:
      pDev->palette = NULL;
      break;
    }

  pDev->srcFormat = stgfxSetPixelFormat (srcSurfBuf->format);

  pDev->srcPitch  = lock->pitch;
  pDev->srcOffset = lock->offset;
  pDev->srcPhys   = lock->phys;

  pDev->srcMemBase = STMFBGP_FRAMEBUFFER;
  for (i = 0; i < D_ARRAY_SIZE (pDev->aux_pools); ++i)
    if (pDev->aux_pools[i] == lock->allocation->pool)
      {
        pDev->srcMemBase = STMFBGP_GFX0 + i;
        break;
      }
}

/******************************************************************************/
/* Create a single int colour from the colour structure received from directFB*/
/******************************************************************************/
static void stgfxSetColour
(
  STGFXDeviceData*  pDev,
  CardState*        pState
)
{
unsigned int alpha  = (unsigned int)pState->color.a;
unsigned int red    = (unsigned int)pState->color.r;
unsigned int green  = (unsigned int)pState->color.g;
unsigned int blue   = (unsigned int)pState->color.b;

  pDev->blitColour = PIXEL_ARGB(alpha, red, green, blue);
  /*
   * Store the alpha for blended blits of non-alpha surfaces
   */
  pDev->globalAlpha = alpha;

  if(pDev->drawingFlags & DSDRAW_SRC_PREMULTIPLY)
  {
    /*
     * We do source alpha pre-multiplication now, rather in the hardware so
     * we can support it regardless of the blend mode.
     */
    red   = (red  *(alpha+1)) >> 8;
    green = (green*(alpha+1)) >> 8;
    blue  = (blue *(alpha+1)) >> 8;
  }

  if (pDev->drawingFlags & DSDRAW_BLEND)
  {
    /*
     * For blended fills we use ARGB8888, to get full alpha precision in the
     * blend, the hardware does color conversion.
     */
    pDev->drawColour = PIXEL_ARGB(alpha, red, green, blue);
    return;
  }

  switch (pState->dst.buffer->format)
  {
  case DSPF_ARGB1555:
    pDev->drawColour = PIXEL_ARGB1555(alpha, red, green, blue);
    break;

  case DSPF_ARGB4444:
    pDev->drawColour = PIXEL_ARGB4444(alpha, red, green, blue);
    break;

  case DSPF_RGB16:
    pDev->drawColour = PIXEL_RGB16(red, green, blue);
    break;

  case DSPF_RGB24:
    pDev->drawColour = PIXEL_RGB32(red, green, blue);
    break;

  case DSPF_ARGB8565:
    pDev->drawColour = PIXEL_ARGB8565(alpha, red, green, blue);
    break;

  case DSPF_UYVY:
  case DSPF_AVYU:
  case DSPF_VYU:
    /*
     * Note we get the hardware to do the conversion from RGB to YUV
     */
  case DSPF_ARGB:
    pDev->drawColour = PIXEL_ARGB(alpha, red, green, blue);
    break;

  case DSPF_LUT2:
  case DSPF_LUT8:
    pDev->drawColour = pState->color_index;
    break;

  case DSPF_ALUT44:
    pDev->drawColour = (alpha & 0xF0) | (pState->color_index & 0x0F);
    break;

  case DSPF_A8:
    pDev->drawColour = alpha;
    break;

  default:
    D_ERROR("DirectFB/gfxdrivers/stgfx: Unexpected colour format\n");
    return;
  }
}

/*********************************************/
/* Check that all graphics ops have completed*/
/*********************************************/
static DFBResult stgfxEngineSync
(
  void* drv,
  void* dev
)
{
  STGFXDriverData* pDrv = (STGFXDriverData*)drv;
  if (ioctl(pDrv->fd, STMFBIO_SYNC_BLITTER, 0 ) < 0)
  {
    D_PERROR("DirectFB/gfxdrivers/stgfx: Sync Engine failed\n");
    D_ERROR("DirectFB/gfxdrivers/stgfx: file descriptor = %d\n",pDrv->fd);
  }

  return DFB_OK;
}

/**************************************************************************/
/* Check that the current state is ok for the operation indicated by accel*/
/**************************************************************************/
static void stgfxCheckState
(
  void*               drv,
  void*               dev,
  CardState*          state,
  DFBAccelerationMask accel
)
{
bool supportedBlend = false;
bool srcHasAlpha    = false;
bool dstHasAlpha    = false;
bool srcIsClut      = false;
bool dstIsClut      = false;
bool canUseHWInputMatrix;


  state->accel = 0;

  /* Flag the blending maths we can support */
  if (state->src_blend == DSBF_ZERO && state->dst_blend == DSBF_ZERO) /* Clear */
    supportedBlend = true;

  if (state->src_blend == DSBF_ONE && state->dst_blend == DSBF_ZERO) /* Copy */
    supportedBlend = true;

  if (state->src_blend == DSBF_ZERO && state->dst_blend == DSBF_ONE) /* Nop */
    supportedBlend = true;

  if (state->src_blend == DSBF_ONE && state->dst_blend == DSBF_INVSRCALPHA)
    supportedBlend = true;

  if (state->src_blend == DSBF_SRCALPHA && state->dst_blend == DSBF_INVSRCALPHA)
    supportedBlend = true;

  if(!stgfxCheckPixelFormat(state->destination->config.format, false, &dstHasAlpha, &dstIsClut))
    return; /* unsupported destination format */

  /* Check there are no other drawing flags than those that are supported */
  if (!(state->drawingflags & ~STGFX_VALID_DRAWINGFLAGS))
  {
    if (state->drawingflags & DSDRAW_BLEND)
    {
      /*
       * Cannot support blend and xor at the same time
       */
      if (state->drawingflags & DSDRAW_XOR)
        goto blit_acceleration;

      if(!supportedBlend || dstIsClut)
      {
        /*
         * Don't allow blends to LUT surfaces, but we do allow fast fills in
         * order to be able to do a "clear" to a specific index.
         */
        goto blit_acceleration;
      }

      if (dstHasAlpha && (state->src_blend == DSBF_SRCALPHA && state->dst_blend == DSBF_INVSRCALPHA))
      {
        /*
         * src func = src alpha, dst func = 1-src alpha can only be accelerated
         * when the destination does not have an alpha channel, becuase the
         * correct calculation of the destination alpha cannot be acheived in
         * the hardware. We go to the trouble of supporting it as the mode is
         * used in the blended fill df_dok performance test.
         */
        goto blit_acceleration;

      }
    }

    state->accel |= STGFX_VALID_DRAWINGFUNCTIONS;
  }


blit_acceleration:

  if (!(state->blittingflags & ~STGFX_VALID_BLITTINGFLAGS))
  {
    if(!state->source)
      goto exit; /* Cannot blit if there is no source surface */

    canUseHWInputMatrix = (stgfx_accel_type == FB_ACCEL_ST_BDISP
                           && ((state->source->config.format != DSPF_UYVY
                                && state->destination->config.format != DSPF_UYVY)
                               || state->source->config.format == state->destination->config.format)
                           && ((state->source->config.format != DSPF_AVYU
                                && state->destination->config.format != DSPF_AVYU)
                               || state->source->config.format == state->destination->config.format)
                           && ((state->source->config.format != DSPF_VYU
                                && state->destination->config.format != DSPF_VYU)
                               || state->source->config.format == state->destination->config.format)
                           );

    if((state->blittingflags & DSBLIT_COLORIZE) && !canUseHWInputMatrix)
      goto exit;

    /*
     * Deal with a special case of LUT->LUT blits that do not go through
     * a colour lookup, but instead do an index translation mapping. Note that
     * we do not actually support anything other than a 1-1 mapping of the CLUT.
     */
    if (dstIsClut)
    {
      if( state->blittingflags == DSBLIT_INDEX_TRANSLATION &&
          state->source && (state->source->config.format == state->destination->config.format) )
      {
        state->accel = DFXL_BLIT;
      }
      /*
       * Note that blits are not supported to LUT destinations other than in
       * the index translation case.
       */
      goto exit;
    }

    if((state->blittingflags & (DSBLIT_SRC_COLORKEY | DSBLIT_DST_COLORKEY)) == (DSBLIT_SRC_COLORKEY | DSBLIT_DST_COLORKEY))
      goto exit; /* Cannot accelerate simultaneous source and destination colour keying */

    if(!stgfxCheckPixelFormat(state->source->config.format, true, &srcHasAlpha, &srcIsClut))
      goto exit; /* unsupported source pixel type */

    if(state->blittingflags & DSBLIT_BLEND_ALPHACHANNEL)
    {
      if(!supportedBlend)
        goto exit;

      /*
       * We cannot blend and XOR at the same time, what a silly idea
       */
      if(state->blittingflags & DSBLIT_XOR)
        goto exit;

      /*
       * We cannot blend with a source of DSBF_RGB32 because the hardware cannot
       * be made to ignore the top 8 bits and will use them as an alpha channel
       */
      if(state->source->config.format == DSPF_RGB32)
        goto exit;

      if(state->blittingflags & DSBLIT_BLEND_COLORALPHA)
      {
        /*
         * If COLORALPHA is also specified then the constant alpha is multiplied
         * with the src alpha, but not with the RGB components unless
         * src premultiply, premultcolor or src blend DSBF_SRCALPHA is set.
         * Unfortunately the hardware always multiplies RGB by the global alpha
         * register.
         */
        if(!(state->blittingflags & (DSBLIT_SRC_PREMULTIPLY | DSBLIT_SRC_PREMULTCOLOR)) && (state->src_blend != DSBF_SRCALPHA))
          goto exit;


        if((state->blittingflags & DSBLIT_SRC_PREMULTCOLOR) && !canUseHWInputMatrix)
        {
          /*
           * We can't do more than one multiply of alpha with color except on
           * BDisp based parts when the source is RGB or expanded Clut data. In
           * that case we can use the input matrix to do the premultcolor.
           */
          if((state->blittingflags & DSBLIT_SRC_PREMULTIPLY) || (state->src_blend == DSBF_SRCALPHA))
            goto exit;
        }

      }
      else if((state->blittingflags & DSBLIT_SRC_PREMULTCOLOR) && !canUseHWInputMatrix)
      {
        /*
         * Premultcolor without coloralpha needs the BDisp input matrix
         */
        goto exit;
      }


      /*
       * We can do blend from alpha channel on all surface formats as on formats
       * without alpha the hardware fills in "full" alpha for us. However we
       * can only do source premultiplication if the src func != DSBF_SRCALPHA
       */
      if((state->blittingflags & DSBLIT_SRC_PREMULTIPLY) && (state->src_blend == DSBF_SRCALPHA))
          goto exit;

      /*
       * We can do src func = DSBF_SRCALPHA as long as there is no incompatible
       * pre-multiply (src alpha or color alpha, which has already been tested
       * for) and the destination does not have an alpha channel.
       *
       * This is becuase the hardware cannot calculate the correct alpha for
       * the destination but can calculate the correct RGB values. This is
       * supported to accelerate the blit-blend performance test in df_dok
       * for the standard 16bit destination.
       */
      if(dstHasAlpha && (state->src_blend == DSBF_SRCALPHA))
          goto exit;

    }
    else if(state->blittingflags & DSBLIT_BLEND_COLORALPHA)
    {
      /*
       * color alpha on its own (i.e. no DSBLIT_BLEND_ALPHACHANNEL). Supported
       * blend modes + flags:
       *
       * (1) src func = 0, dst func = 0 (clear)
       * (2) src func = 0, dst func = 1 (nop)
       * (3) src func = 1, dst func = 0 (copy), any dest surface, premultiply and/or premultcolor on
       * (4) src func = 1, dst func = 1-alpha, any dest surface, premultiply and/or premultcolor on
       * (5) src func = src alpha, dst func = 1-src alpha, no premultiply, non alpha dest surface
       *
       * The source must be an RGB surface with no existing alpha channel in
       * the last three cases. Note that this doesn't include DSPF_RGB32 as the
       * hardware will insist on using the top unused 8 bits as an alpha channel.
       * We have no way of forcing the alpha channel to be fully opqaue.
       *
       * The reason for this is that COLORALPHA on its own replaces the src alpha
       * channel with the alpha value from the colour register. Unfortunately the
       * hardware cannot do this, it always multiplies the source alpha with
       * its global alpha register. So we can only accelerate this when the
       * source alpha is always 1.0f, i.e. surface formats that do not contain
       * an alpha channel, or for those obscure blend modes where the src is
       * discarded.
       *
       */
      if(!supportedBlend)
        goto exit;

      if(state->blittingflags & DSBLIT_XOR)
        goto exit;

      if(state->src_blend == DSBF_ONE)
      {
        if(srcHasAlpha || !(state->blittingflags & (DSBLIT_SRC_PREMULTIPLY | DSBLIT_SRC_PREMULTCOLOR)))
          goto exit;

      }
      else if(state->src_blend == DSBF_SRCALPHA && state->dst_blend == DSBF_INVSRCALPHA)
      {
        if(srcHasAlpha || dstHasAlpha || (state->blittingflags & DSBLIT_SRC_PREMULTIPLY))
          goto exit;
      }

      if((state->blittingflags & DSBLIT_SRC_PREMULTCOLOR) && !canUseHWInputMatrix)
      {
        /*
         * We can't do more than one multiply of alpha with color except on
         * BDisp based parts when the source is RGB or expanded Clut data. In
         * that case we can use the input matrix to do the premultcolor.
         */
        if((state->blittingflags & DSBLIT_SRC_PREMULTIPLY) || (state->src_blend != DSBF_SRCALPHA))
          goto exit;
      }

    }
    else
    {
      /*
       * We can only format convert DSBF_RGB32 to other formats that do not
       * have an alpha channel. We allow this to accelerate the df_dok
       * performance test blit-convert. But these limitations make this
       * surface format pretty useless.
       */
      if(state->source->config.format == DSPF_RGB32)
      {
        if(dstHasAlpha || (state->blittingflags & DSBLIT_SRC_PREMULTIPLY))
          goto exit;
      }

      /*
       * We cannot XOR and premultiply by src alpha at the same time
       */
      if((state->blittingflags & (DSBLIT_XOR|DSBLIT_SRC_PREMULTIPLY)) == (DSBLIT_XOR|DSBLIT_SRC_PREMULTIPLY))
        goto exit;

      /*
       * In this case we need the input matrix on the BDisp to do premultcolor.
       */
      if((state->blittingflags & DSBLIT_SRC_PREMULTCOLOR) && !canUseHWInputMatrix)
      {
        goto exit;
      }
    }

    state->accel |= STGFX_VALID_BLITTINGFUNCTIONS;
  }

exit:
/*  printf("check state - state->accel = 0x%x\n",state->accel);*/
  return;
}

typedef struct  _PIX_FMT_TBL {
    unsigned long  stgBlitFmt;
    const char    *string;
    bool           supportedAsSrc;
    bool           supportedAsDst;
}PIX_FMT_TBL;


static const PIX_FMT_TBL format_tbl_bdisp[DFB_NUM_PIXELFORMATS] = {
    /*
     * Note: we specify that LUT formats have an alpha channel, because as a source
     * it does once the colour lookup has taken place; hence any blending
     * restrictions will apply.
     *
     * Note2: CLUT2 is not supported as a target type on BDISP at the moment.
     */
    [DFB_PIXELFORMAT_INDEX (DSPF_ARGB1555)] = { SURF_ARGB1555,  "SURF_ARGB1555 16bpp alp",        true,  true },
    [DFB_PIXELFORMAT_INDEX (DSPF_RGB16)]    = { SURF_RGB565,    "SURF_RGB565 16bpp",              true,  true },
    [DFB_PIXELFORMAT_INDEX (DSPF_RGB24)]    = { SURF_RGB888,    "SURF_RGB888 24 bpp - packed",    true,  true  },
    [DFB_PIXELFORMAT_INDEX (DSPF_RGB32)]    = { SURF_ARGB8888,  "SURF_ARGB8888 32 bpp - no alpha",true,  false },
    [DFB_PIXELFORMAT_INDEX (DSPF_ARGB)]     = { SURF_ARGB8888,  "SURF_ARGB8888 32bpp alp",        true,  true  },
    [DFB_PIXELFORMAT_INDEX (DSPF_A8)]       = { SURF_A8,        "SURF_A8",                        true,  true  },
    [DFB_PIXELFORMAT_INDEX (DSPF_YUY2)]     = {},
    [DFB_PIXELFORMAT_INDEX (DSPF_RGB332)]   = {},
    [DFB_PIXELFORMAT_INDEX (DSPF_UYVY)]     = { SURF_YCBCR422R, "SURF_YCBCR422R",                 true,  true  },
    [DFB_PIXELFORMAT_INDEX (DSPF_I420)]     = {},
    [DFB_PIXELFORMAT_INDEX (DSPF_YV12)]     = {},
    [DFB_PIXELFORMAT_INDEX (DSPF_LUT8)]     = { SURF_CLUT8,     "SURF_CLUT8",                     true,  true  },
    [DFB_PIXELFORMAT_INDEX (DSPF_ALUT44)]   = { SURF_ACLUT44,   "SURF_ACLUT44",                   true,  true  },
    [DFB_PIXELFORMAT_INDEX (DSPF_AiRGB)]    = {},
    [DFB_PIXELFORMAT_INDEX (DSPF_A1)]       = { SURF_A1,        "SURF_A1",                        true,  false },
    [DFB_PIXELFORMAT_INDEX (DSPF_NV12)]     = {},
    [DFB_PIXELFORMAT_INDEX (DSPF_NV16)]     = {},
    [DFB_PIXELFORMAT_INDEX (DSPF_ARGB2554)] = {},
    [DFB_PIXELFORMAT_INDEX (DSPF_ARGB4444)] = { SURF_ARGB4444,  "SURF_ARGB4444 16bpp",            true,  true  },
    [DFB_PIXELFORMAT_INDEX (DSPF_RGBA4444)] = {},
    [DFB_PIXELFORMAT_INDEX (DSPF_NV21)]     = {},
    [DFB_PIXELFORMAT_INDEX (DSPF_AYUV)]     = {},
    [DFB_PIXELFORMAT_INDEX (DSPF_A4)]       = {},
    [DFB_PIXELFORMAT_INDEX (DSPF_ARGB1666)] = {},
    [DFB_PIXELFORMAT_INDEX (DSPF_ARGB6666)] = {},
    [DFB_PIXELFORMAT_INDEX (DSPF_RGB18)]    = {},
    [DFB_PIXELFORMAT_INDEX (DSPF_LUT2)]     = { SURF_CLUT2,     "SURF_CLUT2",                     true,  false },
    [DFB_PIXELFORMAT_INDEX (DSPF_RGB444)]   = {},
    [DFB_PIXELFORMAT_INDEX (DSPF_RGB555)]   = {},
    [DFB_PIXELFORMAT_INDEX (DSPF_BGR555)]   = {},
    [DFB_PIXELFORMAT_INDEX (DSPF_RGBA5551)] = {},
    [DFB_PIXELFORMAT_INDEX (DSPF_YUV444P)]  = {},
    [DFB_PIXELFORMAT_INDEX (DSPF_ARGB8565)] = { SURF_ARGB8565,  "SURF_ARGB8565 24bpp alp",        true,  true  },
    [DFB_PIXELFORMAT_INDEX (DSPF_AVYU)]     = { SURF_ACRYCB8888,"SURF_ACRYCB8888 32bpp YUV alp",  true,  true  },
    [DFB_PIXELFORMAT_INDEX (DSPF_VYU)]      = { SURF_CRYCB888,  "SURF_CRYCB888 24bpp YUV",        true,  true  }
};


static const PIX_FMT_TBL format_tbl_gamma[DFB_NUM_PIXELFORMATS] = {
    [DFB_PIXELFORMAT_INDEX (DSPF_ARGB1555)] = { SURF_ARGB1555,  "SURF_ARGB1555 16bpp alp",        true,  true  },
    [DFB_PIXELFORMAT_INDEX (DSPF_RGB16)]    = { SURF_RGB565,    "SURF_RGB565 16bpp",              true,  true  },
    [DFB_PIXELFORMAT_INDEX (DSPF_RGB24)]    = { SURF_RGB888,    "SURF_RGB888 24 bpp - packed",    true,  true  },
    [DFB_PIXELFORMAT_INDEX (DSPF_RGB32)]    = { SURF_ARGB8888,  "SURF_ARGB8888 32 bpp - no alpha",true,  false },
    [DFB_PIXELFORMAT_INDEX (DSPF_ARGB)]     = { SURF_ARGB8888,  "SURF_ARGB8888 32bpp alp",        true,  true  },
    [DFB_PIXELFORMAT_INDEX (DSPF_A8)]       = {},
    [DFB_PIXELFORMAT_INDEX (DSPF_YUY2)]     = {},
    [DFB_PIXELFORMAT_INDEX (DSPF_RGB332)]   = {},
    [DFB_PIXELFORMAT_INDEX (DSPF_UYVY)]     = { SURF_YCBCR422R, "SURF_YCBCR422R",                 true,  true  },
    [DFB_PIXELFORMAT_INDEX (DSPF_I420)]     = {},
    [DFB_PIXELFORMAT_INDEX (DSPF_YV12)]     = {},
    [DFB_PIXELFORMAT_INDEX (DSPF_LUT8)]     = { SURF_CLUT8,     "SURF_CLUT8",                     true,  true  },
    [DFB_PIXELFORMAT_INDEX (DSPF_ALUT44)]   = { SURF_ACLUT44,   "SURF_ACLUT44",                   true,  true  },
    [DFB_PIXELFORMAT_INDEX (DSPF_AiRGB)]    = {},
    [DFB_PIXELFORMAT_INDEX (DSPF_A1)]       = {},
    [DFB_PIXELFORMAT_INDEX (DSPF_NV12)]     = {},
    [DFB_PIXELFORMAT_INDEX (DSPF_NV16)]     = {},
    [DFB_PIXELFORMAT_INDEX (DSPF_ARGB2554)] = {},
    [DFB_PIXELFORMAT_INDEX (DSPF_ARGB4444)] = { SURF_ARGB4444,  "SURF_ARGB4444 16bpp",            true,  true  },
    [DFB_PIXELFORMAT_INDEX (DSPF_RGBA4444)] = {},
    [DFB_PIXELFORMAT_INDEX (DSPF_NV21)]     = {},
    [DFB_PIXELFORMAT_INDEX (DSPF_AYUV)]     = {},
    [DFB_PIXELFORMAT_INDEX (DSPF_A4)]       = {},
    [DFB_PIXELFORMAT_INDEX (DSPF_ARGB1666)] = {},
    [DFB_PIXELFORMAT_INDEX (DSPF_ARGB6666)] = {},
    [DFB_PIXELFORMAT_INDEX (DSPF_RGB18)]    = {},
    [DFB_PIXELFORMAT_INDEX (DSPF_LUT2)]     = { SURF_CLUT2,     "SURF_CLUT2",                     true,  true  },
    [DFB_PIXELFORMAT_INDEX (DSPF_RGB444)]   = {},
    [DFB_PIXELFORMAT_INDEX (DSPF_RGB555)]   = {},
    [DFB_PIXELFORMAT_INDEX (DSPF_BGR555)]   = {},
    [DFB_PIXELFORMAT_INDEX (DSPF_RGBA5551)] = {},
    [DFB_PIXELFORMAT_INDEX (DSPF_YUV444P)]  = {},
    [DFB_PIXELFORMAT_INDEX (DSPF_ARGB8565)] = { SURF_ARGB8565,  "SURF_ARGB8565 24bpp alp",        true,  true  },
    [DFB_PIXELFORMAT_INDEX (DSPF_AVYU)]     = { SURF_ACRYCB8888,"SURF_ACRYCB8888 32bpp YUV alp",  true,  true  },
    [DFB_PIXELFORMAT_INDEX (DSPF_VYU)]      = { SURF_CRYCB888,  "SURF_CRYCB888 24bpp YUV",        true,  true  }
};


static const PIX_FMT_TBL *format_tbl;


static bool stgfxCheckPixelFormat(DFBSurfacePixelFormat dfbFmt,
                                  bool bSrc, bool *hasAlpha, bool *isClut)
{
  D_ASSERT (dfbFmt != DSPF_UNKNOWN);
  if (!dfbFmt)
    return false;

  const PIX_FMT_TBL * const format = &format_tbl[DFB_PIXELFORMAT_INDEX (dfbFmt)];

  *hasAlpha = DFB_PIXELFORMAT_HAS_ALPHA (dfbFmt);
  *isClut   = DFB_PIXELFORMAT_IS_INDEXED (dfbFmt);

  return bSrc ? format->supportedAsSrc : format->supportedAsDst;
}


static unsigned long stgfxSetPixelFormat(DFBSurfacePixelFormat dfbFmt)
{
  D_ASSERT (dfbFmt != DSPF_UNKNOWN);
  if (!dfbFmt)
    return 0;

  return format_tbl[DFB_PIXELFORMAT_INDEX (dfbFmt)].stgBlitFmt;
}


static unsigned long stgfxGetPixelFormat(SURF_FMT fmt)
{
  switch (fmt)
    {
    case SURF_ARGB1555:   return DSPF_ARGB1555;
    case SURF_RGB565  :   return DSPF_RGB16;
    case SURF_RGB888  :   return DSPF_RGB24;
    case SURF_ARGB8888:   return DSPF_ARGB;
    case SURF_A8:         return DSPF_A8;
    case SURF_YCBCR422R:  return DSPF_UYVY;
    case SURF_CLUT8:      return DSPF_LUT8;
    case SURF_ACLUT44:    return DSPF_ALUT44;
    case SURF_A1:         return DSPF_A1;
    case SURF_ARGB4444:   return DSPF_ARGB4444;
    case SURF_CLUT2:      return DSPF_LUT2;
    case SURF_ARGB8565:   return DSPF_ARGB8565;
    case SURF_ACRYCB8888: return DSPF_AVYU;
    case SURF_CRYCB888:   return DSPF_VYU;
    default:
      break;
    }
  return DSPF_UNKNOWN;
}


/**********************************************************************/
/* DirectFB has requested a modified state.                           */
/* Check which fields have been modified and then update the fields   */
/* that are necessary for the operation indicated by accel            */
/**********************************************************************/
static void stgfxSetState
(
  void*                 drv,
  void*                 dev,
  GraphicsDeviceFuncs*  funcs,
  CardState*            state,
  DFBAccelerationMask   accel
)
{
  STGFXDriverData *pDrv = (STGFXDriverData*) drv;
  STGFXDeviceData *pDev = (STGFXDeviceData*) dev;

  /*
   * First update the flags, as the rest depends on these being
   * correct.
   */
  if (state->mod_hw & SMF_DRAWING_FLAGS)
    pDev->drawingFlags = state->drawingflags;

  if (state->mod_hw & SMF_BLITTING_FLAGS)
  {
    pDev->blittingFlags        =  state->blittingflags;
    pDev->bBlendFromColorAlpha = (state->blittingflags & DSBLIT_BLEND_COLORALPHA)   ? true : false;
    pDev->bBlendFromSrcAlpha   = (state->blittingflags & DSBLIT_BLEND_ALPHACHANNEL) ? true : false;
    pDev->bSrcPremultiply      = (state->blittingflags & DSBLIT_SRC_PREMULTIPLY)    ? true : false;
    pDev->bPremultColor        = (state->blittingflags & DSBLIT_SRC_PREMULTCOLOR)   ? true : false;
  }

  if (state->mod_hw & SMF_DESTINATION)
    stgfxSetDestination(pDev, state);

  if (state->mod_hw & SMF_SOURCE)
    stgfxSetSource(pDrv, pDev, state);

  /*
   * Make sure that the colour gets updated when the drawing or blitting flags
   * or the destination surface format change, as we have to modify the colour
   * for fills.
   */
  if (state->mod_hw & (SMF_COLOR | SMF_DRAWING_FLAGS | SMF_DESTINATION))
    stgfxSetColour(pDev, state);

  if (state->mod_hw & SMF_SRC_BLEND)
    pDev->srcBlendFunc = state->src_blend;

  if (state->mod_hw & SMF_DST_BLEND)
    pDev->dstBlendFunc = state->dst_blend;

  if (state->mod_hw & SMF_SRC_COLORKEY)
    pDev->srcColourKey = state->src_colorkey;

  if (state->mod_hw & SMF_DST_COLORKEY)
    pDev->dstColourKey = state->dst_colorkey;

  switch (accel)
  {
    case DFXL_FILLRECTANGLE:
    case DFXL_DRAWRECTANGLE:
      state->set |= (DFXL_FILLRECTANGLE | DFXL_DRAWRECTANGLE);
      break;

    case DFXL_BLIT:
    case DFXL_STRETCHBLIT:
      state->set |= (DFXL_BLIT | DFXL_STRETCHBLIT);
      break;

    default:
      /* Should never happen as we have told DirectFB which functions we can support*/
      D_ERROR( "DirectFB/gfxdrivers/stgfx: unexpected drawing/blitting function" );
      break;
  }

  state->mod_hw = (StateModificationFlags)0;
}

static int
_check_src_constraints (const STGFXDriverData * const pDrv,
                        const STGFXDeviceData * const pDev,
                        const DFBRectangle    * const rect,
                        unsigned long         *base,
                        unsigned long         *end)
{
  /* complain if sth crosses a 64MB bank boundary. */
#define MEMORY64MB_SIZE         (1<<26)
#define _ALIGN_DOWN(addr,size)	((addr)&(~((size)-1)))

  DFBSurfacePixelFormat format = stgfxGetPixelFormat (pDev->srcFormat);

  *base = pDev->srcPhys;
  *base += rect->y * pDev->srcPitch
           + (rect->x * DFB_BITS_PER_PIXEL (format) / 8);
  *end = *base
          + (rect->h - 1) * pDev->srcPitch
          + (rect->w * DFB_BITS_PER_PIXEL (format) / 8);

  return (_ALIGN_DOWN (*base, MEMORY64MB_SIZE) != _ALIGN_DOWN (*end, MEMORY64MB_SIZE));
}

static int
_check_dst_constraints (const STGFXDriverData * const pDrv,
                        const STGFXDeviceData * const pDev,
                        const DFBRectangle    * const rect,
                        unsigned long         *base,
                        unsigned long         *end)
{
  DFBSurfacePixelFormat format = stgfxGetPixelFormat (pDev->dstFormat);

  *base = pDev->dstPhys;
  *base += rect->y * pDev->dstPitch
           + (rect->x * DFB_BITS_PER_PIXEL (format) / 8);
  *end = *base
         + (rect->h - 1) * pDev->dstPitch
         + (rect->w * DFB_BITS_PER_PIXEL (format) / 8);

  return (_ALIGN_DOWN (*base, MEMORY64MB_SIZE) != _ALIGN_DOWN (*end, MEMORY64MB_SIZE));
}

/********************************************************************************************/
/* Fill the rectangle 'rect' with the colour that has been previously specified in setstate */
/********************************************************************************************/
static bool stgfxFillRectangle
(
  void* drv,
  void* dev,
  DFBRectangle* rect
)
{
  STGFXDriverData*  pDrv = (STGFXDriverData*) drv;
  STGFXDeviceData*  pDev = (STGFXDeviceData*) dev;
  STMFBIO_BLT_DATA  bltData;

  bzero(&bltData, sizeof(STMFBIO_BLT_DATA));

  bltData.operation = BLT_OP_FILL;
  return stgfxDraw(pDrv, pDev, rect, &bltData);
}

static bool
stgfxFillRectangle_slow (void         *drv,
                         void         *dev,
                         DFBRectangle *rect)
{
  const STGFXDriverData * const pDrv = (STGFXDriverData *) drv;
  const STGFXDeviceData * const pDev = (STGFXDeviceData *) dev;

  unsigned long base, end;
  if (_check_dst_constraints (pDrv, pDev, rect, &base, &end))
  {
    fprintf (stderr, "%s: rect %d/%d/%d/%d (%#.8lx -> %#.8lx) crosses a bank boundary!\n",
             __FUNCTION__, rect->x, rect->y, rect->w, rect->h, base, end);
    return false;
  }

  return stgfxFillRectangle (drv, dev, rect);
}


/********************************************************************************************/
/* Draw the rectangle 'rect' with the colour that has been previously specified in setstate */
/********************************************************************************************/
static bool stgfxDrawRectangle
(
  void* drv,
  void* dev,
  DFBRectangle* rect
)
{
  STGFXDriverData*  pDrv = (STGFXDriverData*) drv;
  STGFXDeviceData*  pDev = (STGFXDeviceData*) dev;
  STMFBIO_BLT_DATA  bltData;

  bzero(&bltData, sizeof(STMFBIO_BLT_DATA));

  bltData.operation = BLT_OP_DRAW_RECTANGLE;
  return stgfxDraw(pDrv, pDev, rect, &bltData);
}

static bool
stgfxDrawRectangle_slow (void         *drv,
                         void         *dev,
                         DFBRectangle *rect)
{
  const STGFXDriverData * const pDrv = (STGFXDriverData *) drv;
  const STGFXDeviceData * const pDev = (STGFXDeviceData *) dev;

  unsigned long base, end;
  if (_check_dst_constraints (pDrv, pDev, rect, &base, &end))
  {
    fprintf (stderr, "%s: rect %d/%d/%d/%d (%#.8lx -> %#.8lx) crosses a bank boundary!\n",
             __FUNCTION__, rect->x, rect->y, rect->w, rect->h, base, end);
    return false;
  }

  return stgfxDrawRectangle (drv, dev, rect);
}


/******************************/
/* Blit from 'rect' to dx, dy */
/******************************/
static bool stgfxBlit
(
  void*         drv,
  void*         dev,
  DFBRectangle* rect,
  int           dx,
  int           dy
)
{
  STGFXDriverData*  pDrv = (STGFXDriverData*) drv;
  STGFXDeviceData*  pDev = (STGFXDeviceData*) dev;
  STMFBIO_BLT_DATA  bltData;

  bzero(&bltData, sizeof(STMFBIO_BLT_DATA));

  if(!stgfxCommonBlitData(pDrv, pDev, &bltData))
  {
    /* We don't need to do the operation as it would be a nop */
    return true;
  }

  if(!stgfxUpdatePalette(pDrv, pDev, &bltData))
    return false;

  bltData.src_left   = (unsigned short)rect->x;
  bltData.src_top    = (unsigned short)rect->y;
  bltData.src_right  = (unsigned short)(rect->x + rect->w);
  bltData.src_bottom = (unsigned short)(rect->y + rect->h);

  bltData.dst_left   = (unsigned short)dx;
  bltData.dst_top    = (unsigned short)dy;
  bltData.dst_right  = (unsigned short)(dx + rect->w);
  bltData.dst_bottom = (unsigned short)(dy + rect->h);

  if (ioctl(pDrv->fd, STMFBIO_BLT, &bltData ) < 0)
  {
    return false;
  }

  return true;
}

static bool
stgfxBlit_slow (void         *drv,
                void         *dev,
                DFBRectangle *rect,
                int           dx,
                int           dy)
{
  const STGFXDriverData * const pDrv = (STGFXDriverData *) drv;
  const STGFXDeviceData * const pDev = (STGFXDeviceData *) dev;

  unsigned long base, end;
  if (_check_src_constraints (pDrv, pDev, rect, &base, &end))
  {
    fprintf (stderr, "%s: srcrect %d/%d/%d/%d (%#.8lx -> %#.8lx) crosses a bank boundary!\n",
             __FUNCTION__, rect->x, rect->y, rect->w, rect->h, base, end);
    return false;
  }
  DFBRectangle dr = { .x = dx, .y = dy, .w = rect->w, .h = rect->h };
  if (_check_dst_constraints (pDrv, pDev, &dr, &base, &end))
  {
    fprintf (stderr, "%s: dstrect %d/%d/%d/%d (%#.8lx -> %#.8lx) crosses a bank boundary!\n",
             __FUNCTION__, dr.x, dr.y, dr.w, dr.h, base, end);
    return false;
  }

  return stgfxBlit (drv, dev, rect, dx, dy);
}


/**************************/
/* Blit from 'sr' to 'dr' */
/**************************/
static bool stgfxStretchBlit
(
  void*         drv,
  void*         dev,
  DFBRectangle* sr,
  DFBRectangle* dr
)
{
  STGFXDriverData*  pDrv = (STGFXDriverData*) drv;
  STGFXDeviceData*  pDev = (STGFXDeviceData*) dev;
  STMFBIO_BLT_DATA  bltData;

  bzero(&bltData, sizeof(STMFBIO_BLT_DATA));

  if(!stgfxCommonBlitData(pDrv, pDev, &bltData))
  {
    /* We don't need to do the operation as it would be a nop */
    return true;
  }

  if(!stgfxUpdatePalette(pDrv, pDev, &bltData))
    return false;

  bltData.src_left   = (unsigned short)sr->x;
  bltData.src_top    = (unsigned short)sr->y;
  bltData.src_right  = (unsigned short)(sr->x + sr->w);
  bltData.src_bottom = (unsigned short)(sr->y + sr->h);

  bltData.dst_left   = (unsigned short)dr->x;
  bltData.dst_top    = (unsigned short)dr->y;
  bltData.dst_right  = (unsigned short)(dr->x + dr->w);
  bltData.dst_bottom = (unsigned short)(dr->y + dr->h);

  if (ioctl(pDrv->fd, STMFBIO_BLT, &bltData ) < 0)
  {
    return false;
  }

  return true;
}

static bool
stgfxStretchBlit_slow (void         *drv,
                       void         *dev,
                       DFBRectangle *sr,
                       DFBRectangle *dr)
{
  const STGFXDriverData * const pDrv = (STGFXDriverData *) drv;
  const STGFXDeviceData * const pDev = (STGFXDeviceData *) dev;

  unsigned long base, end;
  if (_check_src_constraints (pDrv, pDev, sr, &base, &end))
  {
    fprintf (stderr, "%s: srcrect %d/%d/%d/%d (%#.8lx -> %#.8lx) crosses a bank boundary!\n",
             __FUNCTION__, sr->x, sr->y, sr->w, sr->h, base, end);
    return false;
  }
  if (_check_dst_constraints (pDrv, pDev, dr, &base, &end))
  {
    fprintf (stderr, "%s: dstrect %d/%d/%d/%d (%#.8lx -> %#.8lx) crosses a bank boundary!\n",
             __FUNCTION__, dr->x, dr->y, dr->w, dr->h, base, end);
    return false;
  }

  return stgfxStretchBlit (drv, dev, sr, dr);
}


static void
stgfx_check_memory_constraints (CoreGraphicsDevice  * const device,
                                GraphicsDeviceFuncs * const funcs)
{
  /* fbmem could in theory cross a 64MB boundary, stmfb doesn't go through any
     efforts whatsoever to prevent such from happening. It should be changed
     to add a check for configurations which end up crossing a boundary, but
     even then I don't think I'll remove the check from here, since otherwise
     we add a (soft) dependency on an up-to-date version of stmfb. */
  unsigned long fb_base = dfb_system_video_memory_physical (0);
  unsigned long fb_end  = fb_base + dfb_system_videoram_length () - 1;
  if (_ALIGN_DOWN (fb_base, MEMORY64MB_SIZE) != _ALIGN_DOWN (fb_end, MEMORY64MB_SIZE))
  {
    fprintf (stderr,
             "%s: Your framebuffer memory allocation is quite wrong!\n"
             "  Can't use hardware acceleration to its full extent!\n"
             "  Physical framebuffer memory must not cross a 64MB bank!\n"
             "  Please fix your configuration!\n", __FUNCTION__);

    funcs->FillRectangle = stgfxFillRectangle_slow;
    funcs->DrawRectangle = stgfxDrawRectangle_slow;
    funcs->Blit          = stgfxBlit_slow;
    funcs->StretchBlit   = stgfxStretchBlit_slow;
  }
}

/* exported symbols - These functions must be present */

/************************************************************/
/* Check if the hardware is ours by looking at the value    */
/* returned in the accel field of the framebuffer fixed info*/
/************************************************************/
static int driver_probe(CoreGraphicsDevice* device)
{
  int result = 0;
  int accel = dfb_gfxcard_get_accelerator( device );

  switch (accel)
  {
  case FB_ACCEL_ST_GAMMA:
    D_DEBUG( "DirectFB/STGFX: Found FB_ACCEL_ST_GAMMA\n" );
    result = 1;
    break;
  case FB_ACCEL_ST_BDISP:
    D_DEBUG( "DirectFB/STGFX: Found FB_ACCEL_ST_BDISP\n" );
    result = 1;
    break;
  default:
    D_DEBUG( "DirectFB/STGFX: unrecognised accelerator %d\n",accel );
    break;
  }

  return result;
}

/***************************************************/
/* Return some human readable info about our driver*/
/***************************************************/
static void driver_get_info(CoreGraphicsDevice *device, GraphicsDriverInfo *info)
{
  /* fill driver info structure */
  int accel = dfb_gfxcard_get_accelerator( device );

  switch (accel)
  {
    case FB_ACCEL_ST_GAMMA:
      snprintf( info->name, DFB_GRAPHICS_DRIVER_INFO_NAME_LENGTH, "ST Microelectronics Gamma" );
      break;
    case FB_ACCEL_ST_BDISP:
      snprintf( info->name, DFB_GRAPHICS_DRIVER_INFO_NAME_LENGTH, "ST Microelectronics BDisp" );
      break;
    default:
      break;
  }

  snprintf( info->vendor, DFB_GRAPHICS_DRIVER_INFO_VENDOR_LENGTH, "ST Microelectronics" );

  info->version.major = 1;
  info->version.minor = 0;

  /* Tell DirectFB how big our device and driver structures are */
  info->driver_data_size = sizeof (STGFXDriverData);
  info->device_data_size = sizeof (STGFXDeviceData);
}

/****************************************************************/
/* Set the function pointers for the acceleration functions.    */
/* This does not tell DirectFB what we support, this is done in */
/* driver_init_device.                                          */
/****************************************************************/
static DFBResult driver_init_driver
(
  CoreGraphicsDevice*   device,
  GraphicsDeviceFuncs*  funcs,
  void*                 driver_data,
  void*                 device_data,
  CoreDFB*              core
)
{
  STGFXDriverData * const pDrv = driver_data;
  STGFXDeviceData * const pDev = device_data;

  funcs->EngineSync         = stgfxEngineSync;
  funcs->CheckState         = stgfxCheckState;
  funcs->SetState           = stgfxSetState;
  funcs->FillRectangle      = stgfxFillRectangle;
  funcs->DrawRectangle      = stgfxDrawRectangle;
  funcs->Blit               = stgfxBlit;
  funcs->StretchBlit        = stgfxStretchBlit;

  pDrv->core = core;
  pDrv->device_data = device_data;

  /*
   * Get the framebuffer file descriptor. Because we successfully
   * probed ourselves the system _must_ be a framebuffer device
   */
  pDrv->fd = -1;

  if (dfb_system_type () == CORE_FBDEV)
  {
    const FBDev * const dfb_fbdev = dfb_system_data ();

    if (dfb_fbdev)
      pDrv->fd = dfb_fbdev->fd;
  }
  else if (dfb_system_type () == CORE_STMFBDEV)
  {
    const STMfbdev * const stmfbdev = dfb_system_data ();

    D_MAGIC_ASSERT (stmfbdev, STMfbdev);
    if (stmfbdev)
      pDrv->fd = stmfbdev->fd;
  }

  if (pDrv->fd < 0)
  {
    D_ERROR("DirectFB/gfxdrivers/stgfx: Only supported on FBDev and STMfbdev systems");
    return DFB_IO;
  }

  D_INFO("DirectFB/gfxdrivers/stgfx: FB file descriptor = %d\n",pDrv->fd);

  /* adjust function pointers */
  stgfx_check_memory_constraints (device, funcs);

  /*
   * Setup the driver state here. This cannot be done in probe as we need to
   * do it for slaves in multiapp mode and probe is only called by the master
   * application.
   */
  stgfx_accel_type = dfb_gfxcard_get_accelerator( device );

  switch (stgfx_accel_type)
  {
  case FB_ACCEL_ST_GAMMA:
    format_tbl = format_tbl_gamma;
    STGFX_VALID_DRAWINGFLAGS  = STGFX_VALID_DRAWINGFLAGS_GAMMA;
    STGFX_VALID_BLITTINGFLAGS = STGFX_VALID_BLITTINGFLAGS_GAMMA;
    break;
  case FB_ACCEL_ST_BDISP:
    format_tbl = format_tbl_bdisp;
    STGFX_VALID_DRAWINGFLAGS  = STGFX_VALID_DRAWINGFLAGS_BDISP;
    STGFX_VALID_BLITTINGFLAGS = STGFX_VALID_BLITTINGFLAGS_BDISP;
    break;
  default:
    return DFB_IO;
  }

  if (dfb_system_type () == CORE_STMFBDEV)
  {
    extern const SurfacePoolFuncs _g_stmfbdevSurfacePoolFuncs;
    extern ScreenFuncs            _g_stmfbdevScreenFuncs;
    extern DisplayLayerFuncs      _g_stmfbdevLayerFuncs;

    STMfbdev   * const stmfbdev = dfb_system_data ();
    CoreScreen *screen;

    D_MAGIC_ASSERT (stmfbdev, STMfbdev);

    if (!dfb_core_is_master (pDrv->core))
      dfb_surface_pool_join (pDrv->core, pDev->pool,
                             &_g_stmfbdevSurfacePoolFuncs);

    /* Register primary screen functions */
    screen = dfb_screens_register (NULL, stmfbdev,
                                   &_g_stmfbdevScreenFuncs);

    /* Register primary layer functions */
    dfb_layers_register (screen, stmfbdev, &_g_stmfbdevLayerFuncs);
  }
  else if (dfb_system_type() == CORE_FBDEV)
  {
    dfb_screens_hook_primary( device, driver_data,
                              &stgfxPrimaryScreenFuncs_stgfx1,
                              &pDrv->screenData.orig_funcs,
                              &pDrv->screenData.orig_data );

    dfb_layers_hook_primary( device, driver_data,
                             &stgfxPrimaryLayerFuncs_stgfx1,
                             &pDrv->layerData.orig_funcs,
                             &pDrv->layerData.orig_data );
  }

  if (!dfb_core_is_master (pDrv->core))
    stgfx1_surface_pool_join (pDrv->core, pDev->aux_pools,
                              D_ARRAY_SIZE (pDev->aux_pools));

  return DFB_OK;
}

/***************************/
/* Initialise the hardware */
/***************************/
static DFBResult driver_init_device
(
  CoreGraphicsDevice* device,
  GraphicsDeviceInfo* device_info,
  void*               driver_data,
  void*               device_data
)
{
  int              accel = dfb_gfxcard_get_accelerator( device );
  STGFXDeviceData * const pDev = device_data;
  STGFXDriverData * const pDrv = driver_data;

  switch (accel)
  {
    case FB_ACCEL_ST_GAMMA:
      snprintf( device_info->name, DFB_GRAPHICS_DEVICE_INFO_NAME_LENGTH, "Gamma" );
      break;
    case FB_ACCEL_ST_BDISP:
      snprintf( device_info->name, DFB_GRAPHICS_DEVICE_INFO_NAME_LENGTH, "BDisp" );
      break;
    default:
      break;
  }

  snprintf( device_info->vendor,
    DFB_GRAPHICS_DEVICE_INFO_VENDOR_LENGTH,
    "ST Microelectronics" );

  dfb_config->pollvsync_after = true;

  /* Tell DirectFB about what operations we may be able to do */
  device_info->caps.flags    = CCF_NOTRIEMU;
  device_info->caps.accel    = STGFX_VALID_DRAWINGFUNCTIONS | STGFX_VALID_BLITTINGFUNCTIONS;
  device_info->caps.drawing  = STGFX_VALID_DRAWINGFLAGS;
  device_info->caps.blitting = STGFX_VALID_BLITTINGFLAGS;

  /* Give DirectFB info about our surface limitations */
#if 0
  /* real BDispII limitations */
  device_info->limits.surface_byteoffset_alignment =  4;
  device_info->limits.surface_bytepitch_alignment  =  2;
#else
  /* the 3D unit on 7108 has these limitations */
  device_info->limits.surface_byteoffset_alignment = 64;
  device_info->limits.surface_bytepitch_alignment  =  8;
#endif
  device_info->limits.surface_pixelpitch_alignment =  1;

  if (dfb_system_type () == CORE_STMFBDEV)
  {
    extern const SurfacePoolFuncs _g_stmfbdevSurfacePoolFuncs;
    STMfbdev     * const stmfbdev = dfb_system_data ();

    D_MAGIC_ASSERT (stmfbdev, STMfbdev);

    dfb_surface_pool_initialize (pDrv->core, &_g_stmfbdevSurfacePoolFuncs,
                                 &pDev->pool);
  }
  /* aux pools */
  stgfx1_surface_pool_init (pDrv->core, pDev->aux_pools,
                            D_ARRAY_SIZE (pDev->aux_pools));

  return DFB_OK;
}


/*************************************************/
/* Do any hardware shutdown that may be required */
/*************************************************/
static void driver_close_device
(
  CoreGraphicsDevice* device,
  void*               driver_data,
  void*               device_data
)
{
  STGFXDriverData *pDrv = (STGFXDriverData *) driver_data;
  STGFXDeviceData *pDev = (STGFXDeviceData *) device_data;

  if (dfb_system_type () == CORE_STMFBDEV)
    dfb_surface_pool_destroy (pDev->pool);
  stgfx1_surface_pool_destroy (pDrv->core, pDev->aux_pools,
                               D_ARRAY_SIZE (pDev->aux_pools));
}

/*****************/
/* Driver de-init*/
/*****************/
static void driver_close_driver
(
  CoreGraphicsDevice* device,
  void*               driver_data
)
{
  STGFXDriverData* pDrv = (STGFXDriverData*) driver_data;
  STGFXDeviceData* pDev = pDrv->device_data;

  if (!dfb_core_is_master (pDrv->core))
  {
    if (dfb_system_type () == CORE_STMFBDEV)
      dfb_surface_pool_leave (pDev->pool);
    stgfx1_surface_pool_leave (pDrv->core, pDev->aux_pools,
                               D_ARRAY_SIZE (pDev->aux_pools));
  }
}
