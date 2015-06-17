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

Source file name : te_ts_filter.c

Defines TS filter specific operations
******************************************************************************/

#include <stm_te_dbg.h>
#include <te_object.h>
#include <pti_hal_api.h>
#include <te_interface.h>
#include <te_input_filter.h>
#include <te_output_filter.h>

/* Filter structure is private to operators */

struct te_ts_filter {
	struct te_out_filter output;

	bool dlna_formatted_output;
	bool security_formatted_output;
};

static struct te_ts_filter *te_ts_filter_from_obj(struct te_obj *filter)
{
	if (TE_OBJ_TS_FILTER == filter->type)
		return container_of(filter, struct te_ts_filter,
				output.obj);
	else
		return NULL;
}

/*!
 * \brief Updates the HAL objects associated with an existing stm_te TS filter
 *
 * \param filter Te object to update the HAL objects for
 *
 * \retval 0    Success
 * \retval -EIO HAL error
 */
static int te_ts_filter_update_hal(struct te_ts_filter *ts_filter)
{
	FullHandle_t *slot_handles;
	ST_ErrorCode_t hal_err;
	int num_slots;
	int i;
	uint32_t security_output;
	BOOL dlna;
	int err = 0;

	if (!ts_filter->output.hal_buffer)
		return 0; /* nothing to update */

	slot_handles = kmalloc(sizeof(FullHandle_t) * MAX_INT_CONNECTIONS,
			GFP_KERNEL);
	if (!slot_handles) {
		pr_err("No memory for slot handle array\n");
		return -ENOMEM;
	}

	/* Get the list of slots attached to this buffer */
	num_slots = stptiOBJMAN_ReturnAssociatedObjects(
			ts_filter->output.hal_buffer->hdl,
			slot_handles, MAX_INT_CONNECTIONS, OBJECT_TYPE_SLOT);

	if (ts_filter->security_formatted_output)
		security_output = stptiHAL_SECUREPATH_OUTPUT_NODE_SCRAMBLED;
	else
		security_output = stptiHAL_SECUREPATH_OUTPUT_NODE_CLEAR;

	if (ts_filter->dlna_formatted_output)
		dlna = TRUE;
	else
		dlna = FALSE;

	for (i = 0; i < num_slots; i++) {
		hal_err = stptiHAL_call(Slot.HAL_SlotSetSecurePathOutputNode,
				slot_handles[i], security_output);

		if (ST_NO_ERROR != hal_err) {
			pr_err("HAL_SlotSetSecurePathOutputNode error 0x%x\n",
					hal_err);
			err = te_hal_err_to_errno(hal_err);
		}

		hal_err = stptiHAL_call(Slot.HAL_SlotFeatureEnable,
				slot_handles[i],
				stptiHAL_SLOT_OUTPUT_DNLA_TS_TAG, dlna);
		if (ST_NO_ERROR != hal_err) {
			pr_err("HAL_SlotFeatureEnable error 0x%x\n", hal_err);
			err = -EIO;
		}
	}

	kfree(slot_handles);
	return err;
}

static int te_ts_filter_delete(struct te_obj *filter)
{
	struct te_ts_filter *ts_filter = te_ts_filter_from_obj(filter);
	int err;

	err = te_out_filter_deinit(&ts_filter->output);
	if (err)
		return err;

	kfree(ts_filter);
	return 0;
}

int te_obj_ts_filter_get_control(struct te_obj *filter, uint32_t control,
					void *buf, uint32_t size)
{
	int err = 0;

	struct te_ts_filter *ts_filter = te_ts_filter_from_obj(filter);

	switch (control) {
	case STM_TE_TS_FILTER_CONTROL_DLNA_OUTPUT:
		if (size == sizeof(int))
			*(uint32_t *)buf = ts_filter->dlna_formatted_output;
		break;
	case STM_TE_TS_FILTER_CONTROL_SECURE_OUTPUT:
		if (size == sizeof(int))
			*(uint32_t *)buf = ts_filter->security_formatted_output;
		break;
	default:
		err = te_out_filter_get_control(filter, control, buf, size);
	}
	return err;
}

int te_obj_ts_filter_set_control(struct te_obj *filter, uint32_t control,
					const void *buf, uint32_t size)
{
	int err = 0;
	struct te_ts_filter *ts_filter = te_ts_filter_from_obj(filter);

	switch (control) {
	case STM_TE_TS_FILTER_CONTROL_DLNA_OUTPUT:
		if (size == sizeof(int))
			ts_filter->dlna_formatted_output =
				(*(uint32_t *)buf ? true : false);
		break;
	case STM_TE_TS_FILTER_CONTROL_SECURE_OUTPUT:
		if (size == sizeof(int))
			ts_filter->security_formatted_output =
				(*(uint32_t *)buf ? true : false);
		break;
	default:
		err = te_out_filter_set_control(filter, control, buf, size);
	}

	if (!err)
		err = te_ts_filter_update_hal(ts_filter);

	return err;
}

static struct te_obj_ops te_ts_filter_ops = {
	.attach = te_out_filter_attach,
	.detach = te_out_filter_detach,
	.set_control = te_obj_ts_filter_set_control,
	.get_control = te_obj_ts_filter_get_control,
	.delete = te_ts_filter_delete,
};

int te_ts_filter_new(struct te_obj *demux, struct te_obj **new_filter)
{
	int res = 0;
	char name[STM_REGISTRY_MAX_TAG_SIZE];

	struct te_ts_filter *filter = kzalloc(sizeof(*filter), GFP_KERNEL);

	if (!filter) {
		pr_err("couldn't allocate TS filter object\n");
		res = -ENOMEM;
		goto error;
	}

	snprintf(name, STM_REGISTRY_MAX_TAG_SIZE, "TSFilter.%p",
				&filter->output.obj);
	res = te_out_filter_init(&filter->output, demux, name,
			TE_OBJ_TS_FILTER);
	if (res != 0)
		goto error;

	/* Initialise TS filter data */
	filter->dlna_formatted_output =	false;
	filter->security_formatted_output = true;

	filter->output.obj.ops = &te_ts_filter_ops;

	*new_filter = &filter->output.obj;
	return 0;

error:
	*new_filter = NULL;
	kfree(filter);

	return res;
}

bool te_ts_filter_is_security_formatted(struct te_obj *filter)
{
	struct te_ts_filter *ts = te_ts_filter_from_obj(filter);

	if (ts)
		return ts->security_formatted_output;
	return false;
}

bool te_ts_filter_is_dlna_formatted(struct te_obj *filter)
{
	struct te_ts_filter *ts = te_ts_filter_from_obj(filter);

	if (ts)
		return ts->dlna_formatted_output;
	return false;
}
