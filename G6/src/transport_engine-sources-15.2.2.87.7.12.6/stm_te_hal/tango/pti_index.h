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
   @file   pti_index.h
   @brief  Index Object Initialisation, Termination and Manipulation Functions.

   This file declares the object structure and access mechanism for the Index
   Object.

   This file should ONLY contain function prototypes and constants that are
   INTERNAL to the stptiHAL.  i.e. with the exception of the HAL object
   registration function in pti_driver.c, NOTHING outside of the tango
   directory should be referring to this file.

   Defines and enums relevent to the API of the HAL must be put into
   pti_hal_api.h and will be shared across all HALs.


 */

#ifndef _PTI_INDEX_H_
#define _PTI_INDEX_H_

/* Includes ---------------------------------------------------------------- */

/* ANSI C includes */

/* STAPI includes */

/* Includes from API level */
#include "../pti_hal_api.h"

/* Includes from the HAL / ObjMan level */
#include "../objman/pti_object.h"
#include "firmware/pti_tp_api.h"

/* Exported Types ---------------------------------------------------------- */

typedef struct {
	Object_t ObjectHeader;	/* Must be first in structure */
	int IndexerIndex;	/**< The index of the item relative to the start of the region for this vDevice */
} stptiHAL_Index_t;

/* Exported Function Prototypes -------------------------------------------- */
extern const stptiHAL_IndexAPI_t stptiHAL_IndexAPI;

void stptiHAL_IndexerTPEntryInitialise(volatile stptiTP_IndexerInfo_t *IndexerInfo_p, int IndexerIndex);

#define stptiHAL_GetObjectIndex_p(ObjectHandle)   ((stptiHAL_Index_t *)stptiOBJMAN_HandleToObjectPointer(ObjectHandle,OBJECT_TYPE_INDEX))

#endif /* _PTI_INDEX_H_ */
