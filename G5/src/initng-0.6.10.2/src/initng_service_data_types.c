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

#include <string.h>
#include <stdlib.h>
#include <assert.h>

#include "initng_global.h"
#include "initng_service_data_types.h"
#include "initng_toolbox.h"
#include "initng_static_data_id.h"
#include "initng_list.h"

/*
 * Simple function to add the option, to the official list
 * of options in initng.
 */
void initng_service_data_type_register(s_entry * ent)
{

	assert(ent);
	S_;

	/*
	 * set the opt_name_len,
	 * an optimize so that we don't need to strlen every time accessing this one.
	 */
	if (ent->opt_name)
		ent->opt_name_len = strlen(ent->opt_name);
	else
		ent->opt_name_len = 0;

#ifdef CHECK_IF_CURRENTLY_ADDED
	{
		s_entry *current = NULL;

		/* walk the option_db, and make sure its not added, or option_name taken */
		while_service_data_types(current)
		{
			if (current == ent)
			{
				if (ent->opt_name)
					F_("Option %s, already added!\n", ent->opt_name);
				else
					F_("Option, already added!\n");

				return;
			}
			if (current->opt_name && ent->opt_name
				&& strcmp(current->opt_name, ent->opt_name) == 0)
			{
				F_("option %s, name taken.\n");
				return;
			}
		}
	}
#endif

	/* add the option to the option_db list */
	list_add(&ent->list, &g.option_db.list);
#ifdef DEBUG
	if (ent->opt_name)
		D_(" \"%s\" added to option_db!\n", ent->opt_name);
#endif
}

/*
 * This safely deletes and option from option_db.
 * safely removes all data on that option from active_db, and service_db
 * that depends on the option, removed.
 */
void initng_service_data_type_unregister(s_entry * ent)
{
	active_db_h *currentA = NULL;

#ifdef SERVICE_CACHE
	service_cache_h *currentS = NULL;
#endif

	S_;
	assert(ent);

	/* clear the active db for this data */
	while_active_db(currentA)
	{
		remove(ent, currentA);
	}

#ifdef SERVICE_CACHE
	/* clear the file cache from this data */
	while_service_cache(currentS)
	{
		remove(ent, currentS);
	}
#endif

	/* remove it from the list */
	list_del(&ent->list);
}

/*
 * This walks the option_db and
 * removing ALL entries.
 */
void initng_service_data_type_unregister_all(void)
{
	s_entry *current, *safe = NULL;

	/* walk the option db, remove all */
	while_service_data_types_safe(current, safe)
	{
		initng_service_data_type_unregister(current);
	}

	/* make sure the list is cleared! */
	INIT_LIST_HEAD(&g.option_db.list);
}

/*
 * Use this function to search a pointer in the
 * option_db
 */
s_entry *initng_service_data_type_find(const char *string)
{
	s_entry *current = NULL;

	S_;
	assert(string);
	D_("looking for %s.\n", string);
	while_service_data_types(current)
	{
		if (current->opt_name && strcmp(current->opt_name, string) == 0)
			return (current);
	}
	return (NULL);
}
