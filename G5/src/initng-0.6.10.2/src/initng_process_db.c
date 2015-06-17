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

#include "initng.h"

#include <sys/types.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <alloca.h>
#include <assert.h>

#include "initng_active_db.h"
#include "initng_process_db.h"
#include "initng_global.h"

#include "initng_toolbox.h"



/*
 * This function creates a new pipe, and creates a new
 * pipe struct entry.
 */
pipe_h *pipe_new(e_dir dir)
{
	pipe_h *pipe_struct = i_calloc(1, sizeof(pipe_h));

	if (!pipe_struct)
		return (NULL);

	/* set the type */
	pipe_struct->dir = dir;

	/* return the pointer */
	return (pipe_struct);
}

/* creates a process_h struct with defaults */
process_h *initng_process_db_new(ptype_h * type)
{
	process_h *new_p = NULL;
	pipe_h *current_pipe;

	/* allocate a new process entry */
	new_p = (process_h *) i_calloc(1, sizeof(process_h));	/* Allocate memory for a new process */
	if (!new_p)
	{
		F_("Unable to allocate process!\n");
		return (NULL);
	}

	new_p->pt = type;

	/* null pid */
	new_p->pid = 0;

	/* reset return code */
	new_p->r_code = 0;

	/* Set this to active, so it wont get freed */
	new_p->pst = P_ACTIVE;

	/* initziate list of pipes, so we can run add_pipe below without segfault */
	INIT_LIST_HEAD(&new_p->pipes.list);

	/* create the output pipe */
	current_pipe = pipe_new(BUFFERED_OUT_PIPE);
	if (!current_pipe)
	{
		free(new_p);
		return (NULL);
	}

	/* we want this pipe to get fd 1 and 2 in the fork */
	current_pipe->targets[0] = STDOUT_FILENO;
	current_pipe->targets[1] = STDERR_FILENO;
	add_pipe(current_pipe, new_p);

	/* return new process_h pointer */
	return (new_p);
}

/*
 * Gets an special process from an service
 * if it exists 
 */
process_h *initng_process_db_get(ptype_h * type, active_db_h * service)
{
	process_h *current = NULL;

	while_processes(current, service)
	{
		if (current->pst != P_ACTIVE)
			continue;
		if (current->pt == type)
			return (current);
	}
	return (NULL);
}

/*
 * Browse ptypes, search by name
 */
ptype_h *initng_process_db_ptype_find(const char *name)
{
	ptype_h *found = NULL;

	while_ptypes(found)
	{
		if (strcmp(name, found->name) == 0)
		{
			return (found);
		}
	}
	return (NULL);
}

/*
 * Gets an special process from an service
 * if it exists 
 */
process_h *initng_process_db_get_by_name(const char *name,
										 active_db_h * service)
{
	process_h *current = NULL;

	while_processes(current, service)
	{
		if (current->pst != P_ACTIVE)
			continue;
		if (strcmp(current->pt->name, name) == 0)
			return (current);
	}
	return (NULL);

}

/*
 * Finds a process by its pid.
 */
process_h *initng_process_db_get_by_pid(pid_t pid, active_db_h * service)
{
	process_h *current = NULL;

	while_processes(current, service)
	{
		if (current->pst != P_ACTIVE)
			continue;
		if (current->pid == pid)
			return (current);
	}
	return (NULL);
}


void initng_process_db_clear_freed(active_db_h * service)
{
	process_h *current, *safe = NULL;

	while_processes_safe(current, safe, service)
	{
		if (current->pst != P_FREE)
			continue;

		initng_process_db_real_free(current);
	}
}

/* function to free a process_h struct */
void initng_process_db_real_free(process_h * free_this)
{
	pipe_h *current_pipe = NULL;
	pipe_h *current_pipe_safe = NULL;

	assert(free_this);

	/* Make sure this entry are not on any list */
	list_del(&free_this->list);

	while_pipes_safe(current_pipe, free_this, current_pipe_safe)
	{
		/* unbound this pipe from list */
		list_del(&current_pipe->list);

		/* close all pipes */
		if (current_pipe->pipe[0] > 0)
			close(current_pipe->pipe[0]);
		if (current_pipe->pipe[1] > 0)
			close(current_pipe->pipe[1]);

		/* free buffer */
		if (current_pipe->buffer)
			free(current_pipe->buffer);

		/* free it */
		free(current_pipe);
	}

	free(free_this);
	return;
}
