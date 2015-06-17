/***********************************************************************
 *
 * File: linux/kernel/drivers/stm/coredisplay/coredisplay.c
 * Copyright (c) 2000-2012 STMicroelectronics Limited.
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
#include <linux/pm_runtime.h>
#include <linux/string.h>
#include <linux/ktime.h>
#include <linux/dma-mapping.h>
#include <linux/of.h>
#include <linux/clk.h>
#ifndef CONFIG_ARCH_STI
#include <linux/stm/pad.h>
#endif

#include <asm/irq.h>
#include <linux/semaphore.h>

#include <stm_display.h>
#include <linux/stm/stmcoredisplay.h>
#include <display_device_priv.h>
#include <linux/of_platform.h>
#include <vibe_debug.h>


#define PRINT_VSYNC_IRQ_DURATION  0

struct stmcore_display_data {
  uint32_t             device_id;
  stm_display_device_h device_handle;
};

static struct stmcore_display_data           display_data = {0, 0};
static struct stmcore_display_pipeline_data *display_pipeline_data;
static int nr_display_pipelines;

/*
 * Device class to hang all the sysfs attributes from.
 */
static struct class *stmcore_class;

/*
 * This platform driver is begin used to initialize coredisplay platform data
 * in case DT is enabled.
 *
 * Otherwise (non DT support) this is only used to provide the PM support. In
 * this case we would need to create the matching platform device by our self.
 */
static struct platform_driver *stmcore_driver;

/* Functions expected to be provided by the SoC/board specific platform files */
extern int stmcore_probe_device(struct platform_device *pdev, struct stmcore_display_pipeline_data **pd, int *nr_display_pipelines);

extern int stmcore_display_postinit(struct stmcore_display_pipeline_data *);
extern void stmcore_cleanup_device(void);
#ifndef CONFIG_ARCH_STI
extern void stmcore_display_device_set_pm(struct device *dev, int event);
#endif

static inline ktime_t get_system_time(void)
{
  struct timespec ts;

  getrawmonotonic(&ts);

  return timespec_to_ktime(ts);
}

/*
 * First level vsync interrupt handler
 */
static irqreturn_t stmcore_vsync_interrupt(int irq, void* data)
{
  struct stmcore_display_pipeline_data *pd = (struct stmcore_display_pipeline_data *)data;
  struct stmcore_display *runtime;
  stm_time64_t interval;
  ktime_t t1,t2;
  unsigned long saveFlags;
  s64 last_display_processing_us;

  t1 = get_system_time();

  if (pd == NULL)
  {
    printk(KERN_ERR "stmfb_vsync_interrupt: error : null info pointer. Can't do anything.\n");
    return IRQ_NONE;
  }

  runtime = pd->display_runtime;
  runtime->last_irq_begin_time = t1;

  spin_lock_irqsave(&(runtime->spinlock), saveFlags);       /*** Start critical section ***/
  if(runtime->vsync_status != STMCORE_VSYNC_STATUS_IDLE)
  {
    TRC(TRC_ID_MAIN_INFO, "Hard VSync IRQ while last Threaded IRQ not finished!!!");
  }
  runtime->vsync_status = STMCORE_VSYNC_STATUS_HARD_IRQ_PROCESSING;
  spin_unlock_irqrestore(&(runtime->spinlock), saveFlags);  /*** Stop critical section ***/


  stm_display_output_handle_interrupts(runtime->main_output);
  stm_display_output_get_last_timing_event(runtime->main_output, &runtime->timingevent, &interval);

  if(runtime->timingevent == STM_TIMING_EVENT_LINE || runtime->timingevent == STM_TIMING_EVENT_NONE)
  {
    // This was a vtimer callback interrupt (and the main driver has already handled it)
    TRC(TRC_ID_MAIN_INFO, "vtimer callback interrupt");
    spin_lock_irqsave(&(runtime->spinlock), saveFlags);       /*** Start critical section ***/
    runtime->vsync_status = STMCORE_VSYNC_STATUS_IDLE;
    spin_unlock_irqrestore(&(runtime->spinlock), saveFlags);  /*** Stop critical section ***/
    return IRQ_HANDLED;
  }

  wake_up_interruptible(&runtime->vsync_wait_queue);
  atomic_inc (&runtime->vsync_count);

  stm_display_device_update_vsync_irq(pd->device, runtime->main_output_timing_id);

  /*
   * Process VSync callback hooks
   */
  if(!STM_VIBE_USE_THREADED_IRQ)
  {
    struct stmcore_vsync_cb *pCallback, *tmp;
    unsigned long            flags;
    read_lock_irqsave (&runtime->vsync_cb_list.lock, flags);
    list_for_each_entry_safe (pCallback, tmp, &runtime->vsync_cb_list.list, node)
    {
      pCallback->cb(pCallback->context, runtime->timingevent);
    }
    read_unlock_irqrestore (&runtime->vsync_cb_list.lock, flags);
  }

  t2 = get_system_time();
  last_display_processing_us = ktime_to_us(ktime_sub(t2,t1));
  if(PRINT_VSYNC_IRQ_DURATION)
  {
    TRC(TRC_ID_ERROR, "%u: VSync IRQ duration: %lld", runtime->main_output_timing_id, last_display_processing_us);
  }

  spin_lock_irqsave(&(runtime->spinlock), saveFlags);       /*** Start critical section ***/

  runtime->last_display_processing_us = last_display_processing_us;

  runtime->avg_display_processing_us *= 15;
  runtime->avg_display_processing_us += runtime->last_display_processing_us;
  runtime->avg_display_processing_us /= 16;

  if(runtime->last_display_processing_us < runtime->min_display_processing_us)
    runtime->min_display_processing_us = runtime->last_display_processing_us;
  if(runtime->last_display_processing_us > runtime->max_display_processing_us)
    runtime->max_display_processing_us = runtime->last_display_processing_us;

  runtime->vsync_status = STMCORE_VSYNC_STATUS_IDLE;

  spin_unlock_irqrestore(&(runtime->spinlock), saveFlags);  /*** Stop critical section ***/


  if (STM_VIBE_USE_THREADED_IRQ)
      return IRQ_WAKE_THREAD; /* TODO should we disable IRQ until end of IRQ thread function ? */
  else
      return IRQ_HANDLED;
}

/*
 * vsync interrupt thread function
 */
static irqreturn_t stmcore_vsync_interrupt_thread_fn(int irq, void* data)
{
    struct stmcore_display_pipeline_data *pd = (struct stmcore_display_pipeline_data *)data;
    struct stmcore_display *runtime;
    ktime_t t3, t4;
    s64 threaded_irq_duration;
    s64 last_display_thread_processing_us;
    unsigned long saveFlags;


    /* thread processing */
    runtime = pd->display_runtime;

    /* t3 = time when the Threaded IRQ has started */
    t3 = get_system_time();

    spin_lock_irqsave(&(runtime->spinlock), saveFlags);       /*** Start critical section ***/
    if(runtime->vsync_status != STMCORE_VSYNC_STATUS_IDLE)
    {
        TRC(TRC_ID_MAIN_INFO, "Threaded VSync IRQ while last IRQ not finished!!!");
    }
    runtime->vsync_status = STMCORE_VSYNC_STATUS_THREADED_IRQ_PROCESSING;
    spin_unlock_irqrestore(&(runtime->spinlock), saveFlags);  /*** Stop critical section ***/


    stm_display_device_update_vsync_threaded_irq(pd->device, runtime->main_output_timing_id);

    /*
     * Process VSync callback hooks
     */
    {
      struct stmcore_vsync_cb *pCallback, *tmp;
      unsigned long            flags;
      read_lock_irqsave (&runtime->vsync_cb_list.lock, flags);
      list_for_each_entry_safe (pCallback, tmp, &runtime->vsync_cb_list.list, node)
      {
        pCallback->cb(pCallback->context, runtime->timingevent);
      }
      read_unlock_irqrestore (&runtime->vsync_cb_list.lock, flags);
    }

    /* ... */

    /* keep the following at the end of this function */
    t4 = get_system_time();

    threaded_irq_duration = ktime_to_us(ktime_sub(t4,t3));
    last_display_thread_processing_us = ktime_to_us(ktime_sub(t4,runtime->last_irq_begin_time));

    if (last_display_thread_processing_us>10000)
    {
        TRC( TRC_ID_ERROR, "stmcoredisplay: IRQ+thread processing time > 10 ms (%lld us)", last_display_thread_processing_us);
    }
    if(PRINT_VSYNC_IRQ_DURATION)
    {
        TRC(TRC_ID_ERROR, "%u: VSync ThreadedIRQ duration: %lld", runtime->main_output_timing_id, threaded_irq_duration);
        TRC(TRC_ID_ERROR, "%u: Total duration: %lld", runtime->main_output_timing_id, last_display_thread_processing_us);
    }


    spin_lock_irqsave(&(runtime->spinlock), saveFlags);       /*** Start critical section ***/

    /* here "thread processing time" is IRQ + thread time */
    runtime->last_display_thread_processing_us = last_display_thread_processing_us;

    runtime->avg_display_thread_processing_us *= 15;
    runtime->avg_display_thread_processing_us += runtime->last_display_thread_processing_us;
    runtime->avg_display_thread_processing_us /= 16;

    if(runtime->last_display_thread_processing_us < runtime->min_display_thread_processing_us)
      runtime->min_display_thread_processing_us = runtime->last_display_thread_processing_us;
    if(runtime->last_display_thread_processing_us > runtime->max_display_thread_processing_us)
      runtime->max_display_thread_processing_us = runtime->last_display_thread_processing_us;

    runtime->vsync_status = STMCORE_VSYNC_STATUS_IDLE;

    spin_unlock_irqrestore(&(runtime->spinlock), saveFlags);  /*** Stop critical section ***/


    return IRQ_HANDLED;
}

/*
 * First level vsync interrupt handler
 */
static irqreturn_t stmcore_source_vsync_interrupt(int irq, void* data)
{
  struct stmcore_display_pipeline_data *pd = (struct stmcore_display_pipeline_data *)data;
  struct stmcore_source_data *source_data=0;
  struct stmcore_source *source_runtime=0;
  int source_index = 0;
  stm_display_timing_events_t source_vsync;
  stm_time64_t interval;
  ktime_t t1,t2;

  t1 = get_system_time();

  if (pd == 0)
  {
    printk(KERN_ERR "stmfb_source_vsync_interrupt: error: null info pointer. Can't do anything.\n");
    return IRQ_NONE;
  }

  while(source_index < pd->sources_nbr)
  {
    if(pd->sources[source_index].source_irq == irq)
    {
      source_data = &pd->sources[source_index];
      break;
    }
    source_index++;
  }

  if(source_data)
  {
    source_runtime = source_data->source_runtime;

    stm_display_source_handle_interrupts(source_data->source_handle);
    stm_display_source_get_last_timing_event(source_data->source_handle, &source_vsync, &interval);

    if(source_vsync == STM_TIMING_EVENT_LINE || source_vsync == STM_TIMING_EVENT_NONE)
      // This shouldn't happen for the source timing generator.
      return IRQ_HANDLED;

    wake_up_interruptible(&source_runtime->source_vsync_wait_queue);
    atomic_inc (&source_runtime->source_vsync_count);

    if(source_data->source_timing_id)
    {
      stm_display_device_source_update(pd->device, source_data->source_timing_id);
    }

    /*
    * Process Input VSync callback hooks
    */
    {
      struct stmcore_vsync_cb *pCallback, *tmp;
      unsigned long            flags;
      read_lock_irqsave (&(source_runtime->source_vsync_cb_list.lock), flags);
      list_for_each_entry_safe (pCallback, tmp, &(source_runtime->source_vsync_cb_list.list), node)
      {
        pCallback->cb(pCallback->context, source_vsync);
      }
      read_unlock_irqrestore (&(source_runtime->source_vsync_cb_list.lock), flags);
    }

    t2 = get_system_time();
    source_runtime->last_source_processing_us = ktime_to_us(ktime_sub(t2,t1));

    source_runtime->avg_source_processing_us *= 15;
    source_runtime->avg_source_processing_us += source_runtime->last_source_processing_us;
    source_runtime->avg_source_processing_us /= 16;

    if(source_runtime->last_source_processing_us < source_runtime->min_source_processing_us)
      source_runtime->min_source_processing_us = source_runtime->last_source_processing_us;
    if(source_runtime->last_source_processing_us > source_runtime->max_source_processing_us)
      source_runtime->max_source_processing_us = source_runtime->last_source_processing_us;

    return IRQ_HANDLED;
  }

  return IRQ_NONE;
}


/******************************************************************************
 * Driver cleanup
 */
static bool stmcore_display_runtime_exit(int device_id)
{
  if(!display_pipeline_data[device_id].display_runtime)
    return false;

  if(display_pipeline_data[device_id].vtg_irq != -1)
  {
    flush_scheduled_work();
    free_irq(display_pipeline_data[device_id].vtg_irq, &display_pipeline_data[device_id]);
    display_pipeline_data[device_id].vtg_irq = -1;
  }

  stm_display_output_close(display_pipeline_data[device_id].display_runtime->main_output);
  display_pipeline_data[device_id].display_runtime->main_output = NULL;

  kfree(display_pipeline_data[device_id].display_runtime);
  display_pipeline_data[device_id].display_runtime = NULL;

  return true;
}


static bool stmcore_source_runtime_exit(int device_id)
{
  int source_index = 0;

  /* In case of no sources just exist whithout doing nothing */
  if(display_pipeline_data[device_id].sources_nbr <= 0)
    return true;

  for(source_index = 0; source_index < display_pipeline_data[device_id].sources_nbr; source_index++)
  {
    if(!display_pipeline_data[device_id].sources[source_index].source_runtime)
      continue;

    if(display_pipeline_data[device_id].sources[source_index].source_irq != -1)
    {
      flush_scheduled_work();
      free_irq(display_pipeline_data[device_id].sources[source_index].source_irq, &display_pipeline_data[device_id]);
      display_pipeline_data[device_id].sources[source_index].source_irq = -1;
    }

    kfree(display_pipeline_data[device_id].sources[source_index].source_runtime);
    display_pipeline_data[device_id].sources[source_index].source_runtime = NULL;

    stm_display_source_close(display_pipeline_data[device_id].sources[source_index].source_handle);
  }

  return true;
}


static void stmcore_display_exit(void)
{
  int i, res;

  TRCIN( TRC_ID_MAIN_INFO, "" );

  /*
   * Destroy the platform driver
   */
  if(stmcore_driver)
  {
    platform_driver_unregister(stmcore_driver);
    stmcore_driver = NULL;
  }

  if(display_data.device_handle)
  {
    /* Inform the driver that shutting down process is now started so it is no more needed to check the power state */
    res = stm_display_device_shutting_down(display_data.device_handle);
    if(res < 0)
    {
      TRC(TRC_ID_ERROR, "stm_display_device_shutting_down failed!");
    }
  }

  /*
   * Destroy the platform devices and global core driver handles
   */
  for(i=0;i<nr_display_pipelines;i++)
  {
    if(!stmcore_display_runtime_exit(i))
      continue;

    if(!stmcore_source_runtime_exit(i))
      continue;

#ifdef CONFIG_ARCH_STI
    if ((display_pipeline_data[i].dvo_output_id != -1)  && display_pipeline_data[i].dvo_pinctrl)
    {
      devm_pinctrl_put(display_pipeline_data[i].dvo_pinctrl);
      display_pipeline_data[i].dvo_pinctrl = NULL;
      display_pipeline_data[i].dvo_pins_default = NULL;
    }
#else
    if ((display_pipeline_data[i].dvo_output_id != -1)  && display_pipeline_data[i].dvo_pad_config)
    {
      stm_pad_release(display_pipeline_data[i].dvo_pad_state);
      display_pipeline_data[i].dvo_pad_state  = NULL;
      display_pipeline_data[i].dvo_pad_config = NULL;
    }
#endif

    if(display_pipeline_data[i].class_device)
    {
      device_unregister(display_pipeline_data[i].class_device);
      display_pipeline_data[i].class_device = NULL;
    }

    if(display_pipeline_data[i].device)
    {
      display_pipeline_data[i].device = NULL;
    }

    if (display_pipeline_data[i].name)
        kfree(display_pipeline_data[i].name);
    if (display_pipeline_data[i].whitelist)
        kfree(display_pipeline_data[i].whitelist);
    /* the pipe data was allocated with devm_kzalloc
       so it is freed during driver detach */
  }

  if(display_data.device_handle)
  {
    stm_display_device_destroy(display_data.device_handle);
    display_data.device_handle = NULL;
  }


  if(display_pipeline_data)
    kfree(display_pipeline_data);

  stmcore_cleanup_device();

  class_destroy(stmcore_class);

  TRCOUT( TRC_ID_MAIN_INFO, "" );
}


#if defined(SDK2_ENABLE_DISPLAY_ATTRIBUTES)
/******************************************************************************
 * Sysfs attributes
 */
static ssize_t stmcore_show_display_info(struct device *class_device, struct device_attribute *attr, char *buf)
{
  struct stmcore_display_pipeline_data *platform_data = dev_get_drvdata(class_device);
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

  for(plane=0;plane<STMCORE_MAX_PLANES;plane++)
  {
    stm_display_plane_h hPlane;
    struct stmcore_plane_data *p = &platform_data->planes[plane];
    if(p->id == -1)
      break;

    if(stm_display_device_open_plane(platform_data->device, p->id,&hPlane) == 0)
    {
      const char *name;

      if(stm_display_plane_get_name(hPlane, &name) == 0)
      {
        sz += snprintf(&buf[sz], PAGE_SIZE - sz, "%s: ID=%d ",name,p->id);
        sz += snprintf(&buf[sz], PAGE_SIZE - sz, "%s",(p->flags&STMCORE_PLANE_VIDEO)?"Video":"Graphics");
        if(p->flags & STMCORE_PLANE_PREFERRED)
          sz += snprintf(&buf[sz], PAGE_SIZE - sz, ", Preferred");

        if(p->flags & STMCORE_PLANE_SHARED)
          sz += snprintf(&buf[sz], PAGE_SIZE - sz, ", Shared");

        sz += snprintf(&buf[sz], PAGE_SIZE - sz, "\n");
      }

      stm_display_plane_close(hPlane);
    }
  }
  return sz;
}


static ssize_t stmcore_show_display_timing(struct device *class_device, struct device_attribute *attr, char *buf)
{
  struct stmcore_display_pipeline_data *platform_data = dev_get_drvdata(class_device);
  int sz=0;
  unsigned long saveFlags;
  s64 tlast, tavg, tmax, tmin;
  s64 ttlast, ttavg, ttmax, ttmin;

  spin_lock_irqsave(&(platform_data->display_runtime->spinlock), saveFlags);        /*** Start critical section ***/

  tlast = platform_data->display_runtime->last_display_processing_us;
  tavg  = platform_data->display_runtime->avg_display_processing_us;
  tmin  = platform_data->display_runtime->min_display_processing_us;
  tmax  = platform_data->display_runtime->max_display_processing_us;
  platform_data->display_runtime->min_display_processing_us = LLONG_MAX;
  platform_data->display_runtime->max_display_processing_us = 0;

  if (STM_VIBE_USE_THREADED_IRQ)
  {
    ttlast = platform_data->display_runtime->last_display_thread_processing_us;
    ttavg  = platform_data->display_runtime->avg_display_thread_processing_us;
    ttmin  = platform_data->display_runtime->min_display_thread_processing_us;
    ttmax  = platform_data->display_runtime->max_display_thread_processing_us;
    platform_data->display_runtime->min_display_thread_processing_us = LLONG_MAX;
    platform_data->display_runtime->max_display_thread_processing_us = 0;
  }

  spin_unlock_irqrestore(&(platform_data->display_runtime->spinlock), saveFlags);   /*** Stop critical section ***/

  sz += snprintf(&buf[sz], PAGE_SIZE - sz, "Last display processing time: %lldus\n",tlast);
  sz += snprintf(&buf[sz], PAGE_SIZE - sz, "Average display processing time: %lldus\n",tavg);
  sz += snprintf(&buf[sz], PAGE_SIZE - sz, "Min display processing time: %lldus (reset on read)\n",tmin);
  sz += snprintf(&buf[sz], PAGE_SIZE - sz, "Max display processing time: %lldus (reset on read)\n",tmax);

  if (STM_VIBE_USE_THREADED_IRQ)
  {
    sz += snprintf(&buf[sz], PAGE_SIZE - sz, "Last display thread processing time: %lldus\n",ttlast);
    sz += snprintf(&buf[sz], PAGE_SIZE - sz, "Average display thread processing time: %lldus\n",ttavg);
    sz += snprintf(&buf[sz], PAGE_SIZE - sz, "Min display thread processing time: %lldus (reset on read)\n",ttmin);
    sz += snprintf(&buf[sz], PAGE_SIZE - sz, "Max display thread processing time: %lldus (reset on read)\n",ttmax);
  }

  return sz;

}


static ssize_t stmcore_show_source_timing(struct device *class_device, struct device_attribute *attr, char *buf)
{
  struct stmcore_display_pipeline_data *platform_data = dev_get_drvdata(class_device);
  int sz=0;
  unsigned long saveFlags;
  s64 tlast, tavg, tmax, tmin;
  int source_index = 0;
  struct stmcore_source *source_runtime;

  for(source_index = 0; source_index < platform_data->sources_nbr; source_index++)
  {
    source_runtime    = platform_data->sources[source_index].source_runtime;
    if(!source_runtime)
      continue;

    spin_lock_irqsave(&(source_runtime->spinlock), saveFlags);
    tlast = source_runtime->last_source_processing_us;
    tavg  = source_runtime->avg_source_processing_us;
    tmin  = source_runtime->min_source_processing_us;
    tmax  = source_runtime->max_source_processing_us;
    source_runtime->min_source_processing_us = LLONG_MAX;
    source_runtime->max_source_processing_us = 0;
    spin_unlock_irqrestore(&(source_runtime->spinlock), saveFlags);

    sz += snprintf(&buf[sz], PAGE_SIZE - sz, "%s timing info:\n",platform_data->sources[source_index].source_name);
    sz += snprintf(&buf[sz], PAGE_SIZE - sz, "Last input processing time: %lldus\n",tlast);
    sz += snprintf(&buf[sz], PAGE_SIZE - sz, "Average input processing time: %lldus\n",tavg);
    sz += snprintf(&buf[sz], PAGE_SIZE - sz, "Min input processing time: %lldus (reset on read)\n",tmin);
    sz += snprintf(&buf[sz], PAGE_SIZE - sz, "Max input processing time: %lldus (reset on read)\n",tmax);
  }

  return sz;
}


struct output_device_attribute{
        struct device_attribute dev_attr;
        stm_output_control_t    control;
        const char             *units;
};

#define to_output_dev_attr(_dev_attr) \
        container_of(_dev_attr, struct output_device_attribute, dev_attr)

#define OUTPUT_ATTR(_control,_units, _show, _set)                        \
        { .dev_attr = __ATTR(_control, (S_IRUGO|S_IWUSR), _show, _set),  \
          .control  = _control, \
          .units    = _units }

#define OUTPUT_DEVICE_ATTR(_control, _units) \
struct output_device_attribute output_dev_attr_##_control          \
        = OUTPUT_ATTR(_control, _units, stmcore_show_output_control, stmcore_set_output_control)

#define OUTPUT_SIGNED_DEVICE_ATTR(_control, _units) \
struct output_device_attribute output_dev_attr_##_control          \
        = OUTPUT_ATTR(_control, _units, stmcore_show_signed_output_control, stmcore_set_signed_output_control)

#define OUTPUT_COMPOUND_DEVICE_ATTR(_compound_control, _units) \
struct output_device_attribute output_dev_attr_##_compound_control \
        = OUTPUT_ATTR(_compound_control, _units, stmcore_show_compound_output_control, stmcore_set_compound_output_control)

static ssize_t stmcore_show_output_control(struct device *class_device, struct device_attribute *attr, char *buf)
{
  struct stmcore_display_pipeline_data *platform_data = dev_get_drvdata(class_device);
  struct output_device_attribute *out_attr = to_output_dev_attr(attr);
  int sz=0;
  stm_display_output_h hOutput;
  uint32_t value;

  if(stm_display_device_open_output(platform_data->device, platform_data->main_output_id,&hOutput)<0)
    return 0;

  if(stm_display_output_get_control(hOutput, out_attr->control, &value)<0)
  {
    stm_display_output_close(hOutput);
    return 0;
  }

  sz += snprintf(&buf[sz], PAGE_SIZE - sz, "%u%s\n", value, out_attr->units);

  stm_display_output_close(hOutput);
  return sz;
}


static ssize_t stmcore_set_output_control(struct device           *class_device,
                                          struct device_attribute *attr,
                                          const char              *buf,
                                          size_t                   count)
{
  struct stmcore_display_pipeline_data *platform_data = dev_get_drvdata(class_device);
  struct output_device_attribute *out_attr = to_output_dev_attr(attr);
  stm_display_output_h hOutput;
  uint32_t value;

  sscanf(buf,"%u\n",&value);

  if(stm_display_device_open_output(platform_data->device, platform_data->main_output_id,&hOutput)==0)
  {
    stm_display_output_set_control(hOutput, out_attr->control, value);
    stm_display_output_close(hOutput);
  }

  return count;
}


static ssize_t stmcore_show_signed_output_control(struct device *class_device, struct device_attribute *attr, char *buf)
{
  struct stmcore_display_pipeline_data *platform_data = dev_get_drvdata(class_device);
  struct output_device_attribute *out_attr = to_output_dev_attr(attr);
  int sz=0;
  stm_display_output_h hOutput;
  uint32_t value;

  if(stm_display_device_open_output(platform_data->device, platform_data->main_output_id,&hOutput)<0)
    return 0;

  if(stm_display_output_get_control(hOutput, out_attr->control, &value)<0)
  {
    stm_display_output_close(hOutput);
    return 0;
  }

  sz += snprintf(&buf[sz], PAGE_SIZE - sz, "%d%s\n", (int)value, out_attr->units);

  stm_display_output_close(hOutput);
  return sz;
}


static ssize_t stmcore_set_signed_output_control(struct device           *class_device,
                                                 struct device_attribute *attr,
                                                 const char              *buf,
                                                 size_t                   count)
{
  struct stmcore_display_pipeline_data *platform_data = dev_get_drvdata(class_device);
  struct output_device_attribute *out_attr = to_output_dev_attr(attr);
  stm_display_output_h hOutput;
  int value;

  sscanf(buf,"%d\n",&value);

  if(stm_display_device_open_output(platform_data->device, platform_data->main_output_id,&hOutput)==0)
  {
    stm_display_output_set_control(hOutput, out_attr->control, (uint32_t)value);
    stm_display_output_close(hOutput);
  }

  return count;
}


static ssize_t stmcore_show_compound_output_control(struct device *class_device, struct device_attribute *attr, char *buf)
{
  struct stmcore_display_pipeline_data *platform_data = dev_get_drvdata(class_device);
  struct output_device_attribute *out_attr = to_output_dev_attr(attr);
  int sz=0;
  stm_display_output_h hOutput;

  if(stm_display_device_open_output(platform_data->device, platform_data->main_output_id,&hOutput)==0)
  {
    switch (out_attr->control)
    {
      case OUTPUT_CTRL_VIDEO_OUT_CALIBRATION:
      {
        stm_display_analog_calibration_setup_t f;
        memset(&f, 0, sizeof(stm_display_analog_calibration_setup_t));

        if(stm_display_output_get_compound_control(hOutput, out_attr->control, (void *)&f)<0)
        {
          stm_display_output_close(hOutput);
          return 0;
        }

        if(f.type & DENC_FACTORS)
        {
          sz += snprintf(&buf[sz], PAGE_SIZE - sz, "denc=%hu,%hu,%hu,%hi,%hi,%hi\n", f.denc.Scaling_factor_Cb, f.denc.Scaling_factor_Y, f.denc.Scaling_factor_Cr
                                                                                   , f.denc.Offset_factor_Cb , f.denc.Offset_factor_Y , f.denc.Offset_factor_Cr);
        }

        if(f.type & HDF_FACTORS)
        {
          sz += snprintf(&buf[sz], PAGE_SIZE - sz, "hdf=%hu,%hu,%hu,%hi,%hi,%hi\n" , f.hdf.Scaling_factor_Cb, f.hdf.Scaling_factor_Y, f.hdf.Scaling_factor_Cr
                                                                                   , f.hdf.Offset_factor_Cb , f.hdf.Offset_factor_Y , f.hdf.Offset_factor_Cr);
        }
        break;
      }
      case OUTPUT_CTRL_DISPLAY_ASPECT_RATIO:
      {
        stm_rational_t display_aspect_ratio;
        memset(&display_aspect_ratio, 0, sizeof(stm_rational_t));

        if(stm_display_output_get_compound_control(hOutput, out_attr->control, (void *)&display_aspect_ratio)<0)
        {
          stm_display_output_close(hOutput);
          return 0;
        }

        sz += snprintf(&buf[sz], PAGE_SIZE - sz, "%d_%d\n", display_aspect_ratio.numerator, display_aspect_ratio.denominator);
        break;
      }
      default:
        sz = 0;
        break;
    }
    stm_display_output_close(hOutput);
  }

  return sz;
}


static ssize_t stmcore_set_compound_output_control(struct device           *class_device,
                                          struct device_attribute *attr,
                                          const char              *buf,
                                          size_t                   count)
{
  struct stmcore_display_pipeline_data *platform_data = dev_get_drvdata(class_device);
  struct output_device_attribute *out_attr = to_output_dev_attr(attr);
  stm_display_output_h hOutput;

  if(stm_display_device_open_output(platform_data->device, platform_data->main_output_id,&hOutput)==0)
  {
    switch (out_attr->control)
    {
      case OUTPUT_CTRL_VIDEO_OUT_CALIBRATION:
      {
        int ret=0;
        char *buf2;
        stm_display_analog_calibration_setup_t f;
        memset(&f, 0, sizeof(stm_display_analog_calibration_setup_t));

        buf2 = strstr(buf, "denc=");
        if(buf2)
        {
          ret = sscanf(buf2,"denc=%hu,%hu,%hu,%hi,%hi,%hi", &f.denc.Scaling_factor_Cb, &f.denc.Scaling_factor_Y, &f.denc.Scaling_factor_Cr
                                                          , &f.denc.Offset_factor_Cb , &f.denc.Offset_factor_Y , &f.denc.Offset_factor_Cr);

          if(ret == 6)
            f.type |= DENC_FACTORS;
        }
        buf2 = strstr(buf, "hdf=");
        if(buf2)
        {
          ret = sscanf(buf2,"hdf=%hu,%hu,%hu,%hi,%hi,%hi", &f.hdf.Scaling_factor_Cb, &f.hdf.Scaling_factor_Y, &f.hdf.Scaling_factor_Cr
                                                         , &f.hdf.Offset_factor_Cb , &f.hdf.Offset_factor_Y , &f.hdf.Offset_factor_Cr);

          if(ret == 6)
            f.type |= HDF_FACTORS;
        }

        if ( f.type != 0 )
          stm_display_output_set_compound_control(hOutput, out_attr->control, (void *)&f);

        break;
      }
      case OUTPUT_CTRL_DISPLAY_ASPECT_RATIO:
      {
        int ret=0;
        stm_rational_t display_aspect_ratio;
        memset(&display_aspect_ratio, 0, sizeof(stm_rational_t));

        ret = sscanf(buf,"%d_%d", &display_aspect_ratio.numerator, &display_aspect_ratio.denominator);

        if(ret==2)
          stm_display_output_set_compound_control(hOutput, out_attr->control, (void *)&display_aspect_ratio);

        break;
      }
      default:
        break;
    }
    stm_display_output_close(hOutput);
  }

  return count;
}


static DEVICE_ATTR(info,S_IRUGO, stmcore_show_display_info,NULL);
static DEVICE_ATTR(display_timing,S_IRUGO, stmcore_show_display_timing,NULL);
static DEVICE_ATTR(source_timing,S_IRUGO, stmcore_show_source_timing,NULL);

static OUTPUT_DEVICE_ATTR(OUTPUT_CTRL_DAC123_MAX_VOLTAGE,"mV");
static OUTPUT_DEVICE_ATTR(OUTPUT_CTRL_DAC456_MAX_VOLTAGE,"mV");
static OUTPUT_DEVICE_ATTR(OUTPUT_CTRL_CHROMA_SCALE,"(100ths of %)");
static OUTPUT_SIGNED_DEVICE_ATTR(OUTPUT_CTRL_CLOCK_ADJUSTMENT,"ppm");
static OUTPUT_DEVICE_ATTR(OUTPUT_CTRL_CVBS_TRAP_FILTER,"");
static OUTPUT_DEVICE_ATTR(OUTPUT_CTRL_LUMA_NOTCH_FILTER,"");
static OUTPUT_DEVICE_ATTR(OUTPUT_CTRL_IF_LUMA_DELAY,"");
static OUTPUT_DEVICE_ATTR(OUTPUT_CTRL_WSS_INSERTION,"");
static OUTPUT_DEVICE_ATTR(OUTPUT_CTRL_CHROMA_DELAY,"(enumeration 0-9)");
static OUTPUT_COMPOUND_DEVICE_ATTR(OUTPUT_CTRL_VIDEO_OUT_CALIBRATION,"Scaling_factors {Cb,Y,Cr}, Offset_factors{Cb,Y,Cr}");
static OUTPUT_COMPOUND_DEVICE_ATTR(OUTPUT_CTRL_DISPLAY_ASPECT_RATIO,"Display Aspect Ratio : numerator/denominator");


static int __init stmcore_classdev_create_files(int i, struct stmcore_display_pipeline_data *display_pipeline_data)
{
  int res;

  res = device_create_file(display_pipeline_data->class_device, &dev_attr_info);
  res += device_create_file(display_pipeline_data->class_device, &dev_attr_display_timing);
  res += device_create_file(display_pipeline_data->class_device, &dev_attr_source_timing);
  res += device_create_file(display_pipeline_data->class_device, &output_dev_attr_OUTPUT_CTRL_DAC123_MAX_VOLTAGE.dev_attr);
  res += device_create_file(display_pipeline_data->class_device, &output_dev_attr_OUTPUT_CTRL_DAC456_MAX_VOLTAGE.dev_attr);
  res += device_create_file(display_pipeline_data->class_device, &output_dev_attr_OUTPUT_CTRL_CHROMA_SCALE.dev_attr);
  res += device_create_file(display_pipeline_data->class_device, &output_dev_attr_OUTPUT_CTRL_CLOCK_ADJUSTMENT.dev_attr);
  res += device_create_file(display_pipeline_data->class_device, &output_dev_attr_OUTPUT_CTRL_CVBS_TRAP_FILTER.dev_attr);
  res += device_create_file(display_pipeline_data->class_device, &output_dev_attr_OUTPUT_CTRL_LUMA_NOTCH_FILTER.dev_attr);
  res += device_create_file(display_pipeline_data->class_device, &output_dev_attr_OUTPUT_CTRL_IF_LUMA_DELAY.dev_attr);
  res += device_create_file(display_pipeline_data->class_device, &output_dev_attr_OUTPUT_CTRL_WSS_INSERTION.dev_attr);
  res += device_create_file(display_pipeline_data->class_device, &output_dev_attr_OUTPUT_CTRL_CHROMA_DELAY.dev_attr);
  res += device_create_file(display_pipeline_data->class_device, &output_dev_attr_OUTPUT_CTRL_VIDEO_OUT_CALIBRATION.dev_attr);
  res += device_create_file(display_pipeline_data->class_device, &output_dev_attr_OUTPUT_CTRL_DISPLAY_ASPECT_RATIO.dev_attr);

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
#if !defined(KBUILD_VERSION)
#define KBUILD_VERSION "<unknown>"
#endif
#if !defined(KBUILD_DATE)
#define KBUILD_DATE "<unknown>"
#endif

#if (LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 34) )
static ssize_t stmcore_show_driver_info(struct class *class, char *buf)
#else
static ssize_t stmcore_show_driver_info(struct class *class, struct class_attribute *attr, char *buf)
#endif
{
  int sz=0;

  sz += snprintf(&buf[sz], PAGE_SIZE - sz, "Build Source: " KBUILD_SOURCE "\n");
  sz += snprintf(&buf[sz], PAGE_SIZE - sz, "Build Version:" KBUILD_VERSION "\n");
  sz += snprintf(&buf[sz], PAGE_SIZE - sz, "Build User:   " KBUILD_USER "\n");
  sz += snprintf(&buf[sz], PAGE_SIZE - sz, "Build Date:   " KBUILD_DATE "\n");
  sz += snprintf(&buf[sz], PAGE_SIZE - sz, "Build System: " KBUILD_SYSTEM_INFO "\n");

  return sz;
}


static CLASS_ATTR(info,S_IRUGO, stmcore_show_driver_info,NULL);
#endif


/******************************************************************************
 * Initialization
 */
static inline bool stmcore_display_runtime_init ( int device_id )
{
    display_pipeline_data[device_id].display_runtime = kzalloc(sizeof(struct stmcore_display),GFP_KERNEL);
    if(!display_pipeline_data[device_id].display_runtime)
      return false;

    spin_lock_init(&(display_pipeline_data[device_id].display_runtime->spinlock));
    rwlock_init (&display_pipeline_data[device_id].display_runtime->vsync_cb_list.lock);
    INIT_LIST_HEAD (&display_pipeline_data[device_id].display_runtime->vsync_cb_list.list);

    display_pipeline_data[device_id].display_runtime->min_display_processing_us = LLONG_MAX;
    display_pipeline_data[device_id].display_runtime->min_display_thread_processing_us = LLONG_MAX;

    if(stm_display_device_open_output(display_pipeline_data[device_id].device, display_pipeline_data[device_id].main_output_id,
                                     &(display_pipeline_data[device_id].display_runtime->main_output)) != 0)
    {
      /* We must have at least the main output in order to change mode! */
      printk(KERN_ERR "platform configuration error: failed to find the main video output\n");
      return false;
    }

    if(stm_display_output_get_timing_identifier(display_pipeline_data[device_id].display_runtime->main_output,
                                             &display_pipeline_data[device_id].display_runtime->main_output_timing_id)<0)
    {
      return false;
    }

    if(display_pipeline_data[device_id].vtg_irq != -1)
    {
      atomic_set (&display_pipeline_data[device_id].display_runtime->vsync_count, 0);
      init_waitqueue_head(&display_pipeline_data[device_id].display_runtime->vsync_wait_queue);
      display_pipeline_data[device_id].display_runtime->vsync_status = STMCORE_VSYNC_STATUS_IDLE;

      snprintf (display_pipeline_data[device_id].vtg_name, sizeof (display_pipeline_data[device_id].vtg_name),
              "vsync%d", device_id);
      printk(KERN_INFO "Installing Interrupt Handler %s for IRQ %d\n", display_pipeline_data[device_id].vtg_name, display_pipeline_data[device_id].vtg_irq);

      if (STM_VIBE_USE_THREADED_IRQ)
      {
          if(request_threaded_irq(display_pipeline_data[device_id].vtg_irq,
                         stmcore_vsync_interrupt,
                         stmcore_vsync_interrupt_thread_fn,
                         IRQF_DISABLED,
                         display_pipeline_data[device_id].vtg_name,
                         &display_pipeline_data[device_id]) < 0)
            return false;
      } else {
          if(request_irq(display_pipeline_data[device_id].vtg_irq,
                         stmcore_vsync_interrupt,
                         IRQF_DISABLED,
                         display_pipeline_data[device_id].vtg_name,
                         &display_pipeline_data[device_id]) < 0)
            return false;
      }
    }

    return true;
}


static inline bool stmcore_source_runtime_init ( int device_id )
{
  int source_index = 0;
  struct stmcore_source_data *source_data;

  /* In case of no sources just exist whithout doing nothing */
  if(display_pipeline_data[device_id].sources_nbr <= 0)
    return true;

  for(source_index = 0; source_index < display_pipeline_data[device_id].sources_nbr; source_index++)
  {
    source_data    = &(display_pipeline_data[device_id].sources[source_index]);

    if(stm_display_device_open_source(display_pipeline_data[device_id].device, (uint32_t )source_data->source_id,&(source_data->source_handle)) != 0)
    {
      /* We must have at least the main source ! */
      printk(KERN_ERR "platform configuration error: failed to find the main video source\n");
      return false;
    }

    if(stm_display_source_get_timing_identifier(source_data->source_handle, &(source_data->source_timing_id))<0)
    {
      /* We must have at least the main source timing id! */
      printk(KERN_ERR "platform configuration error: failed to find the main video source timing id\n");
      return false;
    }

    source_data->source_runtime = kzalloc(sizeof(struct stmcore_source),GFP_KERNEL);
    if(!source_data->source_runtime)
      return false;

    spin_lock_init(&(source_data->source_runtime->spinlock));
    rwlock_init (&(source_data->source_runtime->source_vsync_cb_list.lock));
    INIT_LIST_HEAD (&(source_data->source_runtime->source_vsync_cb_list.list));

    source_data->source_runtime->min_source_processing_us = LLONG_MAX;

    if(source_data->source_irq != -1)
    {
      atomic_set (&(source_data->source_runtime->source_vsync_count), 0);
      init_waitqueue_head(&(source_data->source_runtime->source_vsync_wait_queue));

      printk(KERN_INFO "Installing Interrupt Handler %s for IRQ %d\n", source_data->source_name, source_data->source_irq);
      if(request_irq(source_data->source_irq,
                     stmcore_source_vsync_interrupt,
                     IRQF_DISABLED,
                     source_data->source_name,
                     &display_pipeline_data[device_id]) < 0)
        return false;
    }
  }

  return true;
}


/******************************************************************************
 * Power management
 */
static int stmcore_display_set_power_state(struct platform_device *pdev, pm_message_t state)
{
  int retval = 0;

  if(!display_pipeline_data[pdev->id].device)
    return -ENODEV;

  switch(state.event) {
    case PM_EVENT_RESUME:
    case PM_EVENT_RESTORE:
      {
        DPRINTK(" %s...\n", (state.event == PM_EVENT_RESUME) ? "Resuming" : "Restoring");
#ifndef CONFIG_ARCH_STI
        /* Power on platform resources */
        stmcore_display_device_set_pm(&pdev->dev, state.event);
#endif
        retval = stm_display_device_resume(display_pipeline_data[pdev->id].device);

        DPRINTK( " OK.\n");
      }
      break;
    case PM_EVENT_SUSPEND:
      {
        DPRINTK(" Suspending...\n");

        retval = stm_display_device_suspend(display_pipeline_data[pdev->id].device);
#ifndef CONFIG_ARCH_STI
        /* Power off platform resources */
        stmcore_display_device_set_pm(&pdev->dev, state.event);
#endif
        DPRINTK(" OK\n");
      }
      break;
    case PM_EVENT_FREEZE:
      {
        DPRINTK(" Freeze...\n");

        retval = stm_display_device_freeze(display_pipeline_data[pdev->id].device);
#ifndef CONFIG_ARCH_STI
        /* Power off platform resources */
        stmcore_display_device_set_pm(&pdev->dev, state.event);
#endif
        DPRINTK(" OK\n");
      }
      break;
    case PM_EVENT_ON:
    case PM_EVENT_THAW:
    default :
      DPRINTK( " Unsupported PM event!\n");
      retval = -EINVAL;
      break;
  }

  return retval;
}

#ifndef CONFIG_ARCH_STI
static int stmcore_display_suspend(struct device *dev)
{
  struct platform_device *pdev = to_platform_device(dev);

  return stmcore_display_set_power_state(pdev,PMSG_SUSPEND);
}

static int stmcore_display_resume(struct device *dev)
{
  struct platform_device *pdev = to_platform_device(dev);

  return stmcore_display_set_power_state(pdev,PMSG_RESUME);
}
#endif

static int stmcore_display_freeze(struct device *dev)
{
  struct platform_device *pdev = to_platform_device(dev);

  return stmcore_display_set_power_state(pdev,PMSG_FREEZE);
}

static int stmcore_display_restore(struct device *dev)
{
  struct platform_device *pdev = to_platform_device(dev);

  return stmcore_display_set_power_state(pdev,PMSG_RESTORE);
}

const struct dev_pm_ops stmcore_display_pm_ops = {
#ifdef CONFIG_ARCH_STI
  .suspend         = stmcore_display_freeze,
  .resume          = stmcore_display_restore,
  .freeze          = stmcore_display_freeze,
  .thaw            = stmcore_display_restore,
  .poweroff        = stmcore_display_freeze,
  .restore         = stmcore_display_restore,
#ifdef CONFIG_PM_RUNTIME
  .runtime_suspend = stmcore_display_freeze,
  .runtime_resume  = stmcore_display_restore,
  .runtime_idle    = NULL,
#endif
#else
  .suspend         = stmcore_display_suspend,
  .resume          = stmcore_display_resume,
  .freeze          = stmcore_display_freeze,
  .thaw            = stmcore_display_resume,
  .poweroff        = stmcore_display_suspend,
  .restore         = stmcore_display_restore,
#ifdef CONFIG_PM_RUNTIME
  .runtime_suspend = stmcore_display_suspend,
  .runtime_resume  = stmcore_display_resume,
  .runtime_idle    = NULL,
#endif
#endif
};

/*
 * Setting clocks from device tree.
 */
int stmcore_display_set_clocks(struct device *dev)
{
    struct device_node * n = dev->of_node;
    const char* clock_name = NULL;
    int i = 0;
    u32 freq = 0;
    struct clk * c = NULL;

    dev_dbg(dev, "using node %p\n", n);

    for (i = 0; of_property_read_string_index(n, "clock-names", i, &clock_name)==0; i++)
    {
        dev_dbg(dev,"clock-names %d = %s\n", i, clock_name);
        if (of_property_read_u32_index(n, "clock-frequency",
            i, &freq)==0)
            {
                dev_dbg(dev, "clock-frequency %d = %d\n", i, freq);
                if (freq)
                {
                    c = clk_get(dev, clock_name);
                    if (!IS_ERR_OR_NULL(c))
                    {
                        clk_set_rate(c, freq);
                        clk_put(c);
                        c = NULL;
                    }
                    else
                    {
                        dev_err(dev, "can't find clk %s (return=%p)\n", clock_name, c);
                    }
                    freq = 0;
                }
            }
            else
            {
                break;
            }
    }
    return 0;
}


/* see part of explanation of ARM interrupts in device tree in
   kernel/Documentation/devicetree/bindings/arm/gic.txt */
#define IRQ_OFFSET 32 /* interrupt number offset */
/* in .dts file, interrupt property is <0 x 0> */
#define INTERRUPT_DESCRIPTORS_NBR 3 /* number of integers to describe an interrupt*/
#define PIPE_NAME_LENGTH 25

/*
 * return 0 for no error, -ENODEV if pipe is disabled, -EINVAL if invalid
 * device tree.
 */
int stmcore_display_fill_pipe_data(struct device *dev, struct device_node *np,
        struct stmcore_display_pipeline_data *pipe)
{
    unsigned int temp_array[INTERRUPT_DESCRIPTORS_NBR];
    const char *s = NULL;
    unsigned int i = 0;
    size_t l = 0;
    int whitelist_size_bytes = 0;
    struct device_node *sources_node = NULL;
    struct device_node *source_node = NULL;
    struct device_node *planes_node = NULL;
    struct device_node *plane_node = NULL;

    DPRINTK_NOPREFIX("stmcore_display_fill_pipe_data\n");

    pipe->owner = THIS_MODULE;

    of_property_read_string(np, "display-pipe", &s );
    if (s) {
        l = strlen(s);
        l = (l<=PIPE_NAME_LENGTH)?l:PIPE_NAME_LENGTH;
        pipe->name = kzalloc(l+1, GFP_KERNEL);
        strncpy(pipe->name, s, PIPE_NAME_LENGTH);
    } else {
        DPRINTK("missing display-pipe property for display\n");
        return -EINVAL;
    }
    DPRINTK_NOPREFIX("pipe->name=%s\n", pipe->name);

    pipe->device = 0;
    pipe->device_id = 0;

    of_property_read_u32(np, "device", &pipe->device_id);
    of_property_read_u32(np, "device-id", &pipe->device_id);
    DPRINTK_NOPREFIX("pipe->device_id=%d\n", pipe->device_id);

    memset(temp_array, 0, sizeof temp_array);
    if (of_property_read_u32_array(np, "interrupts", temp_array,
            INTERRUPT_DESCRIPTORS_NBR) != 0) {
        DPRINTK("missing interrupts property for display\n");
        return -EINVAL;
    }
    pipe->vtg_irq = temp_array[1] + IRQ_OFFSET; /* interrupt = <0 x 0>; */
    DPRINTK_NOPREFIX("pipe->vtg_irq=%d\n", pipe->vtg_irq);

    of_property_read_u32(np, "main-output-id", &pipe->main_output_id);
    DPRINTK_NOPREFIX("pipe->main_output_id=%d\n", pipe->main_output_id);
    of_property_read_u32(np, "hdmi-output-id", &pipe->hdmi_output_id);
    DPRINTK_NOPREFIX("pipe->hdmi_output_id=%d\n", pipe->hdmi_output_id);
    of_property_read_u32(np, "dvo-output-id", &pipe->dvo_output_id);
    DPRINTK_NOPREFIX("pipe->dvo_output_id=%d\n", pipe->dvo_output_id);

#ifdef CONFIG_ARCH_STI
    pipe->dvo_pinctrl = (pipe->dvo_output_id == -1)?NULL:devm_pinctrl_get(dev);
    pipe->dvo_pins_default  = NULL;
#else
    pipe->dvo_pad_config = (pipe->dvo_output_id == -1)?NULL:stm_of_get_pad_config_from_node(dev, np, 0);
    pipe->dvo_pad_state  = NULL;
#endif

    of_property_read_u32(np, "io-offset", &i);
    pipe->io_offset = i;
    DPRINTK_NOPREFIX("pipe->io_offset=%ld\n", pipe->io_offset);

    i = 0;
    pipe->whitelist_size = 0;
    if (of_get_property(np, "whitelist", (int *)&whitelist_size_bytes)) {
        pipe->whitelist_size = whitelist_size_bytes / sizeof(*pipe->whitelist);
        DPRINTK_NOPREFIX( "whitelist_size_bytes=%d "
                "sizeof(*pipe->whitelist)=%d pipe->whitelist_size=%lu\n",
                whitelist_size_bytes, sizeof(*pipe->whitelist),
                pipe->whitelist_size);

        pipe->whitelist = kzalloc(whitelist_size_bytes,
                GFP_KERNEL);
        if (!pipe->whitelist) {
            printk(KERN_ERR "can't allocate memory for display pipe whitelist\n");
        } else {
            of_property_read_u32_array(np, "whitelist", (u32 *)pipe->whitelist,
                pipe->whitelist_size);
            for(i=0; i<pipe->whitelist_size; i++)
                DPRINTK_NOPREFIX("whitelist[%d]=%lx\n", i, pipe->whitelist[i]);
        }
    } else {
        DPRINTK_NOPREFIX("no whitelist found.\n");
    }
    DPRINTK_NOPREFIX("whitelist_size=%lu\n", pipe->whitelist_size);

    sources_node = of_get_child_by_name(np, "sources");
    source_node = NULL;
    i = 0;
    pipe->sources_nbr = 0;
    if (!sources_node) {
        DPRINTK_NOPREFIX("no sources found\n");
    } else {
        for( i = 0; (source_node = of_get_next_child(sources_node, source_node))
                && i<STMCORE_MAX_SOURCES; i++ ) {
            of_property_read_u32(source_node, "source-id",
                    &pipe->sources[i].source_id);
            DPRINTK_NOPREFIX("pipe->sources[%d].source_id=%d\n", i,
                    pipe->sources[i].source_id);
            of_property_read_string(source_node, "source-name", &s );
            strncpy(pipe->sources[i].source_name, s, STMCORE_NAME_MAX_LENGTH);
            pipe->sources[i].source_name[STMCORE_NAME_MAX_LENGTH - 1] = 0;
            DPRINTK_NOPREFIX("pipe->sources[%d].source_name=%s\n",
                    i, pipe->sources[i].source_name);
            memset(temp_array, 0, sizeof temp_array);
            of_property_read_u32_array(source_node, "interrupts",
                    temp_array, INTERRUPT_DESCRIPTORS_NBR);
            /* in .dts file, interrupt property is <0 x 0> */
            pipe->sources[i].source_irq = temp_array[1] + IRQ_OFFSET;
            DPRINTK_NOPREFIX("pipe->sources[%d].source_irq=%d\n",
                    i, pipe->sources[i].source_irq);
            of_node_put(source_node); /* free the node */
        }
        of_node_put(sources_node);
    }
    pipe->sources_nbr = i;
    DPRINTK_NOPREFIX("pipe->sources_nbr=%d\n", pipe->sources_nbr);

    planes_node = of_get_child_by_name(np, "planes");
    plane_node = NULL;
    i = 0;
    if (!planes_node) {
        DPRINTK_NOPREFIX("can't find planes node\n");
    } else {
        for (i = 0; (plane_node = of_get_next_child(planes_node, plane_node))
                && i < STMCORE_MAX_PLANES; i++) {
            of_property_read_u32(plane_node, "id", &pipe->planes[i].id);
            DPRINTK_NOPREFIX("pipe->planes[%d].id=%d\n", i,
                    pipe->planes[i].id);
            of_property_read_u32(plane_node, "flags", &pipe->planes[i].flags);
            DPRINTK_NOPREFIX("pipe->planes[%d].flags=%d\n", i,
                    pipe->planes[i].flags);
            of_node_put(plane_node); /* free the node */
        }
        of_node_put(planes_node);
        if (i < STMCORE_MAX_PLANES) {
            pipe->planes[i].id = -1;
            pipe->planes[i].flags = 0;
        }
    }
    return 0;
}

void stmcore_display_dt_get_pdata(
        struct platform_device *pdev,
        struct stmcore_display_pipeline_data **pp,
        int *n)
{
    struct device_node *np = pdev->dev.of_node;
    struct stmcore_display_pipeline_data *pipe_data;
    struct device_node *pipe_node = NULL;
    int nb_pipes = 0;
    int i = 0;
    int r = 0;
    const char *s;

    DPRINTK_NOPREFIX("stmcore_display_dt_get_pdata\n");

    /* count the number of display pipes */
    while ( (pipe_node = of_get_next_child(np, pipe_node)) ) {
        s=NULL;
        of_property_read_string(pipe_node, "display-pipe", &s);
        if(s)
          nb_pipes++;
        of_node_put(pipe_node);
    }
    DPRINTK_NOPREFIX("nb_pipes=%d\n", nb_pipes);
    pipe_data = kzalloc(sizeof(struct stmcore_display_pipeline_data)*nb_pipes, GFP_KERNEL);
    if (!pipe_data) {
        printk(KERN_ERR "out of memory for pipe_data\n");
        return;
    }

    /*
     * Currently the kernel DT framework is creating only one display device
     * instance. So we are going to hard code the device id value to 0.
     *
     * Note that this device id is ONLY used by the PM hooks to properly setup
     * the power state of the concerned linux device. This allows powering off
     * specific device. Previously with the NON DT version of display driver we
     * were creating a platform device for each available display device (see
     * stmcore_display_create_power_devices() function) than we were using the
     * platform_device 'pdev->id' field to setup the harware ressources which
     * belongs to the concerned device (see stmcore_display_set_power_state()
     * function).
     */
    pdev->id = 0;

    r = 0;
    i = 0;
    pipe_node = of_get_next_child(np, NULL);
    while ( (pipe_node != NULL) && (i < nb_pipes) ) {
        s = NULL;
        of_property_read_string(pipe_node, "display-pipe", &s);
        if(s)
        {
          r = stmcore_display_fill_pipe_data(&(pdev->dev),pipe_node, &pipe_data[i]);
          if (r != 0) {
              printk(KERN_ERR "error %d in display pipe %d device tree\n", r, i);
              of_node_put(pipe_node);
              break;
          }
          i++;
        }
        of_node_put(pipe_node);
        pipe_node = of_get_next_child(np, pipe_node);
    }

    if (r == 0) {
        *pp = pipe_data;
        *n = nb_pipes;
    } else {
        *pp = NULL;
        *n = 0;
        if (pipe_data)
            kfree(pipe_data);
    }
    DPRINTK_NOPREFIX("stmcore_display_dt_get_pdata: nb_pipes=%d\n", *n);
}

#ifdef CONFIG_PM_RUNTIME
static int stmcore_display_pm_runtime_get(const uint32_t id)
{
  if(!display_pipeline_data[id].pdev)
  {
    printk(KERN_ERR "%s: invalid platform device\n", __FUNCTION__);
    return -ENODEV;
  }

  DPRINTK_NOPREFIX("%s: id=%d, dev=%p\n", __FUNCTION__, id, &display_pipeline_data[id].pdev->dev);
  return pm_runtime_get_sync(&display_pipeline_data[id].pdev->dev);
}

static int stmcore_display_pm_runtime_put(const uint32_t id)
{
  if(!display_pipeline_data[id].pdev)
  {
    printk(KERN_ERR "%s: invalid platform device\n", __FUNCTION__);
    return -ENODEV;
  }

  DPRINTK_NOPREFIX("%s: id=%d, dev=%p\n", __FUNCTION__, id, &display_pipeline_data[id].pdev->dev);
  return pm_runtime_put_sync(&display_pipeline_data[id].pdev->dev);
}
#endif

#define STM_HDMI_MAX_TMDS_NODES 10

static stm_display_hdmi_phy_config_t hdmiphy_config[STM_HDMI_MAX_TMDS_NODES];

static void stmcore_set_hdmi_phy_config(struct stmcore_display_pipeline_data *pd)
{
    struct device_node *hdmitx_root_node_p = NULL;
    struct device_node *hdmirepeater_root_node_p = NULL;
    struct device_node *hdmi_phy_config_node_p = NULL;
    char tmds_name[STM_HDMI_MAX_TMDS_NODES];
    struct device_node *hdmi_tmds_node_p[STM_HDMI_MAX_TMDS_NODES];
    uint32_t i, num_tmds = 0;
    uint32_t freq[2];
    uint32_t config[4];
    int res;
    stm_display_output_h hdmi_output;

    if (pd->hdmi_output_id == -1)
        return;

    /* Get HDMI Phy Configuration from Device Tree */

    hdmitx_root_node_p = of_find_compatible_node(NULL, NULL, "st,hdmi");
    hdmirepeater_root_node_p = of_find_compatible_node(NULL, NULL, "st,hdmi-repeater");

    if ((!hdmitx_root_node_p)&& (!hdmirepeater_root_node_p)) {
        printk(KERN_ERR "Failed to get hdmi(tx)/(repeater)_root_node_p");
        return;
    }

    if (hdmitx_root_node_p){
        hdmi_phy_config_node_p = of_get_child_by_name(hdmitx_root_node_p, "hdmi-phy-config");
    } else {
        hdmi_phy_config_node_p = of_get_child_by_name(hdmirepeater_root_node_p, "hdmi-phy-config");
    }

    if (!hdmi_phy_config_node_p) {
        printk(KERN_ERR "Failed to get hdmi_phy_config_node_p");
        return;
    }

    do {
        snprintf(tmds_name, sizeof(tmds_name), "tmds%d", num_tmds);

        hdmi_tmds_node_p[num_tmds] = of_get_child_by_name(hdmi_phy_config_node_p, tmds_name);

        if (hdmi_tmds_node_p[num_tmds] == NULL)
            break;

        num_tmds++;
    } while (num_tmds < STM_HDMI_MAX_TMDS_NODES);

    for (i = 0; i < num_tmds; i++) {
        of_property_read_u32_array(hdmi_tmds_node_p[i], "freq", freq, 2);
        of_property_read_u32_array(hdmi_tmds_node_p[i], "config", config, 4);

        hdmiphy_config[i].min_tmds_freq = freq[0];
        hdmiphy_config[i].max_tmds_freq = freq[1];

        memcpy(&hdmiphy_config[i].config, &config, sizeof(config[0]) * ARRAY_SIZE(config));
    }

    /* Set HDMI Phy Configuration on HDMI O/P */

    res = stm_display_device_open_output(pd->device, pd->hdmi_output_id, &hdmi_output);

    if (res) {
        printk(KERN_ERR "%s: unable to open hdmi output -> Error %d\n", __FUNCTION__, res);
        return;
    }

    res = stm_display_output_set_compound_control(hdmi_output,
                                                  OUTPUT_CTRL_HDMI_PHY_CONF_TABLE,
                                                  hdmiphy_config);
    if (res) {
        printk(KERN_ERR "%s: failed to set hdmi phy config -> Error %d. "
               "This might result in NO HDMI output ", __FUNCTION__, res);
        return;
    }

    stm_display_output_close(hdmi_output);
}


int stmcore_display_probe(struct platform_device *pdev)
{
    int r = 0;
    int i = 0;
    int nvalidpipelines = 0;
    int power_down_dacs=0;

    TRC( TRC_ID_MAIN_INFO," ");
    if (!pdev->dev.of_node) {
        TRC( TRC_ID_ERROR,"of_node==NULL");
        return 0;
    }

    if (stmcore_probe_device(pdev, &display_pipeline_data,
            &nr_display_pipelines)<0)
        return -ENODEV;

    stmcore_class = class_create(THIS_MODULE,"stmcoredisplay");
    if (IS_ERR(stmcore_class))
    {
      TRC(TRC_ID_ERROR, "unable to create class device");
      return -ENOMEM;
    }

#if defined(SDK2_ENABLE_DISPLAY_ATTRIBUTES)
    if (class_create_file(stmcore_class,&class_attr_info)<0)
    {
      TRC(TRC_ID_ERROR, "unable to create class device info attr");
      return -ENOMEM;
    }
#endif

#ifdef CONFIG_PM_RUNTIME
    /* Setup PM runtime data */
    pm_runtime_set_active(&pdev->dev);
    //pm_suspend_ignore_children(&pdev->dev, 1);
    pm_runtime_enable(&pdev->dev);
#endif

    /* Only display device 0 is created. */
    /* Must be a device tree info in the futur instead of being hardcoded. */
    if (stm_display_device_create(display_data.device_id, &(display_data.device_handle)) != 0)
    {
      TRC(TRC_ID_ERROR, "failed to get display device! No BPA2 bigphysarea compatibility partition?");
      stmcore_display_exit();
      return -ENODEV;
    }

    for (i=0; i<nr_display_pipelines; i++)
    {
      /* Save device information in display pipeline, */
      /* waiting that boths are intergrated in one structure */
      display_pipeline_data[i].device_id = display_data.device_id;
      display_pipeline_data[i].device    = display_data.device_handle;

      if (!stmcore_display_runtime_init(i))
      {
        /* continue with following pipeline(s) */
        continue;
      }

      if (!stmcore_source_runtime_init(i))
      {
        /* continue with following pipeline(s) */
        continue;
      }

#ifdef CONFIG_ARCH_STI
      if ((display_pipeline_data[i].dvo_output_id != -1)  && display_pipeline_data[i].dvo_pinctrl)
      {
        display_pipeline_data[i].dvo_pins_default = pinctrl_lookup_state( display_pipeline_data[i].dvo_pinctrl,
                                                                          PINCTRL_STATE_DEFAULT );
        if (IS_ERR(display_pipeline_data[i].dvo_pins_default))
        {
          TRC(TRC_ID_ERROR, "Failed to claim dvo pads on pipeline %d", i);
          devm_pinctrl_put(display_pipeline_data[i].dvo_pinctrl);
          display_pipeline_data[i].dvo_pinctrl = NULL;
          /* continue with following pipeline(s) */
          continue;
        }
        else pinctrl_select_state(display_pipeline_data[i].dvo_pinctrl, display_pipeline_data[i].dvo_pins_default);
      }
#else
      if ((display_pipeline_data[i].dvo_output_id != -1)  && display_pipeline_data[i].dvo_pad_config)
      {
        if ((display_pipeline_data[i].dvo_pad_state = stm_pad_claim(display_pipeline_data[i].dvo_pad_config, dev_name(&(pdev->dev))))==NULL)
        {
          TRC(TRC_ID_ERROR, "Failed to claim dvo pads on pipeline %d\n", i);
          display_pipeline_data[i].dvo_pad_config = NULL;
          /* continue with following pipeline(s) */
          continue;
        }
      }
#endif

      display_pipeline_data[i].class_device = device_create(stmcore_class,
                                                  NULL, MKDEV(0,0), &display_pipeline_data[i],
                                                  "display%d",i);

      if(IS_ERR(display_pipeline_data[i].class_device))
      {
        TRC(TRC_ID_ERROR, "Failed to create class device %d",i);
        /* continue with following pipeline(s) */
        continue;
      }

#if defined(SDK2_ENABLE_DISPLAY_ATTRIBUTES)
      /*
       * Setup a class device, needed now so a HDMI device can hang off it
       */
      if (stmcore_classdev_create_files(i,&display_pipeline_data[i])<0)
      {
        TRC(TRC_ID_ERROR, "Failed to init class device %d\n",i);
        /* continue with following pipeline(s) */
        continue;
      }
#endif

      stmcore_set_hdmi_phy_config(&display_pipeline_data[i]);

      stmcore_display_postinit(&display_pipeline_data[i]);

      nvalidpipelines++;
#ifdef CONFIG_PM_RUNTIME
      display_pipeline_data[i].pdev = pdev;
#endif
    }

    TRC( TRC_ID_MAIN_INFO,"nvalidpipelines=%d", nvalidpipelines);
    if (nvalidpipelines == 0)
    {
      TRC(TRC_ID_ERROR, "No valid display pipelines available");
      stmcore_display_exit();
      return -ENODEV;
    }

    /* Get DACs Power status */
    power_down_dacs = of_property_read_bool(pdev->dev.of_node,"video-dacs-power-down");
    TRC( TRC_ID_MAIN_INFO,"video-dacs-power-down=%d", power_down_dacs);

    /* Power Down Video DACs */
    if(power_down_dacs)
    {
      TRC(TRC_ID_MAIN_INFO, "Powering down video dacs");
      if(stm_display_device_power_down_video_dacs(display_data.device_handle)<0)
        TRC(TRC_ID_ERROR, "Failed to powering down video dacs !!");
    }

#ifdef CONFIG_PM_RUNTIME
    /* Register Get/Put device ops */
    stm_display_device_register_pm_runtime_hooks(display_data.device_handle, stmcore_display_pm_runtime_get, stmcore_display_pm_runtime_put);
    pm_runtime_suspend(&pdev->dev);
#endif

    return r;
}

int stmcore_display_remove(struct platform_device *pdev)
{
#ifdef CONFIG_PM_RUNTIME
    /*
     * Missing pm_runtime_disable call in driver remove path caused
     * an "Unbalanaced pm_runtime_enable" warning when driver is reloaded.
     */
    TRC( TRC_ID_MAIN_INFO,"");
    pm_runtime_disable(&pdev->dev);
#endif
    return 0;
}


static struct of_device_id stm_display_match[] = {
    {
        .compatible = "st,display",
    },
    {},
};

static struct platform_driver stmcore_display_driver = {
   .probe = stmcore_display_probe,
   .remove = stmcore_display_remove,
   .driver   = {
       .name     = "stmcore-display",
       .owner    = THIS_MODULE,
       .pm       = &stmcore_display_pm_ops,
       .of_match_table = of_match_ptr(stm_display_match),
   }
};


/*******************************************************************************
 * Initial module entrypoint.
 */
int __init stmcore_display_init(void)
{
  printk(KERN_INFO "stmcore_display_init\n");

  if (platform_driver_register(&stmcore_display_driver)) {
    printk(KERN_ERR "stmcore-display: Unable to register CoreDisplay platform driver");
    return -ENODEV;
  } else {
    printk(KERN_INFO "stmcore-display: Registered CoreDisplay platform driver\n");
    stmcore_driver = &stmcore_display_driver;
  }

  return 0;
}


/******************************************************************************
 * Public Linux platform API to the coredisplay module
 */
int stmcore_get_display_pipeline(int pipeline, struct stmcore_display_pipeline_data *data)
{
  if((pipeline < 0) || (pipeline >= nr_display_pipelines))
    return -ENODEV;

  memcpy(data, &display_pipeline_data[pipeline], sizeof(struct stmcore_display_pipeline_data));

  return 0;
}


int stmcore_register_vsync_callback(struct stmcore_display *runtime, struct stmcore_vsync_cb *cb)
{
  unsigned long flags;

  if (runtime == NULL)
  {
    printk(KERN_ERR "%s: error: runtime NULL\n", __FUNCTION__);
    return -EINVAL;
  }

  if (!cb || !cb->cb )
  {
    printk(KERN_ERR "%s: error: incorrect cb\n", __FUNCTION__);
    return -EINVAL;
  }

  if (cb->owner && !try_module_get(cb->owner))
  {
    printk(KERN_ERR "%s: error: can't get cb owner\n", __FUNCTION__);
    return -ENODEV;
  }

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

  if(runtime == NULL)
    return -EINVAL;

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

  if(inList)
  {
    write_lock_irqsave (&runtime->vsync_cb_list.lock, flags);
    list_del_init (&cb->node);
    write_unlock_irqrestore (&runtime->vsync_cb_list.lock, flags);

    if(cb->owner)
      module_put(cb->owner);

    return 0;
  } else {
    printk(KERN_ERR "%s: error: can't find callback in list (inList == 0)\n", __FUNCTION__);
    return -EINVAL;
  }
}


int stmcore_register_source_vsync_callback(struct stmcore_source *runtime, struct stmcore_vsync_cb *cb)
{
  unsigned long flags;

  if (runtime == NULL)
  {
    printk(KERN_ERR "%s: error: runtime == NULL\n", __FUNCTION__);
    return -EINVAL;
  }
  if (!cb || !cb->cb)
  {
    printk(KERN_ERR "%s: error: incorrect cb\n", __FUNCTION__);
    return -EINVAL;
  }

  if(cb->owner && !try_module_get(cb->owner))
  {
    printk(KERN_ERR "%s: error: can't get cb owner\n", __FUNCTION__);
    return -ENODEV;
  }
  write_lock_irqsave (&runtime->source_vsync_cb_list.lock, flags);
  list_add_tail (&cb->node, &runtime->source_vsync_cb_list.list);
  write_unlock_irqrestore (&runtime->source_vsync_cb_list.lock, flags);
  return 0;
}


int stmcore_unregister_source_vsync_callback(struct stmcore_source *runtime, struct stmcore_vsync_cb *cb)
{
  struct stmcore_vsync_cb *pCallback, *tmp;
  unsigned long flags;
  int inList;

  if (runtime == NULL)
    return -EINVAL;

  inList = 0;
  read_lock_irqsave (&runtime->source_vsync_cb_list.lock, flags);
  list_for_each_entry_safe (pCallback, tmp, &runtime->source_vsync_cb_list.list, node)
  {
    if(pCallback == cb)
    {
      inList = 1;
      break;
    }
  }
  read_unlock_irqrestore (&runtime->source_vsync_cb_list.lock, flags);

  if(inList)
  {
    write_lock_irqsave (&runtime->source_vsync_cb_list.lock, flags);
    list_del_init (&cb->node);
    write_unlock_irqrestore (&runtime->source_vsync_cb_list.lock, flags);

    if(cb->owner)
    {
      module_put(cb->owner);
    }
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
MODULE_VERSION(KBUILD_VERSION);

EXPORT_SYMBOL(stmcore_get_display_pipeline);
EXPORT_SYMBOL(stmcore_register_vsync_callback);
EXPORT_SYMBOL(stmcore_unregister_vsync_callback);
EXPORT_SYMBOL(stmcore_register_source_vsync_callback);
EXPORT_SYMBOL(stmcore_unregister_source_vsync_callback);

/* device */
EXPORT_SYMBOL(stm_display_open_device);
EXPORT_SYMBOL(stm_display_device_find_outputs_with_capabilities);
EXPORT_SYMBOL(stm_display_device_open_output);
EXPORT_SYMBOL(stm_display_device_find_planes_with_capabilities);
EXPORT_SYMBOL(stm_display_device_open_plane);
EXPORT_SYMBOL(stm_display_device_open_source);
EXPORT_SYMBOL(stm_display_device_get_number_of_tuning_services);
EXPORT_SYMBOL(stm_display_device_get_tuning_caps);
EXPORT_SYMBOL(stm_display_device_set_tuning);
EXPORT_SYMBOL(stm_display_device_close);
/* source */
EXPORT_SYMBOL(stm_display_source_get_control);
EXPORT_SYMBOL(stm_display_source_get_name);
EXPORT_SYMBOL(stm_display_source_get_device_id);
EXPORT_SYMBOL(stm_display_output_get_timing_identifier);
EXPORT_SYMBOL(stm_display_source_get_connected_plane_id);
EXPORT_SYMBOL(stm_display_source_get_connected_plane_caps);
EXPORT_SYMBOL(stm_display_source_get_interface);
EXPORT_SYMBOL(stm_display_source_get_last_timing_event);
EXPORT_SYMBOL(stm_display_source_get_timing_identifier);
EXPORT_SYMBOL(stm_display_source_close);
EXPORT_SYMBOL(stm_display_source_set_control);
EXPORT_SYMBOL(stm_display_source_get_capabilities);
EXPORT_SYMBOL(stm_display_source_get_status);
/* source queue */
EXPORT_SYMBOL(stm_display_source_queue_get_pixel_formats);
EXPORT_SYMBOL(stm_display_source_queue_lock);
EXPORT_SYMBOL(stm_display_source_queue_unlock);
EXPORT_SYMBOL(stm_display_source_queue_buffer);
EXPORT_SYMBOL(stm_display_source_queue_flush);
EXPORT_SYMBOL(stm_display_source_queue_release);
/* source pixel stream */
EXPORT_SYMBOL(stm_display_source_pixelstream_set_signal_status);
EXPORT_SYMBOL(stm_display_source_pixelstream_set_input_params);
EXPORT_SYMBOL(stm_display_source_pixelstream_release);
/* plane */
EXPORT_SYMBOL(stm_display_plane_get_name);
EXPORT_SYMBOL(stm_display_plane_get_capabilities);
EXPORT_SYMBOL(stm_display_plane_get_image_formats);
EXPORT_SYMBOL(stm_display_plane_set_control);
EXPORT_SYMBOL(stm_display_plane_get_control);
EXPORT_SYMBOL(stm_display_plane_get_status);
EXPORT_SYMBOL(stm_display_plane_get_device_id);
EXPORT_SYMBOL(stm_display_plane_connect_to_output);
EXPORT_SYMBOL(stm_display_plane_disconnect_from_output);
EXPORT_SYMBOL(stm_display_plane_get_connected_output_id);
EXPORT_SYMBOL(stm_display_plane_connect_to_source);
EXPORT_SYMBOL(stm_display_plane_disconnect_from_source);
EXPORT_SYMBOL(stm_display_plane_get_available_source_id);
EXPORT_SYMBOL(stm_display_plane_get_connected_source_id);
EXPORT_SYMBOL(stm_display_plane_set_depth);
EXPORT_SYMBOL(stm_display_plane_get_depth);
EXPORT_SYMBOL(stm_display_plane_hide);
EXPORT_SYMBOL(stm_display_plane_show);
EXPORT_SYMBOL(stm_display_plane_pause);
EXPORT_SYMBOL(stm_display_plane_resume);
EXPORT_SYMBOL(stm_display_plane_close);
EXPORT_SYMBOL(stm_display_plane_get_list_of_features);
EXPORT_SYMBOL(stm_display_plane_is_feature_applicable);
EXPORT_SYMBOL(stm_display_plane_get_control_range);
EXPORT_SYMBOL(stm_display_plane_get_compound_control);
EXPORT_SYMBOL(stm_display_plane_set_compound_control);
EXPORT_SYMBOL(stm_display_plane_get_compound_control_range);
EXPORT_SYMBOL(stm_display_plane_get_timing_identifier);
EXPORT_SYMBOL(stm_display_plane_get_tuning_data_revision);
EXPORT_SYMBOL(stm_display_plane_get_tuning_data_control);
EXPORT_SYMBOL(stm_display_plane_set_tuning_data_control);
EXPORT_SYMBOL(stm_display_plane_get_control_mode);
EXPORT_SYMBOL(stm_display_plane_set_control_mode);
EXPORT_SYMBOL(stm_display_plane_apply_sync_controls);
EXPORT_SYMBOL(stm_display_plane_set_asynch_ctrl_listener);
EXPORT_SYMBOL(stm_display_plane_unset_asynch_ctrl_listener);

/* output */
EXPORT_SYMBOL(stm_display_output_get_name);
EXPORT_SYMBOL(stm_display_output_get_device_id);
EXPORT_SYMBOL(stm_display_output_get_capabilities);
EXPORT_SYMBOL(stm_display_output_set_control);
EXPORT_SYMBOL(stm_display_output_get_control);
EXPORT_SYMBOL(stm_display_output_set_compound_control);
EXPORT_SYMBOL(stm_display_output_get_compound_control);
EXPORT_SYMBOL(stm_display_output_get_display_mode);
EXPORT_SYMBOL(stm_display_output_find_display_mode);
EXPORT_SYMBOL(stm_display_output_get_current_display_mode);
EXPORT_SYMBOL(stm_display_output_start);
EXPORT_SYMBOL(stm_display_output_stop);
EXPORT_SYMBOL(stm_display_output_queue_metadata);
EXPORT_SYMBOL(stm_display_output_flush_metadata);
EXPORT_SYMBOL(stm_display_output_handle_interrupts);
EXPORT_SYMBOL(stm_display_output_get_last_timing_event);
EXPORT_SYMBOL(stm_display_output_get_connection_status);
EXPORT_SYMBOL(stm_display_output_set_connection_status);
EXPORT_SYMBOL(stm_display_output_set_clock_reference);
EXPORT_SYMBOL(stm_display_output_soft_reset);
EXPORT_SYMBOL(stm_display_output_close);
#endif /* MODULE */
