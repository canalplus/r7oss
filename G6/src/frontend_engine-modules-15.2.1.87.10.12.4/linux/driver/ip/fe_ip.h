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

Source file name :fe_ip.h
Author :          SD

Low level function installtion for IP FE

Date        Modification                                    Name
----        ------------                                    --------
09-Aug-11   Created                                         SD

************************************************************************/
#ifndef _FE_IP_H
#define _FE_IP_H

#include <linux/netfilter.h>			/* For netfilter */

#define FEC_DEL_COUNT_MIN               200

#define IP_VERS_4			4
#define IP_VERS_6			6
#define IP_HEADER_VERSION_CHECK(X)	(X >> 4)
#define IPV6_HEADER_LEN			40
#define IPV6_SCOPE_DELIMITER		'%'

#define IP_FE_BUFF_SIZE			2048
#define BUFFER_SAFE_GAP			100
#define BUFFER_SIZE_OVERFLOW_THRESHOLD	1650

#define IP_FE_Q_BUFF_MAX		4000
#define IP_FE_Q_BUFF_SAFE		2000

#define IP_FE_IGNORE_PREV		1200

#define IP_FE_SCHED_PRIORITY	        50

#define MAX_JITTER_CONFIGS              4

#define FE_IP_UDP_PROTO_NO		0x11
#define BYTES_PER_IPV4_ADDR		4
#define BYTES_PER_LONG			4
#define IPV4_ADDRESS_MAX_BYTE		4
#define FE_IP_TS_PACKET_SIZE		188


/*RTP Specific information*/
#define FE_IP_RTP_HDR_SIZE		12

#define FE_IP_RTP_DEFAULT_PORT		5000
#define FE_IP_RTP_HEADER_MIN_SIZE	(3 * sizeof(U32))
#define FE_IP_RTP_HEADER_MAX_SIZE	((3 + 15) * sizeof(U32))
#define FE_IP_RTP_CRC_SIZE		4
#define FE_IP_RTP_MAX_SEQ_NO		65535
#define FE_IP_MIN_SEQUENTIAL		2
#define FE_IP_MAX_DROPOUT		3000
#define FE_IP_MAX_MISORDER		1200

#define FE_IP_RTP_PAYLOAD_TYPE_TS	33
#define FE_IP_RTP_PAYLOAD_TYPE_FEC	96 /* The FEC data shall be sent using
					     the first available dynamic payload
					     type number, which is 96 decimal*/
#define  FE_IP_RTP_SEQ_MOD		(1 << 16) /* Sequence No. is 16 bits */

#define MIN(X, Y) ((X) <= (Y) ? (X) : (Y))

/* RTP header structure */
/* RTP Header info extraction macro */
#define RTP_GET_VER(pay_load)	((*pay_load & 0xc0) >> 6)
#define RTP_GET_PADD(pay_load)	((*pay_load & 0x20) >> 5)
#define RTP_GET_EXT(pay_load)	((*pay_load & 0x10) >> 4)
#define RTP_GET_CC(pay_load)	(*pay_load & 0x0f)
#define RTP_GET_MRK(pay_load)	((*(pay_load + 1) & 0x80) >> 7)
#define RTP_GET_PT(pay_load)	(*((pay_load) + 1) & 0x7f)
#define RTP_GET_SN(pay_load)	ntohs(*(unsigned short *)(pay_load + 2))
#define RTP_GET_TSTAMP(pay_load)	ntohl(*(unsigned long *)(pay_load + 4))
#define RTP_GET_SSRC(pay_load)	 ntohl(*(unsigned long *)(pay_load + 8))
#define RTP_GET_EXT_SIZE(pay_load)	\
	ntohs(*(unsigned short *)((unsigned char *)(pay_load) + 2))
#define RTP_GET_PADDING_SIZE(pay_load, size)	\
	ntohl(((unsigned char *)(pay_load))[size - 1])

#define FEC_GET_SNB(pay_load)	ntohs(*(unsigned short *)pay_load)
#define FEC_GET_LR(pay_load)	ntohs(*(unsigned short *)(pay_load + 2))
#define FEC_GET_EXT(pay_load)	((*(pay_load + 4) & 0x80) >> 7)
#define FEC_GET_PTR(pay_load)	(*(pay_load + 4) & 0x7f)
#define FEC_GET_MASK(pay_load)	((*(unsigned long *)(pay_load + 5) \
								& 0xFFF0) >> 8)
#define FEC_GET_TR(pay_load)	(*(unsigned long *)(pay_load + 8))
#define FEC_GET_N(pay_load)	((*(pay_load + 12) & 0x80) >> 7)
#define FEC_GET_D(pay_load)	((*(pay_load + 12) & 0x40) >> 6)
#define FEC_GET_TYPE(pay_load)	((*(pay_load + 12) & 0x38) >> 3)
#define FEC_GET_IDX(pay_load)	(*(pay_load + 12) & 0x07)
#define FEC_GET_OFFSET(pay_load)	(*(pay_load + 13))
#define FEC_GET_NA(pay_load)	(*(pay_load + 14))
#define FEC_GET_RESERVED(pay_load) (*(pay_load + 15))
#define MATRIX_SIZE(inst)      (ipfec_list[inst]->ipfec_data.col_l *	\
				ipfec_list[inst]->ipfec_data.row_d)
#define MAX_SN_SIZE 65535

struct rtp_header_s {
	uint32_t	version:2;
	uint32_t	padding:1;
	uint32_t	extension:1;
	uint32_t	csrc_count:4;
	uint32_t	marker:1;
	uint32_t	payloadtype:7;
	uint32_t	seq_no:16;
	uint32_t	time_stamp;
	uint32_t	ssrc;
};

/*FEC specific information starts here*/

struct fec_header_s {

/*
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    | SNBase low bits               |          Length Recovery      |
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    |E| PT recovery |                    Mask                       |
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    |                          TS recovery                          |
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    |N|D|type |index| Offset        |      NA       |SNBase ext bits|
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

*/

	uint32_t	snbase:16;
	uint32_t	lengthrecovery:16;
	uint32_t	extended:1;
	uint32_t	ptrecovery:7;
	uint32_t	mask:24;
	uint32_t	tsrecovery;
	uint32_t	N:1;
	uint32_t	D:1;
	uint32_t	type:3;
	uint32_t	index:3;
	uint32_t	offset:8;
	uint32_t	NA:8;
	uint32_t	reserved:8;

};

struct fec_source {
	u16	*snbase_p;
	u16	*lenrecovery_p;
	u16	seqfirst;
	u16	seqlast;
	u16	bufflistmaxsize;
	u8	rowl;
	u8	columnd;
	u16	seqoffset;
	struct rtp_source rtp;
	bool	fec_wrapoffset;
	u16	seqno;
       /* ... */
};

#define IPFEC_FEC_HDR_SIZE	16
#define IPFEC_D_MAX	10
#define IPFEC_L_MAX	5

#define IPFEC_GET_HEADER_SNBASE(ipfec_h)  (*(unsigned short *)(ipfec_h))
#define IPFEC_GET_HEADER_LR(ipfec_h)      ntohs(*(unsigned short *)   \
					      ((unsigned char *)(ipfec_h) + 2))
#define IPFEC_GET_HEADER_EXT(ipfec_h)     ((*((unsigned char *)(ipfec_h) + 4)  \
								  & 0x80) >> 7)
#define IPFEC_GET_HEADER_PTR(ipfec_h)     (*((unsigned char *)(ipfec_h) + 4)  \
									& 0x7F)
#define IPFEC_GET_HEADER_TSR(ipfec_h)     ntohl(*(unsigned long *)  \
					      ((unsigned char *)(ipfec_h) + 8))
#define IPFEC_GET_HEADER_N(ipfec_h)       ((*((unsigned char *)(ipfec_h) + 12) \
								  & 0x80) >> 7)
#define IPFEC_GET_HEADER_D(ipfec_h)       ((*((unsigned char *)(ipfec_h) + 12) \
								  & 0x40) >> 6)
#define IPFEC_GET_HEADER_TYPE(ipfec_h)    ((*((unsigned char *)(ipfec_h) + 12) \
								  & 0x38) >> 3)
#define IPFEC_GET_EADER_INDEX(ipfec_h)   (*((unsigned char *)(ipfec_h) + 12) \
									& 0x07)
#define IPFEC_GET_HEADER_OFFSET(ipfec_h)  (*((unsigned char *)(ipfec_h) + 13) \
									& 0xFF)
#define IPFEC_GET_HEADER_NA(ipfec_h)      (*((unsigned char *)(ipfec_h) + 14) \
									& 0xFF)

/*FEC specific information ends here*/



enum {
	IPV4_HOOK = 0,
	IPV6_HOOK = 1
};

struct ipfe_info_lock {
	spinlock_t lock;
	unsigned char readers;
};

struct ipfec_rec_m_pkt {
	uint16_t        lost_rtp_idx;
	uint16_t        *seq_no_list;
	void            *data_addr;
	uint32_t        len;
};

unsigned int fe_ip_nethook(unsigned int hooknum,
				struct sk_buff *params_p,
				const struct net_device *in_p,
				const struct net_device *out_p,
				int (*okfn)(struct sk_buff *));

int fe_ip_attach(struct stm_fe_ip_s *priv);
int fe_ip_detach(struct stm_fe_ip_s *priv);
int ip_init(struct stm_fe_ip_s *priv,
					stm_fe_ip_connection_t *connect_params);
int ip_term(struct stm_fe_ip_s *priv);
int ip_start(struct stm_fe_ip_s *priv);
int ip_stop(struct stm_fe_ip_s *priv);
int ip_set_control(struct stm_fe_ip_s *priv,
				stm_fe_ip_control_t selector, void *args);
int ip_get_control(struct stm_fe_ip_s *priv,
				stm_fe_ip_control_t selector, void *args);
int ip_attach_obj(struct stm_fe_ip_s *priv);
int ip_detach_obj(struct stm_fe_ip_s *priv);
int ip_get_stats(struct stm_fe_ip_s *priv, stm_fe_ip_stat_t *info_p);
void ip_reset_stats(struct stm_fe_ip_s *priv);
uint8_t *get_rtp_base(uint8_t *pay_load, uint8_t *hdr_size);
void update_ip_array(struct stm_fe_ip_s *priv);

int get_info_from_rtp_hdr(struct rtp_header_s *header_p, uint8_t *pay_load);
struct rtp_header_s *get_rtp_hdr(uint8_t *pay_load);
void init_rtp_seq(struct rtp_source *rs, uint16_t rtp_seqnr);
int update_rtp_seq(struct rtp_source *rs, uint16_t seq);
uint8_t rtp_get_hdr_size(struct rtp_header_s *header_p);
int fe_ipfec_attach(struct stm_fe_ipfec_s *ipfec_priv);
int ipfec_init(struct stm_fe_ipfec_s *ipfec_priv,
					stm_fe_ip_fec_config_t *fec_params);
int ipfec_term(struct stm_fe_ipfec_s *ipfec_priv);
int ipfec_start(struct stm_fe_ipfec_s *ipfec_priv);
int ipfec_stop(struct stm_fe_ipfec_s *ipfec_priv);
int ipfec_set_control(struct stm_fe_ipfec_s *ipfec_priv,
				stm_fe_ip_fec_control_t value, void *args);
int ipfec_get_control(struct stm_fe_ipfec_s *ipfec_priv,
				stm_fe_ip_fec_control_t value, void *args);
int ipfec_get_stats(struct stm_fe_ipfec_s *ipfec_priv,
					stm_fe_ip_fec_stat_t *info_p);
int check_fec(struct stm_fe_ipfec_s *ipfec_priv, struct rtp_header_s rtp_hdr_p);
int enqueue_col_fec(struct sk_buff *ip_fe_buffer, struct stm_fe_ipfec_s *priv);
int dequeue_col_fec(struct stm_fe_ipfec_s *priv, uint16_t ipfe_fend_seq_no);
int process_xor_recovery(struct stm_fe_ipfec_s *priv, uint16_t snb_ref,
			uint16_t fec_idx, struct sk_buff **inject_buff,
			struct ipfec_rec_m_pkt *missing_pkt);
int recover_with_col_fec(struct stm_fe_ipfec_s *priv,
					struct sk_buff **inject_buff,
					struct ipfec_rec_m_pkt *missing_pkt);
int ip_wait_for_data(struct stm_fe_ip_s *priv, uint32_t *data_remaining);
int ip_readnbytes(struct stm_fe_ip_s *priv, struct stm_data_block *data_addr,
							uint32_t *data_len,
							uint32_t *bytes_read);
int ip_get_queue_len(struct stm_fe_ip_s *priv, uint32_t *size);


#endif /* _FE_IP_H */
