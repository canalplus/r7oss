/*
 * Initng, a next generation sysvinit replacement.
 * Copyright (C) 2006 Jimmy Wennlund <jimmy.wennlund@gmail.com>
 * Copyright (C) 2006 Ismael Luceno <ismael.luceno@gmail.com>
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

#include <string.h>
#include <assert.h>

#include "initng.h"
#include "initng_list.h"
#include "initng_global.h"
#include "initng_event_types.h"
#include "initng_static_event_types.h"

s_event_type EVENT_STATE_CHANGE = { "state_change", "When an active change its status, this event will apere" };
s_event_type EVENT_SYSTEM_CHANGE = { "system_change", "Triggered when system state changes" };
s_event_type EVENT_IS_CHANGE = { "is_change", "Triggered when the rough state of a service changes" };
s_event_type EVENT_UP_MET = { "up_met", "Triggered when a service is trying to set the RUNNING state, is a up test" };
s_event_type EVENT_MAIN = { "main", "Triggered every main loop" };
s_event_type EVENT_LAUNCH = { "launch", "Triggered when a process are getting launched" };
s_event_type EVENT_AFTER_FORK = { "after_fork", "Triggered after a process forks to start" };
s_event_type EVENT_START_DEP_MET = { "start_dep_met", "Triggered when a service is about to start" };
s_event_type EVENT_STOP_DEP_MET	= { "stop_dep_met", "Triggered when a service is about to stop" };
s_event_type EVENT_PIPE_WATCHER = { "pipe_watcher", "Watch pipes for communication" };
s_event_type EVENT_NEW_ACTIVE = { "new_active", "Triggered when initng tries to resolve a nonexistent service to start" };
s_event_type EVENT_DEP_ON = { "dep_on", "Triggered when a function tries to find out a service dependency" };
s_event_type EVENT_RELOAD_ACTIVE_DB = { "reload_active_db", "Asks for a plugin willing to reload the active_db from a dump" };
s_event_type EVENT_DUMP_ACTIVE_DB = { "dump_active_db", "Asks for a plugin willing to dump the active_db" };
s_event_type EVENT_ERROR_MESSAGE = { "error_message", "Triggered when an error message is sent, so all output plug-ins can show it" };
s_event_type EVENT_COMPENSATE_TIME = { "compensate_time", "Triggered when initng detects a system time change" };
s_event_type EVENT_HANDLE_KILLED = { "handle_killed", "Triggered when a process dies" };
s_event_type EVENT_SIGNAL = { "signal", "Triggered when initng rescives a signal, like SIGHUP" };
s_event_type EVENT_BUFFER_WATCHER = { "buffer_watcher", "Triggered when a service have outputed, and initng have filled its output buffer" };
s_event_type EVENT_FD_WATCHER = { "fd_watcher", "Triggered when initng open file descriptors receive data" };

#ifdef SERVICE_CACHE
s_event_type EVENT_PARSE = { "parse", "Triggered when a service needs its data" };
s_event_type EVENT_ADDITIONAL_PARSE = { "additional_parse", "Triggered after a service been parsed, and extra parsing may exist" };
#endif


s_event_type EVENT_INTERRUPT = { "interrupt", "When initng gets an sysreq, it will get here" };
s_event_type HALT = { "halt", "Initng got a request to halt" };
s_event_type REBOOT = { "reboot", "Initng got a request to reboot" };

void initng_register_static_event_types(void)
{
    initng_event_type_register(&EVENT_STATE_CHANGE);
    initng_event_type_register(&EVENT_SYSTEM_CHANGE);
    initng_event_type_register(&EVENT_IS_CHANGE);
    initng_event_type_register(&EVENT_UP_MET);
    initng_event_type_register(&EVENT_MAIN);
    initng_event_type_register(&EVENT_LAUNCH);
    initng_event_type_register(&EVENT_AFTER_FORK);
    initng_event_type_register(&EVENT_START_DEP_MET);
    initng_event_type_register(&EVENT_STOP_DEP_MET);
    initng_event_type_register(&EVENT_PIPE_WATCHER);
    initng_event_type_register(&EVENT_NEW_ACTIVE);
    initng_event_type_register(&EVENT_DEP_ON);
    initng_event_type_register(&EVENT_RELOAD_ACTIVE_DB);
    initng_event_type_register(&EVENT_DUMP_ACTIVE_DB);
    initng_event_type_register(&EVENT_ERROR_MESSAGE);
    initng_event_type_register(&EVENT_COMPENSATE_TIME);
    initng_event_type_register(&EVENT_HANDLE_KILLED);
    initng_event_type_register(&EVENT_SIGNAL);
    initng_event_type_register(&EVENT_BUFFER_WATCHER);
    initng_event_type_register(&EVENT_FD_WATCHER);

#ifdef SERVICE_CACHE
    initng_event_type_register(&EVENT_PARSE);
    initng_event_type_register(&EVENT_ADDITIONAL_PARSE);
#endif

    initng_event_type_register(&EVENT_INTERRUPT);
    initng_event_type_register(&HALT);
    initng_event_type_register(&REBOOT);
}

