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

Source file name : monitor_module.c
Author :           Julian

Implementation of the Monitor module

Date        Modification                                    Name
----        ------------                                    --------
28-Jul-08   Created                                         Julian

************************************************************************/

#include <linux/sched.h>
#include <linux/syscalls.h>
#include <linux/ioport.h>
#include <linux/fs.h>
#include <linux/bpa2.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/file.h>
#include <linux/kthread.h>
#include <linux/autoconf.h>
#include <linux/cdev.h>
#include <asm/uaccess.h>
#include <linux/platform_device.h>

#include "monitor_module.h"

static int  __init              StmMonitorInit (void);
static void __exit              StmMonitorExit (void);

module_init                     (StmMonitorInit);
module_exit                     (StmMonitorExit);

MODULE_DESCRIPTION              ("STM monitor.");
MODULE_AUTHOR                   ("Julian Wilson");
MODULE_LICENSE                  ("GPL");

#define MODULE_NAME             "STM Monitor"
#define DEVICE_NAME             "stm_monitor"

static dev_t                    FirstDevice;
static struct ModuleContext_s*  ModuleContext = NULL;

static int StmMonitorProbe(struct device *dev)
{
    int                     Result;
    int                     i;
    struct platform_device *MonitorDeviceData;
    unsigned int           *Timer;
    unsigned int            TimerPhysical;

    MonitorDeviceData = to_platform_device(dev);

    if (!MonitorDeviceData) {
            MONITOR_ERROR("%s: Device probe failed.  Check your kernel SoC config!!\n",
                   __FUNCTION__);

            return -ENODEV;
    }

    ModuleContext       = kzalloc (sizeof (struct ModuleContext_s),  GFP_KERNEL);
    if (ModuleContext == NULL)
    {
        MONITOR_ERROR("Unable to allocate device memory\n");
        return -ENOMEM;
    }

    TimerPhysical = platform_get_resource(MonitorDeviceData, IORESOURCE_MEM, 0)->start;
    Timer         = ioremap(TimerPhysical,0x4);

    mutex_init (&(ModuleContext->Lock));
    mutex_lock (&(ModuleContext->Lock));

    Result      = alloc_chrdev_region (&FirstDevice, 0, MONITOR_MAX_DEVICES, DEVICE_NAME);
    if (Result < 0)
    {
        printk (KERN_ERR "%s: unable to allocate device numbers\n",__FUNCTION__);
        return -ENODEV;
    }

    ModuleContext->DeviceClass                  = class_create (THIS_MODULE, DEVICE_NAME);
    if (IS_ERR(ModuleContext->DeviceClass))
    {
        printk (KERN_ERR "%s: unable to create device class\n",__FUNCTION__);
        ModuleContext->DeviceClass              = NULL;
        return -ENODEV;
    }

    for (i = 0; i < MONITOR_MAX_DEVICES; i++)
    {
        struct DeviceContext_s* DeviceContext   = &ModuleContext->DeviceContext[i];
        int                     DevNo           = MKDEV(MAJOR(FirstDevice), i);
        struct file_operations* FileOps;

        DeviceContext->TimerPhysical            = TimerPhysical;
        DeviceContext->Timer                    = Timer;

        FileOps                                 = MonitorInit (DeviceContext);

        DeviceContext->ModuleContext            = ModuleContext;
        cdev_init (&(DeviceContext->CDev), FileOps);
        DeviceContext->CDev.owner               = THIS_MODULE;
        kobject_set_name (&(DeviceContext->CDev.kobj), "%s%d", DEVICE_NAME, i);
        Result                                  = cdev_add (&(DeviceContext->CDev), DevNo, 1);
        if (Result != 0)
        {
            printk (KERN_ERR "%s: unable to add device\n",__FUNCTION__);
            return -ENODEV;
        }
        DeviceContext->ClassDevice              = class_device_create (ModuleContext->DeviceClass,
                                                                       NULL,
                                                                       DeviceContext->CDev.dev,
                                                                       NULL,
                                                                       kobject_name (&(DeviceContext->CDev.kobj)));
        if (IS_ERR(DeviceContext->ClassDevice))
        {
            printk (KERN_ERR "%s: unable to create class device\n",__FUNCTION__);
            DeviceContext->ClassDevice          = NULL;
            return -ENODEV;
        }

        class_set_devdata (DeviceContext->ClassDevice, DeviceContext);
    }

    mutex_unlock (&(ModuleContext->Lock));

    MONITOR_DEBUG("STM monitor device loaded\n");

    return 0;
}

static int StmMonitorRemove(struct device *dev)
{

    unregister_chrdev_region (FirstDevice, MONITOR_MAX_DEVICES);

    if (ModuleContext != NULL)
        kfree (ModuleContext);

    ModuleContext  = NULL;

    MONITOR_DEBUG("STM monitor device unloaded\n");

    return 0;
}

struct DeviceContext_s* GetDeviceContext   (unsigned int        DeviceId)
{
    if (!ModuleContext)
        return NULL;

    if (DeviceId < MONITOR_MAX_DEVICES)
        return &(ModuleContext->DeviceContext[DeviceId]);
    return NULL;
}

static struct device_driver MonitorDriver = {
        .name = "stm-monitor",
        .bus = &platform_bus_type,
        .probe = StmMonitorProbe,
        .remove = StmMonitorRemove,
};

static __init int StmMonitorInit(void)
{
        return driver_register(&MonitorDriver);
}

static void StmMonitorExit(void)
{
        driver_unregister(&MonitorDriver);
}
