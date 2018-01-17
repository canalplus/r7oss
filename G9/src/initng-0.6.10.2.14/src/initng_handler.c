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

#include <sys/time.h>
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
#include <errno.h>

#include "initng_global.h"

#include "initng_active_db.h"
#include "initng_toolbox.h"
#include "initng_main.h"
#include "initng_execute.h"
#include "initng_common.h"
#include "initng_load_module.h"
#include "initng_depend.h"

#include "initng_handler.h"
#include "initng_kill_handler.h"
#include "initng_plugin_callers.h"
#include "initng_static_data_id.h"
#include "initng_static_states.h"

void initng_handler_restart_restarting(void)
{
	active_db_h *current = NULL;

	/* make sure there is no more restarting running or stopping */
	while_active_db(current)
	{
		if (IS_STOPPING(current))
		{
			if (is(&RESTARTING, current))
				return;
		}
	}

	/* Okey, there was no stopping restart marked services. */


	/* okay, now restart all RESTARTING services, that are STOPPED */
	current = NULL;
	while_active_db(current)
	{
		if (IS_DOWN(current))
		{
			if (is(&RESTARTING, current))
			{
				/* remove the restarting flag */
				remove(&RESTARTING, current);

				/* start the service */
				initng_handler_start_service(current);
			}
		}
	}
}

/*
 * Find alarm, will brows active_db for states when the timeout have gone
 * out and run them.
 */
void initng_handler_run_alarm(void)
{
	active_db_h *current, *q = NULL;

	S_;

	/* reset next_alarm */
	g.next_alarm = 0;

	/* walk through all active services */
	while_active_db_safe(current, q)
	{
		assert(current->name);
		assert(current->current_state);

		/* skip services where alarm is not set */
		if (current->alarm == 0)
			continue;

		/* if alarm has gone */
		if (g.now.tv_sec >= current->alarm)
		{
			/* reset alarm */
			current->alarm = 0;


			/* call alarm handler, now when we got an g.interrupt */
			if (current->current_state->state_alarm)
				(*current->current_state->state_alarm) (current);
			continue;
		}

		/* if no next_to_get is set */
		if (!g.next_alarm)
		{
			g.next_alarm = current->alarm;
			continue;
		}

		/* if this alarm is more early then next_to_get set it. */
		if (current->alarm < g.next_alarm)
		{
			g.next_alarm = current->alarm;
		}
	}
}

	/* M A R K    S T U F F */

	/* When this function is called, the service is marked for starting */
int initng_handler_start_service(active_db_h * service_to_start)
{
	assert(service_to_start);
	assert(service_to_start->name);
	assert(service_to_start->current_state);

	D_("start_service(%s);\n", service_to_start->name);

	if (!service_to_start->type)
	{
		F_("Type for service %s not set.\n", service_to_start->name);
		return (FALSE);
	}

	/* check system state, if we can launch. */
	if (g.sys_state != STATE_STARTING && g.sys_state != STATE_UP)
	{
		F_("Can't start service %s, when system status is: %i !\n",
		   service_to_start->name, g.sys_state);
		return (FALSE);
	}

	/* else, mark the service for restarting and stop it */
	if (is(&RESTARTING, service_to_start))
	{
		D_("Cant manually start a service that is restarting.\n");
		return (FALSE);
	}

	/* it might already be starting */
	if (IS_STARTING(service_to_start) || IS_WAITING(service_to_start))
	{
		D_("service %s is starting already.\n", service_to_start->name);
		return (TRUE);
	}

	/* if it failed */
	if (IS_FAILED(service_to_start))
	{
		D_("Service %s failed and must be reset before it can be started again!\n", service_to_start->name);
		return (FALSE);
	}

	/* it might already be up */
	if (IS_UP(service_to_start))
	{
		D_("service %s is already up!\n", service_to_start->name);
		return (TRUE);
	}
	
	/* if new, and not got a stopped state yet, its no idea to bug this process */
	if (IS_NEW(service_to_start))
	{
		D_("service %s is so fresh so we cant start it.\n", service_to_start->name);
		return (TRUE);
	}

	/* it must be down or stopping to start it */
	if (!(IS_DOWN(service_to_start) || IS_STOPPING(service_to_start)))
	{
		W_("Can't set a service with status %s, to start\n",
		   service_to_start->current_state->state_name);
		return (FALSE);
	}

	/* This will run this functuin (start_service) for all dependecys this service have. */
	if (initng_depend_start_deps(service_to_start) != TRUE)
	{
		D_("Could not start %s, because a required dependency could not be found.\n");
		return (FALSE);
	}

	/* Now execute the local start_service code, in the service type plugin. */
	if (!service_to_start->type->start_service)
		return (FALSE);

	/* execute it */
	return ((*service_to_start->type->start_service) (service_to_start));
}

	/* S T O P     S E R V I C E */
int initng_handler_stop_service(active_db_h * service_to_stop)
{

	assert(service_to_stop);
	assert(service_to_stop->name);
	assert(service_to_stop->current_state);

	D_("stop_service(%s);\n", service_to_stop->name);

	if (!service_to_stop->type)
	{
		F_("Service %s type is missing!\n", service_to_stop->name);
		return (FALSE);
	}

	/* check so it not failed */
	if (IS_FAILED(service_to_stop))
	{
		D_("Service %s is set filed, and cant be stopped.\n",
		   service_to_stop->name);
		return (TRUE);
	}

	/* IF service is stopping, do nothing. */
	if (IS_STOPPING(service_to_stop))
	{
		D_("service %s is stopping already!\n", service_to_stop->name);
		return (TRUE);
	}

	/* check if its currently already down */
	if (IS_DOWN(service_to_stop))
	{
		D_("Service %s is down already.\n", service_to_stop->name);
		return (TRUE);
	}

	/* must be up or starting, to stop */
	if (!(IS_UP(service_to_stop) || IS_STARTING(service_to_stop)))
	{
		W_("Service %s is not up but %s, and cant be stopped.\n",
		   service_to_stop->name, service_to_stop->current_state->state_name);

		return (FALSE);
	}

	/* if stop_service code is included in type, use it. */
	if (!service_to_stop->type->stop_service)
	{
		W_("Service %s Type %s  has no stopper, will return FALSE!\n",
		   service_to_stop->name, service_to_stop->type->name);
		return (FALSE);
	}


	return ((*service_to_stop->type->stop_service) (service_to_stop));
}

int initng_handler_restart_service(active_db_h * service_to_restart)
{
	S_;
	assert(service_to_restart);
	assert(service_to_restart->name);
	assert(service_to_restart->current_state);

	/* type has to be known to restart the service */
	if (!service_to_restart->type)
		return (FALSE);

	if (!IS_UP(service_to_restart))
	{
		D_("Can only restart a running service %s. ( now_state : %s )\n",
		   service_to_restart->name,
		   service_to_restart->current_state->state_name);
		return (FALSE);
	}

	/* Restart all dependencys to this service */
	initng_depend_restart_deps(service_to_restart);

	/* if there exits a restart code, use it */
	if (service_to_restart->type->restart_service)
		return ((*service_to_restart->type->
				 restart_service) (service_to_restart));

	/* else, mark the service for restarting and stop it */
	set(&RESTARTING, service_to_restart);
	return (initng_handler_stop_service(service_to_restart));
}



/* Load new process named */
active_db_h *initng_handler_start_new_service_named(const char *service)
{
	active_db_h *to_load = NULL;

	D_(" start_new_service_named(%s);\n", service);

	assert(service);

	/* Try to find it */
	if ((to_load = initng_active_db_find_by_name(service)))
	{
		if (!IS_DOWN(to_load))
		{
			D_("Service %s exits already, and is not stopped!\n",
			   to_load->name);
			return (to_load);
		}

		/* okay, now start it */
		initng_handler_start_service(to_load);

		return (to_load);

	}

	/* get from hook */
	if ((to_load = initng_plugin_create_new_active(service)))
	{
		return (to_load);
	}

#ifdef SERVICE_CACHE
	/* else try create and load a new service */
	if ((to_load = initng_common_load_to_active(service)))
	{
		/* okay, now start it */
		initng_handler_start_service(to_load);

		return (to_load);
	}
#endif

	/* the function calling this function will print out an error */
	D_("Unable to load active for service %s\n", service);
	return (NULL);

}

/* enter a new runlevel */
int initng_handler_stop_all(void)
{
	active_db_h *ser, *q = NULL;

	S_;
	initng_main_set_sys_state(STATE_STOPPING);

	while_active_db_safe(ser, q)
	{
		/* don't stop a stopping service */
		if (IS_STOPPING(ser))
			continue;
		if (IS_DOWN(ser))
			continue;
		if (IS_FAILED(ser))
			continue;

		/* stop services */
		initng_handler_stop_service(ser);
	}
	return (TRUE);
}
