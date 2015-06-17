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

#include "config.h"

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <sys/mman.h>
#include <fcntl.h>
#include <sys/ioctl.h>

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

#include <linux/stm/bdisp2_shared.h>

#include "bdisp2/bdispII_driver_features.h"
#include "stgfx2_features.h"

#include "bdisp_features.h"
#include "stm_gfxdriver.h"
#include "stgfx2_engine.h"

#include "directfb_state_glue.h"
#include "bdisp2/bdisp_accel.h"
#include "stgfx_screen.h"
#include "stgfx_primary.h"

#include "bdisp2_directfb_glue.h"


DFB_GRAPHICS_DRIVER (stgfx2);




static DFBResult
__attribute__((warn_unused_result))
stgfx2_check_memory_constraints (CoreGraphicsDevice     * const device,
                                 const STGFX2DeviceData * const devdata)
{
  /* fbmem could in theory cross a 64MB boundary, stmfb doesn't go through any
     efforts whatsoever to prevent such from happening. It should be changed
     to add a check for configurations which end up crossing a boundary, but
     even then I don't think I'll remove the check from here, since otherwise
     we add a (soft) dependency on an up-to-date version of stmfb. */
  unsigned long fb_base = dfb_gfxcard_memory_physical (device, 0);
  unsigned long fb_end  = fb_base + dfb_gfxcard_memory_length () - 1;
  if (bdisp2_check_memory_constraints (&devdata->stdev, fb_base, fb_end))
    {
      fprintf (stderr,
               "BDisp/BLT: Your framebuffer memory configuration"
               " is incorrect!\n"
               "  Can't use hardware acceleration at all!\n"
               "  Physical framebuffer memory must not cross a 64MB bank!\n"
               "  Please fix your configuration!\n");

      return DFB_UNSUPPORTED;
    }

  return DFB_OK;
}


static int
driver_probe (CoreGraphicsDevice * const device)
{
  switch (dfb_system_type ())
    {
    case CORE_FBDEV:
    case CORE_STMFBDEV:
      if (dfb_gfxcard_get_accelerator (device) != 102)
        break;

      D_DEBUG ("DirectFB/stgfx2: Found STM BDispII w/ user space AQs\n");
      return 1;

    case CORE_ANY:
    case CORE_X11:
    case CORE_OSX:
    case CORE_SDL:
    case CORE_VNC:
    case CORE_DEVMEM:
    case CORE_TI_CMEM:
    case CORE_MESA:
    case CORE_X11VDPAU:
    case CORE_PVR2D:
    case CORE_CARE1:
    case CORE_ANDROID:
    case CORE_EGL:
#if HAVE_DECL_CORE_DRMKMS
    case CORE_DRMKMS:
#endif
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

  info->version.major = 1;
  info->version.minor = 0;

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
  STGFX2DriverData * const drvdata = driver_data;
  STGFX2DeviceData * const devdata = device_data;
  DFBResult         res;
  CoreSystemType    system_type;
  drvdata->device = device;
  static const char *devnames[] = {
    "/dev/stm-bdispII.1.1", /* STiH415 */
    "/dev/stm-bdispII.1.4", /* STi7108 */
    "/dev/stm-bdispII.1",   /* STi7105 */
    "/dev/stm-bdispII.0",   /* STi7109 */
    "/dev/stm-bdispII.0.0", /* Single Blitter */
  };
  unsigned int i;
  long page_size;

  D_INFO ("DirectFB/stgfx2: devdata/drvdata: %p / %p\n", devdata, drvdata);

  /* we will need the kernel graphics driver or framebuffer file descriptor.*/
  drvdata->fd_gfx = -1;

  /* Device file naming is:
       /dev/stm-bdispII.<idx of BDisp (system-wide)>.<idx of AQ (system-wide)>

     Therefore, on a system with 2 BDispII and 4 AQs on each (STi7108,
     FLi7510 FLi7540), we get:
       1st BDisp, AQ1 .. AQ4: /dev/stm-bdispII.0.0
                              /dev/stm-bdispII.0.1
                              /dev/stm-bdispII.0.2
                              /dev/stm-bdispII.0.3
       2nd BDisp, AQ1 .. AQ4: /dev/stm-bdispII.1.4
                              /dev/stm-bdispII.1.5
                              /dev/stm-bdispII.1.6
                              /dev/stm-bdispII.1.7
     On a system with 2 BDispII and 1 AQ on each (STiH415 FLi7610), we get:
       1st BDisp, AQ1: /dev/stm-bdispII.0.0
       2nd BDisp, AQ1: /dev/stm-bdispII.1.1
     On a system with 1 BDispII and 4 AQs (STi7105), we get:
       1st BDisp, AQ1 .. AQ4: /dev/stm-bdispII.0
                              /dev/stm-bdispII.1
                              /dev/stm-bdispII.2
                              /dev/stm-bdispII.3
     On a system with 1 BDispII and 1 AQ (STi7109), we get:
       1st BDisp, AQ1: /dev/stm-bdispII.0


     If we have more than one BDispII in the system, we use the 1st AQ of
     the 2nd IP:
       - on STiH415 /dev/stm-bdispII.1.1
       - on STi7108 /dev/stm-bdispII.1.4

     If we have only one IP in the system, we use the 2nd AQ:
       - on STi7105 /dev/stm-bdispII.1

     Otherwise (one IP, one AQ), we use that:
       - on STi7109 /dev/stm-bdispII.0
  */
  if ((page_size = (long)sysconf(_SC_PAGESIZE)) < 0)
    return DFB_BUG;

  for (i = 0; i < D_ARRAY_SIZE(devnames); ++i)
    {
      if (access (devnames[i], O_RDWR) == 0)
        {
          drvdata->fd_gfx = open (devnames[i], O_RDWR);
          if (drvdata->fd_gfx == -1)
            return DFB_IO;

          drvdata->stdrv.mmio_base = mmap (NULL, page_size,
                                           PROT_READ | PROT_WRITE, MAP_SHARED,
                                           drvdata->fd_gfx,
                                           0); /* FIXME */
          if (drvdata->stdrv.mmio_base == MAP_FAILED)
            {
              D_PERROR ("DirectFB/stgfx2: Could not mmap MMIO region "
                        "(offset %d, length %lld)!\n",
                        0, (long long) page_size);
              drvdata->stdrv.mmio_base = NULL;
              return DFB_IO;
            }

          D_INFO ("DirectFB/stgfx2: using %s\n", devnames[i]);
          drvdata->stmfb_acceleration = false;
          break;
        }
    }
  if (drvdata->fd_gfx == -1)
    {
      system_type = dfb_system_type ();
      if (system_type == CORE_FBDEV
          || system_type == CORE_STMFBDEV)
        {
          if (system_type == CORE_FBDEV)
            {
              const FBDev * const dfb_fbdev = dfb_system_data ();
              D_INFO ("DirectFB/stgfx2: running in fbdev\n");

              if (!dfb_fbdev)
                  D_ERROR ("DirectFB/stgfx2: No System data?!\n");
              else
                  drvdata->fd_gfx = dfb_fbdev->fd;
            }
          else if (system_type == CORE_STMFBDEV)
            {
              const STMfbdev * const stmfbdev = dfb_system_data ();
              D_INFO ("DirectFB/stgfx2: running in stmfbdev\n");

              D_MAGIC_ASSERT (stmfbdev, STMfbdev);
              if (!stmfbdev)
                  D_ERROR ("DirectFB/stgfx2: No System data?!\n");
              else
                  drvdata->fd_gfx = stmfbdev->fd;
            }
        }

      if (drvdata->fd_gfx == -1)
        return DFB_NOCONTEXT;

      drvdata->stdrv.mmio_base = dfb_gfxcard_map_mmio (device, 0, -1);
      if (!drvdata->stdrv.mmio_base)
        {
          D_ERROR ("DirectFB/stgfx2: couldn't mmap mmio from stm-blitter "
                   "or stmfb!\n");
          return DFB_IO;
        }

      drvdata->stmfb_acceleration = true;
    }

  drvdata->accel_type = dfb_gfxcard_get_accelerator (device);

  if (!dfb_core_is_master (core))
    {
      enum bdisp2_palette_type palette;

      /* check memory constraints */
      res = stgfx2_check_memory_constraints (device, devdata);
      if (res != DFB_OK)
        goto out1;

      /* map BDisp kernel driver's shared area... */
      drvdata->stdrv.bdisp_shared = mmap (NULL,
                                          page_size,
                                          PROT_READ | PROT_WRITE,
                                          MAP_SHARED, drvdata->fd_gfx,
                                          devdata->bdisp_shared_cookie.cookie);
      if (drvdata->stdrv.bdisp_shared == MAP_FAILED)
        {
          D_PERROR ("DirectFB/stgfx2: Could not map shared area!\n");
          drvdata->stdrv.bdisp_shared = NULL;
          res = DFB_FAILURE;
          goto out1;
        }

      /* ...and the node list... */
      drvdata->stdrv.bdisp_nodes.base = drvdata->stdrv.bdisp_shared->nodes_phys;
      drvdata->stdrv.bdisp_nodes.size = drvdata->stdrv.bdisp_shared->nodes_size;
      drvdata->stdrv.bdisp_nodes.memory = mmap (NULL,
                                                drvdata->stdrv.bdisp_nodes.size,
                                                PROT_READ | PROT_WRITE,
                                                MAP_SHARED, drvdata->fd_gfx,
                                                drvdata->stdrv.bdisp_nodes.base);
      if (drvdata->stdrv.bdisp_nodes.memory == MAP_FAILED)
        {
          D_PERROR ("DirectFB/stgfx2: Could not map nodes area!\n");
          drvdata->stdrv.bdisp_nodes.memory = NULL;
          drvdata->stdrv.bdisp_nodes.base = 0;
          drvdata->stdrv.bdisp_nodes.size = 0;
          res = DFB_FAILURE;
          goto out2;
        }
      D_INFO ("DirectFB/stgfx2: shared area/nodes mmap()ed to %p / %p\n",
              drvdata->stdrv.bdisp_shared, drvdata->stdrv.bdisp_nodes.memory);

      /* ...and the CLUTs. */
      for (palette = SG2C_NORMAL; palette < SG2C_DYNAMIC_COUNT; ++palette)
        {
          unsigned long offset = (devdata->stdev.clut_phys[palette]
                                  - dfb_gfxcard_memory_physical (device, 0));
          drvdata->stdrv.clut_virt[palette]
            = dfb_gfxcard_memory_virtual (device, offset);
          D_INFO ("DirectFB/stgfx2: CLUT mmap()ed to %p\n",
                  drvdata->stdrv.clut_virt[palette]);
        }
    }


  /* setup the driver state */
  funcs->EngineReset   = bdisp_aq_EngineReset;
  funcs->EngineSync    = bdisp_aq_EngineSync;
  funcs->CheckState    = bdisp_aq_CheckState;
  funcs->SetState      = bdisp_aq_SetState;
  funcs->StateInit     = bdisp_aq_StateInit;
  funcs->StateDestroy  = bdisp_aq_StateDestroy;
  funcs->EmitCommands  = bdisp_aq_EmitCommands;
#ifdef STGFX2_IMPLEMENT_WAITSERIAL
  funcs->GetSerial     = bdisp_aq_GetSerial;
  funcs->WaitSerial    = bdisp_aq_WaitSerial;
#endif
  funcs->FillRectangle = bdisp_aq_FillRectangle;
  funcs->DrawRectangle = bdisp_aq_DrawRectangle;
  funcs->Blit          = bdisp_aq_Blit;
  funcs->StretchBlit   = bdisp_aq_StretchBlit;
  funcs->Blit2         = bdisp_aq_Blit2;

  if (dfb_system_type() == CORE_FBDEV)
    {
      stmfb_screen_hook_fbdev ();

      stmfb_layer_hook_fbdev ();
    }

  return DFB_OK;

out2:
  if (!dfb_core_is_master (core))
    munmap ((void *) drvdata->stdrv.bdisp_shared, page_size);
out1:
  if (drvdata->stmfb_acceleration)
    dfb_gfxcard_unmap_mmio (device, drvdata->stdrv.mmio_base, -1);
  else
    {
      munmap ((void *) drvdata->stdrv.mmio_base, page_size);
      close (drvdata->fd_gfx);
    }

  return res;
}

static DFBResult
driver_init_device (CoreGraphicsDevice * const device,
                    GraphicsDeviceInfo * const device_info,
                    void               * const driver_data,
                    void               * const device_data)
{
  STGFX2DeviceData * const devdata = device_data;
  DFBResult         res;

  snprintf (device_info->name, DFB_GRAPHICS_DEVICE_INFO_NAME_LENGTH,
            "BDispII w/ user space AQs");

  snprintf (device_info->vendor, DFB_GRAPHICS_DEVICE_INFO_VENDOR_LENGTH,
            "ST Microelectronics");

  dfb_config->pollvsync_after = true;

  devdata->device_info = device_info;

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
  if (res == DFB_OK)
    /* check memory constraints */
    res = stgfx2_check_memory_constraints (device, devdata);
  if (res != DFB_OK)
    {
      /* There is a bug in DirectFB (at least 1.6, maybe earlier, maybe later)
         in that it doesn't error out and continues to use hardware
         acceleration if device initialisation fails. Since we don't want to
         crash here, or later (due to the failed init), let's work around
         that by forcing software-only. */
      dfb_config->software_only = true;
      return res;
    }

  /* Tell DirectFB about what operations we may be able to do. */
  device_info->caps.accel    = (STGFX2_VALID_DRAWINGFUNCTIONS
                                | STGFX2_VALID_BLITTINGFUNCTIONS);
  device_info->caps.drawing  = devdata->dfb_features.dfb_drawflags;
  device_info->caps.blitting = devdata->dfb_features.dfb_blitflags;
#if defined(STGFX2_SUPPORT_HW_CLIPPING) \
    && defined(BDISP2_SUPPORT_HW_CLIPPING)
  device_info->caps.flags    = CCF_CLIPPING;
  device_info->caps.clip     = DFXL_STRETCHBLIT;
#endif
  device_info->caps.flags |= CCF_RENDEROPTS;

  if (devdata->stdev.features.hw.size_4k)
    {
      device_info->limits.src_max.w
        = device_info->limits.src_max.h
        = device_info->limits.dst_max.w
        = device_info->limits.dst_max.h
        = 8191;
    }
  else
    {
      device_info->limits.src_max.w
        = device_info->limits.src_max.h
        = device_info->limits.dst_max.w
        = device_info->limits.dst_max.h
        = 4095;
    }

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

  register_stgfx2 ((struct _STGFX2DriverData *) driver_data,
                   (struct _STGFX2DeviceData *) device_data);

  return DFB_OK;
}


static void
driver_close_device (CoreGraphicsDevice * const device,
                     void               * const driver_data,
                     void               * const device_data)
{
}

static void
driver_close_driver (CoreGraphicsDevice * const device,
                     void               * const driver_data)
{
  STGFX2DriverData * const drvdata = driver_data;
  long page_size;

  page_size = (long)sysconf(_SC_PAGESIZE);

  if (drvdata->stdrv.bdisp_nodes.memory)
    munmap ((void *) drvdata->stdrv.bdisp_nodes.memory,
            drvdata->stdrv.bdisp_nodes.size);

  if (drvdata->stdrv.bdisp_shared)
    {
      const struct stm_bdisp2_shared_area __attribute__((unused)) * const shared
        = drvdata->stdrv.bdisp_shared;
      uint64_t ops;

      D_INFO ("DirectFB/stgfx2: %u starts, %u (%u/%u) interrupts, %u wait_idle, %u wait_next, %u idle\n",
              shared->num_starts, shared->num_irqs, shared->num_node_irqs,
              shared->num_lna_irqs,
              shared->num_wait_idle, shared->num_wait_next, shared->num_idle);

      ops = ((uint64_t) shared->num_ops_hi) << 32;
      ops += shared->num_ops_lo;
      D_INFO ("DirectFB/stgfx2: %llu ops, %llu ops/start, %llu ops/idle, %u starts/idle\n",
              ops,
              shared->num_starts ? ops / shared->num_starts : 0,
              shared->num_idle   ? ops / shared->num_idle   : 0,
              shared->num_idle   ? shared->num_starts / shared->num_idle : 0);
      if (page_size >= 0)
        munmap ((void *) drvdata->stdrv.bdisp_shared, page_size);
    }

  if (drvdata->stmfb_acceleration)
    dfb_gfxcard_unmap_mmio (device, drvdata->stdrv.mmio_base, -1);
  else
    {
      if (page_size >= 0)
        munmap ((void *) drvdata->stdrv.mmio_base, page_size);
      close (drvdata->fd_gfx);
    }
}
