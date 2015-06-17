/************************************************************************
Copyright (C) 2012 STMicroelectronics. All Rights Reserved.

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

Source file name :fe_ip_fec.c
Author :          SD

Low level function FE IP --- FEC Packet processing

Date        Modification                                    Name
----        ------------                                    --------
30-Aug-12   Created                                         SD

************************************************************************/
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/stm/ip.h>
#include <stm_fe.h>
#include <stm_fe_os.h>
#include <stm_fe_ip.h>
#include <stm_fe_ipfec.h>

#include <linux/ip.h>		/* struct iphdr */
#include <linux/udp.h>		/* struct udphdr */

/*
 * Name: fe_ipfec_attach()
 *
 * Description: Installs the fe ipfec function pointers
 *
 */
int fe_ipfec_attach(struct stm_fe_ipfec_s *priv)
{
	priv->ops->init = ipfec_init;
	priv->ops->term = ipfec_term;
	priv->ops->check_fec = check_fec;
	if (priv->config->ipfec_scheme == STM_FE_IP_FEC_SCHEME_XOR_COLUMN) {
		priv->ops->enqueue_fec = enqueue_col_fec;
		priv->ops->dequeue_fec = dequeue_col_fec;
		priv->ops->recover_with_fec = recover_with_col_fec;
		priv->ops->ipfec_stop = ipfec_stop;
		priv->ops->ipfec_start = ipfec_start;
	} else {
		pr_info("%s: row based fec yet not supported\n", __func__);
	}

	return 0;
}
EXPORT_SYMBOL(fe_ipfec_attach);


/**section 1: basic functions to support api's*/

/*
 * Name: ipfec_init()
 *
 * Description: initialization of fe ipfec
 *
 */
int ipfec_init(struct stm_fe_ipfec_s *priv, stm_fe_ip_fec_config_t *fec_params)
{
	struct ipfec_params *p;

	p = &priv->ipfec_data;

	p->row_d = IPFEC_L_MAX;
	p->col_l = IPFEC_D_MAX;

	priv->matrix_size = p->row_d * p->col_l;
	priv->fec_enable = false;
	priv->ipfec_start_stop = false;

	if (!priv->config->ipfec_port) {
		if (priv->config->ipfec_scheme ==
				STM_FE_IP_FEC_SCHEME_XOR_COLUMN)
			priv->config->ipfec_port = priv->ip_h->config->ip_port
									+ 2;
		else if (priv->config->ipfec_scheme ==
				STM_FE_IP_FEC_SCHEME_XOR_ROW)
			priv->config->ipfec_port = priv->ip_h->config->ip_port
									+ 4;
	}

	return 0;
}

/*
 * Name: ipfec_term()
 *
 * Description: termination of fe ipfec
 *
 */
int ipfec_term(struct stm_fe_ipfec_s *priv)
{
	struct ipfec_params *p;

	p = &priv->ipfec_data;
	p->row_d = 0;
	p->col_l = 0;

	return 0;
}

/*
 * Name: ipfec_start()
 *
 * Description: start fe ipfec
 *
 */
int ipfec_start(struct stm_fe_ipfec_s *priv)
{
	priv->fec_enable = true;
	return 0;
}

/*
 * Name: ipfec_stop()
 *
 * Description: stop fe ipfec
 *
 */
int ipfec_stop(struct stm_fe_ipfec_s *priv)
{
	struct ipfec_params *p;
	uint16_t head;
	int num_tries = 20;

	priv->fec_enable = false;
	p = &priv->ipfec_data;

	priv->ip_h->ip_data.inj_rate = priv->ip_h->tsk_inj_cnt_min;

	while (priv->ipfec_start_stop != true && num_tries != 0) {
		num_tries--;
		FE_WAIT_N_MS(10);
		if (num_tries == 0)
			pr_err("%s: Timeout: ipfec_start_stop flag not set\n",
								      __func__);
	}
	head = p->head;
	while (head != p->tail) {
		if (p->ipfec_buff[head])
			kfree_skb(p->ipfec_buff[head]);
		p->ipfec_buff[head] = NULL;
		head = (head + 1) % IP_FE_BUFF_SIZE;
	}

	return 0;
}

int ipfec_set_control(struct stm_fe_ipfec_s *priv,
				stm_fe_ip_fec_control_t value, void *args)
{
	int ret = 0;

	switch (value) {
	case STM_FE_IP_FEC_RESET_STATS:
		/*reset fec stats: to be implemented*/
		break;

	default:
		ret = -1;
		pr_err("%s: wrong selector: %d\n", __func__, value);
		break;
	}

	return ret;
}

int ipfec_get_control(struct stm_fe_ipfec_s *priv,
				stm_fe_ip_fec_control_t value, void *args)
{
	pr_err("%s: selector %d is not valid\n", __func__, value);

	return -EINVAL;
}

int ipfec_get_stats(struct stm_fe_ipfec_s *priv,
					stm_fe_ip_fec_stat_t *info_p)
{
	struct ipfec_params *p;
	int ret = 0;

	p = &priv->ipfec_data;

	info_p->corrected_packet_count = p->stats.corrected_packet_count;
	info_p->packet_count = p->stats.packet_count;
	info_p->lost_packet_count = p->stats.lost_packet_count;
	info_p->rejected_packet_count = p->stats.rejected_packet_count;

	return ret;
}

/**section 2: fucntions for checking and enqueuing fec packet inside hook***/

/*
 * Name: init_rtp_fec_idx()
 *
 * Description: initialize rtp object inside fec object
 *
 */
int init_rtp_fec_idx(struct stm_fe_ipfec_s *priv, uint16_t rtp_seqnr)
{
	struct rtp_source *rs_fec;
	rs_fec = &priv->rs_fec;

	rs_fec->base_seq = rtp_seqnr;
	rs_fec->max_seq = rtp_seqnr;
	rs_fec->bad_seq = FE_IP_RTP_SEQ_MOD + 1; /* so seq == bad_seq
						    is false */
	rs_fec->cycles = 0;
	rs_fec->received = 0;
	rs_fec->received_prior = 0;
	rs_fec->expected_prior = 0;
	rs_fec->seq_offset = rtp_seqnr;
	rs_fec->wrap_offset = true;
	/* other initialization */

	return 0;
}

/*
 * Name: update_rtp_fec_idx()
 *
 * Description: update rtp object inside fec object
 *
 */
int update_rtp_fec_idx(struct stm_fe_ipfec_s *priv,  uint16_t seq)
{
	uint16_t udelta;
	struct rtp_source *rs_fec;

	rs_fec = &priv->rs_fec;
	udelta = seq - rs_fec->max_seq;

	/*
	* Source is not valid until MIN_SEQUENTIAL packets with
	* sequential sequence numbers have been received.
	*/
	if (rs_fec->probation) {
		/* packet is in sequence */
		if (seq == rs_fec->max_seq + 1) {
			rs_fec->probation--;
			rs_fec->max_seq = seq;

			if (!rs_fec->probation) {
				init_rtp_fec_idx(priv, seq);
				rs_fec->received++;
				return 0;
			}

		} else {
			rs_fec->probation = FE_IP_MIN_SEQUENTIAL - 1;
			rs_fec->max_seq = seq;
		}
		return 1;
	} else if (udelta < FE_IP_MAX_DROPOUT) {
		/* in order, with permissible gap */
		if (seq < rs_fec->max_seq) {
			/*
			* Sequence number wrapped - count another 64K cycle.
			*/
			rs_fec->cycles += FE_IP_RTP_SEQ_MOD;
		}
		rs_fec->max_seq = seq;
	} else if (udelta <= FE_IP_RTP_SEQ_MOD - FE_IP_MAX_MISORDER) {
		/* the sequence number made a very large jump */
		if (seq == rs_fec->bad_seq) {
			/*
			* Two sequential packets -- assume that the other side
			* restarted without telling us so just re-sync
			* (i.e., pretend this was the first packet).
			*/
			init_rtp_fec_idx(priv, seq);
		} else {
			rs_fec->bad_seq = (seq + 1) & (FE_IP_RTP_SEQ_MOD-1);
			return 1;
		}
	} else {
		/* duplicate or reordered packet */
	}
	rs_fec->received++;

	return 0;
}

/*
 * Name: check_fec()
 *
 * Description: check the fec packet and init/update index accordingly
 *
 */
int check_fec(struct stm_fe_ipfec_s *priv, struct rtp_header_s rtp_hdr_p)
{
	/*get the RTP Header*/
	int err = -1;
	uint32_t seq_no;
	static int initialise;
	struct rtp_source *rs_fec;
	rs_fec = &priv->rs_fec;

	/*check the payload*/
	if (rtp_hdr_p.payloadtype == FE_IP_RTP_PAYLOAD_TYPE_FEC)
		err = -EINVAL;
	/*get the seq no*/
	seq_no = rtp_hdr_p.seq_no;
	/*init and update*/
/*
	TBR :
	if ( ipfec_list[priv->ipfec_id].tail !=
				ipfec_list[priv->ipfec_id].head) {
*/
	if (!initialise) {
		rs_fec->ssrc = rtp_hdr_p.ssrc;
		init_rtp_fec_idx(priv, seq_no);
		initialise++;
	}
/*
	TBR :
	} else if (rs_fec->ssrc == rtp_hdr_p.ssrc) {
		update_rtp_fec_idx(priv, seq_no);
	} else {
		pr_err("%s: sync sourec is wrong", __func__);
		return -EFAULT;
	}
*/
	return 0;
}

/*
 * Name: fec_local_idx()
 *
 * Description: find index of fec packet in the local packet
 *
 */
int fec_local_idx(struct stm_fe_ipfec_s *priv, uint16_t seq_no)
{
	static uint16_t offset;
	struct rtp_source *rs_fec;
	rs_fec = &priv->rs_fec;

	if (seq_no >= rs_fec->seq_offset) {
		offset = (seq_no - rs_fec->seq_offset) % IP_FE_BUFF_SIZE;
	} else {
		offset = ((FE_IP_RTP_MAX_SEQ_NO - rs_fec->seq_offset) + seq_no
									+ 1);
		offset = offset % IP_FE_BUFF_SIZE;
	}

	return offset;

}

 /*
 * Name: idx_in_range()
 *
 * Description: checks if index is in range
 *
 */
inline bool fec_idx_in_range(uint16_t idx, struct stm_fe_ipfec_s *priv)
{
	struct ipfec_params *p;
	bool idx_in_range = false;
	p = &priv->ipfec_data;
	if (p->tail >= p->head)
		idx_in_range = (idx > p->head) && (idx < p->tail);
	else
		idx_in_range = (idx > p->head) || (idx < p->tail);

	return idx_in_range;
}



int get_info_from_fec_hdr(struct fec_header_s *header_p, uint8_t *pay_load)
{
	if (!header_p)
		return -EFAULT;
	header_p->snbase = FEC_GET_SNB(pay_load);
	header_p->lengthrecovery = FEC_GET_LR(pay_load);
	header_p->extended = FEC_GET_EXT(pay_load);
	header_p->ptrecovery = FEC_GET_PTR(pay_load);
	header_p->mask = FEC_GET_MASK(pay_load);
	header_p->tsrecovery =	FEC_GET_TR(pay_load);
	header_p->N = FEC_GET_N(pay_load);
	header_p->D = FEC_GET_D(pay_load);
	header_p->type = FEC_GET_TYPE(pay_load);
	header_p->index = FEC_GET_IDX(pay_load);
	header_p->offset = FEC_GET_OFFSET(pay_load);
	header_p->NA = FEC_GET_NA(pay_load);
	header_p->reserved = FEC_GET_RESERVED(pay_load);

	return 0;
}

/*
 * Name: enqueue_col_fec()
 *
 * Description: enque the fec packet in fec buffer
 *
 */
int enqueue_col_fec(struct sk_buff *ip_fe_buffer, struct stm_fe_ipfec_s *priv)
{
	struct ipfec_params *p;

	uint16_t tail_backup = 0, tail = 0;
	struct fec_header_s fec_hdr_p;
	int err = 0;

	p = &priv->ipfec_data;
	/*get fec_header, NA, offset, snbase, lenrecovery */
	err = get_info_from_fec_hdr(&fec_hdr_p, (ip_fe_buffer)->data
				+ ((ip_hdr(ip_fe_buffer))->ihl * BYTES_PER_LONG)
				+ sizeof(struct udphdr) + FE_IP_RTP_HDR_SIZE);
	/*check FEC Header D process if column FEC*/
	if (!fec_hdr_p.D) {
		p->col_l = fec_hdr_p.offset;
		p->row_d = fec_hdr_p.NA;
		priv->matrix_size = p->row_d * p->col_l;
	} else {
		return NF_ACCEPT;
	}


	tail = p->tail;
	tail_backup = tail;

	/*put FEC packet in local FEC buffer */
	if (unlikely(p->ipfec_buff[tail]))
		return NF_DROP;

	p->ipfec_buff[tail] = ip_fe_buffer;

	p->sn_base[tail] = fec_hdr_p.snbase;
	p->len_rec[tail] = fec_hdr_p.lengthrecovery;
	p->tail = (tail + 1) % IP_FE_BUFF_SIZE;

	/* check tail and window position for buffer overflow */
	if (p->tail == p->head) {
		pr_warn("%s: Overflow detected: tail %u: window %u\n",
				__func__, p->tail, p->head);
		tail = p->tail;
		p->tail = tail_backup;
		return NF_DROP;
	}
	priv->ipfec_data.stats.packet_count++;
	return NF_STOLEN;
}

inline uint16_t get_fend(struct stm_fe_ipfec_s *priv, uint16_t fec_end_seq_no)
{
	struct ipfec_params *p;
	uint16_t idx = 0, tail;
	uint16_t fec_snb;
	uint16_t dist_frm_snb;

	p = &priv->ipfec_data;

	idx = p->head;
	tail = p->tail;

	fec_end_seq_no = (fec_end_seq_no - 2 * (priv->matrix_size) +
						MAX_SN_SIZE) % MAX_SN_SIZE;
	while (idx != tail) {
		fec_snb = p->sn_base[idx];
		dist_frm_snb = (fec_end_seq_no - fec_snb + MAX_SN_SIZE) %
								    MAX_SN_SIZE;
		if (dist_frm_snb % p->col_l == 0)
			if ((dist_frm_snb / p->col_l) < p->row_d)
				return idx;

		idx = (idx + 1) % IP_FE_BUFF_SIZE;
	}

	return p->head;
}

/*
 * Name: dequeue_col_fec()
 *
 * Description: dequeue the fec packet in fec buffer
 *
 */
int dequeue_col_fec(struct stm_fe_ipfec_s *priv, uint16_t ipfe_fend_seq_no)
{
	/*dequeue fec packets*/
	uint16_t fend, idx, del_idx, del_count;
	struct ipfec_params *p;

	p = &priv->ipfec_data;
	if (p->head == p->tail) {
		priv->ipfec_data.overflow_count++;
		return 0;
	}

	idx = p->head;
	fend = get_fend(priv, ipfe_fend_seq_no);

	if (fend == idx) {
		pr_info("%s: related snbase absent in ipfec buffer", __func__);
		return 0;
	}
	del_count = (fend - idx + IP_FE_BUFF_SIZE) % IP_FE_BUFF_SIZE;
	if (del_count < FEC_DEL_COUNT_MIN)
		return 0;

	pr_debug("fend = %d head = %d deletion here\n", fend , idx);
	del_idx = (fend - p->col_l + IP_FE_BUFF_SIZE) % IP_FE_BUFF_SIZE;

	while (idx != del_idx) {
		if (p->ipfec_buff[idx])
			kfree_skb(p->ipfec_buff[idx]);
		p->head = (idx + 1) % IP_FE_BUFF_SIZE;
		p->ipfec_buff[idx] = NULL;
		p->sn_base[idx] = 0;
		p->len_rec[idx] = 0;
		idx = (idx + 1) % IP_FE_BUFF_SIZE;
	}

	return 0;
}

/** section 3: functions for fec based correction during injection **/

/*
 * Name: get_seqno_of_lost_rtp()
 *
 * Description: find seq no of the lost RTP packet
 *
 */
int get_seqno_of_lost_rtp(struct stm_fe_ipfec_s *priv,
					struct sk_buff **inject_buff,
					struct ipfec_rec_m_pkt *missing_pkt)
{
	uint16_t index_f = 0, index_b = 0, index_f1 = 0, index_b1 = 0;
	uint16_t seq_no = 0;
	uint16_t lost_idx, *seq_no_list;
	struct ipfec_params *p;

	p = &priv->ipfec_data;
	lost_idx = missing_pkt->lost_rtp_idx;
	seq_no_list = missing_pkt->seq_no_list;

	index_f = (lost_idx + p->col_l) % IP_FE_BUFF_SIZE;
	index_f1 = (lost_idx + 1) % IP_FE_BUFF_SIZE;
	index_b = (lost_idx - p->col_l + IP_FE_BUFF_SIZE) % IP_FE_BUFF_SIZE;
	index_b1 = (lost_idx - 1 + IP_FE_BUFF_SIZE) % IP_FE_BUFF_SIZE;

	/* lost pkt does not have its index. So find it from either from
	* forward or backward packets (with gap of +/- col_l or +/- 1
	*/

	if (*(inject_buff + index_b))
		seq_no = (*(seq_no_list + index_b) + p->col_l) % MAX_SN_SIZE;
	else if (*(inject_buff + index_b1))
		seq_no = (*(seq_no_list + index_b1) + 1) % MAX_SN_SIZE;
	else if (*(inject_buff + index_f1))
		seq_no = (*(seq_no_list + index_f1) - 1 + MAX_SN_SIZE) %
								    MAX_SN_SIZE;
	else if (*(inject_buff + index_f))
		seq_no = (*(seq_no_list + index_f) - p->col_l + MAX_SN_SIZE) %
								    MAX_SN_SIZE;
	else
		return -EFAULT;

	seq_no_list[lost_idx] = seq_no;

	return 0;
}

/*
 * Name: get_fec_idx_of_lost_rtp()
 *
 * Description: find fec index from the lost RTP packet seq no
 *
 */
int get_fec_idx_of_lost_rtp(struct stm_fe_ipfec_s *priv,
				uint16_t lost_rtp_seq_no, uint16_t *fec_seq)
{
	uint16_t seq_min = 0, seq_no = 0, idx = 0, test_idx = 0, tail;
	struct ipfec_params *p;

	p = &priv->ipfec_data;
	seq_min = (lost_rtp_seq_no - (p->row_d - 1) * p->col_l + MAX_SN_SIZE) %
								    MAX_SN_SIZE;
	tail = (p->tail + 1) % IP_FE_BUFF_SIZE;
	test_idx = p->head;

	for (idx = 0; idx < p->row_d; idx++) {
		seq_no = (seq_min + p->col_l * idx) % MAX_SN_SIZE;
		do {
			if (p->sn_base[test_idx] == seq_no) {
				*fec_seq = test_idx;
				return 0;
			}
			test_idx = (test_idx + 1) % IP_FE_BUFF_SIZE;
		} while (test_idx != tail);
	}
	if (seq_no == p->sn_base[tail])
		*fec_seq = tail;

	return -EFAULT;
}

void incr_pcnt(struct stm_fe_ipfec_s *priv, uint16_t *p_idx)
{
	*p_idx = (*p_idx + priv->ipfec_data.col_l) %
							IP_FE_BUFF_SIZE;
}

/*
 * Name: recover_xor_fec()
 *
 * Description: find the lost packet through xor based operation
 *
 */
int process_xor_recovery(struct stm_fe_ipfec_s *priv, uint16_t snb_ref,
			uint16_t fec_idx, struct sk_buff **inject_buff,
			struct ipfec_rec_m_pkt *missing_pkt)
{
	uint16_t prow_idx = 0, pcol_idx = 0, plen = 0, len = 0, data_idx = 0;
	uint8_t *data_addr = NULL;
	uint8_t *udp_payload_p = NULL, *payload_p = NULL, *media_p = NULL;
	struct udphdr *udpheader_p = NULL;
	uint16_t lost_pkt_idx;
	struct ipfec_params *p;

	p = &priv->ipfec_data;
	lost_pkt_idx = missing_pkt->lost_rtp_idx;

	/*recover the length of the lost packet*/
	plen = p->len_rec[fec_idx];
	for (prow_idx = snb_ref, pcol_idx = 0;
		pcol_idx < p->row_d;
			incr_pcnt(priv, &prow_idx), pcol_idx++) {
		if (prow_idx == lost_pkt_idx)
			continue;

		if (!(*(inject_buff + prow_idx)))
			return -ENOMEM;

		if (!ip_hdr(*(inject_buff + prow_idx)))
			return -ENOMEM;


		udpheader_p =
			(struct udphdr *) ((*(inject_buff + prow_idx))->data
				   + (ip_hdr(*(inject_buff + prow_idx)))->ihl
							   * BYTES_PER_LONG);

		len = (ntohs((udpheader_p)->len)
				- sizeof(struct udphdr));

		if (len % FE_IP_TS_PACKET_SIZE == FE_IP_RTP_HDR_SIZE)
			plen ^=  len - FE_IP_RTP_HDR_SIZE;
		else
			plen ^=  len;
	}

	if (!(p->ipfec_buff[fec_idx]))
		return -ENOMEM;

	/*recover the payload of the lost pkt*/
	udp_payload_p = ((p->ipfec_buff[fec_idx])->data
				+ (ip_hdr(p->ipfec_buff[fec_idx]))->ihl
					* BYTES_PER_LONG)
						+ sizeof(struct udphdr);

	payload_p = udp_payload_p + FE_IP_RTP_HDR_SIZE + IPFEC_FEC_HDR_SIZE;
	for (data_idx = 0; data_idx < plen; data_idx++) {
		for (prow_idx = snb_ref, pcol_idx = 0;
			pcol_idx < p->row_d;
				incr_pcnt(priv, &prow_idx), pcol_idx++) {
			if (prow_idx == lost_pkt_idx)
				continue;

			if (!(*(inject_buff + prow_idx)))
				return -ENOMEM;

			data_addr = ((*(inject_buff + prow_idx))->data
				      + (ip_hdr(*(inject_buff + prow_idx)))->ihl
						* BYTES_PER_LONG);

			if ((len < plen) &&
				((data_addr + data_idx))) {
				payload_p[data_idx] ^= data_addr[data_idx];
				continue;
			}

			if (((len % FE_IP_TS_PACKET_SIZE) == FE_IP_RTP_HDR_SIZE)
						&& ((data_addr + data_idx))) {
				media_p = data_addr + sizeof(struct udphdr) +
							    FE_IP_RTP_HDR_SIZE;
				payload_p[data_idx] ^= media_p[data_idx];
			} else if ((data_addr + data_idx)) {
				payload_p[data_idx] ^= data_addr[data_idx];
			}
		}
	}

	missing_pkt->data_addr = payload_p;
	missing_pkt->len = plen;

	return 0;
}

/*
 * Name: recover_with_col_fec()
 *
 * Description: from the lost RTP seq find the fec packet and correct it
 *
 */
int recover_with_col_fec(struct stm_fe_ipfec_s *priv,
					struct sk_buff **inject_buff,
					struct ipfec_rec_m_pkt *missing_pkt)
{
	uint16_t lost_rtp_seq_no = 0;
	uint16_t fec_idx = 0, head, tail;
	uint16_t dist_from_snb = 0, snb_ref = 0;
	uint16_t lost_rtp_idx;
	int err = 0;

	struct ipfec_params *p;

	p = &priv->ipfec_data;

	head = p->head;
	tail = p->tail;

	lost_rtp_idx = missing_pkt->lost_rtp_idx;
	/*get the sequence number of the lost rtp from the local buufer idx*/
	err = get_seqno_of_lost_rtp(priv, inject_buff, missing_pkt);
	if (err) {
		pr_err("%s: packet rejected\n", __func__);
		return err;
	}

	/*get the fec idx correspoding to the lost packet*/
	lost_rtp_seq_no = missing_pkt->seq_no_list[missing_pkt->lost_rtp_idx];

	err = get_fec_idx_of_lost_rtp(priv, lost_rtp_seq_no, &fec_idx);

	if (err) {
		priv->ipfec_data.stats.lost_packet_count++;
		return err;
	}

	/*get the base reference in local buffer*/
	dist_from_snb = (lost_rtp_seq_no -
			priv->ipfec_data.sn_base[fec_idx] +
						     MAX_SN_SIZE) % MAX_SN_SIZE;

	snb_ref = (IP_FE_BUFF_SIZE + lost_rtp_idx - dist_from_snb) %
								IP_FE_BUFF_SIZE;

	priv->ipfec_data.last_snbase = snb_ref;
	/*recover length and media contents*/
	err = process_xor_recovery(priv, snb_ref,
				fec_idx, inject_buff, missing_pkt);
	if (err) {
		priv->ipfec_data.stats.rejected_packet_count++;
		return err;
	}

	priv->ipfec_data.stats.corrected_packet_count++;
	return err;
}
