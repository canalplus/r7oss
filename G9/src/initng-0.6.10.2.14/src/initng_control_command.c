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
#define _GNU_SOURCE
#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "initng_control_command.h"

#include "initng_global.h"
#include "initng_toolbox.h"

/* this adds a command to the global command struct */
int initng_command_register(s_command * cmd)
{
	s_command *current = NULL;

	assert(cmd);

	/* look for duplicates */
	while_command_db(current)
	{
		if (current == cmd || current->command_id == cmd->command_id)
		{
			F_("Can't add command: %c, %s, it exists already!\n",
			   current->command_id, current->description);
			return (FALSE);
		}
	}

	/* add this command to list */
	list_add(&cmd->list, &g.command_db.list);
	return (TRUE);
}

void initng_command_unregister_all(void)
{
	s_command *current, *safe = NULL;

	while_command_db_safe(current, safe)
	{
		initng_command_unregister(current);
	}
}

/* look for a command by command_id */
s_command *initng_command_find_by_command_id(char cid)
{
	s_command *current = NULL;

	while_command_db(current)
	{
		if (current->command_id == cid)
			return (current);
	}
	return (NULL);
}

/* look for a command by command_id */
s_command *initng_command_find_by_command_string(char *name)
{
	s_command *current = NULL;

	while_command_db(current)
	{
		if (current->long_id && strcmp(current->long_id, name) == 0)
			return (current);
	}
	return (NULL);
}

int initng_command_execute_arg(char cid, char *arg)
{
	/* use function about to search it */
	s_command *cmd = initng_command_find_by_command_id(cid);

	/* make sure it found it. */
	if (!cmd)
	{
		D_("Did not find command %c.\n", cid);
		return (FALSE);
	}

	/* check so its a INT_COMMAND */
	if (cmd->com_type != INT_COMMAND && cmd->com_type != VOID_COMMAND
		&& cmd->com_type != TRUE_OR_FALSE_COMMAND)
	{
		W_("Command %c is not an INT_COMMAND, VOID_COMMAND or TRUE_OR_FALSE_COMMAND.\n", cid);
		return (FALSE);
	}

	/* Check so the call is actually there */

	if (!cmd->u.int_command_void_call)
	{
		W_("Command %c missing u.int_command_call or u.int_command_void_call.\n", cid);
		return (FALSE);
	}


	/* check with arguments */
	if (!arg && cmd->opt_type == REQUIRES_OPT)
	{
		F_("Command %c needs an opt!\n", cid);
		return (FALSE);
	}
	if (arg && cmd->opt_type == NO_OPT)
	{
		F_("Command %c don't uses any options!\n", cid);
		return (FALSE);
	}

	/* now start executing */
	if (cmd->com_type == INT_COMMAND
		|| cmd->com_type == TRUE_OR_FALSE_COMMAND)
	{
		/* execute the command */
		if (arg)
			return ((*cmd->u.int_command_call) (arg));
		/* else */
		return ((*cmd->u.int_command_void_call) ());
	}

	/* else its a VOID command, that wont give us any to return. */
	/* execute the command */
	if (arg)
		(*cmd->u.void_command_call) (arg);
	else
		(*cmd->u.void_command_void_call) ();
	return (TRUE);
}
