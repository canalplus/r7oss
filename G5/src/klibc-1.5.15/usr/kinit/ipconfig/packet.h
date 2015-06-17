#ifndef IPCONFIG_PACKET_H
#define IPCONFIG_PACKET_H

int packet_open(void);
void packet_close(void);
int packet_send(struct netdev *dev, struct iovec *iov, int iov_len);
int packet_peek(int *ifindex);
void packet_discard(void);
int packet_recv(struct iovec *iov, int iov_len);

#endif /* IPCONFIG_PACKET_H */
