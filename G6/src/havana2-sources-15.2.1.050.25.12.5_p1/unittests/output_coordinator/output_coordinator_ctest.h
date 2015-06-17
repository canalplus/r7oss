#ifndef _OUTPUT_COORDINATOR_CTEST_H
#define _OUTPUT_COORDINATOR_CTEST_H

#include "gtest/gtest.h"
#include "gmock/gmock.h"

#include "mock_player.h"
#include "mock_time.h"
#include "mock_manifestation_coordinator.h"
#include "mock_player_stream.h"
#include "mock_playback.h"
#include "player_generic.h"
#include "player_playback.h"
#include "output_coordinator_base.h"

using ::testing::_;
using ::testing::DoAll;
using ::testing::SetArgPointee;
using ::testing::Return;

// A base fixture class to use for all OutputCoordinator unitary tests
class OutputCoordinator_cTest: public ::testing::Test {
  protected:
    OutputCoordinator_cTest()
        : mMockAudioStream(&mMockPlayer, &mMockPlayback, StreamTypeAudio)
        , mMockVideoStream(&mMockPlayer, &mMockPlayback, StreamTypeVideo) {}

    virtual void SetUp() {
        OutputCoordinatorStatus_t error;

        // policy values
        mMockPlayer.mPolicyLivePlaybackLatencyVariabilityMs = kPlaybackLatency;
        mMockPlayer.mPolicyMasterClock                      = PolicyValueSystemClockMaster;
        mMockPlayer.mPolicyLivePlayback                     = PolicyValueApply;


        EXPECT_CALL(mMockPlayer, GetClassList(&mMockAudioStream, _, _, _, _, _))
        .WillRepeatedly(DoAll(
                            SetArgPointee<5>(&mAudioManifestationCoordinator),
                            Return((PlayerStatus_t)PlayerNoError)));

        EXPECT_CALL(mMockPlayer, GetClassList(&mMockVideoStream, _, _, _, _, _))
        .WillRepeatedly(DoAll(
                            SetArgPointee<5>(&mVideoManifestationCoordinator),
                            Return((PlayerStatus_t)PlayerNoError)));

        EXPECT_CALL(mAudioManifestationCoordinator, GetSurfaceParameters(_, _))
        .WillRepeatedly(Return((ManifestationCoordinatorStatus_t)ManifestationCoordinatorNoError));
        EXPECT_CALL(mVideoManifestationCoordinator, GetSurfaceParameters(_, _))
        .WillRepeatedly(Return((ManifestationCoordinatorStatus_t)ManifestationCoordinatorNoError));

        // audio manifestation coordinator behavior
        EXPECT_CALL(mAudioManifestationCoordinator, GetNextQueuedManifestationTimes(_, _))
        .WillRepeatedly(DoAll(
                            SetArgPointee<0>(&mAudioManifestationTimes[0]),
                            SetArgPointee<1>(&mAudioManifestationGranularities[0]),
                            Return((ManifestationCoordinatorStatus_t)ManifestationCoordinatorNoError)));

        // video manifestation coordinator behavior
        EXPECT_CALL(mVideoManifestationCoordinator, GetNextQueuedManifestationTimes(_, _))
        .WillRepeatedly(DoAll(
                            SetArgPointee<0>(&mVideoManifestationTimes[0]),
                            SetArgPointee<1>(&mVideoManifestationGranularities[0]),
                            Return((ManifestationCoordinatorStatus_t)ManifestationCoordinatorNoError)));

        // Expectations on mocks must be set before any call to the mock
        SetDefaultExpectations();

        error = mOutputCoordinator.RegisterPlayer(&mMockPlayer, &mMockPlayback, PlayerAllStreams);
        ASSERT_EQ(OutputCoordinatorNoError, error);

        error = mOutputCoordinator.RegisterStream(&mMockAudioStream, StreamTypeAudio, &mAudioContext);
        EXPECT_EQ(OutputCoordinatorNoError, error);

        error = mOutputCoordinator.RegisterStream(&mMockVideoStream, StreamTypeVideo, &mVideoContext);
        EXPECT_EQ(OutputCoordinatorNoError, error);
    }

    virtual void SetDefaultExpectations() {}

    // Mocks
    MockPlayer_c                    mMockPlayer;
    MockManifestationCoordinator_c  mAudioManifestationCoordinator;
    MockManifestationCoordinator_c  mVideoManifestationCoordinator;
    MockTime                        mMockTime;
    MockPlayerPlayback_c            mMockPlayback;
    MockPlayerStream_c              mMockAudioStream;
    MockPlayerStream_c              mMockVideoStream;

    // Class Under Test
    OutputCoordinator_Base_c        mOutputCoordinator;

    // structs that unfortunately cannot be mocked
    OutputCoordinatorContext_t      mAudioContext;
    OutputCoordinatorContext_t      mVideoContext;

    unsigned long long              mAudioManifestationTimes[1];
    unsigned long long              mAudioManifestationGranularities[1];
    unsigned long long              mVideoManifestationTimes[1];
    unsigned long long              mVideoManifestationGranularities[1];

    // Constants
    static const int kPlaybackLatency = 100;
};

#endif
