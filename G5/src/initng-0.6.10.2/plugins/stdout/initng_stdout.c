/*
 * Initng, a next generation sysvinit replacement.
 * Copyright (C) 2006 Jimmy Wennlund <jimmy.wennlund@gmail.com>
 * Copyright (C) 2005 neuron <neuron@hollowtube.mine.nu>
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

#include <initng.h>


#include <stdio.h>
#include <stdlib.h>							/* free() exit() */
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <assert.h>

#include <initng_handler.h>
#include <initng_global.h>
#include <initng_common.h>
#include <initng_toolbox.h>
#include <initng_env_variable.h>
#include <initng_event_hook.h>
#include <initng_static_event_types.h>
#include <initng_fd.h>


INITNG_PLUGIN_MACRO;

s_entry STDOUT = { "stdout", STRING, NULL,
	"Open a file with this path, and direct service output here."
};
s_entry STDERR = { "stderr", STRING, NULL,
	"Open a file with this path, and direct service error output here."
};
s_entry STDALL = { "stdall", STRING, NULL,
	"Open a file with this path, and direct service all output here."
};
s_entry STDIN = { "stdin", STRING, NULL,
	"Open a file with this path, and direct service input here."
};



static int setup_output(s_event * event)
{
	s_event_after_fork_data * data;

	/* string containing the filename of output */
	const char *s_stdout = NULL;
	const char *s_stderr = NULL;
	const char *s_stdall = NULL;
	const char *s_stdin = NULL;

	/* string containing the variable parser output */
	char *f_stdout = NULL;
	char *f_stderr = NULL;
	char *f_stdall = NULL;
	char *f_stdin = NULL;

	/* file descriptors */
	int fd_stdout = -1;
	int fd_stderr = -1;
	int fd_stdall = -1;
	int fd_stdin = -1;

	assert(event->event_type == &EVENT_AFTER_FORK);
	assert(event->data);

	data = event->data;

	/* assert some */
	assert(data->service);
	assert(data->service->name);
	assert(data->process);

	S_;

	/* get strings if present */
	s_stdout = get_string(&STDOUT, data->service);
	s_stderr = get_string(&STDERR, data->service);
	s_stdall = get_string(&STDALL, data->service);
	s_stdin = get_string(&STDIN, data->service);

	if (!(s_stdout || s_stderr || s_stdall || s_stdin))
	{
		D_("This plugin won't do anything, because no opt set!\n");
		return (TRUE);
	}

	/* if stdout points to same as stderr, set s_stdall */
	if (s_stdout && s_stderr && strcmp(s_stdout, s_stderr) == 0)
	{
		s_stdall = s_stdout;
		s_stdout = NULL;
		s_stderr = NULL;
	}


	/* fix variables */
	if (s_stdout)
		f_stdout = fix_variables(s_stdout, data->service);
	if (s_stderr)
		f_stderr = fix_variables(s_stderr, data->service);
	if (s_stdall)
		f_stdall = fix_variables(s_stdall, data->service);
	if (s_stdin)
		f_stdin = fix_variables(s_stdin, data->service);


	/* if stdall string is set */
	if (f_stdall)
	{
		/* output all to this */
		fd_stdall = open(f_stdall, O_WRONLY | O_NOCTTY | O_CREAT | O_APPEND,
						 0644);
	}
	else
	{
		/* else set them to different files */
		if (f_stdout)
			fd_stdout = open(f_stdout,
							 O_WRONLY | O_NOCTTY | O_CREAT | O_APPEND, 0644);
		if (f_stderr)
			fd_stderr = open(f_stderr,
							 O_WRONLY | O_NOCTTY | O_CREAT | O_APPEND, 0644);
	}
	if (f_stdin)
		fd_stdin = open(f_stdin, O_RDONLY | O_NOCTTY, 0644);

	/* print fail messages, if the files did not open */
	if (f_stdall && fd_stdall < 2)
		F_("StdALL: %s, fd %i\n", f_stdall, fd_stdall);
	if (f_stdout && fd_stdout < 2)
		F_("StdOUT: %s, fd %i\n", f_stdout, fd_stdout);
	if (f_stderr && fd_stderr < 2)
		F_("StdERR: %s, fd %i\n", f_stderr, fd_stderr);
	if (f_stdin && fd_stdin < 2)
		F_("StdIN: %s, fd %i\n", f_stdin, fd_stdin);

#ifdef DEBUG
	if (f_stdall)
		D_("StdALL: %s, fd %i\n", f_stdall, fd_stdall);
	if (f_stdout)
		D_("StdOUT: %s, fd %i\n", f_stdout, fd_stdout);
	if (f_stderr)
		D_("StdERR: %s, fd %i\n", f_stderr, fd_stderr);
	if (f_stdin)
		D_("StdIN:  %s, fd %i\n", f_stdin, fd_stdin);
#endif

	/* if fd succeeded to open */
	if (fd_stdall > 2)
	{
		/* dup both to stdall */
		dup2(fd_stdall, 1);
		dup2(fd_stdall, 2);

		initng_fd_set_cloexec(fd_stdall);
	}
	else
	{
		/* else dup them to diffrent fds */
		if (fd_stdout > 2)
			dup2(fd_stdout, 1);
		if (fd_stderr > 2)
			dup2(fd_stderr, 2);

		initng_fd_set_cloexec(fd_stdout);
		initng_fd_set_cloexec(fd_stderr);
	}

	if (fd_stdin > 2) {
		dup2(fd_stdin, 0);
		initng_fd_set_cloexec(fd_stdin);
	}

	/* free the (by fix_variables) duped variables */
	fix_free(f_stdout, s_stdout);
	fix_free(f_stderr, s_stderr);
	fix_free(f_stdall, s_stdall);
	fix_free(f_stdin, s_stdin);

	return (TRUE);
}


int module_init(int api_version)
{
	S_;

	D_("module_init();\n");
	if (api_version != API_VERSION)
	{
		F_("This module is compiled for api_version %i version and initng is compiled on %i version, won't load this module!\n", API_VERSION, api_version);
		return (FALSE);
	}

	initng_service_data_type_register(&STDOUT);
	initng_service_data_type_register(&STDERR);
	initng_service_data_type_register(&STDALL);
	initng_service_data_type_register(&STDIN);

	initng_event_hook_register(&EVENT_AFTER_FORK, &setup_output);
	return (TRUE);
}

void module_unload(void)
{
	S_;

	D_("module_unload();\n");

	initng_service_data_type_unregister(&STDOUT);
	initng_service_data_type_unregister(&STDERR);
	initng_service_data_type_unregister(&STDALL);
	initng_service_data_type_unregister(&STDIN);

	initng_event_hook_unregister(&EVENT_AFTER_FORK, &setup_output);
}
