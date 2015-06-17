#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

#include "dsmcc.h"
#include "dsmcc-util.h"
#include "dsmcc-carousel.h"
#include "dsmcc-section.h"
#include "dsmcc-cache-file.h"

struct dsmcc_queue_entry
{
	struct dsmcc_stream          *stream;
	struct dsmcc_object_carousel *carousel;
	int                           type;
	uint32_t                      id; /* DSI: transaction ID (optional) / DII: transaction ID / DDB: download ID */

	struct dsmcc_queue_entry *next, *prev;
};

static void load_state(struct dsmcc_state *state)
{
	FILE *f;
	struct stat s;

	if (stat(state->cachefile, &s) < 0)
		return;

	DSMCC_DEBUG("Loading cached state");

	f = fopen(state->cachefile, "r");
	if (!dsmcc_object_carousel_load_all(f, state))
		DSMCC_ERROR("Error while loading cached state");
	fclose(f);
}

static void save_state(struct dsmcc_state *state)
{
	FILE *f;

	if (!state->keep_cache)
		return;

	DSMCC_DEBUG("Saving state");

	f = fopen(state->cachefile, "w");
	if (!dsmcc_object_carousel_save_all(f, state))
		DSMCC_ERROR("Error while saving cached state");
	fclose(f);
}

static void clear_single_carousel(struct dsmcc_state *state, uint32_t carousel_id)
{
	struct dsmcc_object_carousel *carousel, **prev;

	carousel = state->carousels;
	prev = &state->carousels;

	while (carousel)
	{
		if (carousel->cid == carousel_id)
		{
			(*prev) = carousel->next;

			DSMCC_DEBUG("Freeing cached data for carousel 0x%08x", carousel_id);
			dsmcc_object_carousel_free(carousel, 0);
			break;
		}

		prev = &(carousel->next);
		carousel = carousel->next;
	}
}

void *dsmcc_thread_func(void *arg)
{
	struct dsmcc_state *state = (struct dsmcc_state *) arg;

	while (1)
	{
		struct dsmcc_action *buffered_actions;

		pthread_mutex_lock(&state->mutex);

		if (!state->stop && !state->first_action)
		{
			/* compute waiting time: next timeout or if none, infinite waiting time */
			if (state->timeouts)
			{
				struct timeval *waketime;
				struct timespec wakets;

				waketime = &state->timeouts->abstime;

				if (dsmcc_log_enabled(DSMCC_LOG_DEBUG))
				{
					struct timeval curtime;
					struct timeval waittime;
					gettimeofday(&curtime, NULL);
					timersub(waketime, &curtime, &waittime);
					DSMCC_DEBUG("Waiting %d.%06d second(s) for wakeup", waittime.tv_sec, waittime.tv_usec);
				}

				wakets.tv_sec = waketime->tv_sec;
				wakets.tv_nsec = waketime->tv_usec * 1000;
				pthread_cond_timedwait(&state->cond, &state->mutex, &wakets);
			}
			else
			{
				DSMCC_DEBUG("Wait indefinitely for wakeup");
				pthread_cond_wait(&state->cond, &state->mutex);
			}
		}

		buffered_actions = state->first_action;
		state->first_action = state->last_action = NULL;

		pthread_mutex_unlock(&state->mutex);

		/* stop is requested, quit thread immediately */
		if (state->stop)
			break;

		/* handle all buffered actions */
		while (buffered_actions && !state->stop)
		{
			struct dsmcc_action *action = buffered_actions;
			buffered_actions = buffered_actions->next;
			switch (action->type)
			{
				case DSMCC_ACTION_ADD_CAROUSEL:
					DSMCC_DEBUG("Adding carousel to queue, PID 0x%04x queue_id %u",
							action->add_carousel.pid, action->add_carousel.queue_id);
					dsmcc_object_carousel_queue_add(state, action->add_carousel.queue_id,
							action->add_carousel.type, action->add_carousel.pid,
							action->add_carousel.transaction_id, action->add_carousel.downloadpath,
							&action->add_carousel.callbacks);
					free(action->add_carousel.downloadpath);
					break;
				case DSMCC_ACTION_REMOVE_CAROUSEL:
					DSMCC_DEBUG("Removing carousel from queue, queue_id %u", action->remove_carousel.queue_id);
					dsmcc_object_carousel_queue_remove(state, action->remove_carousel.queue_id);
					break;
				case DSMCC_ACTION_ADD_SECTION:
					DSMCC_DEBUG("Parsing a section for PID 0x%04x size %d", action->add_section.section->pid,
							action->add_section.section->length);
					dsmcc_parse_section(state, action->add_section.section);
					free(action->add_section.section);
					break;
				case DSMCC_ACTION_CACHE_CLEAR:
					DSMCC_DEBUG("Clearing all cache");
					dsmcc_object_carousel_free_all(state, 0);
					break;
				case DSMCC_ACTION_CACHE_CLEAR_CAROUSEL:
					DSMCC_DEBUG("Clearing cache for carousel 0x%08x", action->cache_clear_carousel.carousel_id);
					clear_single_carousel(state, action->cache_clear_carousel.carousel_id);
					break;
				default:
					break;
			}
			free(action);
		}
		if (buffered_actions)
		{
			// put back unprocessed actions
			struct dsmcc_action *first_action = buffered_actions;
			while (buffered_actions->next)
				buffered_actions = buffered_actions->next;
			pthread_mutex_lock(&state->mutex);
			buffered_actions->next = state->first_action;
			state->first_action = first_action;
			pthread_mutex_unlock(&state->mutex);
		}

		/* handle expired timeouts */
		if (!state->stop)
		{
			struct dsmcc_timeout *prevtimeout, *timeout, *nexttimeout;

			DSMCC_DEBUG("Current timeouts:");
			timeout = state->timeouts;
			prevtimeout = NULL;
			while (timeout)
			{
				struct timeval curtime;

				gettimeofday(&curtime, NULL);

				if (dsmcc_log_enabled(DSMCC_LOG_DEBUG))
				{
					struct timeval waittime;
					timersub(&timeout->abstime, &curtime, &waittime);
					DSMCC_DEBUG("CID 0x%08x DELAY %d.%06d TYPE %d MODULE_ID 0x%04hhx", timeout->carousel->cid, waittime.tv_sec, waittime.tv_usec, timeout->type, timeout->module_id);
				}

				nexttimeout = timeout->next;
				if (timercmp(&timeout->abstime, &curtime, <))
				{
					dsmcc_object_carousel_set_status(timeout->carousel, DSMCC_STATUS_TIMEDOUT);
					dsmcc_filecache_notify_status(timeout->carousel, NULL);

					/* remove timeout */
					if (prevtimeout)
						prevtimeout->next = timeout->next;
					else
						state->timeouts = timeout->next;
					free(timeout);
				}
				timeout = nexttimeout;
			}
		}

		save_state(state);
	}

	pthread_exit(0);
}

struct dsmcc_state *dsmcc_open(const char *cachedir, bool keep_cache, struct dsmcc_dvb_callbacks *callbacks)
{
	struct dsmcc_state *state = NULL;

	state = calloc(1, sizeof(struct dsmcc_state));

	memcpy(&state->callbacks, callbacks, sizeof(struct dsmcc_dvb_callbacks));

	if (cachedir == NULL || strlen(cachedir) == 0)
	{
		char buffer[32];
		sprintf(buffer, "/tmp/libdsmcc-cache-%d", getpid());
		if (buffer[strlen(buffer) - 1] == '/')
			buffer[strlen(buffer) - 1] = '\0';
		state->cachedir = strdup(buffer);
	}
	else
		state->cachedir = strdup(cachedir);
	mkdir(state->cachedir, 0755);
	state->keep_cache = keep_cache;

	state->cachefile = malloc(strlen(state->cachedir) + 7);
	sprintf(state->cachefile, "%s/state", state->cachedir);

    if (keep_cache)
	    load_state(state);

	pthread_mutex_init(&state->mutex, NULL);
	pthread_cond_init(&state->cond, NULL);
	pthread_create(&state->thread, NULL, &dsmcc_thread_func, state);

	return state;
}

static void buffer_action(struct dsmcc_state *state, struct dsmcc_action *action)
{
	pthread_mutex_lock(&state->mutex);

	if (state->last_action)
		state->last_action->next = action;
	state->last_action = action;
	if (!state->first_action)
		state->first_action = action;

	pthread_cond_signal(&state->cond);
	pthread_mutex_unlock(&state->mutex);
}

struct dsmcc_stream *dsmcc_stream_find_by_pid(struct dsmcc_state *state, uint16_t pid)
{
	struct dsmcc_stream *str;

	for (str = state->streams; str; str = str->next)
	{
		if (str->pid == pid)
			break;
	}

	return str;
}

static struct dsmcc_stream *find_stream_by_assoc_tag(struct dsmcc_stream *streams, uint16_t assoc_tag)
{
	struct dsmcc_stream *str;
	int i;

	for (str = streams; str; str = str->next)
		for (i = 0; i < str->assoc_tag_count; i++)
			if (str->assoc_tags[i] == assoc_tag)
				return str;

	return NULL;
}

void dsmcc_stream_add_assoc_tag(struct dsmcc_stream *stream, uint16_t assoc_tag)
{
	int i;

	for (i = 0; i < stream->assoc_tag_count; i++)
		if (stream->assoc_tags[i] == assoc_tag)
			return;

	stream->assoc_tag_count++;
	stream->assoc_tags = realloc(stream->assoc_tags, stream->assoc_tag_count * sizeof(*stream->assoc_tags));
	stream->assoc_tags[stream->assoc_tag_count - 1] = assoc_tag;

	DSMCC_DEBUG("Added assoc_tag 0x%hx to stream with pid 0x%hx", assoc_tag, stream->pid);
}

static struct dsmcc_stream *find_stream(struct dsmcc_state *state, int stream_selector_type, uint16_t stream_selector, uint16_t default_pid, bool create_if_missing)
{
	struct dsmcc_stream *str;
	uint16_t pid;
	int ret;

	if (stream_selector_type == DSMCC_STREAM_SELECTOR_ASSOC_TAG)
	{
		str = find_stream_by_assoc_tag(state->streams, stream_selector);
		if (str)
        {
            //ugly extra check in case assoc_tag isn't unique
            //TODO in R7 case see stream specification, cause this check shouldn't be necessary
            if(str->pid == default_pid){
			    return str;
            }
            pid = default_pid;
        }

		if (state->callbacks.get_pid_for_assoc_tag)
		{
			ret = (*state->callbacks.get_pid_for_assoc_tag)(state->callbacks.get_pid_for_assoc_tag_arg, stream_selector, &pid);
			if (ret != 0)
			{
				DSMCC_DEBUG("PID/AssocTag Callback returned error %d, using initial carousel PID 0x%04x for assoc tag 0x%04x", ret, default_pid, stream_selector);
				pid = default_pid;
			}
		}
		else
		{
			DSMCC_DEBUG("No PID/AssocTag callback, using initial carousel PID 0x%04x for assoc tag 0x%04x", default_pid, stream_selector);
			pid = default_pid;
		}
	}
	else if (stream_selector_type == DSMCC_STREAM_SELECTOR_PID)
	{
		pid = stream_selector;
	}
	else
	{
		DSMCC_ERROR("Unknown stream selector type %d", stream_selector_type);
		return NULL;
	}

	str = dsmcc_stream_find_by_pid(state, pid);
	if (!str && create_if_missing)
	{
		str = calloc(1, sizeof(struct dsmcc_stream));
		str->pid = pid;
		str->next = state->streams;
		if (str->next)
			str->next->prev = str;
		state->streams = str;
	}

	if (str && stream_selector_type == DSMCC_STREAM_SELECTOR_ASSOC_TAG)
		dsmcc_stream_add_assoc_tag(str, stream_selector);

	return str;
}

struct dsmcc_stream *dsmcc_stream_queue_add(struct dsmcc_object_carousel *carousel, int stream_selector_type, uint16_t stream_selector, int type, uint32_t id)
{
	struct dsmcc_stream *str;
	struct dsmcc_queue_entry *entry;

	str = find_stream(carousel->state, stream_selector_type, stream_selector, carousel->requested_pid, 1);
	if (str)
	{
		if (dsmcc_stream_queue_find(str, type, id))
			return str;

		entry = calloc(1, sizeof(struct dsmcc_queue_entry));
		entry->stream = str;
		entry->carousel = carousel;
		entry->type = type;
		entry->id = id;

		entry->prev = NULL;
		entry->next = str->queue;
		if (entry->next)
			entry->next->prev = entry;
		str->queue = entry;
	}

	return str;
}

struct dsmcc_object_carousel *dsmcc_stream_queue_find(struct dsmcc_stream *stream, int type, uint32_t id)
{
	struct dsmcc_queue_entry *entry = stream->queue;

	while (entry)
	{
		if (entry->type == type)
		{
			if (type == DSMCC_QUEUE_ENTRY_DSI && entry->id == 0xffffffff) /* match all */
				break;
			else if ((entry->id & 0xfffe) == (id & 0xfffe)) /* match only bits 1-15 */
				break;
		}
		entry = entry->next;
	}

	if (entry)
		return entry->carousel;
	return NULL;
}

void dsmcc_stream_queue_remove(struct dsmcc_object_carousel *carousel, int type)
{
	struct dsmcc_stream *stream;
	struct dsmcc_queue_entry *entry, *next;

	stream = carousel->state->streams;
	while (stream)
	{
		entry = stream->queue;
		while (entry)
		{
			next = entry->next;
			if (entry->carousel == carousel && entry->type == type)
			{
				if (entry->prev)
					entry->prev->next = entry->next;
				else
					entry->stream->queue = entry->next;
				if (entry->next)
					entry->next->prev = entry->prev;
				free(entry);
			}
			entry = next;
		}
		stream = stream->next;
	}
}

static void free_queue_entries(struct dsmcc_queue_entry *entry)
{
	while (entry)
	{
		struct dsmcc_queue_entry *next = entry->next;
		free(entry);
		entry = next;
	}
}

static void free_all_streams(struct dsmcc_state *state)
{
	struct dsmcc_stream *stream = state->streams;
	while (stream)
	{
		struct dsmcc_stream *next = stream->next;
		if (stream->assoc_tags)
			free(stream->assoc_tags);
		free_queue_entries(stream->queue);
		free(stream);
		stream = next;
	}
}

void dsmcc_close(struct dsmcc_state *state)
{
	struct dsmcc_action *nextaction;
	int count;

	if (!state)
		return;

	DSMCC_DEBUG("Sending stop signal");
	pthread_mutex_lock(&state->mutex);
	state->stop = 1;
	pthread_cond_signal(&state->cond);
	pthread_mutex_unlock(&state->mutex);
	DSMCC_DEBUG("Waiting for thread to terminate");
	pthread_join(state->thread, NULL);

	count = 0;
	while (state->first_action)
	{
		nextaction = state->first_action->next;
		switch (state->first_action->type)
		{
			case DSMCC_ACTION_ADD_CAROUSEL:
				free(state->first_action->add_carousel.downloadpath);
				break;
			case DSMCC_ACTION_ADD_SECTION:
				free(state->first_action->add_section.section);
				break;
		}
		free(state->first_action);
		state->first_action = nextaction;
		count++;
	}
	DSMCC_DEBUG("Dropped %d action(s) buffered but not parsed", count);

	dsmcc_object_carousel_free_all(state, state->keep_cache);
	state->carousels = NULL;

	free_all_streams(state);
	state->streams = NULL;

	if (!state->keep_cache)
	{
		unlink(state->cachefile);
		rmdir(state->cachedir);
	}

	free(state->cachefile);
	free(state->cachedir);
	free(state);
}

void dsmcc_timeout_set(struct dsmcc_object_carousel *carousel, int type, uint16_t module_id, uint32_t delay_us)
{
	struct dsmcc_timeout *timeout;
	struct dsmcc_timeout *current, *next;
	struct timeval now, delay;

	dsmcc_timeout_remove(carousel, type, module_id);

	if (!delay_us)
		return;

	DSMCC_DEBUG("Adding timeout for carousel 0x%08x type %d module id 0x%04hhx delay %uus", carousel->cid, type, module_id, delay_us);

	timeout = malloc(sizeof(struct dsmcc_timeout));
	timeout->carousel = carousel;
	timeout->type = type;
	timeout->module_id = module_id;

	gettimeofday(&now, NULL);
	delay.tv_sec = delay_us / 1000000;
	delay.tv_usec = delay_us - delay.tv_sec * 1000000;
	timeradd(&now, &delay, &timeout->abstime);

	current = NULL;
	next = carousel->state->timeouts;
	while (next && timercmp(&next->abstime, &timeout->abstime, <))
	{
		current = next;
		next = current->next;
	}
	timeout->next = next;
	if (current)
		current->next = timeout;
	else
		carousel->state->timeouts = timeout;
}

void dsmcc_timeout_remove(struct dsmcc_object_carousel *carousel, int type, uint16_t module_id)
{
	struct dsmcc_timeout *current, *prev;

	current = carousel->state->timeouts;
	prev = NULL;
	while (current)
	{
		int match = current->carousel == carousel && current->type == type;
		if (type == DSMCC_TIMEOUT_MODULE || type == DSMCC_TIMEOUT_NEXTBLOCK)
			match &= current->module_id == module_id;
		if (match)
		{
			if (prev)
				prev->next = current->next;
			else
				carousel->state->timeouts = current->next;
			free(current);
			return;
		}
		prev = current;
		current = current->next;
	}
}

void dsmcc_timeout_remove_all(struct dsmcc_object_carousel *carousel)
{
	struct dsmcc_timeout *current, *prev, *next;

	current = carousel->state->timeouts;
	prev = NULL;
	while (current)
	{
		next = current->next;
		if (current->carousel == carousel)
		{
			free(current);
			if (prev)
				prev->next = next;
			else
				carousel->state->timeouts = next;
		}
		else
		{
			prev = current;
		}
		current = next;
	}
}

void dsmcc_add_section(struct dsmcc_state *state, uint16_t pid, uint8_t *data, int data_length)
{
	struct dsmcc_section *sect;
	struct dsmcc_action *action;

	sect = malloc(sizeof(struct dsmcc_section) + data_length);
	sect->pid = pid;
	sect->data = ((uint8_t *) sect) + sizeof(struct dsmcc_section);
	memcpy(sect->data, data, data_length);
	sect->length = data_length;

	action = calloc(1, sizeof(struct dsmcc_action));
	action->type = DSMCC_ACTION_ADD_SECTION;
	action->add_section.section = sect;
	buffer_action(state, action);
}

uint32_t dsmcc_queue_carousel2(struct dsmcc_state *state, int type, uint16_t pid, uint32_t transaction_id, const char *downloadpath, struct dsmcc_carousel_callbacks *callbacks)
{
	struct dsmcc_action *action;
	uint32_t queue_id;

	pthread_mutex_lock(&state->mutex);
	queue_id = state->next_queue_id++;
	pthread_mutex_unlock(&state->mutex);

	action = calloc(1, sizeof(struct dsmcc_action));
	action->type = DSMCC_ACTION_ADD_CAROUSEL;
	action->add_carousel.type = type;
	action->add_carousel.queue_id = queue_id;
	action->add_carousel.pid = pid;
	action->add_carousel.transaction_id = transaction_id;
	action->add_carousel.downloadpath = strdup(downloadpath);
	memcpy(&action->add_carousel.callbacks, callbacks, sizeof(struct dsmcc_carousel_callbacks));
	buffer_action(state, action);

	return queue_id;
}

uint32_t dsmcc_queue_carousel(struct dsmcc_state *state, uint16_t pid, uint32_t transaction_id, const char *downloadpath, struct dsmcc_carousel_callbacks *callbacks)
{
	return dsmcc_queue_carousel2(state, DSMCC_OBJECT_CAROUSEL, pid, transaction_id, downloadpath, callbacks);
}

void dsmcc_dequeue_carousel(struct dsmcc_state *state, uint32_t queue_id)
{
	struct dsmcc_action *action;
	action = calloc(1, sizeof(struct dsmcc_action));
	action->type = DSMCC_ACTION_REMOVE_CAROUSEL;
	action->remove_carousel.queue_id = queue_id;
	buffer_action(state, action);
}

void dsmcc_cache_clear(struct dsmcc_state *state)
{
	struct dsmcc_action *action;
	action = calloc(1, sizeof(struct dsmcc_action));
	action->type = DSMCC_ACTION_CACHE_CLEAR;
	buffer_action(state, action);
}

void dsmcc_cache_clear_carousel(struct dsmcc_state *state, uint32_t carousel_id)
{
	struct dsmcc_action *action;
	action = calloc(1, sizeof(struct dsmcc_action));
	action->type = DSMCC_ACTION_CACHE_CLEAR_CAROUSEL;
	action->cache_clear_carousel.carousel_id = carousel_id;
	buffer_action(state, action);
}
