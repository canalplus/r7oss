/***********************************************************************
 *
 * File: linux/kernel/drivers/stm/pixel_capture/capture.c
 * Copyright (c) 2012 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#include <linux/kernel.h>
#include <linux/version.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 2, 2)
#include <linux/export.h>
#else
#include <linux/module.h>
#endif
#include <linux/errno.h>
#include <linux/err.h>
#include <linux/kref.h>
#include <linux/device.h>
#include <linux/mutex.h>

#include "capture.h"


struct stm_pixel_capture_device_s
{
  struct stm_capture_class_device *sccd;

  void                     *data;
  struct mutex              mutex;
};

static struct stm_pixel_capture_device_s capture_devices[STM_CAPTURE_MAX_DEVICES];

int
stm_capture_register_device (struct device                     *dev,
           struct device_attribute           *attrs,
           void                              *data)
{
  unsigned int i;

  for (i = 0; i < ARRAY_SIZE (capture_devices); ++i) {
    printk(KERN_INFO "%s: try pixel capture device id %d!\n",__func__, i);
    if (!capture_devices[i].data) {
      struct stm_pixel_capture_device_s *capture = &capture_devices[i];
      printk(KERN_INFO "%s: get pixel capture device id %d!\n",__func__, i);

      capture->sccd = stm_capture_classdev_init (i,
                   dev,
                   data,
                   attrs);
      if (IS_ERR (capture->sccd))
        return PTR_ERR (capture->sccd);

      capture->data = data;

      mutex_init(&capture->mutex);

      return 0;
    }
  }

  return -ENOMEM;
}

int
stm_capture_unregister_device (void *data)
{
  unsigned int i;

  printk(KERN_INFO "%s: unregister device with data %p!\n",__func__, data);
  for (i = 0; i < ARRAY_SIZE (capture_devices); ++i) {
    if (capture_devices[i].data == data) {
      int res = stm_capture_classdev_deinit (capture_devices[i].sccd);

      capture_devices[i].data = NULL;

      return res;
    }
  }

  return -ENODEV;
}
