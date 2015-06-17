#include "mock_encoder.h"
#include "mock_buffer.h"


MockEncoder_c::MockEncoder_c() {
    mBufferTypes.InputMetaDataBufferType = (int) METADATA_ENCODE_FRAME_PARAMETERS;
    EXPECT_CALL(*this, GetBufferTypes())
    .WillRepeatedly(Return(&mBufferTypes));

    for (unsigned int i = 0; i < MAX_COPY_BUFFER; i++) {
        mMockBuffer[i] = new MockBuffer_c(*this, 0);
    }
    mMockBufferIndex = 0;

}


EncoderStatus_t MockEncoder_c::GetInputBuffer(Buffer_t *Buffer, bool NonBlocking) {
    *Buffer = mMockBuffer[mMockBufferIndex++];
    if (mMockBufferIndex == MAX_COPY_BUFFER) { mMockBufferIndex = 0; }
    return ((EncoderStatus_t) EncoderNoError);

}

MockEncoder_c::~MockEncoder_c() {
    for (unsigned int i = 0; i < MAX_COPY_BUFFER; i++) {
        delete mMockBuffer[i] ;
    }
}




