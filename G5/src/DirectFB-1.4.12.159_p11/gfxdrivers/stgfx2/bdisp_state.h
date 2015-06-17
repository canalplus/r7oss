/*
   ST Microelectronics BDispII driver - state handling

   (c) Copyright 2007/2008  STMicroelectronics Ltd.

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

#ifndef __BDISP_STATE_H__
#define __BDISP_STATE_H__


#include <directfb.h>
#include <core/coretypes.h>


bool _bdisp_state_set_buffer_type (const STGFX2DeviceData * const stdev,
                                   u32                    * const reg,
                                   DFBSurfacePixelFormat   format,
                                   u16                     pitch);

void bdisp_aq_CheckState (void                * const drv,
                          void                * const dev,
                          CardState           * const state,
                          DFBAccelerationMask  accel);

void bdisp_aq_SetState (void                * const drv,
                        void                * const dev,
                        GraphicsDeviceFuncs * const funcs,
                        CardState           * const state,
                        DFBAccelerationMask   accel);

#endif /* __BDISP_STATE_H__ */
