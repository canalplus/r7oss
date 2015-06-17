#include "mock_buffer.h"

#define MAX_BUFFERS 32

unsigned int MockBuffer_c::gBufferIndex = 0;


MockBuffer_c::MockBuffer_c(const Player_c &Player) : mIndex(gBufferIndex++) {
    EXPECT_GT(MAX_BUFFERS, gBufferIndex);

    EXPECT_CALL(*this, GetIndex(_))
    .WillRepeatedly(SetArgPointee<0>(mIndex));

    EXPECT_CALL(*this, ObtainMetaDataReference(Player.MetaDataCodedFrameParametersType, _))
    .WillRepeatedly(SetArgPointee<1>(&mCodedFrameParams));

    EXPECT_CALL(*this, ObtainMetaDataReference(Player.MetaDataParsedFrameParametersReferenceType, _))
    .WillRepeatedly(SetArgPointee<1>(&mParsedFrameParams));

    EXPECT_CALL(*this, ObtainMetaDataReference(Player.MetaDataSequenceNumberType, _))
    .WillRepeatedly(SetArgPointee<1>(&mSequenceNumber));

    EXPECT_CALL(*this, ObtainMetaDataReference(Player.MetaDataParsedVideoParametersType, _))
    .WillRepeatedly(SetArgPointee<1>(&mVideoParams));

    EXPECT_CALL(*this, ObtainMetaDataReference(Player.MetaDataStartCodeListType, _))
    .WillRepeatedly(SetArgPointee<1>(&mStartCodeList));
}

MockBuffer_c::MockBuffer_c(Encoder_c &Encoder, unsigned int bufferLenght) :  mDataLenght(bufferLenght), mIndex(gBufferIndex++) {
    EXPECT_GT(MAX_BUFFERS, gBufferIndex);

    EXPECT_CALL(*this, GetIndex(_))
    .WillRepeatedly(SetArgPointee<0>(mIndex));


    EXPECT_CALL(*this, ObtainMetaDataReference(Encoder.GetBufferTypes()->InputMetaDataBufferType, _))
    .WillRepeatedly(SetArgPointee<1>(&mUncompressedFrameMetaData));

    EXPECT_CALL(*this, ObtainMetaDataReference(Encoder.GetBufferTypes()->EncodeCoordinatorMetaDataBufferType, _))
    .WillRepeatedly(SetArgPointee<1>(&mEncodeCoordinatorFrameMetaData));

    EXPECT_CALL(*this, ObtainDataReference(NULL, _, _, _))
    .WillRepeatedly(DoAll(
                        SetArgPointee<1>(mDataLenght),
                        SetArgPointee<2>((void *)mInputBufferAddr),
                        Return((BufferStatus_t) BufferNoError)));


    EXPECT_CALL(*this, ObtainDataReference(NULL, _, NULL, _))
    .WillRepeatedly(DoAll(
                        SetArgPointee<1>(mDataLenght),
                        Return((BufferStatus_t) BufferNoError)));


    EXPECT_CALL(*this, RegisterDataReference(_, _))
    .WillRepeatedly(
        Return((BufferStatus_t) BufferNoError));

    EXPECT_CALL(*this, SetUsedDataSize(_))
    .WillRepeatedly(
        Return((BufferStatus_t) BufferNoError));

    EXPECT_CALL(*this, DecrementReferenceCount(_))
    .WillRepeatedly(Return());

    //  printf("Alloc A buffer %d\n",mIndex);
}

MockBuffer_c::~MockBuffer_c() {
    EXPECT_LT(0, gBufferIndex);

    gBufferIndex--;
}
