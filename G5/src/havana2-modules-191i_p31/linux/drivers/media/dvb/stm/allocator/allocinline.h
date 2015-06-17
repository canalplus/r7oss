/************************************************************************
Copyright (C) 2003 STMicroelectronics. All Rights Reserved.

This file is part of the Player2 Library.

Player2 is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License version 2 as published by the
Free Software Foundation.

Player2 is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with player2; see the file COPYING.  If not, write to the Free Software
Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

The Player2 Library may alternatively be licensed under a proprietary
license from ST.

Source file name : allocinline.h
Author :           Nick

Contains the useful operating system dependent inline functions/types
that allow use of the allocator device.

Date        Modification                                    Name
----        ------------                                    --------
14-Jan-03   Created                                         Nick
12-Nov-07   Changed to use partition names for BPA2         Nick

************************************************************************/

#ifndef H_ALLOCINLINE
#define H_ALLOCINLINE

/* --- Include the os specific headers we will need --- */

#include "report.h"
#include <osinline.h>
#include <osdev_user.h>
#include "allocatorio.h"

/* --- */

typedef enum
{
    allocator_ok                = 0,
    allocator_error
} allocator_status_t;

//

struct allocator_device_s
{
    OSDEV_DeviceIdentifier_t     UnderlyingDevice;
    unsigned int                 BufferSize;
    unsigned char               *CachedAddress;
    unsigned char               *UnCachedAddress;
    unsigned char               *PhysicalAddress;
};

typedef struct allocator_device_s       *allocator_device_t;
#define ALLOCATOR_INVALID_DEVICE         NULL

#define DEVICE_NAME                     "/dev/allocator"
#define	SYS_LMI_PARTITION		"BPA2_Region0"
#define	VID_LMI_PARTITION		"BPA2_Region1"


// ------------------------------------------------------------------------------------------------------------
//   The open function

static inline allocator_status_t   AllocatorOpenEx( 
						    allocator_device_t    *Device,
						    unsigned int           Size,
						    bool                   UseCachedMemory,
						    const char		  *PartitionName )
{
OSDEV_Status_t                  Status;
allocator_ioctl_allocate_t      Parameters;

//
//  Check the size of the partition name
//

    if( strlen(PartitionName) > (ALLOCATOR_MAX_PARTITION_NAME_SIZE - 1) )
    {
	report( severity_error, "AllocatorOpenEx : Partition name too large ( %d > %d)\n", strlen(PartitionName), (ALLOCATOR_MAX_PARTITION_NAME_SIZE - 1) );
	*Device = ALLOCATOR_INVALID_DEVICE;
	return allocator_error;
    }

//
//  Malloc the device structure
//

    *Device = (allocator_device_t)OS_Malloc( sizeof(struct allocator_device_s) );
    if( *Device == NULL )
    {
	report( severity_error, "AllocatorOpenEx : Failed to allocate memory for device context.\n" );
	return allocator_error;
    }

    memset( *Device, 0, sizeof(struct allocator_device_s) );

// 
//  Open the device
//

    Status = OSDEV_Open( DEVICE_NAME, &((*Device)->UnderlyingDevice) );
    if( Status != OSDEV_NoError )
    {
	report( severity_error, "AllocatorOpenEx : Failed to open\n" );
	OS_Free( *Device );
	*Device = ALLOCATOR_INVALID_DEVICE;
	return allocator_error;
    }

//
//  Allocate the memory
//

    Parameters.RequiredSize     	= Size;
    strcpy( Parameters.PartitionName, PartitionName );

    Status = OSDEV_Ioctl(  (*Device)->UnderlyingDevice, ALLOCATOR_IOCTL_ALLOCATE_DATA,
			   &Parameters, sizeof(allocator_ioctl_allocate_t) );
    if( Status != OSDEV_NoError )
    {
	report( severity_error, "AllocatorOpenEx : Failed to allocate memory (0x%08x from '%s')\n", Size, PartitionName );
	OSDEV_Close( (*Device)->UnderlyingDevice );
	OS_Free( *Device );
	*Device = ALLOCATOR_INVALID_DEVICE;
	return allocator_error;
    }

    (*Device)->BufferSize       = Size;
    (*Device)->CachedAddress    = Parameters.CachedAddress;
    (*Device)->UnCachedAddress	= Parameters.UnCachedAddress;
    (*Device)->PhysicalAddress  = Parameters.PhysicalAddress;

//

    return allocator_ok;
}

// ------------------------------------------------------------------------------------------------------------
//   The open function

static inline allocator_status_t   AllocatorOpen( allocator_device_t    *Device,
						  unsigned int           Size,
						  bool                   UseCachedMemory )
{
    return AllocatorOpenEx( Device, Size, UseCachedMemory, SYS_LMI_PARTITION );
}

// ------------------------------------------------------------------------------------------------------------
//   The open function

static inline allocator_status_t   LmiAllocatorOpen( allocator_device_t    *Device,
						     unsigned int           Size,
						     bool                   UseCachedMemory )
{
    return AllocatorOpenEx( Device, Size, UseCachedMemory, VID_LMI_PARTITION );
}

// ------------------------------------------------------------------------------------------------------------
//   The open function

static inline allocator_status_t   PartitionAllocatorOpen( 
							allocator_device_t	*Device,
							const char		*PartitionName,
						     	unsigned int		 Size,
						     	bool			 UseCachedMemory )
{
    return AllocatorOpenEx( Device, Size, UseCachedMemory, PartitionName );
}

// ------------------------------------------------------------------------------------------------------------
//   The close function

static inline void              AllocatorClose( allocator_device_t Device )
{
    if( Device != ALLOCATOR_INVALID_DEVICE )
    {
	OSDEV_Close( Device->UnderlyingDevice );
	OS_Free( Device );
    }
}

// ------------------------------------------------------------------------------------------------------------
//   The kernel address function

static inline   unsigned char *AllocatorKernelAddress( allocator_device_t Device )
{
// Deprecated call, we assume that kernel address only refers to the physical address
    if( Device != ALLOCATOR_INVALID_DEVICE )
	return Device->PhysicalAddress;
    else
	return NULL;
}

// ------------------------------------------------------------------------------------------------------------
//   The User address function

static inline   unsigned char *AllocatorUserAddress( allocator_device_t Device )
{
    if( Device != ALLOCATOR_INVALID_DEVICE )
	return Device->CachedAddress;
    else
	return NULL;
}

// ------------------------------------------------------------------------------------------------------------
//   The uncached user address function

static inline   unsigned char *AllocatorUncachedUserAddress( allocator_device_t Device )
{
    if( Device != ALLOCATOR_INVALID_DEVICE )
	return Device->UnCachedAddress;
    else
	return NULL;
}

// ------------------------------------------------------------------------------------------------------------
//   The Physical address function

static inline   unsigned char *AllocatorPhysicalAddress( allocator_device_t Device )
{
    if( Device != ALLOCATOR_INVALID_DEVICE )
	return Device->PhysicalAddress;
    else
	return NULL;
}

#endif
