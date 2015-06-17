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


#ifndef INTING_DEPEND_H
#define INITNG_DEPEND_H

/* dependecy checkings */
int initng_depend(active_db_h * service, active_db_h * check);
int initng_depend_deep(active_db_h * service, active_db_h * check);
int initng_any_depends_on(active_db_h * service);

/* To stop deps, and check that its okay for stopping a service */
int initng_depend_stop_deps(active_db_h * service);
int initng_depend_stop_dep_met(active_db_h * service, int verbose);

/* To start deps, and if its ok to start a service */
int initng_depend_start_deps(active_db_h * service);
int initng_depend_start_dep_met(active_db_h * service, int verbose);

/* Set the restart flags, and stops all service that are depended on this service */
int initng_depend_restart_deps(active_db_h * service);

/* If its ok, to put a service as up */
int initng_depend_up_check(active_db_h * service);
#endif
