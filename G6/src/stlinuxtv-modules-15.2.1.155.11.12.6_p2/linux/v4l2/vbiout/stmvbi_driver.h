/***********************************************************************
 * File: linux/kernel/drivers/video/stmvbi_driver.h
 * Copyright (c) 2009 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
 ***********************************************************************/

#ifndef __STMVBI_DRIVER_H
#define __STMVBI_DRIVER_H

#ifdef DEBUG
#define PKMOD "stmvbi: "
#define debug_msg(fmt,arg...) printk(PKMOD fmt,##arg)
#define err_msg(fmt,arg...) printk(PKMOD fmt,##arg)
#define info_msg(fmt,arg...) printk(PKMOD fmt,##arg)
#else
#define debug_msg(fmt,arg...)
#define err_msg(fmt,arg...)
#define info_msg(fmt,arg...)
#endif

struct stm_v4l2_queue {
	rwlock_t lock;
	struct list_head list;	/* of type struct _streaming_buffer */
};

typedef struct _streaming_buffer {
	struct list_head node;
	struct v4l2_buffer vbiBuf;
	struct stmvbi_dev *pDev;
	int useCount;
	int botFieldFirst;
	stm_display_metadata_t *topField;
	stm_display_metadata_t *botField;
} streaming_buffer_t;

#define STMVBI_MAX_OUTPUT_NB	2
struct stmvbi_dev {
	struct video_device viddev;
	struct media_pad *pads;

	struct stm_v4l2_queue pendingStreamQ;
	struct stm_v4l2_queue completeStreamQ;

	struct v4l2_format bufferFormat;

	struct file *owner;
	int isStreaming;

	stm_display_device_h hDevice;
	stm_display_output_h hOutput[STMVBI_MAX_OUTPUT_NB];
	const char *out_name[STMVBI_MAX_OUTPUT_NB];

	unsigned int output_id;
	stm_display_output_capabilities_t output_caps;
	unsigned int max_output_nb;

	wait_queue_head_t wqBufDQueue;
	wait_queue_head_t wqStreamingState;
	struct semaphore devLock;
};

static inline int stmvbi_has_queued_buffers(struct stmvbi_dev *pDev)
{
	return !list_empty(&pDev->pendingStreamQ.list);
}

static inline int stmvbi_has_completed_buffers(struct stmvbi_dev *pDev)
{
	return !list_empty(&pDev->completeStreamQ.list);
}

#endif /* __STMVOUT_DRIVER_H */
