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

#ifndef INITNG_MODULE
#define INITNG_MODULE
#include "initng_list.h"

typedef struct module_struct
{
	char *module_name;
	char *module_filename;
	void *module_dlhandle;
	int initziated;
	int marked_for_removal;
	int (*module_init) (int api_version);
	void (*module_unload) (void);
	char **module_needs;

	struct list_head list;
} m_h;

#define while_module_db(current) list_for_each_entry_prev(current, &g.module_db.list, list)
#define while_module_db_safe(current, safe) list_for_each_entry_prev_safe(current, safe, &g.module_db.list, list)

#endif /* INITNG_MODULE */
