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

Source file name : te_ts_index_filter.c

Defines ts index filter specific operations
******************************************************************************/

#include <stm_te_dbg.h>
#include "te_object.h"
#include "te_demux.h"
#include "te_input_filter.h"
#include "te_output_filter.h"
#include "te_ts_index_filter.h"
#include "te_time.h"
#include "te_global.h"

/* TS index constants */
#define SIZE_OF_SCD_MASK 32

#define te_alignupwards(x, alignment) (((x)+(alignment)-1) & ~((alignment)-1))

struct te_ts_index_filter {
	/* ts index filter specific data */

	stm_te_ts_index_definition_t definition;
	uint8_t startcodes[SIZE_OF_SCD_MASK];
	uint8_t num_startcodes;

	struct te_time_points time;

	/* te_global_data_t.index_filters membership */
	struct list_head idx_entry;

	struct te_out_filter output;
};

/* Shared sink is only used by TS Index filters, so declare here */
struct te_shared_sink {
	struct list_head entry;
	struct te_ts_index_filter *filter;
	stm_object_h target;
};

static int te_index_attach_buffer_sink(struct te_ts_index_filter *filter,
				stm_object_h *target);
static int te_index_detach_buffer_sink(struct te_ts_index_filter *filter,
				stm_object_h *target);

struct te_ts_index_filter *te_search_ext_connection(stm_object_h connection);
static int te_store_index(struct te_ts_index_filter *filter);
static int te_remove_index(struct te_ts_index_filter *filter);

static int te_store_multi_connect(struct te_ts_index_filter *filter,
		stm_object_h target);
static struct te_ts_index_filter *te_remove_multi_connect(void *target);
static int te_ts_index_filter_update_hal(struct te_ts_index_filter *index);

static struct te_ts_index_filter *ts_index_filter_from_obj(
		struct te_obj *filter)
{
	if (filter->type == TE_OBJ_TS_INDEX_FILTER)
		return container_of(te_out_filter_from_obj(filter),
				struct te_ts_index_filter,
				output);
	else
		return NULL;
}

struct te_time_points *te_ts_index_get_times(struct te_obj *filter)
{
	struct te_ts_index_filter *index = ts_index_filter_from_obj(filter);

	if (index)
		return &index->time;

	return NULL;
}

static int te_ts_index_filter_attach_to_pull_sink(struct te_obj *filter,
		stm_object_h target)
{
	int err;

	struct te_ts_index_filter *index = ts_index_filter_from_obj(filter);

	pr_debug("Attaching ts index filter %p to pull sink %p\n",
			filter, target);

	if (NULL == te_search_ext_connection(target)) {
		err = index->output.pull_intf.connect(filter, target,
				index->output.pull_src_intf);
		if (err) {
			pr_err("Failed to connect %p->%p (%d)\n", filter,
					target, err);
			goto error;
		}
		te_store_multi_connect(index, target);
	}

	err = te_index_attach_buffer_sink(index, target);
	if (err)
		goto error;

	/* Now that the index has been created, update the index parameters */
	err = te_ts_index_filter_update_hal(index);
	if (err)
		goto error;

	te_store_index(index);

	err = stm_registry_add_connection(filter, STM_DATA_INTERFACE_PULL,
			target);
	if (err) {
		pr_err("Failed to register connection %s (%d)\n",
				STM_DATA_INTERFACE_PULL, err);
		goto error;
	}
	return 0;

error:
	return err;
}

static int te_ts_index_filter_detach_from_pull_sink(struct te_obj *filter,
		stm_object_h target)
{
	int err;
	int result = 0;
	struct te_ts_index_filter *index = ts_index_filter_from_obj(filter);
	struct te_ts_index_filter *conn;

	pr_debug("Detaching ts index filter %p from pull sink %p\n", filter,
			target);

	err = stm_registry_remove_connection(filter, STM_DATA_INTERFACE_PULL);
	if (err) {
		pr_err("Failed to unregister connection %s (%d)\n",
				STM_DATA_INTERFACE_PULL, err);
		result = err;
	}

	te_remove_index(index);
	te_index_detach_buffer_sink(index, target);

	/* If this is the last index filter connected to this target,
	 * disconnect the interface */
	conn = te_search_ext_connection(target);

	if (conn == NULL) {
		conn = te_remove_multi_connect(target);
		err = index->output.pull_intf.disconnect(&conn->output.obj, target);
		if (err) {
			pr_err("Error disconnecting %p->%p\n",
				&conn->output.obj, target);
			result = err;
		}
	}

	return result;
}

static int te_index_attach_buffer_sink(struct te_ts_index_filter *index,
				stm_object_h *target)
{
	stptiHAL_BufferConfigParams_t params;
	int result = 0;
	struct te_demux *demux = te_demux_from_obj(index->output.obj.parent);
	struct te_ts_index_filter *friend;

	if (!demux)
		return -EINVAL;

	friend = te_search_ext_connection(target);

	/* Create/increment ref for HAL index object */
	if (!index->output.hal_index) {
		result = te_hal_obj_alloc(&index->output.hal_index,
				demux->hal_session, OBJECT_TYPE_INDEX, NULL);
		if (result)
			return result;
	} else {
		te_hal_obj_inc(index->output.hal_index);
	}

	if (!friend) {
		/* We're the first, alloc the buffer */
		result = te_out_filter_buffer_alloc(&index->output.obj,
				&index->output.hal_buffer, &params);
		if (result != 0) {
			result = -ENOMEM;
			goto error;
		}

		index->output.buf_start = params.BufferStart_p;
		index->output.buf_size = params.BufferSize;

		pr_debug("Index %s: Allocating buffer\n",
				index->output.obj.name);

	} else {
		/* Take a copy of our friends buffer */
		te_hal_obj_inc(friend->output.hal_buffer);

		index->output.hal_buffer = friend->output.hal_buffer;
		index->output.buf_start = friend->output.buf_start;
		index->output.buf_size = friend->output.buf_size;
		pr_debug("Index %s: sharing buffer from friend %s\n",
				index->output.obj.name,
				friend->output.obj.name);
	}

	if (ST_NO_ERROR != stptiOBJMAN_AssociateObjects(
				index->output.hal_index->hdl,
				index->output.hal_buffer->hdl)) {
		result = -EINVAL;
		goto error;
	}

	index->output.external_connection = target;

	return 0;

error:
	if (index->output.hal_buffer) {
		te_hal_obj_dec(index->output.hal_buffer);
		index->output.hal_buffer = NULL;
		index->output.buf_start = NULL;
		index->output.buf_size = 0;
	}

	return result;
}


static int te_index_detach_buffer_sink(struct te_ts_index_filter *index,
				stm_object_h *target)
{
	/* Clear TE data structures */
	index->output.buf_start = NULL;
	index->output.buf_size = 0;
	index->output.external_connection = NULL;

	/* HAL object allocation association etc */
	if (te_hal_obj_dec(index->output.hal_index))
		index->output.hal_index = NULL;

	te_hal_obj_dec(index->output.hal_buffer);
	index->output.hal_buffer = NULL;

	return 0;
}

struct te_ts_index_filter *te_search_ext_connection(stm_object_h connection)
{
	struct te_ts_index_filter *filter;
	list_for_each_entry(filter, &te_global.index_filters, idx_entry) {
		if (connection == filter->output.external_connection)
			return filter;
	}
	return NULL;
}

static int te_store_index(struct te_ts_index_filter *filter)
{
	list_add(&filter->idx_entry, &te_global.index_filters);
	return 0;
}

static int te_remove_index(struct te_ts_index_filter *filter)
{
	struct te_ts_index_filter *cur, *tmp;

	list_for_each_entry_safe(cur, tmp, &te_global.index_filters, idx_entry)
	{
		if (cur == filter) {
			list_del(&cur->idx_entry);
			return 0;
		}
	}
	return -EINVAL;
}


/* Stores the filter that is registered with the interface connection  */
static int te_store_multi_connect(struct te_ts_index_filter *filter,
		stm_object_h target)
{
	struct te_shared_sink *tmp;

	tmp = kzalloc(sizeof(struct te_shared_sink), GFP_KERNEL);
	if (tmp != NULL) {
		tmp->filter = filter;
		tmp->target = target;
		list_add(&tmp->entry, &te_global.shared_sinks);
		return 0;
	}
	pr_debug("Storing multi-connect index %p (%s) -> sink %p\n",
			&filter->output.obj, filter->output.obj.name, target);
	return -ENOMEM;
}

/*!
 * \brief Finds and removes (sink) object which has attachments to >1 TE output
 * filters
 *
 * Look through the list of all sinks that have > 1 te objects attached for the
 * target. Upon finding it return a ptr to the te filter used when registering
 * its connection.
 *
 * \param target The external object what has > 1 connection
 *
 * \return The TE filter that was used when register the interface connection
 * or NULL if the target was not found
 */
static struct te_ts_index_filter *te_remove_multi_connect(void *target)
{
	struct te_ts_index_filter *connect_filter = NULL;
	struct te_shared_sink *cur = NULL;
	struct te_shared_sink *tmp = NULL;

	list_for_each_entry_safe(cur, tmp, &te_global.shared_sinks, entry) {
		if (target == cur->target) {
			connect_filter = cur->filter;
			list_del(&cur->entry);
			kfree(cur);
			break;
		}
	}
	return connect_filter;
}

/* converts from HAL index structure to te index structure */
int hal_index_to_te_index(stptiHAL_IndexEventData_t *hal_index_data,
			   stm_te_ts_index_data_t *te_index_data,
			   uint32_t no_of_meta_bytes)
{
	ST_ErrorCode_t err;

	memset((void *)te_index_data, 0, sizeof(stm_te_ts_index_data_t));

	/* If this index is for a slot that no longer exists, the HAL will give
	 * us a Null slot handle. In such cases we cannot lookup the PID and
	 * must discard the index */
	if (HAL_HDL_IS_NULL(hal_index_data->IndexedSlotHandle))
		return -EAGAIN;

	err = stptiHAL_call(Slot.HAL_SlotGetPID,
			hal_index_data->IndexedSlotHandle,
			&te_index_data->pid);
	if (err)
		return te_hal_err_to_errno(err);

	te_index_data->flags =
		((uint32_t)(hal_index_data->AdditionalFlags)) << 24
		| hal_index_data->EventFlags;

	if (hal_index_data->NumberOfMPEGStartCodes)
		te_index_data->flags |= STM_TE_INDEX_START_CODE;

	te_index_data->packet_count =
		(uint32_t)(hal_index_data->BufferPacketCount);

	te_index_data->pcr =
		((uint64_t)(hal_index_data->PCR27MHzDiv300Bit32) << 32
		 | hal_index_data->PCR27MHzDiv300Bit31to0);

	te_index_data->system_time =
		((uint64_t)(hal_index_data->Clk27MHzDiv300Bit32) << 32
		 | hal_index_data->Clk27MHzDiv300Bit31to0);

	te_index_data->mpeg_start_code =
		(uint8_t)hal_index_data->MPEGStartCodeValue;

	te_index_data->mpeg_start_code_offset =
		(uint8_t)hal_index_data->MPEGStartCodeOffset;

	te_index_data->number_of_extra_bytes = (uint16_t)no_of_meta_bytes;
	return 0;
}

/*!
 * \brief Creates and associates the HAL objects required to attach a pid
 * filter to an index filter
 *
 * Creates slot, index and buffer HAL objects
 *
 * There is a single buffer per attached memsink
 *
 *  Multiple index filters can be connected to the same memsink This function
 *  allocates a hal_index and associates it with the correct hal_slot. It
 *  cannot do the hal_buffer allcoation as there is one per memsink so the
 *  hal_buffer allocation will be done when attaching to a memsink.
 *
 * \param pid_filter_p   TE Pid filter object to attach
 * \param index_filter_p TE Index filter object to attach to
 *
 * \retval 0       Success
 * \retval -ENOMEM Resource allocation failed
 * \retval -ENODEV TS filter not configured before index filter
 * \retval -EINVAL Bad parameter passed
 */
int attach_pid_to_index(struct te_obj *pid_flt, struct te_obj *index_flt)
{
	ST_ErrorCode_t Error = ST_NO_ERROR;
	int result;
	U16 SlotMode = stptiHAL_SLOT_TYPE_NULL;
	struct te_hal_obj *slot;
	bool found = false;
	struct te_in_filter *input = te_in_filter_from_obj(pid_flt);
	struct te_ts_index_filter *index = ts_index_filter_from_obj(index_flt);
	struct te_demux *demux = te_demux_from_obj(index_flt->parent);

	/* Find an existing RAW slot to attach indexing to */
	list_for_each_entry(slot, &input->slots, entry)
	{
		Error = stptiHAL_call(Slot.HAL_SlotGetMode, slot->hdl, &SlotMode);
		if (ST_NO_ERROR == Error && SlotMode == stptiHAL_SLOT_TYPE_RAW) {
				te_hal_obj_print(slot, "RAW slot found\n");
				te_hal_obj_inc(slot);
				found = true;
				break;
		}
	}

	if (!found) {
		pr_err("no RAW slot found\n");
		return -EINVAL;
	}

	if (0 != input->path_id) {
		Error = stptiHAL_call(Slot.HAL_SlotSetSecurePathID,
				      slot->hdl,
				      input->path_id);
		if (ST_NO_ERROR != Error) {
			result = -EINVAL;
			goto error;
		}
	}

	if (NULL == index->output.hal_index) {
		result = te_hal_obj_alloc(&index->output.hal_index,
				demux->hal_session, OBJECT_TYPE_INDEX, NULL);
		if (result)
			goto error;
	} else {
		te_hal_obj_inc(index->output.hal_index);
	}

	Error = stptiOBJMAN_AssociateObjects(slot->hdl, index->output.hal_index->hdl);
	if (ST_NO_ERROR != Error) {
		result = -EINVAL;
		goto error;
	}

	Error = stptiHAL_call(Slot.HAL_SlotSetPID, slot->hdl, input->pid, FALSE);

	if (Error != ST_NO_ERROR) {
		pr_err("HAL_SlotSetPID error 0x%x\n", Error);
		result = -EINVAL;
		goto error;
	}

	/* Now that the index has been created, update the index parameters */
	result = te_ts_index_filter_update_hal(index);
	if (result)
		goto error;

	return 0;

 error:
	te_hal_obj_dec(slot);
	if (index->output.hal_index) {
		te_hal_obj_dec(index->output.hal_index);
		index->output.hal_index = NULL;
	}
	return result;
}

/*!
 * \brief Translates stm_te index parameters into TE HAL parameters
 *
 * \param parm       stm_te index parameters to convert
 * \param hal_events Pointer to HAL event flags to set
 * \param hal_flags  Pointer to HAL additional flags to set
 * \param start_code_mask Pointer to array, set to HAL start code mask
 */
static int te_compute_index_params(stm_te_ts_index_set_params_t *parm,
		stptiHAL_EventFlags_t *hal_events,
		stptiHAL_AdditionalTransportFlags_t *hal_flags,
		uint32_t *start_code_mask)
{
	int err = 0;

	*hal_events = 0;
	if (parm->index_definition & STM_TE_INDEX_PUSI)
		*hal_events |= stptiHAL_INDEX_PUSI;
	if (parm->index_definition & STM_TE_INDEX_PTS)
		*hal_events |= stptiHAL_INDEX_PES_PTS;
	if (parm->index_definition & STM_TE_INDEX_SCRAM_TO_CLEAR)
		*hal_events |= stptiHAL_INDEX_SCRAMBLE_TO_CLEAR;
	if (parm->index_definition & STM_TE_INDEX_TO_EVEN_SCRAM)
		*hal_events |= stptiHAL_INDEX_SCRAMBLE_TO_EVEN;
	if (parm->index_definition & STM_TE_INDEX_TO_ODD_SCRAM)
		*hal_events |= stptiHAL_INDEX_SCRAMBLE_TO_ODD;
	if (parm->index_definition & STM_TE_INDEX_FIRST_REC_PACKET)
		*hal_events |= stptiHAL_INDEX_FIRST_RECORDED_PKT;

	*hal_flags = 0;
	if (parm->index_definition & STM_TE_INDEX_DISCONTINUITY)
		*hal_flags |= stptiHAL_AF_DISCONTINUITY_INDICATOR;
	if (parm->index_definition & STM_TE_INDEX_RANDOM_ACCESS)
		*hal_flags |= stptiHAL_AF_RANDOM_ACCESS_INDICATOR;
	if (parm->index_definition & STM_TE_INDEX_ES_PRIORITY)
		*hal_flags |= stptiHAL_AF_PRIORITY_INDICATOR;
	if (parm->index_definition & STM_TE_INDEX_PCR)
		*hal_flags |= stptiHAL_AF_PCR_FLAG;
	if (parm->index_definition & STM_TE_INDEX_OPCR)
		*hal_flags |= stptiHAL_AF_OPCR_FLAG;
	if (parm->index_definition & STM_TE_INDEX_SPLICING_POINT)
		*hal_flags |= stptiHAL_AF_SPLICING_POINT_FLAG;
	if (parm->index_definition & STM_TE_INDEX_TS_PRIVATE_DATA)
		*hal_flags |= stptiHAL_AF_PRIVATE_DATA_FLAG;
	if (parm->index_definition & STM_TE_INDEX_ADAPTATION_EXT)
		*hal_flags |= stptiHAL_AF_ADAPTION_EXTENSION_FLAG;

	pr_debug("AdditionalFlags 0x%x Events 0x%0x\n", *hal_flags,
			*hal_events);

	memset(start_code_mask, 0, SIZE_OF_SCD_MASK);
	if (parm->index_definition & STM_TE_INDEX_START_CODE) {
		int i;
		uint8_t start_code;

		if (!parm->start_codes) {
			pr_err("No start code mask supplied\n");
			err = -EINVAL;
			goto error;
		}

		/* Build the start code mask used by the HAL */
		for (i = 0; i < parm->number_of_start_codes &&
				i < SIZE_OF_SCD_MASK; i++) {
			start_code = parm->start_codes[i];
			start_code_mask[start_code / 32] |=
				(1 << (start_code % 32));
		}
	}

error:
	return err;
}

/*!
 * \brief Updates the HAL objects associated with an existing stm_te index
 * filter
 *
 * \param filter Pointer to stm_te index filter object to update
 *
 * \retval 0       Success
 * \retval -EINVAL The index controls for this filter are invalid
 * \retval -EIO    HAL error
 */
static int te_ts_index_filter_update_hal(struct te_ts_index_filter *index)
{
	ST_ErrorCode_t hal_err;
	int err = 0;

	stptiHAL_EventFlags_t events = 0;
	stptiHAL_AdditionalTransportFlags_t additional_flags = 0;
	uint32_t start_code_mask[SIZE_OF_SCD_MASK/sizeof(uint32_t)] = {0};
	uint32_t dummystartcodes[8] = {0};
	int i;
	bool update_scd = false;
	stm_te_ts_index_set_params_t index_parm;

	if (!index->output.hal_index) {
		/* nothing to do */
		return 0;
	}

	if (TE_OBJ_STARTED != index->output.obj.state)
		return 0;

	index->output.obj.ops->get_control(&index->output.obj,
			STM_TE_TS_INDEX_FILTER_CONTROL_INDEX_SET,
			(void *)&index_parm,
			sizeof(index_parm));

	/* Translate index parameters */
	err = te_compute_index_params(&index_parm, &events, &additional_flags,
			start_code_mask);
	if (err) {
		pr_err("Error in index parameters (%d)\n", err);
		goto error;
	}


	hal_err = stptiHAL_call(Index.HAL_IndexTransportEvents,
			index->output.hal_index->hdl,
			0xffffffff, 0xff, FALSE);
	if (ST_NO_ERROR != hal_err) {
		pr_err("HAL_IndexTransportEvents return 0x%x\n", hal_err);
		err = te_hal_err_to_errno(hal_err);
		goto error;
	}

	hal_err = stptiHAL_call(Index.HAL_IndexTransportEvents,
			index->output.hal_index->hdl,
			events, additional_flags, TRUE);
	if (ST_NO_ERROR != hal_err) {
		pr_err("HAL_IndexTransportEvents return 0x%x\n", hal_err);
		err = te_hal_err_to_errno(hal_err);
		goto error;
	}

	hal_err = stptiHAL_call(Index.HAL_IndexOutputStartCodes,
			index->output.hal_index->hdl,
			dummystartcodes, stptiHAL_NO_STARTCODE_INDEXING);
	if (ST_NO_ERROR != hal_err) {
		pr_err("HAL_IndexOutputStartCodes return 0x%x\n", hal_err);
		err = te_hal_err_to_errno(hal_err);
		goto error;
	}


	for (i = 0; i < SIZE_OF_SCD_MASK/sizeof(uint32_t); i++) {
		if (start_code_mask[i]) {
			update_scd = true;
			break;
		}
	}
	if (update_scd) {
		hal_err = stptiHAL_call(Index.HAL_IndexOutputStartCodes,
				index->output.hal_index->hdl,
				start_code_mask,
				stptiHAL_INDEX_STARTCODES_WITH_CONTEXT);
		if (ST_NO_ERROR != hal_err) {
			pr_err("HAL_IndexOutputStartCodes return 0x%x\n",
					hal_err);
			err = te_hal_err_to_errno(hal_err);
			goto error;
		}
	}

error:
	return err;
}

/*!
 * \brief Reads data from a index filter buffer
 *
 *  This function reads the TP buffer data and copies it to the provided buffer
 *  Returned data record are of variable data length. Each index record starts
 *  with a te_ts_index_data_t structure and is immediately followed by a
 *  variable number of bytes as detailed by te_ts_index_data_t.extra_bytes
 *  member. If extra_bytes is not a multiple of 4 then 1-3 stuffing bytes will
 *  be inserted before the next te_ts_index_data_t structure.
 *
 * \param output     TE output filter object to read from
 * \param address_p  Data buffer to copy into
 * \param buffersize Size of buffer pointed to by address_p
 * \param bytesread  Returned number of bytes read
 *
 * \retval 0        Success
 * \retval -EINVAL  HAL error
 * \retval -ENOBUFS No data in the buffer to read
 */
static int te_ts_index_filter_read_bytes(struct te_out_filter *output,
			uint8_t *address_p,
			uint32_t buffersize,
			uint32_t *bytesread)
{
	int result = 0;
	uint32_t ReadOffset = stptiHAL_CURRENT_READ_OFFSET;
	uint32_t no_of_meta_bytes = 0;
	uint8_t index_meta_data[256];
	bool first_time = true;
	int min_bytes_per_index = sizeof(stm_te_ts_index_data_t);

	ST_ErrorCode_t Error;
	FullHandle_t hal_buf_h;
	stptiHAL_IndexEventData_t hal_index;
	stm_te_ts_index_data_t te_index_data;

	*bytesread = 0;
	memset(&hal_index, 0, sizeof(stptiHAL_IndexEventData_t));

	hal_buf_h = output->hal_buffer->hdl;

	/* TS index filter read current pull behaviour is, "read multiple
	 * integer quantisation units per read" */

	while (buffersize >= min_bytes_per_index && result == 0) {
		/* Indexes from the HAL comprise an index structure
		 * (stptiHAL_IndexEventData_t) followed by an optional number
		 * of meta data bytes.
		 */
		Error = stptiHAL_call(Buffer.HAL_BufferRead,
				hal_buf_h,
				stptiHAL_READ_AS_UNITS_ALLOW_TRUNCATION,
				&ReadOffset, 0,
				index_meta_data,
				sizeof(index_meta_data), NULL, 0,
				&hal_index,
				sizeof(stptiHAL_IndexEventData_t),
				&te_out_filter_memcpy, &no_of_meta_bytes);

		if (ST_NO_ERROR == Error) {
			result = hal_index_to_te_index(&hal_index,
					&te_index_data, no_of_meta_bytes);
			if (result != 0) {
				stptiHAL_call(Buffer.HAL_BufferSetReadOffset,
						hal_buf_h, ReadOffset);
				goto ret_result;
			}

			result = te_convert_arrival_to_systime(output,
					NULL, &te_index_data.pcr,
					&te_index_data.system_time, first_time);
			if (result != 0) {
				pr_err("error in converting arrival to systime %d\n",
						result);
				stptiHAL_call(Buffer.HAL_BufferSetReadOffset,
						hal_buf_h, ReadOffset);
				result = -EINVAL;
				goto ret_result;
			}
			first_time = false;

			pr_debug("Flags 0x%08x PacketCount %d pid %d\n",
						te_index_data.flags,
						te_index_data.packet_count,
						te_index_data.pid);

			/* process meta data and update read pointer in
			 * hal_buffer */
			if (buffersize >= (no_of_meta_bytes +
					sizeof(stm_te_ts_index_data_t))) {
				memcpy(address_p, &te_index_data,
						sizeof(stm_te_ts_index_data_t));

				address_p += sizeof(stm_te_ts_index_data_t);
				memcpy(address_p, index_meta_data,
						no_of_meta_bytes);

				address_p += no_of_meta_bytes;
				buffersize -= (sizeof(stm_te_ts_index_data_t) +
						no_of_meta_bytes);
				*bytesread += (sizeof(stm_te_ts_index_data_t) +
						no_of_meta_bytes);

				stptiHAL_call(Buffer.HAL_BufferSetReadOffset,
						hal_buf_h, ReadOffset);
			} else {
				pr_debug("not enough space to output meta data. Buffersize=%d bytesread=%d no_of_meta_bytes=%d\n",
						buffersize, *bytesread,
						no_of_meta_bytes);
				if (!(*bytesread))
					result = -ENOMEM;
				goto ret_result;
			}
		} else if ((STPTI_ERROR_NO_PACKET != Error) &&
				(STPTI_ERROR_NOT_ENOUGH_ROOM_TO_RETURN_DATA !=
				 Error)) {
			result = te_hal_err_to_errno(Error);
		}
	}
ret_result:
	return result;
}

static int32_t te_ts_index_filter_next_read_size(struct te_out_filter *output)
{
	int32_t size = 0;
	ST_ErrorCode_t hal_err;
	uint32_t nb_index, nb_metabytes, bytes;

	nb_index = nb_metabytes = bytes = 0;
	/* TS index filter read current pull behaviour is, "read multiple
	 * integer quantisation units per read" */
	if (output->hal_buffer) {
		hal_err = stptiHAL_call(Buffer.HAL_BufferStatus,
				output->hal_buffer->hdl,
				NULL, &bytes, NULL, NULL, &nb_index,
				&nb_metabytes, NULL);
		if (ST_NO_ERROR != hal_err && STPTI_ERROR_NO_PACKET != hal_err
				&& STPTI_ERROR_NOT_ENOUGH_ROOM_TO_RETURN_DATA
				!= hal_err) {
			pr_err("error reading buffer status: 0x%x\n", hal_err);
			return 0;
		}

		size = nb_index * sizeof(stm_te_ts_index_data_t);
		size += nb_metabytes;
	}

	return size;
}

static int te_ts_index_filter_attach(struct te_obj *filter, stm_object_h target)
{
	int err;
	stm_object_h target_type;
	char type_tag[STM_REGISTRY_MAX_TAG_SIZE];
	uint32_t iface_size;
	struct te_ts_index_filter *index = NULL;

	if (!filter)
		return -EINVAL;

	index = ts_index_filter_from_obj(filter);
	if (!index)
		return -EINVAL;

	/* Check if this output filter is already attached to an external
	 * object */
	if (index->output.external_connection) {
		pr_err("Output filter %p already has an attachment\n", filter);
		return -EBUSY;
	}

	err = stm_registry_get_object_type(target, &target_type);
	if (0 != err) {
		pr_err("unable to get type of target object %p\n", target);
		return err;
	}

	if (0 == stm_registry_get_attribute(target_type,
				STM_DATA_INTERFACE_PULL,
				type_tag,
				sizeof(stm_data_interface_pull_sink_t),
				&index->output.pull_intf,
				&iface_size))
		err = te_ts_index_filter_attach_to_pull_sink(filter, target);
	else
		/* Fallback to base output filter attach */
		err = te_out_filter_attach(filter, target);

	if (!err) {
		index->output.external_connection = target;
		filter->state = TE_OBJ_STARTED;
	}

	return err;
}

static int te_ts_index_filter_detach(struct te_obj *filter, stm_object_h target)
{
	int err;
	stm_object_h connected_obj = NULL;
	struct te_ts_index_filter *index = ts_index_filter_from_obj(filter);

	if (index->output.external_connection != target) {
		pr_err("Output filter %p not attached to %p\n", filter, target);
		return -EINVAL;
	}

	err = stm_registry_get_connection(te_obj_to_hdl(filter),
			STM_DATA_INTERFACE_PULL, &connected_obj);
	if (0 == err && connected_obj == target)
		err = te_ts_index_filter_detach_from_pull_sink(filter, target);
	else
		/* Fallback to base output filter detach */
		err = te_out_filter_detach(filter, target);

	if (!err) {
		index->output.external_connection = NULL;
		filter->state = TE_OBJ_STOPPED;
	}
	return err;
}

static int te_ts_index_filter_delete(struct te_obj *filter)
{
	struct te_ts_index_filter *index = ts_index_filter_from_obj(filter);
	int err;

	err = te_out_filter_deinit(&index->output);
	if (err)
		return err;

	kfree(index);
	return 0;
}

int te_ts_index_filter_get_control(struct te_obj *filter, uint32_t control,
					void *buf, uint32_t size)
{
	int err = 0;

	struct te_ts_index_filter *index = ts_index_filter_from_obj(filter);
	stm_te_ts_index_set_params_t *parms;

	switch (control) {
	case STM_TE_TS_INDEX_FILTER_CONTROL_INDEX_SET:
		if (size == sizeof(stm_te_ts_index_set_params_t)) {
			parms = (stm_te_ts_index_set_params_t *)buf;
			parms->index_definition = index->definition;
			parms->number_of_start_codes = index->num_startcodes;
			parms->start_codes = index->startcodes;
		} else {
			err = -EINVAL;
		}
		break;
        case STM_TE_OUTPUT_FILTER_CONTROL_PAUSE:
                return -EINVAL;
	default:
		err = te_out_filter_get_control(filter, control, buf, size);
	}

	return err;
}

int te_ts_index_filter_set_control(struct te_obj *filter, uint32_t control,
					const void *buf, uint32_t size)
{
	int err = 0;
	struct te_ts_index_filter *index = ts_index_filter_from_obj(filter);
	stm_te_ts_index_set_params_t *parms;

	switch (control) {
	case STM_TE_TS_INDEX_FILTER_CONTROL_INDEX_SET:
		if (size == sizeof(stm_te_ts_index_set_params_t)) {
			parms = (stm_te_ts_index_set_params_t *)buf;

			index->definition = parms->index_definition;
			index->num_startcodes = parms->number_of_start_codes;

			if (index->num_startcodes > SIZE_OF_SCD_MASK)
				index->num_startcodes = SIZE_OF_SCD_MASK;

			if (index->num_startcodes) {
				memcpy(index->startcodes,
					parms->start_codes,
					index->num_startcodes);
			}
		} else {
			err = -EINVAL;
		}
		break;
        case STM_TE_OUTPUT_FILTER_CONTROL_PAUSE:
                return -EINVAL;
	default:
		err = te_out_filter_set_control(filter, control, buf, size);
	}

	if (!err)
		err = te_ts_index_filter_update_hal(index);

	return err;
}

static struct te_obj_ops te_ts_index_filter_ops = {
	.attach = te_ts_index_filter_attach,
	.detach = te_ts_index_filter_detach,
	.set_control = te_ts_index_filter_set_control,
	.get_control = te_ts_index_filter_get_control,
	.delete = te_ts_index_filter_delete,
};

int te_ts_index_filter_new(struct te_obj *demux, struct te_obj **new_filter)
{
	int res = 0;
	char name[STM_REGISTRY_MAX_TAG_SIZE];

	struct te_ts_index_filter *filter = kzalloc(sizeof(*filter),
			GFP_KERNEL);

	if (!filter) {
		pr_err("couldn't allocate TS Index filter object\n");
		return -ENOMEM;
	}

	snprintf(name, STM_REGISTRY_MAX_TAG_SIZE, "TSIdxFilter.%p",
				&filter->output.obj);
	res = te_out_filter_init(&filter->output, demux, name,
			TE_OBJ_TS_INDEX_FILTER);
	if (res != 0)
		goto error;

	/* Initialise TS Index filter data */
	filter->output.obj.ops = &te_ts_index_filter_ops;
	filter->output.read_bytes = &te_ts_index_filter_read_bytes;
	filter->output.next_read_size = &te_ts_index_filter_next_read_size;

	*new_filter = &filter->output.obj;
	return 0;

error:
	*new_filter = NULL;
	kfree(filter);

	return res;
}

