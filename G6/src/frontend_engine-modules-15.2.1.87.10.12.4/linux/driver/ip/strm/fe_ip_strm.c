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
 * Source file name :fe_ip_strm.c
 * Author :          SD
 *
 * Low level function FE IP --- stream monitoring
 *
 * Date        Modification                                    Name
 * ----        ------------                                    --------
 *  22-Apr-13   Created                                         SD
 *
 *  ************************************************************************/

#include <linux/bug.h>
#include <linux/kthread.h>
#include <linux/platform_device.h>
#include <linux/stm/plat_dev.h>
#include <linux/stm/demod.h>
#include <linux/stm/demod.h>
#include <linux/stm/lnb.h>
#include <linux/stm/diseqc.h>
#include <stm_registry.h>
#include <stm_fe.h>
#include <stm_fe_os.h>
#include <stm_fe_ip.h>
#include <stm_registry.h>
#include <linux/log2.h>
#include "fe_ip_strm.h"


static void ip_strm_evt_notify(struct stm_fe_ip_s *ip_h,
						stm_fe_ip_event_t ip_fe_event);

/*
 *ip_strm_evt_init: initialize ip stream monitoring event parameter
 */

int ip_strm_evt_init(struct stm_fe_ip_s *ip_h)
{
	uint16_t idx = 0;
	idx = ip_h->ip_id;

	ip_h->strm_p = stm_fe_malloc(sizeof(struct stream_monitor_params));
	if (!ip_h->strm_p) {
		pr_err("%s: strm_p mem alloc failed\n", __func__);
		return -ENOMEM;
	}
	ip_h->strm_p->loss_bc_p
			= stm_fe_malloc(sizeof(struct lost_pkt_params_s));
	if (!ip_h->strm_p->loss_bc_p) {
		pr_err("%s: loss_bc_p mem alloc failed\n", __func__);
		stm_fe_free((void **)&ip_h->strm_p);
		return -ENOMEM;
	}
	ip_h->strm_p->loss_ac_p
			= stm_fe_malloc(sizeof(struct lost_pkt_params_s));
	if (!ip_h->strm_p->loss_ac_p) {
		pr_err("%s: loss_ac_p mem alloc failed\n", __func__);
		stm_fe_free((void **)&ip_h->strm_p->loss_bc_p);
		stm_fe_free((void **)&ip_h->strm_p);
		return -ENOMEM;
	}

	/*initialize common stream monitoring parameter*/
	ip_h->strm_p->last_evt = 0xff;
	ip_h->strm_p->first_pkt_rcvd = false;
	ip_h->strm_p->signal_to = false;
	ip_h->strm_p->signal_fifo = false;

	/*initialize params for before correction*/
	ip_h->strm_p->loss_bc_p->total_pkt_cnt = 0;
	ip_h->strm_p->loss_bc_p->lost_pkt_cnt = 0;
	ip_h->strm_p->loss_bc_p->good_pkt_cnt = 0;
	ip_h->strm_p->loss_bc_p->severe_loss_pkt_cnt = 0;
	ip_h->strm_p->loss_bc_p->first_packet = false;
	ip_h->strm_p->loss_bc_p->severe_loss = false;
	ip_h->strm_p->loss_bc_p->loss_evt =  false;

	/*initialize params for after correction*/
	ip_h->strm_p->loss_ac_p->total_pkt_cnt = 0;
	ip_h->strm_p->loss_ac_p->lost_pkt_cnt = 0;
	ip_h->strm_p->loss_ac_p->good_pkt_cnt = 0;
	ip_h->strm_p->loss_ac_p->severe_loss_pkt_cnt = 0;
	ip_h->strm_p->loss_ac_p->first_packet = false;
	ip_h->strm_p->loss_ac_p->severe_loss = false;
	ip_h->strm_p->loss_ac_p->loss_evt =  false;

	/*initialize time stamps*/
	ip_h->strm_p->start_to = msecs_to_jiffies(FE_IP_DATA_TIMEOUT);
	ip_h->strm_p->data_to = msecs_to_jiffies(FE_IP_DATA_TIMEOUT);
	ip_h->strm_p->ts_p =
			stm_fe_malloc(STM_IP_FE_BUFF_SIZE * sizeof(time_t));
	if (!ip_h->strm_p->ts_p) {
		pr_err("%s: ts_p mem alloc failed\n", __func__);
		stm_fe_free((void **)&ip_h->strm_p->loss_ac_p);
		stm_fe_free((void **)&ip_h->strm_p->loss_bc_p);
		stm_fe_free((void **)&ip_h->strm_p);
		return -ENOMEM;
	}
	for (idx = 0; idx < STM_IP_FE_BUFF_SIZE; idx++)
		ip_h->strm_p->ts_p[idx] = 0;

	/*initialize timeout parameters*/
	return 0;
}

/*
 *ip_strm_evt_term: deallocate stream monitoring event parameter dynamic
 *structures
 */

void ip_strm_evt_term(struct stm_fe_ip_s *ip_h)
{

	stm_fe_free((void **)&ip_h->strm_p->ts_p);
	stm_fe_free((void **)&ip_h->strm_p->loss_ac_p);
	stm_fe_free((void **)&ip_h->strm_p->loss_bc_p);
	stm_fe_free((void **)&ip_h->strm_p);

	return;
}

#ifdef STM_FE_DEFINED_FOR_FUTURE_USE
/*
 * ip_strm_lost_pkt: determines lost packet
 *				first lost packet
 *				severe lost packet
 *				first severe lost packet
 * */
void ip_strm_lost_pkt(struct stm_fe_ip_s *ip_h, uint16_t idx,
			enum fe_ip_loss_moment_t lm)
{
	uint32_t lost_pkt_cnt = 0, evt_first_lost_pkt = 0;
	uint32_t severe_lost_pkt_cnt = 0, severe_first_lost_pkt = 0;
	struct lost_pkt_params_s *loss_p = NULL;


	if (lm == FE_IP_LOSS_BEF_CORRECTION)
		loss_p = ip_h->strm_p->loss_bc_p;
	else if (lm == FE_IP_LOSS_AFT_CORRECTION)
		loss_p = ip_h->strm_p->loss_ac_p;

	lost_pkt_cnt = loss_p->lost_pkt_cnt;
	evt_first_lost_pkt = loss_p->evt_first_lost_pkt;
	severe_first_lost_pkt = loss_p->severe_first_lost_pkt;
	severe_lost_pkt_cnt = loss_p->severe_loss_pkt_cnt;

	/*check if it is first lost pkt*/
	if (!evt_first_lost_pkt) {
		evt_first_lost_pkt = idx;
		loss_p->loss_event_start_time = ip_h->strm_p->ts_p[idx];
	}
	/*check if it is first severe loss pkt*/
	if (!severe_first_lost_pkt) {
		severe_first_lost_pkt = idx;
		loss_p->severe_loss_start_time = ip_h->strm_p->ts_p[idx];
	}
	/*check if severe loss has happened already*/
	if ((severe_first_lost_pkt + severe_lost_pkt_cnt - 1)
					 % STM_IP_FE_BUFF_SIZE == idx)
		if (severe_lost_pkt_cnt > ip_h->strm_p->sever_loss_min_len)
			loss_p->severe_loss = true;

	loss_p->lost_pkt_cnt = lost_pkt_cnt;
	loss_p->evt_first_lost_pkt = evt_first_lost_pkt;
	loss_p->severe_first_lost_pkt = severe_first_lost_pkt;
	loss_p->severe_loss_pkt_cnt = severe_lost_pkt_cnt;
}

/*
 *ip_strm_evt_notify_loss: notifies loss event
 */
void ip_strm_evt_notify_loss(struct stm_fe_ip_s *ip_h, stm_fe_ip_event_t ev)
{
	uint32_t start_time = 0, end_time = 0, good_pkt_cnt = 0;
	uint32_t total_pkt_cnt = 0, lost_pkt_cnt = 0;

	switch (ev) {
	case STM_FE_IP_EV_LOSS_EVENT_BEFORE_EC:
		start_time = ip_h->strm_p->loss_bc_p->loss_event_start_time;
		end_time = ip_h->strm_p->loss_bc_p->loss_event_end_time;
		good_pkt_cnt = ip_h->strm_p->loss_bc_p->good_pkt_cnt;
		total_pkt_cnt = ip_h->strm_p->loss_bc_p->total_pkt_cnt;
		lost_pkt_cnt = ip_h->strm_p->loss_bc_p->lost_pkt_cnt;
	break;
	case STM_FE_IP_EV_LOSS_EVENT_AFTER_EC:
		start_time = ip_h->strm_p->loss_ac_p->loss_event_start_time;
		end_time = ip_h->strm_p->loss_ac_p->loss_event_end_time;
		good_pkt_cnt = ip_h->strm_p->loss_ac_p->good_pkt_cnt;
		total_pkt_cnt = ip_h->strm_p->loss_ac_p->total_pkt_cnt;
		lost_pkt_cnt = ip_h->strm_p->loss_ac_p->lost_pkt_cnt;
	break;
	case STM_FE_IP_EV_SEVERE_LOSS_EVENT_BEFORE_EC:
		start_time = ip_h->strm_p->loss_bc_p->severe_loss_start_time;
		end_time = ip_h->strm_p->loss_bc_p->loss_event_end_time;
		good_pkt_cnt = 1; /*ip_h->strm_p->loss_bc_p->good_pkt_cnt;*/
		total_pkt_cnt = ip_h->strm_p->loss_bc_p->severe_loss_pkt_cnt;
		lost_pkt_cnt = ip_h->strm_p->loss_bc_p->severe_loss_pkt_cnt;
	break;
	case STM_FE_IP_EV_SEVERE_LOSS_EVENT_AFTER_EC:
		start_time = ip_h->strm_p->loss_ac_p->loss_event_start_time;
		end_time = ip_h->strm_p->loss_ac_p->loss_event_end_time;
		good_pkt_cnt = 1; /*ip_h->strm_p->loss_bc_p->good_pkt_cnt;*/
		total_pkt_cnt = ip_h->strm_p->loss_ac_p->severe_loss_pkt_cnt;
		lost_pkt_cnt = ip_h->strm_p->loss_ac_p->severe_loss_pkt_cnt;
	break;
	default:
		return;
	break;
	}

	/*fill the event parameters*/

	/*notify the event*/
	/*ip_strm_evt_notify();*/

}

/*
 *ip_strm_evt_notify_bc: notifies event before correction
 */
void ip_strm_evt_notify_bc(struct stm_fe_ip_s *ip_h, uint32_t idx)
{
	uint32_t lost_pkt_cnt = ip_h->strm_p->loss_bc_p->lost_pkt_cnt;
	uint32_t good_pkt_cnt = ip_h->strm_p->loss_bc_p->good_pkt_cnt;
	uint32_t total_pkt_cnt = ip_h->strm_p->loss_bc_p->total_pkt_cnt;
	uint32_t first_good_pkt = ip_h->strm_p->loss_bc_p->first_good_pkt;

	if (idx < first_good_pkt)
		return;
	total_pkt_cnt++;
	good_pkt_cnt++;
	ip_h->strm_p->loss_bc_p->loss_event_end_time = ip_h->strm_p->ts_p[idx];

	if (ip_h->strm_p->loss_bc_p->severe_loss) {
		if (ip_h->strm_p->loss_bc_p->severe_loss_pkt_cnt) {
			ip_strm_evt_notify_loss(ip_h,
				STM_FE_IP_EV_SEVERE_LOSS_EVENT_BEFORE_EC);
			ip_h->strm_p->loss_bc_p->severe_loss = false;
			ip_h->strm_p->loss_bc_p->loss_evt = false;
			ip_h->strm_p->loss_bc_p->evt_first_lost_pkt = 0;
			good_pkt_cnt = 0;
			total_pkt_cnt = 0;
			lost_pkt_cnt = 0;
		}
	}

	if (good_pkt_cnt > ip_h->strm_p->gmin) {
		if (lost_pkt_cnt) {
			ip_strm_evt_notify_loss(ip_h,
				 STM_FE_IP_EV_LOSS_EVENT_BEFORE_EC);
			ip_h->strm_p->loss_bc_p->loss_evt = false;
			ip_h->strm_p->loss_bc_p->evt_first_lost_pkt = 0;
			good_pkt_cnt = 0;
			total_pkt_cnt = 0;
			lost_pkt_cnt = 0;
		}
	}

	ip_h->strm_p->loss_bc_p->severe_first_lost_pkt = 0;
	ip_h->strm_p->loss_bc_p->severe_loss_pkt_cnt = 0;

	ip_h->strm_p->loss_bc_p->lost_pkt_cnt = lost_pkt_cnt;
	ip_h->strm_p->loss_bc_p->good_pkt_cnt = good_pkt_cnt;
	ip_h->strm_p->loss_bc_p->total_pkt_cnt = total_pkt_cnt;

}

/*
 *ip_strm_evt_notify_ac: notifies event after correction
 */
void ip_strm_evt_notify_ac(struct stm_fe_ip_s *ip_h, uint32_t idx)
{
	uint32_t lost_pkt_cnt = ip_h->strm_p->loss_ac_p->lost_pkt_cnt;
	uint32_t good_pkt_cnt = ip_h->strm_p->loss_ac_p->good_pkt_cnt;
	uint32_t total_pkt_cnt = ip_h->strm_p->loss_ac_p->total_pkt_cnt;

	total_pkt_cnt++;
	good_pkt_cnt++;

	ip_h->strm_p->loss_ac_p->loss_event_end_time = ip_h->strm_p->ts_p[idx];

	if (ip_h->strm_p->loss_ac_p->severe_loss) {
		if (ip_h->strm_p->loss_ac_p->severe_loss_pkt_cnt) {
			ip_strm_evt_notify_loss(ip_h,
				STM_FE_IP_EV_LOSS_EVENT_AFTER_EC);
			ip_h->strm_p->loss_ac_p->severe_loss = false;
			ip_h->strm_p->loss_ac_p->loss_evt = false;
			ip_h->strm_p->loss_ac_p->evt_first_lost_pkt = 0;
			good_pkt_cnt = 0;
			total_pkt_cnt = 0;
			lost_pkt_cnt = 0;
		}
	}

	if (good_pkt_cnt > ip_h->strm_p->gmin) {
		if (lost_pkt_cnt) {
			ip_strm_evt_notify_loss(ip_h,
				STM_FE_IP_EV_LOSS_EVENT_AFTER_EC);
			ip_h->strm_p->loss_ac_p->loss_evt = false;
			ip_h->strm_p->loss_ac_p->evt_first_lost_pkt = 0;
			good_pkt_cnt = 0;
			total_pkt_cnt = 0;
			lost_pkt_cnt = 0;
		}
	}

	ip_h->strm_p->loss_ac_p->severe_first_lost_pkt = 0;
	ip_h->strm_p->loss_ac_p->severe_loss_pkt_cnt = 0;

	ip_h->strm_p->loss_ac_p->lost_pkt_cnt = lost_pkt_cnt;
	ip_h->strm_p->loss_ac_p->good_pkt_cnt = good_pkt_cnt;
	ip_h->strm_p->loss_ac_p->total_pkt_cnt = total_pkt_cnt;

}
#endif

/*
 *ip_strm_evt_notify_fifo: notifies fifo
 */
void ip_strm_evt_notify_fifo(struct stm_fe_ip_s *ip_h)
{
	if (ip_h->strm_p->last_evt == STM_FE_IP_EV_DATA_TIMEOUT
		|| ip_h->strm_p->last_evt
			== STM_FE_IP_EV_NO_DATA_AFTER_START_TIMEOUT) {
		ip_h->strm_p->last_evt = STM_FE_IP_EV_FIFO_SUPPLIED;
		ip_strm_evt_notify(ip_h, STM_FE_IP_EV_FIFO_SUPPLIED);
	}
}

static struct timeval fe_ip_get_ts(time_t t1, time_t t2)
{
	time_t t;
	struct timeval t_val;

	t = stm_fe_time_plus(t1, t2);
	jiffies_to_timeval((unsigned long)t, &t_val);

	return t_val;

}
/*
 *ip_strm_evt_notify_timeout: notifies timeout event
 */
void ip_strm_evt_notify_timeout(struct stm_fe_ip_s *ip_h, stm_fe_ip_event_t ev)
{
	struct timeval t_val;

	switch (ev) {
	case STM_FE_IP_EV_DATA_TIMEOUT:
		ip_h->strm_p->last_evt = STM_FE_IP_EV_DATA_TIMEOUT;
		t_val = fe_ip_get_ts(ip_h->strm_p->last_ts,
							ip_h->strm_p->start_to);
	break;
	case STM_FE_IP_EV_NO_DATA_AFTER_START_TIMEOUT:
		ip_h->strm_p->last_evt
				 = STM_FE_IP_EV_NO_DATA_AFTER_START_TIMEOUT;
		t_val = fe_ip_get_ts(ip_h->strm_p->last_ts,
						 ip_h->strm_p->data_to);
	break;
	case STM_FE_IP_EV_FIFO_SUPPLIED:
		ip_h->strm_p->last_evt = STM_FE_IP_EV_FIFO_SUPPLIED;
		t_val = fe_ip_get_ts(stm_fe_time_now(), 0);
	break;
	default:
	break;
	}

	/*fill up t_val in the event params*/
	ip_strm_evt_notify(ip_h, ev);
}

/*
*ip_strm_evt_timeout: checks for event timeout notification
*/
void ip_strm_evt_timeout(struct stm_fe_ip_s *ip_h)
{

	if (!ip_h->strm_p->first_pkt_rcvd &&
		ip_h->strm_p->last_evt == FE_IP_EVT_UNKNOWN) {
		if (ip_h->strm_p->start_to) {
			if (ip_h->strm_p->last_ts) {
				if (stm_fe_time_minus(stm_fe_time_now(),
					ip_h->strm_p->last_ts) >
					ip_h->strm_p->start_to) {
					ip_strm_evt_notify_timeout(ip_h,
				STM_FE_IP_EV_NO_DATA_AFTER_START_TIMEOUT);
				}
			}
		}
	} else if ((ip_h->strm_p->last_evt == STM_FE_IP_EV_FIFO_SUPPLIED ||
		ip_h->strm_p->last_evt == STM_FE_IP_EV_FIRST_PACKET_RECEIVED)
		&& (ip_h->strm_p->first_pkt_rcvd)) {
		if (ip_h->strm_p->data_to) {
			if (stm_fe_time_minus(stm_fe_time_now(),
			ip_h->strm_p->last_ts) > ip_h->strm_p->data_to) {
				ip_strm_evt_notify_timeout(ip_h,
					STM_FE_IP_EV_DATA_TIMEOUT);
			}
		}
	}

}

void ip_strm_evt_notify(struct stm_fe_ip_s *ip_h, stm_fe_ip_event_t ip_fe_event)
{
	int ret = -1;
	stm_event_t event;

	event.event_id = ip_fe_event;
	event.object = (stm_fe_ip_h) ip_h;
	ret = stm_event_signal(&event);
	if (ret)
		pr_err("%s: stm_event_signal failed\n", __func__);

	return;
}
