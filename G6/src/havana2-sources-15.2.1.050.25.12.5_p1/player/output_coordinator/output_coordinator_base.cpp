/************************************************************************
Copyright (C) 2003-2014 STMicroelectronics. All Rights Reserved.

This file is part of the Streaming Engine.

Streaming Engine is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

Streaming Engine is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with Streaming Engine; see the file COPYING.  If not, write to the Free
Software Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307,
USA.

The Streaming Engine Library may alternatively be licensed under a
proprietary license from ST.
************************************************************************/

#include "output_coordinator_base.h"
#include "player_generic.h"
#include "player_playback.h"
#include "player_stream.h"
#include "st_relayfs_se.h"

#undef TRACE_TAG
#define TRACE_TAG           "OutputCoordinator_Base_c"

#define VERBOSE_CLOCK_DEBUG 0
#define SYNCHRONIZE_WAIT                                30              // 4 waits of 30ms
#define MAX_SYNCHRONIZE_WAITS_LIVE                      20              // targeting the reception of a PCR within 600ms
#define MAX_SYNCHRONIZE_WAITS                           10               // wait 10 x 30 ms in case of non live.

#define MINIMUM_TIME_TO_MANIFEST                        100000          // I am guessing that it will take at least 100 ms to get from synchronize to the manifestor
#define INTERNAL_TIMESTAMP_LATENCY_ESTIMATE             10000           // 10ms

#define NEGATIVE_REASONABLE_LIMIT                       (-4000000ll)
#define POSITIVE_REASONABLE_LIMIT                       (+4000000ll)

#define REBASE_TIME_TRIGGER_VALUE                       0x10000000

#define PLAYBACK_TIME_JUMP_ERROR                        64000           // 64 milli seconds, some PTSs are specified in ms so need at least 2, however DVR adds errors of up to around 32 ms when it calculates it's ptss
#define OTHER_STREAMS_MUST_FOLLOW_JUMP_BY               1000000         // 1s if you haven't followed a jump in 1s, then you probably aren't following it at all
#define JUMP_CONFIRMATION_ELAPSED_TIME          1000000     // Force the jumped stream to be consistent for a while to be sure we havent just had a duff input ping pong

#define MAX_SYNCHRONIZATION_WINDOW                      10000000        // If two streams are more than 10 seconds apart, we have no hope of avsyncing them

#define MINIMUM_SLEEP_TIME_US                           10000

#define IGNORE_COUNT_FOR_VSYNC_OFFSET                   5
#define INTEGRATION_COUNT_FOR_VSYNC_OFFSET              5

/* With 16 second MINIMUM_INTEGRATION_TIME and 512 MINIMUM_POINTS at max  PPM(72)
And minimum 1 PCR data point every 100ms as per ISO 13818-1 standard (PCR reception is of at least 10 Hz)
Max (72*51  + 54*51  + 36*64 + 18*128 =) 11.059 ms delay after full clock recovery.*/
#define CLOCK_RECOVERY_MINIMUM_POINTS                   512
#define CLOCK_RECOVERY_MINIMUM_INTEGRATION_TIME         ( 16 * 1000000) //
#define CLOCK_RECOVERY_MAXIMUM_INTEGRATION_TIME         (2048 * 1000000)
#define CLOCK_RECOVERY_MAXIMUM_PPM          72          // A parts per million value controlling the sanity check for clock recovery gradient
#define CLOCK_RECOVERY_MAXIMUM_SINGLE_PPM_ADJUSTMENT    18          // A parts per million value controlling the rate of adjustment


/* Latency in micro sec to be applied for HDMIRx Repeater and non repeater mode
   Note this need to be fine tuned */
#define HDMIRX_REPEATER_MODE_LATENCY     40000
#define HDMIRX_NON_REPEATER_MODE_LATENCY 120000
#define MODULES_PROPAGATION_LATENCY      1000   // 1 ms latency to pass buffer between the modules

#define MAXIMUM_DECODE_WINDOW_SLEEP     200000     // 200 milliseconds
#define MINIMUM_DECODE_WINDOW_SLEEP     10000       // sleeping less than 10ms makes no sense

// /////////////////////////////////////////////////////////////////////////
//
// Static declaration of the output rate parameter sets
//  Minimum Integration Frames
//  Maximum Integration Frames
//  Ignore between integrations
//  IntegrationThresholdForDriftCorrection
//  Maximum Jitter allowed

#if 0
static OutputRateAdjustmentParameters_t     ORAInputFollowingAudio  = {  128, 2048, 64,  256, 32 };
static OutputRateAdjustmentParameters_t     ORAOutputDrivenAudio    = {  512, 4096, 64, 2048, 32 };
static OutputRateAdjustmentParameters_t     ORAInputFollowingVideo  = {  128, 2048, 64,  256, 32 };
static OutputRateAdjustmentParameters_t     ORAOutputDrivenVideo    = { 2048, 8192, 64, 8192, 32 };
#else
static OutputRateAdjustmentParameters_t     ORAInputFollowingAudio  = {  128, 2048, 64,  256, 32 };
static OutputRateAdjustmentParameters_t     ORAOutputDrivenAudio    = { 1024, 4096, 64, 2048, 32 };
static OutputRateAdjustmentParameters_t     ORAInputFollowingVideo  = {  128, 2048, 64,  256, 32 };
static OutputRateAdjustmentParameters_t     ORAOutputDrivenVideo    = { 256, 8192, 64, 512, 32 };
#endif

#define StreamType()                    ((Context->StreamType == 1) ? "Audio" : "Video")

static int GradientToPartsPerMillion(Rational_t Gradient)
{
    // report by how many parts per million out argument differs from 1
    Rational_t PartsPerPart = Gradient - 1;
    Rational_t PartsPerMillion = PartsPerPart * 1000000;
    return PartsPerMillion.IntegerPart();
}

//
OutputCoordinator_Base_c::OutputCoordinator_Base_c(void)
    : SynchronizeMayHaveCompleted()
    , StreamCount(0)
    , CoordinatedContexts(NULL)
    , StreamsInSynchronize(0)
    , MaximumStreamOffset(0)
    , MasterTimeMappingEstablished(false)
    , MasterTimeMappingVersion(0)
    , MasterBaseSystemTime(0)
    , MasterBaseNormalizedPlaybackTime(0)
    , UseSystemClockAsMaster(false)
    , SystemClockAdjustmentEstablished(true)
    , SystemClockAdjustment(1)
    , AccumulatedPlaybackTimeJumpsSinceSynchronization(0)
    , JumpSeenAtPlaybackTime(INVALID_TIME)
    , OriginatingStream(NULL)
    , OriginatingStreamConfirmsJump(false)
    , OriginatingStreamJumpedTo(INVALID_TIME)
    , Speed(1)
    , Direction(PlayForward)
    , ClockRecoveryInitialized(false)
    , ClockRecoverySourceTimeFormat(TimeFormatUs)
    , ClockRecoveryDevlogSystemClockOffset(INVALID_TIME)
    , ClockRecoveryLastPts(0)
    , ClockRecoveryPtsBaseLine(0)
    , ClockRecoveryBaseSourceClock(INVALID_TIME)
    , ClockRecoveryBaseLocalClock(INVALID_TIME)
    , ClockRecoveryAccumulatedPoints(0)
    , ClockRecoveryDiscardedPoints(0)
    , ClockRecoveryIntegrationTime(CLOCK_RECOVERY_MINIMUM_INTEGRATION_TIME)
    , ClockRecoveryLeastSquareFit()
    , ClockRecoveryEstablishedGradient(1)
    , ClockRecoveryLastSourceTime(INVALID_TIME)
    , ClockRecoveryLastLocalTime(INVALID_TIME)
    , LatencyUsedInHDMIRxModeDuringTimeMapping(0)
{
    OS_InitializeEvent(&SynchronizeMayHaveCompleted);
}

//
OutputCoordinator_Base_c::~OutputCoordinator_Base_c(void)
{
    Halt();
    Reset();

    OS_TerminateEvent(&SynchronizeMayHaveCompleted);
}

//
OutputCoordinatorStatus_t   OutputCoordinator_Base_c::Halt(void)
{
    if (TestComponentState(ComponentRunning))
    {
        //
        // Move the base state to halted early, to ensure
        // no activity can be queued once we start halting
        //
        BaseComponentClass_c::Halt();
        OS_SetEvent(&SynchronizeMayHaveCompleted);
    }

    return OutputCoordinatorNoError;
}


//
// This function does not fully revert this object to its initial state.  The
// CoordinatedContexts list is not cleaned up because contexts are referenced
// from both this list and OutputTimer_Base_c::mOutputCoordinatorContext and
// deleting them here would turn mOutputCoordinatorContext into a dangling
// pointer and cause double deletion.  OutputTimer_c::Reset() is instead
// responsible for calling DeRegisterStream() for removing its context from this
// list and deleting it.
//
OutputCoordinatorStatus_t   OutputCoordinator_Base_c::Reset(void)
{
    //
    // Initialize the system/playback time mapping
    //
    StreamsInSynchronize                = 0;
    MaximumStreamOffset                 = 0;
    MasterTimeMappingEstablished        = false;
    MasterTimeMappingVersion            = 0;
    SystemClockAdjustmentEstablished    = true;
    SystemClockAdjustment               = 1;
    AccumulatedPlaybackTimeJumpsSinceSynchronization    = 0;
    JumpSeenAtPlaybackTime              = INVALID_TIME;
    OriginatingStream                   = NULL;
    OriginatingStreamConfirmsJump       = false;
    OriginatingStreamJumpedTo           = INVALID_TIME;
    Speed                               = 1;
    Direction                           = PlayForward;
    ClockRecoveryInitialized            = false;
    ClockRecoveryDevlogSystemClockOffset = INVALID_TIME;
    ClockRecoveryLastSourceTime         = INVALID_TIME;
    ClockRecoveryLastLocalTime          = INVALID_TIME;

    return BaseComponentClass_c::Reset();
}

// /////////////////////////////////////////////////////////////////////////
//
//      the register a stream function, creates a new Coordinator context
//

OutputCoordinatorStatus_t   OutputCoordinator_Base_c::RegisterStream(
    PlayerStream_t                    Stream,
    PlayerStreamType_t                StreamType,
    OutputCoordinatorContext_t       *Context)
{
    OutputCoordinatorContext_t      NewContext;
    ComponentMonitor(this);
    //
    // Obtain a new context
    //
    *Context    = NULL;

    NewContext  = new struct OutputCoordinatorContext_s(Stream, StreamType);
    if (NewContext == NULL)
    {
        SE_ERROR("Failed to obtain a new Coordinator context\n");
        return PlayerInsufficientMemory;
    }

    // Initialize non trivial fields of context
    NewContext->OutputRateAdjustmentParameters = (StreamType == StreamTypeVideo) ? ORAInputFollowingVideo : ORAInputFollowingAudio;
    NewContext->ManifestorLatency = MINIMUM_TIME_TO_MANIFEST;
    ResetTimingContext(NewContext);

    //
    // Obtain the appropriate portion of the class list for this stream
    //
    Player->GetClassList(Stream, NULL, NULL, NULL, NULL, &NewContext->ManifestationCoordinator);

    if (NewContext->ManifestationCoordinator == NULL)
    {
        SE_ERROR("This implementation does not support no-output timing.\n ");
        delete NewContext;
        return PlayerNotSupported;
    }

    //
    // If there is an external time mapping in force,
    // then we copy it into this context.
    //
    int ExternalMapping  = Player->PolicyValue(Playback, NewContext->Stream, PolicyExternalTimeMapping);

    if ((ExternalMapping == PolicyValueApply) && MasterTimeMappingEstablished)
    {
        NewContext->BaseSystemTime                                     = MasterBaseSystemTime;
        NewContext->BaseNormalizedPlaybackTime                         = MasterBaseNormalizedPlaybackTime;
        NewContext->AccumulatedPlaybackTimeJumpsSinceSynchronization   = 0;
        NewContext->TimeMappingEstablished                             = MasterTimeMappingEstablished;
        NewContext->BasedOnMasterMappingVersion                        = MasterTimeMappingVersion;
    }

    //
    // Insert the context into the list
    //
    NewContext->Next            = CoordinatedContexts;
    CoordinatedContexts         = NewContext;
    *Context                    = NewContext;
    StreamCount++;
    return OutputCoordinatorNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
//      Function to de-register a stream.
//

OutputCoordinatorStatus_t   OutputCoordinator_Base_c::DeRegisterStream(OutputCoordinatorContext_t        Context)
{
    OutputCoordinatorContext_t      *PointerToContext;
    ComponentMonitor(this);

    //
    // Make sure no one is trying to synchronize this stream
    //

    while (Context->InSynchronizeFn)
    {
        ComponentMonitor.UnLock();
        SE_INFO(group_avsync, "In Synchronize (%d)\n", Context->InSynchronizeFn);
        OS_SleepMilliSeconds(5);
        ComponentMonitor.Lock();
    }

    //
    // Remove the context from the list
    //

    for (PointerToContext        = &CoordinatedContexts;
         *PointerToContext      != NULL;
         PointerToContext        = &((*PointerToContext)->Next))
        if ((*PointerToContext) == Context)
        {
            *PointerToContext   = Context->Next;
            delete Context;
            //
            // Reduce stream count
            //
            StreamCount--;
            OS_SetEvent(&SynchronizeMayHaveCompleted);
            break;
        }

    //
    // Reverify the master clock
    //
    VerifyMasterClock();

    return OutputCoordinatorNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
//      Function to set the speed
//

OutputCoordinatorStatus_t   OutputCoordinator_Base_c::SetPlaybackSpeed(
    OutputCoordinatorContext_t        Context,
    Rational_t                        Speed,
    PlayDirection_t                   Direction)
{
    ComponentMonitor(this);

    if (Context != PlaybackContext)
    {
        SE_ERROR("This implementation does not support stream specific speeds\n");
        return PlayerNotSupported;
    }

    if ((this->Speed == 0) && (Speed == 0))
    {
        // -----------------------------------------------------------------------
        // Was Paused - still paused - do nothing
        //
    }
    else if (Speed == 0)
    {
        //-----------------------------------------------------------------------
        // Enterring a paused state
        //
    }
    else
    {
        ComponentMonitor.UnLock();
        ResetTimeMapping(PlaybackContext);
        ComponentMonitor.Lock();
    }

    //
    // Record the new speed and direction
    //
    this->Speed         = Speed;
    this->Direction     = Direction;

    return OutputCoordinatorNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
//      Function to reset the time mapping
//

void OutputCoordinator_Base_c::ResetStreamTimeMapping(OutputCoordinatorContext_t Context)
{
    SE_INFO(group_avsync, "Stream 0x%p\n", Context->Stream);

    Context->TimeMappingEstablished         = false;
    Context->BasedOnMasterMappingVersion    = 0;
    OS_SetEvent(&Context->AbortPerformEntryIntoDecodeWindowWait);
}

void OutputCoordinator_Base_c::ResetMasterTimeMapping()
{
    SE_INFO(group_avsync, "\n");

    MasterTimeMappingEstablished                            = false;
    AccumulatedPlaybackTimeJumpsSinceSynchronization        = 0;
    JumpSeenAtPlaybackTime                                  = INVALID_TIME;
    OriginatingStream                                       = NULL;
    OriginatingStreamConfirmsJump                           = false;

    // Release anyone waiting to enter a decode window
    for (OutputCoordinatorContext_t ContextLoop = CoordinatedContexts; ContextLoop != NULL; ContextLoop = ContextLoop->Next)
    {
        ResetStreamTimeMapping(ContextLoop);
    }
}

bool OutputCoordinator_Base_c::NoStreamUsesMasterTimeMapping()
{
    for (OutputCoordinatorContext_t ContextLoop = CoordinatedContexts; ContextLoop != NULL; ContextLoop = ContextLoop->Next)
    {
        if (ContextLoop->TimeMappingEstablished == true) { return false; }
    }

    return true;
}

OutputCoordinatorStatus_t   OutputCoordinator_Base_c::ResetTimeMapping(OutputCoordinatorContext_t        Context)
{
    ComponentMonitor(this);

    SE_INFO(group_avsync, "Context 0x%p\n", Context);

    if (Context == PlaybackContext)
    {
        ResetMasterTimeMapping();
    }
    else
    {
        ResetStreamTimeMapping(Context);

        if (NoStreamUsesMasterTimeMapping())
        {
            ResetMasterTimeMapping();
        }
    }

    return OutputCoordinatorNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
//      Set a specific mapping
//

OutputCoordinatorStatus_t   OutputCoordinator_Base_c::EstablishTimeMapping(
    OutputCoordinatorContext_t        Context,
    unsigned long long                NormalizedPlaybackTime,
    unsigned long long                SystemTime)
{
    unsigned long long              Now;
    OutputCoordinatorContext_t      ContextLoop;
    PlayerEventRecord_t             Event;

    ComponentMonitor(this);
    //
    // Set the master mapping as specified
    //
    MasterTimeMappingEstablished                        = false;
    Now                                                 = OS_GetTimeInMicroSeconds();
    MasterBaseNormalizedPlaybackTime                    = NormalizedPlaybackTime;
    MasterBaseSystemTime                                = ValidTime(SystemTime) ? SystemTime : Now;
    AccumulatedPlaybackTimeJumpsSinceSynchronization    = 0;
    JumpSeenAtPlaybackTime                              = INVALID_TIME;
    OriginatingStream                                   = NULL;
    OriginatingStreamConfirmsJump                       = false;
    OriginatingStreamJumpedTo                           = INVALID_TIME;
    MasterTimeMappingEstablished                        = true;
    MasterTimeMappingVersion++;

    //
    // Propagate this to the appropriate coordinated contexts
    //

    for (ContextLoop     = (Context == PlaybackContext) ? CoordinatedContexts : Context;
         ((Context == PlaybackContext) ? (ContextLoop != NULL) : (ContextLoop == Context));
         ContextLoop     = ContextLoop->Next)
    {
        ContextLoop->BaseSystemTime                                     = MasterBaseSystemTime;
        ContextLoop->BaseNormalizedPlaybackTime                         = MasterBaseNormalizedPlaybackTime;
        ContextLoop->AccumulatedPlaybackTimeJumpsSinceSynchronization   = 0;
        ContextLoop->LastPlaybackTimeJump                               = INVALID_TIME;
        ContextLoop->TimeMappingEstablished                             = true;
        ContextLoop->BasedOnMasterMappingVersion                        = MasterTimeMappingVersion;
        RestartOutputRateIntegration(ContextLoop);
        // Ensure no-one is waiting in an out of date decode window.
        OS_SetEvent(&ContextLoop->AbortPerformEntryIntoDecodeWindowWait);
    }

    //
    // All communal activity completed, free the lock.
    //
    ComponentMonitor.UnLock();

    OS_SetEvent(&SynchronizeMayHaveCompleted);
    return OutputCoordinatorNoError;
}

// /////////////////////////////////////////////////////////////////////////
//
//      Translate a given playback time to a system time
//

OutputCoordinatorStatus_t   OutputCoordinator_Base_c::TranslatePlaybackTimeToSystem(
    OutputCoordinatorContext_t        Context,
    unsigned long long                NormalizedPlaybackTime,
    unsigned long long               *SystemTime)
{
    unsigned long long      BaseNormalizedPlaybackTime;
    unsigned long long      BaseSystemTime;
    unsigned long long      ElapsedNormalizedPlaybackTime;
    unsigned long long      ElapsedSystemTime;
    // Lock the component to thread-safely use ClockRecovery variables
    ComponentMonitor(this);

    if (!MasterTimeMappingEstablished)
    {
        return OutputCoordinatorMappingNotEstablished;
    }

    if (Context != PlaybackContext)
    {
        if (!Context->TimeMappingEstablished)
        {
            return OutputCoordinatorMappingNotEstablished;
        }

        BaseNormalizedPlaybackTime      = Context->BaseNormalizedPlaybackTime;
        BaseSystemTime                  = Context->BaseSystemTime;
    }
    else
    {
        BaseNormalizedPlaybackTime      = MasterBaseNormalizedPlaybackTime;
        BaseSystemTime                  = MasterBaseSystemTime;
    }

    ElapsedNormalizedPlaybackTime       = NormalizedPlaybackTime - BaseNormalizedPlaybackTime;
    ElapsedSystemTime                   = SpeedScale(ElapsedNormalizedPlaybackTime);
    ElapsedSystemTime                   = RoundedLongLongIntegerPart(ElapsedSystemTime / SystemClockAdjustment);

    *SystemTime                         = BaseSystemTime + ElapsedSystemTime;

    //
    // Here we rebase if the elapsed times are large
    //

    if (ElapsedSystemTime > REBASE_TIME_TRIGGER_VALUE)
    {
        if ((Context == PlaybackContext) || (MasterBaseSystemTime == Context->BaseSystemTime))
        {
            MasterBaseNormalizedPlaybackTime    = NormalizedPlaybackTime;
            MasterBaseSystemTime                = *SystemTime;
        }

        if (Context != PlaybackContext)
        {
            Context->BaseNormalizedPlaybackTime = NormalizedPlaybackTime;
            Context->BaseSystemTime             = *SystemTime;
        }
    }

    return OutputCoordinatorNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
//      Translate a given system time to a playback time
//

OutputCoordinatorStatus_t   OutputCoordinator_Base_c::TranslateSystemTimeToPlayback(
    OutputCoordinatorContext_t        Context,
    unsigned long long                SystemTime,
    unsigned long long               *NormalizedPlaybackTime)
{
    unsigned long long      BaseNormalizedPlaybackTime;
    unsigned long long      BaseSystemTime;
    unsigned long long      ElapsedNormalizedPlaybackTime;
    unsigned long long      ElapsedSystemTime;
    // Lock the component to thread-safely use ClockRecovery variables
    ComponentMonitor(this);

    if (!MasterTimeMappingEstablished)
    {
        return OutputCoordinatorMappingNotEstablished;
    }

    if (Context != PlaybackContext)
    {
        if (!Context->TimeMappingEstablished)
        {
            return OutputCoordinatorMappingNotEstablished;
        }

        BaseNormalizedPlaybackTime      = Context->BaseNormalizedPlaybackTime;
        BaseSystemTime                  = Context->BaseSystemTime;
    }
    else
    {
        BaseNormalizedPlaybackTime      = MasterBaseNormalizedPlaybackTime;
        BaseSystemTime                  = MasterBaseSystemTime;
    }

    ElapsedSystemTime                   = SystemTime - BaseSystemTime;
    ElapsedSystemTime                   = RoundedLongLongIntegerPart(ElapsedSystemTime * SystemClockAdjustment);
    ElapsedNormalizedPlaybackTime       = InverseSpeedScale(ElapsedSystemTime);

    *NormalizedPlaybackTime             = BaseNormalizedPlaybackTime + ElapsedNormalizedPlaybackTime;

    return OutputCoordinatorNoError;
}

bool OutputCoordinator_Base_c::ReuseExistingTimeMapping(
    OutputCoordinatorContext_t        Context,
    unsigned long long                NormalizedPlaybackTime,
    unsigned long long                NormalizedDecodeTime,
    unsigned long long               *SystemTime,
    void                             *ParsedAudioVideoDataParameters)
{
    bool                            AlternateTimeMappingExists;
    bool                            AlternateMappingIsReasonable;
    unsigned long long              Now;
    unsigned long long              MasterMappingNow;
    long long int                   DeltaNow;


    AlternateTimeMappingExists  = MasterTimeMappingEstablished &&
                                  (Context->BasedOnMasterMappingVersion != MasterTimeMappingVersion);

    if (AlternateTimeMappingExists)
    {
        Now             = OS_GetTimeInMicroSeconds();
        TranslatePlaybackTimeToSystem(PlaybackContext, NormalizedPlaybackTime, &MasterMappingNow);
        DeltaNow    = ((long long int)MasterMappingNow - (long long int)Now);
        AlternateMappingIsReasonable    = inrange(DeltaNow, (long long)Context->ManifestorLatency, POSITIVE_REASONABLE_LIMIT);

        if (Player->PolicyValue(Playback, Stream, PolicyHdmiRxMode) != PolicyValueHdmiRxModeDisabled)
        {
            long long LargestOfMinimumManifestationLatencies = GetLargestOfMinimumManifestationLatencies(ParsedAudioVideoDataParameters);
            if (LatencyUsedInHDMIRxModeDuringTimeMapping < LargestOfMinimumManifestationLatencies)
            {
                AlternateMappingIsReasonable    = false;
                SE_INFO(group_avsync, "Stream 0x%p: Mapping not reasonable %d- Latency used during previous time mapping %lld  Latency required for current time mapping %lld\n",
                        Context->Stream, Context->StreamType, LatencyUsedInHDMIRxModeDuringTimeMapping, LargestOfMinimumManifestationLatencies);
            }
            else
            {
                AlternateMappingIsReasonable    = true;
                SE_INFO(group_avsync, "Stream 0x%p: Mapping reasonable %d- Latency used during previous time mapping %lld  Latency required for current time mapping %lld\n",
                        Context->Stream, Context->StreamType, LatencyUsedInHDMIRxModeDuringTimeMapping, LargestOfMinimumManifestationLatencies);
            }
        }
        else if (Player->PolicyValue(Playback, PlayerAllStreams, PolicyVideoStartImmediate) == PolicyValueApply
                 && Player->PolicyValue(Playback, PlayerAllStreams, PolicyLivePlayback) != PolicyValueApply)
        {
            if (Context->StreamType == StreamTypeAudio)
            {
                // when PolicyVideoStartImmediate is set, audio should not trigger a reset of the time mapping
                AlternateMappingIsReasonable    = true;
            }
            else if (Context->StreamType == StreamTypeVideo)
            {
                // when PolicyVideoStartImmediate is set, video should never reuse a time mapping done by audio
                AlternateMappingIsReasonable    = false;
            }
        }

        if (AlternateMappingIsReasonable)
        {
            Context->AccumulatedPlaybackTimeJumpsSinceSynchronization   = 0;
            Context->LastPlaybackTimeJump                               = INVALID_TIME;
            Context->BaseSystemTimeAdjusted                             = true;
            Context->BaseSystemTime                                     = MasterBaseSystemTime;
            Context->BaseNormalizedPlaybackTime                         = MasterBaseNormalizedPlaybackTime;
            Context->TimeMappingEstablished                             = true;
            Context->BasedOnMasterMappingVersion                        = MasterTimeMappingVersion;
            Context->StreamOffset                                       = DeltaNow;

            if (Abs(Context->StreamOffset) > (long long)MaximumStreamOffset)
            {
                MaximumStreamOffset                   = (unsigned long long)Abs(Context->StreamOffset);
            }

            TranslatePlaybackTimeToSystem(Context, NormalizedPlaybackTime, SystemTime);
            Context->InSynchronizeFn                            = false;
            SE_INFO(group_avsync, "Stream 0x%p: Sync Out (Reusing Existing Time Mapping) - %d - DeltaNow=%lld ManifestorLatency=%lld StreamOffset=%lld\n",
                    Context->Stream, Context->StreamType, DeltaNow, Context->ManifestorLatency, Context->StreamOffset);
            return true;
        }
        SE_INFO(group_avsync, "Stream 0x%p: Mapping not reasonable - %d - DeltaNow=%lld ManifestorLatency=%lld\n",
                Context->Stream, Context->StreamType, DeltaNow, Context->ManifestorLatency);
    }
    return false;
}


// /////////////////////////////////////////////////////////////////////////
//
//      Synchronize the streams to establish a time mapping
//

OutputCoordinatorStatus_t   OutputCoordinator_Base_c::SynchronizeStreams(
    OutputCoordinatorContext_t        Context,
    unsigned long long                NormalizedPlaybackTime,
    unsigned long long                NormalizedDecodeTime,
    unsigned long long               *SystemTime,
    void                             *ParsedAudioVideoDataParameters)
{
    OutputCoordinatorStatus_t       Status;
    OutputCoordinatorContext_t  ContextLoop;
    unsigned int                    WaitCount;
    PlayerEventRecord_t             Event;
    ComponentMonitor(this);

    // We do not perform the synchronization if we use an enforced external time mapping
    int ExternalMapping  = Player->PolicyValue(Playback, Context->Stream, PolicyExternalTimeMapping);
    if (ExternalMapping == PolicyValueApply)
    {
        SE_INFO(group_avsync, "Function entered when PolicyExternalTimeMapping is set\n");
        *SystemTime     = UNSPECIFIED_TIME;
        return OutputCoordinatorNoError;
    }

    //
    // Load in the surface descriptors, and verify the master clock
    //
    VerifyMasterClock();

    SE_INFO(group_avsync, "Stream 0x%p: Sync In - %d - NormalizedPlaybackTime=0x%llx NormalizedDecodeTime=0x%llx\n",
            Context->Stream, Context->StreamType, NormalizedPlaybackTime, NormalizedDecodeTime);

    Context->InSynchronizeFn    = true;

    if (ReuseExistingTimeMapping(Context, NormalizedPlaybackTime, NormalizedDecodeTime, SystemTime, ParsedAudioVideoDataParameters))
    {
        return OutputCoordinatorNoError;
    }

    //
    // Having reached here, either there is no alternate mapping, or the alternate mapping is
    // not appropriate, in either event there is now no valid master time mapping. We therefore
    // invalidate it before proceding to establish a new one.
    //
    MasterTimeMappingEstablished        = false;

    for (ContextLoop     = CoordinatedContexts;
         (ContextLoop != NULL);
         ContextLoop     = ContextLoop->Next)
    {
        ContextLoop->TimeMappingEstablished     = false;
        OS_SetEvent(&ContextLoop->AbortPerformEntryIntoDecodeWindowWait);
    }

    // Now we need to add ourselves to the list of synchronizing streams
    Context->SynchronizingAtPlaybackTime        = NormalizedPlaybackTime;
    StreamsInSynchronize++;

    // Enter the loop where we await synchronization
    bool LivePlay = (Player->PolicyValue(Playback, PlayerAllStreams, PolicyLivePlayback) == PolicyValueApply);
    WaitCount   = 0;

    while (true)
    {
        // Has someone completed the establishment of the mapping
        if (MasterTimeMappingEstablished)
        {
            Context->AccumulatedPlaybackTimeJumpsSinceSynchronization   = 0;
            Context->LastPlaybackTimeJump                               = INVALID_TIME;
            Context->BaseSystemTimeAdjusted                             = true;
            Context->BaseSystemTime                                     = MasterBaseSystemTime;
            Context->BaseNormalizedPlaybackTime                         = MasterBaseNormalizedPlaybackTime;
            Context->TimeMappingEstablished                             = true;
            Context->BasedOnMasterMappingVersion                        = MasterTimeMappingVersion;


            if (MaximumStreamOffset > MAX_SYNCHRONIZATION_WINDOW)
            {
                // We have a problem, these streams cannot be synchronized,
                // oldest frames will be discarded.
                if (!inrange(Context->StreamOffset, -MAX_SYNCHRONIZATION_WINDOW, MAX_SYNCHRONIZATION_WINDOW))
                {
                    SE_WARNING("Impossible to synchronize stream 0x%p, stream offset by %12lldus\n",
                               Context->Stream, Context->StreamOffset);
                }

            }

            Status      = TranslatePlaybackTimeToSystem(Context, NormalizedPlaybackTime, SystemTime);
            break;
        }

        // Can we do the synchronization
        if ((LivePlay && (ValidTime(ClockRecoveryLastSourceTime) || (WaitCount >= MAX_SYNCHRONIZE_WAITS_LIVE))) ||   /* We've received a valid PCR */
            (!LivePlay && ((StreamsInSynchronize == StreamCount)        || (WaitCount >= MAX_SYNCHRONIZE_WAITS)))      ||
            (Player->PolicyValue(Playback, Stream, PolicyHdmiRxMode)    != PolicyValueHdmiRxModeDisabled))/* No wait in HDMIRxMode */

        {
            Status = GenerateTimeMapping();

            if (Status != OutputCoordinatorNoError)
            {
                SE_ERROR("Failed to generate a time mapping\n");
            }

            if (LivePlay && ValidTime(ClockRecoveryLastSourceTime))
            {
                Status = GenerateTimeMappingWithPCR();

                if (Status != OutputCoordinatorNoError)
                {
                    SE_ERROR("Failed to generate a time mapping with PCR\n");
                }
            }

            if (Player->PolicyValue(Playback, Stream, PolicyHdmiRxMode) != PolicyValueHdmiRxModeDisabled)
            {
                GenerateTimeMappingForHdmiRx(Context, ParsedAudioVideoDataParameters);
            }

            // Wake everyone and pass back through to pickup the mapping
            OS_SetEvent(&SynchronizeMayHaveCompleted);
        }

        // Wait until something happens
        if (!MasterTimeMappingEstablished)
        {
            OS_ResetEvent(&SynchronizeMayHaveCompleted);
            ComponentMonitor.UnLock();
            OS_WaitForEventAuto(&SynchronizeMayHaveCompleted, SYNCHRONIZE_WAIT);
            ComponentMonitor.Lock();
            WaitCount++;
        }
    }

    Context->InSynchronizeFn    = false;
    StreamsInSynchronize--;
    long long deltaNow = (long long)(*SystemTime - OS_GetTimeInMicroSeconds());

    SE_INFO(group_avsync, "Stream 0x%p: Sync Out (New Time Mapping) - %d - NormalizedPlaybackTime=0x%llx SystemTime=%8lld (deltaNow=%lld) StreamOffset=%lld\n",
            Context->Stream, Context->StreamType, NormalizedPlaybackTime, *SystemTime, deltaNow, Context->StreamOffset);

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
    unsigned long long                DecodeWindowPorch)
{
    OutputCoordinatorStatus_t         Status;
    unsigned long long                SystemTime;
    unsigned long long                Now;
    unsigned long long                SleepTime;
    //
    // Reset the abort flag
    //
    OS_ResetEvent(&Context->AbortPerformEntryIntoDecodeWindowWait);
    //
    // Get the translation of the decode time, and apply the porch to find the window start
    // If we do not have a mapping here we can use the master mapping.
    //
    Status      = TranslatePlaybackTimeToSystem((Context->TimeMappingEstablished ? Context : PlaybackContext), NormalizedDecodeTime, &SystemTime);

    Now         = OS_GetTimeInMicroSeconds();
    if (Status == OutputCoordinatorMappingNotEstablished)
    {
        SleepTime = 30000;
        SE_WARNING("Stream 0x%p Mapping not established yet, sleeping for %lldus", Stream, SleepTime);
    }
    else
    {

        //
        // Shall we sleep
        //

        if (Now >= SystemTime - DecodeWindowPorch)
        {
            return OutputCoordinatorNoError;
        }

        //
        // How long shall we sleep
        //
        SleepTime                   = (SystemTime - DecodeWindowPorch - Now);

        if (SleepTime < MINIMUM_SLEEP_TIME_US)
        {
            SE_EXTRAVERB(group_avsync, "Stream 0x%p Sleep period too short (%lluus)\n", Stream, SleepTime);
            return OutputCoordinatorNoError;
        }
        else if (SleepTime > MAXIMUM_DECODE_WINDOW_SLEEP)
        {
            SE_DEBUG(group_avsync, "Sleep period too long (%lluus), clamping to %d us\n", SleepTime, MAXIMUM_DECODE_WINDOW_SLEEP);
            SleepTime               = MAXIMUM_DECODE_WINDOW_SLEEP;
        }
    }
    //
    // Sleep
    //

    SE_VERBOSE(group_avsync, "Stream 0x%p About to sleep for  %lluus (system time : %lluus porch : %lluus now : %llus)\n"
               , Stream
               , SleepTime
               , SystemTime
               , DecodeWindowPorch
               , Now);

    OS_Status_t WaitStatus = OS_WaitForEventAuto(&Context->AbortPerformEntryIntoDecodeWindowWait, (unsigned int)(SleepTime / 1000));

    return (WaitStatus == OS_NO_ERROR) ? OutputCoordinatorNoError : OutputCoordinatorAbandonedWait;
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
    unsigned long long                ActualPlaybackTime,
    long long int             DeltaCollationTime)
{
    long long           DeltaPlaybackTime;
    long long           JumpError;
    long long           PlaybackTimeForwardJumpThreshold;               // Values used in handling jumps in PTSs
    long long           PlaybackTimeReverseJumpThreshold;
    ComponentMonitor(this);

    //
    // Check that this call relates to a particular stream
    //

    if (Context == PlaybackContext)
    {
        SE_ERROR("Deltas only handled for specific streams\n");
        return OutputCoordinatorError;
    }

    //
    // Check if this is relevant
    //

    if (!MasterTimeMappingEstablished)
    {
        return OutputCoordinatorNoError;
    }

    //
    // Validate StreamOffset incase of ExternalMapping.Otherwise systemtime will be invalidated causing frame skips.
    //
    if (Player->PolicyValue(Playback, Context->Stream, PolicyExternalTimeMapping) == PolicyValueApply)
    {
        Context->StreamOffset = 0;
    }

    //
    // Are we doing a delayed invalidation of the time mapping because we were unable to synchronize last time around
    //

    if (!inrange(Context->StreamOffset, -MAX_SYNCHRONIZATION_WINDOW, MAX_SYNCHRONIZATION_WINDOW))
    {
        DrainLivePlayback();
        return OutputCoordinatorNoError;
    }

    //
    // What is the working sum of collation time deltas
    //

    if (ValidTime(DeltaCollationTime))
    {
        Context->SigmaCollationTimeDelta                -= Context->CollationTimeDeltas[Context->NextCollationTimeDelta];
        Context->CollationTimeDeltas[Context->NextCollationTimeDelta]    = max(DeltaCollationTime, 0);
        Context->SigmaCollationTimeDelta                += Context->CollationTimeDeltas[Context->NextCollationTimeDelta];
        Context->NextCollationTimeDelta                  = (Context->NextCollationTimeDelta + 1) % COLLATION_TIME_DELTA_COUNT;
    }

    //
    // What are the current value of the jump detector thresholds
    //
    int Threshold = Player->PolicyValue(Playback, Context->Stream, PolicyPtsForwardJumpDetectionThreshold);
    Threshold     = min(Threshold, 43);                   // 43 = ln (0x7fff...) / ln 2 - or the maximum time we can hold
    PlaybackTimeForwardJumpThreshold    = 1000000 * (1ULL << Threshold);
    int Symmetric                    = Player->PolicyValue(Playback, Context->Stream, PolicySymmetricJumpDetection);
    PlaybackTimeReverseJumpThreshold = (Symmetric == PolicyValueApply) ? PlaybackTimeForwardJumpThreshold : PLAYBACK_TIME_JUMP_ERROR;

    //
    // Now specifically for live play we adjust these to take account of interrupted
    // reception which will be effectively signalled by a jump in the collation times.
    //

    if (Player->PolicyValue(Playback, Context->Stream, PolicyLivePlayback) == PolicyValueApply)
    {
        PlaybackTimeForwardJumpThreshold    += Context->SigmaCollationTimeDelta;
        PlaybackTimeReverseJumpThreshold     = PlaybackTimeForwardJumpThreshold;
        KnownJump                = false;
    }

    //
    // If there is a jump in progress, are we the originator looking to confirm the jump
    //

    if (Context == OriginatingStream)
    {
        if (ValidTime(DeltaCollationTime) && (ExpectedPlaybackTime > (OriginatingStreamJumpedTo + JUMP_CONFIRMATION_ELAPSED_TIME)))
        {
            OriginatingStreamConfirmsJump   = true;
            OriginatingStream               = NULL;
            Context->LastPlaybackTimeJump   = INVALID_TIME;     // Remove chance of a ping pong (seen a BBC mistaken ping pong 3.25 hours after the original jump)
        }
    }

    //
    // Now evaluate the delta and handle any jump in the playback time
    //
    DeltaPlaybackTime   = (Direction == PlayBackward) ?
                          (long long)(ExpectedPlaybackTime - ActualPlaybackTime) :
                          (long long)(ActualPlaybackTime - ExpectedPlaybackTime);

    if (!inrange(DeltaPlaybackTime, -PlaybackTimeReverseJumpThreshold, PlaybackTimeForwardJumpThreshold) || KnownJump)
    {
        SE_INFO(group_avsync, "Spotted a delta(%s) %12lld\n", StreamType(), DeltaPlaybackTime);
        SE_INFO(group_avsync, "    CollationTimedelta = %12lld\n", Context->SigmaCollationTimeDelta);
        SE_INFO(group_avsync, "    Deltas = %lld - %lld - %lld - %lld - %lld - %lld - %lld - %lld\n",
                Context->CollationTimeDeltas[0], Context->CollationTimeDeltas[1], Context->CollationTimeDeltas[2], Context->CollationTimeDeltas[3],
                Context->CollationTimeDeltas[4], Context->CollationTimeDeltas[5], Context->CollationTimeDeltas[6], Context->CollationTimeDeltas[7]);
        //
        // Report an error, if we spot a delta on
        // externally timed data, and ignore the delta.
        //
        int ExternalMapping = Player->PolicyValue(Playback, Context->Stream, PolicyExternalTimeMapping);

        if (ExternalMapping == PolicyValueApply)
        {
            SE_WARNING("%s stream: Spotted large playback delta(%lldus) when PolicyExternalTimeMapping is set\n", StreamType(), DeltaPlaybackTime);
        }
        //
        // Is this part of a cascading change
        //
        else if (AccumulatedPlaybackTimeJumpsSinceSynchronization != Context->AccumulatedPlaybackTimeJumpsSinceSynchronization)
        {
            //
            // Is this a complete jump to the cascaded change, or just a partial
            // (ie a rapid sequence of jumps close together).
            //
            JumpError   = AccumulatedPlaybackTimeJumpsSinceSynchronization - (Context->AccumulatedPlaybackTimeJumpsSinceSynchronization - DeltaPlaybackTime);
            SE_INFO(group_avsync, "Cascading a Jump(%s) Error %12lld - %s catchup\n", StreamType(), JumpError,
                    (inrange(JumpError, -PlaybackTimeReverseJumpThreshold, PlaybackTimeForwardJumpThreshold) ? "Complete" : "Partial"));

            if (!inrange(JumpError, -PlaybackTimeReverseJumpThreshold, PlaybackTimeForwardJumpThreshold))
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

                // On a packet injector, there can be a delay during loopback, this breaks live playback
                // however on a real stream, the act of just hitting re-sync can extend buffering by 60..100ms
                // As a compromise we do a playback drain

                if (Player->PolicyValue(Playback, Context->Stream, PolicyLivePlayback) == PolicyValueApply)
                {
                    DrainLivePlayback();
                }
            }
        }
        else if (ValidTime(JumpSeenAtPlaybackTime) &&
                 inrange(DeltaPlaybackTime, 0, 2000000) &&
                 (ExpectedPlaybackTime >= JumpSeenAtPlaybackTime) &&
                 (ExpectedPlaybackTime - JumpSeenAtPlaybackTime <= 10000000) &&
                 (Player->PolicyValue(Playback, Context->Stream, PolicyLivePlayback) != PolicyValueApply))
        {
            SE_WARNING("%s stream: Multiple jumps over short period\n", StreamType());
            ComponentMonitor.UnLock();
            ResetTimeMapping(PlaybackContext);
            ComponentMonitor.Lock();
        }
        //
        // Not part of a cascading change yet, we initiate a cascading change
        //
        else
        {
            //
            // Check for a ping pong correction (duff input times)
            //
            if (ValidTime(Context->LastPlaybackTimeJump) &&
                ((Context->LastPlaybackTimeJump < 0) ^ (DeltaPlaybackTime < 0)) &&  // opposite signs
                inrange((Context->LastPlaybackTimeJump + DeltaPlaybackTime), -PlaybackTimeReverseJumpThreshold, PlaybackTimeForwardJumpThreshold))
            {
                SE_INFO(group_avsync, "detected Ping-Pong Jump(%s)\n", StreamType());
                DeltaPlaybackTime       = -Context->LastPlaybackTimeJump;
                Context->LastPlaybackTimeJump   = INVALID_TIME;
                OriginatingStream       = NULL;
                OriginatingStreamConfirmsJump   = false;
                OriginatingStreamJumpedTo   = INVALID_TIME;
            }
            else
            {
                SE_INFO(group_avsync, "Initiating a Jump(%s)\n", StreamType());
                Context->LastPlaybackTimeJump   = DeltaPlaybackTime;
                OriginatingStream       = Context;
                OriginatingStreamConfirmsJump   = false;
                OriginatingStreamJumpedTo   = ActualPlaybackTime;
            }

            AccumulatedPlaybackTimeJumpsSinceSynchronization                    -= DeltaPlaybackTime;
            MasterBaseNormalizedPlaybackTime                                    += DeltaPlaybackTime;
            JumpSeenAtPlaybackTime                                               = ExpectedPlaybackTime;
            Context->BaseNormalizedPlaybackTime                                 += DeltaPlaybackTime;
            Context->AccumulatedPlaybackTimeJumpsSinceSynchronization            = AccumulatedPlaybackTimeJumpsSinceSynchronization;
            Context->BaseSystemTimeAdjusted                                      = true;

            if (StreamCount == 1) // running with only 1 playstream so complete jump detected. Reset the time mapping. See bug19464
            {
                SE_INFO(group_avsync, "running with only 1 playstream so complete jump detected(%s). Resetting the time mapping\n", StreamType());
                ComponentMonitor.UnLock();
                ResetTimeMapping(PlaybackContext);
                ComponentMonitor.Lock();
            }
        }
    }

    //
    // Here we check that if there is an ongoing cascading change that we are not part of,
    // then we have not reached a point when we really should have been part of it
    //

    if (AccumulatedPlaybackTimeJumpsSinceSynchronization != Context->AccumulatedPlaybackTimeJumpsSinceSynchronization)
    {
        // First check if this is just an adding up error, after a ping pong time oscillation
        DeltaPlaybackTime   = AccumulatedPlaybackTimeJumpsSinceSynchronization - Context->AccumulatedPlaybackTimeJumpsSinceSynchronization;

        if (inrange(DeltaPlaybackTime, -PlaybackTimeReverseJumpThreshold, PlaybackTimeForwardJumpThreshold))
        {
            Context->AccumulatedPlaybackTimeJumpsSinceSynchronization = AccumulatedPlaybackTimeJumpsSinceSynchronization;
        }

        // Now if the error still exists check if we should have matched times yet

        if (OriginatingStreamConfirmsJump &&
            (AccumulatedPlaybackTimeJumpsSinceSynchronization != Context->AccumulatedPlaybackTimeJumpsSinceSynchronization) &&
            ValidTime(JumpSeenAtPlaybackTime))
        {
            DeltaPlaybackTime       = (long long)(ActualPlaybackTime - JumpSeenAtPlaybackTime);

            if (!inrange(DeltaPlaybackTime, -PlaybackTimeForwardJumpThreshold, ((long long)(OTHER_STREAMS_MUST_FOLLOW_JUMP_BY + MaximumStreamOffset))))
            {
                SE_WARNING("%s stream failed to match previous jump in playback times\n", StreamType());
                SE_WARNING("  %12lld - (%12lld, %6d + %6lld) (%d %d)\n", DeltaPlaybackTime, PlaybackTimeForwardJumpThreshold,
                           OTHER_STREAMS_MUST_FOLLOW_JUMP_BY, MaximumStreamOffset,
                           (DeltaPlaybackTime >= -PlaybackTimeForwardJumpThreshold),
                           (DeltaPlaybackTime <= ((long long)(OTHER_STREAMS_MUST_FOLLOW_JUMP_BY + MaximumStreamOffset))));
                DrainLivePlayback();
            }
        }
    }

    //
    // Since it is convenient in here, we also check that we match the master mapping
    // since we may have missed some change to the master mapping
    //

    if (Context->BasedOnMasterMappingVersion != MasterTimeMappingVersion)
    {
        DeltaPlaybackTime       = (long long)(ActualPlaybackTime - MasterBaseNormalizedPlaybackTime);

        if (!inrange(DeltaPlaybackTime, -PlaybackTimeForwardJumpThreshold, PlaybackTimeForwardJumpThreshold))
        {
            SE_WARNING("%s stream failed to match previous master mapping change\n", StreamType());
            DrainLivePlayback();
        }
    }

    return OutputCoordinatorNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
// This function calculates a long term drift corrector that tries to correct the current
// synchronization error
//

void OutputCoordinator_Base_c::CalculateLongTermDriftCorrector(OutputCoordinatorContext_t   Context,
                                                               TimingContext_t             *State,
                                                               long long int                CurrentError,
                                                               int                          i)
{
    unsigned int    MaximumClockRateAdjustmentPpm;
    Rational_t      MaximumClockRateMultiplier;
    Rational_t      MinimumClockRateMultiplier;
    Rational_t          AnticipatedDriftAdjustment;
    long long           AnticipatedDriftCorrection       = 0ll;
    unsigned long long  AnticipatedDriftCorrectionPeriod = 0ll;

    if (State->FramesToIntegrateOver >= Context->OutputRateAdjustmentParameters.IntegrationThresholdForDriftCorrection)
    {
        AnticipatedDriftCorrection       = -CurrentError / 2;
        AnticipatedDriftCorrectionPeriod = (State->FramesToIntegrateOver < Context->OutputRateAdjustmentParameters.ClockDriftMaximumIntegrationFrames) ?
                                           (2 * State->LeastSquareFit.Y()) : State->LeastSquareFit.Y();

        if (0 == AnticipatedDriftCorrectionPeriod)
        {
            SE_WARNING("fixing AnticipatedDriftCorrectionPeriod 0->1\n");
            AnticipatedDriftCorrectionPeriod = 1;
        }

        AnticipatedDriftAdjustment       = Rational_t(AnticipatedDriftCorrection, AnticipatedDriftCorrectionPeriod);
        SE_DEBUG(group_avsync, "(%d-%d) AnticipatedDriftAdjustment pre-clamp: %d.%09d\n",
                 Context->StreamType, i, AnticipatedDriftAdjustment.IntegerPart(), AnticipatedDriftAdjustment.RemainderDecimal(9));
        Clamp(AnticipatedDriftAdjustment, Rational_t(-4, 1000000), Rational_t(4, 1000000));
        SE_DEBUG(group_avsync, "(%d-%d) AnticipatedDriftAdjustment Clamped %d.%09d\n",
                 Context->StreamType, i, AnticipatedDriftAdjustment.IntegerPart(), AnticipatedDriftAdjustment.RemainderDecimal(9));
        SE_DEBUG(group_avsync, "(%d-%d) ClockAdjustment pre drift: %d.%09d %dppm\n",
                 Context->StreamType, i, State->ClockAdjustment.IntegerPart(), State->ClockAdjustment.RemainderDecimal(9),
                 GradientToPartsPerMillion(State->ClockAdjustment));
        State->ClockAdjustment          += AnticipatedDriftAdjustment;
        SE_DEBUG(group_avsync, "(%d-%d) ClockAdjustment drift added: %d.%09d %dppm\n",
                 Context->StreamType, i, State->ClockAdjustment.IntegerPart(), State->ClockAdjustment.RemainderDecimal(9),
                 GradientToPartsPerMillion(State->ClockAdjustment));
        AnticipatedDriftCorrection       = IntegerPart(AnticipatedDriftCorrectionPeriod * AnticipatedDriftAdjustment);
        SE_DEBUG(group_avsync, "(%d-%d) AnticipatedDriftCorrection=%lld AnticipatedDriftCorrectionPeriod=%llu CurrentErrorHistory=(%lld, %lld, %lldx %lld)\n",
                 Context->StreamType, i, AnticipatedDriftCorrection, AnticipatedDriftCorrectionPeriod,
                 State->CurrentErrorHistory[0], State->CurrentErrorHistory[1], State->CurrentErrorHistory[2], State->CurrentErrorHistory[3]);
    }

    MaximumClockRateAdjustmentPpm   = 1 << Player->PolicyValue(Playback, Context->Stream, PolicyClockPullingLimit2ToTheNPartsPerMillion);
    MaximumClockRateMultiplier      = Rational_t(1000000 + MaximumClockRateAdjustmentPpm, 1000000);
    MinimumClockRateMultiplier      = Rational_t(1000000 - MaximumClockRateAdjustmentPpm, 1000000);
    Clamp(State->ClockAdjustment, MinimumClockRateMultiplier, MaximumClockRateMultiplier);
}


// /////////////////////////////////////////////////////////////////////////
//
// Calculate a new adjustment multiplier, and clamp it
// to prevent too rapid adjustment. The clamp range
// starts quite high, and drives down to 4ppm over the
// longer integrations.
//

void OutputCoordinator_Base_c::CalculateAdjustmentMultiplier(
    OutputCoordinatorContext_t   Context,
    TimingContext_t             *State,
    long long int                CurrentError,
    int                          i)
{
    Rational_t  AdjustmentMultiplier;
    AdjustmentMultiplier    = State->LeastSquareFit.Gradient();
    if (AdjustmentMultiplier == 0)
    {
        SE_WARNING("fixing AdjustmentMultiplier 0->1\n");
        AdjustmentMultiplier = 1;
    }

    // The observed gradient should not be updated at the end of the function (we want to observe its pre-processed value)
    Context->Stream->Statistics().OutputRateGradient[i] = GradientToPartsPerMillion(AdjustmentMultiplier);
    SE_DEBUG(group_avsync, "clock (%d-%d) IntegrationCount #%d Gradient: %d.%09d (%dppm) CurrentError: %lld\n",
             Context->StreamType, i, State->IntegrationCount, AdjustmentMultiplier.IntegerPart(),
             AdjustmentMultiplier.RemainderDecimal(9), GradientToPartsPerMillion(AdjustmentMultiplier), CurrentError);
    //
    // Now apply this to the clock adjustment
    //
    State->ClockAdjustment  = State->ClockAdjustmentEstablished ? (State->ClockAdjustment * AdjustmentMultiplier) : AdjustmentMultiplier;
    State->ClockAdjustmentEstablished   = true;
}

// /////////////////////////////////////////////////////////////////////////
//
//      The function to calculate the clock rate adjustment values,
//      and system clock, and for any subsiduary clocks.
//

OutputCoordinatorStatus_t OutputCoordinator_Base_c::CalculateOutputRateAdjustments(
    OutputCoordinatorContext_t    Context,
    OutputTiming_t           *OutputTiming,
    Rational_t          **ParamOutputRateAdjustments,
    Rational_t           *ParamSystemClockAdjustment)
{
    unsigned int                 i;
    bool                     NewOutputRateAdjustmentType;
    bool                     ClockMaster;
    OutputSurfaceDescriptor_t       *Surface;
    TimingContext_t             *State;
    ManifestationOutputTiming_t     *Timing;
    unsigned long long           ExpectedTime;
    unsigned long long           ActualTime;
    long long                    ExpectedDuration;
    long long                    ActualDuration;
    long long                    Difference;
    long long int                CurrentError;
    // Lock the component to thread-safely use ClockRecovery variables
    ComponentMonitor(this);

    if (Context == PlaybackContext)
    {
        SE_ERROR("Attempt to calculate output rate adjustments for 'PlaybackContext'\n");
        return PlayerNotSupported;
    }

    //
    // If called without an output timing this function just returns the current values
    //

    if (OutputTiming == NULL)
    {
        *ParamOutputRateAdjustments     = Context->OutputRateAdjustments;
        *ParamSystemClockAdjustment     = SystemClockAdjustment;
        return OutputCoordinatorNoError;
    }

    //
    // Load in the surface descriptors, and verify the master clock
    //
    VerifyMasterClock();
    //
    // Are we using the correct set of parameters
    //
    int ExternalMapping         = PolicyValueDisapply;
    NewOutputRateAdjustmentType     = false;

    if (Context->OutputRateAdjustmentType == OutputRateAdjustmentNotDetermined)
    {
        ExternalMapping             = Player->PolicyValue(Playback, Context->Stream, PolicyExternalTimeMapping);
        if (((ExternalMapping == PolicyValueApply) && UseSystemClockAsMaster) ||
            (Player->PolicyValue(Playback, Stream, PolicyHdmiRxMode) != PolicyValueHdmiRxModeDisabled))
        {
            if (Context->StreamType == StreamTypeVideo)
            {
                Context->OutputRateAdjustmentParameters = ORAInputFollowingVideo;
            }
            else
            {
                Context->OutputRateAdjustmentParameters = ORAInputFollowingAudio;
            }

        }
        else
        {
            if (Context->StreamType == StreamTypeVideo)
            {
                Context->OutputRateAdjustmentParameters = ORAOutputDrivenVideo;
            }
            else
            {
                Context->OutputRateAdjustmentParameters = ORAOutputDrivenAudio;
            }
        }
        Context->OutputRateAdjustmentType   = (ExternalMapping == PolicyValueApply) ? OutputRateAdjustmentInputFollowing : OutputRateAdjustmentOutputDriven;
        NewOutputRateAdjustmentType     = true;
    }

    // ///////////////////////////////////////////////////////////////////////////////////////
    //
    // Loop processing each individual surface
    //

    for (i = 0; i <= Context->MaxSurfaceIndex; i++)
    {
        Surface     = Context->Surfaces[i];
        State       = &Context->TimingState[i];
        Timing      = &OutputTiming->ManifestationTimings[i];
        ClockMaster = Context->ClockMaster && (i == Context->ClockMasterIndex) && !UseSystemClockAsMaster;

        //
        // Do we need to reset the integration after a new data setting
        //

        if (NewOutputRateAdjustmentType)
        {
            State->IntegratingClockDrift    = (ExternalMapping == PolicyValueApply) &&
                                              Context->ClockMaster &&
                                              (i == Context->ClockMasterIndex);
            State->FramesToIntegrateOver    = Context->OutputRateAdjustmentParameters.ClockDriftMinimumIntegrationFrames;
            Context->Stream->Statistics().OutputRateFramesToIntegrateOver[i] = State->FramesToIntegrateOver;
        }

        ExpectedTime        = Timing->SystemPlaybackTime;
        ActualTime      = Timing->ActualSystemPlaybackTime;
        CurrentError        = (long long)(ExpectedTime - ActualTime);
        ExpectedDuration    = ExpectedTime - State->LastExpectedTime;
        ActualDuration      = ActualTime - State->LastActualTime;
        Difference      = (long long)(ExpectedDuration - ActualDuration);

        if (!Timing->TimingValid || NotValidTime(ExpectedTime) || NotValidTime(ActualTime))
        {
            continue;
        }

        // SE-PIPELINE TRACE
        SE_VERBOSE(group_avsync, "clock(%d-%d) #%d ExpectedDuration=%lld ActualDuration=%lld Difference %lld",
                   Context->StreamType, i, State->IntegrationCount, ExpectedDuration, ActualDuration, Difference);
        SE_VERBOSE(group_avsync, "clock(%d-%d) #%d ExpectedTime = %lld ActualTime = %lld CurrentError = %lld\n",
                   Context->StreamType, i, State->IntegrationCount, ExpectedTime, ActualTime, CurrentError);

        if ((Surface == NULL) || (!ClockMaster && !Surface->ClockPullingAvailable))
        {
            Context->OutputRateAdjustments[i]   = 1;
            continue;
        }

        if (Abs(Difference) > min(Abs(ExpectedDuration), Abs(ActualDuration)))
        {
            continue;
        }

        State->LastExpectedTime = ExpectedTime;
        State->LastActualTime   = ActualTime;
        //
        // Record the error history, and if we have no useful information
        // from the master, just go with default values.
        // Average the last 4 error to discover what we should aim to correct for.
        //
        State->CurrentErrorHistory[3]   = State->CurrentErrorHistory[2];
        State->CurrentErrorHistory[2]   = State->CurrentErrorHistory[1];
        State->CurrentErrorHistory[1]   = State->CurrentErrorHistory[0];
        State->CurrentErrorHistory[0]   = CurrentError;
        CurrentError            = (State->CurrentErrorHistory[3] + State->CurrentErrorHistory[2] +
                                   State->CurrentErrorHistory[1] + State->CurrentErrorHistory[0]) / 4;

        if (!ClockMaster && !SystemClockAdjustmentEstablished)
        {
            continue;
        }

        //
        // Now perform the integration or ignore
        //
        State->IntegrationCount++;

        if (State->IntegratingClockDrift)
        {
            if (inrange(Difference, -Context->OutputRateAdjustmentParameters.MaximumJitterDifference, Context->OutputRateAdjustmentParameters.MaximumJitterDifference))
            {
                State->LeastSquareFit.Add(ActualDuration, ExpectedDuration);
            }

            if (State->IntegrationCount >= State->FramesToIntegrateOver)
            {
                CalculateAdjustmentMultiplier(Context, State, CurrentError, i);

                if (ClockMaster)
                {
                    Rational_t  SysChange;
                    long long   Jerk;
                    SysChange   = (1 / State->ClockAdjustment) - SystemClockAdjustment;
                    Jerk    = LongLongIntegerPart(SysChange * (OS_GetTimeInMicroSeconds() - MasterBaseSystemTime)) / 2;

                    if (!inrange(Jerk, -10000, 10000))
                    {
                        SE_WARNING("Big Jerk R%d- %lld - Old %d.%09d, New %d.%09d, Change %d.%09d - TP %lld\n",
                                   Context->StreamType, Jerk,
                                   SystemClockAdjustment.IntegerPart(), SystemClockAdjustment.RemainderDecimal(9),
                                   State->ClockAdjustment.IntegerPart(),  State->ClockAdjustment.RemainderDecimal(9),
                                   SysChange.IntegerPart(), SysChange.RemainderDecimal(9),
                                   (OS_GetTimeInMicroSeconds() - MasterBaseSystemTime));
                    }

                    MasterBaseSystemTime    -= Jerk;
                    CurrentError        -= Jerk;
                }

                CalculateLongTermDriftCorrector(Context, State, CurrentError, i);
                SE_INFO(group_avsync, "Output rate adjustment (%s,%d): %d ppm (%d.%09d)\n",
                        StreamType(), i, GradientToPartsPerMillion(State->ClockAdjustment),
                        State->ClockAdjustment.IntegerPart(), State->ClockAdjustment.RemainderDecimal(9));

                //
                // If we are the master then transfer this to the system clock adjustment
                // but inverted, if we needed to speed up, we do this by slowing the system clock
                // if we needed to slow down, we do this by speeding up the system clock.
                //

                if (ClockMaster)
                {
                    SystemClockAdjustment               = 1 / State->ClockAdjustment;
                    SystemClockAdjustmentEstablished    = true;
                    Context->OutputRateAdjustments[i]   = 1;
                }

                //
                // In the case where we are not the master recalculate the return value.
                //

                if (!ClockMaster && SystemClockAdjustmentEstablished)
                {
                    Context->OutputRateAdjustments[i] = State->ClockAdjustment;
                }

                //
                // Finally prepare for the ignoring session.
                //
                State->LastIntegrationWasRestarted  = false;
                State->IntegratingClockDrift        = false;

                if (State->FramesToIntegrateOver < Context->OutputRateAdjustmentParameters.ClockDriftMaximumIntegrationFrames)
                {
                    State->FramesToIntegrateOver   *= 2;
                }

                State->IntegrationCount         = 0;
            }
        }
        else if (State->IntegrationCount >= Context->OutputRateAdjustmentParameters.ClockDriftIgnoreBetweenIntegrations)
        {
            //
            // Prepare to integrate over the next set of frames
            //
            State->IntegratingClockDrift    = true;
            State->IntegrationCount     = 0;
            State->LeastSquareFit.Reset();
        }

        //
        // Update statistics
        //
        Context->Stream->Statistics().OutputRateFramesToIntegrateOver[i] = State->FramesToIntegrateOver;
        Context->Stream->Statistics().OutputRateIntegrationCount[i] = State->IntegrationCount;
        Context->Stream->Statistics().OutputRateClockAdjustment[i] = GradientToPartsPerMillion(State->ClockAdjustment);

        // Enable the capture of clock-data points through strelay to easy debugging of the clock-recovery algorithm
        // It provides a mean to record all the data-points and intermediate gradient computation that can then
        // be reviewed later by loading the data in a spreadsheet and post-process them
        {
            struct data_point_t
            {
                OutputCoordinatorContext_t Context;
                uint32_t                   SurfaceIndex;
                uint64_t                   ExpectedTime;
                uint64_t                   ActualTime;
                int64_t                    ExpectedDuration;
                int64_t                    ActualDuration;
                int                        OutputRateAdjustment_int;
                int                        OutputRateAdjustment_dec;

                int                        OutputRateGradient;
                unsigned int               OutputRateFramesToIntegrateOver;
                unsigned int               OutputRateIntegrationCount;
                int                        OutputRateClockAdjustment;
            } DataPoint =
            {
                Context, i,
                ExpectedTime    , ActualTime,
                ExpectedDuration, ActualDuration,
                State->ClockAdjustment.IntegerPart(),
                State->ClockAdjustment.RemainderDecimal(9),
                Context->Stream->Statistics().OutputRateGradient[i],
                Context->Stream->Statistics().OutputRateFramesToIntegrateOver[i],
                Context->Stream->Statistics().OutputRateIntegrationCount[i],
                Context->Stream->Statistics().OutputRateClockAdjustment[i],
            };

            if (Context->StreamType == StreamTypeAudio)
            {
                st_relayfs_write_se(ST_RELAY_TYPE_AUDIO_CLOCK_DATAPOINT, ST_RELAY_SOURCE_SE,
                                    (unsigned char *)&DataPoint, sizeof(DataPoint), 0);
            }
            else
            {
                st_relayfs_write_se(ST_RELAY_TYPE_VIDEO_CLOCK_DATAPOINT, ST_RELAY_SOURCE_SE,
                                    (unsigned char *)&DataPoint, sizeof(DataPoint), 0);
            }
        }
    } // end foreach Surface

    //
    // Finally set the return parameters
    //
    *ParamOutputRateAdjustments     = Context->OutputRateAdjustments;
    *ParamSystemClockAdjustment     = SystemClockAdjustment;
    return OutputCoordinatorNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
//      The function to restart the output rate adjustment
//

OutputCoordinatorStatus_t   OutputCoordinator_Base_c::RestartOutputRateIntegration(
    OutputCoordinatorContext_t  Context,
    bool                ResetCount,
    unsigned int            TimingIndex)
{
    unsigned int       i;
    unsigned int         LoopStart;
    unsigned int         LoopEnd;
    TimingContext_t     *State;

    LoopStart = (TimingIndex == INVALID_INDEX) ? 0 : TimingIndex;
    LoopEnd   = (TimingIndex == INVALID_INDEX) ? MAXIMUM_MANIFESTATION_TIMING_COUNT : (TimingIndex + 1);

    for (i = LoopStart; i < LoopEnd; i++)
    {
        State   = &Context->TimingState[i];

        //
        // Do we wish to stick with the current integration period
        //

        if (ResetCount)
        {
            State->FramesToIntegrateOver  = Context->OutputRateAdjustmentParameters.ClockDriftMinimumIntegrationFrames;
        }
        else if (State->LastIntegrationWasRestarted)
        {
            State->FramesToIntegrateOver  = max((State->FramesToIntegrateOver / 2), Context->OutputRateAdjustmentParameters.ClockDriftMinimumIntegrationFrames);
        }

        State->IntegratingClockDrift        = false;
        State->IntegrationCount             = 0;
        State->LastIntegrationWasRestarted  = true;
        Context->Stream->Statistics().OutputRateFramesToIntegrateOver[i] = State->FramesToIntegrateOver;
        Context->Stream->Statistics().OutputRateIntegrationCount[i] = State->IntegrationCount;
    }

    return OutputCoordinatorNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
//      The function to restart the output rate adjustment
//

OutputCoordinatorStatus_t   OutputCoordinator_Base_c::ResetTimingContext(
    OutputCoordinatorContext_t  Context,
    unsigned int            TimingIndex)
{
    unsigned int         i;
    unsigned int         LoopStart;
    unsigned int         LoopEnd;
    TimingContext_t     *State;

    LoopStart = (TimingIndex == INVALID_INDEX) ? 0 : TimingIndex;
    LoopEnd   = (TimingIndex == INVALID_INDEX) ? MAXIMUM_MANIFESTATION_TIMING_COUNT : (TimingIndex + 1);

    for (i = LoopStart; i < LoopEnd; i++)
    {
        State   = &Context->TimingState[i];
        State->LastIntegrationWasRestarted  = false;
        State->IntegratingClockDrift        = false;
        State->FramesToIntegrateOver        = Context->OutputRateAdjustmentParameters.ClockDriftMinimumIntegrationFrames;
        State->IntegrationCount         = 0;
        State->LeastSquareFit.Reset();
        State->LastExpectedTime         = INVALID_TIME;
        State->LastActualTime           = INVALID_TIME;
        State->ClockAdjustmentEstablished   = false;
        State->ClockAdjustment          = 1;
        memset(State->CurrentErrorHistory, 0, 4 * sizeof(long long int));

        State->VsyncOffsetIntegrationCount  = 0;
        State->MinimumVsyncOffset       = 0x7fffffffffffffffll;
        State->VideoFrameDuration       = 41666;        // Give a 24fps default value

        Context->OutputRateAdjustments[i]   = 1;
        Context->VsyncOffsets[i]        = 0;
        // reset statistics
        Context->Stream->Statistics().OutputRateFramesToIntegrateOver[i] = State->FramesToIntegrateOver;
        Context->Stream->Statistics().OutputRateIntegrationCount[i] = State->IntegrationCount;
    }

    return OutputCoordinatorNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
//      The function to adjust the base system time
//

OutputCoordinatorStatus_t   OutputCoordinator_Base_c::AdjustMappingBase(
    OutputCoordinatorContext_t        Context,
    long long                         Adjustment,
    bool                              ResetVSyncOffsets)
{
    OutputCoordinatorContext_t      ContextLoop;
    ComponentMonitor(this);
    //
    // Perform adjustment
    //
    SE_INFO(group_avsync, "AdjustMappingBase - %p %d - %lld\n", Context, (Context != NULL) ? Context->StreamType : 0, Adjustment);

    if (MasterTimeMappingEstablished)
    {
        MasterBaseSystemTime            += Adjustment;
    }

    for (ContextLoop     = CoordinatedContexts;
         ContextLoop    != NULL;
         ContextLoop     = ContextLoop->Next)
    {
        if (ContextLoop->TimeMappingEstablished)
        {
            ContextLoop->BaseSystemTime     += Adjustment;
            ContextLoop->BaseSystemTimeAdjusted  = (ContextLoop != Context);    // Tell everyone else it has been adjusted
            ContextLoop->VsyncOffsetActionsDone = false;
            RestartOutputRateIntegration(ContextLoop, false);
        }
    }

    return OutputCoordinatorNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
//      The function to check if the base system time has been adjusted
//

OutputCoordinatorStatus_t   OutputCoordinator_Base_c::MappingBaseAdjustmentApplied(
    OutputCoordinatorContext_t        Context)
{
    OutputCoordinatorStatus_t  Status = OutputCoordinatorNoError;

    if (Context->BaseSystemTimeAdjusted)
    {
        Context->BaseSystemTimeAdjusted = false;
        Status                          = OutputCoordinatorMappingAdjusted;
    }

    return Status;
}


// /////////////////////////////////////////////////////////////////////////
//
//      The function to return the delay between the first frame of this
//      stream, and the first frame from any stream in this playback.
//

OutputCoordinatorStatus_t   OutputCoordinator_Base_c::GetStreamStartDelay(
    OutputCoordinatorContext_t        Context,
    unsigned long long               *Delay)
{
    if (NotValidTime((unsigned long long)Context->StreamOffset))
    {
        return OutputCoordinatorMappingNotEstablished;
    }

    *Delay      = (unsigned long long)Context->StreamOffset;
    return OutputCoordinatorNoError;
}

// /////////////////////////////////////////////////////////////////////////
//
//      This function monitors the offset between VSYNCs and the output
//      times of frames.
//

OutputCoordinatorStatus_t   OutputCoordinator_Base_c::MonitorVsyncOffsets(
    OutputCoordinatorContext_t    Context,
    OutputTiming_t           *OutputTiming,
    long long int           **ParamVsyncOffsets)
{
    bool UpdatedVsyncOffsets        = false;
    UpdateVsyncOffsets(Context, OutputTiming, &UpdatedVsyncOffsets);

    if (UpdatedVsyncOffsets && !Context->VsyncOffsetActionsDone)
    {
        int ExternalMapping = Player->PolicyValue(Playback, Context->Stream, PolicyExternalTimeMapping);

        if (ExternalMapping == PolicyValueApply)
        {
            MonitorVsyncOffsetsExternalMapping(Context);
        }
        else
        {
            MonitorVsyncOffsetsInternalMapping(Context);
        }

        Context->VsyncOffsetActionsDone = true;
    }

    return Context->VsyncOffsetActionsDone ? OutputCoordinatorNoError : OutputCoordinatorOffsetsNotEstablished;
}

void OutputCoordinator_Base_c::UpdateVsyncOffsets(
    OutputCoordinatorContext_t    Context,
    OutputTiming_t           *OutputTiming,
    bool *UpdatedVsyncOffsets)

{
    //
    // Loop processing each individual surface
    //
    for (unsigned int i = 0; i <= Context->MaxSurfaceIndex; i++)
    {
        OutputSurfaceDescriptor_t      *Surface = Context->Surfaces[i];
        TimingContext_t                *State   = &Context->TimingState[i];
        ManifestationOutputTiming_t    *Timing  = &OutputTiming->ManifestationTimings[i];

        if ((Surface == NULL) || !Timing->TimingValid || NotValidTime(Timing->ActualSystemPlaybackTime))
        {
            Context->VsyncOffsets[i]    = 0;
            continue;
        }

        if (!Context->VsyncOffsetActionsDone && (State->VsyncOffsetIntegrationCount >= (IGNORE_COUNT_FOR_VSYNC_OFFSET + INTEGRATION_COUNT_FOR_VSYNC_OFFSET)))
        {
            State->VsyncOffsetIntegrationCount    = 0;
            State->MinimumVsyncOffset             = 0x7fffffffffffffffll;
        }

        State->VsyncOffsetIntegrationCount++;

        if (inrange(State->VsyncOffsetIntegrationCount,
                    IGNORE_COUNT_FOR_VSYNC_OFFSET + 1,
                    IGNORE_COUNT_FOR_VSYNC_OFFSET + INTEGRATION_COUNT_FOR_VSYNC_OFFSET))
        {
            long long int Offset = (long long int)(Timing->ActualSystemPlaybackTime - Timing->SystemPlaybackTime);

            if (Offset < State->MinimumVsyncOffset)
            {
                SE_VERBOSE(group_avsync, "Surface #%d: MinimumVsyncOffset=%lld\n", i, Offset);
                State->MinimumVsyncOffset   = Offset;
            }

            if (State->VsyncOffsetIntegrationCount == (IGNORE_COUNT_FOR_VSYNC_OFFSET + INTEGRATION_COUNT_FOR_VSYNC_OFFSET))
            {
                Context->VsyncOffsets[i]        = State->MinimumVsyncOffset;
                *UpdatedVsyncOffsets            = true;
            }
        }
    }
}

// /////////////////////////////////////////////////////////////////////////
//
//      Private - This function retrieve the Surface with on which
//      the BestQuiality should be apply according its OutputType
//
signed int OutputCoordinator_Base_c::RetrieveBestQualitySurface(OutputCoordinatorContext_t Context)
{
    signed int CurrentSelectedPlane = -1;

    for (unsigned int i = 0; i <= Context->MaxSurfaceIndex; i++)
    {
        if (Context->Surfaces[i] != NULL)
        {
            switch (Context->Surfaces[i]->OutputType)
            {
            case SURFACE_OUTPUT_TYPE_VIDEO_MAIN:
                // MAIN (HDMi) is always selected
                CurrentSelectedPlane =  i;
                break;
            case SURFACE_OUTPUT_TYPE_VIDEO_AUX:
            case SURFACE_OUTPUT_TYPE_VIDEO_PIP:
                // We cannot have PIP and AUX at the same time
                if (CurrentSelectedPlane == -1) { CurrentSelectedPlane = i; }
                break;
            default:
                break;
            }
        }
    }

    // Here we should have found a candidate
    if (CurrentSelectedPlane == -1)
    {
        SE_WARNING("No Best Quality plane found\n");
    }
    else
    {
        SE_INFO(group_avsync, "Using Surface #%d as  OutputType=%d ClockPullingAvailable=%d (VsyncOffset=%lldus)\n",
                CurrentSelectedPlane, Context->Surfaces[CurrentSelectedPlane]->OutputType, Context->Surfaces[CurrentSelectedPlane]->ClockPullingAvailable, Context->VsyncOffsets[CurrentSelectedPlane]);
    }


    return CurrentSelectedPlane;
}

void OutputCoordinator_Base_c::MonitorVsyncOffsetsExternalMapping(
    OutputCoordinatorContext_t    Context)
{
    PlayerEventRecord_t Event;
    int VsyncLocked   = Player->PolicyValue(Playback, Context->Stream, PolicyExternalTimeMappingVsyncLocked);
    long long int MinimumOffset = 0x7fffffffffffffffll;

    for (unsigned int i = 0; i <= Context->MaxSurfaceIndex; i++)
    {
        if ((Context->Surfaces[i] != NULL) && (Context->VsyncOffsets[i] < MinimumOffset))
        {
            MinimumOffset = Context->VsyncOffsets[i];
        }
    }

    if (VsyncLocked != PolicyValueApply)
    {
        //
        // Not locked so adjust the mapping, and Normalize offset to 0..frame period
        //
        SE_INFO(group_avsync, "Vsync Offset = %lldus\n", MinimumOffset);
        AdjustMappingBase(PlaybackContext, MinimumOffset, false);
    }
    else
    {
        //
        // Vsync locked, so just report, and signal zero
        //
        SE_INFO(group_avsync, "Vsync Offset = %lldus, as vsync is locked we will not adjust the mapping\n",
                MinimumOffset);
        MinimumOffset   = 0;
    }

    //
    // Raise a player event to signal the measurement of vsync offset
    //
    Event.Code                  = EventVsyncOffsetMeasured;
    Event.Playback              = Playback;
    Event.Stream                = Context->Stream;
    Event.PlaybackTime          = TIME_NOT_APPLICABLE;
    Event.UserData              = EventUserData;
    Event.Value[0].LongLong     = (unsigned long long)MinimumOffset;
    PlayerStatus_t Status   = Stream->SignalEvent(&Event);

    if (Status != PlayerNoError) { SE_ERROR("Failed to signal event\n"); }
}

void   OutputCoordinator_Base_c::MonitorVsyncOffsetsInternalMapping(
    OutputCoordinatorContext_t    Context)
{
    long long int   SurfaceOffset     = 0;
    signed int      SurfaceIndexBestQuality;


    // Get the Surface with Best Quality
    SurfaceIndexBestQuality = RetrieveBestQualitySurface(Context);

    if (SurfaceIndexBestQuality != -1)
    {
        SurfaceOffset = Context->VsyncOffsets[SurfaceIndexBestQuality];

        SE_INFO(group_avsync, "Vsync Offset = %lldus\n", SurfaceOffset);

        AdjustMappingBase(PlaybackContext, SurfaceOffset, false);
    }
}

// /////////////////////////////////////////////////////////////////////////
//
//      Private - This function trawls the contexts and deduces the next
//      system time at which we can start playing.
//

OutputCoordinatorStatus_t   OutputCoordinator_Base_c::GenerateTimeMapping(void)
{
    unsigned int              i;
    bool                  VideoStartImmediate;
    OutputCoordinatorContext_t        LoopContext;
    unsigned long long       *ManifestationTimes;
    unsigned long long       *ManifestationGranularities;
    unsigned long long        EarliestPlaybackTime;
    unsigned long long        EarliestVideoPlaybackTime;
    unsigned long long        LatestPlaybackTime;
    unsigned long long        Now;
    unsigned long long        BaseStartTime;
    unsigned long long        LatestBaseStartTime;
    unsigned long long        LatestBaseStartTimePlaybackTime;
    unsigned long long        LatestBaseVideoStartTime;
    unsigned long long        LatestBaseVideoStartTimePlaybackTime;
    unsigned long long        LargestGranularity;
    //
    // Calculate the stream offset values
    //
    EarliestPlaybackTime    = 0x7fffffffffffffffull;
    EarliestVideoPlaybackTime   = 0x7fffffffffffffffull;
    LatestPlaybackTime      = 0;

    for (LoopContext     = CoordinatedContexts;
         LoopContext    != NULL;
         LoopContext     = LoopContext->Next)
    {
        if (LoopContext->InSynchronizeFn)
        {
            if (LoopContext->SynchronizingAtPlaybackTime < EarliestPlaybackTime)
            {
                EarliestPlaybackTime      = LoopContext->SynchronizingAtPlaybackTime;
            }

            if ((LoopContext->StreamType == StreamTypeVideo) &&
                LoopContext->SynchronizingAtPlaybackTime < EarliestVideoPlaybackTime)
            {
                EarliestVideoPlaybackTime = LoopContext->SynchronizingAtPlaybackTime;
            }

            if (LoopContext->SynchronizingAtPlaybackTime > LatestPlaybackTime)
            {
                LatestPlaybackTime        = LoopContext->SynchronizingAtPlaybackTime;
            }
        }
    }

    for (LoopContext     = CoordinatedContexts;
         LoopContext    != NULL;
         LoopContext     = LoopContext->Next)
    {
        if (LoopContext->InSynchronizeFn)
        {
            LoopContext->StreamOffset = LoopContext->SynchronizingAtPlaybackTime - EarliestPlaybackTime;
        }
    }

    MaximumStreamOffset = (LatestPlaybackTime - EarliestPlaybackTime);
    //
    // Now scan the streams (and recursively the manifestors associated
    // with them), to find the earliest start time for playback.
    //
    Now                     = OS_GetTimeInMicroSeconds();
    LatestBaseStartTime             = Now - (LatestPlaybackTime - EarliestPlaybackTime);
    LatestBaseStartTimePlaybackTime     = EarliestPlaybackTime;
    LatestBaseVideoStartTime            = Now - (LatestPlaybackTime - EarliestVideoPlaybackTime);
    LatestBaseVideoStartTimePlaybackTime    = EarliestVideoPlaybackTime;
    LargestGranularity              = 1;

    for (LoopContext  = CoordinatedContexts;
         LoopContext != NULL;
         LoopContext  = LoopContext->Next)
    {
        //
        // Get the manifestation times,.
        //
        LoopContext->ManifestorLatency  = MINIMUM_TIME_TO_MANIFEST;
        LoopContext->ManifestationCoordinator->GetNextQueuedManifestationTimes(&ManifestationTimes, &ManifestationGranularities);

        if (LoopContext->InSynchronizeFn)
        {
            //
            // Cycle through the manifestation times to find the latest, and the largest granularity
            //
            for (i = 0; i <= LoopContext->MaxSurfaceIndex; i++)
                if (ValidTime(ManifestationTimes[i]))
                {
                    BaseStartTime           = ManifestationTimes[i] - LoopContext->StreamOffset;

                    SE_DEBUG(group_avsync, "StreamType=%d LatestBaseStartTime=%llu BaseStartTime=%llu ManifestationTimes[%d]=%llu StreamOffset=%llu\n",
                             LoopContext->StreamType, LatestBaseStartTime, BaseStartTime, i, ManifestationTimes[i], LoopContext->StreamOffset);

                    if (BaseStartTime > LatestBaseStartTime)
                    {
                        LatestBaseStartTime         = BaseStartTime;
                    }

                    if ((LoopContext->StreamType == StreamTypeVideo) &&
                        (BaseStartTime + (EarliestVideoPlaybackTime - EarliestPlaybackTime) > LatestBaseVideoStartTime))
                    {
                        LatestBaseVideoStartTime        = BaseStartTime + (EarliestVideoPlaybackTime - EarliestPlaybackTime);
                    }

                    if (ManifestationGranularities[i] > LargestGranularity)
                    {
                        LargestGranularity          = ManifestationGranularities[i];
                    }

                    if ((long long)(ManifestationTimes[i] - Now) > (long long)LoopContext->ManifestorLatency)
                    {
                        LoopContext->ManifestorLatency        = ManifestationTimes[i] - Now;
                    }
                }
        }
        else
        {
            //
            // Not reached synchronize, just calculate the manifestor latency for this stream
            //
            for (i = 0; i <= LoopContext->MaxSurfaceIndex; i++)
                if (ValidTime(ManifestationTimes[i]))
                {
                    if ((long long)(ManifestationTimes[i] - Now) > (long long)LoopContext->ManifestorLatency)
                    {
                        LoopContext->ManifestorLatency        = ManifestationTimes[i] - Now;
                    }
                }
        }

        //
        // Warn if some of these numbers are a tad dubious
        //

        if (LoopContext->ManifestorLatency > 1000000ull)
        {
            SE_WARNING("Long latency %12lldus for stream type %d\n", LoopContext->ManifestorLatency, LoopContext->StreamType);
        }

        //
        // Allow the vsync monitoring to run again
        //
        LoopContext->VsyncOffsetActionsDone     = false;
    }

    //
    // Apply a delay, to compensate for variable latency in the a live input stream.
    //
    int LatencyVariability  = Player->PolicyValue(Playback, PlayerAllStreams, PolicyLivePlaybackLatencyVariabilityMs);

    if ((LatencyVariability != 0) &&
        (Player->PolicyValue(Playback, PlayerAllStreams, PolicyLivePlayback) == PolicyValueApply))
    {
        LatestBaseStartTime += ((unsigned long long)LatencyVariability) * 1000; //making ms to micro second
        SE_INFO(group_avsync, "LivePlaybackLatency:%dms\n", LatencyVariability);
    }

    //
    // Revisit the result for video start immediate
    //
    VideoStartImmediate = (Player->PolicyValue(Playback, PlayerAllStreams, PolicyVideoStartImmediate) == PolicyValueApply) &&
                          !(Player->PolicyValue(Playback, PlayerAllStreams, PolicyLivePlayback) == PolicyValueApply);

    if (VideoStartImmediate && (EarliestVideoPlaybackTime != 0x7fffffffffffffffull))
    {
        LatestBaseStartTime         = LatestBaseVideoStartTime;
        LatestBaseStartTimePlaybackTime     = LatestBaseVideoStartTimePlaybackTime;
        SE_INFO(group_avsync, "VideoStartImmediate is set -> LatestBaseStartTime=%llu LatestBaseStartTimePlaybackTime=%llu\n",
                LatestBaseStartTime, LatestBaseStartTimePlaybackTime);
    }

    //
    // Now generate the mapping (Ta Dah!!)
    //
    MasterBaseNormalizedPlaybackTime                    = LatestBaseStartTimePlaybackTime;
    MasterBaseSystemTime                                = LatestBaseStartTime;
    AccumulatedPlaybackTimeJumpsSinceSynchronization    = 0;
    JumpSeenAtPlaybackTime                              = INVALID_TIME;
    MasterTimeMappingEstablished                        = true;
    MasterTimeMappingVersion++;

    return OutputCoordinatorNoError;
}

OutputCoordinatorStatus_t   OutputCoordinator_Base_c::GenerateTimeMappingWithPCR(void)
{
    uint32_t i;
    OutputCoordinatorContext_t  LoopContext;
    uint64_t                   *ManifestationTimes;
    uint64_t                   *ManifestationGranularities;
    uint64_t                    Now;
    uint64_t                    LatestBaseStartTime;
    uint64_t                    LatestBaseStartTimePlaybackTime;
    uint64_t                    MaxManifestorLatency;

    Now = OS_GetTimeInMicroSeconds();

    // Use the PCR info as the most reliable info.
#define TYPICAL_VIDEO_DECODE_LATENCY (2 *  1000000/  25)  // 2     video frames at 25 Hz
    LatestBaseStartTime                   = ClockRecoveryLastLocalTime  + TYPICAL_VIDEO_DECODE_LATENCY;
    LatestBaseStartTimePlaybackTime       = ClockRecoveryLastSourceTime;

    SE_INFO(group_avsync, "Time mapping has been established using PCR  (SourceTime=%llu, SystemTime=%llu)\n",
            ClockRecoveryLastSourceTime, ClockRecoveryLastLocalTime);

    // The loops below searches for the worst case latency; start with an
    // optimistic assumption which will be updated by the loop.
    MaxManifestorLatency = MINIMUM_TIME_TO_MANIFEST;

    for (LoopContext     = CoordinatedContexts;
         LoopContext    != NULL;
         LoopContext     = LoopContext->Next)
    {
        //
        // Get the maximum manifestor latency
        //
        LoopContext->ManifestorLatency  = MINIMUM_TIME_TO_MANIFEST;
        LoopContext->ManifestationCoordinator->GetNextQueuedManifestationTimes(&ManifestationTimes, &ManifestationGranularities);

        for (i = 0; i <= LoopContext->MaxSurfaceIndex; i++)
        {
            if (ValidTime(ManifestationTimes[i]))
            {
                if ((long long)(ManifestationTimes[i] - Now) > (long long)LoopContext->ManifestorLatency)
                {
                    LoopContext->ManifestorLatency        = ManifestationTimes[i] - Now;
                }
            }
        }

        //
        // Warn if some of these numbers are a tad dubious
        //

        if (LoopContext->ManifestorLatency > 1000000ull)
        {
            SE_INFO(group_avsync, "Long latency %12lldus for stream type %d\n", LoopContext->ManifestorLatency, LoopContext->StreamType);
        }
        else
        {
            MaxManifestorLatency = (LoopContext->ManifestorLatency > MaxManifestorLatency) ? LoopContext->ManifestorLatency : MaxManifestorLatency;
        }

        //
        // Allow the vsync monitoring to run again
        //
        LoopContext->VsyncOffsetActionsDone     = false;
    }

    //
    // Apply a Maximum manifestor latency to the mapping derived from the PCR.
    //
    LatestBaseStartTime += MaxManifestorLatency;
    //
    // Apply a delay, to compensate for variable latency in the a live input stream.
    //
    int LatencyVariability  = Player->PolicyValue(Playback, PlayerAllStreams, PolicyLivePlaybackLatencyVariabilityMs);

    if ((LatencyVariability != 0) &&
        (Player->PolicyValue(Playback, PlayerAllStreams, PolicyLivePlayback) == PolicyValueApply))
    {
        LatestBaseStartTime += ((unsigned long long)LatencyVariability) * 1000; //making ms to micro second
        SE_INFO(group_avsync, "LivePlaybackLatency:%dms\n", LatencyVariability);
    }

    //
    // Now generate the mapping (Ta Dah!!)
    //
    SE_INFO(group_avsync, "Original map %llu %llu   New map %llu %llu   Delta %lld\n",
            MasterBaseNormalizedPlaybackTime, MasterBaseSystemTime,
            LatestBaseStartTimePlaybackTime , LatestBaseStartTime ,
            LatestBaseStartTime - (MasterBaseSystemTime +
                                   (LatestBaseStartTimePlaybackTime - MasterBaseNormalizedPlaybackTime)));
    MasterBaseNormalizedPlaybackTime            = LatestBaseStartTimePlaybackTime;
    MasterBaseSystemTime                        = LatestBaseStartTime;
    return OutputCoordinatorNoError;
}

void   OutputCoordinator_Base_c::GenerateTimeMappingForHdmiRx(OutputCoordinatorContext_t    Context, void *ParsedAudioVideoDataParameters)
{
    unsigned long long EarliestPlaybackTime = Context->SynchronizingAtPlaybackTime;
    LatencyUsedInHDMIRxModeDuringTimeMapping = GetLargestOfMinimumManifestationLatencies(ParsedAudioVideoDataParameters);
    SE_INFO(group_avsync, "Applying %lld Latency for HdmiRx\n", LatencyUsedInHDMIRxModeDuringTimeMapping);
    EarliestPlaybackTime += LatencyUsedInHDMIRxModeDuringTimeMapping;
    //
    // Now generate the mapping
    //
    MasterBaseSystemTime = EarliestPlaybackTime;
}

long long OutputCoordinator_Base_c::GetLargestOfMinimumManifestationLatencies(void *ParsedAudioVideoDataParameters)
{
    OutputCoordinatorContext_t  LoopContext;
    unsigned long long       *ManifestationTimes;
    unsigned long long       *ManifestationGranularities;
    uint64_t                  Now;
    long long                 MaxVideoManifestorLatency = 0;
    long long                 MaxVideoInputLatency = 0;
    long long                 LargestOfMinimumManifestationLatencies = 0;
    Now = OS_GetTimeInMicroSeconds();

    for (LoopContext     = CoordinatedContexts;
         LoopContext    != NULL;
         LoopContext     = LoopContext->Next)
    {
        if (LoopContext->InSynchronizeFn)
        {
            if (LoopContext->StreamType == StreamTypeVideo)
            {
                ParsedVideoParameters_t      *ParsedVideoParameters = (ParsedVideoParameters_t *)ParsedAudioVideoDataParameters;
                long long VideoInputLatency   = RoundedLongLongIntegerPart(1000000 / ParsedVideoParameters->Content.FrameRate);
                if (!ParsedVideoParameters->Content.Progressive)
                {
                    VideoInputLatency *= 2;
                }
                if (VideoInputLatency > MaxVideoInputLatency)
                {
                    MaxVideoInputLatency = VideoInputLatency;
                }
                LoopContext->ManifestationCoordinator->GetNextQueuedManifestationTimes(&ManifestationTimes, &ManifestationGranularities);
                for (int i = 0; i <= LoopContext->MaxSurfaceIndex; i++)
                {
                    if (ValidTime(ManifestationTimes[i]))
                    {
                        if ((long long)(ManifestationTimes[i] - Now) > MaxVideoManifestorLatency)
                        {
                            MaxVideoManifestorLatency = ManifestationTimes[i] - Now;
                        }
                    }
                }
            }
        }
    }
    LargestOfMinimumManifestationLatencies = MaxVideoManifestorLatency + MaxVideoInputLatency + MODULES_PROPAGATION_LATENCY;
    if (Player->PolicyValue(Playback, Stream, PolicyHdmiRxMode) == PolicyValueHdmiRxModeRepeater)
    {
        LargestOfMinimumManifestationLatencies = max(HDMIRX_REPEATER_MODE_LATENCY, LargestOfMinimumManifestationLatencies) ;
    }
    else if (Player->PolicyValue(Playback, Stream, PolicyHdmiRxMode) == PolicyValueHdmiRxModeNonRepeater)
    {
        LargestOfMinimumManifestationLatencies = max(HDMIRX_NON_REPEATER_MODE_LATENCY, LargestOfMinimumManifestationLatencies) ;
    }

    return LargestOfMinimumManifestationLatencies;
}

// /////////////////////////////////////////////////////////////////////////
//
//      The function to verify the master clock, will identify what
//      is the master clock, and ensure that it is still extant.
//

OutputCoordinatorStatus_t   OutputCoordinator_Base_c::VerifyMasterClock(void)
{
    unsigned int            i;
    OutputCoordinatorContext_t  ContextLoop;
    bool                GotPrimary;
    OutputCoordinatorContext_t  PrimaryContext;
    unsigned int            PrimaryIndex;
    bool                GotSecondary;
    OutputCoordinatorContext_t  SecondaryContext;
    unsigned int            SecondaryIndex;

    //
    // Separate a first scan just for the surface descriptors
    //

    for (ContextLoop  = CoordinatedContexts;
         ContextLoop    != NULL;
         ContextLoop     = ContextLoop->Next)
    {
        ContextLoop->ManifestationCoordinator->GetSurfaceParameters(&ContextLoop->Surfaces,  &ContextLoop->MaxSurfaceIndex);
    }

    //
    // If it is the system clock, then all is well
    //
    int MasterClock = Player->PolicyValue(Playback, PlayerAllStreams, PolicyMasterClock);

    if (MasterClock == PolicyValueSystemClockMaster)
    {
        UseSystemClockAsMaster  = true;
        return OutputCoordinatorNoError;
    }

    //
    // Scan
    //
    GotPrimary      = false;
    PrimaryContext  = NULL;
    PrimaryIndex    = 0;
    GotSecondary    = false;
    SecondaryContext    = NULL;
    SecondaryIndex  = 0;

    for (ContextLoop  = CoordinatedContexts;
         ContextLoop    != NULL;
         ContextLoop     = ContextLoop->Next)
    {
        //
        // Found it, and it is OK
        //
        if (ContextLoop->ClockMaster && (ContextLoop->Surfaces[ContextLoop->ClockMasterIndex] != NULL))
        {
            return OutputCoordinatorNoError;
        }

        ContextLoop->ClockMaster    = false;    // Just in case it went away

        //
        // Not found it yet, is this a possible
        //

        if (!GotPrimary && (((MasterClock == PolicyValueVideoClockMaster) && (ContextLoop->StreamType == StreamTypeVideo)) ||
                            ((MasterClock == PolicyValueAudioClockMaster) && (ContextLoop->StreamType == StreamTypeAudio))))
        {
            for (i = 0; i <= ContextLoop->MaxSurfaceIndex; i++)
                if ((ContextLoop->Surfaces[i] != NULL) && ContextLoop->Surfaces[i]->MasterCapable)
                {
                    GotPrimary      = true;
                    PrimaryContext  = ContextLoop;
                    PrimaryIndex    = i;
                    break;
                }
        }

        if (!GotPrimary && !GotSecondary)
        {
            for (i = 0; i <= ContextLoop->MaxSurfaceIndex; i++)
                if ((ContextLoop->Surfaces[i] != NULL) && ContextLoop->Surfaces[i]->MasterCapable)
                {
                    GotSecondary    = true;
                    SecondaryContext    = ContextLoop;
                    SecondaryIndex  = i;
                    break;
                }
        }
    }

    //
    // The fact that we got here implies that we didn't
    // have a master, or it has gone away.
    //

    if (GotPrimary)
    {
        PrimaryContext->ClockMasterIndex    = PrimaryIndex;
        PrimaryContext->ClockMaster     = true;
        UseSystemClockAsMaster          = false;
    }
    else if (GotSecondary)
    {
        SecondaryContext->ClockMasterIndex  = SecondaryIndex;
        SecondaryContext->ClockMaster       = true;
        UseSystemClockAsMaster          = false;
    }
    else
    {
        UseSystemClockAsMaster          = true;
    }

    //
    // The fact we got here without use system clock as master
    // implies that we have a new master clock
    //
    SystemClockAdjustmentEstablished        = false;

    return OutputCoordinatorNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
//      The function to restart clock recovery. Used mainly in case of errors
//

OutputCoordinatorStatus_t   OutputCoordinator_Base_c::ClockRecoveryRestart()
{
    ClockRecoveryBaseSourceClock        = INVALID_TIME;
    ClockRecoveryBaseLocalClock         = INVALID_TIME;
    ClockRecoveryEstablishedGradient    = 1;
    ClockRecoveryIntegrationTime        = CLOCK_RECOVERY_MINIMUM_INTEGRATION_TIME;
    //
    // We only reset the system clock here if the system clock is master
    //
    int MasterClock  = Player->PolicyValue(Playback, PlayerAllStreams, PolicyMasterClock);

    if (MasterClock != PolicyValueSystemClockMaster)
    {
        SE_WARNING("Performing clock recovery when system clock is NOT master\n");
        return OutputCoordinatorNoError;
    }

    //
    //  we assume that the local clock is 1:1 with the source clock
    //
    SystemClockAdjustmentEstablished    = true;
    SystemClockAdjustment               = 1;
    return OutputCoordinatorNoError;
}

// /////////////////////////////////////////////////////////////////////////
//
//      The function to initialize clock recovery when we have a system
//      clock master, driven by an external broadcast source such as
//      a satelite.
//      We will allow clock recovery when system clock is not master,
//      but we will not adjust the system clock.
//

OutputCoordinatorStatus_t   OutputCoordinator_Base_c::ClockRecoveryInitialize(PlayerTimeFormat_t       SourceTimeFormat)
{
    SE_INFO(group_avsync, "");
    ClockRecoverySourceTimeFormat       = SourceTimeFormat;
    ClockRecoveryLastPts                = INVALID_TIME;         // Only used if PTS source time format
    ClockRecoveryPtsBaseLine            = 0;
    ClockRecoveryInitialized            = true;
    ClockRecoveryLastSourceTime         = INVALID_TIME;
    ClockRecoveryLastLocalTime          = INVALID_TIME;
    return ClockRecoveryRestart();
}

// /////////////////////////////////////////////////////////////////////////
//
//      This function checks if Clock Recovery is initialized
//

OutputCoordinatorStatus_t OutputCoordinator_Base_c::ClockRecoveryIsInitialized(bool *Initialized)
{
    *Initialized = ClockRecoveryInitialized;
    return OutputCoordinatorNoError;
}

// /////////////////////////////////////////////////////////////////////////
//
//      The function to handle clock recovery when we have a system
//      clock master, driven by an external broadcast source such as
//      a satelite.
//

#if 0
static unsigned long long      TimeOfStart;
static unsigned long long      BasePcr;
static unsigned long long      LastPcr;
#endif


OutputCoordinatorStatus_t   OutputCoordinator_Base_c::ClockRecoveryDataPoint(
    unsigned long long                SourceTime,
    unsigned long long                LocalTime)
{
    unsigned long long          NormalizedSourceTime;
    Rational_t          Intercept;
    Rational_t          Gradient;
    long long           CalculatedX;
    Rational_t          SystemClockChange;
    Rational_t          NewSystemClockAdjustment;
    Rational_t          ClampLower;
    Rational_t          ClampUpper;
    long long           Jerk;
    OutputCoordinatorContext_t  ContextLoop;
    bool                ResetIntegrationPeriod;
    long                            MaximumPointToPointJitter;
    bool                DiscardPoint;
    unsigned long long              InitialLocalTime = LocalTime;
    unsigned long long              InitialNormalizedSourceTime;
    // Lock the component to thread-safely use ClockRecovery variables
    ComponentMonitor(this);

    //
    // Have we been initialized
    //

    if (!ClockRecoveryInitialized)
    {
        SE_ERROR("Clock recovery not initialized\n");
        return OutputCoordinatorError;
    }

    //
    // Before we do anything else, convert the source time to something we can use
    //

    switch (ClockRecoverySourceTimeFormat)
    {
    case TimeFormatUs:
        NormalizedSourceTime            = SourceTime;
        break;

    case TimeFormatPts:
        // This is practically a copy of FrameParser_Base_c::TranslatePlaybackTimeNativeToNormalized()
        // where all the useful comments can be found.
#define         WrapMask        0x0000000180000000ULL
#define         WrapOffset      0x0000000200000000ULL
        if (ClockRecoveryLastPts == INVALID_TIME)
        {
            ClockRecoveryLastPts        = SourceTime;
        }

        if (((ClockRecoveryLastPts & WrapMask) == WrapMask) &&
            ((SourceTime           & WrapMask) == 0))
        {
            SE_INFO(group_avsync, "Detected PCR positive wrap around\n");
            ClockRecoveryPtsBaseLine   += WrapOffset;
        }
        else if (((ClockRecoveryLastPts & WrapMask) == 0) &&
                 ((SourceTime           & WrapMask) == WrapMask))
        {
            SE_INFO(group_avsync, "Detected PCR negative wrap around\n");
            ClockRecoveryPtsBaseLine   -= WrapOffset;
        }

        ClockRecoveryLastPts            = SourceTime;
        NormalizedSourceTime            = (((ClockRecoveryPtsBaseLine + SourceTime) * 300) + 13) / 27;
        break;

    default:
        SE_ERROR("Unsupported source time format (%d)\n", ClockRecoverySourceTimeFormat);
        return PlayerNotSupported;
    }

    InitialNormalizedSourceTime = NormalizedSourceTime;

    //
    // Initialize the accumulated data if this is our first point
    //

    if (ClockRecoveryBaseSourceClock == INVALID_TIME)
    {
        if (ClockRecoveryDevlogSystemClockOffset == INVALID_TIME)
        {
            int Devlog = Player->PolicyValue(Playback, PlayerAllStreams, PolicyRunningDevlog);
            ClockRecoveryDevlogSystemClockOffset    = Devlog ?
                                                      (OS_GetTimeInMicroSeconds() - LocalTime) :
                                                      0;
        }

        ClockRecoveryBaseSourceClock    = NormalizedSourceTime;
        ClockRecoveryBaseLocalClock     = LocalTime + ClockRecoveryDevlogSystemClockOffset;
        ClockRecoveryAccumulatedPoints  = 0;
        ClockRecoveryDiscardedPoints    = 0;
        ClockRecoveryLeastSquareFit.Reset();
    }

    //
    // Accumulate the data point (converting the times to deltas first)
    //
    LocalTime           += ClockRecoveryDevlogSystemClockOffset;
    NormalizedSourceTime        -= ClockRecoveryBaseSourceClock + ClockRecoveryLeastSquareFit.Y();
    LocalTime                   -= ClockRecoveryBaseLocalClock  + ClockRecoveryLeastSquareFit.X();
    //
    // Jitter between points cannot be greater than the maximum playback latency
    // (plus our own timestamping error)
    //
    MaximumPointToPointJitter = (long)Player->PolicyValue(Playback, PlayerAllStreams, PolicyLivePlaybackLatencyVariabilityMs) * 1000;
    MaximumPointToPointJitter += INTERNAL_TIMESTAMP_LATENCY_ESTIMATE;
    DiscardPoint = !inrange((long long)(LocalTime - NormalizedSourceTime), -MaximumPointToPointJitter, MaximumPointToPointJitter);

    if (DiscardPoint)
    {
        /* When using Packet injector, and doing the dynamic change of transport stream having same A/V/PCR pids (bug28255 / bug32446)
           or upon the loop around then it is possible that a miss-match in the LocalTime and the PCR.
           So in this case we need to restart the playback with new time mapping so that new audio video packets have new time mapping.
           Drain is required to flush the old data with old time mapping.
           Also we need to reset the ClockRecovery without saving "EstablishedGradient" because with the dynamic change of stream
           we may have different gradient.
           Depending upon the PCR jump we drain the playback in 2 cases.
           1. Reverse PCR : Current PCR less than Last PCR.
           2. Forward PCR : Current PCR forwarded by more than the forward allowed jump(PolicyPtsForwardJumpDetectionThreshold).
           PolicyPtsForwardJumpDetectionThreshold set forward jump threshold as 2^N seconds. Min 1 sec, Maximum limit setting to 2^16 =~18Hr.
        */
        uint32_t PCRJumpThreshold               = (uint32_t)Player->PolicyValue(Playback, PlayerAllStreams, PolicyPtsForwardJumpDetectionThreshold);
        PCRJumpThreshold                        = min(PCRJumpThreshold, 16);      // 2 ^16 correspond to 18 Hr. Next value(17) will cross the 33bit PCR wrap (26.5 Hr).
        int64_t MaximumAllowedForwardPcrJump    = 1000000 * (1ULL << PCRJumpThreshold) + Player->PolicyValue(Playback, PlayerAllStreams, PolicyLivePlaybackLatencyVariabilityMs) * 1000;
        bool    DrainRequired = (InitialNormalizedSourceTime < ClockRecoveryLastSourceTime) || ((int64_t)(NormalizedSourceTime - LocalTime) > MaximumAllowedForwardPcrJump);

        if ((Player->PolicyValue(Playback, PlayerAllStreams, PolicyPacketInjectorPlayback) == PolicyValueApply) && (DrainRequired))
        {
            SE_WARNING("ClockRecovery: PCR vs LocalTime is not in range PCR %s jump. Draining the data from old PCR so that playback restart with new PCR\n",
                       ((InitialNormalizedSourceTime < ClockRecoveryLastSourceTime) ? "reverse" : "forward"));
            ClockRecoveryRestart();
            DrainLivePlayback();
        }

        ClockRecoveryDiscardedPoints++;
        Playback->GetStatistics().ClockRecoveryCummulativeDiscardedPoints++;

        if (ClockRecoveryDiscardedPoints > 4)
        {
            NewSystemClockAdjustment            = SystemClockAdjustment;
            Gradient                            = ClockRecoveryEstablishedGradient;
            ClockRecoveryRestart();
            SystemClockAdjustment               = NewSystemClockAdjustment;
            ClockRecoveryEstablishedGradient    = Gradient;
            Playback->GetStatistics().ClockRecoveryCummulativeDiscardResets++;
        }

        return OutputCoordinatorNoError;
    }

    ClockRecoveryLeastSquareFit.Add(NormalizedSourceTime, LocalTime);
    ClockRecoveryAccumulatedPoints++;
    ClockRecoveryDiscardedPoints    = 0;
    //
    // Now there are no more failure paths in this function, we can store the raw clock data point
    // for use during initial time mapping.
    //
    ClockRecoveryLastSourceTime = InitialNormalizedSourceTime;
    ClockRecoveryLastLocalTime  = InitialLocalTime;

    //
    // Is it time for a readout, we judge this on elapsed time and on a minimum number of points
    //

    if ((ClockRecoveryAccumulatedPoints >= CLOCK_RECOVERY_MINIMUM_POINTS) &&
        (ClockRecoveryLeastSquareFit.X() >= ClockRecoveryIntegrationTime))
    {
        Gradient                = ClockRecoveryLeastSquareFit.Gradient();
        if (Gradient == 0)
        {
            SE_WARNING("fixing Gradient 0->1\n");
            Gradient = 1;
        }
        Intercept               = ClockRecoveryLeastSquareFit.Intercept();
        if (Intercept == 0)
        {
            SE_WARNING("fixing Intercept 0->1\n");
            Intercept = 1;
        }
        //
        // Perform a sanity check
        //

        if (!inrange(Gradient, Rational_t(1000000 - CLOCK_RECOVERY_MAXIMUM_PPM, 1000000), Rational_t(1000000 + CLOCK_RECOVERY_MAXIMUM_PPM, 1000000)))
        {
            SE_WARNING("Failed to recover valid clock - retrying\n");
            NewSystemClockAdjustment            = SystemClockAdjustment;
            Gradient                            = ClockRecoveryEstablishedGradient;
            ClockRecoveryRestart();
            ClockRecoveryEstablishedGradient    = Gradient;
            SystemClockAdjustment               = NewSystemClockAdjustment;
            Playback->GetStatistics().ClockRecoveryClockAdjustment = GradientToPartsPerMillion(SystemClockAdjustment);
            Playback->GetStatistics().ClockRecoveryEstablishedGradient = GradientToPartsPerMillion(ClockRecoveryEstablishedGradient);
            return OutputCoordinatorNoError;
        }

        // this is the raw observed clock drift for the current integration window (before clamping)
        Playback->GetStatistics().ClockRecoveryActualGradient = GradientToPartsPerMillion(Gradient);
        ClampLower  = ClockRecoveryEstablishedGradient - Rational_t(CLOCK_RECOVERY_MAXIMUM_SINGLE_PPM_ADJUSTMENT, 1000000);
        ClampUpper  = ClockRecoveryEstablishedGradient + Rational_t(CLOCK_RECOVERY_MAXIMUM_SINGLE_PPM_ADJUSTMENT, 1000000);
        Clamp(Gradient, ClampLower, ClampUpper);
        ClockRecoveryEstablishedGradient        = Gradient;
        SE_DEBUG(group_avsync, "ClockRecovery - IntegrationTime = %5llds(%5d), Gradient %d.%09d, Intercept %4d.%09d\n",
                 ClockRecoveryIntegrationTime / 1000000,
                 ClockRecoveryAccumulatedPoints,
                 ClockRecoveryEstablishedGradient.IntegerPart(), ClockRecoveryEstablishedGradient.RemainderDecimal(9),
                 Intercept.IntegerPart(), Intercept.RemainderDecimal(9));
        //
        // Set us up for a reset
        //
        // We move the base values to the most recent PCR, but rather than use the
        // actual system clock value, we use the value of X that matches our line fit
        //
        CalculatedX         = ClockRecoveryLeastSquareFit.Y() - RoundedLongLongIntegerPart(Intercept);
        CalculatedX         = (Gradient.GetDenominator() * CalculatedX) / Gradient.GetNumerator();
        ClockRecoveryBaseSourceClock  = ClockRecoveryBaseSourceClock + ClockRecoveryLeastSquareFit.Y();
        ClockRecoveryBaseLocalClock   = ClockRecoveryBaseLocalClock + ClockRecoveryLeastSquareFit.X();
        ClockRecoveryAccumulatedPoints  = 0;
        ClockRecoveryLeastSquareFit.Reset();

        //
        // Adjust the integration time
        //

        if (ClockRecoveryIntegrationTime < CLOCK_RECOVERY_MAXIMUM_INTEGRATION_TIME)
        {
            ClockRecoveryIntegrationTime        *= 2;
        }

        //
        // Do we need to adjust the system clock rate
        //
        int MasterClock = Player->PolicyValue(Playback, PlayerAllStreams, PolicyMasterClock);

        if (MasterClock == PolicyValueSystemClockMaster)
        {
            //
            // Before performing the system clock adjustment, we need to
            // compensate the mapping for the jerk affect this will have
            // on previous time.
            //
            // NOTE we need to know whether or not to cut the integration
            //      period for clock rate adjustment, we gate this on whether or
            //      the change is greater than 1.5ppm.
            //
            NewSystemClockAdjustment  = ClockRecoveryEstablishedGradient;
            SystemClockChange         = NewSystemClockAdjustment - SystemClockAdjustment;
            Jerk                      = -LongLongIntegerPart(SystemClockChange * (OS_GetTimeInMicroSeconds() - MasterBaseSystemTime));
            ResetIntegrationPeriod    = !inrange(SystemClockChange, Rational_t(-3, 2000000), Rational_t(3, 2000000));

            if (!inrange(Jerk, -10000, 10000))
            {
                SE_WARNING("Big Jerk CR - %lld - Old %d.%09d, New %d.%09d, Change %d.%09d - TP %lld\n", Jerk,
                           SystemClockAdjustment.IntegerPart(), SystemClockAdjustment.RemainderDecimal(9),
                           NewSystemClockAdjustment.IntegerPart(), NewSystemClockAdjustment.RemainderDecimal(9),
                           SystemClockChange.IntegerPart(), SystemClockChange.RemainderDecimal(9),
                           (OS_GetTimeInMicroSeconds() - MasterBaseSystemTime));
                SystemClockAdjustment.DumpVal(group_avsync, "SystemClockAdj");
                NewSystemClockAdjustment.DumpVal(group_avsync, "NewSystemClockAdj");
                SystemClockChange.DumpVal(group_avsync, "SystemClockChg");
            }

            SystemClockAdjustmentEstablished        = true;
            SystemClockAdjustment           = NewSystemClockAdjustment;
            MasterBaseSystemTime            -= Jerk;

            for (ContextLoop         = CoordinatedContexts;
                 ContextLoop        != NULL;
                 ContextLoop         = ContextLoop->Next)
            {
                ContextLoop->BaseSystemTime     -= Jerk;
                ContextLoop->BaseSystemTimeAdjusted  = true;
                RestartOutputRateIntegration(ContextLoop, ResetIntegrationPeriod);
            }
        }
    }

    Playback->GetStatistics().ClockRecoveryAccumulatedPoints = ClockRecoveryAccumulatedPoints;
    Playback->GetStatistics().ClockRecoveryClockAdjustment = GradientToPartsPerMillion(SystemClockAdjustment);
    Playback->GetStatistics().ClockRecoveryEstablishedGradient = GradientToPartsPerMillion(ClockRecoveryEstablishedGradient);
    Playback->GetStatistics().ClockRecoveryIntegrationTimeWindow = ClockRecoveryIntegrationTime / 1000000;
    Playback->GetStatistics().ClockRecoveryIntegrationTimeElapsed = ClockRecoveryLeastSquareFit.X() / 1000000;

    return OutputCoordinatorNoError;
}

// /////////////////////////////////////////////////////////////////////////
//
//      The function to read out an estimate of the recovered clock
//

OutputCoordinatorStatus_t   OutputCoordinator_Base_c::ClockRecoveryEstimate(
    unsigned long long               *SourceTime,
    unsigned long long               *LocalTime)
{
    unsigned long long       Now;
    unsigned long long       EstimatedSourceTime;
    unsigned long long       ElapsedSourceTime;

    //
    // Let us initialize first
    //

    if (SourceTime != NULL)
    {
        *SourceTime     = INVALID_TIME;
    }

    if (LocalTime != NULL)
    {
        *LocalTime      = INVALID_TIME;
    }

    //
    // Is clock recovery live
    //

    if (!ClockRecoveryInitialized)
    {
        SE_ERROR("Clock recovery not initialized\n");
        return OutputCoordinatorError;
    }

    if (ClockRecoveryBaseSourceClock == INVALID_TIME)
    {
        SE_ERROR("No basis on which to make an estimate\n");
        return OutputCoordinatorError;
    }

    //
    // Calculate the value
    //
    Now                 = OS_GetTimeInMicroSeconds();
    ElapsedSourceTime    = (((long long)(Now - ClockRecoveryBaseLocalClock)) *
                            ClockRecoveryEstablishedGradient.GetDenominator()) /
                           ClockRecoveryEstablishedGradient.GetNumerator();
    EstimatedSourceTime  = ClockRecoveryBaseSourceClock + ElapsedSourceTime;

    //
    // Convert the estimate to an appropriate format
    //

    switch (ClockRecoverySourceTimeFormat)
    {
    case TimeFormatUs:
        // We operate in this format
        break;

    case TimeFormatPts:
        EstimatedSourceTime     = (((EstimatedSourceTime * 27) + 150) / 300) & 0x00000001ffffffffULL;
        break;

    default:
        SE_ERROR("Unsupported source time format (%d)\n", ClockRecoverySourceTimeFormat);
        return PlayerNotSupported;
    }

    //
    // Write the desired outputs
    //

    if (SourceTime != NULL)
    {
        *SourceTime     = EstimatedSourceTime;
    }

    if (LocalTime != NULL)
    {
        *LocalTime      = Now;
    }

    return OutputCoordinatorNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
//      Function to drain a live playback if we need to re-synchronize
//

void    OutputCoordinator_Base_c::DrainLivePlayback(void)
{
    //
    // If we are in live playback, it would be wise to drain down all streams before continuing
    //
    if (Player->PolicyValue(Playback, PlayerAllStreams, PolicyLivePlayback) == PolicyValueApply)
    {
        SE_INFO(group_avsync, "Performing DrainLivePlayback()\n");
        OS_SetEvent(&Playback->GetDrainSignalEvent());
    }
    else
    {
        ResetTimeMapping(PlaybackContext);
    }
}


// /////////////////////////////////////////////////////////////////////////
//
//      Time functions, coded here since direction
//      of play may change the meaning later.
//

unsigned long long   OutputCoordinator_Base_c::SpeedScale(unsigned long long      T)
{
    Rational_t      Temp;

    if (Direction == PlayBackward)
    {
        T       = -T;
    }

    if (Speed == 1)
    {
        return T;
    }

    Temp        = (Speed == 0) ? 0 : T / Speed;
    return Temp.LongLongIntegerPart();
}

unsigned long long   OutputCoordinator_Base_c::InverseSpeedScale(unsigned long long      T)
{
    if (Direction == PlayBackward)
    {
        T = -T;
    }

    if (Speed == 1)
    {
        return T;
    }

    Rational_t Temp = Speed * T;
    return Temp.LongLongIntegerPart();
}

