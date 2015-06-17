/***********************************************************************
 *
 * File: linux/kernel/drivers/stm/displaylink/linkmodule.c
 * Copyright (c) 2012 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/kthread.h>
#include <linux/interrupt.h>
#include <linux/wait.h>
#include <linux/string.h>
#include <linux/gpio.h>
#include <linux/pm.h>
#include <linux/pm_runtime.h>
#include <linux/ktime.h>

#include <asm/uaccess.h>
#include <asm/irq.h>
#include <linux/socket.h>
#include <linux/netlink.h>
#include <linux/connector.h>
#include <net/sock.h>

#include <stm_display.h>
#include <stm_display_link.h>
#include <vibe_os.h>
#include <vibe_debug.h>
#include <linux/stm/stmcoredisplay.h>
#include <linux/stm/stmcorelink.h>
#include <linux/stm/stmcorehdmi.h>

#ifndef CONFIG_ARCH_STI
#include <linux/stm/pad.h>
#endif
#include <linux/stm/hdmiplatform.h>
#include <linux/stm/linkplatform.h>

#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/of_platform.h>
#include <linux/err.h>

#include <thread_vib_settings.h>

static dev_t firstdevice;
extern struct file_operations link_fops;

static int thread_vib_hdp[2] = { THREAD_VIB_HDP_POLICY, THREAD_VIB_HDP_PRIORITY };
module_param_array_named(thread_VIB_Hdp,thread_vib_hdp, int, NULL, 0644);
MODULE_PARM_DESC(thread_VIB_Hdp, "VIB-Hdp thread:s(Mode),p(Priority)");

#define I2C_EDID_ADDR             0x50
#define I2C_EDDC_SEGMENT_REG_ADDR 0x30
extern int send_event (struct stmlink* link, unsigned int type);

struct of_device_id stmlink_match[] = {
	{ .compatible = "st,displaylink" },
	{},
};

/***********************************************************************
 * I2C management routines for EDID devices
 */
#ifndef CONFIG_ARCH_STI
static int __init stmlink_i2c_probe(struct i2c_client *client,
    const struct i2c_device_id *id)
#else
static int stmlink_i2c_probe(struct i2c_client *client,
    const struct i2c_device_id *id)
#endif
{
  /* Normally we would do:
     i2c_set_clientdata(client, hdmi?)
   */

  LINKDBG(3,"");

  return 0;
}

static int __exit stmlink_i2c_remove(struct i2c_client *client)
{
  LINKDBG(3,"");

  return 0;
}

static const struct i2c_device_id stmlink_id[] = {
    { "displaylink-eddc", 0 },
    { "displaylink-edid", 0 },
    { }
};
MODULE_DEVICE_TABLE(i2c, stmlink_id);

static struct i2c_driver stmlink_i2c_driver = {
    .driver         = {
        .name           = "displaylink"
    },
    .probe = stmlink_i2c_probe,
    .remove = __exit_p(stmlink_i2c_remove),
    .id_table = stmlink_id,
};

/******************************************************************************
 * Board level registration.
 */

static struct i2c_board_info stmlink_eddc_info = {
    I2C_BOARD_INFO("stmlink", I2C_EDDC_SEGMENT_REG_ADDR),
    .type = "displaylink-eddc",
    .platform_data = 0,
};

static struct i2c_board_info stmlink_edid_info = {
    I2C_BOARD_INFO("stmlink", I2C_EDID_ADDR),
    .type = "displaylink-edid",
    .platform_data = 0,
};

/**
 *      stmlink_i2c_connect - Connect the I2C client
 *      @link: Pointer to the stmlink structure to save I2C client
 *
 */
static void stmlink_i2c_connect(struct stmlink *linkp)
{
  DPRINTK("Creating I2C devices\n");

  linkp->eddc_segment_reg_client = i2c_new_device(linkp->i2c_adapter,
      &stmlink_eddc_info);
  linkp->edid_client = i2c_new_device(linkp->i2c_adapter,
      &stmlink_edid_info);
}

/**
 *      stmlink_i2c_disconnect - Disconnect the I2C client
 *      @link: Pointer to the stmlink structure to remove I2C client
 *
 */
static void stmlink_i2c_disconnect(struct stmlink *linkp)
{
  if(linkp->edid_client)
    {
      i2c_unregister_device(linkp->edid_client);
      linkp->edid_client = 0;
    }
  if(linkp->eddc_segment_reg_client)
    {
      i2c_unregister_device(linkp->eddc_segment_reg_client);
      linkp->eddc_segment_reg_client = 0;
    }
}
/****************************************************************************
* Device initialization helpers
*/
static int __init stmlink_register_device(struct stmlink *linkp,
                                   int id,
                                   struct stmcore_display_pipeline_data *display_pipeline)
{
  cdev_init(&(linkp->cdev),&link_fops);
  linkp->cdev.owner = THIS_MODULE;
  kobject_set_name(&(linkp->cdev.kobj),"hdcp%d.0",id);

  if(cdev_add(&(linkp->cdev),MKDEV(MAJOR(firstdevice),id),1))
    return -ENODEV;

  linkp->class_device = device_create(display_pipeline->class_device->class,
                                     display_pipeline->class_device,
                                     linkp->cdev.dev,
                                     linkp,
                                     "%s", kobject_name(&linkp->cdev.kobj));

  if(IS_ERR(linkp->class_device))
  {
    linkp->class_device = NULL;
    return -ENODEV;
  }

#if defined(SDK2_ENABLE_HDMI_TX_ATTRIBUTES)
  if(stmlink_create_class_device_files(linkp, display_pipeline->class_device))
    return -ENODEV;
#endif

  return 0;
}
/******************************************************************************/
/**
 * stmlink_wait_for_vsync - wait for specified number of vsync's.
 *
 * @hdcp: desired hdcp instance.
 */
int stmlink_wait_for_vsync(struct stmlink* linkp)
{
    return (wait_event_interruptible(linkp->wait_q,
                     (linkp->vsync_wait_count == 0)));
}

/**
 * stmhdcp_vsync_cb - callback function registered with stmcore.
 *
 * @context: desired hdcp instance.
 * @timingevent: display timing event.
 */
static void stmlink_vsync_cb(stm_vsync_context_handle_t context, uint32_t timingevent)
{

    struct stmlink* linkp = (struct stmlink *)context;

    if (linkp->vsync_wait_count > 0) {
        linkp->vsync_wait_count--;

        if (linkp->vsync_wait_count == 0)
            wake_up_interruptible(&linkp->wait_q);
    }
}
/******************************************************************************
 * Link modules implementation
 */

MODULE_LICENSE("GPL");
MODULE_AUTHOR("STMicroelectronics");
MODULE_DESCRIPTION("The link driver managing HDMI or Display Port");

static int link_count = 1;
#ifdef CONFIG_PM
static bool link_pm_suspend = false;
#endif
static char *key_values = NULL;
static char *ksv_values = NULL;
static char *vector_values = NULL;

static char cn_link_name[] = "cn_link";

module_param_named(count, link_count, int, 0664);
module_param_named(keys, key_values, charp, 0664);
module_param_named(vectors, vector_values, charp, 0664);
module_param_named(ksvs, ksv_values, charp, 0664);

MODULE_PARM_DESC(count, "Number of Link Device");
MODULE_PARM_DESC(keys, "HDCP Keys file");
MODULE_PARM_DESC(vectors, "HDCP Vectors file");
MODULE_PARM_DESC(ksvs, "HDCP Public Key file");

static struct stmlink *links;

extern int hdcp_authentication(void *data);
extern int hpd_manager(void *data);
extern irqreturn_t stmlink_interrupt(int irq, void* data);
extern stm_time64_t GetSystemTime(void);
extern void cn_link_callback(struct cn_msg *msg, struct netlink_skb_parms *nsp);

/**
 *      link_create - Initialization of the stmlink struct
 *      @linkp: Pointer to the stmlink structure to initialize
 *
 *      RETURNS:
 *              0:       No error
 */
int link_create(struct stmlink *linkp)
{
  memset(linkp, 0, sizeof(*linkp));

  mutex_init(&(linkp->lock));
  init_waitqueue_head(&(linkp->wait_queue));
  init_waitqueue_head(&linkp->wait_q);
  linkp->type = STM_DISPLAY_LINK_TYPE_HDMI;
  linkp->hpd_state = STM_DISPLAY_LINK_HPD_STATE_LOW;
  linkp->hpd_new = STM_DISPLAY_LINK_HPD_STATE_LOW;
  linkp->rxsense_state = STM_DISPLAY_LINK_RXSENSE_STATE_INACTIVE;
  linkp->rxsense_new = STM_DISPLAY_LINK_RXSENSE_STATE_INACTIVE;
  linkp->status = STM_DISPLAY_LINK_HDCP_STATUS_DISABLED;
  linkp->hdcp = false;
  linkp->irq = -1;
  linkp->frame_encryption = true;
  linkp->hpd_missed = false;
  linkp->rxsense_missed = false;
  linkp->i2c_retry = 5;
  linkp->last_transaction = STM_DISPLAY_LINK_HDCP_TRANSACTION_NONE;
  linkp->enhanced_link_check = false;
  linkp->advanced_cipher_mode = false;
  linkp->thread = NULL;
  linkp->hdcp_frame_counter = 0;
  linkp->link_check_by_frame = true;
  linkp->revoked_ksv = 0;
  linkp->hdcp_mode = HDCP_NVS_MODE;
  linkp->display_mode = DISPLAY_MODE_INVALID;
  linkp->key_read = false;
  linkp->hdcp_delay_ms = 0;
  linkp->retry_interval = 2;
  linkp->restart_authentication = false;
  linkp->hpd_check_needed =false;
  linkp->hdcp_start=false;
  linkp->hdcp_start_needed=false;
  linkp->link_id.idx = 11;
  linkp->link_id.val  = 0x456;
  linkp->error_daemon =0;
  memset(&(linkp->bcaps), 0, HDCP_BCAPS_SIZE);
  memset(&(linkp->bstatus), 0, HDCP_BSTATUS_SIZE);
  linkp->semlock  = vibe_os_create_semaphore(0);
  memset(linkp->ksv_list, 0, sizeof(linkp->ksv_list));
  mutex_init(&(linkp->proxy_lock));
  linkp->pdev = NULL;
  linkp->display_device_id = -1;
  linkp->use_count = 0;
  return 0;
}



/**
 *      start_irq_handler - Start irq handler to get HPD
 *      @linkp: Pointer to the stmlink
 *
 *      RETURNS:
 *              0:       No error
 */
int start_irq_handler (struct stmlink* linkp) {
  LINKDBG(2, "Starting IRQ handler");

  if(!linkp)
    return -EINVAL;

  if (STM_VIBE_USE_THREADED_IRQ)
  {
    if(request_threaded_irq(linkp->irq, NULL, stmlink_interrupt, IRQF_DISABLED | IRQF_ONESHOT, "displaylink", linkp)) {
      LINKDBG(1, "Cannot get HDMI irq = %d\n",linkp->irq);
      return -EFAULT;
    }
  }
  else
  {
    if(request_irq(linkp->irq, stmlink_interrupt, IRQF_DISABLED, "displaylink", linkp)) {
      LINKDBG(1, "Cannot get HDMI irq = %d\n",linkp->irq);
      return -EFAULT;
    }
  }

  return 0;
}

/**
 *      stop_irq_handler - Stop irq handler that gets HPD
 *      @linkp: Pointer to the stmlink
 *
 *      RETURNS:
 *              0:       No error
 */
int stop_irq_handler (struct stmlink* linkp) {
  LINKDBG(2, "Stopping IRQ handler");

  if(!linkp)
    return -EINVAL;

  if(linkp->irq != -1)
    free_irq(linkp->irq, linkp);

  return 0;
}
#ifdef CONFIG_PM
static int stmlink_set_pm(struct device *dev, pm_message_t message)
{
  struct stmlink* linkp = dev_get_drvdata(dev);
  LINKDBG(2, "Event = %d",message.event);

  if (linkp == NULL)
    return -EINVAL;

  switch (message.event) {
  case PM_EVENT_SUSPEND:
  case PM_EVENT_FREEZE:
   LINKDBG(2, "Link is %s...\n", (message.event == PM_EVENT_SUSPEND) ? "Suspending" : "Freezing");
   link_pm_suspend = true;
   if (!device_can_wakeup(dev))
      disable_irq(linkp->irq);
  /*
   * displaylink depend on coredisplay but we don't
   * really need to open/close device in this stage.
   */
   break;

  case PM_EVENT_RESUME:
  case PM_EVENT_RESTORE:
    if (link_pm_suspend == true)
    {
      LINKDBG(2, "Link is %s...\n", (message.event == PM_EVENT_RESUME) ? "Resuming" : "Restoring");
      /*
       * displaylink depend on coredisplay but we don't
       * really need to open/close device in this stage.
       */
      if (!device_can_wakeup(dev))
        enable_irq(linkp->irq);
      linkp->hpd_state = STM_DISPLAY_LINK_HPD_STATE_HIGH;
      if (send_event(linkp, STM_DISPLAY_LINK_HPD_STATE_CHANGE_EVT) != 0)
      {
        LINKDBG(8,"An error occured in HPD state change event");
      }
      linkp->hpd_missed = true;
    }
      break;

  case PM_EVENT_HIBERNATE:
    break;
  case PM_EVENT_RECOVER:
    break;
  default:
    break;
  }
  return 0;
}

static int stmlink_pm_suspend(struct device *dev) {
  return stmlink_set_pm(dev, PMSG_SUSPEND);
}
static int stmlink_pm_resume(struct device *dev) {
  return stmlink_set_pm(dev, PMSG_RESUME);
}
static int stmlink_pm_freeze(struct device *dev) {
  return stmlink_set_pm(dev, PMSG_FREEZE);
}
static int stmlink_pm_restore(struct device *dev) {
  return stmlink_set_pm(dev, PMSG_RESTORE);
}
#ifdef CONFIG_PM_RUNTIME
static int stmlink_pm_runtime_suspend(struct device *dev) {
  return stmlink_set_pm(dev, PMSG_SUSPEND);
}
static int stmlink_pm_runtime_resume(struct device *dev) {
  return stmlink_set_pm(dev, PMSG_RESUME);
}
#endif
static struct dev_pm_ops stmlink_pm_ops = {
    .suspend = stmlink_pm_suspend,
    .resume = stmlink_pm_resume,
    .freeze = stmlink_pm_freeze,
    .restore = stmlink_pm_restore,
#ifdef CONFIG_PM_RUNTIME
    .runtime_suspend = stmlink_pm_runtime_suspend,
    .runtime_resume  = stmlink_pm_runtime_resume,
#endif
};
#endif

int start_netlink(struct stmlink* linkp)
{
  int err;
  LINKDBG(3,"");

  if(!linkp)
   return -EINVAL;

  err = cn_add_callback(&linkp->link_id, cn_link_name, cn_link_callback);
  if (err)
    {
      LINKDBG(1,"Unable to create connector callback");
      return -EINVAL;
    }

  LINKDBG(1, "Netlink initialized with id={%u.%u}",linkp->link_id.idx, linkp->link_id.val);
  return 0;
}

int stop_netlink(struct stmlink* linkp)
{
  LINKDBG(3,"");
  if(!linkp)
    return -EINVAL;

  cn_del_callback(&linkp->link_id);

  return 0;
}

/**
 *      link_del - Delete the stmlink struct
 *      @linkp: Pointer to the stmlink structure to delete
 *
 */
void link_del(struct stmlink* linkp) {

  struct stmcore_display_pipeline_data display_pipeline;
  if(!linkp)
    return;

  stop_irq_handler(linkp);
  vibe_os_delete_semaphore(linkp->semlock);

  if(stmcore_get_display_pipeline(linkp->pipeline_id, &display_pipeline) < 0)
  {
    LINKDBG(1,"HDMI display pipeline (%u) not available, check your board setup",linkp->pipeline_id);
    BUG();
    return;
  }

 /*
  * TODO: the register/unregister vsync callback routines should take the
  *       pipeline ID not an internal data structure pointer.
  */
  stmcore_unregister_vsync_callback(display_pipeline.display_runtime, &linkp->vsync_cb_info);

  if(linkp->class_device)
  {
    device_unregister(linkp->class_device);
    linkp->class_device = NULL;
  }

  cdev_del(&linkp->cdev);

  stop_netlink(linkp);

  stmlink_i2c_disconnect(linkp);
  i2c_del_driver(&stmlink_i2c_driver);

  if(linkp->hdmi_output)
  {
    stm_display_output_close(linkp->hdmi_output);
    linkp->hdmi_output = NULL;
  }

  if(linkp->main_output)
  {
    stm_display_output_close(linkp->main_output);
    linkp->main_output = NULL;
  }

  if(linkp->link)
  {
    stm_display_link_close(linkp->link);
    linkp->link =NULL;
  }

  if(linkp->device)
  {
    stm_display_device_close(linkp->device);
    linkp->device = NULL;
  }

  kfree(linkp);
}

static void *stmlink_get_device_tree_data(struct platform_device *pdev)
{
  struct device_node *link_root_node_p = NULL;
  struct stm_link_platform_data *platform_link_data;

  pdev->id = 0;
  link_root_node_p = pdev->dev.of_node;
  platform_link_data = devm_kzalloc(&pdev->dev,sizeof(struct stm_link_platform_data), GFP_KERNEL);
  if(!platform_link_data)
  {
    printk("%s : Unable to allocate link platform data\n", __FUNCTION__);
    return ERR_PTR(-ENOMEM);
  }

  of_property_read_u32(link_root_node_p,"pipeline-id",&(platform_link_data->pipeline_id));
  platform_link_data->i2c_adapter_id = of_alias_get_id(of_get_parent(pdev->dev.of_node), "i2c");
  platform_link_data->rxsense_support = of_property_read_bool(link_root_node_p,"rxsense-support");

  return (void*)(platform_link_data);
}

/****************************************************************************
 * Platform driver implementation
 */

#ifndef CONFIG_ARCH_STI
static int __init stmlink_probe(struct platform_device *pdev)
#else
static int stmlink_probe(struct platform_device *pdev)
#endif
{
  struct stm_link_platform_data *platform_link_data;
  struct stmcore_display_pipeline_data display_pipeline;
  struct i2c_adapter   *i2c;
  int                   irq;
  struct stmlink*       linkp ;
  char                  thread_name[14];
  int                   sched_policy;
  struct sched_param    sched_param;

  struct resource res_reg;
  struct device_node *hdmi_root_node_p = NULL;
  hdmi_root_node_p = of_parse_phandle(pdev->dev.of_node, "hdmi-dev",0);
  platform_link_data = (struct stm_link_platform_data*)stmlink_get_device_tree_data(pdev);

  if(pdev->id >= link_count)
  {
    LINKDBG(1,"ID is greater than the max link number authorized");
    BUG();
    return -EINVAL;
  }

  linkp = links + pdev->id;
  if (link_create(linkp) < 0)
  {
    LINKDBG(1,"Can't create the link struct");
    BUG();
    return -EINVAL;
  }
  linkp->pdev      = pdev;
  linkp->device_id = pdev->id;

  if(!platform_link_data)
  {
    LINKDBG(1,"platform data pointer is NULL");
    BUG();
    return -EINVAL;
  }

  irq = irq_of_parse_and_map(hdmi_root_node_p, 0);
  if(irq<0)
  {
    LINKDBG(1,"LINK cannot find IRQ in platform resources");
    BUG();
    return -EINVAL;
  }

  if(stmcore_get_display_pipeline(platform_link_data->pipeline_id, &display_pipeline)<0)
  {
    LINKDBG(1,"HDMI display pipeline (%u) not available, check your board setup",platform_link_data->pipeline_id);
    BUG();
    return -EINVAL;
  }
  linkp->display_device_id = display_pipeline.device_id;

  i2c = i2c_get_adapter(platform_link_data->i2c_adapter_id);

  if(!i2c)
  {
    LINKDBG(1,"HDMI i2c bus (%d) not available, check your kernel configuration and board setup",platform_link_data->i2c_adapter_id);
    BUG();
    return -EINVAL;
  }

  if(of_address_to_resource(hdmi_root_node_p, 0,&res_reg)<0)
  {
    LINKDBG(1,"LINK cannot find REG in platform resources");
    BUG();
    return -EINVAL;
  }
  linkp->hdmi_offset  = ioremap_nocache(res_reg.start, (res_reg.end-res_reg.start+1));

  device_set_wakeup_capable(&pdev->dev, 1);
  pm_runtime_set_active(&pdev->dev);
  pm_runtime_enable(&pdev->dev);

  if(stm_display_open_device(linkp->display_device_id, &(linkp->device))<0)
  {
    LINKDBG(1,"LINK cannot open display device = %u",linkp->display_device_id);
    linkp->device = NULL;
    goto exit_nodev;
  }

  linkp->pipeline_id  = platform_link_data->pipeline_id;
  linkp->i2c_adapter  = i2c;
  linkp->capability.rxsense = platform_link_data->rxsense_support;
  linkp->rxsense = platform_link_data->rxsense_support;

  stm_display_device_open_output(linkp->device, display_pipeline.main_output_id, &(linkp->main_output));
  stm_display_device_open_output(linkp->device, display_pipeline.hdmi_output_id, &(linkp->hdmi_output));

  if ( stm_display_link_open (platform_link_data->pipeline_id, &linkp->link)<0 )
  {
    LINKDBG(1, "Unable to open the DISPLAY link %d\n",platform_link_data->pipeline_id);
    goto exit_nodev;
  }

  /* Create the thread */
  snprintf(thread_name, sizeof(thread_name), "VIB-Hpd/%d", linkp->device_id );
  linkp->hpd_thread = kthread_create(hpd_manager, linkp, thread_name);
  if (linkp->hpd_thread == NULL)
  {
    LINKDBG(1, "Cannot start link hpd thread id = %d",linkp->device_id);
    goto exit_nodev;
  }

  /* Set thread scheduling settings */
  sched_policy               = thread_vib_hdp[0];
  sched_param.sched_priority = thread_vib_hdp[1];
  if ( sched_setscheduler(linkp->hpd_thread, sched_policy, &sched_param) )
  {
    TRC(TRC_ID_ERROR, "FAILED to set thread scheduling parameters: name=%s, policy=%d, priority=%d", \
        thread_name, sched_policy, sched_param.sched_priority);
    goto exit_nodev;
  }

  /* Wake up the thread */
  wake_up_process(linkp->hpd_thread);

  if(linkp->main_output == NULL || linkp->hdmi_output == NULL)
  {
    LINKDBG(1,"Cannot get display outputs main = %d, hdmi = %d",display_pipeline.main_output_id,display_pipeline.hdmi_output_id);
    goto exit_nodev;
  }

  if(i2c_add_driver(&stmlink_i2c_driver))
  {
    LINKDBG(1,"Registering I2C driver failed");
    goto exit_nodev;
  }
  INIT_LIST_HEAD(&(linkp->vsync_cb_info.node));
  linkp->vsync_cb_info.owner   = NULL;
  linkp->vsync_cb_info.context = linkp;
  linkp->vsync_cb_info.cb      = stmlink_vsync_cb;
  if(stmcore_register_vsync_callback(display_pipeline.display_runtime, &linkp->vsync_cb_info)<0)
  {
    printk(KERN_ERR "Cannot register link vsync callback\n");
    goto exit_nodev;
  }

  /*
   * This does not probe the required I2C addresses to avoid confusing devices
   * with the SMBUS QUICK style transaction. Instead it creates I2C clients
   * assuming they exist, which as we only use them when a hotplug is detected
   * should be the case. Therefore we can do this once instead of connecting
   * and disconnecting on hotplug events.
   */
  stmlink_i2c_connect(linkp);

  start_netlink(linkp);

  if(stmlink_register_device(linkp, pdev->id, &display_pipeline))
  {
    goto exit_nodev;
  }
  platform_set_drvdata(pdev,linkp);

  linkp->irq = irq;
  if (start_irq_handler(linkp) < 0)
    goto exit_nodev;

  // Call interrupt handler the first time to update the HPD state
  stmlink_interrupt(linkp->irq, linkp);

  pm_runtime_suspend(&pdev->dev);

  /*
   * close device before return
   */
  if(linkp->device)
  {
    stm_display_device_close(linkp->device);
    linkp->device = NULL;
  }

  return 0;

  exit_nodev:
  pm_runtime_disable(&pdev->dev);
  link_del(linkp);
  return -ENODEV;
}


static int __exit stmlink_remove(struct platform_device *pdev)
{
  struct stmlink* linkp = (struct stmlink *)platform_get_drvdata(pdev);

  LINKDBG(3,"");

  if(!linkp)
    return 0;

  if(!linkp->device)
  {
    if(stm_display_open_device(linkp->display_device_id, &(linkp->device))<0)
    {
      printk(KERN_ERR "unable to open display device\n");
      return -ENODEV;
    }
  }

  if(linkp->hpd_thread != NULL)
    kthread_stop(linkp->hpd_thread);

  linkp->hpd_thread = NULL;

  link_del(linkp);

  platform_set_drvdata(pdev,NULL);

  return 0;
}


static void stmlink_shutdown(struct platform_device *pdev)
{
  /*
   * Missing pm_runtime_disable call in driver remove path caused
   * an "Unbalanaced pm_runtime_enable" warning when driver is reloaded.
   */
  pm_runtime_disable(&pdev->dev);
}

static struct platform_driver stmlink_driver = {
    .probe    = stmlink_probe,
    .shutdown = stmlink_shutdown,
    .remove   = __exit_p(stmlink_remove),
    .driver   = {
        .name     = "displaylink",
        .owner    = THIS_MODULE,
#ifdef CONFIG_PM
        .pm = &stmlink_pm_ops,
#endif
    },
   .driver.of_match_table = of_match_ptr(stmlink_match),
};

/**
 *      stmlink_init - Initialization of the displaylink module
 *
 *      RETURNS:
 *              0:       No error
 *              -ENOMEM  Memory issue
 */
int __init stmlink_init(void) {
  links = kzalloc(sizeof(struct stmlink)*link_count, GFP_KERNEL);
  if (links == 0) {
      return -ENOMEM;
  }
  if(alloc_chrdev_region(&firstdevice,0,1,"hdcp")<0)
  {
    printk(KERN_ERR "%s: unable to allocate device numbers\n",__FUNCTION__);
    return -ENODEV;
  }
  platform_driver_register(&stmlink_driver);
  LINKDBG(2,"link module successfully loaded");

  return 0;
}

/**
 *      stmlink_exit - Destroy the displaylink module
 *
 *      RETURNS:
 *              0:       No error
 *              -ENOMEM  Memory issue
 */
void __exit stmlink_exit(void) {

  platform_driver_unregister(&stmlink_driver);
  unregister_chrdev_region(firstdevice,1);
  LINKDBG(2,"link module successfully unloaded");
}

module_init(stmlink_init);
module_exit(stmlink_exit);

/**
 *      stm_display_link_open - Displaylink API,Open the link corresponding to the identifier
 *      @id: Link ID to open
 *      @link: Pointer to store the link handle opened
 *
 *      RETURNS:
 *              0:       No error
 *              -ENODEV  Invalid link handle
 */
int stm_display_link_open  ( uint32_t id , stm_display_link_h * link) {
  struct stmlink* linkp;

  LINKDBG(3,"");

  if (id >= link_count)
    return -ENODEV;

  linkp = links + id;
  linkp->use_count++;

  if(linkp->use_count > 1)
    pm_runtime_get_sync(&linkp->pdev->dev);

  *link = (stm_display_link_h) linkp ;

  return 0;
}

/**
 *      stm_display_link_close - Displaylink API, Close the link pointed by the handle
 *      @link: Link handle
 *
 *      RETURNS:
 *              0:       No error
 *              -EINVAL  Invalid link handle
 */
int stm_display_link_close  ( stm_display_link_h link ) {
  struct stmlink* linkp;

  LINKDBG(3,"");

  linkp = (struct stmlink *)link ;

  if ((linkp == NULL) || (linkp->use_count == 0))
    return -EINVAL;

  if(linkp->use_count > 1)
    pm_runtime_put_sync(&linkp->pdev->dev);

  linkp->use_count--;

  return 0;
}

EXPORT_SYMBOL(stm_display_link_open);
EXPORT_SYMBOL(stm_display_link_close);
