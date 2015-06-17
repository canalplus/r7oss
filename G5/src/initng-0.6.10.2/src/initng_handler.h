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

#ifndef INITNG_HANDLER_H
#define INITNG_HANDLER_H
#include <sys/types.h>
#include <unistd.h>
#include <time.h>
#include "initng_active_db.h"

void initng_handler_restart_restarting(void);
int initng_handler_start_service(active_db_h * service);
int initng_handler_stop_service(active_db_h * service);
int initng_handler_restart_service(active_db_h * service);
active_db_h *initng_handler_start_new_service_named(const char *service);
void initng_handler_run_alarm(void);
int initng_handler_stop_all(void);

/* when set an alarm, we update service->alarm, and set g.next_alarm if this is the closest one */
#define initng_handler_set_alarm(service, seconds) { service->alarm = g.now.tv_sec + seconds; if(g.next_alarm==0 || service->alarm < g.next_alarm) g.next_alarm = service->alarm; }
#endif
