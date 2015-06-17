/***********************************************************************
 *
 * File: linux/kernel/drivers/stm/hdmi/hdmiioctl.c
 * Copyright (c) 2005-2012 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#include <linux/version.h>
#include <linux/compiler.h>
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/mutex.h>
#include <linux/wait.h>

#include <asm/uaccess.h>
#include <linux/semaphore.h>

#include <stm_display.h>
#include <linux/stm/stmcoredisplay.h>
#include <linux/stm/stmcorehdmi.h>

#include "stmhdmi.h"

static int stmhdmi_send_data_packet(struct stm_hdmi *dev, unsigned long arg)
{
  stm_display_metadata_t       *metadata;
  stm_hdmi_info_frame_t        *iframe;
  struct stmhdmiio_data_packet  packet;
  int res;

  if (copy_from_user(&packet,(void*)arg,sizeof(packet)))
    return -EFAULT;

  if((metadata = kzalloc(sizeof(stm_display_metadata_t)+sizeof(stm_hdmi_info_frame_t),GFP_KERNEL)) == 0)
    return -ENOMEM;

  metadata->size      = sizeof(stm_display_metadata_t)+sizeof(stm_hdmi_info_frame_t);
  metadata->release   = (void(*)(struct stm_display_metadata_s*))kfree;
  metadata->presentation_time = ((stm_time64_t)packet.timestamp.tv_sec * USEC_PER_SEC) +
                                (stm_time64_t)packet.timestamp.tv_usec;

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

  if((res = stm_display_output_queue_metadata(dev->hdmi_output, metadata))<0)
  {
    kfree(metadata);
    if(signal_pending(current))
      return -ERESTARTSYS;
    else
      return res;
  }

  return 0;
}


static int stmhdmi_set_isrc_data(struct stm_hdmi *dev, unsigned long arg)
{
  stm_display_metadata_t    *metadata;
  stm_hdmi_isrc_data_t      *isrc;
  struct stmhdmiio_isrc_data isrcdata;
  int res;

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

  if((metadata = kzalloc(sizeof(stm_display_metadata_t)+sizeof(stm_hdmi_isrc_data_t),GFP_KERNEL)) == 0)
    return -ENOMEM;

  metadata->size    = sizeof(stm_display_metadata_t)+sizeof(stm_hdmi_isrc_data_t);
  metadata->release = (void(*)(struct stm_display_metadata_s*))kfree;
  metadata->type    = STM_METADATA_TYPE_ISRC_DATA;
  metadata->presentation_time = ((stm_time64_t)isrcdata.timestamp.tv_sec * USEC_PER_SEC) +
                                (stm_time64_t)isrcdata.timestamp.tv_usec;

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

  if((res = stm_display_output_queue_metadata(dev->hdmi_output, metadata))<0)
  {
    kfree(metadata);
    if(signal_pending(current))
      return -ERESTARTSYS;
    else
      return res;
  }

  return 0;
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
  stm_display_metadata_t *metadata;
  stm_hdmi_info_frame_t  *iframe;
  struct stmhdmiio_audio  audiocfg;
  int res;

  if (copy_from_user(&audiocfg,(void*)arg,sizeof(audiocfg)))
    return -EFAULT;

  if((metadata = kzalloc(sizeof(stm_display_metadata_t)+sizeof(stm_hdmi_info_frame_t),GFP_KERNEL)) == 0)
    return -ENOMEM;

  metadata->size    = sizeof(stm_display_metadata_t)+sizeof(stm_hdmi_info_frame_t);
  metadata->release = (void(*)(struct stm_display_metadata_s*))kfree;
  metadata->type    = STM_METADATA_TYPE_AUDIO_IFRAME;

  iframe          = (stm_hdmi_info_frame_t*)&metadata->data[0];
  iframe->type    = HDMI_AUDIO_INFOFRAME_TYPE;
  iframe->version = HDMI_AUDIO_INFOFRAME_VERSION;
  iframe->length  = HDMI_AUDIO_INFOFRAME_LENGTH;
  iframe->data[1] = ((audiocfg.channel_count-1) << HDMI_AUDIO_INFOFRAME_CHANNEL_COUNT_SHIFT) & HDMI_AUDIO_INFOFRAME_CHANNEL_COUNT_MASK;
  iframe->data[2] = (audiocfg.sample_frequency << HDMI_AUDIO_INFOFRAME_FREQ_SHIFT) & HDMI_AUDIO_INFOFRAME_FREQ_MASK;
  iframe->data[4] = audiocfg.speaker_mapping;
  iframe->data[5] = audiocfg.downmix_info & (HDMI_AUDIO_INFOFRAME_LEVELSHIFT_MASK|
                                             HDMI_AUDIO_INFOFRAME_DOWNMIX_INHIBIT);

  if((res = stm_display_output_queue_metadata(dev->hdmi_output, metadata))<0)
  {
    kfree(metadata);
    if(signal_pending(current))
      return -ERESTARTSYS;
    else
      return res;
  }

  return 0;
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

static int stmhdmi_subscribe_event(struct stm_hdmi *dev, unsigned long arg)
{
  int retval = 0;
  struct hdmi_event_subscription evt_subscription;
  stm_event_subscription_entry_t subscription_entry;

  DPRINTK("Create subscription : hdmi_object : %p\n", dev->hdmi_output);

  if(copy_from_user(&evt_subscription, (void *)arg, sizeof(evt_subscription)))
  {
    DPRINTK("error in copy_from_user\n");
    retval = -EFAULT;
    goto evt_subs_failed;
  }

  subscription_entry.cookie = NULL;
  subscription_entry.object = dev->hdmi_output;
  subscription_entry.event_mask = evt_subscription.type & (~HDMI_EVENT_DISPLAY_PRIVATE_START);
  DPRINTK("subscription_entry.event_mask = %x\n", subscription_entry.event_mask);

  retval = stm_event_subscription_create(&subscription_entry, 1, &dev->subscription);
  if(retval)
  {
    DPRINTK("Error cant create event subscription\n");
    goto evt_subs_failed;
  }

  retval = stm_event_set_wait_queue(dev->subscription, &dev->status_wait_queue, true);
  if(retval)
  {
    DPRINTK("Unable to set wait queue for HDMI Tx events\n");
    goto evt_set_queue_failed;
  }

  return 0;

evt_set_queue_failed:
  if(stm_event_subscription_delete(dev->subscription))
    DPRINTK("Failed to delete event subscription\n");
evt_subs_failed:
  return retval;
}

static int stmhdmi_unsubscribe_event(struct stm_hdmi *dev, unsigned long arg)
{
  int retval = 0;

  DPRINTK("Delete subscription hdmi_object : %p\n", dev->hdmi_output);

  retval = stm_event_subscription_delete(dev->subscription);
  if (retval)
    DPRINTK("Error: cant delete event subscription\n");

  return retval;
}

static int stmhdmi_dqevent(struct stm_hdmi *dev, unsigned long arg, int nonblocking)
{
  int retval = 0, timeout = 0;
  unsigned int number_of_events;
  stm_event_info_t evt_info;
  struct hdmi_event read_event = {0};

  DPRINTK("dequeue event hdmi_object : %p\n", dev->hdmi_output);

  if (copy_from_user(&read_event, (void *)arg, sizeof(struct hdmi_event)))
  {
    DPRINTK("error in copy_from_user\n");
    return -EFAULT;
  }

  if (nonblocking)
    timeout = 0;
  else
    timeout = -1;

  retval = stm_event_wait(dev->subscription, timeout, 1, &number_of_events, &evt_info);
  if(retval != 0)
    return retval;

  read_event.type = evt_info.event.event_id | HDMI_EVENT_DISPLAY_PRIVATE_START;

  if(evt_info.events_missed)
    read_event.sequence +=2;
  else
    read_event.sequence +=1;

  read_event.timestamp.tv_sec = (time_t)evt_info.timestamp;

  if(read_event.type == HDMI_EVENT_DISPLAY_CONNECTED)
  {
    switch(dev->edid_info.display_type)
    {
      case STM_DISPLAY_DVI:
        if(dev->edid_updated)
          read_event.u.data[0] = HDMI_SINK_IS_DVI;
        else
          read_event.u.data[0] = HDMI_SINK_IS_SAFEMODE_DVI;
        break;
      case STM_DISPLAY_HDMI:
        if(dev->edid_updated)
          read_event.u.data[0] = HDMI_SINK_IS_HDMI;
        else
          read_event.u.data[0] = HDMI_SINK_IS_SAFEMODE_HDMI;
        break;
      default:
        break;
    }
  }

  retval = copy_to_user((void *)arg, &read_event, sizeof(read_event));
  if (retval != 0) {
    DPRINTK("error in copy_to_user\n");
    retval = -EFAULT;
  }
  return retval;
}

static int stmhdmi_get_display_connection_state(struct stm_hdmi *dev, unsigned long arg)
{
  if(dev->hpd_state == STM_DISPLAY_LINK_HPD_STATE_HIGH)
  {
    switch(dev->edid_info.display_type)
    {
      case STM_DISPLAY_DVI:
        if(dev->edid_updated)
        {
          if(put_user(HDMI_SINK_IS_CONNECTED|HDMI_SINK_IS_DVI, (__u32 *)arg))
          return -EFAULT;
        }
        else
        {
          if(put_user(HDMI_SINK_IS_CONNECTED|HDMI_SINK_IS_SAFEMODE_DVI, (__u32 *)arg))
            return -EFAULT;
        }
        break;
      case STM_DISPLAY_HDMI:
        if(dev->edid_updated)
        {
          if(put_user(HDMI_SINK_IS_CONNECTED|HDMI_SINK_IS_HDMI, (__u32 *)arg))
            return -EFAULT;
        }
        else
        {
          if(put_user(HDMI_SINK_IS_CONNECTED|HDMI_SINK_IS_SAFEMODE_HDMI, (__u32 *)arg))
            return -EFAULT;
        }
        break;
      default:
        break;
    }
  }
  else
  {
    if (put_user(HDMI_SINK_IS_DISCONNECTED, (__u32 *)arg))
      return -EFAULT;
  }
  return 0;
}

long stmhdmi_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
  struct stm_hdmi *dev = (struct stm_hdmi *)filp->private_data;
  stm_display_output_connection_status_t  current_status;
  long retval=0;
  if(mutex_lock_interruptible(&dev->lock))
    return -ERESTARTSYS;

  if((retval = stm_display_output_get_connection_status(dev->hdmi_output, &current_status)) < 0)
    goto exit;

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
    {
      int enc_enable = (arg == 0)?1:0;
      if(stm_display_output_set_control(dev->hdmi_output, OUTPUT_CTRL_HDMI_AVMUTE, arg)<0)
      {
        if(signal_pending(current))
          retval = -ERESTARTSYS;
        else
          retval = -EIO;
      }
      dev->enc_enable = enc_enable;
      if ((dev->hdcp_enable) &&(!dev->stop_output))
        stm_display_link_set_ctrl(dev->link, STM_DISPLAY_LINK_CTRL_HDCP_FRAME_ENCRYPTION_ENABLE, dev->enc_enable);

      break;
    }
    case STMHDMIIO_SET_AUDIO_SOURCE:
    {
      stm_display_output_audio_source_t audio;
      switch(arg)
      {
        case STMHDMIIO_AUDIO_SOURCE_2CH_I2S:
          audio = STM_AUDIO_SOURCE_2CH_I2S;
          break;
        case STMHDMIIO_AUDIO_SOURCE_4CH_I2S:
          audio = STM_AUDIO_SOURCE_4CH_I2S;
          break;
        case STMHDMIIO_AUDIO_SOURCE_6CH_I2S:
          audio = STM_AUDIO_SOURCE_6CH_I2S;
          break;
        case STMHDMIIO_AUDIO_SOURCE_8CH_I2S:
          audio = STM_AUDIO_SOURCE_8CH_I2S;
          break;
        case STMHDMIIO_AUDIO_SOURCE_SPDIF:
          audio = STM_AUDIO_SOURCE_SPDIF;
          break;
        case STMHDMIIO_AUDIO_SOURCE_NONE:
          audio = STM_AUDIO_SOURCE_NONE;
          break;
        default:
          retval = -EINVAL;
          goto exit;
      }

      if((retval = stm_display_output_set_control(dev->hdmi_output, OUTPUT_CTRL_AUDIO_SOURCE_SELECT, audio))<0)
      {
        if(signal_pending(current))
          retval = -ERESTARTSYS;

        goto exit;
      }

      break;
    }
    case STMHDMIIO_SET_AUDIO_TYPE:
    {
      uint32_t val;

      if(stm_display_output_set_control(dev->hdmi_output, OUTPUT_CTRL_HDMI_AUDIO_OUT_SELECT, arg)<0)
      {
        if(signal_pending(current))
          retval = -ERESTARTSYS;
        else
          retval = -EIO;

        goto exit;
      }

      if(stm_display_output_get_control(dev->hdmi_output, OUTPUT_CTRL_HDMI_AUDIO_OUT_SELECT, &val)<0)
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

      if(stm_display_output_set_control(dev->hdmi_output, OUTPUT_CTRL_AVI_SCAN_INFO, arg)<0)
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
      if(arg > STMHDMIIO_CE_CONTENT)
      {
        retval = -EINVAL;
        goto exit;
      }

      if(stm_display_output_set_control(dev->hdmi_output, OUTPUT_CTRL_AVI_CONTENT_TYPE, arg)<0)
      {
        if(signal_pending(current))
          retval = -ERESTARTSYS;
        else
          retval = -EIO;
      }

      break;
    }
    case STMHDMIIO_SET_QUANTIZATION_MODE:
    {
      if(arg > STMHDMIIO_QUANTIZATION_FULL)
      {
        retval = -EINVAL;
        goto exit;
      }

      if(stm_display_output_set_control(dev->hdmi_output, OUTPUT_CTRL_AVI_QUANTIZATION_MODE, arg)<0)
      {
        if(signal_pending(current))
          retval = -ERESTARTSYS;
        else
          retval = -EIO;
      }

      break;
    }
    case STMHDMIIO_SET_EXTENDED_COLOR:
    {
      if(stm_display_output_set_control(dev->hdmi_output, OUTPUT_CTRL_AVI_EXTENDED_COLORIMETRY_INFO, arg)<0)
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
      stm_display_output_connection_status_t  hdmi_status;

      if(arg > STMHDMIIO_EDID_NON_STRICT_MODE_HANDLING)
      {
        retval = -EINVAL;
        goto exit;
      }

      dev->non_strict_edid_semantics = arg;

      spin_lock_irqsave(&dev->spinlock, flags);

      stm_display_output_get_connection_status(dev->hdmi_output, &hdmi_status);
      if(hdmi_status != STM_DISPLAY_DISCONNECTED)
      {
        dev->status_changed = 1;
      }

      spin_unlock_irqrestore(&dev->spinlock, flags);
      wake_up(&dev->status_wait_queue);

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
          arg = (dev->edid_info.tv_aspect == STM_WSS_ASPECT_RATIO_4_3)?STMHDMIIO_CEA_MODE_4_3:
                ((dev->edid_info.tv_aspect == STM_WSS_ASPECT_RATIO_GT_16_9)?STMHDMIIO_CEA_MODE_64_27:STMHDMIIO_CEA_MODE_16_9);
          break;
        default:
          dev->cea_mode_from_edid = 0;
          break;
      }

      if(stm_display_output_set_control(dev->hdmi_output, OUTPUT_CTRL_AVI_VIC_SELECT, arg)<0)
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
      stm_display_output_connection_status_t  hdmi_status;

      if(dev->hdmi_safe_mode == arg)
        goto exit;

      dev->hdmi_safe_mode = arg;

      spin_lock_irqsave(&dev->spinlock, flags);

      stm_display_output_get_connection_status(dev->hdmi_output, &hdmi_status);
      if(hdmi_status != STM_DISPLAY_DISCONNECTED)
      {
        dev->status_changed = 1;
      }

      spin_unlock_irqrestore(&dev->spinlock, flags);
      wake_up(&dev->status_wait_queue);

      break;
    }
    case STMHDMIIO_SET_FORCE_OUTPUT:
    {
      if(stm_display_output_set_control(dev->hdmi_output, OUTPUT_CTRL_FORCE_COLOR, arg)<0)
      {
        if(signal_pending(current))
          retval = -ERESTARTSYS;
        else
          retval = -EIO;

        goto exit;
      }
      break;
    }
    case STMHDMIIO_SET_FORCED_RGB_VALUE:
    {
      if(stm_display_output_set_control(dev->hdmi_output, OUTPUT_CTRL_FORCED_RGB_VALUE, arg)<0)
      {
        if(signal_pending(current))
          retval = -ERESTARTSYS;
        else
          retval = -EIO;

        goto exit;
      }
      break;
    }
    case STMHDMIIO_GET_OUTPUT_ID:
    {
      if(put_user(dev->hdmi_output_id, (__u32 *)arg))
        retval = -EFAULT;

      break;
    }
    case STMHDMIIO_SET_DISABLED:
    {
      unsigned long flags;
      int disable = (arg != 0)?1:0;

      if(disable && !dev->hdcp_stopped)
      {
        dev->hdcp_stopped =true;
        stm_display_link_set_ctrl(dev->link, STM_DISPLAY_LINK_CTRL_HDCP_ENABLE, 0);
        dev->current_hdcp_status =false;
      }
      spin_lock_irqsave(&dev->spinlock, flags);
      dev->disable = disable;
      dev->status_changed = 1;
      spin_unlock_irqrestore(&dev->spinlock, flags);
      wake_up(&dev->status_wait_queue);
      break;
    }
    case STMHDMIIO_GET_DISABLED:
    {
      __u32 disabled = (dev->disable?1:0);
      if(put_user(disabled, (__u32 *)arg))
        retval = -EFAULT;

      break;
    }
    case STMHDMIIO_FORCE_RESTART:
    {
      unsigned long flags;
      stm_display_output_connection_status_t hdmi_status;

      spin_lock_irqsave(&dev->spinlock, flags);

      stm_display_output_get_connection_status(dev->hdmi_output, &hdmi_status);

      if(hdmi_status == STM_DISPLAY_NEEDS_RESTART)
      {
        dev->status_changed = 1;
      }
      else
      {
        retval = -EIO;
      }

      spin_unlock_irqrestore(&dev->spinlock, flags);
      wake_up(&dev->status_wait_queue);
      break;
    }
    case STMHDMIIO_SET_VIDEO_FORMAT:
    {
      /*
       * NOTE: This is currently an interface designed to be used privately
       *       by the framebuffer driver to implement the control interface
       *       it exposes to DirectFB to configure the various video outputs.
       *       Therefore it is not intended for general purpose use at this time
       *       (that may change in the future if we rework the DirectFB drivers)
       */
      if(dev->video_type != arg)
      {
        unsigned long flags;
        stm_display_output_connection_status_t hdmi_status;
        spin_lock_irqsave(&dev->spinlock, flags);
        dev->video_type = arg;
        spin_unlock_irqrestore(&dev->spinlock, flags);
        /*
         * Stop HDCP first when HDMI o/p will be disabled
         */
        if(dev->hdcp_enable)
        {
          dev->hdcp_stopped =true;
          stm_display_link_set_ctrl(dev->link, STM_DISPLAY_LINK_CTRL_HDCP_ENABLE, 0);
          dev->current_hdcp_status =false;
        }
        spin_lock_irqsave(&dev->spinlock, flags);
        /*
         * We can only change the colorspace or colordepth on HDMI displays
         */
        stm_display_output_get_connection_status(dev->hdmi_output, &hdmi_status);
        if((hdmi_status == STM_DISPLAY_CONNECTED) && (dev->edid_info.display_type == STM_DISPLAY_HDMI))
        {
          dev->status_changed = 1;
        }
        spin_unlock_irqrestore(&dev->spinlock, flags);
        wake_up(&dev->status_wait_queue);
      }
      break;
    }
    case STMHDMIIO_SET_PIXEL_REPETITION:
    {
      unsigned long flags;
      stm_display_output_connection_status_t hdmi_status;
      spin_lock_irqsave(&dev->spinlock, flags);
      dev->max_pixel_repeat = arg;
      spin_unlock_irqrestore(&dev->spinlock, flags);
      /*
       * Stop HDCP first when HDMI o/p will be disabled
       */
      if(dev->hdcp_enable)
      {
        dev->hdcp_stopped =true;
        stm_display_link_set_ctrl(dev->link, STM_DISPLAY_LINK_CTRL_HDCP_ENABLE, 0);
        dev->current_hdcp_status =false;
      }
      spin_lock_irqsave(&dev->spinlock, flags);
      stm_display_output_get_connection_status(dev->hdmi_output, &hdmi_status);
      if((hdmi_status == STM_DISPLAY_CONNECTED) && (dev->edid_info.display_type == STM_DISPLAY_HDMI))
      {
        dev->status_changed = 1;
      }
      spin_unlock_irqrestore(&dev->spinlock, flags);
      wake_up(&dev->status_wait_queue);
      break;
    }
    case STMHDMIIO_DQEVENT:
    {
      mutex_unlock(&dev->lock);
      retval = stmhdmi_dqevent(dev, arg, (filp->f_flags & O_NONBLOCK));
      break;
    }
    case STMHDMIIO_SUBSCRIBE_EVENT:
    {
      retval = stmhdmi_subscribe_event(dev, arg);
      break;
    }
    case STMHDMIIO_UNSUBSCRIBE_EVENT:
    {
      retval = stmhdmi_unsubscribe_event(dev, arg);
      break;
    }
    case STMHDMIIO_GET_DISPLAY_CONNECTION_STATE:
    {
      retval = stmhdmi_get_display_connection_state(dev, arg);
      break;
    }
    default:
      retval = -ENOTTY;
  }

exit:
  mutex_unlock(&dev->lock);
  return retval;
}
