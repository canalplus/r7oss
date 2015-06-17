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

#include <initng.h>
#include <initng-paths.h>

#include "libngeclient.h"
int main(int argc, char *argv[]);


static void connected(int pver, char *initng_version)
{
	fprintf(stdout, "Connected to initng %s\n", initng_version);
}

static void disconnected(void)
{
	fprintf(stdout, "Dissonnected from initng.\n");
}

static void process_killed(char *service, e_is is, char *state, char *process,
						   int exit_status, int term_sig)
{
	fprintf(stdout,
			"Service \"%s\" state \"%s\" (%i) process \"%s\" killed: exit_status \"%i\" term_sig \"%i\"\n",
			service, state, is, process, exit_status, term_sig);

}


static void service_change(char *service, e_is is, char *state, int pstart,
						   int pstop, char *service_type, int hidden)
{
	switch (is)
	{
		case IS_UP:
		case IS_DOWN:
		case IS_FAILED:
		case IS_STARTING:
		case IS_STOPPING:
		case IS_WAITING:
		default:
			fprintf(stdout,
					"%sService \"%s\"::\"%s\" have state \"%s\" (%i)  %i:%i\n",
					hidden ? "<HIDDEN> " : "", service_type, service, state,
					is, pstart, pstop);
			break;
	}
}

static void initial_state_finished(void)
{
	fprintf(stdout, "Initial initng state finished.");
}

static void ping(void)
{
	fprintf(stdout, "Got an ping from initng.");
}

static void service_output(char *service, char *process, char *output)
{
	fprintf(stdout, "Service \"%s\" process \"%s\" outputed:\n%s\n", service,
			process, output);
}

static void err_msg(e_mt mt, char *file, char *func, int line, char *message)
{
	fprintf(stdout, "Message mt: %i, file: %s, func: %s, line %i.\n%s\n",
			mt, file, func, line, message);
}

static void sys_state(h_sys_state state, char *runlevel)
{
	switch (state)
	{
		case STATE_STARTING:
		case STATE_UP:
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
			fprintf(stdout, "Initng [%s] got a new system state no: %i\n",
					runlevel, state);
			break;
	}
}

static void handle_event(nge_event * e)
{
	switch (e->state_type)
	{
		case PING:
			ping();
			return;
		case SERVICE_STATE_CHANGE:
		case INITIAL_SERVICE_STATE_CHANGE:
			service_change(e->payload.service_state_change.service,
						   e->payload.service_state_change.is,
						   e->payload.service_state_change.state_name,
						   e->payload.service_state_change.percent_started,
						   e->payload.service_state_change.percent_stopped,
						   e->payload.service_state_change.service_type,
						   e->payload.service_state_change.hidden);
			return;
		case SYSTEM_STATE_CHANGE:
		case INITIAL_SYSTEM_STATE_CHANGE:
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

		case INITIAL_STATE_FINISHED:
			initial_state_finished();
			return;

		case SERVICE_OUTPUT:
			service_output(e->payload.service_output.service,
						   e->payload.service_output.process,
						   e->payload.service_output.output);
			return;
		case PROCESS_KILLED:
			process_killed(e->payload.process_killed.service,
						   e->payload.process_killed.is,
						   e->payload.process_killed.state_name,
						   e->payload.process_killed.process,
						   e->payload.process_killed.exit_status,
						   e->payload.process_killed.term_sig);
			return;
	}
}

/* THIS IS MAIN */
int main(int argc, char *argv[])
{
	nge_connection *c = NULL;
	nge_event *e = NULL;


	/* open correct socket */
	if (strstr(argv[0], "ngde"))
		c = ngeclient_connect(NGE_TEST);
	else
		c = ngeclient_connect(NGE_REAL);

	/* if open_socket fails, ngeclient_error is set */
	if (ngeclient_error)
	{
		fprintf(stderr, "NGECLIENT ERROR: %s\n", ngeclient_error);
		exit(1);
	}
	assert(c);

	while ((e = get_next_event(c, 20000)))
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
		fprintf(stderr, "NGECLIENT ERROR: %s\n", ngeclient_error);
		exit(1);
	}

	exit(0);
}
