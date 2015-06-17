#define _GNU_SOURCE

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "dsmcc.h"
#include "dsmcc-cache-module.h"
#include "dsmcc-cache-file.h"
#include "dsmcc-carousel.h"
#include "dsmcc-debug.h"
#include "dsmcc-util.h"
#include "dsmcc-compress.h"
#include "dsmcc-biop-message.h"
#include "dsmcc-gii.h"

/* list of directory entries */
struct dsmcc_module_dentry_list
{
	struct dsmcc_module_dentry *first, *last;
};

/* directory entry in a completed module */
struct dsmcc_module_dentry
{
	bool                   dir;
	struct dsmcc_object_id id;

	/* not for top-level dentries */
	char *name;

	/* only for files */
	char    *data_file;
	uint32_t data_size;

	/* only for dirs */
	struct dsmcc_module_dentry_list dentries;

	struct dsmcc_module_dentry *next;
};


/* data for module whose download is in progress */
struct dsmcc_module_partial
{
	uint32_t block_size;

	bool     compressed;
	uint8_t  compress_method;
	uint32_t uncompressed_size;

	char    *data_file;
	uint32_t block_count;
	uint32_t blockmap_size;
	uint8_t *blockmap;
	uint32_t downloaded_bytes;

	uint32_t block_timeout;
};

/* data for completed module */
struct dsmcc_module_complete
{
	struct dsmcc_module_dentry     *gateway;
	struct dsmcc_module_dentry_list dentries;
};

/* module state */
enum
{
	DSMCC_MODULE_STATE_PARTIAL = 0,
	DSMCC_MODULE_STATE_COMPLETE,
	DSMCC_MODULE_STATE_INVALID
};

/* module */
struct dsmcc_module
{
	struct dsmcc_module_id   id;
	int                      state;
	uint32_t                 module_size;

	union
	{
		struct dsmcc_module_partial partial;
		struct dsmcc_module_complete complete;
	} data;

	struct dsmcc_module *next, *prev;
};

static void free_dentries(struct dsmcc_module_dentry_list *list, bool keep_cache)
{
	struct dsmcc_module_dentry *dentry, *next;

	dentry = list->first;
	while (dentry)
	{
		if (dentry->name)
			free(dentry->name);
		if (dentry->dir)
			free_dentries(&dentry->dentries, keep_cache);
		else
		{
			if (dentry->data_file)
			{
				if (!keep_cache)
					unlink(dentry->data_file);
				free(dentry->data_file);
			}
		}
		next = dentry->next;
		free(dentry);
		dentry = next;
	}

	list->first = NULL;
	list->last = NULL;
}

static void free_module_data(struct dsmcc_module *module, bool keep_cache)
{
	switch (module->state)
	{
		case DSMCC_MODULE_STATE_PARTIAL:
			if (module->data.partial.blockmap)
			{
				free(module->data.partial.blockmap);
				module->data.partial.blockmap = NULL;
				module->data.partial.blockmap_size = 0;
			}

			if (module->data.partial.data_file)
			{
				if (!keep_cache)
					unlink(module->data.partial.data_file);
				free(module->data.partial.data_file);
				module->data.partial.data_file = NULL;
			}

			module->data.partial.block_count = 0;
			module->data.partial.downloaded_bytes = 0;
			break;
		case DSMCC_MODULE_STATE_COMPLETE:
			free_dentries(&module->data.complete.dentries, keep_cache);
			break;
	}
	module->state = DSMCC_MODULE_STATE_INVALID;
}

static void free_module(struct dsmcc_object_carousel *carousel, struct dsmcc_module *module, bool keep_cache)
{
	free_module_data(module, keep_cache);

	if (module->prev)
	{
		module->prev->next = module->next;
		if (module->next)
			module->next->prev = module->prev;
	}
	else
	{
		carousel->modules = module->next;
		if (module->next)
			module->next->prev = NULL;
	}

	free(module);
}

void dsmcc_cache_free_all_modules(struct dsmcc_object_carousel *carousel, bool keep_cache)
{
	while (carousel->modules)
		free_module(carousel, carousel->modules, keep_cache);
}

static struct dsmcc_module_dentry *add_dentry(struct dsmcc_module_dentry_list *list, bool dir, struct dsmcc_object_id *id, char *name)
{
	struct dsmcc_module_dentry *dentry;

	dentry = calloc(1, sizeof(struct dsmcc_module_dentry));
	dentry->dir = dir;
	memcpy(&dentry->id, id, sizeof(struct dsmcc_object_id));
	if (name)
		dentry->name = name;

	if (list->last)
	{
		list->last->next = dentry;
		list->last = dentry;
	}
	else
	{
		list->first = dentry;
		list->last = dentry;
	}

	return dentry;
}

static void add_dir_dentry(struct dsmcc_object_carousel *carousel, struct dsmcc_module_complete *module_data, struct biop_msg_dir *msg)
{
	struct dsmcc_module_dentry *dentry;
	struct biop_msg_dentry *d;

	dentry = add_dentry(&module_data->dentries, 1, &msg->id, NULL);
	if (msg->gateway)
		module_data->gateway = dentry;

	d = msg->first_dentry;
	while (d)
	{
		if (dsmcc_log_enabled(DSMCC_LOG_DEBUG))
		{
			struct dsmcc_module *module;
			for (module = carousel->modules; module; module = module->next)
				if (module->id.module_id == d->id.module_id)
					break;
			if (!module)
				DSMCC_DEBUG("Directory entry %s points to a non-existing module 0x%04x", d->name, d->id.module_id);
		}
		add_dentry(&dentry->dentries, d->dir, &d->id, strdup(d->name));
		d = d->next;
	}
}

static void add_file_dentry(struct dsmcc_module_complete *module_data, const char *fileprefix, struct biop_msg_file *msg)
{
	char *fn;
	struct dsmcc_module_dentry *dentry;

	fn = malloc(strlen(fileprefix) + 10);
	switch (msg->id.key_mask)
	{
		case 0xff:
			sprintf(fn, "%s-%02x", fileprefix, msg->id.key & msg->id.key_mask);
			break;
		case 0xffff:
			sprintf(fn, "%s-%04x", fileprefix, msg->id.key & msg->id.key_mask);
			break;
		case 0xffffff:
			sprintf(fn, "%s-%06x", fileprefix, msg->id.key & msg->id.key_mask);
			break;
		default:
			sprintf(fn, "%s-%08x", fileprefix, msg->id.key & msg->id.key_mask);
			break;
	}

	if (!dsmcc_file_copy(fn, msg->data_file, msg->data_offset, msg->data_length))
		return;

	dentry = add_dentry(&module_data->dentries, 0, &msg->id, NULL);
	dentry->data_file = fn;
	dentry->data_size = msg->data_length;
}

static void write_module(struct dsmcc_file_cache *filecache, struct dsmcc_module *module)
{
	char *filename;
	if(module->state != DSMCC_MODULE_STATE_COMPLETE)
		return;

	struct dsmcc_module_dentry *dentry = module->data.complete.dentries.first;
	if(dentry)
	{
		if(asprintf(&filename, "%u-%u-%hu.bin", module->id.download_id, module->id.dii_transaction_id, module->id.module_id) < 0)
			fprintf(stderr, "argh!\n");
		dsmcc_filecache_write_file(filecache, filename, dentry->data_file, dentry->data_size);
		free(filename);
	}

}

static void update_filecache(struct dsmcc_file_cache *filecache, struct dsmcc_module *module)
{
	struct dsmcc_module_dentry *dentry, *dentry2;

	if (module->state != DSMCC_MODULE_STATE_COMPLETE)
		return;

	if (module->data.complete.gateway)
		dsmcc_filecache_cache_dir(filecache, NULL, &module->data.complete.gateway->id, NULL);

	for (dentry = module->data.complete.dentries.first; dentry; dentry = dentry->next)
	{
		if (dentry->dir)
		{
			for (dentry2 = dentry->dentries.first; dentry2; dentry2 = dentry2->next)
			{
				if (dentry2->dir)
					dsmcc_filecache_cache_dir(filecache, &dentry->id, &dentry2->id, dentry2->name);
				else
					dsmcc_filecache_cache_file(filecache, &dentry->id, &dentry2->id, dentry2->name);
			}
		}
		else
		{
			dsmcc_filecache_cache_data(filecache, &dentry->id, dentry->data_file, dentry->data_size);
		}
	}
}

static void update_filecaches(struct dsmcc_object_carousel *carousel, struct dsmcc_module *module)
{
	struct dsmcc_file_cache *filecache;
	for (filecache = carousel->filecaches; filecache; filecache = dsmcc_filecache_next(filecache))
	{
		if(carousel->type == DSMCC_OBJECT_CAROUSEL)
			update_filecache(filecache, module);
		else
			write_module(filecache, module);
	}
}

static void update_carousel_completion(struct dsmcc_object_carousel *carousel, struct dsmcc_file_cache *filecache)
{
	struct dsmcc_module *module;
	uint32_t downloaded, total;
	int complete;

	if ((carousel->dsi_transaction_id != 0xFFFFFFFF) && (carousel->type == DSMCC_OBJECT_CAROUSEL ?
		(carousel->dii_transaction_id != 0xFFFFFFFF) : (unsigned)carousel->group_list))
	{
		complete = 1;
		downloaded = total = 0;
		for (module = carousel->modules; module; module = module->next)
		{
			total += module->module_size;
			switch (module->state)
			{
				case DSMCC_MODULE_STATE_PARTIAL:
					downloaded += module->data.partial.downloaded_bytes;
					complete = 0;
					break;
				case DSMCC_MODULE_STATE_COMPLETE:
					downloaded += module->module_size;
					break;
				default:
					complete = 0;
					break;
			}
		}

		if (complete)
			dsmcc_object_carousel_set_status(carousel, DSMCC_STATUS_DONE);

		dsmcc_filecache_notify_progression(carousel, filecache, downloaded, total);
	}
	dsmcc_filecache_notify_status(carousel, filecache);
}

void dsmcc_cache_init_new_filecache(struct dsmcc_object_carousel *carousel, struct dsmcc_file_cache *filecache)
{
	struct dsmcc_module *module;

	for (module = carousel->modules; module; module = module->next)
		if (module->state == DSMCC_MODULE_STATE_COMPLETE)
			update_filecache(filecache, module);

	update_carousel_completion(carousel, filecache);
}

static void process_module(struct dsmcc_object_carousel *carousel, struct dsmcc_module *module)
{
	int ret;
	char *data_file;
	uint32_t size;
	struct biop_msg *messages = NULL, *msg = NULL;
	struct biop_msg_file allmodfile;

	if (module->state != DSMCC_MODULE_STATE_PARTIAL)
		return;

	if (module->data.partial.downloaded_bytes < module->module_size)
		return;

	DSMCC_DEBUG("Processing module 0x%04hx version 0x%02hhx in carousel 0x%08x (data file is %s)",
			module->id.module_id, module->id.module_version, carousel->cid, module->data.partial.data_file);

	if (module->data.partial.compressed)
	{
		DSMCC_DEBUG("Processing compressed module data");
		if (!dsmcc_inflate_file(module->data.partial.data_file))
		{
			DSMCC_ERROR("Error while processing compressed module");
			return;
		}
		size = module->data.partial.uncompressed_size;
	}
	else
	{
		DSMCC_DEBUG("Processing uncompressed module data");
		size = module->module_size;
	}

	if(carousel->type == DSMCC_OBJECT_CAROUSEL)
	{
		ret = dsmcc_biop_msg_parse_data(&messages, &module->id, module->data.partial.data_file, size);
		if (ret < 0)
		{
			DSMCC_ERROR("Error while parsing module 0x%04hx", module->id.module_id);
			free_module_data(module, 0);
			return;
		}
	}

	data_file = strdup(module->data.partial.data_file);
	free_module_data(module, 1);
	module->state = DSMCC_MODULE_STATE_COMPLETE;
	memset(&module->data.complete, 0, sizeof(struct dsmcc_module_complete));

	/* remove module timeouts */
	dsmcc_timeout_remove(carousel, DSMCC_TIMEOUT_MODULE, module->id.module_id);
	dsmcc_timeout_remove(carousel, DSMCC_TIMEOUT_NEXTBLOCK, module->id.module_id);

	if(carousel->type == DSMCC_OBJECT_CAROUSEL)
	{
		msg = messages;
		while (msg)
		{
			switch (msg->type)
			{
				case BIOP_MSG_DIR:
					add_dir_dentry(carousel, &module->data.complete, &msg->msg.dir);
					break;
				case BIOP_MSG_FILE:
					add_file_dentry(&module->data.complete, data_file, &msg->msg.file);
					break;
			}
			msg = msg->next;
		}
		dsmcc_biop_msg_free_all(messages);
	}
	else
	{
		allmodfile.id.module_id = module->id.module_id;
		allmodfile.id.key = module->id.module_id;
		allmodfile.id.key_mask = 0xFFFF;
		allmodfile.data_file = data_file; //useless ?
		allmodfile.data_offset = 0;
		allmodfile.data_length = size;
		add_file_dentry(&module->data.complete, data_file, &allmodfile);
	}

	unlink(data_file);
	free(data_file);
}

void dsmcc_cache_remove_unneeded_modules(struct dsmcc_object_carousel *carousel, struct dsmcc_module_id *modules_id, int number_modules)
{
	struct dsmcc_module *module, *next;
	int i, ok;

	for (module = carousel->modules; module; module = next)
	{
		ok = 0;
		for (i = 0; i < number_modules; i++)
		{
			if (module->id.module_id == modules_id[i].module_id)
			{
				ok = 1;
				break;
			}
		}
		next = module->next;
		if (!ok)
		{
			DSMCC_DEBUG("Removing Module 0x%04hx Version 0x%02hhx", module->id.module_id, module->id.module_version);
			free_module(carousel, module, 0);
		}
	}
}

void dsmcc_cache_remove_unneeded_modules_by_group(struct dsmcc_object_carousel *carousel, struct dsmcc_group_list *groups)
{
	struct dsmcc_module *module, *next;
	struct dsmcc_group_list *grp;
	int ok;

	for (module = carousel->modules; module; module = next)
	{
		ok = 0;
		for (grp = groups; grp; grp = grp->next)
		{
			if (module->id.dii_transaction_id == grp->id)
			{
				ok = 1;
				break;
			}

		}
		next = module->next;
		if (!ok)
		{
			DSMCC_DEBUG("Removing Module 0x%04hx Version 0x%02hhx", module->id.module_id, module->id.module_version);
			free_module(carousel, module, 0);
		}
	}
}

/**
  * Add module to cache list if no module with same id or version has changed
  */
bool dsmcc_cache_add_module_info(struct dsmcc_object_carousel *carousel, struct dsmcc_module_id *module_id, struct dsmcc_module_info *module_info)
{
	struct dsmcc_module *module;

	for (module = carousel->modules; module; module = module->next)
	{
		if (module->id.module_id == module_id->module_id)
		{
			if (module->id.module_version == module_id->module_version && module->state != DSMCC_MODULE_STATE_INVALID)
			{
				/* Already know this version */
				DSMCC_DEBUG("Up-to-Date Module 0x%04hx Version 0x%02hhx",
						module_id->module_id, module_id->module_version);
				update_filecaches(carousel, module);
				update_carousel_completion(carousel, NULL);
				return module->state == DSMCC_MODULE_STATE_COMPLETE;
			}
			else
			{
				/* New version, drop old data */
				DSMCC_DEBUG("Updating Module 0x%04hx Version 0x%02hhx -> 0x%02hhx",
						module_id->module_id, module->id.module_version, module_id->module_version);
				free_module_data(module, 0);
				break;
			}
		}
	}

	DSMCC_DEBUG("Saving info for Module 0x%04hx Version 0x%02hhx", module_id->module_id, module_id->module_version);

	if (!module)
		module = calloc(1, sizeof(struct dsmcc_module));
	module->state = DSMCC_MODULE_STATE_PARTIAL;
	memcpy(&module->id, module_id, sizeof(struct dsmcc_module_id));
	module->module_size = module_info->module_size;
	memset(&module->data.partial, 0, sizeof(struct dsmcc_module_partial));
	module->data.partial.block_timeout = module_info->block_timeout;
	module->data.partial.compressed = module_info->compressed;
	module->data.partial.compress_method = module_info->compress_method;
	module->data.partial.uncompressed_size = module_info->uncompressed_size;
	module->data.partial.block_size = module_info->block_size;
	module->data.partial.downloaded_bytes = 0;
	module->data.partial.block_count = module->module_size / module->data.partial.block_size;
	if (module->module_size - module->data.partial.block_count * module->data.partial.block_size > 0)
		module->data.partial.block_count++;
	module->data.partial.blockmap_size = (module->data.partial.block_count + 7) >> 3;
	module->data.partial.blockmap = calloc(1, module->data.partial.blockmap_size);

	module->data.partial.data_file = malloc(strlen(carousel->state->cachedir) + 18);
	sprintf(module->data.partial.data_file, "%s/%08x-%04hx-%02hhx", carousel->state->cachedir, carousel->cid, module->id.module_id, module->id.module_version);
	unlink(module->data.partial.data_file);

	module->next = carousel->modules;
	if (module->next)
		module->next->prev = module;
	carousel->modules = module;

	return 0;
}

#ifdef DEBUG
static const char *get_module_state_str(int state)
{
	switch (state)
	{
		case DSMCC_MODULE_STATE_PARTIAL:
			return "DSMCC_MODULE_STATE_PARTIAL";
		case DSMCC_MODULE_STATE_COMPLETE:
			return "DSMCC_MODULE_STATE_COMPLETE";
		case DSMCC_MODULE_STATE_INVALID:
			return "DSMCC_MODULE_STATE_INVALID";
		default:
			return "UNKNOWN";
	}
}
#endif

void dsmcc_cache_save_module_data(struct dsmcc_object_carousel *carousel, struct dsmcc_module_id *module_id, uint16_t block_number, uint8_t *data, int length)
{
	struct dsmcc_module *module = NULL;

	if (length < 0)
	{
		DSMCC_ERROR("Data buffer overflow");
		return;
	}

	for (module = carousel->modules; module; module = module->next)
	{
		if (module->id.download_id == module_id->download_id
				&& module->id.module_id == module_id->module_id
				&& module->id.module_version == module_id->module_version)
		{
			DSMCC_DEBUG("Found Module 0x%04hx Version 0x%02hhx Download ID 0x%08x",
					module->id.module_id, module->id.module_version, module->id.download_id);
			break;
		}
	}

	if (!module)
	{
		DSMCC_DEBUG("Cannot find Module 0x%04hx Version 0x%02hhx Download ID 0x%08x",
				module_id->module_id, module_id->module_version, module_id->download_id);
		return;
	}

	if (module->state == DSMCC_MODULE_STATE_PARTIAL)
	{
		/* Check that DDB size is equal to module block size (or smaller for last block) */
		if (((uint32_t)length) > module->data.partial.block_size)
		{
			DSMCC_WARN("DDB block length is bigger than module block size (%d > %u), dropping excess data", length, module->data.partial.block_size);
			length = module->data.partial.block_size;
		}
		else if (((uint32_t) length) != module->data.partial.block_size && block_number != module->data.partial.block_count - 1)
		{
			DSMCC_ERROR("DDB block length is smaller than module block size (%d < %u)", length, module->data.partial.block_size);
			return;
		}

		/* Check if we have this block already or not. If not save it to disk */
		if ((module->data.partial.blockmap[block_number >> 3] & (1 << (block_number & 7))) == 0)
		{
			if (!dsmcc_file_write_block(module->data.partial.data_file, block_number * module->data.partial.block_size, data, length))
			{
				DSMCC_ERROR("Error while writing block %hu of module 0x%hx", block_number, module->id.module_id);
				return;
			}
			module->data.partial.downloaded_bytes += length;
			module->data.partial.blockmap[block_number >> 3] |= (1 << (block_number & 7));

			dsmcc_timeout_set(carousel, DSMCC_TIMEOUT_NEXTBLOCK, module->id.module_id, module->data.partial.block_timeout);
		}

		DSMCC_DEBUG("Module 0x%04hx Downloaded %d/%d", module->id.module_id, module->data.partial.downloaded_bytes, module->module_size);

		/* If we have all blocks for this module, process it */
		if (module->data.partial.downloaded_bytes >= module->module_size)
		{
			process_module(carousel, module);
			update_filecaches(carousel, module);
		}

		update_carousel_completion(carousel, NULL);
	}
	else
	{
		DSMCC_DEBUG("Skipping data block for module 0x%04hx (%s)", module->id.module_id, get_module_state_str(module->state));
	}
}

static bool load_dentries(FILE *f, struct dsmcc_module_dentry **gateway, struct dsmcc_module_dentry_list *dentries)
{
	uint32_t tmp, isgateway;
	struct dsmcc_module_dentry *dentry;
	bool dir;
	struct dsmcc_object_id id;
	char *name;

	while (1)
	{
		if (!fread(&tmp, sizeof(uint32_t), 1, f))
			return 0;
		if (tmp)
			break;
		if (!fread(&isgateway, sizeof(uint32_t), 1, f))
			return 0;
		if (!fread(&dir, sizeof(bool), 1, f))
			return 0;
		if (!fread(&id.module_id, sizeof(uint16_t), 1, f))
			return 0;
		if (!fread(&id.key, sizeof(uint32_t), 1, f))
			return 0;
		if (!fread(&id.key_mask, sizeof(uint32_t), 1, f))
			return 0;
		if (!fread(&tmp, sizeof(uint32_t), 1, f))
			return 0;
		if (tmp)
		{
			name = malloc(tmp);
			if (!fread(name, tmp, 1, f))
			{
				free(name);
				return 0;
			}
		}
		else
			name = NULL;
		dentry = add_dentry(dentries, dir, &id, name);
		if (isgateway && gateway)
			*gateway = dentry;
		if (dentry->dir)
		{
			if (!load_dentries(f, NULL, &dentry->dentries))
				return 0;
		}
		else
		{
			if (!fread(&tmp, sizeof(uint32_t), 1, f))
				return 0;
			if (tmp)
			{
				dentry->data_file = malloc(tmp);
				if (!fread(dentry->data_file, tmp, 1, f))
					return 0;
			}
			if (!fread(&dentry->data_size, sizeof(uint32_t), 1, f))
				return 0;
		}
	}

	return 1;
}

bool dsmcc_cache_load_modules(FILE *f, struct dsmcc_object_carousel *carousel)
{
	struct dsmcc_module *module = NULL, *lastmod = NULL;
	uint32_t tmp;

	while (1)
	{
		if (!fread(&tmp, sizeof(uint32_t), 1, f))
			goto error;
		if (tmp)
			break;
		module = calloc(1, sizeof(struct dsmcc_module));
		module->state = DSMCC_MODULE_STATE_INVALID;
		if (!fread(&module->id.download_id, sizeof(uint32_t), 1, f))
			goto error;
		if (!fread(&module->id.module_id, sizeof(uint16_t), 1, f))
			goto error;
		if (!fread(&module->id.module_version, sizeof(uint8_t), 1, f))
			goto error;
		if (!fread(&module->state, sizeof(int), 1, f))
			goto error;
		if (!fread(&module->module_size, sizeof(uint32_t), 1, f))
			goto error;
		switch (module->state)
		{
			case DSMCC_MODULE_STATE_PARTIAL:
				if (!fread(&module->data.partial.block_size, sizeof(uint32_t), 1, f))
					goto error;
				if (!fread(&module->data.partial.compressed, sizeof(bool), 1, f))
					goto error;
				if (!fread(&module->data.partial.compress_method, sizeof(uint8_t), 1, f))
					goto error;
				if (!fread(&module->data.partial.uncompressed_size, sizeof(uint32_t), 1, f))
					goto error;
				if (!fread(&tmp, sizeof(uint32_t), 1, f))
					goto error;
				module->data.partial.data_file = malloc(tmp);
				if (!fread(module->data.partial.data_file, tmp, 1, f))
					goto error;
				if (!fread(&module->data.partial.block_count, sizeof(uint32_t), 1, f))
					goto error;
				if (!fread(&module->data.partial.blockmap_size, sizeof(uint32_t), 1, f))
					goto error;
				module->data.partial.blockmap = malloc(module->data.partial.blockmap_size);
				if (!fread(module->data.partial.blockmap, module->data.partial.blockmap_size, 1, f))
					goto error;
				if (!fread(&module->data.partial.downloaded_bytes, sizeof(uint32_t), 1, f))
					goto error;
				break;
			case DSMCC_MODULE_STATE_COMPLETE:
				if (!load_dentries(f, &module->data.complete.gateway, &module->data.complete.dentries))
					goto error;
				break;
		}
		if (carousel->modules)
			lastmod->next = module;
		else
			carousel->modules = module;
		lastmod = module;
		module = NULL;
	}

	/* TODO check that module data files are present and have the correct size */
	return 1;
error:
	dsmcc_cache_free_all_modules(carousel, 0);
	if (module)
	{
		free_module_data(module, 0);
		free(module);
	}
	return 0;
}

static bool save_dentries(FILE *f, struct dsmcc_module_dentry_list *dentries, struct dsmcc_module_dentry *gateway)
{
	uint32_t tmp;
	struct dsmcc_module_dentry *dentry;

	for (dentry = dentries->first; dentry; dentry = dentry->next)
	{
		tmp = 0;
		if (!fwrite(&tmp, sizeof(uint32_t), 1, f))
			goto error;
		tmp = dentry == gateway ? 1 : 0;
		if (!fwrite(&tmp, sizeof(uint32_t), 1, f))
			goto error;
		if (!fwrite(&dentry->dir, sizeof(bool), 1, f))
			goto error;
		if (!fwrite(&dentry->id.module_id, sizeof(uint16_t), 1, f))
			goto error;
		if (!fwrite(&dentry->id.key, sizeof(uint32_t), 1, f))
			goto error;
		if (!fwrite(&dentry->id.key_mask, sizeof(uint32_t), 1, f))
			goto error;
		tmp = dentry->name ? strlen(dentry->name) + 1 : 0;
		if (!fwrite(&tmp, sizeof(uint32_t), 1, f))
			goto error;
		if (tmp)
		{
			if (!fwrite(dentry->name, tmp, 1, f))
				goto error;
		}
		if (dentry->dir)
		{
			save_dentries(f, &dentry->dentries, NULL);
		}
		else
		{
			tmp = dentry->data_file ? strlen(dentry->data_file) + 1 : 0;
			if (!fwrite(&tmp, sizeof(uint32_t), 1, f))
				goto error;
			if (tmp)
				if (!fwrite(dentry->data_file, tmp, 1, f))
					goto error;
			if (!fwrite(&dentry->data_size, sizeof(uint32_t), 1, f))
				goto error;
		}
	}
	tmp = 1;
	if (!fwrite(&tmp, sizeof(uint32_t), 1, f))
		goto error;

	return 1;
error:
	return 0;
}

bool dsmcc_cache_save_modules(FILE *f, struct dsmcc_object_carousel *carousel)
{
	struct dsmcc_module *module;
	uint32_t tmp;

	module = carousel->modules;
	while (module)
	{
		tmp = 0;
		if (!fwrite(&tmp, sizeof(uint32_t), 1, f))
			goto error;
		if (!fwrite(&module->id.download_id, sizeof(uint32_t), 1, f))
			goto error;
		if (!fwrite(&module->id.module_id, sizeof(uint16_t), 1, f))
			goto error;
		if (!fwrite(&module->id.module_version, sizeof(uint8_t), 1, f))
			goto error;
		if (!fwrite(&module->state, sizeof(int), 1, f))
			goto error;
		if (!fwrite(&module->module_size, sizeof(uint32_t), 1, f))
			goto error;
		switch (module->state)
		{
			case DSMCC_MODULE_STATE_PARTIAL:
				if (!fwrite(&module->data.partial.block_size, sizeof(uint32_t), 1, f))
					goto error;
				if (!fwrite(&module->data.partial.compressed, sizeof(bool), 1, f))
					goto error;
				if (!fwrite(&module->data.partial.compress_method, sizeof(uint8_t), 1, f))
					goto error;
				if (!fwrite(&module->data.partial.uncompressed_size, sizeof(uint32_t), 1, f))
					goto error;
				tmp = strlen(module->data.partial.data_file) + 1;
				if (!fwrite(&tmp, sizeof(uint32_t), 1, f))
					goto error;
				if (!fwrite(module->data.partial.data_file, tmp, 1, f))
					goto error;
				if (!fwrite(&module->data.partial.block_count, sizeof(uint32_t), 1, f))
					goto error;
				if (!fwrite(&module->data.partial.blockmap_size, sizeof(uint32_t), 1, f))
					goto error;
				if (!fwrite(module->data.partial.blockmap, module->data.partial.blockmap_size, 1, f))
					goto error;
				if (!fwrite(&module->data.partial.downloaded_bytes, sizeof(uint32_t), 1, f))
					goto error;
				break;
			case DSMCC_MODULE_STATE_COMPLETE:
				if (!save_dentries(f, &module->data.complete.dentries, module->data.complete.gateway))
					goto error;
				break;
		}
		module = module->next;
	}
	tmp = 1;
	if (!fwrite(&tmp, sizeof(uint32_t), 1, f))
		goto error;

	return 1;
error:
	return 0;
}

