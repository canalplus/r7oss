/************************************************************************
COPYRIGHT (C) STMicroelectronics 2005

Source file name : allocator_simple.cpp
Author :           Nick

Implementation of the pure virtual class used to control the allocation
of portions of a buffer.

Note this is the simplest of implementations, it is definitely not the best


Date        Modification                                    Name
----        ------------                                    --------
29-Jun-05   Created                                         Nick

************************************************************************/

#if defined (__KERNEL__)
#include "osinline.h"
#else
#include <stdlib.h>
#endif

#include "report.h"
#include "allocator_simple.h"

// ------------------------------------------------------------------------
// Macro

#define On64MBBoundary( O )	((((unsigned int)(O) + (unsigned int)PhysicalAddress) & 0x03ffffff) == 0)

// ------------------------------------------------------------------------
// Constructor function

AllocatorSimple_c::AllocatorSimple_c(   unsigned int     BufferSize,
					unsigned int     SegmentSize,
					unsigned char   *PhysicalAddress  )
{
    InitializationStatus        = AllocatorError;

    OS_InitializeMutex( &Lock );
    OS_InitializeEvent( &EntryFreed );

    this->BufferSize    	= BufferSize;
    this->SegmentSize   	= SegmentSize;
    this->PhysicalAddress       = PhysicalAddress;

    BeingDeleted		= false;
    InAllocate			= false;

    Free();
    InitializationStatus        = AllocatorNoError;
}

// ------------------------------------------------------------------------
// Destructor function

AllocatorSimple_c::~AllocatorSimple_c( void )
{
    BeingDeleted        = true;

    OS_SetEvent( &EntryFreed );

    while( InAllocate )
	OS_SleepMilliSeconds( 1 );

    OS_TerminateEvent( &EntryFreed );
    OS_TerminateMutex( &Lock );
}

// ------------------------------------------------------------------------
// Allocate function

AllocatorStatus_t   AllocatorSimple_c::Allocate( 
				unsigned int      Size,
				unsigned char   **Block,
				bool              NonBlocking )
{
unsigned int    i;

//

    if( this == NULL )
	return AllocatorError;

//

    InAllocate  = true;
    Size        = (((Size + SegmentSize - 1) / SegmentSize) * SegmentSize);

    do 
    {
	OS_LockMutex( &Lock );
	OS_ResetEvent( &EntryFreed );

//

	if( BeingDeleted )
	    break;

//

	for( i=0; i<(HighestUsedBlockIndex + 1); i++ )
	    if( Blocks[i].InUse && (Blocks[i].Size >= Size) )
	    {
		*Block                   = Blocks[i].Base;
		Blocks[i].Base          += Size;
		Blocks[i].Size          -= Size;

		if( Blocks[i].Size == 0 )
		    Blocks[i].InUse      = false;

		OS_UnLockMutex( &Lock );
		InAllocate       = false;
		return AllocatorNoError;
	    }

//

#if 0
{
    report( severity_info, "Waiting for memory \n" );
    for( i=0; i<ALLOCATOR_SIMPLE_MAX_BLOCKS; i++ )
	if( Blocks[i].InUse )
	    report( severity_info, "        %08x - %06x\n", Blocks[i].Base, Blocks[i].Size );
}
#endif

//

	OS_UnLockMutex( &Lock );

	if( !NonBlocking )
	    OS_WaitForEvent( &EntryFreed, OS_INFINITE );

    } while( !NonBlocking && !BeingDeleted );

    InAllocate   = false;
    return AllocatorUnableToAllocate;
}

// ------------------------------------------------------------------------
// Allocate function

AllocatorStatus_t   AllocatorSimple_c::AllocateLargest( 
				unsigned int     *Size,
				unsigned char   **Block,
				bool              NonBlocking )
{
int	Index;

//

    if( this == NULL )
	return AllocatorError;

//

    InAllocate  = true;

    do 
    {
	OS_LockMutex( &Lock );
	OS_ResetEvent( &EntryFreed );

//

	if( BeingDeleted )
	    break;

//

	Index	= LargestFreeBlock();
	if( (Index >= 0) && (Blocks[Index].Size >= (*Size)) )
	{
	    *Size		= Blocks[Index].Size;
	    *Block		= Blocks[Index].Base;

	    Blocks[Index].Size	= 0;
	    Blocks[Index].InUse	= false;

	    OS_UnLockMutex( &Lock );
	    InAllocate       = false;
	    return AllocatorNoError;
	}

	OS_UnLockMutex( &Lock );

	if( !NonBlocking )
	    OS_WaitForEvent( &EntryFreed, OS_INFINITE );

    } while( !NonBlocking && !BeingDeleted );

    InAllocate   = false;
    return AllocatorUnableToAllocate;
}

// ------------------------------------------------------------------------
// Extend an allocation to its largest available size (either adding at the end or beginning)
// NOTE we practically split this fn into two separate implementations depending on which
// way we want to extend.

AllocatorStatus_t   AllocatorSimple_c::ExtendToLargest( 
				unsigned int	 *Size,
				unsigned char	**Block,
				bool		  ExtendUpwards )
{
unsigned int	 i;
unsigned char	*BoundaryToExtend;

//

    if( ExtendUpwards )
    {
	//
	// First check to see if extension is even possible
	//

	BoundaryToExtend	= *Block+*Size;
	if( On64MBBoundary(BoundaryToExtend) || (BoundaryToExtend >= (PhysicalAddress + BufferSize)) )
	    return AllocatorNoMemory;

	//
	// Now scan for an adjacent block
	//

	OS_LockMutex( &Lock );
	for( i=0; i<(HighestUsedBlockIndex + 1); i++ )
	    if( Blocks[i].InUse && (BoundaryToExtend == Blocks[i].Base) )
	    {
		*Size		+= Blocks[i].Size;
		Blocks[i].Size	 = 0;
		Blocks[i].InUse	 = false;
		OS_UnLockMutex( &Lock );
		return AllocatorNoError;
	    }
    }
    else
    {
	//
	// First check to see if extension is even possible
	//

	BoundaryToExtend	= *Block;
	if( On64MBBoundary(BoundaryToExtend) || (BoundaryToExtend == PhysicalAddress) )
	    return AllocatorNoMemory;

	//
	// Now scan for an adjacent block
	//

	OS_LockMutex( &Lock );
	for( i=0; i<(HighestUsedBlockIndex + 1); i++ )
	    if( Blocks[i].InUse && (BoundaryToExtend == (Blocks[i].Base + Blocks[i].Size)) )
	    {
		*Size		+= Blocks[i].Size;
		*Block		 = Blocks[i].Base;

		Blocks[i].Size	 = 0;
		Blocks[i].InUse	 = false;
		OS_UnLockMutex( &Lock );
		return AllocatorNoError;
	    }
    }

    OS_UnLockMutex( &Lock );
    return AllocatorUnableToAllocate;
}

// ------------------------------------------------------------------------
// Free the whole block, note we partition this into blocks that do not
// cross a 64Mb boundary in the physical address space.

AllocatorStatus_t   AllocatorSimple_c::Free( void )
{
int 			 i;
unsigned int		 LocalBufferSize;
unsigned int		 MaxBlockSize;
unsigned char		*LocalOffset;

    OS_LockMutex( &Lock );

    memset( Blocks, 0x00, sizeof(Blocks) );

    LocalBufferSize		= BufferSize;
    LocalOffset			= NULL;
    MaxBlockSize		= 0x4000000 - ((unsigned int)PhysicalAddress & 0x03ffffff);
    HighestUsedBlockIndex	= 0;

    for( i=0; ((LocalBufferSize != 0) && (i<ALLOCATOR_SIMPLE_MAX_BLOCKS)); i++ )
    {
	Blocks[i].InUse		 = true;
	Blocks[i].Size		 = min( LocalBufferSize, MaxBlockSize );
	Blocks[i].Base		 = LocalOffset;

	LocalBufferSize		-= Blocks[i].Size;
	LocalOffset		+= Blocks[i].Size;
	MaxBlockSize		 = 0x4000000;
	HighestUsedBlockIndex	 = i;
    }

    OS_SetEvent( &EntryFreed );
    OS_UnLockMutex( &Lock );

    return AllocatorNoError;
}

// ------------------------------------------------------------------------
// Free range of entries

AllocatorStatus_t   AllocatorSimple_c::Free(    unsigned int      Size,
						unsigned char    *Block )
{
unsigned int	i;
unsigned int	LowestFreeBlock;
unsigned int	NextHighestUsedBlockIndex;

//

    Size                	= (((Size + SegmentSize - 1) / SegmentSize) * SegmentSize);
    LowestFreeBlock     	= HighestUsedBlockIndex + 1;
    NextHighestUsedBlockIndex	= 0;

    OS_LockMutex( &Lock );

    //
    // Note by adding adjascent block records to the one we are trying to free,
    // this loop does concatenation of free blocks as well as freeing the 
    // current block.
    //

    for( i=0; i<(HighestUsedBlockIndex + 1); i++ )
    {
	if( Blocks[i].InUse )
	{
	    if( ((Block + Size) == Blocks[i].Base) && 
		!On64MBBoundary(Blocks[i].Base) )
	    {
		Size            += Blocks[i].Size;
		Blocks[i].InUse  = false;
	    }
	    else if( (Block == (Blocks[i].Base + Blocks[i].Size)) &&
		!On64MBBoundary(Block) )
	    {
		Size            += Blocks[i].Size;
		Block            = Blocks[i].Base;
		Blocks[i].InUse  = false;
	    }
	    else
	    {
		NextHighestUsedBlockIndex	= i;
	    }
	}

	if( !Blocks[i].InUse )
	{
	    LowestFreeBlock      = min( LowestFreeBlock, i );
	}
    }

//

    if( LowestFreeBlock == ALLOCATOR_SIMPLE_MAX_BLOCKS )
    {
	report( severity_error, "AllocatorSimple_c::Free - Unable to create a free block - too many.\n" );
{
    report( severity_info, "\tBlocks\n" );
    for( i=0; i<ALLOCATOR_SIMPLE_MAX_BLOCKS; i++ )
	if( Blocks[i].InUse )
	    report( severity_info, "        %08x - %06x\n", Blocks[i].Base, Blocks[i].Size );
}

	OS_SetEvent( &EntryFreed );
	OS_UnLockMutex( &Lock );

	return AllocatorError;
    }

//

    Blocks[LowestFreeBlock].InUse       = true;
    Blocks[LowestFreeBlock].Size        = Size;
    Blocks[LowestFreeBlock].Base        = Block;

    HighestUsedBlockIndex		= max( LowestFreeBlock, NextHighestUsedBlockIndex );

//

    OS_SetEvent( &EntryFreed );
    OS_UnLockMutex( &Lock );

    return AllocatorNoError;
}


// ------------------------------------------------------------------------
// Inform the caller of the size of the largest free block

AllocatorStatus_t   AllocatorSimple_c::LargestFreeBlock( unsigned int	 *Size )
{
int Index;

    OS_LockMutex( &Lock );

    Index	= LargestFreeBlock();
    *Size	= (Index < 0) ? 0 : Blocks[Index].Size;

    OS_UnLockMutex( &Lock );

    return AllocatorNoError;
}


// ------------------------------------------------------------------------
// Private - internal function to find the largest free block

int   AllocatorSimple_c::LargestFreeBlock( void )
{
unsigned int	i;
int		Index;

//

    Index	= -1;

    for( i=0; i<(HighestUsedBlockIndex + 1); i++ )
	if( Blocks[i].InUse && ((Index < 0) || (Blocks[i].Size > Blocks[Index].Size)) )
	    Index	= i;

    return Index;
}

