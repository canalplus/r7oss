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
   @file   pti_session.h
   @brief  Session Object Initialisation, Termination and Manipulation Functions.

   This file implements the functions for creating, destroying and accessing a PTI Open/Close
   session.

   An Open/Close session is used for allocating memory and allows the user to group together
   objects so that they can be terminated at once.

   This file should ONLY contain function prototypes and constants that are
   INTERNAL to the stptiHAL.  i.e. with the exception of the HAL object
   registration function in pti_driver.c, NOTHING outside of the tango
   directory should be referring to this file.

   Defines and enums relevent to the API of the HAL must be put into
   pti_hal_api.h and will be shared across all HALs.

 */

#ifndef _PTI_SESSION_H_
#define _PTI_SESSION_H_

/* Includes ---------------------------------------------------------------- */

/* ANSI C includes */

/* STAPI includes */

/* Includes from API level */
#include "../pti_hal_api.h"

/* Includes from the HAL / ObjMan level */
#include "../objman/pti_object.h"

/* Exported Types ---------------------------------------------------------- */

typedef struct {
	Object_t ObjectHeader;	/* Must be first in structure */
} stptiHAL_Session_t;

/* Exported Function Prototypes -------------------------------------------- */
extern const stptiHAL_SessionAPI_t stptiHAL_SessionAPI;

#define stptiHAL_GetObjectSession_p(ObjectHandle)   ((stptiHAL_Session_t *)stptiOBJMAN_HandleToObjectPointer(ObjectHandle,OBJECT_TYPE_SESSION))

#endif /* _PTI_SESSION_H_ */
