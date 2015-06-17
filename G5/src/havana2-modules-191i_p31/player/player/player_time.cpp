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

Source file name : player_time.cpp
Author :           Nick

Implementation of the time related functions of the
generic class implementation of player 2


Date        Modification                                    Name
----        ------------                                    --------
03-Nov-06   Created                                         Nick

************************************************************************/

#include "player_generic.h"

// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Useful defines/macros that need not be user visible
//

// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Set a playback speed.
//

PlayerStatus_t   Player_Generic_c::SetPlaybackSpeed(
						PlayerPlayback_t	  Playback,
						Rational_t		  Speed,
						PlayDirection_t		  Direction )
{
PlayerStatus_t		Status;
unsigned long long	Now;
unsigned long long	NormalizedTimeAtStartOfDrain;
bool			ReTimeQueuedFrames;
PlayerStream_t		Stream;
unsigned char		Policy;

    //
    // Ensure that the playback interval is not cluttered by reversal clamps
    //

    Playback->PresentationIntervalReversalLimitStartNormalizedTime	= INVALID_TIME;
    Playback->PresentationIntervalReversalLimitEndNormalizedTime	= INVALID_TIME;

    //
    // Are we performing a direction flip
    //

    if( Playback->Direction != Direction )
    {
	//
	// Find the current playback time
	//

	Now	= OS_GetTimeInMicroSeconds();
	Status	= Playback->OutputCoordinator->TranslateSystemTimeToPlayback( PlaybackContext, Now, &NormalizedTimeAtStartOfDrain );
    	if( Status != PlayerNoError )
    	{
	    report( severity_error, "Player_Generic_c::SetPlaybackSpeed - Failed to translate system time to playback time.\n" );
	    NormalizedTimeAtStartOfDrain	= INVALID_TIME;
    	}

	//
	// Drain with prejudice, but ensuring all frames are parsed
	//

	if( Playback->Speed == 0 )
	    SetPlaybackSpeed( Playback, 1, Playback->Direction );

	InternalDrainPlayback( Playback, (PlayerPolicy_t)PolicyPlayoutAlwaysDiscard, true );

	//
	// Find the current frames on display, and clamp the play 
	// interval to ensure we don't go too far in the flip.
	//

	Policy	= PolicyValue( Playback, PlayerAllStreams, PolicyClampPlaybackIntervalOnPlaybackDirectionChange );
	if( Policy == PolicyValueApply )
	{
	    if( Direction == PlayForward )
		Playback->PresentationIntervalReversalLimitStartNormalizedTime	= NormalizedTimeAtStartOfDrain;
	    else
		Playback->PresentationIntervalReversalLimitEndNormalizedTime	= NormalizedTimeAtStartOfDrain;
	}
    }

    //
    // Do we need to re-time the queued frames
    //

    ReTimeQueuedFrames	= (Playback->Direction == Direction) &&
			  (Playback->Speed     != Speed);

    //
    // Record the new speed and direction
    //

    Playback->Speed	= Speed;
    Playback->Direction	= Direction;

    //
    // Specifically inform the output coordinator of the change
    //

    Status	= Playback->OutputCoordinator->SetPlaybackSpeed( PlaybackContext, Speed, Direction );
    if( Status != PlayerNoError )
    {
	report( severity_info, "Player_Generic_c::SetPlaybackSpeed - failed to inform output cordinator of speed change.\n" );
	return Status;
    }

    //
    // Perform queued frame re-timing, and release any single step waiters
    //


    for( Stream	 = Playback->ListOfStreams;
	 Stream	!= NULL;
	 Stream	 = Stream->Next )
    {
    	if( ReTimeQueuedFrames )
	{
	    Stream->ReTimeStart		= OS_GetTimeInMicroSeconds();
	    Stream->ReTimeQueuedFrames	= true;
	    Stream->Manifestor->ReleaseQueuedDecodeBuffers();
	}

	OS_SetEvent( &Stream->SingleStepMayHaveHappened );
    }

//

    return PlayerNoError;
}


// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Step a specific stream in a playback.
//

PlayerStatus_t   Player_Generic_c::StreamStep( 	PlayerStream_t		  Stream )
{
    report( severity_info, "Player_Generic_c::StreamStep - Step\n" );

    Stream->Step	= true;
    OS_SetEvent( &Stream->SingleStepMayHaveHappened );

    return PlayerNoError;
}


// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Set the native playback time for a playback.
//

PlayerStatus_t   Player_Generic_c::SetNativePlaybackTime(
						PlayerPlayback_t	  Playback,
						unsigned long long	  NativeTime,
						unsigned long long	  SystemTime )
{
PlayerStatus_t		Status;
unsigned long long	NormalizedTime;

//

#if 0
{
unsigned long long      Now     = OS_GetTimeInMicroSeconds();
report( severity_info, "Player_Generic_c::SetNativePlaybackTime - %016llx %016llx (%016llx)\n", NativeTime, SystemTime, Now );
} 
#endif

    if( NativeTime == INVALID_TIME )
    {
	//
	// Invalid, reset all time mappings
	//

	Playback->OutputCoordinator->ResetTimeMapping( PlaybackContext );
    }
    else
    {
	if( Playback->ListOfStreams == NULL )
	{
	    report( severity_error, "Player_Generic_c::SetNativePlaybackTime - No streams to map native time to normalized time on.\n" );
	    return PlayerUnknownStream;
	}
	
	Status	= Playback->ListOfStreams->FrameParser->TranslatePlaybackTimeNativeToNormalized( NativeTime, &NormalizedTime );
	if( Status != PlayerNoError )
	{
	    report( severity_error, "Player_Generic_c::SetNativePlaybackTime - Failed to translate native time to normalized.\n" );
	    return Status;
	}
	
	Status	= Playback->OutputCoordinator->EstablishTimeMapping( PlaybackContext, NormalizedTime, SystemTime );
	if( Status != PlayerNoError )
	{
	    report( severity_error, "Player_Generic_c::SetNativePlaybackTime - Failed to establish time mapping.\n" );
	    return Status;
	}
    }

    return PlayerNoError;
}


// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Retrieve the native playback time for a playback.
//

PlayerStatus_t   Player_Generic_c::RetrieveNativePlaybackTime(
						PlayerPlayback_t	  Playback,
						unsigned long long	 *NativeTime )
{
PlayerStatus_t		  Status;
unsigned long long	  Now;
unsigned long long	  NormalizedTime;

    //
    // What is the system time
    //

    Now		= OS_GetTimeInMicroSeconds();

    //
    // Translate that to playback time
    //

    Status	= Playback->OutputCoordinator->TranslateSystemTimeToPlayback( PlaybackContext, Now, &NormalizedTime );
    if( Status != PlayerNoError )
    {
	// This one happens quite frequently.
	// report( severity_error, "Player_Generic_c::RetrieveNativePlaybackTime - Failed to translate system time to playback time.\n" );
	return Status;
    }

    //
    // Translate that from normalized to native format
    //

    if( Playback->ListOfStreams == NULL )
    {
	report( severity_error, "Player_Generic_c::RetrieveNativePlaybackTime - No streams to map normalized time to native time on.\n" );
	return PlayerUnknownStream;
    }

    Status	= Playback->ListOfStreams->FrameParser->TranslatePlaybackTimeNormalizedToNative( NormalizedTime, NativeTime );
    if( Status != PlayerNoError )
    {
	report( severity_error, "Player_Generic_c::RetrieveNativePlaybackTime - Failed to translate normalized time to native time.\n" );
	return Status;
    }

    return PlayerNoError;
}


// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Translate the native playback time for a playback into system time.
//

PlayerStatus_t   Player_Generic_c::TranslateNativePlaybackTime(
						PlayerPlayback_t	  Playback,
						unsigned long long	  NativeTime,
						unsigned long long	 *SystemTime )
{
PlayerStatus_t		Status;
unsigned long long	NormalizedTime;

//

    if( NativeTime == INVALID_TIME )
    {
	*SystemTime	= INVALID_TIME;
    }

//

    else
    {
	if( Playback->ListOfStreams == NULL )
	{
	    report( severity_error, "Player_Generic_c::TranslateNativePlaybackTime - No streams to map native time to normalized time on.\n" );
	    return PlayerUnknownStream;
	}

	Status	= Playback->ListOfStreams->FrameParser->TranslatePlaybackTimeNativeToNormalized( NativeTime, &NormalizedTime );
	if( Status != PlayerNoError )
	{
	    report( severity_error, "Player_Generic_c::TranslateNativePlaybackTime - Failed to translate native time to normalized.\n" );
	    return Status;
	}

	Status	= Playback->OutputCoordinator->TranslatePlaybackTimeToSystem( PlaybackContext, NormalizedTime, SystemTime );
	if( Status != PlayerNoError )
	{
	    report( severity_error, "Player_Generic_c::TranslateNativePlaybackTime - Failed to map normalized playback time to system time\n" );
	    return Status;
	}
    }

//

    return PlayerNoError;
}


// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Request a player 2 event to indicate when a particular 
//	stream reaches a specific native playback time.
//

PlayerStatus_t   Player_Generic_c::RequestTimeNotification(
						PlayerStream_t		  Stream,
						unsigned long long	  NativeTime,
						void			 *EventUserData )
{
PlayerStatus_t		  Status;
PlayerEventRecord_t	  Event;

    //
    // We achieve this by performing an in-sequence call to the manifestor 
    // function to queue an event, when this time point is reached.
    //

    Event.Code		= EventTimeNotification;
    Event.Playback	= Stream->Playback;
    Event.Stream	= Stream;
    Event.PlaybackTime	= INVALID_TIME;
    Event.UserData	= EventUserData;

    Status	= CallInSequence( Stream, SequenceTypeBeforePlaybackTime, NativeTime, ManifestorFnQueueEventSignal, &Event );
    if( Status != PlayerNoError )
    {
	report( severity_error, "Player_Generic_c::RequestTimeNotification - Failed to request time notification.\n" );
	return Status;
    }

    return PlayerNoError;
}


// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Clock recovery functions are entirely handled by the output coordinator
//

PlayerStatus_t   Player_Generic_c::ClockRecoveryInitialize(
						PlayerPlayback_t	  Playback,
						PlayerTimeFormat_t	  SourceTimeFormat )
{
    return Playback->OutputCoordinator->ClockRecoveryInitialize( SourceTimeFormat );
}

//

PlayerStatus_t   Player_Generic_c::ClockRecoveryDataPoint(
						PlayerPlayback_t	  Playback,
						unsigned long long	  SourceTime,
						unsigned long long	  LocalTime )
{
    return Playback->OutputCoordinator->ClockRecoveryDataPoint( SourceTime, LocalTime );
}

//

PlayerStatus_t   Player_Generic_c::ClockRecoveryEstimate(
						PlayerPlayback_t	  Playback,
						unsigned long long	 *SourceTime,
						unsigned long long	 *LocalTime )
{
    return Playback->OutputCoordinator->ClockRecoveryEstimate( SourceTime, LocalTime );
}


// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Functions to hold the playback times (cross stream)
//

unsigned long long   Player_Generic_c::GetLastNativeTime(
						 PlayerPlayback_t	  Playback )
{
    return Playback->LastNativeTime;
}

//

void   Player_Generic_c::SetLastNativeTime(	PlayerPlayback_t	  Playback,
						unsigned long long	  Time )
{
    Playback->LastNativeTime	= Time;
}

