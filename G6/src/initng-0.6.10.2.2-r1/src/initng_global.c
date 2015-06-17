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

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "initng.h"
#include "initng_global.h"
#include "initng_load_module.h"
#include "initng_toolbox.h"
#include "initng_static_states.h"
#include "initng_handler.h"
#include "initng_static_service_types.h"
#include "initng_static_event_types.h"

/* HERE IS THE GLOBAL DEFINED STRUCT, WE OFTEN RELATE TO */
s_global g;
static void initng_global_parse_argv(char **argv);

/*
 * initng_global_new.
 * This function initziates the global data struct, with some standard values.
 * This must be set before libinitng can be used in any way
 */
void initng_global_new(int argc, char *argv[], char *env[])
{
	int i;

	assert(argv);
	assert(env);

#ifdef SERVICE_CACHE
	/* INITZIATE NO_CACHE_TYPE, to make sure it wont segfault when locking up data */
	DATA_HEAD_INIT(&NO_CACHE.data);
#endif

	/* zero the complete s_global */
	memset(&g, 0, sizeof(s_global));

	/* Set the i_am */
	g.i_am = I_AM_INIT;

	/* we want to keep a copy of the arguments passed to us, this will be overwritten by set_title() */
	g.Argc = argc;
	g.Argv0 = argv[0];
	g.Argv = (char **) i_calloc(argc + 1, sizeof(char *));
	assert(g.Argv);

	for (i = 0; i < argc; i++)
	{
		g.Argv[i] = i_strdup(argv[i]);
		assert(g.Argv[i]);
	}
	g.Argv[argc] = NULL;

	/*
	 * We want to change our process name, because it looks nice in
	 * "ps" output. But we can only use space that the kernel
	 * allocated for us when we started. This is argv[] and env[]
	 * together.
	 */
	g.maxproclen = 0;
	for (i = 0; i < argc; i++)
	{
		g.maxproclen += strlen(argv[i]) + 1;
	}
	for (i = 0; env[i] != NULL; i++)
	{
		g.maxproclen += strlen(env[i]) + 1;
	}
	D_("Maximum length for our process name is %d\n", g.maxproclen);

	/*
	 * initialize global data storage
	 */
	DATA_HEAD_INIT(&g.data);

	/*
	 * initialize all databases, next and prev have to point to own struct
	 */
	INIT_LIST_HEAD(&g.active_database.list);
	INIT_LIST_HEAD(&g.active_database.interrupt);
#ifdef SERVICE_CACHE
	INIT_LIST_HEAD(&g.service_cache.list);
#endif
	INIT_LIST_HEAD(&g.states.list);
	INIT_LIST_HEAD(&g.ptypes.list);
	INIT_LIST_HEAD(&g.module_db.list);
	INIT_LIST_HEAD(&g.option_db.list);
	INIT_LIST_HEAD(&g.event_db.list);
	INIT_LIST_HEAD(&g.command_db.list);
	INIT_LIST_HEAD(&g.stypes.list);

	/*
	 * default global variables - cleared by memset above
	 * g.interrupt = FALSE;
	 * g.sys_state = STATE_NULL;
	 * g.runlevel = NULL;
	 * g.dev_console = NULL;
	 * g.modules_to_unload = FALSE;
	 * g.hot_reload = FALSE;
	 */

	/* Add defaults (static) to data_types */
	initng_static_data_id_register_defaults();
	/* Add static stats to g.states */
	initng_static_states_register_defaults();
	/* Add static stypes */
	initng_service_register_static_stypes();
	/* Add static event types */
	initng_register_static_event_types();

	/* CLEARED by memset above
	   #ifdef DEBUG
	   g.verbose = 0;
	   {
	   int i;

	   for (i = 0; i < MAX_VERBOSES; i++)
	   g.verbose_this[i] = NULL;
	   }
	   #endif
	 */

	/* parse all options, set in argv */
	initng_global_parse_argv(argv);
}



static void initng_global_parse_argv(char **argv)
{
	int i;

	for (i = 0; argv[i]; i++)
	{
		/* set opt to argv */
		char *opt = argv[i];

		/* don't parse options starting with an '+' */
		if (opt[0] == '+')
			continue;
		/*
		 * if arg is --verbose, skip the two -- here, and start checking.
		 * if arg is -verbose, skip and continue for loop.
		 * if arg is verbose, check it below
		 */

		/* if its a double --, its ok */
		if (opt[0] == '-' && opt[1] == '-')
			opt += 2;
		/* but a single is not ! */
		if (opt[0] == '-')
			continue;


#define R_L "runlevel="
#define KR_L "runlevel:"
#define V_T "verbose_add="
#define C_L "console="

#ifdef DEBUG
		if (strcmp(opt, "verbose") == 0)
			g.verbose = TRUE;
#endif

		if (strcmp(opt, "hot_reload") == 0)
		{
			D_(" Will start after a hot reload ...\n");
			g.hot_reload = TRUE;
		}

		if (strcmp(opt, "fake") == 0)
			g.i_am = I_AM_FAKE_INIT;

		/* Wyplay: we always want no_circular */
		g.no_circular = TRUE;

		/* the ones with a value */
		if (strncmp(opt, R_L, strlen(R_L)) == 0)
			g.runlevel = i_strdup((opt) + strlen(R_L));
		if (!g.runlevel && strncmp(opt, KR_L, strlen(KR_L)) == 0)
			g.runlevel = i_strdup((opt) + strlen(KR_L));
		if (strncmp(opt, C_L, strlen(C_L)) == 0)
			g.dev_console = i_strdup((opt) + strlen(C_L));
#ifdef DEBUG
		if (strncmp(opt, V_T, strlen(V_T)) == 0)
			initng_error_verbose_add((opt) + strlen(V_T));
#endif
	}
}

void initng_global_free(void)
{
	int i;

	/* free all databases */
	initng_active_db_free_all();			/* clean process_db */
#ifdef SERVICE_CACHE
	initng_service_cache_free_all();		/* clean service_cache_db */
#endif
	initng_service_data_type_unregister_all();	/* clean option_db */
	initng_event_type_unregister_all();		/* clean event_db */
	initng_command_unregister_all();		/* clean command_db */

	/* free dynamic global variables */
	remove_all(&g);

	/* free runlevel name string */
	if (g.runlevel)
		free(g.runlevel);

	/* free console string if set */
	if (g.dev_console)
		free(g.dev_console);

	/* free our copy of argv, and argc */
	for (i = 0; i < g.Argc; i++)
	{
		free(g.Argv[i]);
	}
	free(g.Argv);
}
