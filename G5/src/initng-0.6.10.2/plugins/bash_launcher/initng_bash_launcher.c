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

#include <initng.h>

#include <time.h>							/* time() */
#include <fcntl.h>							/* fcntl() */
#include <unistd.h>							/* execv() usleep() pause() chown() */
#include <sys/wait.h>						/* waitpid() sa */
#include <sys/ioctl.h>						/* ioctl() */
#include <stdlib.h>							/* free() exit() */
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <pwd.h>
#include <sys/stat.h>
#include <errno.h>

#ifdef BUSYBOX
#include "../../busybox-1.00/include/busybox.h"
#include "../../busybox-1.00/include/libbb.h"
#endif

#include <initng_active_db.h>
#include <initng_global.h>
#include <initng_plugin_callers.h>
#include <initng_execute.h>
#include <initng_string_tools.h>
#include <initng_toolbox.h>
#include <initng_signal.h>
#include <initng_fork.h>
#include <initng_env_variable.h>
#include <initng_static_event_types.h>
#include <initng_event_hook.h>

INITNG_PLUGIN_MACRO;

s_entry SCRIPT = { "script", VARIABLE_STRING, NULL,
	"A shell script, inserted into a multiline variable."
};
s_entry SCRIPT_OPT = { "script_opt", VARIABLE_STRING, NULL,
	"The options bash should get."
};

static void bash_this(const char *bash_code, active_db_h * s,
					  const char *args);
static int bash_exec(process_h * process_to_exec, active_db_h * s,
					 const char *script, const char *args);


static int initng_bash(s_event * event)
{
	s_event_launch_data * data;
	const char *e = NULL;
	const char *args = NULL;

	assert(event->event_type == &EVENT_LAUNCH);
	assert(event->data);

	data = event->data;

	assert(data->service);
	assert(data->service->name);
	assert(data->exec_name);
	assert(data->process);

	/* WE ARE EXECUTING A START MULTILINE_STRING */
	if (!(e = get_string_var(&SCRIPT, data->exec_name, data->service)))
		return (FALSE);

	/* get the arguments if any */
	args = get_string_var(&SCRIPT_OPT, data->exec_name, data->service);

	/*D_("initng_bash(%s, %s, %s);\n", data->service->name, e, args); */
	if (bash_exec(data->process, data->service, e, args))
		return (HANDLED);
	else
		return (FALSE);
}


static void bash_this(const char *bash_code, active_db_h * s,
					  const char *args)
{
	/* temporary argv */
	char **argtmp;

	D_("bash_this(%s);\n", s->name);

	/* Todo : move bash_this into initng_bash_launcher and parse args properly\n" */

	int i = 0;
	/* allocate, and fill the temp argv */
	argtmp = (char **) i_calloc(8, sizeof(char *));
	argtmp[i] = (char *) i_calloc(1, sizeof(char) * (15 + strlen(s->name)));
	strcpy(argtmp[i++], "/bin/sh");
	if (args)
		argtmp[i++] = i_strdup(args);
	argtmp[i++] = i_strdup("-c");
	argtmp[i++] = i_strdup(bash_code);
	/* Why do we add service name as argument to shell?!?
	   argtmp[3] = i_strdup(s->name);
	 */

	argtmp[i++] = NULL;

#if 0
	fprintf(stderr, ">>exec: exec %s {\n", "/bin/sh");
	int k;
	for(k = 0; k < 5; k++) {
		fprintf(stderr, ">>exec: %s\n", argtmp[k]);
	}
	fprintf(stderr, ">>exec: }\n");
#endif

	/* execute */
	execve("/bin/sh", argtmp, new_environ(s));

	/* free them all */
	{
		int i = 0;

		while (argtmp[i])
		{
			free(argtmp[i]);
			argtmp[i] = NULL;
			i++;
		}
	}
	free(argtmp);

	/* put an error message up */

	F_("bash_this(): child died!\n ERROR!\n");

	/* system free */
	initng_global_free();

	/* exit 1, to emit an false signal */
	_exit(1);
}											/* end fork_and_exec() */



static int bash_exec(process_h * process_to_exec, active_db_h * s,
					 const char *script, const char *args)
{

	/* called from inside the service directory, return the PID or 0 on error */

	/* This is the real service kicker */
	pid_t pid_fork;				/* pid got from fork() */

	assert(process_to_exec);
	assert(script);

	if ((pid_fork = initng_fork(s, process_to_exec)) == 0)
	{
		/* run afterfork hooks from other plugins */
		initng_fork_aforkhooks(s, process_to_exec);

		/* execute code */
		bash_this(script, s, args);

		/* Bash This should NOT return! */
		_exit(33);
	}

	/* save pid of fork */
	D_("FROM_FORK Forkstarted pid %i.\n", pid_fork);

	if (pid_fork > 1)
		return (TRUE);

	F_("bash_exec, did not get a pid!\n");
	process_to_exec->pid = 0;
	return (FALSE);

}
int module_init(int api_version)
{
	D_("initng_simple_plugin: module_init();\n");
	if (api_version != API_VERSION)
	{
		F_("This module is compiled for api_version %i version and initng is compiled on %i version, won't load this module!\n", API_VERSION, api_version);
		return (FALSE);
	}

	initng_service_data_type_register(&SCRIPT);
	initng_service_data_type_register(&SCRIPT_OPT);

	initng_event_hook_register(&EVENT_LAUNCH, &initng_bash);
	return (TRUE);
}

void module_unload(void)
{
	initng_service_data_type_unregister(&SCRIPT);
	initng_service_data_type_unregister(&SCRIPT_OPT);

	D_("initng_simple_plugin: module_unload();\n");
	initng_event_hook_unregister(&EVENT_LAUNCH, &initng_bash);
}
