/***********************************************************************
 *
 * File: include/stmdisplayblitter.h
 * Copyright (c) 2006 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef STM_DISPLAY_BLITTER_H
#define STM_DISPLAY_BLITTER_H

#if defined(__cplusplus)
extern "C" {
#endif

typedef struct
{
  ULONG x;
  ULONG y;
} stm_point_t;


typedef struct
{
  ULONG left;
  ULONG top;
  ULONG right;
  ULONG bottom;
} stm_rect_t;


typedef struct
{
  union {
    ULONG  ulMemory;
    ULONG  ulOffset;
  };

  ULONG    ulSize;
  ULONG    ulWidth;
  ULONG    ulHeight;
  ULONG    ulStride;
  ULONG    ulChromaOffset;

  SURF_FMT format;
} stm_blitter_surface_t;


typedef struct
{
  ULONG                 ulFlags;
  stm_blitter_surface_t srcSurface;
  stm_blitter_surface_t dstSurface;
  ULONG                 ulColour;
  SURF_FMT              colourFormat;
  ULONG                 ulGlobalAlpha;
  ULONG                 ulColorKeyLow;
  ULONG                 ulColorKeyHigh;
  ULONG                 ulPlanemask;
  ULONG                 ulCLUT;
} stm_blitter_operation_t;


/* Note that this must always match the public declaration for enum
   stm_blitter_device in linux/drivers/video/stmfb.h
   FIXME: Having this twice just sucks. */
enum stm_blitter_device_priv {
  _STM_BLITTER_VERSION_7109c2,
  _STM_BLITTER_VERSION_1000,
  _STM_BLITTER_VERSION_ZEUS,
  _STM_BLITTER_VERSION_7109c3,
  _STM_BLITTER_VERSION_7200c1,
  _STM_BLITTER_VERSION_CHRONUS,
  _STM_BLITTER_VERSION_7200c2_7111_7141_7105,
  _STM_BLITTER_VERSION_5197,
  _STM_BLITTER_VERSION_7106_7108,
  _STM_BLITTER_VERSION_5206,
};

/* Note that this must always match the public declaration for struct
   STMFBBdispSharedArea in linux/drivers/video/stmfb.h
   FIXME: need a way to enforce this during compilation.*/
/* This structure is internally padded to be PAGE_SIZE bytes long, therefore
   it occupies the first PAGE_SIZE bytes of the shared area. We do this so as
   to be able to mmap() this first page cached and the remaining pages
   uncached. */
typedef struct { volatile int counter; } stmfb_atomic_priv_t;
struct _STMFBBDispSharedAreaPriv
{
  /* 0 */
  unsigned long nodes_phys;

  /* 4 */
  unsigned long last_free; /* offset of last free node */

  /* 8,12,14,16 */
  unsigned long  bdisp_running;
  unsigned short aq_index; /* which AQ is associated w/ this area, first is 0 */
  unsigned short updating_lna;
  unsigned long  prev_set_lna; /* previously set LNA */

  /* 20,24,28 */
  unsigned int  num_irqs; /* total number of IRQs (not only LNA + NODE) */
  unsigned int  num_lna_irqs;  /* total LNA IRQs */
  unsigned int  num_node_irqs; /* total node IRQs */

  /**** cacheline ****/

  /* 0x20 32 */
  unsigned int  num_idle;

  /* 36 */
  unsigned long next_free;

  /* 40,44 */
  unsigned int  num_wait_idle;
  unsigned int  num_wait_next;

  /* 48,52,56 */
  unsigned int  num_starts;
  unsigned int  num_ops_hi;
  unsigned int  num_ops_lo;

  /* 60 */
  unsigned long nodes_size;

  /**** cacheline ****/

  /* 0x40 64 */
  unsigned long last_lut;

  /* 68,76 */
  unsigned long long bdisp_ops_start_time;
  unsigned long long ticks_busy;

  /* 84,88,92 */
  stmfb_atomic_priv_t lock; /* a lock for concurrent access -> use with
                               atomic_cmpxchg() in kernel and
                               __sync_bool_compare_and_swap() in userspace */
  unsigned char       locked_by; /* 0 == none, 2 == userspace, 3 == kernel,
                                    1 == old version of this struct, it had
                                    the version information here */
  unsigned long       :24;  /* pad 89 */
  unsigned long       :32;  /* pad 92 */

  /**** cacheline ****/

  /* 0x60 96,100 */
  unsigned long                version;  /* version of this structure */
  enum stm_blitter_device_priv device;   /* BDisp implementation */
};
typedef volatile struct _STMFBBDispSharedAreaPriv STMFBBDispSharedAreaPriv;


/*
 * Note that this list must always match the
 * public linux defines for BLT_OP_FLAGS_NULL etc. in
 * linux/drivers/video/stmfb.h
 */
#define STM_BLITTER_FLAGS_NONE                    0x00000000
#define STM_BLITTER_FLAGS_SRC_COLOR_KEY           0x00000001
#define STM_BLITTER_FLAGS_DST_COLOR_KEY           0x00000002
#define STM_BLITTER_FLAGS_GLOBAL_ALPHA            0x00000004
#define STM_BLITTER_FLAGS_BLEND_SRC_ALPHA         0x00000008
#define STM_BLITTER_FLAGS_BLEND_SRC_ALPHA_PREMULT 0x00000010
#define STM_BLITTER_FLAGS_PLANE_MASK              0x00000020
#define STM_BLITTER_FLAGS_FLICKERFILTER           0x00000800
#define STM_BLITTER_FLAGS_CLUT_ENABLE             0x00001000
#define STM_BLITTER_FLAGS_BLEND_DST_COLOR         0x00002000
#define STM_BLITTER_FLAGS_BLEND_DST_MEMORY        0x00004000
#define STM_BLITTER_FLAGS_PREMULT_COLOUR_ALPHA    0x00008000
#define STM_BLITTER_FLAGS_COLORIZE                0x00010000
#define STM_BLITTER_FLAGS_XOR                     0x00020000
#define STM_BLITTER_FLAGS_BLEND_DST_ZERO          0x00040000
#define STM_BLITTER_FLAGS_RLE_DECODE              0x00080000
#define STM_BLITTER_FLAGS_SRC_INTERLACED          0x00100000
#define STM_BLITTER_FLAGS_DST_INTERLACED          0x00200000
#define STM_BLITTER_FLAGS_SRC_COLOURSPACE_709     0x00400000 /* content is in ITU-R BT.709 */
#define STM_BLITTER_FLAGS_DST_COLOURSPACE_709     0x00800000 /* colorspace, the default is */
                                                             /* ITU-R BT.601.              */
/* only supported if the destination is RGB (for now) */
#define STM_BLITTER_FLAGS_DST_FULLRANGE           0x01000000
/* src coordinates are specified in n.10 fixed point */
#define STM_BLITTER_FLAGS_SRC_XY_IN_FIXED_POINT   0x02000000
#define STM_BLITTER_FLAGS_3BUFFER_SOURCE          0x04000000 /* internal flag only */

typedef struct stm_display_blitter_s stm_display_blitter_t;

typedef struct
{
  /*
   * These are just placeholders at the moment
   */
  int (*Blit)          (stm_display_blitter_t *, const stm_blitter_operation_t *, const stm_rect_t *, const stm_rect_t *);
  int (*DrawRect)      (stm_display_blitter_t *, const stm_blitter_operation_t *, const stm_rect_t *);
  int (*FillRect)      (stm_display_blitter_t *, const stm_blitter_operation_t *, const stm_rect_t *);
  int (*Sync)          (stm_display_blitter_t *, int wait_next_only);

  unsigned long (*GetBlitLoad) (stm_display_blitter_t *);

  STMFBBDispSharedAreaPriv * (*GetSharedArea)(stm_display_blitter_t *);

  int (*HandleInterrupt)(stm_display_blitter_t *);

  void (*Release)(stm_display_blitter_t *);

} stm_display_blitter_ops_t;


struct stm_display_blitter_s
{
  ULONG handle;
  ULONG lock;

  const struct stm_display_device_s *owner;
  const stm_display_blitter_ops_t   *ops;
};

/*****************************************************************************
 * C interface to the blitter
 *
 */

/*
 * int stm_display_blitter_blit(stm_display_blitter_t   *b,
 *                              stm_blitter_operation_t *o,
 *                              stm_rect_t              *s,
 *                              stm_rect_t              *d)
 *
 * Queue an image blit operation on the blitter, the call does not wait for the
 * operation to complete.
 *
 * Returns -1 if the device lock cannot be obtained, otherwise it returns 0.
 *
 */
static inline int stm_display_blitter_blit(stm_display_blitter_t   *b,
                                           stm_blitter_operation_t *o,
                                           stm_rect_t              *s,
                                           stm_rect_t              *d)
{
  return b->ops->Blit(b, o, s, d);
}


/*
 * int stm_display_blitter_draw_rect(stm_display_blitter_t   *b,
 *                                   stm_blitter_operation_t *o,
 *                                   stm_rect_t              *s)
 *
 * Queue a rectangle draw operation on the blitter, the call does not wait for
 * the operation to complete.
 *
 * Returns -1 if the device lock cannot be obtained, otherwise it returns 0.
 *
 */
static inline int stm_display_blitter_draw_rect(stm_display_blitter_t   *b,
                                                stm_blitter_operation_t *o,
                                                stm_rect_t              *s)
{
  return b->ops->DrawRect(b, o, s);
}


/*
 * int stm_display_blitter_fill_rect(stm_display_blitter_t   *b,
 *                                   stm_blitter_operation_t *o,
 *                                   stm_rect_t              *s)
 *
 * Queue a rectangle fill operation on the blitter, the call does not wait for
 * the operation to complete.
 *
 * Returns -1 if the device lock cannot be obtained, otherwise it returns 0.
 *
 */
static inline int stm_display_blitter_fill_rect(stm_display_blitter_t   *b,
                                                stm_blitter_operation_t *o,
                                                stm_rect_t              *s)
{
  return b->ops->FillRect(b, o, s);
}


/*
 * int stm_display_blitter_sync(stm_display_blitter_t *b)
 *
 * Block until the blitter has completed all outstanding drawing operations.
 *
 * Returns -1 if the device lock cannot be obtained, otherwise it returns 0.
 *
 */
static inline int stm_display_blitter_sync(stm_display_blitter_t *b)
{
  return b->ops->Sync(b, 0);
}


/*
 * int stm_display_blitter_waitnext(stm_display_blitter_t *b)
 *
 * Block until the blitter has completed 'some' drawing operations.
 *
 * Returns -1 if the device lock cannot be obtained, otherwise it returns 0.
 *
 */
static inline int stm_display_blitter_waitnext(stm_display_blitter_t *b)
{
  return b->ops->Sync(b, 1);
}


/*
 * void stm_display_blitter_handle_interrupt(stm_display_blitter_t *b)
 *
 * This should be called by the primary interrupt handler for the blitter
 * interrupt.
 *
 */
static inline int stm_display_blitter_handle_interrupt(stm_display_blitter_t *b)
{
  return b->ops->HandleInterrupt(b);
}


/*
 * void stm_display_blitter_release(stm_display_blitter_t *b)
 *
 * Release the given blitter instance. The instance pointer is invalid after
 * this call completes.
 *
 */
static inline void stm_display_blitter_release(stm_display_blitter_t *b)
{
  b->ops->Release(b);
}


static inline STMFBBDispSharedAreaPriv * stm_display_blitter_get_shared(stm_display_blitter_t *b)
{
  return b->ops->GetSharedArea(b);
}


static inline unsigned long stm_display_blitter_get_bltload(stm_display_blitter_t *b)
{
  return b->ops->GetBlitLoad(b);
}


/*
 * int stm_create_subsurface(stm_blitter_surface *parent, stm_blitter_surface *newsurface)
 *
 * Helper function to create a subsurface descriptor that is
 * completely contained within the given parent surface.
 *
 * Returns -1 if the newsurface is incompatible with the parent,
 * and returns 0 on success.
 */
static inline int stm_create_subsurface(const stm_blitter_surface_t *parent,
                                        stm_blitter_surface_t *newsurface)
{
  if((newsurface->ulOffset > parent->ulSize) ||
     ((newsurface->ulOffset+(newsurface->ulHeight*newsurface->ulStride)) > parent->ulSize))
  {
    newsurface->ulMemory = 0;
    return -1;
  }

  newsurface->ulMemory = parent->ulMemory + newsurface->ulOffset;
  newsurface->ulSize   = newsurface->ulHeight * newsurface->ulStride;

  if(newsurface->format == 0)
    newsurface->format = parent->format;

  return 0;
}

#if defined(__cplusplus)
}
#endif

#endif /* STM_DISPLAY_BLITTER_H */
