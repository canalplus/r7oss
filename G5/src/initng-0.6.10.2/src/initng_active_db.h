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

#ifndef ACTIVE_DB_H
#define ACTIVE_DB_H

typedef struct active_type active_db_h;


#include <sys/types.h>
#include <unistd.h>
#ifdef SERVICE_CACHE
#include "initng_service_cache.h"
#endif

#include "initng_struct_data.h"
#include "initng_list.h"

#include "initng_service_types.h"
#include "initng_active_state.h"
#include "initng_process_db.h"

#define MAX_SUCCEEDED 30

/* the active service struct */
struct active_type
{

	/***** IDENTIFICATION ***********/
	char *name;					/* the name of this service */
	stype_h *type;				/* the service type */

	/******* STATE ******************/

	/*
	 * current state.
	 * This pointer point to a a_state_h struct containing
	 * info in what state the service is in currelty, also
	 * is a timepoint saved in here when the service got that state.
	 */
	a_state_h *current_state;
	struct timespec time_current_state;	/* the time got current state */

	/*
	 * Last state
	 * Here initng saves a pointer to the last state that this
	 * service have before changing the state pointed by service->current_state
	 * also a timepoint when the service got that state is saved.
	 */
	a_state_h *last_state;
	struct timespec time_last_state;	/* the time got last state */

	/*
	 * Next state & state_lock
	 * They exist to avoid an state change during an event, next_state holds
	 * the state the service will get after unlocking, an state_lock is a
	 * flag to check if it's locked.
	 */
	a_state_h *next_state;
	int state_lock;

	/*
	 * Rough state
	 * The rought state is normally, UP, DOWN, STARTING...
	 * Thie current rough state can by found by service->current_stata->is
	 * here is the last rough time, and a timepoint saved.
	 */
	e_is last_rought_state;
	struct timespec last_rought_time;	/* the time got last rught state */

	/******** SUB_OBJECTS ***********/
	/* list of system processes that are connected to this service */
	process_h processes;

#ifdef SERVICE_CACHE
	/* a pointer to the entry in service_cache_db that matches our service */
	service_cache_h *from_service;
#endif

	/*
	 * list of data
	 * Storage for all dynamic variables that are set to this
	 * service, this data struct also sets a resurvice pointer to
	 * the data_head of the service_chache object pointed by abow,
	 * so if the variable is not fund in this list, it searched in
	 * the from_service->data list too.
	 */
	data_head data;

	/******** VARIABLES *************/

	/* Alarm, the current state alarm is run when this time passes */
	time_t alarm;

	/******* TEMPORARY STUFF ********/

	/* depend cache - Optimization to speed up UP_DEPS_CHECK */
	int depend_cache;

	/******* LIST_HEADS *************/

	/* the list */
	struct list_head list;
	struct list_head interrupt;
};

/* allocate */
active_db_h *initng_active_db_new(const char *name);

/* searching */
active_db_h *initng_active_db_find_by_exact_name(const char *service);
active_db_h *initng_active_db_find_by_name(const char *service);
active_db_h *initng_active_db_find_in_name(const char *service);
active_db_h *initng_active_db_find_by_pid(pid_t pid);


/* service cache calls */
#ifdef SERVICE_CACHE
active_db_h *initng_active_db_find_by_service_h(service_cache_h * service);
void initng_active_db_change_service_h(service_cache_h * from,
									   service_cache_h * to);
#endif

/* mangling */
void initng_active_db_compensate_time(time_t skew);

/* reload service cache if not set */
int reload_service_cache(data_head * data);

/* the db */
#define initng_active_db_unregister(serv) list_del(&(serv)->list)
int initng_active_db_register(active_db_h * new_a);
int initng_active_db_count(a_state_h * state);
void initng_active_db_free(active_db_h * pf);
void initng_active_db_free_all(void);

/* utils */
int initng_active_db_percent_started(void);
int initng_active_db_percent_stopped(void);
void initng_active_db_clean_down(void);

/* walk the db */
#define while_active_db(current) list_for_each_entry_prev(current, &g.active_database.list, list)
#define while_active_db_safe(current, safe) list_for_each_entry_prev_safe(current, safe, &g.active_database.list, list)
#define while_active_db_interrupt(current) list_for_each_entry_prev(current, &g.active_database.interrupt, interrupt)
#define while_active_db_interrupt_safe(current, safe) list_for_each_entry_prev_safe(current, safe, &g.active_database.interrupt, interrupt)

#endif
