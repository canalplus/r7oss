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

#ifndef NGCS_CLIENT_H
#define NGCS_CLIENT_H
#include "ngcs_common.h"
#include "ngcs_marshal.h"

typedef enum
{
	NGCS_CLIENT_CONN_TEST = 1
} e_ngcs_cli_connflags;

typedef struct ngcs_svc_evt_hook_s ngcs_svc_evt_hook;
typedef struct ngcs_cli_conn_s ngcs_cli_conn;

typedef enum
{
	NGCS_SVC_EVT_BEGIN = 0,		/* start getting service events */
	NGCS_SVC_EVT_STATE = 1,					/* state change */
	NGCS_SVC_EVT_OUTPUT = 2,				/* service output */
	NGCS_SVC_EVT_NOW = 3,					/* end of current state information, start
											   of state change information */
	NGCS_SVC_EVT_END = 4					/* end of service events */
} e_ngcs_svc_evt_type;

typedef struct ngcs_svc_evt_s
{
	e_ngcs_svc_evt_type type;
	char *svc_name;
	union
	{
		char *output;
		ngcs_active_db_h adb;
	} r;
} ngcs_svc_evt;

ngcs_cli_conn *ngcs_client_connect(e_ngcs_cli_connflags flags, void *userdata,
								   void (*pollmode_hook) (ngcs_cli_conn *
														  conn,
														  int
														  have_pending_writes),
								   void (*onclose) (ngcs_cli_conn * conn));

void ngcs_client_free(ngcs_cli_conn * cconn);

void ngcs_client_dispatch(ngcs_cli_conn * cconn);

void ngcs_client_data_ready(ngcs_cli_conn * cconn);

void ngcs_client_write_ready(ngcs_cli_conn * cconn);

int ngcs_client_conn_has_pending_writes(ngcs_cli_conn * cconn);

int ngcs_client_get_fd(ngcs_cli_conn * cconn);

int ngcs_cmd_async(ngcs_cli_conn * cconn, int argc, ngcs_data * argv,
				   void (*handler) (ngcs_cli_conn * cconn, void *userdata,
									ngcs_data * ret), void *userdata);

void *ngcs_client_conn_get_userdata(ngcs_cli_conn * cconn);

void ngcs_client_conn_set_userdata(ngcs_cli_conn * cconn, void *userdata);

int ngcs_client_conn_is_closed(ngcs_cli_conn * cconn);

void ngcs_client_free_svc_watch(ngcs_svc_evt_hook * hook);

ngcs_svc_evt_hook *ngcs_svc_event_cmd(ngcs_cli_conn * cconn,
									  int argc, ngcs_data * argv,
									  void (*handler) (ngcs_svc_evt_hook *
													   hook, void *userdata,
													   ngcs_svc_evt * event),
									  void *userdata);

ngcs_svc_evt_hook *ngcs_watch_service(ngcs_cli_conn * cconn, char *svc,
									  int flags,
									  void (*handler) (ngcs_svc_evt_hook *
													   hook, void *userdata,
													   ngcs_svc_evt * event),
									  void *userdata);

ngcs_svc_evt_hook *ngcs_start_stop(ngcs_cli_conn * cconn, const char *cmd,
								   const char *svc,
								   void (*handler) (ngcs_svc_evt_hook * hook,
													void *userdata,
													ngcs_svc_evt * event),
								   void *userdata);

#if 0
/* Note that these should only be used on connections returned 
   by ... */
int ngcs_cmd(ngcs_conn * conn, int argc, ngcs_data * argv, ngcs_data * ret);
int ngcs_short_cmd(ngcs_conn * conn, char cmd, char *arg, ngcs_data * ret);
int ngcs_long_cmd(ngcs_conn * conn, char *cmd, char *arg, ngcs_data * ret);
#endif

#endif
