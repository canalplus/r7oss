/*
 * Initng, a next generation sysvinit replacement.
 * Copyright (C) 2006 Jimmy Wennlund <jimmy.wennlund@gmail.com>
 * Copyright (C) 2005-6 Aidan Thornton <makomk@lycos.co.uk>
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

#include <initng-paths.h>
#include "initng_ngcs.h"
#include "ngcs_marshal.h"
#include "initng_ngcs_cmds.h"

typedef struct ngcs_watch_s
{
	ngcs_chan *chan;
	char *name;
	int flags;
	struct list_head list;
} ngcs_watch;

typedef struct ngcs_genwatch_s
{
	ngcs_chan *chan;
	struct list_head list;
} ngcs_genwatch;

void register_ngcs_cmds(void);
void unregister_ngcs_cmds(void);
static int service_status_watch(s_event * event);
static void ngcs_cmd_watch(ngcs_request * req);
static void ngcs_free_watch(ngcs_chan * chan);
static int service_output_watch(s_event * event);
static ngcs_watch *ngcs_add_watch(ngcs_conn * conn, char *svcname, int flags);
static void ngcs_cmd_start(ngcs_request * req);
static int ngcs_watch_initial(ngcs_watch * watch);
static void ngcs_cmd_stop(ngcs_request * req);
static void ngcs_cmd_hot_reload(ngcs_request * req);
static void ngcs_cmd_zap(ngcs_request * req);
static int system_state_watch(s_event * event);
static void ngcs_free_genwatch(ngcs_chan * chan);
static void ngcs_cmd_swatch(ngcs_request * req);
static void ngcs_cmd_ewatch(ngcs_request * req);
static int error_watch(s_event * event);

ngcs_cmd ngcs_start_cmd = {
	"start",
	ngcs_cmd_start,
	{0, 0}
};


ngcs_cmd ngcs_stop_cmd = {
	"stop",
	ngcs_cmd_stop,
	{0, 0}
};

ngcs_cmd ngcs_watch_cmd = {
	"watch",
	ngcs_cmd_watch,
	{0, 0}
};

ngcs_cmd ngcs_swatch_cmd = {
	"swatch",
	ngcs_cmd_swatch,
	{0, 0}
};

ngcs_cmd ngcs_ewatch_cmd = {
	"ewatch",
	ngcs_cmd_ewatch,
	{0, 0}
};

/* TODO - remove this. It's obsolete */
ngcs_cmd ngcs_hot_reload_cmd = {
	"hot_reload",
	ngcs_cmd_hot_reload,
	{0, 0}
};

ngcs_cmd ngcs_hot_reload_cmd_short = {
	"-c",
	ngcs_cmd_hot_reload,
	{0, 0}
};

ngcs_cmd ngcs_hot_reload_cmd_long = {
	"--hot_reload",
	ngcs_cmd_hot_reload,
	{0, 0}
};

ngcs_cmd ngcs_zap_cmd = {
	"zap",
	ngcs_cmd_zap,
	{0, 0}
};

ngcs_watch watches;
ngcs_genwatch swatches;
ngcs_genwatch ewatches;

static int system_state_watch(s_event * event)
{
	ngcs_genwatch *watch, *nextwatch;

	assert(event->event_type == &EVENT_SYSTEM_CHANGE);
	assert(event->data);

	list_for_each_entry_prev_safe(watch, nextwatch, &swatches.list, list)
	{
		ngcs_chan_send(watch->chan, NGCS_TYPE_INT, sizeof(int), (char *) event->data);
	}

	return (TRUE);
}

static int error_watch(s_event * event)
{
	s_event_error_message_data * data;
	ngcs_genwatch *watch, *nextwatch;
	int len, size;
	ngcs_data dat[5];
	char *buf;
	va_list va;

	assert(event->event_type == &EVENT_ERROR_MESSAGE);
	assert(event->data);

	data = event->data;
	va_copy(va, data->arg);

	/* Don't do the processing if we're just going to throw away the result */
	if (list_empty(&ewatches.list))
		return FALSE;

	dat[0].type = NGCS_TYPE_INT;
	dat[0].d.i = data->mt;
	dat[1].type = NGCS_TYPE_STRING;
	dat[1].len = -1;
	dat[1].d.s = (char *) data->file;
	dat[2].type = NGCS_TYPE_STRING;
	dat[2].len = -1;
	dat[2].d.s = (char *) data->func;
	dat[3].type = NGCS_TYPE_INT;
	dat[3].d.i = data->line;

	dat[4].type = NGCS_TYPE_STRING;

	size = 256;
	dat[4].d.s = i_calloc(1, size);
	len = vsnprintf(dat[4].d.s, size, data->format, va);
	va_end(va);
	while (len < 0 || len >= size)
	{
		/* Some glibc versions apparently return -1 if buffer too small */
		size = (len < 0 ? size * 2 : len + 1);
		free(dat[4].d.s);
		dat[4].d.s = i_calloc(1, size);
		va_copy(va, data->arg);
		len = vsnprintf(dat[4].d.s, size, data->format, va);
		va_end(va);
	}
	dat[4].len = len;

	len = ngcs_pack(dat, 5, NULL);
	assert(len >= 0);
	buf = i_calloc(1, len);
	assert(buf);							/* should always succeed */
	len = ngcs_pack(dat, 5, buf);
	assert(len >= 0);

	list_for_each_entry_prev_safe(watch, nextwatch, &ewatches.list, list)
	{
		ngcs_chan_send(watch->chan, NGCS_TYPE_STRUCT, len, buf);
	}

	free(dat[4].d.s);
	free(buf);
	return FALSE;
}

static int service_status_watch(s_event * event)
{
	active_db_h * service;
	ngcs_watch *watch, *nextwatch;
	int len = 0;
	char *buf = NULL;

	assert(event->event_type == &EVENT_STATE_CHANGE);
	assert(event->data);

	service = event->data;

	len = ngcs_marshal_active_db_h(service, NULL);
	list_for_each_entry_prev_safe(watch, nextwatch, &watches.list, list)
	{
		if ((watch->flags & NGCS_WATCH_STATUS) &&
			(watch->name == NULL || strcmp(watch->name, service->name) == 0))
		{
			if (!buf)
			{
				buf = i_calloc(1, len);
				len = ngcs_marshal_active_db_h(service, buf);
				if (len < 0)
				{
					F_("ngcs_marshal_active_db_h() failed!\n");
					free(buf);
					return (TRUE);
				}
			}
			ngcs_chan_send(watch->chan, NGCS_TYPE_STRUCT, len, buf);
		}
	}
	if (buf)
		free(buf);

	return (TRUE);
}

static int ngcs_watch_initial(ngcs_watch * watch)
{
	if (watch->flags & NGCS_CURRENT_STATUS)
	{
		active_db_h *current;

		current = NULL;
		while_active_db(current)
		{
			if (watch->name == NULL
				|| strcmp(watch->name, current->name) == 0)
			{
				int len = 0;
				char *buf = NULL;

				len = ngcs_marshal_active_db_h(current, NULL);
				buf = i_calloc(1, len);
				len = ngcs_marshal_active_db_h(current, buf);
				if (len < 0)
				{
					F_("ngcs_marshal_active_db_h() failed!\n");
					free(buf);
					return 1;
				}
				if (ngcs_chan_send(watch->chan, NGCS_TYPE_STRUCT, len, buf))
				{
					free(buf);
					return 1;
				};
				free(buf);
			}
		}
	}

	if (ngcs_chan_send(watch->chan, NGCS_TYPE_NULL, 0, NULL))
		return 1;

	return 0;
}

static int service_output_watch(s_event * event)
{
	s_event_buffer_watcher_data *data;
	ngcs_watch *watch, *nextwatch;
	ngcs_data dat[2];
	int len = 0;
	char *buf = NULL;;

	assert(event->event_type == &EVENT_BUFFER_WATCHER);
	assert(event->data);

	data = event->data;

	dat[0].type = NGCS_TYPE_STRING;
	dat[0].len = -1;
	dat[0].d.s = data->service->name;
	dat[1].type = NGCS_TYPE_STRING;
	dat[1].len = -1;
	dat[1].d.s = data->buffer_pos;

	list_for_each_entry_prev_safe(watch, nextwatch, &watches.list, list)
	{
		if ((watch->flags & NGCS_WATCH_OUTPUT) &&
			(watch->name == NULL || strcmp(watch->name, data->service->name) == 0))
		{
			if (!buf)
			{
				len = ngcs_pack(dat, 2, NULL);
				assert(len >= 0);
				buf = i_calloc(1, len);
				len = ngcs_pack(dat, 2, buf);
				assert(len >= 0);
			}
			if (ngcs_chan_send(watch->chan, NGCS_TYPE_STRUCT, len, buf))
			{
				free(buf);
				return FALSE;
			};
		}
	}

	if (buf)
		free(buf);

	return (FALSE);
}

static void ngcs_cmd_stop(ngcs_request * req)
{
	int i = 0;
	ngcs_watch *watch;
	active_db_h *serv = NULL;
	char *svcname = NULL;

	if (req->argc != 2 || req->argv[1].type != NGCS_TYPE_STRING ||
		req->argv[1].len <= 0)
	{
		F_("Bad call to ngcs command 'stop'\n");
		ngcs_send_response(req, NGCS_TYPE_STRING, 8, "BAD_CALL");
		return;
	}

	svcname = req->argv[1].d.s;

	serv = initng_active_db_find_in_name(svcname);
	if (!serv)
	{
		ngcs_send_response(req, NGCS_TYPE_STRING, 9, "NOT_FOUND");
		return;
	}

	watch = ngcs_add_watch(req->conn, serv->name,
						   NGCS_WATCH_STATUS | NGCS_WATCH_OUTPUT |
						   NGCS_CURRENT_STATUS);
	if (watch)
		i = watch->chan->id;
	ngcs_send_response(req, NGCS_TYPE_INT, sizeof(int), (char *) &i);
	ngcs_watch_initial(watch);
	initng_handler_stop_service(serv);
	return;
}


static void ngcs_cmd_start(ngcs_request * req)
{
	int i = 0;
	ngcs_watch *watch;
	active_db_h *serv = NULL;
	char *svcname = NULL;

	if (req->argc != 2 || req->argv[1].type != NGCS_TYPE_STRING ||
		req->argv[1].len <= 0)
	{
		F_("Bad call to ngcs command 'start'\n");
		ngcs_send_response(req, NGCS_TYPE_STRING, 8, "BAD_CALL");
		return;
	}

	svcname = req->argv[1].d.s;

	serv = initng_active_db_find_in_name(svcname);
	if (serv)
	{
		watch = ngcs_add_watch(req->conn, serv->name,
							   NGCS_WATCH_STATUS | NGCS_WATCH_OUTPUT |
							   NGCS_CURRENT_STATUS);
		if (watch)
			i = watch->chan->id;
		ngcs_send_response(req, NGCS_TYPE_INT, sizeof(int), (char *) &i);
		ngcs_watch_initial(watch);
		if (!IS_UP(serv))
			initng_handler_start_service(serv);
		return;
	}

	serv = initng_handler_start_new_service_named(svcname);
	if (!serv)
	{
		ngcs_send_response(req, NGCS_TYPE_STRING, 9, "NOT_FOUND");
		return;
	}

	watch = ngcs_add_watch(req->conn, serv->name,
						   NGCS_WATCH_STATUS | NGCS_WATCH_OUTPUT);
	if (watch)
		i = watch->chan->id;
	ngcs_send_response(req, NGCS_TYPE_INT, sizeof(int), (char *) &i);
	ngcs_watch_initial(watch);
	return;
}

ngcs_watch *ngcs_add_watch(ngcs_conn * conn, char *svcname, int flags)
{
	assert(conn);
	ngcs_chan *chan;
	ngcs_watch *watch;

	watch = i_calloc(1, sizeof(ngcs_watch));

	chan = ngcs_open_channel(conn, NULL, ngcs_free_watch);
	if (!chan)
	{
		F_("ngcs_open_channel failed!\n");
		free(watch);
		return 0;
	}
	if (svcname)
		watch->name = i_strdup(svcname);
	else
		watch->name = NULL;
	watch->flags = flags;
	watch->chan = chan;
	watch->list.prev = 0;
	watch->list.next = 0;
	list_add(&watch->list, &watches.list);
	chan->userdata = watch;
	return watch;
}

static void ngcs_cmd_swatch(ngcs_request * req)
{
	int i = 0;
	ngcs_genwatch *watch;
	ngcs_chan *chan;

	watch = i_calloc(1, sizeof(ngcs_genwatch));

	chan = ngcs_open_channel(req->conn, NULL, &ngcs_free_genwatch);
	if (!chan)
	{
		F_("ngcs_open_channel failed!\n");
		free(watch);
		return;
	}
	watch->chan = chan;
	watch->list.prev = 0;
	watch->list.next = 0;
	list_add(&watch->list, &swatches.list);

	i = chan->id;
	ngcs_send_response(req, NGCS_TYPE_INT, sizeof(int), (char *) &i);
}

static void ngcs_cmd_ewatch(ngcs_request * req)
{
	int i = 0;
	ngcs_genwatch *watch;
	ngcs_chan *chan;

	watch = i_calloc(1, sizeof(ngcs_genwatch));

	chan = ngcs_open_channel(req->conn, NULL, &ngcs_free_genwatch);
	if (!chan)
	{
		F_("ngcs_open_channel failed!\n");
		free(watch);
		return;
	}
	watch->chan = chan;
	watch->list.prev = 0;
	watch->list.next = 0;
	list_add(&watch->list, &ewatches.list);

	i = chan->id;
	ngcs_send_response(req, NGCS_TYPE_INT, sizeof(int), (char *) &i);
}

static void ngcs_cmd_watch(ngcs_request * req)
{
	int i = 0;
	ngcs_watch *watch;
	char *name;

	if (req->argc != 3 || req->argv[1].type != NGCS_TYPE_INT ||
		(req->argv[2].type != NGCS_TYPE_STRING && req->argv[2].type !=
		 NGCS_TYPE_NULL))
	{
		F_("Bad watch command\n");
		ngcs_send_response(req, NGCS_TYPE_INT, sizeof(int), (char *) &i);
		return;
	}
	if (req->argv[2].type == NGCS_TYPE_NULL || req->argv[2].d.s[0] == '\0')
		name = NULL;
	else
		name = req->argv[2].d.s;

	watch = ngcs_add_watch(req->conn, name, req->argv[1].d.i);
	if (watch)
		i = watch->chan->id;
	ngcs_send_response(req, NGCS_TYPE_INT, sizeof(int), (char *) &i);
	ngcs_watch_initial(watch);
}

static void ngcs_free_watch(ngcs_chan * chan)
{
	ngcs_watch *watch = chan->userdata;

	if (!watch)
		return;
	list_del(&watch->list);
	if (watch->name)
		free(watch->name);
	free(watch);
	chan->userdata = 0;
}

static void ngcs_free_genwatch(ngcs_chan * chan)
{
	ngcs_genwatch *watch = chan->userdata;

	if (!watch)
		return;
	list_del(&watch->list);
	free(watch);
	chan->userdata = 0;
}

void register_ngcs_cmds(void)
{
	ngcs_reg_cmd(&ngcs_start_cmd);
	ngcs_reg_cmd(&ngcs_stop_cmd);
	ngcs_reg_cmd(&ngcs_watch_cmd);
	ngcs_reg_cmd(&ngcs_swatch_cmd);
	ngcs_reg_cmd(&ngcs_ewatch_cmd);
	ngcs_reg_cmd(&ngcs_hot_reload_cmd);
	ngcs_reg_cmd(&ngcs_hot_reload_cmd_short);
	ngcs_reg_cmd(&ngcs_hot_reload_cmd_long);
	ngcs_reg_cmd(&ngcs_zap_cmd);
	initng_event_hook_register(&EVENT_STATE_CHANGE, &service_status_watch);
	initng_event_hook_register(&EVENT_BUFFER_WATCHER, &service_output_watch);
	initng_event_hook_register(&EVENT_SYSTEM_CHANGE, &system_state_watch);
	initng_event_hook_register(&EVENT_ERROR_MESSAGE, &error_watch);
	INIT_LIST_HEAD(&watches.list);
	INIT_LIST_HEAD(&swatches.list);
	INIT_LIST_HEAD(&ewatches.list);
}

void unregister_ngcs_cmds(void)
{
	initng_event_hook_unregister(&EVENT_STATE_CHANGE, &service_status_watch);
	initng_event_hook_unregister(&EVENT_BUFFER_WATCHER, &service_output_watch);
	ngcs_unreg_cmd(&ngcs_start_cmd);
	ngcs_unreg_cmd(&ngcs_stop_cmd);
	ngcs_unreg_cmd(&ngcs_watch_cmd);
	ngcs_unreg_cmd(&ngcs_swatch_cmd);
	ngcs_unreg_cmd(&ngcs_ewatch_cmd);
	ngcs_unreg_cmd(&ngcs_hot_reload_cmd);
	ngcs_unreg_cmd(&ngcs_hot_reload_cmd_short);
	ngcs_unreg_cmd(&ngcs_hot_reload_cmd_long);
	ngcs_unreg_cmd(&ngcs_zap_cmd);
	/* FIXME - *really* need to free watches */
}

static void ngcs_cmd_zap(ngcs_request * req)
{
	int i = 0;
	active_db_h *apt = NULL;

	if (req->argc != 2 || req->argv[1].type != NGCS_TYPE_STRING)
	{
		ngcs_send_response(req, NGCS_TYPE_NULL, 0, NULL);
		return;
	}

	if (!(apt = initng_active_db_find_in_name(req->argv[1].d.s)))
	{
		ngcs_send_response(req, NGCS_TYPE_STRING, 9, "NOT_FOUND");
		return;
	}

	initng_active_db_free(apt);

	/* also flush file cache */
	/* cmd_reload(arg); - FIXME */

	/* return happily */
	ngcs_send_response(req, NGCS_TYPE_INT, sizeof(int), (char *) &i);
}

/* ngc2 special-cases this in the client. I prefer to do it here
   and use the general client code, which I can do due to differences
   between the ngcs API and the standard s_command one... */
static void ngcs_cmd_hot_reload(ngcs_request * req)
{
	int i = 1;

	int retval;
	char *new_argv[4];

	retval = initng_plugin_callers_dump_active_db();

	if (retval == TRUE)
	{
		ngcs_send_response(req, NGCS_TYPE_BOOL, sizeof(int), (char *) &i);
		D_("exec()ing initng\n");
		new_argv[0] = i_strdup("/sbin/initng");
		new_argv[1] = i_strdup("--hot_reload");
		new_argv[2] = NULL;

		execve("/sbin/initng", new_argv, environ);
		F_("Failed to reload initng!\n");
	}
	else if (retval == FALSE)
	{
		F_("No plugin was willing to dump state\n");
		ngcs_send_response(req, NGCS_TYPE_ERROR, 13,
						   (char *) "NOT_AVAILABLE");
	}
	else
	{
		F_("dump_state failed!\n");
		ngcs_send_response(req, NGCS_TYPE_ERROR, 10, (char *) "DUMP_ERROR");
	}
}
