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
#include <sys/time.h>
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
#ifdef SERVICE_CACHE
#include <initng_service_cache.h>
#endif
#include <initng_handler.h>
#include <initng_active_db.h>
#include <initng_toolbox.h>
#include <initng_load_module.h>
#include <initng_plugin_callers.h>
#include <initng_global.h>
#include <initng_error.h>
#include <initng_string_tools.h>
#include <initng_static_states.h>
#include <initng_depend.h>
#include <initng_main.h>
#include <initng_static_event_types.h>
#include <initng_event_hook.h>

#include "print_service.h"


static char *cmd_print_fds(char *arg);
static int cmd_initng_quit(char *arg);

#ifdef DEBUG
static int cmd_toggle_verbose(char *arg);
static int cmd_add_verbose(char *arg);
static int cmd_del_verbose(char *arg);
#endif

s_command LIST_FDS = { 'I', "list_filedescriptors", STRING_COMMAND, ADVANCHED_COMMAND, USES_OPT, {(void *) &cmd_print_fds}, "Print all open filedescriptors initng have." };

s_command QUIT_INITNG = { 'q', "quit", TRUE_OR_FALSE_COMMAND, ADVANCHED_COMMAND, NO_OPT,
	{(void *) &cmd_initng_quit},
	"Quits initng"
};


#ifdef SERVICE_CACHE
s_command PRINT_SERVICE_DB = { 'P', "print_service_db", STRING_COMMAND, ADVANCHED_COMMAND,
	USES_OPT,
	{(void *) &service_db_print_all},
	"Print service_db"
};
#endif

s_command PRINT_ACTIVE_DB = { 'p', "print_active_db", STRING_COMMAND, ADVANCHED_COMMAND, USES_OPT,
	{(void *) &active_db_print_all},
	"Print active_db"
};

#ifdef DEBUG
s_command TOGGLE_VERBOSE = { 'v', "verbose", TRUE_OR_FALSE_COMMAND, ADVANCHED_COMMAND, NO_OPT,
	{(void *) &cmd_toggle_verbose},
	"Toggle the verbose flag - ONLY FOR DEBUGGING"
};
s_command ADD_VERBOSE = { 'i', "add_verbose", TRUE_OR_FALSE_COMMAND, ADVANCHED_COMMAND,
	REQUIRES_OPT,
	{(void *) &cmd_add_verbose},
	"Add string to watch for to make initng verbose - ONLY FOR DEBUGGING"
};
s_command DEL_VERBOSE = { 'k', "del_verbose", TRUE_OR_FALSE_COMMAND, ADVANCHED_COMMAND,
	REQUIRES_OPT,
	{(void *) &cmd_del_verbose},
	"Del string to watch for to make initng verbose - ONLY FOR DEBUGGING"
};
#endif

static char *cmd_print_fds(char *arg)
{
	char *string = NULL;
	active_db_h *currentA;
	process_h *currentP;
	pipe_h *current_pipe;
	int i;

	{
		s_event event;
		s_event_fd_watcher_data data;

		event.event_type = &EVENT_FD_WATCHER;
		event.data = &data;
		data.action = FDW_ACTION_DEBUG;
		data.debug_find_what = arg;
		data.debug_out = &string;

		initng_event_send(&event);
	}

	for (i = 0; i < 1024; i++)
	{
		currentA = NULL;

		/* for every service */
		while_active_db(currentA)
		{
			/* if argument was set, only print matching */
			if (!arg || service_match(currentA->name, arg))
			{
				/* for every process */
				currentP = NULL;
				while_processes(currentP, currentA)
				{
					/* for every pipe */
					current_pipe = NULL;
					while_pipes(current_pipe, currentP)
					{
						/* if matching */
						if (current_pipe->pipe[0] == i
							|| current_pipe->pipe[1] == i)
						{
							/* PRINT */
							mprintf(&string,
									" %i: Used service: %s, process: %s\n", i,
									currentA->name, currentP->pt->name);
						}
					}
				}
			}
		}


		/*mprintf(&string, " %i:\n", i); */
	}
	return (string);
}

static int cmd_initng_quit(char *arg)
{
	(void) arg;
	g.when_out = THEN_QUIT;
	initng_handler_stop_all();
	return (TRUE);
}



#ifdef DEBUG


static int cmd_toggle_verbose(char *arg)
{
	(void) arg;
	switch (g.verbose)
	{
		case 0:
			g.verbose = 1;
			break;
		case 1:
			g.verbose = 0;
			break;
		case 2:
			g.verbose = 3;
			break;
		case 3:
			g.verbose = 2;
			break;
		default:
			g.verbose = 0;
			W_("Unknown verbose id %i\n", g.verbose);
			break;
	}
	return (g.verbose);
}

static int cmd_add_verbose(char *arg)
{
	if (!arg)
		return (FALSE);

	return (initng_error_verbose_add(arg));
}

static int cmd_del_verbose(char *arg)
{
	if (!arg)
		return (FALSE);

	return (initng_error_verbose_del(arg));
}
#endif

int module_init(int api_version)
{
	D_("module_init(stcmd);\n");
	if (api_version != API_VERSION)
	{
		F_("This module is compiled for api_version %i version and initng is compiled on %i version, won't load this module!\n", API_VERSION, api_version);
		return (FALSE);
	}

	initng_command_register(&LIST_FDS);
	if (g.i_am == I_AM_FAKE_INIT)
	{
		initng_command_register(&QUIT_INITNG);
	}
#ifdef SERVICE_CACHE
	initng_command_register(&PRINT_SERVICE_DB);
#endif
	initng_command_register(&PRINT_ACTIVE_DB);

#ifdef DEBUG
	initng_command_register(&TOGGLE_VERBOSE);
	initng_command_register(&ADD_VERBOSE);
	initng_command_register(&DEL_VERBOSE);
#endif


	D_("libstcmd.so.0.0 loaded!\n");
	return (TRUE);
}


void module_unload(void)
{
	D_("module_unload(stcmd);\n");

	initng_command_unregister(&LIST_FDS);
	if (g.i_am == I_AM_FAKE_INIT)
	{
		initng_command_unregister(&QUIT_INITNG);
	}
#ifdef SERVICE_CACHE
	initng_command_unregister(&PRINT_SERVICE_DB);
#endif
	initng_command_unregister(&PRINT_ACTIVE_DB);
#ifdef DEBUG
	initng_command_unregister(&TOGGLE_VERBOSE);
	initng_command_unregister(&ADD_VERBOSE);
	initng_command_unregister(&DEL_VERBOSE);
#endif


	D_("libstcmd.so.0.0 unloaded!\n");

}
