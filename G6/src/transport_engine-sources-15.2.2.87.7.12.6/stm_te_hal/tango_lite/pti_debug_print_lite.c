/************************************************************************
Copyright (C) 2013 STMicroelectronics. All Rights Reserved.

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
   @file   pti_debug_print.c
   @brief  Generic debug printer helpers

 */

#include <linux/delay.h>

#include "../pti_debug.h"
#include "../pti_tshal_api.h"
#include "pti_pdevice_lite.h"
#include "pti_vdevice_lite.h"
#include "pti_swinjector_lite.h"
#include "pti_buffer_lite.h"
#include "pti_filter_lite.h"
#include "pti_slot_lite.h"

#include "pti_debug_print_lite.h"

/* MACROS ------------------------------------------------------------------ */

/* Private Constants ------------------------------------------------------- */

/* Private Variables ------------------------------------------------------- */

/* Private Function Prototypes --------------------------------------------- */

/* Functions --------------------------------------------------------------- */
int stptiHAL_Print_FullObjectTreeLite(void *ctx, debug_print_lite printer, stptiHAL_pDevice_lite_t * pDevice_p)
{
	stptiHAL_PrintObjectTreeRecurseLite(ctx, printer, pDevice_p->ObjectHeader.Handle, 0);
	return 0;
}

const char *stptiHAL_ReturnObjectStringLite(ObjectType_t ObjectType)
{
	switch (ObjectType) {
	case OBJECT_TYPE_PDEVICE:
		return ("pDevice");

	case OBJECT_TYPE_VDEVICE:
		return ("vDevice");

	case OBJECT_TYPE_SESSION:
		return ("Session");

	case OBJECT_TYPE_SOFTWARE_INJECTOR:
		return ("SwInjector");

	case OBJECT_TYPE_BUFFER:
		return ("Buffer");

	case OBJECT_TYPE_FILTER:
		return ("Filter");

	case OBJECT_TYPE_SIGNAL:
		return ("Signal");

	case OBJECT_TYPE_SLOT:
		return ("Slot");

	case OBJECT_TYPE_INDEX:
		return ("Index");

	case OBJECT_TYPE_DATA_ENTRY:
		return ("DataEntry");

	case OBJECT_TYPE_CONTAINER:
		return ("Container");

	case OBJECT_TYPE_ANY:
	case OBJECT_TYPE_INVALID:
		return ("*** OBJECT_TYPE_INVALID ***");

	default:
		break;
	}

	return ("*** OBJECT_TYPE_UNKNOWN ****");
}

/**
   @brief  Prints an objects details, and calls itself for any child objects

   Prints the details of an object and calls itself for the object's children.

   @param  ctx                 Context for printing
   @param  printer             Printer function pointer
   @param  RootObjectHandle    Handle of object to start tree print from
  */
void stptiHAL_PrintObjectTreeRecurseLite(void *ctx, debug_print_lite printer, FullHandle_t RootObjectHandle, int level)
{
	int i;
	ObjectType_t ObjectType = stptiOBJMAN_GetObjectType(RootObjectHandle);
	BOOL PrintAssociations = TRUE;
	Object_t *Object_p;

	if (ObjectType == OBJECT_TYPE_PDEVICE) {
		/* For pDevices this variable is ONLY use for the printf below */
		Object_p = (Object_t *) stptiHAL_GetObjectpDevice_lite_p(RootObjectHandle);
	} else {
		Object_p = stptiOBJMAN_HandleToObjectPointer(RootObjectHandle,
							     stptiOBJMAN_GetObjectType(RootObjectHandle));
	}

	printer(ctx, "0x%08x [%08x]  ", (unsigned)Object_p, RootObjectHandle.word);

	for (i = 0; i < level; i++) {
		printer(ctx, " ");
	}

	switch (ObjectType) {
	case OBJECT_TYPE_PDEVICE:
		printer(ctx, "pDevice%d\n", RootObjectHandle.member.pDevice);
		PrintAssociations = FALSE;
		/* Step through each vDevice */
		{
			int index, vDevices;
			FullHandle_t vDeviceHandleArray[MAX_NUMBER_OF_VDEVICES];

			vDevices = stptiOBJMAN_ReturnChildObjects(RootObjectHandle,
								  vDeviceHandleArray, MAX_NUMBER_OF_VDEVICES,
								  OBJECT_TYPE_VDEVICE);

			for (index = 0; index < vDevices; index++) {
				/* Note this is recursive */
				stptiHAL_PrintObjectTreeRecurseLite(ctx, printer, vDeviceHandleArray[index], level + 1);
			}
		}
		break;

	case OBJECT_TYPE_VDEVICE:

		printer(ctx, "vDevice%d StreamID=%04x Protocol=%d\n",
			RootObjectHandle.member.vDevice,
			((stptiHAL_vDevice_lite_t *) Object_p)->StreamID, ((stptiHAL_vDevice_lite_t *) Object_p)->TSProtocol);

		PrintAssociations = FALSE;
		/* Step through each Session */
		{
			Object_t *SessionObject;
			Object_t *vDeviceObject_p = Object_p;
			int index;

			index = -1;
			do {
				stptiOBJMAN_NextInList(&vDeviceObject_p->ChildObjects, (void *)&SessionObject, &index);
				if (index >= 0) {
					/* Note this is recursive */
					stptiHAL_PrintObjectTreeRecurseLite(ctx,
									printer,
									stptiOBJMAN_ObjectPointerToHandle
									(SessionObject), level + 1);
				}
			} while (index >= 0);
		}
		break;

	case OBJECT_TYPE_SESSION:
		printer(ctx, "Session%d\n", RootObjectHandle.member.Session);
		PrintAssociations = FALSE;
		/* Step through each Object */
		{
			Object_t *ChildObject;
			Object_t *SessionObject_p = Object_p;
			int index;

			index = -1;
			do {
				stptiOBJMAN_NextInList(&SessionObject_p->ChildObjects, (void *)&ChildObject, &index);
				if (index >= 0) {
					/* Note this is recursive */
					stptiHAL_PrintObjectTreeRecurseLite(ctx,
									printer,
									stptiOBJMAN_ObjectPointerToHandle(ChildObject),
									level + 1);
				}
			} while (index >= 0);
		}
		break;

	case OBJECT_TYPE_SOFTWARE_INJECTOR:
		printer(ctx, "%s%d Channel=%d",
			stptiHAL_ReturnObjectStringLite(ObjectType),
			(unsigned)RootObjectHandle.member.Object,
			((stptiHAL_SoftwareInjector_lite_t *) Object_p)->Channel);
		break;

	case OBJECT_TYPE_BUFFER:
		printer(ctx, "%s%d",
			stptiHAL_ReturnObjectStringLite(ObjectType),
			(unsigned)RootObjectHandle.member.Object);
		break;

	case OBJECT_TYPE_FILTER:
		printer(ctx, "%s%d",
			stptiHAL_ReturnObjectStringLite(ObjectType),
			(unsigned)RootObjectHandle.member.Object);
		break;

	case OBJECT_TYPE_SLOT:
		printer(ctx, "%s%d SlotIdx=%d,Mode=%d,Pid=0x%04x,PidIdx=%d",
			stptiHAL_ReturnObjectStringLite(ObjectType),
			(unsigned)RootObjectHandle.member.Object,
			((stptiHAL_Slot_lite_t *) Object_p)->SlotIndex,
			((stptiHAL_Slot_lite_t *) Object_p)->SlotMode, ((stptiHAL_Slot_lite_t *) Object_p)->PID,
			((stptiHAL_Slot_lite_t *) Object_p)->PidIndex);
		break;

	case OBJECT_TYPE_CONTAINER:
	case OBJECT_TYPE_SIGNAL:
	case OBJECT_TYPE_INDEX:
	case OBJECT_TYPE_DATA_ENTRY:
		printer(ctx, "%s%d", stptiHAL_ReturnObjectStringLite(ObjectType), (unsigned)RootObjectHandle.member.Object);
		break;

	case OBJECT_TYPE_ANY:
	case OBJECT_TYPE_INVALID:
		PrintAssociations = FALSE;
		printer(ctx, "*** OBJECT_TYPE_INVALID ***");
		break;

	default:
		PrintAssociations = FALSE;
		printer(ctx, "*** OBJECT_TYPE_UNKNOWN %d ***", ObjectType);
		break;
	}

	if (PrintAssociations) {
		Object_t *AssocObject_p;
		int index;
		int count = 0;

		stptiOBJMAN_FirstInList(&Object_p->AssociatedObjects, (void *)&AssocObject_p, &index);
		while (index >= 0) {
			if (count == 0) {
				printer(ctx, "  {");
			} else {
				printer(ctx, ",");
			}
			printer(ctx, "%s%d",
				stptiHAL_ReturnObjectStringLite(AssocObject_p->Handle.member.ObjectType),
				(unsigned)AssocObject_p->Handle.member.Object);
			count++;
			stptiOBJMAN_NextInList(&Object_p->AssociatedObjects, (void *)&AssocObject_p, &index);
		}
		if (count > 0) {
			printer(ctx, "}");
		}
		printer(ctx, "\n");
	}
}
