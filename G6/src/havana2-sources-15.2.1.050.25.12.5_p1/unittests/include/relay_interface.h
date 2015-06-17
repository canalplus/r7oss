#ifndef __RELAY_INTERFACE_H
#define __RELAY_INTERFACE_H

class RelayInterface {
  public:
    virtual void st_relayfs_write_se(unsigned int id, unsigned int source, unsigned char *buf, unsigned int len, void *info) = 0;
};

void useRelayInterface(RelayInterface *relayInterface);

#endif
