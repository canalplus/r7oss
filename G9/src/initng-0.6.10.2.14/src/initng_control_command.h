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

#ifndef CONTROL_COMMAND_H
#define CONTROL_COMMAND_H

#include <stdarg.h>
#include "initng_list.h"

/*
 * This sorts of type, the command retuns
 * when calling it.
 */
typedef enum
{
	COMMAND_FAIL = 0,
	PAYLOAD_COMMAND = 1,
	VOID_COMMAND = 2,
	INT_COMMAND = 3,
	TRUE_OR_FALSE_COMMAND = 4,
	STRING_COMMAND = 5,
} e_com_type;

/*
 * This defines how visible the command is 
 * for the external user.
 */
typedef enum
{
	STANDARD_COMMAND = 0,		/* shown by ngc -h */
	ADVANCHED_COMMAND = 1,					/* shown by ngc -H */
	HIDDEN_COMMAND = 2,						/* not shown by ngc -h/-H but still usable */
	INTERNAL_COMMAND = 3					/* not shown or usable externaly */
} e_opt_vissible;

/*
 * Defines if command requires an option or not 
 */
typedef enum
{
	NO_OPT = 0,					/* Command uses no option ever */
	USES_OPT = 1,							/* Command uses an option if sent */
	REQUIRES_OPT = 2						/* Command requires an option set */
} e_opt_type;


/*
 * If data sent, are in an unknown binary data type,
 * its put as an payload, here an struct there we got a
 * pointer to a payload (payload->p), and specify size of it in (payload->s)
 */
typedef struct
{
	size_t s;
	void *p;
} s_payload;


/*
 * This is a structure of an command.
 */
typedef struct
{
	char command_id;			/* An short char describes a command, like ngc -h */
	const char *long_id;		/* An long string, describes a command, like ngc --help */
	e_com_type com_type;		/* Specify how data command returns looks like */
	e_opt_vissible opt_vissible;	/* Specifys how accesible the command is for the end user */
	e_opt_type opt_type;		/* Is an option required to the command */

	/*
	 * Here we put a pointer to a function,
	 * that is the command we call
	 */
	union
	{
		void (*void_command_call) (void *data);
		void (*void_command_void_call) (void);
		int (*int_command_call) (void *data);
		int (*int_command_void_call) (void);
		char *(*string_command_call) (void *data);
		char *(*string_command_void_call) (void);
		void (*data_command_call) (char *data, s_payload * payload);
	} u;

	/*
	 * An string, with an descripton of what this command is for.
	 * This is used for ngc -h, to describe the commands.
	 * It may be null.
	 */
	const char *description;

	/* Reserved for initng */
	struct list_head list;
} s_command;

/* register */
int initng_command_register(s_command * cmd);

#define initng_command_unregister(cmd) list_del(&(cmd)->list)
void initng_command_unregister_all(void);

/* find */
s_command *initng_command_find_by_command_id(char cid);
s_command *initng_command_find_by_command_string(char *name);

/* walk */
#define while_command_db(current) list_for_each_entry_prev(current, &g.command_db.list, list)
#define while_command_db_safe(current, safe) list_for_each_entry_prev_safe(current, safe, &g.command_db.list, list)

/* global functions */
int initng_command_execute_arg(char cid, char *arg);

#define initng_command_execute(c) initng_command_execute_arg(c, NULL)

#endif
