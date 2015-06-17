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

Source file name : te_pid_ins_filter.c

Defines pid insertion and replacement filter specific operations
******************************************************************************/
#include <stm_te_dbg.h>
#include <pti_hal_api.h>
#include <linux/workqueue.h>

#include "te_object.h"
#include "te_demux.h"
#include "te_output_filter.h"
#include "te_input_filter.h"
#include "te_rep_filter.h"
#include "te_ts_filter.h"
#include "te_sysfs.h"

struct te_rep_filter {
	struct te_in_filter in_filter;

	/* Data formatted into TS fragment */
	uint8_t fmtd_data[DVB_PACKET_SIZE];

	struct te_hal_obj *slot;
	struct te_hal_obj *data_entry;
};

static struct te_rep_filter *te_rep_filter_from_obj(struct te_obj *obj)
{
	if (obj->type == TE_OBJ_PID_REP_FILTER)
		return container_of(obj, struct te_rep_filter, in_filter.obj);

	return NULL;
}

static int te_rep_filter_update(struct te_rep_filter *rep)
{
	int err = 0;
	struct te_demux *demux = te_demux_from_obj(rep->in_filter.obj.parent);
	uint16_t pid = rep->in_filter.pid;

	stptiHAL_DataEntryConfigParams_t ConfigParams;
	ST_ErrorCode_t hal_err;

	/* Do nothing if we don't have a slot or data */
	if (rep->slot == NULL)
		return 0;

	if (rep->data_entry == NULL) {

		stptiHAL_DataEntryAllocateParams_t Params;

		memset(&Params, 0x00, sizeof(Params));
		Params.NumTSPackets = 1;

		err = te_hal_obj_alloc(&rep->data_entry, demux->hal_session,
				OBJECT_TYPE_DATA_ENTRY, &Params);

		if (err != 0) {
			pr_err("Unable to allocate dataentry\n");
			goto done;
		}
	}

	if (stptiOBJMAN_IsAssociated(rep->data_entry->hdl, rep->slot->hdl)) {
		hal_err = stptiOBJMAN_DisassociateObjects(rep->data_entry->hdl,
				rep->slot->hdl);
		pr_debug("disassociating dataentry\n");
		if (ST_NO_ERROR != hal_err) {
			pr_err("unable to disassociate dataentry\n");
			err = te_hal_err_to_errno(hal_err);
			goto done;
		}
	}

	/* Set up TS packet header */
	rep->fmtd_data[0] = 0x47;
	rep->fmtd_data[1] = 0x40 | ((pid >> 8) & 0x1F);
	rep->fmtd_data[2] = pid & 0xFF;
	rep->fmtd_data[3] = 0x10;
	rep->fmtd_data[4] = 0x00;

	memset(&ConfigParams, 0x00, sizeof(ConfigParams));
	ConfigParams.Data_p = rep->fmtd_data;
	ConfigParams.DataSize = DVB_PACKET_SIZE;
	ConfigParams.RepeatCount = 0;

	hal_err = stptiHAL_call(DataEntry.HAL_DataEntryConfigure,
				rep->data_entry->hdl, &ConfigParams);

	if (ST_NO_ERROR != hal_err) {
		pr_err("unable to configure dataentry\n");
		err = te_hal_err_to_errno(hal_err);
		goto done;
	}

	pr_debug("associating dataentry\n");
	hal_err = stptiOBJMAN_AssociateObjects(rep->data_entry->hdl,
			rep->slot->hdl);
	if (ST_NO_ERROR != hal_err) {
		pr_err("unable to associate dataentry\n");
		err = te_hal_err_to_errno(hal_err);
	}

done:
	return err;
}

int te_rep_filter_attach_out(struct te_obj *in, struct te_obj *out)
{
	struct te_rep_filter *rep = te_rep_filter_from_obj(in);
	struct te_out_filter *output = te_out_filter_from_obj(out);
	struct te_demux *demux = te_demux_from_obj(in->parent);

	stptiHAL_SlotConfigParams_t slot_params;
	ST_ErrorCode_t Error = ST_NO_ERROR;
	BOOL dlna = FALSE;

	int err = 0;

	/* A replacement filter can only be connected to one output */
	if (rep->in_filter.output_filters[0] != NULL &&
		rep->in_filter.output_filters[0] != out) {
		pr_err("already connected to %p\n",
				rep->in_filter.output_filters[0]);
		err = -EINVAL;
		goto error;
	}

	memset(&slot_params, 0, sizeof(stptiHAL_SlotConfigParams_t));
	slot_params.SlotMode = stptiHAL_SLOT_TYPE_RAW;
	slot_params.DataEntryReplacement = TRUE;

	err = te_hal_obj_alloc(&rep->slot, demux->hal_session,
					OBJECT_TYPE_SLOT, &slot_params);

	if (err != 0) {
		pr_err("couldn't allocate slot\n");
		goto error;
	}

	list_add(&rep->in_filter.slots, &rep->slot->entry);

	if (output->hal_buffer == NULL) {

		stptiHAL_BufferConfigParams_t params;

		pr_debug("creating HAL buffer\n");

		/* Ok lets allocate a buffer */
		te_out_filter_get_buffer_params(output, &params);

		/* Reset pointers for the new buffer */
		output->wr_offs = 0;
		output->rd_offs = 0;

		err = te_hal_obj_alloc(&output->hal_buffer,
					demux->hal_session,
					OBJECT_TYPE_BUFFER, &params);
		if (err != 0) {
			pr_err("couldn't allocate buffer\n");
			goto error;
		}

		/* The HAL returns the BufferStart in params.BufferStart_p */
		output->buf_start = params.BufferStart_p;
		output->buf_size = params.BufferSize;
	} else {
		/* Reuse existing buffer, but increment refcount */
		pr_debug("reusing HAL buffer\n");
		te_hal_obj_inc(output->hal_buffer);
	}

	pr_debug("connecting TS filter\n");
	if (te_ts_filter_is_security_formatted(out))
		/* Output: If no local scramble (LS) set,
		 * records input stream. If LS set, records
		 * scrambled stream.  Scrambled data, remains
		 * scrambled.
		 */
		Error = stptiHAL_call(
				Slot.HAL_SlotSetSecurePathOutputNode,
				rep->slot->hdl,
				stptiHAL_SECUREPATH_OUTPUT_NODE_SCRAMBLED);
	else
		Error = stptiHAL_call(
				Slot.HAL_SlotSetSecurePathOutputNode,
				rep->slot->hdl,
				stptiHAL_SECUREPATH_OUTPUT_NODE_CLEAR);

	if (Error != ST_NO_ERROR) {
		pr_err("HAL_SlotSetSecurePathOutputNode error 0x%x\n",
				Error);
		err = te_hal_err_to_errno(Error);
		goto error;
	}

	if (te_ts_filter_is_dlna_formatted(out))
		dlna = TRUE;

	Error = stptiHAL_call(Slot.HAL_SlotFeatureEnable,
			rep->slot->hdl,
			stptiHAL_SLOT_OUTPUT_DNLA_TS_TAG,
			dlna);

	if (ST_NO_ERROR != Error) {
		err = te_hal_err_to_errno(Error);
		goto error;
	}

	if (rep->in_filter.path_id) {
		Error = stptiHAL_call(Slot.HAL_SlotSetSecurePathID,
				rep->slot->hdl, rep->in_filter.path_id);
		if (ST_NO_ERROR != Error) {
			err = te_hal_err_to_errno(Error);
			goto error;
		}
	}

	pr_debug
	    ("Pid filter slot handle 0x%x, buffer handle 0x%x\n",
	     rep->slot->hdl.word, output->hal_buffer->hdl.word);

	Error = stptiOBJMAN_AssociateObjects(rep->slot->hdl,
					     output->hal_buffer->hdl);

	if (ST_NO_ERROR != Error) {
		pr_err("stptiOBJMAN_AssociateObjects error 0x%x\n",
				Error);
		err = te_hal_err_to_errno(Error);
		goto error;
	}

	pr_debug("Calling HAL_SlotSetPID\n");

	Error = stptiHAL_call(
			Slot.HAL_SlotSetPID,
			rep->slot->hdl,
			rep->in_filter.pid,
			FALSE);

	if (Error != ST_NO_ERROR) {
		pr_err("HAL_SlotSetPID error 0x%x\n", Error);
		err = te_hal_err_to_errno(Error);
		goto error;
	}

	Error = stptiHAL_call(Slot.HAL_SlotFeatureEnable, rep->slot->hdl,
				stptiHAL_SLOT_CC_FIXUP, true);

	if  (ST_NO_ERROR != Error) {
		pr_err("unable to enable cc fixup\n");
		err = te_hal_err_to_errno(Error);
		goto error;
	}

	rep->in_filter.output_filters[0] = out;

	err =  te_rep_filter_update(rep);
	if (err != 0)
		goto error;

	return 0;

error:
	if (output->hal_buffer) {
		if (te_hal_obj_dec(output->hal_buffer))
			output->hal_buffer = NULL;
	}

	if (rep->slot) {
		te_hal_obj_dec(rep->slot);
		rep->slot = NULL;
	}

	if (output->hal_filter) {
		if (te_hal_obj_dec(output->hal_filter))
			output->hal_filter = NULL;
	}

	return err;
}

int te_rep_filter_detach_out(struct te_obj *in, struct te_obj *out)
{
	struct te_out_filter *output = te_out_filter_from_obj(out);
	struct te_rep_filter *rep = te_rep_filter_from_obj(in);

	if (rep->slot)
		te_hal_obj_dec(rep->slot);
	rep->slot = NULL;

	if (output->hal_buffer)
		if (te_hal_obj_dec(output->hal_buffer))
			output->hal_buffer = NULL;

	return 0;
}

/*!
 * \brief Deletes an PID replacement filter
 *
 * \param TE obj for the PID replacement filter to delete
 *
 * \retval 0      Success
 * \retval -EBUSY Object has connections and cannot be deleted
 */
static int te_rep_filter_delete(struct te_obj *obj)
{
	struct te_rep_filter *rep = te_rep_filter_from_obj(obj);
	int err;

	err = te_in_filter_deinit(&rep->in_filter);
	if (err)
		return err;

	if (rep->data_entry)
		te_hal_obj_dec(rep->data_entry);

	kfree(rep);

	return 0;
}

static int te_rep_attach(struct te_obj *obj, stm_object_h target)
{
	stm_object_h hdl;
	struct te_in_filter *f = NULL;
	int err;

	err = te_hdl_from_obj(obj, &hdl);
	if (err == 0) {
		f = container_of(obj, struct te_in_filter, obj);
		err = te_in_filter_attach(f, target);
	}

	return err;
}

static int te_rep_detach(struct te_obj *obj, stm_object_h target)
{
	struct te_in_filter *f = container_of(obj, struct te_in_filter, obj);
	return te_in_filter_detach(f, target);
}

static int te_rep_filter_get_control(struct te_obj *obj, uint32_t control,
		void *buf, uint32_t size)
{
	struct te_rep_filter *rep = te_rep_filter_from_obj(obj);
	int err = 0;

	switch (control) {
	case STM_TE_PID_INS_FILTER_CONTROL_DATA:
		if (size >= (DVB_PAYLOAD_SIZE - 1))
			memcpy(buf, &rep->fmtd_data[5], (DVB_PAYLOAD_SIZE - 1));
		else
			err = -EINVAL;
		break;

	default:
		err = te_in_filter_get_control(&rep->in_filter, control,
							buf, size);
	}
	return err;
}

static int te_rep_filter_set_control(struct te_obj *obj, uint32_t control,
		const void *buf, uint32_t size)
{
	struct te_rep_filter *rep = te_rep_filter_from_obj(obj);
	int err = 0;

	switch (control) {
	case STM_TE_PID_INS_FILTER_CONTROL_DATA:
		if (size <= (DVB_PAYLOAD_SIZE - 1)) {
			memcpy(&rep->fmtd_data[5], buf, size);
			if (5+size < DVB_PACKET_SIZE) {
				memset(&rep->fmtd_data[5 + size], 0xff,
					DVB_PAYLOAD_SIZE - 1 - size);
			}
		} else {
			err = -EINVAL;
		}
		break;

	default:
		err = te_in_filter_set_control(&rep->in_filter, control,
							buf, size);
	}

	if (!err)
		err = te_rep_filter_update(rep);

	return err;
}

static struct te_obj_ops te_rep_filter_ops = {
	.delete = te_rep_filter_delete,
	.set_control = te_rep_filter_set_control,
	.get_control = te_rep_filter_get_control,
	.attach = te_rep_attach,
	.detach = te_rep_detach,
};

/*!
 * \brief stm_te pid replacement filter object constructor
 *
 * Creates a new pid replacement filter. Initialises all the internal data
 * structures in the object to their default values
 *
 * \param demux Parent demux object for the new filter
 * \param pid Initial pid to be replaced by the pid replacement filter object
 * \param new_filter Set to point to the new pid replacement filter on success
 *
 * \retval 0       Success
 * \retval -ENOMEM Failed to allocate object memory
 */
int te_rep_filter_new(struct te_obj *demux, uint16_t pid,
		struct te_obj **new_filter)
{
	struct te_rep_filter *filter;
	struct te_demux *dmx = te_demux_from_obj(demux);
	int err;
	char name[STM_REGISTRY_MAX_TAG_SIZE];
	ST_ErrorCode_t hal_err = ST_NO_ERROR;

	filter = kzalloc(sizeof(*filter), GFP_KERNEL);
	if (!filter) {
		pr_err("Failed to allocate PID replacement filter\n");
		return -ENOMEM;
	}

	snprintf(name, STM_REGISTRY_MAX_TAG_SIZE, "Rep.Filter.%p\n",
			&filter->in_filter.obj);
	err = te_in_filter_init(&filter->in_filter, demux, name,
			TE_OBJ_PID_REP_FILTER, pid);
	if (err)
		goto err1;

	err = stm_registry_add_attribute(&filter->in_filter.obj,
			TE_SYSFS_NAME_HAL_HDL "slot",
			TE_SYSFS_TAG_HAL_HDL,
			&filter->slot,
			sizeof(struct te_hal_obj *));
	if (err) {
		pr_warn("Failed to add HAL slot attr to obj %s (%d)\n",
				name, err);
		goto err1;
	}
	err = stm_registry_add_attribute(&filter->in_filter.obj,
			TE_SYSFS_NAME_HAL_HDL "data_entry",
			TE_SYSFS_TAG_HAL_HDL,
			&filter->data_entry,
			sizeof(struct te_hal_obj *));
	if (err) {
		pr_warn("Failed to add HAL data_entry attr to obj %s (%d)\n",
				name, err);
		goto err1;
	}

	hal_err = stptiHAL_call(vDevice.HAL_vDeviceSetEvent,
			dmx->hal_vdevice->hdl,
			stptiHAL_DATA_ENTRY_COMPLETE_EVENT, TRUE);
	if (ST_NO_ERROR != hal_err) {
		pr_err("unable to enable entry complete event 0x%x\n",
				hal_err);
		err = te_hal_err_to_errno(hal_err);
		goto err0;
	}

	filter->in_filter.obj.ops = &te_rep_filter_ops;
	*new_filter = &filter->in_filter.obj;
	return 0;
err0:
	te_in_filter_deinit(&filter->in_filter);
err1:
	kfree(filter);
	return err;
}
