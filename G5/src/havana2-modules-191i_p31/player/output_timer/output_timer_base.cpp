/************************************************************************
Copyright (C) 2007 STMicroelectronics. All Rights Reserved.

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

Source file name : output_timer_base.cpp
Author :           Nick

Implementation of the base output timer class for player 2.


Date        Modification                                    Name
----        ------------                                    --------
12-Mar-06   Created                                         Nick

************************************************************************/

// /////////////////////////////////////////////////////////////////////
//
//	Include any component headers

#include "output_timer_base.h"

// /////////////////////////////////////////////////////////////////////////
//
// Locally defined constants
//

#define DECODE_TIME_JUMP_THRESHOLD				16000000	// 16 second default (as coordinator PLAYBACK_TIME_JUMP_THRESHOLD)

#define TRICK_MODE_CHECK_REFERENCE_FRAMES_FOR_N_KEY_FRAMES	2		// Check reference frames for decode 
										// after trick mode domain change
										// until we have seen N key frames

#define DECODE_IN_TIME_INTEGRATION_PERIOD			16		// Integrate decode in time checks over 16 frames
#define DECODE_IN_TIME_PAUSE_BETWEEN_INTEGRATION_PERIOD		64

#define REBASE_MARGIN						50000		// A 50ms margin when we attempt to recover 
										// from decode in time failures by rebasing

// /////////////////////////////////////////////////////////////////////////
//
// Locally defined structures
//

#define SameSign( a, b )	((a)*(b) >= 0)

// /////////////////////////////////////////////////////////////////////////
//
// 	The Constructor function
//
OutputTimer_Base_c::OutputTimer_Base_c( void )
{
    InitializationStatus	= OutputTimerError;

//

    OS_InitializeMutex( &Lock );

    OutputCoordinator		= NULL;
    OutputCoordinatorContext	= NULL;
    BufferManager		= NULL;

//

    InitializationStatus	= OutputTimerNoError;
}

// /////////////////////////////////////////////////////////////////////////
//
// 	The Destructor function
//

OutputTimer_Base_c::~OutputTimer_Base_c( 	void )
{
    Halt();
    Reset();

    OS_TerminateMutex( &Lock );
}


// /////////////////////////////////////////////////////////////////////////
//
// 	The Halt function, give up access to any registered resources
//
//	NOTE for some calls we ignore the return statuses, this is because
//	we will procede with the halt even if we fail (what else can we do)
//

OutputTimerStatus_t   OutputTimer_Base_c::Halt( 	void )
{
    if( TestComponentState(ComponentRunning) )
    {
	//
	// Move the base state to halted early, to ensure 
	// no activity can be queued once we start halting
	//

	BaseComponentClass_c::Halt();

	//
	// If we are registered with the output coordinator, de-register
	//

	if( OutputCoordinator != NULL )
	{
	    OutputCoordinator->DeRegisterStream( OutputCoordinatorContext );
	    OutputCoordinatorContext	= NULL;
	    OutputCoordinator		= NULL;
	}
    }

//

    return OutputTimerNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
// 	The Reset function release any resources, and reset all variables
//

OutputTimerStatus_t   OutputTimer_Base_c::Reset(	void )
{

    TimeMappingValidForDecodeTiming		= false;
    TimeMappingInvalidatedAtDecodeIndex		= INVALID_INDEX;
    LastSeenDecodeTime				= INVALID_TIME;
    LastKeyFramePlaybackTime			= INVALID_TIME;
    NextExpectedPlaybackTime			= INVALID_TIME;

    NormalizedTimeOffset			= 0;
    DecodeTimeJumpThreshold			= DECODE_TIME_JUMP_THRESHOLD;
    LastFrameDropPreDecodeDecision		= OutputTimerNoError;
    KeyFramesSinceDiscontinuity			= 0;

    LastExpectedFrameTime			= INVALID_TIME;
    LastActualFrameTime				= INVALID_TIME;
    OutputRateAdjustment			= 1;
    SystemClockAdjustment			= 1;

    CodedFrameRate				= 0;
    LastCodedFrameRate				= 0;
    AdjustedSpeedAfterFrameDrop			= 1;
    PortionOfPreDecodeFramesToLose		= 0;
    AccumulatedPreDecodeFramesToLose		= 0;

    TrickModeCheckReferenceFrames		= 0;
    LastTrickModePolicy				= PolicyValueTrickModeAuto;
    LastTrickModeDomain				= TrickModeInvalid;
    TrickModeDomain				= TrickModeInvalid;
    LastPortionOfPreDecodeFramesToLose		= 2;
    LastTrickModeSpeed				= 0;
    LastTrickModeGroupStructureIndependentFrames= 0;
    LastTrickModeGroupStructureReferenceFrames	= 0;

    SynchronizationState			= SyncStateInSynchronization;
    SynchronizationFrameCount			= 0;
    SynchronizationAtStartup			= true;
    DecodeInTimeState				= DecodingInTime;
    SeenAnInTimeFrame				= false;

    SynchronizationAccumulatedError		= 0;

    return BaseComponentClass_c::Reset();
}


// /////////////////////////////////////////////////////////////////////////
//
//	Override for component base class set module parameters function
//

OutputTimerStatus_t   OutputTimer_Base_c::SetModuleParameters(
							unsigned int	  ParameterBlockSize,
							void		 *ParameterBlock )
{
OutputTimerParameterBlock_t	 *OutputTimerParameterBlock = (OutputTimerParameterBlock_t *)ParameterBlock;
unsigned char			  ExternalMappingPolicy;

//

    if( ParameterBlockSize != sizeof(OutputTimerParameterBlock_t) )
    {
	report( severity_error, "OutputTimer_Base_c::SetModuleParameters - Invalid parameter block.\n" );
	return OutputTimerError;
    }

//

    switch( OutputTimerParameterBlock->ParameterType )
    {
	case OutputTimerSetNormalizedTimeOffset:

			//
			// Lazily update the value, and force a rebase of the time mapping if required
			//

			if( NormalizedTimeOffset != OutputTimerParameterBlock->Offset.Value )
			{
			    NormalizedTimeOffset	= OutputTimerParameterBlock->Offset.Value;

			    ExternalMappingPolicy	= Player->PolicyValue( Playback, Stream, PolicyExternalTimeMapping );
			    if( ExternalMappingPolicy != PolicyValueApply )
			        ResetTimeMapping();
			}

	                break;

	default:
	                report( severity_error, "OutputTimer_Base_c::SetModuleParameters - Unrecognised parameter block (%d).\n", OutputTimerParameterBlock->ParameterType );
	                return OutputTimerError;
    }

    return  OutputTimerNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
// 	the register a stream function, creates a new timer context
//

OutputTimerStatus_t   OutputTimer_Base_c::RegisterOutputCoordinator(
						OutputCoordinator_t	  OutputCoordinator )
{
PlayerStatus_t		Status;

    //
    // Obtain the class list for this stream then fill in the 
    // configuration record
    //

    Player->GetBufferManager( &BufferManager );
    if( Manifestor == NULL )
    {
	report( severity_error, "OutputTimer_Base_c::RegisterOutputCoordinator(%s) - This implementation does not support no-output timing.\n", Configuration.OutputTimerName );
	return PlayerNotSupported;
    }

    InitializeConfiguration();

    //
    // Read the initial value of the sync policy
    //

    LastSynchronizationPolicy	= Player->PolicyValue( Playback, Stream, PolicyAVDSynchronization );

    //
    // Attach the stream specific (audio|video|data)
    // output timing information to the decode buffer pool.
    //

    Player->GetDecodeBufferPool( Stream, &DecodeBufferPool );
    if( DecodeBufferPool == NULL )
    {
	report( severity_error, "OutputTimer_Base_c::RegisterOutputCoordinator(%s) - Failed to obtain the decode buffer pool.\n", Configuration.OutputTimerName );
	return OutputTimerError;
    }

//

    Status	= DecodeBufferPool->AttachMetaData( Configuration.AudioVideoDataOutputTimingType );
    if( Status != BufferNoError )
    {
	report( severity_error, "OutputTimer_Base_c::RegisterOutputCoordinator(%s) - Failed to attach stream specific timing data to all members of the decode buffer pool.\n", Configuration.OutputTimerName );
	return Status;
    }

    //
    // Get the coded frame buffer pool, so that we can monitor levels if
    // frames fail to decode in time.
    //

    Player->GetCodedFrameBufferPool( Stream, &CodedFrameBufferPool, &CodedFrameBufferMaximumSize );
    if( CodedFrameBufferPool == NULL )
    {
	report( severity_error, "OutputTimer_Base_c::RegisterOutputCoordinator(%s) - Failed to obtain the coded frame buffer pool.\n", Configuration.OutputTimerName );
	return OutputTimerError;
    }

    //
    // Get the output surface descriptor from the manifestor
    //

    Status	= Manifestor->GetSurfaceParameters( Configuration.AudioVideoDataOutputSurfaceDescriptorPointer );
    if( Status != ManifestorNoError )
    {
	report( severity_error, "OutputTimer_Base_c::RegisterOutputCoordinator(%s) - Failed to obtain the output surface descriptor from the manifestor.\n", Configuration.OutputTimerName );
	return Status;
    }

    //
    // Record the output coordinator 
    // and register us with it.
    //

    Status	= OutputCoordinator->RegisterStream( Stream, Configuration.StreamType, &OutputCoordinatorContext );
    if( Status != PlayerNoError )
    {
	report( severity_error, "OutputTimer_Base_c::RegisterOutputCoordinator(%s) - Failed to register with output coordinator.\n", Configuration.OutputTimerName );
	return Status;
    }

    this->OutputCoordinator	= OutputCoordinator;

    //
    // Initialize state
    //

    LastFrameDropPreDecodeDecision	= OutputTimerNoError;
    KeyFramesSinceDiscontinuity		= 0;

    //
    // Initialize the trick mode domain boundaries before going live
    //

    Codec->GetTrickModeParameters( &TrickModeParameters );
    InitializeGroupStructure();
    SetTrickModeDomainBoundaries();

    //
    // Go live, and return
    //

    SetComponentState( ComponentRunning );
    
    return OutputTimerNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
// 	the register a stream function, creates a new timer context
//

OutputTimerStatus_t   OutputTimer_Base_c::ResetTimeMapping(		void )
{
    //
    // Check that we are running
    //

    AssertComponentState( "OutputTimer_Base_c::ResetTimeMapping", ComponentRunning );

    //
    // Reset the mapping
    //

    TimeMappingValidForDecodeTiming	= false;
    TimeMappingInvalidatedAtDecodeIndex	= INVALID_INDEX;
    LastSeenDecodeTime			= INVALID_TIME;
    NextExpectedPlaybackTime		= INVALID_TIME;

    LastExpectedFrameTime		= INVALID_TIME;
    LastActualFrameTime			= INVALID_TIME;

    return OutputCoordinator->ResetTimeMapping( OutputCoordinatorContext );
}


// /////////////////////////////////////////////////////////////////////////
//
// 	The function to await entry into the decode window, if the decode 
//	time is invalid, or if no time mapping has yet been established, 
//	we return immediately.
//

OutputTimerStatus_t   OutputTimer_Base_c::AwaitEntryIntoDecodeWindow(   Buffer_t		  Buffer )
{
OutputCoordinatorStatus_t	  Status;
bool				  TrickModeDiscarding;
ParsedFrameParameters_t		 *ParsedFrameParameters;
long long 			  DeltaDecodeTime;
long long 			  LowerLimit;
unsigned long long		  NormalizedDecodeTime;
unsigned long long		  DecodeWindowPorch;
unsigned long long		  MaximumSleepTime;
Rational_t			  Speed;
PlayDirection_t			  Direction;
unsigned char			  SynchronizationPolicy;

    //
    // Check that we are running
    //

    AssertComponentState( "OutputTimer_Base_c::AwaitEntryIntoDecodeWindow", ComponentRunning );

    //
    // For reverse play and pause we do not implement decode windows.
    //

    Player->GetPlaybackSpeed( Playback, &Speed, &Direction );

    if( (Direction == PlayBackward) || (Speed == 0) )
	return PlayerNoError;

    TrickModeDiscarding		= !inrange(Speed, Configuration.MinimumSpeedSupported, Configuration.MaximumSpeedSupported);

    //
    // Is time mapping usable (Note for discarding a master may be allowed)
    //

    if( !TimeMappingValidForDecodeTiming && !TrickModeDiscarding )
	return PlayerNoError;

    //
    // Do we have an active synchronization policy
    //

    SynchronizationPolicy	= Player->PolicyValue( Playback, Stream, PolicyAVDSynchronization );
    if( SynchronizationPolicy == PolicyValueDisapply )
	return PlayerNoError;

    //
    // Obtain the meta data
    //

    Status	= ExtractPointersPreDecode( Buffer );
    if( Status != PlayerNoError )
	return Status;

    ParsedFrameParameters	= PreDecodeParsedFrameParameters;

    //
    // Would it be helpful to make up a DTS
    //

    if( !ValidTime(ParsedFrameParameters->NormalizedDecodeTime) &&
	!ParsedFrameParameters->ReferenceFrame &&
	 ValidTime(ParsedFrameParameters->NormalizedPlaybackTime) )
    {
	ParsedFrameParameters->NormalizedDecodeTime	= ParsedFrameParameters->NormalizedPlaybackTime - Configuration.FrameDecodeTime;
    }

    //
    // Check for large DTS changes, invalidating the mapping if we find one
    // Note we allow negative jumps here of upto the decode porch time, simply
    // because when we make up a DTS we often create a negative jump of 1 or 
    // two frame times.
    //

    if( ValidTime(LastSeenDecodeTime) && ValidTime(ParsedFrameParameters->NormalizedDecodeTime) )
    {
	DeltaDecodeTime		= (Direction == PlayBackward) ? 
					(LastSeenDecodeTime - ParsedFrameParameters->NormalizedDecodeTime) :
					(ParsedFrameParameters->NormalizedDecodeTime - LastSeenDecodeTime);

	LowerLimit		= min( -100, -(long long)Configuration.EarlyDecodePorch );
	if( !inrange(DeltaDecodeTime, LowerLimit, DecodeTimeJumpThreshold) )
	{
	    TimeMappingValidForDecodeTiming	= false;
	    TimeMappingInvalidatedAtDecodeIndex	= ParsedFrameParameters->DecodeFrameIndex;

	    report( severity_info, "OutputTimer_Base_c::AwaitEntryIntoDecodeWindow(%s) - Jump in decode times detected %12lld\n", Configuration.OutputTimerName, DeltaDecodeTime );
	}
    }

    LastSeenDecodeTime		= ParsedFrameParameters->NormalizedDecodeTime;

    //
    // Check if we have a valid time mapping, return immediately 
    // if we have no time mapping or the decode time is invalid.
    //

    if( (!TimeMappingValidForDecodeTiming && !TrickModeDiscarding) || !ValidTime(ParsedFrameParameters->NormalizedDecodeTime) )
	return OutputTimerNoError;

    //
    // Perform the wait
    //

    NormalizedDecodeTime	= ParsedFrameParameters->NormalizedDecodeTime + (unsigned long long)NormalizedTimeOffset;
    DecodeWindowPorch		= Configuration.FrameDecodeTime + Configuration.EarlyDecodePorch;
    MaximumSleepTime		= (unsigned long long)Configuration.MaximumDecodeTimesToWait * Configuration.FrameDecodeTime;

    Status			= OutputCoordinator->PerformEntryIntoDecodeWindowWait( OutputCoordinatorContext, NormalizedDecodeTime, DecodeWindowPorch, MaximumSleepTime );
    return Status;
}


// /////////////////////////////////////////////////////////////////////////
//
// 	The function to test for frame drop before or after decode
//

OutputTimerStatus_t   OutputTimer_Base_c::TestForFrameDrop(	Buffer_t		  Buffer,
								OutputTimerTestPoint_t	  TestPoint )
{
OutputTimerStatus_t	  Status;
unsigned char		  SynchronizationPolicy;

    //
    // Check that we are running
    //

    AssertComponentState( "OutputTimer_Base_c::TestForFrameDrop", ComponentRunning );

    //
    // Are we applying synchronization
    //

    SynchronizationPolicy	= Player->PolicyValue( Playback, Stream, PolicyAVDSynchronization );
    if( SynchronizationPolicy == PolicyValueDisapply )
	return PlayerNoError;

    //
    // Split out to the different test points
    //

    switch( TestPoint )
    {
	case OutputTimerBeforeDecodeWindow:
		Status	= DropTestBeforeDecodeWindow( Buffer );
		break;

	case OutputTimerBeforeDecode:
		Status	= DropTestBeforeDecode( Buffer );
		break;

	case OutputTimerBeforeOutputTiming:
		Status	= DropTestBeforeOutputTiming( Buffer );
		break;

	case OutputTimerBeforeManifestation:
		Status	= DropTestBeforeManifestation( Buffer );
		break;

	default:
		Status	= OutputTimerNoError;
    }

//

    return Status;
}


// /////////////////////////////////////////////////////////////////////////
//
// 	The function to test for frame drop before decode
//

OutputTimerStatus_t   OutputTimer_Base_c::DropTestBeforeDecodeWindow(	Buffer_t	  Buffer )
{
OutputTimerStatus_t	  Status;
ParsedFrameParameters_t	 *ParsedFrameParameters;
void			 *ParsedAudioVideoDataParameters;
unsigned char		  SingleGroupPlayback;
unsigned char		  IndependentFramesOnly;
unsigned long long	  StartInterval;
unsigned long long	  EndInterval;
unsigned long long	  Duration;
unsigned long long	  DurationEnd;

    //
    // Obtain the meta data
    //

    Status	= ExtractPointersPreDecode( Buffer );
    if( Status != PlayerNoError )
	return Status;

    ParsedFrameParameters		= PreDecodeParsedFrameParameters;
    ParsedAudioVideoDataParameters	= PreDecodeParsedAudioVideoDataParameters;

    //
    // If this is not the first decode of a frame, 
    // then we repeat our previous decision 
    // (IE we cannot drop just slice, or a single field).
    //

    if( !ParsedFrameParameters->FirstParsedParametersForOutputFrame )
	return LastFrameDropPreDecodeDecision;

    //
    // Update the trick mode parameters, and the group structure information
    //

    Codec->GetTrickModeParameters( &TrickModeParameters );
    MonitorGroupStructure( ParsedFrameParameters );

    //
    // Update the key frame count
    //

    if( ParsedFrameParameters->FirstParsedParametersAfterInputJump )
	KeyFramesSinceDiscontinuity	= 0;

    if( ParsedFrameParameters->KeyFrame )
    {
	LastKeyFramePlaybackTime	= ParsedFrameParameters->NormalizedPlaybackTime;
	KeyFramesSinceDiscontinuity++;
    }

    //
    // Handle the playing of single groups policy
    //

    SingleGroupPlayback	= Player->PolicyValue( Playback, Stream, PolicyStreamSingleGroupBetweenDiscontinuities );

    if( (SingleGroupPlayback == PolicyValueApply) &&
	(KeyFramesSinceDiscontinuity >= 2) )
    {
	LastFrameDropPreDecodeDecision	= OutputTimerDropFrameSingleGroupPlayback;
	return LastFrameDropPreDecodeDecision;
    }

    //
    // Handle the key frames only policy - Nick switched the meaning of this
    // to be play independent frames only IE I frames (only in h264 is there a 
    // distinction between I and key frames (IDRs in 264).
    //

    IndependentFramesOnly	= Player->PolicyValue( Playback, Stream, PolicyStreamOnlyKeyFrames );

    if( !ParsedFrameParameters->IndependentFrame &&
	(IndependentFramesOnly == PolicyValueApply) )
    {
	LastFrameDropPreDecodeDecision	= OutputTimerDropFrameKeyFramesOnly;
	return LastFrameDropPreDecodeDecision;
    }

    //
    // Check for presentation interval.
    // We can junk anything if the last key frame was later 
    // than the end of the presentation interval.
    //
    // That last was a mistake, due to the nature of open groups
    // it should say we can junk anything apart from the keyframe itself.
    //
    // Alternatively for a non-reference frame we can dump the 
    // frame if we know it does not intersect the interval.
    //

    Player->GetPresentationInterval( Stream, &StartInterval, &EndInterval );

    if( !ParsedFrameParameters->KeyFrame && PlaybackTimeCheckGreaterThanOrEqual( LastKeyFramePlaybackTime, EndInterval ) )
    {
	LastFrameDropPreDecodeDecision	= OutputTimerDropFrameOutsidePresentationInterval;
	return LastFrameDropPreDecodeDecision;
    }

    if( !ParsedFrameParameters->ReferenceFrame )
    {
    	Status	= FrameDuration( ParsedAudioVideoDataParameters, &Duration );
    	if( Status == OutputTimerNoError )
    	{
	    DurationEnd	= (ParsedFrameParameters->NormalizedPlaybackTime == INVALID_TIME) ? INVALID_TIME : (ParsedFrameParameters->NormalizedPlaybackTime + Duration);
	    if( !PlaybackTimeCheckIntervalsIntersect(ParsedFrameParameters->NormalizedPlaybackTime, DurationEnd, StartInterval, EndInterval) )
	    {
		LastFrameDropPreDecodeDecision	= OutputTimerDropFrameOutsidePresentationInterval;
		return LastFrameDropPreDecodeDecision;
	    }
	}
    }

//
    return OutputTimerNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
// 	The function to test for frame drop before decode
//

OutputTimerStatus_t   OutputTimer_Base_c::DropTestBeforeDecode(	Buffer_t		  Buffer )
{
OutputTimerStatus_t	  Status;
Rational_t		  Speed;
PlayDirection_t		  Direction;
ParsedFrameParameters_t	 *ParsedFrameParameters;
void			 *ParsedAudioVideoDataParameters;

    //
    // Read speed policy etc
    //

    Player->GetPlaybackSpeed( Playback, &Speed, &Direction );

    if( ((Direction == PlayBackward) && !Configuration.ReversePlaySupported) ||
	((Speed != 0) && !inrange(Speed, Configuration.MinimumSpeedSupported, Configuration.MaximumSpeedSupported)) )
    {
	NextExpectedPlaybackTime	= INVALID_TIME;		// If we are discarding based on trick mode
								// we may well do this for a long time.

	LastFrameDropPreDecodeDecision	= OutputTimerTrickModeNotSupportedDropFrame;
	return LastFrameDropPreDecodeDecision;
    }

    //
    // Obtain the meta data
    //

    Status	= ExtractPointersPreDecode( Buffer );
    if( Status != PlayerNoError )
	return Status;

    ParsedFrameParameters		= PreDecodeParsedFrameParameters;
    ParsedAudioVideoDataParameters	= PreDecodeParsedAudioVideoDataParameters;

    //
    // If this is not the first decode of a frame, 
    // then we repeat our previous decision 
    // (IE we cannot drop just slice, or a single field).
    //

    if( !ParsedFrameParameters->FirstParsedParametersForOutputFrame )
	return LastFrameDropPreDecodeDecision;

    //
    // Allow trick mode control to test for a change in behaviour
    //

    Status	= TrickModeControl();
    if( Status != OutputTimerNoError )
	return Status;

    if( ParsedFrameParameters->KeyFrame && (TrickModeCheckReferenceFrames != 0))
	TrickModeCheckReferenceFrames--;

    //
    // Trick mode decisions, trick mode discarding is done once we enter the decode window
    //

    Status	= OutputTimerNoError;
    switch( TrickModeDomain )
    {
 	case TrickModeInvalid:
	case TrickModeDecodeAll:
		break;

	case PolicyValueTrickModeDecodeAllDegradeNonReferenceFrames:
		ParsedFrameParameters->ApplySubstandardDecode		 = !ParsedFrameParameters->ReferenceFrame;
		break;

	case TrickModeStartDiscardingNonReferenceFrames:
		if( !ParsedFrameParameters->ReferenceFrame )
		{
		    ParsedFrameParameters->ApplySubstandardDecode	 = true;

		    AccumulatedPreDecodeFramesToLose			+= PortionOfPreDecodeFramesToLose;
		    if( AccumulatedPreDecodeFramesToLose >= 1 )
		    {
			AccumulatedPreDecodeFramesToLose		-= 1;
			Status						 = OutputTimerTrickModeDropFrame;
		    }
		}
		break;

	case TrickModeDecodeReferenceFramesDegradeNonKeyFrames:
		ParsedFrameParameters->ApplySubstandardDecode		 = !ParsedFrameParameters->IndependentFrame;
		if( !ParsedFrameParameters->ReferenceFrame )
		    Status						 = OutputTimerTrickModeDropFrame;
		break;

	case TrickModeDecodeKeyFrames:
	case TrickModeDiscontinuousKeyFrames:
		if( !ParsedFrameParameters->IndependentFrame )
		    Status						 = OutputTimerTrickModeDropFrame;
		break;
    }

    //
    // Now cater for the drop until key frame recovery from very fast forward
    //

    if( (TrickModeCheckReferenceFrames != 0) &&
	(Status == OutputTimerNoError) )
    {
	Status	= Codec->CheckReferenceFrameList( ParsedFrameParameters->NumberOfReferenceFrameLists,
						  ParsedFrameParameters->ReferenceFrameList );
	if( Status != OutputTimerNoError )
	{
	    Status	= OutputTimerTrickModeDropFrame;
	    if( TrickModeDomain == TrickModeStartDiscardingNonReferenceFrames )
		AccumulatedPreDecodeFramesToLose	-= 1;
	}
    }

    //
    // We are not going to drop the frame, if we are transitioning from an unsupported trick mode 
    // where we would drop all frames, then we haven't been around for a while, and need to
    // reset our sync monitors
    //

    if( LastFrameDropPreDecodeDecision == OutputTimerTrickModeNotSupportedDropFrame )
    {
	SynchronizationState	= SyncStateStartAwaitingCorrectionWorkthrough;
	DecodeInTimeState	= DecodingInTime;
	SeenAnInTimeFrame	= false;
    }

    //
    // If we have reached this point, then we have not discarded, 
    // and we update the last decision variable.
    //

    LastFrameDropPreDecodeDecision	= Status;

    return Status;
}


// /////////////////////////////////////////////////////////////////////////
//
// 	The function to test for frame drop before decode
//

OutputTimerStatus_t   OutputTimer_Base_c::DropTestBeforeOutputTiming(	Buffer_t		  Buffer )
{
OutputTimerStatus_t	  Status;
ParsedFrameParameters_t	 *ParsedFrameParameters;
void			 *ParsedAudioVideoDataParameters;
VideoOutputTiming_t	 *OutputTiming;
unsigned long long	  StartInterval;
unsigned long long	  EndInterval;
unsigned long long	  DurationEnd;
unsigned long long	  Duration;

    //
    // Obtain the meta data
    //

    Status	= ExtractPointersPostDecode( Buffer );
    if( Status != PlayerNoError )
	return Status;

    ParsedFrameParameters		= PostDecodeParsedFrameParameters;
    ParsedAudioVideoDataParameters	= PostDecodeParsedAudioVideoDataParameters;
    OutputTiming			= (VideoOutputTiming_t *)PostDecodeAudioVideoDataOutputTiming;

    //
    // Check for presentation interval.
    // For all frames we can dump the frame if 
    // we know it does not intersect the interval.
    //

    Player->GetPresentationInterval( Stream, &StartInterval, &EndInterval );

    Status	= FrameDuration( ParsedAudioVideoDataParameters, &Duration );
    if( Status == OutputTimerNoError )
    {
	DurationEnd	= (ParsedFrameParameters->NormalizedPlaybackTime == INVALID_TIME) ? INVALID_TIME : (ParsedFrameParameters->NormalizedPlaybackTime + Duration);
	if( !PlaybackTimeCheckIntervalsIntersect(ParsedFrameParameters->NormalizedPlaybackTime, DurationEnd, StartInterval, EndInterval) )
	{
	    return OutputTimerDropFrameOutsidePresentationInterval;
	}
    }

//

    return OutputTimerNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
// 	The function to test for frame drop before decode
//

OutputTimerStatus_t   OutputTimer_Base_c::DropTestBeforeManifestation(	Buffer_t		  Buffer )
{
OutputTimerStatus_t	  Status;
ParsedFrameParameters_t	 *ParsedFrameParameters;
void			 *ParsedAudioVideoDataParameters;
VideoOutputTiming_t	 *OutputTiming;
unsigned char		  DropLateFramesPolicy;
unsigned char		  VideoImmediatePolicy;
Rational_t		  Speed;
PlayDirection_t		  Direction;
unsigned long long	  Now;

    //
    // Obtain the meta data
    //

    Status	= ExtractPointersPostDecode( Buffer );
    if( Status != PlayerNoError )
	return Status;

    ParsedFrameParameters		= PostDecodeParsedFrameParameters;
    ParsedAudioVideoDataParameters	= PostDecodeParsedAudioVideoDataParameters;
    OutputTiming			= (VideoOutputTiming_t *)PostDecodeAudioVideoDataOutputTiming;

    //
    // Drop the frame if it is untimed
    //

    Player->GetPlaybackSpeed( Playback, &Speed, &Direction );
    if( !ValidTime(ParsedFrameParameters->NormalizedPlaybackTime) ||
	((Speed != 0) && (OutputTiming->SystemPlaybackTime == UNSPECIFIED_TIME)) )
    {
	OutputTiming->TimingGenerated	= false;
	return OutputTimerUntimedFrame;
    }

    //
    // Drop the frame if it is too late for manifestation
    //

    Now	= OS_GetTimeInMicroSeconds();
    if( (Speed != 0) && ((Now + Configuration.MinimumManifestorLatency) >= OutputTiming->SystemPlaybackTime) )
    {
	DecodeInTimeFailure( (Now + Configuration.MinimumManifestorLatency) - OutputTiming->SystemPlaybackTime );

	DropLateFramesPolicy	= Player->PolicyValue( Playback, Stream, PolicyDiscardLateFrames );
	switch( DropLateFramesPolicy )
	{
	    case PolicyValueDiscardLateFramesNever:
			break;

	    case PolicyValueDiscardLateFramesAfterSynchronize:
			if( !SeenAnInTimeFrame )
			{
			    // If this is audio in a video play immediate case
			    // we need to ensure that we do not mistakenly think
			    // that we are failing to decode in time

			    VideoImmediatePolicy 	= Player->PolicyValue( Playback, Stream, PolicyVideoStartImmediate );
			    if( (Configuration.StreamType == StreamTypeAudio) &&
				(VideoImmediatePolicy == PolicyValueApply) )
			    {
			        SynchronizationState	= SyncStateStartAwaitingCorrectionWorkthrough;
				DecodeInTimeState	= DecodingInTime;
			    }
			}
			else
			    break;

	    case PolicyValueDiscardLateFramesAlways:
			OutputTiming->TimingGenerated	= false;
			return OutputTimerDropFrameTooLateForManifestation;
			break;
	}
    }
    else
    {
	DecodeInTimeState	= DecodingInTime;
	SeenAnInTimeFrame	= true;
    }

    //
    // Trick mode decisions
    //

    //
    // Re-check the before decode drop, to specifically 
    // cope with pause followed by new trick mode transition.
    //

    Player->GetPlaybackSpeed( Playback, &Speed, &Direction );

    if( ((Direction == PlayBackward) && !Configuration.ReversePlaySupported) ||
	((Speed != 0) && !inrange(Speed, Configuration.MinimumSpeedSupported, Configuration.MaximumSpeedSupported)) )
    {
	OutputTiming->TimingGenerated	= false;
	return OutputTimerTrickModeNotSupportedDropFrame;
    }

    //
    // Drop the frame if (due to avsync or trick mode) 
    // it has been reduced to zero duration.
    //

    if( OutputTiming->ExpectedDurationTime == 0 )
    {
	OutputTiming->TimingGenerated	= false;
	return OutputTimerDropFrameZeroDuration;
    }

//

    return OutputTimerNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
// 	The function to actually generate an output timing record
//

OutputTimerStatus_t   OutputTimer_Base_c::GenerateFrameTiming(	
							Buffer_t		  Buffer )
{
OutputTimerStatus_t	  Status;
ParsedFrameParameters_t	 *ParsedFrameParameters;
void			 *ParsedAudioVideoDataParameters;
void			 *AudioVideoDataOutputTiming;
VideoOutputTiming_t	 *OutputTiming;
unsigned long long	  NormalizedPlaybackTime;
unsigned long long	  NormalizedDecodeTime;
unsigned long long	  SystemTime;
unsigned char		  SynchronizationPolicy;
Rational_t		  Speed;
PlayDirection_t		  Direction;
bool			  BufferForReTiming;
bool			  KnownPlaybackJump;

//

    AssertComponentState( "OutputTimer_Base_c::GenerateFrameTiming", ComponentRunning );

    //
    // Obtain the meta data
    //

    Status	= ExtractPointersPostDecode( Buffer );
    if( Status != PlayerNoError )
	return Status;

    ParsedFrameParameters		= PostDecodeParsedFrameParameters;
    ParsedAudioVideoDataParameters	= PostDecodeParsedAudioVideoDataParameters;
    AudioVideoDataOutputTiming		= PostDecodeAudioVideoDataOutputTiming;
    OutputTiming			= (VideoOutputTiming_t *)PostDecodeAudioVideoDataOutputTiming;

    BufferForReTiming			= OutputTiming->TimingGenerated;

    memset( AudioVideoDataOutputTiming, 0x00, Configuration.SizeOfAudioVideoDataOutputTiming );

    //
    // Read synchronization policy and check for it being turned on
    //

    SynchronizationPolicy	= Player->PolicyValue( Playback, Stream, PolicyAVDSynchronization );
    if( (LastSynchronizationPolicy == PolicyValueDisapply) &&
	(SynchronizationPolicy == PolicyValueApply) )
	ResetTimeMapping();

    LastSynchronizationPolicy	= SynchronizationPolicy;

    //
    // Check for large PTS changes, delta the mapping if we find one.
    // but only if this is not a re-timing exercise
    //

    NormalizedPlaybackTime	= ParsedFrameParameters->NormalizedPlaybackTime + (unsigned long long)NormalizedTimeOffset;

    if( BufferForReTiming )
    	NextExpectedPlaybackTime	= INVALID_TIME;

    if( ValidTime(ParsedFrameParameters->NormalizedPlaybackTime) )
    {
    	if( ValidTime(NextExpectedPlaybackTime) )
    	{
	    KnownPlaybackJump	= ParsedFrameParameters->FirstParsedParametersAfterInputJump && !ParsedFrameParameters->ContinuousReverseJump;

	    Status	= OutputCoordinator->HandlePlaybackTimeDeltas( OutputCoordinatorContext, KnownPlaybackJump, NextExpectedPlaybackTime, NormalizedPlaybackTime );
	    if( Status != OutputCoordinatorNoError )
	    {
	    	report( severity_error, "OutputTimer_Base_c::GenerateFrameTiming(%s) - Unable to handle playback time deltas.\n", Configuration.OutputTimerName );
	    	return Status;
	    }
	}

	NextExpectedPlaybackTime	= NormalizedPlaybackTime;
    }

    //
    // Now we are applying synchronization, and we have a valid time to play with ...
    //

    Player->GetPlaybackSpeed( Playback, &Speed, &Direction );

    if( (Speed != 0) &&
	(SynchronizationPolicy == PolicyValueApply) && 
        ValidTime(ParsedFrameParameters->NormalizedPlaybackTime) )
    {
	//
	// Translate the playback time to a system time
	//

    	Status			= OutputCoordinator->TranslatePlaybackTimeToSystem( OutputCoordinatorContext, NormalizedPlaybackTime, &SystemTime );
    	if( Status == OutputCoordinatorMappingNotEstablished )
    	{
	    NormalizedDecodeTime		= ValidTime(ParsedFrameParameters->NormalizedDecodeTime) ?
							(ParsedFrameParameters->NormalizedDecodeTime + (unsigned long long)NormalizedTimeOffset) : 
							NormalizedPlaybackTime - Configuration.FrameDecodeTime;
//

	    Status				= OutputCoordinator->SynchronizeStreams( OutputCoordinatorContext, NormalizedPlaybackTime, NormalizedDecodeTime, &SystemTime );
	    TimeMappingValidForDecodeTiming	= true;
	    SynchronizationState		= SyncStateInSynchronization;
	    SynchronizationAtStartup		= true;
	    DecodeInTimeState			= DecodingInTime;
	    SeenAnInTimeFrame			= false;
    	}

    	if( Status != OutputCoordinatorNoError )
    	{
	    report( severity_error, "OutputTimer_Base_c::GenerateFrameTiming(%s) - Failed to obtain system time for playback.\n", Configuration.OutputTimerName );
	    return Status;
        }

	//
	// Do we wish to re-validate the time mapping for the decode window timing
	//

	if( !TimeMappingValidForDecodeTiming && 
	    (!ValidIndex(TimeMappingInvalidatedAtDecodeIndex) ||
	     (ParsedFrameParameters->DecodeFrameIndex > TimeMappingInvalidatedAtDecodeIndex)) )
	{
	    TimeMappingValidForDecodeTiming	= true;
	}

    	//
    	// Insert the current OutputRateAdjustment value in the output timing record
    	// I am being evil here, assuming that the output timing records with their 
    	// common section will have the same internal offsets for the common data items
    	//

    	OutputTiming->OutputRateAdjustment	= OutputRateAdjustment;
    	OutputTiming->SystemClockAdjustment	= SystemClockAdjustment;
    }
    else
    {
	SystemTime				= UNSPECIFIED_TIME;
    	OutputTiming->OutputRateAdjustment	= 1;
    	OutputTiming->SystemClockAdjustment	= 1;
    }

    //
    // Generate the timing record (update the output surface descriptor first).
    //

    Manifestor->GetSurfaceParameters( Configuration.AudioVideoDataOutputSurfaceDescriptorPointer );

    Status	= FillOutFrameTimingRecord( SystemTime, ParsedAudioVideoDataParameters, AudioVideoDataOutputTiming );
    if( Status != OutputTimerNoError )
    {
	report( severity_error, "OutputTimer_Base_c::GenerateFrameTiming(%s) - Failed to fill out timing record.\n", Configuration.OutputTimerName );
	return Status;
    }

    //
    // Handle the playback increment
    //

    if( ValidTime(NextExpectedPlaybackTime) )
    {
	NextExpectedPlaybackTime	= (Direction == PlayBackward) ? 
						(NextExpectedPlaybackTime - PlaybackTimeIncrement) :
						(NextExpectedPlaybackTime + PlaybackTimeIncrement);
    }

//

#if 0
{
static unsigned long long LastSystemTime	= 0;
static unsigned long long LastPlaybackTime	= 0;
static unsigned long long LastNativeTime	= 0;
static unsigned long long LastTiming		= 0;

unsigned long long NativeTime	= ParsedFrameParameters->NativePlaybackTime;
unsigned long long Now		= OS_GetTimeInMicroSeconds();

report( severity_info, "Frame - %6lld %6lld %6lld - %6lld %d %d - %6lld - %6lld\n", 
	SystemTime-LastSystemTime, NormalizedPlaybackTime-LastPlaybackTime, NativeTime-LastNativeTime,
	SystemTime-OutputTiming->SystemPlaybackTime, OutputTiming->DisplayCount[0], OutputTiming->DisplayCount[1],
	OutputTiming->SystemPlaybackTime-LastTiming,
	OutputTiming->SystemPlaybackTime - Now ); 

    LastSystemTime	= SystemTime;
    LastPlaybackTime	= NormalizedPlaybackTime;
    LastNativeTime	= NativeTime;
    LastTiming		= OutputTiming->SystemPlaybackTime;
}
#endif

    //
    // Mark the timing as generated - to allow for detection of re-timing
    //

    OutputTiming->TimingGenerated	= true;

//

    return OutputTimerNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
// 	The function to record the actual frame timing
//

OutputTimerStatus_t   OutputTimer_Base_c::RecordActualFrameTiming(
							Buffer_t		  Buffer )
{
PlayerStatus_t		 Status;
VideoOutputTiming_t	*OutputTiming;
unsigned long long 	 ExpectedTime;
unsigned long long 	 ActualTime;
unsigned long long 	 PreviousExpectedDuration;
unsigned long long 	 PreviousActualDuration;
unsigned char		 ExternalMappingPolicy;

//
    AssertComponentState( "OutputTimer_Base_c::RecordActualFrameTiming", ComponentRunning );

    //
    // Obtain the meta data
    //

    Status	= ExtractPointersPostManifest( Buffer );
    if( Status != PlayerNoError )
	return Status;

    OutputTiming			= (VideoOutputTiming_t *)PostManifestAudioVideoDataOutputTiming;

    //
    // mark the output timing as not newly set for the next incarnation of this buffer
    //

     OutputTiming->TimingGenerated	= false;

    //
    // Correct for clock drift 
    // NOTE here we use the assumtion that the common timing 
    // data will be at the same place in the Audio|Video|Data structures.
    //

    ExpectedTime			= OutputTiming->SystemPlaybackTime;
    ActualTime				= OutputTiming->ActualSystemPlaybackTime;

//

    if( (ExpectedTime == UNSPECIFIED_TIME) || !ValidTime(ActualTime) )
    {
	//
	// The frame was not manifested, because of trick 
	// mode or avsync or some other circumstance.
	//

	return OutputTimerNoError;
    }

//

    if( ValidTime(LastActualFrameTime) )
    {
	PreviousExpectedDuration	= ExpectedTime - LastExpectedFrameTime;
	PreviousActualDuration		= ActualTime - LastActualFrameTime;
//report( severity_info, "Durations - %6lld, %6lld - %4lld\n", PreviousExpectedDuration, PreviousActualDuration, PreviousActualDuration - PreviousExpectedDuration );
	Status	= OutputCoordinator->CalculateOutputRateAdjustment( OutputCoordinatorContext, 
								    PreviousExpectedDuration, 
								    PreviousActualDuration, 
								    (long long)(ExpectedTime - ActualTime),
								    &OutputRateAdjustment, 
								    &SystemClockAdjustment );
	if( Status != PlayerNoError )
	    report( severity_error, "OutputTimer_Base_c::RecordActualFrameTiming(%s) - Failed to calculate output rate adjusment.\n", Configuration.OutputTimerName );
    }

    LastActualFrameTime		= ActualTime;
    LastExpectedFrameTime	= ExpectedTime;

    //
    // Perform Audio/Video/Data synchronization
    //

    Status	= PerformAVDSync( ExpectedTime, ActualTime );
    if( Status != PlayerNoError )
    {
	report( severity_error, "OutputTimer_Base_c::RecordActualFrameTiming(%s) - Failed to perform synchronization.\n", Configuration.OutputTimerName );
	return Status;
    }

    //
    // Perform any required monitoring of vsync offsets
    //

    ExternalMappingPolicy	= Player->PolicyValue( Playback, Stream, PolicyExternalTimeMapping );
    if( (Configuration.StreamType == StreamTypeVideo) && 
	(ExternalMappingPolicy == PolicyValueApply) )
    {
	Status	= OutputCoordinator->MonitorVsyncOffset( OutputCoordinatorContext, ExpectedTime, ActualTime );
	if( Status == OutputCoordinatorMappingNotEstablished )
	    SynchronizationState	= SyncStateInSynchronization;		// Cannot evaluate avsync until after we have done this
    }

//

    return OutputTimerNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
// 	The function to extract the stream delay relative to other 
//	streams in the playback (Note returns coordinator values)
//

OutputTimerStatus_t   OutputTimer_Base_c::GetStreamStartDelay(	unsigned long long	 *Delay )
{
    if( OutputCoordinatorContext == NULL )
	return OutputCoordinatorMappingNotEstablished;

    return OutputCoordinator->GetStreamStartDelay( OutputCoordinatorContext, Delay );
}


// /////////////////////////////////////////////////////////////////////////
//
// 	The function responsible for managing AVD synchronization
//

OutputTimerStatus_t   OutputTimer_Base_c::PerformAVDSync(
							unsigned long long	  ExpectedFrameOutputTime,
							unsigned long long	  ActualFrameOutputTime )
{
OutputTimerStatus_t	  Status;
unsigned char		  ExternalMappingPolicy;
long long		  SynchronizationDifference;
long long 		  ErrorThreshhold;
bool			  DifferenceExceedsThreshhold;

//#define DUMP_HISTORY
//#define HISTORY_STREAMTYPE StreamTypeAudio
#ifdef DUMP_HISTORY
static long long 	  DifferenceHistory[256];
static unsigned int 	  HistoryIndex	= 0;
#endif

//

    SynchronizationFrameCount++;

    //
    // First check if the underlying time mapping has 
    // been modified to effect a synchronization change.
    //

    Status	= OutputCoordinator->MappingBaseAdjustmentApplied( OutputCoordinatorContext );
    if( Status != OutputCoordinatorNoError )
    {
        SynchronizationState	= SyncStateStartAwaitingCorrectionWorkthrough;
	DecodeInTimeState	= DecodingInTime;

	//
	// Note we do not reset SeenAnInTimeFrame, because if we are really having problems, we won't get to see one
	//
    }

    //
    // Now a switch statement depending on the state we are in
    //

    ExternalMappingPolicy	= Player->PolicyValue( Playback, Stream, PolicyExternalTimeMapping );
    if( ExternalMappingPolicy == PolicyValueApply )
    {
	ErrorThreshhold		= Configuration.SynchronizationErrorThresholdForExternalSync;
    }
    else
    {
	//
	// When running a stream, we have a loose sync threshold that 
	// gradually tightens over 3..5 mins this seems worse than it is, 
	// most of the error during this period is due to the error in the
 	// clock adjustments being worked out.
	// 

	ErrorThreshhold		= Configuration.SynchronizationErrorThreshold;
	if( SynchronizationFrameCount < 2048 )
	    ErrorThreshhold	= max( ErrorThreshhold, (4 * SynchronizationFrameCount) );
	else if( SynchronizationFrameCount < 4096 )
	    ErrorThreshhold	= 4 * ErrorThreshhold;
	else if( SynchronizationFrameCount < 6144 )
	    ErrorThreshhold	= 2 * ErrorThreshhold;
	else if( SynchronizationFrameCount < 8192 )
	    ErrorThreshhold	= (3 * ErrorThreshhold) / 2;
    }

    SynchronizationDifference	= (long long)ActualFrameOutputTime - (long long)ExpectedFrameOutputTime;
    DifferenceExceedsThreshhold	= !inrange( SynchronizationDifference, -ErrorThreshhold, ErrorThreshhold );
//report( severity_info, "Diff %5lld - %d %d\n", SynchronizationDifference, SynchronizationState, SynchronizationErrorIntegrationCount );
#ifdef DUMP_HISTORY
    if( Configuration.StreamType == HISTORY_STREAMTYPE ) 
	DifferenceHistory[(HistoryIndex++) & 0xff]	= SynchronizationDifference;
#endif

//

    switch( SynchronizationState )
    {
	//
	// We believe we are in sync, can transition to suspected error.
	// if we remain in sync, then we reset the startup flag, as any
	// future errors cannot be at startup.
	//

	case SyncStateInSynchronization:
	    if( DifferenceExceedsThreshhold )
	    {
		SynchronizationState			= SyncStateSuspectedSyncError;
		SynchronizationAccumulatedError		= SynchronizationDifference;
		SynchronizationErrorIntegrationCount	= 1;
	    }
	    else
	    {
		SynchronizationAtStartup		= false;
	    }
	    break;

	//
	// We have a suspected sync error, we integrate over a number of values
	// to elliminate glitch efects. If we confirm the error then we fall through
	// to the next state.
	//

	case SyncStateSuspectedSyncError:
	    if( DifferenceExceedsThreshhold && SameSign(SynchronizationDifference, SynchronizationAccumulatedError) )
	    {
		SynchronizationAccumulatedError		+= SynchronizationDifference;
		SynchronizationErrorIntegrationCount++;
		if( SynchronizationErrorIntegrationCount < Configuration.SynchronizationIntegrationCount )
		    break;
	    }
	    else
	    {
		SynchronizationState			= SyncStateInSynchronization;
		break;
	    }

	    // Fallthrough
	    SynchronizationState			= SyncStateConfirmedSyncError;

	//
	// Confirmed sync error, enterred by a fallthrough, 
	// We request the stream specific function to correct 
	// it, then we fallthrough to start awaiting the 
	// correction to work through.
	//

	case SyncStateConfirmedSyncError:

	    SynchronizationError	= SynchronizationAccumulatedError / SynchronizationErrorIntegrationCount;

	    if (SynchronizationError < 10000)
	        report( severity_info, "AVDsync %s %s - < 10000 us\n", ((ExternalMappingPolicy == PolicyValueApply) ? "Error" : "Correction"), 
									     LookupStreamType( Configuration.StreamType ) );
	    else
	        report( severity_info, "AVDsync %s %s - %6lld us\n", ((ExternalMappingPolicy == PolicyValueApply) ? "Error" : "Correction"), 
									     LookupStreamType( Configuration.StreamType ), SynchronizationError );
#ifdef DUMP_HISTORY
	    if( !SynchronizationAtStartup && (Configuration.StreamType == HISTORY_STREAMTYPE) )
	    {
		unsigned int i;
		for( i=0; i<256; i+=8 )
		    report( severity_info, "\t%3d - %6lld %6lld %6lld %6lld %6lld %6lld %6lld %6lld\n", i,
				DifferenceHistory[(HistoryIndex + i + 0) & 0xff],
				DifferenceHistory[(HistoryIndex + i + 1) & 0xff],
				DifferenceHistory[(HistoryIndex + i + 2) & 0xff],
				DifferenceHistory[(HistoryIndex + i + 3) & 0xff],
				DifferenceHistory[(HistoryIndex + i + 4) & 0xff],
				DifferenceHistory[(HistoryIndex + i + 5) & 0xff],
				DifferenceHistory[(HistoryIndex + i + 6) & 0xff],
				DifferenceHistory[(HistoryIndex + i + 7) & 0xff] );
		report( severity_fatal, "That's all folks.\n" );
	    }
#endif
	    Status			= CorrectSynchronizationError();
	    if( Status != OutputTimerNoError )
	    {
		report( severity_error, "OutputTimer_Base_c::PerformAVDSync - Failed to correct error.\n" );
		// used to break here and re-try, but it seems pointless, may as well perform the normal flow
	    }

	    //
	    // Synchronization corrections can often mess up the output rate integration, 
	    // so we restart it here, but only if this was a correctable thing.
	    //

	    if( ExternalMappingPolicy == PolicyValueDisapply )
		OutputCoordinator->RestartOutputRateIntegration( OutputCoordinatorContext );

	    // Fallthrough
	    SynchronizationState			= SyncStateStartAwaitingCorrectionWorkthrough;

	//
	// Here we set up to wait for as correction to work through
	//

	case SyncStateStartAwaitingCorrectionWorkthrough:

	    SynchronizationState			= SyncStateAwaitingCorrectionWorkthrough;
	    FrameWorkthroughCount			= 0;
	    break;

	//
	// Here we handle waiting for a correction to workthrough
	//

	case SyncStateAwaitingCorrectionWorkthrough:

	    FrameWorkthroughCount++;
	    if( FrameWorkthroughCount >= Configuration.SynchronizationWorkthroughCount )
		SynchronizationState			= SyncStateInSynchronization;

	    break;
    }
//

    return OutputTimerNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
// 	Read the trick mode parameters, and set the trick mode 
//	domain boundaries.
//

void   OutputTimer_Base_c::SetTrickModeDomainBoundaries( void )
{
Rational_t			  Tmp0;
Rational_t			  Tmp1;
Rational_t			  MaxNormalRate;
Rational_t			  MaxSubstandardRate;
unsigned char			  PolicyValue;
Rational_t			  LowerClamp;

    //
    // Calculate useful numbers
    //

    LastCodedFrameRate				= CodedFrameRate;
    EmpiricalCodedFrameRateLimit		= min( TrickModeParameters.EmpiricalMaximumDecodeFrameRateShortIntegration, TrickModeParameters.EmpiricalMaximumDecodeFrameRateLongIntegration );
// Set 2 fps below empirical measure
    EmpiricalCodedFrameRateLimit		= EmpiricalCodedFrameRateLimit.TruncateDenominator(1) - 2;

    MaxNormalRate				= EmpiricalCodedFrameRateLimit / CodedFrameRate;
    MaxSubstandardRate				= TrickModeParameters.SubstandardDecodeSupported ?
							(MaxNormalRate * TrickModeParameters.SubstandardDecodeRateIncrease) :
							MaxNormalRate;

    PolicyValue	= Player->PolicyValue( Playback, Stream, PolicyAllowFrameDiscardAtNormalSpeed );
    LowerClamp	= (PolicyValue == PolicyValueApply) ? Rational_t(1,4) : 1;

    Clamp( MaxNormalRate,      LowerClamp, 16 );
    Clamp( MaxSubstandardRate, LowerClamp, 16 );

    //
    // Calculate the domain boundaries for trick modes
    //

    TrickModeDomainTransitToDegradeNonReference	= MaxNormalRate;

    Tmp0					= (MaxSubstandardRate * TrickModeGroupStructureNonReferenceFrames);
    Tmp1					= (MaxNormalRate      * TrickModeGroupStructureReferenceFrames);
    TrickModeDomainTransitToStartDiscard	= Tmp0 + Tmp1;

    TrickModeDomainTransitToDecodeReference	= MaxNormalRate / TrickModeGroupStructureReferenceFrames;

#if 0
    Tmp0					= (MaxNormalRate * TrickModeGroupStructureIndependentFrames);
    Tmp1					= (TrickModeGroupStructureReferenceFrames - TrickModeGroupStructureIndependentFrames);
    Tmp1					= MaxSubstandardRate * Tmp1;
    Tmp0					= Tmp0 + Tmp1;
    Tmp1 					= (TrickModeGroupStructureReferenceFrames * TrickModeGroupStructureReferenceFrames);
    TrickModeDomainTransitToDecodeKey		= Tmp0 / Tmp1;
#else
    // Nick changed this because we cannot apply substandard decode to reference frames
    // further changed to avoid going to key frames only when at normal speed, and we cannot keep up
    TrickModeDomainTransitToDecodeKey		= max( TrickModeDomainTransitToDecodeReference, 1 ); 
#endif

#if 0
    report( severity_info, "OutputTimer_Base_c::SetTrickModeDomainBoundaries - %4d.%06d, %4d.%06d, %4d.%06d, %4d.%06d\n",
		TrickModeDomainTransitToDegradeNonReference.IntegerPart(), TrickModeDomainTransitToDegradeNonReference.RemainderDecimal(),
		TrickModeDomainTransitToStartDiscard.IntegerPart(), TrickModeDomainTransitToStartDiscard.RemainderDecimal(),
		TrickModeDomainTransitToDecodeReference.IntegerPart(), TrickModeDomainTransitToDecodeReference.RemainderDecimal(),
		TrickModeDomainTransitToDecodeKey.IntegerPart(), TrickModeDomainTransitToDecodeKey.RemainderDecimal() );
#endif

}


// /////////////////////////////////////////////////////////////////////////
//
// 	Read the trick mode parameters, and set the trick mode 
//	domain boundaries.
//

OutputTimerStatus_t   OutputTimer_Base_c::TrickModeControl( void )
{
PlayerStatus_t			  Status;
bool				  TrickModeParametersChanged;
bool				  CodedFrameRateChanged;
bool				  GroupStructureChanged;
Rational_t			  NewEmpiricalCodedFrameRateLimit;
Rational_t			  Speed;
PlayDirection_t			  Direction;
unsigned char			  TrickModePolicy;
PlayerEventRecord_t		  Event;

    //
    // Read speed policy etc
    //

    Player->GetPlaybackSpeed( Playback, &Speed, &Direction );

    TrickModePolicy			= Player->PolicyValue( Playback, Stream, PolicyTrickModeDomain );

    NewEmpiricalCodedFrameRateLimit	= min( TrickModeParameters.EmpiricalMaximumDecodeFrameRateShortIntegration, TrickModeParameters.EmpiricalMaximumDecodeFrameRateLongIntegration );
    TrickModeParametersChanged		= !inrange( NewEmpiricalCodedFrameRateLimit, EmpiricalCodedFrameRateLimit, (EmpiricalCodedFrameRateLimit + 8) );
    CodedFrameRateChanged		= LastCodedFrameRate != CodedFrameRate;
    GroupStructureChanged		= (TrickModeGroupStructureIndependentFrames != LastTrickModeGroupStructureIndependentFrames) ||
					  (TrickModeGroupStructureReferenceFrames != LastTrickModeGroupStructureReferenceFrames);

    //
    // If we are paused, then we do not adjust the trick mode control
    //

    if( Speed == 0 )
	return OutputTimerNoError;

    //
    // If anything has changed, then recalculate the values to determine which domain we are in
    //

    if( TrickModeParametersChanged || CodedFrameRateChanged || GroupStructureChanged )
	SetTrickModeDomainBoundaries();

    if( (Speed 			!= LastTrickModeSpeed)	|| 
	(TrickModePolicy	!= LastTrickModePolicy) ||
	TrickModeParametersChanged 			||
	GroupStructureChanged )
    {
	//
	// Calculate the domain
	//

	if( TrickModePolicy == PolicyValueTrickModeAuto )
	{
	    if( Speed > TrickModeDomainTransitToDecodeKey )
		TrickModeDomain	= TrickModeDecodeKeyFrames;
	    else if( Speed > TrickModeDomainTransitToDecodeReference )
		TrickModeDomain	= TrickModeDecodeReferenceFramesDegradeNonKeyFrames;
	    else if( Speed > TrickModeDomainTransitToStartDiscard )
		TrickModeDomain	= TrickModeStartDiscardingNonReferenceFrames;
	    else if( Speed > TrickModeDomainTransitToDegradeNonReference )
		TrickModeDomain	= TrickModeDecodeAllDegradeNonReferenceFrames;
	    else 
		TrickModeDomain	= TrickModeDecodeAll;
	}
	else
	{
	    TrickModeDomain	= (TrickModeDomain_t)TrickModePolicy;
	}

	//
    	// Need to recalculate the parameters for discard.
    	//

	PortionOfPreDecodeFramesToLose		= 0;

	switch( TrickModeDomain )
	{
	    case TrickModeInvalid:
	    case TrickModeDecodeAll:
	    case PolicyValueTrickModeDecodeAllDegradeNonReferenceFrames:
			AdjustedSpeedAfterFrameDrop	= Speed;
			break;

	    case TrickModeStartDiscardingNonReferenceFrames:
			PortionOfPreDecodeFramesToLose	= (1 - (TrickModeDomainTransitToDegradeNonReference / Speed)) / 
								TrickModeGroupStructureNonReferenceFrames;
			if( PortionOfPreDecodeFramesToLose > 1 )
			    PortionOfPreDecodeFramesToLose	= 1;

			AdjustedSpeedAfterFrameDrop	= Speed * (1 - (PortionOfPreDecodeFramesToLose*TrickModeGroupStructureNonReferenceFrames)); 
			break;

	    case TrickModeDecodeReferenceFramesDegradeNonKeyFrames:
			AdjustedSpeedAfterFrameDrop	= Speed * TrickModeGroupStructureReferenceFrames;
			break;

	    case TrickModeDecodeKeyFrames:
			AdjustedSpeedAfterFrameDrop	= Speed * TrickModeGroupStructureIndependentFrames;
			break;

	    default:	AdjustedSpeedAfterFrameDrop	= Speed;
			break;
	}

	//
	// We need to avoid riduculous adjusted speeds, and allow manifestation times 
	// to take the place of conditions where a large adjustment would have functioned.
	//

	Clamp( AdjustedSpeedAfterFrameDrop, 1, 120 ); 	// Speed only goes less than 1 when we cannot keep up

	//
	// Do we need to signal a change in the trick mode domain
	//
	
	if( (TrickModeDomain != LastTrickModeDomain) &&
	    ((EventTrickModeDomainChange & EventMask) != 0) )
	{
	    Event.Code			= EventTrickModeDomainChange;
	    Event.Playback		= Playback;
	    Event.Stream		= Stream;
	    Event.PlaybackTime		= TIME_NOT_APPLICABLE;
	    Event.UserData		= EventUserData;
	    Event.Value[0].UnsignedInt	= TrickModeDomain;
	    Event.Value[1].UnsignedInt	= LastTrickModeDomain;

	    Status			= Player->SignalEvent( &Event );
	    if( Status != PlayerNoError )
		report( severity_error, "OutputTimer_Base_c::TrickModeControl - Failed to signal event.\n" );
	}

	//
	// If we are switching from domain >= TrickModeDecodeKeyFrames, IE we are losing reference frames
	// to one < TrickModeDecodeKeyFrames, IE where those reference frames may be needed. We set
	// the flag to force checking reference frames until we see a specified number of key frames.
	//

	if( (LastTrickModeDomain >= TrickModeDecodeKeyFrames) &&
	    (TrickModeDomain     <  TrickModeDecodeKeyFrames) )
	    TrickModeCheckReferenceFrames	= TRICK_MODE_CHECK_REFERENCE_FRAMES_FOR_N_KEY_FRAMES;

	if( TrickModeDomain != LastTrickModeDomain )
	    AccumulatedPreDecodeFramesToLose	= 0;

#if 0
	if( (TrickModeDomain != LastTrickModeDomain) ||
	    (PortionOfPreDecodeFramesToLose != LastPortionOfPreDecodeFramesToLose) )
	{
	    report( severity_info, "OutputTimer_Base_c::TrickModeControl - Domain = %d, PortionToLose = %4d.%06d, Adjusted speed = %4d.%06d, max decode rate = %4d.%06d\n",
			TrickModeDomain,
			PortionOfPreDecodeFramesToLose.IntegerPart(), PortionOfPreDecodeFramesToLose.RemainderDecimal(),
			AdjustedSpeedAfterFrameDrop.IntegerPart(), AdjustedSpeedAfterFrameDrop.RemainderDecimal(),
			EmpiricalCodedFrameRateLimit.IntegerPart(), EmpiricalCodedFrameRateLimit.RemainderDecimal() );
	}
#endif

	LastTrickModePolicy				= TrickModePolicy;
	LastTrickModeDomain				= TrickModeDomain;
	LastPortionOfPreDecodeFramesToLose		= PortionOfPreDecodeFramesToLose;
	LastTrickModeSpeed				= Speed;
	LastTrickModeGroupStructureIndependentFrames	= TrickModeGroupStructureIndependentFrames;
	LastTrickModeGroupStructureReferenceFrames	= TrickModeGroupStructureReferenceFrames;
    }

//

    return OutputTimerNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
// 	Function to monitor/init group structure data.
//

void   OutputTimer_Base_c::InitializeGroupStructure( void )
{
unsigned int			  i;

//

    NextGroupStructureEntry	= 0;

    for( i=0; i<GROUP_STRUCTURE_INTEGRATION_PERIOD; i++ )
    {
	GroupStructureSizes[i]		= TrickModeParameters.DefaultGroupSize;
	GroupStructureReferences[i]	= max(TrickModeParameters.DefaultGroupReferenceFrameCount, 1);
    }

    GroupStructureSizeCount		= 0;
    GroupStructureReferenceCount	= 0;

    TrickModeGroupStructureIndependentFrames	= Rational_t(1, TrickModeParameters.DefaultGroupSize);
    TrickModeGroupStructureReferenceFrames	= Rational_t(max(TrickModeParameters.DefaultGroupReferenceFrameCount, 1), TrickModeParameters.DefaultGroupSize);
    TrickModeGroupStructureNonReferenceFrames	= 1 - TrickModeGroupStructureReferenceFrames;
}


//


void   OutputTimer_Base_c::MonitorGroupStructure(	ParsedFrameParameters_t	 *ParsedFrameParameters )
{
unsigned int 	i;
unsigned int	AccumulatedGroupStructureSizes;
unsigned int	AccumulatedGroupStructureReferences;

//

    if( ParsedFrameParameters->IndependentFrame && (GroupStructureSizeCount != 0) )
    {
	GroupStructureSizes[NextGroupStructureEntry]		= GroupStructureSizeCount;
	GroupStructureReferences[NextGroupStructureEntry]	= max(GroupStructureReferenceCount, 1);
	NextGroupStructureEntry					= (NextGroupStructureEntry + 1) % GROUP_STRUCTURE_INTEGRATION_PERIOD;

//

	AccumulatedGroupStructureSizes				= 0;
	AccumulatedGroupStructureReferences			= 0;
	for( i=0; i<GROUP_STRUCTURE_INTEGRATION_PERIOD; i++ )
	{
	    AccumulatedGroupStructureSizes			+= GroupStructureSizes[i];
	    AccumulatedGroupStructureReferences			+= GroupStructureReferences[i];
	}

//

	TrickModeGroupStructureIndependentFrames		= Rational_t(GROUP_STRUCTURE_INTEGRATION_PERIOD, AccumulatedGroupStructureSizes);
	TrickModeGroupStructureReferenceFrames			= Rational_t(AccumulatedGroupStructureReferences, AccumulatedGroupStructureSizes);
	TrickModeGroupStructureNonReferenceFrames		= 1 - TrickModeGroupStructureReferenceFrames;

//

#if 0
report( severity_info, "Group Structure %d %d - %d.%06d - %d.%06d - %d.%06d\n", GroupStructureReferenceCount, GroupStructureSizeCount,
		IntegerPart(TrickModeGroupStructureIndependentFrames), RemainderDecimal(TrickModeGroupStructureIndependentFrames),
		IntegerPart(TrickModeGroupStructureReferenceFrames), RemainderDecimal(TrickModeGroupStructureReferenceFrames),
		IntegerPart(TrickModeGroupStructureNonReferenceFrames), RemainderDecimal(TrickModeGroupStructureNonReferenceFrames) );
#endif

	GroupStructureSizeCount					= 0;
	GroupStructureReferenceCount				= 0;
    }

//

    GroupStructureSizeCount++;
    if( ParsedFrameParameters->ReferenceFrame )
	GroupStructureReferenceCount++;
}


// /////////////////////////////////////////////////////////////////////////
//
// 	Function to monitor/init group structure data.
//

void   OutputTimer_Base_c::DecodeInTimeFailure(	unsigned long long 	FailedBy )
{
PlayerStatus_t			  Status;
unsigned int 			  CodedFrameBufferCount;
unsigned int			  CodedFrameBuffersInUse;
unsigned int			  CodedFrameBufferMemoryInPool;
unsigned int			  CodedFrameBufferMemoryAllocated;
unsigned int			  CodedFrameBufferLargestFreeMemoryBlock;
unsigned int			  DecodeBufferCount;
unsigned int			  DecodeBuffersInUse;
unsigned char			  Policy;
bool				  Rebase;
PlayerEventIdentifier_t		  EventCode;
PlayerEventRecord_t		  Event;

    //
    // Check if the underlying time mapping has 
    // been modified to effect a synchronization change.
    //

    Status	= OutputCoordinator->MappingBaseAdjustmentApplied( OutputCoordinatorContext );
    if( Status != OutputCoordinatorNoError )
    {
        SynchronizationState	= SyncStateStartAwaitingCorrectionWorkthrough;
	DecodeInTimeState	= DecodingInTime;

	//
	// Note we do not reset SeenAnInTimeFrame, because if we are really having problems, we won't get to see one
	//

	return;
    }

    //
    // I am sure we are going to be interested in the coded frame buffer pool usage
    //

    CodedFrameBufferPool->GetPoolUsage( &CodedFrameBufferCount,
					&CodedFrameBuffersInUse, 
					&CodedFrameBufferMemoryInPool,
					&CodedFrameBufferMemoryAllocated,
					NULL,
					&CodedFrameBufferLargestFreeMemoryBlock );

    DecodeBufferPool->GetPoolUsage(	NULL,
					&DecodeBuffersInUse, 
					NULL, NULL, NULL );

    Manifestor->GetDecodeBufferCount( &DecodeBufferCount );

    //
    // Switch on the state
    //

    switch( DecodeInTimeState )
    {
	case DecodingInTime:
				DecodeInTimeFailures		= 1;
				DecodeInTimeCodedDataLevel	= CodedFrameBuffersInUse;
				DecodeInTimeBehind		= FailedBy;
				DecodeInTimeState		= AccumulatingDecodeInTimeFailures;
				break;

	case AccumulatingDecodeInTimeFailures:
				DecodeInTimeFailures++;
				if( DecodeInTimeFailures < DECODE_IN_TIME_INTEGRATION_PERIOD )
				    break;
				DecodeInTimeState		= AccumulatedDecodeInTimeFailures;

	case AccumulatedDecodeInTimeFailures:
				Rebase				= false;
				EventCode			= EventIllegalIdentifier;

				if( DecodeBuffersInUse >= (DecodeBufferCount - 1) )
				{
				    //
				    // Failure to decode in time, but all the decode buffers are full,
				    // this has been seen in previous code bugs where we have a failure of
				    // the output timer, or the manifestor.
				    //

				    report( severity_error, "OutputTimer_Base_c::DecodeInTimeFailure(%s) - Failed to decode in time but decode buffers full (%2d %2d/%2d %2d/%2d) (%6lld %6lld)- probable Implementation Error.\n",
						Configuration.OutputTimerName,
						DecodeInTimeCodedDataLevel, CodedFrameBuffersInUse, CodedFrameBufferCount, 
						DecodeBuffersInUse, DecodeBufferCount,
						DecodeInTimeBehind, FailedBy );

				    Rebase	= true;		// Try to mitigate the effect of this state.
				}
				else if( (CodedFrameBuffersInUse			< (CodedFrameBufferCount / 3)) &&
					 (CodedFrameBufferLargestFreeMemoryBlock	> CodedFrameBufferMaximumSize) &&
					 (CodedFrameBufferMemoryAllocated		< (CodedFrameBufferMemoryInPool / 2)) )
				{
				    //
				    // Failure to deliver data on time
				    //

				    report( severity_info, "OutputTimer_Base_c::DecodeInTimeFailure(%s) - Failed to deliver data to decoder in time (%2d %2d/%2d %2d/%2d) (%6lld %6lld).\n",
						Configuration.OutputTimerName,
						DecodeInTimeCodedDataLevel, CodedFrameBuffersInUse, CodedFrameBufferCount, 
						DecodeBuffersInUse, DecodeBufferCount,
						DecodeInTimeBehind, FailedBy );

				    Player->CheckForDemuxBufferMismatch( Playback, Stream );

				    Policy	= Player->PolicyValue( Playback, Stream, PolicyRebaseOnFailureToDeliverDataInTime );
				    Rebase	= Policy == PolicyValueApply;
				    EventCode	= EventFailedToDeliverDataInTime;
				}
				else if( FailedBy > DecodeInTimeBehind )
				{
				    //
				    // Failure to decode in time
				    //

				    report( severity_info, "OutputTimer_Base_c::DecodeInTimeFailure(%s) - Failed to decode frames in time (%2d %2d/%2d %2d/%2d) (%6lld %6lld).\n",
						Configuration.OutputTimerName,
						DecodeInTimeCodedDataLevel, CodedFrameBuffersInUse, CodedFrameBufferCount, 
						DecodeBuffersInUse, DecodeBufferCount,
						DecodeInTimeBehind, FailedBy );

				    Policy			= Player->PolicyValue( Playback, Stream, PolicyRebaseOnFailureToDecodeInTime );
				    Rebase			= Policy == PolicyValueApply;

				    Policy			= Player->PolicyValue( Playback, Stream, PolicyAllowFrameDiscardAtNormalSpeed );
				    if( Rebase && (Policy == PolicyValueApply) )
					report( severity_error, "OutputTimer_Base_c::DecodeInTimeFailure(%s) - Both PolicyRebaseOnFailureToDecodeInTime & \n\t\t\t\t\tPolicyAllowFrameDiscardAtNormalSpeed are enabled at the same time, \n\t\t\t\t\tthe use of one of these should preclude the use of the other.\n" );

				    EventCode			= EventFailedToDecodeInTime;
				}
				else
				{
				    //
				    // Give the system the benefit of the doubt
				    // It may be getting better.
				    //

				    DecodeInTimeState		= DecodingInTime;
				    break;
				}

				//
				// Did we want to rebase
				//

				if( Rebase )
				{
				    OutputCoordinator->AdjustMappingBase( PlaybackContext, FailedBy + REBASE_MARGIN );
				}

				//
				// Did we want to raise an event
				//

				if( (EventCode & EventMask) != 0 )
				{
				    Event.Code		= EventCode;
				    Event.Playback	= Playback;
				    Event.Stream	= Stream;
				    Event.PlaybackTime	= TIME_NOT_APPLICABLE;
				    Event.UserData	= EventUserData;

				    Status		= Player->SignalEvent( &Event );
				    if( Status != PlayerNoError )
					report( severity_error, "OutputTimer_Base_c::DecodeInTimeFailure(%s) - Failed to signal event.\n", Configuration.OutputTimerName );
				}

				//

				DecodeInTimeState	= PauseBetweenDecodeInTimeIntegrations;
				DecodeInTimeFailures	= 0;

	case PauseBetweenDecodeInTimeIntegrations:
				DecodeInTimeFailures++;
				if( DecodeInTimeFailures >= DECODE_IN_TIME_PAUSE_BETWEEN_INTEGRATION_PERIOD )
				    DecodeInTimeState	= DecodingInTime;
				break;
    }
}


// /////////////////////////////////////////////////////////////////////////
//
// 	Extract pointers, these two functions extract the pointers 
//	appropriate to a buffer either before or after a decode, 
//	this common code is used to ensure that we only actually 
//	do the extraction once for each buffer in each circumstance,
//	since we need these pointers in at least three cases in the 
//	decode->manifest sequence.
//

OutputTimerStatus_t   OutputTimer_Base_c::ExtractPointersPreDecode(	Buffer_t		Buffer )
{
PlayerStatus_t	  Status;

    //
    // Obtain the parsed frame parameters
    //

    Status	= Buffer->ObtainMetaDataReference( Player->MetaDataParsedFrameParametersType, (void **)(&PreDecodeParsedFrameParameters) );
    if( Status != PlayerNoError )
    {
	report( severity_error, "OutputTimer_Base_c::ExtractPointersPreDecode(%s) - Unable to obtain the meta data \"ParsedFrameParameters\".\n", Configuration.OutputTimerName );
	return Status;
    }

    //
    // And the stream type specific frame parameters 
    //

    Status	= Buffer->ObtainMetaDataReference( Configuration.AudioVideoDataParsedParametersType, (void **)(&PreDecodeParsedAudioVideoDataParameters) );
    if( Status != PlayerNoError )
    {
	report( severity_error, "OutputTimer_Base_c::ExtractPointersPreDecode(%s) - Unable to obtain the meta data \"Parsed[Audio|Video|Data]Parameters\".\n", Configuration.OutputTimerName );
	return Status;
    }

    return OutputTimerNoError;
}


//


OutputTimerStatus_t   OutputTimer_Base_c::ExtractPointersPostDecode(	Buffer_t		Buffer )
{
PlayerStatus_t	  Status;

    //
    // Extract the appropriate data pointers
    //

    Status	= Buffer->ObtainMetaDataReference( Player->MetaDataParsedFrameParametersReferenceType, (void **)(&PostDecodeParsedFrameParameters) );
    if( Status != PlayerNoError )
    {
	report( severity_error, "OutputTimer_Base_c::ExtractPointersPostDecode(%s) - Unable to obtain the meta data \"ParsedFrameParametersReference\".\n", Configuration.OutputTimerName );
	return Status;
    }

//

    Status	= Buffer->ObtainMetaDataReference( Configuration.AudioVideoDataParsedParametersType, &PostDecodeParsedAudioVideoDataParameters );
    if( Status != PlayerNoError )
    {
	report( severity_error, "OutputTimer_Base_c::ExtractPointersPostDecode(%s) - Unable to obtain the meta data \"Audio|Video|DataParsedFrameParameters\".\n", Configuration.OutputTimerName );
	return Status;
    }

//

    Status	= Buffer->ObtainMetaDataReference( Configuration.AudioVideoDataOutputTimingType, &PostDecodeAudioVideoDataOutputTiming );
    if( Status != PlayerNoError )
    {
	report( severity_error, "OutputTimer_Base_c::ExtractPointersPostDecode(%s) - Unable to obtain the meta data \"Audio|Video|DataOutputTiming\".\n", Configuration.OutputTimerName );
	return Status;
    }

    return OutputTimerNoError;
}


//


OutputTimerStatus_t   OutputTimer_Base_c::ExtractPointersPostManifest(	Buffer_t		Buffer )
{
PlayerStatus_t	  Status;

    //
    // Extract the appropriate data pointers
    //

    Status	= Buffer->ObtainMetaDataReference( Configuration.AudioVideoDataOutputTimingType, &PostManifestAudioVideoDataOutputTiming );
    if( Status != PlayerNoError )
    {
	report( severity_error, "OutputTimer_Base_c::ExtractPointersPostManifest(%s) - Unable to obtain the meta data \"Audio|Video|DataOutputTiming\".\n", Configuration.OutputTimerName );
	return Status;
    }

    return OutputTimerNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
// 	Time functions 
//
//	NOTE these are playback times, which are independant of the direction of play
//	so play direction does not affect them in any way.
//

bool   OutputTimer_Base_c::PlaybackTimeCheckGreaterThanOrEqual(
							unsigned long long 	T0,
							unsigned long long	T1 )
{
    if( !ValidTime(T0) || !ValidTime(T1) )
	return false;

    return (T0 >= T1);
}

//

bool   OutputTimer_Base_c::PlaybackTimeCheckIntervalsIntersect(
							unsigned long long 	I0Start,
							unsigned long long	I0End,
							unsigned long long 	I1Start,
							unsigned long long	I1End )
{
    if( !ValidTime(I0Start) )
	I0Start	= 0;

    if( !ValidTime(I0End) )
	I0End	= 0xffffffffffffffffULL;

    if( !ValidTime(I1Start) )
	I1Start	= 0;

    if( !ValidTime(I1End) )
	I1End	= 0xffffffffffffffffULL;

    //
    // if I0Start < I1Start 
    //		intersect if I0End >= I1Start
    // else
    //		intersect if I0Start <= I1End
    //

    return (I0Start < I1Start) ? (I0End > I1Start) : (I0Start < I1End);
}

const char *OutputTimer_Base_c::LookupStreamType(PlayerStreamType_t StreamType)
{
#define E(x) case StreamType##x: return #x
    switch( StreamType )
    {
    E(None);
    E(Audio);
    E(Video);
    E(Other);
    default: return "INVALID";
    }
#undef E    
}
