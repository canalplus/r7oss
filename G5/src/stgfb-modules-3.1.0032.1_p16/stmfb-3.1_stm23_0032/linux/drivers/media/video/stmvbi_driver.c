/***********************************************************************
 *
 * File: linux/drivers/media/video/stmvbi_driver.c
 * Copyright (c) 2009 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
 * V4L2 video output device driver for ST SoC display subsystems.
 *
\***********************************************************************/

#include <linux/version.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/errno.h>
#include <linux/wait.h>
#include <linux/poll.h>
#include <linux/mm.h>
#include <media/v4l2-dev.h>
#include <media/v4l2-common.h>

#include <asm/semaphore.h>
#include <asm/page.h>

#include <stmdisplay.h>
#include <linux/stm/stmcoredisplay.h>

#include "stmvbi_driver.h"
#include "stm_v4l2.h"

static int video_nr = -1;
module_param(video_nr, int, 0444);

static stvbi_device_t g_vbiData[MAX_PIPELINES];

void stmvbi_send_next_buffer_to_display(stvbi_device_t *pDev)
{
  streaming_buffer_t    *pStreamBuffer, *tmp;
  stm_meta_data_result_t res;
  unsigned long          flags;
  int                    ret;
  stm_meta_data_t       *firstField;
  stm_meta_data_t       *secondField;

  read_lock_irqsave (&pDev->pendingStreamQ.lock, flags);
  list_for_each_entry_safe (pStreamBuffer, tmp, &pDev->pendingStreamQ.list, node) {
    read_unlock_irqrestore (&pDev->pendingStreamQ.lock, flags);

    debug_msg("%s: pDev = %p, pStreamBuffer = %p\n", __FUNCTION__, pDev, pStreamBuffer);

    if(pStreamBuffer->botFieldFirst)
    {
      firstField  = pStreamBuffer->botField;
      secondField = pStreamBuffer->topField;
    }
    else
    {
      firstField  = pStreamBuffer->topField;
      secondField = pStreamBuffer->botField;
    }

    if(firstField)
    {
      ret = stm_display_output_queue_metadata(pDev->pOutput,
                                              pStreamBuffer->topField,
                                              &res);

      if (ret < 0 || res != STM_METADATA_RES_OK) {
        debug_msg ("%s: queueing first field failed\n", __PRETTY_FUNCTION__);
        return;
      }
    }

    if(secondField)
    {
      ret = stm_display_output_queue_metadata(pDev->pOutput,
                                              pStreamBuffer->botField,
                                              &res);

      if (ret < 0 || res != STM_METADATA_RES_OK) {
        debug_msg ("%s: queueing second field failed\n", __PRETTY_FUNCTION__);
        return;
      }
    }

    pStreamBuffer->vbiBuf.flags |= V4L2_BUF_FLAG_QUEUED;

    write_lock_irqsave (&pDev->pendingStreamQ.lock, flags);
    list_del_init (&pStreamBuffer->node);
    write_unlock_irqrestore (&pDev->pendingStreamQ.lock, flags);

    read_lock_irqsave (&pDev->pendingStreamQ.lock, flags);
  }
  read_unlock_irqrestore (&pDev->pendingStreamQ.lock, flags);
}


int stmvbi_dequeue_buffer(stvbi_device_t *pDev, struct v4l2_buffer* pVBIBuf)
{
  streaming_buffer_t *pBuf;
  unsigned long       flags;

  debug_msg("%s: pDev = %p, pVBIBuf = %p\n", __FUNCTION__, pDev, pVBIBuf);

  write_lock_irqsave (&pDev->completeStreamQ.lock, flags);
  list_for_each_entry (pBuf, &pDev->completeStreamQ.list, node)
  {
    list_del_init (&pBuf->node);
    write_unlock_irqrestore (&pDev->completeStreamQ.lock, flags);

    pBuf->vbiBuf.flags &= ~(V4L2_BUF_FLAG_QUEUED | V4L2_BUF_FLAG_DONE);

    *pVBIBuf = pBuf->vbiBuf;

    kfree(pBuf);

    return 1;
  }
  write_unlock_irqrestore (&pDev->completeStreamQ.lock, flags);

  return 0;
}


int stmvbi_streamoff(stvbi_device_t  *pDev)
{
  streaming_buffer_t *newBuf, *tmp;
  unsigned long       flags;

  debug_msg("%s: pDev = %p\n", __FUNCTION__, pDev);

  /* Discard the pending queue */
  write_lock_irqsave (&pDev->pendingStreamQ.lock, flags);
  list_for_each_entry_safe (newBuf, tmp, &pDev->pendingStreamQ.list, node)
  {
    list_del(&newBuf->node);
    kfree(newBuf->topField); // must manually free the meta data here.
    kfree(newBuf->botField); // must manually free the meta data here.
    kfree(newBuf);
  }
  write_unlock_irqrestore (&pDev->pendingStreamQ.lock, flags);

  debug_msg("%s: About to flush teletext metadata\n", __FUNCTION__);

  stm_display_output_flush_metadata(pDev->pOutput,STM_METADATA_TYPE_EBU_TELETEXT);

  debug_msg("%s: About to dequeue completed queue \n", __FUNCTION__);

  /* dqueue all buffers from the complete queue */
  write_lock_irqsave (&pDev->completeStreamQ.lock, flags);
  list_for_each_entry_safe (newBuf, tmp, &pDev->completeStreamQ.list, node)
  {
    list_del(&newBuf->node);
    kfree(newBuf); // metadata already released as we are on the completed Q
  }
  write_unlock_irqrestore (&pDev->completeStreamQ.lock, flags);

  pDev->isStreaming=0;
  wake_up_interruptible(&(pDev->wqStreamingState));

  debug_msg("%s\n", __FUNCTION__);
  return 0;
}


static int
stmvbi_set_buffer_format(stvbi_device_t    *pDev,
                         struct v4l2_format *fmt,
                         int updateConfig)
{
  const stm_mode_line_t* pCurrentMode;
  int servicelinecount = 0;
  int i;

  pCurrentMode = stm_display_output_get_current_display_mode(pDev->pOutput);
  if(!pCurrentMode)
  {
    if(signal_pending(current))
      return -ERESTARTSYS;
    else
      return -EIO;
  }

  /*
   * We only support EBU Teletext on 625line PAL/SECAM systems
   */
  if(pCurrentMode->Mode != STVTG_TIMING_MODE_576I50000_13500)
    return -EINVAL;

  /*
   * We have to force the following lines
   */
  for(i=0;i<7;i++)
  {
    fmt->fmt.sliced.service_lines[0][i] = 0;
    fmt->fmt.sliced.service_lines[1][i] = 0;
  }

  fmt->fmt.sliced.service_lines[0][23] = 0;
  fmt->fmt.sliced.service_lines[1][23] = 0;

  if(fmt->fmt.sliced.service_set != 0)
  {
    fmt->fmt.sliced.service_set = V4L2_SLICED_TELETEXT_B;
    for(i=7;i<23;i++)
    {
      fmt->fmt.sliced.service_lines[0][i] = V4L2_SLICED_TELETEXT_B;
      fmt->fmt.sliced.service_lines[1][i] = V4L2_SLICED_TELETEXT_B;
      servicelinecount += 2;
    }
  }
  else
  {
    for(i=7;i<23;i++)
    {
      if(fmt->fmt.sliced.service_lines[0][i] & V4L2_SLICED_TELETEXT_B)
      {
        fmt->fmt.sliced.service_lines[0][i] = V4L2_SLICED_TELETEXT_B;
        fmt->fmt.sliced.service_set |= V4L2_SLICED_TELETEXT_B;
        servicelinecount++;
      }
      else
      {
        fmt->fmt.sliced.service_lines[0][i] = 0;
      }

      if(fmt->fmt.sliced.service_lines[1][i] & V4L2_SLICED_TELETEXT_B)
      {
        fmt->fmt.sliced.service_lines[1][i] = V4L2_SLICED_TELETEXT_B;
        fmt->fmt.sliced.service_set |= V4L2_SLICED_TELETEXT_B;
        servicelinecount++;
      }
      else
      {
        fmt->fmt.sliced.service_lines[1][i] = 0;
      }
    }
  }

  fmt->fmt.sliced.io_size = sizeof(struct v4l2_sliced_vbi_data) * servicelinecount;

  if(updateConfig)
  {
    pDev->bufferFormat.type = V4L2_BUF_TYPE_SLICED_VBI_OUTPUT;
    pDev->bufferFormat.fmt.sliced = fmt->fmt.sliced;
  }

  return 0;
}


static void stmvbi_release_metadata(struct stm_meta_data_s *s)
{
streaming_buffer_t *pBuf = (streaming_buffer_t *)s->privateData;

  debug_msg("%s: metadata = %p pBuf = %p\n", __FUNCTION__, s, pBuf);

  if(pBuf->topField == s)
    pBuf->topField = NULL;

  if(pBuf->botField == s)
    pBuf->botField = NULL;

  kfree(s);

  BUG_ON(pBuf->useCount == 0);
  pBuf->useCount--;

  if(pBuf->pDev->isStreaming && (pBuf->useCount == 0))
  {
    unsigned long flags;
    pBuf->vbiBuf.flags |= V4L2_BUF_FLAG_DONE;

    write_lock_irqsave (&pBuf->pDev->completeStreamQ.lock, flags);
    list_add_tail (&pBuf->node, &pBuf->pDev->completeStreamQ.list);
    write_unlock_irqrestore (&pBuf->pDev->completeStreamQ.lock, flags);

    wake_up_interruptible(&pBuf->pDev->wqBufDQueue);
  }

}


static stm_meta_data_t *
stmvbi_allocate_vbi_metadata(streaming_buffer_t *pBuf)
{
  const int size = sizeof(stm_meta_data_t) + sizeof(stm_ebu_teletext_t) + (sizeof(stm_ebu_teletext_line_t)*18);
  stm_meta_data_t *m = kzalloc(size,GFP_KERNEL);
  stm_ebu_teletext_t *t;

  if(!m)
    return NULL;

  m->type             = STM_METADATA_TYPE_EBU_TELETEXT;
  m->size             = size;
  m->presentationTime = ((TIME64)pBuf->vbiBuf.timestamp.tv_sec * USEC_PER_SEC) +
                         (TIME64)pBuf->vbiBuf.timestamp.tv_usec;
  m->privateData      = pBuf;
  m->release          = &stmvbi_release_metadata;

  t = (stm_ebu_teletext_t *)m->data;
  t->lines = (stm_ebu_teletext_line_t *)(((char *)m->data)+sizeof(stm_ebu_teletext_t));

#ifdef DEBUG_VBI_BUFFER_SETUP
  debug_msg ("%s: metadata = %p size = %d t = %p t->lines = %p.\n", __PRETTY_FUNCTION__, m, size, t, t->lines);
#endif

  return m;
}


static int
stmvbi_fill_metadata_buffers(stvbi_device_t *pDev,
                             struct v4l2_sliced_vbi_data *data,
                             stm_ebu_teletext_t *ttxtdata[2])
{
  int packet;
  int lastline  = 5;
  int lastfield = 0;
  int npackets  = pDev->bufferFormat.fmt.sliced.io_size/sizeof(struct v4l2_sliced_vbi_data);

#ifdef DEBUG_VBI_BUFFER_SETUP
  debug_msg ("%s.\n", __PRETTY_FUNCTION__);
#endif

  for(packet = 0; packet<npackets; packet++)
  {
    switch(data[packet].id)
    {
      case 0:
        continue;
      case V4L2_SLICED_TELETEXT_B:
        break;
      default:
        return -EINVAL;
    }

    if((data[packet].line <= lastline) || (data[packet].field < lastfield))
      return -EINVAL;

    if((data[packet].line > 23) || (data[packet].field > 1))
      return -EINVAL;

    if((pDev->bufferFormat.fmt.sliced.service_lines[data[packet].field][data[packet].line] & data[packet].id) == 0)
      return -EINVAL;

#ifdef DEBUG_VBI_BUFFER_SETUP
    debug_msg ("%s: copying field %d, line %d: memcpy(%p,%p,%d)\n",
                __FUNCTION__, data[packet].field, data[packet].line,
                ttxtdata[data[packet].field]->lines[data[packet].line-6].lineData,
                data[packet].data,
                sizeof(stm_ebu_teletext_line_t));
#endif

    memcpy(ttxtdata[data[packet].field]->lines[data[packet].line-6].lineData, data[packet].data, sizeof(stm_ebu_teletext_line_t));
    ttxtdata[data[packet].field]->ulValidLineMask |= (1L << data[packet].line);
  }
  return 0;
}


static int
stmvbi_queue_buffer(stvbi_device_t *pDev, struct v4l2_buffer* pVBIBuf)
{
  const stm_mode_line_t *pCurrentMode;
  streaming_buffer_t    *pBuf     = NULL;
  stm_meta_data_t       *topField = NULL;
  stm_meta_data_t       *botField = NULL;
  stm_ebu_teletext_t    *lines[2] = {NULL};
  int ret;

  if (pVBIBuf->memory != V4L2_MEMORY_USERPTR)
   return -EINVAL;

  if (!access_ok(VERIFY_READ,(void *)pVBIBuf->m.userptr,pDev->bufferFormat.fmt.sliced.io_size))
    return -EFAULT;

  pCurrentMode = stm_display_output_get_current_display_mode(pDev->pOutput);
  if(!pCurrentMode)
  {
    if(signal_pending(current))
      return -ERESTARTSYS;
    else
      return -EIO;
  }

  /*
   * We only support EBU Teletext on 625line PAL/SECAM systems
   */
  if(pCurrentMode->Mode != STVTG_TIMING_MODE_576I50000_13500)
    return -EINVAL;

  if((pBuf = kzalloc(sizeof(streaming_buffer_t),GFP_KERNEL)) == NULL)
    return -ENOMEM;

  INIT_LIST_HEAD (&pBuf->node);
  pBuf->pDev   = pDev;
  pBuf->vbiBuf = *pVBIBuf;
  pBuf->vbiBuf.flags &= ~(V4L2_BUF_FLAG_DONE | V4L2_BUF_FLAG_QUEUED);

  if((topField = stmvbi_allocate_vbi_metadata(pBuf)) == NULL)
  {
    ret = -ENOMEM;
    goto error_ret;
  }

  if((botField = stmvbi_allocate_vbi_metadata(pBuf)) == NULL)
  {
    ret = -ENOMEM;
    goto error_ret;
  }

  /*
   * I know we are not supporting VBI on 525line systems today, but tomorrow
   * we may want to support closed caption, so keep the code generic, this
   * makes everything to get the temporal field order correct.
   */
  if(pCurrentMode->Mode == STVTG_TIMING_MODE_480I59940_13500)
  {
    pBuf->botFieldFirst = 1;
    lines[0] = (stm_ebu_teletext_t *)botField->data;
    lines[1] = (stm_ebu_teletext_t *)topField->data;

    if(topField->presentationTime != 0ULL)
      topField->presentationTime++;
  }
  else
  {
    lines[0] = (stm_ebu_teletext_t *)topField->data;
    lines[1] = (stm_ebu_teletext_t *)botField->data;

    if(botField->presentationTime != 0ULL)
      botField->presentationTime++;
  }

  if((ret = stmvbi_fill_metadata_buffers(pDev, (struct v4l2_sliced_vbi_data *)pBuf->vbiBuf.m.userptr, lines)) < 0)
    return ret;

  if(((stm_ebu_teletext_t *)topField->data)->ulValidLineMask != 0)
  {
    pBuf->topField = topField;
    pBuf->useCount++;
  }
  else
  {
    kfree(topField);
    topField = NULL;
  }

  if(((stm_ebu_teletext_t *)botField->data)->ulValidLineMask != 0)
  {
    ((stm_ebu_teletext_t *)botField->data)->ulValidLineMask |= 0x1; // Mark as for bottom field
    pBuf->botField = botField;
    pBuf->useCount++;
  }
  else
  {
    kfree(botField);
    botField = NULL;
  }

  if(pBuf->useCount == 0)
  {
    ret = -EINVAL;
    goto error_ret;
  }

  {
    unsigned long flags;

    BUG_ON (!list_empty (&pBuf->node));
    write_lock_irqsave (&pDev->pendingStreamQ.lock, flags);
    list_add_tail (&pBuf->node, &pDev->pendingStreamQ.list);
    write_unlock_irqrestore (&pDev->pendingStreamQ.lock, flags);
  }

  if (pDev->isStreaming)
    stmvbi_send_next_buffer_to_display(pDev);

  return 0;

error_ret:
  if(topField) kfree(topField);
  if(botField) kfree(botField);
  if(pBuf)     kfree(pBuf);
  return ret;
}


static int
stmvbi_close (struct stm_v4l2_handles    *handle,
              enum _stm_v4l2_driver_type  type,
              struct file                *file)
{
  stvbi_device_t *pDev;

  debug_msg ("%s in.\n", __FUNCTION__);

  pDev = &g_vbiData[handle->device];

  if (down_interruptible (&pDev->devLock))
    return -ERESTARTSYS;

  if(pDev->owner == file)
  {
    debug_msg ("%s flushing buffers.\n", __FUNCTION__);
    stmvbi_streamoff (pDev);
    pDev->owner = NULL;
    pDev->bufferFormat.type = 0;
  }

  up (&pDev->devLock);

  debug_msg ("%s out.\n", __FUNCTION__);

  return 0;
}


static int
stmvbi_ioctl (struct stm_v4l2_handles    *handle,
              struct stm_v4l2_driver     *driver,
              int                         minor,
              enum _stm_v4l2_driver_type  type,
              struct file                *file,
              unsigned int                cmd,
              void                       *arg)
{
  int ret;
  stvbi_device_t  *pDev = NULL;
  struct semaphore *pLock = NULL;

  pDev  = &g_vbiData[handle->device];
  pLock = &pDev->devLock;

  if (down_interruptible (pLock))
    return -ERESTARTSYS;

  if(pDev->owner != file)
  {
    if(pDev->owner == NULL)
    {
      pDev->owner = file;
      handle->v4l2type[type].handle = pDev;
    }
    else
      goto err_busy;
  }

  debug_msg ("%s: cmd = 0x%x\n", __FUNCTION__,cmd);

  switch (cmd)
  {
    case VIDIOC_G_FMT:
      {
        struct v4l2_format * const fmt = arg;

        if(pDev->bufferFormat.type != V4L2_BUF_TYPE_SLICED_VBI_OUTPUT)
          goto err_inval;

        fmt->fmt.sliced = pDev->bufferFormat.fmt.sliced;
      }
      break;

    case VIDIOC_S_FMT:
      {
        struct v4l2_format * const fmt = arg;

        if ((ret = stmvbi_set_buffer_format (pDev, fmt, 1)) < 0)
          goto err_ret;
      }
      break;

    case VIDIOC_TRY_FMT:
      {
        struct v4l2_format * const fmt = arg;

        if ((ret = stmvbi_set_buffer_format (pDev, fmt, 0)) < 0)
          goto err_ret;
      }
      break;

    case VIDIOC_REQBUFS:
      {
        struct v4l2_requestbuffers * const req = arg;

        if (req->memory != V4L2_MEMORY_USERPTR) {
          err_msg ("%s: VIDIOC_REQBUFS: unsupported type (%d) or memory "
                   "parameters (%d)\n", __FUNCTION__, req->type, req->memory);
          goto err_inval;
        }

      }
      break;

    case VIDIOC_QUERYBUF:
      goto err_inval;

    case VIDIOC_QBUF:
      {
        struct v4l2_buffer * const pBuf = arg;

        if (pBuf->memory != V4L2_MEMORY_USERPTR)
        {
          err_msg ("%s: VIDIOC_QBUF: illegal memory type %d\n", __FUNCTION__,
                   pBuf->memory);
          goto err_inval;
        }

        if(pDev->bufferFormat.type != V4L2_BUF_TYPE_SLICED_VBI_OUTPUT)
        {
          err_msg ("%s: VIDIOC_QBUF: no buffer format has been set\n", __FUNCTION__);
          goto err_inval;
        }

        if ((ret = stmvbi_queue_buffer (pDev, pBuf)) < 0)
          goto err_ret;
      }
      break;

    case VIDIOC_DQBUF:
      {
        struct v4l2_buffer * const pBuf = arg;

        /*
         * The API documentation states that the memory field must be set correctly
         * by the application for the call to succeed. It isn't clear under what
         * circumstances this would ever be needed and the requirement
         * seems a bit over zealous, but we honour it anyway. I bet users will
         * miss the small print and log lots of bugs that this call doesn't work.
         */
        if (pBuf->memory != V4L2_MEMORY_USERPTR)
        {
          err_msg ("%s: VIDIOC_DQBUF: illegal memory type %d\n",
                   __FUNCTION__, pBuf->memory);
          goto err_inval;
        }

        /*
         * We can release the driver lock now as stmvbi_dequeue_buffer is safe
         * (the buffer queues have their own access locks), this makes
         * the logic a bit simpler particularly in the blocking case.
         */
        up (pLock);

        if ((file->f_flags & O_NONBLOCK) == O_NONBLOCK) {
          debug_msg ("%s: VIDIOC_DQBUF: Non-Blocking dequeue\n", __FUNCTION__);
          if (!stmvbi_dequeue_buffer (pDev, pBuf))
            return -EAGAIN;

          return 0;
        } else {
          debug_msg ("%s: VIDIOC_DQBUF: Blocking dequeue\n", __FUNCTION__);
          return wait_event_interruptible (pDev->wqBufDQueue,
                                           stmvbi_dequeue_buffer (pDev, pBuf));
        }
      }
      break;

    case VIDIOC_STREAMON:
      {
        unsigned long ctrlCaps;
        if (pDev->isStreaming) {
          err_msg ("%s: VIDIOC_STREAMON: device already streaming\n",
                   __FUNCTION__);
          goto err_busy;
        }

  #if defined(_STRICT_V4L2_API)
        if (!stmvbi_has_queued_buffers (pDev)) {
          /* The API states that at least one buffer must be queued before
             STREAMON succeeds. */
          err_msg ("%s: VIDIOC_STREAMON: no buffers queued on stream\n",
                   __FUNCTION__);
          goto err_inval;
        }
  #endif

        debug_msg ("%s: VIDIOC_STREAMON: Starting the stream\n", __FUNCTION__);
        if(stm_display_output_get_control_capabilities(pDev->pOutput,&ctrlCaps)<0)
          goto err_restart;

        if((ctrlCaps & STM_CTRL_CAPS_TELETEXT) == 0)
          goto err_inval;

        if(stm_display_output_set_control(pDev->pOutput, STM_CTRL_TELETEXT_SYSTEM, STM_TELETEXT_SYSTEM_B)<0)
          goto err_restart;

        pDev->isStreaming = 1;
        wake_up_interruptible (&(pDev->wqStreamingState));

        /* Make sure any frames already on the pending queue are written to the
           hardware */
        stmvbi_send_next_buffer_to_display (pDev);
      }
      break;

    case VIDIOC_STREAMOFF:
      if ((ret = stmvbi_streamoff (pDev)) < 0)
        goto err_ret;

      break;

    case VIDIOC_G_SLICED_VBI_CAP:
      {
        int i;
        struct v4l2_sliced_vbi_cap * vbicap = arg;
        if(vbicap->type != V4L2_BUF_TYPE_SLICED_VBI_OUTPUT)
          goto err_inval;

        vbicap->service_set = V4L2_SLICED_TELETEXT_B;
        for(i=0;i<7;i++)
        {
          vbicap->service_lines[0][i] = 0;
          vbicap->service_lines[1][i] = 0;
        }

        vbicap->service_lines[0][23] = 0;
        vbicap->service_lines[1][23] = 0;

        for(i=7;i<23;i++)
        {
          vbicap->service_lines[0][i] = V4L2_SLICED_TELETEXT_B;
          vbicap->service_lines[1][i] = V4L2_SLICED_TELETEXT_B;
        }

      }
      break;

    default:
      ret = -ENOTTY;
      goto err_ret;
  }

  if (pLock) up (pLock);
  return 0;

err_busy:
  if (pLock) up (pLock);
  return -EBUSY;

err_restart:
    if (pLock) up (pLock);
    return -ERESTARTSYS;

err_inval:
  if (pLock) up (pLock);
  return -EINVAL;

err_ret:
  if (pLock) up (pLock);
  return ret;
}


/************************************************
 * v4l2_poll
 ************************************************/
static unsigned int
stmvbi_poll (struct stm_v4l2_handles    *handle,
             enum _stm_v4l2_driver_type  type,
             struct file                *file,
             poll_table                 *table)
{
  stvbi_device_t *pDev;
  unsigned int     mask;

  pDev = &g_vbiData[handle->device];

  if (down_interruptible (&pDev->devLock))
    return -ERESTARTSYS;

  /* Add DQueue wait queue to the poll wait state */
  poll_wait (file, &pDev->wqBufDQueue,      table);
  poll_wait (file, &pDev->wqStreamingState, table);

  if (!pDev->isStreaming)
    mask = POLLOUT; /* Write is available when not streaming */
  else if (stmvbi_has_completed_buffers (pDev))
    mask = POLLIN; /* Indicate we can do a DQUEUE ioctl */
  else
    mask = 0;

  up (&pDev->devLock);

  return mask;
}


static int stmvbi_configure_device (stvbi_device_t *pDev, int pipelinenr)
{

  debug_msg("%s in. pDev = 0x%08X\n", __FUNCTION__, (unsigned int)pDev);

  init_waitqueue_head(&(pDev->wqBufDQueue));
  init_waitqueue_head(&(pDev->wqStreamingState));
  sema_init(&(pDev->devLock),1);

  stmcore_get_display_pipeline(pipelinenr,&pDev->displayPipe);

  pDev->pOutput = stm_display_get_output(pDev->displayPipe.device,
                                         pDev->displayPipe.main_output_id);

  BUG_ON(pDev->pOutput == 0);

  if(!pDev->pOutput)
    return -ENODEV;

  pDev->owner = NULL;

  rwlock_init (&pDev->pendingStreamQ.lock);
  INIT_LIST_HEAD (&pDev->pendingStreamQ.list);

  rwlock_init (&pDev->completeStreamQ.lock);
  INIT_LIST_HEAD (&pDev->completeStreamQ.list);

  debug_msg("%s out.\n", __FUNCTION__);

  return 0;
}


static struct stm_v4l2_driver stmvbi_stm_v4l2_driver = {
  .name  = "stmvbi",
  .type  = STM_V4L2_SLICED_VBI_OUTPUT,

  .ioctl = stmvbi_ioctl,
  .close = stmvbi_close,
  .poll  = stmvbi_poll
};


static void __exit stmvbi_exit (void)
{
  int i;

  debug_msg("%s\n", __PRETTY_FUNCTION__);

  stm_v4l2_unregister_driver (&stmvbi_stm_v4l2_driver);

  for(i=0;i<MAX_PIPELINES;i++)
  {
    if(g_vbiData[i].pOutput)
      stm_display_output_release(g_vbiData[i].pOutput);
  }
}


static int __init stmvbi_init (void)
{
  struct stmcore_display_pipeline_data tmpdisplaypipe;
  int                                  pipeline;

  debug_msg("%s\n", __PRETTY_FUNCTION__);

  pipeline = 0;
  while (stmcore_get_display_pipeline (pipeline, &tmpdisplaypipe) == 0) {
    /* complain when we need to increase STM_V4L2_MAX_DEVICES */
    if (pipeline >= STM_V4L2_MAX_DEVICES) {
      WARN_ON (pipeline >= STM_V4L2_MAX_DEVICES);
      break;
    }

    if(stmvbi_configure_device (&g_vbiData[pipeline], pipeline)<0)
      return -ENODEV;

    pipeline++;
  }

  return stm_v4l2_register_driver (&stmvbi_stm_v4l2_driver);
}

module_init (stmvbi_init);
module_exit (stmvbi_exit);

MODULE_LICENSE ("GPL");
