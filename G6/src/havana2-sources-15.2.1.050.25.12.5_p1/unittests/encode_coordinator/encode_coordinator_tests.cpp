#include "encode_coordinator_ctest.h"
#include "mock_buffer.h"
#include "stdio.h"

using ::testing::_;
using ::testing::InvokeArgument;
using ::testing::Invoke;
using ::testing::ByRef;
using ::testing::InSequence;

#define NATIVE_TIME_FORMAT      TIME_FORMAT_US

//
// According from the specification array , allocates all the buffers with correct
// feature , like pts , mediatype , eos , frameduration
// and put them in an array which will be used to inject the buffer
unsigned int    FillBufferList(MockEncoder_c &enc, MockBuffer_c  *Buf[], unsigned int frameduration, unsigned int spec[], stm_se_encode_stream_media_t mediatype) {
    int i = 0, j = 0;
    unsigned long long current_time = 0;

    do {
        if (!ISATEMPO(spec[i])) {
            Buf[j] = new MockBuffer_c(enc, frameduration * 1000);
            Buf[j]->mUncompressedFrameMetaData.native_time_format = NATIVE_TIME_FORMAT;
            Buf[j]->mUncompressedFrameMetaData.native_time = PTS(spec[i]) * 1000;
            Buf[j]->mUncompressedFrameMetaData.media = mediatype;
            Buf[j]->mUncompressedFrameMetaData.discontinuity = STM_SE_DISCONTINUITY_CONTINUOUS;


            Buf[j]->injection_time = current_time;
            SE_INFO(group_encoder_stream, "%p %s(%u) injection time=%lld\n", Buf[j], (mediatype == STM_SE_ENCODE_STREAM_MEDIA_AUDIO) ? "A" : "V", PTS(spec[i]), current_time);

            current_time += frameduration * 1000;
            j++;
        } else {
            current_time += TEMPO(spec[i]) * 1000;

        }
    } while (! EOS(spec[i++]));
    Buf[j - 1]->mUncompressedFrameMetaData.discontinuity = STM_SE_DISCONTINUITY_EOS;
    return j;
}

//
// Search in a an array a buffer matching a specific pts
//
MockBuffer_c    *GetBuffAtTime(MockBuffer_c  *Buf[], unsigned int n, unsigned int pts, unsigned int MediaType) {
    // convert to us
    pts = pts * 1000;


    switch (MediaType) {

    case AUDIO_MUTED:
    case VIDEO_REPEAT:
    case AUDIO_EOS:
    case VIDEO_EOS:
        return NULL;
        break;

    default:
        for (unsigned int i = 0; i < n; i++) {
            if (Buf[i]->mUncompressedFrameMetaData.native_time == pts) {
                return Buf[i];
            }
        }
        SE_ERROR("Unable to find buffer matching time %d", pts);
        break;
    }

    return NULL;
}


//
// according from the expected specification fill an array with
// the buffer. buffers are pickup from 2 different array , one for
// the audio , one for the video.
unsigned int    FillExpectedBufferList(MockBuffer_c  *Buf[], MockBuffer_c  *ABuf[], unsigned int na, MockBuffer_c  *VBuf[], unsigned int nv, spec_t spec[]) {
    int i = 0;
    unsigned int  pts;

    while (NOT_THE_END(spec[i])) {
        pts = EXPECTED_PTS(spec[i]);

        if (VIDEO(spec[i])) {
            Buf[i] = GetBuffAtTime(VBuf, nv, pts, spec[i].mMediaType);
        } else {
            Buf[i] = GetBuffAtTime(ABuf, na, pts, spec[i].mMediaType);
        }
        i++;
    }

    return i;
}




using ::testing::Mock;

MATCHER_P(IsBufferReleased, BufferToRelease,  "") {
    SE_INFO(group_encoder_stream, "in IsBufferReleased Buffer = %p expected =  %p\n", arg, BufferToRelease);
    //FIXME : seems we cannot call delete arg  from here , gmock limitattion ??
    return (arg == BufferToRelease) ;
}


//
// create the expect for the buffer which are return dirtectly
// by the output coordinator. These buffers are the one which are
// not in the expected list ..
// this function is called with All set of input buffers.

void EncodeCoordinator_cTest::CreateExpectedRelease(MockBuffer_c  *InputBuf[], unsigned int n,
                                                    MockBuffer_c  *ExpectedBuf[], unsigned int nout) {

    for (unsigned int i = 0; i < n ; i++) {
        EXPECT_CALL(mMockReleaseBufferInterface, ReleaseBuffer(IsBufferReleased(InputBuf[i])))
        .WillOnce(Return(PlayerNoError));
    }
}

using ::testing::InSequence;

int GetEOS;


MATCHER_P2(EncCoordinatorOutput1, ExpectedBuffer, Spec, "") {
    MockBuffer_c *Buffer = (MockBuffer_c *) arg;
    bool match = false;
    char *s;
    stm_se_uncompressed_frame_metadata_t    UncompressedFrameMetaData;
    __stm_se_encode_coordinator_metadata_t  EncodeCoordinatorFrameMetaData;

    UncompressedFrameMetaData = Buffer->mUncompressedFrameMetaData;
    EncodeCoordinatorFrameMetaData = Buffer->mEncodeCoordinatorFrameMetaData;

    switch (Spec->mMediaType) {

    case VIDEO_NEWGOP:
        s = "NEWGOP";
        match = (ExpectedBuffer == Buffer);
        match = match && (UncompressedFrameMetaData.native_time_format == NATIVE_TIME_FORMAT);
        match = match && (UncompressedFrameMetaData.native_time == (uint64_t)Spec->mPTS * 1000);
        match = match && (EncodeCoordinatorFrameMetaData.encoded_time_format == NATIVE_TIME_FORMAT);
        match = match && (EncodeCoordinatorFrameMetaData.encoded_time == (uint64_t) Spec->mRemapPTS * 1000) ;
        match = match && (UncompressedFrameMetaData.discontinuity & STM_SE_DISCONTINUITY_CLOSED_GOP_REQUEST);
        break;

    case AUDIO_FADEIN:
        s = "FADEIN";
        match = (ExpectedBuffer == Buffer);
        match = match && (UncompressedFrameMetaData.native_time_format == NATIVE_TIME_FORMAT);
        match = match && (UncompressedFrameMetaData.native_time == (uint64_t)Spec->mPTS * 1000);
        match = match && (EncodeCoordinatorFrameMetaData.encoded_time_format == NATIVE_TIME_FORMAT);
        match = match && (EncodeCoordinatorFrameMetaData.encoded_time == (uint64_t) Spec->mRemapPTS * 1000) ;
        match = match && (UncompressedFrameMetaData.discontinuity & STM_SE_DISCONTINUITY_FADEIN);
        break;

    case AUDIO_FADEOUT:
        s = "FADEOUT";
        match = (ExpectedBuffer == Buffer);
        match = match && (UncompressedFrameMetaData.native_time_format == NATIVE_TIME_FORMAT);
        match = match && (UncompressedFrameMetaData.native_time == (uint64_t)Spec->mPTS * 1000);
        match = match && (EncodeCoordinatorFrameMetaData.encoded_time_format == NATIVE_TIME_FORMAT);
        match = match && (EncodeCoordinatorFrameMetaData.encoded_time == (uint64_t) Spec->mRemapPTS * 1000) ;
        match = match && (UncompressedFrameMetaData.discontinuity & STM_SE_DISCONTINUITY_FADEOUT);
        break;

    case AUDIO_NORMAL:
        s = "A";
        match = (ExpectedBuffer == Buffer);
        match = match && (UncompressedFrameMetaData.native_time_format == NATIVE_TIME_FORMAT);
        match = match && (UncompressedFrameMetaData.native_time == (uint64_t)Spec->mPTS * 1000);
        match = match && (EncodeCoordinatorFrameMetaData.encoded_time_format == NATIVE_TIME_FORMAT);
        match = match && (EncodeCoordinatorFrameMetaData.encoded_time == (uint64_t) Spec->mRemapPTS * 1000) ;
        break;

    case VIDEO_NORMAL:
        s = "V";
        match = (ExpectedBuffer == Buffer);
        match = match && (UncompressedFrameMetaData.native_time_format == NATIVE_TIME_FORMAT);
        match = match && (UncompressedFrameMetaData.native_time == (uint64_t)Spec->mPTS * 1000);
        match = match && (EncodeCoordinatorFrameMetaData.encoded_time_format == NATIVE_TIME_FORMAT);
        match = match && (EncodeCoordinatorFrameMetaData.encoded_time == (uint64_t) Spec->mRemapPTS * 1000) ;
        break;

    case AUDIO_MUTED:
        s = "M";
        match = true; // No match on buffer content
        match = match && (UncompressedFrameMetaData.native_time_format == NATIVE_TIME_FORMAT);
        match = match && (UncompressedFrameMetaData.native_time == (uint64_t)Spec->mPTSrepeat * 1000);
        match = match && (EncodeCoordinatorFrameMetaData.encoded_time_format == NATIVE_TIME_FORMAT);
        match = match && (EncodeCoordinatorFrameMetaData.encoded_time == (uint64_t) Spec->mRemapPTS * 1000) ;
        match = match && (UncompressedFrameMetaData.discontinuity & STM_SE_DISCONTINUITY_MUTE);
        break;

    case VIDEO_REPEAT:
        s = "R";
        match = true; // No match on buffer content
        match = match && (UncompressedFrameMetaData.native_time_format == NATIVE_TIME_FORMAT);
        match = match && (UncompressedFrameMetaData.native_time == (uint64_t)Spec->mPTSrepeat * 1000);
        match = match && (EncodeCoordinatorFrameMetaData.encoded_time_format == NATIVE_TIME_FORMAT);
        match = match && (EncodeCoordinatorFrameMetaData.encoded_time == (uint64_t) Spec->mRemapPTS * 1000) ;
        break;

    case AUDIO_EOS:
        s = "AEOS";
        match = true; // No match on buffer content
        match = match && ((UncompressedFrameMetaData.discontinuity & STM_SE_DISCONTINUITY_EOS) == STM_SE_DISCONTINUITY_EOS);
        match = match && (UncompressedFrameMetaData.media == STM_SE_ENCODE_STREAM_MEDIA_AUDIO);
        if (match) { GetEOS++; }
        break;

    case VIDEO_EOS:
        s = "VEOS";
        match = true; // No match on buffer content
        match = match && ((UncompressedFrameMetaData.discontinuity & STM_SE_DISCONTINUITY_EOS) == STM_SE_DISCONTINUITY_EOS);
        match = match && (UncompressedFrameMetaData.media == STM_SE_ENCODE_STREAM_MEDIA_VIDEO);
        if (match) { GetEOS++; }
        break;

    default:
        s = "INVALID";
        break;
    }

    SE_INFO(group_encoder_stream, "%s get %p(%u,%u) expected %p %s(pts=%lld,remap=%lld)\n",
            match ? "MATCH" : "",
            Buffer,
            (unsigned int) UncompressedFrameMetaData.native_time,
            (unsigned int) EncodeCoordinatorFrameMetaData.encoded_time,
            ExpectedBuffer,
            s,
            (uint64_t)EXPECTED_PTS(*Spec) * 1000,
            (uint64_t)EXPECTED_REMAP_PTS(*Spec) * 1000);

    return (match);
}




RingStatus_t EncodeCoordinator_cTest::MyReleaseBuffer(uintptr_t arg) {
    MockBuffer_c *Buffer = (MockBuffer_c *) arg;
    ReleaseBufferInterface_c *cReleaseBuffer;
    if (Buffer->mUncompressedFrameMetaData.media == STM_SE_ENCODE_STREAM_MEDIA_AUDIO) {
        cReleaseBuffer = mEncodeCoordinatorStreamAudioReleaseBufferInterface;
    } else {
        cReleaseBuffer = mEncodeCoordinatorStreamVideoReleaseBufferInterface;
    }


    SE_DEBUG(group_encoder_stream, "MyReleaseBuffer media= %s\n", Buffer->mUncompressedFrameMetaData.media == 0 ? "AUDIO" : "VIDEO");

    cReleaseBuffer->ReleaseBuffer(Buffer);

    return RingNoError;

}

void EncodeCoordinator_cTest::runtest(unsigned int SpecAudioBuffer[], unsigned int Aduration, unsigned int SpecVideoBuffer[], unsigned int Vduration,  spec_t SpecExpected[]) {
    MockBuffer_c  *AudioBuf[MAXTESTLEN];
    MockBuffer_c  *VideoBuf[MAXTESTLEN];
    MockBuffer_c  *ExpectedBuf[MAXTESTLEN];
    unsigned int na, nv, nout;

    // create the input buffer array for Audio and Video
    na = FillBufferList(*mMockEncoder, AudioBuf, Aduration, SpecAudioBuffer, STM_SE_ENCODE_STREAM_MEDIA_AUDIO);
    nv = FillBufferList(*mMockEncoder, VideoBuf, Vduration, SpecVideoBuffer, STM_SE_ENCODE_STREAM_MEDIA_VIDEO);

    // Create the ordered expected buffer list
    nout = FillExpectedBufferList(ExpectedBuf, AudioBuf, na, VideoBuf, nv, SpecExpected);
    SE_INFO(group_encoder_stream, "Buffer count, Audio: %d  Video:%d  Expected;%d\n", na, nv, nout);

    //create expectaction list for buffer than should be returned immediately
    CreateExpectedRelease(VideoBuf, nv, ExpectedBuf, nout);
    CreateExpectedRelease(AudioBuf, na, ExpectedBuf, nout);

    // Here InSequence test
    {

        InSequence  Dummy;
        // create expectations for the output of the transcode coordinator

        for (unsigned int i = 0; i < nout; i++) {

            SE_INFO(group_encoder_stream, "Buffer Expected %p  pts: %lld\n",
                    ExpectedBuf[i],
                    (ExpectedBuf[i] != NULL) ? ExpectedBuf[i]->mUncompressedFrameMetaData.native_time : 0
                   ) ;
            EXPECT_CALL(mMockAudioVideoStreamInputPort, Insert(EncCoordinatorOutput1(ExpectedBuf[i], &SpecExpected[i])))
            .WillRepeatedly(Invoke(this, &EncodeCoordinator_cTest::MyReleaseBuffer));
        }

    }


    // lets go insert all buffers and managed time
    unsigned int i = 0, j = 0;
    GetEOS = 0;
    do {
        if (i < na) {
            if (mTime >= AudioBuf[i]->injection_time) {
                mEncodeCoordinatorAudioPort->Insert((uintptr_t) AudioBuf[i]);
                SE_INFO(group_encoder_stream, "Test Inject A(%lld) %dms  %p at time %lld\n", AudioBuf[i]->mUncompressedFrameMetaData.native_time,
                        AudioBuf[i]->GetDuration(),
                        AudioBuf[i], mTime) ;
                i++;
            }
        }

        if (j < nv) {
            if (mTime >= VideoBuf[j]->injection_time) {
                mEncodeCoordinatorVideoPort->Insert((uintptr_t) VideoBuf[j]);
                SE_INFO(group_encoder_stream, "Test Inject V(%lld) %dms  %p at time %lld\n", VideoBuf[j]->mUncompressedFrameMetaData.native_time,
                        VideoBuf[j]->GetDuration(),
                        VideoBuf[j], mTime) ;
                j++;
            }
        }
        OS_SleepMilliSeconds(10);
        mTime += 5000;
    } while ((j < nv) || (i < na));

    SE_INFO(group_encoder_stream, "Test Injection DOne\n");

    // looping here until we get EOS on both streams
    int twait = 1000;
    while ((GetEOS != 2) && (twait >= 0)) {
        OS_SleepMilliSeconds(500);
        twait -= 50;
    }

    EXPECT_GE(twait, 0);


    // Release allocated buffer
    for (unsigned int i = 0; i < na; i++) { delete    AudioBuf[i]; }
    for (unsigned int i = 0; i < nv; i++) { delete    VideoBuf[i]; }


}





/*
How to create a New Test ,

Basically you have to only create 3 different Array ,one for the Audio input stream ,one
for the Video input stream and one for the output expected stream

The array SpecAudioBuffer and SpecVideoBuffer describes input of the transcode coordinator
in current approach , a buffer is characterized by its Presentation time stamps

unsigned int SpecAudioBuffer[]= {0, 10 | EOSMARKER};

Means two buffers,  first one with PTS =0 , second one with PTS=10  , unit is ms
last buffer must be marked with EOSMARKER to help runtest functionto determine how many
buffer are sent


spec_t SpecExpected[]   = { A(0),V(0) ,END};

Describes the expected output buffers.Buffer are listed in the order they should be output
from the transcode coordinator. { A(0),V(0) ,END} means that Audio Buffer with PTS=0 is output
first, followed by Video Bufffer with PTS=0.

END indicates the end of the expected buffers.
By convention when buffer have same PTS we expect the AUdio to be outputed first.

 Here is the description of the supported syntax :

 A(10)      :Audio buffer with PTS=10 is expected at output stage
 V(10)      :Video buffer with PTS=10 is expected at output stage
 A(10,20)   :Audio buffer with original PTS=10 , remap PTS=20
        when remap is not specified it is assumed to be the
        same as the orginal one.

 FADEIN(10,10) :means Audio buffer , with FadeIn flag set
 FADEOUT(10,10) :means Audio buffer , with FadeOut flag set
 NEWGOP(10,10) :means Video buffer , with NEWGOP flag set
 M(10)       :Muted Audio buffer with PatchedPTS andPTS set to 10
 R(10,20)    :Repeated Video buffer with PTS and remapPTS equal to 20

*/

TEST_F(EncodeCoordinator_cTest, Short) {
    unsigned int SpecAudioBuffer[] = { 10 | EOSMARKER};
    unsigned int SpecVideoBuffer[] = { 10 | EOSMARKER};
    spec_t SpecExpected[]   = { AEOS, VEOS, END};

    runtest(SpecAudioBuffer, 10, SpecVideoBuffer, 10, SpecExpected);
}


TEST_F(EncodeCoordinator_cTest, Init) {
    unsigned int SpecAudioBuffer[] = {0, 10 | EOSMARKER};
    unsigned int SpecVideoBuffer[] = {0, 10 | EOSMARKER};
    spec_t SpecExpected[]   = { A(0), V(0) , AEOS , VEOS, END};

    runtest(SpecAudioBuffer, 10, SpecVideoBuffer, 10, SpecExpected);
}

TEST_F(EncodeCoordinator_cTest, InitDiscardAudio) {
    unsigned int SpecAudioBuffer[] = {0, 10 , 20 , 100 , 110 | EOSMARKER};
    unsigned int SpecVideoBuffer[] = {100, 110 | EOSMARKER};
    spec_t SpecExpected[]   = { A(100),  V(100) , AEOS , VEOS, END};

    runtest(SpecAudioBuffer, 10, SpecVideoBuffer, 10, SpecExpected);
}

TEST_F(EncodeCoordinator_cTest, InitDiscardVideo) {
    unsigned int SpecAudioBuffer[] = {100, 110 | EOSMARKER};
    unsigned int SpecVideoBuffer[] = {0, 10 , 20 , 100 , 110 | EOSMARKER};
    spec_t SpecExpected[]   = { A(100), V(100) , AEOS, VEOS, END};

    runtest(SpecAudioBuffer, 10, SpecVideoBuffer, 10, SpecExpected);
}

TEST_F(EncodeCoordinator_cTest, BasicSameDuration) {

    unsigned int SpecAudioBuffer[] = {10, 20 , 30 , 40 | EOSMARKER};
    unsigned int SpecVideoBuffer[] = {10, 20 , 30 , 40 | EOSMARKER};
    spec_t SpecExpected[]  = { A(10), V(10), A(20), V(20), A(30), V(30), AEOS,  VEOS, END};

    runtest(SpecAudioBuffer, 10, SpecVideoBuffer, 10, SpecExpected);

}

TEST_F(EncodeCoordinator_cTest, BasicDifferentDuration1) {

    unsigned int SpecAudioBuffer[] = {0, 5, 10, 15, 20, 25 | EOSMARKER};
    unsigned int SpecVideoBuffer[] = {0, 20 , 40 | EOSMARKER};
    spec_t SpecExpected[]   = { A(0), V(0), A(5), A(10), A(15), A(20),  V(20), AEOS , VEOS, END};
    runtest(SpecAudioBuffer, 5, SpecVideoBuffer, 20, SpecExpected);
}
#if 0
TEST_F(EncodeCoordinator_cTest, BasicDifferentDuration) {

    unsigned int SpecAudioBuffer[] = {0, 5, 10, 15, 20, 25, 30, 35, 40, 45 | EOSMARKER};
    unsigned int SpecVideoBuffer[] = {0, 20, 40, 60 | EOSMARKER};
    spec_t SpecExpected[]   = { A(0), V(0), A(5), A(10), A(15), A(20), V(20), A(25), A(30), A(35), A(40), V(40), END};
    runtest(SpecAudioBuffer, 5, SpecVideoBuffer, 20, SpecExpected);
}
#endif

#if 0
// OBSOLET
// Here On GAP test assume no repeat or no softmute
// that is development step !
TEST_F(EncodeCoordinator_cTest, GapAudio) {

    unsigned int SpecAudioBuffer[] = {0, 5, 10, 30, 35, 40, 45 | EOSMARKER};
    unsigned int SpecVideoBuffer[] = {0, 20, 40, 60 | EOSMARKER};
    spec_t SpecExpected[]   = { A(0), V(0), A(5), A(10), V(20), A(30), A(35), A(40), V(40), END};
    runtest(SpecAudioBuffer, 5, SpecVideoBuffer, 20, SpecExpected);
}

TEST_F(EncodeCoordinator_cTest, GapVideo) {

    unsigned int SpecAudioBuffer[] = {0, 5, 10, 15, 20, 25, 30, 35, 40, 45 | EOSMARKER};
    unsigned int SpecVideoBuffer[] = {0, 40, 60 | EOSMARKER};
    spec_t SpecExpected[]   = { A(0), V(0), A(5), A(10), A(15), A(20), A(25), A(30), A(35), V(40), A(40), END};
    runtest(SpecAudioBuffer, 5, SpecVideoBuffer, 20, SpecExpected);
}

TEST_F(EncodeCoordinator_cTest, Gap_FSO1) {

    unsigned int SpecAudioBuffer[] = {0, 5, 10, 15, 20, 25, 30, 35, 40, 45, 50,  60, 70, 75, 80 | EOSMARKER};
    unsigned int SpecVideoBuffer[] = {0,  40, 80 | EOSMARKER};
    spec_t SpecExpected[]   = { A(0), V(0), A(5), A(10), A(15), A(20), A(25), A(30), A(35), V(40), A(40), A(45), A(50),  A(70), A(75), END};

    runtest(SpecAudioBuffer, 5, SpecVideoBuffer, 20, SpecExpected);
}

TEST_F(EncodeCoordinator_cTest, Gap_FSO2) {

    unsigned int SpecAudioBuffer[] = {0, 5, 10,   50, 55, 60 | EOSMARKER};
    unsigned int SpecVideoBuffer[] = {0,     40,        60 | EOSMARKER};
    spec_t SpecExpected[]   = { A(0), V(0), A(5), A(10), V(40), A(50), A(55), END};
//                                                   20     30      35

    runtest(SpecAudioBuffer, 5, SpecVideoBuffer, 20, SpecExpected);
}


//FIXME  remapPTS should be different from current PTS
// not visible on this test ...
TEST_F(EncodeCoordinator_cTest, GapAudioVideo) {

    unsigned int SpecAudioBuffer[] = {0, 5, 10, 15, 30, 35, 40, 45 | EOSMARKER};
    unsigned int SpecVideoBuffer[] = {0, 40, 60 | EOSMARKER};
    spec_t SpecExpected[]   = { A(0), V(0), A(5), A(10), A(15) , A(30), A(35), V(40), A(40), END};
    runtest(SpecAudioBuffer, 5, SpecVideoBuffer, 20, SpecExpected);
}
#endif

// Here we test a gap in Audio , and expect FadeOut FadeIn

TEST_F(EncodeCoordinator_cTest, GapAudioX) {

    unsigned int SpecAudioBuffer[] = {0, 5, 10, 30, 35, 40, 45 | EOSMARKER};
    unsigned int SpecVideoBuffer[] = {0, 20, 40, 60 | EOSMARKER};
    spec_t SpecExpected[]   = { A(0), V(0), A(5), FADEOUT(10), M_(10, 15), M_(10, 20), V(20), M_(10, 25), FADEIN(30), A(35), A(40),  V(40), AEOS, VEOS , END};
    runtest(SpecAudioBuffer, 5, SpecVideoBuffer, 20, SpecExpected);
}

TEST_F(EncodeCoordinator_cTest, GapVideoX) {

    unsigned int SpecAudioBuffer[] = {0, 5, 10, 15, 20, 25, 30, 35, 40, 45 | EOSMARKER};
    unsigned int SpecVideoBuffer[] = {0, 40, 60 | EOSMARKER};
    spec_t SpecExpected[]   = { A(0), V(0), A(5), A(10), A(15), A(20), R_(0, 20), A(25), A(30), A(35), A(40), NEWGOP(40), AEOS, VEOS, END};
    runtest(SpecAudioBuffer, 5, SpecVideoBuffer, 20, SpecExpected);
}


/*
GapAudioVideoX1 : No compression in this cas because gap is less that max frame duration
                             |                          |
                    FadeOut  |            FadeIn        |
A) 0 --- 5--- 10 ---15       |M(20) M(25) 30 ---35 ---- |40
                             |                          |
B) 0 ----------------------- |20                        |40 -----------------------
                             |Repeat(0)                 |  NewGop
 */

TEST_F(EncodeCoordinator_cTest, GapAudioVideoX1) {

    unsigned int SpecAudioBuffer[] = {0, 5, 10, 15, 30, 35, 40, 45 | EOSMARKER};
    unsigned int SpecVideoBuffer[] = {0, 40, 60, 80 | EOSMARKER};

    spec_t SpecExpected[]   = { A(0), V(0), A(5), A(10), FADEOUT(15), M_(15, 20), R_(0, 20),  M_(15, 25), FADEIN_(30, 30), A(35), A(40) , NEWGOP_(40, 40), AEOS , V(60), VEOS , END};
    runtest(SpecAudioBuffer, 5, SpecVideoBuffer, 20, SpecExpected);
}

/*
GapAudioVideoX2 : Here compression o 5 video frames
                             |                               |
                    FadeOut  |              FadeIn           |
A) 0 --- 5--- 10 ---15       |M(20) M(25) A(130,30) A(135,35)|140
                             |            ^                  |
B) 0 ----------------------- |20          |                  |140 -----------------------
                             |Repeat(0)   |                  |  NewGop(140,40)
                             |            |                  |
                                          |
                                          |Time compression here (Offset -100)
 */
TEST_F(EncodeCoordinator_cTest, GapAudioVideoX2) {

    unsigned int SpecAudioBuffer[] = {0, 5, 10, 15, 130, 135, 140, 145 | EOSMARKER};
    unsigned int SpecVideoBuffer[] = {0, 140, 160, 180 | EOSMARKER};

    spec_t SpecExpected[]   = { A(0), V(0), A(5), A(10), FADEOUT(15), R__(0, 120, 20),  FADEIN_(130, 30), A_(135, 35), A_(140, 40), NEWGOP_(140, 40), AEOS , V_(160, 60), VEOS, END};
    runtest(SpecAudioBuffer, 5, SpecVideoBuffer, 20, SpecExpected);
}

TEST_F(EncodeCoordinator_cTest, GapAudioVideoX2bis) {

    unsigned int SpecAudioBuffer[] = {0, 5, 10, 15, 120, 125, 130, 135 | EOSMARKER};
    unsigned int SpecVideoBuffer[] = {0, 140, 160, 180 | EOSMARKER};

    spec_t SpecExpected[]   = { A(0), V(0), A(5), A(10), FADEOUT(15),  R__(0, 120, 20), FADEIN_(120, 20), A_(125, 25), A_(130, 30), AEOS , NEWGOP_(140, 40) , V_(160, 60), VEOS, END};
    runtest(SpecAudioBuffer, 5, SpecVideoBuffer, 20, SpecExpected);
}

TEST_F(EncodeCoordinator_cTest, GapAudioVideoX3bis) {

    unsigned int SpecAudioBuffer[] = {0, 5, 10, 15, 120, 125, 130, 135 | EOSMARKER};
    unsigned int SpecVideoBuffer[] = {0, 160, 180, 200 | EOSMARKER};

    spec_t SpecExpected[]   = { A(0), V(0), A(5), A(10), FADEOUT(15), R__(0, 120, 20), FADEIN_(120, 20), A_(125, 25), A_(130, 30), AEOS , R__(0, 140, 40) , NEWGOP_(160, 60), V_(180, 80), VEOS, END};
    runtest(SpecAudioBuffer, 5, SpecVideoBuffer, 20, SpecExpected);
}

TEST_F(EncodeCoordinator_cTest, GapAudioNegative) {

    unsigned int SpecAudioBuffer[] = {1100, 1105, 1110, 1115 , 20 , 25 , 30  | EOSMARKER};
    unsigned int SpecVideoBuffer[] = {1100, 1120 , 20 , 40 | EOSMARKER};

    spec_t SpecExpected[]   = { A(1100), V(1100), A(1105), A(1110), FADEOUT(1115), M_(1115, 1120), V(1120), FADEIN_(20, 1140), NEWGOP_(20, 1140) , A_(25, 1145) , AEOS , VEOS , END };
    runtest(SpecAudioBuffer, 5, SpecVideoBuffer, 20, SpecExpected);
}







/*
                             |                               |
                    FadeOut  |              FadeIn           |       FadeOut                                           |
A) A(0)--A(5)--A(10)--FO(15) |M(20)-- M(25)-- A(30)-- A(35)--|A(40)--FO(45)--M(50)--M(55)--M(60)--M(65)--M(70)--M(75)--|M(80)--M(85)-FI(90)A(95)
                             |                               |                                                         |
B) V(0)--------------------- |R(20)                          |V(40)------------------------ R(60)                      |V(80)------------------------
                             |Repeat(0)                      |  NewGop(40,40)                                          |
                             |                               |                                                         |
*/


TEST_F(EncodeCoordinator_cTest, GapAudioVideoX3) {

    unsigned int SpecAudioBuffer[] = {0, 5, 10, 15, 30, 35, 40, 45, 90, 95, 100 | EOSMARKER};
    unsigned int SpecVideoBuffer[] = {0, 40, 60, 80, 100 | EOSMARKER};

    spec_t SpecExpected[]   = { A(0), V(0), A(5), A(10), FADEOUT(15), M_(15, 20), R_(0, 20), M_(15, 25), FADEIN_(30, 30), A(35),
                                A(40), V(40), FADEOUT(45), M_(45, 50), M_(45, 55), M_(45, 60), V(60), M_(45, 65), M_(45, 70), M_(45, 75), M_(45, 80), V(80) , M_(45, 85), FADEIN(90), A(95), AEOS , VEOS , END
                              };
    runtest(SpecAudioBuffer, 5, SpecVideoBuffer, 20, SpecExpected);
}


// Now dealing with TimeOut
TEST_F(EncodeCoordinator_cTest, SimpleTimeOutAudio) {

    unsigned int SpecAudioBuffer[] = {0, WAIT(20), 40, 45 | EOSMARKER};
    unsigned int SpecVideoBuffer[] = {0, 20 , 40, 60 | EOSMARKER};

    spec_t SpecExpected[]   = { FADEOUT(0), V(0), M_(0, 5), M_(0, 10), M_(0, 15), M_(0, 20), V(20), M_(0, 25), M_(0, 30), M_(0, 35), FADEIN(40) , V(40), AEOS, VEOS ,  END};
    runtest(SpecAudioBuffer, 5, SpecVideoBuffer, 20, SpecExpected);
}


// Now dealing with TimeOut
TEST_F(EncodeCoordinator_cTest, TimeOutAudio) {

    // unsigned int SpecAudioBuffer[]= {0,5,10,WAIT(20),15,35,40, 45 | EOSMARKER};
    unsigned int SpecAudioBuffer[] = {0, 5, 10, 15, 35, 40, 45 | EOSMARKER};
    unsigned int SpecVideoBuffer[] = {0, 20 , 40, 60 | EOSMARKER};
    spec_t SpecExpected[]   = { A(0), V(0), A(5), A(10), FADEOUT(15), M_(15, 20), V(20), M_(15, 25), M_(15, 30), FADEIN(35), A(40), V(40), AEOS , VEOS, END};
    runtest(SpecAudioBuffer, 5, SpecVideoBuffer, 20, SpecExpected);
}

