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

#include <initng.h>

#include <stdio.h>
#include <string.h>							/* strstr() */
#include <stdlib.h>							/* free() exit() */
#include <assert.h>

#include <initng_handler.h>
#include <initng_global.h>
#include <initng_common.h>
#include <initng_toolbox.h>
#include <initng_static_states.h>
#include <initng_env_variable.h>
#include <initng_static_event_types.h>
#include <initng_event_hook.h>

INITNG_PLUGIN_MACRO;

s_entry CONFLICT = { "conflict", STRINGS, NULL,
	"If service put here is starting or running, bail out."
};

a_state_h CONFLICTING = { "FAILED_BY_CONFLICT", "There is a running service that is conflicting with this service, initng cannot launch this service.", IS_FAILED, NULL, NULL, NULL };

static int check_conflict(s_event * event)
{
	active_db_h * service;
	const char *conflict_entry = NULL;
	s_data *itt = NULL;

	assert(event->event_type == &EVENT_IS_CHANGE);
	assert(event->data);

	service = event->data;

	assert(service->name);

	/* Do this check when this service is put in a STARTING state */
	if (!IS_STARTING(service))
		return (TRUE);

	/* make sure the conflict entry is set */
	while ((conflict_entry = get_next_string(&CONFLICT, service, &itt)))
	{
		active_db_h *s = NULL;

		/*D_("Making sure that %s is not running.\n", conflict_entry); */
		char *fixed = fix_variables(conflict_entry, service);

		s = initng_active_db_find_by_name(fixed);

		fix_free(fixed, conflict_entry);
		/* this is actually good */
		if (!s)
		{
			/*D_("Conflict not found!\n"); */
			continue;
		}

		if (IS_UP(s) || IS_STARTING(s))
		{
			initng_common_mark_service(service, &CONFLICTING);
			F_("Service \"%s\" is conflicting with service \"%s\"!\n",
			   service->name, s->name);
			return (FALSE);
		}
	}
	return (TRUE);
}

int module_init(int api_version)
{
	D_("module_init();\n");
	if (api_version != API_VERSION)
	{
		F_("This module is compiled for api_version %i version and initng is compiled on %i version, won't load this module!\n", API_VERSION, api_version);
		return (FALSE);
	}

	initng_service_data_type_register(&CONFLICT);
	initng_event_hook_register(&EVENT_IS_CHANGE, &check_conflict);
	initng_active_state_register(&CONFLICTING);

	return (TRUE);
}

void module_unload(void)
{

	initng_event_hook_unregister(&EVENT_IS_CHANGE, &check_conflict);
	initng_active_state_unregister(&CONFLICTING);
	initng_service_data_type_unregister(&CONFLICT);
}
