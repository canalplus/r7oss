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

#ifndef H_OUTPUT_TIMER_BASE
#define H_OUTPUT_TIMER_BASE

#include "player.h"
#include "player_stream.h"

#undef TRACE_TAG
#define TRACE_TAG "OutputTimer_Base_c"

#define GROUP_STRUCTURE_INTEGRATION_PERIOD  8

//
// Enumeration of the possible states of the synchronizer
//

typedef enum
{
    SyncStateNullState              = 0,
    SyncStateInSynchronization      = 1,
    SyncStateSuspectedSyncError,
    SyncStateConfirmedSyncError,
    SyncStateStartAwaitingCorrectionWorkthrough,
    SyncStateAwaitingCorrectionWorkthrough,
    SyncStateStartLongSyncHoldoff
} SynchronizationState_t;

//
// Enumeration of the decoding in time states
//

typedef enum
{
    DecodingInTime              = 1,
    AccumulatingDecodeInTimeFailures,
    AccumulatedDecodeInTimeFailures,
    PauseBetweenDecodeInTimeIntegrations
} DecodeInTimeState_t;

//
// Enumeration of trick mode domains
//

typedef enum
{
    TrickModeInvalid                    = 0,
    TrickModeDecodeAll                  = PolicyValueTrickModeDecodeAll,
    TrickModeDecodeAllDegradeNonReferenceFrames     = PolicyValueTrickModeDecodeAllDegradeNonReferenceFrames,
    TrickModeStartDiscardingNonReferenceFrames      = PolicyValueTrickModeStartDiscardingNonReferenceFrames,
    TrickModeDecodeReferenceFramesDegradeNonKeyFrames   = PolicyValueTrickModeDecodeReferenceFramesDegradeNonKeyFrames,
    TrickModeDecodeKeyFrames                = PolicyValueTrickModeDecodeKeyFrames,
    TrickModeDiscontinuousKeyFrames         = PolicyValueTrickModeDiscontinuousKeyFrames
} TrickModeDomain_t;

//
// mConfiguration structure for the base class
//

typedef struct OutputTimerConfiguration_s
{
    const char           *OutputTimerName;

    PlayerStreamType_t    StreamType;

    BufferType_t          AudioVideoDataParsedParametersType;
    unsigned int          SizeOfAudioVideoDataParsedParameters;

    //
    // Values controlling the decode window for a frame,
    // the first of which can be refined over a period as
    // frames are timed.
    //

    unsigned long long    FrameDecodeTime;
    unsigned long long    EarlyDecodePorch;
    unsigned long long    MinimumManifestorLatency;

    unsigned int          MaximumDecodeTimesToWait; // This will be adjusted by the speed factor when used.

    unsigned int          ReverseEarlyDecodePorch;  // reverse is a whole new kettle of fish for decode porches,
    // This is generally hardcoded to less than the maximum manifestation wait.

    //
    // Values controlling the avsync mechanisms
    //

    unsigned long long    SynchronizationErrorThreshold;
    unsigned long long    SynchronizationErrorThresholdForQuantizedSync;
    unsigned int          SynchronizationIntegrationCount;

    //
    // trick mode controls
    //

    bool                  ReversePlaySupported;
    Rational_t            MinimumSpeedSupported;
    Rational_t            MaximumSpeedSupported;

    OutputTimerConfiguration_s(void)
        : OutputTimerName(NULL)
        , StreamType(StreamTypeNone)
        , AudioVideoDataParsedParametersType(0)
        , SizeOfAudioVideoDataParsedParameters(0)
        , FrameDecodeTime(0)
        , EarlyDecodePorch(0)
        , MinimumManifestorLatency(0)
        , MaximumDecodeTimesToWait(0)
        , ReverseEarlyDecodePorch(0)
        , SynchronizationErrorThreshold(0)
        , SynchronizationErrorThresholdForQuantizedSync(0)
        , SynchronizationIntegrationCount(0)
        , ReversePlaySupported(false)
        , MinimumSpeedSupported()
        , MaximumSpeedSupported()
    {}
} OutputTimerConfiguration_t;

//
// Parameter block settings
//

typedef enum
{
    OutputTimerSetNormalizedTimeOffset      = BASE_OUTPUT_TIMER
} OutputTimerParameterBlockType_t;


typedef struct OutputTimerSetNormalizedTimeOffset_s
{
    long long         Value;
} OutputTimerSetNormalizedTimeOffset_t;


typedef struct OutputTimerParameterBlock_s
{
    OutputTimerParameterBlockType_t     ParameterType;

    union
    {
        OutputTimerSetNormalizedTimeOffset_t    Offset; ///< Offset to apply to the stream (in microseconds)
    };
} OutputTimerParameterBlock_t;


//
// Individual timing state - for a specific manifestation
//

typedef struct ManifestationTimingState_s
{
    bool                    Initialized;

    SynchronizationState_t  SynchronizationState;             // Values used in synchronization
    bool                    SynchronizationAtStartup;
    unsigned int            SynchronizationFrameCount;
    unsigned int            SynchronizationErrorIntegrationCount;
    long long               SynchronizationAccumulatedError;
    long long               SynchronizationError;
    long long               SynchronizationPreviousError;
    unsigned int            SynchronizationErrorStabilityCounter;
    unsigned int            FrameWorkthroughCount;
    unsigned int            FrameWorkthroughFinishCount;

    Rational_t              AccumulatedError;             // Generic

    unsigned int            ExtendSamplesForSynchronization;      // Audio specific
    unsigned int            SynchronizationCorrectionUnits;
    unsigned int            SynchronizationOneTenthCorrectionUnits;

    unsigned int            LoseFieldsForSynchronization;

    Rational_t              PreviousDisplayFrameRate;
    Rational_t              CountMultiplier;
    unsigned long long      FieldDurationTime;

    ManifestationTimingState_s(void)
        : Initialized(false)
        , SynchronizationState()
        , SynchronizationAtStartup(false)
        , SynchronizationFrameCount(0)
        , SynchronizationErrorIntegrationCount(0)
        , SynchronizationAccumulatedError(0)
        , SynchronizationError(0)
        , SynchronizationPreviousError(0)
        , SynchronizationErrorStabilityCounter(false)
        , FrameWorkthroughCount(0)
        , FrameWorkthroughFinishCount(0)
        , AccumulatedError(0)
        , ExtendSamplesForSynchronization(0)
        , SynchronizationCorrectionUnits(0)
        , SynchronizationOneTenthCorrectionUnits(0)
        , LoseFieldsForSynchronization(0)
        , PreviousDisplayFrameRate()
        , CountMultiplier()
        , FieldDurationTime(0)
    {}
} ManifestationTimingState_t;

class OutputTimer_Base_c : public OutputTimer_c
{
public:
    OutputTimer_Base_c(void);
    ~OutputTimer_Base_c(void);

    //
    // Overrides for component base class functions
    //

    OutputTimerStatus_t   Halt(void);
    OutputTimerStatus_t   Reset(void);

    OutputTimerStatus_t   SetModuleParameters(unsigned int    ParameterBlockSize,
                                              void         *ParameterBlock);

    //
    // OutputTimer class functions
    //

    OutputTimerStatus_t   RegisterOutputCoordinator(OutputCoordinator_t   mOutputCoordinator);

    OutputTimerStatus_t   ResetTimeMapping(void);

    OutputTimerStatus_t   ResetOnStreamSwitch(void) { return OutputTimerNoError; }

    OutputTimerStatus_t   ResetTimingContext(unsigned int         ManifestationIndex = INVALID_INDEX);

    OutputTimerStatus_t   AwaitEntryIntoDecodeWindow(Buffer_t         Buffer);

    OutputTimerStatus_t   TestForFrameDrop(Buffer_t       Buffer,
                                           OutputTimerTestPoint_t    TestPoint);

    OutputTimerStatus_t   GenerateFrameTiming(Buffer_t        Buffer);
    OutputTimerStatus_t   RecordActualFrameTiming(Buffer_t        Buffer);

    OutputTimerStatus_t   GetStreamStartDelay(unsigned long long     *Delay);

    static const char           *StringifyOutputTimerStatus(OutputTimerStatus_t Status);

    static const char    *LookupStreamType(PlayerStreamType_t StreamType);
    void          MonitorGroupStructure(ParsedFrameParameters_t  *ParsedFrameParameters);
    OutputTimerStatus_t   SetSmoothReverseSupport(bool SmoothReverseSupportFromParser);
    OutputTimerStatus_t   TrickModeControl(void);

protected:
    OS_Mutex_t                    mLock;
    OutputTimerConfiguration_t    mConfiguration;

    OutputSurfaceDescriptor_t   **mOutputSurfaceDescriptors;
    unsigned int                  mOutputSurfaceDescriptorsHighestIndex;

    Rational_t                    mCodedFrameRate;
    Rational_t                    mAdjustedSpeedAfterFrameDrop;
    Rational_t                    mLastCountMultiplier;

    unsigned long long            mNextExpectedPlaybackTime;         // Value used in check for PTS jumps
    unsigned long long            mLastFrameCollationTime;

    ManifestationTimingState_t    mManifestationTimingState[MAXIMUM_MANIFESTATION_TIMING_COUNT];
    Rational_t                    mSystemClockAdjustment;
    Rational_t                   *mOutputRateAdjustments;

    TrickModeDomain_t             mTrickModeDomain;

    bool   PlaybackTimeCheckGreaterThanOrEqual(unsigned long long   T0,
                                               unsigned long long  T1);

    bool   PlaybackTimeCheckIntervalsIntersect(unsigned long long   I0Start,
                                               unsigned long long  I0End,
                                               unsigned long long  I1Start,
                                               unsigned long long  I1End,
                                               OutputTimerTestPoint_t    TestPoint);

    void              SetTrickModeDomainBoundaries(void);
    void              InitializeGroupStructure(void);
    void              DecodeInTimeFailure(unsigned long long      FailedBy,
                                          bool              CollatedAfterDisplayTime);

    ParsedFrameParameters_t *GetParsedFrameParameters(Buffer_t Buffer)
    {
        ParsedFrameParameters_t *ParsedFrameParameters;
        Buffer->ObtainMetaDataReference(Player->MetaDataParsedFrameParametersType, (void **) &ParsedFrameParameters);
        SE_ASSERT(ParsedFrameParameters != NULL);

        return ParsedFrameParameters;
    }

    ParsedFrameParameters_t *GetParsedFrameParametersReference(Buffer_t Buffer)
    {
        ParsedFrameParameters_t *ParsedFrameParametersReference;
        Buffer->ObtainMetaDataReference(Player->MetaDataParsedFrameParametersReferenceType, (void **) &ParsedFrameParametersReference);
        SE_ASSERT(ParsedFrameParametersReference != NULL);

        return ParsedFrameParametersReference;
    }

    void *GetParsedAudioVideoDataParameters(Buffer_t Buffer)
    {
        void *ParsedAudioVideoDataParameters = NULL;

        if (Stream->GetStreamType() == StreamTypeVideo)
        {
            Buffer->ObtainMetaDataReference(Player->MetaDataParsedVideoParametersType, (void **) &ParsedAudioVideoDataParameters);
        }
        else if (Stream->GetStreamType() == StreamTypeAudio)
        {
            Buffer->ObtainMetaDataReference(Player->MetaDataParsedAudioParametersType, (void **) &ParsedAudioVideoDataParameters);
        }
        SE_ASSERT(ParsedAudioVideoDataParameters != NULL);

        return ParsedAudioVideoDataParameters;
    }

    OutputTiming_t *GetOutputTiming(Buffer_t Buffer)
    {
        OutputTiming_t *OutputTiming;
        Buffer->ObtainMetaDataReference(Player->MetaDataOutputTimingType, (void **) &OutputTiming);
        SE_ASSERT(OutputTiming != NULL);

        return OutputTiming;
    }

    OutputTimerStatus_t       DropTestBeforeDecodeWindow(Buffer_t       Buffer);
    OutputTimerStatus_t       DropTestBeforeDecode(Buffer_t     Buffer);
    OutputTimerStatus_t       DropTestBeforeOutputTiming(Buffer_t       Buffer);
    OutputTimerStatus_t       DropTestBeforeManifestation(Buffer_t      Buffer);

    void IncrementTestFrameDropStatistics(OutputTimerStatus_t  Status,
                                          OutputTimerTestPoint_t  TestPoint);

    //
    // Function to perform AVD synchronization, could be overridden, but not recomended
    //

    virtual OutputTimerStatus_t   PerformAVDSync(Buffer_t             Buffer,
                                                 unsigned int         TimingIndex,
                                                 unsigned long long   ExpectedFrameOutputTime,
                                                 unsigned long long   ActualFrameOutputTime,
                                                 OutputTiming_t      *OutputTiming);


    //
    // Functions to be provided by the stream specific implementations
    //

    virtual OutputTimerStatus_t   InitializeConfiguration(void) = 0;

    virtual OutputTimerStatus_t   FrameDuration(void                *ParsedAudioVideoDataParameters,
                                                unsigned long long  *Duration) = 0;

    virtual OutputTimerStatus_t   FillOutFrameTimingRecord(unsigned long long   SystemTime,
                                                           void                *ParsedAudioVideoDataParameters,
                                                           OutputTiming_t      *OutputTiming) = 0;

    virtual OutputTimerStatus_t   CorrectSynchronizationError(Buffer_t Buffer, unsigned int      TimingIndex) = 0;

private:
    OS_Mutex_t                    mMonitorGroupStructureLock;

    OutputCoordinator_t           mOutputCoordinator;
    OutputCoordinatorContext_t    mOutputCoordinatorContext;

    BufferManager_t               mBufferManager;
    BufferPool_t                  mDecodeBufferPool;
    BufferPool_t                  mCodedFrameBufferPool;
    unsigned int                  mCodedFrameBufferMaximumSize;

    CodecTrickModeParameters_t    mTrickModeParameters;
    unsigned int                  mTrickModeCheckReferenceFrames;
    Rational_t                    mTrickModeDomainTransitToDegradeNonReference;
    Rational_t                    mTrickModeDomainTransitToStartDiscard;
    Rational_t                    mTrickModeDomainTransitToDecodeReference;
    Rational_t                    mTrickModeDomainTransitToDecodeKey;
    Rational_t                    mCodedFrameRateLimitForIOnly;
    Rational_t                    mLastCodedFrameRate;
    Rational_t                    mEmpiricalCodedFrameRateLimit;
    Rational_t                    mPortionOfPreDecodeFramesToLose;
    Rational_t                    mAccumulatedPreDecodeFramesToLose;
    unsigned int                  mNextGroupStructureEntry;
    unsigned int                  mGroupStructureSizes[GROUP_STRUCTURE_INTEGRATION_PERIOD];
    unsigned int                  mGroupStructureReferences[GROUP_STRUCTURE_INTEGRATION_PERIOD];
    unsigned int                  mGroupStructureSizeCount;
    unsigned int                  mGroupStructureReferenceCount;
    Rational_t                    mTrickModeGroupStructureIndependentFrames;
    Rational_t                    mTrickModeGroupStructureReferenceFrames;
    Rational_t                    mTrickModeGroupStructureNonReferenceFrames;
    int                           mLastTrickModePolicy;  // Used to detect changes in trick mode parameters
    TrickModeDomain_t             mLastTrickModeDomain;
    Rational_t                    mLastPortionOfPreDecodeFramesToLose;
    Rational_t                    mLastTrickModeSpeed;
    Rational_t                    mLastTrickModeGroupStructureIndependentFrames;
    Rational_t                    mLastTrickModeGroupStructureReferenceFrames;
    long long                     mDecodeTimeJumpThreshold;
    long long                     mNormalizedTimeOffset;
    bool                          mTimeMappingValidForDecodeTiming;      // Values used in decode time window checking
    unsigned int                  mTimeMappingInvalidatedAtDecodeIndex;
    unsigned long long            mLastSeenDecodeTime;
    OutputTimerStatus_t           mLastFrameDropPreDecodeDecision;
    unsigned int                  mKeyFramesSinceDiscontinuity;
    unsigned long long            mLastKeyFramePlaybackTime;
    DecodeInTimeState_t           mDecodeInTimeState;
    unsigned int                  mDecodeInTimeFailures;
    unsigned int                  mDecodeInTimeCodedDataLevel;
    unsigned long long            mDecodeInTimeBehind;
    bool                          mSeenAnInTimeFrame;
    int                           mLastSynchronizationPolicy;
    bool                          mStreamNotifiedInSync;
    bool                          mSmoothReverseSupport;
    bool                          mSmoothReverseSupportChanged;
    bool                          mGlobalSetSynchronizationAtStartup;
    SynchronizationState_t        mGlobalSynchronizationState;
    Rational_t                    mLastSystemClockAdjustment;
    long long int                *mVsyncOffsets;

    DISALLOW_COPY_AND_ASSIGN(OutputTimer_Base_c);
};

#endif
