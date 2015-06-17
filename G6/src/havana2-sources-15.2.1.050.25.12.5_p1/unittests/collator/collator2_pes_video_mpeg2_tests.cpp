#include <assert.h>
#include "gtest/gtest.h"
#include "collator_ctest.h"
#include "collator2_pes_video_mpeg2.h"

using ::testing::Eq;
using ::testing::Invoke;
using ::testing::InvokeWithoutArgs;
using ::testing::NotNull;
using ::testing::WithArg;

/*
 * Class helping to mock the buffer management behaviour expected by collator2.
 */
class RotatingTestBuffer_c {
  public:
    RotatingTestBuffer_c(char *poolBase, size_t poolSize);
    void Partition(unsigned int offset);
    void Rewind();
    void GetSize(unsigned int *size) const;
    void GetBase(void **base) const;

  private:
    char         *mPoolBase;
    unsigned int  mPoolSize;
    char         *mBufferBase;
    unsigned int  mBufferSize;
};

class Collator2_PesVideoMpeg2_cTest: public Collator_cTest {
  protected:
    Collator2_PesVideoMpeg2_cTest();

    virtual void SetDefaultExpectations();

    Collator2_PesVideoMpeg2_c mCollator_PesVideoMpeg2;
    RotatingTestBuffer_c      mRotatingTestBuffer;
};

class Collator2_PesVideoMpeg2SplicingMarkerInsertionTest
    : public Collator2_PesVideoMpeg2_cTest,
      public ::testing::WithParamInterface<MarkerInsertionParams> {
};

//////////////////////////////////////////////////////////////////////////////

// The buffer initially spans the whole pool.
RotatingTestBuffer_c::RotatingTestBuffer_c(char *poolBase, size_t poolSize)
    : mPoolBase(poolBase), mPoolSize(poolSize), mBufferBase(poolBase), mBufferSize(poolSize) {
}

// Partition buffer in two parts.  This buffer becomes the right part.
void RotatingTestBuffer_c::Partition(unsigned int leftSize) {
    mBufferBase += leftSize;
    assert(mBufferBase - mPoolBase < mPoolSize);
    mBufferSize -= leftSize;
    assert(mBufferSize > 0);
}

// Move back the buffer to the beginning of the pool and make it span the whole pool.
void RotatingTestBuffer_c::Rewind() {
    mBufferBase = mPoolBase;
    mBufferSize = mPoolSize;
}

void RotatingTestBuffer_c::GetBase(void **base) const {
    *base = mBufferBase;
}

void RotatingTestBuffer_c::GetSize(unsigned int *size) const {
    *size = mBufferSize;
}

//////////////////////////////////////////////////////////////////////////////

Collator2_PesVideoMpeg2_cTest::Collator2_PesVideoMpeg2_cTest()
    : Collator_cTest(mCollator_PesVideoMpeg2, StreamTypeVideo),
      mRotatingTestBuffer(mCodedFrameBufferChunk, sizeof(mCodedFrameBufferChunk)) {
}

void Collator2_PesVideoMpeg2_cTest::SetDefaultExpectations() {
    Collator_cTest::SetDefaultExpectations();

    EXPECT_CALL(mMockCodedFrameBuffer, ObtainDataReference(NULL, NULL, _, _))
    .WillRepeatedly(DoAll(
                        WithArg<2>(Invoke(&mRotatingTestBuffer, &RotatingTestBuffer_c::GetBase)),
                        Return((int) BufferNoError)));

    EXPECT_CALL(mMockCodedFrameBuffer, ObtainDataReference(NotNull(), NULL, NotNull(), _))
    .WillRepeatedly(DoAll(
                        WithArg<2>(Invoke(&mRotatingTestBuffer, &RotatingTestBuffer_c::GetBase)),
                        WithArg<0>(Invoke(&mRotatingTestBuffer, &RotatingTestBuffer_c::GetSize)),
                        Return((int) BufferNoError)));

    EXPECT_CALL(mMockCodedFrameBuffer, PartitionBuffer(_, _, NotNull(), _, _))
    .WillRepeatedly(DoAll(
                        WithArg<1>(Invoke(&mRotatingTestBuffer, &RotatingTestBuffer_c::Partition)),
                        SetArgPointee<2>(&mMockCodedFrameBuffer),
                        Return(BufferNoError)));

    EXPECT_CALL(mMockCodedFrameBufferPool, GetBuffer(NotNull(), _, _, _, _, _))
    .WillRepeatedly(DoAll(
                        InvokeWithoutArgs(&mRotatingTestBuffer, &RotatingTestBuffer_c::Rewind),
                        SetArgPointee<0>(&mMockCodedFrameBuffer),
                        Return((int) BufferNoError)));

    EXPECT_CALL(mMockCodedFrameBufferPool, GetPoolUsage(NotNull(), NotNull(), _, _, _, _))
    .WillRepeatedly(DoAll(
                        SetArgPointee<0>(1),
                        SetArgPointee<0>(0)
                    ));

    EXPECT_CALL(mMockPlayer, GetPlaybackSpeed(_, _, NotNull()))
    .WillRepeatedly(DoAll(
                        SetArgPointee<2>(PlayForward),
                        Return(PlayerNoError)));

    EXPECT_CALL(mMockStream.mParser, PresentCollatedHeader(_, _, NotNull()))
    .WillRepeatedly(DoAll(
                        SetArgPointee<2>(FrameParserHeaderFlagPartitionPoint),
                        Return(FrameParserNoError)));
}

TEST_F(Collator2_PesVideoMpeg2_cTest, EmptyInput) {
    PlayerInputDescriptor_t  inputDescriptor;
    inputDescriptor.UnMuxedStream           = &mMockStream;
    inputDescriptor.PlaybackTimeValid       = false;
    inputDescriptor.DecodeTimeValid         = false;
    inputDescriptor.DataSpecificFlags       = 0;

    EXPECT_CALL(mMockPort, Insert(_)).Times(0);

    unsigned int remaining = ~0;
    PlayerStatus_t status = mCollator.Input(&inputDescriptor, 0, NULL, true, &remaining);
    EXPECT_THAT(status, PlayerNoError);
    EXPECT_THAT(remaining, 0);
}

//////////////////////////////////////////////////////////////////////////////

TEST_P(Collator2_PesVideoMpeg2SplicingMarkerInsertionTest, SplicingMarkerInsertionTest) {

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
    STREAMTESTSPLMK,
    1024,
    NO_MARKER,
    PES_CONTROL_BRK_SPLICING,
    132
};

INSTANTIATE_TEST_CASE_P(
    Collator2_PesVideoMpeg2SplicingMarkerInsertionTests,
    Collator2_PesVideoMpeg2SplicingMarkerInsertionTest,
    ::testing::Values(splicing_test0));
