/************************************************************************
Copyright (C) 2003-2014 STMicroelectronics. All Rights Reserved.

This file is part of the Streaming Engine.

Streaming Engine is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

Streaming Engine is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with Streaming Engine; see the file COPYING.  If not, write to the Free
Software Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307,
USA.

The Streaming Engine Library may alternatively be licensed under a
proprietary license from ST.
************************************************************************/

#include <linux/version.h>
#include <linux/compiler.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/kthread.h>
#include <linux/freezer.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/fb.h>
#include <linux/interrupt.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/wait.h>
#include <linux/fs.h>
#include <linux/debugfs.h>
#include <linux/string.h>
#include <linux/syscalls.h>
#include <linux/vmalloc.h>

#include <asm/uaccess.h>
#include <asm/irq.h>

#include "KTM_ControlData_dbgfs.h"
#include "KTM_ControlData.h"
#include "KTM_ControlData_event.h"

#include "st_relayfs_se.h"

#ifndef __init
#define __init
#endif
#ifndef __exit
#define __exit
#endif

static int  __init              ControlDataPacketTestLoadModule(void);
static void __exit              ControlDataPacketTestUnloadModule(void);

module_init(ControlDataPacketTestLoadModule);
module_exit(ControlDataPacketTestUnloadModule);

MODULE_DESCRIPTION("Control data packet STKPI Test Module");
MODULE_AUTHOR("STMicroelectronics");
MODULE_LICENSE("GPL");

/* Global structure used throughout the test */
struct ControlDataTestCtx Context;

static int __init ControlDataPacketTestLoadModule(void)
{
    int Result;
    Context.Stream = vzalloc(TEST_CTRL_STREAM_SIZE);

    if (Context.Stream == NULL)
    {
        pr_err("Couldn't allocate memory\n");
        return -ENOMEM;
    }

    pr_info("Allocated %u bytes at 0x%x\n", TEST_CTRL_STREAM_SIZE, (unsigned int) Context.Stream);
    Context.StreamSize = 0;
    Context.InjectionSize = 0;
    Context.FSIndex = 0;
    Context.PlaybackSpeed = 1000;
    Context.StreamEncoding = STM_SE_STREAM_ENCODING_VIDEO_H264;
    stm_se_set_control(STM_SE_CTRL_ENABLE_CONTROL_DATA_DETECTION, STM_SE_CTRL_VALUE_APPLY);
    Result = stm_se_playback_new("Playback1", &(Context.PlaybackHandle));

    if (Result != 0)
    {
        pr_err("Couldn't create playback\n");
        return Result;
    }

    Result = stm_se_play_stream_new("VideoH264",
                                    Context.PlaybackHandle,
                                    STM_SE_STREAM_ENCODING_VIDEO_H264,
                                    &(Context.StreamH264Handle));

    if (Result != 0)
    {
        pr_err("Couldn't create H264 stream\n");
        return Result;
    }

    Result = stm_se_play_stream_new("VideoMPEG2",
                                    Context.PlaybackHandle,
                                    STM_SE_STREAM_ENCODING_VIDEO_MPEG2,
                                    &(Context.StreamMpeg2Handle));

    if (Result != 0)
    {
        pr_err("Couldn't create Mpeg2 stream\n");
        return Result;
    }

    SubscribeEvent();
    Context.FSIndex = st_relayfs_getindex_forsource_se(ST_RELAY_SOURCE_CONTROL_TEST);
    ControlDataTestDbgfsOpen();
    pr_info("%s: Control Data Test Module Loaded\n", __func__);
    return 0;
}

static void __exit ControlDataPacketTestUnloadModule(void)
{
    /* stm_se_term gets called when Player2 Module is unloaded */
    pr_info("%s: Unloading..\n", __func__);
    UnsubscribeEvent();
    vfree(Context.Stream);
    stm_se_play_stream_delete(Context.StreamMpeg2Handle);
    stm_se_play_stream_delete(Context.StreamH264Handle);
    stm_se_playback_delete(Context.PlaybackHandle);
    ControlDataTestDbgfsClose();
    st_relayfs_freeindex_forsource_se(ST_RELAY_SOURCE_CONTROL_TEST, Context.FSIndex);
    stm_se_set_control(STM_SE_CTRL_ENABLE_CONTROL_DATA_DETECTION, STM_SE_CTRL_VALUE_DISAPPLY);
    pr_info("%s: Control Data Test Module unloaded\n", __func__);
}
