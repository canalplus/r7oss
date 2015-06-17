/************************************************************************
 * Copyright (C) 2012 STMicroelectronics. All Rights Reserved.
 *
 * This file is part of the stm_fe Library.
 *
 * stm_fe is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License version 2 as published by the
 * Free Software Foundation.
 *
 * stm_fe is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with stm_fe; see the file COPYING.  If not, write to the Free Software
 * Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * The stm_fe Library may alternatively be licensed under a proprietary
 * license from ST.
 *
 * Source file name :fe_ip_strm.h
 * Author :          SD
 *
 * Low level function FE IP --- stream monitoring
 *
 * Date        Modification                                    Name
 * ----        ------------                                    --------
 *  22-Apr-13   Created                                         SD
 *
 *  ************************************************************************/
#ifndef __IPFE_STRM_H
#define __IPFE_STRM_H

#define FE_IP_DATA_TIMEOUT	50
#define FE_IP_EVT_UNKNOWN	0xff

enum fe_ip_loss_moment_t {
	FE_IP_LOSS_BEF_CORRECTION,
	FE_IP_LOSS_AFT_CORRECTION
};

struct lost_pkt_params_s {
	/*packet counts*/
	uint32_t total_pkt_cnt;
	uint32_t lost_pkt_cnt;
	uint32_t good_pkt_cnt;
	uint32_t severe_loss_pkt_cnt;
	/*first packet*/
	uint32_t first_good_pkt;
	uint32_t evt_first_lost_pkt;
	uint32_t severe_first_lost_pkt;
	/*loss related flags*/
	bool first_packet;
	bool severe_loss;
	bool loss_evt;
	/*time stamp*/
	unsigned long  loss_event_start_time;
	unsigned long  loss_event_end_time;
	unsigned long  severe_loss_start_time;
};

struct evt_params_s {
	uint32_t lost_pkt_cnt;
	unsigned long  loss_event_start_time;
	unsigned long  loss_event_end_time;
};

struct stream_monitor_params {
	struct lost_pkt_params_s *loss_bc_p;
	struct lost_pkt_params_s *loss_ac_p;
	struct evt_params_s	evt_p;
	stm_fe_ip_event_t last_evt;
	bool no_data_after_sto; /*np data after start time out*/
	bool first_pkt_rcvd;
	bool signal_to;
	bool signal_fifo;
	unsigned long gmin;
	unsigned long data_to;
	unsigned long start_to;
	uint32_t sever_loss_min_len;
	unsigned long   *ts_p;
	unsigned long   last_ts;
};

int ip_strm_evt_init(struct stm_fe_ip_s *ip_h);
void ip_strm_evt_term(struct stm_fe_ip_s *ip_h);
void ip_strm_evt_timeout(struct stm_fe_ip_s *ip_h);
void ip_strm_evt_notify_fifo(struct stm_fe_ip_s *ip_h);
#endif
