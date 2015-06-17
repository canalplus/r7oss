#include "gtest/gtest.h"
#include "gmock/gmock.h"
#include "encoder_generic.h"
#include "buffer.h"

#include "mock_buffer.h"

using ::testing::_;
using ::testing::DoAll;
using ::testing::SetArgPointee;
using ::testing::Return;

#define MAX_COPY_BUFFER 10


class MockEncoder_c : public Encoder_c {
  public:
    MockEncoder_c();
    virtual ~MockEncoder_c();

    MOCK_METHOD1(CreateEncode,
                 EncoderStatus_t(Encode_t *Encode));
    MOCK_METHOD1(TerminateEncode,
                 EncoderStatus_t(Encode_t Encode));
    MOCK_METHOD4(CreateEncodeStream,
                 EncoderStatus_t(EncodeStream_t *EncodeStream, Encode_t Encode, stm_se_encode_stream_media_t Media, stm_se_encode_stream_encoding_t Encoding));
    MOCK_METHOD2(TerminateEncodeStream,
                 EncoderStatus_t(Encode_t Encode, EncodeStream_t EncodeStream));
#if 0
    MOCK_METHOD5(CallInSequence,
                 EncoderStatus_t(EncodeStream_t Stream, EncodeSequenceType_t Type, EncodeSequenceValue_t Value, EncodeComponentFunction_t Fn, ...));
#endif
    virtual EncoderStatus_t   CallInSequence(EncodeStream_t            Stream,
                                             EncodeSequenceType_t      Type,
                                             EncodeSequenceValue_t     Value,
                                             EncodeComponentFunction_t Fn,
                                             ...) { return EncoderNoError; }

    MOCK_METHOD1(RegisterBufferManager,
                 EncoderStatus_t(BufferManager_t BufferManager));
    MOCK_METHOD1(RegisterScaler,
                 EncoderStatus_t(Scaler_t Scaler));
    MOCK_METHOD1(RegisterBlitter,
                 EncoderStatus_t(Blitter_t Blitter));
    MOCK_METHOD1(GetBufferManager,
                 EncoderStatus_t(BufferManager_t *BufferManager));
    MOCK_METHOD0(GetBufferTypes,
                 const EncoderBufferTypes * (void));
    MOCK_METHOD6(GetClassList,
                 EncoderStatus_t(EncodeStream_t Stream, Encode_t *Encode, Preproc_t *Preproc, Coder_t *Coder, Transporter_t *Transporter, EncodeCoordinator_t *EncodeCoordinator));
    MOCK_METHOD1(GetStatistics,
                 EncodeStreamStatistics_t(EncodeStream_t Stream));
    MOCK_METHOD1(ResetStatistics,
                 void(EncodeStream_t Stream));
#if 0
    MOCK_METHOD1(GetInputBuffer,
                 EncoderStatus_t(Buffer_t *Buffer));
#endif
    EncoderStatus_t    GetInputBuffer(Buffer_t *Buffer, bool NonBlocking = false);

    MOCK_METHOD1(LowPowerEnter,
                 EncoderStatus_t(__stm_se_low_power_mode_t low_power_mode));
    MOCK_METHOD0(LowPowerExit,
                 EncoderStatus_t(void));
    MOCK_METHOD0(GetLowPowerMode,
                 __stm_se_low_power_mode_t(void));

    EncoderBufferTypes mBufferTypes;

  private:
    MockBuffer_c *mMockBuffer[MAX_COPY_BUFFER];
    unsigned int    mMockBufferIndex;
};
