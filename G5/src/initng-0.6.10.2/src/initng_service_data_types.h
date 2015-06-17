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

#ifndef SERVICE_DATA_TYPES_H
#define SERVICE_DATA_TYPES_H

#include "initng_service_types.h"
#include "initng_list.h"


typedef enum
{
	U_D_T = 0,					/* UnDefinedType, unknown data type */
	STRING = 1,								/* Entry shud contain a string, when set again, the old string is owerwritten */
	STRINGS = 2,							/* On every add, a new string with same name is added */
	SET = 3,								/* A Bool, Enable or Disable */
	INT = 6,								/* An value set in it */
	ALIAS = 7,								/* Set this datatype when s_entry->alias is filled */
	TIME_T = 8,								/* Contains an time entry */

	/*
	 * This works works like abow, but you can attach variable names to it,
	 * like # exec start = /bin/test; and # env TEST = "hello";
	 */
	VARIABLE_STRING = 51,
	VARIABLE_STRINGS = 52,
	VARIABLE_SET = 53,
	VARIABLE_INT = 56,
	VARIABLE_TIME_T = 58,
} e_dt;

typedef struct ss_entry s_entry;
struct ss_entry
{
	/* set in statically */
	const char *opt_name;		/* The option name in a string */
	e_dt opt_type;				/* The variable type, the type of content, see abow */
	stype_h *ot;				/* Only used if you want to bound the option, to a special service type */
	const char *opt_desc;		/* Short description, shown by ngc -O */
	s_entry *alias;				/* You might point this to another s_entry, with another option_name, to get an alias */

	/* this should not be set static */
	int opt_name_len;
	struct list_head list;
};

void initng_service_data_type_register(s_entry * ent);
void initng_service_data_type_unregister(s_entry * ent);
s_entry *initng_service_data_type_find(const char *string);
void initng_service_data_type_unregister_all(void);

#define while_service_data_types(current) list_for_each_entry_prev(current, &g.option_db.list, list)
#define while_service_data_types_safe(current, safe) list_for_each_entry_prev_safe(current, safe, &g.option_db.list, list)

#endif
