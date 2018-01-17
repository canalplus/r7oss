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
#include <unistd.h>							/* execv() pipe() usleep() pause() chown() pid_t */
#include <sys/types.h>
#include <sys/wait.h>						/* waitpid() sa */
#include <sys/ioctl.h>						/* ioctl() */
#include <sys/socket.h>						/* socketpair() */
#include <stdlib.h>							/* free() exit() */
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <pwd.h>
#include <sys/stat.h>
#include <errno.h>

#include "initng_common.h"
#include "initng_active_db.h"
#include "initng_process_db.h"
#include "initng_signal.h"
#include "initng_plugin_callers.h"
#include "initng_execute.h"
#include "initng_toolbox.h"
#include "initng_static_event_types.h"
#include "initng_event.h"

#include "initng_fork.h"

void initng_fork_aforkhooks(active_db_h * service, process_h * process)
{
	s_event event;
	s_event_after_fork_data data;

	event.event_type = &EVENT_AFTER_FORK;
	event.data = &data;

	data.process = process;
	data.service = service;

	if (initng_event_send(&event) == FAIL) {
		F_("Some plugin did fail in after fork launch.\n");
		_exit(1);
	}
}

pid_t initng_fork(active_db_h * service, process_h * process)
{
	/* This is the real service kicker */
	pid_t pid_fork;				/* pid got from fork() */
	int try_count = 0;			/* Count tryings */
	pipe_h *current_pipe = NULL;	/* used while walking */

	assert(service);
	assert(process);

	/* Create all pipes */
	while_pipes(current_pipe, process)
	{
		if (current_pipe->dir == IN_AND_OUT_PIPE)
		{
			/*printf("calling socketpair:\n"); */
			/* create an two directional pipe with socketpair */
			if (socketpair(AF_UNIX, SOCK_STREAM, 0, current_pipe->pipe) < 0)
			{
				F_("Fail call socketpair: \"%s\"\n", strerror(errno));
				return (-1);
			}
			/* printf("parent: fd%i fork: fd%i\n", current_pipe->pipe[1], current_pipe->pipe[0]); */

		}
		else
		{
			/* create an one directional pipe with pipe */
			if (pipe(current_pipe->pipe) != 0)
			{
				F_("Failed adding pipe ! %s\n", strerror(errno));
				return (-1);
			}
		}
	}

	/* reset, used for walking later */
	current_pipe = NULL;

	/* Try to fork 30 times */
	while ((pid_fork = fork()) == -1)
	{
		if (try_count > 30)
		{									/* Already tried 30 times = no more try */
			F_("GIVING UP TRY TO FORK, FATAL for service.\n");
			return (pid_fork);
		}
		F_("FAILED TO FORK! try no# %i, this can be FATAL!\n", try_count);
		usleep(++try_count * 2000);			/* Sleep a while before trying again */
	}

	/* if this is child */
	if (pid_fork == 0)
	{
		/* from now, don't trap signals */
		initng_signal_disable();

#ifdef DEBUG
		/* unverbose by default for now */
		/* g.verbose = 0; */
#endif


		if (g.i_am != I_AM_UTILITY)
		{
			/* TODO, comment this */
			if (g.i_am == I_AM_INIT)
			{
				ioctl(0, TIOCNOTTY, 0);
				setsid();					/* Run a program in a new session ??? */
			}

			/*
			 * set up file descriptors, for local fork,
			 * a fork in initng, should now receive any input, but stdout & stderr, should be sent
			 * to process->out_pipe[], that is set up by pipe() #man 2 pipe
			 * [0] for reading, and [1] for writing, as the pipe is for sending output
			 * FROM the fork, to initng for handle, the input part should be closed here,
			 * the other are mapped to STDOUT and STDERR.
			 */

			/* walk thru all the added pipes */
			while_pipes(current_pipe, process)
			{
				int i;

				/* for every target */
				for (i = 0; current_pipe->targets[i] > 0 && i < MAX_TARGETS;
					 i++)
				{
					/* close any conflicting one */
					close(current_pipe->targets[i]);

					if (current_pipe->dir == OUT_PIPE
						|| current_pipe->dir == BUFFERED_OUT_PIPE)
					{
						/* duplicate the new target right */
						dup2(current_pipe->pipe[1], current_pipe->targets[i]);
					}
					else if (current_pipe->dir == IN_PIPE)
					{
						/* duplicate the input pipe instead */
						dup2(current_pipe->pipe[0], current_pipe->targets[i]);
					}
					else if (current_pipe->dir == IN_AND_OUT_PIPE)
					{
						/* in a unidirectional socket, there is pipe[0] that is used in the child */
						/*printf("dup2(%i, %i);\n", current_pipe->pipe[0], current_pipe->targets[i]); */
						dup2(current_pipe->pipe[0], current_pipe->targets[i]);
					}
					else
						continue;

					/* IMPORTANT Tell the os not to close the new target on execve */
					/*printf("Put non close: fd%i\n", current_pipe->targets[i]); */
					fcntl(current_pipe->targets[i], F_SETFD, 0);
				}
			}
		}

		/* TODO, what does this do? */
		if (g.i_am == I_AM_INIT)
			tcsetpgrp(0, getpgrp());		/* run this in foreground on fd 0 */


		/* do a minimum sleep, to let the mother process
		   to register child, and notice death */
		usleep(ALL_USLEEP);

	}
	else
	{

		/* walk all pipes and close all remote sides of pipes */
		while_pipes(current_pipe, process)
		{
			if (current_pipe->dir == OUT_PIPE
				|| current_pipe->dir == BUFFERED_OUT_PIPE)
			{
				if (current_pipe->pipe[1] > 0)
					close(current_pipe->pipe[1]);
				current_pipe->pipe[1] = -1;
			}
			/* close the OUTPUT end */
			else if (current_pipe->dir == IN_PIPE)
			{
				if (current_pipe->pipe[0] > 0)
					close(current_pipe->pipe[0]);
				current_pipe->pipe[0] = -1;
			}

			else if (current_pipe->dir == IN_AND_OUT_PIPE)
			{
				/* in an unidirectional pipe, pipe[0] is fork, and pipe[1] is parent */
				/*printf("parent closing: fd%i\n", current_pipe->pipe[0]); */
				if (current_pipe->pipe[0] > 0)
					close(current_pipe->pipe[0]);
				current_pipe->pipe[0] = -1;
			}
		}

		/* set process->pid if lucky */
		if (pid_fork > 0)
		{
			process->pid = pid_fork;
		}
	}

	return (pid_fork);
}
