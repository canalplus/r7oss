/*
   ST Microelectronics BDispII driver - surface pool memory (auxmem)

   (c) Copyright 2009-2010  STMicroelectronics Ltd.

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
#include <core/gfxcard.h>
#include <core/surface_pool.h>

#include <gfx/convert.h>

#include <misc/conf.h>

#include "surfacemanager.h"

#if STGFX_DRIVER == 2
  #include <core/system.h>
  #include "stm_gfxdriver.h"
  #include "bdisp_accel.h"
  typedef struct _STGFX2DriverData STGFXDriverData;
  typedef struct _STGFX2DeviceData STGFXDeviceData;
#elif STGFX_DRIVER == 1
  #include "stgfx.h"
#else
  #error unknown stgfx driver version
#endif
#include "bdisp_surface_pool.h"


D_DEBUG_DOMAIN (BDISP_Surfaces, "BDisp/Surfaces", "BDisp Surface Pool");
D_DEBUG_DOMAIN (BDISP_SurfLock, "BDisp/SurfLock", "BDisp Surface Pool Locks");

/****************************************************************************/

typedef struct
{
  int             magic;

  SurfaceManager *manager;

  unsigned long   part_base;
  unsigned int    aux_part;
} BDispSurfacePoolData;

typedef struct
{
  int      magic;

  CoreDFB *core;
  void    *mem;
} BDispSurfacePoolLocalData;

typedef struct
{
  int    magic;

  Chunk *chunk;
} BDispSurfacePoolAllocationData;

/****************************************************************************/

static int
bdispPoolDataSize (void)
{
  return sizeof (BDispSurfacePoolData);
}

static int
bdispPoolLocalDataSize (void)
{
  return sizeof (BDispSurfacePoolLocalData);
}

static int
bdispAllocationDataSize (void)
{
  return sizeof (BDispSurfacePoolAllocationData);
}

static DFBResult
bdispLeavePool (CoreSurfacePool *pool,
                void            *pool_data,
                void            *pool_local)
{
  BDispSurfacePoolData      * const data = pool_data;
  BDispSurfacePoolLocalData * const local = pool_local;

  D_DEBUG_AT (BDISP_Surfaces, "%s (%d)\n", __FUNCTION__, data->aux_part);

  D_MAGIC_ASSERT (pool, CoreSurfacePool);
  D_MAGIC_ASSERT (data, BDispSurfacePoolData);
  D_MAGIC_ASSERT (local, BDispSurfacePoolLocalData);

  (void) data; /* prevent warning if debug is disabled */

  if (local->mem && local->mem != MAP_FAILED)
    {
      munmap (local->mem, pool->desc.size);
      local->mem = NULL;
    }

  D_MAGIC_CLEAR (local);

  return DFB_OK;
}

static DFBResult
bdispJoinPool (CoreDFB         *core,
               CoreSurfacePool *pool,
               void            *pool_data,
               void            *pool_local,
               void            *system_data)
{
  BDispSurfacePoolData      * const data  = pool_data;
  BDispSurfacePoolLocalData * const local = pool_local;
  STGFXDriverData           * const stdrv = dfb_gfxcard_get_driver_data ();

  D_DEBUG_AT (BDISP_Surfaces, "%s (%d)\n", __FUNCTION__, data->aux_part);

  D_ASSERT (core != NULL);
  D_MAGIC_ASSERT (pool, CoreSurfacePool);
  D_MAGIC_ASSERT (data, BDispSurfacePoolData);
  D_ASSERT (local != NULL);

  D_MAGIC_SET (local, BDispSurfacePoolLocalData);

  local->mem = mmap (NULL, pool->desc.size, PROT_READ | PROT_WRITE,
                     MAP_SHARED, stdrv->fd, data->part_base);
  if (local->mem == MAP_FAILED)
    {
      D_PERROR ("BDisp/Surfaces: Could not mmap gfx part %d!\n",
                data->aux_part);
      return DFB_INIT;
    }

  local->core = core;

  return DFB_OK;
}

static DFBResult
bdispDestroyPool (CoreSurfacePool *pool,
                  void            *pool_data,
                  void            *pool_local)
{
  BDispSurfacePoolData      * const data  = pool_data;
  BDispSurfacePoolLocalData * const local = pool_local;

  D_DEBUG_AT (BDISP_Surfaces, "%s (%d)\n", __FUNCTION__, data->aux_part);

  D_MAGIC_ASSERT (pool, CoreSurfacePool);
  D_MAGIC_ASSERT (data, BDispSurfacePoolData);
  D_MAGIC_ASSERT (local, BDispSurfacePoolLocalData);

  (void) local; /* prevent warning if debug is disabled */

  bdispLeavePool (pool, pool_data, pool_local);

  dfb_surfacemanager_destroy (data->manager);

  D_MAGIC_CLEAR (data);

  return DFB_OK;
}

static DFBResult
bdispInitPool (CoreDFB                    *core,
               CoreSurfacePool            *pool,
               void                       *pool_data,
               void                       *pool_local,
               void                       *system_data,
               CoreSurfacePoolDescription *ret_desc)
{
  DFBResult                  ret;
  BDispSurfacePoolData      * const data  = pool_data;
  BDispSurfacePoolLocalData * const local = pool_local;
  STGFXDriverData           * const stdrv = dfb_gfxcard_get_driver_data ();
  STGFXDeviceData           * const stdev = dfb_gfxcard_get_device_data ();
  struct stmfbio_auxmem2     auxmem;

  D_DEBUG_AT (BDISP_Surfaces, "%s (%d)\n", __FUNCTION__,
              stdev->aux_pool_index);

  D_ASSERT (core != NULL);
  D_MAGIC_ASSERT (pool, CoreSurfacePool);
  D_ASSERT (data != NULL);
  D_ASSERT (local != NULL);
  D_ASSERT (ret_desc != NULL);

  (void) local; /* prevent warning if debug is disabled */

  D_MAGIC_SET (data, BDispSurfacePoolData);

  auxmem.index = stdev->aux_pool_index;

  snprintf (ret_desc->name, DFB_SURFACE_POOL_DESC_NAME_LENGTH,
            "BDispII AuxMemory %d", auxmem.index);

  /* query auxmem */
  if (ioctl (stdrv->fd, STMFBIO_GET_AUXMEMORY2, &auxmem) != 0)
    return DFB_IO;

  if (!auxmem.size)
    return DFB_ITEMNOTFOUND;

  D_INFO ("BDisp/Surfaces: found auxmem @ %.8x (%ukB)!\n",
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

  ret = bdispJoinPool (core, pool, pool_data, pool_local, system_data);
  if (ret)
    {
      bdispDestroyPool (pool, pool_data, pool_local);
      return ret;
    }

  return DFB_OK;
}

static DFBResult
bdispTestConfig (CoreSurfacePool         *pool,
                 void                    *pool_data,
                 void                    *pool_local,
                 CoreSurfaceBuffer       *buffer,
                 const CoreSurfaceConfig *config)
{
  CoreSurface               *surface;
  BDispSurfacePoolData      * const data  = pool_data;
  BDispSurfacePoolLocalData * const local = pool_local;
  DFBResult                  ret;

  D_MAGIC_ASSERT (pool, CoreSurfacePool);
  D_MAGIC_ASSERT (data, BDispSurfacePoolData);
  D_MAGIC_ASSERT (local, BDispSurfacePoolLocalData);
  D_MAGIC_ASSERT (buffer, CoreSurfaceBuffer);
  D_ASSERT (config != NULL);

  surface = buffer->surface;
  D_MAGIC_ASSERT (surface, CoreSurface);

  D_DEBUG_AT (BDISP_Surfaces, "%s (part/buffer/type/id: %u/%p/0x%x/%lu)\n",
              __FUNCTION__, data->aux_part, buffer,
              surface->type, surface->resource_id);

  /* FIXME: this depends on the hardware, but we have no interface at the
     moment anyway... */
  if (surface->type & CSTF_LAYER)
    return DFB_BUG;

  ret = dfb_surfacemanager_allocate (local->core, data->manager,
                                     buffer, NULL, NULL);

  D_DEBUG_AT (BDISP_Surfaces, "  -> %s\n", DirectFBErrorString (ret));

  return ret;
}

static DFBResult
bdispAllocateBuffer (CoreSurfacePool       *pool,
                     void                  *pool_data,
                     void                  *pool_local,
                     CoreSurfaceBuffer     *buffer,
                     CoreSurfaceAllocation *allocation,
                     void                  *alloc_data)
{
  CoreSurface                    *surface;
  BDispSurfacePoolData           * const data  = pool_data;
  BDispSurfacePoolLocalData      * const local = pool_local;
  BDispSurfacePoolAllocationData * const alloc = alloc_data;
  DFBResult                       ret;
  Chunk                          *chunk;

  D_DEBUG_AT (BDISP_Surfaces, "%s (%d, %p)\n", __FUNCTION__,
              data->aux_part, buffer);

  D_MAGIC_ASSERT (pool, CoreSurfacePool);
  D_MAGIC_ASSERT (data, BDispSurfacePoolData);
  D_MAGIC_ASSERT (local, BDispSurfacePoolLocalData);
  D_MAGIC_ASSERT (buffer, CoreSurfaceBuffer);
  D_MAGIC_ASSERT (allocation, CoreSurfaceAllocation);

  surface = buffer->surface;
  D_MAGIC_ASSERT (surface, CoreSurface);

  /* FIXME: this depends on the hardware, but we have no interface at the
     moment anyway... */
  if (surface->type & CSTF_LAYER)
    return DFB_BUG;

  ret = dfb_surfacemanager_allocate (local->core, data->manager, buffer,
                                     allocation, &chunk);
  if (ret)
    return ret;

  D_MAGIC_ASSERT (chunk, Chunk);

  alloc->chunk = chunk;

  D_DEBUG_AT (BDISP_Surfaces,
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
      STGFX2DriverData * const stdrv = dfb_gfxcard_get_driver_data ();
      STGFX2DeviceData * const stdev = dfb_gfxcard_get_device_data ();
      DFBRectangle      rect = { .x = 0, .y = 0,
                                 .w = buffer->surface->config.size.w,
                                 .h = buffer->surface->config.size.h };

      D_WARN ("BDisp/Surfaces: RGB32 support is experimental and slow!");
      if (dfb_system_type () != CORE_STMFBDEV)
        D_WARN ("BDisp/Surfaces: RGB32 is only supported in STMfbdev system!");

      D_DEBUG_AT (BDISP_Surfaces, "  -> rgb32 allocation!\n");
      dfb_gfxcard_lock (GDLF_WAIT);

      _bdisp_aq_RGB32_init (stdrv, stdev,
                            data->part_base + chunk->offset, chunk->pitch,
                            &rect);
      dfb_gfxcard_unlock ();
    }
#endif

  D_MAGIC_SET (alloc, BDispSurfacePoolAllocationData);

  return DFB_OK;
}

static DFBResult
bdispDeallocateBuffer (CoreSurfacePool       *pool,
                       void                  *pool_data,
                       void                  *pool_local,
                       CoreSurfaceBuffer     *buffer,
                       CoreSurfaceAllocation *allocation,
                       void                  *alloc_data)
{
  BDispSurfacePoolData            * const data  = pool_data;
  const BDispSurfacePoolLocalData * const local = pool_local;
  BDispSurfacePoolAllocationData  * const alloc = alloc_data;

  D_DEBUG_AT (BDISP_Surfaces, "%s (%d, %p)\n", __FUNCTION__,
              data->aux_part, buffer);

  D_MAGIC_ASSERT (pool, CoreSurfacePool);
  D_MAGIC_ASSERT (data, BDispSurfacePoolData);
  D_MAGIC_ASSERT (local, BDispSurfacePoolLocalData);
  D_MAGIC_ASSERT (buffer, CoreSurfaceBuffer);
  D_MAGIC_ASSERT (allocation, CoreSurfaceAllocation);
  D_MAGIC_ASSERT (alloc, BDispSurfacePoolAllocationData);

  (void) local;

  D_ASSERT (alloc->chunk != NULL);
  dfb_surfacemanager_deallocate (data->manager, alloc->chunk);

  D_MAGIC_CLEAR (alloc);

  return DFB_OK;
}

static DFBResult
bdispMuckOut (CoreSurfacePool   *pool,
              void              *pool_data,
              void              *pool_local,
              CoreSurfaceBuffer *buffer )
{
  BDispSurfacePoolData            * const data  = pool_data;
  const BDispSurfacePoolLocalData * const local = pool_local;

  D_DEBUG_AT (BDISP_Surfaces, "%s (%p)\n", __FUNCTION__, buffer);

  D_MAGIC_ASSERT (pool, CoreSurfacePool);
  D_MAGIC_ASSERT (data, BDispSurfacePoolData);
  D_MAGIC_ASSERT (local, BDispSurfacePoolLocalData);
  D_MAGIC_ASSERT (buffer, CoreSurfaceBuffer);

  return dfb_surfacemanager_displace (local->core, data->manager, buffer);
}

static DFBResult
bdispLock (CoreSurfacePool       *pool,
           void                  *pool_data,
           void                  *pool_local,
           CoreSurfaceAllocation *allocation,
           void                  *alloc_data,
           CoreSurfaceBufferLock *lock)
{
  const BDispSurfacePoolData           * const data  = pool_data;
  const BDispSurfacePoolLocalData      * const local = pool_local;
  const BDispSurfacePoolAllocationData * const alloc = alloc_data;
  const Chunk                          *chunk;

  D_MAGIC_ASSERT (pool, CoreSurfacePool);
  D_MAGIC_ASSERT (data, BDispSurfacePoolData);
  D_MAGIC_ASSERT (local, BDispSurfacePoolLocalData);
  D_MAGIC_ASSERT (allocation, CoreSurfaceAllocation);
  D_MAGIC_ASSERT (alloc, BDispSurfacePoolAllocationData);
  D_MAGIC_ASSERT (lock, CoreSurfaceBufferLock);

  D_DEBUG_AT (BDISP_SurfLock, "%s (%d, %p)\n", __FUNCTION__,
              data->aux_part, lock->buffer);

  D_MAGIC_ASSERT (alloc->chunk, Chunk);
  chunk = alloc->chunk;

  lock->pitch  = chunk->pitch;
  lock->offset = chunk->offset;
  lock->addr   = local->mem + chunk->offset;
  lock->phys   = data->part_base + chunk->offset;

  D_DEBUG_AT (BDISP_SurfLock,
              "  -> offset 0x%.8lx (%lu), pitch %d, addr %p, phys 0x%.8lx\n",
              lock->offset, lock->offset, lock->pitch, lock->addr, lock->phys);

  return DFB_OK;
}

static DFBResult
bdispUnlock (CoreSurfacePool       *pool,
             void                  *pool_data,
             void                  *pool_local,
             CoreSurfaceAllocation *allocation,
             void                  *alloc_data,
             CoreSurfaceBufferLock *lock)
{
  const BDispSurfacePoolData           * const data  = pool_data;
  const BDispSurfacePoolLocalData      * const local = pool_local;
  const BDispSurfacePoolAllocationData * const alloc = alloc_data;

  D_MAGIC_ASSERT (pool, CoreSurfacePool);
  D_MAGIC_ASSERT (data, BDispSurfacePoolData);
  D_MAGIC_ASSERT (local, BDispSurfacePoolLocalData);
  D_MAGIC_ASSERT (allocation, CoreSurfaceAllocation);
  D_MAGIC_ASSERT (alloc, BDispSurfacePoolAllocationData);
  D_MAGIC_ASSERT (lock, CoreSurfaceBufferLock);

  D_DEBUG_AT (BDISP_SurfLock, "%s (%d, %p)\n", __FUNCTION__,
              data->aux_part, lock->buffer);

  (void) data;
  (void) local;
  (void) alloc;

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

      D_DEBUG_AT (BDISP_SurfLock, "  -> rgb32 write release!\n");
      dfb_gfxcard_lock (GDLF_WAIT);
      _bdisp_aq_RGB32_fixup (stdrv, stdev,
                             lock->phys, lock->pitch,
                             &rect);
      dfb_gfxcard_unlock ();
    }
#endif

  return DFB_OK;
}

static const SurfacePoolFuncs bdispSurfacePoolFuncs = {
  .PoolDataSize       = bdispPoolDataSize,
  .PoolLocalDataSize  = bdispPoolLocalDataSize,
  .AllocationDataSize = bdispAllocationDataSize,

  .InitPool    = bdispInitPool,
  .JoinPool    = bdispJoinPool,
  .DestroyPool = bdispDestroyPool,
  .LeavePool   = bdispLeavePool,

  .TestConfig       = bdispTestConfig,
  .AllocateBuffer   = bdispAllocateBuffer,
  .DeallocateBuffer = bdispDeallocateBuffer,

  .MuckOut = bdispMuckOut,

  .Lock   = bdispLock,
  .Unlock = bdispUnlock,
};

static inline void
bdisp_surface_pool_init (CoreDFB          * const core,
                         CoreSurfacePool ** const aux_pools,
                         int               max_pools)
{
  unsigned long           total_auxmem = 0;
  STGFXDriverData        * const stdrv = dfb_gfxcard_get_driver_data ();
  STGFXDeviceData        * const stdev = dfb_gfxcard_get_device_data ();
  struct stmfbio_auxmem2  auxmem;
  int                     i;

  D_ASSERT (dfb_core_is_master (core));

  for (i = -1, auxmem.index = 0; auxmem.index < max_pools; ++auxmem.index)
    {
      CoreSurfacePool *pool;

      if (ioctl (stdrv->fd, STMFBIO_GET_AUXMEMORY2, &auxmem) != 0)
        break;

      if (!auxmem.size)
        continue;

      stdev->aux_pool_index = auxmem.index;

      if (unlikely (dfb_surface_pool_initialize (core,
                                                 &bdispSurfacePoolFuncs,
                                                 &pool)))
        continue;

      aux_pools[++i] = pool;
      total_auxmem += pool->desc.size;
    }

  if (total_auxmem)
    D_INFO ("BDisp/Surfaces: %lukB of auxmem!\n", total_auxmem / 1024);
  else
    D_INFO ("BDisp/Surfaces: no auxmem available!\n");
}

static inline void
bdisp_surface_pool_destroy (CoreDFB          * const core,
                            CoreSurfacePool ** const aux_pools,
                            int               n_pools)
{
  int i;

  D_ASSERT (dfb_core_is_master (core));

  for (i = 0; i < n_pools; ++i)
    {
      CoreSurfacePool * const pool = aux_pools[i];
      if (!pool)
        continue;

      if (unlikely (dfb_surface_pool_destroy (pool)))
        continue;

      aux_pools[i] = NULL;
    }
}

static inline void
bdisp_surface_pool_join (CoreDFB                 * const core,
                         CoreSurfacePool * const * const aux_pools,
                         int                      n_pools)
{
  unsigned long total_auxmem = 0;
  int           i;

  D_ASSERT (!dfb_core_is_master (core));

  for (i = 0; i < n_pools; ++i)
    {
      CoreSurfacePool * const pool = aux_pools[i];
      if (!pool)
        continue;

      if (unlikely (dfb_surface_pool_join (core, pool,
                                           &bdispSurfacePoolFuncs)))
        continue;

      total_auxmem += pool->desc.size;
    }

  if (total_auxmem)
    D_INFO ("BDisp/Surfaces: %lukB of auxmem!\n", total_auxmem / 1024);
  else
    D_INFO ("BDisp/Surfaces: no auxmem available!\n");
}

static inline void
bdisp_surface_pool_leave (CoreDFB                 * const core,
                          CoreSurfacePool * const * const aux_pools,
                          int                      n_pools)
{
  int i;

  D_ASSERT (!dfb_core_is_master (core));

  for (i = 0; i < n_pools; ++i)
    {
      CoreSurfacePool * const pool = aux_pools[i];
      if (!pool)
        continue;

      dfb_surface_pool_leave (pool);
    }
}


#define _bdisp_surface_pool_funcs(v) \
void \
stgfx##v##_surface_pool_init (CoreDFB          * const core,  \
                              CoreSurfacePool ** const pools, \
                              int               max_pools)    \
{ \
  bdisp_surface_pool_init (core, pools, max_pools); \
} \
\
void \
stgfx##v##_surface_pool_destroy (CoreDFB          * const core,  \
                                 CoreSurfacePool ** const pools, \
                                 int               n_pools)      \
{ \
  bdisp_surface_pool_destroy (core, pools, n_pools); \
} \
\
void \
stgfx##v##_surface_pool_join (CoreDFB                 * const core,  \
                              CoreSurfacePool * const * const pools, \
                              int                      n_pools)      \
{ \
  bdisp_surface_pool_join (core, pools, n_pools); \
} \
\
void \
stgfx##v##_surface_pool_leave (CoreDFB                 * const core,  \
                               CoreSurfacePool * const * const pools, \
                               int                      n_pools)      \
{ \
  bdisp_surface_pool_leave (core, pools, n_pools); \
}

#define bdisp_surface_pool_funcs(driver) _bdisp_surface_pool_funcs(driver)

bdisp_surface_pool_funcs (STGFX_DRIVER);
