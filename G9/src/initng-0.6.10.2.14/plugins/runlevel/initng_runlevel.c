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
#include <assert.h>
/*#include <time.h> */

#include <initng_handler.h>
#include <initng_global.h>
#include <initng_common.h>
#include <initng_toolbox.h>
#include <initng_static_data_id.h>
#include <initng_static_states.h>
#include <initng_static_service_types.h>
#include <initng_depend.h>
#include <initng_execute.h>

INITNG_PLUGIN_MACRO;

/*
 * ############################################################################
 * #                         STYPE HANDLERS FUNCTION DEFINES                  #
 * ############################################################################
 */
static int start_RUNLEVEL(active_db_h * service_to_start);
static int stop_RUNLEVEL(active_db_h * service);

/*
 * ############################################################################
 * #                        STATE HANDLERS FUNCTION DEFINES                   #
 * ############################################################################
 */
static void init_RUNLEVEL_START_MARKED(active_db_h * service);
static void init_RUNLEVEL_STOP_MARKED(active_db_h * service);
static void handle_RUNLEVEL_WAITING_FOR_START_DEP(active_db_h * service);
static void handle_RUNLEVEL_WAITING_FOR_STOP_DEP(active_db_h * service);


/*
 * ############################################################################
 * #                     Official SERVICE_TYPE STRUCT                         #
 * ############################################################################
 */
stype_h TYPE_RUNLEVEL = { "runlevel", "A runlevel contains a set of services that shud be running on the system.", FALSE, &start_RUNLEVEL, &stop_RUNLEVEL, NULL };
stype_h TYPE_VIRTUAL = { "virtual", "A virtual is a virtual set of services with an unik group name.", TRUE, &start_RUNLEVEL, &stop_RUNLEVEL, NULL };


/*
 * ############################################################################
 * #                         SERVICE STATES STRUCTS                           #
 * ############################################################################
 */

/*
 * When we want to start a service, it is first RUNLEVEL_START_MARKED
 */
a_state_h RUNLEVEL_START_MARKED = { "START_MARKED", "This runlevel is marked to be started.", IS_STARTING, NULL, &init_RUNLEVEL_START_MARKED, NULL };

/*
 * When we want to stop a SERVICE_DONE service, its marked RUNLEVEL_STOP_MARKED
 */
a_state_h RUNLEVEL_STOP_MARKED = { "STOP_MARKED", "This runlevel is marked to be stopped.", IS_STOPPING, NULL, &init_RUNLEVEL_STOP_MARKED, NULL };

/*
 * When a service is UP, it is marked as RUNLEVEL_UP
 */
a_state_h RUNLEVEL_UP = { "UP", "This runlevel is UP.", IS_UP, NULL, NULL, NULL };

/*
 * When services needed by current one is starting, current service is set RUNLEVEL_WAITING_FOR_START_DEP
 */
a_state_h RUNLEVEL_WAITING_FOR_START_DEP = { "WAITING_FOR_START_DEP", "Waiting for all services in this runlevel to start.", IS_STARTING,
	&handle_RUNLEVEL_WAITING_FOR_START_DEP, NULL, NULL
};

/*
 * When services needed to stop, before this is stopped is stopping, current service is set RUNLEVEL_WAITING_FOR_STOP_DEP
 */
a_state_h RUNLEVEL_WAITING_FOR_STOP_DEP = { "WAITING_FOR_STOP_DEP", "Waiting for all services depending on this runlevel to stop, before the runlevel can be stopped.", IS_STOPPING,
	&handle_RUNLEVEL_WAITING_FOR_STOP_DEP, NULL, NULL
};

/*
 * This marks the services, as DOWN.
 */
a_state_h RUNLEVEL_DOWN = { "DOWN", "Runlevel is not currently active.", IS_DOWN, NULL, NULL, NULL };

/*
 * Generally FAILING states, if something goes wrong, some of these are set.
 */
a_state_h RUNLEVEL_START_DEPS_FAILED = { "START_DEPS_FAILED", "Some services in this runlevel failed", IS_FAILED, NULL, NULL, NULL };


/*
 * ############################################################################
 * #                         STYPE HANDLERS FUNCTIONS                         #
 * ############################################################################
 */


/* This are run, when initng wants to start a service */
static int start_RUNLEVEL(active_db_h * service)
{

	/* if not yet stopped */
	if (IS_MARK(service, &RUNLEVEL_WAITING_FOR_STOP_DEP))
	{
		initng_common_mark_service(service, &RUNLEVEL_UP);
		return (TRUE);
	}

	/* mark it WAITING_FOR_START_DEP and wait */
	if (!initng_common_mark_service(service, &RUNLEVEL_START_MARKED))
	{
		W_("mark_service RUNLEVEL_START_MARKED failed for service %s\n",
		   service->name);
		return (FALSE);
	}

	/* return happily */
	return (TRUE);
}


/* This are run, when initng wants to stop a service */
static int stop_RUNLEVEL(active_db_h * service)
{

	/* if not yet stopped */
	if (IS_MARK(service, &RUNLEVEL_WAITING_FOR_START_DEP))
	{
		initng_common_mark_service(service, &RUNLEVEL_DOWN);
		return (TRUE);
	}

	/* set stopping */
	if (!initng_common_mark_service(service, &RUNLEVEL_STOP_MARKED))
	{
		W_("mark_service RUNLEVEL_STOP_MARKED failed for service %s.\n",
		   service->name);
		return (FALSE);
	}

	/* return happily */
	return (TRUE);
}

/*
 * ############################################################################
 * #                         PLUGIN INITIATORS                               #
 * ############################################################################
 */

int module_init(int api_version)
{
	D_("module_init();\n");
	if (api_version != API_VERSION)
	{
		F_("This module is compiled for api_version %i version and initng is compiled on %i version, won't load this module!\n", API_VERSION, api_version);
		return (FALSE);
	}

	initng_service_type_register(&TYPE_RUNLEVEL);
	initng_service_type_register(&TYPE_VIRTUAL);


	initng_active_state_register(&RUNLEVEL_START_MARKED);
	initng_active_state_register(&RUNLEVEL_STOP_MARKED);
	initng_active_state_register(&RUNLEVEL_UP);
	initng_active_state_register(&RUNLEVEL_WAITING_FOR_START_DEP);
	initng_active_state_register(&RUNLEVEL_WAITING_FOR_STOP_DEP);
	initng_active_state_register(&RUNLEVEL_DOWN);
	initng_active_state_register(&RUNLEVEL_START_DEPS_FAILED);

	return (TRUE);
}

void module_unload(void)
{
	D_("module_unload();\n");

	initng_service_type_unregister(&TYPE_RUNLEVEL);
	initng_service_type_unregister(&TYPE_VIRTUAL);


	initng_active_state_unregister(&RUNLEVEL_START_MARKED);
	initng_active_state_unregister(&RUNLEVEL_STOP_MARKED);
	initng_active_state_unregister(&RUNLEVEL_UP);
	initng_active_state_unregister(&RUNLEVEL_WAITING_FOR_START_DEP);
	initng_active_state_unregister(&RUNLEVEL_WAITING_FOR_STOP_DEP);
	initng_active_state_unregister(&RUNLEVEL_DOWN);
	initng_active_state_unregister(&RUNLEVEL_START_DEPS_FAILED);
}

/*
 * ############################################################################
 * #                         STATE_FUNCTIONS                                  #
 * ############################################################################
 */


/*
 * Everything RUNLEVEL_START_MARKED are gonna do, is to set it RUNLEVEL_WAITING_FOR_START_DEP
 */
static void init_RUNLEVEL_START_MARKED(active_db_h * new_runlevel)
{
	/* Start our dependencies */
	initng_depend_start_deps(new_runlevel);


	/* Make sure there will only exist 1 runlevel on the system */
	if (new_runlevel->type == &TYPE_RUNLEVEL)
	{
		active_db_h *current = NULL;
		active_db_h *old_runlevel = NULL;

		/* STEP 1, Go find old runlevel, shud be only one */
		while_active_db(current)
		{
			/* dont look for myself */
			if (current == new_runlevel)
				continue;

			if (current->type == &TYPE_RUNLEVEL  && current->current_state != &RUNLEVEL_DOWN)
			{
				old_runlevel = current;
				break;
			}
		}

		/* if an old runlevel was found */
		if (old_runlevel)
		{
			const char *dep_old = NULL;
			s_data *itt_old = NULL;

			/* STEP 2, Stop all old runlevel deps, that are not deps of new runlevel */

			/* for every dep the old runlevel have */
			while ((dep_old = get_next_string(&NEED, old_runlevel, &itt_old)))
			{
				const char *dep_new = NULL;
				s_data *itt_new = NULL;
				int found = 0;

				while ((dep_new =
						get_next_string(&NEED, new_runlevel, &itt_new)))
				{
					/* if it matches */
					if (strcmp(dep_new, dep_old) == 0)
					{
						found = 1;
						break;
					}
				}


				/* If the service found in old runlevel, does not exsist i new one, stop it */
				if (found == 0)
				{
					active_db_h *service_to_stop = initng_active_db_find_by_name(dep_old);

					if (service_to_stop)
					{
						W_("Stopping service %s, not in new service %s\n",
						   service_to_stop->name, new_runlevel->name);
						initng_handler_stop_service(service_to_stop);
					}
				}
			}
			/* check that it also exists in new runlevel */


			/* STEP 3, Stop old runlevel */
			initng_handler_stop_service(old_runlevel);
		}
		free(g.runlevel);
		g.runlevel = NULL;
		g.runlevel = i_strdup(new_runlevel->name);
	}

	initng_common_mark_service(new_runlevel, &RUNLEVEL_WAITING_FOR_START_DEP);
}

/*
 * Everything RUNLEVEL_STOP_MARKED are gonna do, is to set it RUNLEVEL_WAITING_FOR_STOP_DEP
 */
static void init_RUNLEVEL_STOP_MARKED(active_db_h * service)
{
	/* Stop all services dependeing on this service */
	initng_depend_stop_deps(service);

	initng_common_mark_service(service, &RUNLEVEL_WAITING_FOR_STOP_DEP);
}

static void handle_RUNLEVEL_WAITING_FOR_START_DEP(active_db_h * service)
{
	assert(service);

	/* this checks with external plug-ins, if its ok to start this service now */

	switch (initng_depend_start_dep_met(service, FALSE))
	{
			/* if not met, youst return */
		case FALSE:
			return;


			/* set FAILURE */
		case FAIL:
			initng_common_mark_service(service, &RUNLEVEL_START_DEPS_FAILED);
			return;

			/* if met, continue */
		case TRUE:
		default:
			break;
	}

	/* set status to START_DEP_MET */
	initng_common_mark_service(service, &RUNLEVEL_UP);

}

static void handle_RUNLEVEL_WAITING_FOR_STOP_DEP(active_db_h * service)
{
	assert(service);

	/* check with other plug-ins, if it is ok to stop this service now */
	switch (initng_depend_stop_dep_met(service, FALSE))
	{
			/* deps not met, youst return */
		case FALSE:
			return;

			/* if met, youst continue */
		case TRUE:
		default:
			break;

	}

	/* ok, stopping deps are met */
	initng_common_mark_service(service, &RUNLEVEL_DOWN);
}
