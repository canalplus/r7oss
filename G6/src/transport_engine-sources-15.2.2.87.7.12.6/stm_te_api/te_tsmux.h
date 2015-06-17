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

Source file name : te_tsmux.h

Declares TE tsmux type and operators
******************************************************************************/

#ifndef TE_TSMUX_H
#define TE_TSMUX_H

#include <linux/list.h>
#include <linux/semaphore.h>
#include <linux/wait.h>
#include <mme.h>
#include "te_object.h"
#include <stm_data_interface.h>
#include "TsmuxTransformerTypes.h"

#define TSMUX_DEFAULT_UNSPECIFIED 0xffffffffU
#define TSMUX_DEFAULT_PCR_PERIOD 4500 /* Was 9000 */
#define TSMUX_DEFAULT_BITRATE (48*1024*1024)
#define TSMUX_DEFAULT_BIT_BUFFER_SIZE (4*1024*1024)
#define TSMUX_DEFAULT_64BIT_UNSPECIFIED 0xffffffffffffffffull
#define TSMUX_DEFAULT_FULL_DISCONT_WAIT_TIME   90000
#define TSMUX_MAX_DTS_DISCONTINUITY_COUNT	3
#define TSMUX_DEFAULT_TSG_INPUT_BUFFERS	32

/* Global flags */
/* Set TS discontinuity indicator for PCR packet
 * like internally generated PCR and Table packets */
#define PCR_DISCONTINUITY_FLAG		0x00000001
/* Set TS discontinuity indicator for Table packets */
#define TABLE_DISCONTINUITY_FLAG	0x00000002

enum te_tsmux_state {
	TE_TSMUX_STATE_INVALID = 0x0,
	TE_TSMUX_STATE_NEW = 0x1,
	TE_TSMUX_STATE_STARTED = 0x2,
	TE_TSMUX_STATE_RUNNING = 0x4,
	TE_TSMUX_STATE_FLUSHING = 0x8,
	TE_TSMUX_STATE_FLUSHED = 0x10,
	TE_TSMUX_STATE_STOPPED = 0x20,
};

struct te_tsmux {

	/* Child filters */
	struct list_head tsg_filters;
	struct list_head pid_ins_filters;
	struct list_head tsg_index_filters;

	/* MME transformer handle */
	bool		transformer_valid;
	MME_TransformerHandle_t transformer_handle;
	TSMUX_CommandStatus_t transform_status;

	/* Wait queue for waiting for enough data on all tsg filters */
	wait_queue_head_t ready_wq;

	/* Global Parameters */
	uint32_t	output_type;
	uint32_t	pcr_period;
	uint32_t	pcr_stream;
	uint32_t	pcr_pid;
	uint32_t	table_gen;
	uint32_t	table_period;
	uint32_t	ts_id;
	uint32_t	bitrate_is_constant;
	uint32_t	bitrate;
	uint32_t	global_flags;
	/* Program parameters */
	uint32_t	program_id;
	uint32_t	program_number;
	uint32_t	pmt_pid;
	char		sdt_prov_name[STM_TE_TSMUX_MAX_DESCRIPTOR+1];
	char		sdt_serv_name[STM_TE_TSMUX_MAX_DESCRIPTOR+1];
	uint32_t	pmt_descriptor_size;
	char		pmt_descriptor[STM_TE_TSMUX_MAX_DESCRIPTOR];
	/* TE control parameters */
	enum stm_te_tsmux_stop_mode_e stop_mode;
	/* Internal parameters */
	uint32_t	next_stream_id;
	uint32_t	next_sec_stream_id;
	uint64_t	min_DTS;
	uint64_t	last_PCR;
	uint64_t	shift_PCR;
	uint32_t	buffer_size;
	uint32_t	memory_used;
	uint32_t	max_memory;
	uint32_t	total_buffers;
	uint32_t	max_buffers;
	uint32_t    full_discont_wait_time;
	/* Required when tsmux is restarted.*/
	bool		restarting;
	/* Required to monitor to restart complete */
	bool		restart_complete;
	uint32_t	num_tsg;

	/* State of tsmux */
	enum te_tsmux_state	state;

	/* Connections to other object and interfaces */
	stm_object_h external_connection;
	stm_data_interface_pull_sink_t pull_intf;
	stm_data_interface_pull_src_t *pull_src_intf;

	/* strelay index */
	unsigned int pull_relayfs_index;
	/* For statistic */
	unsigned int total_ts_packets;
	/* Stores received time stamp of last_DTS
	 * of ES/PES packets w.r.t. ticks */
	uint64_t last_DTS_time_stamp;
	/* The API object variables */
	struct te_obj obj;
};

struct te_tsmux *te_tsmux_from_obj(struct te_obj *obj);
int te_tsmux_get_next_stream_id(struct te_tsmux *tsmux_p,
				enum te_obj_type_e type);

int te_tsmux_new(char *name, struct te_obj **new_tsmux);
int te_tsmux_start(struct te_obj *obj);
int te_tsmux_stop(struct te_obj *obj);

#endif
