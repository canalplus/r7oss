#ifndef BOOTP_PACKET_H
#define BOOTP_PACKET_H

#include <sys/uio.h>

struct netdev;

/* packet ops */
#define BOOTP_REQUEST	1
#define BOOTP_REPLY	2

/* your basic bootp packet */
struct bootp_hdr {
	uint8_t	 op;
	uint8_t	 htype;
	uint8_t	 hlen;
	uint8_t	 hops;
	uint32_t xid;
	uint16_t secs;
	uint16_t flags;
	uint32_t ciaddr;
	uint32_t yiaddr;
	uint32_t siaddr;
	uint32_t giaddr;
	uint8_t	 chaddr[16];
	char	 server_name[64];
	char	 boot_file[128];
	/* 312 bytes of extensions */
};

#endif
