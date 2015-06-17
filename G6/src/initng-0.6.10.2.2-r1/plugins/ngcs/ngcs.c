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

#include "ngcs_client.h"
#include "initng_ngcs_cmds.h"
#include "ansi_colors.h"
#include <initng.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <initng_list.h>
#include <sys/select.h>
#include <errno.h>
#include <time.h>
#include <ctype.h>

#define C_ERROR "\n  "C_FG_RED" [ERROR] -->> "C_OFF

#define maybe_printf(...) { if(quiet==FALSE) printf(__VA_ARGS__ ); }
#define maybe_grab_out(me) ( quiet ? 0 : grab_out(me))


typedef enum
{
	WATCH_NORMAL = 0,
	WATCH_START = 1,
	WATCH_STOP = 2,
	WATCH_STATUS = 3
} e_watch_mode;

typedef struct cmd_res_s
{
	char *cmd;
	char *arg;
	union
	{
		struct
		{
			int in_present;
			int mode;
			int got_svcs;
		} svc_watch;
	} d;
	struct list_head list;
} cmd_res;

cmd_res pending;
int quiet = 0;
ngcs_cli_conn *cconn;
int debug = 0;
int failed = 0;

void resp_handler(ngcs_cli_conn * cconn, void *userdata, ngcs_data * ret);
void docmd(char *cmd, char *arg);
void svc_watch_cb(ngcs_svc_evt_hook * hook, void *userdata,
				  ngcs_svc_evt * event);
const char *state_color(int rough_state);
int grab_out(void *me);

void *last_out;
int need_nl = 0;

int grab_out(void *me)
{
	int n;

	if (me == NULL || last_out != me)
	{
		for (n = 0; n < need_nl; n++)
			putchar('\n');
		last_out = me;
		return 1;
	}
	else
		return 0;
}

const char *state_color(int rough_state)
{
	switch (rough_state)
	{
		case IS_UP:
			return C_FG_NEON_GREEN;
		case IS_DOWN:
			return C_FG_LIGHT_BLUE;
		case IS_FAILED:
			return C_FG_LIGHT_RED;
		case IS_STARTING:
			return C_FG_YELLOW;
		case IS_STOPPING:
			return C_FG_CYAN;
		case IS_WAITING:
			return C_FG_MAGENTA;
		default:
			return C_FG_DEFAULT;
	}
}

void svc_watch_cb(ngcs_svc_evt_hook * hook, void *userdata,
				  ngcs_svc_evt * event)
{
	cmd_res *res = userdata;

	assert(event);
	assert(res);

	switch (event->type)
	{

		case NGCS_SVC_EVT_BEGIN:
			if (res->d.svc_watch.mode == WATCH_STATUS)
			{
				grab_out(NULL);
				printf(C_FG_LIGHT_RED "hh:mm:ss" C_OFF C_FG_CYAN " T "
					   C_OFF "service                             : "
					   C_FG_NEON_GREEN "status\n" C_OFF);
				printf("----------------------------------------------------------------\n");
			}
			break;
		case NGCS_SVC_EVT_NOW:
			res->d.svc_watch.in_present = 1;
			if (res->d.svc_watch.mode == WATCH_STATUS)
			{
				ngcs_client_free_svc_watch(hook);
				return;
			}
			break;
		case NGCS_SVC_EVT_OUTPUT:
			if (res->d.svc_watch.mode == WATCH_STATUS)
				break;
			if (maybe_grab_out(res))
			{
				maybe_printf(C_FG_BLUE "\n%s output:" C_OFF "\n",
							 event->svc_name);
				need_nl = 1;
			}
			maybe_printf("%s", event->r.output);
			break;
		case NGCS_SVC_EVT_STATE:
			res->d.svc_watch.got_svcs = 1;
			switch (res->d.svc_watch.mode)
			{
				case WATCH_START:
					if (event->r.adb.current_state->is == IS_UP)
					{
						grab_out(NULL);
						printf("Service \"%s\" is started (%s%s" C_OFF ")\n",
							   event->svc_name,
							   state_color(event->r.adb.current_state->is),
							   event->r.adb.current_state->state_name);
						ngcs_client_free_svc_watch(hook);
						return;

					}
					/* FIXME - below possibly incorrect if it was stopping */
					else if ((res->d.svc_watch.in_present ||
							  event->r.adb.current_state->is != IS_DOWN) &&
							 event->r.adb.current_state->is != IS_STARTING)

					{
						grab_out(NULL);
						printf("%s " C_FG_LIGHT_RED "failed to start"
							   C_OFF " (%s%s" C_OFF ")\n",
							   event->svc_name,
							   state_color(event->r.adb.current_state->is),
							   event->r.adb.current_state->state_name);
						failed = 1;
						ngcs_client_free_svc_watch(hook);
						return;
					}
					break;
				case WATCH_STOP:
					if (event->r.adb.current_state->is == IS_DOWN)
					{
						grab_out(NULL);
						printf("Service \"%s\" is stopped (%s%s" C_OFF ")\n",
							   event->svc_name,
							   state_color(event->r.adb.current_state->is),
							   event->r.adb.current_state->state_name);
						ngcs_client_free_svc_watch(hook);
						return;

					}
					/* FIXME - below possibly incorrect if it was starting */
					else if ((res->d.svc_watch.in_present ||
							  event->r.adb.current_state->is != IS_UP) &&
							 event->r.adb.current_state->is != IS_STOPPING)

					{
						grab_out(NULL);
						printf("Service \"%s\" " C_FG_LIGHT_RED
							   "failed to stop" C_OFF " (%s%s" C_OFF ")\n",
							   event->svc_name,
							   state_color(event->r.adb.current_state->is),
							   event->r.adb.current_state->state_name);
						failed = 1;
						ngcs_client_free_svc_watch(hook);
						return;
					}
					break;
				case WATCH_STATUS:
					{
						struct tm *ts = localtime(&event->r.adb.time_current_state.tv_sec);
						grab_out(NULL);
						printf(C_FG_LIGHT_RED "%.2i:%.2i:%.2i" C_OFF C_FG_CYAN
							   " %c" C_OFF " %-35s : %s%s" C_OFF "\n",
							   ts->tm_hour, ts->tm_min, ts->tm_sec,
							   (char) toupper((int) event->r.adb.type.
											  name[0]), event->svc_name,
							   state_color(event->r.adb.current_state->is),
							   event->r.adb.current_state->state_name);
						return;
					}
				default:
					break;
			}
			if (res->d.svc_watch.in_present
				&& (res->d.svc_watch.mode == WATCH_NORMAL || !quiet))
			{
				maybe_grab_out(NULL);
				maybe_printf("Service \"%s\" is now in state %s%s" C_OFF "\n",
							 event->svc_name,
							 state_color(event->r.adb.current_state->is),
							 event->r.adb.current_state->state_name);
			}
			break;
		case NGCS_SVC_EVT_END:
			/* FIXME - need to detect failure here */
			if (!res->d.svc_watch.got_svcs && (
				    res->d.svc_watch.mode == WATCH_START ||
				    res->d.svc_watch.mode == WATCH_STOP))
			{
				printf("Service \"%s\" " C_FG_RED "couldn't be started" 
				       C_OFF "\n", res->arg);
				failed = 1;
			}

			if (last_out == res)
				last_out = NULL;
			list_del(&res->list);
			free(res);
			break;
		default:
			break;
	}
}

void docmd(char *cmd, char *arg)
{
	ngcs_data dat[2];
	cmd_res *res = malloc(sizeof(cmd_res));

	if (res == NULL)
	{
		printf(C_ERROR "Malloc failed\n" C_OFF);
		exit(2);
	}

	res->cmd = cmd;
	res->arg = arg;
	res->list.prev = 0;
	res->list.next = 0;

	if (strcmp(cmd, "--watch") == 0)
	{
		res->d.svc_watch.in_present = 0;
		res->d.svc_watch.got_svcs = 0;
		res->d.svc_watch.mode = WATCH_NORMAL;
		if (ngcs_watch_service(cconn, arg, NGCS_WATCH_OUTPUT |
							   NGCS_WATCH_STATUS, svc_watch_cb, res) == NULL)
		{
			grab_out(NULL);
			printf(C_ERROR "Couldn't send command %s %s\n" C_OFF,
				   cmd, (arg == NULL ? "" : arg));
			failed = 1;
			free(res);
			return;
		}

		list_add(&res->list, &pending.list);
		return;
	}
	else if (strcmp(cmd, "--status") == 0 || strcmp(cmd, "-s") == 0)
	{
		res->d.svc_watch.in_present = 0;
		res->d.svc_watch.got_svcs = 0;
		res->d.svc_watch.mode = WATCH_STATUS;
		if (ngcs_watch_service(cconn, arg, NGCS_CURRENT_STATUS,
							   svc_watch_cb, res) == NULL)
		{
			grab_out(NULL);
			printf(C_ERROR "Couldn't send command %s %s\n" C_OFF,
				   cmd, (arg == NULL ? "" : arg));
			failed = 1;
			free(res);
			return;
		}

		list_add(&res->list, &pending.list);
		return;
	}
	else if (strcmp(cmd, "--start") == 0 || strcmp(cmd, "-u") == 0)
	{
		res->d.svc_watch.in_present = 0;
		res->d.svc_watch.got_svcs = 0;
		res->d.svc_watch.mode = WATCH_START;
		if (ngcs_start_stop(cconn, "start", arg, svc_watch_cb, res) == NULL)
		{
			grab_out(NULL);
			printf(C_ERROR "Couldn't send command %s %s\n" C_OFF,
				   cmd, (arg == NULL ? "" : arg));
			failed = 1;
			free(res);
			return;
		}

		list_add(&res->list, &pending.list);
		return;
	}
	else if (strcmp(cmd, "--stop") == 0 || strcmp(cmd, "-d") == 0)
	{
		res->d.svc_watch.in_present = 0;
		res->d.svc_watch.got_svcs = 0;
		res->d.svc_watch.mode = WATCH_STOP;
		if (ngcs_start_stop(cconn, "stop", arg, svc_watch_cb, res) == NULL)
		{
			grab_out(NULL);
			printf(C_ERROR "Couldn't send command %s %s\n" C_OFF,
				   cmd, (arg == NULL ? "" : arg));
			failed = 1;
			free(res);
			return;
		}

		list_add(&res->list, &pending.list);
		return;
	}

	dat[0].type = NGCS_TYPE_STRING;
	dat[0].len = -1;
	dat[0].d.s = cmd;

	dat[1].type = NGCS_TYPE_STRING;
	dat[1].len = -1;
	dat[1].d.s = arg;

	if (ngcs_cmd_async(cconn, (arg == NULL ? 1 : 2), dat, resp_handler, res))
	{
		grab_out(NULL);
		printf(C_ERROR "Couldn't send command %s %s\n" C_OFF,
			   cmd, (arg == NULL ? "" : arg));
		failed = 1;
		free(res);
		return;
	}

	list_add(&res->list, &pending.list);
}

void resp_handler(ngcs_cli_conn * cconn, void *userdata, ngcs_data * ret)
{

	cmd_res *res = userdata;

	assert(res);

	if (ret == NULL || ret->len < 0)
	{
		grab_out(NULL);
		printf(C_ERROR "Didn't get response for command %s %s\n" C_OFF,
			   res->cmd, (res->arg == NULL ? "" : res->arg));
		failed = 1;
	}
	else
		switch (ret->type)
		{
			case NGCS_TYPE_NULL:
				maybe_grab_out(NULL);
				maybe_printf("%s %s returned " C_FG_BLUE "nothing" C_OFF "\n",
							 res->cmd, (res->arg == NULL ? "" : res->arg));
				break;
			case NGCS_TYPE_INT:
				maybe_grab_out(NULL);
				maybe_printf("%s %s returned %i\n",
							 res->cmd, (res->arg == NULL ? "" : res->arg),
							 ret->d.i);
				break;
			case NGCS_TYPE_LONG:
				maybe_grab_out(NULL);
				maybe_printf("%s %s returned %li\n",
							 res->cmd, (res->arg == NULL ? "" : res->arg),
							 ret->d.l);
				break;
			case NGCS_TYPE_BOOL:
				maybe_grab_out(NULL);
				maybe_printf("%s %s returned %s\n",
							 res->cmd, (res->arg == NULL ? "" : res->arg),
							 (ret->d.i ? "TRUE" : "FALSE"));
				if (!ret->d.i)
					failed = 1;
				break;
			case NGCS_TYPE_ERROR:
				{
					grab_out(NULL);
					printf(C_ERROR "%s %s failed: %s\n" C_OFF,
						   res->cmd, (res->arg == NULL ? "" : res->arg),
						   ret->d.s);
					failed = 1;
					break;
				}
			default:
				grab_out(NULL);
				printf(C_ERROR "%s %s returned unknown type %i\n" C_OFF,
					   res->cmd, (res->arg == NULL ? "" : res->arg),
					   ret->type);
				failed = 1;
				break;

		}

	list_del(&res->list);
	free(res);
}

/* THIS IS MAIN */
int main(int argc, char *argv[])
{
	int i;
	int cc = 1, last_sw = -1;
	char *prog_name = NULL;

	assert(argv[0]);
	INIT_LIST_HEAD(&pending.list);

	last_out = NULL;
	need_nl = 0;

	/*
	 * Skip path in Argv.
	 * example argv[0] == "/sbin/ngc" then Argv == "ngc"
	 * example argv[0] == "./ngstart" then Argv == "ngstart"
	 */
	{
		i = 0;

		/* skip to last char of "/sbin/ngc" */
		while (argv[0][i])
			i++;

		/* then walk backwards, to the first '/' found */
		while (i > 0 && argv[0][i] != '/')
		{
			i--;
		}

		/* Here we have the program name */
		prog_name = &argv[0][i];

		/* Make sure there is no stale '/' char in beginning */
		if (prog_name[0] == '/')
			prog_name++;

		/*
		 * Special case, libtool test adds lt-ngc to path
		 * when running in make dir.
		 * So translate "lt-ngc" to "ngc"
		 */
		if (prog_name[0] == 'l' && prog_name[1] == 't' && prog_name[2] == '-')
			prog_name += 3;

		/* Only for testing.
		 * printf("Argv: %s argv[0]: %s\n", Argv, argv[0]);
		 */
	}

	/* check if cmd-line contains "ngdc", if set debug */
	if (strncasecmp(prog_name, "ngd", 3) == 0)
	{
		printf(C_FG_YELLOW "Warning. This is ngdcs!" C_OFF "\n");
		debug = TRUE;
	}


	if (debug == FALSE && getuid() != 0)
	{
		printf(C_ERROR "You need root access to communicate with initng."
			   C_OFF "\n");
		exit(2);
	}

	cconn = ngcs_client_connect((debug ? NGCS_CLIENT_CONN_TEST : 0),
								NULL, NULL, NULL);

	if (cconn == NULL)
	{
		printf(C_ERROR "Could not connect to InitNG" C_OFF "\n");
		exit(2);
	}

	for (cc = 1; cc < argc; cc++)
	{
		if (argv[cc][0] == '-')
		{
			if (last_sw >= 0 && last_sw == cc - 1)
				docmd(argv[last_sw], NULL);
			last_sw = cc;
		}
		else
		{
			if (last_sw < 0)
			{
				printf(C_FG_RED "Bad usage" C_OFF "\n");
				ngcs_client_free(cconn);
				exit(2);
			}

			docmd(argv[last_sw], argv[cc]);
		}
	}

	if (last_sw >= 0 && last_sw == cc - 1)
		docmd(argv[last_sw], NULL);


	while (!list_empty(&pending.list) && !ngcs_client_conn_is_closed(cconn))
	{
		fd_set readset, writeset;
		int fd = ngcs_client_get_fd(cconn);
		int retval;

		FD_ZERO(&readset);
		FD_ZERO(&writeset);
		FD_SET(fd, &readset);
		if (ngcs_client_conn_has_pending_writes(cconn))
			FD_SET(fd, &writeset);

		retval = select(fd + 1, &readset, &writeset, NULL, NULL);

		if (retval < 0 && errno != EINTR)
		{
			printf(C_ERROR "select() failed!\n" C_OFF "\n");
			ngcs_client_free(cconn);
			exit(2);
		}

		if (retval > 0 && FD_ISSET(fd, &readset))
			ngcs_client_data_ready(cconn);
		if (retval > 0 && FD_ISSET(fd, &writeset))
			ngcs_client_write_ready(cconn);

		ngcs_client_dispatch(cconn);
	}

	grab_out(NULL);
	ngcs_client_free(cconn);
	return failed;
}
