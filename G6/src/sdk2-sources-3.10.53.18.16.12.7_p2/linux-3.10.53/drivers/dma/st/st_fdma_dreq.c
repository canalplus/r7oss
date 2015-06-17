/*
 * STMicroelectronics FDMA dmaengine driver dreq functions
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
#include <linux/platform_device.h>

#include "st_fdma.h"


/*
 * DREQ configuration
 */

static LIST_HEAD(st_fdma_dreq_router_list);

int st_fdma_register_dreq_router(struct st_fdma_dreq_router *router)
{
	list_add(&router->list, &st_fdma_dreq_router_list);

	return 0;
}

void st_fdma_unregister_dreq_router(struct st_fdma_dreq_router *router)
{
	struct st_fdma_dreq_router *tmp, *next = NULL;

	list_for_each_entry_safe(tmp, next, &st_fdma_dreq_router_list, list)
		if (tmp->xbar_id == router->xbar_id)
			list_del(&tmp->list);
}

static struct st_fdma_dreq_router *st_fdma_dreq_get_router(
		struct st_fdma_device *fdev)
{
	struct st_fdma_dreq_router *tmp, *router = NULL;

	list_for_each_entry(tmp, &st_fdma_dreq_router_list, list)
		if (tmp->xbar_id == fdev->xbar)
			router = tmp;

	return router;
}

struct st_dma_dreq_config *st_fdma_dreq_alloc(struct st_fdma_chan *fchan,
		struct st_dma_dreq_config *config)
{
	struct st_fdma_device *fdev = fchan->fdev;
	struct st_fdma_dreq_router *router = NULL;
	struct st_dma_dreq_config *dreq = NULL;
	int request_line;
	int result;

	spin_lock(&fdev->dreq_lock);

	/* If not explicit directly connected, get any dreq xbar router */
	if (!config->direct_conn)
		router = st_fdma_dreq_get_router(fdev);

	if (router) {
		dev_dbg(fdev->dev, "Request line routed through xbar\n");

		/* DREQ crossbar - get first available request line */
		if (fdev->dreq_mask == ~0L) {
			dev_err(fdev->dev, "No request lines available!\n");
			goto error_dreq_mask;
		}

		/* Find first unused request line */
		request_line = ffz(fdev->dreq_mask);

		/* Route the dreq through the crossbar */
		result = router->route(router, config->request_line,
				       fdev->fdma_id, request_line);
		if (result) {
			dev_err(fdev->dev, "Error routing request line!\n");
			goto error_dreq_route;
		}
	} else {
		/* No DREQ crossbar - request lines directly connected */
		unsigned long mask;

		dev_dbg(fdev->dev, "Request line directly connected\n");

		/* Request line must not exceed number of DREQs */
		if (config->request_line >= ST_FDMA_NUM_DREQS) {
			dev_err(fdev->dev, "Invalid request line\n");
			goto error_dreq_invalid;
		}

		mask = 1 << config->request_line;

		/* Check if the dreq is in use */
		if (fdev->dreq_mask & mask) {
			dev_err(fdev->dev, "Request line in use!\n");
			goto error_dreq_mask;
		}

		request_line = config->request_line;
	}

	/* Mark request line as used */
	fdev->dreq_mask |= (1 << request_line);

	/* Save the dreq configuration */
	dreq = &fdev->dreq_list[request_line];

	dreq->request_line = request_line;
	dreq->initiator	= config->initiator;
	dreq->increment	= config->increment;
	dreq->hold_off	= config->hold_off;
	dreq->maxburst	= config->maxburst;
	dreq->buswidth	= config->buswidth;
	dreq->direction	= config->direction;

	spin_unlock(&fdev->dreq_lock);

	return dreq;

error_dreq_route:
error_dreq_invalid:
error_dreq_mask:
	spin_unlock(&fdev->dreq_lock);
	return NULL;
}

void st_fdma_dreq_free(struct st_fdma_chan *fchan,
		struct st_dma_dreq_config *config)
{
	struct st_fdma_device *fdev = fchan->fdev;

	BUG_ON(!config);

	spin_lock(&fdev->dreq_lock);

	/* Indicate the dreq is no longer used */
	fdev->dreq_mask &=  ~(1 << config->request_line);

	/* Clear the dreq configuration */
	memset(config, 0, sizeof(struct st_dma_dreq_config));

	spin_unlock(&fdev->dreq_lock);
}

int st_fdma_dreq_config(struct st_fdma_chan *fchan,
		struct st_dma_dreq_config *config)
{
	struct st_fdma_device *fdev = fchan->fdev;
	int result;

	BUG_ON(!config);

	spin_lock(&fdev->dreq_lock);

	result = st_fdma_hw_channel_set_dreq(fchan, config);

	spin_unlock(&fdev->dreq_lock);

	return result;
}
