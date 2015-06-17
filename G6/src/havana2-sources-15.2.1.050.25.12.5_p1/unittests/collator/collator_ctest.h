#ifndef _COLLATOR_CTEST_H
#define _COLLATOR_CTEST_H

#include "gtest/gtest.h"
#include "gmock/gmock.h"

#include "collator.h"
#include "mock_player.h"
#include "mock_playback.h"
#include "mock_port.h"
#include "mock_buffer_pool.h"
#include "mock_buffer.h"
#include "mock_collate_time_frame_parser.h"
#include "mock_player_stream.h"

using ::testing::_;
using ::testing::DoAll;
using ::testing::SetArgPointee;
using ::testing::SetArrayArgument;
using ::testing::Return;
using ::testing::InSequence;
using ::testing::AnyNumber;

#ifndef TEST_STREAMS_DIR
#error "TEST_STREAMS_DIR not defined"
#endif
#define _STRN(arg) #arg
#define _TOSTR(arg) _STRN(arg)
#define STREAMTESTFORM1 _TOSTR( TEST_STREAMS_DIR ) "/TS_MPEG2_MPEG1-L2_1920x1080_02min04sec_Formula1.ts.audio"
#define STREAMTESTSPLMK _TOSTR( TEST_STREAMS_DIR ) "/joshort-no-splicing_markers_new.p2v"

#define NO_MARKER -(2*PES_CONTROL_SIZE)

// This value must be big enough to avoid weird behaviour with collator2 (when hitting end of buffer
// pool, collator2 reports ECollatorWouldBlock without rotating back to the beginning of the pool
// (observed with 9000 value)) and small enough to exercise the collator2 rotation path, i.e.
// smaller than the test stream used.
#define COLLATED_BUFFER_SIZE    (512*1024) // bytes

// fixture for all collator unit tests
class Collator_cTest: public ::testing::Test {
  protected:
    Collator_cTest(Collator_c &CollatorUnderTest, PlayerStreamType_t streamType)
        : mCollator(CollatorUnderTest)
        , mMockStream(&mMockPlayer, &mMockPlayback, streamType)
        , mMockCodedFrameBuffer(mMockPlayer) {}

    void PlayerExpectations() {
        EXPECT_CALL(mMockStream, GetCodedFrameBufferPool(_))
        .WillRepeatedly(DoAll(
                            SetArgPointee<0>(COLLATED_BUFFER_SIZE),
                            Return(&mMockCodedFrameBufferPool)
                        ));
        EXPECT_CALL(mMockStream, GetCodedFrameBufferPool(0))
        .WillRepeatedly(Return(&mMockCodedFrameBufferPool));
    }

    void CodedFrameBufferPoolExpectations() {
        EXPECT_CALL(mMockCodedFrameBufferPool, AttachMetaData(_, _, _, _, _))
        .WillRepeatedly(Return((int) BufferNoError));

        EXPECT_CALL(mMockCodedFrameBufferPool, GetBuffer(_, _, _, _, _, _))
        .WillRepeatedly(DoAll(
                            SetArgPointee<0>(&mMockCodedFrameBuffer),
                            Return((int) BufferNoError)));

    }

    void CodedFrameBufferExpectations() {
        EXPECT_CALL(mMockCodedFrameBuffer, ObtainDataReference(NULL, NULL, _, _))
        .WillRepeatedly(DoAll(
                            SetArgPointee<2>(&mCodedFrameBufferChunk),
                            Return((int) BufferNoError)));

        EXPECT_CALL(mMockCodedFrameBuffer, SetUsedDataSize(_))
        .WillRepeatedly(Return((int) BufferNoError));

        EXPECT_CALL(mMockCodedFrameBuffer, ShrinkBuffer(_))
        .WillRepeatedly(Return((int) BufferNoError));

    }

    // Called at beginning of SetUp() to enable expectations required for correct SetUp()
    // execution.
    //
    // Virtual to allow derived classes to override or add to the default expectation set.  Adding
    // expectations in a derived constructor or derived SetUp() before calling
    // Collator_cTest::SetUp() is not an option as gmock matches expectations last-to-first.  Adding
    // expectations in derived SetUp() after calling Collator_cTest::SetUp() is not an option either
    // for expectations required for correct SetUp() execution.
    virtual void SetDefaultExpectations() {
        PlayerExpectations();
        CodedFrameBufferPoolExpectations();
        CodedFrameBufferExpectations();
    }

    virtual void SetUp() {

        SetDefaultExpectations();

        PlayerStatus_t status = mCollator.RegisterPlayer(&mMockPlayer, &mMockPlayback, &mMockStream);
        EXPECT_EQ(PlayerNoError, status);

        status = mCollator.Connect(&mMockPort);
        EXPECT_EQ(PlayerNoError, status);
    }


    static void insertMarkerFrame(char *data, char markerType);
    void doTest(const char *filename, int blocksize, int markerFrameOffset, int markerType);


    // Class Under Test
    Collator_c             &mCollator;

    // Mocks
    MockPlayerPlayback_c        mMockPlayback;
    MockPlayerStream_c          mMockStream;
    MockPort_c                  mMockPort;
    MockPlayer_c                mMockPlayer;
    MockBufferPool_c            mMockCodedFrameBufferPool;
    MockBuffer_c                mMockCodedFrameBuffer;

    char                    mCodedFrameBufferChunk[COLLATED_BUFFER_SIZE];
};

// SUPPORT FOR MARKER INSERTION TESTS

int controlDataTypeToInternalMarkerType(int controlDataType);

struct MarkerInsertionParams {
    const char *filename;
    int blocksize;
    int markerFrameOffset;
    int markerType;
    int expectedCollatedFrames;
};

MATCHER_P(IsMarkerFrame, markerType, "") {
    MockBuffer_c *buf = (MockBuffer_c *)arg;
    return buf->mSequenceNumber.MarkerFrame == true && buf->mSequenceNumber.MarkerType == controlDataTypeToInternalMarkerType(markerType);
}

MATCHER(IsAlarmParsedPtsEvent, "") {

    bool CorrectEvent = true;
    if (arg->Value[0].UnsignedInt != 8) { // Size of the data
        CorrectEvent = false;
    }
    if (arg->Value[1].UnsignedInt != 0x01020304) { // MarkerID0
        CorrectEvent = false;
    }
    if (arg->Value[2].UnsignedInt != 0x0A0B0C0D) { // MarkerID1
        CorrectEvent = false;
    }
    if (arg->Code != EventAlarmParsedPts) {
        CorrectEvent = false;
    }
    return CorrectEvent;
}

#endif // _COLLATOR_CTEST_H
