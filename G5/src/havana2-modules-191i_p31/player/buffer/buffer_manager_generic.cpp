/************************************************************************
Copyright (C) 2006 STMicroelectronics. All Rights Reserved.

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

Source file name : buffer_manager_generic.cpp
Author :           Nick

Implementation of the buffer manager generic class for use in player 2


Date        Modification                                    Name
----        ------------                                    --------
14-Jul-06   Created                                         Nick

************************************************************************/

#include "osinline.h"

#include "buffer_generic.h"

// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Useful defines/macros that need not be user visible
//

// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Contructor function - Initialize our data
//

BufferManager_Generic_c::BufferManager_Generic_c( void )
{
    InitializationStatus	= BufferError;

//

    OS_InitializeMutex( &Lock );

    TypeDescriptorCount		= 0;
    ListOfBufferPools		= NULL;

//

    InitializationStatus	= BufferNoError;
}


// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Destructor function - close down
//

BufferManager_Generic_c::~BufferManager_Generic_c( void )
{
    while( ListOfBufferPools != NULL )
	DestroyPool( ListOfBufferPools );

    OS_TerminateMutex( &Lock );
}


// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//	Add to defined types
//

BufferStatus_t   BufferManager_Generic_c::CreateBufferDataType(
						BufferDataDescriptor_t	 *Descriptor,
						BufferType_t		 *Type )
{
    //
    // Initialize the return parameter
    //

    *Type	= 0xffffffff;

    //
    // Are we ok with this
    //

    if( Descriptor->Type == MetaDataTypeBase )
    {
	if( Descriptor->AllocationSource == AllocateIndividualSuppliedBlocks )
	{
	    report( severity_error, "BufferManager_Generic_c::CreateBufferDataType - Cannot allocate meta data from supplied block list.\n" );
	    return BufferUnsupportedAllocationSource;
	}

	if( Descriptor->AllocateOnPoolCreation )
	{
	    report( severity_error, "BufferManager_Generic_c::CreateBufferDataType - Cannot allocate meta data on pool creation.\n" );
	    return BufferUnsupportedAllocationSource;
	}
    }
    else if( Descriptor->Type != BufferDataTypeBase )
    {
	report( severity_error, "BufferManager_Generic_c::CreateBufferDataType - Invalid type value.\n" );
	return BufferInvalidDescriptor;
    }

//

    if( Descriptor->HasFixedSize && (Descriptor->FixedSize == NOT_SPECIFIED) )
    {
	report( severity_error, "BufferManager_Generic_c::CreateBufferDataType - Fixed size not specified.\n" );
	return BufferInvalidDescriptor;
    }

    //
    // From here on in we need to lock access to the data structure, since we are now allocating an entry
    //

    OS_LockMutex( &Lock );

    if( TypeDescriptorCount >= MAX_BUFFER_DATA_TYPES )
    {
	report( severity_error, "BufferManager_Generic_c::CreateBufferDataType - Insufficient space to hold new data type descriptor.\n" );
	OS_UnLockMutex( &Lock );
	return BufferTooManyDataTypes;
    }

//

    memcpy( &TypeDescriptors[TypeDescriptorCount], Descriptor, sizeof(BufferDataDescriptor_t) );

    if( Descriptor->TypeName != NULL )
    {
	TypeDescriptors[TypeDescriptorCount].TypeName	= strdup( Descriptor->TypeName );
	if( TypeDescriptors[TypeDescriptorCount].TypeName == NULL )
	{
	    report( severity_error, "BufferManager_Generic_c::CreateBufferDataType - Insufficient memory to copy type name.\n" );
	    OS_UnLockMutex( &Lock );
	    return BufferInsufficientMemoryGeneral;
	}
    }

    TypeDescriptors[TypeDescriptorCount].Type	= Descriptor->Type | TypeDescriptorCount;
    *Type					= TypeDescriptors[TypeDescriptorCount].Type;


//

    TypeDescriptorCount++;
    OS_UnLockMutex( &Lock );

    return BufferNoError;
}


// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//	Retrieve the descriptor for a defined type
//

BufferStatus_t   BufferManager_Generic_c::FindBufferDataType(
						const char		 *TypeName,
						BufferType_t		 *Type )
{
unsigned int	i;

    //
    // Initialize the return parameter
    //

    if( TypeName == NULL )
    {
	report( severity_error, "BufferManager_Generic_c::FindBufferDataType - Cannot find an unnamed type.\n" );
	return BufferDataTypeNotFound;
    }

//

    for( i=0; i<TypeDescriptorCount; i++ )
	if( (TypeDescriptors[i].TypeName != NULL) &&
	    (strcmp(TypeDescriptors[i].TypeName, TypeName) == 0) )
	{
	    *Type	= TypeDescriptors[i].Type;
	    return BufferNoError;
	}

//

    return BufferDataTypeNotFound;
}


// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Get descriptor
//

BufferStatus_t   BufferManager_Generic_c::GetDescriptor( BufferType_t		  Type,
							 BufferPredefinedType_t	  RequiredKind,
							 BufferDataDescriptor_t	**Descriptor )
{
    if( (Type & TYPE_TYPE_MASK) != (unsigned int)RequiredKind )
    {
	report( severity_error, "BufferManager_Generic_c::GetDescriptor - Wrong kind of data type (%08x).\n", RequiredKind );
	return BufferDataTypeNotFound;
    }

    if( (Type & TYPE_INDEX_MASK) > TypeDescriptorCount )
    {
	report( severity_error, "BufferManager_Generic_c::GetDescriptor - Attempt to create buffer with unregisted type (%08x).\n", Type );
	return BufferDataTypeNotFound;
    }

    *Descriptor	= &TypeDescriptors[Type & TYPE_INDEX_MASK];

    if( (*Descriptor)->Type != Type )
    {
	report( severity_error, "BufferManager_Generic_c::GetDescriptor - Type descriptor does not match type.\n" );
	return BufferInvalidDescriptor;
    }

    return BufferNoError;
}


// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Create a pool of buffers - common implementation
//

BufferStatus_t   BufferManager_Generic_c::CreatePool(
						BufferPool_t	 *Pool,
						BufferType_t	  Type,
					    	unsigned int	  NumberOfBuffers,
						unsigned int	  Size,
					    	void		 *MemoryPool[3],
					    	void		 *ArrayOfMemoryBlocks[][3],
						char		 *DeviceMemoryPartitionName )
{
BufferStatus_t		 Status;
BufferDataDescriptor_t	*Descriptor;
BufferPool_Generic_t	 NewPool;

    //
    // Initialize the return parameter
    //

    *Pool	= NULL;

    //
    // Get the descriptor
    //

    Status	= GetDescriptor( Type, BufferDataTypeBase, &Descriptor );
    if( Status != BufferNoError )
	return Status;

    //
    // Perform simple parameter checks
    //

    if( Pool == NULL )
    {
	report( severity_error, "BufferManager_Generic_c::CreatePool - Null supplied as place to return Pool pointer.\n" );
	return BufferError;
    }

    if( Descriptor->AllocateOnPoolCreation &&
	(NumberOfBuffers == NOT_SPECIFIED) )
    {
	report( severity_error, "BufferManager_Generic_c::CreatePool - Unable to allocate on creation, NumberOfBuffers not specified.\n" );
	return BufferParametersIncompatibleWithAllocationSource;
    }

    //
    // Perform the creation
    //

    NewPool	= new BufferPool_Generic_c( this, Descriptor, NumberOfBuffers, Size, MemoryPool, ArrayOfMemoryBlocks, DeviceMemoryPartitionName );
    if( (NewPool == NULL) || (NewPool->InitializationStatus != BufferNoError) )
    {
    BufferStatus_t	Status;

	Status		= BufferInsufficientMemoryForPool;
	if( NewPool != NULL )
	    Status	= NewPool->InitializationStatus;

	report( severity_error, "BufferManager_Generic_c::CreatePool - Failed to create pool (%08x).\n", Status );
	return Status;
    }

    //
    // Insert the pool into our list
    //

    OS_LockMutex( &Lock );

    NewPool->Next	= ListOfBufferPools;
    ListOfBufferPools	= NewPool;

    OS_UnLockMutex( &Lock );

//

    *Pool	= NewPool;
    return BufferNoError;
}


// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Destroy a pool of buffers
//

BufferStatus_t   BufferManager_Generic_c::DestroyPool( BufferPool_t	  Pool )
{
BufferPool_Generic_t	 LocalPool	= (BufferPool_Generic_t)Pool;
BufferPool_Generic_t	*LocationOfPointer;

    //
    // First find and remove the pool from our list
    //

    OS_LockMutex( &Lock );

    for( LocationOfPointer	 = &ListOfBufferPools;
	 *LocationOfPointer	!= NULL;
	 LocationOfPointer	 = &((*LocationOfPointer)->Next) )
	if( *LocationOfPointer == LocalPool )
	    break;

    if( *LocationOfPointer == NULL )
    {
	report( severity_error, "BufferManager_Generic_c::DestroyPool - Pool not found.\n" );
	OS_UnLockMutex( &Lock );
	return BufferPoolNotFound;
    }

    *LocationOfPointer	= LocalPool->Next;
    OS_UnLockMutex( &Lock );

    //
    // Then destroy the actual pool
    //

    delete LocalPool;

    return BufferNoError;
}


// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Status dump/reporting
//

static const char *AllocationSources[] =
{
    "NoAllocation",
    "AllocateFromOSMemory",
    "AllocateFromDeviceMemory",
    "AllocateFromDeviceVideoLMIMemory",
    "AllocateFromSuppliedBlock",
    "AllocateIndividualSuppliedBlocks",
    "AllocateFromNamedDeviceMemory"
};

//

void   BufferManager_Generic_c::Dump( unsigned int 	  Flags )
{
unsigned int		i;
BufferPool_Generic_t	Pool;

//

    if( (Flags & DumpBufferTypes) != 0 )
    {
	report( severity_info, "Dump of Buffer Types\n" );

	for( i=0; i<TypeDescriptorCount; i++ )
	    if( (TypeDescriptors[i].Type & TYPE_TYPE_MASK) == BufferDataTypeBase )
	    {
		report( severity_info, "    Buffer Type $%04x - '%s'\n", TypeDescriptors[i].Type,
			(TypeDescriptors[i].TypeName == NULL) ? "Unnamed" : TypeDescriptors[i].TypeName );

		report( severity_info, "\tAllocationSource = %s\n", (TypeDescriptors[i].AllocationSource <= AllocateIndividualSuppliedBlocks) ?
			AllocationSources[TypeDescriptors[i].AllocationSource] :
			"Invalid" );

		report( severity_info, "\tRequiredAllignment = %08x, AllocationUnitSize = %08x, AllocateOnPoolCreation = %d\n",
			TypeDescriptors[i].RequiredAllignment, TypeDescriptors[i].AllocationUnitSize, TypeDescriptors[i].AllocateOnPoolCreation );

		report( severity_info, "\tHasFixedSize = %d, FixedSize = %08x\n",
			TypeDescriptors[i].HasFixedSize, TypeDescriptors[i].FixedSize );
	    }
	report( severity_info, "\n" );
    }

//

    if( (Flags & DumpMetaDataTypes) != 0 )
    {
	report( severity_info, "Dump of Meta Data Types\n" );

	for( i=0; i<TypeDescriptorCount; i++ )
	    if( (TypeDescriptors[i].Type & TYPE_TYPE_MASK) == MetaDataTypeBase )
	    {
		report( severity_info, "    Buffer Type $%04x - '%s'\n", TypeDescriptors[i].Type,
			(TypeDescriptors[i].TypeName == NULL) ? "Unnamed" : TypeDescriptors[i].TypeName );

		report( severity_info, "\tAllocationSource = %s\n", (TypeDescriptors[i].AllocationSource <= AllocateIndividualSuppliedBlocks) ?
			AllocationSources[TypeDescriptors[i].AllocationSource] :
			"Invalid" );

		report( severity_info, "\tRequiredAllignment = %08x, AllocationUnitSize = %08x, AllocateOnPoolCreation = %d\n",
			TypeDescriptors[i].RequiredAllignment, TypeDescriptors[i].AllocationUnitSize, TypeDescriptors[i].AllocateOnPoolCreation );

		report( severity_info, "\tHasFixedSize = %d, FixedSize = %08x\n",
			TypeDescriptors[i].HasFixedSize, TypeDescriptors[i].FixedSize );
	    }
	report( severity_info, "\n" );
    }

//

    if( (Flags & DumpListPools) != 0 )
    {
	report( severity_info, "Dump of Buffer Pools\n" );

	OS_LockMutex( &Lock );

	for( Pool  = ListOfBufferPools;
	     Pool != NULL;
	     Pool  = Pool->Next )
	{
	    Pool->Dump( Flags );
	}

	OS_UnLockMutex( &Lock );
    }
}

