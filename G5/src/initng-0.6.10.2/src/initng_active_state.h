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

#ifndef ACTIVE_STATE_H
#define ACTIVE_STATE_H

typedef struct a_state_t a_state_h;

#include "initng_list.h"
#include "initng_active_db.h"
#include "initng_is.h"

struct a_state_t
{
	/* the name of the state in a string, will be printed */
	const char *state_name;

	/* The long description of the state */
	const char *state_desc;

	/* If this state is set for a service, is it roughly: */
	e_is is;

	/*
	 * This function will be run on service with this state set, if g.interrupt is set.
	 */
	void (*state_interrupt) (active_db_h * service);

	/*
	 * This will run directly after a service is set this state.
	 */
	void (*state_init) (active_db_h * service);

	/*
	 * This function will be run when alarm (timeout) is reached
	 */
	void (*state_alarm) (active_db_h * service);

	/* The list this struct is in */
	struct list_head list;
};

/* register */
int initng_active_state_register(a_state_h * state);

#define initng_active_state_unregister(st) list_del(&(st)->list)

/* searching */
a_state_h *initng_active_state_find(const char *state_name);

/* walking */
#define while_active_states(current) list_for_each_entry_prev(current, &g.states.list, list)
#define while_active_states_safe(current) list_for_each_entry_prev_safe(current, safe, &g.states.list, list)
#define while_active_state_hooks(current, state) list_for_each_entry_prev(current, &state->test.list, list)
#define while_active_state_hooks_safe(current, safe, state) list_for_each_entry_prev_safe(current, safe, &state->test.list, list)

#endif
