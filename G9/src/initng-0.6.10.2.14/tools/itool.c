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
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>

#include <initng_global.h>
#include <initng_signal.h>
#include <initng_handler.h>
#include <initng_execute.h>					/* new_environ() */
#include <initng_service_cache.h>
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


#define MAX_PATH_LEN 40

static char *fix_path(char *ipath)
{
	char *path = NULL;

	/* if path is specified with CWD */
	if (ipath[0] == '.')
	{
		path = malloc(sizeof(char) * (strlen(ipath) + MAX_PATH_LEN + 1));
		path = getcwd(path, MAX_PATH_LEN);
		path = strcat(path, &ipath[1]);
	}
	else
	{
		path = strdup(ipath);
	}

	return (path);
}

static service_cache_h *parse_path(char *ipath)
{
	/* fix the path */
	char *path = fix_path(ipath);

	/* Start parsing file. */
	return (initng_common_parse_service(path));
}

int main(int argc, char *argv[], char *env[])
{
	/*printf("argc: %i argv[0]: %s argv[1]: %s argv[2]: %s\n", argc, argv[0],
	   argv[1], argv[2]); */

	/* initialise global variables */
	initng_global_new(argc, argv, env);
	g.i_am = I_AM_UTILITY;

	/* Load all plugins */
	if (!initng_load_module_load_all(INITNG_PLUGIN_DIR))
	{
		printf("could not load all modules\n");
		exit(1);
	}

	/* fallback */
	if (argc == 3)
	{
		service_cache_h *serv = NULL;

		serv = parse_path(argv[1]);
		if (serv)
		{

			if (strcmp(argv[2], "list") == 0)
			{
				serv = NULL;				/* walk them all */
				while_service_cache(serv)
				{
					printf("%s ", serv->name);
				}
				putchar('\n');
				goto end;
			}

			if (strcmp(argv[2], "print") == 0)
			{
				char *out = service_db_print_all(serv->name);

				printf("%s\n", out);
				free(out);
				goto end;
			}

		}
	}

	if (argc == 4 && argv[3] && strcmp(argv[2], "execute") == 0)
	{

		/* create an active struct */
		ptype_h ptype = { "test-run", NULL };

		/* Create an active service struct */
		active_db_h *active = initng_active_db_new(fix_path(argv[1]));

		/* This will parse file */
		initng_common_get_service(active);

		printf("Executing service %s type %s\n", active->name, argv[3]);


		/* Now launch! (will fork) */
		initng_execute_launch(active, &ptype, argv[3]);

		wait(NULL);
		goto end;
	}


	puts("Usage: itool i_file.i [list] [print] [execute name]\n");
  end:
	/* unload all modules */
	initng_unload_module_unload_all();
	initng_service_cache_free_all();
	initng_global_free();
	return (TRUE);
}
