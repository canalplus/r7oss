#ifndef _MANIFESTOR_STMFB_CTEST_H
#define _MANIFESTOR_STMFB_CTEST_H

#include "gtest/gtest.h"
#include "gmock/gmock.h"

#include "mock_player.h"
#include "mock_vibe.h"
#include "mock_relay.h"
#include "mock_ring.h"
#include "mock_decode_buffer_manager.h"
#include "mock_buffer_pool.h"
#include "mock_buffer.h"
#include "mock_player_stream.h"
#include "mock_playback.h"
#include "player_generic.h"
#include "player_playback.h"
#include "manifestor_video_stmfb.h"
#include "st_relayfs_se.h"

using ::testing::_;
using ::testing::DoAll;
using ::testing::SetArgPointee;
using ::testing::SetArrayArgument;
using ::testing::Return;
using ::testing::InSequence;
using ::testing::AnyNumber;

#define OUT_MAX     3
#define PLANE_H(i) ((stm_display_plane_h)((i)+1))
#define OUTPUT_H(i) ((stm_display_output_h)((i)+1))
#define SOURCE_H    ((stm_display_device_h)0xdead)

ACTION_P(AssignLatency, param) {*static_cast<stm_compound_control_latency_t *>(arg2) = param; }

// fixture for all  Manifestor_stmfb unit tests
class Manifestor_stmfb_cTest: public ::testing::Test {
  protected:

    Manifestor_stmfb_cTest()
        : mMockStream(&mMockPlayer, &mMockPlayback, StreamTypeVideo) {}

    void SourceExpectations() {
        EXPECT_CALL(mMockVibe, stm_display_source_get_interface((stm_display_source_h)mDisplayHandle, _, _))
        .WillRepeatedly(DoAll(
                            SetArgPointee<2>(&mQueueInterface),
                            Return(0)));
        EXPECT_CALL(mMockVibe, stm_display_source_get_device_id((stm_display_source_h)mDisplayHandle, _))
        .WillRepeatedly(Return(0));
        EXPECT_CALL(mMockVibe, stm_display_open_device(_, _))
        .WillRepeatedly(DoAll(
                            SetArgPointee<1>(SOURCE_H),
                            Return(0)));
        EXPECT_CALL(mMockVibe, stm_display_source_queue_lock(_, _))
        .WillRepeatedly(Return(0));

        EXPECT_CALL(mMockVibe, stm_display_source_get_control(_, SOURCE_CTRL_CLOCK_ADJUSTMENT, _))
        .WillRepeatedly(DoAll(
                            SetArgPointee<2>(mSourceSupportsClockPulling),
                            Return(0)));
        EXPECT_CALL(mMockVibe, stm_display_source_queue_unlock(_))
        .WillOnce(Return(0));
        EXPECT_CALL(mMockVibe, stm_display_source_queue_release(_))
        .WillOnce(Return(0));
        EXPECT_CALL(mMockVibe, stm_display_device_close(_))
        .Times(1);
        EXPECT_CALL(mMockVibe, stm_display_source_queue_flush(_, _))
        .Times(1);
    }

    void OutputsExpectations() {
        EXPECT_CALL(mMockVibe, stm_display_source_get_connected_plane_id((stm_display_source_h)mDisplayHandle, _, _))
        .WillRepeatedly(DoAll(
                            SetArrayArgument<1>(mPlaneIds, mPlaneIds + mNumOutputs),
                            Return(mNumOutputs)));

        for (int i = 0; i < PLANE_MAX; i++) {
            EXPECT_CALL(mMockVibe, stm_display_device_open_plane(_, mPlaneIds[i], _))
            .WillRepeatedly(DoAll(
                                SetArgPointee<2>(PLANE_H(i)),
                                Return(0)));
            EXPECT_CALL(mMockVibe, stm_display_plane_get_connected_output_id(PLANE_H(i), _, 1))
            .WillRepeatedly(DoAll(
                                SetArgPointee<1>(i),
                                Return(1)));
            EXPECT_CALL(mMockVibe, stm_display_device_open_output(_, i, _))
            .WillRepeatedly(DoAll(
                                SetArgPointee<2>(OUTPUT_H(i)),
                                Return(0)));
            EXPECT_CALL(mMockVibe, stm_display_plane_get_capabilities(PLANE_H(i), _))
            .WillRepeatedly(DoAll(
                                SetArgPointee<1>(mPlaneCapabilities[i]),
                                Return(0)));
            EXPECT_CALL(mMockVibe, stm_display_output_get_current_display_mode(OUTPUT_H(i), _))
            .WillRepeatedly(DoAll(
                                SetArgPointee<1>(mOutputMode[i]),
                                Return(0)));
            EXPECT_CALL(mMockVibe, stm_display_plane_get_compound_control(PLANE_H(i), PLANE_CTRL_MIN_VIDEO_LATENCY, _))
            .WillRepeatedly(DoAll(
                                AssignLatency(mOutputMinLatency[i]),
                                Return(0)));
            EXPECT_CALL(mMockVibe, stm_display_plane_get_compound_control(PLANE_H(i), PLANE_CTRL_MAX_VIDEO_LATENCY, _))
            .WillRepeatedly(DoAll(
                                AssignLatency(mOutputMaxLatency[i]),
                                Return(0)));
            EXPECT_CALL(mMockVibe, stm_display_plane_close(PLANE_H(i)))
            .Times(AnyNumber());
            EXPECT_CALL(mMockVibe, stm_display_output_close(OUTPUT_H(i)))
            .Times(AnyNumber());
            EXPECT_CALL(mMockVibe, stm_display_plane_show(PLANE_H(i)))
            .WillRepeatedly(Return(0));
        }
    }

    void VibeExpectations() {
        SourceExpectations();
        OutputsExpectations();
    }

    void VibeConfig() {
        mNumOutputs             = 1;
        mPlaneCapabilities[0]   = (stm_plane_capabilities_t)(PLANE_CAPS_VIDEO | PLANE_CAPS_VIDEO_BEST_QUALITY | PLANE_CAPS_PRIMARY_OUTPUT | PLANE_CAPS_PRIMARY_PLANE);
        mPlaneIds[0]            = 1;
        mOutputMinLatency[0]    = {2, 2};
        mOutputMaxLatency[0]    = {2, 3};

        mOutputMode[0].mode_params.vertical_refresh_rate    = 50000;
        mOutputMode[0].mode_params.scan_type                = STM_PROGRESSIVE_SCAN;
    }

    void RelayExpectations() {
        EXPECT_CALL(mMockRelay, st_relayfs_write_se(ST_RELAY_TYPE_DECODED_VIDEO_BUFFER, ST_RELAY_SOURCE_SE, _, _, 0))
        .Times(AnyNumber());
    }

    void DecodeBufferManagerExpectations() {

        EXPECT_CALL(mMockStream, GetDecodeBufferManager())
        .WillRepeatedly(Return(&mMockDecodeBufferManager));

        EXPECT_CALL(mMockDecodeBufferManager, GetDecodeBufferPool(_))
        .WillRepeatedly(DoAll(
                            SetArgPointee<0>(&mMockBufferPool),
                            Return((DecodeBufferManagerStatus_t)DecodeBufferManagerNoError)));
        EXPECT_CALL(mMockDecodeBufferManager, ComponentBaseAddress(_, PrimaryManifestationComponent, PhysicalAddress))
        .WillRepeatedly(Return((void *)0xDEADBEEF));
        EXPECT_CALL(mMockDecodeBufferManager, ComponentBaseAddress(_, PrimaryManifestationComponent, CachedAddress))
        .WillRepeatedly(Return((void *)0xDEADBEEF));
        EXPECT_CALL(mMockDecodeBufferManager, ComponentSize(_, PrimaryManifestationComponent))
        .WillRepeatedly(Return(0xBEEF));
        EXPECT_CALL(mMockDecodeBufferManager, Chroma(_, PrimaryManifestationComponent))
        .WillRepeatedly(Return((unsigned char *)0xBEEF));
        EXPECT_CALL(mMockDecodeBufferManager, Luma(_, PrimaryManifestationComponent))
        .WillRepeatedly(Return((unsigned char *)0xBEEF));
        EXPECT_CALL(mMockDecodeBufferManager, ComponentStride(_, PrimaryManifestationComponent, _, _))
        .WillRepeatedly(Return(0xBEEF));
        EXPECT_CALL(mMockDecodeBufferManager, ComponentPresent(_, DecimatedManifestationComponent))
        .WillRepeatedly(Return(false));
        EXPECT_CALL(mMockDecodeBufferManager, ComponentDimension(_, PrimaryManifestationComponent, 0))
        .WillRepeatedly(Return(1920));
        EXPECT_CALL(mMockDecodeBufferManager, ComponentDimension(_, PrimaryManifestationComponent, 1))
        .WillRepeatedly(Return(1080));
        EXPECT_CALL(mMockDecodeBufferManager, ComponentDataType(PrimaryManifestationComponent))
        .WillRepeatedly(Return(FormatVideo420_Raster2B));
        EXPECT_CALL(mMockBufferPool, GetPoolUsage(_, _, _, _, _, _))
        .WillRepeatedly(SetArgPointee<0>(8));
    }

    virtual void SetUp() {

        mDisplayHandle = (void *) 0xdeadbeef;

        VibeConfig();

        DecodeBufferManagerExpectations();
        VibeExpectations();
        RelayExpectations();
        useVibeInterface(&mMockVibe);
        useRelayInterface(&mMockRelay);

        // Manifestor init
        ManifestorStatus_t Status = mManifestorStmfb.OpenOutputSurface(mDisplayHandle);
        EXPECT_EQ(ManifestorNoError, Status);

        Status = mManifestorStmfb.Enable();
        EXPECT_EQ(ManifestorNoError, Status);


        Status = mManifestorStmfb.RegisterPlayer(&mMockPlayer, &mMockPlayback, &mMockStream);
        EXPECT_EQ(ManifestorNoError, Status);

        Status = mManifestorStmfb.Connect(&mMockOutputPort);
        EXPECT_EQ(ManifestorNoError, Status);

        mTimings[0] = &mSourceTiming;
        for (int i = 0; i < OUT_MAX; i++) {
            mTimings[i + 1] = &mOutputTiming[i];
        }
    }

    // Mocks
    MockVibe_c                  mMockVibe;
    MockRelay_c                 mMockRelay;
    MockPlayer_c                mMockPlayer;
    MockRing_c                  mMockOutputPort;
    MockDecodeBufferManager_c   mMockDecodeBufferManager;
    MockBufferPool_c            mMockBufferPool;
    MockPlayerStream_c          mMockStream;
    MockPlayerPlayback_c        mMockPlayback;
    // buffers and associated meta data

    // class under test
    Manifestor_VideoStmfb_c     mManifestorStmfb;

    // Display Source related stuff
    stm_object_h                            mDisplayHandle;
    stm_display_source_interface_params_t   mQueueInterface;
    int                                     mSourceSupportsClockPulling;

    // Display Outputs related stuff
    int                                     mNumOutputs;
    int                                     mPlaneIds[OUT_MAX];
    stm_display_mode_t                      mOutputMode[OUT_MAX];
    stm_plane_capabilities_t                mPlaneCapabilities[OUT_MAX];
    stm_compound_control_latency_t          mOutputMinLatency[OUT_MAX];
    stm_compound_control_latency_t          mOutputMaxLatency[OUT_MAX];
    ManifestationOutputTiming_t             mSourceTiming;
    ManifestationOutputTiming_t             mOutputTiming[OUT_MAX];
    ManifestationOutputTiming_t            *mTimings[OUT_MAX + 1];
};



#endif // _MANIFESTOR_STMFB_CTEST_H
