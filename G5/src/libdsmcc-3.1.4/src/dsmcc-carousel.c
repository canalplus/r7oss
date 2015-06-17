#include <stdlib.h>
#include <string.h>

#include "dsmcc.h"
#include "dsmcc-carousel.h"
#include "dsmcc-cache-module.h"
#include "dsmcc-cache-file.h"

/* default timeout for aquisition of DSI message in microseconds (30s) */
#define DEFAULT_DSI_TIMEOUT (30 * 1000000)

#define CAROUSEL_CACHE_FILE_MAGIC 0xDDCC0002

static struct dsmcc_object_carousel *find_carousel_by_requested_pid(struct dsmcc_state *state, uint16_t pid)
{
	struct dsmcc_object_carousel *carousel;

	for (carousel = state->carousels; carousel; carousel = carousel->next)
		if (carousel->requested_pid == pid)
			return carousel;
	return NULL;
}

static void stop_carousel(struct dsmcc_object_carousel *carousel)
{
	DSMCC_DEBUG("Stopping download for carousel 0x%08x on PID 0x%04x", carousel->cid, carousel->requested_pid);

	dsmcc_stream_queue_remove(carousel, DSMCC_QUEUE_ENTRY_DSI);
	dsmcc_stream_queue_remove(carousel, DSMCC_QUEUE_ENTRY_DII);
	dsmcc_stream_queue_remove(carousel, DSMCC_QUEUE_ENTRY_DDB);
	dsmcc_timeout_remove_all(carousel);

	/* set default unknown value for transaction ids */
	carousel->dsi_transaction_id = 0xFFFFFFFF;
	carousel->dii_transaction_id = 0xFFFFFFFF;
	if (carousel->status == DSMCC_STATUS_DOWNLOADING)
	{
		dsmcc_object_carousel_set_status(carousel, DSMCC_STATUS_PARTIAL);
		dsmcc_filecache_notify_status(carousel, NULL);
	}
}

static void start_carousel(struct dsmcc_object_carousel *carousel)
{
	if (carousel->status != DSMCC_STATUS_PARTIAL && carousel->status != DSMCC_STATUS_TIMEDOUT)
		return;

	DSMCC_DEBUG("Starting download for carousel 0x%08x on PID 0x%04x", carousel->cid, carousel->requested_pid);

	dsmcc_stream_queue_add(carousel, DSMCC_STREAM_SELECTOR_PID, carousel->requested_pid, DSMCC_QUEUE_ENTRY_DSI, carousel->requested_transaction_id);
	/* add section filter on stream for DSI (table_id == 0x3B, table_id_extension == 0x0000 or 0x0001) */
	if (carousel->state->callbacks.add_section_filter)
	{
		uint8_t pattern[3]  = { 0x3B, 0x00, 0x00 };
		uint8_t equal[3]    = { 0xff, 0xff, 0xfe };
		uint8_t notequal[3] = { 0x00, 0x00, 0x00 };
		(*carousel->state->callbacks.add_section_filter)(carousel->state->callbacks.add_section_filter_arg,
				carousel->requested_pid, pattern, equal, notequal, 3);
	}

	dsmcc_timeout_set(carousel, DSMCC_TIMEOUT_DSI, 0, DEFAULT_DSI_TIMEOUT);

	dsmcc_object_carousel_set_status(carousel, DSMCC_STATUS_DOWNLOADING);
	dsmcc_filecache_notify_status(carousel, NULL);
}

void dsmcc_object_carousel_queue_remove(struct dsmcc_state *state, uint32_t queue_id)
{
	struct dsmcc_object_carousel *carousel;
	struct dsmcc_file_cache *filecache;

	// remove filecache for queue_id
	for (carousel = state->carousels; carousel; carousel = carousel->next)
	{
		filecache = dsmcc_filecache_find(carousel, queue_id);
		if (filecache)
		{
			dsmcc_filecache_remove(filecache);
			break;
		}
	}

	// carousel found and has no more filecaches, stop it
	if (carousel && !carousel->filecaches)
		stop_carousel(carousel);
}

void dsmcc_object_carousel_queue_add(struct dsmcc_state *state, uint32_t queue_id, int type, uint16_t pid, uint32_t transaction_id,
		const char *downloadpath, struct dsmcc_carousel_callbacks *callbacks)
{
	struct dsmcc_object_carousel *carousel;
	/* Check if carousel is already requested */
	carousel = find_carousel_by_requested_pid(state, pid);
	if (!carousel)
	{
		carousel = calloc(1, sizeof(struct dsmcc_object_carousel));
		carousel->state = state;
		carousel->status = DSMCC_STATUS_PARTIAL;
		carousel->next = state->carousels;
		carousel->type = type;
		carousel->group_list = NULL;
		carousel->requested_pid = pid;
		carousel->requested_transaction_id = transaction_id;
		state->carousels = carousel;

		/* set default unknown value for transaction ids */
		carousel->dsi_transaction_id = 0xFFFFFFFF;
		carousel->dii_transaction_id = 0xFFFFFFFF;
	}

	start_carousel(carousel);
	dsmcc_filecache_add(carousel, queue_id, downloadpath, callbacks);
}

#ifdef DEBUG
static const char *status_str(int status)
{
	switch (status)
	{
		case DSMCC_STATUS_PARTIAL:
			return "PARTIAL";
		case DSMCC_STATUS_DOWNLOADING:
			return "DOWNLOADING";
		case DSMCC_STATUS_TIMEDOUT:
			return "TIMED-OUT";
		case DSMCC_STATUS_DONE:
			return "DONE";
		default:
			return "Unknown!";
	}
}
#endif

void dsmcc_object_carousel_set_status(struct dsmcc_object_carousel *carousel, int newstatus)
{
	if (newstatus == carousel->status)
		return;

	DSMCC_DEBUG("Carousel 0x%08x status changed to %s", carousel->cid, status_str(newstatus));
	carousel->status = newstatus;
}

void dsmcc_object_carousel_free(struct dsmcc_object_carousel *carousel, bool keep_cache)
{
	/* stop carousel (timers, stream queues) */
	stop_carousel(carousel);

	/* free filecaches */
	dsmcc_filecache_remove_all(carousel);

	/* free modules */
	dsmcc_cache_free_all_modules(carousel, keep_cache);
	carousel->modules = NULL;

	/* free remaining data */
	free(carousel);
}

static void free_all_carousels(struct dsmcc_object_carousel *carousels, bool keep_cache)
{
	struct dsmcc_object_carousel *car, *nextcar;

	car = carousels;
	while (car)
	{
		nextcar = car->next;
		dsmcc_object_carousel_free(car, keep_cache);
		car = nextcar;
	}
}

void dsmcc_object_carousel_free_all(struct dsmcc_state *state, bool keep_cache)
{
	free_all_carousels(state->carousels, keep_cache);
	state->carousels = NULL;
}

bool dsmcc_object_carousel_load_all(FILE *f, struct dsmcc_state *state)
{
	uint32_t tmp;
	struct dsmcc_object_carousel *carousel = NULL, *lastcar = NULL;

	if (!fread(&tmp, sizeof(uint32_t), 1, f))
		goto error;
	if (tmp != CAROUSEL_CACHE_FILE_MAGIC)
		goto error;

	while (1)
	{
		if (!fread(&tmp, sizeof(uint32_t), 1, f))
			goto error;
		if (tmp)
			break;
		carousel = calloc(1, sizeof(struct dsmcc_object_carousel));
		carousel->state = state;
		carousel->group_list = NULL;
		if (!fread(&carousel->cid, sizeof(uint32_t), 1, f))
			goto error;
		if (!fread(&carousel->type, sizeof(int), 1, f))
			goto error;
		if (!fread(&carousel->status, sizeof(int), 1, f))
			goto error;
		if (!fread(&carousel->requested_pid, sizeof(uint16_t), 1, f))
			goto error;
		if (!fread(&carousel->requested_transaction_id, sizeof(uint32_t), 1, f))
			goto error;
		if (!dsmcc_cache_load_modules(f, carousel))
			goto error;

		if (carousel->status == DSMCC_STATUS_DOWNLOADING)
			carousel->status = DSMCC_STATUS_PARTIAL;
		if (state->carousels)
			lastcar->next = carousel;
		else
			state->carousels = carousel;
		lastcar = carousel;
		carousel = NULL;
	}

	return 1;
error:
	DSMCC_ERROR("Error while loading carousels");
	free_all_carousels(state->carousels, 0);
	if (carousel)
		free_all_carousels(carousel, 0);
	return 0;
}

bool dsmcc_object_carousel_save_all(FILE *f, struct dsmcc_state *state)
{
	uint32_t tmp;
	struct dsmcc_object_carousel *carousel;

	tmp = CAROUSEL_CACHE_FILE_MAGIC;
	if (!fwrite(&tmp, sizeof(uint32_t), 1, f))
		goto error;

	carousel = state->carousels;
	while (carousel)
	{
		tmp = 0;
		if (!fwrite(&tmp, sizeof(uint32_t), 1, f))
			goto error;
		if (!fwrite(&carousel->cid, sizeof(uint32_t), 1, f))
			goto error;
		if (!fwrite(&carousel->type, sizeof(int), 1, f))
			goto error;
		if (!fwrite(&carousel->status, sizeof(int), 1, f))
			goto error;
		if (!fwrite(&carousel->requested_pid, sizeof(uint16_t), 1, f))
			goto error;
		if (!fwrite(&carousel->requested_transaction_id, sizeof(uint32_t), 1, f))
			goto error;

		if (!dsmcc_cache_save_modules(f, carousel))
			goto error;

		carousel = carousel->next;
	}
	tmp = 1;
	if (!fwrite(&tmp, sizeof(uint32_t), 1, f))
		goto error;

	return 1;
error:
	return 0;
}

