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
#include <stdarg.h>
#include <time.h>
#include <assert.h>

#include "initng_error.h"

#include "initng_global.h"
#include "initng_toolbox.h"

#include "initng_static_event_types.h"

int lock_error_printing = 0;

static void initng_failsafe_print_error(e_mt mt, const char *file,
										const char *func, int line,
										const char *format, va_list arg)
{
	struct tm *ts;
	time_t t;

	t = time(0);
	ts = localtime(&t);


	switch (mt)
	{
		case MSG_FAIL:
			if (file && func)
				fprintf(stderr,
						"\n\n FAILSAFE ERROR ** \"%s\", %s() line %i:\n",
						file, func, line);
			fprintf(stderr, " %.2i:%.2i:%.2i -- FAIL:\t", ts->tm_hour,
					ts->tm_min, ts->tm_sec);
			break;
		case MSG_WARN:
			if (file && func)
				fprintf(stderr,
						"\n\n FAILSAFE ERROR ** \"%s\", %s() line %i:\n",
						file, func, line);
			fprintf(stderr, " %.2i:%.2i:%.2i -- WARN:\t", ts->tm_hour,
					ts->tm_min, ts->tm_sec);
			break;
		default:
			break;
	}

	vfprintf(stderr, format, arg);

}

int initng_error_print(e_mt mt, const char *file, const char *func, int line,
					   const char *format, ...)
{
	assert(file);
	assert(func);
	assert(format);

	int delivered = FALSE;
	va_list arg;

	/* This lock is to make sure we don't get into an endless print loop */
	if (lock_error_printing == 1)
		return (0);

	/* put the lock, to avoid a circular bug */
	lock_error_printing = 1;

	/* start the variable list */
	va_start(arg, format);

	/* check for hooks */
	{
		s_event event;
		s_event_error_message_data data;

		event.event_type = &EVENT_ERROR_MESSAGE;
		event.data = &data;
		data.mt = mt;
		data.file = file;
		data.func = func;
		data.line = line;
		data.format = format;

		va_copy(data.arg, arg);

		if (initng_event_send(&event) == TRUE)
			delivered = TRUE;

		va_end(data.arg);
	}

	/* Print on failsafe if no hook is to listen. */
	if (!delivered)
	{
		va_list pass;

		va_copy(pass, arg);

		initng_failsafe_print_error(mt, file, func, line, format, pass);

		va_end(pass);
	}

	va_end(arg);

	lock_error_printing = 0;
	return (TRUE);
}



/*      ***************           Below are only extra prints, used with DEBUG defined!    ***********************   */



#ifdef DEBUG
static int msgs = 0;
static const char *last_file = NULL;
static const char *last_func = NULL;

static void initng_verbose_print(void)
{
	int i;

	if (lock_error_printing == 1)
		return;

	W_("This words will i look for in debug: \n");
	for (i = 0; i < MAX_VERBOSES; i++)
		if (g.verbose_this[i])
			W_("  * \"%s\"\n", g.verbose_this[i]);
}

int initng_error_verbose_add(const char *string)
{
	int i = 0;

	if (g.verbose == 1)
		g.verbose = 3;
	else
		g.verbose = 2;


	while (g.verbose_this[i] && i < MAX_VERBOSES)
		i++;

	if (i >= MAX_VERBOSES - 1)
	{
		F_("Can't add another \"%s\" debug trace\n", string);
		return (FALSE);
	}

	g.verbose_this[i] = i_strdup(string);

	initng_verbose_print();

	if (g.verbose_this[i])
		return (TRUE);
	return (FALSE);
}

int initng_error_verbose_del(const char *string)
{
	int i;
	int t = FALSE;

	for (i = 0; i < MAX_VERBOSES; i++)
		if (g.verbose_this[i] && strcasecmp(g.verbose_this[i], string) == 0)
		{
			free(g.verbose_this[i]);
			g.verbose_this[i] = NULL;
			t = TRUE;
		}
	initng_verbose_print();
	return (t);
}

void initng_error_print_func(const char *file, const char *func)
{
	int i = 0;

	if (lock_error_printing == 1)
		return;
	lock_error_printing = 1;

	if (g.verbose == 2 || g.verbose == 3)
	{
		for (i = 0; i < MAX_VERBOSES; i++)
		{
			if (g.verbose_this[i])
			{
				if (g.verbose_this[i][0] == '%' && (g.verbose_this[i] + 1))
				{
					if (strcasestr(file, (g.verbose_this[i]) + 1) ||
						strcasestr(func, (g.verbose_this[i]) + 1))
					{
						lock_error_printing = 0;
						return;
					}
				}
				else
				{
					if (strcasestr(file, g.verbose_this[i]) ||
						strcasestr(func, g.verbose_this[i]))
					{
						i = 1;
						break;
					}
				}
			}
		}
	}

	if (g.verbose == 1 || i == 1)
	{
		if (last_file != file || last_func != func)
		{
			fprintf(stderr, "\n\n ** \"%s\", %s():\n", file, func);
		}

		last_file = file;
		last_func = func;
	}

	lock_error_printing = 0;
}






int initng_error_print_debug(const char *file, const char *func, int line,
							 const char *format, ...)
{
	int done;
	struct tm *ts;
	int i;
	time_t t;


	assert(file);
	assert(func);

	if (lock_error_printing == 1)
		return (0);
	lock_error_printing = 1;



	if (g.verbose == 1)
		goto yes;

	if (g.verbose == 2 || g.verbose == 3)
		for (i = 0; i < MAX_VERBOSES; i++)
			if (g.verbose_this[i])
			{
				if (g.verbose_this[i][0] == '%' && (g.verbose_this[i] + 1))
				{
					if (strcasestr(file, (g.verbose_this[i]) + 1) ||
						strcasestr(func, (g.verbose_this[i]) + 1))
					{
						lock_error_printing = 0;
						return (TRUE);
					}
				}
				else
				{
					if (strcasestr(file, g.verbose_this[i]) ||
						strcasestr(func, g.verbose_this[i]))
					{
						goto yes;
					}
				}
			}

	if (g.verbose == 3)
		goto yes;
	/* else */

	lock_error_printing = 0;
	return (TRUE);

  yes:

	/* print the function name, if not set */
	if (last_file != file || last_func != func)
	{
		fprintf(stderr, "\n\n ** \"%s\", %s():\n", file, func);
	}

	last_file = file;
	last_func = func;

	if (g.i_am != I_AM_INIT && getpid() != 1)
	{
		fprintf(stderr, " [%i]: ", getpid());
	}

	/* Don't fetch time, until we know we wanna print on screen */
	t = time(0);
	ts = localtime(&t);


	fprintf(stderr, " %.2i:%.2i:%.2i -- l:%i\t", ts->tm_hour,
			ts->tm_min, ts->tm_sec, line);

	msgs++;
	if (msgs > 20)
	{
		sleep(0.5);
		msgs = 0;
	}

	va_list arg;

	va_start(arg, format);

	done = vfprintf(stderr, format, arg);

	va_end(arg);

	lock_error_printing = 0;
	return (done);
}

#endif
