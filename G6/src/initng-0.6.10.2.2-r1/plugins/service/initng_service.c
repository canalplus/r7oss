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
#include <sys/types.h>
#include <signal.h>
#include <errno.h>
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
static int start_SERVICE(active_db_h * service_to_start);
static int stop_SERVICE(active_db_h * service);

/*
 * ############################################################################
 * #                        STATE HANDLERS FUNCTION DEFINES                   #
 * ############################################################################
 */
static void handle_SERVICE_WAITING_FOR_START_DEP(active_db_h * service);
static void handle_SERVICE_WAITING_FOR_STOP_DEP(active_db_h * service);

static void init_SERVICE_START_DEPS_MET(active_db_h * service);
static void init_SERVICE_STOP_DEPS_MET(active_db_h * service);
static void init_SERVICE_START_MARKED(active_db_h * service);
static void init_SERVICE_STOP_MARKED(active_db_h * service);


static void timeout_SERVICE_START_RUN(active_db_h * service);
static void init_SERVICE_START_RUN(active_db_h * service);
static void timeout_SERVICE_STOP_RUN(active_db_h * service);
static void init_SERVICE_STOP_RUN(active_db_h * service);

/*
 * ############################################################################
 * #                        PROCESS TYPE FUNCTION DEFINES                     #
 * ############################################################################
 */
static void handle_killed_start(active_db_h * killed_service,
								process_h * process);
static void handle_killed_stop(active_db_h * killed_service,
							   process_h * process);


/*
 * ############################################################################
 * #                     Official SERVICE_TYPE STRUCT                         #
 * ############################################################################
 */
stype_h TYPE_SERVICE = { "service", "The start code will run on start, and stop code when stopping.", FALSE, &start_SERVICE, &stop_SERVICE, NULL };

/*
 * ############################################################################
 * #                       PROCESS TYPES STRUCTS                              #
 * ############################################################################
 */
ptype_h T_START = { "start", &handle_killed_start };
ptype_h T_STOP = { "stop", &handle_killed_stop };

/*
 * ############################################################################
 * #                       SERVICE VARIABLES                                  #
 * ############################################################################
 */

#define DEFAULT_START_TIMEOUT 240
#define DEFAULT_STOP_TIMEOUT 60
s_entry START_TIMEOUT = { "start_timeout", INT, &TYPE_SERVICE,
	"Let the start process run maximum this time."
};
s_entry STOP_TIMEOUT = { "stop_timeout", INT, &TYPE_SERVICE,
	"Let the stop process run maximum this time."
};
s_entry NEVER_KILL = { "never_kill", SET, &TYPE_SERVICE,
	"This service is to important to be killed by any timeout!"
};
s_entry START_FAIL_OK = { "start_fail_ok", SET, &TYPE_SERVICE,
	"Set this service to a succes, even if it returns bad on start service."
};
s_entry STOP_FAIL_OK = { "stop_fail_ok", SET, &TYPE_SERVICE,
	"Set this service to stopped even if it returns bad on stop service."
};

/*
 * ############################################################################
 * #                         SERVICE STATES STRUCTS                           #
 * ############################################################################
 */

/*
 * When we want to start a service, it is first SERVICE_START_MARKED
 */
a_state_h SERVICE_START_MARKED = { "SERVICE_START_MARKED", "This service is marked for starting.", IS_STARTING, NULL, &init_SERVICE_START_MARKED,
	NULL
};

/*
 * When we want to stop a SERVICE_DONE service, its marked SERVICE_STOP_MARKED
 */
a_state_h SERVICE_STOP_MARKED = { "SERVICE_STOP_MARKED", "This service is marked for stopping.", IS_STOPPING, NULL, &init_SERVICE_STOP_MARKED,
	NULL
};

/*
 * When a service is UP, it is marked as SERVICE_DONE
 */
a_state_h SERVICE_DONE = { "SERVICE_DONE", "The start code has successfully returned, the service is marked as UP.", IS_UP, NULL, NULL, NULL };

/*
 * When services needed by current one is starting, current service is set SERVICE_WAITING_FOR_START_DEP
 */
a_state_h SERVICE_WAITING_FOR_START_DEP = { "SERVICE_WAITING_FOR_START_DEP", "This service is waiting for all its depdencencies to be met.", IS_STARTING,
	&handle_SERVICE_WAITING_FOR_START_DEP, NULL, NULL
};

/*
 * When services needed to stop, before this is stopped is stopping, current service is set SERVICE_WAITING_FOR_STOP_DEP
 */
a_state_h SERVICE_WAITING_FOR_STOP_DEP = { "SERVICE_WAITING_FOR_STOP_DEP", "This service is wating for all services depending it to stop, before stopping.", IS_STOPPING,
	&handle_SERVICE_WAITING_FOR_STOP_DEP, NULL, NULL
};

/*
 * This state is set, when all services needed to start, is started.
 */
a_state_h SERVICE_START_DEPS_MET = { "SERVICE_START_DEPS_MET", "The dependencies for starting this service are met.", IS_STARTING, NULL,
	&init_SERVICE_START_DEPS_MET, NULL
};

/*
 * This state is set, when all services needed top be stopped, before stopping, is stopped.
 */
a_state_h SERVICE_STOP_DEPS_MET = { "SERVICE_STOP_DEPS_MET", "The dependencies for stopping this services are met.", IS_STOPPING, NULL, &init_SERVICE_STOP_DEPS_MET,
	NULL
};

/*
 * This marks the services, as DOWN.
 */
a_state_h SERVICE_STOPPED = { "SERVICE_STOPPED", "The stop code has been returned, service is marked as DOWN.", IS_DOWN, NULL, NULL, NULL };

/*
 * This is the state, when the Start code is actually running.
 */
a_state_h SERVICE_START_RUN = { "SERVICE_START_RUN", "The start code is currently running on this service.", IS_STARTING, NULL, &init_SERVICE_START_RUN,
	&timeout_SERVICE_START_RUN
};

/*
 * This is the state, when the Stop code is actually running.
 */
a_state_h SERVICE_STOP_RUN = { "SERVICE_STOP_RUN", "The service stop code is being run.", IS_STOPPING, NULL, &init_SERVICE_STOP_RUN,
	&timeout_SERVICE_STOP_RUN
};

/*
 * Generally FAILING states, if something goes wrong, some of these are set.
 */

a_state_h SERVICE_START_DEPS_FAILED = { "SERVICE_START_DEPS_FAILED", "One of the depdencencies this service requires has failed, this service cant start.", IS_FAILED, NULL, NULL, NULL };
a_state_h SERVICE_FAIL_START_LAUNCH = { "SERVICE_FAIL_START_LAUNCH", "Service failed to launch start script/executable.", IS_FAILED, NULL, NULL, NULL };
a_state_h SERVICE_FAIL_START_NONEXIST = { "SERVICE_FAIL_START_NONEXIST", "There was no start code avaible for this service.", IS_FAILED, NULL, NULL, NULL };
a_state_h SERVICE_FAIL_START_RCODE = { "SERVICE_FAIL_START_RCODE", "The start code returned a non-zero value, this usually meens that the service failed", IS_FAILED, NULL, NULL, NULL };
a_state_h SERVICE_FAIL_START_SIGNAL = { "SERVICE_FAIL_START_SIGNAL", "The service start code segfaulted.", IS_FAILED, NULL, NULL, NULL };
a_state_h SERVICE_START_FAILED_TIMEOUT = { "SERVICE_START_FAILED_TIMEOUT", "Time out running the start code.", IS_FAILED, NULL, NULL, NULL };

a_state_h SERVICE_STOP_DEPS_FAILED = { "SERVICE_STOP_DEPS_FAILED", "Failed to stop one of the services depending on this service, cannot stop this service", IS_FAILED, NULL, NULL, NULL };
a_state_h SERVICE_FAIL_STOP_NONEXIST = { "SERVICE_FAIL_STOP_NONEXIST", "The stop code did not exist.", IS_FAILED, NULL, NULL, NULL };
a_state_h SERVICE_FAIL_STOP_RCODE = { "SERVICE_FAIL_STOP_RCODE", "The stop exec returned a non-zero code, this usually means a failure.", IS_FAILED, NULL, NULL, NULL };
a_state_h SERVICE_FAIL_STOP_SIGNAL = { "SERVICE_FAIL_STOP_SIGNAL", "The stop exec segfaulted.", IS_FAILED, NULL, NULL, NULL };
a_state_h SERVICE_STOP_FAILED_TIMEOUT = { "SERVICE_STOP_FAILED_TIMEOUT", "Timeout running the stop code.", IS_FAILED, NULL, NULL, NULL };

a_state_h SERVICE_UP_CHECK_FAILED = { "SERVICE_UP_CHECK_FAILED", "The checks that are done before the service can be set to UP failed", IS_FAILED, NULL, NULL, NULL };


/*
 * ############################################################################
 * #                         STYPE HANDLERS FUNCTIONS                         #
 * ############################################################################
 */


/* This are run, when initng wants to start a service */
static int start_SERVICE(active_db_h * service)
{

	/* if not stopped yet, reset DONE */
	if (IS_MARK(service, &SERVICE_WAITING_FOR_STOP_DEP))
	{
		initng_common_mark_service(service, &SERVICE_DONE);
		return (TRUE);
	}

	/* mark it WAITING_FOR_START_DEP and wait */
	if (!initng_common_mark_service(service, &SERVICE_START_MARKED))
	{
		W_("mark_service SERVICE_START_MARKED failed for service %s\n",
		   service->name);
		return (FALSE);
	}

	/* return happily */
	return (TRUE);
}


/* This are run, when initng wants to stop a service */
static int stop_SERVICE(active_db_h * service)
{
	/* if not started yet, reset STOPPED */
	if (IS_MARK(service, &SERVICE_WAITING_FOR_START_DEP))
	{
		initng_common_mark_service(service, &SERVICE_STOPPED);
		return (TRUE);
	}


	/* set stopping */
	if (!initng_common_mark_service(service, &SERVICE_STOP_MARKED))
	{
		W_("mark_service SERVICE_STOP_MARKED failed for service %s.\n",
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

	initng_service_type_register(&TYPE_SERVICE);

	initng_process_db_ptype_register(&T_START);
	initng_process_db_ptype_register(&T_STOP);

	initng_active_state_register(&SERVICE_START_MARKED);
	initng_active_state_register(&SERVICE_STOP_MARKED);
	initng_active_state_register(&SERVICE_DONE);
	initng_active_state_register(&SERVICE_WAITING_FOR_START_DEP);
	initng_active_state_register(&SERVICE_WAITING_FOR_STOP_DEP);
	initng_active_state_register(&SERVICE_START_DEPS_MET);
	initng_active_state_register(&SERVICE_STOP_DEPS_MET);
	initng_active_state_register(&SERVICE_STOPPED);
	initng_active_state_register(&SERVICE_START_RUN);
	initng_active_state_register(&SERVICE_STOP_RUN);
	initng_active_state_register(&SERVICE_START_DEPS_FAILED);
	initng_active_state_register(&SERVICE_STOP_DEPS_FAILED);

	/* FAIL states, may got while starting */
	initng_active_state_register(&SERVICE_FAIL_START_LAUNCH);
	initng_active_state_register(&SERVICE_FAIL_START_NONEXIST);
	initng_active_state_register(&SERVICE_FAIL_START_RCODE);
	initng_active_state_register(&SERVICE_FAIL_START_SIGNAL);

	initng_active_state_register(&SERVICE_START_FAILED_TIMEOUT);
	initng_active_state_register(&SERVICE_STOP_FAILED_TIMEOUT);
	initng_active_state_register(&SERVICE_FAIL_STOP_NONEXIST);
	initng_active_state_register(&SERVICE_UP_CHECK_FAILED);
	initng_active_state_register(&SERVICE_FAIL_STOP_RCODE);
	initng_active_state_register(&SERVICE_FAIL_STOP_SIGNAL);

	initng_service_data_type_register(&START_TIMEOUT);
	initng_service_data_type_register(&STOP_TIMEOUT);
	initng_service_data_type_register(&NEVER_KILL);
	initng_service_data_type_register(&START_FAIL_OK);
	initng_service_data_type_register(&STOP_FAIL_OK);

	return (TRUE);
}

void module_unload(void)
{
	D_("module_unload();\n");

	initng_service_type_unregister(&TYPE_SERVICE);

	initng_process_db_ptype_unregister(&T_START);
	initng_process_db_ptype_unregister(&T_STOP);


	initng_active_state_unregister(&SERVICE_START_MARKED);
	initng_active_state_unregister(&SERVICE_STOP_MARKED);
	initng_active_state_unregister(&SERVICE_DONE);
	initng_active_state_unregister(&SERVICE_WAITING_FOR_START_DEP);
	initng_active_state_unregister(&SERVICE_WAITING_FOR_STOP_DEP);
	initng_active_state_unregister(&SERVICE_START_DEPS_MET);
	initng_active_state_unregister(&SERVICE_STOP_DEPS_MET);
	initng_active_state_unregister(&SERVICE_STOPPED);
	initng_active_state_unregister(&SERVICE_START_RUN);
	initng_active_state_unregister(&SERVICE_STOP_RUN);
	initng_active_state_unregister(&SERVICE_START_DEPS_FAILED);
	initng_active_state_unregister(&SERVICE_STOP_DEPS_FAILED);

	/* fail states may got while starting */
	initng_active_state_unregister(&SERVICE_FAIL_START_LAUNCH);
	initng_active_state_unregister(&SERVICE_FAIL_START_NONEXIST);
	initng_active_state_unregister(&SERVICE_FAIL_START_RCODE);
	initng_active_state_unregister(&SERVICE_FAIL_START_SIGNAL);

	initng_active_state_unregister(&SERVICE_START_FAILED_TIMEOUT);
	initng_active_state_unregister(&SERVICE_STOP_FAILED_TIMEOUT);
	initng_active_state_unregister(&SERVICE_UP_CHECK_FAILED);
	initng_active_state_unregister(&SERVICE_FAIL_STOP_NONEXIST);
	initng_active_state_unregister(&SERVICE_FAIL_STOP_RCODE);
	initng_active_state_unregister(&SERVICE_FAIL_STOP_SIGNAL);

	initng_service_data_type_unregister(&START_TIMEOUT);
	initng_service_data_type_unregister(&STOP_TIMEOUT);
	initng_service_data_type_unregister(&NEVER_KILL);
	initng_service_data_type_unregister(&START_FAIL_OK);
	initng_service_data_type_unregister(&STOP_FAIL_OK);

}

/*
 * ############################################################################
 * #                         STATE_FUNCTIONS                                  #
 * ############################################################################
 */


/*
 * Everything SERVICE_START_MARKED are gonna do, is to set it SERVICE_WAITING_FOR_START_DEP
 */
static void init_SERVICE_START_MARKED(active_db_h * service)
{
	/* Start our dependencies */
	if (initng_depend_start_deps(service) != TRUE)
	{
		initng_common_mark_service(service, &SERVICE_START_DEPS_FAILED);
		return;
	}

	/* set WAITING_FOR_START_DEP */
	initng_common_mark_service(service, &SERVICE_WAITING_FOR_START_DEP);
}

/*
 * Everything SERVICE_STOP_MARKED are gonna do, is to set it SERVICE_WAITING_FOR_STOP_DEP
 */
static void init_SERVICE_STOP_MARKED(active_db_h * service)
{
	/* Stopp all services dependeing on this service */
	if (initng_depend_stop_deps(service) != TRUE)
	{
		initng_common_mark_service(service, &SERVICE_STOP_DEPS_FAILED);
		return;
	}

	initng_common_mark_service(service, &SERVICE_WAITING_FOR_STOP_DEP);
}

static void handle_SERVICE_WAITING_FOR_START_DEP(active_db_h * service)
{
	assert(service);

	/*
	 * this checks depencncy.
	 * initng_depend_start_dep_met() will return:
	 * TRUE (dep is met)
	 * FALSE (dep is not met)
	 * FAIL (dep is failed)
	 */
	switch (initng_depend_start_dep_met(service, FALSE))
	{
		case TRUE:
			break;
		case FAIL:
			initng_common_mark_service(service, &SERVICE_START_DEPS_FAILED);
			return;
		default:
			/* return and hope that this handler will be called again. */
			return;
	}

	/* if system is shutting down, don't start anything */
	if (g.sys_state != STATE_STARTING && g.sys_state != STATE_UP)
	{
		D_("Can't start service, when system status is: %i !\n", g.sys_state);
		initng_common_mark_service(service, &SERVICE_STOPPED);
		return;
	}

	/* set status to START_DEP_MET */
	initng_common_mark_service(service, &SERVICE_START_DEPS_MET);

}

static void handle_SERVICE_WAITING_FOR_STOP_DEP(active_db_h * service)
{
	assert(service);

	/* check with other plug-ins, if it is ok to stop this service now */
	if (initng_depend_stop_dep_met(service, FALSE) != TRUE)
		return;

	/* ok, stopping deps are met */
	initng_common_mark_service(service, &SERVICE_STOP_DEPS_MET);
}

static void init_SERVICE_START_DEPS_MET(active_db_h * service)
{
	if (!initng_common_mark_service(service, &SERVICE_START_RUN))
		return;

	/* F I N A L L Y   S T A R T */
	switch (initng_execute_launch(service, &T_START, NULL))
	{
		case FALSE:
			F_("Did not find a start entry to run!\n", service->name);
			initng_common_mark_service(service, &SERVICE_FAIL_START_NONEXIST);
			return;
		case FAIL:
			F_("Service %s, could not launch start, did not find any to launch!\n", service->name);
			initng_common_mark_service(service, &SERVICE_FAIL_START_LAUNCH);
			return;
		default:
			return;
	}

}

static void init_SERVICE_STOP_DEPS_MET(active_db_h * service)
{
	/* mark this service as STOPPING */
	if (!initng_common_mark_service(service, &SERVICE_STOP_RUN))
		return;

	/* launch stop service */
	switch (initng_execute_launch(service, &T_STOP, NULL))
	{
		case FAIL:
			F_("  --  (%s): fail launch stop!\n", service->name);
			initng_common_mark_service(service, &SERVICE_FAIL_STOP_NONEXIST);
			return;
		case FALSE:
			/* there exists no stop process */
			initng_common_mark_service(service, &SERVICE_STOPPED);
			return;
		default:
			return;
	}

}

/*
 * When a service got the state SERVICE_START_RUN
 * this function will set the timeout, so the alarm
 * function will be called after a while.
 */
static void init_SERVICE_START_RUN(active_db_h * service)
{
	int timeout;

	D_("Service %s, run init_SERVICE_START_RUN\n", service->name);

	/* if NEVER_KILL is set, dont bother */
	if (is(&NEVER_KILL, service))
		return;

	/* get the timeout */
	timeout = get_int(&START_TIMEOUT, service);

	/* if not set, use the default one */
	if (!timeout)
		timeout = DEFAULT_START_TIMEOUT;

	/* set state alarm */
	initng_handler_set_alarm(service, timeout);
}

/*
 * When a service got the state SERVICE_STOP_RUN
 * this function will set the timeout, so the alarm
 * function will be called after a while.
 */
static void init_SERVICE_STOP_RUN(active_db_h * service)
{
	int timeout;

	D_("Service %s, run init_SERVICE_STOP_RUN\n", service->name);

	/* if NEVER_KILL is set, dont bother */
	if (is(&NEVER_KILL, service))
		return;

	/* get the timeout */
	timeout = get_int(&STOP_TIMEOUT, service);

	/* if not set, use the default timeout */
	if (!timeout)
		timeout = DEFAULT_STOP_TIMEOUT;

	/* set state alarm */
	initng_handler_set_alarm(service, timeout);
}


static void timeout_SERVICE_START_RUN(active_db_h * service)
{
	process_h *process = NULL;

	/* if NEVER_KILL is set, dont bother */
	if (is(&NEVER_KILL, service))
		return;

	W_("Timeout for start process, service %s.  Killing this process now.\n",
	   service->name);

	/* get the process */
	if (!(process = initng_process_db_get(&T_START, service)))
	{
		F_("Did not find the T_START process!\n");
		return;
	}

	/* if the process does not exist on the system anymore, run the killd handler. */
	if (process->pid <= 0 || (kill(process->pid, 0) && errno == ESRCH))
	{
		W_("The start process have dissapeared from the system without notice, running start kill handler.\n");
		handle_killed_start(service, process);
		return;
	}

	/* send the process the SIGKILL signal */
	kill(process->pid, SIGKILL);

	/* set service state, to a failure state */
	initng_common_mark_service(service, &SERVICE_START_FAILED_TIMEOUT);
}

static void timeout_SERVICE_STOP_RUN(active_db_h * service)
{
	process_h *process = NULL;

	/* if NEVER_KILL is set, dont bother */
	if (is(&NEVER_KILL, service))
		return;

	W_("Timeout for stop process, service %s.   Killing this process now.\n",
	   service->name);

	/* get the process */
	if (!(process = initng_process_db_get(&T_STOP, service)))
	{
		F_("Did not find the T_STOP process!\n");
		return;
	}

	/* if the process does not exist on the system anymore, run the killd handler. */
	if (process->pid <= 0 || (kill(process->pid, 0) && errno == ESRCH))
	{
		W_("The stop process have dissapeared from the system without notice, running stop kill handler.\n");
		handle_killed_stop(service, process);
		return;
	}

	/* send the process the SIGKILL signal */
	kill(process->pid, SIGKILL);

	/* set service state, to a failure state */
	initng_common_mark_service(service, &SERVICE_STOP_FAILED_TIMEOUT);
}




/*
 * ############################################################################
 * #                         KILL HANDLER FUNCTIONS                            #
 * ############################################################################
 */

/*
 * handle_killed_start(), description:
 *
 * A process dies, and kill_handler walks the active_db looking for a match
 * on that pid, if it founds a match for a active->start_process, this
 * function is called, with a pointer to that active_service to
 * handle the situation.
 */
static void handle_killed_start(active_db_h * service, process_h * process)
{
	assert(service);
	assert(service->name);
	assert(service->current_state);
	assert(service->current_state->state_name);
	assert(process);
	int rcode;

	D_("handle_killed_start(%s): initial status: \"%s\".\n",
	   service->name, service->current_state->state_name);


	/* free this process what ever happends */
	rcode = process->r_code;				/* save rcode */
	initng_process_db_free(process);

	/*
	 * If this exited after a timeout, stay failed
	 */
	if (IS_MARK(service, &SERVICE_START_FAILED_TIMEOUT))
		return;

	/*
	 * If service is stopping, ignore this signal
	 */
	if (!IS_MARK(service, &SERVICE_START_RUN))
	{
		F_("Start exited!, and service is not marked starting!\nWas this one launched manually by ngc --run ??");
		return;
	}

	/* check so it did not segfault */

	if (WTERMSIG(process->r_code) == 11)
	{
		F_("Service %s process start SETGFAUTED!\n");
		initng_common_mark_service(service, &SERVICE_FAIL_START_SIGNAL);
		return;
	}

	/*
	 * Make sure r_code don't signal error (can be override by UP_ON_FAILURE.
	 */

	/* if rcode > 0 */
	if (WEXITSTATUS(rcode) > 0 && !is(&START_FAIL_OK, service))
	{
		F_(" start %s, Returned with TERMSIG %i rcode %i.\n", service->name,
		   WTERMSIG(rcode), WEXITSTATUS(rcode));
		initng_common_mark_service(service, &SERVICE_FAIL_START_RCODE);
		return;
	}

	/* TODO, create state WAITING_FOR_UP_CHECK */

	/* make the final up check */
	if (initng_depend_up_check(service) != TRUE)
	{
		initng_common_mark_service(service, &SERVICE_UP_CHECK_FAILED);
		return;
	}

	/* OK! now service is DONE! */
	initng_common_mark_service(service, &SERVICE_DONE);
}


/* to do when a stop action dies */
static void handle_killed_stop(active_db_h * service, process_h * process)
{
	assert(service);
	assert(service->name);
	assert(service->current_state);
	assert(service->current_state->state_name);
	assert(process);
	int rcode;

	D_("(%s);\n", service->name);

	/* Free the process what ever happens below */
	rcode = process->r_code;
	initng_process_db_free(process);


	/*
	 * If this exited after a timeout, stay failed
	 */
	if (IS_MARK(service, &SERVICE_STOP_FAILED_TIMEOUT))
		return;

	/* make sure its a STOP_RUN state */
	if (!IS_MARK(service, &SERVICE_STOP_RUN))
	{
		F_("stop service died, but service is not status STOPPING!\n");
		return;
	}

	/* check with SIGSEGV (11) */
	if (WTERMSIG(process->r_code) == 11)
	{
		F_(" service %s stop process SEGFAUTED!\n", service->name);

		/* mark service stopped */
		initng_common_mark_service(service, &SERVICE_FAIL_STOP_SIGNAL);

		return;
	}


	/*
	 * If the return code (for example "exit 1", in a bash script)
	 * from the program, is bigger than 0, this commonly signalize
	 * that something got wrong, print this as an error msg on screen
	 */
	if (WEXITSTATUS(process->r_code) > 0 && !is(&STOP_FAIL_OK, service))
	{
		F_(" stop %s, Returned with exit %i.\n", service->name,
		   WEXITSTATUS(process->r_code));

		/* mark service stopped */
		initng_common_mark_service(service, &SERVICE_FAIL_STOP_RCODE);

		return;
	}

	/* mark service stopped */
	initng_common_mark_service(service, &SERVICE_STOPPED);
}
