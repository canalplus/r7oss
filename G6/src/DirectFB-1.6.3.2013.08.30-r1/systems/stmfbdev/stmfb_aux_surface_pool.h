/*
   ST Microelectronics BDispII driver - surface pool memory (auxmem)

   (c) Copyright 2009       STMicroelectronics Ltd.

   Written by André Draszik <andre.draszik@st.com>

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

#ifndef __STMFB_AUX_SURFACE_POOL_H__
#define __STMFB_AUX_SURFACE_POOL_H__

#include <core/coretypes.h>

extern void
stmfb_aux_surface_pools_init (CoreDFB * const core);

extern void
stmfb_aux_surface_pools_destroy (CoreDFB * const core);

extern void
stmfb_aux_surface_pools_join (CoreDFB * const core);

extern void
stmfb_aux_surface_pools_leave (CoreDFB * const core);


#endif /* __STMFB_AUX_SURFACE_POOL_H__ */
