/*
 * STMicroelectronics FDMA dmaengine driver descriptor functions
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
#include <linux/device.h>
#include <linux/dmapool.h>
#include <linux/dma-mapping.h>

#include "st_fdma.h"

/*
 * Descriptor functions
 */

struct st_fdma_desc *st_fdma_desc_alloc(struct st_fdma_chan *fchan)
{
	struct st_fdma_desc *fdesc;

	/* Allocate the descriptor */
	fdesc = kzalloc(sizeof(struct st_fdma_desc), GFP_KERNEL);
	if (!fdesc) {
		dev_err(fchan->fdev->dev, "Failed to alloc desc\n");
		goto error_kzalloc;
	}

	/* Allocate the llu from the dma pool */
	fdesc->llu = dma_pool_alloc(fchan->fdev->dma_pool, GFP_KERNEL,
			&fdesc->dma_desc.phys);
	if (!fdesc->llu) {
		dev_err(fchan->fdev->dev, "Failed to alloc from dma pool\n");
		goto error_dma_pool;
	}

	/* Initialise the descriptor */
	fdesc->fchan = fchan;
	INIT_LIST_HEAD(&fdesc->node);
	INIT_LIST_HEAD(&fdesc->llu_list);

	dma_async_tx_descriptor_init(&fdesc->dma_desc, &fchan->dma_chan);

	/* Set ack bit to ensure descriptor is ready for use */
	fdesc->dma_desc.flags = DMA_CTRL_ACK;
	fdesc->dma_desc.tx_submit = st_fdma_tx_submit;

	return fdesc;

error_dma_pool:
	kfree(fdesc);
error_kzalloc:
	return NULL;
}

static void st_fdma_desc_dealloc(struct st_fdma_desc *fdesc)
{
	struct st_fdma_device *fdev = fdesc->fchan->fdev;

	/* Free the llu back to the dma pool and free the descriptor */
	dma_pool_free(fdev->dma_pool, fdesc->llu, fdesc->dma_desc.phys);
	kfree(fdesc);
}

void st_fdma_desc_free(struct st_fdma_desc *fdesc)
{
	struct st_fdma_desc *ldesc, *_ldesc;

	BUG_ON(!fdesc);

	/* Ensure any lingering llu_list descriptors are also freed */
	if (!list_empty(&fdesc->llu_list))
		list_for_each_entry_safe(ldesc, _ldesc, &fdesc->llu_list, node)
			st_fdma_desc_dealloc(ldesc);

	st_fdma_desc_dealloc(fdesc);
}

struct st_fdma_desc *st_fdma_desc_get(struct st_fdma_chan *fchan)
{
	struct st_fdma_desc *fdesc = NULL, *tdesc, *_tdesc;
	unsigned long irqflags = 0;

	/*
	 * Search the free list for the next available descriptor. Only those
	 * descriptors that have been ACKed can be used. If a descriptor has
	 * been ACKed but has a non-empty llu_list, add the descriptors in the
	 * llu_list to the free list.
	 *
	 * We should only encounter a non-empty llu_list if a transfer is not
	 * automatically ACKed upon completion. In case the transfer is to be
	 * re-submitted, it is moved to the free list as a single complete
	 * transfer entity.
	 *
	 * When the transfer is finally ACKed, the only time we can move the
	 * llu_list to the free list is when getting the next available
	 * descriptor.
	 */

	spin_lock_irqsave(&fchan->lock, irqflags);

	/* Search the free list for an available descriptor */
	list_for_each_entry_safe(tdesc, _tdesc, &fchan->desc_free, node) {
		/* If descriptor has been ACKed then remove from free list */
		if (async_tx_test_ack(&tdesc->dma_desc)) {
			/* Add llu_list to free list in case not empty */
			list_splice_init(&tdesc->llu_list, &fchan->desc_free);
			list_del_init(&tdesc->node);
			fdesc = tdesc;
			break;
		}
		dev_dbg(fchan->fdev->dev, "Descriptor %p not ACKed\n", tdesc);
	}

	if (!fdesc) {
		spin_unlock_irqrestore(&fchan->lock, irqflags);

		/* No descriptors available, attempt to allocate a new one */
		dev_dbg(fchan->fdev->dev, "Allocating a new descriptor\n");
		fdesc = st_fdma_desc_alloc(fchan);
		if (!fdesc) {
			dev_err(fchan->fdev->dev, "Not enough descriptors\n");
			return NULL;
		}

		/* Increment number of descriptors allocated */
		spin_lock_irqsave(&fchan->lock, irqflags);
		fchan->desc_count++;
	}

	spin_unlock_irqrestore(&fchan->lock, irqflags);

	/* Re-initialise the descriptor */
	memset(fdesc->llu, 0, sizeof(struct st_fdma_llu));
	INIT_LIST_HEAD(&fdesc->llu_list);
	fdesc->dma_desc.cookie = 0;
	fdesc->dma_desc.callback = NULL;
	fdesc->dma_desc.callback_param = NULL;
	fdesc->dma_desc.tx_submit = st_fdma_tx_submit;

	return fdesc;
}

void st_fdma_desc_put(struct st_fdma_desc *fdesc)
{
	if (fdesc) {
		struct st_fdma_chan *fchan = fdesc->fchan;
		unsigned long irqflags = 0;

		spin_lock_irqsave(&fchan->lock, irqflags);

		/* Move all linked descriptors to the free list */
		list_splice_init(&fdesc->llu_list, &fchan->desc_free);
		list_add(&fdesc->node, &fchan->desc_free);

		spin_unlock_irqrestore(&fchan->lock, irqflags);
	}
}

void st_fdma_desc_chain(struct st_fdma_desc **head,
		struct st_fdma_desc **prev, struct st_fdma_desc *fdesc)
{
	if (!(*head)) {
		/* First descriptor becomes the head */
		*head = fdesc;
	} else {
		/* Link previous descriptor to this one */
		(*prev)->llu->next = fdesc->dma_desc.phys;
		list_add_tail(&fdesc->node, &(*head)->llu_list);
	}

	/* Descriptor just added now becomes the previous */
	*prev = fdesc;
}

/*
 * This function should be called with the channel locked!
 */
void st_fdma_desc_start(struct st_fdma_chan *fchan)
{
	struct st_fdma_desc *fdesc;

	/* There must be no active descriptors */
	if (!list_empty(&fchan->desc_active))
		return;

	/* Descriptor queue must not be empty */
	if (list_empty(&fchan->desc_queue))
		return;

	/* Remove first descriptor from queue and add to active list */
	fdesc = list_first_entry(&fchan->desc_queue, struct st_fdma_desc,
				node);
	list_del_init(&fdesc->node);
	list_add_tail(&fdesc->node, &fchan->desc_active);


	if (test_bit(ST_FDMA_IS_PARKED, &fchan->flags)) {
		/* Switch from parking to next descriptor (no interrupt) */
		BUG_ON(!fchan->desc_park);
		st_fdma_hw_channel_switch(fchan, fchan->desc_park, fdesc, 0);
		clear_bit(ST_FDMA_IS_PARKED, &fchan->flags);
	} else {
		/* Start the channel for the descriptor */
		st_fdma_hw_channel_start(fchan, fdesc);
		fchan->state = ST_FDMA_STATE_RUNNING;
	}
}

void st_fdma_desc_unmap_buffers(struct st_fdma_desc *fdesc)
{
	struct dma_async_tx_descriptor *desc = &fdesc->dma_desc;
	struct device *dev = desc->chan->device->dev;

	if (!(desc->flags & DMA_COMPL_SKIP_SRC_UNMAP)) {
		if (desc->flags & DMA_COMPL_SRC_UNMAP_SINGLE)
			dma_unmap_single(dev, fdesc->llu->saddr,
					fdesc->llu->nbytes, DMA_MEM_TO_DEV);
		else
			dma_unmap_page(dev, fdesc->llu->saddr,
					fdesc->llu->nbytes, DMA_MEM_TO_DEV);
	}

	if (!(desc->flags & DMA_COMPL_SKIP_DEST_UNMAP)) {
		if (desc->flags & DMA_COMPL_DEST_UNMAP_SINGLE)
			dma_unmap_single(dev, fdesc->llu->daddr,
					fdesc->llu->nbytes, DMA_DEV_TO_MEM);
		else
			dma_unmap_page(dev, fdesc->llu->daddr,
					fdesc->llu->nbytes, DMA_DEV_TO_MEM);
	}
}

void st_fdma_desc_complete(unsigned long data)
{
	struct st_fdma_chan *fchan = (struct st_fdma_chan *) data;
	struct st_fdma_desc *fdesc = NULL;
	unsigned long irqflags = 0;

	dev_dbg(fchan->fdev->dev, "%s(data=%08lx)\n", __func__, data);

	spin_lock_irqsave(&fchan->lock, irqflags);

	/*
	 * A terminate all or error may result in a completion with an empty
	 * active descriptor list. If no active descriptor, we are done, unless
	 * a descriptor start is requested.
	 */

	if (list_empty(&fchan->desc_active)) {
		/* Start the next descriptor (if available) */
		st_fdma_desc_start(fchan);
		spin_unlock_irqrestore(&fchan->lock, irqflags);
		return;
	}

	/* Get the head of the active list */
	fdesc = list_first_entry(&fchan->desc_active, struct st_fdma_desc,
			node);

	/* Process a non-cyclic descriptor */
	if (!test_bit(ST_FDMA_IS_CYCLIC, &fchan->flags)) {
		/* Remove non-cyclic descriptor from head of active list */
		list_del_init(&fdesc->node);

		/* Start the next descriptor */
		st_fdma_desc_start(fchan);

		/* Set the cookie to the last completed descriptor cookie */
		fchan->last_completed = fdesc->dma_desc.cookie;

		/* Unmap dma address for descriptor and children */
		if (!fchan->dma_chan.private) {
			struct st_fdma_desc *child;

			st_fdma_desc_unmap_buffers(fdesc);

			list_for_each_entry(child, &fdesc->llu_list, node)
					st_fdma_desc_unmap_buffers(child);
		}

		/*
		 * If the transfer has been ACKed, then all individual
		 * descriptors that make up the transfer can be moved to the
		 * free list. If the transfer has not been ACKed, then it is
		 * possible that it will be re-used, in which case the transfer
		 * should be moved to the free list as-is.
		 *
		 * Until the ACK bit is set, the descriptor will not be re-used.
		 * When the ACK bit is eventually set, and the descriptor is
		 * about to be re-used, if the llu_list is not empty, the
		 * llu_list will be added to the free list.
		 */

		/* Move the llu_list to the free list only if desriptor ACKed */
		if (fdesc->dma_desc.flags & DMA_CTRL_ACK)
			list_splice_init(&fdesc->llu_list, &fchan->desc_free);

		/* Move the descriptor to the free list (regardless of ACK) */
		list_move(&fdesc->node, &fchan->desc_free);
	}

	spin_unlock_irqrestore(&fchan->lock, irqflags);

	/* Issue callback */
	if (fdesc->dma_desc.callback) {
		fdesc->dma_desc.callback(fdesc->dma_desc.callback_param);
		trace_printk("dma_desc:%p\n", fdesc->dma_desc);	
	}
}
