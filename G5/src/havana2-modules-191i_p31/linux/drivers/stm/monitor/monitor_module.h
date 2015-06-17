/************************************************************************
Copyright (C) 2003 STMicroelectronics. All Rights Reserved.

This file is part of the Player2 Library.

Player2 is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License version 2 as published by the
Free Software Foundation.

Player2 is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with player2; see the file COPYING.  If not, write to the Free Software
Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

The Player2 Library may alternatively be licensed under a proprietary
license from ST.

Source file name : monitor_module.h - monitor device definitions
Author :           Julian

Date        Modification                                    Name
----        ------------                                    --------
25-Jul-08   Created                                         Julian

************************************************************************/

#ifndef H_MONITOR_MODULE
#define H_MONITOR_MODULE

#include "monitor_device.h"

#ifndef false
#define false   0
#define true    1
#endif

/*      Debug printing macros   */
#ifndef ENABLE_MONITOR_DEBUG
#define ENABLE_MONITOR_DEBUG            0
#endif

#define MONITOR_DEBUG(fmt, args...)     ((void) (ENABLE_MONITOR_DEBUG && (printk("%s: " fmt, __FUNCTION__, ##args), 0)))

/* Output trace information off the critical path */
#define MONITOR_TRACE(fmt, args...)     (printk("%s: " fmt, __FUNCTION__, ##args))
/* Output errors, should never be output in 'normal' operation */
#define MONITOR_ERROR(fmt, args...)     (printk("ERROR:%s: " fmt, __FUNCTION__, ##args))


#define MONITOR_MAX_DEVICES             1

struct ModuleContext_s
{
    struct mutex                Lock;
    struct class*               DeviceClass;
    struct DeviceContext_s      DeviceContext[MONITOR_MAX_DEVICES];

};

struct DeviceContext_s* GetDeviceContext       (unsigned int    DeviceId);


#endif
