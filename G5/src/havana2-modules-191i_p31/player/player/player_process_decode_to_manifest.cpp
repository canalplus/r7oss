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

Source file name : player_process_decode_to_manifest.cpp
Author :           Nick

Implementation of the process handling data transfer 
between the decode phase and the manifestation phase of
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

OS_TaskEntry(PlayerProcessDecodeToManifest)
{
PlayerStream_t          Stream = (PlayerStream_t)Parameter;

    Stream->Player->ProcessDecodeToManifest( Stream );

    OS_TerminateThread();
    return NULL;
}


// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Main process code
//

void   Player_Generic_c::ProcessDecodeToManifest(       PlayerStream_t            Stream )
{
unsigned int                      i;
PlayerStatus_t                    Status;
RingStatus_t                      RingStatus;
unsigned int                      AccumulatedBufferTableOccupancy;
PlayerBufferRecord_t             *AccumulatedBufferTable;
Buffer_t                          Buffer = NULL;
Buffer_t                          OriginalCodedFrameBuffer;
BufferType_t                      BufferType;
PlayerControlStructure_t         *ControlStructure;
ParsedFrameParameters_t          *ParsedFrameParameters;
unsigned int                      LowestIndex;
unsigned int                      LowestDisplayFrameIndex;
unsigned int                      DesiredFrameIndex;
unsigned int			  PossibleDecodeBuffers;
unsigned int                      MaxDecodesOutOfOrder;
PlayerSequenceNumber_t           *SequenceNumberStructure;
unsigned long long                LastEntryTime;
unsigned long long                SequenceNumber;
unsigned long long                MinumumSequenceNumberAccumulated;
unsigned long long                MaximumActualSequenceNumberSeen;
unsigned long long                Time;
unsigned int                      AccumulatedBeforeControlMessagesCount;
unsigned int                      AccumulatedAfterControlMessagesCount;
bool                              SequenceCheck;
bool                              ProcessNow;
unsigned int                     *Count;
PlayerBufferRecord_t             *Table;
Buffer_t                          MarkerFrameBuffer;
bool                              FirstFrame;
bool                              DiscardBuffer;
bool				  LastPreManifestDiscardBuffer;
unsigned char                     SubmitInitialFrame;
Buffer_t                          InitialFrameBuffer;

    //
    // Set parameters
    //

    AccumulatedBufferTableOccupancy             = 0;
    AccumulatedBufferTable                      = Stream->AccumulatedDecodeBufferTable;

    LastEntryTime                               = OS_GetTimeInMicroSeconds();
    SequenceNumber                              = INVALID_SEQUENCE_VALUE;
    Time                                        = INVALID_TIME;
    AccumulatedBeforeControlMessagesCount       = 0;
    AccumulatedAfterControlMessagesCount        = 0;

    MinumumSequenceNumberAccumulated		= 0xffffffffffffffffULL;
    MaximumActualSequenceNumberSeen             = 0;
    DesiredFrameIndex                           = 0;
    FirstFrame                                  = true;

    MarkerFrameBuffer                           = NULL;
    InitialFrameBuffer				= NULL;

    LastPreManifestDiscardBuffer		= false;

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
	//
	// Buffer re-ordering loop
	//

	while( !Stream->Terminating )
	{
	    //
	    // Scan the list of accumulated buffers to see if the next display frame is available
	    // (whether because it was already accumulated, or because its display frame index has been updated)
	    //

	    MinumumSequenceNumberAccumulated    = 0xffffffffffffffffULL;
	    if( AccumulatedBufferTableOccupancy != 0 )
	    {
		LowestIndex                     = INVALID_INDEX;
		LowestDisplayFrameIndex         = INVALID_INDEX;
		for( i=0; i<Stream->NumberOfDecodeBuffers; i++ )
		    if( AccumulatedBufferTable[i].Buffer != NULL )
		    {
			MinumumSequenceNumberAccumulated        = min(MinumumSequenceNumberAccumulated, AccumulatedBufferTable[i].SequenceNumber);

			if( (AccumulatedBufferTable[i].ParsedFrameParameters->DisplayFrameIndex != INVALID_INDEX)       &&
			    ((LowestIndex == INVALID_INDEX) || (AccumulatedBufferTable[i].ParsedFrameParameters->DisplayFrameIndex < LowestDisplayFrameIndex)) )
			{
			    LowestDisplayFrameIndex     = AccumulatedBufferTable[i].ParsedFrameParameters->DisplayFrameIndex;
			    LowestIndex                 = i;
			}
		    }

		Stream->Manifestor->GetDecodeBufferCount( &PossibleDecodeBuffers );
		MaxDecodesOutOfOrder		= (Stream->Playback->Direction == PlayForward) ? (PossibleDecodeBuffers >> 1) : (3 * PossibleDecodeBuffers)/4;
		MaxDecodesOutOfOrder		= min( (PossibleDecodeBuffers - PLAYER_MINIMUM_NUMBER_OF_WORKING_DECODE_BUFFERS), MaxDecodesOutOfOrder );
		if( Stream->Playback->Direction == PlayForward )
		    MaxDecodesOutOfOrder	= min( PLAYER_LIMIT_ON_OUT_OF_ORDER_DECODES, MaxDecodesOutOfOrder );

		if( (LowestIndex != INVALID_INDEX) &&
		    (   (LowestDisplayFrameIndex == DesiredFrameIndex) || 
			(AccumulatedBufferTableOccupancy >= MaxDecodesOutOfOrder) ||
			(AccumulatedBufferTable[LowestIndex].ParsedFrameParameters->CollapseHolesInDisplayIndices) ||
			(MarkerFrameBuffer != NULL) ) )
		{
		    Buffer                                                      = AccumulatedBufferTable[LowestIndex].Buffer;
		    ParsedFrameParameters                                       = AccumulatedBufferTable[LowestIndex].ParsedFrameParameters;
		    SequenceNumber                                              = AccumulatedBufferTable[LowestIndex].SequenceNumber;
		    BufferType                                                  = Stream->DecodeBufferType;
		    AccumulatedBufferTable[LowestIndex].Buffer                  = NULL;
		    AccumulatedBufferTable[LowestIndex].ParsedFrameParameters   = NULL;
		    AccumulatedBufferTableOccupancy--;
		    break;
		}
	    }

	    //
	    // Skip any frame indices that were unused
	    //

	    while( CheckForNonDecodedFrame( Stream, DesiredFrameIndex ) )
		DesiredFrameIndex++;

	    //
	    // If we get here with a marker frame, then we have emptied our accumulated list
	    //

	    if( MarkerFrameBuffer != NULL )
	    {
		SequenceNumber                          = SequenceNumberStructure->Value;
		MaximumActualSequenceNumberSeen		= max(SequenceNumber, MaximumActualSequenceNumberSeen);
		ProcessAccumulatedControlMessages(  Stream, 
						    &AccumulatedBeforeControlMessagesCount,
						    PLAYER_MAX_DTOM_MESSAGES,
						    Stream->AccumulatedBeforeDtoMControlMessages, 
						    SequenceNumber, Time );

		ProcessAccumulatedControlMessages(  Stream,
						    &AccumulatedAfterControlMessagesCount,
						    PLAYER_MAX_DTOM_MESSAGES,
						    Stream->AccumulatedAfterDtoMControlMessages, 
						    SequenceNumber, Time );

		Stream->ManifestedBufferRing->Insert( (unsigned int)MarkerFrameBuffer );        // Pass on the marker
		MarkerFrameBuffer                       = NULL;
		Stream->DiscardingUntilMarkerFrameDtoM  = false;
		continue;
	    }

	    //
	    // Get a new buffer (continue will still perform the scan)
	    //

	    RingStatus  = Stream->DecodedFrameRing->Extract( (unsigned int *)(&Buffer), PLAYER_NEXT_FRAME_EVENT_WAIT );
	    if( (RingStatus == RingNothingToGet) || (Buffer == NULL) || Stream->Terminating )
		continue;

	    Buffer->TransferOwnership( IdentifierProcessDecodeToManifest );
	    Buffer->GetType( &BufferType );
	    if( BufferType != Stream->DecodeBufferType )
		break;

	    //
	    // Obtain a sequence number from the buffer
	    //

	    Status      = Buffer->ObtainAttachedBufferReference( Stream->CodedFrameBufferType, &OriginalCodedFrameBuffer );
	    if( Status != PlayerNoError )
	    {
		report( severity_error, "Player_Generic_c::ProcessDecodeToManifest - Unable to obtain the the original coded frame buffer - Implementation error\n" );
		Buffer->DecrementReferenceCount( IdentifierProcessDecodeToManifest );
		continue;
	    }

	    Status      = OriginalCodedFrameBuffer->ObtainMetaDataReference( MetaDataSequenceNumberType, (void **)(&SequenceNumberStructure) );
	    if( Status != PlayerNoError )
	    {
		report( severity_error, "Player_Generic_c::ProcessDecodeToManifest - Unable to obtain the meta data \"SequenceNumber\" - Implementation error\n" );
		Buffer->DecrementReferenceCount( IdentifierProcessDecodeToManifest );
		continue;
	    }

	    SequenceNumberStructure->TimeEntryInProcess2        = OS_GetTimeInMicroSeconds();
	    SequenceNumberStructure->DeltaEntryInProcess2       = SequenceNumberStructure->TimeEntryInProcess2 - LastEntryTime;
	    LastEntryTime                                       = SequenceNumberStructure->TimeEntryInProcess2;
	    SequenceNumber                                      = SequenceNumberStructure->Value;

	    //
	    // Check, is this a marker frame
	    //

	    if( SequenceNumberStructure->MarkerFrame )
	    {
		MarkerFrameBuffer       = Buffer;
		continue;                       // allow us to empty the accumulated buffer list
	    }

	    //
	    // If this is the first seen decode buffer do we wish to offer it up as an initial frame
	    //

	    if( FirstFrame )
	    {
		FirstFrame              = false;
		SubmitInitialFrame      = PolicyValue( Stream->Playback, Stream, PolicyManifestFirstFrameEarly );
		if( SubmitInitialFrame == PolicyValueApply )
		{
		    Status      	= Stream->Manifestor->InitialFrame( Buffer );
		    if( Status != ManifestorNoError )
			report( severity_error, "Player_Generic_c::ProcessDecodeToManifest - Failed to apply InitialFrame action.\n" );

		    if( InitialFrameBuffer != NULL )
			Stream->Codec->ReleaseDecodeBuffer( InitialFrameBuffer );

		    InitialFrameBuffer	= Buffer;
		    InitialFrameBuffer->IncrementReferenceCount();
		}
	    }

	    //
	    // Do we want to insert this in the table
	    //

	    Status      = Buffer->ObtainMetaDataReference( MetaDataParsedFrameParametersReferenceType, (void **)(&ParsedFrameParameters) );
	    if( Status != PlayerNoError )
	    {
		report( severity_error, "Player_Generic_c::ProcessDecodeToManifest - Unable to obtain the meta data \"ParsedFrameParametersReference\" - Implementation error\n" );
		Buffer->DecrementReferenceCount( IdentifierProcessDecodeToManifest );
		continue;
	    }

#if 0
{
unsigned int A, B;
	Stream->DecodeBufferPool->GetPoolUsage( &A, &B, NULL, NULL, NULL );

report( severity_info, "Got(%d) %3d (R = %d, K = %d) %d, %016llx - %d %d/%d\n", Stream->StreamType, ParsedFrameParameters->DecodeFrameIndex, ParsedFrameParameters->ReferenceFrame, ParsedFrameParameters->KeyFrame, ParsedFrameParameters->DisplayFrameIndex, ParsedFrameParameters->NormalizedPlaybackTime, AccumulatedBufferTableOccupancy, B, A );
}
#endif
	    if( ParsedFrameParameters->DisplayFrameIndex <= DesiredFrameIndex )
		break;

	    for( i=0; i<Stream->NumberOfDecodeBuffers; i++ )
		if( AccumulatedBufferTable[i].Buffer == NULL )
		{
		    AccumulatedBufferTable[i].Buffer                    = Buffer;
		    AccumulatedBufferTable[i].SequenceNumber            = SequenceNumber;
		    AccumulatedBufferTable[i].ParsedFrameParameters     = ParsedFrameParameters;
		    AccumulatedBufferTableOccupancy++;
		    break;
		}

	    if( i >= Stream->NumberOfDecodeBuffers )
	    {
		report( severity_error, "Player_Generic_c::ProcessDecodeToManifest - Unable to insert buffer in table - Implementation error.\n" );
		break;  // Assume it is immediate, as an implementation error this is pretty nasty
	    }
	}

	if( Stream->Terminating )
	    continue;

	// --------------------------------------------------------------------------------------------
	// We now have a buffer after frame re-ordering
	//
	// First calculate the sequence number that applies to this frame
	// this calculation may appear wierd, the idea is this, assume you
	// have a video stream IPBB, sequence numbers 0 1 2 3, frame reordering
	// will yield sequence numbers 0 2 3 1 IE any command to be executed at
	// the end of the stream will appear 1 frame early, the calculations 
	// below will re-wossname the sequence numbers to 0 1 1 3 causing the 
	// signal to occur at the correct point.
	//

	//
	// Deal with a coded frame buffer 
	//

	if( BufferType == Stream->DecodeBufferType )
	{
	    //
	    // Report any re-ordering problems
	    //

	    if( ParsedFrameParameters->CollapseHolesInDisplayIndices && (ParsedFrameParameters->DisplayFrameIndex > DesiredFrameIndex) )
		DesiredFrameIndex	= ParsedFrameParameters->DisplayFrameIndex;

	    if( ParsedFrameParameters->DisplayFrameIndex > DesiredFrameIndex )
		report( severity_error, "Player_Generic_c::ProcessDecodeToManifest - Hole in display frame indices (Got %d Expected %d).\n", ParsedFrameParameters->DisplayFrameIndex, DesiredFrameIndex );

	    if( ParsedFrameParameters->DisplayFrameIndex < DesiredFrameIndex )
		report( severity_error, "Player_Generic_c::ProcessDecodeToManifest - Frame re-ordering failure (Got %d Expected %d) - Implementation error.\n", ParsedFrameParameters->DisplayFrameIndex, DesiredFrameIndex );

	    //
	    // First calculate the sequence number that applies to this frame
	    // this calculation may appear wierd, the idea is this, assume you
	    // have a video stream IPBB, sequence numbers 0 1 2 3, frame reordering
	    // will yield sequence numbers 0 2 3 1 IE any command to be executed at
	    // the end of the stream will appear 1 frame early, the calculations 
	    // below will re-wossname the sequence numbers to 0 1 1 3 causing the 
	    // signal to occur at the correct point.
	    //

	    MaximumActualSequenceNumberSeen     = max(SequenceNumber, MaximumActualSequenceNumberSeen);
	    SequenceNumber                      = min(MaximumActualSequenceNumberSeen, MinumumSequenceNumberAccumulated );

	    Time                                = ParsedFrameParameters->NativePlaybackTime;

	    //
	    // Process any outstanding control messages to be applied before this buffer
	    //

	    ProcessAccumulatedControlMessages(  Stream, 
						&AccumulatedBeforeControlMessagesCount,
						PLAYER_MAX_DTOM_MESSAGES,
						Stream->AccumulatedBeforeDtoMControlMessages, 
						SequenceNumber, Time );

	    //
	    // If we are paused, then we loop waiting for something to happen
	    //

	    if( Stream->Playback->Speed == 0 )
	    {
		while( (Stream->Playback->Speed == 0) && !Stream->Step && !Stream->Terminating && !Stream->DiscardingUntilMarkerFrameDtoM )
		{
		    OS_WaitForEvent( &Stream->SingleStepMayHaveHappened, PLAYER_NEXT_FRAME_EVENT_WAIT );
		    OS_ResetEvent( &Stream->SingleStepMayHaveHappened );
		}

		Stream->Step    = false;
	    }

	    //
	    // If we are not discarding everything, then procede to process the buffer
	    //

	    DiscardBuffer       = Stream->DiscardingUntilMarkerFrameDtoM;

	    //
	    // Handle output timing functions, await entry into the decode window, 
	    // Then check for frame drop (whether due to trick mode, or because 
	    // we are running late). 
	    // NOTE1 Indicating we are not before decode, means 
	    // reference frames can be dropped, we will simply not display them
	    // NOTE2 We may block in these functions, so it is important to 
	    // recheck flags
	    //

	    if( !DiscardBuffer )
	    {
		Status  = Stream->OutputTimer->TestForFrameDrop( Buffer, OutputTimerBeforeOutputTiming );

		if( Status == OutputTimerNoError )
		{
		    //
		    // Note we loop here if we are engaged in re-timing the decoded frames
		    //

		    while( !Stream->Terminating && Stream->ReTimeQueuedFrames )
			OS_SleepMilliSeconds( PLAYER_RETIMING_WAIT );

		    Stream->OutputTimer->GenerateFrameTiming( Buffer );
		    Status  = Stream->OutputTimer->TestForFrameDrop( Buffer, OutputTimerBeforeManifestation );
		}

		if( Stream->DiscardingUntilMarkerFrameDtoM ||
		    Stream->Terminating ||
		    (Status != OutputTimerNoError) )
		    DiscardBuffer       = true;

		if( (DiscardBuffer != LastPreManifestDiscardBuffer) &&
		    (Status        == OutputTimerUntimedFrame) )
		{
		    report( severity_error, "Discarding untimed frames.\n" );
		}
		LastPreManifestDiscardBuffer	= DiscardBuffer;

#if 0
		// Nick debug data
		if( Status != OutputTimerNoError )
		    report( severity_info, "Timer Discard(%d) %3d (before Manifest) %08x\n", Stream->StreamType, ParsedFrameParameters->DecodeFrameIndex, Status );
#endif
	    }

	    //
	    // Pass the buffer to the manifestor for manifestation
	    // we do not release our hold on this buffer, buffers passed
	    // to the manifestor always re-appear on its output ring.
	    // NOTE calculate next desired frame index before we 
	    // give away the buffer, because ParsedFrameParameters
	    // can become invalid after either of the calls below.
	    //

	    DesiredFrameIndex   = ParsedFrameParameters->DisplayFrameIndex + 1;

	    if( !DiscardBuffer )
	    {
		SequenceNumberStructure->TimePassToManifestor   = OS_GetTimeInMicroSeconds();

#if 0
{
static unsigned long long         LastOutputTime = 0;
static unsigned long long         LastOutputTime1 = 0;
VideoOutputTiming_t              *OutputTiming;
unsigned int                      C0,C1,C2,C3;

Buffer->ObtainMetaDataReference( MetaDataVideoOutputTimingType, (void **)&OutputTiming );
Stream->CodedFrameBufferPool->GetPoolUsage( &C0, &C1, NULL, NULL, NULL );
Stream->DecodeBufferPool->GetPoolUsage( &C2, &C3, NULL, NULL, NULL );
report( severity_info, "Ord %3d (R = %d, K = %d) %d, %6lld %6lld %6lld %6lld (%d/%d %d/%d) (%d %d) %6lld %6lld\n",
	ParsedFrameParameters->DecodeFrameIndex, ParsedFrameParameters->ReferenceFrame, ParsedFrameParameters->KeyFrame, ParsedFrameParameters->DisplayFrameIndex,
	OutputTiming->SystemPlaybackTime - SequenceNumberStructure->TimePassToManifestor,
	SequenceNumberStructure->TimePassToManifestor - SequenceNumberStructure->TimeEntryInProcess2,
	SequenceNumberStructure->TimePassToManifestor - SequenceNumberStructure->TimeEntryInProcess1,
	SequenceNumberStructure->TimePassToManifestor - SequenceNumberStructure->TimeEntryInProcess0,
	C0, C1, C2, C3,
	Stream->FramesToManifestorCount, Stream->FramesFromManifestorCount,
	OutputTiming->SystemPlaybackTime - LastOutputTime, ParsedFrameParameters->NormalizedPlaybackTime - LastOutputTime1 );

//Buffer->TransferOwnership( IdentifierProcessDecodeToManifest, IdentifierManifestor );
//if( (OutputTiming->SystemPlaybackTime - SequenceNumberStructure->TimePassToManifestor) > 0xffffffffULL )
//    Stream->DecodeBufferPool->Dump( DumpAll );

    LastOutputTime = OutputTiming->SystemPlaybackTime;
    LastOutputTime1 = ParsedFrameParameters->NormalizedPlaybackTime;

}
if( Stream->FramesToManifestorCount >= 55 )
{
OS_SleepMilliSeconds( 1000 );
report( severity_info, "Ord(%d) %3d (R = %d, K = %d) %d, %016llx %016llx\n", Stream->StreamType, ParsedFrameParameters->DecodeFrameIndex, ParsedFrameParameters->ReferenceFrame, ParsedFrameParameters->KeyFrame, ParsedFrameParameters->DisplayFrameIndex, ParsedFrameParameters->NormalizedPlaybackTime, ParsedFrameParameters->NativePlaybackTime );
OS_SleepMilliSeconds( 4000 );
}
#endif
		Stream->FramesToManifestorCount++;
		Status	= Stream->Manifestor->QueueDecodeBuffer( Buffer );

		if( Status != ManifestorNoError )
		    DiscardBuffer	= true;

		if( InitialFrameBuffer != NULL )
		{
		    Stream->Codec->ReleaseDecodeBuffer( InitialFrameBuffer );
		    InitialFrameBuffer	= NULL;
		}
	    }

	    if( DiscardBuffer )
	    {
		Stream->Codec->ReleaseDecodeBuffer( Buffer );

		if( Stream->Playback->Speed == 0 )
		    Stream->Step	= true;
	    }

	    //
	    // Process any outstanding control messages to be applied after this buffer
	    //

	    ProcessAccumulatedControlMessages(  Stream,
						&AccumulatedAfterControlMessagesCount,
						PLAYER_MAX_DTOM_MESSAGES,
						Stream->AccumulatedAfterDtoMControlMessages, 
						SequenceNumber, Time );
	}

	//
	// Deal with a player control structure
	//

	else if( BufferType == BufferPlayerControlStructureType )
	{
	    Buffer->ObtainDataReference( NULL, NULL, (void **)(&ControlStructure) );

	    ProcessNow  = (ControlStructure->SequenceType == SequenceTypeImmediate);
	    if( !ProcessNow )
	    {
		SequenceCheck   = (ControlStructure->SequenceType == SequenceTypeBeforeSequenceNumber) ||
				  (ControlStructure->SequenceType == SequenceTypeAfterSequenceNumber);

		ProcessNow      = SequenceCheck ? ((SequenceNumber != INVALID_SEQUENCE_VALUE) && (ControlStructure->SequenceValue <= MaximumActualSequenceNumberSeen)) :
						  ((Time           != INVALID_SEQUENCE_VALUE) && (ControlStructure->SequenceValue <= Time));
	    }

	    if( ProcessNow )
		ProcessControlMessage( Stream, Buffer, ControlStructure );
	    else
	    {
		if( (ControlStructure->SequenceType == SequenceTypeBeforeSequenceNumber) ||
		    (ControlStructure->SequenceType == SequenceTypeBeforePlaybackTime) )
		{
		    Count       = &AccumulatedBeforeControlMessagesCount;
		    Table       = Stream->AccumulatedBeforeDtoMControlMessages;
		}
		else
		{
		    Count       = &AccumulatedAfterControlMessagesCount;
		    Table       = Stream->AccumulatedAfterDtoMControlMessages;
		}

		AccumulateControlMessage( Buffer, ControlStructure, Count, PLAYER_MAX_DTOM_MESSAGES, Table );
	    }
	}
	else
	{
	    report( severity_error, "Player_Generic_c::ProcessDecodeToManifest - Unknown buffer type received - Implementation error.\n" );
	    Buffer->DecrementReferenceCount();
	}
    }

    //
    // Perform some cleanup on local data
    //

    if( InitialFrameBuffer != NULL )
    {
	Stream->Codec->ReleaseDecodeBuffer( InitialFrameBuffer );
	InitialFrameBuffer	= NULL;
    }

    if( AccumulatedBufferTableOccupancy != 0 )
	for( i=0; i<Stream->NumberOfDecodeBuffers; i++ )
	    if( AccumulatedBufferTable[i].Buffer != NULL )
		Stream->Codec->ReleaseDecodeBuffer( AccumulatedBufferTable[i].Buffer );

    FlushNonDecodedFrameList( Stream );

    report( severity_info, "2222 Holding control strutures %d\n", AccumulatedBeforeControlMessagesCount + AccumulatedAfterControlMessagesCount );

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
//      Function to check if a frame index is non-decoded
//

bool   Player_Generic_c::CheckForNonDecodedFrame( PlayerStream_t                  Stream,
						  unsigned int                    DisplayFrameIndex )
{
unsigned int    i;
bool            Return;

    //
    // Check for this index in the table (if the table is non-empty)
    //

    if( Stream->InsertionsIntoNonDecodedBuffers == Stream->RemovalsFromNonDecodedBuffers )
	return false;

    for( i=0; i<PLAYER_MAX_DISCARDED_FRAMES; i++ )
	if( Stream->NonDecodedBuffers[i].Buffer != NULL )
	{
	    //
	    // Can we release the actual buffer
	    //

	    if( !Stream->NonDecodedBuffers[i].ReleasedBuffer )
	    {
		if( Stream->NonDecodedBuffers[i].ParsedFrameParameters->DisplayFrameIndex != INVALID_INDEX )
		{

		    if( Stream->DiscardingUntilMarkerFrameDtoM ||
			Stream->NonDecodedBuffers[i].ParsedFrameParameters->CollapseHolesInDisplayIndices )
			Stream->DisplayIndicesCollapse			= max( Stream->DisplayIndicesCollapse, Stream->NonDecodedBuffers[i].ParsedFrameParameters->DisplayFrameIndex );

		    // NOTE due to union, ParsedFrameParameters is invalid after this line, which is ok as we free the buffer in the next line anyway
		    Stream->NonDecodedBuffers[i].DisplayFrameIndex      = Stream->NonDecodedBuffers[i].ParsedFrameParameters->DisplayFrameIndex;

		    Stream->NonDecodedBuffers[i].Buffer->DecrementReferenceCount( IdentifierNonDecodedFrameList );
		    Stream->NonDecodedBuffers[i].ReleasedBuffer         = true;
		}
		else
		{
		    continue;
		}
	    }

	    //
	    // Now check the index
	    //

	    if( Stream->NonDecodedBuffers[i].DisplayFrameIndex <= DisplayFrameIndex )
	    {
		Return                                  = (Stream->NonDecodedBuffers[i].DisplayFrameIndex == DisplayFrameIndex);
		Stream->NonDecodedBuffers[i].Buffer     = NULL;
		Stream->RemovalsFromNonDecodedBuffers++;

		if( Return )
		    return true;
	    }
	}

//

#if 0
    if( (Stream->InsertionsIntoNonDecodedBuffers - Stream->RemovalsFromNonDecodedBuffers) > 16 )
	report( severity_info, "Eeek %d\n" , (Stream->InsertionsIntoNonDecodedBuffers - Stream->RemovalsFromNonDecodedBuffers) );
#endif

//
    return (DisplayFrameIndex < Stream->DisplayIndicesCollapse);
}


// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Function to flush any recorded non decoded frames
//

void   Player_Generic_c::FlushNonDecodedFrameList( PlayerStream_t                 Stream )
{
unsigned int i;

//

    if( Stream->InsertionsIntoNonDecodedBuffers == Stream->RemovalsFromNonDecodedBuffers )
	return;

    for( i=0; i<PLAYER_MAX_DISCARDED_FRAMES; i++ )
	if( (Stream->NonDecodedBuffers[i].Buffer != NULL) && !Stream->NonDecodedBuffers[i].ReleasedBuffer )
	{
	    Stream->NonDecodedBuffers[i].Buffer->DecrementReferenceCount( IdentifierNonDecodedFrameList );
	    Stream->NonDecodedBuffers[i].Buffer                 = NULL;
	    Stream->RemovalsFromNonDecodedBuffers++;
	}
}


