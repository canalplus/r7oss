/************************************************************************
Copyright (C) 2007 STMicroelectronics. All Rights Reserved.

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

Source file name : manifestor_clone.cpp
Author :           Julian

Implementation of the clone manifestor class for player 2.

Date        Modification                                    Name
----        ------------                                    --------
20-Jan-10   Created                                         Nick

************************************************************************/

// /////////////////////////////////////////////////////////////////////
//
//      Include any component headers

#include "manifestor_clone.h"
#include "ring_generic.h"

// /////////////////////////////////////////////////////////////////////////
//
// Locally defined constants
//

// /////////////////////////////////////////////////////////////////////////
//
//  Thread entry stub
//

static OS_TaskEntry (BufferReleaseThreadStub)
{
    Manifestor_Clone_c  *CloneManifestor	= (Manifestor_Clone_c*)Parameter;

    CloneManifestor->BufferReleaseThread();
    OS_TerminateThread();
    return NULL;
}

// /////////////////////////////////////////////////////////////////////////
//
//      Class Constructor
//

Manifestor_Clone_c::Manifestor_Clone_c( Manifestor_t	Original )
{
    InitializationStatus                = ManifestorError;

    this->Original			= Original;

    CloneTo				= NULL;
    CloneOutputRing			= NULL;
    OriginalOutputRing			= NULL;
    BufferReleaseThreadId               = OS_INVALID_THREAD;
    BufferReleaseThreadRunning		= false;

    InitializationStatus                = ManifestorNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
//      Class Destructor
//

Manifestor_Clone_c::~Manifestor_Clone_c( void )
{
    if( CloneTo != NULL )
	SetCloneTo( NULL );
}


// /////////////////////////////////////////////////////////////////////////
//
//      Component base class clones
//

PlayerStatus_t   Manifestor_Clone_c::Halt( 			void )
{
    OriginalOutputRing	= NULL;

    if( CloneTo != NULL )
	CloneTo->Halt();

    return Original->Halt();
}

//

PlayerStatus_t   Manifestor_Clone_c::Reset(			void )
{
    if( CloneTo != NULL )
	CloneTo->Reset();

    return Original->Reset();
}

//

PlayerStatus_t   Manifestor_Clone_c::SetModuleParameters(	unsigned int		 ParameterBlockSize,
								void			*ParameterBlock)
{
    if( CloneTo != NULL )
	CloneTo->SetModuleParameters( ParameterBlockSize, ParameterBlock );

    return Original->SetModuleParameters( ParameterBlockSize, ParameterBlock );
}

//

PlayerStatus_t   Manifestor_Clone_c::RegisterPlayer(		Player_t		 Player,
								PlayerPlayback_t	 Playback,
								PlayerStream_t		 Stream,
								Collator_t		 Collator,
								FrameParser_t		 FrameParser,
								Codec_t			 Codec,
								OutputTimer_t		 OutputTimer,
								Manifestor_t		 Manifestor )
{
    BaseComponentClass_c::RegisterPlayer( Player, Playback, Stream, Collator, FrameParser, Codec, OutputTimer, Manifestor );

    if( CloneTo != NULL )
	CloneTo->RegisterPlayer( Player, Playback, Stream, Collator, FrameParser, Codec, OutputTimer, Manifestor );

    return Original->RegisterPlayer( Player, Playback, Stream, Collator, FrameParser, Codec, OutputTimer, Manifestor );
}


// /////////////////////////////////////////////////////////////////////////
//
//      GetDecodeBufferPool - We use the original classes pool
//


ManifestorStatus_t   Manifestor_Clone_c::GetDecodeBufferPool(           BufferPool_t             *Pool )
{
    return Original->GetDecodeBufferPool( Pool );
}

// /////////////////////////////////////////////////////////////////////////
//
//      GetPostProcessControlBufferPool - We use the original classes pool
//


ManifestorStatus_t   Manifestor_Clone_c::GetPostProcessControlBufferPool(BufferPool_t            *Pool )
{
    return Original->GetPostProcessControlBufferPool( Pool );
}

// /////////////////////////////////////////////////////////////////////////
//
//	RegisterOutputBufferRing - we use a different ring for the clone,
//				   as allocated when the clone was registered.
//


ManifestorStatus_t   Manifestor_Clone_c::RegisterOutputBufferRing(      Ring_t                    Ring )
{
    OriginalOutputRing	= Ring;

    if( CloneTo != NULL )
	CloneTo->RegisterOutputBufferRing( CloneOutputRing );

    return Original->RegisterOutputBufferRing( Ring );
}

// /////////////////////////////////////////////////////////////////////////
//
//      GetSurfaceParameters -  we always use the original classes surface
//


ManifestorStatus_t   Manifestor_Clone_c::GetSurfaceParameters(          void                    **SurfaceParameters )
{
    return Original->GetSurfaceParameters( SurfaceParameters );
}

// /////////////////////////////////////////////////////////////////////////
//
//      GetNextQueuedManifestationTime - we always use the original classes value
//


ManifestorStatus_t   Manifestor_Clone_c::GetNextQueuedManifestationTime(unsigned long long       *Time)
{
    return Original->GetNextQueuedManifestationTime( Time );
}

// /////////////////////////////////////////////////////////////////////////
//
//      ReleaseQueuedDecodeBuffers
//


ManifestorStatus_t   Manifestor_Clone_c::ReleaseQueuedDecodeBuffers(    void )
{
    if( CloneTo != NULL )
	CloneTo->ReleaseQueuedDecodeBuffers();

    return Original->ReleaseQueuedDecodeBuffers();
}

// /////////////////////////////////////////////////////////////////////////
//
//      InitialFrame
//


ManifestorStatus_t   Manifestor_Clone_c::InitialFrame(                  Buffer_t                  Buffer )
{
ManifestorStatus_t	Status;

    if( CloneTo != NULL )
    {
	Buffer->IncrementReferenceCount( IdentifierManifestorClone );

	Status	= CloneTo->InitialFrame( Buffer );

	if( Status != ManifestorNoError )
	    Buffer->DecrementReferenceCount( IdentifierManifestorClone );
    }

    return Original->InitialFrame( Buffer );
}

// /////////////////////////////////////////////////////////////////////////
//
//      QueueDecodeBuffer - this is the actual cloning part
//


ManifestorStatus_t   Manifestor_Clone_c::QueueDecodeBuffer(             Buffer_t                  Buffer )
{
ManifestorStatus_t	Status;

    if( CloneTo != NULL )
    {
	Buffer->IncrementReferenceCount( IdentifierManifestorClone );

	Status	= CloneTo->QueueDecodeBuffer( Buffer );

	if( Status != ManifestorNoError )
	    Buffer->DecrementReferenceCount( IdentifierManifestorClone );
    }

    return Original->QueueDecodeBuffer( Buffer );
}

// /////////////////////////////////////////////////////////////////////////
//
//      QueueNullManifestation
//


ManifestorStatus_t   Manifestor_Clone_c::QueueNullManifestation(        void )
{
    if( CloneTo != NULL )
	CloneTo->QueueNullManifestation();

    return Original->QueueNullManifestation();
}

// /////////////////////////////////////////////////////////////////////////
//
//      QueueEventSignal - we do not want the clone to do this, it would 
//			   interfere with the correct functioning of the player
//


ManifestorStatus_t   Manifestor_Clone_c::QueueEventSignal(              PlayerEventRecord_t      *Event )
{
    return Original->QueueEventSignal( Event );
}

// /////////////////////////////////////////////////////////////////////////
//
//      GetNativeTimeOfCurrentlyManifestedFrame - apply only to ther original class
//


ManifestorStatus_t   Manifestor_Clone_c::GetNativeTimeOfCurrentlyManifestedFrame(unsigned long long *Time)
{
    return Original->GetNativeTimeOfCurrentlyManifestedFrame( Time );
}

// /////////////////////////////////////////////////////////////////////////
//
//      GetDecodeBuffer - we use the original class as the source of buffers
//


ManifestorStatus_t   Manifestor_Clone_c::GetDecodeBuffer( 
							BufferStructure_t	 *RequestedStructure,
							Buffer_t		 *Buffer )
{
    return Original->GetDecodeBuffer( RequestedStructure, Buffer );
}

// /////////////////////////////////////////////////////////////////////////
//
//      GetDecodeBufferCount - we use the original class as the source of buffers
//


ManifestorStatus_t   Manifestor_Clone_c::GetDecodeBufferCount(          unsigned int             *Count )
{
    return Original->GetDecodeBufferCount( Count );
}

// /////////////////////////////////////////////////////////////////////////
//
//      SynchronizeOutput
//


ManifestorStatus_t   Manifestor_Clone_c::SynchronizeOutput(             void )
{
    if( CloneTo != NULL )
	CloneTo->SynchronizeOutput();

    return Original->SynchronizeOutput();
}

// /////////////////////////////////////////////////////////////////////////
//
//      GetFrameCount - we use the original class as the source of this
//


ManifestorStatus_t   Manifestor_Clone_c::GetFrameCount(                 unsigned long long       *FrameCount)
{
    return Original->GetFrameCount( FrameCount );
}


// /////////////////////////////////////////////////////////////////////////
//
//      The clone management function responsible for attaching a clone
//

ManifestorStatus_t   Manifestor_Clone_c::SetCloneTo(			Manifestor_t		  CloneTo )
{
OS_Status_t	OSStatus;

    //
    // Shutdown any running clone
    //

    if( this->CloneTo != NULL )
    {
	this->CloneTo->ReleaseQueuedDecodeBuffers();

	this->CloneTo		= NULL;
	CloneOutputRing->Insert( NULL );

	while( BufferReleaseThreadRunning )
	    OS_SleepMilliSeconds( 5 );

	delete CloneOutputRing;
	CloneOutputRing		= NULL;
	BufferReleaseThreadId	= OS_INVALID_THREAD;
    }

    //
    // Switch to the new clone
    //

    if( CloneTo != NULL )
    {
	//
	// Create the output ring and the process to handle it
	//

	CloneOutputRing		= new RingGeneric_c;		// Default ring size is plenty large enough
	if( (CloneOutputRing == NULL) || (CloneOutputRing->InitializationStatus != RingNoError) )
	{
	    report( severity_error, "Manifestor_Clone_c::SetCloneTo - Unable to create clone output ring\n" );
	    return PlayerInsufficientMemory;
	}

	OSStatus	= OS_CreateThread( &BufferReleaseThreadId, BufferReleaseThreadStub, this, "Manifestor Clone Buffer Release Thread", OS_MID_PRIORITY+16 );

	if( (OSStatus == OS_NO_ERROR) && !BufferReleaseThreadRunning )		// Given priority should run immediately but you never can tell
	    OS_SleepMilliSeconds( 5 );

	if( (OSStatus != OS_NO_ERROR) || !BufferReleaseThreadRunning )
	{
	    report( severity_error, "Manifestor_Clone_c::SetCloneTo - Failed to start buffer release thread\n" );

	    delete CloneOutputRing;
	    CloneOutputRing	= NULL;

	    return ManifestorError;
	}

	//
	// Catch up with any initialization
	//

	CloneTo->RegisterPlayer( Player, Playback, Stream, Collator, FrameParser, Codec, OutputTimer, Manifestor );

	if( OriginalOutputRing != NULL )
	    CloneTo->RegisterOutputBufferRing( CloneOutputRing );

//

	this->CloneTo		= CloneTo;
    }

//

    return ManifestorNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
//      The clone management function responsible for attaching a clone
//

ManifestorStatus_t   Manifestor_Clone_c::GetManifestors(	Manifestor_t		  *Original,
								Manifestor_t		  *CloneTo )
{
    if( Original != NULL )
	*Original	= this->Original;

    if( CloneTo != NULL )
	*CloneTo	= this->CloneTo;

    return ManifestorNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
//      The clone buffer release process
//

void   Manifestor_Clone_c::BufferReleaseThread(	void )
{
RingStatus_t	Status;
Buffer_t	Buffer;

//

    BufferReleaseThreadRunning		= true;

    while( true )
    {
	Status	= CloneOutputRing->Extract( (unsigned int *)(&Buffer), OS_INFINITE );

	if( Buffer == NULL )
	    break;

	Codec->ReleaseDecodeBuffer( Buffer );
    }

    BufferReleaseThreadRunning		= false;
}

