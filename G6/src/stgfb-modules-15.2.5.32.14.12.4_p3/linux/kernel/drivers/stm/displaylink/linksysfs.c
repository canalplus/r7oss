/***********************************************************************
 *
 * File: linux/kernel/drivers/stm/displaylink/linksysfs.c
 * Copyright (c) 2008-2013 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#include <linux/version.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/fb.h>
#include <linux/interrupt.h>
#include <linux/i2c.h>
#include <linux/kthread.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/mutex.h>
#include <linux/wait.h>

#include <asm/uaccess.h>
#include <asm/irq.h>
#include <linux/semaphore.h>

#include <linux/stm/stmcorelink.h>



/******************************************************************************
 * LINK Sysfs implementation.
 */
 static ssize_t show_link_hdcp_bcaps(struct device *dev, struct device_attribute *attr, char *buf)
{
  struct stmlink *linkp = dev_get_drvdata(dev);
  int ret;
  uint8_t caps;
  int sz = 0;

#define S(x) sz += sprintf(&buf[sz], "%s", #x)
  if ((ret = stm_display_link_hdcp_get_sink_caps_info(linkp->link, &caps, 0)) < 0)
  {
    S (not_available);
  }
  else
  {
    sz += sprintf(buf,"0x%x",caps);
  }
  S(\n);
#undef S
  return sz;
}

static ssize_t show_link_hdcp_bstatus(struct device *dev, struct device_attribute *attr, char *buf)
{
  struct stmlink *linkp = dev_get_drvdata(dev);
  int ret;
  uint16_t status;
  int sz = 0;

#define S(x) sz += sprintf(&buf[sz], "%s", #x)
  if ((ret = stm_display_link_hdcp_get_sink_status_info(linkp->link, &status, 0)) < 0)
  {
    S (not_available);
  }
  else
  {
    sz += sprintf(buf,"0x%2x",status);
  }
  S(\n);
#undef S
  return sz;
}

static ssize_t show_link_hdcp_status(struct device *dev, struct device_attribute *attr, char *buf)
{
  stm_display_link_hdcp_status_t state;
  struct stmlink *linkp = dev_get_drvdata(dev);
  int ret;
  int sz = 0;

#define S(x) sz += sprintf(&buf[sz], "%s", #x)
  if ((ret = stm_display_link_hdcp_get_status(linkp->link, &state)) < 0)
  {
    S (not_available);
  }
  else
  {
    switch (state)
    {
    case STM_DISPLAY_LINK_HDCP_STATUS_DISABLED:
      S(disabled);
      break;
    case STM_DISPLAY_LINK_HDCP_STATUS_IN_PROGRESS:
      S(in_progress);
      break;
    case STM_DISPLAY_LINK_HDCP_STATUS_SUCCESS_ENCRYPTING:
      S(encrypting);
      break;
    case STM_DISPLAY_LINK_HDCP_STATUS_SUCCESS_NOT_ENCRYPTING:
      S(not_encrypting);
      break;
    case STM_DISPLAY_LINK_HDCP_STATUS_FAIL_NO_ACK:
      S(fail_no_ack);
      break;
    case STM_DISPLAY_LINK_HDCP_STATUS_FAIL_BKSV_INVALID:
      S(fail_bksv_invalid);
      break;
    case STM_DISPLAY_LINK_HDCP_STATUS_FAIL_KSV_REVOKED:
      S(fail_ksv_revoked);
      break;
    case STM_DISPLAY_LINK_HDCP_STATUS_FAIL_MAX_CASCADE_EXCEEDED:
      S(fail_max_cascade_exceeded);
      break;
    case STM_DISPLAY_LINK_HDCP_STATUS_FAIL_MAX_DEVICE_COUNT_EXCEEDED:
      S(fail_max_device_count_exceeded);
      break;
    case STM_DISPLAY_LINK_HDCP_STATUS_FAIL_V_CHECK:
      S(fail_v_check);
      break;
    case STM_DISPLAY_LINK_HDCP_STATUS_FAIL_R0_CHECK:
      S(fail_r0_check);
      break;
    case STM_DISPLAY_LINK_HDCP_STATUS_FAIL_FIFO_NOT_READY:
      S(fail_fifo_not_ready);
      break;
    case STM_DISPLAY_LINK_HDCP_STATUS_FAIL_WDT_TIMEOUT:
      S(fail_wdt_timeout);
      break;
    case STM_DISPLAY_LINK_HDCP_STATUS_FAIL_LINK_INTEGRITY:
      S(fail_link_integrity);
      break;
    case STM_DISPLAY_LINK_HDCP_STATUS_FAIL_ENHANCED_LINK_VERIFICATION:
      S(fail_enhanced_link_verification);
      break;
    default:
      S(unknown);
      break;
    }
  }
  S(\n);
#undef S
  return sz;
}

static struct device_attribute stmlink_device_attrs[] = {
__ATTR(hdcp_bcaps, S_IRUGO, show_link_hdcp_bcaps, NULL),
__ATTR(hdcp_bstatus, S_IRUGO, show_link_hdcp_bstatus, NULL),
__ATTR(hdcp_status, S_IRUGO, show_link_hdcp_status, NULL)
};

int stmlink_create_class_device_files(struct stmlink *linkp, struct device *display_class_device)
{
  int i,ret;

  for (i = 0; i < ARRAY_SIZE(stmlink_device_attrs); i++) {
    ret = device_create_file(linkp->class_device, &stmlink_device_attrs[i]);
    if(ret)
      break;
  }
  return 0;
}

