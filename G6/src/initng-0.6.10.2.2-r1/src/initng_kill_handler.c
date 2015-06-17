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

#include <time.h>							/* time() */
#include <fcntl.h>							/* fcntl() */
#include <sys/un.h>							/* memmove() strcmp() */
#include <sys/wait.h>						/* waitpid() sa */
#include <linux/kd.h>						/* KDSIGACCEPT */
#include <sys/ioctl.h>						/* ioctl() */
#include <stdio.h>							/* printf() */
#include <stdlib.h>							/* free() exit() */
#include <sys/reboot.h>						/* reboot() RB_DISABLE_CAD */
#include <assert.h>

#include "initng_global.h"
#include "initng_handler.h"
#include "initng_process_db.h"
#include "initng_execute.h"
#include "initng_toolbox.h"
#include "initng_main.h"
#include "initng_common.h"

//#include "initng_service_cache.h"
#include "initng_active_db.h"
#include "initng_load_module.h"
#include "initng_plugin_callers.h"
#include "initng_process_db.h"
#include "initng_fd.h"

#include "initng_kill_handler.h"
#include "initng_static_data_id.h"
#include "initng_static_states.h"

/* called when a process got killed, identify it, and make a call with a pointer to the process */
void initng_kill_handler_killed_by_pid(pid_t kpid, int r_code)
{
	/* The process that got killed */
	active_db_h *service = NULL;
	process_h *process = NULL;
	pipe_h *current_pipe = NULL;

	D_("handle_killed_by_pid(%i);\n", kpid);

	/* don't do anything on this pid */
	if (kpid <= 1)
		return;

	/* Look in process database for a match */
	if (!(service = initng_active_db_find_by_pid(kpid)))
	{
		D_("handle_killed_by_pid(%i): No match in active_db!\n", kpid);
		return;
	}

	D_("handle_killed_by_pid(%i): found service \"%s\"...\n", kpid,
	   service->name);

	/* Get the process pointer from the service */
	if (!(process = initng_process_db_get_by_pid(kpid, service)))
	{
		W_("Could not fetch process, even when initng_active_db_find_by_pid() saw it here!\n");
		return;
	}

	D_("(%i), found process type: %s\n", kpid, process->pt->name);


	/* set r_code */
	process->r_code = r_code;


	/* close all pipes */
	while_pipes(current_pipe, process)
	{
		if ((current_pipe->dir == OUT_PIPE
			 || current_pipe->dir == BUFFERED_OUT_PIPE)
			&& current_pipe->pipe[0] > 0)
		{
			/*
			 * calling initng_process_read_input, Make sure all buffers read, before closing them.
			 */
			initng_fd_process_read_input(service, process, current_pipe);

			/* now close */
			close(current_pipe->pipe[0]);
			current_pipe->pipe[0] = -1;
		}

		else if (current_pipe->dir == IN_PIPE && current_pipe->pipe[1] > 0)
		{
			close(current_pipe->pipe[1]);
			current_pipe->pipe[1] = -1;
		}
	}

	/* Check if a plugin wants to override handle_killed behavior */
	if (initng_plugin_callers_handle_killed(service, process))
	{
		D_("Plugin did handle this kill.\n");
		return;
	}


	/* launch a kill_handler if any */
	if (process->pt && process->pt->kill_handler)
	{
		D_("Launching process->pt->kill_handler\n");
		(*process->pt->kill_handler) (service, process);
	}
	else
	{
		D_("service %s pid %i p_type %s died with unknown handler, freeing process!\n", service->name, kpid, process->pt->name);
		initng_process_db_free(process);
	}
}
