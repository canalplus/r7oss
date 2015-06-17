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

Source file name : te_output_filter.h

Declares te output filter-specific functions and types
******************************************************************************/

#ifndef __TE_OUT_FILTER_H
#define __TE_OUT_FILTER_H

#include <linux/wait.h>
#include "te_object.h"
#include "te_hal_obj.h"
#include "te_buffer_work.h"
#include <stm_data_interface.h>
#include <stm_se.h>
#include <stm_memsink.h>

struct te_out_filter {
	struct te_obj obj;

	/* Data common to all output filters */
	struct te_hal_obj *hal_buffer;
	struct te_hal_obj *hal_filter;
	struct te_hal_obj *hal_index;

	int   buf_size;
	void *buf_start;

	int wr_offs;
	int rd_offs;

	/* Function to get the next read size for this filter */
	int32_t (*next_read_size)(struct te_out_filter *of);

	/* Function to read data from this filter */
	int (*read_bytes)(struct te_out_filter *of, uint8_t *buf,
			uint32_t size, uint32_t *bytes_read);

	/* Buffer workqueue function */
	TE_BWQ_WORK buffer_work_func;

	unsigned int overflow_behaviour;
	unsigned int flushing_behaviour;
	unsigned int error_recovery;
	bool read_quantisation_units;

	stm_object_h external_connection;
	wait_queue_head_t reader_waitq;

	stm_data_interface_pull_sink_t pull_intf;
	stm_data_interface_push_sink_t push_intf;
	stm_se_clock_data_point_interface_push_sink_t pcr_push_intf;

	struct mutex queue_lock;
	struct list_head queued_data;
	uint32_t max_queued_data;
	uint32_t total_queued_data;
	uint32_t lifetime_queued_data;
	stm_data_interface_pull_src_t *pull_src_intf;

	int (*flush)(struct te_out_filter *);
};

struct te_out_filter *te_out_filter_from_obj(struct te_obj *filter);
int te_out_filter_from_hdl(stm_object_h hdl, struct te_out_filter **obj);

int te_out_filter_init(struct te_out_filter *filter, struct te_obj *demux,
		char *name, enum te_obj_type_e type);
int te_out_filter_deinit(struct te_out_filter *filter);

int te_out_filter_attach(struct te_obj *filter, stm_object_h target);
int te_out_filter_detach(struct te_obj *filter, stm_object_h target);

int te_out_filter_queue_data(struct te_out_filter *output, uint8_t *data,
		uint32_t size, bool valid);
int te_out_filter_queue_read(struct te_out_filter *output, uint8_t *buffer,
		uint32_t buffer_size, uint32_t *bytes_read, bool *valid);
int te_out_filter_queue_empty(struct te_out_filter *output);
int te_out_filter_queue_peek(struct te_out_filter *output, uint32_t *size);

int te_out_filter_get_control(struct te_obj *filter, uint32_t control,
				void *buf, uint32_t size);

int te_out_filter_set_control(struct te_obj *filter, uint32_t control,
				const void *buf, uint32_t size);

int te_out_filter_get_compound_control(struct te_obj *filter, uint32_t control,
				void *buf, uint32_t size);

int te_out_filter_set_compound_control(struct te_obj *filter, uint32_t control,
				const void *buf, uint32_t size);

int te_out_filter_connect_input(struct te_obj *out_flt, struct te_obj *in_flt);
int te_out_filter_disconnect_input(struct te_obj *out_flt,
		struct te_obj *in_flt);

int te_out_filter_get_pkt_count(struct te_obj *filter, uint32_t *count);

int te_out_filter_buffer_alloc(struct te_obj *filter,
			struct te_hal_obj **hal_buffer,
			stptiHAL_BufferConfigParams_t *params);

int te_out_filter_wait(struct te_obj *obj);

ST_ErrorCode_t te_out_filter_memcpy(void **dest_pp, const void *src_p,
		size_t size);

void te_out_filter_get_buffer_params(struct te_out_filter *output,
		stptiHAL_BufferConfigParams_t *hal_buf_params);

int te_out_filter_read_bytes(struct te_out_filter *output, uint8_t *buf,
		uint32_t buf_size, uint32_t *bytes_read);

int te_out_filter_get_sizes(struct te_obj *obj, unsigned int *total_size,
						unsigned int *bytes_in_buffer,
						unsigned int *free_space);
void te_out_filter_work(struct te_obj *obj, struct te_hal_obj *buf);
int te_sysfs_print_buffer_overflow_stat(stm_object_h object, char *buf,
			size_t size, char *user_buf, size_t user_size);
int te_out_filter_send_event(struct te_out_filter *output,
		stm_te_filter_event_t event);
int te_out_filter_notify_pull_sink(struct te_out_filter *output,
		stm_memsink_event_t event);

#endif
