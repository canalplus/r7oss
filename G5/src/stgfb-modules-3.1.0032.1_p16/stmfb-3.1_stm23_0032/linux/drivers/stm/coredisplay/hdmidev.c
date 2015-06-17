/***********************************************************************
 *
 * File: linux/drivers/stm/coredisplay/hdmidev.c
 * Copyright (c) 2005,2007,2008 STMicroelectronics Limited.
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
#include <linux/fb.h>
#include <linux/interrupt.h>
#include <linux/i2c.h>
#include <linux/kthread.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/mutex.h>
#include <linux/wait.h>

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,23)
#  include <linux/stpio.h>
#else
#  include <linux/stm/pio.h>
#endif

#include <asm/uaccess.h>
#include <asm/irq.h>
#include <asm/semaphore.h>

#include <stmdisplay.h>
#include <linux/stm/stmcoredisplay.h>
#include <linux/stm/stmcorehdmi.h>

#include "stmhdmi.h"

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,20)
  #define IRQF_DISABLED SA_INTERRUPT
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,19)
  typedef irqreturn_t (*stm_irq_handler_type)(int, void *, struct pt_regs *);
#else
  typedef irqreturn_t (*stm_irq_handler_type)(int, void *);
#endif

static char *hdmi0;

module_param(hdmi0, charp, 0444);
MODULE_PARM_DESC(hdmi0, "[enable|disable]");

extern int stmhdmi_manager(void *data);


static int stmhdmi_open(struct inode *inode,struct file *filp)
{
  struct stm_hdmi *dev = container_of(inode->i_cdev, struct stm_hdmi, cdev);
  filp->private_data = dev;
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

  if(*off >= sizeof(dev->edid_info.raw))
    return retval;

  if(*off + sz > sizeof(dev->edid_info.raw))
    sz = sizeof(dev->edid_info.raw) - *off;

  if(mutex_lock_interruptible(&dev->lock))
    return -ERESTARTSYS;

  if(dev->edid_info.display_type != STM_DISPLAY_INVALID)
  {
    if(copy_to_user(buf, &dev->edid_info.raw[0][0] + *off, sz))
    {
      retval = -EFAULT;
    }
    else
    {
      *off += sz;
      retval = sz;
    }
  }

  mutex_unlock(&dev->lock);

  return retval;
}


static inline int stmhdmi_convert_metadata_result_to_errno(stm_meta_data_result_t res)
{
  switch(res)
  {
    case STM_METADATA_RES_OK:
      break;
    case STM_METADATA_RES_UNSUPPORTED_TYPE:
      return -EOPNOTSUPP;
    case STM_METADATA_RES_TIMESTAMP_IN_PAST:
    case STM_METADATA_RES_INVALID_DATA:
      return -EINVAL;
    case STM_METADATA_RES_QUEUE_BUSY:
      return -EBUSY;
    case STM_METADATA_RES_QUEUE_UNAVAILABLE:
      return -EAGAIN;
  }

  return 0;
}


static int stmhdmi_send_data_packet(struct stm_hdmi *dev, unsigned long arg)
{
  stm_meta_data_result_t res;
  stm_meta_data_t *metadata;
  stm_hdmi_info_frame_t *iframe;
  struct stmhdmiio_data_packet packet;

  if (copy_from_user(&packet,(void*)arg,sizeof(packet)))
    return -EFAULT;

  if((metadata = kzalloc(sizeof(stm_meta_data_t)+sizeof(stm_hdmi_info_frame_t),GFP_KERNEL)) == 0)
    return -ENOMEM;

  metadata->size      = sizeof(stm_meta_data_t)+sizeof(stm_hdmi_info_frame_t);
  metadata->release   = (void(*)(struct stm_meta_data_s*))kfree;
  metadata->presentationTime = ((TIME64)packet.timestamp.tv_sec * USEC_PER_SEC) +
                                (TIME64)packet.timestamp.tv_usec;

  switch(packet.type)
  {
    case HDMI_ACP_PACKET_TYPE:
    {
      /*
       * Don't allow the configuration of ACP packets unless the
       * connected TV has the supports AI flag set in its EDID.
       */
      if((dev->edid_info.display_type != STM_DISPLAY_HDMI) ||
         (dev->edid_info.hdmi_vsdb_flags & STM_HDMI_VSDB_SUPPORTS_AI) == 0)
      {
        DPRINTK("Not Sending ACP Datapacket, sink does not support AI\n");
        kfree(metadata);
        return -EPERM;
      }

      DPRINTK("Sending ACP Datapacket\n");

      metadata->type = STM_METADATA_TYPE_ACP_DATA;
      break;
    }
    case HDMI_VENDOR_INFOFRAME_TYPE:
    {
      DPRINTK("Sending vendor IFrame\n");
      metadata->type = STM_METADATA_TYPE_VENDOR_IFRAME;
      break;
    }
    case HDMI_NTSC_INFOFRAME_TYPE:
    {
      DPRINTK("Sending NTSC IFrame\n");
      metadata->type = STM_METADATA_TYPE_NTSC_IFRAME;
      break;
    }
    case HDMI_GAMUT_DATA_PACKET_TYPE:
    {
      DPRINTK("Sending Color Gamut Datapacket\n");
      metadata->type = STM_METADATA_TYPE_COLOR_GAMUT_DATA;
      break;
    }
    default:
    {
      DPRINTK("Unsupported Datapacket\n");
      kfree(metadata);
      return -EINVAL;
    }
  }

  iframe = (stm_hdmi_info_frame_t*)&metadata->data[0];
  iframe->type    = packet.type;
  iframe->version = packet.version;
  iframe->length  = packet.length;
  /*
   * Note: we cannot use packet.length to size the memcpy as this is only
   * valid for real InfoFrames not arbitrary HDMI data island packets.
   */
  memcpy(&iframe->data[0],&packet.data[0],28);

  if(stm_display_output_queue_metadata(dev->hdmi_output, metadata, &res)<0)
  {
    kfree(metadata);
    if(signal_pending(current))
      return -ERESTARTSYS;
    else
      return -EIO;
  }

  return stmhdmi_convert_metadata_result_to_errno(res);
}


static int stmhdmi_set_isrc_data(struct stm_hdmi *dev, unsigned long arg)
{
  stm_meta_data_result_t res;
  stm_meta_data_t       *metadata;
  stm_hdmi_isrc_data_t  *isrc;
  struct stmhdmiio_isrc_data isrcdata;

  if (copy_from_user(&isrcdata,(void*)arg,sizeof(isrcdata)))
    return -EFAULT;

  /*
   * Don't allow the configuration of ISRC packets unless the
   * connected TV has the supports AI flag set in its EDID.
   */
  if((dev->edid_info.display_type != STM_DISPLAY_HDMI) ||
     (dev->edid_info.hdmi_vsdb_flags & STM_HDMI_VSDB_SUPPORTS_AI) == 0)
  {
    DPRINTK("Not Sending ISRC Datapackets, sink does not support AI\n");
    return -EPERM;
  }

  if((metadata = kzalloc(sizeof(stm_meta_data_t)+sizeof(stm_hdmi_isrc_data_t),GFP_KERNEL)) == 0)
    return -ENOMEM;

  metadata->size    = sizeof(stm_meta_data_t)+sizeof(stm_hdmi_isrc_data_t);
  metadata->release = (void(*)(struct stm_meta_data_s*))kfree;
  metadata->type    = STM_METADATA_TYPE_ISRC_DATA;
  metadata->presentationTime = ((TIME64)isrcdata.timestamp.tv_sec * USEC_PER_SEC) +
                                (TIME64)isrcdata.timestamp.tv_usec;

  isrc = (stm_hdmi_isrc_data_t*)&metadata->data[0];
  isrc->isrc1.type = HDMI_ISRC1_PACKET_TYPE;
  isrc->isrc2.type = HDMI_ISRC2_PACKET_TYPE;

  if(isrcdata.status != ISRC_STATUS_DISABLE)
  {
    int i;
    isrc->isrc1.version = (isrcdata.status & HDMI_ISRC1_STATUS_MASK) | HDMI_ISRC1_VALID;
    /*
     * Just copy the first 16 bytes of information to ISRC1
     */
    memcpy(isrc->isrc1.data,&isrcdata.upc_ean_isrc[0],16);
    /*
     * For the second 16 bytes we need to see if there is any non-zero data in
     * there. If not then the second ISRC packet will not be transmitted.
     */
    for(i=16;i<32;i++)
    {
      if(isrcdata.upc_ean_isrc[i] != 0)
      {
        isrc->isrc1.version |= HDMI_ISRC1_CONTINUED;
        isrc->isrc2.data[i-16] = isrcdata.upc_ean_isrc[i];
      }
    }
  }

  DPRINTK("Sending ISRC Datapackets\n");

  if(stm_display_output_queue_metadata(dev->hdmi_output, metadata, &res)<0)
  {
    kfree(metadata);
    if(signal_pending(current))
      return -ERESTARTSYS;
    else
      return -EIO;
  }

  return stmhdmi_convert_metadata_result_to_errno(res);
}


static void stmhdmi_flush_data_packet_queue(struct stm_hdmi *dev, unsigned long arg)
{
  if(arg & STMHDMIIO_FLUSH_ACP_QUEUE)
    stm_display_output_flush_metadata(dev->hdmi_output,STM_METADATA_TYPE_ACP_DATA);

  if(arg & STMHDMIIO_FLUSH_ISRC_QUEUE)
    stm_display_output_flush_metadata(dev->hdmi_output,STM_METADATA_TYPE_ISRC_DATA);

  if(arg & STMHDMIIO_FLUSH_VENDOR_QUEUE)
    stm_display_output_flush_metadata(dev->hdmi_output,STM_METADATA_TYPE_VENDOR_IFRAME);

  if(arg & STMHDMIIO_FLUSH_GAMUT_QUEUE)
    stm_display_output_flush_metadata(dev->hdmi_output,STM_METADATA_TYPE_COLOR_GAMUT_DATA);

  if(arg & STMHDMIIO_FLUSH_NTSC_QUEUE)
    stm_display_output_flush_metadata(dev->hdmi_output,STM_METADATA_TYPE_NTSC_IFRAME);
}


static int stmhdmi_set_audio_iframe_data(struct stm_hdmi *dev, unsigned long arg)
{
  stm_meta_data_result_t res;
  stm_meta_data_t       *metadata;
  stm_hdmi_info_frame_t *iframe;
  struct stmhdmiio_audio audiocfg;

  if (copy_from_user(&audiocfg,(void*)arg,sizeof(audiocfg)))
    return -EFAULT;

  if((metadata = kzalloc(sizeof(stm_meta_data_t)+sizeof(stm_hdmi_info_frame_t),GFP_KERNEL)) == 0)
    return -ENOMEM;

  metadata->size    = sizeof(stm_meta_data_t)+sizeof(stm_hdmi_info_frame_t);
  metadata->release = (void(*)(struct stm_meta_data_s*))kfree;
  metadata->type    = STM_METADATA_TYPE_AUDIO_IFRAME;

  iframe          = (stm_hdmi_info_frame_t*)&metadata->data[0];
  iframe->type    = HDMI_AUDIO_INFOFRAME_TYPE;
  iframe->version = HDMI_AUDIO_INFOFRAME_VERSION;
  iframe->length  = HDMI_AUDIO_INFOFRAME_LENGTH;
  iframe->data[1] = (audiocfg.channel_count    << HDMI_AUDIO_INFOFRAME_CHANNEL_COUNT_SHIFT) & HDMI_AUDIO_INFOFRAME_CHANNEL_COUNT_MASK;
  iframe->data[2] = (audiocfg.sample_frequency << HDMI_AUDIO_INFOFRAME_FREQ_SHIFT) & HDMI_AUDIO_INFOFRAME_FREQ_MASK;
  iframe->data[4] = audiocfg.speaker_mapping;
  iframe->data[5] = audiocfg.downmix_info & (HDMI_AUDIO_INFOFRAME_LEVELSHIFT_MASK|
                                             HDMI_AUDIO_INFOFRAME_DOWNMIX_INHIBIT);

  if(stm_display_output_queue_metadata(dev->hdmi_output, metadata, &res)<0)
  {
    kfree(metadata);
    if(signal_pending(current))
      return -ERESTARTSYS;
    else
      return -EIO;
  }

  return stmhdmi_convert_metadata_result_to_errno(res);
}


static int stmhdmi_set_spd_data(struct stm_hdmi *dev, unsigned long arg)
{
  struct stmhdmiio_spd spdinfo;
  unsigned long flags;

  if (copy_from_user(&spdinfo,(void*)arg,sizeof(spdinfo)))
    return -EFAULT;

  /*
   * We need to update these atomically as the buffer is being shared with
   * the low level driver's info frame transmission manager. In this case,
   * due to the very infrequent update of SPD information, it is OK to
   * do it this way.
   */
  spin_lock_irqsave(&dev->spinlock, flags);
  memcpy(&dev->spd_frame->data[HDMI_SPD_INFOFRAME_VENDOR_START] , spdinfo.vendor_name , HDMI_SPD_INFOFRAME_VENDOR_LENGTH);
  memcpy(&dev->spd_frame->data[HDMI_SPD_INFOFRAME_PRODUCT_START], spdinfo.product_name, HDMI_SPD_INFOFRAME_PRODUCT_LENGTH);
  dev->spd_frame->data[HDMI_SPD_INFOFRAME_SPI_OFFSET] = spdinfo.identifier;
  spin_unlock_irqrestore(&dev->spinlock, flags);

  return 0;
}


static long stmhdmi_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
  struct stm_hdmi *dev = (struct stm_hdmi *)filp->private_data;
  long retval=0;

  if(mutex_lock_interruptible(&dev->lock))
    return -ERESTARTSYS;


  switch(cmd)
  {
    case STMHDMIIO_SET_SPD_DATA:
      retval = stmhdmi_set_spd_data(dev, arg);
      break;

    case STMHDMIIO_SEND_DATA_PACKET:
      retval = stmhdmi_send_data_packet(dev, arg);
      break;

    case STMHDMIIO_SET_ISRC_DATA:
      retval = stmhdmi_set_isrc_data(dev, arg);
      break;

    case STMHDMIIO_FLUSH_DATA_PACKET_QUEUE:
      stmhdmi_flush_data_packet_queue(dev,arg);
      break;

    case STMHDMIIO_SET_AUDIO_DATA:
      retval = stmhdmi_set_audio_iframe_data(dev, arg);
      break;

    case STMHDMIIO_SET_AVMUTE:
      if(stm_display_output_set_control(dev->hdmi_output, STM_CTRL_AVMUTE, arg)<0)
      {
        if(signal_pending(current))
          retval = -ERESTARTSYS;
        else
          retval = -EIO;
      }
      break;

    case STMHDMIIO_SET_AUDIO_SOURCE:
    {
      unsigned long audio = STM_AV_SOURCE_MAIN_INPUT;
      unsigned long val;

      switch(arg)
      {
        case STMHDMIIO_AUDIO_SOURCE_2CH_I2S:
          audio |= STM_AV_SOURCE_2CH_I2S_INPUT;
          break;
        case STMHDMIIO_AUDIO_SOURCE_SPDIF:
          audio |= STM_AV_SOURCE_SPDIF_INPUT;
          break;
        case STMHDMIIO_AUDIO_SOURCE_8CH_I2S:
          audio |= STM_AV_SOURCE_8CH_I2S_INPUT;
          break;
        case STMHDMIIO_AUDIO_SOURCE_NONE:
          break;
        default:
          retval = -EINVAL;
          goto exit;
      }

      if(stm_display_output_set_control(dev->hdmi_output, STM_CTRL_AV_SOURCE_SELECT, audio)<0)
      {
        if(signal_pending(current))
          retval = -ERESTARTSYS;
        else
          retval = -EIO;

        goto exit;
      }

      if(stm_display_output_get_control(dev->hdmi_output, STM_CTRL_AV_SOURCE_SELECT, &val)<0)
      {
        if(signal_pending(current))
          retval = -EINTR;
        else
          retval = -EIO;

        goto exit;
      }

      if(val != audio)
        retval = -EINVAL;

      break;
    }
    case STMHDMIIO_SET_AUDIO_TYPE:
    {
      unsigned long val;

      if(stm_display_output_set_control(dev->hdmi_output, STM_CTRL_HDMI_AUDIO_OUT_SELECT, arg)<0)
      {
        if(signal_pending(current))
          retval = -ERESTARTSYS;
        else
          retval = -EIO;

        goto exit;
      }

      if(stm_display_output_get_control(dev->hdmi_output, STM_CTRL_HDMI_AUDIO_OUT_SELECT, &val)<0)
      {
        if(signal_pending(current))
          retval = -EINTR;
        else
          retval = -EIO;

        goto exit;
      }

      if(val != arg)
        retval = -EINVAL;

      break;
    }
    case STMHDMIIO_SET_OVERSCAN_MODE:
    {
      if(arg > STMHDMIIO_SCAN_UNDERSCANNED)
      {
        retval = -EINVAL;
        goto exit;
      }

      if(stm_display_output_set_control(dev->hdmi_output, STM_CTRL_HDMI_OVERSCAN_MODE, arg)<0)
      {
        if(signal_pending(current))
          retval = -ERESTARTSYS;
        else
          retval = -EIO;
      }

      break;
    }
    case STMHDMIIO_SET_CONTENT_TYPE:
    {
      ULONG val = 0;
      if(arg > STMHDMIIO_CE_CONTENT)
      {
        retval = -EINVAL;
        goto exit;
      }

      /*
       * Map the argument to infoframe bits IT|CN1|CN0
       */
      switch(arg)
      {
        case STMHDMIIO_CE_CONTENT:
          val = 0;
          break;
        default:
          val = arg | (1L<<2);
          break;
      }

      if(stm_display_output_set_control(dev->hdmi_output, STM_CTRL_HDMI_CONTENT_TYPE, val)<0)
      {
        if(signal_pending(current))
          retval = -ERESTARTSYS;
        else
          retval = -EIO;
      }

      break;
    }
    case STMHDMIIO_SET_EDID_MODE_HANDLING:
    {
      unsigned long flags;

      if(arg > STMHDMIIO_EDID_NON_STRICT_MODE_HANDLING)
      {
        retval = -EINVAL;
        goto exit;
      }

      dev->non_strict_edid_semantics = arg;

      spin_lock_irqsave(&dev->spinlock, flags);

      if(dev->status != STM_DISPLAY_DISCONNECTED)
      {
        dev->status = STM_DISPLAY_NEEDS_RESTART;
        dev->status_changed = 1;
        wake_up (&dev->status_wait_queue);
      }

      spin_unlock_irqrestore(&dev->spinlock, flags);

      break;
    }
    case STMHDMIIO_SET_HOTPLUG_MODE:
    {
      if(arg > STMHDMIIO_HPD_STOP_IF_NECESSARY)
      {
        retval = -EINVAL;
        goto exit;
      }

      dev->hotplug_mode = arg;
      break;
    }
    case STMHDMIIO_SET_CEA_MODE_SELECTION:
    {
      if(arg > STMHDMIIO_CEA_MODE_FROM_EDID_ASPECT_RATIO)
      {
        retval = -EINVAL;
        goto exit;
      }

      switch(arg)
      {
        case STMHDMIIO_CEA_MODE_FROM_EDID_ASPECT_RATIO:
          dev->cea_mode_from_edid = 1;
          /*
           * In case the display is already running, change the mode to whatever
           * the EDID says. Note that if the EDID had bad data we default to
           * 16:9.
           */
          arg = (dev->edid_info.tv_aspect == STM_WSS_4_3)?STMHDMIIO_CEA_MODE_4_3:STMHDMIIO_CEA_MODE_16_9;
          break;
        default:
          dev->cea_mode_from_edid = 0;
          break;
      }

      if(stm_display_output_set_control(dev->hdmi_output, STM_CTRL_HDMI_CEA_MODE_SELECT, arg)<0)
      {
        if(signal_pending(current))
          retval = -ERESTARTSYS;
        else
          retval = -EIO;
      }

      break;
    }
    case STMHDMIIO_SET_SAFE_MODE_PROTOCOL:
    {
      unsigned long flags;

      if(dev->hdmi_safe_mode == arg)
        goto exit;

      dev->hdmi_safe_mode = arg;

      spin_lock_irqsave(&dev->spinlock, flags);

      if(dev->status != STM_DISPLAY_DISCONNECTED)
      {
        dev->status = STM_DISPLAY_NEEDS_RESTART;
        dev->status_changed = 1;
        wake_up (&dev->status_wait_queue);
      }

      spin_unlock_irqrestore(&dev->spinlock, flags);

      break;
    }
    default:
      retval = -ENOTTY;

  }

exit:
  mutex_unlock(&dev->lock);
  return retval;
}


struct file_operations hdmi_fops = {
  .owner = THIS_MODULE,
  .open  = stmhdmi_open,
  .read  = stmhdmi_read,
  .unlocked_ioctl = stmhdmi_ioctl
};


/******************************************************************************/

static void stmhdmi_vsync_cb(stm_vsync_context_handle_t context, stm_field_t field)
{
  stm_display_status_t hdmi_status;
  struct stm_hdmi *hdmi_data = (struct stm_hdmi *)context;

  stm_display_output_get_status(hdmi_data->hdmi_output, &hdmi_status);

  if(hdmi_data->display_runtime->hotplug_poll_pio)
  {
    unsigned hotplugstate = stpio_get_pin(hdmi_data->display_runtime->hotplug_poll_pio);
    if(hdmi_status == STM_DISPLAY_DISCONNECTED)
    {
      /*
       * If the device has just been plugged in, flag that we now need to
       * start the output.
       */
      if(hotplugstate != 0)
      {
        hdmi_status = STM_DISPLAY_NEEDS_RESTART;
        stm_display_output_set_status(hdmi_data->hdmi_output, hdmi_status);
      }
    }
    else
    {
      /*
       * We may either be waiting for the output to be started, or already
       * started, so only change the state if the device has now been
       * disconnected.
       */
      if(hotplugstate == 0)
      {
        hdmi_status = STM_DISPLAY_DISCONNECTED;
        stm_display_output_set_status(hdmi_data->hdmi_output, hdmi_status);
      }
    }
  }

  if(hdmi_status != hdmi_data->status)
  {
    hdmi_data->status         = hdmi_status;
    hdmi_data->status_changed = 1;

    wake_up(&hdmi_data->status_wait_queue);
  }

}


/******************************************************************************/

int __exit stmhdmi_destroy(struct stm_hdmi *hdmi)
{
  if(!hdmi)
    return 0;

  /*
   * Terminate HDMI management thread
   */
  hdmi->auth = 0;
  wake_up(&(hdmi->auth_wait_queue));

  if(hdmi->thread != NULL)
    kthread_stop(hdmi->thread);

  hdmi->thread = NULL;

  stmcore_unregister_vsync_callback(hdmi->display_runtime, &hdmi->vsync_cb_info);

  if(hdmi->class_device)
  {
    class_device_unregister(hdmi->class_device);
    hdmi->class_device = NULL;
  }

  cdev_del(&hdmi->cdev);

  if(hdmi->irq != -1)
    free_irq(hdmi->irq, hdmi->hdmi_output);

  if(hdmi->hdmi_output)
  {
    stm_display_output_release(hdmi->hdmi_output);
    hdmi->hdmi_output = NULL;
  }

  if(hdmi->main_output)
  {
    stm_display_output_release(hdmi->main_output);
    hdmi->main_output = NULL;
  }

  /*
   * Do not release the device handle, it was a copy of the platform data
   * which will get released elsewhere.
   */
  hdmi->device = NULL;

  kfree(hdmi->spd_metadata);
  kfree(hdmi);

  return 0;
}


static irqreturn_t stmhdmi_interrupt(int irq, void* data)
{
  stm_display_output_t *o = (stm_display_output_t *)data;

  stm_display_output_handle_interrupts(o);
  return IRQ_HANDLED;
}


static int __init stmhdmi_create_spd_metadata(struct stm_hdmi *hdmi)
{
  if((hdmi->spd_metadata = kzalloc(sizeof(stm_meta_data_t)+sizeof(stm_hdmi_info_frame_t),GFP_KERNEL))==0)
  {
    DPRINTK("Cannot allocate SPD metadata\n");
    return -ENOMEM;
  }

  hdmi->spd_metadata->size = sizeof(stm_meta_data_t)+sizeof(stm_hdmi_info_frame_t);
  hdmi->spd_metadata->type = STM_METADATA_TYPE_SPD_IFRAME;

  hdmi->spd_frame = (stm_hdmi_info_frame_t*)&hdmi->spd_metadata->data[0];
  hdmi->spd_frame->type    = HDMI_SPD_INFOFRAME_TYPE;
  hdmi->spd_frame->version = HDMI_SPD_INFOFRAME_VERSION;
  hdmi->spd_frame->length  = HDMI_SPD_INFOFRAME_LENGTH;
  strncpy(&hdmi->spd_frame->data[HDMI_SPD_INFOFRAME_VENDOR_START],"STM",HDMI_SPD_INFOFRAME_VENDOR_LENGTH);
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
  spin_lock_init(&(hdmi->spinlock));
  init_waitqueue_head(&(hdmi->status_wait_queue));
  init_waitqueue_head(&(hdmi->auth_wait_queue));

  return hdmi;
}


static int __init stmhdmi_register_device(struct stm_hdmi *hdmi,
                                          int id,
                                          dev_t firstdevice,
                                          struct stmcore_display_pipeline_data *platform_data)
{
  cdev_init(&(hdmi->cdev),&hdmi_fops);
  hdmi->cdev.owner = THIS_MODULE;
  kobject_set_name(&(hdmi->cdev.kobj),"hdmi%d.0",id);

  if(cdev_add(&(hdmi->cdev),MKDEV(MAJOR(firstdevice),id),1))
    return -ENODEV;

  if(stmhdmi_init_class_device(hdmi, platform_data))
    return -ENODEV;

  return 0;
}

static struct stm_hdmi *hdmi_dev = NULL;
int stmhdmi_get_cec_address(unsigned short *addr)
{
  if (!hdmi_dev || !addr)
    return -1;

  if (mutex_lock_interruptible(&hdmi_dev->lock))
    return -ERESTARTSYS;

  *addr = (hdmi_dev->edid_info.cec_address[0] << 12) +
          (hdmi_dev->edid_info.cec_address[1] << 8) +
          (hdmi_dev->edid_info.cec_address[2] << 4) +
          (hdmi_dev->edid_info.cec_address[3]);

  mutex_unlock(&hdmi_dev->lock);
  return 0; 
}
EXPORT_SYMBOL(stmhdmi_get_cec_address);

int __init stmhdmi_create(int id,
                          dev_t firstdevice,
                          struct stmcore_display_pipeline_data *platform_data)
{
  struct stm_hdmi *hdmi;
  struct i2c_adapter *i2c;
  char *paramstring;

  if(!platform_data)
  {
    printk(KERN_ERR "platform data pointer is NULL\n");
    BUG();
    return -EINVAL;
  }

  i2c = i2c_get_adapter(platform_data->hdmi_i2c_adapter_id);

  if(!i2c)
  {
    printk(KERN_ERR "HDMI i2c bus (%d) not available, check your kernel configuration and board setup\n",platform_data->hdmi_i2c_adapter_id);
    return -EINVAL;
  }

  platform_data->hdmi_data = NULL;

  if((hdmi = stmhdmi_create_hdmi_dev_struct()) == NULL)
    return -ENOMEM;

  DPRINTK("new hdmi structure = %p\n",hdmi);

  /*
   * Note that we reuse the device handle from the platform data.
   */
  hdmi->device      = platform_data->device;
  hdmi->irq         = -1;
  hdmi->i2c_adapter = i2c;
  hdmi->video_type  = STM_VIDEO_OUT_RGB;

  switch(id)
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
      printk(KERN_WARNING "hdmi%d.0 is initially disabled, use 'stfbset -e hdmi' to enable it\n",id);
      hdmi->disable = 1;
    }
  }

  /*
   * Set the default CEA selection behaviour to use the aspect ratio in the EDID
   */
  hdmi->cea_mode_from_edid = 1;

  /*
   * Copy the display runtime pointer for the vsync callback handling.
   */
  hdmi->display_runtime = platform_data->display_runtime;

  /*
   * Note that we are trusting the output identifiers are valid
   * and pointing to correct output types.
   */
  hdmi->main_output = stm_display_get_output(hdmi->device, platform_data->main_output_id);
  hdmi->hdmi_output = stm_display_get_output(hdmi->device, platform_data->hdmi_output_id);

  if(hdmi->main_output == NULL || hdmi->hdmi_output == NULL)
  {
    DPRINTK("Cannot get display outputs main = %d, hdmi = %d\n",platform_data->main_output_id,platform_data->hdmi_output_id);
    stmhdmi_destroy(hdmi);
    return -ENODEV;
  }

  if(stm_display_output_get_capabilities(hdmi->hdmi_output, &hdmi->capabilities)<0)
  {
    DPRINTK("Cannot get hdmi output capabilities\n");
    stmhdmi_destroy(hdmi);
    return -ENODEV;
  }

  if(!(hdmi->capabilities & STM_OUTPUT_CAPS_TMDS))
  {
    printk(KERN_ERR "Provided HDMI output identifier doesn't support TMDS??\n");
    stmhdmi_destroy(hdmi);
    return -ENODEV;
  }

  if(request_irq(platform_data->hdmi_irq, (stm_irq_handler_type) stmhdmi_interrupt, IRQF_DISABLED, "hdmi", hdmi->hdmi_output))
  {
    printk(KERN_ERR "Cannot get HDMI irq = %d\n",platform_data->hdmi_irq);
    stmhdmi_destroy(hdmi);
    return -ENODEV;
  }

  hdmi->irq = platform_data->hdmi_irq;

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
  if(stmcore_register_vsync_callback(hdmi->display_runtime, &hdmi->vsync_cb_info)<0)
  {
    printk(KERN_ERR "Cannot register hdmi vsync callback\n");
    return -ENODEV;
  }

  hdmi->thread = kthread_run(stmhdmi_manager,hdmi,"hdmid/%d",id);
  if (hdmi->thread == NULL)
  {
    printk(KERN_ERR "Cannot start hdmi thread id = %d\n",id);
    stmhdmi_destroy(hdmi);
    return -ENOMEM;
  }

  platform_data->hdmi_data = hdmi;

  if(stmhdmi_register_device(hdmi, id, firstdevice, platform_data))
  {
    stmhdmi_destroy(hdmi);
    return -ENODEV;
  }
  /* get a ref for exported symbol */
  hdmi_dev = hdmi;

  return 0;
}

