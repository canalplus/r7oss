/*
   ST Microelectronics system driver - surface pool memory (auxmem)

   (c) Copyright 2009-2012  STMicroelectronics Ltd.

   Written by André Draszik <andre.draszik@st.com>

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
#include <sys/mman.h>
#include <sys/ioctl.h>

#include <directfb_util.h>

#include <direct/debug.h>
#include <direct/mem.h>

#include <core/core.h>
#include <core/surface_pool.h>

#include <gfx/convert.h>

#include <misc/conf.h>

#include "stmfbdev.h"
#include "surfacemanager.h"

#include <core/system.h>
#include "stmfb_aux_surface_pool.h"

#include <linux/types.h>
#include <linux/stmfb.h>

D_DEBUG_DOMAIN (STMfbdev_Surfaces_Aux, "STMfbdev/Surfaces/Aux", "STMfb Aux Surface Pool");
D_DEBUG_DOMAIN (STMfbdev_SurfLock_Aux, "STMfbdev/SurfLock/Aux", "STMfb Aux Surface Pool Locks");

/****************************************************************************/

typedef struct
{
  int             magic;

  SurfaceManager *manager;

  unsigned long   part_base;
  unsigned int    aux_part;
} STMFB_AuxSurfacePoolData;

typedef struct
{
  int      magic;

  CoreDFB *core;
  void    *mem;
} STMFB_AuxSurfacePoolLocalData;

typedef struct
{
  int    magic;

  Chunk *chunk;
} STMFB_AuxSurfacePoolAllocationData;

/****************************************************************************/

static int
stmfb_aux_PoolDataSize (void)
{
  return sizeof (STMFB_AuxSurfacePoolData);
}

static int
stmfb_aux_PoolLocalDataSize (void)
{
  return sizeof (STMFB_AuxSurfacePoolLocalData);
}

static int
stmfb_aux_AllocationDataSize (void)
{
  return sizeof (STMFB_AuxSurfacePoolAllocationData);
}

static DFBResult
stmfb_aux_LeavePool (CoreSurfacePool *pool,
                     void            *pool_data,
                     void            *pool_local)
{
  STMFB_AuxSurfacePoolData      * const data = pool_data;
  STMFB_AuxSurfacePoolLocalData * const local = pool_local;

  D_DEBUG_AT (STMfbdev_Surfaces_Aux, "%s (%d)\n", __func__, data->aux_part);

  D_MAGIC_ASSERT (pool, CoreSurfacePool);
  D_MAGIC_ASSERT (data, STMFB_AuxSurfacePoolData);
  D_MAGIC_ASSERT (local, STMFB_AuxSurfacePoolLocalData);

  D_UNUSED_P (data);

  if (local->mem && local->mem != MAP_FAILED)
    {
      munmap (local->mem, pool->desc.size);
      local->mem = NULL;
    }

  D_MAGIC_CLEAR (local);

  return DFB_OK;
}

static DFBResult
stmfb_aux_JoinPool (CoreDFB         *core,
                    CoreSurfacePool *pool,
                    void            *pool_data,
                    void            *pool_local,
                    void            *system_data)
{
  STMFB_AuxSurfacePoolData      * const data  = pool_data;
  STMFB_AuxSurfacePoolLocalData * const local = pool_local;
  STMfbdev                      * const stmfbdev = dfb_system_data ();

  D_DEBUG_AT (STMfbdev_Surfaces_Aux, "%s (%d)\n", __func__, data->aux_part);

  D_ASSERT (core != NULL);
  D_MAGIC_ASSERT (pool, CoreSurfacePool);
  D_MAGIC_ASSERT (data, STMFB_AuxSurfacePoolData);
  D_ASSERT (local != NULL);

  D_MAGIC_SET (local, STMFB_AuxSurfacePoolLocalData);

  local->mem = mmap (NULL, pool->desc.size, PROT_READ | PROT_WRITE,
                     MAP_SHARED, stmfbdev->fd, data->part_base);
  if (local->mem == MAP_FAILED)
    {
      D_PERROR ("STMfbdev/Surfaces/Aux: Could not mmap gfx part %d!\n",
                data->aux_part);
      return DFB_INIT;
    }

  local->core = core;

  return DFB_OK;
}

static DFBResult
stmfb_aux_DestroyPool (CoreSurfacePool *pool,
                       void            *pool_data,
                       void            *pool_local)
{
  STMFB_AuxSurfacePoolData      * const data  = pool_data;
  STMFB_AuxSurfacePoolLocalData * const local = pool_local;

  D_DEBUG_AT (STMfbdev_Surfaces_Aux, "%s (%d)\n", __func__, data->aux_part);

  D_MAGIC_ASSERT (pool, CoreSurfacePool);
  D_MAGIC_ASSERT (data, STMFB_AuxSurfacePoolData);
  D_MAGIC_ASSERT (local, STMFB_AuxSurfacePoolLocalData);

  D_UNUSED_P (local);

  stmfb_aux_LeavePool (pool, pool_data, pool_local);

  dfb_surfacemanager_destroy (data->manager);

  D_MAGIC_CLEAR (data);

  return DFB_OK;
}

static DFBResult
stmfb_aux_InitPool (CoreDFB                    *core,
                    CoreSurfacePool            *pool,
                    void                       *pool_data,
                    void                       *pool_local,
                    void                       *system_data,
                    CoreSurfacePoolDescription *ret_desc)
{
  DFBResult                      ret;
  STMFB_AuxSurfacePoolData      * const data  = pool_data;
  STMFB_AuxSurfacePoolLocalData * const local = pool_local;
  const STMfbdev                * const stmfbdev = dfb_system_data ();
  struct stmfbio_auxmem2         auxmem;

  D_DEBUG_AT (STMfbdev_Surfaces_Aux, "%s (%d)\n", __func__,
              stmfbdev->shared->aux_pool_index);

  D_ASSERT (core != NULL);
  D_MAGIC_ASSERT (pool, CoreSurfacePool);
  D_ASSERT (data != NULL);
  D_ASSERT (local != NULL);
  D_ASSERT (ret_desc != NULL);

  D_UNUSED_P (local);

  D_MAGIC_SET (data, STMFB_AuxSurfacePoolData);

  auxmem.index = stmfbdev->shared->aux_pool_index;

  snprintf (ret_desc->name, DFB_SURFACE_POOL_DESC_NAME_LENGTH,
            "STMFB AuxMemory %d", auxmem.index);

  /* query auxmem */
  if (ioctl (stmfbdev->fd, STMFBIO_GET_AUXMEMORY2, &auxmem) != 0)
    return DFB_IO;

  if (!auxmem.size)
    return DFB_ITEMNOTFOUND;

  D_INFO ("STMfbdev/Surfaces/Aux: found auxmem @ %.8x (%ukB)!\n",
          auxmem.physical, auxmem.size / 1024);
  /* auxmem partitions will never cross a 64MB boundary, stmfb should make
     sure that's the case. And it does in fact :-), which is why we don't need
     to check for configuration errors. */

  ret = dfb_surfacemanager_create (core, auxmem.size, &data->manager);
  if (ret)
    return ret;

  data->aux_part  = auxmem.index;
  data->part_base = auxmem.physical;

  ret_desc->caps              = CSPCAPS_PHYSICAL | CSPCAPS_VIRTUAL;
  ret_desc->access[CSAID_CPU] = CSAF_READ | CSAF_WRITE | CSAF_SHARED;
  ret_desc->access[CSAID_GPU] = CSAF_READ | CSAF_WRITE | CSAF_SHARED;
  ret_desc->types             = (0
                                 | CSTF_WINDOW | CSTF_CURSOR | CSTF_FONT
                                 | CSTF_SHARED | CSTF_EXTERNAL
                                 | CSTF_BUFFEROBJECT
                                );
  ret_desc->priority          = CSPP_PREFERED;
  ret_desc->size              = auxmem.size;

  /* other accelerators */
  ret_desc->access[CSAID_ACCEL0] = CSAF_READ | CSAF_WRITE;
  ret_desc->access[CSAID_ACCEL1] = CSAF_READ | CSAF_WRITE;
  ret_desc->access[CSAID_ACCEL2] = CSAF_READ | CSAF_WRITE;
  ret_desc->access[CSAID_ACCEL3] = CSAF_READ | CSAF_WRITE;
  ret_desc->access[CSAID_ACCEL4] = CSAF_READ | CSAF_WRITE;
  ret_desc->access[CSAID_ACCEL5] = CSAF_READ | CSAF_WRITE;

#if 0
  /* FIXME: this depends on the hardware, but we have no interface at the
     moment anyway... */
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
#endif

  ret = stmfb_aux_JoinPool (core, pool, pool_data, pool_local, system_data);
  if (ret)
    {
      stmfb_aux_DestroyPool (pool, pool_data, pool_local);
      return ret;
    }

  return DFB_OK;
}

static DFBResult
stmfb_aux_TestConfig (CoreSurfacePool         *pool,
                      void                    *pool_data,
                      void                    *pool_local,
                      CoreSurfaceBuffer       *buffer,
                      const CoreSurfaceConfig *config)
{
  CoreSurface                   *surface;
  STMFB_AuxSurfacePoolData      * const data  = pool_data;
  STMFB_AuxSurfacePoolLocalData * const local = pool_local;
  DFBResult                      ret;

  D_MAGIC_ASSERT (pool, CoreSurfacePool);
  D_MAGIC_ASSERT (data, STMFB_AuxSurfacePoolData);
  D_MAGIC_ASSERT (local, STMFB_AuxSurfacePoolLocalData);
  D_MAGIC_ASSERT (buffer, CoreSurfaceBuffer);
  D_ASSERT (config != NULL);

  surface = buffer->surface;
  D_MAGIC_ASSERT (surface, CoreSurface);

  D_DEBUG_AT (STMfbdev_Surfaces_Aux,
              "%s (part/buffer/type/id: %u/%p/0x%x/%lu)\n",
              __func__, data->aux_part, buffer,
              surface->type, surface->resource_id);

  /* FIXME: this depends on the hardware, but we have no interface at the
     moment anyway... */
  if (surface->type & CSTF_LAYER)
    return DFB_BUG;

  ret = dfb_surfacemanager_allocate (local->core, data->manager,
                                     buffer, NULL, NULL, false);

  D_DEBUG_AT (STMfbdev_Surfaces_Aux, "  -> %s\n", DirectFBErrorString (ret));

  return ret;
}

static DFBResult
stmfb_aux_AllocateBuffer (CoreSurfacePool       *pool,
                          void                  *pool_data,
                          void                  *pool_local,
                          CoreSurfaceBuffer     *buffer,
                          CoreSurfaceAllocation *allocation,
                          void                  *alloc_data)
{
  CoreSurface                        *surface;
  STMFB_AuxSurfacePoolData           * const data  = pool_data;
  STMFB_AuxSurfacePoolLocalData      * const local = pool_local;
  STMFB_AuxSurfacePoolAllocationData * const alloc = alloc_data;
  DFBResult                           ret;
  Chunk                              *chunk;

  D_DEBUG_AT (STMfbdev_Surfaces_Aux, "%s (%d, %p)\n", __func__,
              data->aux_part, buffer);

  D_MAGIC_ASSERT (pool, CoreSurfacePool);
  D_MAGIC_ASSERT (data, STMFB_AuxSurfacePoolData);
  D_MAGIC_ASSERT (local, STMFB_AuxSurfacePoolLocalData);
  D_MAGIC_ASSERT (buffer, CoreSurfaceBuffer);
  D_MAGIC_ASSERT (allocation, CoreSurfaceAllocation);

  surface = buffer->surface;
  D_MAGIC_ASSERT (surface, CoreSurface);

  /* FIXME: this depends on the hardware, but we have no interface at the
     moment anyway... */
  if (surface->type & CSTF_LAYER)
    return DFB_BUG;

  ret = dfb_surfacemanager_allocate (local->core, data->manager, buffer,
                                     allocation, &chunk, false);
  if (ret)
    return ret;

  D_MAGIC_ASSERT (chunk, Chunk);

  alloc->chunk = chunk;

  D_DEBUG_AT (STMfbdev_Surfaces_Aux,
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

      D_DEBUG_AT (STMfbdev_Surfaces_Aux, "  -> rgb32 allocation!\n");

      D_WARN ("STMfbdev/Surfaces/Aux: RGB32 support is experimental and slow!");

      dfb_gfxcard_lock (GDLF_WAIT);

      _bdisp_aq_RGB32_init (stdrv, stdev,
                            data->part_base + chunk->offset, chunk->pitch,
                            &rect);
      dfb_gfxcard_unlock ();
    }
#endif

  D_MAGIC_SET (alloc, STMFB_AuxSurfacePoolAllocationData);

  return DFB_OK;
}

static DFBResult
stmfb_aux_DeallocateBuffer (CoreSurfacePool       *pool,
                            void                  *pool_data,
                            void                  *pool_local,
                            CoreSurfaceBuffer     *buffer,
                            CoreSurfaceAllocation *allocation,
                            void                  *alloc_data)
{
  STMFB_AuxSurfacePoolData            * const data  = pool_data;
  const STMFB_AuxSurfacePoolLocalData * const local = pool_local;
  STMFB_AuxSurfacePoolAllocationData  * const alloc = alloc_data;

  D_DEBUG_AT (STMfbdev_Surfaces_Aux, "%s (%d, %p)\n", __func__,
              data->aux_part, buffer);

  D_MAGIC_ASSERT (pool, CoreSurfacePool);
  D_MAGIC_ASSERT (data, STMFB_AuxSurfacePoolData);
  D_MAGIC_ASSERT (local, STMFB_AuxSurfacePoolLocalData);
  D_MAGIC_ASSERT (allocation, CoreSurfaceAllocation);
  D_MAGIC_ASSERT (alloc, STMFB_AuxSurfacePoolAllocationData);

  (void) local;

  D_ASSERT (alloc->chunk != NULL);
  dfb_surfacemanager_deallocate (data->manager, alloc->chunk);

  D_MAGIC_CLEAR (alloc);

  return DFB_OK;
}

static DFBResult
stmfb_aux_MuckOut (CoreSurfacePool   *pool,
                   void              *pool_data,
                   void              *pool_local,
                   CoreSurfaceBuffer *buffer )
{
  STMFB_AuxSurfacePoolData            * const data  = pool_data;
  const STMFB_AuxSurfacePoolLocalData * const local = pool_local;

  D_DEBUG_AT (STMfbdev_Surfaces_Aux, "%s (%p)\n", __func__, buffer);

  D_MAGIC_ASSERT (pool, CoreSurfacePool);
  D_MAGIC_ASSERT (data, STMFB_AuxSurfacePoolData);
  D_MAGIC_ASSERT (local, STMFB_AuxSurfacePoolLocalData);
  D_MAGIC_ASSERT (buffer, CoreSurfaceBuffer);

  return dfb_surfacemanager_displace (local->core, data->manager, buffer);
}

static DFBResult
stmfb_aux_Lock (CoreSurfacePool       *pool,
                void                  *pool_data,
                void                  *pool_local,
                CoreSurfaceAllocation *allocation,
                void                  *alloc_data,
                CoreSurfaceBufferLock *lock)
{
  const STMFB_AuxSurfacePoolData           * const data  = pool_data;
  const STMFB_AuxSurfacePoolLocalData      * const local = pool_local;
  const STMFB_AuxSurfacePoolAllocationData * const alloc = alloc_data;
  const Chunk                              *chunk;

  D_MAGIC_ASSERT (pool, CoreSurfacePool);
  D_MAGIC_ASSERT (data, STMFB_AuxSurfacePoolData);
  D_MAGIC_ASSERT (local, STMFB_AuxSurfacePoolLocalData);
  D_MAGIC_ASSERT (allocation, CoreSurfaceAllocation);
  D_MAGIC_ASSERT (alloc, STMFB_AuxSurfacePoolAllocationData);
  D_MAGIC_ASSERT (lock, CoreSurfaceBufferLock);

  D_DEBUG_AT (STMfbdev_SurfLock_Aux, "%s (%d, %p)\n", __func__,
              data->aux_part, lock->buffer);

  D_MAGIC_ASSERT (alloc->chunk, Chunk);
  chunk = alloc->chunk;

  lock->pitch  = chunk->pitch;
  lock->offset = chunk->offset;
  lock->addr   = local->mem + chunk->offset;
  lock->phys   = data->part_base + chunk->offset;

  D_DEBUG_AT (STMfbdev_SurfLock_Aux,
              "  -> offset 0x%.8lx (%lu), pitch %d, addr %p, phys 0x%.8lx\n",
              lock->offset, lock->offset, lock->pitch, lock->addr, lock->phys);

  return DFB_OK;
}

static DFBResult
stmfb_aux_Unlock (CoreSurfacePool       *pool,
                  void                  *pool_data,
                  void                  *pool_local,
                  CoreSurfaceAllocation *allocation,
                  void                  *alloc_data,
                  CoreSurfaceBufferLock *lock)
{
  const STMFB_AuxSurfacePoolData           * const data  = pool_data;
  const STMFB_AuxSurfacePoolLocalData      * const local = pool_local;
  const STMFB_AuxSurfacePoolAllocationData * const alloc = alloc_data;

  D_MAGIC_ASSERT (pool, CoreSurfacePool);
  D_MAGIC_ASSERT (data, STMFB_AuxSurfacePoolData);
  D_MAGIC_ASSERT (local, STMFB_AuxSurfacePoolLocalData);
  D_MAGIC_ASSERT (allocation, CoreSurfaceAllocation);
  D_MAGIC_ASSERT (alloc, STMFB_AuxSurfacePoolAllocationData);
  D_MAGIC_ASSERT (lock, CoreSurfaceBufferLock);

  D_DEBUG_AT (STMfbdev_SurfLock_Aux, "%s (%d, %p)\n", __func__,
              data->aux_part, lock->buffer);

  D_UNUSED_P (data);
  D_UNUSED_P (local);
  D_UNUSED_P (alloc);

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

      D_DEBUG_AT (STMfbdev_SurfLock_Aux, "  -> rgb32 write release!\n");
      dfb_gfxcard_lock (GDLF_WAIT);
      _bdisp_aq_RGB32_fixup (stdrv, stdev,
                             lock->phys, lock->pitch,
                             &rect);
      dfb_gfxcard_unlock ();
    }
#endif

  return DFB_OK;
}

static const SurfacePoolFuncs stmfb_aux_SurfacePoolFuncs = {
  .PoolDataSize       = stmfb_aux_PoolDataSize,
  .PoolLocalDataSize  = stmfb_aux_PoolLocalDataSize,
  .AllocationDataSize = stmfb_aux_AllocationDataSize,

  .InitPool    = stmfb_aux_InitPool,
  .JoinPool    = stmfb_aux_JoinPool,
  .DestroyPool = stmfb_aux_DestroyPool,
  .LeavePool   = stmfb_aux_LeavePool,

  .TestConfig       = stmfb_aux_TestConfig,
  .AllocateBuffer   = stmfb_aux_AllocateBuffer,
  .DeallocateBuffer = stmfb_aux_DeallocateBuffer,

  .MuckOut = stmfb_aux_MuckOut,

  .Lock   = stmfb_aux_Lock,
  .Unlock = stmfb_aux_Unlock,
};

void
stmfb_aux_surface_pools_init (CoreDFB * const core)
{
  unsigned long           total_auxmem = 0;
  STMfbdev               * const stmfbdev = dfb_system_data ();
  struct stmfbio_auxmem2  auxmem;
  int                     i;

  D_ASSERT (dfb_core_is_master (core));

  D_ASSUME (D_ARRAY_SIZE (stmfbdev->shared->aux_pools) ==
            (STMFBGP_GFX_LAST - STMFBGP_GFX_FIRST + 1));

  for (i = -1, auxmem.index = 0;
       auxmem.index < D_ARRAY_SIZE (stmfbdev->shared->aux_pools);
       ++auxmem.index)
    {
      CoreSurfacePool *pool;

      if (ioctl (stmfbdev->fd, STMFBIO_GET_AUXMEMORY2, &auxmem) != 0)
        break;

      if (!auxmem.size)
        continue;

      stmfbdev->shared->aux_pool_index = auxmem.index;

      if (dfb_surface_pool_initialize (core,
                                       &stmfb_aux_SurfacePoolFuncs, &pool))
        continue;

      stmfbdev->shared->aux_pools[++i] = pool;
      total_auxmem += pool->desc.size;
    }

  stmfbdev->shared->aux_pool_index = -1;

  if (total_auxmem)
    D_INFO ("STMfbdev/Surfaces/Aux: %lukB of auxmem!\n", total_auxmem / 1024);
  else
    D_INFO ("STMfbdev/Surfaces/Aux: no auxmem available!\n");
}

void
stmfb_aux_surface_pools_destroy (CoreDFB * const core)
{
  STMfbdev * const stmfbdev = dfb_system_data ();
  int       i;

  D_ASSERT (dfb_core_is_master (core));

  for (i = 0; i < D_ARRAY_SIZE (stmfbdev->shared->aux_pools); ++i)
    {
      CoreSurfacePool * const pool = stmfbdev->shared->aux_pools[i];
      if (!pool)
        continue;

      if (dfb_surface_pool_destroy (pool))
        continue;

      stmfbdev->shared->aux_pools[i] = NULL;
    }
}

void
stmfb_aux_surface_pools_join (CoreDFB * const core)
{
  STMfbdev      * const stmfbdev = dfb_system_data ();
  unsigned long  total_auxmem = 0;
  int            i;

  D_ASSERT (!dfb_core_is_master (core));

  for (i = 0; i < D_ARRAY_SIZE (stmfbdev->shared->aux_pools); ++i)
    {
      CoreSurfacePool * const pool = stmfbdev->shared->aux_pools[i];
      if (!pool)
        continue;

      if (dfb_surface_pool_join (core, pool, &stmfb_aux_SurfacePoolFuncs))
        continue;

      total_auxmem += pool->desc.size;
    }

  if (total_auxmem)
    D_INFO ("STMfbdev/Surfaces/Aux: %lukB of auxmem!\n", total_auxmem / 1024);
  else
    D_INFO ("STMfbdev/Surfaces/Aux: no auxmem available!\n");
}

void
stmfb_aux_surface_pools_leave (CoreDFB * const core)
{
  STMfbdev * const stmfbdev = dfb_system_data ();
  int       i;

  D_ASSERT (!dfb_core_is_master (core));

  for (i = 0; i < D_ARRAY_SIZE (stmfbdev->shared->aux_pools); ++i)
    {
      CoreSurfacePool * const pool = stmfbdev->shared->aux_pools[i];
      if (!pool)
        continue;

      dfb_surface_pool_leave (pool);
    }
}
