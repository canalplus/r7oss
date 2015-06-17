/*
   ST Microelectronics BDispII driver - primary screen implementation

   (c) Copyright 2009       STMicroelectronics Ltd.

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

#ifndef __STGFX_SCREEN_H__
#define __STGFX_SCREEN_H__

#include <linux/stmfb.h>
#include <core/screens.h>

#ifndef STGFX_DRIVER
#  error Please define STGFX_DRIVER
#endif

struct _STGFX2ScreenData {
  void                                *orig_data;
  ScreenFuncs                          orig_funcs;
  struct stmfbio_output_configuration  startup_config;
};

#define _stgfxPrimaryScreenFuncs(driver) \
ScreenFuncs stgfxPrimaryScreenFuncs_stgfx ##driver

#define stgfxPrimaryScreenFuncs(driver) _stgfxPrimaryScreenFuncs(driver)

extern stgfxPrimaryScreenFuncs (STGFX_DRIVER);

#undef _stgfxPrimaryScreenFuncs
#undef stgfxPrimaryScreenFuncs

#endif /* __STGFX_SCREEN_H__ */
