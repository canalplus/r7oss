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
#include <initng_string_tools.h>
#include <initng_fd.h>

#include <initng-paths.h>

#include "initng_ngc4.h"

INITNG_PLUGIN_MACRO;

static void accepted_client(f_module_h * from, e_fdw what);
static void closesock(void);
static void handle_client(int fd);
static int sendping(void);
static int open_socket(void);
static int check_socket(s_event * event);
static int fdh_handler(s_event * event);

s_command local_commands_db;

const char *module_needs[] = {
	"stcmd",
	NULL
};

/*
   In the Linux implementation, sockets which are visible in the file system
   honour the permissions of the directory they are in.  Their owner, group
   and their permissions can be changed.  Creation of a new socket will
   fail if the process does not have write and search (execute) permission
   on the directory the socket is created in.  Connecting to the socket
   object requires read/write permission.  This behavior differs from  many
   BSD derived systems which ignore permissions for Unix sockets.  Portable
   programs should not rely on this feature for security.
 */


/* globals */
struct stat sock_stat;
const char *socket_filename;

f_module_h fdh = { &accepted_client, FDW_READ, -1 };


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

			accepted_client(&fdh, FDW_READ);
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

/* Function to search for local commands, bound only to ngc4 */
static s_command *lfbc(char cid)
{
	s_command *current = NULL;

	list_for_each_entry_prev(current, &local_commands_db.list, list)
	{
		if (current->command_id == cid)
			return (current);
	}
	return (NULL);
}

static s_command *lfbs(char *name)
{
	s_command *current = NULL;

	list_for_each_entry_prev(current, &local_commands_db.list, list)
	{
		if (current->long_id && strcmp(current->long_id, name) == 0)
			return (current);
	}
	return (NULL);
}


static void closesock(void)
{
	/* Check if we need to remove hooks */
	if (fdh.fds < 0)
		return;
	D_("closesock %d\n", fdh.fds);

	/* close socket and set to 0 */
	close(fdh.fds);
	fdh.fds = -1;
}


/* this is called on protocol_missmatch to reload initng if that plugin is present */
static void initng_reload(void)
{
	s_command *reload;


	/*
	 * Now we are going to try reload initng
	 *
	 * ngc -c  is the reload command
	 */
	reload = initng_command_find_by_command_id('c');
	if (reload && reload->u.void_command_call)
	{
		/* call the reload call in that s_command */
		(*reload->u.void_command_call) (NULL);
	}
}


static void handle_client(int fd)
{
	result_desc *result = NULL;
	read_header header;
	void *header_data = NULL;
	ssize_t sent = 0;

	s_command *tmp_cmd;			/* temporary storage for a command */

	assert(fd > 0);

	D_("handle_client(%i);\n", fd);

	/* use file descriptor, because fread hangs here? */
	if (TEMP_FAILURE_RETRY(recv(fd, &header, sizeof(read_header), 0)) <
		(signed) sizeof(read_header))
	{
		F_("Could not read header.\n");
		return;
	}

	if (header.p_ver != PROTOCOL_4_VERSION)
	{
		F_("ngc protcol_version miss-match, server_protocol_version :%i, client_protocol_version :%i !\n Will try to hot-reload initng.", PROTOCOL_4_VERSION, header.p_ver);
		initng_reload();
		return;
	}

	header.l[100] = '\0';
	D_("command type '%c', long \"%s\", protocol_version %i\n", header.c,
	   header.l, header.p_ver);
	if (header.body_len > 0)
	{
		D_("There is a body to read!\n");
		header_data = i_calloc(1, header.body_len + 1);
		if (!header_data)
		{
			F_("Could not allocate memory for header_data\n");
			return;
		}

		/* yost so ngc got a chance to fill header_data again */
		usleep(10);

		if (TEMP_FAILURE_RETRY(recv(fd, header_data, header.body_len, 0)) <
			(signed) header.body_len)
		{
			F_("Could not read header_data\n");
			if (header_data)
				free(header_data);
			return;
		}
	}

	/* allocate space for the result we will send */
	result = i_calloc(1, sizeof(result_desc));
	if (!result)
		return;

	/* set version */
	strncpy(result->version, INITNG_VERSION, 100);
	result->p_ver = PROTOCOL_4_VERSION;

	/* ping check : */
	if (header.c == 'X')
	{
		result->c = 'Y';
		result->t = VOID_COMMAND;
		result->s = S_TRUE;
		result->payload = 0;
		D_("Ping received, sending pong\n");
		send(fd, result, sizeof(result_desc), 0);
		if (header_data)
			free(header_data);
		if (result)
			free(result);
		return;
	}

	/* Find the command requesting in the command database */
	if (header.c)							/* find by short opt */
	{
		/* first search in the local db */
		tmp_cmd = lfbc(header.c);
		if (!tmp_cmd)
			tmp_cmd = initng_command_find_by_command_id(header.c);
	}
	else if (header.l)						/* find by long opt */
	{
		/* first search in the local db */
		tmp_cmd = lfbs(header.l);
		if (!tmp_cmd)
			tmp_cmd = initng_command_find_by_command_string(header.l);
	}
	else
	{
		if (header_data)
			free(header_data);
		if (result)
			free(result);
		return;
	}

	/* Make sure the command we got is valid, else return an bad result */
	if (!tmp_cmd || tmp_cmd->com_type == 0)
	{
		D_("command type '%c', long \"%s\"\n", header.c, header.l);
		result->c = header.c;
		result->t = COMMAND_FAIL;
		result->payload = 0;
		result->s = S_COMMAND_NOT_FOUND;
		send(fd, result, sizeof(result_desc), 0);
		if (header_data)
			free(header_data);
		if (result)
			free(result);
		return;
	}

	/* check if command requires option, and option is not set */
	if (tmp_cmd->opt_type == REQUIRES_OPT && header.body_len < 1)
	{
		D_("Command %c - %s, requires an option!\n", header.c, header.l);
		result->c = header.c;
		result->t = COMMAND_FAIL;
		result->payload = 0;
		result->s = S_REQUIRES_OPT;
		send(fd, result, sizeof(result_desc), 0);
		if (header_data)
			free(header_data);
		return;
	}

	/* check if command is not using and option, and option is set */
	if (tmp_cmd->opt_type == NO_OPT && header.body_len > 0)
	{
		D_("Command %c - %s, don't want an option!\n", header.c, header.l);
		result->c = header.c;
		result->t = COMMAND_FAIL;
		result->payload = 0;
		result->s = S_NOT_REQUIRES_OPT;
		send(fd, result, sizeof(result_desc), 0);
		if (header_data)
			free(header_data);
		return;
	}

	/* set the result statics. */
	result->c = tmp_cmd->command_id;
	result->t = tmp_cmd->com_type;

	switch (tmp_cmd->com_type)
	{

		case TRUE_OR_FALSE_COMMAND:
		case INT_COMMAND:
			{
				int ret = 0;

				assert(tmp_cmd->u.int_command_call);
				D_("Calling an int or true or false command.\n");

				/* execute command */
				ret = (int) (*tmp_cmd->u.
							 int_command_call) ((char *) header_data);

				/* Write a header respond */
				result->s = S_TRUE;
				result->payload = sizeof(int);

				/* send the result */
				if ((sent =
					 send(fd, result, sizeof(result_desc),
						  0)) != sizeof(result_desc))
				{
					F_("failed to send result, sent: %i of %i.\n", sent,
					   sizeof(result_desc));
					break;
				}

				/* send the payload */
				if ((sent =
					 send(fd, &ret, result->payload,
						  0)) != (signed) result->payload)
				{
					F_("Could not send complete payload, sent %i of %i.",
					   sent, result->payload);
					break;
				}
				break;
			}
		case STRING_COMMAND:
			{
				char *send_buf = NULL;

				assert(tmp_cmd->u.string_command_call);
				D_("Calling an string command.\n");

				/* execute command */
				send_buf = (*tmp_cmd->u.
							string_command_call) ((char *) header_data);
				if (!send_buf)
					break;

				/* write an header respond */
				result->s = S_TRUE;
				result->payload = strlen(send_buf) * sizeof(char);

				/* send the result */
				if ((sent =
					 send(fd, result, sizeof(result_desc),
						  0)) != sizeof(result_desc))
				{
					F_("failed to send result, sent: %i of %i.\n", sent,
					   sizeof(result_desc));
					free(send_buf);
					break;
				}
				/* send the payload */
				if ((sent =
					 send(fd, send_buf, result->payload,
						  0)) != (signed) result->payload)
				{
					F_("Could not send complete payload, sent %i of %i.",
					   sent, result->payload);
					free(send_buf);
					break;
				}

				/* free and clear */
				free(send_buf);
				break;
			}
		case VOID_COMMAND:
			{
				assert(tmp_cmd->u.void_command_call);
				D_("Calling a void command!\n");

				/* execute command */
				(*tmp_cmd->u.void_command_call) ((char *) header_data);

				/* write an header respond */
				result->s = S_TRUE;
				result->payload = 0;		/* unknown payload size here, TODO FIX THIS */

				/* sent result */
				if ((sent =
					 send(fd, result, sizeof(result_desc),
						  0)) != sizeof(result_desc))
				{
					F_("failed to send result, sent: %i of %i.\n", sent,
					   sizeof(result_desc));
					break;
				}
				break;
			}
		case PAYLOAD_COMMAND:
			{
				/* iniziate a data payload variable */
				s_payload payload;

				/* clear payload */
				memset(&payload, 0, sizeof(s_payload));

				assert(tmp_cmd->u.data_command_call);
				D_("Calling an data_command.\n");

				/* execute command */
				(*tmp_cmd->u.data_command_call) ((char *) header_data,
												 &payload);

				/* check that there was any payload */
				if (payload.s < 1)
				{
					W_("Payload command, that dont set any payload size.\n");
				}

				/* write an header respond */
				result->s = S_TRUE;
				result->payload = payload.s;

				/* send the result */
				if ((sent =
					 send(fd, result, sizeof(result_desc),
						  0)) != sizeof(result_desc))
				{
					F_("failed to send result, sent: %i of %i.\n", sent,
					   sizeof(result_desc));
					free(payload.p);
					break;
				}

				/* with out this, the client wont get the 2ond send. WHY? */
				usleep(1);

				/* send the payload */
				D_("Sending a payload of %i bytes.\n", result->payload);
				if ((sent =
					 send(fd, payload.p, result->payload,
						  0)) != (signed) result->payload)
				{
					F_("Could not send complete payload, sent %i of %i.",
					   sent, result->payload);
					free(payload.p);
					break;
				}

				/* cleanup and free */
				free(payload.p);
				break;
			}
		case COMMAND_FAIL:

			/* return FAIL header respond */
			result->s = S_INVALID_TYPE;
			if ((sent =
				 send(fd, result, sizeof(result_desc),
					  0)) != (signed) sizeof(result_desc))
			{
				F_("failed to send result, sent: %i of %i.\n", sent,
				   sizeof(result_desc));
				break;
			}
			/* TODO: really continue?? */

			D_("Invalid command type '%c', line '%s'\n", header.c, header.l);
			if (header_data)
				free(header_data);
			return;
	}
	D_("Returned successfully.\n");
	if (header_data)
		free(header_data);
	if (result)
		free(result);
	return;
}


/* called by fd hook, when data is no socket */
void accepted_client(f_module_h * from, e_fdw what)
{
	int newsock;

	/* make a dumb check */
	if (from != &fdh)
		return;

	D_("Got here from fd hook.\n");
	/* we try to fix socket after every service start
	   if it fails here chances are a user screwed it
	   up, and we shouldn't manually try to fix anything. */
	if (fdh.fds <= 0)
	{
		/* socket is invalid but we were called, call closesocket to make sure it wont happen again */
		F_("accepted client called with fdh.fds %d, report bug\nWill repopen socket.", fdh.fds);
		open_socket();
		return;
	}

	/* create a new socket, for reading */
	if ((newsock = accept(fdh.fds, NULL, NULL)) > 0)
	{
		/* read the data, by the handle_client function */
		handle_client(newsock);

		/* close the socket when finished */
		close(newsock);
		return;
	}

	/* temporary unavailable */
	if ((errno == EAGAIN) || (errno == EWOULDBLOCK))
	{
		W_("errno = EAGAIN!\n");
		return;
	}

	/* This'll generally happen on shutdown, don't cry about it. */
	D_("Error accepting socket %d, %s\n", fdh.fds, strerror(errno));
	closesock();
	return;
}

/* Send a ping to ourselves to check if we're 100% ok. */
static int sendping()
{
	int client;
	struct sockaddr_un sockname;
	read_header header;
	result_desc result;


	D_("Sending ping\n");

	memset(&header, 0, sizeof(read_header));
	header.p_ver = PROTOCOL_4_VERSION;

	/* Create the socket. */
	client = socket(PF_UNIX, SOCK_STREAM, 0);
	if (client < 0)
	{
		F_("Failed to init socket\n");
		return FALSE;
	}

	initng_fd_set_cloexec(client);

	/* Bind a name to the socket. */
	sockname.sun_family = AF_UNIX;

	strcpy(sockname.sun_path, socket_filename);

	/* Try to connect */
	if (connect
		(client, (struct sockaddr *) &sockname,
		 (strlen(sockname.sun_path) + sizeof(sockname.sun_family))) < 0)
	{
		close(client);
		return FALSE;
	}

	/* Write X to ping ourselves */

	header.c = 'X';
	header.l[0] = '\0';

	D_("Sending PING..\n");
	if (write(client, &header, sizeof(read_header)) <
		(signed) sizeof(read_header))
	{
		F_("Unable to send PING!\n");
		close(client);
		return (FALSE);
	}
	D_("PING sent..\n");

	/* Accept "server side" */
	accepted_client(&fdh, FDW_READ);

	D_("Reading PONG..\n");
	if ((read(client, &result, sizeof(result_desc)) <
		 (signed) sizeof(result_desc)) || result.c != 'Y'
		|| result.s != S_TRUE)
	{
		F_("Unable to receive PONG!\n");
		close(client);
		return (FALSE);
	}

	D_("Got pong\n");

	return TRUE;
}

/* this will try to open a new socket */
static int open_socket()
{
	/*    int flags; */
	struct sockaddr_un serv_sockname;

	D_("Creating " SOCKET_4_ROOTPATH " dir\n");

	closesock();

	/* Make /dev/initng if it doesn't exist (try either way) */
	if (mkdir(SOCKET_4_ROOTPATH, S_IRUSR | S_IWUSR | S_IXUSR) == -1
		&& errno != EEXIST)
	{
		if (errno != EROFS)
			F_("Could not create " SOCKET_4_ROOTPATH
			   " : %s, may be / fs not mounted read-write yet?, will retry until I succeed.\n",
			   strerror(errno));
		return (FALSE);
	}

	/* chmod root path for root use only */
	if (chmod(SOCKET_4_ROOTPATH, S_IRUSR | S_IWUSR | S_IXUSR) == -1)
	{
		/* path doesn't exist, we don't have /dev yet. */
		if (errno == ENOENT || errno == EROFS)
			return (FALSE);

		F_("CRITICAL, failed to chmod %s, THIS IS A SECURITY PROBLEM.\n",
		   SOCKET_4_ROOTPATH);
	}

	/* Create the socket. */
	fdh.fds = socket(PF_UNIX, SOCK_STREAM, 0);
	if (fdh.fds < 1)
	{
		F_("Failed to init socket (%s)\n", strerror(errno));
		fdh.fds = -1;
		return (FALSE);
	}

	initng_fd_set_cloexec(fdh.fds);

	/* Set socket to non blocking mode */
	/*    flags = fcntl(fdh.fds, F_GETFL);
	   if (flags < 0)
	   {
	   F_("Failed to fcntl fdh.fds\n");
	   closesock();
	   return (FALSE);
	   }
	   flags |= O_NONBLOCK;
	   if (fcntl(fdh.fds, F_SETFL, flags) < 0)
	   {
	   F_("Failed to set fdh.fds O_NONBLOCK\n");
	   closesock();
	   return (FALSE);
	   } */


	/* Bind a name to the socket. */
	serv_sockname.sun_family = AF_UNIX;

	strcpy(serv_sockname.sun_path, socket_filename);

	/* Remove old socket file if any */
	unlink(serv_sockname.sun_path);

	/* Try to bind */
	if (bind
		(fdh.fds, (struct sockaddr *) &serv_sockname,
		 (strlen(serv_sockname.sun_path) +
		  sizeof(serv_sockname.sun_family))) < 0)
	{
		F_("Error binding to socket (errno: %d str: '%s')\n", errno,
		   strerror(errno));
		closesock();
		unlink(serv_sockname.sun_path);
		return (FALSE);
	}

	/* chmod socket for root only use */
	if (chmod(serv_sockname.sun_path, S_IRUSR | S_IWUSR) == -1)
	{
		F_("CRITICAL, failed to chmod %s, THIS IS A SECURITY PROBLEM.\n",
		   serv_sockname.sun_path);
		closesock();
		return (FALSE);
	}

	/* store sock_stat for checking if we need to recreate socket later */
	stat(serv_sockname.sun_path, &sock_stat);

	/* Listen to socket */
	if (listen(fdh.fds, 5))
	{
		F_("Error on listen (errno: %d str: '%s')\n", errno, strerror(errno));
		closesock();
		unlink(serv_sockname.sun_path);
		return (FALSE);
	}



	/* Run check : */
	if (!sendping())
	{
		F_("Sendping check failed, ngc2 communication not available (if you see this open a bug)\n");
		closesock();
		return (FALSE);
	}

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

	D_("Checking socket\n");

	/* Check if socket needs reopening */
	if (fdh.fds <= 0)
	{
		D_("fdh.fds not set, opening new socket.\n");
		open_socket();
		return (TRUE);
	}

	/* stat the socket, reopen on failure */
	memset(&st, 0, sizeof(st));
	if (stat(socket_filename, &st) < 0)
	{
		W_("Stat failed! Opening new socket.\n");
		open_socket();
		return (TRUE);
	}

	/* compare socket file, with the one that we know, reopen on failure */
	if (st.st_dev != sock_stat.st_dev || st.st_ino != sock_stat.st_ino
		|| st.st_mtime != sock_stat.st_mtime)
	{
		F_("Invalid socket found, reopening\n");
		open_socket();
		return (TRUE);
	}

	D_("Socket ok.\n");
	return (TRUE);
}






static void cmd_help(char *arg, s_payload * payload)
{
	s_command *current = NULL;
	int i = 0;

	(void) arg;
	payload->p = (help_row *) i_calloc(101, sizeof(help_row));
	memset(payload->p, 0, sizeof(help_row) * 100);

	list_for_each_entry_prev(current, &local_commands_db.list, list)
	{
		help_row *row = payload->p + (i * sizeof(help_row));

		if (current->opt_vissible != STANDARD_COMMAND)
			continue;

		row->dt = HELP_ROW;
		row->c = current->command_id;
		row->t = current->com_type;
		row->o = current->opt_type;

		/* copy description */
		if (current->description)
			strncpy(row->d, current->description, 200);
		else
			row->d[0] = '\0';

		/* copy long id */
		if (current->long_id)
			strncpy(row->l, current->long_id, 100);
		else
			row->l[0] = '\0';

		i++;
		/*
		 * Until this is rewritten to count number of servics before allocating, or reallocating every time
		 * we have a static limit
		 */
		if (i > 100)
			break;
	}
	current = NULL;
	while_command_db(current)
	{
		help_row *row = payload->p + (i * sizeof(help_row));

		if (current->opt_vissible != STANDARD_COMMAND)
			continue;

		row->dt = HELP_ROW;
		row->c = current->command_id;
		row->t = current->com_type;
		row->o = current->opt_type;

		/* copy description */
		if (current->description)
			strncpy(row->d, current->description, 200);
		else
			row->d[0] = '\0';

		/* copy long id */
		if (current->long_id)
			strncpy(row->l, current->long_id, 100);
		else
			row->l[0] = '\0';

		i++;
		/*
		 * Until this is rewritten to count number of servics before allocating, or reallocating every time
		 * we have a static limit
		 */
		if (i > 100)
			break;
	}

	/* set up the payload info */
	payload->s = i * sizeof(help_row);
}

static void cmd_help_all(char *arg, s_payload * payload)
{
	int i = 0;
	s_command *current = NULL;

	(void) arg;


	/* allocate space for payload, static malloc for now */
	payload->p = (help_row *) i_calloc(101, sizeof(help_row));
	memset(payload->p, 0, sizeof(help_row) * 100);

	list_for_each_entry_prev(current, &local_commands_db.list, list)
	{
		help_row *row = payload->p + (i * sizeof(help_row));

		if (current->opt_vissible != STANDARD_COMMAND
			&& current->opt_vissible != ADVANCHED_COMMAND)
			continue;

		row->dt = HELP_ROW;
		row->c = current->command_id;
		row->t = current->com_type;
		row->o = current->opt_type;
		/* copy description */
		if (current->description)
			strncpy(row->d, current->description, 200);
		else
			row->d[0] = '\0';

		/* copy long id */
		if (current->long_id)
			strncpy(row->l, current->long_id, 100);
		else
			row->l[0] = '\0';
		i++;
	}

	current = NULL;
	while_command_db(current)
	{
		help_row *row = payload->p + (i * sizeof(help_row));

		if (current->opt_vissible != STANDARD_COMMAND
			&& current->opt_vissible != ADVANCHED_COMMAND)
			continue;

		row->dt = HELP_ROW;
		row->c = current->command_id;
		row->t = current->com_type;
		row->o = current->opt_type;
		/* copy description */
		if (current->description)
			strncpy(row->d, current->description, 200);
		else
			row->d[0] = '\0';

		/* copy long id */
		if (current->long_id)
			strncpy(row->l, current->long_id, 100);
		else
			row->l[0] = '\0';
		i++;
		/*
		 * Until this is rewritten to count number of servics before allocating, or reallocating every time
		 * we have a static limit
		 */
		if (i > 100)
			break;
	}

	/* set up the payload info */
	payload->s = i * sizeof(help_row);
}

static void cmd_start(char *arg, s_payload * payload)
{
	active_db_h *serv = NULL;

	payload->p = (active_row *) i_calloc(1, sizeof(active_row));
	payload->s = sizeof(active_row);
	memset(payload->p, 0, sizeof(active_row));
	active_row *row = payload->p;

	row->dt = ACTIVE_ROW;

	/* argument required */
	if (!arg || strlen(arg) < 1)
	{
		strcpy(row->state, "NOT_FOUND");
		row->is = IS_FAILED;
		return;
	}

	/* Find the service in the db */
	serv = initng_active_db_find_by_name(arg);
	if (serv)
	{
		memcpy(&row->time_set, &serv->time_current_state,
			   sizeof(struct timespec));
		strncpy(row->name, serv->name, 100);
		if (IS_UP(serv))
		{
			/* set status == ALREADY_RUNNING */
			strcpy(row->state, "ALREADY_RUNNING");
		}
		else
		{
			/* Try start this service */
			initng_handler_start_service(serv);
			/* set current status instead */
			strncpy(row->state, serv->current_state->state_name, 100);
		}

		/* the routh state */
		row->is = serv->current_state->is;

		/* Copy service type name */
		if (serv->type && serv->type->name)
			strncpy(row->type, serv->type->name, 100);
		return;
	}

	serv = initng_handler_start_new_service_named(arg);
	if (!serv)
	{
		strncpy(row->name, arg, 100);
		strcpy(row->state, "NOT_FOUND");
		row->is = IS_FAILED;
		return;
	}

	memcpy(&row->time_set, &serv->time_current_state, sizeof(struct timespec));
	strncpy(row->name, serv->name, 100);

	if (serv->current_state && serv->current_state->state_name)
	{
		row->is = serv->current_state->is;
		strncpy(row->state, serv->current_state->state_name, 100);
		/* Copy service type name */
		if (serv->type && serv->type->name)
			strncpy(row->type, serv->type->name, 100);
	}
}

static void cmd_stop(char *arg, s_payload * payload)
{
	active_db_h *serv = NULL;

	payload->p = (active_row *) i_calloc(1, sizeof(active_row));
	payload->s = sizeof(active_row);
	memset(payload->p, 0, sizeof(active_row));
	active_row *row = payload->p;

	row->dt = ACTIVE_ROW;

	/* argument required */
	if (!arg || strlen(arg) < 2)
	{
		strcpy(row->name, "UNSET");
		strcpy(row->state, "NOT_FOUND");
		row->is = IS_FAILED;
		return;
	}

	serv = initng_active_db_find_in_name(arg);
	if (!serv)
	{
		strncpy(row->name, arg, 100);
		strcpy(row->state, "NOT_FOUND");
		row->is = IS_FAILED;
		return;
	}

	initng_handler_stop_service(serv);

	row->time_set = serv->time_current_state;
	strncpy(row->name, serv->name, 100);

	if (serv->current_state && serv->current_state->state_name)
	{
		row->is = serv->current_state->is;

		/* Copy service type name */
		if (serv->type && serv->type->name)
			strncpy(row->type, serv->type->name, 100);

		/* Copy state name */
		strncpy(row->state, serv->current_state->state_name, 100);
	}
	else
	{
		row->is = 0;
		row->state[0] = '\0';
	}
}


static void cmd_options(char *arg, s_payload * payload)
{
	int i = 0;
	s_entry *current = NULL;

	/* if an argument is provided */
	if (arg && strlen(arg) > 1)
	{
		/* malloc some space for it */
		payload->p = (option_row *) i_calloc(1, sizeof(option_row));
		memset(payload->p, 0, sizeof(option_row));
		option_row *row = payload->p;

		row->dt = OPTION_ROW;
		strncpy(row->n, arg, 100);
		strcpy(row->d, "NOT_FOUND");
		row->t = 0;
		strcpy(row->o, "UNKNOWN");
		current = initng_service_data_type_find(arg);

		if (current)
		{
			if (current->opt_name)
				strncpy(row->n, current->opt_name, 100);
			if (current->opt_desc)
				strncpy(row->d, current->opt_desc, 300);

			row->t = current->opt_type;
			strncpy(row->o, current->ot->name, 100);
		}

		payload->s = sizeof(option_row);
		return;
	}


	/* malloc some space for it (static for now) */
	payload->p = (option_row *) i_calloc(101, sizeof(option_row));

	while_service_data_types(current)
	{
		option_row *row = payload->p + (i * sizeof(option_row));

		if (!current->opt_name)
			continue;

		/* if the names starts with "internal" its no value here */
		if (strncasecmp(current->opt_name, "internal", 8) == 0)
			continue;

		row->dt = OPTION_ROW;
		row->d[0] = '\0';

		strncpy(row->n, current->opt_name, 100);
		if (current->opt_desc)
			strncpy(row->d, current->opt_desc, 300);

		row->t = current->opt_type;
		if (current->ot)
			strncpy(row->o, current->ot->name, 100);
		else
			strcpy(row->o, "all");

		i++;
		/*
		 * Until this is rewritten to count number of servics before allocating, or reallocating every time
		 * we have a static limit
		 */
		if (i > 100)
			break;
	}

	/* Last, put s, to indicat that there is data */
	payload->s = i * sizeof(option_row);
}

static void cmd_states(char *arg, s_payload * payload)
{
	a_state_h *current = NULL;
	int no_states = 1;
	int i = 0;

	/* first do a round counting states */
	while_active_states(current)
	{
		no_states++;
	}

	/* allocate payload */
	payload->p = (state_row *) i_calloc(no_states, sizeof(state_row));
	if (!payload->p)
	{
		F_("Unable to allocate space for payload!\n");
		return;
	}

	/* then do a round and add them */
	current = NULL;
	while_active_states(current)
	{
		/* make sure we wont buffer overrun */
		if (i >= no_states)
		{
			F_("More state then we allocated for\n");
			return;
		}

		/* get on a entry */
		state_row *row = payload->p + (i * sizeof(state_row));

		/* set the datatype */
		row->dt = STATE_ROW;

		/* copy name */
		assert(current->state_name);
		strncpy(row->name, current->state_name, 100);

		/* copy description */
		if (current->state_desc)
			strncpy(row->desc, current->state_desc, 200);
		else
			row->desc[0] = '\0';

		/* set is */
		row->is = current->is;

		i++;
		/*
		 * Until this is rewritten to count number of servics before allocating, or reallocating every time
		 * we have a static limit
		 */
		if (i > 100)
			break;
	}

	/* tell how mutch data there is in this payload */
	payload->s = i * sizeof(state_row);
}

static void cmd_services(char *arg, s_payload * payload)
{
	int i = 0;
	active_db_h *current = NULL;


	/* if an argument is provided */
	if (arg && strlen(arg) > 1)
	{
		/* malloc some space for it */
		payload->p = (active_row *) i_calloc(1, sizeof(active_row));
		memset(payload->p, 0, sizeof(active_row));
		active_row *row = payload->p;

		row->dt = ACTIVE_ROW;
		/* fill with defaults, if no data is found */
		strncpy(row->name, arg, 100);
		strcpy(row->state, "NOT_FOUND");
		row->is = IS_FAILED;
		if ((current = initng_active_db_find_in_name(arg)))
		{
			if (current->current_state && current->current_state->state_name)
			{
				row->is = current->current_state->is;
				strncpy(row->state, current->current_state->state_name, 100);

				/* Copy service type name */
				if (current->type && current->type->name)
					strncpy(row->type, current->type->name, 100);

			}
			else
			{
				row->is = 0;
				row->state[0] = '\0';
			}
			row->time_set = current->time_current_state;
			strncpy(row->name, current->name, 100);
		}
		payload->s = sizeof(active_row);
		return;
	}

	/* else */

	/* malloc some space for it (static for now) */
	payload->p = (active_row *) i_calloc(101, sizeof(active_row));
	memset(payload->p, 0, sizeof(active_row) * 100);
	current = NULL;
	while_active_db(current)
	{
		active_row *row;

		/* dont display the hidden ones */
		if (current->type && current->type->hidden == TRUE)
			continue;

		/* Put the row marker on next free place in the allocated area */
		row = payload->p + (i * sizeof(active_row));

		row->dt = ACTIVE_ROW;

		if (current->current_state && current->current_state->state_name)
		{
			row->is = current->current_state->is;
			strncpy(row->state, current->current_state->state_name, 100);
			/* Copy service type name */
			if (current->type && current->type->name)
				strncpy(row->type, current->type->name, 100);
		}
		else
		{
			row->is = 0;
			row->state[0] = '\0';
		}
		row->time_set = current->time_current_state;
		strncpy(row->name, current->name, 100);
		i++;

		/*
		 * Until this is rewritten to count number of servics before allocating, or reallocating every time
		 * we have a static limit
		 */
		if (i > 100)
			break;
	}

	payload->s = i * sizeof(active_row);
}

static void cmd_all_services(char *arg, s_payload * payload)
{
	int i = 0;
	active_db_h *current = NULL;

	/* malloc some space for it (static for now) */
	payload->p = (active_row *) i_calloc(101, sizeof(active_row));
	memset(payload->p, 0, sizeof(active_row) * 100);
	current = NULL;
	while_active_db(current)
	{
		active_row *row;

		/* Put the row marker on next free place in the allocated area */
		row = payload->p + (i * sizeof(active_row));

		row->dt = ACTIVE_ROW;

		if (current->current_state && current->current_state->state_name)
		{
			row->is = current->current_state->is;
			strncpy(row->state, current->current_state->state_name, 100);
			/* Copy service type name */
			if (current->type && current->type->name)
				strncpy(row->type, current->type->name, 100);
		}
		else
		{
			row->is = 0;
			row->state[0] = '\0';
		}
		row->time_set = current->time_current_state;
		strncpy(row->name, current->name, 100);
		i++;

		/*
		 * Until this is rewritten to count number of servics before allocating, or reallocating every time
		 * we have a static limit
		 */
		if (i > 100)
			break;
	}

	payload->s = i * sizeof(active_row);
}



/*
 * These are local commands, not added to initng commands db,
 * other plugins wont be able to use the replys from these commands
 * anyway.
 */

s_command HELP = { 'h', "help", PAYLOAD_COMMAND, STANDARD_COMMAND, NO_OPT,
	{(void *) &cmd_help}, "Print what commands you can send to initng."
};

s_command HELP_ALL = { 'H', "help_all", PAYLOAD_COMMAND, STANDARD_COMMAND, NO_OPT,
	{(void *) &cmd_help_all}, "Print out verbose list of all commands."
};

s_command SERVICES = { 's', "status", PAYLOAD_COMMAND, STANDARD_COMMAND, USES_OPT,
	{(void *) &cmd_services}, "Print all services."
};

s_command ALL_SERVICES = { 'S', "allservice", PAYLOAD_COMMAND, HIDDEN_COMMAND, NO_OPT,
	{(void *) &cmd_all_services}, "Print all servics, even hidden ones."
};

s_command STATES = { 't', "states", PAYLOAD_COMMAND, STANDARD_COMMAND, USES_OPT,
	{(void *) &cmd_states}, "Print out all possible states."
};

s_command OPTIONS = { 'O', "options", PAYLOAD_COMMAND, ADVANCHED_COMMAND, USES_OPT,
	{(void *) &cmd_options}, "Print out option_db."
};

s_command START = { 'u', "start", PAYLOAD_COMMAND, STANDARD_COMMAND, REQUIRES_OPT,
	{(void *) &cmd_start}, "Start service."
};

s_command STOP = { 'd', "stop", PAYLOAD_COMMAND, STANDARD_COMMAND, REQUIRES_OPT,
	{(void *) &cmd_stop}, "Stop service."
};






int module_init(int api_version)
{
	D_("module_init(ngc2);\n");
	if (api_version != API_VERSION)
	{
		F_("This module is compiled for api_version %i version and initng is compiled on %i version, won't load this module!\n", API_VERSION, api_version);
		return (FALSE);
	}

	/* initziate the local commands db */
	INIT_LIST_HEAD(&local_commands_db.list);

	/* zero globals */
	fdh.fds = -1;
	memset(&sock_stat, 0, sizeof(sock_stat));


	/* decide which socket to use */
	if (g.i_am == I_AM_INIT)
		socket_filename = SOCKET_4_FILENAME_REAL;
	else if (g.i_am == I_AM_FAKE_INIT)
		socket_filename = SOCKET_4_FILENAME_TEST;
	else
		return (TRUE);

	D_("Socket is: %s\n", socket_filename);

	D_("adding hook, that will reopen socket, for every started service.\n");
	initng_event_hook_register(&EVENT_FD_WATCHER, &fdh_handler);
	initng_event_hook_register(&EVENT_SIGNAL, &check_socket);

	/* add the help command, that list commands to the client */
	list_add(&HELP.list, &local_commands_db.list);
	list_add(&HELP_ALL.list, &local_commands_db.list);
	list_add(&SERVICES.list, &local_commands_db.list);
	list_add(&ALL_SERVICES.list, &local_commands_db.list);
	list_add(&OPTIONS.list, &local_commands_db.list);
	list_add(&START.list, &local_commands_db.list);
	list_add(&STOP.list, &local_commands_db.list);
	list_add(&STATES.list, &local_commands_db.list);

	/* do the first socket directly */
	open_socket();

	D_("ngc2.so.0.0 loaded!\n");
	return (TRUE);
}


void module_unload(void)
{
	D_("module_unload(ngc2);\n");
	if (g.i_am != I_AM_INIT && g.i_am != I_AM_FAKE_INIT)
		return;

	/* close open sockets */
	closesock();

	/* remove hooks */
	initng_event_hook_unregister(&EVENT_FD_WATCHER, &fdh_handler);
	initng_event_hook_unregister(&EVENT_SIGNAL, &check_socket);

	D_("ngc2.so.0.0 unloaded!\n");

}
