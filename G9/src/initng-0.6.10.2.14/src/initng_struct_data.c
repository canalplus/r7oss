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
#include "initng_global.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "initng_struct_data.h"
#include "initng_static_data_id.h"
#include "initng_toolbox.h"

#undef D_
#define D_(fmt, ...)

static void d_dfree(s_data * current);

/* if this is an alias variable type, walk to find the correct one */
#define ALIAS_WALK \
    /* this might be an alias */ \
    while (type && type->opt_type == ALIAS && type->alias) \
        type = type->alias;


#define IT(x) (type->opt_type == x || type->opt_type == (x + 50))


/*
 * This function will return a s_data pointer, matching type, vn
 * by the adress struct list.
 */
s_data *d_get_next_var(s_entry * type, const char *vn, data_head * head,
					   s_data * last)
{
	assert(head);
	struct list_head *place = NULL;
	s_data *current = NULL;

	ALIAS_WALK;

	/*
	 * There is a problem, functions run get_next_var, and want all variables, not just
	 * the onse with a special vn, so searching for variables with an vn, shud be possible
	 * without setting an vn to search for
	 */
#ifdef HURT_ME
	/* Check that vn is sent, when needed */
	if (!vn && type && type->opt_type >= 50)
	{
		F_("The vn variable is missing for a type %i (%s): %s!\n",
		   type->opt_type, type->opt_name);
		return (NULL);
	}

	/* check that vn is not set, when not needed */
	if (vn && type && type->opt_type < 50)
	{
		F_("The vn %s is set, but not needed for type %i, %s\n", vn,
		   type->opt_type, type->opt_name);
		return (NULL);
	}
#endif

	/* Now do the first hook if set */
	if (head->data_request && (*head->data_request) (head) == FALSE)
		return (NULL);

	/* Make sure the list is not empty */
	if (!list_empty(&head->head.list))
		/* put place on the initial */
		place = head->head.list.prev;

	/* as long as we dont stand on the header */
	while (place && place != &head->head.list)
	{
		/* get the entry */
		current = list_entry(place, s_data, list);

		/* if last still set, fast forward */
		if (last && current != last)
		{
			place = place->prev;
			continue;
		}

		/* if this is the last entry */
		if (last == current)
		{
			last = NULL;
			place = place->prev;
			continue;
		}

		/* Make sure the string variable name matches if set */
		if ((!type || current->type == type)
			&& (!current->vn || !vn || strcasecmp(current->vn, vn) == 0))
		{
			return (current);
		}

		/* try next */
		place = place->prev;
	}


	/* This second hook might fill head->res */
	if (head->res_request && (*head->res_request) (head) == FALSE)
		return (NULL);

	/* if there is any resursive next to check, do that. */
	if (head->res)
	{
		/* ok return with that */
		return (d_get_next_var(type, vn, head->res, last));
	}

	/* no luck */
	return (NULL);
}

/* returns a string pointer */
inline const char *d_get_string_var(s_entry * type, const char *vn,
									data_head * head)
{
	s_data *current = d_get_next_var(type, vn, head, NULL);

	return (current ? current->t.s : NULL);
}


/* returns an int */
inline int d_get_int_var(s_entry * type, const char *vn, data_head * d)
{
	s_data *current = d_get_next_var(type, vn, d, NULL);

	return (current ? current->t.i : 0);
}


inline const char *d_get_next_string_var(s_entry * type, const char *vn,
										 data_head * d, s_data ** cur)
{
	/* Temporary store string pointer here */
	const char *to_ret;

	/* find next var */
	s_data *current = d_get_next_var(type, vn, d, *cur);

	/* Get the string path out of it */
	to_ret = current ? current->t.s : NULL;

	/* update to next */
	*cur = current;

	/* return string */
	return (to_ret);
}



/* this has to be only one of */
void d_set_string_var(s_entry * type, char *vn, data_head * d, char *string)
{
	s_data *current = NULL;

	assert(d);
	assert(string);

	if (!type)
	{
		F_("Type can't be zero!\n");
		return;
	}

	ALIAS_WALK;

	if (!vn && type->opt_type >= 50)
	{
		F_("The vn variable is missing for a type %i %s, trying to set \"%s\"!\n", type->opt_type, type->opt_name, string);
		return;
	}


	if (!IT(STRING))
	{
		F_(" \"%s\" is not an STRING type, sleeping 1 sec ..\n",
		   type->opt_name);
		sleep(1);
		return;
	}

	/* check the db, for an current entry to overwrite */
	if ((current = d_get_next_var(type, vn, d, NULL)))
	{
		if (current->t.s)
			free(current->t.s);
		if (vn)
			free(vn);
		current->t.s = string;
		return;
	}

	current = (s_data *) i_calloc(1, sizeof(s_data));
	current->type = type;
	current->t.s = string;
	current->vn = vn;

	/* add this one */
	list_add(&(current->list), &d->head.list);
}




/* this is a function to add a string, that it can exist many of */
void d_set_another_string_var(s_entry * type, char *vn, data_head * d,
							  char *string)
{
	s_data *current = NULL;

	assert(d);
	assert(string);

	if (!type)
	{
		F_("Type can't be zero!\n");
		return;
	}

	ALIAS_WALK;

	if (!vn && type->opt_type >= 50)
	{
		F_("The vn variable is missing for a type %i %s, trying to set string: \"%s\"!\n", type->opt_type, type->opt_name, string);
		return;
	}

	if (!IT(STRINGS))
	{
		F_(" \"%s\" is not an strings type!\n", type->opt_name);
		return;
	}

	current = (s_data *) i_calloc(1, sizeof(s_data));
	current->type = type;
	current->t.s = string;
	current->vn = vn;

	/* add this one */
	list_add(&(current->list), &d->head.list);
}

/* this has to be only one of */
void d_set_int_var(s_entry * type, char *vn, data_head * d, int value)
{
	s_data *current = NULL;

	assert(d);

	if (!type)
	{
		F_("Type can't be zero!\n");
		return;
	}

	ALIAS_WALK;

	if (!vn && type->opt_type >= 50)
	{
		F_("The vn variable is missing for a type %i %s!\n", type->opt_type,
		   type->opt_name);
		return;
	}


	if (!IT(INT))
	{
		F_(" \"%s\" is not an int type!\n", type->opt_name);
		return;
	}

	/* check the db, for an current entry to overwrite */
	if ((current = d_get_next_var(type, vn, d, NULL)))
	{
		current->t.i = value;
		return;
	}

	/* else create a new one */
	current = (s_data *) i_calloc(1, sizeof(s_data));
	current->type = type;
	current->t.i = value;
	current->vn = vn;

	/* add this one */
	list_add(&(current->list), &d->head.list);
}



/* 
 * d_is can be used to check any type, if its in the db,
 * or just to check unset and set types.
 */
inline int d_is_var(s_entry * type, const char *vn, data_head * d)
{
	s_data *current = d_get_next_var(type, vn, d, NULL);

	return (current ? TRUE : FALSE);
}



/* Walk through and count */
int d_count_type(s_entry * type, data_head * d)
{
	s_data *current = NULL;
	int count = 0;

	/* walk through all entries on address */
	list_for_each_entry(current, &d->head.list, list)
	{
		if (!type || current->type == type)
			count++;
	}

	return (count);
}


void d_set_var(s_entry * type, char *vn, data_head * d)
{
	s_data *current = NULL;

	assert(d);

	if (!type)
	{
		F_("Type can't be zero!\n");
		return;
	}

	ALIAS_WALK;

	if (!vn && type->opt_type >= 50)
	{
		F_("The vn variable is missing for a type %i %s!\n", type->opt_type,
		   type->opt_name);
		return;
	}

	if (!IT(SET))
	{
		F_("It has to be an SET type to d_set!\n");
		return;
	}

	/* check if its set already */
	if (d_is_var(type, vn, d))
		return;

	/* allocate the entry */
	current = (s_data *) i_calloc(1, sizeof(s_data));
	current->type = type;
	current->vn = vn;

	/* add this one */
	list_add(&(current->list), &d->head.list);
}


/*
 * A function to nicely free the s_data content.
 */
static void d_dfree(s_data * current)
{
	assert(current);
	assert(current->type);

	/* Unlink this entry from any list */
	list_del(&current->list);

	/* free variable data */
	switch (current->type->opt_type)
	{
		case STRING:
		case STRINGS:
		case VARIABLE_STRING:
		case VARIABLE_STRINGS:
			if (current->t.s)
				free(current->t.s);
			current->t.s = NULL;
			break;
		default:
			break;
	}

	/* free variable name, if set */
	if (current->vn)
		free(current->vn);
	current->vn = NULL;

	/* ok, free the struct */
	free(current);
}


void d_remove_var(s_entry * type, const char *vn, data_head * d)
{
	s_data *current;

	assert(d);
	assert(type);

	if (!type)
	{
		F_("Type can't be zero!\n");
		return;
	}

	/* for every matching, free it */
	while ((current = d_get_next_var(type, vn, d, NULL)))
	{
		d_dfree(current);
	}
}



void d_remove_all(data_head * d)
{
	s_data *current = NULL;
	s_data *s = NULL;

	assert(d);

	/* walk through all entries on address */
	list_for_each_entry_safe(current, s, &d->head.list, list)
	{
		/* walk, and remove all */
		d_dfree(current);
		current = NULL;
	}

	/* make sure its cleared */
	DATA_HEAD_INIT(d);
}


/*
 * Walk the db, copy all strings in list from to to.
 */
void d_copy_all(data_head * from, data_head * to)
{
	s_data *tmp = NULL;
	s_data *current = NULL;

	list_for_each_entry(current, &from->head.list, list)
	{
		/* make sure type is set
		   TODO, should this be an assert(current->type) ??? */
		if (!current->type)
			continue;

		/* allocate the new one */
		tmp = (s_data *) i_calloc(1, sizeof(s_data));

		/* copy */
		memcpy(tmp, current, sizeof(s_data));

		/* copy the data */
		switch (current->type->opt_type)
		{
			case STRING:
			case STRINGS:
			case VARIABLE_STRING:
			case VARIABLE_STRINGS:
				if (current->t.s)
					tmp->t.s = i_strdup(current->t.s);
				break;
			default:
				break;
		}

		/* copy variable name */
		if (current->vn)
			tmp->vn = i_strdup(current->vn);
		else
			tmp->vn = NULL;

		/* add to list */
		list_add(&tmp->list, &to->head.list);
	}
}
