#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "dsmcc.h"
#include "dsmcc-cache-file.h"
#include "dsmcc-cache-module.h"
#include "dsmcc-carousel.h"
#include "dsmcc-debug.h"
#include "dsmcc-util.h"
#include "dsmcc-biop-message.h"
#include "dsmcc-biop-module.h"

struct dsmcc_cached_dir
{
	struct dsmcc_object_id id;

	char *name;
	char *path;
	bool  written;

	struct dsmcc_object_id   parent_id;
	struct dsmcc_cached_dir *parent;

	struct dsmcc_cached_file *files;
	struct dsmcc_cached_dir  *subdirs;

	struct dsmcc_cached_dir *next, *prev;
};

struct dsmcc_cached_file
{
	struct dsmcc_object_id id;

	char *name;
	char *path;
	bool  written;

	struct dsmcc_object_id   parent_id;
	struct dsmcc_cached_dir *parent;

	char *data_file;
	int   data_size;

	struct dsmcc_cached_file *next, *prev;
};

struct dsmcc_file_cache
{
	struct dsmcc_object_carousel   *carousel;
	uint32_t                        queue_id;
	char                           *downloadpath;
	struct dsmcc_carousel_callbacks callbacks;
	int                             last_carousel_status;

	struct dsmcc_cached_dir  *gateway;
	struct dsmcc_cached_dir  *orphan_dirs;
	struct dsmcc_cached_file *orphan_files;
	struct dsmcc_cached_file *nameless_files;

	struct dsmcc_file_cache *prev, *next;
};

static int idcmp(struct dsmcc_object_id *id1, struct dsmcc_object_id *id2)
{
	if (id1->module_id != id2->module_id)
		return 0;

	if (id1->key_mask != id2->key_mask)
		return 0;

	return (id1->key & id1->key_mask) == (id2->key & id2->key_mask);
}

struct dsmcc_file_cache *dsmcc_filecache_add(struct dsmcc_object_carousel *carousel, uint32_t queue_id, const char *downloadpath, struct dsmcc_carousel_callbacks *callbacks)
{
	struct dsmcc_file_cache *filecache;

	filecache = calloc(1, sizeof(struct dsmcc_file_cache));
	filecache->carousel = carousel;
	filecache->queue_id = queue_id;
	filecache->last_carousel_status = -1;

	filecache->downloadpath = strdup(downloadpath);
	if (downloadpath[strlen(downloadpath) - 1] == '/')
		filecache->downloadpath[strlen(downloadpath) - 1] = '\0';

	memcpy(&filecache->callbacks, callbacks, sizeof(struct dsmcc_carousel_callbacks));

	filecache->prev = NULL;
	filecache->next = carousel->filecaches;
	if (filecache->next)
		filecache->next->prev = filecache;
	carousel->filecaches = filecache;

	dsmcc_cache_init_new_filecache(carousel, filecache);

	return filecache;
}

struct dsmcc_file_cache *dsmcc_filecache_find(struct dsmcc_object_carousel *carousel, uint32_t queue_id)
{
	struct dsmcc_file_cache *filecache;
	for (filecache = carousel->filecaches; filecache; filecache = filecache->next)
		if (filecache->queue_id == queue_id)
			return filecache;
	return NULL;
}

static void free_files(struct dsmcc_cached_file *file)
{
	struct dsmcc_cached_file *next;

	if (!file)
		return;

	while (file)
	{
		next = file->next;
		if (file->name)
			free(file->name);
		if (file->path)
			free(file->path);
		if (file->data_file)
			free(file->data_file);
		free(file);
		file = next;
	}
}

static void free_dirs(struct dsmcc_cached_dir *dir)
{
	struct dsmcc_cached_dir *subdir, *subdirnext;

	if (!dir)
		return;

	subdir = dir->subdirs;
	while (subdir)
	{
		subdirnext = subdir->next;
		free_dirs(subdir);
		subdir = subdirnext;
	}

	free_files(dir->files);

	if (dir->name)
		free(dir->name);
	if (dir->path)
		free(dir->path);
	free(dir);
}

static void dsmcc_filecache_clear(struct dsmcc_file_cache *filecache)
{
	free_dirs(filecache->gateway);
	filecache->gateway = NULL;
	free_dirs(filecache->orphan_dirs);
	filecache->orphan_dirs = NULL;
	free_files(filecache->orphan_files);
	filecache->orphan_files = NULL;
	free_files(filecache->nameless_files);
	filecache->nameless_files = NULL;
}

void dsmcc_filecache_clear_all(struct dsmcc_object_carousel *carousel)
{
	struct dsmcc_file_cache *filecache;
	for (filecache = carousel->filecaches; filecache; filecache = filecache->next)
		dsmcc_filecache_clear(filecache);
}

void dsmcc_filecache_remove_all(struct dsmcc_object_carousel *carousel)
{
	struct dsmcc_file_cache *filecache;
	struct dsmcc_file_cache *next;

	for (filecache = carousel->filecaches; filecache; filecache = next)
	{
		next = filecache->next;

		dsmcc_filecache_clear(filecache);
		free(filecache->downloadpath);
		free(filecache);
	}

	carousel->filecaches = NULL;
}

void dsmcc_filecache_remove(struct dsmcc_file_cache *filecache)
{
	DSMCC_DEBUG("Removing filecache with queue_id %u", filecache->queue_id);
	dsmcc_filecache_clear(filecache);
	free(filecache->downloadpath);
	if (filecache->next)
		filecache->next->prev = filecache->prev;
	if (filecache->prev)
		filecache->prev->next = filecache->next;
	else
		filecache->carousel->filecaches = filecache->next;
	free(filecache);
}

struct dsmcc_file_cache *dsmcc_filecache_next(struct dsmcc_file_cache *filecache)
{
	return filecache->next;
}

static void add_file_to_list(struct dsmcc_cached_file **list_head, struct dsmcc_cached_file *file)
{
	file->next = *list_head;
	if (file->next)
		file->next->prev = file;
	file->prev = NULL;
	*list_head = file;
}

static void remove_file_from_list(struct dsmcc_cached_file **list_head, struct dsmcc_cached_file *file)
{
	if (file->prev)
		file->prev->next = file->next;
	else
		*list_head = file->next;
	if (file->next)
		file->next->prev = file->prev;
	file->prev = NULL;
	file->next = NULL;
}

static struct dsmcc_cached_file *find_file_in_list(struct dsmcc_cached_file *list_head, struct dsmcc_object_id *id)
{
	struct dsmcc_cached_file *file;

	for (file = list_head; file != NULL; file = file->next)
		if (idcmp(&file->id, id))
			break;

	return file;
}

static void add_dir_to_list(struct dsmcc_cached_dir **list_head, struct dsmcc_cached_dir *dir)
{
	dir->next = *list_head;
	if (dir->next)
		dir->next->prev = dir;
	dir->prev = NULL;
	*list_head = dir;
}

static void remove_dir_from_list(struct dsmcc_cached_dir **list_head, struct dsmcc_cached_dir *dir)
{
	if (dir->prev)
		dir->prev->next = dir->next;
	else
		*list_head = dir->next;
	if (dir->next)
		dir->next->prev = dir->prev;
	dir->prev = NULL;
	dir->next = NULL;
}

static struct dsmcc_cached_dir *find_dir_in_subdirs(struct dsmcc_cached_dir *parent, struct dsmcc_object_id *id)
{
	struct dsmcc_cached_dir *dir, *subdir;

	if (!parent)
		return NULL;

	if (idcmp(&parent->id, id))
		return parent;

	/* Search sub dirs */
	for (subdir = parent->subdirs; subdir != NULL; subdir = subdir->next)
	{
		dir = find_dir_in_subdirs(subdir, id);
		if (dir)
			return dir;
	}

	return NULL;
}

int dsmcc_filecache_write_file(struct dsmcc_file_cache *filecache, const char *file_path, const char *data_file, int data_size)
{
	char *fn;
	int written = 0;
	
	fn = malloc(strlen(filecache->downloadpath) + strlen(file_path) + 2);
	sprintf(fn, "%s%s%s", filecache->downloadpath, file_path[0] == '/' ? "" : "/", file_path);

	if (filecache->callbacks.dentry_check)
	{
		DSMCC_DEBUG("Filecache calling callback dentry_check(%u, 0x%08x, 0, '%s', '%s')",
				filecache->queue_id, filecache->carousel->cid, file_path, fn);
		if (!(*filecache->callbacks.dentry_check)(filecache->callbacks.dentry_check_arg,
				filecache->queue_id, filecache->carousel->cid, 0, file_path, fn))
		{
			DSMCC_DEBUG("Skipping file %s as requested", file_path);
			goto cleanup;
		}
	}

	DSMCC_DEBUG("Linking data from %s to %s", data_file, fn);
	if (dsmcc_file_link(fn, data_file, data_size))
	{
		written = 1;

		if (filecache->callbacks.dentry_saved)
		{
			DSMCC_DEBUG("Filecache calling callback dentry_saved(%u, 0x%08x, 0, '%s', '%s')",
					filecache->queue_id, filecache->carousel->cid, file_path, fn);
			(*filecache->callbacks.dentry_saved)(filecache->callbacks.dentry_saved_arg,
					filecache->queue_id, filecache->carousel->cid, 0, file_path, fn);
		}
	}

cleanup:
	free(fn);
	return written;
}

static void link_file(struct dsmcc_file_cache *filecache, struct dsmcc_cached_file *file)
{
	/* Skip already written files or files for which data has not yet arrived */
	if (file->written || !file->data_file)
		return;

	DSMCC_DEBUG("Writing out file %s under dir %s", file->name, file->parent->path);

	file->path = malloc(strlen(file->parent->path) + strlen(file->name) + 2);
	sprintf(file->path, "%s/%s", file->parent->path, file->name);

	file->written = dsmcc_filecache_write_file(filecache, file->path, file->data_file, file->data_size);

}

static void write_dir(struct dsmcc_file_cache *filecache, struct dsmcc_cached_dir *dir)
{
	bool gateway;
	struct dsmcc_cached_dir *subdir;
	struct dsmcc_cached_file *file;
	char *dn = NULL;

	/* Skip already written dirs */
	if (dir->written)
		return;

	gateway = (dir == filecache->gateway);

	if (gateway)
	{
		DSMCC_DEBUG("Writing out gateway dir");
		dir->path = strdup("");
		dn = strdup(filecache->downloadpath);
	}
	else
	{
		DSMCC_DEBUG("Writing out dir %s under dir %s", dir->name, dir->parent->path);
		dir->path = malloc(strlen(dir->parent->path) + strlen(dir->name) + 2);
		sprintf(dir->path, "%s/%s", dir->parent->path, dir->name);
		dn = malloc(strlen(filecache->downloadpath) + strlen(dir->path) + 2);
		sprintf(dn, "%s/%s", filecache->downloadpath, dir->path);
	}

	/* call callback (except for gateway) */
	if (!gateway && filecache->callbacks.dentry_check)
	{
		DSMCC_DEBUG("Filecache calling callback dentry_check(%u, 0x%08x, 1, '%s', '%s')",
				filecache->queue_id, filecache->carousel->cid, dir->path, dn);
		if (!(*filecache->callbacks.dentry_check)(filecache->callbacks.dentry_check_arg,
				filecache->queue_id, filecache->carousel->cid, 1, dir->path, dn))
		{
			DSMCC_DEBUG("Skipping directory %s as requested", dir->path);
			goto end;
		}
	}

	DSMCC_DEBUG("Creating directory %s", dn);
	mkdir(dn, 0755); 
	dir->written = 1;

	/* register and call callback (except for gateway) */
	if (!gateway && filecache->callbacks.dentry_saved)
	{
		DSMCC_DEBUG("Filecache calling callback dentry_saved(%u, 0x%08x, 1, '%s', '%s')",
				filecache->queue_id, filecache->carousel->cid, dir->path, dn);
		(*filecache->callbacks.dentry_saved)(filecache->callbacks.dentry_saved_arg,
				filecache->queue_id, filecache->carousel->cid, 1, dir->path, dn);
	}

	/* Write out files that had arrived before directory */
	for (file = dir->files; file; file = file->next)
		link_file(filecache, file);

	/* Recurse through child directories */
	for (subdir = dir->subdirs; subdir; subdir = subdir->next)
		write_dir(filecache, subdir);

end:
	if (dn)
		free(dn);
}

static struct dsmcc_cached_dir *find_cached_dir(struct dsmcc_file_cache *filecache, struct dsmcc_object_id *id)
{
	struct dsmcc_cached_dir *dir;

	DSMCC_DEBUG("Searching for dir 0x%04hx:0x%08x:0x%08x", id->module_id, id->key, id->key_mask);

	/* Find dir */
	dir = find_dir_in_subdirs(filecache->gateway, id);
	if (dir == NULL)
	{
		struct dsmcc_cached_dir *d;

		/* Try looking in orphan dirs */
		for (d = filecache->orphan_dirs; !dir && d; d = d->next)
			dir = find_dir_in_subdirs(d, id);
	}

	return dir;
}

void dsmcc_filecache_cache_dir(struct dsmcc_file_cache *filecache, struct dsmcc_object_id *parent_id, struct dsmcc_object_id *id, const char *name)
{
	struct dsmcc_cached_dir *dir, *subdir, *subdirnext;
	struct dsmcc_cached_file *file, *filenext;

	dir = find_cached_dir(filecache, id);
	if (dir)
		return; /* Already got */

	dir = calloc(1, sizeof(struct dsmcc_cached_dir));
	dir->name = name ? strdup(name) : NULL;
	memcpy(&dir->id, id, sizeof(struct dsmcc_object_id));

	/* Check if parent ID is NULL, i.e. if this is the gateway */
	if (!parent_id)
	{
		DSMCC_DEBUG("Caching gateway dir 0x%04hx:0x%08x:0x%08x", id->module_id, id->key, id->key_mask);

		filecache->gateway = dir;
	}
	else
	{
		DSMCC_DEBUG("Caching dir %s (0x%04hx:0x%08x:0x%08x) with parent 0x%04hx:0x%08x:0x%08x", dir->name,
				id->module_id, id->key, id->key_mask, parent_id->module_id, parent_id->key, parent_id->key_mask);

		memcpy(&dir->parent_id, parent_id, sizeof(struct dsmcc_object_id));
		dir->parent = find_cached_dir(filecache, &dir->parent_id);
		if (!dir->parent)
		{
			/* Directory not yet known. Add this to dirs list */
			add_dir_to_list(&filecache->orphan_dirs, dir);
		}
		else
		{
			/* Create under parent directory */
			add_dir_to_list(&dir->parent->subdirs, dir);
		}
	}

	/* Attach any files that arrived previously */
	for (file = filecache->orphan_files; file != NULL; file = filenext)
	{
		filenext = file->next;
		if (idcmp(&file->parent_id, &dir->id))
		{
			DSMCC_DEBUG("Attaching previously arrived file %s to newly created directory %s", file->name, dir->name);

			/* detach from orphan files */
			remove_file_from_list(&filecache->orphan_files, file);

			/* attach to dir */
			file->parent = dir;
			add_file_to_list(&dir->files, file);
		}
	}

	/* Attach any subdirs that arrived beforehand */
	for (subdir = filecache->orphan_dirs; subdir != NULL; subdir = subdirnext)
	{
		subdirnext = subdir->next;
		if (idcmp(&subdir->parent_id, &dir->id))
		{
			DSMCC_DEBUG("Attaching previously arrived dir %s to newly created directory %s", subdir->name, dir->name);

			/* detach from orphan dirs */
			remove_dir_from_list(&filecache->orphan_dirs, subdir);

			/* attach to parent */
			subdir->parent = dir;
			add_dir_to_list(&dir->subdirs, subdir);
		}
	}

	/* Write dir/files to filesystem */
	if (dir == filecache->gateway || (dir->parent && dir->parent->written))
		write_dir(filecache, dir);
}

static struct dsmcc_cached_file *find_file_in_dir(struct dsmcc_cached_dir *dir, struct dsmcc_object_id *id)
{
	struct dsmcc_cached_file *file;
	struct dsmcc_cached_dir *subdir;

	if (!dir)
		return NULL;

	/* Search files in this dir */
	file = find_file_in_list(dir->files, id);
	if (file)
		return file;

	/* Search sub dirs */
	for (subdir = dir->subdirs; subdir != NULL; subdir = subdir->next)
		if ((file = find_file_in_dir(subdir, id)) != NULL)
			return file;

	return NULL;
}

static struct dsmcc_cached_file *find_file(struct dsmcc_file_cache *filecache, struct dsmcc_object_id *id)
{
	struct dsmcc_cached_file *file;

	/* Try looking in parent-less list */
	file = find_file_in_list(filecache->orphan_files, id);
	if (file)
		return file;

	/* Scan through known files and return details if known else NULL */
	file = find_file_in_dir(filecache->gateway, id);

	return file;
}

void dsmcc_filecache_cache_file(struct dsmcc_file_cache *filecache, struct dsmcc_object_id *parent_id, struct dsmcc_object_id *id, const char *name)
{
	struct dsmcc_cached_file *file;

	/* Check we do not already have file info or data cached  */
	if (find_file(filecache, id))
		return;

	/* See if the data had already arrived for the file */
	file = find_file_in_list(filecache->nameless_files, id);
	if (file)
	{
		DSMCC_DEBUG("Data already arrived for file %s", name);
		remove_file_from_list(&filecache->nameless_files, file);
	}
	else
	{
		DSMCC_DEBUG("Data not arrived for file %s, caching file info", name);
		file = calloc(1, sizeof(struct dsmcc_cached_file));
		memcpy(&file->id, id, sizeof(struct dsmcc_object_id));
	}

	memcpy(&file->parent_id, parent_id, sizeof(struct dsmcc_object_id));
	file->name = strdup(name);
	file->parent = find_cached_dir(filecache, &file->parent_id);

	DSMCC_DEBUG("Caching info in carousel %u for file %s (0x%04hx:0x%08x/0x%08x) with parent dir 0x%04hx:0x%08x/0x%08x",
			filecache->carousel->cid, file->name, file->id.module_id, file->id.key, file->id.key_mask,
			file->parent_id.module_id, file->parent_id.key, file->parent_id.key_mask);

	/* Check if parent directory is known */
	if (file->parent)
	{
		add_file_to_list(&file->parent->files, file);
		link_file(filecache, file);
	}
	else
		add_file_to_list(&filecache->orphan_files, file);
}

void dsmcc_filecache_cache_data(struct dsmcc_file_cache *filecache, struct dsmcc_object_id *id, const char *data_file, uint32_t data_size)
{
	struct dsmcc_cached_file *file;

	/* search for file info */
	file = find_file(filecache, id);
	if (!file)
	{
		/* Not known yet. Save data */
		DSMCC_DEBUG("Nameless file 0x%04x:0x%08x/0x%08x in carousel %u, caching data", id->module_id, id->key, id->key_mask, filecache->carousel->cid);

		file = calloc(1, sizeof(struct dsmcc_cached_file));
		memcpy(&file->id, id, sizeof(struct dsmcc_object_id));

		file->data_file = strdup(data_file);
		file->data_size = data_size;

		/* Add to nameless files */
		add_file_to_list(&filecache->nameless_files, file);
	}
	else
	{
		/* Save data */
		if (!file->written)
		{
			file->data_file = strdup(data_file);
			file->data_size = data_size;

			link_file(filecache, file);
		}
		else
		{
			DSMCC_DEBUG("Data for file %s in dir %s had already been saved", file->name, file->parent->path);
		}
	}
}

void dsmcc_filecache_notify_progression(struct dsmcc_object_carousel *carousel, struct dsmcc_file_cache *filecache, uint32_t downloaded, uint32_t total)
{
	if (filecache)
	{
		DSMCC_DEBUG("Filecache calling callback download_progression(%u, 0x%08x, %u, %u)",
				filecache->queue_id, filecache->carousel->cid, downloaded, total);
		(*filecache->callbacks.download_progression)(filecache->callbacks.download_progression_arg,
				filecache->queue_id, filecache->carousel->cid, downloaded, total);
	}
	else
	{
		for (filecache = carousel->filecaches; filecache; filecache = dsmcc_filecache_next(filecache))
			dsmcc_filecache_notify_progression(carousel, filecache, downloaded, total);
	}
}

void dsmcc_filecache_notify_status(struct dsmcc_object_carousel *carousel, struct dsmcc_file_cache *filecache)
{
	if (filecache)
	{
		if (filecache->carousel->status != filecache->last_carousel_status)
		{
			filecache->last_carousel_status = filecache->carousel->status;
			DSMCC_DEBUG("Filecache calling callback carousel_status_changed(%u, 0x%08x, %d)",
					filecache->queue_id, filecache->carousel->cid, filecache->carousel->status);
			(*filecache->callbacks.carousel_status_changed)(filecache->callbacks.carousel_status_changed_arg,
					filecache->queue_id, filecache->carousel->cid, filecache->carousel->status);
		}
	}
	else
	{
		for (filecache = carousel->filecaches; filecache; filecache = dsmcc_filecache_next(filecache))
			dsmcc_filecache_notify_status(carousel, filecache);
	}
}
