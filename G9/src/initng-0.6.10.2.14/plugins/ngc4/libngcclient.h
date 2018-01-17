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

#ifndef LIBNGCCLIENT_H
#define LIBNGCCLIENT_H

#include "initng_ngc4.h"

typedef struct
{
	result_desc result;
	void *payload;
} reply;


#define ngcclient_send_short_command(c, o) ngcclient_send_command(c, NULL, o)
#define ngcclient_send_long_command(l, o)  ngcclient_send_command('\0', l, o)
reply *ngcclient_send_command(const char *path, const char c, const char *l,
							  const char *o);

extern const char *ngcclient_error;

char *ngcclient_reply_to_string(reply * rep, int ansi);


char *ngc_hlp(reply * rep, int ansi);
char *ngc_active_db(reply * rep, int ansi);
char *ngc_option_db(reply * rep, int ansi);
char *ngc_state_entry(reply * rep, int ansi);

/* fetch an ansi color on a is type */
const char *is_to_ansi(e_is is);

/* get an onstant string telling what the e_is no is */
const char *is_to_string(e_is is);

#endif
