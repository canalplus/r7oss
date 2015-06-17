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
   @file   pti_container.c
   @brief  Container Object Initialisation, Termination and Manipulation Functions.

   This file implements the functions for a container object.  A container object doesn't do much,
   it can be created, destroyed and associated with other objects without restriction.  It is
   intended as a container of information that is stored in the HAL but not used directly by the
   HAL.

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
ST_ErrorCode_t stptiHAL_ContainerAssociator(FullHandle_t ContainerHandle, FullHandle_t AssocObjectHandle);
ST_ErrorCode_t stptiHAL_ContainerDisassociator(FullHandle_t ContainerHandle, FullHandle_t AssocObjectHandle);

/* Public Constants ------------------------------------------------------- */

/* Export the API */
/* The Session concept is really just a way of group objects.  Nothing special needs to be done for
 * allocation / deallocation, and association / disassociation is not allowed. */
const stptiHAL_ContainerAPI_t stptiHAL_ContainerAPI = {
	{
	     /* Allocator  Associator                     Disassociator                      Deallocator */
		NULL,         stptiHAL_ContainerAssociator,  stptiHAL_ContainerDisassociator,   NULL,
		NULL,  NULL
	}
};

/* Object Management Functions ------------------------------------------------------------------ */

/**
   @brief  Associate a Container Object to another Object

   This function allows association of this container object with any other object with the exception
   Session or vDevice objects.

   @param  ContainerHandle      A handle for the container object
   @param  AssocObjectHandle    A handle for the object being associated

   @return                      A standard st error type...
                                - ST_NO_ERROR
 */
ST_ErrorCode_t stptiHAL_ContainerAssociator(FullHandle_t ContainerHandle, FullHandle_t AssocObjectHandle)
{
	/* Already write locked */
	ST_ErrorCode_t Error = ST_NO_ERROR;

	ContainerHandle = ContainerHandle;

	switch (stptiOBJMAN_GetObjectType(AssocObjectHandle)) {
	case OBJECT_TYPE_VDEVICE:
	case OBJECT_TYPE_SESSION:
		Error = ST_ERROR_BAD_PARAMETER;
		break;

	default:
		break;
	}

	return (Error);
}

/**
   @brief  Disassociate a Container from another Object.

   This function allows the user to disassociate from a container object.

   @param  SignalHandle         A handle for the buffer
   @param  AssocObjectHandle    A handle for the object being disassociated

   @return                      A standard st error type...
                                - ST_NO_ERROR
 */
ST_ErrorCode_t stptiHAL_ContainerDisassociator(FullHandle_t ContainerHandle, FullHandle_t AssocObjectHandle)
{
	/* Already write locked */
	ContainerHandle = ContainerHandle;
	AssocObjectHandle = AssocObjectHandle;

	return (ST_NO_ERROR);
}
