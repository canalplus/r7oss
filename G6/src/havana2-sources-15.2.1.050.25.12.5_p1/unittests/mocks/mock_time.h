#ifndef _MOCK_TIME_H
#define _MOCK_TIME_H

#include "gmock/gmock.h"
#include "time_interface.h"

class MockTime: public TimeInterface {
  public:
    MockTime();
    virtual ~MockTime();

    MOCK_METHOD0(GetTimeInMicroSeconds, unsigned long long(void));
};
#endif

