/*
 * Initng, a next generation sysvinit replacement.
 * Copyright (C) 2005-6 Aidan Thornton <makomk@lycos.co.uk>
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

#include <stdlib.h>
#include "ngcs_client.h"
#include "ngcs_paths.h"
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <assert.h>

struct ngcs_svc_evt_hook_s
{
	void *userdata;
	ngcs_chan *chan;
	void (*hook) (ngcs_svc_evt_hook * hook, void *userdata,
				  ngcs_svc_evt * event);
};

typedef struct ngcs_cli_req_s
{
	void *cookie;
	void (*handler) (ngcs_cli_conn * /* conn */ , void * /* cookie */ ,
					 ngcs_data * /* ret */ );
	ngcs_data resp;
	struct list_head list;
} ngcs_cli_req;

struct ngcs_cli_conn_s
{
	ngcs_conn *conn;
	ngcs_chan *chan0;
	ngcs_cli_req pending_reqs;
	ngcs_cli_req resps;
	void (*onclose) (ngcs_cli_conn * conn);
	void (*pollmode_hook) (ngcs_cli_conn * cconn, int have_pending_writes);
	void *userdata;
};

static void ngcs_client_close_hook(ngcs_conn * conn);
static void pollmode_hook_fixup(ngcs_conn * conn, int have_pending_writes);
static void chan0_gotdata(ngcs_chan * chan, int type, int len, char *data);
static void chan0_close(ngcs_chan * chan);
static void handle_svc_event_req(ngcs_cli_conn * cconn, void *userdata,
								 ngcs_data * ret);
static void svc_status_gotdata(ngcs_chan * chan, int type, int len,
							   char *data);
static void svc_status_end(ngcs_chan * chan);

void ngcs_client_free_svc_watch(ngcs_svc_evt_hook * hook)
{
	ngcs_svc_evt event;

	assert(hook);
	if (hook->chan != NULL)
	{
		ngcs_chan_del(hook->chan);
	}
	else
	{
		event.type = NGCS_SVC_EVT_END;
		event.svc_name = NULL;
		hook->hook(hook, hook->userdata, &event);
		free(hook);
	}
}

static void handle_svc_event_req(ngcs_cli_conn * cconn, void *userdata,
								 ngcs_data * ret)
{
	ngcs_svc_evt_hook *hook = userdata;
	ngcs_svc_evt event;

	assert(hook != NULL);

	if (ret == NULL || ret->len < 0 || ret->type != NGCS_TYPE_INT)
	{
		/* FIXME - better error handling? */
		ngcs_client_free_svc_watch(hook);
		return;
	}

	hook->chan = ngcs_chan_reg(cconn->conn, ret->d.i, svc_status_gotdata,
							   ngcs_chan_del, svc_status_end);
	if (hook->chan == NULL)
	{
		ngcs_client_free_svc_watch(hook);
		return;
	}
	hook->chan->userdata = hook;

	event.type = NGCS_SVC_EVT_BEGIN;
	event.svc_name = NULL;
	hook->hook(hook, hook->userdata, &event);
}

static void svc_status_gotdata(ngcs_chan * chan, int type, int len,
							   char *data)
{
	ngcs_svc_evt_hook *hook = chan->userdata;
	ngcs_svc_evt event;
	ngcs_data *s;
	int cnt;

	assert(hook != NULL);

	if (len < 0)
		return;

	if (type == NGCS_TYPE_NULL)
	{
		event.type = NGCS_SVC_EVT_NOW;
		event.svc_name = NULL;
		hook->hook(hook, hook->userdata, &event);
		return;
	}

	if (type != NGCS_TYPE_STRUCT)
		return;								/* FIXME - report error */

	cnt = ngcs_unpack(data, len, &s);
	if (cnt < 0)
		return;

	if (cnt == 2 && s[0].type == NGCS_TYPE_STRING
		&& s[1].type == NGCS_TYPE_STRING)
	{
		event.type = NGCS_SVC_EVT_OUTPUT;
		event.svc_name = s[0].d.s;
		event.r.output = s[1].d.s;
		hook->hook(hook, hook->userdata, &event);
		ngcs_free_unpack(cnt, s);
	}
	else
	{
		ngcs_free_unpack(cnt, s);

		if (ngcs_unmarshal_ngcs_active_db_h(&event.r.adb, len, data))
		{
			printf("DEBUG: unmarshal active_db error\n");
			return;							/* FIXME - should report error too */
		}

		event.type = NGCS_SVC_EVT_STATE;
		event.svc_name = event.r.adb.name;

		hook->hook(hook, hook->userdata, &event);

		ngcs_free_ngcs_active_db_h(&event.r.adb);


		/* TODO */
	}
}

static void svc_status_end(ngcs_chan * chan)
{
	ngcs_svc_evt_hook *hook = chan->userdata;

	assert(hook != NULL);

	hook->chan = NULL;
	ngcs_client_free_svc_watch(hook);
}


ngcs_cli_conn *ngcs_client_connect(e_ngcs_cli_connflags flags, void *userdata,
								   void (*pollmode_hook) (ngcs_cli_conn *
														  cconn,
														  int
														  have_pending_writes),
								   void (*onclose) (ngcs_cli_conn * conn))
{
	int len;
	int sock;
	ngcs_cli_conn *cconn;
	struct sockaddr_un sockname;


	sock = socket(PF_UNIX, SOCK_STREAM, 0);
	if (sock < 0)
		return NULL;

	sockname.sun_family = AF_UNIX;
	strcpy(sockname.sun_path, (flags & NGCS_CLIENT_CONN_TEST ?
							   SOCKET_FILENAME_TEST : SOCKET_FILENAME_REAL));

	len = strlen(sockname.sun_path) + sizeof(sockname.sun_family);

	if (connect(sock, (struct sockaddr *) &sockname, len))
	{
		close(sock);
		return NULL;
	}

	cconn = malloc(sizeof(ngcs_cli_conn));
	if (cconn == NULL)
	{
		close(sock);
		return NULL;
	}

	cconn->conn = ngcs_conn_from_fd(sock, cconn, ngcs_client_close_hook,
									pollmode_hook_fixup);

	if (cconn->conn == NULL)
	{
		free(cconn);
		close(sock);
		return NULL;
	}

	INIT_LIST_HEAD(&cconn->pending_reqs.list);
	INIT_LIST_HEAD(&cconn->resps.list);
	cconn->onclose = NULL;
	cconn->pollmode_hook = NULL;
	cconn->userdata = userdata;

	/* Command channel; we don't need notification of it being freed */
	cconn->chan0 = ngcs_chan_reg(cconn->conn, 0, chan0_gotdata,
								 chan0_close, NULL);

	if (cconn->chan0 == NULL)
	{
		ngcs_conn_close(cconn->conn);
		return NULL;
	}

	cconn->onclose = onclose;
	cconn->pollmode_hook = NULL;

	return cconn;
}

void ngcs_client_free(ngcs_cli_conn * cconn)
{
	if (cconn->conn != NULL)
	{
		ngcs_conn_free(cconn->conn);
	}

	/* TODO - ? */

	free(cconn);
}

int ngcs_client_conn_is_closed(ngcs_cli_conn * cconn)
{
	return cconn->conn == NULL;
}

int ngcs_client_conn_has_pending_writes(ngcs_cli_conn * cconn)
{
	return cconn->conn != NULL && ngcs_conn_has_pending_writes(cconn->conn);
}

static void pollmode_hook_fixup(ngcs_conn * conn, int have_pending_writes)
{
	ngcs_cli_conn *cconn = conn->userdata;

	if (cconn->pollmode_hook)
		cconn->pollmode_hook(cconn, have_pending_writes);
}


void *ngcs_client_conn_get_userdata(ngcs_cli_conn * cconn)
{
	return cconn->userdata;
}

void ngcs_client_conn_set_userdata(ngcs_cli_conn * cconn, void *userdata)
{
	cconn->userdata = userdata;
}

static void ngcs_client_close_hook(ngcs_conn * conn)
{
	ngcs_cli_conn *cconn = conn->userdata;

	if (cconn->onclose != NULL)
		cconn->onclose(cconn);

	cconn->conn = NULL;

	/* TODO */
}

static void chan0_gotdata(ngcs_chan * chan, int type, int len, char *data)
{
	ngcs_cli_conn *cconn = chan->conn->userdata;
	ngcs_cli_req *req;

	if (list_empty(&cconn->pending_reqs.list))
	{
		/* FIXME - should display some sort of error */
		ngcs_chan_close(chan);
		return;
	}

	req = list_entry(cconn->pending_reqs.list.next, ngcs_cli_req, list);

	if (ngcs_unpack_one(type, len, data, &req->resp))
	{
		/* FIXME - should also display an error */
		req->resp.type = NGCS_TYPE_NULL;
		req->resp.len = -1;
		req->resp.d.p = NULL;
	}

	list_move_tail(&req->list, &cconn->resps.list);
}

/* Cancels all requests awaiting a response */
static void chan0_close(ngcs_chan * chan)
{
	ngcs_cli_conn *cconn = chan->conn->userdata;
	ngcs_cli_req *req, *next;

	list_for_each_entry_safe(req, next, &cconn->pending_reqs.list, list)
	{
		req->resp.type = NGCS_TYPE_NULL;
		req->resp.len = -1;
		req->resp.d.p = NULL;
		list_move_tail(&req->list, &cconn->resps.list);
	}

	cconn->chan0 = NULL;
}

void ngcs_client_dispatch(ngcs_cli_conn * cconn)
{
	ngcs_cli_req *req, *next;

	if (cconn->conn != NULL)
		ngcs_conn_dispatch(cconn->conn);

	list_for_each_entry_safe(req, next, &cconn->resps.list, list)
	{
		if (req->handler)
			req->handler(cconn, req->cookie, &req->resp);

		list_del(&req->list);
		ngcs_free_unpack_one(&req->resp);
		free(req);
	}
}

int ngcs_client_get_fd(ngcs_cli_conn * cconn)
{
	assert(cconn->conn != NULL);
	return cconn->conn->fd;
}

void ngcs_client_data_ready(ngcs_cli_conn * cconn)
{
	ngcs_client_dispatch(cconn);
	if (cconn->conn != NULL)
		ngcs_conn_data_ready(cconn->conn);
	ngcs_client_dispatch(cconn);
}

void ngcs_client_write_ready(ngcs_cli_conn * cconn)
{
	if (cconn->conn != NULL)
		ngcs_conn_write_ready(cconn->conn);
}

int ngcs_cmd_async(ngcs_cli_conn * cconn, int argc, ngcs_data * argv,
				   void (*handler) (ngcs_cli_conn * cconn, void *userdata,
									ngcs_data * ret), void *userdata)
{
	int len, ret;
	char *buf;
	ngcs_cli_req *req;

	assert(cconn);
	if (cconn->chan0 == NULL)
		return 1;

	len = ngcs_pack(argv, argc, NULL);
	if (len < 0)
		return -1;

	buf = malloc(len);
	if (buf == NULL)
		return 1;

	len = ngcs_pack(argv, argc, buf);
	if (len < 0)
	{
		free(buf);
		return 1;
	}

	req = malloc(sizeof(ngcs_cli_req));
	if (req == NULL)
	{
		free(buf);
		return 1;
	}

	ret = ngcs_chan_send(cconn->chan0, NGCS_TYPE_STRUCT, len, buf);
	free(buf);
	if (ret)
	{
		free(req);
		return 1;
	}

	req->cookie = userdata;
	req->handler = handler;
	req->list.prev = 0;
	req->list.next = 0;

	list_add_tail(&req->list, &cconn->pending_reqs.list);
	return 0;
}

ngcs_svc_evt_hook *ngcs_svc_event_cmd(ngcs_cli_conn * cconn,
									  int argc, ngcs_data * argv,
									  void (*handler) (ngcs_svc_evt_hook *
													   hook, void *userdata,
													   ngcs_svc_evt * event),
									  void *userdata)
{
	ngcs_svc_evt_hook *hook = malloc(sizeof(ngcs_svc_evt_hook));

	assert(cconn);
	assert(handler);

	if (hook == NULL)
		return NULL;

	hook->chan = NULL;
	hook->hook = handler;
	hook->userdata = userdata;

	if (ngcs_cmd_async(cconn, argc, argv, handle_svc_event_req, hook))
	{
		free(hook);
		return NULL;
	}

	return hook;
}

ngcs_svc_evt_hook *ngcs_watch_service(ngcs_cli_conn * cconn, char *svc,
									  int flags,
									  void (*handler) (ngcs_svc_evt_hook
													   * hook, void *userdata,
													   ngcs_svc_evt * event),
									  void *userdata)
{
	ngcs_data args[3];

	args[0].type = NGCS_TYPE_STRING;
	args[0].len = -1;
	args[0].d.s = (char *) "watch";
	args[1].type = NGCS_TYPE_INT;
	args[1].d.i = flags;

	if (svc == NULL)
	{
		args[2].type = NGCS_TYPE_NULL;
		args[2].len = 0;
	}
	else
	{
		args[2].type = NGCS_TYPE_STRING;
		args[2].len = -1;
		args[2].d.s = svc;
	}

	return ngcs_svc_event_cmd(cconn, 3, args, handler, userdata);
}

ngcs_svc_evt_hook *ngcs_start_stop(ngcs_cli_conn * cconn, const char *cmd,
								   const char *svc,
								   void (*handler) (ngcs_svc_evt_hook * hook,
													void *userdata,
													ngcs_svc_evt * event),
								   void *userdata)
{
	ngcs_data args[3];

	args[0].type = NGCS_TYPE_STRING;
	args[0].len = -1;
	args[0].d.s = (char *) cmd;

	args[1].type = NGCS_TYPE_STRING;
	args[1].len = -1;
	args[1].d.s = (char *) svc;

	return ngcs_svc_event_cmd(cconn, 2, args, handler, userdata);
}


#if 0
int ngcs_cmd(ngcs_conn * conn, int argc, ngcs_data * argv, ngcs_data * ret)
{
	ngcs_cli_conn *cconn = (ngcs_cli_conn *) conn->userdata;
	int len, ret;
	char *buf;

	len = ngcs_pack(argv, argc, NULL);
	if (len < 0)
		return 1;
	buf = malloc(len);
	if (buf == NULL)
		return 1;

	len = ngcs_pack(argv, argc, buf);
	if (len < 0)
	{
		free(buf);
		return 1;
	}

	ret = ngcs_chan_send(cconn->chan0, NGCS_TYPE_STRUCT, len, buf);
	free(buf);
	buf = NULL;
	if (ret)
		return 1;

	return 1;								/* TODO */
}
#endif

/* TODO */
