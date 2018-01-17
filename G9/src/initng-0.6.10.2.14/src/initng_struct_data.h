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

#ifndef STRUCT_DATA_H
#define STRUCT_DATA_H

#include "initng_service_data_types.h"
#include "initng_list.h"

typedef struct ss_data s_data;


/*
 * This is the data struct == an initng variable, set to an service.
 */
struct ss_data
{
	/* and the data that it contains */
	union
	{
		/* by a char string */
		char *s;
		char *ss;
		/* or a int value */
		int i;
		/* or a long value */
		long l;
		/* or a pointer */
		void *p;
	} t;

	/* the type of the data */
	s_entry *type;

	/* if it is a dynamic variable name, put name here */
	char *vn;


	struct list_head list;
};

typedef struct s_data_head data_head;

struct s_data_head
{
	s_data head;				/* This is the header */
	data_head *res;				/* When no data is found, go look in this s_data */

	/*
	 * function pointers, if set this will be called first before
	 * looking for data.
	 */
	int (*data_request) (data_head * data_head);
	int (*res_request) (data_head * data_head);
};

#define DATA_HEAD_INIT(point) { INIT_LIST_HEAD(&(point)->head.list); (point)->res=NULL; (point)->data_request = NULL; (point)->res_request = NULL; }
#define DATA_HEAD_INIT_REQUEST(point, request_data, request_res) { INIT_LIST_HEAD(&(point)->head.list); (point)->res=NULL; (point)->data_request = request_data; (point)->res_request = request_res; }


/* data walkers */
s_data *d_get_next_var(s_entry * type, const char *vn, data_head * head,
					   s_data * last);
#define d_get_next(type, head, last) d_get_next_var(type, NULL, head, last)
#define get_next_var(type, vn, head, last) d_get_next_var(type, vn, &(head)->data, last)
#define get_next(type, head, last) get_next_var(type, NULL, head, last)
#define while_data(current, d) list_for_each_entry_prev(current, d, list)
#define while_data_safe(current, safe, d) list_for_each_entry_prev_safe(current, safe, d, list)

/*
 * d_is()
 * Returns true if a data by that type exists.
 */
int d_is_var(s_entry * type, const char *vn, data_head * d);

#define is_var(type, vn, point) d_is_var(type, vn, &(point)->data)
#define is(type, point) is_var(type, NULL, point)

/*
 * d_set()
 * Sets an bool value.
 */
void d_set_var(s_entry * type, char *vn, data_head * d);

#define set_var(type, vn, point) d_set_var(type, vn, &(point)->data)
#define set(type, point) set_var(type, NULL, point)


/*
 * d_get_int()
 * Gets an integear, with matching type, or 0 if not found.
 */
int d_get_int_var(s_entry * type, const char *vn, data_head * d);

#define get_int_var(type, vn, point) d_get_int_var(type, vn, &(point)->data)
#define get_int(type, point) get_int_var(type, NULL, point)

/*
 * set_int()
 * Sets an int value, on type.
 */
void d_set_int_var(s_entry * type, char *vn, data_head * d, int value);

#define set_int_var(type, vn, point, value) d_set_int_var(type, vn, &(point)->data, value)
#define set_int(type, point, value) set_int_var(type, NULL, point, value)

/*
 * d_get_string()
 * Returns a string, if found.
 */
const char *d_get_string_var(s_entry * type, const char *vn, data_head * d);

#define get_string_var(type, vn, point) d_get_string_var(type, vn, &(point)->data)
#define get_string(type, point) get_string_var(type, NULL, point)

/*
 * d_get_next_string()
 * If it is an array with lots of strings, step by this
 * function.
 */
const char *d_get_next_string_var(s_entry * type, const char *vn,
								  data_head * d, s_data ** cur);
#define get_next_string_var(type, vn, point, cur) d_get_next_string_var(type, vn, &(point)->data, cur)
#define get_next_string(type, point, cur) get_next_string_var(type, NULL, point, cur)

/*
 * d_set_string()
 * Sets a string.
 */
void d_set_string_var(s_entry * type, char *vn, data_head * d, char *string);

#define set_string_var(type, vn, point, string) d_set_string_var(type, vn, &(point)->data, string)
#define set_string(type, point, string) set_string_var(type, NULL, point, string)

/*
 * set_another_string, for use with get_next_string, with entrys containing lots of strings.
 */
void d_set_another_string_var(s_entry * type, char *vn, data_head * d,
							  char *string);
#define set_another_string_var(type, vn, point, string) d_set_another_string_var(type, vn, &(point)->data, string)
#define set_another_string(type, point, string) set_another_string_var(type, NULL, point, string)


/*
 * Count all variables, of a speical type.
 */
int d_count_type(s_entry * type, data_head * data);

#define d_count_all(point) d_count_type(NULL, point)
#define count_type(type, point) d_count_type(type, &(point)->data)
#define count_all(point) d_count_all(&(point)->data)


/*
 * free var, frees data of this type.
 */
void d_remove_var(s_entry * type, const char *vn, data_head * d);

#define remove_var(type, vn, point) d_remove_var(type, vn, &(point)->data)
#define remove(type, point) remove_var(type, NULL, point)

/*
 * free_all
 * Free all data in the data_head.
 */
void d_remove_all(data_head * d);

#define remove_all(point) d_remove_all(&(point)->data)

/*
 * copy_all
 * Copy a full data_head set.
 */
void d_copy_all(data_head * from, data_head * to);

#define copy_all(from, to) d_copy_all(&(from)->data, &(to)->data)


#endif
