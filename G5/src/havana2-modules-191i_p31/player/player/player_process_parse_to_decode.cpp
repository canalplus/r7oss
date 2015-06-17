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

Source file name : player_process_parse_to_decode.cpp
Author :           Nick

Implementation of the process handling data transfer 
between the parse phase and the decode phase of 
the generic class implementation of player 2.


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
//      C stub
//

OS_TaskEntry(PlayerProcessParseToDecode)
{
PlayerStream_t          Stream = (PlayerStream_t)Parameter;

    Stream->Player->ProcessParseToDecode( Stream );

    OS_TerminateThread();
    return NULL;
}


// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Main process code
//

void   Player_Generic_c::ProcessParseToDecode(  PlayerStream_t            Stream )
{
PlayerStatus_t                    Status;
RingStatus_t                      RingStatus;
Buffer_t                          Buffer;
BufferType_t                      BufferType;
PlayerControlStructure_t         *ControlStructure;
PlayerSequenceNumber_t           *SequenceNumberStructure;
ParsedFrameParameters_t          *ParsedFrameParameters;
unsigned long long                LastEntryTime;
unsigned long long                SequenceNumber;
unsigned long long                MaximumActualSequenceNumberSeen;
unsigned int                      AccumulatedBeforeControlMessagesCount;
unsigned int                      AccumulatedAfterControlMessagesCount;
bool                              ProcessNow;
unsigned int                     *Count;
PlayerBufferRecord_t             *Table;
bool                              DiscardBuffer;
bool				  PromoteNextStreamParametersToNew;

    //
    // Set parameters
    //

    LastEntryTime                               = OS_GetTimeInMicroSeconds();
    SequenceNumber                              = INVALID_SEQUENCE_VALUE;
    MaximumActualSequenceNumberSeen             = 0;
    AccumulatedBeforeControlMessagesCount       = 0;
    AccumulatedAfterControlMessagesCount        = 0;
    PromoteNextStreamParametersToNew		= false;

    //
    // Signal we have started
    //

    OS_LockMutex( &Lock );

    Stream->ProcessRunningCount++;

    if( Stream->ProcessRunningCount == Stream->ExpectedProcessCount )
	OS_SetEvent( &Stream->StartStopEvent );

    OS_UnLockMutex( &Lock );

    //
    // Main Loop
    //

    while( !Stream->Terminating )
    {
	RingStatus      = Stream->ParsedFrameRing->Extract( (unsigned int *)(&Buffer), PLAYER_MAX_EVENT_WAIT );
	if( (RingStatus == RingNothingToGet) || Stream->Terminating )
	{
	    continue;
	}

	Buffer->GetType( &BufferType );
	Buffer->TransferOwnership( IdentifierProcessParseToDecode );

	//
	// Deal with a coded frame buffer 
	//

	if( BufferType == Stream->CodedFrameBufferType )
	{
	    //
	    // Obtain a sequence number from the buffer
	    //

	    Status      = Buffer->ObtainMetaDataReference( MetaDataSequenceNumberType, (void **)(&SequenceNumberStructure) );
	    if( Status != PlayerNoError )
	    {
		report( severity_error, "Player_Generic_c::ProcessParseToDecode - Unable to obtain the meta data \"SequenceNumber\" - Implementation error\n" );
		Buffer->DecrementReferenceCount( IdentifierProcessParseToDecode );
		continue;
	    }

	    SequenceNumberStructure->TimeEntryInProcess1        = OS_GetTimeInMicroSeconds();
	    SequenceNumberStructure->DeltaEntryInProcess1       = SequenceNumberStructure->TimeEntryInProcess1 - LastEntryTime;
	    LastEntryTime                                       = SequenceNumberStructure->TimeEntryInProcess1;
	    SequenceNumber                                      = SequenceNumberStructure->Value;
	    MaximumActualSequenceNumberSeen                     = max(SequenceNumber, MaximumActualSequenceNumberSeen);

	    if( SequenceNumberStructure->MarkerFrame )
		Stream->DiscardingUntilMarkerFramePtoD  = false;

	    //
	    // Obtain the parsed frame parameters
	    //

	    Status      = Buffer->ObtainMetaDataReference( MetaDataParsedFrameParametersType, (void **)(&ParsedFrameParameters) );
	    if( Status != PlayerNoError )
	    {
		report( severity_error, "Player_Generic_c::ProcessParseToDecode - Unable to obtain the meta data \"ParsedFrameParameters\" - Implementation error\n" );
		Buffer->DecrementReferenceCount( IdentifierProcessParseToDecode );
		continue;
	    }

	    //
	    // Process any outstanding control messages to be applied before this buffer
	    //

	    ProcessAccumulatedControlMessages(  Stream,
						&AccumulatedBeforeControlMessagesCount,
						PLAYER_MAX_PTOD_MESSAGES,
						Stream->AccumulatedBeforePtoDControlMessages, 
						SequenceNumber, INVALID_TIME );

	    //
	    // If we are not discarding everything, then procede to process the buffer
	    //

	    DiscardBuffer       = Stream->DiscardingUntilMarkerFramePtoD;

	    //
	    // Handle output timing functions, await entry into the decode window, 
	    // Then check for frame drop (whether due to trick mode, or because 
	    // we are running late). 
	    // NOTE1 Indicating we are before decode, means 
	    // reference frames will probably not be dropped.
	    // NOTE2 We may block in these functions, so it is important to 
	    // recheck flags
	    //

	    if( !DiscardBuffer && !SequenceNumberStructure->MarkerFrame )
	    {
		Status  = Stream->OutputTimer->TestForFrameDrop( Buffer, OutputTimerBeforeDecodeWindow );

		if( Status == OutputTimerNoError )
		{
		    if( ParsedFrameParameters->FirstParsedParametersForOutputFrame )
			Stream->OutputTimer->AwaitEntryIntoDecodeWindow( Buffer );

		    Status  = Stream->OutputTimer->TestForFrameDrop( Buffer, OutputTimerBeforeDecode );
		}

		if( Stream->DiscardingUntilMarkerFramePtoD ||
		    Stream->Terminating ||
		    (Status != OutputTimerNoError) )
		    DiscardBuffer       = true;

#if 0
		// Nick debug data
		if( Status!= OutputTimerNoError )
		    report( severity_info, "Timer Discard (before decode) %08x - %d\n", Status, ParsedFrameParameters->DecodeFrameIndex );
#endif
	    }

	    //
	    // Pass the buffer to the codec for decoding
	    // then release our hold on this buffer.
	    //
	    // If we are discarding the frame, and it has new stream parameters,
	    // we remember the fact, and on the next frame that is decoded, we
	    // promote its stream parameters as new.
	    //

	    if( !DiscardBuffer )
	    {
		if( PromoteNextStreamParametersToNew && (ParsedFrameParameters->StreamParameterStructure != NULL) )
		{
		    ParsedFrameParameters->NewStreamParameters	= true;
		    PromoteNextStreamParametersToNew		= false;
		}

		SequenceNumberStructure->TimePassToCodec        = OS_GetTimeInMicroSeconds();
		Status	= Stream->Codec->Input( Buffer );
		if( Status != CodecNoError )
		    DiscardBuffer				= true;
	    }

	    if( DiscardBuffer )
	    {
		if( ParsedFrameParameters->NewStreamParameters )
		    PromoteNextStreamParametersToNew		= true;

		if( ParsedFrameParameters->FirstParsedParametersForOutputFrame )
		{
		    RecordNonDecodedFrame( Stream, Buffer, ParsedFrameParameters );
		    Stream->Codec->OutputPartialDecodeBuffers();
		}
	    }

	    Buffer->DecrementReferenceCount( IdentifierProcessParseToDecode );

	    //
	    // Process any outstanding control messages to be applied after this buffer
	    //

	    ProcessAccumulatedControlMessages(  Stream,
						&AccumulatedAfterControlMessagesCount,
						PLAYER_MAX_PTOD_MESSAGES,
						Stream->AccumulatedAfterPtoDControlMessages, 
						SequenceNumber, INVALID_TIME );
	}

	//
	// Deal with a player control structure
	//

	else if( BufferType == BufferPlayerControlStructureType )
	{
	    Buffer->ObtainDataReference( NULL, NULL, (void **)(&ControlStructure) );

	    ProcessNow  = (ControlStructure->SequenceType == SequenceTypeImmediate) ||
			  ((SequenceNumber != INVALID_SEQUENCE_VALUE) && (ControlStructure->SequenceValue <= MaximumActualSequenceNumberSeen));

	    if( ProcessNow )
		ProcessControlMessage( Stream, Buffer, ControlStructure );
	    else
	    {
		if( (ControlStructure->SequenceType == SequenceTypeBeforeSequenceNumber) ||
		    (ControlStructure->SequenceType == SequenceTypeBeforePlaybackTime) )
		{
		    Count       = &AccumulatedBeforeControlMessagesCount;
		    Table       = Stream->AccumulatedBeforePtoDControlMessages;
		}
		else
		{
		    Count       = &AccumulatedAfterControlMessagesCount;
		    Table       = Stream->AccumulatedAfterPtoDControlMessages;
		}

		AccumulateControlMessage( Buffer, ControlStructure, Count, PLAYER_MAX_PTOD_MESSAGES, Table );
	    }
	}
	else
	{
	    report( severity_error, "Player_Generic_c::ProcessParseToDecode - Unknown buffer type received - Implementation error.\n" );
	    Buffer->DecrementReferenceCount();
	}
    }

    report( severity_info, "1111 Holding control strutures %d\n", AccumulatedBeforeControlMessagesCount + AccumulatedAfterControlMessagesCount );

    //
    // Signal we have terminated
    //

    OS_LockMutex( &Lock );

    Stream->ProcessRunningCount--;

    if( Stream->ProcessRunningCount == 0 )
	OS_SetEvent( &Stream->StartStopEvent );

    OS_UnLockMutex( &Lock );
}


// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Function to record a frame as non-decoded
//

void   Player_Generic_c::RecordNonDecodedFrame( PlayerStream_t            Stream,
						Buffer_t                  Buffer,
						ParsedFrameParameters_t  *ParsedFrameParameters )
{
unsigned int            i;
PlayerStatus_t          Status;

    //
    // Fill me a record
    //

    for( i=0; i<PLAYER_MAX_DISCARDED_FRAMES; i++ )
	if( Stream->NonDecodedBuffers[i].Buffer == NULL )
	{
	    //
	    // Do we want the buffer, or can we take the index now
	    //

	    Stream->NonDecodedBuffers[i].ReleasedBuffer = ParsedFrameParameters->DisplayFrameIndex != INVALID_INDEX;

	    if( Stream->NonDecodedBuffers[i].ReleasedBuffer )
	    {
		Stream->NonDecodedBuffers[i].DisplayFrameIndex  = ParsedFrameParameters->DisplayFrameIndex;
		if( Stream->DiscardingUntilMarkerFramePtoD || ParsedFrameParameters->CollapseHolesInDisplayIndices )
		    Stream->DisplayIndicesCollapse	= max( Stream->DisplayIndicesCollapse, ParsedFrameParameters->DisplayFrameIndex );
	    }
	    else
	    {
		Buffer->IncrementReferenceCount( IdentifierNonDecodedFrameList );
		Stream->NonDecodedBuffers[i].ParsedFrameParameters      = ParsedFrameParameters;        // Check for it later

		//
		// Shrink that buffer
		//

		Buffer->SetUsedDataSize( 0 );
		Status  = Buffer->ShrinkBuffer( 0 );
		if( Status != PlayerNoError )
		    report( severity_info, "Player_Generic_c::RecordNonDecodedFrame - Failed to shrink buffer.\n" );
	    }

	    Stream->NonDecodedBuffers[i].Buffer                 = Buffer;
	    Stream->InsertionsIntoNonDecodedBuffers++;

	    //
	    // Would it be wise to poke the post decode process to check the non decoded list
	    //

	    if( (Stream->InsertionsIntoNonDecodedBuffers - Stream->RemovalsFromNonDecodedBuffers) > 
		(PLAYER_MAX_DISCARDED_FRAMES - 2) )
		Stream->DecodedFrameRing->Insert( (unsigned int)NULL );

	    return;
	}

    //
    // If we have no free entry report it, but ignore it, 
    // it will be picked up as a hole in the list, but
    // there will be a latency impact. Comment out the report,
    // because this does occur when we do a drain (skip, 
    // terminate, reverse).
    //

#if 0
    OS_SleepMilliSeconds(20);
    report( severity_info, "Player_Generic_c::RecordNonDecodedFrame - Unable to record %4d, table full.\n", ParsedFrameParameters->DisplayFrameIndex );
#endif
}
