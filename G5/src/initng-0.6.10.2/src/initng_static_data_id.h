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

#ifndef STATIC_DATA_ID_H
#define STATIC_DATA_ID_H
#include "initng_service_data_types.h"

/*extern s_entry NAME; */
extern s_entry USE;
extern s_entry NEED;
extern s_entry REQUIRE;
extern s_entry FROM_FILE;
extern s_entry ENV;
extern s_entry RESTARTING;

void initng_static_data_id_register_defaults(void);

#endif
