
/*
 * STMicroelectronics FDMA dmaengine driver audio extensions
 *
 * Copyright (c) 2012 STMicroelectronics Limited
 *
 * Author: John Boddie <john.boddie@st.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/slab.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/dma-mapping.h>

#include <linux/platform_data/dma-st-fdma.h>
#include "st_fdma.h"

/*
 * Audio parking buffer defines and structures
 */

#define ST_FDMA_MIN_BUFFER_SIZE	4

struct st_fdma_audio {
	void *park_buffer;
	int park_buffer_size;
	dma_addr_t park_dma_addr;
	int park_sub_periods;
};


/*
 * Audio dmaengine extension helper functions
 */

static int st_fdma_audio_alloc_parking_buffer(struct st_fdma_chan *fchan,
		int buffer_size)
{
	struct st_fdma_audio *audio = fchan->extension;

	dev_dbg(fchan->fdev->dev, "%s(fchan=%p, buffer_size=%d)\n", __func__,
			fchan, buffer_size);

	/* Allocate the parking buffer */
	audio->park_buffer = kzalloc(buffer_size, GFP_KERNEL);
	if (!audio->park_buffer) {
		dev_err(fchan->fdev->dev, "Failed to alloc parking buffer\n");
		return -ENOMEM;
	}

	/* Save the new buffer size */
	audio->park_buffer_size = buffer_size;

	/* Map the buffer (DMA_BIDIRECTIONAL forces writeback/invalidate) */
	audio->park_dma_addr = dma_map_single(NULL, audio->park_buffer,
			audio->park_buffer_size, DMA_BIDIRECTIONAL);

	return 0;
}

static void st_fdma_audio_free_parking_buffer(struct st_fdma_chan *fchan)
{
	struct st_fdma_audio *audio = fchan->extension;

	dev_dbg(fchan->fdev->dev, "%s(fchan=%p)\n", __func__, fchan);

	/* Unmap any parking buffer */
	if (audio->park_dma_addr)
		dma_unmap_single(NULL, audio->park_dma_addr,
				audio->park_buffer_size, DMA_BIDIRECTIONAL);

	/* Free any parking buffer (kfree is tolerant of null arg) */
	kfree(audio->park_buffer);

	/* Reset parking data */
	audio->park_dma_addr = 0;
	audio->park_buffer = NULL;
	audio->park_buffer_size = 0;
}

static int st_fdma_audio_config(struct st_fdma_chan *fchan,
		struct st_dma_park_config *config)
{
	struct st_fdma_audio *audio = fchan->extension;

	dev_dbg(fchan->fdev->dev, "%s(fchan=%p, config=%p)\n", __func__, fchan,
			config);

	/* Check for a valid number of sub-periods */
	if (config->sub_periods < 0) {
		dev_err(fchan->fdev->dev, "Invalid parking sub-periods\n");
		return -EINVAL;
	}

	/* Check for a valid buffer size */
	if (config->buffer_size < ST_FDMA_MIN_BUFFER_SIZE) {
		dev_err(fchan->fdev->dev, "Parking buffer too small (min=8)\n");
		return -EINVAL;
	}

	/* Free existing parking buffer if not large enough */
	if (config->buffer_size > audio->park_buffer_size)
		st_fdma_audio_free_parking_buffer(fchan);

	/* Check if parking buffer needs to be (re)allocated */
	if (!audio->park_buffer)
		st_fdma_audio_alloc_parking_buffer(fchan, config->buffer_size);

	/* Save the sub periods */
	audio->park_sub_periods = config->sub_periods;

	/* Set next to the parking descriptor to make a loop */
	fchan->desc_park->llu->next = fchan->desc_park->dma_desc.phys;

	/* This is a DMA_MEM_TO_DEV transfer and requires a dreq */
	BUG_ON(!fchan->dreq);
	fchan->desc_park->llu->control = fchan->dreq->request_line;
	fchan->desc_park->llu->control |= NODE_CONTROL_SRC_INCR;
	fchan->desc_park->llu->control |= NODE_CONTROL_DST_STATIC;

	/* Buffer size may be larger than config size, so use config size */
	fchan->desc_park->llu->nbytes = config->buffer_size;

	/* Source is the parking buffer, destination is the dma_addr */
	fchan->desc_park->llu->saddr = audio->park_dma_addr;
	fchan->desc_park->llu->daddr = fchan->dma_addr;

	/* Transfer is only 1D */
	fchan->desc_park->llu->generic.length = config->buffer_size;
	fchan->desc_park->llu->generic.sstride = 0;
	fchan->desc_park->llu->generic.dstride = 0;

	return 0;
}


/*
 * Audio dmaengine extension resources functions
 */

int st_fdma_audio_alloc_chan_resources(struct st_fdma_chan *fchan)
{
	struct st_dma_audio_config *config = fchan->dma_chan.private;
	struct st_fdma_audio *audio;
	int result;

	dev_dbg(fchan->fdev->dev, "%s(fchan=%p)\n", __func__, fchan);

	BUG_ON(fchan->type != ST_DMA_TYPE_AUDIO);

	/* Allocate the dreq */
	fchan->dreq = st_fdma_dreq_alloc(fchan, &config->dreq_config);
	if (!fchan->dreq) {
		dev_err(fchan->fdev->dev, "Failed to allocate audio dreq\n");
		return -EBUSY;
	}

	/* Configure the dreq */
	result = st_fdma_dreq_config(fchan, fchan->dreq);
	if (result) {
		dev_err(fchan->fdev->dev, "Failed to set audio dreq\n");
		goto error_dreq_config;
	}

	/* Set the initial dma address */
	fchan->dma_addr = config->dma_addr;

	/* Allocate the audio channel structure */
	audio = kzalloc(sizeof(struct st_fdma_audio), GFP_KERNEL);
	if (!audio) {
		dev_err(fchan->fdev->dev, "Failed to alloc audio structure\n");
		result = -ENOMEM;
		goto error_kzalloc;
	}

	/* Save audio extensions structure in the channel */
	fchan->extension = audio;

	/* Allocate a descriptor to use for parking */
	fchan->desc_park = st_fdma_desc_alloc(fchan);
	if (!fchan->desc_park) {
		dev_err(fchan->fdev->dev, "Failed to alloc park descriptor\n");
		result = -ENOMEM;
		goto error_desc_alloc;
	}

	/* Configure the descriptor (allocate dma buffer etc) */
	result = st_fdma_audio_config(fchan, &config->park_config);
	if (result) {
		dev_err(fchan->fdev->dev, "Failed to config park descriptor\n");
		goto error_audio_config;
	}

	return 0;

error_audio_config:
	st_fdma_desc_free(fchan->desc_park);
	fchan->desc_park = NULL;
error_desc_alloc:
	kfree(fchan->extension);
	fchan->extension = NULL;
error_kzalloc:
error_dreq_config:
	st_fdma_dreq_free(fchan, fchan->dreq);
	return result;
}

void st_fdma_audio_free_chan_resources(struct st_fdma_chan *fchan)
{
	BUG_ON(fchan->type != ST_DMA_TYPE_AUDIO);

	dev_dbg(fchan->fdev->dev, "%s(fchan=%p)\n", __func__, fchan);

	BUG_ON(fchan->type != ST_DMA_TYPE_AUDIO);

	/* Free any parking buffer data */
	st_fdma_audio_free_parking_buffer(fchan);

	/* Free the descriptor used for parking */
	st_fdma_desc_free(fchan->desc_park);
	fchan->desc_park = NULL;

	/* Free the audio data (kfree is tolerant of null arg) */
	kfree(fchan->extension);
	fchan->extension = NULL;

	/* Free the dreq */
	st_fdma_dreq_free(fchan, fchan->dreq);
}


/*
 * Audio dmaengine extensions API
 */

int dma_audio_parking_config(struct dma_chan *chan,
		struct st_dma_park_config *config)
{
	struct st_fdma_chan *fchan = to_st_fdma_chan(chan);
	struct st_fdma_audio *audio = fchan->extension;
	unsigned long irqflags = 0;
	int result;

	dev_dbg(fchan->fdev->dev, "%s(fchan=%p, config=%p)\n", __func__, fchan,
			config);

	BUG_ON(fchan->type != ST_DMA_TYPE_AUDIO);
	BUG_ON(!audio);

	spin_lock_irqsave(&fchan->lock, irqflags);

	/* Do nothing if already parked */
	if (test_bit(ST_FDMA_IS_PARKED, &fchan->flags)) {
		dev_err(fchan->fdev->dev, "Cannot configure when parked!\n");
		result = -EBUSY;
		goto unlock;
	}

	/* Configure the descriptor (allocate dma buffer etc) */
	result = st_fdma_audio_config(fchan, config);

unlock:
	spin_unlock_irqrestore(&fchan->lock, irqflags);
	return result;
}
EXPORT_SYMBOL(dma_audio_parking_config);

int dma_audio_parking_enable(struct dma_chan *chan)
{
	struct st_fdma_chan *fchan = to_st_fdma_chan(chan);
	struct st_fdma_audio *audio = fchan->extension;
	struct st_fdma_desc *fdesc;
	unsigned long irqflags = 0;
	int result;

	dev_dbg(fchan->fdev->dev, "%s(fchan=%p)\n", __func__, fchan);

	BUG_ON(fchan->type != ST_DMA_TYPE_AUDIO);
	BUG_ON(!audio);

	spin_lock_irqsave(&fchan->lock, irqflags);

	/* Do nothing if already parked */
	if (test_bit(ST_FDMA_IS_PARKED, &fchan->flags)) {
		result = 0;
		goto unlock;
	}

	/* We can only park when running */
	if (fchan->state != ST_FDMA_STATE_RUNNING) {
		dev_err(fchan->fdev->dev, "Can only park a running channel!\n");
		result = -EINVAL;
		goto unlock;
	}

	/* Ensure parking node points to itself */
	fchan->desc_park->llu->next = fchan->desc_park->dma_desc.phys;

	/* We expect an active descriptor when entering parking mode */
	if (list_empty(&fchan->desc_active)) {
		dev_err(fchan->fdev->dev, "Can only park active descriptor!\n");
		result = -EINVAL;
		goto unlock;
	}

	/* Get the head of the active list */
	fdesc = list_first_entry(&fchan->desc_active,
			struct st_fdma_desc, node);

	/* Set the parking node to loop to itself */
	fchan->desc_park->llu->next = fchan->desc_park->dma_desc.phys;

	/* Park the channel (set ioc to complete active desc) */
	set_bit(ST_FDMA_IS_PARKED, &fchan->flags);
	st_fdma_hw_channel_switch(fchan, fdesc, fchan->desc_park, 1);

	/* Clear the cyclic bit else wont be able to start any new transfers */
	clear_bit(ST_FDMA_IS_CYCLIC, &fchan->flags);

	result = 0;

unlock:
	spin_unlock_irqrestore(&fchan->lock, irqflags);
	return result;
}
EXPORT_SYMBOL(dma_audio_parking_enable);

int dma_audio_is_parking_active(struct dma_chan *chan)
{
	struct st_fdma_chan *fchan = to_st_fdma_chan(chan);

	return test_bit(ST_FDMA_IS_PARKED, &fchan->flags);
}
EXPORT_SYMBOL(dma_audio_is_parking_active);

struct dma_async_tx_descriptor *dma_audio_prep_tx_cyclic(struct dma_chan *chan,
		dma_addr_t buf_addr, size_t buf_len, size_t period_len)
{
	struct st_fdma_chan *fchan = to_st_fdma_chan(chan);
	struct st_fdma_audio *audio = fchan->extension;
	struct st_fdma_desc *head = NULL;
	struct st_fdma_desc *prev = NULL;
	int result;
	int sub_periods;
	int sub_period_len;
	int p;

	dev_dbg(fchan->fdev->dev, "%s(chan=%p, b_addr=%pad, b_len=%zd, p_len=%zd)\n",
			__func__, chan, &buf_addr, buf_len, period_len);

	/* Only allow this function on audio channels */
	BUG_ON(fchan->type != ST_DMA_TYPE_AUDIO);
	BUG_ON(!audio);

	/* The slave must be configured! */
	if (!fchan->dma_addr) {
		dev_err(fchan->fdev->dev, "Slave not configured!\n");
		return NULL;
	}

	/* Only a single transfer allowed is channel is cyclic */
	result = test_and_set_bit(ST_FDMA_IS_CYCLIC, &fchan->flags);
	if (result) {
		dev_err(fchan->fdev->dev, "Channel %d already in use\n",
				fchan->id);
		return NULL;
	}

	/* Calculate the number of periods */
	sub_periods = (buf_len / period_len) * audio->park_sub_periods;
	sub_period_len = period_len / audio->park_sub_periods;

	/* Ensure we can evenly divide periods */
	if ((sub_period_len * audio->park_sub_periods) != period_len)
		dev_err(fchan->fdev->dev, "Period length issue!!!\n");

	/* Ensure we have dreq information */
	BUG_ON(!fchan->dreq);

	/* Build a cyclic linked list */
	for (p = 0; p < sub_periods; ++p) {
		/* Allocate a descriptor for this period */
		struct st_fdma_desc *fdesc = st_fdma_desc_get(fchan);
		if (!fdesc) {
			dev_err(fchan->fdev->dev, "Not enough decriptors\n");
			goto error_desc_get;
		}

		/* Configure the desciptor llu */
		fdesc->llu->next = 0;
		fdesc->llu->control = fchan->dreq->request_line;
		fdesc->llu->control |= NODE_CONTROL_SRC_INCR;
		fdesc->llu->control |= NODE_CONTROL_DST_STATIC;

		/* Only interrupt once per period not per sub-period */
		if ((p % audio->park_sub_periods) ==
				(audio->park_sub_periods - 1))
			fdesc->llu->control |= NODE_CONTROL_COMP_IRQ;

		fdesc->llu->nbytes = sub_period_len;

		fdesc->llu->saddr = buf_addr + (p * sub_period_len);
		fdesc->llu->daddr = fchan->dma_addr;

		fdesc->llu->generic.length = sub_period_len;
		fdesc->llu->generic.sstride = 0;
		fdesc->llu->generic.dstride = 0;

		/* Add the descriptor to the chain */
		st_fdma_desc_chain(&head, &prev, fdesc);
	}

	/* Ensure last llu points to first llu */
	prev->llu->next = head->dma_desc.phys;

	/* Set first descriptor of chain cookie to error */
	head->dma_desc.cookie = -EBUSY;

	return &head->dma_desc;

error_desc_get:
	st_fdma_desc_put(head);
	clear_bit(ST_FDMA_IS_CYCLIC, &fchan->flags);
	return NULL;
}
EXPORT_SYMBOL(dma_audio_prep_tx_cyclic);
