/*
 * STMicroelectronics FDMA dmaengine driver MCHI Rx extensions
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
 * MCHI packet defines
 */
#define ST_FDMA_MCHI_MAX_PAYLOAD	4096
#define ST_FDMA_MCHI_MIN_PACKET	3
#define ST_FDMA_MCHI_MAX_PACKET	(3 + 4096)


/*
 * MCHI Rx node control defines
 */
#define MCHI_CONTROL_REQ_MAP_MASK	0x0000001f
#define MCHI_CONTROL_REQ_MAP_EXT	0x0000001f
#define MCHI_CONTROL_TYPE_MASK		0x000000e0
#define MCHI_CONTROL_TYPE_MCHI		0x000000c0
#define MCHI_CONTROL_DREQ_MASK		0x00001f00
#define MCHI_CONTROL_DREQ_SHIFT		8
#define MCHI_CONTROL_SECURE		0x00008000
#define MCHI_CONTROL_PKT_RX_DREQ_MASK	0x001f0000
#define MCHI_CONTROL_PKT_RX_DREQ_SHIFT	16
#define MCHI_CONTROL_COMP_PAUSE		0x40000000
#define MCHI_CONTROL_COMP_IRQ		0x80000000


/*
 * MCHI structures
 */
struct st_fdma_mchi {
	struct st_dma_dreq_config *pkt_start_rx_dreq;
	u32 rx_fifo_threshold_addr;
};


/*
 * MCHI dmaengine extension resources functions
 */
int st_fdma_mchi_alloc_chan_resources(struct st_fdma_chan *fchan)
{
	struct st_dma_mchi_config *config = fchan->dma_chan.private;
	struct st_fdma_mchi *mchi;
	int result;

	dev_dbg(fchan->fdev->dev, "%s(fchan=%p)\n", __func__, fchan);

	BUG_ON(fchan->type != ST_DMA_TYPE_MCHI);

	/* Allocate the mchi channel structure */
	mchi = kzalloc(sizeof(struct st_fdma_mchi), GFP_KERNEL);
	if (!mchi) {
		dev_err(fchan->fdev->dev, "Failed to alloc mchi structure\n");
		return -ENOMEM;
	}

	/* Initialise the mchi channel structure */
	mchi->rx_fifo_threshold_addr = config->rx_fifo_threshold_addr;

	/* Save mchi extensions structure in the channel */
	fchan->extension = mchi;

	/* Set the initial dma address */
	fchan->dma_addr = config->dma_addr;

	/* Allocate the dreq */
	fchan->dreq = st_fdma_dreq_alloc(fchan, &config->dreq_config);
	if (!fchan->dreq) {
		dev_err(fchan->fdev->dev, "Failed to allocate mchi dreq\n");
		result = -EBUSY;
		goto error_dreq_alloc;
	}

	/* Configure the dreq */
	result = st_fdma_dreq_config(fchan, fchan->dreq);
	if (result) {
		dev_err(fchan->fdev->dev, "Failed to configure mchi dreq\n");
		goto error_dreq_config;
	}

	/* Allocate the packet start rx dreq */
	mchi->pkt_start_rx_dreq = st_fdma_dreq_alloc(fchan,
			&config->pkt_start_rx_dreq_config);
	if (!mchi->pkt_start_rx_dreq) {
		dev_err(fchan->fdev->dev, "Failed to allocate mchi rx dreq\n");
		result = -EBUSY;
		goto error_rx_dreq_alloc;
	}

	/* Configure the packet start rx dreq */
	result = st_fdma_dreq_config(fchan, mchi->pkt_start_rx_dreq);
	if (result) {
		dev_err(fchan->fdev->dev, "Failed to configure mchi rx dreq\n");
		goto error_rx_dreq_config;
	}

	return 0;

error_rx_dreq_config:
	st_fdma_dreq_free(fchan, mchi->pkt_start_rx_dreq);
error_rx_dreq_alloc:
error_dreq_config:
	st_fdma_dreq_free(fchan, fchan->dreq);
error_dreq_alloc:
	kfree(mchi);
	fchan->extension = NULL;
	return result;
}

void st_fdma_mchi_free_chan_resources(struct st_fdma_chan *fchan)
{
	struct st_fdma_mchi *mchi = fchan->extension;

	dev_dbg(fchan->fdev->dev, "%s(fchan=%p)\n", __func__, fchan);

	BUG_ON(fchan->type != ST_DMA_TYPE_MCHI);
	BUG_ON(!mchi);

	/* Free the dreqs */
	st_fdma_dreq_free(fchan, mchi->pkt_start_rx_dreq);
	st_fdma_dreq_free(fchan, fchan->dreq);

	/* Free the mchi data (kfree handles null arg) */
	kfree(fchan->extension);
	fchan->extension = NULL;
}


/*
 * MCHI dmaengine extensions API
 */
struct dma_async_tx_descriptor *dma_mchi_prep_rx_cyclic(struct dma_chan *chan,
		struct scatterlist *sgl, unsigned int sg_len)
{
	struct st_fdma_chan *fchan = to_st_fdma_chan(chan);
	struct st_fdma_mchi *mchi = fchan->extension;
	struct st_fdma_desc *head = NULL;
	struct st_fdma_desc *prev = NULL;
	struct scatterlist *sg;
	int result;
	int i;

	dev_dbg(fchan->fdev->dev, "%s(chan=%p, sgl=%p, sg_len=%d)\n", __func__,
			chan, sgl, sg_len);

	/* Only allow this function on mchi channels */
	BUG_ON(fchan->type != ST_DMA_TYPE_MCHI);
	BUG_ON(!mchi);

	/* Quick sanity check on parameters */
	if ((!sgl) || (sg_len < 2)) {
		dev_err(fchan->fdev->dev, "Invalid scatterlist parameters\n");
		return NULL;
	}

	/* Check each scatter-gather entry can hold maximum payload size */
	for_each_sg(sgl, sg, sg_len, i)
		if (sg_dma_len(sg) < ST_FDMA_MCHI_MAX_PACKET) {
			dev_err(fchan->fdev->dev, "sgl[%d] length is less than"
					" max packet size (%d)\n", i,
					ST_FDMA_MCHI_MAX_PACKET);
			return NULL;
		}

	/* The slave must be configured! */
	if (!fchan->dma_addr) {
		dev_err(fchan->fdev->dev, "Slave not configured!\n");
		return NULL;
	}

	/* Only a single transfer allowed as channel is cyclic */
	result = test_and_set_bit(ST_FDMA_IS_CYCLIC, &fchan->flags);
	if (result) {
		dev_err(fchan->fdev->dev, "Channel %d already in use\n",
				fchan->id);
		return NULL;
	}

	/* Ensure we have dreq information */
	BUG_ON(!fchan->dreq);
	BUG_ON(!mchi->pkt_start_rx_dreq);

	/* Build a cyclic linked list */
	for_each_sg(sgl, sg, sg_len, i) {
		/* Allocate a descriptor for this node */
		struct st_fdma_desc *fdesc = st_fdma_desc_get(fchan);
		if (!fdesc) {
			dev_err(fchan->fdev->dev, "Not enough decriptors\n");
			goto error_desc_get;
		}

		/* Configure the desciptor llu */
		fdesc->llu->next = 0;
		fdesc->llu->control = NODE_CONTROL_REQ_MAP_EXT;
		fdesc->llu->control |= MCHI_CONTROL_TYPE_MCHI;
		fdesc->llu->control |= fchan->dreq->request_line <<
				MCHI_CONTROL_DREQ_SHIFT;
		fdesc->llu->control |= mchi->pkt_start_rx_dreq->request_line <<
				MCHI_CONTROL_PKT_RX_DREQ_SHIFT;
		fdesc->llu->control |= MCHI_CONTROL_COMP_IRQ;
		fdesc->llu->nbytes = sg_dma_len(sg);
		fdesc->llu->saddr = fchan->dma_addr;
		fdesc->llu->daddr = sg_dma_address(sg);

		/* Configure the mchi rx specific parameters */
		fdesc->llu->mchi.length = sg_dma_len(sg);
		fdesc->llu->mchi.rx_fifo_threshold_add =
				mchi->rx_fifo_threshold_addr;
		fdesc->llu->mchi.dstride = 0;

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
EXPORT_SYMBOL(dma_mchi_prep_rx_cyclic);
