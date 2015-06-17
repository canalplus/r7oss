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

Source file name : player_interface.h - player access points
Author :           Julian

Date        Modification                                    Name
----        ------------                                    --------
31-Jan-07   Created                                         Julian

************************************************************************/

#ifndef H_PLAYER_INTERFACE
#define H_PLAYER_INTERFACE

#include <linux/kthread.h>

#include "player_interface_ops.h"
#include "sysfs_module.h"

/*      Debug printing macros   */

#ifndef ENABLE_INTERFACE_DEBUG
#define ENABLE_INTERFACE_DEBUG            1
#endif

#define INTERFACE_DEBUG(fmt, args...)  ((void) (ENABLE_INTERFACE_DEBUG && \
                                            (printk("Interface:%s: " fmt, __FUNCTION__, ##args), 0)))

/* Output trace information off the critical path */
#define INTERFACE_TRACE(fmt, args...)  (printk("Interface:%s: " fmt, __FUNCTION__, ##args))
/* Output errors, should never be output in 'normal' operation */
#define INTERFACE_ERROR(fmt, args...)  (printk("ERROR:Interface:%s: " fmt, __FUNCTION__, ##args))


/* Entry point list */

int PlayerInterfaceInit                (void);
int PlayerInterfaceDelete              (void);

int ComponentGetAttribute              (player_component_handle_t       Component,
                                        const char*                     Attribute,
                                        union attribute_descriptor_u*   Value);
int ComponentSetAttribute              (player_component_handle_t       Component,
                                        const char*                     Attribute,
                                        union attribute_descriptor_u*   Value);

player_event_signal_callback PlayerRegisterEventSignalCallback         (player_event_signal_callback  Callback);

#endif
