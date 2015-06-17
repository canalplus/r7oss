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

Source file name : player_playback.cpp
Author :           Nick

Implementation of the playback management related functions of the
generic class implementation of player 2


Date        Modification                                    Name
----        ------------                                    --------
06-Nov-06   Created                                         Nick

************************************************************************/

#include "player_generic.h"
#include "ring_generic.h"

// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Useful defines/macros that need not be user visible
//

#define DEFAULT_RING_SIZE       64

// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Static constant data
//

static const char *ProcessNames[4][4] = 
{
    { "Player None  0", "Player None  1", "Player None  2", "Player None  3" },
    { "Player Aud   0", "Player Aud   1", "Player Aud   2", "Player Aud   3" },
    { "Interocitor  0", "Interocitor  1", "Interocitor  2", "Interocitor  3" },
    { "Player Other 0", "Player Other 1", "Player Other 2", "Player Other 3" }
};


// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Create a new playback
//

PlayerStatus_t   Player_Generic_c::CreatePlayback(
						OutputCoordinator_t       OutputCoordinator,
						PlayerPlayback_t         *Playback,
						bool                      SignalEvent,
						void                     *EventUserData )
{
PlayerPlayback_t          NewPlayback;
PlayerStatus_t            Status;
PlayerEventRecord_t       Event;

//

    *Playback           = NULL;

//

    NewPlayback         = new struct PlayerPlayback_s;
    if( NewPlayback == NULL )
    {
	report( severity_error, "Player_Generic_c::CreatePlayback - Unable to allocate new playback structure.\n" );
	return PlayerInsufficientMemory;
    }

    memset( NewPlayback, 0x00, sizeof(struct PlayerPlayback_s) );

//

    NewPlayback->OutputCoordinator                              = OutputCoordinator;
    NewPlayback->ListOfStreams                                  = NULL;

    NewPlayback->Speed                                          = 1;
    NewPlayback->Direction                                      = PlayForward;

    NewPlayback->PresentationIntervalReversalLimitStartNormalizedTime 	= INVALID_TIME;
    NewPlayback->PresentationIntervalReversalLimitEndNormalizedTime	= INVALID_TIME;
    NewPlayback->RequestedPresentationIntervalStartNormalizedTime	= INVALID_TIME;
    NewPlayback->RequestedPresentationIntervalEndNormalizedTime 	= INVALID_TIME;

    NewPlayback->AudioCodedFrameCount				= AudioCodedFrameCount;
    NewPlayback->AudioCodedMemorySize				= AudioCodedMemorySize;
    NewPlayback->AudioCodedFrameMaximumSize			= AudioCodedFrameMaximumSize;
    memcpy( NewPlayback->AudioCodedMemoryPartitionName, AudioCodedMemoryPartitionName, ALLOCATOR_MAX_PARTITION_NAME_SIZE );
    NewPlayback->VideoCodedFrameCount				= VideoCodedFrameCount;
    NewPlayback->VideoCodedMemorySize				= VideoCodedMemorySize;
    NewPlayback->VideoCodedFrameMaximumSize			= VideoCodedFrameMaximumSize;
    memcpy( NewPlayback->VideoCodedMemoryPartitionName, VideoCodedMemoryPartitionName, ALLOCATOR_MAX_PARTITION_NAME_SIZE );
    NewPlayback->OtherCodedFrameCount				= OtherCodedFrameCount;
    NewPlayback->OtherCodedMemorySize				= OtherCodedMemorySize;
    NewPlayback->OtherCodedFrameMaximumSize			= OtherCodedFrameMaximumSize;
    memcpy( NewPlayback->OtherCodedMemoryPartitionName, OtherCodedMemoryPartitionName, ALLOCATOR_MAX_PARTITION_NAME_SIZE );

    SetLastNativeTime( NewPlayback, INVALID_TIME );

    OS_LockMutex( &Lock );
    NewPlayback->Next                                           = ListOfPlaybacks;
    ListOfPlaybacks                                             = NewPlayback;
    OS_UnLockMutex( &Lock );

//

    NewPlayback->OutputCoordinator->RegisterPlayer( this, NewPlayback, PlayerAllStreams );

//

    *Playback                           = NewPlayback;

    if( SignalEvent )
    {
	Event.Code              = EventPlaybackCreated;
	Event.Playback          = *Playback;
	Event.Stream            = PlayerAllStreams;
	Event.PlaybackTime      = TIME_NOT_APPLICABLE;
	Event.UserData          = EventUserData;

	Status                  = this->SignalEvent( &Event );
	if( Status != PlayerNoError )
	{
	    report( severity_error, "Player_Generic_c::CreatePlayback - Failed to signal event.\n" );
	    return PlayerError;
	}
    }

    return PlayerNoError;
}


// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Delete a playback
//

PlayerStatus_t   Player_Generic_c::TerminatePlayback(
						PlayerPlayback_t          Playback,
						bool                      SignalEvent,
						void                     *EventUserData )
{
PlayerStatus_t            Status;
PlayerEventRecord_t       Event;
PlayerPlayback_t         *PointerToPlayback;

    //
    // Commence drain on all playing streams
    //

    Status      = InternalDrainPlayback( Playback, PolicyPlayoutOnTerminate, false );
    if( Status != PlayerNoError )
	report( severity_error, "Player_Generic_c::TerminatePlayback - Failed to drain streams.\n" );

    //
    // Clean up the structures
    //

    while( Playback->ListOfStreams != NULL  )
    {
	Status  = CleanUpAfterStream( Playback->ListOfStreams );
	if( Status != PlayerNoError )
	    report( severity_error, "Player_Generic_c::TerminatePlayback - Failed to clean up after stream.\n" );
    }

    //
    // Is there any event to signal
    //

    if( SignalEvent )
    {
	Event.Code              = EventPlaybackTerminated;
	Event.Playback          = Playback;
	Event.Stream            = PlayerAllStreams;
	Event.PlaybackTime      = TIME_NOT_APPLICABLE;
	Event.UserData          = EventUserData;

	Status  = this->SignalEvent( &Event );
	if( Status != PlayerNoError )
	{
	    report( severity_error, "Player_Generic_c::TerminatePlayback - Failed to signal event.\n" );
	    return PlayerError;
	}
    }

    for( PointerToPlayback       = &ListOfPlaybacks;
	 (*PointerToPlayback)   != NULL;
	 PointerToPlayback       = &((*PointerToPlayback)->Next) )
	if( (*PointerToPlayback) == Playback )
	{
	    (*PointerToPlayback)  = Playback->Next;
	    break;
	}

    delete Playback;
    Playback = NULL;

//

    return PlayerNoError;
}


// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Add a stream to an existing playback
//

PlayerStatus_t   Player_Generic_c::AddStream(   PlayerPlayback_t          Playback,
						PlayerStream_t           *Stream,
						PlayerStreamType_t        StreamType,
						Collator_t                Collator,
						FrameParser_t             FrameParser,
						Codec_t                   Codec,
						OutputTimer_t             OutputTimer,
						Manifestor_t              Manifestor,
						bool                      SignalEvent,
						void                     *EventUserData )
{
PlayerStream_t            NewStream;
PlayerStatus_t            Status;
OS_Status_t               OSStatus;
OS_Thread_t               Thread;
unsigned int              Count;
PlayerEventRecord_t       Event;

    //
    // Check this is reasonable
    //

    if( StreamType == StreamTypeNone )
    {
	report( severity_error, "Player_Generic_c::AddStream - Stream type must be specified (not StreamTypeNone).\n" );
	return PlayerError;
    }

//

    if( (Manifestor == NULL) && SignalEvent )
    {
	report( severity_error, "Player_Generic_c::AddStream - Request event on first manifestation, when no frames are to be manifested.\n" );
	return PlayerError;
    }

    //
    // Create new stream
    //

    NewStream   = new struct PlayerStream_s;
    if( NewStream == NULL )
    {
	report( severity_error, "Player_Generic_c::AddStream - Unable to allocate new stream structure.\n" );
	return PlayerInsufficientMemory;
    }

    memset( NewStream, 0x00, sizeof(struct PlayerStream_s) );

    //
    // Initialize the stream structure
    //

    NewStream->Player                   	= this;
    NewStream->Playback                 	= Playback;
    NewStream->StreamType               	= StreamType;
    NewStream->Collator                 	= Collator;
    NewStream->FrameParser              	= FrameParser;
    NewStream->Codec                    	= Codec;
    NewStream->OutputTimer              	= OutputTimer;
    NewStream->Manifestor               	= Manifestor;

    NewStream->Demultiplexor            	= NULL;

    NewStream->UnPlayable			= false;

    NewStream->Terminating              	= false;
    NewStream->ProcessRunningCount      	= 0;
    NewStream->ExpectedProcessCount     	= (Manifestor != NULL) ? 4 : 2;
    OS_InitializeEvent( &NewStream->StartStopEvent );
    OS_InitializeEvent( &NewStream->Drained );


    NewStream->CodecReset			= false;
    NewStream->SwitchStreamInProgress		= false;
    OS_InitializeEvent( &NewStream->SwitchStreamLastOneOutOfTheCodec );

    NewStream->Step				= false;
    OS_InitializeEvent( &NewStream->SingleStepMayHaveHappened );

    NewStream->MarkerInCodedFrameIndex  	= INVALID_INDEX;
    NewStream->NextBufferSequenceNumber 	= 0;

    NewStream->ReTimeQueuedFrames       	= false;

    NewStream->InsertionsIntoNonDecodedBuffers	= 0;
    NewStream->RemovalsFromNonDecodedBuffers	= 0;
    NewStream->DisplayIndicesCollapse		= 0;

    NewStream->RequestedPresentationIntervalStartNormalizedTime	= Playback->RequestedPresentationIntervalStartNormalizedTime;
    NewStream->RequestedPresentationIntervalEndNormalizedTime	= Playback->RequestedPresentationIntervalEndNormalizedTime;

    //
    // Get the coded frame parameters
    //

    switch( StreamType )
    {
	case StreamTypeAudio:
		NewStream->CodedFrameBufferType		= BufferCodedFrameBufferType;
		NewStream->CodedFrameCount		= Playback->AudioCodedFrameCount;
		NewStream->CodedMemorySize		= Playback->AudioCodedMemorySize;
		NewStream->CodedFrameMaximumSize	= Playback->AudioCodedFrameMaximumSize;
		memcpy( NewStream->CodedMemoryPartitionName, Playback->AudioCodedMemoryPartitionName, ALLOCATOR_MAX_PARTITION_NAME_SIZE );
		break;

	case StreamTypeVideo:
		NewStream->CodedFrameBufferType		= BufferCodedFrameBufferType;
		NewStream->CodedFrameCount		= Playback->VideoCodedFrameCount;
		NewStream->CodedMemorySize		= Playback->VideoCodedMemorySize;
		NewStream->CodedFrameMaximumSize	= Playback->VideoCodedFrameMaximumSize;
		memcpy( NewStream->CodedMemoryPartitionName, Playback->VideoCodedMemoryPartitionName, ALLOCATOR_MAX_PARTITION_NAME_SIZE );
		break;

	case StreamTypeOther:
		NewStream->CodedFrameBufferType		= BufferCodedFrameBufferType;
		NewStream->CodedFrameCount		= Playback->OtherCodedFrameCount;
		NewStream->CodedMemorySize		= Playback->OtherCodedMemorySize;
		NewStream->CodedFrameMaximumSize	= Playback->OtherCodedFrameMaximumSize;
		memcpy( NewStream->CodedMemoryPartitionName, Playback->OtherCodedMemoryPartitionName, ALLOCATOR_MAX_PARTITION_NAME_SIZE );
		break;

	default:
		break;
    }

    //
    // Register the player to the child classes, they will require it early,
    // these function do no work, so cannot return a fail status.
    //

    Collator->RegisterPlayer( this, Playback, NewStream, Collator, FrameParser, Codec, OutputTimer, Manifestor );
    FrameParser->RegisterPlayer( this, Playback, NewStream, Collator, FrameParser, Codec, OutputTimer, Manifestor );
    Codec->RegisterPlayer( this, Playback, NewStream, Collator, FrameParser, Codec, OutputTimer, Manifestor );
    OutputTimer->RegisterPlayer( this, Playback, NewStream, Collator, FrameParser, Codec, OutputTimer, Manifestor );
    if( Manifestor != NULL )
	Manifestor->RegisterPlayer( this, Playback, NewStream, Collator, FrameParser, Codec, OutputTimer, Manifestor );

    //
    // Get the buffer pools, and attach NOTE the get's provoke creation of the pools.
    // the sequence numbers to them
    //

    Status      = GetCodedFrameBufferPool( NewStream, NULL, NULL );
    if( Status != PlayerNoError )
    {
	report( severity_error, "Player_Generic_c::AddStream - Failed to get the coded buffer pool.\n" );
	CleanUpAfterStream( NewStream );
	return Status;
    }

    Status      = NewStream->CodedFrameBufferPool->AttachMetaData( MetaDataSequenceNumberType );
    if( Status != PlayerNoError )
    {
	report( severity_error, "Player_Generic_c::AddStream - Failed to get attach sequence numbers to the coded buffer pool.\n" );
	CleanUpAfterStream( NewStream );
	return Status;
    }

//

    if( Manifestor != NULL )
    {
	Status = Manifestor->GetDecodeBufferPool( &NewStream->DecodeBufferPool );
	if( Status != PlayerNoError )
	{
	    report( severity_error, "Player_Generic_c::AddStream - Failed to get the decode buffer pool.\n" );
	    CleanUpAfterStream( NewStream );
	    return Status;
	}

	Status = Manifestor->GetPostProcessControlBufferPool( &NewStream->PostProcessControlBufferPool );
	if( Status != PlayerNoError )
	{
	    report( severity_error, "Player_Generic_c::AddStream - Failed to get the post processing control buffer pool.\n" );
	    CleanUpAfterStream( NewStream );
	    return Status;
	}
    }

    //
    // Create the rings needed to hold the buffers
    // during inter process communications.
    //

    NewStream->CollatedFrameRing        = new RingGeneric_c( NewStream->CodedFrameCount + PLAYER_MAX_CONTROL_STRUCTURE_BUFFERS );
    if( (NewStream->CollatedFrameRing == NULL) || (NewStream->CollatedFrameRing->InitializationStatus != RingNoError) )
    {
	report( severity_error, "Player_Generic_c::AddStream - Unable to create interprocess collated frame ring.\n" );
	CleanUpAfterStream( NewStream );
	return PlayerInsufficientMemory;
    }

    NewStream->ParsedFrameRing  = new RingGeneric_c( NewStream->CodedFrameCount + PLAYER_MAX_CONTROL_STRUCTURE_BUFFERS );
    if( (NewStream->ParsedFrameRing == NULL) || (NewStream->ParsedFrameRing->InitializationStatus != RingNoError) )
    {
	report( severity_error, "Player_Generic_c::AddStream - Unable to create interprocess parsed frame ring.\n" );
	CleanUpAfterStream( NewStream );
	return PlayerInsufficientMemory;
    }

//

    if( Manifestor != NULL )
    {
	NewStream->DecodeBufferPool->GetType( &NewStream->DecodeBufferType );
	NewStream->DecodeBufferPool->GetPoolUsage( &Count, NULL, NULL, NULL, NULL );
	if( Count == 0 )
	    Count       = DEFAULT_RING_SIZE;

	NewStream->NumberOfDecodeBuffers        = Count;
	if( NewStream->NumberOfDecodeBuffers > PLAYER_MAX_DECODE_BUFFERS )
	{
	    report( severity_error, "Player_Generic_c::AddStream - Too many decode buffers - Implementation constant range error.\n" );
	    CleanUpAfterStream( NewStream );
	    return PlayerImplementationError;
	}

	NewStream->DecodedFrameRing     = new RingGeneric_c( Count + PLAYER_MAX_CONTROL_STRUCTURE_BUFFERS );
	if( (NewStream->DecodedFrameRing == NULL) || (NewStream->DecodedFrameRing->InitializationStatus != RingNoError) )
	{
	    report( severity_error, "Player_Generic_c::AddStream - Unable to create interprocess decoded frame ring.\n" );
	    CleanUpAfterStream( NewStream );
	    return PlayerInsufficientMemory;
	}

	NewStream->ManifestedBufferRing = new RingGeneric_c( Count + PLAYER_MAX_CONTROL_STRUCTURE_BUFFERS );
	if( (NewStream->ManifestedBufferRing == NULL) || (NewStream->ManifestedBufferRing->InitializationStatus != RingNoError) )
	{
	    report( severity_error, "Player_Generic_c::AddStream - Unable to create interprocess manifested frame ring.\n" );
	    CleanUpAfterStream( NewStream );
	    return PlayerInsufficientMemory;
	}
    }

    //
    // Create the tasks that pass data bewteen components, 
    // and provide them with a context in which to operate.
    // NOTE Since we are unsure about the startup, we use a 
    // simple delay to shut down the threads if startup is not 
    // as expected.
    //

    OS_ResetEvent( &NewStream->StartStopEvent );

    OSStatus            = OS_CreateThread( &Thread, PlayerProcessCollateToParse,   NewStream, ProcessNames[StreamType][0], (OS_MID_PRIORITY+8) );

    if( OSStatus == OS_NO_ERROR )
	OSStatus        = OS_CreateThread( &Thread, PlayerProcessParseToDecode,    NewStream, ProcessNames[StreamType][1], (OS_MID_PRIORITY+8) );

    if( (OSStatus == OS_NO_ERROR) && (Manifestor != NULL) )
	OSStatus        = OS_CreateThread( &Thread, PlayerProcessDecodeToManifest, NewStream, ProcessNames[StreamType][2], (OS_MID_PRIORITY+10) );

    if( (OSStatus == OS_NO_ERROR) && (Manifestor != NULL) )
	OSStatus        = OS_CreateThread( &Thread, PlayerProcessPostManifest,     NewStream, ProcessNames[StreamType][3], (OS_MID_PRIORITY+12) );

//

    if( OSStatus != OS_NO_ERROR )
    {
	report( severity_error, "Player_Generic_c::AddStream - Failed to create stream processes.\n" );
	NewStream->Terminating  = true;
	OS_SleepMilliSeconds( 2 * PLAYER_MAX_EVENT_WAIT );
	NewStream->ProcessRunningCount  = 0;
	CleanUpAfterStream( NewStream );
	return PlayerError;
    }

//

    OS_WaitForEvent( &NewStream->StartStopEvent, PLAYER_MAX_EVENT_WAIT );

    if(  NewStream->ProcessRunningCount != NewStream->ExpectedProcessCount )
    {
	report( severity_error, "Player_Generic_c::AddStream - Stream processes failed to run.\n" );
	NewStream->Terminating  = true;
	OS_SleepMilliSeconds( 2 * PLAYER_MAX_EVENT_WAIT );
	NewStream->ProcessRunningCount  = 0;
	CleanUpAfterStream( NewStream );
	return PlayerError;
    }

    //
    // Insert the stream into the playback list at this point
    //

    OS_LockMutex( &Lock );

    NewStream->Next             = Playback->ListOfStreams;
    Playback->ListOfStreams     = NewStream;

    OS_UnLockMutex( &Lock );

    //
    // Now exchange the appropriate information between the classes
    //

    Status      = Collator->RegisterOutputBufferRing( NewStream->CollatedFrameRing );
    if( Status != PlayerNoError )
    {
	report( severity_error, "Player_Generic_c::AddStream - Failed to register stream parameters with Collator.\n" );
	CleanUpAfterStream( NewStream );
	return Status;
    }

//

    Status      = FrameParser->RegisterOutputBufferRing( NewStream->ParsedFrameRing );
    if( Status != PlayerNoError )
    {
	report( severity_error, "Player_Generic_c::AddStream - Failed to register stream parameters with Frame Parser.\n" );
	CleanUpAfterStream( NewStream );
	return Status;
    }

//

    Status      = Codec->RegisterOutputBufferRing( NewStream->DecodedFrameRing );       /* NULL for no manifestor */
    if( Status != PlayerNoError )
    {
	report( severity_error, "Player_Generic_c::AddStream - Failed to register stream parameters with Codec.\n" );
	CleanUpAfterStream( NewStream );
	return Status;
    }

//

    Status      = OutputTimer->RegisterOutputCoordinator( Playback->OutputCoordinator );
    if( Status != PlayerNoError )
    {
	report( severity_error, "Player_Generic_c::AddStream - Failed to register Output Coordinator with Output Timer.\n" );
	CleanUpAfterStream( NewStream );
	return Status;
    }

//

    if( Manifestor != NULL )
    {
	Status  = Manifestor->RegisterOutputBufferRing( NewStream->ManifestedBufferRing );
	if( Status != PlayerNoError )
	{
	    report( severity_error, "Player_Generic_c::AddStream - Failed to register stream parameters with Manifestor.\n" );
	    CleanUpAfterStream( NewStream );
	    return Status;
	}
    }

    //
    // Do we need to queue an event on display of first frame and stream created
    //

    if( SignalEvent )
    {
	Event.Code              = EventFirstFrameManifested;
	Event.Playback          = Playback;
	Event.Stream            = NewStream;
	Event.PlaybackTime      = TIME_NOT_APPLICABLE;
	Event.UserData          = EventUserData;

	Status  = CallInSequence( NewStream, SequenceTypeImmediate, TIME_NOT_APPLICABLE, ManifestorFnQueueEventSignal, &Event );
	if( Status != PlayerNoError )
	{
	    report( severity_error, "Player_Generic_c::AddStream - Failed to issue call to signal first frame event.\n" );
	    CleanUpAfterStream( NewStream );
	    return Status;
	}

	Event.Code              = EventStreamCreated;

	Status  = this->SignalEvent( &Event );
	if( Status != PlayerNoError )
	{
       	    report( severity_error, "Player_Generic_c::AddStream - Failed to issue stream created event.\n" );
	    CleanUpAfterStream( NewStream );
	    return Status;
	}
    }

//

    *Stream     = NewStream;

//

    return PlayerNoError;
}


// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Remove a stream from an existing playback
//

PlayerStatus_t   Player_Generic_c::RemoveStream(PlayerStream_t            Stream,
						bool                      SignalEvent,
						void                     *EventUserData )
{
PlayerStatus_t          Status;
PlayerPlayback_t	Playback;
PlayerEventRecord_t     Event;

    //
    // If we are attached to a demultiplexor, complain loudly
    //

    if( Stream->Demultiplexor != NULL )
    {
	report( severity_error, "Player_Generic_c::RemoveStream - Removing stream that is still attached to a demultiplexor - Implementation error.\n" );
	return PlayerImplementationError;
    }

    //
    // First drain the stream
    //

    Status      = InternalDrainStream( Stream, false, false, NULL, PolicyPlayoutOnTerminate, false );
    if( Status != PlayerNoError )
    {
	report( severity_error, "Player_Generic_c::RemoveStream - Failed to drain stream.\n" );
    }

    //
    // Since we blocked in the drain, we should now be able to shutdown the stream cleanly.
    //

    Playback	= Stream->Playback;
    Status      = CleanUpAfterStream( Stream );
    if( Status != PlayerNoError )
    {
	report( severity_error, "Player_Generic_c::RemoveStream - Failed to clean up after the stream.\n" );
    }

    //
    // Is there any event to signal
    //

    if( SignalEvent )
    {
	Event.Code              = EventStreamTerminated;
	Event.Playback          = Playback;
	Event.Stream            = Stream;
	Event.PlaybackTime      = TIME_NOT_APPLICABLE;
	Event.UserData          = EventUserData;

	Status  = this->SignalEvent( &Event );
	if( Status != PlayerNoError )
	{
	    report( severity_error, "Player_Generic_c::RemoveStream - Failed to signal event.\n" );
	    return PlayerError;
	}
    }

//

    return PlayerNoError;
}


// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Drain the decode chain for a stream in a playback
//

PlayerStatus_t   Player_Generic_c::DrainStream( PlayerStream_t            Stream,
						bool                      NonBlocking,
						bool                      SignalEvent,
						void                     *EventUserData )
{
    return InternalDrainStream( Stream, NonBlocking, SignalEvent, EventUserData, PolicyPlayoutOnDrain, false );
}


// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Private - Clean up when a stream needs to be shut down
//

PlayerStatus_t   Player_Generic_c::CleanUpAfterStream(  PlayerStream_t    Stream )
{
PlayerStream_t          *PointerToStream;
unsigned int             OwnerIdentifier;
Buffer_t                 Buffer;

    //
    // Shut down the processes
    //

    OS_ResetEvent( &Stream->StartStopEvent );

    Stream->DiscardingUntilMarkerFramePostM = true;
    Stream->DiscardingUntilMarkerFrameDtoM  = true;
    Stream->DiscardingUntilMarkerFramePtoD  = true;
    Stream->DiscardingUntilMarkerFrameCtoP  = true;

    Stream->Collator->DiscardAccumulatedData();
    Stream->Collator->Halt();

    Stream->FrameParser->Halt();

    Stream->Codec->OutputPartialDecodeBuffers();
    Stream->Codec->ReleaseReferenceFrame( CODEC_RELEASE_ALL );
    Stream->Codec->Halt();

    Stream->OutputTimer->Halt();

    if( Stream->Manifestor != NULL )
    {
	Stream->Manifestor->QueueNullManifestation();
	Stream->Manifestor->Halt();
    }

    Stream->Terminating         = true;

    OS_SetEvent( &Stream->SingleStepMayHaveHappened );		// Just in case they are waiting for a single step

    if( Stream->ProcessRunningCount != 0 )
    {
	OS_WaitForEvent( &Stream->StartStopEvent, 2 * PLAYER_MAX_EVENT_WAIT );

	if(  Stream->ProcessRunningCount != 0 )
	    report( severity_error, "Player_Generic_c::CleanUpAfterStream - Stream processes failed to terminate (%d).\n", Stream->ProcessRunningCount );
    }

    //
    // Strip the rings - NOTE we assume we are the first owner on any buffer
    //

    if( Stream->CollatedFrameRing != NULL )
    {
	while( Stream->CollatedFrameRing->NonEmpty() )
	{
	    Stream->CollatedFrameRing->Extract( (unsigned int *)(&Buffer) );
	    Buffer->GetOwnerList( 1, &OwnerIdentifier );
	    Buffer->DecrementReferenceCount( OwnerIdentifier );
	}

	delete Stream->CollatedFrameRing;
    }

//

    if( Stream->ParsedFrameRing != NULL )
    {
	while( Stream->ParsedFrameRing->NonEmpty() )
	{
	    Stream->ParsedFrameRing->Extract( (unsigned int *)(&Buffer) );
	    Buffer->GetOwnerList( 1, &OwnerIdentifier );
	    Buffer->DecrementReferenceCount( OwnerIdentifier );
	}

	delete Stream->ParsedFrameRing;
    }

//

    if( Stream->DecodedFrameRing != NULL )
    {
	while( Stream->DecodedFrameRing->NonEmpty() )
	{
	    Stream->DecodedFrameRing->Extract( (unsigned int *)(&Buffer) );
	    Buffer->GetOwnerList( 1, &OwnerIdentifier );
	    Buffer->DecrementReferenceCount( OwnerIdentifier );
	}

	delete Stream->DecodedFrameRing;
    }

//

    if( Stream->ManifestedBufferRing != NULL )
    {
	while( Stream->ManifestedBufferRing->NonEmpty() )
	{
	    Stream->ManifestedBufferRing->Extract( (unsigned int *)(&Buffer) );
	    Buffer->GetOwnerList( 1, &OwnerIdentifier );
	    Buffer->DecrementReferenceCount( OwnerIdentifier );
	}

	delete Stream->ManifestedBufferRing;
    }

    //
    // Extract from playback list
    //

    OS_LockMutex( &Lock );

    for( PointerToStream         = &Stream->Playback->ListOfStreams;
	 (*PointerToStream)     != NULL;
	 PointerToStream         = &((*PointerToStream)->Next) )
	if( (*PointerToStream) == Stream )
	{
	    (*PointerToStream)  = Stream->Next;
	    break;
	}

    OS_UnLockMutex( &Lock );

    //
    // Now reset the individual classes
    //

    Stream->Collator->Reset();
    Stream->FrameParser->Reset();

    Stream->CodecReset	= true;
    Stream->Codec->Reset();

    Stream->OutputTimer->Reset();
    Stream->Manifestor->Reset();

    OS_TerminateEvent( &Stream->StartStopEvent );
    OS_TerminateEvent( &Stream->Drained );
    OS_TerminateEvent( &Stream->SwitchStreamLastOneOutOfTheCodec );
    OS_TerminateEvent( &Stream->SingleStepMayHaveHappened );

    //
    // If the playback no longer has any streams, we reset the 
    // output coordinator refreshing the registration of the player.
    //

    if( Stream->Playback->ListOfStreams == NULL )
    {
	Stream->Playback->OutputCoordinator->Halt();
	Stream->Playback->OutputCoordinator->Reset();
	Stream->Playback->OutputCoordinator->RegisterPlayer( this, Stream->Playback, PlayerAllStreams );
    }

    //
    // Delete the code frame buffer pool
    //

    if( Stream->CodedFrameBufferPool != NULL )
    {
        BufferManager->DestroyPool( Stream->CodedFrameBufferPool );
        Stream->CodedFrameBufferPool  = NULL;
    }

    if( Stream->CodedFrameMemory[CachedAddress] != NULL )
    {
#if __KERNEL__
        AllocatorClose( Stream->CodedFrameMemoryDevice );
#endif

        Stream->CodedFrameMemory[CachedAddress]         = NULL;
        Stream->CodedFrameMemory[UnCachedAddress]       = NULL;
        Stream->CodedFrameMemory[PhysicalAddress]       = NULL;
    }

    //
    // Delete the stream structure
    //

    delete Stream;

//

    return PlayerNoError;
}


// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Drain the decode chain for a stream in a playback
//

PlayerStatus_t   Player_Generic_c::InternalDrainPlayback(       
						PlayerPlayback_t          Playback,
						PlayerPolicy_t            PlayoutPolicy,
						bool                      ParseAllFrames )
{
PlayerStatus_t            Status;
bool			  Discard;
unsigned char		  PlayoutPolicyValue;
PlayerStream_t            Stream;

    //
    // Commence drain on all playing streams (use non-blocking on individuals)
    //

    Discard		= false;

    for( Stream  = Playback->ListOfStreams;
	 Stream != NULL;
	 Stream  = Stream->Next )
    {
	PlayoutPolicyValue	= PolicyValue( Stream->Playback, Stream, PlayoutPolicy );
	if( PlayoutPolicyValue == PolicyValueDiscard )
	    Discard		= true;

	Status  = InternalDrainStream( Stream, true, false, NULL, PlayoutPolicy, ParseAllFrames );
	if( Status != PlayerNoError )
	    report( severity_error, "Player_Generic_c::InternalDrainPlayback - Failed to drain stream.\n" );
    }

    //
    // Start waiting for the drains to complete, and removing the appropriate data
    //

    for( Stream  = Playback->ListOfStreams;
	 Stream != NULL;
	 Stream  = Stream->Next )
    {
	Status		= WaitForDrainCompletion( Stream, Discard );

	if( Status != PlayerNoError )
	{
	    report( severity_error, "Player_Generic_c::InternalDrainPlayback - Failed to drain within allowed time (%d %d %d %d) - Implementation error.\n",
			Stream->DiscardingUntilMarkerFrameCtoP, Stream->DiscardingUntilMarkerFramePtoD,
			Stream->DiscardingUntilMarkerFrameDtoM, Stream->DiscardingUntilMarkerFramePostM );
	    break;
	}
    }

//

    return PlayerNoError;
}


// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Drain the decode chain for a stream in a playback
//

PlayerStatus_t   Player_Generic_c::InternalDrainStream( 
						PlayerStream_t            Stream,
						bool                      NonBlocking,
						bool                      SignalEvent,
						void                     *EventUserData,
						PlayerPolicy_t            PlayoutPolicy,
						bool                      ParseAllFrames )
{
PlayerStatus_t            Status;
unsigned char             PlayoutPolicyValue;
PlayerEventRecord_t       Event;
Buffer_t                  MarkerFrame;
CodedFrameParameters_t   *CodedFrameParameters;
PlayerSequenceNumber_t   *SequenceNumberStructure;
unsigned long long	  PlayoutTime;
unsigned long long	  Delay;

    //
    // Read the appropriate policy
    //

    PlayoutPolicyValue  = PolicyValue( Stream->Playback, Stream, PlayoutPolicy );

    //
    // If we are to discard data in the drain, then perform the flushing
    //

    if( PlayoutPolicyValue == PolicyValueDiscard )
    {
	Stream->DiscardingUntilMarkerFramePostM = true;
	Stream->DiscardingUntilMarkerFrameDtoM  = true;
	Stream->DiscardingUntilMarkerFramePtoD  = true;
	Stream->DiscardingUntilMarkerFrameCtoP  = !ParseAllFrames;

	Stream->ReTimeQueuedFrames              = false;
	OS_SetEvent( &Stream->SingleStepMayHaveHappened );

	Status  = Stream->Collator->InputJump( true, false );
	Status  = Stream->Codec->DiscardQueuedDecodes();
	Status	= Stream->Codec->OutputPartialDecodeBuffers();
	Status  = Stream->Manifestor->ReleaseQueuedDecodeBuffers();
    }

    //
    // Just in case there is another marker frame doing the rounds
    //

    if( Stream->MarkerInCodedFrameIndex != INVALID_INDEX )
    {
	Status	= WaitForDrainCompletion( Stream, (PlayoutPolicyValue == PolicyValueDiscard) );
	if( Status != PlayerNoError )
	{
	    report( severity_error, "Player_Generic_c::InternalDrainStream - Unable to launch a marker frame - Implementation error.\n" );
	    MarkerFrame->DecrementReferenceCount();
	    return PlayerImplementationError;
	}
    }

    //
    // Insert a marker frame into the processing ring
    //

    Status      = Stream->CodedFrameBufferPool->GetBuffer( &MarkerFrame, IdentifierDrain, 0 );
    if( Status != BufferNoError )
    {
	report( severity_error, "Player_Generic_c::InternalDrainStream - Unable to obtain a marker frame.\n" );
	return Status;
    }

//

    Status      = MarkerFrame->ObtainMetaDataReference( MetaDataCodedFrameParametersType, (void **)(&CodedFrameParameters) );
    if( Status != BufferNoError )
    {
	report( severity_error, "Player_Generic_c::InternalDrainStream - Unable to obtain the meta data coded frame parameters.\n" );
	MarkerFrame->DecrementReferenceCount();
	return Status;
    }

    memset( CodedFrameParameters, 0x00, sizeof(CodedFrameParameters_t) );

//

    Status      = MarkerFrame->ObtainMetaDataReference( MetaDataSequenceNumberType, (void **)(&SequenceNumberStructure) );
    if( Status != PlayerNoError )
    {
	report( severity_error, "Player_Generic_c::InternalDrainStream - Unable to obtain the meta data \"SequenceNumber\" - Implementation error\n" );
	MarkerFrame->DecrementReferenceCount();
	return Status;
    }

    Stream->DrainSequenceNumber                 = Stream->NextBufferSequenceNumber + PLAYER_MAX_RING_SIZE;
    SequenceNumberStructure->MarkerFrame        = true;
    SequenceNumberStructure->Value              = Stream->DrainSequenceNumber;

    //
    // Reset the event indicating draining and insert the marker into the flow
    //

    OS_ResetEvent( &Stream->Drained );

    MarkerFrame->GetIndex( &Stream->MarkerInCodedFrameIndex );
    Stream->CollatedFrameRing->Insert( (unsigned int)MarkerFrame );

    //
    // Issue an in sequence synchronization reset
    //

    Status  = CallInSequence( Stream, SequenceTypeBeforeSequenceNumber, Stream->DrainSequenceNumber, OutputTimerFnResetTimeMapping, PlaybackContext );
    if( Status != PlayerNoError )
    {
	report( severity_error, "Player_Generic_c::InternalDrainStream - Failed to issue call to reset synchronization.\n" );
	return Status;
    }

    //
    // Do we want to raise a signal on drain completion
    //

    if( SignalEvent )
    {
	Event.Code              = EventStreamDrained;
	Event.Playback          = Stream->Playback;
	Event.Stream            = Stream;
	Event.PlaybackTime      = TIME_NOT_APPLICABLE;
	Event.UserData          = EventUserData;

	Status  = CallInSequence( Stream, SequenceTypeBeforeSequenceNumber, Stream->DrainSequenceNumber, ManifestorFnQueueEventSignal, &Event );
	if( Status != PlayerNoError )
	{
	    report( severity_error, "Player_Generic_c::InternalDrainStream - Failed to issue call to signal drain completion.\n" );
	    return Status;
	}
    }

    //
    // Queue the setting of the internal event, when the stream is drained
    // this allows us to commence multiple stream drains in a playback shutdown
    // and then block on completion of them all.
    //

    Status      = CallInSequence( Stream, SequenceTypeAfterSequenceNumber, Stream->DrainSequenceNumber, OSFnSetEventOnPostManifestation, &Stream->Drained );
    if( Status != PlayerNoError )
    {
	report( severity_error, "Player_Generic_c::InternalDrainStream - Failed to request OS_SetEvent.\n" );
	return Status;
    }

    //
    // Are we a blocking/non-blocking call
    //

    if( !NonBlocking )
    {
	Status	= WaitForDrainCompletion( Stream, (PlayoutPolicyValue == PolicyValueDiscard) );
	if( Status != PlayerNoError )
	{
	    report( severity_error, "Player_Generic_c::InternalDrainStream - Failed to drain within allowed time (%d %d %d %d).\n",
			Stream->DiscardingUntilMarkerFrameCtoP, Stream->DiscardingUntilMarkerFramePtoD,
			Stream->DiscardingUntilMarkerFrameDtoM, Stream->DiscardingUntilMarkerFramePostM );
	    return PlayerTimedOut;
	}

	//
	// If this was a playout drain, then check when the last 
	// queued frame will complete and wait for it to do so.
	//

	if( PlayoutPolicyValue == PolicyValuePlayout )
	{
	    //
	    // The last frame will playout when the next frame could be displayed
	    //

	    Status	= Stream->Manifestor->GetNextQueuedManifestationTime( &PlayoutTime );
	    Delay	= (PlayoutTime - OS_GetTimeInMicroSeconds()) / 1000;
	    if( (Status == ManifestorNoError) && inrange(Delay, 1, (unsigned long long)Abs(RoundedLongLongIntegerPart( PLAYER_MAX_PLAYOUT_TIME / Stream->Playback->Speed ))) )
	    {
		report( severity_info, "Player_Generic_c::InternalDrainStream - Delay to manifest last frame is %lldms\n", Delay );
		OS_SleepMilliSeconds( (unsigned int)Delay );
	    }
	}
    }

//

    return PlayerNoError;
}


// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Drain the decode chain for a stream in a playback
//

unsigned int   Player_Generic_c::WaitForDrainCompletion(PlayerStream_t		  Stream,
							bool			  Discard )
{
OS_Status_t	OSStatus;
unsigned int	IndividualWaitTime;

//

    if( Discard )
    {
	OSStatus	= OS_WaitForEvent( &Stream->Drained, PLAYER_MAX_DISCARD_DRAIN_TIME );
    }
    else
    {
	IndividualWaitTime	= Abs(RoundedIntegerPart( PLAYER_MAX_TIME_ON_DISPLAY / Stream->Playback->Speed ));
	do
	{
	    Stream->BuffersComingOutOfManifestation	= false;
	    OSStatus					= OS_WaitForEvent( &Stream->Drained, IndividualWaitTime );
	} while( (OSStatus != OS_NO_ERROR) && Stream->BuffersComingOutOfManifestation );
    }

    return (OSStatus == OS_NO_ERROR) ? PlayerNoError : PlayerTimedOut;
}


// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      A function to allow component to mark a stream as unplayable
//

PlayerStatus_t   Player_Generic_c::MarkStreamUnPlayable( PlayerStream_t		Stream )
{
unsigned char		  Ignore;
PlayerStatus_t		  Status;
PlayerEventRecord_t	  Event;

    //
    // First check if we should just ignore this call
    //

    Ignore  = PolicyValue( Stream->Playback, Stream, PolicyIgnoreStreamUnPlayableCalls );
    if( Ignore == PolicyValueApply )
    {
	report( severity_info, "Player_Generic_c::MarkStreamUnPlayable - Ignoring call.\n" );
	return PlayerNoError;
    }

    //
    // Mark the stream as unplayable
    //

    Stream->UnPlayable	= true;

    //
    // Drain the stream
    //

    Status      = InternalDrainStream( Stream, true, false, NULL, (PlayerPolicy_t)PolicyPlayoutAlwaysDiscard, false );
    if( Status != PlayerNoError )
    {
	report( severity_error, "Player_Generic_c::MarkStreamUnPlayable - Failed to drain stream.\n" );
    }

    //
    // raise the event to signal this to the user
    //

    Event.Code		= EventStreamUnPlayable;
    Event.Playback	= Stream->Playback;
    Event.Stream	= Stream;
    Event.PlaybackTime	= TIME_NOT_APPLICABLE;
    Event.UserData	= NULL;

    Status  = this->SignalEvent( &Event );
    if( Status != PlayerNoError )
    {
	report( severity_error, "Player_Generic_c::MarkStreamUnPlayable - Failed to signal event.\n" );
	return PlayerError;
    }

    return PlayerNoError;
}

