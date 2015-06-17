/***********************************************************************
 *
 * File: linux/kernel/drivers/stm/pixel_capture/class.c
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
#include <linux/device.h>
#include <linux/kdev_t.h>
#include <linux/slab.h>
#include <linux/fs.h>

#include <linux/cdev.h>

#include "class.h"
#include "capture.h"

struct stm_capture_class_device {
  struct device                     *sccd_device;
  dev_t                              sccd_devt;
};


static struct class *stm_capture_class;
static dev_t stm_capture_devt;


#if !defined(KBUILD_SYSTEM_INFO)
#define KBUILD_SYSTEM_INFO "<unknown>"
#endif
#if !defined(KBUILD_USER)
#define KBUILD_USER "<unknown>"
#endif
#if !defined(KBUILD_SOURCE)
#define KBUILD_SOURCE "<unknown>"
#endif
#if !defined(KBUILD_VERSION)
#define KBUILD_VERSION "<unknown>"
#endif
#if !defined(KBUILD_DATE)
#define KBUILD_DATE "<unknown>"
#endif

struct stm_capture_class_device *
stm_capture_classdev_init (int                                i,
         struct device                     *parent,
         void                              *dev_data,
         struct device_attribute           *dev_attrs)
{
  struct stm_capture_class_device *sccd;
  int                              res=res; /* suppress GCC warning */
  char name[20];

  sccd = kzalloc (sizeof (*sccd), GFP_KERNEL);
  if (sccd == NULL)
    return ERR_PTR (-ENOMEM);

  sccd->sccd_devt = MKDEV (MAJOR (stm_capture_devt), i);

  snprintf(name, sizeof (name), "stm-capture.%u", i);
  sccd->sccd_device = device_create (stm_capture_class, parent,
             sccd->sccd_devt,
             dev_data, name);
  if (IS_ERR (sccd->sccd_device)) {
    printk (KERN_ERR "Failed to create class device %d\n", i);
    res = PTR_ERR (sccd->sccd_device);
    goto out;
  }

  if (dev_attrs) {
    for (i = 0; attr_name (dev_attrs[i]); ++i) {
      res = device_create_file (sccd->sccd_device,
              &dev_attrs[i]);
      if (res)
        break;
    }

    if (res) {
      while (--i >= 0)
        device_remove_file (sccd->sccd_device,
                &dev_attrs[i]);
      goto out_device;
    }
  }

  return sccd;

out_device:
  device_destroy (stm_capture_class, sccd->sccd_devt);

out:
  kfree (sccd);
  return ERR_PTR (res);
}

int
stm_capture_classdev_deinit (struct stm_capture_class_device *sccd)
{
  device_destroy (stm_capture_class, sccd->sccd_devt);

  kfree (sccd);

  printk(KERN_INFO "%s: deinit class ok\n",__func__);

  return 0;
}


static struct device_attribute stm_capture_dev_attrs[] = {
  __ATTR_NULL
};


#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 34))
static CLASS_ATTR_STRING(info, S_IRUGO,
      "Build Source:  " KBUILD_SOURCE "\n"
      "Build Version: " KBUILD_VERSION "\n"
      "Build User:    " KBUILD_USER "\n"
      "Build Date:    " KBUILD_DATE "\n"
      "Build System:  " KBUILD_SYSTEM_INFO "\n");
#else /* 2.6.34 */
static ssize_t
stm_capture_show_driver_info (struct class *class,
            char         *buf)
{
  int sz = 0;

  sz += snprintf (&buf[sz], PAGE_SIZE - sz,
      "Build Source:  " KBUILD_SOURCE "\n");
  sz += snprintf (&buf[sz], PAGE_SIZE - sz,
      "Build Version: " KBUILD_VERSION "\n");
  sz += snprintf (&buf[sz], PAGE_SIZE - sz,
      "Build User:    " KBUILD_USER "\n");
  sz += snprintf (&buf[sz], PAGE_SIZE - sz,
      "Build Date:    " KBUILD_DATE "\n");
  sz += snprintf (&buf[sz], PAGE_SIZE - sz,
      "Build System:  " KBUILD_SYSTEM_INFO "\n");

  return sz;
}

static CLASS_ATTR (info, S_IRUGO, stm_capture_show_driver_info, NULL);
#endif /* 2.6.34 */


int __init
stm_capture_class_init (int n_devices)
{
  int res;

  stm_capture_class = class_create (THIS_MODULE, "stm-capture");
  if (IS_ERR (stm_capture_class)) {
    printk (KERN_ERR "%s: couldn't create class device\n",
      __func__);
    return PTR_ERR (stm_capture_class);
  }
  stm_capture_class->dev_attrs = stm_capture_dev_attrs;

  if ((res = class_create_file (stm_capture_class,
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 34))
              &class_attr_info.attr)) < 0) {
#else /* 2.6.34 */
              &class_attr_info)) < 0) {
#endif /* 2.6.34 */
    printk (KERN_ERR "%s: couldn't create class device info attr\n",
      __func__);
    return -ENOMEM;
  }

  if ((res = alloc_chrdev_region (&stm_capture_devt, 0, n_devices,
          "stm-capture")) < 0) {
    printk (KERN_ERR "%s: couldn't allocate device numbers\n",
      __func__);
    return res;
  }

  return res;
}


void
stm_capture_class_cleanup (int n_devices)
{
  class_destroy (stm_capture_class);

  unregister_chrdev_region (stm_capture_devt, n_devices);
}
