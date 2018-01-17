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

#ifndef INITNG_ERROR
#define INITNG_ERROR

#include "initng_msg.h"

#ifdef DEBUG
int initng_error_verbose_add(const char *string);
int initng_error_verbose_del(const char *string);
int initng_error_print_debug(const char *file, const char *func, int line,
							 const char *format, ...);

#endif

int initng_error_print(e_mt mt, const char *file, const char *func, int line,
					   const char *format, ...);

void initng_error_print_func(const char *file, const char *func);
#endif
