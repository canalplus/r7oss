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
#include <dirent.h>

#include <initng_global.h>
#include <initng_active_state.h>
#include <initng_active_db.h>
#include <initng_process_db.h>
#include <initng_service_cache.h>
#include <initng_handler.h>
#include <initng_active_db.h>
#include <initng_toolbox.h>
#include <initng_load_module.h>
#include <initng_plugin_callers.h>
#include <initng_error.h>
#include <initng_plugin.h>
#include <initng_static_states.h>
#include <initng_control_command.h>
#include <initng_static_event_types.h>
#include <initng_event_hook.h>
#include <initng_string_tools.h>
#include <initng_fd.h>

#include <initng-paths.h>

/* the standard intotify headers */
#include "inotify.h"
#include "inotify-syscalls.h"

INITNG_PLUGIN_MACRO;

/* static functions */
static void initng_reload(void);
static void filemon_event(f_module_h * from, e_fdw what);

static int fdh_handler(s_event * event);

/* this plugin file descriptor we add to monitor */
f_module_h fdh = { &filemon_event, FDW_READ, -1 };

/* Saved to be closed later on */
int plugins_watch = -1;
int initng_watch = -1;
int i_watch = -1;


static int fdh_handler(s_event * event)
{
	s_event_fd_watcher_data * data;

	assert(event);
	assert(event->data);

	data = event->data;

	switch (data->action)
	{
		case FDW_ACTION_CLOSE:
			if (fdh.fds > 0)
				close(fdh.fds);
			break;

		case FDW_ACTION_CHECK:
			if (fdh.fds <= 2)
				break;

			/* This is a expensive test, but better safe then sorry */
			if (!STILL_OPEN(fdh.fds))
			{
				D_("%i is not open anymore.\n", fdh.fds);
				fdh.fds = -1;
				break;
			}

			FD_SET(fdh.fds, data->readset);
			data->added++;
			break;

		case FDW_ACTION_CALL:
			if (!data->added || fdh.fds <= 2)
				break;

			if(!FD_ISSET(fdh.fds, data->readset))
				break;

			filemon_event(&fdh, FDW_READ);
			data->added--;
			break;

		case FDW_ACTION_DEBUG:
			if (!data->debug_find_what || strstr(__FILE__, data->debug_find_what))
				mprintf(data->debug_out, " %i: Used by plugin: %s\n",
					fdh.fds, __FILE__);
			break;
	}

	return (TRUE);
}

/* This function trys to reload initng if reload plugin is loaded */
static void initng_reload(void)
{
	/* get the command */
	s_command *reload = initng_command_find_by_command_id('c');

	/* if found */
	if (reload && reload->u.void_command_call)
	{
		/* execute */
		(*reload->u.void_command_call) (NULL);
	}
}


/* called by fd hook, when there is data */
void filemon_event(f_module_h * from, e_fdw what)
{

	/* this is overkill, we wont get 1024 events */
#define EVENT_SIZE (sizeof(struct inotify_event))
#define BUF_LEN (1024 * (EVENT_SIZE + 16))

	int len = 0;
	int i = 0;

#ifdef SERVICE_CACHE
	char *tmp;
#endif
	char buf[BUF_LEN];

	/* read events */
	len = read(from->fds, buf, BUF_LEN);

	/* if error */
	if (len < 0)
	{
		F_("fmon read error\n");
		return;
	}

	/* handle all */
	while (i < len)
	{
		struct inotify_event *event;

		event = (struct inotify_event *) &buf[i];

		/*printf("wd=%d, mask=%u, cookie=%u, len=%u\n",
		   event->wd, event->mask, event->cookie, event->len);
		   if(event->len)
		   printf("name: %s\n", event->name); */


		if (event->mask & IN_MODIFY)
		{
			/* check if its a plugin modified */
			if (event->wd == plugins_watch && event->len
				&& strstr(event->name, ".so"))
			{
				W_("Plugin %s/%s has been changed, reloading initng.\n",
				   INITNG_PLUGIN_DIR, event->name);

				/* sleep 1 seconds, maby more files will be modified in short */
				sleep(1);

				/* hot-reload initng */
				initng_reload();

				return;
			}

			/* check if its initng binary modified */
			if (event->wd == initng_watch && event->len
				&& strcmp(event->name, "/sbin/initng") == 0)
			{
				W_("/sbin/initng modified, reloading initng.\n");

				/* sleep 1 seconds, maby more files will be modified in short */
				sleep(1);

				/* hot-reload initng */
				initng_reload();

				return;
			}

#ifdef SERVICE_CACHE
			/* check if there are any data file updated */
			if (((tmp = strstr(event->name, ".i")) && tmp[2] == '\0')
				|| strstr(event->name, ".runlevel")
				|| strstr(event->name, ".virtual"))
			{
				/* if cache is not cleared */
				if (!list_empty(&g.service_cache.list))
				{

					/* zap failing services using this file */
					{
						active_db_h *active = NULL;
						active_db_h *safe = NULL;
						const char *fn;

						while_active_db_safe(active, safe)
						{
							/* if we got a string from FROM_FILE */
							if ((fn = get_string(&FROM_FILE, active)) &&
								/* and we found the file in that string */
								strstr(fn, event->name) &&
								/* and the service is marked FAILED */
								IS_FAILED(active))
							{
								W_("Zapping %s because the source %s has changed, and it might work again.\n", active->name, event->name);
								initng_active_db_free(active);
							}
						}
					}

					W_("Source file \"%s\" changed, flushing file cache.\n",
					   event->len ? event->name : "unkown");
					initng_service_cache_free_all();
				}

			}
#endif

		}

		i += EVENT_SIZE + event->len;
	}
}


static int mon_dir(const char *dir)
{
	DIR *path;
	struct dirent *dir_e;
	struct stat fstat;

	/*printf("add watch: %s\n", dir); */

	/* monitor /etc/initng */
	if (inotify_add_watch(fdh.fds, dir, IN_MODIFY) < 0)
	{
		F_("Fail to monitor \"%s\"\n", dir);
		return (FALSE);
	}

	path = opendir(dir);
	if (!path)
		return (FALSE);

	/* Walk thru all files in dir */
	while ((dir_e = readdir(path)))
	{
		/* skip dirs/files starting with a . */
		if (dir_e->d_name[0] == '.')
			continue;

		/* set up full path */
		char *file = NULL;
		asprintf(&file, "%s/%s", dir, dir_e->d_name);

		/* get the stat of that file */
		if (stat(file, &fstat) != 0)
		{
			printf("File %s failed stat errno: %s\n", file, strerror(errno));
			if(file) free(file);
			continue;
		}

		/* if it is a dir */
		if (S_ISDIR(fstat.st_mode))
		{
			mon_dir(file);
			/* continue while loop */
			if(file) free(file);
			continue;
		}
		if(file) free(file);
	}
	closedir(path);
	return (FALSE);
}


int module_init(int api_version)
{
	if (api_version != API_VERSION)
	{
		F_("This module is compiled for api_version %i version and initng is compiled on %i version, won't load this module!\n", API_VERSION, api_version);
		return (FALSE);
	}

	/* zero globals */
	fdh.fds = -1;

	/* initziate file monitor */
	fdh.fds = inotify_init();

	/* check so it succeded */
	if (fdh.fds < 0)
	{
		F_("Fail start file monitoring\n");
		return (FALSE);
	}

	/* monitor initng plugins */
	plugins_watch = inotify_add_watch(fdh.fds, INITNG_PLUGIN_DIR, IN_MODIFY);

	/* check so it succeded */
	if (plugins_watch < 0)
	{
		F_("Fail to monitor \"%s\"\n", INITNG_PLUGIN_DIR);
		return (FALSE);
	}

	/* monitor initng binary */
	initng_watch = inotify_add_watch(fdh.fds, "/sbin/initng", IN_MODIFY);

	/* check so it succeded */
	if (initng_watch < 0)
	{
		F_("Fail to monitor \"/sbin/initng\"\n");
		return (FALSE);
	}

#ifdef SERVICE_CACHE
	mon_dir("/etc/initng");
#endif

	/* add this hook */
	initng_event_hook_register(&EVENT_FD_WATCHER, &fdh_handler);

	/* printf("Now monitoring...\n"); */

	return (TRUE);
}


void module_unload(void)
{
	/* remove watchers */
	inotify_rm_watch(fdh.fds, plugins_watch);
	inotify_rm_watch(fdh.fds, initng_watch);

	/* close sockets */
	close(fdh.fds);

	/* remove hooks */
	initng_event_hook_unregister(&EVENT_FD_WATCHER, &fdh_handler);
}
