/***********************************************************************
 *
 * File: linux/kernel/drivers/stm/displaylink/linkhdcp_fops.c
 * Copyright (c) 2013 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/cdev.h>
#include <linux/version.h>
#include <linux/compiler.h>
#include <linux/init.h>
#include <linux/mm.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/mutex.h>
#include <linux/wait.h>
#include <linux/device.h>
#include <linux/cdev.h>
#include <linux/platform_device.h>
#include <linux/pm.h>
#include <linux/pm_runtime.h>

#include <asm/uaccess.h>
#include <linux/semaphore.h>

#include <stm_display.h>
#include <stm_display_link.h>
#include <linux/stm/stmcoredisplay.h>
#include <linux/stm/stmcorelink.h>

#include "stmhdcp.h"

/* -------------------- port of Vibe on kernel 3.10 ------------------------ */
/*
 * The VM flag VM_RESERVED has been removed and VM_DONTDUMP should be
 * used instead. Unfortunately VM_DONTDUMP is not avilable in 3.4,
 * hence the code below :-(
 */
#include <linux/version.h>
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3, 10, 0))
#define   LINKHDCP_VM_FLAGS (VM_IO | VM_RESERVED | VM_DONTEXPAND)
#else
#define   LINKHDCP_VM_FLAGS (VM_IO | VM_DONTDUMP | VM_DONTEXPAND)
#endif

extern int stmlink_wait_for_vsync(struct stmlink *linkp);

static int stmlink_open(struct inode *inode,struct file *filp)
{
  struct stmlink *linkp = container_of(inode->i_cdev, struct stmlink, cdev);
  filp->private_data = linkp;

  pm_runtime_get_sync(&linkp->pdev->dev);
  return 0;
}

static int stmlink_release(struct inode *inode,struct file *filp)
{
  struct stmlink *linkp = filp->private_data;
  pm_runtime_put_sync(&linkp->pdev->dev);
  return 0;
}

static int stmlink_iomm_vma_fault(struct vm_area_struct *vma,
                                 struct vm_fault       *vmf)
{
    /* we want to provoke a bus error rather than give the client
       the zero page */
    return VM_FAULT_SIGBUS;
}

static struct vm_operations_struct stmlink_iomm_nopage_ops = {
  .fault  = stmlink_iomm_vma_fault,
};

static int
stmlink_mmap (struct file *filp,struct vm_area_struct * const vma)
{
  unsigned long rawaddr, physaddr, vsize, off;

  vma->vm_flags |= LINKHDCP_VM_FLAGS;
  vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);

  rawaddr  = (vma->vm_pgoff << PAGE_SHIFT);
  physaddr = rawaddr;
  vsize = vma->vm_end - vma->vm_start;

  for (off=0; off<vsize; off+=PAGE_SIZE)
  {
    io_remap_pfn_range(vma, vma->vm_start+off, (physaddr+off) >> PAGE_SHIFT, PAGE_SIZE, vma->vm_page_prot);
  }

  // ensure we get bus errors when we access illegal memory address
  vma->vm_ops = &stmlink_iomm_nopage_ops;

  return 0;
}

long stmlink_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
  struct stmlink *dev = (struct stmlink *)filp->private_data;
  long retval=0;

  switch(cmd)
  {
    case STMHDCPIO_SET_HDCP_STATUS :
    {
      uint32_t val;
      /*
       * Before Starting HDCP, check whether display is valid or not.
       * if display is valid, this implies HDMI/DVI interface was enabled.
       */
      if ((retval = stm_display_link_get_ctrl(dev->link, STM_DISPLAY_LINK_CTRL_DISPLAY_MODE, &val))<0){
        break;
      }

      if (arg ==0){
        /*
         * HDCP disabling is requested. So, HDCP starting is no more required.
         * Furthermore, HDCP is stopped when Display is still valid which implies TMDS running
         */
        if ((retval =stm_display_link_set_ctrl(dev->link, STM_DISPLAY_LINK_CTRL_HDCP_START, 0))>=0){
          if (val != 0){
            retval = stm_display_link_set_ctrl(dev->link, STM_DISPLAY_LINK_CTRL_HDCP_ENABLE, 0);
          }
        }
      }
      else
      {
        /*
         * HDCP Enabled, check whether HDMI/DVI interface has been already enabled
         * Start and prepare HDCP when display is invalid : TMDS off.
         * This will be transparent for user when TMDS start running,
         * HDCP will be switched on automatically according to the enabled interface.
         */
        if (val != 0){
          retval = stm_display_link_set_ctrl(dev->link, STM_DISPLAY_LINK_CTRL_HDCP_ENABLE, val);
        }
        else
        {
          retval = stm_display_link_set_ctrl(dev->link, STM_DISPLAY_LINK_CTRL_HDCP_START, 1);
        }
      }
      break;
    }
    case STMHDCPIO_SET_ENCRYPTED_FRAME :
    {
      if (stm_display_link_set_ctrl(dev->link, STM_DISPLAY_LINK_CTRL_HDCP_FRAME_ENCRYPTION_ENABLE, arg) <0)
        retval = -EIO;
      break;
    }
    case STMHDCPIO_STOP_HDCP :
    {
      if (stm_display_link_set_ctrl(dev->link, STM_DISPLAY_LINK_CTRL_HDCP_ENABLE, 0) <0)
        retval = -EIO;
      break;
    }
    case STMHDCPIO_SET_FORCE_AUTHENTICATION :
    {
      if (stm_display_link_set_ctrl(dev->link, STM_DISPLAY_LINK_CTRL_HDCP_ENABLE, 0) <0)
        retval = -EIO;
      if (stm_display_link_set_ctrl(dev->link, STM_DISPLAY_LINK_CTRL_HDCP_ENABLE, 1) <0)
        retval = -EIO;
      break;
    }
    case STMHDCPIO_GET_HDCP_STATUS :
    {
      __u32 hdcp_status;
      if (stm_display_link_hdcp_get_status(dev->link, &hdcp_status) <0)
        retval = -EIO;
      if(put_user(hdcp_status, (__u32 *)arg))
        retval = -EFAULT;

      break;
    }
    case STMHDCPIO_WAIT_FOR_VSYNC :
    {
      if (arg <= 0) {
        DPRINTK("invalid VYSNC count %d\n",dev->vsync_wait_count );
        retval = -EINVAL;
      }
      dev->vsync_wait_count = arg;
      retval = stmlink_wait_for_vsync(dev);
      if (retval) {
        DPRINTK("failed to wait for %d vsync's",dev->vsync_wait_count );
        retval = -EAGAIN;
      }
      break;
    }
    default:
      retval = -ENOTTY;
      break;

  }
  return retval;
}

struct file_operations link_fops = {
  .owner   = THIS_MODULE,
  .open    = stmlink_open,
  .release = stmlink_release,
  .mmap    = stmlink_mmap,
  .unlocked_ioctl = stmlink_ioctl
};
