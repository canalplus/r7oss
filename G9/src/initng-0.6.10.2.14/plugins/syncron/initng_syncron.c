/*
 * Initng, a next generation sysvinit replacement.
 * Copyright (C) 2006 Jimmy Wennlund <jimmy.wennlund@gmail.com>
 * Copyright (C) 2005 neuron <neuron@hollowtube.mine.nu>
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
#include <initng_static_event_types.h>
#include <initng_event_hook.h>

INITNG_PLUGIN_MACRO;

const char *module_needs[] = {
	"service",
	NULL
};

s_entry SYNCRON = { "syncron", STRING, NULL,
	"All services with this same syncron string, can't be started asynchronous."
};

a_state_h *SERVICE_START_RUN;
static int check;

static int resolv_SSR(void)
{
	SERVICE_START_RUN = initng_active_state_find("SERVICE_START_RUN");
	if (!SERVICE_START_RUN)
	{
		F_("Could not resolve SERVICE_START_RUN, is service type loaded?");
		return (FALSE);

	}
	return (TRUE);
}


/* Make sure if service syncron=module_loading, only one of the services with module_loading runs at once */
static int check_syncronicly_service(s_event * event)
{
	active_db_h * service;
	active_db_h *current, *q = NULL;
	const char *service_syncron;
	const char *current_syncron;

	assert(event->event_type == &EVENT_START_DEP_MET);
	assert(event->data);

	service = event->data;

	assert(service->name);

	/* we must have this state resolve, to compare it */
	if (!resolv_SSR())
		return (TRUE);

	if ((service_syncron = get_string(&SYNCRON, service)) == NULL)
		return (TRUE);

	while_active_db_safe(current, q)
	{
		/* don't check ourself */
		if (current == service)
			continue;

		/* If this service has the start process running */
		if (IS_MARK(current, SERVICE_START_RUN))
		{
			if ((current_syncron = get_string(&SYNCRON, current)) != NULL)
			{
				if (strcmp(service_syncron, current_syncron) == 0)
				{
					D_("Service %s has to wait for %s\n", service->name,
					   current->name);
					/* refuse to change status */
					return (FAIL);
				}
			}
		}
	}
	return (TRUE);
}

/* Make sure there is only one service starting */
static int check_syncronicly(s_event * event)
{
	active_db_h * service;
	active_db_h *current, *q = NULL;

	assert(event->event_type == &EVENT_START_DEP_MET);
	assert(event->data);

	service = event->data;

	/* we must have this state resolve, to compare it */
	if (!resolv_SSR())
		return (TRUE);

	while_active_db_safe(current, q)
	{
		/* don't check ourself */
		if (current == service)
			continue;

		if (IS_MARK(service, SERVICE_START_RUN))
		{
			/* no i cant set this status yet */
			return (FAIL);
		}
	}

	return (TRUE);
}

int module_init(int api_version)
{
	int i;

	SERVICE_START_RUN = NULL;

	D_("module_init();\n");
	if (api_version != API_VERSION)
	{
		F_("This module is compiled for api_version %i version and initng is compiled on %i version, won't load this module!\n", API_VERSION, api_version);
		return (FALSE);
	}

	initng_service_data_type_register(&SYNCRON);
	for (i = 0; g.Argv[i]; i++)
		if (strstr(g.Argv[i], "synchronously"))
		{
			check = TRUE;
			initng_event_hook_register(&EVENT_START_DEP_MET,
										&check_syncronicly);

			return (TRUE);
		}
	check = FALSE;
	/* Notice this is only added if we don't have --synchronously */
	D_("Adding synchron\n");
	initng_event_hook_register(&EVENT_START_DEP_MET,
								&check_syncronicly_service);


	return (TRUE);
}

void module_unload(void)
{

	if (check == TRUE)
		initng_event_hook_unregister(&EVENT_START_DEP_MET, &check_syncronicly);
	initng_event_hook_unregister(&EVENT_START_DEP_MET,
								  &check_syncronicly_service);
	initng_service_data_type_unregister(&SYNCRON);
}
