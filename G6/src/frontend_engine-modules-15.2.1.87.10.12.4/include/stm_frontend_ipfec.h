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

Source file name : stm_frontend_ipfec.h
Author :           Rahul.V

stm_fe component header

Date        Modification                                    Name
----        ------------                                    --------
16-Jun-11   Created                                         Rahul.V

************************************************************************/

#ifndef _STM_FRONTEND_IPFEC_H
#define _STM_FRONTEND_IPFEC_H

typedef enum stm_fe_ip_fec_scheme_e {
	STM_FE_IP_FEC_SCHEME_XOR_ROW    = (1 << 0),
	STM_FE_IP_FEC_SCHEME_XOR_COLUMN = (1 << 1)
} stm_fe_ip_fec_scheme_t;

typedef struct stm_fe_ip_fec_caps_s {
	stm_fe_ip_fec_scheme_t scheme;
} stm_fe_ip_fec_caps_t;

typedef enum stm_fe_ip_fec_control_e {
	STM_FE_IP_FEC_RESET_STATS,
} stm_fe_ip_fec_control_t;

typedef struct stm_fe_ip_fec_config_s {
	stm_fe_ip_fec_scheme_t scheme;
	uint32_t scheme_port;
	char ip[STM_FE_IP_MAX_IPADDR_LEN];
} stm_fe_ip_fec_config_t;

typedef struct stm_fe_ip_fec_stat_s {
	uint32_t corrected_packet_count;
	uint32_t packet_count;
	uint32_t lost_packet_count;
	uint32_t rejected_packet_count;
} stm_fe_ip_fec_stat_t;

typedef struct stm_fe_ipfec_s *stm_fe_ip_fec_h;

int32_t __must_check stm_fe_ip_fec_new(const char *name, stm_fe_ip_h ip_obj,
			stm_fe_ip_fec_config_t *params, stm_fe_ip_fec_h *obj);
int32_t __must_check stm_fe_ip_fec_delete(stm_fe_ip_fec_h obj);
int32_t __must_check stm_fe_ip_fec_start(stm_fe_ip_fec_h obj);
int32_t __must_check stm_fe_ip_fec_stop(stm_fe_ip_fec_h obj);
int32_t __must_check stm_fe_ip_fec_get_stats(stm_fe_ip_fec_h obj,
					stm_fe_ip_fec_stat_t *info_p);
int32_t __must_check stm_fe_ip_fec_set_compound_control(stm_fe_ip_fec_h obj,
				stm_fe_ip_fec_control_t selector, void *args);
int32_t __must_check stm_fe_ip_fec_get_compound_control(stm_fe_ip_fec_h obj,
				stm_fe_ip_fec_control_t selector, void *args);

#endif /*_STM_FRONTEND_IPFEC_H*/
