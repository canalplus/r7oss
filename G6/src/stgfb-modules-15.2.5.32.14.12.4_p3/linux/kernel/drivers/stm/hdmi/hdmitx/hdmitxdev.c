/***********************************************************************
 *
 * File: linux/kernel/drivers/stm/hdmi/hdmitx/hdmitxdev.c
 * Copyright (c) 2005-2012 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#include <linux/version.h>
#include <linux/compiler.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/interrupt.h>
#include <linux/i2c.h>
#include <linux/kthread.h>
#include <linux/device.h>
#include <linux/cdev.h>
#include <linux/platform_device.h>
#include <linux/mutex.h>
#include <linux/wait.h>

#ifndef CONFIG_ARCH_STI
#include <linux/stm/pad.h>
#endif
#include <linux/gpio.h>

#include <asm/uaccess.h>
#include <linux/semaphore.h>

#include <stm_display.h>
#include <linux/stm/stmcoredisplay.h>
#include <linux/stm/hdmiplatform.h>
#include <linux/stm/stmcorehdmi.h>

#include "stmhdmi.h"

#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/err.h>

#include <vibe_debug.h>
#include <thread_vib_settings.h>

static dev_t firstdevice;
static char *hdmi0;


module_param(hdmi0, charp, 0444);
MODULE_PARM_DESC(hdmi0, "[enable|disable]");

static int thread_vib_hdmid[2] = { THREAD_VIB_HDMID_POLICY, THREAD_VIB_HDMID_PRIORITY };
module_param_array_named(thread_VIB_Hdmid,thread_vib_hdmid, int, NULL, 0644);
MODULE_PARM_DESC(thread_VIB_Hdmid, "VIB-Hdmid thread:s(Mode),p(Priority)");

struct of_device_id stmhdmi_match[] = {
	{ .compatible = "st,hdmi" },
	{},
};

extern int stmhdmi_manager(void *data);
extern long stmhdmi_ioctl(struct file *filp, unsigned int cmd, unsigned long arg);

static int stmhdmi_open(struct inode *inode,struct file *filp)
{
  struct stm_hdmi *dev = container_of(inode->i_cdev, struct stm_hdmi, cdev);
#ifdef CONFIG_PM_RUNTIME
  /*
   * DPM : Open required devices
   */
  mutex_lock(&dev->hdmilock);
  if(!dev->device)
  {
    if(stm_display_open_device(dev->display_device_id, &dev->device)<0)
    {
      mutex_unlock(&dev->hdmilock);
      printk(KERN_ERR "Unable to open the LINK %d\n",dev->display_device_id);
      return -ENODEV;
    }
  }
  dev->device_use_count++;
  mutex_unlock(&dev->hdmilock);
#endif
  filp->private_data = dev;
  return 0;
}

static int stmhdmi_release(struct inode *inode,struct file *filp)
{
#ifdef CONFIG_PM_RUNTIME
  struct stm_hdmi *hdmi = filp->private_data;
  /*
   * DPM : Close used devices
   */
  mutex_lock(&hdmi->hdmilock);
  if(hdmi->device && (hdmi->device_use_count > 0))
  {
    hdmi->device_use_count--;
    if((hdmi->device_use_count == 0)  && (hdmi->device_is_opened == 0))
    {
      stm_display_device_close(hdmi->device);
      hdmi->device = NULL;
    }
  }
  mutex_unlock(&hdmi->hdmilock);
#endif
  return 0;
}

/*
 * Reading from the HDMI device returns the full raw EDID data read from
 * the downstream device.
 */
static ssize_t stmhdmi_read(struct file *filp, char __user *buf, size_t sz, loff_t *off)
{
  struct stm_hdmi *dev = (struct stm_hdmi *)filp->private_data;
  ssize_t retval = 0;
  stm_event_subscription_h       subscription = NULL;
  stm_event_subscription_entry_t subscription_entry;
  stm_event_info_t               evt_info;
  unsigned int                   number_of_events;

  if(*off >= (1+dev->edid_info.base_raw[STM_EDID_EXTENSION])*sizeof(edid_block_t))
    return retval;

  if(*off + sz > (1+dev->edid_info.base_raw[STM_EDID_EXTENSION])*sizeof(edid_block_t))
    sz = (1+dev->edid_info.base_raw[STM_EDID_EXTENSION])*sizeof(edid_block_t) - *off;

  if(mutex_lock_interruptible(&dev->lock))
    return -ERESTARTSYS;

  if((filp->f_flags & O_NONBLOCK)&&(dev->edid_info.display_type == STM_DISPLAY_INVALID))
      retval = -EAGAIN;
  else if(dev->edid_info.display_type == STM_DISPLAY_INVALID)
  {
    DPRINTK("Create subscription : hdmi_object : %p\n", dev->hdmi_output);

    subscription_entry.cookie = NULL;
    subscription_entry.object = dev->hdmi_output;
    subscription_entry.event_mask = HDMI_EVENT_DISPLAY_CONNECTED;

    retval = stm_event_subscription_create(&subscription_entry, 1, &subscription);
    if(retval)
    {
      DPRINTK("Error can't create event subscription\n");
      goto evt_subs_failed;
    }

    retval = stm_event_set_wait_queue(subscription, &dev->status_wait_queue, true);
    if(retval)
    {
      DPRINTK("Unable to set wait queue for HDMI Tx events\n");
      goto evt_set_queue_failed;
    }

    mutex_unlock(&dev->lock);

    retval = stm_event_wait(subscription, -1, 1, &number_of_events, &evt_info);
    if(mutex_lock_interruptible(&dev->lock))
      return -ERESTARTSYS;
  }

  if(dev->edid_info.display_type != STM_DISPLAY_INVALID)
  {
    /* First check if we need data from the basic block */
    if (*off < sizeof(edid_block_t))
    {
      size_t base_sz = (*off + sz) < sizeof(edid_block_t)?sz : sizeof(edid_block_t) - *off;
      if(copy_to_user(buf,
                      dev->edid_info.base_raw + *off,
                      base_sz))
      {
        retval = -EFAULT;
      }
      else
      {
        *off += base_sz;
        buf += base_sz;
        retval += base_sz;
        sz -= base_sz;
      }
    }
    /*
     * Do we need data from the extension block
     * We already ensured at the beginning that we would not overflow
     */
    if (sz)
    {
      loff_t ext_off = *off - sizeof(edid_block_t);
      if(copy_to_user(buf,
                      *(dev->edid_info.extension_raw) + ext_off,
                      sz))
      {
        retval = -EFAULT;
      }
      else
      {
        *off += sz;
        retval += sz;
      }
    }
  }

evt_set_queue_failed:
  if(subscription)
  {
    if(stm_event_subscription_delete(subscription))
      DPRINTK("Failed to delete event subscription\n");
  }
evt_subs_failed:
  mutex_unlock(&dev->lock);
  return retval;
}


struct file_operations hdmi_fops = {
  .owner = THIS_MODULE,
  .open  = stmhdmi_open,
  .release = stmhdmi_release,
  .read  = stmhdmi_read,
  .unlocked_ioctl = stmhdmi_ioctl
};


/******************************************************************************/

static void stmhdmi_vsync_cb(stm_vsync_context_handle_t context, uint32_t timingevent)
{
  stm_display_output_connection_status_t  hdmi_status;
  struct stm_hdmi *hdmi_data = (struct stm_hdmi *)context;

  /*
  if((timingevent & (STM_TIMING_EVENT_TOP_FIELD|STM_TIMING_EVENT_FRAME)) == 0)
    return;
  */

  stm_display_output_get_connection_status(hdmi_data->hdmi_output, &hdmi_status);

  if((hdmi_status != hdmi_data->status) && ( hdmi_status!= STM_DISPLAY_DISCONNECTED))
  {
    unsigned long flags;
    hdmi_data->status         = hdmi_status;
    spin_lock_irqsave(&hdmi_data->spinlock, flags);
    hdmi_data->status_changed = 1;
    spin_unlock_irqrestore(&hdmi_data->spinlock, flags);
    wake_up(&hdmi_data->status_wait_queue);
   }
}


/******************************************************************************/

int stmhdmi_destroy(struct stm_hdmi *hdmi)
{
  struct stmcore_display_pipeline_data display_pipeline;

  if(!hdmi)
    return 0;

  /*
   * Terminate HDMI management thread
   */
  if(hdmi->thread != NULL)
    kthread_stop(hdmi->thread);

  hdmi->thread = NULL;

  if(stmcore_get_display_pipeline(hdmi->pipeline_id, &display_pipeline)<0)
  {
    printk(KERN_ERR "HDMI display pipeline (%u) not available, check your board setup\n",hdmi->pipeline_id);
    BUG();
    return -EINVAL;
  }
  /*
   * TODO: the register/unregister vsync callback routines should take the
   *       pipeline ID not an internal data structure pointer.
   */
  stmcore_unregister_vsync_callback(display_pipeline.display_runtime, &hdmi->vsync_cb_info);

  if(hdmi->class_device)
  {
    device_unregister(hdmi->class_device);
    hdmi->class_device = NULL;
  }

  cdev_del(&hdmi->cdev);

  if(hdmi->hdmi_output)
  {
    stm_display_output_close(hdmi->hdmi_output);
    hdmi->hdmi_output = NULL;
  }
  if(hdmi->main_output)
  {
    stm_display_output_close(hdmi->main_output);
    hdmi->main_output = NULL;
  }
  if(hdmi->link)
  {
    stm_display_link_close (hdmi->link);
    hdmi->link = NULL;
  }

  if(hdmi->device)
  {
    stm_display_device_close(hdmi->device);
    hdmi->device = NULL;
  }
  hdmi->device_use_count = 0;

#ifndef CONFIG_ARCH_STI
  if(hdmi->hotplug_pad != NULL)
  {
    if(hdmi->hotplug_gpio != STM_GPIO_INVALID)
      stm_pad_gpio_free(hdmi->hotplug_pad, hdmi->hotplug_gpio);

    stm_pad_release(hdmi->hotplug_pad);
    hdmi->hotplug_pad = NULL;
  }

#endif
  kfree(hdmi->spd_metadata);
  kfree(hdmi);

  return 0;
}


/****************************************************************************
 * Device initialization helpers
 */
static int __init stmhdmi_create_spd_metadata(struct stm_hdmi *hdmi)
{
  if((hdmi->spd_metadata = kzalloc(sizeof(stm_display_metadata_t)+sizeof(stm_hdmi_info_frame_t),GFP_KERNEL))==0)
  {
    DPRINTK("Cannot allocate SPD metadata\n");
    return -ENOMEM;
  }

  hdmi->spd_metadata->size = sizeof(stm_display_metadata_t)+sizeof(stm_hdmi_info_frame_t);
  hdmi->spd_metadata->type = STM_METADATA_TYPE_SPD_IFRAME;

  hdmi->spd_frame = (stm_hdmi_info_frame_t*)&hdmi->spd_metadata->data[0];
  hdmi->spd_frame->type    = HDMI_SPD_INFOFRAME_TYPE;
  hdmi->spd_frame->version = HDMI_SPD_INFOFRAME_VERSION;
  hdmi->spd_frame->length  = HDMI_SPD_INFOFRAME_LENGTH;
  strncpy(&hdmi->spd_frame->data[HDMI_SPD_INFOFRAME_VENDOR_START],"STMicro",HDMI_SPD_INFOFRAME_VENDOR_LENGTH);
  strncpy(&hdmi->spd_frame->data[HDMI_SPD_INFOFRAME_PRODUCT_START],"STLinux",HDMI_SPD_INFOFRAME_PRODUCT_LENGTH);
  hdmi->spd_frame->data[HDMI_SPD_INFOFRAME_SPI_OFFSET] = HDMI_SPD_INFOFRAME_SPI_PC;

  return 0;
}


static struct stm_hdmi * __init stmhdmi_create_hdmi_dev_struct(void)
{
  struct stm_hdmi *hdmi;
  hdmi = kzalloc(sizeof(struct stm_hdmi), GFP_KERNEL);
  if(!hdmi)
    return NULL;

  mutex_init(&(hdmi->lock));
  mutex_init(&(hdmi->hdmilock));
  spin_lock_init(&(hdmi->spinlock));
  init_waitqueue_head(&(hdmi->status_wait_queue));

  return hdmi;
}


static int __init stmhdmi_register_device(struct stm_hdmi *hdmi,
                                   int id,
                                   struct stmcore_display_pipeline_data *display_pipeline)
{
  cdev_init(&(hdmi->cdev),&hdmi_fops);
  hdmi->cdev.owner = THIS_MODULE;
  kobject_set_name(&(hdmi->cdev.kobj),"hdmi%d.0",id);

  if(cdev_add(&(hdmi->cdev),MKDEV(MAJOR(firstdevice),id),1))
    return -ENODEV;

  hdmi->class_device = device_create(display_pipeline->class_device->class,
                                     display_pipeline->class_device,
                                     hdmi->cdev.dev,
                                     hdmi,
                                     "%s", kobject_name(&hdmi->cdev.kobj));

  if(IS_ERR(hdmi->class_device))
  {
    hdmi->class_device = NULL;
    return -ENODEV;
  }

#if defined(SDK2_ENABLE_HDMI_TX_ATTRIBUTES)
  if(stmhdmi_create_class_device_files(hdmi, display_pipeline->class_device))
    return -ENODEV;
#endif

  return 0;
}

void *stmhdmi_get_device_tree_data(struct platform_device *pdev)
{
  struct device_node *hdmi_root_node_p = NULL;
  struct stm_hdmi_platform_data *hdmi_platform_data;
  pdev->id =0;

  hdmi_root_node_p = of_find_compatible_node(NULL, NULL, "st,hdmi");
  if ( !hdmi_root_node_p)
   return NULL;

  hdmi_platform_data = devm_kzalloc(&pdev->dev,sizeof(struct stm_hdmi_platform_data), GFP_KERNEL);
  if(!hdmi_platform_data)
  {
    printk("%s : Unable to allocate platform data\n", __FUNCTION__);
    return ERR_PTR(-ENOMEM);
  }

  of_property_read_u32(hdmi_root_node_p, "pipeline-id",  &(hdmi_platform_data->pipeline_id));
  hdmi_platform_data->device_id = of_alias_get_id(pdev->dev.of_node, "hdmi");

  return (void*)(hdmi_platform_data);
}

/****************************************************************************
 * Platform driver implementation
 */
#ifndef CONFIG_ARCH_STI
static int __init stmhdmi_probe(struct platform_device *pdev)
#else
static int stmhdmi_probe(struct platform_device *pdev)
#endif
{
  struct stm_hdmi_platform_data *platform_data;
  struct stmcore_display_pipeline_data display_pipeline;
  stm_display_link_type_t             link_type;
  stm_display_link_capability_t       link_capability;

#ifndef CONFIG_ARCH_STI
  struct stm_pad_state *hotplug_pad = NULL;
  uint32_t              hotplug_gpio = STM_GPIO_INVALID;
#endif

  struct stm_hdmi      *hdmi;
  char                 *paramstring;

  char               thread_name[14];
  int                sched_policy;
  struct sched_param sched_param;

  platform_data = (struct stm_hdmi_platform_data*)stmhdmi_get_device_tree_data(pdev);

  if(!platform_data)
  {
    printk(KERN_ERR "platform data pointer is NULL\n");
    return -EINVAL;
  }

  if(pdev->id != 0)
  {
    printk(KERN_ERR "only one HDMI TX output device supported\n");
    BUG();
    return -EINVAL;
  }

  if(stmcore_get_display_pipeline(platform_data->pipeline_id, &display_pipeline)<0)
  {
    printk(KERN_ERR "HDMI display pipeline (%u) not available, check your board setup\n",platform_data->pipeline_id);
    BUG();
    return -EINVAL;
  }

#ifndef CONFIG_ARCH_STI
  if (pdev->dev.of_node == NULL)
  {
    if(platform_data->hpd_pad_config.gpios_num > 0)
    {
      hotplug_pad = stm_pad_claim(&platform_data->hpd_pad_config,"HDMI Hotplug");
      if(hotplug_pad == NULL)
      {
        printk(KERN_WARNING "HDMI device cannot claim any HPD pads, hotplug may not work\n");
      }
      else if(platform_data->hpd_poll_pio)
      {
        hotplug_gpio = stm_pad_gpio_request_input(hotplug_pad,"HPD");
      }
    }
  }

#endif
  if((hdmi = stmhdmi_create_hdmi_dev_struct()) == NULL)
    return -ENOMEM;

  DPRINTK("new hdmi structure = %p\n",hdmi);

  hdmi->display_device_id = display_pipeline.device_id;
  if(stm_display_open_device(hdmi->display_device_id, &(hdmi->device))<0)
  {
    printk(KERN_ERR "HDMI cannot open display device = %u\n",platform_data->device_id);
    goto exit_nodev;
  }
  hdmi->device_use_count = 1;

  hdmi->pipeline_id  = platform_data->pipeline_id;
#ifndef CONFIG_ARCH_STI
  hdmi->hotplug_pad  = hotplug_pad;
  hdmi->hotplug_gpio = hotplug_gpio;
#endif
  hdmi->video_type   = (STM_VIDEO_OUT_RGB | STM_VIDEO_OUT_24BIT);

  switch(pdev->id)
  {
    case 0:
      paramstring = hdmi0;
      break;
    default:
      paramstring = NULL;
      break;
  }

  if(paramstring)
  {
    if(paramstring[0] == 'd' || paramstring[0] == 'D')
    {
      printk(KERN_WARNING "hdmi%d.0 is initially disabled, use 'stfbset -e hdmi' to enable it\n",pdev->id);
      hdmi->disable = 1;
    }
  }

  hdmi->stop_output = 1;
  hdmi->hdcp_enable = 0;
  hdmi->enc_enable  = 1;
  hdmi->hdcp_status = 0;
  hdmi->hdcp_stopped = 0;
  hdmi->edid_updated = 0;
  hdmi->current_hdcp_status =0;
  hdmi->max_pixel_repeat = 2;
  hdmi->non_strict_edid_semantics = 0;

  /*
   * Set the default CEA selection behaviour to use the aspect ratio in the EDID
   */
  hdmi->cea_mode_from_edid = 1;

  /*
   * Note that we are trusting the output identifiers are valid
   * and pointing to correct output types.
   */
  stm_display_device_open_output(hdmi->device, display_pipeline.main_output_id, &(hdmi->main_output));
  stm_display_device_open_output(hdmi->device, display_pipeline.hdmi_output_id, &(hdmi->hdmi_output));

  if(hdmi->main_output == NULL || hdmi->hdmi_output == NULL)
  {
    DPRINTK("Cannot get display outputs main = %d, hdmi = %d\n",display_pipeline.main_output_id,display_pipeline.hdmi_output_id);
    goto exit_nodev;
  }

  hdmi->hdmi_output_id = display_pipeline.hdmi_output_id;

  if(stm_display_output_get_capabilities(hdmi->hdmi_output, &hdmi->capabilities)<0)
  {
    DPRINTK("Cannot get hdmi output capabilities\n");
    goto exit_nodev;
  }

  if(!(hdmi->capabilities & OUTPUT_CAPS_HDMI))
  {
    printk(KERN_ERR "Provided HDMI output identifier doesn't support HDMI??\n");
    goto exit_nodev;
  }

  if(stmhdmi_create_spd_metadata(hdmi))
  {
    stmhdmi_destroy(hdmi);
    return -ENOMEM;
  }

  /*
   * If we split the HDMI management into another module then we should change
   * the owner field in the callback info to THIS_MODULE. However this is
   * linked into the coredisplay module at the moment we do not want to have
   * another reference to ourselves.
   */
  INIT_LIST_HEAD(&(hdmi->vsync_cb_info.node));
  hdmi->vsync_cb_info.owner   = NULL;
  hdmi->vsync_cb_info.context = hdmi;
  hdmi->vsync_cb_info.cb      = stmhdmi_vsync_cb;
  if(stmcore_register_vsync_callback(display_pipeline.display_runtime, &hdmi->vsync_cb_info)<0)
  {
    printk(KERN_ERR "Cannot register hdmi vsync callback\n");
    goto exit_nodev;
  }
  /*
   * Open display link connection
   */
  if ( stm_display_link_open (hdmi->pipeline_id, &hdmi->link)<0 )
  {
    printk(KERN_ERR "Unable to open the DISPLAY link %d\n",hdmi->pipeline_id);
    goto exit_nodev;
  }

  /*
   * Get display link type
   */
  stm_display_link_get_type(hdmi->link, &link_type);
  if ( link_type != STM_DISPLAY_LINK_TYPE_HDMI )
  {
    printk(KERN_ERR "Display link is not HDMI type\n");
    goto exit_nodev;
  }

  /*
   * Check display link capability
   */
  if ( stm_display_link_get_capability (hdmi->link, &link_capability)<0 )
  {
    printk(KERN_ERR "Display link is not HDMI type\n");
    goto exit_nodev;
  }

  if (link_capability.rxsense)
  {
    if (stm_display_link_set_ctrl(hdmi->link, STM_DISPLAY_LINK_CTRL_RXSENSE_ENABLE, 1)<0)
    {
      printk(KERN_ERR "Can't enable rxsense\n");
      goto exit_nodev;
    }
  }
  hdmi->link_capability = link_capability;

  /* Create the thread */
  snprintf(thread_name, sizeof(thread_name), "VIB-Hdmid/%d", pdev->id);
  hdmi->thread = kthread_create(stmhdmi_manager, hdmi, thread_name);
  if (hdmi->thread == NULL)
  {
    printk(KERN_ERR "Cannot start hdmi thread id = %d\n",pdev->id);
    stmhdmi_destroy(hdmi);
    return -ENOMEM;
  }

  /* Set thread scheduling settings */
  sched_policy               = thread_vib_hdmid[0];
  sched_param.sched_priority = thread_vib_hdmid[1];
  if ( sched_setscheduler(hdmi->thread, sched_policy, &sched_param) )
  {
    TRC(TRC_ID_ERROR, "FAILED to set thread scheduling parameters: name=%s, policy=%d, priority=%d", \
        thread_name, sched_policy,sched_param.sched_priority);
  }

  if(stmhdmi_register_device(hdmi, pdev->id, &display_pipeline))
  {
    goto exit_nodev;
  }

  /* Wake up the thread */
  wake_up_process(hdmi->thread);

  platform_set_drvdata(pdev,hdmi);

  /* Set wake-up capability for hdmi through sysfs*/
  device_set_wakeup_capable(&pdev->dev,true);

  stm_display_device_close(hdmi->device);
  hdmi->device = NULL;
  hdmi->device_use_count = 0;
  return 0;

exit_nodev:
  stmhdmi_destroy(hdmi);
  return -ENODEV;
}


static int __exit stmhdmi_remove(struct platform_device *pdev)
{
  struct stm_hdmi *i = (struct stm_hdmi *)platform_get_drvdata(pdev);

  DPRINTK("\n");

  if(!i)
    return 0;

  /*
   * Ensure display device is opened before closing its ressources
   */
  if(!i->device)
  {
    if(stm_display_open_device(i->display_device_id, &i->device)<0)
    {
      printk(KERN_ERR "Unable to open the DISPLAY %d\n",i->display_device_id);
      i->device = NULL;
      return -ENODEV;
    }
  }

  stmhdmi_destroy(i);

  platform_set_drvdata(pdev,NULL);

  return 0;
}


static void stmhdmi_shutdown(struct platform_device *pdev)
{
  return;
}

static struct platform_driver stmhdmi_driver = {
   .probe    = stmhdmi_probe,
   .shutdown = stmhdmi_shutdown,
   .remove   = __exit_p(stmhdmi_remove),
   .driver   = {
       .name     = "hdmi",
       .owner    = THIS_MODULE
   },
   .driver.of_match_table = of_match_ptr(stmhdmi_match),
};


static void __exit stmhdmi_exit(void)
{
  platform_driver_unregister(&stmhdmi_driver);
  unregister_chrdev_region(firstdevice,1);
}


static int __init stmhdmi_init(void)
{
  if(alloc_chrdev_region(&firstdevice,0,1,"hdmi")<0)
  {
    printk(KERN_ERR "%s: unable to allocate device numbers\n",__FUNCTION__);
    return -ENODEV;
  }

  platform_driver_register(&stmhdmi_driver);

  return 0;
}


/******************************************************************************
 *  Modularization
 */

module_init(stmhdmi_init);
module_exit(stmhdmi_exit);

MODULE_LICENSE("GPL");
