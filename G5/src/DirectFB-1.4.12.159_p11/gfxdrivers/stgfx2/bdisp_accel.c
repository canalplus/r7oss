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


#include <string.h>
#include <asm/unistd.h>

#include <sys/ioctl.h>
#include <sys/mman.h>

#include <linux/fb.h>
#include <linux/stmfb.h>

#include <directfb.h>
#include <dfb_types.h>

#include <core/coretypes.h>
#include <core/gfxcard.h>
#include <core/palette.h>
#include <core/surface.h>

#include <gfx/convert.h>


#include "stgfx2_features.h"
#include "bdisp_features.h"
#include "bdisp_accel.h"
#include "bdisp_accel_types.h"
#define __IS_BDISP_ACCEL_C__
#include "bdisp_tables.h"
#undef __IS_BDISP_ACCEL_C__
#include "bdisp_registers.h"
#include "bdisp_state.h"
#include "stm_gfxdriver.h"



D_DEBUG_DOMAIN (BDISP_BLT,           "BDisp/blt",         "STM BDispII Drawing Engine");
D_DEBUG_DOMAIN (BDISP_STRETCH,       "BDisp/strtch",      "STM BDispII stretch blits");
D_DEBUG_DOMAIN (BDISP_STRETCH_SPANS, "BDisp/strtch/spns", "STM BDispII stretch blits (span generation)");
D_DEBUG_DOMAIN (BDISP_STRETCH_FILT,  "BDisp/fltr",        "STM BDispII blit filters");
D_DEBUG_DOMAIN (BDISP_NODES,         "BDisp/nodes",       "STM BDispII blit nodes");

#if D_DEBUG_ENABLED
#define DUMP_INFO() \
  ( {                                                               \
    const STMFBBDispSharedArea __attribute__((unused)) * const shared = stdrv->bdisp_shared; \
    D_DEBUG_AT (BDISP_NODES,                                        \
                "  -> %srunning CTL: %.8x IP/LNA/STA: %lu/%lu/%lu"  \
                " next/last %lu/%lu\n",                             \
                shared->bdisp_running ? "" : "not ",                \
                bdisp_aq_GetAQReg (stdrv, stdev, BDISP_AQ_CTL),     \
                (bdisp_aq_GetAQReg (stdrv, stdev, BDISP_AQ_IP) - shared->nodes_phys) / sizeof (struct _BltNode_Complete),  \
                (bdisp_aq_GetAQReg (stdrv, stdev, BDISP_AQ_LNA) - shared->nodes_phys) / sizeof (struct _BltNode_Complete), \
                (bdisp_aq_GetAQReg (stdrv, stdev, BDISP_AQ_STA) - shared->nodes_phys) / sizeof (struct _BltNode_Complete), \
                shared->next_free, shared->last_free);              \
  } )

static void
_dump_node (const struct _BltNodeGroup00 * const config_general)
{
  const struct _BltNodeGroup01 * const config_target =
    (struct _BltNodeGroup01 *) (config_general + 1);
  const void *pos = config_target + 1;

  D_DEBUG_AT (BDISP_NODES,
              "BLT_NIP  BLT_CIC  BLT_INS  BLT_ACK : %.8x %.8x %.8x %.8x\n",
              config_general->BLT_NIP, config_general->BLT_CIC,
              config_general->BLT_INS, config_general->BLT_ACK);
  D_DEBUG_AT (BDISP_NODES,
              "BLT_TBA  BLT_TTY  BLT_TXY  BLT_TSZ : %.8x %.8x %.8x %.8x\n",
              config_target->BLT_TBA, config_target->BLT_TTY,
              config_target->BLT_TXY, config_target->BLT_TSZ_S1SZ);

  if (config_general->BLT_CIC & CIC_NODE_COLOR)
    {
      const struct _BltNodeGroup02 * const config_color = pos;
      pos = config_color + 1;

      D_DEBUG_AT (BDISP_NODES,
                  "BLT_S1CF BLT_S2CF                  : %.8x %.8x\n",
                  config_color->BLT_S1CF, config_color->BLT_S2CF);
    }
  if (config_general->BLT_CIC & CIC_NODE_SOURCE1)
    {
      const struct _BltNodeGroup03 * const config_source1 = pos;
      pos = config_source1 + 1;

      D_DEBUG_AT (BDISP_NODES,
                  "BLT_S1BA BLT_S1TY BLT_S1XY         : %.8x %.8x %.8x\n",
                  config_source1->BLT_S1BA, config_source1->BLT_S1TY,
                  config_source1->BLT_S1XY);
    }
  if (config_general->BLT_CIC & CIC_NODE_SOURCE2)
    {
      const struct _BltNodeGroup04 * const config_source2 = pos;
      pos = config_source2 + 1;

      D_DEBUG_AT (BDISP_NODES,
                  "BLT_S2BA BLT_S2TY BLT_S2XY BLT_S2SZ: %.8x %.8x %.8x %.8x\n",
                  config_source2->BLT_S2BA, config_source2->BLT_S2TY,
                  config_source2->BLT_S2XY, config_source2->BLT_S2SZ);
    }
  if (config_general->BLT_CIC & CIC_NODE_SOURCE3)
    {
      const struct _BltNodeGroup05 * const config_source3 = pos;
      pos = config_source3 + 1;

      D_DEBUG_AT (BDISP_NODES,
                  "BLT_S3BA BLT_S3TY BLT_S3XY BLT_S3SZ: %.8x %.8x %.8x %.8x\n",
                  config_source3->BLT_S3BA, config_source3->BLT_S3TY,
                  config_source3->BLT_S3XY, config_source3->BLT_S3SZ);
    }
  if (config_general->BLT_CIC & CIC_NODE_CLIP)
    {
      const struct _BltNodeGroup06 * const config_clip = pos;
      pos = config_clip + 1;

      D_DEBUG_AT (BDISP_NODES,
                  "BLT_CWO  BLT_CWS                   : %.8x %.8x\n",
                  config_clip->BLT_CWO, config_clip->BLT_CWS);
    }
  if (config_general->BLT_CIC & CIC_NODE_CLUT)
    {
      const struct _BltNodeGroup07 * const config_clut = pos;
      pos = config_clut + 1;

      D_DEBUG_AT (BDISP_NODES,
                  "BLT_CCO  BLT_CML                   : %.8x %.8x\n",
                  config_clut->BLT_CCO, config_clut->BLT_CML);
    }
  if (config_general->BLT_CIC & CIC_NODE_FILTERS)
    {
      const struct _BltNodeGroup08 * const config_filters = pos;
      pos = config_filters + 1;

      D_DEBUG_AT (BDISP_NODES,
                  "BLT_FCTL BLT_PMK                   : %.8x %.8x\n",
                  config_filters->BLT_FCTL_RZC, config_filters->BLT_PMK);
    }
  if (config_general->BLT_CIC & CIC_NODE_2DFILTERSCHR)
    {
      const struct _BltNodeGroup09 * const config_2dfilterschr = pos;
      pos = config_2dfilterschr + 1;

      D_DEBUG_AT (BDISP_NODES,
                  "BLT_RSF  BLT_RZI  BLT_HFP  BLT_VFP : %.8x %.8x %.8x %.8x\n",
                  config_2dfilterschr->BLT_RSF, config_2dfilterschr->BLT_RZI,
                  config_2dfilterschr->BLT_HFP, config_2dfilterschr->BLT_VFP);
    }
  if (config_general->BLT_CIC & CIC_NODE_2DFILTERSLUMA)
    {
      const struct _BltNodeGroup10 * const config_2dfiltersluma = pos;
      pos = config_2dfiltersluma + 1;

      D_DEBUG_AT (BDISP_NODES,
                  "BLT_YRSF BLT_YRZI BLT_YHFP BLT_YVFP: %.8x %.8x %.8x %.8x\n",
                  config_2dfiltersluma->BLT_Y_RSF, config_2dfiltersluma->BLT_Y_RZI,
                  config_2dfiltersluma->BLT_Y_HFP, config_2dfiltersluma->BLT_Y_VFP);
    }
  if (config_general->BLT_CIC & CIC_NODE_FLICKER)
    {
      const struct _BltNodeGroup11 * const config_flicker = pos;
      pos = config_flicker + 1;

      D_DEBUG_AT (BDISP_NODES,
                  "BLT_FF0  BLT_FF1  BLT_FF2  BLT_FF3 : %.8x %.8x %.8x %.8x\n",
                  config_flicker->BLT_FF0, config_flicker->BLT_FF1,
                  config_flicker->BLT_FF2, config_flicker->BLT_FF3);
    }
  if (config_general->BLT_CIC & CIC_NODE_COLORKEY)
    {
      const struct _BltNodeGroup12 * const config_ckey = pos;
      pos = config_ckey + 1;

      D_DEBUG_AT (BDISP_NODES,
                  "BLT_KEY1 BLT_KEY2                  : %.8x %.8x\n",
                  config_ckey->BLT_KEY1, config_ckey->BLT_KEY2);
    }
  if (config_general->BLT_CIC & CIC_NODE_STATIC)
    {
      const struct _BltNodeGroup14 * const config_static = pos;
      pos = config_static + 1;

      D_DEBUG_AT (BDISP_NODES,
                  "BLT_SAR  BLT_USR                   : %.8x %.8x\n",
                  config_static->BLT_SAR, config_static->BLT_USR);
    }
  if (config_general->BLT_CIC & CIC_NODE_IVMX)
    {
      const struct _BltNodeGroup15 * const config_ivmx = pos;
      pos = config_ivmx + 1;

      D_DEBUG_AT (BDISP_NODES,
                  "BLT_IVMX BLT_IVMX BLT_IVMX BLT_IVMX: %.8x %.8x %.8x %.8x\n",
                  config_ivmx->BLT_IVMX0, config_ivmx->BLT_IVMX1,
                  config_ivmx->BLT_IVMX2, config_ivmx->BLT_IVMX3);
    }
  if (config_general->BLT_CIC & CIC_NODE_OVMX)
    {
      const struct _BltNodeGroup16 * const config_ovmx = pos;
      pos = config_ovmx + 1;

      D_DEBUG_AT (BDISP_NODES,
                  "BLT_OVMX BLT_OVMX BLT_OVMX BLT_OVMX: %.8x %.8x %.8x %.8x\n",
                  config_ovmx->BLT_OVMX0, config_ovmx->BLT_OVMX1,
                  config_ovmx->BLT_OVMX2, config_ovmx->BLT_OVMX3);
    }
  if (config_general->BLT_CIC & CIC_NODE_VC1R)
    {
      const struct _BltNodeGroup18 * const config_vc1 = pos;
      pos = config_vc1 + 1;

      D_DEBUG_AT (BDISP_NODES,
                  "BLT_VC1R                           : %.8x\n",
                  config_vc1->BLT_VC1R);
    }
}
#else
#define DUMP_INFO() do {} while (0)
static inline void
_dump_node (const struct _BltNodeGroup00 * const config_general) { }
#endif



struct _BltNode_Common
{
  struct _BltNodeGroup00 ConfigGeneral; // 4 -> General
  struct _BltNodeGroup01 ConfigTarget; // 4 -> Target
  struct _BltNodeGroup02 ConfigColor; // 2 -> Color
  struct _BltNodeGroup03 ConfigSource1; // 4 -> Source1
  // total: 14 * 4 = 56
#define CIC_BLT_NODE_COMMON   (0 \
                               | CIC_NODE_COLOR   \
                               | CIC_NODE_SOURCE1 \
                              )
};


struct _BltNode_Fast
{
  struct _BltNode_Common common; // 14
#ifdef STGFX2_IMPLEMENT_WAITSERIAL
  struct _BltNodeGroup14 ConfigStatic; // 2
#endif /* STGFX2_IMPLEMENT_WAITSERIAL */
  // total: 14(+2) * 4 = 56(+8)
#define CIC_BLT_NODE_FAST   (0 \
                             | CIC_BLT_NODE_COMMON \
                             | CIC_NODE_STATIC     \
                            )
};


struct _BltNode_YCbCr422r_shortcut
{
  struct _BltNodeGroup00 ConfigGeneral;  // 4
  struct _BltNodeGroup01 ConfigTarget;   // 4
  struct _BltNodeGroup02 ConfigColor;    // 2
  struct _BltNodeGroup04 ConfigSource2;  // 4
#ifdef STGFX2_SUPPORT_HW_CLIPPING
  struct _BltNodeGroup06 ConfigClip;     // 2
#endif /* STGFX2_SUPPORT_HW_CLIPPING */
#ifdef STGFX2_IMPLEMENT_WAITSERIAL
  struct _BltNodeGroup14 ConfigStatic;   // 2
#endif /* STGFX2_IMPLEMENT_WAITSERIAL */
  struct _BltNodeGroup15 ConfigIVMX;     // 4
  // total: 18(+2+2) * 4 = 72(+8+8)
#define CIC_BLT_NODE_YCbCr422r_SHORTCUT (0 \
                                         | CIC_NODE_COLOR   \
                                         | CIC_NODE_SOURCE2 \
                                         | CIC_NODE_CLIP    \
                                         | CIC_NODE_STATIC  \
                                         | CIC_NODE_IVMX    \
                                        )
};


struct _BltNode_RGB32fixup
{
  struct _BltNodeGroup00 ConfigGeneral; // 4 -> General
  struct _BltNodeGroup01 ConfigTarget; // 4 -> Target
  struct _BltNodeGroup03 ConfigSource1; // 4 -> Source1
  struct _BltNodeGroup08 ConfigFilters; // 2
#ifdef STGFX2_IMPLEMENT_WAITSERIAL
  struct _BltNodeGroup14 ConfigStatic; // 2
#endif /* STGFX2_IMPLEMENT_WAITSERIAL */
  // total: 11(+2) * 4 = 44(+8)
#define CIC_BLT_NODE_RGB32fixup (0 \
                                 | CIC_NODE_SOURCE1 \
                                 | CIC_NODE_FILTERS \
                                 | CIC_NODE_STATIC  \
                                )
};


struct _BltNode_Complete
{
  struct _BltNode_Common common;  // 14

  struct _BltNodeGroup04 ConfigSource2;     //  4
  struct _BltNodeGroup05 ConfigSource3;     //  4
  struct _BltNodeGroup06 ConfigClip;        //  2
  struct _BltNodeGroup07 ConfigClut;        //  2
  struct _BltNodeGroup08 ConfigFilters;     //  2
  struct _BltNodeGroup09 ConfigFiltersChr;  //  4
  struct _BltNodeGroup10 ConfigFiltersLuma; //  4
  struct _BltNodeGroup11 ConfigFlicker;     //  4
  struct _BltNodeGroup12 ConfigColorkey;    //  2
  struct _BltNodeGroup13 ConfigXYL;         //  2
  struct _BltNodeGroup14 ConfigStatic;      //  2
  struct _BltNodeGroup15 ConfigIVMX;        //  4
  struct _BltNodeGroup16 ConfigOVMX;        //  4
  struct _BltNodeGroup17 ConfigPaceDot;     //  4
  struct _BltNodeGroup18 ConfigVC1;         //  2
  struct _BltNodeGroup19 ConfigGradient;    //  2
  // total: 62 * 4 = 248
};


struct _BltNode_FullFill
{
  struct _BltNode_Common common; // 14

  struct _BltNodeGroup04 ConfigSource2;  // 4
  /* the following groups are not necessarily used, thus don't rely on
     a 1:1 mapping of this struct in memory... */
#ifdef STGFX2_SUPPORT_HW_CLIPPING
  struct _BltNodeGroup06 ConfigClip;     // 2
#endif /* STGFX2_SUPPORT_HW_CLIPPING */
  struct _BltNodeGroup07 ConfigClut;     // 2
  struct _BltNodeGroup08 ConfigFilters;  // 2
  struct _BltNodeGroup12 ConfigColorkey; // 2
#ifdef STGFX2_IMPLEMENT_WAITSERIAL
  struct _BltNodeGroup14 ConfigStatic;   // 2
#endif /* STGFX2_IMPLEMENT_WAITSERIAL */
  struct _BltNodeGroup15 ConfigIVMX;     // 4
  // max. total: 28(+2+2) * 4 = 112(+8+8)
};


static inline u32
bdisp_aq_GetReg (const STGFX2DriverData * const stdrv,
                 u16                     offset)
{
  return *(volatile u32 *) (stdrv->mmio_base + 0xa00 + offset);
}

static inline void
bdisp_aq_SetReg (STGFX2DriverData * const stdrv,
                 u16               offset,
                 u32               value)
{
  *(volatile u32 *) (stdrv->mmio_base + 0xa00 + offset) = value;
}

static inline u32
bdisp_aq_GetAQReg (const STGFX2DriverData * const stdrv,
                   const STGFX2DeviceData * const stdev,
                   u16                     offset)
{
  return bdisp_aq_GetReg (stdrv,
                          offset + (BDISP_AQ1_BASE + (stdev->aq_index * 0x10))
                         );
}

static inline void
bdisp_aq_SetAQReg (STGFX2DriverData * const stdrv,
                   const STGFX2DeviceData * const stdev,
                   u16               offset,
                   u32               value)
{
  bdisp_aq_SetReg (stdrv,
                   offset + (BDISP_AQ1_BASE + (stdev->aq_index * 0x10)),
                   value);
}


#define set_matrix(dst, src) \
  ({ \
    dst ## 0 = src[0]; \
    dst ## 1 = src[1]; \
    dst ## 2 = src[2]; \
    dst ## 3 = src[3]; \
  })

DFBResult
bdisp_aq_initialize (CoreGraphicsDevice * const device,
                     GraphicsDeviceInfo * const device_info,
                     STGFX2DriverData   * const stdrv,
                     STGFX2DeviceData   * const stdev)
{
  struct _BltNode_Complete *node;
  unsigned long             nip;
  STMFBBDispSharedArea     *shared;

  if (ioctl (stdrv->fd, STMFBIO_GET_SHARED_AREA, &stdev->bdisp_shared_info) < 0
      || !stdev->bdisp_shared_info.physical
      || !stdev->bdisp_shared_info.size)
    return DFB_NOVIDEOMEMORY;

  D_INFO ("BDisp/BLT: shared area phys/size: 0x%.8x/%u\n",
          stdev->bdisp_shared_info.physical, stdev->bdisp_shared_info.size);

  /* the actual shared area */
  shared = stdrv->bdisp_shared = mmap (NULL,
                                       sysconf (_SC_PAGESIZE),
                                       PROT_READ | PROT_WRITE,
                                       MAP_SHARED, stdrv->fd,
                                       stdev->bdisp_shared_info.physical);
  if (stdrv->bdisp_shared == MAP_FAILED)
    {
      D_PERROR ("BDisp/BLT: Could not map shared area!\n");
      stdrv->bdisp_shared = NULL;
      return DFB_INIT;
    }

  /* versions of stmfb < 0025 didn't report any version of the shared area
     struct, but luckily initialized all the memory to 0xab. Thus we can check
     for that and remain binary compatible with those versions of stmfb. */
  if (shared->version == 0xabababab)
    {
      /* some trickery - we are not really allowed to do this... */
      *((unsigned long *) &shared->version) = 0;
      *((enum stm_blitter_device * ) &shared->device) = STM_BLITTER_VERSION_7200c2_7111_7141_7105;
      /* those two were introduced in stmfb 0026, but luckily the layout of
         the shared area struct changed, so we can just initialize them. */
      shared->lock.counter = 0;
      shared->locked_by = 0;
    }

  /* figure out available node size in bytes and align to a multiple of the
     maximum size of a node */
  stdev->usable_nodes_size = (shared->nodes_size / sizeof (*node)) * sizeof (*node);
  stdev->node_irq_delay = (stdev->usable_nodes_size / sizeof (*node)) / 4;

  /* figure out what specific implementation this is, and which features it
     supports */
  if (!bdisp_hw_features_set_bdisp (shared->device, &stdev->features))
    {
      D_INFO ("BDisp/BLT: blitter is %d (unknown)\n", shared->device);
      return DFB_UNSUPPORTED;
    }
  D_INFO ("BDisp/BLT: blitter is a '%s' (%d)\n",
          stdev->features.name, shared->device);

  /* initialize shared area */
  D_ASSUME (shared->bdisp_running == 0);
  /* important stuff */
  shared->bdisp_running
    = shared->updating_lna = 0;
  shared->prev_set_lna
    = shared->last_lut = 0;
  /* stats only */
  shared->num_idle = shared->num_irqs = shared->num_lna_irqs
    = shared->num_node_irqs = shared->num_ops_hi = shared->num_ops_lo
    = shared->num_starts
    = shared->num_wait_idle = shared->num_wait_next
    = 0;


  D_INFO ("BDisp/BLT: nodes area phys/size:  0x%.8lx/%lu (%lu usable -> %lu nodes)"
          " -> ends @ 0x%.8lx\n",
          shared->nodes_phys, shared->nodes_size, stdev->usable_nodes_size,
          stdev->usable_nodes_size / sizeof (*node),
          shared->nodes_phys + stdev->usable_nodes_size);

  /* the DMA memory (nodelist, filters) */
  stdrv->bdisp_nodes = mmap (NULL,
                             (stdev->bdisp_shared_info.size
                              - sysconf (_SC_PAGESIZE)),
                             PROT_READ | PROT_WRITE,
                             MAP_SHARED, stdrv->fd,
                             (stdev->bdisp_shared_info.physical
                              + sysconf (_SC_PAGESIZE)));
  if (stdrv->bdisp_nodes == MAP_FAILED)
    {
      D_PERROR ("BDisp/BLT: Could not map nodes area!\n");
      stdrv->bdisp_nodes = NULL;
      return DFB_INIT;
    }

  D_INFO ("BDisp/BLT: shared area/nodes mmap()ed to %p / %p\n",
          shared, stdrv->bdisp_nodes);

  /* set nodelist to some sane defaults */
  memset ((void *) stdrv->bdisp_nodes, 0, stdev->usable_nodes_size);
  /* prelink all the buffer nodes around to make a circular list */
  node = (void *) stdrv->bdisp_nodes;
  nip = shared->nodes_phys + sizeof (*node);
  while (((void *) node) < (stdrv->bdisp_nodes + stdev->usable_nodes_size - sizeof (*node)))
    {
      node->common.ConfigGeneral.BLT_NIP = nip;
      node->common.ConfigGeneral.BLT_CIC = CIC_BLT_NODE_COMMON;
      node->common.ConfigGeneral.BLT_INS = BLIT_INS_SRC1_MODE_DISABLED;

      ++node;
      nip += sizeof (*node);
    }
  /* last node points back to the first */
  node->common.ConfigGeneral.BLT_NIP = shared->nodes_phys;
  node->common.ConfigGeneral.BLT_CIC = CIC_BLT_NODE_COMMON;
  node->common.ConfigGeneral.BLT_INS = BLIT_INS_SRC1_MODE_DISABLED;


  /* we need some memory for some CLUTs and H/V filter coefficients, too.
     The CLUTs need to be aligned to a 16byte address and the filters to an
     8byte address. We have two types of CLUT here - a) a usual CLUT,
     and b) CLUTs used for various types of color correction, e.g. for
     supporting the RGB32 pixel format to force the alpha to be 0xff. */
#define CLUT_SIZE (256 * sizeof (u32))
  {
  unsigned int   total_reserve;
  unsigned int   siz_clut;
  unsigned int   siz_8x8_filter;
  unsigned int   siz_5x8_filter;

  Stgfx2Clut     palette;
  unsigned long *clut_virt;
  int            ofs_clut;
  unsigned char *filter_virt;
  unsigned int   ofs_8x8_filter;
  unsigned int   ofs_5x8_filter;

  int            i;

  siz_clut       = CLUT_SIZE * SG2C_COUNT;
  siz_8x8_filter = D_ARRAY_SIZE (bdisp_aq_blitter_8x8_filters) * BDISP_AQ_BLIT_FILTER_8X8_TABLE_SIZE;
  siz_5x8_filter = D_ARRAY_SIZE (bdisp_aq_blitter_5x8_filters) * BDISP_AQ_BLIT_FILTER_5X8_TABLE_SIZE;

  total_reserve = siz_clut + siz_8x8_filter + siz_5x8_filter + 5;

  /* CLUT needs to be 16byte aligned */
  if (device_info->limits.surface_byteoffset_alignment < 16)
    device_info->limits.surface_byteoffset_alignment = 16;

  /* but else it's relatively simple, just reserve the memory... */
  ofs_clut = dfb_gfxcard_reserve_memory (device, total_reserve);
  if (ofs_clut <= 0)
    {
      D_ERROR ("BDisp/BLT: couldn't reserve %u bytes for CLUTs & H/V filter"
               " memory!\n", total_reserve);
      return DFB_NOVIDEOMEMORY;
    }

  D_INFO ("BDisp/BLT: reserved %u bytes for CLUT & filters @ "
          "0x%.8lx + 0x%.8x = 0x%.8lx\n",
          total_reserve,
          dfb_gfxcard_memory_physical (device, 0), ofs_clut,
          dfb_gfxcard_memory_physical (device, ofs_clut));

  /* ...and prepare the color correction LUTs. The alpha range for LUTs is
     0 ... 128 (not 127); this includes both, the lookup and the final value!

  1) RGB32 -> Aout = 1, RGBout = RGBin */
  stdev->clut_phys[SG2C_ONEALPHA_RGB] = dfb_gfxcard_memory_physical (device,
                                                                     ofs_clut);
  clut_virt = dfb_gfxcard_memory_virtual (device, ofs_clut);
  for (i = 0; i <= 0x80; ++i)
    clut_virt[i] = PIXEL_ARGB (0x80, i, i, i);
  for (; i < 256; ++i)
    clut_virt[i] = PIXEL_ARGB (0, i, i, i);
  ofs_clut += CLUT_SIZE;

  /* 2) Aout = 1 - Ain, RGBout = 0 */
  stdev->clut_phys[SG2C_INVALPHA_ZERORGB] = dfb_gfxcard_memory_physical (device,
                                                                         ofs_clut);
  clut_virt = dfb_gfxcard_memory_virtual (device, ofs_clut);
  for (i = 0; i <= 0x80; ++i)
    clut_virt[i] = PIXEL_ARGB (0x80 - i, 0, 0, 0);
  memset (&clut_virt[i], 0x00, CLUT_SIZE - (i * sizeof (u32)));
  ofs_clut += CLUT_SIZE;

  /* 3) Aout = Ain, RGBout = 0 */
  stdev->clut_phys[SG2C_ALPHA_ZERORGB] = dfb_gfxcard_memory_physical (device,
                                                                      ofs_clut);
  clut_virt = dfb_gfxcard_memory_virtual (device, ofs_clut);
  for (i = 0; i <= 0x80; ++i)
    clut_virt[i] = PIXEL_ARGB (i, 0, 0, 0);
  memset (&clut_virt[i], 0x00, CLUT_SIZE - (i * sizeof (u32)));
  ofs_clut += CLUT_SIZE;

  /* 4) Aout = 0, RGBout = RGBin */
  stdev->clut_phys[SG2C_ZEROALPHA_RGB] = dfb_gfxcard_memory_physical (device,
                                                                      ofs_clut);
  clut_virt = dfb_gfxcard_memory_virtual (device, ofs_clut);
  for (i = 0; i < 256; ++i)
    clut_virt[i] = PIXEL_ARGB (0, i, i, i);
  ofs_clut += CLUT_SIZE;

  /* 5) Aout = 0, RGBout = 0 */
  stdev->clut_phys[SG2C_ZEROALPHA_ZERORGB] = dfb_gfxcard_memory_physical (device,
                                                                          ofs_clut);
  clut_virt = dfb_gfxcard_memory_virtual (device, ofs_clut);
  memset (clut_virt, 0, CLUT_SIZE);
  ofs_clut += CLUT_SIZE;

  /* 6) Aout = 1, RGBout = 0 */
  stdev->clut_phys[SG2C_ONEALPHA_ZERORGB] = dfb_gfxcard_memory_physical (device,
                                                                         ofs_clut);
  clut_virt = dfb_gfxcard_memory_virtual (device, ofs_clut);
  for (i = 0; i <= 0x80; ++i)
    clut_virt[i] = PIXEL_ARGB (0x80, 0, 0, 0);
  memset (&clut_virt[i], 0x00, CLUT_SIZE - (i * sizeof (u32)));
  ofs_clut += CLUT_SIZE;


  /* dynamic palettes */
  /* Aout = coloralpha */
  stdev->clut_offset[SG2C_COLORALPHA] = ofs_clut;
  stdev->clut_phys[SG2C_COLORALPHA] = dfb_gfxcard_memory_physical (device,
                                                                   ofs_clut);
  stdrv->clut_virt[SG2C_COLORALPHA] = dfb_gfxcard_memory_virtual (device,
                                                                  ofs_clut);
  ofs_clut += CLUT_SIZE;

  /* Aout = invcoloralpha */
  stdev->clut_offset[SG2C_INVCOLORALPHA] = ofs_clut;
  stdev->clut_phys[SG2C_INVCOLORALPHA] = dfb_gfxcard_memory_physical (device,
                                                                      ofs_clut);
  stdrv->clut_virt[SG2C_INVCOLORALPHA] = dfb_gfxcard_memory_virtual (device,
                                                                     ofs_clut);
  ofs_clut += CLUT_SIZE;

  /* Aout = coloralpha */
  stdev->clut_offset[SG2C_ALPHA_MUL_COLORALPHA] = ofs_clut;
  stdev->clut_phys[SG2C_ALPHA_MUL_COLORALPHA] = dfb_gfxcard_memory_physical (device,
                                                                             ofs_clut);
  stdrv->clut_virt[SG2C_ALPHA_MUL_COLORALPHA] = dfb_gfxcard_memory_virtual (device,
                                                                            ofs_clut);
  ofs_clut += CLUT_SIZE;

  /* Aout = invcoloralpha */
  stdev->clut_offset[SG2C_INV_ALPHA_MUL_COLORALPHA] = ofs_clut;
  stdev->clut_phys[SG2C_INV_ALPHA_MUL_COLORALPHA] = dfb_gfxcard_memory_physical (device,
                                                                                 ofs_clut);
  stdrv->clut_virt[SG2C_INV_ALPHA_MUL_COLORALPHA] = dfb_gfxcard_memory_virtual (device,
                                                                                ofs_clut);
  ofs_clut += CLUT_SIZE;

  /* now for the 'normal' LUT. We need to remember the virtual address so we
     can update it if necessary, of course. */
  stdev->clut_offset[SG2C_NORMAL] = ofs_clut;
  stdev->clut_phys[SG2C_NORMAL] = dfb_gfxcard_memory_physical (device,
                                                               ofs_clut);
  stdrv->clut_virt[SG2C_NORMAL] = dfb_gfxcard_memory_virtual (device, ofs_clut);
  ofs_clut += CLUT_SIZE;

  for (palette = SG2C_NORMAL; palette < SG2C_DYNAMIC_COUNT; ++palette)
    D_INFO ("DirectFB/stgfx2: CLUT mmap()ed to %p\n",
            stdrv->clut_virt[palette]);


  /* the filter coeffs need to be copied into the filter area and later
     don't ever need to be touched again. Except that we need to
     remember the physical address to tell the BDisp later. */
  ofs_8x8_filter = ofs_clut;
  stdev->filter_8x8_phys = dfb_gfxcard_memory_physical (device, ofs_8x8_filter);
  filter_virt = dfb_gfxcard_memory_virtual  (device, ofs_8x8_filter);
  for (i = 0; i < D_ARRAY_SIZE (bdisp_aq_blitter_8x8_filters); ++i)
    memcpy (&filter_virt[i * BDISP_AQ_BLIT_FILTER_8X8_TABLE_SIZE],
            bdisp_aq_blitter_8x8_filters[i].filter_coeffs,
            BDISP_AQ_BLIT_FILTER_8X8_TABLE_SIZE);

  /* same for vertical filter coeffs. */
  ofs_5x8_filter = ofs_8x8_filter + siz_8x8_filter;
  stdev->filter_5x8_phys = dfb_gfxcard_memory_physical (device, ofs_5x8_filter);
  filter_virt = dfb_gfxcard_memory_virtual  (device, ofs_5x8_filter);
  for (i = 0; i < D_ARRAY_SIZE (bdisp_aq_blitter_5x8_filters); ++i)
    memcpy (&filter_virt[i * BDISP_AQ_BLIT_FILTER_5X8_TABLE_SIZE],
            bdisp_aq_blitter_5x8_filters[i].filter_coeffs,
            BDISP_AQ_BLIT_FILTER_5X8_TABLE_SIZE);
  }

  /* the flicker filter coefficients don't ever change and are directly
     mapped to registers, no physical memory is needed. */
  stdev->ConfigFlicker = bdisp_aq_config_flicker_filter;

  /* we don't use the static addresses registers, so we need to initialize
     this one to 0, because we _do_ use the BLT_USR from the same register
     group. */
  stdev->ConfigStatic.BLT_SAR = 0;

  stdev->ConfigColor.BLT_S1CF = 0;
  stdev->ConfigColor.BLT_S2CF = 0;

  stdev->ConfigFilters.BLT_PMK = 0x00ffffff;

  set_matrix (stdev->ConfigOVMX.BLT_OVMX, bdisp_aq_RGB_2_VideoYCbCr601);

  /* final init of shared area and BDisp registers */
  shared->next_free = 0;
  shared->last_free = stdev->usable_nodes_size - sizeof (*node);

  stdev->aq_index = shared->aq_index;

  bdisp_aq_SetAQReg (stdrv,
                     stdev,
                     BDISP_AQ_CTL,
                     (3 - shared->aq_index) | (0
                                               | BDISP_AQ_CTL_QUEUE_EN
                                               | BDISP_AQ_CTL_EVENT_SUSPEND
                                               | BDISP_AQ_CTL_IRQ_NODE_COMPLETED
                                               | BDISP_AQ_CTL_IRQ_LNA_REACHED
                                              )
                    );

  /* no blit will start until the LNA register is written to */
  bdisp_aq_SetAQReg (stdrv, stdev, BDISP_AQ_IP, shared->nodes_phys);

  return DFB_OK;
}

/*********************************************/
/* Reset the device                          */
/*********************************************/
void
bdisp_aq_EngineReset (void * const drv,
                      void * const dev)
{
#if 0
  printf("%s: drv/dev: %p/%p\n", __FUNCTION__, drv, dev);

  /* don't do this! it resets too much! */
  STGFX2DriverData * const stdrv = drv;

  bdisp_aq_SetReg (stdrv, BDISP_CTL, (1 << 31) /* BDISP_CTL_GLOBAL_SOFT_RESET */ );
  bdisp_aq_SetReg (stdrv, BDISP_CTL, 0);
  while (!(bdisp_aq_GetReg (stdrv, BDISP_STA) & 0x1))
    usleep (1);
#endif
}

DFBResult
bdisp_aq_EngineSync (void * const drv,
                     void * const dev)
{
  STGFX2DriverData     * const stdrv = drv;
  STGFX2DeviceData     * const stdev = dev;
  STMFBBDispSharedArea * const shared = stdrv->bdisp_shared;
  DFBResult             ret = DFB_OK;

  D_DEBUG_AT (BDISP_BLT, "%s()\n", __FUNCTION__);

  DUMP_INFO ();

  /* can't lock here, because the ioctl() does another lock which might have
     been taken by somebody else. */
//  LOCK_ATOMIC_DEBUG_ONLY;

  while (shared->bdisp_running && ioctl (stdrv->fd, STMFBIO_SYNC_BLITTER) < 0)
    {
      if (errno == EINTR)
        continue;

      ret = errno2result (errno);
      D_PERROR ("BDisp/BLT: STMFBIO_SYNC_BLITTER failed!\n" );

      direct_log_printf (NULL, "  -> %srunning, CTL: %.8x IP/LNA/STA: %lu/%lu/%lu"
                         " next/last %lu/%lu\n",
                         shared->bdisp_running ? "" : "not ",
                         bdisp_aq_GetAQReg (stdrv, stdev, BDISP_AQ_CTL),
                         (bdisp_aq_GetAQReg (stdrv, stdev, BDISP_AQ_IP) - shared->nodes_phys) / sizeof (struct _BltNode_Complete),
                         (bdisp_aq_GetAQReg (stdrv, stdev, BDISP_AQ_LNA) - shared->nodes_phys) / sizeof (struct _BltNode_Complete),
                         (bdisp_aq_GetAQReg (stdrv, stdev, BDISP_AQ_STA) - shared->nodes_phys) / sizeof (struct _BltNode_Complete),
                         shared->next_free, shared->last_free);

      break;
    }

  /* need the LOCK_ATOMIC around the ioctl() and this assertion */
//  if (ret == DFB_OK)
//    D_ASSERT (!shared->bdisp_running);

//  UNLOCK_ATOMIC_DEBUG_ONLY;

  return ret;
}


static void
bdisp_aq_WaitSpace (STGFX2DriverData     * const stdrv,
                    STGFX2DeviceData     * const stdev,
                    STMFBBDispSharedArea * const shared)
{
  D_DEBUG_AT (BDISP_BLT, "%s()\n", __FUNCTION__);

  DUMP_INFO ();

  if (!shared->bdisp_running)
    return;

  while (ioctl (stdrv->fd, STMFBIO_WAIT_NEXT) < 0)
    {
      if (errno == EINTR)
        continue;

      D_PERROR ("BDisp/BLT: STMFBIO_WAIT_NEXT failed!\n" );

//      direct_log_printf (NULL, "  -> %srunning, CTL: %.8x IP/LNA/STA: %lu/%lu/%lu"
//                         " next/last %lu/%lu\n",
//                         shared->bdisp_running ? "" : "not ",
//                         bdisp_aq_GetAQReg (stdrv, stdev, BDISP_AQ_CTL),
//                         (bdisp_aq_GetAQReg (stdrv, stdev, BDISP_AQ_IP) - shared->nodes_phys) / sizeof (struct _BltNode_Complete),
//                         (bdisp_aq_GetAQReg (stdrv, stdev, BDISP_AQ_LNA) - shared->nodes_phys) / sizeof (struct _BltNode_Complete),
//                         (bdisp_aq_GetAQReg (stdrv, stdev, BDISP_AQ_STA) - shared->nodes_phys) / sizeof (struct _BltNode_Complete),
//                         shared->next_free, shared->last_free);

      break;
    }
}

/* FIXME: 'synco' is only available starting w/ ST-300 series, i.e.
   not STb7109 and it is not understood by the STLinux-2.2 binutils! */
#ifdef __SH4__
#  define mb() __asm__ __volatile__ ("synco" : : : "memory")
#else
#  define mb() __asm__ __volatile__ ("" : : : "memory")
#endif

#if 1
#define LOCK_ATOMIC \
  { \
  while (unlikely (!__sync_bool_compare_and_swap (&shared->lock.counter, 0, 1))) \
    usleep(1); \
  if (unlikely (shared->locked_by)) \
    D_WARN ("locked an already locked lock by %x\n", shared->locked_by); \
  shared->locked_by = 2; \
  }

#define UNLOCK_ATOMIC \
  { \
  if (unlikely (shared->locked_by != 2)) \
    D_WARN ("shared->locked_by error (%x)!\n", shared->locked_by); \
  shared->locked_by = 0 ; \
  mb(); \
  shared->lock.counter = 0; \
  }
//  __sync_fetch_and_sub (&shared->lock.counter, 1);
//  while (unlikely (!__sync_bool_compare_and_swap (&shared->lock.counter, 1, 0))) {
//    printf("sleeping2...\n");
//  }
#else
#define LOCK_ATOMIC ({})
#define UNLOCK_ATOMIC ({})
#endif

static void
_bdisp_aq_EmitCommands (void * const drv,
                        void * const dev,
                        bool  is_locked)
{
  STGFX2DriverData     * const stdrv = drv;
  STGFX2DeviceData     * const stdev = dev;
  STMFBBDispSharedArea * const shared = stdrv->bdisp_shared;
  unsigned long         new_last_valid;

  if (!is_locked)
    LOCK_ATOMIC;

  new_last_valid = ((shared->next_free ? shared->next_free : stdev->usable_nodes_size)
                    - sizeof (struct _BltNode_Complete)
                    + shared->nodes_phys);

  D_DEBUG_AT (BDISP_BLT, "%s() old/new last valid: %.8lx/%.8lx\n", __FUNCTION__,
              shared->prev_set_lna, new_last_valid);

  /* have any been added since last time we were called? This test is
     necessary because an operation like FillRect() might fail, without
     actually queueing _any_ node (or FillRect_nop() which intentionally
     doesn't queue anything). Ignoring this case, we would set
     bdisp_running to true, and at some later point wait for the interrupt
     to signal completion. Since LNA wouldn't have changed, the interrupt
     would never be fired, and the kernel driver would sleep forever
     waiting for bdisp_running to be false again. */
  if (new_last_valid != shared->prev_set_lna)
    {
      ++shared->num_starts;

      /* signal to kernel driver that we are in the process of updating
         BDISP_AQ_LNA and want bdisp_running to be left untouched, which is
         important in case we are interrupted between setting bdisp_running
         = true and the actual writing to the register. */
      shared->updating_lna = true;
      shared->prev_set_lna = new_last_valid;

      /* Make sure updating_lna and prev_set_lna are set before
         bdisp_running! */
      mb ();

#ifdef STGFX2_MEASURE_BLITLOAD
      if (!shared->bdisp_running)
        {
          struct timespec currenttime;
#define USEC_PER_SEC	1000000L
          clock_gettime (CLOCK_MONOTONIC, &currenttime);
          shared->bdisp_ops_start_time = (((unsigned long long) currenttime.tv_sec * USEC_PER_SEC)
                                          + (currenttime.tv_nsec / 1000));
        }
#endif /* STGFX2_MEASURE_BLITLOAD */

      shared->bdisp_running = true;

#ifdef __SH4__
      /* LMI write posting: make sure outstanding writes to LMI made it into
         RAM, mb() doesn't help, because of the ST40 bandwidth limiter */
      {
      u32 offs = shared->prev_set_lna - shared->nodes_phys;
      volatile struct _BltNodeGroup00 * const ConfigGeneral = (struct _BltNodeGroup00 *) (stdrv->bdisp_nodes + offs);
      u32 nip = ConfigGeneral->BLT_NIP;
      D_ASSERT (shared->next_free == nip - shared->nodes_phys);
      shared->next_free = nip - shared->nodes_phys;
      }
#endif

      /* The idea of this second barrier here is to make sure bdisp_running is
         set before the register, just to make sure the IRQ is not fired for
         this new command while bdisp_running gets set to true only later,
         because of instruction scheduling. The probability of such a
         situation happening should be around zero, but what the heck... */
      mb ();

      /* kick */
      bdisp_aq_SetAQReg (stdrv, stdev, BDISP_AQ_LNA, new_last_valid);

      /* we're done */
      shared->updating_lna = false;
    }

  /* unconditionally unlock */
  UNLOCK_ATOMIC;
}

void
bdisp_aq_EmitCommands (void * const drv,
                       void * const dev)
{
  _bdisp_aq_EmitCommands (drv, dev, false);
}



void
bdisp_aq_GetSerial (void               * const drv,
                    void               * const dev,
                    CoreGraphicsSerial * const serial)
{
  const STMFBBDispSharedArea * const shared =
                                     ((STGFX2DriverData *) drv)->bdisp_shared;

//printf("getserial -> %lu\n", shared->num_ops_lo);

  serial->generation = shared->num_ops_hi;
  serial->serial     = shared->num_ops_lo;
}


DFBResult
bdisp_aq_WaitSerial (void                     * const drv,
                     void                     * const dev,
                     const CoreGraphicsSerial * const serial)
{
  STGFX2DriverData     * const stdrv = drv;
  STMFBBDispSharedArea * const shared = stdrv->bdisp_shared;

  /* FIXME: make this work w/ num_ops_hi != 0, too */
  if (!shared->num_ops_hi
      || serial->serial >= shared->num_ops_lo)
    {
      u32 usr = bdisp_aq_GetReg (stdrv, 0x200 + 0xb4 /* BLT_USR */ );
      while (usr < serial->serial)// shared->num_ops_lo && !shared->num_ops_hi)
        {
//printf("waitserial -> %lu, ops %lu", serial->serial, shared->num_ops_lo);
//printf("   -> usr %lu\n", usr);
//          bdisp_aq_WaitSpace (drv, dev, ((STGFX2DriverData *) drv)->bdisp_shared);
          usr = bdisp_aq_GetReg (stdrv, 0x200 + 0xb4 /* BLT_USR */ );
        }
    }
  else
    bdisp_aq_EngineSync (drv, dev);

  return DFB_OK;
}





#ifdef STGFX2_USE_BSEARCH
#include <stdlib.h>
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
             D_ARRAY_SIZE (bdisp_aq_blitter_5x8_filters),
             sizeof (*bdisp_aq_blitter_5x8_filters),
             _filter_table_lookup);

  if (!res)
    return D_ARRAY_SIZE (bdisp_aq_blitter_5x8_filters) - 1;

  return res - bdisp_aq_blitter_5x8_filters;
}

static int
CSTmBlitter__ChooseBlitter8x8FilterCoeffs (int scale)
{
  const struct _bdisp_aq_blitfilter_coeffs_8x8 * const res =
    bsearch ((void *) scale,
             bdisp_aq_blitter_8x8_filters,
             D_ARRAY_SIZE (bdisp_aq_blitter_8x8_filters),
             sizeof (*bdisp_aq_blitter_8x8_filters),
             _filter_table_lookup);

  if (!res)
    return D_ARRAY_SIZE (bdisp_aq_blitter_8x8_filters) - 1;

  return res - bdisp_aq_blitter_8x8_filters;
}
#else
static int
CSTmBlitter__ChooseBlitter5x8FilterCoeffs (int scale)
{
  int i;

  if (scale <= 0)
    return -1;

  for (i = 0; i < D_ARRAY_SIZE (bdisp_aq_blitter_5x8_filters); ++i)
    if ((scale <= bdisp_aq_blitter_5x8_filters[i].range.scale_max)
        && (scale > bdisp_aq_blitter_5x8_filters[i].range.scale_min))
      return i;

  return D_ARRAY_SIZE (bdisp_aq_blitter_5x8_filters) - 1;
}

static int
CSTmBlitter__ChooseBlitter8x8FilterCoeffs (int scale)
{
  int i;

  if (scale <= 0)
    return -1;

  for (i = 0; i < D_ARRAY_SIZE (bdisp_aq_blitter_8x8_filters); ++i)
    if ((scale <= bdisp_aq_blitter_8x8_filters[i].range.scale_max)
        && (scale > bdisp_aq_blitter_8x8_filters[i].range.scale_min))
      return i;

  return D_ARRAY_SIZE (bdisp_aq_blitter_8x8_filters) - 1;
}
#endif /* STGFX2_USE_BSEARCH */


static void
CSTmBlitter__SetCLUTOperation (const STGFX2DriverData * const stdrv,
                               STGFX2DeviceData       * const stdev,
                               bool                    do_expand,
                               u32                    * const blt_cic,
                               u32                    * const blt_ins,
                               struct _BltNodeGroup07 * const config_clut)
{
  *blt_cic |= CIC_NODE_CLUT;
  *blt_ins |= BLIT_INS_ENABLE_CLUT;

#ifdef STGFX2_CLUT_UNSAFE_MULTISESSION
  stdrv->bdisp_shared->last_lut = 1;
#endif /* STGFX2_CLUT_UNSAFE_MULTISESSION */

  config_clut->BLT_CCO &= ~BLIT_CCO_CLUT_MODE_MASK;
  config_clut->BLT_CCO |= (do_expand ? BLIT_CCO_CLUT_EXPAND
                                     : BLIT_CCO_CLUT_CORRECT);

  config_clut->BLT_CCO |= BLIT_CCO_CLUT_UPDATE_EN;

  config_clut->BLT_CML = stdev->clut_phys[stdev->palette_type];
}



#ifdef STGFX2_PRINT_NODE_WAIT_TIME

  #define bdisp_aq_WaitSpace_start_timer() \
            ({                             \
              struct timeval start, end;   \
              gettimeofday (&start, NULL);

  #define bdisp_aq_WaitSpace_end_timer() \
              gettimeofday (&end, NULL);                               \
              timersub (&end, &start, &end);                           \
              D_INFO ("BDisp/BLT: had to wait for space %lu.%06lus\n", \
                      end.tv_sec, end.tv_usec);                        \
            })

#else /* STGFX2_PRINT_NODE_WAIT_TIME */

  #define bdisp_aq_WaitSpace_start_timer() \
            ({

  #define bdisp_aq_WaitSpace_end_timer() \
            })

#endif /* STGFX2_PRINT_NODE_WAIT_TIME */


/* get a pointer to the next unused node in the nodelist. Will _not_ mark
   the returned node as being used. I.e. you must call
   _bdisp_aq_finish_node() on this node before requesting another node. */
static void *
_bdisp_aq_get_new_node (STGFX2DriverData     * const stdrv,
                        STGFX2DeviceData     * const stdev,
                        STMFBBDispSharedArea * const shared)
{
  D_ASSERT (shared == stdrv->bdisp_shared);

  LOCK_ATOMIC;

  /* if this is the last free entry in the list, kick the bdisp and wait a
     bit. Need to be careful to not create deadlocks. */
  if (unlikely (shared->next_free == shared->last_free))
    {
      bdisp_aq_WaitSpace_start_timer ();

restart:
      _bdisp_aq_EmitCommands (stdrv, stdev, true);
      /* _bdisp_aq_EmitCommands() unlocks the lock for us on exit */

      bdisp_aq_WaitSpace (stdrv, stdev, shared);
      LOCK_ATOMIC;

      if (shared->next_free == shared->last_free)
        goto restart;

      bdisp_aq_WaitSpace_end_timer ();
    }

  return (void *) stdrv->bdisp_nodes + shared->next_free;
}

/* tell the node list handling, that we are done with this node, i.e. it is
   valid and the next node should be used for new commands. Doesn't enqueue
   the node to the hardware, just updates an 'internal' pointer. */
static void
_bdisp_aq_finish_node (STGFX2DriverData     * const stdrv,
                       STGFX2DeviceData     * const stdev,
                       STMFBBDispSharedArea * const shared,
                       const void           * const node)
{
  D_ASSERT (shared == stdrv->bdisp_shared);
  D_ASSERT (node == (stdrv->bdisp_nodes + shared->next_free));

  _dump_node (node);

  /* we need to update shared->next_free:
     - either we could read back node->BLT_NIP and set
       shared->next_free = nip - shared->nodes_phys
       i.e.
       volatile struct _BltNodeGroup00 * const ConfigGeneral = (struct _BltNodeGroup00 *) node;
       u32 nip = ConfigGeneral->BLT_NIP;
       shared->next_free = nip - shared->nodes_phys;

     - or calculate shared->next_free, because we know the size of one node
       (see bdisp_aq_initialize())

     The former has the advantage of being a nice memory barrier in case the
     nodelist area is mmap()ed cached (but currently we don't do so), while
     the latter has the advantage of not having to read back from (at the
     moment) uncached memory. So we calculate... In addition, on the ST40
     due to write posting, we need a read from LMI in any case to make sure
     any previous writes have been completed, but that read is done in another
     place, namely in _bdisp_aq_EmitCommands(). */

  /* advance to next */
  shared->next_free += sizeof (struct _BltNode_Complete);
  if (unlikely (shared->next_free == stdev->usable_nodes_size))
    shared->next_free = 0;

  DUMP_INFO ();

  UNLOCK_ATOMIC;
}


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


#define bdisp_aq_update_num_ops(stdev, shared, blt_ins) \
            ({ \
              /* we have a counter for the number of total operations */     \
              if (unlikely (! ++shared->num_ops_lo))                         \
                ++shared->num_ops_hi;                                        \
              /* every now and then we want to see a 'node completed'        \
                 interrupt (in addition to the usual 'queue completed'       \
                 interrupt), so we don't have to wait for the queue to be    \
                 completely empty when adding a node. But it also shouldn't  \
                 occur too often so as to not waste too much CPU time in the \
                 interrupt handler... */                                     \
              if (!(shared->num_ops_lo % stdev->node_irq_delay))             \
                blt_ins |= BLIT_INS_ENABLE_BLITCOMPIRQ;                      \
            })

#ifdef STGFX2_IMPLEMENT_WAITSERIAL
  #define bdisp_aq_update_config_static(shared, group14) \
            ({ \
              (group14)->BLT_SAR = 0;                  \
              (group14)->BLT_USR = shared->num_ops_lo; \
            })
#else /* STGFX2_IMPLEMENT_WAITSERIAL */
  #define bdisp_aq_update_config_static(shared, group14)
#endif /* STGFX2_IMPLEMENT_WAITSERIAL */

#ifdef STGFX2_CLUT_UNSAFE_MULTISESSION
  #define bdisp_aq_clut_update_disable_if_unsafe(stdev) \
        ({ \
          if (!stdev->clut_disabled)                                        \
            {                                                               \
              stdev->ConfigGeneral.BLT_INS &= ~BLIT_INS_ENABLE_BLITCOMPIRQ; \
              stdev->ConfigClut.BLT_CCO    &= ~BLIT_CCO_CLUT_UPDATE_EN;     \
              stdev->clut_disabled = true;                                  \
            }                                                               \
        })
#else /* STGFX2_CLUT_UNSAFE_MULTISESSION */
  #define bdisp_aq_clut_update_disable_if_unsafe(stdev)
#endif /* STGFX2_CLUT_UNSAFE_MULTISESSION */


static void
bdisp_aq_copy_n_push_node (STGFX2DriverData * const stdrv,
                           STGFX2DeviceData * const stdev)
{
  unsigned char *node;
  struct _BltNodeGroup00 *ConfigGeneral;
  u32                     valid_groups;
  STMFBBDispSharedArea   * const shared = stdrv->bdisp_shared;

#ifdef STGFX2_IMPLEMENT_WAITSERIAL
  stdev->ConfigGeneral.BLT_CIC |= CIC_NODE_STATIC;
#endif /* STGFX2_IMPLEMENT_WAITSERIAL */

  ConfigGeneral = _bdisp_aq_get_new_node (stdrv, stdev, shared);

  stdev->ConfigGeneral.BLT_CIC |= stdev->all_states.extra_blt_cic;
  stdev->ConfigGeneral.BLT_INS |= stdev->all_states.extra_blt_ins;
  bdisp_aq_update_num_ops (stdev, shared, stdev->ConfigGeneral.BLT_INS);

  valid_groups = stdev->ConfigGeneral.BLT_CIC;

  /* now copy the individual groups into the free node item of the nodelist */
  node = ((unsigned char *) ConfigGeneral) + sizeof (u32);
  COPY_TO_NODE_SZ (node,
                   ((void *) &stdev->ConfigGeneral) + sizeof (u32),
                   sizeof (stdev->ConfigGeneral) - sizeof (u32));
  stdev->ConfigGeneral.BLT_INS &= ~(BLIT_INS_ENABLE_BLITCOMPIRQ
                                    | stdev->all_states.extra_blt_ins);
  stdev->ConfigGeneral.BLT_CIC &= ~stdev->all_states.extra_blt_cic;

  COPY_TO_NODE (node, stdev->ConfigTarget);

  if (valid_groups & CIC_NODE_COLOR)
    COPY_TO_NODE (node, stdev->ConfigColor);

  if (valid_groups & CIC_NODE_SOURCE1)
    COPY_TO_NODE (node, stdev->ConfigSource1);

  if (valid_groups & CIC_NODE_SOURCE2)
    COPY_TO_NODE (node, stdev->ConfigSource2);

  if (valid_groups & CIC_NODE_SOURCE3)
    COPY_TO_NODE (node, stdev->ConfigSource3);

#ifdef STGFX2_SUPPORT_HW_CLIPPING
  if (valid_groups & CIC_NODE_CLIP)
    COPY_TO_NODE (node, stdev->ConfigClip);
#endif

  if (valid_groups & CIC_NODE_CLUT)
    COPY_TO_NODE (node, stdev->ConfigClut);

  if (valid_groups & CIC_NODE_FILTERS)
    COPY_TO_NODE (node, stdev->ConfigFilters);

  if (valid_groups & CIC_NODE_2DFILTERSCHR)
    COPY_TO_NODE (node, stdev->ConfigFiltersChr);

  if (valid_groups & CIC_NODE_2DFILTERSLUMA)
    COPY_TO_NODE (node, stdev->ConfigFiltersLuma);

  if (valid_groups & CIC_NODE_FLICKER)
    COPY_TO_NODE (node, stdev->ConfigFlicker);

  if (valid_groups & CIC_NODE_COLORKEY)
    COPY_TO_NODE (node, stdev->ConfigColorkey);

#ifdef STGFX2_IMPLEMENT_WAITSERIAL
  if (likely (valid_groups & CIC_NODE_STATIC))
    {
      bdisp_aq_update_config_static (shared, &stdev->ConfigStatic);
      COPY_TO_NODE (node, stdev->ConfigStatic);
    }
#endif

  if (valid_groups & CIC_NODE_IVMX)
    COPY_TO_NODE (node, stdev->ConfigIVMX);

  if (valid_groups & CIC_NODE_OVMX)
    COPY_TO_NODE (node, stdev->ConfigOVMX);

#if 0
  if (valid_groups & CIC_NODE_VC1R)
    COPY_TO_NODE (node, stdev->ConfigVC1R);
#endif

  _bdisp_aq_finish_node (stdrv, stdev, shared, ConfigGeneral);
}


bool
bdisp_aq_FillDraw_nop (void         * const drv,
                       void         * const dev,
                       DFBRectangle * const rect)
{
  const STGFX2DriverData __attribute__((unused)) * const stdrv = drv;
  const STGFX2DeviceData __attribute__((unused)) * const stdev = dev;

  D_DEBUG_AT (BDISP_BLT, "%s (%d, %d - %dx%d)\n", __FUNCTION__,
              DFB_RECTANGLE_VALS (rect));
  DUMP_INFO ();

  return true;
}



#if 0
static bool
CSTmBDispAQ__SetPlaneMask (const enum STM_BLITTER_FLAGS  flags,
                           const unsigned long           plane_mask,
                           u32                          * const blt_cic,
                           u32                          * const blt_ins,
                           u32                          * const blt_pmk)
{
  if (!D_FLAGS_IS_SET (flags, STM_BLITTER_FLAGS_PLANE_MASK))
    return true;

  if (unlikely ((*blt_ins & BLIT_INS_SRC1_MODE_MASK) == BLIT_INS_SRC1_MODE_COLOR_FILL))
    {
      D_DEBUG ("%s(): cannot use plane masking and blend against a solid colour\n",
               __FUNCTION__);
      return false;
    }

  *blt_cic |= CIC_NODE_FILTERS;
  *blt_ins &= ~BLIT_INS_SRC1_MODE_MASK;
  *blt_ins |= BLIT_INS_ENABLE_PLANEMASK | BLIT_INS_SRC1_MODE_MEMORY;

  *blt_pmk = plane_mask;

  return true;
}
#endif


#ifdef STGFX2_SUPPORT_HW_CLIPPING
static void
bdisp_aq_SetClip (const STGFX2DeviceData * const stdev,
                  u32                    * const blt_cic,
                  u32                    * const blt_ins,
                  struct _BltNodeGroup06 * const config_clip)
{
  if (stdev->v_flags & 0x00000002 /* CLIP */)
    {
      *blt_cic |= CIC_NODE_CLIP;
      *blt_ins |= BLIT_INS_ENABLE_RECTCLIP;
      *config_clip = stdev->ConfigClip;
    }
}
#else
  #define bdisp_aq_SetClip(dev, cic, ins, clip)
#endif


void
_bdisp_aq_prepare_upload_palette_hw (STGFX2DriverData * const stdrv,
                                     STGFX2DeviceData * const stdev)
{
#ifndef STGFX2_CLUT_UNSAFE_MULTISESSION
  /* before updating the LUT, we have to wait for the blitter to finish all
     previous operations that would reload the LUT from memory. That's
     rather difficult to find out - so we wait for the complete nodelist to
     finish. */
  /* FIXME: this is very inefficient! */
#warning FIXME it is very inefficient to wait for the blitter to sync here!
  bdisp_aq_EngineSync (stdrv, stdev);
#else /* STGFX2_CLUT_UNSAFE_MULTISESSION */
  /* before updating the LUT, we have to wait for the blitter to finish all
     previous operations that would reload the LUT from memory. Rather than
     waiting for the complete nodelist to finish, we just wait as long as the
     blitter instruction involving the previous LUT has not been executed. */
   /* FIXME: locking */
  while (stdrv->bdisp_shared->last_lut)
    /* the CLUT could have been updated between the check above and the call
       to bdisp_aq_WaitSpace(). In that case we would be waiting
       unnecessarily; we could create a new IOCTL specifically for waiting
       for the CLUT to be updated, but we don't really care as no real harm
       arises from waiting unnecessarily. */
    bdisp_aq_WaitSpace (stdrv, stdev, stdrv->bdisp_shared);
#endif /* STGFX2_CLUT_UNSAFE_MULTISESSION */
}

static void
_bdisp_aq_upload_palette_hw (STGFX2DriverData * const stdrv,
                             STGFX2DeviceData * const stdev)
{
  int i;

  _bdisp_aq_prepare_upload_palette_hw (stdrv, stdev);

  for (i = 0; i < stdev->palette->num_entries; ++i)
    {
      const DFBColor * const color = &stdev->palette->entries[i];
      u16 alpha = color->a;
      /* Note that the alpha range for the lut is 0-128 (not 127!) */
      stdrv->clut_virt[SG2C_NORMAL][i] = PIXEL_ARGB ((alpha + 1) / 2, color->r,
                                                     color->g, color->b);
    }
}


/* get a node and also prepare it for a 'simple' drawing operation, so that
   the only action which is left to the caller is to fill in the coordinates
   and call _bdisp_aq_finish_node(). */
static struct _BltNode_Fast *
_bdisp_aq_get_new_node_prepared_simple (STGFX2DriverData     * const stdrv,
                                        STGFX2DeviceData     * const stdev,
                                        STMFBBDispSharedArea * const shared)
{
  struct _BltNode_Fast * const node = _bdisp_aq_get_new_node (stdrv, stdev,
                                                              shared);
  u32 blt_ins = BLIT_INS_SRC1_MODE_DIRECT_FILL;

  D_ASSERT (shared == stdrv->bdisp_shared);

  bdisp_aq_update_num_ops (stdev, shared, blt_ins);

  node->common.ConfigTarget.BLT_TBA = stdev->ConfigTarget.BLT_TBA;
  node->common.ConfigTarget.BLT_TTY = bdisp_ty_sanitise_direction (stdev->ConfigTarget.BLT_TTY);

  /* source1 == destination */
  node->common.ConfigSource1.BLT_S1TY = stdev->drawstate.color_ty;
  node->common.ConfigColor.BLT_S1CF   = stdev->drawstate.color;

  node->common.ConfigGeneral.BLT_CIC = CIC_BLT_NODE_FAST;
  node->common.ConfigGeneral.BLT_INS = blt_ins;
  /* BLT_ACK is ignored by the hardware in this case... */

  bdisp_aq_update_config_static (shared, &node->ConfigStatic);

  return node;
}

/* get a node and also prepare it for a drawing operation on the 'slow'
   path, so that the only action which is left to the caller is to fill in
   the coordinates and call _bdisp_aq_finish_node(). */
static struct _BltNode_FullFill *
_bdisp_aq_get_new_node_prepared_slow (STGFX2DriverData     * const stdrv,
                                      STGFX2DeviceData     * const stdev,
                                      STMFBBDispSharedArea * const shared)
{
  struct _BltNode_FullFill * const node = _bdisp_aq_get_new_node (stdrv,
                                                                  stdev,
                                                                  shared);
  struct _BltNodeGroup08   *config_filters;
  struct _BltNodeGroup12   *config_colorkey;
  struct _BltNodeGroup14   *config_static;

  u32 blt_cic = CIC_BLT_NODE_COMMON | CIC_NODE_SOURCE2 | CIC_NODE_STATIC;
  u32 blt_ins;
  u32 blt_ack;
  u32 src2type = stdev->drawstate.color_ty | BLIT_TY_COLOR_EXPAND_MSB;

  D_ASSERT (shared == stdrv->bdisp_shared);

  blt_cic |= stdev->drawstate.ConfigGeneral.BLT_CIC | stdev->all_states.extra_blt_cic;
  blt_ins  = stdev->drawstate.ConfigGeneral.BLT_INS | stdev->all_states.extra_blt_ins;
  blt_ack  = stdev->drawstate.ConfigGeneral.BLT_ACK;

  node->common.ConfigTarget.BLT_TBA = stdev->ConfigTarget.BLT_TBA;
  node->common.ConfigTarget.BLT_TTY = bdisp_ty_sanitise_direction (stdev->ConfigTarget.BLT_TTY);

  /* source1 == destination */
  node->common.ConfigSource1.BLT_S1BA = stdev->ConfigTarget.BLT_TBA;
  node->common.ConfigSource1.BLT_S1TY =
    (bdisp_ty_sanitise_direction (stdev->ConfigTarget.BLT_TTY)
     | BLIT_TY_COLOR_EXPAND_MSB);

  node->common.ConfigColor.BLT_S2CF = stdev->drawstate.color;
  node->ConfigSource2.BLT_S2TY = src2type;

  /* if the clut is enabled, filters starts after the clut group, if
     not, everything is moved up a bit */
  if (blt_cic & CIC_NODE_CLUT)
    {
      node->ConfigClut = stdev->drawstate.ConfigClut;
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
  if (blt_cic & CIC_NODE_FILTERS)
    {
      node->ConfigFilters.BLT_FCTL_RZC = 0;
      node->ConfigFilters.BLT_PMK = stdev->ConfigFilters.BLT_PMK;
      config_colorkey = (struct _BltNodeGroup12 *) (config_filters + 1);
    }
  else
    config_colorkey = (struct _BltNodeGroup12 *) config_filters;

  /* if colorkeying is enabled, static starts after the colorkey group, if
     not, everything is moved up a bit */
  if (blt_cic & CIC_NODE_COLORKEY)
    {
      config_colorkey->BLT_KEY1
        = config_colorkey->BLT_KEY2
        = stdev->all_states.dst_ckey;

      config_static = (struct _BltNodeGroup14 *) (config_colorkey + 1);
    }
  else
    config_static = (struct _BltNodeGroup14 *) config_colorkey;

  bdisp_aq_update_config_static (shared, config_static);

  if (blt_cic & CIC_NODE_IVMX)
    {
      /* if we have the static group, ivmx starts after it, if not, everything
         is moved up a bit */
      struct _BltNodeGroup15 * const config_ivmx = (struct _BltNodeGroup15 *)
        ((blt_cic & CIC_NODE_STATIC) ? (config_static + 1)
                                     : config_static);

      set_matrix (config_ivmx->BLT_IVMX, bdisp_aq_RGB_2_VideoYCbCr601);
    }

  bdisp_aq_SetClip (stdev, &blt_cic, &blt_ins, &node->ConfigClip);

  bdisp_aq_update_num_ops (stdev, shared, blt_ins);

  node->common.ConfigGeneral.BLT_CIC = blt_cic;
  node->common.ConfigGeneral.BLT_INS = blt_ins;
  node->common.ConfigGeneral.BLT_ACK = blt_ack;

  return node;
}




void
bdisp_aq_setup_blit_operation (STGFX2DriverData * const stdrv,
                               STGFX2DeviceData * const stdev)
{
  stdev->ConfigGeneral.BLT_INS &= ~BLIT_INS_ENABLE_FLICKERFILTER;
  if (stdev->blitstate.flags & STM_BLITTER_FLAGS_FLICKERFILTER)
    stdev->ConfigGeneral.BLT_INS |= BLIT_INS_ENABLE_FLICKERFILTER;

  /* we might have done a stretch blit before and do a normal blit
     now, in which case make sure to disable the rescaling engine. */
  stdev->ConfigGeneral.BLT_CIC &= ~(CIC_NODE_2DFILTERSCHR
                                    | CIC_NODE_2DFILTERSLUMA);
  if (!(stdev->ConfigGeneral.BLT_INS & BLIT_INS_ENABLE_PLANEMASK))
    stdev->ConfigGeneral.BLT_CIC &= ~CIC_NODE_FILTERS;
  stdev->ConfigGeneral.BLT_INS &= ~BLIT_INS_ENABLE_2DRESCALE;

  if (!stdev->bIndexTranslation
      && (stdev->palette
          || stdev->palette_type != SG2C_NORMAL))
    {
      if (stdev->palette)
        /* user palettes are written to the same memory location, so we need
           to make sure not to overwrite any previous potentially different
           palette while still in use. Therefore wait for the BDisp to be
           idle. Color correction palettes are located at different places
           in physical memory, which is why we can simply adjust the memory
           pointer for those.
           We will likely have to optimize this behaviour! */
        _bdisp_aq_upload_palette_hw (stdrv, stdev);

      CSTmBlitter__SetCLUTOperation (stdrv, stdev,
                                     stdev->palette_type == SG2C_NORMAL,
                                     &stdev->ConfigGeneral.BLT_CIC,
                                     &stdev->ConfigGeneral.BLT_INS,
                                     &stdev->ConfigClut);

      D_DEBUG ("%s(): CLUT type: %#.8lx cco/cml: %#.8x %#.8x\n",
               __FUNCTION__,
               stdev->ConfigSource2.BLT_S2TY & (BLIT_TY_COLOR_FORM_MASK
                                                | BLIT_TY_FULL_ALPHA_RANGE
                                                | BLIT_TY_BIG_ENDIAN),
               stdev->ConfigClut.BLT_CCO, stdev->ConfigClut.BLT_CML);

#ifdef STGFX2_CLUT_UNSAFE_MULTISESSION
      /* set this to false, so code elsewhere knows the CLUT upload
         is enabled and can disable it. */
      stdev->clut_disabled = false;
#endif
    }
  else
    {
      stdev->ConfigGeneral.BLT_CIC &= ~CIC_NODE_CLUT;
      stdev->ConfigGeneral.BLT_INS &= ~BLIT_INS_ENABLE_CLUT;
    }
}






#define DFB_FIXED_POINT_TO_BDISP(f)  ((f) >> 6)

static int
FixedPointToInteger (int  f16,
                     u32 * const fraction)
{
  int integer = DFB_FIXED_POINT_TO_INT (f16);

  if (fraction)
    *fraction = f16 - (DFB_FIXED_POINT_VAL (integer));

  return integer;
}


static void
_bdisp_aq_Blit_using_spans (STGFX2DriverData   * const stdrv,
                            STGFX2DeviceData   * const stdev,
                            const DFBRectangle * const src,
                            const DFBRectangle * const dst)
{
  /* The filter delay lines can hold 128 pixels (or more, depending on the
   implementation), we want to process slightly less than that because we need
   up to 4 pixels of overlap between spans to fill the filter taps properly
   and not end up with stripes down the image at the edges of two adjoining
   spans. */
  unsigned long long max_filterstep = ((stdev->features.line_buffer_length - 8)
                                       * DFB_FIXED_POINT_ONE);
  int                hsrcinc = stdev->hsrcinc;

  /* Calculate how many destination pixels will get generated from one
     span of the source image. */
  int this_dst_step = max_filterstep / hsrcinc;

  /* Work the previous calculation backwards so we don't get rounding
     errors causing bad filter positioning in the middle of the scaled
     image. We deal with any left over pixels at the edge of the image
     separately.
     This puts srcstep into 16.16 fixed point. */
  int srcstep = this_dst_step * hsrcinc;
  D_DEBUG_AT (BDISP_STRETCH_SPANS,
              "%s: %d.%06d,%d.%06d-%d.%06dx%d.%06d -> %d.%06d,%d.%06d-%d.%06dx%d.%06d srcstep: 0x%x (%d.%06d) dststep: %d\n",
              __FUNCTION__,
              DFB_RECTANGLE_VALSf (src), DFB_RECTANGLE_VALSf (dst),
              srcstep, DFB_INT_VALf (srcstep), this_dst_step);

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

  /* because of rounding errors we might run out of source pixels (when
     upscaling). To work around that, we calculate how many pixels we are
     off and slightly move the source x such that we read this extra
     amount of pixels per span. Chosing a span width small enough doesn't
     cause any visible artifacts, but the code should be changed to select
     the span width dynamically based on the resulting rounding error. In
     addition, the latests chips have a big line buffer, and doing this
     adjustment statically, without changing the span width will look
     horrible. */
  int error_adjust = 0;
#if 0
  {
  int total = DFB_FIXED_POINT_VAL ((long long) src->w) / hsrcinc;
//  total -= DFB_FIXED_POINT_VAL (3);
  int error = (((long long) (dst->w - total)) * hsrcinc) / DFB_FIXED_POINT_ONE;
  if (error > 0)
    {
      int spans = (src->w + max_filterstep/2) / max_filterstep;
      error_adjust = error / spans;
    }
  }
#endif

  int this_src_x = src->x;
  int this_dst_x = dst->x;
  int _srcspanwidth = srcstep /* 16.16 */;

  u32 s2sz = stdev->ConfigSource2.BLT_S2SZ & 0xffff0000;
  u32 tsz  = stdev->ConfigTarget.BLT_TSZ_S1SZ & 0xffff0000;
  u32 rzi = stdev->ConfigFiltersChr.BLT_RZI & ~(BLIT_RZI_H_REPEAT_MASK
                                                | BLIT_RZI_H_INIT_MASK);

  bool do_filter = (((stdev->ConfigFilters.BLT_FCTL_RZC & BLIT_RZC_2DHF_MODE_MASK)
                     != BLIT_RZC_2DHF_MODE_RESIZE_ONLY)
                    || ((stdev->ConfigFilters.BLT_FCTL_RZC & BLIT_RZC_FF_MODE_MASK)
                        != BLIT_RZC_FF_MODE_FILTER0));

  while (this_dst_step /* int */ > 0)
    {
      int adjustedsrcx /* int */, srcspanwidth = _srcspanwidth /* 16.16 */;
      u32 subpixelpos /* n.16 */;
      int edgedistance /* 16.16 */;

      /* We need the subpixel position of the start, which is basically where
         the filter left off on the previous span. */
      adjustedsrcx = FixedPointToInteger (this_src_x, &subpixelpos);
      if (stdev->h_src2_sign == -1)
        {
          /* This is not immediately obvious:
             1. We have to flip around the subpixel position as the filter
                is being filled with pixels in reverse.
             2. Then if the subpixel position is not zero, we have to move
                our first real pixel sample in the middle of the filter to
                the one on the right of the current position.
             This could make the edge distance go negative on the first
             span if the subpixel position is not zero, but that is ok
             because we will then end up with an extra repeated pixel to
             pad the filter taps. */
          subpixelpos = (DFB_FIXED_POINT_ONE - subpixelpos) % DFB_FIXED_POINT_ONE;
          if (subpixelpos != 0)
            {
              ++adjustedsrcx;
              srcspanwidth += DFB_FIXED_POINT_ONE;
            }
        }

      /* Adjust things to fill the first three filter taps either with
         real data or a repeat of the first pixel. */
      edgedistance = ((DFB_FIXED_POINT_VAL (adjustedsrcx) - src->x)
                      * stdev->h_src2_sign);
      D_DEBUG_AT (BDISP_STRETCH, "  -> span edgedistance: %d.%06d\n",
                  DFB_INT_VALf (edgedistance));

      int repeat;
      if (do_filter)
        {
          if (edgedistance >= DFB_FIXED_POINT_VAL (3))
            {
              repeat        = 0;
              adjustedsrcx -= (3 * stdev->h_src2_sign);
              srcspanwidth += DFB_FIXED_POINT_VAL (3);
            }
          else
            {
              repeat        = 3 - DFB_FIXED_POINT_TO_INT (edgedistance);
              adjustedsrcx  = DFB_FIXED_POINT_TO_INT (src->x);
              srcspanwidth += edgedistance;
            }

          /* Adjust for an extra 4 pixels to be read to fill last four filter
             taps, or to repeat the last pixel if we have gone outside of the
             source image... */
          srcspanwidth += DFB_FIXED_POINT_VAL (4);
        }
      else
        repeat = 0;

      /* ...by clamping the source width we program in the hardware. */
      if (stdev->h_src2_sign == 1
          && ((DFB_FIXED_POINT_VAL (adjustedsrcx) - src->x + srcspanwidth) > src->w))
        srcspanwidth = src->w - (DFB_FIXED_POINT_VAL (adjustedsrcx) - src->x);
      else if (stdev->h_src2_sign == -1
               && ((src->x - src->w + srcspanwidth) > DFB_FIXED_POINT_VAL (adjustedsrcx)))
        srcspanwidth = DFB_FIXED_POINT_VAL (adjustedsrcx) - (src->x - src->w);

      D_DEBUG_AT (BDISP_STRETCH, "     adjsrcx: %d subpixelpos: %x (%d.%06d) srcspanwidth: %x (%d.%06d) this_dst_x: %d.%06d this_dst_step: %d repeat: %d\n",
                  adjustedsrcx,
                  subpixelpos, DFB_INT_VALf (subpixelpos),
                  srcspanwidth, DFB_INT_VALf (srcspanwidth),
                  DFB_INT_VALf (this_dst_x), this_dst_step, repeat);


      stdev->ConfigFiltersChr.BLT_RZI  = rzi;
      stdev->ConfigFiltersChr.BLT_RZI |= (repeat << BLIT_RZI_H_REPEAT_SHIFT);
      stdev->ConfigFiltersChr.BLT_RZI |= DFB_FIXED_POINT_TO_BDISP (subpixelpos) << BLIT_RZI_H_INIT_SHIFT;

      stdev->ConfigTarget.BLT_TXY  = DFB_FIXED_POINT_TO_INT (dst->y) << 16;
      stdev->ConfigTarget.BLT_TXY |= DFB_FIXED_POINT_TO_INT (this_dst_x);

      stdev->ConfigTarget.BLT_TSZ_S1SZ  = tsz;
      stdev->ConfigTarget.BLT_TSZ_S1SZ |= (this_dst_step & 0x0fff);

      if (!D_FLAGS_IS_SET (stdev->ConfigGeneral.BLT_CIC, CIC_NODE_SOURCE3))
        {
          stdev->ConfigSource1.BLT_S1XY = stdev->ConfigTarget.BLT_TXY;

          stdev->ConfigSource2.BLT_S2XY  = DFB_FIXED_POINT_TO_INT (src->y) << 16;
          stdev->ConfigSource2.BLT_S2XY |= adjustedsrcx;
          stdev->ConfigSource2.BLT_S2SZ  = s2sz;
          stdev->ConfigSource2.BLT_S2SZ |= (DFB_FIXED_POINT_TO_INT (srcspanwidth)
                                            & 0x0fff);

          D_DEBUG_AT (BDISP_STRETCH, "     S2XY: %.8x S2SZ: %.8x -> TXY %.8x TSZ %.8x (RZI %.8x)\n",
                      stdev->ConfigSource2.BLT_S2XY, stdev->ConfigSource2.BLT_S2SZ,
                      stdev->ConfigTarget.BLT_TXY, stdev->ConfigTarget.BLT_TSZ_S1SZ,
                      stdev->ConfigFiltersChr.BLT_RZI);
        }
      else
        {
          stdev->ConfigSource3.BLT_S3XY  = DFB_FIXED_POINT_TO_INT (src->y) << 16;
          stdev->ConfigSource3.BLT_S3XY |= adjustedsrcx;
          stdev->ConfigSource3.BLT_S3SZ  = s2sz;
          stdev->ConfigSource3.BLT_S3SZ |= (DFB_FIXED_POINT_TO_INT (srcspanwidth)
                                            & 0x0fff);

          stdev->ConfigFiltersLuma.BLT_Y_RZI = stdev->ConfigFiltersChr.BLT_RZI;

          if (stdev->srcFactorH == 2
              || stdev->srcFactorV == 2)
            {
              int adjustedsrcx_cb /* int */;
              u32 subpixelpos_cb /* n.16 */;
              int this_src_x_cb = this_src_x / stdev->srcFactorH /* 16.16 */;
              int srcspanwidth_cb = _srcspanwidth / stdev->srcFactorH /* 16.16 */;
              int edgedistance_cb /* 16.16 */;
              int src_x_cb = src->x / stdev->srcFactorH /* 16.16 */;
              int src_w_cb = src->w / stdev->srcFactorH /* 16.16 */;

              adjustedsrcx_cb = FixedPointToInteger (this_src_x_cb, &subpixelpos_cb);
              if (stdev->h_src2_sign == -1)
                {
                  subpixelpos_cb = (DFB_FIXED_POINT_ONE - subpixelpos_cb) % DFB_FIXED_POINT_ONE;
                  if (subpixelpos_cb != 0)
                    {
                      ++adjustedsrcx_cb;
                      srcspanwidth_cb += DFB_FIXED_POINT_ONE;
                    }
                }

              edgedistance_cb = ((DFB_FIXED_POINT_VAL (adjustedsrcx_cb) - src_x_cb)
                                 * stdev->h_src2_sign);
              D_DEBUG_AT (BDISP_STRETCH, "  -> span edgedistance_cb: %d.%06d\n",
                          DFB_INT_VALf (edgedistance_cb));

              int repeat_cb;
              if (do_filter)
                {
                  if (edgedistance_cb >= DFB_FIXED_POINT_VAL (3))
                    {
                      repeat_cb        = 0;
                      adjustedsrcx_cb -= (3 * stdev->h_src2_sign);
                      srcspanwidth_cb += DFB_FIXED_POINT_VAL (3);
                    }
                  else
                    {
                      repeat_cb        = 3 - DFB_FIXED_POINT_TO_INT (edgedistance_cb);
                      adjustedsrcx_cb  = DFB_FIXED_POINT_TO_INT (src_x_cb);
                      srcspanwidth_cb += edgedistance_cb;
                    }

                  srcspanwidth_cb += DFB_FIXED_POINT_VAL (4);
                }
              else
                repeat_cb = 0;

              /* ...by clamping the source width we program in the hardware. */
              if (stdev->h_src2_sign == 1
                  && ((DFB_FIXED_POINT_VAL (adjustedsrcx_cb) - src_x_cb + srcspanwidth_cb) > src_w_cb))
                srcspanwidth_cb = src_w_cb - (DFB_FIXED_POINT_VAL (adjustedsrcx_cb) - src_x_cb);
              else if (stdev->h_src2_sign == -1
                       && ((src_x_cb - src_w_cb + srcspanwidth_cb) > DFB_FIXED_POINT_VAL (adjustedsrcx_cb)))
                srcspanwidth_cb = DFB_FIXED_POINT_VAL (adjustedsrcx_cb) - (src_x_cb - src_w_cb);

              D_DEBUG_AT (BDISP_STRETCH, "     adjsrcx_cb: %d subpixelpos_cb: %x (%d.%06d) srcspanwidth_cb: %x (%d.%06d) this_dst_x: %d.%06d this_dst_step: %d repeat_cb: %d\n",
                          adjustedsrcx_cb,
                          subpixelpos_cb, DFB_INT_VALf (subpixelpos_cb),
                          srcspanwidth_cb, DFB_INT_VALf (srcspanwidth_cb),
                          DFB_INT_VALf (this_dst_x), this_dst_step, repeat_cb);

              stdev->ConfigSource2.BLT_S2XY  = (DFB_FIXED_POINT_TO_INT (src->y / stdev->srcFactorV) << 16) & 0x0fff0000;
              stdev->ConfigSource2.BLT_S2XY |= adjustedsrcx_cb;
              stdev->ConfigSource2.BLT_S2SZ  = (s2sz / stdev->srcFactorV) & 0x0fff0000;
              stdev->ConfigSource2.BLT_S2SZ |= (DFB_FIXED_POINT_TO_INT (srcspanwidth_cb) & 0x0fff);

              /* just to be safe in case we determined to blit 1 luma pixel (which
                 could be 0.5 chroma pixels, i.e. 0) */
              if (unlikely (!(stdev->ConfigSource2.BLT_S2SZ & 0x0fff0000)))
                stdev->ConfigSource2.BLT_S2SZ |= 1 << 16;
              if (unlikely (!(stdev->ConfigSource2.BLT_S2SZ & 0x0fff)))
                stdev->ConfigSource2.BLT_S2SZ |= 1;

              stdev->ConfigFiltersChr.BLT_RZI  = rzi;
              stdev->ConfigFiltersChr.BLT_RZI |= (repeat_cb << BLIT_RZI_H_REPEAT_SHIFT);
              stdev->ConfigFiltersChr.BLT_RZI |= DFB_FIXED_POINT_TO_BDISP (subpixelpos_cb) << BLIT_RZI_H_INIT_SHIFT;
            }
          else /* if (stdev->srcFactorH == 1) */
            {
              stdev->ConfigSource2.BLT_S2XY = stdev->ConfigSource3.BLT_S3XY;
              stdev->ConfigSource2.BLT_S2SZ = stdev->ConfigSource3.BLT_S3SZ;
            }

          stdev->ConfigSource1.BLT_S1XY = stdev->ConfigSource2.BLT_S2XY;

          D_DEBUG_AT (BDISP_STRETCH, "     S3XY: %.8x S3SZ: %.8x S2XY: %.8x S2SZ: %.8x S1XY: %.8x -> TXY %.8x TSZ %.8x (RZI %.8x Y_RZI %.8x)\n",
                      stdev->ConfigSource3.BLT_S3XY, stdev->ConfigSource3.BLT_S3SZ,
                      stdev->ConfigSource2.BLT_S2XY, stdev->ConfigSource2.BLT_S2SZ,
                      stdev->ConfigSource1.BLT_S1XY,
                      stdev->ConfigTarget.BLT_TXY, stdev->ConfigTarget.BLT_TSZ_S1SZ,
                      stdev->ConfigFiltersChr.BLT_RZI, stdev->ConfigFiltersLuma.BLT_Y_RZI);
        }

      bdisp_aq_copy_n_push_node (stdrv, stdev);

      this_src_x += (srcstep * stdev->h_src2_sign);
      this_src_x -= (error_adjust * stdev->h_src2_sign);
      this_dst_x += (DFB_FIXED_POINT_VAL (this_dst_step) * stdev->h_trgt_sign);

      /* Alter the span sizes for the "last" span where we have an
         odd bit left over (i.e. the total destination is not a nice
         multiple of this_dst_step). */
      if (stdev->h_trgt_sign == 1)
        {
          /* Remember that x + w (== right) isn't in the destination rect */
          int dst_right = dst->x + dst->w;
          if ((dst_right - this_dst_x) < DFB_FIXED_POINT_VAL (this_dst_step))
            this_dst_step = DFB_FIXED_POINT_TO_INT (dst_right - this_dst_x);
        }
      else
        {
          int dst_left = dst->x - dst->w + DFB_FIXED_POINT_ONE;
          if (((this_dst_x - dst_left) + DFB_FIXED_POINT_ONE) < DFB_FIXED_POINT_VAL (this_dst_step))
            this_dst_step = DFB_FIXED_POINT_TO_INT ((this_dst_x - dst_left) + DFB_FIXED_POINT_ONE);
        }

      /* to save bandwidth, we make sure to update the CLUT only
         once. Also, we turn off the BLIT_INS_ENABLE_BLITCOMPIRQ
         since we need it only once to notify us when the node
         containing the CLUT update has been executed. */
      bdisp_aq_clut_update_disable_if_unsafe (stdev);
    }
}


static bool
_bdisp_aq_rectangles_intersect (const DFBRectangle * __restrict rect1,
                                const DFBRectangle * __restrict rect2)
{
  int right1  = rect1->x + rect1->w;
  int bottom1 = rect1->y + rect1->h;
  int right2  = rect2->x + rect2->w;
  int bottom2 = rect2->y + rect2->h;

  return !(rect1->x >= right2 || right1 <= rect2->x
           || rect1->y >= bottom2 || bottom1 <= rect2->y);
}

static bool
__attribute__((warn_unused_result))
_bdisp_aq_Blit_setup_directions (STGFX2DeviceData   * const stdev,
                                 const DFBRectangle * const src2,
                                 const DFBRectangle * const src1,
                                 const DFBRectangle * const dst)
{
  stdev->ConfigTarget.BLT_TTY   &= ~BLIT_TY_COPYDIR_MASK;
  stdev->ConfigSource1.BLT_S1TY &= ~BLIT_TY_COPYDIR_MASK;
  stdev->ConfigSource2.BLT_S2TY &= ~BLIT_TY_COPYDIR_MASK;
  stdev->ConfigSource3.BLT_S3TY &= ~BLIT_TY_COPYDIR_MASK;

  /* work out copy direction */

  /* we don't support any overlap at all during rotation */
  if (stdev->rotate
      && stdev->ConfigSource2.BLT_S2BA == stdev->ConfigSource1.BLT_S1BA)
    {
      DFBRegion s1region = { .x1 = src1->x,
                             .y1 = src1->y };
      s1region.x2 = src1->x + src1->h - DFB_FIXED_POINT_ONE;
      s1region.y2 = src1->y + src1->w - DFB_FIXED_POINT_ONE;
      if (dfb_region_rectangle_intersect (&s1region, src2))
        /* scary, by carefully checking we might be able to allow some of
           these. but not today... */
        return false;
    }

  /* we need to know the copy direction in various places */
  stdev->h_trgt_sign
    = stdev->h_src2_sign
    = stdev->v_trgt_sign
    = stdev->v_src2_sign
    = 1;

  switch (stdev->rotate)
    {
    case 0:
      {
      bool backwards;
      u32  trgt_dir;

      /* FIXME: for blit2 */
      backwards = ((stdev->ConfigSource2.BLT_S2BA == stdev->ConfigSource1.BLT_S1BA)
                   && ((src1->y > src2->y
                        || ((src1->y == src2->y) && (src1->x > src2->x)))
                       && _bdisp_aq_rectangles_intersect (src2, src1)
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
          stdev->h_trgt_sign
            = stdev->h_src2_sign
            = stdev->v_trgt_sign
            = stdev->h_src2_sign
            = -1;
        }
      stdev->ConfigTarget.BLT_TTY |= trgt_dir;
      stdev->ConfigSource2.BLT_S2TY |= trgt_dir;
      stdev->ConfigSource3.BLT_S3TY |= trgt_dir;
      }
      break;

    case 180: /* rotate 180° */
      if ((src2->w + DFB_FIXED_POINT_VAL (7)) >= (src1->w + dst->w))
        {
          /* target from bottom right to top left */
          /* source2 from top left to bottom right */
          stdev->ConfigTarget.BLT_TTY |= (BLIT_TY_COPYDIR_BOTTOMTOP | BLIT_TY_COPYDIR_RIGHTLEFT);
          stdev->ConfigSource2.BLT_S2TY |= (BLIT_TY_COPYDIR_TOPBOTTOM | BLIT_TY_COPYDIR_LEFTRIGHT);
          stdev->h_trgt_sign = stdev->v_trgt_sign = -1;
        }
      else
        {
          /* target from top left to bottom right */
          /* source2 from bottom right to top left */
          stdev->ConfigTarget.BLT_TTY |= (BLIT_TY_COPYDIR_TOPBOTTOM | BLIT_TY_COPYDIR_LEFTRIGHT);
          stdev->ConfigSource2.BLT_S2TY |= (BLIT_TY_COPYDIR_BOTTOMTOP | BLIT_TY_COPYDIR_RIGHTLEFT);
          stdev->h_src2_sign = stdev->v_src2_sign = -1;
        }
      break;

    case 181: /* reflection against x */
      if ((src2->h + DFB_FIXED_POINT_VAL (4)) >= (src1->h + dst->h))
        {
          /* target from bottom left to top right */
          /* source2 from top left to bottom right */
          stdev->ConfigTarget.BLT_TTY |= (BLIT_TY_COPYDIR_BOTTOMTOP | BLIT_TY_COPYDIR_LEFTRIGHT);
          stdev->ConfigSource2.BLT_S2TY |= (BLIT_TY_COPYDIR_TOPBOTTOM | BLIT_TY_COPYDIR_LEFTRIGHT);
          stdev->v_trgt_sign = -1;
        }
      else
        {
          /* target from top left to bottom right */
          /* source2 from bottom left to top right */
          stdev->ConfigTarget.BLT_TTY |= (BLIT_TY_COPYDIR_TOPBOTTOM | BLIT_TY_COPYDIR_LEFTRIGHT);
          stdev->ConfigSource2.BLT_S2TY |= (BLIT_TY_COPYDIR_BOTTOMTOP | BLIT_TY_COPYDIR_LEFTRIGHT);
          stdev->v_src2_sign = -1;
        }
      break;

    case 182: /* reflection against y */
      if ((src2->w + DFB_FIXED_POINT_VAL (7)) >= (src1->w + dst->w))
        {
          /* target from top right to bottom left */
          /* source2 from top left to bottom right */
          stdev->ConfigTarget.BLT_TTY |= (BLIT_TY_COPYDIR_TOPBOTTOM | BLIT_TY_COPYDIR_RIGHTLEFT);
          stdev->ConfigSource2.BLT_S2TY |= (BLIT_TY_COPYDIR_TOPBOTTOM | BLIT_TY_COPYDIR_LEFTRIGHT);
          stdev->h_trgt_sign = -1;
        }
      else
        {
          /* target from top left to bottom right */
          /* source2 from top right to bottom left */
          stdev->ConfigTarget.BLT_TTY |= (BLIT_TY_COPYDIR_TOPBOTTOM | BLIT_TY_COPYDIR_LEFTRIGHT);
          stdev->ConfigSource2.BLT_S2TY |= (BLIT_TY_COPYDIR_TOPBOTTOM | BLIT_TY_COPYDIR_RIGHTLEFT);
          stdev->h_src2_sign = -1;
        }
      break;

    default:
      /* should not be reached */
      return false;
    }

  /* FIXME: 3 buffer formats */
  stdev->ConfigSource1.BLT_S1TY |= (stdev->ConfigTarget.BLT_TTY & BLIT_TY_COPYDIR_MASK);

  D_DEBUG_AT (BDISP_BLT, "  -> Generating src2 blit %s & %s\n",
              (stdev->h_src2_sign == 1) ? "left to right" : "right to left",
              (stdev->v_src2_sign == 1) ? "top to bottom" : "bottom to top");
  D_DEBUG_AT (BDISP_BLT, "  -> Generating trgt blit %s & %s\n",
              (stdev->h_trgt_sign == 1) ? "left to right" : "right to left",
              (stdev->v_trgt_sign == 1) ? "top to bottom" : "bottom to top");

  return true;
}

static bool
__attribute__((warn_unused_result))
_bdisp_aq_Blit_setup_locations (const STGFX2DriverData * const stdrv,
                                STGFX2DeviceData       * const stdev,
                                DFBRectangle           * const src2,
                                DFBRectangle           * const src1,
                                DFBRectangle           * const dst,
                                bool                    requires_spans)
{
  /* FIXME: treat src1 */
  if (stdev->h_trgt_sign == -1)
    /* target/source1 from right to left */
    dst->x += (dst->w - DFB_FIXED_POINT_ONE);
  if (stdev->v_trgt_sign == -1)
    /* target/source1 from bottom to top */
    dst->y += (dst->h - DFB_FIXED_POINT_ONE);
  if (stdev->h_src2_sign == -1)
    /* source2 from right to left */
    src2->x += (src2->w - DFB_FIXED_POINT_ONE);
  if (stdev->v_src2_sign == -1)
    /* source2 from bottom to top */
    src2->y += (src2->h - DFB_FIXED_POINT_ONE);

  if (!requires_spans)
    {
      /*
       * This is a workaround for:
       *     https://bugzilla.stlinux.com/show_bug.cgi?id=7084
       */
      if (unlikely (dst->w < DFB_FIXED_POINT_ONE))
        dst->w = DFB_FIXED_POINT_ONE;
      if (unlikely (dst->h < DFB_FIXED_POINT_ONE))
        {
          printf ("%s: dst->h %x (%d.%06d) < DFB_FIXED_POINT_ONE!\n",
                  __FUNCTION__, dst->h, DFB_INT_VALf (dst->h));
          dst->h = DFB_FIXED_POINT_ONE;
        }
      if (unlikely (src2->w < DFB_FIXED_POINT_ONE))
        {
          printf ("%s: src2->w %x (%d.%06d) < DFB_FIXED_POINT_ONE!\n",
                  __FUNCTION__, src2->w, DFB_INT_VALf (src2->w));
          src2->w = DFB_FIXED_POINT_ONE;
        }
      if (unlikely (src2->h < DFB_FIXED_POINT_ONE))
        {
          printf ("%s: src2->h %x (%d.%06d) < DFB_FIXED_POINT_ONE!\n",
                  __FUNCTION__, src2->h, DFB_INT_VALf (src2->h));
          src2->h = DFB_FIXED_POINT_ONE;
        }

      stdev->ConfigTarget.BLT_TXY   = ((DFB_FIXED_POINT_TO_INT (dst->y) << 16)
                                       | DFB_FIXED_POINT_TO_INT (dst->x));
      stdev->ConfigTarget.BLT_TSZ_S1SZ = (((DFB_FIXED_POINT_TO_INT (dst->h) & 0x0fff) << 16)
                                          | (DFB_FIXED_POINT_TO_INT (dst->w) & 0x0fff));

      stdev->ConfigSource1.BLT_S1XY = ((DFB_FIXED_POINT_TO_INT (src1->y) << 16)
                                       | DFB_FIXED_POINT_TO_INT (src1->x));
      stdev->ConfigSource2.BLT_S2XY = ((DFB_FIXED_POINT_TO_INT (src2->y) << 16)
                                       | DFB_FIXED_POINT_TO_INT (src2->x));
      stdev->ConfigSource2.BLT_S2SZ    = (((DFB_FIXED_POINT_TO_INT (src2->h) & 0x0fff) << 16)
                                          | (DFB_FIXED_POINT_TO_INT (src2->w) & 0x0fff));

      if (D_FLAGS_IS_SET (stdev->ConfigGeneral.BLT_CIC, CIC_NODE_SOURCE3))
        {
          int src2_x = DFB_FIXED_POINT_TO_INT (src2->x / stdev->srcFactorH);
          int src2_y = DFB_FIXED_POINT_TO_INT (src2->y / stdev->srcFactorV) << 16;
          int src2_w = DFB_FIXED_POINT_TO_INT (src2->w / stdev->srcFactorH) & 0x0fff;
          int src2_h = (DFB_FIXED_POINT_TO_INT (src2->h / stdev->srcFactorV) & 0x0fff) << 16;

          if (unlikely (stdev->srcFactorH != 1
                        && !src2_w))
            /* 1 luma pixel could be 0.5 chroma pixels, i.e. 0. That would
               stall the BDisp. I am not sure if it is OK to make this case
               work using this workaround, or if we rather should just deny
               such blits, though. */
            src2_w = 1;
          if (unlikely (stdev->srcFactorV != 1
                        && !src2_h))
            src2_h = 1 << 16;

          stdev->ConfigSource3.BLT_S3XY = stdev->ConfigSource2.BLT_S2XY;
          stdev->ConfigSource3.BLT_S3SZ = stdev->ConfigSource2.BLT_S2SZ;

          stdev->ConfigSource2.BLT_S2XY = src2_y | src2_x;
          stdev->ConfigSource2.BLT_S2SZ = src2_h | src2_w;

          stdev->ConfigSource1.BLT_S1XY = stdev->ConfigSource2.BLT_S2XY;

          D_DEBUG_AT (BDISP_BLT, "   S3XY: %.8x S3SZ: %.8x S2XY: %.8x S2SZ: %.8x S1XY: %.8x -> TXY %.8x TSZ %.8x (RZI %.8x Y_RZI %.8x)\n",
                      stdev->ConfigSource3.BLT_S3XY, stdev->ConfigSource3.BLT_S3SZ,
                      stdev->ConfigSource2.BLT_S2XY, stdev->ConfigSource2.BLT_S2SZ,
                      stdev->ConfigSource1.BLT_S1XY,
                      stdev->ConfigTarget.BLT_TXY, stdev->ConfigTarget.BLT_TSZ_S1SZ,
                      stdev->ConfigFiltersChr.BLT_RZI,
                      stdev->ConfigFiltersLuma.BLT_Y_RZI);
        }
      else
        D_DEBUG_AT (BDISP_BLT, "   S2XY: %.8x S2SZ: %.8x -> TXY %.8x TSZ %.8x (RZI %.8x)\n",
                    stdev->ConfigSource2.BLT_S2XY, stdev->ConfigSource2.BLT_S2SZ,
                    stdev->ConfigTarget.BLT_TXY, stdev->ConfigTarget.BLT_TSZ_S1SZ,
                    stdev->ConfigFiltersChr.BLT_RZI);
    }
  else
    {
      /* Fill in the vertical sizes as they are constant for all spans and
         it saves us from having to find out the values again. */
      stdev->ConfigTarget.BLT_TSZ_S1SZ = (DFB_FIXED_POINT_TO_INT (dst->h) & 0x0fff) << 16;
      stdev->ConfigSource2.BLT_S2SZ    = (DFB_FIXED_POINT_TO_INT (src2->h) & 0x0fff) << 16;
    }

  return true;
}


static bool
bdisp_aq_flickerfilter_setup (STGFX2DeviceData * const stdev)
{
  if (likely (!D_FLAGS_IS_SET (stdev->ConfigGeneral.BLT_INS,
                               BLIT_INS_ENABLE_FLICKERFILTER)))
    /* FIXME: at the moment BLIT_INS_ENABLE_FLICKERFILTER is never set,
       therefore the likely() is fine. Remove as soon as we make use of it. */
    return false;

  stdev->ConfigGeneral.BLT_CIC &= ~CIC_NODE_2DFILTERSLUMA;
  stdev->ConfigGeneral.BLT_CIC |= (CIC_NODE_FILTERS
                                   | CIC_NODE_2DFILTERSCHR
                                   | CIC_NODE_FLICKER);
  stdev->ConfigGeneral.BLT_INS |= (BLIT_INS_ENABLE_FLICKERFILTER
                                   | BLIT_INS_ENABLE_2DRESCALE);

  /* stdev->ConfigFlicker is initialized once and never changes */

  /* We need to turn on the whole filter/resize pipeline to use the
     flicker filter, even if we are not resizing as well. */
  stdev->ConfigFilters.BLT_FCTL_RZC = (BLIT_RZC_FF_MODE_ADAPTIVE
                                       | BLIT_RZC_2DHF_MODE_RESIZE_ONLY
                                       | BLIT_RZC_2DVF_MODE_RESIZE_ONLY);

  /* 1:1 scaling by default */
  stdev->hsrcinc = stdev->vsrcinc = DFB_FIXED_POINT_ONE;
  stdev->ConfigFiltersChr.BLT_RSF  = DFB_FIXED_POINT_TO_BDISP (stdev->vsrcinc) << 16;
  stdev->ConfigFiltersChr.BLT_RSF |= DFB_FIXED_POINT_TO_BDISP (stdev->hsrcinc);
  /* Default horizontal filter setup, repeat the first pixel 3 times to
     pre-fill the 8 Tap filter. No initial phase shift is required. */
  stdev->ConfigFiltersChr.BLT_RZI = (3 << BLIT_RZI_H_REPEAT_SHIFT);
  stdev->ConfigFiltersChr.BLT_HFP = 0;
  stdev->ConfigFiltersChr.BLT_VFP = 0;

  return true;
}

static bool
__attribute__((const))
bdisp_aq_filter_need_spans (int srcwidth,
                            u16 line_buffer_length)
{
  /* If the source width is >line_length pixels then we need to split the
     operation up into <line_length pixel vertical spans. This is because the
     filter pipeline only has line buffering for up to 128 pixels.
     In addition, even if we don't require spans for an operation we apply
     filtering and take surrounding pixels into account, similar to
     _bdisp_aq_Blit_using_spans(). This means we need additional pixels and
     therefore have to subtract 8 from the available line buffer length here,
     too. */
  return srcwidth > DFB_FIXED_POINT_VAL (line_buffer_length - 8);
}

static u32
round_f16_to_f10 (u32 f16)
{
  f16 += (1 << 5);
  f16 &= ~0x3f;

  return f16;
}

static int
__attribute__((const))
divide_f16 (int dividend, int divisor)
{
  long long temp = DFB_FIXED_POINT_VAL ((long long) dividend);
  return temp / divisor;
}

static void
Get5TapFilterSetup (const STGFX2DeviceData * const stdev,
                    DFBRectangle           * const src,
                    DFBRectangle           * const dst,
                    unsigned int           *repeat,
                    unsigned int           *phase)
{
  /* Vertical filter setup: pre-fill the 5 Tap filter, either with real
     data (if available) or just a repeat of the first pixel. */
  u32 fraction;
  dst->y = DFB_FIXED_POINT_VAL (FixedPointToInteger (dst->y, &fraction));

  fraction = (((long long) stdev->vsrcinc * fraction) / DFB_FIXED_POINT_ONE);

  src->y -= fraction;

  int distance = src->y;

  /* we need 2 pixels to init the filter */
  int _prefill = DFB_FIXED_POINT_VAL (2);

  if (distance >= _prefill)
    {
      /* this code path is untested, but should work along these lines (it
         should be unused, anyway) */
      *repeat = 0;
      src->y -= _prefill;
      src->h += _prefill;
      src->y  = DFB_FIXED_POINT_VAL (FixedPointToInteger (src->y,
                                                          phase));
    }
  else
    {
      *repeat = 2;

      src->y = DFB_FIXED_POINT_VAL (FixedPointToInteger (src->y, &fraction));
      while (src->y < 0)
        {
          *repeat += 1;
          src->y += DFB_FIXED_POINT_VAL (1);
          if (*repeat == 4)
            break;
        }
      *phase = fraction & 0x0000ffff;
      if(src->y < 0)
        src->y = 0;
    }
  /* we can not adjust the source height here (to read an extra 2 pixels),
     because that would go outside the source image, always. */
}

union _BltNodeGroup0910 {
  struct _BltNodeGroup09 *chroma;
  struct _BltNodeGroup10 *luma;
};

static void
_bdisp_aq_filtering_setup (STGFX2DeviceData        * const stdev,
                           int                      hsrcinc,
                           int                      vsrcinc,
                           union _BltNodeGroup0910 * const filter)
{
  struct _BltNodeGroup09 * __restrict flt = filter->chroma;
  int                     filterIndex;

  filterIndex = CSTmBlitter__ChooseBlitter8x8FilterCoeffs (hsrcinc);
  flt->BLT_HFP = (stdev->filter_8x8_phys
                  + filterIndex * BDISP_AQ_BLIT_FILTER_8X8_TABLE_SIZE);

  filterIndex = CSTmBlitter__ChooseBlitter5x8FilterCoeffs (vsrcinc);
  flt->BLT_VFP = (stdev->filter_5x8_phys
                  + filterIndex * BDISP_AQ_BLIT_FILTER_5X8_TABLE_SIZE);

  flt->BLT_RSF  = DFB_FIXED_POINT_TO_BDISP (vsrcinc) << 16;
  flt->BLT_RSF |= DFB_FIXED_POINT_TO_BDISP (hsrcinc);

  /* Vertical filter setup: repeat the first line twice to pre-fill the
     filter. This is _not_ the same setup as used for 5 tap video filtering
     which sets the filter phase to start at +1/2. */
  flt->BLT_RZI = 2 << BLIT_RZI_V_REPEAT_SHIFT;

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
_bdisp_aq_StretchBlit_setup (STGFX2DriverData * const stdrv,
                             STGFX2DeviceData * const stdev,
                             DFBRectangle     * const src,
                             DFBRectangle     * const dst,
                             bool             * const requires_spans)
{
  stdev->hsrcinc = divide_f16 (src->w / stdev->srcFactorH, dst->w);
  stdev->vsrcinc = divide_f16 (src->h / stdev->srcFactorV, dst->h);

  D_DEBUG_AT (BDISP_STRETCH, "StretchBlit_setup(): vsrcinc/hsrcinc: %.8x/%.8x -> %u.%06u/%u.%06u\n",
              stdev->vsrcinc, stdev->hsrcinc,
              DFB_INT_VALf (stdev->vsrcinc), DFB_INT_VALf (stdev->hsrcinc));

  stdev->hsrcinc = round_f16_to_f10 (stdev->hsrcinc);
  stdev->vsrcinc = round_f16_to_f10 (stdev->vsrcinc);

  D_DEBUG_AT (BDISP_STRETCH, "  -> rounded to                       %.8x/%.8x -> %u.%06u/%u.%06u\n",
              stdev->vsrcinc, stdev->hsrcinc,
              DFB_INT_VALf (stdev->vsrcinc), DFB_INT_VALf (stdev->hsrcinc));

  /* out of range */
  if ((stdev->hsrcinc > (DFB_FIXED_POINT_VAL (63) + 0xffc0))
      || (stdev->vsrcinc > (DFB_FIXED_POINT_VAL (63) + 0xffc0))
      || (stdev->hsrcinc < 0x40)
      || (stdev->vsrcinc < 0x40))
    return false;

  /* before setting up the resize filter, setup the flicker filter... */
  bdisp_aq_flickerfilter_setup (stdev);

  /* ...because the resize filter setup may override the default 1:1
     resize configuration set by the flicker filter; this is fine the
     vertical and flicker filters can be active at the same time. */
  *requires_spans = bdisp_aq_filter_need_spans (src->w,
                                                stdev->features.line_buffer_length);

  stdev->ConfigGeneral.BLT_CIC |= CIC_NODE_FILTERS | CIC_NODE_2DFILTERSCHR;
  stdev->ConfigGeneral.BLT_INS |= BLIT_INS_ENABLE_2DRESCALE;

  union _BltNodeGroup0910 filters = { .chroma = &stdev->ConfigFiltersChr };
  _bdisp_aq_filtering_setup (stdev, stdev->hsrcinc, stdev->vsrcinc, &filters);

  /* FIXME: remove this at some point soon */
  stdev->ConfigFilters.BLT_FCTL_RZC &= (BLIT_RZC_2DHF_MODE_MASK
                                        | BLIT_RZC_2DVF_MODE_MASK
                                        | BLIT_RZC_Y_2DHF_MODE_MASK
                                        | BLIT_RZC_Y_2DVF_MODE_MASK);

  /* FIXME: treat src1 */
  if (!_bdisp_aq_Blit_setup_directions (stdev, src, dst, dst))
    return false;

  if (unlikely (D_FLAGS_IS_SET (stdev->ConfigGeneral.BLT_CIC, CIC_NODE_SOURCE3)))
    {
      stdev->ConfigGeneral.BLT_CIC |= CIC_NODE_2DFILTERSLUMA;

      stdev->hsrcinc *= stdev->srcFactorH;
      stdev->vsrcinc *= stdev->srcFactorV;

      D_DEBUG_AT (BDISP_STRETCH, "StretchBlit_setup(): luma vsrcinc/hsrcinc: %.8x/%.8x -> %u.%06u/%u.%06u\n",
                  stdev->vsrcinc, stdev->hsrcinc,
                  DFB_INT_VALf (stdev->vsrcinc), DFB_INT_VALf (stdev->hsrcinc));

      /* out of range */
      if ((stdev->hsrcinc > (DFB_FIXED_POINT_VAL (63) + 0xffc0))
          || (stdev->vsrcinc > (DFB_FIXED_POINT_VAL (63) + 0xffc0))
          || (stdev->hsrcinc < 0x40)
          || (stdev->vsrcinc < 0x40))
        return false;

      filters.luma = &stdev->ConfigFiltersLuma;
      _bdisp_aq_filtering_setup (stdev, stdev->hsrcinc, stdev->vsrcinc,
                                 &filters);

      D_DEBUG_AT (BDISP_STRETCH, "Y_RSF: %.8x (v %d.%06d h %d.%06d) Y_RZI %.8x\n",
                  stdev->ConfigFiltersLuma.BLT_Y_RSF,
                  DFB_INT_VALf ((stdev->ConfigFiltersLuma.BLT_Y_RSF >> 16) << 6),
                  DFB_INT_VALf ((stdev->ConfigFiltersLuma.BLT_Y_RSF & 0xffff) << 6),
                  stdev->ConfigFiltersLuma.BLT_Y_RZI);
    }
  else
    {
      /* Vertical filter setup: repeat the first line twice to pre-fill the
         filter and set the filter phase to start at +1/2. This is the same
         setup used for 5 tap video filtering. */
      u32 repeat, phase;
      Get5TapFilterSetup (stdev, src, dst, &repeat, &phase);

      stdev->ConfigFiltersChr.BLT_RZI = ((repeat << BLIT_RZI_V_REPEAT_SHIFT)
                                         | (DFB_FIXED_POINT_TO_BDISP (phase) << BLIT_RZI_V_INIT_SHIFT)
                                        );

      if (!*requires_spans)
        {
          /* Horizontal filter setup: pre-fill the 8 Tap filter, either with
             real data (if available) or just a repeat of the first pixel. */
          u32 repeat, phase;

          /* we need 3 pixels to init the filter */
          int distance;
          int final_x; /* the x used when copying backwards */
          if (stdev->h_src2_sign == 1)
            {
              final_x = src->x;
              distance = src->x;
            }
          else
            {
              final_x = src->x + (src->w - DFB_FIXED_POINT_ONE);
              distance = stdev->source_w - (final_x + DFB_FIXED_POINT_ONE);
            }

          if (distance > DFB_FIXED_POINT_VAL (3))
            {
              repeat  = 0;
              src->x -= DFB_FIXED_POINT_VAL (3) * stdev->h_src2_sign;
              src->w += DFB_FIXED_POINT_VAL (3);
              src->x  = DFB_FIXED_POINT_VAL (FixedPointToInteger (src->x,
                                                                  &phase));
              if (stdev->h_src2_sign == -1)
                /* need to reverse the phase, because the tap is filled
                   backwards */
                phase = (DFB_FIXED_POINT_ONE - phase) % DFB_FIXED_POINT_ONE;
            }
          else
            {
              distance  = DFB_FIXED_POINT_VAL (FixedPointToInteger (distance,
                                                                    &phase));
              /* no need to reverse the phase for h_src2_sign == -1, because
                 the it will already be calculated that way */
              repeat  = 3 - DFB_FIXED_POINT_TO_INT (distance);
              src->w += distance;
              if (stdev->h_src2_sign == 1)
                src->x  = 0;
              else
                src->x = stdev->source_w - DFB_FIXED_POINT_ONE;
              distance = 0;
            }

          if (stdev->h_src2_sign == -1
              && phase != 0
              && distance > 0)
            {
              src->x += DFB_FIXED_POINT_ONE;
              src->w += DFB_FIXED_POINT_ONE;
            }

          /* the tap needs 4 extra pixels at the end */
          src->w += DFB_FIXED_POINT_VAL (4);
          if (stdev->h_src2_sign == 1)
            {
              if (src->x + src->w > stdev->source_w)
                src->w = (stdev->source_w - src->x);
            }
          else
            {
              if (final_x - src->w + DFB_FIXED_POINT_ONE < 0)
                src->w = final_x + DFB_FIXED_POINT_ONE;
            }

          stdev->ConfigFiltersChr.BLT_RZI |= ((repeat << BLIT_RZI_H_REPEAT_SHIFT)
                                              | (DFB_FIXED_POINT_TO_BDISP (phase) << BLIT_RZI_H_INIT_SHIFT)
                                             );
        }
    }

  D_DEBUG_AT (BDISP_STRETCH, "RSF: %.8x (v %d.%06d h %d.%06d) RZI %.8x\n",
              stdev->ConfigFiltersChr.BLT_RSF,
              DFB_INT_VALf ((stdev->ConfigFiltersChr.BLT_RSF >> 16) << 6),
              DFB_INT_VALf ((stdev->ConfigFiltersChr.BLT_RSF & 0xffff) << 6),
              stdev->ConfigFiltersChr.BLT_RZI);

  return _bdisp_aq_Blit_setup_locations (stdrv, stdev, src, dst, dst,
                                         *requires_spans);
}

/* Setup a complex blit node, with the exception of XY and Size and filter
   subposition.

   This forms a template node, potentially used to create multiple nodes,
   when doing a rescale that requires the operation to be split up into
   smaller spans. */
static bool
__attribute__((warn_unused_result))
_bdisp_aq_Blit_setup (STGFX2DriverData * const stdrv,
                      STGFX2DeviceData * const stdev,
                      DFBRectangle     * const src2,
                      DFBRectangle     * const src1,
                      DFBRectangle     * const dst,
                      bool             * const requires_spans)
{
  if (unlikely (bdisp_aq_flickerfilter_setup (stdev)))
    *requires_spans = bdisp_aq_filter_need_spans (src2->w,
                                                  stdev->features.line_buffer_length);
  else
    {
      /* we might have done a stretch blit before and do a normal blit now,
         in which case make sure to disable the rescaling engine. In the
         above case w/ the enabled flicker filter, this doesn't matter as the
         rescaling factors are reset to 1 anyway. */
      stdev->ConfigGeneral.BLT_CIC &= ~(CIC_NODE_2DFILTERSCHR
                                        | CIC_NODE_2DFILTERSLUMA);
      if (!(stdev->ConfigGeneral.BLT_INS & BLIT_INS_ENABLE_PLANEMASK))
        stdev->ConfigGeneral.BLT_CIC &= ~CIC_NODE_FILTERS;
      stdev->ConfigGeneral.BLT_INS &= ~BLIT_INS_ENABLE_2DRESCALE;

      *requires_spans = false;
    }

  return (_bdisp_aq_Blit_setup_directions (stdev, src2, src1, dst)
          && _bdisp_aq_Blit_setup_locations (stdrv, stdev, src2, src1, dst,
                                             *requires_spans));
}




typedef struct _BDispAqNodeState
{
  CorePalette *palette;
  Stgfx2Clut   palette_type;
  struct _BltNodeGroup00 ConfigGeneral;
} BDispAqNodeState;

static void
bdisp_aq_second_node_prepare (STGFX2DriverData * const stdrv,
                              STGFX2DeviceData * const stdev,
                              BDispAqNodeState * const state)
{
  D_ASSERT (state != NULL);

  state->ConfigGeneral = stdev->ConfigGeneral;
  stdev->ConfigGeneral = stdev->blitstate.extra_passes[0].ConfigGeneral;

  state->palette      = stdev->palette;
  state->palette_type = stdev->palette_type;

  stdev->palette_type = stdev->blitstate.extra_passes[0].palette_type;
  if (stdev->palette_type != SG2C_NORMAL)
    stdev->palette = NULL;

  bdisp_aq_setup_blit_operation (stdrv, stdev);
}

static void
bdisp_aq_third_node_prepare (STGFX2DriverData * const stdrv,
                             STGFX2DeviceData * const stdev)
{
  stdev->ConfigGeneral = stdev->blitstate.extra_passes[1].ConfigGeneral;

  stdev->palette_type = stdev->blitstate.extra_passes[1].palette_type;
  if (stdev->palette_type != SG2C_NORMAL)
    stdev->palette = NULL;

  bdisp_aq_setup_blit_operation (stdrv, stdev);
}

static void
bdisp_aq_second_node_release (STGFX2DriverData * const stdrv,
                              STGFX2DeviceData * const stdev,
                              BDispAqNodeState * const state)
{
  D_ASSERT (state != NULL);

  /* restore */
  stdev->ConfigGeneral = state->ConfigGeneral;
  stdev->palette = state->palette;
  stdev->palette_type = state->palette_type;

  bdisp_aq_setup_blit_operation (stdrv, stdev);
}


static bool
_check_size_constraints (int x,
                         int y,
                         int w,
                         int h)
{
  if (unlikely (x < -4096 || y < -4096))
    return false;
  if (unlikely (x > 4095 || y > 4095))
    return false;
  if (unlikely (w < 1 || h < 1))
    return false;
  if (unlikely (w > 4095 || h > 4095))
    return false;

  return true;
}

static bool
_check_size_constraints_f16 (int x,
                             int y,
                             int w,
                             int h)
{
  if (unlikely (x < DFB_FIXED_POINT_VAL (-4096)
                || y < DFB_FIXED_POINT_VAL (-4096)))
    return false;
  if (unlikely (x > DFB_FIXED_POINT_VAL (4095)
                || y > DFB_FIXED_POINT_VAL (4095)))
    return false;
  if (unlikely (w <= 0 || h <= 0))
    return false;
  if (unlikely (w > DFB_FIXED_POINT_VAL (4095)
                || h > DFB_FIXED_POINT_VAL (4095)))
    return false;

  return true;
}

bool
bdisp_aq_FillRectangle_simple (void         * const drv,
                               void         * const dev,
                               DFBRectangle * const rect)
{
  STGFX2DriverData     * const stdrv = drv;
  STGFX2DeviceData     * const stdev = dev;
  STMFBBDispSharedArea * const shared = stdrv->bdisp_shared;

  D_DEBUG_AT (BDISP_BLT, "%s (%d, %d - %dx%d)\n", __FUNCTION__,
              DFB_RECTANGLE_VALS (rect));
  DUMP_INFO ();

  if (unlikely (!rect->w || !rect->h))
    return true;
  if (!_check_size_constraints (rect->x, rect->y, rect->w, rect->h))
    return false;

  struct _BltNode_Fast * node =
    _bdisp_aq_get_new_node_prepared_simple (stdrv, stdev, shared);

  /* dimensions */
  node->common.ConfigTarget.BLT_TXY
    = (rect->y << 16) | rect->x;
  node->common.ConfigTarget.BLT_TSZ_S1SZ
    = (rect->h << 16) | rect->w;

  _bdisp_aq_finish_node (stdrv, stdev, shared, node);

  return true;
}

bool
bdisp_aq_FillRectangle (void         * const drv,
                        void         * const dev,
                        DFBRectangle * const rect)
{
  STGFX2DriverData     * const stdrv = drv;
  STGFX2DeviceData     * const stdev = dev;
  STMFBBDispSharedArea * const shared = stdrv->bdisp_shared;

  D_DEBUG_AT (BDISP_BLT, "%s (%d, %d - %dx%d)\n", __FUNCTION__,
              DFB_RECTANGLE_VALS (rect));
  DUMP_INFO ();

  if (unlikely (!rect->w || !rect->h))
    return true;
  if (!_check_size_constraints (rect->x, rect->y, rect->w, rect->h))
    return false;

  struct _BltNode_FullFill * const node =
    _bdisp_aq_get_new_node_prepared_slow (stdrv, stdev, shared);

  /* dimensions */
  node->common.ConfigTarget.BLT_TXY
    = node->common.ConfigSource1.BLT_S1XY
    = node->ConfigSource2.BLT_S2XY
    = (rect->y << 16) | rect->x;
  node->common.ConfigTarget.BLT_TSZ_S1SZ
    = node->ConfigSource2.BLT_S2SZ
    = (rect->h << 16) | rect->w;

  _bdisp_aq_finish_node (stdrv, stdev, shared, node);

  return true;
}


bool
bdisp_aq_DrawRectangle_simple (void         * const drv,
                               void         * const dev,
                               DFBRectangle * const rect)
{
  STGFX2DriverData     * const stdrv = drv;
  STGFX2DeviceData     * const stdev = dev;
  STMFBBDispSharedArea * const shared = stdrv->bdisp_shared;

  struct _BltNode_Fast *node;

  D_DEBUG_AT (BDISP_BLT, "%s (%d, %d - %dx%d)\n", __FUNCTION__,
              DFB_RECTANGLE_VALS (rect));
  DUMP_INFO ();

  if (unlikely (!rect->w || !rect->h))
    return true;
  if (!_check_size_constraints (rect->x, rect->y, rect->w, rect->h))
    return false;

  if (unlikely (rect->h == 2 || rect->w == 2))
    return bdisp_aq_FillRectangle_simple (drv, dev, rect);

  /* this is optimized for w >= h, we could optimize this for the case w < h,
     or maybe do it in a more generic way. but we don't care. */
  {
  /* top line */
  node = _bdisp_aq_get_new_node_prepared_simple (stdrv, stdev, shared);
  node->common.ConfigTarget.BLT_TXY
    = (rect->y << 16) | rect->x;
  node->common.ConfigTarget.BLT_TSZ_S1SZ = (1 << 16) | rect->w;
  _bdisp_aq_finish_node (stdrv, stdev, shared, node);
  }

  if (likely (rect->h > 1))
    {
      int y2 = rect->y + 1;
      int h2 = rect->h - 2;

      /* left line */
      node = _bdisp_aq_get_new_node_prepared_simple (stdrv, stdev, shared);
      node->common.ConfigTarget.BLT_TXY
        = (y2 << 16) | rect->x;
      node->common.ConfigTarget.BLT_TSZ_S1SZ = (h2 << 16) | 1;
      _bdisp_aq_finish_node (stdrv, stdev, shared, node);

      if (likely (rect->w > 1))
        {
          /* right line */
          node = _bdisp_aq_get_new_node_prepared_simple (stdrv, stdev, shared);
          node->common.ConfigTarget.BLT_TXY
            = (y2 << 16) | (rect->x + rect->w - 1);
          node->common.ConfigTarget.BLT_TSZ_S1SZ = (h2 << 16) | 1;
          _bdisp_aq_finish_node (stdrv, stdev, shared, node);
        }

      /* bottom line */
      node = _bdisp_aq_get_new_node_prepared_simple (stdrv, stdev, shared);
      node->common.ConfigTarget.BLT_TXY
        = ((rect->y + rect->h - 1) << 16) | rect->x;
      node->common.ConfigTarget.BLT_TSZ_S1SZ = (1 << 16) | rect->w;
      _bdisp_aq_finish_node (stdrv, stdev, shared, node);
    }

  return true;
}


bool
bdisp_aq_DrawRectangle (void         * const drv,
                        void         * const dev,
                        DFBRectangle * const rect)
{
  STGFX2DriverData     * const stdrv = drv;
  STGFX2DeviceData     * const stdev = dev;
  STMFBBDispSharedArea * const shared = stdrv->bdisp_shared;

  struct _BltNode_FullFill *node;

  D_DEBUG_AT (BDISP_BLT, "%s (%d, %d - %dx%d)\n", __FUNCTION__,
              DFB_RECTANGLE_VALS (rect));
  DUMP_INFO ();

  if (unlikely (!rect->w || !rect->h))
    return true;
  if (!_check_size_constraints (rect->x, rect->y, rect->w, rect->h))
    return false;

  if (unlikely (rect->h == 2 || rect->w == 2))
    return bdisp_aq_FillRectangle (drv, dev, rect);

  /* FIXME: instead of calling _bdisp_aq_get_new_node_prepared_slow() multiple
     times, we could probably just memcpy() the first one, but then, the
     source memory would be uncached, need to check which method is actually
     faster... */
  node = _bdisp_aq_get_new_node_prepared_slow (stdrv, stdev, shared);

  /* this is optimized for w >= h, we could optimize this for the case w < h,
     or maybe do it in a more generic way. but we don't care. */
  {
  /* top line */
  node->common.ConfigTarget.BLT_TXY
    = node->common.ConfigSource1.BLT_S1XY
    = node->ConfigSource2.BLT_S2XY
    = (rect->y << 16) | rect->x;
  node->common.ConfigTarget.BLT_TSZ_S1SZ
    = node->ConfigSource2.BLT_S2SZ
    = (1 << 16) | rect->w;
  _bdisp_aq_finish_node (stdrv, stdev, shared, node);
  }

  if (likely (rect->h > 1))
    {
      int y2 = rect->y + 1;
      int h2 = rect->h - 2;

      /* left line */
      node = _bdisp_aq_get_new_node_prepared_slow (stdrv, stdev, shared);
      node->common.ConfigTarget.BLT_TXY
        = node->common.ConfigSource1.BLT_S1XY
        = node->ConfigSource2.BLT_S2XY
        = (y2 << 16) | rect->x;
      node->common.ConfigTarget.BLT_TSZ_S1SZ
        = node->ConfigSource2.BLT_S2SZ
        = (h2 << 16) | 1;
      _bdisp_aq_finish_node (stdrv, stdev, shared, node);

      if (likely (rect->w > 1))
        {
          /* right line */
          node = _bdisp_aq_get_new_node_prepared_slow (stdrv, stdev, shared);
          node->common.ConfigTarget.BLT_TXY
            = node->common.ConfigSource1.BLT_S1XY
            = node->ConfigSource2.BLT_S2XY
            = (y2 << 16) | (rect->x + rect->w - 1);
          node->common.ConfigTarget.BLT_TSZ_S1SZ
            = node->ConfigSource2.BLT_S2SZ
            = (h2 << 16) | 1;
          _bdisp_aq_finish_node (stdrv, stdev, shared, node);
        }

      /* bottom line */
      node = _bdisp_aq_get_new_node_prepared_slow (stdrv, stdev, shared);
      node->common.ConfigTarget.BLT_TXY
        = node->common.ConfigSource1.BLT_S1XY
        = node->ConfigSource2.BLT_S2XY
        = ((rect->y + rect->h - 1) << 16) | rect->x;
      node->common.ConfigTarget.BLT_TSZ_S1SZ
        = node->ConfigSource2.BLT_S2SZ
        = (1 << 16) | rect->w;
      _bdisp_aq_finish_node (stdrv, stdev, shared, node);
    }

  return true;
}


bool
bdisp_aq_Blit_simple (void         * const drv,
                      void         * const dev,
                      DFBRectangle * const rect,
                      int           dx,
                      int           dy)
{
  STGFX2DriverData     * const stdrv = drv;
  STGFX2DeviceData     * const stdev = dev;
  STMFBBDispSharedArea * const shared = stdrv->bdisp_shared;
  bool                  copy_backwards;

  if (!stdev->bFixedPoint)
    D_DEBUG_AT (BDISP_BLT, "%s (%d,%d-%dx%d -> %d,%d)\n", __FUNCTION__,
                DFB_RECTANGLE_VALS (rect), dx, dy);
  else
    {
      D_DEBUG_AT (BDISP_BLT, "%s (%d,%d-%dx%d -> %d,%d)\n", __FUNCTION__,
                  DFB_RECTANGLE_VALS (rect), dx, dy);
      dfb_rectangle_downscale (rect);
      dx /= DFB_FIXED_POINT_ONE;
      dy /= DFB_FIXED_POINT_ONE;
    }
  DUMP_INFO ();

  if (unlikely (!rect->w || !rect->h))
    return true;
  if (!_check_size_constraints (rect->x, rect->y, rect->w, rect->h))
    goto error;
  if (!_check_size_constraints (dx, dy, 1, 1))
    goto error;

  struct _BltNode_Fast * const node = _bdisp_aq_get_new_node (stdrv, stdev,
                                                              shared);
  u32 blt_cic = CIC_BLT_NODE_FAST;
  u32 blt_ins = BLIT_INS_SRC1_MODE_DIRECT_COPY;
  u32 tty;
  u32 s1ty;

  bdisp_aq_update_num_ops (stdev, shared, blt_ins);

  /* target */
  tty = bdisp_ty_sanitise_direction (stdev->ConfigTarget.BLT_TTY);
  node->common.ConfigTarget.BLT_TBA = stdev->ConfigTarget.BLT_TBA;

  /* source (taken from source2 setup) */
  node->common.ConfigSource1.BLT_S1BA = stdev->ConfigSource2.BLT_S2BA;
  s1ty = (bdisp_ty_sanitise_direction (stdev->ConfigSource2.BLT_S2TY)
          & ~BLIT_TY_COLOR_EXPAND_MASK);

  D_ASSUME ((s1ty & BLIT_TY_COLOR_FORM_MASK) == (tty & BLIT_TY_COLOR_FORM_MASK));

  /* work out copy direction */
  copy_backwards = false;
  if (unlikely ((stdev->ConfigSource2.BLT_S2BA == stdev->ConfigTarget.BLT_TBA)
                && (dy > rect->y
                    || ((dy == rect->y) && (dx > rect->x)))))
    {
      DFBRectangle dst = { .x = dx, .y = dy, .w = rect->w, .h = rect->h };
      copy_backwards = _bdisp_aq_rectangles_intersect (rect, &dst);
    }

  /* and set up coordinates & size */
  if (!copy_backwards)
    {
      node->common.ConfigSource1.BLT_S1XY = (rect->y << 16) | rect->x;
      node->common.ConfigTarget.BLT_TXY = (dy << 16) | dx;
    }
  else
    {
      s1ty |= (BLIT_TY_COPYDIR_BOTTOMTOP | BLIT_TY_COPYDIR_RIGHTLEFT);
      tty  |= (BLIT_TY_COPYDIR_BOTTOMTOP | BLIT_TY_COPYDIR_RIGHTLEFT);

      node->common.ConfigSource1.BLT_S1XY
        = ((rect->y + rect->h - 1) << 16) | (rect->x + rect->w - 1);
      node->common.ConfigTarget.BLT_TXY
        = ((dy + rect->h - 1) << 16) | (dx + rect->w - 1);
    }

  node->common.ConfigTarget.BLT_TTY   = tty;
  node->common.ConfigSource1.BLT_S1TY = s1ty;

  node->common.ConfigTarget.BLT_TSZ_S1SZ = (rect->h << 16) | rect->w;

  node->common.ConfigGeneral.BLT_CIC = blt_cic;
  node->common.ConfigGeneral.BLT_INS = blt_ins;
  /* BLT_ACK is ignored by the hardware in this case... */

  bdisp_aq_update_config_static (shared, &node->ConfigStatic);

  _bdisp_aq_finish_node (stdrv, stdev, shared, node);

  return true;

error:
  if (stdev->bFixedPoint)
    dfb_rectangle_upscale (rect);
  return false;
}

bool
bdisp_aq_Blit_simple_YCbCr422r (void         * const drv,
                                void         * const dev,
                                DFBRectangle * const rect,
                                int           dx,
                                int           dy)
{
  STGFX2DeviceData * const stdev = dev;
  u32               tty = stdev->ConfigTarget.BLT_TTY;
  u32               s2ty = stdev->ConfigSource2.BLT_S2TY;
  bool              result;

  stdev->ConfigTarget.BLT_TTY &= ~(BLIT_TY_COLOR_FORM_MASK | BLIT_TY_FULL_ALPHA_RANGE | BLIT_TY_BIG_ENDIAN);
  stdev->ConfigTarget.BLT_TTY |= BLIT_COLOR_FORM_RGB565;
  stdev->ConfigSource2.BLT_S2TY &= ~(BLIT_TY_COLOR_FORM_MASK | BLIT_TY_FULL_ALPHA_RANGE | BLIT_TY_BIG_ENDIAN);
  stdev->ConfigSource2.BLT_S2TY |= BLIT_COLOR_FORM_RGB565;

  result = bdisp_aq_Blit_simple (drv, dev, rect, dx, dy);

  stdev->ConfigSource2.BLT_S2TY = s2ty;
  stdev->ConfigTarget.BLT_TTY  = tty;

  return result;
}

bool
bdisp_aq_Blit (void         * const drv,
               void         * const dev,
               DFBRectangle * const rect,
               int           dx,
               int           dy)
{
  STGFX2DriverData * const stdrv = drv;
  STGFX2DeviceData * const stdev = dev;
  bool              requires_spans;

  if (!stdev->bFixedPoint)
    {
      dfb_rectangle_upscale (rect);
      dx *= DFB_FIXED_POINT_ONE;
      dy *= DFB_FIXED_POINT_ONE;
    }
  D_DEBUG_AT (BDISP_BLT, "%s (%d.%06d,%d.%06d-%d.%06dx%d.%06d -> %d.%06d,%d.%06d)\n", __FUNCTION__,
              DFB_RECTANGLE_VALSf (rect), DFB_INT_VALf (dx),
              DFB_INT_VALf (dy));
  DUMP_INFO ();

  /* split buffer formats _always_ need the rescale engine enabled. */
  D_ASSERT (!D_FLAGS_IS_SET (stdev->ConfigGeneral.BLT_CIC, CIC_NODE_SOURCE3));

  if (unlikely (!rect->w || !rect->h))
    return true;
  if (!_check_size_constraints_f16 (rect->x, rect->y, rect->w, rect->h))
    goto error;
  if (!_check_size_constraints_f16 (dx, dy, 1, 1))
    goto error;

  DFBRectangle rect_backup;
  if (stdev->blitstate.n_passes > 1)
    rect_backup = *rect;

  DFBRectangle dst = { dx, dy, rect->w, rect->h };
  if (unlikely (!_bdisp_aq_Blit_setup (stdrv, stdev, rect, &dst, &dst,
                                       &requires_spans)))
    goto error;

  if (likely (!requires_spans))
    {
      bdisp_aq_copy_n_push_node (stdrv, stdev);
      bdisp_aq_clut_update_disable_if_unsafe (stdev);
    }
  else
    _bdisp_aq_Blit_using_spans (stdrv, stdev, rect, &dst);


  if (stdev->blitstate.n_passes > 1)
    {
      BDispAqNodeState state;
      bool             dummy;

      bdisp_aq_second_node_prepare (stdrv, stdev, &state);

      *rect = rect_backup;
      dst.x = dx; dst.y = dy; dst.w = rect->w; dst.h = rect->h;
      /* setup did work earlier, so it just has to work a second time, too! */
      dummy = _bdisp_aq_Blit_setup (stdrv, stdev, rect, &dst, &dst,
                                    &requires_spans);
      (void) dummy;

      if (likely (!requires_spans))
        {
          bdisp_aq_copy_n_push_node (stdrv, stdev);
          bdisp_aq_clut_update_disable_if_unsafe (stdev);
        }
      else
        _bdisp_aq_Blit_using_spans (stdrv, stdev, rect, &dst);

      if (stdev->blitstate.n_passes > 2)
        {
          bdisp_aq_third_node_prepare (stdrv, stdev);

          *rect = rect_backup;
          dst.x = dx; dst.y = dy; dst.w = rect->w; dst.h = rect->h;
          /* setup did work earlier, so it just has to work a second time, too! */
          dummy = _bdisp_aq_Blit_setup (stdrv, stdev, rect, &dst, &dst,
                                        &requires_spans);
          (void) dummy;

          if (likely (!requires_spans))
            {
              bdisp_aq_copy_n_push_node (stdrv, stdev);
              bdisp_aq_clut_update_disable_if_unsafe (stdev);
            }
          else
            _bdisp_aq_Blit_using_spans (stdrv, stdev, rect, &dst);
        }

      bdisp_aq_second_node_release (stdrv, stdev, &state);
    }


  return true;

error:
  if (!stdev->bFixedPoint)
    dfb_rectangle_downscale (rect);
  return false;
}


bool
bdisp_aq_StretchBlit (void         * const drv,
                      void         * const dev,
                      DFBRectangle * const srect,
                      DFBRectangle * const drect)
{
  STGFX2DriverData * const stdrv = drv;
  STGFX2DeviceData * const stdev = dev;
  bool              requires_spans;

  if (!stdev->bFixedPoint)
    {
      dfb_rectangle_upscale (srect);
      dfb_rectangle_upscale (drect);
    }
  D_DEBUG_AT (BDISP_STRETCH, "%s (%d.%06d,%d.%06d-%d.%06dx%d.%06d -> %d.%06d,%d.%06d-%d.%06dx%d.%06d)\n", __FUNCTION__,
              DFB_RECTANGLE_VALSf (srect), DFB_RECTANGLE_VALSf (drect));
  DUMP_INFO ();

  if (unlikely (!srect->w || !srect->h || !drect->w || !drect->h))
    return true;
  if (!_check_size_constraints_f16 (srect->x, srect->y, srect->w, srect->h))
    goto error;
  if (!_check_size_constraints_f16 (drect->x, drect->y, drect->w, drect->h))
    goto error;
  /* FIXME: something in WebKit/GTK+/Cairo ends up setting a stretchblit to
     a destination size of 1x1. We should special case this by using just some
     random pixel value in the input (maybe the top left one?) */

  DFBRectangle srect_backup;
  DFBRectangle drect_backup;
  if (stdev->blitstate.n_passes > 1)
    {
      srect_backup = *srect;
      drect_backup = *drect;
    }

  if (!_bdisp_aq_StretchBlit_setup (stdrv, stdev, srect, drect,
                                    &requires_spans))
    goto error;

  if (!requires_spans)
    {
      bdisp_aq_copy_n_push_node (stdrv, stdev);
      bdisp_aq_clut_update_disable_if_unsafe (stdev);
    }
  else
    _bdisp_aq_Blit_using_spans (stdrv, stdev, srect, drect);


  if (stdev->blitstate.n_passes > 1)
    {
      BDispAqNodeState state;
      bool             dummy;

      bdisp_aq_second_node_prepare (stdrv, stdev, &state);

      *srect = srect_backup;
      *drect = drect_backup;
      /* setup did work earlier, so it just has to work a second time, too! */
      dummy = _bdisp_aq_StretchBlit_setup (stdrv, stdev, srect, drect,
                                           &requires_spans);
      (void) dummy;

      if (!requires_spans)
        {
          bdisp_aq_copy_n_push_node (stdrv, stdev);
          bdisp_aq_clut_update_disable_if_unsafe (stdev);
        }
      else
        _bdisp_aq_Blit_using_spans (stdrv, stdev, srect, drect);

      if (stdev->blitstate.n_passes > 2)
        {
          bdisp_aq_third_node_prepare (stdrv, stdev);

          *srect = srect_backup;
          *drect = drect_backup;
          /* setup did work earlier, so it just has to work a second time, too! */
          dummy = _bdisp_aq_StretchBlit_setup (stdrv, stdev, srect, drect,
                                               &requires_spans);
          (void) dummy;

          if (!requires_spans)
            {
              bdisp_aq_copy_n_push_node (stdrv, stdev);
              bdisp_aq_clut_update_disable_if_unsafe (stdev);
            }
          else
            _bdisp_aq_Blit_using_spans (stdrv, stdev, srect, drect);
        }

      bdisp_aq_second_node_release (stdrv, stdev, &state);
    }

  return true;

error:
  if (!stdev->bFixedPoint)
    {
      dfb_rectangle_downscale (srect);
      dfb_rectangle_downscale (drect);
    }
  return false;
}

bool
bdisp_aq_Blit_as_stretch (void         * const drv,
                          void         * const dev,
                          DFBRectangle * const rect,
                          int           dx,
                          int           dy)
{
  DFBRectangle drect = { .x = dx, .y = dy, .w = rect->w, .h = rect->h };
  return bdisp_aq_StretchBlit (drv, dev, rect, &drect);
}

bool
bdisp_aq_Blit2 (void         * const drv,
                void         * const dev,
                DFBRectangle * const rect,
                int           dx,
                int           dy,
                int           sx2,
                int           sy2)
{
  STGFX2DriverData * const stdrv = drv;
  STGFX2DeviceData * const stdev = dev;
  bool              requires_spans;

  if (!stdev->bFixedPoint)
    {
      dfb_rectangle_upscale (rect);
      dx *= DFB_FIXED_POINT_ONE;
      dy *= DFB_FIXED_POINT_ONE;
      sx2 *= DFB_FIXED_POINT_ONE;
      sy2 *= DFB_FIXED_POINT_ONE;
    }
  D_DEBUG_AT (BDISP_BLT, "%s (%d.%06d,%d.%06d-%d.%06dx%d.%06d + %d.%06d,%d.%06d -> %d.%06d,%d.%06d)\n", __FUNCTION__,
              DFB_RECTANGLE_VALSf (rect), DFB_INT_VALf (sx2),
              DFB_INT_VALf (sy2), DFB_INT_VALf (dx), DFB_INT_VALf (dy));
  DUMP_INFO ();

  /* split buffer formats _always_ need the rescale engine enabled. */
  D_ASSERT (!D_FLAGS_IS_SET (stdev->ConfigGeneral.BLT_CIC, CIC_NODE_SOURCE3));

  if (unlikely (!rect->w || !rect->h))
    return true;
  if (!_check_size_constraints_f16 (rect->x, rect->y, rect->w, rect->h))
    goto error;
  if (!_check_size_constraints_f16 (dx, dy, 1, 1))
    goto error;
  if (!_check_size_constraints_f16 (sx2, sy2, 1, 1))
    goto error;

  DFBRectangle rect_backup;
  if (stdev->blitstate.n_passes > 1)
    rect_backup = *rect;

  DFBRectangle src1 = { sx2, sy2, rect->w, rect->h };
  DFBRectangle dst = { dx, dy, rect->w, rect->h };
  if (unlikely (!_bdisp_aq_Blit_setup (stdrv, stdev, rect, &src1, &dst,
                                       &requires_spans)))
    goto error;

  if (likely (!requires_spans))
    {
      bdisp_aq_copy_n_push_node (stdrv, stdev);
      bdisp_aq_clut_update_disable_if_unsafe (stdev);
    }
  else
    _bdisp_aq_Blit_using_spans (stdrv, stdev, rect, &dst);


  if (stdev->blitstate.n_passes > 1)
    {
      BDispAqNodeState state;
      bool             dummy;

      bdisp_aq_second_node_prepare (stdrv, stdev, &state);

      *rect = rect_backup;
      src1.x = sx2; src1.y = sy2; src1.w = rect->w; src1.h = rect->h;
      dst.x = dx; dst.y = dy; dst.w = rect->w; dst.h = rect->h;
      /* setup did work earlier, so it just has to work a second time, too! */
      dummy = _bdisp_aq_Blit_setup (stdrv, stdev, rect, &src1, &dst,
                                    &requires_spans);
      (void) dummy;

      if (likely (!requires_spans))
        {
          bdisp_aq_copy_n_push_node (stdrv, stdev);
          bdisp_aq_clut_update_disable_if_unsafe (stdev);
        }
      else
        _bdisp_aq_Blit_using_spans (stdrv, stdev, rect, &dst);

      if (stdev->blitstate.n_passes > 2)
        {
          bdisp_aq_third_node_prepare (stdrv, stdev);

          *rect = rect_backup;
          dst.x = dx; dst.y = dy; dst.w = rect->w; dst.h = rect->h;
          src1.x = sx2; src1.y = sy2; src1.w = rect->w; src1.h = rect->h;
          /* setup did work earlier, so it just has to work a second time, too! */
          dummy = _bdisp_aq_Blit_setup (stdrv, stdev, rect, &src1, &dst,
                                        &requires_spans);
          (void) dummy;

          if (likely (!requires_spans))
            {
              bdisp_aq_copy_n_push_node (stdrv, stdev);
              bdisp_aq_clut_update_disable_if_unsafe (stdev);
            }
          else
            _bdisp_aq_Blit_using_spans (stdrv, stdev, rect, &dst);
        }

      bdisp_aq_second_node_release (stdrv, stdev, &state);
    }


  return true;

error:
  if (!stdev->bFixedPoint)
    dfb_rectangle_downscale (rect);
  return false;
}


bool
bdisp_aq_StretchBlit_RLE (void                        * const drv,
                          void                        * const dev,
                          unsigned long                src_address,
                          unsigned long                src_length,
                          const CoreSurfaceBufferLock * const dst,
                          const DFBRectangle          * const drect)
{
  STGFX2DriverData     * const stdrv = drv;
  STGFX2DeviceData     * const stdev = dev;
  STMFBBDispSharedArea * const shared = stdrv->bdisp_shared;

  D_DEBUG_AT (BDISP_BLT, "%s (%lu -> %d,%d-%dx%d)\n", __FUNCTION__,
              src_length, DFB_RECTANGLE_VALS (drect));
  DUMP_INFO ();

  /* FIXME: FIXED_POINT support? */

  if (unlikely (!src_length))
    return true;
  if (unlikely (src_length > 0x00ffffff))
    return false;
  if (!_check_size_constraints (drect->x, drect->y, drect->w, drect->h))
    return false;
  if (unlikely (!(stdev->features.blitflags & STM_BLITTER_FLAGS_RLE_DECODE)))
    return false;

  struct _BltNode_Fast * const node = _bdisp_aq_get_new_node (stdrv, stdev,
                                                              shared);
  u32 blt_ins = BLIT_INS_SRC1_MODE_MEMORY;

  bdisp_aq_update_num_ops (stdev, shared, blt_ins);

  /* target */
  node->common.ConfigTarget.BLT_TBA = dst->phys;
  _bdisp_state_set_buffer_type (stdev, &node->common.ConfigTarget.BLT_TTY,
                                dst->buffer->format, dst->pitch);
  node->common.ConfigTarget.BLT_TXY = (drect->y << 16) | drect->x;
  node->common.ConfigTarget.BLT_TSZ_S1SZ = (drect->h << 16) | drect->w;

  /* source */
  node->common.ConfigSource1.BLT_S1BA = src_address;
  node->common.ConfigSource1.BLT_S1TY = BLIT_COLOR_FORM_RLD_BD;
  node->common.ConfigSource1.BLT_S1XY = src_length;

  node->common.ConfigGeneral.BLT_CIC = CIC_BLT_NODE_FAST;
  node->common.ConfigGeneral.BLT_INS = blt_ins;
  node->common.ConfigGeneral.BLT_ACK = BLIT_ACK_BYPASSSOURCE1;

  bdisp_aq_update_config_static (shared, &node->ConfigStatic);

  _bdisp_aq_finish_node (stdrv, stdev, shared, node);

  return true;
}


static bool
bdisp_aq_Blit_shortcut_real (void         * const drv,
                             void         * const dev,
                             DFBRectangle * const rect,
                             int           dx,
                             int           dy,
                             u32           color)
{
  STGFX2DriverData     * const stdrv = drv;
  STGFX2DeviceData     * const stdev = dev;
  STMFBBDispSharedArea * const shared = stdrv->bdisp_shared;

  if (!stdev->bFixedPoint)
    D_DEBUG_AT (BDISP_BLT, "%s (%d,%d-%dx%d -> %d,%d)\n", __FUNCTION__,
                DFB_RECTANGLE_VALS (rect), dx, dy);
  else
    {
      D_DEBUG_AT (BDISP_BLT, "%s (%d.%06d,%d.%06d-%d.%06dx%d.%06d -> %d.%06d,%d.%06d)\n",
                  __FUNCTION__, DFB_RECTANGLE_VALSf (rect),
                  DFB_INT_VALf (dx), DFB_INT_VALf (dy));
      dfb_rectangle_downscale (rect);
      dx /= DFB_FIXED_POINT_ONE;
      dy /= DFB_FIXED_POINT_ONE;
    }
  DUMP_INFO ();

  if (unlikely (!rect->w || !rect->h))
    return true;
  if (!_check_size_constraints (rect->x, rect->y, rect->w, rect->h))
    goto error;
  if (!_check_size_constraints (dx, dy, 1, 1))
    goto error;

  struct _BltNode_Fast * node =
    _bdisp_aq_get_new_node_prepared_simple (stdrv, stdev, shared);

  /* this must be DSPD_CLEAR, set source1 == destination. For YUV formats,
     we let the hardware do conversion from ARGB, thus will not get here */
  D_ASSERT (!BLIT_TY_COLOR_FORM_IS_YUV (stdev->ConfigTarget.BLT_TTY));

  node->common.ConfigSource1.BLT_S1TY =
    (bdisp_ty_sanitise_direction (stdev->ConfigTarget.BLT_TTY)
     & ~BLIT_TY_BIG_ENDIAN);
  node->common.ConfigColor.BLT_S1CF = color;

  /* dimensions */
  node->common.ConfigTarget.BLT_TXY
    = (dy << 16) | dx;
  node->common.ConfigTarget.BLT_TSZ_S1SZ
    = (rect->h << 16) | rect->w;

  _bdisp_aq_finish_node (stdrv, stdev, shared, node);

  return true;
error:
  if (stdev->bFixedPoint)
    dfb_rectangle_upscale (rect);
  return false;
}

bool
bdisp_aq_Blit_shortcut (void         * const drv,
                        void         * const dev,
                        DFBRectangle * const rect,
                        int           dx,
                        int           dy)
{
  return bdisp_aq_Blit_shortcut_real (drv, dev, rect, dx, dy, 0x00000000);
}

bool
bdisp_aq_StretchBlit_shortcut (void         * const drv,
                               void         * const dev,
                               DFBRectangle * const srect,
                               DFBRectangle * const drect)
{
  return bdisp_aq_Blit_shortcut (drv, dev, drect, drect->x, drect->y);
}

bool
bdisp_aq_Blit2_shortcut (void         * const drv,
                         void         * const dev,
                         DFBRectangle * const rect,
                         int           dx,
                         int           dy,
                         int           sx2,
                         int           sy2)
{
  return bdisp_aq_Blit_shortcut (drv, dev, rect, dx, dy);
}

bool
bdisp_aq_Blit_shortcut_YCbCr422r (void         * const drv,
                                  void         * const dev,
                                  DFBRectangle * const rect,
                                  int           dx,
                                  int           dy)
{
  STGFX2DriverData     * const stdrv = drv;
  STGFX2DeviceData     * const stdev = dev;
  STMFBBDispSharedArea * const shared = stdrv->bdisp_shared;

  if (!stdev->bFixedPoint)
    D_DEBUG_AT (BDISP_BLT, "%s (%d,%d-%dx%d -> %d,%d)\n", __FUNCTION__,
                DFB_RECTANGLE_VALS (rect), dx, dy);
  else
    {
      D_DEBUG_AT (BDISP_BLT, "%s (%d.%06d,%d.%06d-%d.%06dx%d.%06d -> %d.%06d,%d.%06d)\n",
                  __FUNCTION__, DFB_RECTANGLE_VALSf (rect),
                  DFB_INT_VALf (dx), DFB_INT_VALf (dy));
      dfb_rectangle_downscale (rect);
      dx /= DFB_FIXED_POINT_ONE;
      dy /= DFB_FIXED_POINT_ONE;
    }
  DUMP_INFO ();

  if (unlikely (!rect->w || !rect->h))
    return true;
  if (!_check_size_constraints (dx, dy, rect->w, rect->h))
    goto error;

  struct _BltNode_YCbCr422r_shortcut * node =
    _bdisp_aq_get_new_node (stdrv, stdev, shared);

  u32 blt_cic = CIC_BLT_NODE_YCbCr422r_SHORTCUT;
  u32 blt_ins = (BLIT_INS_SRC1_MODE_DISABLED
                 | BLIT_INS_SRC2_MODE_COLOR_FILL
                 | BLIT_INS_ENABLE_IVMX);
  u32 blt_ack = BLIT_ACK_BYPASSSOURCE2;

  D_ASSERT (shared == stdrv->bdisp_shared);

  node->ConfigTarget.BLT_TBA = stdev->ConfigTarget.BLT_TBA;
  node->ConfigTarget.BLT_TTY = bdisp_ty_sanitise_direction (stdev->ConfigTarget.BLT_TTY);

  node->ConfigColor.BLT_S2CF = 0x00000000;
  node->ConfigSource2.BLT_S2TY = (BLIT_COLOR_FORM_ARGB8888
                                  | BLIT_TY_COLOR_EXPAND_MSB);

  node->ConfigTarget.BLT_TXY
    = node->ConfigSource2.BLT_S2XY
    = (dy << 16) | dx;
  node->ConfigTarget.BLT_TSZ_S1SZ
    = node->ConfigSource2.BLT_S2SZ
    = (rect->h << 16) | rect->w;

  bdisp_aq_SetClip (stdev, &blt_cic, &blt_ins, &node->ConfigClip);
  bdisp_aq_update_config_static (shared, node->ConfigStatic);

  set_matrix (node->ConfigIVMX.BLT_IVMX, bdisp_aq_RGB_2_VideoYCbCr601);

  bdisp_aq_update_num_ops (stdev, shared, blt_ins);

  node->ConfigGeneral.BLT_CIC = blt_cic;
  node->ConfigGeneral.BLT_INS = blt_ins;
  node->ConfigGeneral.BLT_ACK = blt_ack;

  _bdisp_aq_finish_node (stdrv, stdev, shared, node);

  return true;
error:
  if (stdev->bFixedPoint)
    dfb_rectangle_upscale (rect);
  return false;
}

bool
bdisp_aq_StretchBlit_shortcut_YCbCr422r (void         * const drv,
                                         void         * const dev,
                                         DFBRectangle * const srect,
                                         DFBRectangle * const drect)
{
  return bdisp_aq_Blit_shortcut_YCbCr422r (drv, dev, drect, drect->x, drect->y);
}

bool
bdisp_aq_Blit2_shortcut_YCbCr422r (void         * const drv,
                                   void         * const dev,
                                   DFBRectangle * const rect,
                                   int           dx,
                                   int           dy,
                                   int           sx2,
                                   int           sy2)
{
  return bdisp_aq_Blit_shortcut_YCbCr422r (drv, dev, rect, dx, dy);
}

bool
bdisp_aq_Blit_shortcut_rgb32 (void         * const drv,
                              void         * const dev,
                              DFBRectangle * const rect,
                              int           dx,
                              int           dy)
{
  return bdisp_aq_Blit_shortcut_real (drv, dev, rect, dx, dy, 0xff000000);
}

bool
bdisp_aq_StretchBlit_shortcut_rgb32 (void         * const drv,
                                     void         * const dev,
                                     DFBRectangle * const srect,
                                     DFBRectangle * const drect)
{
  return bdisp_aq_Blit_shortcut_rgb32 (drv, dev, drect, drect->x, drect->y);
}

bool
bdisp_aq_Blit2_shortcut_rgb32 (void         * const drv,
                               void         * const dev,
                               DFBRectangle * const rect,
                               int           dx,
                               int           dy,
                               int           sx2,
                               int           sy2)
{
  return bdisp_aq_Blit_shortcut_rgb32 (drv, dev, rect, dx, dy);
}


static void
_bdisp_aq_Blit_rotate_using_spans (STGFX2DriverData   * const stdrv,
                                   STGFX2DeviceData   * const stdev,
                                   const DFBRectangle * const src,
                                   int                 dx,
                                   int                 dy,
                                   int                 dy_step)
{
  int w_remaining = src->w & 0x0fff;
  int this_w = ABS (dy_step);
  int src_x = src->x;
  int src_y = src->y << 16;
  int src_h = src->h & 0x0fff;

  while (w_remaining)
    {
      stdev->ConfigTarget.BLT_TXY      = (dy << 16) | dx;
      stdev->ConfigSource1.BLT_S1XY    = stdev->ConfigTarget.BLT_TXY;
      stdev->ConfigTarget.BLT_TSZ_S1SZ = (this_w << 16) | src_h;

      stdev->ConfigSource2.BLT_S2XY = src_y | src_x;
      stdev->ConfigSource2.BLT_S2SZ = (src_h << 16) | this_w;

      bdisp_aq_copy_n_push_node (stdrv, stdev);
      bdisp_aq_clut_update_disable_if_unsafe (stdev);

      dy += dy_step;
      src_x += this_w;
      w_remaining -= this_w;

      this_w = MIN (ABS (dy_step), w_remaining);
    }
}

bool
bdisp_aq_Blit_rotate_90_270 (void         * const drv,
                             void         * const dev,
                             DFBRectangle * const rect,
                             int           dx,
                             int           dy)
{
  STGFX2DriverData * const stdrv = drv;
  STGFX2DeviceData * const stdev = dev;

  if (!stdev->bFixedPoint)
    D_DEBUG_AT (BDISP_BLT, "%s (%d,%d-%dx%d -> %d,%d) %u°\n", __FUNCTION__,
                DFB_RECTANGLE_VALS (rect), dx, dy, stdev->rotate);
  else
    {
      D_DEBUG_AT (BDISP_BLT, "%s (%d.%06d,%d.%06d-%d.%06dx%d.%06d -> %d.%06d,%d.%06d) %u°\n", __FUNCTION__,
                  DFB_RECTANGLE_VALSf (rect), DFB_INT_VALf (dx),
                  DFB_INT_VALf (dy), stdev->rotate);
      dfb_rectangle_downscale (rect);
      dx /= DFB_FIXED_POINT_ONE;
      dy /= DFB_FIXED_POINT_ONE;
    }
  DUMP_INFO ();

  if (unlikely (!rect->w || !rect->h))
    return true;
  if (!_check_size_constraints (rect->x, rect->y, rect->w, rect->h))
    return false;
  if (!_check_size_constraints (dx, dy, 1, 1))
    return false;

  if (rect->w % 16)
    goto error;

  bool requires_spans;

  /* read from top left to bottom right
     90°: writes from bottom left to top right
     270°: writes from top right to bottom left */
  int dy_step;

  if (stdev->ConfigSource2.BLT_S2BA == stdev->ConfigTarget.BLT_TBA)
    {
      DFBRegion dregion;
      dregion.x1 = dx;
      dregion.y1 = dy;
      dregion.x2 = dx + rect->h - 1;
      dregion.y2 = dy + rect->w - 1;
      if (dfb_region_rectangle_intersect (&dregion, rect))
        goto error;
    }

  requires_spans = (rect->w > stdev->features.rotate_buffer_length);

  if (stdev->rotate == 90)
    {
      dx = dx;
      dy += rect->w - 1;
      dy_step = -stdev->features.rotate_buffer_length;
    }
  else
    {
      dx += rect->h - 1;
      dy = dy;
      dy_step = stdev->features.rotate_buffer_length;
    }

  if (unlikely (!requires_spans))
    {
      stdev->ConfigTarget.BLT_TSZ_S1SZ = ((rect->w & 0x0fff) << 16) | (rect->h & 0x0fff);
      stdev->ConfigTarget.BLT_TXY   = (dy << 16) | dx;
      stdev->ConfigSource1.BLT_S1XY = stdev->ConfigTarget.BLT_TXY;
      stdev->ConfigSource2.BLT_S2XY = (rect->y << 16) | rect->x;
      stdev->ConfigSource2.BLT_S2SZ = ((rect->h & 0x0fff) << 16) | (rect->w & 0x0fff);

      bdisp_aq_copy_n_push_node (stdrv, stdev);
      bdisp_aq_clut_update_disable_if_unsafe (stdev);
    }
  else
    _bdisp_aq_Blit_rotate_using_spans (stdrv, stdev, rect, dx, dy, dy_step);

  return true;

error:
  if (stdev->bFixedPoint)
    dfb_rectangle_upscale (rect);
  return false;
}


bool
bdisp_aq_Blit_nop (void         * const drv,
                   void         * const dev,
                   DFBRectangle * const rect,
                   int           dx,
                   int           dy)
{
  const STGFX2DriverData __attribute__((unused)) * const stdrv = drv;
  const STGFX2DeviceData __attribute__((unused)) * const stdev = dev;

  if (!stdev->bFixedPoint)
    D_DEBUG_AT (BDISP_BLT, "%s (%d,%d-%dx%d -> %d,%d)\n", __FUNCTION__,
                DFB_RECTANGLE_VALS (rect), dx, dy);
  else
    D_DEBUG_AT (BDISP_BLT, "%s (%d.%06d,%d.%06d-%d.%06dx%d.%06d -> %d.%06d,%d.%06d)\n",
                __FUNCTION__, DFB_RECTANGLE_VALSf (rect),
                DFB_INT_VALf (dx), DFB_INT_VALf (dy));
  DUMP_INFO ();

  return true;
}

bool
bdisp_aq_StretchBlit_nop (void         * const drv,
                          void         * const dev,
                          DFBRectangle * const srect,
                          DFBRectangle * const drect)
{
  const STGFX2DriverData __attribute__((unused)) * const stdrv = drv;
  const STGFX2DeviceData __attribute__((unused)) * const stdev = dev;

  if (!stdev->bFixedPoint)
    D_DEBUG_AT (BDISP_STRETCH, "%s (%d,%d-%dx%d -> %d,%d-%dx%d)\n", __FUNCTION__,
                DFB_RECTANGLE_VALS (srect), DFB_RECTANGLE_VALS (drect));
  else
    D_DEBUG_AT (BDISP_STRETCH, "%s (%d.%06d,%d.%06d-%d.%06dx%d.%06d -> %d.%06d,%d.%06d-%d.%06dx%d.%06d)\n", __FUNCTION__,
                DFB_RECTANGLE_VALSf (srect), DFB_RECTANGLE_VALSf (drect));
  DUMP_INFO ();

  return true;
}

bool
bdisp_aq_Blit2_nop (void         * const drv,
                    void         * const dev,
                    DFBRectangle * const rect,
                    int           dx,
                    int           dy,
                    int           sx2,
                    int           sy2)
{
  const STGFX2DriverData __attribute__((unused)) * const stdrv = drv;
  const STGFX2DeviceData __attribute__((unused)) * const stdev = dev;

  if (!stdev->bFixedPoint)
    D_DEBUG_AT (BDISP_STRETCH, "%s (%d,%d-%dx%d + %d,%d -> %d,%d)\n", __FUNCTION__,
                DFB_RECTANGLE_VALS (rect), sx2, sy2, dx, dy);
  else
    D_DEBUG_AT (BDISP_STRETCH, "%s (%d.%06d,%d.%06d-%d.%06dx%d.%06d + %d.%06d,%d.%06d -> %d.%06d,%d.%06d)\n", __FUNCTION__,
                DFB_RECTANGLE_VALSf (rect), DFB_INT_VALf (sx2),
                DFB_INT_VALf (sy2), DFB_INT_VALf (dx), DFB_INT_VALf (dy));
  DUMP_INFO ();

  return true;
}


void
_bdisp_aq_RGB32_init (STGFX2DriverData * const stdrv,
                      STGFX2DeviceData * const stdev,
                      u32               blt_tba,
                      u16               pitch,
                      DFBRectangle     * const rect)
{
  D_DEBUG_AT (BDISP_BLT, "%s\n", __FUNCTION__);

  /* save state */
  u32 tba = stdev->ConfigTarget.BLT_TBA;
  u32 tty = stdev->ConfigTarget.BLT_TTY;
  u32 cty = stdev->drawstate.color_ty;
  u32 col = stdev->drawstate.color;

  stdev->ConfigTarget.BLT_TBA = blt_tba;
  stdev->ConfigTarget.BLT_TTY = (BLIT_COLOR_FORM_ARGB8888
                                 | BLIT_TY_FULL_ALPHA_RANGE) | pitch;
  stdev->drawstate.color_ty = stdev->ConfigTarget.BLT_TTY;
  stdev->drawstate.color    = 0xff000000;

  bdisp_aq_FillRectangle_simple (stdrv, stdev, rect);

  /* restore state */
  stdev->drawstate.color = col;
  stdev->drawstate.color_ty = cty;
  stdev->ConfigTarget.BLT_TTY = tty;
  stdev->ConfigTarget.BLT_TBA = tba;

  bdisp_aq_EmitCommands (stdrv, stdev);
}

/* fixup for rgb32, force the alpha to all 0xff again. */
void
_bdisp_aq_RGB32_fixup (STGFX2DriverData * const stdrv,
                       STGFX2DeviceData * const stdev,
                       u32               blt_tba,
                       u16               pitch,
                       DFBRectangle     * const rect)
{
  D_DEBUG_AT (BDISP_BLT, "%s\n", __FUNCTION__);

  STMFBBDispSharedArea * const shared = stdrv->bdisp_shared;

  struct _BltNode_RGB32fixup * const node =
    _bdisp_aq_get_new_node (stdrv, stdev, shared);

  node->ConfigGeneral.BLT_CIC = CIC_BLT_NODE_RGB32fixup;
  node->ConfigGeneral.BLT_INS = BLIT_INS_SRC1_MODE_MEMORY | BLIT_INS_ENABLE_PLANEMASK;
  node->ConfigGeneral.BLT_ACK = BLIT_ACK_ROP | BLIT_ACK_ROP_SET;

  node->ConfigTarget.BLT_TBA
    = node->ConfigSource1.BLT_S1BA
    = blt_tba;
  node->ConfigTarget.BLT_TTY
    = node->ConfigSource1.BLT_S1TY
    = BLIT_COLOR_FORM_ARGB8888 | BLIT_TY_FULL_ALPHA_RANGE | pitch;
  node->ConfigTarget.BLT_TXY
    = node->ConfigSource1.BLT_S1XY
    = (rect->y << 16) | rect->x;
  node->ConfigTarget.BLT_TSZ_S1SZ = (rect->h << 16) | rect->w;

  node->ConfigFilters.BLT_PMK = 0xff000000;

  bdisp_aq_update_num_ops (stdev, shared, node->ConfigGeneral.BLT_INS);

  bdisp_aq_update_config_static (shared, &node->ConfigStatic);

  _bdisp_aq_finish_node (stdrv, stdev, shared, node);

  bdisp_aq_EmitCommands (stdrv, stdev);
  bdisp_aq_EngineSync (stdrv, stdev);
}
