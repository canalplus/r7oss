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

////////////////////////////////////////////////////////////////////////
///     \class Collator_Base_c
///
///     Provides a buffer management framework to assist collator implementation.
///


// /////////////////////////////////////////////////////////////////////
//
//      Include any component headers

#include "collator_base.h"

// /////////////////////////////////////////////////////////////////////////
//
// Locally defined constants
//

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

Collator_Base_c::Collator_Base_c( void )
{
    InitializationStatus        = CollatorError;

//

    OS_InitializeMutex( &Lock );

    InputEntryDepth		= 0;

    Collator_Base_c::Reset();

//

    InitializationStatus        = CollatorNoError;
}

// /////////////////////////////////////////////////////////////////////////
//
//      The Destructor function
//

Collator_Base_c::~Collator_Base_c(      void )
{
    Collator_Base_c::Halt();
    Collator_Base_c::Reset();
    OS_TerminateMutex( &Lock );
}


// /////////////////////////////////////////////////////////////////////////
//
//      The Halt function, give up access to any registered resources
//

CollatorStatus_t   Collator_Base_c::Halt(       void )
{
unsigned int Count;

    //
    // Make sure input is not still running
    //

    OS_LockMutex( &Lock );

    for( Count = 0; (InputEntryDepth != 0); Count++ )
    {
	OS_UnLockMutex( &Lock );
	OS_SleepMilliSeconds( 10 );
	OS_LockMutex( &Lock );

	if( (Count % 100) == 99 )
	{
	    report( severity_fatal, "Collator_Base_c::Halt - Input still ongoing.\n" );
	    break;
	}
    }

    //
    // Tidy up
    //

    if( CodedFrameBuffer != NULL )
    {
	CodedFrameBuffer->DecrementReferenceCount( IdentifierCollator );
	CodedFrameBuffer        = NULL;
    }

    CodedFrameBufferPool        = NULL;
    BufferBase                  = NULL;
    CodedFrameParameters        = NULL;
    StartCodeList               = NULL;
    OutputRing                  = NULL;

    BaseComponentClass_c::Halt();

//

    OS_UnLockMutex( &Lock );
    return CollatorNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
//      The Reset function release any resources, and reset all variable
//

CollatorStatus_t   Collator_Base_c::Reset(      void )
{
    memset( &Configuration, 0x00, sizeof(CollatorConfiguration_t) );

    Configuration.DetermineFrameBoundariesByPresentationToFrameParser	= false;

//

    CodedFrameBufferPool        = NULL;
    CodedFrameBuffer            = NULL;
    MaximumCodedFrameSize       = 0;

    AccumulatedDataSize         = 0;
    BufferBase                  = NULL;
    StartCodeList               = NULL;

    OutputRing                  = NULL;

    LimitHandlingLastPTS	= INVALID_TIME;
    LimitHandlingJumpInEffect	= false;
    LimitHandlingJumpAt		= INVALID_TIME;
 
    Glitch			= false;
    LastFramePreGlitchPTS	= INVALID_TIME;
    FrameSinceLastPTS		= 0;

    InputExitPerformFrameFlush	= false;

    return BaseComponentClass_c::Reset();
}


// /////////////////////////////////////////////////////////////////////////
//
//      Override for component base class set module parameters function
//

CollatorStatus_t   Collator_Base_c::SetModuleParameters(
						unsigned int              ParameterBlockSize,
						void                     *ParameterBlock )
{
CollatorParameterBlock_t        *CollatorParameterBlock = (CollatorParameterBlock_t *)ParameterBlock;
CollatorStatus_t                 Status;

//

    if( ParameterBlockSize != sizeof(CollatorParameterBlock_t) )
    {
	report( severity_error, "Collator_Base_c::SetModuleParameters - Invalid parameter block.\n" );
	return CollatorError;
    }

//

    switch( CollatorParameterBlock->ParameterType )
    {
	case CollatorConfiguration:

			//
			// Configuration change, if we have a start code list get rid of it
			//

			if( Configuration.GenerateStartCodeList && (CodedFrameBufferPool != NULL) )
			    CodedFrameBufferPool->DetachMetaData( Player->MetaDataStartCodeListType );

			//
			// Copy in new configuration, and check if we need to attach meta data
			//

			memcpy( &Configuration, &CollatorParameterBlock->Configuration, sizeof(CollatorConfiguration_t) );

			if( Configuration.GenerateStartCodeList && (CodedFrameBufferPool != NULL) )
			{

			    Status      = CodedFrameBufferPool->AttachMetaData( Player->MetaDataStartCodeListType, SizeofStartCodeList(Configuration.MaxStartCodes) );
			    if( Status != BufferNoError )
			    {
				report( severity_error, "Collator_Base_c::SetModuleParameters - Failed to attach start code list to all coded frame buffers.\n" );
				return Status;
			    }

			    // In order to cleanly get pointers, we release and get a new buffer
			    CodedFrameBuffer->DecrementReferenceCount( IdentifierCollator );
			    Status      = GetNewBuffer();
			    return Status;
			}
			break;

	default:
			report( severity_error, "Collator_Base_c::SetModuleParameters - Unrecognised parameter block (%d).\n", CollatorParameterBlock->ParameterType );
			return CollatorError;
    }

    return  CollatorNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
//      The register output ring function function
//

CollatorStatus_t   Collator_Base_c::RegisterOutputBufferRing(Ring_t                       Ring )
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
	report( severity_error, "Collator_Base_c::RegisterOutputBufferRing - Failed to attach coded frame descriptor to all coded frame buffers.\n" );
	CodedFrameBufferPool    = NULL;
	return Status;
    }

    //
    // Attach optional start code list
    //

    if( Configuration.GenerateStartCodeList )
    {
	Status  = CodedFrameBufferPool->AttachMetaData( Player->MetaDataStartCodeListType, SizeofStartCodeList(Configuration.MaxStartCodes) );
	if( Status != BufferNoError )
	{
	    report( severity_error, "Collator_Base_c::RegisterOutputBufferRing - Failed to attach start code list to all coded frame buffers.\n" );
	    CodedFrameBufferPool        = NULL;
	    return Status;
	}
    }

    //
    // Aquire an operating buffer
    //

    Status      = GetNewBuffer();
    if( Status != CollatorNoError )
    {
	CodedFrameBufferPool    = NULL;
	return Status;
    }

    //
    // Go live
    //

    SetComponentState( ComponentRunning );

    return CollatorNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
//      The Frame Flush function
//

CollatorStatus_t   Collator_Base_c::FrameFlush(         void )
{
CollatorStatus_t	Status = CollatorNoError;

    OS_LockMutex( &Lock );

    if( InputEntryDepth != 0 )
	InputExitPerformFrameFlush	= true;
    else
	Status	= InternalFrameFlush();

    OS_UnLockMutex( &Lock );

    return Status;
}


// /////////////////////////////////////////////////////////////////////////
//
//      The Frame Flush implementation
//

CollatorStatus_t   Collator_Base_c::InternalFrameFlush(         void )
{
CollatorStatus_t        Status;
unsigned char           TerminalStartCode[4];
Buffer_t		BufferToOutput;

//

    if( (AccumulatedDataSize == 0) &&
	!CodedFrameParameters->StreamDiscontinuity && 
	!CodedFrameParameters->ContinuousReverseJump )
	return CollatorNoError;

//

    if( (AccumulatedDataSize != 0) && Configuration.InsertFrameTerminateCode )
    {
	TerminalStartCode[0]    = 0x00;
	TerminalStartCode[1]    = 0x00;
	TerminalStartCode[2]    = 0x01;
	TerminalStartCode[3]    = Configuration.TerminalCode;

	Status  = AccumulateData( 4, TerminalStartCode );
	if( Status != CollatorNoError )
	{
	    report( severity_error, "Collator_Base_c::InternalFrameFlush - Failed to add terminal start code.\n" );
	    DiscardAccumulatedData();
	}
    }

//

    if( CodedFrameBuffer == NULL )
	return CollatorNoError;

//

    CodedFrameBuffer->SetUsedDataSize( AccumulatedDataSize );

    Status      = CodedFrameBuffer->ShrinkBuffer( max(AccumulatedDataSize, 1) );
    if( Status != BufferNoError )
    {
	report( severity_error, "Collator_Base_c::InternalFrameFlush - Failed to shrink the operating buffer to size (%08x).\n", Status );
    }

    //
    // In order to ensure that we always have a buffer, 
    // we get a new buffer before outputting the current one.
    // Also in order to ensure we do not punt out one buffer twice
    // we need to lock the three lines of code between the copy of 
    // CodedFrameBuffer, and its actual output.
    //

    CheckForGlitchPromotion();
    DelayForInjectionThrottling();

    BufferToOutput	= CodedFrameBuffer;
    Status      	= GetNewBuffer();
    OutputRing->Insert( (unsigned int )BufferToOutput );

//

    return Status;
}


// /////////////////////////////////////////////////////////////////////////
//
//      The discard all accumulated data function
//

CollatorStatus_t   Collator_Base_c::DiscardAccumulatedData(     void )
{
    if( CodedFrameBuffer != NULL )
    {
	AccumulatedDataSize                         = 0;
	CodedFrameBuffer->SetUsedDataSize( AccumulatedDataSize );

	if( Configuration.GenerateStartCodeList )
	    StartCodeList->NumberOfStartCodes       = 0;

	memset( CodedFrameParameters, 0x00, sizeof(CodedFrameParameters_t) );
    }
    return CollatorNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
//      This function is called when we enter input 
//	for a specific collator

CollatorStatus_t   Collator_Base_c::InputEntry(
			PlayerInputDescriptor_t	 *Input,
			unsigned int		  DataLength,
			void			 *Data,
			bool			  NonBlocking )
{
    OS_LockMutex( &Lock );

    InputEntryDepth++;

    OS_UnLockMutex( &Lock );

    return CollatorNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
//      Protected - This function is called when we exit input 
//	for a specific collator

CollatorStatus_t   Collator_Base_c::InputExit(		void )
{
CollatorStatus_t	Status;

//

    OS_LockMutex( &Lock );

    Status		= CollatorNoError;
    if( (InputEntryDepth == 1) && InputExitPerformFrameFlush )
    {
	Status				= InternalFrameFlush();
	InputExitPerformFrameFlush	= false;
    }

    InputEntryDepth--;

    OS_UnLockMutex( &Lock );

//

    return Status;
}


// /////////////////////////////////////////////////////////////////////////
//
//      Handle a jump
//

CollatorStatus_t   Collator_Base_c::InputJump(
					bool                      SurplusDataInjected,
					bool                      ContinuousReverseJump )
{
    AssertComponentState( "Collator_Base_c::InputJump", ComponentRunning );

//

    if( SurplusDataInjected )
	DiscardAccumulatedData();
    else
	FrameFlush();

    CodedFrameParameters->StreamDiscontinuity           = true;
    CodedFrameParameters->FlushBeforeDiscontinuity      = SurplusDataInjected;
    CodedFrameParameters->ContinuousReverseJump         = ContinuousReverseJump;
    FrameFlush();

    return CollatorNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
//      Handle a glitch
//

CollatorStatus_t   Collator_Base_c::InputGlitch( void )
{
    AssertComponentState( "Collator_Base_c::InputGlitch", ComponentRunning );

//

    DiscardAccumulatedData();
    Glitch		= true;

    return CollatorNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
//      Protected - Get a new buffer
//

CollatorStatus_t   Collator_Base_c::GetNewBuffer(       void )
{
CollatorStatus_t        Status;

//

    Status      = CodedFrameBufferPool->GetBuffer( &CodedFrameBuffer, IdentifierCollator, MaximumCodedFrameSize );
    if( Status != BufferNoError )
    {
	CodedFrameBuffer = NULL;
	report( severity_error, "Collator_Base_c::GetNewBuffer - Failed to obtain an operating buffer.\n" );
	return Status;
    }

//

    AccumulatedDataSize = 0;
    CodedFrameBuffer->ObtainDataReference( NULL, NULL, (void **)(&BufferBase) );

//

    Status      = CodedFrameBuffer->ObtainMetaDataReference( Player->MetaDataCodedFrameParametersType, (void **)(&CodedFrameParameters) );
    if( Status != BufferNoError )
    {
	report( severity_error, "Collator_Base_c::GetNewBuffer - Unable to obtain the meta data coded frame parameters.\n" );
	return Status;
    }

    memset( CodedFrameParameters, 0x00, sizeof(CodedFrameParameters_t) );

//

    if( Configuration.GenerateStartCodeList )
    {
	Status  = CodedFrameBuffer->ObtainMetaDataReference( Player->MetaDataStartCodeListType, (void **)(&StartCodeList) );
	if( Status != BufferNoError )
	{
	    report( severity_error, "Collator_Base_c::GetNewBuffer - Unable to obtain the meta data start code list.\n" );
	    return Status;
	}

	StartCodeList->NumberOfStartCodes       = 0;
    }

    return CollatorNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
//      Protected - The accumulate data into the buffer function
//

CollatorStatus_t   Collator_Base_c::AccumulateData(
						unsigned int              Length,
						unsigned char            *Data )
{
    if( (AccumulatedDataSize + Length) > MaximumCodedFrameSize )
    {
      report( severity_error, "Collator_Base_c::AccumulateData - Buffer overflow.\n");
      return CollatorBufferOverflow;
    }

    memcpy( BufferBase+AccumulatedDataSize, Data, Length );
    AccumulatedDataSize += Length;
    return CollatorNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
//      Protected - The accumulate data into the buffer function
//

CollatorStatus_t   Collator_Base_c::AccumulateStartCode(        PackedStartCode_t      Code )
{
    if( !Configuration.GenerateStartCodeList )
	return CollatorNoError;

//

    if( (StartCodeList->NumberOfStartCodes + 1) > Configuration.MaxStartCodes )
    {
	report( severity_error, "Collator_Base_c::AccumulateStartCode - Start code list overflow.\n" );
	return CollatorBufferOverflow;
    }

    StartCodeList->StartCodes[StartCodeList->NumberOfStartCodes++]      = Code;

    return CollatorNoError;
}

////////////////////////////////////////////////////////////////////////////
///
///     Copy any important information held in the input descriptor to the
///     coded frame parameters.
///
///     If the input descriptor provides any hints about playback or
///     presentation time then initialize Collator_Base_c::CodedFrameParameters
///     with these values.
///
///     \param Input The input descriptor
///
void Collator_Base_c::ActOnInputDescriptor(    PlayerInputDescriptor_t   *Input)
{
    if( Input->PlaybackTimeValid && !CodedFrameParameters->PlaybackTimeValid )
    {
	CodedFrameParameters->PlaybackTimeValid = true;
	CodedFrameParameters->PlaybackTime      = Input->PlaybackTime;
    }

    if( Input->DecodeTimeValid && !CodedFrameParameters->DecodeTimeValid )
    {
	CodedFrameParameters->DecodeTimeValid   = true;
	CodedFrameParameters->DecodeTime        = Input->DecodeTime;
    }

    CodedFrameParameters->DataSpecificFlags     |= Input->DataSpecificFlags;
}


// /////////////////////////////////////////////////////////////////////////
//
//      Private - Check if we should promote a glitch to a full blown 
// 		  discontinuity. This works by checking pts flow around
//		  the glitch.
//

void   Collator_Base_c::CheckForGlitchPromotion( void )
{
long long 		 DeltaPTS;
long long		 Range;

    if( CodedFrameParameters->PlaybackTimeValid )
    {
	//
	// Handle any glitch promotion
	//     We promote if there is a glitch, and if the 
	//     PTS varies by more than the maximum of 1/4 second, and 40ms times the number of frames that have passed since a pts.
	//

	if( Glitch && 
	    (LastFramePreGlitchPTS != INVALID_TIME) )
	{
	    DeltaPTS		= CodedFrameParameters->PlaybackTime - LastFramePreGlitchPTS;
	    Range		= max( 22500, (3600 * FrameSinceLastPTS) );
	    if( !inrange(DeltaPTS, -Range, Range) )
	    {
		report( severity_info, "Collator_Base_c::CheckForGlitchPromotion (%d) Promoted\n", Configuration.GenerateStartCodeList );
		CodedFrameParameters->StreamDiscontinuity           = true;
		CodedFrameParameters->FlushBeforeDiscontinuity      = false;
		CodedFrameParameters->ContinuousReverseJump         = false;
	    }
	}

	//
	// Remember the frame pts
	//

	Glitch			= false;
	LastFramePreGlitchPTS	= CodedFrameParameters->PlaybackTime;
	FrameSinceLastPTS	= 0;
    }

    FrameSinceLastPTS++;
}


// /////////////////////////////////////////////////////////////////////////
//
//      Private - If the input is to be throttled, the the delay 
//		  will occur here.
//

void   Collator_Base_c::DelayForInjectionThrottling( void )
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
    if( (Policy != PolicyValueApply) || !CodedFrameParameters->PlaybackTimeValid )
	return;

    Player->GetPlaybackSpeed( Playback, &Speed, &Direction );
    if( Direction == PlayBackward )
        return;

    //
    // Obtain the relevent data
    //

    Status	= OutputTimer->GetStreamStartDelay( &StreamDelay );
    if( Status == PlayerNoError )
	Status	= Player->TranslateNativePlaybackTime( Playback, CodedFrameParameters->PlaybackTime, &SystemPlaybackTime );
    if( Status != PlayerNoError )
	return;

    //
    // Watch out for jumping PTS values, they can confuse us
    //

    if( LimitHandlingLastPTS == INVALID_TIME )
	LimitHandlingLastPTS	= CodedFrameParameters->PlaybackTime;

//

    DeltaPTS			= CodedFrameParameters->PlaybackTime - LimitHandlingLastPTS;
    if( !inrange(DeltaPTS, -90000, 90000) )
    {
	LimitHandlingJumpInEffect	= true;
	LimitHandlingJumpAt		= CodedFrameParameters->PlaybackTime;
    }

    LimitHandlingLastPTS	= CodedFrameParameters->PlaybackTime;

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
