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

Source file name : buffer_individual_generic.cpp
Author :           Nick

Implementation of the buffer generic class for use in player 2


Date        Modification                                    Name
----        ------------                                    --------
26-Oct-06   Created                                         Nick

************************************************************************/

#include "osdev_device.h"
#include "osinline.h"

#include "buffer_generic.h"
#include "st_relay.h"
#include "player_generic.h"


// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Useful defines/macros that need not be user visible
//

#define AssertNonZeroReferenceCount( s )		do { if( ReferenceCount == 0 ) {this->Dump(); report( severity_fatal, "%s - Attempt to act on a buffer that has a reference count of zero.\n", s ); Manager->Dump();}/*return BufferError;*/ } while(0)

// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Contructor function - Initialize our data
//

Buffer_Generic_c::Buffer_Generic_c( 	BufferManager_Generic_t	  Manager,
					BufferPool_Generic_t	  Pool,
					BufferDataDescriptor_t	 *Descriptor )
{
//

    InitializationStatus	= BufferError;

    //
    // Initialize class data
    //

    this->Manager		= Manager;
    this->Pool			= Pool;

    Next			= NULL;
    ReferenceCount		= 0;
    BufferBlock			= NULL;
    ListOfMetaData		= 0;
    DataSize			= 0;

    memset( OwnerIdentifier, 0x00, MAX_BUFFER_OWNER_IDENTIFIERS * sizeof(unsigned int) );
    memset( AttachedBuffers, 0x00, MAX_ATTACHED_BUFFERS * sizeof(Buffer_t) );

    OS_InitializeMutex( &Lock );

    //
    // Get a memory block descriptor, and initialize it
    //

    BufferBlock	= new struct BlockDescriptor_s;
    if( BufferBlock == NULL )
    {
        report( severity_error, "Buffer_Generic_c::Buffer_Generic_c - Unable to create a block descriptor record.\n" );
        return;
    }

    BufferBlock->Next				= NULL;
    BufferBlock->Descriptor			= Descriptor;
    BufferBlock->AttachedToPool			= false;
    BufferBlock->Size				= 0;
    BufferBlock->Address[CachedAddress]		= NULL;
    BufferBlock->Address[UnCachedAddress]	= NULL;
    BufferBlock->Address[PhysicalAddress]	= NULL;
    BufferBlock->MemoryAllocatorDevice		= ALLOCATOR_INVALID_DEVICE;

//

    InitializationStatus	= BufferNoError;
}


// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Destructor function - close down
//

Buffer_Generic_c::~Buffer_Generic_c( void )
{
    if (BufferBlock != NULL)
    {
        delete BufferBlock;
	BufferBlock = NULL;
    }
    else
	report(severity_error,"Maybe not such a bug!\n");	
    OS_TerminateMutex( &Lock );
}


// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//	Attach meta data structure
//

BufferStatus_t   Buffer_Generic_c::AttachMetaData(
						MetaDataType_t	  Type,
						unsigned int	  Size,
						void		 *MemoryBlock,
						char		 *DeviceMemoryPartitionName )
{
BufferStatus_t		 Status;
BufferDataDescriptor_t	*Descriptor;
unsigned int		 ItemSize;
BlockDescriptor_t	 Block;

    //
    // Get the descriptor
    //

    AssertNonZeroReferenceCount( "Buffer_Generic_c::AttachMetaData" );

    Status      = Manager->GetDescriptor( Type, MetaDataTypeBase, &Descriptor );
    if( Status != BufferNoError )
        return Status;

    //
    // Check the parameters and associated information to see if we can do this
    //

    if( Descriptor->AllocationSource != NoAllocation )
    {
	Status	= Pool->CheckMemoryParameters( Descriptor, false, Size, MemoryBlock, NULL, DeviceMemoryPartitionName,
						 "Buffer_Generic_c::AttachMetaData", &ItemSize );
	if( Status != BufferNoError )
	    return Status;
    }
    else if( MemoryBlock == NULL )
    {
	report( severity_error, "Buffer_Generic_c::AttachMetaData - Pointer not supplied for NoAllocation meta data.\n" );
	return BufferParametersIncompatibleWithAllocationSource;
    }

    //
    // Create a new block descriptor record
    //

    Block	= new struct BlockDescriptor_s;
    if( Block == NULL )
    {
        report( severity_error, "Buffer_Generic_c::AttachMetaData - Unable to create a block descriptor record.\n" );
        return BufferInsufficientMemoryForMetaData;
    }

    Block->Descriptor			= Descriptor;
    Block->AttachedToPool		= false;
    Block->Size				= ItemSize;

    OS_LockMutex( &Lock );
    Block->Next				= ListOfMetaData;
    ListOfMetaData			= Block;
    OS_UnLockMutex( &Lock );

    //
    // Associate the memory
    //

    if( Descriptor->AllocationSource != NoAllocation )
    {
	Status	= Pool->AllocateMemoryBlock( Block, false, 0, NULL, MemoryBlock, NULL, DeviceMemoryPartitionName,
					"Buffer_Generic_c::AttachMetaData" );
	if( Status != BufferNoError )
	    return Status;
    }
    else
    {
    	Block->Size			= Size;
	Block->Address[CachedAddress]	= MemoryBlock;
    }

//

    return BufferNoError;
}


// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//	Detach meta data
//

BufferStatus_t   Buffer_Generic_c::DetachMetaData(	
						MetaDataType_t	  Type )
{
BlockDescriptor_t	*LocationOfBlockPointer;
BlockDescriptor_t	 Block;

    //
    // First find the descriptor block in the buffer
    //

    AssertNonZeroReferenceCount( "Buffer_Generic_c::DetachMetaData" );

    OS_LockMutex( &Lock );
    
    for( LocationOfBlockPointer	  = &ListOfMetaData;
	 *LocationOfBlockPointer != NULL;
	 LocationOfBlockPointer	  = &((*LocationOfBlockPointer)->Next) )
	if( ((*LocationOfBlockPointer)->Descriptor->Type == Type) && !((*LocationOfBlockPointer)->AttachedToPool) )
	    break;

    if( *LocationOfBlockPointer == NULL )
    {
	report( severity_error, "Buffer_Generic_c::DetachMetaData - No meta data of the specified type is attached to the buffer.\n" );
	OS_UnLockMutex( &Lock );
	return BufferMetaDataTypeNotFound;
    }

    //
    // Get a local block pointer, and unthread the block from the list
    //

    Block			= *LocationOfBlockPointer;
    *LocationOfBlockPointer	= Block->Next;

    //
    // Free up the memory, and delete the block record.
    //

    Pool->DeAllocateMemoryBlock( Block );
    delete Block;

//

    OS_UnLockMutex( &Lock );
    return BufferNoError;
}


// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//	Obtain a meta data reference 
//

BufferStatus_t	 Buffer_Generic_c::ObtainMetaDataReference(
						MetaDataType_t	  Type,
						void		**Pointer )
{
BlockDescriptor_t	 Block;

    //
    // Find the descriptor block 
    //
    
    AssertNonZeroReferenceCount( "Buffer_Generic_c::ObtainMetaDataReference " );

    OS_LockMutex( &Lock );
    
    for( Block  = ListOfMetaData;
	 Block != NULL;
	 Block  = Block->Next )
	if( Block->Descriptor->Type == Type )
	    break;

    if( Block == NULL )
    {
	report( severity_error, "Buffer_Generic_c::ObtainMetaDataReference - No meta data of the specified type is attached to the buffer.\n" );
	OS_UnLockMutex( &Lock );
	return BufferMetaDataTypeNotFound;
    }

    //
    // Return the appropriate value
    //

    *Pointer	= Block->Address[CachedAddress];
    OS_UnLockMutex( &Lock );

//

    return BufferNoError;
}


// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//	Set the amount of the buffer that is used.
//

BufferStatus_t	 Buffer_Generic_c::SetUsedDataSize( unsigned int	  DataSize )
{

    AssertNonZeroReferenceCount( "Buffer_Generic_c::SetUsedDataSize" );

    if( (BufferBlock == NULL) && (DataSize != 0) )
    {
	report( severity_error, "Buffer_Generic_c::SetUsedDataSize - Attempt to set none zero DataSize when buffer has no associated memory.\n" );
	return BufferError;
    }

//

    if( (BufferBlock != NULL) && (DataSize > BufferBlock->Size) )
    {
	report( severity_error, "Buffer_Generic_c::SetUsedDataSize - Attempt to set DataSize larger than Buffer size.\n" );
	return BufferError;
    }

//

    Pool->TotalUsedMemory	= Pool->TotalUsedMemory - this->DataSize + DataSize;
    this->DataSize		= DataSize;

    return BufferNoError;
}


// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//	Shrink the buffer
//

BufferStatus_t	 Buffer_Generic_c::ShrinkBuffer( unsigned int	  NewSize )
{
    AssertNonZeroReferenceCount( "Buffer_Generic_c::ShrinkBuffer" );

    if( NewSize < DataSize )
    {
	report( severity_error, "Buffer_Generic_c::ShrinkBuffer - The new buffer size will be less than the content size.\n" );
	return BufferError;
    }

    return Pool->ShrinkMemoryBlock( BufferBlock, NewSize );
}


// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//	Expand the buffer
//

BufferStatus_t	 Buffer_Generic_c::ExtendBuffer(unsigned int	 *NewSize,
						bool		  ExtendUpwards )
{
    AssertNonZeroReferenceCount( "Buffer_Generic_c::ExtendBuffer" );

    return Pool->ExtendMemoryBlock( BufferBlock, NewSize, ExtendUpwards );
}


// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//	Partition a buffer into sepearate buffers
//

BufferStatus_t   Buffer_Generic_c::PartitionBuffer(
						unsigned int	  LeaveInFirstPartitionSize,
						bool		  DuplicateMetaData,
						Buffer_t	 *SecondPartition,
						unsigned int	  SecondOwnerIdentifier,
						bool		  NonBlocking )
{
unsigned int		  i;
BufferStatus_t		  Status;
BufferDataDescriptor_t	 *Descriptor;
Buffer_Generic_c	 *NewBuffer;
BlockDescriptor_t	  Block;
BlockDescriptor_t	  Datum;
MetaDataType_t		  Type;
unsigned int		  Size;
void			 *MemoryBlock;

    //
    // Can we partition this buffer
    //

    AssertNonZeroReferenceCount( "Buffer_Generic_c::PartitionBuffer" );

    Descriptor	= BufferBlock->Descriptor;

    if( Descriptor->AllocateOnPoolCreation )
    {
	report( severity_error, "Buffer_Generic_c::PartitionBuffer - All buffer memory allocated on pool creation, partitioning not supported.\n" );
	return BufferOperationNotSupportedByThisDescriptor;
    }

    switch( Descriptor->AllocationSource )
    {
	case AllocateFromOSMemory:
	case AllocateFromNamedDeviceMemory:
	case AllocateFromDeviceVideoLMIMemory:
	case AllocateFromDeviceMemory:
	case AllocateIndividualSuppliedBlocks:
		report( severity_error, "Buffer_Generic_c::PartitionBuffer - Partitioning not supported for this allocation source (%d).\n", Descriptor->AllocationSource );
		return BufferOperationNotSupportedByThisDescriptor;
		break;

	case AllocateFromSuppliedBlock:
#if 0
// If partitioning, then we allow partitions of any size
		if( (LeaveInFirstPartitionSize % Descriptor->AllocationUnitSize) != 0 )
		{
		    report( severity_error, "Buffer_Generic_c::PartitionBuffer - Can only partition in 'AllocationUnitSize; chunks (%d %% %d != 0).\n", LeaveInFirstPartitionSize, Descriptor->AllocationUnitSize );
		    return BufferOperationNotSupportedByThisDescriptor;
		}
#endif
		break;

	case NoAllocation:
		break;
    }

    //
    // Get a new buffer of zero size (specially forces no allocation)
    //

    Status	= Pool->GetBuffer( SecondPartition, SecondOwnerIdentifier, 0, NonBlocking );
    if( Status != BufferNoError )
	return Status;

    NewBuffer	= (Buffer_Generic_t)(*SecondPartition);

    //
    // Copy first buffer block into new one, and adjust memory pointers
    //

    Block				= NewBuffer->BufferBlock;
    memcpy( Block, BufferBlock, sizeof(struct BlockDescriptor_s) );

    for( i=0; i<3; i++ )
	if( Block->Address[i] != NULL )
	    Block->Address[i]	= (unsigned char *)BufferBlock->Address[i] + LeaveInFirstPartitionSize;

    Block->PoolAllocatedOffset		= BufferBlock->PoolAllocatedOffset + LeaveInFirstPartitionSize;

    Block->Size				= BufferBlock->Size - LeaveInFirstPartitionSize;
    NewBuffer->DataSize			= DataSize - LeaveInFirstPartitionSize;

    BufferBlock->Size			= LeaveInFirstPartitionSize;
    DataSize				= LeaveInFirstPartitionSize;

    //
    // Do we need to copy the metadata 
    //

    if( DuplicateMetaData )
	for( Datum  = ListOfMetaData;
	     Datum != NULL;
	     Datum  = Datum->Next )
	{
	    Type	= Datum->Descriptor->Type;
	    Size	= Datum->Size;

	    switch( Datum->Descriptor->AllocationSource )
	    {
		case AllocateFromOSMemory:			MemoryBlock	= NULL;					break;
		case AllocateFromNamedDeviceMemory:		MemoryBlock	= NULL;					break;
		case AllocateFromDeviceVideoLMIMemory:		MemoryBlock	= NULL;					break;
		case AllocateFromDeviceMemory:			MemoryBlock	= NULL;					break;
		case AllocateFromSuppliedBlock:			MemoryBlock	= NULL;					break;
		case NoAllocation:				MemoryBlock	= Datum->Address[CachedAddress];	break;

		default:					break;	// No Action, already returned error
	    }

	    Status	= NewBuffer->AttachMetaData( Type, Size, MemoryBlock );
	    if( Status != BufferNoError )
	    {
		report( severity_error, "BufferPool_Generic_c::PartitionBuffer - Unable to duplicate meta data block.\n" );
		return BufferInsufficientMemoryForMetaData;
	    }

	    if( MemoryBlock == NULL )
	    {
		Status = NewBuffer->ObtainMetaDataReference( Type, &MemoryBlock );
		if( Status != BufferNoError )
		{
		    report( severity_error, "BufferPool_Generic_c::PartitionBuffer - Unable to reference attached meta data block.\n" );
		    return BufferInsufficientMemoryForMetaData;
		}

		memcpy( MemoryBlock, Datum->Address[CachedAddress], Size );
	    }
	}

//

    return BufferNoError;
}


// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//	Register data reference functions
//

BufferStatus_t   Buffer_Generic_c::RegisterDataReference(
						unsigned int	  BlockSize,
						void		 *Pointer,
						AddressType_t	  AddressType )
{
void	*Pointers[3];

    AssertNonZeroReferenceCount( "Buffer_Generic_c::RegisterDataReference" );

    Pointers[0]			= NULL;
    Pointers[1]			= NULL;
    Pointers[2]			= NULL;
    Pointers[AddressType]	= Pointer;

    return RegisterDataReference( BlockSize, Pointers );
}

// -------------------------------------

BufferStatus_t   Buffer_Generic_c::RegisterDataReference(
						unsigned int	  BlockSize,
						void		 *Pointers[3] )
{
    //
    // We can only specify the buffer address when the no-allocation method is used
    //

    AssertNonZeroReferenceCount( "Buffer_Generic_c::RegisterDataReference" );

    if( BufferBlock->Descriptor->AllocationSource != NoAllocation )
    {
	report( severity_error, "Buffer_Generic_c::RegisterDataReference - Attempt to register a data reference pointer on an allocated buffer.\n" ); 
	return BufferOperationNotSupportedByThisDescriptor;
    }

//

    BufferBlock->Size		= BlockSize;
    BufferBlock->Address[0]	= Pointers[0];
    BufferBlock->Address[1]	= Pointers[1];
    BufferBlock->Address[2]	= Pointers[2];
    DataSize			= 0;

//

    return BufferNoError;
}


// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//	
//

BufferStatus_t   Buffer_Generic_c::ObtainDataReference(	
						unsigned int	 *BlockSize,
						unsigned int	 *UsedDataSize,
						void		**Pointer,
						AddressType_t	  AddressType )

{
    AssertNonZeroReferenceCount( "Buffer_Generic_c::ObtainDataReference" );

    #ifdef VIRTULISATION
    AddressType = (AddressType_t)0;
    #endif

    if( BufferBlock->Size == 0 )
    {
        // This is not an exceptional behaviour (we call this from the audio compressed data bypass code
        // to discover if the buffer *has* compressed data attached in the first place). Since we do this
        // every frame we could really do not to report each occassion this happens.
	//report( severity_error, "Buffer_Generic_c::ObtainDataReference - Attempt to obtain a data reference pointer on a buffer with no data.\n" ); 
	return BufferNoDataAttached;
    }

    if( BufferBlock->Address[AddressType] == NULL )
    {
        report( severity_error, "Buffer_Generic_c::ObtainDataReference - Attempt to obtain a data reference pointer in an address type that is unavailable for the buffer - Address translation not supported.\n" );
	*Pointer = NULL;
	return BufferError;
    }

//

    if( BlockSize != NULL )
	*BlockSize	= BufferBlock->Size;

    if( UsedDataSize != NULL )
	*UsedDataSize	= DataSize;

    if( Pointer != NULL )
	*Pointer	= BufferBlock->Address[AddressType];

//

    return BufferNoError;
}


// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//	
//

BufferStatus_t   Buffer_Generic_c::TransferOwnership(
						unsigned int	  OwnerIdentifier0,
						unsigned int	  OwnerIdentifier1 )
{
unsigned int	  i;

//

    AssertNonZeroReferenceCount( "Buffer_Generic_c::TransferOwnership" );

    if( OwnerIdentifier1 != UNSPECIFIED_OWNER )
    {
	for( i=0; i<MAX_BUFFER_OWNER_IDENTIFIERS; i++ )
            if( OwnerIdentifier[i] == OwnerIdentifier0 )
	    {
	        OwnerIdentifier[i]	= OwnerIdentifier1;
		return BufferNoError;
	    }

	report( severity_note, "Buffer_Generic_c::TransferOwnership - specified current owner not found.\n" );
    }
    else
    {
	OwnerIdentifier[0]	= OwnerIdentifier0;
    }

//

    return BufferNoError;
}


// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//	
//

BufferStatus_t   Buffer_Generic_c::IncrementReferenceCount( unsigned int  NewOwnerIdentifier )
{
unsigned int	  i;

//

    AssertNonZeroReferenceCount( "Buffer_Generic_c::IncrementReferenceCount" );

    OS_LockMutex( &Pool->Lock );
    Pool->ReferenceCount++;
    OS_UnLockMutex( &Pool->Lock );

    OS_LockMutex( &Lock );
    ReferenceCount++;

    if( NewOwnerIdentifier != UNSPECIFIED_OWNER )
    {
    	for( i=0; i<MAX_BUFFER_OWNER_IDENTIFIERS; i++ )
	    if( OwnerIdentifier[i] == UNSPECIFIED_OWNER )
	    {
	    	OwnerIdentifier[i]	= NewOwnerIdentifier;
	    	break;
	    }

	if( i >= MAX_BUFFER_OWNER_IDENTIFIERS )
	    report( severity_note, "Buffer_Generic_c::IncrementReferenceCount - More than expected references, new owner not recorded.\n" );
    }

    OS_UnLockMutex( &Lock );
    return BufferNoError;
}


// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//	Decrement a reference count, and release the buffer if we get to zero.
//	We take a copy (during the locked period) of the reference to ensure 
//	that if two processes are decrementing the count, only one will do the
//	actual release (without having to extend the period of locking).
//

BufferStatus_t   Buffer_Generic_c::DecrementReferenceCount( unsigned int  OldOwnerIdentifier )
{
unsigned int	  i;
unsigned int	  ReferenceCountAfterDecrement;

//

    AssertNonZeroReferenceCount( "Buffer_Generic_c::DecrementReferenceCount" );

    OS_LockMutex( &Pool->Lock );
    Pool->ReferenceCount--;
    OS_UnLockMutex( &Pool->Lock );

    OS_LockMutex( &Lock );
    ReferenceCountAfterDecrement	= --ReferenceCount;
    OS_UnLockMutex( &Lock );

    if( OldOwnerIdentifier != UNSPECIFIED_OWNER )
    {
    	for( i=0; i<MAX_BUFFER_OWNER_IDENTIFIERS; i++ )
	    if( OwnerIdentifier[i] == OldOwnerIdentifier )
	    {
	    	OwnerIdentifier[i]	= UNSPECIFIED_OWNER;
	    	break;
	    }
    }

//

    if( ReferenceCountAfterDecrement == 0 )
	return Pool->ReleaseBuffer( this );

//

    return BufferNoError;
}


// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//	Attach a buffer to this one
//

BufferStatus_t   Buffer_Generic_c::AttachBuffer( Buffer_t	  Buffer )
{
unsigned int	i;

//

    AssertNonZeroReferenceCount( "Buffer_Generic_c::AttachBuffer" );

    OS_LockMutex( &Lock );
    for( i=0; i<MAX_ATTACHED_BUFFERS; i++ )
	if( AttachedBuffers[i] == NULL )
	{
	    AttachedBuffers[i]	= Buffer;
	    OS_UnLockMutex( &Lock );
	    Buffer->IncrementReferenceCount();
	    return BufferNoError;
	}

    report( severity_error, "Buffer_Generic_c::AttachBuffer - Too many buffers attached to this one.\n" );
    OS_UnLockMutex( &Lock );
    return BufferTooManyAttachments;
}


// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//	Detach a buffer from this one
//

BufferStatus_t   Buffer_Generic_c::DetachBuffer( Buffer_t	  Buffer )
{
unsigned int	i;

//

    AssertNonZeroReferenceCount( "Buffer_Generic_c::DetachBuffer" );

    OS_LockMutex( &Lock );
    for( i=0; i<MAX_ATTACHED_BUFFERS; i++ )
	if( AttachedBuffers[i] == Buffer )
	{
	    AttachedBuffers[i]	= NULL;
	    OS_UnLockMutex( &Lock );
	    Buffer->DecrementReferenceCount( IdentifierAttachedToOtherBuffer );
	    return BufferNoError;
	}

    report( severity_error, "Buffer_Generic_c::DetachBuffer - Attached buffer not found.\n" );
    OS_UnLockMutex( &Lock );
    return BufferAttachmentNotFound;
}


// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//	Obtain a meta data reference 
//

BufferStatus_t	 Buffer_Generic_c::ObtainAttachedBufferReference(
						BufferType_t	  Type,
						Buffer_t	 *Buffer )
{
unsigned int	i;
BufferType_t	AttachedType;

//

    AssertNonZeroReferenceCount( "Buffer_Generic_c::ObtainAttachedBufferReference" );

    for( i=0; i<MAX_ATTACHED_BUFFERS; i++ )
	if( AttachedBuffers[i] != NULL )
	{
	    AttachedBuffers[i]->GetType( &AttachedType );
	    if( AttachedType == Type )
	    {
		*Buffer	= AttachedBuffers[i];
		return BufferNoError;
	    }
	}

    return BufferAttachmentNotFound;
}


// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//	Discover the type of buffer supported by the pool
//

BufferStatus_t   Buffer_Generic_c::GetType(	BufferType_t	 *Type )
{
    if (this)
        AssertNonZeroReferenceCount( "Buffer_Generic_c::GetType" );

    *Type	= BufferBlock->Descriptor->Type;
    return BufferNoError;
}


// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//	Discover the type of buffer supported nby the pool
//

BufferStatus_t   Buffer_Generic_c::GetIndex(	unsigned int	 *Index )
{
    AssertNonZeroReferenceCount( "Buffer_Generic_c::GetIndex" );

    *Index	= this->Index;
    return BufferNoError;
}


// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//	
//

BufferStatus_t   Buffer_Generic_c::GetMetaDataCount( 	unsigned int	 *Count )
{
BlockDescriptor_t	  Block;

    AssertNonZeroReferenceCount( "Buffer_Generic_c::GetMetaDataCount" );

    *Count	= 0;
    for( Block	 = ListOfMetaData;
	 Block	!= NULL;
	 Block	 = Block->Next )
	(*Count)++;

    return BufferNoError;
}


// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//	
//

BufferStatus_t   Buffer_Generic_c::GetMetaDataList(	unsigned int	  ArraySize,
							unsigned int	 *ArrayOfMetaDataTypes )
{
unsigned int		  i;
BlockDescriptor_t	  Block;

//

    AssertNonZeroReferenceCount( "Buffer_Generic_c::GetMetaDataList" );

    i 	= 0;
    for( Block	 = ListOfMetaData;
	 (Block	!= NULL) && (i<ArraySize);
	 Block	 = Block->Next )
	ArrayOfMetaDataTypes[i++]	= Block->Descriptor->Type;

//

    if( Block != NULL )
    {
	report( severity_error, "Buffer_Generic_c::GetMetaDataList - Too many meta data types for array.\n" );
	return BufferError;
    }

//
    return BufferNoError;
}


// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//	
//

BufferStatus_t   Buffer_Generic_c::GetOwnerCount( 	unsigned int	 *Count )
{
    AssertNonZeroReferenceCount( "Buffer_Generic_c::GetOwnerCount" );

    *Count	= ReferenceCount;
    return BufferNoError;
}


// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//	
//

BufferStatus_t   Buffer_Generic_c::GetOwnerList(	unsigned int	  ArraySize,
							unsigned int	 *ArrayOfOwnerIdentifiers )
{
unsigned int		  i,j;

//

    AssertNonZeroReferenceCount( "Buffer_Generic_c::GetOwnerList" );

    i 	= 0;
    for( j=0; j<MAX_BUFFER_OWNER_IDENTIFIERS; j++ )
	if( OwnerIdentifier[j] != UNSPECIFIED_OWNER )
	{
	    if( i >= ArraySize )
		break;

	    ArrayOfOwnerIdentifiers[i++]	= OwnerIdentifier[j];
	}

//

    if( j < MAX_BUFFER_OWNER_IDENTIFIERS )
    {
	report( severity_error, "Buffer_Generic_c::GetOwnerList - Too many owner identifiers for array.\n" );
	return BufferError;
    }

//

    return BufferNoError;
}


// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//	
//

BufferStatus_t		 Buffer_Generic_c::FlushCache( void )
{

	OSDEV_FlushCacheRange((void*)BufferBlock->Address[CachedAddress],DataSize);

	return BufferNoError;
}


BufferStatus_t		 Buffer_Generic_c::PurgeCache( void )
{

	OSDEV_PurgeCacheRange((void*)BufferBlock->Address[CachedAddress],DataSize);

	return BufferNoError;
}


// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//	
//

void		 Buffer_Generic_c::Dump( unsigned int	  Flags )
{
unsigned int		  i;
unsigned int		  LocalMetaCount;
BlockDescriptor_t	  MetaData;
BufferDataDescriptor_t	 *Descriptor;

//

    if( (Flags & DumpBufferStates) != 0 )
    {
	report( severity_info, "\tBuffer (index %2d) of type %04x - '%s'\n", Index, BufferBlock->Descriptor->Type, 
			(BufferBlock->Descriptor->TypeName == NULL) ? "Unnamed" : BufferBlock->Descriptor->TypeName );

//

	report( severity_info, "\t    Current values  Size %06x, Occupied %06x, References %d\n",
			BufferBlock->Size, DataSize, ReferenceCount );

	if( ReferenceCount != 0 )
	{
	    report( severity_info, "\t    Current owners   " );
	    for( i=0; i<MAX_BUFFER_OWNER_IDENTIFIERS; i++ )
	        if( OwnerIdentifier[i] != UNSPECIFIED_OWNER )
		    report( severity_info, " - %08x", OwnerIdentifier[i] );
	    report( severity_info, "\n" );
	}

//

	LocalMetaCount	= 0;
	for( MetaData  = ListOfMetaData;
	     MetaData != NULL;
	     MetaData  = MetaData->Next )
	    if( !MetaData->AttachedToPool )
		LocalMetaCount++;

	report( severity_info, "\t    %s specifically attached to this buffer.\n", 
		(LocalMetaCount == 0) ? "There are no Meta data items" : "The following meta data items are" );

	for( MetaData  = ListOfMetaData;
	     MetaData != NULL;
	     MetaData  = MetaData->Next )
	    if( !MetaData->AttachedToPool )
		report( severity_info, "\t\t%04x - %s\n", MetaData->Descriptor->Type, 
			(MetaData->Descriptor->TypeName == NULL) ? "Unnamed" : MetaData->Descriptor->TypeName );

	for( i=0; i<MAX_ATTACHED_BUFFERS; i++ )
	    if( AttachedBuffers[i] != NULL )
	    {
		Descriptor	= ((Buffer_Generic_c *)AttachedBuffers[i])->Pool->BufferDescriptor;
		report( severity_info, "\t    Buffer of type %04x - %s attached\n", Descriptor->Type,
			(Descriptor->TypeName == NULL) ? "Unnamed" : Descriptor->TypeName );
	    }
    }

}

//will need to pass in Player ref, or the type.. and what about the struct def too...
void Buffer_Generic_c::DumpToRelayFS( unsigned int id, unsigned int source, void *param )
{

	BufferStatus_t          BufferStatus;
	ParsedAudioParameters_t *TheAudioParameters;
    	struct ParsedVideoParameters_s* TheVideoParameters;
	Player_Generic_t Player = (Player_Generic_t) param;

	switch(id) {

	case ST_RELAY_TYPE_DECODED_AUDIO_BUFFER:
		BufferStatus = ObtainMetaDataReference (Player->MetaDataParsedAudioParametersType, (void**) &TheAudioParameters);
		if (BufferStatus == BufferNoError)
		{
			if(BufferBlock->Address[UnCachedAddress]) {
			if(TheAudioParameters->SampleCount>0)
				st_relayfs_write(id, source, (unsigned char *)BufferBlock->Address[UnCachedAddress],
					(unsigned int)(TheAudioParameters->SampleCount *
							TheAudioParameters->Source.ChannelCount *
							(TheAudioParameters->Source.BitsPerSample / 8)),
					0
				);
			}
		}
		break;

	case ST_RELAY_TYPE_DECODED_VIDEO_BUFFER:
		BufferStatus = ObtainMetaDataReference (Player->MetaDataParsedVideoParametersType, (void**)&TheVideoParameters);
		if (BufferStatus == BufferNoError)
		{
			struct relay_video_frame_info_s info;

			info.width         = TheVideoParameters->Content.Width;
			info.height        = TheVideoParameters->Content.Height;
			info.type          = 0;                              //TODO: just ASSUMING SURF_YCBCR420MB for now
			info.luma_offset   = 0; //SurfaceDescriptor.LumaOffset;
			info.chroma_offset = ((BufferBlock->Size*2)/3); //SurfaceDescriptor.ChromaOffset;

			if(BufferBlock->Address[CachedAddress]) {
				st_relayfs_write(id, source, (unsigned char *)BufferBlock->Address[CachedAddress],
					(unsigned int)((TheVideoParameters->Content.Width *
							TheVideoParameters->Content.Height *
							4)/3), //TODO: just ASSUMING SURF_YCBCR420MB for now
					&info
				);
			}
		}
		break;

	default:
		break;
	}

}



