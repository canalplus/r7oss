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
#include <string.h>
#include <assert.h>
#include <errno.h>

#include <initng_handler.h>
#include <initng_global.h>
#include <initng_event_hook.h>
#include <initng_static_event_types.h>

INITNG_PLUGIN_MACRO;

s_entry NICE = { "nice", INT, NULL, "Set this nice value before executing service." };

static int do_renice(s_event * event)
{
	s_event_after_fork_data * data;

	assert(event->event_type == &EVENT_AFTER_FORK);
	assert(event->data);

	data = event->data;

	assert(data->service);
	assert(data->service->name);
	assert(data->process);

	if (is(&NICE, data->service))
	{
		D_("Will renice %s to %i !\n", data->service->name, get_int(&NICE, data->service));
		errno = 0;
		if (nice(get_int(&NICE, data->service)) == -1 && errno != 0)
		{
			F_("Failed to set the nice value: %s\n", strerror(errno));
			return (FAIL);
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

	initng_service_data_type_register(&NICE);
	return (initng_event_hook_register(&EVENT_AFTER_FORK, &do_renice));
}

void module_unload(void)
{
	D_("module_unload();\n");
	initng_service_data_type_unregister(&NICE);
	initng_event_hook_unregister(&EVENT_AFTER_FORK, &do_renice);
}
