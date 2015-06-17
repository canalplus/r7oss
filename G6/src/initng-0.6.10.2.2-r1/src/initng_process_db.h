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

#ifndef PROCESS_DB_H
#define PROCESS_DB_H

#include <sys/types.h>
#include <unistd.h>
#include "initng_active_db.h"
//#include "initng_service_cache.h"
#include "initng_list.h"

/* this doesn't work!, it will create a circular dependency */
/* so we use a struct prototype !! */
/* this works, as we only declare pointers to this struct here. */

//struct active_type;

#define MAX_TARGETS 10

/* struct containing data on an running process */
typedef struct t_ptype_h ptype_h;
typedef struct t_process_h process_h;

/* process state */
typedef enum
{
	P_ACTIVE = 0,
	P_FREE = 1,
} e_pst;

typedef enum
{
	UNKNOWN_PIPE = 0,
	OUT_PIPE = 1,
	IN_PIPE = 2,
	BUFFERED_OUT_PIPE = 3,
	IN_AND_OUT_PIPE = 4,
} e_dir;

/* the pipe identifier */
typedef struct
{
	/* this array contains the pipe fds */
	int pipe[2];

	/* The direction of the stream, an OUTPUT or INPUT ?? */
	e_dir dir;

	/* If targets set (max 10) the fd are duped after fork to match targets */
	int targets[MAX_TARGETS + 1];

	/* If this pipe is a BUFFERED_OUT_PIPE stor a buffer here */
	char *buffer;				/* stdout buffer ## THE BEGINNING ## */
	int buffer_allocated;		/* chars right now allocated for this buffer */
	int buffer_len;				/* the count of chars from the beginning in buffer right now */

	/* The list entry */
	struct list_head list;
} pipe_h;

struct t_process_h
{
	ptype_h *pt;
	pid_t pid;					/* pid of process */

	/*
	 * r_code is the return code, this is not the process exit(1) no
	 * instead see man waitpid for how to use it.
	 * example exit_code = WEXITSTATUS(process->r_code);
	 */
	int r_code;

	/* This is a list of pipes open to this process */
	pipe_h pipes;

	/* small mark, this process are not freed directly this will be set to P_FREE */
	e_pst pst;

	struct list_head list;		/* this process should be in a list */
};

struct t_ptype_h
{
	/* name to be used with launcher */
	const char *name;

	/* The function to call, when the process is returning */
	void (*kill_handler) (active_db_h * service, process_h * process);

	/* the list of process types */
	struct list_head list;
};

#define initng_process_db_free(pr) (pr)->pst=P_FREE

/* functions for creating and freeing a process struct */
process_h *initng_process_db_new(ptype_h * ptype);
void initng_process_db_real_free(process_h * free_this);
process_h *initng_process_db_get(ptype_h * type, active_db_h * service);
process_h *initng_process_db_get_by_name(const char *name,
										 active_db_h * service);
process_h *initng_process_db_get_by_pid(pid_t pid, active_db_h * service);

#define while_processes(current, service) list_for_each_entry_prev(current, &service->processes.list, list)
#define while_processes_safe(current, safe, service) list_for_each_entry_prev_safe(current, safe, &service->processes.list, list)

#define while_ptypes(current) list_for_each_entry_prev(current, &g.ptypes.list, list)
#define while_ptypes_safe(current, safe) list_for_each_entry_prev_safe(current, safe, &g.ptypes.list, list)

#define initng_process_db_ptype_register(pt) list_add(&(pt)->list, &g.ptypes.list)
#define initng_process_db_ptype_unregister(pt) list_del(&(pt)->list)

#define initng_process_db_register_to_service(p_t_a, s_t_a) list_add(&(p_t_a)->list, &(s_t_a)->processes.list)

void initng_process_db_clear_freed(active_db_h * service);
ptype_h *initng_process_db_ptype_find(const char *name);

/* add the process to our service */
#define add_process(pss, sss) list_add(&(pss)->list, &(sss)->processes.list);

/* used for browing pipes */
pipe_h *pipe_new(e_dir dir);

#define add_pipe(PIPE, PROCESS) list_add(&(PIPE)->list, &(PROCESS)->pipes.list);
#define while_pipes(CURRENT, PROCESS) list_for_each_entry_prev(CURRENT, &(PROCESS)->pipes.list, list)
#define while_pipes_safe(CURRENT, PROCESS, SAFE) list_for_each_entry_prev_safe(CURRENT, SAFE, &(PROCESS)->pipes.list, list)

#endif
