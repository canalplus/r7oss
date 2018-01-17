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

#include <stdio.h>
#include <stdlib.h>							/* free() exit() */
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <assert.h>

#include <initng_handler.h>
#include <initng_global.h>
#include <initng_common.h>
#include <initng_toolbox.h>
#include <initng_static_data_id.h>
#include <initng_static_states.h>
#include <initng_static_event_types.h>
#include <initng_event_hook.h>

INITNG_PLUGIN_MACRO;

s_entry WAIT_FOR_FILE = { "wait_for_file", STRINGS, NULL,
	"Check so that this files exits before launching."
};

s_entry REQUIRE_FILE = { "require_file", STRINGS, NULL,
	"If this file dont exist, this service will FAIL directly."
};

s_entry WAIT_FOR_FILE_AFTER = { "wait_for_file_after", STRINGS, NULL,
	"Make sure this files exits before a service can be marked as up."
};

s_entry REQUIRE_FILE_AFTER = { "require_file_after", STRINGS, NULL,
	"If this file dont exist after, the service will be marked FAIL."
};

a_state_h REQUIRE_FILE_FAILED = { "REQUIRE_FILE_FAILED", "A file that is required to exist before this service can start was not found.", IS_FAILED, NULL, NULL, NULL };
a_state_h REQUIRE_FILE_AFTER_FAILED = { "REQUIRE_FILE_AFTER_FAILED", "A file that is required to exist after this service is started was not found.", IS_FAILED, NULL, NULL, NULL };



/*
 * Do this check before START_DEP_MET can be set.
 */
static int check_files_to_exist(s_event * event)
{
	active_db_h * service;
	const char *file = NULL;
	s_data *itt = NULL;
	struct stat file_stat;

	assert(event->event_type == &EVENT_START_DEP_MET);
	assert(event->data);

	service = event->data;

	/* CHECK WAIT_FOR_FILE */
	while ((file = get_next_string(&WAIT_FOR_FILE, service, &itt)))
	{
		D_("Service %s need file %s to exist\n", service->name, file);
		if (stat(file, &file_stat) != 0)
		{
			D_("File %s needed by %s doesn't exist.\n", file, service->name);
			/* set the alarm, make sure initng will search files, in one second */
			initng_global_set_sleep(1);

			/* don't change status of service to START_DEP_MET */
			return (FAIL);
		}
	}

	/* CHECK REQUIRE_FILE */
	while ((file = get_next_string(&REQUIRE_FILE, service, &itt)))
	{
		D_("Service %s need file %s to exist\n", service->name, file);
		if (stat(file, &file_stat) != 0)
		{
			initng_common_mark_service(service, &REQUIRE_FILE_FAILED);
			return (FAIL);
		}
	}

	return (TRUE);
}

/*
 * Do this test before status RUNNING can be set.
 */
static int check_files_to_exist_after(s_event * event)
{
	active_db_h * service;
	const char *file = NULL;
	s_data *itt = NULL;

	struct stat file_stat;

	assert(event->event_type == &EVENT_UP_MET);
	assert(event->data);

	service = event->data;

	/* check WAIT_FOR_FILE_AFTER */
	while ((file = get_next_string(&WAIT_FOR_FILE_AFTER, service, &itt)))
	{
		D_("Service %s need file %s before it can be set to RUNNING\n",
		   service->name, file);
		if (stat(file, &file_stat) != 0)
		{
			D_("File %s needed by %s doesn't exist.\n", file, service->name);
			/* set the alarm, make sure initng will search files, in one second */
			initng_global_set_sleep(1);

			/* don't change status of service to START_READY */
			return (FAIL);
		}
	}

	/* check REQUIRE_FILE_AFTER */
	while ((file = get_next_string(&WAIT_FOR_FILE_AFTER, service, &itt)))
	{
		if (stat(file, &file_stat) != 0)
		{
			initng_common_mark_service(service, &REQUIRE_FILE_AFTER_FAILED);
			return (FAIL);
		}
	}

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

	initng_service_data_type_register(&WAIT_FOR_FILE);
	initng_service_data_type_register(&REQUIRE_FILE);
	initng_service_data_type_register(&WAIT_FOR_FILE_AFTER);
	initng_service_data_type_register(&REQUIRE_FILE_AFTER);

	initng_active_state_register(&REQUIRE_FILE_FAILED);
	initng_active_state_register(&REQUIRE_FILE_AFTER_FAILED);

	initng_event_hook_register(&EVENT_START_DEP_MET, &check_files_to_exist);
	initng_event_hook_register(&EVENT_UP_MET, &check_files_to_exist_after);
	return (TRUE);
}

void module_unload(void)
{
	S_;
	initng_service_data_type_unregister(&WAIT_FOR_FILE);
	initng_service_data_type_unregister(&REQUIRE_FILE);
	initng_service_data_type_unregister(&WAIT_FOR_FILE_AFTER);
	initng_service_data_type_unregister(&REQUIRE_FILE_AFTER);

	initng_active_state_unregister(&REQUIRE_FILE_FAILED);
	initng_active_state_unregister(&REQUIRE_FILE_AFTER_FAILED);

	initng_event_hook_unregister(&EVENT_START_DEP_MET, &check_files_to_exist);
	initng_event_hook_unregister(&EVENT_UP_MET, &check_files_to_exist_after);
}
