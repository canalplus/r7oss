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
#include <stdlib.h>
#include <sys/poll.h>
#include <fcntl.h>
#include <string.h>
#include <time.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <assert.h>

#include <initng_global.h>
#include <initng_process_db.h>
#include <initng_service_cache.h>
#include <initng_handler.h>
#include <initng_active_db.h>
#include <initng_toolbox.h>
#include <initng_load_module.h>
#include <initng_plugin_callers.h>
#include <initng_global.h>
#include <initng_error.h>
#include <initng_depend.h>
#include <initng_string_tools.h>
#include <initng_static_service_types.h>

INITNG_PLUGIN_MACRO;

const char *module_needs[] = {
	"runlevel",
	NULL
};

static int cmd_stop_unneeded(char *arg);
s_command STOP_UNNEEDED = { 'y', "stop_unneeded", TRUE_OR_FALSE_COMMAND,
	STANDARD_COMMAND, NO_OPT,
	{(void *) &cmd_stop_unneeded},
	"Stop all services, not in runlevel"
};

static int cmd_stop_unneeded(char *arg)
{

	int needed = FALSE;
	active_db_h *A, *As = NULL;
	active_db_h *B = NULL;
	stype_h *TYPE_RUNLEVEL = initng_service_type_get_by_name("runlevel");

	S_;

	/* walk through all and check */
	while_active_db_safe(A, As)
	{
		if (A->type == TYPE_RUNLEVEL)
			continue;

		/* reset variables */
		needed = FALSE;
		B = NULL;

		while_active_db(B)
		{
			/* don't check ourself */
			if (A == B)
				continue;

			/* if B needs A */
			if (initng_depend(B, A) == TRUE)
			{
				needed = TRUE;
				break;
			}
		}

		/* if there was no needed this service, stop it */
		if (needed == FALSE)
			initng_handler_stop_service(A);
	}

	return (TRUE);
}



int module_init(int api_version)
{
	D_("module_init(unneeded);\n");
	if (api_version != API_VERSION)
	{
		F_("This module is compiled for api_version %i version and initng is compiled on %i version, won't load this module!\n", API_VERSION, api_version);
		return (FALSE);
	}

	initng_command_register(&STOP_UNNEEDED);

	D_("libunneeded.so.0.0 loaded!\n");
	return (TRUE);
}


void module_unload(void)
{
	D_("module_unload(unneeded);\n");

	initng_command_unregister(&STOP_UNNEEDED);


	D_("libunneeded.so.0.0 unloaded!\n");

}
