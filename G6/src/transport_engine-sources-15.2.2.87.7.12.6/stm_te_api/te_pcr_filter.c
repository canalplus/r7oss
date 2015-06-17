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

Source file name : te_pcr_filter.c

Defines PCR filter specific operations
******************************************************************************/

#include <stm_te_dbg.h>
#include "te_object.h"
#include "te_output_filter.h"
#include "te_pcr_filter.h"
#include "te_time.h"
#include "te_buffer_work.h"
#include "te_demux.h"

struct te_pcr_filter {
	struct te_time_points time;
	struct te_out_filter output;
	bool new_seq;
};

static struct te_pcr_filter *pcr_filter_from_obj(struct te_obj *filter)
{
	if (filter->type == TE_OBJ_PCR_FILTER)
		return container_of(te_out_filter_from_obj(filter),
				struct te_pcr_filter,
				output);
	else
		return NULL;
}

struct te_time_points *te_pcr_get_times(struct te_obj *filter)
{
	struct te_pcr_filter *pcr = pcr_filter_from_obj(filter);

	if (pcr)
		return &pcr->time;

	return NULL;
}

static void te_pcr_filter_push_work(struct te_pcr_filter *pcr,
		struct te_hal_obj *buf)
{
	te_pcr_data_t raw_pcr_data;
	stm_te_pcr_t convert_data;
	uint32_t bytes_read;

	int result;

	result = te_out_filter_read_bytes(&pcr->output, raw_pcr_data.raw_data,
					sizeof(raw_pcr_data.raw_data),
					&bytes_read);

	if (!result && (bytes_read == sizeof(raw_pcr_data.raw_data))) {
		result = te_convert_arrival_to_systime
			     (&pcr->output, &raw_pcr_data,
			      &convert_data.pcr,
			      &convert_data.system_time,
			      true);

		if (result != 0) {
			pr_err("error in converting arrival to systime %d\n",
				result);
			/* Drop this PCR and continue
			 * on to the next one */
			return;
		}

		result = pcr->output.pcr_push_intf.set_clock_data_point_data(
						pcr->output.external_connection,
						TIME_FORMAT_PTS, pcr->new_seq,
						convert_data.pcr,
						convert_data.system_time);
		if (result) {
			pr_err("set_clock_data_point_data error %d\n", result);
		} else {
			pcr->new_seq = false;
		}
	} else {
		/* This is probably due to read triggered by timeout, ignore */
		pr_debug("error reading PCR data. result %d bytes_read %d\n",
			result, bytes_read);
	}
}

static void te_pcr_filter_work(struct te_obj *obj, struct te_hal_obj *buf)
{
	struct te_out_filter *output = te_out_filter_from_obj(obj);
	if (obj->state == TE_OBJ_PAUSED)
		return;

	if (output->pcr_push_intf.connect)
		te_pcr_filter_push_work(pcr_filter_from_obj(obj), buf);
	else
		te_out_filter_work(obj, buf);
}

static int te_pcr_filter_attach_to_clk(struct te_pcr_filter *pcr,
		stm_object_h target)
{
	int err;
	struct te_demux *demux = te_demux_from_obj(pcr->output.obj.parent);

	pr_debug("Attaching pcr filter %p to clock %p\n",
				&pcr->output.obj, target);

	err = pcr->output.pcr_push_intf.connect(&pcr->output.obj, target);
	if (err) {
		pr_err("Failed to connect PCR filter %p -> %p (%d)\n",
				&pcr->output.obj, target, err);
		goto error;
	}

	pcr->new_seq = true;

	if (pcr->output.hal_buffer)
		err = te_bwq_register(demux->bwq, &pcr->output.obj,
				pcr->output.hal_buffer,
				pcr->output.buffer_work_func);
	if (err) {
		pr_err("Failed to register buffer work\n");
		goto error;
	}

	err = stm_registry_add_connection(&pcr->output.obj,
			STM_SE_CLOCK_DATA_POINT_INTERFACE_PUSH, target);
	if (err) {
		pr_err("Failed to register connection %s (%d)\n",
				STM_SE_CLOCK_DATA_POINT_INTERFACE_PUSH, err);
		goto error;
	}
	return 0;

error:
	if (pcr->output.hal_buffer)
		if (te_bwq_unregister(demux->bwq, pcr->output.hal_buffer))
			pcr->output.hal_buffer = NULL;
	pcr->output.pcr_push_intf.disconnect(&pcr->output.obj, target);
	return err;
}

static int te_pcr_filter_detach_from_clk(struct te_pcr_filter *pcr, stm_object_h
		target)
{
	int err;
	int result = 0;
	struct te_demux *demux = te_demux_from_obj(pcr->output.obj.parent);

	pr_debug("Detaching pcr filter %p from clock %p\n",
					&pcr->output.obj, target);

	err = stm_registry_remove_connection(&pcr->output.obj,
			STM_SE_CLOCK_DATA_POINT_INTERFACE_PUSH);
	if (err) {
		pr_err("Failed to unregister connection %s (%d)\n",
				STM_SE_CLOCK_DATA_POINT_INTERFACE_PUSH, err);
		result = err;
	}

	if (pcr->output.hal_buffer)
		if (te_bwq_unregister(demux->bwq, pcr->output.hal_buffer))
			pcr->output.hal_buffer = NULL;

	err = pcr->output.pcr_push_intf.disconnect(&pcr->output.obj, target);
	if (err) {
		pr_err("Failed to disconnect %p->%p (%d)\n",
				&pcr->output.obj, target, err);
		result = err;
	} else {
		memset(&pcr->output.pcr_push_intf, 0,
				sizeof(pcr->output.pcr_push_intf));
	}

	return result;

}

static int te_pcr_filter_attach(struct te_obj *filter, stm_object_h target)
{
	int err;
	stm_object_h target_type;
	char type_tag[STM_REGISTRY_MAX_TAG_SIZE];
	uint32_t iface_size;
	struct te_pcr_filter *pcr = NULL;

	if (!filter)
		return -EINVAL;

	pcr = pcr_filter_from_obj(filter);
	if (!pcr)
		return -EINVAL;

	/* Check if this output filter is already attached to an external
	 * object */
	if (pcr->output.external_connection) {
		pr_err("Output filter %p already has an attachment\n",
						filter);
		return -EBUSY;
	}

	err = stm_registry_get_object_type(target, &target_type);
	if (0 != err) {
		pr_err("unable to get type of target object %p\n", target);
		return err;
	}

	if (0 == stm_registry_get_attribute(target_type,
			STM_SE_CLOCK_DATA_POINT_INTERFACE_PUSH,
			type_tag,
			sizeof(stm_se_clock_data_point_interface_push_sink_t),
			&pcr->output.pcr_push_intf,
			&iface_size))
		err = te_pcr_filter_attach_to_clk(pcr, target);
	else
		/* Fallback to base output filter attach */
		err = te_out_filter_attach(filter, target);

	if (!err)
		pcr->output.external_connection = target;
	return err;
}

static int te_pcr_filter_detach(struct te_obj *filter, stm_object_h target)
{
	int err;
	stm_object_h connected_obj = NULL;
	struct te_pcr_filter *pcr = NULL;

	if (!filter)
		return -EINVAL;

	pcr = pcr_filter_from_obj(filter);
	if (!pcr)
		return -EINVAL;

	if (pcr->output.external_connection != target) {
		pr_err("Output filter %p not attached to %p\n",
				&pcr->output.obj, target);
		return -EINVAL;
	}

	/* Use "push_clk" detach handler if available */
	if (pcr->output.pcr_push_intf.disconnect) {
		err = stm_registry_get_connection(te_obj_to_hdl(filter),
				STM_SE_CLOCK_DATA_POINT_INTERFACE_PUSH,
				&connected_obj);
		if (0 == err && connected_obj == target)
			err = te_pcr_filter_detach_from_clk(pcr, target);
	} else {
		/* Fallback to base output filter detach */
		err = te_out_filter_detach(filter, target);
	}

	if (!err)
		pcr->output.external_connection = NULL;

	return err;
}

int te_pcr_filter_get_control(struct te_obj *filter, uint32_t control,
					void *buf, uint32_t size)
{
	return te_out_filter_get_control(filter, control, buf, size);
}

int te_pcr_filter_set_control(struct te_obj *filter, uint32_t control,
					const void *buf, uint32_t size)
{
	return te_out_filter_set_control(filter, control, buf, size);
}

static int te_pcr_filter_delete(struct te_obj *filter)
{
	struct te_pcr_filter *pcr = pcr_filter_from_obj(filter);
	int err;

	err = te_out_filter_deinit(&pcr->output);
	if (err)
		return err;

	kfree(pcr);
	return 0;
}

static int te_pcr_filter_read_bytes(struct te_out_filter *output, uint8_t *buf,
		uint32_t buf_size, uint32_t *bytes_read)
{
	int err;
	te_pcr_data_t raw_pcr_data;
	stm_te_pcr_t *convert_data = (stm_te_pcr_t *)buf;

	/* Currently PCR filter always reads in quantisation units: 1 PCR at a
	 * time */
	if (buf_size < sizeof(stm_te_pcr_t)) {
		pr_err("Supplied buffer not large enough for PCR data\n");
		return -ENOMEM;
	}

	err = te_out_filter_read_bytes(output, raw_pcr_data.raw_data,
			sizeof(raw_pcr_data.raw_data), bytes_read);
	if (err) {
		pr_err("Error reading raw PCR data (%d)\n", err);
		return err;
	}

	/* Convert PCR timestamps (STC time -> SYS time) */
	err = te_convert_arrival_to_systime(output, &raw_pcr_data,
			&convert_data->pcr, &convert_data->system_time, true);

	*bytes_read = sizeof(*convert_data);
	return err;
}

static int32_t te_pcr_filter_next_read_size(struct te_out_filter *output)
{
	int32_t size = 0;
	int err;

	err = mutex_lock_interruptible(&output->obj.lock);
	if (err < 0)
		return err;

	/* Currently PCR filter always reads 1 PCR at a time, so read size is
	 * either 0 or sizeof the PCR data struct */
	if (output->wr_offs != output->rd_offs)
		size = sizeof(stm_te_pcr_t);

	mutex_unlock(&output->obj.lock);
	return size;
}

static struct te_obj_ops te_pcr_filter_ops = {
	.attach = te_pcr_filter_attach,
	.detach = te_pcr_filter_detach,
	.set_control = te_out_filter_set_control,
	.get_control = te_out_filter_get_control,
	.delete = te_pcr_filter_delete,
};

int te_pcr_filter_new(struct te_obj *demux, struct te_obj **new_filter)
{
	int res = 0;
	char name[STM_REGISTRY_MAX_TAG_SIZE];

	struct te_pcr_filter *filter = kzalloc(sizeof(*filter), GFP_KERNEL);

	if (!filter) {
		pr_err("couldn't allocate PCR filter object\n");
		return -ENOMEM;
	}

	snprintf(name, STM_REGISTRY_MAX_TAG_SIZE, "PCRFilter.%p",
					&filter->output.obj);

	res = te_out_filter_init(&filter->output, demux, name,
			TE_OBJ_PCR_FILTER);

	if (res != 0)
		goto error;

	filter->output.obj.ops = &te_pcr_filter_ops;
	filter->output.read_bytes = &te_pcr_filter_read_bytes;
	filter->output.next_read_size = &te_pcr_filter_next_read_size;

	/* PCR filter has a different buffer work function */
	filter->output.buffer_work_func = &te_pcr_filter_work;

	*new_filter = &filter->output.obj;
	return 0;

error:
	*new_filter = NULL;
	kfree(filter);

	return res;
}

