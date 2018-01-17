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

#include <initng_control_command.h>

/* Warning.
 * Changing these variables, will make reload incompatible,
 * and probably hang initng
 */
#define MAX_SERVICE_NAME_STRING_LEN 200
#define MAX_SERVICE_STATE_LEN 100
#define MAX_DATA_STRING_LEN 200
#define MAX_TYPE_STRING_LEN 100
#define MAX_PROCESSES 6
#define MAX_PTYPE_STRING_LEN 100
#define MAX_ENTRYS_FOR_SERVICE 20
#define MAX_DATA_VN_LEN 100
#define MAX_PIPES 6
#define MAX_PIPE_TARGETS 5

typedef struct
{
	char type[MAX_TYPE_STRING_LEN + 1];
	e_dt opt_type;
	char vn[MAX_DATA_VN_LEN + 1];
	union
	{
		char s[MAX_DATA_STRING_LEN + 1];
		int i;
	} t;
} r_d_e;

typedef struct
{
	int pipe[2];
	e_dir dir;
	int targets[MAX_PIPE_TARGETS + 1];
} r_pipe;


typedef struct
{
	char ptype[MAX_PTYPE_STRING_LEN + 1];
	r_pipe pipes[MAX_PIPES + 1];
	int pid;
	int rcode;
} r_process;

/* this lines will the active contain */
typedef struct
{
	char name[MAX_SERVICE_NAME_STRING_LEN + 1];
	char type[MAX_TYPE_STRING_LEN + 1];
	char state[MAX_SERVICE_STATE_LEN + 1];
	struct timespec time_current_state;

	/*  struct with some processes */
	r_process process[MAX_PROCESSES];

	/* struct with some data */
	r_d_e data[MAX_ENTRYS_FOR_SERVICE + 1];
} data_save_struct;

typedef struct
{
        char type[MAX_TYPE_STRING_LEN + 1];
        e_dt opt_type;
        union
        {
                char s[MAX_DATA_STRING_LEN + 1];
                int i;
        } t;
} r_d_e_v13;

typedef struct
{
        char ptype[MAX_PTYPE_STRING_LEN + 1];
        int stdout1;
        int stdout2;
        int pid;
        int rcode;
} r_process_v13;

/* this lines will the active contain */
typedef struct
{
        char name[MAX_SERVICE_NAME_STRING_LEN + 1];
        char type[MAX_TYPE_STRING_LEN + 1];
        char state[101];
        struct timespec time_current_state;

        /*  struct with some processes */
        r_process_v13 process[MAX_PROCESSES];

        /* struct with some data */
        r_d_e_v13 data[MAX_ENTRYS_FOR_SERVICE + 1];
} data_save_struct_v13;
