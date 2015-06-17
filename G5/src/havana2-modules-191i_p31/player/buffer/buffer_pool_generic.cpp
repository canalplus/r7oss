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

Source file name : buffer_pool_generic.cpp
Author :           Nick

Implementation of the buffer pool generic class for use in player 2


Date        Modification                                    Name
----        ------------                                    --------
26-Jul-06   Created                                         Nick

************************************************************************/

#include "osinline.h"

#include "allocator_simple.h"
#include "ring_generic.h"
#include "buffer_generic.h"

// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Useful defines/macros that need not be user visible
//

#define BUFFER_MAX_EXPECTED_WAIT_PERIOD		30000000ULL

// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Contructor function - Initialize our data
//

BufferPool_Generic_c::BufferPool_Generic_c(     BufferManager_Generic_t   Manager,
						BufferDataDescriptor_t   *Descriptor, 
						unsigned int              NumberOfBuffers,
						unsigned int              Size,
						void                     *MemoryPool[3],
						void                     *ArrayOfMemoryBlocks[][3],
						char			 *DeviceMemoryPartitionName )
{
unsigned int            i,j;
BufferStatus_t          Status;
Buffer_Generic_t        Buffer;
unsigned int            ItemSize;

//

    InitializationStatus        = BufferError;

    //
    // Initialize class data
    //

    this->Manager               = Manager;
    Next                        = NULL;

    OS_InitializeMutex( &Lock );
    OS_InitializeEvent( &BufferReleaseSignal );

    ReferenceCount		= 0;

    BufferDescriptor            = NULL;
    this->NumberOfBuffers       = 0;
    CountOfBuffers              = 0;
    this->Size                  = 0;

    ListOfBuffers               = NULL;
    FreeBuffer                  = NULL;
    this->MemoryPool[0]         = NULL;
    this->MemoryPool[1]         = NULL;
    this->MemoryPool[2]         = NULL;
    MemoryPoolAllocator         = NULL;
    MemoryPoolAllocatorDevice   = ALLOCATOR_INVALID_DEVICE;
    memset( MemoryPartitionName, 0x00, ALLOCATOR_MAX_PARTITION_NAME_SIZE );
    if( DeviceMemoryPartitionName != NULL )
	strncpy( MemoryPartitionName, DeviceMemoryPartitionName, ALLOCATOR_MAX_PARTITION_NAME_SIZE-1 );

    BufferBlock                 = NULL;
    ListOfMetaData              = NULL;

    AbortGetBuffer              = false;
    BufferReleaseSignalWaitedOn = false;

    CountOfReferencedBuffers	= 0;
    TotalAllocatedMemory	= 0;
    TotalUsedMemory		= 0;

    //
    // Record parameters
    //

    this->BufferDescriptor      = Descriptor;
    this->NumberOfBuffers       = NumberOfBuffers;
    this->Size                  = Size;

    if( MemoryPool != NULL )
    {
	this->MemoryPool[0]     = MemoryPool[0];
	this->MemoryPool[1]     = MemoryPool[1];
	this->MemoryPool[2]     = MemoryPool[2];
    }

    //
    // Shall we create the buffer class instances
    //

    if( NumberOfBuffers != NOT_SPECIFIED )
    {
	//
	// Get a ring to hold the free buffers
	//

	FreeBuffer      = new RingGeneric_c(NumberOfBuffers);
	if( (FreeBuffer == NULL) || (FreeBuffer->InitializationStatus != RingNoError) )
	{
	    report( severity_error, "BufferPool_Generic_c::BufferPool_Generic_c - Failed to create free buffer ring.\n" );
	    TidyUp();
	    return;
	}

	//
	// Can we allocate the memory for the buffers
	//

	if( Descriptor->AllocateOnPoolCreation )
	{
	    Status      = CheckMemoryParameters( Descriptor, true, Size, MemoryPool, ArrayOfMemoryBlocks, DeviceMemoryPartitionName, 
						 "BufferPool_Generic_c::BufferPool_Generic_c", &ItemSize );
	    if( Status != BufferNoError )
	    {
		TidyUp();
		return;
	    }

	    //
	    // Create a buffer block descriptor record
	    //

	    BufferBlock                         = new struct BlockDescriptor_s;
	    if( BufferBlock == NULL )
	    {
		report( severity_error, "BufferPool_Generic_c::BufferPool_Generic_c - Failed to allocate block descriptor.\n" );
		TidyUp();
		return;
	    }

	    BufferBlock->Descriptor                     = Descriptor;
	    BufferBlock->AttachedToPool                 = true;
	    BufferBlock->Size                           = ItemSize * NumberOfBuffers;
	    BufferBlock->MemoryAllocatorDevice          = ALLOCATOR_INVALID_DEVICE;

	    Status      = AllocateMemoryBlock( BufferBlock, true, 0, NULL, MemoryPool, ArrayOfMemoryBlocks, DeviceMemoryPartitionName, 
						"BufferPool_Generic_c::BufferPool_Generic_c" );
	    if( Status != BufferNoError )
	    {
		TidyUp();
		return;
	    }
	}

	//
	// Now create the buffers
	//

	for( i=0; i<NumberOfBuffers; i++ )
	{
	    Buffer              = new Buffer_Generic_c( Manager, this, Descriptor );
	    if( (Buffer == NULL) || (Buffer->InitializationStatus != BufferNoError) )
	    {
		InitializationStatus            = BufferInsufficientMemoryForBuffer;
		if( Buffer != NULL )
		{
		    InitializationStatus        = Buffer->InitializationStatus;
		    delete Buffer;
		}

		report( severity_error, "BufferPool_Generic_c::BufferPool_Generic_c - Failed to create buffer (%08x)\n", InitializationStatus );
		TidyUp();
		return;
	    }

	    Buffer->Next        = ListOfBuffers;
	    Buffer->Index       = i;
	    ListOfBuffers       = Buffer;

	    FreeBuffer->Insert( (unsigned int)Buffer );

	    //
	    // Have we allocated the buffer data block
	    //

	    if( Descriptor->AllocateOnPoolCreation )
	    {
		Buffer->DataSize                                = 0;
		Buffer->BufferBlock->AttachedToPool             = true;
		Buffer->BufferBlock->Size                       = ItemSize;
		Buffer->BufferBlock->MemoryAllocatorDevice      = ALLOCATOR_INVALID_DEVICE;
		Buffer->BufferBlock->Address[CachedAddress]     = NULL;
		Buffer->BufferBlock->Address[UnCachedAddress]   = NULL;
		Buffer->BufferBlock->Address[PhysicalAddress]   = NULL;

		if( Descriptor->AllocationSource == AllocateIndividualSuppliedBlocks )
		{
		    for( j=0; j<3; j++ )
			Buffer->BufferBlock->Address[j]         = ArrayOfMemoryBlocks[i][j];
		}
		else
		{
		    for( j=0; j<3; j++ )
			if( BufferBlock->Address[j] != NULL )
			    Buffer->BufferBlock->Address[j]     = (unsigned char *)BufferBlock->Address[j] + (i * ItemSize);
		}
	    }
	}
    }

    //
    // If we have pool memory, and we have not used it already, then we need to initialize the allocation mechanism.
    //

    if( (MemoryPool != NULL) && !Descriptor->AllocateOnPoolCreation )
    {
#if 0
	MemoryPoolAllocator     = new AllocatorSimple_c( Size, Descriptor->AllocationUnitSize, (unsigned char *)MemoryPool[PhysicalAddress] );
#else
	MemoryPoolAllocator     = new AllocatorSimple_c( Size, 1, (unsigned char *)MemoryPool[PhysicalAddress] );
#endif
	if( (MemoryPoolAllocator == NULL) || (MemoryPoolAllocator->InitializationStatus != AllocatorNoError) )
	{
	    report( severity_error, "BufferPool_Generic_c::BufferPool_Generic_c - Failed to initialize MemoryPool allocator\n" );
	    TidyUp();
	    return;
	}
    }

//

    InitializationStatus        = BufferNoError;
}


// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Destructor function - close down
//

BufferPool_Generic_c::~BufferPool_Generic_c( void )
{
    TidyUp();

    if( ReferenceCount != 0 )
	report( severity_error, "BufferPool_Generic_c::~BufferPool_Generic_c - Destroying pool of type '%s', Final reference count = %d\n",
			(BufferDescriptor->TypeName == NULL) ? "Unnamed" : BufferDescriptor->TypeName,
			ReferenceCount );
}


// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Attach meta data structures to every element of the pool
//

BufferStatus_t   BufferPool_Generic_c::AttachMetaData(
						MetaDataType_t    Type,
						unsigned int      Size,
						void             *MemoryPool,
						void             *ArrayOfMemoryBlocks[],
						char		 *DeviceMemoryPartitionName )
{
BufferStatus_t           Status;
unsigned int             ItemSize;
BufferDataDescriptor_t  *Descriptor;
BlockDescriptor_t        Block;
BlockDescriptor_t        SubBlock;
Buffer_Generic_t         Buffer;

    //
    // Check to see if it is already attached
    //

    for( Block	 = ListOfMetaData;
	 Block	!= NULL;
	 Block	 = Block->Next )
	if( Block->Descriptor->Type == Type )
	{
	    report( severity_info, "BufferPool_Generic_c::AttachMetaData - Meta data already attached.\n" );
	    return BufferNoError;
	}

    //
    // Get the descriptor
    //

    Status      = Manager->GetDescriptor( Type, MetaDataTypeBase, &Descriptor );
    if( Status != BufferNoError )
	return Status;

    //
    // Check the parameters and associated information to see if we can do this
    //

    Status      = CheckMemoryParameters( Descriptor, true, Size, MemoryPool, ArrayOfMemoryBlocks, DeviceMemoryPartitionName,
						 "BufferPool_Generic_c::AttachMetaData", &ItemSize );
    if( Status != BufferNoError )
	return Status;

    //
    // Create a new block descriptor record
    //

    Block       = new struct BlockDescriptor_s;
    if( Block == NULL )
    {
	report( severity_error, "BufferPool_Generic_c::AttachMetaData - Unable to create a block descriptor record.\n" );
	return BufferInsufficientMemoryForMetaData;
    }

    Block->Descriptor                   = Descriptor;
    Block->AttachedToPool               = true;
    Block->Size                         = ItemSize * NumberOfBuffers;
    Block->MemoryAllocatorDevice        = ALLOCATOR_INVALID_DEVICE;

    Status      = AllocateMemoryBlock( Block, true, 0, NULL, MemoryPool, ArrayOfMemoryBlocks, DeviceMemoryPartitionName, 
					"BufferPool_Generic_c::AttachMetaData" );
    if( Status != BufferNoError )
    {
	delete Block;
	return Status;
    }

    OS_LockMutex( &Lock );
    Block->Next                         = ListOfMetaData;
    ListOfMetaData                      = Block;
    OS_UnLockMutex( &Lock );

    //
    // Now loop assigning values to each buffer
    //

    for( Buffer  = ListOfBuffers;
	 Buffer != NULL;
	 Buffer  = Buffer->Next )
    {
	SubBlock        = new struct BlockDescriptor_s;
	if( SubBlock == NULL )
	{
	    report( severity_error, "BufferPool_Generic_c::AttachMetaData - Unable to create a block descriptor record.\n" );
	    return BufferInsufficientMemoryForMetaData;
	}

	SubBlock->Descriptor                    = Descriptor;
	SubBlock->AttachedToPool                = true;
	SubBlock->Size                          = ItemSize;
	SubBlock->MemoryAllocatorDevice         = ALLOCATOR_INVALID_DEVICE;
	SubBlock->Address[CachedAddress]        = NULL;
	SubBlock->Address[UnCachedAddress]      = NULL;
	SubBlock->Address[PhysicalAddress]      = NULL;

	if( Descriptor->AllocationSource == AllocateIndividualSuppliedBlocks )
	    SubBlock->Address[CachedAddress]    = ArrayOfMemoryBlocks[Buffer->Index];
	else
	    SubBlock->Address[CachedAddress]    = (unsigned char *)Block->Address[CachedAddress] + (Buffer->Index * ItemSize);

	OS_LockMutex( &Lock );
	SubBlock->Next                          = Buffer->ListOfMetaData;
	Buffer->ListOfMetaData                  = SubBlock;
	OS_UnLockMutex( &Lock );
    }

//

    return BufferNoError;
}


// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Detach meta data from every element of the pool
//

BufferStatus_t   BufferPool_Generic_c::DetachMetaData(  
						MetaDataType_t    Type )
{
BlockDescriptor_t       *LocationOfBlockPointer;
BlockDescriptor_t        Block;
BlockDescriptor_t        SubBlock;
Buffer_Generic_t         Buffer;

    //
    // First find the descriptor block in the pool 
    //

    OS_LockMutex( &Lock );

    for( LocationOfBlockPointer   = &ListOfMetaData;
	 *LocationOfBlockPointer != NULL;
	 LocationOfBlockPointer   = &((*LocationOfBlockPointer)->Next) )
	if( (*LocationOfBlockPointer)->Descriptor->Type == Type )
	    break;

    if( *LocationOfBlockPointer == NULL )
    {
	report( severity_error, "BufferPool_Generic_c::DetachMetaData - No meta data of the specified type is attached to the buffer pool.\n" );
	OS_UnLockMutex( &Lock );
	return BufferMetaDataTypeNotFound;
    }

    //
    // Get a local block pointer, and unthread the block from the list
    //

    Block                       = *LocationOfBlockPointer;
    *LocationOfBlockPointer     = Block->Next;

    //
    // For each buffer, find a block describing this type, unthread it, and delete the block record
    //

    for( Buffer  = ListOfBuffers;
	 Buffer != NULL;
	 Buffer  = Buffer->Next )
    {
	if( Buffer->ReferenceCount != 0 )
	{
	    report( severity_note, "BufferPool_Generic_c::DetachMetaData - Detaching meta data (%s) from a live buffer (%s) - Implementation error.\n",
			Manager->TypeDescriptors[Type & TYPE_INDEX_MASK].TypeName, BufferDescriptor->TypeName );
#if 0
	    // Helpful dump and clean terminate for Nick
	    Dump( DumpAll );
	    Manager->Dump( DumpAll );
	    report( severity_fatal, "Aaaarrrrgggghhhhh !!!!!!!!!\n" );
#endif
	}

	for( LocationOfBlockPointer       = &Buffer->ListOfMetaData;
	     *LocationOfBlockPointer     != NULL;
	     LocationOfBlockPointer       = &((*LocationOfBlockPointer)->Next) )
	    if( (*LocationOfBlockPointer)->Descriptor->Type == Type )
		break;

	if( ((*LocationOfBlockPointer) == NULL) || !(*LocationOfBlockPointer)->AttachedToPool )
	{
	    report( severity_error, "BufferPool_Generic_c::DetachMetaData - Meta data record missing from buffer, system inconsistent.\n" );
	    continue;
	}

	SubBlock                = *LocationOfBlockPointer;
	*LocationOfBlockPointer = SubBlock->Next;

	delete SubBlock;
    }

    //
    // Free up the memory, and delete the block record.
    //

    DeAllocateMemoryBlock( Block );
    delete Block;

//

    OS_UnLockMutex( &Lock );
    return BufferNoError;
}


// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Obtain a buffer from the pool
//

BufferStatus_t   BufferPool_Generic_c::GetBuffer(
						Buffer_t         *Buffer,
						unsigned int      OwnerIdentifier,
						unsigned int      RequiredSize,
						bool              NonBlocking,
						bool		  RequiredSizeIsLowerBound )
{
unsigned int		 i;
BufferStatus_t		 Status;
RingStatus_t		 RingStatus;
unsigned int		 ItemSize;
Buffer_Generic_t	 LocalBuffer;
unsigned long long	 EntryTime;

    //
    // Initialize the input parameters, and clear the abort flag
    //

    *Buffer             = NULL;
    AbortGetBuffer      = false;

    //
    // Perform simple parameter checks.
    //

    if( !BufferDescriptor->AllocateOnPoolCreation && (BufferDescriptor->AllocationSource != NoAllocation) )
    {
	Status  = CheckMemoryParameters( BufferDescriptor, false, RequiredSize, MemoryPool, NULL, MemoryPartitionName, 
					"BufferPool_Generic_c::GetBuffer", &ItemSize );
	if( Status != BufferNoError )
	    return Status;
    }

    //
    // Get a buffer - two different paths depending on whether or not there are a fixed number of buffers
    //

    if( NumberOfBuffers == NOT_SPECIFIED )
    {
	LocalBuffer             = new Buffer_Generic_c( Manager, this, BufferDescriptor );
	if( (LocalBuffer == NULL) || (LocalBuffer->InitializationStatus != BufferNoError) )
	{
	    if( LocalBuffer != NULL )
		delete LocalBuffer;

	    report( severity_error, "BufferPool_Generic_c::GetBuffer - Failed to create buffer\n" );
	    return BufferFailedToCreateBuffer;
	}

	OS_LockMutex( &Lock );
	LocalBuffer->Next       = ListOfBuffers;
	LocalBuffer->Index      = CountOfBuffers++;
	ListOfBuffers           = LocalBuffer;
	OS_UnLockMutex( &Lock );
    }
    else
    {
	OS_LockMutex( &Lock );
	EntryTime		= OS_GetTimeInMicroSeconds();
	do
	{
	    OS_ResetEvent( &BufferReleaseSignal );

	    RingStatus	= FreeBuffer->Extract( (unsigned int *)(&LocalBuffer), RING_NONE_BLOCKING );

	    if( !NonBlocking && !AbortGetBuffer && (RingStatus != RingNoError) )
	    {
		BufferReleaseSignalWaitedOn     = true;
		OS_UnLockMutex( &Lock );

		OS_WaitForEvent( &BufferReleaseSignal, BUFFER_MAXIMUM_EVENT_WAIT );

		OS_LockMutex( &Lock );
		BufferReleaseSignalWaitedOn	= false;
	    }

	    if( (OS_GetTimeInMicroSeconds() - EntryTime) > BUFFER_MAX_EXPECTED_WAIT_PERIOD )
	    {
		report( severity_info, "BufferPool_Generic_c::GetBuffer - Waiting for a buffer of type %04x - '%s'\n", BufferDescriptor->Type, 
				(BufferDescriptor->TypeName == NULL) ? "Unnamed" : BufferDescriptor->TypeName );
		EntryTime	= OS_GetTimeInMicroSeconds();
	    }

	} while( !NonBlocking && !AbortGetBuffer && (RingStatus != RingNoError) );
	OS_UnLockMutex( &Lock );

	if( RingStatus != RingNoError )
	    return BufferNoFreeBufferAvailable;
    }

    //
    // Deal with the memory
    //

    if( !BufferDescriptor->AllocateOnPoolCreation && (BufferDescriptor->AllocationSource != NoAllocation) )
    {
	LocalBuffer->BufferBlock->AttachedToPool        = true;
	LocalBuffer->BufferBlock->Size                  = ItemSize;

	Status                                          = AllocateMemoryBlock( LocalBuffer->BufferBlock, false, 0, MemoryPoolAllocator,
										MemoryPool, NULL,  MemoryPartitionName, 
										"BufferPool_Generic_c::GetBuffer",
										RequiredSizeIsLowerBound );
	if( Status != BufferNoError )
	    return Status;

    }

    LocalBuffer->DataSize               = 0;

    //
    // Record the owner identifier
    //


    LocalBuffer->OwnerIdentifier[0]     = OwnerIdentifier;
    LocalBuffer->ReferenceCount         = 1;

    for( i=1; i<MAX_BUFFER_OWNER_IDENTIFIERS; i++ )
	LocalBuffer->OwnerIdentifier[i] = UNSPECIFIED_OWNER;

    //
    // Initalize Attached buffers
    //

    for( i=0; i<MAX_ATTACHED_BUFFERS; i++ )
	LocalBuffer->AttachedBuffers[i] = NULL;

    //
    // Increment the global reference count
    //

    OS_LockMutex( &Lock );
    ReferenceCount++;
    CountOfReferencedBuffers++;
    OS_UnLockMutex( &Lock );

    //
    // Set the return value
    //

    *Buffer     = LocalBuffer;

    return BufferNoError;
}


// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Abort any ongoing get buffer command
//

BufferStatus_t   BufferPool_Generic_c::AbortBlockingGetBuffer( void )
{
    AbortGetBuffer      = true;
    OS_SetEvent( &BufferReleaseSignal );

    return BufferNoError;
}


// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Release a buffer to the pool
//

BufferStatus_t   BufferPool_Generic_c::ReleaseBuffer(
						Buffer_t          Buffer )
{
unsigned int             i;
Buffer_Generic_t         LocalBuffer;
Buffer_Generic_t        *LocationOfBufferPointer;
BlockDescriptor_t       *LocationOfBlockPointer;
BlockDescriptor_t        Block;
PlayerEventRecord_t      ReleaseEvent;

//

    LocalBuffer         = (Buffer_Generic_t)Buffer;

    //
    // Do I want to signal this free ?
    //

    if( (EventMask & EventBufferRelease) != 0 )
    {
	ReleaseEvent.Code                       = EventBufferRelease;
	ReleaseEvent.Playback                   = Playback;
	ReleaseEvent.Stream                     = Stream;
	ReleaseEvent.PlaybackTime               = TIME_NOT_APPLICABLE;
	ReleaseEvent.Value[0].Pointer           = LocalBuffer->BufferBlock->Address[0];
	if( ReleaseEvent.Value[0].Pointer == NULL )
	    ReleaseEvent.Value[0].Pointer       = LocalBuffer->BufferBlock->Address[1];
	if( ReleaseEvent.Value[0].Pointer == NULL )
	    ReleaseEvent.Value[0].Pointer       = LocalBuffer->BufferBlock->Address[2];
	ReleaseEvent.Value[1].UnsignedInt       = LocalBuffer->BufferBlock->Size;
	ReleaseEvent.UserData                   = EventUserData;

	Player->SignalEvent( &ReleaseEvent );
    }

    //
    // Release any non-persistant meta data
    //

    LocationOfBlockPointer        = &LocalBuffer->ListOfMetaData;
    while( *LocationOfBlockPointer != NULL )
    {
	if( !((*LocationOfBlockPointer)->AttachedToPool) )
	{
	    //
	    // Unthread the meta data item block
	    //

	    Block                       = *LocationOfBlockPointer;
	    *LocationOfBlockPointer     = Block->Next;

	    DeAllocateMemoryBlock( Block );
	    delete Block;
	}
	else
	    LocationOfBlockPointer      = &((*LocationOfBlockPointer)->Next);
    }

    //
    // Release our hold on any attached buffers
    //

    OS_LockMutex( &LocalBuffer->Lock );
    for( i=0; i<MAX_ATTACHED_BUFFERS; i++ )
    {
	Buffer_t	Temporary	= LocalBuffer->AttachedBuffers[i];
	if( Temporary != NULL )
	{
	    LocalBuffer->AttachedBuffers[i]     = NULL;
	    Temporary->DecrementReferenceCount();
	}
    }
    OS_UnLockMutex( &LocalBuffer->Lock );

    //
    // If non-persistant delete the memory associated with the block
    //

    if( !BufferDescriptor->AllocateOnPoolCreation && (BufferDescriptor->AllocationSource != NoAllocation) )
	DeAllocateMemoryBlock( LocalBuffer->BufferBlock );

    OS_LockMutex( &Lock );
    TotalUsedMemory		-= LocalBuffer->DataSize;
    LocalBuffer->DataSize        = 0;

    //
    // If there are a fixed number of buffers insert this on the ring, 
    // else unthread from list and delete the buffer
    //

    if( NumberOfBuffers != NOT_SPECIFIED )
    {
	FreeBuffer->Insert( (unsigned int)LocalBuffer );
	OS_SetEvent( &BufferReleaseSignal );
    }
    else
    {
	for( LocationOfBufferPointer     = &ListOfBuffers;
	     *LocationOfBufferPointer   != NULL;
	     LocationOfBufferPointer     = &((*LocationOfBufferPointer)->Next) )
	    if( (*LocationOfBufferPointer) == LocalBuffer )
		break;

	if( *LocationOfBufferPointer == NULL )
	{
	    report( severity_error, "BufferPool_Generic_c::ReleaseBuffer - Buffer not found in list, internal consistency error.\n" );
	    return BufferError;
	}

	*LocationOfBufferPointer        = LocalBuffer->Next;

	delete LocalBuffer;
	CountOfBuffers--;
    }

    CountOfReferencedBuffers--;
    OS_UnLockMutex( &Lock );

//

    return BufferNoError;
}


// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Discover the type of buffer supported by the pool
//

BufferStatus_t   BufferPool_Generic_c::GetType( BufferType_t     *Type )
{
    *Type       = BufferDescriptor->Type;
    return BufferNoError;
}


// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Discover usage statistics about the pool
//

BufferStatus_t   BufferPool_Generic_c::GetPoolUsage(
						unsigned int     *BuffersInPool,
						unsigned int     *BuffersWithNonZeroReferenceCount,
						unsigned int     *MemoryInPool,
						unsigned int     *MemoryAllocated,
						unsigned int     *MemoryInUse,
						unsigned int	 *LargestFreeMemoryBlock )
{
unsigned int            TotalMemory;
unsigned int            LargestFreeBlock;

//

    switch( BufferDescriptor->AllocationSource )
    {
	case AllocateFromSuppliedBlock:
			TotalMemory		= Size;
			if( MemoryPoolAllocator == NULL )
			    LargestFreeBlock	= Size;
			else
			    MemoryPoolAllocator->LargestFreeBlock( &LargestFreeBlock );
			break;

	case AllocateIndividualSuppliedBlocks:
			TotalMemory		= Size * NumberOfBuffers;
			LargestFreeBlock	= (((NumberOfBuffers == NOT_SPECIFIED) ? CountOfBuffers : NumberOfBuffers) 
							!= CountOfReferencedBuffers) ? Size : 0;
			break;

	case NoAllocation:
	case AllocateFromOSMemory:
	case AllocateFromNamedDeviceMemory:
	case AllocateFromDeviceVideoLMIMemory:
	case AllocateFromDeviceMemory:
	default:
			TotalMemory		= NOT_SPECIFIED;
			LargestFreeBlock	= NOT_SPECIFIED;
			break;
    }

//

    if( BuffersInPool != NULL )
	*BuffersInPool  	= (NumberOfBuffers == NOT_SPECIFIED) ? CountOfBuffers : NumberOfBuffers;

    if( BuffersWithNonZeroReferenceCount != NULL )
	*BuffersWithNonZeroReferenceCount       = CountOfReferencedBuffers;

    if( MemoryInPool != NULL )
	*MemoryInPool   	= TotalMemory;

    if( MemoryAllocated != NULL )
	*MemoryAllocated	= TotalAllocatedMemory;

    if( MemoryInUse != NULL )
	*MemoryInUse    	= TotalUsedMemory;

    if( LargestFreeMemoryBlock != NULL )
	*LargestFreeMemoryBlock	= LargestFreeBlock;

//

    return BufferNoError;
}


// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Discover how many buffers are owned by a particular state
//

BufferStatus_t   BufferPool_Generic_c::CountBuffersReferencedBy( 
						unsigned int      OwnerIdentifier,
						unsigned int     *Count )
{
unsigned int            i;
unsigned int            ReferenceCount;
Buffer_Generic_t        Buffer;

//

    ReferenceCount      = 0;

//

    for( Buffer  = ListOfBuffers;
	 Buffer != NULL;
	 Buffer  = Buffer->Next )
    {
	for( i=0; i<MAX_BUFFER_OWNER_IDENTIFIERS; i++ )
	    if( Buffer->OwnerIdentifier[i] == OwnerIdentifier )
	    {
		ReferenceCount++;
		break;
	    }
    }

//

    *Count      = ReferenceCount;

    return BufferNoError;
}


// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Obtain a reference to all buffers that are currently allocated (a debug/reporting aid)
//

BufferStatus_t   BufferPool_Generic_c::GetAllUsedBuffers(
						unsigned int      ArraySize,
						Buffer_t         *ArrayOfBuffers,
						unsigned int      OwnerIdentifier )
{
unsigned int            Count;
Buffer_Generic_t        Buffer;

//

    Count       = 0;

    for( Buffer  = ListOfBuffers;
	 Buffer != NULL;
	 Buffer  = Buffer->Next )
	if( Buffer->ReferenceCount != 0 )
	{
	    if( Count >= ArraySize )
	    {
		report( severity_error, "BufferPool_Generic_c::GetAllUsedBuffers - Too many used buffers for output array.\n" );
		return BufferError;
	    }

	    ArrayOfBuffers[Count++]     = Buffer;
	}

//

    return BufferNoError;
}


// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Dump the status of the pool
//

void   BufferPool_Generic_c::Dump(              unsigned int      Flags )
{
unsigned int            BuffersWithNonZeroReferenceCount;
unsigned int            MemoryInPool;
unsigned int            MemoryAllocated;
unsigned int            MemoryInUse;
BlockDescriptor_t       MetaData;
Buffer_Generic_t        Buffer;

//

    if( (Flags & DumpPoolStates) != 0 )
    {
	report( severity_info, "    Pool of type %04x - '%s'\n", BufferDescriptor->Type, 
			(BufferDescriptor->TypeName == NULL) ? "Unnamed" : BufferDescriptor->TypeName );

//

	GetPoolUsage( NULL, &BuffersWithNonZeroReferenceCount, &MemoryInPool, &MemoryAllocated, &MemoryInUse );

	if( NumberOfBuffers == NOT_SPECIFIED )
	    report( severity_info, "\tDynamic buffer allocation - currently there are %d buffers.\n", CountOfBuffers );
	else
	    report( severity_info, "\tFixed buffer allocation   - %d buffers, of which %d are allocated.\n", NumberOfBuffers, BuffersWithNonZeroReferenceCount );

	report( severity_info, "\tThere is %08x memory available to the pool (0 => Unlimited), %08x is allocated, and %08x is actually used.\n", MemoryInPool, MemoryAllocated, MemoryInUse );

//

	report( severity_info, "\t%s attached to every element of pool.\n", 
		(ListOfMetaData == NULL) ? "There are no Meta data items" : "The following meta data items are" );

	for( MetaData  = ListOfMetaData;
	     MetaData != NULL;
	     MetaData  = MetaData->Next )
	    report( severity_info, "\t    %04x - %s\n", MetaData->Descriptor->Type, 
			(MetaData->Descriptor->TypeName == NULL) ? "Unnamed" : MetaData->Descriptor->TypeName );
    }

//

    if( (Flags & DumpBufferStates) != 0 )
    {
	report( severity_info, "    Dump of Buffers\n" );

//      OS_LockMutex( &Lock );

	for( Buffer  = ListOfBuffers;
	     Buffer != NULL;
	     Buffer  = Buffer->Next )
	{
	    Buffer->Dump( Flags );
	}

//      OS_UnLockMutex( &Lock );
    }
}


// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Private - Tidy up all data function 
//

void   BufferPool_Generic_c::TidyUp( void )
{
Buffer_Generic_t        Buffer;

//
    //
    // Ensure no-one is waiting on anything.
    //

    if( BufferReleaseSignalWaitedOn )
	AbortBlockingGetBuffer();

    //
    // detach any globally attached meta data
    //

    while( ListOfMetaData != NULL )
	DetachMetaData( ListOfMetaData->Descriptor->Type );

    //
    // Clean up the buffers
    //

    if( NumberOfBuffers != NOT_SPECIFIED )
    {
	for( Buffer  = ListOfBuffers;
	     Buffer != NULL;
	     Buffer  = Buffer->Next )
	    if( Buffer->ReferenceCount != 0 )
		FreeUpABuffer( Buffer );
    }
    else
    {
	while( ListOfBuffers != NULL )
	    FreeUpABuffer( ListOfBuffers );
    }

    while( ListOfBuffers != NULL )
    {
	Buffer          = ListOfBuffers;
	ListOfBuffers   = Buffer->Next;

	delete Buffer;
    }

    //
    // delete the ring holding free buffers
    //

    if( FreeBuffer != NULL )
    {
	delete FreeBuffer;
	FreeBuffer              = NULL;
    }

    //
    // Delete any created allocator for the memory pool
    //

    if( MemoryPoolAllocator != NULL )
    {
	delete MemoryPoolAllocator;
	MemoryPoolAllocator     = NULL;
    }

    //
    // Delete any global memory allocation for the buffers
    //

    if( BufferBlock != NULL )
    {
	DeAllocateMemoryBlock( BufferBlock );
	delete BufferBlock;
	BufferBlock     = NULL;
    }

    //
    // Free up the OS entities 
    //

    OS_TerminateEvent( &BufferReleaseSignal );
    OS_TerminateMutex( &Lock );
}


// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//
//      Private - Function to free up a buffer
//

void   BufferPool_Generic_c::FreeUpABuffer( Buffer_Generic_t	Buffer )
{
int			i;
unsigned int		References;
BufferPool_Generic_t	PoolSearch;
Buffer_Generic_t	BufferSearch;

    //
    // Setup the maximum number of references to remove, note buffer may cease to exist, 
    // during a release, so we maintain a local count.
    //

    References	= Buffer->ReferenceCount;

    //
    // Attempt to see if this buffer is attached to any other buffers
    //

    for( PoolSearch  = Manager->ListOfBufferPools;
	 (PoolSearch != NULL) && (References != 0);
	 PoolSearch  = PoolSearch->Next )
    {
	for( BufferSearch  = PoolSearch->ListOfBuffers;
	     (BufferSearch != NULL) && (References != 0);
	     BufferSearch  = BufferSearch->Next )
	{
	    for( i=0; ((i<MAX_ATTACHED_BUFFERS) && (References != 0)); i++ )
	    {
		if( BufferSearch->AttachedBuffers[i] == Buffer )
		{
		    BufferSearch->DetachBuffer( Buffer );
		    References--;
		}
	    }
	}
    }

#if 0
    //
    // Now just do standard release on any other references
    // NOTE there should not be any of these, I have commented this code out
    // to encourage cleanliness rather than fixing up.
    //

    for( ;
         References != 0;
	 References-- )
	ReleaseBuffer( Buffer );
#endif
}


// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//
//      Private - Function to check that memory parameters are appropriate for allocating data
//

BufferStatus_t   BufferPool_Generic_c::CheckMemoryParameters(
						BufferDataDescriptor_t   *Descriptor,
						bool                      ArrayAllocate,
						unsigned int              Size,
						void                     *MemoryPool,
						void                     *ArrayOfMemoryBlocks,
						char			 *MemoryPartitionName,
						const char               *Caller,
						unsigned int             *ItemSize )
{
unsigned int    RequiredSize;
unsigned int    PoolSize;

    //
    // Calculate locals
    //

    PoolSize            = Size;

    if( MemoryPool != NULL )
	RequiredSize    = Descriptor->HasFixedSize ? Descriptor->FixedSize : (ArrayAllocate ? (PoolSize / NumberOfBuffers) : PoolSize);
    else
	RequiredSize    = (Size != NOT_SPECIFIED) ? Size : (Descriptor->HasFixedSize ? Descriptor->FixedSize : NOT_SPECIFIED);

    //
    // Perform sizing check
    //

    if( RequiredSize == NOT_SPECIFIED )
    {
	report( severity_error, "%s - Size not specified.\n", Caller );
	return BufferError;
    }
    else if( Descriptor->HasFixedSize && (RequiredSize != Descriptor->FixedSize) )
    {
	report( severity_error, "%s - Size does not match FixedSize in descriptor (%04x %04x).\n", Caller, RequiredSize, Descriptor->FixedSize );
	return BufferError;
    }

    //
    // Perform those checks specific to array allocation
    //

    if( ArrayAllocate )
    {
	if( NumberOfBuffers == NOT_SPECIFIED )
	{
	    report( severity_error, "%s - Unknown number of buffers.\n", Caller );
	    return BufferError;
	}
    }
    else
    {
	if( ArrayOfMemoryBlocks != NULL )
	{
	    report( severity_error, "%s - Attempt to use ArrayOfMemoryBlocks for single memory allocation.\n", Caller );
	    return BufferError;
	}
    }

    //
    // Now generic parameter checking
    //

    switch( Descriptor->AllocationSource )
    {
	case    NoAllocation:
			report( severity_error, "%s - No allocation allowed.\n", Caller );
			return BufferError;
			break;


	case    AllocateFromNamedDeviceMemory:
			if( (MemoryPartitionName == NULL) ||
			    (MemoryPartitionName[0] == '\0') )
			{
			    report( severity_error, "%s - Parameters incompatible with buffer allocation source (No partition specified).\n", Caller );
			    return BufferError;
			}
			// Fall through

	case    AllocateFromOSMemory:
	case    AllocateFromDeviceMemory:
	case    AllocateFromDeviceVideoLMIMemory:
			if( (MemoryPool          != NULL)               ||
			    (ArrayOfMemoryBlocks != NULL) )
			{
			    report( severity_error, "%s - Parameters incompatible with buffer allocation source.\n", Caller );
			    return BufferError;
			}
			break;

	case    AllocateFromSuppliedBlock:
			if( (PoolSize           == NOT_SPECIFIED)       ||
			    (MemoryPool         == NULL) )
			{
			    report( severity_error, "%s - Memory block not supplied.\n", Caller );
			    return BufferError;
			}
			break;

	case    AllocateIndividualSuppliedBlocks:
			if( ArrayOfMemoryBlocks == NULL )
			{
			    report( severity_error, "%s - Individual blocks of memory not supplied.\n", Caller );
			    return BufferError;
			}
			break;
    }

//

    if( ItemSize != NULL )
	*ItemSize       = RequiredSize;

    return BufferNoError;
}


// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Private - Function to allocate a block of memory
//

BufferStatus_t   BufferPool_Generic_c::AllocateMemoryBlock(
						BlockDescriptor_t         Block,
						bool                      ArrayAllocate,
						unsigned int              Index,
						Allocator_c              *PoolAllocator,
						void                     *MemoryPool,
						void                     *ArrayOfMemoryBlocks,
						char			 *MemoryPartitionName,
						const char               *Caller,
						bool			  RequiredSizeIsLowerBound )
{
unsigned int              i;
BufferDataDescriptor_t   *Descriptor;
AllocatorStatus_t         Status;
allocator_status_t        AStatus;
const char		 *Partition	= NULL;

//

    Block->Address[CachedAddress]       = NULL;
    Block->Address[UnCachedAddress]     = NULL;
    Block->Address[PhysicalAddress]     = NULL;
    Descriptor                          = Block->Descriptor;

//

    if( Block->Size == 0 )
	return BufferNoError;

//

    switch( Descriptor->AllocationSource )
    {
	case AllocateFromOSMemory:
			Block->Address[CachedAddress]   = new unsigned char[Block->Size];
			if( Block->Address[CachedAddress] == NULL )
			{
			    report( severity_error, "%s - Failed to malloc memory\n", Caller );
			    return BufferInsufficientMemoryGeneral;
			}
			break;

	case AllocateFromNamedDeviceMemory:
			Partition		= MemoryPartitionName;
	case AllocateFromDeviceVideoLMIMemory:
			if( Descriptor->AllocationSource == AllocateFromDeviceVideoLMIMemory )
			    Partition		= VID_LMI_PARTITION;
	case AllocateFromDeviceMemory:
			if( Descriptor->AllocationSource == AllocateFromDeviceMemory )
			    Partition		= SYS_LMI_PARTITION;

			AStatus                 = AllocatorOpenEx( &Block->MemoryAllocatorDevice, Block->Size, true, Partition );
			if( AStatus != allocator_ok )
			{
			    report( severity_error, "%s - Failed to allocate memory\n", Caller );
			    return BufferInsufficientMemoryGeneral;
			}
			Block->Address[CachedAddress]   = AllocatorUserAddress( Block->MemoryAllocatorDevice );
			Block->Address[UnCachedAddress] = AllocatorUncachedUserAddress( Block->MemoryAllocatorDevice );
			Block->Address[PhysicalAddress] = AllocatorPhysicalAddress( Block->MemoryAllocatorDevice );
			break;

	case AllocateFromSuppliedBlock:
			Block->PoolAllocatedOffset      = 0;

			if( !ArrayAllocate && (PoolAllocator != NULL) )
			{
			    Block->Size         = (((Block->Size + Descriptor->AllocationUnitSize - 1) / Descriptor->AllocationUnitSize) * Descriptor->AllocationUnitSize);

			    if( RequiredSizeIsLowerBound )
				Status		= PoolAllocator->AllocateLargest( &Block->Size, (unsigned char **)&Block->PoolAllocatedOffset );
			    else
				Status		= PoolAllocator->Allocate( Block->Size, (unsigned char **)&Block->PoolAllocatedOffset );

			    if( Status != AllocatorNoError )
			    {
				report( severity_error, "%s - Failed to allocate memory from pool\n", Caller );
				return BufferInsufficientMemoryGeneral;
			    }
			}

			if( (Descriptor->Type & TYPE_TYPE_MASK) == MetaDataTypeBase )
			    Block->Address[CachedAddress]       = (unsigned char *)MemoryPool + Block->PoolAllocatedOffset;
			else
			{
			    for( i=0; i<3; i++ )
				if( ((unsigned char **)MemoryPool)[i] != NULL )
				    Block->Address[i]   = ((unsigned char **)MemoryPool)[i] + Block->PoolAllocatedOffset;
			}
			break;

	case AllocateIndividualSuppliedBlocks:
			break;          // Individual blocks are done externally

	default:        break;          // No action
    }

    //
    // If this is meta data we are allocating, then clear the memory.
    //

    if( (Descriptor->Type & TYPE_TYPE_MASK) == MetaDataTypeBase )
	memset( Block->Address[CachedAddress], 0x00, Block->Size );

    //
    // If this is buffer memory then update the total allocation record.
    //

    if( (Descriptor->Type & TYPE_TYPE_MASK) == BufferDataTypeBase )
	TotalAllocatedMemory	+= Block->Size;

//

    return BufferNoError;
}


// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Private - Function to allocate a block of memory
//

BufferStatus_t   BufferPool_Generic_c::DeAllocateMemoryBlock(
						BlockDescriptor_t         Block )
{
BufferDataDescriptor_t   *Descriptor;

//

    if( Block->Size == 0 )
	return BufferNoError;

//

    Descriptor          = Block->Descriptor;

    switch( Descriptor->AllocationSource )
    {
	case AllocateFromOSMemory:
			delete (unsigned char *)Block->Address[CachedAddress];
			break;

	case AllocateFromNamedDeviceMemory:
	case AllocateFromDeviceVideoLMIMemory:
	case AllocateFromDeviceMemory:
			AllocatorClose( Block->MemoryAllocatorDevice );
			break;

	case AllocateFromSuppliedBlock:
			if( MemoryPoolAllocator != NULL )
			    MemoryPoolAllocator->Free( Block->Size, (unsigned char*)Block->PoolAllocatedOffset );
			break;

	default:        break;          // No action
    }

    //
    // If this is buffer memory then update the total allocation record.
    //

    if( (Descriptor->Type & TYPE_TYPE_MASK) == BufferDataTypeBase )
	TotalAllocatedMemory	-= Block->Size;

//

    Block->Address[CachedAddress]       = NULL;
    Block->Address[UnCachedAddress]     = NULL;
    Block->Address[PhysicalAddress]     = NULL;
    Block->Size                         = 0;
    Block->MemoryAllocatorDevice        = ALLOCATOR_INVALID_DEVICE;

//

    return BufferNoError;
}


// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Private - Function to allocate a block of memory
//

BufferStatus_t   BufferPool_Generic_c::ShrinkMemoryBlock( 
						BlockDescriptor_t         Block,
						unsigned int              NewSize )
{
BufferDataDescriptor_t   *Descriptor;

//

    Descriptor          = Block->Descriptor;

    //
    // Check that this mechanism has memory allocated on a per buffer basis
    //

    if( Descriptor->AllocateOnPoolCreation )
    {
	report( severity_error, "BufferPool_Generic_c::ShrinkMemoryBlock - Attempt to shrink a buffer allocated on pool creation.\n" );
	return BufferError;
    }

    //
    // Check that shrinkage is appropriate.
    //

    if( NewSize >= Block->Size )
    {
	report( severity_error, "BufferPool_Generic_c::ShrinkMemoryBlock - Attempt to shrink a buffer to the same, or a larger size.\n\t\tThis must be some new usage of the word 'shrink' that I was previously unaware of.\n" );
	return BufferError;
    }

    //
    // Only specific allocation types can be shrunk.
    //

    switch( Descriptor->AllocationSource )
    {
	case NoAllocation:
			return BufferError;
			break;

	case AllocateFromOSMemory:
	case AllocateFromNamedDeviceMemory:
	case AllocateFromDeviceVideoLMIMemory:
	case AllocateFromDeviceMemory:
	case AllocateIndividualSuppliedBlocks:
			break;

	case AllocateFromSuppliedBlock:
			// First round up to the nearest allocation unit size
			NewSize         = (((NewSize + Descriptor->AllocationUnitSize - 1) / Descriptor->AllocationUnitSize) * Descriptor->AllocationUnitSize);

			if( NewSize < Block->Size )
			    MemoryPoolAllocator->Free( Block->Size-NewSize, (unsigned char*)Block->PoolAllocatedOffset+NewSize );

			Block->Size     = NewSize;
			break;
    }

//

    return BufferNoError;
}


// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Private - Function to allocate a block of memory
//

BufferStatus_t   BufferPool_Generic_c::ExtendMemoryBlock( 
						BlockDescriptor_t	  Block,
						unsigned int		 *NewSize,
						bool			  ExtendUpwards )
{
unsigned int		  i;
AllocatorStatus_t	  Status;
BufferDataDescriptor_t	 *Descriptor;

    //
    // Initialize newsize consistently
    //

    if( NewSize != NULL )
	*NewSize	= Block->Size;

    //
    // Check that this mechanism has memory allocated on a per buffer basis
    //

    Descriptor		= Block->Descriptor;
    if( Descriptor->AllocateOnPoolCreation )
    {
	report( severity_error, "BufferPool_Generic_c::ExpandMemoryBlock - Attempt to expand a buffer allocated on pool creation.\n" );
	return BufferError;
    }

    //
    // Only specific allocation types can be extended.
    //

    switch( Descriptor->AllocationSource )
    {
	case NoAllocation:
	case AllocateIndividualSuppliedBlocks:
	case AllocateFromOSMemory:
	case AllocateFromNamedDeviceMemory:
	case AllocateFromDeviceVideoLMIMemory:
	case AllocateFromDeviceMemory:
			return BufferError;
			break;

	case AllocateFromSuppliedBlock:

			//
			// Attempt extension
			//

			Status	= MemoryPoolAllocator->ExtendToLargest( &Block->Size, (unsigned char **)&Block->PoolAllocatedOffset, ExtendUpwards );
			if( Status != AllocatorNoError )
			    return BufferError;

			for( i=0; i<3; i++ )
			    if( ((unsigned char **)MemoryPool)[i] != NULL )
				Block->Address[i]   = ((unsigned char **)MemoryPool)[i] + Block->PoolAllocatedOffset;

			if( NewSize != NULL )
			    *NewSize	= Block->Size;

			break;
    }

//

    return BufferNoError;
}

