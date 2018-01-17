/*
 * Initng, a next generation sysvinit replacement.
 * Copyright (C) 2006 Elan Ruusamäe <glen@pld-linux.org>
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

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <assert.h>

#include <initng_handler.h>
#include <initng_global.h>
#include <initng_common.h>
#include <initng_depend.h>
#include <initng_toolbox.h>
#include <initng_static_data_id.h>
#include <initng_static_states.h>
#include <initng_static_service_types.h>
#include <initng_static_event_types.h>
#include <initng_event_hook.h>

INITNG_PLUGIN_MACRO;

s_entry LOCKFILE = { "lockfile", SET, NULL,
	"If this option is set, plugin will create /var/lock/subsys/$NAME lockfile."
};

#define LOCKDIR "/var/lock/subsys/"

static int status_change(s_event * event)
{
	active_db_h * service;

	assert(event->event_type == &EVENT_IS_CHANGE);
	assert(event->data);

	service = event->data;

	D_("status change [%s]\n", service->name);

	// are we under influence of lockfile?
	if (is(&LOCKFILE, service))
	{
		char *p = strrchr(service->name, '/') + 1;
		char lockfile[sizeof(LOCKDIR) + strlen(p)];

		strcpy(mempcpy(lockfile, LOCKDIR, sizeof(LOCKDIR) - 1), p);

		D_("lockfile path [%s]\n", lockfile);
		// service states from initng_is.h
		if (IS_UP(service))
		{
			int fd;

			D_("service got up\n");
			fd = creat(lockfile, 0640);
			if (fd != -1)
			{
				close(fd);
			}
		}
		else if (IS_DOWN(service))
		{
			D_("service went down\n");
			unlink(lockfile);
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

	initng_service_data_type_register(&LOCKFILE);
	initng_event_hook_register(&EVENT_IS_CHANGE, &status_change);
	return (TRUE);
}

void module_unload(void)
{
	D_("module_unload();\n");
	initng_service_data_type_unregister(&LOCKFILE);
	initng_event_hook_unregister(&EVENT_IS_CHANGE, &status_change);
}
