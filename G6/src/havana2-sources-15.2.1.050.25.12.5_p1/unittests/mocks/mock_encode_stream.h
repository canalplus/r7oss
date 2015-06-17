#include "encode_stream_interface.h"

class MockEncodeStream_c : public EncodeStreamInterface_c {
  public:
    MOCK_METHOD0(GetInputPort, Port_c * (void));
    MOCK_METHOD0(GetEncoderObject, Encoder_c * (void));
};
