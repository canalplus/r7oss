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
#include <alloca.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <initng_global.h>
#include <initng_service_cache.h>
#include <initng_string_tools.h>
#include <initng_open_read_close.h>
#include <initng_toolbox.h>
#include <initng_handler.h>
#include <initng_static_service_types.h>
#include <initng_service_types.h>
#include <initng_static_event_types.h>
#include <initng_event_hook.h>

#include <initng-paths.h>

#define RUNLEVEL_PATH INITNG_ROOT "/"

INITNG_PLUGIN_MACRO;

const char *module_needs[] = {
	"runlevel",
	NULL
};

stype_h *TYPE_RUNLEVEL;
stype_h *TYPE_VIRTUAL;

static service_cache_h *parse_file(char *filetoparse,
				   const char *runlevel_name, stype_h * type)
{
	service_cache_h *n_service;	/* service struct pointer too   */
	char *w = NULL;
	char *a = NULL;
	char *w_depends = NULL;

	D_("parse_file(%s, %s);\n", filetoparse, runlevel_name);

	/* allocate a new service */
	if (!(n_service = initng_service_cache_new(runlevel_name, type)))
	{
		free(filetoparse);
		return (NULL);
	}

	/* copy file filename into allocated file_buf */
	if (!open_read_close(filetoparse, &w_depends))
	{
		D_("parse_file(%s): Can't open config file!\n", filetoparse);
		if (n_service->name)
			free(n_service->name);

		/* free all data entries */
		remove_all(n_service);

		if (n_service)
			free(n_service);

		free(filetoparse);
		return (NULL);
	}
	w = w_depends;

	while (w[0] != '\0')
	{
		/* skip leading spaces */
		JUMP_SPACES(w);
		if (w[0] == '\0')
			break;

		/* skip lines, starting with '#' */
		if (w[0] == '#')
		{
			JUMP_TO_NEXT_ROW(w);
			continue;
		}

		a = st_dup_line(&w);
		if (a)
		{
			D_("adding dep: \"%s\"\n", a);
			set_another_string(&NEED, n_service, a);
		}

		JUMP_TO_NEXT_ROW(w);
	}

	free(w_depends);

	set_string(&FROM_FILE, n_service, i_strdup(filetoparse));

	free(filetoparse);

	if (initng_service_cache_register(n_service))
		return (n_service);

	/* We failed to register, so free it */
	if (n_service->name)
		free(n_service->name);

	remove_all(n_service);

	if (n_service)
		free(n_service);

	return (NULL);
}


/* a simple parser for a runlevel file */
static int initng_rl_parser(s_event * event)
{
	s_event_parse_data * data;
	char *filetoparse = NULL;
	struct stat file_stat;

	assert(event->event_type == &EVENT_PARSE);
	assert(event->data);

	data = event->data;

	assert(data->name);
	D_("initng_rl_parser(%s);", data->name);

	/* make sure service type RUNLEVEL is set */
	if (!TYPE_RUNLEVEL)
	{
		TYPE_RUNLEVEL = initng_service_type_get_by_name("runlevel");
		if (!TYPE_RUNLEVEL)
		{
			F_("ERROR, runlevel servicetype is not found, make sure runlevel plugin is loaded.\n");
			return (FALSE);
		}
	}

	if (!TYPE_VIRTUAL)
	{
		TYPE_VIRTUAL = initng_service_type_get_by_name("virtual");
		if (!TYPE_VIRTUAL)
		{
			F_("ERROR, virtual servicetype is not found, make sure runlevel plugin is loaded.\n");
			return (FALSE);
		}
	}

	filetoparse = (char *) i_calloc(sizeof(RUNLEVEL_PATH) + strlen(data->name) + 9, sizeof(char));

	/* check /etc/initng/runlevel/name.virtual */
	strcpy(filetoparse, RUNLEVEL_PATH);
	strcat(filetoparse, data->name);
	strcat(filetoparse, ".virtual");

	if (stat(filetoparse, &file_stat) == 0 && S_ISREG(file_stat.st_mode))
	{
		data->ret = parse_file(filetoparse, data->name, TYPE_VIRTUAL);
		return (HANDLED);
	}

	/* check /etc/initng/runlevel/name.runlevel */
	strcpy(filetoparse, RUNLEVEL_PATH);
	strcat(filetoparse, data->name);
	strcat(filetoparse, ".runlevel");

	if (stat(filetoparse, &file_stat) == 0 && S_ISREG(file_stat.st_mode))
	{
		data->ret = parse_file(filetoparse, data->name, TYPE_RUNLEVEL);
		return (HANDLED);
	}

	free(filetoparse);
	return (FALSE);
}

int module_init(int api_version)
{
	/* initziate globals */
	TYPE_RUNLEVEL = NULL;

	D_("initng_rl_parser: module_init();\n");
	if (api_version != API_VERSION)
	{
		F_("This module is compiled for api_version %i version and initng is compiled on %i version, won't load this module!\n", API_VERSION, api_version);
		return (FALSE);
	}

	return (initng_event_hook_register(&EVENT_PARSE, &initng_rl_parser));
}

void module_unload(void)
{
	initng_event_hook_unregister(&EVENT_PARSE, &initng_rl_parser);
}
