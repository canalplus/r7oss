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

#include <sys/types.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <alloca.h>
#include <assert.h>
#include <fnmatch.h>
#include <time.h>

#include "initng_global.h"
#include "initng_active_db.h"
#include "initng_process_db.h"
#include "initng_toolbox.h"
#include "initng_common.h"
#include "initng_static_data_id.h"
#include "initng_plugin_callers.h"
#include "initng_string_tools.h"
#include "initng_static_states.h"
#include "initng_depend.h"

/* #####  ACTIVE_DB_FIND_BY   ######## */

/*
 * This is an exact search "net/eth0" must be "net/eth0"
 */
active_db_h *initng_active_db_find_by_exact_name(const char *service)
{
	active_db_h *current = NULL;

	D_("(%s);\n", (char *) service);

	assert(service);

	/* first, search for an exact match */
	while_active_db(current)
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
active_db_h *initng_active_db_find_by_name(const char *service)
{
	assert(service);
	active_db_h *current = NULL;

	D_("(%s);\n", (char *) service);

	/* first give the exact find a shot */
	if ((current = initng_active_db_find_by_exact_name(service)))
		return (current);


	/* did not find any */
	return NULL;

	/* no need in pattern matching, because of unique names in cache (TheLich) */

	/* walk the active db and compere */
	current = NULL;
	while_active_db(current)
	{
		assert(current->name);
		/* then try to find alike name */
		if (service_match(current->name, service))
			return (current);

	}
}

#ifdef SERVICE_CACHE
/*
 * This is called before dynamic data fetchers goes resursive.
 * and give us a chanse to try to set head->res if possible
 */
int reload_service_cache(data_head * head)
{
	active_db_h *service = NULL;

	/*printf("reload_service_cache();\n"); */
	if (head->res)
		return (TRUE);

	/* Get the service pointer */
	service = list_entry(head, active_db_h, data);

	/* Check if from_service is set */
	if (service->from_service)
		/* point the resursive data pointer, to the service_cache data head */
		head->res = &service->from_service->data;

	/* OK, try reload data from disk then */
	if (initng_common_get_service(service) && service->from_service)
	{
		/* point the resursive data pointer, to the service_cache data head */
		head->res = &service->from_service->data;
		return (TRUE);
	}

	D_("Failed to reload service_cache for %s\n", service->name);
	service->from_service = &NO_CACHE;
	return (FALSE);
}
#endif

/*
 * This will search and find the best possible.
 * Search for "eth" will get you "net/eth0"
 */
active_db_h *initng_active_db_find_in_name(const char *service)
{
	active_db_h *current = NULL;

	assert(service);

	D_("(%s);\n", (char *) service);

	/* first search by name */
	if ((current = initng_active_db_find_by_name(service)))
		return (current);

	/* then search for a word match */
	current = NULL;
	while_active_db(current)
	{
		assert(current->name);
		if (match_in_service(current->name, service))
			return (current);
	}

	/* did not find any */
	return (NULL);
}

#ifdef SERVICE_CACHE
/* return index of service in active data structure or -1 if not found */
active_db_h *initng_active_db_find_by_service_h(service_cache_h * service)
{
	active_db_h *current = NULL;

	assert(service);
	assert(service->name);

	/* walk the active_db */
	while_active_db(current)
	{
		assert(current->name);
		/* check if this service->from_service is like service */
		if (current->from_service && current->from_service == service)
			return (current);				/* return it */

	}

	return NULL;
}
#endif

/* returns pointer to active_h process belongs to, and sets process_type */
active_db_h *initng_active_db_find_by_pid(pid_t pid)
{
	active_db_h *currentA = NULL;
	process_h *currentP = NULL;

	/* walk the active_db */
	while_active_db(currentA)
	{
		assert(currentA->name);
		currentP = NULL;
		while_processes(currentP, currentA)
		{
			if (currentP->pid == pid)
				return (currentA);
		}
	}

	/* bad luck */
	return (NULL);
}

/* #########   STRUCT/CHAIN HANDLE  ######## */

/* add active to data structure */
/* remember to free service, if this fails */
int initng_active_db_register(active_db_h * add_this)
{
	active_db_h *current = NULL;

	/* we have to get this data */
	assert(add_this);
	assert(add_this->name);

	/* Look for duplicate */
	if ((current = initng_active_db_find_by_name(add_this->name)))
	{
		/* TODO, should add_this bee freed? */
		W_("active_db_add(%s): duplicate here\n", add_this->name);
		return (FALSE);
	}

	list_add(&add_this->list, &g.active_database.list);

	return (TRUE);
}

/* ############    ACTIVE_DB_ALLOC/FREE ##########  */

/* creates a new active_h at new_active */
active_db_h *initng_active_db_new(const char *name)
{
	active_db_h *new_active = NULL;

	assert(name);

	/* allocate a new active entry */
	new_active = (active_db_h *) i_calloc(1, sizeof(active_db_h));	/* Allocate memory for a new active */
	if (!new_active)						/* out of memory? */
	{
		F_("Unable to allocate active, out of memory?\n");
		return (NULL);
	}

	/* initialize all lists */
	INIT_LIST_HEAD(&(new_active->processes.list));

	/* set the name */
	new_active->name = i_strdup(name);
	if (!new_active->name)
	{
		F_("Unable to set name, out of memory?\n");
		return (NULL);
	}

#ifdef SERVICE_CACHE
	/* initiate the data list, little special here sense data list relays on service->from_service */
	DATA_HEAD_INIT_REQUEST(&new_active->data, NULL, &reload_service_cache);
#else
	DATA_HEAD_INIT_REQUEST(&new_active->data, NULL, NULL);
#endif

	/* get the time, and copy that time to all time entries */
	clock_gettime(CLOCK_MONOTONIC, &new_active->time_current_state);
	memcpy(&new_active->time_last_state, &new_active->time_current_state,
		   sizeof(struct timespec));
	memcpy(&new_active->last_rought_time, &new_active->time_current_state,
		   sizeof(struct timespec));

	/* mark this service as stopped, because it is not yet starting, or running */
	new_active->current_state = &NEW;

	/* reset alarm */
	new_active->alarm = 0;

	/* return the newly created active_db_h */
	return (new_active);
}

/* free some values in this one */
void initng_active_db_free(active_db_h * pf)
{
	process_h *current = NULL;
	process_h *safe = NULL;

	assert(pf);
	assert(pf->name);

	D_("(%s);\n", pf->name);

	/* look if there are plug-ins, that is interested to now, this is freeing */
	initng_common_mark_service(pf, &FREEING);

	/* unregister on all lists */
	list_del(&pf->list);
	list_del(&pf->interrupt);

	while_processes_safe(current, safe, pf)
	{
		initng_process_db_real_free(current);
	}

	/* remove every data entry */
	remove_all(pf);

#ifdef SERVICE_CACHE
	/* remove file cache of entry if present, so we got a fresh read from file when this service is restarted */
	if (pf->from_service)
	{
		/* remove from cache list */
		list_del(&pf->from_service->list);
		/* free entry */
		initng_service_cache_free(pf->from_service);
	}
#endif

	/* free service name */
	if (pf->name)
		free(pf->name);

	/* free service struct */
	free(pf);
}


/* clear database */
void initng_active_db_free_all(void)
{
	active_db_h *current, *safe = NULL;

	while_active_db_safe(current, safe)
	{
		initng_active_db_free(current);
	}
}

/* #########  ACTIVE_DB_UTILS  #############  */


/* compensate time */
void initng_active_db_compensate_time(time_t skew)
{
	active_db_h *current = NULL;

	/* walk the active_db */
	while_active_db(current)
	{
		assert(current->name);
		/* change this time */
		current->time_current_state.tv_sec += skew;
		current->time_last_state.tv_sec += skew;
		current->last_rought_time.tv_sec += skew;
		current->alarm += skew;
	}
}

#ifdef SERVICE_CACHE
/* update all service_h pointers in whole active_db */
void initng_active_db_change_service_h(service_cache_h * from,
									   service_cache_h * to)
{
	active_db_h *current = NULL;

	assert(from);
	/* !assert(to) to can be NULL, if we want clear that entry */

	/* walk the active_db */
	while_active_db(current)
	{
		assert(current->name);
		/* change this time */
		if (current->from_service == from)
		{
			current->from_service = to;

			/* Reset data resursive pointer, will be set by reload_service_cache */
			if (current->from_service)
				current->data.res = &current->from_service->data;
			else
				current->data.res = NULL;
		}
	}
}
#endif

/* active_db_count counts a type, if null, count all */
int initng_active_db_count(a_state_h * current_state_to_count)
{
	int counter = 0;			/* actives counter */
	active_db_h *current = NULL;

	/* ok, go COUNT ALL */
	if (!current_state_to_count)
	{
		/* ok, go through all */
		while_active_db(current)
		{
			assert(current->name);

			/* count almost all */

			/* but not failed services */
			if (IS_FAILED(current))
				continue;
			/* and not stopped */
			if (IS_DOWN(current))
				continue;

			counter++;
		}

		return (counter);
	}

	/* ok, go COUNT A SPECIAL */
	while_active_db(current)
	{
		assert(current->name);
		/* check if this is the status to count */
		if (current->current_state == current_state_to_count)
			counter++;
	}
	/* return counter */
	return (counter);
}


/* calculate percent of processes started */
int initng_active_db_percent_started(void)
{
	int starting = 0;
	int up = 0;
	int other = 0;
	float tmp = 0;

	active_db_h *current = NULL;

	/* walk the active_db */
	while_active_db(current)
	{
		assert(current->name);
		assert(current->current_state);

		/* count starting */
		if (IS_STARTING(current))
		{
			starting++;
			continue;
		}

		/* count up */
		if (IS_UP(current))
		{
			up++;
			continue;
		}

		/* count others */
		other++;
	}
	D_("active_db_percent_started(): up: %i   starting: %i  other: %i\n", up,
	   starting, other);

	/* if no one starting */
	if (starting <= 0)
		return (100);

	if (up > 0)
	{
		tmp = 100 * (float) up / (float) (starting + up);
		D_("active_db_percent_started(): up/starting: %f percent: %i\n\n",
		   (float) up / (float) starting, (int) tmp);
		return ((int) tmp);
	}
	return 0;
}

/* calculate percent of processes stopped */
int initng_active_db_percent_stopped(void)
{
	int stopping = 0;
	int down = 0;
	int other = 0;
	float tmp = 0;
	active_db_h *current = NULL;

	while_active_db(current)
	{
		assert(current->name);
		assert(current->current_state);

		/* count stopped services */
		if (IS_DOWN(current))
		{
			down++;
			continue;
		}

		/* count stopping */
		if (IS_STOPPING(current))
		{
			stopping++;
			continue;
		}

		/* count others */
		other++;
	}

	D_("active_db_percent_stopped(): down: %i   stopping: %i  other: %i\n",
	   down, stopping, other);

	/* if no one stopping */
	if (stopping <= 0)
		return (100);

	if (down > 0)
	{
		tmp = 100 * (float) down / (float) (stopping + down);
		D_("active_db_percent_stopped(): down/stopping: %f percent: %i\n\n",
		   (float) down / (float) stopping, (int) tmp);
		return ((int) tmp);
	}
	return 0;
}



/*
 * Walk the active db, searching for services that are down, and been so for a minute.
 * It will remove this entry to save memory.
 * CLEAN_DELAY are defined in initng.h
 */
void initng_active_db_clean_down(void)
{
	active_db_h *current = NULL;
	active_db_h *safe = NULL;

	while_active_db_safe(current, safe)
	{
		assert(current->name);
		assert(current->current_state);
		if (g.now.tv_sec > current->time_current_state.tv_sec + CLEAN_DELAY)
		{
			initng_process_db_clear_freed(current);

			/* count stopped services */
			if (!IS_DOWN(current))
				continue;

			initng_active_db_free(current);
		}
	}
}
