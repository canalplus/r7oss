/******************************************************************************
Copyright (C) 2013 STMicroelectronics. All Rights Reserved.

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

Source file name : te_tsg_index_filter.h

TSMUX indexing filter
******************************************************************************/

#ifndef __TE_TSG_INDEX_FILTER_H
#define __TE_TSG_INDEX_FILTER_H

#include <linux/completion.h>
#include "stm_te.h"

struct te_tsg_index_filter {
	struct te_obj obj;

	/* Connections to other object and interfaces */
	stm_object_h external_connection;

	stm_data_interface_pull_sink_t pull_intf;
	stm_data_interface_push_sink_t push_intf;

	stm_data_interface_pull_src_t *pull_src_intf;

	uint32_t index_mask;

	stm_te_tsg_index_data_t *index_buffer_p;
	unsigned int buffer_size; /* Num of stm_te_ts_index_data_t struct */
	unsigned int read_pointer;
	unsigned int write_pointer;
	uint32_t overflows;

	bool detach;

	wait_queue_head_t reader_waitq;
};
struct te_tsg_index_filter *te_tsg_index_filter_from_obj(struct te_obj *obj);
int te_tsg_index_filter_new(struct te_obj *tsmux, struct te_obj **new_filter);
struct te_tsg_index_filter *te_tsg_index_filter_from_obj(struct te_obj *filter);
int te_tsg_index_filter_init(struct te_tsg_index_filter *filter,
		struct te_obj *tsmux_obj,
		char *name, enum te_obj_type_e type);
int te_tsg_index_filter_deinit(struct te_tsg_index_filter *filter);

int te_tsg_index_filter_attach(struct te_obj *obj, stm_object_h target);
int te_tsg_index_filter_detach(struct te_obj *obj, stm_object_h target);
#endif
