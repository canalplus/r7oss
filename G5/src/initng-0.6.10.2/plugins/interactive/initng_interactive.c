/*
 * Initng, a next generation sysvinit replacement.
 * Copyright (C) 2005 Jimmy Wennlund <jimmy.wennlund@gmail.com>
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
#include <initng_load_module.h>
#include <initng_static_states.h>
#include <initng_static_event_types.h>
#include <initng_event_hook.h>

INITNG_PLUGIN_MACRO;

static int active;
static int interactive_STARTING(s_event * service);
static int interactive_STOP_MARKED(s_event * service);

a_state_h INT_DISABLED = { "INTERACTIVELY_DISABLED", "The user choose to not start this service.", IS_FAILED, NULL, NULL, NULL };

static int interactive_STARTING(s_event * event)
{
	active_db_h * service;
	char asw[10];

	assert(event->event_type == &EVENT_START_DEP_MET);
	assert(event->data);

	service = event->data;

	asw[0] = '\0';

	fprintf(stderr, "Start service %s, (Y/n/a):", service->name);
	/* HACK: ignore read errors by assuming 'n' */
	if (fgets(asw, 9, stdin) == NULL)
		asw[0] = 'n';

	/* if it is true, then its ok to launch service */
	if (asw[0] == 'y' || asw[0] == 'Y')
		return (TRUE);

	if (asw[0] == 'a' || asw[0] == 'A')
	{
		initng_event_hook_unregister(&EVENT_START_DEP_MET,
									  &interactive_STARTING);
		initng_event_hook_unregister(&EVENT_STOP_DEP_MET,
									  &interactive_STOP_MARKED);
		active = FALSE;
		return (TRUE);
	}

	initng_common_mark_service(service, &INT_DISABLED);
	return (FAIL);
}

static int interactive_STOP_MARKED(s_event * event)
{
	active_db_h * service;
	char asw[10];

	assert(event->event_type == &EVENT_STOP_DEP_MET);
	assert(event->data);

	service = event->data;

	asw[0] = '\0';

	fprintf(stderr, "Stop service %s, (Y/n/a):", service->name);
	/* HACK: ignore read errors by assuming 'n' */
	if (fgets(asw, 9, stdin) == NULL)
		asw[0] = 'n';

	if (asw[0] == 'y' || asw[0] == 'Y')
		return (TRUE);

	if (asw[0] == 'a' || asw[0] == 'A')
	{
		initng_event_hook_unregister(&EVENT_START_DEP_MET,
									  &interactive_STARTING);
		initng_event_hook_unregister(&EVENT_STOP_DEP_MET,
									  &interactive_STOP_MARKED);
		active = FALSE;
		return (TRUE);
	}

	initng_common_mark_service(service, &INT_DISABLED);
	return (FAIL);
}

int module_init(int api_version)
{
	int i;

	if (api_version != API_VERSION)
	{
		F_("This module is compiled for api_version %i version and initng is compiled on %i version, won't load this module!\n", API_VERSION, api_version);
		return (FALSE);
	}

	D_("module_init();\n");

	/* look for the string interactive */
	for (i = 0; g.Argv[i]; i++)
		if (strstr(g.Argv[i], "interactive"))
		{									/* if found */

			P_("Initng is started in interactive mode!\n");
			initng_event_hook_register(&EVENT_START_DEP_MET,
										&interactive_STARTING);
			initng_event_hook_register(&EVENT_STOP_DEP_MET,
										&interactive_STOP_MARKED);
			active = TRUE;
			return (TRUE);
		}

	active = FALSE;
	initng_unload_module_named("interactive");
	return (TRUE);
}

void module_unload(void)
{
	D_("module_unload();\n");
	if (active == TRUE)
	{
		initng_event_hook_unregister(&EVENT_START_DEP_MET,
									  &interactive_STARTING);
		initng_event_hook_unregister(&EVENT_STOP_DEP_MET,
									  &interactive_STOP_MARKED);
	}
}
