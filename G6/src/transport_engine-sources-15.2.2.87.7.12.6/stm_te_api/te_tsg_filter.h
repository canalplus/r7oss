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

Source file name : te_tsg_filter.h

Declares te tsg filter functions and types
******************************************************************************/

#ifndef __TE_TSG_FILTER_H
#define __TE_TSG_FILTER_H

#include <linux/semaphore.h>
#include <linux/wait.h>
#include "te_object.h"
#include <stm_te_if_tsmux.h>
#include <mme.h>
#include "te_tsmux.h"
#include "te_internal_cfg.h"

#define MAX_BUF_PER_STREAM	64

enum te_tsg_filter_state {
	TE_TSG_STATE_INVALID = 0x0,
	TE_TSG_STATE_NEW = 0x1,
	TE_TSG_STATE_CONNECTED = 0x2,
	TE_TSG_STATE_STARTED = 0x4,
	TE_TSG_STATE_RUNNING = 0x8,
	TE_TSG_STATE_PAUSED = 0x10,
	TE_TSG_STATE_FLUSHING = 0x20,
	TE_TSG_STATE_STOPPED = 0x40,
	TE_TSG_STATE_DISCONNECTED = 0x80,
};

struct te_tsg_filter {
	struct te_obj obj;

	struct stm_te_async_data_interface_src *async_src_if;
	/* Stream parameters */
	uint32_t	program_id;
	uint32_t	stream_id;
	uint32_t	include_pcr;
	uint32_t	stream_is_pes;
	uint32_t	stream_pid;
	uint32_t	stream_type;
	uint32_t	pes_stream_id;
	uint8_t		stream_id_index;
	uint32_t	stream_descriptor_size;
	char		stream_descriptor[STM_TE_TSMUX_MAX_DESCRIPTOR];
	uint32_t	dts_integrity_threshold;
	uint32_t	bit_buffer_size;
	uint32_t	multiplexaheadlimit;
	uint32_t	dontwaitlimit;
	uint32_t	include_rap;
	uint32_t	dtsduration;
	/*
	 * These are buffers after which flow control will be active
	 * Internal buffers are 64 per tsg filter.
	 * Default value is 32
	 *
	 * When internal buffers are filled upto 32 then flow control
	 * will be active
	 *
	 * In case user sets it to 64, then flow control will be active
	 * from first buffer
	 *
	 * If user set it to zero then no flow control.
	 *
	 * Flow control means, release callback will be given
	 * when there is free space available (more or equal)
	 * internally in tsmux which is specifed by input_buffers
	 *
	 */
	uint32_t	input_buffers;
	/* Internal parameters */
	uint64_t	last_DTS;
	uint64_t	first_DTS;
	/* To store DTS after Full Time Discontinuity (FTD) */
	uint64_t	ftd_first_DTS;
	/* Discontinuity counter for FTD detection */
	uint32_t	dts_discont_cnt;
	/* It is set to true when dts_discont_cnt = 0 */
	bool		full_discont_marker;
	/* Specific for FTD requirement */
	bool		eos_detected;
	uint64_t	native_DTS;
	enum te_tsg_filter_state state;

	/* Connections */
	struct te_obj *tsg_index_filters[TE_MAX_TSG_INDEX_PER_TSG_FILTER];

	stm_object_h external_connection;

	struct list_head tsg_buffer_list;
	struct list_head tsg_mme_buffer_list;
	int		num_buffers;
	uint32_t 	stream_man_paused;
	uint32_t	ignore_first_dts_check;
	int 		ignore_stream_auto_pause;
	int		scattered_stream;
	/* If this flag is set, it will not pause other stream */
	bool	dont_pause_other;
	/* This will check if queue is active for release*/
	bool		queued_buf;
	/* strelay index */
	unsigned int push_relayfs_index;
	/* Purpose of this variable is to store total number
	 * of received frames. It is statistic collection */
	uint64_t total_received_frames;
	/* Purpose of this variable is to store total number of
	 * accepted frames by each tsg. It is for statistic collection */
	uint64_t total_accepted_frames;
};

int te_tsg_filter_new(struct te_obj *tsmux, struct te_obj **new_filter);

struct te_tsg_filter *te_tsg_filter_from_obj(struct te_obj *filter);

int te_tsg_filter_init(struct te_tsg_filter *filter,
		struct te_obj *tsmux_obj,
		char *name, enum te_obj_type_e type);
int te_tsg_filter_deinit(struct te_tsg_filter *filter);

int te_tsg_filter_attach(struct te_obj *obj, stm_object_h target);
int te_tsg_filter_detach(struct te_obj *obj, stm_object_h target);

int te_tsg_filter_stop_all(struct te_obj *tsmux_obj);
#endif
