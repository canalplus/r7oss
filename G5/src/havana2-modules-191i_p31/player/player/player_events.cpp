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

Source file name : player_events.cpp
Author :           Nick

Implementation of the event related functions of the
generic class implementation of player 2


Date        Modification                                    Name
----        ------------                                    --------
06-Nov-06   Created                                         Nick

************************************************************************/

#include "player_generic.h"

// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Useful defines/macros that need not be user visible
//


// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Specify which ongoing events will be signalled
//

PlayerStatus_t   Player_Generic_c::SpecifySignalledEvents(
						PlayerPlayback_t          Playback,
						PlayerStream_t            Stream,
						PlayerEventMask_t         Events,
						void                     *UserData )
{
    //
    // Check the parameters are legal
    //

    if( (Playback == PlayerAllPlaybacks) || (Stream && PlayerAllStreams) )
    {
	report( severity_error, "Player_Generic_c::SpecifySignalledEvents - Signalled events must be specified on a single stream basis\n" );
	return PlayerError;
    }

    //
    // Pass on down to the subclasses
    //

    Stream->Collator->SpecifySignalledEvents( Events, UserData );
    Stream->FrameParser->SpecifySignalledEvents( Events, UserData );
    Stream->Codec->SpecifySignalledEvents( Events, UserData );
    Stream->OutputTimer->SpecifySignalledEvents( Events, UserData );
    Stream->Manifestor->SpecifySignalledEvents( Events, UserData );

//

    return PlayerNoError;
}


// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Specify an OS independent event to be signalled when a 
//      player 2 event is ready for collection.
//

PlayerStatus_t   Player_Generic_c::SetEventSignal(
						PlayerPlayback_t          Playback,
						PlayerStream_t            Stream,
						PlayerEventMask_t         Events,
						OS_Event_t               *Event )
{
unsigned int      i;
PlayerStatus_t    Status;
unsigned int     *PtrToIndex;

    //
    // Split into two paths, are we clearing a signal registration, or setting one
    //

    if( Event == NULL )
    {
	for( i=0; i<PLAYER_MAX_EVENT_SIGNALS; i++ )
	    if( (ExternalEventSignals[i].Signal         != NULL)        &&
		(ExternalEventSignals[i].Playback       == Playback)    &&
		(ExternalEventSignals[i].Stream         == Stream)      &&
		(ExternalEventSignals[i].Events         == Events) )
	    {
		ExternalEventSignals[i].Signal  = NULL;
		return PlayerNoError;
	    }

	report( severity_error, "Player_Generic_c::SetEventSignal - Exact match not found for clearing event signal.\n" );
	return PlayerMatchNotFound;
    }
    else

    //
    // Setting a new signal, first find a free entry and set it
    // Then check if there is an event record in the queue that
    // reuskts in us setting this event.
    //

    {

	OS_LockMutex( &Lock );

	for( i=0; i<PLAYER_MAX_EVENT_SIGNALS; i++ )
	    if( ExternalEventSignals[i].Signal == NULL )
	    {
		ExternalEventSignals[i].Playback        = Playback;
		ExternalEventSignals[i].Stream          = Stream;
		ExternalEventSignals[i].Events          = Events;
		ExternalEventSignals[i].Signal          = Event;
		break;
	    }

	OS_UnLockMutex( &Lock );

	if( i == PLAYER_MAX_EVENT_SIGNALS )
	{
	    report( severity_error, "Player_Generic_c::SetEventSignal - Attempt to set too many signal conditions.\n" );
	    return PlayerToMany;
	}

//

	Status  = ScanEventListForMatch( Playback, Stream, Events, &PtrToIndex );
	if( Status == PlayerNoError )
	    OS_SetEvent( Event );
    }

//

    return PlayerNoError;
}


// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Retrieve the next player 2 event record matching the given criteria.
//

PlayerStatus_t   Player_Generic_c::GetEventRecord(
						PlayerPlayback_t          Playback,
						PlayerStream_t            Stream,
						PlayerEventMask_t         Events,
						PlayerEventRecord_t      *Record,
						bool                      NonBlocking )
{
PlayerStatus_t            Status;
unsigned int              Index;
unsigned int             *PtrToIndex;
PlayerPlayback_t          PlaybackScan;
PlayerStream_t            StreamScan;

//

    do
    {
	OS_ResetEvent( &InternalEventSignal );

	//
	// While locked scan and if possible take the event
	//

	OS_LockMutex( &Lock );
	Status  = ScanEventListForMatch( Playback, Stream, Events, &PtrToIndex );
	if( Status == PlayerNoError )
	{
	    Index                       = *PtrToIndex;
	    memcpy( Record, &EventList[Index].Record, sizeof(PlayerEventRecord_t) );

	    *PtrToIndex                 = EventList[Index].NextIndex;
	    EventList[Index].NextIndex  = INVALID_INDEX;
	    EventList[Index].Record.Code= EventIllegalIdentifier;

	    //
	    // If we just removed the tail, walk the list to find it again.
	    //

	    if( Index == EventListTail )
		for( EventListTail = EventListHead; (EventListTail != INVALID_INDEX); EventListTail = EventList[EventListTail].NextIndex )
		    if( EventList[EventListTail].NextIndex == INVALID_INDEX )
			break;

	    OS_UnLockMutex( &Lock );
	    return PlayerNoError;
	}
	OS_UnLockMutex( &Lock );

	//
	// If we are blocking then wait for something to happen, then
	// check that the stream and playback (if specified) still exist
	// or we could stay in the function for a very long time
	//

	if( !NonBlocking )
	{
	    OS_WaitForEvent( &InternalEventSignal, PLAYER_MAX_EVENT_WAIT );

	    if( Playback != PlayerAllPlaybacks )
	    {
		for( PlaybackScan       = ListOfPlaybacks;
		     (PlaybackScan != NULL) && (PlaybackScan != Playback);
		     PlaybackScan       = PlaybackScan->Next );

		if( PlaybackScan == NULL )
		{
		    report( severity_error, "Player_Generic_c::GetEventRecord - Attempting to get an event record that on a playback that no longer exists.\n" );
		    return PlayerUnknownStream;
		}
	    }

	    if( Stream != PlayerAllStreams )
	    {
		for( StreamScan = Stream->Playback->ListOfStreams;
		     (StreamScan != NULL) && (StreamScan != Stream);
		     StreamScan = StreamScan->Next );

		if( StreamScan == NULL )
		{
		    report( severity_error, "Player_Generic_c::GetEventRecord - Attempting to get an event record that on a stream that no longer exists.\n" );
		    return PlayerUnknownStream;
		}
	    }
	}

    } while( !ShutdownPlayer && !NonBlocking ) ;

    return PlayerNoEventRecords;
}


// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Internal function to generate a player 2 event
//

PlayerStatus_t   Player_Generic_c::SignalEvent( PlayerEventRecord_t      *Record )
{
unsigned int    i;

//

    OS_LockMutex( &Lock );

    //
    // Find an empty vessel
    //

    for( i=0; i<PLAYER_MAX_OUTSTANDING_EVENTS; i++ )
	if( EventList[i].Record.Code == EventIllegalIdentifier )
	{
	    memcpy( &EventList[i].Record, Record, sizeof(PlayerEventRecord_t) );

	    EventList[i].NextIndex      = INVALID_INDEX;

	    if( EventListTail != INVALID_INDEX )
	    {
		EventList[EventListTail].NextIndex      = i;
		EventListTail                           = i;
	    }
	    else
	    {
		EventListHead                           = i;
		EventListTail                           = i;
	    }
	    break;
	}

    if( i == PLAYER_MAX_OUTSTANDING_EVENTS )
    {
	//
	// The easiest solution if there is no free slot, is to discard 
	// the oldest event and then recurse into this function.
	//

	report( severity_error, "Player_Generic_c::SignalEvent - Discarding uncollected event (%08x)\n", EventList[EventListHead].Record.Code );
	EventList[EventListHead].Record.Code    = EventIllegalIdentifier;
	EventListHead                           = EventList[EventListHead].NextIndex;

	OS_UnLockMutex( &Lock );
	return SignalEvent( Record );
    }

    //
    // We are still locked, now we can see if any of the signals 
    // that have been queued are interested in this event.
    //

    OS_SetEvent( &InternalEventSignal );

    for( i=0; i<PLAYER_MAX_EVENT_SIGNALS; i++ )
	if( (ExternalEventSignals[i].Signal != NULL) &&
	    EventMatchesCriteria( Record, ExternalEventSignals[i].Playback, ExternalEventSignals[i].Stream, ExternalEventSignals[i].Events ) )
	    OS_SetEvent( ExternalEventSignals[i].Signal );

//

    OS_UnLockMutex( &Lock );
    return PlayerNoError;
}


// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Internal function to scan the list of events, and see if any of them match the criteria
//

PlayerStatus_t   Player_Generic_c::ScanEventListForMatch(
						PlayerPlayback_t          Playback,
						PlayerStream_t            Stream,
						PlayerEventMask_t         Events,
						unsigned int            **PtrToIndex )
{
unsigned int    *IndexPtr;

//

    for( IndexPtr        = &EventListHead;
	 *IndexPtr      != INVALID_INDEX;
	 IndexPtr        = &EventList[*IndexPtr].NextIndex )
	if( EventMatchesCriteria( &EventList[*IndexPtr].Record, Playback, Stream, Events ) )
	{
	    *PtrToIndex = IndexPtr;
	    return PlayerNoError;
	}

//

    return PlayerMatchNotFound;
}


// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Internal function to scan the list of events, and see if any of them match the criteria
//

bool   Player_Generic_c::EventMatchesCriteria(  PlayerEventRecord_t      *Record,
						PlayerPlayback_t          Playback,
						PlayerStream_t            Stream,
						PlayerEventMask_t         Events )
{
bool    PlaybackMatch;
bool    StreamMatch;
bool    EventMatch;

//

    PlaybackMatch       = (Playback == PlayerAllPlaybacks) || (Playback == Record->Playback);
    StreamMatch         = (Stream == PlayerAllStreams)     || (Stream   == Record->Stream);
    EventMatch          = (Events & Record->Code) != 0;

//

    return (PlaybackMatch && StreamMatch && EventMatch);
}

