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

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/kd.h>

#include <directfb.h>

#include <direct/mem.h>
#include <direct/memcpy.h>
#include <direct/signals.h>

#include <fusion/fusion.h>
#include <fusion/arena.h>
#include <fusion/shmalloc.h>
#include <fusion/call.h>

#include <core/core.h>
#include <core/surface_pool.h>
#include <core/screens.h>
#include <core/layers.h>

#include <misc/conf.h>

#include "stmfbdev.h"
#include "fb.h"

#include <core/core_system.h>

#include "stmfb_aux_surface_pool.h"

DFB_CORE_SYSTEM( stmfbdev )

D_DEBUG_DOMAIN( STMfbdev_System, "STMfbdev/System", "STMfb System Module" );

/******************************************************************************/

extern const SurfacePoolFuncs  _g_stmfbdevSurfacePoolFuncs;
extern       ScreenFuncs       _g_stmfbdevScreenFuncs;
extern       DisplayLayerFuncs _g_stmfbdevLayerFuncs;

/******************************************************************************/

STMfbdev *dfb_stmfbdev;

/******************************************************************************/

static DFBResult
dfb_stmfbdev_open( STMfbdev * const stmfbdev )
{
     mode_t mode = O_RDWR;
#ifdef O_CLOEXEC
     /* should be closed automatically in children upon exec(...), but is
        only available since Linux 2.6.23 */
     mode |= O_CLOEXEC;
#endif

     D_MAGIC_ASSERT( stmfbdev, STMfbdev );
     D_ASSERT( stmfbdev->fd < 0 );

     if (dfb_config->fb_device) {
          stmfbdev->fd = open( dfb_config->fb_device, mode );
          if (stmfbdev->fd < 0) {
               D_PERROR( "DirectFB/STMfbdev: Error opening '%s'!\n",
                         dfb_config->fb_device);

               return errno2result( errno );
          }
     }
     else if (getenv( "FRAMEBUFFER" ) && *getenv( "FRAMEBUFFER" ) != '\0') {
          stmfbdev->fd = open( getenv ( "FRAMEBUFFER" ), mode );
          if (stmfbdev->fd < 0) {
               D_PERROR( "DirectFB/STMfbdev: Error opening '%s'!\n",
                          getenv( "FRAMEBUFFER" ) );

               return errno2result( errno );
          }
     }
     else {
          stmfbdev->fd = direct_try_open( "/dev/fb0", "/dev/fb/0", mode, true );
          if (stmfbdev->fd < 0) {
               D_ERROR( "DirectFB/STMfbdev: Error opening framebuffer device!\n" );
               D_ERROR( "DirectFB/STMfbdev: Use 'fbdev' option or set FRAMEBUFFER environment variable.\n" );
               return DFB_INIT;
          }
     }

#ifndef O_CLOEXEC
     if (fcntl( stmfbdev->fd, F_SETFD, FD_CLOEXEC ) < 0) {
          D_PERROR( "DirectFB/STMfbdev: Setting FD_CLOEXEC flag failed!\n" );
          return errno2result( errno );
     }
#endif

     return DFB_OK;
}




static void
system_get_info( CoreSystemInfo *info )
{
     info->version.major = 0;
     info->version.minor = 9;

     info->type = CORE_STMFBDEV;
     info->caps = CSCAPS_ACCELERATION;

     snprintf( info->name, sizeof (info->name), "STMfbdev" );
     snprintf( info->vendor, sizeof (info->vendor), "STMicroelectronics (R&D) Ltd." );
     snprintf( info->url, sizeof (info->url), "http://www.stlinux.com" );
     snprintf( info->license, sizeof (info->license), "LGPL" );
}

static DFBResult
system_initialize( CoreDFB *core, void **data )
{
     DFBResult            ret;
     CoreScreen          *screen;
     long                 page_size;
     STMfbdevSharedData  *shared;
     FusionSHMPoolShared *shmpool;
     STMfbdev            *stmfbdev;

     D_DEBUG_AT( STMfbdev_System, "%s()\n", __FUNCTION__ );

     D_ASSERT( dfb_stmfbdev == NULL );

     shmpool = dfb_core_shmpool( core );

     *data = dfb_stmfbdev = stmfbdev = D_CALLOC( 1, sizeof(STMfbdev) );
     if (!stmfbdev)
          return D_OOM();

     D_MAGIC_SET( stmfbdev, STMfbdev );
     stmfbdev->fd = -1;

     shared = (STMfbdevSharedData *) SHCALLOC( shmpool, 1,
                                               sizeof(STMfbdevSharedData) );
     if (!shared) {
          ret = D_OOSHM();
          goto error;
     }

     D_MAGIC_SET( shared, STMfbdevSharedData );

     shared->shmpool = shmpool;

     core_arena_add_shared_field( core, "stmfbdev", shared );

     stmfbdev->core   = core;
     stmfbdev->shared = shared;

     page_size = direct_pagesize();
     shared->page_mask = page_size < 0 ? 0 : (page_size - 1);

     ret = dfb_stmfbdev_open( stmfbdev );
     if (ret)
          goto error;

     if (dfb_config->vt) {
          ret = dfb_vt_initialize();
          if (ret)
               goto error;
     }

     /* Retrieve fixed information like video ram size */
     if (ioctl( stmfbdev->fd, FBIOGET_FSCREENINFO, &shared->fix ) < 0) {
          D_PERROR( "DirectFB/STMfbdev: "
                    "Could not get fixed screen information!\n" );
          ret = errno2result( errno );
          goto error;
     }

     D_INFO( "DirectFB/STMfbdev: Found '%s' (ID %d) with frame buffer at 0x%08lx, %dk (MMIO 0x%08lx, %dk)\n",
             shared->fix.id, shared->fix.accel,
             shared->fix.smem_start, shared->fix.smem_len >> 10,
             shared->fix.mmio_start, shared->fix.mmio_len >> 10 );

     /* Map the framebuffer */
     stmfbdev->framebuffer_base = mmap( NULL, shared->fix.smem_len,
                                        PROT_READ | PROT_WRITE, MAP_SHARED,
                                        stmfbdev->fd, 0 );
     if (stmfbdev->framebuffer_base == MAP_FAILED) {
          D_PERROR( "DirectFB/STMfbdev: "
                    "Could not mmap the framebuffer!\n");
          stmfbdev->framebuffer_base = NULL;
          ret = errno2result( errno );
          goto error;
     }

     /* as we're running on ST hardware, we can assume pollvsync_after. We
        set it here as well as in the gfxdriver, so that it does not matter
        which system is used (fbdev vs. stmfbdev) or if a gfxdriver is used
        at all (via disable-module=) */
     dfb_config->pollvsync_after = true;

     dfb_surface_pool_initialize( core, &_g_stmfbdevSurfacePoolFuncs,
                                  &stmfbdev->shared->pool );
     stmfb_aux_surface_pools_init( core );

     /* Register primary screen functions */
     screen = dfb_screens_register( NULL, stmfbdev, &_g_stmfbdevScreenFuncs );

     /* Register primary layer functions */
     dfb_layers_register( screen, stmfbdev, &_g_stmfbdevLayerFuncs );

     return DFB_OK;


error:
     if (shared)
          SHFREE( shmpool, shared );

     if (stmfbdev->framebuffer_base)
          munmap( stmfbdev->framebuffer_base, shared->fix.smem_len );

     if (stmfbdev->fd != -1)
          close( stmfbdev->fd );

     D_FREE( stmfbdev );
     *data = dfb_stmfbdev = NULL;

     return ret;
}

static DFBResult
system_join( CoreDFB *core, void **data )
{
     DFBResult   ret;
     CoreScreen *screen;
     STMfbdev   *stmfbdev;
     void       *shared;

     D_DEBUG_AT( STMfbdev_System, "%s()\n", __FUNCTION__ );

     D_ASSERT( dfb_stmfbdev == NULL );

     *data = dfb_stmfbdev = stmfbdev = D_CALLOC( 1, sizeof(STMfbdev) );
     if (!stmfbdev)
          return D_OOM();

     D_MAGIC_SET( stmfbdev, STMfbdev );
     stmfbdev->fd = -1;

     core_arena_get_shared_field( core, "stmfbdev", &shared );
     stmfbdev->shared = shared;
     D_MAGIC_ASSERT( stmfbdev->shared, STMfbdevSharedData );

     stmfbdev->core = core;

     /* Open framebuffer device */
     ret = dfb_stmfbdev_open( stmfbdev );
     if (ret)
          goto error;

     if (dfb_config->vt) {
          ret = dfb_vt_join();
          if (ret)
               goto error;
     }

     /* Map the framebuffer */
     stmfbdev->framebuffer_base = mmap( NULL, stmfbdev->shared->fix.smem_len,
                                        PROT_READ | PROT_WRITE, MAP_SHARED,
                                        stmfbdev->fd, 0 );
     if (stmfbdev->framebuffer_base == MAP_FAILED) {
          D_PERROR( "DirectFB/STMfbdev: "
                    "Could not mmap the framebuffer!\n");
          stmfbdev->framebuffer_base = NULL;
          ret = errno2result( errno );
          goto error;
     }

     dfb_surface_pool_join( core, stmfbdev->shared->pool,
                            &_g_stmfbdevSurfacePoolFuncs );
     stmfb_aux_surface_pools_join( core );

     /* Register primary screen functions */
     screen = dfb_screens_register( NULL, stmfbdev, &_g_stmfbdevScreenFuncs );

     /* Register primary layer functions */
     dfb_layers_register( screen, stmfbdev, &_g_stmfbdevLayerFuncs );

     return DFB_OK;


error:
     if (stmfbdev->fd != -1)
          close( stmfbdev->fd );

     D_FREE( stmfbdev );
     *data = dfb_stmfbdev = NULL;

     return ret;
}

static DFBResult
system_shutdown( bool emergency )
{
     DFBResult ret;

     D_DEBUG_AT( STMfbdev_System, "%s()\n", __FUNCTION__ );

     D_MAGIC_ASSERT( dfb_stmfbdev, STMfbdev );
     D_MAGIC_ASSERT( dfb_stmfbdev->shared, STMfbdevSharedData );
     D_MAGIC_ASSERT( dfb_stmfbdev->shared->pool, CoreSurfacePool );

     stmfb_aux_surface_pools_destroy( dfb_stmfbdev->core );
     dfb_surface_pool_destroy( dfb_stmfbdev->shared->pool );

     munmap( dfb_stmfbdev->framebuffer_base,
             dfb_stmfbdev->shared->fix.smem_len );

     if (dfb_config->vt) {
          ret = dfb_vt_shutdown( emergency );
          if (ret)
               return ret;
     }

     close( dfb_stmfbdev->fd );

     D_MAGIC_CLEAR( dfb_stmfbdev->shared );
     D_MAGIC_CLEAR( dfb_stmfbdev );

     SHFREE( dfb_stmfbdev->shared->shmpool, dfb_stmfbdev->shared );
     D_FREE( dfb_stmfbdev );
     dfb_stmfbdev = NULL;

     return DFB_OK;
}

static DFBResult
system_leave( bool emergency )
{
     DFBResult ret;

     D_DEBUG_AT( STMfbdev_System, "%s()\n", __FUNCTION__ );

     D_MAGIC_ASSERT( dfb_stmfbdev, STMfbdev );
     D_MAGIC_ASSERT( dfb_stmfbdev->shared, STMfbdevSharedData );
     D_MAGIC_ASSERT( dfb_stmfbdev->shared->pool, CoreSurfacePool );

     stmfb_aux_surface_pools_leave( dfb_stmfbdev->core );
     dfb_surface_pool_leave( dfb_stmfbdev->shared->pool );

     munmap( dfb_stmfbdev->framebuffer_base,
             dfb_stmfbdev->shared->fix.smem_len );

     if (dfb_config->vt) {
          ret = dfb_vt_leave( emergency );
          if (ret)
               return ret;
     }

     close( dfb_stmfbdev->fd );

     D_MAGIC_CLEAR( dfb_stmfbdev );

     D_FREE( dfb_stmfbdev );
     dfb_stmfbdev = NULL;

     return DFB_OK;
}

static DFBResult
system_suspend( void )
{
     return DFB_OK;
}

static DFBResult
system_resume( void )
{
     return DFB_OK;
}

/******************************************************************************/

static volatile void *
system_map_mmio( unsigned int    offset,
                 int             length )
{
     void *addr;

     D_MAGIC_ASSERT( dfb_stmfbdev, STMfbdev );

     if (length <= 0)
          length = dfb_stmfbdev->shared->fix.mmio_len;

     addr = mmap( NULL, length, PROT_READ | PROT_WRITE, MAP_SHARED,
                  dfb_stmfbdev->fd,
                  dfb_stmfbdev->shared->fix.smem_len + offset );
     if (addr == MAP_FAILED) {
          D_PERROR( "DirectFB/STMfbdev: Could not mmap MMIO region "
                     "(offset %d, length %d)!\n", offset, length );
          return NULL;
     }

     return(volatile void*) ((u8*) addr + (dfb_stmfbdev->shared->fix.mmio_start
                                           & dfb_stmfbdev->shared->page_mask));
}

static void
system_unmap_mmio( volatile void  *addr,
                   int             length )
{
     D_MAGIC_ASSERT( dfb_stmfbdev, STMfbdev );

     if (length <= 0)
          length = dfb_stmfbdev->shared->fix.mmio_len;

     if (munmap( (void*) ((u8*) addr - (dfb_stmfbdev->shared->fix.mmio_start
                                        & dfb_stmfbdev->shared->page_mask)),
                 length ) < 0)
          D_PERROR( "DirectFB/STMfbdev: Could not unmap MMIO region "
                     "at %p (length %d)!\n", addr, length );
}

static int
system_get_accelerator( void )
{
     D_MAGIC_ASSERT( dfb_stmfbdev, STMfbdev );

     if (dfb_config->accelerator)
          return dfb_config->accelerator;

     return dfb_stmfbdev->shared->fix.accel;
}

static VideoMode *
system_get_modes( void )
{
     return NULL;
}

static VideoMode *
system_get_current_mode( void )
{
     return NULL;
}

static DFBResult
system_thread_init( void )
{
     if (dfb_config->block_all_signals)
          direct_signals_block_all();

     return DFB_OK;
}

static bool
system_input_filter( CoreInputDevice *device,
                     DFBInputEvent   *event )
{
     if (dfb_config->vt && dfb_config->vt_switching) {
          switch (event->type) {
               case DIET_KEYPRESS:
                    if (DFB_KEY_TYPE(event->key_symbol) == DIKT_FUNCTION &&
                        event->modifiers == (DIMM_CONTROL | DIMM_ALT))
                         return dfb_vt_switch( event->key_symbol - DIKS_F1 + 1 );

                    break;

               case DIET_KEYRELEASE:
                    if (DFB_KEY_TYPE(event->key_symbol) == DIKT_FUNCTION &&
                        event->modifiers == (DIMM_CONTROL | DIMM_ALT))
                         return true;

                    break;

               default:
                    break;
          }
     }

     return false;
}

static unsigned long
system_video_memory_physical( unsigned int offset )
{
     D_MAGIC_ASSERT( dfb_stmfbdev, STMfbdev );

     return dfb_stmfbdev->shared->fix.smem_start + offset;
}

static void *
system_video_memory_virtual( unsigned int offset )
{
     D_MAGIC_ASSERT( dfb_stmfbdev, STMfbdev );

     return(void*)((u8*)(dfb_stmfbdev->framebuffer_base) + offset);
}

static unsigned int
system_videoram_length( void )
{
     D_MAGIC_ASSERT( dfb_stmfbdev, STMfbdev );

     return dfb_stmfbdev->shared->fix.smem_len;
}

static unsigned long
system_aux_memory_physical( unsigned int offset )
{
     return 0;
}

static void *
system_aux_memory_virtual( unsigned int offset )
{
     return NULL;
}

static unsigned int
system_auxram_length( void )
{
     return 0;
}

static void
system_get_busid( int *ret_bus, int *ret_dev, int *ret_func )
{
     return;
}

static void
system_get_deviceid( unsigned int *ret_vendor_id,
                     unsigned int *ret_device_id )
{
     *ret_vendor_id = 0x104a; /* STMicroelectronics */

     return;
}

static int
system_surface_data_size( void )
{
     /* Return zero because shared surface data is unneeded. */
     return 0;
}

static void
system_surface_data_init( CoreSurface *surface, void *data )
{
     /* Ignore since unneeded. */
}

static void
system_surface_data_destroy( CoreSurface *surface, void *data )
{
     /* Ignore since unneeded. */
}
