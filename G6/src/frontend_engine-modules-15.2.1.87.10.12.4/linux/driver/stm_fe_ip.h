/************************************************************************
Copyright (C) 2011 STMicroelectronics. All Rights Reserved.

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

Source file name : stm_fe_ip.h
Author :           SD

Header for stm_fe_ip.c

Date        Modification                                    Name
----        ------------                                    --------
6-Jul-11   Created                                         SD

************************************************************************/
#ifndef _STM_FE_IP_H
#define _STM_FE_IP_H

#include <linux/stm/ip.h>
#include<stm_data_interface.h>
#include<ip/strm/fe_ip_strm.h>
#include <linux/in6.h>

/* Private data common to all IP objects */
struct stm_fe_ip_obj_s {
	void *priv;
};

struct fe_ip_connection_s {
	uint16_t			port_no;
	uint32_t			ipaddr;
	struct in6_addr			ipv6addr;
	bool				is_ipv6;
	stm_fe_ip_connect_protocol_t	protocol;
};

#define IP_FE_SINK_OBJ_MAX 4
/*! IP source object */
struct stm_ipsrc_s {
	stm_object_h			sink_obj[IP_FE_SINK_OBJ_MAX];
	stm_data_interface_push_sink_t	push_intf;
	stm_data_interface_pull_sink_t	pull_intf;
};

struct ipfe_queued_data {
	struct list_head lh;
	struct	stm_data_block  *data;
	uint32_t size;
	struct sk_buff *ip_buff;
	bool valid;
};

#define STM_IP_FE_BUFF_SIZE 2048
#define STM_IP_FE_BLOCK_INJ_COUNT_MAX 2048

struct ip_params_s {
	struct fe_ip_connection_s params;
	struct stm_ipsrc_s *ip_src;
	struct sk_buff *inject_buff[STM_IP_FE_BUFF_SIZE];
	uint8_t block_count;
	uint8_t attach_cnt;
	struct stm_data_block block_list[STM_IP_FE_BLOCK_INJ_COUNT_MAX];
	uint16_t tail;
	uint16_t tail_backup;
	uint16_t wstart;
	uint16_t wend;
	stm_fe_ip_stat_t stats;
	bool ovf;
	fe_mutex *start_stop_mtx;
	fe_mutex *pull_data_mtx;
	uint16_t last_seq;
	struct socket *socket;
	const char *ethdev;
	uint32_t push_time_ms;
	uint32_t first_inj_latency;
	uint32_t stop_start_gap;
	uint32_t overflow_count;
	uint16_t inj_rate;
	uint32_t qsize;
	uint32_t q_total_data;

};

struct ip_flags_s {
	bool list_updated;
	bool start_active;
	bool attach_obj;
	bool rcvd_first;
};

/*FE IP RTP Handling Params */
/*
 * Per-source state information
 */
struct rtp_source {
	uint16_t max_seq;        /* highest seq. number seen */
	uint32_t cycles;         /* shifted count of seq. number cycles */
	uint32_t base_seq;       /* base seq number */
	uint32_t bad_seq;        /* last 'bad' seq number + 1 */
	uint32_t probation;      /* sequ. packets till source is valid */
	uint32_t received;       /* packets received */
	uint32_t expected_prior; /* packet expected at last interval */
	uint32_t received_prior; /* packet received at last interval */
	uint32_t transit;        /* relative trans time for prev pkt */
	uint32_t jitter;         /* estimated jitter */
	uint32_t ssrc;
	uint16_t seq_offset;
	uint16_t last_rtp_idx;
	uint16_t temp_seq_offset;
	uint16_t seq_no[STM_IP_FE_BUFF_SIZE];
	bool wrap_offset;
	bool init_rtp;
	uint16_t wrap_tail;
       /* ... */
};

/*RTP Specific information ends here*/


struct stm_fe_ip_s {
	struct list_head list;
	char ip_name[IP_NAME_MAX_LEN];
	uint32_t ip_id;
	struct ip_config_s *config;
	struct fe_ip_connection_s *params;
	struct stm_ipsrc_s *ip_src;
	struct stm_ipsink_s *ip_sink;
	struct ip_ops_s *ops;
	fe_thread ip_fe_thread;
	stm_fe_ip_fec_h ipfec_h;
	bool ip_start_stop;
	char *ethdev;
	struct list_head queued_data;
	struct ip_params_s ip_data;
	struct ip_flags_s ip_flags;
	struct rtp_source rtp_src;
	fe_wait_queue_head_t q_event;
	atomic_t doinject;
	bool ip_fe_thread_break;
	bool first_inject;
	uint32_t jitter_conf;
	uint32_t blk_inj_cnt_min;
	uint32_t blk_inj_cnt_max;
	uint32_t tsk_inj_cnt_min;
	uint32_t tsk_inj_cnt_max;
	struct stream_monitor_params *strm_p;
	unsigned long wait_to;
	uint32_t start_jiffies;
	uint32_t stop_jiffies;
};

struct ip_ops_s {
	int (*init)(struct stm_fe_ip_s *priv, stm_fe_ip_connection_t *params);
	int (*term)(struct stm_fe_ip_s *priv);
	int (*start)(struct stm_fe_ip_s *priv);
	int (*stop)(struct stm_fe_ip_s *priv);
	int (*fe_ip_detach)(struct stm_fe_ip_s *priv);
	int (*set_control)(struct stm_fe_ip_s *priv, stm_fe_ip_control_t sel,
								void *args);
	int (*get_control)(struct stm_fe_ip_s *priv, stm_fe_ip_control_t sel,
								void *args);
	int (*get_stats)(struct stm_fe_ip_s *priv, stm_fe_ip_stat_t *info_p);
	int (*attach_obj)(struct stm_fe_ip_s *priv);
	int (*detach_obj)(struct stm_fe_ip_s *priv);
	int (*wait_for_data)(struct stm_fe_ip_s *priv, uint32_t *data);
	int (*readnbytes)(struct stm_fe_ip_s *priv,
		struct stm_data_block *addr, uint32_t *len, uint32_t *bytes);
	int (*get_queue_len)(struct stm_fe_ip_s *priv, uint32_t *size);
};

int stm_fe_ip_probe(struct  platform_device *pdev);
int stm_fe_ip_remove(struct platform_device *pdev);
struct stm_fe_ip_s *ip_from_name(const char *name);
struct stm_fe_ip_s *ip_from_id(uint32_t id);

#endif /* _STM_FE_IP_H */
