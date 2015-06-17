/***********************************************************************
 *
 * File: linux/kernel/drivers/media/video/stmvbi_driver.c
 * Copyright (c) 2009 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
 * V4L2 video output device driver for ST SoC display subsystems.
 *
\***********************************************************************/

#include <linux/slab.h>
#include <linux/version.h>
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/errno.h>
#include <linux/wait.h>
#include <linux/poll.h>
#include <linux/mm.h>
#include <linux/types.h>
#include <media/v4l2-dev.h>
#include <media/v4l2-ioctl.h>
#include <media/v4l2-fh.h>

#include <linux/semaphore.h>
#include <asm/page.h>

#include <stm_display.h>

#include "stmvbi_driver.h"
#include "stmedia.h"

static struct stmvbi_dev g_vbiData;

int stmvbi_find_output_with_capability(struct stmvbi_dev *pDev,
	stm_display_output_capabilities_t pCaps)
{
	stm_display_device_h hDevice = pDev->hDevice;
	stm_display_output_h hOutput;
	int ret, output;

	debug_msg("%s in. pDev = 0x%08X, pCaps = 0x%08X\n", __func__, (unsigned int)pDev, (unsigned int)pCaps);

	/* Remove the pCaps from the device caps */
	pDev->output_caps &= ~pCaps;

	/* Scan all outputs - looking for one with VBI */
	for (output = 0;; output++) {
		const char *name = "";
		stm_display_output_capabilities_t caps;

		if (pDev->max_output_nb >= STMVBI_MAX_OUTPUT_NB)
			break;

		ret = stm_display_device_open_output(hDevice, output, &hOutput);
		if (ret)
			break;

		ret = stm_display_output_get_capabilities(hOutput, &caps);
		if (ret) {
			printk(KERN_WARNING
					"%s: failed to get output capabilities (%d)\n",
					__func__, ret);
			continue;
		}

		/* Search for the output that has HW_TELETEXT support */
		if ((caps & pCaps) == 0) {
			stm_display_output_close(hOutput);
			continue;
		}

		ret = stm_display_output_get_name(hOutput, &name);
		if (ret) {
			printk(KERN_WARNING
					"%s: output without a name ?? (%d)\n", __func__,
					ret);
			stm_display_output_close(hOutput);
			continue;
		}

		if (caps & OUTPUT_CAPS_HW_TELETEXT) {
			debug_msg("%s: found output with Teletext capability (id=%d)\n",
					__func__, output);
		}

		if (caps & OUTPUT_CAPS_HW_CC) {
			debug_msg("%s: found output with Closed Caption capability (id=%d)\n",
					__func__, output);
		}

		pDev->hOutput[pDev->max_output_nb] = hOutput;
		pDev->out_name[pDev->max_output_nb] = name;
		pDev->max_output_nb++;
		pDev->output_caps = caps;

	}

	if (!pDev->max_output_nb)
		return -ENODEV;

	return 0;
}

void stmvbi_send_next_buffer_to_display(struct stmvbi_dev *pDev)
{
	streaming_buffer_t *pStreamBuffer, *tmp;
	unsigned long flags;
	int ret;
	stm_display_metadata_t *firstField;
	stm_display_metadata_t *secondField;

	read_lock_irqsave(&pDev->pendingStreamQ.lock, flags);
	list_for_each_entry_safe(pStreamBuffer, tmp, &pDev->pendingStreamQ.list,
				 node) {
		read_unlock_irqrestore(&pDev->pendingStreamQ.lock, flags);

		debug_msg("%s: pDev = %p, pStreamBuffer = %p\n", __func__,
			  pDev, pStreamBuffer);

		if (pStreamBuffer->botFieldFirst) {
			firstField = pStreamBuffer->botField;
			secondField = pStreamBuffer->topField;
		} else {
			firstField = pStreamBuffer->topField;
			secondField = pStreamBuffer->botField;
		}

		if (firstField) {
			ret =
			    stm_display_output_queue_metadata(pDev->
							      hOutput[pDev->
								      output_id],
                    firstField);

			if (ret < 0) {
				err_msg("%s: queueing first field failed (%d) \n",
						__PRETTY_FUNCTION__, ret);
				return;
			}
		}

		if (secondField) {
			ret =
			    stm_display_output_queue_metadata(pDev->
							      hOutput[pDev->
								      output_id],
                    secondField);

			if (ret < 0) {
				err_msg("%s: queueing second field failed (%d)\n",
						__PRETTY_FUNCTION__, ret);
				return;
			}
		}

		pStreamBuffer->vbiBuf.flags |= V4L2_BUF_FLAG_QUEUED;

		write_lock_irqsave(&pDev->pendingStreamQ.lock, flags);
		list_del_init(&pStreamBuffer->node);
		write_unlock_irqrestore(&pDev->pendingStreamQ.lock, flags);

		read_lock_irqsave(&pDev->pendingStreamQ.lock, flags);
	}
	read_unlock_irqrestore(&pDev->pendingStreamQ.lock, flags);
}

int stmvbi_dequeue_buffer(struct stmvbi_dev *pDev, struct v4l2_buffer *pVBIBuf)
{
	streaming_buffer_t *pBuf;
	unsigned long flags;

	debug_msg("%s: pDev = %p, pVBIBuf = %p\n", __func__, pDev, pVBIBuf);

	write_lock_irqsave(&pDev->completeStreamQ.lock, flags);
	list_for_each_entry(pBuf, &pDev->completeStreamQ.list, node) {
		list_del_init(&pBuf->node);
		write_unlock_irqrestore(&pDev->completeStreamQ.lock, flags);

		pBuf->vbiBuf.flags &=
		    ~(V4L2_BUF_FLAG_QUEUED | V4L2_BUF_FLAG_DONE);

		*pVBIBuf = pBuf->vbiBuf;

		kfree(pBuf);

		return 1;
	}
	write_unlock_irqrestore(&pDev->completeStreamQ.lock, flags);

	return 0;
}

int stmvbi_streamoff(struct stmvbi_dev *pDev)
{
	streaming_buffer_t *newBuf, *tmp;
	unsigned long flags;

	debug_msg("%s: pDev = %p\n", __func__, pDev);

	/* Discard the pending queue */
	write_lock_irqsave(&pDev->pendingStreamQ.lock, flags);
	list_for_each_entry_safe(newBuf, tmp, &pDev->pendingStreamQ.list, node) {
		list_del(&newBuf->node);
		/* Must manually free the meta data here */
		kfree(newBuf->topField);
		/* Must manually free the meta data here */
		kfree(newBuf->botField);
		kfree(newBuf);
	}
	write_unlock_irqrestore(&pDev->pendingStreamQ.lock, flags);

	if(pDev->bufferFormat.fmt.sliced.service_set & V4L2_SLICED_TELETEXT_B) {
		debug_msg("%s: About to flush TTX metadata\n", __func__);
		stm_display_output_flush_metadata(pDev->hOutput[pDev->output_id],
				STM_METADATA_TYPE_TELETEXT);
	}

	if(pDev->bufferFormat.fmt.sliced.service_set & V4L2_SLICED_CAPTION_525) {
		debug_msg("%s: About to flush CC metadata\n", __func__);
		stm_display_output_flush_metadata(pDev->hOutput[pDev->output_id],
				STM_METADATA_TYPE_CLOSED_CAPTION);
		debug_msg("%s: About to disable CC HW\n", __func__);
		stm_display_output_set_control(pDev->hOutput[pDev->output_id], OUTPUT_CTRL_CC_INSERTION_ENABLE, 0);
	}

	debug_msg("%s: About to dequeue completed queue\n", __func__);

	/* dqueue all buffers from the complete queue */
	write_lock_irqsave(&pDev->completeStreamQ.lock, flags);
	list_for_each_entry_safe(newBuf, tmp, &pDev->completeStreamQ.list, node) {
		list_del(&newBuf->node);
		/* metadata already released as we are on the completed Q */
		kfree(newBuf);
	}
	write_unlock_irqrestore(&pDev->completeStreamQ.lock, flags);

	pDev->isStreaming = 0;
	wake_up_interruptible(&(pDev->wqStreamingState));

	debug_msg("%s\n", __func__);
	return 0;
}

static int
stmvbi_set_buffer_format(struct stmvbi_dev *pDev,
			 struct v4l2_format *fmt, int updateConfig)
{
	stm_display_mode_t currentMode;
	int servicelinecount = 0;
	int i,j;
	int ret;
	int cc_field = 0;

	ret = stm_display_output_get_current_display_mode(pDev->hOutput[pDev->output_id], &currentMode);
	if (ret < 0) {
		if (signal_pending(current))
			return -ERESTARTSYS;
		else
			return -EIO;
	}

	if(fmt->fmt.sliced.service_set != 0) {
		/*
		 * List of supported VBI services sorted by priority
		 */

		/*
		 * Priority 1: Teletext
		 * We only support EBU Teletext on 625line PAL/SECAM systems
		 */
		if(fmt->fmt.sliced.service_set & V4L2_SLICED_TELETEXT_B) {
			if (((pDev->output_caps & OUTPUT_CAPS_HW_TELETEXT) == 0) || (currentMode.mode_id != STM_TIMING_MODE_576I50000_13500)) {
				debug_msg("%s: feature not supported\n", __func__);
				return -EINVAL;
			}

			for (j = 0; j < 2 ; j++) {
				for (i = 0; i <= 23 ; i++) {
					if(fmt->fmt.sliced.service_lines[j][i] & V4L2_SLICED_TELETEXT_B) {
						if((i < 6) || (i > 22)) {
							fmt->fmt.sliced.service_lines[j][i] = 0;
						}
						else {
							fmt->fmt.sliced.service_lines[j][i] = V4L2_SLICED_TELETEXT_B;
							servicelinecount++;
						}
					}
				}
			}
		}
		/*
		 * Priority 2 : Closed Caption
		 * We only support closed caption 525 on 525 line  systems
		 */
		if(fmt->fmt.sliced.service_set & V4L2_SLICED_CAPTION_525) {
			if (((pDev->output_caps & OUTPUT_CAPS_HW_CC) == 0) || (currentMode.mode_id != STM_TIMING_MODE_480I59940_13500)) {
				debug_msg("%s: feature not supported\n", __func__);
				return -EINVAL;
			}

			for (j = 0; j < 2 ; j++) {
				for (i = 0; i <= 23 ; i++) {
					if(fmt->fmt.sliced.service_lines[j][i] & V4L2_SLICED_CAPTION_525) {
						if((i < 10) || (i > 22)) {
							fmt->fmt.sliced.service_lines[j][i] = 0;
						}
						else {
							fmt->fmt.sliced.service_lines[j][i] = V4L2_SLICED_CAPTION_525;
							servicelinecount++;
							cc_field |= (1L << j);
						}
					}
				}
			}
		}
	} else {
		for (j = 0; j < 2 ; j++) {
			for (i = 0; i <= 23; i++) {
				/*
				 * List of supported VBI services sorted by priority
				 */

				/*
				 * Priority 1: Teletext
				 * We only support EBU Teletext on 625 line PAL/SECAM systems
				 */
				if((fmt->fmt.sliced.service_lines[j][i] & V4L2_SLICED_TELETEXT_B) && (pDev->output_caps & OUTPUT_CAPS_HW_TELETEXT) && (currentMode.mode_id == STM_TIMING_MODE_576I50000_13500)) {
					if((i < 6) || (i > 22)) {
						fmt->fmt.sliced.service_lines[j][i] = 0;
					}
					else {
						fmt->fmt.sliced.service_set |= V4L2_SLICED_TELETEXT_B;
						fmt->fmt.sliced.service_lines[j][i] = V4L2_SLICED_TELETEXT_B;
						servicelinecount++;
					}
				}
				/*
				 * Priority 2 : Closed Caption
				 * We only support closed caption 525 on 525 line systems
				 */
				else if((fmt->fmt.sliced.service_lines[j][i] & V4L2_SLICED_CAPTION_525) && (pDev->output_caps & OUTPUT_CAPS_HW_CC) && (currentMode.mode_id == STM_TIMING_MODE_480I59940_13500)) {
					if((i < 10) || (i > 22)) {
						fmt->fmt.sliced.service_lines[j][i] = 0;
					} else {
						fmt->fmt.sliced.service_set |= V4L2_SLICED_CAPTION_525;
						fmt->fmt.sliced.service_lines[j][i] = V4L2_SLICED_CAPTION_525;
						servicelinecount++;
						cc_field |= (1L << j);
					}
				}
				else
				{
					fmt->fmt.sliced.service_lines[j][i] = 0;
				}
			}
		}
	}

	/*
	 * Enable the requested VBI HW by this device
	 */
	if(fmt->fmt.sliced.service_set & V4L2_SLICED_TELETEXT_B) {
		debug_msg("%s: Enabling TTX HW\n", __func__);
		ret = stm_display_output_set_control(pDev->hOutput[pDev->output_id],
				OUTPUT_CTRL_TELETEXT_SYSTEM,
				STM_TELETEXT_SYSTEM_B);
	}

	if(fmt->fmt.sliced.service_set & V4L2_SLICED_CAPTION_525) {
		debug_msg("%s: Enabling CC HW\n", __func__);
		ret = stm_display_output_set_control(pDev->hOutput[pDev->output_id],
				OUTPUT_CTRL_CC_INSERTION_ENABLE,
				cc_field);
	}

	if (ret) {
		printk(KERN_ERR "%s: failed to enable HW buffer format (%d)\n",
				__func__, ret);
		return ret;
	}

	fmt->fmt.sliced.io_size =
		sizeof(struct v4l2_sliced_vbi_data) *servicelinecount;
	memset(fmt->fmt.sliced.reserved, 0, sizeof(fmt->fmt.sliced.reserved));

	debug_msg("%s: Total service count is %d\n", __func__, servicelinecount);

	if (updateConfig) {
		pDev->bufferFormat.type = V4L2_BUF_TYPE_SLICED_VBI_OUTPUT;
		pDev->bufferFormat.fmt.sliced = fmt->fmt.sliced;
	}

	return 0;
}

static void stmvbi_release_metadata(struct stm_display_metadata_s *s)
{
	streaming_buffer_t *pBuf = (streaming_buffer_t *) s->private_data;

	debug_msg("%s: metadata = %p pBuf = %p\n", __func__, s, pBuf);

	if (pBuf->topField == s)
		pBuf->topField = NULL;

	if (pBuf->botField == s)
		pBuf->botField = NULL;

	kfree(s);

	BUG_ON(pBuf->useCount == 0);
	pBuf->useCount--;

	if (pBuf->pDev->isStreaming && (pBuf->useCount == 0)) {
		unsigned long flags;
		pBuf->vbiBuf.flags |= V4L2_BUF_FLAG_DONE;

		write_lock_irqsave(&pBuf->pDev->completeStreamQ.lock, flags);
		list_add_tail(&pBuf->node, &pBuf->pDev->completeStreamQ.list);
		write_unlock_irqrestore(&pBuf->pDev->completeStreamQ.lock,
					flags);

		wake_up_interruptible(&pBuf->pDev->wqBufDQueue);
	}

}

static stm_display_metadata_t *stmvbi_allocate_metadata(int vbi_type, streaming_buffer_t *pBuf)
{
	stm_closed_caption_t *c = NULL;
	stm_teletext_t *t = NULL;
	stm_display_metadata_t *m = NULL;
	int size = 0;

	switch(vbi_type) {
	case V4L2_SLICED_CAPTION_525:
		size = sizeof(stm_display_metadata_t) +
			sizeof(stm_closed_caption_t);
		m = kzalloc(size, GFP_KERNEL);
		if (!m)
			return NULL;
		m->type = STM_METADATA_TYPE_CLOSED_CAPTION;
		c = (stm_closed_caption_t *) m->data;
#ifdef DEBUG_VBI_BUFFER_SETUP
		debug_msg("%s: metadata = %p size = %d c = %p.\n",__PRETTY_FUNCTION__, m, size, c);
#endif
		break;
	case V4L2_SLICED_TELETEXT_B:
		size = sizeof(stm_display_metadata_t) +
			sizeof(stm_teletext_t) + (sizeof(stm_teletext_line_t) *18);
		m = kzalloc(size, GFP_KERNEL);
	       if (!m)
			return NULL;
		m->type = STM_METADATA_TYPE_TELETEXT;
		t = (stm_teletext_t *) m->data;
		t->lines =(stm_teletext_line_t *) (((char *)m->data) + sizeof(stm_teletext_t));
#ifdef DEBUG_VBI_BUFFER_SETUP
		debug_msg("%s: metadata = %p size = %d t = %p t->lines = %p.\n",__PRETTY_FUNCTION__, m, size, t, t->lines);
#endif
		break;
	default:
		if (!m)
			return NULL;

	}

	m->size = size;
	m->presentation_time =
		((stm_time64_t) pBuf->vbiBuf.timestamp.tv_sec * USEC_PER_SEC) +
		(stm_time64_t) pBuf->vbiBuf.timestamp.tv_usec;
	m->private_data = pBuf;
	m->release = &stmvbi_release_metadata;

	return m;
}

static int
stmvbi_fill_closedcaption_buffers(struct stmvbi_dev * pDev,
			struct v4l2_sliced_vbi_data *data,
      stm_closed_caption_t *topField, stm_closed_caption_t *botField, unsigned int *vfileds)
{
	int packet;
	int lastline = 9;
	int lastfield = 0;
	int npackets =
		pDev->bufferFormat.fmt.sliced.io_size /
		sizeof(struct v4l2_sliced_vbi_data);

#ifdef DEBUG_VBI_BUFFER_SETUP
	debug_msg("%s: in - npackets = %d\n", __PRETTY_FUNCTION__, npackets);
#endif

	*vfileds = 0;

	for (packet = 0; packet < npackets; packet++) {
		switch (data[packet].id) {
			case 0:
#ifdef DEBUG_VBI_BUFFER_SETUP
				debug_msg("%s: ignoring packet %d\n", __PRETTY_FUNCTION__, packet);
#endif
				continue;
			case V4L2_SLICED_CAPTION_525:
				break;
			default:
#ifdef DEBUG_VBI_BUFFER_SETUP
				err_msg("%s: invalid packet id\n", __PRETTY_FUNCTION__);
#endif
				return -EINVAL;
		}

		if ((data[packet].line <= lastline)
				|| (data[packet].field < lastfield)) {
#ifdef DEBUG_VBI_BUFFER_SETUP
			err_msg("%s: invalid packet line/field (%d/%d)\n", __PRETTY_FUNCTION__, data[packet].line, data[packet].field);
#endif
			return -EINVAL;
		}

		if ((data[packet].line > 22) || (data[packet].field > 1)) {
#ifdef DEBUG_VBI_BUFFER_SETUP
			err_msg("%s: invalid packet line/field (%d/%d)\n", __PRETTY_FUNCTION__, data[packet].line, data[packet].field);
#endif
			return -EINVAL;
		}

		if ((pDev->bufferFormat.fmt.sliced.
					service_lines[data[packet].field][data[packet].
					line] & data[packet].
					id) == 0) {
#ifdef DEBUG_VBI_BUFFER_SETUP
			err_msg("%s: packet id does not match cc id=%x (p=%d,i=%x,l=%d,f=%d)\n", __PRETTY_FUNCTION__,
					V4L2_SLICED_CAPTION_525,
					packet,
					data[packet].id,
					data[packet].line,
					data[packet].field);
#endif
			return -EINVAL;
		}

#ifdef DEBUG_VBI_BUFFER_SETUP
		debug_msg("%s: copying field %d, line %d, data[0] 0x%x, data[1] 0x%x\n",
				__FUNCTION__, data[packet].field, data[packet].line,
				data[packet].data[0], data[packet].data[1]
			 );
#endif
		if(data[packet].field == 0)
		{
			*vfileds |= (1L << 0);
			topField->field = data[packet].field;
			topField->lines_field = data[packet].line - 6;
			topField->data[0] = data[packet].data[0];
			topField->data[1] = data[packet].data[1];
		}
		else if(data[packet].field == 1)
		{
			*vfileds |= (1L << 1);
			botField->field = data[packet].field;
			botField->lines_field = data[packet].line - 6;
			botField->data[0] = data[packet].data[0];
			botField->data[1] = data[packet].data[1];
		}
	}
#ifdef DEBUG_VBI_BUFFER_SETUP
	debug_msg("%s: out\n", __PRETTY_FUNCTION__);
#endif
	return 0;
}

static int stmvbi_fill_teletext_buffers(struct stmvbi_dev *pDev,
			    struct v4l2_sliced_vbi_data *data,
			     stm_teletext_t *ttxtdata[2])
{
	int packet;
	int lastline = 5;
	int lastfield = 0;
	int npackets =
		pDev->bufferFormat.fmt.sliced.io_size /
		sizeof(struct v4l2_sliced_vbi_data);

#ifdef DEBUG_VBI_BUFFER_SETUP
	debug_msg("%s: in - npackets = %d\n", __PRETTY_FUNCTION__, npackets);
#endif

	for (packet = 0; packet < npackets; packet++) {
		switch (data[packet].id) {
			case 0:
#ifdef DEBUG_VBI_BUFFER_SETUP
				debug_msg("%s: ignoring packet %d\n", __PRETTY_FUNCTION__, packet);
#endif
				continue;
			case V4L2_SLICED_TELETEXT_B:
				break;
			default:
#ifdef DEBUG_VBI_BUFFER_SETUP
				err_msg("%s: invalid packet id\n", __PRETTY_FUNCTION__);
#endif
				return -EINVAL;
		}

		if ((data[packet].line <= lastline)
				|| (data[packet].field < lastfield)) {
#ifdef DEBUG_VBI_BUFFER_SETUP
			err_msg("%s: invalid packet line/field (%d/%d)\n", __PRETTY_FUNCTION__, data[packet].line, data[packet].field);
#endif
			return -EINVAL;
		}

		if ((data[packet].line > 22) || (data[packet].field > 1)) {
#ifdef DEBUG_VBI_BUFFER_SETUP
			err_msg("%s: invalid packet line/field (%d/%d)\n", __PRETTY_FUNCTION__, data[packet].line, data[packet].field);
#endif
			return -EINVAL;
		}

		if ((pDev->bufferFormat.fmt.
					sliced.service_lines[data[packet].
					field][data[packet].line] &
					data[packet].id) == 0) {
#ifdef DEBUG_VBI_BUFFER_SETUP
			err_msg("%s: packet id does not match ttx id=%x (p=%d,i=%x,l=%d,f=%d)\n", __PRETTY_FUNCTION__,
					V4L2_SLICED_TELETEXT_B,
					packet,
					data[packet].id,
					data[packet].line,
					data[packet].field);
#endif
			return -EINVAL;
		}

#ifdef DEBUG_VBI_BUFFER_SETUP
		debug_msg("%s: copying field %d, line %d: memcpy(%p,%p,%d)\n",
				__func__, data[packet].field, data[packet].line,
				ttxtdata[data[packet].field]->
				lines[data[packet].line - 6].line_data,
				data[packet].data, sizeof(stm_teletext_line_t));
#endif

		memcpy(ttxtdata[data[packet].field]->lines
				[data[packet].line - 6].line_data, data[packet].data,
				sizeof(stm_teletext_line_t));
		ttxtdata[data[packet].field]->valid_line_mask |=
			(1L << data[packet].line);
	}
#ifdef DEBUG_VBI_BUFFER_SETUP
	debug_msg("%s: out\n", __PRETTY_FUNCTION__);
#endif
	return 0;
}

static int teletext_queue_buffer(struct stmvbi_dev * pDev, streaming_buffer_t *pBuf)
{
	stm_display_metadata_t *topField = NULL;
	stm_display_metadata_t *botField = NULL;
	stm_teletext_t *lines[2] = { NULL };
	int ret;

	topField = stmvbi_allocate_metadata(V4L2_SLICED_TELETEXT_B, pBuf);
	if (topField == NULL) {
		ret = -ENOMEM;
		goto error_ret;
	}

	botField = stmvbi_allocate_metadata(V4L2_SLICED_TELETEXT_B, pBuf);
	if (botField == NULL) {
		ret = -ENOMEM;
		goto error_ret;
	}

	pBuf->botFieldFirst = 1;
	lines[0] = (stm_teletext_t *) botField->data;
	lines[1] = (stm_teletext_t *) topField->data;
	if (topField->presentation_time != 0ULL)
		topField->presentation_time++;

	ret = stmvbi_fill_teletext_buffers(pDev,
			(struct v4l2_sliced_vbi_data *)
			pBuf->vbiBuf.m.userptr, lines);
	if (ret < 0)
		goto error_ret;


	if (((stm_teletext_t *) topField->data)->valid_line_mask != 0) {
		pBuf->topField = topField;
		pBuf->useCount++;
	} else {
		kfree(topField);
		topField = NULL;
	}

	if (((stm_teletext_t *) botField->data)->valid_line_mask != 0) {
		/* Mark as for bottom field */
		((stm_teletext_t *) botField->data)->valid_line_mask |= 0x1;
		pBuf->botField = botField;
		pBuf->useCount++;
	} else {
		kfree(botField);
		botField = NULL;
	}

	if (pBuf->useCount == 0) {
		err_msg("%s: invalid lines mask\n", __PRETTY_FUNCTION__);
		ret = -EINVAL;
		goto error_ret;
	}

	return 0;

error_ret:
	/* kfree(NULL) is safe */
	kfree(topField);
	kfree(botField);
	kfree(pBuf);
	return ret;
}

static int closedcaption_queue_buffer(struct stmvbi_dev * pDev, streaming_buffer_t *pBuf)
{
	stm_display_metadata_t *topField = NULL;
	stm_display_metadata_t *botField = NULL;
	int ret;
  unsigned int vfileds = 0;

#ifdef DEBUG_VBI_BUFFER_SETUP
  debug_msg("%s: in\n",__func__);
#endif

	if ((topField = stmvbi_allocate_metadata(V4L2_SLICED_CAPTION_525, pBuf)) == NULL) {
	ret = -ENOMEM;
	goto error_ret;
	}

	if ((botField = stmvbi_allocate_metadata(V4L2_SLICED_CAPTION_525, pBuf)) == NULL) {
		ret = -ENOMEM;
		goto error_ret;
	}

  if (topField->presentation_time != 0ULL)
	topField->presentation_time++;

	if ((ret = stmvbi_fill_closedcaption_buffers(pDev,
					  (struct v4l2_sliced_vbi_data *)pBuf->
            vbiBuf.m.userptr, (stm_closed_caption_t*)topField->data, (stm_closed_caption_t*)botField->data, &vfileds)) < 0)
    goto error_ret;

  if (vfileds & (1L << 0)) {
		pBuf->topField = topField;
		pBuf->useCount++;

	} else {
		kfree(topField);
		topField = NULL;
	}

  if (vfileds & (1L << 1)) {
		pBuf->botField = botField;
		pBuf->useCount++;

	} else {
		kfree(botField);
		botField = NULL;

	}

	if (pBuf->useCount == 0) {
		ret = -EINVAL;

		goto error_ret;

	}
#ifdef DEBUG_VBI_BUFFER_SETUP
  debug_msg("%s: out\n",__func__);
#endif
	return 0;

error_ret:
	/* kfree(NULL) is safe */
	kfree(topField);
	kfree(botField);
	kfree(pBuf);
#ifdef DEBUG_VBI_BUFFER_SETUP
  debug_msg("%s: exiting with error %d\n",__func__, ret);
#endif
	return ret;
}

static int
stmvbi_queue_buffer(struct stmvbi_dev *pDev, struct v4l2_buffer *pVBIBuf)
{
	unsigned long flags;
	stm_display_mode_t currentMode;
	streaming_buffer_t *pBuf = NULL;
	int ret=0;

	if (pVBIBuf->memory != V4L2_MEMORY_USERPTR)
		return -EINVAL;

	if (!access_ok
			(VERIFY_READ, (void *)pVBIBuf->m.userptr,
			 pDev->bufferFormat.fmt.sliced.io_size))
		return -EFAULT;

	ret = stm_display_output_get_current_display_mode(pDev->
			hOutput[pDev->
			output_id],
			&currentMode);
	if (ret < 0) {
		if (signal_pending(current))
			return -ERESTARTSYS;
		else
			return -EIO;
	}

	pBuf = kzalloc(sizeof(streaming_buffer_t), GFP_KERNEL);
	if (pBuf == NULL)
		return -ENOMEM;

	INIT_LIST_HEAD(&pBuf->node);
	pBuf->pDev = pDev;
	pBuf->vbiBuf = *pVBIBuf;
	pBuf->vbiBuf.flags &= ~(V4L2_BUF_FLAG_DONE | V4L2_BUF_FLAG_QUEUED);

	if (pDev->output_caps & OUTPUT_CAPS_HW_TELETEXT) {
		if (pDev->bufferFormat.fmt.sliced.service_set
				& V4L2_SLICED_TELETEXT_B)
		{
			if ((ret = teletext_queue_buffer(pDev, pBuf)) < 0)
				return ret;
		}
	}

	if (pDev->output_caps & OUTPUT_CAPS_HW_CC) {
		if(pDev->bufferFormat.fmt.sliced.service_set
				& V4L2_SLICED_CAPTION_525)
		{
			if ((ret = closedcaption_queue_buffer(pDev, pBuf)) < 0)
				return ret;
		}
	}

	BUG_ON(!list_empty(&pBuf->node));
	write_lock_irqsave(&pDev->pendingStreamQ.lock, flags);
	list_add_tail(&pBuf->node, &pDev->pendingStreamQ.list);
	write_unlock_irqrestore(&pDev->pendingStreamQ.lock, flags);

	if(pDev->isStreaming)
		stmvbi_send_next_buffer_to_display(pDev);

	return ret;
}

static int stmvbi_close(struct file *file)
{
	struct stmvbi_dev *dev = video_drvdata(file);

	debug_msg("%s in.\n", __func__);

	if (down_interruptible(&dev->devLock))
		return -ERESTARTSYS;

	if (dev->owner == file) {
		debug_msg("%s flushing buffers.\n", __func__);
		stmvbi_streamoff(dev);
		dev->owner = NULL;
		dev->bufferFormat.type = 0;
	}

	up(&dev->devLock);

	debug_msg("%s out.\n", __func__);

	return 0;
}

static int vidioc_enum_output(struct file *file, void *priv,
			      struct v4l2_output *a)
{
	struct stmvbi_dev *dev = video_drvdata(file);
	int index = a->index;

	if (a->index >= dev->max_output_nb)
		return -EINVAL;

	memset(a, 0, sizeof(struct v4l2_output));

	snprintf(a->name, sizeof(a->name), "%s", dev->out_name[index]);
	a->type = V4L2_OUTPUT_TYPE_ANALOG;
	a->index = index;

	return 0;
}

static int vidioc_s_output(struct file *file, void *priv, unsigned int i)
{
	struct stmvbi_dev *dev = video_drvdata(file);

	if (dev->isStreaming)
		return -EBUSY;

	if (i >= dev->max_output_nb)
		return -EINVAL;

	dev->output_id = i;

	return 0;
}

static int vidioc_g_output(struct file *file, void *priv, unsigned int *i)
{
	struct stmvbi_dev *dev = video_drvdata(file);

	*i = dev->output_id;

	return 0;
}

static int vidioc_querycap(struct file *file, void *priv,
			   struct v4l2_capability *cap)
{
	strlcpy(cap->driver, "Analog VBI Out", sizeof(cap->driver));
	strlcpy(cap->card, "STMicroelectronics", sizeof(cap->card));
	strlcpy(cap->bus_info, "platform: ST SoC", sizeof(cap->bus_info));

	cap->version = LINUX_VERSION_CODE;
	cap->device_caps = V4L2_CAP_SLICED_VBI_OUTPUT | V4L2_CAP_STREAMING;
	cap->capabilities = cap->device_caps | V4L2_CAP_DEVICE_CAPS;

	return 0;
}

static int vidioc_g_fmt_sliced_vbi_out(struct file *file, void *priv,
				       struct v4l2_format *f)
{
	struct stmvbi_dev *dev = video_drvdata(file);

	f->fmt.sliced = dev->bufferFormat.fmt.sliced;

	return 0;
}

static int vidioc_s_fmt_sliced_vbi_out(struct file *file, void *priv,
				       struct v4l2_format *f)
{
	struct stmvbi_dev *dev = video_drvdata(file);
	int ret;
	ret = stmvbi_set_buffer_format(dev, f, 1);
	if (ret) {
		printk(KERN_ERR "%s: failed to set buffer format (%d)\n",
		       __func__, ret);
		return ret;
	}

	return 0;
}

static int vidioc_try_fmt_sliced_vbi_out(struct file *file, void *priv,
					 struct v4l2_format *f)
{
	struct stmvbi_dev *dev = video_drvdata(file);
	int ret;

	ret = stmvbi_set_buffer_format(dev, f, 0);
	if (ret) {
		printk(KERN_ERR "%s: failed to try buffer format (%d)\n",
		       __func__, ret);
		return ret;
	}

	return 0;
}

static int vidioc_reqbufs(struct file *file, void *priv,
			  struct v4l2_requestbuffers *p)
{
	if (p->memory != V4L2_MEMORY_USERPTR) {
		printk(KERN_ERR "%s: unsupported memory type\n", __func__);
		return -EINVAL;
	}

	return 0;
}

static int vidioc_querybuf(struct file *file, void *priv, struct v4l2_buffer *p)
{
	/* TODO */
	return -EINVAL;
}

static int vidioc_qbuf(struct file *file, void *priv, struct v4l2_buffer *pBuf)
{
	struct stmvbi_dev *dev = video_drvdata(file);
	int ret = 0;

	if (pBuf->memory != V4L2_MEMORY_USERPTR) {
		err_msg
			("%s: VIDIOC_QBUF: illegal memory type %d\n",
			 __func__, pBuf->memory);
		ret = -EINVAL;
		goto err_ret;
	}

	if (dev->bufferFormat.type != V4L2_BUF_TYPE_SLICED_VBI_OUTPUT) {
		err_msg
			("%s: VIDIOC_QBUF: no buffer format has been set\n",
			 __func__);
		ret = -EINVAL;
		goto err_ret;
	}

	debug_msg("%s: VIDIOC_QBUF: Queuing new buffer\n", __func__);

	ret = stmvbi_queue_buffer(dev, pBuf);
	if (ret < 0) {
		err_msg
			("%s: VIDIOC_QBUF: failed to queue buffer (%d)\n",
			 __func__, ret);
		goto err_ret;
	}

err_ret:
	return ret;
}

static int vidioc_dqbuf(struct file *file, void *priv, struct v4l2_buffer *pBuf)
{
	struct stmvbi_dev *dev = video_drvdata(file);
	int ret = 0;

	/*
	 * The API documentation states that the memory field must be
	 * set correctly by the application for the call to succeed.
	 * It isn't clear under what circumstances this would ever be
	 * needed and the requirement seems a bit over zealous,
	 * but we honour it anyway. I bet users will miss the small print
	 * and log lots of bugs that this call doesn't work.
	 */
	if (pBuf->memory != V4L2_MEMORY_USERPTR) {
		err_msg
		    ("%s: VIDIOC_DQBUF: illegal memory type %d\n",
		     __func__, pBuf->memory);
		ret = -EINVAL;
		goto err_ret;
	}

	/*
	 * We can release the driver lock now as stmvbi_dequeue_buffer is safe
	 * (the buffer queues have their own access locks), this makes
	 * the logic a bit simpler particularly in the blocking case.
	 */
	up(&dev->devLock);

	if ((file->f_flags & O_NONBLOCK) == O_NONBLOCK) {
		debug_msg("%s: VIDIOC_DQBUF: Non-Blocking dequeue\n", __func__);
		if (!stmvbi_dequeue_buffer(dev, pBuf))
			return -EAGAIN;

		return 0;
	} else {
		debug_msg("%s: VIDIOC_DQBUF: Blocking dequeue\n", __func__);
		return wait_event_interruptible(dev->wqBufDQueue,
						stmvbi_dequeue_buffer
						(dev, pBuf));
	}

err_ret:
	return ret;
}

static int vidioc_streamon(struct file *file, void *priv, enum v4l2_buf_type i)
{
	struct stmvbi_dev *dev = video_drvdata(file);
	int ret = 0;

	if (dev->isStreaming) {
		err_msg
			("%s: VIDIOC_STREAMON: device already streaming\n",
			 __func__);
		ret = -EBUSY;
		goto err_ret;
	}

	if (!stmvbi_has_queued_buffers(dev)) {
		/* The API states that at least one buffer must be queued before
		   STREAMON succeeds. */
		err_msg
			("%s: VIDIOC_STREAMON: no buffers queued on stream\n",
			 __func__);
		ret = -EINVAL;
		goto err_ret;
	}

	debug_msg("%s: VIDIOC_STREAMON: Starting the stream\n", __func__);

	dev->isStreaming = 1;

	if(dev->owner == NULL)
		dev->owner = file;

	wake_up_interruptible(&(dev->wqStreamingState));

	/* Make sure any frames already on the pending queue are written to the
	   hardware */
	stmvbi_send_next_buffer_to_display(dev);

err_ret:
	return ret;
}

static int vidioc_streamoff(struct file *file, void *priv, enum v4l2_buf_type i)
{
  int ret = 0;

	struct stmvbi_dev *dev = video_drvdata(file);

  ret = stmvbi_streamoff(dev);

  if(!ret) {
    if(dev->owner == file)
      dev->owner = NULL;
  }

  return ret;
}

static int vidioc_g_sliced_vbi_cap(struct file *file, void *priv,
				   struct v4l2_sliced_vbi_cap *cap)
{
	struct stmvbi_dev *dev = video_drvdata(file);
	int i;

	if (cap->type != V4L2_BUF_TYPE_SLICED_VBI_OUTPUT)
		return -EINVAL;

	memset(cap, 0, sizeof(struct v4l2_sliced_vbi_cap));

	if (dev->output_caps & OUTPUT_CAPS_HW_TELETEXT) {
		cap->service_set |= V4L2_SLICED_TELETEXT_B;
		for (i = 6; i < 23; i++) {
			cap->service_lines[0][i] |= V4L2_SLICED_TELETEXT_B;
			cap->service_lines[1][i] |= V4L2_SLICED_TELETEXT_B;
		}
	}

	if (dev->output_caps & OUTPUT_CAPS_HW_CC) {
		cap->service_set |= V4L2_SLICED_CAPTION_525;
		for (i = 10; i < 23; i++) {
			cap->service_lines[0][i] |= V4L2_SLICED_CAPTION_525;
			cap->service_lines[1][i] |= V4L2_SLICED_CAPTION_525;
		}
	}
	return 0;
}

#if 0
/************************************************
 * v4l2_poll
 ************************************************/
static unsigned int
stmvbi_poll(struct stm_v4l2_handles *handle,
	    enum _stm_v4l2_driver_type type,
	    struct file *file, poll_table * table)
{
	struct stmvbi_dev *pDev;
	unsigned int mask;

	pDev = &g_vbiData[handle->device];

	if (down_interruptible(&pDev->devLock))
		return -ERESTARTSYS;

	/* Add DQueue wait queue to the poll wait state */
	poll_wait(file, &pDev->wqBufDQueue, table);
	poll_wait(file, &pDev->wqStreamingState, table);

	if (!pDev->isStreaming)
		mask = POLLOUT;	/* Write is available when not streaming */
	else if (stmvbi_has_completed_buffers(pDev))
		mask = POLLIN;	/* Indicate we can do a DQUEUE ioctl */
	else
		mask = 0;

	up(&pDev->devLock);

	return mask;
}
#endif

static int stmvbi_configure_device(struct stmvbi_dev *pDev)
{
	stm_display_device_h hDevice;
	int ret;

	debug_msg("%s in. pDev = 0x%08X\n", __func__, (unsigned int)pDev);

	memset(pDev, 0, sizeof(struct stmvbi_dev));

	init_waitqueue_head(&(pDev->wqBufDQueue));
	init_waitqueue_head(&(pDev->wqStreamingState));
	sema_init(&(pDev->devLock), 1);

	/* Open the display device */
	ret = stm_display_open_device(0, &hDevice);
	if (ret) {
		printk(KERN_ERR "%s: failed to open display device (%d)\n",
				__func__, ret);
		return -ENODEV;
	}
	pDev->hDevice = hDevice;

	/* Scan all outputs - looking for one with VBI */
	ret = stmvbi_find_output_with_capability(pDev, (OUTPUT_CAPS_HW_CC|OUTPUT_CAPS_HW_TELETEXT));
	if (ret) {
		printk(KERN_ERR "%s: failed to find an output with appropriate capability (%d)\n",
				__func__, ret);
		return ret;
	}

	/* Flush queues and disable HW */
	if (pDev->output_caps & OUTPUT_CAPS_HW_TELETEXT) {
		debug_msg("%s: Flush TTX metadata\n", __func__);
		stm_display_output_flush_metadata(pDev->hOutput[pDev->output_id],
				STM_METADATA_TYPE_TELETEXT);
	}

	if (pDev->output_caps & OUTPUT_CAPS_HW_CC) {
		debug_msg("%s: Flush CC metadata\n", __func__);
		stm_display_output_flush_metadata(pDev->hOutput[pDev->output_id],
				STM_METADATA_TYPE_CLOSED_CAPTION);
		debug_msg("%s: Disable CC HW\n", __func__);
		stm_display_output_set_control(pDev->hOutput[pDev->output_id], OUTPUT_CTRL_CC_INSERTION_ENABLE, 0);
	}

	pDev->owner = NULL;

	rwlock_init(&pDev->pendingStreamQ.lock);
	INIT_LIST_HEAD(&pDev->pendingStreamQ.list);

	rwlock_init(&pDev->completeStreamQ.lock);
	INIT_LIST_HEAD(&pDev->completeStreamQ.list);

	debug_msg("%s out.\n", __func__);

	return 0;
}

static void stmvbi_release_device(struct stmvbi_dev *pDev)
{
	int output;

	for (output = 0; output < pDev->max_output_nb; output++) {
		if (!pDev->hOutput[output])
			continue;
		stm_display_output_close(pDev->hOutput[output]);
	}

	stm_display_device_close(pDev->hDevice);
}

static struct v4l2_file_operations stmvbi_dev_fops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = video_ioctl2,
	.open = v4l2_fh_open,
	.release = stmvbi_close,
#if 0
	.poll = stmvbi_poll
#endif
};

static struct v4l2_ioctl_ops stmvbi_ioctl_ops = {
	.vidioc_enum_output = vidioc_enum_output,
	.vidioc_s_output = vidioc_s_output,
	.vidioc_g_output = vidioc_g_output,
	.vidioc_querycap = vidioc_querycap,
	.vidioc_g_fmt_sliced_vbi_out = vidioc_g_fmt_sliced_vbi_out,
	.vidioc_s_fmt_sliced_vbi_out = vidioc_s_fmt_sliced_vbi_out,
	.vidioc_try_fmt_sliced_vbi_out = vidioc_try_fmt_sliced_vbi_out,
	.vidioc_reqbufs = vidioc_reqbufs,
	.vidioc_querybuf = vidioc_querybuf,
	.vidioc_qbuf = vidioc_qbuf,
	.vidioc_dqbuf = vidioc_dqbuf,
	.vidioc_streamon = vidioc_streamon,
	.vidioc_streamoff = vidioc_streamoff,
	.vidioc_g_sliced_vbi_cap = vidioc_g_sliced_vbi_cap,
};

static void stmvbi_dev_vdev_release(struct video_device *vdev)
{
	/* Nothing to do, but need by V4L2 stack */
}

static void __exit stmvbi_exit(void)
{
	debug_msg("%s\n", __PRETTY_FUNCTION__);

	stm_media_unregister_v4l2_video_device(&g_vbiData.viddev);

	media_entity_cleanup(&g_vbiData.viddev.entity);

	kfree(g_vbiData.pads);

	stmvbi_release_device(&g_vbiData);
}

static int __init stmvbi_init(void)
{
	struct video_device *viddev = &g_vbiData.viddev;
	struct media_pad *pads;
	int ret;

	debug_msg("%s\n", __PRETTY_FUNCTION__);

	/* Search for the Analog VBI output */
	ret = stmvbi_configure_device(&g_vbiData);
	if (ret) {
		printk(KERN_ERR
		       "%s: failed to properly find and configure output (%d)\n",
		       __func__, ret);
		ret = -ENODEV;
		goto failed_init_device;
	}

	/*
	 * Initialize the media entity
	 */
	pads = (struct media_pad *)kzalloc(sizeof(*pads), GFP_KERNEL);
	if (!pads) {
		printk(KERN_ERR "Failed to allocate memory for vbi pads\n");
		ret = -ENOMEM;
		goto failed_alloc_pad;
	}
	g_vbiData.pads = pads;

	ret = media_entity_init(&viddev->entity, 1, pads, 0);
	if (ret) {
		printk(KERN_ERR "Failed to init the stmvbi media entity\n");
		goto failed_entity_init;
	}

	/* Initialize the Analog VBI output device */
	strlcpy(viddev->name, "STM Analog VBI", sizeof(viddev->name));
	viddev->fops = &stmvbi_dev_fops;
	viddev->ioctl_ops = &stmvbi_ioctl_ops;
	viddev->release = stmvbi_dev_vdev_release;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 7, 0))
        viddev->vfl_dir = VFL_DIR_TX;
#endif
	video_set_drvdata(viddev, &g_vbiData);

	/* Register the Analog VBI output device */
	ret = stm_media_register_v4l2_video_device(viddev, VFL_TYPE_VBI, -1);
	if (ret < 0) {
		printk(KERN_ERR
		       "%s: failed to register the analog output vbi driver (%d)\n",
		       __func__, ret);
		ret = -EIO;
		goto failed_register_device;
	}

	return 0;

failed_register_device:
	media_entity_cleanup(&viddev->entity);
failed_entity_init:
	kfree(pads);
failed_alloc_pad:
	stmvbi_release_device(&g_vbiData);
failed_init_device:
	return ret;
}

module_init(stmvbi_init);
module_exit(stmvbi_exit);

MODULE_LICENSE("GPL");
