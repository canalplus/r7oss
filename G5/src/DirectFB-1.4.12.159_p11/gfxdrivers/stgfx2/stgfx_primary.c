/*
   ST Microelectronics BDispII driver - primary layer implementation

   (c) Copyright 2007,2009-2010  STMicroelectronics Ltd.
   (c) Copyright 2000-2002       convergence integrated media GmbH.
   (c) Copyright 2002            convergence GmbH.

   All rights reserved.

   Based on the cle266 driver implementation:

   Written by Denis Oliver Kropp <dok@directfb.org>,
              Andreas Hundt <andi@fischlustig.de> and
              Sven Neumann <sven@convergence.de>.

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

#include <config.h>

#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/ioctl.h>

#include <linux/fb.h>
#include <linux/stmfb.h>

#include <directfb.h>

#include <core/layers.h>
#include <core/system.h>
#include <direct/debug.h>
#include <misc/conf.h>
#include <gfx/convert.h>

#if STGFX_DRIVER == 2
  #include "stm_gfxdriver.h"
  typedef struct _STGFX2DriverData STGFXDriverData;
#elif STGFX_DRIVER == 1
  #include "stgfx.h"
#else
  #error unknown stgfx driver version
#endif
#include "stgfx_primary.h"


D_DEBUG_DOMAIN (STGFX_Primary, "STGFX/Primary", "STM Primary (hook)");


typedef struct {
  u32                     originalVarCaps;
  DFBDisplayLayerOptions  supported_options;
  DFBColorAdjustmentFlags supported_adjustments;
} STGFXPrimarySharedData;


static int
stgfxPrimaryLayerDataSize (void)
{
  D_DEBUG_AT (STGFX_Primary, "%s (%u)\n",
              __FUNCTION__, sizeof (STGFXPrimarySharedData));

  return sizeof (STGFXPrimarySharedData);
}


static DFBResult
stgfxPrimaryInitLayer (CoreLayer                  *layer,
                       void                       *driver_data,
                       void                       *layer_data,
                       DFBDisplayLayerDescription *description,
                       DFBDisplayLayerConfig      *config,
                       DFBColorAdjustment         *adjustment)
{
  STGFXDriverData        * const drv = driver_data;
  STGFXPrimarySharedData * const shd = layer_data;
  DFBResult               ret;

  D_DEBUG_AT (STGFX_Primary, "%s\n", __FUNCTION__);

  D_ASSERT (dfb_system_type() == CORE_FBDEV);

  /* call the original initialization function first */
  ret = drv->layerData.orig_funcs.InitLayer (layer,
                                             drv->layerData.orig_data,
                                             layer_data, description,
                                             config, adjustment);
  if (ret)
    return ret;

  /*
   * Note we cannot use dfb_layer_id_translated because it requires state that
   * will not be setup until this hook has completed.
   */
  drv->layerData.startup_var.layerid = dfb_config->primary_layer;

  if (ioctl (drv->fd,
             STMFBIO_GET_VAR_SCREENINFO_EX, &drv->layerData.startup_var) < 0)
  {
    /*
     * We have a framebuffer driver that doesn't support the extended var
     * stuff, so ignore it, it will be as if we are a dumb framebuffer.
     */
    drv->layerData.startup_var.caps = STMFBIO_VAR_CAPS_NONE;
    shd->originalVarCaps            = STMFBIO_VAR_CAPS_NONE;
    return DFB_OK;
  }

  /* Duplicate originalVar.caps into Shared Memory for Fusion Slaves. */
  shd->originalVarCaps = drv->layerData.startup_var.caps;

  D_DEBUG ("DirectFB/gfxdrivers/stgfx_primary: got extended var caps: 0x%08x\n",
           shd->originalVarCaps);

  /* set name */
  snprintf (description->name,
            DFB_DISPLAY_LAYER_DESC_NAME_LENGTH, "STM Graphics Plane");

  shd->supported_options     = DLOP_NONE;
  shd->supported_adjustments = DCAF_NONE;

  /* add support alphachannel, we always do this */
  config->flags          |= DLCONF_OPTIONS;
  config->options         = DLOP_ALPHACHANNEL;
  shd->supported_options  = DLOP_ALPHACHANNEL;
  description->caps      |= DLCAPS_ALPHACHANNEL;

  if (drv->layerData.startup_var.caps & STMFBIO_VAR_CAPS_COLOURKEY)
    {
      D_DEBUG ("DirectFB/gfxdrivers/stgfx_primary: supports src colourkey\n");
      shd->supported_options |= DLOP_SRC_COLORKEY;
      description->caps      |= DLCAPS_SRC_COLORKEY;
    }

  if (drv->layerData.startup_var.caps & STMFBIO_VAR_CAPS_FLICKER_FILTER)
    {
      D_DEBUG ("DirectFB/gfxdrivers/stgfx_primary: supports flicker filtering\n");
      shd->supported_options |= DLOP_FLICKER_FILTERING;
      description->caps      |= DLCAPS_FLICKER_FILTERING;
    }

  if (drv->layerData.startup_var.caps & STMFBIO_VAR_CAPS_PREMULTIPLIED)
    {
      D_DEBUG ("DirectFB/gfxdrivers/stgfx_primary: supports pre-multiplied alpha\n");
      config->flags        |= DLCONF_SURFACE_CAPS;
      config->surface_caps  = DSCAPS_PREMULTIPLIED;
      description->caps    |= DLCAPS_PREMULTIPLIED;
    }

  if (drv->layerData.startup_var.caps & STMFBIO_VAR_CAPS_OPACITY)
    {
      D_DEBUG ("DirectFB/gfxdrivers/stgfx_primary: supports opacity\n");
      config->options        |= DLOP_OPACITY;
      /* Strangely we cannot specify the default layer opacity, lets hope its
         fully opaque! */
      shd->supported_options |= DLOP_OPACITY;
      description->caps      |= DLCAPS_OPACITY;
    }

  /* clear the adjustments added by the fbdev system, we can't support the
     gamma ramp approach, so override with the plane capabilities instead. */
  adjustment->flags = 0;

  if (drv->layerData.startup_var.caps & (STMFBIO_VAR_CAPS_GAIN
                                         | STMFBIO_VAR_CAPS_BRIGHTNESS))
    {
      D_DEBUG ("DirectFB/gfxdrivers/stgfx_primary: supports gain/brightness adjustment\n");
      shd->supported_adjustments |= DCAF_BRIGHTNESS;
      adjustment->flags          |= DCAF_BRIGHTNESS;
      adjustment->brightness      = 0x8000;
      description->caps          |= DLCAPS_BRIGHTNESS;
    }
  else
    {
      adjustment->flags &= ~DCAF_BRIGHTNESS;
      description->caps &= ~DLCAPS_BRIGHTNESS;
    }

  if (drv->layerData.startup_var.caps & STMFBIO_VAR_CAPS_CONTRAST)
    {
      D_DEBUG ("DirectFB/gfxdrivers/stgfx_primary: supports contrast adjustment\n");
      shd->supported_adjustments |= DCAF_CONTRAST;
      adjustment->flags          |= DCAF_CONTRAST;
      adjustment->contrast        = 0x8000;
      description->caps          |= DLCAPS_CONTRAST;
    }
  else
    {
      adjustment->flags &= ~DCAF_CONTRAST;
      description->caps &= ~DLCAPS_CONTRAST;
    }

  if (drv->layerData.startup_var.caps & STMFBIO_VAR_CAPS_TINT)
    {
      D_DEBUG ("DirectFB/gfxdrivers/stgfx_primary: supports tint adjustment\n");
      shd->supported_adjustments |= DCAF_HUE;
      adjustment->flags          |= DCAF_HUE;
      adjustment->hue             = 0x8000;
      description->caps          |= DLCAPS_HUE;
    }
  else
    {
      adjustment->flags &= ~DCAF_HUE;
      description->caps &= ~DLCAPS_HUE;
    }

  if (drv->layerData.startup_var.caps & STMFBIO_VAR_CAPS_SATURATION)
    {
      D_DEBUG ("DirectFB/gfxdrivers/stgfx_primary: supports saturation adjustment\n");
      shd->supported_adjustments |= DCAF_SATURATION;
      adjustment->flags          |= DCAF_SATURATION;
      adjustment->saturation      = 0x8000;
      description->caps          |= DLCAPS_SATURATION;
    }
  else
    {
      adjustment->flags &= ~DCAF_SATURATION;
      description->caps &= ~DLCAPS_SATURATION;
    }

  if (drv->layerData.startup_var.caps & STMFBIO_VAR_CAPS_ALPHA_RAMP)
    {
      D_DEBUG ("DirectFB/gfxdrivers/stgfx_primary: supports RGB1555 alpha ramp\n");
      description->caps |= DLCAPS_ALPHA_RAMP;
    }

  if (drv->layerData.startup_var.caps & STMFBIO_VAR_CAPS_ZPOSITION)
    {
      D_DEBUG ("DirectFB/gfxdrivers/stgfx_primary: supports plane re-ordering\n");
      description->caps |= DLCAPS_LEVELS;
      description->level = drv->layerData.startup_var.z_position;
    }

  return DFB_OK;
}

static DFBResult
stgfxPrimaryShutdownLayer (CoreLayer *layer,
                           void      *driver_data,
                           void      *layer_data)
{
  STGFXDriverData * const drv = driver_data;

  /* try and restore the framebuffer's extended state. */
  drv->layerData.startup_var.activate = STMFBIO_ACTIVATE_IMMEDIATE;
  if (ioctl (drv->fd,
             STMFBIO_SET_VAR_SCREENINFO_EX, &drv->layerData.startup_var) < 0)
    return DFB_IO;

  return DFB_OK;
}


static DFBResult
stgfxPrimaryTestRegion (CoreLayer                  *layer,
                        void                       *driver_data,
                        void                       *layer_data,
                        CoreLayerRegionConfig      *config,
                        CoreLayerRegionConfigFlags *failed)
{
  const STGFXDriverData        * const drv = driver_data;
  const STGFXPrimarySharedData * const shd = layer_data;
  DFBResult                     ret;
  CoreLayerRegionConfigFlags    fail = 0;
  DFBDisplayLayerOptions        options = config->options;

  /* remove options before calling the original function */
  config->options = DLOP_NONE;

  ret = drv->layerData.orig_funcs.TestRegion (layer,
                                              drv->layerData.orig_data,
                                              layer_data, config, &fail);

  /* restore options */
  config->options = options;

  /* check options if specified */
  if (options && ((options & ~shd->supported_options) != 0))
    fail |= CLRCF_OPTIONS;

  if ((config->surface_caps & DSCAPS_PREMULTIPLIED)
      && !(shd->originalVarCaps & STMFBIO_VAR_CAPS_PREMULTIPLIED))
    fail |= CLRCF_SURFACE_CAPS;

  if (failed)
    *failed = fail;

  if (fail)
    return DFB_UNSUPPORTED;

  return ret;
}


static DFBResult
stgfxPrimarySetRegion (CoreLayer                  *layer,
                       void                       *driver_data,
                       void                       *layer_data,
                       void                       *region_data,
                       CoreLayerRegionConfig      *config,
                       CoreLayerRegionConfigFlags  updated,
                       CoreSurface                *surface,
                       CorePalette                *palette,
                       CoreSurfaceBufferLock      *lock)
{
  const STGFXDriverData            * const drv = driver_data;
  const STGFXPrimarySharedData     * const shd = layer_data;
  DFBResult                         ret;
  struct stmfbio_var_screeninfo_ex  tmp = { 0 };

  D_DEBUG ("DirectFB/gfxdrivers/stgfx_primary: set region layer %p config %p "
           "surface %p\n", layer, config, surface);

  /* call the original function */
  ret = drv->layerData.orig_funcs.SetRegion (layer, drv->layerData.orig_data,
                                             layer_data, region_data,
                                             config, updated, surface,
                                             palette, lock);
  if (ret)
    return ret;

  if (shd->originalVarCaps == STMFBIO_VAR_CAPS_NONE)
    return DFB_OK;

  D_DEBUG ("DirectFB/gfxdrivers/stgfx_primary: set extended layer options\n");

  tmp.layerid  = dfb_layer_id_translated (layer);
  tmp.activate = STMFBIO_ACTIVATE_IMMEDIATE;

  if ((updated & (CLRCF_OPTIONS | CLRCF_SRCKEY))
      && (shd->originalVarCaps & STMFBIO_VAR_CAPS_COLOURKEY))
    {
      tmp.caps          |= STMFBIO_VAR_CAPS_COLOURKEY;
      tmp.colourKeyFlags = ((config->options & DLOP_SRC_COLORKEY)
                            ? STMFBIO_COLOURKEY_FLAGS_ENABLE
                            : 0
                           );
      tmp.min_colour_key = PIXEL_ARGB (0, config->src_key.r,
                                       config->src_key.g, config->src_key.b);
      tmp.max_colour_key = tmp.min_colour_key;
    }

  if ((updated & CLRCF_OPTIONS)
      && (shd->originalVarCaps & STMFBIO_VAR_CAPS_FLICKER_FILTER))
    {
      tmp.caps    |= STMFBIO_VAR_CAPS_FLICKER_FILTER;
      tmp.ff_state = ((config->options & DLOP_FLICKER_FILTERING)
                      ? STMFBIO_FF_ADAPTIVE
                      : STMFBIO_FF_OFF
                     );
    }

  if ((updated & CLRCF_SURFACE_CAPS)
      && (shd->originalVarCaps & STMFBIO_VAR_CAPS_PREMULTIPLIED))
    {
      tmp.caps |= STMFBIO_VAR_CAPS_PREMULTIPLIED;
      tmp.premultiplied_alpha = ((config->surface_caps & DSCAPS_PREMULTIPLIED)
                                 ? 1
                                 : 0
                                );
      D_DEBUG ("DirectFB/gfxdrivers/stgfx_primary: premultiplied alpha: %u\n",
               tmp.premultiplied_alpha);
    }

  if ((updated & CLRCF_OPACITY)
      && (shd->originalVarCaps & STMFBIO_VAR_CAPS_OPACITY))
    {
      tmp.caps   |= STMFBIO_VAR_CAPS_OPACITY;
      tmp.opacity = ((config->options & DLOP_OPACITY)
                     ? config->opacity
                     : 255
                    );
      D_DEBUG ("DirectFB/gfxdrivers/stgfx_primary: opacity: %u\n",
               tmp.opacity);
    }

  if ((updated & CLRCF_ALPHA_RAMP)
      && (shd->originalVarCaps & STMFBIO_VAR_CAPS_ALPHA_RAMP))
    {
      tmp.caps         |= STMFBIO_VAR_CAPS_ALPHA_RAMP;
      tmp.alpha_ramp[0] = config->alpha_ramp[0];
      tmp.alpha_ramp[1] = config->alpha_ramp[3];
    }

  if (tmp.caps
      && ioctl (drv->fd, STMFBIO_SET_VAR_SCREENINFO_EX, &tmp) < 0)
    return DFB_IO;

  return DFB_OK;
}


static DFBResult
stgfxPrimaryGetLevel (CoreLayer              *layer,
                      void                   *driver_data,
                      void                   *layer_data,
                      int                    *level)
{
  const STGFXDriverData            * const drv = driver_data;
  const STGFXPrimarySharedData     * const shd = layer_data;
  struct stmfbio_var_screeninfo_ex  tmp = { 0 };

  if (!(shd->originalVarCaps & STMFBIO_VAR_CAPS_ZPOSITION))
    return DFB_UNSUPPORTED;

  tmp.layerid = dfb_layer_id_translated (layer);

  if (ioctl (drv->fd, STMFBIO_GET_VAR_SCREENINFO_EX, &tmp) < 0)
    return DFB_IO;

  *level = (int) tmp.z_position;

  return DFB_OK;
}


static DFBResult
stgfxPrimarySetLevel (CoreLayer              *layer,
                      void                   *driver_data,
                      void                   *layer_data,
                      int                     level)
{
  const STGFXDriverData            * const drv = driver_data;
  const STGFXPrimarySharedData     * const shd = layer_data;
  struct stmfbio_var_screeninfo_ex  tmp = { 0 };

  if (!(shd->originalVarCaps & STMFBIO_VAR_CAPS_ZPOSITION))
    return DFB_UNSUPPORTED;

  tmp.layerid    = dfb_layer_id_translated (layer);
  tmp.activate   = STMFBIO_ACTIVATE_IMMEDIATE;
  tmp.caps       = STMFBIO_VAR_CAPS_ZPOSITION;
  tmp.z_position = (level < 0) ? 0 : level;

  if (ioctl (drv->fd, STMFBIO_SET_VAR_SCREENINFO_EX, &tmp) < 0)
    return (errno == EINVAL) ? DFB_INVARG : DFB_IO;

  return DFB_OK;
}


static DFBResult
stgfxPrimarySetColorAdjustment (CoreLayer          *layer,
                                void               *driver_data,
                                void               *layer_data,
                                DFBColorAdjustment *adjustment)
{
  const STGFXDriverData            * const drv = driver_data;
  const STGFXPrimarySharedData     * const shd = layer_data;
  struct stmfbio_var_screeninfo_ex  tmp = { 0 };

//  if ((adjustment->flags & ~shd->supported_adjustments) != 0)
//    return DFB_INVARG;

  tmp.layerid  = dfb_layer_id_translated (layer);
  tmp.activate = STMFBIO_ACTIVATE_IMMEDIATE;

  if (adjustment->flags & DCAF_BRIGHTNESS)
    {
      if (shd->originalVarCaps & STMFBIO_VAR_CAPS_BRIGHTNESS)
        {
          tmp.caps |= STMFBIO_VAR_CAPS_BRIGHTNESS;
          tmp.brightness = adjustment->brightness >> 8;
        }
      else
        {
          tmp.caps |= STMFBIO_VAR_CAPS_GAIN;
          /* 0-0x8000 -> 0-255 */
          tmp.gain = ((adjustment->brightness > 0x8000)
                      ? 255
                      : (((adjustment->brightness >> 8) * 255) / 128)
                     );
        }
    }

  if (adjustment->flags & DCAF_CONTRAST)
    {
      tmp.caps |= STMFBIO_VAR_CAPS_CONTRAST;
      tmp.contrast = adjustment->contrast >> 8;
    }

  if (adjustment->flags & DCAF_HUE)
    {
      tmp.caps |= STMFBIO_VAR_CAPS_TINT;
      tmp.tint = adjustment->hue >> 8;
    }

  if (adjustment->flags & DCAF_SATURATION)
    {
      tmp.caps |= STMFBIO_VAR_CAPS_SATURATION;
      tmp.saturation = adjustment->saturation >> 8;
    }

  if (ioctl (drv->fd, STMFBIO_SET_VAR_SCREENINFO_EX, &tmp) < 0)
    return (errno == EINVAL) ? DFB_INVARG : DFB_IO;

  return DFB_OK;
}


#define _stgfxPrimaryLayerFuncs(driver) \
DisplayLayerFuncs stgfxPrimaryLayerFuncs_stgfx ##driver = { \
  .LayerDataSize      = stgfxPrimaryLayerDataSize,     \
                                                       \
  .InitLayer          = stgfxPrimaryInitLayer,         \
  .ShutdownLayer      = stgfxPrimaryShutdownLayer,     \
                                                       \
  .TestRegion         = stgfxPrimaryTestRegion,        \
  .SetRegion          = stgfxPrimarySetRegion,         \
                                                       \
  .GetLevel           = stgfxPrimaryGetLevel,          \
  .SetLevel           = stgfxPrimarySetLevel,          \
                                                       \
  .SetColorAdjustment = stgfxPrimarySetColorAdjustment \
}

#define stgfxPrimaryLayerFuncs(driver) _stgfxPrimaryLayerFuncs(driver)

stgfxPrimaryLayerFuncs(STGFX_DRIVER);
