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

/* common standard first define */
#include "initng.h"

/* system headers */
#include <sys/time.h>
#include <time.h>							/* time() */
#include <fcntl.h>							/* fcntl() */
#include <sys/un.h>							/* memmove() strcmp() */
#include <sys/wait.h>						/* waitpid() sa */
#include <linux/kd.h>						/* KDSIGACCEPT */
#include <sys/ioctl.h>						/* ioctl() */
#include <stdio.h>							/* printf() */
#include <stdlib.h>							/* free() exit() */
#include <sys/reboot.h>						/* reboot() RB_DISABLE_CAD */
#include <assert.h>

#include "initng_common.h"

/* local headers include */
#include "initng_main.h"
#include "initng_active_db.h"
#include "initng_load_module.h"
#include "initng_plugin_callers.h"
#include "initng_toolbox.h"
#include "initng_string_tools.h"
#include "initng_global.h"
#include "initng_static_states.h"
#include "initng_depend.h"
#include "initng_handler.h"
#include "initng_static_event_types.h"

/* self */
#include "initng_interrupt.h"

/*
 * when a service fails to start, this function
 * walks through all its dependencies, and mark it "dependency failed to start"
 */
static void dep_failed_to_start(active_db_h * service)
{
	/* TODO, find a way to handle this */
#ifdef NONO
	active_db_h *current = NULL;

	/* walk over all services */
	while_active_db(current)
	{
		if (current == service)
			continue;

		/* if current depends on service failed to start */
		if (initng_depend(current, service) == TRUE)
			initng_common_mark_service(current, &START_DEP_FAILED);
	}
#endif
}

#ifdef DEP_FAILED_TO_STOP
/*
 * when a service fails to stop, this function
 * walks through all its dependencies, and mark it "dependency failed to stop"
 */
static void dep_failed_to_stop(active_db_h * service)
{
	active_db_h *current = NULL;

	/* walk over all services */
	while_active_db(current)
	{
		if (current == service)
			continue;

		/* if current depends on service failed to stop */
		if (initng_active_db_dep_on(current, service) == TRUE)
			initng_common_mark_service(current, STOP_DEP_FAILED);
	}
}
#endif

static void check_sys_state_up(void)
{
	active_db_h *current = NULL;

	/* If system is not starting, we have nothing to set. */
	if (g.sys_state != STATE_STARTING)
		return;

	/* check actives, if any has one of these status, system cant be set to STATE_UP */
	while_active_db(current)
	{
		if (IS_STARTING(current))
			return;
	}

	/* OK, system is up */
	initng_main_set_sys_state(STATE_UP);
}


/*
 * This function is run from main when g.interrupt is set.
 * Will do special actions, for some status mode.
 */
static void initng_handler_run_interrupt_handlers(void)
{
	active_db_h *current, *q = NULL;

	S_;

	/* walk through all active services */
	while_active_db_safe(current, q)
	{
		assert(current->name);
		assert(current->current_state);

		/* call state handler, now when we got an g.interrupt */
		if (current->current_state->state_interrupt)
			(*current->current_state->state_interrupt) (current);
	}
}


static void handle(active_db_h * service)
{
	s_event event;
	a_state_h *state = service->current_state;

	event.event_type = &EVENT_STATE_CHANGE;
	event.data = service;

	/* lock the state, so we could know about a change */
	initng_common_state_lock(service);

	/* call all hooks again, to notify about the change */
	initng_event_send(&event);

	/* abort if the state has changed since the event was sent */
	if (initng_common_state_unlock(service))
		return;

	/* If the rough state has changed */
	if (service->last_rought_state != state->is)
	{
		D_("An is change from %i to %i for %s.\n",
		   service->last_rought_state, state->is, service->name);

		initng_common_state_lock(service);

		event.event_type = &EVENT_IS_CHANGE;
		initng_event_send(&event);

		/* abort if the state has changed since the event was sent */
		if (initng_common_state_unlock(service))
			return;

		/* This checks if all services on a runlevel is up, then set STATE_UP */
		if (IS_UP(service))
			check_sys_state_up();

		/* this will make all services, that depend of this, DEP_FAILED_TO_START */
		if (IS_FAILED(service))
		{
			dep_failed_to_start(service);
			check_sys_state_up();
		}

		/* if this service is marked restarting, please restart it if its set to STOPPED */
		if (IS_DOWN(service))
		{
			if (is(&RESTARTING, service))
			{
				initng_handler_restart_restarting();
			}
		}
		/* this will make all services, that depend of this to stop, DEP_FAILED_TO_STOP */
		/* TODO
		   if (IS_MARK(service, &FAIL_STOPPING))
		   {
		   #ifdef DEP_FAIL_TO_STOP
		   dep_failed_to_stop(service);
		   #endif
		   check_sys_state_up();
		   }
		 */

	}


	/* Run state init hook if present */
	if (service->current_state->state_init)
		(*service->current_state->state_init) (service);

	D_("service %s is now %s.\n", service->name,
	   service->current_state->state_name);
}

int initng_interrupt(void)
{
	active_db_h *service, *safe = NULL;
	int interrupt = FALSE;

	/*D_(" ** INTERRUPT **\n"); */

	/* check what services that changed state */
	while_active_db_interrupt_safe(service, safe)
	{
		/* mark as interrupt run */
		interrupt = TRUE;

		/* remove from interrupt list */
		list_del(&service->interrupt);

		/* handle this one. */
		handle(service);
	}

	/* if there was any interupt, run interupt handler hooks */
	if (interrupt)
		initng_handler_run_interrupt_handlers();

	/* return positive if any interupt was handled */
	return (interrupt);
}
