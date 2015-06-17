#include "player_generic.h"
#include "player_playback.h"
#include "player_stream.h"
#include "collator_ctest.h"
#include "collator_pes_audio_mpeg.h"

using ::testing::InSequence;

class Collator_PesAudioMpeg_cTest: public Collator_cTest {
  protected:
    Collator_PesAudioMpeg_cTest() : Collator_cTest(mCollator_PesAudioMpeg, StreamTypeAudio) {}

    Collator_PesAudioMpeg_c mCollator_PesAudioMpeg;
};

class Collator_PesAudioMpegSplicingMarkerInsertionTest : public Collator_PesAudioMpeg_cTest, public ::testing::WithParamInterface<MarkerInsertionParams> {};

TEST_P(Collator_PesAudioMpegSplicingMarkerInsertionTest, SplicingMarkerInsertionTest) {

    MarkerInsertionParams params = GetParam();

    // mMockPort expectations
    EXPECT_CALL(mMockPort, Insert(_))
    .Times(params.expectedCollatedFrames)
    .WillRepeatedly(Return(RingNoError));

    if (params.markerFrameOffset >= 0) {
        // in case a marker is injected, expect it to propagate a marker on the output port
        EXPECT_CALL(mMockPort, Insert(IsMarkerFrame(params.markerType)))
        .WillOnce(Return(RingNoError));
    }

    doTest(params.filename, params.blocksize, params.markerFrameOffset, params.markerType);
}

// check that things still work without any marker
static MarkerInsertionParams splicing_test0 = {
    STREAMTESTFORM1,
    1024,
    NO_MARKER,
    PES_CONTROL_BRK_SPLICING,
    675
};

// check insertion of a marker in the middle of a injection block
static MarkerInsertionParams splicing_test1 = {
    STREAMTESTFORM1,
    1024,
    150,
    PES_CONTROL_BRK_SPLICING,
    667
};

// check insertion of a marker at the very end of an injection block
static MarkerInsertionParams splicing_test2 = {
    STREAMTESTFORM1,
    1024,
    998,
    PES_CONTROL_BRK_SPLICING,
    668
};

// check insertion of a marker at the very beginning of an injection block
static MarkerInsertionParams splicing_test3 = {
    STREAMTESTFORM1,
    1024,
    1024,
    PES_CONTROL_BRK_SPLICING,
    668
};

// check insertion of a marker splitted of 2 injection blocks
static MarkerInsertionParams splicing_test4 = {
    STREAMTESTFORM1,
    1024,
    1015,
    PES_CONTROL_BRK_SPLICING,
    668
};

// check insertion of a marker inside accumulated data due to trailing start code bytes
static MarkerInsertionParams splicing_test5 = {
    STREAMTESTFORM1,
    1024,
    322568,
    PES_CONTROL_BRK_SPLICING,
    670
};

// check insertion of a marker cutting a pes header start code @offset 1
static MarkerInsertionParams splicing_test6 = {
    STREAMTESTFORM1,
    1024,
    1,
    PES_CONTROL_BRK_SPLICING,
    668
};

// check insertion of a marker cutting a pes header start code @offset 2
static MarkerInsertionParams splicing_test7 = {
    STREAMTESTFORM1,
    1024,
    2,
    PES_CONTROL_BRK_SPLICING,
    668
};

// check insertion of a marker cutting a pes header start code @offset 3
static MarkerInsertionParams splicing_test8 = {
    STREAMTESTFORM1,
    1024,
    3,
    PES_CONTROL_BRK_SPLICING,
    668
};

// check PES start code cut at position 3, no marker
static MarkerInsertionParams splicing_test9 = {
    STREAMTESTFORM1,
    12319,
    NO_MARKER,
    PES_CONTROL_BRK_SPLICING,
    675
};

// check PES start code cut at position 3, marker cutting start code @offset 1
static MarkerInsertionParams splicing_test10 = {
    STREAMTESTFORM1,
    12319,
    12317,
    PES_CONTROL_BRK_SPLICING,
    666
};

// check PES start code cut at position 3, marker cutting start code @offset 2
static MarkerInsertionParams splicing_test11 = {
    STREAMTESTFORM1,
    12319,
    12318,
    PES_CONTROL_BRK_SPLICING,
    666
};

// check PES start code cut at position 3, marker cutting start code @offset 3
static MarkerInsertionParams splicing_test12 = {
    STREAMTESTFORM1,
    12319,
    12319,
    PES_CONTROL_BRK_SPLICING,
    666
};


INSTANTIATE_TEST_CASE_P(
    Collator_PesAudioMpegSplicingMarkerInsertionTests,
    Collator_PesAudioMpegSplicingMarkerInsertionTest,
    ::testing::Values(splicing_test0, splicing_test1, splicing_test2, splicing_test3, splicing_test4,
                      splicing_test5, splicing_test6, splicing_test7, splicing_test8, splicing_test9, splicing_test10, splicing_test11, splicing_test12));


class Collator_PesAudioMpegChunkIdMarkerInsertionTest : public Collator_PesAudioMpeg_cTest, public ::testing::WithParamInterface<MarkerInsertionParams> {};

TEST_P(Collator_PesAudioMpegChunkIdMarkerInsertionTest, ChunkIdMarkerInsertionTest) {

    MarkerInsertionParams params = GetParam();

    // mMockPort expectations
    EXPECT_CALL(mMockPort, Insert(_))
    .Times(params.expectedCollatedFrames)
    .WillRepeatedly(Return(RingNoError));

    if (params.markerFrameOffset >= 0) {
        // in case a marker is injected, expect an event to be generated
        EXPECT_CALL(mMockStream, SignalEvent(IsAlarmParsedPtsEvent()))
        .WillOnce(Return(PlayerNoError));
    }

    doTest(params.filename, params.blocksize, params.markerFrameOffset, params.markerType);
}

static MarkerInsertionParams chunkid_test0 = {
    STREAMTESTFORM1,
    1024,
    3000,
    PES_CONTROL_REQ_TIME,
    675
};

INSTANTIATE_TEST_CASE_P(
    Collator_PesAudioMpegChunkIdMarkerInsertionTests,
    Collator_PesAudioMpegChunkIdMarkerInsertionTest,
    ::testing::Values(chunkid_test0));

