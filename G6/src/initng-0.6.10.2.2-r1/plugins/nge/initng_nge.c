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
#include <initng_fd.h>
#include <initng_string_tools.h>

#include <initng-paths.h>

#include "initng_nge.h"

INITNG_PLUGIN_MACRO;

/* globals */
struct stat sock_stat;
const char *socket_filename;

#define MAX_LISTENERS 20
int listeners[MAX_LISTENERS + 1];

int is_active = FALSE;

void event_acceptor(f_module_h * from, e_fdw what);
static void close_all_listeners(void);
static void close_initiator_socket(void);
static int open_initiator_socket(void);
static int check_socket(s_event * event);
void send_to_all(const void *buf, size_t len);


static int astatus_change(s_event * event);
static int system_state_change(s_event * event);
static int system_pipe_watchers(s_event * event);
static int print_error(s_event * event);
static int fd_event_acceptor_handler(s_event * event);

/* todo, when last listener closed, del hooks to save cpu cykles */

f_module_h fd_event_acceptor = { &event_acceptor, FDW_READ, -1 };


static int fd_event_acceptor_handler(s_event * event)
{
	s_event_fd_watcher_data * data;

	assert(event);
	assert(event->data);

	data = event->data;

	switch (data->action)
	{
		case FDW_ACTION_CLOSE:
			if (fd_event_acceptor.fds > 0)
				close(fd_event_acceptor.fds);
			break;

		case FDW_ACTION_CHECK:
			if (fd_event_acceptor.fds <= 2)
				break;

			/* This is a expensive test, but better safe then sorry */
			if (!STILL_OPEN(fd_event_acceptor.fds))
			{
				D_("%i is not open anymore.\n", fd_event_acceptor.fds);
				fd_event_acceptor.fds = -1;
				break;
			}

			FD_SET(fd_event_acceptor.fds, data->readset);
			data->added++;
			break;

		case FDW_ACTION_CALL:
			if (!data->added || fd_event_acceptor.fds <= 2)
				break;

			if(!FD_ISSET(fd_event_acceptor.fds, data->readset))
				break;

			event_acceptor(&fd_event_acceptor, FDW_READ);
			data->added--;
			break;

		case FDW_ACTION_DEBUG:
			if (!data->debug_find_what || strstr(__FILE__, data->debug_find_what))
				mprintf(data->debug_out, " %i: Used by plugin: %s\n",
					fd_event_acceptor.fds, __FILE__);
			break;
	}

	return (TRUE);
}

static void close_all_listeners(void)
{
	int i;

	for (i = 0; i < MAX_LISTENERS; i++)
	{
		if (listeners[i] > 0)
		{
			send(listeners[i], "</disconnect>\n", sizeof(char) * 16, 0);

			close(listeners[i]);
			listeners[i] = -1;
		}
	}
}

static int handle_killed(s_event * event)
{
	s_event_handle_killed_data * data;
	char *buffert = NULL;
	int len;

	assert(event->event_type == &EVENT_HANDLE_KILLED);
	assert(event->data);

	data = event->data;

	buffert = i_calloc(180 + strlen(data->service->name) +
					   strlen(data->service->current_state->state_name) +
					   strlen(data->process->pt->name), sizeof(char));

	len = sprintf(buffert,
				  "<event type=\"process_killed\" service=\"%s\" is=\"%i\" state=\"%s\" process=\"%s\" exit_status=\"%i\" term_sig=\"%i\"/>\n",
				  data->service->name, data->service->current_state->is,
				  data->service->current_state->state_name,
				  data->process->pt->name, WEXITSTATUS(data->process->r_code),
				  WTERMSIG(data->process->r_code));

	if (len > 1)
		send_to_all(buffert, sizeof(char) * len);

	free(buffert);

	/* AlwaYs, AlwaYs, return FALSE to mark this killed process as NOT handled */
	return (FALSE);
}

static void close_initiator_socket(void)
{
	/* Check if we need to remove hooks */
	if (fd_event_acceptor.fds < 0)
		return;

	/* only remove if hook is added */
	if (is_active)
	{
		/*
		 * UnRegister that hooks, that we forwards events from.
		 */
		initng_event_hook_unregister(&EVENT_STATE_CHANGE, &astatus_change);
		initng_event_hook_unregister(&EVENT_SYSTEM_CHANGE, &system_state_change);
		initng_event_hook_unregister(&EVENT_BUFFER_WATCHER, &system_pipe_watchers);
		initng_event_hook_unregister(&EVENT_ERROR_MESSAGE, &print_error);
		initng_event_hook_unregister(&EVENT_HANDLE_KILLED, &handle_killed);

		is_active = FALSE;
	}

	/* close socket and set to 0 */
	close(fd_event_acceptor.fds);
	fd_event_acceptor.fds = -1;

	/* remove tha hook too */
	initng_event_hook_unregister(&EVENT_FD_WATCHER, &fd_event_acceptor_handler);
}

/* send to all listeners */
void send_to_all(const void *buf, size_t len)
{
	D_("send_to_all(%s)\n", (char *) buf);
	int i;

	/* walk all lissiners */
	for (i = 0; i < MAX_LISTENERS; i++)
	{
		/* if its not set */
		if (listeners[i] < 1)
			continue;

		D_("Sending to listeners[%i] fd %i : %s\n", i, listeners[i],
		   (char *) buf);
		/* if not succed to send */
		if (send(listeners[i], buf, len, 0) < (signed) len)
		{
			D_("Fd %i must have been closed.\n", listeners[i]);
			/* close it */
			close(listeners[i]);
			listeners[i] = -1;
		}
	}

}

/* called by fd hook, when data is no socket */
void event_acceptor(f_module_h * from, e_fdw what)
{
	/* Temporary variables for sending data */
	char *string = NULL;
	int len;

	/* the fd the new listeners will get */
	int lis = 0;

	/* make a dumb check */
	if (from != &fd_event_acceptor)
		return;

	/* skipp all set listeners, so we dont owerwrite them */
	while (listeners[lis] > 0 && lis != MAX_LISTENERS)
		lis++;
	if (lis == MAX_LISTENERS)
	{
		F_("Maximum no of listeners reached.\n");
		return;
	}

	D_("Adding new listener listensers[%i]\n", lis);
	if (is_active == FALSE)
	{
		/*
		 * Register that hooks, that we forwards events from.
		 */
		initng_event_hook_register(&EVENT_STATE_CHANGE, &astatus_change);
		initng_event_hook_register(&EVENT_SYSTEM_CHANGE, &system_state_change);
		initng_event_hook_register(&EVENT_BUFFER_WATCHER, &system_pipe_watchers);
		initng_event_hook_register(&EVENT_ERROR_MESSAGE, &print_error);
		initng_event_hook_register(&EVENT_HANDLE_KILLED, &handle_killed);
		is_active = TRUE;
	}
	/* create a new socket, for reading */
	if ((listeners[lis] = accept(fd_event_acceptor.fds, NULL, NULL)) < 1)
	{
		F_("Failed to accept listener!\n");
		return;
	}

	D_("opening listener no #%i.\n", lis);

	/* send header */
	{
		send(listeners[lis], "<? xml version=\"1.0\" ?/>\n",
			 sizeof(char) * 25, 0);
	}

	/* send protocol info */
	{
		string = i_calloc(70 + strlen(INITNG_VERSION), sizeof(char));

		/* Make a string ready for sending */
		len = sprintf(string,
					  "<connect protocol_version=\"%i\", initng_version=\"%s\"/>\n",
					  NGE_VERSION, INITNG_VERSION);

		/* send the init string to this socket */
		send(listeners[lis], string, sizeof(char) * len, 0);

		/* free the initial string */
		free(string);
		string = NULL;
	}

	/* send system initiating state */
	{
		if (g.runlevel)
		{
			string = i_calloc(100 + strlen(g.runlevel), sizeof(char));
			len = sprintf(string,
						  "<event type=\"initial_system_state\" system_state=\"%i\" runlevel=\"%s\" />\n",
						  g.sys_state, g.runlevel);
		}
		else
		{
			string = i_calloc(100, sizeof(char));
			len = sprintf(string,
						  "<event type=\"initial_system_state\" system_state=\"%i\" runlevel=\"\" />\n",
						  g.sys_state);
		}
		/* send the init string to this socket */
		send(listeners[lis], string, sizeof(char) * len, 0);

		/* free the initial string */
		free(string);
		string = NULL;
	}

	/* send all current services states */
	{
		active_db_h *service = NULL;

		while_active_db(service)
		{
			string = i_realloc(string, (160 + strlen(service->name) +
										strlen(service->current_state->
											   state_name) +
										strlen(service->type->name)) *
							   sizeof(char));

			len = sprintf(string,
						  "<event type=\"initial_service_state\" service=\"%s\" is=\"%i\" state=\"%s\" service_type=\"%s\" hidden=\"%i\"/>\n",
						  service->name, service->current_state->is,
						  service->current_state->state_name,
						  service->type->name, service->type->hidden);

			/* send the init string to this socket */
			send(listeners[lis], string, sizeof(char) * len, 0);
		}

		if (string)
		{
			/* free the initial string */
			free(string);
			string = NULL;
		}
	}

	/* tell client initialization is finished */
	{
		string = i_calloc(50, sizeof(char));
		len = sprintf(string, "<event type=\"initial_state_finished\" />\n");

		/* send the init string to this socket */
		send(listeners[lis], string, sizeof(char) * len, 0);

		/* free the initial string */
		free(string);
		string = NULL;
	}

}


/* This will try to open a new socket, clients can iniziate to */
static int open_initiator_socket(void)
{
	/*    int flags; */
	struct sockaddr_un serv_sockname;

	/* Close the iniztiator */
	close_initiator_socket();

	/* Make NGE_PREFIX (/dev/initng) if it doesn't exist (try either way) */
	if (mkdir(NGE_PREFIX, S_IRUSR | S_IWUSR | S_IXUSR) == -1
		&& errno != EEXIST)
	{
		if (errno != EROFS)
			F_("Could not create " NGE_PREFIX
			   " : %s, may be / fs not mounted read-write yet?, will retry until I succeed.\n",
			   strerror(errno));
		return (FALSE);
	}

	/* chmod root path for root use only */
	if (chmod(NGE_PREFIX, S_IRUSR | S_IWUSR | S_IXUSR) == -1)
	{
		/* path doesn't exist, we don't have /dev yet. */
		if (errno == ENOENT || errno == EROFS)
			return (FALSE);

		F_("CRITICAL, failed to chmod %s, THIS IS A SECURITY PROBLEM.\n",
		   NGE_PREFIX);
	}

	/* Create the socket. */
	fd_event_acceptor.fds = socket(PF_UNIX, SOCK_STREAM, 0);
	if (fd_event_acceptor.fds < 1)
	{
		F_("Failed to init socket (%s)\n", strerror(errno));
		fd_event_acceptor.fds = -1;
		return (FALSE);
	}

	initng_fd_set_cloexec(fd_event_acceptor.fds);

	/* Bind a name to the socket. */
	serv_sockname.sun_family = AF_UNIX;

	/* get the socket_filename, set in module_init() */
	strcpy(serv_sockname.sun_path, socket_filename);

	/* Remove old socket file if any */
	unlink(serv_sockname.sun_path);

	/* Try to bind */
	if (bind
		(fd_event_acceptor.fds, (struct sockaddr *) &serv_sockname,
		 (strlen(serv_sockname.sun_path) +
		  sizeof(serv_sockname.sun_family))) < 0)
	{
		F_("Error binding to socket (errno: %d str: '%s')\n", errno,
		   strerror(errno));
		close_initiator_socket();
		unlink(serv_sockname.sun_path);
		return (FALSE);
	}

	/* chmod socket for root only use */
	if (chmod(serv_sockname.sun_path, S_IRUSR | S_IWUSR) == -1)
	{
		F_("CRITICAL, failed to chmod %s, THIS IS A SECURITY PROBLEM.\n",
		   serv_sockname.sun_path);
		close_initiator_socket();
		return (FALSE);
	}

	/* store sock_stat for checking if we need to recreate socket later */
	stat(serv_sockname.sun_path, &sock_stat);

	/* Listen to socket */
	if (listen(fd_event_acceptor.fds, 5))
	{
		F_("Error on listen (errno: %d str: '%s')\n", errno, strerror(errno));
		close_initiator_socket();
		unlink(serv_sockname.sun_path);
		return (FALSE);
	}

	/*
	 * Add an hook, so when fd_event_acceptor.fds have data,
	 * fd_event_acceptor.call (event_acceptor()) is called.
	 */
	initng_event_hook_register(&EVENT_FD_WATCHER, &fd_event_acceptor_handler);

	/* return happily */
	return (TRUE);
}

/* this will check socket, and reopen on failure */
static int check_socket(s_event * event)
{
	int *signal;
	struct stat st;

	assert(event->event_type == &EVENT_SIGNAL);

	signal = event->data;

	if (*signal != SIGHUP)
		return (TRUE);

#define PING "<event type=\"ping\"/>\n"
	send_to_all(PING, sizeof(char) * strlen(PING));
	D_("Checking socket\n");

	/* Check if socket needs reopening */
	if (fd_event_acceptor.fds <= 0)
	{
		D_("fd_event_acceptor.fds not set, opening new socket.\n");
		open_initiator_socket();
		return (TRUE);
	}

	/* stat the socket, reopen on failure */
	memset(&st, 0, sizeof(st));
	if (stat(socket_filename, &st) < 0)
	{
		W_("Stat failed! Opening new socket.\n");
		open_initiator_socket();
		return (TRUE);
	}

	/* compare socket file, with the one that we know, reopen on failure */
	if (st.st_dev != sock_stat.st_dev || st.st_ino != sock_stat.st_ino
		|| st.st_mtime != sock_stat.st_mtime)
	{
		F_("Invalid socket found, reopening\n");
		open_initiator_socket();
		return (TRUE);
	}

	D_("Socket ok.\n");
	return (TRUE);
}

static int astatus_change(s_event * event)
{
	active_db_h * service;
	char *buffert = NULL;
	int len;

	assert(event->event_type == &EVENT_STATE_CHANGE);
	assert(event->data);

	service = event->data;

	buffert = i_calloc(180 + strlen(service->name) +
					   strlen(service->current_state->state_name) +
					   strlen(service->type->name), sizeof(char));

	if (g.sys_state == STATE_STARTING)
		len = sprintf(buffert,
					  "<event type=\"service_state_change\" service=\"%s\" is=\"%i\" state=\"%s\" percent_started=\"%i\" percent_stopped=\"0\" service_type=\"%s\" hidden=\"%i\"/>\n",
					  service->name, service->current_state->is,
					  service->current_state->state_name,
					  initng_active_db_percent_started(),
					  service->type->name, service->type->hidden);
	else if (g.sys_state == STATE_STOPPING)
		len = sprintf(buffert,
					  "<event type=\"service_state_change\" service=\"%s\" is=\"%i\" state=\"%s\" percent_started=\"0\" percent_stopped=\"%i\" service_type=\"%s\" hidden=\"%i\"/>\n",
					  service->name, service->current_state->is,
					  service->current_state->state_name,
					  initng_active_db_percent_stopped(),
					  service->type->name, service->type->hidden);
	else
		len = sprintf(buffert,
					  "<event type=\"service_state_change\" service=\"%s\" is=\"%i\" state=\"%s\" percent_started=\"0\" percent_stopped=\"0\" service_type=\"%s\" hidden=\"%i\"/>\n",
					  service->name, service->current_state->is,
					  service->current_state->state_name,
					  service->type->name, service->type->hidden);

	/*printf("astatus_change: %s %i %s\n",
	   service->name, service->current_state->is,
	   service->current_state->state_name); */

	if (len > 1)
		send_to_all(buffert, sizeof(char) * len);

	free(buffert);
	return (TRUE);
}

static int system_state_change(s_event * event)
{
	e_is * state;
	char *buffert = i_calloc(90 + strlen(g.runlevel), sizeof(char));
	int len;

	assert(event->event_type == &EVENT_SYSTEM_CHANGE);
	assert(event->data);

	state = event->data;

	len = sprintf(buffert,
				  "<event type=\"system_state_change\" system_state=\"%i\" runlevel=\"%s\" />\n",
				  *state, g.runlevel);
	if (len > 1)
		send_to_all(buffert, sizeof(char) * len);

	free(buffert);

	return (TRUE);
}

static int system_pipe_watchers(s_event * event)
{
	s_event_buffer_watcher_data * data;
	char *buffert = NULL;
	int len;

	assert(event->event_type == &EVENT_BUFFER_WATCHER);
	assert(event->data);

	data = event->data;

	buffert = i_calloc(100 + strlen(data->service->name) +
					   strlen(data->process->pt->name) + strlen(data->buffer_pos),
					   sizeof(char));

	len = sprintf(buffert,
				  "<event type=\"service_output\" service=\"%s\" process=\"%s\">%s</event>\n",
				  data->service->name, data->process->pt->name, data->buffer_pos);

	if (len > 0)
		send_to_all(buffert, sizeof(char) * len);

	/* free buffert */
	free(buffert);

	/* return, output not handled */
	return (FALSE);
}

static int print_error(s_event * event)
{
	s_event_error_message_data * data;
	char *buffert = NULL;
	char *msg;
	int len, size;
	va_list va;

	assert(event->event_type == &EVENT_ERROR_MESSAGE);
	assert(event->data);

	data = event->data;
	va_copy(va, data->arg);

	size = 256;
	msg = i_calloc(1, size);
	len = vsnprintf(msg, size, data->format, va);
	va_end(va);
	while (len < 0 || len >= size)
	{
		/* Some glibc versions apparently return -1 if buffer too small.
		   Oh, and the argument counts the null char, but the return
		   value doesn't, apparently. (See man page for gory details.) */
		size = (len < 0 ? size * 2 : len + 1);
		free(msg);
		msg = i_calloc(1, size);
		va_copy(va, data->arg);
		len = vsnprintf(msg, size, data->format, va);
		va_end(va);
	}

	buffert = i_calloc(100 + len + strlen(data->file) + strlen(data->func), sizeof(char));

	len = sprintf(buffert,
				  "<event type=\"err_msg\" mt=\"%i\" file=\"%s\" func=\"%s\" line=\"%i\">%s</event>\n",
				  data->mt, data->file, data->func, data->line, msg);

	send_to_all(buffert, sizeof(char) * len);

	free(msg);
	free(buffert);

	return (FALSE);
}


int module_init(int api_version)
{
	int i;

	if (api_version != API_VERSION)
	{
		F_("This module is compiled for api_version %i version and initng is compiled on %i version, won't load this module!\n", API_VERSION, api_version);
		return (FALSE);
	}

	/* clear listeners struct */
	for (i = 0; i < MAX_LISTENERS; i++)
		listeners[i] = -1;

	/* zero globals */
	fd_event_acceptor.fds = -1;
	memset(&sock_stat, 0, sizeof(sock_stat));

	/*
	 * Decide witch path to socket to use.
	 * set socket_filename to right path
	 */

	/* decide which socket to use */
	if (g.i_am == I_AM_INIT)
		socket_filename = NGE_REAL;
	else if (g.i_am == I_AM_FAKE_INIT)
		socket_filename = NGE_TEST;
	else
		return (FALSE);
	D_("Socket is: %s\n", socket_filename);


	/*
	 * Giving initng a SIGHUP, will make initng check that all sockets are open,
	 * and reopen the sockets that have been deleted.
	 */
	initng_event_hook_register(&EVENT_SIGNAL, &check_socket);

	/* do the first socket directly */
	open_initiator_socket();


	/* return happily */
	return (TRUE);
}




void module_unload(void)
{

	/* close initiator socket */
	close_initiator_socket();

	/* dissconect all listeners */
	close_all_listeners();

	/* remove EVENT_SIGNAL check hook */
	initng_event_hook_unregister(&EVENT_SIGNAL, &check_socket);

}
