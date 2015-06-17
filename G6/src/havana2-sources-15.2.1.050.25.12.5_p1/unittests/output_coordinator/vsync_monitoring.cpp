
#include "gtest/gtest.h"
#include "gmock/gmock.h"

#include "output_coordinator_ctest.h"

// XXX should ideally be queried from OutputCoordinator
static const int kVsyncIntegrationCount = 10;

class OutputCoordinator_VsyncMonitoring_cTest: public OutputCoordinator_cTest {
  protected:
    virtual void SetUp() {
        OutputCoordinator_cTest::SetUp();

        mVideoContext->MaxSurfaceIndex          = 1;
        mVideoContext->Surfaces                 = mSurfaces;
        mSurfaces[0]                            = &mSourceSurface;
        mSurfaces[1]                            = &mOutputSurface;
        mVideoContext->VsyncOffsetActionsDone   = false;

        mSourceSurface.ClockPullingAvailable    = true;
        mSourceSurface.OutputType               = SURFACE_OUTPUT_TYPE_VIDEO_NO_OUTPUT;
        mSourceSurface.PercussiveCapable        = true;

        mOutputSurface.ClockPullingAvailable    = true;
        mOutputSurface.OutputType               = SURFACE_OUTPUT_TYPE_VIDEO_MAIN;
        mOutputSurface.PercussiveCapable        = false;

        mSourceSurface.FrameRate                = 25;
        mSourceSurface.Progressive              = true;

        mOutputSurface.FrameRate                = 25;
        mOutputSurface.Progressive              = true;

    }

    void doTimeMapping();
    long long  getMonitoredVsyncOffset();

    OutputSurfaceDescriptor_t *mSurfaces[2];
    OutputSurfaceDescriptor_t   mSourceSurface;
    OutputSurfaceDescriptor_t   mOutputSurface;

    unsigned long long mBaseSystemTime;
    unsigned long long mBasePlaybackTime;

};

void OutputCoordinator_VsyncMonitoring_cTest::doTimeMapping() {
    OutputCoordinatorStatus_t error;

    mBaseSystemTime     = INVALID_TIME;
    mBasePlaybackTime   = 50020000ull;

    unsigned long long Now  = OS_GetTimeInMicroSeconds();
    mAudioManifestationGranularities[0] = 32000;
    mAudioManifestationTimes[0] = Now + 2 * mAudioManifestationGranularities[0];
    void *ParsedAudioVideoDataParameters = NULL;
    error = mOutputCoordinator.SynchronizeStreams(mAudioContext, mBasePlaybackTime, UNSPECIFIED_TIME, &mBaseSystemTime, ParsedAudioVideoDataParameters);
    EXPECT_EQ(OutputCoordinatorNoError, error);
}

long long OutputCoordinator_VsyncMonitoring_cTest::getMonitoredVsyncOffset() {
    OutputCoordinatorStatus_t error;
    unsigned long long systemTime = INVALID_TIME;

    error = mOutputCoordinator.TranslatePlaybackTimeToSystem(NULL, mBasePlaybackTime, &systemTime);
    EXPECT_EQ(OutputCoordinatorNoError, error);

    return systemTime - mBaseSystemTime;
}

struct VsyncOffsetSequence {
    Rational_t  FrameRate;
    bool        Progressive;
    long long   OutputVsyncOffsets[kVsyncIntegrationCount];
    long long   ExpectedMonitoredOffset;
};

class VsyncMonitoringInternalMappingTest:  public OutputCoordinator_VsyncMonitoring_cTest,
    public ::testing::WithParamInterface<VsyncOffsetSequence> {};


TEST_P(VsyncMonitoringInternalMappingTest, VsyncMonitoringInternalMapping) {
    OutputTiming_t              timings;
    OutputCoordinatorStatus_t   Status;
    VsyncOffsetSequence         param = GetParam();

    mOutputSurface.FrameRate    = mSourceSurface.FrameRate      = param.FrameRate;
    mOutputSurface.Progressive  = mSourceSurface.Progressive    = param.Progressive;

    // we need a time mapping to assess the effect of the Vsync monitoring
    doTimeMapping();

    timings.ManifestationTimings[0].TimingValid = true;
    timings.ManifestationTimings[1].TimingValid = true;

    timings.ManifestationTimings[0].SystemPlaybackTime          = 100000;
    timings.ManifestationTimings[0].ActualSystemPlaybackTime    = 100000;
    timings.ManifestationTimings[1].SystemPlaybackTime          = 200000;

    for (int i = 0; i < kVsyncIntegrationCount; i++) {
        timings.ManifestationTimings[1].ActualSystemPlaybackTime =
            timings.ManifestationTimings[1].SystemPlaybackTime + param.OutputVsyncOffsets[i];

        Status = mOutputCoordinator.MonitorVsyncOffsets(mVideoContext, &timings, 0);
    }
    EXPECT_EQ(OutputCoordinatorNoError, Status);

    EXPECT_EQ(param.ExpectedMonitoredOffset, getMonitoredVsyncOffset());
}

struct VsyncOffsetSequence Seq1 = {
    Rational_t(24000, 1001),
    true,
    { 145985, 145987, 13, 14, -10222, -10227, -10229, -10221, -10221, -10221 },
    -10229
};

struct VsyncOffsetSequence Seq2 = {
    Rational_t(24000, 1001),
    true,
    { -180, -182, -185, -187, -4242, -4225, -4221, -4235, -4235, -4235 },
    -4235
};

struct VsyncOffsetSequence Seq3 = {
    Rational_t(24000, 1000),
    true,
    { -457, -459, -457, -457, -1900, -1902, -1905, -1899, -1899, -1899 },
    -1905
};

struct VsyncOffsetSequence Seq4 = {
    Rational_t(25000, 1000),
    true,
    { -2, -2, -3, -1, 1729, 1738, 1732, 1735, 1735, 1735 },
    1732
};

struct VsyncOffsetSequence Seq5 = {
    Rational_t(25000, 1000),
    true,
    { 710, 677, -30125, -30813, -30855, -30136, -30132, -30131, -30131, -30131 },
    -30136
};

struct VsyncOffsetSequence Seq6 = {
    Rational_t(25000, 1000),
    false,
    { 19994, 19994, 19994, 3561, 3564, 3563, 3562, 3548, 3548, 3548 },
    3548
};

struct VsyncOffsetSequence Seq7 = {
    Rational_t(25000, 1000),
    false,
    { 0, 0, 7016, 7017, 7016, 7016, 7016, 7016, 7016, 7016 },
    7016
};

struct VsyncOffsetSequence Seq8 = {
    Rational_t(25000, 1000),
    true,
    { -29, -30, 4889, 4891, 4887, 4884, 4897, 4890, 4890, 4890  },
    4884
};

struct VsyncOffsetSequence Seq9 = {
    Rational_t(30000, 1001),
    true,
    { -7, -4, -4, -6, -5583, -5592, -5588, -5592, -5592, -5592 },
    -5592
};

struct VsyncOffsetSequence Seq10 = {
    Rational_t(60000, 1001),
    true,
    { -16679, -16680, -16679, -29343, -29343, -29334, -29338, -29345, -29345, -29345 },
    -29345
};

struct VsyncOffsetSequence Seq11 = {
    Rational_t(24000, 1001),
    true,
    { -87608, -87610, -4, -7, 11679, 11669, 11676, 11659, 11659, 11659 },
    11659
};


INSTANTIATE_TEST_CASE_P(
    VsyncMonitoringInternalMappingTests,
    VsyncMonitoringInternalMappingTest,
    ::testing::Values(Seq1, Seq2, Seq3, Seq4, Seq5, Seq6, Seq7, Seq8, Seq9, Seq10, Seq11));

