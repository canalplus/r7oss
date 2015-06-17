#ifndef _BUFFER_H_
#define _BUFFER_H_

#include "gtest/gtest.h"
#include "gmock/gmock.h"
#include "player_generic.h"
#include "player_playback.h"
#include "player_stream.h"
#include "encoder.h"
#include "buffer.h"

using ::testing::_;
using ::testing::DoAll;
using ::testing::SetArgPointee;
using ::testing::Return;

class MockBuffer_c : public Buffer_c {
  public:
    explicit MockBuffer_c(const Player_c &Player);
    explicit MockBuffer_c(Encoder_c &Encoder, unsigned int bufferLenght = 88888);
    virtual ~MockBuffer_c();

    MOCK_METHOD4(AttachMetaData,
                 BufferStatus_t(MetaDataType_t Type, unsigned int Size, void *MemoryBlock, char *DeviceMemoryPartitionName));

    MOCK_METHOD1(DetachMetaData,
                 void(MetaDataType_t Type));

    MOCK_METHOD2(ObtainMetaDataReference,
                 void(MetaDataType_t Type, void **Pointer));

    MOCK_METHOD1(SetUsedDataSize,
                 BufferStatus_t(unsigned int DataSize));

    MOCK_METHOD1(ShrinkBuffer,
                 BufferStatus_t(unsigned int NewSize));

    MOCK_METHOD2(ExtendBuffer,
                 BufferStatus_t(unsigned int *NewSize, bool Upwards));

    MOCK_METHOD5(PartitionBuffer,
                 BufferStatus_t(unsigned int LeaveInFirstPartitionSize, bool DuplicateMetaData,
                                Buffer_t *SecondPartition, unsigned int SecondOwnerIdentifier, bool NonBlocking));

    MOCK_METHOD3(RegisterDataReference,
                 BufferStatus_t(unsigned int BlockSize, void *Pointer, AddressType_t AddressType));

    MOCK_METHOD2(RegisterDataReference,
                 BufferStatus_t(unsigned int BlockSize, void **Pointers));

    MOCK_METHOD4(ObtainDataReference,
                 BufferStatus_t(unsigned int *BlockSize, unsigned int *UsedDataSize, void **Pointer, AddressType_t AddressType));

    MOCK_METHOD2(TransferOwnership,
                 void(unsigned int OwnerIdentifier0, unsigned int OwnerIdentifier1));

    MOCK_METHOD1(IncrementReferenceCount,
                 void(unsigned int OwnerIdentifier));

    MOCK_METHOD1(DecrementReferenceCount,
                 void(unsigned int OwnerIdentifier));

    MOCK_METHOD1(AttachBuffer,
                 BufferStatus_t(Buffer_t Buffer));

    MOCK_METHOD1(DetachBuffer,
                 void(Buffer_t Buffer));

    MOCK_METHOD2(ObtainAttachedBufferReference,
                 void(BufferType_t Type, Buffer_t *Buffer));

    MOCK_METHOD1(GetType,
                 void(BufferType_t *Type));

    MOCK_METHOD1(GetIndex,
                 void(unsigned int *Index));

    MOCK_METHOD1(GetOwnerCount,
                 void(unsigned int *Count));

    MOCK_METHOD2(GetOwnerList,
                 void(unsigned int ArraySize, unsigned int *ArrayOfOwnerIdentifiers));

    MOCK_METHOD1(Dump,
                 void(unsigned int Flags));

    MOCK_METHOD2(DumpViaRelay,
                 void(unsigned int id, unsigned int type));


    unsigned int GetDuration(void) { return mDataLenght; };

    // Some methods useful in testing
    // To ease testing, let's have member data corresponding to every kind of metadata that can be attached
    // to a buffer. The members are public so that it is easy in tests to fill specific fields of some metadata
    // struct. Then in the constructor, some default expectations are used to return the correct struct
    CodedFrameParameters_t  mCodedFrameParams;
    ParsedFrameParameters_t mParsedFrameParams;
    PlayerSequenceNumber_t  mSequenceNumber;
    ParsedVideoParameters_t mVideoParams;
    StartCodeList_t         mStartCodeList;
    stm_se_uncompressed_frame_metadata_t    mUncompressedFrameMetaData;
    __stm_se_encode_coordinator_metadata_t  mEncodeCoordinatorFrameMetaData;
    unsigned int            mDataLenght;
    void                   *mInputBufferAddr[3];

    // needed for internal test transcode coordinator
    unsigned long long              injection_time;

    void AttachBufferReference(Buffer_t buffer) {
        EXPECT_CALL(*this, ObtainAttachedBufferReference(_, _))
        .WillRepeatedly(SetArgPointee<1>(buffer));
    }

  private:
    unsigned int mIndex;

    static unsigned int gBufferIndex;
};

#endif
