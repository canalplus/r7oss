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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#define _GNU_SOURCE
#include <fcntl.h>
#include <sys/file.h>
#include <sys/types.h>
#include <signal.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/time.h>
#include <time.h>
#include <getopt.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <assert.h>
#include <errno.h>
#include <poll.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>
#include "usplash.h"

#include <initng.h>
#include <initng-paths.h>

#include "libngeclient.h"

int main(int argc, char *argv[]);


/*
 * mprintf, a sprintf clone that automaticly mallocs the string
 * and new content to same string applys after that content.
 */
static int usplash(const char *format, ...)
{
	va_list arg;				/* used for the variable lists */

	char *p = NULL;
	int add_len = 0;			/* This mutch more strings are we gonna alloc for */

	/*printf("\n\nmprintf(%s);\n", format); */


	/*
	 * format contains the minimum needed chars that
	 * are gonna be used, so we set that value and try
	 * with that.
	 */
	add_len = strlen(format) + 1;

	/*
	 * Now realloc the memoryspace containing the
	 * string so that the new apending string will
	 * have room.
	 * Also have a check that it succeds.
	 */
	/*printf("Changing size to %i\n", add_len); */
	p = malloc(add_len * sizeof(char));
	if (!p)
		return (FALSE);

	/* Ok, have a try until vsnprintf succeds */
	while (1)
	{
		/* start filling the newly allocaded area */
		va_start(arg, format);
		int done = vsnprintf(p, add_len, format, arg);

		va_end(arg);

		/* check if that was enouth */
		if (done > -1 && done < add_len)
		{
			int pipe_fd;

			pipe_fd = open(USPLASH_FIFO, O_WRONLY | O_NONBLOCK);

			if (pipe_fd == -1)
			{
				/* We can't really do anything useful here */
				return (FALSE);
			}

			write(pipe_fd, p, done + 1);

			usleep(100);
			free(p);
			close(pipe_fd);
			return (TRUE);
		}
		/*printf("BAD: done : %i, len: %i\n", done, add_len); */

		/* try increase it a bit. */
		add_len = (done < 0 ? add_len * 2 : done + 1);
		/*printf("Changing size to %i\n", add_len); */
		if (!(p = realloc(p, ((add_len) * sizeof(char)))))
			return (-1);

	}

	/* will never go here */
	return (FALSE);
}



static void connected(int pver, char *initng_version)
{
	usplash("TEXT Initng %s", initng_version);
	usplash("TEXT  by Jimmy Wennlund");
}

static void disconnected(void)
{
	usplash("TEXT Dissonnected from initng.");
	usplash("QUIT");
	exit(0);
}

int last_progress = 0;

static void service_change(char *service, e_is is, char *state, int pstart,
						   int pstop, char *service_type, int hidden)
{
	if (pstart)
	{
		if (last_progress != pstart)
		{
			usplash("PROGRESS %i", pstart + 10);
			last_progress = pstart;
		}
	}
	else if (pstop)
	{
		if (last_progress != pstop)
		{
			usplash("PROGRESS %i", pstop + 10);
			last_progress = pstop;
		}
	}

	/* dont display hidden once */
	if (hidden)
		return;

	switch (is)
	{
		case IS_DOWN:
			usplash("TEXT %s", service);
			usplash("SUCCESS DOWN");
			break;
		case IS_UP:
			usplash("TEXT %s", service);
			usplash("SUCCESS UP");
			break;
		case IS_FAILED:
			usplash("TEXT %s", service);
			usplash("FAILURE FAIL", service);
			break;
		default:
			break;
	}
}

static void service_output(char *service, char *process, char *output)
{
	/*usplash( "TEXT Service \"%s\" process \"%s\" outputed:\n%s\n", service,
	   process, output); */
}

static void err_msg(e_mt mt, char *file, char *func, int line, char *message)
{
	/*usplash( "TEXT Message mt: %i, file: %s, func: %s, line %i.\n%s\n",
	   mt, file, func, line, message); */
}

static void sys_state(h_sys_state state, char *runlevel)
{
	switch (state)
	{
		case STATE_UP:
			usplash("TEXT Runlevel %s.", runlevel);
			usplash("SUCCESS UP");
			usplash("QUIT");
			exit(0);
			break;
		case STATE_STARTING:
		case STATE_STOPPING:
		case STATE_ASE:
		case STATE_SERVICES_LOADED:
		case STATE_EXIT:
		case STATE_RESTART:
		case STATE_SULOGIN:
		case STATE_HALT:
		case STATE_POWEROFF:
		case STATE_REBOOT:
		case STATE_EXECVE:
		default:
			break;
	}
}

static void handle_event(nge_event * e)
{
	switch (e->state_type)
	{
		case SERVICE_STATE_CHANGE:
			service_change(e->payload.service_state_change.service,
						   e->payload.service_state_change.is,
						   e->payload.service_state_change.state_name,
						   e->payload.service_state_change.percent_started,
						   e->payload.service_state_change.percent_stopped,
						   e->payload.service_state_change.service_type,
						   e->payload.service_state_change.hidden);
			return;
		case SYSTEM_STATE_CHANGE:
			sys_state(e->payload.system_state_change.system_state,
					  e->payload.system_state_change.runlevel);
			return;
		case ERR_MSG:
			err_msg(e->payload.err_msg.mt,
					e->payload.err_msg.file,
					e->payload.err_msg.func,
					e->payload.err_msg.line, e->payload.err_msg.message);
			return;
		case CONNECT:
			connected(e->payload.connect.pver,
					  e->payload.connect.initng_version);
			return;
		case DISSCONNECT:
			disconnected();
			return;
		case SERVICE_OUTPUT:
			service_output(e->payload.service_output.service,
						   e->payload.service_output.process,
						   e->payload.service_output.output);
			return;
		default:
			return;
	}
}

/* THIS IS MAIN */
int main(int argc, char *argv[])
{
	nge_connection *c = NULL;
	nge_event *e = NULL;
	pid_t fork_pid;

	/* open up for usplash */
	if (chdir("/dev/.initramfs") != 0)
	{
		printf("Error chdir \"/dev/.initramfs\"\n");
		exit(0);
	}

	/* ignore all killtries */
	signal(SIGTERM, SIG_IGN);
	signal(SIGSTOP, SIG_IGN);
	signal(SIGKILL, SIG_IGN);

	/* launch usplash */
	fork_pid = fork();

	/* make sure this is the fork */
	if (fork_pid == 0)
	{
		const char *usplash_argv[] = { "/sbin/usplash", "-c", NULL };
		const char *usplash_env[] = { NULL };

		execve((char *) usplash_argv[0], (char **) usplash_argv,
			   (char **) usplash_env);
		_exit(0);
	}

	/* short sleep */
	usleep(10000);

	/* set some progress */
	usplash("TEXT HALT/REBOOT SYSTEM");
	usplash("PROGRESS 10");
	usplash("TIMEOUT 180");

	/* FROM HERE, IS run in a fork. */

	/* open correct socket */
	while (c == NULL)
	{
		/* reset error every time, or you get an can not connect to socket */
		ngeclient_error = NULL;
		c = ngeclient_connect(NGE_REAL);
		usleep(100);
	}

	/* if open_socket fails, ngeclient_error is set */
	if (ngeclient_error)
	{
		usplash("QUIT");
		fprintf(stderr, "NGECLIENT ERROR: %s\n", ngeclient_error);
		exit(0);
	}
	assert(c);

	while ((e = get_next_event(c, 200000)))
	{
		/*printf("Got an event: %i!\n", e->state_type); */
		handle_event(e);

		ngeclient_event_free(e);
	}

	/* clean up */
	ngeclient_close(c);

	/* check so there is no ngeclient_error set */
	if (ngeclient_error)
	{
		usplash("QUIT");
		fprintf(stderr, "NGECLIENT ERROR: %s\n", ngeclient_error);
		exit(0);
	}

	/* stop usplash daemon */
	usplash("QUIT");
	exit(0);
}
