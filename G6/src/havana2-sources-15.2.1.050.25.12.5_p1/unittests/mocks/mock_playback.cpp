#include "mock_playback.h"

using ::testing::Return;
using ::testing::ReturnRef;

PlayerPlaybackInterface_c::~PlayerPlaybackInterface_c() {}

MockPlayerPlayback_c::MockPlayerPlayback_c() {
    memset(&mStatistics, 0, sizeof(mStatistics));

    EXPECT_CALL(*this, GetStatistics())
    .WillRepeatedly(ReturnRef(mStatistics));

    EXPECT_CALL(*this, IsLowPowerState())
    .WillRepeatedly(Return(false));
}

MockPlayerPlayback_c::~MockPlayerPlayback_c() {
}
