#ifndef DSMCC_CAROUSEL
#define DSMCC_CAROUSEL

#include <stdint.h>
#include <stdio.h>


struct dsmcc_object_carousel
{
	struct dsmcc_state *state;
	uint32_t            cid;
	int                 type;
	int                 status;

	uint16_t requested_pid;
	uint32_t requested_transaction_id;

	uint32_t dsi_transaction_id;
	uint32_t dii_transaction_id;

	struct dsmcc_module     *modules;
	struct dsmcc_file_cache *filecaches;
	struct dsmcc_group_list *group_list;

	struct dsmcc_object_carousel *next;
};

void dsmcc_object_carousel_queue_add(struct dsmcc_state *state, uint32_t queue_id, int type, uint16_t pid, uint32_t transaction_id,
		const char *downloadpath, struct dsmcc_carousel_callbacks *callbacks);
void dsmcc_object_carousel_queue_remove(struct dsmcc_state *state, uint32_t queue_id);
bool dsmcc_object_carousel_load_all(FILE *file, struct dsmcc_state *state);
bool dsmcc_object_carousel_save_all(FILE *file, struct dsmcc_state *state);
void dsmcc_object_carousel_free(struct dsmcc_object_carousel *carousel, bool keep_cache);
void dsmcc_object_carousel_free_all(struct dsmcc_state *state, bool keep_cache);
void dsmcc_object_carousel_set_status(struct dsmcc_object_carousel *carousel, int newstatus);

#endif
