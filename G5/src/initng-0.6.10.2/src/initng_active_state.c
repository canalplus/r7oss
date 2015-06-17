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

#include <assert.h>
#include <string.h>
#include <stdlib.h>

#include "initng.h"
#include "initng_global.h"
#include "initng_active_state.h"
#include "initng_toolbox.h"


int initng_active_state_register(a_state_h * state)
{
	assert(state);

	/* look for duplicates */
	if (initng_active_state_find(state->state_name) != NULL)
	{
		F_("There exists a state with this state_name (%s) already, please check this!\n", state->state_name);
		return (FALSE);
	}

	D_("adding %s.\n", state->state_name);
	/* add this state, to the big list of states */
	list_add(&(state->list), &(g.states.list));

	/* return happily */
	return (TRUE);
}

a_state_h *initng_active_state_find(const char *state_name)
{
	a_state_h *current = NULL;

	assert(state_name);

	/* walk the state db */
	while_active_states(current)
	{
		if (strcmp(state_name, current->state_name) == 0)
			return (current);
	}
	return (NULL);
}
