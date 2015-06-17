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

Source file name : stm_fe_ipfec.h
Author :           SD

Header for stm_fe_ip.c

Date        Modification                                    Name
----        ------------                                    --------
30-Jul-12   Created                                         SD

************************************************************************/
#ifndef _STM_FE_IPFEC_H
#define _STM_FE_IPFEC_H

#include "fe_ip.h"
#include <linux/stm/ipfec.h>
#include<stm_data_interface.h>

/* Private data common to all IP objects */
struct stm_fe_ipfec_obj_s {
	void *priv;
};

struct stm_fe_ipfec_s;

struct ipfec_params {
	struct sk_buff *ipfec_buff[IP_FE_BUFF_SIZE];
	uint16_t sn_base[IP_FE_BUFF_SIZE];
	uint16_t len_rec[IP_FE_BUFF_SIZE];
	uint16_t tail;
	uint16_t head;
	uint16_t search_head; /* kept for optimized search for fec pkts */
	uint16_t row_d;
	uint16_t col_l;
	stm_fe_ip_fec_stat_t stats;
	uint32_t recovery_time_ms;
	uint16_t last_snbase;
	uint16_t overflow_count;
};

struct ipfec_ops_s {
	int (*init)(struct stm_fe_ipfec_s *priv,
				stm_fe_ip_fec_config_t *fec_params);
	int (*term)(struct stm_fe_ipfec_s *priv);
	int (*set_control)(struct stm_fe_ipfec_s *priv,
				stm_fe_ip_fec_control_t value, void *args);
	int (*get_control)(struct stm_fe_ipfec_s *priv,
				stm_fe_ip_fec_control_t value, void *args);
	int (*get_stats)(struct stm_fe_ipfec_s *priv,
					stm_fe_ip_fec_stat_t *info_p);
	int (*fe_ipfec_detach)(struct stm_fe_ipfec_s *priv);
	int (*check_fec)(struct stm_fe_ipfec_s *priv,
						struct rtp_header_s rtp_hdr_p);
	int (*enqueue_fec)(struct sk_buff *ip_fe_buffer,
						struct stm_fe_ipfec_s *priv);
	int (*dequeue_fec)(struct stm_fe_ipfec_s *priv,
						uint16_t ipfe_fend_seq_no);
	int (*recover_with_fec)(struct stm_fe_ipfec_s *priv,
				struct sk_buff **inject_buff,
				struct ipfec_rec_m_pkt *missing_pkt);
	int (*ipfec_stop) (struct stm_fe_ipfec_s *priv);
	int (*ipfec_start) (struct stm_fe_ipfec_s *priv);
};

struct stm_fe_ipfec_s {
	struct list_head list;
	char ipfec_name[IPFEC_NAME_MAX_LEN];
	uint32_t ipfec_id;
	bool fec_enable;
	int matrix_size;
	bool ipfec_start_stop;
	struct ipfec_config_s *config;
	struct ipfec_ops_s *ops;
	struct stm_fe_ip_s *ip_h;
	struct rtp_source rs_fec;
	struct ipfec_params ipfec_data;
};

int stm_fe_ipfec_probe(struct  platform_device *pdev);
int stm_fe_ipfec_remove(struct platform_device *pdev);
struct stm_fe_ipfec_s *ipfec_from_name(const char *name);
struct stm_fe_ipfec_s *ipfec_from_id(uint32_t id);

#endif /* _STM_FE_IP_H */
