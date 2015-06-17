/*
   (c) Copyright 2010       STMicroelectronics (R&D) Ltd.
   (c) Copyright 2001-2009  The world wide DirectFB Open Source Community (directfb.org)
   (c) Copyright 2000-2004  Convergence (integrated media) GmbH

   All rights reserved.

   Written by André Draszik <andre.draszik@st.com>.

   Based on work by Denis Oliver Kropp <dok@directfb.org>,
                    Andreas Hundt <andi@fischlustig.de>,
                    Sven Neumann <neo@directfb.org>,
                    Ville Syrjälä <syrjala@sci.fi> and
                    Claudio Ciccani <klan@users.sf.net>.

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

#include <sys/ioctl.h>

#include <asm/types.h>
#include <linux/stmfb.h>

#include <directfb.h>
#include <directfb_util.h>

#include <core/coretypes.h>
#include <core/core.h>
#include <core/layers.h>
#include <core/layers_internal.h>
#include <core/palette.h>
#include <core/screen.h>
#include <core/screens.h>

#include <gfx/convert.h>

#include <misc/conf.h>

#include "stmfbdev/stmfbdev.h"

D_DEBUG_DOMAIN( STMfbdev_Layer, "STMfbdev/Layer", "STMfb System Module Layer Handling" );


typedef struct {
     int magic;

     int                      layerid;
     DFBDisplayLayerOptions   supported_options;
     DFBSurfaceCapabilities   supported_surfacecaps;

     struct stmfbio_planeinfo cur_planeinfo;

     struct fb_cmap           current_cmap;

     DFBColorAdjustmentFlags  supported_adjustments;

     struct stmfbio_planeinfo         orig_planeinfo;
     struct stmfbio_var_screeninfo_ex orig_varex;
     struct fb_cmap                   orig_cmap;
     void                            *cmap_memory[2]; /* orig and current */
     FusionSHMPoolShared             *shmpool_data;
} STMfbdevLayerSharedData;



/* supported surface formats */
#define STM_supported_pixelformat \
  DSPF_ARGB1555: case DSPF_RGB16: case DSPF_RGB24: case DSPF_ARGB: \
  case DSPF_A8: case DSPF_UYVY: case DSPF_LUT8: \
  case DSPF_ALUT44: case DSPF_A1: case DSPF_ARGB4444: case DSPF_LUT2: \
  case DSPF_ARGB8565: case DSPF_AVYU: case DSPF_VYU

static const SURF_FMT dspf_to_stmfb[DFB_NUM_PIXELFORMATS] = {
     [DFB_PIXELFORMAT_INDEX (DSPF_UNKNOWN)]  = SURF_NULL_PAD,
     [DFB_PIXELFORMAT_INDEX (DSPF_ARGB1555)] = SURF_ARGB1555,
     [DFB_PIXELFORMAT_INDEX (DSPF_RGB16)]    = SURF_RGB565,
     [DFB_PIXELFORMAT_INDEX (DSPF_RGB24)]    = SURF_RGB888,
     [DFB_PIXELFORMAT_INDEX (DSPF_RGB32)]    = SURF_NULL_PAD,
     [DFB_PIXELFORMAT_INDEX (DSPF_ARGB)]     = SURF_ARGB8888,
     [DFB_PIXELFORMAT_INDEX (DSPF_A8)]       = SURF_A8,
     [DFB_PIXELFORMAT_INDEX (DSPF_YUY2)]     = SURF_NULL_PAD,
     [DFB_PIXELFORMAT_INDEX (DSPF_RGB332)]   = SURF_NULL_PAD,
     [DFB_PIXELFORMAT_INDEX (DSPF_UYVY)]     = SURF_YCBCR422R,
     [DFB_PIXELFORMAT_INDEX (DSPF_I420)]     = SURF_NULL_PAD,
     [DFB_PIXELFORMAT_INDEX (DSPF_YV12)]     = SURF_NULL_PAD,
     [DFB_PIXELFORMAT_INDEX (DSPF_LUT8)]     = SURF_CLUT8,
     [DFB_PIXELFORMAT_INDEX (DSPF_ALUT44)]   = SURF_ACLUT44,
     [DFB_PIXELFORMAT_INDEX (DSPF_AiRGB)]    = SURF_NULL_PAD,
     [DFB_PIXELFORMAT_INDEX (DSPF_A1)]       = SURF_A1,
     [DFB_PIXELFORMAT_INDEX (DSPF_NV12)]     = SURF_NULL_PAD,
     [DFB_PIXELFORMAT_INDEX (DSPF_NV16)]     = SURF_NULL_PAD,
     [DFB_PIXELFORMAT_INDEX (DSPF_ARGB2554)] = SURF_NULL_PAD,
     [DFB_PIXELFORMAT_INDEX (DSPF_ARGB4444)] = SURF_ARGB4444,
     [DFB_PIXELFORMAT_INDEX (DSPF_RGBA4444)] = SURF_NULL_PAD,
     [DFB_PIXELFORMAT_INDEX (DSPF_NV21)]     = SURF_NULL_PAD,
     [DFB_PIXELFORMAT_INDEX (DSPF_AYUV)]     = SURF_NULL_PAD,
     [DFB_PIXELFORMAT_INDEX (DSPF_A4)]       = SURF_NULL_PAD,
     [DFB_PIXELFORMAT_INDEX (DSPF_ARGB1666)] = SURF_NULL_PAD,
     [DFB_PIXELFORMAT_INDEX (DSPF_ARGB6666)] = SURF_NULL_PAD,
     [DFB_PIXELFORMAT_INDEX (DSPF_RGB18)]    = SURF_NULL_PAD,
     [DFB_PIXELFORMAT_INDEX (DSPF_LUT2)]     = SURF_CLUT2,
     [DFB_PIXELFORMAT_INDEX (DSPF_RGB444)]   = SURF_NULL_PAD,
     [DFB_PIXELFORMAT_INDEX (DSPF_RGB555)]   = SURF_NULL_PAD,
     [DFB_PIXELFORMAT_INDEX (DSPF_BGR555)]   = SURF_NULL_PAD,
     [DFB_PIXELFORMAT_INDEX (DSPF_RGBA5551)] = SURF_NULL_PAD,
     [DFB_PIXELFORMAT_INDEX (DSPF_YUV444P)]  = SURF_NULL_PAD,
     [DFB_PIXELFORMAT_INDEX (DSPF_ARGB8565)] = SURF_ARGB8565,
     [DFB_PIXELFORMAT_INDEX (DSPF_AVYU)]     = SURF_ACRYCB8888,
     [DFB_PIXELFORMAT_INDEX (DSPF_VYU)]      = SURF_CRYCB888,
     [DFB_PIXELFORMAT_INDEX (DSPF_A1_LSB)]   = SURF_NULL_PAD,
     [DFB_PIXELFORMAT_INDEX (DSPF_YV16)]     = SURF_NULL_PAD,
#if 0
     /* unsupported in DirectFB */
     [DFB_PIXELFORMAT_INDEX (DSPF_BGRA)]     = { SURF_BGRA8888,
     [DFB_PIXELFORMAT_INDEX (DSPF_LUT1)]     = { SURF_CLUT1,
     [DFB_PIXELFORMAT_INDEX (DSPF_LUT4)]     = { SURF_CLUT4,
     [DFB_PIXELFORMAT_INDEX (DSPF_ALUT88)]   = { SURF_ACLUT88,
#endif
};

static DFBSurfacePixelFormat
stmfb_to_dsbf( SURF_FMT fmt )
{
     switch (fmt) {
          case SURF_ARGB1555:   return DSPF_ARGB1555;
          case SURF_RGB565:     return DSPF_RGB16;
          case SURF_RGB888:     return DSPF_RGB24;
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
};


/******************************************************************************/

static int
stmfbdevLayerDataSize( void )
{
     D_DEBUG_AT( STMfbdev_Layer, "%s() <- %u\n",
                 __FUNCTION__, sizeof(STMfbdevLayerSharedData) );

     return sizeof(STMfbdevLayerSharedData);
}

static int
stmfbdevRegionDataSize( void )
{
     return 0;
}

static DFBResult
stmfbdevInitLayer( CoreLayer                  *layer,
                   void                       *driver_data,
                   void                       *layer_data,
                   DFBDisplayLayerDescription *description,
                   DFBDisplayLayerConfig      *config,
                   DFBColorAdjustment         *adjustment )
{
     const STMfbdev                    * const stmfbdev = driver_data;
     STMfbdevLayerSharedData           * const shared = layer_data;
     const struct stmfbio_plane_config *pc;
     u32                                hw_caps;

     D_DEBUG_AT( STMfbdev_Layer, "%s( %p %d )\n",
                 __FUNCTION__, layer, stmfbdev->shared->num_layers + 1 );

     D_MAGIC_ASSERT( stmfbdev, STMfbdev );
     D_MAGIC_ASSERT( stmfbdev->shared, STMfbdevSharedData );

     D_MAGIC_SET( shared, STMfbdevLayerSharedData );

     shared->layerid = stmfbdev->shared->num_layers++;

     shared->shmpool_data = dfb_core_shmpool_data( stmfbdev->core );

     /* remember startup config, so we can restore it on shutdown */
     shared->orig_planeinfo.layerid = shared->layerid;
     if (ioctl( stmfbdev->fd, STMFBIO_GET_PLANEMODE, &shared->orig_planeinfo ) < 0) {
          D_PERROR( "STMfbdev/Layer: Could not get planeinfo!\n" );
          return errno2result( errno );
     }
     shared->orig_planeinfo.activate = STMFBIO_ACTIVATE_IMMEDIATE;
     shared->cur_planeinfo = shared->orig_planeinfo;
     pc = &shared->cur_planeinfo.config;

     D_DEBUG_AT( STMfbdev_Layer,
                 "  -> current: %dx%d @ %d,%d-%dx%d (%d: %s) %ubpp pitch %u @ 0x%08lx\n",
                 pc->source.w, pc->source.h, pc->dest.x, pc->dest.y,
                 pc->dest.dim.w, pc->dest.dim.h, pc->format,
                 dfb_pixelformat_name( stmfb_to_dsbf( pc->format ) ),
                 pc->bitdepth, pc->pitch, pc->baseaddr );

     /* same for stmfb's extended var */
     shared->orig_varex.layerid = shared->layerid;
     if (ioctl( stmfbdev->fd,
                STMFBIO_GET_VAR_SCREENINFO_EX, &shared->orig_varex ) < 0) {
          D_PERROR( "STMfbdev/Layer: Could not get extended planeinfo!\n" );
          return errno2result( errno );
     }
     shared->orig_varex.activate = STMFBIO_ACTIVATE_IMMEDIATE;
     hw_caps = shared->orig_varex.caps;
     D_DEBUG_AT( STMfbdev_Layer, "  -> extended caps: 0x%08x\n", hw_caps );

     /* allocate space for two colormaps - one to work with ... */
     shared->cmap_memory[0] = SHMALLOC( shared->shmpool_data, 256 * 2 * 4 );
     if (!shared->cmap_memory[0])
          return D_OOSHM();
     shared->current_cmap.start  = 0;
     shared->current_cmap.len    = 256;
     shared->current_cmap.red    = shared->cmap_memory[0] + 256 * 2 * 0;
     shared->current_cmap.green  = shared->cmap_memory[0] + 256 * 2 * 1;
     shared->current_cmap.blue   = shared->cmap_memory[0] + 256 * 2 * 2;
     shared->current_cmap.transp = shared->cmap_memory[0] + 256 * 2 * 3;

     /* ... and one to store the original colormap */
     shared->cmap_memory[1] = SHMALLOC( shared->shmpool_data, 256 * 2 * 4 );
     if (!shared->cmap_memory[1]) {
          SHFREE( shared->shmpool_data, shared->cmap_memory[0] );
          shared->cmap_memory[0] = NULL;
          shared->current_cmap.len = 0;
          return D_OOSHM();
     }
     shared->orig_cmap.start  = 0;
     shared->orig_cmap.len    = 256;
     shared->orig_cmap.red    = shared->cmap_memory[1] + 256 * 2 * 0;
     shared->orig_cmap.green  = shared->cmap_memory[1] + 256 * 2 * 1;
     shared->orig_cmap.blue   = shared->cmap_memory[1] + 256 * 2 * 2;
     shared->orig_cmap.transp = shared->cmap_memory[1] + 256 * 2 * 3;

     if (ioctl( stmfbdev->fd, FBIOGETCMAP, &shared->orig_cmap ) < 0) {
          D_DEBUG( "STMfbdev/Layer: Could not retrieve palette for backup!\n" );

          memset( &shared->orig_cmap, 0, sizeof(shared->orig_cmap) );

          SHFREE( shared->shmpool_data, shared->cmap_memory[1] );
          shared->cmap_memory[1] = NULL;
     }


     /* set name */
     snprintf( description->name, sizeof (description->name),
               "STM Layer %d", shared->cur_planeinfo.layerid );

     /* set capabilities and type */
     description->caps = (DLCAPS_SURFACE
                          | DLCAPS_SCREEN_POSITION | DLCAPS_SCREEN_SIZE
                          | DLCAPS_SCREEN_LOCATION
                          | DLCAPS_ALPHACHANNEL
                         );
     description->type = DLTF_GRAPHICS;
     description->regions = 1;

     /* fill out the default configuration */
     config->flags      = (DLCONF_WIDTH | DLCONF_HEIGHT | DLCONF_PIXELFORMAT
                           | DLCONF_BUFFERMODE
                           | DLCONF_OPTIONS
                          );
     config->buffermode = DLBM_FRONTONLY;
     if (D_FLAGS_ARE_SET (dfb_config->layers[shared->layerid].config.flags,
                          (DLCONF_WIDTH | DLCONF_HEIGHT))) {
          config->width = dfb_config->layers[shared->layerid].config.width;
          config->height = dfb_config->layers[shared->layerid].config.height;
     }
     else if (dfb_config->mode.width && dfb_config->mode.height) {
          config->width = dfb_config->mode.width;
          config->height = dfb_config->mode.height;
     }
     else {
          config->width = pc->source.w;
          config->height = pc->source.h;
     }
     if (D_FLAGS_ARE_SET (dfb_config->layers[shared->layerid].config.flags,
                          DLCONF_PIXELFORMAT))
          config->pixelformat = dfb_config->layers[shared->layerid].config.pixelformat;
     else if (dfb_config->mode.format)
          config->pixelformat = dfb_config->mode.format;
     else
          config->pixelformat = stmfb_to_dsbf( pc->format );

     config->options = DLOP_ALPHACHANNEL;

     adjustment->flags = DCAF_NONE;

     shared->supported_options     = DLOP_ALPHACHANNEL;
     shared->supported_adjustments = DCAF_NONE;
     shared->supported_surfacecaps = DSCAPS_NONE;

     if (D_FLAGS_IS_SET( hw_caps, STMFBIO_VAR_CAPS_COLOURKEY )) {
          D_DEBUG_AT( STMfbdev_Layer, "    +> src colorkey\n" );
          description->caps |= DLCAPS_SRC_COLORKEY;
          shared->supported_options |= DLOP_SRC_COLORKEY;
     }
     if (D_FLAGS_IS_SET( hw_caps, STMFBIO_VAR_CAPS_FLICKER_FILTER )) {
          D_DEBUG_AT( STMfbdev_Layer, "    +> flicker filtering (state: %d)\n",
                      shared->orig_varex.ff_state );
          description->caps |= DLCAPS_FLICKER_FILTERING;
          shared->supported_options |= DLOP_FLICKER_FILTERING;
     }
     if (D_FLAGS_IS_SET( hw_caps, STMFBIO_VAR_CAPS_PREMULTIPLIED )) {
          D_DEBUG_AT( STMfbdev_Layer, "    +> pre-multiplied alpha (state: %d)\n",
                      shared->orig_varex.premultiplied_alpha );
          description->caps |= DLCAPS_PREMULTIPLIED;
          shared->supported_surfacecaps |= DSCAPS_PREMULTIPLIED;

          /* we force this to on! */
          config->flags |= DLCONF_SURFACE_CAPS;
          config->surface_caps = DSCAPS_PREMULTIPLIED;
     }
     if (D_FLAGS_IS_SET( hw_caps, STMFBIO_VAR_CAPS_OPACITY )) {
          D_DEBUG_AT( STMfbdev_Layer, "    +> opacity (%d)\n",
                      shared->orig_varex.opacity );
          description->caps |= DLCAPS_OPACITY;
          shared->supported_options |= DLOP_OPACITY;

          config->options |= DLOP_OPACITY;
          /* Strangely we cannot specify the default layer opacity, lets hope
             it's fully opaque! */
     }
     if (D_FLAGS_IS_SET( hw_caps, (STMFBIO_VAR_CAPS_GAIN
                                   | STMFBIO_VAR_CAPS_BRIGHTNESS ))) {
          D_DEBUG_AT( STMfbdev_Layer, "    +> brightness/gain adjustment (%d %d)\n",
                      shared->orig_varex.brightness, shared->orig_varex.gain );
          description->caps |= DLCAPS_BRIGHTNESS;
          shared->supported_adjustments |= DCAF_BRIGHTNESS;

          adjustment->flags |= DCAF_BRIGHTNESS;
          adjustment->brightness = 0x8000;
     }
     if (D_FLAGS_IS_SET( hw_caps, STMFBIO_VAR_CAPS_CONTRAST )) {
          D_DEBUG_AT( STMfbdev_Layer, "    +> contrast adjustment (%d)\n",
                      shared->orig_varex.contrast );
          description->caps |= DLCAPS_CONTRAST;
          shared->supported_adjustments |= DCAF_CONTRAST;

          adjustment->flags |= DCAF_CONTRAST;
          adjustment->contrast = 0x8000;
     }
     if (D_FLAGS_IS_SET( hw_caps, STMFBIO_VAR_CAPS_TINT )) {
          D_DEBUG_AT( STMfbdev_Layer, "    +> tint adjustment (%d)\n",
                      shared->orig_varex.tint );
          description->caps |= DLCAPS_HUE;
          shared->supported_adjustments |= DCAF_HUE;

          adjustment->flags |= DCAF_HUE;
          adjustment->hue = 0x8000;
     }
     if (D_FLAGS_IS_SET( hw_caps, STMFBIO_VAR_CAPS_SATURATION )) {
          D_DEBUG_AT( STMfbdev_Layer, "    +> saturation adjustment (%d)\n",
                      shared->orig_varex.saturation );
          description->caps |= DLCAPS_SATURATION;
          shared->supported_adjustments |= DCAF_SATURATION;

          adjustment->flags |= DCAF_SATURATION;
          adjustment->saturation = 0x8000;
     }
     if (D_FLAGS_IS_SET( hw_caps, STMFBIO_VAR_CAPS_ALPHA_RAMP )) {
          D_DEBUG_AT( STMfbdev_Layer, "    +> RGB1555 alpha ramp\n" );
          description->caps |= DLCAPS_ALPHA_RAMP;
     }
     if (D_FLAGS_IS_SET( hw_caps, STMFBIO_VAR_CAPS_ZPOSITION )) {
          D_DEBUG_AT( STMfbdev_Layer, "    +> plane re-ordering (%d)\n",
                      shared->orig_varex.z_position );
          description->caps |= DLCAPS_LEVELS;
          description->level = shared->orig_varex.z_position;
       }

     return DFB_OK;
}

static DFBResult
stmfbdevShutdownLayer( CoreLayer *layer,
                       void      *driver_data,
                       void      *layer_data )
{
     const STMfbdev          * const stmfbdev = driver_data;
     STMfbdevLayerSharedData * const shared = layer_data;

     D_DEBUG_AT( STMfbdev_Layer, "%s()\n", __FUNCTION__ );

     D_MAGIC_ASSERT( stmfbdev, STMfbdev );
     D_MAGIC_ASSERT( shared, STMfbdevLayerSharedData );

     if (shared->cmap_memory[1]) {
          if (ioctl( stmfbdev->fd, FBIOPUTCMAP, &shared->orig_cmap ) < 0)
               D_DEBUG( "STMfbdev/Layer: "
                        "Could not restore palette!\n" );

          SHFREE( shared->shmpool_data, shared->cmap_memory[1] );
     }

     if (shared->cmap_memory[0])
          SHFREE( shared->shmpool_data, shared->cmap_memory[0] );

     D_MAGIC_CLEAR( shared );

     return DFB_OK;
}

static DFBResult
stmfbdevTestRegion( CoreLayer                  *layer,
                    void                       *driver_data,
                    void                       *layer_data,
                    CoreLayerRegionConfig      *config,
                    CoreLayerRegionConfigFlags *failed )
{
     const STMfbdev             * const stmfbdev = driver_data;
     STMfbdevLayerSharedData    * const shared = layer_data;
     struct stmfbio_planeinfo    plane;
     CoreLayerRegionConfigFlags  fail = CLRCF_NONE;

     D_DEBUG_AT( STMfbdev_Layer, "%s()\n", __FUNCTION__ );

     D_MAGIC_ASSERT( stmfbdev, STMfbdev );
     D_MAGIC_ASSERT( shared, STMfbdevLayerSharedData );
     D_ASSERT( config != NULL );

     (void) stmfbdev;

     DFB_CORE_LAYER_REGION_CONFIG_DEBUG_AT( STMfbdev_Layer, config );

     if (D_FLAGS_INVALID( config->options, shared->supported_options ))
          fail |= CLRCF_OPTIONS;

     /* some defaults - the original base address should always be ok, as it
        should be at the beginning of the framebuffer, thus any plane should
        fit. */
     plane.layerid = shared->orig_planeinfo.layerid;
     plane.config.baseaddr = shared->orig_planeinfo.config.baseaddr;

     /* we can't (and don't want to) test these individually, as the driver
        does some comprehensive checking on the parameters, and they must
        all be consistent, e.g. size/pitch/memory/scaling etc. */
     plane.activate = STMFBIO_ACTIVATE_TEST;
     plane.config.source.w = config->width;
     plane.config.source.h = config->height;
     plane.config.dest.x = config->dest.x;
     plane.config.dest.y = config->dest.y;
     plane.config.dest.dim.w = config->dest.w;
     plane.config.dest.dim.h = config->dest.h;
     plane.config.format = dspf_to_stmfb[DFB_PIXELFORMAT_INDEX( config->format )];
     plane.config.pitch = DFB_BYTES_PER_LINE( config->format, config->width );
     if (ioctl( stmfbdev->fd, STMFBIO_SET_PLANEMODE, &plane ) < 0)
          fail |= CLRCF_FORMAT | CLRCF_WIDTH | CLRCF_HEIGHT | CLRCF_DEST;

     /* can there be more flags set? */
     if (D_FLAGS_IS_SET( config->surface_caps, DSCAPS_PREMULTIPLIED )
         && !D_FLAGS_IS_SET( shared->supported_surfacecaps,
                             DSCAPS_PREMULTIPLIED ))
          fail |= CLRCF_SURFACE_CAPS;

     if (failed)
          *failed = fail;

     if (fail) {
          D_DEBUG_AT( STMfbdev_Layer, "  => FAILED!\n" );
          return DFB_UNSUPPORTED;
     }

     D_DEBUG_AT( STMfbdev_Layer, "  => SUCCESS\n" );

     return DFB_OK;
}

static DFBResult
stmfbdevAddRegion( CoreLayer             *layer,
                   void                  *driver_data,
                   void                  *layer_data,
                   void                  *region_data,
                   CoreLayerRegionConfig *config )
{
     const STMfbdev          * const stmfbdev = driver_data;
     STMfbdevLayerSharedData * const shared = layer_data;

     D_DEBUG_AT( STMfbdev_Layer, "%s()\n", __FUNCTION__ );

     D_MAGIC_ASSERT( stmfbdev, STMfbdev );
     D_MAGIC_ASSERT( shared, STMfbdevLayerSharedData );
     D_ASSERT( config != NULL );

     (void) stmfbdev;
     (void) shared;

     return DFB_OK;
}

static DFBResult
stmfbdevSetRegion( CoreLayer                  *layer,
                   void                       *driver_data,
                   void                       *layer_data,
                   void                       *region_data,
                   CoreLayerRegionConfig      *config,
                   CoreLayerRegionConfigFlags  updated,
                   CoreSurface                *surface,
                   CorePalette                *palette,
                   CoreSurfaceBufferLock      *lock )
{
     const STMfbdev                   * const stmfbdev = driver_data;
     STMfbdevLayerSharedData          * const shared = layer_data;
     struct stmfbio_planeinfo          plane;
     struct stmfbio_var_screeninfo_ex  var_ex;
     u32                               hw_caps;
     bool                              need_update = false;

     D_DEBUG_AT( STMfbdev_Layer, "%s()\n", __FUNCTION__ );

     D_MAGIC_ASSERT( stmfbdev, STMfbdev );
     D_MAGIC_ASSERT( shared, STMfbdevLayerSharedData );
     D_ASSERT( config != NULL );

     (void) stmfbdev;

     if (updated)
          DFB_CORE_LAYER_REGION_CONFIG_DEBUG_AT( STMfbdev_Layer, config );

     D_DEBUG_AT( STMfbdev_Layer, "  -> updated: %x\n", updated);

     plane.layerid = shared->cur_planeinfo.layerid;
     plane.activate = STMFBIO_ACTIVATE_TEST;
     plane.config = shared->cur_planeinfo.config;

     /* Update position? */
     if (D_FLAGS_IS_SET( updated, CLRCF_DEST )) {
          /* Set horizontal and vertical offset & size. */
          D_DEBUG_AT( STMfbdev_Layer, "    +> destination: %d,%d - %dx%d\n",
                      config->dest.x, config->dest.y,
                      config->dest.w, config->dest.h );

          need_update = true;

          plane.config.dest.x = config->dest.x;
          plane.config.dest.y = config->dest.y;
          plane.config.dest.dim.w = config->dest.w;
          plane.config.dest.dim.h = config->dest.h;
     }

     /* Update size? */
     if (D_FLAGS_IS_SET( updated, (CLRCF_WIDTH | CLRCF_HEIGHT) )) {
          /* Set width and height. */
          D_DEBUG_AT( STMfbdev_Layer, "    +> width/height: %dx%d, vp: %d,%d - %dx%d\n",
                      config->width, config->height,
                      config->source.x, config->source.y,
                      config->source.w, config->source.h);

          need_update = true;

          /* FIXME: stmfb driver API is incomplete! Should be updated to
             support a real source 'viewport' inside a source surface. */
          plane.config.source.w = config->width;
          plane.config.source.h = config->height;

          /* fixme: clipping? */
#if 0
          if (config->dest.x + config->width > screen_width)
               plane.config.source.w = screen_width - config->dest.x;

          if (config->dest.y + config->height > screen_height)
               plane.config.source.h = screen_height - config->dest.y;
#endif
     }
     D_ASSUME (config->width == config->source.w);
     D_ASSUME (config->height == config->source.h);
     D_ASSUME (config->source.x == 0);
     D_ASSUME (config->source.y == 0);

     /* Update surface? */
     if (D_FLAGS_IS_SET( updated, CLRCF_SURFACE )) {
          /* Set buffer base address and pitch. */
          D_DEBUG_AT( STMfbdev_Layer,
                      "    +> offset 0x%.8lx, pitch %d, addr %p, phys 0x%.8lx\n",
                      lock->offset, lock->pitch, lock->addr, lock->phys);

          need_update = true;

          plane.config.baseaddr = lock->phys;
          plane.config.pitch = lock->pitch;
     }

     /* Update format? */
     if (D_FLAGS_IS_SET( updated, CLRCF_FORMAT )) {
          /* Set pixel format. */
          switch (config->format) {
               case STM_supported_pixelformat:
                    plane.config.format = dspf_to_stmfb[DFB_PIXELFORMAT_INDEX( config->format )];
                    need_update = true;
                    break;

               default:
                    break;
          }

          D_DEBUG_AT( STMfbdev_Layer, "    +> pixelformat: %x (%s) -> %d\n",
                      config->format, dfb_pixelformat_name( config->format ),
                      plane.config.format );
     }
     D_ASSUME( surface->config.format == config->format );


     /* now for the extended state stuff */
     hw_caps = shared->orig_varex.caps;
     var_ex.layerid = shared->orig_varex.layerid;
     var_ex.activate = STMFBIO_ACTIVATE_IMMEDIATE;
     var_ex.caps = 0;

     if (D_FLAGS_IS_SET( updated, CLRCF_OPTIONS )) {
          D_DEBUG_AT( STMfbdev_Layer, "    +> options: 0x%.8x %s%s%s%s%s%s%s\n",
                      config->options,
                      (config->options & DLOP_ALPHACHANNEL) ? "ALPHA " : "",
                      (config->options & DLOP_FLICKER_FILTERING) ? "FLICKER " : "",
                      (config->options & DLOP_DEINTERLACING) ? "DEI " : "",
                      (config->options & DLOP_SRC_COLORKEY) ? "SRCKEY " : "",
                      (config->options & DLOP_DST_COLORKEY) ? "DSTKEY " : "",
                      (config->options & DLOP_OPACITY) ? "OPACITY " : "",
                      (config->options & DLOP_FIELD_PARITY) ? "PARITY " : "" );

          D_ASSUME( D_FLAGS_IS_SET( config->options, DLOP_ALPHACHANNEL ));

          if (D_FLAGS_IS_SET( hw_caps, STMFBIO_VAR_CAPS_FLICKER_FILTER )) {
               var_ex.caps |= STMFBIO_VAR_CAPS_FLICKER_FILTER;
               var_ex.ff_state = ((config->options & DLOP_FLICKER_FILTERING)
                                  ? STMFBIO_FF_ADAPTIVE
                                  : STMFBIO_FF_OFF
                                 );
          }
     }

     if (D_FLAGS_IS_SET( updated, (CLRCF_OPTIONS | CLRCF_SRCKEY) )) {
          D_DEBUG_AT( STMfbdev_Layer, "    +> srckey: %02x%02x%02x\n",
                      config->src_key.r, config->src_key.g, config->src_key.b );

          if (D_FLAGS_IS_SET( hw_caps, STMFBIO_VAR_CAPS_COLOURKEY )) {

               var_ex.caps |= STMFBIO_VAR_CAPS_COLOURKEY;
               var_ex.colourKeyFlags = ((config->options & DLOP_SRC_COLORKEY)
                                        ? STMFBIO_COLOURKEY_FLAGS_ENABLE
                                        : 0
                                       );
               var_ex.min_colour_key
                    = var_ex.max_colour_key
                    = PIXEL_ARGB (0, config->src_key.r, config->src_key.g,
                                  config->src_key.b);
          }
     }

     if (D_FLAGS_IS_SET( updated, CLRCF_SURFACE_CAPS )) {
          D_DEBUG_AT( STMfbdev_Layer, "    +> surface caps: %spremultiplied alpha\n",
                      ((config->surface_caps & DSCAPS_PREMULTIPLIED)
                       ? "" : "non-") );

          D_ASSUME( D_FLAGS_IS_SET( hw_caps, STMFBIO_VAR_CAPS_PREMULTIPLIED ));

          var_ex.caps |= STMFBIO_VAR_CAPS_PREMULTIPLIED;
          var_ex.premultiplied_alpha = ((config->surface_caps & DSCAPS_PREMULTIPLIED)
                                        ? 1
                                        : 0
                                       );
     }

     if (D_FLAGS_IS_SET( updated, CLRCF_OPACITY )) {
          D_DEBUG_AT( STMfbdev_Layer, "    +> opacity: %d\n", config->opacity );

          D_ASSUME( D_FLAGS_IS_SET( hw_caps, STMFBIO_VAR_CAPS_OPACITY ));

          var_ex.caps |= STMFBIO_VAR_CAPS_OPACITY;
          var_ex.opacity = ((config->options & DLOP_OPACITY)
                            ? config->opacity
                            : 255
                           );
     }

     if (D_FLAGS_IS_SET( updated, CLRCF_ALPHA_RAMP )) {
          D_DEBUG_AT( STMfbdev_Layer, "    +> alpha ramp: 0x%02x 0x%02x 0x%02x 0x%02x\n",
                      config->alpha_ramp[0], config->alpha_ramp[1],
                      config->alpha_ramp[2], config->alpha_ramp[3] );

          D_ASSUME( D_FLAGS_IS_SET( hw_caps, STMFBIO_VAR_CAPS_ALPHA_RAMP ));

          var_ex.caps |= STMFBIO_VAR_CAPS_ALPHA_RAMP;
          var_ex.alpha_ramp[0] = config->alpha_ramp[0];
          var_ex.alpha_ramp[1] = config->alpha_ramp[3];
     }

     if (need_update) {
          if (ioctl( stmfbdev->fd, STMFBIO_SET_PLANEMODE, &plane ) < 0) {
               D_PERROR( "  => STMFBIO_SET_PLANEINFO failed\n" );

               /* not being able to set the new configuration is fatal, I
                  would say. */
               return errno2result( errno );
          }

          shared->cur_planeinfo = plane;
     }

     if (var_ex.caps
         && ioctl( stmfbdev->fd, STMFBIO_SET_VAR_SCREENINFO_EX, &var_ex) < 0)
          D_PERROR( "  => STMFBIO_SET_VAR_SCREENINFO_EX failed\n" );

     /* Update CLUT? */
     if (D_FLAGS_IS_SET( updated, CLRCF_PALETTE ) && palette) {
          struct fb_cmap *cmap = &shared->current_cmap;
          unsigned int    i;

          cmap->len = ((palette->num_entries <= 256)
                       ? palette->num_entries : 256);

          D_DEBUG_AT( STMfbdev_Layer, "    +> palette (%u entries)\n", cmap->len);

          for (i = 0; i < cmap->len; ++i) {
               /* expand to 16 bit */
               cmap->red[i]    = palette->entries[i].r * 0x0101;
               cmap->green[i]  = palette->entries[i].g * 0x0101;
               cmap->blue[i]   = palette->entries[i].b * 0x0101;
               cmap->transp[i] = (0xff - palette->entries[i].a) * 0x0101;
          }

          if (ioctl( stmfbdev->fd, FBIOPUTCMAP, cmap ) < 0)
               D_PERROR( "STMfbdev/Layer: Could not set the palette!\n" );
     }

     return DFB_OK;
}

static DFBResult
stmfbdevRemoveRegion( CoreLayer *layer,
                      void      *driver_data,
                      void      *layer_data,
                      void      *region_data )
{
     const STMfbdev          * const stmfbdev = driver_data;
     STMfbdevLayerSharedData * const shared = layer_data;

     D_DEBUG_AT( STMfbdev_Layer, "%s()\n", __FUNCTION__ );

     D_MAGIC_ASSERT( stmfbdev, STMfbdev );
     D_MAGIC_ASSERT( shared, STMfbdevLayerSharedData );

     (void) stmfbdev;
     (void) shared;

     return DFB_OK;
}

static DFBResult
stmfbdevFlipRegion( CoreLayer             *layer,
                    void                  *driver_data,
                    void                  *layer_data,
                    void                  *region_data,
                    CoreSurface           *surface,
                    DFBSurfaceFlipFlags    flags,
                    CoreSurfaceBufferLock *lock )
{
     const STMfbdev           * const stmfbdev = driver_data;
     STMfbdevLayerSharedData  * const shared = layer_data;
     struct stmfbio_plane_pan  pan;

     D_DEBUG_AT( STMfbdev_Layer, "%s()\n", __FUNCTION__ );

     D_MAGIC_ASSERT( stmfbdev, STMfbdev );
     D_MAGIC_ASSERT( shared, STMfbdevLayerSharedData );

     D_DEBUG_AT( STMfbdev_Layer, "  -> buffer: %p\n", lock->buffer );
     D_DEBUG_AT( STMfbdev_Layer,
                 "  -> offset 0x%.8lx, pitch %d, addr %p, phys 0x%.8lx\n",
                 lock->offset, lock->pitch, lock->addr, lock->phys);

     pan.layerid = shared->cur_planeinfo.layerid;
     pan.activate = (((flags & DSFLIP_WAITFORSYNC) == DSFLIP_ONSYNC)
                     ? STMFBIO_ACTIVATE_VBL : STMFBIO_ACTIVATE_IMMEDIATE);
     pan.baseaddr = lock->phys;

     if (((flags & DSFLIP_WAITFORSYNC) == DSFLIP_WAITFORSYNC)
         && !dfb_config->pollvsync_after)
          dfb_screen_wait_vsync( layer->screen );

     if (ioctl( stmfbdev->fd, STMFBIO_PAN_PLANE, &pan ) < 0) {
          D_PERROR( "STMfbdev/Layer: Panning display to %lx (%s) failed\n",
                    pan.baseaddr,
                    (pan.activate & STMFBIO_ACTIVATE_VBL) ? "vbl" : "now");

          return errno2result( errno );
     }

     if ((flags & DSFLIP_WAIT)
         && (dfb_config->pollvsync_after || !(flags & DSFLIP_ONSYNC)))
          dfb_screen_wait_vsync( dfb_screens_at( DSCID_PRIMARY ) );

     dfb_surface_flip( surface, false );

     shared->cur_planeinfo.config.baseaddr = pan.baseaddr;

     return DFB_OK;
}

static DFBResult
stmfbdevGetLevel( CoreLayer *layer,
                  void      *driver_data,
                  void      *layer_data,
                  int       *level )
{
     const STMfbdev                   * const stmfbdev = driver_data;
     const STMfbdevLayerSharedData    * const shared = layer_data;
     struct stmfbio_var_screeninfo_ex  var_ex;

     D_DEBUG_AT( STMfbdev_Layer, "%s()\n", __FUNCTION__ );

     D_MAGIC_ASSERT( stmfbdev, STMfbdev );
     D_MAGIC_ASSERT( shared, STMfbdevLayerSharedData );

     (void) stmfbdev;

     if (!D_FLAGS_IS_SET( shared->orig_varex.caps, STMFBIO_VAR_CAPS_ZPOSITION ))
          return DFB_UNSUPPORTED;

     var_ex.layerid = shared->orig_varex.layerid;
     if (ioctl( stmfbdev->fd, STMFBIO_GET_VAR_SCREENINFO_EX, &var_ex ) < 0 )
          return errno2result( errno );

     *level = (int) var_ex.z_position;

     return DFB_OK;
}

static DFBResult
stmfbdevSetLevel( CoreLayer *layer,
                  void      *driver_data,
                  void      *layer_data,
                  int        level )
{
     const STMfbdev                   * const stmfbdev = driver_data;
     const STMfbdevLayerSharedData    * const shared = layer_data;
     struct stmfbio_var_screeninfo_ex  var_ex;

     D_DEBUG_AT( STMfbdev_Layer, "%s()\n", __FUNCTION__ );

     D_MAGIC_ASSERT( stmfbdev, STMfbdev );
     D_MAGIC_ASSERT( shared, STMfbdevLayerSharedData );

     (void) stmfbdev;

     D_DEBUG_AT( STMfbdev_Layer, "  -> %d\n", level );

     if (!D_FLAGS_IS_SET( shared->orig_varex.caps, STMFBIO_VAR_CAPS_ZPOSITION ))
          return DFB_UNSUPPORTED;

     var_ex.layerid = shared->orig_varex.layerid;
     var_ex.activate = STMFBIO_ACTIVATE_IMMEDIATE;
     var_ex.caps = STMFBIO_VAR_CAPS_ZPOSITION;
     var_ex.z_position = (level < 0) ? 0 : level;

     if (ioctl( stmfbdev->fd, STMFBIO_SET_VAR_SCREENINFO_EX, &var_ex ) < 0 )
          return errno2result( errno );

     return DFB_OK;
}

static DFBResult
stmfbdevSetColorAdjustment( CoreLayer          *layer,
                            void               *driver_data,
                            void               *layer_data,
                            DFBColorAdjustment *adjustment )
{
     const STMfbdev                   * const stmfbdev = driver_data;
     const STMfbdevLayerSharedData    * const shared = layer_data;
     struct stmfbio_var_screeninfo_ex  var_ex;

     D_DEBUG_AT( STMfbdev_Layer, "%s()\n", __FUNCTION__ );

     D_MAGIC_ASSERT( stmfbdev, STMfbdev );
     D_MAGIC_ASSERT( shared, STMfbdevLayerSharedData );

     (void) stmfbdev;

     if (D_FLAGS_INVALID( adjustment->flags, shared->supported_adjustments ))
          return DFB_INVARG;

     var_ex.layerid = shared->orig_varex.layerid;
     var_ex.activate = STMFBIO_ACTIVATE_IMMEDIATE;
     var_ex.caps = 0;

     D_DEBUG_AT( STMfbdev_Layer, "  -> flags: %x\n", adjustment->flags );

     if (D_FLAGS_IS_SET( adjustment->flags, DCAF_BRIGHTNESS )) {
          D_DEBUG_AT( STMfbdev_Layer, "    +> brightness: 0x%04x\n",
                      adjustment->brightness );

          if (D_FLAGS_IS_SET( shared->orig_varex.caps,
                              STMFBIO_VAR_CAPS_BRIGHTNESS )) {
               var_ex.caps |= STMFBIO_VAR_CAPS_BRIGHTNESS;
               var_ex.brightness = adjustment->brightness >> 8;
          }
          else {
               var_ex.caps |= STMFBIO_VAR_CAPS_GAIN;
               /* 0-0x8000 -> 0-255 */
               var_ex.gain = ((adjustment->brightness > 0x8000)
                              ? 255
                              : (((adjustment->brightness >> 8) * 255) / 128)
                             );
          }
     }

     if (D_FLAGS_IS_SET( adjustment->flags, DCAF_CONTRAST )) {
          D_DEBUG_AT( STMfbdev_Layer, "    +> contrast: 0x%04x\n",
                      adjustment->contrast );

          var_ex.caps |= STMFBIO_VAR_CAPS_CONTRAST;
          var_ex.contrast = adjustment->contrast >> 8;
     }

     if (D_FLAGS_IS_SET( adjustment->flags, DCAF_HUE )) {
          D_DEBUG_AT( STMfbdev_Layer, "    +> hue: 0x%04x\n",
                      adjustment->hue );

          var_ex.caps |= STMFBIO_VAR_CAPS_TINT;
          var_ex.tint = adjustment->hue >> 8;
     }

     if (D_FLAGS_IS_SET( adjustment->flags, DCAF_SATURATION )) {
          D_DEBUG_AT( STMfbdev_Layer, "    +> saturation: 0x%04x\n",
                      adjustment->saturation );

          var_ex.caps |= STMFBIO_VAR_CAPS_SATURATION;
          var_ex.saturation = adjustment->saturation >> 8;
     }

     if (ioctl( stmfbdev->fd, STMFBIO_SET_VAR_SCREENINFO_EX, &var_ex ) < 0 )
          return errno2result( errno );

     return DFB_OK;
}


DisplayLayerFuncs _g_stmfbdevLayerFuncs = {
     .LayerDataSize  = stmfbdevLayerDataSize,
     .RegionDataSize = stmfbdevRegionDataSize,

     .InitLayer     = stmfbdevInitLayer,
     .ShutdownLayer = stmfbdevShutdownLayer,

     .TestRegion   = stmfbdevTestRegion,
     .AddRegion    = stmfbdevAddRegion,
     .SetRegion    = stmfbdevSetRegion,
     .RemoveRegion = stmfbdevRemoveRegion,
     .FlipRegion   = stmfbdevFlipRegion,

     .GetLevel = stmfbdevGetLevel,
     .SetLevel = stmfbdevSetLevel,

     .SetColorAdjustment = stmfbdevSetColorAdjustment,
};
