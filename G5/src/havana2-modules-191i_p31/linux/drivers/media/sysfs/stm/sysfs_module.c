/************************************************************************
Copyright (C) 2005 STMicroelectronics. All Rights Reserved.

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

Source file name : sysfs_module.c
Author :           Julian

Implementation of the sysfs access to the ST streaming engine

Date        Modification                                    Name
----        ------------                                    --------
28-Apr-08   Created                                         Julian

************************************************************************/

#include <linux/sched.h>
#include <linux/syscalls.h>
#include <linux/ioport.h>
#include <linux/bpa2.h>
#include <linux/module.h>
#include <linux/file.h>
#include <linux/kthread.h>
#include <linux/autoconf.h>
#include <asm/uaccess.h>

#include "sysfs_module.h"
#include "player_interface.h"

static int  __init      SysfsLoadModule (void);
static void __exit      SysfsUnloadModule (void);

module_init             (SysfsLoadModule);
module_exit             (SysfsUnloadModule);

MODULE_DESCRIPTION      ("Sysfs driver for accessing STM streaming architecture.");
MODULE_AUTHOR           ("Julian Wilson");
MODULE_LICENSE          ("GPL");

#define MODULE_NAME     "Player sysfs"


struct SysfsContext_s*   SysfsContext;

static int __init SysfsLoadModule (void)
{
    SysfsContext        = kmalloc (sizeof (struct SysfsContext_s),  GFP_KERNEL);
    if (SysfsContext == NULL)
    {
        SYSFS_ERROR("Unable to allocate device memory\n");
        return -ENOMEM;
    }

    PlayerInterfaceInit ();

    SYSFS_DEBUG("sysfs interface to stream device loaded\n");

    return 0;
}

static void __exit SysfsUnloadModule (void)
{
    PlayerInterfaceDelete ();

    if (SysfsContext != NULL)
        kfree (SysfsContext);

    SysfsContext        = NULL;

    SYSFS_DEBUG("STM sysfs interface to stream device unloaded\n");

    return;
}

