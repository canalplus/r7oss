#ifndef DSMCC_CACHE_FILE_H
#define DSMCC_CACHE_FILE_H

#include <stdint.h>

#include "dsmcc-cache.h"

/* from dsmcc-carousel.h */
struct dsmcc_object_carousel;

struct dsmcc_file_cache;

struct dsmcc_file_cache *dsmcc_filecache_add(struct dsmcc_object_carousel *carousel, uint32_t queue_id, const char *downloadpath, struct dsmcc_carousel_callbacks *callbacks);
void dsmcc_filecache_remove(struct dsmcc_file_cache *filecache);
void dsmcc_filecache_remove_all(struct dsmcc_object_carousel *carousel);

struct dsmcc_file_cache *dsmcc_filecache_find(struct dsmcc_object_carousel *carousel, uint32_t queue_id);
struct dsmcc_file_cache *dsmcc_filecache_next(struct dsmcc_file_cache *filecache);

void dsmcc_filecache_clear_all(struct dsmcc_object_carousel *carousel);

void dsmcc_filecache_cache_dir(struct dsmcc_file_cache *filecache, struct dsmcc_object_id *parent_id, struct dsmcc_object_id *id, const char *name);
void dsmcc_filecache_cache_file(struct dsmcc_file_cache *filecache, struct dsmcc_object_id *parent_id, struct dsmcc_object_id *id, const char *name);
void dsmcc_filecache_cache_data(struct dsmcc_file_cache *filecache, struct dsmcc_object_id *id, const char *data_file, uint32_t data_size);

void dsmcc_filecache_notify_progression(struct dsmcc_object_carousel *carousel, struct dsmcc_file_cache *filecache, uint32_t downloaded, uint32_t total);
void dsmcc_filecache_notify_status(struct dsmcc_object_carousel *carousel, struct dsmcc_file_cache *filecache);

/* /!\ skip cache and write a file directly */
int dsmcc_filecache_write_file(struct dsmcc_file_cache *filecache, const char *file_path, const char *data_file, int data_size);

#endif /* DSMCC_CACHE_FILE_H */
