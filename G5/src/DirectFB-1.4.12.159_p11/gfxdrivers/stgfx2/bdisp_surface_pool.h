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

#ifndef __BDISP_SURFACE_POOL_H__
#define __BDISP_SURFACE_POOL_H__

#include <core/coretypes.h>

#ifndef STGFX_DRIVER
#  error Please define STGFX_DRIVER
#endif

#define _bdisp_surface_pool_funcs(v) \
extern void \
stgfx##v##_surface_pool_init (CoreDFB          * const core,  \
                              CoreSurfacePool ** const pools, \
                              int               max_pools);   \
  \
extern void \
stgfx##v##_surface_pool_destroy (CoreDFB          * const core,  \
                                 CoreSurfacePool ** const pools, \
                                 int               n_pools);     \
  \
extern void \
stgfx##v##_surface_pool_join (CoreDFB                 * const core,  \
                              CoreSurfacePool * const * const pools, \
                              int                      n_pools);     \
  \
extern void \
stgfx##v##_surface_pool_leave (CoreDFB                 * const core,  \
                               CoreSurfacePool * const * const pools, \
                               int                      n_pools);

#define bdisp_surface_pool_funcs(driver) _bdisp_surface_pool_funcs(driver)

bdisp_surface_pool_funcs (STGFX_DRIVER);

#undef  bdisp_surface_pool_funcs
#undef _bdisp_surface_pool_funcs


#endif /* __BDISP_SURFACE_POOL_H__ */
