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

Source file name : te_in_filter.h

Defines input filter specific operations
******************************************************************************/

#ifndef __TE_IN_FILTER_H
#define __TE_IN_FILTER_H

#include "te_object.h"
#include "te_hal_obj.h"
#include "te_internal_cfg.h"

#include <stm_te_dbg.h>
#include <pti_hal_api.h>
#include <stm_te_if_ce.h>

#include <linux/list.h>

/* Limits */
#define MAX_SLOT_CHAIN       4

/* Max number of output filters that a PID filter may attach to
 *
 * Note that this is only the TE API limit. The actual max number of
 * connections depends on the type of input and output filter */
#define MAX_INT_CONNECTIONS 256

struct te_in_filter {
	struct te_obj obj;

	uint16_t pid;
	uint32_t path_id;

	/* Controls */
	uint32_t flushing_behaviour;

	/* Connections */
	struct te_obj *output_filters[MAX_INT_CONNECTIONS];
	stm_object_h transform;
	stm_ce_path_interface_t iface_path;

	/* HAL objects */
	struct list_head slots;

	/* shared slot for section filters */
	struct te_hal_obj *sf_slot;
};

struct te_in_filter *te_in_filter_from_obj(struct te_obj *filter);

/* Input filter [de]initialiser */
int te_in_filter_init(struct te_in_filter *filter, struct te_obj *demux,
		char *name, enum te_obj_type_e type, uint16_t pid);
int te_in_filter_deinit(struct te_in_filter *filter);

/* Input filter operations */
int te_in_filter_set_control(struct te_in_filter *filter, uint32_t control,
		const void *buffer, uint32_t size);
int te_in_filter_get_control(struct te_in_filter *filter, uint32_t control,
		void *buffer, uint32_t size);
int te_in_filter_attach(struct te_in_filter *filter, stm_object_h target);
int te_in_filter_detach(struct te_in_filter *filter, stm_object_h target);
int te_in_filter_get_compound_control(struct te_in_filter *filter,
		uint32_t control, void *buffer, uint32_t size);

int te_in_filter_update(struct te_in_filter *filter);

bool te_in_filter_has_slot_space(struct te_in_filter *filter);

#endif
