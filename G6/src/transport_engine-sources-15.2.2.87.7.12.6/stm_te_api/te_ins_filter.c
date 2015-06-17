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

Source file name : te_pid_filter.c

Defines Insertion filter specific operations
******************************************************************************/
#include "te_object.h"
#include "te_input_filter.h"
#include "te_demux.h"
#include "te_pid_filter.h"
#include "te_output_filter.h"
#include "te_ts_filter.h"
#include "te_sysfs.h"

struct te_ins_filter {
	struct te_in_filter in_filter;

	/* Original Data */
	uint8_t *data;
	size_t data_len;

	/* Data formatted into TS fragment */
	uint8_t *fmtd_data;
	size_t fmtd_len;
	uint32_t num_fmtd_pkts;

	/* Data entries */
	struct list_head data_entries;

	/* State variables */
	bool reformat;
	bool running;
	uint32_t delay;
	uint8_t cc;
	struct delayed_work dwork;

	/* Shared injection slot */
	struct te_hal_obj *slot;
};

static int te_ins_filter_inject(struct te_ins_filter *ins);

static struct te_ins_filter *te_ins_filter_from_obj(struct te_obj *obj)
{
	if (obj->type == TE_OBJ_PID_INS_FILTER)
		return container_of(obj, struct te_ins_filter, in_filter.obj);

	return NULL;
}

/* \brief detroy list of dataentries */
static int te_ins_destroy_de(struct te_ins_filter *ins)
{
	struct te_hal_obj *de, *tmp;
	list_for_each_entry_safe(de, tmp, &ins->data_entries, entry) {
		pr_debug("destroying data entry 0x%08x\n", de->hdl.word);
		te_hal_obj_dec(de); /* Automagically removes DE from list */
	}
	return 0;
}

/* \brief create a list of dataentries, enough to hold section */
static int te_ins_create_de(struct te_ins_filter *ins)
{
	struct te_demux *demux = te_demux_from_obj(ins->in_filter.obj.parent);
	struct te_hal_obj *de;

	int i;
	int num_des = (ins->num_fmtd_pkts / 2) + (ins->num_fmtd_pkts % 2);

	int err = 0;

	te_ins_destroy_de(ins);

	for (i = 0; i < num_des; i++) {
		stptiHAL_DataEntryAllocateParams_t Params;

		memset(&Params, 0x00, sizeof(Params));
		Params.NumTSPackets = 2;

		if (i == (num_des - 1) && (ins->num_fmtd_pkts % 2))
			Params.NumTSPackets = 1;

		err = te_hal_obj_alloc(&de, demux->hal_session,
				OBJECT_TYPE_DATA_ENTRY, &Params);
		if (err != 0) {
			pr_err("unable to allocate data entry object\n");
			err = -ENOMEM;
			goto error;
		}

		list_add_tail(&de->entry, &ins->data_entries);
	}
	return 0;
error:
	te_ins_destroy_de(ins);
	return err;
}

/* \brief sets up TS headers on formatted stream */
static void te_ins_filter_create_hdrs(struct te_ins_filter *ins)
{
	int i;

	int pkt_len = DVB_PACKET_SIZE;
	int hdr_off = 0; /* Do we need to adjust this for DLNA? */

	uint8_t *pkt = ins->fmtd_data;
	uint16_t pid = ins->in_filter.pid;

	for (i = 0; i < ins->num_fmtd_pkts; i++) {
		pkt[hdr_off + 0] = 0x47;
		pkt[hdr_off + 1] = ((pid >> 8) & 0x1F);

		if (i == 0)
			pkt[hdr_off + 1] |= 0x40 ;

		pkt[hdr_off + 2] = pid & 0xFF;
		pkt[hdr_off + 3] = 0x10 | (ins->cc & 0xF);

		pkt += pkt_len;
		ins->cc++;
	}
}

static int te_ins_filter_associate_de(struct te_ins_filter *ins)
{
	int err = 0;
	ST_ErrorCode_t hal_err;
	int i = 0;

	int pkt_len = DVB_PACKET_SIZE; /* Do we need to adjust this for DLNA? */
	uint8_t *pkt = ins->fmtd_data;
	int num_des = (ins->num_fmtd_pkts / 2) + (ins->num_fmtd_pkts % 2);

	struct te_hal_obj *de;

	if (ins->slot == NULL) {
		pr_debug("not connected yet, reschedule\n");
		goto done;
	}

	/* Check if any DEs still waiting */
	list_for_each_entry(de, &ins->data_entries, entry) {
		if (stptiOBJMAN_IsAssociated(de->hdl, ins->slot->hdl)) {
			pr_debug("data entry 0x%08x still associated\n",
					de->hdl.word);
			goto done;
		}
	}

	/* Update CCs */
	te_ins_filter_create_hdrs(ins);

	/* Recreate DEs */
	te_ins_create_de(ins);

	/* Associate all the data entries to the slot to trigger insertion */
	i = 0;
	list_for_each_entry(de, &ins->data_entries, entry) {
		stptiHAL_DataEntryConfigParams_t ConfigParams;

		memset(&ConfigParams, 0x00, sizeof(ConfigParams));
		ConfigParams.Data_p = pkt;
		ConfigParams.DataSize = pkt_len * 2;
		ConfigParams.RepeatCount = 1;

		if ((ins->num_fmtd_pkts % 2) && i == (num_des - 1))
			ConfigParams.DataSize = pkt_len;

		hal_err = stptiHAL_call(DataEntry.HAL_DataEntryConfigure,
			de->hdl, &ConfigParams);
		if (ST_NO_ERROR != hal_err) {
			pr_err("unable to set data entry 0x%x\n", hal_err);
			err = te_hal_err_to_errno(hal_err);
			goto done;
		}

		hal_err = stptiOBJMAN_AssociateObjects(de->hdl,
				ins->slot->hdl);
		if (ST_NO_ERROR != hal_err) {
			pr_err("unable to associate de to slot\n");
			err = te_hal_err_to_errno(hal_err);
			goto done;
		}
		pr_debug("associated dataentry 0x%08x (%d in set)\n",
				de->hdl.word, i);

		i ++;
		pkt += ConfigParams.DataSize;
	}
done:

	return err;

}

/* \brief Map input data onto TS packets */
static int te_ins_filter_format(struct te_ins_filter *ins)
{
	int err = 0;

	int pkts = 1;

	int pkt_len = DVB_PACKET_SIZE; /* Do we need to adjust this for DLNA? */
	int hdr_len = DVB_HEADER_SIZE;
	int pld_len = DVB_PAYLOAD_SIZE;
	int pad_len = 0;

	uint8_t *pkt, *inb;

	int i;

	if (ins->fmtd_data) {
		kfree(ins->fmtd_data);
		ins->fmtd_len = 0;
	}

	if (!ins->data) {
		pr_err("no data to format\n");
		err = -EINVAL;
		goto done;
	}

	/*
	 * First packet has a DVB_PAYLOAD_SIZE - 1 byte payload
	 * (one byte for pointer), subsequent have DVB_PAYLOAD_SIZE
	 * Calculate how much space we need (including padding the last packet)
	 */

	if (ins->data_len > pld_len - 1) {
		pad_len = pld_len - ( (ins->data_len - (pld_len - 1)) % pld_len );
		pkts = (1 + ins->data_len + pad_len) / pld_len;
	} else {
		pad_len = (pld_len - 1) - ins->data_len;
		pkts = 1;
	}

	pr_debug("input section of %d bytes gives %d pkts with %d pad\n",
		ins->data_len, pkts, pad_len);

	/* Allocate and map */
	ins->fmtd_data = kzalloc(pkts * pkt_len, GFP_KERNEL);
	if (!ins->fmtd_data) {
		pr_err("no memory for formatted data\n");
		err = -ENOMEM;
		goto done;
	}
	ins->fmtd_len = pkts * pkt_len;
	ins->num_fmtd_pkts = pkts;

	inb = ins->data;
	for (i = 0; i < pkts; i++) {
		int pos = hdr_len;

		pkt = &ins->fmtd_data[i * pkt_len];

		if (i == 0)
			pkt[pos++] = 0; /* pointer byte */

		if (i == pkts - 1) {
			memcpy(&pkt[pos], inb, pkt_len - pos - pad_len);
			memset(&pkt[pkt_len - pad_len], 0xff, pad_len);
		} else {
			memcpy(&pkt[pos], inb, pkt_len - pos);
		}

		inb +=  pkt_len - pos;
	}

done:
	return err;
}

void te_ins_filter_work(struct work_struct *work)
{
	struct te_ins_filter *ins =
		container_of(work, struct te_ins_filter, dwork.work);

	te_ins_filter_inject(ins);
}

static int te_ins_filter_schedule(struct te_ins_filter *ins)
{
	struct te_demux *demux = te_demux_from_obj(ins->in_filter.obj.parent);

	/* Don't schedule if we're not connected */
	if (ins->in_filter.output_filters[0] == NULL)
		return -EAGAIN;

	ins->running = true;

	pr_debug("scheduling work in %dmsec\n", ins->delay);

	INIT_DELAYED_WORK(&ins->dwork, te_ins_filter_work);
	queue_delayed_work(demux->work_que, &ins->dwork,
		msecs_to_jiffies(ins->delay));
	return 0;
}

static int te_ins_filter_inject(struct te_ins_filter *ins)
{
	int err = 0;

	if (!ins->fmtd_data || ins->reformat) {
		ins->reformat = false;
		err = te_ins_filter_format(ins);
		if (err != 0)
			goto done;
	}

	err = te_ins_filter_associate_de(ins);

done:
	if (ins->delay > 0 && (err == 0 || err == -EAGAIN)) {
		/* Schedule delayed work */
		te_ins_filter_schedule(ins);
	} else {
		ins->running = false;
	}

	return err;
}


/*!
 * \brief Deletes a TE PID filter object
 *
 * \param obj TE PID filter object to delete
 *
 * \retval 0 Success
 */
static int te_ins_filter_delete(struct te_obj *obj)
{
	int err;
	struct te_ins_filter *ins = te_ins_filter_from_obj(obj);

	if (ins->running && ins->delay > 0) {
		pr_debug("cancelling delayed work\n");
		ins->delay = 0;
		cancel_delayed_work_sync(&ins->dwork);
	}

	te_ins_destroy_de(ins);

	err = te_in_filter_deinit(&ins->in_filter);
	if (err)
		return err;

	if (ins->data)
		kfree(ins->data);

	if (ins->fmtd_data)
		kfree(ins->fmtd_data);

	kfree(ins);

	return err;
}

int te_ins_filter_attach_out(struct te_obj *in, struct te_obj *out)
{
	struct te_ins_filter *ins = te_ins_filter_from_obj(in);
	struct te_out_filter *output = te_out_filter_from_obj(out);
	struct te_demux *demux = te_demux_from_obj(in->parent);
	struct te_in_filter *iflt;

	stptiHAL_SlotConfigParams_t slot_params;
	ST_ErrorCode_t Error = ST_NO_ERROR;
	BOOL dlna = FALSE;

	int err = 0;

	/* An insertion filter can only be connected to one output */
	if (ins->in_filter.output_filters[0] != NULL &&
		ins->in_filter.output_filters[0] != out) {
		pr_err("already connected to %p\n",
				ins->in_filter.output_filters[0]);
		err = -EINVAL;
		goto error;
	}

	/* Check we always connect to the same TS output filter */
	list_for_each_entry(iflt, &demux->in_filters, obj.lh) {
		if (iflt->obj.type == TE_OBJ_PID_INS_FILTER) {
			struct te_ins_filter *other
				= te_ins_filter_from_obj(&iflt->obj);

			if (iflt->output_filters[0] != NULL
					&& iflt->output_filters[0] != out) {
				pr_err("can't connect to different output\n");
				err = -EINVAL;
				goto error;
			}

			if (ins->slot == NULL && other->slot) {
				/* Copy the slot and increment buffer refcount */
				te_hal_obj_inc(output->hal_buffer);
				te_hal_obj_inc(other->slot);
				ins->slot = other->slot;
			}
		}
	}

	if (ins->slot)
		goto done;

	memset(&slot_params, 0, sizeof(stptiHAL_SlotConfigParams_t));
	slot_params.SlotMode = stptiHAL_SLOT_TYPE_RAW;
	slot_params.DataEntryInsertion = TRUE;

	err = te_hal_obj_alloc(&ins->slot, demux->hal_session,
					OBJECT_TYPE_SLOT, &slot_params);

	if (err != 0) {
		pr_err("couldn't allocate slot\n");
		goto error;
	}

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
				ins->slot->hdl,
				stptiHAL_SECUREPATH_OUTPUT_NODE_SCRAMBLED);
	else
		Error = stptiHAL_call(
				Slot.HAL_SlotSetSecurePathOutputNode,
				ins->slot->hdl,
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
			ins->slot->hdl,
			stptiHAL_SLOT_OUTPUT_DNLA_TS_TAG,
			dlna);

	if (ST_NO_ERROR != Error) {
		err = te_hal_err_to_errno(Error);
		goto error;
	}

	if (ins->in_filter.path_id) {
		Error = stptiHAL_call(Slot.HAL_SlotSetSecurePathID,
				ins->slot->hdl, ins->in_filter.path_id);
		if (ST_NO_ERROR != Error) {
			err = te_hal_err_to_errno(Error);
			goto error;
		}
	}

	pr_debug
	    ("Pid filter slot handle 0x%x, buffer handle 0x%x\n",
	     ins->slot->hdl.word, output->hal_buffer->hdl.word);

	Error = stptiOBJMAN_AssociateObjects(ins->slot->hdl,
					     output->hal_buffer->hdl);

	if (ST_NO_ERROR != Error) {
		pr_err("stptiOBJMAN_AssociateObjects error 0x%x\n",
				Error);
		err = te_hal_err_to_errno(Error);
		goto error;
	}

done:
	ins->in_filter.output_filters[0] = out;
	if (ins->delay)
		return te_ins_filter_schedule(ins);

	return 0;

error:
	if (output->hal_buffer) {
		if (te_hal_obj_dec(output->hal_buffer))
			output->hal_buffer = NULL;
	}

	if (ins->slot) {
		te_hal_obj_dec(ins->slot);
		ins->slot = NULL;
	}

	if (output->hal_filter) {
		if (te_hal_obj_dec(output->hal_filter))
			output->hal_filter = NULL;
	}

	return err;
}

int te_ins_filter_detach_out(struct te_obj *in, struct te_obj *out)
{
	struct te_out_filter *output = te_out_filter_from_obj(out);
	struct te_ins_filter *ins = te_ins_filter_from_obj(in);

	if (ins->running && ins->delay > 0) {
		/* We're running and scheduled, wait... */
		pr_debug("cancelling delayed work\n");
		ins->delay = 0;
		cancel_delayed_work_sync(&ins->dwork);
	}

	if (ins->slot)
		te_hal_obj_dec(ins->slot);
	ins->slot = NULL;

	if (output->hal_buffer)
		if (te_hal_obj_dec(output->hal_buffer))
			output->hal_buffer = NULL;

	return 0;
}

static int te_ins_filter_attach(struct te_obj *obj, stm_object_h target)
{
	stm_object_h hdl;
	struct te_ins_filter *ins = NULL;
	int err;

	err = te_hdl_from_obj(obj, &hdl);
	if (err == 0) {
		ins =  te_ins_filter_from_obj(obj);
		err = te_in_filter_attach(&ins->in_filter, target);
	}

	return err;
}

static int te_ins_filter_detach(struct te_obj *obj, stm_object_h target)
{
	struct te_ins_filter *ins = te_ins_filter_from_obj(obj);
	return te_in_filter_detach(&ins->in_filter, target);
}

static int te_ins_filter_get_control(struct te_obj *obj, uint32_t control,
		void *buf, uint32_t size)
{
	struct te_ins_filter *ins = te_ins_filter_from_obj(obj);
	int err = 0;

	switch (control) {
	case STM_TE_PID_INS_FILTER_CONTROL_DATA:
		if (size >= ins->data_len && ins->data)
			memcpy(buf, ins->data, ins->data_len);
		else
			err = -EINVAL;
		break;

	case STM_TE_PID_INS_FILTER_CONTROL_FREQ:
		err = GET_CONTROL(ins->delay, buf, size);
		break;

	case STM_TE_PID_INS_FILTER_CONTROL_TRIG:
		break;

	default:
		err = te_in_filter_get_control(&ins->in_filter, control,
						buf, size);
	}

	return err;
}

static int te_ins_filter_set_control(struct te_obj *obj, uint32_t control,
		const void *buf, uint32_t size)
{
	struct te_ins_filter *ins = te_ins_filter_from_obj(obj);
	int err = 0;

	switch (control) {
	case STM_TE_PID_INS_FILTER_CONTROL_DATA:

		if (size > TE_MAX_SECTION_SIZE) {
			pr_err("section too big\n");
			return -EINVAL;
		}

		if (ins->data) {
			kfree(ins->data);
		}
		ins->data = kzalloc(size, GFP_KERNEL);
		ins->data_len = size;
		if (ins->data) {
			memcpy(ins->data, buf, size);
			ins->reformat = true;
		} else {
			err = -EINVAL;
		}
		break;

	case STM_TE_PID_INS_FILTER_CONTROL_FREQ:
		err = SET_CONTROL(ins->delay, buf, size);
		if (err == 0 && !ins->running && ins->delay != 0)
			err = te_ins_filter_schedule(ins);
		break;

	case STM_TE_PID_INS_FILTER_CONTROL_TRIG:
		/* Only inject if we're not freerunning or busy */
		err = -EAGAIN;
		if (!ins->running && ins->delay == 0)
			err = te_ins_filter_schedule(ins);
		else
			pr_warn("insertion busy, trigger ignored\n");

		break;

	case STM_TE_INPUT_FILTER_CONTROL_PID:
		/* Intercept set pid, don't want the in filter to handle it */
		err = SET_CONTROL(ins->in_filter.pid, buf, size);
		ins->reformat = true;
		break;

	default:
		err = te_in_filter_set_control(&ins->in_filter, control,
						buf, size);
	}
	return err;
}

static struct te_obj_ops te_ins_filter_ops = {
	.delete = &te_ins_filter_delete,
	.set_control = &te_ins_filter_set_control,
	.get_control = &te_ins_filter_get_control,
	.attach = &te_ins_filter_attach,
	.detach = &te_ins_filter_detach,
};

/*!
 * \brief Creates a new TE Insertion filter object
 *
 * \param demux      Parent demux for new PID filter object
 * \param pid        Initial PID to be injected by the PID filter object
 * \param new_filter Set to point to the new PID filter TE object on success
 *
 * \retval 0       Success
 * \retval -EINVAL A bad parameter was supplied
 * \retval -ENOMEM Insufficient resources to allocate the new filter
 */
int te_ins_filter_new(struct te_obj *demux, uint16_t pid,
		struct te_obj **new_filter)
{
	struct te_ins_filter *filter;
	struct te_demux *dmx = te_demux_from_obj(demux);

	int err = 0;
	char name[STM_REGISTRY_MAX_TAG_SIZE];
	ST_ErrorCode_t hal_err = ST_NO_ERROR;

	filter = kzalloc(sizeof(struct te_ins_filter), GFP_KERNEL);
	if (!filter) {
		pr_err("Failed to allocate insertion filter\n");
		return -ENOMEM;
	}

	snprintf(name, STM_REGISTRY_MAX_TAG_SIZE, "Ins.Filter.%p",
			&filter->in_filter.obj);

	err = te_in_filter_init(&filter->in_filter, demux, name,
			TE_OBJ_PID_INS_FILTER, pid);
	if (err)
		goto err1;

	filter->in_filter.obj.ops = &te_ins_filter_ops;

	INIT_LIST_HEAD(&filter->data_entries);

	hal_err = stptiHAL_call(vDevice.HAL_vDeviceSetEvent,
			dmx->hal_vdevice->hdl,
			stptiHAL_DATA_ENTRY_COMPLETE_EVENT, TRUE);
	if (ST_NO_ERROR != hal_err) {
		pr_err("unable to enable entry complete event 0x%x\n",
				hal_err);
		err = te_hal_err_to_errno(hal_err);
		goto err0;
	}

	/* Add registry attributes */
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
			TE_SYSFS_NAME_HAL_HDL "data_entries",
			TE_SYSFS_TAG_HAL_HDL_LIST,
			&filter->data_entries,
			sizeof(struct list_head));
	if (err) {
		pr_warn("Failed to add HAL data_entries attr to obj %s (%d)\n",
				name, err);
		goto err1;
	}

	*new_filter = &filter->in_filter.obj;

	return 0;
err0:
	te_in_filter_deinit(&filter->in_filter);
err1:
	kfree(filter);
	return err;

}
