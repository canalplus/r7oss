/*
   ST Microelectronics BDispII driver - hardware acceleration

   (c) Copyright 2007-2010  STMicroelectronics Ltd.

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


#include <linux/types.h>

#include "bdisp2/bdisp2_os.h"

#include "bdisp2/bdispII_driver_features.h"

#include <linux/stm/bdisp2_shared.h>
#include <linux/stm/bdisp2_shared_ioctl.h>
#include <linux/stm/bdisp2_nodegroups.h>
#include "bdisp2/bdispII_aq_state.h"
#include "bdisp2/bdispII_nodes.h"

#include "bdisp2/bdisp_accel.h"
#include "bdisp2/bdispII_aq_operations.h"
#define __IS_BDISP_ACCEL_C__
#include "bdisp2/bdisp_tables.h"
#undef __IS_BDISP_ACCEL_C__
#include <linux/stm/bdisp2_registers.h>

#ifdef __KERNEL__
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/ktime.h>
#include <linux/hrtimer.h>
#include <asm/cacheflush.h>
#undef BDISP2_USE_BSEARCH
#else /* __KERNEL__ */
#include <string.h>
#include <stdlib.h>
/* use bsearch() when trying to figure out the filter table's base address
   we need to use when scaling (instead of iterating over the list).
   Seems to be faster. */
#define BDISP2_USE_BSEARCH
#endif /* __KERNEL__ */

#ifndef likely
#  define likely(x)       __builtin_expect(!!(x),1)
#endif /* likely */
#ifndef unlikely
#  define unlikely(x)     __builtin_expect(!!(x),0)
#endif /* unlikely */

#ifndef offset_of
#  define offset_of(type, memb)     ((unsigned long)(&((type *)0)->memb))
#endif /* offset_of */
#ifndef container_of
#  define container_of(ptr, type, member) ({			\
              const typeof( ((type *)0)->member ) *__mptr = (ptr);	\
              (type *)( (char *)__mptr - offset_of(type,member) );})
#endif /* container_of */

STM_BLITTER_DEBUG_DOMAIN (BDISP_BLT,           "BDisp/blt",         "STM BDispII Drawing Engine");
STM_BLITTER_DEBUG_DOMAIN (BDISP_STRETCH,       "BDisp/strtch",      "STM BDispII stretch blits");
STM_BLITTER_DEBUG_DOMAIN (BDISP_STRETCH_SPANS, "BDisp/strtch/spns", "STM BDispII stretch blits (span generation)");
STM_BLITTER_DEBUG_DOMAIN (BDISP_STRETCH_FILT,  "BDisp/fltr",        "STM BDispII blit filters");
STM_BLITTER_DEBUG_DOMAIN (BDISP_NODES,         "BDisp/nodes",       "STM BDispII blit nodes");


#define FIXED_POINT_ONE       (1 << 16) /* 1 in 16.16 format */
#define TO_FIXED_POINT(x)     ((x) << 16) /* convert x to fixed point */
#define FIXED_POINT_TO_INT(x) ((x) >> 16) /* convert x to int */

#define INT_VALf(x)           (int64_t) ((x) >> 16), (((((int64_t) x) & 0xffff) * 1000000) / FIXED_POINT_ONE)

#define RECTANGLE_VALS(r)     (int64_t) (r)->position.x, (int64_t) (r)->position.y, (int64_t) (r)->size.w, (int64_t) (r)->size.h
#define RECTANGLE_VALSf(r)    INT_VALf((r)->position.x), INT_VALf((r)->position.y), INT_VALf((r)->size.w), INT_VALf((r)->size.h)


#ifdef DEBUG
#define DUMP_INFO() \
  ( {                                                               \
    const struct stm_bdisp2_shared_area __attribute__((unused)) * const shared = stdrv->bdisp_shared; \
    stm_blit_printd (BDISP_NODES,                                        \
                     "  -> %srunning CTL: %.8x IP/LNA/STA: %lu/%lu/%lu next/last %lu/%lu %.8lx/%.8lx\n", \
                     !bdisp2_is_idle(stdrv) ? "" : "not ",                \
                     bdisp2_get_AQ_reg (stdrv, stdev, BDISP_AQ_CTL),     \
                     (bdisp2_get_AQ_reg (stdrv, stdev, BDISP_AQ_IP) - shared->nodes_phys) / sizeof (struct _BltNode_Complete),  \
                     (bdisp2_get_AQ_reg (stdrv, stdev, BDISP_AQ_LNA) - shared->nodes_phys) / sizeof (struct _BltNode_Complete), \
                     (bdisp2_get_AQ_reg (stdrv, stdev, BDISP_AQ_STA) - shared->nodes_phys) / sizeof (struct _BltNode_Complete), \
                     shared->next_free, shared->last_free,               \
                     shared->next_free, shared->last_free);              \
  } )

static void
_dump_node (const struct _BltNodeGroup00 * const config_general)
{
  char *str = bdisp2_sprint_node(config_general, false);
  stm_blit_printd(BDISP_NODES, "\n%s", str);
  stm_blitter_free_mem(str);
}
#else
#define DUMP_INFO() do {} while (0)
static inline void
_dump_node (const struct _BltNodeGroup00 * const config_general) { }
#endif



struct _BltNode_Fast
{
  struct _BltNode_Common common; // 14
#ifdef BDISP2_IMPLEMENT_WAITSERIAL
  struct _BltNodeGroup14 ConfigStatic; // 2
#endif /* BDISP2_IMPLEMENT_WAITSERIAL */
  // total: 14(+2) * 4 = 56(+8)
#define BLIT_CIC_BLT_NODE_FAST   (0 \
                                  | BLIT_CIC_BLT_NODE_COMMON \
                                  | BLIT_CIC_NODE_STATIC     \
                                 )
};


struct _BltNode_YCbCr422r_shortcut
{
  struct _BltNodeGroup00 ConfigGeneral;  // 4
  struct _BltNodeGroup01 ConfigTarget;   // 4
  struct _BltNodeGroup02 ConfigColor;    // 2
  struct _BltNodeGroup04 ConfigSource2;  // 4
#ifdef BDISP2_SUPPORT_HW_CLIPPING
  struct _BltNodeGroup06 ConfigClip;     // 2
#endif /* BDISP2_SUPPORT_HW_CLIPPING */
#ifdef BDISP2_IMPLEMENT_WAITSERIAL
  struct _BltNodeGroup14 ConfigStatic;   // 2
#endif /* BDISP2_IMPLEMENT_WAITSERIAL */
  struct _BltNodeGroup15 ConfigIVMX;     // 4
  // total: 18(+2+2) * 4 = 72(+8+8)
#define BLIT_CIC_BLT_NODE_YCbCr422r_SHORTCUT (0 \
                                              | BLIT_CIC_NODE_COLOR   \
                                              | BLIT_CIC_NODE_SOURCE2 \
                                              | BLIT_CIC_NODE_CLIP    \
                                              | BLIT_CIC_NODE_STATIC  \
                                              | BLIT_CIC_NODE_IVMX    \
                                             )
};


struct _BltNode_RGB32fixup
{
  struct _BltNodeGroup00 ConfigGeneral; // 4 -> General
  struct _BltNodeGroup01 ConfigTarget; // 4 -> Target
  struct _BltNodeGroup03 ConfigSource1; // 4 -> Source1
  struct _BltNodeGroup08 ConfigFilters; // 2
#ifdef BDISP2_IMPLEMENT_WAITSERIAL
  struct _BltNodeGroup14 ConfigStatic; // 2
#endif /* BDISP2_IMPLEMENT_WAITSERIAL */
  // total: 11(+2) * 4 = 44(+8)
#define BLIT_CIC_BLT_NODE_RGB32fixup (0 \
                                      | BLIT_CIC_NODE_SOURCE1 \
                                      | BLIT_CIC_NODE_FILTERS \
                                      | BLIT_CIC_NODE_STATIC  \
                                     )
};


struct _BltNode_FullFill
{
  struct _BltNode_Common common; // 14

  struct _BltNodeGroup04 ConfigSource2;  // 4
  /* the following groups are not necessarily used, thus don't rely on
     a 1:1 mapping of this struct in memory... */
#ifdef BDISP2_SUPPORT_HW_CLIPPING
  struct _BltNodeGroup06 ConfigClip;     // 2
#endif /* BDISP2_SUPPORT_HW_CLIPPING */
  struct _BltNodeGroup07 ConfigClut;     // 2
  struct _BltNodeGroup08 ConfigFilters;  // 2
  struct _BltNodeGroup12 ConfigColorkey; // 2
#ifdef BDISP2_IMPLEMENT_WAITSERIAL
  struct _BltNodeGroup14 ConfigStatic;   // 2
#endif /* BDISP2_IMPLEMENT_WAITSERIAL */
  struct _BltNodeGroup15 ConfigIVMX;     // 4
  // max. total: 28(+2+2) * 4 = 112(+8+8)
};




#define set_matrix(dst, src) \
  ({ \
    dst ## 0 = src[0]; \
    dst ## 1 = src[1]; \
    dst ## 2 = src[2]; \
    dst ## 3 = src[3]; \
  })

#define SET_ARGB_PIXEL(a,r,g,b) (((a) << 24) | \
                                 ((r) << 16) | \
                                 ((g) << 8)  | \
                                  (b))


void
bdisp2_cleanup (struct stm_bdisp2_driver_data * const stdrv,
                struct stm_bdisp2_device_data * const stdev)
{
  if (stdrv->tables.size)
    bdisp2_free_memory_os (stdrv, stdev, &stdrv->tables);
}

int
bdisp2_initialize (struct stm_bdisp2_driver_data * const stdrv,
                   struct stm_bdisp2_device_data * const stdev)
{
  struct _BltNode_Complete __iomem *node;
#ifdef __KERNEL__
  unsigned long                     nip;
#endif /* __KERNEL__*/
  struct stm_bdisp2_shared_area    *shared;
  int                               result;

  stm_blit_printd (BDISP_BLT, "%s: stdrv/stdev: %p/%p (id %u)\n",
                   __func__, stdrv, stdev, stdev->queue_id);

  STM_BLITTER_ASSERT (stdrv->bdisp_shared != NULL);
  STM_BLITTER_ASSERT (stdrv->bdisp_nodes.memory != NULL);
  STM_BLITTER_ASSERT (stdrv->bdisp_nodes.base != 0);

  shared = stdrv->bdisp_shared;

  /* figure out available node size in bytes and align to a multiple of the
     maximum size of a node */
  stdev->usable_nodes_size = ((shared->nodes_size / sizeof (*node))
                              * sizeof (*node));
  stdev->node_irq_delay = (stdev->usable_nodes_size / sizeof (*node)) / 4;

  /* final init of BDisp registers */
  stdev->queue_id = shared->aq_index;

  /* figure out what specific implementation this is, and which features it
     supports */
  if (bdisp2_init_features_by_type (&stdev->features, &shared->plat_blit))
    {
      stm_blit_printe ("BDisp/BLT: blitter is %d (unknown)\n", shared->plat_blit.device_type);
      return -EINVAL;
    }

  stm_blit_printd (BDISP_BLT,
                   "BDisp/BLT: nodes area phys/size:  0x%.8lx/%lu  (%zu usable -> %zu nodes) -> ends @ 0x%.8lx\n",
                   shared->nodes_phys, shared->nodes_size,
                   stdev->usable_nodes_size,
                   stdev->usable_nodes_size / sizeof (*node),
                   shared->nodes_phys + stdev->usable_nodes_size);

#ifdef __KERNEL__
  /* initialize shared area */
  STM_BLITTER_ASSUME (bdisp2_is_idle(stdrv));
  /* important stuff */
  shared->bdisp_running
    = shared->bdisp_suspended
    = shared->updating_lna = 0;

#if defined(CONFIG_PM_RUNTIME)
  /* Make sure first suspend reset counter to zero */
  shared->num_pending_requests = 1;
#endif

  shared->prev_set_lna = (stdev->usable_nodes_size
                          - sizeof (struct _BltNode_Complete)
                          + shared->nodes_phys);
  shared->last_lut = 0;
  /* stats only */
  shared->num_idle = shared->num_irqs = shared->num_lna_irqs
    = shared->num_node_irqs = shared->num_ops_hi = shared->num_ops_lo
    = shared->num_starts
    = shared->num_wait_idle = shared->num_wait_next
    = 0;


  /* the DMA memory (nodelist, filters) */
  /* set nodelist to some sane defaults. We use the __raw*() versions
     here because we don't care about barries here. And there is a final
     barrier at the end. */
  STM_BLITTER_MEMSET_IO ((void __iomem *) stdrv->bdisp_nodes.memory,
                         0, stdev->usable_nodes_size);
  /* prelink all the buffer nodes around to make a circular list */
  node = (void __iomem *) stdrv->bdisp_nodes.memory;
  nip = shared->nodes_phys + sizeof (*node);
  shared->num_ops_lo = 0xfff00000;
  while (((void __iomem *) node) < (stdrv->bdisp_nodes.memory
                                    + stdev->usable_nodes_size
                                    - sizeof (*node)))
    {
      STM_BLITTER_RAW_WRITEL (nip,
                              &node->common.ConfigGeneral.BLT_NIP);
      STM_BLITTER_RAW_WRITEL (BLIT_CIC_BLT_NODE_COMMON,
                              &node->common.ConfigGeneral.BLT_CIC);
      STM_BLITTER_RAW_WRITEL (BLIT_INS_SRC1_MODE_DISABLED,
                              &node->common.ConfigGeneral.BLT_INS);

      STM_BLITTER_RAW_WRITEL (BLIT_CIC_BLT_NODE_INIT,
                              &node->common.ConfigGeneral.BLT_CIC);
      STM_BLITTER_RAW_WRITEL (shared->num_ops_lo,
                              &((struct _BltNode_Init *) node)->ConfigStatic.BLT_USR);

      ++shared->num_ops_lo;
      ++node;
      nip += sizeof (*node);
    }
  /* last node points back to the first */
  STM_BLITTER_RAW_WRITEL (shared->nodes_phys,
                          &node->common.ConfigGeneral.BLT_NIP);
  STM_BLITTER_RAW_WRITEL (BLIT_CIC_BLT_NODE_COMMON,
                          &node->common.ConfigGeneral.BLT_CIC);
  STM_BLITTER_RAW_WRITEL (BLIT_INS_SRC1_MODE_DISABLED,
                          &node->common.ConfigGeneral.BLT_INS);

  STM_BLITTER_RAW_WRITEL (BLIT_CIC_BLT_NODE_INIT,
                          &node->common.ConfigGeneral.BLT_CIC);
  STM_BLITTER_RAW_WRITEL (shared->num_ops_lo,
                          &((struct _BltNode_Init *) node)->ConfigStatic.BLT_USR);
  shared->num_ops_lo = 0;

  flush_bdisp2_dma_area_region(&stdrv->bdisp_nodes,
                               0, stdev->usable_nodes_size);

  /* final init of shared area */
  shared->next_free = 0;
  shared->next_ip   = 0;
  shared->last_free = stdev->usable_nodes_size - sizeof (*node);
#else /* __KERNEL__ */
  /* check that our notion of the size of a node is the same as
     the kernel's */
  node = (void __iomem *) stdrv->bdisp_nodes.memory;
  /* would like to use STM_BLITTER_ASSERT(), but that only works in
     debug mode in DirectFB (D_ASSERT()) */
  if ((node->common.ConfigGeneral.BLT_NIP - shared->nodes_phys)
      != sizeof (*node))
    {
      stm_blit_printe("BDisp/BLT: mismatch between kernel and userspace drivers (%lu vs. %zu)\n",
                      node->common.ConfigGeneral.BLT_NIP - shared->nodes_phys,
                      sizeof (*node));
      return -EILSEQ;
    }
#endif /* !__KERNEL__ */

  /* we need some memory for some CLUTs and H/V filter coefficients, too.
     The CLUTs need to be aligned to a 16byte address and the filters to an
     8byte address. We have two types of CLUT here - a) a usual CLUT,
     and b) CLUTs used for various types of color correction, e.g. for
     supporting the RGB32 pixel format to force the alpha to be 0xff. */
#define CLUT_SIZE (256 * sizeof (uint32_t))
  {
  unsigned int   total_reserve;
  unsigned int   siz_clut;
  unsigned int   siz_8x8_filter;
  unsigned int   siz_5x8_filter;
  unsigned int   i;

  unsigned long  base;
  void __iomem  *memory;

  enum bdisp2_palette_type palette;
  unsigned long __iomem *clut_virt;
  unsigned char __iomem *filter_virt;

  siz_clut       = CLUT_SIZE * SG2C_COUNT;
  siz_8x8_filter = (STM_BLITTER_N_ELEMENTS (bdisp_aq_blitter_8x8_filters)
                    * BDISP_AQ_BLIT_FILTER_8X8_TABLE_SIZE);
  siz_5x8_filter = (STM_BLITTER_N_ELEMENTS (bdisp_aq_blitter_5x8_filters)
                    * BDISP_AQ_BLIT_FILTER_5X8_TABLE_SIZE);

  total_reserve = siz_clut + siz_8x8_filter + siz_5x8_filter + 5;

  /* CLUT needs to be 16byte aligned */
  stdrv->tables.size = total_reserve;
  result = bdisp2_alloc_memory_os (stdrv, stdev, 16, false, &stdrv->tables);
  if (result)
    {
      stm_blit_printe ("BDisp/BLT: couldn't reserve %u bytes for CLUTs & H/V filter memory!\n",
                       total_reserve);
      stdrv->tables.size = 0;
      return result;
    }

  stm_blit_printd (BDISP_BLT,
                   "reserved %u bytes for CLUT & filters @ 0x%.8llx\n",
                   total_reserve, (uint64_t) stdrv->tables.base);

  base = stdrv->tables.base;
  memory = (void __iomem *) stdrv->tables.memory;

  /* ...and prepare the color correction LUTs. The alpha range for LUTs is
     0 ... 128 (not 127); this includes both, the lookup and the final value!

  1) RGB32 -> Aout = 1, RGBout = RGBin */
  stdev->clut_phys[SG2C_ONEALPHA_RGB] = base;
  clut_virt = memory;
  for (i = 0; i <= 0x80; ++i)
    STM_BLITTER_RAW_WRITEL (SET_ARGB_PIXEL (0x80, i, i, i),
                            &clut_virt[i]);
  for (; i < 256; ++i)
    STM_BLITTER_RAW_WRITEL (SET_ARGB_PIXEL (0, i, i, i),
                            &clut_virt[i]);
  base += CLUT_SIZE;
  memory += CLUT_SIZE;

  /* 2) Aout = 1 - Ain, RGBout = 0 */
  stdev->clut_phys[SG2C_INVALPHA_ZERORGB] = base;
  clut_virt = memory;
  for (i = 0; i <= 0x80; ++i)
    STM_BLITTER_RAW_WRITEL (SET_ARGB_PIXEL (0x80 - i, 0, 0, 0),
                            &clut_virt[i]);
  STM_BLITTER_MEMSET_IO (&clut_virt[i], 0x00,
                         CLUT_SIZE - (i * sizeof (uint32_t)));
  base += CLUT_SIZE;
  memory += CLUT_SIZE;

  /* 3) Aout = Ain, RGBout = 0 */
  stdev->clut_phys[SG2C_ALPHA_ZERORGB] = base;
  clut_virt = memory;
  for (i = 0; i <= 0x80; ++i)
    STM_BLITTER_RAW_WRITEL (SET_ARGB_PIXEL (i, 0, 0, 0),
                            &clut_virt[i]);
  STM_BLITTER_MEMSET_IO (&clut_virt[i], 0x00,
                         CLUT_SIZE - (i * sizeof (uint32_t)));
  base += CLUT_SIZE;
  memory += CLUT_SIZE;

  /* 4) Aout = 0, RGBout = RGBin */
  stdev->clut_phys[SG2C_ZEROALPHA_RGB] = base;
  clut_virt = memory;
  for (i = 0; i < 256; ++i)
    STM_BLITTER_RAW_WRITEL (SET_ARGB_PIXEL (0, i, i, i),
                            &clut_virt[i]);
  base += CLUT_SIZE;
  memory += CLUT_SIZE;

  /* 5) Aout = 0, RGBout = 0 */
  stdev->clut_phys[SG2C_ZEROALPHA_ZERORGB] = base;
  clut_virt = memory;
  STM_BLITTER_MEMSET_IO (clut_virt, 0, CLUT_SIZE);
  base += CLUT_SIZE;
  memory += CLUT_SIZE;

  /* 6) Aout = 1, RGBout = 0 */
  stdev->clut_phys[SG2C_ONEALPHA_ZERORGB] = base;
  clut_virt = memory;
  for (i = 0; i <= 0x80; ++i)
    STM_BLITTER_RAW_WRITEL (SET_ARGB_PIXEL (0x80, 0, 0, 0),
                            &clut_virt[i]);
  STM_BLITTER_MEMSET_IO (&clut_virt[i], 0x00,
                         CLUT_SIZE - (i * sizeof (uint32_t)));
  base += CLUT_SIZE;
  memory += CLUT_SIZE;


  /* dynamic and 'normal' palettes */
  for (palette = SG2C_NORMAL; palette < SG2C_DYNAMIC_COUNT; ++palette)
    {
      stdev->clut_phys[palette] = base;
      stdrv->clut_virt[palette] = memory;
      base += CLUT_SIZE;
      memory += CLUT_SIZE;
    }

  for (palette = SG2C_NORMAL; palette < SG2C_DYNAMIC_COUNT; ++palette)
    stm_blit_printd (BDISP_BLT, "CLUT %d @ %llx mmap()ed to %p\n",
                     palette, (unsigned long long) stdev->clut_phys[palette],
                     stdrv->clut_virt[palette]);


  /* the filter coeffs need to be copied into the filter area and later
     don't ever need to be touched again. Except that we need to
     remember the physical address to tell the BDisp later. */
  stdev->filter_8x8_phys = base;
  filter_virt = memory;
  for (i = 0; i < STM_BLITTER_N_ELEMENTS (bdisp_aq_blitter_8x8_filters); ++i)
    STM_BLITTER_MEMCPY_TO_IO (&filter_virt[i * BDISP_AQ_BLIT_FILTER_8X8_TABLE_SIZE],
                              bdisp_aq_blitter_8x8_filters[i].filter_coeffs,
                              BDISP_AQ_BLIT_FILTER_8X8_TABLE_SIZE);
  base += siz_8x8_filter;
  memory += siz_8x8_filter;

  /* same for vertical filter coeffs. */
  stdev->filter_5x8_phys = base;
  filter_virt = memory;
  for (i = 0; i < STM_BLITTER_N_ELEMENTS (bdisp_aq_blitter_5x8_filters); ++i)
    STM_BLITTER_MEMCPY_TO_IO (&filter_virt[i * BDISP_AQ_BLIT_FILTER_5X8_TABLE_SIZE],
                              bdisp_aq_blitter_5x8_filters[i].filter_coeffs,
                              BDISP_AQ_BLIT_FILTER_5X8_TABLE_SIZE);
  }

  flush_bdisp2_dma_area(&stdrv->tables);

  /* stick in a memory barrier particularly for when we are doing
     uncached write combined. */
  BDISP2_WMB ();


  stdev->setup = &stdev->_setup;

  /* the flicker filter coefficients don't ever change and are directly
     mapped to registers, no physical memory is needed. */
  stdev->setup->ConfigFlicker = bdisp_aq_config_flicker_filter;

  /* we don't use the static addresses registers, so we need to initialize
     this one to 0, because we _do_ use the BLT_USR from the same register
     group. */
  stdev->setup->ConfigStatic.BLT_SAR = 0;

  stdev->setup->ConfigColor.BLT_S1CF = 0;
  stdev->setup->ConfigColor.BLT_S2CF = 0;

  stdev->setup->ConfigFilters.BLT_PMK = 0x00ffffff;

  set_matrix (stdev->setup->ConfigOVMX.BLT_OVMX, bdisp_aq_RGB_2_VideoYCbCr601);


  /* set up the hardware -> point registers to right place */
  bdisp2_set_AQ_reg (stdrv, stdev,
                     BDISP_AQ_CTL,
                     (3 - shared->aq_index) | (0
                                               | BDISP_AQ_CTL_QUEUE_EN
                                               | BDISP_AQ_CTL_EVENT_SUSPEND
                                               | BDISP_AQ_CTL_IRQ_NODE_COMPLETED
                                               | BDISP_AQ_CTL_IRQ_LNA_REACHED
                                              )
                    );

  if (stdev->features.hw.need_itm)
    bdisp2_set_reg (stdrv, BDISP_ITM0,
                    (BDISP_ITS_AQ_MASK << (BDISP_ITS_AQ1_SHIFT
                                           + (shared->aq_index * 4))));

  /* no blit will start until the LNA register is written to */
  stm_blit_printd (BDISP_BLT, "blit node base address: %#08lx\n",
                   stdrv->bdisp_nodes.base);
  bdisp2_set_AQ_reg (stdrv, stdev, BDISP_AQ_IP, stdrv->bdisp_nodes.base);

  return 0;
}

static int
bdisp2_is_suspended(struct stm_bdisp2_driver_data * const stdrv)
{
  int res;
  struct stm_bdisp2_shared_area * const shared = stdrv->bdisp_shared;

  LOCK_ATOMIC_RPM (stdrv);

  /* bdisp_suspended was only added in stm-blitter v1.10.0 - let's take that
     into account as otherwise it has no meaning! */
  res = (shared->version >= 3) && shared->bdisp_suspended;

  UNLOCK_ATOMIC (shared);

  return res;
}

/*********************************************/
/* Reset the device                          */
/*********************************************/
void
bdisp2_engine_reset (struct stm_bdisp2_driver_data * const stdrv,
                     struct stm_bdisp2_device_data * const stdev)
{
  stm_blit_printd (BDISP_BLT, "%s: stdrv/stdev: %p/%p (id %u)\n",
                   __func__, stdrv, stdev, stdev->queue_id);

#ifdef __KERNEL__
  bdisp2_set_reg (stdrv, BDISP_CTL, BDISP_CTL_GLOBAL_SOFT_RESET);
  STM_BLITTER_UDELAY (10);
  bdisp2_set_reg (stdrv, BDISP_CTL, 0);
  while (!(bdisp2_get_reg (stdrv, BDISP_STA) & 0x1))
    STM_BLITTER_UDELAY (1);
#else /* __KERNEL__ */
  /* don't do this! it resets too much! */
#endif /* __KERNEL__ */
}

int
bdisp2_engine_sync (struct stm_bdisp2_driver_data       * const stdrv,
                    const struct stm_bdisp2_device_data * const stdev)
{
  int ret;

  stm_blit_printd (BDISP_BLT, "%s()\n", __func__);

  DUMP_INFO ();

  /* can't lock here, because the os dependent code might try a different lock
     in addition which might have been taken by somebody else. */
//  LOCK_ATOMIC_DEBUG_ONLY (shared);

  ret = bdisp2_engine_sync_os (stdrv, stdev);

//  UNLOCK_ATOMIC_DEBUG_ONLY (shared);

  return ret;
}


void
bdisp2_wait_event (struct stm_bdisp2_driver_data       * const stdrv,
                   const struct stm_bdisp2_device_data * const stdev,
                   const stm_blitter_serial_t          * const serial)
{
  stm_blit_printd (BDISP_BLT, "%s()\n", __func__);

  DUMP_INFO ();

  if (bdisp2_is_idle(stdrv))
    return;

  if (!serial)
    bdisp2_wait_space_os (stdrv, stdev);
  else
    {
      struct stm_bdisp2_shared_area * const shared = stdrv->bdisp_shared;
      unsigned long lo = *serial & 0xffffffff;

      /* FIXME: make this work w/ num_ops_hi != 0, too */
      if (!shared->num_ops_hi || lo >= shared->num_ops_lo)
        bdisp2_wait_fence_os (stdrv, stdev, serial);
      else
        bdisp2_engine_sync (stdrv, stdev);
    }
}


void
bdisp2_emit_commands (struct stm_bdisp2_driver_data       * const stdrv,
                      const struct stm_bdisp2_device_data * const stdev,
                      bool                                 is_locked)
{
  struct stm_bdisp2_shared_area * const shared = stdrv->bdisp_shared;
  unsigned long                  new_last_valid;

  if (!is_locked)
    LOCK_ATOMIC_RPM (stdrv);

  new_last_valid = ((shared->next_free ? shared->next_free
                                       : stdev->usable_nodes_size)
                    - sizeof (struct _BltNode_Complete)
                    + shared->nodes_phys);

  stm_blit_printd (BDISP_BLT, "%s() old/new last valid: %.8lx/%.8lx\n",
                   __func__, shared->prev_set_lna, new_last_valid);

  /* have any been added since last time we were called? This test is
     necessary because an operation like FillRect() might fail, without
     actually queueing _any_ node (or FillRect_nop() which intentionally
     doesn't queue anything). Ignoring this case, we would fire the hardware
     for doing nothing. */
  if (new_last_valid != shared->prev_set_lna)
    {
      ++shared->num_starts;

      shared->prev_set_lna = new_last_valid;

      if (bdisp2_is_idle(stdrv))
        shared->bdisp_ops_start_time = bdisp2_start_timer_os ();

#ifdef __SH4__
      /* LMI write posting: make sure outstanding writes to LMI made it into
         RAM, mb() doesn't help, because of the ST40 bandwidth limiter */
      {
      uint32_t offs = shared->prev_set_lna - shared->nodes_phys;
      volatile struct _BltNodeGroup00 * const ConfigGeneral
        = (struct _BltNodeGroup00 *) (stdrv->bdisp_nodes.memory + offs);
      uint32_t nip = ConfigGeneral->BLT_NIP;
      STM_BLITTER_ASSERT (shared->next_free == nip - shared->nodes_phys);
      shared->next_free = nip - shared->nodes_phys;
      }
#endif

#ifdef DEBUG
      /* Add memory barrier to make sure data are well synchronized in case
         of debug so debug message are displaying valid values */
      BDISP2_MB();
#endif
      /* kick */
      bdisp2_set_AQ_reg (stdrv, stdev, BDISP_AQ_LNA, new_last_valid);
    }

  /* unconditionally unlock */
  UNLOCK_ATOMIC (shared);
}



void
bdisp2_get_serial (const struct stm_bdisp2_driver_data * const stdrv,
                   const struct stm_bdisp2_device_data * const stdev,
                   stm_blitter_serial_t                * const serial)
{
  /* FIXME: need lock */
  *serial = ((stm_blitter_serial_t) stdrv->bdisp_shared->num_ops_hi) << 32;
  *serial |= stdrv->bdisp_shared->num_ops_lo;
}


void
bdisp2_wait_serial (struct stm_bdisp2_driver_data * const stdrv,
                    struct stm_bdisp2_device_data * const stdev,
                    stm_blitter_serial_t           serial)
{
  struct stm_bdisp2_shared_area * const shared = stdrv->bdisp_shared;
  unsigned long lo = serial & 0xffffffff;

  stm_blit_printd (BDISP_BLT, "%s (%llu)\n", __func__, serial);

  if(bdisp2_is_suspended(stdrv))
    return;

  DUMP_INFO ();

  /* FIXME: make this work w/ num_ops_hi != 0, too */
  if (!shared->num_ops_hi
      || lo >= shared->num_ops_lo)
    {
      uint32_t usr = bdisp2_get_reg (stdrv, 0x200 + 0xb4 /* BLT_USR */ );

      while ((usr < (lo + 1)) && (!bdisp2_is_idle(stdrv)))
        usr = bdisp2_get_reg (stdrv, 0x200 + 0xb4 /* BLT_USR */ );
    }
  else
    bdisp2_engine_sync (stdrv, stdev);
}





#ifdef BDISP2_USE_BSEARCH
static int
_filter_table_lookup (const void * const key,
                      const void * const member)
{
  const struct _bdisp_aq_blitfilter_coeffs_range * const range = member;
  int                                             scale = (int) key;

  if (scale <= range->scale_min)
    return -1;

  if (scale > range->scale_max)
    return 1;

  return 0;
}

static int
CSTmBlitter__ChooseBlitter5x8FilterCoeffs (int scale)
{
  const struct _bdisp_aq_blitfilter_coeffs_5x8 * const res =
    bsearch ((void *) scale,
             bdisp_aq_blitter_5x8_filters,
             STM_BLITTER_N_ELEMENTS (bdisp_aq_blitter_5x8_filters),
             sizeof (*bdisp_aq_blitter_5x8_filters),
             _filter_table_lookup);

  if (!res)
    return STM_BLITTER_N_ELEMENTS (bdisp_aq_blitter_5x8_filters) - 1;

  return res - bdisp_aq_blitter_5x8_filters;
}

static int
CSTmBlitter__ChooseBlitter8x8FilterCoeffs (int scale)
{
  const struct _bdisp_aq_blitfilter_coeffs_8x8 * const res =
    bsearch ((void *) scale,
             bdisp_aq_blitter_8x8_filters,
             STM_BLITTER_N_ELEMENTS (bdisp_aq_blitter_8x8_filters),
             sizeof (*bdisp_aq_blitter_8x8_filters),
             _filter_table_lookup);

  if (!res)
    return STM_BLITTER_N_ELEMENTS (bdisp_aq_blitter_8x8_filters) - 1;

  return res - bdisp_aq_blitter_8x8_filters;
}
#else /* BDISP2_USE_BSEARCH */
static int
CSTmBlitter__ChooseBlitter5x8FilterCoeffs (int scale)
{
  unsigned int i;

  if (scale <= 0)
    return -1;

  for (i = 0; i < STM_BLITTER_N_ELEMENTS (bdisp_aq_blitter_5x8_filters); ++i)
    if ((scale <= bdisp_aq_blitter_5x8_filters[i].range.scale_max)
        && (scale > bdisp_aq_blitter_5x8_filters[i].range.scale_min))
      return i;

  return STM_BLITTER_N_ELEMENTS (bdisp_aq_blitter_5x8_filters) - 1;
}

static int
CSTmBlitter__ChooseBlitter8x8FilterCoeffs (int scale)
{
  unsigned int i;

  if (scale <= 0)
    return -1;

  for (i = 0; i < STM_BLITTER_N_ELEMENTS (bdisp_aq_blitter_8x8_filters); ++i)
    if ((scale <= bdisp_aq_blitter_8x8_filters[i].range.scale_max)
        && (scale > bdisp_aq_blitter_8x8_filters[i].range.scale_min))
      return i;

  return STM_BLITTER_N_ELEMENTS (bdisp_aq_blitter_8x8_filters) - 1;
}
#endif /* BDISP2_USE_BSEARCH */


static void
bdisp2_set_clut_operation (const struct stm_bdisp2_driver_data * const stdrv,
                           struct stm_bdisp2_device_data       * const stdev,
                           bool                                 do_expand,
                           uint32_t                            * const blt_cic,
                           uint32_t                            * const blt_ins,
                           struct _BltNodeGroup07              * const config_clut)
{
  *blt_cic |= BLIT_CIC_NODE_CLUT;
  *blt_ins |= BLIT_INS_ENABLE_CLUT;

#ifdef BDISP2_CLUT_UNSAFE_MULTISESSION
  stdrv->bdisp_shared->last_lut = 1;
#endif /* BDISP2_CLUT_UNSAFE_MULTISESSION */

  config_clut->BLT_CCO &= ~BLIT_CCO_CLUT_MODE_MASK;
  config_clut->BLT_CCO |= (do_expand ? BLIT_CCO_CLUT_EXPAND
                                     : BLIT_CCO_CLUT_CORRECT);

  config_clut->BLT_CCO |= BLIT_CCO_CLUT_UPDATE_EN;

  if (stdev->setup->palette_type < SG2C_COUNT)
    config_clut->BLT_CML = stdev->clut_phys[stdev->setup->palette_type];
}



/* get a pointer to the next unused node in the nodelist. Will _not_ mark
   the returned node as being used. I.e. you must call
   bdisp2_finish_node() on this node before requesting another node. */
static void __iomem *
bdisp2_get_new_node (struct stm_bdisp2_driver_data       * const stdrv,
                     const struct stm_bdisp2_device_data * const stdev)
{
  LOCK_ATOMIC_RPM (stdrv);

  /* if this is the last free entry in the list, kick the bdisp and wait a
     bit. Need to be careful to not create deadlocks. */
  if (unlikely (stdrv->bdisp_shared->next_free
                == stdrv->bdisp_shared->last_free))
    {
      bdisp2_wait_space_start_timer ();

restart:
      bdisp2_emit_commands (stdrv, stdev, true);
      /* bdisp2_emit_commands() unlocks the lock for us on exit */

      bdisp2_wait_event (stdrv, stdev, NULL);
      LOCK_ATOMIC_RPM (stdrv);

      if (stdrv->bdisp_shared->next_free == stdrv->bdisp_shared->last_free)
        goto restart;

      bdisp2_wait_space_end_timer ();
    }

  return (void __iomem *) (stdrv->bdisp_nodes.memory
                           + stdrv->bdisp_shared->next_free);
}

/* tell the node list handling, that we are done with this node, i.e. it is
   valid and the next node should be used for new commands. Doesn't enqueue
   the node to the hardware, just updates an 'internal' pointer. */
static void
bdisp2_finish_node (struct stm_bdisp2_driver_data       * const stdrv,
                    const struct stm_bdisp2_device_data * const stdev,
                    const void                          * const node)
{
  STM_BLITTER_ASSERT (node == (stdrv->bdisp_nodes.memory
                               + stdrv->bdisp_shared->next_free));

  _dump_node (node);

  /* we need to update shared->next_free:
     - either we could read back node->BLT_NIP and set
       shared->next_free = nip - shared->nodes_phys
       i.e.
       volatile struct _BltNodeGroup00 * const ConfigGeneral = (struct _BltNodeGroup00 *) node;
       uint32_t nip = ConfigGeneral->BLT_NIP;
       shared->next_free = nip - shared->nodes_phys;

     - or calculate shared->next_free, because we know the size of one node
       (see bdisp_aq_initialize())

     The former has the advantage of being a nice memory barrier in case the
     nodelist area is mmap()ed cached (but currently we don't do so), while
     the latter has the advantage of not having to read back from (at the
     moment) uncached memory. So we calculate... In addition, on the ST40
     due to write posting, we need a read from LMI in any case to make sure
     any previous writes have been completed, but that read is done in another
     place, namely in bdisp2_emit_commands(). */

  /* advance to next */
  stdrv->bdisp_shared->next_free += sizeof (struct _BltNode_Complete);
  if (unlikely (stdrv->bdisp_shared->next_free == stdev->usable_nodes_size))
    stdrv->bdisp_shared->next_free = 0;

  DUMP_INFO ();

  UNLOCK_ATOMIC (stdrv->bdisp_shared);
}


static void
bdisp2_update_num_ops (struct stm_bdisp2_driver_data       * const stdrv,
                       const struct stm_bdisp2_device_data * const stdev,
                       uint32_t                            * const blt_ins)
{
  /* we have a counter for the number of total operations */
  if (unlikely (! ++stdrv->bdisp_shared->num_ops_lo))
    ++stdrv->bdisp_shared->num_ops_hi;
  /* every now and then we want to see a 'node completed interrupt (in
     addition to the usual 'queue completed' interrupt), so we don't have to
     wait for the queue to be completely empty when adding a node. But it also
     shouldn't occur too often so as to not waste too much CPU time in the
     interrupt handler... */
  if (!(stdrv->bdisp_shared->num_ops_lo % stdev->node_irq_delay))
    *blt_ins |= BLIT_INS_ENABLE_BLITCOMPIRQ;
}

#ifdef BDISP2_IMPLEMENT_WAITSERIAL
static void
bdisp2_update_config_static (struct stm_bdisp2_shared_area * const shared,
                             struct _BltNodeGroup14        * const group14)
{
  group14->BLT_SAR = 0;
  group14->BLT_USR = shared->num_ops_lo;
}
#else /* BDISP2_IMPLEMENT_WAITSERIAL */
#  define bdisp2_update_config_static(shared, group14)
#endif /* BDISP2_IMPLEMENT_WAITSERIAL */


#define COPY_TO_NODE_SZ(n, addr, size)    \
            ({                            \
              memcpy (n, (addr), (size)); \
              n += (size);                \
            })
#define COPY_TO_NODE(n, strct)                      \
            ({                                      \
              memcpy (n, &(strct), sizeof (strct)); \
              n += sizeof (strct);                  \
            })


#ifdef BDISP2_CLUT_UNSAFE_MULTISESSION
  #define bdisp_aq_clut_update_disable_if_unsafe(stdev) \
        ({ \
          if (!stdev->clut_disabled)                                        \
            {                                                               \
              stdev->setup->ConfigGeneral.BLT_INS &= ~BLIT_INS_ENABLE_BLITCOMPIRQ; \
              stdev->setup->ConfigClut.BLT_CCO    &= ~BLIT_CCO_CLUT_UPDATE_EN;     \
              stdev->clut_disabled = true;                                  \
            }                                                               \
        })
#else /* BDISP2_CLUT_UNSAFE_MULTISESSION */
  #define bdisp_aq_clut_update_disable_if_unsafe(stdev)
#endif /* BDISP2_CLUT_UNSAFE_MULTISESSION */


static void
bdisp_aq_copy_n_push_node (struct stm_bdisp2_driver_data * const stdrv,
                           struct stm_bdisp2_device_data * const stdev)
{
  unsigned char          *node;
  struct _BltNodeGroup00 *ConfigGeneral;
  uint32_t                valid_groups;

#ifdef BDISP2_IMPLEMENT_WAITSERIAL
  stdev->setup->ConfigGeneral.BLT_CIC |= BLIT_CIC_NODE_STATIC;
#endif /* BDISP2_IMPLEMENT_WAITSERIAL */

  ConfigGeneral = bdisp2_get_new_node (stdrv, stdev);

  stdev->setup->ConfigGeneral.BLT_CIC |= stdev->setup->all_states.extra_blt_cic;
  stdev->setup->ConfigGeneral.BLT_INS |= stdev->setup->all_states.extra_blt_ins;
  bdisp2_update_num_ops (stdrv, stdev, &stdev->setup->ConfigGeneral.BLT_INS);

  valid_groups = stdev->setup->ConfigGeneral.BLT_CIC;

  /* now copy the individual groups into the free node item of the nodelist */
  node = ((unsigned char *) ConfigGeneral) + sizeof (uint32_t);
  COPY_TO_NODE_SZ (node,
                   ((void *) &stdev->setup->ConfigGeneral) + sizeof (uint32_t),
                   sizeof (stdev->setup->ConfigGeneral) - sizeof (uint32_t));
  stdev->setup->ConfigGeneral.BLT_INS &= ~(BLIT_INS_ENABLE_BLITCOMPIRQ
                                           | stdev->setup->all_states.extra_blt_ins);
  stdev->setup->ConfigGeneral.BLT_CIC &= ~stdev->setup->all_states.extra_blt_cic;

  COPY_TO_NODE (node, stdev->setup->ConfigTarget);

  if (valid_groups & BLIT_CIC_NODE_COLOR)
    COPY_TO_NODE (node, stdev->setup->ConfigColor);

  if (valid_groups & BLIT_CIC_NODE_SOURCE1)
    COPY_TO_NODE (node, stdev->setup->ConfigSource1);

  if (valid_groups & BLIT_CIC_NODE_SOURCE2)
    COPY_TO_NODE (node, stdev->setup->ConfigSource2);

  if (valid_groups & BLIT_CIC_NODE_SOURCE3)
    COPY_TO_NODE (node, stdev->setup->ConfigSource3);

#ifdef BDISP2_SUPPORT_HW_CLIPPING
  if (valid_groups & BLIT_CIC_NODE_CLIP)
    COPY_TO_NODE (node, stdev->setup->ConfigClip);
#endif

  if (valid_groups & BLIT_CIC_NODE_CLUT)
    COPY_TO_NODE (node, stdev->setup->ConfigClut);

  if (valid_groups & BLIT_CIC_NODE_FILTERS)
    COPY_TO_NODE (node, stdev->setup->ConfigFilters);

  if (valid_groups & BLIT_CIC_NODE_2DFILTERSCHR)
    COPY_TO_NODE (node, stdev->setup->ConfigFiltersChr);

  if (valid_groups & BLIT_CIC_NODE_2DFILTERSLUMA)
    COPY_TO_NODE (node, stdev->setup->ConfigFiltersLuma);

  if (valid_groups & BLIT_CIC_NODE_FLICKER)
    COPY_TO_NODE (node, stdev->setup->ConfigFlicker);

  if (valid_groups & BLIT_CIC_NODE_COLORKEY)
    COPY_TO_NODE (node, stdev->setup->ConfigColorkey);

#ifdef BDISP2_IMPLEMENT_WAITSERIAL
  if (likely (valid_groups & BLIT_CIC_NODE_STATIC))
    {
      bdisp2_update_config_static (stdrv->bdisp_shared,
                                   &stdev->setup->ConfigStatic);
      COPY_TO_NODE (node, stdev->setup->ConfigStatic);
    }
#endif /* BDISP2_IMPLEMENT_WAITSERIAL */

  if (valid_groups & BLIT_CIC_NODE_IVMX)
    COPY_TO_NODE (node, stdev->setup->ConfigIVMX);

  if (valid_groups & BLIT_CIC_NODE_OVMX)
    COPY_TO_NODE (node, stdev->setup->ConfigOVMX);

#if 0
  if (valid_groups & BLIT_CIC_NODE_VC1R)
    COPY_TO_NODE (node, stdev->setup->ConfigVC1R);
#endif

  bdisp2_finish_node (stdrv, stdev, ConfigGeneral);
}




#if 0
static bool
CSTmBDispAQ__SetPlaneMask (const enum STM_BLITTER_FLAGS  flags,
                           const unsigned long           plane_mask,
                           uint32_t                     * const blt_cic,
                           uint32_t                     * const blt_ins,
                           uint32_t                     * const blt_pmk)
{
  if (!features->plane_mask)
    return true;

  if (unlikely ((*blt_ins & BLIT_INS_SRC1_MODE_MASK)
                == BLIT_INS_SRC1_MODE_COLOR_FILL))
    {
      stm_blit_printe ("%s: cannot use plane masking and blend against a solid colour\n",
                       __func__);
      return false;
    }

  *blt_cic |= BLIT_CIC_NODE_FILTERS;
  *blt_ins &= ~BLIT_INS_SRC1_MODE_MASK;
  *blt_ins |= BLIT_INS_ENABLE_PLANEMASK | BLIT_INS_SRC1_MODE_MEMORY;

  *blt_pmk = plane_mask;

  return true;
}
#endif


#ifdef BDISP2_SUPPORT_HW_CLIPPING
static void
bdisp2_set_clip (const struct stm_bdisp2_device_data * const stdev,
                 uint32_t                            * const blt_cic,
                 uint32_t                            * const blt_ins,
                 struct _BltNodeGroup06              * const config_clip)
{
  if (stdev->v_flags & 0x00000002 /* CLIP */)
    {
      *blt_cic |= BLIT_CIC_NODE_CLIP;
      *blt_ins |= BLIT_INS_ENABLE_RECTCLIP;
      *config_clip = stdev->setup->ConfigClip;
    }
}
#else /* BDISP2_SUPPORT_HW_CLIPPING */
#  define bdisp2_set_clip(stdev, blt_cic, blt_ins, config_clip)
#endif /* BDISP2_SUPPORT_HW_CLIPPING */


void
bdisp2_prepare_upload_palette_hw (struct stm_bdisp2_driver_data * const stdrv,
                                  struct stm_bdisp2_device_data * const stdev)
{
#ifndef BDISP2_CLUT_UNSAFE_MULTISESSION
  /* before updating the LUT, we have to wait for the blitter to finish all
     previous operations that would reload the LUT from memory. That's
     rather difficult to find out - so we wait for the complete nodelist to
     finish. */
  /* FIXME: it is very inefficient to wait for the blitter to sync here! */
  bdisp2_engine_sync (stdrv, stdev);
#else /* BDISP2_CLUT_UNSAFE_MULTISESSION */
  /* before updating the LUT, we have to wait for the blitter to finish all
     previous operations that would reload the LUT from memory. Rather than
     waiting for the complete nodelist to finish, we just wait as long as the
     blitter instruction involving the previous LUT has not been executed. */
   /* FIXME: locking */
  while (stdrv->bdisp_shared->last_lut)
    /* the CLUT could have been updated between the check above and the call
       to bdisp2_wait_space(). In that case we would be waiting
       unnecessarily; we could create a new IOCTL specifically for waiting
       for the CLUT to be updated, but we don't really care as no real harm
       arises from waiting unnecessarily. */
    bdisp2_wait_event (stdrv, stdev, NULL);
#endif /* BDISP2_CLUT_UNSAFE_MULTISESSION */
}

static void
bdisp2_upload_palette_hw (struct stm_bdisp2_driver_data * const stdrv,
                          struct stm_bdisp2_device_data * const stdev)
{
  int i;

  bdisp2_prepare_upload_palette_hw (stdrv, stdev);

  for (i = 0; i < stdev->setup->palette->num_entries; ++i)
    {
      const struct stm_blitter_color_s * const color
        = &stdev->setup->palette->entries[i];
      uint16_t alpha = color->a;

      /* Note that the alpha range for the lut is 0-128 (not 127!) */
      stdrv->clut_virt[SG2C_NORMAL][i] = SET_ARGB_PIXEL (((alpha + 1) / 2),
                                                         color->r,
                                                         color->g,
                                                         color->b);
    }
}


/* get a node and also prepare it for a 'simple' drawing operation, so that
   the only action which is left to the caller is to fill in the coordinates
   and call bdisp2_finish_node(). */
static struct _BltNode_Fast *
bdisp2_get_new_node_prepared_simple (struct stm_bdisp2_driver_data       * const stdrv,
                                     const struct stm_bdisp2_device_data * const stdev)
{
  struct _BltNode_Fast * const node = bdisp2_get_new_node (stdrv, stdev);
  uint32_t blt_ins = BLIT_INS_SRC1_MODE_DIRECT_FILL;

  bdisp2_update_num_ops (stdrv, stdev, &blt_ins);

  node->common.ConfigTarget.BLT_TBA = stdev->setup->ConfigTarget.BLT_TBA;
  node->common.ConfigTarget.BLT_TTY = bdisp_ty_sanitise_direction (stdev->setup->ConfigTarget.BLT_TTY);

  /* source1 == destination */
  node->common.ConfigSource1.BLT_S1TY = stdev->setup->drawstate.color_ty;
  node->common.ConfigColor.BLT_S1CF   = stdev->setup->drawstate.color;

  node->common.ConfigGeneral.BLT_CIC = BLIT_CIC_BLT_NODE_FAST;
  node->common.ConfigGeneral.BLT_INS = blt_ins;
  /* BLT_ACK is enabling only src1 */
  node->common.ConfigGeneral.BLT_ACK = BLIT_ACK_BYPASSSOURCE1;

  bdisp2_update_config_static (stdrv->bdisp_shared, &node->ConfigStatic);

  return node;
}

/* get a node and also prepare it for a drawing operation on the 'slow'
   path, so that the only action which is left to the caller is to fill in
   the coordinates and call bdisp2_finish_node(). */
static struct _BltNode_FullFill *
bdisp2_get_new_node_prepared_slow (struct stm_bdisp2_driver_data       * const stdrv,
                                   const struct stm_bdisp2_device_data * const stdev)
{
  struct _BltNode_FullFill * const node = bdisp2_get_new_node (stdrv, stdev);
  struct _BltNodeGroup08   *config_filters;
  struct _BltNodeGroup12   *config_colorkey;
  struct _BltNodeGroup14   *config_static;
  struct _BltNodeGroup15   *config_ivmx;
  struct _BltNodeGroup16   *config_ovmx;

  uint32_t blt_cic = (BLIT_CIC_BLT_NODE_COMMON
                      | BLIT_CIC_NODE_SOURCE2
                      | BLIT_CIC_NODE_STATIC);
  uint32_t blt_ins;
  uint32_t blt_ack;
  uint32_t src2type = stdev->setup->drawstate.color_ty | BLIT_STY_COLOR_EXPAND_MSB;

  blt_cic |= (stdev->setup->drawstate.ConfigGeneral.BLT_CIC
              | stdev->setup->all_states.extra_blt_cic);
  blt_ins  = (stdev->setup->drawstate.ConfigGeneral.BLT_INS
              | stdev->setup->all_states.extra_blt_ins);
  blt_ack  = stdev->setup->drawstate.ConfigGeneral.BLT_ACK;

  bdisp2_update_num_ops (stdrv, stdev, &blt_ins);

  node->common.ConfigTarget.BLT_TBA = stdev->setup->ConfigTarget.BLT_TBA;
  node->common.ConfigTarget.BLT_TTY = bdisp_ty_sanitise_direction (stdev->setup->ConfigTarget.BLT_TTY);

  /* source1 == destination */
  node->common.ConfigSource1.BLT_S1BA = stdev->setup->ConfigTarget.BLT_TBA;
  node->common.ConfigSource1.BLT_S1TY =
    (bdisp_ty_sanitise_direction (stdev->setup->ConfigTarget.BLT_TTY)
     | BLIT_STY_COLOR_EXPAND_MSB);

  node->common.ConfigColor.BLT_S2CF = stdev->setup->drawstate.color;
  node->ConfigSource2.BLT_S2TY = src2type;

  /* if the clut is enabled, filters starts after the clut group, if
     not, everything is moved up a bit */
  if (blt_cic & BLIT_CIC_NODE_CLUT)
    {
      node->ConfigClut = stdev->setup->drawstate.ConfigClut;
      config_filters = (struct _BltNodeGroup08 *) (&node->ConfigClut + 1);
    }
  else
    {
      /* avoid compiler warning: dereferencing type-punned pointer will break
         strict-aliasing rules */
      union {
        struct _BltNodeGroup07 *clut;
        struct _BltNodeGroup08 *filt;
      } temp;
      temp.clut = &node->ConfigClut;
      config_filters = temp.filt;
    }

  /* if filters is enabled, colorkey starts after the filters group, if
     not, everything is moved up a bit */
  if (blt_cic & BLIT_CIC_NODE_FILTERS)
    {
      node->ConfigFilters.BLT_FCTL_RZC = 0;
      node->ConfigFilters.BLT_PMK = stdev->setup->ConfigFilters.BLT_PMK;
      config_colorkey = (struct _BltNodeGroup12 *) (config_filters + 1);
    }
  else
    config_colorkey = (struct _BltNodeGroup12 *) config_filters;

  /* if colorkeying is enabled, static starts after the colorkey group, if
     not, everything is moved up a bit */
  if (blt_cic & BLIT_CIC_NODE_COLORKEY)
    {
      config_colorkey->BLT_KEY1 = stdev->setup->all_states.dst_ckey[0];
      config_colorkey->BLT_KEY2 = stdev->setup->all_states.dst_ckey[1];

      config_static = (struct _BltNodeGroup14 *) (config_colorkey + 1);
    }
  else
    config_static = (struct _BltNodeGroup14 *) config_colorkey;

  bdisp2_update_config_static (stdrv->bdisp_shared, config_static);

  /* if we have the static group, ivmx starts after it, if not, everything
     is moved up a bit */
  config_ivmx = (struct _BltNodeGroup15 *)
    ((blt_cic & BLIT_CIC_NODE_STATIC) ? (config_static + 1) : config_static);

  if (blt_cic & BLIT_CIC_NODE_IVMX)
    {
      if (BLIT_TY_COLOR_FORM_IS_YUV (stdev->setup->ConfigTarget.BLT_TTY)
          && !BLIT_TY_COLOR_FORM_IS_YUV (stdev->setup->drawstate.color_ty))
        set_matrix (config_ivmx->BLT_IVMX, bdisp_aq_RGB_2_VideoYCbCr601);

      if (!BLIT_TY_COLOR_FORM_IS_YUV (stdev->setup->ConfigTarget.BLT_TTY)
          && BLIT_TY_COLOR_FORM_IS_YUV (stdev->setup->drawstate.color_ty))
        set_matrix (config_ivmx->BLT_IVMX, bdisp_aq_VideoYCbCr601_2_RGB);

      config_ovmx = (struct _BltNodeGroup16 *) (config_ivmx + 1);
    }
  else
    config_ovmx = (struct _BltNodeGroup16 *) config_ivmx;

  /* if we have the ivmx group, ovmx starts after it, if not, everything
     is moved up a bit */
  if (blt_cic & BLIT_CIC_NODE_OVMX)
    {
      config_ovmx->BLT_OVMX0 = stdev->setup->ConfigOVMX.BLT_OVMX0;
      config_ovmx->BLT_OVMX1 = stdev->setup->ConfigOVMX.BLT_OVMX1;
      config_ovmx->BLT_OVMX2 = stdev->setup->ConfigOVMX.BLT_OVMX2;
      config_ovmx->BLT_OVMX3 = stdev->setup->ConfigOVMX.BLT_OVMX3;
    }

  bdisp2_set_clip (stdev, &blt_cic, &blt_ins, &node->ConfigClip);

  node->common.ConfigGeneral.BLT_CIC = blt_cic;
  node->common.ConfigGeneral.BLT_INS = blt_ins;
  node->common.ConfigGeneral.BLT_ACK = blt_ack;

  return node;
}




void
bdisp_aq_setup_blit_operation (struct stm_bdisp2_driver_data * const stdrv,
                               struct stm_bdisp2_device_data * const stdev)
{
  /* we might have done a stretch blit before and do a normal blit
     now, in which case make sure to disable the rescaling engine. */
  stdev->setup->ConfigGeneral.BLT_CIC &= ~(BLIT_CIC_NODE_2DFILTERSCHR
                                    | BLIT_CIC_NODE_2DFILTERSLUMA);
  if (!(stdev->setup->ConfigGeneral.BLT_INS & BLIT_INS_ENABLE_PLANEMASK))
    stdev->setup->ConfigGeneral.BLT_CIC &= ~BLIT_CIC_NODE_FILTERS;
  stdev->setup->ConfigGeneral.BLT_INS &= ~BLIT_INS_ENABLE_2DRESCALE;

  if (!stdev->setup->blitstate.bIndexTranslation
      && (stdev->setup->palette
          || stdev->setup->palette_type != SG2C_NORMAL))
    {
      if (stdev->setup->palette)
        /* user palettes are written to the same memory location, so we need
           to make sure not to overwrite any previous potentially different
           palette while still in use. Therefore wait for the BDisp to be
           idle. Color correction palettes are located at different places
           in physical memory, which is why we can simply adjust the memory
           pointer for those.
           We will likely have to optimize this behaviour! */
        bdisp2_upload_palette_hw (stdrv, stdev);

      bdisp2_set_clut_operation (stdrv, stdev,
                                 stdev->setup->do_expand,
                                 &stdev->setup->ConfigGeneral.BLT_CIC,
                                 &stdev->setup->ConfigGeneral.BLT_INS,
                                 &stdev->setup->ConfigClut);

      stm_blit_printd (BDISP_BLT,
                       "%s(): CLUT type: %#.8x cco/cml: %#.8x %#.8x\n",
                       __func__,
                       (stdev->setup->ConfigSource2.BLT_S2TY
                        & (BLIT_TY_COLOR_FORM_MASK
                           | BLIT_TY_FULL_ALPHA_RANGE
                           | BLIT_TY_BIG_ENDIAN)),
                       stdev->setup->ConfigClut.BLT_CCO,
                       stdev->setup->ConfigClut.BLT_CML);

#ifdef BDISP2_CLUT_UNSAFE_MULTISESSION
      /* set this to false, so code elsewhere knows the CLUT upload
         is enabled and can disable it. */
      stdev->clut_disabled = false;
#endif /* BDISP2_CLUT_UNSAFE_MULTISESSION */
    }
  else
    {
      stdev->setup->ConfigGeneral.BLT_CIC &= ~BLIT_CIC_NODE_CLUT;
      stdev->setup->ConfigGeneral.BLT_INS &= ~BLIT_INS_ENABLE_CLUT;
    }
}






#define FIXED_POINT_TO_BDISP(f)  ((f) >> 6)

static int
FixedPointToInteger (int       f16,
                     uint32_t * const fraction)
{
  int integer = FIXED_POINT_TO_INT (f16);

  if (fraction)
    *fraction = f16 - (TO_FIXED_POINT (integer));

  return integer;
}


static void
GetXTapFilterSetup (int           this_src_x /* 16.16 */,
                    int           real_src_x /* 16.16 */,
                    int           total_src_width /* 16.16 */,
                    int           h_src2_sign /* 1 or -1 */,
                    uint32_t     * const subpixelpos /* n.16 */,
                    int          * const adjustedsrcx /* int */,
                    int          * const srcspanwidth /* 16.16 */,
                    unsigned int * const repeat /* int */,
                    unsigned int  n_taps)
{
  int edgedistance /* int */;

  /* we need some pixels to init and end the filter */
  int _prefill, _postfill;

  switch (n_taps)
    {
    case 8:
      _prefill = 3;
      _postfill = 4;
      break;

    case 5:
      _prefill = 2;
      _postfill = 2;
      break;

    default:
      _prefill = (n_taps - 1) / 2;
      _postfill = n_taps / 2;
      break;
    }

  /* Adjust the start position by half a pixel. This makes sure that the
     middle of the filter is used. */
  this_src_x += ((FIXED_POINT_ONE / 2) * h_src2_sign);
  ++_prefill;

  /* We need the subpixel position of the start, which is basically where the
     filter left off on the previous span. */
  *adjustedsrcx = FixedPointToInteger (this_src_x, subpixelpos);
  if (h_src2_sign == -1)
    {
      /* This is not immediately obvious:
         1. We have to flip around the subpixel position as the filter is
            being filled with pixels in reverse.
         2. Then if the subpixel position is not zero, we have to move our
            first real pixel sample in the middle of the filter to the one on
            the right of the current position.
            This could make the edge distance go negative on the first span
            if the subpixel position is not zero, but that is ok because we
            will then end up with an extra repeated pixel to pad the filter
            taps. */
      *subpixelpos = (FIXED_POINT_ONE - *subpixelpos) % FIXED_POINT_ONE;
      if (*subpixelpos != 0)
        {
          ++(*adjustedsrcx);
          *srcspanwidth += FIXED_POINT_ONE;
        }
    }

  /* Adjust things to fill the first three filter taps either with real data
     or a repeat of the first pixel. */
  edgedistance = ((*adjustedsrcx - FIXED_POINT_TO_INT (real_src_x))
                  * h_src2_sign
                 );
  stm_blit_printd (BDISP_STRETCH,
                   "  -> span edgedistance: %d\n", edgedistance);

  if (edgedistance >= _prefill)
    {
      *repeat        = 0;
      *adjustedsrcx -= (_prefill * h_src2_sign);
      *srcspanwidth += TO_FIXED_POINT (_prefill);
    }
  else
    {
      *repeat        = _prefill - edgedistance;
      *adjustedsrcx  = FIXED_POINT_TO_INT (real_src_x);
      *srcspanwidth += TO_FIXED_POINT (edgedistance);
    }

  /* Now adjust for an extra number of pixels to be read to fill the last
     filter taps, or to repeat the last pixel if we have gone outside of
     the source image... */
  *srcspanwidth += TO_FIXED_POINT (_postfill);

  /* ...by clamping the source width we program in the hardware. */
  if (h_src2_sign == 1
      && ((TO_FIXED_POINT (*adjustedsrcx) - real_src_x + *srcspanwidth)
          > total_src_width))
    *srcspanwidth = total_src_width - (TO_FIXED_POINT (*adjustedsrcx)
                                       - real_src_x);
  else if (h_src2_sign == -1
           && ((real_src_x - total_src_width + *srcspanwidth)
               > TO_FIXED_POINT (*adjustedsrcx)))
    *srcspanwidth = TO_FIXED_POINT (*adjustedsrcx) - (real_src_x
                                                      - total_src_width);

  stm_blit_printd (BDISP_STRETCH,
                   "     adjsrcx/y: %d subpixelpos: %x (%lld.%06lld) srcspanwidth/height: %x (%lld.%06lld) repeat: %d\n",
                   *adjustedsrcx,
                   *subpixelpos, INT_VALf (*subpixelpos),
                   *srcspanwidth, INT_VALf (*srcspanwidth),
                   *repeat);
}

static void
Get8TapFilterSetup (int           this_src_x /* 16.16 */,
                    int           real_src_x /* 16.16 */,
                    int           total_src_width /* 16.16 */,
                    int           h_src2_sign /* 1 or -1 */,
                    uint32_t     * const subpixelpos /* n.16 */,
                    int          * const adjustedsrcx /* int */,
                    int          * const srcspanwidth /* 16.16 */,
                    unsigned int * const repeat /* int */)
{
  GetXTapFilterSetup (this_src_x, real_src_x, total_src_width,
                      h_src2_sign,
                      subpixelpos, adjustedsrcx, srcspanwidth, repeat,
                      8);
}

static void
Get5TapFilterSetup (int           this_src_y /* 16.16 */,
                    int           real_src_y /* 16.16 */,
                    int           total_src_height /* 16.16 */,
                    int           v_src2_sign /* 1 or -1 */,
                    uint32_t     * const subpixelpos /* n.16 */,
                    int          * const adjustedsrcy /* int */,
                    int          * const srcspanheight /* 16.16 */,
                    unsigned int * const repeat /* int */)
{
  GetXTapFilterSetup (this_src_y, real_src_y, total_src_height,
                      v_src2_sign,
                      subpixelpos, adjustedsrcy, srcspanheight, repeat,
                      5);
}

/* Avoid internal bottleneck due to pixel synchronization between luma
 * and chroma component for the chroma pass, by reducing luma size to
 * match chroma size.
 * Note: BLT_S3SZ, BLT_Y_RSF will be changed */
#define BLIT_OPTIMIZE_LUMA(setup)                                               \
{                                                                               \
  if ((setup)->ConfigTarget.BLT_TTY & BLIT_TTY_CHROMA_NOTLUMA                   \
      && (BLIT_TY_COLOR_FORM_IS_2_BUFFER((setup)->ConfigSource2.BLT_S2TY)       \
          || BLIT_TY_COLOR_FORM_IS_3_BUFFER((setup)->ConfigSource2.BLT_S2TY)))  \
    {                                                                           \
      (setup)->ConfigSource3.BLT_S3SZ      = (setup)->ConfigSource2.BLT_S2SZ;   \
      (setup)->ConfigFiltersLuma.BLT_Y_RSF = (setup)->ConfigFiltersChr.BLT_RSF; \
    }                                                                           \
}

static void
bdisp2_blit_using_spans (struct stm_bdisp2_driver_data * const stdrv,
                         struct stm_bdisp2_device_data * const stdev,
                         uint16_t                       line_buffer_length,
                         const stm_blitter_rect_t      * const src,
                         const stm_blitter_rect_t      * const src1,
                         const stm_blitter_rect_t      * const dst)
{
  /* The filter delay lines can hold 128 pixels (or more, depending on the
   implementation), we want to process slightly less than that because we need
   up to 4 pixels of overlap between spans to fill the filter taps properly
   and not end up with stripes down the image at the edges of two adjoining
   spans. */
  unsigned long max_filterstep = ((line_buffer_length - 8)
                                  * FIXED_POINT_ONE);
  int           hsrcinc = stdev->hsrcinc;

  /* Calculate how many destination pixels will get generated from one
     span of the source image. */
  int this_dst_step = max_filterstep / hsrcinc;

  /* Work the previous calculation backwards so we don't get rounding
     errors causing bad filter positioning in the middle of the scaled
     image. We deal with any left over pixels at the edge of the image
     separately.
     This puts srcstep into 16.16 fixed point. */
  const int srcstep = this_dst_step * hsrcinc;

  /* the following combinations are possible:
                              SRC2                     SRC1 (TARGET)
     copy formwards    TopLeft -> BottomRight     TopLeft -> BottomRight
     copy backwards    BottomRight -> TopLeft     BottomRight -> TopLeft
         for SRC2 width >= SRC1
     rotate 180°       TopLeft -> BottomRight     BottomRight -> TopLeft
     mirror against x  TopLeft -> BottomRight     BottomLeft -> TopRight
     mirror against y  TopLeft -> BottomRight     TopRight -> BottomLeft
         for SRC2 width < SRC1
     rotate 180°       BottomRight -> TopLeft     TopLeft -> BottomRight
     mirror against x  BottomLeft -> TopRight     TopLeft -> BottomRight
     mirror against y  TopRight -> BottomLeft     TopLeft -> BottomRight
  */

  /* rounding adjustment */
  int error_adjust = 0;

  long this_src_x = src->position.x;
  long this_src1_x = src1->position.x;
  long this_dst_x = dst->position.x;
  int  _srcspanwidth = srcstep /* 16.16 */;

  uint32_t s2sz = stdev->setup->ConfigSource2.BLT_S2SZ & 0xffff0000;
  uint32_t tsz  = stdev->setup->ConfigTarget.BLT_TSZ_S1SZ & 0xffff0000;
  uint32_t rzi = (stdev->setup->ConfigFiltersChr.BLT_RZI
                  & ~(BLIT_RZI_H_REPEAT_MASK | BLIT_RZI_H_INIT_MASK));

  /* because of rounding errors we might run out of source pixels (when
     upscaling). To work around that, we calculate how many pixels we are
     off and slightly move the source x such that we read this extra
     amount of pixels per span. Chosing a span width small enough doesn't
     cause any visible artifacts, but the code should be changed to select
     the span width dynamically based on the resulting rounding error. In
     addition, the latests chips have a big line buffer, and doing this
     adjustment statically, without changing the span width will look
     horrible. */
#if 0
  {
  long total = TO_FIXED_POINT ((int64_t) src->size.w) / hsrcinc;
//  total -= TO_FIXED_POINT (3);
  long error = (((int64_t) (dst->size.w - total)) * hsrcinc) / FIXED_POINT_ONE;
  if (error > 0)
    {
      long spans = (src->size.w + max_filterstep/2) / max_filterstep;
      error_adjust = error / spans;
    }
  }
#endif

  stm_blit_printd (BDISP_STRETCH_SPANS,
                   "%s: %lld.%06lld,%lld.%06lld-%lld.%06lldx%lld.%06lld -> %lld.%06lld,%lld.%06lld-%lld.%06lldx%lld.%06lld srcstep: 0x%x (%lld.%06lld) dststep: %d\n",
                   __func__,
                   RECTANGLE_VALSf (src), RECTANGLE_VALSf (dst),
                   srcstep, INT_VALf (srcstep), this_dst_step);

  while (this_dst_step /* int */ > 0)
    {
      int adjustedsrcx /* int */, srcspanwidth = _srcspanwidth /* 16.16 */;
      uint32_t subpixelpos /* n.16 */;
      unsigned int repeat /* int */;

      stm_blit_printd (BDISP_STRETCH,
                       "     this_dst_x: %lld.%06lld this_dst_step: %d\n",
                       INT_VALf (this_dst_x), this_dst_step);

      Get8TapFilterSetup (this_src_x, src->position.x, src->size.w,
                          stdev->setup->h_src2_sign,
                          &subpixelpos, &adjustedsrcx, &srcspanwidth, &repeat);

      stdev->setup->ConfigFiltersChr.BLT_RZI  = rzi;
      stdev->setup->ConfigFiltersChr.BLT_RZI |= (repeat << BLIT_RZI_H_REPEAT_SHIFT);
      stdev->setup->ConfigFiltersChr.BLT_RZI |= FIXED_POINT_TO_BDISP (subpixelpos) << BLIT_RZI_H_INIT_SHIFT;

      stdev->setup->ConfigTarget.BLT_TXY  = FIXED_POINT_TO_INT (dst->position.y) << 16;
      stdev->setup->ConfigTarget.BLT_TXY |= FIXED_POINT_TO_INT (this_dst_x);

      stdev->setup->ConfigSource1.BLT_S1XY = FIXED_POINT_TO_INT (src1->position.y) << 16;
      stdev->setup->ConfigSource1.BLT_S1XY |= FIXED_POINT_TO_INT (this_src1_x);

      stdev->setup->ConfigTarget.BLT_TSZ_S1SZ  = tsz;
      stdev->setup->ConfigTarget.BLT_TSZ_S1SZ |= (this_dst_step
                                                  & stdev->features.hw.size_mask);

      if (!(stdev->setup->ConfigGeneral.BLT_CIC & BLIT_CIC_NODE_SOURCE3))
        {
          stdev->setup->ConfigSource2.BLT_S2XY  = FIXED_POINT_TO_INT (src->position.y) << 16;
          stdev->setup->ConfigSource2.BLT_S2XY |= adjustedsrcx;
          stdev->setup->ConfigSource2.BLT_S2SZ  = s2sz;
          stdev->setup->ConfigSource2.BLT_S2SZ |= (FIXED_POINT_TO_INT (srcspanwidth)
                                                   & stdev->features.hw.size_mask);

          stm_blit_printd (BDISP_STRETCH,
                           "     S2XY: %.8x S2SZ: %.8x -> TXY %.8x TSZ %.8x (RZI %.8x)\n",
                           stdev->setup->ConfigSource2.BLT_S2XY,
                           stdev->setup->ConfigSource2.BLT_S2SZ,
                           stdev->setup->ConfigTarget.BLT_TXY,
                           stdev->setup->ConfigTarget.BLT_TSZ_S1SZ,
                           stdev->setup->ConfigFiltersChr.BLT_RZI);
        }
      else
        {
          stdev->setup->ConfigFiltersLuma.BLT_Y_RZI
            = stdev->setup->ConfigFiltersChr.BLT_RZI;

          stdev->setup->ConfigSource3.BLT_S3XY
            = ((FIXED_POINT_TO_INT (src->position.y) << 16)
               | adjustedsrcx);
          stdev->setup->ConfigSource3.BLT_S3SZ
            = (s2sz
               | (FIXED_POINT_TO_INT (srcspanwidth) & stdev->features.hw.size_mask));

          if (stdev->setup->blitstate.srcFactorH == 2
              || stdev->setup->blitstate.srcFactorV == 2)
            {
              srcspanwidth = _srcspanwidth / stdev->setup->blitstate.srcFactorH;

              Get8TapFilterSetup (this_src_x / stdev->setup->blitstate.srcFactorH,
                                  src->position.x / stdev->setup->blitstate.srcFactorH,
                                  src->size.w / stdev->setup->blitstate.srcFactorH,
                                  stdev->setup->h_src2_sign,
                                  &subpixelpos,
                                  &adjustedsrcx,
                                  &srcspanwidth,
                                  &repeat);

              stdev->setup->ConfigFiltersChr.BLT_RZI
                = (rzi
                   | (repeat << BLIT_RZI_H_REPEAT_SHIFT)
                   | (FIXED_POINT_TO_BDISP (subpixelpos) << BLIT_RZI_H_INIT_SHIFT));

              stdev->setup->ConfigSource2.BLT_S2XY
                = (((FIXED_POINT_TO_INT (src->position.y
                                         / stdev->setup->blitstate.srcFactorV)
                     << 16) & (stdev->features.hw.size_mask << 16))
                   | adjustedsrcx);
              stdev->setup->ConfigSource2.BLT_S2SZ
                = (((s2sz / stdev->setup->blitstate.srcFactorV)
		    & (stdev->features.hw.size_mask << 16))
                   | (FIXED_POINT_TO_INT (srcspanwidth)
		      & stdev->features.hw.size_mask));

              /* just to be safe in case we determined to blit 1 luma pixel (which
                 could be 0.5 chroma pixels, i.e. 0) */
              if (unlikely (!(stdev->setup->ConfigSource2.BLT_S2SZ
	                      & (stdev->features.hw.size_mask << 16))))
                stdev->setup->ConfigSource2.BLT_S2SZ |= 1 << 16;
              if (unlikely (!(stdev->setup->ConfigSource2.BLT_S2SZ
	                      & stdev->features.hw.size_mask)))
                stdev->setup->ConfigSource2.BLT_S2SZ |= 1;
            }
          else /* if (stdev->setup->blitstate.srcFactorH == 1) */
            {
              stdev->setup->ConfigSource2.BLT_S2XY = stdev->setup->ConfigSource3.BLT_S3XY;
              stdev->setup->ConfigSource2.BLT_S2SZ = stdev->setup->ConfigSource3.BLT_S3SZ;
            }

          BLIT_OPTIMIZE_LUMA(stdev->setup);

          stm_blit_printd (BDISP_STRETCH,
                           "     S3XY: %.8x S3SZ: %.8x S2XY: %.8x S2SZ: %.8x S1XY: %.8x -> TXY %.8x TSZ %.8x (RZI %.8x Y_RZI %.8x)\n",
                           stdev->setup->ConfigSource3.BLT_S3XY,
                           stdev->setup->ConfigSource3.BLT_S3SZ,
                           stdev->setup->ConfigSource2.BLT_S2XY,
                           stdev->setup->ConfigSource2.BLT_S2SZ,
                           stdev->setup->ConfigSource1.BLT_S1XY,
                           stdev->setup->ConfigTarget.BLT_TXY,
                           stdev->setup->ConfigTarget.BLT_TSZ_S1SZ,
                           stdev->setup->ConfigFiltersChr.BLT_RZI,
                           stdev->setup->ConfigFiltersLuma.BLT_Y_RZI);
        }

      bdisp_aq_copy_n_push_node (stdrv, stdev);

      this_src_x += (srcstep * stdev->setup->h_src2_sign);
      this_src_x -= (error_adjust * stdev->setup->h_src2_sign);
      this_src1_x += (TO_FIXED_POINT (this_dst_step) * stdev->setup->h_trgt_sign);
      this_dst_x += (TO_FIXED_POINT (this_dst_step) * stdev->setup->h_trgt_sign);

      /* Alter the span sizes for the "last" span where we have an
         odd bit left over (i.e. the total destination is not a nice
         multiple of this_dst_step). */
      if (stdev->setup->h_trgt_sign == 1)
        {
          /* Remember that x + w (== right) isn't in the destination rect */
          long dst_right = dst->position.x + dst->size.w;
          if ((dst_right - this_dst_x) < TO_FIXED_POINT (this_dst_step))
            this_dst_step = FIXED_POINT_TO_INT (dst_right - this_dst_x);
        }
      else
        {
          long dst_left = dst->position.x - dst->size.w + FIXED_POINT_ONE;
          if (((this_dst_x - dst_left) + FIXED_POINT_ONE) < TO_FIXED_POINT (this_dst_step))
            this_dst_step = FIXED_POINT_TO_INT ((this_dst_x - dst_left) + FIXED_POINT_ONE);
        }

      /* to save bandwidth, we make sure to update the CLUT only
         once. Also, we turn off the BLIT_INS_ENABLE_BLITCOMPIRQ
         since we need it only once to notify us when the node
         containing the CLUT update has been executed. */
      bdisp_aq_clut_update_disable_if_unsafe (stdev);
    }
}

static void
bdisp2_blit_flicker_filter(struct stm_bdisp2_driver_data * const stdrv,
                           struct stm_bdisp2_device_data * const stdev,
                           int                            line_buffer_length,
                           const stm_blitter_rect_t      * const src,
                           const stm_blitter_rect_t      * const dst)
{
  long w_remaining = FIXED_POINT_TO_INT(src->size.w);
  long src_x = FIXED_POINT_TO_INT(src->position.x);
  long src_y = FIXED_POINT_TO_INT(src->position.y);
  long src_h = FIXED_POINT_TO_INT(src->size.h);
  long dst_x = FIXED_POINT_TO_INT(dst->position.x);
  long dst_y = FIXED_POINT_TO_INT(dst->position.y);
  long dst_h = FIXED_POINT_TO_INT(dst->size.h);
  long this_w = STM_BLITTER_MIN ((long)line_buffer_length, w_remaining);

  while (w_remaining > 0)
    {
      stdev->setup->ConfigTarget.BLT_TXY = (dst_y << 16) | dst_x;
      /* FIXME: This has to be updated when supporting blit2 */
      stdev->setup->ConfigSource1.BLT_S1XY = stdev->setup->ConfigTarget.BLT_TXY;

      stdev->setup->ConfigTarget.BLT_TSZ_S1SZ = (dst_h << 16) | this_w;

      stdev->setup->ConfigSource2.BLT_S2XY = (src_y << 16) | src_x;
      stdev->setup->ConfigSource2.BLT_S2SZ = (src_h << 16) | this_w;

      bdisp_aq_copy_n_push_node (stdrv, stdev);
      bdisp_aq_clut_update_disable_if_unsafe (stdev);

      dst_x += line_buffer_length;
      src_x += this_w;
      w_remaining -= this_w;

      this_w = STM_BLITTER_MIN ((long)line_buffer_length, w_remaining);
    }
}

static bool
bdisp2_rectangles_intersect (const stm_blitter_rect_t * __restrict rect1,
                             const stm_blitter_rect_t * __restrict rect2)
{
  long right1  = rect1->position.x + rect1->size.w;
  long bottom1 = rect1->position.y + rect1->size.h;
  long right2  = rect2->position.x + rect2->size.w;
  long bottom2 = rect2->position.y + rect2->size.h;

  return !(rect1->position.x >= right2 || right1 <= rect2->position.x
           || rect1->position.y >= bottom2 || bottom1 <= rect2->position.y);
}

static bool
__attribute__((warn_unused_result))
bdisp2_setup_directions (struct stm_bdisp2_device_data * const stdev,
                         const stm_blitter_rect_t      * const src2,
                         const stm_blitter_rect_t      * const src1,
                         const stm_blitter_rect_t      * const dst)
{
  stdev->setup->ConfigTarget.BLT_TTY   &= ~BLIT_TY_COPYDIR_MASK;
  stdev->setup->ConfigSource1.BLT_S1TY &= ~BLIT_TY_COPYDIR_MASK;
  stdev->setup->ConfigSource2.BLT_S2TY &= ~BLIT_TY_COPYDIR_MASK;
  stdev->setup->ConfigSource3.BLT_S3TY &= ~BLIT_TY_COPYDIR_MASK;

  /* work out copy direction */

  /* we don't support any overlap at all during rotation */
  if (stdev->setup->blitstate.rotate
      && (stdev->setup->ConfigSource2.BLT_S2BA
          == stdev->setup->ConfigSource1.BLT_S1BA))
    {
      if (bdisp2_rectangles_intersect (src1, src2))
        /* scary, by carefully checking we might be able to allow some of
           these. but not today... */
        return false;
    }

  /* we need to know the copy direction in various places */
  stdev->setup->h_trgt_sign
    = stdev->setup->h_src2_sign
    = stdev->setup->v_trgt_sign
    = stdev->setup->v_src2_sign
    = 1;

  switch (stdev->setup->blitstate.rotate)
    {
    case 0:
      {
      bool     backwards;
      uint32_t trgt_dir;

      /* FIXME: for blit2 */
      backwards = ((stdev->setup->ConfigSource2.BLT_S2BA
                    == stdev->setup->ConfigSource1.BLT_S1BA)
                   && ((src1->position.y > src2->position.y
                        || ((src1->position.y == src2->position.y)
                            && (src1->position.x >= src2->position.x)))
                       && bdisp2_rectangles_intersect (src2, src1)
                      )
                  );

      if (!backwards)
        {
          /* target from top left to bottom right */
          /* source2 from top left to bottom right */
          trgt_dir = (BLIT_TY_COPYDIR_TOPBOTTOM | BLIT_TY_COPYDIR_LEFTRIGHT);
        }
      else
        {
          /* target from bottom right to top left */
          /* source2 from bottom right to top left */
          trgt_dir = (BLIT_TY_COPYDIR_BOTTOMTOP | BLIT_TY_COPYDIR_RIGHTLEFT);
          stdev->setup->h_trgt_sign
            = stdev->setup->h_src2_sign
            = stdev->setup->v_trgt_sign
            = stdev->setup->v_src2_sign
            = -1;
        }
      stdev->setup->ConfigTarget.BLT_TTY |= trgt_dir;
      stdev->setup->ConfigSource2.BLT_S2TY |= trgt_dir;
      stdev->setup->ConfigSource3.BLT_S3TY |= trgt_dir;
      }
      break;

    case 180: /* rotate 180° */
      if ((src2->size.w + TO_FIXED_POINT (7)) >= (src1->size.w + dst->size.w))
        {
          /* target from bottom right to top left */
          /* source2 from top left to bottom right */
          stdev->setup->ConfigTarget.BLT_TTY |= (BLIT_TY_COPYDIR_BOTTOMTOP
                                                 | BLIT_TY_COPYDIR_RIGHTLEFT);
          stdev->setup->ConfigSource2.BLT_S2TY |= (BLIT_TY_COPYDIR_TOPBOTTOM
                                                   | BLIT_TY_COPYDIR_LEFTRIGHT);
          stdev->setup->h_trgt_sign = stdev->setup->v_trgt_sign = -1;
        }
      else
        {
          /* target from top left to bottom right */
          /* source2 from bottom right to top left */
          stdev->setup->ConfigTarget.BLT_TTY |= (BLIT_TY_COPYDIR_TOPBOTTOM
                                                 | BLIT_TY_COPYDIR_LEFTRIGHT);
          stdev->setup->ConfigSource2.BLT_S2TY |= (BLIT_TY_COPYDIR_BOTTOMTOP
                                                   | BLIT_TY_COPYDIR_RIGHTLEFT);
          stdev->setup->h_src2_sign = stdev->setup->v_src2_sign = -1;
        }
      break;

    case 181: /* reflection against x */
      if ((src2->size.h + TO_FIXED_POINT (4)) >= (src1->size.h + dst->size.h))
        {
          /* target from bottom left to top right */
          /* source2 from top left to bottom right */
          stdev->setup->ConfigTarget.BLT_TTY |= (BLIT_TY_COPYDIR_BOTTOMTOP
                                                 | BLIT_TY_COPYDIR_LEFTRIGHT);
          stdev->setup->ConfigSource2.BLT_S2TY |= (BLIT_TY_COPYDIR_TOPBOTTOM
                                                   | BLIT_TY_COPYDIR_LEFTRIGHT);
          stdev->setup->v_trgt_sign = -1;
        }
      else
        {
          /* target from top left to bottom right */
          /* source2 from bottom left to top right */
          stdev->setup->ConfigTarget.BLT_TTY |= (BLIT_TY_COPYDIR_TOPBOTTOM
                                                 | BLIT_TY_COPYDIR_LEFTRIGHT);
          stdev->setup->ConfigSource2.BLT_S2TY |= (BLIT_TY_COPYDIR_BOTTOMTOP
                                                   | BLIT_TY_COPYDIR_LEFTRIGHT);
          stdev->setup->v_src2_sign = -1;
        }
      break;

    case 182: /* reflection against y */
      if ((src2->size.w + TO_FIXED_POINT (7)) >= (src1->size.w + dst->size.w))
        {
          /* target from top right to bottom left */
          /* source2 from top left to bottom right */
          stdev->setup->ConfigTarget.BLT_TTY |= (BLIT_TY_COPYDIR_TOPBOTTOM
                                                 | BLIT_TY_COPYDIR_RIGHTLEFT);
          stdev->setup->ConfigSource2.BLT_S2TY |= (BLIT_TY_COPYDIR_TOPBOTTOM
                                                   | BLIT_TY_COPYDIR_LEFTRIGHT);
          stdev->setup->h_trgt_sign = -1;
        }
      else
        {
          /* target from top left to bottom right */
          /* source2 from top right to bottom left */
          stdev->setup->ConfigTarget.BLT_TTY |= (BLIT_TY_COPYDIR_TOPBOTTOM
                                                 | BLIT_TY_COPYDIR_LEFTRIGHT);
          stdev->setup->ConfigSource2.BLT_S2TY |= (BLIT_TY_COPYDIR_TOPBOTTOM
                                                   | BLIT_TY_COPYDIR_RIGHTLEFT);
          stdev->setup->h_src2_sign = -1;
        }
      break;

    default:
      /* should not be reached */
      return false;
    }

  /* FIXME: 3 buffer formats */
  stdev->setup->ConfigSource1.BLT_S1TY |= (stdev->setup->ConfigTarget.BLT_TTY
                                           & BLIT_TY_COPYDIR_MASK);

  stm_blit_printd (BDISP_BLT,
                   "  -> Generating src2 blit %s & %s\n",
                   (stdev->setup->h_src2_sign == 1) ? "left to right" : "right to left",
                   (stdev->setup->v_src2_sign == 1) ? "top to bottom" : "bottom to top");
  stm_blit_printd (BDISP_BLT,
                   "  -> Generating trgt blit %s & %s\n",
                   (stdev->setup->h_trgt_sign == 1) ? "left to right" : "right to left",
                   (stdev->setup->v_trgt_sign == 1) ? "top to bottom" : "bottom to top");

  return true;
}

static void
bdisp2_setup_locations (struct stm_bdisp2_device_data * const stdev,
                        stm_blitter_rect_t            * const src2,
                        stm_blitter_rect_t            * const src1,
                        stm_blitter_rect_t            * const dst,
                        bool                           requires_spans)
{
  /* FIXME: treat src1 */
  if (stdev->setup->h_trgt_sign == -1)
    /* target/source1 from right to left */
    dst->position.x += (dst->size.w - FIXED_POINT_ONE);
  if (stdev->setup->v_trgt_sign == -1)
    /* target/source1 from bottom to top */
    dst->position.y += (dst->size.h - FIXED_POINT_ONE);
  if (stdev->setup->h_src2_sign == -1)
    /* source2 from right to left */
    src2->position.x += (src2->size.w - FIXED_POINT_ONE);
  if (stdev->setup->v_src2_sign == -1)
    /* source2 from bottom to top */
    src2->position.y += (src2->size.h - FIXED_POINT_ONE);

  if (!requires_spans)
    {
      /*
       * This is a workaround for:
       *     https://bugzilla.stlinux.com/show_bug.cgi?id=7084
       */
      if (unlikely (dst->size.w < FIXED_POINT_ONE))
        dst->size.w = FIXED_POINT_ONE;
      if (unlikely (dst->size.h < FIXED_POINT_ONE))
        {
          stm_blit_printe ("%s: dst->h %lx (%lld.%06lld) < FIXED_POINT_ONE!\n",
                           __func__, dst->size.h, INT_VALf (dst->size.h));
          dst->size.h = FIXED_POINT_ONE;
        }
      if (unlikely (src2->size.w < FIXED_POINT_ONE))
        {
          stm_blit_printe ("%s: src2->size.w %lx (%lld.%06lld) < FIXED_POINT_ONE!\n",
                           __func__, src2->size.w, INT_VALf (src2->size.w));
          src2->size.w = FIXED_POINT_ONE;
        }
      if (unlikely (src2->size.h < FIXED_POINT_ONE))
        {
          stm_blit_printe ("%s: src2->size.h %lx (%lld.%06lld) < FIXED_POINT_ONE!\n",
                           __func__, src2->size.h, INT_VALf (src2->size.h));
          src2->size.h = FIXED_POINT_ONE;
        }

      stdev->setup->ConfigTarget.BLT_TXY
        = ((FIXED_POINT_TO_INT (dst->position.y) << 16)
           | FIXED_POINT_TO_INT (dst->position.x));
      stdev->setup->ConfigTarget.BLT_TSZ_S1SZ
        = (((FIXED_POINT_TO_INT (dst->size.h) & stdev->features.hw.size_mask) << 16)
           | (FIXED_POINT_TO_INT (dst->size.w) & stdev->features.hw.size_mask));

      stdev->setup->ConfigSource1.BLT_S1XY
        = ((FIXED_POINT_TO_INT (src1->position.y) << 16)
           | FIXED_POINT_TO_INT (src1->position.x));
      stdev->setup->ConfigSource2.BLT_S2XY
        = ((FIXED_POINT_TO_INT (src2->position.y) << 16)
           | FIXED_POINT_TO_INT (src2->position.x));
      stdev->setup->ConfigSource2.BLT_S2SZ
        = (((FIXED_POINT_TO_INT (src2->size.h) & stdev->features.hw.size_mask) << 16)
           | (FIXED_POINT_TO_INT (src2->size.w) & stdev->features.hw.size_mask));

      if (stdev->setup->ConfigGeneral.BLT_CIC & BLIT_CIC_NODE_SOURCE3)
        {
          long src2_x = FIXED_POINT_TO_INT (src2->position.x
                                            / stdev->setup->blitstate.srcFactorH);
          long src2_y = FIXED_POINT_TO_INT (src2->position.y
                                            / stdev->setup->blitstate.srcFactorV) << 16;
          long src2_w = FIXED_POINT_TO_INT (src2->size.w
                                            / stdev->setup->blitstate.srcFactorH)
			  & stdev->features.hw.size_mask;
          long src2_h = (FIXED_POINT_TO_INT (src2->size.h
                                             / stdev->setup->blitstate.srcFactorV)
			  & stdev->features.hw.size_mask) << 16;

          if (unlikely (stdev->setup->blitstate.srcFactorH != 1
                        && !src2_w))
            /* 1 luma pixel could be 0.5 chroma pixels, i.e. 0. That would
               stall the BDisp. I am not sure if it is OK to make this case
               work using this workaround, or if we rather should just deny
               such blits, though. */
            src2_w = 1;
          if (unlikely (stdev->setup->blitstate.srcFactorV != 1
                        && !src2_h))
            src2_h = 1 << 16;

          stdev->setup->ConfigSource3.BLT_S3XY
            = stdev->setup->ConfigSource2.BLT_S2XY;
          stdev->setup->ConfigSource3.BLT_S3SZ
            = stdev->setup->ConfigSource2.BLT_S2SZ;

          stdev->setup->ConfigSource2.BLT_S2XY = src2_y | src2_x;
          stdev->setup->ConfigSource2.BLT_S2SZ = src2_h | src2_w;

          if ((stdev->setup->ConfigSource1.BLT_S1TY & BLIT_TY_COLOR_FORM_MASK)
              == BLIT_COLOR_FORM_YUV444P)
            stdev->setup->ConfigSource1.BLT_S1XY
              = stdev->setup->ConfigSource2.BLT_S2XY;

          BLIT_OPTIMIZE_LUMA(stdev->setup);

          stm_blit_printd (BDISP_BLT,
                           "   S3XY: %.8x S3SZ: %.8x S2XY: %.8x S2SZ: %.8x S1XY: %.8x -> TXY %.8x TSZ %.8x (RZI %.8x Y_RZI %.8x)\n",
                           stdev->setup->ConfigSource3.BLT_S3XY,
                           stdev->setup->ConfigSource3.BLT_S3SZ,
                           stdev->setup->ConfigSource2.BLT_S2XY,
                           stdev->setup->ConfigSource2.BLT_S2SZ,
                           stdev->setup->ConfigSource1.BLT_S1XY,
                           stdev->setup->ConfigTarget.BLT_TXY,
                           stdev->setup->ConfigTarget.BLT_TSZ_S1SZ,
                           stdev->setup->ConfigFiltersChr.BLT_RZI,
                           stdev->setup->ConfigFiltersLuma.BLT_Y_RZI);
        }
      else
        stm_blit_printd (BDISP_BLT,
                         "   S2XY: %.8x S2SZ: %.8x -> TXY %.8x TSZ %.8x (RZI %.8x)\n",
                         stdev->setup->ConfigSource2.BLT_S2XY,
                         stdev->setup->ConfigSource2.BLT_S2SZ,
                         stdev->setup->ConfigTarget.BLT_TXY,
                         stdev->setup->ConfigTarget.BLT_TSZ_S1SZ,
                         stdev->setup->ConfigFiltersChr.BLT_RZI);
    }
  else
    {
      /* Fill in the vertical sizes as they are constant for all spans and
         it saves us from having to find out the values again. */
      stdev->setup->ConfigTarget.BLT_TSZ_S1SZ
        = (FIXED_POINT_TO_INT (dst->size.h) & stdev->features.hw.size_mask) << 16;
      stdev->setup->ConfigSource2.BLT_S2SZ
        = (FIXED_POINT_TO_INT (src2->size.h) & stdev->features.hw.size_mask) << 16;
    }
}


static bool
bdisp_aq_flickerfilter_setup (struct stm_bdisp2_device_data * const stdev,
                              stm_blitter_rect_t       * const src,
                              stm_blitter_rect_t       * const dst)
{
  long src_h = FIXED_POINT_TO_INT(src->size.h);

  if (!stdev->setup->blitstate.flicker_filter)
  {
    stdev->setup->ConfigGeneral.BLT_CIC &= ~BLIT_CIC_NODE_FLICKER;
    stdev->setup->ConfigGeneral.BLT_INS &= ~BLIT_INS_ENABLE_FLICKERFILTER;
    stdev->setup->ConfigFilters.BLT_FCTL_RZC &= ~BLIT_RZC_FF_REPEAT_FL;
    stdev->setup->ConfigFilters.BLT_FCTL_RZC &= ~BLIT_RZC_FF_REPEAT_LL;
    return false;
  }
  else
  {
    stdev->setup->ConfigGeneral.BLT_CIC &= ~BLIT_CIC_NODE_2DFILTERSLUMA;
    stdev->setup->ConfigGeneral.BLT_CIC |= (BLIT_CIC_NODE_FILTERS
                                            | BLIT_CIC_NODE_FLICKER);
    stdev->setup->ConfigGeneral.BLT_INS |= BLIT_INS_ENABLE_FLICKERFILTER;
    stdev->setup->ConfigGeneral.BLT_INS |= BLIT_INS_ENABLE_2DRESCALE;

    stdev->setup->ConfigFilters.BLT_FCTL_RZC |= BLIT_RZC_FF_REPEAT_FL;
    stdev->setup->ConfigFilters.BLT_FCTL_RZC |= BLIT_RZC_FF_REPEAT_LL;

    if (stdev->setup->ConfigFilters.BLT_FCTL_RZC & BLIT_RZC_FF_FIELD)
    {
      if (!(src_h % 2))
        stdev->setup->ConfigFilters.BLT_FCTL_RZC &= ~BLIT_RZC_FF_REPEAT_LL;

      dst->size.h = (src_h * FIXED_POINT_ONE) / 2;
    }

    return true;
  }
}

static bool
__attribute__((const))
bdisp_aq_filter_need_spans (int      srcwidth,
                            uint16_t line_buffer_length)
{
  /* If the source width is >line_length pixels then we need to split the
     operation up into <line_length pixel vertical spans. This is because the
     filter pipeline only has line buffering for up to 128 pixels.
     In addition, even if we don't require spans for an operation we apply
     filtering and take surrounding pixels into account, similar to
     bdisp2_blit_using_spans(). This means we need additional pixels and
     therefore have to subtract 8 from the available line buffer length here,
     too. */
  return srcwidth > TO_FIXED_POINT (line_buffer_length - 8);
}

static bool
__attribute__((const))
bdisp_aq_flicker_filter_need_spans (int      srcwidth,
                                    uint16_t line_buffer_length)
{
  return srcwidth > TO_FIXED_POINT (line_buffer_length);
}

static uint32_t
__attribute__((const))
round_f16_to_f10 (uint32_t f16)
{
  f16 += (1 << 5);
  f16 &= ~0x3f;

  return f16;
}

static int
__attribute__((const))
divide_f16 (int32_t dividend, int divisor)
{
  uint64_t temp = TO_FIXED_POINT ((int64_t) dividend);

  STM_BLITTER_ASSERT (dividend >= 0);
  STM_BLITTER_ASSERT (divisor >= 0);

  return bdisp2_div64_u64 (temp, (unsigned int) divisor);
}

union _BltNodeGroup0910 {
  struct _BltNodeGroup09 *chroma;
  struct _BltNodeGroup10 *luma;
};

static void
_bdisp_aq_filtering_setup (struct stm_bdisp2_device_data * const stdev,
                           int                            hsrcinc,
                           int                            vsrcinc,
                           union _BltNodeGroup0910       * const filter)
{
  struct _BltNodeGroup09 * __restrict flt = filter->chroma;
  int                     filterIndex;

  filterIndex = CSTmBlitter__ChooseBlitter8x8FilterCoeffs (hsrcinc);
  flt->BLT_HFP = (stdev->filter_8x8_phys
                  + filterIndex * BDISP_AQ_BLIT_FILTER_8X8_TABLE_SIZE);

  filterIndex = CSTmBlitter__ChooseBlitter5x8FilterCoeffs (vsrcinc);
  flt->BLT_VFP = (stdev->filter_5x8_phys
                  + filterIndex * BDISP_AQ_BLIT_FILTER_5X8_TABLE_SIZE);

  flt->BLT_RSF  = FIXED_POINT_TO_BDISP (vsrcinc) << 16;
  flt->BLT_RSF |= FIXED_POINT_TO_BDISP (hsrcinc);

  /* Vertical filter setup: repeat the first line twice to pre-fill the
     filter. This is _not_ the same setup as used for 5 tap video filtering
     which sets the filter phase to start at +1/2. */
  if (!stdev->features.hw.spatial_dei
      && stdev->setup->blitstate.bDeInterlacingBottom)
    flt->BLT_RZI = ((3 << BLIT_RZI_V_REPEAT_SHIFT)
                     | (FIXED_POINT_TO_BDISP (FIXED_POINT_ONE / 2)
                        << BLIT_RZI_V_INIT_SHIFT));
  else
    flt->BLT_RZI = ((2 << BLIT_RZI_V_REPEAT_SHIFT)
                     | (0 << BLIT_RZI_V_INIT_SHIFT));


  /* Default horizontal filter setup, repeat the first pixel 3 times to
     pre-fill the 8 Tap filter. */
  flt->BLT_RZI |= (3 << BLIT_RZI_H_REPEAT_SHIFT);
}

/* Setup a complex blit node, with the exception of XY and Size and filter
   subposition.

   This forms a template node, potentially used to create multiple nodes,
   when doing a rescale that requires the operation to be split up into
   smaller spans. */
static bool
__attribute__((warn_unused_result))
bdisp2_stretchblit_setup (struct stm_bdisp2_device_data * const stdev,
                          unsigned int                   line_buffer_length,
                          stm_blitter_rect_t            * const src,
                          stm_blitter_rect_t            * const src1,
                          stm_blitter_rect_t            * const dst,
                          bool                          * const requires_spans)
{
  union _BltNodeGroup0910 filters;

  stdev->hsrcinc = divide_f16 (src->size.w / stdev->setup->blitstate.srcFactorH,
                               dst->size.w);
  stdev->vsrcinc = divide_f16 (src->size.h / stdev->setup->blitstate.srcFactorV,
                               dst->size.h);

  stm_blit_printd (BDISP_STRETCH,
                   "StretchBlit_setup(): vsrcinc/hsrcinc: %.8x/%.8x -> %lld.%06lld/%lld.%06lld\n",
                   stdev->vsrcinc, stdev->hsrcinc,
                   INT_VALf (stdev->vsrcinc), INT_VALf (stdev->hsrcinc));

  stdev->hsrcinc = round_f16_to_f10 (stdev->hsrcinc);
  stdev->vsrcinc = round_f16_to_f10 (stdev->vsrcinc);

  stm_blit_printd (BDISP_STRETCH,
                   "  -> rounded to                       %.8x/%.8x -> %lld.%06lld/%lld.%06lld\n",
                   stdev->vsrcinc, stdev->hsrcinc,
                   INT_VALf (stdev->vsrcinc), INT_VALf (stdev->hsrcinc));

  /* out of range */
  if ((stdev->hsrcinc > (TO_FIXED_POINT (63) + 0xffc0))
      || (stdev->vsrcinc > (TO_FIXED_POINT (63) + 0xffc0))
      || (stdev->hsrcinc < 0x40)
      || (stdev->vsrcinc < 0x40))
    return false;

  /* before setting up the resize filter, setup the flicker filter... */
  bdisp_aq_flickerfilter_setup (stdev, src, dst);

  /* ...because the resize filter setup may override the default 1:1
     resize configuration set by the flicker filter; this is fine the
     vertical and flicker filters can be active at the same time. */
  *requires_spans = bdisp_aq_filter_need_spans (src->size.w,
                                                line_buffer_length);

  stdev->setup->ConfigGeneral.BLT_CIC |= (BLIT_CIC_NODE_FILTERS
                                          | BLIT_CIC_NODE_2DFILTERSCHR);
  stdev->setup->ConfigGeneral.BLT_INS |= BLIT_INS_ENABLE_2DRESCALE;

  filters.chroma = &stdev->setup->ConfigFiltersChr;
  _bdisp_aq_filtering_setup (stdev, stdev->hsrcinc, stdev->vsrcinc, &filters);

  /* FIXME: remove this at some point soon */
  stdev->setup->ConfigFilters.BLT_FCTL_RZC &= (BLIT_RZC_HF2D_MODE_MASK
                                               | BLIT_RZC_VF2D_MODE_MASK
                                               | BLIT_RZC_Y_HF2D_MODE_MASK
                                               | BLIT_RZC_Y_VF2D_MODE_MASK);

  if (!bdisp2_setup_directions (stdev, src, src1, dst))
    return false;

  if (unlikely (stdev->setup->ConfigGeneral.BLT_CIC & BLIT_CIC_NODE_SOURCE3))
    {
      stdev->setup->ConfigGeneral.BLT_CIC |= BLIT_CIC_NODE_2DFILTERSLUMA;

      stdev->hsrcinc *= stdev->setup->blitstate.srcFactorH;
      stdev->vsrcinc *= stdev->setup->blitstate.srcFactorV;

      stm_blit_printd (BDISP_STRETCH,
                       "StretchBlit_setup(): luma vsrcinc/hsrcinc: %.8x/%.8x -> %lld.%06lld/%lld.%06lld\n",
                       stdev->vsrcinc, stdev->hsrcinc,
                       INT_VALf (stdev->vsrcinc), INT_VALf (stdev->hsrcinc));

      /* out of range */
      if ((stdev->hsrcinc > (TO_FIXED_POINT (63) + 0xffc0))
          || (stdev->vsrcinc > (TO_FIXED_POINT (63) + 0xffc0))
          || (stdev->hsrcinc < 0x40)
          || (stdev->vsrcinc < 0x40))
        return false;

      filters.luma = &stdev->setup->ConfigFiltersLuma;
      _bdisp_aq_filtering_setup (stdev, stdev->hsrcinc, stdev->vsrcinc,
                                 &filters);

      stm_blit_printd (BDISP_STRETCH,
                       "Y_RSF: %.8x (v %lld.%06lld h %lld.%06lld) Y_RZI %.8x\n",
                       stdev->setup->ConfigFiltersLuma.BLT_Y_RSF,
                       INT_VALf ((stdev->setup->ConfigFiltersLuma.BLT_Y_RSF >> 16) << 6),
                       INT_VALf ((stdev->setup->ConfigFiltersLuma.BLT_Y_RSF & 0xffff) << 6),
                       stdev->setup->ConfigFiltersLuma.BLT_Y_RZI);

      stm_blit_printd (BDISP_STRETCH,
                       "RSF: %.8x (v %lld.%06lld h %lld.%06lld) RZI %.8x\n",
                       stdev->setup->ConfigFiltersChr.BLT_RSF,
                       INT_VALf ((stdev->setup->ConfigFiltersChr.BLT_RSF >> 16) << 6),
                       INT_VALf ((stdev->setup->ConfigFiltersChr.BLT_RSF & 0xffff) << 6),
                       stdev->setup->ConfigFiltersChr.BLT_RZI);

      bdisp2_setup_locations (stdev, src, src1, dst, *requires_spans);
    }
  else
    {
      /* Vertical filter setup: repeat the first line twice to pre-fill the
         filter and set the filter phase to start at +1/2. This is the same
         setup used for 5 tap video filtering. */
      uint32_t subpixelpos /* n.16 */;
      unsigned int repeat /* int */;
      int adjustedsrcy /* int */;
      int srcspanheight /* 16.16 */;

      /* let's have bdisp2_setup_locations() only adjust src2/dst x/y
         for backwards blits, it should not set the coordinates yet, because
         we'll either do that in bdisp2_blit_using_spans() (when
         *requires_spans == true), or below. */
      bdisp2_setup_locations (stdev, src, dst, dst, true);

      stdev->setup->ConfigFiltersChr.BLT_RZI  = 0;

      srcspanheight = src->size.h;
      if (unlikely(stdev->setup->blitstate.strict_input_rect))
        {
          Get5TapFilterSetup (src->position.y,
                              src->position.y,
                              stdev->setup->blitstate.source_h,
                              stdev->setup->v_src2_sign,
                              &subpixelpos, &adjustedsrcy, &srcspanheight,
                              &repeat);
        }
      else
        {
          Get5TapFilterSetup (src->position.y,
                              (stdev->setup->v_src2_sign == 1) ? 0 : src->position.y,
                              stdev->setup->blitstate.source_h,
                              stdev->setup->v_src2_sign,
                              &subpixelpos, &adjustedsrcy, &srcspanheight,
                              &repeat);
	}
      src->position.y = TO_FIXED_POINT (adjustedsrcy);
      src->size.h = srcspanheight;
      stdev->setup->ConfigSource2.BLT_S2SZ
        = (FIXED_POINT_TO_INT (src->size.h) & stdev->features.hw.size_mask) << 16;

      if (!stdev->features.hw.spatial_dei
          && stdev->setup->blitstate.bDeInterlacingBottom)
        stdev->setup->ConfigFiltersChr.BLT_RZI
          = ((3 << BLIT_RZI_V_REPEAT_SHIFT)
             | (FIXED_POINT_TO_BDISP (FIXED_POINT_ONE / 2)
                << BLIT_RZI_V_INIT_SHIFT));
      else
        stdev->setup->ConfigFiltersChr.BLT_RZI
          = ((repeat << BLIT_RZI_V_REPEAT_SHIFT)
             | (FIXED_POINT_TO_BDISP (subpixelpos)
                << BLIT_RZI_V_INIT_SHIFT));

      if (!*requires_spans)
        {
          int adjustedsrcx /* int */;
          int srcspanwidth /* 16.16 */ = src->size.w;

          /* assume we can use the whole source surface for filter
             initialisation */
          if (unlikely(stdev->setup->blitstate.strict_input_rect))
            {
              Get8TapFilterSetup (src->position.x,
                                  src->position.x,
                                  stdev->setup->blitstate.source_w,
                                  stdev->setup->h_src2_sign,
                                  &subpixelpos,
                                  &adjustedsrcx,
                                  &srcspanwidth,
                                  &repeat);
            }
          else
            {
              Get8TapFilterSetup (src->position.x,
                                  (stdev->setup->h_src2_sign == 1) ? 0 : src->position.x,
                                  stdev->setup->blitstate.source_w,
                                  stdev->setup->h_src2_sign,
                                  &subpixelpos,
                                  &adjustedsrcx,
                                  &srcspanwidth,
                                  &repeat);
	    }

          stdev->setup->ConfigFiltersChr.BLT_RZI |= (repeat << BLIT_RZI_H_REPEAT_SHIFT);
          stdev->setup->ConfigFiltersChr.BLT_RZI |= (FIXED_POINT_TO_BDISP (subpixelpos)
                                                     << BLIT_RZI_H_INIT_SHIFT);

          stdev->setup->ConfigTarget.BLT_TXY  = (FIXED_POINT_TO_INT (dst->position.y)
	                                         & stdev->features.hw.size_mask) << 16;
          stdev->setup->ConfigTarget.BLT_TXY |= (FIXED_POINT_TO_INT (dst->position.x)
	                                         & stdev->features.hw.size_mask);

          stdev->setup->ConfigTarget.BLT_TSZ_S1SZ  = (FIXED_POINT_TO_INT (dst->size.h)
						      & stdev->features.hw.size_mask) << 16;
          stdev->setup->ConfigTarget.BLT_TSZ_S1SZ |= FIXED_POINT_TO_INT (dst->size.w)
	                                              & stdev->features.hw.size_mask;

          stdev->setup->ConfigSource1.BLT_S1XY = stdev->setup->ConfigTarget.BLT_TXY;

          stdev->setup->ConfigSource2.BLT_S2XY  = (adjustedsrcy
	                                           & stdev->features.hw.size_mask) << 16;
          stdev->setup->ConfigSource2.BLT_S2XY |= adjustedsrcx & stdev->features.hw.size_mask;
          stdev->setup->ConfigSource2.BLT_S2SZ  = (FIXED_POINT_TO_INT (srcspanheight)
	                                           & stdev->features.hw.size_mask) << 16;
          stdev->setup->ConfigSource2.BLT_S2SZ |= FIXED_POINT_TO_INT (srcspanwidth)
	                                           & stdev->features.hw.size_mask;
        }

      stm_blit_printd (BDISP_STRETCH,
                       "RSF: %.8x (v %lld.%06lld h %lld.%06lld) RZI %.8x\n",
                       stdev->setup->ConfigFiltersChr.BLT_RSF,
                       INT_VALf ((stdev->setup->ConfigFiltersChr.BLT_RSF >> 16) << 6),
                       INT_VALf ((stdev->setup->ConfigFiltersChr.BLT_RSF & 0xffff) << 6),
                       stdev->setup->ConfigFiltersChr.BLT_RZI);

      if (!*requires_spans)
        stm_blit_printd (BDISP_STRETCH,
                         "     S2XY: %.8x S2SZ: %.8x -> TXY %.8x TSZ %.8x (RZI %.8x)\n",
                         stdev->setup->ConfigSource2.BLT_S2XY,
                         stdev->setup->ConfigSource2.BLT_S2SZ,
                         stdev->setup->ConfigTarget.BLT_TXY,
                         stdev->setup->ConfigTarget.BLT_TSZ_S1SZ,
                         stdev->setup->ConfigFiltersChr.BLT_RZI);
    }

  return true;
}

/* Setup a complex blit node, with the exception of XY and Size and filter
   subposition.

   This forms a template node, potentially used to create multiple nodes,
   when doing a rescale that requires the operation to be split up into
   smaller spans. */
static bool
__attribute__((warn_unused_result))
bdisp2_blit_setup (struct stm_bdisp2_device_data * const stdev,
                   uint16_t                       line_buffer_length,
                   stm_blitter_rect_t            * const src2,
                   stm_blitter_rect_t            * const src1,
                   stm_blitter_rect_t            * const dst,
                   bool                          * const requires_spans)
{
    if (unlikely (bdisp_aq_flickerfilter_setup (stdev, src2, dst)))
    {
      *requires_spans = bdisp_aq_flicker_filter_need_spans (src2->size.w,
                                                            line_buffer_length);
    }
    else
    {
      /* we might have done a stretch blit before and do a normal blit now,
         in which case make sure to disable the rescaling engine. In the
         above case w/ the enabled flicker filter, this doesn't matter as the
         rescaling factors are reset to 1 anyway. */
      stdev->setup->ConfigGeneral.BLT_CIC &= ~(BLIT_CIC_NODE_2DFILTERSCHR
                                               | BLIT_CIC_NODE_2DFILTERSLUMA);
      if (!(stdev->setup->ConfigGeneral.BLT_INS & BLIT_INS_ENABLE_PLANEMASK))
        stdev->setup->ConfigGeneral.BLT_CIC &= ~BLIT_CIC_NODE_FILTERS;
      stdev->setup->ConfigGeneral.BLT_INS &= ~BLIT_INS_ENABLE_2DRESCALE;

      *requires_spans = false;
    }

  if (!bdisp2_setup_directions (stdev, src2, src1, dst))
    return false;

  bdisp2_setup_locations (stdev, src2, src1, dst, *requires_spans);

  return true;
}




typedef struct _BDispAqNodeState
{
  stm_blitter_palette_t        *palette;
  enum bdisp2_palette_type      palette_type;
  struct _BltNodeGroup00        ConfigGeneral;
  struct _BltNodeGroup03        ConfigSource1;
} BDispAqNodeState;

static void
bdisp_aq_second_node_prepare (struct stm_bdisp2_driver_data * const stdrv,
                              struct stm_bdisp2_device_data * const stdev,
                              BDispAqNodeState              * const state)
{
  STM_BLITTER_ASSERT (state != NULL);

  state->ConfigGeneral = stdev->setup->ConfigGeneral;
  stdev->setup->ConfigGeneral
    = stdev->setup->blitstate.extra_passes[0].ConfigGeneral;

  state->palette      = stdev->setup->palette;
  state->palette_type = stdev->setup->palette_type;

  stdev->setup->palette_type
    = stdev->setup->blitstate.extra_passes[0].palette_type;
  if (stdev->setup->palette_type != SG2C_NORMAL)
    stdev->setup->palette = NULL;

  bdisp_aq_setup_blit_operation (stdrv, stdev);
}

static void
bdisp_aq_second_node_prepare_2 (struct stm_bdisp2_driver_data * const stdrv,
                                struct stm_bdisp2_device_data * const stdev,
                                BDispAqNodeState              * const state)
{
  STM_BLITTER_ASSERT (state != NULL);

  state->ConfigSource1 = stdev->setup->ConfigSource1;
  stdev->setup->ConfigSource1.BLT_S1BA = stdev->setup->ConfigTarget.BLT_TBA;
  stdev->setup->ConfigSource1.BLT_S1XY = stdev->setup->ConfigTarget.BLT_TXY;
  stdev->setup->ConfigSource1.BLT_S1TY = stdev->setup->ConfigTarget.BLT_TTY;

  bdisp_aq_second_node_prepare (stdrv, stdev, state);
}

static void
bdisp_aq_third_node_prepare (struct stm_bdisp2_driver_data * const stdrv,
                             struct stm_bdisp2_device_data * const stdev)
{
  stdev->setup->ConfigGeneral
    = stdev->setup->blitstate.extra_passes[1].ConfigGeneral;

  stdev->setup->palette_type
    = stdev->setup->blitstate.extra_passes[1].palette_type;
  if (stdev->setup->palette_type != SG2C_NORMAL)
    stdev->setup->palette = NULL;

  bdisp_aq_setup_blit_operation (stdrv, stdev);
}

static void
bdisp_aq_second_node_release (struct stm_bdisp2_driver_data * const stdrv,
                              struct stm_bdisp2_device_data * const stdev,
                              BDispAqNodeState              * const state)
{
  STM_BLITTER_ASSERT (state != NULL);

  /* restore */
  stdev->setup->ConfigGeneral = state->ConfigGeneral;
  stdev->setup->palette = state->palette;
  stdev->setup->palette_type = state->palette_type;

  bdisp_aq_setup_blit_operation (stdrv, stdev);
}

static void
bdisp_aq_second_node_release_2 (struct stm_bdisp2_driver_data * const stdrv,
                                struct stm_bdisp2_device_data * const stdev,
                                BDispAqNodeState              * const state)
{
  STM_BLITTER_ASSERT (state != NULL);

  /* restore */
  stdev->setup->ConfigSource1 = state->ConfigSource1;

  bdisp_aq_second_node_release (stdrv, stdev, state);
}


static void
get_size_constraints (const struct stm_bdisp2_device_data * const stdev,
                      long                                * const largest,
                      long                                * const smallest)
{
  if (stdev->features.hw.size_4k)
    {
      *largest = 8191;
      *smallest = -8192;
    }
  else
    {
      *largest = 4095;
      *smallest = -4096;
    }
}

static int
check_size_constraints (const struct stm_bdisp2_device_data * const stdev,
                        long x,
                        long y,
                        long w,
                        long h)
{
  long largest, smallest;

  get_size_constraints (stdev, &largest, &smallest);

  if (unlikely (x < smallest || y < smallest))
    return -EINVAL;
  if (unlikely (x > largest || y > largest))
    return -EFBIG;
  if (unlikely (w < 1 || h < 1))
    return -EINVAL;
  if (unlikely (w > largest || h > largest))
    return -EFBIG;

  return 0;
}

static int
check_size_constraints_f16 (const struct stm_bdisp2_device_data * const stdev,
                            long x,
                            long y,
                            long w,
                            long h)
{
  long largest, smallest;

  get_size_constraints (stdev, &largest, &smallest);
  largest = TO_FIXED_POINT (largest);
  smallest = TO_FIXED_POINT (smallest);

  if (unlikely (x < smallest || y < smallest))
    return -EINVAL;
  if (unlikely (x > largest || y > largest))
    return -EFBIG;
  if (unlikely (w <= 0 || h <= 0))
    return -EINVAL;
  if (unlikely (w > largest || h > largest))
    return -EFBIG;

  return 0;
}


int
bdisp2_fence (struct stm_bdisp2_driver_data * const stdrv,
              struct stm_bdisp2_device_data * const stdev)
{
  stm_blit_printd (BDISP_BLT, "%s\n", __func__);
  DUMP_INFO ();

  if(bdisp2_is_suspended(stdrv))
    return -EAGAIN;

  {
  struct _BltNode_Fast * const node = bdisp2_get_new_node (stdrv, stdev);
  uint32_t blt_ins = (BLIT_INS_SRC1_MODE_COLOR_FILL
                    | BLIT_INS_ENABLE_RECTCLIP
                    | BLIT_INS_ENABLE_BLITCOMPIRQ);

  bdisp2_update_num_ops (stdrv, stdev, &blt_ins);

  node->common.ConfigTarget.BLT_TBA = stdev->setup->ConfigTarget.BLT_TBA;

  node->common.ConfigTarget.BLT_TTY = (stdev->setup->ConfigTarget.BLT_TTY
                                       & BLIT_TY_PIXMAP_PITCH_MASK);
  node->common.ConfigTarget.BLT_TTY |= BLIT_COLOR_FORM_A8;

  /* source1 == destination */
  node->common.ConfigSource1.BLT_S1TY = BLIT_COLOR_FORM_A8 | 1;
  node->common.ConfigColor.BLT_S1CF   = 0x00;

  node->common.ConfigGeneral.BLT_CIC = BLIT_CIC_BLT_NODE_FAST;
  node->common.ConfigGeneral.BLT_INS = blt_ins;
  node->common.ConfigGeneral.BLT_ACK = BLIT_ACK_BYPASSSOURCE1;

  node->common.ConfigSource1.BLT_S1XY = node->common.ConfigTarget.BLT_TXY;

  /* x/y: -1/-1 is outside the area of the surface and nothing will be drawn */
  node->common.ConfigTarget.BLT_TXY      = ((int16_t)-1 << 16) | (int16_t)-1;
  node->common.ConfigTarget.BLT_TSZ_S1SZ = (1 << 16) | 1;

  bdisp2_update_config_static (stdrv->bdisp_shared, &node->ConfigStatic);

  bdisp2_finish_node (stdrv, stdev, node);
  }

  return 0;
}


int
bdisp2_fill_draw_nop (struct stm_bdisp2_driver_data       * const stdrv,
                      const struct stm_bdisp2_device_data * const stdev,
                      const stm_blitter_rect_t            * const dst)
{
  stm_blit_printd (BDISP_BLT,
                   "%s (%lld, %lld - %lldx%lld)\n", __func__,
                   RECTANGLE_VALS (dst));
  DUMP_INFO ();

  if(bdisp2_is_suspended(stdrv))
    return -EAGAIN;

  return 0;
}

int
bdisp2_fill_rect_simple (struct stm_bdisp2_driver_data       * const stdrv,
                         const struct stm_bdisp2_device_data * const stdev,
                         const stm_blitter_rect_t            * const dst)
{
  stm_blit_printd (BDISP_BLT, "%s (%lld, %lld - %lldx%lld)\n", __func__,
                   RECTANGLE_VALS (dst));
  DUMP_INFO ();

  STM_BLITTER_ASSERT (stdev->setup->n_target_extra_loops == 0);

  if (unlikely (!dst->size.w || !dst->size.h))
    return 0;
  if (check_size_constraints (stdev, dst->position.x, dst->position.y,
                              dst->size.w, dst->size.h))
    return -EINVAL;
  if (BLIT_TY_COLOR_FORM_IS_UP_SAMPLED (stdev->setup->ConfigTarget.BLT_TTY)
      &&((stdev->setup->upsampled_dst_nbpix_min > dst->size.w)
         || (stdev->setup->upsampled_dst_nbpix_min > dst->size.h)))
    return -EOPNOTSUPP;

  if(bdisp2_is_suspended(stdrv))
    return -EAGAIN;

  {
  struct _BltNode_Fast * const node
    = bdisp2_get_new_node_prepared_simple (stdrv, stdev);
  /* dimensions */
  node->common.ConfigTarget.BLT_TXY
    = (dst->position.y << 16) | dst->position.x;
  node->common.ConfigTarget.BLT_TSZ_S1SZ
    = (dst->size.h << 16) | dst->size.w;

  bdisp2_finish_node (stdrv, stdev, node);
  }

  return 0;
}

int
bdisp2_fill_rect (struct stm_bdisp2_driver_data       * const stdrv,
                  const struct stm_bdisp2_device_data * const stdev,
                  const stm_blitter_rect_t            * const dst)
{
  unsigned int i;

  stm_blit_printd (BDISP_BLT, "%s (%lld, %lld - %lldx%lld)\n", __func__,
                   RECTANGLE_VALS (dst));
  DUMP_INFO ();

  if (unlikely (!dst->size.w || !dst->size.h))
    return 0;
  if (check_size_constraints (stdev, dst->position.x, dst->position.y,
                              dst->size.w, dst->size.h))
    return -EINVAL;
  if (BLIT_TY_COLOR_FORM_IS_UP_SAMPLED (stdev->setup->ConfigTarget.BLT_TTY)
      &&((stdev->setup->upsampled_dst_nbpix_min > dst->size.w)
         || (stdev->setup->upsampled_dst_nbpix_min > dst->size.h)))
    return -EOPNOTSUPP;

  for (i = 0; i < stdev->setup->n_target_extra_loops; ++i)
    {
      if (check_size_constraints (stdev,
                                  dst->position.x / stdev->setup->target_loop[i].coordinate_divider.h,
                                  dst->position.y / stdev->setup->target_loop[i].coordinate_divider.v,
                                  dst->size.w / stdev->setup->target_loop[i].coordinate_divider.h,
                                  dst->size.h / stdev->setup->target_loop[i].coordinate_divider.v))
        return -EINVAL;
    }

  if(bdisp2_is_suspended(stdrv))
    return -EAGAIN;

  {
  struct _BltNode_FullFill * const node
    = bdisp2_get_new_node_prepared_slow (stdrv, stdev);
  /* dimensions */
  node->common.ConfigTarget.BLT_TXY
    = node->common.ConfigSource1.BLT_S1XY
    = node->ConfigSource2.BLT_S2XY
    = (dst->position.y << 16) | dst->position.x;
  node->common.ConfigTarget.BLT_TSZ_S1SZ
    = node->ConfigSource2.BLT_S2SZ
    = (dst->size.h << 16) | dst->size.w;

  bdisp2_finish_node (stdrv, stdev, node);
  }

  for (i = 0; i < stdev->setup->n_target_extra_loops; ++i)
    {
      /* This is an operation that needs additional passes for split buffer
         destinations. */
      struct _BltNode_FullFill * const node
        = bdisp2_get_new_node_prepared_slow (stdrv, stdev);
      node->common.ConfigTarget.BLT_TBA = stdev->setup->target_loop[i].BLT_TBA;
      node->common.ConfigTarget.BLT_TTY = stdev->setup->target_loop[i].BLT_TTY;

      node->common.ConfigSource1.BLT_S1BA = stdev->setup->target_loop[i].BLT_TBA;
      node->common.ConfigSource1.BLT_S1TY
        = (stdev->setup->target_loop[i].BLT_TTY
           & ~(BLIT_TTY_CHROMA_NOTLUMA | BLIT_TTY_CR_NOT_CB));

      /* dimensions */
      node->common.ConfigTarget.BLT_TXY
        = node->common.ConfigSource1.BLT_S1XY
        = node->ConfigSource2.BLT_S2XY
        = (((dst->position.y
             / stdev->setup->target_loop[i].coordinate_divider.v) << 16)
           | (dst->position.x
              / stdev->setup->target_loop[i].coordinate_divider.h));
      node->common.ConfigTarget.BLT_TSZ_S1SZ
        = node->ConfigSource2.BLT_S2SZ
        = (((dst->size.h
             / stdev->setup->target_loop[i].coordinate_divider.v) << 16)
           | (dst->size.w
              / stdev->setup->target_loop[i].coordinate_divider.h));

      bdisp2_finish_node (stdrv, stdev, node);
    }

  return 0;
}

int
bdisp2_draw_rect_simple (struct stm_bdisp2_driver_data       * const stdrv,
                         const struct stm_bdisp2_device_data * const stdev,
                         const stm_blitter_rect_t            * const dst)
{
  struct _BltNode_Fast *node;

  stm_blit_printd (BDISP_BLT, "%s (%lld, %lld - %lldx%lld)\n", __func__,
                   RECTANGLE_VALS (dst));
  DUMP_INFO ();

  if (unlikely (!dst->size.w || !dst->size.h))
    return 0;
  if (check_size_constraints (stdev, dst->position.x, dst->position.y,
                              dst->size.w, dst->size.h))
    return -EINVAL;
  if (stdev->features.hw.upsampled_nbpix_min)
    return -EOPNOTSUPP;

  if(bdisp2_is_suspended(stdrv))
    return -EAGAIN;

  if (unlikely (dst->size.h == 2 || dst->size.w == 2))
    return bdisp2_fill_rect_simple (stdrv, stdev, dst);

  /* this is optimized for w >= h, we could optimize this for the case w < h,
     or maybe do it in a more generic way. but we don't care. */
  {
  /* top line */
  node = bdisp2_get_new_node_prepared_simple (stdrv, stdev);
  node->common.ConfigTarget.BLT_TXY
    = (dst->position.y << 16) | dst->position.x;
  node->common.ConfigTarget.BLT_TSZ_S1SZ = (1 << 16) | dst->size.w;
  bdisp2_finish_node (stdrv, stdev, node);
  }

  if (likely (dst->size.h > 1))
    {
      long y2 = dst->position.y + 1;
      long h2 = dst->size.h - 2;

      /* left line */
      node = bdisp2_get_new_node_prepared_simple (stdrv, stdev);
      node->common.ConfigTarget.BLT_TXY
        = (y2 << 16) | dst->position.x;
      node->common.ConfigTarget.BLT_TSZ_S1SZ = (h2 << 16) | 1;
      bdisp2_finish_node (stdrv, stdev, node);

      if (likely (dst->size.w > 1))
        {
          /* right line */
          node = bdisp2_get_new_node_prepared_simple (stdrv, stdev);
          node->common.ConfigTarget.BLT_TXY
            = (y2 << 16) | (dst->position.x + dst->size.w - 1);
          node->common.ConfigTarget.BLT_TSZ_S1SZ = (h2 << 16) | 1;
          bdisp2_finish_node (stdrv, stdev, node);
        }

      /* bottom line */
      node = bdisp2_get_new_node_prepared_simple (stdrv, stdev);
      node->common.ConfigTarget.BLT_TXY
        = ((dst->position.y + dst->size.h - 1) << 16) | dst->position.x;
      node->common.ConfigTarget.BLT_TSZ_S1SZ = (1 << 16) | dst->size.w;
      bdisp2_finish_node (stdrv, stdev, node);
    }

  return 0;
}

int
bdisp2_draw_rect (struct stm_bdisp2_driver_data       * const stdrv,
                  const struct stm_bdisp2_device_data * const stdev,
                  const stm_blitter_rect_t            * const dst)
{
  stm_blitter_rect_t _dst;
  int result;

  stm_blit_printd (BDISP_BLT, "%s (%lld, %lld - %lldx%lld)\n", __func__,
                   RECTANGLE_VALS (dst));
  DUMP_INFO ();

  if (unlikely (!dst->size.w || !dst->size.h))
    return 0;
  if (check_size_constraints (stdev, dst->position.x, dst->position.y,
                              dst->size.w, dst->size.h))
    return -EINVAL;
  if (stdev->features.hw.upsampled_nbpix_min)
    return -EOPNOTSUPP;

  if(bdisp2_is_suspended(stdrv))
    return -EAGAIN;

  if (unlikely (dst->size.h == 2 || dst->size.w == 2))
    return bdisp2_fill_rect (stdrv, stdev, dst);

  /* FIXME: instead of calling bdisp2_get_new_node_prepared_slow() multiple
     times, we could probably just memcpy() the first one, but then, the
     source memory would be uncached, need to check which method is actually
     faster... */

  /* this is optimized for w >= h, we could optimize this for the case w < h,
     or maybe do it in a more generic way. but we don't care. */

  /* top line */
  _dst.size     = (stm_blitter_dimension_t) { .w = dst->size.w,
                                              .h = 1 };
  _dst.position = (stm_blitter_point_t) { .x = dst->position.x,
                                          .y = dst->position.y };
  result = bdisp2_fill_rect(stdrv, stdev, &_dst);

  if (likely (dst->size.h > 1))
    {
      /* left line */
      _dst.size     = (stm_blitter_dimension_t) { .w = 1,
                                                  .h = dst->size.h - 2 };
      _dst.position = (stm_blitter_point_t) { .x = dst->position.x,
                                              .y = dst->position.y + 1 };
      result |= bdisp2_fill_rect(stdrv, stdev, &_dst);

      if (likely (dst->size.w > 1))
        {
          /* right line */
          _dst.size     = (stm_blitter_dimension_t) { .w = 1,
                                                      .h = dst->size.h - 2 };
          _dst.position = (stm_blitter_point_t) { .x = dst->position.x
                                                       + dst->size.w - 1,
                                                  .y = dst->position.y + 1};
          result |= bdisp2_fill_rect(stdrv, stdev, &_dst);
        }

      /* bottom line */
      _dst.size     = (stm_blitter_dimension_t) { .w = dst->size.w, .h = 1 };
      _dst.position = (stm_blitter_point_t) { .x = dst->position.x,
                                              .y = dst->position.y
                                                   + dst->size.h - 1};
      result |= bdisp2_fill_rect(stdrv, stdev, &_dst);
    }

    return result;
}


static int
bdisp2_blit_simple_full (struct stm_bdisp2_driver_data * const stdrv,
                         struct stm_bdisp2_device_data * const stdev,
                         const stm_blitter_rect_t      * const _src,
                         const stm_blitter_point_t     * const _dst_pt,
                         uint32_t                        blt_ins,
                         uint32_t                        s1ty,
                         uint32_t                        tty)
{
  stm_blitter_rect_t src;
  stm_blitter_point_t dst_pt;
  struct _BltNode_Fast *node;
  bool     copy_backwards;
  uint32_t blt_cic;

  STM_BLITTER_ASSERT (_src != NULL);
  STM_BLITTER_ASSERT (_dst_pt != NULL);

  src = *_src;
  dst_pt = *_dst_pt;

  if (!stdev->setup->blitstate.bFixedPoint)
    {
      if (stdev->setup->blitstate.srcxy_fixed_point)
        {
          src.position.x /= FIXED_POINT_ONE;
          src.position.y /= FIXED_POINT_ONE;
        }
      stm_blit_printd (BDISP_BLT, "%s (%lld,%lld-%lldx%lld -> %ld,%ld)\n",
                       __func__,
                       RECTANGLE_VALS (&src), dst_pt.x, dst_pt.y);
    }
  else
    {
      stm_blit_printd (BDISP_BLT,
                       "%s (%lld.%06lld,%lld.%06lld-%lld.%06lldx%lld.%06lld -> %lld.%06lld,%lld.%06lld)\n",
                       __func__,
                       RECTANGLE_VALSf (&src),
                       INT_VALf (dst_pt.x), INT_VALf (dst_pt.y));
      src.position.x /= FIXED_POINT_ONE;
      src.position.y /= FIXED_POINT_ONE;
      src.size.w /= FIXED_POINT_ONE;
      src.size.h /= FIXED_POINT_ONE;
      dst_pt.x /= FIXED_POINT_ONE;
      dst_pt.y /= FIXED_POINT_ONE;
    }
  DUMP_INFO ();

  if (unlikely (!src.size.w || !src.size.h))
    return 0;
  if (check_size_constraints (stdev, src.position.x, src.position.y,
                              src.size.w, src.size.h))
    return -EINVAL;
  if (check_size_constraints (stdev, dst_pt.x, dst_pt.y, 1, 1))
    return -EINVAL;
  if (BLIT_TY_COLOR_FORM_IS_UP_SAMPLED (stdev->setup->ConfigSource2.BLT_S2TY)
      &&((stdev->setup->upsampled_src_nbpix_min > src.size.w)
	 || (stdev->setup->upsampled_src_nbpix_min > src.size.h)))
    return -EOPNOTSUPP;

  if(bdisp2_is_suspended(stdrv))
    return -EAGAIN;

  node = bdisp2_get_new_node (stdrv, stdev);
  blt_cic = BLIT_CIC_BLT_NODE_FAST;

  bdisp2_update_num_ops (stdrv, stdev, &blt_ins);

  /* target */
  tty = bdisp_ty_sanitise_direction (tty);
  node->common.ConfigTarget.BLT_TBA = stdev->setup->ConfigTarget.BLT_TBA;

  /* source (taken from source2 setup) */
  node->common.ConfigSource1.BLT_S1BA = stdev->setup->ConfigSource2.BLT_S2BA;
  s1ty = bdisp_ty_sanitise_direction (s1ty);

  STM_BLITTER_ASSUME ((s1ty & BLIT_TY_COLOR_FORM_MASK)
                      == (tty & BLIT_TY_COLOR_FORM_MASK));

  /* work out copy direction */
  copy_backwards = false;
  if (unlikely ((stdev->setup->ConfigSource2.BLT_S2BA
                 == stdev->setup->ConfigTarget.BLT_TBA)
                && (dst_pt.y > src.position.y
                    || ((dst_pt.y == src.position.y)
                        && (dst_pt.x > src.position.x)))))
    {
      stm_blitter_rect_t d_rect = { .position = dst_pt,
                                    .size = src.size };
      copy_backwards = bdisp2_rectangles_intersect (&src, &d_rect);
    }

  /* and set up coordinates & size */
  if (!copy_backwards)
    {
      node->common.ConfigSource1.BLT_S1XY = ((src.position.y << 16)
                                             | src.position.x);
      node->common.ConfigTarget.BLT_TXY = (dst_pt.y << 16) | dst_pt.x;
    }
  else
    {
      s1ty |= (BLIT_TY_COPYDIR_BOTTOMTOP | BLIT_TY_COPYDIR_RIGHTLEFT);
      tty  |= (BLIT_TY_COPYDIR_BOTTOMTOP | BLIT_TY_COPYDIR_RIGHTLEFT);

      node->common.ConfigSource1.BLT_S1XY
        = (((src.position.y + src.size.h - 1) << 16)
           | (src.position.x + src.size.w - 1));
      node->common.ConfigTarget.BLT_TXY
        = ((dst_pt.y + src.size.h - 1) << 16) | (dst_pt.x + src.size.w - 1);
    }

  node->common.ConfigTarget.BLT_TTY   = tty;
  node->common.ConfigSource1.BLT_S1TY = s1ty;

  node->common.ConfigTarget.BLT_TSZ_S1SZ = (src.size.h << 16) | src.size.w;

  node->common.ConfigGeneral.BLT_CIC = blt_cic;
  node->common.ConfigGeneral.BLT_INS = blt_ins;
  /* BLT_ACK is enabling only src1 */
  node->common.ConfigGeneral.BLT_ACK = BLIT_ACK_BYPASSSOURCE1;

  bdisp2_update_config_static (stdrv->bdisp_shared, &node->ConfigStatic);

  bdisp2_finish_node (stdrv, stdev, node);

  return 0;
}

int
bdisp2_blit_simple (struct stm_bdisp2_driver_data * const stdrv,
                    struct stm_bdisp2_device_data * const stdev,
                    const stm_blitter_rect_t      * const _src,
                    const stm_blitter_point_t     * const _dst_pt)
{
  STM_BLITTER_ASSUME (
     (stdev->setup->ConfigSource2.BLT_S2TY & (BLIT_TY_COLOR_FORM_MASK
                                              | BLIT_TY_FULL_ALPHA_RANGE
                                              | BLIT_TY_BIG_ENDIAN))
     == (stdev->setup->ConfigTarget.BLT_TTY & (BLIT_TY_COLOR_FORM_MASK
                                               | BLIT_TY_FULL_ALPHA_RANGE
                                               | BLIT_TY_BIG_ENDIAN)));

  return bdisp2_blit_simple_full (stdrv, stdev, _src, _dst_pt,
                                  BLIT_INS_SRC1_MODE_DIRECT_COPY,
                                  (stdev->setup->ConfigSource2.BLT_S2TY
                                   & ~BLIT_STY_COLOR_EXPAND_MASK),
                                  stdev->setup->ConfigTarget.BLT_TTY);
}

int
bdisp2_blit_simple_YCbCr422r (struct stm_bdisp2_driver_data * const stdrv,
                              struct stm_bdisp2_device_data * const stdev,
                              const stm_blitter_rect_t      * const src,
                              const stm_blitter_point_t     * const dst_pt)
{
  uint32_t tty = stdev->setup->ConfigTarget.BLT_TTY;
  uint32_t s2ty = stdev->setup->ConfigSource2.BLT_S2TY;

  tty &= ~(BLIT_TY_COLOR_FORM_MASK
           | BLIT_TY_FULL_ALPHA_RANGE
           | BLIT_TY_BIG_ENDIAN);
  tty |= BLIT_COLOR_FORM_RGB565;

  s2ty &= ~(BLIT_TY_COLOR_FORM_MASK
            | BLIT_TY_FULL_ALPHA_RANGE
            | BLIT_TY_BIG_ENDIAN);
  s2ty |= BLIT_COLOR_FORM_RGB565;

  return bdisp2_blit_simple_full (stdrv, stdev, src, dst_pt,
                                  BLIT_INS_SRC1_MODE_DIRECT_COPY,
                                  s2ty, tty);
}

int
bdisp2_blit_swap_YCbCr422r (struct stm_bdisp2_driver_data * const stdrv,
                            struct stm_bdisp2_device_data * const stdev,
                            const stm_blitter_rect_t      * const src,
                            const stm_blitter_point_t     * const dst_pt)
{
  uint32_t tty = stdev->setup->ConfigTarget.BLT_TTY;
  uint32_t s2ty = stdev->setup->ConfigSource2.BLT_S2TY;

  tty &= ~(BLIT_TY_COLOR_FORM_MASK
           | BLIT_TY_FULL_ALPHA_RANGE
           | BLIT_TY_BIG_ENDIAN
           | BLIT_TTY_ENABLE_DITHER);
  tty |= BLIT_COLOR_FORM_RGB565;

  s2ty &= ~(BLIT_TY_COLOR_FORM_MASK
            | BLIT_TY_FULL_ALPHA_RANGE
            | BLIT_TY_BIG_ENDIAN
            | BLIT_STY_COLOR_EXPAND_MASK);
  s2ty |= (BLIT_COLOR_FORM_RGB565
           | BLIT_TY_BIG_ENDIAN
           | BLIT_STY_COLOR_EXPAND_ZEROS);

  return bdisp2_blit_simple_full (stdrv, stdev, src, dst_pt,
                                  BLIT_INS_SRC1_MODE_MEMORY,
                                  s2ty, tty);
}

/* simple copy of planar formats is done via 2 (or 3) nodes and fast path
   BYTE copy */
static int
bdisp2_blit_simple_NV42x (struct stm_bdisp2_driver_data * const stdrv,
                          struct stm_bdisp2_device_data * const stdev,
                          const stm_blitter_rect_t      * const src_y,
                          const stm_blitter_point_t     * const dst_pt_y,
                          stm_blitter_rect_t            * const src_cbcr,
                          stm_blitter_point_t           * const dst_pt_cbcr)
{
  uint32_t tty = stdev->setup->ConfigTarget.BLT_TTY;
  uint32_t tba = stdev->setup->ConfigTarget.BLT_TBA;
  uint32_t s2ty = stdev->setup->ConfigSource2.BLT_S2TY;
  uint32_t s2ba = stdev->setup->ConfigSource2.BLT_S2BA;

  int result;

  /* a Cb/Cr pair starts on even boundaries */
  if (!stdev->setup->blitstate.bFixedPoint)
    {
      if (stdev->setup->blitstate.srcxy_fixed_point)
        src_cbcr->position.x &= 0xfffe0000;
      else
        src_cbcr->position.x &= ~0x00000001;
      dst_pt_cbcr->x &= ~0x00000001;
    }
  else
    {
      src_cbcr->position.x &= 0xfffe0000;
      dst_pt_cbcr->x &= 0xfffe0000;
    }

  stdev->setup->ConfigTarget.BLT_TTY &= ~(BLIT_TY_COLOR_FORM_MASK
                                          | BLIT_TY_FULL_ALPHA_RANGE
                                          | BLIT_TY_BIG_ENDIAN);
  stdev->setup->ConfigTarget.BLT_TTY |= BLIT_COLOR_FORM_BYTE;
  stdev->setup->ConfigSource2.BLT_S2TY &= ~(BLIT_TY_COLOR_FORM_MASK
                                            | BLIT_TY_FULL_ALPHA_RANGE
                                            | BLIT_TY_BIG_ENDIAN);
  stdev->setup->ConfigSource2.BLT_S2TY |= BLIT_COLOR_FORM_BYTE;

  /* copy Y */
  stdev->setup->ConfigSource2.BLT_S2BA = stdev->setup->ConfigSource3.BLT_S3BA;
  result = bdisp2_blit_simple (stdrv, stdev, src_y, dst_pt_y);

  /* copy Cb/Cr */
  stdev->setup->ConfigSource2.BLT_S2BA = s2ba;
  stdev->setup->ConfigTarget.BLT_TBA = stdev->setup->target_loop[0].BLT_TBA;
  result |= bdisp2_blit_simple (stdrv, stdev, src_cbcr, dst_pt_cbcr);

  /* restore */
  stdev->setup->ConfigSource2.BLT_S2BA = s2ba;
  stdev->setup->ConfigSource2.BLT_S2TY = s2ty;
  stdev->setup->ConfigTarget.BLT_TBA = tba;
  stdev->setup->ConfigTarget.BLT_TTY = tty;

  return result;
}

int
bdisp2_blit_simple_NV422 (struct stm_bdisp2_driver_data * const stdrv,
                          struct stm_bdisp2_device_data * const stdev,
                          const stm_blitter_rect_t      * const src,
                          const stm_blitter_point_t     * const dst_pt)
{
  stm_blitter_rect_t src_cbcr = *src;
  stm_blitter_point_t dst_pt_cbcr = *dst_pt;

  return bdisp2_blit_simple_NV42x(stdrv, stdev,
                                  src, dst_pt, &src_cbcr, &dst_pt_cbcr);
}

int
bdisp2_blit_simple_NV420 (struct stm_bdisp2_driver_data * const stdrv,
                          struct stm_bdisp2_device_data * const stdev,
                          const stm_blitter_rect_t      * const src,
                          const stm_blitter_point_t     * const dst_pt)
{
  stm_blitter_rect_t src_cbcr = *src;
  stm_blitter_point_t dst_pt_cbcr = *dst_pt;

  src_cbcr.position.y /= 2;
  src_cbcr.size.h /= 2;
  dst_pt_cbcr.y /= 2;

  return bdisp2_blit_simple_NV42x(stdrv, stdev,
                                  src, dst_pt, &src_cbcr, &dst_pt_cbcr);
}

static int
bdisp2_blit_simple_YV4xx (struct stm_bdisp2_driver_data * const stdrv,
                          struct stm_bdisp2_device_data * const stdev,
                          const stm_blitter_rect_t      * const src_y,
                          const stm_blitter_point_t     * const dst_pt_y,
                          const stm_blitter_rect_t      * const src_cb,
                          const stm_blitter_point_t     * const dst_pt_cb,
                          const stm_blitter_rect_t      * const src_cr,
                          const stm_blitter_point_t     * const dst_pt_cr,
                          const int                       pitch_divider)
{
  uint32_t tty = stdev->setup->ConfigTarget.BLT_TTY;
  uint32_t tba = stdev->setup->ConfigTarget.BLT_TBA;
  uint32_t s2ty = stdev->setup->ConfigSource2.BLT_S2TY;
  uint32_t s2ba = stdev->setup->ConfigSource2.BLT_S2BA;

  int result;

  stdev->setup->ConfigTarget.BLT_TTY &= ~(BLIT_TY_COLOR_FORM_MASK
                                          | BLIT_TY_FULL_ALPHA_RANGE
                                          | BLIT_TY_BIG_ENDIAN);
  stdev->setup->ConfigTarget.BLT_TTY |= BLIT_COLOR_FORM_BYTE;
  stdev->setup->ConfigSource2.BLT_S2TY = stdev->setup->ConfigSource3.BLT_S3TY;
  stdev->setup->ConfigSource2.BLT_S2TY &= ~(BLIT_TY_COLOR_FORM_MASK
                                            | BLIT_TY_FULL_ALPHA_RANGE
                                            | BLIT_TY_BIG_ENDIAN);
  stdev->setup->ConfigSource2.BLT_S2TY |= BLIT_COLOR_FORM_BYTE;

  /* copy Y */
  stdev->setup->ConfigSource2.BLT_S2BA = stdev->setup->ConfigSource3.BLT_S3BA;
  result = bdisp2_blit_simple (stdrv, stdev, src_y, dst_pt_y);

  /* Update pitch for chroma */
  stdev->setup->ConfigTarget.BLT_TTY
    = (stdev->setup->ConfigTarget.BLT_TTY & ~BLIT_TY_PIXMAP_PITCH_MASK)
      | ((stdev->setup->ConfigTarget.BLT_TTY & BLIT_TY_PIXMAP_PITCH_MASK)
         /pitch_divider);
  stdev->setup->ConfigSource2.BLT_S2TY
    = (stdev->setup->ConfigSource2.BLT_S2TY & ~BLIT_TY_PIXMAP_PITCH_MASK)
      | ((stdev->setup->ConfigSource2.BLT_S2TY & BLIT_TY_PIXMAP_PITCH_MASK)
         /pitch_divider);

  /* copy Cb */
  stdev->setup->ConfigSource2.BLT_S2BA = s2ba;
  stdev->setup->ConfigTarget.BLT_TBA = stdev->setup->target_loop[0].BLT_TBA;
  result |= bdisp2_blit_simple (stdrv, stdev, src_cb, dst_pt_cb);

  /* copy Cr */
  stdev->setup->ConfigSource2.BLT_S2BA = stdev->setup->ConfigSource1.BLT_S1BA;
  stdev->setup->ConfigTarget.BLT_TBA = stdev->setup->target_loop[1].BLT_TBA;
  result |= bdisp2_blit_simple (stdrv, stdev, src_cr, dst_pt_cr);

  /* restore */
  stdev->setup->ConfigSource2.BLT_S2BA = s2ba;
  stdev->setup->ConfigSource2.BLT_S2TY = s2ty;
  stdev->setup->ConfigTarget.BLT_TBA = tba;
  stdev->setup->ConfigTarget.BLT_TTY = tty;

  return result;
}

int
bdisp2_blit_simple_YV444 (struct stm_bdisp2_driver_data * const stdrv,
                          struct stm_bdisp2_device_data * const stdev,
                          const stm_blitter_rect_t      * const src,
                          const stm_blitter_point_t     * const dst_pt)
{
  return bdisp2_blit_simple_YV4xx(stdrv, stdev,
                                  src, dst_pt,
                                  src, dst_pt,
                                  src, dst_pt,
                                  1);
}

int
bdisp2_blit_simple_YV422 (struct stm_bdisp2_driver_data * const stdrv,
                          struct stm_bdisp2_device_data * const stdev,
                          const stm_blitter_rect_t      * const src,
                          const stm_blitter_point_t     * const dst_pt)
{
  stm_blitter_rect_t src_cbcr = *src;
  stm_blitter_point_t dst_pt_cbcr = *dst_pt;

  src_cbcr.position.x /= 2;
  src_cbcr.size.w /= 2;
  dst_pt_cbcr.x /= 2;

  return bdisp2_blit_simple_YV4xx(stdrv, stdev,
                                  src, dst_pt,
                                  &src_cbcr, &dst_pt_cbcr,
                                  &src_cbcr, &dst_pt_cbcr,
                                  2);
}

int
bdisp2_blit_simple_YV420 (struct stm_bdisp2_driver_data * const stdrv,
                          struct stm_bdisp2_device_data * const stdev,
                          const stm_blitter_rect_t      * const src,
                          const stm_blitter_point_t     * const dst_pt)
{
  stm_blitter_rect_t src_cbcr = *src;
  stm_blitter_point_t dst_pt_cbcr = *dst_pt;

  src_cbcr.position.x /= 2;
  src_cbcr.position.y /= 2;
  src_cbcr.size.w /= 2;
  src_cbcr.size.h /= 2;
  dst_pt_cbcr.x /= 2;
  dst_pt_cbcr.y /= 2;

  return bdisp2_blit_simple_YV4xx(stdrv, stdev,
                                  src, dst_pt,
                                  &src_cbcr, &dst_pt_cbcr,
                                  &src_cbcr, &dst_pt_cbcr,
                                  2);
}

int
bdisp2_blit (struct stm_bdisp2_driver_data * const stdrv,
             struct stm_bdisp2_device_data * const stdev,
             const stm_blitter_rect_t      * const _src,
             const stm_blitter_point_t     * const _dst_pt)
{
  stm_blitter_rect_t src = *_src, src_backup;
  stm_blitter_rect_t dst;
  stm_blitter_point_t dst_pt = *_dst_pt;
  bool requires_spans;

  if (!stdev->setup->blitstate.bFixedPoint)
    {
      if (!stdev->setup->blitstate.srcxy_fixed_point)
        {
          src.position.x *= FIXED_POINT_ONE;
          src.position.y *= FIXED_POINT_ONE;
        }
      src.size.w *= FIXED_POINT_ONE;
      src.size.h *= FIXED_POINT_ONE;
      dst_pt.x *= FIXED_POINT_ONE;
      dst_pt.y *= FIXED_POINT_ONE;
    }
  stm_blit_printd (BDISP_BLT,
                   "%s (%lld.%06lld,%lld.%06lld-%lld.%06lldx%lld.%06lld -> %lld.%06lld,%lld.%06lld)\n",
                   __func__, RECTANGLE_VALSf (&src), INT_VALf (dst_pt.x),
                   INT_VALf (dst_pt.y));
  DUMP_INFO ();

  /* split buffer formats _always_ need the rescale engine enabled. */
  STM_BLITTER_ASSERT (!(stdev->setup->ConfigGeneral.BLT_CIC
                        & BLIT_CIC_NODE_SOURCE3));

  if (unlikely (!src.size.w || !src.size.h))
    return 0;
  if (check_size_constraints_f16 (stdev, src.position.x, src.position.y,
                                  src.size.w, src.size.h))
    return -EINVAL;
  if (check_size_constraints_f16 (stdev, dst_pt.x, dst_pt.y, 1, 1))
    return -EINVAL;
  if (BLIT_TY_COLOR_FORM_IS_UP_SAMPLED (stdev->setup->ConfigSource2.BLT_S2TY)
      &&((TO_FIXED_POINT(stdev->setup->upsampled_src_nbpix_min) > src.size.w)
         || (TO_FIXED_POINT(stdev->setup->upsampled_src_nbpix_min) > src.size.h)))
    return -EOPNOTSUPP;

  if(bdisp2_is_suspended(stdrv))
    return -EAGAIN;

  if (stdev->setup->blitstate.n_passes > 1)
    src_backup = src;

  dst.position = dst_pt;
  dst.size = src.size;
  if (unlikely (!bdisp2_blit_setup (stdev,
                                    stdev->features.hw.line_buffer_length,
                                    &src, &dst, &dst, &requires_spans)))
    return -EINVAL;

  if (likely (!requires_spans))
    {
      bdisp_aq_copy_n_push_node (stdrv, stdev);
      bdisp_aq_clut_update_disable_if_unsafe (stdev);
    }
  else
    {
      if (!stdev->setup->blitstate.flicker_filter)
        bdisp2_blit_using_spans (stdrv, stdev,
                                 stdev->features.hw.line_buffer_length,
                                 &src, &dst, &dst);
      else
        bdisp2_blit_flicker_filter (stdrv, stdev,
                                    stdev->features.hw.line_buffer_length,
                                    &src, &dst);
    }

  if (stdev->setup->blitstate.n_passes > 1)
    {
      BDispAqNodeState state;
      bool             dummy;

      bdisp_aq_second_node_prepare (stdrv, stdev, &state);

      src = src_backup;
      dst.position = dst_pt;
      dst.size = src.size;
      /* setup did work earlier, so it just has to work a second time, too! */
      dummy = bdisp2_blit_setup (stdev,
                                 stdev->features.hw.line_buffer_length,
                                 &src, &dst, &dst, &requires_spans);
      (void) dummy;

      if (likely (!requires_spans))
        {
          bdisp_aq_copy_n_push_node (stdrv, stdev);
          bdisp_aq_clut_update_disable_if_unsafe (stdev);
        }
      else
        bdisp2_blit_using_spans (stdrv, stdev,
                                 stdev->features.hw.line_buffer_length,
                                 &src, &dst, &dst);

      if (stdev->setup->blitstate.n_passes > 2)
        {
          bdisp_aq_third_node_prepare (stdrv, stdev);

          src = src_backup;
          dst.position = dst_pt;
          dst.size = src.size;
          /* setup did work earlier, so it just has to work a second time,
             too! */
          dummy = bdisp2_blit_setup (stdev,
                                     stdev->features.hw.line_buffer_length,
                                     &src, &dst, &dst, &requires_spans);
          (void) dummy;

          if (likely (!requires_spans))
            {
              bdisp_aq_copy_n_push_node (stdrv, stdev);
              bdisp_aq_clut_update_disable_if_unsafe (stdev);
            }
          else
            bdisp2_blit_using_spans (stdrv, stdev,
                                     stdev->features.hw.line_buffer_length,
                                     &src, &dst, &dst);
        }

      bdisp_aq_second_node_release (stdrv, stdev, &state);
    }

  return 0;
}


static int
bdisp2_stretch_blit_internal (struct stm_bdisp2_driver_data * const stdrv,
                              struct stm_bdisp2_device_data * const stdev,
                              uint16_t                       line_buffer_length,
                              const stm_blitter_rect_t      * const _src,
                              const stm_blitter_rect_t      * const _dst)
{
  stm_blitter_rect_t src = *_src, src_backup;
  stm_blitter_rect_t dst = *_dst, dst_backup;
  bool requires_spans;

  if (!stdev->setup->blitstate.bFixedPoint)
    {
      /* src */
      if (!stdev->setup->blitstate.srcxy_fixed_point)
        {
          src.position.x *= FIXED_POINT_ONE;
          src.position.y *= FIXED_POINT_ONE;
        }
      src.size.w *= FIXED_POINT_ONE;
      src.size.h *= FIXED_POINT_ONE;
      /* dst */
      dst.position.x *= FIXED_POINT_ONE;
      dst.position.y *= FIXED_POINT_ONE;
      dst.size.w *= FIXED_POINT_ONE;
      dst.size.h *= FIXED_POINT_ONE;
    }
  stm_blit_printd (BDISP_STRETCH,
                   "%s (%lld.%06lld,%lld.%06lld-%lld.%06lldx%lld.%06lld -> %lld.%06lld,%lld.%06lld-%lld.%06lldx%lld.%06lld)\n",
                   __func__,
                   RECTANGLE_VALSf (&src), RECTANGLE_VALSf (&dst));
  DUMP_INFO ();

  if (unlikely (!src.size.w || !src.size.h || !dst.size.w || !dst.size.h))
    return 0;
  if (check_size_constraints_f16 (stdev, src.position.x, src.position.y,
                                  src.size.w, src.size.h))
    return -EINVAL;
  if (check_size_constraints_f16 (stdev, dst.position.x, dst.position.y,
                                  dst.size.w, dst.size.h))
    return -EINVAL;
  if ((TO_FIXED_POINT(stdev->setup->upsampled_src_nbpix_min) > src.size.w)
      || (TO_FIXED_POINT(stdev->setup->upsampled_src_nbpix_min) > src.size.h))
    return -EOPNOTSUPP;

  /* FIXME: something in WebKit/GTK+/Cairo ends up setting a stretchblit to
     a destination size of 1x1. We should special case this by using just some
     random pixel value in the input (maybe the top left one?) */

  if(bdisp2_is_suspended(stdrv))
    return -EAGAIN;

  if (stdev->setup->blitstate.n_passes > 1)
    {
      src_backup = src;
      dst_backup = dst;
    }

  if (!bdisp2_stretchblit_setup (stdev,
                                 line_buffer_length,
                                 &src, &dst, &dst, &requires_spans))
    return -EINVAL;

  if (!requires_spans)
    {
      bdisp_aq_copy_n_push_node (stdrv, stdev);
      bdisp_aq_clut_update_disable_if_unsafe (stdev);
    }
  else
    bdisp2_blit_using_spans (stdrv, stdev,
                             line_buffer_length,
                             &src, &dst, &dst);


  if (stdev->setup->blitstate.n_passes > 1)
    {
      BDispAqNodeState state;
      bool             dummy;

      bdisp_aq_second_node_prepare (stdrv, stdev, &state);

      src = src_backup;
      dst = dst_backup;
      /* setup did work earlier, so it just has to work a second time, too! */
      dummy = bdisp2_stretchblit_setup (stdev,
                                        line_buffer_length,
                                        &src, &dst, &dst, &requires_spans);
      (void) dummy;

      if (!requires_spans)
        {
          bdisp_aq_copy_n_push_node (stdrv, stdev);
          bdisp_aq_clut_update_disable_if_unsafe (stdev);
        }
      else
        bdisp2_blit_using_spans (stdrv, stdev,
                                 line_buffer_length,
                                 &src, &dst, &dst);

      if (stdev->setup->blitstate.n_passes > 2)
        {
          bdisp_aq_third_node_prepare (stdrv, stdev);

          src = src_backup;
          dst = dst_backup;
          /* setup did work earlier, so it just has to work a second time,
             too! */
          dummy = bdisp2_stretchblit_setup (stdev,
                                            line_buffer_length,
                                            &src, &dst, &dst, &requires_spans);
          (void) dummy;

          if (!requires_spans)
            {
              bdisp_aq_copy_n_push_node (stdrv, stdev);
              bdisp_aq_clut_update_disable_if_unsafe (stdev);
            }
          else
            bdisp2_blit_using_spans (stdrv, stdev,
                                     line_buffer_length,
                                     &src, &dst, &dst);
        }

      bdisp_aq_second_node_release (stdrv, stdev, &state);
    }

  return 0;
}

int
bdisp2_stretch_blit (struct stm_bdisp2_driver_data * const stdrv,
                     struct stm_bdisp2_device_data * const stdev,
                     const stm_blitter_rect_t      * const _src,
                     const stm_blitter_rect_t      * const _dst)
{
  return bdisp2_stretch_blit_internal (
                     stdrv, stdev,
                     stdev->features.hw.line_buffer_length,
                     _src, _dst);

}

int
bdisp2_stretch_blit_MB (struct stm_bdisp2_driver_data * const stdrv,
                        struct stm_bdisp2_device_data * const stdev,
                        const stm_blitter_rect_t      * const src,
                        const stm_blitter_rect_t      * const dst)
{
  return bdisp2_stretch_blit_internal (
                     stdrv, stdev,
                     STM_BLITTER_MIN (stdev->features.hw.line_buffer_length,
                                      stdev->features.hw.mb_buffer_length),
                     src, dst);
}

static int
bdisp2_blit_as_stretch_internal (struct stm_bdisp2_driver_data * const stdrv,
                                 struct stm_bdisp2_device_data * const stdev,
                                 uint16_t                       line_buffer_length,
                                 const stm_blitter_rect_t      * const src,
                                 const stm_blitter_point_t     * const dst_pt)
{
  stm_blitter_rect_t dst = { .position = *dst_pt, .size = src->size };

  /* In case of De-interlacing with no hardware support
     double the height as gonna use re-size to simulate it */
  if (!stdev->features.hw.spatial_dei
      && (stdev->setup->blitstate.bDeInterlacingTop
          || stdev->setup->blitstate.bDeInterlacingBottom))
    dst.size.h *= 2;

  return bdisp2_stretch_blit_internal (stdrv, stdev, line_buffer_length,
                                       src, &dst);
}

int
bdisp2_blit_as_stretch_MB (struct stm_bdisp2_driver_data * const stdrv,
                           struct stm_bdisp2_device_data * const stdev,
                           const stm_blitter_rect_t      * const src,
                           const stm_blitter_point_t     * const dst_pt)
{
  return bdisp2_blit_as_stretch_internal (
                     stdrv, stdev,
                     STM_BLITTER_MIN (stdev->features.hw.line_buffer_length,
                                      stdev->features.hw.mb_buffer_length),
                     src, dst_pt);
}

int
bdisp2_blit_as_stretch (struct stm_bdisp2_driver_data * const stdrv,
                        struct stm_bdisp2_device_data * const stdev,
                        const stm_blitter_rect_t      * const src,
                        const stm_blitter_point_t     * const dst_pt)
{
  return bdisp2_blit_as_stretch_internal (stdrv, stdev,
                                          stdev->features.hw.line_buffer_length,
                                          src, dst_pt);
}

/* The following 2 macro should ONLY be used with multi buffer source to
 * multibuffer destination colorformat.
 * The idea is to only do Bus access for data (chroma or luma relevant to the
 * current node.
 * Note: s3ty and ins will be changed */
#define BLIT_OPTIMIZE_CHROMA_BUS_ACCESS(s3ty, ins, tty)  \
{                                                        \
  (s3ty) |= BLIT_S3TY_BLANK_ACC;                         \
  if (BLIT_TY_COLOR_FORM_IS_3_BUFFER(s3ty)               \
      && BLIT_TY_COLOR_FORM_IS_3_BUFFER(tty))            \
    {                                                    \
       if ((tty) & BLIT_TTY_CR_NOT_CB)                   \
         (ins) |= BLIT_INS_SRC2_MODE_COLOR_FILL;         \
       else                                              \
         (ins) |= BLIT_INS_SRC1_MODE_COLOR_FILL;         \
    }                                                    \
}

#define BLIT_OPTIMIZE_LUMA_BUS_ACCESS(s3ty, ins)  \
{                                                 \
  (ins) |= BLIT_INS_SRC2_MODE_COLOR_FILL;         \
  if (BLIT_TY_COLOR_FORM_IS_3_BUFFER(s3ty))       \
    {                                             \
       (ins) |= BLIT_INS_SRC1_MODE_COLOR_FILL;    \
    }                                             \
}

static int
bdisp2_stretch_blit_planar_4xx_internal (struct stm_bdisp2_driver_data * const stdrv,
                                         struct stm_bdisp2_device_data * const stdev,
                                         uint16_t                       line_buffer_length,
                                         const stm_blitter_rect_t      * const src,
                                         const stm_blitter_rect_t      * const dst)
{
  uint32_t tba  = stdev->setup->ConfigTarget.BLT_TBA;
  uint32_t tty  = stdev->setup->ConfigTarget.BLT_TTY;
  uint32_t s3ty = stdev->setup->ConfigSource3.BLT_S3TY;
  uint32_t s2ty = stdev->setup->ConfigSource2.BLT_S2TY;
  uint32_t ins  = stdev->setup->ConfigGeneral.BLT_INS;

  int result, i;

  stm_blit_printd (BDISP_STRETCH, "%s\n", __func__);

  /* copy Y */
  /* Optimize luma node*/
  if (BLIT_TY_COLOR_FORM_IS_2_BUFFER(s2ty) || BLIT_TY_COLOR_FORM_IS_3_BUFFER(s2ty))
    {
      BLIT_OPTIMIZE_LUMA_BUS_ACCESS(stdev->setup->ConfigSource3.BLT_S3TY,
                                    stdev->setup->ConfigGeneral.BLT_INS);
    }

  result = bdisp2_stretch_blit_internal (stdrv, stdev, line_buffer_length,
                                         src, dst);

  stdev->setup->ConfigGeneral.BLT_INS  = ins;
  /* copy Cb/Cr */
  for (i = 0; i < stdev->setup->n_target_extra_loops; ++i)
    {
      stm_blitter_rect_t dst_cbcr = *dst;

      dst_cbcr.position.x /= stdev->setup->target_loop[i].coordinate_divider.h;
      dst_cbcr.size.w     /= stdev->setup->target_loop[i].coordinate_divider.h;
      dst_cbcr.position.y /= stdev->setup->target_loop[i].coordinate_divider.v;
      dst_cbcr.size.h     /= stdev->setup->target_loop[i].coordinate_divider.v;

      stdev->setup->ConfigTarget.BLT_TBA = stdev->setup->target_loop[i].BLT_TBA;
      stdev->setup->ConfigTarget.BLT_TTY = stdev->setup->target_loop[i].BLT_TTY;

      /* Optimize chroma node*/
      if (BLIT_TY_COLOR_FORM_IS_2_BUFFER(s2ty) || BLIT_TY_COLOR_FORM_IS_3_BUFFER(s2ty))
        {
          BLIT_OPTIMIZE_CHROMA_BUS_ACCESS(stdev->setup->ConfigSource3.BLT_S3TY,
                                          stdev->setup->ConfigGeneral.BLT_INS,
                                          stdev->setup->ConfigTarget.BLT_TTY);
        }
      result |= bdisp2_stretch_blit_internal (stdrv, stdev, line_buffer_length,
                                              src, &dst_cbcr);
      stdev->setup->ConfigSource3.BLT_S3TY = s3ty;
      stdev->setup->ConfigGeneral.BLT_INS  = ins;
    }

  /* restore */
  stdev->setup->ConfigTarget.BLT_TBA = tba;
  stdev->setup->ConfigTarget.BLT_TTY = tty;

  return result;
}

int
bdisp2_stretch_blit_planar_4xx (struct stm_bdisp2_driver_data * const stdrv,
                                struct stm_bdisp2_device_data * const stdev,
                                const stm_blitter_rect_t      * const src,
                                const stm_blitter_rect_t      * const dst)
{
  return bdisp2_stretch_blit_planar_4xx_internal (
                   stdrv, stdev,
                   stdev->features.hw.line_buffer_length,
                   src, dst);
}

int
bdisp2_stretch_blit_MB4xx (struct stm_bdisp2_driver_data * const stdrv,
                           struct stm_bdisp2_device_data * const stdev,
                           const stm_blitter_rect_t      * const src,
                           const stm_blitter_rect_t      * const dst)
{
  return bdisp2_stretch_blit_planar_4xx_internal (
                   stdrv, stdev,
                   STM_BLITTER_MIN (stdev->features.hw.line_buffer_length,
                                    stdev->features.hw.mb_buffer_length),
                   src, dst);
}

static int
bdisp2_blit_as_stretch_planar_4xx_internal (struct stm_bdisp2_driver_data * const stdrv,
                                            struct stm_bdisp2_device_data * const stdev,
                                            uint16_t                       line_buffer_length,
                                            const stm_blitter_rect_t      * const src,
                                            const stm_blitter_point_t     * const dst_pt)
{
  stm_blitter_rect_t dst = { .position = *dst_pt, .size = src->size };

  /* In case of De-interlacing with no hardware support
     double the height as gonna use re-size to simulate it */
  if (!stdev->features.hw.spatial_dei
      && (stdev->setup->blitstate.bDeInterlacingTop
          || stdev->setup->blitstate.bDeInterlacingBottom))
    dst.size.h *= 2;

  return bdisp2_stretch_blit_planar_4xx_internal (stdrv, stdev, line_buffer_length,
                                                  src, &dst);
}

int
bdisp2_blit_as_stretch_planar_4xx (struct stm_bdisp2_driver_data * const stdrv,
                                   struct stm_bdisp2_device_data * const stdev,
                                   const stm_blitter_rect_t      * const src,
                                   const stm_blitter_point_t     * const dst_pt)
{
  return bdisp2_blit_as_stretch_planar_4xx_internal (
                   stdrv, stdev,
                   stdev->features.hw.line_buffer_length,
                   src, dst_pt);
}

int
bdisp2_blit_as_stretch_MB4xx (struct stm_bdisp2_driver_data * const stdrv,
                              struct stm_bdisp2_device_data * const stdev,
                              const stm_blitter_rect_t      * const src,
                              const stm_blitter_point_t     * const dst_pt)
{
  return bdisp2_blit_as_stretch_planar_4xx_internal (
                   stdrv, stdev,
                   STM_BLITTER_MIN (stdev->features.hw.line_buffer_length,
                                    stdev->features.hw.mb_buffer_length),
                   src, dst_pt);
}

int
bdisp2_blit2_as_stretch_internal (struct stm_bdisp2_driver_data * const stdrv,
                                  struct stm_bdisp2_device_data * const stdev,
                                  uint16_t                       line_buffer_length,
                                  const stm_blitter_rect_t      * const _src1,
                                  const stm_blitter_point_t     * const _src2_pt,
                                  const stm_blitter_point_t     * const _dst_pt)
{
  /* _src1 is BLT_S2XX in BDisp2 terms,
     _src2_pt is the BLT_S1XX in BDisp2 terms */
  stm_blitter_rect_t  src2 = *_src1;
  stm_blitter_point_t src1_pt = *_src2_pt, dst_pt = *_dst_pt;
  stm_blitter_rect_t src2_backup;
  stm_blitter_rect_t src1, dst;
  bool requires_spans;

  if (!stdev->setup->blitstate.bFixedPoint)
    {
      if (!stdev->setup->blitstate.srcxy_fixed_point)
        {
          src2.position.x *= FIXED_POINT_ONE;
          src2.position.y *= FIXED_POINT_ONE;
          src1_pt.x *= FIXED_POINT_ONE;
          src1_pt.y *= FIXED_POINT_ONE;
        }
      src2.size.w *= FIXED_POINT_ONE;
      src2.size.h *= FIXED_POINT_ONE;
      dst_pt.x *= FIXED_POINT_ONE;
      dst_pt.y *= FIXED_POINT_ONE;
    }
  stm_blit_printd (BDISP_BLT,
                   "%s (%lld.%06lld,%lld.%06lld-%lld.%06lldx%lld.%06lld + %lld.%06lld,%lld.%06lld -> %lld.%06lld,%lld.%06lld)\n",
                   __func__,
                   RECTANGLE_VALSf (&src2),
                   INT_VALf (src1_pt.x), INT_VALf (src1_pt.y),
                   INT_VALf (dst_pt.x), INT_VALf (dst_pt.y));
  DUMP_INFO ();

  if (unlikely (!src2.size.w || !src2.size.h))
    return 0;
  if (check_size_constraints_f16 (stdev, src2.position.x, src2.position.y,
                                  src2.size.w, src2.size.h))
    return -EINVAL;
  if (check_size_constraints_f16 (stdev, src1_pt.x, src1_pt.y, 1, 1))
    return -EINVAL;
  if (check_size_constraints_f16 (stdev, dst_pt.x, dst_pt.y, 1, 1))
    return -EINVAL;
  if ((TO_FIXED_POINT(stdev->setup->upsampled_src_nbpix_min) > src2.size.w)
      || (TO_FIXED_POINT(stdev->setup->upsampled_src_nbpix_min) > src2.size.h))
    return -EOPNOTSUPP;

  if(bdisp2_is_suspended(stdrv))
    return -EAGAIN;


  if (stdev->setup->blitstate.n_passes > 1)
    src2_backup = src2;

  src1.position = src1_pt;
  src1.size = src2.size;
  dst.position = dst_pt;
  dst.size = src2.size;

  if (unlikely (!bdisp2_stretchblit_setup (stdev,
                                           line_buffer_length,
                                           &src2, &src1, &dst, &requires_spans)))
    return -EINVAL;

  if (likely (!requires_spans))
    {
      bdisp_aq_copy_n_push_node (stdrv, stdev);
      bdisp_aq_clut_update_disable_if_unsafe (stdev);
    }
  else
    bdisp2_blit_using_spans (stdrv, stdev,
                             line_buffer_length,
                             &src2, &src1, &dst);


  if (stdev->setup->blitstate.n_passes > 1)
    {
      BDispAqNodeState state;
      bool             dummy;

      bdisp_aq_second_node_prepare_2 (stdrv, stdev, &state);

      src2 = src2_backup;
      src1.position = src1_pt;
      src1.size = src2.size;
      dst.position = dst_pt;
      dst.size = src2.size;
      /* setup did work earlier, so it just has to work a second time, too! */
      dummy = bdisp2_stretchblit_setup (stdev,
                                        line_buffer_length,
                                        &src2, &src1, &dst, &requires_spans);
      (void) dummy;

      if (likely (!requires_spans))
        {
          bdisp_aq_copy_n_push_node (stdrv, stdev);
          bdisp_aq_clut_update_disable_if_unsafe (stdev);
        }
      else
        bdisp2_blit_using_spans (stdrv, stdev,
                                 line_buffer_length,
                                 &src2, &src1, &dst);

      if (stdev->setup->blitstate.n_passes > 2)
        {
          bdisp_aq_third_node_prepare (stdrv, stdev);

          src2 = src2_backup;
          src1.position = src1_pt;
          src1.size = src2.size;
          dst.position = dst_pt;
          dst.size = src2.size;
          /* setup did work earlier, so it just has to work a second time,
             too! */
          dummy = bdisp2_stretchblit_setup (stdev,
                                            line_buffer_length,
                                            &src2, &src1, &dst, &requires_spans);
          (void) dummy;

          if (likely (!requires_spans))
            {
              bdisp_aq_copy_n_push_node (stdrv, stdev);
              bdisp_aq_clut_update_disable_if_unsafe (stdev);
            }
          else
            bdisp2_blit_using_spans (stdrv, stdev,
                                     line_buffer_length,
                                     &src2, &src1, &dst);
        }

      bdisp_aq_second_node_release_2 (stdrv, stdev, &state);
    }

  return 0;
}

int
bdisp2_blit2_as_stretch_MB (struct stm_bdisp2_driver_data * const stdrv,
                            struct stm_bdisp2_device_data * const stdev,
                            const stm_blitter_rect_t      * const src1,
                            const stm_blitter_point_t     * const src2_pt,
                            const stm_blitter_point_t     * const dst_pt)
{
  return bdisp2_blit2_as_stretch_internal (
                   stdrv, stdev,
                   STM_BLITTER_MIN (stdev->features.hw.line_buffer_length,
                                    stdev->features.hw.mb_buffer_length),
                   src1, src2_pt, dst_pt);
}

int
bdisp2_blit2_as_stretch (struct stm_bdisp2_driver_data * const stdrv,
                         struct stm_bdisp2_device_data * const stdev,
                         const stm_blitter_rect_t      * const src1,
                         const stm_blitter_point_t     * const src2_pt,
                         const stm_blitter_point_t     * const dst_pt)
{
  return bdisp2_blit2_as_stretch_internal (
                     stdrv, stdev,
                     stdev->features.hw.line_buffer_length,
                     src1, src2_pt, dst_pt);

}

int
bdisp2_blit2 (struct stm_bdisp2_driver_data * const stdrv,
              struct stm_bdisp2_device_data * const stdev,
              const stm_blitter_rect_t      * const _src1,
              const stm_blitter_point_t     * const _src2_pt,
              const stm_blitter_point_t     * const _dst_pt)
{
  /* _src1 is BLT_S2XX in BDisp2 terms,
     _src2_pt is the BLT_S1XX in BDisp2 terms */
  stm_blitter_rect_t  src2 = *_src1;
  stm_blitter_point_t src1_pt = *_src2_pt, dst_pt = *_dst_pt;
  stm_blitter_rect_t src2_backup;
  stm_blitter_rect_t src1, dst;
  bool requires_spans;

  if (!stdev->setup->blitstate.bFixedPoint)
    {
      if (!stdev->setup->blitstate.srcxy_fixed_point)
        {
          src2.position.x *= FIXED_POINT_ONE;
          src2.position.y *= FIXED_POINT_ONE;
          src1_pt.x *= FIXED_POINT_ONE;
          src1_pt.y *= FIXED_POINT_ONE;
        }
      src2.size.w *= FIXED_POINT_ONE;
      src2.size.h *= FIXED_POINT_ONE;
      dst_pt.x *= FIXED_POINT_ONE;
      dst_pt.y *= FIXED_POINT_ONE;
    }
  stm_blit_printd (BDISP_BLT,
                   "%s (%lld.%06lld,%lld.%06lld-%lld.%06lldx%lld.%06lld + %lld.%06lld,%lld.%06lld -> %lld.%06lld,%lld.%06lld)\n",
                   __func__,
                   RECTANGLE_VALSf (&src2),
                   INT_VALf (src1_pt.x), INT_VALf (src1_pt.y),
                   INT_VALf (dst_pt.x), INT_VALf (dst_pt.y));
  DUMP_INFO ();

  /* split buffer formats _always_ need the rescale engine enabled. */
  STM_BLITTER_ASSERT (!(stdev->setup->ConfigGeneral.BLT_CIC
                        & BLIT_CIC_NODE_SOURCE3));

  if (unlikely (!src2.size.w || !src2.size.h))
    return 0;
  if (check_size_constraints_f16 (stdev, src2.position.x, src2.position.y,
                                  src2.size.w, src2.size.h))
    return -EINVAL;
  if (check_size_constraints_f16 (stdev, src1_pt.x, src1_pt.y, 1, 1))
    return -EINVAL;
  if (check_size_constraints_f16 (stdev, dst_pt.x, dst_pt.y, 1, 1))
    return -EINVAL;
  if (BLIT_TY_COLOR_FORM_IS_UP_SAMPLED (stdev->setup->ConfigSource2.BLT_S2TY)
      &&((TO_FIXED_POINT(stdev->setup->upsampled_src_nbpix_min) > src2.size.w)
         || (TO_FIXED_POINT(stdev->setup->upsampled_src_nbpix_min) > src2.size.h)))
    return -EOPNOTSUPP;

  if(bdisp2_is_suspended(stdrv))
    return -EAGAIN;

  if (stdev->setup->blitstate.n_passes > 1)
    src2_backup = src2;

  src1.position = src1_pt;
  src1.size = src2.size;
  dst.position = dst_pt;
  dst.size = src2.size;
  if (unlikely (!bdisp2_blit_setup (stdev,
                                    stdev->features.hw.line_buffer_length,
                                    &src2, &src1, &dst, &requires_spans)))
    return -EINVAL;

  if (likely (!requires_spans))
    {
      bdisp_aq_copy_n_push_node (stdrv, stdev);
      bdisp_aq_clut_update_disable_if_unsafe (stdev);
    }
  else
    bdisp2_blit_using_spans (stdrv, stdev,
                             stdev->features.hw.line_buffer_length,
                             &src2, &src1, &dst);


  if (stdev->setup->blitstate.n_passes > 1)
    {
      BDispAqNodeState state;
      bool             dummy;

      bdisp_aq_second_node_prepare_2 (stdrv, stdev, &state);

      src2 = src2_backup;
      src1.position = src1_pt;
      src1.size = src2.size;
      dst.position = dst_pt;
      dst.size = src2.size;
      /* setup did work earlier, so it just has to work a second time, too! */
      dummy = bdisp2_blit_setup (stdev,
                                 stdev->features.hw.line_buffer_length,
                                 &src2, &src1, &dst, &requires_spans);
      (void) dummy;

      if (likely (!requires_spans))
        {
          bdisp_aq_copy_n_push_node (stdrv, stdev);
          bdisp_aq_clut_update_disable_if_unsafe (stdev);
        }
      else
        bdisp2_blit_using_spans (stdrv, stdev,
                                 stdev->features.hw.line_buffer_length,
                                 &src2, &src1, &dst);

      if (stdev->setup->blitstate.n_passes > 2)
        {
          bdisp_aq_third_node_prepare (stdrv, stdev);

          src2 = src2_backup;
          src1.position = src1_pt;
          src1.size = src2.size;
          dst.position = dst_pt;
          dst.size = src2.size;
          /* setup did work earlier, so it just has to work a second time,
             too! */
          dummy = bdisp2_blit_setup (stdev,
                                     stdev->features.hw.line_buffer_length,
                                     &src2, &src1, &dst, &requires_spans);
          (void) dummy;

          if (likely (!requires_spans))
            {
              bdisp_aq_copy_n_push_node (stdrv, stdev);
              bdisp_aq_clut_update_disable_if_unsafe (stdev);
            }
          else
            bdisp2_blit_using_spans (stdrv, stdev,
                                     stdev->features.hw.line_buffer_length,
                                     &src2, &src1, &dst);
        }

      bdisp_aq_second_node_release_2 (stdrv, stdev, &state);
    }

  return 0;
}


extern int
_bdisp_state_set_buffer_type (const struct stm_bdisp2_device_data * const stdev,
                              uint32_t                            * const ty,
                              stm_blitter_surface_format_t         format,
                              uint16_t                             pitch);

extern bool
bdisp2_aq_check_src_format (const struct stm_bdisp2_device_data * const stdev,
                            stm_blitter_surface_format_t         format);

int
bdisp2_stretch_blit_RLE (struct stm_bdisp2_driver_data * const stdrv,
                         struct stm_bdisp2_device_data * const stdev,
                         unsigned long                  src_address,
                         unsigned long                  src_length,
                         unsigned long                  dst_address,
                         stm_blitter_surface_format_t   dst_format,
                         uint16_t                       dst_pitch,
                         const stm_blitter_rect_t      * const drect)
{
  struct _BltNode_Fast *node;
  uint32_t blt_ins;

  stm_blit_printd (BDISP_BLT, "%s (%lu -> %lld,%lld-%lldx%lld)\n", __func__,
                   src_length, RECTANGLE_VALS (drect));
  DUMP_INFO ();

  /* FIXME: FIXED_POINT support? */

  if (unlikely (!src_length))
    return 0;
  if (unlikely (src_length > 0x00ffffff))
    return -EFBIG;
  if (check_size_constraints (stdev, drect->position.x, drect->position.y,
                              drect->size.w, drect->size.h))
    return -EINVAL;
  if (bdisp2_aq_check_src_format(stdev, STM_BLITTER_SF_RLD_BD))
    return -EOPNOTSUPP;

  if(bdisp2_is_suspended(stdrv))
    return -EAGAIN;

  node = bdisp2_get_new_node (stdrv, stdev);
  blt_ins = BLIT_INS_SRC1_MODE_MEMORY;

  bdisp2_update_num_ops (stdrv, stdev, &blt_ins);

  /* target */
  node->common.ConfigTarget.BLT_TBA = dst_address;
  _bdisp_state_set_buffer_type (stdev, &node->common.ConfigTarget.BLT_TTY,
                                dst_format, dst_pitch);
  node->common.ConfigTarget.BLT_TXY = ((drect->position.y << 16)
                                       | drect->position.x);
  node->common.ConfigTarget.BLT_TSZ_S1SZ = ((drect->size.h << 16)
                                            | drect->size.w);

  /* source */
  node->common.ConfigSource1.BLT_S1BA = src_address;
  node->common.ConfigSource1.BLT_S1TY = BLIT_COLOR_FORM_RLD_BD;
  node->common.ConfigSource1.BLT_S1XY = src_length;

  node->common.ConfigGeneral.BLT_CIC = BLIT_CIC_BLT_NODE_FAST;
  node->common.ConfigGeneral.BLT_INS = blt_ins;
  node->common.ConfigGeneral.BLT_ACK = BLIT_ACK_BYPASSSOURCE1;

  bdisp2_update_config_static (stdrv->bdisp_shared, &node->ConfigStatic);

  bdisp2_finish_node (stdrv, stdev, node);

  return 0;
}


static int
bdisp2_blit_shortcut_real (struct stm_bdisp2_driver_data * const stdrv,
                           struct stm_bdisp2_device_data * const stdev,
                           stm_blitter_rect_t            * const dst,
                           uint32_t                       color)
{
  struct _BltNode_Fast *node;

  if (!stdev->setup->blitstate.bFixedPoint)
    stm_blit_printd (BDISP_BLT, "%s (%lld,%lld-%lldx%lld)\n",
                     __func__, RECTANGLE_VALS (dst));
  else
    {
      stm_blit_printd (BDISP_BLT, "%s (%lld.%06lld,%lld.%06lld-%lld.%06lldx%lld.%06lld)\n",
                       __func__, RECTANGLE_VALSf (dst));
      dst->position.x /= FIXED_POINT_ONE;
      dst->position.y /= FIXED_POINT_ONE;
      dst->size.w /= FIXED_POINT_ONE;
      dst->size.h /= FIXED_POINT_ONE;
    }
  DUMP_INFO ();

  if (unlikely (!dst->size.w || !dst->size.h))
    return 0;
  if (check_size_constraints (stdev, dst->position.x, dst->position.y,
                              dst->size.w, dst->size.h))
    return -EINVAL;
  if (BLIT_TY_COLOR_FORM_IS_UP_SAMPLED (stdev->setup->ConfigTarget.BLT_TTY)
      &&((stdev->setup->upsampled_dst_nbpix_min > dst->size.w)
	 || (stdev->setup->upsampled_dst_nbpix_min > dst->size.h)))
    return -EOPNOTSUPP;

  if(bdisp2_is_suspended(stdrv))
    return -EAGAIN;

  /* this must be DSPD_CLEAR, set source1 == destination. For YUV formats,
     we let the hardware do conversion from ARGB, thus will not get here */
  STM_BLITTER_ASSERT (!BLIT_TY_COLOR_FORM_IS_YUV (stdev->setup->ConfigTarget.BLT_TTY));

  node = bdisp2_get_new_node_prepared_simple (stdrv, stdev);
  node->common.ConfigSource1.BLT_S1TY =
    (bdisp_ty_sanitise_direction (stdev->setup->ConfigTarget.BLT_TTY)
     & ~BLIT_TY_BIG_ENDIAN);
  node->common.ConfigColor.BLT_S1CF = color;

  /* dimensions */
  node->common.ConfigTarget.BLT_TXY
    = (dst->position.y << 16) | dst->position.x;
  node->common.ConfigTarget.BLT_TSZ_S1SZ
    = (dst->size.h << 16) | dst->size.w;

  bdisp2_finish_node (stdrv, stdev, node);

  return 0;
}

int
bdisp2_blit_shortcut (struct stm_bdisp2_driver_data * const stdrv,
                      struct stm_bdisp2_device_data * const stdev,
                      const stm_blitter_rect_t      * const src,
                      const stm_blitter_point_t     * const dst_pt)
{
  stm_blitter_rect_t dst = { .position = *dst_pt, .size = src->size };

  return bdisp2_blit_shortcut_real (stdrv, stdev, &dst, 0x00000000);
}

int
bdisp2_stretch_blit_shortcut (struct stm_bdisp2_driver_data * const stdrv,
                              struct stm_bdisp2_device_data * const stdev,
                              const stm_blitter_rect_t      * const src,
                              const stm_blitter_rect_t      * const dst)
{
  return bdisp2_blit_shortcut (stdrv, stdev, dst, &dst->position);
}

int
bdisp2_blit2_shortcut (struct stm_bdisp2_driver_data * const stdrv,
                       struct stm_bdisp2_device_data * const stdev,
                       const stm_blitter_rect_t      * const src1,
                       const stm_blitter_point_t     * const src2_pt,
                       const stm_blitter_point_t     * const dst_pt)
{
  return bdisp2_blit_shortcut (stdrv, stdev, src1, dst_pt);
}

int
bdisp2_blit_shortcut_YCbCr422r (struct stm_bdisp2_driver_data * const stdrv,
                                struct stm_bdisp2_device_data * const stdev,
                                const stm_blitter_rect_t      * const _src,
                                const stm_blitter_point_t     * const _dst_pt)
{
  stm_blitter_rect_t src = *_src;
  stm_blitter_point_t dst_pt = *_dst_pt;
  struct _BltNode_YCbCr422r_shortcut *node;
  uint32_t blt_cic;
  uint32_t blt_ins;
  uint32_t blt_ack;

  if (!stdev->setup->blitstate.bFixedPoint)
    {
      if (stdev->setup->blitstate.srcxy_fixed_point)
        {
          src.position.x /= FIXED_POINT_ONE;
          src.position.y /= FIXED_POINT_ONE;
        }
      stm_blit_printd (BDISP_BLT, "%s (%lld,%lld-%lldx%lld -> %ld,%ld)\n",
                       __func__,
                       RECTANGLE_VALS (&src), dst_pt.x, dst_pt.y);
    }
  else
    {
      stm_blit_printd (BDISP_BLT,
                       "%s (%lld.%06lld,%lld.%06lld-%lld.%06lldx%lld.%06lld -> %lld.%06lld,%lld.%06lld)\n",
                       __func__, RECTANGLE_VALSf (&src),
                       INT_VALf (dst_pt.x), INT_VALf (dst_pt.y));
      src.position.x /= FIXED_POINT_ONE;
      src.position.y /= FIXED_POINT_ONE;
      src.size.w /= FIXED_POINT_ONE;
      src.size.h /= FIXED_POINT_ONE;
      dst_pt.x /= FIXED_POINT_ONE;
      dst_pt.y /= FIXED_POINT_ONE;
    }
  DUMP_INFO ();

  if (unlikely (!src.size.w || !src.size.h))
    return 0;
  if (check_size_constraints (stdev,
                              dst_pt.x, dst_pt.y, src.size.w, src.size.h))
    return -EINVAL;
  if ((stdev->setup->upsampled_src_nbpix_min > src.size.w)
      || (stdev->setup->upsampled_src_nbpix_min > src.size.h))
    return -EOPNOTSUPP;

  if(bdisp2_is_suspended(stdrv))
    return -EAGAIN;

  node = bdisp2_get_new_node (stdrv, stdev);

  blt_cic = BLIT_CIC_BLT_NODE_YCbCr422r_SHORTCUT;
  blt_ins = (BLIT_INS_SRC1_MODE_DISABLED
             | BLIT_INS_SRC2_MODE_COLOR_FILL
             | BLIT_INS_ENABLE_IVMX);
  blt_ack = BLIT_ACK_BYPASSSOURCE2;

  bdisp2_update_num_ops (stdrv, stdev, &blt_ins);

  node->ConfigTarget.BLT_TBA = stdev->setup->ConfigTarget.BLT_TBA;
  node->ConfigTarget.BLT_TTY
    = bdisp_ty_sanitise_direction (stdev->setup->ConfigTarget.BLT_TTY);

  node->ConfigColor.BLT_S2CF = 0x00000000;
  node->ConfigSource2.BLT_S2TY = (BLIT_COLOR_FORM_ARGB8888
                                  | BLIT_STY_COLOR_EXPAND_MSB);

  node->ConfigTarget.BLT_TXY
    = node->ConfigSource2.BLT_S2XY
    = (dst_pt.y << 16) | dst_pt.x;
  node->ConfigTarget.BLT_TSZ_S1SZ
    = node->ConfigSource2.BLT_S2SZ
    = (src.size.h << 16) | src.size.w;

  bdisp2_set_clip (stdev, &blt_cic, &blt_ins, &node->ConfigClip);
  bdisp2_update_config_static (stdrv->bdisp_shared, &node->ConfigStatic);

  set_matrix (node->ConfigIVMX.BLT_IVMX, bdisp_aq_RGB_2_VideoYCbCr601);

  node->ConfigGeneral.BLT_CIC = blt_cic;
  node->ConfigGeneral.BLT_INS = blt_ins;
  node->ConfigGeneral.BLT_ACK = blt_ack;

  bdisp2_finish_node (stdrv, stdev, node);

  return 0;
}

int
bdisp2_stretch_blit_shortcut_YCbCr422r (struct stm_bdisp2_driver_data * const stdrv,
                                        struct stm_bdisp2_device_data * const stdev,
                                        const stm_blitter_rect_t      * const src,
                                        const stm_blitter_rect_t      * const dst)
{
  return bdisp2_blit_shortcut_YCbCr422r (stdrv, stdev, dst, &dst->position);
}

int
bdisp2_blit2_shortcut_YCbCr422r (struct stm_bdisp2_driver_data * const stdrv,
                                 struct stm_bdisp2_device_data * const stdev,
                                 const stm_blitter_rect_t      * const src1,
                                 const stm_blitter_point_t     * const src2_pt,
                                 const stm_blitter_point_t     * const dst_pt)
{
  return bdisp2_blit_shortcut_YCbCr422r (stdrv, stdev, src1, dst_pt);
}

int
bdisp2_blit_shortcut_rgb32 (struct stm_bdisp2_driver_data * const stdrv,
                            struct stm_bdisp2_device_data * const stdev,
                            const stm_blitter_rect_t      * const src,
                            const stm_blitter_point_t     * const dst_pt)
{
  stm_blitter_rect_t dst = { .position = *dst_pt, .size = src->size };

  STM_BLITTER_ASSUME (!stdev->features.hw.rgb32);

  return bdisp2_blit_shortcut_real (stdrv, stdev, &dst, 0xff000000);
}

int
bdisp2_stretch_blit_shortcut_rgb32 (struct stm_bdisp2_driver_data * const stdrv,
                                    struct stm_bdisp2_device_data * const stdev,
                                    const stm_blitter_rect_t      * const src,
                                    const stm_blitter_rect_t      * const dst)
{
  return bdisp2_blit_shortcut_rgb32 (stdrv, stdev, dst, &dst->position);
}

int
bdisp2_blit2_shortcut_rgb32 (struct stm_bdisp2_driver_data * const stdrv,
                             struct stm_bdisp2_device_data * const stdev,
                             const stm_blitter_rect_t      * const src1,
                             const stm_blitter_point_t     * const src2_pt,
                             const stm_blitter_point_t     * const dst_pt)
{
  return bdisp2_blit_shortcut_rgb32 (stdrv, stdev, src1, dst_pt);
}


static void
bdisp2_blit_rotate_using_spans (struct stm_bdisp2_driver_data * const stdrv,
                                struct stm_bdisp2_device_data * const stdev,
                                const stm_blitter_rect_t      * const src,
                                stm_blitter_point_t           * const dst,
                                int                            dy_step)
{
  long w_remaining = src->size.w & stdev->features.hw.size_mask;
  long src_x = src->position.x;
  long src_y = src->position.y << 16;
  long src_h = src->size.h & stdev->features.hw.size_mask;
  int this_w = STM_BLITTER_ABS (dy_step);

  while (w_remaining)
    {
      stdev->setup->ConfigTarget.BLT_TXY = (dst->y << 16) | dst->x;
      stdev->setup->ConfigSource1.BLT_S1XY = stdev->setup->ConfigTarget.BLT_TXY;
      stdev->setup->ConfigTarget.BLT_TSZ_S1SZ = (this_w << 16) | src_h;

      stdev->setup->ConfigSource2.BLT_S2XY = src_y | src_x;
      stdev->setup->ConfigSource2.BLT_S2SZ = (src_h << 16) | this_w;

      bdisp_aq_copy_n_push_node (stdrv, stdev);
      bdisp_aq_clut_update_disable_if_unsafe (stdev);

      dst->y += dy_step;
      src_x += this_w;
      w_remaining -= this_w;

      this_w = STM_BLITTER_MIN (STM_BLITTER_ABS (dy_step), w_remaining);
    }
}

int
bdisp2_blit_rotate_90_270 (struct stm_bdisp2_driver_data * const stdrv,
                           struct stm_bdisp2_device_data * const stdev,
                           const stm_blitter_rect_t      * const _src,
                           const stm_blitter_point_t     * const _dst_pt)
{
  stm_blitter_rect_t src = *_src;
  stm_blitter_point_t dst_pt = *_dst_pt;
  bool requires_spans;
  int dy_step;

  if (!stdev->setup->blitstate.bFixedPoint)
    {
      if (stdev->setup->blitstate.srcxy_fixed_point)
        {
          src.position.x /= FIXED_POINT_ONE;
          src.position.y /= FIXED_POINT_ONE;
        }
      stm_blit_printd (BDISP_BLT, "%s (%lld,%lld-%lldx%lld -> %ld,%ld) %u°\n",
                       __func__,
                       RECTANGLE_VALS (&src), dst_pt.x, dst_pt.y,
                       stdev->setup->blitstate.rotate);
    }
  else
    {
      stm_blit_printd (BDISP_BLT,
                       "%s (%lld.%06lld,%lld.%06lld-%lld.%06lldx%lld.%06lld -> %lld.%06lld,%lld.%06lld) %u°\n",
                       __func__,
                       RECTANGLE_VALSf (&src), INT_VALf (dst_pt.x),
                       INT_VALf (dst_pt.y), stdev->setup->blitstate.rotate);
      src.position.x /= FIXED_POINT_ONE;
      src.position.y /= FIXED_POINT_ONE;
      src.size.w /= FIXED_POINT_ONE;
      src.size.h /= FIXED_POINT_ONE;
      dst_pt.x /= FIXED_POINT_ONE;
      dst_pt.y /= FIXED_POINT_ONE;
    }
  DUMP_INFO ();

  if (unlikely (!src.size.w || !src.size.h))
    return 0;
  if (check_size_constraints (stdev, src.position.x, src.position.y,
                              src.size.w, src.size.h))
    return -EINVAL;
  if (check_size_constraints (stdev, dst_pt.x, dst_pt.y, 1, 1))
    return -EINVAL;
  if ((stdev->setup->upsampled_src_nbpix_min > src.size.w)
      || (stdev->setup->upsampled_src_nbpix_min > src.size.h))
    return -EOPNOTSUPP;

  if (src.size.w % 16
      || src.size.h % 16)
    return -EOPNOTSUPP;

  if(bdisp2_is_suspended(stdrv))
    return -EAGAIN;

  /* read from top left to bottom right
     90°: writes from bottom left to top right
     270°: writes from top right to bottom left */
  if (stdev->setup->ConfigSource2.BLT_S2BA
      == stdev->setup->ConfigTarget.BLT_TBA)
    {
      stm_blitter_rect_t d_rect = { .position = dst_pt, .size = src.size };
      if (bdisp2_rectangles_intersect (&src, &d_rect))
        return -EINVAL;
    }

  requires_spans = (src.size.w > stdev->features.hw.rotate_buffer_length);

  if (stdev->setup->blitstate.rotate == 90)
    {
      /*dst_pt.x keep his value*/
      dst_pt.y += src.size.w - 1;
      dy_step = -stdev->features.hw.rotate_buffer_length;
    }
  else
    {
      dst_pt.x += src.size.h - 1;
      /*dst_pt.y keep his value*/
      dy_step = stdev->features.hw.rotate_buffer_length;
    }

  if (unlikely (!requires_spans))
    {
      stdev->setup->ConfigTarget.BLT_TSZ_S1SZ =
        (((src.size.w & stdev->features.hw.size_mask) << 16)
         | (src.size.h & stdev->features.hw.size_mask));
      stdev->setup->ConfigTarget.BLT_TXY   = (dst_pt.y << 16) | dst_pt.x;
      stdev->setup->ConfigSource1.BLT_S1XY = stdev->setup->ConfigTarget.BLT_TXY;
      stdev->setup->ConfigSource2.BLT_S2XY = ((src.position.y << 16)
                                              | src.position.x);
      stdev->setup->ConfigSource2.BLT_S2SZ =
        (((src.size.h & stdev->features.hw.size_mask) << 16)
         | (src.size.w & stdev->features.hw.size_mask));

      bdisp_aq_copy_n_push_node (stdrv, stdev);
      bdisp_aq_clut_update_disable_if_unsafe (stdev);
    }
  else
    bdisp2_blit_rotate_using_spans (stdrv, stdev, &src, &dst_pt, dy_step);

  return 0;
}


int
bdisp2_blit_nop (struct stm_bdisp2_driver_data * const stdrv,
                 struct stm_bdisp2_device_data * const stdev,
                 const stm_blitter_rect_t      * const src,
                 const stm_blitter_point_t     * const dst_pt)
{
  if (stdev->setup->blitstate.srcxy_fixed_point)
    stm_blit_printd (BDISP_BLT, "%s (%lld.%06lld,%lld.%06lld-%ldx%ld -> %ld,%ld)\n",
                     __func__,
                     INT_VALf (src->position.x), INT_VALf (src->position.y),
                     src->size.w, src->size.h, dst_pt->x, dst_pt->y);
  else if (!stdev->setup->blitstate.bFixedPoint)
    stm_blit_printd (BDISP_BLT, "%s (%lld,%lld-%lldx%lld -> %ld,%ld)\n",
                     __func__,
                     RECTANGLE_VALS (src), dst_pt->x, dst_pt->y);
  else
    stm_blit_printd (BDISP_BLT,
                     "%s (%lld.%06lld,%lld.%06lld-%lld.%06lldx%lld.%06lld -> %lld.%06lld,%lld.%06lld)\n",
                     __func__, RECTANGLE_VALSf (src),
                     INT_VALf (dst_pt->x), INT_VALf (dst_pt->y));
  DUMP_INFO ();

  if(bdisp2_is_suspended(stdrv))
    return -EAGAIN;

  return 0;
}

int
bdisp2_stretch_blit_nop (struct stm_bdisp2_driver_data * const stdrv,
                         struct stm_bdisp2_device_data * const stdev,
                         const stm_blitter_rect_t      * const src,
                         const stm_blitter_rect_t      * const dst)
{
  if (stdev->setup->blitstate.srcxy_fixed_point)
    stm_blit_printd (BDISP_STRETCH,
                     "%s (%lld.%06lld,%lld.%06lld-%ldx%ld -> %lld,%lld-%lldx%lld)\n",
                     __func__,
                     INT_VALf (src->position.x), INT_VALf (src->position.y),
                     src->size.w, src->size.h,
                     RECTANGLE_VALS (dst));
  else if (!stdev->setup->blitstate.bFixedPoint)
    stm_blit_printd (BDISP_STRETCH,
                     "%s (%lld,%lld-%lldx%lld -> %lld,%lld-%lldx%lld)\n",
                     __func__,
                     RECTANGLE_VALS (src), RECTANGLE_VALS (dst));
  else
    stm_blit_printd (BDISP_STRETCH,
                     "%s (%lld.%06lld,%lld.%06lld-%lld.%06lldx%lld.%06lld -> %lld.%06lld,%lld.%06lld-%lld.%06lldx%lld.%06lld)\n",
                     __func__,
                     RECTANGLE_VALSf (src), RECTANGLE_VALSf (dst));
  DUMP_INFO ();

  if(bdisp2_is_suspended(stdrv))
    return -EAGAIN;

  return 0;
}

int
bdisp2_blit2_nop (struct stm_bdisp2_driver_data * const stdrv,
                  struct stm_bdisp2_device_data * const stdev,
                  const stm_blitter_rect_t      * const src1,
                  const stm_blitter_point_t     * const src2_pt,
                  const stm_blitter_point_t     * const dst_pt)
{
  if (stdev->setup->blitstate.srcxy_fixed_point)
    stm_blit_printd (BDISP_STRETCH,
                     "%s (%lld.%06lld,%lld.%06lld-%ldx%ld + %ld,%ld -> %ld,%ld)\n",
                     __func__,
                     INT_VALf (src1->position.x), INT_VALf (src1->position.y),
                     src1->size.w, src1->size.h,
                     src2_pt->x, src2_pt->y, dst_pt->x, dst_pt->y);
  else if (!stdev->setup->blitstate.bFixedPoint)
    stm_blit_printd (BDISP_STRETCH,
                     "%s (%lld,%lld-%lldx%lld + %ld,%ld -> %ld,%ld)\n",
                     __func__,
                     RECTANGLE_VALS (src1),
                     src2_pt->x, src2_pt->y, dst_pt->x, dst_pt->y);
  else
    stm_blit_printd (BDISP_STRETCH,
                     "%s (%lld.%06lld,%lld.%06lld-%lld.%06lldx%lld.%06lld + %lld.%06lld,%lld.%06lld -> %lld.%06lld,%lld.%06lld)\n",
                     __func__,
                     RECTANGLE_VALSf (src1),
                     INT_VALf (src2_pt->x), INT_VALf (src2_pt->y),
                     INT_VALf (dst_pt->x), INT_VALf (dst_pt->y));
  DUMP_INFO ();

  if(bdisp2_is_suspended(stdrv))
    return -EAGAIN;

  return 0;
}


void
bdisp2_RGB32_init (struct stm_bdisp2_driver_data * const stdrv,
                   struct stm_bdisp2_device_data * const stdev,
                   uint32_t                       blt_tba,
                   uint16_t                       pitch,
                   const stm_blitter_rect_t      * const dst)
{
  uint32_t tba;
  uint32_t tty;
  uint32_t cty;
  uint32_t col;

  stm_blit_printd (BDISP_BLT, "%s\n", __func__);

  STM_BLITTER_ASSUME (!stdev->features.hw.rgb32);
  if (stdev->features.hw.rgb32)
    return;

  /* save state */
  tba = stdev->setup->ConfigTarget.BLT_TBA;
  tty = stdev->setup->ConfigTarget.BLT_TTY;
  cty = stdev->setup->drawstate.color_ty;
  col = stdev->setup->drawstate.color;

  stdev->setup->ConfigTarget.BLT_TBA = blt_tba;
  stdev->setup->ConfigTarget.BLT_TTY = (BLIT_COLOR_FORM_ARGB8888
                                        | BLIT_TY_FULL_ALPHA_RANGE
                                        | pitch);
  stdev->setup->drawstate.color_ty = stdev->setup->ConfigTarget.BLT_TTY;
  stdev->setup->drawstate.color    = 0xff000000;

  bdisp2_fill_rect_simple (stdrv, stdev, dst);

  /* restore state */
  stdev->setup->drawstate.color = col;
  stdev->setup->drawstate.color_ty = cty;
  stdev->setup->ConfigTarget.BLT_TTY = tty;
  stdev->setup->ConfigTarget.BLT_TBA = tba;

  bdisp2_emit_commands (stdrv, stdev, false);
}

/* fixup for rgb32, force the alpha to all 0xff again. */
void
bdisp2_RGB32_fixup (struct stm_bdisp2_driver_data * const stdrv,
                    struct stm_bdisp2_device_data * const stdev,
                    uint32_t                       blt_tba,
                    uint16_t                       pitch,
                    const stm_blitter_rect_t      * const dst)
{
  struct _BltNode_RGB32fixup *node;

  stm_blit_printd (BDISP_BLT, "%s\n", __func__);

  STM_BLITTER_ASSUME (!stdev->features.hw.rgb32);
  if (stdev->features.hw.rgb32)
    return;

  node = bdisp2_get_new_node (stdrv, stdev);

  node->ConfigGeneral.BLT_CIC = BLIT_CIC_BLT_NODE_RGB32fixup;
  node->ConfigGeneral.BLT_INS = (BLIT_INS_SRC1_MODE_MEMORY
                                 | BLIT_INS_ENABLE_PLANEMASK);
  node->ConfigGeneral.BLT_ACK = BLIT_ACK_ROP | BLIT_ACK_ROP_SET;

  node->ConfigTarget.BLT_TBA
    = node->ConfigSource1.BLT_S1BA
    = blt_tba;
  node->ConfigTarget.BLT_TTY
    = node->ConfigSource1.BLT_S1TY
    = BLIT_COLOR_FORM_ARGB8888 | BLIT_TY_FULL_ALPHA_RANGE | pitch;
  node->ConfigTarget.BLT_TXY
    = node->ConfigSource1.BLT_S1XY
    = (dst->position.y << 16) | dst->position.x;
  node->ConfigTarget.BLT_TSZ_S1SZ = (dst->size.h << 16) | dst->size.w;

  node->ConfigFilters.BLT_PMK = 0xff000000;

  bdisp2_update_num_ops (stdrv, stdev, &node->ConfigGeneral.BLT_INS);

  bdisp2_update_config_static (stdrv->bdisp_shared, &node->ConfigStatic);

  bdisp2_finish_node (stdrv, stdev, node);

  bdisp2_emit_commands (stdrv, stdev, false);
  bdisp2_engine_sync (stdrv, stdev);
}
