/***********************************************************************
 *
 * File: linux/kernel/drivers/stm/pixel_capture/capture_module.c
 * Copyright (c) 2012 STMicroelectronics Limited.
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
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/err.h>
#include <linux/workqueue.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/pm.h>
#include <linux/pm_runtime.h>
#include <linux/ktime.h>
#include <linux/io.h>

#include <linux/delay.h>
#include <stm_pixel_capture.h>
#include <linux/of_device.h>

#include "capture.h"

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

extern int __init stm_capture_probe_devices(void);
extern void stm_capture_cleanup_devices(void);
extern int __init stm_pixel_capture_device_init(stm_pixel_capture_device_type_t type,uint32_t addr,uint32_t size,stm_pixel_capture_hw_features_t hw_features);

struct stm_capture_sync_context {
  int                        pipeline_id;
  struct stmcore_vsync_cb    vsync_cb_info;
  struct stm_capture_config *cap_cfg;
  struct work_struct         cap_work;
  uint32_t                   timingevent;
  struct platform_device    *pdev;
  stm_pixel_capture_time_t   vsyncTime;
};

#define CAPTURE_TOTAL_INSTANCES_NUMBER  (STM_CAPTURE_MAX_INSTANCES*STM_CAPTURE_MAX_CALLBACKS)
static struct stm_capture_sync_context capture_contexts[STM_CAPTURE_MAX_DEVICES][CAPTURE_TOTAL_INSTANCES_NUMBER];

struct device_attribute capture_attrs[] = {
  /* TODO : add more attributes for capture */
  __ATTR_NULL
};

struct of_device_id stm_capture_match[] = {
  {
    .compatible = "st,dvp-v1.6",
    .data = (void *)&(struct _stm_capture_hw_features) {
      .name                     = "DVP",
      .type                     = STM_PIXEL_CAPTURE_DVP,
      .hw_features              = {
        .chroma_hsrc            = 1,
      },
    }
  },
  /* Default compatible for compo capture*/
  {
    .compatible = "st,compo-capture",
    .data = (void *)&(struct _stm_capture_hw_features) {
      .name                     = "COMPO",
      .type                     = STM_PIXEL_CAPTURE_COMPO,
      .hw_features              = {
        .chroma_hsrc            = 0,
      },
    }
  },
  /* Default compatible for dvp capture*/
  {
    .compatible = "st,dvp-capture",
    .data = (void *)&(struct _stm_capture_hw_features) {
      .name                     = "DVP",
      .type                     = STM_PIXEL_CAPTURE_DVP,
      .hw_features              = {
        .chroma_hsrc            = 0,
      },
    }
  },
  {},
};

static inline ktime_t get_system_time(void)
{
  struct timespec ts;

  getrawmonotonic(&ts);

  return timespec_to_ktime(ts);
}


static void stm_capture_update_worker(struct work_struct *work)
{
  struct stm_capture_sync_context *cap_ctx;
  struct stm_capture_config *cap_cfg = NULL;
  int res = 0;

  if(!work)
    return;

  cap_ctx = container_of(work, struct stm_capture_sync_context, cap_work);
  cap_cfg = cap_ctx->cap_cfg;

  if(cap_cfg->pixel_capture) {
    if((res = stm_pixel_capture_device_update(cap_cfg->pixel_capture, cap_ctx->pipeline_id, cap_ctx->vsyncTime, cap_ctx->timingevent)) < 0)
      stm_cap_printe ("update capture device failed err = %d!!\n", res);
  }
}

#if !defined(CONFIG_STM_VIRTUAL_PLATFORM)
  /*
   * We are not going to notify the capture event through stm_event in
   * the VSOC platform as this will kill us!
   */
#define CAPTURE_USE_WORK_THREAD
#endif /* !CONFIG_STM_VIRTUAL_PLATFORM */

static void stm_capture_vsync_cb(stm_vsync_context_handle_t context, uint32_t timingevent)
{
  struct stm_capture_sync_context *cap_ctx = (struct stm_capture_sync_context *)context;
  ktime_t currenttime = get_system_time();

  if(!cap_ctx)
    return;

  cap_ctx->timingevent = timingevent;

  if(timingevent & STM_TIMING_EVENT_TOP_FIELD)
    return;

  cap_ctx->vsyncTime = ktime_to_us(currenttime);
#if defined(CAPTURE_USE_WORK_THREAD)
  schedule_work(&cap_ctx->cap_work);
#else
  stm_capture_update_worker(&cap_ctx->cap_work);
#endif
}

/*
 * First level capture interrupt handler
 */
static irqreturn_t stm_capture_interrupt(int irq, void* data)
{
  struct stm_capture_sync_context *cap_ctx = (struct stm_capture_sync_context *)data;
  ktime_t currenttime = get_system_time();

  if(!cap_ctx)
    return IRQ_HANDLED;

  if(!cap_ctx->cap_cfg)
    return IRQ_HANDLED;

  /* set the vsync time value during the interrupt handler */
  cap_ctx->vsyncTime = ktime_to_us(currenttime);

  if(stm_pixel_capture_handle_interrupts(cap_ctx->cap_cfg->pixel_capture, &cap_ctx->timingevent) < 0)
    return IRQ_HANDLED;

  return IRQ_WAKE_THREAD;
}

/*
 * Threaded capture interrupt handler
 */
static irqreturn_t stm_capture_interrupt_thread_fn(int irq, void* data)
{
  struct stm_capture_sync_context *cap_ctx = (struct stm_capture_sync_context *)data;

  if(!cap_ctx)
    return IRQ_HANDLED;

  if(!cap_ctx->cap_cfg)
    return IRQ_HANDLED;

  stm_capture_update_worker(&cap_ctx->cap_work);

  return IRQ_HANDLED;
}

#ifdef CONFIG_PM_RUNTIME
static int stm_pixel_capture_pm_runtime_get(const uint32_t type, const uint32_t id)
{
  struct stm_capture_sync_context *cap_context = &capture_contexts[type][id];
  if(!cap_context->pdev)
  {
    printk(KERN_ERR "%s: invalid platform device\n", __FUNCTION__);
    return -ENODEV;
  }

  DPRINTK_NOPREFIX("%s: id=%d, dev=%p\n", __FUNCTION__, id, &cap_context->pdev->dev);
  return pm_runtime_get_sync(&cap_context->pdev->dev);
}

static int stm_pixel_capture_pm_runtime_put(const uint32_t type, const uint32_t id)
{
  struct stm_capture_sync_context *cap_context = &capture_contexts[type][id];
  if(!cap_context->pdev)
  {
    printk(KERN_ERR "%s: invalid platform device\n", __FUNCTION__);
    return -ENODEV;
  }

  DPRINTK_NOPREFIX("%s: id=%d, dev=%p\n", __FUNCTION__, id, &cap_context->pdev->dev);
  return pm_runtime_put_sync(&cap_context->pdev->dev);
}
#endif

#ifndef CONFIG_ARCH_STI
static int __init
stm_capture_probe (struct platform_device *pdev)
#else
static int
stm_capture_probe (struct platform_device *pdev)
#endif
{
  struct device *dev = &pdev->dev;
  struct stm_capture_config *cap_cfg = NULL;
  struct stmcore_display_pipeline_data display_pipeline;
  struct resource *irq_resource, *mem_resource;
  uint32_t size;
  int pipeline_id=0;
  int res;

  stm_cap_printd (0, "%s\n", __func__);

  cap_cfg = devm_kzalloc (dev, sizeof (*cap_cfg), GFP_KERNEL);
  if (!cap_cfg) {
    stm_cap_printe ("Can't allocate memory for device "
         "description\n");
    res = -ENOMEM;
    goto out;
  }

  pm_runtime_set_active(&pdev->dev);
  pm_runtime_enable(&pdev->dev);

  if (pdev->dev.of_node)
  {
    const struct of_device_id *dev_id = of_match_device(stm_capture_match, &pdev->dev);
    if(dev_id != NULL)
    {
      cap_cfg->device_type    = ((struct _stm_capture_hw_features *)dev_id->data)->type;
      cap_cfg->hw_features    = ((struct _stm_capture_hw_features *)dev_id->data)->hw_features;
      snprintf(cap_cfg->name, sizeof(cap_cfg->name), "%s", ((struct _stm_capture_hw_features *)dev_id->data)->name);
    }
    else
    {
      res = -ENODEV;
      stm_cap_printe("capture device (name = %s) is not supported!\n", pdev->dev.of_node->name);
      goto out;
    }

    if (cap_cfg->device_type == STM_PIXEL_CAPTURE_COMPO)
    {
      cap_cfg->device_id = of_alias_get_id(pdev->dev.of_node, "compo");
    }
    else if (cap_cfg->device_type == STM_PIXEL_CAPTURE_DVP)
    {
      cap_cfg->device_id = of_alias_get_id(pdev->dev.of_node, "dvp");
    }
    else
    {
      res = -ENODEV;
      stm_cap_printe("capture device (name = %s) is not supported!\n", pdev->dev.of_node->name);
      goto out;
    }

    if(cap_cfg->device_id < 0)
    {
      /*
       * No capture alias available for this capture platform_device.
       *
       * We may be providing DT nodes description with no aliases begin
       * registered for capture. This is true in case we only have a single
       * capture device instance on the concerned platform.
       */
      cap_cfg->device_id = 0;
      stm_cap_printi("No capture alias available for this capture platform_device.\n");
    }

    /* Update the device name with the device id */
    snprintf(cap_cfg->name, sizeof(cap_cfg->name), "%s-CAPTURE_%u", cap_cfg->name, cap_cfg->device_id);
    stm_cap_printi("capture device (id=%d) supported.\n", cap_cfg->device_id);
  }
  else
  {
    cap_cfg->device_type = ((struct _stm_capture_hw_features *)pdev->dev.platform_data)->type;
    cap_cfg->hw_features = ((struct _stm_capture_hw_features *)pdev->dev.platform_data)->hw_features;
    snprintf(cap_cfg->name, sizeof(cap_cfg->name), "%s", ((struct _stm_capture_hw_features *)pdev->dev.platform_data)->name);
    /* set capture configuration info */
    cap_cfg->device_id   = pdev->dev.id;
  }

  res = stm_capture_register_device (dev,
               capture_attrs, cap_cfg);
  if (res)
  {
    stm_cap_printe("failed to register pixel capture device id %d!\n", cap_cfg->device_id);
    goto out;
  }

  mem_resource = platform_get_resource_byname(pdev, IORESOURCE_MEM, "capture-io");
  if (!mem_resource)
  {
    stm_cap_printe("failed to get register map%d!\n", cap_cfg->device_id);
    res=-ENODEV;
    goto out;
  }

  size = mem_resource->end - mem_resource->start + 1;
  if(stm_pixel_capture_device_init(cap_cfg->device_type, mem_resource->start, size, cap_cfg->hw_features) != 0)
  {
    stm_cap_printe("failed to init pixel capture device id %d!\n", cap_cfg->device_id);
    res=-ENODEV;
    goto out;
  }

  if(stm_pixel_capture_open(cap_cfg->device_type, cap_cfg->device_id, &(cap_cfg->pixel_capture)) != 0)
  {
    stm_cap_printe("failed to get pixel capture device id %d!\n", cap_cfg->device_id);
    res=-ENODEV;
    goto out;
  }

  /* Look for IT if available */
  irq_resource = platform_get_resource_byname(pdev, IORESOURCE_IRQ, "capture-int");

  if(!irq_resource)
  {
    /* register vsync callbacks */
    for(pipeline_id=0; pipeline_id < STM_CAPTURE_MAX_INSTANCES; pipeline_id++)
    {
      struct stm_capture_sync_context *cap_context = &capture_contexts[cap_cfg->device_type][pipeline_id];

      if((res = stmcore_get_display_pipeline(pipeline_id, &display_pipeline)<0))
      {
        stm_cap_printi("display pipeline (%u) not available, skip callback registration\n",pipeline_id);
        continue;
      }

      INIT_LIST_HEAD(&(cap_context->vsync_cb_info.node));
      cap_context->vsync_cb_info.owner    = NULL;
      cap_context->pipeline_id            = pipeline_id;
      cap_context->cap_cfg                = cap_cfg;
      cap_context->vsync_cb_info.context  = cap_context;
      cap_context->vsync_cb_info.cb       = stm_capture_vsync_cb;
      cap_context->pdev                   = pdev;

      if((res = stmcore_register_vsync_callback(display_pipeline.display_runtime, &cap_context->vsync_cb_info)<0))
      {
        stm_cap_printe("Cannot register capture vsync callback\n");
        continue;
      }
      INIT_WORK(&cap_context->cap_work, stm_capture_update_worker);
    }
  }
  else
  {
    struct stm_capture_sync_context *cap_context = &capture_contexts[cap_cfg->device_type][cap_cfg->device_id];

    cap_context->pipeline_id            = cap_cfg->device_id;
    cap_context->cap_cfg                = cap_cfg;
    cap_context->pdev                   = pdev;

    /* install interrupt for this capture HW */
    if((res = request_threaded_irq(irq_resource->start,
                   stm_capture_interrupt,
                   stm_capture_interrupt_thread_fn,
                   IRQF_DISABLED,
                   cap_cfg->name,
                   cap_context)) < 0)
      goto out;

    INIT_WORK(&cap_context->cap_work, stm_capture_update_worker);
  }

  platform_set_drvdata (pdev, cap_cfg);

#ifdef CONFIG_PM_RUNTIME
  /* Register Get/Put device ops */
  stm_pixel_capture_device_register_pm_runtime_hooks(cap_cfg->device_type, cap_cfg->device_id, stm_pixel_capture_pm_runtime_get, stm_pixel_capture_pm_runtime_put);
#endif
  pm_runtime_suspend(&pdev->dev);

  return 0;

out:
  stm_capture_unregister_device (cap_cfg);
  return res;
}

static int __exit
stm_capture_remove (struct platform_device *pdev)
{
  struct stm_capture_config *cap_cfg;
  struct stmcore_display_pipeline_data display_pipeline;
  struct resource *irq_resource;
  int pipeline_id=0;

  stm_cap_printd (0, "%s\n", __func__);

  if ((cap_cfg = platform_get_drvdata (pdev)) != NULL)
  {
    irq_resource = platform_get_resource_byname(pdev, IORESOURCE_IRQ, "capture-int");
    if(!irq_resource)
    {
      /* unregister VSync callbacks */
      for(pipeline_id=0; pipeline_id < STM_CAPTURE_MAX_INSTANCES; pipeline_id++)
      {
        struct stm_capture_sync_context *cap_context = &capture_contexts[cap_cfg->device_type][pipeline_id];
        if(stmcore_get_display_pipeline(pipeline_id, &display_pipeline)<0)
        {
          stm_cap_printd(0, "%s : display pipeline (%u) not available, skip callback removal\n", __func__, pipeline_id);
          continue;
        }
        stmcore_unregister_vsync_callback(display_pipeline.display_runtime, &cap_context->vsync_cb_info);
        if(!list_empty(&(cap_context->cap_work.entry)))
          flush_work(&cap_context->cap_work);
        cap_context->cap_cfg = NULL;
      }
    }
    else
    {
      struct stm_capture_sync_context *cap_context = &capture_contexts[cap_cfg->device_type][cap_cfg->device_id];
      free_irq(irq_resource->start, cap_context);
      if(!list_empty(&(cap_context->cap_work.entry)))
        flush_work(&cap_context->cap_work);
      cap_context->cap_cfg = NULL;
    }

    stm_pixel_capture_close(cap_cfg->pixel_capture);

    stm_capture_unregister_device (cap_cfg);
  }
  else
    stm_cap_printe ("%s : cannot retreive platform driver data!\n", __func__);

  /*
   * Missing pm_runtime_disable call in driver remove path caused
   * an "Unbalanaced pm_runtime_enable" warning when driver is reloaded.
   */
  pm_runtime_disable(&pdev->dev);

  stm_cap_printd (0, "%s driver removed\n", __func__);
  return 0;
}

#if defined(CONFIG_PM)
static int stm_capture_set_power_state(struct platform_device *pdev, pm_message_t state)
{
  int retval = 0;
  struct stm_capture_config *cap_cfg = (struct stm_capture_config *)platform_get_drvdata(pdev);

  if(!cap_cfg)
    return -ENODEV;

  stm_cap_printd (0, "%s : changing power state of device (type = %d - id = %d)\n", __func__,
                     cap_cfg->device_type, cap_cfg->device_id);

  switch(state.event) {
    case PM_EVENT_RESUME:
    case PM_EVENT_RESTORE:
      {
        stm_cap_printd (0, " %s...\n", (state.event == PM_EVENT_RESUME) ? "Resuming" : "Restoring");

        retval = stm_pixel_capture_device_resume(cap_cfg->pixel_capture);

        stm_cap_printd (0, " %s.\n", (retval != 0) ? "KO" : "OK");
      }
      break;
    case PM_EVENT_SUSPEND:
      {
        stm_cap_printd (0, " Suspending...\n");

        retval = stm_pixel_capture_device_suspend(cap_cfg->pixel_capture);

        stm_cap_printd (0, " %s.\n", (retval != 0) ? "KO" : "OK");
      }
      break;
    case PM_EVENT_FREEZE:
      {
        stm_cap_printd (0, " Freezing...\n");

        retval = stm_pixel_capture_device_freeze(cap_cfg->pixel_capture);

        stm_cap_printd (0, " %s.\n", (retval != 0) ? "KO" : "OK");
      }
      break;
    case PM_EVENT_ON:
    case PM_EVENT_THAW:
    default :
      stm_cap_printe(" Unsupported PM event!\n");
      retval = -EINVAL;
      break;
  }

  return retval;
}

static int stm_capture_suspend(struct device *dev)
{
  struct platform_device *pdev = to_platform_device(dev);

  return stm_capture_set_power_state(pdev,PMSG_SUSPEND);
}

static int stm_capture_resume(struct device *dev)
{
  struct platform_device *pdev = to_platform_device(dev);

  return stm_capture_set_power_state(pdev,PMSG_RESUME);
}

static int stm_capture_freeze(struct device *dev)
{
  struct platform_device *pdev = to_platform_device(dev);

  return stm_capture_set_power_state(pdev,PMSG_FREEZE);
}

static int stm_capture_restore(struct device *dev)
{
  struct platform_device *pdev = to_platform_device(dev);

  return stm_capture_set_power_state(pdev,PMSG_RESTORE);
}

const struct dev_pm_ops stm_capture_pm_ops = {
  .suspend         = stm_capture_suspend,
  .resume          = stm_capture_resume,
  .freeze          = stm_capture_freeze,
  .thaw            = stm_capture_resume,
  .poweroff        = stm_capture_suspend,
  .restore         = stm_capture_restore,
  .runtime_suspend = stm_capture_suspend,
  .runtime_resume  = stm_capture_resume,
  .runtime_idle    = NULL,
};

#define CAPTURE_PM_OPS   (&stm_capture_pm_ops)
#else /* CONFIG_PM */
#define CAPTURE_PM_OPS   NULL
#endif /* CONFIG_PM */

static struct platform_driver stm_capture_driver = {
  .driver.name  = "stm-capture",
  .driver.owner = THIS_MODULE,
  .driver.pm    = CAPTURE_PM_OPS,
  .driver.of_match_table = of_match_ptr(stm_capture_match),
  .probe        = stm_capture_probe,
  .remove       = __exit_p (stm_capture_remove),
};


static int __init capture_module_init(void)
{
  int res;

  stm_cap_printd (0, "%s\n", __func__);

  stm_capture_class_init (STM_CAPTURE_MAX_DEVICES);

  res = platform_driver_register (&stm_capture_driver);
  if (res)
  {
    stm_capture_class_cleanup (STM_CAPTURE_MAX_DEVICES);
    return res;
  }

  return res;
}


static void __exit capture_module_exit(void)
{
  stm_cap_printd (0, "%s\n", __func__);

  platform_driver_unregister (&stm_capture_driver);

  stm_capture_class_cleanup (STM_CAPTURE_MAX_DEVICES);
}


/******************************************************************************
 *  Modularization
 */
#ifdef MODULE

MODULE_AUTHOR ("Akram BEN BELGACEM <akram.ben-belgacem@st.com");
MODULE_DESCRIPTION ("STMicroelectronics Pixel Capture driver");
MODULE_LICENSE ("GPL");
MODULE_VERSION (KBUILD_VERSION);

module_init (capture_module_init);
module_exit (capture_module_exit);

EXPORT_SYMBOL(stm_pixel_capture_open);
EXPORT_SYMBOL(stm_pixel_capture_close);
EXPORT_SYMBOL(stm_pixel_capture_lock);
EXPORT_SYMBOL(stm_pixel_capture_unlock);
EXPORT_SYMBOL(stm_pixel_capture_enum_inputs);
EXPORT_SYMBOL(stm_pixel_capture_set_input);
EXPORT_SYMBOL(stm_pixel_capture_get_input);
EXPORT_SYMBOL(stm_pixel_capture_get_input_window);
EXPORT_SYMBOL(stm_pixel_capture_get_input_window_capabilities);
EXPORT_SYMBOL(stm_pixel_capture_set_input_window);
EXPORT_SYMBOL(stm_pixel_capture_try_format);
EXPORT_SYMBOL(stm_pixel_capture_set_format);
EXPORT_SYMBOL(stm_pixel_capture_get_format);
EXPORT_SYMBOL(stm_pixel_capture_start);
EXPORT_SYMBOL(stm_pixel_capture_stop);
EXPORT_SYMBOL(stm_pixel_capture_query_capabilities);
EXPORT_SYMBOL(stm_pixel_capture_queue_buffer);
EXPORT_SYMBOL(stm_pixel_capture_dequeue_buffer);
EXPORT_SYMBOL(stm_pixel_capture_attach);
EXPORT_SYMBOL(stm_pixel_capture_detach);
EXPORT_SYMBOL(stm_pixel_capture_get_status);
EXPORT_SYMBOL(stm_pixel_capture_set_stream_params);
EXPORT_SYMBOL(stm_pixel_capture_get_stream_params);
EXPORT_SYMBOL(stm_pixel_capture_set_input_params);
EXPORT_SYMBOL(stm_pixel_capture_get_input_params);
EXPORT_SYMBOL(stm_pixel_capture_device_update);
EXPORT_SYMBOL(stm_pixel_capture_handle_interrupts);
EXPORT_SYMBOL(stm_pixel_capture_enum_image_formats);

#endif /* MODULE */
