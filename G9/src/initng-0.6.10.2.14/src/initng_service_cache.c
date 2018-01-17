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

#include "initng.h"

#define _GNU_SOURCE
#include <stdio.h>
#include <alloca.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <assert.h>

#include "initng_global.h"
#include "initng_process_db.h"
#include "initng_service_cache.h"
#include "initng_string_tools.h"
#include "initng_active_db.h"
#include "initng_toolbox.h"
#include "initng_plugin_callers.h"
#include "initng_common.h"
#include "initng_service_data_types.h"
#include "initng_struct_data.h"

service_cache_h NO_CACHE = { (char *) "NO_CACHE", NULL };

/*
 * This is a simple list_add(&service->list), but
 * will first check for duplicates.
 */
int initng_service_cache_register(service_cache_h * s)
{
	service_cache_h *found = NULL;

	assert(s->name);
	assert(s);

	/* Check that we are unique */
	found = initng_service_cache_find_by_exact_name(s->name);
	if (found)
	{
		W_("Service %s already in db, please look there before parsing\nTo flush your file cache do ngc -R\n", s->name);
		return (FALSE);
	}

	/* if not found, add this one on the list */
	list_add(&(s->list), &g.service_cache.list);

	return (TRUE);
}


/*
 * Instead of service_db_new, this service allocates a new
 * service, and copy data from another service.
 *
 * OBS, this is not, as setting service->father.
 */
service_cache_h *initng_service_cache_copy(char *name, service_cache_h * s)
{
	service_cache_h *new_serv = NULL;

	assert(s);
	assert(name);
	D_("copy_service(%s);\n", s->name);

	/* allocate new service */
	new_serv = (service_cache_h *) i_calloc(1, sizeof(service_cache_h));
	if (!new_serv)
	{
		F_("Could not allocate space, for new copy of service!\n");
		return (NULL);
	}

	/* copy service structure */
	memcpy(new_serv, s, sizeof(service_cache_h));
	/* name has to diff */
	new_serv->name = name;

	/* initialize the data structure */
	DATA_HEAD_INIT(&new_serv->data);

	/* copy over all strings */
	d_copy_all(&s->data, &new_serv->data);

	/* we must clear pointers, otherwise this service 
	 * will not be added to list
	 */
	new_serv->list.prev = new_serv->list.next = NULL;

	return (new_serv);
}

/*
 * Allocates an entry, in memory, and sets defaults,
 * name & type, initialize lists.
 */
service_cache_h *initng_service_cache_new(const char *name, stype_h * type)
{
	service_cache_h *new_s = NULL;

	assert(name);
	D_("default_service();\n");
	if (!(new_s = (service_cache_h *) i_calloc(1, sizeof(service_cache_h))))
	{
		F_("OUT OF MEMORY default_service().\n");
		return (NULL);
	}

	/* set name & type */
	new_s->name = i_strdup(name);
	new_s->type = type;

	/* initialize the data list struct */
	DATA_HEAD_INIT(&new_s->data);
	return (new_s);
}


/*
 *
 *          FIND SERVICE
 *
 */

/*
 * This is an exact search "net/eth0" must be "net/eth0"
 */
service_cache_h *initng_service_cache_find_by_exact_name(const char *service)
{
	service_cache_h *current = NULL;

	D_("(%s);\n", service);

	assert(service);

	/* first, search for an exact match */
	while_service_cache(current)
	{

		assert(current->name);
		/* check if this service name is like service */
		if (strcmp(current->name, service) == 0)
			return (current);				/* return it */
	}

	/* did not find any */
	return NULL;
}

/*
 * This is an less exact name, that will work with wildcards.
 * Searching for "net/eth*" will give you "net/eth0"
 */
service_cache_h *initng_service_cache_find_by_name(const char *service)
{
	assert(service);
	service_cache_h *current = NULL;

	D_("(%s);\n", service);

	/* first give the exact find a shot */
	if ((current = initng_service_cache_find_by_exact_name(service)))
	{
		return (current);
	}


	/* walk the active db and compere */
	current = NULL;
	while_service_cache(current)
	{
		assert(current->name);
		/*
		 * then try to find alike name
		 * When searching for services, in service cache, the match is reverse,
		 * then how active_db_find_by_name.
		 * 
		 * The service name searching data for is always correct, but the data,
		 * might have an '*' or '?'
		 */
		if (service_match(service, current->name))
		{
			D_("(%s, %s) MATCH\n", service, current->name);
			return (current);
		}

	}
	return (NULL);
}

/*
 * This will search and find the best possible.
 * Search for "eth" will get you "net/eth0"
 */

 /* TODO, why not use service_match in this */
service_cache_h *initng_service_cache_find_in_name(const char *service)
{
	service_cache_h *current = NULL;

	assert(service);

	/* first search by name */
	if ((current = initng_service_cache_find_by_name(service)))
		return (current);

	/* then search for a word match */
	current = NULL;
	while_service_cache(current)
	{
		assert(current->name);
		if (match_in_service(current->name, service))
			return (current);
	}

	/* did not find any */
	return (NULL);
}

/*
 * When a service is freed, we have to make sure there is
 * no service that links to empty space, so we walk
 * the service_db, looking for this service pointer, 
 * and removes it.
 */
static void initng_service_cache_clear_father_pointer_from(service_cache_h *
														   s)
{
	service_cache_h *current = NULL;

	while_service_cache(current)
	{
		if (current->father == s)
		{
			current->father = NULL;
			current->data.res = NULL;
		}
	}
}

/*
 * Free all dynamically allocated strings, and other data
 * in this service, and free the service.
 * OBS. this function WONT delete the entry from the
 * service cache, so calling this, with this service on
 * that list, will make initng segfault.
 */
int initng_service_cache_free(service_cache_h * to_free)
{
	assert(to_free);
	D_("service_db_free(%s);\n", to_free->name);

	/* obious this one cant be free */
	if (to_free == &NO_CACHE)
		return (TRUE);

	/* change other database relations to this service to null */
	initng_active_db_change_service_h(to_free, NULL);

	/* this is a dangerous pointer that don't have to hang loose */
	initng_service_cache_clear_father_pointer_from(to_free);

	/* free all data & strings in service */
	remove_all(to_free);
	DATA_HEAD_INIT(&to_free->data);

	/* free father_name */
	if (to_free->father_name)
		free(to_free->father_name);

	/* free service name */
	if (to_free->name)
		free(to_free->name);

	/* make sure its not on the service list */
	if (to_free->list.next || to_free->list.prev)
		list_del(&to_free->list);

	free(to_free);
	return (TRUE);
}

/*
 * "Free all service data" == "Flush the service data cache"
 */
void initng_service_cache_free_all(void)
{
	service_cache_h *current, *q = NULL;

	while_service_cache_safe(current, q)
	{
		initng_service_cache_free(current);
	}

	/* make sure its compleatly empty */
	INIT_LIST_HEAD(&g.service_cache.list);
}

/*
 * if the service->father_name is set, but the pointer
 * to the service service->father is not set, this
 * functions go out, searching for that father.
 */
int initng_service_cache_find_father(service_cache_h * service)
{
	assert(service);
	/* if father is known, return ok */
	if (service->father)
		return (TRUE);
	/* if no father is needed */
	if (!service->father_name)
		return (FALSE);

	service->father = initng_common_parse_service(service->father_name);

	/* check */
	if (service->father)
	{
		/* Put service data collection to be resursive to fathers data */
		service->data.res = &service->father->data;
		return (TRUE);
	}

	/* else fail */
	return (FALSE);
}
