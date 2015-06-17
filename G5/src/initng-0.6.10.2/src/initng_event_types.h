/*
 * Initng, a next generation sysvinit replacement.
 * Copyright (C) 2006 Jimmy Wennlund <jimmy.wennlund@gmail.com>
 * Copyright (C) 2006 Ismael Luceno <ismael.luceno@gmail.com>
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

#ifndef INITNG_EVENT_TYPES_H
#define INITNG_EVENT_TYPES_H

#include "initng_list.h"
#include "initng_plugin.h"

typedef struct s_event_type_s {
	const char *name;
	const char *description;

	s_call hooks;

	int name_len;
	struct list_head list;
} s_event_type;

void initng_event_type_register(s_event_type *ent);
void initng_event_type_unregister(s_event_type *ent);
void initng_event_type_unregister_all(void);
s_event_type *initng_event_type_find(const char *string);

#define while_event_types(current) list_for_each_entry_prev(current, &g.event_db.list, list)
#define while_event_types_safe(current, safe) list_for_each_entry_prev_safe(current, safe, &g.event_db.list, list)

#endif
