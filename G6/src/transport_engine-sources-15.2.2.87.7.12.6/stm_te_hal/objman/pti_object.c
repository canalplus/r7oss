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
   @file   pti_object.c
   @brief  Object Initialisation, Termination and Manipulation Functions.

   This file implements the functions for creating, destroying and accessing all the PTI objects
   that hold important information for the host.

   In addition to regular objects we also handle "List Objects" which once allocated must be sized.
   There are various list helper functions to facilitate use of the the lists.

 */

#if 0
#define STPTI_PRINT
#endif

/* Includes ---------------------------------------------------------------- */
#include "linuxcommon.h"

#include "stddefs.h"

/* Includes from API level */
#include "../pti_debug.h"
#include "../pti_driver.h"
#include "../pti_osal.h"
#include "../pti_stpti.h"

/* Includes from the HAL / ObjMan level */
#include "pti_object.h"

/* MACROS ------------------------------------------------------------------ */
/* Older versions of STCOMMON do not have STCOMMON_INVALID_HANDLE */
#if !defined( STCOMMON_INVALID_HANDLE )
#define STCOMMON_INVALID_HANDLE 0xFFFFFFFF
#endif

/* Private Constants ------------------------------------------------------- */
/* Private Variables ------------------------------------------------------- */
/* Private Function Prototypes --------------------------------------------- */

/* These dummy functions are useful for debug, and also avoid us having to check function pointers are valid in the complex object manipulation functions */
ST_ErrorCode_t stptiOBJMAN_DummyAllocator(FullHandle_t ObjectHandle, void *params_p);
ST_ErrorCode_t stptiOBJMAN_DummyAssociator(FullHandle_t ObjectHandle, FullHandle_t ObjectToAssociateHandle);
ST_ErrorCode_t stptiOBJMAN_DummyDisassociator(FullHandle_t ObjectHandle, FullHandle_t ObjectToDisassociateHandle);
ST_ErrorCode_t stptiOBJMAN_DummyDeallocator(FullHandle_t ObjectHandle);
ST_ErrorCode_t stptiOBJMAN_DummySerialiseObject(FullHandle_t ObjectHandle, int mode, void *Region_p, size_t RegionSize,
						size_t * SerialisedObjectSize);
ST_ErrorCode_t stptiOBJMAN_DummyDeserialiseObject(FullHandle_t ObjectHandle, void *params_p, int mode);
ST_ErrorCode_t stptiOBJMAN_DummyActionFunction(FullHandle_t ObjectHandle, void *params_p);

static ST_ErrorCode_t stptiOBJMAN_AllocateObjectInternal(FullHandle_t ParentObjectHandle, ObjectType_t NewObjectType,
							 FullHandle_t * NewObjectHandle_p);
static ST_ErrorCode_t stptiOBJMAN_AssociateObjectsInternal(FullHandle_t Object1Handle, FullHandle_t Object2Handle);
static ST_ErrorCode_t stptiOBJMAN_DisassociateObjectsInternal(FullHandle_t Object1Handle, FullHandle_t Object2Handle);
static ST_ErrorCode_t stptiOBJMAN_DeallocateObjectInternal(FullHandle_t ObjectHandle, BOOL CallDeallocator, BOOL Force,
							   BOOL ReleaseHandle);

static void *stptiOBJMAN_AllocateObjectAuxiliaryDataInternal(FullHandle_t ObjectHandle, size_t size);

/* Functions --------------------------------------------------------------- */

/**
   @brief  Return the HAL Function Pool for this Object.

   This function returns the HAL Function Pool for this Object (by looking up the physical device).
   It is done here to reduce the complexity in the HAL function call macro, and avoid a cyclic
   header dependency between pti_driver.h and pti_hal_api.h.  For this reason, it returns it as
   a void *.

   @param  FullObjectHandle  Full Object Handle.

   @return                   The HAL Function Pool pointer, or NULL if unable to find it.

 */
void *stptiOBJMAN_ReturnHALFunctionPool(FullHandle_t FullObjectHandle)
{
	if (stptiOBJMAN_GetpDeviceIndex(FullObjectHandle) >= STPTI_Driver.NumberOfpDevices) {
		return (NULL);
	} else {
		return ((void *)STPTI_Driver.HALFunctionPool[stptiOBJMAN_GetpDeviceIndex(FullObjectHandle)]);
	}
}

/**
   @brief  Return the TSHAL Function Pool for this TSDevice.

   This function returns the TSHAL Function Pool for this TSDevice (by looking up the physical device).
   It is done here to reduce the complexity in the TSHAL function call macro, and avoid a cyclic
   header dependency between pti_driver.h and pti_tshal_api.h.  For this reason, it returns it as
   a void *.

   @param  FullObjectHandle  Full Object Handle.

   @return                   The HAL Function Pool pointer, or NULL if unable to find it.

 */
void *stptiOBJMAN_ReturnTSHALFunctionPool(int TSDevice)
{
	if (TSDevice != 0) {
		return (NULL);
	} else {
		return ((void *)&STPTI_Driver.TSHALFunctionPool[TSDevice]);
	}
}

/**
   @brief  Allocate Memory

   This function allocates memory, keeping track of the memory usage.

   @param  Size           Size in bytes

   @return                Pointer to allocated Memory or NULL.

 */
void *stptiOBJMAN_AllocateMemory(int Size)
{
	void *Ptr = NULL;

	Ptr = kmalloc(Size, GFP_KERNEL);

	if (NULL != Ptr) {
		STPTI_Driver.ObjManMemoryTally += Size;

		if (STPTI_Driver.ObjManMemoryTally > STPTI_Driver.ObjManMemoryHighWater) {
			STPTI_Driver.ObjManMemoryHighWater = STPTI_Driver.ObjManMemoryTally;
		}
		stpti_printf("Allocated %u bytes, total %u bytes", Size, STPTI_Driver.ObjManMemoryTally);
	}

	return (Ptr);
}

/**
   @brief  Deallocate Memory

   This function deallocates (frees) memory, keeping track of the memory usage.

   @param  Ptr            Pointer to memory to free
   @param  Size           Size in bytes

   @return                Nothing

 */
void stptiOBJMAN_DeallocateMemory(void *Ptr, unsigned int Size)
{
	kfree(Ptr);

	STPTI_Driver.ObjManMemoryTally -= Size;

	stpti_printf("Deallocated %u bytes, remaining total %u bytes", Size, STPTI_Driver.ObjManMemoryTally);
}

/**
   @brief  Initialise (Allocate) a pti osal WriteLock to be used for this pDevice

   This function sets up the object group's (pDevice group) WriteLock.

   @param  pDeviceIndex  The index of the pDevice
   @param  WriteLock_p   A pointer to the WriteLock.

   @return               ST_NO_ERROR

 */
ST_ErrorCode_t stptiOBJMAN_InitWriteLock(unsigned int pDeviceIndex)
{
	mutex_init(&STPTI_Driver.ObjMan[pDeviceIndex]->AccessLock.WriteLockMutex);
	STPTI_Driver.ObjMan[pDeviceIndex]->AccessLock.WriteLockPending = FALSE;
	STPTI_Driver.ObjMan[pDeviceIndex]->AccessLock.ReadLockCount = 0;
	return (ST_NO_ERROR);
}

/**
   @brief  Write lock the pDevice that this Object belongs to.

   This function locks the pDevice.

   @param  ObjectHandle      Handle of an Object under the pDevice
   @param  LocalLockState_p  Lock State (which will be modified)

   @return                   ST_NO_ERROR

 */
void stptiOBJMAN_WriteLock(FullHandle_t ObjectHandle, stptiSupport_AccessLockState_t * LocalLockState_p)
{
	if (ObjectHandle.member.pDevice < STPTI_Driver.NumberOfpDevices) {
		stptiSupport_WriteLock(&STPTI_Driver.ObjMan[ObjectHandle.member.pDevice]->AccessLock, LocalLockState_p);
	}
}
/**
   @brief  Try Write lock the pDevice that this Object belongs to.

   This function Try to lock the pDevice.

   @param  ObjectHandle      Handle of an Object under the pDevice
   @param  LocalLockState_p  Lock State (which will be modified)

   @return                   1 if Lock has been acquired successfully, and 0 on contention

 */
void stptiOBJMAN_TryWriteLock(FullHandle_t ObjectHandle, stptiSupport_AccessLockState_t * LocalLockState_p)
{
	if (ObjectHandle.member.pDevice < STPTI_Driver.NumberOfpDevices) {
		stptiSupport_TryWriteLock(&STPTI_Driver.ObjMan[ObjectHandle.member.pDevice]->AccessLock, LocalLockState_p);
	}
}
/**
   @brief  Read lock the pDevice that this Object belongs to.

   This function locks the pDevice.

   @param  ObjectHandle      Handle of an Object under the pDevice
   @param  LocalLockState_p  Lock State (which will be modified)

   @return                   ST_NO_ERROR

 */
void stptiOBJMAN_ReadLock(FullHandle_t ObjectHandle, stptiSupport_AccessLockState_t * LocalLockState_p)
{
	if (ObjectHandle.member.pDevice < STPTI_Driver.NumberOfpDevices) {
		stptiSupport_ReadLock(&STPTI_Driver.ObjMan[ObjectHandle.member.pDevice]->AccessLock, LocalLockState_p);
	}
}

/**
   @brief  Unlock the pDevice that this Object belongs to.

   This function unlocks the pDevice.

   @param  ObjectHandle      Handle of an Object under the pDevice
   @param  LocalLockState_p  Lock State (which will be modified)

   @return                   ST_NO_ERROR

 */
void stptiOBJMAN_Unlock(FullHandle_t ObjectHandle, stptiSupport_AccessLockState_t * LocalLockState_p)
{
	if (ObjectHandle.member.pDevice < STPTI_Driver.NumberOfpDevices) {
		stptiSupport_Unlock(&STPTI_Driver.ObjMan[ObjectHandle.member.pDevice]->AccessLock, LocalLockState_p);
	}
}

/**
   @brief  Deallocate a pti osal WriteLock to be used for this pDevice

   This function deallocates the object group's (pDevice group) WriteLock.

   @param  pDeviceIndex  The index of the pDevice
   @param  WriteLock_p   A pointer to the WriteLock.

   @return               ST_NO_ERROR

 */
ST_ErrorCode_t stptiOBJMAN_TermWriteLock(unsigned int pDeviceIndex)
{
	ST_ErrorCode_t Error = ST_ERROR_BAD_PARAMETER;

	if (pDeviceIndex < (unsigned)STPTI_Driver.NumberOfpDevices) {
		mutex_destroy(&STPTI_Driver.ObjMan[pDeviceIndex]->AccessLock.WriteLockMutex);
		Error = ST_NO_ERROR;
	}
	return (Error);
}

/* These are dummy functions for Allocation, Association, Disassociation and Deallocation */
/* They make the code much cleaner (don't have to keep testing for NULL function pointers) */

/**
   @brief  Dummy Allocator

   The Allocator will do any additional allocation of resources required for this object.  This
   is usually any lists used by the object, but could also be physical resources as well.

   This specific function doesn't allocate anything, but will printout a message.  These dummy
   functions make the code much cleaner (don't have to keep testing for NULL function pointers)

   @param  ObjectHandle  A handle for the Objectjust been created
   @param  params_p      Usually a point to a structure containing any relevent parameters needed
                         for allocating this object.

   @return               A standard st error type...
                         - ST_NO_ERROR if no errors
 */
ST_ErrorCode_t stptiOBJMAN_DummyAllocator(FullHandle_t ObjectHandle, void *params_p)
{
	ObjectHandle = ObjectHandle;
	params_p = params_p;
	stpti_printf("Allocating ObjectType=%u Handle=%08x using default allocator.",
		     (unsigned)stptiOBJMAN_GetObjectType(ObjectHandle), (unsigned)ObjectHandle.word);

	return (ST_NO_ERROR);
}

/**
   @brief  Dummy Associator (reports error)

   The Associator will do any addition work required when associating objects.  Note there will be
   TWO association functions called for each association (one for obj1,obj2 and another for
   obj2,obj1).

   This specific function doesn't associate anything, but will printout a message.  These dummy
   functions make the code much cleaner (don't have to keep testing for NULL function pointers)

   If you want to associate objects you need to supply an associator (you cannot use this one).

   @param  ObjectHandle             A handle for the Object
   @param  ObjectToAssociateHandle  A handle for the Object to be associated to

   @return                          A standard st error type...
                                    - ST_ERROR_BAD_PARAMETER (as by default we don't permit
                                      association)
 */
ST_ErrorCode_t stptiOBJMAN_DummyAssociator(FullHandle_t ObjectHandle, FullHandle_t ObjectToAssociateHandle)
{
	ObjectHandle = ObjectHandle;
	ObjectToAssociateHandle = ObjectToAssociateHandle;
	stpti_printf("You cannot Associate ObjectType=%u Handle=%08x with ObjectType=%u Handle=%08x.",
		     (unsigned)stptiOBJMAN_GetObjectType(ObjectHandle), (unsigned)ObjectHandle.word,
		     (unsigned)stptiOBJMAN_GetObjectType(ObjectToAssociateHandle),
		     (unsigned)ObjectToAssociateHandle.word);

	return (ST_ERROR_BAD_PARAMETER);
}

/**
   @brief  Dummy Disassociator

   The Disassociator will do any addition work required when disassociating objects.

   This specific function doesn't do anything, but will printout a message.  These dummy
   functions make the code much cleaner (don't have to keep testing for NULL function pointers)

   @param  ObjectHandle                A handle for the Object
   @param  ObjectToDisassociateHandle  A handle for the Object to be associated to

   @return                             A standard st error type...
                                       - ST_NO_ERROR
 */
ST_ErrorCode_t stptiOBJMAN_DummyDisassociator(FullHandle_t ObjectHandle, FullHandle_t ObjectToDisassociateHandle)
{
	ObjectHandle = ObjectHandle;
	ObjectToDisassociateHandle = ObjectToDisassociateHandle;
	stpti_printf
	    ("Disassociating ObjectType=%u Handle=%08x with ObjectType=%u Handle=%08x using default disassociator.",
	     (unsigned)stptiOBJMAN_GetObjectType(ObjectHandle), (unsigned)ObjectHandle.word,
	     (unsigned)stptiOBJMAN_GetObjectType(ObjectToDisassociateHandle),
	     (unsigned)ObjectToDisassociateHandle.word);

	return (ST_NO_ERROR);
}

/**
   @brief  Dummy Deallocator

   The Deallocator will do any addition work required when deallocating objects.

   This specific function doesn't do anything, but will printout a message.  These dummy
   functions make the code much cleaner (don't have to keep testing for NULL function pointers)

   @param  ObjectHandle  A handle for the Object being deallocated

   @return               A standard st error type...
                         - ST_NO_ERROR
 */
ST_ErrorCode_t stptiOBJMAN_DummyDeallocator(FullHandle_t ObjectHandle)
{
	ObjectHandle = ObjectHandle;
	stpti_printf("Deallocating ObjectType=%u Handle=%08x using default deallocator.",
		     (unsigned)stptiOBJMAN_GetObjectType(ObjectHandle), (unsigned)ObjectHandle.word);

	return (ST_NO_ERROR);
}

ST_ErrorCode_t stptiOBJMAN_DummySerialiseObject(FullHandle_t ObjectHandle, int mode, void *Region_p, size_t RegionSize,
						size_t * SerialisedObjectSize)
{
	ObjectHandle = ObjectHandle;
	mode = mode;
	Region_p = Region_p;
	RegionSize = RegionSize;

	STPTI_PRINTF_ERROR("No function for Serialising ObjectType=%u Handle=%08x.",
			   (unsigned)stptiOBJMAN_GetObjectType(ObjectHandle), (unsigned)ObjectHandle.word);

	*SerialisedObjectSize = 0;

	return (ST_ERROR_FEATURE_NOT_SUPPORTED);
}

ST_ErrorCode_t stptiOBJMAN_DummyDeserialiseObject(FullHandle_t ObjectHandle, void *params_p, int mode)
{
	ObjectHandle = ObjectHandle;
	params_p = params_p;
	mode = mode;
	STPTI_PRINTF_ERROR("No function for Deserialising ObjectType=%u Handle=%08x.",
			   (unsigned)stptiOBJMAN_GetObjectType(ObjectHandle), (unsigned)ObjectHandle.word);

	return (ST_ERROR_FEATURE_NOT_SUPPORTED);
}

/**
   @brief  Dummy Getter, Setter, Reader, Writer Function

   This specific function doesn't do anything, but will printout a message.  Calling this function
   will be an error.  It means the object doesn't support the operation requested of it.

   @param  ObjectHandle  A handle for the Object being acted on
   @param  params_p      A pointer to a structure containing the params for the action (ignored)

   @return               A standard st error type...
                         - ST_NO_ERROR
 */
ST_ErrorCode_t stptiOBJMAN_DummyActionFunction(FullHandle_t ObjectHandle, void *params_p)
{
	ObjectHandle = ObjectHandle;
	params_p = params_p;
	stpti_printf("This object does not support the requested action ObjectType=%u Handle=%08x.",
		     (unsigned)stptiOBJMAN_GetObjectType(ObjectHandle), (unsigned)ObjectHandle.word);

	return (ST_ERROR_BAD_PARAMETER);
}

#if defined( STPTI_IPRC_PRESENT )

/**
   @brief  Converts an APIHandle (STCOMMON SysHandle) into a FullHandle

   This specific function converts and STCOMMON SysHandle into a FullHandle used within the HAL layer.

   @param  APIHandle     The SysHandle to convert

   @return               The FullHandle
 */
FullHandle_t stptiOBJMAN_ConvertToFullHandle(U32 APIHandle)
{
	ST_ErrorCode_t Error;
	FullHandle_t FullHandle = stptiOBJMAN_NullHandle;

	if (APIHandle != STCOMMON_INVALID_HANDLE) {
		Error = STCOMMON_HandleGetDriverData(APIHandle, (void **)&FullHandle.word);
		if (ST_NO_ERROR != Error) {
			FullHandle = stptiOBJMAN_NullHandle;
		}
	}

	return (FullHandle);
}

/**
   @brief  Converts an PTI FullHandle into a APIHandle (STCOMMON SysHandle).

   This specific function converts a FullHandle used within the HAL layer into and STCOMMON SysHandle.

   @param  FullHandle    The FullHandle to convert

   @return               The API Handle
 */
U32 stptiOBJMAN_ConvertToAPIHandle(FullHandle_t FullHandle)
{
	U32 APIHandle = STCOMMON_INVALID_HANDLE;

	if (!stptiOBJMAN_IsNullHandle(FullHandle)) {
		Object_t *Object_p =
		    stptiOBJMAN_HandleToObjectPointer(FullHandle, stptiOBJMAN_GetObjectType(FullHandle));
		if (Object_p != NULL) {
			APIHandle = Object_p->APIHandle;
		}
	}

	return (APIHandle);
}

#endif

/**
   @brief  Root Object Handle for creating vDevice Objects.

   This function returns the RootObjectHandle for a specified class.

   @param  pDevice       The physical device number

   @return               A FullHandle for allocating vDevices.

 */
FullHandle_t stptiOBJMAN_pDeviceObjectHandle(unsigned int pDevice)
{
	FullHandle_t pDeviceHandle = stptiOBJMAN_NullHandle;
	pDeviceHandle.member.pDevice = pDevice;
	pDeviceHandle.member.vDevice = STPTI_HANDLE_INVALID_VDEVICE_NUMBER;
	pDeviceHandle.member.Session = STPTI_HANDLE_INVALID_SESSION_NUMBER;
	pDeviceHandle.member.ObjectType = OBJECT_TYPE_PDEVICE;
	pDeviceHandle.member.Object = STPTI_HANDLE_INVALID_OBJECT_NUMBER;
	return (pDeviceHandle);
}

/**
   @brief  Convert a Full Object Handle into a Full vDevice Handle.

   This function returns the vDevice Handle for a specified Object.

   @param  FullObjectHandle  The Object Handle to Convert

   @return                   A FullHandle of the vDevice

 */
FullHandle_t stptiOBJMAN_ConvertFullObjectHandleToFullvDeviceHandle(FullHandle_t FullObjectHandle)
{
	FullHandle_t vDeviceHandle = stptiOBJMAN_NullHandle;
	vDeviceHandle.member.pDevice = FullObjectHandle.member.pDevice;
	vDeviceHandle.member.vDevice = FullObjectHandle.member.vDevice;
	vDeviceHandle.member.Session = STPTI_HANDLE_INVALID_SESSION_NUMBER;
	vDeviceHandle.member.ObjectType = OBJECT_TYPE_VDEVICE;
	vDeviceHandle.member.Object = STPTI_HANDLE_INVALID_OBJECT_NUMBER;
	return (vDeviceHandle);
}

/**
   @brief  Convert a Full Object Handle into a Full Session Handle.

   This function returns the Session Handle for a specified Object.

   @param  FullObjectHandle  The Object Handle to Convert

   @return                   A FullHandle of the Session

 */
FullHandle_t stptiOBJMAN_ConvertFullObjectHandleToFullSessionHandle(FullHandle_t FullObjectHandle)
{
	FullHandle_t SessionHandle = stptiOBJMAN_NullHandle;
	SessionHandle.member.pDevice = FullObjectHandle.member.pDevice;
	SessionHandle.member.vDevice = FullObjectHandle.member.vDevice;
	SessionHandle.member.Session = FullObjectHandle.member.Session;
	SessionHandle.member.ObjectType = OBJECT_TYPE_SESSION;
	SessionHandle.member.Object = STPTI_HANDLE_INVALID_OBJECT_NUMBER;
	return (SessionHandle);
}

/**
   @brief  Check a Handle is valid

   This function checks a handle is valid (points to an allocated object), and matches a specified
   ObjectType.

   Note that we check for both the NullHandle (0xFFFFFFFF) as well as making sure that zero handles
   (0x00000000) will be rejected.

   @param  Handle      Handle to check
   @param  ObjectType  The ObjectType to check against

   @return             A standard st error type...
                       - ST_NO_ERROR if no errors
                       - ST_ERROR_INVALID_HANDLE if the handle is invalid
 */
ST_ErrorCode_t stptiOBJMAN_CheckHandle(FullHandle_t Handle, ObjectType_t ObjectType)
{
	if ((ObjectType != OBJECT_TYPE_ANY) && (ObjectType != stptiOBJMAN_GetObjectType(Handle))) {
		return (ST_ERROR_INVALID_HANDLE);
	}

	if (stptiOBJMAN_HandleToObjectPointerWithChecks(Handle, ObjectType) == NULL) {
		return (ST_ERROR_INVALID_HANDLE);
	}

	return (ST_NO_ERROR);
}

/**
   @brief  Convert and handle to an Object pointer (with checks)

   This function returns a object pointer.  This function is mainly used for type checking for
   returning object structures.

   Note that we check for both the NullHandle (0xFFFFFFFF) as well as making sure that zero handles
   (0x00000000) will be rejected.

   @param  Handle      Handle of Object
   @param  ObjectType  The ObjectType to return

   @return             A object pointer, or NULL if there is a failure.

 */
Object_t *stptiOBJMAN_HandleToObjectPointerWithChecks(FullHandle_t Handle, ObjectType_t ObjectType)
{
	Object_t *Object_p = NULL;
	Object_t *pDeviceObject_p;

	/* Check a NULL Handle has not been supplied */
	if (stptiOBJMAN_IsNullHandle(Handle)) {
		stpti_printf("Null Handle supplied. [%08x]", Handle.word);
		return (NULL);
	}

	/* pDevice number is not larger than the number of vDevices */
	if (Handle.member.pDevice >= STPTI_Driver.NumberOfpDevices) {
		stpti_printf("Referenced vDevice exceeds pDevice limits [%08x]", Handle.word);
		return (NULL);
	}

	/* OBJECT_TYPE_INVALID is zero - which means that zero handles will be rejected */
	if (Handle.member.ObjectType == OBJECT_TYPE_INVALID) {
		stpti_printf("Null (zero) Handle supplied. [%08x]", Handle.word);
		return (NULL);
	}

	pDeviceObject_p = STPTI_Driver.ObjMan[Handle.member.pDevice]->pDevice_p;

	if (ObjectType == OBJECT_TYPE_PDEVICE) {
		Object_p = pDeviceObject_p;
	} else {
		Object_t *vDeviceObject_p;

		/* Session number is not larger than the number of sessions */
		if (Handle.member.vDevice >= stptiOBJMAN_GetListCapacity(&pDeviceObject_p->ChildObjects)) {
			stpti_printf("Referenced vDevice does not exist [%08x]", Handle.word);
			return (NULL);
		}

		vDeviceObject_p =
		    (Object_t *) stptiOBJMAN_GetItemFromList(&pDeviceObject_p->ChildObjects, Handle.member.vDevice);
		if (vDeviceObject_p == NULL) {
			stpti_printf("Referenced vDevice does not exist [%08x]", Handle.word);
			return (NULL);
		}

		if (ObjectType == OBJECT_TYPE_VDEVICE) {
			Object_p = vDeviceObject_p;
		} else {
			Object_t *SessionObject_p;

			/* Session number is not larger than the number of sessions */
			if (Handle.member.Session >= stptiOBJMAN_GetListCapacity(&vDeviceObject_p->ChildObjects)) {
				stpti_printf("Referenced Session does not exist [%08x]", Handle.word);
				return (NULL);
			}

			SessionObject_p =
			    (Object_t *) stptiOBJMAN_GetItemFromList(&vDeviceObject_p->ChildObjects,
								     Handle.member.Session);
			if (SessionObject_p == NULL) {
				stpti_printf("Referenced Session does not exist [%08x]", Handle.word);
				stptiOBJMAN_PrintList(&vDeviceObject_p->ChildObjects);
				return (NULL);
			}

			if (ObjectType == OBJECT_TYPE_SESSION) {
				Object_p = SessionObject_p;
			} else {
				/* Object number is not larger than the number of objects */
				if (Handle.member.Object >= stptiOBJMAN_GetListCapacity(&SessionObject_p->ChildObjects)) {
					stpti_printf("Referenced Object does not exist [%08x]", Handle.word);
					return (NULL);
				}

				Object_p =
				    (Object_t *) stptiOBJMAN_GetItemFromList(&SessionObject_p->ChildObjects,
									     Handle.member.Object);
				if (Object_p == NULL) {
					stpti_printf("Referenced Object does not exist [%08x]", Handle.word);
					return (NULL);
				}

				/* See if object handle matches stated Object Type */
				if ((Handle.member.ObjectType != ObjectType) && (ObjectType != OBJECT_TYPE_ANY)) {
					stpti_printf
					    ("Referenced Object type is not the same as stated Object type [%08x,%d]",
					     Handle.word, ObjectType);
					return (NULL);
				}
			}
		}
	}
	return (Object_p);
}

/**
   @brief  Convert and handle to an Object pointer (without checks)

   This function returns a object pointer.  This function is quick, but no checks are performed on
   the handle.

   @param  Handle      Handle of Object
   @param  ObjectType  The ObjectType to return

   @return             A object pointer

 */
Object_t *stptiOBJMAN_HandleToObjectPointerWithoutChecks(FullHandle_t Handle, ObjectType_t ObjectType)
{
	Object_t *Object_p = NULL;

	/* Always check for a NULL handle as it is a convenient feature to use */
	if (!stptiOBJMAN_IsNullHandle(Handle)) {
		Object_t *pDeviceObject_p = STPTI_Driver.ObjMan[Handle.member.pDevice]->pDevice_p;

		if (ObjectType == OBJECT_TYPE_PDEVICE) {
			Object_p = pDeviceObject_p;
		} else {
			Object_t *vDeviceObject_p =
			    (Object_t *) stptiOBJMAN_GetItemFromList(&pDeviceObject_p->ChildObjects,
								     Handle.member.vDevice);

			if (ObjectType == OBJECT_TYPE_VDEVICE) {
				Object_p = vDeviceObject_p;
			} else {
				Object_t *SessionObject_p =
				    (Object_t *) stptiOBJMAN_GetItemFromList(&vDeviceObject_p->ChildObjects,
									     Handle.member.Session);

				if (ObjectType == OBJECT_TYPE_SESSION) {
					Object_p = SessionObject_p;
				} else {
					/* We always check the ObjectType is it will pickup the Lion's share of handle issues */
					if ((Handle.member.ObjectType == ObjectType) || (ObjectType == OBJECT_TYPE_ANY)) {
						Object_p =
						    (Object_t *) stptiOBJMAN_GetItemFromList(&SessionObject_p->ChildObjects,
											     Handle.member.Object);
					} else {
						stpti_printf("Referenced Object type is not the same as stated Object type [%08x,%d]",
						     Handle.word, ObjectType);
					}
				}
			}
		}
	}
	return (Object_p);
}

/**
   @brief  Register functions for the various actions that can happen to an object.

   This function sets the object action functions.  This is done for each pDevice, to allow multiple
   HALs to be used simultaneously.

   @param  pDeviceIndex               The index of the pDevice
   @param  ObjectType                 Object's Type (class)
   @param  ObjectSize                 The size of the object in bytes (i.e. sizeof)

   @param  ObjectManagementFunctions  A pointer to a structure containing the functions used upon
                                      allocation, association, disassociation, deallocation of this
                                      object.  If no action is required any function can be NULL.

   @return               ST_NO_ERROR

 */
ST_ErrorCode_t stptiOBJMAN_RegisterObjectType(unsigned int pDeviceIndex, ObjectType_t ObjectType, size_t ObjectSize,
					      ObjectManagerFunctions_t * ObjectManagementFunctions_p)
{
	if ((ObjectType >= NUMBER_OF_OBJECT_TYPES) || (ObjectSize == 0)) {
		return (ST_ERROR_BAD_PARAMETER);
	}

	STPTI_Driver.ObjMan[pDeviceIndex]->ObjectSize[ObjectType] = ObjectSize;
	STPTI_Driver.ObjMan[pDeviceIndex]->HAL_id = stptiHAL_IDENTIFIER;

	/* Object Management functions are allowed to be unassigned (NULL) if they are not relevent.
	   We reassign unassigned Object Management functions to the Dummy ones to aid debugging and make
	   the object manipulation functions simplier (by avoiding having to add checking). */
	{
		ObjectManagerFunctions_t *p = &STPTI_Driver.ObjMan[pDeviceIndex]->ObjFunPtr[ObjectType];

		p->Allocator =
		    (ObjectManagementFunctions_p->Allocator ==
		     NULL) ? stptiOBJMAN_DummyAllocator : ObjectManagementFunctions_p->Allocator;
		p->Associator =
		    (ObjectManagementFunctions_p->Associator ==
		     NULL) ? stptiOBJMAN_DummyAssociator : ObjectManagementFunctions_p->Associator;
		p->Disassociator =
		    (ObjectManagementFunctions_p->Disassociator ==
		     NULL) ? stptiOBJMAN_DummyDisassociator : ObjectManagementFunctions_p->Disassociator;
		p->Deallocator =
		    (ObjectManagementFunctions_p->Deallocator ==
		     NULL) ? stptiOBJMAN_DummyDeallocator : ObjectManagementFunctions_p->Deallocator;
		p->Serialiser =
		    (ObjectManagementFunctions_p->Serialiser ==
		     NULL) ? stptiOBJMAN_DummySerialiseObject : ObjectManagementFunctions_p->Serialiser;
		p->Deserialiser =
		    (ObjectManagementFunctions_p->Deserialiser ==
		     NULL) ? stptiOBJMAN_DummyDeserialiseObject : ObjectManagementFunctions_p->Deserialiser;

		/* Once copied across, we assign these functions to NULL to cause an exception if someone
		   accidentally calls them using stptiHAL_call().  The Object Manager must keep track of calls
		   made to these functions as they affect object relationships. */
		ObjectManagementFunctions_p->Allocator = NULL;
		ObjectManagementFunctions_p->Associator = NULL;
		ObjectManagementFunctions_p->Disassociator = NULL;
		ObjectManagementFunctions_p->Deallocator = NULL;
		ObjectManagementFunctions_p->Serialiser = NULL;
		ObjectManagementFunctions_p->Deserialiser = NULL;
	}

	return (ST_NO_ERROR);
}

/**
   @brief  Allocate (create) an Object

   This function is the interface to stptiOBJMAN_AllocateObjectInternal, used for Allocating an Object.
   It will apply the Write Lock, before calling the function.

   Allocate is special in that this function calls the Allocate function directly rather than via
   stptiOBJMAN_AllocateObjectInternal.  This is because stptiOBJMAN_AllocateObjectInternal is reused
   for stptiOBJMAN_DeserialiseObject, which has a similar objective, but a different configuration
   function.  It also helps to simplify the already complex Allocation function.

   @param  ParentObjectHandle  The parents ObjectHandle (pDevice for vDevice, vDevice for Session,
                               Session for other objects)
   @param  NewObjectType       The type of the object to be allocated
   @param  NewObjectHandle_p   This is where the new object's handle will be returned
   @param  params              A pointer to a structure that will hold parameters understood by
                               the objects allocation function.

   @return                     A standard st error type...
                               - ST_NO_ERROR
                               - ST_ERROR_BAD_PARAMETER
                               - ST_ERROR_NO_MEMORY       (no space to allocate object)
                               Or a variety of other errors from the object's allocation function
 */
ST_ErrorCode_t stptiOBJMAN_AllocateObject(FullHandle_t ParentObjectHandle, ObjectType_t NewObjectType,
					  FullHandle_t * NewObjectHandle_p, void *params_p)
{
	ST_ErrorCode_t Error = ST_NO_ERROR;
	stptiSupport_AccessLockState_t LocalLockState = stptiSupport_UNLOCKED;

	stptiOBJMAN_WriteLock(ParentObjectHandle, &LocalLockState);
	{
		FullHandle_t AllocatedObjectFullHandle = stptiOBJMAN_NullHandle;

		Error =
		    stptiOBJMAN_AllocateObjectInternal(ParentObjectHandle, NewObjectType, &AllocatedObjectFullHandle);

		if (ST_NO_ERROR == Error) {
			unsigned int pDevice = ParentObjectHandle.member.pDevice;

			/* Allocate Object */
			Error =
			    (*STPTI_Driver.ObjMan[pDevice]->ObjFunPtr[NewObjectType].
			     Allocator) (AllocatedObjectFullHandle, params_p);

			if (ST_NO_ERROR != Error) {
				/* Purposefully ignore error returned to preserve original error */
				stptiOBJMAN_DeallocateObjectInternal(AllocatedObjectFullHandle,
								     FALSE /*CallDeallocator */ , TRUE, TRUE);
				AllocatedObjectFullHandle = stptiOBJMAN_NullHandle;
			}
		}

		*NewObjectHandle_p = AllocatedObjectFullHandle;
	}
	stptiOBJMAN_Unlock(ParentObjectHandle, &LocalLockState);

	return (Error);
}

/**
   @brief  Associate two Objects

   This function is the interface to stptiOBJMAN_AssociateObjectsInternal, used for Associating objects.
   It will apply the Write Lock, before calling the function.

   It is important to recognise that BOTH objects' Association functions will be called.  The call
   to the Association functions is done in stptiOBJMAN_AssociateObjectsInternal().

   @param  Object1Handle  The handle of the first object being associated.
   @param  Object1Handle  The handle of the second object being associated.

   @return                A standard st error type...
                          - ST_NO_ERROR
                          - ST_ERROR_BAD_PARAMETER
                          Or a variety of other errors from each object's association function
 */
ST_ErrorCode_t stptiOBJMAN_AssociateObjects(FullHandle_t Object1Handle, FullHandle_t Object2Handle)
{
	ST_ErrorCode_t Error = ST_NO_ERROR;
	stptiSupport_AccessLockState_t LocalLockState = stptiSupport_UNLOCKED;

	stptiOBJMAN_WriteLock(Object1Handle, &LocalLockState);
	{
		Error = stptiOBJMAN_AssociateObjectsInternal(Object1Handle, Object2Handle);
	}
	stptiOBJMAN_Unlock(Object1Handle, &LocalLockState);

	return (Error);
}

/**
   @brief  Disassociate two Objects

   This function is the interface to stptiOBJMAN_DisassociateObjectsInternal, used for Disassociating
   objects.  It will apply the Write Lock, before calling the function.

   It is important to recognise that BOTH objects' Disassociation functions will be called.  The call
   to the Disassociation functions is done in stptiOBJMAN_DisassociateObjectsInternal().

   @param  Object1Handle  The handle of the first object being disassociated.
   @param  Object1Handle  The handle of the second object being disassociated.

   @return                A standard st error type...
                          - ST_NO_ERROR
                          - ST_ERROR_BAD_PARAMETER
                          Or a variety of other errors from each object's disassociation function
 */
ST_ErrorCode_t stptiOBJMAN_DisassociateObjects(FullHandle_t Object1Handle, FullHandle_t Object2Handle)
{
	ST_ErrorCode_t Error = ST_NO_ERROR;
	stptiSupport_AccessLockState_t LocalLockState = stptiSupport_UNLOCKED;

	stptiOBJMAN_WriteLock(Object1Handle, &LocalLockState);
	{
		Error = stptiOBJMAN_DisassociateObjectsInternal(Object1Handle, Object2Handle);
	}
	stptiOBJMAN_Unlock(Object1Handle, &LocalLockState);

	return (Error);
}

/**
   @brief  Deallocated (Destroy) an Object

   This function is the interface to stptiOBJMAN_DeallocateObjectInternal, used for Deallocating
   objects.  It will apply the Write Lock, before calling the function.

   Instead of this function calling the deallocator, it is the stptiOBJMAN_DeallocateObjectInternal
   that calls the deallocator.  This is done to allow recursion, so that when you deallocate a
   parent, all it's children get disassociated and deallocated too.

   @param  ObjectHandle  The handle of the object being deallocated.
   @param  Force         If TRUE it will deallocate the object even if it has children.

   @return               A standard st error type...
                         - ST_NO_ERROR
                         - ST_ERROR_BAD_PARAMETER
                         - ST_ERROR_DEVICE_BUSY    (cannot deallocate as has children, Force=FALSE)
                         Or a variety of other errors from each object's disassociation function
 */
ST_ErrorCode_t stptiOBJMAN_DeallocateObject(FullHandle_t ObjectHandle, BOOL Force)
{
	ST_ErrorCode_t Error = ST_NO_ERROR;
	stptiSupport_AccessLockState_t LocalLockState = stptiSupport_UNLOCKED;

	stptiOBJMAN_WriteLock(ObjectHandle, &LocalLockState);
	{
		stptiHAL_debug_rem_object(ObjectHandle);
		Error = stptiOBJMAN_DeallocateObjectInternal(ObjectHandle, TRUE, Force, TRUE);
	}
	stptiOBJMAN_Unlock(ObjectHandle, &LocalLockState);

	return (Error);
}

/**
   @brief  Serialise an Object

   This function is used to get a serialised version of the object to restore its state by
   deserialisation.

   Note that the object must be fully disassociated.  You will need to store the Object's
   relationships separately.

   You will need to specify a mode for the serialisation.  This should be 0 for full
   serialisation (such as passive standby), or 1 for partial serialisation (such as object
   transfer).  The latter will save on memory deallocations (for example for buffers it
   won't deallocate the actual buffer memory).

   @param  ObjectHandle          Handle of the Object to Serialise
   @param  mode                  The method of deserialisation (passed to deserialiser), usually 0
   @param  Region_p              A pointer to a region of memory to put the serialised object
   @param  RegionSize            The size of the region of memory referenced by Region_p (in bytes)
   @param  SerialisedObjectSize  (return) The size of the serialised object returned.

   @return                       A standard st error type...
                                 - ST_NO_ERROR
                                 - ST_ERROR_BAD_PARAMETER
                                 - STPTI_ERROR_OBJECT_ASSOCIATED  (object has associations)
                                 - STPTI_ERROR_OBJECT_HAS_CHILDREN  (object has children)
                                 - ST_ERROR_NO_MEMORY       (no space to store serialised object)
 */
ST_ErrorCode_t stptiOBJMAN_SerialiseObject(FullHandle_t ObjectHandle, int mode, void *Region_p, size_t RegionSize,
					   size_t * SerialisedObjectSize_p)
{
	ST_ErrorCode_t Error = ST_NO_ERROR;
	stptiSupport_AccessLockState_t LocalLockState = stptiSupport_UNLOCKED;

	if (stptiOBJMAN_CountAssociatedObjects(ObjectHandle, OBJECT_TYPE_ANY) != 0) {
		return (STPTI_ERROR_OBJECT_ASSOCIATED);
	}

	if (stptiOBJMAN_CountChildObjects(ObjectHandle, OBJECT_TYPE_ANY) != 0) {
		return (STPTI_ERROR_OBJECT_HAS_CHILDREN);
	}

	stptiOBJMAN_WriteLock(ObjectHandle, &LocalLockState);
	{
		Object_t *Object_p =
		    stptiOBJMAN_HandleToObjectPointer(ObjectHandle, stptiOBJMAN_GetObjectType(ObjectHandle));

		/* Format of SerialisedObject (packed)...
		 *   U32 CONST_0x12345678  (to aid debug, and endian issues)
		 *   U8  HAL_id
		 *   U8  ObjectType
		 *   U8  Mode
		 *   U32 AuxilaryDataSize
		 *   U8  AuxilaryData[]
		 *   U32 SerialisedObjectSize
		 *   U8  SerialisedObjectData[]
		 */

		unsigned int ObjMetaDataSize = 10 + (unsigned int)Object_p->AuxiliaryDataSize + 4;	/* excludes SerialisedObjectData[] */

		/* Region must be at least a byte large */
		if (RegionSize < ObjMetaDataSize) {
			Error = ST_ERROR_NO_MEMORY;
		} else {
			unsigned int pDevice = ObjectHandle.member.pDevice;
			ObjectType_t ObjectType = stptiOBJMAN_GetObjectType(ObjectHandle);
			U8 *p = Region_p;
			U8 u8v;
			U32 u32v;

			/* Store marker for debug purposes (also an endian indicator) */
			u32v = 0x12345678;
			memcpy(p, &u32v, sizeof(u32v));
			p += sizeof(u32v);

			/* HAL_id */
			u8v = (U8) STPTI_Driver.ObjMan[pDevice]->HAL_id;
			memcpy(p, &u8v, sizeof(u8v));
			p += sizeof(u8v);

			/* Store ObjectType for error checking purposes */
			u8v = (U8) ObjectType;
			memcpy(p, &u8v, sizeof(u8v));
			p += sizeof(u8v);

			/* mode affects the way the object is serialised/deserialised */
			u8v = (U8) mode;
			memcpy(p, &u8v, sizeof(u8v));
			p += sizeof(u8v);

			/* We also store the AuxiliaryData */
			u32v = (U32) Object_p->AuxiliaryDataSize;
			memcpy(p, &u32v, sizeof(u32v));
			p += sizeof(u32v);
			if (Object_p->AuxiliaryDataSize > 0) {
				memcpy(p, Object_p->AuxiliaryData_p, Object_p->AuxiliaryDataSize);
				p += Object_p->AuxiliaryDataSize;
			}

			/* Call serialiser, leaving room for the size of the data returned */
			Error =
			    (*STPTI_Driver.ObjMan[pDevice]->ObjFunPtr[ObjectType].Serialiser) (ObjectHandle, mode,
											       p + 4,
											       RegionSize - ObjMetaDataSize, &u32v);
			if (ST_NO_ERROR == Error) {
				/* Add size of SerialisedObject (for debugging purposes */
				memcpy(p, &u32v, sizeof(u32v));

				/* Note unlike the internal allocation fn, this will call the object's deallocation fn */
				Error = stptiOBJMAN_DeallocateObjectInternal(ObjectHandle, TRUE, TRUE, TRUE);

				if (SerialisedObjectSize_p != NULL) {
					*SerialisedObjectSize_p = u32v + ObjMetaDataSize;
				}
			}
		}
	}
	stptiOBJMAN_Unlock(ObjectHandle, &LocalLockState);

	return (Error);
}

/**
   @brief  Allocate (create) an Object by Deserialising it

   This function is a second interface to stptiOBJMAN_AllocateObjectInternal, used for Allocating
   an Object.  It will apply the Write Lock, before calling the function.  Its purpose is to also
   restore state for a serialised object.

   @param  ParentObjectHandle  The parents ObjectHandle (pDevice for vDevice, vDevice for Session,
                               Session for other objects)
   @param  NewObjectType       The type of the object to be allocated
   @param  NewObjectHandle_p   This is where the new object's handle will be returned
   @param  Region_p            A pointer to the serialised Object.

   @return                     A standard st error type...
                               - ST_NO_ERROR
                               - ST_ERROR_BAD_PARAMETER
                               - ST_ERROR_NO_MEMORY       (no space to allocate object)
                               Or a variety of other errors from the object's Deserialisation function
 */
ST_ErrorCode_t stptiOBJMAN_DeserialiseObject(FullHandle_t ParentObjectHandle, ObjectType_t NewObjectType,
					     FullHandle_t * NewObjectHandle_p, void *Region_p)
{
	ST_ErrorCode_t Error = ST_NO_ERROR;
	stptiSupport_AccessLockState_t LocalLockState = stptiSupport_UNLOCKED;

	stptiOBJMAN_WriteLock(ParentObjectHandle, &LocalLockState);
	{
		FullHandle_t AllocatedObjectFullHandle = stptiOBJMAN_NullHandle;
		unsigned int pDevice = ParentObjectHandle.member.pDevice;

		/* Format of SerialisedObject (packed)...
		 *   U32 CONST_0x12345678  (to aid debug, and endian issues)
		 *   U8  HAL_id
		 *   U8  ObjectType
		 *   U8  Mode
		 *   U32 AuxilaryDataSize
		 *   U8  AuxilaryData[]
		 *   U32 SerialisedObjectSize
		 *   U8  SerialisedObjectData[]
		 */

		U8 *p = Region_p;
		U8 u8v, hal_id;
		U32 u32v;

		/* CONST_0x12345678 marker */
		memcpy(&u32v, p, sizeof(u32v));
		p += sizeof(u32v);

		/* HAL_id (U8) */
		memcpy(&hal_id, p, sizeof(hal_id));
		p += sizeof(hal_id);

		/* ObjectType */
		memcpy(&u8v, p, sizeof(u8v));
		p += sizeof(u8v);

		/* Do some checks, since the repercussions are significant */
		if (u32v != 0x12345678) {
			STPTI_PRINTF_ERROR("Error trying to Deserialise Object, Missing Marker [0x%x!=0x12345678]",
					   u32v);
			Error = STPTI_ERROR_CORRUPT_DATA_IN_BUFFER;
		} else if (hal_id != (U8) STPTI_Driver.ObjMan[pDevice]->HAL_id) {
			STPTI_PRINTF_ERROR("Error trying to Deserialise Object, into a different HAL");
			Error = ST_ERROR_FEATURE_NOT_SUPPORTED;
		} else if (u8v != (U8) NewObjectType) {
			STPTI_PRINTF_ERROR("Error trying to Deserialise Object, Wrong ObjectType [%u!=%u]",
					   (unsigned)u8v, (unsigned)NewObjectType);
			Error = STPTI_ERROR_CORRUPT_DATA_IN_BUFFER;
		}

		if (ST_NO_ERROR == Error) {
			int mode;
			size_t AuxiliaryDataSize = 0;

			/* mode */
			memcpy(&u8v, p, sizeof(u8v));
			p += sizeof(u8v);
			mode = (int)u8v;

			/* AuxilaryDataSize */
			memcpy(&u32v, p, sizeof(u32v));
			p += sizeof(u32v);
			AuxiliaryDataSize = (size_t) u32v;

			Error =
			    stptiOBJMAN_AllocateObjectInternal(ParentObjectHandle, NewObjectType,
							       &AllocatedObjectFullHandle);
			if (ST_NO_ERROR == Error) {
				void *aux_p =
				    stptiOBJMAN_AllocateObjectAuxiliaryDataInternal(AllocatedObjectFullHandle,
										    AuxiliaryDataSize);
				memcpy(aux_p, p, AuxiliaryDataSize);
				p += AuxiliaryDataSize;
			}

			if (ST_NO_ERROR == Error) {
				/* Allocate Object via the Deserialiser (skipping the SerialisedObjectSize field) */
				Error =
				    (*STPTI_Driver.ObjMan[pDevice]->ObjFunPtr[NewObjectType].
				     Deserialiser) (AllocatedObjectFullHandle, p + 4, mode);
			}

			if (ST_NO_ERROR != Error) {
				/* Purposefully ignore error returned to preserve original error */
				stptiOBJMAN_DeallocateObjectInternal(AllocatedObjectFullHandle,
								     FALSE /*CallDeallocator */ , TRUE, TRUE);
				AllocatedObjectFullHandle = stptiOBJMAN_NullHandle;
			}
		}

		*NewObjectHandle_p = AllocatedObjectFullHandle;
	}
	stptiOBJMAN_Unlock(ParentObjectHandle, &LocalLockState);

	return (Error);
}

/* The Base functions for Object Creation (Allocation), Destruction (Deallocation), Association and Disassociation */

/**
   @brief  Manage infrastructure for Allocating (create) an Object

   This function allocates space for an Object, connects it to the object hierarchy, determines it's
   handle.

   NOTE that unlike the equivalent association, disassociation, deallocation functions it does not
   call the Object's Allocator function (that is done in the caller stptiOBJMAN_AllocateObject).

   @param  ParentObjectHandle  The parents ObjectHandle (pDevice for vDevice, vDevice for Session,
                               Session for other objects)
   @param  NewObjectType       The type of the object to be allocated
   @param  NewObjectHandle_p   This is where the new object's handle will be returned
   @param  params              A pointer to a structure that will hold parameters understood by
                               the objects allocation function.

   @return                     A standard st error type...
                               - ST_NO_ERROR
                               - ST_ERROR_BAD_PARAMETER
                               - ST_ERROR_NO_MEMORY       (no space to allocate object)
                               Or a variety of other errors from the object's allocation function
 */
static ST_ErrorCode_t stptiOBJMAN_AllocateObjectInternal(FullHandle_t ParentObjectHandle, ObjectType_t NewObjectType,
							 FullHandle_t * NewObjectHandle_p)
{
	ST_ErrorCode_t Error = ST_NO_ERROR;
	Object_t *NewObject_p;
	int pDevice = (int)ParentObjectHandle.member.pDevice;

	/* Be defensive in case errors occur and we have to bail out early */
	*NewObjectHandle_p = stptiOBJMAN_NullHandle;

	if (pDevice >= STPTI_Driver.NumberOfpDevices) {
		STPTI_PRINTF_ERROR
		    ("Trying to allocate on Unknown pDevice %u.  The driver is configured only for %d pDevices.",
		     pDevice, STPTI_Driver.NumberOfpDevices);
		return (ST_ERROR_BAD_PARAMETER);
	}

	if (NewObjectType >= NUMBER_OF_OBJECT_TYPES || STPTI_Driver.ObjMan[pDevice]->ObjectSize[NewObjectType] == 0) {
		STPTI_PRINTF("Trying to allocate unregistered Object of type %d.", NewObjectType);
		return (ST_ERROR_BAD_PARAMETER);
	}

	/* Objects other than vDevices and Sessions get the partition from the parent (a session) */
	if ((NewObjectType != OBJECT_TYPE_PDEVICE) && (NewObjectType != OBJECT_TYPE_VDEVICE)
	    && (NewObjectType != OBJECT_TYPE_SESSION)) {
		if (stptiOBJMAN_GetObjectType(ParentObjectHandle) != OBJECT_TYPE_SESSION) {
			STPTI_PRINTF_ERROR("Objects must have session parent handles.");
			return (ST_ERROR_BAD_PARAMETER);
		}
	}

	NewObject_p = (Object_t *) stptiOBJMAN_AllocateMemory(STPTI_Driver.ObjMan[pDevice]->ObjectSize[NewObjectType]);
	if (NewObject_p == NULL) {
		return (ST_ERROR_NO_MEMORY);
	} else {
		stpti_printf("MALLOCed %ubytes (%p)", (unsigned)STPTI_Driver.ObjMan[pDevice]->ObjectSize[NewObjectType],
			     NewObject_p);
	}
	memset(NewObject_p, 0, STPTI_Driver.ObjMan[pDevice]->ObjectSize[NewObjectType]);

	NewObject_p->Handle = stptiOBJMAN_NullHandle;
	NewObject_p->AuxiliaryData_p = NULL;
	NewObject_p->AuxiliaryDataSize = 0;

	switch (NewObjectType) {
	case OBJECT_TYPE_PDEVICE:
		if (STPTI_Driver.ObjMan[pDevice]->pDevice_p != NULL) {
			Error = ST_ERROR_NO_MEMORY;
		} else {
			STPTI_Driver.ObjMan[pDevice]->pDevice_p = NewObject_p;
			NewObject_p->Handle = stptiOBJMAN_pDeviceObjectHandle(pDevice);

			/* pDevices can't be associated so we don't allocate a list (and mark invalid for bombproofing) */
			NewObject_p->AssociatedObjects.MaxItems = 0;

			/* Allocate a Child List */
			Error = stptiOBJMAN_AllocateList(&NewObject_p->ChildObjects);
			if (Error != ST_NO_ERROR) {
				/* There was a problem, need to remove this vDevice from the vDevice List */
				STPTI_Driver.ObjMan[pDevice]->pDevice_p = NULL;
			}
		}
		break;

	case OBJECT_TYPE_VDEVICE:
		if (ParentObjectHandle.member.ObjectType != OBJECT_TYPE_PDEVICE) {
			Error = ST_ERROR_BAD_PARAMETER;
		} else {
			int index_device;
			Object_t *pDeviceObject_p =
			    stptiOBJMAN_HandleToObjectPointer(ParentObjectHandle, OBJECT_TYPE_PDEVICE);

			Error =
			    stptiOBJMAN_AddToList(&pDeviceObject_p->ChildObjects, NewObject_p, FALSE, &index_device);
			if (index_device > MAX_NUMBER_OF_VDEVICES || ST_NO_ERROR != Error) {
				Error = ST_ERROR_NO_MEMORY;
			} else {
				NewObject_p->Handle.member.pDevice = (unsigned int)pDevice;
				NewObject_p->Handle.member.vDevice = index_device;
				NewObject_p->Handle.member.Session = STPTI_HANDLE_INVALID_SESSION_NUMBER;
				NewObject_p->Handle.member.ObjectType = NewObjectType;
				NewObject_p->Handle.member.Object = STPTI_HANDLE_INVALID_OBJECT_NUMBER;

				/* vDevices can't be associated so we don't allocate a list (and mark invalid for bombproofing) */
				NewObject_p->AssociatedObjects.MaxItems = 0;

				/* Allocate a Child List */
				Error = stptiOBJMAN_AllocateList(&NewObject_p->ChildObjects);
				if (Error != ST_NO_ERROR) {
					/* There was a problem, need to remove this vDevice from the vDevice List */
					stptiOBJMAN_RemoveFromList(&pDeviceObject_p->ChildObjects, index_device);
				}
			}
		}
		break;

	case OBJECT_TYPE_SESSION:
		if (stptiOBJMAN_GetObjectType(ParentObjectHandle) != OBJECT_TYPE_VDEVICE) {
			Error = ST_ERROR_BAD_PARAMETER;
		} else {
			int index_session;
			Object_t *vDeviceObject_p =
			    stptiOBJMAN_HandleToObjectPointer(ParentObjectHandle, OBJECT_TYPE_VDEVICE);

			Error =
			    stptiOBJMAN_AddToList(&vDeviceObject_p->ChildObjects, NewObject_p, FALSE, &index_session);
			if (index_session >= MAX_NUMBER_OF_SESSIONS || ST_NO_ERROR != Error) {
				stptiOBJMAN_RemoveFromList(&vDeviceObject_p->ChildObjects, index_session);
				Error = ST_ERROR_NO_FREE_HANDLES;
			}
			if (Error == ST_NO_ERROR) {
				NewObject_p->Handle.member.pDevice = pDevice;
				NewObject_p->Handle.member.vDevice = vDeviceObject_p->Handle.member.vDevice;
				NewObject_p->Handle.member.Session = index_session;
				NewObject_p->Handle.member.ObjectType = NewObjectType;
				NewObject_p->Handle.member.Object = STPTI_HANDLE_INVALID_OBJECT_NUMBER;

				/* Sessions can't be associated so we don't allocate a list (and mark invalid for bombproofing) */
				NewObject_p->AssociatedObjects.MaxItems = 0;

				/* Allocate a Child List */
				Error = stptiOBJMAN_AllocateList(&NewObject_p->ChildObjects);
				if (Error != ST_NO_ERROR) {
					/* There was a problem, need to remove this session object from the Session List */
					stptiOBJMAN_RemoveFromList(&vDeviceObject_p->ChildObjects, index_session);
				}
			}
		}
		break;

	default:
		if (stptiOBJMAN_GetObjectType(ParentObjectHandle) != OBJECT_TYPE_SESSION) {
			Error = ST_ERROR_BAD_PARAMETER;
		} else {
			int index;
			Object_t *SessionObject_p =
			    stptiOBJMAN_HandleToObjectPointer(ParentObjectHandle, OBJECT_TYPE_SESSION);

			Error = stptiOBJMAN_AddToList(&SessionObject_p->ChildObjects, NewObject_p, FALSE, &index);
			if (index >= STPTI_HANDLE_INVALID_OBJECT_NUMBER || ST_NO_ERROR != Error) {
				stptiOBJMAN_RemoveFromList(&SessionObject_p->ChildObjects, index);
				Error = ST_ERROR_NO_FREE_HANDLES;
			}
			if (Error == ST_NO_ERROR) {
				NewObject_p->Handle.member.pDevice = pDevice;
				NewObject_p->Handle.member.vDevice = SessionObject_p->Handle.member.vDevice;
				NewObject_p->Handle.member.Session = SessionObject_p->Handle.member.Session;
				NewObject_p->Handle.member.ObjectType = NewObjectType;
				NewObject_p->Handle.member.Object = index;

				/* Objects other than vDevices and Session don't have children so we don't allocate a list (and mark it invalid for bombproofing) */
				NewObject_p->ChildObjects.MaxItems = 0;

				/* Allocate an association list */
				Error = stptiOBJMAN_AllocateList(&NewObject_p->AssociatedObjects);
				if (Error != ST_NO_ERROR) {
					/* There was a problem, need to remove this object from the Object List */
					stptiOBJMAN_RemoveFromList(&SessionObject_p->ChildObjects, index);
				}
			}
		}
		break;
	}

	if (Error == ST_NO_ERROR) {
		*NewObjectHandle_p = NewObject_p->Handle;
#if defined( STPTI_IPRC_PRESENT )
		Error =
		    STCOMMON_HandleAlloc(STPTI_DRIVER_ID, (void *)NewObject_p->Handle.word, &NewObject_p->APIHandle);
		if (Error != ST_NO_ERROR) {
			/* No system handles. We'll have to deallocate fully */
			stptiOBJMAN_DeallocateObjectInternal(NewObject_p->Handle, TRUE, TRUE,
							     FALSE /*ReleaseHandle */ );
		}
#endif
	} else {
		/* If there was an Error free the memory used by the object */
		stptiOBJMAN_DeallocateMemory((void *)NewObject_p,
					     STPTI_Driver.ObjMan[pDevice]->ObjectSize[NewObjectType]);
		stpti_printf(">> FREEed (%p)", NewObject_p);
	}

	return (Error);
}

/**
   @brief  Manage infrastructure for Associating two Objects, and call thier respective Association functions.

   It is important to recognise that BOTH objects' Association functions will be called.

   @param  Object1Handle  The handle of the first object being associated.
   @param  Object1Handle  The handle of the second object being associated.

   @return                A standard st error type...
                          - ST_NO_ERROR
                          - ST_ERROR_BAD_PARAMETER
                          Or a variety of other errors from each object's association function
 */
static ST_ErrorCode_t stptiOBJMAN_AssociateObjectsInternal(FullHandle_t Object1Handle, FullHandle_t Object2Handle)
{
	ST_ErrorCode_t Error;
	int IndexObject2InObject1AssocList = -1, IndexObject1InObject2AssocList = -1;

	ObjectType_t Object1Type = stptiOBJMAN_GetObjectType(Object1Handle);
	ObjectType_t Object2Type = stptiOBJMAN_GetObjectType(Object2Handle);
	Object_t *Object1_p, *Object2_p;
	unsigned int Object1Class, Object2Class;

	/* As both objects' association functions are called we take measures to make sure that they are
	   always called in the same order.  This is determined by the ObjectType, the Object type with
	   the lowest number is done first. */
	if (Object1Type > Object2Type) {
		/* Swap the object around */
		FullHandle_t tmp = Object1Handle;
		Object1Handle = Object2Handle;
		Object2Handle = tmp;
		Object1Type = stptiOBJMAN_GetObjectType(Object1Handle);
		Object2Type = stptiOBJMAN_GetObjectType(Object2Handle);
	}
	Object1_p = stptiOBJMAN_HandleToObjectPointer(Object1Handle, Object1Type);
	Object2_p = stptiOBJMAN_HandleToObjectPointer(Object2Handle, Object2Type);
	Object1Class = Object1Handle.member.pDevice;
	Object2Class = Object2Handle.member.pDevice;

	/* Objects can only be associated ONCE.  Check to see if we are already associated. */
	Error =
	    stptiOBJMAN_SearchListForItem(&Object1_p->AssociatedObjects, Object2_p, &IndexObject2InObject1AssocList);
	if ((Error == ST_NO_ERROR) && (IndexObject2InObject1AssocList != -1)) {
		stpti_printf("Objects already Associated. [%08x %08x]", Object1Handle.word, Object2Handle.word);
		Error = STPTI_ERROR_OBJECT_ALREADY_ASSOCIATED;
	}

	if (Error == ST_NO_ERROR) {
		/* This could be considered overkill as we are now checking the two object lists are sync
		 * which would be an object manager fault rather than a HAL fault. */
		Error =
		    stptiOBJMAN_SearchListForItem(&Object2_p->AssociatedObjects, Object1_p,
						  &IndexObject1InObject2AssocList);
		if ((Error == ST_NO_ERROR) && (IndexObject1InObject2AssocList != -1)) {
			stpti_printf("Objects already Associated. [%08x %08x] - WARNING OBJECTS LISTS INCONSISTENT",
				     Object1Handle.word, Object2Handle.word);
			Error = STPTI_ERROR_OBJECT_ALREADY_ASSOCIATED;
		}
	}

	/* Add to association lists */
	if (Error == ST_NO_ERROR) {
		Error =
		    stptiOBJMAN_AddToList(&Object1_p->AssociatedObjects, Object2_p, FALSE,
					  &IndexObject2InObject1AssocList);
		if (Error == ST_NO_ERROR) {
			Error =
			    stptiOBJMAN_AddToList(&Object2_p->AssociatedObjects, Object1_p, FALSE,
						  &IndexObject1InObject2AssocList);
			if (Error != ST_NO_ERROR) {
				stptiOBJMAN_RemoveFromList(&Object1_p->AssociatedObjects,
							   IndexObject2InObject1AssocList);
			}
		}
	}

	if (Error == ST_NO_ERROR) {
		/* First half of the association Object1 -> Object2) */
		Error =
		    (*STPTI_Driver.ObjMan[Object1Class]->ObjFunPtr[Object1Type].Associator) (Object1Handle,
											     Object2Handle);
		if (Error == ST_NO_ERROR) {
			/* Second half of the association Object2 -> Object1) */
			Error =
			    (*STPTI_Driver.ObjMan[Object2Class]->ObjFunPtr[Object2Type].Associator) (Object2Handle,
												     Object1Handle);
			if (Error != ST_NO_ERROR) {
				/* Awkward situation as the second half of the association failed */
				/* So need need to undo the first half (disassociate) */

				/* Purposefully ignore the error from this function - to preserve the original error */
				(*STPTI_Driver.ObjMan[Object1Class]->ObjFunPtr[Object1Type].
				 Disassociator) (Object1Handle, Object2Handle);
			}
		}

		if (Error != ST_NO_ERROR) {
			stptiOBJMAN_RemoveFromList(&Object1_p->AssociatedObjects, IndexObject2InObject1AssocList);
			stptiOBJMAN_RemoveFromList(&Object2_p->AssociatedObjects, IndexObject1InObject2AssocList);
		}
	}

	return (Error);
}

/**
   @brief  Manage infrastructure for Disassociating two Objects, and call thier respective Disassociation functions.

   It is important to recognise that BOTH objects' Disassociation functions will be called.

   @param  Object1Handle  The handle of the first object being disassociated.
   @param  Object1Handle  The handle of the second object being disassociated.

   @return                A standard st error type...
                          - ST_NO_ERROR
                          - ST_ERROR_BAD_PARAMETER
                          Or a variety of other errors from each object's disassociation function
 */
static ST_ErrorCode_t stptiOBJMAN_DisassociateObjectsInternal(FullHandle_t Object1Handle, FullHandle_t Object2Handle)
{
	ST_ErrorCode_t Error = ST_NO_ERROR;
	int index, Object2IndexInObject1List = -1, Object1IndexInObject2List = -1;

	ObjectType_t Object1Type = stptiOBJMAN_GetObjectType(Object1Handle);

	/* Object2 can be null to make this Object1 disassociate all its objects */
	if (stptiOBJMAN_IsNullHandle(Object2Handle)) {
		/* Disassociate all objects associated to this object */
		Object_t *Object2_p;
		Object_t *Object1_p = stptiOBJMAN_HandleToObjectPointer(Object1Handle, Object1Type);
		stptiOBJMAN_FirstInList(&Object1_p->AssociatedObjects, (void *)&Object2_p, &index);
		while (index >= 0) {
			if (stptiOBJMAN_IsNullHandle(Object2_p->Handle)) {
				STPTI_ASSERT(FALSE, "RECURSION LOOP");
				Error = ST_ERROR_BAD_PARAMETER;
				break;
			}

			Error = stptiOBJMAN_DisassociateObjectsInternal(Object1Handle, Object2_p->Handle);	/* Note recursive */
			if (Error != ST_NO_ERROR) {
				break;
			}
			stptiOBJMAN_NextInList(&Object1_p->AssociatedObjects, (void *)&Object2_p, &index);
		}
	} else {
		/* Just disassociate objects specified */
		ObjectType_t Object2Type = stptiOBJMAN_GetObjectType(Object2Handle);

		Object_t *Object1_p, *Object2_p;
		unsigned int Object1Class, Object2Class;

		/* As both objects' disassociation functions are called we take measures to make sure that they are
		   always called in the same order.  This is determined by the ObjectType, the Object type with
		   the highest number is done first. (i.e. the reverse of association) */
		if (Object2Type > Object1Type) {
			/* Swap the object around */
			FullHandle_t tmp = Object1Handle;
			Object1Handle = Object2Handle;
			Object2Handle = tmp;
			Object1Type = stptiOBJMAN_GetObjectType(Object1Handle);
			Object2Type = stptiOBJMAN_GetObjectType(Object2Handle);
		}

		Object1Class = Object1Handle.member.pDevice;
		Object2Class = Object2Handle.member.pDevice;
		Object1_p = stptiOBJMAN_HandleToObjectPointer(Object1Handle, Object1Type);
		Object2_p = stptiOBJMAN_HandleToObjectPointer(Object2Handle, Object2Type);

		if ((Object1_p == NULL) || (Object2_p == NULL)) {
			Error = ST_ERROR_BAD_PARAMETER;
		} else {
			/* Check to see if these objects are actually associated */
			stptiOBJMAN_SearchListForItem(&Object1_p->AssociatedObjects, Object2_p,
						      &Object2IndexInObject1List);
			if (Object2IndexInObject1List == -1) {
				stpti_printf("Trying to disassociate objects that are not associated. [%08x %08x]",
					     Object1Handle.word, Object2Handle.word);
				Error = STPTI_ERROR_OBJECTS_NOT_ASSOCIATED;
			} else {
				stptiOBJMAN_SearchListForItem(&Object2_p->AssociatedObjects, Object1_p,
							      &Object1IndexInObject2List);
				if (Object1IndexInObject2List == -1) {
					stpti_printf("Trying to disassociate objects that are not associated. [%08x %08x]",
					     Object1Handle.word, Object2Handle.word);
					Error = STPTI_ERROR_OBJECTS_NOT_ASSOCIATED;
				}
			}
		}

		if (Error == ST_NO_ERROR) {
			/* Perform disassociation */
			Error =
			    (*STPTI_Driver.ObjMan[Object1Class]->ObjFunPtr[Object1Type].Disassociator) (Object1Handle,
													Object2Handle);
			if (Error == ST_NO_ERROR) {
				Error =
				    (*STPTI_Driver.ObjMan[Object2Class]->ObjFunPtr[Object2Type].
				     Disassociator) (Object2Handle, Object1Handle);
				if (Error != ST_NO_ERROR) {
					/* This is a potential headache, this object is now partically disassociated */
					/* Better reassociate it to keep things consistent - but this could cause a problem for objects */
					/* that "do things" on association. */
					Error = (*STPTI_Driver.ObjMan[Object1Class]->ObjFunPtr[Object1Type].Associator) (Object1Handle, Object2Handle);
				}
			}
		}

		/* Sort out lists */
		if (Error == ST_NO_ERROR) {
			stptiOBJMAN_RemoveFromList(&Object1_p->AssociatedObjects, Object2IndexInObject1List);
			stptiOBJMAN_RemoveFromList(&Object2_p->AssociatedObjects, Object1IndexInObject2List);
		}
	}
	return (Error);
}

/**
   @brief  Manage infrastructure for Deallocating (Destroying) an Object, and calling it Deallocation function.

   @param  ObjectHandle   The handle of the object being deallocated.
   @param  Force          If TRUE it will deallocate the object even if it has children / associations.
   @param  ReleaseHandle  If TRUE it will release the handle from STCOMMON (but only if IPRC).

   @return                A standard st error type...
                          - ST_NO_ERROR
                          - ST_ERROR_BAD_PARAMETER
                          - ST_ERROR_DEVICE_BUSY    (cannot deallocate as has children, Force=FALSE)
                          Or a variety of other errors from each object's disassociation function
 */
static ST_ErrorCode_t stptiOBJMAN_DeallocateObjectInternal(FullHandle_t ObjectHandle, BOOL CallDeallocator, BOOL Force,
							   BOOL ReleaseHandle)
{
	ST_ErrorCode_t Error = ST_NO_ERROR;
	int index;
	unsigned int pDevice;
	ObjectType_t ObjectType;
	Object_t *Object_p;
	BOOL Dealloc_Ok = TRUE;

	pDevice = ObjectHandle.member.pDevice;
	ObjectType = stptiOBJMAN_GetObjectType(ObjectHandle);
	Object_p = stptiOBJMAN_HandleToObjectPointer(ObjectHandle, ObjectType);

	if (Object_p == NULL) {
		Error = ST_ERROR_BAD_PARAMETER;
	} else {
		if (Force) {
			/* Need to disassociate any associated objects before we deallocate ourselves */
			stptiOBJMAN_DisassociateObjectsInternal(ObjectHandle, stptiOBJMAN_NullHandle);
			Error = ST_NO_ERROR;	/* deliberately ignoring any error */
		} else if (stptiOBJMAN_GetNumberOfItemsInList(&Object_p->AssociatedObjects) > 0) {
			Error = ST_ERROR_DEVICE_BUSY;
		}
	}

	if (ST_NO_ERROR == Error) {
		/* For vDevice and Session objects we need to deallocate any child objects */
		switch (ObjectType) {
		case OBJECT_TYPE_PDEVICE:
			/* Step through each vDevice */
			{
				Object_t *vDeviceObject;
				Object_t *pDeviceObject_p = Object_p;

				stptiOBJMAN_FirstInList(&pDeviceObject_p->ChildObjects, (void *)&vDeviceObject, &index);
				while (index >= 0) {
					if (!Force) {
						/* If there are objects, and we are not forcing deallocation return error */
						Error = ST_ERROR_DEVICE_BUSY;
						Dealloc_Ok = FALSE;
						break;
					}
					Error = stptiOBJMAN_DeallocateObjectInternal(vDeviceObject->Handle, CallDeallocator, Force, TRUE);	/* Note this is recursive */
					if (Error != ST_NO_ERROR) {
						Dealloc_Ok = FALSE;
						/* We don't break here, as we keep trying to deallocate other objects */
					}
					stptiOBJMAN_NextInList(&pDeviceObject_p->ChildObjects, (void *)&vDeviceObject,
							       &index);
				}
			}
			break;

		case OBJECT_TYPE_VDEVICE:
			/* Step through each Session */
			{
				Object_t *SessionObject;
				Object_t *vDeviceObject_p = Object_p;

				stptiOBJMAN_FirstInList(&vDeviceObject_p->ChildObjects, (void *)&SessionObject, &index);
				while (index >= 0) {
					if (!Force) {
						/* If there are objects, and we are not forcing deallocation return error */
						Error = ST_ERROR_DEVICE_BUSY;
						Dealloc_Ok = FALSE;
						break;
					}
					Error = stptiOBJMAN_DeallocateObjectInternal(SessionObject->Handle, CallDeallocator, Force, TRUE);	/* Note this is recursive */
					if (Error != ST_NO_ERROR) {
						Dealloc_Ok = FALSE;
						/* We don't break here, as we keep trying to deallocate other objects */
					}
					stptiOBJMAN_NextInList(&vDeviceObject_p->ChildObjects, (void *)&SessionObject,
							       &index);
				}
			}
			break;

		case OBJECT_TYPE_SESSION:
			/* Step through each Object */
			{
				Object_t *ChildObject;
				Object_t *SessionObject_p = Object_p;

				stptiOBJMAN_FirstInList(&SessionObject_p->ChildObjects, (void *)&ChildObject, &index);
				while (index >= 0) {
					if (!Force) {
						/* If there are objects, and we are not forcing deallocation return error */
						Error = ST_ERROR_DEVICE_BUSY;
						Dealloc_Ok = FALSE;
						break;
					}
					Error = stptiOBJMAN_DeallocateObjectInternal(ChildObject->Handle, CallDeallocator, Force, TRUE);	/* Note this is recursive */
					if (Error != ST_NO_ERROR) {
						Dealloc_Ok = FALSE;
						/* We don't break here, as we keep trying to deallocate other objects */
					}
					stptiOBJMAN_NextInList(&SessionObject_p->ChildObjects, (void *)&ChildObject,
							       &index);
				}
			}
			break;

		default:
			break;
		}
	} else {
		Dealloc_Ok = FALSE;
	}

	/* We don't deallocate Sessions or vDevices when objects are still allocated */
	if (Dealloc_Ok) {
		/* Deallocate Object calling the object's deallocator before freeing the memory */
		if (CallDeallocator) {
			Error = (*STPTI_Driver.ObjMan[pDevice]->ObjFunPtr[ObjectType].Deallocator) (ObjectHandle);
		}

		if (Error == ST_NO_ERROR) {
			/* Update the object lists */
			switch (ObjectType) {
			case OBJECT_TYPE_PDEVICE:
				{
					/* Deallocate the pDevices ChildList */
					Object_t *pDeviceObject_p =
					    stptiOBJMAN_HandleToObjectPointer(ObjectHandle, OBJECT_TYPE_PDEVICE);
					stptiOBJMAN_DeallocateList(&pDeviceObject_p->ChildObjects);

					/* Its a special object so need removing from a special list */
					STPTI_Driver.ObjMan[pDevice]->pDevice_p = NULL;
				}
				break;

			case OBJECT_TYPE_VDEVICE:
				{
					/* Deallocate the vDevices ChildList */
					Object_t *pDeviceObject_p =
					    stptiOBJMAN_HandleToObjectPointer(ObjectHandle, OBJECT_TYPE_PDEVICE);
					Object_t *vDeviceObject_p =
					    stptiOBJMAN_HandleToObjectPointer(ObjectHandle, OBJECT_TYPE_VDEVICE);
					stptiOBJMAN_RemoveFromList(&pDeviceObject_p->ChildObjects,
								   ObjectHandle.member.vDevice);
					stptiOBJMAN_DeallocateList(&vDeviceObject_p->ChildObjects);
				}
				break;

			case OBJECT_TYPE_SESSION:
				{
					Object_t *vDeviceObject_p =
					    stptiOBJMAN_HandleToObjectPointer(ObjectHandle, OBJECT_TYPE_VDEVICE);
					Object_t *SessionObject_p =
					    stptiOBJMAN_HandleToObjectPointer(ObjectHandle, OBJECT_TYPE_SESSION);
					stptiOBJMAN_RemoveFromList(&vDeviceObject_p->ChildObjects,
								   ObjectHandle.member.Session);
					stptiOBJMAN_DeallocateList(&SessionObject_p->ChildObjects);
				}
				break;

			default:
				{
					Object_t *SessionObject_p =
					    stptiOBJMAN_HandleToObjectPointer(ObjectHandle, OBJECT_TYPE_SESSION);
					stptiOBJMAN_RemoveFromList(&SessionObject_p->ChildObjects,
								   ObjectHandle.member.Object);
					stptiOBJMAN_DeallocateList(&Object_p->AssociatedObjects);
				}
				break;
			}
			if (Object_p->AuxiliaryData_p != NULL) {
				/* Deallocate the AuxiliaryData if necessary */
				stptiOBJMAN_DeallocateMemory(Object_p->AuxiliaryData_p, Object_p->AuxiliaryDataSize);
			}

			if (ReleaseHandle) {
#if defined( STPTI_IPRC_PRESENT )
				Error = STCOMMON_HandleFree(Object_p->APIHandle);
#endif
			}

			stptiOBJMAN_DeallocateMemory((void *)Object_p,
						     STPTI_Driver.ObjMan[pDevice]->ObjectSize[ObjectType]);
			stpti_printf(">> FREEed (%p)", Object_p);
		}
	} else {
		stpti_printf("Failure to deallocate object [%08x] type %d", ObjectHandle.word, ObjectType);
	}

	return (Error);
}

/**
   @brief  Returns the associated Objects of a given type.

   @param  ObjectHandle       		The handle of the object being examined
   @param  AssocObjectHandles 		An arry to hold the returned handles
   @param  SizeOfAssocObjectHandles 	The size of the array
   @param  ObjectTypeToReturn 		The type of the objects to return, or OBJECT_TYPE_ANY

   @return                    The number of objects found.

 */
int stptiOBJMAN_ReturnAssociatedObjects(FullHandle_t ObjectHandle, FullHandle_t *AssocObjectHandles,
					int SizeOfAssocObjectHandles, ObjectType_t ObjectTypeToReturn)
{
	ObjectType_t ObjectType = stptiOBJMAN_GetObjectType(ObjectHandle);
	Object_t *Object_p = stptiOBJMAN_HandleToObjectPointer(ObjectHandle, ObjectType);

	int count = 0, index;

	if (Object_p != NULL) {
		Object_t *AssociatedObject;
		stptiOBJMAN_FirstInList(&Object_p->AssociatedObjects, (void *)&AssociatedObject, &index);
		while (index >= 0) {
			if ((ObjectTypeToReturn == OBJECT_TYPE_ANY)
			    || (AssociatedObject->Handle.member.ObjectType == ObjectTypeToReturn)) {
				if (count < SizeOfAssocObjectHandles) {
					AssocObjectHandles[count] = stptiOBJMAN_ObjectPointerToHandle(AssociatedObject);
				}
				count++;
			}
			stptiOBJMAN_NextInList(&Object_p->AssociatedObjects, (void *)&AssociatedObject, &index);
		}
	}

	return (count);
}

/**
   @brief  Returns the associated Objects of a given type.

   @param  ObjectHandle1      		The handle of the object being examined
   @param  ObjectHandle2      		The handle of the object being examined

   @return                    		TRUE if they are associated

 */
BOOL stptiOBJMAN_IsAssociated(FullHandle_t ObjectHandle1, FullHandle_t ObjectHandle2)
{
	BOOL IsAssociated = FALSE;
	int index;

	ObjectType_t ObjectType = stptiOBJMAN_GetObjectType(ObjectHandle1);
	Object_t *Object_p = stptiOBJMAN_HandleToObjectPointer(ObjectHandle1, ObjectType);

	ObjectType_t Object2Type = stptiOBJMAN_GetObjectType(ObjectHandle2);
	Object_t *Object2Find_p = stptiOBJMAN_HandleToObjectPointer(ObjectHandle2, Object2Type);

	if (Object_p != NULL) {
		Object_t *AssociatedObject;
		stptiOBJMAN_FirstInList(&Object_p->AssociatedObjects, (void *)&AssociatedObject, &index);
		while (index >= 0) {
			if (AssociatedObject == Object2Find_p) {
				IsAssociated = TRUE;
				break;
			}
			stptiOBJMAN_NextInList(&Object_p->AssociatedObjects, (void *)&AssociatedObject, &index);
		}
	}
	return (IsAssociated);
}

/**
   @brief  Returns the child Objects of a given type.

   @param  ObjectHandle       		The handle of the object being examined
   @param  ChildObjectHandles 		An arry to hold the returned handles
   @param  SizeOfChildObjectHandles 	The size of the array
   @param  ObjectTypeToReturn 		The type of the objects to return, or OBJECT_TYPE_ANY

   @return                    		The number of objects found.

 */
int stptiOBJMAN_ReturnChildObjects(FullHandle_t ObjectHandle, FullHandle_t *ChildObjectHandles,
				   int SizeOfChildObjectHandles, ObjectType_t ObjectTypeToReturn)
{
	ObjectType_t ObjectType = stptiOBJMAN_GetObjectType(ObjectHandle);
	Object_t *Object_p = stptiOBJMAN_HandleToObjectPointer(ObjectHandle, ObjectType);

	int count = 0, index;

	if (Object_p != NULL) {
		Object_t *ChildObject;
		stptiOBJMAN_FirstInList(&Object_p->ChildObjects, (void *)&ChildObject, &index);
		while (index >= 0) {
			if ((ObjectTypeToReturn == OBJECT_TYPE_ANY)
			    || (ChildObject->Handle.member.ObjectType == ObjectTypeToReturn)) {
				if (count < SizeOfChildObjectHandles) {
					ChildObjectHandles[count] = stptiOBJMAN_ObjectPointerToHandle(ChildObject);
				}
				count++;
			}
			stptiOBJMAN_NextInList(&Object_p->ChildObjects, (void *)&ChildObject, &index);
		}
	}

	return (count);
}

/**
   @brief Allocates memory for Auxillary Data (API metadata information bout the object)

   This function allocates memory to be used by software OUTSIDE of the stptiHAL (for example the
   API layer).  This is a way of associating information with an object.  The memory is
   automatically recovered when the object is deallocated.

   @param  ObjectHandle       The handle of the object to have Auxillary Data added to
   @param  size               The amount of memory to allocate for the Auxillary Data (can be zero)

   @return                    A pointer to the memory, or NULL if it can't be allocated (either
                              because it has been allocated already, or there is no memory left)
 */
void *stptiOBJMAN_AllocateObjectAuxiliaryData(FullHandle_t ObjectHandle, size_t size)
{
	void *p;
	stptiSupport_AccessLockState_t LocalLockState = stptiSupport_UNLOCKED;

	stptiOBJMAN_WriteLock(ObjectHandle, &LocalLockState);
	{
		p = stptiOBJMAN_AllocateObjectAuxiliaryDataInternal(ObjectHandle, size);
	}
	stptiOBJMAN_Unlock(ObjectHandle, &LocalLockState);
	return (p);
}

static void *stptiOBJMAN_AllocateObjectAuxiliaryDataInternal(FullHandle_t ObjectHandle, size_t size)
{
	void *ptr = NULL;
	Object_t *Object_p = stptiOBJMAN_HandleToObjectPointer(ObjectHandle, stptiOBJMAN_GetObjectType(ObjectHandle));

	/* Allocate memory only if no memory already allocated */
	if (Object_p->AuxiliaryData_p == NULL) {
		if (size > 0) {
			ptr = stptiOBJMAN_AllocateMemory(size);
			if (ptr != NULL) {
				memset(ptr, 0, size);
			} else {
				size = 0;
			}
		}
		Object_p->AuxiliaryData_p = ptr;
		Object_p->AuxiliaryDataSize = size;
	} else {
		STPTI_PRINTF_ERROR("AuxillaryData Memory already allocated for this handle %08x", ObjectHandle.word);
	}

	return (ptr);
}

/**
   @brief Deallocates the memory for Auxillary Data (API metadata information bout the object)

   This recovers memory previously allocated by stptiOBJMAN_AllocateObjectAuxiliaryData().  It is
   not necessary to call this function, as this memory is automatically recovered when the object is
   deallocated.

   @param  ObjectHandle       The handle of the object

   @return                    nothing

 */
void stptiOBJMAN_DeallocateObjectAuxiliaryData(FullHandle_t ObjectHandle)
{
	stptiSupport_AccessLockState_t LocalLockState = stptiSupport_UNLOCKED;

	stptiOBJMAN_WriteLock(ObjectHandle, &LocalLockState);
	{
		Object_t *Object_p =
		    stptiOBJMAN_HandleToObjectPointer(ObjectHandle, stptiOBJMAN_GetObjectType(ObjectHandle));

		if (Object_p->AuxiliaryData_p != NULL) {
			stptiOBJMAN_DeallocateMemory(Object_p->AuxiliaryData_p, Object_p->AuxiliaryDataSize);
			Object_p->AuxiliaryData_p = NULL;
			Object_p->AuxiliaryDataSize = 0;
		}
	}
	stptiOBJMAN_Unlock(ObjectHandle, &LocalLockState);
}
