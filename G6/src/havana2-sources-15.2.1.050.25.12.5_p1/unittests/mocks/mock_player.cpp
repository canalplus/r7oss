#include "mock_player.h"

using ::testing::_;
using ::testing::ReturnPointee;
using ::testing::Return;

MockPlayer_c::MockPlayer_c()
    : mPolicyLivePlaybackLatencyVariabilityMs(0),
      mPolicyMasterClock(PolicyValueVideoClockMaster),
      mPolicyExternalTimeMapping(PolicyValueDisapply),
      mPolicyExternalTimeMappingVsyncLocked(PolicyValueDisapply),
      mPolicyRunningDevlog(false),
      mPolicyPtsForwardJumpDetectionThreshold(128),
      mPolicySymmetricJumpDetection(128),
      mPolicyLivePlayback(PolicyValueDisapply),
      mPolicyVideoStartImmediate(PolicyValueApply),
      mPolicyDiscardLateFrames(PolicyValueDiscardLateFramesNever),
      mPolicyStreamDiscardFrames(PolicyValueNoDiscard),
      mPolicyVideoLastFrameBehaviour(PolicyValueBlankScreen),
      mPolicyLimitInputInjectAhead(PolicyValueDisapply),
      mPolicyReduceCollatedData(PolicyValueDisapply),
      mPolicyControlDataDetection(PolicyValueDisapply),
      mPolicyHdmiRxMode(PolicyValueHdmiRxModeDisabled) {
    MetaDataCodedFrameParametersType            = (int) METADATA_CODED_FRAME_PARAMETERS;
    MetaDataParsedFrameParametersType           = (int) METADATA_PARSED_FRAME_PARAMETERS;
    MetaDataParsedFrameParametersReferenceType  = (int) METADATA_PARSED_FRAME_PARAMETERS_REFERENCE;
    MetaDataParsedVideoParametersType           = (int) METADATA_PARSED_VIDEO_PARAMETERS;
    MetaDataSequenceNumberType                  = (int) METADATA_SEQUENCE_NUMBER;

    EXPECT_CALL(*this, PolicyValue(_, _, PolicyStreamDiscardFrames))
    .WillRepeatedly(ReturnPointee(&mPolicyStreamDiscardFrames));

    EXPECT_CALL(*this, PolicyValue(_, _, PolicyLimitInputInjectAhead))
    .WillRepeatedly(ReturnPointee(&mPolicyLimitInputInjectAhead));

    EXPECT_CALL(*this, PolicyValue(_, _, PolicyReduceCollatedData))
    .WillRepeatedly(ReturnPointee(&mPolicyReduceCollatedData));

    EXPECT_CALL(*this, PolicyValue(_, _, PolicyDiscardLateFrames))
    .WillRepeatedly(ReturnPointee(&mPolicyDiscardLateFrames));

    EXPECT_CALL(*this, PolicyValue(_, _, PolicyVideoLastFrameBehaviour))
    .WillRepeatedly(ReturnPointee(&mPolicyVideoLastFrameBehaviour));
    EXPECT_CALL(*this, PolicyValue(_, _, PolicyLivePlaybackLatencyVariabilityMs))
    .WillRepeatedly(ReturnPointee(&mPolicyLivePlaybackLatencyVariabilityMs));

    EXPECT_CALL(*this, PolicyValue(_, _, PolicyMasterClock))
    .WillRepeatedly(ReturnPointee(&mPolicyMasterClock));

    EXPECT_CALL(*this, PolicyValue(_, _, PolicyExternalTimeMapping))
    .WillRepeatedly(ReturnPointee(&mPolicyExternalTimeMapping));

    EXPECT_CALL(*this, PolicyValue(_, _, PolicyExternalTimeMappingVsyncLocked))
    .WillRepeatedly(ReturnPointee(&mPolicyExternalTimeMappingVsyncLocked));

    EXPECT_CALL(*this, PolicyValue(_, _, PolicyRunningDevlog))
    .WillRepeatedly(ReturnPointee(&mPolicyRunningDevlog));

    EXPECT_CALL(*this, PolicyValue(_, _, PolicyPtsForwardJumpDetectionThreshold))
    .WillRepeatedly(ReturnPointee(&mPolicyPtsForwardJumpDetectionThreshold));

    EXPECT_CALL(*this, PolicyValue(_, _, PolicySymmetricJumpDetection))
    .WillRepeatedly(ReturnPointee(&mPolicySymmetricJumpDetection));

    EXPECT_CALL(*this, PolicyValue(_, _, PolicyLivePlayback))
    .WillRepeatedly(ReturnPointee(&mPolicyLivePlayback));

    EXPECT_CALL(*this, PolicyValue(_, _, PolicyVideoStartImmediate))
    .WillRepeatedly(ReturnPointee(&mPolicyVideoStartImmediate));

    EXPECT_CALL(*this, PolicyValue(_, _, PolicyControlDataDetection))
    .WillRepeatedly(ReturnPointee(&mPolicyControlDataDetection));

    EXPECT_CALL(*this, PolicyValue(_, _, PolicyVideoLastFrameBehaviour))
    .WillRepeatedly(Return(0));

    EXPECT_CALL(*this, PolicyValue(_, _, PolicyHdmiRxMode))
    .WillRepeatedly(ReturnPointee(&mPolicyHdmiRxMode));
}

MockPlayer_c::~MockPlayer_c() {
}
