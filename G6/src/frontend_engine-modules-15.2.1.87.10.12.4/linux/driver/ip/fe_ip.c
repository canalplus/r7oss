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

Low level functions for FE IP

Date        Modification                                    Name
----        ------------                                    --------
09-Aug-11   Created                                         SD
18-Sep-12   Modified                                        AD
************************************************************************/

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/bug.h>
#include <linux/kthread.h>
#include <linux/list.h>
#include <linux/semaphore.h>
#include <linux/sched.h>
#include <linux/inet.h>
#include <linux/platform_device.h>
#include <linux/stm/plat_dev.h>
#include <linux/stm/ip.h>
#include <linux/stm/ipfec.h>
#include <stm_registry.h>
#include <stm_fe.h>
#include <stm_fe_os.h>
#include <stm_fe_ip.h>
#include <stm_fe_ipfec.h>
#include "fe_ip.h"
#include <strm/fe_ip_strm.h>
/*network related header*/
#include <linux/skbuff.h>
#include <linux/netfilter.h>	/* For netfilter */
#include <linux/netfilter_ipv4.h>
#include <linux/in.h>		/*IPPROTO_UDP, INADDR_LOOPBACK, INADDR_ANY*/
#include <linux/ip.h>		/* struct iphdr */
#include <linux/udp.h>		/* struct udphdr */
#include <linux/netdevice.h>
#include <linux/inetdevice.h>
#include <linux/igmp.h>
#include <net/ipv6.h>
#include <net/addrconf.h>

static struct nf_hook_ops fe_ip_hook_ops[] = {
	{
		.hook = (nf_hookfn *)fe_ip_nethook,
		.owner = THIS_MODULE,
		.pf = PF_INET,
		.hooknum = NF_INET_PRE_ROUTING
	},
	{
		.hook = (nf_hookfn *)fe_ip_nethook,
		.owner = THIS_MODULE,
		.pf = PF_INET6,
		.hooknum = NF_INET_PRE_ROUTING
	}
};

/*structure introduced for ipfe device navigation*/
struct ip_insts {
	struct stm_fe_ip_s *ip_priv;
	fe_mutex *inuse_mtx;
	struct list_head list;
};

int inject_count_lookup[MAX_JITTER_CONFIGS][4] = {
/* blk_inj_cnt_min, blk_inj_cnt_max, tsk_inj_cnt_min, tsk_inj_cnt_max*/
		/*zero jitter config*/    {40, 60, 40, 60},
		/*LOW  jitter config*/    {80, 100, 80, 100},
		/*MEDIUM jitter config*/  {130, 150, 130, 150},
		/*HIGH jitter config*/    {180, 200, 180, 200}
};

LIST_HEAD(ip_insts_list);
DEFINE_SPINLOCK(inst_in_use);

static uint8_t ip_live_count;
static DEFINE_MUTEX(live_cnt_update_mtx);

static int thread_inp_fe_ip[2] = {SCHED_RR, IP_FE_SCHED_PRIORITY};
module_param_array_named(thread_INP_FE_IP, thread_inp_fe_ip, int, NULL, 0644);
MODULE_PARM_DESC(thread_INP_FE_IP, "INP-FE-IP thread: s(Mode),p(Priority)");

/*
 * Name: fe_ip_attach()
 *
 * Description: Installs the fe ip function pointers
 *
 */
int fe_ip_attach(struct stm_fe_ip_s *priv)
{
	if (!priv->ops)
		return -EFAULT;

	priv->ops->init = ip_init;
	priv->ops->term = ip_term;
	priv->ops->start = ip_start;
	priv->ops->stop = ip_stop;
	priv->ops->set_control = ip_set_control;
	priv->ops->get_control = ip_get_control;
	priv->ops->fe_ip_detach = fe_ip_detach;
	priv->ops->get_stats = ip_get_stats;
	priv->ops->attach_obj = ip_attach_obj;
	priv->ops->detach_obj = ip_detach_obj;
	priv->ops->wait_for_data = ip_wait_for_data;
	priv->ops->readnbytes = ip_readnbytes;
	priv->ops->get_queue_len = ip_get_queue_len;

	return 0;
}
EXPORT_SYMBOL(fe_ip_attach);

/*
 * Name: stm_fe_ip_detach()
 *
 * Description: uninstall the IP wrapper functions
 *
 */
int fe_ip_detach(struct stm_fe_ip_s *priv)
{
	if (!priv->ops)
		return -EFAULT;

	priv->ops->get_queue_len = NULL;
	priv->ops->readnbytes = NULL;
	priv->ops->wait_for_data = NULL;
	priv->ops->detach_obj = NULL;
	priv->ops->attach_obj = NULL;
	priv->ops->get_stats = NULL;
	priv->ops->fe_ip_detach = NULL;
	priv->ops->get_control = NULL;
	priv->ops->set_control = NULL;
	priv->ops->stop = NULL;
	priv->ops->start = NULL;
	priv->ops->term = NULL;
	priv->ops->init = NULL;

	return 0;
}

/**
******************************************************************
* Hook related functions start here ******************************
******************************************************************
*/
/**
*helper1 for fe_ip_nethook:rtp_local_idx
*/
static inline uint16_t rtp_local_idx(struct stm_fe_ip_s *priv, uint16_t seq_no)
{
	static uint16_t offset;

	if (seq_no >= priv->rtp_src.seq_offset)
		offset = (seq_no - priv->rtp_src.seq_offset) % IP_FE_BUFF_SIZE;
	else
		offset = ((FE_IP_RTP_MAX_SEQ_NO - priv->rtp_src.seq_offset)
						+ seq_no + 1) % IP_FE_BUFF_SIZE;

	return offset;
}
/**
*helper1 for fe_ip_nethook:idx_in_range
*/
inline bool idx_in_range(uint16_t idx, struct stm_fe_ip_s *priv)
{
	bool idx_in_range = false;
	if (priv->ip_data.tail >= priv->ip_data.wstart)
		idx_in_range = idx > priv->ip_data.wstart &&
				idx < priv->ip_data.tail;
	else
		idx_in_range = idx > priv->ip_data.wstart ||
				idx < priv->ip_data.tail;

	return idx_in_range;
}
/**
*helper1 for fe_ip_nethook:set_ip_and_ipfec_flag
*/
void set_ip_and_ipfec_flag(struct stm_fe_ip_s *priv, bool flag)
{
	if (priv->ipfec_h)
		priv->ipfec_h->ipfec_start_stop = flag;
	priv->ip_start_stop = flag;
}
inline void set_injection_count(struct stm_fe_ip_s *priv)
{
	priv->blk_inj_cnt_min =  inject_count_lookup[priv->jitter_conf][0];
	priv->blk_inj_cnt_max =  inject_count_lookup[priv->jitter_conf][1];
	priv->tsk_inj_cnt_min =  inject_count_lookup[priv->jitter_conf][2];
	priv->tsk_inj_cnt_max =  inject_count_lookup[priv->jitter_conf][3];
	priv->ip_data.inj_rate = priv->tsk_inj_cnt_max;
}
/*
* event notification function for ip frontend
*/
void evt_notify(struct stm_fe_ip_s *priv, stm_fe_ip_event_t *ip_fe_event)
{
	int ret = -1;
	stm_event_t event;

	event.event_id = *ip_fe_event;
	event.object = (stm_fe_ip_h) priv;
	ret = stm_event_signal(&event);
	if (ret)
		pr_err("%s: stm_event_signal failed\n", __func__);

	return;
}

int check_params(struct stm_fe_ip_s *priv)
{
	if (unlikely(!priv))
		return -1;

	/*check instance initialized and start called*/
	if (unlikely(priv->ip_flags.list_updated != true ||
		priv->ip_flags.start_active != true ||
			priv->ip_flags.attach_obj != true))
		return -1;

	return 0;
}

void update_strm_params(struct stm_fe_ip_s *priv)
{
	if (priv->strm_p->signal_to) {
		priv->strm_p->signal_fifo = true;
	} else if (!priv->strm_p->first_pkt_rcvd) {
		ip_strm_evt_notify_fifo(priv);
		priv->strm_p->first_pkt_rcvd = true;
	}
	priv->strm_p->last_ts = stm_fe_time_now();
}

int get_valid_udp_hdr(const struct sk_buff *ip_buff,
			const struct stm_fe_ip_s *priv, void **udphdr_p)
{
	struct iphdr *ip_hdr = NULL;
	struct ipv6hdr *ipv6_hdr = NULL;
	unsigned char *nw_hdr = NULL;
	bool is_equal;

	nw_hdr = skb_network_header(ip_buff);
	if (unlikely(!nw_hdr))
		return NF_ACCEPT;

	switch (IP_HEADER_VERSION_CHECK(nw_hdr[0])) {

	case IP_VERS_4:
		ip_hdr = (struct iphdr *)nw_hdr;

		if (ip_hdr->protocol != FE_IP_UDP_PROTO_NO)
			return NF_ACCEPT;

		if (ip_hdr->daddr != priv->ip_data.params.ipaddr)
			return -1;

		*udphdr_p = ((ip_buff)->data + (ip_hdr)->ihl * BYTES_PER_LONG);
		break;

	case IP_VERS_6:
		ipv6_hdr = (struct ipv6hdr *)nw_hdr;

		if (ipv6_hdr->nexthdr != FE_IP_UDP_PROTO_NO)
			return NF_ACCEPT;

		is_equal = ipv6_addr_equal(&ipv6_hdr->daddr,
					&priv->ip_data.params.ipv6addr);
		if (!is_equal)
			return -1;

		*udphdr_p = ((ip_buff)->data + IPV6_HEADER_LEN);
		break;

	default:
		return -1;
	}

	return 0;
}

int enqueue_validfec(struct sk_buff *ip_buff, const struct stm_fe_ip_s *priv,
				void *udphdr_p)
{
	struct rtp_header_s rtp_hdr_p;
	int32_t nf_status = NF_ACCEPT;

	if (ntohs(((struct udphdr *)udphdr_p)->dest) !=
					priv->ipfec_h->config->ipfec_port)
		return 0;

	get_info_from_rtp_hdr(&rtp_hdr_p, udphdr_p + sizeof(struct udphdr));
	priv->ipfec_h->ops->check_fec(priv->ipfec_h, rtp_hdr_p);
	nf_status = priv->ipfec_h->ops->enqueue_fec(ip_buff, priv->ipfec_h);

	return nf_status;
}

int enqueue_validrtp(struct sk_buff *ip_buff, struct stm_fe_ip_s *priv,
							void *udphdr_p)
{
	struct rtp_header_s rtp_hdr_p;
	static uint16_t tail;
	uint16_t idx = 0;
	int ret = -1;

	ret = get_info_from_rtp_hdr(&rtp_hdr_p,
					 udphdr_p + sizeof(struct udphdr));
	if (unlikely(ret))
		return NF_DROP;
	if ((!priv->rtp_src.init_rtp)) {
		priv->rtp_src.ssrc = rtp_hdr_p.ssrc;
		init_rtp_seq(&priv->rtp_src, rtp_hdr_p.seq_no);
		priv->rtp_src.max_seq = rtp_hdr_p.seq_no - 1;
		priv->rtp_src.probation = FE_IP_MIN_SEQUENTIAL;
		priv->rtp_src.init_rtp = true;
		priv->ip_data.inj_rate = priv->tsk_inj_cnt_max;
		priv->ip_data.last_seq = 0;
		priv->first_inject = true;
		return NF_ACCEPT;
	} else {
		ret = update_rtp_seq(&priv->rtp_src, rtp_hdr_p.seq_no);
		if (ret)
			return NF_ACCEPT;
	}

	tail = priv->ip_data.tail;
	priv->ip_data.tail_backup = tail;

	priv->rtp_src.last_rtp_idx = rtp_local_idx(priv, rtp_hdr_p.seq_no);
	idx = priv->rtp_src.last_rtp_idx;

	if (unlikely(priv->ip_data.inject_buff[idx]))
		return NF_DROP;

	priv->ip_data.inject_buff[idx] = ip_buff;
	priv->rtp_src.seq_no[idx] = rtp_hdr_p.seq_no;
	if (!idx_in_range(idx, priv)) {
		if ((priv->first_inject) &&
			((idx - tail) > IP_FE_IGNORE_PREV)) {
			priv->ip_data.inject_buff[idx] = NULL;
			priv->rtp_src.seq_no[idx] = 0;
			return NF_DROP;
		}
		priv->ip_data.tail = (idx + 1) % IP_FE_BUFF_SIZE;
	}

	return -1; /*NF DROP and NF ACCEPT have values 0 and 1 so -ve val is
						returned here in normal case*/
}

void enqueue_validudp(struct sk_buff *ip_buff, struct stm_fe_ip_s *priv)
{
	static uint16_t tail;

	tail = priv->ip_data.tail;
	priv->ip_data.tail_backup = tail;
	priv->ip_data.inject_buff[tail] = ip_buff;
	priv->ip_data.tail = (priv->ip_data.tail + 1) % IP_FE_BUFF_SIZE;
}

int check_ovf(struct stm_fe_ip_s *priv)
{
	uint16_t tail_backup;
	uint16_t idx = 0;

	tail_backup = priv->ip_data.tail_backup;

	if (priv->ip_data.tail == priv->ip_data.wstart) {
		priv->ip_data.overflow_count++;
		if (priv->ip_data.params.protocol
						== STM_FE_IP_PROTOCOL_RTP_TS) {
			idx = priv->rtp_src.last_rtp_idx;
			priv->ip_data.inject_buff[idx] = NULL;
		} else {
			priv->ip_data.inject_buff[tail_backup] = NULL;
		}

		priv->ip_data.tail = tail_backup;
		atomic_inc(&priv->doinject);
		fe_wake_up_interruptible(&priv->q_event);
		priv->ip_data.ovf = true;
		return -1;
	} else {
		priv->ip_data.last_seq = priv->ip_data.last_seq + 1;
		priv->ip_data.ovf = false;
	}

	return 0;
}

void notify_thread(struct stm_fe_ip_s *priv)
{
	if (priv->ip_data.last_seq != priv->ip_data.inj_rate)
		return;

	priv->ip_data.last_seq = 0;
	if (priv->first_inject) {
		priv->ip_data.inj_rate = priv->tsk_inj_cnt_min;
		priv->first_inject = false;
	} else {
		if (priv->ipfec_h && (priv->ipfec_h->fec_enable))
			priv->ip_data.inj_rate = priv->ipfec_h->matrix_size * 2;
		atomic_inc(&priv->doinject);
		fe_wake_up_interruptible(&priv->q_event);
	}
}

/*
 * Name: fe_ip_nethook()
 *
 * Description: hook function for IPv4/IPv6
 *
 */

unsigned int fe_ip_nethook(unsigned int hooknum, struct sk_buff *ip_buff,
		const struct net_device *in_p, const struct net_device *out_p,
		int (*okfn)(struct sk_buff *))
{
	void *udphdr_p = NULL;
	int ret = 0;
	struct stm_fe_ip_s *priv = NULL;
	stm_fe_ip_connect_protocol_t ip_protocol;
	struct ip_insts *ip_inst;

	stm_fe_spin_lock(&inst_in_use);
	list_for_each_entry(ip_inst, &ip_insts_list, list) {
		priv = ip_inst->ip_priv;
		set_ip_and_ipfec_flag(priv, false);
		wmb();
		ret = check_params(priv);
		if (ret) {
			set_ip_and_ipfec_flag(priv, true);
			continue;
		}

		ret = get_valid_udp_hdr(ip_buff, priv, &udphdr_p);
		if (ret == -1) {
			set_ip_and_ipfec_flag(priv, true);
			continue;
		}  else if (ret == NF_ACCEPT) {
			set_ip_and_ipfec_flag(priv, true);
			stm_fe_spin_unlock(&inst_in_use);
			return NF_ACCEPT;
		}

		/*Handle FEC packets*/
		if (priv->ipfec_h) {
			if (priv->ipfec_h->fec_enable) {
				ret = enqueue_validfec(ip_buff, priv, udphdr_p);
				if (ret) {
					set_ip_and_ipfec_flag(priv, true);
					stm_fe_spin_unlock(&inst_in_use);
					return ret;
				}
			}
		}

		if (((struct udphdr *)udphdr_p)->dest !=
				htons(priv->ip_data.params.port_no)) {
			set_ip_and_ipfec_flag(priv, true);
			continue;
		}

		/*Handle Media packets*/
		ip_protocol = priv->ip_data.params.protocol;
		if (ip_protocol == STM_FE_IP_PROTOCOL_RTP_TS) {
			ret = enqueue_validrtp(ip_buff, priv, udphdr_p);
			if (ret != -1) {
				set_ip_and_ipfec_flag(priv, true);
				stm_fe_spin_unlock(&inst_in_use);
				return ret;
			}
		} else if (ip_protocol == STM_FE_IP_PROTOCOL_UDP_TS) {
			/* store new sk_buff at the tail of inject buff*/
			enqueue_validudp(ip_buff, priv);
		} else {
			set_ip_and_ipfec_flag(priv, true);
			stm_fe_spin_unlock(&inst_in_use);
			return NF_ACCEPT;
		}

		update_strm_params(priv);

		/* check tail and window position for buffer overflow */
		ret = check_ovf(priv);
		if (ret) {
			set_ip_and_ipfec_flag(priv, true);
			stm_fe_spin_unlock(&inst_in_use);
			return NF_DROP;
		}

		/* inject or not to inject */
		notify_thread(priv);

		set_ip_and_ipfec_flag(priv, true);
		stm_fe_spin_unlock(&inst_in_use);
		priv->ip_data.stats.packet_count++;
		return NF_STOLEN;
	}
	set_ip_and_ipfec_flag(priv, true);
	stm_fe_spin_unlock(&inst_in_use);

	return NF_ACCEPT;
}

/**
******************************************************************
* Hook related functions end here ******************************
******************************************************************
*/

/*
 * Name: update_ip_array()
 *
 * Description: updates the new entry for IP global lists
 *
 */
inline void update_ip_array(struct stm_fe_ip_s *priv)
{

	if (!priv->params)
		return;

	priv->ip_data.params.port_no = priv->params->port_no;
	priv->ip_data.params.ipaddr = priv->params->ipaddr;
	priv->ip_data.params.ipv6addr = priv->params->ipv6addr;
	priv->ip_data.params.is_ipv6 = priv->params->is_ipv6;
	priv->ip_data.params.protocol = priv->params->protocol;
	priv->ip_data.ethdev = priv->config->ethdev;
	priv->ip_flags.list_updated = true;

	return;
}

inline uint16_t get_fstart_ip_buff(struct stm_fe_ip_s *priv, uint16_t count)
{
	uint16_t fstart = 0;

	if (priv->ipfec_h && priv->ipfec_h->fec_enable)
		fstart = (IP_FE_BUFF_SIZE + priv->ip_data.wend -
			count - (2 * priv->ipfec_h->matrix_size)) %
							IP_FE_BUFF_SIZE;
	else
		fstart =  priv->ip_data.wstart;
	return fstart;
}

inline uint16_t get_fend_ip_buff(struct stm_fe_ip_s *priv, uint16_t count)
{
	uint16_t fend = 0;

	if (priv->ipfec_h && priv->ipfec_h->fec_enable)
		fend = (IP_FE_BUFF_SIZE + priv->ip_data.wend
		 - (2 * priv->ipfec_h->matrix_size)) % IP_FE_BUFF_SIZE;
	else
		fend = priv->ip_data.wend;

	return fend;
}

inline uint16_t get_fend_seq_no(struct stm_fe_ip_s *priv, uint16_t fend)
{
	struct iphdr *ip_header_p = NULL;
	struct sk_buff *ip_fe_buffer;
	uint16_t seq_no = 0;

	ip_fe_buffer = priv->ip_data.inject_buff[fend];

	ip_header_p = ip_hdr(ip_fe_buffer);

	seq_no = RTP_GET_SN((ip_fe_buffer)->data
			+ ((ip_header_p)->ihl * BYTES_PER_LONG)
			+ sizeof(struct udphdr));
	return seq_no;
}

/*
 * PULL DATA Interface related fucntion defined here
 */

/*
 * ip_wait_for_data: waits till data available for pulling
 */

int ip_wait_for_data(struct stm_fe_ip_s *priv, uint32_t *size)
{
	int ret = 0;

	do {
		if (list_first_entry(&priv->queued_data,
					struct ipfe_queued_data, lh))
			*size = priv->ip_data.q_total_data;

		if (0 == *size) {
			uint32_t interrupted;
			set_current_state(TASK_INTERRUPTIBLE);
			interrupted = schedule_timeout(msecs_to_jiffies(10));
			if (interrupted)
				return -EINTR;
		}

	} while (0 == *size);

	return ret;
}

/*
 * ip_readnbytes: reads data from queue & deletes the node
 */

int ip_readnbytes(struct stm_fe_ip_s *priv, struct stm_data_block *data,
		uint32_t *len, uint32_t *bytes)
{
	int ret = 0;
	struct ipfe_queued_data *q;

	stm_fe_mutex_lock(priv->ip_data.pull_data_mtx);

	*bytes = 0;
	if (list_empty(&priv->queued_data)) {
		stm_fe_mutex_unlock(priv->ip_data.pull_data_mtx);
		pr_warn("%s: queue is empty\n", __func__);
		return -ENODATA;
	}

	q = list_first_entry(&priv->queued_data, struct ipfe_queued_data, lh);
	if (!q) {
		pr_warn("%s: list_first_entry is NULL\n", __func__);
		return -EFAULT;
	}

	*bytes = min(q->data->len, *len);
	memcpy(data->data_addr, q->data->data_addr, *bytes);
	q->data->len -= *bytes;

	if (q->data->len <= 0) {
		list_del(&q->lh);
		priv->ip_data.qsize -= 1;
		kfree_skb(q->ip_buff);
		kfree(q->data->data_addr);
		kfree(q->data);
		kfree(q);
	} else {
		q->data->data_addr += *bytes;
	}

	priv->ip_data.q_total_data -= *bytes;

	stm_fe_mutex_unlock(priv->ip_data.pull_data_mtx);

	return ret;
}

/*
 * ip_get_queue_len: gets the length of the queue for pull data operation
 */

int ip_get_queue_len(struct stm_fe_ip_s *priv, uint32_t *size)
{
	int ret = 0;

	*size = priv->ip_data.q_total_data;

	return ret;
}

/**
******************************************************************
* Thread related functions start here ******************************
******************************************************************
*/

/**
*helper1 for ip_thread:push_and_free_blist
*/
inline int inject_and_free_blist(struct stm_fe_ip_s *priv, uint16_t count)
{
	uint16_t j;
	int result = 0;
	int32_t processed;
	uint16_t fstart = 0, fend = 0, fend_seq_no = 0;
	struct stm_data_block *data;
	struct stm_data_block *tmp;
	bool ipfe_deleted = false;
	uint32_t inj_start_jiff;
	uint8_t idx = 0;

	if (!count)
		return 0;

	data = stm_fe_malloc(count * sizeof(struct stm_data_block));
	if (unlikely(!data)) {
		pr_err("%s: data mem alloc failed\n", __func__);
		return -ENOMEM;
	}
	tmp = data;

	for (j = 0; j < count; j++) {
		data->data_addr =
				priv->ip_data.block_list[j].data_addr;
		data->len = priv->ip_data.block_list[j].len;
		data++;
	}
	data = tmp;

	stm_fe_mutex_unlock(priv->ip_data.start_stop_mtx);
	for (idx = 0; idx < IP_FE_SINK_OBJ_MAX; idx++) {
		if (!priv->ip_src->sink_obj[idx])
			continue;

		if (unlikely(priv->ip_flags.rcvd_first == false)) {
			priv->ip_data.first_inj_latency =
					stm_fe_jiffies_to_msecs(jiffies -
						priv->start_jiffies);
			priv->ip_flags.rcvd_first = true;
		}

		inj_start_jiff = jiffies;

		if (likely(priv->ip_src->push_intf.push_data)) {
			result = priv->ip_src->push_intf.push_data(
					priv->ip_src->sink_obj[idx],
					data, count, &processed);
			priv->ip_data.push_time_ms =
					stm_fe_jiffies_to_msecs(jiffies -
						inj_start_jiff);

			if (result)
				pr_err("%s: sink push_data failed\n", __func__);

		} else if (priv->ip_src->pull_intf.connect) {
			if (priv->ip_data.qsize > IP_FE_Q_BUFF_MAX)
				while (priv->ip_data.qsize >
							IP_FE_Q_BUFF_SAFE) {
					schedule();
			}
		}

	}
	stm_fe_mutex_lock(priv->ip_data.start_stop_mtx);

	fstart = get_fstart_ip_buff(priv, count);
	fend = get_fend_ip_buff(priv, count);
	for (j = fstart; j != fend; j = (j + 1) % IP_FE_BUFF_SIZE) {
		if (priv->ip_data.inject_buff[j]) {
			ipfe_deleted = true;
			if (priv->ipfec_h)
				fend_seq_no = get_fend_seq_no(priv, j);
			priv->ip_data.stats.total_pkts_freed++;
			kfree_skb(priv->ip_data.inject_buff[j]);
			priv->ip_data.inject_buff[j] = NULL;
		}
	}
	stm_fe_free((void **)&data);
	if (priv->ipfec_h && ipfe_deleted == true &&
						    (priv->ipfec_h->fec_enable))
		priv->ipfec_h->ops->dequeue_fec(priv->ipfec_h, fend_seq_no);

	return result;
}

/**
*helper2 for ip_thread:update_block_list
*/
static inline int update_block_list(struct sk_buff *ip_fe_buff,
		struct stm_fe_ip_s *priv,
		uint16_t count)
{
	void *data_addr = NULL;
	void *udpheader_p;
	uint32_t len = 0;
	struct iphdr *ip_header_p = NULL;
	struct ipv6hdr *ipv6_header_p = NULL;
	struct sk_buff *ip_fe_buffer = NULL;
	struct ipfe_queued_data *q;

	if (priv->ip_src->pull_intf.connect)
		ip_fe_buffer = skb_clone(ip_fe_buff, GFP_KERNEL);
	else
		ip_fe_buffer = ip_fe_buff;

	/*get udp header*/
	if (priv->params->is_ipv6 == false) {
		ip_header_p = ip_hdr(ip_fe_buffer);
		if (unlikely(!ip_header_p)) {
			pr_err("%s: ip_header_p is NULL", __func__);
			return -EINVAL;
		}

		udpheader_p = (ip_fe_buffer)->data
				+ (ip_header_p)->ihl * BYTES_PER_LONG;
	} else {
		ipv6_header_p = ipv6_hdr(ip_fe_buffer);
		if (unlikely(!ipv6_header_p)) {
			pr_err("%s: ipv6_header_p is NULL", __func__);
			return -EINVAL;
		}

		udpheader_p = ((ip_fe_buffer)->data + IPV6_HEADER_LEN);
	}

	/* update block list array*/
	if (priv->ip_data.params.protocol ==
						STM_FE_IP_PROTOCOL_RTP_TS) {

		uint8_t rtp_hdr_size;
		data_addr = get_rtp_base(udpheader_p + sizeof(struct udphdr),
								&rtp_hdr_size);

		len = (ntohs(((struct udphdr *)udpheader_p)->len)
				- sizeof(struct udphdr) - rtp_hdr_size);

	} else if (priv->ip_data.params.protocol ==
						STM_FE_IP_PROTOCOL_UDP_TS) {

		data_addr = (udpheader_p + sizeof(struct udphdr));

		len = (ntohs(((struct udphdr *)udpheader_p)->len)
						- sizeof(struct udphdr));
	}

	priv->ip_data.block_list[count].data_addr = data_addr;
	priv->ip_data.block_list[count].len = len;

	/*update inject queue for memsink*/
	if (priv->ip_src->pull_intf.connect) {
		stm_fe_mutex_lock(priv->ip_data.pull_data_mtx);
		q = stm_fe_malloc(sizeof(struct ipfe_queued_data));
		if (unlikely(!q)) {
			pr_err("%s: queued_data mem alloc failed\n", __func__);
			return -ENOMEM;
		}
		q->data = stm_fe_malloc(sizeof(struct stm_data_block) + len);
		if (unlikely(!q->data)) {
			pr_err("%s: q->data mem alloc failed\n", __func__);
			stm_fe_free((void **)&q);
			return -ENOMEM;
		}
		q->data->data_addr = stm_fe_malloc(sizeof(uint8_t *) * len);
		if (unlikely(!q->data->data_addr)) {
			pr_err("%s: queued_data_addr mem alloc failed\n",
								__func__);
			stm_fe_free((void **)&q->data);
			stm_fe_free((void **)&q);
			return -ENOMEM;
		}
		q->data->len = len;
		q->ip_buff = ip_fe_buffer;
		memcpy(q->data->data_addr, data_addr, len);
		priv->ip_data.q_total_data += len;
		list_add_tail(&q->lh, &priv->queued_data);
		priv->ip_data.qsize += 1;
		stm_fe_mutex_unlock(priv->ip_data.pull_data_mtx);
	}

	return 0;
}

/**
*helper3 for ip_thread:find_inject_params
*/
static inline void find_inject_params(struct stm_fe_ip_s *priv,
					uint16_t *inject_count,
					uint16_t *iter, uint16_t *buff_size)
{
	/*calculate buffer size*/
	if (priv->ip_data.tail >= priv->ip_data.wstart)
		*buff_size = priv->ip_data.tail
						- priv->ip_data.wstart;
	else
		*buff_size = priv->ip_data.tail + IP_FE_BUFF_SIZE
						- priv->ip_data.wstart;
	/*decide the injection count*/
	if (*buff_size < BUFFER_SIZE_OVERFLOW_THRESHOLD) {
		*inject_count = MIN(*buff_size, priv->blk_inj_cnt_min);
		*iter = 1;
		return;
	} else {
		*inject_count = priv->blk_inj_cnt_max;
		*iter = (*buff_size / priv->blk_inj_cnt_max);
	}

	return;
}
/**
*helper4 for ip_thread:clean_bklog
*/
inline void clean_bklog(struct stm_fe_ip_s *priv)
{
	uint16_t j = 0;

	for (j = 0; j < priv->blk_inj_cnt_max; j++) {
		if (priv->ip_data.inject_buff[j]) {
			priv->ip_data.stats.total_pkts_freed++;
			kfree_skb(priv->ip_data.inject_buff[j]);
			priv->ip_data.inject_buff[j] = NULL;
		}
	}

	return;
}

inline int check_for_recovery(struct stm_fe_ip_s *priv, uint16_t lost_rtp_idx,
								uint16_t count)
{
	int err = 0;
	uint32_t start_jiff;
	struct ipfec_rec_m_pkt missing_pkt;
	if (!priv->ipfec_h->ops->recover_with_fec)
		return -EINVAL;

	missing_pkt.lost_rtp_idx = lost_rtp_idx;
	missing_pkt.seq_no_list = priv->rtp_src.seq_no;
	start_jiff = jiffies;
	err = priv->ipfec_h->ops->recover_with_fec(priv->ipfec_h,
			priv->ip_data.inject_buff, &missing_pkt);
	priv->ipfec_h->ipfec_data.recovery_time_ms =
		stm_fe_jiffies_to_msecs(jiffies - start_jiff);
	if (err)
		return err;

	priv->ip_data.block_list[count].data_addr
							= missing_pkt.data_addr;
	priv->ip_data.block_list[count].len = missing_pkt.len;

	return err;

}

/*
 * ip_thread : it is an injection task . it keeps on injecting
 * BLOCK_INJECTION_COUNT_MAX/MIN no of packets per instance to the transport
 * engine for decoding
 */

static int ip_thread(void *data)
{
	uint16_t wend;
	uint16_t count = 0;
	uint16_t k = 0;
	uint16_t inject_count, buff_size;
	uint16_t i;
	uint16_t iter;
	struct sk_buff *ip_fe_buffer = NULL;
	int result = 0;
	stm_fe_ip_event_t event;
	struct stm_fe_ip_s *priv = (struct stm_fe_ip_s *) data;

	while (!kthread_should_stop()) {
		result = fe_wait_event_interruptible_timeout(priv->q_event,
			(((atomic_read(&priv->doinject) > 0) &&
			 priv->ip_flags.start_active) ||
			 priv->ip_fe_thread_break), priv->wait_to);

		if (result < 0)
			pr_err("%s: wait_event failed: %d", __func__, result);

		/*clear back log in case of term*/
		if (priv->ip_fe_thread_break) {
			clean_bklog(priv);
			break;
		}
		/*throw timeout events*/
		if (!result) {
			priv->strm_p->signal_to = true;
			if (priv->ip_flags.start_active)
				ip_strm_evt_timeout(priv);
			continue;
		} else if (priv->strm_p->signal_to) {
			ip_strm_evt_notify_fifo(priv);
			priv->strm_p->signal_to = false;
		}

		stm_fe_mutex_lock(priv->ip_data.start_stop_mtx);

		find_inject_params(priv, &inject_count, &iter, &buff_size);
		if (priv->ipfec_h &&
			((buff_size < (priv->ipfec_h->matrix_size * 2)))) {
			stm_fe_mutex_unlock(priv->ip_data.start_stop_mtx);
			continue;
		}

		for (k = 0; (k < iter) && inject_count; k++) {
			count = 0;
			for (i = 0; i < (inject_count); i++) {
				/* pick sk_buff from inject buffer */
				wend = priv->ip_data.wend;
				ip_fe_buffer = priv->ip_data.inject_buff[wend];

				priv->ip_data.wend = (priv->ip_data.wend + 1)
							      % IP_FE_BUFF_SIZE;

				if (ip_fe_buffer)
					goto update;
				priv->ip_data.stats.lost_packet_count++;
				if (priv->ipfec_h &&
				    priv->ipfec_h->fec_enable &&
				    (priv->ip_data.params.protocol ==
					   STM_FE_IP_PROTOCOL_RTP_TS)) {
					result = check_for_recovery(priv
							, wend, count);
					if (!result)
						count++;
					continue;
				} else {
					continue;
				}
				/*update block list with the picked up buffer*/
update:
				result = update_block_list(ip_fe_buffer,
								  priv, count);
				if (unlikely(result))
					continue;
				count++;
			}
			/* throw event once the 1st block of packets is rcvd */
			priv->strm_p->last_evt
					 = STM_FE_IP_EV_FIRST_PACKET_RECEIVED;
			event = STM_FE_IP_EV_FIRST_PACKET_RECEIVED;
			if (unlikely(!priv->ip_flags.rcvd_first))
				evt_notify(priv, &event);

			/* push blocks and free bufs*/
			result = inject_and_free_blist(priv, count);
			/* update wstart*/
			priv->ip_data.wstart = priv->ip_data.wend;

		}

		stm_fe_mutex_unlock(priv->ip_data.start_stop_mtx);
		atomic_dec(&priv->doinject);
	}
	return 0;
}

/**
******************************************************************
* Thread related functions end here ******************************
******************************************************************
*/

/*
 * Name: ip_init()
 *
 * Description: initialization of fe ip
 *
 */
int ip_init(struct stm_fe_ip_s *priv, stm_fe_ip_connection_t *params)
{
	int ret = 0;
	char task_name[20];
	const char *pos, *end;
	int32_t mtx_ret = 0;
	struct ip_insts *ip_inst;

	/* populate private params */
	priv->params = stm_fe_malloc(sizeof(struct stm_fe_ip_connection_s));
	if (!priv->params) {
		pr_err("%s: params mem alloc failed\n", __func__);
		return -ENOMEM;
	}

	ip_inst = stm_fe_malloc(sizeof(struct ip_insts));
	if (!ip_inst) {
		printk(KERN_ERR "%s: ip_inst mem allocation fail\n", __func__);
		return -ENOMEM;
	}

	if (params) {
		priv->params->port_no = params->port_no;
		pos = strchr(params->ip, '.');
		if (pos) {
			priv->params->ipaddr = in_aton(params->ip);
			priv->params->is_ipv6 = false;
		} else {
			in6_pton(params->ip, -1,
					(u8 *)&priv->params->ipv6addr.in6_u,
					IPV6_SCOPE_DELIMITER, &end);
			priv->params->is_ipv6 = true;
		}
		priv->params->protocol = params->protocol;
	} else {
		priv->params->port_no = priv->config->ip_port;
		priv->params->ipaddr = in_aton(priv->config->ip_addr);
		priv->params->is_ipv6 = false;
		priv->params->protocol =
			(stm_fe_ip_connect_protocol_t) priv->config->protocol;
	}
	priv->ip_fe_thread = NULL;
	priv->ip_data.block_count = 0;
	priv->ip_data.tail = 0;
	priv->ip_data.wstart = 0;
	priv->ip_data.wend = 0;
	priv->ip_data.stats.packet_count = 0;
	priv->ip_data.stats.lost_packet_count = 0;
	priv->ip_data.stats.bad_frag_count = 0;
	priv->ip_data.stats.bad_length_count = 0;
	priv->ip_data.stats.total_pkts_freed = 0;
	priv->ip_data.ovf = false;
	priv->ip_data.start_stop_mtx = NULL;
	priv->ip_data.last_seq = 0;
	priv->ip_data.socket = NULL;
	priv->ip_data.inj_rate = 0;
	priv->jitter_conf = 0; /*DEFAULT zero jitter configuration*/
	priv->ip_flags.list_updated = false;
	priv->ip_flags.start_active = false;
	priv->ip_flags.attach_obj = false;
	priv->ip_data.attach_cnt = 0;
	priv->ip_flags.rcvd_first = false;
	priv->ip_fe_thread_break = false;
	priv->first_inject = true;
	priv->ip_start_stop = false;
	priv->ip_data.q_total_data = 0;
	priv->ip_src->pull_intf.connect = NULL;
	priv->wait_to = msecs_to_jiffies(200);
	priv->stop_jiffies = jiffies;

	atomic_set(&priv->doinject, 0);

	update_ip_array(priv);

	/*init q for injection to memsink*/
	INIT_LIST_HEAD(&priv->queued_data);

	if (!priv->ip_data.start_stop_mtx) {
		priv->ip_data.start_stop_mtx
				 = (fe_mutex *)stm_fe_malloc(sizeof(fe_mutex));
		if (!priv->ip_data.start_stop_mtx) {
			pr_info("%s: start_stop_mtx_init failed\n", __func__);
			stm_fe_free((void **)&priv->params);
			stm_fe_free((void **)&ip_inst);
			return -ENOMEM;
		}
		mtx_ret = stm_fe_mutex_init(priv->ip_data.start_stop_mtx);
	}

	if (!priv->ip_data.pull_data_mtx) {
		priv->ip_data.pull_data_mtx
				 = (fe_mutex *)stm_fe_malloc(sizeof(fe_mutex));
		if (!priv->ip_data.pull_data_mtx) {
			pr_info("%s: pull_data_mtx_init failed\n", __func__);
			stm_fe_free((void **)&priv->ip_data.start_stop_mtx);
			stm_fe_free((void **)&priv->params);
			stm_fe_free((void **)&ip_inst);
			return -ENOMEM;
		}
		mtx_ret = stm_fe_mutex_init(priv->ip_data.pull_data_mtx);
	}

	ret = ip_strm_evt_init(priv);

	if (ret) {
		pr_err("%s: ip_strm_evt_init error", __func__);
		goto free_alloc;
	}

	/* store ip_priv into ip_inst to use it in nethook */
	ip_inst->ip_priv = priv;
	/* init list within the struct and add it to the tail of the list */
	stm_fe_spin_lock_bh(&inst_in_use);
	list_add_tail(&ip_inst->list, &ip_insts_list);
	stm_fe_spin_unlock_bh(&inst_in_use);
	priv->ip_fe_thread_break = false;
	/*initialize wait queue*/
	fe_init_waitqueue_head(&priv->q_event);
	/*ensure injection buffer is empty from the beginning*/
	memset(priv->ip_data.inject_buff, 0, IP_FE_BUFF_SIZE);
	/* create IP thread*/
	snprintf(task_name, sizeof(task_name), "INP-FE-IP%d", priv->ip_id);
	ret = stm_fe_thread_create(&priv->ip_fe_thread, ip_thread,
			      (void *)priv, task_name, thread_inp_fe_ip);
	if (ret) {
		pr_err("%s: ip fe thread creation error %d", __func__, ret);
		goto free_alloc;
	}

	return ret;

free_alloc:
	stm_fe_free((void **)&priv->ip_data.pull_data_mtx);
	stm_fe_free((void **)&priv->ip_data.start_stop_mtx);
	stm_fe_free((void **)&priv->params);
	stm_fe_free((void **)&ip_inst);

	return ret;
}

/*
 * Name: ip_term()
 *
 * Description: termination of fe ip
 *
 */
int ip_term(struct stm_fe_ip_s *priv)
{
	int ret = 0;
	struct ip_insts *ip_inst, *tmp;

	if (priv->ip_flags.start_active)
		ip_stop(priv);/*in case if ip_term called before ip_stop*/

	priv->ip_fe_thread_break = true;
	if (priv->ip_fe_thread)
		ret = stm_fe_thread_stop(&priv->ip_fe_thread);
	if (!ret)
		priv->ip_fe_thread_break = false;

	priv->ip_flags.list_updated = false;

	ip_strm_evt_term(priv);

	stm_fe_free((void **)&priv->ip_data.pull_data_mtx);
	stm_fe_free((void **)&priv->ip_data.start_stop_mtx);
	stm_fe_free((void **)&priv->params);

	stm_fe_spin_lock_bh(&inst_in_use);
	list_for_each_entry_safe(ip_inst, tmp, &ip_insts_list, list) {
		if (priv->ip_id == ip_inst->ip_priv->ip_id) {
			list_del(&ip_inst->list);
			stm_fe_free((void **)&ip_inst);
		}
	}
	stm_fe_spin_unlock_bh(&inst_in_use);

	return ret;
}

/**
* Multicast related functions def start here
*/
static inline bool ipv6_is_multicast(const struct in6_addr *addr)
{
	int addr_type;
	addr_type = ipv6_addr_type(addr);
	if (addr_type & IPV6_ADDR_MULTICAST)
		return true;
	else
		return false;
}

int join_v4_multicast(struct stm_fe_ip_s *priv)
{
	struct ip_mreqn mreqn;
	struct net_device *netdev = NULL;
	struct in_device *indev = NULL;
	int err = -1;

	err = sock_create(AF_INET, SOCK_DGRAM, 0,
						&priv->ip_data.socket);
	if (err < 0)
		pr_err("%s: multicast socket creation error\n", __func__);

	netdev = __dev_get_by_name(&init_net, priv->ip_data.ethdev);
	indev = __in_dev_get_rcu(netdev);

	mreqn.imr_multiaddr.s_addr = priv->ip_data.params.ipaddr;
	mreqn.imr_address.s_addr = indev->ifa_list->ifa_local;
	mreqn.imr_ifindex = 0;

	err = ip_mc_join_group(priv->ip_data.socket->sk, &mreqn);
	if (err < 0) {
		pr_err("%s: ip_mc_join_group failed\n", __func__);
		sock_release(priv->ip_data.socket);
		priv->ip_data.socket = NULL;
	}

	return err;
}

int join_v6_multicast(struct stm_fe_ip_s *priv)
{
	int err = -1;
#ifdef CONFIG_IPV6
        struct ipv6_mreq	mreq;


	err = sock_create(AF_INET6, SOCK_DGRAM, 0,
						&priv->ip_data.socket);
	if (err < 0)
		pr_err("%s: IPv6 multicast socket creation error\n", __func__);

	mreq.ipv6mr_ifindex = 0;
	mreq.ipv6mr_multiaddr = priv->ip_data.params.ipv6addr;

	err = ipv6_sock_mc_join(priv->ip_data.socket->sk,
				mreq.ipv6mr_ifindex, &mreq.ipv6mr_multiaddr);
	if (err < 0) {
		pr_err("%s: ipv6_sock_mc_join failed\n", __func__);
		sock_release(priv->ip_data.socket);
		priv->ip_data.socket = NULL;
	}
#endif
	return err;
}

int leave_multicast(struct stm_fe_ip_s *priv)
{
	int err = -1;

	if (!priv->ip_data.socket)
		return -EFAULT;
	/*ip_mc_leave_group: not exported in kernel: so not called here*/
	sock_release(priv->ip_data.socket);
	priv->ip_data.socket = NULL;

	return err;
}
/**
* Multicast related functions def end here
*/

/*
 * Name: ip_start()
 *
 * Description: start to filter packets
 *
 */
int ip_start(struct stm_fe_ip_s *priv)
{
	uint16_t j = 0;
	int ret = 0;

	update_ip_array(priv);

	if (priv->ip_data.params.is_ipv6 &&
		ipv6_is_multicast(&(priv->ip_data.params.ipv6addr))) {
		ret = join_v6_multicast(priv);
		if (ret) {
			pr_err("%s: IPv6 mcast join failed: %d", __func__, ret);
			return ret;
		}
	} else if (ipv4_is_multicast(priv->ip_data.params.ipaddr)) {
		ret = join_v4_multicast(priv);
		if (ret) {
			pr_err("%s: IPv4 mcast join failed: %d", __func__, ret);
			return ret;
		}
	} else {
		pr_debug("%s: address is not multicast\n", __func__);
	}

	if (priv->ip_flags.start_active) {
		stm_fe_spin_lock_bh(&inst_in_use);
		priv->ip_flags.start_active = false;
		stm_fe_spin_unlock_bh(&inst_in_use);
		for (j = priv->ip_data.wstart; j < priv->ip_data.tail; j++) {
			if (priv->ip_data.inject_buff[j]) {
				kfree_skb(priv->ip_data.inject_buff[j]);
				priv->ip_data.inject_buff[j] = NULL;
			}
		}
	} else {
		stm_fe_mutex_lock(&live_cnt_update_mtx);
		++ip_live_count;
		if (ip_live_count == 1)
			ret = nf_register_hooks(fe_ip_hook_ops,
						    ARRAY_SIZE(fe_ip_hook_ops));
		stm_fe_mutex_unlock(&live_cnt_update_mtx);
	}
	if (ret) {
		pr_err("%s: Error: nf_register_hooks: %x", __func__, ret);
		return -EFAULT;
	}

	if (stm_fe_mutex_lock(priv->ip_data.start_stop_mtx)) {
		pr_info("%s: start_stop_mtx is NULL\n", __func__);
		return -EINVAL;
	}

	/*to enusre clean start the following buffer params are reset*/
	priv->ip_data.tail = 0;
	priv->ip_data.tail_backup = 0;
	priv->ip_data.wstart = 0;
	priv->ip_data.wend = 0;
	priv->first_inject = true;
	set_injection_count(priv);
	ip_reset_stats(priv);
	priv->strm_p->first_pkt_rcvd = false;
	priv->strm_p->last_ts = stm_fe_time_now();

	stm_fe_mutex_unlock(priv->ip_data.start_stop_mtx);

	priv->ip_flags.rcvd_first = false;
	priv->ip_flags.start_active = true;

	priv->start_jiffies = jiffies;
	priv->ip_data.stop_start_gap = stm_fe_jiffies_to_msecs(
				priv->start_jiffies - priv->stop_jiffies);

	return ret;
}

/*
 * Name: ip_stop()
 *
 * Description: stop the filtering of the packets
 *
 */
int ip_stop(struct stm_fe_ip_s *priv)
{
	uint16_t j = 0;
	int ret = 0;
	int num_tries = 20;
	if (!priv->ip_flags.start_active) {
		pr_info("%s: ip fe is already in stop state\n", __func__);
		return 0;
	}

	priv->ip_flags.start_active = false;
	if (priv->ip_data.params.protocol == STM_FE_IP_PROTOCOL_RTP_TS)
		priv->rtp_src.init_rtp = false;

	if (stm_fe_mutex_lock(priv->ip_data.start_stop_mtx)) {
		pr_err("%s: start_stop_mtx is NULL\n", __func__);
		return -EINVAL;
	}

	priv->ip_data.tail = 0;
	priv->ip_data.wstart = 0;
	priv->ip_data.wend = 0;
	priv->first_inject = false;

	if (priv->ipfec_h && priv->ipfec_h->ops->ipfec_stop)
		priv->ipfec_h->ops->ipfec_stop(priv->ipfec_h);

	while (priv->ip_start_stop != true && num_tries != 0) {
			num_tries--;
			FE_WAIT_N_MS(10);
			if (num_tries == 0)
				pr_err("%s: timeout ip_start_stop flag unset\n",
								      __func__);
	};

	for (j = 0; j < IP_FE_BUFF_SIZE; j++) {
		if (priv->ip_data.inject_buff[j]) {
			priv->ip_data.stats.total_pkts_freed++;
			kfree_skb(priv->ip_data.inject_buff[j]);
			priv->ip_data.inject_buff[j] = NULL;
		}
	}
	stm_fe_mutex_unlock(priv->ip_data.start_stop_mtx);

	stm_fe_mutex_lock(&live_cnt_update_mtx);
	--ip_live_count;
	if (!ip_live_count)
		nf_unregister_hooks(fe_ip_hook_ops, ARRAY_SIZE(fe_ip_hook_ops));
	stm_fe_mutex_unlock(&live_cnt_update_mtx);

	if (ipv4_is_multicast(priv->ip_data.params.ipaddr) ||
			ipv6_is_multicast(&(priv->ip_data.params.ipv6addr)))
		leave_multicast(priv);

	priv->stop_jiffies = jiffies;

	return ret;
}

/*
 * Name: ip_set_control()
 *
 * Description: sets the controlling parameters on the run time
 *
 */
int ip_set_control(struct stm_fe_ip_s *priv, stm_fe_ip_control_t value,
								void *args)
{
	int ret = 0;
	const char *pos, *end;
	stm_fe_ip_connection_t *tmp = NULL;
	stm_fe_ip_fec_h ipfec_priv = NULL;

	switch (value) {

	case STM_FE_IP_CTRL_RESET_STATS:
		ip_reset_stats(priv);
		break;

	case STM_FE_IP_CTRL_SET_CONFIG:
		tmp = (stm_fe_ip_connection_t *)args;
		if (!tmp) {
			pr_err("%s: arg is NULL", __func__);
			return -EINVAL;
		}
		if (!priv->params) {
			pr_err("%s: params NULL\n", __func__);
			return -EFAULT;
		}

		priv->params->port_no = tmp->port_no;
		pos = strchr(tmp->ip, '.');
		if (pos) {
			priv->params->ipaddr = in_aton(tmp->ip);
			priv->params->is_ipv6 = false;
		} else {
			in6_pton(tmp->ip, -1, (u8 *)&priv->params->
				ipv6addr.in6_u, IPV6_SCOPE_DELIMITER, &end);
			priv->params->is_ipv6 = true;
		}
		priv->params->protocol = tmp->protocol;
		if (priv->ipfec_h) {
			ipfec_priv = priv->ipfec_h;
			if (ipfec_priv->config->ipfec_scheme ==
						STM_FE_IP_FEC_SCHEME_XOR_COLUMN)
				ipfec_priv->config->ipfec_port
							= tmp->port_no + 2;
			else if (ipfec_priv->config->ipfec_scheme ==
						  STM_FE_IP_FEC_SCHEME_XOR_ROW)
				ipfec_priv->config->ipfec_port
							= tmp->port_no + 4;
		}

		break;

	default:
		ret = -EINVAL;
		pr_err("%s: wrong selector %d", __func__, value);
		break;
	}
	return ret;
}

/*
 * Name: ip_get_control()
 *
 * Description: gets the controlling parameters on the run time
 *
 */
int ip_get_control(struct stm_fe_ip_s *priv,
				stm_fe_ip_control_t selector, void *args)
{
	pr_err("%s: selector %d is not valid\n", __func__, selector);
	return -EINVAL;
}

/*
 * Name: ip_get_stats()
 *
 * Description: gets the stats params during run time
 *
 */
int ip_get_stats(struct stm_fe_ip_s *priv, stm_fe_ip_stat_t *info_p)
{
	int ret = 0;

	info_p->packet_count = priv->ip_data.stats.packet_count;
	info_p->lost_packet_count = priv->ip_data.stats.lost_packet_count;
	info_p->bad_frag_count = priv->ip_data.stats.bad_frag_count;
	info_p->bad_length_count = priv->ip_data.stats.bad_length_count;

	return ret;
}

/*
 * Name: ip_reset_stats()
 *
 * Description: reset the stats params during run time
 *
 */
void ip_reset_stats(struct stm_fe_ip_s *priv)
{
	/* Reset the stats */
	priv->ip_data.stats.lost_packet_count = 0;
	priv->ip_data.stats.total_pkts_freed = 0;
	priv->ip_data.stats.packet_count = 0;
	priv->ip_data.overflow_count = 0;
	priv->ip_data.stats.bad_frag_count = 0;
	priv->ip_data.stats.bad_length_count = 0;
	if (priv->ipfec_h) {
		struct stm_fe_ipfec_s *ipfec;
		ipfec = priv->ipfec_h;

		ipfec->ipfec_data.stats.packet_count = 0;
		ipfec->ipfec_data.stats.lost_packet_count = 0;
		ipfec->ipfec_data.stats.corrected_packet_count = 0;
		ipfec->ipfec_data.stats.rejected_packet_count = 0;
		ipfec->ipfec_data.overflow_count = 0;
	}
}


/*
 * Name: ip_attach_obj()
 *
 * Description: attaches sink interface
 *
 */
int ip_attach_obj(struct stm_fe_ip_s *priv)
{
	int ret = 0;
	int ip_id = 0;

	ip_id = priv->ip_id;
	priv->ip_flags.attach_obj = true;
	priv->ip_data.attach_cnt++;

	return ret;
}
EXPORT_SYMBOL(ip_attach_obj);

/*
 * Name: ip_detach()
 *
 * Description: detaches interface
 *
 */
int ip_detach_obj(struct stm_fe_ip_s *priv)
{
	int ret = 0;
	int ip_id = 0;

	ip_id = priv->ip_id;
	priv->ip_data.attach_cnt--;
	if (!priv->ip_data.attach_cnt)
		priv->ip_flags.attach_obj = false;

	return ret;
}

static int32_t __init stm_fe_ip_init(void)
{
	pr_info("Loading FE IP driver module...\n");
	return 0;
}

module_init(stm_fe_ip_init);

static void __exit stm_fe_ip_term(void)
{
	pr_info("Removing FE IP driver module ...\n");
}

module_exit(stm_fe_ip_term);

MODULE_DESCRIPTION("IP frontend driver");
MODULE_AUTHOR("STMicroelectronics Limited");
MODULE_LICENSE("GPL");
