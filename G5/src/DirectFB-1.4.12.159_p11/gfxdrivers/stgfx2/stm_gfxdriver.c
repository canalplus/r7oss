/*
   ST Microelectronics BDispII driver - the DirectFB interface

   (c) Copyright 2003, 2007-2010   STMicroelectronics Ltd.

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

#include <fbdev/fbdev.h>
#include <stmfbdev/stmfbdev.h>

#include <gfx/convert.h>
#include <gfx/util.h>

#include <core/graphics_driver.h>

#include "stgfx2_features.h"

#include "stm_gfxdriver.h"

#include "bdisp_features.h"
#include "bdisp_state.h"
#include "bdisp_accel.h"
#include "stgfx_screen.h"
#include "stgfx_primary.h"
#include "bdisp_surface_pool.h"



DFB_GRAPHICS_DRIVER (stgfx2);




#define MEMORY64MB_SIZE         (1<<26)
#define _ALIGN_DOWN(addr,size)	((addr)&(~((size)-1)))

static bool
__attribute__((warn_unused_result))
stgfx2_check_memory_constraints (CoreGraphicsDevice     * const device,
                                 const STGFX2DeviceData * const stdev)
{
  /* fbmem could in theory cross a 64MB boundary, stmfb doesn't go through any
     efforts whatsoever to prevent such from happening. It should be changed
     to add a check for configurations which end up crossing a boundary, but
     even then I don't think I'll remove the check from here, since otherwise
     we add a (soft) dependency on an up-to-date version of stmfb. */
  unsigned long fb_base = dfb_gfxcard_memory_physical (device, 0);
  unsigned long fb_end  = fb_base + dfb_gfxcard_memory_length () - 1;
  if (_ALIGN_DOWN (fb_base, MEMORY64MB_SIZE) != _ALIGN_DOWN (fb_end, MEMORY64MB_SIZE))
    {
      fprintf (stderr,
               "BDisp/BLT: Your framebuffer memory configuration"
               " is incorrect!\n"
               "  Can't use hardware acceleration at all!\n"
               "  Physical framebuffer memory must not cross a 64MB bank!\n"
               "  Please fix your configuration!\n");

      return false;
    }

  return true;
}


static int
driver_probe (CoreGraphicsDevice * const device)
{
  switch (dfb_system_type ())
    {
    case CORE_FBDEV:
    case CORE_STMFBDEV:
      if (dfb_gfxcard_get_accelerator (device) != FB_ACCEL_ST_BDISP_USER)
        break;

      D_DEBUG ("DirectFB/stgfx2: Found STM BDispII w/ user space AQs\n");
      return 1;

    default:
      break;
    }

  return 0;
}

static void
driver_get_info (CoreGraphicsDevice * const device,
                 GraphicsDriverInfo * const info)
{
  /* fill driver info structure */
  snprintf (info->name, DFB_GRAPHICS_DRIVER_INFO_NAME_LENGTH,
            "STM BDispII w/ user space AQs");

  snprintf (info->vendor, DFB_GRAPHICS_DRIVER_INFO_VENDOR_LENGTH,
            "ST Microelectronics");

  info->version.major = 0;
  info->version.minor = 9;

  /* Tell DirectFB how big our device and driver structures are */
  info->driver_data_size = sizeof (STGFX2DriverData);
  info->device_data_size = sizeof (STGFX2DeviceData);
}


static DFBResult
driver_init_driver (CoreGraphicsDevice  * const device,
                    GraphicsDeviceFuncs * const funcs,
                    void                * const driver_data,
                    void                * const device_data,
                    CoreDFB             * const core)
{
  STGFX2DeviceData * const stdev = device_data;
  STGFX2DriverData * const stdrv = driver_data;
  DFBResult         res;

  stdrv->core        = core;
  stdrv->device      = device;
  stdrv->device_data = device_data;

  D_INFO ("DirectFB/stgfx2: stdev/stdrv: %p / %p\n", stdev, stdrv);

  /* check memory constraints */
  if (!stgfx2_check_memory_constraints (device, stdev))
    return DFB_UNSUPPORTED;

  stdrv->mmio_base = dfb_gfxcard_map_mmio (device, 0, -1);
  if (!stdrv->mmio_base)
    {
      D_ERROR ("DirectFB/stgfx2: couldn't mmap mmio!\n");
      return DFB_IO;
    }

  /* we will need the framebuffer file descriptor. */
  stdrv->fd = -1;
  if (dfb_system_type () == CORE_FBDEV)
    {
      const FBDev * const dfb_fbdev = dfb_system_data ();
      D_INFO ("DirectFB/stgfx2: running in fbdev\n");

      if (!dfb_fbdev)
        {
          D_ERROR ("DirectFB/stgfx2: No System data?!\n");
          res = DFB_NOCONTEXT;
          goto out1;
        }

      stdrv->fd = dfb_fbdev->fd;
    }
  else if (dfb_system_type () == CORE_STMFBDEV)
    {
      const STMfbdev * const stmfbdev = dfb_system_data ();
      D_INFO ("DirectFB/stgfx2: running in stmfbdev\n");

      D_MAGIC_ASSERT (stmfbdev, STMfbdev);
      if (!stmfbdev)
        {
          D_ERROR ("DirectFB/stgfx2: No System data?!\n");
          res = DFB_NOCONTEXT;
          goto out1;
        }

      stdrv->fd = stmfbdev->fd;
    }
  else
    {
      /* shouldn't be reached, though. Because of our driver_probe(). */
      D_ERROR ("DirectFB/stgfx2: Only supported on FBDev and STMfbdev systems\n");
      res = DFB_UNSUPPORTED;
      goto out1;
    }

  if (stdrv->fd < 0)
    {
      D_ERROR ("DirectFB/stgfx2: No fd?!");
      res = DFB_IO;
      goto out1;
    }

  stdrv->accel_type = dfb_gfxcard_get_accelerator (device);

  if (!dfb_core_is_master (core))
    {
      Stgfx2Clut palette;

      /* map BDisp kernel driver's shared area... */
      stdrv->bdisp_shared = mmap (NULL,
                                  sysconf (_SC_PAGESIZE),
                                  PROT_READ | PROT_WRITE,
                                  MAP_SHARED, stdrv->fd,
                                  stdev->bdisp_shared_info.physical);
      if (stdrv->bdisp_shared == MAP_FAILED)
        {
          D_PERROR ("DirectFB/stgfx2: Could not map shared area!\n");
          stdrv->bdisp_shared = NULL;
          res = DFB_FAILURE;
          goto out1;
        }

      /* ...and the node list... */
      stdrv->bdisp_nodes = mmap (NULL,
                                 (stdev->bdisp_shared_info.size
                                  - sysconf (_SC_PAGESIZE)),
                                 PROT_READ | PROT_WRITE,
                                 MAP_SHARED, stdrv->fd,
                                 (stdev->bdisp_shared_info.physical
                                  + sysconf (_SC_PAGESIZE)));
      if (stdrv->bdisp_nodes == MAP_FAILED)
        {
          D_PERROR ("DirectFB/stgfx2: Could not map nodes area!\n");
          stdrv->bdisp_nodes = NULL;
          res = DFB_FAILURE;
          goto out2;
        }
      D_INFO ("DirectFB/stgfx2: shared area/nodes mmap()ed to %p / %p\n",
              stdrv->bdisp_shared, stdrv->bdisp_nodes);

      /* ...and the CLUTs. */
      for (palette = SG2C_NORMAL; palette < SG2C_DYNAMIC_COUNT; ++palette)
        {
          stdrv->clut_virt[palette]
            = dfb_gfxcard_memory_virtual (device, stdev->clut_offset[palette]);
          D_INFO ("DirectFB/stgfx2: CLUT mmap()ed to %p\n",
                  stdrv->clut_virt[palette]);
        }
    }


  /* setup the driver state */
  funcs->EngineReset   = bdisp_aq_EngineReset;
  funcs->EngineSync    = bdisp_aq_EngineSync;
  funcs->CheckState    = bdisp_aq_CheckState;
  funcs->SetState      = bdisp_aq_SetState;
  funcs->EmitCommands  = bdisp_aq_EmitCommands;
#ifdef STGFX2_IMPLEMENT_WAITSERIAL
  funcs->GetSerial     = bdisp_aq_GetSerial;
  funcs->WaitSerial    = bdisp_aq_WaitSerial;
#endif
  funcs->FillRectangle = bdisp_aq_FillRectangle;
  funcs->DrawRectangle = bdisp_aq_DrawRectangle;
  funcs->Blit          = bdisp_aq_Blit;
  funcs->StretchBlit   = bdisp_aq_StretchBlit;
  funcs->Blit2         = bdisp_aq_Blit2_nop;

  if (dfb_system_type () == CORE_STMFBDEV)
    {
      extern const SurfacePoolFuncs _g_stmfbdevSurfacePoolFuncs;
      extern ScreenFuncs            _g_stmfbdevScreenFuncs;
      extern DisplayLayerFuncs      _g_stmfbdevLayerFuncs;

      STMfbdev   * const stmfbdev = dfb_system_data ();
      CoreScreen *screen;

      D_MAGIC_ASSERT (stmfbdev, STMfbdev);

      if (!dfb_core_is_master (stdrv->core))
        dfb_surface_pool_join (stdrv->core, stdev->pool,
                               &_g_stmfbdevSurfacePoolFuncs);

      /* Register primary screen functions */
      screen = dfb_screens_register (NULL, stmfbdev,
                                     &_g_stmfbdevScreenFuncs);

      /* Register primary layer functions */
      dfb_layers_register (screen, stmfbdev, &_g_stmfbdevLayerFuncs);
    }
  else if (dfb_system_type() == CORE_FBDEV)
    {
      dfb_screens_hook_primary (device, driver_data,
                                &stgfxPrimaryScreenFuncs_stgfx2,
                                &stdrv->screenData.orig_funcs,
                                &stdrv->screenData.orig_data);

      dfb_layers_hook_primary (device, driver_data,
                               &stgfxPrimaryLayerFuncs_stgfx2,
                               &stdrv->layerData.orig_funcs,
                               &stdrv->layerData.orig_data);
    }

  if (!dfb_core_is_master (stdrv->core))
    stgfx2_surface_pool_join (stdrv->core, stdev->aux_pools,
                              D_ARRAY_SIZE (stdev->aux_pools));

  return DFB_OK;

out2:
  if (!dfb_core_is_master (core))
    munmap ((void *) stdrv->bdisp_shared, 4096);
out1:
  dfb_gfxcard_unmap_mmio (device, stdrv->mmio_base, -1);
  return res;
}

static DFBResult
driver_init_device (CoreGraphicsDevice * const device,
                    GraphicsDeviceInfo * const device_info,
                    void               * const driver_data,
                    void               * const device_data)
{
  STGFX2DeviceData * const stdev = device_data;
  STGFX2DriverData * const stdrv = driver_data;
  DFBResult         res;

  snprintf (device_info->name, DFB_GRAPHICS_DEVICE_INFO_NAME_LENGTH,
            "BDispII w/ user space AQs");

  snprintf (device_info->vendor, DFB_GRAPHICS_DEVICE_INFO_VENDOR_LENGTH,
            "ST Microelectronics");

  dfb_config->pollvsync_after = true;

  /* give DirectFB info about our surface limitations. need to do this
     before bdisp_aq_initialize(), because it has its own limitations, which
     are different, depending on the platform. FIXME: should really make
     this dependent on the platform, i.e. without an #if 0... */
#if 0
  /* real BDispII limitations */
  device_info->limits.surface_byteoffset_alignment =  4;
  device_info->limits.surface_bytepitch_alignment  =  2;
#else
  /* the 3D unit on 7108 has these limitations */
  device_info->limits.surface_byteoffset_alignment = 64;
  device_info->limits.surface_bytepitch_alignment  =  8;
  /* JPEGHWDEC requires surfaces aligned on 256-bytes boundaries. Surfaces
     for uncompressed BMP must have a pitch which is a multiple of 4. */
  device_info->limits.surface_byteoffset_alignment = 256;
#endif
  /* for YUV 4:2:x formats, the pitch must be even, so that we can safely
     deduct the chroma pitch by dividing the luma pitch by two. */
  device_info->limits.surface_pixelpitch_alignment =  2;

  res = bdisp_aq_initialize (device,
                             device_info,
                             (STGFX2DriverData *) driver_data,
                             (STGFX2DeviceData *) device_data);
  if (res != DFB_OK)
    return res;

  /* Tell DirectFB about what operations we may be able to do. */
  device_info->caps.accel    = (STGFX2_VALID_DRAWINGFUNCTIONS
                                | STGFX2_VALID_BLITTINGFUNCTIONS);
  device_info->caps.drawing  = stdev->features.dfb_drawflags;
  device_info->caps.blitting = stdev->features.dfb_blitflags;
#ifdef STGFX2_SUPPORT_HW_CLIPPING
  device_info->caps.flags    = CCF_CLIPPING;
  device_info->caps.clip     = DFXL_STRETCHBLIT;
#endif
  device_info->caps.flags |= CCF_RENDEROPTS;
  device_info->limits.src_max.w
    = device_info->limits.src_max.h
    = device_info->limits.dst_max.w
    = device_info->limits.dst_max.h
    = 4095;

  /* Need to update the surface limitations again, because
     bdisp_aq_initialize() might have changed them, depending on the platform.
     FIXME: should really make this dependent on the platform, i.e. without an
     #if 0... */
#if 0
  /* real BDispII limitations */
  device_info->limits.surface_byteoffset_alignment =  4;
  device_info->limits.surface_bytepitch_alignment  =  2;
#else
  /* the 3D unit on 7108 has these limitations */
  device_info->limits.surface_byteoffset_alignment = 64;
  device_info->limits.surface_bytepitch_alignment  =  8;
  /* JPEGHWDEC requires surfaces aligned on 256-bytes boundaries. Surfaces
     for uncompressed BMP must have a pitch which is a multiple of 4. */
  device_info->limits.surface_byteoffset_alignment = 256;
#endif
  /* for YUV 4:2:x formats, the pitch must be even, so that we can safely
     deduct the chroma pitch by dividing the luma pitch by two. */
  device_info->limits.surface_pixelpitch_alignment =  2;

  if (dfb_system_type () == CORE_STMFBDEV)
    {
      extern const SurfacePoolFuncs _g_stmfbdevSurfacePoolFuncs;
      dfb_surface_pool_initialize (stdrv->core, &_g_stmfbdevSurfacePoolFuncs,
                                   &stdev->pool);
    }
  /* aux pools */
  stgfx2_surface_pool_init (stdrv->core, stdev->aux_pools,
                            D_ARRAY_SIZE (stdev->aux_pools));

  return DFB_OK;
}


static void
driver_close_device (CoreGraphicsDevice * const device,
                     void               * const driver_data,
                     void               * const device_data)
{
  STGFX2DriverData * const stdrv = driver_data;
  STGFX2DeviceData * const stdev = device_data;

  if (dfb_system_type () == CORE_STMFBDEV)
    dfb_surface_pool_destroy (stdev->pool);
  stgfx2_surface_pool_destroy (stdrv->core, stdev->aux_pools,
                               D_ARRAY_SIZE (stdev->aux_pools));
}

static void
driver_close_driver (CoreGraphicsDevice * const device,
                     void               * const driver_data)
{
  STGFX2DriverData * const stdrv = driver_data;
  STGFX2DeviceData * const stdev = stdrv->device_data;

  if (!dfb_core_is_master (stdrv->core))
    {
      if (dfb_system_type () == CORE_STMFBDEV)
        dfb_surface_pool_leave (stdev->pool);
      stgfx2_surface_pool_leave (stdrv->core, stdev->aux_pools,
                                 D_ARRAY_SIZE (stdev->aux_pools));
    }

  if (stdrv->bdisp_shared)
    {
      const STMFBBDispSharedArea __attribute__((unused)) * const shared
        = stdrv->bdisp_shared;
      unsigned long long ops;

      D_INFO ("DirectFB/stgfx2: %u starts, %u (%u/%u) interrupts, %u wait_idle, %u wait_next, %u idle\n",
              shared->num_starts, shared->num_irqs, shared->num_node_irqs,
              shared->num_lna_irqs,
              shared->num_wait_idle, shared->num_wait_next, shared->num_idle);

      ops = ((unsigned long long) shared->num_ops_hi) << 32;
      ops += shared->num_ops_lo;
      D_INFO ("DirectFB/stgfx2: %llu ops, %llu ops/start, %llu ops/idle, %u starts/idle\n",
              ops,
              shared->num_starts ? ops / shared->num_starts : 0,
              shared->num_idle   ? ops / shared->num_idle   : 0,
              shared->num_idle   ? shared->num_starts / shared->num_idle : 0);

      munmap ((void *) stdrv->bdisp_shared, 4096);
    }
  if (stdrv->bdisp_nodes)
    munmap ((void *) stdrv->bdisp_nodes,
            stdrv->device_data->bdisp_shared_info.size - 4096);

  dfb_gfxcard_unmap_mmio (device, stdrv->mmio_base, -1);
}
