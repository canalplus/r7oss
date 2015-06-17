#ifndef _ENCODE_COORDINATOR_CTEST_H
#define _ENCODE_COORDINATOR_CTEST_H

#include "gtest/gtest.h"
#include "gmock/gmock.h"

#include "encode_coordinator.h"
#include "mock_encode_stream.h"
#include "mock_port.h"
#include "mock_encoder.h"
#include "mock_release_buffer_interface.h"
#include "mock_buffer.h"
#include "mock_time.h"

using ::testing::_;
using ::testing::DoAll;
using ::testing::SetArgPointee;
using ::testing::SetArrayArgument;
using ::testing::Return;
using ::testing::InSequence;
using ::testing::AnyNumber;
using ::testing::ReturnPointee;


#define AUDIOFRAMEDURATION  40000
#define VIDEOFRAMEDURATION  33000
#define MAXTESTLEN  80


// Macros used to specify the input feature of the Buffers
#define WAITMARKER  0x80000000
#define EOSMARKER   0x40000000
#define PTSMASK     0x0FFFFFFF
#define EOS(a)      ((a) & EOSMARKER)
#define PTS(a)      (a & PTSMASK)
#define WAIT(a)     ((a) | WAITMARKER)
#define ISATEMPO(a) ((a) & WAITMARKER)
#define TEMPO(a)    ((a) & ~WAITMARKER)




enum {
    AUDIO_NORMAL = STM_SE_ENCODE_STREAM_MEDIA_AUDIO,
    VIDEO_NORMAL = STM_SE_ENCODE_STREAM_MEDIA_VIDEO,
    AUDIO_FADEIN,
    AUDIO_FADEOUT,
    AUDIO_MUTED,
    VIDEO_NEWGOP,
    VIDEO_REPEAT,
    END_MEDIA_TYPE,
    AUDIO_EOS,
    VIDEO_EOS,
    THE_END
};


#define NOT_THE_END(a)  ((a).mMediaType!=THE_END)



#define FADEIN(pts)         Spec_cTest(AUDIO_FADEIN,(unsigned int)pts,(unsigned int)pts)
#define FADEIN_(pts,remap ) Spec_cTest(AUDIO_FADEIN,(unsigned int)pts,(unsigned int)remap)

#define FADEOUT(pts)        Spec_cTest(AUDIO_FADEOUT,(unsigned int)pts,(unsigned int)pts)
#define FADEOUT_(pts,remap)     Spec_cTest(AUDIO_FADEOUT,(unsigned int)pts,(unsigned int)remap)


#define NEWGOP(pts)         Spec_cTest(VIDEO_NEWGOP,(unsigned int)pts,(unsigned int)pts)
#define NEWGOP_(pts,remap)  Spec_cTest(VIDEO_NEWGOP,(unsigned int)pts,(unsigned int)remap)


#define A(a)    Spec_cTest(AUDIO_NORMAL,(unsigned int) a,(unsigned int) a)
#define V(a)    Spec_cTest(VIDEO_NORMAL,(unsigned int) a,(unsigned int) a)
#define A_(a,b) Spec_cTest(AUDIO_NORMAL,(unsigned int) a,(unsigned int) b)
#define V_(a,b) Spec_cTest(VIDEO_NORMAL,(unsigned int) a,(unsigned int) b)

// muted frame
#define M(a)    Spec_cTest(AUDIO_MUTED,(unsigned int)a,(unsigned int)a,(unsigned int)a)
#define M_(a,b) Spec_cTest(AUDIO_MUTED,(unsigned int)a,(unsigned int)b,(unsigned int)b)

// Repeated frame
#define R(a)    Spec_cTest(VIDEO_REPEAT,(unsigned int) a,(unsigned int)a)
#define R_(a,b) Spec_cTest(VIDEO_REPEAT,(unsigned int) a,(unsigned int)b,(unsigned int)b)
#define R__(a,r,b) Spec_cTest(VIDEO_REPEAT,(unsigned int) a,(unsigned int) r ,(unsigned int)b)

// EOS
#define AEOS Spec_cTest(AUDIO_EOS,0u,0u)
#define VEOS Spec_cTest(VIDEO_EOS,0u,0u)

// marker
#define END Spec_cTest(THE_END,0u,0u)


#define VIDEO(a)    (  ((a).mMediaType ==VIDEO_NORMAL) || ((a).mMediaType ==VIDEO_NEWGOP) ||  ((a).mMediaType ==VIDEO_REPEAT) )

#define EXPECTED_PTS(a)     ((a).mPTS)
#define EXPECTED_REMAP_PTS(a)   ((a).mRemapPTS)

class Spec_cTest {
  public:
    // Misc
    unsigned int     mMediaType;
    unsigned int     mPTS;
    unsigned int     mPTSrepeat;
    unsigned int     mRemapPTS;
    bool             mFadeIn;
    bool             mFadeOut;
    bool             mNewGop;

    explicit    Spec_cTest(unsigned int mediatype , unsigned int pts, unsigned int remap_pts)

        : mMediaType(mediatype), mPTS(pts), mPTSrepeat(remap_pts), mRemapPTS(remap_pts) {

    };

    explicit    Spec_cTest(unsigned int mediatype , unsigned int pts, unsigned int ptsrepeat, unsigned int remap_pts)

        : mMediaType(mediatype), mPTS(pts), mPTSrepeat(ptsrepeat), mRemapPTS(remap_pts) {

    };


};
typedef Spec_cTest  spec_t;



// fixture for all encode_coordinator unit tests
class EncodeCoordinator_cTest: public ::testing::Test {
  protected:
    EncodeCoordinator_cTest()
        : mEncodeCoordinatorAudioPort(NULL),
          mEncodeCoordinatorVideoPort(NULL),
          mTime(0),
          mAudioFirstPTS(0),
          mAudioFrameDuration(32000),
          mAudioInsertCount(0),
          mVideoFirstPTS(0),
          mVideoFrameDuration(20000),
          mVideoInsertCount(0) {

    }
    void CreateExpectedRelease(MockBuffer_c  *InputBuf[], unsigned int n,
                               MockBuffer_c  *ExpectedBuf[], unsigned int nout);


    RingStatus_t MyReleaseBuffer(uintptr_t arg);

    void runtest(unsigned int SpecAudioBuffer[], unsigned int Aduration,
                 unsigned int SpecVideoBuffer[], unsigned int Vduration,
                 spec_t SpecExpected[]);

    void defaultExpectations() {
        EXPECT_CALL(mMockAudioEncodeStream, GetInputPort())
        .WillRepeatedly(Return(&mMockAudioVideoStreamInputPort));


        EXPECT_CALL(mMockAudioEncodeStream, GetEncoderObject())
        .WillRepeatedly(Return(mMockEncoder));


        EXPECT_CALL(mMockVideoEncodeStream, GetInputPort())
        .WillRepeatedly(Return(&mMockAudioVideoStreamInputPort));

        EXPECT_CALL(mMockVideoEncodeStream, GetEncoderObject())
        .WillRepeatedly(Return(mMockEncoder));

        // We want to mock calls to GetTime in these tests
        useTimeInterface(&mMockTime);

        // for the purpose of testing the time mapping algorithm
        // we will assume that time does not change
        EXPECT_CALL(mMockTime, GetTimeInMicroSeconds())
        .WillRepeatedly(ReturnPointee(&mTime));
    }

    virtual void SetUp() {
        mMockEncoder = new MockEncoder_c();

        mEncodeCoordinator = new EncodeCoordinator_c();
        defaultExpectations();

        PlayerStatus_t status = mEncodeCoordinator->FinalizeInit();
        EXPECT_EQ(PlayerNoError, status);

        mInputMetaDataBufferType = mMockEncoder->GetBufferTypes()->InputMetaDataBufferType;

        status = mEncodeCoordinator->Connect(&mMockAudioEncodeStream, &mMockReleaseBufferInterface, &mEncodeCoordinatorStreamAudioReleaseBufferInterface, &mEncodeCoordinatorAudioPort);
        EXPECT_EQ(PlayerNoError, status);
        ASSERT_NE((void *) 0, mEncodeCoordinatorAudioPort);

        status = mEncodeCoordinator->Connect(&mMockVideoEncodeStream, &mMockReleaseBufferInterface, &mEncodeCoordinatorStreamVideoReleaseBufferInterface, &mEncodeCoordinatorVideoPort);
        EXPECT_EQ(PlayerNoError, status);
        ASSERT_NE((void *) 0, mEncodeCoordinatorVideoPort);
    }

    virtual void TearDown() {
        PlayerStatus_t status = mEncodeCoordinator->Disconnect(&mMockAudioEncodeStream);
        EXPECT_EQ(PlayerNoError, status);

        status = mEncodeCoordinator->Disconnect(&mMockVideoEncodeStream);
        EXPECT_EQ(PlayerNoError, status);

        mEncodeCoordinator->Halt();

        delete mEncodeCoordinator;
        delete mMockEncoder;

        // restart time
        mTime = 0;
        useTimeInterface(NULL);
    }

    // Class Under Test
    EncodeCoordinator_c     *mEncodeCoordinator;

    // Encode Coordinator input ports
    Port_c                 *mEncodeCoordinatorAudioPort;
    Port_c                 *mEncodeCoordinatorVideoPort;

    // Mocks
    MockTime                        mMockTime;
    MockEncoder_c                   *mMockEncoder;
    MockEncodeStream_c              mMockAudioEncodeStream;
    MockEncodeStream_c              mMockVideoEncodeStream;
    MockPort_c                      mMockAudioVideoStreamInputPort;
    MockReleaseBufferInterface_c    mMockReleaseBufferInterface;
    ReleaseBufferInterface_c       *mEncodeCoordinatorStreamVideoReleaseBufferInterface;
    ReleaseBufferInterface_c       *mEncodeCoordinatorStreamAudioReleaseBufferInterface;


    // Misc
    unsigned long long      mTime;
    unsigned long long      mAudioFirstPTS;
    int                     mAudioFrameDuration;
    int                     mAudioInsertCount;
    unsigned long long      mVideoFirstPTS;
    int                     mVideoFrameDuration;
    int                     mVideoInsertCount;

    BufferType_t        mInputMetaDataBufferType;

    unsigned long long      AudioNextTimeStamp() { return mAudioFirstPTS + (mAudioInsertCount++ * mAudioFrameDuration); }
    unsigned long long      VideoNextTimeStamp() { return mVideoFirstPTS + (mVideoInsertCount++ * mVideoFrameDuration); }
};

#endif // _ENCODE_COORDINATOR_CTEST_H
