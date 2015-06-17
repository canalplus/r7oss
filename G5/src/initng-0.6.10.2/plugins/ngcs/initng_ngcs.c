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

/* Experimental code for notifying external programs of status changes
   and the like. Perhaps one day I'll write a DBUS proxy for all this */

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

#include "initng_ngcs.h"

INITNG_PLUGIN_MACRO;

static void accepted_client(f_module_h * from, e_fdw what);
static void closesock(void);

/* static int sendping(void); */
static int open_socket(void);
static void check_socket(void);
static void data_ready(f_module_h * from, e_fdw what);
void register_ngcs_cmds(void);
void unregister_ngcs_cmds(void);
static void clean_dead_conns(void);
static void handle_chan0(ngcs_chan * chan, int type, int len, char *data);
static void handle_close(ngcs_conn * conn);
static void ngcs_cmd_compat(ngcs_request * req);
static void pollmode_hook(ngcs_conn * conn, int have_pending_writes);

const char *module_needs[] = {
	"stcmd",
	NULL
};

ngcs_cmd ngcs_compat_cmds = {
	NULL,
	ngcs_cmd_compat,
	{0, 0}
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
ngcs_svr_conn ngcs_conns;
ngcs_svr_conn ngcs_dead_conns;
ngcs_cmd ngcs_cmds;

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

static int conn_fdw_handler(s_event * event)
{
	s_event_fd_watcher_data * data;
	ngcs_svr_conn *current = NULL;

	assert(event);
	assert(event->data);

	data = event->data;

	list_for_each_entry(current, &ngcs_conns.list, list);
	{
		switch (data->action)
		{
			case FDW_ACTION_CLOSE:
				if (current->fdw.fds > 0)
					close(current->fdw.fds);
				break;

			case FDW_ACTION_CHECK:
				if (current->fdw.fds <= 2)
					break;

				/* This is a expensive test, but better safe then sorry */
				if (!STILL_OPEN(current->fdw.fds))
				{
					D_("%i is not open anymore.\n", current->fdw.fds);
					current->fdw.fds = -1;
					break;
				}

				FD_SET(current->fdw.fds, data->readset);
				data->added++;
				break;

			case FDW_ACTION_CALL:
				if (!data->added || current->fdw.fds <= 2)
					break;

				if(!FD_ISSET(current->fdw.fds, data->readset))
					break;

				current->fdw.call_module(&current->fdw, FDW_READ);
				data->added--;
				break;

			case FDW_ACTION_DEBUG:
				if (!data->debug_find_what || strstr(__FILE__, data->debug_find_what))
					mprintf(data->debug_out, " %i: Used by plugin: %s\n",
						current->fdw.fds, __FILE__);
				break;
		}
	}

	return (TRUE);
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

/* called by fd hook, when data is no socket */
void accepted_client(f_module_h * from, e_fdw what)
{
	int newsock;
	ngcs_svr_conn *conn;
	ngcs_chan *chan0;

	/* In case of lots of short-lived connections */
	clean_dead_conns();

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
		F_("accepted client called with fdh.fds %d, report bug\n", fdh.fds);
		closesock();
		F_("Attempting to reopen socket\n");
		open_socket();
		return;
	}

	/* create a new socket, for reading */
	if ((newsock = accept(fdh.fds, NULL, NULL)) > 0)
	{
		initng_fd_set_cloexec(newsock);

		D_("read socket open, now setting options\n");
		conn = (ngcs_svr_conn *) i_calloc(1, sizeof(ngcs_svr_conn));
		if (conn == NULL)
		{
			F_("Couldn't allocate ngcs_svr_conn!");
			close(newsock);
			return;
		}

		conn->conn = ngcs_conn_from_fd(newsock, conn, handle_close,
									   pollmode_hook);
		if (conn->conn == NULL)
		{
			F_("Couldn't allocate ngcs_conn!");
			close(newsock);
			free(conn);
			return;
		}

		chan0 = ngcs_chan_reg(conn->conn, 0, &handle_chan0, &ngcs_chan_del,
							  NULL);
		if (chan0 == NULL)
		{
			F_("Couldn't register channel 0!");
			ngcs_conn_free(conn->conn);
			free(conn);
			close(newsock);
			return;
		}

		conn->fdw.fds = conn->conn->fd;
		conn->fdw.what = FDW_READ;
		conn->fdw.call_module = data_ready;
		conn->nextid = 1;
		conn->list.next = 0;
		conn->list.prev = 0;
		list_add(&conn->list, &ngcs_conns.list);
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

static void handle_close(ngcs_conn * conn)
{
	ngcs_svr_conn *sconn = (ngcs_svr_conn *) conn->userdata;

	sconn->fdw.fds = -1;
	list_move(&sconn->list, &ngcs_dead_conns.list);
}

static void data_ready(f_module_h * from, e_fdw what)
{
	ngcs_svr_conn *conn = (ngcs_svr_conn *) from;

	if (what & FDW_READ)
		ngcs_conn_data_ready(conn->conn);
	if (what & FDW_WRITE)
		ngcs_conn_write_ready(conn->conn);
	clean_dead_conns();
}

static void pollmode_hook(ngcs_conn * conn, int have_pending_writes)
{
	ngcs_svr_conn *sconn = (ngcs_svr_conn *) conn->userdata;

	sconn->fdw.what = FDW_READ | (have_pending_writes ? FDW_WRITE : 0);
}

static void handle_chan0(ngcs_chan * chan, int type, int len, char *data)
{
	ngcs_request req;
	ngcs_cmd *cmd;

	/* sanity checks on message */
	if (len < 0)
	{
		return;
	}
	else if (type != NGCS_TYPE_STRUCT || len == 0)
	{
		F_("Invalid chan0 message with type=%i, len=%i\n", type, len);
		ngcs_chan_send(chan, NGCS_TYPE_ERROR, 11, "BAD_REQUEST");
		return;
	}

	/* unpack message */
	req.argv = NULL;
	req.argc = ngcs_unpack(data, len, &req.argv);
	if (req.argc < 0)
	{
		F_("Unpacking failed!\n");
		if (req.argc >= 0)
			free(req.argv);
		ngcs_chan_send(chan, NGCS_TYPE_ERROR, 11, "BAD_REQUEST");
		return;
	}


	if (req.argc <= 0 || req.argv[0].type != NGCS_TYPE_STRING)
	{
		F_("Bad unpacked message\n");
		if (req.argc >= 0)
			free(req.argv);
		ngcs_chan_send(chan, NGCS_TYPE_ERROR, 11, "BAD_REQUEST");
		return;
	}

	req.chan = chan;
	req.conn = chan->conn;					/* backwards compatibility */
	req.sent_resp_flag = 0;

	/* run the associated command */
	while_ngcs_cmds(cmd)
	{
		if (cmd->name != NULL && strcmp(cmd->name, req.argv[0].d.s) == 0)
		{
			cmd->func(&req);
			if (!req.sent_resp_flag)
			{
				/* if the command handler didn't send a response,
				   we send a default one to keep the protocol in sync */
				ngcs_chan_send(chan, NGCS_TYPE_NULL, 0, NULL);
			}
			ngcs_free_unpack(req.argc, req.argv);
			return;
		}
	}

	/* Command handlers which do multiple commands - TODO document this */
	while_ngcs_cmds(cmd)
	{
		if (cmd->name == NULL)
		{
			cmd->func(&req);
			if (req.sent_resp_flag)
			{
				ngcs_free_unpack(req.argc, req.argv);
				return;
			}
		}
	}

	F_("Unknown ngcs command: %s\n", req.argv[0].d.s);
	ngcs_chan_send(chan, NGCS_TYPE_ERROR, 17, "COMMAND_NOT_FOUND");
	ngcs_free_unpack(req.argc, req.argv);
	return;
}

static void ngcs_cmd_compat(ngcs_request * req)
{
	s_command *cur, *cmd = NULL;
	int ret;
	char *arg;
	char *sret;

	if (req->argv[0].type != NGCS_TYPE_STRING || req->argv[0].d.s[0] != '-')
		return;

	if (req->argv[0].d.s[1] == '-')
	{
		while_command_db(cur)
		{
			if (cur->long_id
				&& strcmp(cur->long_id, req->argv[0].d.s + 2) == 0)
			{
				cmd = cur;
				break;
			}
		}
	}
	else if (req->argv[0].d.s[1] != 0 && req->argv[0].d.s[2] == 0)
	{
		while_command_db(cur)
		{
			if (cur->command_id == req->argv[0].d.s[1])
			{
				cmd = cur;
				break;
			}
		}
	}

	if (cmd == NULL)
		return;

	if (req->argc > 2)
	{
		ngcs_send_response(req, NGCS_TYPE_ERROR, 13, "TOO_MANY_ARGS");
		return;
	}

	if (req->argc == 2 && req->argv[1].type != NGCS_TYPE_STRING)
	{
		ngcs_send_response(req, NGCS_TYPE_ERROR, 8, "BAD_ARGS");
		return;
	}

	if (cmd->opt_type == NO_OPT && req->argc > 1)
	{
		ngcs_send_response(req, NGCS_TYPE_ERROR, 13, "TOO_MANY_ARGS");
		return;
	}

	if (cmd->opt_type == REQUIRES_OPT && req->argc < 2)
	{
		ngcs_send_response(req, NGCS_TYPE_ERROR, 12, "TOO_FEW_ARGS");
		return;
	}

	arg = req->argc > 1 ? req->argv[1].d.s : NULL;

	switch (cmd->com_type)
	{
		case TRUE_OR_FALSE_COMMAND:
		case INT_COMMAND:
			assert(cmd->u.int_command_call);

			ret = cmd->u.int_command_call(arg);
			ngcs_send_response(req, (cmd->com_type == INT_COMMAND ?
									 NGCS_TYPE_INT : NGCS_TYPE_BOOL),
							   sizeof(int), (char *) &ret);
			break;

		case VOID_COMMAND:
			assert(cmd->u.void_command_call);
			cmd->u.void_command_call(arg);
			ngcs_send_response(req, NGCS_TYPE_NULL, 0, NULL);
			break;

		case STRING_COMMAND:
			assert(cmd->u.string_command_call);
			sret = cmd->u.string_command_call(arg);
			if (sret)
				ngcs_send_response(req, NGCS_TYPE_STRING, strlen(sret), sret);
			else
				ngcs_send_response(req, NGCS_TYPE_NULL, 0, NULL);
			break;

			/* case DATA_COMMAND: - TODO */

		default:
			F_("Unknown s_command command type\n");
			ngcs_send_response(req, NGCS_TYPE_ERROR, 12, "INTERNAL_ERROR");
			return;

	}

}

static void clean_dead_conns(void)
{
	ngcs_svr_conn *curr, *tmp;

	while_ngcs_dead_conns_safe(curr, tmp)
	{
		ngcs_conn_free(curr->conn);
		list_del(&curr->list);
		free(curr);
	}
}

ngcs_chan *ngcs_open_channel(ngcs_conn * conn,
							 void (*gotdata) (ngcs_chan *, int, int,
											  char *),
							 void (*chanfree) (ngcs_chan *))
{
	ngcs_svr_conn *sconn = (ngcs_svr_conn *) conn->userdata;

	/* FIXME - if nextid wraps around to 0, this breaks */
	return ngcs_chan_reg(conn, sconn->nextid++, gotdata, ngcs_chan_del,
						 chanfree);
}

void ngcs_close_channel(ngcs_chan * chan)
{
	ngcs_chan_send(chan, NGCS_TYPE_NULL, -1, NULL);
	ngcs_chan_del(chan);
}

int ngcs_send_response(ngcs_request * req, int type, int len,
					   const char *data)
{
	assert(req);
	if (req->sent_resp_flag)
		return 1;
	req->sent_resp_flag = 1;
	return ngcs_chan_send(req->chan, type, len, data);
}

void ngcs_reg_cmd(ngcs_cmd * cmd)
{
	cmd->list.prev = 0;
	cmd->list.next = 0;
	list_add(&cmd->list, &ngcs_cmds.list);
}

void ngcs_unreg_cmd(ngcs_cmd * cmd)
{
	assert(cmd);
	assert(cmd->list.prev);
	assert(cmd->list.next);
	list_del(&cmd->list);
}

#if 0
/* Send a ping to ourselves to check if we're 100% ok. */
static int sendping()
{
	int client;
	struct sockaddr_un sockname;
	data_header head;

	D_("Sending ping\n");


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

	D_("Sending PING..\n");
	if (ngcs_sendmsg(NGCS_PING, 0, 123, NULL, 0)) ;
	{
		F_("Unable to send PING!\n");
		close(client);
		return (FALSE);
	}
	D_("PING sent..\n");

	/* Accept "server side" */
	accepted_client(&fdh);
	/* FIXME: won't work anymore */

	D_("Reading PONG..\n");
	if (ngcs_recvall(client, &head, sizeof(head)))
	{
		F_("Unable to receive PONG!\n");
		close(client);
		return (FALSE);
	}

	D_("Got pong\n");

	return TRUE;
}
#endif

/* this will try to open a new socket */
static int open_socket()
{
	/*    int flags; */
	struct sockaddr_un serv_sockname;

	D_("Creating " SOCKET_ROOTPATH " dir\n");

	closesock();

	/* Make /dev/initng if it doesn't exist (try either way) */
	if (mkdir(SOCKET_ROOTPATH, S_IRUSR | S_IWUSR | S_IXUSR) == -1
		&& errno != EEXIST)
	{
		if (errno != EROFS)
			F_("Could not create " SOCKET_ROOTPATH
			   " : %s, may be / fs not mounted read-write yet?, will retry until I succeed.\n",
			   strerror(errno));
		return (FALSE);
	}

	/* chmod root path for root use only */
	if (chmod(SOCKET_ROOTPATH, S_IRUSR | S_IWUSR | S_IXUSR) == -1)
	{
		/* path doesn't exist, we don't have /dev yet. */
		if (errno == ENOENT || errno == EROFS)
			return (FALSE);

		F_("CRITICAL, failed to chmod %s, THIS IS A SECURITY PROBLEM.\n",
		   SOCKET_ROOTPATH);
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
	/*    if (!sendping())
	   {
	   F_("Sendping check failed, ngcs communication not available (if you see this open a bug)\n");
	   closesock();
	   return (FALSE);
	   } */

	return (TRUE);
}

/* this will check socket, and reopen on failure */
static void check_socket()
{
	struct stat st;

	D_("Checking socket\n");

	/* Check if socket needs reopening */
	if (fdh.fds <= 0)
	{
		D_("fdh.fds not set, opening new socket.\n");
		open_socket();
		return;
	}

	/* stat the socket, reopen on failure */
	memset(&st, 0, sizeof(st));
	if (stat(socket_filename, &st) < 0)
	{
		W_("Stat failed! Opening new socket.\n");
		open_socket();
		return;
	}

	/* compare socket file, with the one that we know, reopen on failure */
	if (st.st_dev != sock_stat.st_dev || st.st_ino != sock_stat.st_ino
		|| st.st_mtime != sock_stat.st_mtime)
	{
		F_("Invalid socket found, reopening\n");
		open_socket();
		return;
	}

	D_("Socket ok.\n");
	return;
}

/* this function, will make a check for socket, on every new service that goes up */
int service_status(s_event * event)
{
	active_db_h * service;

	assert(event->event_type == &EVENT_IS_CHANGE);
	assert(event->data);

	service = event->data;

	/* only try open, when a service got up */
	if (IS_UP(service))
		check_socket();

	return (TRUE);
}

int module_init(int api_version)
{
	D_("module_init(ngcs);\n");

	if (api_version != API_VERSION)
	{
		F_("This module is compiled for api_version %i version and initng is compiled on %i version, won't load this module!\n", API_VERSION, api_version);
		return (FALSE);
	}

	/* zero globals */
	fdh.fds = -1;
	memset(&sock_stat, 0, sizeof(sock_stat));
	INIT_LIST_HEAD(&ngcs_conns.list);
	INIT_LIST_HEAD(&ngcs_dead_conns.list);
	INIT_LIST_HEAD(&ngcs_cmds.list);

	/* decide which socket to use */
	if (g.i_am == I_AM_INIT)
		socket_filename = SOCKET_FILENAME_REAL;
	else if (g.i_am == I_AM_FAKE_INIT)
		socket_filename = SOCKET_FILENAME_TEST;
	else
		return (TRUE);
	D_("Socket is: %s\n", socket_filename);

	D_("adding hook, that will reopen socket, for every started service.\n");
	initng_event_hook_register(&EVENT_IS_CHANGE, &service_status);
	initng_event_hook_register(&EVENT_FD_WATCHER, &fdh_handler);
	initng_event_hook_register(&EVENT_FD_WATCHER, &conn_fdw_handler);

	ngcs_reg_cmd(&ngcs_compat_cmds);

	register_ngcs_cmds();

	/* do the first socket directly */
	open_socket();

	D_("ngcs.so.0.0 loaded!\n");
	return (TRUE);
}

void module_unload(void)
{
	ngcs_svr_conn *curr, *tmp;

	D_("module_unload(ngcs);\n");

	if (g.i_am != I_AM_INIT && g.i_am != I_AM_FAKE_INIT)
		return;

	/* close open sockets */
	closesock();
	while_ngcs_conns_safe(curr, tmp)
	{
		ngcs_conn_free(curr->conn);
	}
	clean_dead_conns();

	ngcs_unreg_cmd(&ngcs_compat_cmds);

	unregister_ngcs_cmds();

	/* remove hooks */
	initng_event_hook_unregister(&EVENT_FD_WATCHER, &conn_fdw_handler);
	initng_event_hook_unregister(&EVENT_FD_WATCHER, &fdh_handler);
	initng_event_hook_unregister(&EVENT_IS_CHANGE, &service_status);

	D_("ngcs.so.0.0 unloaded!\n");
}
