#ifndef _ES_PROCESSOR_CTEST_H
#define _ES_PROCESSOR_CTEST_H

#include "gtest/gtest.h"
#include "gmock/gmock.h"

#include "es_processor_base.h"
#include "mock_port.h"
#include "mock_player.h"
#include "mock_playback.h"
#include "mock_player_stream.h"


using ::testing::_;
using ::testing::DoAll;
using ::testing::SetArgPointee;
using ::testing::SetArrayArgument;
using ::testing::Return;
using ::testing::InSequence;
using ::testing::AnyNumber;

// fixture for all es_processor unit tests
class ES_Processor_cTest: public ::testing::Test {
  protected:

    ES_Processor_cTest()
        : mMockStream(&mMockPlayer, &mMockPlayback, StreamTypeVideo) {}

    void defaultExpectations() {
    }

    virtual void SetUp() {

        defaultExpectations();

        PlayerStatus_t status = mESProcessor.FinalizeInit(&mMockStream);
        EXPECT_EQ(PlayerNoError, status);

        status = mESProcessor.Connect(&mMockPort);
        EXPECT_EQ(PlayerNoError, status);
    }

    // Class Under Test
    ES_Processor_Base_c     mESProcessor;

    // Mocks
    MockPort_c              mMockPort;
    MockPlayer_c            mMockPlayer;
    MockPlayerPlayback_c    mMockPlayback;
    MockPlayerStream_c      mMockStream;
};

#endif // _ES_PROCESSOR_CTEST_H
