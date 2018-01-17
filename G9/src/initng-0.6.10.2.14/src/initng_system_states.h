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

#ifndef INITNG_SYSTEM_STATES
#define INITNG_SYSTEM_STATES
/* System states */
typedef enum
{
	STATE_NULL = 0,
	STATE_STARTING = 1,
	STATE_UP = 2,
	STATE_STOPPING = 3,
	STATE_ASE = 4,							/* All Services Exited */
	STATE_SERVICES_LOADED = 5,				/* This should only be changed by sys_state_services_loaded, that resets sysstate to last service afterwards. */
	STATE_EXIT = 6,
	STATE_RESTART = 7,
	STATE_SULOGIN = 8,
	STATE_HALT = 9,
	STATE_REBOOT = 10,
	STATE_EXECVE = 11,
	STATE_POWEROFF = 12,
} h_sys_state;

#endif
