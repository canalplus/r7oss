

/***********************************************************************
 *
 * File: linux/kernel/drivers/stm/coredisplaytest/displaytestdbgfs.c
 * Copyright (c) 2011 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
 * Source file name : displaytestdbgfs.c
 * Description      : 1. create the coredisplay test control file in debugfs
 *                    2. allow the user space set the display test commands to the test module
 *
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

#include <vibe_debug.h>

#define TEST_CTRL_CMD_SIZE     360  /* 7108: 240; */
                                    /* orly: 360; */

int32_t CmdFileValue;
uint8_t ResetFlag = 0;
static char DisplayTestCtrlCmd[TEST_CTRL_CMD_SIZE] = {0,};
static char LastCmdResult[TEST_CTRL_CMD_SIZE] = {0,};
static struct dentry *DisplayTestCtrlCmdEntry = NULL;
static struct dentry *LastCmdResultEntry = NULL;
static struct dentry *CoreDisplayTestDir = NULL;
static struct dentry *ResetFlagEntry = NULL;


static ssize_t read_test_command(struct file *file, char __user *userbuf, size_t count, loff_t *ppos)
{
    ssize_t size;
    size = simple_read_from_buffer(userbuf, count, ppos, DisplayTestCtrlCmd, TEST_CTRL_CMD_SIZE);
    return size;
}

static ssize_t write_test_command(struct file *file, const char __user *buf, size_t count, loff_t *ppos)
{
    if(count > TEST_CTRL_CMD_SIZE)
    {
        TRC(TRC_ID_ERROR, "CmdLength = %d is above maximum length of TEST_CTRL_CMD_SIZE!", count);
        return -EINVAL;
    }
    if(copy_from_user(DisplayTestCtrlCmd, buf, count))
        return -EFAULT;
    return count;
}

static ssize_t read_test_result(struct file *file, char __user *userbuf, size_t count, loff_t *ppos)
{
    ssize_t size;
    size= simple_read_from_buffer(userbuf, count, ppos, LastCmdResult, TEST_CTRL_CMD_SIZE);
    return size;

}

static const struct file_operations cmd_fops =
{
    owner: THIS_MODULE,
    read:  read_test_command,
    write: write_test_command
};


static const struct file_operations result_fops =
{
    owner: THIS_MODULE,
    read:  read_test_result
};


int displaytest_dbgfs_open(void)
{
    /* set up debugfs dir and entries for coredisplaytest */
    CoreDisplayTestDir = debugfs_create_dir("CoreDisplayTest", NULL);
    if (!CoreDisplayTestDir)
    {
        TRC(TRC_ID_ERROR, "Couldn't create debugfs app directory.");
        return -EINVAL;
    }
    else
    {
        /* init test control in debugfs */
        DisplayTestCtrlCmdEntry = debugfs_create_file("DisplayTestCtrlCmd", 0666, CoreDisplayTestDir, &CmdFileValue, &cmd_fops);
        LastCmdResultEntry = debugfs_create_file("LastCmdResult", 0666, CoreDisplayTestDir, &CmdFileValue, &result_fops);
        ResetFlagEntry          = debugfs_create_u8("ResetStatistics", 0666, CoreDisplayTestDir, &ResetFlag);
    }
    return 0;
}

void displaytest_dbgfs_close(void )
{
    if(CoreDisplayTestDir != NULL)
    {
        /* remove test control from debugfs */
        if(DisplayTestCtrlCmdEntry)
            debugfs_remove (DisplayTestCtrlCmdEntry);
        if(LastCmdResultEntry)
            debugfs_remove (LastCmdResultEntry);
        if(ResetFlagEntry)
            debugfs_remove (ResetFlagEntry);
        if(CoreDisplayTestDir)
            debugfs_remove(CoreDisplayTestDir);
    }
}

char* displaytest_get_cmd_str(void)
{
    return DisplayTestCtrlCmd;
}

void displaytest_reset_cmd_str(void)
{
    memset((void*)DisplayTestCtrlCmd, 0, TEST_CTRL_CMD_SIZE);
}

void displaytest_set_result_str(char *result)
{
   strncpy(LastCmdResult, result, sizeof(LastCmdResult));
   LastCmdResult[sizeof(LastCmdResult) - 1] = 0;
}

void displaytest_reset_result_str(void)
{
   memset((void*)LastCmdResult, 0, TEST_CTRL_CMD_SIZE);
}


bool displaytest_get_reset_flag(void)
{
    return ((ResetFlag>0)? true:false);
}
void  displaytest_clear_reset_flag(void)
{
    ResetFlag = 0;
}

