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
   @file   pti_object.h
   @brief  Object Initialisation, Termination and Manipulation Functions.

   This file implements the functions for creating, destroying and accessing all the PTI objects
   that hold important information for the host.

   In addition to regular objects we also handle "List Objects" which once allocated must be sized.
   There are various list helper functions to facilitate use of the the lists.

 */

#ifndef _PTI_OBJECT_H_
#define _PTI_OBJECT_H_

/* Includes ---------------------------------------------------------------- */
#include "pti_list.h"
#include "linuxcommon.h"
#include "stddefs.h"

/* Includes from API level (only for parameterisation) */
#include "../pti_stpti.h" /* include specifically allowed defines from stpti.h */
#include "../pti_osal.h"

/* Includes from the HAL / ObjMan level */

/* Exported Types ---------------------------------------------------------- */

#define MAX_NUMBER_OF_PDEVICES      6	/* Must be less than 7 */
#define MAX_NUMBER_OF_VDEVICES      16	/* (per physical device) Must be less than 32 */
#define MAX_NUMBER_OF_SESSIONS      15	/* (per vDevice) Must be less than 16 */

#define STPTI_OBJ_ALLOC_BLOCK_SIZE  1024

typedef enum stptiOBJMAN_HAL_id_e {
	stptiHAL_IDENTIFIER = 0x1
} stptiOBJMAN_HAL_id_t;

/* Do not conditionally compile in this object list (it will make debugging difficult) */

/* The order of this list is important.  When associating, objects with lower numbers have their
   association functions called first.  For example when associating a BUFFER and a SLOT, the BUFFER
   association function is called before the SLOT association function.  For disassociation the
   reverse is the case. */
typedef volatile enum ObjectType_e {
	OBJECT_TYPE_INVALID = 0,    /**< First object type is invalid to make sure handles of value 0 aren't accepted */
	OBJECT_TYPE_PDEVICE,
	OBJECT_TYPE_VDEVICE,
	OBJECT_TYPE_SESSION,
	OBJECT_TYPE_SOFTWARE_INJECTOR,
	OBJECT_TYPE_BUFFER,
	OBJECT_TYPE_FILTER,
	OBJECT_TYPE_SIGNAL,
	OBJECT_TYPE_INDEX,
	OBJECT_TYPE_DATA_ENTRY,
	OBJECT_TYPE_SLOT,
	OBJECT_TYPE_CONTAINER,	    /**< This object should do nothing, but contain information relevent to the API layer */
	NUMBER_OF_OBJECT_TYPES,	    /**< This must be the last item in the list of (real) objects */

	OBJECT_TYPE_ANY = 62,

	STPTI_HANDLE_INVALID_OBJECT_TYPE = 63,
} ObjectType_t;

/* If you mess with these defines and the union below, make sure that stptiOBJMAN_NullHandle, and
   the STPTI_NullHandle() constants agree! */
#define STPTI_HANDLE_INVALID_PDEVICE_NUMBER 7		/* Make the maximum allowed by the Handle */
#define STPTI_HANDLE_INVALID_VDEVICE_NUMBER 31		/* Make the maximum allowed by the Handle */
#define STPTI_HANDLE_INVALID_SESSION_NUMBER 15		/* Make the maximum allowed by the Handle */
#define STPTI_HANDLE_INVALID_OBJECT_NUMBER  16383	/* Make the maximum allowed by the Handle */
typedef union FullHandle_u {
	struct {
		unsigned int Object:14;		/**< 16384 objects (for each open/close session)        */
		unsigned int ObjectType:6;	/**<    64 types of data objects                        */
		unsigned int Session:4;		/**<    16 sessions for each vDevice                    */
		unsigned int vDevice:5;		/**<    32 virtual devices for each physical device     */
		unsigned int pDevice:3;		/**<     8 physical devices                             */
	} member;

	U32 word;

} FullHandle_t;

typedef struct {
	FullHandle_t Handle;
	U32 APIHandle;
	List_t AssociatedObjects;
	List_t ChildObjects;
	void *AuxiliaryData_p; /* AuxiliaryMetadata was meant to be used for information not used by the HAL but useful to be linked with an object (currently not needed) */
	size_t AuxiliaryDataSize;
} Object_t;

typedef struct {
	ST_ErrorCode_t(*Allocator) (FullHandle_t ObjectHandle, void *params_p);
	ST_ErrorCode_t(*Associator) (FullHandle_t Object1Handle, FullHandle_t Object2Handle);
	ST_ErrorCode_t(*Disassociator) (FullHandle_t Object1Handle, FullHandle_t Object2Handle);
	ST_ErrorCode_t(*Deallocator) (FullHandle_t ObjectHandle);
	ST_ErrorCode_t(*Serialiser) (FullHandle_t ObjectHandle, int mode, void *Region_p, size_t RegionSize,
				     size_t * SerialisedObjectSize_p);
	ST_ErrorCode_t(*Deserialiser) (FullHandle_t ObjectHandle, void *Region_p, int mode);
} ObjectManagerFunctions_t;

typedef struct {
	stptiSupport_AccessLock_t AccessLock;
	Object_t *pDevice_p;
	stptiOBJMAN_HAL_id_t HAL_id;
	size_t ObjectSize[NUMBER_OF_OBJECT_TYPES];
	ObjectManagerFunctions_t ObjFunPtr[NUMBER_OF_OBJECT_TYPES];
} ObjectManagerRoot_t;

/* Exported Function Prototypes -------------------------------------------- */

/*
   Access control functions

   These function Control Access to the objects themselves, and structures that they manage.  For
   simplicity, currently the lock will lock the WHOLE pDevice, and not objects independently.

 */
ST_ErrorCode_t stptiOBJMAN_InitWriteLock(unsigned int pDeviceIndex);
void stptiOBJMAN_WriteLock(FullHandle_t ObjectHandle, stptiSupport_AccessLockState_t * LocalLockState_p);
void stptiOBJMAN_TryWriteLock(FullHandle_t ObjectHandle, stptiSupport_AccessLockState_t * LocalLockState_p);
void stptiOBJMAN_ReadLock(FullHandle_t ObjectHandle, stptiSupport_AccessLockState_t * LocalLockState_p);
void stptiOBJMAN_Unlock(FullHandle_t ObjectHandle, stptiSupport_AccessLockState_t * LocalLockState_p);
ST_ErrorCode_t stptiOBJMAN_TermWriteLock(unsigned int pDeviceIndex);

void *stptiOBJMAN_ReturnHALFunctionPool(FullHandle_t FullObjectHandle);
void *stptiOBJMAN_ReturnTSHALFunctionPool(int TSDevice);

/*
    Handle Conversion Functions

    These will convert an API handle (STCOMMON SysHandle) into a FullHandle and back again.  There
    are also functions that can also generate the vDevice and Session FullHandles from an object
    Handle (see below).
 */
#if defined( STPTI_IPRC_PRESENT )
FullHandle_t stptiOBJMAN_ConvertToFullHandle(U32 APIHandle);
U32 stptiOBJMAN_ConvertToAPIHandle(FullHandle_t FullHandle);
#else
#define stptiOBJMAN_ConvertToFullHandle(APIHandle)              ((FullHandle_t)(APIHandle))
#define stptiOBJMAN_ConvertToAPIHandle(FullHandle)              ((STPTI_Handle_t)((FullHandle).word));
#endif

/*
    Default Handles

    A function to return the pDevice FullHandle, and Null Handles.  We can also test to see if it
    is a NULL handle.
 */
FullHandle_t stptiOBJMAN_pDeviceObjectHandle(unsigned int pDeviceIndex);
#define stptiOBJMAN_NullHandle                                  ((FullHandle_t)0xFFFFFFFF)	/* if you change this remember STPTI_NullHandle() in stpti.h */
#define stptiOBJMAN_IsNullHandle(Handle)                        ((Handle).word == 0xFFFFFFFF)

#define stptiOBJMAN_IsHandlesEqual(Handle1, Handle2)            ((Handle1).word == (Handle2).word)

/*
    Handle inspection

    Check a handle is valid.
    Return the Object Type referred to by a Handle
    Or return the parent vDevice or Session FullHandle of a Object.
 */
ST_ErrorCode_t stptiOBJMAN_CheckHandle(FullHandle_t Handle, ObjectType_t ObjectType);
#define stptiOBJMAN_GetpDeviceIndex(ObjectHandle) (ObjectHandle.member.pDevice)
#define stptiOBJMAN_GetvDeviceIndex(ObjectHandle) (ObjectHandle.member.vDevice)
#define stptiOBJMAN_GetObjectType(ObjectHandle) (ObjectHandle.member.ObjectType)
#define stptiOBJMAN_ConvertFullObjectHandleToFullpDeviceHandle(ObjectHandle) (stptiOBJMAN_pDeviceObjectHandle(stptiOBJMAN_GetpDeviceIndex(ObjectHandle)))
FullHandle_t stptiOBJMAN_ConvertFullObjectHandleToFullvDeviceHandle(FullHandle_t FullHandle);
FullHandle_t stptiOBJMAN_ConvertFullObjectHandleToFullSessionHandle(FullHandle_t FullHandle);

/*
    Functions for converting object pointers to/from handles.

    In general... stptiOBJMAN_HandleToObjectPointer(). ...should be used.  Depending on
    STPTI_DEBUG_HANDLES it will check handle before using it or not.

    In different situations you may want to use...
         stptiOBJMAN_HandleToObjectPointerWithChecks()          SAFE - returns NULL if handle invalid
         stptiOBJMAN_HandleToObjectPointerWithoutChecks()       FAST - may exception if handle invalid

    Reversing the process use...           stptiOBJMAN_ObjectPointerToHandle()
 */
#define stptiOBJMAN_HandleToObjectPointer(Handle, ObjectType)   stptiOBJMAN_HandleToObjectPointerWithChecks(Handle, ObjectType)
Object_t *stptiOBJMAN_HandleToObjectPointerWithChecks(FullHandle_t Handle, ObjectType_t ObjectType);
Object_t *stptiOBJMAN_HandleToObjectPointerWithoutChecks(FullHandle_t Handle, ObjectType_t ObjectType);

/* Return an Object Handle from a Object Pointer */
#define stptiOBJMAN_ObjectPointerToHandle(Object_p)             (((Object_t *)Object_p)->Handle)

/*
    Functions for examining Associations, and Children.
 */
int stptiOBJMAN_ReturnAssociatedObjects(FullHandle_t ObjectHandle, FullHandle_t *AssocObjectHandles,
					int SizeOfAssocObjectHandles, ObjectType_t ObjectTypeToReturn);
#define stptiOBJMAN_CountAssociatedObjects(ObjectHandle, ObjectTypeToCount)  stptiOBJMAN_ReturnAssociatedObjects(ObjectHandle, NULL, 0, ObjectTypeToCount)
int stptiOBJMAN_ReturnChildObjects(FullHandle_t ObjectHandle, FullHandle_t *ChildObjectHandles,
				   int SizeOfChildObjectHandles, ObjectType_t ObjectTypeToReturn);
#define stptiOBJMAN_CountChildObjects(ObjectHandle, ObjectTypeToCount)  stptiOBJMAN_ReturnChildObjects(ObjectHandle, NULL, 0, ObjectTypeToCount)
BOOL stptiOBJMAN_IsAssociated(FullHandle_t ObjectHandle1, FullHandle_t ObjectHandle2);

/*
    AuxiliaryData

    This allow access to the AuxiliaryData field in the Object.  It is meant to be used for
    information not used by the HAL but useful to be linked with an object.

    The memory allocated for AuxiliaryData will be automatically free'd when the object is
    deallocated.
 */
void *stptiOBJMAN_AllocateObjectAuxiliaryData(FullHandle_t ObjectHandle, size_t size);
void stptiOBJMAN_DeallocateObjectAuxiliaryData(FullHandle_t ObjectHandle);
#define stptiOBJMAN_GetObjectAuxiliaryData_p(ObjectHandle)          (stptiOBJMAN_HandleToObjectPointer(ObjectHandle, stptiOBJMAN_GetObjectType(ObjectHandle))->AuxiliaryData_p)

/*
    Register an Object for use in the Object Manager.

    Each HAL must have it's objects registered before the Object Manager can use them.  This is so
    the Object Manager can relate the objects function's to the HAL api function calls.
 */
ST_ErrorCode_t stptiOBJMAN_RegisterObjectType(unsigned int pDeviceIndex, ObjectType_t ObjectType, size_t ObjectSize,
					      ObjectManagerFunctions_t *ObjectManagementFunctions_p);

/*
    Main API for allocating, associating, disassociating, deallocate objects.
    stptiHAL_call() (in pti_hal_api.h) provides access to the additional object manipulation
    functions.
 */
ST_ErrorCode_t stptiOBJMAN_AllocateObject(FullHandle_t ParentObjectHandle, ObjectType_t NewObjectType,
					  FullHandle_t *NewObjectHandle_p, void *params_p);
ST_ErrorCode_t stptiOBJMAN_AssociateObjects(FullHandle_t Object1Handle, FullHandle_t Object2Handle);
ST_ErrorCode_t stptiOBJMAN_DisassociateObjects(FullHandle_t Object1Handle, FullHandle_t Object2Handle);
ST_ErrorCode_t stptiOBJMAN_DeallocateObject(FullHandle_t ObjectHandle, BOOL Force);

/* Used for obtaining/setting object state (for passive power down modes or cloning objects) */
ST_ErrorCode_t stptiOBJMAN_SerialiseObject(FullHandle_t ObjectHandle, int mode, void *Region_p, size_t RegionSize,
					   size_t *SerialisedObjectSize);
ST_ErrorCode_t stptiOBJMAN_DeserialiseObject(FullHandle_t ParentObjectHandle, ObjectType_t NewObjectType,
					     FullHandle_t *NewObjectHandle_p, void *Region_p);

/* Helper Functions for monitoring Memory Usage */
void *stptiOBJMAN_AllocateMemory(int Size);
void stptiOBJMAN_DeallocateMemory(void *Ptr, unsigned int Size);

#endif /* _PTI_OBJECT_H_ */
