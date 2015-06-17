/***********************************************************************

This file is part of stgfx2 driver

COPYRIGHT (C) 2003, 2004, 2005 STMicroelectronics - All Rights Reserved

License type: LGPLv2.1

stgfx2 is free software; you can redistribute it and/or modify it
under the terms of the GNU Lesser General Public License version 2.1 as published by
the Free Software Foundation.

stgfx2 is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA

The stgfx2 Library may alternatively be licensed under a proprietary
license from ST.

This file was created by STMicroelectronics on 2008-09-12
and modified by STMicroelectronics on 2014-05-28

************************************************************************/

#ifndef __STM_GFXDRIVER_H__
#define __STM_GFXDRIVER_H__


#include <dfb_types.h>
#include <core/state.h>
#include <core/screens.h>
#include <core/layers.h>
#include <core/surface_pool.h>

#include <linux/stm/bdisp2_shared.h>
#include <linux/stm/bdisp2_shared_ioctl.h>
#include <linux/stm/bdisp2_nodegroups.h>
#include "bdisp2/bdispII_aq_state.h"

#include "stm_types.h"
#include "bdisp_features.h"

#include "stgfx_screen.h"
#include "stgfx_primary.h"


/* Structure for storing information about device state */
struct _STGFX2DeviceData
{
  struct stm_bdisp2_device_data stdev;

  /* physical address and size of stm_bdisp2_shared_area */
  struct stm_bdisp2_shared_cookie bdisp_shared_cookie;

  struct _bdisp_dfb_features dfb_features;

  GraphicsDeviceInfo *device_info;
};


struct _STGFX2DriverData
{
  int fd_gfx;
  int stmfb_acceleration; /* old style stmfb or new stm-blitter driver */

  int accel_type;

  struct stm_bdisp2_driver_data stdrv;

  CoreGraphicsDevice *device;
};


#endif /* __STM_GFXDRIVER_H__ */
