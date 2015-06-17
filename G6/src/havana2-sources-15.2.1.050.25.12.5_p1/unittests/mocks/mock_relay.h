#ifndef _MOCK_RELAY_H
#define _MOCK_RELAY_H

#include "gmock/gmock.h"
#include "relay_interface.h"

class MockRelay_c : public RelayInterface {
  public:
    MockRelay_c();
    virtual ~MockRelay_c();

    MOCK_METHOD5(st_relayfs_write_se,
                 void(unsigned int id, unsigned int source, unsigned char *buf, unsigned int len, void *info));
};

#endif

