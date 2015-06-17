#ifndef _MOCK_PLAYBACK_H
#define _MOCK_PLAYBACK_H

#include "gmock/gmock.h"
#include "player_playback_interface.h"

class MockPlayerPlayback_c : public PlayerPlaybackInterface_c {
  public:
    MockPlayerPlayback_c();
    ~MockPlayerPlayback_c();

    MOCK_METHOD0(GetStatistics,
                 PlayerPlaybackStatistics_t &());
    MOCK_METHOD0(GetDrainSignalEvent,
                 OS_Event_t &());
    MOCK_METHOD0(IsLowPowerState,
                 bool());
    MOCK_METHOD0(LowPowerEnter,
                 void());
    MOCK_METHOD0(LowPowerExit,
                 void());

    PlayerPlaybackStatistics_t mStatistics;
};


#endif
