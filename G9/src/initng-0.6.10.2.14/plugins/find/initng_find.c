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
#include <errno.h>
#include <dirent.h>

#include <initng_global.h>
#include <initng_string_tools.h>
#include <initng_service_cache.h>
#include <initng_open_read_close.h>
#include <initng_toolbox.h>
#include <initng_plugin_callers.h>
#include <initng_common.h>
#include <initng_static_event_types.h>
#include <initng_event_hook.h>

#include <initng-paths.h>

INITNG_PLUGIN_MACRO;

static char *get_find_alias(const char *from);

static service_cache_h *search_dir(const char *for_service, const char *dir)
{
	service_cache_h *tmp = NULL;
	DIR *path;
	struct dirent *dir_e;
	struct stat fstat;
	char file[101];

	/*printf("search_dir: %s for %s\n", dir, for_service); */

	/* check if there is a virtual file */

	strncpy(file, dir, 40);
	strcat(file, "/");
	strncat(file, for_service, 50);
	strcat(file, ".virtual");

	if (stat(file, &fstat) == 0 && S_ISREG(fstat.st_mode))
		return (NULL);

	/* check if there is a runlevel file */

	strncpy(file, dir, 40);
	strcat(file, "/");
	strncat(file, for_service, 50);
	strcat(file, ".runlevel");

	if (stat(file, &fstat) == 0 && S_ISREG(fstat.st_mode))
		return (NULL);

	/* ok, there was not :) */

	path = opendir(dir);
	if (!path)
		return (NULL);

	/* Walk thru all files in dir */
	while ((dir_e = readdir(path)))
	{
		/* skip dirs/files starting with a . */
		if (dir_e->d_name[0] == '.')
			continue;


		/* set up full path */
		strncpy(file, dir, 40);
		strcat(file, "/");
		strcat(file, dir_e->d_name);



		/* get the stat of that file */
		if (stat(file, &fstat) != 0)
		{
			printf("File %s failed stat errno: %s\n", file, strerror(errno));
			continue;
		}

		/* if it is a dir */
		if (S_ISDIR(fstat.st_mode))
		{
			/* Cal ur self function with that dir */
			tmp = search_dir(for_service, file);
			if (tmp)
				return (tmp);

			/* continue while loop */
			continue;
		}

		/* if it is a file */
		if (S_ISREG(fstat.st_mode))
		{
			/* check if it contains the keyword we are looking for */
			if (!strstr(file, for_service))
				continue;

			{
				/* cut the initiating "/etc/initng/" */
				char *s = file + ((strlen(INITNG_ROOT) + 1) * sizeof(char));

				/* cut the ending ".i" */
				int i = 0;

				while (s[i] && s[i] != '.')
					i++;
				s[i] = '\0';

				/*
				 * this was a bug, because initng_common_parse_service calls this function,
				 * this could easy be an cirular bug,
				 * for now we check so that the new parse word is not the same as for_service
				 * that was orginally looking for.
				 */
				if (strcmp(s, for_service) != 0)
				{
					D_("Service %s should be \"%s\"\n", s, for_service);
					if ((tmp = initng_common_parse_service(s)))
					{
						closedir(path);
						return (tmp);
					}
				}
			}
		}
	}
	closedir(path);
	return (NULL);
}

/* Load a service from a process_name or process_path */
static int initng_find(s_event * event)
{
	/* means that it may not be a direct service request, then we start looking in subfolders */
	service_cache_h *tmp = NULL;
	s_event_parse_data * data;

	assert(event->event_type == &EVENT_PARSE);
	assert(event->data);

	data = event->data;

	assert(data->name);

	/* Try get by alias file */
	{
		char *alias_name = NULL;

		/* try get from alias file */
		if ((alias_name = get_find_alias(data->name)))
		{
			/* also try to find a service with that name */
			tmp = initng_common_parse_service(alias_name);
			free(alias_name);

			if (tmp)
			{
				data->ret = tmp;
				return (HANDLED);
			}
		}
	}

	/* Never try to find a service with a '/' in the name, it already have a path */
	if (strstr(data->name, "/"))
	{
		D_("This is a full path, nothing to search on.\n");
		return (TRUE);
	}

	/* browse initng root, searching for a file matching service name */
	if ((tmp = search_dir(data->name, INITNG_ROOT)))
	{
		data->ret = tmp;
		return (HANDLED);
	}

	return (FALSE);
}

#define ALIAS_FILE  INITNG_PLUGIN_DIR "/service_alias"
static char *get_find_alias(const char *from)
{
	int from_len = 0;
	int i = 0;

	char *file_content = NULL;
	char *point = NULL;
	char *point_result = NULL;
	char *ret = NULL;

	D_("Finding alias for %s\n", from);
	/* open that file */
	if (!open_read_close(ALIAS_FILE, &file_content))
		return (NULL);

	/* get length of what we are searching */
	from_len = strlen(from);

	/* Start Parsing */
	point = file_content;
	while (point[0])
	{
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

		/* check if first word is a match */
		if (strncasecmp(point, from, from_len) != 0)
		{
			JUMP_TO_NEXT_ROW(point);
			continue;
		}

		if (!point[from_len] || !point[from_len + 1]
			|| point[from_len] != '=')
		{
			JUMP_TO_NEXT_ROW(point);
			continue;
		}

		/* Put pointer to result after the '=' */
		point_result = point + from_len + 1;

		while (point_result[i] && point_result[i] != '\n')
			i++;

		/* copy the rest of line */
		ret = i_strndup(point_result, i);

		free(file_content);
		return (ret);
	}
	free(file_content);
	return (NULL);
}


int module_init(int api_version)
{

	D_("module_init();\n");
	if (api_version != API_VERSION)
	{
		F_("This module is compiled for api_version %i version and initng is compiled on %i version, won't load this module!\n", API_VERSION, api_version);
		return (FALSE);
	}

	return (initng_event_hook_register(&EVENT_PARSE, &initng_find));

}

void module_unload(void)
{
	D_("module_unload();\n");
	initng_event_hook_unregister(&EVENT_PARSE, &initng_find);
}
