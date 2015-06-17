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
   @file   pti_dataentry.h
   @brief  Data Entry Object Initialisation, Termination and Manipulation
   Functions.

   This file declares the object structure and access mechanism for the Data
   Entry Object.

   This file should ONLY contain function prototypes and constants that are
   INTERNAL to the stptiHAL.  i.e. with the exception of the HAL object
   registration function in pti_driver.c, NOTHING outside of the tango
   directory should be referring to this file.

   Defines and enums relevent to the API of the HAL must be put into
   pti_hal_api.h and will be shared across all HALs.

 */

#ifndef _PTI_DATA_ENTRY_H_
#define _PTI_DATA_ENTRY_H_

/* Includes ---------------------------------------------------------------- */

/* ANSI C includes */

/* STAPI includes */

/* Includes from API level */
#include "../pti_hal_api.h"

/* Includes from the HAL / ObjMan level */
#include "../objman/pti_object.h"
#include "firmware/pti_tp_api.h"

/* Exported Types ---------------------------------------------------------- */

typedef struct stptiHAL_DataEntry_s {
	Object_t ObjectHeader;	/* Must be first in structure */

	struct stptiHAL_DataEntry_s *PreviousEntry_p;		/**< Pointer to previous data entry when in a processing chain */
	struct stptiHAL_DataEntry_s *NextEntry_p;		/**< Pointer to next data entry when in a processing chain */
	U8 *SrcData_p;						/**< Pointer to Entry Data source - i.e. where to copy from */
	U8 *Data_p;						/**< Pointer to Entry Data allocated */
	U16 AllocSize;						/**< Size of memory allocated */
	U16 DataSize;						/**< Size of Data - multiple of packet size */
	U16 RepeatCount;					/**< Number of times to repeat this entry */
	U8 FromByte;						/**< Number of bytes to skip on packet replacement */
	BOOL NotifyEvent;					/**< Enable/Disable notify event */

	stptiSupport_DMAMemoryStructure_t CachedMemoryStructure; /**< A structure holding the range of flushable memory */
} stptiHAL_DataEntry_t;

/* Exported Function Prototypes -------------------------------------------- */
extern const stptiHAL_DataEntryAPI_t stptiHAL_DataEntryAPI;

#define stptiHAL_GetObjectDataEntry_p(ObjectHandle)   ((stptiHAL_DataEntry_t *)stptiOBJMAN_HandleToObjectPointer(ObjectHandle,OBJECT_TYPE_DATA_ENTRY))

#endif /* _PTI_DATA_ENTRY_H_ */
