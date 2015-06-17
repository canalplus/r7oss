#ifndef DSMCC_H
#define DSMCC_H

#include <stdint.h>
#include <stdbool.h>
#include <pthread.h>
#include <sys/time.h>
#include <dsmcc/dsmcc.h>
#include "dsmcc-debug.h"
#include "dsmcc-section.h"

enum
{
	DSMCC_QUEUE_ENTRY_DSI,
	DSMCC_QUEUE_ENTRY_DII,
	DSMCC_QUEUE_ENTRY_DDB
};

enum
{
	DSMCC_STREAM_SELECTOR_PID,
	DSMCC_STREAM_SELECTOR_ASSOC_TAG
};

struct dsmcc_stream
{
	uint16_t  pid;
	uint16_t  assoc_tag_count;
	uint16_t *assoc_tags;

	struct dsmcc_queue_entry *queue;

	struct dsmcc_stream *next, *prev;
};

enum
{
	DSMCC_TIMEOUT_DSI,
	DSMCC_TIMEOUT_DII,
	DSMCC_TIMEOUT_MODULE,
	DSMCC_TIMEOUT_NEXTBLOCK
};

struct dsmcc_timeout
{
	struct dsmcc_object_carousel *carousel;  /*< carousel this timeout applies to */
	int                           type;      /*< type of timeout */
	uint16_t                      module_id; /*< module ID, for type == DSMCC_TIMEOUT_MODULE or DSMCC_TIMEOUT_NEXTBLOCK */
	struct timeval                abstime;   /*< absolute time */

	struct dsmcc_timeout *next;
};

enum
{
	DSMCC_ACTION_ADD_CAROUSEL,
	DSMCC_ACTION_REMOVE_CAROUSEL,
	DSMCC_ACTION_ADD_SECTION,
	DSMCC_ACTION_CACHE_CLEAR,
	DSMCC_ACTION_CACHE_CLEAR_CAROUSEL,
};

struct dsmcc_action
{
	int type;

	union {
		struct {
			int type;
			uint32_t queue_id;
			uint16_t pid;
			uint32_t transaction_id;
			char *downloadpath;
			struct dsmcc_carousel_callbacks callbacks;
		} add_carousel;

		struct {
			uint32_t queue_id;
		} remove_carousel;

		struct {
			struct dsmcc_section *section;
		} add_section;

		struct {
			uint32_t carousel_id;
		} cache_clear_carousel;
	};

	struct dsmcc_action *next;
};

struct dsmcc_state
{
	char *cachedir;   /*< path of the directory where cached files will be stored */
	char *cachefile;  /*< name of the file where cached state will be stored */
	bool  keep_cache; /*< if the cache should be kept at exit */
	uint32_t next_queue_id;

	struct dsmcc_dvb_callbacks callbacks;    /*< Callbacks called to interract with DVB stack */

	struct dsmcc_stream          *streams;   /*< Linked list of streams, used to cache assoc_tag/pid mapping and to queue requests */
	struct dsmcc_object_carousel *carousels; /*< Linked list of carousels */

	pthread_t       thread;
	pthread_mutex_t mutex;
	pthread_cond_t  cond;
	int             stop;

	struct dsmcc_action *first_action, *last_action;
	struct dsmcc_timeout *timeouts;
};

struct dsmcc_stream *dsmcc_stream_find_by_pid(struct dsmcc_state *state, uint16_t pid);

struct dsmcc_object_carousel *dsmcc_stream_queue_find(struct dsmcc_stream *stream, int type, uint32_t id);
struct dsmcc_stream *dsmcc_stream_queue_add(struct dsmcc_object_carousel *carousel, int stream_selector_type, uint16_t stream_selector, int type, uint32_t id);
void dsmcc_stream_queue_remove(struct dsmcc_object_carousel *carousel, int type);

void dsmcc_timeout_set(struct dsmcc_object_carousel *carousel, int type, uint16_t module_id, uint32_t delay_us);
void dsmcc_timeout_remove(struct dsmcc_object_carousel *carousel, int type, uint16_t module_id);
void dsmcc_timeout_remove_all(struct dsmcc_object_carousel *carousel);

#endif
