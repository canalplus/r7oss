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

#ifndef INITNG_ENV_VARIABLE
#define INITNG_ENV_VARIABLE

#include "initng.h"
#include "initng_active_db.h"

#define fix_free(variable, from) { if(variable && variable != from) free(variable); variable=NULL; }

char *fix_redefined_variable(const char *name, const char *oldval,
							 const char *newdef);
char *fix_variables(const char *from, active_db_h * s);

#ifdef SERVICE_CACHE
char *fix_variables2(const char *from, service_cache_h * s);
#endif
char **new_environ(active_db_h * s);
void free_environ(char **tf);
int is_same_env_var(char *var1, char *var2);

#endif
