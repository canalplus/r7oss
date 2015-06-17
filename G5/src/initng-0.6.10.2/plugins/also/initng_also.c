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
#include <stdlib.h>							/* free() exit() */
#include <string.h>
#include <assert.h>

#include <initng_handler.h>
#include <initng_global.h>
#include <initng_common.h>
#include <initng_toolbox.h>
#include <initng_static_data_id.h>
#include <initng_static_states.h>
#include <initng_env_variable.h>
#include <initng_static_event_types.h>
#include <initng_event_hook.h>

INITNG_PLUGIN_MACRO;

s_entry ALSO_START = { "also_start", STRINGS, NULL,
	"When this service is starting, also start this."
};
s_entry ALSO_STOP = { "also_stop", STRINGS, NULL,
	"When this service is stopping, also stop this."
};

static int service_state(s_event * event)
{
	active_db_h * service;
	const char *tmp = NULL;
	active_db_h *current = NULL;

	assert(event->event_type == &EVENT_IS_CHANGE);

	service = event->data;

	assert(service);
	assert(service->name);

	/* if service is loading, start all in ALSO_START */
	if (IS_STARTING(service))
	{
		tmp = NULL;
		s_data *itt = NULL;

		while ((tmp = get_next_string(&ALSO_START, service, &itt)))
		{
			char *fixed = NULL;

			fixed = fix_variables(tmp, service);
			if ((current = initng_active_db_find_by_name(fixed)))
			{
				if (!initng_handler_start_service(current))
				{
					F_("Failed to also_start %s.\n", fixed);
					free(fixed);
					continue;
				}
				D_("Service also_start %s already running.\n", fixed);
				fix_free(fixed, tmp);
				continue;
			}

			if (!initng_handler_start_new_service_named(fixed))
			{
				F_("%s also_start %s could not start!\n", service->name,
				   fixed);
				initng_handler_stop_service(service);
				fix_free(fixed, tmp);
				return (FALSE);
			}
			fix_free(fixed, tmp);
		}
		return (TRUE);
	}

	/* if this service is stopping, stop all in ALSO_STOP */
	if (IS_STOPPING(service))
	{
		/* Handle ALSO_STOP */
		tmp = NULL;
		s_data *itt = NULL;

		while ((tmp = get_next_string(&ALSO_STOP, service, &itt)))
		{
			char *fixed = NULL;

			fixed = fix_variables(tmp, service);
			if ((current = initng_active_db_find_by_name(fixed)))
			{
				/* Tell this verbose */
				D_("service %s also stops %s\n", service->name, fixed);

				if (!initng_handler_stop_service(current))
				{
					F_("Could not stop also_stop service %s\n",
					   current->name);
				}
			}
			fix_free(fixed, tmp);
		}
		return (TRUE);
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

	initng_service_data_type_register(&ALSO_START);
	initng_service_data_type_register(&ALSO_STOP);
	initng_event_hook_register(&EVENT_IS_CHANGE, &service_state);
	return (TRUE);
}

void module_unload(void)
{
	D_("module_unload();\n");
	initng_service_data_type_unregister(&ALSO_START);
	initng_service_data_type_unregister(&ALSO_STOP);
	initng_event_hook_unregister(&EVENT_IS_CHANGE, &service_state);
}
