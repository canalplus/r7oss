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

Source file name : player_switch.cpp
Author :           Nick

Implementation of the switch stream capability of the
generic class implementation of player 2


Date        Modification                                    Name
----        ------------                                    --------
16-Apr-09   Created                                         Nick

************************************************************************/

#include "player_generic.h"

// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Switch one stream in a playback for an alternative one
//

PlayerStatus_t   Player_Generic_c::SwitchStream(PlayerStream_t            Stream,
						Collator_t                Collator,
						FrameParser_t             FrameParser,
						Codec_t                   Codec,
						OutputTimer_t             OutputTimer,
						bool                      NonBlocking,
						bool                      SignalEvent,
						void                     *EventUserData )
{
PlayerStatus_t		  Status;
OS_Status_t		  OSStatus;
PlayerEventRecord_t	  Event;

    //
    // This is messy enough as is without trying to do it twice at the same time
    //

    if( Stream->SwitchStreamInProgress )
    {
	report( severity_error, "Player_Generic_c::SwitchStream - Attempt to switch stream, when switch already in progress\n" );
	return PlayerError;
    }

    //
    // Initiate a non blocking stream drain, this generates and inserts a marker frame
    //

    Status	= InternalDrainStream( Stream, true, false, NULL, PolicyPlayoutOnSwitch, false );
    if( Status != PlayerNoError )
    {
	report( severity_error, "Player_Generic_c::SwitchStream - Failed to drain stream.\n" );
    }

    //
    // Insert the appropriate switch calls into the flow
    //

    Stream->SwitchStreamInProgress	= true;
    Stream->SwitchingToCollator		= (Collator    != NULL) ? Collator    : Stream->Collator;
    Stream->SwitchingToFrameParser	= (FrameParser != NULL) ? FrameParser : Stream->FrameParser;
    Stream->SwitchingToCodec		= (Codec       != NULL) ? Codec       : Stream->Codec;
    Stream->SwitchingToOutputTimer	= (OutputTimer != NULL) ? OutputTimer : Stream->OutputTimer;

    OS_ResetEvent( &Stream->SwitchStreamLastOneOutOfTheCodec );

//

    SwitchCollator( Stream );
    if( Stream->Demultiplexor != NULL )
	Stream->Demultiplexor->SwitchStream( Stream->DemultiplexorContext, Stream );

    CallInSequence( Stream, SequenceTypeAfterSequenceNumber, Stream->DrainSequenceNumber, PlayerFnSwitchFrameParser, Stream );
    CallInSequence( Stream, SequenceTypeAfterSequenceNumber, Stream->DrainSequenceNumber, PlayerFnSwitchCodec,       Stream );
    CallInSequence( Stream, SequenceTypeAfterSequenceNumber, Stream->DrainSequenceNumber, PlayerFnSwitchOutputTimer, Stream );
    CallInSequence( Stream, SequenceTypeBeforeSequenceNumber, (Stream->DrainSequenceNumber+1), PlayerFnSwitchComplete, Stream );

    //
    // Do we want to raise a signal when the first frame of the new stream is queued to the manifestor
    //

    if( SignalEvent )
    {
	Event.Code              = EventStreamSwitched;
	Event.Playback          = Stream->Playback;
	Event.Stream            = Stream;
	Event.PlaybackTime      = TIME_NOT_APPLICABLE;
	Event.UserData          = EventUserData;

	Status  = CallInSequence( Stream, SequenceTypeBeforeSequenceNumber, Stream->DrainSequenceNumber, ManifestorFnQueueEventSignal, &Event );
	if( Status != PlayerNoError )
	{
	    report( severity_error, "Player_Generic_c::SwitchStream - Failed to issue call to signal manifestation of new stream.\n" );
	    return Status;
	}
    }

    //
    // Are we blocking 
    //

    if( !NonBlocking )
    {
	OSStatus    = OS_WaitForEvent( &Stream->Drained, PLAYER_MAX_MARKER_TIME_THROUGH_CODEC );
	if( OSStatus != OS_NO_ERROR )
	{
	    report( severity_error, "Player_Generic_c::SwitchStream - Failed to drain old stream within allowed time (%d %d %d %d).\n",
			Stream->DiscardingUntilMarkerFrameCtoP, Stream->DiscardingUntilMarkerFramePtoD,
			Stream->DiscardingUntilMarkerFrameDtoM, Stream->DiscardingUntilMarkerFramePostM );
	    return PlayerTimedOut;
	}
    }

//

    return PlayerNoError;
}


// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Switch stream component function for the collator
//

void    Player_Generic_c::SwitchCollator(	PlayerStream_t		  Stream )
{
PlayerStatus_t		  Status;

    //
    // Halt and reset the current collator
    //

    Stream->Collator->DiscardAccumulatedData();
    Stream->Collator->Halt();
    Stream->Collator->Reset();

    //
    // Switch over to the new collator
    //

    if( (Stream->SwitchingToCollator != NULL) && (Stream->SwitchingToCollator != Stream->Collator) )
	Stream->Collator	= Stream->SwitchingToCollator;

    //
    // Initialize the collator
    //

    Stream->Collator->RegisterPlayer(   this, Stream->Playback, Stream, 
					Stream->SwitchingToCollator, 
					Stream->SwitchingToFrameParser, 
					Stream->SwitchingToCodec, 
					Stream->SwitchingToOutputTimer,
					Stream->Manifestor );

    Status	= Stream->Collator->RegisterOutputBufferRing( Stream->CollatedFrameRing );
    if( Status != PlayerNoError )
        report( severity_error, "Player_Generic_c::SwitchCollator - Failed to register stream parameters with Collator.\n" );
}


// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Switch stream component function for the FrameParser
//

void    Player_Generic_c::SwitchFrameParser(	PlayerStream_t		  Stream )
{
PlayerStatus_t		  Status;
OS_Status_t		  OSStatus;

    //
    // Halt the current FrameParser
    //

    Stream->FrameParser->Halt();

    //
    // Wait for the last decode to come out the other end, of the codec
    //

    OSStatus	= OS_WaitForEvent( &Stream->SwitchStreamLastOneOutOfTheCodec, PLAYER_MAX_MARKER_TIME_THROUGH_CODEC );
    if( OSStatus != OS_NO_ERROR )
	report( severity_error, "Player_Generic_c::SwitchFrameParser - Last decode did not complete in reasonable time.\n" );

    //
    // Reset the current FrameParser, we should be sure at this point that no-one will 
    // need to use any of the stream, or frame, parameters of the current frame parser. 
    //

    Stream->FrameParser->Reset();

    //
    // Switch over to the new FrameParser
    //

    if( (Stream->SwitchingToFrameParser != NULL) && (Stream->SwitchingToFrameParser != Stream->FrameParser) )
	Stream->FrameParser	= Stream->SwitchingToFrameParser;

    //
    // Initialize the FrameParser
    //

    Stream->FrameParser->RegisterPlayer(   this, Stream->Playback, Stream, 
					Stream->SwitchingToCollator, 
					Stream->SwitchingToFrameParser, 
					Stream->SwitchingToCodec, 
					Stream->SwitchingToOutputTimer,
					Stream->Manifestor );

    Status	= Stream->FrameParser->RegisterOutputBufferRing( Stream->ParsedFrameRing );
    if( Status != PlayerNoError )
        report( severity_error, "Player_Generic_c::SwitchFrameParser - Failed to register stream parameters with FrameParser.\n" );
}


// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Switch stream component function for the Codec
//

void    Player_Generic_c::SwitchCodec(		PlayerStream_t		  Stream )
{
OS_Status_t		  OSStatus;
PlayerStatus_t		  Status;

    //
    // Wait for the last inserted decode to come out the other end
    //

    OSStatus	= OS_WaitForEvent( &Stream->SwitchStreamLastOneOutOfTheCodec, PLAYER_MAX_DISCARD_DRAIN_TIME );
    if( OSStatus != OS_NO_ERROR )
	report( severity_error, "Player_Generic_c::SwitchCodec - Last decode did not complete in reasonable time.\n" );

    //
    // Halt and reset the current Codec
    //

    Stream->Codec->OutputPartialDecodeBuffers();
    Stream->Codec->ReleaseReferenceFrame( CODEC_RELEASE_ALL );
    Stream->Codec->Halt();

    Stream->CodecReset	= true;
    Stream->Codec->Reset();

    //
    // Switch over to the new Codec
    //

    if( (Stream->SwitchingToCodec != NULL) && (Stream->SwitchingToCodec != Stream->Codec) )
	Stream->Codec	= Stream->SwitchingToCodec;

    //
    // Initialize the Codec
    //

    Stream->Codec->RegisterPlayer(   this, Stream->Playback, Stream, 
					Stream->SwitchingToCollator, 
					Stream->SwitchingToFrameParser, 
					Stream->SwitchingToCodec, 
					Stream->SwitchingToOutputTimer,
					Stream->Manifestor );

    Status	= Stream->Codec->RegisterOutputBufferRing( Stream->DecodedFrameRing );
    if( Status != PlayerNoError )
        report( severity_error, "Player_Generic_c::SwitchFrameParser - Failed to register stream parameters with Codec.\n" );

}


// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Switch stream component function for the OutputTimer
//

void    Player_Generic_c::SwitchOutputTimer(	PlayerStream_t		  Stream )
{
PlayerStatus_t		  Status;

    //
    // If we are ready to switch the output timer then the last decode must 
    // have come through, so we signal that the codec can be swapped.
    //

    OS_SetEvent( &Stream->SwitchStreamLastOneOutOfTheCodec );

    //
    // We treat the output timer differently from the other classes, 
    // because if there is not actually a new timer, then we want to 
    // inherit the previous time mapping and just implement a jump.
    //

    if( (Stream->SwitchingToOutputTimer != NULL) && (Stream->SwitchingToOutputTimer != Stream->OutputTimer) )
    {
	//
	// Here we handle an actual switch, first tearing down the old timer, and building a new one
	//

	Stream->OutputTimer->Halt();
	Stream->OutputTimer->Reset();

	Stream->OutputTimer	= Stream->SwitchingToOutputTimer;

	Stream->OutputTimer->RegisterPlayer(   this, Stream->Playback, Stream, 
					Stream->SwitchingToCollator, 
					Stream->SwitchingToFrameParser, 
					Stream->SwitchingToCodec, 
					Stream->SwitchingToOutputTimer,
					Stream->Manifestor );

	Status	= Stream->OutputTimer->RegisterOutputCoordinator( Stream->Playback->OutputCoordinator );
	if( Status != PlayerNoError )
	    report( severity_error, "Player_Generic_c::SwitchOutputTimer - Failed to register Output Coordinator with Output Timer.\n" );
    }
    else
    {
	//
	// Here we handle the case of an unchanged output timer, 
	// where we simply update the class pointers that have changed.
	//

	Stream->OutputTimer->RegisterPlayer(   this, Stream->Playback, Stream, 
					Stream->SwitchingToCollator, 
					Stream->SwitchingToFrameParser, 
					Stream->SwitchingToCodec, 
					Stream->SwitchingToOutputTimer,
					Stream->Manifestor );
    }
}


// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Switch stream component function to recognize the switch is complete
//

void    Player_Generic_c::SwitchComplete(	PlayerStream_t		  Stream )
{
    //
    // When a frame reaches this point, we can safely mark the state 
    // of the codec as being initialized for this and subsequent frames.
    // This means that these frames will be released via the codec, where
    // the codec tables will be appropriately handled
    //

    Stream->CodecReset			= false;
    Stream->SwitchStreamInProgress	= false;
}



