#include "mock_player_stream.h"

using ::testing::Return;
using ::testing::ReturnRef;

PlayerStreamInterface_c::~PlayerStreamInterface_c() {}

MockPlayerStream_c::MockPlayerStream_c(Player_t player, PlayerPlayback_t playback, PlayerStreamType_t streamType)
    : mPlayer(player)
    , mPlayback(playback)
    , mStreamType(streamType) {
    EXPECT_CALL(*this, GetCollateTimeFrameParser())
    .WillRepeatedly(Return(&mParser));

    memset(&mStatistics, 0, sizeof(mStatistics));

    EXPECT_CALL(*this, Statistics())
    .WillRepeatedly(ReturnRef(mStatistics));

    EXPECT_CALL(*this, GetStatistics())
    .WillRepeatedly(ReturnRef(mStatistics));

    EXPECT_CALL(*this, GetPlayer())
    .WillRepeatedly(Return(mPlayer));

    EXPECT_CALL(*this, GetPlayback())
    .WillRepeatedly(Return(mPlayback));

    EXPECT_CALL(*this, GetStreamType())
    .WillRepeatedly(Return(mStreamType));

}

MockPlayerStream_c::~MockPlayerStream_c() {
}
