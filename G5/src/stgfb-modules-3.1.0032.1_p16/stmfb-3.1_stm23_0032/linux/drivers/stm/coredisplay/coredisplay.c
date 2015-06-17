/***********************************************************************
 *
 * File: linux/drivers/stm/coredisplay/coredisplay.c
 * Copyright (c) 2000-2009 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#include <linux/version.h>
#include <linux/kernel.h>
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
#include <linux/pm.h>
#include <linux/string.h>
#include <linux/ktime.h>

#include <asm/irq.h>
#include <asm/semaphore.h>

#include <stmdisplay.h>
#include <linux/stm/stmcoredisplay.h>
#include <linux/stm/stmcorehdmi.h>

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,17)
  #error This driver version is for ST LDDE2.2 and kernel version >= 2.6.17
#elif LINUX_VERSION_CODE < KERNEL_VERSION(2,6,20)
  #define IRQF_DISABLED SA_INTERRUPT
  #define IRQF_SHARED   SA_SHIRQ
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,19)
  typedef irqreturn_t (*stm_irq_handler_type)(int, void *, struct pt_regs *);
#else
  typedef irqreturn_t (*stm_irq_handler_type)(int, void *);
#endif


static struct stmcore_display_pipeline_data *platform_data;
static int nr_platform_devices;
static struct class *stmcore_class;
static dev_t firstdevice;

extern int stmcore_probe_device(struct stmcore_display_pipeline_data **pd, int *nr_platform_devices);
extern int stmcore_display_postinit(struct stmcore_display *);
extern void stmcore_cleanup_device(void);

/*
 * First level vsync interrupt handler
 */
static irqreturn_t stmcore_vsync_interrupt(int irq, void* data)
{
  struct stmcore_display_pipeline_data *pd = (struct stmcore_display_pipeline_data *)data;
  struct stmcore_display *runtime = pd->display_runtime;
  stm_field_t vsync;
  TIME64 interval;
  ktime_t t1,t2;


  t1 = ktime_get();

  if(pd == 0)
    panic("stmfb_vsync_interrupt: null info pointer\n");

  stm_display_output_handle_interrupts(runtime->main_output);
  stm_display_output_get_last_vsync_info(runtime->main_output, &vsync, &interval);

  if(vsync == STM_NOT_A_FIELD)
    // This was a vtimer callback interrupt (and the main driver has already handled it)
    return IRQ_HANDLED;

  wake_up_interruptible(&runtime->vsync_wait_queue);
  atomic_inc (&runtime->vsync_count);

  stm_display_update(pd->device, runtime->main_output);

  /*
   * Process VSync callback hooks
   */
  {
    struct stmcore_vsync_cb *pCallback, *tmp;
    unsigned long            flags;
    read_lock_irqsave (&runtime->vsync_cb_list.lock, flags);
    list_for_each_entry_safe (pCallback, tmp, &runtime->vsync_cb_list.list, node)
    {
      pCallback->cb(pCallback->context, vsync);
    }
    read_unlock_irqrestore (&runtime->vsync_cb_list.lock, flags);
  }

  t2 = ktime_get();
  runtime->last_display_processing_us = ktime_to_us(ktime_sub(t2,t1));

  runtime->avg_display_processing_us *= 15;
  runtime->avg_display_processing_us += runtime->last_display_processing_us;
  runtime->avg_display_processing_us /= 16;

  if(runtime->last_display_processing_us < runtime->min_display_processing_us)
    runtime->min_display_processing_us = runtime->last_display_processing_us;
  if(runtime->last_display_processing_us > runtime->max_display_processing_us)
    runtime->max_display_processing_us = runtime->last_display_processing_us;

  return IRQ_HANDLED;
}


static irqreturn_t stmcore_blitter_interrupt(int irq, void* data)
{
  stm_display_blitter_t *blitter = (stm_display_blitter_t *)data;

  return stm_display_blitter_handle_interrupt(blitter) ? IRQ_HANDLED
                                                       : IRQ_NONE;
}


static void __exit stmcore_display_exit(void)
{
  int i;

  /*
   * Destroy the platform devices and global core driver handles
   */
  for(i=0;i<nr_platform_devices;i++)
  {
    if(!platform_data[i].display_runtime)
      continue;

    if(platform_data[i].vtg_irq != -1)
    {
      flush_scheduled_work();
      free_irq(platform_data[i].vtg_irq, &platform_data[i]);
      platform_data[i].vtg_irq = -1;
    }

    if(platform_data[i].blitter_irq_kernel != -1)
    {
      free_irq(platform_data[i].blitter_irq_kernel, platform_data[i].blitter_kernel);
      platform_data[i].blitter_irq_kernel = -1;
    }
    if(platform_data[i].blitter_irq != -1)
    {
      free_irq(platform_data[i].blitter_irq, platform_data[i].blitter);
      platform_data[i].blitter_irq = -1;
    }

    if(platform_data[i].class_device)
    {
      class_device_unregister(platform_data[i].class_device);
      platform_data[i].class_device = NULL;
    }

    if(platform_data[i].platform_device)
    {
      platform_device_unregister(platform_data[i].platform_device);
      platform_data[i].platform_device = NULL;
    }

    if (platform_data[i].hdmi_data)
    {
      stmhdmi_destroy(platform_data[i].hdmi_data);
      platform_data[i].hdmi_data = NULL;
    }

    stm_display_output_release(platform_data[i].display_runtime->main_output);

    if (platform_data[i].display_runtime->hdmi_output)
    {
      stm_display_output_release(platform_data[i].display_runtime->hdmi_output);
      platform_data[i].display_runtime->hdmi_output = NULL;
    }

    if (platform_data[i].blitter_kernel
        && platform_data[i].blitter_kernel != platform_data[i].blitter)
    {
      stm_display_blitter_release(platform_data[i].blitter_kernel);
      platform_data[i].blitter_kernel = NULL;
    }
    if (platform_data[i].blitter)
    {
      stm_display_blitter_release(platform_data[i].blitter);
      platform_data[i].blitter = NULL;
    }

    kfree(platform_data[i].display_runtime);
    platform_data[i].display_runtime = NULL;
  }

  for(i=0;i<nr_platform_devices;i++)
  {
    stm_display_release_device(platform_data[i].device);
    platform_data[i].device = NULL;
  }

  stmcore_cleanup_device();

  class_destroy(stmcore_class);
  unregister_chrdev_region(firstdevice,nr_platform_devices);
}


static int __init stmcore_platformdev_init(int i, struct stmcore_display_pipeline_data *platform_data)
{
  int res;
  /*
   * Create a platform device for this display pipeline
   */
  platform_data->platform_device = platform_device_alloc("stmcore-display",i);

  if(IS_ERR(platform_data->platform_device))
    return -ENODEV;

  /*
   * Note that adding the platform device data makes a shallow copy of
   * the _pointer_ to the platform structure in this case.
   */
  res = platform_device_add_data(platform_data->platform_device,&platform_data, sizeof(struct stmcore_display_pipeline_data *));
  if(res<0)
  {
    platform_device_put(platform_data->platform_device);
    platform_data->platform_device = NULL;
    return -ENODEV;
  }

  res = platform_device_add(platform_data->platform_device);
  if(res<0)
  {
     printk(KERN_ERR "Failed to add platform device %d\n",i);
     platform_device_put(platform_data->platform_device);
     platform_data->platform_device = NULL;
     return -ENODEV;
  }


  return 0;
}


static ssize_t stmcore_show_display_info(struct class_device *class_device, char *buf)
{
  struct stmcore_display_pipeline_data *platform_data = class_get_devdata(class_device);
  int sz=0;
  int plane;

  sz += snprintf(&buf[sz], PAGE_SIZE - sz, "Name: %s\n", platform_data->name);

  sz += snprintf(&buf[sz], PAGE_SIZE - sz, "Analogue: Output ID=%d\n", platform_data->main_output_id);

  if(platform_data->hdmi_output_id!=-1)
    sz += snprintf(&buf[sz], PAGE_SIZE - sz, "HDMI: Output ID=%d\n", platform_data->hdmi_output_id);
  else
    sz += snprintf(&buf[sz], PAGE_SIZE - sz, "HDMI: (NONE)\n");

  if(platform_data->dvo_output_id!=-1)
    sz += snprintf(&buf[sz], PAGE_SIZE - sz, "DVO: Output ID=%d\n", platform_data->dvo_output_id);
  else
    sz += snprintf(&buf[sz], PAGE_SIZE - sz, "DVO: (NONE)\n");

  if(platform_data->blitter_id == -1)
  {
    sz += snprintf(&buf[sz], PAGE_SIZE - sz, "Blitter: (NONE)\n");
  }
  else
  {
    sz += snprintf(&buf[sz], PAGE_SIZE - sz, "Blitter%d: %s\n",platform_data->blitter_id,
                                                               platform_data->blitter_type == STMCORE_BLITTER_GAMMA?"Gamma":"BDispII");
  }
  if(platform_data->blitter_id_kernel == -1)
  {
    sz += snprintf(&buf[sz], PAGE_SIZE - sz, "Kernel Blitter: (NONE)\n");
  }
  else
  {
    sz += snprintf(&buf[sz], PAGE_SIZE - sz, "Kernel Blitter%d: %s\n",
                   platform_data->blitter_id_kernel, platform_data->blitter_type == STMCORE_BLITTER_GAMMA?"Gamma":"BDispII");
  }

  for(plane=0;plane<STMCORE_MAX_PLANES;plane++)
  {
    struct stmcore_plane_data *p = &platform_data->planes[plane];
    if(p->id != 0)
    {
      sz += snprintf(&buf[sz], PAGE_SIZE - sz, "Plane%d: ",plane);
      sz += snprintf(&buf[sz], PAGE_SIZE - sz, "%s",p->flags&STMCORE_PLANE_VIDEO?"Video":"Graphics");
      if(p->flags & STMCORE_PLANE_PREFERRED)
        sz += snprintf(&buf[sz], PAGE_SIZE - sz, ", Preferred");

      if(p->flags & STMCORE_PLANE_SHARED)
        sz += snprintf(&buf[sz], PAGE_SIZE - sz, ", Shared");

      sz += snprintf(&buf[sz], PAGE_SIZE - sz, "\n");
    }
  }
  return sz;
}


static ssize_t stmcore_show_display_timing(struct class_device *class_device, char *buf)
{
  struct stmcore_display_pipeline_data *platform_data = class_get_devdata(class_device);
  int sz=0;
  unsigned long saveFlags;
  s64 tlast, tavg, tmax, tmin;

  spin_lock_irqsave(&(platform_data->display_runtime->spinlock), saveFlags);
  tlast = platform_data->display_runtime->last_display_processing_us;
  tavg  = platform_data->display_runtime->avg_display_processing_us;
  tmin  = platform_data->display_runtime->min_display_processing_us;
  tmax  = platform_data->display_runtime->max_display_processing_us;
  platform_data->display_runtime->min_display_processing_us = LLONG_MAX;
  platform_data->display_runtime->max_display_processing_us = 0;
  spin_unlock_irqrestore(&(platform_data->display_runtime->spinlock), saveFlags);

  sz += snprintf(&buf[sz], PAGE_SIZE - sz, "Last display processing time: %lldus\n",tlast);
  sz += snprintf(&buf[sz], PAGE_SIZE - sz, "Average display processing time: %lldus\n",tavg);
  sz += snprintf(&buf[sz], PAGE_SIZE - sz, "Min display processing time: %lldus (reset on read)\n",tmin);
  sz += snprintf(&buf[sz], PAGE_SIZE - sz, "Max display processing time: %lldus (reset on read)\n",tmax);

  return sz;

}


static CLASS_DEVICE_ATTR(info,S_IRUGO, stmcore_show_display_info,NULL);
static CLASS_DEVICE_ATTR(timing,S_IRUGO, stmcore_show_display_timing,NULL);


static int __init stmcore_classdev_init(int i, struct stmcore_display_pipeline_data *platform_data)
{
  int res;
  platform_data->class_device = class_device_create(stmcore_class, NULL, MKDEV(0,0), NULL, "display%d",i);

  if(IS_ERR(platform_data->class_device))
  {
    printk(KERN_ERR "Failed to create class device %d\n",i);
    platform_data->class_device = NULL;
    return -ENODEV;
  }

  class_set_devdata(platform_data->class_device,platform_data);

  res = class_device_create_file(platform_data->class_device,&class_device_attr_info);
  res += class_device_create_file(platform_data->class_device,&class_device_attr_timing);
  return res;
}

#if !defined(KBUILD_SYSTEM_INFO)
#define KBUILD_SYSTEM_INFO "<unknown>"
#endif
#if !defined(KBUILD_USER)
#define KBUILD_USER "<unknown>"
#endif
#if !defined(KBUILD_SOURCE)
#define KBUILD_SOURCE "<unknown>"
#endif
#if !defined(KBUILD_DATE)
#define KBUILD_DATE "<unknown>"
#endif

static ssize_t stmcore_show_driver_info(struct class *class, char *buf)
{
  int sz=0;

  sz += snprintf(&buf[sz], PAGE_SIZE - sz, "Build Source: " KBUILD_SOURCE "\n");
  sz += snprintf(&buf[sz], PAGE_SIZE - sz, "Build User:   " KBUILD_USER "\n");
  sz += snprintf(&buf[sz], PAGE_SIZE - sz, "Build Date:   " KBUILD_DATE "\n");
  sz += snprintf(&buf[sz], PAGE_SIZE - sz, "Build System: " KBUILD_SYSTEM_INFO "\n");

  return sz;
}


static CLASS_ATTR(info,S_IRUGO, stmcore_show_driver_info,NULL);


/* stmfb_init
 *    This function is the first function called in the driver. It has the
 *    responsibility of setting up the hardware for use
 */
int __init stmcore_display_init(void)
{
  int i;
  int res=0;
  int nvaliddevices=0;

  if(stmcore_probe_device(&platform_data,&nr_platform_devices)<0)
    return -ENODEV;

  printk(KERN_ERR "device probe found %d display pipelines\n",nr_platform_devices);

  stmcore_class = class_create(THIS_MODULE,"stmcoredisplay");
  if(IS_ERR(stmcore_class))
  {
    printk(KERN_ERR "%s: unable to create class device\n",__FUNCTION__);
    return -ENOMEM;
  }

  if(class_create_file(stmcore_class,&class_attr_info)<0)
  {
    printk(KERN_ERR "%s: unable to create class device info attr\n",__FUNCTION__);
    return -ENOMEM;
  }

  if(alloc_chrdev_region(&firstdevice,0,nr_platform_devices,"stmcoredisplay")<0)
  {
    printk(KERN_ERR "%s: unable to allocate device numbers\n",__FUNCTION__);
    return -ENODEV;
  }

  for(i=0;i<nr_platform_devices;i++)
  {
    platform_data[i].device = stm_display_get_device((unsigned)platform_data[i].device);

    if(platform_data[i].device == NULL)
    {
      printk(KERN_ERR "%s: failed to get display device! No BPA2 bigphysarea compatibility partition?\n",__FUNCTION__);
      continue;
    }

    platform_data[i].display_runtime = kzalloc(sizeof(struct stmcore_display),GFP_KERNEL);

    if(!platform_data[i].display_runtime)
      continue;

    spin_lock_init(&(platform_data[i].display_runtime->spinlock));
    rwlock_init (&platform_data[i].display_runtime->vsync_cb_list.lock);
    INIT_LIST_HEAD (&platform_data[i].display_runtime->vsync_cb_list.list);

    platform_data[i].display_runtime->min_display_processing_us = LLONG_MAX;

    platform_data[i].display_runtime->main_output = stm_display_get_output(platform_data[i].device, platform_data[i].main_output_id);

    if(platform_data[i].display_runtime->main_output == NULL)
    {
      /* We must have at least the main output in order to change mode! */
      printk(KERN_ERR "platform configuration error: failed to find the main video output\n");
      stm_display_release_device(platform_data[i].device);
      platform_data[i].device = NULL;
      continue;
    }

    atomic_set (&platform_data[i].display_runtime->vsync_count, 0);
    init_waitqueue_head(&platform_data[i].display_runtime->vsync_wait_queue);
    platform_data[i].display_runtime->hotplug_poll_pio = NULL;

    snprintf (platform_data[i].vtg_name, sizeof (platform_data[i].vtg_name),
              "vsync%d", i);
    if(request_irq(platform_data[i].vtg_irq,
                   (stm_irq_handler_type) stmcore_vsync_interrupt,
                   IRQF_DISABLED,
                   platform_data[i].vtg_name,
                   &platform_data[i]) < 0)
      continue;

    /*
     * This is still a bit grotty, it only works properly if the first pipeline
     * has a blitter defined.
     */
    if(platform_data[i].blitter_id != -1)
    {
      platform_data[i].blitter = stm_display_get_blitter(platform_data[i].device, platform_data[i].blitter_id);

      /*
       * There's only one blitter IRQ, even if there are multiple AQs
       * available in the hardware, so we install the IRQ handler shared!
       */
      if(platform_data[i].blitter && platform_data[i].blitter_irq != -1)
      {
        snprintf (platform_data[i].blitter_name, sizeof (platform_data[i].blitter_name),
                  "blitter%d:%d", i, platform_data[i].blitter_id);
        if((res = request_irq(platform_data[i].blitter_irq,
                              (stm_irq_handler_type) stmcore_blitter_interrupt,
                              IRQF_SHARED,
                              platform_data[i].blitter_name,
                              platform_data[i].blitter)) < 0)
        {
          stm_display_blitter_release(platform_data[i].blitter);
          platform_data[i].blitter = NULL;
          platform_data[i].blitter_irq = -1;
        }
      }
    }
    if(platform_data[i].blitter_id_kernel != -1)
    {
      platform_data[i].blitter_kernel = stm_display_get_blitter(platform_data[i].device,
                                                                platform_data[i].blitter_id_kernel);
      if(platform_data[i].blitter_kernel && platform_data[i].blitter_irq_kernel != -1)
      {
        snprintf (platform_data[i].blitter_name_kernel,
                  sizeof (platform_data[i].blitter_name_kernel),
                  "kblitter%d:%d", i, platform_data[i].blitter_id_kernel);
        if((res = request_irq(platform_data[i].blitter_irq_kernel,
                              (stm_irq_handler_type) stmcore_blitter_interrupt,
                              IRQF_SHARED,
                              platform_data[i].blitter_name_kernel,
                              platform_data[i].blitter_kernel)) < 0)
        {
          stm_display_blitter_release(platform_data[i].blitter_kernel);
            platform_data[i].blitter_kernel = NULL;
            platform_data[i].blitter_irq_kernel = -1;
        }
      }
    }
    if(!platform_data[i].blitter_kernel)
      platform_data[i].blitter_kernel = platform_data[i].blitter;

    /*
     * Setup a class device, needed now so a HDMI device can hang off it
     */
    if(stmcore_classdev_init(i,&platform_data[i])<0)
    {
      printk(KERN_ERR "Failed to init class device %d\n",i);
      continue;
    }

    /*
     * Setup HDMI if present, however failure is not fatal
     */
    if(platform_data[i].hdmi_output_id != -1 && platform_data[i].hdmi_i2c_adapter_id != -1)
    {
      stmhdmi_create(i, firstdevice, &platform_data[i]);
      /*
       * We require a handle to the HDMI output in the display runtime as
       * well, so we can do board specific postinit configuration on it.
       * Although it may seem like we should just pass the platform data to
       * postinit and let it access the HDMI runtime data, we would like to
       * avoid this as there is a long term goal of separating out the HDMI
       * device and kernel thread from of the core display module.
       */
      platform_data[i].display_runtime->hdmi_output = stm_display_get_output(platform_data[i].device, platform_data[i].hdmi_output_id);
    }

    stmcore_display_postinit(platform_data[i].display_runtime);

    /*
     * Finally setup the platform device. Note that if a platform driver has
     * already been registered then this will cause the driver probe function
     * to be called, so everything has to be setup before we do this.
     */
    if(stmcore_platformdev_init(i,&platform_data[i])<0)
    {
      printk(KERN_ERR "Failed to init platform device %d\n",i);
      continue;
    }

    nvaliddevices++;
  }

  if(nvaliddevices == 0)
  {
    printk(KERN_ERR "No valid display pipelines available\n");
    stmcore_display_exit();
    return -ENODEV;
  }

  return 0;
}


int stmcore_get_display_pipeline(int pipeline, struct stmcore_display_pipeline_data *data)
{
  if((pipeline < 0) || (pipeline >= nr_platform_devices))
    return -ENODEV;

  memcpy(data, &platform_data[pipeline], sizeof(struct stmcore_display_pipeline_data));

  return 0;
}


int stmcore_register_vsync_callback(struct stmcore_display *runtime, struct stmcore_vsync_cb *cb)
{
  unsigned long flags;

  BUG_ON(runtime == NULL);
  BUG_ON(cb->cb == NULL);

  if(cb->owner && !try_module_get(cb->owner))
    return -ENODEV;

  write_lock_irqsave (&runtime->vsync_cb_list.lock, flags);
  list_add_tail (&cb->node, &runtime->vsync_cb_list.list);
  write_unlock_irqrestore (&runtime->vsync_cb_list.lock, flags);

  return 0;
}


int stmcore_unregister_vsync_callback(struct stmcore_display *runtime, struct stmcore_vsync_cb *cb)
{
  struct stmcore_vsync_cb *pCallback, *tmp;
  unsigned long flags;
  int inList;

  BUG_ON(runtime == NULL);

  inList = 0;
  read_lock_irqsave (&runtime->vsync_cb_list.lock, flags);
  list_for_each_entry_safe (pCallback, tmp, &runtime->vsync_cb_list.list, node)
  {
    if(pCallback == cb)
    {
      inList = 1;
      break;
    }
  }
  read_unlock_irqrestore (&runtime->vsync_cb_list.lock, flags);

  BUG_ON(inList == 0);

  if(inList)
  {
    write_lock_irqsave (&runtime->vsync_cb_list.lock, flags);
    list_del_init (&cb->node);
    write_unlock_irqrestore (&runtime->vsync_cb_list.lock, flags);

    if(cb->owner)
      module_put(cb->owner);

    return 0;
  }

  return -EINVAL;
}


/******************************************************************************
 *  Modularization
 */

#ifdef MODULE

module_init(stmcore_display_init);
module_exit(stmcore_display_exit);

MODULE_LICENSE("GPL");

EXPORT_SYMBOL(stm_display_get_device);
EXPORT_SYMBOL(stmcore_get_display_pipeline);
EXPORT_SYMBOL(stmcore_register_vsync_callback);
EXPORT_SYMBOL(stmcore_unregister_vsync_callback);

#endif /* MODULE */
