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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <initng_handler.h>
#include <initng_global.h>
#include <initng_toolbox.h>
#include <initng_open_read_close.h>
#include <initng_string_tools.h>
#include <initng_active_db.h>
#include <initng_env_variable.h>
#include <initng_static_event_types.h>
#include <initng_event_hook.h>

INITNG_PLUGIN_MACRO;

s_entry ENV_FILE = { "env_file", STRINGS, NULL,
	"Parse this file for environmental variables."
};
s_entry ENV_FILE_REQUIRED = { "env_file_required", STRINGS, NULL,
	"Same as env_file, but service will fail if the file doesn't exist."
};

static int parse_file(const char *file, service_cache_h * s);

/* Parse all the env_file's when service finished loading */
static int env_parser(s_event * event)
{
	service_cache_h * s;
	const char *file = NULL;
	s_data *itt = NULL;

	assert(event->event_type == &EVENT_ADDITIONAL_PARSE);
	assert(event->data);

	s = event->data;

	D_("env_parser(%s)\n", s->name);

	/* TODO, put this into one while loop */


	/* Parse all ENV_FILE_REQUIREDs, and stop if files don't exits, or
	   not parseable */
	while ((file = get_next_string(&ENV_FILE_REQUIRED, s, &itt)))
	{
		if (parse_file(file, s) == FALSE)
			return (FAIL);
	}

	/* Parse all ENV_FILE's */
	while ((file = get_next_string(&ENV_FILE, s, &itt)))
	{
		parse_file(file, s);
	}
	itt = NULL;

	return (TRUE);
}

static int parse_file(const char *file_to, service_cache_h * s)
{

	char *file_content = NULL;
	char *point = NULL;

	char *file = fix_variables2(file_to, s);

	if (!file)
		return (FALSE);

	D_("parse_file(%s, %s);\n", file, s->name);

	/* open that file */
	if (!open_read_close(file, &file_content))
	{
		D_("Unable to parse file %s\n", file);
		fix_free(file, file_to);
		return (FALSE);
	}


	/* Start Parsing */
	point = file_content;
	while (point[0])
	{
		char *vn = NULL;		/* env variable name  */
		char *vv = NULL;		/* env variable value */

		int i = 0;
		int quoted = FALSE;

		/* skip initial spaces */
		JUMP_SPACES(point);
		if (!point[0])
			break;

		/* skip rows, starting with '#' */
		if (point[0] == '#')
		{
			JUMP_TO_NEXT_ROW(point);
			continue;
		}

		/* make sure the line contains (and doesn't begin with) a '=', if not, its not an ENV line */
		i = 0;
		while (point[i] && point[i] != '=' && point[i] != '\n')
			i++;
		if (point[i] != '=' || i == 0)
		{
			JUMP_TO_NEXT_ROW(point);
			continue;
		}

		/* skip ending spaces and '=' char */
		while (point[i - 1] == ' ' || point[i - 1] == '\t')
			i--;

		/* copy that vn */
		vn = i_strndup(point, i);

		point += i;

		/* jump any spaces and '=' char */
		JUMP_SPACES(point);
		if (point[0] == '=')
			point++;
		JUMP_SPACES(point);

		i = 0;
		while (point[i] && (quoted != FALSE || point[i] != '\n'))
		{
			if (point[i] == '"' || point[i] == '\'')
			{

				if (quoted == FALSE)
					quoted = point[i];
				else if (quoted == point[i])
					quoted = FALSE;

				if (i == 0)
				{
					point++;
					continue;
				}

				if (vv)
				{
					vv = i_realloc(vv, sizeof(char) * (strlen(vv) + i + 1));
					strncat(vv, point, i);
				}
				else
					vv = i_strndup(point, i);

				point += (i + 1);
				i = 0;
			}
			else
				i++;
		}

		/* copy the variable value */
		if (vv && i > 0)
		{
			vv = i_realloc(vv, sizeof(char) * (strlen(vv) + i));
			strncat(vv, point, i);
		}
		else if (i > 0)
			vv = i_strndup(point, i);
		else if (!vv)
		{
			/* empty value. make sure vv contains at least '\0' */
			vv = i_calloc(1, sizeof(char));
			vv[0] = '\0';
		}

		point += i;

		/* add to service cache */
		if (is_var(&ENV, vn, s))
		{
			const char *oldval = get_string_var(&ENV, vn,
												s);
			char *fixed = fix_redefined_variable(vn, oldval, vv);

			free(vv);
			set_string_var(&ENV, vn, s, fixed);
		}
		else
		{
			set_string_var(&ENV, vn, s, vv);
		}

		/* go to next row and parse */
		JUMP_TO_NEXT_ROW(point);
	}
	free(file_content);

	D_("parse_file(%s, %s): Return TRUE\n", file, s->name);

	fix_free(file, file_to);
	return (TRUE);
}

int module_init(int api_version)
{
	if (api_version != API_VERSION)
	{
		F_("This module is compiled for api_version %i version and initng is compiled on %i version, won't load this module!\n", API_VERSION, api_version);
		return (FALSE);
	}
	D_("module_init();\n");

	initng_service_data_type_register(&ENV_FILE);
	initng_service_data_type_register(&ENV_FILE_REQUIRED);
	return (initng_event_hook_register
			(&EVENT_ADDITIONAL_PARSE, &env_parser));
}

void module_unload(void)
{
	D_("module_unload();\n");
	initng_service_data_type_unregister(&ENV_FILE);
	initng_service_data_type_unregister(&ENV_FILE_REQUIRED);
	initng_event_hook_unregister(&EVENT_ADDITIONAL_PARSE, &env_parser);
}
