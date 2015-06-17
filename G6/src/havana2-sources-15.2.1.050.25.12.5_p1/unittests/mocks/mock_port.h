#ifndef _MOCK_PORT_H_
#define _MOCK_PORT_H_

#include "gmock/gmock.h"
#include "port.h"

class MockPort_c : public Port_c {
  public:
    MockPort_c();
    virtual ~MockPort_c();

    MOCK_METHOD1(Insert,
                 RingStatus_t(uintptr_t Value));
    MOCK_METHOD1(InsertFromIRQ,
                 RingStatus_t(uintptr_t Value));
    MOCK_METHOD2(Extract,
                 RingStatus_t(uintptr_t *Value, unsigned int BlockingPeriod));
    MOCK_METHOD0(Flush,
                 RingStatus_t(void));
    MOCK_METHOD0(NonEmpty,
                 bool(void));
};


#endif
