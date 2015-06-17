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
   @file   pti_swinjector.h
   @brief  The Software Injector for playback streams.

   This file declares the object structure and access mechanism for the Software
   Injector Object.

   This file should ONLY contain function prototypes and constants that are
   INTERNAL to the stptiHAL.  i.e. with the exception of the HAL object
   registration function in pti_driver.c, NOTHING outside of the tango
   directory should be referring to this file.

   Defines and enums relevent to the API of the HAL must be put into
   pti_hal_api.h and will be shared across all HALs.


 */

#ifndef _PTI_SWINJECTOR_H_
#define _PTI_SWINJECTOR_H_

/* Includes ---------------------------------------------------------------- */

/* ANSI C includes */

/* STAPI includes */

/* Includes from API level */
#include "../pti_hal_api.h"

/* Includes from the HAL / ObjMan level */
#include "../objman/pti_object.h"
#include "firmware/pti_tp_api.h"

/* Exported Types ---------------------------------------------------------- */

#define PLAYBACK_PARTIAL_PACKET_MAX_SIZE 200
typedef struct {
	stptiTP_Playback_t PlaybackNode;
} stptiHAL_SoftwareInjectionNode_t;

typedef struct {
	Object_t ObjectHeader;					/* Must be first in structure */
	unsigned int PacketLength;				/**< Transport Packet Length */
	struct semaphore InjectionSemaphore;			/**< SoftwareInjection (Playback) Node Completion Semaphore */

	int Channel;						/**< Playback Channel (0..7) */
	BOOL IsActive;						/**< Is this channel currently injecting? */
	BOOL FlushPartialPkts;					/**< Flush partial packet upon next AddNode */

	stptiHAL_SoftwareInjectionNode_t *InjectionNodeList_p;	/**< Aligned start of Node List */

	int LengthOfNodeList;					/**< Size of the Node List */
	int NodeCount;						/**< Total Number of populated Nodes */
	int NextToBeInjected;					/**< Starting Node for Next Injection */
	int InjectionNodeCount;					/**< Number of Nodes involved in the current injection */

	stptiSupport_DMAMemoryStructure_t CachedMemoryStructure;/**< A structure holding the range of flushable memory */

} stptiHAL_SoftwareInjector_t;

/* Exported Function Prototypes -------------------------------------------- */

extern const stptiHAL_SoftwareInjectorAPI_t stptiHAL_SoftwareInjectorAPI;

ST_ErrorCode_t stptiHAL_SoftwareInjectorAbortInternal(FullHandle_t SoftwareInjectorHandle);	/* Must be called outside a HAL access lock */

#define stptiHAL_GetObjectSoftwareInjector_p(ObjectHandle)   ((stptiHAL_SoftwareInjector_t *)stptiOBJMAN_HandleToObjectPointer(ObjectHandle,OBJECT_TYPE_SOFTWARE_INJECTOR))

#endif /* _PTI_SWINJECTOR_H_ */
