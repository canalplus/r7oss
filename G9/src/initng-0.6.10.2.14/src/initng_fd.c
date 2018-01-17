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
#include <sys/select.h>

#include "initng.h"
#include "initng_global.h"
//#include "initng_service_cache.h"
#include "initng_load_module.h"
#include "initng_toolbox.h"
#include "initng_signal.h"

#include "initng_plugin_callers.h"
#include "initng_plugin.h"
#include "initng_static_event_types.h"

#include "initng_fd.h"

void initng_fd_close_all(void)
{
	s_event event;
	s_event_fd_watcher_data data;

	S_;

	event.event_type = &EVENT_FD_WATCHER;
	event.data = &data;
	data.action = FDW_ACTION_CLOSE;

	initng_event_send(&event);

	/* TODO: Should we clear the event hooks list here? */
}

/*
 * This function delivers what read to plugin,
 * through the hooks.
 * If no hook is found, or no return TRUE, it will
 * be printed to screen anyway.
 */
static void initng_fd_plugin_readpipe(active_db_h * service,
									  process_h * process, pipe_h * pi,
									  char *buffer_pos)
{
	s_event event;
	s_event_buffer_watcher_data data;

	S_;

	event.event_type = &EVENT_BUFFER_WATCHER;
	event.data = &data;
	data.service = service;
	data.process = process;
	data.pipe = pi;
	data.buffer_pos = buffer_pos;

	if (initng_event_send(&event) != TRUE)
	{
		/* make sure someone handled this */
		fprintf(stdout, "%s", buffer_pos);
	}
}


/* if there is data incoming in a pipe, tell the plugins */
static int initng_fd_pipe(active_db_h * service, process_h * process,
						  pipe_h * pi)
{
	s_event event;
	s_event_pipe_watcher_data data;

	data.service = service;
	data.process = process;
	data.pipe = pi;

	event.event_type = &EVENT_PIPE_WATCHER;
	event.data = &data;

	if (initng_event_send(&event) == HANDLED)
		return (TRUE);

	return (FALSE);
}


/*
 * This function is called when data is polled below,
 * or when a process is freed ( with flush_buffer set)
 */
void initng_fd_process_read_input(active_db_h * service, process_h * p,
								  pipe_h * pi)
{
	int old_content_offset = pi->buffer_len;
	int read_ret = 0;
	char *tmp;

	D_("\ninitng_fd_process_read_input(%s, %s);\n", service->name,
	   p->pt->name);

	if (pi->pipe[0] <= 0)
	{
		F_("FIFO, can't be read! NOT OPEN!\n");
		return;
	}

	/* INITZIATE
	 * if this are not set to out_pipe, if there is nothing to read. read() will block.
	 * initng and sit down waiting for input.
	 */
	if (!pi->buffer)
	{
		/* initziate buffer fnctl */
		int fd_flags;

		fd_flags = fcntl(pi->pipe[0], F_GETFL, 0);
		fcntl(pi->pipe[0], F_SETFL, fd_flags | O_NONBLOCK);
	}

	/* read data from process, and continue again after a interrupt */
	do
	{
		errno = 0;

		/* OBSERVE, i_realloc may change the path to the data, so dont set buffer_pos to early */

		/* Make sure there is room for 100 more chars */
		D_(" %i (needed buffersize) > %i(current buffersize)\n", pi->buffer_len + 100, pi->buffer_allocated);
		if (pi->buffer_len + 100 >= pi->buffer_allocated)
		{
			/* do a realloc */
			D_("Changing size of buffer %p from %i to: %i bytes.\n", pi->buffer, pi->buffer_allocated,
			   pi->buffer_allocated + 100 + 1 );
			tmp = i_realloc(pi->buffer,
							(pi->buffer_allocated + 100 + 1) * sizeof(char));

			/* make sure realloc suceeded */
			if (tmp)
			{
				D_("pi->buffer changes from %p to %p.\n", pi->buffer, tmp);
				pi->buffer = tmp;
				pi->buffer_allocated += 100;

				/*
				 * make sure it nulls, specially when i_realloc is run for the verry first time
				 * and maby there is nothing to get by read
				 */
				pi->buffer[pi->buffer_len] = '\0';
			}
			else
			{
				F_("realloc failed, possibly out of memory!\n");
				return;
			}
		}

		/* read the data */
		D_("Trying to read 100 chars:\n");
		read_ret = read(pi->pipe[0], &pi->buffer[pi->buffer_len], 100);
		/*printf("read_ret = %i  : \"%.100s\"\n", read_ret, read_pos); */
		D_("And got %i chars...\n", read_ret);

		/* make sure read does not return -1 */
		if (read_ret <= 0)
			break;

		/* increase buffer_len */
		pi->buffer_len += read_ret;

		/* make sure its nulled at end */
		pi->buffer[pi->buffer_len] = '\0';
	}
	/* if read_ret == 100, it migit be more to read, or it got interrupted. */
	while (read_ret >= 100 || errno == EINTR);


	D_("Done reading (read_ret=%i) (errno == %i).\n", read_ret, errno);

	/* make sure there is any */
	if (pi->buffer_len > old_content_offset)
	{
		D_("Calling plugins for new buffer content...");
		/* let all plugin take part of data */
		initng_fd_plugin_readpipe(service, p, pi,
								  pi->buffer + old_content_offset);
	}

	/*if empty, dont waist memory */
	if (pi->buffer_len == 0 && pi->buffer)
	{
		D_("Freeing empty buffer..");
		free(pi->buffer);
		pi->buffer = NULL;
		pi->buffer_allocated = 0;
		pi->buffer_len = 0;
	}


	/*
	 * if EOF close pipes.
	 * Dont free buffer, until the whole process_h frees
	 */
	if (read_ret == 0)
	{
		D_("Closing fifos for %s.\n", service->name);
		if (pi->pipe[0] > 0)
			close(pi->pipe[0]);
		if (pi->pipe[1] > 0)
			close(pi->pipe[1]);
		pi->pipe[0] = -1;
		pi->pipe[1] = -1;

		/* else, realloc to exact size */
		if (pi->buffer && pi->buffer_allocated > (pi->buffer_len + 1))
		{
			tmp = i_realloc(pi->buffer, (pi->buffer_len + 1) * sizeof(char));
			if (tmp)
			{
				pi->buffer = tmp;
				pi->buffer_allocated = pi->buffer_len;
			}
		}
		return;
	}

	/* if buffer reached 10000 chars */
	if (pi->buffer_len > 10000)
	{
		D_("Buffer to big (%i > 10000), purging...", pi->buffer_len);
		/* copy the last 9000 chars to start */
		memmove(pi->buffer, &pi->buffer[pi->buffer_len - 9000],
				9000 * sizeof(char));
		/* rezise the buffer - leave some expansion space! */
		tmp = i_realloc(pi->buffer, 9501 * sizeof(char));

		/* make sure realloc suceeded */
		if (tmp)
		{
			pi->buffer = tmp;
			pi->buffer_allocated = 9500;
		}
		else
		{
			F_("realloc failed, possibly out of memory!\n");
		}

		/* Even if realloc failed, the buffer is still valid
		   and we've still reduced the length of its contents */
		pi->buffer_len = 9000;				/* shortened by 1000 chars */
		pi->buffer[9000] = '\0';			/* shortened by 1000 chars */
	}

	D_("function done...");
}

/*
 * FILEDESCRIPTORPOLLNG
 *
 * File descriptors are scanned in 3 steps.
 * First, scan all plug-ins, and processes after file descriptors to
 * watch, and add them with FD_SET(); added++;
 *
 * Second, Run select if there where any added, with an timeout set
 * when function was called.
 *
 * Third, if select returns that there is data to fetch, walk over
 * plug-ins and file descriptors again, and call plug-ins or initng_process_readpipe
 * with pointer to plugin/service.
 *
 * This function will return FALSE, if it timeout, or if there was
 * no fds to monitor.
 */
void initng_fd_plugin_poll(int timeout)
{
	/* Variables */
	fd_set readset, writeset, errset;
	struct timespec tv;
	int retval;
	int added = 0;
	active_db_h *currentA, *qA;
	process_h *currentP, *qP;
	pipe_h *current_pipe;

	/* initialization */
	S_;
	FD_ZERO(&readset);
	FD_ZERO(&writeset);
	FD_ZERO(&errset);

	/* Set timespec struct, to our timeout */
	tv.tv_sec = timeout;
	tv.tv_nsec = 0;


	/*
	 * STEP 1:  Scan for fds to add
	 */


	/* scan through active plug-ins that have listening file descriptors, and add them */
	{
		s_event event;
		s_event_fd_watcher_data data;

		event.event_type = &EVENT_FD_WATCHER;
		event.data = &data;
		data.action = FDW_ACTION_CHECK;
		data.added = 0;
		data.readset = &readset;
		data.writeset = &writeset;
		data.errset = &errset;

		initng_event_send(&event);

		added += data.added;
	}

	/* Then add all file descriptors from process read pipes */
	currentA = NULL;
	while_active_db(currentA)
	{
		currentP = NULL;
		while_processes(currentP, currentA)
		{
			current_pipe = NULL;
			while_pipes(current_pipe, currentP)
			{
				if ((current_pipe->dir == OUT_PIPE ||
					 current_pipe->dir == BUFFERED_OUT_PIPE) &&
					current_pipe->pipe[0] > 2)
				{
					/* expensive test to make sure the pipe is open, before adding */
					if (!STILL_OPEN(current_pipe->pipe[0]))
					{
						D_("%i is not open anymore.\n",
						   current_pipe->pipe[0]);
						current_pipe->pipe[0] = -1;
						continue;
					}


					FD_SET(current_pipe->pipe[0], &readset);
					added++;
				}

				if (current_pipe->dir == IN_AND_OUT_PIPE &&
					current_pipe->pipe[1] > 2)
				{
					/* expensive test to make sure the pipe is open, before adding */
					if (!STILL_OPEN(current_pipe->pipe[1]))
					{
						D_("%i is not open anymore.\n",
						   current_pipe->pipe[1]);
						current_pipe->pipe[1] = -1;
						continue;
					}


					FD_SET(current_pipe->pipe[1], &readset);
					added++;
				}

				if (current_pipe->dir == IN_PIPE && current_pipe->pipe[1] > 2)
				{
					/* expensive test to make sure the pipe is open, before adding */
					if (!STILL_OPEN(current_pipe->pipe[1]))
					{
						D_("%i is not open anymore.\n",
						   current_pipe->pipe[1]);
						current_pipe->pipe[1] = -1;
						continue;
					}


					FD_SET(current_pipe->pipe[1], &writeset);
					added++;
				}
			}
		}
	}


	/*
	 * STEP 2: Do the select-poll, if any fds where added
	 */

	/* check if there are any set */
	if (added <= 0)
	{
		D_("No file descriptors set.\n");
		sleep(timeout);
		return;
	}
	D_("%i file descriptors added.\n", added);


	/* make the select */
	retval = pselect(256, &readset, &writeset, &errset, &tv, NULL);


	/* error - Truly a interrupt */
	if (retval < 0)
	{
		D_("Select returned %i\n", retval);
		return;
	}

	/* none was found */
	if (retval == 0)
	{
		D_("There was no data found on any added fd.\n");
		return;
	}

	D_("%d fd's active\n", retval);

	/* If a fsck is running select will always return one file handler active, we give it 0.1 seconds to get some more data into the buffer. */
	sleep(0.1);


	/*
	 * STEP 3:  Find the plug-ins/processes that handles the fd input, and call them
	 */

	/* Now scan through callers */
	{
		s_event event;
		s_event_fd_watcher_data data;

		event.event_type = &EVENT_FD_WATCHER;
		event.data = &data;
		data.action = FDW_ACTION_CALL;
		data.readset = &readset;
		data.writeset = &writeset;
		data.errset = &errset;
		data.added = retval;

		initng_event_send(&event);

		retval = data.added;
		if (retval == 0)
			return;
	}

	/* scan every service */
	currentA = NULL;
	qA = NULL;
	while_active_db_safe(currentA, qA)
	{
		currentP = NULL;
		qP = NULL;
		/* and all the processes */
		while_processes_safe(currentP, qP, currentA)
		{
			current_pipe = NULL;

			/* check if this fd is a pipe bound to a process */
			while_pipes(current_pipe, currentP)
			{
				/* if a bufered output pipe is found . */
				if (current_pipe->dir == BUFFERED_OUT_PIPE
					&& current_pipe->pipe[0] > 2
					&& FD_ISSET(current_pipe->pipe[0], &readset))
				{
					D_("BUFFERED_OUT_PIPE: Will read from %s->start_process on fd #%i\n", currentA->name, current_pipe->pipe[0]);

					/* Do the actual read from pipe */
					initng_fd_process_read_input(currentA, currentP,
												 current_pipe);

					/* Found match, that means we need to look for one less, if we've found all we should then return */
					retval--;
					if (retval == 0)
						return;
				}

				if (current_pipe->dir == OUT_PIPE &&
					current_pipe->pipe[0] > 2 &&
					FD_ISSET(current_pipe->pipe[0], &readset))
				{
					initng_fd_pipe(currentA, currentP, current_pipe);

					/* Found match, that means we need to look for one less, if we've found all we should then return */
					retval--;
					if (retval == 0)
						return;
				}

				if (current_pipe->dir == IN_AND_OUT_PIPE &&
					current_pipe->pipe[1] > 2 &&
					FD_ISSET(current_pipe->pipe[1], &readset))
				{
					initng_fd_pipe(currentA, currentP, current_pipe);

					/* Found match, that means we need to look for one less, if we've found all we should then return */
					retval--;
					if (retval == 0)
						return;
				}

				if (current_pipe->dir == IN_PIPE &&
					current_pipe->pipe[1] > 2 &&
					FD_ISSET(current_pipe->pipe[1], &writeset))
				{
					initng_fd_pipe(currentA, currentP, current_pipe);

					/* Found match, that means we need to look for one less, if we've found all we should then return */
					retval--;
					if (retval == 0)
						return;
				}

			}
		}
	}

	if (retval != 0)
	{
		F_("There is a missed pipe or fd that we missed to poll!\n");
	}

	return;
}

int initng_fd_set_cloexec(int fd)
{
	return fcntl(fd, F_SETFD, fcntl(fd, F_GETFD, 0) | FD_CLOEXEC);
}
