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

#ifndef INITNG_LOAD_MODULE
#define INITNG_LOAD_MODULE
#include "initng_active_db.h"
//#include "initng_service_cache.h"
#include "initng_global.h"
#include "initng_system_states.h"
#include "initng_module.h"

/* public interface */
m_h *initng_load_module(const char *module_path);
int initng_unload_module_named(const char *name);
int initng_load_module_load_all(const char *plugin_path);
void initng_unload_module_unload_all(void);
void initng_unload_module_unload_marked(void);

/* functions for internal use only (exposed for testing) */
m_h *initng_load_module_open(const char *module_path,
							 const char *module_name);
void initng_load_module_close_and_free(m_h * m);

#endif
