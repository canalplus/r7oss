/*
   (c) Copyright 2010-2012  STMicroelectronics (R&D) Ltd.
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

#include <asm/types.h>

#include <directfb_util.h>

#include <direct/debug.h>
#include <direct/mem.h>

#include <core/surface_pool.h>

#include <gfx/convert.h>

#include "stmfbdev.h"
#include "surfacemanager.h"

D_DEBUG_DOMAIN (STMfbdev_Surfaces_Main, "STMfbdev/Surfaces/Main", "STMfb Surface Pool");
D_DEBUG_DOMAIN (STMfbdev_SurfLock_Main, "STMfbdev/SurfLock/Main", "STMfb Surface Pool Locks");

/****************************************************************************/

typedef struct
{
  int             magic;

  SurfaceManager *manager;

  unsigned long   physical;
} STMfbdevPoolData;

typedef struct
{
  int      magic;

  CoreDFB *core;
  void    *mem;
} STMfbdevPoolLocalData;

typedef struct
{
  int    magic;

  Chunk *chunk;
} STMfbdevPoolAllocationData;

/****************************************************************************/

static int
stmfbdevPoolDataSize (void)
{
  return sizeof (STMfbdevPoolData);
}

static int
stmfbdevPoolLocalDataSize (void)
{
  return sizeof (STMfbdevPoolLocalData);
}

static int
stmfbdevAllocationDataSize (void)
{
  return sizeof (STMfbdevPoolAllocationData);
}

static DFBResult
stmfbdevLeavePool (CoreSurfacePool *pool,
                   void            *pool_data,
                   void            *pool_local)
{
  STMfbdevPoolData      * const data  = pool_data;
  STMfbdevPoolLocalData * const local = pool_local;

  D_DEBUG_AT (STMfbdev_Surfaces_Main, "%s()\n", __func__);

  D_MAGIC_ASSERT (pool, CoreSurfacePool);
  D_MAGIC_ASSERT (data, STMfbdevPoolData);
  D_MAGIC_ASSERT (local, STMfbdevPoolLocalData);

  D_UNUSED_P (data);

  local->mem = NULL;

  D_MAGIC_CLEAR (local);

  return DFB_OK;
}

static DFBResult
stmfbdevJoinPool (CoreDFB                    *core,
                  CoreSurfacePool            *pool,
                  void                       *pool_data,
                  void                       *pool_local,
                  void                       *system_data)
{
  STMfbdevPoolData      * const data  = pool_data;
  STMfbdevPoolLocalData * const local = pool_local;
  STMfbdev              * const stmfbdev = dfb_system_data ();

  D_DEBUG_AT (STMfbdev_Surfaces_Main, "%s()\n", __func__);

  D_ASSERT (core != NULL);
  D_MAGIC_ASSERT (pool, CoreSurfacePool);
  D_MAGIC_ASSERT (data, STMfbdevPoolData);
  D_ASSERT (local != NULL);
  D_MAGIC_ASSERT (stmfbdev, STMfbdev);

  D_UNUSED_P (data);

  D_MAGIC_SET (local, STMfbdevPoolLocalData);

  local->mem = stmfbdev->framebuffer_base;
  D_ASSERT (local->mem != NULL);

  local->core = core;

  return DFB_OK;
}

static DFBResult
stmfbdevDestroyPool (CoreSurfacePool *pool,
                     void            *pool_data,
                     void            *pool_local)
{
  STMfbdevPoolData      * const data  = pool_data;
  STMfbdevPoolLocalData * const local = pool_local;

  D_DEBUG_AT (STMfbdev_Surfaces_Main, "%s()\n", __func__);

  D_MAGIC_ASSERT (pool, CoreSurfacePool);
  D_MAGIC_ASSERT (data, STMfbdevPoolData);
  D_MAGIC_ASSERT (local, STMfbdevPoolLocalData);

  D_UNUSED_P (local);

  stmfbdevLeavePool (pool, pool_data, pool_local);

  dfb_surfacemanager_destroy (data->manager);

  D_MAGIC_CLEAR (data);

  return DFB_OK;
}

static DFBResult
stmfbdevInitPool (CoreDFB                    *core,
                  CoreSurfacePool            *pool,
                  void                       *pool_data,
                  void                       *pool_local,
                  void                       *system_data,
                  CoreSurfacePoolDescription *ret_desc)
{
  DFBResult              ret;
  STMfbdevPoolData      * const data  = pool_data;
  STMfbdevPoolLocalData * const local = pool_local;
  STMfbdev              * const stmfbdev = dfb_system_data ();

  D_DEBUG_AT (STMfbdev_Surfaces_Main, "%s()\n", __func__);

  D_ASSERT (core != NULL);
  D_MAGIC_ASSERT (pool, CoreSurfacePool);
  D_ASSERT (data != NULL);
  D_ASSERT (local != NULL);
  D_ASSERT (ret_desc != NULL);

  D_UNUSED_P (local);

  D_MAGIC_ASSERT (stmfbdev, STMfbdev);
  D_MAGIC_ASSERT (stmfbdev->shared, STMfbdevSharedData);

  D_MAGIC_SET (data, STMfbdevPoolData);

  snprintf (ret_desc->name,
            DFB_SURFACE_POOL_DESC_NAME_LENGTH, "STMfb Memory");

  ret = dfb_surfacemanager_create (core, stmfbdev->shared->fix.smem_len,
                                   &data->manager);

  if (ret)
    return ret;

  data->physical = stmfbdev->shared->fix.smem_start;

  ret_desc->caps              = CSPCAPS_PHYSICAL | CSPCAPS_VIRTUAL;
  ret_desc->access[CSAID_CPU] = CSAF_READ | CSAF_WRITE | CSAF_SHARED;
  ret_desc->access[CSAID_GPU] = CSAF_READ | CSAF_WRITE | CSAF_SHARED;
  ret_desc->types             = (0
                                 | CSTF_LAYER
                                 | CSTF_WINDOW | CSTF_CURSOR | CSTF_FONT
                                 | CSTF_SHARED | CSTF_EXTERNAL
                                 | CSTF_BUFFEROBJECT
                                );
  ret_desc->priority          = CSPP_DEFAULT;
  ret_desc->size              = stmfbdev->shared->fix.smem_len;

  /* other accelerators */
  ret_desc->access[CSAID_ACCEL0] = CSAF_READ | CSAF_WRITE;
  ret_desc->access[CSAID_ACCEL1] = CSAF_READ | CSAF_WRITE;
  ret_desc->access[CSAID_ACCEL2] = CSAF_READ | CSAF_WRITE;
  ret_desc->access[CSAID_ACCEL3] = CSAF_READ | CSAF_WRITE;
  ret_desc->access[CSAID_ACCEL4] = CSAF_READ | CSAF_WRITE;
  ret_desc->access[CSAID_ACCEL5] = CSAF_READ | CSAF_WRITE;

  /* For hardware layers */
  ret_desc->access[CSAID_LAYER0] = CSAF_READ;
  ret_desc->access[CSAID_LAYER1] = CSAF_READ;
  ret_desc->access[CSAID_LAYER2] = CSAF_READ;
  ret_desc->access[CSAID_LAYER3] = CSAF_READ;
  ret_desc->access[CSAID_LAYER4] = CSAF_READ;
  ret_desc->access[CSAID_LAYER5] = CSAF_READ;
  ret_desc->access[CSAID_LAYER6] = CSAF_READ;
  ret_desc->access[CSAID_LAYER7] = CSAF_READ;
  ret_desc->access[CSAID_LAYER8] = CSAF_READ;
  ret_desc->access[CSAID_LAYER9] = CSAF_READ;
  ret_desc->access[CSAID_LAYER10] = CSAF_READ;
  ret_desc->access[CSAID_LAYER11] = CSAF_READ;
  ret_desc->access[CSAID_LAYER12] = CSAF_READ;
  ret_desc->access[CSAID_LAYER13] = CSAF_READ;
  ret_desc->access[CSAID_LAYER14] = CSAF_READ;
  ret_desc->access[CSAID_LAYER15] = CSAF_READ;

  ret = stmfbdevJoinPool (core, pool, pool_data, pool_local, system_data);
  if (ret)
    {
      stmfbdevDestroyPool (pool, pool_data, pool_local);
      return ret;
    }

  return DFB_OK;
}

static DFBResult
stmfbdevTestConfig (CoreSurfacePool         *pool,
                    void                    *pool_data,
                    void                    *pool_local,
                    CoreSurfaceBuffer       *buffer,
                    const CoreSurfaceConfig *config)
{
  CoreSurface           *surface;
  STMfbdevPoolData      * const data  = pool_data;
  STMfbdevPoolLocalData * const local = pool_local;
  DFBResult              ret;

  D_DEBUG_AT (STMfbdev_Surfaces_Main, "%s (%p)\n", __func__, buffer);

  D_MAGIC_ASSERT (pool, CoreSurfacePool);
  D_MAGIC_ASSERT (data, STMfbdevPoolData);
  D_MAGIC_ASSERT (local, STMfbdevPoolLocalData);
  D_MAGIC_ASSERT (buffer, CoreSurfaceBuffer);
  D_ASSERT (config != NULL);

  surface = buffer->surface;
  D_MAGIC_ASSERT (surface, CoreSurface);
  D_UNUSED_P( surface );

  D_DEBUG_AT (STMfbdev_Surfaces_Main, "  -> buffer/type/id %p/0x%x/%lu\n",
              buffer, surface->type, surface->resource_id);

  ret = dfb_surfacemanager_allocate (local->core, data->manager,
                                     buffer, NULL, NULL, true);

  D_DEBUG_AT (STMfbdev_Surfaces_Main, "  -> %s\n", DirectFBErrorString (ret));

  return ret;
}

static DFBResult
stmfbdevAllocateBuffer (CoreSurfacePool       *pool,
                        void                  *pool_data,
                        void                  *pool_local,
                        CoreSurfaceBuffer     *buffer,
                        CoreSurfaceAllocation *allocation,
                        void                  *alloc_data)
{
  CoreSurface                *surface;
  STMfbdevPoolData           * const data  = pool_data;
  STMfbdevPoolLocalData      * const local = pool_local;
  STMfbdevPoolAllocationData * const alloc = alloc_data;
  DFBResult                   ret;
  Chunk                      *chunk;

  D_DEBUG_AT (STMfbdev_Surfaces_Main, "%s (%p)\n", __func__, buffer);

  D_MAGIC_ASSERT (pool, CoreSurfacePool);
  D_MAGIC_ASSERT (data, STMfbdevPoolData);
  D_MAGIC_ASSERT (local, STMfbdevPoolLocalData);
  D_MAGIC_ASSERT (buffer, CoreSurfaceBuffer);
  D_MAGIC_ASSERT (allocation, CoreSurfaceAllocation);

  surface = buffer->surface;
  D_MAGIC_ASSERT (surface, CoreSurface);
  D_UNUSED_P( surface );

  ret = dfb_surfacemanager_allocate (local->core, data->manager, buffer,
                                     allocation, &chunk, true);
  if (ret)
    return ret;

  D_MAGIC_ASSERT (chunk, Chunk);

  alloc->chunk = chunk;

  D_DEBUG_AT (STMfbdev_Surfaces_Main,
              "  -> offset 0x%.8x (%u), format: %s, pitch %d, size %d\n",
              chunk->offset, chunk->offset,
              dfb_pixelformat_name (buffer->format), chunk->pitch,
              chunk->length);

  allocation->size   = chunk->length;
  allocation->offset = chunk->offset;

#if STGFX_DRIVER == 2
  if (unlikely (buffer->format == DSPF_RGB32))
    {
      /* for RGB32, we need to set the alpha to 0xff */
      /* FIXME: check if we can hook into dfb_gfx_clear() or
         dfb_surface_clear_buffers() */
      STGFX2DriverData * const stdrv = dfb_gfxcard_get_driver_data ();
      STGFX2DeviceData * const stdev = dfb_gfxcard_get_device_data ();
      DFBRectangle      rect = { .x = 0, .y = 0,
                                 .w = buffer->surface->config.size.w,
                                 .h = buffer->surface->config.size.h };

      D_DEBUG_AT (STMfbdev_Surfaces_Main, "  -> rgb32 allocation!\n");

      D_WARN ("STMfbdev/Surfaces/Main: RGB32 support is experimental and slow!");

      dfb_gfxcard_lock (GDLF_WAIT);
      _bdisp_aq_RGB32_init (stdrv, stdev,
                            data->physical + chunk->offset, chunk->pitch,
                            &rect);
      dfb_gfxcard_unlock ();
    }
#endif

  D_MAGIC_SET (alloc, STMfbdevPoolAllocationData);

  return DFB_OK;
}

static DFBResult
stmfbdevDeallocateBuffer (CoreSurfacePool       *pool,
                          void                  *pool_data,
                          void                  *pool_local,
                          CoreSurfaceBuffer     *buffer,
                          CoreSurfaceAllocation *allocation,
                          void                  *alloc_data)
{
  STMfbdevPoolData            * const data  = pool_data;
  const STMfbdevPoolLocalData * const local = pool_local;
  STMfbdevPoolAllocationData  * const alloc = alloc_data;

  D_DEBUG_AT (STMfbdev_Surfaces_Main, "%s (%p)\n", __func__, buffer);

  D_MAGIC_ASSERT (pool, CoreSurfacePool);
  D_MAGIC_ASSERT (data, STMfbdevPoolData);
  D_MAGIC_ASSERT (local, STMfbdevPoolLocalData);
  D_MAGIC_ASSERT (allocation, CoreSurfaceAllocation);
  D_MAGIC_ASSERT (alloc, STMfbdevPoolAllocationData);

  (void) local;

  D_ASSERT (alloc->chunk != NULL);
  dfb_surfacemanager_deallocate (data->manager, alloc->chunk);

  D_MAGIC_CLEAR (alloc);

  return DFB_OK;
}

static DFBResult
stmfbdevMuckOut (CoreSurfacePool   *pool,
                 void              *pool_data,
                 void              *pool_local,
                 CoreSurfaceBuffer *buffer )
{
  STMfbdevPoolData            * const data  = pool_data;
  const STMfbdevPoolLocalData * const local = pool_local;

  D_DEBUG_AT (STMfbdev_Surfaces_Main, "%s (%p)\n", __func__, buffer);

  D_MAGIC_ASSERT (pool, CoreSurfacePool);
  D_MAGIC_ASSERT (data, STMfbdevPoolData);
  D_MAGIC_ASSERT (local, STMfbdevPoolLocalData);
  D_MAGIC_ASSERT (buffer, CoreSurfaceBuffer);

  return dfb_surfacemanager_displace (local->core, data->manager, buffer);
}

#if D_DEBUG_ENABLED
static char *
_accessor_str (CoreSurfaceAccessorID accessor)
{
  char *str = malloc (220);
  int pos = 0;

  pos += sprintf (str + pos, "%.8x -> ", accessor);
  if (accessor == CSAID_NONE)
    pos += sprintf (str + pos, "NONE");
  else if (accessor == CSAID_CPU)
    pos += sprintf (str + pos, "CPU");
  else if (accessor == CSAID_GPU)
    pos += sprintf (str + pos, "GPU");
  else if (accessor == CSAID_ACCEL0)
    pos += sprintf (str + pos, "ACCEL0");
  else if (accessor == CSAID_ACCEL1)
    pos += sprintf (str + pos, "ACCEL1");
  else if (accessor == CSAID_ACCEL2)
    pos += sprintf (str + pos, "ACCEL2");
  else if (accessor == CSAID_ACCEL3)
    pos += sprintf (str + pos, "ACCEL3");
  else if (accessor == CSAID_ACCEL4)
    pos += sprintf (str + pos, "ACCEL4");
  else if (accessor == CSAID_ACCEL5)
    pos += sprintf (str + pos, "ACCEL5");
  else if (accessor == CSAID_LAYER0)
    pos += sprintf (str + pos, "LAYER0");
  else if (accessor == CSAID_LAYER1)
    pos += sprintf (str + pos, "LAYER1");
  else if (accessor == CSAID_LAYER2)
    pos += sprintf (str + pos, "LAYER2");
  else if (accessor == CSAID_LAYER3)
    pos += sprintf (str + pos, "LAYER3");
  else if (accessor == CSAID_LAYER4)
    pos += sprintf (str + pos, "LAYER4");
  else if (accessor == CSAID_LAYER5)
    pos += sprintf (str + pos, "LAYER5");
  else if (accessor == CSAID_LAYER6)
    pos += sprintf (str + pos, "LAYER6");
  else if (accessor == CSAID_LAYER7)
    pos += sprintf (str + pos, "LAYER7");
  else
    pos += sprintf (str + pos, "UNKNOWN");

  return str;
}

static char *
_access_str (CoreSurfaceAccessFlags access)
{
  char *str = malloc (220);
  int pos = 0;

  pos += sprintf (str + pos, "%.8x ->", access);
  if (access == CSAF_NONE)
    pos += sprintf (str + pos, " NONE");
  else
    {
      if (access & CSAF_READ)
        pos += sprintf (str + pos, " READ");
      if (access & CSAF_WRITE)
        pos += sprintf (str + pos, " WRITE");
      if (access & CSAF_SHARED)
        pos += sprintf (str + pos, " SHARED");
    }

  return str;
}
#endif

static DFBResult
stmfbdevLock (CoreSurfacePool       *pool,
              void                  *pool_data,
              void                  *pool_local,
              CoreSurfaceAllocation *allocation,
              void                  *alloc_data,
              CoreSurfaceBufferLock *lock)
{
  const STMfbdevPoolData           * const data  = pool_data;
  const STMfbdevPoolLocalData      * const local = pool_local;
  const STMfbdevPoolAllocationData * const alloc = alloc_data;
  const Chunk                      *chunk;

  D_MAGIC_ASSERT (pool, CoreSurfacePool);
  D_MAGIC_ASSERT (data, STMfbdevPoolData);
  D_MAGIC_ASSERT (local, STMfbdevPoolLocalData);
  D_MAGIC_ASSERT (allocation, CoreSurfaceAllocation);
  D_MAGIC_ASSERT (alloc, STMfbdevPoolAllocationData);
  D_MAGIC_ASSERT (lock, CoreSurfaceBufferLock);

  D_DEBUG_AT (STMfbdev_SurfLock_Main, "%s (%p)\n", __func__, lock->buffer);

  D_MAGIC_ASSERT (alloc->chunk, Chunk);
  chunk = alloc->chunk;

#if D_DEBUG_ENABLED
  {
  /* heavy performance hit */
  char *accessor = _accessor_str (lock->accessor);
  char *access   = _access_str (lock->access);
  D_DEBUG_AT (STMfbdev_SurfLock_Main, "  -> by %s for %s\n",
              accessor,  access);
  free (access);
  free (accessor);
  }
#endif

  lock->pitch  = chunk->pitch;
  lock->offset = chunk->offset;
  lock->addr   = local->mem + chunk->offset;
  lock->phys   = data->physical + chunk->offset;

  D_DEBUG_AT (STMfbdev_SurfLock_Main,
              "  -> offset 0x%.8lx (%lu), pitch %d, addr %p, phys 0x%.8lx\n",
              lock->offset, lock->offset, lock->pitch, lock->addr, lock->phys);

  return DFB_OK;
}

static DFBResult
stmfbdevUnlock (CoreSurfacePool       *pool,
                void                  *pool_data,
                void                  *pool_local,
                CoreSurfaceAllocation *allocation,
                void                  *alloc_data,
                CoreSurfaceBufferLock *lock)
{
  const STMfbdevPoolData           * const data = pool_data;
  const STMfbdevPoolLocalData      * const local = pool_local;
  const STMfbdevPoolAllocationData * const alloc = alloc_data;

  D_MAGIC_ASSERT (pool, CoreSurfacePool);
  D_MAGIC_ASSERT (data, STMfbdevPoolData);
  D_MAGIC_ASSERT (local, STMfbdevPoolLocalData);
  D_MAGIC_ASSERT (allocation, CoreSurfaceAllocation);
  D_MAGIC_ASSERT (alloc, STMfbdevPoolAllocationData);
  D_MAGIC_ASSERT (lock, CoreSurfaceBufferLock);

  D_DEBUG_AT (STMfbdev_SurfLock_Main, "%s (%p)\n", __func__, lock->buffer);

  D_UNUSED_P (data);
  D_UNUSED_P (local);
  D_UNUSED_P (alloc);

#if D_DEBUG_ENABLED
  {
  /* heavy performance hit */
  char *accessor = _accessor_str (lock->accessor);
  char *access   = _access_str (lock->access);
  D_DEBUG_AT (STMfbdev_SurfLock_Main, "  -> by %s for %s\n",
              accessor,  access);
  free (access);
  free (accessor);
  }
#endif

#if STGFX_DRIVER == 2
  if (unlikely (lock->buffer->format == DSPF_RGB32
                && lock->accessor != CSAID_GPU
                && lock->access & CSAF_WRITE))
    {
      /* if a non-GPU accessor did a write access to an RGB32 surface, we
         should make sure the alpha is forced to 0xff, as the BDisp doesn't
         support this format natively */
      STGFX2DriverData * const stdrv = dfb_gfxcard_get_driver_data ();
      STGFX2DeviceData * const stdev = dfb_gfxcard_get_device_data ();
      DFBRectangle      rect = { .x = 0, .y = 0,
                                 .w = lock->buffer->surface->config.size.w,
                                 .h = lock->buffer->surface->config.size.h };

      D_DEBUG_AT (STMfbdev_SurfLock_Main, "  -> rgb32 write release!\n");
      dfb_gfxcard_lock (GDLF_WAIT);
      _bdisp_aq_RGB32_fixup (stdrv, stdev,
                             lock->phys, lock->pitch,
                             &rect);
      dfb_gfxcard_unlock ();
    }
#endif

  return DFB_OK;
}

const SurfacePoolFuncs _g_stmfbdevSurfacePoolFuncs = {
  .PoolDataSize       = stmfbdevPoolDataSize,
  .PoolLocalDataSize  = stmfbdevPoolLocalDataSize,
  .AllocationDataSize = stmfbdevAllocationDataSize,

  .InitPool    = stmfbdevInitPool,
  .JoinPool    = stmfbdevJoinPool,
  .DestroyPool = stmfbdevDestroyPool,
  .LeavePool   = stmfbdevLeavePool,

  .TestConfig       = stmfbdevTestConfig,
  .AllocateBuffer   = stmfbdevAllocateBuffer,
  .DeallocateBuffer = stmfbdevDeallocateBuffer,

  .MuckOut = stmfbdevMuckOut,

  .Lock   = stmfbdevLock,
  .Unlock = stmfbdevUnlock,
};
