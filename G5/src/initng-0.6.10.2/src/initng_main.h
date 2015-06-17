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

#ifndef INITNG_MAIN_H
#define INITNG_MAIN_H
#include <sys/types.h>
#include <unistd.h>
//#include "initng_service_cache.h"
#include "initng_process_db.h"
#include "initng_global.h"
void initng_main_exit(int i);
void initng_main_restart(void);
void initng_main_new_init(void);
void initng_main_set_sys_state(h_sys_state state);
void initng_main_sys_state_services_loaded(void);

int initng_main_ready_to_quit(void);
void initng_main_when_out(void);
void initng_main_su_login(void);
void initng_main_start_extra_services(void);
int initng_main_blacklist_add(const char *sname);

void initng_main_segfault(void);

void initng_main_set_runlevel(const char *runlevel);

#endif
