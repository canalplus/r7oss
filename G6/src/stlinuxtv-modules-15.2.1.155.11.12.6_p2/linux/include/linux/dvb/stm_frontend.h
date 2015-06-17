/************************************************************************
Copyright (C) 2012 STMicroelectronics. All Rights Reserved.

This file is part of the STLinuxTV Library.

STLinuxTV is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License version 2 as published by the
Free Software Foundation.

STLinuxTV is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with STLinuxTV; see the file COPYING.  If not, write to the Free Software
Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

The STLinuxTV Library may alternatively be licensed under a proprietary
license from ST.

Source file name : stm_frontend.h

Extensions to the LinuxDVB API for IP Frontend(v3)

Date        Modification                                    Name
----        ------------                                    --------

 ************************************************************************/

#ifndef H_STM_FRONTEND
#define H_STM_FRONTEND

#include <linux/types.h>

#define FE_IPFE 0x1000

typedef enum fe_ip_connect_protocol_e {
	FE_IP_PROTOCOL_RTP_TS = (1 << 0),
	FE_IP_PROTOCOL_UDP_TS = (1 << 1)
} fe_ip_connect_protocol_t;

typedef enum fe_ip_error_control_type_e {
	FE_IP_ERROR_CONTROL_NONE = (1 << 0),
	FE_IP_ERROR_CONTROL_FEC = (1 << 1),
	FE_IP_ERROR_CONTROL_RTCP = (1 << 2)
} fe_ip_error_control_type_t;

typedef enum fe_ip_control_e {
	FE_IP_CTRL_RESET_STATS,
	FE_IP_CTRL_SET_CONFIG
} fe_ip_control_t;

typedef enum fe_ip_fec_scheme_e {

	FE_IP_FEC_SCHEME_XOR_ROW = (1<<0),
	FE_IP_FEC_SCHEME_XOR_COLUMN =(1<<1)

} fe_ip_fec_scheme_t;

typedef enum fe_ip_fec_control_e {
	FE_IP_FEC_RESET_STATS,
} fe_ip_fec_control_t;

typedef enum fe_ip_rtcp_control_e {
	STM_FE_IP_RTCP_CTRL_RESET_STATS
} fe_ip_rtcp_control_t;

#define FE_IP_MAX_IPADDR_LEN 40
struct fe_ip_parameters {
	__u16 port_no;
	char ip[FE_IP_MAX_IPADDR_LEN];
	fe_ip_connect_protocol_t protocol;
	__u32 flags;
#define FE_IP_IMMEDIATE_START 1
};

struct fe_ip_caps {
	fe_ip_error_control_type_t ec_type;
	fe_ip_connect_protocol_t protocol;
};

struct fe_ip_fec_config_s{
	fe_ip_fec_scheme_t	scheme;
	unsigned int		scheme_port;
	char	ip [FE_IP_MAX_IPADDR_LEN];
};

struct fe_ip_fec_stat_s{
	unsigned int  corrected_packet_count;
	unsigned int  packet_count;
	unsigned int  lost_packet_count;
	unsigned int  rejected_packet_count;
};

struct fe_ip_rtcp_config_s{
	unsigned short  port_no;
	char      ip [FE_IP_MAX_IPADDR_LEN];
	unsigned int  bandwidth;
	unsigned short  rxtime;
	unsigned short  N;
	unsigned short  D;
	unsigned short  P;
};

struct fe_ip_rtcp_stat_s{
	unsigned int  min_burst_size;
	unsigned int  max_burst_size;
	unsigned int  corrected_packet_count;
	unsigned int  packet_count;
	unsigned int  lost_packet_count;
	unsigned int  rejected_packet_count;
};

#define FE_IP_GET_CAPS            _IOR('o', 154, struct fe_ip_caps)
#define FE_IP_SET_FILTER          _IOW('o', 156, struct fe_ip_parameters)
#define FE_IP_START               _IO('o', 157)
#define FE_IP_STOP                _IO('o', 158)
#define FE_IP_SET_FILTER_FEC      _IOW('o', 159, struct fe_ip_fec_config_s)
#define FE_IP_SET_FILTER_RTCP     _IOW('o', 160, struct fe_ip_rtcp_config_s)
#define FE_IP_FEC_START           _IO('o', 161)
#define FE_IP_FEC_STOP            _IO('o', 162)

#endif /*H_STM_FRONTEND */
