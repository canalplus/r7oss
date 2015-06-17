/***********************************************************************
 * File: linux/drivers/video/stmvbi_driver.h
 * Copyright (c) 2009 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
 ***********************************************************************/

#ifndef __STMVBI_DRIVER_H
#define __STMVBI_DRIVER_H

#if defined DEBUG
#  define PKMOD "stmvbi: "
#  define debug_msg(fmt,arg...) printk(PKMOD fmt,##arg)
#  define err_msg(fmt,arg...) printk(PKMOD fmt,##arg)
#  define info_msg(fmt,arg...) printk(PKMOD fmt,##arg)
#else
#  define debug_msg(fmt,arg...)
#  define err_msg(fmt,arg...)
#  define info_msg(fmt,arg...)
#endif

#define MAX_PIPELINES 3

struct _stvbi_device;


struct stm_v4l2_queue
{
  rwlock_t         lock;
  struct list_head list; // of type struct _streaming_buffer
};


typedef struct _streaming_buffer
{
  struct list_head       node;
  struct v4l2_buffer     vbiBuf;
  struct _stvbi_device  *pDev;
  int                    useCount;
  int                    botFieldFirst;
  stm_meta_data_t       *topField;
  stm_meta_data_t       *botField;
} streaming_buffer_t;


typedef struct _stvbi_device
{
  struct stm_v4l2_queue   pendingStreamQ;
  struct stm_v4l2_queue   completeStreamQ;

  struct v4l2_format      bufferFormat;

  struct stmcore_display_pipeline_data displayPipe;

  struct file            *owner;
  int                     isStreaming;

  stm_display_output_t*   pOutput;

  int                     queues_inited;
  wait_queue_head_t       wqBufDQueue;
  wait_queue_head_t       wqStreamingState;
  struct semaphore        devLock;

} stvbi_device_t;


static inline int stmvbi_has_queued_buffers(stvbi_device_t *pDev)
{
  return !list_empty(&pDev->pendingStreamQ.list);
}


static inline int stmvbi_has_completed_buffers(stvbi_device_t *pDev)
{
  return !list_empty(&pDev->completeStreamQ.list);
}


#endif /* __STMVOUT_DRIVER_H */
