#ifndef H_MOCK_RELEASE_BUFFER_INTERFACE
#define H_MOCK_RELEASE_BUFFER_INTERFACE

#include "gmock/gmock.h"
#include "release_buffer_interface.h"

class MockReleaseBufferInterface_c : public ReleaseBufferInterface_c {
  public:
    MockReleaseBufferInterface_c();
    virtual ~MockReleaseBufferInterface_c();

    MOCK_METHOD1(ReleaseBuffer, PlayerStatus_t(Buffer_t buffer));
};

#endif // H_MOCK_RELEASE_BUFFER_INTERFACE
