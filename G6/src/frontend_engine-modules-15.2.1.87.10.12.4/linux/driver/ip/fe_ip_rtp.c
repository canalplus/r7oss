/************************************************************************
Copyright (C) 2011, 2012 STMicroelectronics. All Rights Reserved.

This file is part of the stm_fe Library.

stm_fe is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License version 2 as published by the
Free Software Foundation.

stm_fe is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with stm_fe; see the file COPYING.  If not, write to the Free Software
Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

The stm_fe Library may alternatively be licensed under a proprietary
license from ST.

Source file name :fe_ip.c
Author :          SD

Low level function FE IP --- RTP Packet processing

Date        Modification                                    Name
----        ------------                                    --------
09-Aug-11   Created                                         SD

************************************************************************/
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/semaphore.h>
#include <linux/skbuff.h>
#include <linux/platform_device.h>
#include <linux/stm/plat_dev.h>
#include <linux/stm/ip.h>
#include <stm_registry.h>
#include <stm_fe.h>
#include <stm_fe_os.h>
#include <stm_fe_ip.h>
#include "fe_ip.h"
#include <linux/netfilter.h>			/* For netfilter */
#include <linux/netfilter_ipv4.h>
#include <linux/in.h>		/* IPPROTO_UDP, INADDR_LOOPBACK, INADDR_ANY */
#include <linux/netfilter.h>			/* Nethook registering */
#include <linux/ip.h>					/* struct iphdr */
#include <linux/udp.h>					/* struct udphdr */
#include <linux/netdevice.h>

void init_rtp_seq(struct rtp_source *rs, uint16_t rtp_seqnr)
{
	rs->base_seq = rtp_seqnr;
	rs->max_seq = rtp_seqnr;
	rs->bad_seq = FE_IP_RTP_SEQ_MOD + 1;   /* so seq == bad_seq is false */
	rs->cycles = 0;
	rs->received = 0;
	rs->received_prior = 0;
	rs->expected_prior = 0;
	rs->seq_offset = rtp_seqnr;
	rs->wrap_offset = true;
       /* other initialization */
}

int update_rtp_seq(struct rtp_source *rs, uint16_t seq)
{

	uint16_t udelta = seq - rs->max_seq;

	/*
	* Source is not valid until MIN_SEQUENTIAL packets with
	* sequential sequence numbers have been received.
	*/
	if (rs->probation) {
		/* packet is in sequence */
		if (seq == rs->max_seq + 1) {
			rs->probation--;
			rs->max_seq = seq;

			if (!rs->probation) {
				init_rtp_seq(rs, seq);
				rs->received++;
				return 0;
			}

		} else {
			rs->probation = FE_IP_MIN_SEQUENTIAL - 1;
			rs->max_seq = seq;
		}
		return 1;
	} else if (udelta < FE_IP_MAX_DROPOUT) {
		/* in order, with permissible gap */
		if (seq < rs->max_seq) {
			/*
			* Sequence number wrapped - count another 64K cycle.
			*/
			rs->cycles += FE_IP_RTP_SEQ_MOD;
		}
		rs->max_seq = seq;
	} else if (udelta <= FE_IP_RTP_SEQ_MOD - FE_IP_MAX_MISORDER) {
		/* the sequence number made a very large jump */
		if (seq == rs->bad_seq) {
			/*
			 * Two sequential packets -- assume that the other side
			 * restarted without telling us so just re-sync
			 * (i.e., pretend this was the first packet).
			 */
			init_rtp_seq(rs, seq);
		} else {
			rs->bad_seq = (seq + 1) & (FE_IP_RTP_SEQ_MOD-1);
			return 1;
		}
	} else {
		/* duplicate or reordered packet */
	}
	rs->received++;
	return 0;

}

uint8_t rtp_get_hdr_size(struct rtp_header_s *header_p)
{
	uint8_t header_size;
	header_size = sizeof(struct rtp_header_s);
	header_size += header_p->csrc_count * 4;
	return header_size;
}

uint8_t *strip_header_of_rtp(struct rtp_header_s *header_p, uint8_t *pay_load)
{
	uint8_t header_size;
	header_size = rtp_get_hdr_size(header_p);
	pay_load += header_size;
	return pay_load;
}

int get_info_from_rtp_hdr(struct rtp_header_s *header_p, uint8_t *pay_load)
{
	if (!header_p)
		return -EFAULT;
	header_p->version = RTP_GET_VER(pay_load);
	header_p->padding = RTP_GET_PADD(pay_load);
	header_p->extension = RTP_GET_EXT(pay_load);
	header_p->csrc_count = RTP_GET_CC(pay_load);
	header_p->marker = RTP_GET_MRK(pay_load);
	header_p->payloadtype =	RTP_GET_PT(pay_load);
	header_p->seq_no = RTP_GET_SN(pay_load);
	header_p->time_stamp = RTP_GET_TSTAMP(pay_load);
	header_p->ssrc = RTP_GET_SSRC(pay_load);
	return 0;
}


uint8_t *get_rtp_base(uint8_t *pay_load, uint8_t *hdr_size)
{
	struct rtp_header_s header_p;
	uint8_t *rtp_payload;
	if (!pay_load) {
		pr_err("%s: pay_load is NULL", __func__);
		return NULL;
	}
	get_info_from_rtp_hdr(&header_p, pay_load);
	rtp_payload = strip_header_of_rtp(&header_p, pay_load);
	if (!rtp_payload) {
		pr_err("%s: rtp_payload is  NULL", __func__);
		return NULL;
	}
	*hdr_size = rtp_get_hdr_size(&header_p);
	return rtp_payload;
}
