#include "config.h"

#include <sys/ioctl.h>
#include <sys/mman.h>

#include <directfb.h>
#include <core/coretypes.h>
#include <core/gfxcard.h>

#include <linux/stm/bdisp2_nodegroups.h>
#include "bdisp2/bdispII_aq_state.h"

#include "stgfx2_features.h"
#include "stm_gfxdriver.h"
#include "bdisp2/bdisp_accel.h"

#include "bdisp2_directfb_glue.h"
#include "directfb_state_glue.h"
#include "bdisp2/bdisp2_os.h"


#define offset_of(type, memb)     ((unsigned long)(&((type *)0)->memb))
#define container_of(ptr, type, member) ({ \
           const typeof( ((type *)0)->member ) *__mptr = (ptr); \
           (type *)( (char *)__mptr - offset_of(type,member) );})


int
bdisp2_engine_sync_os (struct stm_bdisp2_driver_data       * const stdrv,
                       const struct stm_bdisp2_device_data * const stdev)
{
  struct _STGFX2DriverData * const drvdata
    = container_of (stdrv, struct _STGFX2DriverData, stdrv);
  int ret = 0;

  while (!bdisp2_is_idle(stdrv)
         && ioctl (drvdata->fd_gfx, STM_BLITTER_SYNC) < 0)
    {
      if (errno == EINTR)
        continue;

      ret = errno;
      D_PERROR ("BDisp/BLT: STM_BLITTER_SYNC failed!\n" );

      direct_log_printf (NULL,
                         "  -> %srunning, CTL: %.8x IP/LNA/STA: %lu/%lu/%lu "
                         "next/last %lu/%lu\n",
                         !bdisp2_is_idle(stdrv) ? "" : "not ",
                         bdisp2_get_AQ_reg (stdrv, stdev, BDISP_AQ_CTL),
                         ((bdisp2_get_AQ_reg (stdrv, stdev, BDISP_AQ_IP)
                           - stdrv->bdisp_shared->nodes_phys)
                          / sizeof (struct _BltNode_Complete)),
                         ((bdisp2_get_AQ_reg (stdrv, stdev, BDISP_AQ_LNA)
                           - stdrv->bdisp_shared->nodes_phys)
                          / sizeof (struct _BltNode_Complete)),
                         ((bdisp2_get_AQ_reg (stdrv, stdev, BDISP_AQ_STA)
                           - stdrv->bdisp_shared->nodes_phys)
                          / sizeof (struct _BltNode_Complete)),
                         stdrv->bdisp_shared->next_free,
                         stdrv->bdisp_shared->last_free);

      break;
    }

  return ret;
}

int
bdisp2_wakeup_os (struct stm_bdisp2_driver_data * const stdrv)
{
  struct _STGFX2DriverData * const drvdata
    = container_of (stdrv, struct _STGFX2DriverData, stdrv);
  const struct stm_bdisp2_device_data * const stdev = stdrv->stdev;

  int ret = 0;

  while (ioctl (drvdata->fd_gfx, STM_BLITTER_WAKEUP) < 0)
    {
      if (errno == EINTR)
        continue;

      ret = errno;
      D_PERROR ("BDisp/BLT: STM_BLITTER_WAKEUP failed!\n" );

      direct_log_printf (NULL,
                         "  -> %srunning, CTL: %.8x IP/LNA/STA: %lu/%lu/%lu "
                         "next/last %lu/%lu\n",
                         !bdisp2_is_idle(stdrv) ? "" : "not ",
                         bdisp2_get_AQ_reg (stdrv, stdev, BDISP_AQ_CTL),
                         ((bdisp2_get_AQ_reg (stdrv, stdev, BDISP_AQ_IP)
                           - stdrv->bdisp_shared->nodes_phys)
                          / sizeof (struct _BltNode_Complete)),
                         ((bdisp2_get_AQ_reg (stdrv, stdev, BDISP_AQ_LNA)
                           - stdrv->bdisp_shared->nodes_phys)
                          / sizeof (struct _BltNode_Complete)),
                         ((bdisp2_get_AQ_reg (stdrv, stdev, BDISP_AQ_STA)
                           - stdrv->bdisp_shared->nodes_phys)
                          / sizeof (struct _BltNode_Complete)),
                         stdrv->bdisp_shared->next_free,
                         stdrv->bdisp_shared->last_free);

      break;
    }

  return ret;
}

void
bdisp2_wait_space_os (struct stm_bdisp2_driver_data       * const stdrv,
                      const struct stm_bdisp2_device_data * const stdev)
{
  struct _STGFX2DriverData * const drvdata
    = container_of (stdrv, struct _STGFX2DriverData, stdrv);

  while (ioctl (drvdata->fd_gfx, STM_BDISP2_WAIT_NEXT) < 0)
    {
      if (errno == EINTR)
        continue;

      D_PERROR ("BDisp/BLT: STM_BDISP2_WAIT_NEXT failed!\n" );

      direct_log_printf (NULL, "  -> %srunning, CTL: %.8x IP/LNA/STA: %lu/%lu/%lu"
                         " next/last %lu/%lu\n",
                         !bdisp2_is_idle(stdrv) ? "" : "not ",
                         bdisp2_get_AQ_reg (stdrv, stdev, BDISP_AQ_CTL),
                         ((bdisp2_get_AQ_reg (stdrv, stdev, BDISP_AQ_IP)
                           - stdrv->bdisp_shared->nodes_phys)
                          / sizeof (struct _BltNode_Complete)),
                         ((bdisp2_get_AQ_reg (stdrv, stdev, BDISP_AQ_LNA)
                           - stdrv->bdisp_shared->nodes_phys)
                          / sizeof (struct _BltNode_Complete)),
                         ((bdisp2_get_AQ_reg (stdrv, stdev, BDISP_AQ_STA)
                           - stdrv->bdisp_shared->nodes_phys)
                          / sizeof (struct _BltNode_Complete)),
                         stdrv->bdisp_shared->next_free,
                         stdrv->bdisp_shared->last_free);

      break;
    }
}

void
bdisp2_wait_fence_os (struct stm_bdisp2_driver_data       * const stdrv,
                      const struct stm_bdisp2_device_data * const stdev,
                      const stm_blitter_serial_t          * const serial)
{
  D_UNIMPLEMENTED();
}

int
bdisp2_alloc_memory_os (struct stm_bdisp2_driver_data * const stdrv,
                        struct stm_bdisp2_device_data * const stdev,
                        int                            alignment,
                        bool                           cached,
                        struct stm_bdisp2_dma_area    * const dma_mem)
{
  struct _STGFX2DriverData *drvdata;
  struct _STGFX2DeviceData *devdata;
  CoreGraphicsDevice       *device;
  GraphicsDeviceInfo       *device_info;
  int                       offset;

  drvdata = container_of (stdrv, struct _STGFX2DriverData, stdrv);
  devdata = container_of (stdev, struct _STGFX2DeviceData, stdev);

  device = drvdata->device;
  device_info = devdata->device_info;

  if (device_info->limits.surface_byteoffset_alignment < alignment)
    device_info->limits.surface_byteoffset_alignment = alignment;

  /* but else it's relatively simple, just reserve the memory... */
  offset = dfb_gfxcard_reserve_memory (device, dma_mem->size);
  if (offset <= 0)
      return -ENOMEM;

  dma_mem->base = dfb_gfxcard_memory_physical (device, offset);
  dma_mem->memory = dfb_gfxcard_memory_virtual (device, offset);

  return 0;
}

void
bdisp2_free_memory_os (struct stm_bdisp2_driver_data * const stdrv,
                       struct stm_bdisp2_device_data * const stdev,
                       struct stm_bdisp2_dma_area    * const dma_mem)
{
  /* nothing to do, we can't free it (and don't need to, as the master
     process is shutting down anyway */
}

#ifdef BDISP2_PRINT_NODE_WAIT_TIME
struct _my_timer_state {
  struct timeval start;
  struct timeval end;
};

void *
bdisp2_wait_space_start_timer_os (void)
{
  struct _my_timer_state *state;

  state = D_CALLOC (1, sizeof (*state));
  if (state)
    gettimeofday (&state->start, NULL);

  return state;
}

void
bdisp2_wait_space_end_timer_os (void *handle)
{
  if (handle)
    {
      struct _my_timer_state * const state = handle;

      gettimeofday (&state->end, NULL);
      timersub (&state->end, &state->start, &state->end);
      D_INFO ("BDisp/BLT: had to wait for space %lu.%06lus\n",
              state->end.tv_sec, state->end.tv_usec);

      D_FREE (state);
  }
}
#endif /* BDISP2_PRINT_NODE_WAIT_TIME */


static void
bdisp_set_dfb_features (enum stm_blitter_device_type  device,
                        const struct bdisp2_features *features,
                        struct _bdisp_dfb_features   *dfb_features)
{
  enum stm_blitter_surface_drawflags_e bdisp2_drawflags = features->drawflags;
  enum stm_blitter_surface_blitflags_e bdisp2_blitflags = features->blitflags;

  dfb_features->dfb_drawflags = DSDRAW_NOFX;
  dfb_features->dfb_blitflags = DSBLIT_NOFX;
  dfb_features->dfb_renderopts = DSRO_NONE;

  if (D_FLAGS_IS_SET (bdisp2_drawflags, STM_BLITTER_SDF_DST_COLORKEY))
    {
      bdisp2_drawflags &= ~STM_BLITTER_SDF_DST_COLORKEY;
      dfb_features->dfb_drawflags |= DSDRAW_DST_COLORKEY;
    }
  if (D_FLAGS_IS_SET (bdisp2_drawflags, STM_BLITTER_SDF_XOR))
    {
      bdisp2_drawflags &= ~STM_BLITTER_SDF_XOR;
      dfb_features->dfb_drawflags |= DSDRAW_XOR;
    }
  if (D_FLAGS_IS_SET (bdisp2_drawflags, STM_BLITTER_SDF_GRADIENT))
    {
      /* FIXME: STM_BLITTER_SDF_GRADIENT */
    }
  if (D_FLAGS_IS_SET (bdisp2_drawflags, STM_BLITTER_SDF_BLEND))
    {
      bdisp2_drawflags &= ~STM_BLITTER_SDF_BLEND;
      dfb_features->dfb_drawflags |= DSDRAW_BLEND;
    }
  if (D_FLAGS_IS_SET (bdisp2_drawflags, STM_BLITTER_SDF_SRC_PREMULTIPLY))
    {
      bdisp2_drawflags &= ~STM_BLITTER_SDF_SRC_PREMULTIPLY;
      dfb_features->dfb_drawflags |= DSDRAW_SRC_PREMULTIPLY;
    }
  if (D_FLAGS_IS_SET (bdisp2_drawflags, STM_BLITTER_SDF_DST_PREMULTIPLY))
    {
      bdisp2_drawflags &= ~STM_BLITTER_SDF_DST_PREMULTIPLY;
      dfb_features->dfb_drawflags |= DSDRAW_DST_PREMULTIPLY;
    }
  if (D_FLAGS_IS_SET (bdisp2_drawflags, STM_BLITTER_SDF_ANTIALIAS))
    {
      bdisp2_drawflags &= ~STM_BLITTER_SDF_ANTIALIAS;
      dfb_features->dfb_renderopts |= DSRO_ANTIALIAS;
    }

  if (D_FLAGS_ARE_SET (bdisp2_blitflags, STM_BLITTER_SBF_ALL_IN_FIXED_POINT))
    {
      bdisp2_blitflags &= ~STM_BLITTER_SBF_ALL_IN_FIXED_POINT;
#if HAVE_DECL_DSBLIT_FIXEDPOINT
      dfb_features->dfb_blitflags |= DSBLIT_FIXEDPOINT;
#endif /* HAVE_DECL_DSBLIT_FIXEDPOINT */
    }
  if (D_FLAGS_ARE_SET (bdisp2_blitflags, STM_BLITTER_SBF_SRC_COLORIZE))
    {
      /* FIXME: STM_BLITTER_SBF_SRC_COLORIZE is currently implemented to
         behave like DSBLIT_COLORIZE, but according to the API documentation
         it should behave more like DSBLIT_SRC_COLORMATRIX. */
      bdisp2_blitflags &= ~STM_BLITTER_SBF_SRC_COLORIZE;
      dfb_features->dfb_blitflags |= DSBLIT_COLORIZE;
      //dfb_features->dfb_blitflags |= DSBLIT_SRC_COLORMATRIX;
    }
  if (D_FLAGS_ARE_SET (bdisp2_blitflags, STM_BLITTER_SBF_SRC_COLORKEY))
    {
      bdisp2_blitflags &= ~STM_BLITTER_SBF_SRC_COLORKEY;
      dfb_features->dfb_blitflags |= DSBLIT_SRC_COLORKEY;
      /* FIXME: extended is not implemented */
      //dfb_features->dfb_blitflags |= DSBLIT_SRC_COLORKEY_EXTENDED;
    }
  if (D_FLAGS_ARE_SET (bdisp2_blitflags, STM_BLITTER_SBF_DST_COLORKEY))
    {
      bdisp2_blitflags &= ~STM_BLITTER_SBF_DST_COLORKEY;
      dfb_features->dfb_blitflags |= DSBLIT_DST_COLORKEY;
      /* FIXME: extended is not implemented */
      //dfb_features->dfb_blitflags |= DSBLIT_DST_COLORKEY_EXTENDED;
    }
  if (D_FLAGS_ARE_SET (bdisp2_blitflags, STM_BLITTER_SBF_XOR))
    {
      bdisp2_blitflags &= ~STM_BLITTER_SBF_XOR;
      dfb_features->dfb_blitflags |= DSBLIT_XOR;
    }
  if (D_FLAGS_ARE_SET (bdisp2_blitflags, STM_BLITTER_SBF_BLEND_ALPHACHANNEL))
    {
      bdisp2_blitflags &= ~STM_BLITTER_SBF_BLEND_ALPHACHANNEL;
      dfb_features->dfb_blitflags |= DSBLIT_BLEND_ALPHACHANNEL;
    }
  if (D_FLAGS_ARE_SET (bdisp2_blitflags, STM_BLITTER_SBF_BLEND_COLORALPHA))
    {
      bdisp2_blitflags &= ~STM_BLITTER_SBF_BLEND_COLORALPHA;
      dfb_features->dfb_blitflags |= DSBLIT_BLEND_COLORALPHA;
    }
  if (D_FLAGS_ARE_SET (bdisp2_blitflags, STM_BLITTER_SBF_SRC_PREMULTCOLOR))
    {
      bdisp2_blitflags &= ~STM_BLITTER_SBF_SRC_PREMULTCOLOR;
      dfb_features->dfb_blitflags |= DSBLIT_SRC_PREMULTCOLOR;
    }
  if (D_FLAGS_ARE_SET (bdisp2_blitflags, STM_BLITTER_SBF_SRC_PREMULTIPLY))
    {
      bdisp2_blitflags &= ~STM_BLITTER_SBF_SRC_PREMULTIPLY;
      dfb_features->dfb_blitflags |= DSBLIT_SRC_PREMULTIPLY;
    }
  if (D_FLAGS_ARE_SET (bdisp2_blitflags, STM_BLITTER_SBF_DST_PREMULTIPLY))
    {
      bdisp2_blitflags &= ~STM_BLITTER_SBF_DST_PREMULTIPLY;
      dfb_features->dfb_blitflags |= DSBLIT_DST_PREMULTIPLY;
    }
  if (D_FLAGS_ARE_SET (bdisp2_blitflags, STM_BLITTER_SBF_READ_SOURCE2))
    {
      bdisp2_blitflags &= ~STM_BLITTER_SBF_READ_SOURCE2;
#if HAVE_DECL_DSBLIT_SOURCE2
      dfb_features->dfb_blitflags |= DSBLIT_SOURCE2;
#endif
    }

  if (D_FLAGS_ARE_SET (bdisp2_blitflags, STM_BLITTER_SBF_FLIP_HORIZONTAL))
    dfb_features->dfb_blitflags |= DSBLIT_FLIP_HORIZONTAL;
  if (D_FLAGS_ARE_SET (bdisp2_blitflags, STM_BLITTER_SBF_FLIP_VERTICAL))
    dfb_features->dfb_blitflags |= DSBLIT_FLIP_VERTICAL;
  if (D_FLAGS_ARE_SET (bdisp2_blitflags, STM_BLITTER_SBF_ROTATE90))
    dfb_features->dfb_blitflags |= DSBLIT_ROTATE90;
  if (D_FLAGS_ARE_SET (bdisp2_blitflags, STM_BLITTER_SBF_ROTATE180))
    dfb_features->dfb_blitflags |= DSBLIT_ROTATE180;
  if (D_FLAGS_ARE_SET (bdisp2_blitflags, STM_BLITTER_SBF_ROTATE270))
    dfb_features->dfb_blitflags |= DSBLIT_ROTATE270;
  bdisp2_blitflags &= ~(STM_BLITTER_SBF_FLIP_HORIZONTAL
                        | STM_BLITTER_SBF_FLIP_VERTICAL
                        | STM_BLITTER_SBF_ROTATE90
                        | STM_BLITTER_SBF_ROTATE180
                        | STM_BLITTER_SBF_ROTATE270
                       );

  if (D_FLAGS_ARE_SET (bdisp2_blitflags, (STM_BLITTER_SBF_DEINTERLACE_BOTTOM
                                          | STM_BLITTER_SBF_DEINTERLACE_TOP)))
    {
      bdisp2_blitflags &= ~(STM_BLITTER_SBF_DEINTERLACE_BOTTOM
                            | STM_BLITTER_SBF_DEINTERLACE_TOP);
      /* FIXME: deinterlace is not implemented */
      //dfb_features->dfb_blitflags |= DSBLIT_DEINTERLACE;
    }

  if (D_FLAGS_ARE_SET (bdisp2_blitflags, STM_BLITTER_SBF_ANTIALIAS))
    {
      bdisp2_blitflags &= ~STM_BLITTER_SBF_ANTIALIAS;
      dfb_features->dfb_renderopts |= DSRO_ANTIALIAS;
    }
  if (D_FLAGS_ARE_SET (bdisp2_blitflags, STM_BLITTER_SBF_COLORMASK))
    {
      bdisp2_blitflags &= ~STM_BLITTER_SBF_COLORMASK;
      /* FIXME: not implemented */
      //dfb_features->dfb_renderopts |= DSRO_WRITE_MASK_BITS;
    }

  /* stuff that has no corresponding DirectFB API */
  bdisp2_drawflags &= ~STM_BLITTER_SDF_GRADIENT;
  bdisp2_blitflags &= ~STM_BLITTER_SBF_VC1RANGE_LUMA;
  bdisp2_blitflags &= ~STM_BLITTER_SBF_VC1RANGE_CHROMA;
  bdisp2_blitflags &= ~STM_BLITTER_SBF_FLICKER_FILTER;
  bdisp2_blitflags &= ~STM_BLITTER_SBF_STRICT_INPUT_RECT;
  /* stuff we always support (that isn't represented in the core driver and is
     DirectFB specific). */
  bdisp2_blitflags &= ~STM_BLITTER_SBF_NO_FILTER;
  dfb_features->dfb_renderopts |= (DSRO_SMOOTH_UPSCALE
                                   | DSRO_SMOOTH_DOWNSCALE);
  dfb_features->dfb_blitflags |= DSBLIT_INDEX_TRANSLATION;
  dfb_features->dfb_renderopts |= DSRO_MATRIX;

  if (bdisp2_drawflags)
    D_WARN ("BDisp/BLT: unhandled core driver draw flags: %.08x\n",
            bdisp2_drawflags);
  if (bdisp2_blitflags)
    D_WARN ("BDisp/BLT: unhandled core driver blit flags: %.08x\n",
            bdisp2_blitflags);


  {
  char * const drawflags_str = stgfx2_get_dfb_drawflags_string (dfb_features->dfb_drawflags);
  char * const blitflags_str = stgfx2_get_dfb_blitflags_string (dfb_features->dfb_blitflags);
  char * const renderopts_str = stgfx2_get_dfb_renderoptions_string (dfb_features->dfb_renderopts);

  D_INFO("BDisp/BLT: supported DirectFB draw flags: %s\n", drawflags_str);
  D_INFO("BDisp/BLT: supported DirectFB blit flags: %s\n", blitflags_str);
  D_INFO("BDisp/BLT: supported DirectFB render opt: %s\n", renderopts_str);

  free (renderopts_str);
  free (blitflags_str);
  free (drawflags_str);
  }
}

DFBResult
bdisp_aq_initialize (CoreGraphicsDevice       * const device,
                     GraphicsDeviceInfo       * const device_info,
                     struct _STGFX2DriverData * const drvdata,
                     struct _STGFX2DeviceData * const devdata)
{
  DFBResult res;
  long page_size;

  if ((page_size = (long)sysconf(_SC_PAGESIZE)) < 0)
    return DFB_BUG;

  if (ioctl (drvdata->fd_gfx,
             STM_BDISP2_GET_SHARED_AREA, &devdata->bdisp_shared_cookie) < 0
      || !devdata->bdisp_shared_cookie.cookie
      || !devdata->bdisp_shared_cookie.size)
    return DFB_NOVIDEOMEMORY;

  D_INFO ("BDisp/BLT: shared area phys/size: 0x%.8x/%u\n",
          devdata->bdisp_shared_cookie.cookie,
          devdata->bdisp_shared_cookie.size);

  /* the actual shared area */
  drvdata->stdrv.bdisp_shared = mmap (NULL,
                                      page_size,
                                      PROT_READ | PROT_WRITE, MAP_SHARED,
                                      drvdata->fd_gfx,
                                      devdata->bdisp_shared_cookie.cookie);
  if (drvdata->stdrv.bdisp_shared == MAP_FAILED)
    {
      D_PERROR ("BDisp/BLT: Could not map shared area!\n");
      drvdata->stdrv.bdisp_shared = NULL;
      return DFB_INIT;
    }

  /* the DMA memory (nodelist, filters) */
  drvdata->stdrv.bdisp_nodes.base = drvdata->stdrv.bdisp_shared->nodes_phys;
  drvdata->stdrv.bdisp_nodes.size = drvdata->stdrv.bdisp_shared->nodes_size;
  drvdata->stdrv.bdisp_nodes.memory = mmap (NULL,
                                            drvdata->stdrv.bdisp_nodes.size,
                                            PROT_READ | PROT_WRITE,
                                            MAP_SHARED, drvdata->fd_gfx,
                                            drvdata->stdrv.bdisp_nodes.base);
  if (drvdata->stdrv.bdisp_nodes.memory == MAP_FAILED)
    {
      D_PERROR ("BDisp/BLT: Could not map nodes area!\n");
      drvdata->stdrv.bdisp_nodes.memory = NULL;
      drvdata->stdrv.bdisp_nodes.base = 0;
      drvdata->stdrv.bdisp_nodes.size = 0;
      return DFB_INIT;
    }

  D_INFO ("BDisp/BLT: shared area/nodes mmap()ed to %p / %p\n",
          drvdata->stdrv.bdisp_shared, drvdata->stdrv.bdisp_nodes.memory);

  res = errno2result (bdisp2_initialize (&drvdata->stdrv, &devdata->stdev));
  if (res == DFB_OK)
    bdisp_set_dfb_features (drvdata->stdrv.bdisp_shared->plat_blit.device_type,
                            &devdata->stdev.features,
                            &devdata->dfb_features);

  return res;
}


void
bdisp_aq_EngineReset (void * const driver_data,
                      void * const device_data)
{
  struct _STGFX2DriverData * const drvdata = driver_data;
  struct _STGFX2DeviceData * const devdata = device_data;

  struct stm_bdisp2_driver_data * const stdrv = &drvdata->stdrv;
  struct stm_bdisp2_device_data * const stdev = &devdata->stdev;

  bdisp2_engine_reset (stdrv, stdev);
}

DFBResult
bdisp_aq_EngineSync (void * const driver_data,
                     void * const device_data)
{
  struct _STGFX2DriverData * const drvdata = driver_data;
  struct _STGFX2DeviceData * const devdata = device_data;

  struct stm_bdisp2_driver_data * const stdrv = &drvdata->stdrv;
  struct stm_bdisp2_device_data * const stdev = &devdata->stdev;

  return errno2result (bdisp2_engine_sync (stdrv, stdev));
}

void
bdisp_aq_EmitCommands (void * const driver_data,
                       void * const device_data)
{
  struct _STGFX2DriverData * const drvdata = driver_data;
  struct _STGFX2DeviceData * const devdata = device_data;

  struct stm_bdisp2_driver_data       * const stdrv = &drvdata->stdrv;
  const struct stm_bdisp2_device_data * const stdev = &devdata->stdev;

  bdisp2_emit_commands (stdrv, stdev, false);
}

void
bdisp_aq_GetSerial (void               * const driver_data,
                    void               * const device_data,
                    CoreGraphicsSerial * const dfb_serial)
{
  struct _STGFX2DriverData * const drvdata = driver_data;
  struct _STGFX2DeviceData * const devdata = device_data;

  struct stm_bdisp2_driver_data * const stdrv = &drvdata->stdrv;
  struct stm_bdisp2_device_data * const stdev = &devdata->stdev;

  stm_blitter_serial_t serial;

  bdisp2_get_serial (stdrv, stdev, &serial);

  dfb_serial->generation = serial >> 32;
  dfb_serial->serial     = serial & 0xffffffff;
}

DFBResult
bdisp_aq_WaitSerial (void                     * const driver_data,
                     void                     * const device_data,
                     const CoreGraphicsSerial * const dfb_serial)
{
  struct _STGFX2DriverData * const drvdata = driver_data;
  struct _STGFX2DeviceData * const devdata = device_data;

  struct stm_bdisp2_driver_data * const stdrv = &drvdata->stdrv;
  struct stm_bdisp2_device_data * const stdev = &devdata->stdev;

  stm_blitter_serial_t serial;

  serial = ((stm_blitter_serial_t) dfb_serial->generation) << 32;
  serial |= dfb_serial->serial;

  bdisp2_wait_serial (stdrv, stdev, serial);

  return DFB_OK;
}

bool
bdisp_aq_FillRectangle (void         * const driver_data,
                        void         * const device_data,
                        DFBRectangle * const drect)
{
  struct _STGFX2DriverData * const drvdata = driver_data;
  struct _STGFX2DeviceData * const devdata = device_data;

  struct stm_bdisp2_driver_data * const stdrv = &drvdata->stdrv;
  struct stm_bdisp2_device_data * const stdev = &devdata->stdev;

  stm_blitter_rect_t dst = { .position = { .x = drect->x,
                                           .y = drect->y },
                             .size = { .w = drect->w,
                                       .h = drect->h } };

  if (stdrv->funcs.fill_rect (stdrv, stdev, &dst))
    return false;

  return true;
}

bool
bdisp_aq_DrawRectangle (void         * const driver_data,
                        void         * const device_data,
                        DFBRectangle * const drect)
{
  struct _STGFX2DriverData * const drvdata = driver_data;
  struct _STGFX2DeviceData * const devdata = device_data;

  struct stm_bdisp2_driver_data * const stdrv = &drvdata->stdrv;
  struct stm_bdisp2_device_data * const stdev = &devdata->stdev;

  stm_blitter_rect_t dst = { .position = { .x = drect->x,
                                           .y = drect->y },
                             .size = { .w = drect->w,
                                       .h = drect->h } };

  if (stdrv->funcs.draw_rect (stdrv, stdev, &dst))
    return false;

  return true;
}

bool
bdisp_aq_Blit (void         * const driver_data,
               void         * const device_data,
               DFBRectangle * const srect,
               int           dx,
               int           dy)
{
  struct _STGFX2DriverData * const drvdata = driver_data;
  struct _STGFX2DeviceData * const devdata = device_data;

  struct stm_bdisp2_driver_data * const stdrv = &drvdata->stdrv;
  struct stm_bdisp2_device_data * const stdev = &devdata->stdev;

  stm_blitter_rect_t src = { .position = { .x = srect->x,
                                           .y = srect->y },
                             .size = { .w = srect->w,
                                       .h = srect->h } };
  stm_blitter_point_t dst_pt = { .x = dx,
                                 .y = dy };

  if (stdrv->funcs.blit (stdrv, stdev, &src, &dst_pt))
    return false;

  return true;
}

bool
bdisp_aq_StretchBlit (void         * const driver_data,
                      void         * const device_data,
                      DFBRectangle * const srect,
                      DFBRectangle * const drect)
{
  struct _STGFX2DriverData * const drvdata = driver_data;
  struct _STGFX2DeviceData * const devdata = device_data;

  struct stm_bdisp2_driver_data * const stdrv = &drvdata->stdrv;
  struct stm_bdisp2_device_data * const stdev = &devdata->stdev;

  stm_blitter_rect_t src = { .position = { .x = srect->x,
                                           .y = srect->y },
                             .size = { .w = srect->w,
                                       .h = srect->h } };
  stm_blitter_rect_t dst = { .position = { .x = drect->x,
                                           .y = drect->y },
                             .size = { .w = drect->w,
                                       .h = drect->h } };

  if (stdrv->funcs.stretch_blit (stdrv, stdev, &src, &dst))
    return false;

  return true;
}

bool
bdisp_aq_Blit2 (void         * const driver_data,
                void         * const device_data,
                DFBRectangle * const srect,
                int           dx,
                int           dy,
                int           sx2,
                int           sy2)
{
  struct _STGFX2DriverData * const drvdata = driver_data;
  struct _STGFX2DeviceData * const devdata = device_data;

  struct stm_bdisp2_driver_data * const stdrv = &drvdata->stdrv;
  struct stm_bdisp2_device_data * const stdev = &devdata->stdev;

  stm_blitter_rect_t src1 = { .position = { .x = srect->x,
                                            .y = srect->y },
                              .size = { .w = srect->w,
                                        .h = srect->h } };
  stm_blitter_point_t src2_pt = { .x = sx2, .y = sy2 };
  stm_blitter_point_t dst_pt  = { .x = dx, .y = dy };

  if (stdrv->funcs.blit2 (stdrv, stdev, &src1, &src2_pt, &dst_pt))
    return false;

  return true;
}

extern const enum stm_blitter_surface_format_e dspf_to_stm_blit[];
bool
bdisp_aq_StretchBlit_RLE (void                        * const driver_data,
                          void                        * const device_data,
                          unsigned long                src_address,
                          unsigned long                src_length,
                          const CoreSurfaceBufferLock * const dst,
                          const DFBRectangle          * const drect)
{
  struct _STGFX2DriverData * const drvdata = driver_data;
  struct _STGFX2DeviceData * const devdata = device_data;

  struct stm_bdisp2_driver_data * const stdrv = &drvdata->stdrv;
  struct stm_bdisp2_device_data * const stdev = &devdata->stdev;

  stm_blitter_rect_t rect = { .position = { .x = drect->x,
                                            .y = drect->y },
                              .size = { .w = drect->w,
                                        .h = drect->h } };

  enum stm_blitter_surface_format_e dst_format;

  dst_format = dspf_to_stm_blit[DFB_PIXELFORMAT_INDEX (dst->buffer->format)];

  if (bdisp2_stretch_blit_RLE (stdrv, stdev, src_address, src_length,
                               dst->phys, dst_format, dst->pitch,
                               &rect))
    return false;

  return true;
}

void
_bdisp_aq_RGB32_init (struct _STGFX2DriverData * const drvdata,
                      struct _STGFX2DeviceData * const devdata,
                      uint32_t                  blt_tba,
                      uint16_t                  pitch,
                      DFBRectangle             * const drect)
{
  struct stm_bdisp2_driver_data * const stdrv = &drvdata->stdrv;
  struct stm_bdisp2_device_data * const stdev = &devdata->stdev;

  stm_blitter_rect_t dst = { .position = { .x = drect->x,
                                           .y = drect->y },
                             .size = { .w = drect->w,
                                       .h = drect->h } };

  bdisp2_RGB32_init (stdrv, stdev, blt_tba, pitch, &dst);
}

void
_bdisp_aq_RGB32_fixup (struct _STGFX2DriverData * const drvdata,
                       struct _STGFX2DeviceData * const devdata,
                       uint32_t                  blt_tba,
                       uint16_t                  pitch,
                       DFBRectangle             * const drect)
{
  struct stm_bdisp2_driver_data * const stdrv = &drvdata->stdrv;
  struct stm_bdisp2_device_data * const stdev = &devdata->stdev;

  stm_blitter_rect_t dst = { .position = { .x = drect->x,
                                           .y = drect->y },
                             .size = { .w = drect->w,
                                       .h = drect->h } };

  bdisp2_RGB32_fixup (stdrv, stdev, blt_tba, pitch, &dst);
}
