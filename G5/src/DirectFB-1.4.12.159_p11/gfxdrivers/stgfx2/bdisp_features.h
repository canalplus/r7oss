/*
   ST Microelectronics BDispII driver - distinguishing hardware

   (c) Copyright 2008       STMicroelectronics Ltd.

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

#ifndef __BDISP_FEATURES_H__
#define __BDISP_FEATURES_H__

#include <directfb.h>
#include <linux/types.h>
#include <linux/stmfb.h>

#include "bdisp_registers.h"

struct _bdisp_pixelformat_table {
  u32  bdisp_type; /* BLIT_COLOR_FORM_RGB565 etc. */
  bool supported_as_src : 1;
  bool supported_as_dst : 1;
};

struct _bdisp_hw_features {
  char                     name[28];
  u32                      extra_src_formats; /* private */

  u16                      line_buffer_length;
  u16                      rotate_buffer_length;

  enum STM_BLITTER_FLAGS   blitflags;

  DFBSurfaceDrawingFlags   dfb_drawflags;
  DFBSurfaceBlittingFlags  dfb_blitflags;
  DFBSurfaceRenderOptions  dfb_renderopts;
  struct _bdisp_pixelformat_table dfb_dspf_to_bdisp[DFB_NUM_PIXELFORMATS];
};

bool
bdisp_hw_features_set_bdisp (enum stm_blitter_device    device,
                             struct _bdisp_hw_features *ret_fetures);


#endif /* __BDISP_FEATURES_H__ */
