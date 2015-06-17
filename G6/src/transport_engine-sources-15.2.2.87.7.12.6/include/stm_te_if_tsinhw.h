/******************************************************************************
Copyright (C) 2011, 2012 STMicroelectronics. All Rights Reserved.

This file is part of the Transport Engine Library.

Transport Engine is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License version 2 as published by
the Free Software Foundation.

Transport Engine is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with Transport Engine; see the file COPYING.  If not, write to the Free
Software Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

The Transport Engine Library may alternatively be licensed under a proprietary
license from ST.

Source file name : stm_te_if_tsinhw

Declares the private interface between stm_te demux objects and frontend
objects
******************************************************************************/
#ifndef _STM_TE_IF_TSINHW_H
#define _STM_TE_IF_TSINHW_H

#include "stm_common.h"
#include "stm_te.h"

typedef struct {
	unsigned int tsin_number;
	bool serial_not_parallel;
	bool invert_ts_clk;
	bool async_not_sync;
	bool ts_thru_cablecard; /* The TS is to be routed through cablecard */
	bool ts_merged;         /* The TS contains multiple streams */
	unsigned int ts_tag;    /* Tag value used for cablecard or merged */
	bool use_timer_tag;

	/* Backchannel function pointers for remote PID filtering */
	int (*stm_fe_bc_pid_set)(stm_object_h demod_object,
			stm_object_h demux_object, uint32_t pid);
	int (*stm_fe_bc_pid_clear)(stm_object_h demod_object,
			stm_object_h demux_object, uint32_t pid);

} stm_te_tsinhw_config_t;

typedef struct {
	int (*stm_te_tsinhw_connect_handler)(
		stm_object_h demod_object,
		stm_object_h demux_object,
		stm_te_tsinhw_config_t *tsconfig);
	int (*stm_te_tsinhw_disconnect_handler)(
		stm_object_h demod_object,
		stm_object_h demux_object);
} stm_fe_te_sink_interface_t;

typedef struct {
	/* TBD */
}
stm_te_ip_config_t;

typedef struct {
	int (*stm_ip_te_connect_handler)(
		stm_object_h demod_object,
		stm_object_h demux_object,
		stm_te_ip_config_t *ipconfig);
	int (*stm_ip_te_disconnect_handler)(
		stm_object_h demod_object,
		stm_object_h demux_object);
} stm_ip_te_sink_interface_t;

/*
 * This is the name of the sink interface that must be registered by the
 * frontend component so that the demux object can configure the TSInput when
 * attached to a demod.
 */

#define STM_FE_TE_SINK_INTERFACE    "fe_demod-te-sink-interface"
#define STM_FE_IP_TE_SINK_INTERFACE "fe_ip-te-sink-interface"

#endif /*_STM_TE_IF_TSINHW_H*/
