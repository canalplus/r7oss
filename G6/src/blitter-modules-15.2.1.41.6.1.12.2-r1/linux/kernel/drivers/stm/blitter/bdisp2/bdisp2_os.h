#ifndef __BDISP2_OS_H__
#define __BDISP2_OS_H__


#include <stdbool.h>

struct stm_bdisp2_driver_data;
struct stm_bdisp2_device_data;
struct stm_bdisp2_dma_area;

#include "bdisp2/bdispII_driver_features.h"
#include "blitter_os.h"

#include <linux/stm/bdisp2_nodegroups.h>
#include "bdisp2/bdispII_aq_state.h"

#ifdef __KERNEL__
# include "bdisp2_os_linuxkernel.h"
#else
# include "bdisp2_os_directfbglibc.h"
#endif


#ifndef likely
#  define likely(x)       __builtin_expect(!!(x),1)
#endif /* likely */
#ifndef unlikely
#  define unlikely(x)     __builtin_expect(!!(x),0)
#endif /* unlikely */


#ifdef BDISP2_PRINT_NODE_WAIT_TIME

void *bdisp2_wait_space_start_timer_os (void);
void  bdisp2_wait_space_end_timer_os (void *handle);

#  define bdisp2_wait_space_start_timer() \
     ({ \
       void *__timeh = bdisp2_wait_space_start_timer_os ();

#  define bdisp2_wait_space_end_timer() \
       bdisp2_wait_space_end_timer_os (__timeh); \
     })

#else /* BDISP2_PRINT_NODE_WAIT_TIME */

#  define bdisp2_wait_space_start_timer() \
     ({

#  define bdisp2_wait_space_end_timer() \
      })

#endif /* BDISP2_PRINT_NODE_WAIT_TIME */


static inline bool
bdisp2_is_idle (const struct stm_bdisp2_driver_data * const stdrv)
{
  return !!(bdisp2_get_reg (stdrv, BDISP_STA) & BDISP_STA_IDLE);
}

static inline uint32_t
bdisp2_get_AQ_reg (const struct stm_bdisp2_driver_data * const stdrv,
                   const struct stm_bdisp2_device_data * const stdev,
                   uint16_t                             offset)
{
  return bdisp2_get_reg (stdrv,
                         offset + (BDISP_AQ1_BASE + (stdev->queue_id * 0x10)));
}

static inline void
bdisp2_set_AQ_reg (struct stm_bdisp2_driver_data       * const stdrv,
                   const struct stm_bdisp2_device_data * const stdev,
                   uint16_t                             offset,
                   uint32_t                             value)
{
  bdisp2_set_reg (stdrv,
                  offset + (BDISP_AQ1_BASE + (stdev->queue_id * 0x10)),
                  value);
}


int
bdisp2_alloc_memory_os (struct stm_bdisp2_driver_data *stdrv,
                        struct stm_bdisp2_device_data *stdev,
                        int                            alignment,
                        bool                           cached,
                        struct stm_bdisp2_dma_area    *dma_mem);
void
bdisp2_free_memory_os (struct stm_bdisp2_driver_data *stdrv,
                       struct stm_bdisp2_device_data *stdev,
                       struct stm_bdisp2_dma_area    *dma_mem);

int
bdisp2_engine_sync_os (struct stm_bdisp2_driver_data       *stdrv,
                       const struct stm_bdisp2_device_data *stdev);

void
bdisp2_wait_space_os (struct stm_bdisp2_driver_data       *stdrv,
                      const struct stm_bdisp2_device_data *stdev);

void
bdisp2_wait_fence_os (struct stm_bdisp2_driver_data       *stdrv,
                      const struct stm_bdisp2_device_data *stdev,
                      const stm_blitter_serial_t          *serial);


#endif /* __BDISP2_OS_H__ */
