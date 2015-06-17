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

#include "player_generic.h"
#include "player_playback.h"
#include "player_stream.h"
#include "output_timer_base.h"

#undef TRACE_TAG
#define TRACE_TAG "OutputTimer_Base_c"

#define DECODE_TIME_JUMP_THRESHOLD              16000000    // 16 second default (as coordinator PLAYBACK_TIME_JUMP_THRESHOLD)

#define TRICK_MODE_CHECK_REFERENCE_FRAMES_FOR_N_KEY_FRAMES  2       // Check reference frames for decode
// after trick mode domain change
// until we have seen N key frames

#define DECODE_IN_TIME_INTEGRATION_PERIOD           16      // Integrate decode in time checks over 16 frames
#define DECODE_IN_TIME_PAUSE_BETWEEN_INTEGRATION_PERIOD     64

#define REBASE_MARGIN                       50000       // A 50ms margin when we attempt to recover
// from decode in time failures by rebasing

#define MAX_SYNCHRONIZATION_DIFFERENCE              60000000    // On first try synchronize up to 10 minutes.
// On following tries synchronize up to a minute,
// otherwise setup a new time mapping
//

#define MANIFESTATION_WINDOW_WIDTH   300000000ULL   // Drop frame if it's is supposed to be presented more
// than 5 minutes in the future

OutputTimer_Base_c::OutputTimer_Base_c(void)
    : mLock()
    , mConfiguration()
    , mOutputSurfaceDescriptors(NULL)
    , mOutputSurfaceDescriptorsHighestIndex(0)
    , mCodedFrameRate(1)
    , mAdjustedSpeedAfterFrameDrop(1)
    , mLastCountMultiplier(1)
    , mNextExpectedPlaybackTime(INVALID_TIME)
    , mLastFrameCollationTime(INVALID_TIME)
    , mManifestationTimingState()
    , mSystemClockAdjustment(1)
    , mOutputRateAdjustments(NULL)
    , mTrickModeDomain(TrickModeInvalid)
    , mMonitorGroupStructureLock()
    , mOutputCoordinator(NULL)
    , mOutputCoordinatorContext(NULL)
    , mBufferManager(NULL)
    , mDecodeBufferPool(NULL)
    , mCodedFrameBufferPool(NULL)
    , mCodedFrameBufferMaximumSize(0)
    , mTrickModeParameters()
    , mTrickModeCheckReferenceFrames(0)
    , mTrickModeDomainTransitToDegradeNonReference(0)
    , mTrickModeDomainTransitToStartDiscard(0)
    , mTrickModeDomainTransitToDecodeReference(0)
    , mTrickModeDomainTransitToDecodeKey(0)
    , mCodedFrameRateLimitForIOnly(0)
    , mLastCodedFrameRate(0)
    , mEmpiricalCodedFrameRateLimit(0)
    , mPortionOfPreDecodeFramesToLose(0)
    , mAccumulatedPreDecodeFramesToLose(0)
    , mNextGroupStructureEntry(0)
    , mGroupStructureSizes()
    , mGroupStructureReferences()
    , mGroupStructureSizeCount(0)
    , mGroupStructureReferenceCount(0)
    , mTrickModeGroupStructureIndependentFrames()
    , mTrickModeGroupStructureReferenceFrames()
    , mTrickModeGroupStructureNonReferenceFrames()
    , mLastTrickModePolicy(PolicyValueTrickModeAuto)
    , mLastTrickModeDomain(TrickModeInvalid)
    , mLastPortionOfPreDecodeFramesToLose(2)
    , mLastTrickModeSpeed(0)
    , mLastTrickModeGroupStructureIndependentFrames(0)
    , mLastTrickModeGroupStructureReferenceFrames(0)
    , mDecodeTimeJumpThreshold(DECODE_TIME_JUMP_THRESHOLD)
    , mNormalizedTimeOffset(0)
    , mTimeMappingValidForDecodeTiming(false)
    , mTimeMappingInvalidatedAtDecodeIndex(INVALID_INDEX)
    , mLastSeenDecodeTime(INVALID_TIME)
    , mLastFrameDropPreDecodeDecision(OutputTimerNoError)
    , mKeyFramesSinceDiscontinuity(0)
    , mLastKeyFramePlaybackTime(INVALID_TIME)
    , mDecodeInTimeState(DecodingInTime)
    , mDecodeInTimeFailures(0)
    , mDecodeInTimeCodedDataLevel(0)
    , mDecodeInTimeBehind(0)
    , mSeenAnInTimeFrame(false)
    , mLastSynchronizationPolicy(0)
    , mStreamNotifiedInSync(false)
    , mSmoothReverseSupport(true)
    , mSmoothReverseSupportChanged(false)
    , mGlobalSetSynchronizationAtStartup(true)
    , mGlobalSynchronizationState(SyncStateNullState)
    , mLastSystemClockAdjustment(1)
    , mVsyncOffsets(NULL)
{
    OS_InitializeMutex(&mLock);
    OS_InitializeMutex(&mMonitorGroupStructureLock);
}

//
OutputTimer_Base_c::~OutputTimer_Base_c(void)
{
    Halt();
    Reset();

    OS_TerminateMutex(&mLock);
    OS_TerminateMutex(&mMonitorGroupStructureLock);
}


//
OutputTimerStatus_t   OutputTimer_Base_c::Halt(void)
{
    if (TestComponentState(ComponentRunning))
    {
        //
        // Move the base state to halted early, to ensure
        // no activity can be queued once we start halting
        //
        BaseComponentClass_c::Halt();
    }

    return OutputTimerNoError;
}


//
OutputTimerStatus_t   OutputTimer_Base_c::Reset(void)
{
    // Since the Reset is invoked from Player_Generic_c dtor()
    // after each play stream thread is terminated, we release output coordinator
    // context only here.

    if (mOutputCoordinator != NULL)
    {
        mOutputCoordinator->DeRegisterStream(mOutputCoordinatorContext);
        mOutputCoordinatorContext = NULL;
        mOutputCoordinator        = NULL;
    }

    mTimeMappingValidForDecodeTiming     = false;
    mTimeMappingInvalidatedAtDecodeIndex     = INVALID_INDEX;
    mLastSeenDecodeTime              = INVALID_TIME;
    mLastKeyFramePlaybackTime        = INVALID_TIME;
    mNextExpectedPlaybackTime        = INVALID_TIME;
    mLastFrameCollationTime          = INVALID_TIME;
    mNormalizedTimeOffset            = 0;
    mDecodeTimeJumpThreshold         = DECODE_TIME_JUMP_THRESHOLD;
    mLastFrameDropPreDecodeDecision  = OutputTimerNoError;
    mKeyFramesSinceDiscontinuity     = 0;
    mTrickModeDomain                             = TrickModeInvalid; // rational
    mTrickModeCheckReferenceFrames               = 0;
    mTrickModeDomainTransitToDegradeNonReference = 0; // rational
    mTrickModeDomainTransitToStartDiscard        = 0; // rational
    mTrickModeDomainTransitToDecodeReference     = 0; // rational
    mTrickModeDomainTransitToDecodeKey           = 0; // rational
    mCodedFrameRate                      = 1; // rational
    mCodedFrameRateLimitForIOnly         = 0; // rational
    mLastCodedFrameRate                  = 0; // rational
    mEmpiricalCodedFrameRateLimit        = 0; // rational
    mAdjustedSpeedAfterFrameDrop         = 1; // rational
    mLastCountMultiplier                 = 1; // rational
    mPortionOfPreDecodeFramesToLose      = 0; // rational
    mAccumulatedPreDecodeFramesToLose    = 0; // rational
    mLastTrickModePolicy             = PolicyValueTrickModeAuto;
    mLastTrickModeDomain             = TrickModeInvalid;
    mLastPortionOfPreDecodeFramesToLose              = 2; // rational
    mLastTrickModeSpeed                              = 0; // rational
    mLastTrickModeGroupStructureIndependentFrames    = 0; // rational
    mLastTrickModeGroupStructureReferenceFrames      = 0; // rational
    mSystemClockAdjustment               = 1; // rational
    mLastSystemClockAdjustment           = 1; // rational
    mDecodeInTimeState               = DecodingInTime;
    mSeenAnInTimeFrame               = false;
    mGlobalSynchronizationState          = SyncStateNullState;
    mGlobalSetSynchronizationAtStartup       = true;
    mStreamNotifiedInSync            = false;
    mSmoothReverseSupport            = true;
    mSmoothReverseSupportChanged         = false;
    mVsyncOffsets                = NULL;

    memset(mManifestationTimingState, 0, MAXIMUM_MANIFESTATION_TIMING_COUNT * sizeof(ManifestationTimingState_t));

    for (int i = 0; i < MAXIMUM_MANIFESTATION_TIMING_COUNT; i++)
    {
        mManifestationTimingState[i].AccumulatedError            = 0; // rational
        mManifestationTimingState[i].PreviousDisplayFrameRate    = 0; // rational
        mManifestationTimingState[i].CountMultiplier             = 0; // rational
    }

    return BaseComponentClass_c::Reset();
}


// /////////////////////////////////////////////////////////////////////////
//
//  Override for component base class set module parameters function
//

OutputTimerStatus_t   OutputTimer_Base_c::SetModuleParameters(
    unsigned int      ParameterBlockSize,
    void             *ParameterBlock)
{
    OutputTimerParameterBlock_t  *OutputTimerParameterBlock = (OutputTimerParameterBlock_t *)ParameterBlock;

    if (ParameterBlockSize != sizeof(OutputTimerParameterBlock_t))
    {
        SE_ERROR("Invalid parameter block\n");
        return OutputTimerError;
    }

    switch (OutputTimerParameterBlock->ParameterType)
    {
    case OutputTimerSetNormalizedTimeOffset:

        //
        // Lazily update the value, and force a rebase of the time mapping if required
        //
        if (mNormalizedTimeOffset != OutputTimerParameterBlock->Offset.Value)
        {
            mNormalizedTimeOffset    = OutputTimerParameterBlock->Offset.Value;
            int ExternalMappingPolicy  = Player->PolicyValue(Playback, Stream, PolicyExternalTimeMapping);

            if (ExternalMappingPolicy != PolicyValueApply)
            {
                ResetTimeMapping();
            }
        }

        break;

    default:
        SE_ERROR("Unrecognised parameter block (%d)\n", OutputTimerParameterBlock->ParameterType);
        return OutputTimerError;
    }

    return  OutputTimerNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
//  the register a stream function, creates a new timer context
//

OutputTimerStatus_t   OutputTimer_Base_c::RegisterOutputCoordinator(
    OutputCoordinator_t   mOutputCoordinator)
{
    //
    // Obtain the class list for this stream then fill in the
    // configuration record
    //
    Player->GetBufferManager(&mBufferManager);
    InitializeConfiguration();
    //
    // Read the initial value of the sync policy
    //
    mLastSynchronizationPolicy = Player->PolicyValue(Playback, Stream, PolicyAVDSynchronization);
    //
    // Attach the stream specific (audio|video|data)
    // output timing information to the decode buffer pool.
    //
    Player->GetDecodeBufferPool(Stream, &mDecodeBufferPool);

    if (mDecodeBufferPool == NULL)
    {
        SE_ERROR("Stream 0x%p - %s - Failed to obtain the decode buffer pool\n", Stream , mConfiguration.OutputTimerName);
        return OutputTimerError;
    }

    PlayerStatus_t Status = mDecodeBufferPool->AttachMetaData(Player->MetaDataOutputTimingType);
    if (Status != BufferNoError)
    {
        SE_ERROR("Stream 0x%p - %s - Failed to attach stream specific timing data to all members of the decode buffer pool\n",
                 Stream, mConfiguration.OutputTimerName);
        return Status;
    }

    //
    // Get the coded frame buffer pool, so that we can monitor levels if
    // frames fail to decode in time.
    //
    mCodedFrameBufferPool = Stream->GetCodedFrameBufferPool(&mCodedFrameBufferMaximumSize);
    SE_ASSERT(mCodedFrameBufferPool != NULL);

    //
    // Record the output coordinator
    // and register us with it.
    //
    Status  = mOutputCoordinator->RegisterStream(Stream, mConfiguration.StreamType, &mOutputCoordinatorContext);

    if (Status != PlayerNoError)
    {
        SE_ERROR("Stream 0x%p - %s - Failed to register with output coordinator\n", Stream, mConfiguration.OutputTimerName);
        return Status;
    }

    this->mOutputCoordinator = mOutputCoordinator;
    //
    // Initialize state
    //
    mLastFrameDropPreDecodeDecision  = OutputTimerNoError;
    mKeyFramesSinceDiscontinuity     = 0;
    ResetTimingContext();
    mOutputCoordinator->CalculateOutputRateAdjustments(mOutputCoordinatorContext, NULL,
                                                       &mOutputRateAdjustments, &mSystemClockAdjustment);
    mLastSystemClockAdjustment = mSystemClockAdjustment;
    //
    // Initialize the trick mode domain boundaries before going live
    //
    Stream->GetCodec()->GetTrickModeParameters(&mTrickModeParameters);
    OS_LockMutex(&mMonitorGroupStructureLock);
    InitializeGroupStructure();
    SetTrickModeDomainBoundaries();
    OS_UnLockMutex(&mMonitorGroupStructureLock);
    //
    // Go live, and return
    //
    SetComponentState(ComponentRunning);
    return OutputTimerNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
//  the reset time mapping function
//

OutputTimerStatus_t   OutputTimer_Base_c::ResetTimeMapping(void)
{
    //
    // Check that we are running
    //
    AssertComponentState(ComponentRunning);
    //
    // Reset the mapping
    //
    mTimeMappingValidForDecodeTiming = false;
    mTimeMappingInvalidatedAtDecodeIndex = INVALID_INDEX;
    mLastSeenDecodeTime          = INVALID_TIME;
    mNextExpectedPlaybackTime    = INVALID_TIME;
    SE_DEBUG2(group_avsync, group_output_timer, "Stream 0%p\n", Stream);
    return mOutputCoordinator->ResetTimeMapping(mOutputCoordinatorContext);
}


// /////////////////////////////////////////////////////////////////////////
//
//  the reset time mapping function
//

OutputTimerStatus_t   OutputTimer_Base_c::ResetTimingContext(unsigned int     ManifestationIndex)
{
    unsigned int    i;
    unsigned int    LoopStart;
    unsigned int    LoopEnd;
    LoopStart   = (ManifestationIndex == INVALID_INDEX) ? 0 : ManifestationIndex;
    LoopEnd = (ManifestationIndex == INVALID_INDEX) ? MAXIMUM_MANIFESTATION_TIMING_COUNT : (ManifestationIndex + 1);

    for (i = LoopStart; i < LoopEnd; i++)
    {
        mManifestationTimingState[i].SynchronizationState        = SyncStateInSynchronization;
        mManifestationTimingState[i].SynchronizationFrameCount       = 0;
        mManifestationTimingState[i].SynchronizationAtStartup        = true;
        mManifestationTimingState[i].SynchronizationErrorIntegrationCount = 0;
        mManifestationTimingState[i].SynchronizationAccumulatedError = 0;
        mManifestationTimingState[i].SynchronizationError        = 0;
        mManifestationTimingState[i].FrameWorkthroughCount       = 0;
        mManifestationTimingState[i].AccumulatedError            = 0; // rational
        mManifestationTimingState[i].ExtendSamplesForSynchronization = false;
        mManifestationTimingState[i].SynchronizationCorrectionUnits  = 0;
        mManifestationTimingState[i].SynchronizationOneTenthCorrectionUnits  = 0;
        mManifestationTimingState[i].LoseFieldsForSynchronization    = 0;
        mManifestationTimingState[i].PreviousDisplayFrameRate        = 0; // rational
        mManifestationTimingState[i].CountMultiplier         = 1; // rational
        mManifestationTimingState[i].FieldDurationTime           = 0;
        mManifestationTimingState[i].Initialized             = true;
    }

    mOutputCoordinator->ResetTimingContext(mOutputCoordinatorContext, ManifestationIndex);

    return OutputTimerNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
//  The function to await entry into the decode window, if the decode
//  time is invalid, or if no time mapping has yet been established,
//  we return immediately.
//

OutputTimerStatus_t   OutputTimer_Base_c::AwaitEntryIntoDecodeWindow(Buffer_t         Buffer)
{
    OutputCoordinatorStatus_t     Status;
    bool                  TrickModeDiscarding;
    ParsedFrameParameters_t      *ParsedFrameParameters;
    long long             DeltaDecodeTime;
    long long             LowerLimit;
    unsigned long long        NormalizedDecodeTime;
    unsigned long long        DecodeWindowPorch;
    Rational_t            Speed;
    PlayDirection_t           Direction;
    //
    // Check that we are running
    //
    AssertComponentState(ComponentRunning);

    //
    // Check for policies under which we do not wait for entry into the decode window.
    //

    if ((Player->PolicyValue(Playback, Stream, PolicyAVDSynchronization) == PolicyValueDisapply) ||
        (Player->PolicyValue(Playback, Stream, PolicyLivePlayback) == PolicyValueApply))
    {
        return PlayerNoError;
    }

    //
    // For reverse play we use different values, but now still do implement decode windows
    //
    Player->GetPlaybackSpeed(Playback, &Speed, &Direction);

    // Note : Wait to entry into decode window only needed for the trick mode (when the speed ~=1). See bz27174 and 26315
    if ((Speed == 0) || (Speed == 1))
    {
        return PlayerNoError;
    }

    TrickModeDiscarding     = !inrange(Speed, mConfiguration.MinimumSpeedSupported, mConfiguration.MaximumSpeedSupported);

    //
    // Is time mapping usable (Note for discarding a master may be allowed)
    //

    if (!mTimeMappingValidForDecodeTiming && !TrickModeDiscarding)
    {
        return PlayerNoError;
    }

    //
    // Would it be helpful to make up a DTS
    //

    ParsedFrameParameters = GetParsedFrameParameters(Buffer);

    if (NotValidTime(ParsedFrameParameters->NormalizedDecodeTime) &&
        !ParsedFrameParameters->ReferenceFrame &&
        ValidTime(ParsedFrameParameters->NormalizedPlaybackTime))
    {
        ParsedFrameParameters->NormalizedDecodeTime = ParsedFrameParameters->NormalizedPlaybackTime - mConfiguration.FrameDecodeTime;
    }

    //
    // Check for large DTS changes, invalidating the mapping if we find one
    // Note we allow negative jumps here of up to the decode porch time, simply
    // because when we make up a DTS we often create a negative jump of 1 or
    // two frame times.
    //

    if (ValidTime(mLastSeenDecodeTime) && ValidTime(ParsedFrameParameters->NormalizedDecodeTime))
    {
        DeltaDecodeTime     = (Direction == PlayBackward) ?
                              (mLastSeenDecodeTime - ParsedFrameParameters->NormalizedDecodeTime) :
                              (ParsedFrameParameters->NormalizedDecodeTime - mLastSeenDecodeTime);
        LowerLimit      = min(-100, -(long long)mConfiguration.EarlyDecodePorch);

        if (!inrange(DeltaDecodeTime, LowerLimit, mDecodeTimeJumpThreshold))
        {
            mTimeMappingValidForDecodeTiming = false;
            mTimeMappingInvalidatedAtDecodeIndex = ParsedFrameParameters->DecodeFrameIndex;
            SE_WARNING("Stream 0x%p - %s - Jump in decode times detected %12lld\n", Stream, mConfiguration.OutputTimerName, DeltaDecodeTime);
        }
    }

    mLastSeenDecodeTime      = ParsedFrameParameters->NormalizedDecodeTime;

    //
    // Check if we have a valid time mapping, return immediately
    // if we have no time mapping or the decode time is invalid.
    //

    if ((!mTimeMappingValidForDecodeTiming && !TrickModeDiscarding) || NotValidTime(ParsedFrameParameters->NormalizedDecodeTime))
    {
        return OutputTimerNoError;
    }

    //
    // Perform the wait
    //
    NormalizedDecodeTime   = ParsedFrameParameters->NormalizedDecodeTime + (unsigned long long)mNormalizedTimeOffset;

    if (Direction == PlayForward)
    {
        DecodeWindowPorch   = mConfiguration.FrameDecodeTime + mConfiguration.EarlyDecodePorch;
    }
    else
    {
        DecodeWindowPorch   = mConfiguration.ReverseEarlyDecodePorch;
    }

    Status          = mOutputCoordinator->PerformEntryIntoDecodeWindowWait(mOutputCoordinatorContext, NormalizedDecodeTime, DecodeWindowPorch);

    return Status;
}


// /////////////////////////////////////////////////////////////////////////
//
//  The function to test for frame drop before or after decode
//

OutputTimerStatus_t   OutputTimer_Base_c::TestForFrameDrop(Buffer_t       Buffer,
                                                           OutputTimerTestPoint_t    TestPoint)
{
    OutputTimerStatus_t   Status;
    //
    // Check that we are running
    //
    AssertComponentState(ComponentRunning);

    //
    // Are we applying synchronization
    //

    // test for frame drop before decode is allowed even if we are in sync disable
    //
    if (TestPoint != OutputTimerBeforeDecodeWindow)
    {
        int SynchronizationPolicy   = Player->PolicyValue(Playback, Stream, PolicyAVDSynchronization);

        if (SynchronizationPolicy == PolicyValueDisapply)
        {
            return PlayerNoError;
        }
    }

    //
    // Split out to the different test points
    //

    switch (TestPoint)
    {
    case OutputTimerBeforeDecodeWindow:
        Status  = DropTestBeforeDecodeWindow(Buffer);
        break;

    case OutputTimerBeforeDecode:
        Status  = DropTestBeforeDecode(Buffer);
        break;

    case OutputTimerBeforeOutputTiming:
        Status  = DropTestBeforeOutputTiming(Buffer);
        break;

    case OutputTimerBeforeManifestation:
        Status  = DropTestBeforeManifestation(Buffer);
        break;

    default:
        Status  = OutputTimerNoError;
    }

    if (Status != OutputTimerNoError)
    {
        IncrementTestFrameDropStatistics(Status, TestPoint);
    }

    return Status;
}


// /////////////////////////////////////////////////////////////////////////
//
//  The function to test for frame drop before decode
//

OutputTimerStatus_t   OutputTimer_Base_c::DropTestBeforeDecodeWindow(Buffer_t     Buffer)
{
    OutputTimerStatus_t   Status;
    unsigned long long    StartInterval;
    unsigned long long    EndInterval;
    unsigned long long    Duration;
    unsigned long long    DurationEnd;
    Rational_t        Speed;
    PlayDirection_t       Direction;

    //
    // If this is not the first decode of a frame,
    // then we repeat our previous decision
    // (IE we cannot drop just slice, or a single field).
    //

    ParsedFrameParameters_t  *ParsedFrameParameters = GetParsedFrameParameters(Buffer);

    if (!ParsedFrameParameters->FirstParsedParametersForOutputFrame)
    {
        return mLastFrameDropPreDecodeDecision;
    }

    //
    // Update the trick mode parameters, and the group structure information
    //
    Stream->GetCodec()->GetTrickModeParameters(&mTrickModeParameters);
    Player->GetPlaybackSpeed(Playback, &Speed, &Direction);

    // Do not call MonitorGroupStructure in case of not smooth backward.
    // For this case, it has already been called in frame parser RevPlayProcessFrame, before Bs and Ps are dropped.
    // Here it would be too late.
    if ((mSmoothReverseSupport && (Direction == PlayBackward)) ||
        (Direction != PlayBackward))
    {
        MonitorGroupStructure(ParsedFrameParameters);
    }

    //
    // Update the key frame count
    //

    if (ParsedFrameParameters->FirstParsedParametersAfterInputJump)
    {
        mKeyFramesSinceDiscontinuity   = 0;
    }

    if (ParsedFrameParameters->KeyFrame)
    {
        mLastKeyFramePlaybackTime    = ParsedFrameParameters->NormalizedPlaybackTime;
        mKeyFramesSinceDiscontinuity++;
    }

    //
    // Handle the playing of single groups policy
    //
    int SingleGroupPlayback = Player->PolicyValue(Playback, Stream, PolicyStreamSingleGroupBetweenDiscontinuities);

    if ((SingleGroupPlayback == PolicyValueApply) &&
        (mKeyFramesSinceDiscontinuity >= 2))
    {
        mLastFrameDropPreDecodeDecision  = OutputTimerDropFrameSingleGroupPlayback;
        return mLastFrameDropPreDecodeDecision;
    }

    //
    // If only independant frames should be decoded, then drop all non-independant frames
    // Note that the policy name states "Decode key frames", but in H264 and HEVC, we can decode also non-key independant frames
    // as no reference will be needed when this policy is set
    //

    int TrickModePolicy = Player->PolicyValue(Playback, Stream, PolicyTrickModeDomain);
    if (!(ParsedFrameParameters->IndependentFrame)
        && (
            (TrickModePolicy == PolicyValueTrickModeDecodeKeyFrames)
            || mTrickModeDomain == PolicyValueTrickModeDecodeKeyFrames
            || Player->PolicyValue(Playback, Stream, PolicyStreamOnlyKeyFrames) == PolicyValueApply
            || Player->PolicyValue(Playback, Stream, PolicyStreamDiscardFrames) == PolicyValueKeyFramesOnly
        )
       )
    {
        // In this case, we migh receive spurious non key frame, they should be discarded
        mLastFrameDropPreDecodeDecision  = OutputTimerDropFrameKeyFramesOnly;
        return mLastFrameDropPreDecodeDecision;
    }

    //
    // Handle the key frames only policy - switched the meaning of this
    // to be play independent frames only IE I frames (only in h264 is there a
    // distinction between I and key frames (IDRs in 264).
    //

    int IndependentFramesOnly = Player->PolicyValue(Playback, Stream, PolicyStreamOnlyKeyFrames);

    if (!ParsedFrameParameters->IndependentFrame &&
        (IndependentFramesOnly == PolicyValueApply))
    {
        mLastFrameDropPreDecodeDecision  = OutputTimerDropFrameKeyFramesOnly;
        return mLastFrameDropPreDecodeDecision;
    }

    int Policy = Player->PolicyValue(Playback, Stream, PolicyStreamDiscardFrames);

    if (!ParsedFrameParameters->IndependentFrame && (Policy == PolicyValueKeyFramesOnly))
    {
        // skip if it is a non I frame
        mLastFrameDropPreDecodeDecision  = OutputTimerDropFrameKeyFramesOnly;
        return mLastFrameDropPreDecodeDecision;
    }
    else if (!ParsedFrameParameters->ReferenceFrame &&
             (Policy == PolicyValueReferenceFramesOnly))
    {
        // skip if it is a B frame
        mLastFrameDropPreDecodeDecision  = OutputTimerDropFrameReferenceFramesOnly;
        return mLastFrameDropPreDecodeDecision;
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
    Player->GetPresentationInterval(Stream, &StartInterval, &EndInterval);

    if (((!ParsedFrameParameters->KeyFrame && PlaybackTimeCheckGreaterThanOrEqual(mLastKeyFramePlaybackTime, EndInterval) && (Direction == PlayForward))) ||
        ((!ParsedFrameParameters->KeyFrame && PlaybackTimeCheckGreaterThanOrEqual(EndInterval, mLastKeyFramePlaybackTime) && (Direction == PlayBackward))))
    {
        mLastFrameDropPreDecodeDecision  = OutputTimerDropFrameOutsidePresentationInterval;
        SE_VERBOSE(group_output_timer, "Stream 0x%p skipping non key frame, Direction %d, endI %lld LastIPlaybackTime %lld \n",  Stream, Direction, EndInterval, mLastKeyFramePlaybackTime);
        return mLastFrameDropPreDecodeDecision;
    }

    if (!ParsedFrameParameters->ReferenceFrame)
    {
        Status  = FrameDuration(GetParsedAudioVideoDataParameters(Buffer), &Duration);

        if (Status == OutputTimerNoError)
        {
            DurationEnd = (ParsedFrameParameters->NormalizedPlaybackTime == INVALID_TIME) ? INVALID_TIME : (ParsedFrameParameters->NormalizedPlaybackTime + Duration);

            if (((Direction == PlayForward) &&
                 (!PlaybackTimeCheckIntervalsIntersect(ParsedFrameParameters->NormalizedPlaybackTime, DurationEnd, StartInterval, EndInterval, OutputTimerBeforeDecodeWindow))) ||
                ((Direction == PlayBackward) &&
                 (!PlaybackTimeCheckIntervalsIntersect(ParsedFrameParameters->NormalizedPlaybackTime, DurationEnd, EndInterval, StartInterval, OutputTimerBeforeDecodeWindow))))

            {
                mLastFrameDropPreDecodeDecision  = OutputTimerDropFrameOutsidePresentationInterval;
                SE_VERBOSE(group_output_timer, "Stream 0x%p Skipping non ref frame, Direction %d PTS %lld, Durationend %lld, EndInterval %lld, StartInterval %lld \n",  Stream, Direction,
                           ParsedFrameParameters->NormalizedPlaybackTime,
                           DurationEnd, EndInterval, StartInterval);
                return mLastFrameDropPreDecodeDecision;
            }
        }
    }

    return OutputTimerNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
//  The function to test for frame drop before decode
//

OutputTimerStatus_t   OutputTimer_Base_c::DropTestBeforeDecode(Buffer_t       Buffer)
{
    OutputTimerStatus_t   Status;
    Rational_t        Speed;
    PlayDirection_t       Direction;
    //
    // Read speed policy etc
    //
    Player->GetPlaybackSpeed(Playback, &Speed, &Direction);

    if (((Direction == PlayBackward) && !mConfiguration.ReversePlaySupported) ||
        ((Speed != 0) && !inrange(Speed, mConfiguration.MinimumSpeedSupported, mConfiguration.MaximumSpeedSupported)))
    {
        mNextExpectedPlaybackTime    = INVALID_TIME;     // If we are discarding based on trick mode
        // we may well do this for a long time.
        mLastFrameDropPreDecodeDecision  = OutputTimerTrickModeNotSupportedDropFrame;
        return mLastFrameDropPreDecodeDecision;
    }

    //
    // If this is not the first decode of a frame,
    // then we repeat our previous decision
    // (IE we cannot drop just slice, or a single field).
    //

    ParsedFrameParameters_t  *ParsedFrameParameters = GetParsedFrameParameters(Buffer);

    if (!ParsedFrameParameters->FirstParsedParametersForOutputFrame)
    {
        return mLastFrameDropPreDecodeDecision;
    }

    //
    // Allow trick mode control to test for a change in behaviour
    //
    Status  = TrickModeControl();

    if (Status != OutputTimerNoError)
    {
        return Status;
    }

    if (ParsedFrameParameters->KeyFrame && (mTrickModeCheckReferenceFrames != 0))
    {
        mTrickModeCheckReferenceFrames--;
    }

    //
    // Trick mode decisions, trick mode discarding is done once we enter the decode window
    //
    Status  = OutputTimerNoError;

    switch (mTrickModeDomain)
    {
    case TrickModeInvalid:
    case TrickModeDecodeAll:
        break;

    case PolicyValueTrickModeDecodeAllDegradeNonReferenceFrames:
        ParsedFrameParameters->ApplySubstandardDecode        = mTrickModeParameters.SubstandardDecodeSupported && !ParsedFrameParameters->ReferenceFrame;
        break;

    case TrickModeStartDiscardingNonReferenceFrames:
        if (!ParsedFrameParameters->ReferenceFrame)
        {
            ParsedFrameParameters->ApplySubstandardDecode    = mTrickModeParameters.SubstandardDecodeSupported;
            mAccumulatedPreDecodeFramesToLose            += mPortionOfPreDecodeFramesToLose;

            if (mAccumulatedPreDecodeFramesToLose >= 1)
            {
                mAccumulatedPreDecodeFramesToLose        -= 1;
                Status                       = OutputTimerTrickModeDropFrame;
            }
        }

        break;

    case TrickModeDecodeReferenceFramesDegradeNonKeyFrames:
        ParsedFrameParameters->ApplySubstandardDecode        = mTrickModeParameters.SubstandardDecodeSupported && !ParsedFrameParameters->IndependentFrame;

        if (!ParsedFrameParameters->ReferenceFrame)
        {
            Status                         = OutputTimerTrickModeDropFrame;
        }

        break;

    case TrickModeDecodeKeyFrames:
    case TrickModeDiscontinuousKeyFrames:
        if (!ParsedFrameParameters->IndependentFrame)
        {
            Status                         = OutputTimerTrickModeDropFrame;
        }

        break;
    }

    //
    // Now cater for the drop until key frame recovery from very fast forward
    //

    if ((mTrickModeCheckReferenceFrames != 0) &&
        (Status == OutputTimerNoError))
    {
        Status  = Stream->GetCodec()->CheckReferenceFrameList(ParsedFrameParameters->NumberOfReferenceFrameLists,
                                                              ParsedFrameParameters->ReferenceFrameList);

        if (Status != OutputTimerNoError)
        {
            Status  = OutputTimerTrickModeDropFrame;

            if (mTrickModeDomain == TrickModeStartDiscardingNonReferenceFrames)
            {
                mAccumulatedPreDecodeFramesToLose  -= 1;
            }
        }
    }

    //
    // We are not going to drop the frame, if we are transitioning from an unsupported trick mode
    // where we would drop all frames, then we haven't been around for a while, and need to
    // reset our sync monitors
    //

    if (mLastFrameDropPreDecodeDecision == OutputTimerTrickModeNotSupportedDropFrame)
    {
        mGlobalSynchronizationState  = SyncStateStartAwaitingCorrectionWorkthrough;
        mDecodeInTimeState       = DecodingInTime;
        mSeenAnInTimeFrame       = false;
    }

    //
    // If we have reached this point, then we have not discarded,
    // and we update the last decision variable.
    //
    mLastFrameDropPreDecodeDecision  = Status;
    return Status;
}


// /////////////////////////////////////////////////////////////////////////
//
//  The function to test for frame drop before decode
//

OutputTimerStatus_t   OutputTimer_Base_c::DropTestBeforeOutputTiming(Buffer_t         Buffer)
{
    OutputTimerStatus_t      Status;
    unsigned long long       StartInterval;
    unsigned long long       EndInterval;
    unsigned long long       DurationEnd;
    unsigned long long       Duration;
    Rational_t               Speed;
    PlayDirection_t          Direction;

    //
    // Check for presentation interval.
    // For all frames we can dump the frame if
    // we know it does not intersect the interval.
    //
    Player->GetPlaybackSpeed(Playback, &Speed, &Direction);
    Player->GetPresentationInterval(Stream, &StartInterval, &EndInterval);
    Status  = FrameDuration(GetParsedAudioVideoDataParameters(Buffer), &Duration);

    if (Status == OutputTimerNoError)
    {

        ParsedFrameParameters_t *ParsedFrameParameters = GetParsedFrameParametersReference(Buffer);

        DurationEnd = (ParsedFrameParameters->NormalizedPlaybackTime == INVALID_TIME) ? INVALID_TIME : (ParsedFrameParameters->NormalizedPlaybackTime + Duration);

        if (((!PlaybackTimeCheckIntervalsIntersect(ParsedFrameParameters->NormalizedPlaybackTime, DurationEnd, StartInterval, EndInterval, OutputTimerBeforeOutputTiming))
             && (Direction == PlayForward)) ||
            ((!PlaybackTimeCheckIntervalsIntersect(ParsedFrameParameters->NormalizedPlaybackTime, DurationEnd, EndInterval, StartInterval, OutputTimerBeforeOutputTiming))
             && (Direction == PlayBackward)))
        {
            SE_VERBOSE(group_output_timer, "Stream 0x%p Skipping non ref frame, Direction %d PTS %lld, Durationend %lld, EndInterval %lld, StartInterval %lld \n",  Stream, Direction,
                       ParsedFrameParameters->NormalizedPlaybackTime, DurationEnd, EndInterval, StartInterval);
            return OutputTimerDropFrameOutsidePresentationInterval;
        }
    }

    return OutputTimerNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
//  The function to test for frame drop before decode
//

OutputTimerStatus_t   OutputTimer_Base_c::DropTestBeforeManifestation(Buffer_t        Buffer)
{
    Rational_t            Speed;
    PlayDirection_t           Direction;
    unsigned long long        Now;

    //
    // Drop the frame if it is untimed
    //
    Player->GetPlaybackSpeed(Playback, &Speed, &Direction);
    int TrickModePolicy = Player->PolicyValue(Playback, Stream, PolicyTrickModeDomain);

    // Check on valid time is not meaningful in case of I only decode mode.
    // In case of I only decode mode, we can have only I frames injected separated by discontinuity.
    // In case the I frame doesn't have an associated PTS, ParsedFrameParameters->NormalizedPlaybackTime will be
    // invalid because the discontinuity prevents computing it from former PTS.

    ParsedFrameParameters_t *ParsedFrameParameters  = GetParsedFrameParametersReference(Buffer);
    OutputTiming_t          *OutputTiming           = GetOutputTiming(Buffer);

    if (((NotValidTime(ParsedFrameParameters->NormalizedPlaybackTime)) &&
         (TrickModePolicy != TrickModeDecodeKeyFrames)) ||
        ((Speed != 0) && (NotValidTime(OutputTiming->BaseSystemPlaybackTime))))
    {
        return OutputTimerUntimedFrame;
    }


    //
    // Drop the frame if it is too late for manifestation or (really) too far in the future
    //
    Now = OS_GetTimeInMicroSeconds();
    if ((Speed != 0) && ((Now + MANIFESTATION_WINDOW_WIDTH) < OutputTiming->BaseSystemPlaybackTime))
    {
        SE_WARNING("Stream 0x%p - %s - Frame is to be presented too far in the future (window=%lld BaseSystemPlaybackTime=%lld)\n"
                   , Stream
                   , mConfiguration.OutputTimerName
                   , MANIFESTATION_WINDOW_WIDTH
                   , OutputTiming->BaseSystemPlaybackTime);
        return OutputTimerDropFrameTooEarlyForManifestation;
    }

    SE_VERBOSE(group_avsync, "Stream 0x%p Frame timings - Now:%lld MinimumManifestorLatency:%lld BaseSystemPlaybackTime:%lld\n"
            , Stream
            , Now
            , mConfiguration.MinimumManifestorLatency
            , OutputTiming->BaseSystemPlaybackTime);

    if ((Speed != 0) && ((Now + mConfiguration.MinimumManifestorLatency) >= OutputTiming->BaseSystemPlaybackTime))
    {
	  SE_DEBUG(group_avsync, "Stream 0x%p DecodeInTimeFailure - Now:%lld MinimumManifestorLatency:%lld BaseSystemPlaybackTime:%lld\n"
                 , Stream
                 , Now
                 , mConfiguration.MinimumManifestorLatency
                 , OutputTiming->BaseSystemPlaybackTime);

        DecodeInTimeFailure((Now + mConfiguration.MinimumManifestorLatency) - OutputTiming->BaseSystemPlaybackTime,
                            OutputTiming->BaseSystemPlaybackTime < ParsedFrameParameters->CollationTime);
        int DropLateFramesPolicy = Player->PolicyValue(Playback, Stream, PolicyDiscardLateFrames);

        if (Player->PolicyValue(Playback, Stream, PolicyLivePlayback) == PolicyValueApply)
        {
            DropLateFramesPolicy  = PolicyValueDiscardLateFramesNever;
        }

        switch (DropLateFramesPolicy)
        {
        case PolicyValueDiscardLateFramesNever:
            break;

        case PolicyValueDiscardLateFramesAfterSynchronize:
            if (!mSeenAnInTimeFrame)
            {
                // If this is audio in a video play immediate case
                // we need to ensure that we do not mistakenly think
                // that we are failing to decode in time
                int VideoImmediatePolicy = Player->PolicyValue(Playback, Stream, PolicyVideoStartImmediate);

                if ((mConfiguration.StreamType == StreamTypeAudio) &&
                    (VideoImmediatePolicy == PolicyValueApply))
                {
                    mGlobalSynchronizationState  = SyncStateStartAwaitingCorrectionWorkthrough;
                    mDecodeInTimeState       = DecodingInTime;
                }
            }
            else
            {
                break;
            }

        case PolicyValueDiscardLateFramesAlways:
            return OutputTimerDropFrameTooLateForManifestation;
            break;
        }
    }
    else
    {
        mDecodeInTimeState   = DecodingInTime;
        mSeenAnInTimeFrame   = true;
    }

    //
    // Trick mode decisions
    //
    //
    // Re-check the before decode drop, to specifically
    // cope with pause followed by new trick mode transition.
    //
    Player->GetPlaybackSpeed(Playback, &Speed, &Direction);

    if (((Direction == PlayBackward) && !mConfiguration.ReversePlaySupported) ||
        ((Speed != 0) && !inrange(Speed, mConfiguration.MinimumSpeedSupported, mConfiguration.MaximumSpeedSupported)))
    {
        return OutputTimerTrickModeNotSupportedDropFrame;
    }

    return OutputTimerNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
//  The function to actually generate an output timing record
//

OutputTimerStatus_t   OutputTimer_Base_c::GenerateFrameTiming(
    Buffer_t          Buffer)
{
    OutputTimerStatus_t     Status;
    unsigned long long      NormalizedPlaybackTime;
    unsigned long long      NormalizedDecodeTime;
    unsigned long long      SystemTime;
    bool                    BufferForReTiming;
    bool                    KnownPlaybackJump;
    long long int           DeltaCollationTime;

    AssertComponentState(ComponentRunning);

    OutputTiming_t *OutputTiming = GetOutputTiming(Buffer);

    BufferForReTiming = OutputTiming->RetimeBuffer;
    memset(OutputTiming, 0, sizeof(OutputTiming_t));

    for (int i = 0; i < MAXIMUM_MANIFESTATION_TIMING_COUNT; i++)
    {
        OutputTiming->ManifestationTimings[i].OutputRateAdjustment  = 0; // rational
        OutputTiming->ManifestationTimings[i].SystemClockAdjustment = 1; // rational
    }

    //
    // Read synchronization policy and check for it being turned on
    //
    int SynchronizationPolicy = Player->PolicyValue(Playback, Stream, PolicyAVDSynchronization);

    if ((mLastSynchronizationPolicy == PolicyValueDisapply) &&
        (SynchronizationPolicy == PolicyValueApply))
    {
        ResetTimeMapping();
    }

    mLastSynchronizationPolicy   = SynchronizationPolicy;
    //
    // Check for large PTS changes, delta the mapping if we find one.
    // but only if this is not a re-timing exercise
    //
    ParsedFrameParameters_t *ParsedFrameParameters  = GetParsedFrameParametersReference(Buffer);

    NormalizedPlaybackTime  = ParsedFrameParameters->NormalizedPlaybackTime + (unsigned long long)mNormalizedTimeOffset;

    if (BufferForReTiming)
    {
        mNextExpectedPlaybackTime  = INVALID_TIME;
    }

    if (ValidTime(ParsedFrameParameters->NormalizedPlaybackTime))
    {
        if (NotValidTime(mLastFrameCollationTime))
        {
            mLastFrameCollationTime    = ParsedFrameParameters->CollationTime;
        }

        if (ValidTime(mNextExpectedPlaybackTime))
        {
            KnownPlaybackJump   = ParsedFrameParameters->FirstParsedParametersAfterInputJump && !ParsedFrameParameters->ContinuousReverseJump;
            DeltaCollationTime  = ParsedFrameParameters->SpecifiedPlaybackTime ?
                                  ((long long int)ParsedFrameParameters->CollationTime - (long long int)mLastFrameCollationTime) :
                                  INVALID_TIME;
            int trickmodepolicy = Player->PolicyValue(Playback, Stream, PolicyTrickModeDomain);
            SE_VERBOSE(group_output_timer, "Stream 0x%p trickmodepolicy=%d\n", Stream, trickmodepolicy);

            if (trickmodepolicy  != PolicyValueTrickModeDecodeKeyFrames
                && mTrickModeDomain  != PolicyValueTrickModeDecodeKeyFrames
                && Player->PolicyValue(Playback, Stream, PolicyStreamOnlyKeyFrames) != PolicyValueApply
                && Player->PolicyValue(Playback, Stream, PolicyStreamDiscardFrames) != PolicyValueKeyFramesOnly
               )
            {
                Status  = mOutputCoordinator->HandlePlaybackTimeDeltas(mOutputCoordinatorContext,
                                                                       KnownPlaybackJump,
                                                                       mNextExpectedPlaybackTime,
                                                                       NormalizedPlaybackTime,
                                                                       DeltaCollationTime);
                if (Status != OutputCoordinatorNoError)
                {
                    SE_ERROR("Stream 0x%p - %s - Unable to handle playback time deltas\n", Stream, mConfiguration.OutputTimerName);
                    return Status;
                }
            }
            else
            {
                SE_VERBOSE(group_output_timer, "Stream 0x%p Stubbing jump detection in I-only mode for frame with PTS=%lld \n"
                           , Stream
                           , ParsedFrameParameters->NativePlaybackTime);
            }
        }

        mNextExpectedPlaybackTime    = NormalizedPlaybackTime;

        if (ParsedFrameParameters->SpecifiedPlaybackTime)
        {
            mLastFrameCollationTime    = ParsedFrameParameters->CollationTime;
        }
    }

    //
    // Now we are applying synchronization, and we have a valid time to play with ...
    //
    Rational_t          Speed;
    PlayDirection_t     Direction;

    Player->GetPlaybackSpeed(Playback, &Speed, &Direction);

    if ((Speed != 0) &&
        (SynchronizationPolicy == PolicyValueApply) &&
        ValidTime(ParsedFrameParameters->NormalizedPlaybackTime))
    {
        //
        // Translate the playback time to a system time
        //
        Status          = mOutputCoordinator->TranslatePlaybackTimeToSystem(mOutputCoordinatorContext, NormalizedPlaybackTime, &SystemTime);

        if (Status == OutputCoordinatorMappingNotEstablished)
        {
            NormalizedDecodeTime = ValidTime(ParsedFrameParameters->NormalizedDecodeTime) ?
                                   (ParsedFrameParameters->NormalizedDecodeTime + (unsigned long long)mNormalizedTimeOffset) :
                                   NormalizedPlaybackTime - mConfiguration.FrameDecodeTime;

            Status = mOutputCoordinator->SynchronizeStreams(
                         mOutputCoordinatorContext, NormalizedPlaybackTime, NormalizedDecodeTime, &SystemTime, GetParsedAudioVideoDataParameters(Buffer));
            mTimeMappingValidForDecodeTiming = true;
            mGlobalSynchronizationState      = SyncStateInSynchronization;
            mGlobalSetSynchronizationAtStartup   = true;
            mDecodeInTimeState           = DecodingInTime;
            mSeenAnInTimeFrame           = false;
        }

        if (Status != OutputCoordinatorNoError)
        {
            SE_ERROR("Stream 0x%p - %s - Failed to obtain system time for playback\n", Stream, mConfiguration.OutputTimerName);
            return Status;
        }

        //
        // Do we wish to re-validate the time mapping for the decode window timing
        //

        if (!mTimeMappingValidForDecodeTiming &&
            (!ValidIndex(mTimeMappingInvalidatedAtDecodeIndex) ||
             (ParsedFrameParameters->DecodeFrameIndex > mTimeMappingInvalidatedAtDecodeIndex)))
        {
            mTimeMappingValidForDecodeTiming = true;
        }
    }
    else
    {
        SystemTime              = UNSPECIFIED_TIME;
    }


    //
    // Generate the timing record
    //
    Status  = FillOutFrameTimingRecord(SystemTime, GetParsedAudioVideoDataParameters(Buffer), OutputTiming);
    if (Status != OutputTimerNoError)
    {
        SE_ERROR("Stream 0x%p - %s - Failed to fill out timing record\n", Stream, mConfiguration.OutputTimerName);
        return Status;
    }

    return OutputTimerNoError;
}

// /////////////////////////////////////////////////////////////////////////
//
//  The function to record the actual frame timing
//

OutputTimerStatus_t   OutputTimer_Base_c::RecordActualFrameTiming(
    Buffer_t          Buffer)
{
    unsigned int         i;
    PlayerStatus_t       Status;
    OutputSurfaceDescriptor_t *PercussiveSurface;
    unsigned long long   ExpectedTime;
    unsigned long long   ActualTime;
    Rational_t       SystemClockJerk;

    AssertComponentState(ComponentRunning);

    //
    // Maybe the policy changed since last frame
    // In this case, we have to react accordingly and update the trick state variables
    //
    int TrickModePolicy = Player->PolicyValue(Playback, Stream, PolicyTrickModeDomain);
    if (mLastTrickModePolicy != (TrickModeDomain_t)TrickModePolicy)
    {
        TrickModeControl();
    }

    //
    // Load a new set of output descriptors.
    //
    Status  = Stream->GetManifestationCoordinator()->GetSurfaceParameters(&mOutputSurfaceDescriptors, &mOutputSurfaceDescriptorsHighestIndex);

    if (Status != ManifestationCoordinatorNoError)
    {
        SE_ERROR("Stream 0x%p - %s - Failed to obtain the output surface descriptors from the Manifestation Coordinator\n", Stream, mConfiguration.OutputTimerName);
        return OutputCoordinatorError;
    }

    //
    // Calculate the clock adjustment rates to compensate for clock drift
    // NOTE if we have a sudden change in the system clock rate, we need to
    //      hold off on avsync while the output rate adjustments have a time
    //      to complete an integration.
    //

    OutputTiming_t *OutputTiming = GetOutputTiming(Buffer);

    Status  = mOutputCoordinator->CalculateOutputRateAdjustments(mOutputCoordinatorContext,
                                                                 OutputTiming,
                                                                 &mOutputRateAdjustments,
                                                                 &mSystemClockAdjustment);

    if (Status != PlayerNoError)
    {
        SE_ERROR("Stream 0x%p - %s: Failed to calculate output rate adjustments\n", Stream, mConfiguration.OutputTimerName);
    }

    SystemClockJerk = mSystemClockAdjustment - mLastSystemClockAdjustment;

    if (!inrange(SystemClockJerk, Rational_t(-4, 1000000), Rational_t(4, 1000000)))
    {
        mGlobalSynchronizationState    = SyncStateStartLongSyncHoldoff;
    }

    mLastSystemClockAdjustment       = mSystemClockAdjustment;

    //
    // Perform monitoring of vsync offsets
    //

    if (mConfiguration.StreamType == StreamTypeVideo)
    {
        Status  = mOutputCoordinator->MonitorVsyncOffsets(mOutputCoordinatorContext, OutputTiming, &mVsyncOffsets);

        if ((Status == OutputCoordinatorOffsetsNotEstablished) && (mGlobalSynchronizationState != SyncStateStartLongSyncHoldoff))
        {
            mGlobalSynchronizationState    = SyncStateInSynchronization;    // Cannot evaluate avsync until after we have done this
        }
    }

    //
    // Check if the underlying time mapping has
    // been modified to effect a synchronization change.
    //
    Status  = mOutputCoordinator->MappingBaseAdjustmentApplied(mOutputCoordinatorContext);

    if ((Status != OutputCoordinatorNoError) && (mGlobalSynchronizationState != SyncStateStartLongSyncHoldoff))
    {
        mGlobalSynchronizationState  = SyncStateStartAwaitingCorrectionWorkthrough;
        mDecodeInTimeState       = DecodingInTime;
    }

    //
    // Perform the individual AVD synchronizations
    //

    for (i = 0; i <= OutputTiming->HighestTimingIndex; i++)
    {
        ExpectedTime    = OutputTiming->ManifestationTimings[i].SystemPlaybackTime;
        ActualTime  = OutputTiming->ManifestationTimings[i].ActualSystemPlaybackTime;
        PercussiveSurface = mOutputSurfaceDescriptors[i];

        //
        // Copy any global sync state setting
        //

        if (mGlobalSynchronizationState != SyncStateNullState)
        {
            mManifestationTimingState[i].SynchronizationState  = mGlobalSynchronizationState;
        }

        if (mGlobalSetSynchronizationAtStartup)
        {
            mManifestationTimingState[i].SynchronizationAtStartup = true;
        }

        //
        // Was the frame manifested on this manifestation
        //

        if (NotValidTime(ExpectedTime) || NotValidTime(ActualTime) || PercussiveSurface == NULL)
        {
            continue;
        }

        //
        // Perform Audio/Video/Data synchronization
        //

        if (PercussiveSurface->PercussiveCapable)
        {
            if (PercussiveSurface->IsSlavedSurface)
            {
                // if the surface is a slaved surface, the percussive adjustment must be be impacted on the master surface
                int MasterSurfaceTimingIndex = i - (mOutputSurfaceDescriptors[i] - PercussiveSurface->MasterSurface);
                Status  = PerformAVDSync(Buffer, MasterSurfaceTimingIndex, ExpectedTime, ActualTime, OutputTiming);
            }
            else
            {
                Status    = PerformAVDSync(Buffer, i, ExpectedTime, ActualTime, OutputTiming);
            }

            if (Status != PlayerNoError)
            {
                SE_ERROR("Stream 0x%p - %s - Failed to perform synchronization\n", Stream, mConfiguration.OutputTimerName);
                continue;
            }
        }
    }

    //
    // Clear the global synchronization state, and return
    //
    mGlobalSynchronizationState      = SyncStateNullState;
    mGlobalSetSynchronizationAtStartup   = false;
    return OutputTimerNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
//  The function to extract the stream delay relative to other
//  streams in the playback (Note returns coordinator values)
//

OutputTimerStatus_t   OutputTimer_Base_c::GetStreamStartDelay(unsigned long long     *Delay)
{
    if (mOutputCoordinatorContext == NULL)
    {
        return OutputCoordinatorMappingNotEstablished;
    }

    return mOutputCoordinator->GetStreamStartDelay(mOutputCoordinatorContext, Delay);
}

const char *OutputTimer_c::StringifyOutputTimerStatus(OutputTimerStatus_t Status)
{

    switch (Status)
    {
    case OutputTimerDropFrame :
        return "OutputTimerDropFrame";
    case OutputTimerDropFrameSingleGroupPlayback:
        return "OutputTimerDropFrameSingleGroupPlayback";
    case OutputTimerDropFrameKeyFramesOnly:
        return "OutputTimerDropFrameKeyFramesOnly";
    case OutputTimerDropFrameReferenceFramesOnly:
        return "OutputTimerDropFrameReferenceFramesOnly";
    case OutputTimerDropFrameOutsidePresentationInterval:
        return "OutputTimerDropFrameOutsidePresentationInterval";
    case OutputTimerUntimedFrame:
        return "OutputTimerUntimedFrame";

    case OutputTimerTrickModeNotSupportedDropFrame:
        return "OutputTimerTrickModeNotSupportedDropFrame";
    case OutputTimerTrickModeDropFrame:
        return "OutputTimerTrickModeDropFrame";
    case OutputTimerLateDropFrame:
        return "OutputTimerLateDropFrame";

    case OutputTimerDropFrameTooLateForManifestation:
        return "OutputTimerDropFrameTooLateForManifestation";
    case OutputTimerDropFrameTooEarlyForManifestation:
        return "OutputTimerDropFrameTooEarlyForManifestation";
    default:
        return "Unknown error";
    };

}

// /////////////////////////////////////////////////////////////////////////
//
//  The function responsible for managing AVD synchronization
//

OutputTimerStatus_t   OutputTimer_Base_c::PerformAVDSync(
    Buffer_t              Buffer,
    unsigned int          TimingIndex,
    unsigned long long    ExpectedFrameOutputTime,
    unsigned long long    ActualFrameOutputTime,
    OutputTiming_t           *OutputTiming)
{
    OutputTimerStatus_t       Status;
    ManifestationTimingState_t   *State;
    unsigned int                      i;
    long long             SynchronizationDifference;
    long long             ErrorThreshhold;
    bool                  DifferenceExceedsThreshhold;

    SE_ASSERT(!mOutputSurfaceDescriptors[TimingIndex]->IsSlavedSurface);
    State       = &mManifestationTimingState[TimingIndex];
    //
    // Calculate the current error threshold, first using a base threshold,
    // and then adding any legitimate offset relating to this specific manifestation
    //
    State->SynchronizationFrameCount++;
    //
    // Determine whether to use the default error threshold (for when we can freely deploy percussive
    // adjustments) or the quantized error threshold (when we have some limit)
    //
    int MasterClockPolicy = Player->PolicyValue(Playback, Stream, PolicyMasterClock);
    int ExternalMappingPolicy = Player->PolicyValue(Playback, Stream, PolicyExternalTimeMapping);
    int ExternalTimeMappingVsyncLockedPolicy = Player->PolicyValue(Playback, Stream, PolicyExternalTimeMappingVsyncLocked);

    if ((MasterClockPolicy == PolicyValueAudioClockMaster && mConfiguration.StreamType == StreamTypeAudio) ||
        (ExternalMappingPolicy == PolicyValueApply && ExternalTimeMappingVsyncLockedPolicy == PolicyValueApply &&
         mConfiguration.StreamType == StreamTypeVideo))
    {
        ErrorThreshhold = mConfiguration.SynchronizationErrorThreshold;
    }
    else
    {
        ErrorThreshhold = mConfiguration.SynchronizationErrorThresholdForQuantizedSync;
    }

    //
    // Widen the error threshhold when we are not targeting a quantized threshhold. There should
    // be no need to widen a quantized threshhold because the quantum can be assumed to be large.
    //
    // Note this branch is distinct from the section above because for streams without any
    // quanitization effect SynchronizationErrorThreshold == SynchronizationErrorThresholdForQuantizedSync .
    //

    if ((unsigned long long) ErrorThreshhold == mConfiguration.SynchronizationErrorThreshold)
    {
        //
        // When running a stream, we have a loose sync threshold that
        // gradually tightens over 3..5 mins this seems worse than it is,
        // most of the error during this period is due to the error in the
        // clock adjustments being worked out.
        //
        ErrorThreshhold     = mConfiguration.SynchronizationErrorThreshold;

        if (State->SynchronizationFrameCount < 2048)
        {
            ErrorThreshhold   = max(ErrorThreshhold, (4 * State->SynchronizationFrameCount));
        }
        else if (State->SynchronizationFrameCount < 4096)
        {
            ErrorThreshhold   = 4 * ErrorThreshhold;
        }
        else if (State->SynchronizationFrameCount < 6144)
        {
            ErrorThreshhold   = 2 * ErrorThreshhold;
        }
        else if (State->SynchronizationFrameCount < 8192)
        {
            ErrorThreshhold   = (3 * ErrorThreshhold) / 2;
        }
    }

    if (mVsyncOffsets != NULL)
    {
        ErrorThreshhold += Abs(mVsyncOffsets[TimingIndex]);
    }

    SynchronizationDifference   = (long long)ActualFrameOutputTime - (long long)ExpectedFrameOutputTime;
    DifferenceExceedsThreshhold = !inrange(SynchronizationDifference, -ErrorThreshhold, ErrorThreshhold);
    SE_VERBOSE(group_avsync,
               "Stream 0x%p - %d - SynchronizationDifference=%lld Actual=%lld Expected=%lld ErrorThreshold=%lld SynchronizationState=%d SynchronizationErrorIntegrationCount=%d FrameWorkthroughCount=%d\n",
               Stream, mConfiguration.StreamType, SynchronizationDifference, ActualFrameOutputTime, ExpectedFrameOutputTime,
               ErrorThreshhold, State->SynchronizationState, State->SynchronizationErrorIntegrationCount, State->FrameWorkthroughCount);

    //
    // Now a switch statement depending on the state we are in
    //

    switch (State->SynchronizationState)
    {
    //
    // We believe we are in sync, can transition to suspected error.
    // if we remain in sync, then we reset the startup flag, as any
    // future errors cannot be at startup.
    //
    case SyncStateNullState:
    case SyncStateInSynchronization:
        if (DifferenceExceedsThreshhold)
        {
            State->SynchronizationState            = SyncStateSuspectedSyncError;
            State->SynchronizationAccumulatedError = SynchronizationDifference;
            State->SynchronizationPreviousError    = SynchronizationDifference;
            State->SynchronizationErrorStabilityCounter = 0;
            State->SynchronizationErrorIntegrationCount = 1;

        }

        break;

    //
    // We have a suspected sync error, we integrate over a number of values
    // to eliminate glitch effects. If we confirm the error then we fall through
    // to the next state.
    //

    case SyncStateSuspectedSyncError:
        if (DifferenceExceedsThreshhold && samesign(SynchronizationDifference, State->SynchronizationAccumulatedError))
        {
            State->SynchronizationAccumulatedError      += SynchronizationDifference;
            if (abs64(SynchronizationDifference - State->SynchronizationPreviousError) < abs64(SynchronizationDifference) >> 4)
            {
                // We are observing a stable error
                State->SynchronizationErrorStabilityCounter ++;
            }
            else
            {
                State->SynchronizationErrorStabilityCounter = 0;
            }


            State->SynchronizationPreviousError    = SynchronizationDifference;
            State->SynchronizationErrorIntegrationCount++;

            // Extend threshold  by 10 for first attempt to synchronize, just to be sure, it kicks in only when absolutely necessary
            // So if threshold is exceeded, it is considered as a new stream and time mapping will be re-established, if the threshold is not
            // exceeded, it is considered as the same stream with some disruption, therefore no drain or reset of time mapping.
            if (!inrange(SynchronizationDifference, -(10 * MAX_SYNCHRONIZATION_DIFFERENCE), 10 * MAX_SYNCHRONIZATION_DIFFERENCE))
            {
                SE_WARNING("Stream 0x%p Too far to sync %lldus\n", Stream, SynchronizationDifference);
                mOutputCoordinator->DrainLivePlayback();
            }

            if (State->SynchronizationErrorIntegrationCount < mConfiguration.SynchronizationIntegrationCount)
            {
                break;
            }
        }
        else
        {
            State->SynchronizationState         = SyncStateInSynchronization;
            State->SynchronizationAtStartup         = false;
            break;
        }

        // Fallthrough
        State->SynchronizationState             = SyncStateConfirmedSyncError;

    //
    // Confirmed sync error, entered by a fallthrough,
    // We request the stream specific function to correct
    // it, then we fallthrough to start awaiting the
    // correction to work through.
    //

    case SyncStateConfirmedSyncError:
        // Should adjustment be performed on average or based on last value ?
        // if error is not stable over integration period, using average makes no sense
        if (State->SynchronizationErrorStabilityCounter >= State->SynchronizationErrorIntegrationCount
            && State->SynchronizationErrorIntegrationCount != 0)
        {
            // Averaging when error is stable will absorb the jitter
            State->SynchronizationError = State->SynchronizationAccumulatedError / State->SynchronizationErrorIntegrationCount;
        }
        else
        {
            // This is the best we can do : use latest known error
            State->SynchronizationError = State->SynchronizationPreviousError;
        }

        SE_INFO(group_avsync, "Stream 0x%p AVDsync %s %s - %6lld us (error %s :  %d<%d) ErrorThreshold = %lld\n", Stream,
                ((ExternalMappingPolicy == PolicyValueApply) ? "Error" : "Initiating percussive adjustment"),
                LookupStreamType(mConfiguration.StreamType)
                , State->SynchronizationError
                , (State->SynchronizationErrorStabilityCounter >= State->SynchronizationErrorIntegrationCount ? "stable" : "unstable")
                , State->SynchronizationErrorStabilityCounter
                , State->SynchronizationErrorIntegrationCount
                , ErrorThreshhold);
        mStreamNotifiedInSync        = false;
        Status          = CorrectSynchronizationError(Buffer, TimingIndex);

        if (Status != OutputTimerNoError) { SE_ERROR("Stream 0x%p Failed to correct error\n", Stream); }

        //
        // Synchronization corrections can often mess up the output rate integration,
        // so we restart it here, but only if this was a correctable thing.
        //

        if (ExternalMappingPolicy == PolicyValueDisapply)
        {
            mOutputCoordinator->RestartOutputRateIntegration(mOutputCoordinatorContext, false, TimingIndex);

            for (i = 0; i <= OutputTiming->HighestTimingIndex; i++)
            {
                OutputSurfaceDescriptor_t *SlavedSurface = mOutputSurfaceDescriptors[i];

                if (SlavedSurface == NULL) { continue; }

                // also restart output rate integration of any slaved surface
                if (SlavedSurface->IsSlavedSurface && SlavedSurface->MasterSurface == mOutputSurfaceDescriptors[TimingIndex])
                {
                    mOutputCoordinator->RestartOutputRateIntegration(mOutputCoordinatorContext, false, i);
                }
            }
        }

        // Fallthrough
        State->SynchronizationState         = SyncStateStartAwaitingCorrectionWorkthrough;

    //
    // Here we set up to wait for as correction to work through
    //

    case SyncStateStartAwaitingCorrectionWorkthrough:
        State->SynchronizationState         = SyncStateAwaitingCorrectionWorkthrough;

        break;

    //
    // Here we handle waiting for a correction to workthrough
    //

    case SyncStateAwaitingCorrectionWorkthrough:
        State->FrameWorkthroughCount++;

        if (State->FrameWorkthroughCount >= State->FrameWorkthroughFinishCount)
        {
            State->FrameWorkthroughCount      = 0;
            State->SynchronizationState       = SyncStateInSynchronization;
            State->SynchronizationAtStartup   = false;

            if (!inrange(SynchronizationDifference, -MAX_SYNCHRONIZATION_DIFFERENCE, MAX_SYNCHRONIZATION_DIFFERENCE))
            {
                SE_WARNING("Stream 0x%p Failed to sync %lldus\n", Stream, SynchronizationDifference);
                mOutputCoordinator->DrainLivePlayback();
            }

            //
            // In sync event signalling
            //

            if (!mStreamNotifiedInSync && !DifferenceExceedsThreshhold)
            {
                PlayerEventRecord_t         DisplayEvent;
                DisplayEvent.Code           = EventStreamInSync;
                DisplayEvent.Playback       = Playback;
                DisplayEvent.Stream         = Stream;
                PlayerStatus_t StatusEvt = Stream->SignalEvent(&DisplayEvent);
                if (StatusEvt != PlayerNoError)
                {
                    SE_ERROR("Stream 0x%p Failed to signal event\n", Stream);
                }
                mStreamNotifiedInSync        = true;
            }
        }

        break;

    //
    // Here we handle waiting for a long holdoff to allow a full
    // output rate adjustment integration to take place.
    //

    case SyncStateStartLongSyncHoldoff:
        State->SynchronizationState         = SyncStateAwaitingCorrectionWorkthrough;
        State->FrameWorkthroughCount        = 0;
        State->FrameWorkthroughFinishCount      = 1024 + 64;
        break;
    }

    return OutputTimerNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
//  Read the trick mode parameters, and set the trick mode
//  domain boundaries.
//  This function is under mMonitorGroupStructureLock protection from the functions calling it,
//  either RegisterOutputCoordinator() or TrickModeControl().

void   OutputTimer_Base_c::SetTrickModeDomainBoundaries(void)
{
    Rational_t            Tmp0;
    Rational_t            Tmp1;
    Rational_t            MaxNormalRate;
    Rational_t            NormalRateForIOnly;
    Rational_t            MaxSubstandardRate;
    Rational_t            LowerClamp;
    Rational_t            ThresholdToDecodeKey;
    //
    // Calculate useful numbers
    //
    // WARNING: we should not lock the mMonitorGroupStructureLock mutex here, as it is already locked by caller
    OS_AssertMutexHeld(&mMonitorGroupStructureLock);
    mLastCodedFrameRate = mCodedFrameRate;
    mEmpiricalCodedFrameRateLimit = min(mTrickModeParameters.EmpiricalMaximumDecodeFrameRateShortIntegration,
                                        mTrickModeParameters.EmpiricalMaximumDecodeFrameRateLongIntegration);
    // Frame rate used to compute I only decode threshold
    mCodedFrameRateLimitForIOnly = min(mTrickModeParameters.DecodeFrameRateShortIntegrationForIOnly,
                                       mTrickModeParameters.DecodeFrameRateLongIntegrationForIOnly);
// Set 2 fps below empirical measure
    mEmpiricalCodedFrameRateLimit = mEmpiricalCodedFrameRateLimit.TruncateDenominator(1) - 2;
    mCodedFrameRateLimitForIOnly = mCodedFrameRateLimitForIOnly.TruncateDenominator(1) - 2;
    MaxNormalRate = mEmpiricalCodedFrameRateLimit / mCodedFrameRate;
    NormalRateForIOnly = mCodedFrameRateLimitForIOnly / mCodedFrameRate;
    MaxSubstandardRate = mTrickModeParameters.SubstandardDecodeSupported ?
                         (MaxNormalRate * mTrickModeParameters.SubstandardDecodeRateIncrease) : MaxNormalRate;
    int Policy = Player->PolicyValue(Playback, Stream, PolicyAllowFrameDiscardAtNormalSpeed);
    LowerClamp  = (Policy == PolicyValueApply) ? Rational_t(1, 4) : 1;
    Clamp(MaxNormalRate,      LowerClamp, 16);
    Clamp(MaxSubstandardRate, LowerClamp, 16);
    Clamp(NormalRateForIOnly, LowerClamp, 16);
    //
    // Calculate the domain boundaries for trick modes
    //
    mTrickModeDomainTransitToDegradeNonReference = MaxNormalRate;
    Tmp0                    = (MaxSubstandardRate * mTrickModeGroupStructureNonReferenceFrames);
    Tmp1                    = (MaxNormalRate      * mTrickModeGroupStructureReferenceFrames);
    mTrickModeDomainTransitToStartDiscard    = Tmp0 + Tmp1;

    if (mTrickModeGroupStructureReferenceFrames != 0)
    {
        mTrickModeDomainTransitToDecodeReference = MaxNormalRate / mTrickModeGroupStructureReferenceFrames;
        ThresholdToDecodeKey            = NormalRateForIOnly / mTrickModeGroupStructureReferenceFrames;
    }

    // we cannot apply substandard decode to reference frames
    // further changed to avoid going to key frames only when at normal speed, and we cannot keep up
    mTrickModeDomainTransitToDecodeKey       = max(ThresholdToDecodeKey, 1);
    SE_DEBUG(group_output_timer,
             "Stream 0x%p EmpiricalCodedFrameLimit=%d.%06d CodedFrameRateForIOnly=%d.%06d CodedFrameRate=%d.%06d MaxNormalRate=%d.%06d DegradeNonRef=%d.%06d, StartDiscard=%d.%06d, DecodeReference=%d.%06d, DecodeKey=%d.%06d\n",
             Stream,
             mEmpiricalCodedFrameRateLimit.IntegerPart(), mEmpiricalCodedFrameRateLimit.RemainderDecimal(),
             mCodedFrameRateLimitForIOnly.IntegerPart(), mCodedFrameRateLimitForIOnly.RemainderDecimal(),
             mCodedFrameRate.IntegerPart(), mCodedFrameRate.RemainderDecimal(),
             MaxNormalRate.IntegerPart(), MaxNormalRate.RemainderDecimal(),
             mTrickModeDomainTransitToDegradeNonReference.IntegerPart(), mTrickModeDomainTransitToDegradeNonReference.RemainderDecimal(),
             mTrickModeDomainTransitToStartDiscard.IntegerPart(), mTrickModeDomainTransitToStartDiscard.RemainderDecimal(),
             mTrickModeDomainTransitToDecodeReference.IntegerPart(), mTrickModeDomainTransitToDecodeReference.RemainderDecimal(),
             mTrickModeDomainTransitToDecodeKey.IntegerPart(), mTrickModeDomainTransitToDecodeKey.RemainderDecimal());
}


// /////////////////////////////////////////////////////////////////////////
//
//  Read the trick mode parameters, and set the trick mode
//  domain boundaries.
//

OutputTimerStatus_t   OutputTimer_Base_c::TrickModeControl(void)
{
    PlayerStatus_t            Status;
    bool                  TrickModeParametersChanged;
    bool                  CodedFrameRateChanged;
    bool                  GroupStructureChanged;
    Rational_t            NewEmpiricalCodedFrameRateLimit;
    Rational_t            Speed;
    PlayDirection_t           Direction;
    PlayerEventRecord_t       Event;
    //
    // Read speed policy etc
    //
    Player->GetPlaybackSpeed(Playback, &Speed, &Direction);
    int TrickModePolicy = Player->PolicyValue(Playback, Stream, PolicyTrickModeDomain);
    OS_LockMutex(&mMonitorGroupStructureLock);
    NewEmpiricalCodedFrameRateLimit = min(mTrickModeParameters.DecodeFrameRateShortIntegrationForIOnly, mTrickModeParameters.DecodeFrameRateLongIntegrationForIOnly);
    TrickModeParametersChanged      = !inrange(NewEmpiricalCodedFrameRateLimit, mCodedFrameRateLimitForIOnly, (mCodedFrameRateLimitForIOnly + 8));
    CodedFrameRateChanged       = mLastCodedFrameRate != mCodedFrameRate;
    GroupStructureChanged       = (mTrickModeGroupStructureIndependentFrames != mLastTrickModeGroupStructureIndependentFrames) ||
                                  (mTrickModeGroupStructureReferenceFrames != mLastTrickModeGroupStructureReferenceFrames);

    //
    // If anything has changed, then recalculate the values to determine which domain we are in
    //

    if (TrickModeParametersChanged || CodedFrameRateChanged || GroupStructureChanged || mSmoothReverseSupportChanged)
    {
        SetTrickModeDomainBoundaries();
    }

    if ((Speed           != mLastTrickModeSpeed)  ||
        (TrickModePolicy != mLastTrickModePolicy) ||
        TrickModeParametersChanged          ||
        GroupStructureChanged               ||
        mSmoothReverseSupportChanged)
    {
        SE_DEBUG(group_output_timer, "Stream 0x%p Domain = %d, speed = %4d.%06d (was %d.%06d) \n",
                 Stream, mTrickModeDomain,
                 Speed.IntegerPart(), Speed.RemainderDecimal(),
                 mLastTrickModeSpeed.IntegerPart(), mLastTrickModeSpeed.RemainderDecimal()
                );
        //
        // Calculate the domain
        //
        if (TrickModePolicy == PolicyValueTrickModeAuto)
        {
            if ((!mSmoothReverseSupport) && (Direction == PlayBackward))
            {
                mTrickModeDomain = TrickModeDecodeKeyFrames;
            }
            else if (Speed > mTrickModeDomainTransitToDecodeKey)
            {
                mTrickModeDomain   = TrickModeDecodeKeyFrames;
            }
            else if (Speed > mTrickModeDomainTransitToDecodeReference)
            {
                mTrickModeDomain   = TrickModeDecodeReferenceFramesDegradeNonKeyFrames;
            }
            else if (Speed > mTrickModeDomainTransitToStartDiscard)
            {
                mTrickModeDomain   = TrickModeStartDiscardingNonReferenceFrames;
            }
            else if (Speed > mTrickModeDomainTransitToDegradeNonReference)
            {
                mTrickModeDomain   = TrickModeDecodeAllDegradeNonReferenceFrames;
            }
            else
            {
                mTrickModeDomain   = TrickModeDecodeAll;
            }
        }
        else
        {
            mTrickModeDomain = (TrickModeDomain_t)TrickModePolicy;
        }

        //
        // Need to recalculate the parameters for discard.
        //
        mPortionOfPreDecodeFramesToLose      = 0; // rational

        switch (mTrickModeDomain)
        {
        case TrickModeInvalid:
        case TrickModeDecodeAll:
        case PolicyValueTrickModeDecodeAllDegradeNonReferenceFrames:
            mAdjustedSpeedAfterFrameDrop = Speed;
            break;

        case TrickModeStartDiscardingNonReferenceFrames:
            mPortionOfPreDecodeFramesToLose  = (1 - (mTrickModeDomainTransitToDegradeNonReference / Speed)) /
                                               mTrickModeGroupStructureNonReferenceFrames;

            if (mPortionOfPreDecodeFramesToLose > 1)
            {
                mPortionOfPreDecodeFramesToLose    = 1;
            }

            mAdjustedSpeedAfterFrameDrop = Speed * (1 - (mPortionOfPreDecodeFramesToLose * mTrickModeGroupStructureNonReferenceFrames));
            break;

        case TrickModeDecodeReferenceFramesDegradeNonKeyFrames:
            mAdjustedSpeedAfterFrameDrop = Speed * mTrickModeGroupStructureReferenceFrames;
            break;

        case TrickModeDecodeKeyFrames:
            mAdjustedSpeedAfterFrameDrop = Speed * mTrickModeGroupStructureIndependentFrames;
            break;

        default:    mAdjustedSpeedAfterFrameDrop = Speed;
            break;
        }

        //
        // Do we need to signal a change in the trick mode domain
        //

        if ((mTrickModeDomain != mLastTrickModeDomain) &&
            ((EventTrickModeDomainChange & EventMask) != 0))
        {
            Event.Code          = EventTrickModeDomainChange;
            Event.Playback      = Playback;
            Event.Stream        = Stream;
            Event.PlaybackTime      = TIME_NOT_APPLICABLE;
            Event.UserData      = EventUserData;
            Event.Value[0].UnsignedInt  = mTrickModeDomain;
            Event.Value[1].UnsignedInt  = mLastTrickModeDomain;
            Status          = Stream->SignalEvent(&Event);

            if (Status != PlayerNoError)
            {
                SE_ERROR("Stream 0x%p Failed to signal event\n", Stream);
            }
        }

        //
        // If we are switching from domain >= TrickModeDecodeKeyFrames, IE we are losing reference frames
        // to one < TrickModeDecodeKeyFrames, IE where those reference frames may be needed. We set
        // the flag to force checking reference frames until we see a specified number of key frames.
        //

        if ((mLastTrickModeDomain >= TrickModeDecodeKeyFrames) &&
            (mTrickModeDomain     <  TrickModeDecodeKeyFrames))
        {
            mTrickModeCheckReferenceFrames = TRICK_MODE_CHECK_REFERENCE_FRAMES_FOR_N_KEY_FRAMES;
        }

        if (mTrickModeDomain != mLastTrickModeDomain)
        {
            mAccumulatedPreDecodeFramesToLose  = 0;    // rational
        }

        if ((mTrickModeDomain != mLastTrickModeDomain) ||
            (mPortionOfPreDecodeFramesToLose != mLastPortionOfPreDecodeFramesToLose))
        {
            SE_DEBUG(group_output_timer, "Stream 0x%p Domain = %d, PortionToLose = %4d.%06d, Adjusted speed = %4d.%06d, max decode rate = %4d.%06d\n",
                     Stream, mTrickModeDomain,
                     mPortionOfPreDecodeFramesToLose.IntegerPart(), mPortionOfPreDecodeFramesToLose.RemainderDecimal(),
                     mAdjustedSpeedAfterFrameDrop.IntegerPart(), mAdjustedSpeedAfterFrameDrop.RemainderDecimal(),
                     mEmpiricalCodedFrameRateLimit.IntegerPart(), mEmpiricalCodedFrameRateLimit.RemainderDecimal());
        }

        mLastTrickModePolicy             = TrickModePolicy;
        mLastTrickModeDomain             = mTrickModeDomain;
        mLastPortionOfPreDecodeFramesToLose      = mPortionOfPreDecodeFramesToLose;
        mLastTrickModeSpeed              = Speed;
        mLastTrickModeGroupStructureIndependentFrames    = mTrickModeGroupStructureIndependentFrames;
        mLastTrickModeGroupStructureReferenceFrames  = mTrickModeGroupStructureReferenceFrames;
        mSmoothReverseSupportChanged = false;
    }

    OS_UnLockMutex(&mMonitorGroupStructureLock);

    return OutputTimerNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
//  Function to monitor/init group structure data.
//  The function is called under mMonitorGroupStructureLock protection from RegisterOutputCoordinator()
//

void   OutputTimer_Base_c::InitializeGroupStructure(void)
{
    unsigned int              i;

    // WARNING: we should not lock the mMonitorGroupStructureLock mutex here, as it is already locked by caller
    OS_AssertMutexHeld(&mMonitorGroupStructureLock);
    mNextGroupStructureEntry = 0;

    for (i = 0; i < GROUP_STRUCTURE_INTEGRATION_PERIOD; i++)
    {
        mGroupStructureSizes[i]      = mTrickModeParameters.DefaultGroupSize;
        mGroupStructureReferences[i] = max(mTrickModeParameters.DefaultGroupReferenceFrameCount, 1);
    }

    mGroupStructureSizeCount     = 0;
    mGroupStructureReferenceCount    = 0;

    if (0 != mTrickModeParameters.DefaultGroupSize)
    {
        mTrickModeGroupStructureIndependentFrames    = Rational_t(1, mTrickModeParameters.DefaultGroupSize);
        mTrickModeGroupStructureReferenceFrames  = Rational_t(max(mTrickModeParameters.DefaultGroupReferenceFrameCount, 1), mTrickModeParameters.DefaultGroupSize);
        mTrickModeGroupStructureNonReferenceFrames   = 1 - mTrickModeGroupStructureReferenceFrames;
    }
}

void   OutputTimer_Base_c::MonitorGroupStructure(ParsedFrameParameters_t     *ParsedFrameParameters)
{
    unsigned int    i;
    unsigned int    AccumulatedGroupStructureSizes;
    unsigned int    AccumulatedGroupStructureReferences;

    // Take a lock here because this function could be called from output timer itself from parse to decode thread
    // or from parser in case of backward playback and smooth reverse is not supported, which belongs to
    // collate to parse thread.
    OS_LockMutex(&mMonitorGroupStructureLock);

    if (ParsedFrameParameters->IndependentFrame && (mGroupStructureSizeCount != 0))
    {
        mGroupStructureSizes[mNextGroupStructureEntry]        = mGroupStructureSizeCount;
        mGroupStructureReferences[mNextGroupStructureEntry]   = max(mGroupStructureReferenceCount, 1);
        mNextGroupStructureEntry                 = (mNextGroupStructureEntry + 1) % GROUP_STRUCTURE_INTEGRATION_PERIOD;

        AccumulatedGroupStructureSizes              = 0;
        AccumulatedGroupStructureReferences         = 0;

        for (i = 0; i < GROUP_STRUCTURE_INTEGRATION_PERIOD; i++)
        {
            AccumulatedGroupStructureSizes          += mGroupStructureSizes[i];
            AccumulatedGroupStructureReferences         += mGroupStructureReferences[i];
        }

        SE_ASSERT(0 != AccumulatedGroupStructureSizes);
        mTrickModeGroupStructureIndependentFrames        = Rational_t(GROUP_STRUCTURE_INTEGRATION_PERIOD, AccumulatedGroupStructureSizes);
        mTrickModeGroupStructureReferenceFrames          = Rational_t(AccumulatedGroupStructureReferences, AccumulatedGroupStructureSizes);
        mTrickModeGroupStructureNonReferenceFrames       = 1 - mTrickModeGroupStructureReferenceFrames;

        SE_VERBOSE(group_output_timer, "Stream 0x%p Group Structure %d %d - %d.%06d - %d.%06d - %d.%06d\n",
                   Stream, mGroupStructureReferenceCount, mGroupStructureSizeCount,
                   IntegerPart(mTrickModeGroupStructureIndependentFrames), RemainderDecimal(mTrickModeGroupStructureIndependentFrames),
                   IntegerPart(mTrickModeGroupStructureReferenceFrames), RemainderDecimal(mTrickModeGroupStructureReferenceFrames),
                   IntegerPart(mTrickModeGroupStructureNonReferenceFrames), RemainderDecimal(mTrickModeGroupStructureNonReferenceFrames));
        mGroupStructureSizeCount                 = 0;
        mGroupStructureReferenceCount                = 0;
    }

    mGroupStructureSizeCount++;

    if (ParsedFrameParameters->ReferenceFrame)
    {
        mGroupStructureReferenceCount++;
    }

    OS_UnLockMutex(&mMonitorGroupStructureLock);
}


// /////////////////////////////////////////////////////////////////////////
//
//  Function to monitor/init group structure data.
//

void   OutputTimer_Base_c::DecodeInTimeFailure(unsigned long long   FailedBy,
                                               bool            CollatedAfterDisplayTime)
{
    PlayerStatus_t            Status;
    unsigned int              CodedFrameBufferCount;
    unsigned int              CodedFrameBuffersInUse;
    unsigned int              CodedFrameBufferMemoryInPool;
    unsigned int              CodedFrameBufferMemoryAllocated;
    unsigned int              CodedFrameBufferLargestFreeMemoryBlock;
    unsigned int              DecodeBufferCount;
    unsigned int              DecodeBuffersInUse;
    bool                  LivePlay;
    bool                  PacketInjector;
    bool                  Rebase;
    PlayerEventIdentifier_t       EventCode;
    PlayerEventRecord_t       Event;
    //
    // Check if the underlying time mapping has
    // been modified to effect a synchronization change.
    //
    Status  = mOutputCoordinator->MappingBaseAdjustmentApplied(mOutputCoordinatorContext);

    if (Status != OutputCoordinatorNoError)
    {
        mGlobalSynchronizationState  = SyncStateStartAwaitingCorrectionWorkthrough;
        mDecodeInTimeState       = DecodingInTime;
        //
        // Note we do not reset mSeenAnInTimeFrame, because if we are really having problems, we won't get to see one
        //
        return;
    }

    //
    mCodedFrameBufferPool->GetPoolUsage(&CodedFrameBufferCount,
                                        &CodedFrameBuffersInUse,
                                        &CodedFrameBufferMemoryInPool,
                                        &CodedFrameBufferMemoryAllocated,
                                        NULL,
                                        &CodedFrameBufferLargestFreeMemoryBlock);
    mDecodeBufferPool->GetPoolUsage(NULL,
                                    &DecodeBuffersInUse,
                                    NULL, NULL, NULL);

    Stream->GetDecodeBufferManager()->GetEstimatedBufferCount(&DecodeBufferCount, ForManifestation | ForReference);
    LivePlay       = (Player->PolicyValue(Playback, PlayerAllStreams, PolicyLivePlayback) == PolicyValueApply);
    PacketInjector = (Player->PolicyValue(Playback, PlayerAllStreams, PolicyPacketInjectorPlayback) == PolicyValueApply);

    //
    // Switch on the state
    //

    switch (mDecodeInTimeState)
    {
    case DecodingInTime:
        mDecodeInTimeFailures        = 1;
        mDecodeInTimeCodedDataLevel  = CodedFrameBuffersInUse;
        mDecodeInTimeBehind      = FailedBy;
        mDecodeInTimeState       = AccumulatingDecodeInTimeFailures;
        break;

    // no fallthrough (tbc)
    case AccumulatingDecodeInTimeFailures:
        mDecodeInTimeFailures++;

        if (mDecodeInTimeFailures < DECODE_IN_TIME_INTEGRATION_PERIOD)
        {
            break;
        }

        mDecodeInTimeState       = AccumulatedDecodeInTimeFailures;

    // fallthrough
    case AccumulatedDecodeInTimeFailures:
        Rebase              = false;
        EventCode           = EventIllegalIdentifier;

        if (DecodeBuffersInUse >= (DecodeBufferCount - 1))
        {
            //
            // Failure to decode in time, but all the decode buffers are full,
            // this has been seen in previous code bugs where we have a failure of
            // the output timer, or the manifestor. TODO check if implementation error or not
            //
            SE_ERROR("Stream 0x%p - %s - Failed to decode in time but decode buffers full (%2d %2d/%2d %2d/%2d) (%6lld %6lld)\n",
                     Stream, mConfiguration.OutputTimerName,
                     mDecodeInTimeCodedDataLevel, CodedFrameBuffersInUse, CodedFrameBufferCount,
                     DecodeBuffersInUse, DecodeBufferCount,
                     mDecodeInTimeBehind, FailedBy);
            Rebase  = true;     // Try to mitigate the effect of this state.
        }
        else if ((CodedFrameBuffersInUse         < (CodedFrameBufferCount / 3)) &&
                 (CodedFrameBufferLargestFreeMemoryBlock    > mCodedFrameBufferMaximumSize) &&
                 (CodedFrameBufferMemoryAllocated       < (CodedFrameBufferMemoryInPool / 2)))
        {
            //
            // Failure to deliver data on time
            //
            SE_ERROR("Stream 0x%p - %s - Failed to deliver data to decoder in time (%2d %2d/%2d %2d/%2d) (%6lld %6lld)\n",
                     Stream, mConfiguration.OutputTimerName,
                     mDecodeInTimeCodedDataLevel, CodedFrameBuffersInUse, CodedFrameBufferCount,
                     DecodeBuffersInUse, DecodeBufferCount,
                     mDecodeInTimeBehind, FailedBy);
            Playback->CheckForCodedBufferMismatch(Stream);
            int Policy = Player->PolicyValue(Playback, Stream, PolicyRebaseOnFailureToDeliverDataInTime);
            Rebase  = (Policy == PolicyValueApply);
            EventCode   = EventFailedToDeliverDataInTime;
        }
        else if (FailedBy > mDecodeInTimeBehind)
        {
            //
            // Failure to decode in time
            //
            SE_ERROR("Stream 0x%p - %s - Failed to decode frames in time (%2d %2d/%2d %2d/%2d) (%6lld %6lld)\n",
                     Stream, mConfiguration.OutputTimerName,
                     mDecodeInTimeCodedDataLevel, CodedFrameBuffersInUse, CodedFrameBufferCount,
                     DecodeBuffersInUse, DecodeBufferCount,
                     mDecodeInTimeBehind, FailedBy);
            int Policy  = Player->PolicyValue(Playback, Stream, PolicyRebaseOnFailureToDecodeInTime);
            Rebase      = (Policy == PolicyValueApply);
            Policy      = Player->PolicyValue(Playback, Stream, PolicyAllowFrameDiscardAtNormalSpeed);

            if (Rebase && (Policy == PolicyValueApply))
                SE_ERROR("Stream 0x%p - %s - Both PolicyRebaseOnFailureToDecodeInTime & PolicyAllowFrameDiscardAtNormalSpeed are enabled at the same time\n"
                         "  =>the use of one of these should preclude the use of the other\n", Stream, mConfiguration.OutputTimerName);

            EventCode           = EventFailedToDecodeInTime;
        }
        else
        {
            //
            // Give the system the benefit of the doubt
            // It may be getting better.
            //
            mDecodeInTimeState       = DecodingInTime;
            break;
        }

        //
        // Did we want to rebase, treat differently for live play and non-live play,
        // Do not rebase on live play, but if we are receiving data after it should
        // be displayed consistently, then just reset the mapping.
        //

        /* AdjustMappingBase the base if injection using PacketInjector.
            If getting late frames continuously.
            Or collected after its PTS value.
            It is possible that if errors are injected from the packet injector then buffer level goes down as a result under flow at the players.
            Note that packet injector "do pause" before injecting error.
            So it will give some margin to recover from the buffer under flow */
        int RebaseMargin;
        if (Player->PolicyValue(Playback, Stream, PolicyHdmiRxMode) == PolicyValueHdmiRxModeRepeater)
        {
            // For repeater mode Latency should be as minimum as possibe. So don't apply any margin.
            RebaseMargin = 0;
        }
        else
        {
            RebaseMargin = REBASE_MARGIN;
        }

        if (PacketInjector && (CollatedAfterDisplayTime || Rebase))
        {
            mOutputCoordinator->AdjustMappingBase(PlaybackContext, FailedBy + RebaseMargin, true);
        }
        else if (LivePlay && CollatedAfterDisplayTime)
        {
            //
            // Commented out, because we can get a frame that starts collating with a PTS,
            // then has the aerial pulled out so the collation time (recorded at end of
            // frame collation), is late for the display
            //
            // ResetTimeMapping();
        }
        else if (!LivePlay && Rebase)
        {
            mOutputCoordinator->AdjustMappingBase(PlaybackContext, FailedBy + RebaseMargin, true);
        }

        //
        // Did we want to raise an event
        //

        if ((EventCode & EventMask) != 0)
        {
            Event.Code      = EventCode;
            Event.Playback  = Playback;
            Event.Stream    = Stream;
            Event.PlaybackTime  = TIME_NOT_APPLICABLE;
            Event.UserData  = EventUserData;
            Status      = Stream->SignalEvent(&Event);

            if (Status != PlayerNoError)
            {
                SE_ERROR("Stream 0x%p - %s - Failed to signal event\n", Stream, mConfiguration.OutputTimerName);
            }
        }
        Player->SetPolicy(Playback, Stream, PolicyTrickModeDomain, mTrickModeDomain);

        //
        mDecodeInTimeState   = PauseBetweenDecodeInTimeIntegrations;
        mDecodeInTimeFailures    = 0;

    // fallthrough
    case PauseBetweenDecodeInTimeIntegrations:
        mDecodeInTimeFailures++;

        if (mDecodeInTimeFailures >= DECODE_IN_TIME_PAUSE_BETWEEN_INTEGRATION_PERIOD)
        {
            mDecodeInTimeState = DecodingInTime;
        }

        break;
    }
}

// /////////////////////////////////////////////////////////////////////////
//
//  Time functions
//
//  NOTE these are playback times, which are independant of the direction of play
//  so play direction does not affect them in any way.
//

bool   OutputTimer_Base_c::PlaybackTimeCheckGreaterThanOrEqual(
    unsigned long long  T0,
    unsigned long long  T1)
{
    if (NotValidTime(T0) || NotValidTime(T1))
    {
        return false;
    }

    return (T0 >= T1);
}

bool   OutputTimer_Base_c::PlaybackTimeCheckIntervalsIntersect(
    unsigned long long  I0Start,
    unsigned long long  I0End,
    unsigned long long  I1Start,
    unsigned long long  I1End,
    OutputTimerTestPoint_t    TestPoint)
{
    if (NotValidTime(I0Start))
    {
        I0Start   = 0;
    }

    if (NotValidTime(I0End))
    {
        I0End = 0xffffffffffffffffULL;
    }

    if (NotValidTime(I1Start))
    {
        I1Start   = 0;
    }

    if (NotValidTime(I1End))
    {
        I1End = 0xffffffffffffffffULL;
    }

    if (TestPoint == OutputTimerBeforeDecodeWindow)
    {
        //
        // if I0Start < I1Start
        //      intersect if I0End >= I1Start
        // else
        //      intersect if I0Start <= I1End
        //
        return (I0Start < I1Start) ? (I0End > I1Start) : (I0Start < I1End);
    }
    else
    {
        //
        // if I0Start < I1Start
        //      no intersection. Accepting I0End > I1Start leads to display pictures from the past in case of DropTestBeforeOutputTiming
        // else
        //      intersect if I0Start <= I1End
        //
        return ((I0Start >= I1Start) && (I0Start < I1End));
    }
}

const char *OutputTimer_Base_c::LookupStreamType(PlayerStreamType_t StreamType)
{
#define E(x) case StreamType##x: return #x

    switch (StreamType)
    {
        E(None);
        E(Audio);
        E(Video);
        E(Other);

    default: return "INVALID";
    }

#undef E
}

// /////////////////////////////////////////////////////////////////////////
//
//      Increment statistic counters for frame dropping on different
//      output timer edges
//

void OutputTimer_Base_c::IncrementTestFrameDropStatistics(OutputTimerStatus_t  Status, OutputTimerTestPoint_t  TestPoint)
{
    switch (TestPoint)
    {
    case OutputTimerBeforeDecodeWindow:
        switch (Status)
        {
        case OutputTimerDropFrameSingleGroupPlayback :
            Stream->Statistics().DroppedBeforeDecodeWindowSingleGroupPlayback++;
            break;

        case OutputTimerDropFrameKeyFramesOnly :
            Stream->Statistics().DroppedBeforeDecodeWindowKeyFramesOnly++;
            break;

        case OutputTimerDropFrameOutsidePresentationInterval:
            Stream->Statistics().DroppedBeforeDecodeWindowOutsidePresentationInterval++;
            break;

        case OutputTimerTrickModeNotSupportedDropFrame:
            Stream->Statistics().DroppedBeforeDecodeWindowTrickModeNotSupported++;
            break;

        case OutputTimerTrickModeDropFrame:
            Stream->Statistics().DroppedBeforeDecodeWindowTrickMode++;
            break;

        default:
            Stream->Statistics().DroppedBeforeDecodeWindowOthers++;
            break;
        }

        Stream->Statistics().DroppedBeforeDecodeWindowTotal++;
        break;

    case OutputTimerBeforeDecode:
        switch (Status)
        {
        case OutputTimerDropFrameSingleGroupPlayback :
            Stream->Statistics().DroppedBeforeDecodeSingleGroupPlayback++;
            break;

        case OutputTimerDropFrameKeyFramesOnly :
            Stream->Statistics().DroppedBeforeDecodeKeyFramesOnly++;
            break;

        case OutputTimerDropFrameOutsidePresentationInterval:
            Stream->Statistics().DroppedBeforeDecodeOutsidePresentationInterval++;
            break;

        case OutputTimerTrickModeNotSupportedDropFrame:
            Stream->Statistics().DroppedBeforeDecodeTrickModeNotSupported++;
            break;

        case OutputTimerTrickModeDropFrame:
            Stream->Statistics().DroppedBeforeDecodeTrickMode++;
            break;

        default:
            Stream->Statistics().DroppedBeforeDecodeOthers++;
            break;
        }

        Stream->Statistics().DroppedBeforeDecodeTotal++;
        break;

    case OutputTimerBeforeOutputTiming:
        switch (Status)
        {
        case OutputTimerDropFrameOutsidePresentationInterval:
            Stream->Statistics().DroppedBeforeOutputTimingOutsidePresentationInterval++;
            break;

        default:
            Stream->Statistics().DroppedBeforeOutputTimingOthers++;
            break;
        }

        Stream->Statistics().DroppedBeforeOutputTimingTotal++;
        break;

    case OutputTimerBeforeManifestation:
        switch (Status)
        {
        case OutputTimerUntimedFrame:
            Stream->Statistics().DroppedBeforeManifestationUntimed++;
            break;

        case OutputTimerDropFrameTooLateForManifestation:
            Stream->Statistics().DroppedBeforeManifestationTooLateForManifestation++;
            break;

        case OutputTimerTrickModeNotSupportedDropFrame:
            Stream->Statistics().DroppedBeforeManifestationTrickModeNotSupported++;
            break;

        default:
            Stream->Statistics().DroppedBeforeManifestationOthers++;
            break;
        }

        Stream->Statistics().DroppedBeforeManifestationTotal++;
        break;
    }
}

// /////////////////////////////////////////////////////////////////////////
//
//      Function to know about the parser ability to support smooth reverse
//
//      This function is updating variables for output timer
//      for it to adapt its dropping policy to the backward decode mode.
//
OutputTimerStatus_t  OutputTimer_Base_c::SetSmoothReverseSupport(bool SmoothReverseSupportFromParser)
{
    if (SmoothReverseSupportFromParser != mSmoothReverseSupport)
    {
        mSmoothReverseSupport = SmoothReverseSupportFromParser;
        mSmoothReverseSupportChanged = true;
    }

    return OutputTimerNoError;
}
