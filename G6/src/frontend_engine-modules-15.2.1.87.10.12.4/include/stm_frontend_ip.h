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

Source file name : stm_frontend_ip.h
Author :           Rahul.V

stm_fe component header

Date        Modification                                    Name
----        ------------                                    --------
16-Jun-11   Created                                         Rahul.V

************************************************************************/

#ifndef _STM_FRONTEND_IP_H
#define _STM_FRONTEND_IP_H

typedef enum stm_fe_ip_error_control_type_e {
	STM_FE_IP_ERROR_CONTROL_NONE = (1 << 0),
	STM_FE_IP_ERROR_CONTROL_FEC  = (1 << 1),
	STM_FE_IP_ERROR_CONTROL_RTCP = (1 << 2)
} stm_fe_ip_error_control_type_t;

typedef enum stm_fe_ip_connect_protocol_e {
	STM_FE_IP_PROTOCOL_RTP_TS = (1 << 0),
	STM_FE_IP_PROTOCOL_UDP_TS = (1 << 1)
} stm_fe_ip_connect_protocol_t;

typedef enum stm_fe_ip_control_e {
	STM_FE_IP_CTRL_RESET_STATS,
	STM_FE_IP_CTRL_SET_CONFIG
} stm_fe_ip_control_t;

typedef enum stm_fe_ip_event_e {
	STM_FE_IP_EV_FIRST_PACKET_RECEIVED = (1 << 0),
	STM_FE_IP_EV_RTCP_RETRANS = (1 << 1),
	STM_FE_IP_EV_NO_DATA_AFTER_START_TIMEOUT = (1 << 2),
	STM_FE_IP_EV_DATA_TIMEOUT = (1 << 3),
	STM_FE_IP_EV_FIFO_SUPPLIED = (1 << 4),
	STM_FE_IP_EV_LOSS_EVENT_BEFORE_EC = (1 << 5),
	STM_FE_IP_EV_LOSS_EVENT_AFTER_EC = (1 << 6),
	STM_FE_IP_EV_SEVERE_LOSS_EVENT_BEFORE_EC = (1 << 7),
	STM_FE_IP_EV_SEVERE_LOSS_EVENT_AFTER_EC = (1 << 8)
} stm_fe_ip_event_t;

#define STM_FE_IP_MAX_IPADDR_LEN 40
typedef struct stm_fe_ip_connection_s {
	uint16_t port_no;
	char ip[STM_FE_IP_MAX_IPADDR_LEN];
	stm_fe_ip_connect_protocol_t protocol;
} stm_fe_ip_connection_t;

typedef struct stm_fe_ip_caps_s {
	stm_fe_ip_error_control_type_t ec_types;
	stm_fe_ip_connect_protocol_t protocol;
} stm_fe_ip_caps_t;

typedef struct stm_fe_ip_stat_s {
	uint32_t packet_count;
	uint32_t lost_packet_count;
	uint32_t total_pkts_freed;
	uint32_t bad_frag_count;
	uint32_t bad_length_count;
} stm_fe_ip_stat_t;

#define STM_FE_IP_MAX_EVT_DATA_LEN 20
typedef struct stm_fe_ip_event_data_s {
	stm_fe_ip_event_t event;
	uint8_t data[STM_FE_IP_MAX_EVT_DATA_LEN];
} stm_fe_ip_event_data_t;


typedef struct stm_fe_ip_s *stm_fe_ip_h;

int32_t __must_check stm_fe_ip_new(const char *name, stm_fe_ip_connection_t *p,
							stm_fe_ip_h *obj);
int32_t __must_check stm_fe_ip_delete(stm_fe_ip_h obj);
int32_t __must_check stm_fe_ip_start(stm_fe_ip_h obj);
int32_t __must_check stm_fe_ip_stop(stm_fe_ip_h obj);
int32_t __must_check stm_fe_ip_set_compound_control(stm_fe_ip_h obj,
					stm_fe_ip_control_t sel, void *args);
int32_t __must_check stm_fe_ip_get_compound_control(stm_fe_ip_h obj,
					stm_fe_ip_control_t sel, void *args);
int32_t __must_check stm_fe_ip_get_stats(stm_fe_ip_h obj,
						 stm_fe_ip_stat_t *info);
int32_t __must_check stm_fe_ip_attach(stm_fe_ip_h obj, stm_object_h dest_obj);
int32_t __must_check stm_fe_ip_detach(stm_fe_ip_h obj, stm_object_h dest_obj);
int32_t __must_check stm_fe_ip_get_event_data(stm_fe_ip_h obj,
						stm_fe_ip_event_data_t *data);

#endif /*_STM_FRONTEND_IP_H*/
