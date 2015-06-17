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

/************************************************************************
 * Description    : 1. create the test files in debugfs to transmit the stream,
 *                   injection size and commands
 *                  2. Store the stream put from user space in
 *                  /debugfs/KTM_ControlData/ControlDataStream
 *                  3. Get the injection size from what's written in
 *                  /debugfs/KTM_ControlData/ControlDataInjectionSize
 *                  4. Execute the command written in
 *                  /debugfs/KTM_ControlData/ControlDataCmd
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

#include <asm/uaccess.h>
#include <asm/irq.h>

#include "KTM_ControlData_dbgfs.h"
#include "KTM_ControlData.h"
#include "KTM_ControlData_event.h"
#include "stm_control_data.h"

int32_t CmdFileValue;
int32_t StreamFileValue;

static char ControlDataTestStream[TEST_CTRL_STREAM_SIZE] = {0,};
static char ControlDataTestCmd[TEST_CTRL_CMD_SIZE] = {0,};
static struct dentry *ControlDataTestPlaybackSpeedEntry = NULL;
static struct dentry *ControlDataTestInjectionSizeEntry = NULL;
static struct dentry *ControlDataTestStreamEntry = NULL;
static struct dentry *ControlDataTestCmdEntry = NULL;
static struct dentry *ControlDataTestDir = NULL;
extern struct ControlDataTestCtx Context;

#ifndef CONFIG_DEBUG_FS
#warn "file unexpectedly compiled with no debugfs support"
#endif

static ssize_t ReadBufferForStream(struct file *file, char __user *userbuf, size_t count, loff_t *ppos)
{
    return simple_read_from_buffer(userbuf, count, ppos, ControlDataTestStream, TEST_CTRL_STREAM_SIZE);
}

static ssize_t CopyStream(struct file *file, const char __user *buf, size_t count, loff_t *ppos)
{
    if (Context.StreamSize + count > TEST_CTRL_STREAM_SIZE)
    {
        pr_err("StreamSize >= %d which is above maximum length of TEST_CTRL_STREAM_SIZE!\n", Context.StreamSize + count);
        return -EINVAL;
    }

    if (copy_from_user(ControlDataTestStream, buf, count))
    {
        return -EFAULT;
    }
    ControlDataTestStream[TEST_CTRL_STREAM_SIZE - 1] = '\0';

    memcpy(Context.Stream, ControlDataTestStream, count);
    Context.StreamSize += count;
    pr_info("count %d\n", count);
    /* Increment pointer to continue copy on next call */
    Context.Stream += count;
    return count;
}
static const struct file_operations file_ops =
{
owner:
    THIS_MODULE,
read:
    ReadBufferForStream,
write:
    CopyStream
};

static ssize_t ReadBufferForCmd(struct file *file, char __user *userbuf, size_t count, loff_t *ppos)
{
    return simple_read_from_buffer(userbuf, count, ppos, ControlDataTestCmd, TEST_CTRL_CMD_SIZE);
}

static ssize_t GetCmd(struct file *file, const char __user *buf, size_t count, loff_t *ppos)
{
    if (count > TEST_CTRL_CMD_SIZE)
    {
        pr_err("CmdLength = %d which is above maximum length of TEST_CTRL_STREAM_SIZE!\n", count);
        return -EINVAL;
    }

    if (copy_from_user(ControlDataTestCmd, buf, count))
    {
        return -EFAULT;
    }
    ControlDataTestCmd[TEST_CTRL_CMD_SIZE - 1] = '\0'; // NULL terminated string

    ControlDataTestParseCmd(ControlDataTestCmd);
    return count;
}

static const struct file_operations cmd_fops =
{
owner:
    THIS_MODULE,
read:
    ReadBufferForCmd,
write:
    GetCmd
};


void ControlDataTestParseCmd(char *CmdStr)
{
    //int Result;
    /* Available commands are:
     * PLAY
     * CHANGE_STREAM
     */
    if (strstr(CmdStr, "PLAY") != NULL)
    {
        pr_info("Receive START command\n");

        if ((strstr(CmdStr, "H264") != NULL) || (strstr(CmdStr, "h264") != NULL))
        {
            pr_info("Will play an H264 stream\n");

            if (Context.StreamEncoding != STM_SE_STREAM_ENCODING_VIDEO_H264)
            {
                Context.StreamEncoding = STM_SE_STREAM_ENCODING_VIDEO_H264;
            }

            ControlDataTestInject(Context.StreamH264Handle);
        }
        else if ((strstr(CmdStr, "MPEG2") != NULL) || (strstr(CmdStr, "mpeg2") != NULL))
        {
            pr_info("Will play an MPEG2 stream\n");

            if (Context.StreamEncoding != STM_SE_STREAM_ENCODING_VIDEO_MPEG2)
            {
                Context.StreamEncoding = STM_SE_STREAM_ENCODING_VIDEO_MPEG2;
            }

            ControlDataTestInject(Context.StreamMpeg2Handle);
        }
        else
        {
            pr_err("Codec not supported - Only H264 and MPEG2 codecs are supported\n");
            return;
        }

        /* Start injection */
        //ControlDataTestInject();
    }
    else if (strstr(CmdStr, "CHANGE_STREAM") != NULL)
    {
        pr_info("Receive CHANGE_STREAM command\n");
        ControlDataTestChangeStream();
    }
}

void ControlDataTestInject(stm_se_play_stream_h StreamHandle)
{
    uint32_t InjectedData = 0;
    uint32_t InjectionSize;
    uint32_t RetCode;
    uint8_t InjectionNumber = 0;
    uint8_t     user_buffer[STM_CONTROL_DATA_USER_DATA_SIZE];
    uint8_t     pes_control_data_buffer[STM_CONTROL_DATA_PES_SIZE];
    uint16_t    control_data_size = 0;
    int i;

    // Initialise the user buffer.
    for (i = 0; i < sizeof(user_buffer); i++)
    {
        user_buffer[i] = i;
    }

    for (i = 0; i < sizeof(pes_control_data_buffer); i++)
    {
        pes_control_data_buffer[i] = 0xff;
    }

    RetCode = stm_pes_control_data_create(
                  STM_CONTROL_DATA_BREAK_BACKWARD_SMOOTH_REVERSE,
                  pes_control_data_buffer,
                  sizeof(pes_control_data_buffer),
                  &control_data_size,
                  user_buffer,
                  sizeof(user_buffer));
    if (RetCode)
    {
        pr_err("stm_pes_control_data_create failed\n");
        return;
    }

    /* Injection size is coming from the value written in ControlDataTestInjectionSizeEntry */
    if (Context.InjectionSize == 0)
    {
        pr_err("Stream cannot be played\n");
        pr_info("Injection size is not set\n");
        return;
    }

    InjectionSize = Context.InjectionSize;
    pr_info("Injection size is of %d bytes\n", Context.InjectionSize);
    stm_se_playback_set_speed(Context.PlaybackHandle, Context.PlaybackSpeed);

    if ((Context.StreamSize == 0) || (Context.Stream == NULL))
    {
        pr_err("Stream cannot be played\n");
        pr_err("No stream set\n");
        return;
    }

    Context.Stream -= Context.StreamSize;

    while (InjectedData != Context.StreamSize)
    {
        if ((Context.StreamSize - InjectedData) < InjectionSize)
        {
            InjectionSize = Context.StreamSize - InjectedData;
        }

        stm_se_play_stream_inject_data(StreamHandle, Context.Stream, InjectionSize);

        if (Context.PlaybackSpeed < 0)
        {
            InjectionNumber ++;

            if (InjectionNumber == 4)
            {
                stm_se_play_stream_inject_data(StreamHandle, pes_control_data_buffer, control_data_size);
                InjectionNumber = 0;
            }
        }

        Context.Stream += InjectionSize;
        InjectedData += InjectionSize;
        pr_info("transfered %d bytes\n", InjectionSize);
        //pr_info("transfered %d control_data bytes\n", control_data_size);
    }
}

void ControlDataTestChangeStream()
{
    /* Reset stream params */
    Context.Stream -= Context.StreamSize;
    Context.StreamSize = 0;
}

static int ControlDataTestIntSet(void *data, u64 val)
{
    *(int *)data = val;
    return 0;
}
static int ControlDataTestIntGet(void *data, u64 *val)
{
    *val = *(int *)data;
    return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(int_fops, ControlDataTestIntGet, ControlDataTestIntSet, "%lld\n");

uint8_t ControlDataTestDbgfsOpen(void)
{
    /* set up debugfs dir and entries for control data test */
    ControlDataTestDir = debugfs_create_dir("KTM_ControlData", NULL);

    if (!ControlDataTestDir)
    {
        pr_err("Error: %s Couldn't create debugfs app directory\n", __func__);
        return -EINVAL;
    }
    else
    {
        /* init test control in debugfs */
        ControlDataTestStreamEntry = debugfs_create_file("ControlDataStream", 0666, ControlDataTestDir, &StreamFileValue, &file_ops);

        if (ControlDataTestStreamEntry != NULL)
        {
            pr_info("Created ControlDataStream in debugfs\n");
        }
        else
        {
            pr_err("Couldn't create ControlDataStream in debugfs\n");
            return -EINVAL;
        }

        ControlDataTestCmdEntry = debugfs_create_file("ControlDataCmd", 0666, ControlDataTestDir, &CmdFileValue, &cmd_fops);

        if (ControlDataTestCmdEntry != NULL)
        {
            pr_info("Created ControlDataCmd in debugfs\n");
        }
        else
        {
            pr_err("Couldn't create ControlDataCmd in debugfs\n");
            return -EINVAL;
        }

        ControlDataTestInjectionSizeEntry = debugfs_create_u32("ControlDataInjectionSize", 0666, ControlDataTestDir, &Context.InjectionSize);

        if (ControlDataTestInjectionSizeEntry != NULL)
        {
            pr_info("Created ControlDataInjectionSize in debugfs\n");
        }
        else
        {
            pr_err("Couldn't create ControlDataInjectionSize in debugfs\n");
            return -EINVAL;
        }

        ControlDataTestPlaybackSpeedEntry = debugfs_create_file("ControlDataPlaybackSpeed", 0666, ControlDataTestDir, &Context.PlaybackSpeed, &int_fops);

        if (ControlDataTestPlaybackSpeedEntry != NULL)
        {
            pr_info("Created ControlDataPlaybackSpeed in debugfs\n");
        }
        else
        {
            pr_err("Couldn't create ControlDataPlaybackSpeed in debugfs\n");
            return -EINVAL;
        }
    }

    return 0;
}

void ControlDataTestDbgfsClose(void)
{
    if (ControlDataTestDir != NULL)
    {
        /* remove test files from debugfs */
        if (ControlDataTestStreamEntry)
        {
            debugfs_remove(ControlDataTestStreamEntry);
        }

        if (ControlDataTestCmdEntry)
        {
            debugfs_remove(ControlDataTestCmdEntry);
        }

        if (ControlDataTestInjectionSizeEntry)
        {
            debugfs_remove(ControlDataTestInjectionSizeEntry);
        }

        if (ControlDataTestPlaybackSpeedEntry)
        {
            debugfs_remove(ControlDataTestPlaybackSpeedEntry);
        }

        if (ControlDataTestDir)
        {
            debugfs_remove(ControlDataTestDir);
        }
    }
}
