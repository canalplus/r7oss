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

Source file name : collator_base.cpp
Author :           Nick

Implementation of the base collator class for player 2.


Date        Modification                                    Name
----        ------------                                    --------
16-Nov-06   Created                                         Nick

************************************************************************/

// /////////////////////////////////////////////////////////////////////
//
//      Include any component headers

#include "collator2_base.h"
#include "stack_generic.h"

// /////////////////////////////////////////////////////////////////////////
//
// Locally defined constants
//

#define MAXIMUM_ACCUMULATED_START_CODES		2048

#define LIMITED_EARLY_INJECTION_WINDOW		500000		// Say half a second
#define MAXIMUM_LIMITED_EARLY_INJECTION_DELAY	42000		// Restrict to 42ms norm for 24fps
 
// /////////////////////////////////////////////////////////////////////////
//
// Locally defined structures
//

// /////////////////////////////////////////////////////////////////////////
//
//      The Constructor function
//

Collator2_Base_c::Collator2_Base_c( void )
{
    InitializationStatus        = CollatorError;

//

    OS_InitializeMutex( &PartitionLock );

    NextWriteInStartCodeList	= 0;
    NextReadInStartCodeList	= 0;
    AccumulatedStartCodeList	= NULL;

    ReverseFrameStack		= NULL;
    TemporaryHoldingStack	= NULL;

    Collator2_Base_c::Reset();

//

    InitializationStatus        = CollatorNoError;
}

// /////////////////////////////////////////////////////////////////////////
//
//      The Destructor function
//

Collator2_Base_c::~Collator2_Base_c(      void )
{
    Collator2_Base_c::Halt();
    Collator2_Base_c::Reset();
    OS_TerminateMutex( &PartitionLock );
}


// /////////////////////////////////////////////////////////////////////////
//
//      The Halt function, give up access to any registered resources
//

CollatorStatus_t   Collator2_Base_c::Halt(       void )
{
unsigned int	BufferAndFlag;
Buffer_t	Buffer;

//

    if( ReverseFrameStack != NULL )
    {
	while( ReverseFrameStack->NonEmpty() )
    	{
	    ReverseFrameStack->Pop( &BufferAndFlag );
	    Buffer	= (Buffer_t)(BufferAndFlag & ~1);
	    Buffer->DecrementReferenceCount( IdentifierCollator );
    	}
    }

//

    if( TemporaryHoldingStack != NULL )
    {
	while( TemporaryHoldingStack->NonEmpty() )
	{
	    TemporaryHoldingStack->Pop( &BufferAndFlag );
	    Buffer	= (Buffer_t)(BufferAndFlag & ~1);
	    Buffer->DecrementReferenceCount( IdentifierCollator );
	}
    }

//

    if( CodedFrameBuffer != NULL )
    {
	CodedFrameBuffer->DecrementReferenceCount( IdentifierCollator );
	CodedFrameBuffer        = NULL;
    }

    CodedFrameBufferPool        = NULL;
    CodedFrameBufferBase	= NULL;

    OutputRing                  = NULL;

    return BaseComponentClass_c::Halt();
}


// /////////////////////////////////////////////////////////////////////////
//
//      The Reset function release any resources, and reset all variable
//

CollatorStatus_t   Collator2_Base_c::Reset(      void )
{

//

    if( AccumulatedStartCodeList != NULL )
    {
	delete AccumulatedStartCodeList;
	AccumulatedStartCodeList	= NULL;
    }

    if( ReverseFrameStack != NULL )
    {
	delete ReverseFrameStack;
	ReverseFrameStack		= NULL;
    }

    if( TemporaryHoldingStack != NULL )
    {
	delete TemporaryHoldingStack;
	TemporaryHoldingStack		= NULL;
    }

//

    memset( &Configuration, 0x00, sizeof(Collator2Configuration_t) );
    memset( &PartitionPoints, 0x00, MAXIMUM_PARTITION_POINTS * sizeof(PartitionPoint_t) );

    CodedFrameBufferPool        	= NULL;
    CodedFrameBuffer            	= NULL;
    MaximumCodedFrameSize       	= 0;

    PlayDirection			= PlayForward;

    CodedFrameBufferFreeSpace		= 0;
    CodedFrameBufferUsedSpace		= 0;
    LargestFrameSeen			= 0;
    CodedFrameBufferBase        	= NULL;
    PartitionPointUsedCount		= 0;
    PartitionPointMarkerCount		= 0;
    PartitionPointSafeToOutputCount	= 0;
    NextPartition			= NULL;

    OutputRing                  	= NULL;

    LimitHandlingLastPTS		= INVALID_TIME;
    LimitHandlingJumpInEffect		= false;
    LimitHandlingJumpAt			= INVALID_TIME;
 
    LastFramePreGlitchPTS		= INVALID_TIME;
    FrameSinceLastPTS			= 0;

    return BaseComponentClass_c::Reset();
}


// /////////////////////////////////////////////////////////////////////////
//
//      The register output ring function function
//

CollatorStatus_t   Collator2_Base_c::RegisterOutputBufferRing(Ring_t                       Ring )
{
PlayerStatus_t  Status;

//

    OutputRing  = Ring;

    //
    // Obtain the class list, and the coded frame buffer pool
    //

    Player->GetCodedFrameBufferPool( Stream, &CodedFrameBufferPool, &MaximumCodedFrameSize );

    //
    // Attach the coded frame parameters to every element of the pool
    //

    Status      = CodedFrameBufferPool->AttachMetaData( Player->MetaDataCodedFrameParametersType );
    if( Status != BufferNoError )
    {
	report( severity_error, "Collator2_Base_c::RegisterOutputBufferRing - Failed to attach coded frame descriptor to all coded frame buffers.\n" );
	CodedFrameBufferPool    = NULL;
	return Status;
    }

    //
    // If we wish to collect start codes, create our master list, and attach one to each buffer
    //

    if( Configuration.GenerateStartCodeList )
    {
	AccumulatedStartCodeList	= (StartCodeList_t *)new unsigned char[SizeofStartCodeList(MAXIMUM_ACCUMULATED_START_CODES)];
	if( AccumulatedStartCodeList == NULL )
	{
	    report( severity_error, "Collator2_Base_c::RegisterOutputBufferRing - Failed to create accumulated start code list.\n" );
	    CodedFrameBufferPool        = NULL;
	    return Status;
	}	
	memset( AccumulatedStartCodeList, 0x00, SizeofStartCodeList(MAXIMUM_ACCUMULATED_START_CODES) );

	Status  = CodedFrameBufferPool->AttachMetaData( Player->MetaDataStartCodeListType, SizeofStartCodeList(Configuration.MaxStartCodes) );
	if( Status != BufferNoError )
	{
	    report( severity_error, "Collator2_Base_c::RegisterOutputBufferRing - Failed to attach start code list to all coded frame buffers.\n" );
	    CodedFrameBufferPool        = NULL;
	    return Status;
	}
    }

    //
    // Allocate the reverse play stacks
    //

    ReverseFrameStack		= new class StackGeneric_c( MAXIMUM_PARTITION_POINTS );
    TemporaryHoldingStack	= new class StackGeneric_c( MAXIMUM_PARTITION_POINTS );

    if( (ReverseFrameStack == NULL) ||
	(TemporaryHoldingStack == NULL) )
    {
	CodedFrameBufferPool    = NULL;
	report( severity_error, "Collator2_Base_c::RegisterOutputBufferRing - Failed to obtain reverse collation stacks.\n" );
	return PlayerInsufficientMemory;
    }

    //
    // Aquire an operating buffer, hopefully all of the memory available to this pool
    //

    Status      = CodedFrameBufferPool->GetBuffer( &CodedFrameBuffer, IdentifierCollator, MaximumCodedFrameSize, false, true );
    if( Status != BufferNoError )
    {
	CodedFrameBufferPool    = NULL;
	report( severity_error, "Collator2_Base_c::RegisterOutputBufferRing - Failed to obtain an operating buffer.\n" );
	return Status;
    }

//

    NonBlockingInput				= false;
    ExtendCodedFrameBufferAtEarliestOpportunity	= false;

    CodedFrameBufferUsedSpace			 = 0;
    CodedFrameBuffer->ObtainDataReference( &CodedFrameBufferFreeSpace, NULL, (void **)(&CodedFrameBufferBase) );
    InitializePartition();

    //
    // Go live
    //

    SetComponentState( ComponentRunning );

    return CollatorNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
//      The input function
//

CollatorStatus_t   Collator2_Base_c::Input(	PlayerInputDescriptor_t	 *Input,
						unsigned int		  DataLength,
						void			 *Data,
						bool			  NonBlocking,
						unsigned int		 *DataLengthRemaining )
{
CollatorStatus_t	 Status;
Rational_t		 Speed;
PlayDirection_t		 Direction;
PlayDirection_t		 PreviousDirection;
unsigned char		 Policy;

//

    AssertComponentState( "Collator2_PesVideo_c::Input", ComponentRunning );

    //
    // Initialize return value
    //

    if( DataLengthRemaining != NULL )
	*DataLengthRemaining	= DataLength;

    //
    // Are we in reverse, and operating in reversible mode
    //

    PreviousDirection	= PlayDirection;
    PlayDirection	= PlayForward;

    Player->GetPlaybackSpeed( Playback, &Speed, &Direction );
    if( Direction == PlayBackward )
    {
	Policy	= Player->PolicyValue( Playback, Stream, PolicyOperateCollator2InReversibleMode );
	if( Policy == PolicyValueApply )
	    PlayDirection	= PlayBackward;
    }

    //
    // Check for switch of direction
    //

    if( PreviousDirection != PlayDirection )
    {
	if( (PartitionPointUsedCount != 0) || (NextPartition->PartitionSize != 0) )
	{
	    report( severity_error, "Collator2_Base_c::Input(%s) - Attempt to switch direction without proper flushing.\n", Configuration.CollatorName );
	    PlayDirection	= PreviousDirection;
	    return CollatorError;
	}

	InitializePartition();		// Force base to be re-calculated
    }

    //
    // Perform input entry activity, may result in a would block status
    //

    OS_LockMutex( &PartitionLock );
    Status	= InputEntry( Input, DataLength, Data, NonBlocking );

    //
    // Process the input
    //

    if( Status == CollatorNoError )
    {
	if( PlayDirection == PlayForward )
	    Status	= ProcessInputForward( DataLength, Data, DataLengthRemaining );
	else
	    Status	= ProcessInputBackward( DataLength, Data, DataLengthRemaining );
    }

    //
    // Handle exit
    //

    if( Status == CollatorNoError )
        Status = InputExit();

    OS_UnLockMutex( &PartitionLock );
    return Status;
}


// /////////////////////////////////////////////////////////////////////////
//
//      Protected - This function is called when we enter input 
//	for a specific collator

CollatorStatus_t   Collator2_Base_c::InputEntry(
			PlayerInputDescriptor_t	 *Input,
			unsigned int		  DataLength,
			void			 *Data,
			bool			  NonBlocking )
{
CollatorStatus_t	Status;

    //
    // If the input descriptor provides any hints about playback or
    // presentation time then initialize CodedFrameParameters
    // with these values.
    //

    if( Input->PlaybackTimeValid && !NextPartition->CodedFrameParameters.PlaybackTimeValid )
    {
	NextPartition->CodedFrameParameters.PlaybackTimeValid = true;
	NextPartition->CodedFrameParameters.PlaybackTime      = Input->PlaybackTime;
    }

    if( Input->DecodeTimeValid && !NextPartition->CodedFrameParameters.DecodeTimeValid )
    {
	NextPartition->CodedFrameParameters.DecodeTimeValid   = true;
	NextPartition->CodedFrameParameters.DecodeTime        = Input->DecodeTime;
    }

    NextPartition->CodedFrameParameters.DataSpecificFlags     |= Input->DataSpecificFlags;

    //
    // Do we have any activity to complete that would have caused us to block previously
    //

    Status		= CollatorNoError;
    NonBlockingInput	= NonBlocking;

    if( ExtendCodedFrameBufferAtEarliestOpportunity )
	Status	= PartitionOutput();

//

    return Status;
}


// /////////////////////////////////////////////////////////////////////////
//
//      Protected - This function is called when we exit input 
//	for a specific collator

CollatorStatus_t   Collator2_Base_c::InputExit(		void )
{
CollatorStatus_t	Status;

//

    Status		= CollatorNoError;

    if( PartitionPointSafeToOutputCount != 0 )
	Status	= PartitionOutput();

//

    return Status;
}


// /////////////////////////////////////////////////////////////////////////
//
//    Protected - Support functions for adjusting partition boundary 
//    to recognize data added or removed


void   Collator2_Base_c::EmptyCurrentPartition(	void )
{
    MoveCurrentPartitionBoundary( -NextPartition->PartitionSize );
    NextWriteInStartCodeList		= NextPartition->StartCodeListIndex;		// Wind back the start code list
    NextPartition->NumberOfStartCodes	= 0;
}

// ------

void   Collator2_Base_c::MoveCurrentPartitionBoundary(	int		  Bytes )
{
   if( PlayDirection != PlayForward )
	NextPartition->PartitionBase	-= Bytes;

    NextPartition->PartitionSize	+= Bytes;
    CodedFrameBufferUsedSpace		+= Bytes;
    CodedFrameBufferFreeSpace		-= Bytes;
}

// ------

void   Collator2_Base_c::AccumulateOnePartition(	void )
{
CollatorStatus_t	Status;
unsigned char           TerminalStartCode[4];

    //
    // If discarding, preserve flags, but  dump data
    //

    if( DiscardingData )
	EmptyCurrentPartition();

    //
    // Do we have anything to accumulate
    //

    if( (NextPartition->PartitionSize == 0) &&
        !NextPartition->CodedFrameParameters.StreamDiscontinuity &&
        !NextPartition->CodedFrameParameters.ContinuousReverseJump )
        return;

//

    if( CodedFrameBuffer == NULL )
        return;

    //
    // Do we have any terminate code to append in forward play
    // NOTE we preload the terminate code in reverse play
    //

    if( (PlayDirection == PlayForward) && 
	(NextPartition->PartitionSize != 0) && 
	Configuration.InsertFrameTerminateCode )
    {
	TerminalStartCode[0]    = 0x00;
	TerminalStartCode[1]    = 0x00;
	TerminalStartCode[2]    = 0x01;
	TerminalStartCode[3]    = Configuration.TerminalCode;

	Status  = AccumulateData( 4, TerminalStartCode );
	if( Status != CollatorNoError )
	    report( severity_error, "Collator2_Base_c::AccumulateOnePartition - Failed to add terminal start code.\n" );
    }

//

    if( NextPartition->PartitionSize > LargestFrameSeen )
	LargestFrameSeen	= NextPartition->PartitionSize;

//

    PartitionPointUsedCount++;
    NextPartition->NumberOfStartCodes	= NextWriteInStartCodeList - NextPartition->StartCodeListIndex;

    InitializePartition();

//

    if( (PlayDirection != PlayForward) && 
	(NextPartition->PartitionSize != 0) && 
	Configuration.InsertFrameTerminateCode )
    {
	TerminalStartCode[0]    = 0x00;
	TerminalStartCode[1]    = 0x00;
	TerminalStartCode[2]    = 0x01;
	TerminalStartCode[3]    = Configuration.TerminalCode;

	Status  = AccumulateData( 4, TerminalStartCode );
	if( Status != CollatorNoError )
	    report( severity_error, "Collator2_Base_c::AccumulateOnePartition - Failed to add terminal start code.\n" );
    }
}

// ------

void   Collator2_Base_c::InitializePartition(	void )
{
unsigned int	CodedFrameBufferSize;

//

    NextPartition			= &PartitionPoints[PartitionPointUsedCount];

    memset( NextPartition, 0x00, sizeof(PartitionPoint_t) );
    NextPartition->StartCodeListIndex	= NextWriteInStartCodeList;

    if( PlayDirection == PlayForward )
	NextPartition->PartitionBase	= CodedFrameBufferBase + CodedFrameBufferUsedSpace;
    else
    {
	CodedFrameBuffer->ObtainDataReference( &CodedFrameBufferSize, NULL, NULL );
	NextPartition->PartitionBase	= CodedFrameBufferBase + CodedFrameBufferSize - CodedFrameBufferUsedSpace;
    }
}


// /////////////////////////////////////////////////////////////////////////
//
//      Protected - This function handles the output of a partition. This
//	is easy when we are in forward play, but depends on the associated 
//	header status bits when we are in reverse (IE we stack frames until 
//	we hit a confirmed reversal point).
//
//	Note we stack the buffer, with the reversible point flag encoded in 
//	bit 0, this assumes that buffer will be alligned to 16 bits at least 
//	(32 expected). since this may not be true on future processors, we 
//	test this assumption.
//

CollatorStatus_t   Collator2_Base_c::OutputOnePartition( PartitionPoint_t	*Descriptor )
{
BufferStatus_t		 Status;
Buffer_t		 Buffer;
CodedFrameParameters_t	*Parameters;
unsigned int 		 BufferAndFlag;

//

    Descriptor->Buffer->SetUsedDataSize( Descriptor->PartitionSize );

//

    if( PlayDirection == PlayForward )
    {
	CheckForGlitchPromotion( Descriptor );
	DelayForInjectionThrottling( Descriptor );
	OutputRing->Insert( (unsigned int )Descriptor->Buffer );
    }
    else
    {
	if( ((unsigned int)Descriptor->Buffer & 0x1) != 0 )
	    report( severity_fatal, "Collator2_Base_c::OutputOnePartition - We assume word allignment of buffer structure - Implementation error.\n" );

 	CheckForGlitchPromotion( Descriptor );

	BufferAndFlag	= (unsigned int)Descriptor->Buffer | 
			  (((Descriptor->FrameFlags & FrameParserHeaderFlagPossibleReversiblePoint) != 0) ? 1 : 0);

	ReverseFrameStack->Push( BufferAndFlag );

	if( (Descriptor->FrameFlags & FrameParserHeaderFlagConfirmReversiblePoint) != 0 )
	{
	    while( ReverseFrameStack->NonEmpty() )
	    {
		ReverseFrameStack->Pop( &BufferAndFlag );

		if( (BufferAndFlag & 1) != 0 )
		    break;

		TemporaryHoldingStack->Push( BufferAndFlag );
	    }

	    if( (BufferAndFlag & 1) == 0 )
	    {
		report( severity_error, "Collator2_Base_c::OutputOnePartition - Failed to find a reversible point.\n" );
	    }
	    else
	    {
		//
		// Smooth reverse flag
		//

		Buffer	= (Buffer_t)(BufferAndFlag & ~1);
		Status	= Buffer->ObtainMetaDataReference( Player->MetaDataCodedFrameParametersType, (void **)(&Parameters) );
		if( Status != BufferNoError )
		{
		    report( severity_error, "Collator2_Base_c::OutputOnePartition - Unable to obtain the meta data coded frame parameters.\n" );
		    return Status;
		}

		Parameters->StreamDiscontinuity		= true;
		Parameters->ContinuousReverseJump	= true;

//

		OutputRing->Insert( (unsigned int)Buffer );
		while( ReverseFrameStack->NonEmpty() )
		{
		    ReverseFrameStack->Pop( &BufferAndFlag );
		    OutputRing->Insert( BufferAndFlag & ~1 );
		}
	    }

	    while( TemporaryHoldingStack->NonEmpty() )
	    {
		TemporaryHoldingStack->Pop( &BufferAndFlag );
		ReverseFrameStack->Push( BufferAndFlag );
	    }
	}
    }

//
    return CollatorNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
//      Protected - This function splits the coded data into two partitions,
//	and copied in the appropriate meta data.
//

CollatorStatus_t   Collator2_Base_c::PerformOnePartition(
						PartitionPoint_t	*Descriptor,
						Buffer_t		*NewPartition,
						Buffer_t		*OutputPartition,
						unsigned int		 SizeOfFirstPartition )
{
unsigned int		 i;
BufferStatus_t		 Status;
CodedFrameParameters_t	*CodedFrameParameters;
StartCodeList_t		*StartCodeList;
PackedStartCode_t	 Code;
unsigned long long 	 NewOffset;

    //
    // Do the partition
    //

    Status	= CodedFrameBuffer->PartitionBuffer( SizeOfFirstPartition, false, NewPartition );
    if( Status != BufferNoError )
    {
	report( severity_error, "Collator2_Base_c::PerformOnePartition - Failed to partition a buffer.\n" );
	return Status;
    }

    //
    // Mark down the accumulated data size
    //

    CodedFrameBufferUsedSpace	-= Descriptor->PartitionSize;

    //
    // Copy in the coded frame parameters appropriate to this partition
    //

    Descriptor->Buffer	= *OutputPartition;
    Status	= Descriptor->Buffer->ObtainMetaDataReference( Player->MetaDataCodedFrameParametersType, (void **)(&CodedFrameParameters) );
    if( Status != BufferNoError )
    {
	report( severity_error, "Collator2_Base_c::PerformOnePartition - Unable to obtain the meta data coded frame parameters.\n" );
	return Status;
    }

    memcpy( CodedFrameParameters, &Descriptor->CodedFrameParameters, sizeof(CodedFrameParameters_t) );
    Descriptor->Buffer->SetUsedDataSize( Descriptor->PartitionSize );

    //
    // Copy in the start code list
    //

    if( Configuration.GenerateStartCodeList )
    {
	if( NextReadInStartCodeList != Descriptor->StartCodeListIndex )
	    report( severity_fatal, "Collator2_Base_c::PerformOnePartition - Start code list in dubious condition (%2d %2d %d) - Implementation error\n", NextReadInStartCodeList, NextWriteInStartCodeList, Descriptor->StartCodeListIndex );


	Status  = Descriptor->Buffer->ObtainMetaDataReference( Player->MetaDataStartCodeListType, (void **)(&StartCodeList) );
	if( Status != BufferNoError )
	{
	    report( severity_error, "Collator2_Base_c::PerformOnePartition - Unable to obtain the meta data start code list.\n" );
	    return Status;
	}

	if( PlayDirection == PlayForward )
	{
	    for( i=0; i<Descriptor->NumberOfStartCodes; i++ )
		StartCodeList->StartCodes[i] 	= AccumulatedStartCodeList->StartCodes[(NextReadInStartCodeList + i) % MAXIMUM_ACCUMULATED_START_CODES];
	}
	else
	{
	    for( i=0; i<Descriptor->NumberOfStartCodes; i++ )
	    {
		Code				= AccumulatedStartCodeList->StartCodes[(NextReadInStartCodeList + Descriptor->NumberOfStartCodes - 1 - i) % MAXIMUM_ACCUMULATED_START_CODES];
		NewOffset			= Descriptor->PartitionSize - ExtractStartCodeOffset(Code);
		StartCodeList->StartCodes[i] 	= PackStartCode( NewOffset, ExtractStartCodeCode(Code) );
	    }
	}

	StartCodeList->NumberOfStartCodes	 = Descriptor->NumberOfStartCodes;
	NextReadInStartCodeList			+= Descriptor->NumberOfStartCodes;
    }

//

    return CollatorNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
//      Protected - This is a fairly crucial function, it takes the 
//	accumulated data buffer and outputs the individual collated frames.
//	it also manages the aquistition of a new operating buffer when 
//	appropriate.
//

CollatorStatus_t   Collator2_Base_c::PartitionOutput( void )
{
unsigned int		  i;
CollatorStatus_t	  Status;
unsigned int		  SucessfullyOutputPartitions;
unsigned int		  Partition;
unsigned int		  TotalBuffers;
unsigned int		  BuffersUsed;
unsigned int		  KnownFreeBuffers;
unsigned int		  FirstPartitionSize;
Buffer_t		  NewBuffer;
Buffer_t		 *RetainedBuffer;
Buffer_t		 *OutputBuffer;
unsigned int		  CodedFrameBufferSize;
unsigned int		  AcceptableBufferSpace;
unsigned int		  MinimumSoughtSize;
unsigned int		  LargestFreeMemoryBlock;
unsigned int		  NewBufferSize;
unsigned char		 *NewBufferBase;
unsigned char		 *TransferFrom;
unsigned char		 *TransferTo;

    //
    // First we perform the actual partitioning
    //

    KnownFreeBuffers		= 0;
    SucessfullyOutputPartitions	= 0;

    for( Partition = 0; Partition<PartitionPointSafeToOutputCount; Partition++ )
    {
	//
	// Can we perform a partition, without blocking when we shouldn't
	//

	if( KnownFreeBuffers == 0 )
	{
	    CodedFrameBufferPool->GetPoolUsage( &TotalBuffers, &BuffersUsed );
	    KnownFreeBuffers	= (TotalBuffers - BuffersUsed);
	}

	if( (KnownFreeBuffers == 0) && NonBlockingInput )
	    break;

	//
	// Ok we can perform a partition
	//

	if( PlayDirection == PlayForward )
	{
	    RetainedBuffer	= &NewBuffer;
	    OutputBuffer	= &CodedFrameBuffer;
	    FirstPartitionSize	= PartitionPoints[Partition].PartitionSize;
	}
	else
	{
	    RetainedBuffer	= &CodedFrameBuffer;
	    OutputBuffer	= &NewBuffer;
	    FirstPartitionSize	= (PartitionPoints[Partition].PartitionBase - CodedFrameBufferBase);
	}

	Status	= PerformOnePartition( &PartitionPoints[Partition], &NewBuffer, OutputBuffer, FirstPartitionSize );
	if( Status != PlayerNoError )
	    break;

	KnownFreeBuffers--;

	Status	= OutputOnePartition( &PartitionPoints[Partition] );
	if( Status != PlayerNoError )
	    break;

	CodedFrameBuffer	 	 = *RetainedBuffer;
	SucessfullyOutputPartitions++;
    }

    //
    // Now we have finished partitioning, do we need to compact the partioning list 
    // NOTE <= in loop copies the current partition also.
    //

    if( SucessfullyOutputPartitions != 0 )
    {
	for( i=0; i<=(PartitionPointUsedCount - SucessfullyOutputPartitions); i++ )
	    PartitionPoints[i]	= PartitionPoints[Partition + i];

	PartitionPointUsedCount		= PartitionPointUsedCount - SucessfullyOutputPartitions;
	PartitionPointMarkerCount	= PartitionPointMarkerCount - SucessfullyOutputPartitions;
	PartitionPointSafeToOutputCount	= PartitionPointSafeToOutputCount - SucessfullyOutputPartitions;
	NextPartition			= &PartitionPoints[PartitionPointUsedCount];
    }

    //
    // Try to extend the cumulative buffer we are playing with
    //

    CodedFrameBuffer->ExtendBuffer( NULL, (PlayDirection == PlayForward) );
    CodedFrameBuffer->ObtainDataReference( &CodedFrameBufferSize, NULL, (void **)(&CodedFrameBufferBase) );

    CodedFrameBufferFreeSpace			= CodedFrameBufferSize - CodedFrameBufferUsedSpace;

    //
    // Has the extension led to a buffer of an acceptable size, 
    // if not then can we get a new one, and transfer any data we have to it.
    //

    AcceptableBufferSpace			= ((NextPartition->PartitionSize > LargestFrameSeen) ? 
							min((NextPartition->PartitionSize + (MaximumCodedFrameSize/16)), MaximumCodedFrameSize) :
							(LargestFrameSeen + MINIMUM_ACCUMULATION_HEADROOM) ) - NextPartition->PartitionSize;
    MinimumSoughtSize				= CodedFrameBufferUsedSpace + AcceptableBufferSpace;

    ExtendCodedFrameBufferAtEarliestOpportunity	= false;

    if( CodedFrameBufferFreeSpace < AcceptableBufferSpace )
    {
	CodedFrameBufferPool->GetPoolUsage( &TotalBuffers, &BuffersUsed, NULL, NULL, NULL, &LargestFreeMemoryBlock );
	KnownFreeBuffers	= (TotalBuffers - BuffersUsed);
	if( !NonBlockingInput || ((KnownFreeBuffers != 0) && (LargestFreeMemoryBlock >= MinimumSoughtSize)) )
	{
	    //
	    // We are going to get a new buffer, minimise our use of the 
	    // current one (NOTE no shrink, it gives us jip when we are reversing). 
	    // No point holding onto memory that we aren't using.
	    //

	    if( CodedFrameBufferUsedSpace == 0 )
		CodedFrameBuffer->DecrementReferenceCount( IdentifierCollator );

	    //
	    // Get a new one
	    //

	    Status      = CodedFrameBufferPool->GetBuffer( &NewBuffer, IdentifierCollator, MinimumSoughtSize, false, true );
	    if( Status != BufferNoError )
	    {
		report( severity_error, "Collator2_Base_c::PartitionOutput - Failed to obtain a new operating buffer.\n" );
		return Status;
	    }

	    //
	    // Move to the new buffer
	    //

	    NewBuffer->ObtainDataReference( &NewBufferSize, NULL, (void **)(&NewBufferBase) );

	    if( PlayDirection == PlayForward )
	    {
		TransferFrom	= CodedFrameBufferBase;
		TransferTo	= NewBufferBase;
	    }
	    else
	    {
		TransferFrom	= CodedFrameBufferBase + CodedFrameBufferSize - CodedFrameBufferUsedSpace;
		TransferTo	= NewBufferBase + NewBufferSize - CodedFrameBufferUsedSpace;
	    }

	    if( CodedFrameBufferUsedSpace != 0 )
	    {
		memcpy( TransferTo, TransferFrom, CodedFrameBufferUsedSpace );

		// Having done the transfer we can now release the old buffer
		CodedFrameBuffer->DecrementReferenceCount( IdentifierCollator );
	    }

	    //
	    // Move any current partition pointers to the new buffer
	    //    NOTE it may be possible to have zero length partitions
	    //         even if there is no actual data associated with 
	    //	       them (discontinuities etc...)
	    //

	    for( i=0; i<=PartitionPointUsedCount; i++ )
		PartitionPoints[i].PartitionBase	+= TransferTo - TransferFrom;

	    //
	    // Complete the transfer to the new buffer by updating pointers
	    //

	    CodedFrameBuffer				= NewBuffer;
	    CodedFrameBufferBase			= NewBufferBase;
	    CodedFrameBufferFreeSpace			= NewBufferSize - CodedFrameBufferUsedSpace;
	}
	else
	{
	    ExtendCodedFrameBufferAtEarliestOpportunity	= true;
	}
    }

    return CollatorNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
//      The Frame Flush function
//

CollatorStatus_t   Collator2_Base_c::FrameFlush(         void )
{
CollatorStatus_t        Status;
bool			PreservedNonBlocking;

//

    OS_LockMutex( &PartitionLock );
    AccumulateOnePartition();

    if( PlayDirection == PlayForward )
	PartitionPointSafeToOutputCount	= 0;
    else
	PartitionPointSafeToOutputCount	= PartitionPointMarkerCount;	// If we are flushing, then we should move the marker and safe to output pointers

    PartitionPointMarkerCount		= PartitionPointUsedCount;

    PreservedNonBlocking	= NonBlockingInput;
    NonBlockingInput		= false;
    Status			= PartitionOutput();
    NonBlockingInput		= PreservedNonBlocking;
    OS_UnLockMutex( &PartitionLock );

//

    return Status;
}


// /////////////////////////////////////////////////////////////////////////
//
//      The discard all accumulated data for the current partition function
//

CollatorStatus_t   Collator2_Base_c::DiscardAccumulatedData(     void )
{
    if( CodedFrameBuffer != NULL )
    {
	OS_LockMutex( &PartitionLock );
#if 0
	EmptyCurrentPartition();		// Empty to handle correct accounting
	InitializePartition();
#else
	while( PartitionPointUsedCount > PartitionPointSafeToOutputCount )
	{
	    EmptyCurrentPartition();
	    PartitionPointUsedCount--;
	    NextPartition		= &PartitionPoints[PartitionPointUsedCount];
	}
	PartitionPointMarkerCount	= PartitionPointSafeToOutputCount;
	EmptyCurrentPartition();
	InitializePartition();
#endif
	OS_UnLockMutex( &PartitionLock );
    }
    return CollatorNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
//      Handle a jump
//

CollatorStatus_t   Collator2_Base_c::InputJump(
					bool                      SurplusDataInjected,
					bool                      ContinuousReverseJump )
{
    AssertComponentState( "Collator2_Base_c::InputJump", ComponentRunning );

//

    if( SurplusDataInjected )
	DiscardAccumulatedData();
    else
	FrameFlush();

    FrameParser->ResetCollatedHeaderState();

    NextPartition->CodedFrameParameters.StreamDiscontinuity           = true;
    NextPartition->CodedFrameParameters.FlushBeforeDiscontinuity      = SurplusDataInjected;
    NextPartition->CodedFrameParameters.ContinuousReverseJump         = ContinuousReverseJump;
    FrameFlush();

    return CollatorNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
//      Handle a glitch
//

CollatorStatus_t   Collator2_Base_c::InputGlitch( void )
{
    AssertComponentState( "Collator2_Base_c::InputGlitch", ComponentRunning );

//

    DiscardAccumulatedData();

    NextPartition->Glitch	= true;

    return CollatorNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
//      Handle a glitch
//

CollatorStatus_t   Collator2_Base_c::NonBlockingWriteSpace( 	unsigned int		 *Size )
{
    AssertComponentState( "Collator2_Base_c::NonBlockingWriteSpace", ComponentRunning );

    *Size	= (CodedFrameBufferFreeSpace > MINIMUM_ACCUMULATION_HEADROOM) ? 0 : (CodedFrameBufferFreeSpace - MINIMUM_ACCUMULATION_HEADROOM);

    return CollatorNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
//      Protected - The accumulate data into the buffer function
//

CollatorStatus_t   Collator2_Base_c::AccumulateData(
						unsigned int              Length,
						unsigned char            *Data )
{
CollatorStatus_t	Status;

//

    if( (NextPartition->PartitionSize + Length) > MaximumCodedFrameSize )
    {
      report( severity_error, "Collator2_Base_c::AccumulateData - Buffer overflow.\n");

      EmptyCurrentPartition();		// Dump any collected data in the current partition
      InitializePartition();

      return CollatorBufferOverflow;
    }

//

    if( CodedFrameBufferFreeSpace < Length )
    {
	Status	= PartitionOutput();
	if( Status != CollatorNoError )
	{
	    report( severity_error, "Collator2_Base_c::AccumulateData - Output of partitions failed.\n" );
	    return Status;
	}

	if( CodedFrameBufferFreeSpace < Length )
	    return CollatorWouldBlock;
    }
//

    memcpy( NextPartition->PartitionBase + ((PlayDirection == PlayForward) ? NextPartition->PartitionSize : -Length), Data, Length );
    MoveCurrentPartitionBoundary( Length );

//

    return CollatorNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
//      Protected - The accumulate data into the buffer function
//

CollatorStatus_t   Collator2_Base_c::AccumulateStartCode(	PackedStartCode_t	Code )
{

    if( !Configuration.GenerateStartCodeList )
	return CollatorNoError;

//

    if( (NextWriteInStartCodeList - NextReadInStartCodeList) >= MAXIMUM_ACCUMULATED_START_CODES )
    {
	report( severity_error, "Collator2_Base_c::AccumulateStartCode - Start code list overflow.\n" );
	return CollatorBufferOverflow;
    }

    AccumulatedStartCodeList->StartCodes[(NextWriteInStartCodeList++) % MAXIMUM_ACCUMULATED_START_CODES]      = Code;

    return CollatorNoError;
}

// /////////////////////////////////////////////////////////////////////////
//
//      Private - Check if we should promote a glitch to a full blown 
// 		  discontinuity. This works by checking pts flow around
//		  the glitch.
//

void   Collator2_Base_c::CheckForGlitchPromotion( PartitionPoint_t	 *Descriptor )
{
long long 		 DeltaPTS;
long long		 Range;

    if( Descriptor->CodedFrameParameters.PlaybackTimeValid )
    {
	//
	// Handle any glitch promotion
	//     We promote if there is a glitch, and if the 
	//     PTS varies by more than the maximum of 1/4 second, and 40ms times the number of frames that have passed since a pts.
	//

	if( Descriptor->Glitch && 
	    (LastFramePreGlitchPTS != INVALID_TIME) )
	{
	    DeltaPTS		= Descriptor->CodedFrameParameters.PlaybackTime - LastFramePreGlitchPTS;
	    Range		= max( 22500, (3600 * FrameSinceLastPTS) );
	    if( !inrange(DeltaPTS, -Range, Range) )
	    {
		report( severity_info, "Collator2_Base_c::CheckForGlitchPromotion (%d) Promoted\n", Configuration.GenerateStartCodeList );
		Descriptor->CodedFrameParameters.StreamDiscontinuity           = true;
		Descriptor->CodedFrameParameters.FlushBeforeDiscontinuity      = false;
		Descriptor->CodedFrameParameters.ContinuousReverseJump         = false;
	    }
	}

	//
	// Remember the frame pts
	//

	Descriptor->Glitch	= false;
	LastFramePreGlitchPTS	= Descriptor->CodedFrameParameters.PlaybackTime;
	FrameSinceLastPTS	= 0;
    }

    FrameSinceLastPTS++;
}


// /////////////////////////////////////////////////////////////////////////
//
//      Private - If the input is to be throttled, the the delay 
//		  will occur here.
//

void   Collator2_Base_c::DelayForInjectionThrottling( PartitionPoint_t	 *Descriptor )
{
PlayerStatus_t		 Status;
unsigned char		 Policy;
Rational_t		 Speed;
PlayDirection_t		 Direction;
unsigned long long	 StreamDelay;
unsigned long long	 SystemPlaybackTime;
unsigned long long	 Now;
unsigned long long	 CurrentPTS;
long long 		 DeltaPTS;
long long		 EarliestInjectionTime;
long long		 Delay;

    //
    // Is throttling enabled, and do we have a pts to base the limit on
    //

    Policy	= Player->PolicyValue( Playback, Stream, PolicyLimitInputInjectAhead );
    if( (Policy != PolicyValueApply) || !Descriptor->CodedFrameParameters.PlaybackTimeValid )
	return;

    Player->GetPlaybackSpeed( Playback, &Speed, &Direction );
    if( Direction == PlayBackward )
        return;

    //
    // Obtain the relevent data
    //

    Status	= OutputTimer->GetStreamStartDelay( &StreamDelay );
    if( Status == PlayerNoError )
	Status	= Player->TranslateNativePlaybackTime( Playback, Descriptor->CodedFrameParameters.PlaybackTime, &SystemPlaybackTime );
    if( Status != PlayerNoError )
	return;

    //
    // Watch out for jumping PTS values, they can confuse us
    //

    if( LimitHandlingLastPTS == INVALID_TIME )
	LimitHandlingLastPTS	= Descriptor->CodedFrameParameters.PlaybackTime;

//

    DeltaPTS			= Descriptor->CodedFrameParameters.PlaybackTime - LimitHandlingLastPTS;
    if( !inrange(DeltaPTS, -90000, 90000) )
    {
	LimitHandlingJumpInEffect	= true;
	LimitHandlingJumpAt		= Descriptor->CodedFrameParameters.PlaybackTime;
    }

    LimitHandlingLastPTS	= Descriptor->CodedFrameParameters.PlaybackTime;

//

    if( LimitHandlingJumpInEffect )
    {
	Status = Player->RetrieveNativePlaybackTime( Playback, &CurrentPTS );
	if( (Status != PlayerNoError) || 
	    !inrange((CurrentPTS - LimitHandlingJumpAt), 0, 16*90000) )
	    return;

	LimitHandlingJumpInEffect	= false;
    }

    //
    // Calculate and perform a delay if necessary
    //

    Now				= OS_GetTimeInMicroSeconds();
    EarliestInjectionTime	= SystemPlaybackTime - StreamDelay - LIMITED_EARLY_INJECTION_WINDOW;

    Delay			= EarliestInjectionTime - (long long)Now;
    if( Delay <= 1000 )
	return;

    if( Delay > MAXIMUM_LIMITED_EARLY_INJECTION_DELAY )
	Delay			= MAXIMUM_LIMITED_EARLY_INJECTION_DELAY;

    OS_SleepMilliSeconds( Delay / 1000 ); 
}
