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

#include "initng_nge.h"
#include <initng_is.h>
#include <initng_system_states.h>
#include <initng_msg.h>


typedef enum
{
	PING = 0,
	SERVICE_STATE_CHANGE = 1,
	SYSTEM_STATE_CHANGE = 2,
	ERR_MSG = 3,
	CONNECT = 4,
	DISSCONNECT = 5,
	INITIAL_SERVICE_STATE_CHANGE = 6,
	INITIAL_SYSTEM_STATE_CHANGE = 7,
	INITIAL_STATE_FINISHED = 8,
	SERVICE_OUTPUT = 9,
	PROCESS_KILLED = 10,
} e_state_type;

typedef struct
{
	/* standard variables to have */
	int sock;
	char *read_buffer;
	int read_buffer_len;

	/* user data variable, not used by initng itself */
	void *user_data;
} nge_connection;

typedef struct
{
	e_state_type state_type;

	/* Here comes the possible payload */
	union
	{
		/* service_state_change and initial_service_state */
		struct
		{
			char *service;
			e_is is;
			char *state_name;
			int percent_started;
			int percent_stopped;
			char *service_type;
			int hidden;
		} service_state_change;

		/* process_killed event */
		struct
		{
			char *service;
			e_is is;
			char *state_name;
			char *process;
			int exit_status;
			int term_sig;
		} process_killed;

		/* system_state_change  and initial_system_state */
		struct
		{
			h_sys_state system_state;
			char *runlevel;
		} system_state_change;

		/* service_output */
		struct
		{
			char *service;
			char *process;
			char *output;
		} service_output;

		/* err_msg */
		struct
		{
			e_mt mt;
			char *file;
			char *func;
			int line;
			char *message;
		} err_msg;

		/* connect */
		struct
		{
			int pver;
			char *initng_version;
		} connect;
	} payload;

} nge_event;

extern const char *ngeclient_error;

nge_event *get_next_event(nge_connection * c, int block);
void ngeclient_event_free(nge_event * e);

nge_connection *ngeclient_connect(const char *path);
void ngeclient_close(nge_connection * c);


int ngeclient_poll_for_input(nge_connection * c, int sec);
