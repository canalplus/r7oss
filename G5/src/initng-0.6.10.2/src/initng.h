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

#ifndef INITNG_H
#define INITNG_H

#ifdef HAVE_CONFIG_H
#   include <config.h>
#endif

#define _GNU_SOURCE
#include <stdio.h>							/* printf */
#include "initng_error.h"

/* Undef TRUE FALSE FAIL, so not compiled complains about redefining */
#ifdef TRUE
#undef TRUE
#endif

#ifdef FALSE
#undef FALSE
#endif

#ifdef FAIL
#undef FAIL
#endif


/* Now, define them */
#define TRUE    1
#define FALSE   0
#define FAIL    -1


/* Event-specific return values */
#define HANDLED 2


#ifdef _SVN_REF
#define INITNG_VERSION VERSION "+svn " _SVN_REF " " VERSION_NAME
#else
#define INITNG_VERSION VERSION " " VERSION_NAME
#endif

#define INITNG_CREATOR "Jimmy Wennlund <jimmy.wennlund@gmail.com>"

/* Add to this counter everytime the api changes, and plugins need to recompile */
#define API_VERSION 19

/* define this macro in start of every plugin to check api version */
#define INITNG_PLUGIN_MACRO int plugin_api_version = API_VERSION;

/* unset this in an production environment */
/* this is set in config.h */
/*#define DEBUG */

/* specify paths */
#define INITNG_EXT      ".i"

/* standard console */
#define INITNG_CONSOLE	"/dev/console"

/* set maximums */
#define MAX_VERBOSES    50
#define MAX_BLACKLIST   20

/* makes all services sleep this many microseconds before launching,
 * this will get initng time to register service */
#define ALL_USLEEP 1000

/*
 * Clean delay, wait this no of seconds after a service is down, before removing
 * the trace of it in memory.
 */
#define CLEAN_DELAY 60

/*
 * Temporary printf-replacement macros until a real logging system can be added.
 */

#define P_(fmt, ...) \
    initng_error_print(MSG, __FILE__, (const char*)__PRETTY_FUNCTION__, __LINE__, fmt, ## __VA_ARGS__)

#define F_(fmt, ...) \
    initng_error_print(MSG_FAIL, __FILE__, (const char*)__PRETTY_FUNCTION__, __LINE__, fmt, ## __VA_ARGS__)

#define W_(fmt, ...) \
    initng_error_print(MSG_WARN, __FILE__, (const char*)__PRETTY_FUNCTION__, __LINE__, fmt, ## __VA_ARGS__)

#ifdef DEBUG
#define V_ADD(to_verb) s_set_another_string(VERBOSE_THIS, to_verb)
#define D_(fmt, ...) \
    initng_error_print_debug(__FILE__, (const char*)__PRETTY_FUNCTION__, __LINE__, fmt, ## __VA_ARGS__)
#define S_ initng_error_print_func(__FILE__, (const char*)__PRETTY_FUNCTION__)
#else
#define D_(fmt, ...)
#define S_
#endif


/* standard defines for module inits and unload */
int module_init(int api_version);
void module_unload(void);

#endif
