/*
 * Initng, a next generation sysvinit replacement.
 * Copyright (C) 2006 Jimmy Wennlund <jimmy.wennlund@gmail.com>
 * Copyright (C) 2006 Ismael Luceno <ismael.luceno@gmail.com>
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

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <assert.h>
#include <time.h>
#include "initng.h"
#include "initng_list.h"
#include "initng_toolbox.h"
#include "initng_event_types.h"
#include "initng_event_hook.h"

int initng_event_hook_register_real(const char *from_file, s_event_type *t,
					int (*hook) (s_event * event))
{
	s_call *new_call = NULL;

	assert(hook);
	assert(t);

	D_("\n\nAdding event hook type %i from file %s\n", t,
	   from_file);

	/* allocate space for new call */
	new_call = i_calloc(1, sizeof(s_call));

	/* add data to call struct */
	new_call->from_file = i_strdup(from_file);
	new_call->c.pointer = hook;

	list_add(&new_call->list, &t->hooks.list);

	return (TRUE);
}

void initng_event_hook_unregister_real(const char *from_file,
					const char *func, int line,
					s_event_type *t,
					int (*hook) (s_event * event))
{
	s_call *current, *safe = NULL;

	assert(hook);
	assert(t);

	D_("Deleting event hook from file %s, func %s, line %i.\n", from_file,
	   func, line);

	while_list_safe(current, &t->hooks, safe)
	{
		/* make sure the pointer is right */
		if (current->c.pointer != hook)
			continue;

		list_del(&current->list);

		if (current->from_file)
			free(current->from_file);

		free(current);
		return;
	}

	F_("Could not find event hook to delete, file: %s, func:%s, line %i!!!!\n",
	   from_file, func, line);
}
