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

#ifndef H_OSDEV_DEVICE
#define H_OSDEV_DEVICE

#ifdef __cplusplus
#error "included from player cpp code"
#endif

#include <linux/platform_device.h>
#include <linux/ptrace.h>

#include "osdev_mem.h"
#include "osdev_status.h"

#ifndef bool
#define bool unsigned char
#endif

#define OSDEV_MAXIMUM_DEVICE_LINKS      32
#define OSDEV_MAXIMUM_MAJOR_NUMBER      256
#define OSDEV_MAX_OPENS                 96

// forward declaration
typedef struct OSDEV_Descriptor_s OSDEV_Descriptor_t;

// The device list
typedef struct OSDEV_DeviceLink_s {
	bool                 Valid;
	unsigned int         MajorNumber;
	unsigned int         MinorNumber;
	char                *Name;
} OSDEV_DeviceLink_t;

// The context for an open
typedef struct OSDEV_OpenContext_s {
	bool                  Open;
	unsigned int          MinorNumber;
	OSDEV_Descriptor_t   *Descriptor;
	void                 *PrivateData;
} OSDEV_OpenContext_t;

extern OSDEV_DeviceLink_t   OSDEV_DeviceList[OSDEV_MAXIMUM_DEVICE_LINKS];
extern OSDEV_Descriptor_t  *OSDEV_DeviceDescriptors[OSDEV_MAXIMUM_MAJOR_NUMBER];

// The function types and macro's used in defining the device descriptor type

typedef OSDEV_Status_t (*OSDEV_OpenFn_t)(OSDEV_OpenContext_t   *OSDEV_OpenContext);
#define OSDEV_OpenEntrypoint(fn) OSDEV_Status_t fn( OSDEV_OpenContext_t   *OSDEV_OpenContext )


typedef OSDEV_Status_t (*OSDEV_CloseFn_t)(OSDEV_OpenContext_t   *OSDEV_OpenContext);
#define OSDEV_CloseEntrypoint(fn) OSDEV_Status_t fn( OSDEV_OpenContext_t   *OSDEV_OpenContext )


typedef OSDEV_Status_t (*OSDEV_IoctlFn_t)(OSDEV_OpenContext_t   *OSDEV_OpenContext,   unsigned int   cmd,
                                          unsigned long   arg);
#define OSDEV_IoctlEntrypoint(fn) OSDEV_Status_t fn( OSDEV_OpenContext_t   *OSDEV_OpenContext,   unsigned int   cmd,   unsigned long   arg )


typedef OSDEV_Status_t (*OSDEV_MmapFn_t)(OSDEV_OpenContext_t   *OSDEV_OpenContext,   unsigned int  OSDEV_VMOffset,
                                         unsigned int   OSDEV_VMSize,
                                         unsigned char **OSDEV_VMAddress);
#define OSDEV_MmapEntrypoint(fn)   OSDEV_Status_t fn( OSDEV_OpenContext_t   *OSDEV_OpenContext,   unsigned int  OSDEV_VMOffset, unsigned int   OSDEV_VMSize,   unsigned char **OSDEV_VMAddress )

//

struct OSDEV_Descriptor_s {
	char                *Name;
	unsigned int         MajorNumber;

	OSDEV_OpenFn_t       OpenFn;
	OSDEV_CloseFn_t      CloseFn;
	OSDEV_IoctlFn_t      IoctlFn;
	OSDEV_MmapFn_t       MmapFn;

	// Protect OpenContexts[i]::Open.
	struct mutex         lock;

	OSDEV_OpenContext_t  OpenContexts[OSDEV_MAX_OPENS];
};

//      The implementation level device functions to support device implementation
//      TODO(pht) rework all these to use modern dynamic dev allocations

static inline OSDEV_Status_t OSDEV_RegisterDevice(OSDEV_Descriptor_t *Descriptor)
{
	size_t namesize;

	if (OSDEV_DeviceDescriptors[Descriptor->MajorNumber] != NULL) {
		pr_err("Error: %s %s Attempt to register two devices with same MajorNumber\n", __func__, Descriptor->Name);
		return OSDEV_Error;
	}

	OSDEV_DeviceDescriptors[Descriptor->MajorNumber] = (OSDEV_Descriptor_t *)OSDEV_Malloc(sizeof(OSDEV_Descriptor_t));
	if (OSDEV_DeviceDescriptors[Descriptor->MajorNumber] == NULL) {
		pr_err("Error: %s %s Insufficient memory to hold device descriptor\n", __func__, Descriptor->Name);
		return OSDEV_Error;
	}

	memset(OSDEV_DeviceDescriptors[Descriptor->MajorNumber], 0, sizeof(OSDEV_Descriptor_t));

	OSDEV_DeviceDescriptors[Descriptor->MajorNumber]->MajorNumber       = Descriptor->MajorNumber;
	OSDEV_DeviceDescriptors[Descriptor->MajorNumber]->OpenFn            = Descriptor->OpenFn;
	OSDEV_DeviceDescriptors[Descriptor->MajorNumber]->CloseFn           = Descriptor->CloseFn;
	OSDEV_DeviceDescriptors[Descriptor->MajorNumber]->IoctlFn           = Descriptor->IoctlFn;
	OSDEV_DeviceDescriptors[Descriptor->MajorNumber]->MmapFn            = Descriptor->MmapFn;

	namesize = strlen(Descriptor->Name) + 1;
	OSDEV_DeviceDescriptors[Descriptor->MajorNumber]->Name              = OSDEV_Malloc(namesize);
	if (OSDEV_DeviceDescriptors[Descriptor->MajorNumber]->Name == NULL) {
		pr_err("Error: %s %s Insufficient memory to record device name\n", __func__, Descriptor->Name);

		OSDEV_Free(OSDEV_DeviceDescriptors[Descriptor->MajorNumber]);
		OSDEV_DeviceDescriptors[Descriptor->MajorNumber] = NULL;

		return OSDEV_Error;
	}
	strncpy(OSDEV_DeviceDescriptors[Descriptor->MajorNumber]->Name, Descriptor->Name, namesize);
	OSDEV_DeviceDescriptors[Descriptor->MajorNumber]->Name[namesize - 1] = '\0';
	mutex_init(&OSDEV_DeviceDescriptors[Descriptor->MajorNumber]->lock);

	return OSDEV_NoError;
}

static inline void OSDEV_DeRegisterDevice(OSDEV_Descriptor_t      *Descriptor)
{
	int     i;
	OSDEV_Descriptor_t  *Descriptor_major;

	if (OSDEV_DeviceDescriptors[Descriptor->MajorNumber] == NULL) {
		pr_err("Error: %s %s Attempt to de-register an unregistered device\n", __func__, Descriptor->Name);
		return;
	}

	Descriptor_major = OSDEV_DeviceDescriptors[Descriptor->MajorNumber];
	OSDEV_DeviceDescriptors[Descriptor->MajorNumber]    = NULL;

	for (i = 0; i < OSDEV_MAX_OPENS; i++) {
		if (Descriptor_major->OpenContexts[i].Open) {
			pr_info("%s %s - Forcing close\n", __func__, Descriptor_major->Name);
			Descriptor_major->CloseFn(&Descriptor_major->OpenContexts[i]);
		}
	}
	mutex_destroy(&Descriptor_major->lock);
	OSDEV_Free(Descriptor_major->Name);
	OSDEV_Free(Descriptor_major);
}

static inline OSDEV_Status_t  OSDEV_LinkDevice(char         *Name,
                                               unsigned int  MajorNumber,
                                               unsigned int  MinorNumber)
{
	size_t namesize;
	int i;

	pr_info("%s %s - Major %d, Minor %d\n", __func__, Name, MajorNumber, MinorNumber);

	for (i = 0; i < OSDEV_MAXIMUM_DEVICE_LINKS; i++) {
		if (!OSDEV_DeviceList[i].Valid) {
			OSDEV_DeviceList[i].MajorNumber     = MajorNumber;
			OSDEV_DeviceList[i].MinorNumber     = MinorNumber;

			namesize = strlen(Name) + 1;
			OSDEV_DeviceList[i].Name = OSDEV_Malloc(namesize);
			if (OSDEV_DeviceList[i].Name == NULL) {
				pr_err("Error: %s failed to allocate memory for link name\n", __func__);
				return OSDEV_Error;
			}
			strncpy(OSDEV_DeviceList[i].Name, Name, namesize);
			OSDEV_DeviceList[i].Name[namesize - 1] = '\0';

			OSDEV_DeviceList[i].Valid = true;
			return OSDEV_NoError;
		}
	}

	pr_err("Error: %s All device links used\n", __func__);
	return OSDEV_Error;
}

static inline void OSDEV_UnLinkDevice(char *Name)
{
	int     i;

	for (i = 0; i < OSDEV_MAXIMUM_DEVICE_LINKS; i++) {
		if (OSDEV_DeviceList[i].Valid && (strcmp(Name, OSDEV_DeviceList[i].Name) == 0)) {
			OSDEV_Free(OSDEV_DeviceList[i].Name);
			OSDEV_DeviceList[i].Name = NULL;
			OSDEV_DeviceList[i].Valid           = false;
			return;
		}
	}

	pr_err("Error: %s Device not found\n", __func__);
}

static inline void OSDEV_CopyToDeviceSpace(void         *DeviceAddress,
                                           unsigned int  UserAddress,
                                           unsigned int  Size)
{
	memcpy(DeviceAddress, (void *)UserAddress, Size);
}

static inline void OSDEV_CopyToUserSpace(unsigned int    UserAddress,
                                         void           *DeviceAddress,
                                         unsigned int    Size)
{
	memcpy((void *)UserAddress, DeviceAddress, Size);
}

// The entry/exit macros for use on each of the device fns
// and the access macros to store and retrieve context
//
// During the functions, variables such as OSDEV_PrivateData,
// and OSDEV_MinorDeviceNumber are available. The private data
// is stored on exit, all others are not.
//

#define OSDEV_OpenEntry()       void            *OSDEV_PrivateData              = OSDEV_OpenContext->PrivateData;       \
                                unsigned int     OSDEV_MinorDeviceNumber        = OSDEV_OpenContext->MinorNumber;

#define OSDEV_OpenExit(Status)  OSDEV_OpenContext->PrivateData                  = OSDEV_PrivateData;    \
                                (void) OSDEV_MinorDeviceNumber; /* warning suppression */               \
                                return Status;

//

#define OSDEV_CloseEntry()      void            *OSDEV_PrivateData              = OSDEV_OpenContext->PrivateData;       \
                                unsigned int     OSDEV_MinorDeviceNumber        = OSDEV_OpenContext->MinorNumber;


#define OSDEV_CloseExit(Status) (void) OSDEV_MinorDeviceNumber; /* warning suppression */               \
                                return Status;

//

#define OSDEV_IoctlEntry()      void            *OSDEV_PrivateData              = OSDEV_OpenContext->PrivateData;       \
                                unsigned int     OSDEV_MinorDeviceNumber        = OSDEV_OpenContext->MinorNumber;       \
                                unsigned int     OSDEV_IoctlCode                = cmd;                                  \
                                unsigned int     OSDEV_ParameterAddress         = arg;


#define OSDEV_IoctlExit(Status) OSDEV_OpenContext->PrivateData                  = OSDEV_PrivateData;    \
                                (void) OSDEV_MinorDeviceNumber; /* warning suppression */               \
                                return Status;

//

#define OSDEV_MmapEntry()       void            *OSDEV_PrivateData              = OSDEV_OpenContext->PrivateData;       \
                                unsigned char   *OSDEV_VirtualMemoryStart       = NULL;                                 \
                                unsigned char   *OSDEV_VirtualMemoryEnd         = (unsigned char *)OSDEV_VMSize;        \
                                unsigned int     OSDEV_Offset                   = OSDEV_VMOffset;

#define OSDEV_MmapExit(Status)  OSDEV_OpenContext->PrivateData                  = OSDEV_PrivateData;                    \
                                *OSDEV_VMAddress                                = OSDEV_VirtualMemoryStart;             \
                                (void) OSDEV_VirtualMemoryEnd; /* warning suppression */                                \
                                (void) OSDEV_Offset; /* warning suppression */                                          \
                                return Status;

#define OSDEV_MmapPerformMap(PhysicalAddress, MappingStatus)                                                            \
                                {                                                                                       \
                                    MappingStatus                               = OSDEV_NoError;                        \
                                    OSDEV_VirtualMemoryStart                    = PhysicalAddress;                      \
                                }

#endif // H_OSDEV_DEVICE
