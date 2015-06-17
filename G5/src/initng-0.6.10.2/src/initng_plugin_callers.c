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

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <dlfcn.h>
#include <assert.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>							/* fcntl() */
#include <time.h>
#include "initng.h"
#include "initng_global.h"
//#include "initng_service_cache.h"
#include "initng_load_module.h"
#include "initng_toolbox.h"
#include "initng_signal.h"
#include "initng_static_event_types.h"
#include "initng_common.h"

#include "initng_plugin_callers.h"
#include "initng_plugin.h"


active_db_h *initng_plugin_create_new_active(const char *name)
{
	s_event event;
	s_event_new_active_data data;

	event.event_type = &EVENT_NEW_ACTIVE;
	event.data = &data;

	data.name = name;

	if (initng_event_send(&event) == HANDLED) {
		return data.ret;
	}

	return (NULL);
}


void initng_plugin_callers_signal(int signal)
{
	s_event event;

	event.event_type = &EVENT_SIGNAL;
	event.data = &signal;

	initng_event_send(&event);
}

int initng_plugin_callers_handle_killed(active_db_h * s, process_h * p)
{
	s_event event;
	s_event_handle_killed_data data;

	event.event_type = &EVENT_HANDLE_KILLED;
	event.data = &data;
	data.service = s;
	data.process = p;

	if (initng_event_send(&event) == HANDLED)
		return (TRUE);

	return (FALSE);
}


void initng_plugin_callers_compensate_time(int t)
{
	s_event event;

	event.event_type = &EVENT_COMPENSATE_TIME;
	event.data = (void *) (long)t;

	initng_event_send(&event);
}

/* called when system state has changed. */
void initng_plugin_callers_load_module_system_changed(h_sys_state state)
{
	s_event event;

	event.event_type = &EVENT_SYSTEM_CHANGE;
	event.data = &state;

	initng_event_send(&event);

	return;
}

/* called to dump active_db */
int initng_plugin_callers_dump_active_db(void)
{
	s_event event;

	event.event_type = &EVENT_DUMP_ACTIVE_DB;

	return initng_event_send(&event);
}

/* called to reload dump of active_db */
int initng_plugin_callers_reload_active_db(void)
{
	s_event event;

	event.event_type = &EVENT_RELOAD_ACTIVE_DB;

	return initng_event_send(&event);
}
