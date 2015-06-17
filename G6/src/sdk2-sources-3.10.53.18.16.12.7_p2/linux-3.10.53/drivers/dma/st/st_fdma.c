/*
 * STMicroelectronics FDMA dmaengine driver
 *
 * Copyright (c) 2012 STMicroelectronics Limited
 *
 * Author: John Boddie <john.boddie@st.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/device.h>
#include <linux/dmapool.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/io.h>

#include <linux/platform_data/dma-st-fdma.h>
#include <linux/clk.h>

#include "st_fdma.h"

static char *fdma_clk_name[] = {
	[CLK_SLIM] = "fdma_slim",
	[CLK_HI] = "fdma_hi",
	[CLK_LOW] = "fdma_low",
	[CLK_IC] = "fdma_ic",
};

/*
 * Interrupt functions
 */
static void st_fdma_irq_complete(struct st_fdma_chan *fchan)
{
	unsigned long irqflags = 0;
	struct device *dev = fchan->fdev->dev;

	spin_lock_irqsave(&fchan->lock, irqflags);

	/* Update the channel state */
	switch (st_fdma_hw_channel_status(fchan)) {
	case CMD_STAT_STATUS_PAUSED:
		switch (fchan->state) {
		case ST_FDMA_STATE_ERROR:
		case ST_FDMA_STATE_RUNNING:	/* Hit a pause node */
			fchan->state = ST_FDMA_STATE_PAUSED;
			break;

		case ST_FDMA_STATE_STOPPING:
			st_fdma_hw_channel_reset(fchan);
			fchan->state = ST_FDMA_STATE_IDLE;
			break;

		default:
			goto err_inv;
		}
		break;

	case CMD_STAT_STATUS_IDLE:
		switch (fchan->state) {
		case ST_FDMA_STATE_IDLE:	/* Ignore IDLE -> IDLE */
		case ST_FDMA_STATE_RUNNING:
		case ST_FDMA_STATE_STOPPING:
			fchan->state = ST_FDMA_STATE_IDLE;
			break;

		default:
			goto err_inv;
		}
		break;

	case CMD_STAT_STATUS_RUNNING:
		/* Return if currently processing an error */
		if (fchan->state == ST_FDMA_STATE_ERROR)
			dev_err(dev, "Error %d on channel %d\n",
				st_fdma_hw_channel_error(fchan),
				fchan->id);
		break;

	default:
		goto err_inv;
	}

	/* Complete the descriptor */
	tasklet_hi_schedule(&fchan->tasklet_complete);
	spin_unlock_irqrestore(&fchan->lock, irqflags);
	return;

err_inv:
	spin_unlock_irqrestore(&fchan->lock, irqflags);
	dev_err(dev, "Invalid state transition\n");
	BUG();
}

static irqreturn_t st_fdma_irq_handler(int irq, void *dev_id)
{
	struct st_fdma_device *fdev = dev_id;
	irqreturn_t result = IRQ_NONE;
	unsigned long flags;
	u32 status;
	int c;

	/* Read and immediately clear the interrupt status */
	spin_lock_irqsave(&fdev->irq_lock, flags);
	status = readl(fdev->io_base + fdev->regs.int_sta);
	writel(status, fdev->io_base + fdev->regs.int_clr);
	spin_unlock_irqrestore(&fdev->irq_lock, flags);

	/* Process each channel raising an interrupt */
	for (c = ST_FDMA_MIN_CHANNEL; status != 0; status >>= 2, ++c) {
		/*
		 * On error both interrupts raised, so check for error first.
		 *
		 * When switching to the parking buffer we set each node of the
		 * currently active descriptor to interrupt on complete. This
		 * can result in a missed interrupt error. We suppress this
		 * error here and handle the interrupt as a normal completion.
		 */
		int state = ST_FDMA_STATE_RUNNING;
		struct st_fdma_chan *fchan = &fdev->ch_list[c];

		if (unlikely(status & 2))
			if (!test_bit(ST_FDMA_IS_PARKED, &fchan->flags))
				state = ST_FDMA_STATE_ERROR;

		if (status & 1 || state == ST_FDMA_STATE_ERROR) {
			fchan->state = state;
			st_fdma_irq_complete(fchan);
			result = IRQ_HANDLED;
		}
	}

	return result;
}

/*
 * Clock functions.
 */
static int st_fdma_clk_get(struct st_fdma_device *fdev)
{
	int i;

	for (i = 0; i < CLK_MAX_NUM; i++) {
		fdev->clks[i] = devm_clk_get(fdev->dev, fdma_clk_name[i]);
		if (IS_ERR(fdev->clks[i])) {
			dev_err(fdev->dev,
				"failed to get clock: %s\n", fdma_clk_name[i]);
			return PTR_ERR(fdev->clks[i]);
		}
	}

	return 0;
}

static int st_fdma_clk_enable(struct st_fdma_device *fdev)
{
	int i, ret;

	for (i = 0; i < CLK_MAX_NUM; i++) {
		if (fdev->clks[i]) {
			ret = clk_prepare_enable(fdev->clks[i]);
			if (ret < 0)
				return ret;
		}
	}

	return 0;
}

static void st_fdma_clk_disable(struct st_fdma_device *fdev)
{
	int i;

	for (i = 0; i < CLK_MAX_NUM; ++i) {
		if (fdev->clks[i])
			clk_disable_unprepare(fdev->clks[i]);
	}
}

/*
 * dmaengine callback functions
 */
dma_cookie_t st_fdma_tx_submit(struct dma_async_tx_descriptor *desc)
{
	struct st_fdma_desc *fdesc = to_st_fdma_desc(desc);
	struct st_fdma_chan *fchan = fdesc->fchan;
	unsigned long irqflags = 0;

	dev_info(fchan->fdev->dev, "%s(desc=%p) ptr_callback:%p\n", __func__, desc, desc->callback);
	trace_printk("%s(desc=%p) ptr_callback:%p\n", __func__, desc, desc->callback);

	spin_lock_irqsave(&fchan->lock, irqflags);

	/* Assign descriptor next positive cookie */
	if (++fchan->dma_chan.cookie < 0)
		fchan->dma_chan.cookie = DMA_MIN_COOKIE;

	desc->cookie = fchan->dma_chan.cookie;

	/* Queue the descriptor */
	list_move_tail(&fdesc->node, &fchan->desc_queue);

	/* Attempt to start the next available descriptor */
	st_fdma_desc_start(fchan);

	spin_unlock_irqrestore(&fchan->lock, irqflags);

	return desc->cookie;
}

static int st_fdma_alloc_chan_resources(struct dma_chan *chan)
{
	struct st_fdma_chan *fchan = to_st_fdma_chan(chan);
	struct st_fdma_device *fdev = fchan->fdev;
	struct st_dma_paced_config *paced;
	unsigned long irqflags = 0;
	LIST_HEAD(list);
	int result;
	int i;

	dev_dbg(fdev->dev, "%s(chan=%p)\n", __func__, chan);

	/* Ensure firmware has loaded */
	if (st_fdma_fw_check(fchan->fdev)) {
		dev_err(fchan->fdev->dev, "Firmware not loaded!\n");
		return -ENODEV;
	}

	/* Set the channel type */
	if (chan->private)
		fchan->type = ((struct st_dma_config *) chan->private)->type;
	else
		fchan->type = ST_DMA_TYPE_FREE_RUNNING;

	/* Perform and channel specific configuration */
	switch (fchan->type) {
	case ST_DMA_TYPE_FREE_RUNNING:
		fchan->dma_addr = 0;
		break;

	case ST_DMA_TYPE_PACED:
		paced = chan->private;
		fchan->dma_addr = paced->dma_addr;
		/* Allocate the dreq */
		fchan->dreq = st_fdma_dreq_alloc(fchan, &paced->dreq_config);
		if (!fchan->dreq) {
			dev_err(fdev->dev, "Failed to configure paced dreq\n");
			return -EINVAL;
		}
		/* Configure the dreq */
		result = st_fdma_dreq_config(fchan, fchan->dreq);
		if (result) {
			dev_err(fdev->dev, "Failed to set paced dreq\n");
			st_fdma_dreq_free(fchan, fchan->dreq);
			return result;
		}
		break;

	case ST_DMA_TYPE_AUDIO:
		result = st_fdma_audio_alloc_chan_resources(fchan);
		if (result) {
			dev_err(fdev->dev, "Failed to alloc audio resources\n");
			return result;
		}
		break;

	case ST_DMA_TYPE_TELSS:
		result = st_fdma_telss_alloc_chan_resources(fchan);
		if (result) {
			dev_err(fdev->dev, "Failed to alloc telss resources\n");
			return result;
		}
		break;

	case ST_DMA_TYPE_MCHI:
		result = st_fdma_mchi_alloc_chan_resources(fchan);
		if (result) {
			dev_err(fdev->dev, "Failed to alloc mchi resources\n");
			return result;
		}
		break;

	default:
		dev_err(fchan->fdev->dev, "Invalid channel type (%d)\n",
				fchan->type);
		return -EINVAL;
	}

	/* Allocate descriptors to a temporary list */
	for (i = 0; i < ST_FDMA_DESCRIPTORS; ++i) {
		struct st_fdma_desc *fdesc;

		fdesc = st_fdma_desc_alloc(fchan);
		if (!fdesc) {
			dev_err(fdev->dev, "Failed to allocate desc\n");
			break;
		}

		list_add_tail(&fdesc->node, &list);
	}

	spin_lock_irqsave(&fchan->lock, irqflags);
	fchan->desc_count = i;
	list_splice(&list, &fchan->desc_free);
	fchan->last_completed = chan->cookie = 1;
	spin_unlock_irqrestore(&fchan->lock, irqflags);

	return fchan->desc_count;
}

static void st_fdma_free_chan_resources(struct dma_chan *chan)
{
	struct st_fdma_chan *fchan = to_st_fdma_chan(chan);
	struct st_fdma_desc *fdesc, *_fdesc;
	unsigned long irqflags = 0;
	LIST_HEAD(list);

	dev_dbg(fchan->fdev->dev, "%s(chan=%p)\n", __func__, chan);

	/* Stop the completion tasklet (this waits until tasklet finishes) */
	tasklet_kill(&fchan->tasklet_complete);

	spin_lock_irqsave(&fchan->lock, irqflags);

	/*
	 * Channel must not be running and there must be no active or queued
	 * descriptors. We cannot check for being idle as on PM suspend we may
	 * never get the transition from stopping to idle!
	 */
	BUG_ON(fchan->state == ST_FDMA_STATE_RUNNING);
	BUG_ON(!list_empty(&fchan->desc_queue));
	BUG_ON(!list_empty(&fchan->desc_active));

	list_splice_init(&fchan->desc_free, &list);
	fchan->desc_count = 0;
	spin_unlock_irqrestore(&fchan->lock, irqflags);

	/* Free all allocated transfer descriptors */
	list_for_each_entry_safe(fdesc, _fdesc, &list, node) {
		st_fdma_desc_free(fdesc);
	}

	/* Perform any channel configuration clean up */
	switch (fchan->type) {
	case ST_DMA_TYPE_FREE_RUNNING:
		break;

	case ST_DMA_TYPE_PACED:
		st_fdma_dreq_free(fchan, fchan->dreq);
		break;

	case ST_DMA_TYPE_AUDIO:
		st_fdma_audio_free_chan_resources(fchan);
		break;

	case ST_DMA_TYPE_TELSS:
		st_fdma_telss_free_chan_resources(fchan);
		break;

	case ST_DMA_TYPE_MCHI:
		st_fdma_mchi_free_chan_resources(fchan);
		break;

	default:
		dev_err(fchan->fdev->dev, "Invalid channel type (%d)\n",
				fchan->type);
	}

	/* Clear the channel type */
	fchan->type = 0;
}

static struct dma_async_tx_descriptor *st_fdma_prep_dma_memcpy(
		struct dma_chan *chan, dma_addr_t dest, dma_addr_t src,
		size_t len, unsigned long flags)
{
	struct st_fdma_chan *fchan = to_st_fdma_chan(chan);
	struct st_fdma_desc *fdesc;
	int result;

	dev_dbg(fchan->fdev->dev,
			"%s(chan=%p, dest=%pad, src=%pad, len=%zd, flags=%#lx)\n",
			__func__, chan, &dest, &src, len, flags);

	/* Check parameters */
	if (len == 0) {
		dev_err(fchan->fdev->dev, "Invalid length\n");
		return NULL;
	}

	/* Only a single transfer allowed on cyclic channel */
	result = test_bit(ST_FDMA_IS_CYCLIC, &fchan->flags);
	if (result) {
		dev_err(fchan->fdev->dev, "Channel %d already in use for "
				"cyclic transfers!\n", fchan->id);
		return NULL;
	}

	/* Get a descriptor */
	fdesc = st_fdma_desc_get(fchan);
	if (!fdesc) {
		dev_err(fchan->fdev->dev, "Not enough decriptors!\n");
		return NULL;
	}

	/* We only require a single descriptor */
	fdesc->llu->next = 0;
	/* Configure as free running with incrementing src/dst  */
	fdesc->llu->control = NODE_CONTROL_REQ_MAP_FREE_RUN;
	fdesc->llu->control |= NODE_CONTROL_SRC_INCR;
	fdesc->llu->control |= NODE_CONTROL_DST_INCR;
	fdesc->llu->control |= NODE_CONTROL_COMP_IRQ;
	/* Set the number of bytes to transfer */
	fdesc->llu->nbytes = len;
	/* Set the src/dst addresses */
	fdesc->llu->saddr = src;
	fdesc->llu->daddr = dest;
	/* Configure for 1D data */
	fdesc->llu->generic.length = len;
	fdesc->llu->generic.sstride = 0;
	fdesc->llu->generic.dstride = 0;

	/* Set descriptor cookie to error and save flags */
	fdesc->dma_desc.cookie = -EBUSY;
	fdesc->dma_desc.flags = flags;

	return &fdesc->dma_desc;
}

static struct dma_async_tx_descriptor *st_fdma_prep_slave_sg(
		struct dma_chan *chan, struct scatterlist *sgl,
		unsigned int sg_len, enum dma_transfer_direction direction,
		unsigned long flags, void *context)
{
	struct st_fdma_chan *fchan = to_st_fdma_chan(chan);
	struct st_fdma_desc *head = NULL;
	struct st_fdma_desc *prev = NULL;
	struct scatterlist *sg;
	int result;
	int i;

	dev_dbg(fchan->fdev->dev, "%s(chan=%p, sgl=%p, sg_len=%d,"
			"direction=%d, flags=%08lx)\n", __func__, chan,
			sgl, sg_len, direction, flags);

	/* The slave must be configured! */
	if (!fchan->dma_addr) {
		dev_err(fchan->fdev->dev, "Slave not configured!\n");
		return NULL;
	}

	/* Only a single transfer allowed on cyclic channel */
	result = test_bit(ST_FDMA_IS_CYCLIC, &fchan->flags);
	if (result) {
		dev_err(fchan->fdev->dev, "Channel %d already in use for "
				"cyclic transfers!\n", fchan->id);
		return NULL;
	}

	/* Build a linked list */
	for_each_sg(sgl, sg, sg_len, i) {
		/* Allocate a descriptor for this node */
		struct st_fdma_desc *fdesc = st_fdma_desc_get(fchan);
		if (!fdesc) {
			dev_err(fchan->fdev->dev, "Not enough decriptors\n");
			goto error_desc_get;
		}

		/* Configure the desciptor llu */
		fdesc->llu->next = 0;

		switch (fchan->type) {
		case ST_DMA_TYPE_FREE_RUNNING:
			fdesc->llu->control = NODE_CONTROL_REQ_MAP_FREE_RUN;
			break;

		case ST_DMA_TYPE_PACED:
		case ST_DMA_TYPE_AUDIO:
			fdesc->llu->control = fchan->dreq->request_line;
			break;

		default:
			dev_err(fchan->fdev->dev, "Invalid channel type!\n");
			goto error_chan_type;
		}

		fdesc->llu->nbytes = sg_dma_len(sg);

		switch (direction) {
		case DMA_MEM_TO_MEM:
			fdesc->llu->control |= NODE_CONTROL_SRC_INCR;
			fdesc->llu->control |= NODE_CONTROL_DST_INCR;
			fdesc->llu->saddr = sg_dma_address(sg);
			fdesc->llu->daddr = fchan->dma_addr;
			break;

		case DMA_DEV_TO_MEM:
			fdesc->llu->control |= NODE_CONTROL_SRC_STATIC;
			fdesc->llu->control |= NODE_CONTROL_DST_INCR;
			fdesc->llu->saddr = fchan->dma_addr;
			fdesc->llu->daddr = sg_dma_address(sg);
			break;

		case DMA_MEM_TO_DEV:
			fdesc->llu->control |= NODE_CONTROL_SRC_INCR;
			fdesc->llu->control |= NODE_CONTROL_DST_STATIC;
			fdesc->llu->saddr = sg_dma_address(sg);
			fdesc->llu->daddr = fchan->dma_addr;
			break;

		default:
			dev_err(fchan->fdev->dev, "Invalid direction!\n");
			goto error_direction;
		}

		fdesc->llu->generic.length = sg_dma_len(sg);
		fdesc->llu->generic.sstride = 0;
		fdesc->llu->generic.dstride = 0;

		/* Add the descriptor to the chain */
		st_fdma_desc_chain(&head, &prev, fdesc);
	}

	/* Set the last descriptor to generate an interrupt on completion */
	prev->llu->control |= NODE_CONTROL_COMP_IRQ;

	/* Set first descriptor of chain cookie to error and save flags */
	head->dma_desc.cookie = -EBUSY;
	head->dma_desc.flags = flags;

	return &head->dma_desc;

error_direction:
error_chan_type:
error_desc_get:
	st_fdma_desc_put(head);
	return NULL;
}

static struct dma_async_tx_descriptor *st_fdma_prep_dma_cyclic(
		struct dma_chan *chan, dma_addr_t buf_addr, size_t buf_len,
		size_t period_len, enum dma_transfer_direction direction,
		unsigned long flags, void *context)
{
	struct st_fdma_chan *fchan = to_st_fdma_chan(chan);
	struct st_fdma_desc *head = NULL;
	struct st_fdma_desc *prev = NULL;
	int result;
	int p;

	dev_dbg(fchan->fdev->dev, "%s(chan=%p, b_addr=%pad, b_len=%zd, p_len=%zd, dir=%d)\n",
			__func__, chan, &buf_addr, buf_len,
			period_len, direction);

	/* The slave must be configured! */
	if (!fchan->dma_addr) {
		dev_err(fchan->fdev->dev, "Slave not configured!\n");
		return NULL;
	}

	/* Only a single transfer allowed on cyclic channel */
	result = test_and_set_bit(ST_FDMA_IS_CYCLIC, &fchan->flags);
	if (result) {
		dev_err(fchan->fdev->dev, "Channel %d already in use!\n",
				fchan->id);
		return NULL;
	}

	/* Build a cyclic linked list */
	for (p = 0; p < (buf_len / period_len); ++p) {
		/* Allocate a descriptor for this period */
		struct st_fdma_desc *fdesc = st_fdma_desc_get(fchan);
		if (!fdesc) {
			dev_err(fchan->fdev->dev, "Not enough decriptors\n");
			goto error_desc_get;
		}

		/* Configure the desciptor llu */
		fdesc->llu->next = 0;

		switch (fchan->type) {
		case ST_DMA_TYPE_FREE_RUNNING:
			fdesc->llu->control = NODE_CONTROL_REQ_MAP_FREE_RUN;
			break;

		case ST_DMA_TYPE_PACED:
		case ST_DMA_TYPE_AUDIO:
			fdesc->llu->control = fchan->dreq->request_line;
			break;

		default:
			dev_err(fchan->fdev->dev, "Invalid channel type!\n");
			goto error_chan_type;
		}

		fdesc->llu->control |= NODE_CONTROL_COMP_IRQ;

		fdesc->llu->nbytes = period_len;

		switch (direction) {
		case DMA_DEV_TO_MEM:
			fdesc->llu->control |= NODE_CONTROL_SRC_STATIC;
			fdesc->llu->control |= NODE_CONTROL_DST_INCR;
			fdesc->llu->saddr = fchan->dma_addr;
			fdesc->llu->daddr = buf_addr + (p * period_len);
			break;

		case DMA_MEM_TO_DEV:
			fdesc->llu->control |= NODE_CONTROL_SRC_INCR;
			fdesc->llu->control |= NODE_CONTROL_DST_STATIC;
			fdesc->llu->saddr = buf_addr + (p * period_len);
			fdesc->llu->daddr = fchan->dma_addr;
			break;

		default:
			dev_err(fchan->fdev->dev, "Invalid direction!\n");
			goto error_direction;
		}

		fdesc->llu->generic.length = period_len; /* 1D: node bytes */
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

error_direction:
error_chan_type:
error_desc_get:
	st_fdma_desc_put(head);
	clear_bit(ST_FDMA_IS_CYCLIC, &fchan->flags);
	return NULL;
}

static int st_fdma_pause(struct st_fdma_chan *fchan)
{
	unsigned long irqflags = 0;
	int result = 0;

	spin_lock_irqsave(&fchan->lock, irqflags);

	switch (fchan->state) {
	case ST_FDMA_STATE_IDLE:
		/* Hardware isn't set up yet, so treat this as an error */
		result = -EBUSY;
		break;

	case ST_FDMA_STATE_PAUSED:
		/* Hardware is already paused */
		break;

	case ST_FDMA_STATE_ERROR:
	case ST_FDMA_STATE_RUNNING:
		/* Hardware is running, send the command */
		st_fdma_hw_channel_pause(fchan, 0);
		/* Fall through */
		/* Hardware is pausing already, wait for interrupt */
		break;

	case ST_FDMA_STATE_STOPPING:
		/* Hardware is stopping, so treat this as an error */
		result = -EBUSY;
		break;
	}

	spin_unlock_irqrestore(&fchan->lock, irqflags);

	return result;
}

static int st_fdma_resume(struct st_fdma_chan *fchan)
{
	unsigned long irqflags = 0;

	spin_lock_irqsave(&fchan->lock, irqflags);

	if (fchan->state != ST_FDMA_STATE_PAUSED) {
		spin_unlock_irqrestore(&fchan->lock, irqflags);
		return -EBUSY;
	}

	st_fdma_hw_channel_resume(fchan);

	fchan->state = ST_FDMA_STATE_RUNNING;

	spin_unlock_irqrestore(&fchan->lock, irqflags);

	return 0;
}

static void st_fdma_stop(struct st_fdma_chan *fchan)
{
	unsigned long irqflags = 0;

	dev_dbg(fchan->fdev->dev, "%s(fchan=%p)\n", __func__, fchan);

	spin_lock_irqsave(&fchan->lock, irqflags);

	switch (fchan->state) {
	case ST_FDMA_STATE_IDLE:
	case ST_FDMA_STATE_PAUSED:
		/* Channel is idle - just change state and reset channel */
		fchan->state = ST_FDMA_STATE_IDLE;
		st_fdma_hw_channel_reset(fchan);
		break;

	case ST_FDMA_STATE_ERROR:
	case ST_FDMA_STATE_RUNNING:
		/* Channel is running - issue stop (pause) command */
		st_fdma_hw_channel_pause(fchan, 0);
		/* Fall through */

	case ST_FDMA_STATE_STOPPING:
		/* Channel is pausing - just change state */
		fchan->state = ST_FDMA_STATE_STOPPING;
		break;
	}

	spin_unlock_irqrestore(&fchan->lock, irqflags);
}

static int st_fdma_terminate_all(struct st_fdma_chan *fchan)
{
	struct st_fdma_desc *fdesc, *_fdesc;
	unsigned long irqflags = 0;
	LIST_HEAD(list);

	dev_dbg(fchan->fdev->dev, "%s(fchan=%p)\n", __func__, fchan);

	/* Stop the channel */
	st_fdma_stop(fchan);

	spin_lock_irqsave(&fchan->lock, irqflags);

	/* Move active and queued descriptors to a temporary list */
	list_splice_init(&fchan->desc_queue, &list);
	list_splice_init(&fchan->desc_active, &list);

	/* Channel is no longer cyclic/parked after a terminate all! */
	clear_bit(ST_FDMA_IS_CYCLIC, &fchan->flags);
	clear_bit(ST_FDMA_IS_PARKED, &fchan->flags);

	spin_unlock_irqrestore(&fchan->lock, irqflags);

	list_for_each_entry_safe(fdesc, _fdesc, &list, node) {
		/* Unmap buffers for non-slave channels (e.g. memcpy) */
		if (!fchan->dma_chan.private)
			st_fdma_desc_unmap_buffers(fdesc);

		/* Move from temporary list to free list (no callbacks!) */
		st_fdma_desc_put(fdesc);
	}

	return 0;
}

static int st_fdma_slave_config(struct st_fdma_chan *fchan,
		struct dma_slave_config *config)
{
	struct st_fdma_device *fdev = fchan->fdev;
	unsigned long irqflags = 0;
	dma_addr_t dma_addr = 0;

	dev_dbg(fdev->dev, "%s(fchan=%p, config=%p)\n", __func__,
			fchan, config);

	/* Save the supplied dma address */
	switch (config->direction) {
	case DMA_MEM_TO_MEM:
		dma_addr = config->dst_addr;
		break;

	case DMA_DEV_TO_MEM:
		dma_addr = config->src_addr;
		break;

	case DMA_MEM_TO_DEV:
		dma_addr = config->dst_addr;
		break;

	default:
		dev_err(fdev->dev, "Invalid slave config direction!\n");
		return -EINVAL;
	}

	spin_lock_irqsave(&fchan->lock, irqflags);
	fchan->dma_addr = dma_addr;
	spin_unlock_irqrestore(&fchan->lock, irqflags);

	spin_lock(&fdev->dreq_lock);

	/* Only update dreq configuration if we are using a dreq */
	if (fchan->dreq) {
		/* Update the max burst, bus width and direction */
		switch (config->direction) {
		case DMA_DEV_TO_MEM:
			fchan->dreq->maxburst = config->src_maxburst;
			fchan->dreq->buswidth = config->src_addr_width;
			fchan->dreq->direction = DMA_DEV_TO_MEM;
			break;

		case DMA_MEM_TO_MEM:
		case DMA_MEM_TO_DEV:
			fchan->dreq->maxburst = config->dst_maxburst;
			fchan->dreq->buswidth = config->dst_addr_width;
			fchan->dreq->direction = DMA_MEM_TO_DEV;
			break;

		default:
			dev_err(fdev->dev, "Invalid slave config direction!\n");
			spin_unlock(&fdev->dreq_lock);
			return -EINVAL;
		}

		spin_unlock(&fdev->dreq_lock);

		/* Configure the dreq and return */
		return st_fdma_dreq_config(fchan, fchan->dreq);
	}

	spin_unlock(&fdev->dreq_lock);

	return 0;
}

static int st_fdma_control(struct dma_chan *chan, enum dma_ctrl_cmd cmd,
		unsigned long arg)
{
	struct st_fdma_chan *fchan = to_st_fdma_chan(chan);
	struct dma_slave_config *config;

	dev_dbg(fchan->fdev->dev, "%s(chan_id=%d chan=%p, cmd=%d, arg=%lu)\n",
		__func__, fchan->id, chan, cmd, arg);

	switch (cmd) {
	case DMA_PAUSE:
		return st_fdma_pause(fchan);

	case DMA_RESUME:
		return st_fdma_resume(fchan);
		break;

	case DMA_TERMINATE_ALL:
		return st_fdma_terminate_all(fchan);

	case DMA_SLAVE_CONFIG:
		config = (struct dma_slave_config *) arg;
		return st_fdma_slave_config(fchan, config);

	default:
		dev_err(fchan->fdev->dev, "Invalid control cmd (%d)\n", cmd);
	}

	return -ENOSYS;
}

/*
 * Assume we only get called with the channel lock held! As we should only be
 * called from st_fdma_tx_status this should not be an issue.
 */
static u32 st_fdma_get_residue(struct st_fdma_chan *fchan)
{
	u32 count = 0;

	dev_dbg(fchan->fdev->dev, "%s(fchan=%p)\n", __func__, fchan);

	/* If channel is parked, return a notional residue */
	if (test_bit(ST_FDMA_IS_PARKED, &fchan->flags)) {
		BUG_ON(!fchan->desc_park);
		return fchan->desc_park->llu->nbytes;
	}

	/* Only attempt to get residue on a non-idle channel */
	if (fchan->state != ST_FDMA_STATE_IDLE) {
		unsigned long stat1, stat2;
		struct st_fdma_desc *fdesc, *child;
		unsigned long phys_node;
		int found_node = 0;

		/*
		 * Get the current node bytes remaining. We loop until status
		 * is identical in case we are moving on to the next node.
		 */

		do {
			stat1 = readl(CMD_STAT_REG(fchan));
			count = readl(NODE_COUNT_REG(fchan));
			stat2 = readl(CMD_STAT_REG(fchan));
		} while (stat1 != stat2);

		switch (stat1 & CMD_STAT_STATUS_MASK) {
		case CMD_STAT_STATUS_IDLE:
			/*
			 * Channel stopped but not yet taken the interrupt to
			 * change the channel state. Pretend still data to
			 * process and let interrupt do tidy up.
			 */
			return 1;

		case CMD_STAT_STATUS_RUNNING:
		case CMD_STAT_STATUS_PAUSED:
			/*
			 * FDMA firmware modifies CMD_STAT before it updates
			 * the count. However as we write the count when
			 * starting the channel we assume it is valid.
			 */
			break;

		case CMD_STAT_CMD_START:
			/*
			 * Channel has not yet started running so count not yet
			 * loaded from the node. However as we write the count
			 * when starting the channel we assume it is valid.
			 */
			break;
		}

		/*
		 * The descriptor residue is calculated as the sum of the
		 * remaining bytes for the current node and each node left to
		 * process. Thus we need to find the current descriptor node
		 * using the physical node address and only add the number of
		 * bytes from the following nodes to the residue count.
		 */

		/* Convert the status to the physical node pointer address */
		phys_node = stat1 & CMD_STAT_DATA_MASK;

		/* Get the active descriptor */
		fdesc = list_first_entry_or_null(&fchan->desc_active,
				struct st_fdma_desc, node);
		if (!fdesc)
			goto out;

		/* Does the physical node match the first descriptor node? */
		if (phys_node == fdesc->dma_desc.phys)
			found_node = 1;

		/* Loop through all descriptor child nodes */
		list_for_each_entry(child, &fdesc->llu_list, node) {
			/* If node has been found, add node nbytes to count */
			if (found_node) {
				count += child->llu->nbytes;
				continue;
			}

			/* Does physical node match child? */
			if (phys_node == child->dma_desc.phys)
				found_node = 1;
		}

		/* Ensure the current node is from the active descriptor */
		BUG_ON(found_node == 0);
	}
out:
	return count;
}

static enum dma_status st_fdma_tx_status(struct dma_chan *chan,
		dma_cookie_t cookie, struct dma_tx_state *txstate)
{
	struct st_fdma_chan *fchan = to_st_fdma_chan(chan);
	unsigned long irqflags = 0;
	dma_cookie_t last_used;
	dma_cookie_t last_complete;
	enum dma_status status;
	u32 residue = 0;

	dev_dbg(fchan->fdev->dev, "%s(chan=%p, cookie=%08x, txstate=%p)\n",
			__func__, chan, cookie, txstate);

	spin_lock_irqsave(&fchan->lock, irqflags);

	/* Set the last cookie value returned to the client */
	last_used = chan->cookie;
	last_complete = fchan->last_completed;

	/* Check channel status */
	switch (fchan->state) {
	case ST_FDMA_STATE_PAUSED:
		status = DMA_PAUSED;
		break;

	case ST_FDMA_STATE_ERROR:
		status = DMA_ERROR;
		break;

	default:
		status = dma_async_is_complete(cookie, last_complete,
				last_used);
	}

	/* Get the transfer residue based on transfer status */
	switch (status) {
	case DMA_SUCCESS:
		residue = 0;
		break;

	case DMA_ERROR:
		residue = 0;
		break;

	default:
		residue = st_fdma_get_residue(fchan);
	}

	spin_unlock_irqrestore(&fchan->lock, irqflags);

	/* Set the state */
	dma_set_tx_state(txstate, last_complete, last_used, residue);

	return status;
}

static void st_fdma_issue_pending(struct dma_chan *chan)
{
	struct st_fdma_chan *fchan = to_st_fdma_chan(chan);
	unsigned long irqflags = 0;

	dev_info(fchan->fdev->dev, "%s(chan=%p)num chan:%d\n", __func__, chan, chan->chan_id);
	trace_printk("%s(chan=%p)num chan:%d\n", __func__, chan, chan->chan_id);

	/* Try starting any next available descriptor */
	spin_lock_irqsave(&fchan->lock, irqflags);
	st_fdma_desc_start(fchan);
	spin_unlock_irqrestore(&fchan->lock, irqflags);
}

void st_fdma_parse_dt(struct platform_device *pdev,
		struct st_fdma_device *fdev)
{
	struct device_node *np = pdev->dev.of_node;
	struct st_plat_fdma_hw *hw;
	struct st_plat_fdma_fw_regs *fw;
	struct device_node *fwnp, *hwnp;
	u32 xbar;

	fw = devm_kzalloc(&pdev->dev, sizeof(*fw), GFP_KERNEL);
	hw = devm_kzalloc(&pdev->dev, sizeof(*hw), GFP_KERNEL);
	fdev->fw = fw;
	fdev->hw = hw;

	fdev->fdma_id = of_alias_get_id(np, "fdma");

	of_property_read_u32(np, "xbar", &xbar);
	fdev->xbar = xbar;

	/* fw */
	of_property_read_string(np, "fw-name", &fdev->fw_name);
	/* fw regs */
	fwnp = of_parse_phandle(np, "fw-regs", 0);
	of_property_read_u32(fwnp, "fw-rev-id", (u32 *)&fw->rev_id);
	of_property_read_u32(fwnp, "fw-mchi-rx-nb-cur",
				(u32 *)&fw->mchi_rx_nb_cur);
	of_property_read_u32(fwnp, "fw-mchi-rx-nb-all",
				(u32 *)&fw->mchi_rx_nb_all);
	of_property_read_u32(fwnp, "fw-cmd-statn", (u32 *)&fw->cmd_statn);
	of_property_read_u32(fwnp, "fw-req-ctln", (u32 *)&fw->req_ctln);
	of_property_read_u32(fwnp, "fw-ptrn", (u32 *)&fw->ptrn);
	of_property_read_u32(fwnp, "fw-ctrln", (u32 *)&fw->ctrln);
	of_property_read_u32(fwnp, "fw-cntn", (u32 *)&fw->cntn);
	of_property_read_u32(fwnp, "fw-saddrn", (u32 *)&fw->saddrn);
	of_property_read_u32(fwnp, "fw-daddrn", (u32 *)&fw->daddrn);
	of_property_read_u32(fwnp, "fw-node-size", (u32 *)&fw->node_size);
	/* hw */
	hwnp = of_parse_phandle(np, "hw-conf", 0);
	of_property_read_u32(hwnp, "slim-reg-id", (u32 *)&hw->slim_regs.id);
	of_property_read_u32(hwnp, "slim-reg-ver", (u32 *)&hw->slim_regs.ver);
	of_property_read_u32(hwnp, "slim-reg-en", (u32 *)&hw->slim_regs.en);
	of_property_read_u32(hwnp, "slim-reg-clk-gate",
				(u32 *)&hw->slim_regs.clk_gate);
	of_property_read_u32(hwnp, "slim-reg-slim-pc",
				(u32 *)&hw->slim_regs.slim_pc);

	of_property_read_u32(hwnp, "dmem-offset", (u32 *)&hw->dmem.offset);
	of_property_read_u32(hwnp, "dmem-size", (u32 *)&hw->dmem.size);

	of_property_read_u32(hwnp, "periph-reg-sync-reg",
				(u32 *)&hw->periph_regs.sync_reg);
	of_property_read_u32(hwnp, "periph-reg-cmd-sta",
				(u32 *)&hw->periph_regs.cmd_sta);
	of_property_read_u32(hwnp, "periph-reg-cmd-set",
				(u32 *)&hw->periph_regs.cmd_set);
	of_property_read_u32(hwnp, "periph-reg-cmd-clr",
				(u32 *)&hw->periph_regs.cmd_clr);
	of_property_read_u32(hwnp, "periph-reg-cmd-mask",
				(u32 *)&hw->periph_regs.cmd_mask);
	of_property_read_u32(hwnp, "periph-reg-int-sta",
				(u32 *)&hw->periph_regs.int_sta);
	of_property_read_u32(hwnp, "periph-reg-int-set",
				(u32 *)&hw->periph_regs.int_set);
	of_property_read_u32(hwnp, "periph-reg-int-clr",
				(u32 *)&hw->periph_regs.int_clr);
	of_property_read_u32(hwnp, "periph-reg-int-mask",
				(u32 *)&hw->periph_regs.int_mask);

	of_property_read_u32(hwnp, "imem-offset", (u32 *)&hw->imem.offset);
	of_property_read_u32(hwnp, "imem-size", (u32 *)&hw->imem.size);
}


/*
 * Platform driver initialise.
 */

static int st_fdma_probe(struct platform_device *pdev)
{
	struct st_plat_fdma_data *pdata = pdev->dev.platform_data;
	struct st_fdma_device *fdev;
	struct resource *iores;
	int result;
	int irq;
	int i;

	/* Allocate FDMA device structure */
	fdev = devm_kzalloc(&pdev->dev, sizeof(*fdev), GFP_KERNEL);
	if (!fdev) {
		dev_err(&pdev->dev, "Failed to allocate device structure\n");
		return -ENOMEM;
	}

	/* Initialise structures */
	fdev->dev = &pdev->dev;
	fdev->pdev = pdev;

	if (!pdev->dev.of_node) {
		fdev->fw = pdata->fw;
		fdev->hw = pdata->hw;
		fdev->xbar = pdata->xbar;
		fdev->fw_name = pdata->fw_name;
		fdev->fdma_id = pdev->id;
	} else {
		st_fdma_parse_dt(pdev, fdev);
	}

	spin_lock_init(&fdev->lock);
	spin_lock_init(&fdev->irq_lock);

	/* Retrieve FDMA platform memory resource */
	iores = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!iores) {
		dev_err(&pdev->dev, "Failed to get memory resource\n");
		return -ENXIO;
	}

	/* Retrieve the FDMA platform interrupt handler */
	irq = platform_get_irq(pdev, 0);
	if (irq < 0) {
		dev_err(&pdev->dev, "Failed to get irq resource\n");
		return -ENXIO;
	}
	dev_notice(&pdev->dev, "IRQ: %d\n", irq);

	/* Request the FDMA memory region */
	fdev->io_res = devm_request_mem_region(&pdev->dev, iores->start,
				resource_size(iores), pdev->name);
	if (fdev->io_res == NULL) {
		dev_err(&pdev->dev, "Failed to request memory region\n");
		return -EBUSY;
	}

	/* Remap the FDMA memory region into uncached memory */
	fdev->io_base = devm_ioremap_nocache(&pdev->dev, iores->start,
				resource_size(iores));
	if (fdev->io_base == NULL) {
		dev_err(&pdev->dev, "Failed to ioremap memory region\n");
		return -ENXIO;
	}
	dev_notice(&pdev->dev, "Base address: %p\n", fdev->io_base);

	/* Get all FDMA clocks */
	result = st_fdma_clk_get(fdev);
	if (result) {
		dev_err(&pdev->dev, "Failed to get all clocks\n");
		return result;
	}

	/* Enable the FDMA clocks */
	result = st_fdma_clk_enable(fdev);
	if (result) {
		dev_err(&pdev->dev, "Failed to enable clocks\n");
		goto error_clk_enb;
	}

	/* Initialise list of FDMA channels */
	INIT_LIST_HEAD(&fdev->dma_device.channels);
	for (i = ST_FDMA_MIN_CHANNEL; i <= ST_FDMA_MAX_CHANNEL; ++i) {
		struct st_fdma_chan *fchan = &fdev->ch_list[i];
		struct dma_chan *chan = &fchan->dma_chan;

		/* Set the channel data */
		fchan->fdev = fdev;
		fchan->id = i;

		/* Initialise channel lock and descriptor lists */
		spin_lock_init(&fchan->lock);
		INIT_LIST_HEAD(&fchan->desc_free);
		INIT_LIST_HEAD(&fchan->desc_queue);
		INIT_LIST_HEAD(&fchan->desc_active);

		/* Initialise the completion tasklet */
		tasklet_init(&fchan->tasklet_complete, st_fdma_desc_complete,
				(unsigned long) fchan);

		/* Set the dmaengine channel data */
		chan->device = &fdev->dma_device;

		/* Add the dmaengine channel to the dmaengine device */
		list_add_tail(&chan->device_node, &fdev->dma_device.channels);
	}

	/* Initialise the FDMA dreq data (reserve 0 & 31 for FDMA use) */
	spin_lock_init(&fdev->dreq_lock);
	fdev->dreq_mask = (1 << 0) | (1 << 31);

	/* Create the dma llu pool */
	fdev->dma_pool = dma_pool_create(dev_name(fdev->dev), NULL,
			ST_FDMA_LLU_SIZE, ST_FDMA_LLU_ALIGN, 0);
	if (fdev->dma_pool == NULL) {
		dev_err(fdev->dev, "Failed to create dma pool\n");
		result = -ENOMEM;
		goto error_dma_pool;
	}

	/* Set the FDMA register offsets */
	fdev->regs.id = fdev->hw->slim_regs.id;
	fdev->regs.ver = fdev->hw->slim_regs.ver;
	fdev->regs.en = fdev->hw->slim_regs.en;
	fdev->regs.clk_gate = fdev->hw->slim_regs.clk_gate;
	fdev->regs.slim_pc = fdev->hw->slim_regs.slim_pc;
	fdev->regs.rev_id = fdev->fw->rev_id;
	fdev->regs.mchi_rx_nb_cur = fdev->fw->mchi_rx_nb_cur;
	fdev->regs.mchi_rx_nb_all = fdev->fw->mchi_rx_nb_all;
	fdev->regs.cmd_statn = fdev->fw->cmd_statn;
	fdev->regs.req_ctln = fdev->fw->req_ctln;
	fdev->regs.ptrn = fdev->fw->ptrn;
	fdev->regs.ctrln = fdev->fw->ctrln;
	fdev->regs.cntn = fdev->fw->cntn;
	fdev->regs.saddrn = fdev->fw->saddrn;
	fdev->regs.daddrn = fdev->fw->daddrn;
	fdev->regs.node_size = fdev->fw->node_size ? : LEGACY_NODE_DATA_SIZE;
	fdev->regs.sync_reg = fdev->hw->periph_regs.sync_reg;
	fdev->regs.cmd_sta = fdev->hw->periph_regs.cmd_sta;
	fdev->regs.cmd_set = fdev->hw->periph_regs.cmd_set;
	fdev->regs.cmd_clr = fdev->hw->periph_regs.cmd_clr;
	fdev->regs.cmd_mask = fdev->hw->periph_regs.cmd_mask;
	fdev->regs.int_sta = fdev->hw->periph_regs.int_sta;
	fdev->regs.int_set = fdev->hw->periph_regs.int_set;
	fdev->regs.int_clr = fdev->hw->periph_regs.int_clr;
	fdev->regs.int_mask = fdev->hw->periph_regs.int_mask;

	/* Install the FDMA interrupt handler (and enabled the irq) */
	result = devm_request_irq(&pdev->dev, irq, st_fdma_irq_handler,
			IRQF_DISABLED|IRQF_SHARED, dev_name(&pdev->dev), fdev);
	if (result < 0) {
		dev_err(&pdev->dev, "Failed to request irq\n");
		result = -EBUSY;
		goto error_req_irq;
	}

	/* Create the firmware loading wait queue */
	init_waitqueue_head(&fdev->fw_load_q);

	/* Set the FDMA device capabilities */
	dma_cap_set(DMA_SLAVE,  fdev->dma_device.cap_mask);
	dma_cap_set(DMA_CYCLIC, fdev->dma_device.cap_mask);
	dma_cap_set(DMA_MEMCPY, fdev->dma_device.cap_mask);

	/* Initialise the dmaengine device */
	fdev->dma_device.dev = &pdev->dev;

	fdev->dma_device.device_alloc_chan_resources =
		st_fdma_alloc_chan_resources;
	fdev->dma_device.device_free_chan_resources =
		st_fdma_free_chan_resources;
	fdev->dma_device.device_prep_dma_memcpy	= st_fdma_prep_dma_memcpy;
	fdev->dma_device.device_prep_slave_sg	= st_fdma_prep_slave_sg;
	fdev->dma_device.device_prep_dma_cyclic	= st_fdma_prep_dma_cyclic;
	fdev->dma_device.device_control		= st_fdma_control;
	fdev->dma_device.device_tx_status	= st_fdma_tx_status;
	fdev->dma_device.device_issue_pending	= st_fdma_issue_pending;

	/* Register the dmaengine device */
	result = dma_async_device_register(&fdev->dma_device);
	if (result) {
		dev_err(&pdev->dev, "Failed to register DMA device\n");
		goto error_register;
	}

	/* Register the device with debugfs */
	st_fdma_debugfs_register(fdev);

	/* Associate this FDMA with platform device */
	platform_set_drvdata(pdev, fdev);

	return 0;

error_register:
error_req_irq:
	dma_pool_destroy(fdev->dma_pool);
error_dma_pool:
	/* Kill completion tasklet for each channel */
	for (i = ST_FDMA_MIN_CHANNEL; i <= ST_FDMA_MAX_CHANNEL; ++i) {
		struct st_fdma_chan *fchan = &fdev->ch_list[i];

		tasklet_disable(&fchan->tasklet_complete);
		tasklet_kill(&fchan->tasklet_complete);
	}
error_clk_enb:
	st_fdma_clk_disable(fdev);
	return result;
}

static int st_fdma_remove(struct platform_device *pdev)
{
	struct st_fdma_device *fdev = platform_get_drvdata(pdev);
	int i;

	/* Clear the platform driver data */
	platform_set_drvdata(pdev, NULL);

	/* Unregister the device from debugfs */
	st_fdma_debugfs_unregister(fdev);

	/* Unregister the dmaengine device */
	dma_async_device_unregister(&fdev->dma_device);

	/* Disable all channels */
	st_fdma_hw_channel_disable_all(fdev);

	/* Turn off and release the FDMA clocks */
	st_fdma_clk_disable(fdev);

	/* Destroy the dma pool */
	dma_pool_destroy(fdev->dma_pool);

	/* Kill tasklet for each channel */
	for (i = ST_FDMA_MIN_CHANNEL; i <= ST_FDMA_MAX_CHANNEL; ++i) {
		struct st_fdma_chan *fchan = &fdev->ch_list[i];

		tasklet_disable(&fchan->tasklet_complete);
		tasklet_kill(&fchan->tasklet_complete);
	}

	return 0;
}


/*
 * Power management
 */

#ifdef CONFIG_PM_SLEEP
static int st_fdma_pm_suspend(struct device *dev)
{
	struct st_fdma_device *fdev = dev_get_drvdata(dev);
	int result;

	/* We can only suspend after firmware has been loaded */
	if (fdev->fw_state != ST_FDMA_FW_STATE_LOADED) {
		dev_err(fdev->dev, "Cannot suspend as firmware never loaded\n");
		goto exit;
	}

	/*
	 * At this point the channel users are already suspended. This makes
	 * safe the 'channel_disable_all' call.
	 */

	/* Disable all channels (prevents memory access in self-refresh) */
	result = st_fdma_hw_channel_disable_all(fdev);
	if (result) {
		dev_err(fdev->dev, "Failed to disable channels on suspend\n");
		return -ENODEV;
	}

	st_fdma_hw_disable(fdev);

exit:
	/* Disable the FDMA clocks */
	st_fdma_clk_disable(fdev);

	return 0;
}

static int st_fdma_pm_resume(struct device *dev)
{
	struct st_fdma_device *fdev = dev_get_drvdata(dev);

	/* Enable the FDMA clocks */
	st_fdma_clk_enable(fdev);

	/* We can only resume after firmware has been loaded */
	fdev->fw_state = ST_FDMA_FW_STATE_INIT;
	return 0;
}

const struct dev_pm_ops st_fdma_pm_ops = {
	.suspend_late = st_fdma_pm_suspend,
	.resume_early = st_fdma_pm_resume,
};
#define ST_FDMA_PM_OPS	(&st_fdma_pm_ops)
#else
#define ST_FDMA_PM_OPS	NULL
#endif

static struct of_device_id st_fdma_match[] = {
	{ .compatible = "st,fdma", },
	{},
};
MODULE_DEVICE_TABLE(of, st_fdma_match);

static struct platform_driver st_fdma_platform_driver = {
	.driver = {
		.name = "st-fdma",
		.owner = THIS_MODULE,
		.of_match_table = st_fdma_match,
		.pm = ST_FDMA_PM_OPS,
	},
	.probe = st_fdma_probe,
	.remove = st_fdma_remove,
};

static int st_fdma_init(void)
{
	int ret;

	st_fdma_debugfs_init();

	ret = platform_driver_register(&st_fdma_platform_driver);
	if (ret) {
		st_fdma_debugfs_exit();
		return ret;
	}

	return 0;
}

static void st_fdma_exit(void)
{
	st_fdma_debugfs_exit();

	platform_driver_unregister(&st_fdma_platform_driver);
}
subsys_initcall(st_fdma_init);
module_exit(st_fdma_exit);

MODULE_AUTHOR("John Boddie <john.boddie@st.com>");
MODULE_DESCRIPTION("STMicroelectronics FDMA dmaengine driver");
MODULE_LICENSE("GPL");

