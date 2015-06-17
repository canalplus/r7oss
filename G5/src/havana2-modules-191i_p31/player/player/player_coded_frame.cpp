/************************************************************************
Copyright (C) 2009 STMicroelectronics. All Rights Reserved.

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

Source file name : player_coded_frame.cpp
Author :           Nick

Implementation of the coded frame buffer related functions of the
generic class implementation of player 2


Date        Modification                                    Name
----        ------------                                    --------
23-Feb-09   Created                                         Nick

************************************************************************/

#include <stdarg.h>
#include "player_generic.h"


// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Find the coded frame buffer pool associated with a stream playback
//

PlayerStatus_t   Player_Generic_c::GetCodedFrameBufferPool(
						PlayerStream_t	 	  Stream,
						BufferPool_t		 *Pool,
						unsigned int		 *MaximumCodedFrameSize )
{
PlayerStatus_t          Status;
#ifdef __KERNEL__
allocator_status_t      AStatus;
#endif

    //
    // If we haven't already created the buffer pool, do it now.
    //

    if( Stream->CodedFrameBufferPool == NULL )
    {
	//
	// Get the memory and Create the pool with it
	//

#if __KERNEL__
	AStatus = PartitionAllocatorOpen( &Stream->CodedFrameMemoryDevice, Stream->CodedMemoryPartitionName, Stream->CodedMemorySize, true );
	if( AStatus != allocator_ok )
	{
	    report( severity_error, "Player_Generic_c::GetCodedFrameBufferPool - Failed to allocate memory\n" );
	    return PlayerInsufficientMemory;
	}

	Stream->CodedFrameMemory[CachedAddress]         = AllocatorUserAddress( Stream->CodedFrameMemoryDevice );
	Stream->CodedFrameMemory[UnCachedAddress]       = AllocatorUncachedUserAddress( Stream->CodedFrameMemoryDevice );
	Stream->CodedFrameMemory[PhysicalAddress]       = AllocatorPhysicalAddress( Stream->CodedFrameMemoryDevice );
#else
	static unsigned char    Memory[4][4*1024*1024];

	Stream->CodedFrameMemory[CachedAddress]         = Memory[Stream->StreamType];
	Stream->CodedFrameMemory[UnCachedAddress]       = NULL;
	Stream->CodedFrameMemory[PhysicalAddress]       = Memory[Stream->StreamType];
	Stream->CodedMemorySize				= 4*1024*1024;
#endif

//

	Status  = BufferManager->CreatePool( &Stream->CodedFrameBufferPool, Stream->CodedFrameBufferType, Stream->CodedFrameCount, Stream->CodedMemorySize, Stream->CodedFrameMemory );
	if( Status != BufferNoError )
	{
	    report( severity_error, "Player_Generic_c::GetCodedFrameBufferPool - Failed to create the pool.\n" );
	    return PlayerInsufficientMemory;
	}
    }

    //
    // Setup the parameters and return
    //

    if( Pool != NULL )
	*Pool			= Stream->CodedFrameBufferPool;

    if( MaximumCodedFrameSize != NULL )
	*MaximumCodedFrameSize	= Stream->CodedFrameMaximumSize;

    return PlayerNoError;
}




