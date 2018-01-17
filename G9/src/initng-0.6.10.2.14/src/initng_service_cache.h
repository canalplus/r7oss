/*
 * Initng, a next generation sysvinit replacement.
 * Copyright (C) 2006 Jimmy Wennlund <jimmy.wennlund@gmail.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef SERVICE_DB_H
#define SERVICE_DB_H

#include "initng_list.h"
#include "initng_service_types.h"
#include "initng_struct_data.h"

typedef struct s_service_cache_h service_cache_h;
struct s_service_cache_h
{
	char *name;
	stype_h *type;

	/* list container, to store data */
	data_head data;

	/* father/relative data */
	char *father_name;
	service_cache_h *father;

	/* add a list entry */
	struct list_head list;
};

/* set, services that is virtually created not from a file to NO_CACHE */
extern service_cache_h NO_CACHE;

/* service handling functions */
int initng_service_cache_register(service_cache_h * s);
service_cache_h *initng_service_cache_copy(char *name, service_cache_h * s);
service_cache_h *initng_service_cache_new(const char *name, stype_h * type);

/* Searching */
service_cache_h *initng_service_cache_find_by_exact_name(const char *service);
service_cache_h *initng_service_cache_find_by_name(const char *service);
service_cache_h *initng_service_cache_find_in_name(const char *service);

int initng_service_cache_find_father(service_cache_h * service);

/* freeing */
int initng_service_cache_free(service_cache_h * to_free);
void initng_service_cache_free_all(void);

/* update */
void initng_service_cache_change(service_cache_h * from,
								 service_cache_h * to);

/* standard wrappers using service */
#define initng_service_cache_get_next(t,s,l) d_get_next(t, &(s)->data.list, l)

/* service_list walker */
#define while_service_cache(current) list_for_each_entry_prev(current, &g.service_cache.list, list)
#define while_service_cache_safe(current, safe) list_for_each_entry_prev_safe(current, safe, &g.service_cache.list, list)

#endif
