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

Defines PID filter specific operations
******************************************************************************/

#include <stm_te_dbg.h>
#include <pti_hal_api.h>
#include "te_object.h"
#include "te_input_filter.h"
#include "te_pid_filter.h"
#include "te_demux.h"
#include "te_hal_obj.h"

/* TE PID filter object */
struct te_pid_filter {
	struct te_in_filter in_filter;

	/* If true, this PID filter is acting as a secondary PID filter */
	bool is_secondary;

	/* Linked PID filter. If this is a regular PID filter this points to
	 * the linked secondary PID filter. If this is a secondary PID filter
	 * this points to the linked primary PID filter */
	struct te_pid_filter *linked_filter;

	/* Secondary PID filters hold a reference to the linked slot in the
	 * primary PID filter */
	struct te_hal_obj *primary_slot;

	/* Secondary PID filtering mode */
	stm_te_secondary_pid_mode_t secondary_pid_mode;

	/* store info for slot corruption*/
	/* tri state variable to manage corruption at hal layer
	 * -1 means, no action requested
	 *  0 means, disabled
	 *  1 means enabled*/
	int do_corruption;
	stm_te_pid_filter_pkt_corruption_t corrupt_param;
};

static int te_pid_filter_set_slot_corruption(struct te_pid_filter *filter,
					bool set_corruption);
static int te_pid_filter_detach(struct te_obj *obj, stm_object_h target);

static struct te_pid_filter *te_pid_filter_from_obj(struct te_obj *filter)
{

	if (filter->type != TE_OBJ_PID_FILTER)
		return NULL;

	return container_of(te_in_filter_from_obj(filter),
				struct te_pid_filter,
				in_filter);
}

static stptiHAL_SecondaryPidMode_t
	te_pid_api_to_hal_mode(stm_te_secondary_pid_mode_t mode)
{
	switch(mode) {
	case STM_TE_SECONDARY_PID_SUBSTITUTION:
		return stptiHAL_SECONDARY_PID_MODE_SUBSTITUTION;
	case STM_TE_SECONDARY_PID_INSERTION:
		return stptiHAL_SECONDARY_PID_MODE_INSERTION;
	case STM_TE_SECONDARY_PID_INSERTDELETE:
		return stptiHAL_SECONDARY_PID_MODE_INSERTDELETE;
	}

	pr_warn ("secondary PID mode == NONE\n");

	return stptiHAL_SECONDARY_PID_MODE_NONE;
}

/**
 * \brief Configures the given PID filter as a secondary PID filter
 *
 * \param filter         - PID filter to configure for secondary PID
 * \param primary_obj    - Primary PID filter to link to
 * \param mode           - Secondary PID filtering mode
 */
static int te_pid_filter_set_secondary(struct te_pid_filter *filter,
		struct te_obj *primary_obj, stm_te_secondary_pid_mode_t mode)
{
	int i;

	/* To act as a secondary PID filter, this filter must not be connected
	 * to any output filters */
	for (i = 0; i < MAX_INT_CONNECTIONS; i++) {
		if (filter->in_filter.output_filters[i]) {
			pr_err("Filter %s is connected to %s and cannot be used for secondary PID filtering\n",
				filter->in_filter.obj.name,
				filter->in_filter.output_filters[i]->name);
			return -EINVAL;
		}
	}

	/* Check primary_obj is a valid PID filter */
	if (TE_OBJ_PID_FILTER != primary_obj->type) {
		pr_err("Obj %s is not a PID filter\n", primary_obj->name);
		return -EINVAL;
	}

	filter->is_secondary = true;
	filter->linked_filter = te_pid_filter_from_obj(primary_obj);
	filter->secondary_pid_mode = mode;

	/* Create backlink */
	filter->linked_filter->linked_filter = filter;

	pr_debug("Secondary pid filter %s, linked to primary %s\n",
			filter->in_filter.obj.name,
			filter->linked_filter->in_filter.obj.name);

	return 0;
}

/**
 * \brief Updates a secondary PID filter
 *
 * \param filter - Secondary PID filter to update
 */
static int te_pid_filter_update_secondary(struct te_pid_filter *filter)
{
	int err = 0;
	struct te_hal_obj *slot_sec = NULL;
	struct te_demux *demux = te_demux_from_obj(
			filter->in_filter.obj.parent);
	u16 pri_slot_mode = 0;
	ST_ErrorCode_t hal_err;

	pr_debug("Updating secondary PID filter %s\n",
			filter->in_filter.obj.name);

	/* Is this filter already linked to the primary filter?
	 * If so check if this slot is still valid */
	if (filter->primary_slot) {
		struct te_hal_obj *slot = NULL;

		list_for_each_entry(slot,
				&filter->linked_filter->in_filter.slots,
				entry) {
			if (slot == filter->primary_slot)
				/* Primary slot still owned by filter. Nothing
				 * to do */
				return 0;
		}

		pr_debug("Pid filter %s - primary slot removed\n",
				filter->in_filter.obj.name);
		/* Primary slot removed */
		filter->primary_slot = NULL;
	}

	/* If the primary PID filter doesn't have a slot we cannot link yet and
	 * must wait for a future "update" call to inform us that the slot has
	 * been created */
	if (list_empty(&filter->linked_filter->in_filter.slots))
		return 0;

	/* This filter must have at least one slot to act as a secondary PID
	 * filter */
	if (list_empty(&filter->in_filter.slots)) {
		stptiHAL_SlotConfigParams_t slot_params = {
			.SlotMode = stptiHAL_SLOT_TYPE_RAW,
		};

		pr_debug("Allocating slot for secondary pid filter %s\n",
				filter->in_filter.obj.name);

		err = te_hal_obj_alloc(&slot_sec, demux->hal_session,
				OBJECT_TYPE_SLOT, &slot_params);
		if (err) {
			pr_err("error %d creating slot\n", err);
			return err;
		}

		list_add(&slot_sec->entry, &filter->in_filter.slots);
	} else {
		slot_sec = list_first_entry(&filter->in_filter.slots,
				struct te_hal_obj, entry);
		if (!slot_sec) {
			pr_err("error %d while looking for slots\n", -EINVAL);
			return -EINVAL;
		}
	}

	filter->primary_slot = list_first_entry(
			&filter->linked_filter->in_filter.slots,
			struct te_hal_obj, entry);

	/* Link the secondary slot (owned by this filter) to the primary slot
	 * (owned by the primary filter) */
	pr_debug("Linking slots pri=0x%x, sec=0x%x, mode=%d\n",
			filter->primary_slot->hdl.word,
			slot_sec->hdl.word,
			filter->secondary_pid_mode);
	hal_err = stptiHAL_call(Slot.HAL_SlotSetSecondaryPid, slot_sec->hdl,
			filter->primary_slot->hdl,
			te_pid_api_to_hal_mode(filter->secondary_pid_mode));
	if (ST_NO_ERROR != hal_err) {
		pr_err("Failed to set secondary slot (0x%08x, 0x%08x) e=0x%x\n",
				filter->primary_slot->hdl.word,
				slot_sec->hdl.word, hal_err);
		err = te_hal_err_to_errno(hal_err);
		goto error;
	}

	hal_err = stptiHAL_call(Slot.HAL_SlotSetPID, slot_sec->hdl,
			filter->in_filter.pid, FALSE);
	if (ST_NO_ERROR != hal_err) {
		pr_err("Failed to set PID for secondary slot e=0x%x\n",
				hal_err);
		err = te_hal_err_to_errno(hal_err);
		goto error;
	}

	if (filter->in_filter.path_id) {
		hal_err = stptiHAL_call(Slot.HAL_SlotSetSecurePathID,
				slot_sec->hdl, filter->in_filter.path_id);
		if (ST_NO_ERROR != hal_err) {
			pr_err("HAL_SlotSetSecurePathID return 0x%x\n",
					hal_err);
			err = te_hal_err_to_errno(hal_err);
			goto error;
		}
	}

	hal_err = stptiHAL_call(Slot.HAL_SlotGetMode,
				filter->primary_slot->hdl,
				&pri_slot_mode);
	if (ST_NO_ERROR != hal_err) {
		pr_err("Failed to get primary slot mode e=0x%x\n",
				hal_err);
		goto error;
	}

	if (pri_slot_mode == stptiHAL_SLOT_TYPE_RAW) {
		hal_err = stptiHAL_call(Slot.HAL_SlotFeatureEnable,
				filter->primary_slot->hdl,
				stptiHAL_SLOT_CC_FIXUP, TRUE);
		if (ST_NO_ERROR != hal_err) {
			pr_err("Failed to set CC fixup for secondary slot e=0x%x\n",
					hal_err);
			err = te_hal_err_to_errno(hal_err);
			goto error;
		}
	}

	return 0;
error:
	te_hal_obj_dec(slot_sec);

	filter->primary_slot = NULL;

	return err;
}

/**
 * \brief Set Corruption byte on the SLot for this PID filter
 *
 * \param filter - PID filter to update
 */
static int te_pid_filter_set_slot_corruption(struct te_pid_filter *filter,
		bool set_corruption)
{
	int err = 0;
	ST_ErrorCode_t hal_err;

	struct te_in_filter *in_filter = &filter->in_filter;
	struct te_hal_obj *slot;

	if (in_filter->obj.state != TE_OBJ_STARTED)
		return 0;

	/*is corruption requested by the user.
	 * default value is negetive*/
	if (filter->do_corruption < 0)
		return 0;

	list_for_each_entry(slot, &in_filter->slots, entry) {
		hal_err = stptiHAL_call(Slot.HAL_SlotSetCorruption,
				slot->hdl,
				set_corruption,
				filter->corrupt_param.offset,
				filter->corrupt_param.value);

		if (ST_NO_ERROR != hal_err) {
			pr_err("stptiHAL_SlotSetCorruption for filter %p returned 0x%x\n",
					&in_filter->obj, hal_err);
			err = te_hal_err_to_errno(hal_err);
		}
	}

	return err;
}

/**
 * \brief Stops a PID filter from performing secondary PID filtering
 *
 * \param filter - PID filter to stop performing secondary PID
 */
static int te_pid_filter_clear_secondary(struct te_pid_filter *filter)
{
	/* Unlink and delete secondary filter's slot */
	if (!list_empty(&filter->in_filter.slots)) {
		struct te_hal_obj *slot = list_first_entry(
				&filter->in_filter.slots, struct te_hal_obj,
				entry);

		if (filter->primary_slot) {
			pr_debug("Unlinking slots pri=0x%x, sec=0x%x\n",
					filter->primary_slot->hdl.word,
					slot->hdl.word);
			stptiHAL_call(Slot.HAL_SlotClearSecondaryPid,
					slot->hdl, filter->primary_slot->hdl);
		}
		te_hal_obj_dec(slot);
	}

	if (filter->linked_filter)
		filter->linked_filter->linked_filter = NULL;

	filter->primary_slot = NULL;
	filter->linked_filter = NULL;
	filter->is_secondary = false;
	filter->secondary_pid_mode = 0;

	pr_debug("Cleared secondary PID from filter %s\n",
			filter->in_filter.obj.name);

	return 0;
}

/**
 * \brief Updates the HAL resources associated with a PID filter so that they
 * reflect the state of the TE PID filter
 */
int te_pid_filter_update(struct te_obj *obj)
{
	int err = 0;
	struct te_pid_filter *filter = te_pid_filter_from_obj(obj);

	/* This has a linked secondary PID filter, which also needs updating */
	if (!filter->is_secondary && filter->linked_filter)
		err = te_pid_filter_update(
				&filter->linked_filter->in_filter.obj);

	/* This is a secondary PID filter */
	if (filter->is_secondary && !err)
		err = te_pid_filter_update_secondary(filter);

	/* Check if slot corruption is enabled on this slot*/
	if (!err)
		err = te_pid_filter_set_slot_corruption(filter,
				filter->corrupt_param.enable);

	if (!err)
		err = te_in_filter_update(&filter->in_filter);

	return err;
}

/*!
 * \brief Deletes a TE PID filter object
 *
 * \param obj TE PID filter object to delete
 *
 * \retval 0 Success
 */
static int te_pid_filter_delete(struct te_obj *obj)
{
	int err;
	struct te_pid_filter *pid_filter = te_pid_filter_from_obj(obj);
	stm_te_pid_t old_pid;

	if (pid_filter->is_secondary)
		te_pid_filter_clear_secondary(pid_filter);

	/* Announce that this PID filter's pid is changing to "NONE". This will
	 * cause all all autotargets to be disconnected */
	old_pid = pid_filter->in_filter.pid;
	err = te_demux_pid_announce_change(&pid_filter->in_filter, old_pid,
			STM_TE_PID_NONE);
	if (err) {
		/* Warn on error, but do not prevent deletion */
		pr_warn("%s te_demux_pid_announce_change failed (%d)\n",
				obj->name, err);
		err = 0;
	}

	err = te_in_filter_deinit(&pid_filter->in_filter);
	if (err)
		return err;

	kfree(pid_filter);
	return err;
}

static int te_pid_filter_get_control(struct te_obj *obj, uint32_t control,
		void *buf, uint32_t size)
{
	int res = 0;

	struct te_pid_filter *pidf = te_pid_filter_from_obj(obj);
	struct te_pid_filter_secondary_data data = {0};

	switch (control) {
	case STM_TE_PID_FILTER_CONTROL_SECONDARY_PID:
		if (pidf->is_secondary) {
			data.filter = &pidf->linked_filter->in_filter.obj;
			data.mode = pidf->secondary_pid_mode;
		}

		res = GET_CONTROL(data, buf, size);
		break;
	case STM_TE_PID_FILTER_CONTROL_PKT_CORRUPTION:
		res = GET_CONTROL(pidf->corrupt_param, buf, size);
		break;
	default:
		res = te_in_filter_get_control(&pidf->in_filter, control, buf, size);
	}

	return res;
}

static int te_pid_filter_set_control(struct te_obj *obj, uint32_t control,
		const void *buf, uint32_t size)
{
	int res = 0;

	struct te_pid_filter *pidf = te_pid_filter_from_obj(obj);
	struct te_pid_filter_secondary_data data = {0};

	switch (control) {
	case STM_TE_PID_FILTER_CONTROL_SECONDARY_PID:
		res = SET_CONTROL(data, buf, size);
		if (!res) {
			if (data.filter)
				res = te_pid_filter_set_secondary(pidf,
						data.filter, data.mode);
			else
				res = te_pid_filter_clear_secondary(pidf);
			if (!res)
				res = te_pid_filter_update(obj);
		}
		break;
	case STM_TE_PID_FILTER_CONTROL_PKT_CORRUPTION:
		res = SET_CONTROL(pidf->corrupt_param,
				buf,
				sizeof(stm_te_pid_filter_pkt_corruption_t));
		pidf->do_corruption = pidf->corrupt_param.enable;
		if (!res)
			res = te_pid_filter_update(obj);
		break;
	default:
		res = te_in_filter_set_control(&pidf->in_filter, control, buf, size);
	}

	return res;
}

static int te_pid_filter_attach(struct te_obj *obj, stm_object_h target)
{
	stm_object_h hdl;
	struct te_in_filter *inf = NULL;
	int err;

	err = te_hdl_from_obj(obj, &hdl);
	if (err == 0) {
		inf =  te_in_filter_from_obj(obj);
		err = te_in_filter_attach(inf, target);
	}
	/* if attach was successfull, update the filter control
	 * set before attach
	 * */
	if (!err) {
		err = te_pid_filter_update(obj);
		/* filter update failed, hence detach the connection*/
		if (err) {
			pr_err("%s filter(%p) update failed\n",
					obj->name, obj);
			pr_warn("%s(%p) detaching from target(%p)\n",
					obj->name, obj, target);
			te_pid_filter_detach(obj, target);
		}
	}
	return err;
}

static int te_pid_filter_detach(struct te_obj *obj, stm_object_h target)
{
	stm_object_h hdl;
	struct te_in_filter *in = NULL;
	int err;

	err = te_hdl_from_obj(obj, &hdl);
	if (err == 0) {
		in = te_in_filter_from_obj(obj);
		err = te_in_filter_detach(in, target);
	}

	return err;
}

static struct te_obj_ops te_pid_filter_ops = {
	.delete = &te_pid_filter_delete,
	.set_control = &te_pid_filter_set_control,
	.get_control = &te_pid_filter_get_control,
	.attach = &te_pid_filter_attach,
	.detach = &te_pid_filter_detach,
};

/*!
 * \brief Creates a new TE PID filter object
 *
 * \param demux      Parent demux for new PID filter object
 * \param pid        Initial PID to be capture by the PID filter object
 * \param new_filter Set to point to the new PID filter TE object on success
 *
 * \retval 0       Success
 * \retval -EINVAL A bad parameter was supplied
 * \retval -ENOMEM Insufficient resources to allocate the new filter
 */
int te_pid_filter_new(struct te_obj *demux, uint16_t pid,
		struct te_obj **new_filter)
{
	struct te_pid_filter *filter;
	int err = 0;
	char name[STM_REGISTRY_MAX_TAG_SIZE];

	filter = kzalloc(sizeof(struct te_pid_filter), GFP_KERNEL);
	if (!filter) {
		pr_err("Failed to allocate PID filter\n");
		return -ENOMEM;
	}

	snprintf(name, STM_REGISTRY_MAX_TAG_SIZE, "Pid.Filter.%p",
			&filter->in_filter.obj);

	err = te_in_filter_init(&filter->in_filter, demux, name,
			TE_OBJ_PID_FILTER, pid);
	if (err)
		goto error;

	filter->in_filter.obj.ops = &te_pid_filter_ops;
	filter->do_corruption = -1;

	*new_filter = &filter->in_filter.obj;

	te_demux_pid_announce(&filter->in_filter);

	return 0;
error:
	kfree(filter);
	return err;
}
