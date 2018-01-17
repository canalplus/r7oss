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
#define _GNU_SOURCE
#include <stdio.h>
#include <alloca.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <assert.h>
#include <sys/types.h>
#include <dirent.h>   /*#include <dirent.h> */	/* opendir() closedir() */
#include <fnmatch.h>

#include <initng_global.h>
#include <initng_string_tools.h>
#include <initng_service_cache.h>
#include <initng_open_read_close.h>
#include <initng_toolbox.h>
#include <initng_error.h>
#include <initng_handler.h>
#include <initng_static_event_types.h>
#include <initng_event_hook.h>

#include <initng-paths.h>

/* a check with escape chars check */
#define CH_(STRING, CHAR) (((*STRING)[0] == CHAR) && ((*STRING)[-1] != '\\'))
#define CHL_(STRING, LEN, CHAR) (((*STRING)[LEN] == CHAR) && ((*STRING)[LEN-1] != '\\'))
#define ENDED(x) (!(x)[0] || ((x)[0]=='\n' && (x)[-1]!='\\'))

INITNG_PLUGIN_MACRO;

const char *module_needs[] = {
	"service",
	"daemon",
	NULL
};

const char *g_filename = NULL;
const char *g_pointer = NULL;	/* a pointer to the first char we can back step to when printing line copies */


/* set this to have a really verbose walking output */
/* #define DL_(a,m) err_print_line2t(a, m, MSG, __FILE__, (const char*)__PRETTY_FUNCTION__, __LINE__) */
#define DL_(a,m)

#define FL_(a,m) err_print_line2t(a, m, MSG_FAIL, __FILE__, (const char*)__PRETTY_FUNCTION__, __LINE__)

static void err_print_line2t(char *point, const char *message, e_mt err,
							 const char *file, const char *func,
							 int codeline);
static service_cache_h *parse_file(const char *filename,
								   const char *watch_for);
static int parse_service_line(char **to_parse, const char *watch_for,
							  service_cache_h * father, stype_h * type,
							  const char *filename, service_cache_h ** match,
							  service_cache_h ** exact_match);
static int parse_opt(char **where, stype_h * type, service_cache_h * srv);

static int set_parser(s_entry * type, char **value, char *va,
					  service_cache_h * from_service);
static int string_parser(s_entry * type, char **value, char *va,
						 service_cache_h * from_service);
static int strings_parser(s_entry * type, char **value, char *va,
						  service_cache_h * from_service);
static int iint_parser(s_entry * type, char **value, char *va,
					   service_cache_h * from_service);

static char *dup_string_and_walk(char **value, int break_on_space);

static int is_valid(char *string);

static int is_valid(char *string)
{
	int i;

	for (i = 0; string[i] != 0; i++)
		if (string[i] <= 32)
			return (FALSE);

	return (TRUE);
}

static void err_print_line2t(char *point, const char *message, e_mt err,
							 const char *file, const char *func, int codeline)
{
	char *line;
	char *pstr;
	int len = 0;
	int i = 0;
	int pi = 0;
	char *start = point;

	/* make sure global filename is set */
	if (!g_pointer || !g_filename)
		return;

	/*while(start != g_pointer && start[-1] != g_pointer && start[-1] != '\n') */
	while (start != g_pointer && (start - 1)[0] != '\n')
		start--;

	while (start[len] && start[len] != '\n')
		len++;

	pi = point - start;

	/* copy line working on */
	line = i_strndup(start, len);
	pstr = i_calloc(pi + 4, sizeof(char));
	if (!line || !pstr)
		return;

	/* put spaces */
	memset(pstr, ' ', sizeof(char) * (pi));

	/* Tabs destroy our position pointers, replaces with spaces */
	for (i = 0; line[i]; i++)
		if (line[i] == '\t')
			line[i] = ' ';


	/* then, put the marker. */
	pstr[pi] = '^';
	pstr[pi + 1] = '\0';


	initng_error_print(err, file, func, codeline,
					   "%s\nfile: %s\n\"%s\"\n %s\n\n", message,
					   g_filename, line, pstr);
	free(line);
	free(pstr);
	sleep(0.2);
}


/* if path is daemon/bluetooth/serial/daemon */
static service_cache_h *test_parse(char *path, const char *service_to_find)
{
	char filetoparse[200];
	service_cache_h *got_serv = NULL;
	const char *service_strip, *path_strip = NULL;

	/* if path points to service_to_find this is the first run */
	int first = (path == service_to_find);

	/*D_("test_parse(%s, %s)\n", path, service_to_find); */

	/* get string from last '/' char */
	if ((service_strip = strrchr(service_to_find, '/')))
		service_strip++;
	else
		service_strip = service_to_find;

	/* 1, try /etc/initng/daemon/bluetooth/serial/daemon/daemon.i */
	strcpy(filetoparse, INITNG_ROOT "/");
	strcat(filetoparse, path);
	strcat(filetoparse, "/");
	strcat(filetoparse, service_strip);
	strcat(filetoparse, INITNG_EXT);
	if ((got_serv = parse_file(filetoparse, service_to_find)))
	{
		return (got_serv);
	}

	/* 2. try /etc/initng/daemon/bluetooth/serial/daemon/default.i */
	strcpy(filetoparse, INITNG_ROOT "/");
	strcat(filetoparse, path);
	strcat(filetoparse, "/default");
	strcat(filetoparse, INITNG_EXT);
	if ((got_serv = parse_file(filetoparse, service_to_find)))
	{
		return (got_serv);
	}
	/* 3, try /etc/initng/daemon/bluetooth/serial/daemon.i */
	strcpy(filetoparse, INITNG_ROOT "/");
	strcat(filetoparse, path);
	strcat(filetoparse, INITNG_EXT);
	if ((got_serv = parse_file(filetoparse, service_to_find)))
	{
		return (got_serv);
	}

	if ((path_strip = strrchr(path, '/')))
		path_strip++;
	else
		path_strip = path;

	/* 4, A last try with path_strip if differ. */
	if (strcmp(path_strip, service_strip) != 0)
	{
		strcpy(filetoparse, INITNG_ROOT "/");
		strcat(filetoparse, path);
		strcat(filetoparse, "/");
		strcat(filetoparse, path_strip);
		strcat(filetoparse, INITNG_EXT);
		if ((got_serv = parse_file(filetoparse, service_to_find)))
		{
			return (got_serv);
		}
	}

	/* If this function is not run inside this function.. */
	if (first)
	{
		path = i_strdup(service_to_find);
		if (!path)
			return (NULL);
	}

	/* Cut path on last '/' */
	char *p = NULL;

	/* replace the first '/' found when searching from last char to left, with an '\0' */
	if ((p = strrchr(path, '/')))
	{
		/* this will change path */
		p[0] = '\0';

		/* call own function again with a shorter path */
		got_serv = test_parse(path, service_to_find);

		/* if this is the first in stack, we need to free */
		if (first)
			free(path);

		/* if found, the path is freed upstream */
		return (got_serv);
	}

	/* path is only freed on succes, do it else here */
	if (first)
		free(path);

	return (NULL);
}

/* Load a service from a service_to_find or process_path */
static int initng_i_parser(s_event * event)
{
	service_cache_h * got_serv = NULL;
	s_event_parse_data * data;

	assert(event->event_type == &EVENT_PARSE);
	assert(event->data);

	data = event->data;

	assert(data->name);
	D_("Parsing for %s\n", data->name);

	/* Make sure the filename of 200 chars above are more than enough */
	if (strlen(data->name) > 50)
	{
		F_("Service name to long, initng_i_parser can't look for this service!\n");
		return (FALSE);
	}

	/*
	 * check if service to find is a filename.
	 * when searching on a full .i filename, the service
	 * returned is the first entry found in the i.file
	 */
	if (data->name[0] == '/')
	{
		/* sending null returns the first found */
		if ((data->ret = parse_file(data->name, NULL)))
			return (HANDLED);

		return (FALSE);
	}

	/*
	 * now set up path and service.
	 * if service_to_find is system/initial/udevd
	 * path should be "system/initial"
	 * and service "udevd"
	 */

	got_serv = test_parse((char *) data->name, data->name);
	if (got_serv) {
		data->ret = got_serv;
		return (HANDLED);
	}

	D_("Was not able to parse: %s\n", data->name);
	return (FALSE);
}

/* parse a file for services */
static service_cache_h *parse_file(const char *filename,
				   const char *watch_for)
{
	/* Pointer to content read, and pointer to where we are reading */
	char *file_content = NULL;
	char *file = NULL;


	/* if service is matching, it is set, if service is exact_match this is set */
	service_cache_h *match = NULL;
	service_cache_h *exact_match = NULL;

	/* standard assertions */
	assert(filename);

	/* set the error message filename */
	g_filename = filename;

	D_("parse_file(%s) for: %s\n", filename, watch_for);

	/* read config file */
	if (!open_read_close(filename, &file_content))
	{
		D_("Can't open config file %s.\n", filename);
		return (NULL);
	}
	file = file_content;
	g_pointer = file;


	/* main parse line by line loop */
	while (file[0] != '\0')
	{
		stype_h *stype = NULL;

		DL_(file, "parse_file while loop:");
		/* skip leading spaces */
		JUMP_NSPACES(file);
		if (file[0] == '\0')
		{
			DL_(file, "End of file");
			break;
		}

		/* skip lines starting with '#' */
		if (file[0] == '#' || file[0] == '\n' || file[0] == ';')
		{
			REALLY_JUMP_TO_NEXT_ROW(file);
			continue;
		}
		/*
		 * parsometer:
		 *   service test {
		 *   ^
		 */

		/* get the stype */
		if (!(stype = initng_service_type_get_by_name(file)))
		{
			match = NULL;
			break;
		}

		JUMP_TO_NEXT_WORD(file);
		/*
		 * parsometer:
		 *   service test {
		 *           ^
		 */

		/* parse line, with options from that service type */
		parse_service_line(&file, watch_for, NULL, stype,
						   filename, &match, &exact_match);

		D_("parse_service_line(%s, %s)\n", match ? match->name : NULL,
		   exact_match ? exact_match->name : NULL);

	}										/* end while */

	/* free file_content */
	if (file_content)
	{
		free(file_content);
		file_content = NULL;
	}

	/* Return what we got, and hope for the best */
	if (exact_match)
		return (exact_match);

	/* else, return any match */
	return (match);
}


static int parse_service_line(char **to_parse, const char *watch_for,
							  service_cache_h * father, stype_h * type,
							  const char *filename, service_cache_h ** match,
							  service_cache_h ** exact_match)
{
	char *name = NULL;
	char *father_name = NULL;
	service_cache_h *new_service = NULL;

#ifdef SUB_CLASS
	service_cache_h *duplicate = NULL;
	int len;
#endif

	assert(to_parse);
	assert(*to_parse);

	/* for debugging */
	DL_(*to_parse, "parse_service_line:");

	/* service test : class { */
	/*        |               */
	/* jump forward on spaces */
	JUMP_NSPACES(*to_parse);
	if (ENDED(*to_parse))
	{
		FL_(*to_parse, "Line ended unexpectedly.");
		return (FALSE);
	}

	/* service test : class { */
	/*         |              */

	/* this fetches name, and increase *to_parse */
	if (!(name = st_dup_next_word((const char **) to_parse)))
	{
		FL_(*to_parse, "Did not get a name!");
		return (FALSE);
	}

	if (!is_valid(name))
	{
		FL_(*to_parse, "Name contains invalid characters.");
		return (FALSE);
	}

	/* service test : class { */
	/*             |          */

	D_("parse_service_line(s,%s): service name: \"%s\"\n", watch_for, name);

	/* jump forward on spaces */
	JUMP_NSPACES(*to_parse);
	if (ENDED(*to_parse))
	{
		FL_(*to_parse, "Line ended unexpectedly.");
		free(name);
		return (FALSE);;
	}

	/* service test : class { */
	/*              |         */
	if (CH_(to_parse, ':'))
	{
		(*to_parse)++;
		/* service test : class { */
		/*               |        */

		/* jump forward on spaces */
		JUMP_NSPACES(*to_parse);
		if (ENDED(*to_parse))
		{
			FL_(*to_parse, "Line ended unexpectedly.");
			free(name);
			return (FALSE);
		}
		/* service test : class { */
		/*                |       */

		if (!(father_name = st_dup_next_word((const char **) to_parse)))
		{
			FL_(*to_parse, "Unable to fetch fathername.");
			free(name);
			return (FALSE);
		}

		if (!is_valid(father_name))
		{
			FL_(*to_parse, "Father name contains invalid characters.");
			return (FALSE);
		}

#ifdef DEBUG
		if (father_name)
			D_("Father is: %s, set from \"server : father\"\n", father_name);
#endif

		/* service test : class { */
		/*                     |  */
		/* jump forward on spaces */
		JUMP_NSPACES(*to_parse);
		if (ENDED(*to_parse))
		{
			FL_(*to_parse, "Line ended unexpectedly.");
			free(name);
			free(father_name);
			return (FALSE);
		}
	}

	DL_(*to_parse, "Parse opt, should stand directly over { char");
	/* service test : class { */
	/*                      | */
	if (!CH_(to_parse, '{'))
	{
		/* something is wrong */
		FL_(*to_parse, "error parsing, expected something else.");
		free(name);
		if (father_name)
			free(father_name);
		return (FALSE);
	}

	/* jump to first char after start tag and begin from there. */
	(*to_parse)++;
	JUMP_NSPACES(*to_parse);

	DL_(*to_parse, "Parse opt, should be first char in stack");


	/* service test : class { */
	/*                       | */

	new_service = initng_service_cache_new(name, type);

	/* from here, don't use name anymore, use new_service->name */
	free(name);
	name = NULL;

	/* FROM NOW, we should not free name or father_name, because its ADDED, and required */

	/* check so that it was allocated! */
	if (!new_service)
	{
		F_("Unable to allocate space for new service.\n");
		FL_(*to_parse, "Unable to allocate space for new service.");
		if (father_name)
			free(father_name);
		return (FALSE);
	}

	/* set the father to the service */
	if (father_name)
	{
		if (father)
			new_service->father = father;
		new_service->father_name = father_name;
	}

	/* set type if not set. */
	if (father)
		new_service->type = type;

	/* carry on until segment stop or eof, this will handle all lines in current section */
	while ((*to_parse)[0])
	{

		DL_(*to_parse, "parse_service_line, while loop :");

		/* skip spaces and empty lines with them */
		JUMP_NSPACES(*to_parse);

		/* end of row */
		/* TODO, make sure (*to_parse)[-1] is not before the string */
		if (CH_(to_parse, ';'))
		{
			(*to_parse)++;
			continue;
		}

		/* end of file or stack */
		if (!(*to_parse)[0] || CH_(to_parse, '}'))
		{
			DL_(*to_parse,
				"When escaping from parse_service_line while loop");
			break;
		}

		/* skip lines starting with '#' */
		if (CH_(to_parse, '#'))
		{
			REALLY_JUMP_TO_NEXT_ROW(*to_parse);
			continue;
		}


		DL_(*to_parse, "parse_service_line, on first option char:");

		/* parse line - search for keywords in g.option_table */
		if (parse_opt(to_parse, type, new_service))
		{
			if (!CH_(to_parse, ';'))
			{
				FL_(*to_parse, "Must be a ';' char here");
				return (FALSE);
			}

			/* jump the ';' char */
			(*to_parse)++;
			continue;
		}

		/* Bad content, could not be parsed */
		/* free and reset */
		initng_service_cache_free(new_service);
		new_service = NULL;
		return (FALSE);
	}										/* while */

	/* make sure the while loop only stoped on a break */
	if (!(*to_parse)[0])
	{
		FL_(*to_parse, "Service file did end unexpected.\n");
		return (FALSE);
	}

	/* skip the '}' char we must stand on */
	(*to_parse)++;

	set_string(&FROM_FILE, new_service, i_strdup(filename));

	/* Add this entry to the initng service cache, it might be the next service we are looking for */
	if (!initng_service_cache_register(new_service))
	{
		initng_service_cache_free(new_service);
		return (FALSE);
	}


	/* if not watching for anything special */
	if (!watch_for)
	{
		D_("Nothing to wait for\n");
		/* set matach to the first found, and user may walk the services from that point added */
		if (!(*match))
		{
			(*match) = new_service;
		}
		return (TRUE);
	}


	if (strcmp(new_service->name, watch_for) == 0)
	{
		D_("Found EXACT match: %s\n", new_service->name);
		(*exact_match) = new_service;
		return (TRUE);
	}

	/* check if this is the service we are looking for. */
	if (service_match(watch_for, new_service->name) == TRUE)
	{
		D_("MATCH Found :%s\n", new_service->name);
		(*match) = new_service;
		return (TRUE);
	}

	D_(" returning true, no match or exact match\n");
	/* return null if not */
	return (TRUE);
}


#define MAX_OPT_LEN 100
#define MAX_VAR_LEN 100

static int parse_opt(char **where, stype_h * type, service_cache_h * srv)
{
	s_entry *current = NULL;
	char opt_name[MAX_OPT_LEN + 1];
	char *var_name = NULL;
	int opt_len = 0;
	int var_len = 0;

	DL_(*where, "parse_opt");

	/* Parsometer
	 *
	 * "   script start = /usr/bin/gdm; "
	 * "|                               "
	 */

	JUMP_NSPACES(*where);
	if (ENDED(*where))
	{
		FL_(*where, "Line ended unexpectedly.");
		return (FALSE);
	}

	/* Parsometer
	 *
	 * "   script start = /usr/bin/gdm; "
	 * "   |                            "
	 */

	/* skip empty services */
	if (CH_(where, '}'))
	{
		FL_(*where, "found a } char here!");
		return (FALSE);
	}

	/* count number of chars this option name have */
	if ((opt_len = strcspn((*where), "\n; \t=\"'")) < 1)
	{
		FL_(*where, "option name to short!");
		return (FALSE);
	}

	/* get option name and strip all blanks from it */
	if (opt_len >= MAX_OPT_LEN)
	{
		FL_(*where, "Max optlen 100 reached!");
		return (FALSE);
	}
	strncpy(opt_name, (*where), opt_len);
	opt_name[opt_len] = '\0';

	/* walk */
	(*where) += opt_len;
	if (ENDED(*where))
	{
		FL_(*where, "Line just ended after variable name.");
		return (FALSE);
	}
	/* Parsometer
	 *
	 * "   script start = /usr/bin/gdm; "
	 * "         |                      "
	 */

	JUMP_NSPACES(*where);
	if (ENDED(*where))
	{
		FL_(*where, "Line just ended after variable name.");
		return (FALSE);
	}

	/* Parsometer
	 *
	 * "   script start = /usr/bin/gdm; "
	 * "          |                     "
	 */
	DL_(*where, "parse opt, before fetching opt name:");

	if (!CH_(where, '=') && !CH_(where, ';'))
	{
		/* count number of chars this option name have */
		if (!(var_len = strcspn((*where), "\n; \t=\"'")))
		{
			FL_(*where, "variable name to short!");
			return (FALSE);
		}

		/* get option name and strip all blanks from it */
		if (var_len >= MAX_VAR_LEN)
		{
			FL_(*where, "Maximum variable name of 100 met.");
			return (FALSE);
		}

		/* copy the string */
		var_name = i_strndup(*where, var_len);

		if (!is_valid(var_name))
		{
			FL_(*where, "Variable name contains invalid characters.");
			return (FALSE);
		}

		DL_(*where, var_name);

		(*where) += var_len;

		DL_(*where, "EMPTYSPACE");

		/* Parsometer
		 *
		 * "   script start = /usr/bin/gdm; "
		 * "               |                "
		 */
		JUMP_NSPACES(*where);
		if (ENDED(*where))
		{
			FL_(*where, "Line ended unexpectedly.");
			free(var_name);
			return (FALSE);
		}
		/* Parsometer
		 *
		 * "   script start = /usr/bin/gdm; "
		 * "                |               "
		 */


	}

	DL_(*where, " Should be a '='");

	/* walk the option db */
	while_service_data_types(current)
	{
		/* temporary pointer storage */
		s_entry *tmp = current;

		/* make sure opt_name is set */
		if (!current->opt_name)
			continue;

		/* check that option is not marked for a certain service type */
		if (current->ot && current->ot != type)
			continue;

		/* check string length of word, this is faster then strcasecmp */
		if (opt_len != current->opt_name_len)
		{
			/*F_("Lenght is not correct.\n"); */
			continue;
		}

		/* finally, check if opt_name is what we are looking for */
		if (strncmp(current->opt_name, opt_name, opt_len) != 0)
			continue;

		D_("parse_opt(%s): option \"%s\" value at option.\n",
		   srv->name, current->opt_name);

		/* if this is a alias, browse forward to the correct opt_type */
		while (tmp->opt_type == ALIAS && tmp->alias)
			tmp = tmp->alias;

		D_("option type %i, var_name: %s, opt_name: %s\n", tmp->opt_type,
		   var_name ? var_name : "(none)", current->opt_name);

		DL_(*where, current->opt_name);

		/* now switch the opt_type */
		switch (tmp->opt_type)
		{
			case STRING:
			case VARIABLE_STRING:
				return (string_parser(current, where, var_name, srv));
			case STRINGS:
			case VARIABLE_STRINGS:
				return (strings_parser(current, where, var_name, srv));
			case SET:
			case VARIABLE_SET:
				return (set_parser(current, where, var_name, srv));
			case INT:
			case VARIABLE_INT:
				return (iint_parser(current, where, var_name, srv));
			default:
				break;
		}
		FL_(*where, "Unknown, or undefined type to parse!");
		return (FALSE);
	}

	/* bail out if we did not find a match */
	FL_(*where, "Did not get a MATCH!");
	F_("Did not get an match on option: \"%s\"!\n", opt_name);

	return (FALSE);
}

static int set_parser(s_entry * type, char **value
					  __attribute__ ((unused)), char *va,
					  service_cache_h * from_service)
{
	assert(from_service);
	assert(*value);
	assert(value);

	DL_(*value, "set_parser");
	/*DEBUG{
	   char print[200];
	   sprintf(print, "set parser: type:%s va: %s service: %s", type->opt_name, va, from_service->name);
	   FL_(*value, print);
	   } */

	/* Parsometer
	 *
	 * "   enable;"
	 * "         |"
	 */

	if (CH_(value, ';'))
	{
		D_("set_parser(%s,s,%s);\n", type->opt_name, from_service->name);
		set_var(type, va, from_service);
		return (TRUE);
	}

	/* Parsometer
	 *
	 * "   enable = yes"
	 * "          |    "
	 */
	if (!CH_(value, '='))
	{
		FL_(*value, "There should be an ';' or '=' here!");
		return (FALSE);
	}

	/* skip the '=' char */
	(*value)++;
	JUMP_NSPACES(*value);
	if (ENDED(*value))
	{
		FL_(*value, "Line ended unexpectedly.");
		return (FALSE);
	}

	/* Parsometer
	 *
	 * "   enable = yes"
	 * "            |  "
	 */
	if (strncasecmp(*value, "yes", 3) == 0)
	{
		D_("set_parser(%s,s,%s);\n", type->opt_name, from_service->name);
		set_var(type, va, from_service);

		/* skip 3 chars */
		(*value) += 3;
		return (TRUE);
	}

	if (strncasecmp(*value, "true", 4) == 0)
	{
		D_("set_parser(%s,s,%s);\n", type->opt_name, from_service->name);
		set_var(type, va, from_service);

		/* skip 4 chars */
		(*value) += 4;
		return (TRUE);
	}

	/* else, remove */
	remove_var(type, va, from_service);
	return (TRUE);
}


static int string_parser(s_entry * type, char **value, char *va,
						 service_cache_h * from_service)
{
	char *to;

	assert(from_service);
	assert(*value);
	assert(value);

	/*FL_(*value, "stringparser"); */
	/* Parsometer
	 *
	 * "   script start = /usr/bin/gdm; "
	 * "                |               "
	 */

	DL_(*value, "string_parser()");
	if (!CH_(value, '='))
	{
		FL_(*value, "There should be an = here.");
		return (FALSE);
	}

	(*value)++;
	if (ENDED(*value))
	{
		FL_(*value, "Line ended unexpectedly.");
		return (FALSE);
	}
	/* Parsometer
	 *
	 * "   script start = /usr/bin/gdm; "
	 * "                 |              "
	 */
	JUMP_NSPACES(*value);
	if (ENDED(*value))
	{
		FL_(*value, "Line ended unexpectedly.");
		return (FALSE);
	}

	/* Parsometer
	 *
	 * "   script start = /usr/bin/gdm; "
	 * "                  |             "
	 */

	/* duplicate next word, or string content */
	to = dup_string_and_walk(value, FALSE);

	/* go to the end and stay there */
	while ((*value)[0] && !CH_(value, ';'))
		(*value)++;

	DL_(*value, "skip to the ';' char");

	/* Parsometer
	 *
	 * "   script start = /usr/bin/gdm; "
	 * "                              | "
	 */
	/* make sure the end is a ';' */
	if (ENDED(*value))
	{
		FL_(*value, "Line ended unexpectedly.");
		free(to);
		return (FALSE);
	}

	if (!CH_(value, ';'))
	{
		FL_(*value, "There should be an ; here.");
		if ((*value)[0] == '\n')
			F_("Its a *newline* instead.\n");
		else
			F_("Its a %c instead.\n", (*value)[0]);
		return (FALSE);
	}



	set_string_var(type, va, from_service, to);
	return (TRUE);
}

static int iint_parser(s_entry * type, char **value, char *va,
					   service_cache_h * from_service)
{
	char to[11];
	int len = 0;

	assert(from_service);
	assert(*value);
	assert(value);

	/*FL_(*value, "intparser"); */
	/* Parsometer
	 *
	 * "   pause = 10; "
	 * "         |     "
	 */

	if (!CH_(value, '='))

	{
		FL_(*value, "Missing an '=' here.");
		return (FALSE);
	}

	(*value)++;
	if (ENDED(*value))
	{
		FL_(*value, "Line ended unexpectedly.");
		return (FALSE);
	}

	/* Parsometer
	 *
	 * "   pause = 10; "
	 * "          |    "
	 */

	JUMP_NSPACES(*value);
	if (ENDED(*value))
	{
		FL_(*value, "Line ended unexpectedly.");
		return (FALSE);
	}

	/* Parsometer
	 *
	 * "   pause = 10; "
	 * "           |   "
	 */

	len = strcspn(*value, "\n;");
	/* make sure the end is a ';' */
	if (!CHL_(value, len, ';'))
	{
		FL_(*value, "Missing an ';' char.");
		return (FALSE);
	}

	/* make sure value is not to big */
	if (len >= 10)
	{
		FL_(*value, "Length is bigger than 10.");
		return (FALSE);
	}

	/* ok, copy the code */
	strncpy(to, *value, len);
	to[len] = '\0';

	/* walk */
	(*value) += len;
	if (ENDED(*value))
	{
		FL_(*value, "Line ended unexpectedly.");
		return (FALSE);
	}

	set_int_var(type, va, from_service, atoi(to));
	return (TRUE);
}

static int strings_parser(s_entry * type, char **value, char *va,
						  service_cache_h * from_service)
{
	char *to = NULL;

	assert(from_service);
	assert(*value);
	assert(value);

	/*FL_(*value, "stringS parser"); */
	/* Parsometer
	 *
	 * "   need = test service; "
	 * "        |               "
	 */
	DL_(*value, "strings_parser()");


	if (!CH_(value, '='))
	{
		FL_(*value, "Missing an '=' here.");
		return (FALSE);
	}

	(*value)++;
	if (ENDED(*value))
	{
		FL_(*value, "Line ended unexpectedly.");
		return (FALSE);
	}

	/* Parsometer
	 *
	 * "   need = test service; "
	 * "         |              "
	 */
	JUMP_NSPACES(*value);
	if (ENDED(*value))
	{
		FL_(*value, "Line ended unexpectedly.");
		return (FALSE);
	}
	/* Parsometer
	 *
	 * "   need = test service; "
	 * "          |             "
	 */
	DL_(*value,
		"strings_parser, Youst before dup_string_and_walk(value, TRUE)");

	while ((to = dup_string_and_walk(value, TRUE)))
	{
		DL_(*value, "adding");
		if (va)
		{
			set_another_string_var(type, i_strdup(va), from_service, to);
		}
		else
		{
			set_another_string_var(type, NULL, from_service, to);
		}
		/* check if this is end */
		if (CH_(value, ';'))
			break;

		JUMP_NSPACES(*value);

		/* check if this is end */
		if (CH_(value, ';'))
			break;
		if (ENDED(*value))
			break;
		DL_(*value, "a_add");
	}

	/* dont forgot to free va */
	if (va)
	{
		free(va);
		va = NULL;
	}
	/* Parsometer
	 *
	 * "   need = test service; "
	 * "                      | "
	 */

	DL_(*value, "all added");
	if (!CH_(value, ';'))
	{
		FL_(*value, "End char is not an ';'.");
		return (FALSE);
	}
	return (TRUE);
}

int module_init(int api_version)
{

	D_("i_parser: module_init();\n");
	if (api_version != API_VERSION)
	{
		F_("This module is compiled for api_version %i version and initng is compiled on %i version, won't load this module!\n", API_VERSION, api_version);
		return (FALSE);
	}

	return (initng_event_hook_register(&EVENT_PARSE, &initng_i_parser));

}

void module_unload(void)
{
	D_("i_parser: module_unload();\n");
	initng_event_hook_unregister(&EVENT_PARSE, &initng_i_parser);
}


static char *dup_string_and_walk(char **value, int space_is_new_word)
{
	char *to = NULL;
	int len = 0;

	/* Parsometer
	 *
	 * "   script start = { /usr/bin/gdm };            "
	 * "   script start = "/usr/bin/gdm";              "
	 * "   script start = /usr/bin/gdm;                "
	 * "   script start = /usr/bin/gdm /usr/sbin/gdm;  "
	 * "                  |                            "
	 */


	if (space_is_new_word == TRUE)
	{
		DL_(*value, "Space is new word");
	}
	else
	{
		DL_(*value, "Space is none word");
	}

	DL_(*value, "dup_string_and_walk:");



	/* If this value we should copy is stacked in braces */
	if (CH_(value, '{'))
	{
		int stack = 1;

		(*value)++;

#ifndef NOTRIM
		JUMP_NSPACES(*value);
#endif
		/* count length to the '}' char */
		while ((*value)[len])
		{
			if (CHL_(value, len, '{'))
				stack++;
			else if (CHL_(value, len, '}'))
				stack--;

			/* if stack is zero or below, we are standing on a '}' with no escape char before */
			if (stack <= 0 && CHL_(value, len, '}'))
			{
				(*value)[len] = ';';
				break;
			}
			len++;
		}

#ifndef NOTRIM
		while (((*value)[len - 1] == ' ' || (*value)[len - 1] == '\t'
				|| (*value)[len - 1] == '\n') && (*value)[len - 2] != '\\')
			len--;
#endif
		/* ok, copy the code */
		if (!(to = i_strndup(*value, len)))
		{
			FL_(*value, "Failed copying.");
			return (NULL);
		}

		/* walk forward again */
		(*value) += len;

#ifndef NOTRIM
		JUMP_NSPACES(*value);
#endif
		if (CH_(value, '}'))
			(*value)++;

		DL_(*value, "after += len");

		return (to);

	}


	/* if it is embraced in with '"' */
	if (CH_(value, '"'))
	{
		/* skip the '"' char */
		(*value)++;

		/* count the number of tokens */
		while ((*value)[len])
		{
			/* if this is the ending '"' with no escape char before */
			if (CH_(value, '"'))
			{
				break;
			}
			len++;
		}

		/* ok, copy the code */
		if (!(to = i_strndup(*value, len)))
		{
			FL_(*value, "Failed copying.");
			return (NULL);
		}

		/* walk forward again */
		(*value) += len;
		if (CH_(value, '"'))
			(*value)++;

		DL_(*value, "after += len");

		return (to);
	}

	/* else */
	DL_(*value, "Non stack or quoted value");

	/* Count length */
	while ((*value)[len])
	{
		/* If we should break copying on a space, check this */
		if (space_is_new_word == TRUE)
		{
			DL_(*value, "Space is new word");
			if (((*value)[len] == ' ' || (*value)[len] == '\t'
				 || (*value)[len] == '\n') && (*value)[len - 1] != '\\')
				break;
		}

		/* always break on a ; */
		if (CHL_(value, len, ';'))
			break;

		len++;
	}

	/* ok, copy the code */
	if (!(to = i_strndup(*value, len)))
	{
		FL_(*value, "Failed copying.");
		return (FALSE);
	}

	/* walk forward again */
	(*value) += len;
	DL_(*value, "after += len");

	return (to);
}
