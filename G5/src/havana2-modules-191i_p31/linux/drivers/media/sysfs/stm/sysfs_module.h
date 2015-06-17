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

Source file name : sysfs_module.h - streamer device access interface definitions
Author :           Julian

Date        Modification                                    Name
----        ------------                                    --------
24-Mar-03   Created                                         Julian

************************************************************************/

#ifndef H_SYSFS_MODULE
#define H_SYSFS_MODULE

#include "player_interface.h"

#ifndef false
#define false   0
#define true    1
#endif

/*      Debug printing macros   */
#ifndef ENABLE_SYSFS_DEBUG
#define ENABLE_SYSFS_DEBUG              0
#endif

#define SYSFS_DEBUG(fmt, args...)       ((void) (ENABLE_SYSFS_DEBUG && \
                                            (printk("%s: " fmt, __FUNCTION__, ##args), 0)))

/* Output trace information off the critical path */
#define SYSFS_TRACE(fmt, args...)       (printk("%s: " fmt, __FUNCTION__, ##args))
/* Output errors, should never be output in 'normal' operation */
#define SYSFS_ERROR(fmt, args...)       (printk("ERROR:%s: " fmt, __FUNCTION__, ##args))


struct SysfsContext_s
{
    unsigned int        Something;
};

#endif
