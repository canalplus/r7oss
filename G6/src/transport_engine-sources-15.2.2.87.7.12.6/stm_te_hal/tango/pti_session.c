/************************************************************************
Copyright (C) 2011 STMicroelectronics. All Rights Reserved.

This file is part of the Transport Engine Library.

The Transport Engine is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

Transport Engine is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with Transport Engine; see the file COPYING.  If not, write to the Free
Software Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307,
USA.

The Transport Engine Library may alternatively be licensed under a
proprietary license from ST.

************************************************************************/
/**
   @file   pti_session.c
   @brief  Session Object Initialisation, Termination and Manipulation Functions.

   This file implements the functions for creating, destroying and accessing a PTI Open/Close
   session.  Or at least it would do, if there was anything to do.

   An Open/Close session is used for allocating memory and allows the user to group together
   objects so that they can be terminated at once.

 */

#if 0
#define STPTI_PRINT
#endif

/* Includes ---------------------------------------------------------------- */

/* ANSI C includes */

/* STAPI includes */

/* Includes from API level */
#include "../pti_debug.h"
#include "../pti_hal_api.h"

/* Includes from the HAL / ObjMan level */

/* MACROS ------------------------------------------------------------------ */
/* Private Constants ------------------------------------------------------- */
/* Private Variables ------------------------------------------------------- */
/* Private Function Prototypes --------------------------------------------- */
/* Public Constants ------------------------------------------------------- */

/* Export the API */
/* The Session concept is really just a way of group objects.  Nothing special needs to be done for
 * allocation / deallocation, and association / disassociation is not allowed. */
const stptiHAL_SessionAPI_t stptiHAL_SessionAPI = {
	{
		/* Allocator  Associator  Disassociator  Deallocator Serialise Deserialise */
		 NULL, NULL, NULL, NULL, NULL, NULL
	}
};

/* Functions --------------------------------------------------------------- */
