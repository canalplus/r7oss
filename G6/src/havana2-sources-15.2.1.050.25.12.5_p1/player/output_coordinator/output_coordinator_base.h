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

#ifndef H_OUTPUT_COORDINATOR_BASE
#define H_OUTPUT_COORDINATOR_BASE

#include "player.h"
#include "least_squares.h"

#undef TRACE_TAG
#define TRACE_TAG           "OutputCoordinator_Base_c"

#define COLLATION_TIME_DELTA_COUNT  8

typedef enum
{
    OutputRateAdjustmentNotDetermined,
    OutputRateAdjustmentInputFollowing,
    OutputRateAdjustmentOutputDriven
} OutputRateAdjustmentType_t;

typedef struct OutputRateAdjustmentParameters_s
{
    unsigned int              ClockDriftMinimumIntegrationFrames;
    unsigned int              ClockDriftMaximumIntegrationFrames;
    unsigned int              ClockDriftIgnoreBetweenIntegrations;
    unsigned int              IntegrationThresholdForDriftCorrection;
    long long                 MaximumJitterDifference;
} OutputRateAdjustmentParameters_t;

typedef struct TimingContext_s
{
    //
    // Output rate information
    //

    bool                  LastIntegrationWasRestarted;
    bool                  IntegratingClockDrift;
    unsigned int          FramesToIntegrateOver;
    unsigned int          IntegrationCount;
    LeastSquares_t        LeastSquareFit;

    bool                  ClockAdjustmentEstablished;
    Rational_t            ClockAdjustment;

    unsigned long long    LastExpectedTime;
    unsigned long long    LastActualTime;

    long long int         CurrentErrorHistory[4];

    //
    // Vsync offset information (video only)
    //

    unsigned int          VsyncOffsetIntegrationCount;
    long long int         MinimumVsyncOffset;
    long long int         VideoFrameDuration;

    TimingContext_s(void)
        : LastIntegrationWasRestarted(false)
        , IntegratingClockDrift(false)
        , FramesToIntegrateOver(0)
        , IntegrationCount(0)
        , LeastSquareFit()
        , ClockAdjustmentEstablished(false)
        , ClockAdjustment()
        , LastExpectedTime(0)
        , LastActualTime(0)
        , CurrentErrorHistory()
        , VsyncOffsetIntegrationCount(0)
        , MinimumVsyncOffset(0)
        , VideoFrameDuration(0)
    {}
} TimingContext_t;

struct OutputCoordinatorContext_s
{
    OutputCoordinatorContext_t  Next;

    PlayerStream_t              Stream;
    PlayerStreamType_t          StreamType;

    ManifestationCoordinator_t  ManifestationCoordinator;
    unsigned long long          ManifestorLatency;

    bool                        InSynchronizeFn;
    uint64_t                    SynchronizingAtPlaybackTime;
    OS_Event_t                  AbortPerformEntryIntoDecodeWindowWait;

    bool                        TimeMappingEstablished;
    bool                        BaseSystemTimeAdjusted;
    unsigned int                BasedOnMasterMappingVersion;
    unsigned long long          BaseSystemTime;
    unsigned long long          BaseNormalizedPlaybackTime;

    long long                   AccumulatedPlaybackTimeJumpsSinceSynchronization;
    long long                   LastPlaybackTimeJump;

    bool                        ClockMaster;
    unsigned int                ClockMasterIndex;             // Which surface is it on

    OutputRateAdjustmentType_t        OutputRateAdjustmentType;
    OutputRateAdjustmentParameters_t  OutputRateAdjustmentParameters;

    unsigned int                MaxSurfaceIndex;
    OutputSurfaceDescriptor_t **Surfaces;
    TimingContext_t             TimingState[MAXIMUM_MANIFESTATION_TIMING_COUNT];

    Rational_t                  OutputRateAdjustments[MAXIMUM_MANIFESTATION_TIMING_COUNT];
    long long int               VsyncOffsets[MAXIMUM_MANIFESTATION_TIMING_COUNT];

    bool                        VsyncOffsetActionsDone;
    long long int               StreamOffset;

    unsigned long long          SigmaCollationTimeDelta;
    unsigned int                NextCollationTimeDelta;
    long long int               CollationTimeDeltas[COLLATION_TIME_DELTA_COUNT];

    OutputCoordinatorContext_s(PlayerStream_t stream, PlayerStreamType_t streamtype)
        : Next(NULL)
        , Stream(stream)
        , StreamType(streamtype)
        , ManifestationCoordinator(NULL)
        , ManifestorLatency(0)
        , InSynchronizeFn(false)
        , SynchronizingAtPlaybackTime(0)
        , AbortPerformEntryIntoDecodeWindowWait()
        , TimeMappingEstablished(false)
        , BaseSystemTimeAdjusted(false)
        , BasedOnMasterMappingVersion(0)
        , BaseSystemTime(0)
        , BaseNormalizedPlaybackTime(0)
        , AccumulatedPlaybackTimeJumpsSinceSynchronization(0)
        , LastPlaybackTimeJump(INVALID_TIME)
        , ClockMaster(false)
        , ClockMasterIndex(0)
        , OutputRateAdjustmentType(OutputRateAdjustmentNotDetermined)
        , OutputRateAdjustmentParameters()
        , MaxSurfaceIndex(0)
        , Surfaces(NULL)
        , TimingState()
        , OutputRateAdjustments()
        , VsyncOffsets()
        , VsyncOffsetActionsDone(false)
        , StreamOffset((long long)INVALID_TIME)
        , SigmaCollationTimeDelta(0)
        , NextCollationTimeDelta(0)
        , CollationTimeDeltas()
    {
        OS_InitializeEvent(&AbortPerformEntryIntoDecodeWindowWait);
    }
    ~OutputCoordinatorContext_s(void)
    {
        OS_TerminateEvent(&AbortPerformEntryIntoDecodeWindowWait);
    }
private:
    DISALLOW_COPY_AND_ASSIGN(OutputCoordinatorContext_s);
};

class OutputCoordinator_Base_c : public OutputCoordinator_c
{
public:
    OutputCoordinator_Base_c(void);
    ~OutputCoordinator_Base_c(void);

    //
    // Overrides for component base class functions
    //

    OutputCoordinatorStatus_t   Halt(void);
    OutputCoordinatorStatus_t   Reset(void);

    //
    // OutputTimer class functions
    //

    OutputCoordinatorStatus_t   RegisterStream(PlayerStream_t             Stream,
                                               PlayerStreamType_t        StreamType,
                                               OutputCoordinatorContext_t   *Context);

    OutputCoordinatorStatus_t   DeRegisterStream(OutputCoordinatorContext_t   Context);

    OutputCoordinatorStatus_t   SetPlaybackSpeed(OutputCoordinatorContext_t   Context,
                                                 Rational_t            Speed,
                                                 PlayDirection_t           Direction);

    OutputCoordinatorStatus_t   ResetTimeMapping(OutputCoordinatorContext_t   Context);

    OutputCoordinatorStatus_t   EstablishTimeMapping(OutputCoordinatorContext_t   Context,
                                                     unsigned long long        NormalizedPlaybackTime,
                                                     unsigned long long        SystemTime = INVALID_TIME);

    OutputCoordinatorStatus_t   TranslatePlaybackTimeToSystem(
        OutputCoordinatorContext_t    Context,
        unsigned long long        NormalizedPlaybackTime,
        unsigned long long       *SystemTime);

    OutputCoordinatorStatus_t   TranslateSystemTimeToPlayback(
        OutputCoordinatorContext_t    Context,
        unsigned long long        SystemTime,
        unsigned long long       *NormalizedPlaybackTime);

    OutputCoordinatorStatus_t   SynchronizeStreams(OutputCoordinatorContext_t     Context,
                                                   unsigned long long        NormalizedPlaybackTime,
                                                   unsigned long long        NormalizedDecodeTime,
                                                   unsigned long long       *SystemTime,
                                                   void                     *ParsedAudioVideoDataParameters);

    OutputCoordinatorStatus_t   PerformEntryIntoDecodeWindowWait(
        OutputCoordinatorContext_t    Context,
        unsigned long long        NormalizedDecodeTime,
        unsigned long long        DecodeWindowPorch);

    OutputCoordinatorStatus_t   HandlePlaybackTimeDeltas(
        OutputCoordinatorContext_t    Context,
        bool                  KnownJump,
        unsigned long long        ExpectedPlaybackTime,
        unsigned long long        ActualPlaybackTime,
        long long int             DeltaCollationTime);

    OutputCoordinatorStatus_t   CalculateOutputRateAdjustments(
        OutputCoordinatorContext_t    Context,
        OutputTiming_t           *OutputTiming,
        Rational_t          **OutputRateAdjustments,
        Rational_t           *SystemClockAdjustment);

    OutputCoordinatorStatus_t   RestartOutputRateIntegration(
        OutputCoordinatorContext_t    Context,
        bool                  ResetCount = true,
        unsigned int              TimingIndex = INVALID_INDEX);

    OutputCoordinatorStatus_t   ResetTimingContext(OutputCoordinatorContext_t     Context,
                                                   unsigned int              TimingIndex = INVALID_INDEX);

    OutputCoordinatorStatus_t   AdjustMappingBase(OutputCoordinatorContext_t      Context,
                                                  long long             Adjustment,
                                                  bool                  ResetVSyncOffsets);

    OutputCoordinatorStatus_t   MappingBaseAdjustmentApplied(OutputCoordinatorContext_t  Context);

    OutputCoordinatorStatus_t   GetStreamStartDelay(OutputCoordinatorContext_t    Context,
                                                    unsigned long long       *Delay);

    OutputCoordinatorStatus_t   MonitorVsyncOffsets(OutputCoordinatorContext_t    Context,
                                                    OutputTiming_t           *OutputTiming,
                                                    long long int           **VsyncOffsets);

    OutputCoordinatorStatus_t   ClockRecoveryInitialize(PlayerTimeFormat_t        SourceTimeFormat);
    OutputCoordinatorStatus_t   ClockRecoveryIsInitialized(bool              *Initialized);

    OutputCoordinatorStatus_t   ClockRecoveryRestart();

    OutputCoordinatorStatus_t   ClockRecoveryDataPoint(unsigned long long         SourceTime,
                                                       unsigned long long        LocalTime);

    OutputCoordinatorStatus_t   ClockRecoveryEstimate(unsigned long long         *SourceTime,
                                                      unsigned long long       *LocalTime);

private:
    OS_Event_t                  SynchronizeMayHaveCompleted;

    unsigned int                StreamCount;
    OutputCoordinatorContext_t  CoordinatedContexts;
    unsigned int                StreamsInSynchronize;
    unsigned long long          MaximumStreamOffset;
    bool                        MasterTimeMappingEstablished;
    unsigned int                MasterTimeMappingVersion;
    unsigned long long          MasterBaseSystemTime;
    unsigned long long          MasterBaseNormalizedPlaybackTime;

    bool                        UseSystemClockAsMaster;

    bool                        SystemClockAdjustmentEstablished;
    Rational_t                  SystemClockAdjustment;

    long long                   AccumulatedPlaybackTimeJumpsSinceSynchronization;
    unsigned long long          JumpSeenAtPlaybackTime;
    OutputCoordinatorContext_t  OriginatingStream;
    bool                        OriginatingStreamConfirmsJump;
    unsigned long long          OriginatingStreamJumpedTo;


    Rational_t                  Speed;
    PlayDirection_t             Direction;

    bool                        ClockRecoveryInitialized;
    PlayerTimeFormat_t          ClockRecoverySourceTimeFormat;
    unsigned long long          ClockRecoveryDevlogSystemClockOffset;
    unsigned long long          ClockRecoveryLastPts;
    unsigned long long          ClockRecoveryPtsBaseLine;
    unsigned long long          ClockRecoveryBaseSourceClock;
    unsigned long long          ClockRecoveryBaseLocalClock;
    unsigned int                ClockRecoveryAccumulatedPoints;
    unsigned int                ClockRecoveryDiscardedPoints;
    long long                   ClockRecoveryIntegrationTime;
    LeastSquares_t              ClockRecoveryLeastSquareFit;
    Rational_t                  ClockRecoveryEstablishedGradient;

    unsigned long long          ClockRecoveryLastSourceTime;
    unsigned long long          ClockRecoveryLastLocalTime;

    long long                   LatencyUsedInHDMIRxModeDuringTimeMapping;

    void                        DrainLivePlayback(void);
    OutputCoordinatorStatus_t   VerifyMasterClock(void);
    OutputCoordinatorStatus_t   GenerateTimeMapping(void);
    OutputCoordinatorStatus_t   GenerateTimeMappingWithPCR(void);
    void                        GenerateTimeMappingForHdmiRx(OutputCoordinatorContext_t    Context,
                                                             void   *ParsedAudioVideoDataParameters);
    bool                        ReuseExistingTimeMapping(OutputCoordinatorContext_t     Context,
                                                         unsigned long long        NormalizedPlaybackTime,
                                                         unsigned long long        NormalizedDecodeTime,
                                                         unsigned long long        *SystemTime,
                                                         void                      *ParsedAudioVideoDataParameters);
    unsigned long long          SpeedScale(unsigned long long   T);
    unsigned long long          InverseSpeedScale(unsigned long long    T);

    void CalculateLongTermDriftCorrector(OutputCoordinatorContext_t   Context,
                                         TimingContext_t             *State,
                                         long long int                CurrentError,
                                         int                          i);

    void CalculateAdjustmentMultiplier(OutputCoordinatorContext_t   Context,
                                       TimingContext_t             *State,
                                       long long int                CurrentError,
                                       int                          i);

    void   UpdateVsyncOffsets(OutputCoordinatorContext_t Context, OutputTiming_t *OutputTiming, bool *UpdatesVsyncOffsets);
    void   MonitorVsyncOffsetsExternalMapping(OutputCoordinatorContext_t      Context);
    void   MonitorVsyncOffsetsInternalMapping(OutputCoordinatorContext_t      Context);

    void   ResetStreamTimeMapping(OutputCoordinatorContext_t Context);
    void   ResetMasterTimeMapping(void);
    bool   NoStreamUsesMasterTimeMapping();
    signed int RetrieveBestQualitySurface(OutputCoordinatorContext_t Context);


    long long GetLargestOfMinimumManifestationLatencies(void *ParsedAudioVideoDataParameters);

    DISALLOW_COPY_AND_ASSIGN(OutputCoordinator_Base_c);
};
#endif
