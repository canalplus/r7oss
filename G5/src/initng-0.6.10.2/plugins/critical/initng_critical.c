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
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>							/* free() exit() */
#include <string.h>
#include <assert.h>

#include <initng_handler.h>
#include <initng_global.h>
#include <initng_common.h>
#include <initng_toolbox.h>
#ifdef SERVICE_CACHE
#include <initng_service_cache.h>
#endif
#include <initng_main.h>
#include <initng_active_state.h>
#include <initng_static_states.h>
#include <initng_static_event_types.h>
#include <initng_event_hook.h>

INITNG_PLUGIN_MACRO;

s_entry CRITICAL = { "critical", SET, NULL,
	"If this option is set, and service doesn't succeed, initng will quit and offer a sulogin."
};

/* returns TRUE if all use deps are started */
static int check_critical(s_event * event)
{
	active_db_h * service;

	assert(event->event_type == &EVENT_IS_CHANGE);
	assert(event->data);

	service = event->data;

	assert(service->name);

	if (!IS_FAILED(service))
		return (TRUE);

	if (!is(&CRITICAL, service))
		return (TRUE);

	F_("Service %s failed, this is critical, going su_login!!\n",
	   service->name);

	initng_main_su_login();
	/* now try to reload service from disk and run it again */

#ifdef SERVICE_CACHE
	if (service->from_service)
	{
		list_del(&(service->from_service->list));
		initng_service_cache_free(service->from_service);
		service->from_service = NULL;
	}
#endif

	/* Reset the service state */
	initng_common_mark_service(service, &NEW);

	/* start the service again */
	initng_handler_start_service(service);

	/* Make sure full runlevel starting fine */
	if (!initng_active_db_find_by_exact_name(g.runlevel))
		if (!initng_handler_start_new_service_named(g.runlevel))
			F_("runlevel \"%s\" could not be executed!\n", g.runlevel);

	return (FALSE);
}

int module_init(int api_version)
{
	D_("module_init();\n");
	if (api_version != API_VERSION)
	{
		F_("This module is compiled for api_version %i version and initng is compiled on %i version, won't load this module!\n", API_VERSION, api_version);
		return (FALSE);
	}

	initng_service_data_type_register(&CRITICAL);
	initng_event_hook_register(&EVENT_IS_CHANGE, &check_critical);
	return (TRUE);
}

void module_unload(void)
{
	D_("module_unload();\n");
	initng_service_data_type_unregister(&CRITICAL);
	initng_event_hook_unregister(&EVENT_IS_CHANGE, &check_critical);
}
