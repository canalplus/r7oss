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

#ifndef NGC4_H
#define NGC4_H

#include <initng_active_db.h>
#include <initng_system_states.h>
#include <initng_control_command.h>

#define SOCKET_4_ROOTPATH      DEVDIR "/initng/"
#define SOCKET_4_FILENAME_REAL SOCKET_4_ROOTPATH "initng-4"
#define SOCKET_4_FILENAME_TEST SOCKET_4_ROOTPATH "initng-4-test"

#define PROTOCOL_4_VERSION 9

typedef enum
{
	NO_PAYLOAD = 0,
	HELP_ROW = 1,
	ACTIVE_ROW = 2,
	STATE_ROW = 3,
	OPTION_ROW = 4,
} data_type;

/* this is a structure for an help_row payload */
typedef struct
{
	data_type dt;				/* == HELP_ROW */
	char c;
	char l[101];
	e_com_type t;
	e_opt_type o;
	char d[201];
} help_row;

/* this is a structure for an active_row payload */
typedef struct
{
	data_type dt;				/* == ACTIVE_ROW */
	char state[101];			/* status, a word describing the status */
	char name[101];				/* name of service */
	struct timespec time_set;	/* time status set */
	e_is is;					/* is status, status in a number */
	char type[101];				/* Type of service this is */
} active_row;

/* this is a structor for a state description payload */
typedef struct
{
	data_type dt;				/* == STATE_ROW */
	char name[101];
	e_is is;
	char desc[201];
} state_row;

/* this is a structure for an option_row payload */
typedef struct
{
	data_type dt;				/* == OPTION_ROW */
	char n[101];
	e_dt t;
	char o[101];
	char d[301];
} option_row;

/* an enum sent in the reply, signals the status of the reply */
typedef enum
{
	S_FALSE = 0,
	S_TRUE = 1,
	S_REQUIRES_OPT = 2,
	S_NOT_REQUIRES_OPT = 3,
	S_INVALID_TYPE = 4,
	S_COMMAND_NOT_FOUND = 5
} e_suceed;

/*
 * This binary structure is the reply initng will send
 * after a header.
 */
typedef struct
{
	e_suceed s;					/* succeed, TRUE or FALSE */
	char c;
	e_com_type t;				/* type of data got back */
	char version[101];
	int p_ver;
	size_t payload;
} result_desc;

/*
 * This is a header structure, that initng fetches from
 * ngc4 as a request.
 */
typedef struct
{
	char c;
	char l[101];
	int p_ver;
	size_t body_len;
} read_header;

#endif
