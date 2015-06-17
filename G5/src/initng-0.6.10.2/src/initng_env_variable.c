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

/* common standard first define */
#include "initng.h"

/* system headers */
#include <time.h>							/* time() */
#include <fcntl.h>							/* fcntl() */
#include <sys/un.h>							/* memmove() strcmp() */
#include <sys/wait.h>						/* waitpid() sa */
#include <linux/kd.h>						/* KDSIGACCEPT */
#include <sys/ioctl.h>						/* ioctl() */
#include <stdio.h>							/* printf() */
#include <stdlib.h>							/* free() exit() */
#include <sys/reboot.h>						/* reboot() RB_DISABLE_CAD */
#include <assert.h>
#include <ctype.h>							/* isgraph */

/* own header */
#include "initng_env_variable.h"

/* local headers include */
#include "initng_main.h"
#include "initng_active_db.h"
#include "initng_load_module.h"
#include "initng_plugin_callers.h"
#include "initng_common.h"
#include "initng_toolbox.h"
#include "initng_string_tools.h"
#include "initng_global.h"

#include <initng-paths.h>

const char *initng_environ[] = {
	"INITNG=" INITNG_VERSION,
	"INITNG_CREATOR=" INITNG_CREATOR,
	"INIT_VERSION=" INITNG_VERSION,
	"INITNG_PLUGIN_DIR=" INITNG_PLUGIN_DIR,
	"INITNG_ROOT=" INITNG_ROOT,
	"PATH=/bin:/usr/bin:/sbin:/usr/sbin:/usr/local/bin:/opt/bin",
	"HOME=/root",
	"USER=root",
	"TERM=linux",
#ifdef FORCE_POSIX_IFILES
	"POSIXLY_CORRECT=1",
#endif
	NULL
};

/* creates a custom set of environment variables, for passing to exec
 * Warning: this leaks memory, so only use if you are going to exec()!
 * Don't even *think* about free()ing the strings, unless you want
 * invalid pointers in the service_db! (easy to fix, see comment below)
 */
char **new_environ(active_db_h * s)
{
	int allocate;
	int nr = 0;
	int i;
	char **env;
	char *var;

	/* Better safe than sorry... */
	if (s && s->name == NULL)
		s->name = i_strdup("unknown");

	/*
	 * FIRST, try to figure out how big array we want to create.
	 */

	/*  At least 11 allocations below, and place for plugin added to (about 100) */
	allocate = 114;

	/* count existing env's */
#ifdef USE_OLD_ENV							/* DO NOT USE - BROKEN */
	while (environ[nr])
		allocate++;
#endif

	/* count ENVs in service ENV variable */
	if (s)
		allocate += count_type(&ENV, s);


	/* finally allocate */
	env = (char **) i_calloc(allocate, sizeof(char *));

	/* duplicate */
#ifdef USE_OLD_ENV
	for (nr = 0; environ[nr] && nr < allocate; nr++)
	{
		env[nr] = i_strdup(environ[nr]);
	}
#endif

	/* add all static defined above in initng_environ */
	for (nr = 0; initng_environ[nr]; nr++)
	{
		env[nr] = i_strdup(initng_environ[nr]);
	}


	if (s && (nr + 4) < allocate)
	{
		env[nr] = (char *) i_calloc(1, sizeof(char) * (9 + strlen(s->name)));
		strcpy(env[nr], "SERVICE=");
		strcat(env[nr], s->name);
		nr++;

		env[nr] = (char *) i_calloc(1, sizeof(char) * (6 + strlen(s->name)));
		strcpy(env[nr], "NAME=");
		strcat(env[nr], st_strip_path(s->name));
		nr++;

		env[nr] = (char *) i_calloc(1, sizeof(char) * (7 + strlen(s->name)));
		strcpy(env[nr], "CLASS=");
		strcat(env[nr], s->name);
		st_strip_end(&env[nr]);
		nr++;

		if (g.dev_console)
		{
			env[nr] = (char *) i_calloc(1,
										sizeof(char) * (9 +
														strlen(g.
															   dev_console)));
			strcpy(env[nr], "CONSOLE=");
			strcat(env[nr], g.dev_console);
			nr++;
		}
		else
		{
			env[nr] = (char *) i_calloc(1,
										sizeof(char) * (9 +
														strlen
														(INITNG_CONSOLE)));
			strcpy(env[nr], "CONSOLE=");
			strcat(env[nr], INITNG_CONSOLE);
			nr++;
		}

		if (g.runlevel && (nr + 1) < allocate)
		{
			env[nr] = (char *) i_calloc(1,
										sizeof(char) * (10 +
														strlen(g.runlevel)));
			strcpy(env[nr], "RUNLEVEL=");
			strcat(env[nr], g.runlevel);
			nr++;
		}

		if (g.old_runlevel && (nr + 1) < allocate)
		{
			env[nr] = (char *) i_calloc(1,
										sizeof(char) * (14 +
														strlen(g.
															   old_runlevel)));
			strcpy(env[nr], "PREVLEVEL=");
			strcat(env[nr], g.old_runlevel);
			nr++;
		}

		env[nr] = NULL;
		/* insert all env strings from config */
		{
			var = NULL;
			s_data *tmp = NULL;

			while ((nr + 1) < allocate)
			{
				char *fixed = NULL;

				if (!(tmp = get_next(&ENV, s, tmp)))
					break;

				fixed = fix_variables(tmp->t.s, s);

				/* then malloc */
				var = i_calloc((strlen(tmp->vn) + strlen(fixed) + 3),
							   sizeof(char));
				if (!var)
					continue;

				/* copy the data */
				strcpy(var, tmp->vn);
				strcat(var, "=");
				strcat(var, fixed);

				/* free the fixed ones */
				fix_free(fixed, tmp->t.s);

				/* check for duplicates */
				for (i = 0; i < nr; i++)
				{
					if (is_same_env_var(env[i], var))
					{
						/* we may want to override environemntal variables
						   set above, particularly PATH and HOME */
						free(env[i]);
						env[i] = var;
						break;
					}
				}

				if (i == nr)
					env[nr++] = var;
			}
		}
	}

	/* null last */
	if (env[nr] != NULL)
		env[nr++] = NULL;

#ifdef DEBUG
	for (nr = 0; env[nr]; nr++)
	{
		D_("environ[%i] = \"%s\"\n", nr, env[nr]);
	}
#endif

	/* return new environ */
	return (env);
}

/* this frees an environment variable - not to be used on the output of
 * new_environ!
 */
void free_environ(char **tf)
{
	int i = 0;

	D_("free_environ();\n");
	assert(tf);
	for (i = 0; tf[i]; tf++)
	{
		free(tf[i]);
	}
	free(tf);
}

int is_same_env_var(char *var1, char *var2)
{
	int i = 0;

	if (!var1 || !var2)
		return 0;							/* bad error checking in caller! */

	for (i = 0; var1[i] && var2[i] && var1[i] != '=' && var1[i] == var2[i];
		 i++)
		;

	return var1[i] == var2[i];
}

/* Handles redefinitions like "env FOO = $FOO bar" */
char *fix_redefined_variable(const char *name, const char *oldval,
							 const char *newdef)
{
	char *to;					/* this is the pointer we are going to return */
	char *set;					/* pointer to step */

	assert(name);
	assert(oldval);
	assert(newdef);

	/* make sure we got any data */
	if (!newdef)
		return (NULL);

	/* allocate that much memory that we think we need */
	/* TODO, add checks that this value is never overridden */
	to = (char *) i_calloc((strlen(newdef) + 150), sizeof(char));
	if (!to)
		return (NULL);

	set = to;
	/* while there is data to parse */
	while (newdef[0])
	{
		int len = 0;

		/*
		 * if its not containing the magical char loop, just copy the
		 * char and continue the main loop.
		 */
		if (newdef[0] != '$')
		{
			set[0] = newdef[0];
			newdef++;
			set++;
			set[0] = '\0';
			continue;
		}


		/* jump the '$' char */
		newdef++;
		if (!newdef[0])
			continue;

		/*
		 * Calculate length of variable ${VARIABLE}.
		 */
		if (newdef[0] == '{')				/* if this is a ${VARIABLE} */
		{
			newdef++;
			for (; newdef[len] && newdef[len] != '}'; len++) ;
		}
		else								/* else a $VARIABLE */
		{
			for (; isgraph(newdef[len]); len++) ;
		}

		if (strncasecmp(newdef, name, len) == 0 && (int) strlen(name) == len)
		{
			strcpy(set, oldval);
			/* go forward */
			newdef += len;
			if (newdef[0] == '}')
				newdef++;
			while (set[0])
				set++;
		}
		else
		{
			if (newdef[-1] == '{')
			{
				strncpy(set, newdef - 2, len + 2);
				newdef += len;
				set += len + 2;
				set[0] = '\0';
			}
			else
			{
				strncpy(set, newdef - 1, len + 1);
				newdef += len;
				set += len + 1;
				set[0] = '\0';
			}
		}
	}

	/* make sure end is terminated */
	set[0] = '\0';

	/* return the string we now fixed */
	return (to);

}

/* use a macro, so this works on any data_head target */
#define get_data_env(from, len, serv) get_data_env_int(from, len, &(serv)->data)
static const char *get_data_env_int(const char *from, int len, data_head * s)
{
	s_data *tmp = NULL;

	/*D_("get_data_env_int(%s, %i)\n", from, len); */

	while ((tmp = d_get_next(&ENV, s, tmp)))
	{
		/* then do a full match */
		if (strncasecmp(from, tmp->vn, len) == 0 && tmp->vn[len] == 0)
		{
			/*D_("match: %s\n", tmp->t.s); */
			return (tmp->t.s);
		}
	}
	return (NULL);
}

static const char *get_system_env(const char *from, int len)
{
	int i;

	if (g.runlevel && strncasecmp(from, "CONSOLE", len) == 0)
	{
		return (g.dev_console);
	}

	if (g.runlevel && strncasecmp(from, "RUNLEVEL", len) == 0)
	{
		return (g.runlevel);
	}

	if (g.runlevel && strncasecmp(from, "PREVLEVEL", len) == 0)
	{
		return (g.old_runlevel);
	}

	/* Fetch variable from initng_environ */
	for (i = 0; initng_environ[i]; i++)
	{
		if (strncasecmp(from, initng_environ[i], len) == 0
			&& initng_environ[i][len] == '=')
		{
			return (&initng_environ[i][len + 1]);
		}
	}
	return (NULL);
}

/* This function will change variables in from, like $NAME, $SERVICE, $CLASS
 * into its right content, and return a new allocated string with the result.
 */

/* WARNING, fix_variables will return from pointer, if no $ was found, so
 * when freeing you must do a if(got!=from) free(got) */
char *fix_variables(const char *from, active_db_h * s)
{
	char *ret = NULL;

	assert(s);
	assert(s->name);

	/* make sure we got any data */
	if (!from)
		return (NULL);

	/* example string to handle:
	 * "Hello $name!"
	 * "Hello ${name}!"
	 */

	/* go to the first '$' */
	const char *var_point = strchr(from, '$');


	/* If there is no variables here, return the dup */
	if (!var_point)
		return ((char *) from);

	/* check if its escaped, if it is, take next */
	while (var_point && var_point != from && var_point[-1] == '\\')
		var_point = strchr(var_point + 1, '$');

	/* If there is no variables here, return the dup */
	if (!var_point)
		return ((char *) from);

	/*printf("Fix2: \"%s\"\n", from);
	   sleep(1); */


	/*D_("fix_variables(%s): %s\n", s->name, from); */

	const char *var_start;
	const char *var_stop;
	int var_len = 0;


	/* get arg len */
	if (var_point[1] == '{')
	{
		/* walk to the char after the '{' */
		var_start = var_point + 2;
		/* simple cont chars to the first '}' */
		while (var_start[var_len] && var_start[var_len] != '}')
			var_len++;

	}
	else
	{
		var_start = var_point + 1;
		/* simple cont chars to the first ' ' */
		while (var_start[var_len] && var_start[var_len] != ' ')
			var_len++;
	}

	/* set the point from orgin where variable stops */
	var_stop = var_start + var_len;
	if (var_point[1] == '{')
		var_stop++;							/* then it must be an '}' ending */

	/* now we have set to compare with arg_len chars */

	/* if we got any to compare */
	if (var_len < 1)
	{
		F_("No var length.\n");
		return (NULL);
	}

	/* the replacement */
	const char *rep = NULL;
	int rep_len = 0;

	/*
	 * Start comparing keywords.
	 */

	if (strncasecmp(var_start, "NAME", var_len) == 0)
	{
		rep = st_strip_path(s->name);
	}

	else if (strncasecmp(var_start, "SERVICE", var_len) == 0)
	{
		rep = s->name;
	}

	else if (strncasecmp(var_start, "CLASS", var_len) == 0)
	{
		rep = s->name;
		/* go to last char of name */
		while (rep[rep_len])
			rep_len++;

		/* then go back to the last '/' */
		while (rep_len > 1 && rep[rep_len] != '/')
			rep_len--;

	}

	if (!rep)
		rep = get_system_env(var_start, var_len);

	if (!rep)
		rep = get_data_env(var_start, var_len, s);


	if (!rep)
		rep = "";

	/* make sure we dont have an cirular */
	{
		const char *check = rep;

		while ((check = strchr(check, '$')))
		{
			/* skip the '$' */
			check++;
			/* and the first '{' if present */
			if (check[0] == '{')
				check++;

			/* if conteht after the $ char, is the same as var_start */
			if (strncasecmp(check, var_start, var_len) == 0)
			{
				F_("Circular ENV variables found, this wont work!\n");
				return (NULL);
			}
		}
	}


	/* get len if not set */
	if (!rep_len)
		rep_len = strlen(rep);

	/* so, finished finding replacement, now realloc to new size */
	ret = i_calloc(strlen(from) - var_len + rep_len + 1, sizeof(char));
	if (!ret)
		return (NULL);

	/* start fill in our new replacement */
	strncpy(ret, from, (size_t) (var_point - from));	/* leng to the first $ char */
	strncat(ret, rep, rep_len);				/* the replacement word */
	strcat(ret, var_stop);					/* the rest of the string */

	/*printf("Got: \"%s\"\n", ret); */

	/* check if there is more of '$' chars */
	if (strchr(ret, '$'))
	{
		char *again = fix_variables(ret, s);

		if (again && ret != again)
		{
			free(ret);
			return (again);
		}
	}

	/* return what we got */
	return (ret);
}

#ifdef SERVICE_CACHE
/* this shud be a complete copy of fix_variables, but with a service_cache_h instead of a active_h to use */
/* WARNING, fix_variables will return from pointer, if no $ was found, so
 * when freeing you must do a if(got!=from) free(got) */
char *fix_variables2(const char *from, service_cache_h * s)
{
	char *ret = NULL;

	assert(s);
	assert(s->name);

	/* make sure we got any data */
	if (!from)
		return (NULL);

	/* example string to handle:
	 * "Hello $name!"
	 * "Hello ${name}!"
	 */

	/* go to the first '$' */
	const char *var_point = strchr(from, '$');


	/* If there is no variables here, return the dup */
	if (!var_point)
		return ((char *) from);

	/*printf("Fix: \"%s\"\n", from);
	   sleep(1); */

	/* check if its escaped, if it is, take next */
	while (var_point && var_point != from && var_point[-1] == '\\')
		var_point = strchr(var_point + 1, '$');

	/* If there is no variables here, return the dup */
	if (!var_point)
		return ((char *) from);


	/*D_("fix_variables(%s): %s\n", s->name, from); */

	const char *var_start;
	const char *var_stop;
	int var_len = 0;


	/* get arg len */
	if (var_point[1] == '{')
	{
		/* walk to the char after the '{' */
		var_start = var_point + 2;
		/* simple cont chars to the first '}' */
		while (var_start[var_len] && var_start[var_len] != '}')
			var_len++;

	}
	else
	{
		var_start = var_point + 1;
		/* simple cont chars to the first ' ' */
		while (var_start[var_len] && var_start[var_len] != ' ')
			var_len++;
	}

	/* set the point from orgin where variable stops */
	var_stop = var_start + var_len;
	if (var_point[1] == '{')
		var_stop++;							/* then it must be an '}' ending */

	/* now we have set to compare with arg_len chars */

	/* if we got any to compare */
	if (var_len < 1)
	{
		F_("No var length.\n");
		return (NULL);
	}

	/* the replacement */
	const char *rep = NULL;
	int rep_len = 0;

	/*
	 * Start comparing keywords.
	 */

	if (strncasecmp(var_start, "NAME", var_len) == 0)
	{
		rep = st_strip_path(s->name);
	}

	else if (strncasecmp(var_start, "SERVICE", var_len) == 0)
	{
		rep = s->name;
	}

	else if (strncasecmp(var_start, "CLASS", var_len) == 0)
	{
		rep = s->name;
		/* go to last char of name */
		while (rep[rep_len])
			rep_len++;

		/* then go back to the last '/' */
		while (rep_len > 1 && rep[rep_len] != '/')
			rep_len--;

	}

	if (!rep)
		rep = get_system_env(var_start, var_len);

	if (!rep)
		rep = get_data_env(var_start, var_len, s);


	if (!rep)
		rep = "";

	/* make sure we dont have an cirular */
	{
		const char *check = rep;

		while ((check = strchr(check, '$')))
		{
			/* skip the '$' */
			check++;
			/* and the first '{' if present */
			if (check[0] == '{')
				check++;

			/* if conteht after the $ char, is the same as var_start */
			if (strncasecmp(check, var_start, var_len) == 0)
			{
				F_("Circular ENV variables found, this wont work!\n");
				return (NULL);
			}
		}
	}


	/* get len if not set */
	if (!rep_len)
		rep_len = strlen(rep);

	/* so, finished finding replacement, now realloc to new size */
	ret = i_calloc(strlen(from) - var_len + rep_len + 1, sizeof(char));
	if (!ret)
		return (NULL);

	/* start fill in our new replacement */
	strncpy(ret, from, (size_t) (var_point - from));	/* leng to the first $ char */
	strncat(ret, rep, rep_len);				/* the replacement word */
	strcat(ret, var_stop);					/* the rest of the string */

	/*printf("Got: \"%s\"\n", ret); */

	/* check if there is more of '$' chars */
	if (strchr(ret, '$'))
	{
		char *again = fix_variables2(ret, s);

		if (again && again != ret)
		{
			free(ret);
			return (again);
		}
	}

	/* return what we got */
	return (ret);
}
#endif



























#ifdef DISABLED_TODO_CODE
		/* Get the variable from the data set on an initng_variable */
{
	s_entry *entry = NULL;

	/* Copy the string, so that we can put an '\0' on the end */
	char *tmp = i_strndup(from, len);

	if (!tmp)
		goto print_error;

	/* search if a initng variable exits with that name */
	entry = initng_service_data_type_find(tmp);

	/* free the duped one */
	free(tmp);

	/* if we got an entry, and if the entry exits in this service */
	if (entry && is(entry, s))
	{
		/* Handle differently with different types */
		switch (entry->opt_type)
		{
				/* if it is a string, just strcpy in it */
			case STRING:
				{
					strcpy(set, get_string(entry, s));
					goto go_forward;
				}
			case STRINGS:
				{
					const char *tmp2 = NULL;
					s_data *itterator = NULL;

					while ((tmp2 = get_next_string(entry, s, &itterator)))
					{
						strcat(set, tmp2);
						strcat(set, " ");
					}
					if (tmp2)
						goto go_forward;
				}
				/* if it is an integer, sprintf in it */
			case INT:
				{
					sprintf(set, "%i", get_int(entry, s));
					goto go_forward;
				}
			default:
				{
					D_("Don't know how to set a variable with %s:%i\n",
					   entry->opt_name, entry->opt_type);
					break;
				}
		}
	}
}
#endif
