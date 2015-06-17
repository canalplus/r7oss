#ifndef __BDISP2_OS_DIRECTFBGLIBC_H__
#define __BDISP2_OS_DIRECTFBGLIBC_H__

#include <direct/debug.h>
#include <direct/util.h>
#include <stdlib.h>

#define STM_BLITTER_DEBUG_DOMAIN D_DEBUG_DOMAIN

#define STM_BLITTER_ASSERT D_ASSERT
#define STM_BLITTER_ASSUME D_ASSUME
#define stm_blit_printw    D_WARN
#define stm_blit_printe    D_ERROR
#define stm_blit_printd    D_DEBUG_AT
#define stm_blit_printi    D_INFO


#define STM_BLITTER_N_ELEMENTS(array) D_ARRAY_SIZE(array)
#define STM_BLITTER_ABS(val)          ABS(val)
#define STM_BLITTER_MIN(a,b)          MIN(a,b)

#define __iomem
#define STM_BLITTER_RAW_WRITEL(val,addr)        *(addr) = (val)
#define STM_BLITTER_MEMSET_IO(dst,val,size)     memset(dst,val,size)
#define STM_BLITTER_MEMCPY_TO_IO(dst,src,size)  memcpy(dst,src,size)

#define flush_bdisp2_dma_area_region(a,o,s)
#define flush_bdisp2_dma_area(a)


#define bdisp2_div64_u64(d,n)   ((uint64_t) ((d) / (n)))


/* FIXME: 'synco' is only available starting w/ ST-300 series, i.e.
   not STb7109 and it is not understood by the STLinux-2.2 binutils! */
#ifdef __SH4__
#  define BDISP2_MB()  __asm__ __volatile__ ("synco" : : : "memory")
#  define BDISP2_WMB() __asm__ __volatile__ ("synco" : : : "memory")
#else
#  define BDISP2_MB()  __asm__ __volatile__ ("" : : : "memory")
#  define BDISP2_WMB() __asm__ __volatile__ ("" : : : "memory")
#endif

#define LOCK_ATOMIC_RPM(stdrv) \
  { \
  while (unlikely (!__sync_bool_compare_and_swap (&((stdrv)->bdisp_shared->lock.counter), 0, 1))) \
  { \
    if ( (stdrv)->bdisp_shared->bdisp_suspended == 2) \
      bdisp2_wakeup_os((stdrv)); \
    usleep(1); \
  } \
  if (unlikely ((stdrv)->bdisp_shared->locked_by)) \
    D_WARN ("locked an already locked lock by %x\n", (stdrv)->bdisp_shared->locked_by); \
  (stdrv)->bdisp_shared->locked_by = 2; \
  }

#define LOCK_ATOMIC(s) \
  { \
  while (unlikely (!__sync_bool_compare_and_swap (&((s)->lock.counter), 0, 1))) \
    usleep(1); \
  if (unlikely ((s)->locked_by)) \
    D_WARN ("locked an already locked lock by %x\n", (s)->locked_by); \
  (s)->locked_by = 2; \
  }

#define UNLOCK_ATOMIC(s) \
  { \
    if (unlikely ((s)->locked_by != 2))                            \
      D_WARN ("shared->locked_by error (%x)!\n", (s)->locked_by);  \
    (s)->locked_by = 0 ;                                           \
    BDISP2_MB(); /* update locked_by before releasing lock */ \
    (s)->lock.counter = 0;                                         \
  }
//  __sync_fetch_and_sub (&((s)->lock.counter), 1);
//  while (unlikely (!__sync_bool_compare_and_swap (&((s)->lock.counter), 1, 0))) {
//    printf("sleeping2...\n");
//  }


#define USEC_PER_SEC    1000000L
#define bdisp2_start_timer_os() \
     ({ \
       struct timespec currenttime;                              \
       clock_gettime (CLOCK_MONOTONIC, &currenttime);            \
       (((uint64_t) currenttime.tv_sec * USEC_PER_SEC) \
        + (currenttime.tv_nsec / 1000));                         \
     })


static inline uint32_t
bdisp2_get_reg (const struct stm_bdisp2_driver_data * const stdrv,
                uint16_t                             offset)
{
  return *(volatile uint32_t *) (stdrv->mmio_base + 0xa00 + offset);
}

static inline void
bdisp2_set_reg (struct stm_bdisp2_driver_data * const stdrv,
                uint16_t                       offset,
                uint32_t                       value)
{
  *(volatile uint32_t *) (stdrv->mmio_base + 0xa00 + offset) = value;
}

int
bdisp2_wakeup_os (struct stm_bdisp2_driver_data * const stdrv);

#endif /* __BDISP2_OS_DIRECTFBGLIBC_H__ */
