#include "gtest/gtest.h"
#include "gmock/gmock.h"

#include "relay_interface.h"

RelayInterface *gRelay;

void useRelayInterface(RelayInterface *relayInterface) {
    gRelay = relayInterface;
}

void st_relayfs_write_se(unsigned int id, unsigned int source, unsigned char *buf, unsigned int len, void *info) {
    gRelay->st_relayfs_write_se(id, source, buf, len, info);
}

