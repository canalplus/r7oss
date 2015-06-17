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

#include <time.h>							/* time() */
#include <fcntl.h>							/* fcntl() */
#include <sys/un.h>							/* memmove() strcmp() */
#include <sys/wait.h>						/* waitpid() sa */
#include <linux/kd.h>						/* KDSIGACCEPT */
#include <sys/ioctl.h>						/* ioctl() */
#include <stdlib.h>							/* free() exit() */
#include <sys/reboot.h>						/* reboot() RB_DISABLE_CAD */
#include <termios.h>
#include <stdio.h>
#include <sys/klog.h>
#include <dirent.h>
#include <sys/stat.h>
#include <fnmatch.h>
#include <sys/time.h>
#include <sys/resource.h>

#include <initng_global.h>
#include <initng_signal.h>
#include <initng_handler.h>
#include <initng_execute.h>					/* new_environ() */
#include <initng_active_db.h>
#include <initng_load_module.h>
#include <initng_plugin_callers.h>
#include <initng_toolbox.h>
#include <initng_main.h>
#include <initng_service_data_types.h>
#include <initng_fd.h>
#include <initng_common.h>
#include <initng-paths.h>

/* include some soruce files directly, remember this is only a test */
#include <initng_global.c>
#include "../plugins/debug_commands/print_service.c"

/*
 * A test, that will load service, set by argument 1, and then exit
 */

#define SPACE for(i=0; i<level; i++, printf("  "))

#define CIRCULAR_ERR -3
#define DEPEND_ERR -2
#define PARSE_ERR -1
#define NOT_OK 0
#define OK 1
#define CHECKED 2

static int load_service(const char *name, int level);
static int parse_all(const char *dirname);
static int check_deps(const char **dep_list, const char *dep, int level);

static int verbose = 1;
static int print_each = 0;
static int check_circular = 0;

s_entry STATUS = { "status", INT, NULL, "Status of testing." };
s_entry CIRCULAR = { "circular", STRING, NULL, "Name of curcular dependencied service." };

int main(int argc, char *argv[], char *env[])
{

	int parse_all_files = 0;
	int summary = 1;
	int print_dep_err = 0;
	int result = NOT_OK;
	int i;
	char *srv_name = NULL;

	srv_name = i_strdup("default");

	assert(srv_name);

	service_cache_h *current = NULL;

	/* initialise global variables */
	initng_global_new(argc, argv, env);
	g.i_am = I_AM_UTILITY;

	if (!initng_load_module_load_all(INITNG_PLUGIN_DIR))
	{
		printf("could not load all modules\n");
		goto exit;
	}

	/* enable verbose mode */
#ifdef DEBUG
	g.verbose = 0;
#endif

	for (i = 1; i < argc; i++)
	{
		int opt_type = 0;
		char *opt = argv[i];

		if (strlen(opt) <= 1)
			continue;

		if (opt[0] == '-' && opt[1] == '-')
		{
			if (strlen(opt) == 2)
				continue;

			opt_type = 1;
			opt += 2;
		}
		else if (opt[0] == '-')
		{
			opt_type = 2;
			opt++;
		}

		if (opt_type == 1)
		{
			if (strcmp(opt, "help") == 0)
			{
				printf("test_parser --help --verbose --all --no_sumary --check_circular --print_each --print_dep_err\n");
				exit(1);
			}

			if (strcmp(opt, "verbose") == 0)
				verbose = 1;

			if (strcmp(opt, "all") == 0)
				parse_all_files = 1;

			if (strcmp(opt, "no_summary") == 0)
				summary = 0;

			if (strcmp(opt, "check_circular") == 0)
				check_circular = 1;

			if (strcmp(opt, "print_each") == 0)
				print_each = 1;

			if (strcmp(opt, "print_dep_err") == 0)
				print_dep_err = 1;
		}
		else if (opt_type == 2)
		{
			if (strcmp(opt, "v") == 0)
				verbose = 1;

			if (strcmp(opt, "a") == 0)
				parse_all_files = 1;

			if (strcmp(opt, "s") == 0)
				summary = 0;

			if (strcmp(opt, "c") == 0)
				check_circular = 1;

			if (strcmp(opt, "p") == 0)
				print_each = 1;

			if (strcmp(opt, "d") == 0)
				print_dep_err = 1;

		}
		else
		{
			free(srv_name);
			srv_name = i_strdup(opt);
		}
	}

	if (parse_all_files == 1)
	{
		result = parse_all("system");
		result &= parse_all("daemon");
		result &= parse_all("net");
		//parse_all("debug");
		result &= parse_all("daemon/bluetooth");
	}
	else
	{
		result = load_service(srv_name, 0);
		if (check_circular == 1)
			result &= check_deps(NULL, srv_name, 0);

		if (summary == 1)
		{
			if (result == NOT_OK)
			{
				printf("Service %s is failed because:\n", srv_name);
				while_service_cache(current)
				{
					switch (get_int(&STATUS, current))
					{
						case PARSE_ERR:
							printf("%s has parsing errors or service file was not found\n", current->name);
							break;
						case CIRCULAR_ERR:
							printf("%s has circular dependency with %s\n",
								   current->name,
								   get_string(&CIRCULAR, current));
							break;
						case DEPEND_ERR:
							if (print_dep_err == 1)
								printf("%s has one of its depends failed\n",
									   current->name);
							break;
						default:
							break;
					};
				}
			}
			else
				printf("Service %s is ok.\n", srv_name);
		}

	}

#ifdef PRINT_ALL
	{
		char *all = service_db_print_all(NULL);

		printf("%s\n", all);
	}
#endif


  exit:
	/* unload all modules */
	initng_unload_module_unload_all();
	initng_service_cache_free_all();
	initng_global_free();
	free(srv_name);
	return (result == NOT_OK) ? OK : NOT_OK;
}

static int parse_all(const char *dirname)
{
	struct dirent *e;
	char tmp[200];
	char tmp2[200];
	DIR *d;
	int i;
	int result = NOT_OK;

	sprintf(tmp, INITNG_ROOT "/%s", dirname);

	d = opendir(tmp);
	if (!d)
		return result;

	result = OK;

	while ((e = readdir(d)))
	{
		if (e->d_name[0] == '.')
			continue;

		/* get string length */
		i = strlen(e->d_name);

		/* Check that is it a .i file */
		if (e->d_name[i - 1] != 'i' && e->d_name[i - 2] != '.')
			continue;

		strncpy(tmp, e->d_name, i - 2);
		tmp[i - 2] = '\0';
		sprintf(tmp2, "%s/%s", dirname, tmp);
		result &= load_service(tmp2, 0);
		if (check_circular == 1)
			result &= check_deps(NULL, tmp2, 0);
	}

	closedir(d);
	return result;
}

static int load_service(const char *name, int level)
{
	service_cache_h *service = NULL;
	service_cache_h *tmp_service = NULL;
	const char *string = NULL;
	int result = NOT_OK;
	int srv_status;
	int i;

	if (verbose == 1)
	{
		SPACE;
		printf("Probeparsing: %s\n", name);
	}

	service = initng_common_parse_service(name);
	if (!service)
	{
		/* unload all modules
		   initng_unload_all_modules();
		   initng_free();
		   exit(2);
		 */
		if (verbose == 1)
		{
			SPACE;
			printf("%s - failed\n", name);
		}
		service = initng_service_cache_new(name,
										   initng_service_type_get_by_name
										   ("service"));
		if (service)
		{
			initng_service_cache_register(service);
			set_int(&STATUS, service, PARSE_ERR);
		}
		return result;
	}

	if (print_each == 1)
	{
		char *string = service_db_print_all(service->name);

		printf("%s\n", string);
		free(string);
	}

	result = OK;

	set_int(&STATUS, service, OK);
	s_data *itt = NULL;

	while ((string = get_next_string(&NEED, service, &itt)))
	{
		tmp_service = initng_service_cache_find_by_name(string);
		if (!tmp_service || (srv_status = get_int(&STATUS, tmp_service)) == 0)
			result &= load_service(string, level + 1);
		else
			result &= (srv_status > NOT_OK) ? OK : NOT_OK;
	}


	if (verbose == 1)
	{
		SPACE;
		if (result == NOT_OK)
			printf("%s - failed\n", name);
		else
			printf("%s - ok\n", name);
	}

	set_int(&STATUS, service, (result == NOT_OK) ? DEPEND_ERR : OK);

	return result;
}

static int check_deps(const char **dep_list, const char *dep, int level)
{
	int result = NOT_OK;
	int tmp_result;
	int status;
	const char **my_list;
	const char *string = NULL;
	int i, j;
	service_cache_h *service = initng_service_cache_find_by_name(dep);

	if (verbose == 1)
	{
		SPACE;
		printf("Checking: %s\n", dep);
	}

	if (!service)
		status = NOT_OK;
	else
		status = get_int(&STATUS, service);

	if (status == CHECKED)
		result = OK;

	if (status == OK)
	{
		s_data *itt = NULL;

		result = OK;

		my_list = (const char **) i_calloc(level + 2, sizeof(char *));
		my_list[level + 1] = NULL;

		while ((string = get_next_string(&NEED, service, &itt)))
		{
			tmp_result = OK;

			for (j = 0; dep_list && dep_list[j]; j++)
			{
				if (strcmp(string, dep_list[j]) == 0)
				{
					status = CIRCULAR_ERR;
					set_int(&STATUS, service, status);
					set_string(&CIRCULAR, service, i_strdup(string));
					if (verbose == 1)
					{
						SPACE;
						printf("Service %s has circular dependency with %s\n",
							   dep, string);
					}
					tmp_result = NOT_OK;
					break;
				}
				my_list[j] = dep_list[j];
			}

			if (tmp_result == OK)
			{
				my_list[j] = dep;
				tmp_result = check_deps(my_list, string, level + 1);
			}

			result &= tmp_result;
		}
		free(my_list);
	}

	if (verbose == 1)
	{
		SPACE;
		if (result == NOT_OK)
			printf("%s - failed\n", dep);
		else
			printf("%s - ok\n", dep);
	}

	status = (result ==
			  OK) ? CHECKED : ((status !=
								CIRCULAR_ERR) ? DEPEND_ERR : CIRCULAR_ERR);

	if (service)
		set_int(&STATUS, service, status);

	return result;
}
