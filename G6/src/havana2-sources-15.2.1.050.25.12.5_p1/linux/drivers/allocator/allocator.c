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

#define MAJOR_NUMBER    245

#include <linux/module.h>
#include <ics.h>

#include "osdev_mem.h"
#include "osdev_device.h"

#include "allocatorio.h"

MODULE_DESCRIPTION("memory allocator char driver");
MODULE_AUTHOR("STMicroelectronics");
MODULE_LICENSE("GPL");

typedef struct AllocatorContext_s {
	unsigned int   Size;
	unsigned char  PartitionName[ALLOCATOR_MAX_PARTITION_NAME_SIZE];
	unsigned char *Memory;
	unsigned char *CachedAddress;
	unsigned char *PhysicalAddress;
	unsigned int   MemoryAccess;
	ICS_REGION     CachedRegion;
} AllocatorContext_t;


// ////////////////////////////////////////////////////////////////////////////////
// Return a mask to defined the list of core that can run transformer
// secure core is excluded from the list
unsigned long filter_cpu_mask(void)
{
	unsigned long mask = ics_cpu_mask();
	int  gpidx = ics_cpu_lookup("gp0");
	if (gpidx != -1) { mask = mask & ~(1 << gpidx); }
	return mask;
}

// ////////////////////////////////////////////////////////////////////////////////
static OSDEV_OpenEntrypoint(AllocatorOpen)
{
	AllocatorContext_t *AllocatorContext;

	OSDEV_OpenEntry();
	AllocatorContext    = (AllocatorContext_t *)OSDEV_Malloc(sizeof(AllocatorContext_t));
	OSDEV_PrivateData   = (void *)AllocatorContext;

	if (AllocatorContext == NULL) {
		pr_err("Error: %s failed to allocate memory for open context\n", __func__);
		OSDEV_OpenExit(OSDEV_Error);
	}

	memset(AllocatorContext, 0, sizeof(AllocatorContext_t));

	AllocatorContext->Size      = 0;
	AllocatorContext->Memory    = NULL;
	AllocatorContext->MemoryAccess = MEMORY_DEFAULT_ACCESS;
	OSDEV_OpenExit(OSDEV_NoError);
}

//    The ioctl function to handle ics map - this is required when we enter CPS
static void AllocatorIoctlRemoveMap(AllocatorContext_t    *AllocatorContext)
{
	ICS_ERROR IcsErr;

	IcsErr = ICS_region_remove(AllocatorContext->CachedRegion, 0);
	if (IcsErr != ICS_SUCCESS) {
		pr_err("Error: %s failed to remove Cached ICS region\n", __func__);
	}
}

static OSDEV_CloseEntrypoint(AllocatorClose)
{
	AllocatorContext_t *AllocatorContext;

	OSDEV_CloseEntry();
	AllocatorContext    = (AllocatorContext_t *)OSDEV_PrivateData;

	if (AllocatorContext->Memory != NULL) {
		if (AllocatorContext->MemoryAccess & MEMORY_ICS_ACCESS) {
			ICS_ERROR IcsErr = ICS_region_remove(AllocatorContext->CachedRegion, 0);
			if (IcsErr != ICS_SUCCESS) {
				pr_err("Error: %s failed to remove Cached ICS region\n", __func__);
			}
		}

		if (AllocatorContext->MemoryAccess & (MEMORY_VMA_CACHED_ACCESS | MEMORY_VMA_UNCACHED_ACCESS)) {
			if (AllocatorContext->CachedAddress == 0) {
				pr_err("Error: %s cannot unmap null buffer\n", __func__);
			} else {
				iounmap((void *)AllocatorContext->CachedAddress);
			}
		}

		OSDEV_FreePartitioned(AllocatorContext->PartitionName, AllocatorContext->PhysicalAddress);
	}

	OSDEV_Free(AllocatorContext);

	OSDEV_CloseExit(OSDEV_NoError);
}

//    The mmap function to make the Allocator buffers available to the user for writing
static OSDEV_MmapEntrypoint(AllocatorMmap)
{
	AllocatorContext_t      *AllocatorContext;
	OSDEV_Status_t           MappingStatus;

	OSDEV_MmapEntry();
	AllocatorContext = (AllocatorContext_t *)OSDEV_PrivateData;

	OSDEV_MmapPerformMap(AllocatorContext->Memory, MappingStatus);

	OSDEV_MmapExit(MappingStatus);
}

#define ALIGNUP(x,a)    (((unsigned long)(x) + ((a)-1UL)) & ~((a)-1UL)) /* 'a' must be a power of 2 */

//    The ioctl function to handle ics map - this is required when we exit from CPS
static OSDEV_Status_t AllocatorIoctlCreateMap(AllocatorContext_t    *AllocatorContext)
{
	ICS_ERROR IcsErr;

	IcsErr = ICS_region_add(AllocatorContext->CachedAddress, (ICS_OFFSET) AllocatorContext->PhysicalAddress,
	                        AllocatorContext->Size,
	                        ICS_CACHED, filter_cpu_mask(), &AllocatorContext->CachedRegion);
	if (IcsErr != ICS_SUCCESS) {
		pr_err("Error: %s failed to allocate Cached ICS region\n", __func__);
		return OSDEV_Error;
	}

	return OSDEV_NoError;
}


//    The ioctl function to handle allocating the memory
static OSDEV_Status_t AllocatorIoctlAllocateData(AllocatorContext_t    *AllocatorContext,
                                                 unsigned int           ParameterAddress)
{
	allocator_ioctl_allocate_t params;
	ICS_ERROR IcsErr;

	OSDEV_CopyToDeviceSpace(&params, ParameterAddress, sizeof(allocator_ioctl_allocate_t));


	AllocatorContext->MemoryAccess = params.MemoryAccessType;
	if (AllocatorContext->MemoryAccess & MEMORY_ICS_ACCESS) {
		AllocatorContext->Size = ALIGNUP(params.RequiredSize, ICS_PAGE_SIZE);
	} else {
		AllocatorContext->Size = params.RequiredSize;
	}
	strncpy(AllocatorContext->PartitionName, params.PartitionName, sizeof(AllocatorContext->PartitionName));
	AllocatorContext->PartitionName[sizeof(AllocatorContext->PartitionName) - 1] = '\0';

	AllocatorContext->Memory = OSDEV_MallocPartitioned(AllocatorContext->PartitionName,
	                                                   AllocatorContext->Size);

	if (AllocatorContext->Memory == NULL) {
		pr_err("Error: %s failed to allocate memory from %s (%d)\n", __func__, AllocatorContext->PartitionName,
		       AllocatorContext->Size);
		return OSDEV_Error;
	}


	// The memory supplied by BPA2 is physical, get the necessary mappings
	// Cached memory
	AllocatorContext->CachedAddress = 0;
	AllocatorContext->PhysicalAddress = AllocatorContext->Memory ;

	if (AllocatorContext->MemoryAccess & MEMORY_VMA_CACHED_ACCESS) {
		AllocatorContext->CachedAddress = (unsigned char *) ioremap_cache((unsigned int)AllocatorContext->Memory,
		                                                                  AllocatorContext->Size);

		if (AllocatorContext->CachedAddress == 0) {
			pr_err("Error: %s failed to remap cached buffer ,physical address %d ,size %d \n", __func__,
			       (unsigned int)AllocatorContext->Memory, AllocatorContext->Size);
			return OSDEV_Error;

		}
	} else if (AllocatorContext->MemoryAccess & MEMORY_VMA_UNCACHED_ACCESS) {
		AllocatorContext->CachedAddress = (unsigned char *) ioremap_wc((unsigned int)AllocatorContext->Memory,
		                                                               AllocatorContext->Size);

		if (AllocatorContext->CachedAddress == 0) {
			pr_err("Error: %s failed to remap uncached buffer ,physical address %d ,size %d \n", __func__,
			       (unsigned int)AllocatorContext->Memory, AllocatorContext->Size);
			return OSDEV_Error;

		}
	}


	if (AllocatorContext->MemoryAccess & MEMORY_ICS_ACCESS) {

		IcsErr = ICS_region_add(AllocatorContext->CachedAddress, (ICS_OFFSET) AllocatorContext->PhysicalAddress,
		                        AllocatorContext->Size,
		                        ICS_CACHED, filter_cpu_mask(), &AllocatorContext->CachedRegion);
		if (IcsErr != ICS_SUCCESS) {
			pr_err("Error: %s failed to allocate Cached ICS region, physical address %d ,size %d\n", __func__,
			       (unsigned int)AllocatorContext->Memory, AllocatorContext->Size);
			if (AllocatorContext->MemoryAccess & (MEMORY_VMA_CACHED_ACCESS | MEMORY_VMA_UNCACHED_ACCESS)) {
				iounmap((void *)AllocatorContext->CachedAddress);
			}
			return OSDEV_Error;
		}
	}

	// Copy the data into the parameters and pass back
	params.CachedAddress   = AllocatorContext->CachedAddress;
	params.PhysicalAddress = AllocatorContext->PhysicalAddress;

	OSDEV_CopyToUserSpace(ParameterAddress, &params, sizeof(allocator_ioctl_allocate_t));

	return OSDEV_NoError;
}

static OSDEV_IoctlEntrypoint(AllocatorIoctl)
{
	OSDEV_Status_t      Status = OSDEV_NoError;
	AllocatorContext_t  *AllocatorContext;

	OSDEV_IoctlEntry();
	AllocatorContext    = (AllocatorContext_t *)OSDEV_PrivateData;

	switch (OSDEV_IoctlCode) {
	case    ALLOCATOR_IOCTL_ALLOCATE_DATA:
		Status = AllocatorIoctlAllocateData(AllocatorContext, OSDEV_ParameterAddress);
		break;
	case    ALLOCATOR_IOCTL_REMOVE_MAP:
		AllocatorIoctlRemoveMap(AllocatorContext);
		break;
	case    ALLOCATOR_IOCTL_CREATE_MAP:
		Status = AllocatorIoctlCreateMap(AllocatorContext);
		break;
	default:
		pr_err("Error: %s Invalid ioctl %08x\n", __func__, OSDEV_IoctlCode);
		Status = OSDEV_Error;
	}

	OSDEV_IoctlExit(Status);
}

// /////////////////////////////////////////////////////////////////////////////////

static OSDEV_Descriptor_t AllocatorDeviceDescriptor = {
	.Name = "AllocatorModule",
	.MajorNumber = MAJOR_NUMBER,
	.OpenFn  = AllocatorOpen,
	.CloseFn = AllocatorClose,
	.IoctlFn = AllocatorIoctl,
	.MmapFn  = AllocatorMmap
};

static int __init AllocatorLoadModule(void)
{
	OSDEV_Status_t  Status;

	Status = OSDEV_RegisterDevice(&AllocatorDeviceDescriptor);
	if (Status != OSDEV_NoError) {
		pr_err("Error: %s failed\n", __func__);
		return -ENODEV;
	}

	Status = OSDEV_LinkDevice(ALLOCATOR_DEVICE, MAJOR_NUMBER, 0);
	if (Status != OSDEV_NoError) {
		OSDEV_DeRegisterDevice(&AllocatorDeviceDescriptor);
		pr_err("Error: %s failed\n", __func__);
		return -ENODEV;
	}

	OSDEV_Dump_MemCheckCounters(__func__);
	pr_info("%s done ok\n", __func__);

	return 0;
}
module_init(AllocatorLoadModule);

static void AllocatorUnloadModule(void)
{
	OSDEV_UnLinkDevice(ALLOCATOR_DEVICE);
	OSDEV_DeRegisterDevice(&AllocatorDeviceDescriptor);

	OSDEV_Dump_MemCheckCounters(__func__);
	pr_info("%s done\n", __func__);
}
module_exit(AllocatorUnloadModule);
