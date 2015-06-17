/*
 * DHCP RFC 2131 and 2132
 */
#include <sys/types.h>
#include <sys/uio.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "ipconfig.h"
#include "netdev.h"
#include "bootp_packet.h"
#include "bootp_proto.h"
#include "dhcp_proto.h"
#include "packet.h"

static uint8_t dhcp_params[] = {
	1,			/* subnet mask */
	3,			/* default gateway */
	6,			/* DNS server */
	12,			/* host name */
	15,			/* domain name */
	17,			/* root path */
	26,			/* interface mtu */
	28,			/* broadcast addr */
	40,			/* NIS domain name (why?) */
};

static uint8_t dhcp_discover_hdr[] = {
	99, 130, 83, 99,		/* bootp cookie */
	53, 1, DHCPDISCOVER,		/* dhcp message type */
	55, sizeof(dhcp_params),	/* parameter list */
};

static uint8_t dhcp_request_hdr[] = {
	99, 130, 83, 99,		/* boot cookie */
	53, 1, DHCPREQUEST,		/* dhcp message type */
#define SERVER_IP_OFF 9
	54, 4, 0, 0, 0, 0,		/* server IP */
#define REQ_IP_OFF 15
	50, 4, 0, 0, 0, 0,		/* requested IP address */
	55, sizeof(dhcp_params),	/* parameter list */
};

static uint8_t dhcp_end[] = {
	255,
};

/* Both iovecs below have to have the same structure, since dhcp_send()
   pokes at the internals */
#define DHCP_IOV_LEN 6

static struct iovec dhcp_discover_iov[] = {
	/* [0] = ip + udp header */
	/* [1] = bootp header */
	[2] = {dhcp_discover_hdr, sizeof(dhcp_discover_hdr)},
	[3] = {dhcp_params, sizeof(dhcp_params)},
	/* [4] = DHCP vendor class */
	[5] = {dhcp_end, sizeof(dhcp_end)}
};

static struct iovec dhcp_request_iov[] = {
	/* [0] = ip + udp header */
	/* [1] = bootp header */
	[2] = {dhcp_request_hdr, sizeof(dhcp_request_hdr)},
	[3] = {dhcp_params, sizeof(dhcp_params)},
	/* [4] = DHCP vendor class */
	[5] = {dhcp_end, sizeof(dhcp_end)}
};

/*
 * Parse a DHCP response packet
 * Returns:
 * 0 = Not handled
 * 2 = DHCPOFFER (from dhcp_proto.h)
 * 5 = DHCPACK
 * 6 = DHCPNACK
 */
static int dhcp_parse(struct netdev *dev, struct bootp_hdr *hdr,
		      uint8_t * exts, int extlen)
{
	uint8_t type = 0;
	uint32_t serverid = INADDR_NONE;
	int ret = 0;

	if (extlen >= 4 && exts[0] == 99 && exts[1] == 130 &&
	    exts[2] == 83 && exts[3] == 99) {
		uint8_t *ext;

		for (ext = exts + 4; ext - exts < extlen;) {
			uint8_t len, *opt = ext++;
			if (*opt == 0)
				continue;

			len = *ext++;

			ext += len;

			if (*opt == 53)
				type = opt[2];
			if (*opt == 54)
				memcpy(&serverid, opt + 2, 4);
		}
	}

	switch (type) {
	case DHCPOFFER:
		ret = bootp_parse(dev, hdr, exts, extlen) ? DHCPOFFER : 0;
		if (ret == DHCPOFFER && serverid != INADDR_NONE)
			dev->serverid = serverid;
		DEBUG(("\n   dhcp offer\n"));
		break;

	case DHCPACK:
		ret = bootp_parse(dev, hdr, exts, extlen) ? DHCPACK : 0;
		DEBUG(("\n   dhcp ack\n"));
		break;

	case DHCPNAK:
		ret = DHCPNAK;
		DEBUG(("\n   dhcp nak\n"));
		break;
	}
	return ret;
}

/*
 * Receive and parse a DHCP packet
 * Returns:
 *-1 = Error in packet_recv
 * 0 = Not handled
 * 2 = DHCPOFFER (from dhcp_proto.h)
 * 5 = DHCPACK
 * 6 = DHCPNACK
 */
static int dhcp_recv(struct netdev *dev)
{
	struct bootp_hdr bootp;
	uint8_t dhcp_options[1500];
	struct iovec iov[] = {
		/* [0] = ip + udp header */
		[1] = {&bootp, sizeof(struct bootp_hdr)},
		[2] = {dhcp_options, sizeof(dhcp_options)}
	};
	int ret;

	ret = packet_recv(iov, 3);
	if (ret <= 0)
		return ret;

	DEBUG(("\n   dhcp xid %08x ", dev->bootp.xid));

	if (ret < sizeof(struct bootp_hdr) || bootp.op != BOOTP_REPLY ||
	    /* RFC951 7.5 */ bootp.xid != dev->bootp.xid ||
	    memcmp(bootp.chaddr, dev->hwaddr, 16))
		return 0;

	ret -= sizeof(struct bootp_hdr);

	return dhcp_parse(dev, &bootp, dhcp_options, ret);
}

static int dhcp_send(struct netdev *dev, struct iovec *vec)
{
	struct bootp_hdr bootp;

	memset(&bootp, 0, sizeof(struct bootp_hdr));

	bootp.op	= BOOTP_REQUEST;
	bootp.htype	= dev->hwtype;
	bootp.hlen	= dev->hwlen;
	bootp.xid	= dev->bootp.xid;
	bootp.ciaddr	= dev->ip_addr;
	bootp.giaddr	= INADDR_ANY;
	bootp.secs	= htons(time(NULL) - dev->open_time);
	memcpy(bootp.chaddr, dev->hwaddr, 16);

	vec[1].iov_base	= &bootp;
	vec[1].iov_len	= sizeof(struct bootp_hdr);

	vec[4].iov_base = vendor_class_identifier;
	vec[4].iov_len  = vendor_class_identifier_len;

	DEBUG(("xid %08x secs %d ", bootp.xid, ntohs(bootp.secs)));

	return packet_send(dev, vec, DHCP_IOV_LEN);
}

/*
 * Send a DHCP discover packet
 */
int dhcp_send_discover(struct netdev *dev)
{
	dev->ip_addr = INADDR_ANY;
	dev->ip_gateway = INADDR_ANY;

	DEBUG(("-> dhcp discover "));

	return dhcp_send(dev, dhcp_discover_iov);
}

/*
 * Receive a DHCP offer packet
 */
int dhcp_recv_offer(struct netdev *dev)
{
	return dhcp_recv(dev);
}

/*
 * Send a DHCP request packet
 */
int dhcp_send_request(struct netdev *dev)
{
	memcpy(&dhcp_request_hdr[SERVER_IP_OFF], &dev->serverid, 4);
	memcpy(&dhcp_request_hdr[REQ_IP_OFF], &dev->ip_addr, 4);

	DEBUG(("-> dhcp request "));

	return dhcp_send(dev, dhcp_request_iov);
}

/*
 * Receive a DHCP ack packet
 */
int dhcp_recv_ack(struct netdev *dev)
{
	return dhcp_recv(dev);
}
