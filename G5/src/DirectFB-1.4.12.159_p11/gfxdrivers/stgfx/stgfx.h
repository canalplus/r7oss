/*
   (c) Copyright 2007,2009  STMicroelectronics.

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

#include <linux/stmfb.h>
#include <core/coretypes.h>
#include <core/screens.h>
#include <core/layers.h>
#include "stgfx2/stgfx_screen.h"
#include "stgfx2/stgfx_primary.h"


#define likely(x)       __builtin_expect(!!(x),1)
#define unlikely(x)     __builtin_expect(!!(x),0)

/* Structure for storing information about device state */
typedef struct {
  STMFB_GFXMEMORY_PARTITION srcMemBase;
  unsigned long             srcPitch;
  unsigned long             srcOffset;
  SURF_FMT                  srcFormat;
  unsigned long             srcPhys;
  STMFB_GFXMEMORY_PARTITION dstMemBase;
  unsigned long             dstPitch;
  unsigned long             dstOffset;
  SURF_FMT                  dstFormat;
  unsigned long             dstPhys;
  unsigned long blitColour;
  unsigned long drawColour;
  unsigned long srcColourKey;
  unsigned long dstColourKey;
  unsigned long globalAlpha;

  DFBSurfaceBlendFunction srcBlendFunc;
  DFBSurfaceBlendFunction dstBlendFunc;

  DFBSurfaceDrawingFlags  drawingFlags;
  DFBSurfaceBlittingFlags blittingFlags;

  CorePalette   *palette;

  bool           bBlendFromColorAlpha;
  bool           bBlendFromSrcAlpha;
  bool           bSrcPremultiply;
  bool           bPremultColor;
  bool           bNeedCLUTReload;

  /* stmfbdev system */
  CoreSurfacePool *pool;

  CoreSurfacePool *aux_pools[STMFBGP_GFX_LAST - STMFBGP_GFX_FIRST + 1];
  /* for exclusive use by bdisp_surface_pool.c */
  unsigned int aux_pool_index;
} STGFXDeviceData;


/* Structure for storing information about driver state */
typedef struct {
  CoreDFB         *core;
  STGFXDeviceData *device_data;
  int              fd;

  struct _STGFX2ScreenData screenData;
  struct _STGFX2LayerData  layerData;

} STGFXDriverData;
