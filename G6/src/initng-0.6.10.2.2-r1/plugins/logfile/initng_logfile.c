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
#include <initng_static_event_types.h>
#include <initng_event_hook.h>

INITNG_PLUGIN_MACRO;

s_entry LOGFILE = { "logfile", STRING, NULL, "An extra output of service output." };

static int program_output(s_event * event)
{
	s_event_buffer_watcher_data * data;
	const char *filename = NULL;
	char *filename_fixed = NULL;
	int len = 0;
	int fd = -1;

	assert(event->event_type == &EVENT_BUFFER_WATCHER);
	assert(event->data);

	data = event->data;

	assert(data->service);
	assert(data->service->name);
	assert(data->process);

	/*D_("%s process fd: # %i, %i, service %s, have something to say\n",
	   data->process->pt->name, data->process->out_pipe[0], data->process->out_pipe[1], data->service->name); */

	/* get the filename */
	filename = get_string(&LOGFILE, data->service);
	if (!filename)
	{
		D_("Logfile not set\n");
		return (FALSE);
	}

	/* Fix $variables in filename string */
	filename_fixed = fix_variables(filename, data->service);

	/* open the file */
	fd = open(filename, O_WRONLY | O_CREAT | O_APPEND, 0600);
	if (fd < 1)
	{
		F_("Error opening %s, err : %s\n", filename, strerror(errno));
		return (FALSE);
	}

	/* Write data to logfile */
	D_("Writing data...\n");
	len = strlen(data->buffer_pos);

	if (write(fd, data->buffer_pos, len) != len)
		F_("Error writing to %s's log, err : %s\n", data->service->name,
		   strerror(errno));

	fix_free(filename_fixed, filename);
	close(fd);

	return (TRUE);
}

int module_init(int api_version)
{
	S_;

	if (api_version != API_VERSION)
	{
		F_("This module is compiled for api_version %i version and initng is compiled on %i version, won't load this module!\n", API_VERSION, api_version);
		return (FALSE);
	}

	initng_service_data_type_register(&LOGFILE);

	initng_event_hook_register(&EVENT_BUFFER_WATCHER, &program_output);
	return (TRUE);
}

void module_unload(void)
{
	S_;

	D_("module_unload();\n");

	initng_service_data_type_unregister(&LOGFILE);

	initng_event_hook_unregister(&EVENT_BUFFER_WATCHER, &program_output);
}
