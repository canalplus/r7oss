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

Source file name : player_in_sequence.cpp
Author :           Nick

Implementation of the in sequence call related functions of the
generic class implementation of player 2


Date        Modification                                    Name
----        ------------                                    --------
27-Feb-07   Created                                         Nick

************************************************************************/

#include <stdarg.h>
#include "player_generic.h"

#include "manifestor_video.h"		// Specific include here, because we use two functions that are video only

// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Useful defines/macros that need not be user visible
//

// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Static constant data
//


// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Implement a call in sequence
//

PlayerStatus_t   Player_Generic_c::CallInSequence(
						PlayerStream_t            Stream,
						PlayerSequenceType_t      SequenceType,
						PlayerSequenceValue_t     SequenceValue,
						PlayerComponentFunction_t Fn,
						... )
{
va_list                   List;
BufferStatus_t            Status;
Buffer_t                  ControlStructureBuffer;
PlayerControlStructure_t *ControlStructure;
Ring_t                    DestinationRing;

    //
    // Garner a control structure, fill it in
    //

    Status      = PlayerControlStructurePool->GetBuffer( &ControlStructureBuffer, IdentifierInSequenceCall );
    if( Status != PlayerNoError )
    {
	report( severity_error, "Player_Generic_c::CallInSequence - Failed to get a control structure buffer.\n" );
	return Status;
    }

    ControlStructureBuffer->ObtainDataReference( NULL, NULL, (void **)(&ControlStructure) );

    ControlStructure->Action            = ActionInSequenceCall;
    ControlStructure->SequenceType      = SequenceType;
    ControlStructure->SequenceValue     = SequenceValue;
    ControlStructure->InSequence.Fn     = Fn;

//

    DestinationRing                     = NULL;

    switch( Fn )
    {
	case FrameParserFnSetModuleParameters:
		DestinationRing         = Stream->CollatedFrameRing;

		va_start( List, Fn );
		ControlStructure->InSequence.UnsignedInt        = va_arg( List, unsigned int );
		memcpy( ControlStructure->InSequence.Block, va_arg( List, void * ), ControlStructure->InSequence.UnsignedInt );
		va_end( List );

		break;

//

	case CodecFnOutputPartialDecodeBuffers:
		DestinationRing         = Stream->ParsedFrameRing;
		break;

//

	case CodecFnReleaseReferenceFrame:
		DestinationRing         = Stream->ParsedFrameRing;

		va_start( List, Fn );
		ControlStructure->InSequence.UnsignedInt        = va_arg( List, unsigned int );
		va_end( List );

//report(severity_info, "Requesting a release %d\n", ControlStructure->InSequence.UnsignedInt );

		break;

//

	case CodecFnSetModuleParameters:
		DestinationRing = Stream->ParsedFrameRing;

		va_start( List, Fn );
		ControlStructure->InSequence.UnsignedInt        = va_arg( List, unsigned int );
		memcpy( ControlStructure->InSequence.Block, va_arg( List, void * ), ControlStructure->InSequence.UnsignedInt );
		va_end( List );

		break;

//

	case ManifestorFnSetModuleParameters:
		DestinationRing = Stream->DecodedFrameRing;

		va_start( List, Fn );
		ControlStructure->InSequence.UnsignedInt        = va_arg( List, unsigned int );
		memcpy( ControlStructure->InSequence.Block, va_arg( List, void * ), ControlStructure->InSequence.UnsignedInt );
		va_end( List );

		break;

//

	case ManifestorFnQueueEventSignal:
		DestinationRing = Stream->DecodedFrameRing;

		va_start( List, Fn );
		memcpy( &ControlStructure->InSequence.Event, va_arg( List, PlayerEventRecord_t * ), sizeof(PlayerEventRecord_t) );
		va_end( List );

		break;

//

	case ManifestorVideoFnSetInputWindow:
	case ManifestorVideoFnSetOutputWindow:
		DestinationRing = Stream->DecodedFrameRing;

		va_start( List, Fn );
		{
		    unsigned int *Words	= (unsigned int *)ControlStructure->InSequence.Block;

		    Words[0]		= va_arg( List, unsigned int );
		    Words[1]		= va_arg( List, unsigned int );
		    Words[2]		= va_arg( List, unsigned int );
		    Words[3]		= va_arg( List, unsigned int );
		}
		va_end( List );

		break;

//

	case OutputTimerFnResetTimeMapping:
		DestinationRing = Stream->DecodedFrameRing;
		break;

//

	case OutputTimerFnSetModuleParameters:
		DestinationRing = Stream->DecodedFrameRing;

		va_start( List, Fn );
		ControlStructure->InSequence.UnsignedInt        = va_arg( List, unsigned int );
		memcpy( ControlStructure->InSequence.Block, va_arg( List, void * ), ControlStructure->InSequence.UnsignedInt );
		va_end( List );

		break;

//

	case OSFnSetEventOnManifestation:
		DestinationRing = Stream->DecodedFrameRing;     // This is where manifestation would take place

		va_start( List, Fn );
		ControlStructure->InSequence.Pointer    = (void *)va_arg( List, OS_Event_t * );
		va_end( List );

		break;

//

	case OSFnSetEventOnPostManifestation:
		DestinationRing = Stream->ManifestedBufferRing;

		va_start( List, Fn );
		ControlStructure->InSequence.Pointer    = (void *)va_arg( List, OS_Event_t * );
		va_end( List );

		break;

//

	case PlayerFnSwitchFrameParser:
		DestinationRing = Stream->CollatedFrameRing;

		va_start( List, Fn );
		ControlStructure->InSequence.Pointer    = (void *)va_arg( List, PlayerStream_t );
		va_end( List );

		break;

//

	case PlayerFnSwitchCodec:
		DestinationRing = Stream->ParsedFrameRing;

		va_start( List, Fn );
		ControlStructure->InSequence.Pointer    = (void *)va_arg( List, PlayerStream_t );
		va_end( List );

		break;

//

	case PlayerFnSwitchOutputTimer:
		DestinationRing = Stream->DecodedFrameRing;

		va_start( List, Fn );
		ControlStructure->InSequence.Pointer    = (void *)va_arg( List, PlayerStream_t );
		va_end( List );

		break;

//

	case PlayerFnSwitchComplete:
		DestinationRing = Stream->ManifestedBufferRing;

		va_start( List, Fn );
		ControlStructure->InSequence.Pointer    = (void *)va_arg( List, PlayerStream_t );
		va_end( List );

		break;

//

	default:
		report( severity_error, "Player_Generic_c::CallInSequence - Unsupported function call.\n" );
		ControlStructureBuffer->DecrementReferenceCount( IdentifierInSequenceCall );
		return PlayerNotSupported;
    }

    //
    // Send it to the appropriate process
    //

    DestinationRing->Insert( (unsigned int)ControlStructureBuffer );

//

    return PlayerNoError;
}


// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Perform in sequence call
//

PlayerStatus_t   Player_Generic_c::PerformInSequenceCall(
						PlayerStream_t            Stream,
						PlayerControlStructure_t *ControlStructure )
{
PlayerStatus_t  Status;

//

    Status      = PlayerNoError;                // We ignore the status for some specific functions

//

    switch( ControlStructure->InSequence.Fn )
    {
	case CodecFnOutputPartialDecodeBuffers:
		Stream->Codec->OutputPartialDecodeBuffers();
		break;

	case CodecFnReleaseReferenceFrame:
//report(severity_info, "Performing a release %d\n", ControlStructure->InSequence.UnsignedInt );
		Stream->Codec->ReleaseReferenceFrame( ControlStructure->InSequence.UnsignedInt );
		break;

	case FrameParserFnSetModuleParameters:
		Status  = Stream->FrameParser->SetModuleParameters( ControlStructure->InSequence.UnsignedInt,
								    ControlStructure->InSequence.Block );
		break;

	case CodecFnSetModuleParameters:
		Status  = Stream->Codec->SetModuleParameters(   ControlStructure->InSequence.UnsignedInt,
								ControlStructure->InSequence.Block );
		break;

	case ManifestorFnSetModuleParameters:
		Status  = Stream->Manifestor->SetModuleParameters(      ControlStructure->InSequence.UnsignedInt,
									ControlStructure->InSequence.Block );
		break;

	case ManifestorFnQueueEventSignal:
		Status  = Stream->Manifestor->QueueEventSignal(         &ControlStructure->InSequence.Event );
		break;

	case ManifestorVideoFnSetInputWindow:
		{
		    unsigned int *Words	= (unsigned int *)ControlStructure->InSequence.Block;
		    Status 		= ((Manifestor_Video_c *)Stream->Manifestor)->SetInputWindow( Words[0], Words[1], Words[2], Words[3] );
		}
		break;

	case ManifestorVideoFnSetOutputWindow:
		{
		    unsigned int *Words	= (unsigned int *)ControlStructure->InSequence.Block;
		    Status 		= ((Manifestor_Video_c *)Stream->Manifestor)->SetOutputWindow( Words[0], Words[1], Words[2], Words[3] );
		}
		break;

	case OutputTimerFnResetTimeMapping:
		Status	= Stream->OutputTimer->ResetTimeMapping();
		break;

	case OutputTimerFnSetModuleParameters:
		Status	= Stream->OutputTimer->SetModuleParameters( 	ControlStructure->InSequence.UnsignedInt,
							    		ControlStructure->InSequence.Block );
		break;

	case OSFnSetEventOnManifestation:
	case OSFnSetEventOnPostManifestation:
		OS_SetEvent( (OS_Event_t *)ControlStructure->InSequence.Pointer );
		break;

	case PlayerFnSwitchFrameParser:
		SwitchFrameParser( (PlayerStream_t)ControlStructure->InSequence.Pointer );
		break;

	case PlayerFnSwitchCodec:
		SwitchCodec( (PlayerStream_t)ControlStructure->InSequence.Pointer );
		break;

	case PlayerFnSwitchOutputTimer:
		SwitchOutputTimer( (PlayerStream_t)ControlStructure->InSequence.Pointer );
		break;

	case PlayerFnSwitchComplete:
		SwitchComplete( (PlayerStream_t)ControlStructure->InSequence.Pointer );
		break;

	default:
		report( severity_error, "Player_Generic_c::PerformInSequenceCall - Unsupported function call - Implementation error.\n" );
		Status  = PlayerNotSupported;
		break;
    }

//

    return Status;
}


// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Store a control message in the appropriate table
//

PlayerStatus_t   Player_Generic_c::AccumulateControlMessage(
						Buffer_t                          Buffer,
						PlayerControlStructure_t         *Message,
						unsigned int                     *MessageCount,
						unsigned int                      MessageTableSize,
						PlayerBufferRecord_t             *MessageTable )
{
unsigned int    i;

//

    for( i=0; i<MessageTableSize; i++ )
	if( MessageTable[i].Buffer == NULL )
	{
	    MessageTable[i].Buffer              = Buffer;
	    MessageTable[i].ControlStructure    = Message;
	    (*MessageCount)++;

	    return PlayerNoError;
	}

//

    report( severity_error, "Player_Generic_c::AccumulateControlMessage - Message table full - Implementation constant range error.\n" );
    Buffer->DecrementReferenceCount();

    return PlayerImplementationError;
}


// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Process a single control message
//

PlayerStatus_t   Player_Generic_c::ProcessControlMessage(
						PlayerStream_t                    Stream,
						Buffer_t                          Buffer,
						PlayerControlStructure_t         *Message )
{
PlayerStatus_t  Status;

//

    switch( Message->Action )
    {
	case ActionInSequenceCall:
		Status  = PerformInSequenceCall( Stream, Message );
		if( Status != PlayerNoError )
		    report( severity_error, "Player_Generic_c::ProcessControlMessage - Failed InSequence call (%08x)\n", Status );

		break;

	default:
		report( severity_error, "Player_Generic_c::ProcessControlMessage - Unhandled control structure - Implementation error.\n" );
		Status  = PlayerImplementationError;
		break;
    }

    Buffer->DecrementReferenceCount();
    return Status;
}


// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Scan the table looking for messages to be actioned at, or before, this point
//

PlayerStatus_t   Player_Generic_c::ProcessAccumulatedControlMessages(
						PlayerStream_t                    Stream,
						unsigned int                     *MessageCount,
						unsigned int                      MessageTableSize,
						PlayerBufferRecord_t             *MessageTable,
						unsigned long long                SequenceNumber,
						unsigned long long                Time )
{
unsigned int    i;
bool            SequenceCheck;
bool            ProcessNow;

    //
    // If we have no messages to scan return in a timely fashion
    //

    if( *MessageCount == 0 )
	return PlayerNoError;

    //
    // Perform the scan
    //

    for( i=0; i<MessageTableSize; i++ )
	if( MessageTable[i].Buffer != NULL )
	{
	    SequenceCheck       = (MessageTable[i].ControlStructure->SequenceType == SequenceTypeBeforeSequenceNumber) ||
				  (MessageTable[i].ControlStructure->SequenceType == SequenceTypeAfterSequenceNumber);

	    ProcessNow          = SequenceCheck ? ((SequenceNumber != INVALID_SEQUENCE_VALUE) && (MessageTable[i].ControlStructure->SequenceValue <= SequenceNumber)) :
						  ((Time           != INVALID_SEQUENCE_VALUE) && (MessageTable[i].ControlStructure->SequenceValue <= Time));

	    if( ProcessNow )
	    {
		ProcessControlMessage( Stream, MessageTable[i].Buffer, MessageTable[i].ControlStructure );

		MessageTable[i].Buffer                  = NULL;
		MessageTable[i].ControlStructure        = NULL;
		(*MessageCount)--;
	    }
	}

    return PlayerNoError;
}

