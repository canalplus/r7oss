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

#ifndef IS_STATE_H
#define IS_STATE_H


typedef enum
{
	IS_NEW = 0,
	IS_UP = 1,
	IS_DOWN = 2,
	IS_FAILED = 3,
	IS_STARTING = 4,
	IS_STOPPING = 5,
	IS_WAITING = 6
} e_is;

#define IS_MARK(serv, state) (serv && serv->current_state && serv->current_state == state)
#define IS_NEW(serv) (serv && serv->current_state && serv->current_state->is == IS_NEW)
#define IS_UP(serv) (serv && serv->current_state && serv->current_state->is == IS_UP)
#define IS_DOWN(serv) (!serv->current_state || (serv && serv->current_state && serv->current_state->is == IS_DOWN))
#define IS_FAILED(serv) (serv && serv->current_state && serv->current_state->is == IS_FAILED)
#define IS_STARTING(serv) (serv && serv->current_state && serv->current_state->is == IS_STARTING)
#define IS_STOPPING(serv) (serv && serv->current_state && serv->current_state->is == IS_STOPPING)
#define IS_WAITING(serv) (serv && serv->current_state && serv->current_state->is == IS_WAITING)

#endif
