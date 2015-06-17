/*
   ST Microelectronics BDispII driver - the DirectFB interface

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

#ifndef __STM_GFXDRIVER_H__
#define __STM_GFXDRIVER_H__


#include <dfb_types.h>
#include <core/state.h>
#include <core/screens.h>
#include <core/layers.h>
#include <core/surface_pool.h>

#include <linux/stmfb.h>

#include "stm_types.h"
#include "bdisp_accel_types.h"
#include "bdisp_registers.h"
#include "bdisp_features.h"

#include "stgfx_screen.h"
#include "stgfx_primary.h"


/* Structures for storing information about graphics state */
typedef enum
{
  SG2C_NORMAL,
  SG2C_COLORALPHA               /* αout = colorα, RGBout = RGBin */,
  SG2C_INVCOLORALPHA            /* αout = 1-colorα, RGBout = RGBin */,
  SG2C_ALPHA_MUL_COLORALPHA     /* αout = α * colorα, RGBout = RGBin */,
  SG2C_INV_ALPHA_MUL_COLORALPHA /* αout = 1-(α * colorα), RGBout = RGBin */,
  SG2C_DYNAMIC_COUNT            /* number of dynamic palettes */,

  SG2C_ONEALPHA_RGB      /* αout = 1, RGBout = RGBin */ = SG2C_DYNAMIC_COUNT,
  SG2C_INVALPHA_ZERORGB  /* αout = 1 - Ain, RGBout = 0 */,
  SG2C_ALPHA_ZERORGB     /* αout = Ain, RGBout = 0 */,
  SG2C_ZEROALPHA_RGB     /* αout = 0, RGBout = RGBin */,
  SG2C_ZEROALPHA_ZERORGB /* αout = 0, RGBout = 0 */,
  SG2C_ONEALPHA_ZERORGB  /* αout = 1, RGBout = 0 */,

  SG2C_COUNT
} Stgfx2Clut;


/* Structure for storing information about device state */
struct _STGFX2DeviceData
{
  int aq_index;

  /* physical address and size of STMFBBDispSharedArea */
  struct stmfbio_shared bdisp_shared_info;
  unsigned long usable_nodes_size; /* shared->nodes_size may not be a multiple
                                      of our sizeof (<biggest_node>) */
  unsigned long node_irq_delay; /* every now and then, we want to see a node
                                   completed interrupt, so a) we don't have to
                                   wait for a complete list to finish, and b)
                                   we don't want them to happen too often. */

  /* validation flags */
  int v_flags;

  struct _BltNodeGroup00  ConfigGeneral;
  struct _BltNodeGroup01  ConfigTarget;
  struct _BltNodeGroup02  ConfigColor;
  struct _BltNodeGroup03  ConfigSource1;
  struct _BltNodeGroup04  ConfigSource2;
  struct _BltNodeGroup05  ConfigSource3;
#ifdef STGFX2_SUPPORT_HW_CLIPPING
  struct _BltNodeGroup06  ConfigClip;
#endif
  struct _BltNodeGroup07  ConfigClut;
  struct _BltNodeGroup08  ConfigFilters;
  struct _BltNodeGroup09  ConfigFiltersChr;
  struct _BltNodeGroup10  ConfigFiltersLuma;
  struct _BltNodeGroup11  ConfigFlicker;
  struct _BltNodeGroup12  ConfigColorkey;
  struct _BltNodeGroup14  ConfigStatic;
  struct _BltNodeGroup15  ConfigIVMX;
  struct _BltNodeGroup16  ConfigOVMX;
  struct _BltNodeGroup18  ConfigVC1R;

  /* drawing state */
  struct {
    struct _BltNodeGroup00 ConfigGeneral;
    unsigned long          color;
    unsigned long          color_ty;
    struct _BltNodeGroup07 ConfigClut;
  } drawstate;

  struct {
    u32 src_ckey;

    bool canUseHWInputMatrix;
    bool isOptimisedModulation;
    bool src_premultcolor;
    u32  blt_ins_src1_mode;

    int n_passes;
    struct {
      struct _BltNodeGroup00 ConfigGeneral;
      Stgfx2Clut             palette_type;
    } extra_passes[2];

    enum STM_BLITTER_FLAGS flags; /* FIXME: remove soon! */
  } blitstate;

  /* shared between both draw and blit states */
  struct {
    u32 dst_ckey;
    u32 extra_blt_ins; /* for RGB32 to always enable plane mask */
    u32 extra_blt_cic; /* for RGB32 to always enable plane mask */
  } all_states;


  unsigned long clut_phys[SG2C_COUNT];
  int  clut_offset[SG2C_DYNAMIC_COUNT];
#ifdef STGFX2_CLUT_UNSAFE_MULTISESSION
  bool clut_disabled;
#endif


  unsigned long filter_8x8_phys;
  unsigned long filter_5x8_phys;


  /* state etc */
  u32 srcFactorH; /* factors for source1 and source2 coordinate increment,
                     will normally be == 1. For certain YCbCr pixel formats it
                     will be == 2, though. */
  u32 srcFactorV;

  DFBSurfacePorterDuffRule porter_duff_rule;

  CorePalette *palette;
  Stgfx2Clut   palette_type;

  bool bIndexTranslation    : 1;
  bool bFixedPoint          : 1;
  int  rotate; /* degree counter clockwise. Only 90 180 270 */

  int h_trgt_sign, h_src2_sign, v_trgt_sign, v_src2_sign;
  int source_w; /* the total width of the surface in 16.16 */
  int source_h; /* the total height of the surface in 16.16 */
  u32 hsrcinc; /* in 16.16 */
  u32 vsrcinc; /* in 16.16 */

  struct _bdisp_hw_features features;

  /* stmfbdev system */
  CoreSurfacePool *pool;

  CoreSurfacePool *aux_pools[STMFBGP_GFX_LAST - STMFBGP_GFX_FIRST + 1];
  /* for exclusive use by bdisp_surface_pool.c */
  unsigned int aux_pool_index;
};


struct _STGFX2DriverData
{
  CoreDFB            *core;

  CoreGraphicsDevice *device;
  STGFX2DeviceData   *device_data;
  int                 fd;

  int accel_type;

  /* blit nodes */
  STMFBBDispSharedArea *bdisp_shared; /* shared area */
  volatile void        *bdisp_nodes;  /* node list   */

  /* BDisp base */
  volatile void *mmio_base;

  /* clut */
  unsigned long *clut_virt[SG2C_DYNAMIC_COUNT];


  struct _STGFX2ScreenData screenData;
  struct _STGFX2LayerData  layerData;
};




#endif /* __STM_GFXDRIVER_H__ */
