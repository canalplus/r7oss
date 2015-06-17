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


#include <stdio.h>
#include <stdlib.h>							/* free() exit() */
#include <string.h>
#include <assert.h>

#include "initng_handler.h"
#include "initng_global.h"
#include "initng_common.h"
#include "initng_toolbox.h"
#include "initng_static_data_id.h"
#include "initng_static_states.h"
#include "initng_env_variable.h"
#include "initng_static_event_types.h"

#include "initng_depend.h"

static int dep_on(active_db_h * service, active_db_h * check);


/*
 * initng_depend:
 *  Will check with all plug-ins and return TRUE
 *  if service depends on check.
 */
int initng_depend(active_db_h * service, active_db_h * check)
{
	assert(service);
	assert(check);

	/* it can never depend on itself */
	if (service == check)
		return (FALSE);

	/* run the local static dep check */
	if (dep_on(service, check) == TRUE)
		return (TRUE);

	/* run the global plugin dep check */
	{
		s_event event;
		s_event_dep_on_data data;

		event.event_type = &EVENT_DEP_ON;
		event.data = &data;
		data.service = service;
		data.check = check;

		if (initng_event_send(&event) == TRUE)
			return (TRUE);
	}

	/* No, "service" was not depending on "check" */
	return (FALSE);
}

/*
 * A deeper deep-find.
 * Logic, we wanna make sure that depend check in a deeper level.
 * if daemon/smbd -> daemon/samba -> system/checkroot -> system/initial.
 * So should initng_depend_deep(daemon/smbd, system/initial) == TRUE
 *
 * Summary, does service depends on check?
 */
static int initng_depend_deep2(active_db_h * service, active_db_h * check,
							   int *stack);
int initng_depend_deep(active_db_h * service, active_db_h * check)
{
	int stack = 0;

	return (initng_depend_deep2(service, check, &stack));
}

static int initng_depend_deep2(active_db_h * service, active_db_h * check,
							   int *stack)
{
	active_db_h *current = NULL;
	int result = FALSE;

	/* avoid cirular */
	if (current == service)
		return (FALSE);

	/* if service depends on check, it also dep_on_deep's on check */
	/* this serves as an exit from the recursion */
	if (initng_depend(service, check))
		return (TRUE);

	/* in case there is a circular dependency, break after 10 levels of recursion */
	(*stack)++;
	if (*stack > 10)
		return (FALSE);

	/* loop over all services, if service depends on current, recursively check if
	 * current may depend (deep) on check */
	while_active_db(current)
	{
		if (initng_depend(service, current))
		{
			if ((result = initng_depend_deep2(current, check, stack)))
				break;
		}
	}

	/* return with the result found at last */
	return (result);
}

/* check if any service in list, that is starting, running, or stopping
 * is depending on service
 * returns TRUE if any service depends * service
 */
int initng_any_depends_on(active_db_h * service)
{
	active_db_h *current = NULL;

	D_("initng_any_depends_on(%s);\n", service->name);

	while_active_db(current)
	{
		/* Dont mind stop itself */
		if (current == service)
			continue;

		if (IS_UP(current) || IS_STARTING(current) || IS_STOPPING(current))
		{

			/* if current depends on service */
			if (initng_depend_deep(current, service) == TRUE)
			{
				D_("Service %s depends on %s\n", current->name,
				   service->name);
				return (TRUE);
			}
		}
	}
	D_("None found depending on %s.\n", service->name);
	return (FALSE);
}

/* standard dep check , does service depends on check? */
static int dep_on(active_db_h * service, active_db_h * check)
{
	s_data *current = NULL;
	char *str = NULL;

	assert(service);
	assert(service->name);
	assert(check);
	assert(check->name);

	/* walk all possible entrys, use get_next with NULL because we want both REQUIRE and NEED */
	while ((current = get_next(NULL, service, current)))
	{
		/* only intreseted in two types */
		if (current->type != &REQUIRE && current->type != &NEED
			&& current->type != &USE)
			continue;

		/* to be sure */
		if (!current->t.s)
			continue;

		/* fix the variables */
		if (!(str = fix_variables(current->t.s, service)))
			continue;

		if (strcmp(str, check->name) == 0)
		{
			fix_free(str, current->t.s);
			return (TRUE);
		}
		fix_free(str, current->t.s);
	}

	/* No, it did not */
	return (FALSE);
}

int initng_depend_stop_deps(active_db_h * service)
{
	active_db_h *current = NULL;
	active_db_h *safe = NULL;

	/* also stop all service depending on service_to_stop */
	while_active_db_safe(current, safe)
	{
		/* Dont mind stop itself */
		if (current == service)
			continue;

		/* if current depends on the one we are stopping */
		if (initng_depend_deep(current, service) == TRUE)
			initng_handler_stop_service(current);
	}

	return (TRUE);
}

int initng_depend_restart_deps(active_db_h * service)
{
	active_db_h *current = NULL;
	active_db_h *safe = NULL;

	/* also stop all service depending on service_to_stop */
	while_active_db_safe(current, safe)
	{
		/* Dont mind stop itself */
		if (current == service)
			continue;

		/* if current depends on the one we are stopping */
		if (initng_depend_deep(current, service) == TRUE)
			initng_handler_restart_service(current);
	}

	return (TRUE);
}


/*
 * This is a final check, before a daemon or service can be marked as UP.
 */
int initng_depend_up_check(active_db_h * service)
{
	s_event event;

	event.event_type = &EVENT_UP_MET;
	event.data = service;

	return (initng_event_send(&event));
}

/*
 * This will check with plug-ins if dependencies for start this is met.
 * If this returns FALSE deps are not met yet, try later.
 * If this returns FAIL deps wont ever be met, so stop trying.
 */
int initng_depend_start_dep_met(active_db_h * service, int verbose)
{
	active_db_h *dep = NULL;
	s_data *current = NULL;
	char *str = NULL;
	int count = 0;

	assert(service);
	assert(service->name);

	/* walk all possible entrys, use get_next with NULL becouse we want both REQUIRE, NEED and USE */
	while ((current = get_next(NULL, service, current)))
	{
		/* only intreseted in two types */
		if (current->type != &REQUIRE && current->type != &NEED
			&& current->type != &USE)
			continue;

		/* to be sure */
		if (!current->t.s)
			continue;

		/* this is a cache, that meens that calling this function over and over again, we dont make
		 * two checks on same entry */
		count++;
		if (service->depend_cache >= count)
		{
			D_("Dep %s is ok allredy for %s.\n", current->t.s, service->name);
			continue;
		}


		/* tell the user what we got */
#ifdef DEBUG
		if (current->type == &REQUIRE)
			D_(" %s requires %s\n", service->name, current->t.s);
		else if (current->type == &NEED)
			D_(" %s needs %s\n", service->name, current->t.s);
		else if (current->type == &USE)
			D_(" %s uses %s\n", service->name, current->t.s);
#endif

		/* fix the variables */
		if (!(str = fix_variables(current->t.s, service)))
			continue;

		/* look if it exits already */
		if (!(dep = initng_active_db_find_by_exact_name(str)))
		{
			if (current->type == &USE)
			{
				/* if its not yet found, and i dont care */
				fix_free(str, current->t.s);
				continue;
			}
			else if (current->type == &REQUIRE)
			{
				F_("%s required dep \"%s\" could not start!\n", service->name,
				   str);
				initng_common_mark_service(service, &REQ_NOT_FOUND);
				/* if its not yet found, this dep is not reached */
				fix_free(str, current->t.s);
				return (FAIL);
			}
			else
			{								/* NEED */
				/* if its not yet found, this dep is not reached */
				fix_free(str, current->t.s);
				return (FALSE);
			}
		}

		/* if service dep on is starting, wait a bit */
		if (IS_STARTING(dep))
		{
			if (verbose)
			{
				F_("Could not start service %s because it depends on service %s that is still starting.\n", service->name, dep->name);
			}
			fix_free(str, current->t.s);
			return (FALSE);
		}

		/* if service failed, return that */
		if (IS_FAILED(dep))
		{
			if (verbose)
			{
				F_("Could not start service %s because it depends on service %s that is failed.\n", service->name, dep->name);
			}
			fix_free(str, current->t.s);
			return (FAIL);
		}

		/* if its this fresh, we dont do anything */
		if (IS_NEW(dep))
		{
			fix_free(str, current->t.s);
			return(FALSE);
		}

		/* if its marked down, and not starting, start it */
		if (IS_DOWN(dep))
		{
		   initng_handler_start_service(dep);
		   fix_free(str, current->t.s);
		   return (FALSE);
		}

		/* if its not starting or up, return FAIL */
		if (!IS_UP(dep))
		{
			F_("Could not start service %s because it depends on service %s has state %s\n", service->name, dep->name, dep->current_state->state_name);
			fix_free(str, current->t.s);
			return (FALSE);
		}

		/* GOT HERE MEENS THAT ITS OK */
		service->depend_cache = count;
		D_("Dep %s is ok for %s.\n", current->t.s, service->name);
		/* continue; */
	}

	/* run the global plugin dep check */
	{
		s_event event;

		event.event_type = &EVENT_START_DEP_MET;
		event.data = service;

		if (initng_event_send(&event) == FAIL)
		{
			if (verbose)
			{
				F_("Service %s can not be started because a plugin (EVENT_START_DEP_MET) says so.\n", service->name);
			}

			return (FALSE);
		}
	}

	D_("dep met for %s\n", service->name);

	/* reset the count cache */
	service->depend_cache = 0;
	return (TRUE);
}

/*
 * This will check with plug-ins if dependencies for stop is met.
 * If this returns FALSE deps for stopping are not met, try again later.
 * If this returns FAIL stop deps, wont EVER be met, stop trying.
 */
int initng_depend_stop_dep_met(active_db_h * service, int verbose)
{
	active_db_h *currentA = NULL;
	int count = 0;

	/*
	 *    Check so all deps, that needs service, is down.
	 *    if there are services depending on this one still running, return false and still try
	 */
	while_active_db(currentA)
	{
		count++;
		if (service->depend_cache >= count)
			continue;

		/* temporary increase depend_cache - will degrese before reutrn (FALSE) */
		service->depend_cache++;

		if (currentA == service)
			continue;

		/* Does service depends on current ?? */
		if (initng_depend(currentA, service) == FALSE)
			continue;

		/* if its done, this is perfect */
		if (IS_DOWN(currentA))
			continue;

		/* If the dep is failed, continue */
		if (IS_FAILED(currentA))
			continue;

		/* BIG TODO.
		 * This is not correct, but if we wait for a service that is
		 * starting to stop, and that service is waiting for this service
		 * to start, until it starts, makes a deadlock.
		 *
		 * Asuming that STARTING services WAITING_FOR_START_DEP are down for now.
		 */
		if (IS_STARTING(currentA)
			&& strstr(currentA->current_state->state_name,
					  "WAITING_FOR_START_DEP"))
			continue;

#ifdef DEBUG
		/* else RETURN */
		if (verbose)
			D_("still waiting for service %s state %s\n", currentA->name,
			   currentA->current_state->state_name);
		else
			D_("still waiting for service %s state %s\n", currentA->name,
			   currentA->current_state->state_name);
#endif

		/* if its still marked as UP and not stopping, tell the service AGAIN nice to stop */
		/*if (IS_UP(currentA))
		   initng_handler_stop_service(currentA); */

		/* no, the dependency are not met YET */
		service->depend_cache--;
		return (FALSE);
	}


	/* run the global plugin dep check */
	{
		s_event event;

		event.event_type = &EVENT_STOP_DEP_MET;
		event.data = service;

		if (initng_event_send(&event) == FAIL)
		{
			if (verbose)
			{
				F_("Service %s can not be started because a plugin (START_DEP_MET) says so.\n", service->name);
			}

			return (FALSE);
		}
	}

	service->depend_cache = 0;
	return (TRUE);
}

/*
 * Start all deps, required or needed.
 * If a required deps failed to start, this will return FALSE
 */
int initng_depend_start_deps(active_db_h * service)
{
	active_db_h *dep = NULL;
	s_data *current = NULL;
	char *str = NULL;

	assert(service);
	assert(service->name);
#ifdef DEBUG
	D_("initng_depend_start_deps(%s);\n", service->name);
#endif

	/* walk all possible entrys, use get_next with NULL becouse we want both REQUIRE and NEED */
	while ((current = get_next(NULL, service, current)))
	{
		/* only intreseted in two types */
		if (current->type != &REQUIRE && current->type != &NEED)
			continue;

		/* to be sure */
		if (!current->t.s)
			continue;

		/* tell the user what we got */
		D_(" %s %s %s\n", service->name,
		   current->type == &REQUIRE ? "requires" : "needs", current->t.s);

		/* fix the variables */
		if (!(str = fix_variables(current->t.s, service)))
			continue;

		/* look if it exits already */
		if ((dep = initng_active_db_find_by_exact_name(str)))
		{
			D_("No need to LOAD \"%s\" == \"%s\", state %s it is already loaded!\n", str, dep->name, dep->current_state->state_name);
			/* start the service if its down */
			if (IS_DOWN(dep))
			{
				D_("Service %s is down, starting.\n", dep->name);
				initng_handler_start_service(dep);
			}

			fix_free(str, current->t.s);
			continue;
		}

		D_("Starting new_service becouse not found: %s\n", str);
		/* if we where not succeded to start this new one */
		if (!initng_handler_start_new_service_named(str))
		{
			/* if its NEED */
			if (current->type == &NEED)
			{
				D_("service \"%s\" needs service \"%s\", that could not be found!\n", service->name, str);
				fix_free(str, current->t.s);
				continue;
				/* else its REQUIRE */
			}
			else
			{

				F_("%s required dep \"%s\" could not start!\n", service->name,
				   str);
				initng_common_mark_service(service, &REQ_NOT_FOUND);
				fix_free(str, current->t.s);
				return (FALSE);
			}
		}
		fix_free(str, current->t.s);
		/* continue ; */
	}

	D_("initng_depend_start_deps(%s): DONE-TRUE\n", service->name);

	/* if we got here, its a sucess */
	return (TRUE);
}
