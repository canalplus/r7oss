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

Source file name : player_process_collate_to_parse.cpp
Author :           Nick

Implementation of the process handling data transfer 
between the collation phase and the frame parsers of 
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

OS_TaskEntry(PlayerProcessCollateToParse)
{
PlayerStream_t		Stream = (PlayerStream_t)Parameter;

    Stream->Player->ProcessCollateToParse( Stream );

    OS_TerminateThread();
    return NULL;
}


// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Main process code
//

void   Player_Generic_c::ProcessCollateToParse(	PlayerStream_t		  Stream )
{
PlayerStatus_t			  Status;
RingStatus_t			  RingStatus;
Buffer_t			  Buffer;
BufferType_t			  BufferType;
unsigned int			  BufferIndex;
PlayerControlStructure_t	 *ControlStructure;
PlayerSequenceNumber_t		 *SequenceNumberStructure;
unsigned long long		  LastEntryTime;
unsigned long long		  SequenceNumber;
unsigned long long                MaximumActualSequenceNumberSeen;
unsigned int			  AccumulatedBeforeControlMessagesCount;
unsigned int			  AccumulatedAfterControlMessagesCount;
bool				  ProcessNow;
unsigned int			 *Count;
PlayerBufferRecord_t		 *Table;
unsigned int			  BufferLength;
bool				  DiscardBuffer;

    //
    // Set parameters
    //

    LastEntryTime				= OS_GetTimeInMicroSeconds();
    SequenceNumber				= INVALID_SEQUENCE_VALUE;
    MaximumActualSequenceNumberSeen             = 0;
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
	RingStatus	= Stream->CollatedFrameRing->Extract( (unsigned int *)(&Buffer), PLAYER_MAX_EVENT_WAIT );
	if( (RingStatus == RingNothingToGet) || Stream->Terminating )
	    continue;

	Buffer->GetType( &BufferType );
	Buffer->GetIndex( &BufferIndex );
	Buffer->TransferOwnership( IdentifierProcessCollateToParse );

	//
	// Deal with a coded frame buffer 
	//

	if( BufferType == Stream->CodedFrameBufferType )
	{
	    //
	    // Apply a sequence number to the buffer
	    //

	    Status	= Buffer->ObtainMetaDataReference( MetaDataSequenceNumberType, (void **)(&SequenceNumberStructure) );
	    if( Status != PlayerNoError )
	    {
	        report( severity_error, "Player_Generic_c::ProcessCollateToParse - Unable to obtain the meta data \"SequenceNumber\" - Implementation error\n" );
		Buffer->DecrementReferenceCount( IdentifierProcessCollateToParse );
		continue;
	    }

	    SequenceNumberStructure->TimeEntryInProcess0	= OS_GetTimeInMicroSeconds();
	    SequenceNumberStructure->DeltaEntryInProcess0	= SequenceNumberStructure->TimeEntryInProcess0 - LastEntryTime;
	    LastEntryTime					= SequenceNumberStructure->TimeEntryInProcess0;
	    SequenceNumberStructure->TimeEntryInProcess1	= INVALID_TIME;
	    SequenceNumberStructure->TimePassToCodec		= INVALID_TIME;
	    SequenceNumberStructure->TimeEntryInProcess2	= INVALID_TIME;
	    SequenceNumberStructure->TimePassToManifestor	= INVALID_TIME;
	    SequenceNumberStructure->TimeEntryInProcess3	= INVALID_TIME;

	    if( (Stream->MarkerInCodedFrameIndex != INVALID_INDEX) &&
		(Stream->MarkerInCodedFrameIndex == BufferIndex) ) 
	    {
		// This is a marker frame
		SequenceNumberStructure->MarkerFrame	= true;

		Stream->NextBufferSequenceNumber	= SequenceNumberStructure->Value + 1;
		Stream->DiscardingUntilMarkerFrameCtoP	= false;
		Stream->MarkerInCodedFrameIndex		= INVALID_INDEX;
	    }
	    else
	    {
		SequenceNumberStructure->MarkerFrame	= false;
		SequenceNumberStructure->Value		= Stream->NextBufferSequenceNumber++;
	    }

	    SequenceNumber				= SequenceNumberStructure->Value;
	    MaximumActualSequenceNumberSeen		= max(SequenceNumber, MaximumActualSequenceNumberSeen);

	    //
	    // Process any outstanding control messages to be applied before this buffer
	    //

	    ProcessAccumulatedControlMessages( 	Stream, 
						&AccumulatedBeforeControlMessagesCount,
						PLAYER_MAX_CTOP_MESSAGES,
						Stream->AccumulatedBeforeCtoPControlMessages, 
						SequenceNumber, INVALID_TIME );

	    //
	    // Pass the buffer to the frame parser for processing
	    // then release our hold on this buffer. When discarding we 
	    // do not pass on, unless we have a zero length buffer, used when 
	    // signalling an input jump.
	    //

	    Buffer->ObtainDataReference( NULL, &BufferLength, NULL );

	    DiscardBuffer	= !SequenceNumberStructure->MarkerFrame &&
				  (BufferLength != 0) &&
				  (Stream->UnPlayable || Stream->DiscardingUntilMarkerFrameCtoP);

//report( severity_error, "FP++\n" );
	    if( !DiscardBuffer )
		Stream->FrameParser->Input( Buffer );
//report( severity_error, "FP--\n" );

	    Buffer->DecrementReferenceCount( IdentifierProcessCollateToParse );

	    //
	    // Process any outstanding control messages to be applied after this buffer
	    //

	    ProcessAccumulatedControlMessages( 	Stream,
						&AccumulatedAfterControlMessagesCount,
						PLAYER_MAX_CTOP_MESSAGES,
						Stream->AccumulatedAfterCtoPControlMessages, 
						SequenceNumber, INVALID_TIME );
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
		    Table	= Stream->AccumulatedBeforeCtoPControlMessages;
		}
		else
		{
		    Count	= &AccumulatedAfterControlMessagesCount;
		    Table	= Stream->AccumulatedAfterCtoPControlMessages;
		}

		AccumulateControlMessage( Buffer, ControlStructure, Count, PLAYER_MAX_CTOP_MESSAGES, Table );
	    }
	}
	else
	{
	    report( severity_error, "Player_Generic_c::ProcessCollateToParse - Unknown buffer type received - Implementation error.\n" );
	    Buffer->DecrementReferenceCount();
	}
    }

    report( severity_info, "0000 Holding control strutures %d\n", AccumulatedBeforeControlMessagesCount + AccumulatedAfterControlMessagesCount );

    //
    // Signal we have terminated
    //

    OS_LockMutex( &Lock );

    Stream->ProcessRunningCount--;

    if( Stream->ProcessRunningCount == 0 )
	OS_SetEvent( &Stream->StartStopEvent );

    OS_UnLockMutex( &Lock );
}

