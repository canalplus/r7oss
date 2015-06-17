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

Source file name : te_input_filter.c

Defines input filter specific operations
******************************************************************************/

#include <stm_te_dbg.h>
#include <te_object.h>
#include <te_global.h>
#include <te_input_filter.h>
#include <te_output_filter.h>
#include <te_demux.h>
#include <te_sysfs.h>
#include <pti_hal_api.h>

/* Local function prototypes */
static int te_in_filter_attach_path(struct te_in_filter *filter,
		const stm_object_h target);
static int te_in_filter_detach_path(struct te_in_filter *filter,
		const stm_object_h target);
static int te_in_filter_attach_out_filter(struct te_in_filter *in_filter,
		struct te_obj *out_filter_p);
static int te_in_filter_detach_out_filter(struct te_in_filter *in_filter,
		struct te_obj *out_filter);
static int te_in_filter_get_stat(struct te_in_filter *filter,
			stm_te_input_filter_stats_t *stat);

/*!
 * \brief Initialises a TE input filter object
 *
 * Initialises all the internal data structures in the input filter object to
 * their default values
 *
 * \param filter Input filter object to initialise
 * \param demux Parent demux for input filter
 * \param name   Name of input filter
 * \param type   Type of input filter
 * \param pid    Initial pid to be captured by the input filter object
 *
 * \note This function allocates and initialises only stm_te-level resources
 * and does not perform any HAL operations or allocate HAL resources
 *
 * \retval 0       Success
 * \retval -EINVAL Bad parameter
 */
int te_in_filter_init(struct te_in_filter *filter, struct te_obj *demux_obj,
		char *name, enum te_obj_type_e type, uint16_t pid)
{
	int err = 0;
	struct te_demux *demux = NULL;
	stm_object_h demux_object, demod_object;

	demux = te_demux_from_obj(demux_obj);
	if (!demux) {
		pr_err("Bad demux handle\n");
		return -EINVAL;
	}

	/* Initialise embedded TE obj */
	err = te_obj_init(&filter->obj, type, name, demux_obj,
			&te_global.in_filter_class);
	if (err)
		goto err1;

	/* Add input filter attributes to registry */
	err = te_sysfs_entries_add(&filter->obj);
	if (err)
		goto err0;

	err = stm_registry_add_attribute(&filter->obj, "pid",
			STM_REGISTRY_UINT32, &filter->pid,
			sizeof(uint32_t));
	if (err) {
		pr_err("Failed to add attr pid to %s (%d)\n", name, err);
		goto err0;
	}

	err = stm_registry_add_attribute(&filter->obj, "path_id",
			STM_REGISTRY_UINT32, &filter->path_id,
			sizeof(filter->path_id));
	if (err) {
		pr_err("Failed to add attr path_id to %s (%d)\n", name, err);
		goto err0;
	}

	err = stm_registry_add_attribute(&filter->obj, "flushing",
			STM_REGISTRY_UINT32, &filter->flushing_behaviour,
			sizeof(filter->flushing_behaviour));
	if (err) {
		pr_err("Failed to add attr flushing to %s (%d)\n", name, err);
		goto err0;
	}
	err = stm_registry_add_attribute(&filter->obj,
			TE_SYSFS_NAME_HAL_HDL "slots",
			TE_SYSFS_TAG_HAL_HDL_LIST,
			&filter->slots,
			sizeof(struct list_head));
	if (err) {
		pr_warn("Failed to add HAL slots attr to obj %s (%d)\n",
				name, err);
		goto err1;
	}

	/* Initialise input filter fields with non-zero defaults to default
	 * values */
	filter->pid = pid;

	/* if the loopback function exists, use it */
	if (demux->stm_fe_bc_pid_set != NULL) {
		te_hdl_from_obj(demux_obj, &demux_object);
		demod_object = demux->upstream_obj ;
		demux->stm_fe_bc_pid_set(demod_object, demux_object, filter->pid);
	}

	INIT_LIST_HEAD(&filter->slots);

	list_add_tail(&filter->obj.lh, &demux->in_filters);

	return 0;
err0:
	te_obj_deinit(&filter->obj);
err1:
	return err;
}

/*!
 * \brief Uninitializes a TE input filter object
 *
 * \param filter Pointer to the input filter object to destroy
 *
 * \retval 0 Success
 */
int te_in_filter_deinit(struct te_in_filter *filter)
{
	struct te_demux *demux = te_demux_from_obj(filter->obj.parent);
	stm_object_h demux_object, demod_object;
	int err;

	te_sysfs_entries_remove(&filter->obj);

	err = te_obj_deinit(&filter->obj);
	if (err)
		return err;

	/* if the loopback function exists, use it */
	if (demux && demux->stm_fe_bc_pid_clear != NULL) {
		te_hdl_from_obj(&demux->obj, &demux_object);
		demod_object = demux->upstream_obj ;
		demux->stm_fe_bc_pid_clear(demod_object, demux_object, filter->pid);
	}

	list_del(&filter->obj.lh);
	return 0;
}

/*!
 * \brief Sets a control value on a input filter object
 *
 * \param obj     Pointer to the input filter object to set the control on
 * \param control Control to set
 * \param buf     Buffer containing the new control value
 * \param size    Size of the new control value in buf
 *
 * \retval 0       Success
 * \retval -ENOSYS Control not available
 * \retval -EINVAL Incorrect size for this control
 */
int te_in_filter_set_control(struct te_in_filter *filter, uint32_t control,
		const void *buf, uint32_t size)
{
	int err = 0;
	stm_te_pid_t old_pid;
	struct te_demux *demux = te_demux_from_obj(filter->obj.parent);
	stm_object_h demux_object, demod_object;

	switch (control) {
	case STM_TE_INPUT_FILTER_CONTROL_PID:
		old_pid = filter->pid;
		err = SET_CONTROL(filter->pid, buf, size);
		if (err == 0)
			err = te_demux_pid_announce_change(filter, old_pid,
					filter->pid);

		/* if the loopback function exists, use it */
		if (demux && demux->stm_fe_bc_pid_set != NULL) {
			te_hdl_from_obj(&demux->obj, &demux_object);
			demod_object = demux->upstream_obj ;
			err = demux->stm_fe_bc_pid_set(demod_object,
					demux_object,
					filter->pid);
		}
		break;
	case STM_TE_OUTPUT_FILTER_CONTROL_FLUSHING_BEHAVIOUR:
		/* TODO: This control is mis-named, since it also applies to
		 * input filters */
		err = SET_CONTROL(filter->flushing_behaviour, buf, size);
		break;
	default:
		err = -ENOSYS;
		break;
	}

	/* If the new control was valid, update the input filter config */
	if (!err)
		err = te_in_filter_update(filter);

	return err;
}

/*!
 * \brief Reads a control value from a input filter object
 *
 * \param obj     Point to the input filter object to read
 * \param control Control to read
 * \param buf     Buffer populated with the read value
 * \param size    Size of buf
 *
 * \retval 0       Success
 * \retval -ENOSYS Control not available
 * \retval -EINVAL Buffer size too small to read data
 */
int te_in_filter_get_control(struct te_in_filter *filter, uint32_t control,
		void *buf, uint32_t size)
{
	int err = 0;

	switch (control) {
	case STM_TE_INPUT_FILTER_CONTROL_PID:
		err = GET_CONTROL(filter->pid, buf, size);
		break;
	case STM_TE_OUTPUT_FILTER_CONTROL_FLUSHING_BEHAVIOUR:
		/* TODO: This control is mis-named, since it also applies to
		 * input filters */
		err = GET_CONTROL(filter->flushing_behaviour, buf, size);
		break;
	case __deprecated_STM_TE_FILTER_CONTROL_STATUS:
	case STM_TE_INPUT_FILTER_CONTROL_STATUS:
		err = te_in_filter_get_stat(filter,
					(stm_te_input_filter_stats_t *) buf);
		break;
	default:
		err = -ENOSYS;
	}
	return err;
}

/*!
 * \brief Connects a input filter to another STKPI object
 *
 * \param[in] obj    Pointer to TE obj for the input filter to connect
 * \param[in] target Object handle of the object being connected to
 *
 * \retval 0      Success
 * \retval -EPERM Do not know how to connect the specified objects
 **/
int te_in_filter_attach(struct te_in_filter *filter, stm_object_h target)
{
	int err = 0;
	char type_tag[STM_REGISTRY_MAX_TAG_SIZE];
	uint32_t iface_size;
	stm_object_h target_type;
	struct te_out_filter *out_filter;

	/* Try pid -> output filter connection */
	if (0 == te_out_filter_from_hdl(target, &out_filter)) {
		err = te_in_filter_attach_out_filter(filter,
				&out_filter->obj);
		goto out;
	}

	err = stm_registry_get_object_type(target, &target_type);
	if (0 != err) {
		pr_err("unable to get type of target object %p\n", target);
		goto out;
	}

	/* Look for supported interfaces of the target object.
	 * Currently looks for the following interfaces:
	 * 1. STM_PATH_ID_INTERFACE
	 *
	 * The order of interface checking should be modified so that the more
	 * frequently-connected interfaces are looked for first */
	if (0 == stm_registry_get_attribute(target_type,
					    STM_PATH_ID_INTERFACE, type_tag,
					    sizeof(stm_ce_path_interface_t),
					    &filter->iface_path, &iface_size)) {
		err = te_in_filter_attach_path(filter, target);
	} else {
		pr_err("unable to connect input filter %p to %p: No supported interfaces\n",
				&filter->obj, target);
		err = -EPERM;
	}
out:
	return err;
}

/*!
 * \brief Disconnects a input filter from another object
 *
 * \param[in] obj    Pointer to TE obj for the input filter to disconnect
 * \param[in] target Object handle of the object being disconnected from
 *
 * Objects must have previously been connected
 */
int te_in_filter_detach(struct te_in_filter *filter, stm_object_h target)
{
	int err = 0;
	struct te_out_filter *out_filter;
	stm_object_h connected_object = (stm_object_h)NULL;

	/* Try pid -> output filter disconnection */
	if (0 == te_out_filter_from_hdl(target, &out_filter)) {
		err = te_in_filter_detach_out_filter(filter,
				&out_filter->obj);
		goto out;
	}

	/* Check for existing connections between the filter and target
	 * The following connections are looked for:
	 * 1. STM_PATH_ID_INTERFACE
	 **/
	err = stm_registry_get_connection(&filter->obj, STM_PATH_ID_INTERFACE,
			&connected_object);
	if (0 == err && connected_object == target)
		return te_in_filter_detach_path(filter, target);

out:
	return err;
}

/*!
 * \brief Connects a input filter object to another object, which supplies the
 * interface STM_PATH_ID_INTERFACE
 *
 * \param[in] filter Pointer to input filter object to connect
 * \param[in] target Object handle of CE transform to connect to
 */
static int te_in_filter_attach_path(struct te_in_filter *filter,
		const stm_object_h target)
{
	int err;

	pr_debug("Connect input filter %p to target %p, using %s", &filter->obj,
		    target, STM_PATH_ID_INTERFACE);

	/* Call the path interface connect function (returns path_id from
	 * attached transform) */
	err = (*filter->iface_path.connect)(target, &filter->obj,
			&filter->path_id);
	if (0 != err) {
		pr_err("connect(target=%p, filter=%p) error %d\n",
			    target, &filter->obj, err);
		return err;
	}

	filter->transform = target;

	/* Inform the registry of the connection */
	err = stm_registry_add_connection(&filter->obj, STM_PATH_ID_INTERFACE,
			target);
	if (0 != err) {
		pr_err("failed to register connection (%s): %p -> %p\n",
			    STM_PATH_ID_INTERFACE, &filter->obj, target);
		goto error;
	}

	/* Update any HAL objects owned by this input filter*/
	err = te_in_filter_update(filter);
	if (err)
		goto error;

	return 0;
error:
	/* If something went wrong, we need to clean-up any state:
	 * 1. Disconnect from target
	 * 2. Unregister the connection */
	if (0 != (*filter->iface_path.disconnect)(target, &filter->obj)) {
		pr_err("disconnect(target=%p, filter=%p) failed\n",
			    target, &filter->obj);
	}
	if (0 == stm_registry_remove_connection(&filter->obj,
				STM_PATH_ID_INTERFACE)) {
		pr_err("failed to unregister connection (%s): %p -> %p\n",
			    STM_PATH_ID_INTERFACE, &filter->obj, target);
	}
	filter->transform = NULL;

	return err;
}

/*!
 * \brief Disconnects a input filter object from another object, which
 * supplies the interface STM_PATH_ID_INTERFACE
 *
 * \param[in] filter Pointer to input filter object to disconnect
 * \param[in] target Object handle of CE transform to disconnect from
 */
static int te_in_filter_detach_path(struct te_in_filter *filter,
		const stm_object_h target)
{
	int err;

	pr_debug("Disconnect filter %p from target %p using %s\n", &filter->obj,
		    target, STM_PATH_ID_INTERFACE);

	err = (*filter->iface_path.disconnect)(target, &filter->obj);
	if (err)
		pr_err("Target %p disconnect function failed, error %d\n",
			     target, err);

	/* Set the path_id to zero for this input filter and reset the CE
	 * transform connection info */
	filter->path_id = 0;
	filter->transform = NULL;
	memset(&filter->iface_path, 0, sizeof(stm_ce_path_interface_t));

	/* Remove the registry connection */
	err = stm_registry_remove_connection(&filter->obj,
			STM_PATH_ID_INTERFACE);
	if (err)
		pr_err("Unable to remove connection from %p to %p\n",
			    &filter->obj, target);

	/* Update any HAL objects owned by this input filter */
	err = te_in_filter_update(filter);
	return err;
}

/*!
 * \brief Attaches a input filter to an output filter
 *
 * \param in_filter  Pointer to input filter to attach
 * \param out_filter TE obj for output filter to attach to
 *
 * \retval 0       Success
 * \retval -EEXIST Filters are already connected
 */
static int te_in_filter_attach_out_filter(struct te_in_filter *in_filter,
		struct te_obj *out_filter)
{
	int err = 0;
	int i;
	char link_name[STM_REGISTRY_MAX_TAG_SIZE];

	snprintf(link_name, STM_REGISTRY_MAX_TAG_SIZE, "in(%p)->output(%p)",
			in_filter, out_filter);

	/* Check if objects are already attached */
	for (i = 0; i < MAX_INT_CONNECTIONS; i++) {
		if (in_filter->output_filters[i] == out_filter) {
			pr_err("Filters %p and %p are already attached\n",
					&in_filter->obj, out_filter);
			return -EEXIST;
		}
	}

	/* Check for free internal connections */
	for (i = 0; i < MAX_INT_CONNECTIONS; i++) {
		if (NULL == in_filter->output_filters[i]) {
			break;
		}
	}
	if (MAX_INT_CONNECTIONS == i) {
		pr_err("input filter %p has too many connections\n",
				&in_filter->obj);
		goto error;
	}

	/* Inform registry of connections */
	err = stm_registry_add_connection(&in_filter->obj, link_name,
			out_filter);
	if (err) {
		pr_err("Failed to register connection (%d)\n", err);
		goto error;
	}

	/* Create HAL objects resulting from this attachment */
	err = te_out_filter_connect_input(out_filter, &in_filter->obj);
	if (err)
		goto error;

	/* Update filter states and connection */
	in_filter->obj.state = TE_OBJ_STARTED;
	out_filter->state = TE_OBJ_STARTED;
	in_filter->output_filters[i] = out_filter;

	pr_debug("Attached input filter %p -> out filter %p\n", &in_filter->obj,
			out_filter);

	return 0;

error:
	if (stm_registry_remove_connection(&in_filter->obj, link_name))
		pr_warn("Cleanup:Error removing link %s\n", link_name);
	for (i = 0; i < MAX_INT_CONNECTIONS; i++) {
		if (in_filter->output_filters[i] == out_filter)
			in_filter->output_filters[i] = NULL;
	}

	return err;
}

/*!
 * \brief Disconnects a input filter from an output filter
 *
 * \param in_filter  Input filter object to disconnect
 * \param out_filter TE Output filter object to disconnect from
 *
 * \retval 0       Success
 * \retval -EINVAL No connection
 */
static int te_in_filter_detach_out_filter(struct te_in_filter *in_filter,
		struct te_obj *out_filter)
{
	int err = 0;
	int i;
	bool connected = false;
	char link_name[STM_REGISTRY_MAX_TAG_SIZE];

	snprintf(link_name, STM_REGISTRY_MAX_TAG_SIZE, "in(%p)->output(%p)",
			in_filter, out_filter);

	/* All errors in this function are treated as non-fatal: we
	 * print a warning but continue */


	/* Remove internal connections */
	for (i = 0; i < MAX_INT_CONNECTIONS; i++) {
		if (in_filter->output_filters[i] == out_filter) {
			in_filter->output_filters[i] = NULL;
			connected = true;
		}
	}

	if (!connected) {
		pr_err("output (0x%p) not connected to input (0x%p)\n", out_filter, in_filter);
		return -EINVAL;
	}

	/* Unregister connection */
	err = stm_registry_remove_connection(&in_filter->obj, link_name);
	if (err) {
		pr_warn("Failed to remove connection %s (%d)\n", link_name,
				err);
	}
	/* If the input filter has no other connections, set it's state to
	 * stopped */
	for (i = 0; i < MAX_INT_CONNECTIONS; i++) {
		if (in_filter->output_filters[i] != NULL)
			break;
	}
	if (MAX_INT_CONNECTIONS == i)
		in_filter->obj.state = TE_OBJ_STOPPED;

	/* Destroy HAL objects */
	err = te_out_filter_disconnect_input(out_filter, &in_filter->obj);
	if (err)
		pr_warn("Failed to disconnect HAL objects (err %d)"
			"filter 0x%p -> out filter 0x%p\n",
			err, &in_filter->obj, out_filter);
	else
		pr_debug("Detached input filter %p -> out filter %p\n", &in_filter->obj,
			out_filter);

	return err;
}

/*!
 * \brief Updates the HAL objects associated with a started TE input filter
 *
 * Updates the HAL slots associated with a input filter to set
 * 1. The pid
 * 2. Update the flushing behaviour
 * 3. Set the secure path id
 *
 * \param filter TE input filter object to update
 *
 * \retval 0    Success
 * \retval -EIO HAL error
 */
int te_in_filter_update(struct te_in_filter *filter)
{
	int err = 0;
	bool suppress_reset = true;
	ST_ErrorCode_t hal_err;

	struct te_hal_obj *slot;

	if (filter->obj.state != TE_OBJ_STARTED)
		return 0;

	if (STM_TE_FILTER_CONTROL_FLUSH_ON_PID_CHANGE
			&filter->flushing_behaviour)
		suppress_reset = false;

	list_for_each_entry(slot, &filter->slots, entry) {
		hal_err = stptiHAL_call(Slot.HAL_SlotSetPID,
				slot->hdl, filter->pid,
				suppress_reset);
		if (ST_NO_ERROR != hal_err) {
			pr_err("HAL_SlotSet_PID for filter %p return 0x%x\n",
					&filter->obj, hal_err);
			err = te_hal_err_to_errno(hal_err);
		}
		hal_err = stptiHAL_call(Slot.HAL_SlotSetSecurePathID,
				slot->hdl, filter->path_id);
		if (ST_NO_ERROR != hal_err) {
			pr_err("HAL_SlotSetSecurePathID return 0x%x\n",
					hal_err);
			err = te_hal_err_to_errno(hal_err);
		}
	}

	return err;
}

/*
 * PID filter interface (CE transform backlink) definition
 */

/*!
 * \brief Handles disconnect request from attached stm_ce transform
 */
static int te_in_filter_ce_disconnect_handler(stm_object_h filter_h,
		stm_object_h transform)
{
	pr_debug("filter=%p, transform=%p\n", filter_h, transform);
	return stm_te_filter_detach(filter_h, transform);

}

stm_te_pid_interface_t te_in_filter_pid_interface = {
	te_in_filter_ce_disconnect_handler,
};

struct te_in_filter *te_in_filter_from_obj(struct te_obj *filter)
{
	return container_of(filter, struct te_in_filter, obj);
}

bool te_in_filter_has_slot_space(struct te_in_filter *filter)
{
	struct te_hal_obj *slot;
	int count = 0;

	list_for_each_entry(slot, &filter->slots, entry) {
		count ++;
		if (count >= MAX_SLOT_CHAIN) {
			return false;
		}
	}

	return true;
}

/*!
 * \brief Get input filter statistics
 *
 * \param in_filter Input filter object
 * \param stat	pointer to stat
 *
 * \retval 0 Success
 * \retval -EINVAL HAL error
 */
static int te_in_filter_get_stat(struct te_in_filter *filter,
			stm_te_input_filter_stats_t *stat)
{
	ST_ErrorCode_t hal_err;
	int err = 0;
	uint32_t packet_count = 0;
	struct te_hal_obj *slot;
	struct timespec ts;
	ktime_t	ktime;

	stptiHAL_ScrambledState_t scramble_state;

	if (list_empty(&filter->slots)) {
		pr_warn("No valid slots found\n");
		stat->packet_count = 0;
		goto exit;
	}

	/* We will only use the first slot in this input filter */
	slot = list_first_entry(&filter->slots, struct te_hal_obj, entry);

	hal_err = stptiHAL_call(Slot.HAL_SlotGetState, slot->hdl, &packet_count,
				&scramble_state);
	if (ST_NO_ERROR != hal_err) {
		pr_err("could not get slot state slot (0x%x)", hal_err);
		err = hal_err;
		goto exit;
	}

	stat->packet_count = packet_count;
	getrawmonotonic(&ts);
	ktime = timespec_to_ktime(ts);
	stat->system_time = ktime_to_us(ktime);
exit:
	return err;
}
