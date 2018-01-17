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

char *environ[] = { NULL };

#define MAX_PATH_LEN 40

static char *get_serv(char *ipath)
{
	char *path = NULL;
	char *service = NULL;

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

	/* cut the full path */
	if (strncmp("/etc/initng/", path, 12) == 0)
	{
		service = strdup(&path[12]);
		free(path);
	}
	else
	{
		printf("%s dont begin with /etc/initng/", path);
		free(path);
		return (NULL);
	}

	{
		int i = 0;

		/* skip to last .i */
		while (service[i] && service[i] != '.')
			i++;
		service[i] = '\0';
	}

	/* return what we got */
	return (service);
}


int main(int argc, char *argv[], char *env[])
{
	const char *new_argv[] = { "/sbin/ngc", NULL, NULL, NULL };
	char *fixed_path;


	if (!argv[1] || argc != 3)
	{
		if (argc >= 2)
			printf("Usage: %s %s [start] [stop] [restart] [status] [zap]\n",
				   argv[0], argv[1]);
		else
			printf("Usage: itype i_file.i [start] [stop] [restart] [status] [zap]\n");
		exit(1);
	}
	/*printf("argc: %i argv[0]: %s argv[1]: %s argv[2]: %s\n", argc, argv[0], argv[1], argv[2]); */


	fixed_path = get_serv(argv[1]);
	if (!fixed_path)
	{
		printf("unkown service\n");
		exit(1);
	}

	/*printf("Service: %s\n", fixed_path); */

	/* usage "/etc/initng/test.i start" */
	if (strcmp(argv[2], "start") == 0)
	{
		new_argv[1] = "-u";
		new_argv[2] = fixed_path;
		execve((char *) new_argv[0], (char **) new_argv, environ);
		goto end;
	}

	/* usage "/etc/initng/test.i stop" */
	if (strcmp(argv[2], "stop") == 0)
	{
		new_argv[1] = "-d";
		new_argv[2] = fixed_path;
		execve((char *) new_argv[0], (char **) new_argv, environ);
		goto end;
	}

	/* usage "/etc/initng/test.i stop" */
	if (strcmp(argv[2], "status") == 0)
	{
		new_argv[1] = "-s";
		new_argv[2] = fixed_path;
		execve((char *) new_argv[0], (char **) new_argv, environ);
		goto end;
	}

	/* usage "/etc/initng/test.i zap" */
	if (strcmp(argv[2], "zap") == 0)
	{
		new_argv[1] = "-z";
		new_argv[2] = fixed_path;
		execve((char *) new_argv[0], (char **) new_argv, environ);
		goto end;
	}

	/* usage "/etc/initng/test.i restart" */
	if (strcmp(argv[2], "restart") == 0)
	{
		new_argv[1] = "-r";
		new_argv[2] = fixed_path;
		execve((char *) new_argv[0], (char **) new_argv, environ);
		goto end;
	}

	{
		printf("Usage: %s %s [start] [stop] [restart] [status] [zap]\n",
			   argv[0], argv[1]);
	}
  end:
	/* unload all modules */
	free(fixed_path);
	return (0);
}
