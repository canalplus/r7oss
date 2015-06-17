/*
** allocator.c - implementation of a simple memory allocator for kernel memory
**
** Copyright 2003 STMicroelectronics, all right reserved.
**
**          MAJOR  MINOR  DESCRIPTION
**          -----  -----  ----------------------------------------------
**          245     0      Allocate chunks of kernel/device memory
*/

#define MAJOR_NUMBER    245
#define MINOR_NUMBER    0

#include "osdev_device.h"

/* --- */

#include "allocatorio.h"

// ////////////////////////////////////////////////////////////////////////////////
//
//

static OSDEV_OpenEntrypoint(  AllocatorOpen );
static OSDEV_CloseEntrypoint( AllocatorClose );
static OSDEV_IoctlEntrypoint( AllocatorIoctl );
static OSDEV_MmapEntrypoint(  AllocatorMmap );

static OSDEV_Descriptor_t AllocatorDeviceDescriptor =
{
	Name:           "AllocatorModule",
	MajorNumber:    MAJOR_NUMBER,

	OpenFn:         AllocatorOpen,
	CloseFn:        AllocatorClose,
	IoctlFn:        AllocatorIoctl,
	MmapFn:         AllocatorMmap
};

/* --- External entrypoints --- */

OSDEV_LoadEntrypoint( AllocatorLoadModule );
OSDEV_UnloadEntrypoint( AllocatorUnloadModule );

OSDEV_RegisterDeviceLoadFn( AllocatorLoadModule );
OSDEV_RegisterDeviceUnloadFn( AllocatorUnloadModule );

/* --- The context for an opening of the device --- */

typedef struct AllocatorContext_s
{
    unsigned int         Size;
    unsigned char	 PartitionName[ALLOCATOR_MAX_PARTITION_NAME_SIZE];
    unsigned char       *Memory;
    unsigned char	*CachedAddress;
    unsigned char	*UnCachedAddress;
    unsigned char       *PhysicalAddress;
} AllocatorContext_t;

// ////////////////////////////////////////////////////////////////////////////////
//
//    The Initialize module function

OSDEV_LoadEntrypoint( AllocatorLoadModule )
{
OSDEV_Status_t  Status;

//

    OSDEV_LoadEntry();

    //
    // Register our device
    //

    Status = OSDEV_RegisterDevice( &AllocatorDeviceDescriptor );
    if( Status != OSDEV_NoError )
    {
	OSDEV_Print( "AllocatorLoadModule : Unable to get major %d\n", MAJOR_NUMBER );
	OSDEV_LoadExit( OSDEV_Error );
    }

    OSDEV_LinkDevice(  ALLOCATOR_DEVICE, MAJOR_NUMBER, MINOR_NUMBER );

//

    OSDEV_Print( "AllocatorLoadModule : Allocator device loaded\n" );
    OSDEV_LoadExit( OSDEV_NoError );
}


// ////////////////////////////////////////////////////////////////////////////////
//
//    The Terminate module function

OSDEV_UnloadEntrypoint( AllocatorUnloadModule )
{
OSDEV_Status_t          Status;

/* --- */

    OSDEV_UnloadEntry();


/* --- */

    Status = OSDEV_DeRegisterDevice( &AllocatorDeviceDescriptor );
    if( Status != OSDEV_NoError )
	OSDEV_Print( "AllocatorUnloadModule : Unregister of device failed\n");

/* --- */

    OSDEV_Print( "Allocator device unloaded\n" );
    OSDEV_UnloadExit(OSDEV_NoError);
}


// ////////////////////////////////////////////////////////////////////////////////
//
//    The Open function

static OSDEV_OpenEntrypoint(  AllocatorOpen )
{
AllocatorContext_t      *AllocatorContext;

    //
    // Entry
    //

    OSDEV_OpenEntry();

    //
    // Allocate and initialize the private data structure.
    //

    AllocatorContext    = (AllocatorContext_t *)OSDEV_Malloc( sizeof(AllocatorContext_t) );
    OSDEV_PrivateData   = (void *)AllocatorContext;

    if( AllocatorContext == NULL )
    {
	OSDEV_Print( "AllocatorOpen - Unable to allocate memory for open context.\n" );
	OSDEV_OpenExit( OSDEV_Error );
    }

//

    memset( AllocatorContext, 0x00, sizeof(AllocatorContext_t) );

    AllocatorContext->Size      = 0;
    AllocatorContext->Memory    = NULL;

//

    OSDEV_OpenExit( OSDEV_NoError );
}


// ////////////////////////////////////////////////////////////////////////////////
//
//    The Release function

static OSDEV_CloseEntrypoint(  AllocatorClose )
{
AllocatorContext_t      *AllocatorContext;

//

    OSDEV_CloseEntry();
    AllocatorContext    = (AllocatorContext_t  *)OSDEV_PrivateData;

    //
    // Perform the release activity
    //

    if( AllocatorContext->Memory != NULL )
    {
        // Do what is necessary to free up any mapping here
        
//        OSDEV_Print( "Freeing up bpa 2 partition - phys %p - C %p - UC %p\n", AllocatorContext->PhysicalAddress,AllocatorContext->CachedAddress,AllocatorContext->UnCachedAddress);
        OSDEV_IOUnMap( (unsigned int)AllocatorContext->UnCachedAddress );
        OSDEV_IOUnMap( (unsigned int)AllocatorContext->CachedAddress );
        OSDEV_FreePartitioned( AllocatorContext->PartitionName, AllocatorContext->PhysicalAddress );
        
    }

    OSDEV_Free( AllocatorContext );

//

    OSDEV_CloseExit( OSDEV_NoError );
}

// ////////////////////////////////////////////////////////////////////////////////
//
//    The mmap function to make the Allocator buffers available to the user for writing

static OSDEV_MmapEntrypoint( AllocatorMmap )
{
AllocatorContext_t      *AllocatorContext;
OSDEV_Status_t           MappingStatus;

//

    OSDEV_MmapEntry();
    AllocatorContext    = (AllocatorContext_t  *)OSDEV_PrivateData;

//

    OSDEV_MmapPerformMap( AllocatorContext->Memory, MappingStatus );

//

    OSDEV_MmapExit( MappingStatus );
}

// ////////////////////////////////////////////////////////////////////////////////
//
//    The ioctl function to handle allocating the memory

static OSDEV_Status_t AllocatorIoctlAllocateData( AllocatorContext_t    *AllocatorContext,
						  unsigned int           ParameterAddress )
{
allocator_ioctl_allocate_t      params;

//

    OSDEV_CopyToDeviceSpace( &params, ParameterAddress, sizeof(allocator_ioctl_allocate_t) );

//

    AllocatorContext->Size              = params.RequiredSize;
    memcpy( AllocatorContext->PartitionName, params.PartitionName, ALLOCATOR_MAX_PARTITION_NAME_SIZE );

//

    AllocatorContext->Memory        = OSDEV_MallocPartitioned( 	AllocatorContext->PartitionName, 
								AllocatorContext->Size );

    if( AllocatorContext->Memory == NULL )
    {
	OSDEV_Print( "AllocatorIoctlAllocateData : Unable to allocate memory\n" );
	return OSDEV_Error;
    }

    //
    // The memory supplied by BPA2 is physical, get the necessary mappings
    //

    AllocatorContext->CachedAddress	= NULL;
    AllocatorContext->UnCachedAddress	= NULL;
    AllocatorContext->PhysicalAddress	= NULL;

        
    AllocatorContext->CachedAddress	= ioremap_cache((unsigned int)AllocatorContext->Memory,AllocatorContext->Size);
    AllocatorContext->PhysicalAddress	= AllocatorContext->Memory ;
    AllocatorContext->UnCachedAddress	= (unsigned char *)OSDEV_IOReMap( (unsigned int)AllocatorContext->PhysicalAddress, AllocatorContext->Size );

/*    
    OSDEV_Print("Alloc - Phys %p - C %p - UC %p  -- Size 0x%x\n",AllocatorContext->PhysicalAddress,
                AllocatorContext->CachedAddress, AllocatorContext->UnCachedAddress,AllocatorContext->Size);
*/    
    //
    // Copy the data into the parameters and pass back 
    //

    params.CachedAddress	= AllocatorContext->CachedAddress;
    params.UnCachedAddress	= AllocatorContext->UnCachedAddress;
    params.PhysicalAddress	= AllocatorContext->PhysicalAddress;

    OSDEV_CopyToUserSpace( ParameterAddress, &params, sizeof(allocator_ioctl_allocate_t) );

//

    return OSDEV_NoError;
}


// ////////////////////////////////////////////////////////////////////////////////
//
//    The ioctl function

static OSDEV_IoctlEntrypoint( AllocatorIoctl )
{
OSDEV_Status_t           Status;
AllocatorContext_t      *AllocatorContext;

/* --- */

    OSDEV_IoctlEntry();
    AllocatorContext    = (AllocatorContext_t  *)OSDEV_PrivateData;

    switch( OSDEV_IoctlCode )
    {
	case    ALLOCATOR_IOCTL_ALLOCATE_DATA:  Status = AllocatorIoctlAllocateData(    AllocatorContext, OSDEV_ParameterAddress );             break;

	default:        OSDEV_Print( "AllocatorIoctl : Invalid ioctl %08x\n", OSDEV_IoctlCode );
			Status = OSDEV_Error;
    }

/* --- */

    OSDEV_IoctlExit( Status );
}


