/***********************************************************************
 *
 * File: linux/kernel/drivers/stm/scaler/stiH416/stiH416_scaler.c
 * Copyright (c) 2011-2012 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#include <linux/version.h>
#include <linux/kernel.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 2, 2)
#include <linux/export.h>
#else
#include <linux/module.h>
#endif
#include <linux/platform_device.h>


int __init scaler_platform_device_register(void)
{
    return 0;
}


void __exit scaler_platform_device_unregister(void)
{
}

