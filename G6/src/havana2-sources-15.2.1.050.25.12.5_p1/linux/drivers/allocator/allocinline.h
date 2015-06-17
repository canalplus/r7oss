/************************************************************************
Copyright (C) 2003-2014 STMicroelectronics. All Rights Reserved.

This file is part of the Streaming Engine.

Streaming Engine is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

Streaming Engine is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with Streaming Engine; see the file COPYING.  If not, write to the Free
Software Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307,
USA.

The Streaming Engine Library may alternatively be licensed under a
proprietary license from ST.
************************************************************************/

#ifndef H_ALLOCINLINE
#define H_ALLOCINLINE

// this file is meant to be included from player cpp code
// e.g. not to be included from linux native modules
#ifndef __cplusplus
#error "not included from player cpp code"
#endif

#include "osinline.h"
#include "osdev_user.h"

#include "allocatorio.h"

typedef enum {
	allocator_ok                = 0,
	allocator_error
} allocator_status_t;

struct allocator_device_s {
	OSDEV_DeviceIdentifier_t UnderlyingDevice;
	unsigned int             BufferSize;
	unsigned char           *CachedAddress;
	unsigned char           *PhysicalAddress;
	unsigned int             AccessMode;
};
typedef struct allocator_device_s *allocator_device_t;

#define DEVICE_NAME                     "/dev/allocator"

#define BUF_ALIGN_UP(addr, align) (((uint32_t)(addr) + (align)) & (~align))

// ------------------------------------------------------------------------------------------------------------
//   The open function

static inline allocator_status_t   AllocatorOpenEx(
        allocator_device_t *Device,
        unsigned int        Size,
        unsigned int        MemoryAccessType,
        const char         *PartitionName)
{
	OSDEV_Status_t                  Status;
	allocator_ioctl_allocate_t      Parameters;

	if (strlen(PartitionName) > (ALLOCATOR_MAX_PARTITION_NAME_SIZE - 1)) {
		SE_ERROR("partition name too large ( %d > %d)\n", strlen(PartitionName),
		         (ALLOCATOR_MAX_PARTITION_NAME_SIZE - 1));
		*Device = NULL;
		return allocator_error;
	}

	*Device = (allocator_device_t)OS_Malloc(sizeof(struct allocator_device_s));
	if (*Device == NULL) {
		SE_ERROR("Failed to allocate memory for device context\n");
		return allocator_error;
	}

	memset(*Device, 0, sizeof(struct allocator_device_s));

	Status = OSDEV_Open(DEVICE_NAME, &((*Device)->UnderlyingDevice));
	if (Status != OSDEV_NoError) {
		SE_ERROR("Failed to open\n");
		OS_Free(*Device);
		*Device = NULL;
		return allocator_error;
	}

	Parameters.RequiredSize = Size;
	strncpy(Parameters.PartitionName, PartitionName, sizeof(Parameters.PartitionName));
	Parameters.PartitionName[sizeof(Parameters.PartitionName) - 1] = '\0';
	Parameters.MemoryAccessType = MemoryAccessType;
	Status = OSDEV_Ioctl((*Device)->UnderlyingDevice, ALLOCATOR_IOCTL_ALLOCATE_DATA,
	                     &Parameters, sizeof(allocator_ioctl_allocate_t));
	if (Status != OSDEV_NoError) {
		SE_ERROR("Failed to allocate memory (0x%08x (%u) from '%s')\n", Size, Size, PartitionName);
		OSDEV_Close((*Device)->UnderlyingDevice);
		OS_Free(*Device);
		*Device = NULL;
		return allocator_error;
	}

	(*Device)->BufferSize       = Size;
	(*Device)->CachedAddress    = Parameters.CachedAddress;
	(*Device)->PhysicalAddress  = Parameters.PhysicalAddress;

	return allocator_ok;
}

// ------------------------------------------------------------------------------------------------------------
//   The open function

static inline allocator_status_t   PartitionAllocatorOpen(
        allocator_device_t *Device,
        const char         *PartitionName,
        unsigned int        Size,
        int                 MemoryAccessType)
{
	return AllocatorOpenEx(Device, Size, MemoryAccessType, PartitionName);
}

// ------------------------------------------------------------------------------------------------------------
//   The close function

static inline void              AllocatorClose(allocator_device_t *DevicePtr)
{
	SE_ASSERT(DevicePtr != NULL);
	allocator_device_t Device = *DevicePtr;
	if (Device != NULL) {
		OSDEV_Close(Device->UnderlyingDevice);
		OS_Free(Device);
		*DevicePtr = NULL;
	}
}

// ------------------------------------------------------------------------------------------------------------
//   The User address function

static inline   unsigned char *AllocatorUserAddress(allocator_device_t Device)
{
	if (Device != NULL) {
		return Device->CachedAddress;
	} else {
		return NULL;
	}
}

// ------------------------------------------------------------------------------------------------------------
//   The Physical address function

static inline   unsigned char *AllocatorPhysicalAddress(allocator_device_t Device)
{
	if (Device != NULL) {
		return Device->PhysicalAddress;
	} else {
		return NULL;
	}
}

static inline allocator_status_t   AllocatorCreateMapEx(
        OSDEV_DeviceIdentifier_t        Handle)
{
	OSDEV_Status_t                  Status;
	Status = OSDEV_Ioctl(Handle, ALLOCATOR_IOCTL_CREATE_MAP, NULL, 0);
	if (Status != OSDEV_NoError) {
		SE_ERROR("Failed to create map\n");
		return allocator_error;
	}

	return allocator_ok;
}

static inline void   AllocatorRemoveMapEx(
        OSDEV_DeviceIdentifier_t        Handle)
{
	OSDEV_Ioctl(Handle, ALLOCATOR_IOCTL_REMOVE_MAP, NULL, 0);
}


#endif
