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

Source file name : te_pes_filter.c

Defines PES filter specific operations
******************************************************************************/

#include <stm_te_dbg.h>
#include "te_object.h"
#include "te_output_filter.h"
#include "te_pes_filter.h"
#include "te_internal_cfg.h"

struct te_pes_filter {
	uint8_t stream_id_filter;
	struct te_out_filter output;
};

static struct te_pes_filter *te_pes_filter_from_obj(struct te_obj *filter)
{
	if (filter->type == TE_OBJ_PES_FILTER)
		return container_of(filter, struct te_pes_filter,
				output.obj);
	else
		return NULL;
}

uint8_t te_pes_filter_get_stream_id(struct te_obj *filter)
{
	struct te_pes_filter *pes = te_pes_filter_from_obj(filter);
	return pes->stream_id_filter;
}

static int te_pes_filter_update(struct te_pes_filter *pes)
{

	ST_ErrorCode_t Error = ST_NO_ERROR;

	if (pes->output.hal_filter)
		Error = stptiHAL_call(Filter.HAL_FilterUpdate,
				pes->output.hal_filter->hdl,
				stptiHAL_PES_STREAMID_FILTER,
				FALSE, FALSE, &pes->stream_id_filter,
				NULL, NULL);

	if (ST_NO_ERROR != Error) {
		pr_err("Error calling HAL_FilterUpdate Error 0x%x\n", Error);
		return -ENOMEM;
	}

	return 0;
}

int te_pes_filter_get_control(struct te_obj *filter, uint32_t control,
				void *buf, uint32_t size)
{
	int err = 0;
	struct te_pes_filter *pes = te_pes_filter_from_obj(filter);
	unsigned int val;

	switch (control) {
	case STM_TE_PES_FILTER_CONTROL_STREAM_ID_FILTER:
		val = pes->stream_id_filter;
		err = GET_CONTROL(val, buf, size);
		break;
	default:
		err = te_out_filter_get_control(filter, control, buf, size);
	}
	return err;
}

int te_pes_filter_set_control(struct te_obj *filter, uint32_t control,
				const void *buf, uint32_t size)
{
	int err = 0;
	struct te_pes_filter *pes = te_pes_filter_from_obj(filter);
	unsigned int val;

	switch (control) {
	case STM_TE_PES_FILTER_CONTROL_STREAM_ID_FILTER:
		err = SET_CONTROL(val, buf, size);
		if (!err)
			pes->stream_id_filter = (uint8_t)val;
		break;
	default:
		err = te_out_filter_set_control(filter, control, buf, size);
	}

	if (err == 0)
		err = te_pes_filter_update(pes);

	return err;
}

static int te_pes_filter_delete(struct te_obj *filter)
{
	struct te_pes_filter *pes = te_pes_filter_from_obj(filter);
	int err;

	err = te_out_filter_deinit(&pes->output);
	if (err)
		return err;

	kfree(pes);
	return 0;
}

static struct te_obj_ops te_pes_filter_ops = {
	.attach = te_out_filter_attach,
	.detach = te_out_filter_detach,
	.set_control = te_pes_filter_set_control,
	.get_control = te_pes_filter_get_control,
	.delete = te_pes_filter_delete,
};

int te_pes_filter_new(struct te_obj *demux, struct te_obj **new_filter)
{
	int res = 0;
	char name[STM_REGISTRY_MAX_TAG_SIZE];

	struct te_pes_filter *filter = kzalloc(sizeof(*filter), GFP_KERNEL);

	if (!filter) {
		pr_err("couldn't allocate PES filter object\n");
		return -ENOMEM;
	}

	snprintf(name, STM_REGISTRY_MAX_TAG_SIZE, "PESFilter.%p",
				&filter->output.obj);

	res = te_out_filter_init(&filter->output, demux, name,
			TE_OBJ_PES_FILTER);
	if (res != 0)
		goto error;

	/* Initialise TS Index filter data */
	filter->stream_id_filter = TE_PES_STREAM_ID_ALL;
	filter->output.obj.ops = &te_pes_filter_ops;

	*new_filter = &filter->output.obj;
	return 0;

error:
	*new_filter = NULL;
	kfree(filter);

	return res;
}

