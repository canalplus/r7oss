/*
   ST Microelectronics BDispII driver - primary layer implementation

   (c) Copyright 2007-2009  STMicroelectronics Ltd.

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

#ifndef __STGFX_PRIMARY_H__
#define __STGFX_PRIMARY_H__

#include <linux/stmfb.h>
#include <core/layers.h>

#ifndef STGFX_DRIVER
#  error Please define STGFX_DRIVER
#endif

struct _STGFX2LayerData
{
  void                             *orig_data;
  DisplayLayerFuncs                 orig_funcs;
  struct stmfbio_var_screeninfo_ex  startup_var;
};

#define _stgfxPrimaryLayerFuncs(driver) \
DisplayLayerFuncs stgfxPrimaryLayerFuncs_stgfx ##driver

#define stgfxPrimaryLayerFuncs(driver) _stgfxPrimaryLayerFuncs(driver)

extern stgfxPrimaryLayerFuncs (STGFX_DRIVER);

#undef _stgfxPrimaryLayerFuncs
#undef stgfxPrimaryLayerFuncs

#endif /* __STGFX_PRIMARY_H__ */
