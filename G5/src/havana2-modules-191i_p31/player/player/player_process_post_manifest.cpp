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

Source file name : player_process_post_manifest.cpp
Author :           Nick

Implementation of the process handling post manifestation 
activities of the generic class implementation of player 2.


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

OS_TaskEntry(PlayerProcessPostManifest)
{
PlayerStream_t		Stream = (PlayerStream_t)Parameter;

    Stream->Player->ProcessPostManifest( Stream );

    OS_TerminateThread();
    return NULL;
}


// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Main process code
//

void   Player_Generic_c::ProcessPostManifest(	PlayerStream_t		  Stream )
{
PlayerStatus_t			  Status;
RingStatus_t			  RingStatus;
Buffer_t			  Buffer;
Buffer_t			  OriginalCodedFrameBuffer;
BufferType_t			  BufferType;
PlayerControlStructure_t	 *ControlStructure;
ParsedFrameParameters_t		 *ParsedFrameParameters;
PlayerSequenceNumber_t		 *SequenceNumberStructure;
unsigned long long		  LastEntryTime;
unsigned long long		  SequenceNumber;
unsigned long long                MaximumActualSequenceNumberSeen;
unsigned long long 		  Time;
unsigned int			  AccumulatedBeforeControlMessagesCount;
unsigned int			  AccumulatedAfterControlMessagesCount;
bool				  ProcessNow;
unsigned int			 *Count;
PlayerBufferRecord_t		 *Table;
VideoOutputTiming_t		 *OutputTiming;
unsigned long long		  Now;

//

    LastEntryTime				= OS_GetTimeInMicroSeconds();
    SequenceNumber				= INVALID_SEQUENCE_VALUE;
    MaximumActualSequenceNumberSeen             = 0;
    Time					= INVALID_TIME;
    AccumulatedBeforeControlMessagesCount	= 0;
    AccumulatedAfterControlMessagesCount	= 0;

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
	RingStatus	= Stream->ManifestedBufferRing->Extract( (unsigned int *)(&Buffer), PLAYER_MAX_EVENT_WAIT );

	Now	= OS_GetTimeInMicroSeconds();
	if( Stream->ReTimeQueuedFrames && ((Now - Stream->ReTimeStart) > PLAYER_MAX_TIME_IN_RETIMING) )
	    Stream->ReTimeQueuedFrames	= false;

	if( RingStatus == RingNothingToGet )
	    continue;

	Stream->BuffersComingOutOfManifestation	= true;

	Buffer->GetType( &BufferType );
	Buffer->TransferOwnership( IdentifierProcessPostManifest );

	//
	// Deal with a coded frame buffer 
	//

	if( BufferType == Stream->DecodeBufferType )
	{
	    Stream->FramesFromManifestorCount++;

#if 0
{
static unsigned long long	  LastTime = 0;
static unsigned long long	  LastActualTime = 0;
VideoOutputTiming_t     	 *OutputTiming;

Buffer->ObtainMetaDataReference( MetaDataVideoOutputTimingType, (void **)&OutputTiming );

report( severity_info, "Post Dn = %d %d, I = %d, TFF = %d, DS= %6lld, DAS = %6lld, S = %016llx, AS = %016llx\n",
		OutputTiming->DisplayCount[0], OutputTiming->DisplayCount[1],
		OutputTiming->Interlaced, OutputTiming->TopFieldFirst,
		OutputTiming->SystemPlaybackTime - LastTime,
		OutputTiming->ActualSystemPlaybackTime - LastActualTime,
		OutputTiming->SystemPlaybackTime, OutputTiming->ActualSystemPlaybackTime );

    LastTime 		= OutputTiming->SystemPlaybackTime;
    LastActualTime 	= OutputTiming->ActualSystemPlaybackTime;
}
#endif

	    //
	    // Obtain a sequence number from the buffer
	    //

	    Status	= Buffer->ObtainAttachedBufferReference( Stream->CodedFrameBufferType, &OriginalCodedFrameBuffer );
	    if( Status != PlayerNoError )
	    {
	        report( severity_error, "Player_Generic_c::ProcessPostManifest - Unable to obtain the the original coded frame buffer - Implementation error\n" );
		Buffer->DecrementReferenceCount( IdentifierProcessPostManifest );
		continue;
	    }

	    Status	= OriginalCodedFrameBuffer->ObtainMetaDataReference( MetaDataSequenceNumberType, (void **)(&SequenceNumberStructure) );
	    if( Status != PlayerNoError )
	    {
	        report( severity_error, "Player_Generic_c::ProcessPostManifest - Unable to obtain the meta data \"SequenceNumber\" - Implementation error\n" );
		Buffer->DecrementReferenceCount( IdentifierProcessPostManifest );
		continue;
	    }

	    Status	= Buffer->ObtainMetaDataReference( MetaDataParsedFrameParametersReferenceType, (void **)(&ParsedFrameParameters) );
	    if( Status != PlayerNoError )
	    {
	        report( severity_error, "Player_Generic_c::ProcessPostManifest - Unable to obtain the meta data \"ParsedFrameParametersReference\" - Implementation error\n" );
		Buffer->DecrementReferenceCount( IdentifierProcessPostManifest );
		continue;
	    }

	    //
	    // Check for whether or not we are in re-timing
	    //

	    if( Stream->ReTimeQueuedFrames && !SequenceNumberStructure->MarkerFrame )
	    {
		Status	= Buffer->ObtainMetaDataReference( (Stream->StreamType == StreamTypeVideo ? MetaDataVideoOutputTimingType : MetaDataAudioOutputTimingType),
							   (void **)&OutputTiming );
		if( Status != PlayerNoError )
		{
	            report( severity_error, "Player_Generic_c::ProcessPostManifest - Unable to obtain the meta data \"%s\" - Implementation error\n",
				(Stream->StreamType == StreamTypeVideo ? "VideoOutputTiming" : "AudioOutputTiming") );
		    Buffer->DecrementReferenceCount( IdentifierProcessPostManifest );
		    continue;
		}

		if( ValidTime(OutputTiming->ActualSystemPlaybackTime) )
		{
		    Stream->ReTimeQueuedFrames	= false;
		}
		else
		{
		    Stream->OutputTimer->GenerateFrameTiming( Buffer );
		    Status  = Stream->OutputTimer->TestForFrameDrop( Buffer, OutputTimerBeforeManifestation );
		    if( !Stream->Terminating && (Status == OutputTimerNoError) )
		    {
			Stream->FramesToManifestorCount++;
			Stream->Manifestor->QueueDecodeBuffer( Buffer );
			continue;
		    }
		}
	    }

	    //
	    // Extract the sequence number, and write the timing statistics
	    //

//report( severity_info, "MQ Post Man %d - %d\n", Stream->StreamType, ParsedFrameParameters->DisplayFrameIndex );

	    SequenceNumberStructure->TimeEntryInProcess3	= OS_GetTimeInMicroSeconds();
	    SequenceNumberStructure->DeltaEntryInProcess3	= SequenceNumberStructure->TimeEntryInProcess3 - LastEntryTime;
	    LastEntryTime					= SequenceNumberStructure->TimeEntryInProcess3;
	    SequenceNumber					= SequenceNumberStructure->Value;
	    MaximumActualSequenceNumberSeen			= max(SequenceNumber, MaximumActualSequenceNumberSeen);
	    Time						= ParsedFrameParameters->NativePlaybackTime;

	    ProcessStatistics( Stream, SequenceNumberStructure );

	    if( SequenceNumberStructure->MarkerFrame )
	    {
		Stream->DiscardingUntilMarkerFramePostM	= false;
		Time					= INVALID_TIME;
	    }

	    //
	    // Process any outstanding control messages to be applied before this buffer
	    //

	    ProcessAccumulatedControlMessages( 	Stream, 
						&AccumulatedBeforeControlMessagesCount,
						PLAYER_MAX_POSTM_MESSAGES,
						Stream->AccumulatedBeforePostMControlMessages, 
						SequenceNumber, Time );

	    //
	    // Pass buffer back into output timer
	    // and release the buffer.
	    //

	    if( !SequenceNumberStructure->MarkerFrame )
		Stream->OutputTimer->RecordActualFrameTiming( Buffer );

	    if( !SequenceNumberStructure->MarkerFrame && !Stream->CodecReset )
		Stream->Codec->ReleaseDecodeBuffer( Buffer );
	    else
		Buffer->DecrementReferenceCount( IdentifierProcessPostManifest );

	    //
	    // Process any outstanding control messages to be applied after this buffer
	    //

	    ProcessAccumulatedControlMessages( 	Stream,
						&AccumulatedAfterControlMessagesCount,
						PLAYER_MAX_POSTM_MESSAGES,
						Stream->AccumulatedAfterPostMControlMessages, 
						SequenceNumber, Time );
	}

	//
	// Deal with a player control structure
	//

	else if( BufferType == BufferPlayerControlStructureType )
	{
	    Buffer->ObtainDataReference( NULL, NULL, (void **)(&ControlStructure) );

	    ProcessNow	= (ControlStructure->SequenceType == SequenceTypeImmediate) ||
			  ((SequenceNumber != INVALID_SEQUENCE_VALUE) && (ControlStructure->SequenceValue <= MaximumActualSequenceNumberSeen));

	    if( ProcessNow )
		ProcessControlMessage( Stream, Buffer, ControlStructure );
	    else
	    {
		if( (ControlStructure->SequenceType == SequenceTypeBeforeSequenceNumber) ||
		    (ControlStructure->SequenceType == SequenceTypeBeforePlaybackTime) )
		{
		    Count	= &AccumulatedBeforeControlMessagesCount;
		    Table	= Stream->AccumulatedBeforePostMControlMessages;
		}
		else
		{
		    Count	= &AccumulatedAfterControlMessagesCount;
		    Table	= Stream->AccumulatedAfterPostMControlMessages;
		}

		AccumulateControlMessage( Buffer, ControlStructure, Count, PLAYER_MAX_POSTM_MESSAGES, Table );
	    }
	}
	else
	{
	    report( severity_error, "Player_Generic_c::ProcessPostManifest - Unknown buffer type received - Implementation error.\n" );
	    Buffer->DecrementReferenceCount();
	}
    }

    report( severity_info, "3333 Holding control strutures %d\n", AccumulatedBeforeControlMessagesCount + AccumulatedAfterControlMessagesCount );

    //
    // Make sur no one will wait for these
    //

    Stream->ReTimeQueuedFrames	= false;

    //
    // Signal we have terminated
    //

    OS_LockMutex( &Lock );

    Stream->ProcessRunningCount--;

    if( Stream->ProcessRunningCount == 0 )
	OS_SetEvent( &Stream->StartStopEvent );

    OS_UnLockMutex( &Lock );
}


