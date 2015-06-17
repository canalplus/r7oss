/*
 * BOOTP packet protocol handling.
 */
#include <sys/types.h>
#include <sys/uio.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <netinet/in.h>

#include "ipconfig.h"
#include "netdev.h"
#include "bootp_packet.h"
#include "bootp_proto.h"
#include "packet.h"

static uint8_t bootp_options[312] = {
	[  0] = 99, 130, 83, 99,/* RFC1048 magic cookie */
	[  4] = 1, 4,		/*   4-  9 subnet mask */
	[ 10] = 3, 4,		/*  10- 15 default gateway */
	[ 16] = 5, 8,		/*  16- 25 nameserver */
	[ 26] = 12, 32,		/*  26- 59 host name */
	[ 60] = 40, 32,		/*  60- 95 nis domain name */
	[ 96] = 17, 40,		/*  96-137 boot path */
	[138] = 57, 2, 1, 150,	/* 138-141 extension buffer */
	[142] = 255,		/* end of list */
};

/*
 * Send a plain bootp request packet with options
 */
int bootp_send_request(struct netdev *dev)
{
	struct bootp_hdr bootp;
	struct iovec iov[] = {
		/* [0] = ip + udp headers */
		[1] = {&bootp, sizeof(bootp)},
		[2] = {bootp_options, 312}
	};

	memset(&bootp, 0, sizeof(struct bootp_hdr));

	bootp.op	= BOOTP_REQUEST, bootp.htype = dev->hwtype;
	bootp.hlen	= dev->hwlen;
	bootp.xid	= dev->bootp.xid;
	bootp.ciaddr	= dev->ip_addr;
	bootp.secs	= htons(time(NULL) - dev->open_time);
	memcpy(bootp.chaddr, dev->hwaddr, 16);

	DEBUG(("-> bootp xid 0x%08x secs 0x%08x ",
	       bootp.xid, ntohs(bootp.secs)));

	return packet_send(dev, iov, 2);
}

/*
 * Parse a bootp reply packet
 */
int bootp_parse(struct netdev *dev, struct bootp_hdr *hdr,
		uint8_t *exts, int extlen)
{
	dev->bootp.gateway	= hdr->giaddr;
	dev->ip_addr		= hdr->yiaddr;
	dev->ip_server		= hdr->siaddr;
	dev->ip_netmask		= INADDR_ANY;
	dev->ip_broadcast	= INADDR_ANY;
	dev->ip_gateway		= hdr->giaddr;
	dev->ip_nameserver[0]	= INADDR_ANY;
	dev->ip_nameserver[1]	= INADDR_ANY;
	dev->hostname[0]	= '\0';
	dev->nisdomainname[0]	= '\0';
	dev->bootpath[0]	= '\0';
	memcpy(&dev->filename, &hdr->boot_file, FNLEN);

	if (extlen >= 4 && exts[0] == 99 && exts[1] == 130 &&
	    exts[2] == 83 && exts[3] == 99) {
		uint8_t *ext;

		for (ext = exts + 4; ext - exts < extlen;) {
			int len;
			uint8_t opt = *ext++;

			if (opt == 0)
				continue;
			else if (opt == 255)
				break;

			len = *ext++;

			switch (opt) {
			case 1:	/* subnet mask */
				if (len == 4)
					memcpy(&dev->ip_netmask, ext, 4);
				break;
			case 3:	/* default gateway */
				if (len >= 4)
					memcpy(&dev->ip_gateway, ext, 4);
				break;
			case 6:	/* DNS server */
				if (len >= 4)
					memcpy(&dev->ip_nameserver, ext,
					       len >= 8 ? 8 : 4);
				break;
			case 12:	/* host name */
				if (len > sizeof(dev->hostname) - 1)
					len = sizeof(dev->hostname) - 1;
				memcpy(&dev->hostname, ext, len);
				dev->hostname[len] = '\0';
				break;
			case 15:	/* domain name */
				if (len > sizeof(dev->dnsdomainname) - 1)
					len = sizeof(dev->dnsdomainname) - 1;
				memcpy(&dev->dnsdomainname, ext, len);
				dev->dnsdomainname[len] = '\0';
				break;
			case 17:	/* root path */
				if (len > sizeof(dev->bootpath) - 1)
					len = sizeof(dev->bootpath) - 1;
				memcpy(&dev->bootpath, ext, len);
				dev->bootpath[len] = '\0';
				break;
			case 26:	/* interface MTU */
				if (len == 2)
					dev->mtu = (ext[0] << 8) + ext[1];
				break;
			case 28:	/* broadcast addr */
				if (len == 4)
					memcpy(&dev->ip_broadcast, ext, 4);
				break;
			case 40:	/* NIS domain name */
				if (len > sizeof(dev->nisdomainname) - 1)
					len = sizeof(dev->nisdomainname) - 1;
				memcpy(&dev->nisdomainname, ext, len);
				dev->nisdomainname[len] = '\0';
				break;
			case 54:	/* server identifier */
				if (len == 4 && !dev->ip_server)
					memcpy(&dev->ip_server, ext, 4);
				break;
			}

			ext += len;
		}
	}

	/*
	 * Got packet.
	 */
	return 1;
}

/*
 * Receive a bootp reply and parse packet
 */
int bootp_recv_reply(struct netdev *dev)
{
	struct bootp_hdr bootp;
	uint8_t bootp_options[312];
	struct iovec iov[] = {
		/* [0] = ip + udp headers */
		[1] = {&bootp, sizeof(struct bootp_hdr)},
		[2] = {bootp_options, 312}
	};
	int ret;

	ret = packet_recv(iov, 3);
	if (ret <= 0)
		return ret;

	if (ret < sizeof(struct bootp_hdr) ||
	    bootp.op != BOOTP_REPLY ||	/* RFC951 7.5 */
	    bootp.xid != dev->bootp.xid ||
	    memcmp(bootp.chaddr, dev->hwaddr, 16))
		return 0;

	ret -= sizeof(struct bootp_hdr);

	return bootp_parse(dev, &bootp, bootp_options, ret);
}

/*
 * Initialise interface for bootp.
 */
int bootp_init_if(struct netdev *dev)
{
	short flags;

	/*
	 * Get the device flags
	 */
	if (netdev_getflags(dev, &flags))
		return -1;

	/*
	 * We can't do DHCP nor BOOTP if this device
	 * doesn't support broadcast.
	 */
	if (dev->mtu < 364 || (flags & IFF_BROADCAST) == 0) {
		dev->caps &= ~(CAP_BOOTP | CAP_DHCP);
		return 0;
	}

	/*
	 * Get a random XID
	 */
	dev->bootp.xid = (uint32_t) lrand48();
	dev->open_time = time(NULL);

	return 0;
}
