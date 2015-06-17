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
#include <stdlib.h>							/* free() exit() */
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdarg.h>							/* vastart() vaend() */

#include "initng_toolbox.h"
#include "initng_global.h"

/* in init, a calloc must never fail! */
/*void *initng_calloc2(size_t nmemb, size_t size, const char *func, int line) */
void *i_calloc(size_t nmemb, size_t size)
{
	void *alloced;

	/*D_("function %s() line %i allocating %i bytes\n", func, line,
	   nmemb * size); */
	while ((alloced = calloc(nmemb, size)) == NULL)
	{
		F_("Out of memory, trying again in 1 second\n");
		sleep(1);
	}
	return alloced;
}

/* in init, a realloc must never fail! */
void *i_realloc2(void *ptr, size_t size, const char *func, int line)
{
	void *alloced;

	while ((alloced = realloc(ptr, size)) == NULL)
	{
		F_("%s(): line %i Out of memory, trying again in 1 second\n",
		   func, line);
		sleep(1);
	}
	return alloced;
}

/* in init, a strdup must never fail! */
char *i_strdup2(const char *s, const char *func, int line)
{
	char *alloced;

	while ((alloced = strdup(s)) == NULL)
	{
		F_("%s(): line %i Out of memory, trying again in 1 second\n",
		   func, line);
		sleep(1);
	}
	return alloced;
}

/* in init, a strndup must never fail! */
char *i_strndup2(const char *s, size_t n, const char *func, int line)
{
	char *alloced;

	while ((alloced = strndup(s, n)) == NULL)
	{
		F_("%s(): line %i Out of memory, trying again in 1 second\n",
		   func, line);
		sleep(1);
	}
	return alloced;
}

/* stolen from sysvinit */

/* Set the name of the process, so it looks good in "ps" output. */
/* The new name cannot be longer than the space available, which */
/* is the space for argv[] and env[]. */

/* returns the length of the new title, or 0 if it cannot be changed */

/* 
 * See discussion of "Changing argv[0] under Linux." on Linux-Kernel 
 * mailing list:
 * http://www.uwsg.iu.edu/hypermail/linux/kernel/0301.1/index.html#2368
 */
int initng_toolbox_set_proc_title(const char *fmt, ...)
{
	va_list ap;
	int len;
	char *buf;

	buf = i_calloc(1, g.maxproclen + 1);

	va_start(ap, fmt);
	len = vsnprintf(buf, (g.maxproclen * sizeof(char)), fmt, ap);
	va_end(ap);

	/* we can only set our name as big as the space available */
	if (g.maxproclen > len)
	{
		memset(g.Argv0, 0, g.maxproclen);	/* clear */
		strcpy(g.Argv0, buf);				/* copy */
		D_("g.Argv0: %s\n", g.Argv0);
	}
	else
	{
		len = 0;
		D_("Cant reset own argv[0].\n");
	}

	free(buf);
	return len;
}
