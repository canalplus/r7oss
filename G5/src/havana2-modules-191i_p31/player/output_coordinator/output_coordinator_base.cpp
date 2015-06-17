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

Source file name : output_coordinator_base.cpp
Author :           Nick

Implementation of the base output coordinator class for player 2.


Date        Modification                                    Name
----        ------------                                    --------
12-Mar-06   Created                                         Nick

************************************************************************/

// /////////////////////////////////////////////////////////////////////
//
//      Include any component headers

#include "output_coordinator_base.h"

// /////////////////////////////////////////////////////////////////////////
//
// Locally defined constants
//

#define SYNCHRONIZE_WAIT                                50
#define MAX_SYNCHRONIZE_WAITS                           4

#define MAXIMUM_STARTUP_DELAY                           200             // ms
#define DEFAULT_STARTUP_DELAY                           40

#define MINIMUM_TIME_TO_MANIFEST                        50000           // I am guessing that it will take at least 50 ms to get from synchronize to the manifestor

#define NEGATIVE_REASONABLE_LIMIT                       (unsigned long long)(-4000000ll)
#define POSITIVE_REASONABLE_LIMIT                       (unsigned long long)(+4000000ll)

#define REBASE_TIME_TRIGGER_VALUE                       0x10000000

#define PLAYBACK_TIME_JUMP_ERROR                        64000           // 64 milli seconds, some PTSs are specified in ms so need at least 2, however DVR adds errors of upto around 32 ms when it calculates it's ptss
#define OTHER_STREAMS_MUST_FOLLOW_JUMP_BY               250000          // 250ms if you haven't followed a jump in 250 ms, then you probably aren't following it at all

#define MAX_SYNCHRONIZATION_WINDOW                      10000000        // If two streams are more than 10 seconds apart, we have no hope of avsyncing them

#define INTEGRATION_COUNT_FOR_VSYNC_OFFSET              4

#define CLOCK_RECOVERY_MINIMUM_POINTS                   4
#define CLOCK_RECOVERY_MINIMUM_INTEGRATION_TIME         2000000         // us
#define CLOCK_RECOVERY_MAXIMUM_INTEGRATION_TIME         512000000       // us


// /////////////////////////////////////////////////////////////////////////
//
// Static declaration of the output rate parameter sets
//	Minimum Integration Frames
//	Maximum Integration Frames
//	Ignore between integrations
//	Maximum Jitter allowed

static OutputRateAdjustmentParameters_t		ORAInputFollowingAudio 	= {  128, 2048, 64,  256, 32 };
static OutputRateAdjustmentParameters_t		ORAOutputDrivenAudio 	= {  512, 4096, 64, 2048, 32 };
static OutputRateAdjustmentParameters_t		ORAInputFollowingVideo	= {  128, 2048, 64,  256, 32 };
static OutputRateAdjustmentParameters_t		ORAOutputDrivenVideo 	= { 2048, 8192, 64, 8192, 32 };

// /////////////////////////////////////////////////////////////////////////
//
// Locally defined structures
//

// /////////////////////////////////////////////////////////////////////////
//
// Locally defined macros
//

#define StreamType()                    ((Context->StreamType == 1) ? "Audio" : "Video")

// /////////////////////////////////////////////////////////////////////////
//
//      The Constructor function
//
OutputCoordinator_Base_c::OutputCoordinator_Base_c( void )
{
    InitializationStatus        = OutputCoordinatorError;

//

    OS_InitializeMutex( &Lock );
    OS_InitializeEvent( &SynchronizeMayHaveCompleted );

    StreamCount                 = 0;
    CoordinatedContexts         = NULL;

    Reset();

//

    InitializationStatus        = OutputCoordinatorNoError;
}

// /////////////////////////////////////////////////////////////////////////
//
//      The Destructor function
//

OutputCoordinator_Base_c::~OutputCoordinator_Base_c(    void )
{
    Halt();
    Reset();

    OS_TerminateEvent( &SynchronizeMayHaveCompleted );
    OS_TerminateMutex( &Lock );
}

// /////////////////////////////////////////////////////////////////////////
//
//      The Halt function, give up access to any registered resources
//
//      NOTE for some calls we ignore the return statuses, this is because
//      we will procede with the halt even if we fail (what else can we do)
//

OutputCoordinatorStatus_t   OutputCoordinator_Base_c::Halt(     void )
{
    if( TestComponentState(ComponentRunning) )
    {
	//
	// Move the base state to halted early, to ensure 
	// no activity can be queued once we start halting
	//

	BaseComponentClass_c::Halt();

	OS_SetEvent( &SynchronizeMayHaveCompleted );
    }

//

    return OutputCoordinatorNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
//      The Reset function release any resources, and reset all variables
//

OutputCoordinatorStatus_t   OutputCoordinator_Base_c::Reset(    void )
{
    //
    // Delete the contexts
    //

    while( CoordinatedContexts != NULL )
	DeRegisterStream( CoordinatedContexts );

    //
    // Initialize the system/playback time mapping
    //

    StreamCount                         = 0;
    StreamsInSynchronize                = 0;

    MasterTimeMappingEstablished        = false;
    MasterTimeMappingVersion            = 0;
    GotAMasterClock                     = false;
    GotAnAlternateMasterClock           = false;
    AlternateMasterContext              = NULL;
    SystemClockAdjustmentEstablished    = true;
    SystemClockAdjustment               = 1;

    GotAVideoStream                     = false;

    AccumulatedPlaybackTimeJumpsSinceSynchronization    = 0;
    JumpSeenAtPlaybackTime              = INVALID_TIME;

    Speed                               = 1;
    Direction                           = PlayForward;

    MinimumStreamOffset                 = 0;

    ClockRecoveryInitialized            = false;

//

    return BaseComponentClass_c::Reset();
}


// /////////////////////////////////////////////////////////////////////////
//
//      the register a stream function, creates a new Coordinator context
//

OutputCoordinatorStatus_t   OutputCoordinator_Base_c::RegisterStream(
						PlayerStream_t                    Stream,
						PlayerStreamType_t                StreamType,
						OutputCoordinatorContext_t       *Context )
{
PlayerStatus_t                  Status;
OutputCoordinatorContext_t      NewContext;
unsigned char                   ExternalMapping;
unsigned char                   MasterClock;
bool                            PossibleMaster;
bool                            PossibleAlternateMaster;

    //
    // Obtain a new context
    //

    *Context    = NULL;

    NewContext  = new struct OutputCoordinatorContext_s;
    if( NewContext == NULL )
    {
	report( severity_error, "OutputCoordinator_Base_c::RegisterStream - Failed to obtain a new Coordinator context.\n" );
	return PlayerInsufficientMemory;
    }

    //
    // Initialize the new context
    //

    memset( NewContext, 0x00, sizeof(struct OutputCoordinatorContext_s) );

    NewContext->Next                                                    = NULL;
    NewContext->Stream                                                  = Stream;
    NewContext->StreamType                                              = StreamType;
    NewContext->TimeMappingEstablished                                  = false;
    NewContext->BasedOnMasterMappingVersion                             = 0;

    NewContext->AccumulatedPlaybackTimeJumpsSinceSynchronization        = 0;

    NewContext->OutputRateAdjustmentType				= OutputRateAdjustmentNotDetermined;
    NewContext->OutputRateAdjustmentParameters				= (StreamType == StreamTypeVideo) ? ORAInputFollowingVideo : ORAInputFollowingAudio;

    NewContext->ClockAdjustmentEstablished                              = false;
    NewContext->IntegratingClockDrift                                   = true;
    NewContext->FramesToIntegrateOver                                   = NewContext->OutputRateAdjustmentParameters.ClockDriftMinimumIntegrationFrames;

    NewContext->LastIntegrationWasRestarted                             = false;
    NewContext->IntegrationCount                                        = 0;
    NewContext->LeastSquareFit.Reset();

    NewContext->ManifestorLatency                                       = INVALID_TIME;
    NewContext->StreamOffset                                            = (long long)INVALID_TIME;

    OS_InitializeEvent( &NewContext->AbortPerformEntryIntoDecodeWindowWait );

    //
    // Obtain the appropriate portion of the class list for this stream
    //

    Player->GetClassList( Stream, NULL, NULL, NULL, NULL, &NewContext->Manifestor );
    if( NewContext->Manifestor == NULL )
    {
	report( severity_error, "OutputCoordinator_Base_c::RegisterStream - This implementation does not support no-output timing.\n ");
	delete NewContext;
	return PlayerNotSupported;
    }

    //
    // If this is a video stream, get the output surface descriptor
    //

    if( NewContext->StreamType == StreamTypeVideo )
    {
	Status  = NewContext->Manifestor->GetSurfaceParameters( (void **)(&NewContext->VideoSurfaceDescriptor) );
	if( Status != ManifestorNoError )
	{
	    report( severity_error, "OutputCoordinator_Base_c::RegisterStream - Failed to obtain the output surface descriptor from the manifestor.\n" );
	    delete NewContext;
	    return OutputCoordinatorError;
	}
    }

    //
    // If there is an external time mapping in force, 
    // then we copy it into this context. 
    //

    ExternalMapping     = Player->PolicyValue( Playback, NewContext->Stream, PolicyExternalTimeMapping );
    if( (ExternalMapping == PolicyValueApply) && MasterTimeMappingEstablished )
    {
	NewContext->BaseSystemTime                                     = MasterBaseSystemTime;
	NewContext->BaseNormalizedPlaybackTime                         = MasterBaseNormalizedPlaybackTime;
	NewContext->AccumulatedPlaybackTimeJumpsSinceSynchronization   = 0;
	NewContext->TimeMappingEstablished                             = MasterTimeMappingEstablished;
	NewContext->BasedOnMasterMappingVersion                        = MasterTimeMappingVersion;
    }

    //
    // Is this the master clock - we follow what is specified, but if it is
    // audio/video, and we have only the other (ie video is master on an audio only stream)
    // then we use that other as an alternate master clock.
    //

    OS_LockMutex( &Lock );

    MasterClock                 = Player->PolicyValue( Playback, Stream, PolicyMasterClock );
    PossibleMaster              = ((MasterClock == PolicyValueVideoClockMaster) && (StreamType == StreamTypeVideo)) ||  
				  ((MasterClock == PolicyValueAudioClockMaster) && (StreamType == StreamTypeAudio));
    PossibleAlternateMaster     = (MasterClock != PolicyValueSystemClockMaster);

    if( PossibleMaster && GotAnAlternateMasterClock )
    {
	// If we already had an alternate master that we no longer need we re-initialize it as a plain vanilla clock

	GotAnAlternateMasterClock                               = false;
	AlternateMasterContext->ClockMaster                     = false;
	AlternateMasterContext->IntegratingClockDrift           = false;
	AlternateMasterContext->FramesToIntegrateOver           = NewContext->OutputRateAdjustmentParameters.ClockDriftMinimumIntegrationFrames;
	AlternateMasterContext->IntegrationCount                = 0;
    }

    if( !GotAMasterClock && PossibleMaster )
    {
	// We have found a real master

	GotAMasterClock                                         = true;
	SystemClockAdjustmentEstablished                        = false;
	NewContext->ClockMaster                                 = true;
    }

    if( !GotAMasterClock && !GotAnAlternateMasterClock && PossibleAlternateMaster )
    {
	// Not a real master, but a possible alternate 

	GotAnAlternateMasterClock                               = true;
	AlternateMasterContext                                  = NewContext;
	SystemClockAdjustmentEstablished                        = false;
	NewContext->ClockMaster                                 = true;
    }

    //
    // Fudge this is where I fudge the initial clock values to cope with the duff clocks on a CB101
    //

#if 1
#else
    if( MasterClock == PolicyValueSystemClockMaster )
    {
	report( severity_info, "$$$$$$ Nick friggs the initial ClockAdjustment to match the CB101's duff clock $$$$$$\n" );

	NewContext->ClockAdjustmentEstablished                  = true;
	NewContext->ClockAdjustment                             = Rational_t( 1000000, 999883 );
    }
#endif

    //
    // If this stream turns out not to be the master, then we modify it's 
    // configuration to integrate over the longer period immediately, and 
    // to allow a workthrough of the system clock configuration after the
    // master has first run
    //

#if 0
//Nick forcing us to allow a settle period at startup before we start integrating
    if( !NewContext->ClockMaster )
#endif
    {
	NewContext->IntegratingClockDrift                       = false;
    }

    //
    // Is this the video stream that we will allow to manipulate mappings for AVsync purposes
    //

    if( !GotAVideoStream && (StreamType == StreamTypeVideo) )
    {
	GotAVideoStream                         = true;
	NewContext->AllowedToAdjustMappingBase  = true;
    }

    //
    // Insert the context into the list
    //

    NewContext->Next            = CoordinatedContexts;
    CoordinatedContexts         = NewContext;
    *Context                    = NewContext;

    StreamCount++;

    OS_UnLockMutex( &Lock );

    return OutputCoordinatorNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
//      Function to de-register a stream.
//

OutputCoordinatorStatus_t   OutputCoordinator_Base_c::DeRegisterStream( OutputCoordinatorContext_t        Context )
{
OutputCoordinatorContext_t      *PointerToContext;
OutputCoordinatorContext_t       ContextLoop;

    //
    // Make sure no one is trying to synchronize this stream
    //

    OS_LockMutex( &Lock );

    while( Context->InStartupDelay || Context->InSynchronizeFn )
    {
	OS_UnLockMutex( &Lock );
	report( severity_info, "OutputCoordinator_Base_c::DeRegisterStream - In Synchronize (%d %d)\n", Context->InStartupDelay, Context->InSynchronizeFn );
	OS_SleepMilliSeconds( 5 );
	OS_LockMutex( &Lock );
    }

    //
    // Was this a master clock, if it was remove it, check for 
    // an alternative and allow any established clock values to persist.
    // we do not seek an 'alternative' master as the values obtained from
    // the previous clock should persist.
    //

    if( Context->ClockMaster )
    {
	GotAMasterClock                 = false;
	GotAnAlternateMasterClock       = false;
	AlternateMasterContext          = NULL;

	for( ContextLoop         = CoordinatedContexts;
	     ContextLoop        != NULL;
	     ContextLoop         = ContextLoop->Next )
	    if( (ContextLoop             != Context) &&
		(ContextLoop->StreamType == Context->StreamType) )
	    {
		ContextLoop->ClockMaster        = true;
		GotAMasterClock                 = true;
		break;
	    }
    }

    //
    // Was this the one and only stream allowed to adjust the base system time for avsync purposes
    //

    if( Context->AllowedToAdjustMappingBase )
    {
	GotAVideoStream         = false;
	for( ContextLoop         = CoordinatedContexts;
	     ContextLoop        != NULL;
	     ContextLoop         = ContextLoop->Next )
	    if( (ContextLoop             != Context) &&
		(ContextLoop->StreamType == StreamTypeVideo) )
	    {
		ContextLoop->AllowedToAdjustMappingBase = true;
		GotAVideoStream                         = true;
		break;
	    }
    }

    //
    // Remove the context from the list
    //

    for( PointerToContext        = &CoordinatedContexts;
	 *PointerToContext      != NULL;
	 PointerToContext        = &((*PointerToContext)->Next) )
	if( (*PointerToContext) == Context )
	{
	    OS_TerminateEvent( &Context->AbortPerformEntryIntoDecodeWindowWait );

	    *PointerToContext   = Context->Next;
	    delete Context;

	    //
	    // Reduce stream count
	    //

	    StreamCount--;
	    OS_SetEvent( &SynchronizeMayHaveCompleted );
	    break;
	}

    OS_UnLockMutex( &Lock );

//

    return OutputCoordinatorNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
//      Function to set the speed
//

OutputCoordinatorStatus_t   OutputCoordinator_Base_c::SetPlaybackSpeed( 
							OutputCoordinatorContext_t        Context,
							Rational_t                        Speed,
							PlayDirection_t                   Direction )
{
unsigned long long              Now;
OutputCoordinatorContext_t      ContextLoop;

//

    if( Context != PlaybackContext )
    {
	report( severity_error, "OutputCoordinator_Base_c::SetPlaybackSpeed - This implementation does not support stream specific speeds.\n" );
	return PlayerNotSupported;
    }

    //
    // Need to rebase the base times on a speed change, and release 
    // anyone waiting on entry into a decode window.
    //

    OS_LockMutex( &Lock );

    Now         = OS_GetTimeInMicroSeconds();

    //
    // Now split into one of four possible behaviours
    //

    if( (this->Speed == 0) && (Speed == 0) )
    {
	// -----------------------------------------------------------------------
	// Was Paused - still paused - do nothing
	//

    }
    else if( Speed == 0 )
    {
	//-----------------------------------------------------------------------
	// Enterring a paused state
	//

    }
    else if( this->Speed == 0 )
    {
	//-----------------------------------------------------------------------
	// Exitting a paused state
	//

	OS_UnLockMutex( &Lock );
	ResetTimeMapping( PlaybackContext );
	OS_LockMutex( &Lock );
    }
    else
    {
	//-----------------------------------------------------------------------
	// Normal speed change
	//
	// Rebase the master mapping
	//

	if( MasterTimeMappingEstablished )
	{
	    TranslateSystemTimeToPlayback( PlaybackContext, Now, &MasterBaseNormalizedPlaybackTime );
	    MasterBaseSystemTime        = Now;
	}

	//
	// Rebase the local mappings
	//

	for( ContextLoop     = CoordinatedContexts;
	     ContextLoop    != NULL;
	     ContextLoop     = ContextLoop->Next )
	{
	    if( ContextLoop->TimeMappingEstablished )
	    {
		TranslateSystemTimeToPlayback( ContextLoop, Now, &ContextLoop->BaseNormalizedPlaybackTime );
		ContextLoop->BaseSystemTime             = Now;
		ContextLoop->BaseSystemTimeAdjusted     = true;
	    }

	    OS_SetEvent( &ContextLoop->AbortPerformEntryIntoDecodeWindowWait );
	}
    }

    //
    // Record the new speed and direction
    //

    this->Speed         = Speed;
    this->Direction     = Direction;

    OS_UnLockMutex( &Lock );

//

    return OutputCoordinatorNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
//      Function to reset the time mapping
//

OutputCoordinatorStatus_t   OutputCoordinator_Base_c::ResetTimeMapping( OutputCoordinatorContext_t        Context )
{
PlayerStatus_t                  Status;
OutputCoordinatorContext_t      ContextLoop;
PlayerEventRecord_t             Event;

//
    OS_LockMutex( &Lock );

    //
    // Are we resetting every mapping
    //

    if( Context == PlaybackContext )
    {
	MasterTimeMappingEstablished                            = false;
	AccumulatedPlaybackTimeJumpsSinceSynchronization        = 0;
	JumpSeenAtPlaybackTime                                  = INVALID_TIME;
    }

    //
    // Release anyone waiting to enter a decode window
    //

    for( ContextLoop     = (Context == PlaybackContext) ? CoordinatedContexts : Context;
	 ((Context == PlaybackContext) ? (ContextLoop != NULL) : (ContextLoop == Context));
	 ContextLoop     = ContextLoop->Next )
    {
	ContextLoop->TimeMappingEstablished     = false;

	OS_SetEvent( &ContextLoop->AbortPerformEntryIntoDecodeWindowWait );
    }

    //
    // Raise a player event to signal the mapping loss
    //

    if( (EventTimeMappingReset & EventMask) != 0 )
    {
	Event.Code              = EventTimeMappingReset;
	Event.Playback          = Playback;
	Event.Stream            = (Context == PlaybackContext) ? PlayerAllStreams : Context->Stream;
	Event.PlaybackTime      = TIME_NOT_APPLICABLE;
	Event.UserData          = EventUserData;

	Status                  = Player->SignalEvent( &Event );
	if( Status != PlayerNoError )
	    report( severity_error, "OutputCoordinator_Base_c::ResetTimeMapping - Failed to signal event.\n" );
    }

//

    OS_UnLockMutex( &Lock );

    return OutputCoordinatorNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
//      Set a specific mapping
//


OutputCoordinatorStatus_t   OutputCoordinator_Base_c::EstablishTimeMapping(
							OutputCoordinatorContext_t        Context,
							unsigned long long                NormalizedPlaybackTime,
							unsigned long long                SystemTime )
{
PlayerStatus_t                  Status;
unsigned long long              Now;
OutputCoordinatorContext_t      ContextLoop;
PlayerEventRecord_t             Event;

//

    OS_LockMutex( &Lock );

    //
    // Set the master mapping as specified
    //

    MasterTimeMappingEstablished                        = false;

    Now                                                 = OS_GetTimeInMicroSeconds();
    MasterBaseNormalizedPlaybackTime                    = NormalizedPlaybackTime;
    MasterBaseSystemTime                                = ValidTime(SystemTime) ? SystemTime : Now;

    AccumulatedPlaybackTimeJumpsSinceSynchronization    = 0;
    JumpSeenAtPlaybackTime                              = INVALID_TIME;
    MasterTimeMappingEstablished                        = true;
    MasterTimeMappingVersion++;

    VsyncOffsetIntegrationCount                         = 0;            // Need to re-evaluate the vsync offset - if it is being monitored
    MinimumVsyncOffset                                  = 0x7fffffffffffffffll;

    //
    // Propogate this to the appropriate coordinated contexts
    //

    for( ContextLoop     = (Context == PlaybackContext) ? CoordinatedContexts : Context;
	 ((Context == PlaybackContext) ? (ContextLoop != NULL) : (ContextLoop == Context));
	 ContextLoop     = ContextLoop->Next )
    {
	ContextLoop->BaseSystemTime                                     = MasterBaseSystemTime;
	ContextLoop->BaseNormalizedPlaybackTime                         = MasterBaseNormalizedPlaybackTime;
	ContextLoop->AccumulatedPlaybackTimeJumpsSinceSynchronization   = 0;
	ContextLoop->TimeMappingEstablished                             = true;
	ContextLoop->BasedOnMasterMappingVersion                        = MasterTimeMappingVersion;

	ContextLoop->IntegratingClockDrift                              = false;
	ContextLoop->FramesToIntegrateOver                              = ContextLoop->OutputRateAdjustmentParameters.ClockDriftMinimumIntegrationFrames;
	ContextLoop->IntegrationCount                                   = 0;

	// Ensure no-one is waiting in an out of date decode window.
	OS_SetEvent( &ContextLoop->AbortPerformEntryIntoDecodeWindowWait );
    }

    //
    // All communal activity completed, free the lock.
    //

    OS_UnLockMutex( &Lock );

    //
    // Signal the player event
    //

    if( (EventTimeMappingEstablished & EventMask) != 0 )
    {
	Event.Code                      = EventTimeMappingEstablished;
	Event.Playback                  = Playback;
	Event.Stream                    = (Context == PlaybackContext) ? PlayerAllStreams : Context->Stream;
	Event.PlaybackTime              = MasterBaseNormalizedPlaybackTime;
	Event.UserData                  = EventUserData;
	Event.Value[0].LongLong         = MasterBaseSystemTime;

	Status                          = Player->SignalEvent( &Event );
	if( Status != PlayerNoError )
	    report( severity_error, "OutputCoordinator_Base_c::EstablishTimeMapping - Failed to signal event.\n" );
    }

//

    OS_SetEvent( &SynchronizeMayHaveCompleted );

    return OutputCoordinatorNoError;
}

// /////////////////////////////////////////////////////////////////////////
//
//      Translate a given playback time to a system time
//

OutputCoordinatorStatus_t   OutputCoordinator_Base_c::TranslatePlaybackTimeToSystem(
							OutputCoordinatorContext_t        Context,
							unsigned long long                NormalizedPlaybackTime,
							unsigned long long               *SystemTime )
{
unsigned long long      BaseNormalizedPlaybackTime;
unsigned long long      BaseSystemTime;
unsigned long long      ElapsedNormalizedPlaybackTime;
unsigned long long      ElapsedSystemTime;

//

    if( !MasterTimeMappingEstablished )
	return OutputCoordinatorMappingNotEstablished;

    if( Context != PlaybackContext )
    {
	if( !Context->TimeMappingEstablished )
	    return OutputCoordinatorMappingNotEstablished;

	BaseNormalizedPlaybackTime      = Context->BaseNormalizedPlaybackTime;
	BaseSystemTime                  = Context->BaseSystemTime;
    }
    else
    {
	BaseNormalizedPlaybackTime      = MasterBaseNormalizedPlaybackTime;
	BaseSystemTime                  = MasterBaseSystemTime;
    }

//

    ElapsedNormalizedPlaybackTime       = NormalizedPlaybackTime - BaseNormalizedPlaybackTime;
    ElapsedSystemTime                   = SpeedScale( ElapsedNormalizedPlaybackTime );
    ElapsedSystemTime                   = RoundedLongLongIntegerPart( ElapsedSystemTime / SystemClockAdjustment );

//

    *SystemTime                         = BaseSystemTime + ElapsedSystemTime;

    //
    // Here we rebase if the elapsed times are large
    //

    if( ElapsedSystemTime > REBASE_TIME_TRIGGER_VALUE )
    {
	if( (Context == PlaybackContext) || (MasterBaseSystemTime == Context->BaseSystemTime) )
	{
	    MasterBaseNormalizedPlaybackTime    = NormalizedPlaybackTime;
	    MasterBaseSystemTime                = *SystemTime;      
	}

	if( Context != PlaybackContext )
	{
	    Context->BaseNormalizedPlaybackTime = NormalizedPlaybackTime;
	    Context->BaseSystemTime             = *SystemTime;
	}
    }

//

    return OutputCoordinatorNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
//      Translate a given system time to a playback time
//

OutputCoordinatorStatus_t   OutputCoordinator_Base_c::TranslateSystemTimeToPlayback(
							OutputCoordinatorContext_t        Context,
							unsigned long long                SystemTime,
							unsigned long long               *NormalizedPlaybackTime )
{
unsigned long long      BaseNormalizedPlaybackTime;
unsigned long long      BaseSystemTime;
unsigned long long      ElapsedNormalizedPlaybackTime;
unsigned long long      ElapsedSystemTime;

//

    if( !MasterTimeMappingEstablished )
	return OutputCoordinatorMappingNotEstablished;

    if( Context != PlaybackContext )
    {
	if( !Context->TimeMappingEstablished )
	    return OutputCoordinatorMappingNotEstablished;

	BaseNormalizedPlaybackTime      = Context->BaseNormalizedPlaybackTime;
	BaseSystemTime                  = Context->BaseSystemTime;
    }
    else
    {
	BaseNormalizedPlaybackTime      = MasterBaseNormalizedPlaybackTime;
	BaseSystemTime                  = MasterBaseSystemTime;
    }

//

    ElapsedSystemTime                   = SystemTime - BaseSystemTime;
    ElapsedSystemTime                   = RoundedLongLongIntegerPart( ElapsedSystemTime * SystemClockAdjustment );
    ElapsedNormalizedPlaybackTime       = InverseSpeedScale( ElapsedSystemTime );

//

    *NormalizedPlaybackTime             = BaseNormalizedPlaybackTime + ElapsedNormalizedPlaybackTime;

    return OutputCoordinatorNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
//      Synchronize the streams to establish a time mapping
//

OutputCoordinatorStatus_t   OutputCoordinator_Base_c::SynchronizeStreams(
							OutputCoordinatorContext_t        Context,
							unsigned long long                NormalizedPlaybackTime,
							unsigned long long                NormalizedDecodeTime,
							unsigned long long               *SystemTime )
{
OutputCoordinatorStatus_t       Status;
unsigned char                   ExternalMapping;
bool                            AlternateTimeMappingExists;
bool                            AlternateMappingIsReasonable;
unsigned long long              Now;
unsigned long long              MasterMappingNow;
OutputCoordinatorContext_t      LoopContext;
OutputCoordinatorContext_t      EarliestVideoContext;
OutputCoordinatorContext_t      EarliestContext;
unsigned long long              EarliestStartTime;
unsigned long long              StartTimeJitter;
unsigned long long              StartupDelay;
unsigned int                    WaitCount;
unsigned char                   VideoStartImmediatePolicy;
long long                       StreamOffset;
PlayerEventRecord_t             Event;
unsigned int                    MaxSynchronizeWaits;

    //
    // We do not perform the synchronization if we use an external time mapping
    //

    ExternalMapping     = Player->PolicyValue( Playback, Context->Stream, PolicyExternalTimeMapping );
    if( ExternalMapping == PolicyValueApply )
    {
	report( severity_info, "OutputCoordinator_Base_c::SynchronizeStreams - Function entered when PolicyExternalTimeMapping is set.\n" );

	*SystemTime     = UNSPECIFIED_TIME;
	return OutputCoordinatorNoError;
    }

    //
    // Is there an appropriate time mapping we can switch to.
    //

report( severity_info, "Sync In - %d - %016llx %016llx\n", Context->StreamType, NormalizedPlaybackTime, NormalizedDecodeTime );

    Context->InStartupDelay                     = true;

    OS_LockMutex( &Lock );

    AlternateTimeMappingExists  = MasterTimeMappingEstablished &&
				  (Context->BasedOnMasterMappingVersion != MasterTimeMappingVersion);
    if( AlternateTimeMappingExists )
    {
	Now             = OS_GetTimeInMicroSeconds();
	TranslatePlaybackTimeToSystem( PlaybackContext, NormalizedPlaybackTime, &MasterMappingNow );

	AlternateMappingIsReasonable =  ((MasterMappingNow - Now) > NEGATIVE_REASONABLE_LIMIT) ||
					((MasterMappingNow - Now) < POSITIVE_REASONABLE_LIMIT);

	if( AlternateMappingIsReasonable )
	{
	    Context->AccumulatedPlaybackTimeJumpsSinceSynchronization   = 0;
	    Context->BaseSystemTimeAdjusted                             = true;
	    Context->BaseSystemTime                                     = MasterBaseSystemTime;
	    Context->BaseNormalizedPlaybackTime                         = MasterBaseNormalizedPlaybackTime;
	    Context->TimeMappingEstablished                             = true;
	    Context->BasedOnMasterMappingVersion                        = MasterTimeMappingVersion;
	    Context->StreamOffset                                       = MasterMappingNow - Now;

	    if( Context->StreamOffset < MinimumStreamOffset )
		MinimumStreamOffset                                     = Context->StreamOffset;

	    Status	= TranslatePlaybackTimeToSystem( Context, NormalizedPlaybackTime, SystemTime );
	    Context->InStartupDelay                     		= false;

report( severity_info, "Sync out0 - %d - %016llx %016llx (%6lld)\n", Context->StreamType, MasterBaseNormalizedPlaybackTime, MasterBaseSystemTime, Context->StreamOffset );
	    OS_UnLockMutex( &Lock );
	    return Status;
	}
    }

    //
    // Having reached here, either there is no alternate mapping, or the alternate mapping is
    // not appropriate, in either event there is now no valid master time mapping. We therefore
    // invalidate it before proceding to establish a new one.
    //

    MasterTimeMappingEstablished        = false;
    MinimumStreamOffset                 = 0;
    OS_UnLockMutex( &Lock );

    //
    // Before performing synchronization, we now perform the wait while
    // sufficient data is decoded to allow smooth playback after synchronization.
    // This should simply be PTS - DTS in forwad play, and 0 in reverse play, 
    // we limit the value just in case DTS is invalid, 
    //

    if( Direction == PlayForward )
    {
	StartupDelay    = (NormalizedDecodeTime != UNSPECIFIED_TIME) ? 
			((NormalizedPlaybackTime - NormalizedDecodeTime) / 1000) : DEFAULT_STARTUP_DELAY;

	if(NormalizedPlaybackTime < NormalizedDecodeTime) {
		report( severity_error, "OutputCoordinator_Base_c::SynchronizeStreams - Startup delay calculated to be negative! Bad DTS? (%lldms) (%016llx - %016llx).\n", StartupDelay, NormalizedPlaybackTime, NormalizedDecodeTime );
		StartupDelay    = MAXIMUM_STARTUP_DELAY; //DEFAULT_STARTUP_DELAY;
	}
    }
    else
    {
	StartupDelay    = 0;
    }

    if( StartupDelay > MAXIMUM_STARTUP_DELAY )
    {
	report( severity_error, "OutputCoordinator_Base_c::SynchronizeStreams - Startup delay calculated to be too large (%lldms) (%016llx - %016llx).\n", StartupDelay, NormalizedPlaybackTime, NormalizedDecodeTime );
	StartupDelay        = MAXIMUM_STARTUP_DELAY;
    }

    if( StartupDelay != 0 )
	OS_SleepMilliSeconds( (unsigned int)StartupDelay );

    // Determine Policy for reducing Stream Start Time.
    if (Player->PolicyValue( Playback, Context->Stream, PolicySyncStartImmediate ))
    {
	// Reduce the Wait Time Cycles for the first usage.
	MaxSynchronizeWaits = 1;
	// Disable policy until next startup
	Player->SetPolicy( Playback, PlayerAllStreams, PolicySyncStartImmediate, PolicyValueDisapply);
    } else
	MaxSynchronizeWaits = MAX_SYNCHRONIZE_WAITS;

    //
    // Now we need to add ourselves to the list of synchronizing streams
    //

    OS_LockMutex( &Lock );

    Context->InStartupDelay                     = false;
    Context->InSynchronizeFn                    = true;
    Context->SynchronizingAtPlaybackTime        = NormalizedPlaybackTime;
    StreamsInSynchronize++;

    //
    // Enter the loop where we await synchronization
    //

    WaitCount   = 0;
    while( true )
    {
	//
	// Has someone completed the establishment of the mapping
	//

	if( MasterTimeMappingEstablished )
	{
	    StreamOffset                                                = NormalizedPlaybackTime - MasterBaseNormalizedPlaybackTime - AccumulatedPlaybackTimeJumpsSinceSynchronization;
	    Context->AccumulatedPlaybackTimeJumpsSinceSynchronization   = 0;
	    Context->BaseSystemTimeAdjusted                             = true;
	    Context->BaseSystemTime                                     = MasterBaseSystemTime;
	    Context->BaseNormalizedPlaybackTime                         = MasterBaseNormalizedPlaybackTime + AccumulatedPlaybackTimeJumpsSinceSynchronization;
	    Context->TimeMappingEstablished                             = true;
	    Context->BasedOnMasterMappingVersion                        = MasterTimeMappingVersion;
	    Context->StreamOffset                                       = 0;

	    if( StreamOffset != 0 )
	    {
		if( inrange(StreamOffset, -MAX_SYNCHRONIZATION_WINDOW, MAX_SYNCHRONIZATION_WINDOW) )
		{
		    Context->StreamOffset                               = StreamOffset;
//		    report( severity_info, "OutputCoordinator_Base_c::SynchronizeStreams(%s) - Stream offset by %12lldus\n", StreamType(), StreamOffset );
		}
		else
		{
		    report( severity_error, "OutputCoordinator_Base_c::SynchronizeStreams(%s) - Impossible to synchronize, stream offset by %12lldus\n", StreamType(), StreamOffset );
		    report( severity_info, "\t\t\t\t will anticipate a stream jump, and probably re-enter synchronization shortly.\n" );
		    // Invert the offset so the code looks the same as an actual jump (in the HandlePlaybackTimeDeltas fn).
		    StreamOffset                                                 = -StreamOffset;       
		    AccumulatedPlaybackTimeJumpsSinceSynchronization            += StreamOffset;
		    MasterBaseNormalizedPlaybackTime                            -= StreamOffset;
		    JumpSeenAtPlaybackTime                                       = Context->BaseNormalizedPlaybackTime;
		    Context->BaseNormalizedPlaybackTime                         -= StreamOffset;
		    Context->AccumulatedPlaybackTimeJumpsSinceSynchronization    = AccumulatedPlaybackTimeJumpsSinceSynchronization;
		    Context->BaseSystemTimeAdjusted                              = true;
		}
	    }

	    if( Context->StreamOffset < MinimumStreamOffset )
		MinimumStreamOffset                                              = Context->StreamOffset;

	    Status      = TranslatePlaybackTimeToSystem( Context, NormalizedPlaybackTime, SystemTime );
	    break;
	}

	//
	// Can we do the synchronization
	//

	if( (StreamsInSynchronize == StreamCount) ||
	    (WaitCount            >= MaxSynchronizeWaits) )
	{
	    //
	    // Scan through and find the earliest playback 
	    // time, and the earliest video playback time.
	    //

	    EarliestContext             = NULL;
	    EarliestVideoContext        = NULL;

	    for( LoopContext     = CoordinatedContexts;
		 LoopContext    != NULL;
		 LoopContext     = LoopContext->Next )
		if( LoopContext->InSynchronizeFn )
		{
		    if( (LoopContext->StreamType == StreamTypeVideo) &&
			((EarliestVideoContext == NULL) || 
			 (EarliestVideoContext->SynchronizingAtPlaybackTime > LoopContext->SynchronizingAtPlaybackTime)) )
			EarliestVideoContext    = LoopContext;

		    if( (EarliestContext == NULL) || 
			(EarliestContext->SynchronizingAtPlaybackTime > LoopContext->SynchronizingAtPlaybackTime) )
			EarliestContext         = LoopContext;
		}

	    //
	    // Are we running video start immediate
	    //

	    VideoStartImmediatePolicy   = Player->PolicyValue( Playback, PlayerAllStreams, PolicyVideoStartImmediate );
	    if( (VideoStartImmediatePolicy == PolicyValueApply) &&
		(EarliestVideoContext != NULL) )
		EarliestContext         = EarliestVideoContext;

	    //
	    // Find the earliest start time
	    //

	    EarliestStartTime   = RestartTime();

	    //
	    // If audio is the starter, Jitter start time so 
	    // that first video frame hits a video frame point.
	    //

	    StartTimeJitter             = 0;
	    if( (EarliestVideoContext != NULL) && (EarliestVideoContext != EarliestContext) )
	    {
		StartTimeJitter         = (EarliestVideoContext->SynchronizingAtPlaybackTime - EarliestContext->SynchronizingAtPlaybackTime);
		StartTimeJitter         = StartTimeJitter - (VideoFrameDuration * (StartTimeJitter/VideoFrameDuration));
		StartTimeJitter         = VideoFrameDuration - StartTimeJitter;
	    }

	    //
	    // Establish the mapping
	    //

	    MasterBaseNormalizedPlaybackTime                    = EarliestContext->SynchronizingAtPlaybackTime;
	    MasterBaseSystemTime                                = EarliestStartTime + StartTimeJitter;
	    AccumulatedPlaybackTimeJumpsSinceSynchronization    = 0;
	    JumpSeenAtPlaybackTime                              = INVALID_TIME;
	    MasterTimeMappingEstablished                        = true;
	    MasterTimeMappingVersion++;

	    //
	    // Signal the player event
	    //

	    if( (EventTimeMappingEstablished & EventMask) != 0 )
	    {
		Event.Code                      = EventTimeMappingEstablished;
		Event.Playback                  = Playback;
		Event.Stream                    = PlayerAllStreams;
		Event.PlaybackTime              = MasterBaseNormalizedPlaybackTime;
		Event.UserData                  = EventUserData;
		Event.Value[0].LongLong         = MasterBaseSystemTime;

		Status                          = Player->SignalEvent( &Event );
		if( Status != PlayerNoError )
		    report( severity_error, "OutputCoordinator_Base_c::SynchronizeStreams - Failed to signal event.\n" );
	    }

	    //
	    // Wake everyone and pass back through to pickup the mapping
	    //

	    OS_SetEvent( &SynchronizeMayHaveCompleted );
	}

	//
	// Wait until something happens
	//

	if( !MasterTimeMappingEstablished )
	{
	    OS_ResetEvent( &SynchronizeMayHaveCompleted );

	    OS_UnLockMutex( &Lock );
	    OS_WaitForEvent( &SynchronizeMayHaveCompleted, SYNCHRONIZE_WAIT );
	    OS_LockMutex( &Lock );

	    WaitCount++;
	}
    }

//

    Context->InSynchronizeFn    = false;
    StreamsInSynchronize--;
    OS_UnLockMutex( &Lock );

//
report( severity_info, "Sync out1 - %d - %016llx %016llx - %016llx (%6lld)\n", Context->StreamType, NormalizedPlaybackTime, *SystemTime, OS_GetTimeInMicroSeconds(), Context->StreamOffset );

    return Status;
}


// /////////////////////////////////////////////////////////////////////////
//
//      The function to await entry into the decode window, if the decode 
//      time is invalid, or if no time mapping has yet been established, 
//      we return immediately.
//

OutputCoordinatorStatus_t   OutputCoordinator_Base_c::PerformEntryIntoDecodeWindowWait(
						OutputCoordinatorContext_t        Context,
						unsigned long long                NormalizedDecodeTime,
						unsigned long long                DecodeWindowPorch,
						unsigned long long                MaximumAllowedSleepTime )
{
OutputCoordinatorStatus_t         Status;
OS_Status_t                       OSStatus;
unsigned long long                SystemTime;
unsigned long long                Now;
unsigned long long                SleepTime;
unsigned long long                MaximumSleepTime;

    //
    // Reset the abort flag
    //

    OS_ResetEvent( &Context->AbortPerformEntryIntoDecodeWindowWait );

    //
    // Get the translation of the decode time, and apply the porch to find the window start
    // If we do not have a mapping here we can use the master mapping.
    //

    Status      = TranslatePlaybackTimeToSystem( (Context->TimeMappingEstablished ? Context : PlaybackContext), NormalizedDecodeTime, &SystemTime );
    if( Status == OutputCoordinatorMappingNotEstablished )
	return OutputCoordinatorNoError;

    SystemTime  -= DecodeWindowPorch;

    //
    // Shall we sleep
    //

    Now         = OS_GetTimeInMicroSeconds();

    if( Now >= SystemTime )
	return OutputCoordinatorNoError;

    //
    // How long shall we sleep
    //

    SleepTime                   = (SystemTime - Now);
    MaximumSleepTime            = SpeedScale( MaximumAllowedSleepTime );

    if( SleepTime > MaximumSleepTime )
    {
//      Nick Says -> during startup this occurs several times.
//      report( severity_error, "OutputCoordinator_Base_c::PerformEntryIntoDecodeWindowWait - Sleep period too long (%lluus).\n", SleepTime );
	SleepTime               = MaximumSleepTime;
    }

    //
    // Sleep
    //

    OSStatus    = OS_WaitForEvent( &Context->AbortPerformEntryIntoDecodeWindowWait, (unsigned int)(SleepTime / 1000) );

    return (OSStatus == OS_NO_ERROR) ? OutputCoordinatorAbandonedWait : OutputCoordinatorNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
//      The function to handle deltas in the playback time versus the
//      expected playback time of a frame.
//
//      This function splits into two halves, first checking for any 
//      large delta and performing a rebase of the time mapping when one 
//      occurs.
//
//      A second half checking that if we did a rebase, then if this stream
//      has not matched the rebase then we invalidate mapping to force a 
//      complete re-synchronization.
//

OutputCoordinatorStatus_t   OutputCoordinator_Base_c::HandlePlaybackTimeDeltas(
							OutputCoordinatorContext_t        Context,
							bool                              KnownJump,
							unsigned long long                ExpectedPlaybackTime,
							unsigned long long                ActualPlaybackTime )
{
long long       DeltaPlaybackTime;
long long       JumpError;
unsigned char   ExternalMapping;
unsigned char   Threshold;
unsigned char   Symmetric;
long long       PlaybackTimeForwardJumpThreshold;               // Values used in handling jumps in PTSs
long long       PlaybackTimeReverseJumpThreshold;

    //
    // Check that this call relates to a particular stream
    //

    if( Context == PlaybackContext )
    {
	report( severity_error, "OutputCoordinator_Base_c::HandlePlaybackTimeDeltas - Deltas only handled for specific streams.\n" );
	return OutputCoordinatorError;
    }

    //
    // Check if this is relevent
    //

    if( !MasterTimeMappingEstablished )
	return OutputCoordinatorNoError;

    //
    // What are the current value of the jump detector thresholds
    //

    Threshold                           = Player->PolicyValue( Playback, Context->Stream, PolicyPtsForwardJumpDetectionThreshold );
    Threshold                           = min( Threshold, 43 );                 // 43 = ln (0x7fff...) / ln 2 - or the maximum time we can hold 
    PlaybackTimeForwardJumpThreshold    = 1000000 * (1ULL << Threshold);

    Symmetric                           = Player->PolicyValue( Playback, Context->Stream, PolicySymmetricJumpDetection );
    PlaybackTimeReverseJumpThreshold    = (Symmetric == PolicyValueApply) ? PlaybackTimeForwardJumpThreshold : PLAYBACK_TIME_JUMP_ERROR;

    //
    // Now evaluate the delta and handle any jump in the playback time
    //

    DeltaPlaybackTime   = (Direction == PlayBackward) ?
				(long long)(ExpectedPlaybackTime - ActualPlaybackTime) :
				(long long)(ActualPlaybackTime - ExpectedPlaybackTime);

    if( !inrange(DeltaPlaybackTime, -PlaybackTimeReverseJumpThreshold, PlaybackTimeForwardJumpThreshold) || KnownJump )
    {
report( severity_info, "Spotted a delta(%s) %12lld\n", StreamType(), DeltaPlaybackTime );
	OS_LockMutex( &Lock );

	//
	// Report an error, if we spot a delta on 
	// externally timed data, and ignore the delta.
	//

	ExternalMapping = Player->PolicyValue( Playback, Context->Stream, PolicyExternalTimeMapping );
	if( ExternalMapping == PolicyValueApply )
	{
	    report( severity_error, "OutputCoordinator_Base_c::HandlePlaybackTimeDeltas(%s) - Spotted large playback delta(%lldus) when PolicyExternalTimeMapping is set.\n", StreamType(), DeltaPlaybackTime );
	}

	//
	// Is this part of a cascading change
	//

	else if( AccumulatedPlaybackTimeJumpsSinceSynchronization != Context->AccumulatedPlaybackTimeJumpsSinceSynchronization )
	{
	    //
	    // Is this a complete jump to the cascaded change, or just a partial 
	    // (ie a rapid sequence of jumps close together).
	    //

report( severity_info, "Cascading a Jump(%s)\n", StreamType() );

	    JumpError   = AccumulatedPlaybackTimeJumpsSinceSynchronization - (Context->AccumulatedPlaybackTimeJumpsSinceSynchronization - DeltaPlaybackTime);
	    if( !inrange(JumpError, -PLAYBACK_TIME_JUMP_ERROR, PlaybackTimeForwardJumpThreshold) )
	    {
		//Partial catchup

		Context->AccumulatedPlaybackTimeJumpsSinceSynchronization       -= DeltaPlaybackTime;
		Context->BaseNormalizedPlaybackTime                             += DeltaPlaybackTime;
		Context->BaseSystemTimeAdjusted                                  = true;
	    }
	    else
	    {
		//Complete catchup

		Context->BaseNormalizedPlaybackTime                             += Context->AccumulatedPlaybackTimeJumpsSinceSynchronization;
		Context->BaseNormalizedPlaybackTime                             -= AccumulatedPlaybackTimeJumpsSinceSynchronization;
		Context->AccumulatedPlaybackTimeJumpsSinceSynchronization        = AccumulatedPlaybackTimeJumpsSinceSynchronization;
		Context->BaseSystemTimeAdjusted                                  = true;
	    }
	}

	else if( ValidTime(JumpSeenAtPlaybackTime) && 
	    inrange(DeltaPlaybackTime, 0, 2000000) &&
	    inrange(ExpectedPlaybackTime - JumpSeenAtPlaybackTime, 0, 10000000) )
	{
	    report( severity_error, "OutputCoordinator_Base_c::HandlePlaybackTimeDeltas(%s) - Multiple jumps over short period.\n", StreamType() );
	    OS_UnLockMutex( &Lock );
	    ResetTimeMapping( PlaybackContext );
	    OS_LockMutex( &Lock );
	}

	//
	// Not part of a cascading change yet, we initiate a cascading change
	//
	else
	{
report( severity_info, "Initiating a Jump(%s)\n", StreamType() );
	    AccumulatedPlaybackTimeJumpsSinceSynchronization                    -= DeltaPlaybackTime;
	    MasterBaseNormalizedPlaybackTime                                    += DeltaPlaybackTime;
	    JumpSeenAtPlaybackTime                                               = ExpectedPlaybackTime;
	    Context->BaseNormalizedPlaybackTime                                 += DeltaPlaybackTime;
	    Context->AccumulatedPlaybackTimeJumpsSinceSynchronization            = AccumulatedPlaybackTimeJumpsSinceSynchronization;
	    Context->BaseSystemTimeAdjusted                                      = true;        
	}

	OS_UnLockMutex( &Lock );
    }

    //
    // Here we check that if there is an ongoing cascading change that we are not part of,
    // then we have not reached a point when we really should have been part of it 
    //

    if( ValidTime(JumpSeenAtPlaybackTime) &&
	(AccumulatedPlaybackTimeJumpsSinceSynchronization != Context->AccumulatedPlaybackTimeJumpsSinceSynchronization) )
    {
	DeltaPlaybackTime       = (long long)(ActualPlaybackTime - JumpSeenAtPlaybackTime);
	if( !inrange(DeltaPlaybackTime, -PlaybackTimeForwardJumpThreshold, OTHER_STREAMS_MUST_FOLLOW_JUMP_BY) )
	{
	    report( severity_error, "OutputCoordinator_Base_c::HandlePlaybackTimeDeltas(%s) - Stream failed to match previous jump in playback times\n", StreamType() );
	    ResetTimeMapping( PlaybackContext );
	}
    }

    //
    // Since it is convenient in here, we also check that we match the master mapping
    // since we may have missed some change to the master mapping
    //

    if( Context->BasedOnMasterMappingVersion != MasterTimeMappingVersion )
    {
	DeltaPlaybackTime       = (long long)(ActualPlaybackTime - MasterBaseNormalizedPlaybackTime);
	if( !inrange(DeltaPlaybackTime, -PlaybackTimeForwardJumpThreshold, PlaybackTimeForwardJumpThreshold) )
	{
	    report( severity_error, "OutputCoordinator_Base_c::HandlePlaybackTimeDeltas(%s) - Stream failed to match previous master mapping change\n", StreamType() );
	    ResetTimeMapping( PlaybackContext );
	}
    }

//

    return OutputCoordinatorNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
//      The function to calculate the clock rate adjustment values, 
//      system clock, and for any subsiduary clocks.
//

OutputCoordinatorStatus_t   OutputCoordinator_Base_c::CalculateOutputRateAdjustment(
							OutputCoordinatorContext_t        Context,
							unsigned long long                ExpectedDuration,
							unsigned long long                ActualDuration,
							long long                         CurrentError,
							Rational_t                       *OutputRateAdjustment,
							Rational_t                       *ParamSystemClockAdjustment )
{
unsigned char           ExternalMapping;
long long               Difference;
Rational_t              AdjustmentMultiplier;
unsigned int            MaximumClockRateAdjustmentPpm;
Rational_t              MaximumClockRateMultiplier;
Rational_t              MinimumClockRateMultiplier;
long long               ClampChange;
long long               AnticipatedDriftCorrection;
unsigned long long      AnticipatedDriftCorrectionPeriod;
Rational_t              AnticipatedDriftAdjustment;

    //
    // If called from a playback context this function just returns the current 
    // system clock adjustment.
    //

    Context->CurrentErrorHistory[3]     = Context->CurrentErrorHistory[2];
    Context->CurrentErrorHistory[2]     = Context->CurrentErrorHistory[1];
    Context->CurrentErrorHistory[1]     = Context->CurrentErrorHistory[0];
    Context->CurrentErrorHistory[0]     = CurrentError;

    *OutputRateAdjustment       = 1;
    *ParamSystemClockAdjustment = SystemClockAdjustment;

    if( Context == PlaybackContext )
	return OutputCoordinatorNoError;

    //
    // Are we using the correct set of parameters
    //

    if( Context->OutputRateAdjustmentType == OutputRateAdjustmentNotDetermined )
    {
	ExternalMapping				= Player->PolicyValue( Playback, Context->Stream, PolicyExternalTimeMapping );

	Context->OutputRateAdjustmentParameters	= (ExternalMapping == PolicyValueApply) ?
							((Context->StreamType == StreamTypeVideo) ? ORAInputFollowingVideo : ORAInputFollowingAudio) :
							((Context->StreamType == StreamTypeVideo) ? ORAOutputDrivenVideo   : ORAOutputDrivenAudio);

	Context->OutputRateAdjustmentType	= (ExternalMapping == PolicyValueApply) ? OutputRateAdjustmentInputFollowing : OutputRateAdjustmentOutputDriven;

	Context->IntegratingClockDrift		= Context->ClockMaster && (ExternalMapping == PolicyValueApply);
	Context->FramesToIntegrateOver 		= Context->OutputRateAdjustmentParameters.ClockDriftMinimumIntegrationFrames;
    }

    //
    // Setup the default output rate adjustment
    // and return if we aren't master and the master 
    // has not baselined yet.
    //

    if( !Context->ClockMaster )
    {
	if( !SystemClockAdjustmentEstablished )
	    return OutputCoordinatorNoError;

	if( Context->ClockAdjustmentEstablished )
	    *OutputRateAdjustment       = Context->ClockAdjustment;
    }

    //
    // Calculate the difference, if too large then return.
    //

    Difference  = (int)((long long)ExpectedDuration - (long long)ActualDuration);

#if 0
//
// Collect and output the actual recorded differences
//
if( Context->IntegratingClockDrift && (Context->StreamType == StreamTypeAudio) )
{
#define COUNT 8192
static unsigned int C = 0;
static unsigned int D[COUNT];

    D[C++] = Difference;

    if( C==COUNT )
    {
	for( C=0; C<COUNT; C+=8 )
	    report( severity_info, "%4d -   %6d  %6d  %6d  %6d  %6d  %6d  %6d  %6d\n", C,
				D[C+0], D[C+1], D[C+2], D[C+3], D[C+4], D[C+5], D[C+6], D[C+7] );

	report( severity_fatal, "Thats all folks\n" );
    }  
}
#endif

    //
    // Now perform the integration or ignore 
    //

    Context->IntegrationCount++;

    if( Context->IntegratingClockDrift )
    {
	Context->LeastSquareFit.Add( ActualDuration, ExpectedDuration );

	if( inrange(Difference, -Context->OutputRateAdjustmentParameters.MaximumJitterDifference, Context->OutputRateAdjustmentParameters.MaximumJitterDifference) &&
	    (Context->IntegrationCount >= Context->FramesToIntegrateOver) )
	{
	    //
	    // We are going to calculate a new adjustment, 
	    // now would be a good time to read the current 
	    // policy and calculate the clamping values.
	    //

	    MaximumClockRateAdjustmentPpm       = 1 << Player->PolicyValue( Playback, Context->Stream, PolicyClockPullingLimit2ToTheNPartsPerMillion );
	    MaximumClockRateMultiplier          = Rational_t( 1000000 + MaximumClockRateAdjustmentPpm, 1000000 );
	    MinimumClockRateMultiplier          = Rational_t( 1000000 - MaximumClockRateAdjustmentPpm, 1000000 );

	    //
	    // Calculate a new adjustment multiplier, and clamp it
	    // to prevent too rapid adjustment. The clamp range is 
	    // driven by a maximum of 16ppm, further restricted as 
	    // the sample size grows.
	    //

	    AdjustmentMultiplier                = Context->LeastSquareFit.Gradient();
#if 0
	    report( severity_info, "AAA 0 (%d) Clock adjustment %4d %d.%09d %d.%09d - %06lld\n", Context->StreamType,
			Context->IntegrationCount,
			AdjustmentMultiplier.IntegerPart(), AdjustmentMultiplier.RemainderDecimal(9),
			SystemClockAdjustment.IntegerPart(), SystemClockAdjustment.RemainderDecimal(9),
			CurrentError );
#endif

	    ClampChange                         = (Context->OutputRateAdjustmentParameters.ClockDriftMaximumIntegrationFrames / Context->FramesToIntegrateOver);
	    ClampChange                         = min( 128, 4*(ClampChange * ClampChange) );
	    Clamp( AdjustmentMultiplier, (1 - Rational_t(ClampChange, 1000000)), (1 + Rational_t(ClampChange, 1000000)) );

	    //
	    // Now apply this to the clock adjustment
	    //

	    Context->ClockAdjustment            		= Context->ClockAdjustmentEstablished ? 
									(Context->ClockAdjustment * AdjustmentMultiplier) : AdjustmentMultiplier;

	    Context->ClockAdjustmentEstablished 		= true;

	    //
	    // Now we calculate a long term drift corrector, clamp it,
	    // and re-calculate its anticipated affect after clamping.
	    // We then add the clamped adjustment to the main clock adjustment
	    // and finally clamp it.
	    //

	    if( Context->ClockMaster )
	    {
	    Rational_t	SysChange;
	    long long	Jerk;

		SysChange	= (1 / Context->ClockAdjustment) - SystemClockAdjustment;
		Jerk	= LongLongIntegerPart(SysChange * (OS_GetTimeInMicroSeconds() - MasterBaseSystemTime)) / 2;
//		report( severity_info, "Jerk - %lld\n", Jerk );

		MasterBaseSystemTime	-= Jerk;
		CurrentError		-= Jerk;
	    }

	    AnticipatedDriftCorrection		    = 0;
	    if( Context->FramesToIntegrateOver >= Context->OutputRateAdjustmentParameters.IntegrationThresholdForDriftCorrection )
	    {
		AnticipatedDriftCorrection          = -CurrentError/2;
		AnticipatedDriftCorrectionPeriod    = (Context->FramesToIntegrateOver < Context->OutputRateAdjustmentParameters.ClockDriftMaximumIntegrationFrames) ? 
									(2 * Context->LeastSquareFit.Y()) :
									Context->LeastSquareFit.Y();
		AnticipatedDriftAdjustment          = Rational_t( AnticipatedDriftCorrection, AnticipatedDriftCorrectionPeriod );

		Clamp( AnticipatedDriftAdjustment, Rational_t( -4, 1000000 ), Rational_t( 4, 1000000 ) );
		AnticipatedDriftCorrection          = IntegerPart( AnticipatedDriftCorrectionPeriod * AnticipatedDriftAdjustment );

		Context->ClockAdjustment           += AnticipatedDriftAdjustment;
	    }

//

	    Clamp( Context->ClockAdjustment, MinimumClockRateMultiplier, MaximumClockRateMultiplier );

#if 0
//if( Context->StreamType == 1 )
    {
    Rational_t  Tmp;

	    Tmp = Context->LeastSquareFit.Gradient();

	    report( severity_info, "RateAdjuster (%d) Clock adjustment %d.%09d (%d.%09d) (%5d) %4lld (%5lld %5lld %5lld %5lld)\n", Context->StreamType,
			Context->ClockAdjustment.IntegerPart(), Context->ClockAdjustment.RemainderDecimal(9),
			Tmp.IntegerPart(), Tmp.RemainderDecimal(9), 
			Context->FramesToIntegrateOver, AnticipatedDriftCorrection,
			Context->CurrentErrorHistory[0], Context->CurrentErrorHistory[1], Context->CurrentErrorHistory[2], Context->CurrentErrorHistory[3] );
    }
#endif

	    //
	    // If we are the master then transfer this to the system clock adjustment
	    // but inverted, if we needed to speed up, we do this by slowing the system clock
	    // if we needed to slow down, we do this by speeding up the system clock.
	    //

	    if( Context->ClockMaster )
	    {
		SystemClockAdjustment           = 1 / Context->ClockAdjustment;
		SystemClockAdjustmentEstablished= true;
		*ParamSystemClockAdjustment     = SystemClockAdjustment;
	    }

	    //
	    // In the case where we are not the master recalculate the return value.
	    //

	    if( !Context->ClockMaster && SystemClockAdjustmentEstablished )
		*OutputRateAdjustment   = Context->ClockAdjustment;

	    //
	    // Finally prepare for the ignoring session.
	    //

	    Context->LastIntegrationWasRestarted = false;
	    Context->IntegratingClockDrift       = false;

	    if( Context->FramesToIntegrateOver < Context->OutputRateAdjustmentParameters.ClockDriftMaximumIntegrationFrames )
		Context->FramesToIntegrateOver  *= 2;

	    Context->IntegrationCount           = 0;
	}
    }
    else if( Context->IntegrationCount >= Context->OutputRateAdjustmentParameters.ClockDriftIgnoreBetweenIntegrations )
    {
	//
	// Prepare to integrate over the next set of frames
	//

	Context->IntegratingClockDrift  = true;
	Context->IntegrationCount       = 0;
	Context->LeastSquareFit.Reset();
    }

    //
    // Finally return a good status
    //

    return OutputCoordinatorNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
//      The function to restart the output rate adjustment
//

OutputCoordinatorStatus_t   OutputCoordinator_Base_c::RestartOutputRateIntegration( OutputCoordinatorContext_t  Context )
{
    //
    // Do we wish to stick with the current integration period
    //

    if( Context->LastIntegrationWasRestarted )
	Context->FramesToIntegrateOver  = max( (Context->FramesToIntegrateOver/2), Context->OutputRateAdjustmentParameters.ClockDriftMinimumIntegrationFrames );

//

    Context->IntegratingClockDrift              = false;
    Context->IntegrationCount                   = 0;

//

    Context->LastIntegrationWasRestarted        = true;

//

    return OutputCoordinatorNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
//      The function to adjust the base system time
//

OutputCoordinatorStatus_t   OutputCoordinator_Base_c::AdjustMappingBase(
							OutputCoordinatorContext_t        Context,
							long long                         Adjustment )
{
OutputCoordinatorContext_t      ContextLoop;
unsigned char                   ExternalMapping;

    //
    // If we have an externally specified time mapping we cannot perform the adjusment, based on a particular stream
    //

    if( Context != PlaybackContext )
    {
	ExternalMapping = Player->PolicyValue( Playback, Context->Stream, PolicyExternalTimeMapping );
	if( ExternalMapping == PolicyValueApply )
	{
	    report( severity_info, "\t\tMapping base not adjusted - PolicyExternalTimeMapping in effect.\n" );
	    return OutputCoordinatorNoError;
	}
    }

    //
    // Check that adjustment is allowed, in practice only a 
    // video stream will ever desire to adjust the base, and 
    // if there is more than one synchronized video stream, one
    // would hope that they will agree on where the vsync is 
    // (IE if stream A has a frame at PTS X, the stream B 
    //  should not have a frame at PTS X + 1/2 a vsync period).
    // If two video streams do disagree on the positioning of 
    // the vsync then only the first one seen should allow
    // adjustment of mapping bases, the other will be ignored.
    // The report below is to let us know if a problem occurs,
    // in principle it can be removed.
    // PS I mean two video streams in one synchronized playback 
    // above.
    //

    if( (Context != PlaybackContext) && !Context->AllowedToAdjustMappingBase )
    {
	report( severity_info, "OutputCoordinator_Base_c::AdjustMappingBase - Adjustment ignored, caller not allowed to adjust.\n" );
	return OutputCoordinatorNoError;
    }

    //
    // Perform adjustment
    //

    OS_LockMutex( &Lock );

    if( MasterTimeMappingEstablished )
	MasterBaseSystemTime            += Adjustment;

    for( ContextLoop     = CoordinatedContexts;
	 ContextLoop    != NULL;
	 ContextLoop     = ContextLoop->Next )
    {
	if( ContextLoop->TimeMappingEstablished )
	{
	    ContextLoop->BaseSystemTime         += Adjustment;
	    ContextLoop->BaseSystemTimeAdjusted  = (Context != ContextLoop);            // Tell everyone else it has been adjusted
	}
    }

    OS_UnLockMutex( &Lock );

//

    return OutputCoordinatorNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
//      The function to check if the base system time has been adjusted
//

OutputCoordinatorStatus_t   OutputCoordinator_Base_c::MappingBaseAdjustmentApplied( 
							OutputCoordinatorContext_t        Context )
{
OutputCoordinatorStatus_t       Status;

//

    Status      = OutputCoordinatorNoError;

    if( Context->BaseSystemTimeAdjusted )
    {
	Context->BaseSystemTimeAdjusted = false;
	Status                          = OutputCoordinatorMappingAdjusted;
    }

//

    return Status;
}


// /////////////////////////////////////////////////////////////////////////
//
//      The function to return the delay between the first frame of this 
//      stream, and the first frame from any stream in this playback.
//

OutputCoordinatorStatus_t   OutputCoordinator_Base_c::GetStreamStartDelay(
							OutputCoordinatorContext_t        Context,
							unsigned long long               *Delay )
{
    if( Context->StreamOffset == (long long)INVALID_TIME )
	return OutputCoordinatorMappingNotEstablished;

    *Delay      = (unsigned long long)(Context->StreamOffset - MinimumStreamOffset);
    return OutputCoordinatorNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
//      This function monitors the offset between VSYNCs and the output
//      times of frames.
//

OutputCoordinatorStatus_t   OutputCoordinator_Base_c::MonitorVsyncOffset(
							OutputCoordinatorContext_t        Context, 
							unsigned long long                RequestedOutputTime, 
							unsigned long long                ActualOutputTime )
{
PlayerStatus_t                  Status;
PlayerEventRecord_t             Event;
long long int                   CorrectedOffset;
long long                       VideoFrameDuration;
unsigned char                   VsyncLocked;

//

    if( VsyncOffsetIntegrationCount >= (2 * INTEGRATION_COUNT_FOR_VSYNC_OFFSET) )
	return OutputCoordinatorNoError;

//


    VsyncOffsetIntegrationCount++;

    if( inrange(VsyncOffsetIntegrationCount,INTEGRATION_COUNT_FOR_VSYNC_OFFSET,(2 * INTEGRATION_COUNT_FOR_VSYNC_OFFSET)) )
    {
	if( (long long)(ActualOutputTime - RequestedOutputTime) < MinimumVsyncOffset )
	    MinimumVsyncOffset  = (long long)(ActualOutputTime - RequestedOutputTime);

	if( VsyncOffsetIntegrationCount == (2 * INTEGRATION_COUNT_FOR_VSYNC_OFFSET) )
	{
	    VsyncLocked = Player->PolicyValue( Playback, Context->Stream, PolicyExternalTimeMappingVsyncLocked );
	    if( VsyncLocked != PolicyValueApply )
	    {
		//
		// Not locked so adjust the mapping, and Normalize offset to 0..frame period
		//

		Context->Manifestor->GetSurfaceParameters( (void **)(&Context->VideoSurfaceDescriptor) );

		VideoFrameDuration      = RoundedLongLongIntegerPart( 1000000 / Context->VideoSurfaceDescriptor->FrameRate );
		if( !Context->VideoSurfaceDescriptor->Progressive )
		    VideoFrameDuration  = 2 * VideoFrameDuration;

		CorrectedOffset          = MinimumVsyncOffset;
		if( MinimumVsyncOffset < 0 )
		    CorrectedOffset     += (((-MinimumVsyncOffset) + VideoFrameDuration - 1) / VideoFrameDuration) * VideoFrameDuration;
		else if( MinimumVsyncOffset >= VideoFrameDuration )
		    CorrectedOffset     -= (MinimumVsyncOffset  / VideoFrameDuration) * VideoFrameDuration; 

		report( severity_info, "Vsync Offset = %lldus (%lldus)\n", CorrectedOffset, MinimumVsyncOffset );
		AdjustMappingBase( PlaybackContext,  CorrectedOffset );
	    }
	    else
	    {
		//
		// Vsync locked, so just report, and signal zero
		//

		report( severity_info, "Vsync Offset = %lld, as vsync is locked we will not adjust the mapping.\n" );
		CorrectedOffset         = 0;
	    }

	    //
	    // Raise a player event to signal the measurement of vsync offset
	    //

	    Event.Code                  = EventVsyncOffsetMeasured;
	    Event.Playback              = Playback;
	    Event.Stream                = Context->Stream;
	    Event.PlaybackTime          = TIME_NOT_APPLICABLE;
	    Event.UserData              = EventUserData;
	    Event.Value[0].LongLong     = (unsigned long long)CorrectedOffset;

	    Status                      = Player->SignalEvent( &Event );
	    if( Status != PlayerNoError )
		report( severity_error, "OutputCoordinator_Base_c::MonitorVsyncOffset - Failed to signal event.\n" );

	    return OutputCoordinatorNoError;
	}
    }

    return OutputCoordinatorMappingNotEstablished;
}


// /////////////////////////////////////////////////////////////////////////
//
//      Private - This function trawls the contexts and deduces the next
//      system time at which we can start playing.
//

unsigned long long   OutputCoordinator_Base_c::RestartTime( void )
{
unsigned long long                ManifestTime;
unsigned long long                LatestVideoManifestTime;
unsigned long long                LatestManifestTime;
OutputCoordinatorContext_t        LoopContext;

//

    VideoFrameDuration          = 1;
    LatestVideoManifestTime     = OS_GetTimeInMicroSeconds();
    LatestManifestTime          = LatestVideoManifestTime;

    for( LoopContext     = CoordinatedContexts;
	 LoopContext    != NULL;
	 LoopContext     = LoopContext->Next )
    {
	//
	// We have found a video stream, calculate the frame Duration
	//

	if( (LoopContext->StreamType == StreamTypeVideo) &&
	    (VideoFrameDuration      == 1) )
	{
	    LoopContext->Manifestor->GetSurfaceParameters( (void **)(&LoopContext->VideoSurfaceDescriptor) );

	    VideoFrameDuration  = RoundedLongLongIntegerPart( 1000000 / LoopContext->VideoSurfaceDescriptor->FrameRate );
	    if( !LoopContext->VideoSurfaceDescriptor->Progressive )
		    VideoFrameDuration  = 2 * VideoFrameDuration;
	}

	//
	// Get the manifest time - if this is the first time we have done this,
	// record the manifestor latency in the structure.
	//

	LoopContext->Manifestor->GetNextQueuedManifestationTime( &ManifestTime );
	if( LoopContext->ManifestorLatency == INVALID_TIME )
	    LoopContext->ManifestorLatency      = ManifestTime - OS_GetTimeInMicroSeconds();

	if( LoopContext->StreamType == StreamTypeVideo )
	    LatestVideoManifestTime     = max( ManifestTime, LatestVideoManifestTime );

	LatestManifestTime              = max( ManifestTime, LatestManifestTime );

	//
	// Just to check
	//

	if( (ManifestTime - OS_GetTimeInMicroSeconds()) > 1000000ull )
	    report( severity_info, "OutputCoordinator_Base_c::RestartTime - Long latency %12lldus for stream type %d\n", (ManifestTime - OS_GetTimeInMicroSeconds()), LoopContext->StreamType );
    }

    //
    // Now round up the latest time to the nearest video frame point
    //

    if( LatestManifestTime != LatestVideoManifestTime )
	LatestManifestTime      = LatestVideoManifestTime + (((LatestManifestTime - LatestVideoManifestTime + VideoFrameDuration - 1)/ VideoFrameDuration) * VideoFrameDuration);

//

    return LatestManifestTime;
}


// /////////////////////////////////////////////////////////////////////////
//
//      The function to initialize clock recovery when we have a system
//      clock master, driven by an external broadcast source such as 
//      a satelite.
//      We will allow clock recovery when system clock is not master, 
//      but we will not adjust the system clock.
//

OutputCoordinatorStatus_t   OutputCoordinator_Base_c::ClockRecoveryInitialize( PlayerTimeFormat_t       SourceTimeFormat )
{
unsigned char   MasterClock;

    //
    // During clock recovery initialization, we assume that the local clock is 1:1 with the source clock
    //

    ClockRecoverySourceTimeFormat       = SourceTimeFormat;
    ClockRecoveryLastPts                = INVALID_TIME;         // Only used if PTS source time format
    ClockRecoveryPtsBaseLine            = 0;

    ClockRecoveryBaseSourceClock        = INVALID_TIME;
    ClockRecoveryBaseLocalClock         = INVALID_TIME;

    ClockRecoveryEstablishedGradient    = 0;
    ClockRecoveryEstablishedBaseSource  = INVALID_TIME;
    ClockRecoveryEstablishedBaseLocal   = INVALID_TIME;

    ClockRecoveryInitialized            = true;

    ClockRecoveryIntegrationTime        = CLOCK_RECOVERY_MINIMUM_INTEGRATION_TIME;

    //
    // We only reset the system clock here if the system clock is master
    //

    MasterClock                 = Player->PolicyValue( Playback, PlayerAllStreams, PolicyMasterClock );
    if( MasterClock != PolicyValueSystemClockMaster )
    {
	report( severity_error, "OutputCoordinator_Base_c::ClockRecoveryInitialize - Performing clock recovery when system clock is NOT master.\n" );
	report( severity_error, "                                                    clock recovery provided for application use only.\n" );
	return OutputCoordinatorNoError;
    }

    SystemClockAdjustmentEstablished    = true;
    SystemClockAdjustment               = 1;

//

    return OutputCoordinatorNoError;
}

// /////////////////////////////////////////////////////////////////////////
//
//      The function to handle clock recovery when we have a system 
//      clock master, driven by an external broadcast source such as 
//      a satelite.
//

OutputCoordinatorStatus_t   OutputCoordinator_Base_c::ClockRecoveryDataPoint(
							unsigned long long                SourceTime,
							unsigned long long                LocalTime )
{
unsigned long long      NormalizedSourceTime;
unsigned char           MasterClock;

    //
    // Have we been initialized
    //

    if( !ClockRecoveryInitialized )
    {
	report( severity_error, "OutputCoordinator_Base_c::ClockRecoveryDataPoint - Clock recovery not initialized.\n" );
	return OutputCoordinatorError;
    }

    //
    // Before we do anything else, convert the source time to something we can use
    //

    switch( ClockRecoverySourceTimeFormat )
    {
	case TimeFormatUs:
		NormalizedSourceTime            = SourceTime;
		break;

	case TimeFormatPts:
		// This is practically a copy of FrameParser_Base_c::TranslatePlaybackTimeNativeToNormalized()
		// where all the useful comments can be found.

		#define         WrapMask        0x0000000180000000ULL
		#define         WrapOffset      0x0000000200000000ULL

		if( ClockRecoveryLastPts == INVALID_TIME )
		    ClockRecoveryLastPts        = SourceTime;

		if( ((ClockRecoveryLastPts & WrapMask) == WrapMask) &&
		    ((SourceTime           & WrapMask) == 0) )
		    ClockRecoveryPtsBaseLine   += WrapOffset;

		else if( ((ClockRecoveryLastPts & WrapMask) == 0) &&
			 ((SourceTime           & WrapMask) == WrapMask) )
		    ClockRecoveryPtsBaseLine   -= WrapOffset;

		ClockRecoveryLastPts            = SourceTime;

		NormalizedSourceTime            = (((ClockRecoveryPtsBaseLine + SourceTime) * 300) + 13) / 27;
		break;

	default:
		report( severity_error, "OutputCoordinator_Base_c::ClockRecoveryDataPoint - Unsupported source time format (%d).\n", ClockRecoverySourceTimeFormat );
		return PlayerNotSupported;
    }

    //
    // Initialize the accumulated data if this is our first point
    //

    if( ClockRecoveryBaseSourceClock == INVALID_TIME )
    {
	ClockRecoveryBaseSourceClock    = NormalizedSourceTime;
	ClockRecoveryBaseLocalClock     = LocalTime;
	ClockRecoveryAccumulatedPoints  = 0;
	ClockRecoveryLeastSquareFit.Reset();

	if( ClockRecoveryEstablishedBaseSource == INVALID_TIME )
	{
	    // If this is our first ever point, establish the minimum needed for guesstimating the source clock 
	    ClockRecoveryEstablishedGradient    = 1;
	    ClockRecoveryEstablishedBaseSource  = ClockRecoveryBaseSourceClock;
	    ClockRecoveryEstablishedBaseLocal   = ClockRecoveryBaseLocalClock;
	}
    }

    //
    // Accumulate the data point (converting the times to deltas first)
    //

    NormalizedSourceTime        -= ClockRecoveryBaseSourceClock + ClockRecoveryLeastSquareFit.Y();
    LocalTime                   -= ClockRecoveryBaseLocalClock  + ClockRecoveryLeastSquareFit.X();

    ClockRecoveryLeastSquareFit.Add( NormalizedSourceTime, LocalTime );
    ClockRecoveryAccumulatedPoints++;

    //
    // Is it time for a readout, we judge this on elapsed time and on a minimum number of points
    //

    if( (ClockRecoveryAccumulatedPoints >= CLOCK_RECOVERY_MINIMUM_POINTS) &&
	((unsigned long long)ClockRecoveryLeastSquareFit.X() >= ClockRecoveryIntegrationTime) )
    {
	ClockRecoveryEstablishedGradient        = ClockRecoveryLeastSquareFit.Gradient();
	ClockRecoveryEstablishedBaseSource      = ClockRecoveryBaseSourceClock + RoundedLongLongIntegerPart(ClockRecoveryLeastSquareFit.Intercept());
	ClockRecoveryEstablishedBaseLocal       = ClockRecoveryBaseLocalClock;

	//
	// Set us up for a reset
	//

	ClockRecoveryLastPts            = INVALID_TIME;         // Only used if PTS source time format
	ClockRecoveryPtsBaseLine        = 0;

	ClockRecoveryBaseSourceClock    = INVALID_TIME;
	ClockRecoveryBaseLocalClock     = INVALID_TIME;

	//
	// Adjust the integration time
	//

	if( ClockRecoveryIntegrationTime < CLOCK_RECOVERY_MAXIMUM_INTEGRATION_TIME )
	    ClockRecoveryIntegrationTime        *= 2;

	//
	// Do we need to adjust the system clock rate
	//

	MasterClock                     = Player->PolicyValue( Playback, PlayerAllStreams, PolicyMasterClock );
	if( MasterClock == PolicyValueSystemClockMaster )
	{
	    SystemClockAdjustmentEstablished    = true;
	    SystemClockAdjustment               = ClockRecoveryEstablishedGradient;
	}
    }

//

    return OutputCoordinatorNoError;
}

// /////////////////////////////////////////////////////////////////////////
//
//      The function to read out an estimate of the recovered clock
//

    OutputCoordinatorStatus_t   OutputCoordinator_Base_c::ClockRecoveryEstimate(
							unsigned long long               *SourceTime,
							unsigned long long               *LocalTime )
{
unsigned long long       Now;
unsigned long long       EstimatedSourceTime;
Rational_t               ElapsedSourceTime;

    //
    // Let us initialize first
    //

    if( SourceTime != NULL )
	*SourceTime     = INVALID_TIME;

    if( LocalTime != NULL )
	*LocalTime      = INVALID_TIME;

    //
    // Is clock recovery live 
    //

    if( !ClockRecoveryInitialized )
    {
	report( severity_error, "OutputCoordinator_Base_c::ClockRecoveryEstimate - Clock recovery not initialized.\n" );
	return OutputCoordinatorError;
    }

    if( ClockRecoveryEstablishedBaseSource == INVALID_TIME )
    {
	report( severity_error, "OutputCoordinator_Base_c::ClockRecoveryEstimate - No basis on which to make an estimate.\n" );
	return OutputCoordinatorError;
    }

    //
    // Calculate the value
    //

    Now                 = OS_GetTimeInMicroSeconds();
    ElapsedSourceTime   = ClockRecoveryEstablishedGradient * ((int)(Now - ClockRecoveryEstablishedBaseLocal));
    EstimatedSourceTime = ClockRecoveryEstablishedBaseSource + ElapsedSourceTime.RoundedLongLongIntegerPart();

    //
    // If a large amount of time has passed rebase before we use up all the bits
    //

    if( ElapsedSourceTime > (2<<29) )
    {
	ClockRecoveryEstablishedBaseLocal       = Now;
	ClockRecoveryEstablishedBaseSource      = EstimatedSourceTime;
    }

    //
    // Convert the estimate to an appropriate format
    //

    switch( ClockRecoverySourceTimeFormat )
    {
	case TimeFormatUs:
		// We operate in this format
		break;

	case TimeFormatPts:
		EstimatedSourceTime     = (((EstimatedSourceTime * 27) + 150) / 300) & 0x00000001ffffffffULL;
		break;

	default:
		report( severity_error, "OutputCoordinator_Base_c::ClockRecoveryEstimate - Unsupported source time format (%d).\n", ClockRecoverySourceTimeFormat );
		return PlayerNotSupported;
    }

    //
    // Write the desired outputs
    //

    if( SourceTime != NULL )
	*SourceTime     = EstimatedSourceTime;

    if( LocalTime != NULL )
	*LocalTime      = Now;


    return OutputCoordinatorNoError;
}



// /////////////////////////////////////////////////////////////////////////
//
//      Time functions, coded here since direction 
//      of play may change the meaning later.
//

unsigned long long   OutputCoordinator_Base_c::SpeedScale(      unsigned long long      T )
{
Rational_t      Temp;

    if( Direction == PlayBackward )
	T       = -T;

    if( Speed == 1 )
	return T;

    Temp        = (Speed == 0) ? 0 : T / Speed;
    return Temp.LongLongIntegerPart();
}

//

unsigned long long   OutputCoordinator_Base_c::InverseSpeedScale(       unsigned long long      T )
{
Rational_t      Temp;

    if( Direction == PlayBackward )
	T       = -T;

    if( Speed == 1 )
	return T;

    Temp        = Speed * T;
    return Temp.LongLongIntegerPart();
}

