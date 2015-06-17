#include "gtest/gtest.h"
#include "gmock/gmock.h"

#include "output_coordinator_ctest.h"

class OutputCoordinator_TimeMapping_cTest: public OutputCoordinator_cTest {
  protected:
    virtual void SetDefaultExpectations() {

        // We want to mock calls to GetTime in these tests
        useTimeInterface(&mMockTime);

        // for the purpose of testing the time mapping algorithm
        // we will assume that time does not change
        EXPECT_CALL(mMockTime, GetTimeInMicroSeconds())
        .WillRepeatedly(Return(100000000));
    }

    virtual void TearDown() {
        useTimeInterface(NULL);
    }

    void validateTimeMapping(unsigned long long PTS,
                             unsigned long long manifestationTime, unsigned long long playbackLatency,
                             unsigned long long earliestPTS);


    static const unsigned long long defaultStartupDelay = 40000;// must match DEFAULT_STARTUP_DELAY
    static const unsigned long long maximumStartupDelay = 260000;// must match DEFAULT_STARTUP_DELAY

};

// useful macro to get right line number in case of gtest assertion failure
#define VALIDATE_TIMEMAPPING(...) { SCOPED_TRACE(""); validateTimeMapping(__VA_ARGS__); }

void OutputCoordinator_TimeMapping_cTest::validateTimeMapping(
    unsigned long long PTS,
    unsigned long long manifestationTime,
    unsigned long long playbackLatency,
    unsigned long long earliestPTS) {
    OutputCoordinatorStatus_t error;
    unsigned long long systemTime;
    unsigned long long streamOffset;
    unsigned long long earliestSystemTime;

    streamOffset = PTS - earliestPTS;

    error = mOutputCoordinator.TranslatePlaybackTimeToSystem(NULL, PTS, &systemTime);
    EXPECT_EQ(OutputCoordinatorNoError, error);

    error = mOutputCoordinator.TranslatePlaybackTimeToSystem(NULL, earliestPTS, &earliestSystemTime);
    EXPECT_EQ(OutputCoordinatorNoError, error);

    EXPECT_LE(manifestationTime + (playbackLatency * 1000), systemTime);
    EXPECT_GE(earliestSystemTime + streamOffset, systemTime);
}

TEST_F(OutputCoordinator_TimeMapping_cTest, TimeMappingAudioFirstVideoReuses) {
    OutputCoordinatorStatus_t error;

    unsigned long long systemTime = INVALID_TIME;
    const unsigned long long PTS = 50020000ull;
    const unsigned long long DTS = 50000000ull;

    unsigned long long Now  = OS_GetTimeInMicroSeconds();
    mAudioManifestationGranularities[0] = 32000;
    mAudioManifestationTimes[0] = Now + 2 * mAudioManifestationGranularities[0];

    mVideoManifestationGranularities[0] = 40000;
    mVideoManifestationTimes[0] = Now + 2 *  mVideoManifestationGranularities[0];

    // Test time mapping based on audio only
    void *ParsedAudioVideoDataParameters = NULL;
    error = mOutputCoordinator.SynchronizeStreams(mAudioContext, PTS, UNSPECIFIED_TIME, &systemTime, ParsedAudioVideoDataParameters);

    EXPECT_EQ(OutputCoordinatorNoError, error);
    VALIDATE_TIMEMAPPING(PTS, mAudioManifestationTimes[0], kPlaybackLatency, PTS);

    unsigned long long systemTimeVideo;

    // Time mapping for video when alternate mapping is already available and suitable
    error = mOutputCoordinator.SynchronizeStreams(mVideoContext, PTS, DTS, &systemTimeVideo, ParsedAudioVideoDataParameters);
    EXPECT_EQ(OutputCoordinatorNoError, error);
    EXPECT_EQ(systemTime, systemTimeVideo);
}

TEST_F(OutputCoordinator_TimeMapping_cTest, TimeMappingVideoFirstAudioReuses) {
    OutputCoordinatorStatus_t error;

    unsigned long long systemTime = INVALID_TIME;
    const unsigned long long PTS = 50020000ull;
    const unsigned long long DTS = 50000000ull;

    unsigned long long Now  = OS_GetTimeInMicroSeconds();
    mAudioManifestationGranularities[0] = 20000;
    mAudioManifestationTimes[0] = Now + 3 * mAudioManifestationGranularities[0];

    mVideoManifestationGranularities[0] = 20000;
    mVideoManifestationTimes[0] = Now + 2 *  mVideoManifestationGranularities[0];

    // Test time mapping based on video only
    void *ParsedAudioVideoDataParameters = NULL;
    error = mOutputCoordinator.SynchronizeStreams(mVideoContext, PTS, DTS, &systemTime, ParsedAudioVideoDataParameters);

    EXPECT_EQ(OutputCoordinatorNoError, error);
    VALIDATE_TIMEMAPPING(PTS, mVideoManifestationTimes[0], kPlaybackLatency, PTS);

    unsigned long long systemTimeAudio;

    // Time mapping for audio when alternate mapping is already available and suitable
    error = mOutputCoordinator.SynchronizeStreams(mAudioContext, PTS, UNSPECIFIED_TIME, &systemTimeAudio, ParsedAudioVideoDataParameters);
    EXPECT_EQ(OutputCoordinatorNoError, error);
    EXPECT_EQ(systemTime, systemTimeAudio);
}

struct VideoSyncParams {
    OutputCoordinator_Base_c       *outputCoordinator;
    unsigned long long              PTS;
    unsigned long long              DTS;
    OutputCoordinatorContext_t      videoContext;
};

void *videoSync(void *p) {
    VideoSyncParams *params = static_cast<VideoSyncParams *>(p);
    OutputCoordinatorStatus_t error;
    unsigned long long systemTime = INVALID_TIME;
    void *ParsedAudioVideoDataParameters = NULL;
    error = params->outputCoordinator->SynchronizeStreams(
                params->videoContext, params->PTS, params->DTS, &systemTime, ParsedAudioVideoDataParameters);

    EXPECT_EQ(OutputCoordinatorNoError, error);

    OS_TerminateThread();
    return NULL;
}

TEST_F(OutputCoordinator_TimeMapping_cTest, TimeMappingAudioAndVideoTogether) {
    OutputCoordinatorStatus_t error;

    unsigned long long systemTime = INVALID_TIME;
    const unsigned long long audioPTS = 50020000ull;
    const unsigned long long videoPTS = 50200000ull;
    const unsigned long long videoDTS = 50000000ull;

    unsigned long long Now  = OS_GetTimeInMicroSeconds();
    mAudioManifestationGranularities[0] = 32000;
    mAudioManifestationTimes[0] = Now + 2 * mAudioManifestationGranularities[0];

    mVideoManifestationGranularities[0] = 40000;
    mVideoManifestationTimes[0] = Now + 2 *  mVideoManifestationGranularities[0];

    OS_Thread_t videoThread;
    VideoSyncParams params = { &mOutputCoordinator, videoPTS, videoDTS, mVideoContext };
    OS_TaskDesc_t taskdesc = {
        "SE-Video Sync",
        0,
        OS_MID_PRIORITY,
    };
    OS_Status_t status = OS_CreateThread(&videoThread, videoSync, static_cast<void *>(&params), &taskdesc);
    EXPECT_EQ(OS_NO_ERROR, status);
    void *ParsedAudioVideoDataParameters = NULL;
    error = mOutputCoordinator.SynchronizeStreams(mAudioContext, audioPTS, UNSPECIFIED_TIME, &systemTime, ParsedAudioVideoDataParameters);

    EXPECT_EQ(OutputCoordinatorNoError, error);

    VALIDATE_TIMEMAPPING(audioPTS, mAudioManifestationTimes[0], kPlaybackLatency, audioPTS);
    VALIDATE_TIMEMAPPING(videoPTS, mVideoManifestationTimes[0], kPlaybackLatency, audioPTS);

    status = OS_JoinThread(videoThread);
    EXPECT_EQ(OS_NO_ERROR, status);
}

TEST_F(OutputCoordinator_TimeMapping_cTest, TimeMappingAudioAndVideoTogetherBigOffset) {
    OutputCoordinatorStatus_t error;

    unsigned long long systemTime = INVALID_TIME;
    const unsigned long long audioPTS = 50000000ull;
    const unsigned long long videoPTS = 50800000ull;
    const unsigned long long videoDTS = 50740000ull;

    unsigned long long Now  = OS_GetTimeInMicroSeconds();
    mAudioManifestationGranularities[0] = 32000;
    mAudioManifestationTimes[0] = Now + 2 * mAudioManifestationGranularities[0];

    mVideoManifestationGranularities[0] = 20000;
    mVideoManifestationTimes[0] = Now + 2 *  mVideoManifestationGranularities[0];

    OS_Thread_t videoThread;
    VideoSyncParams params = { &mOutputCoordinator, videoPTS, videoDTS, mVideoContext };
    OS_TaskDesc_t taskdesc = {
        "SE-Video Sync",
        0,
        OS_MID_PRIORITY,
    };
    OS_Status_t status = OS_CreateThread(&videoThread, videoSync, static_cast<void *>(&params), &taskdesc);
    EXPECT_EQ(OS_NO_ERROR, status);
    void *ParsedAudioVideoDataParameters = NULL;
    error = mOutputCoordinator.SynchronizeStreams(mAudioContext, audioPTS, UNSPECIFIED_TIME, &systemTime, ParsedAudioVideoDataParameters);

    EXPECT_EQ(OutputCoordinatorNoError, error);

    VALIDATE_TIMEMAPPING(audioPTS, mAudioManifestationTimes[0], kPlaybackLatency, audioPTS);
    VALIDATE_TIMEMAPPING(videoPTS, mVideoManifestationTimes[0], kPlaybackLatency, audioPTS);

    status = OS_JoinThread(videoThread);
    EXPECT_EQ(OS_NO_ERROR, status);
}

TEST_F(OutputCoordinator_TimeMapping_cTest, TimeMappingVideoStartImmediate) {
    OutputCoordinatorStatus_t error;

    mMockPlayer.mPolicyLivePlayback         = PolicyValueDisapply;
    mMockPlayer.mPolicyVideoStartImmediate  = PolicyValueApply;

    unsigned long long systemTime = INVALID_TIME;
    const unsigned long long audioPTS = 17854976400ull;
    const unsigned long long audioDTS = audioPTS - 260000;
    const unsigned long long videoPTS = 17856184378ull;
    const unsigned long long videoDTS = 17856184377ull;

    unsigned long long Now  = OS_GetTimeInMicroSeconds();
    mAudioManifestationGranularities[0] = 32000;
    mAudioManifestationTimes[0] = Now + 100000;

    mVideoManifestationGranularities[0] = 33366;
    mVideoManifestationTimes[0] = Now + 412000;

    OS_Thread_t videoThread;
    VideoSyncParams params = { &mOutputCoordinator, videoPTS, videoDTS, mVideoContext };
    OS_TaskDesc_t taskdesc = {
        "SE-Video Sync",
        0,
        OS_MID_PRIORITY,
    };
    OS_Status_t status = OS_CreateThread(&videoThread, videoSync, static_cast<void *>(&params), &taskdesc);
    EXPECT_EQ(OS_NO_ERROR, status);
    void *ParsedAudioVideoDataParameters = NULL;
    error = mOutputCoordinator.SynchronizeStreams(mAudioContext, audioPTS, audioDTS, &systemTime, ParsedAudioVideoDataParameters);

    EXPECT_EQ(OutputCoordinatorNoError, error);

    VALIDATE_TIMEMAPPING(videoPTS, mVideoManifestationTimes[0], 0, videoPTS);

    status = OS_JoinThread(videoThread);
    EXPECT_EQ(OS_NO_ERROR, status);
}
