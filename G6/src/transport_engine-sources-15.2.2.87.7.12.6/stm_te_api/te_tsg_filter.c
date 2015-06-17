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

Source file name : te_tsg_filter.c

Defines TSG filter specific operations
******************************************************************************/

#include <linux/slab.h>
#include <linux/spinlock.h>
#include <stm_te_dbg.h>
#include <te_object.h>
#include <stm_data_interface.h>
#include "te_tsg_filter.h"
#include "te_tsmux.h"
#include "te_global.h"
#include "te_tsmux_mme.h"
#include "te_internal_cfg.h"
#include "te_tsg_index_filter.h"
#ifdef CONFIG_RELAY
#include "st_relay.h"
#endif
#include "te_sysfs.h"

struct te_tsg_filter *te_tsg_filter_from_obj(struct te_obj *obj)
{
	if (obj->type != TE_OBJ_TSG_FILTER)
		return NULL;

	return container_of(obj, struct te_tsg_filter, obj);
}


int te_tsg_filter_stop_all(struct te_obj *tsmux_obj)
{
	struct te_tsmux *tsmux = NULL;
	struct te_tsg_filter *filter;
        tsmux = te_tsmux_from_obj(tsmux_obj);
        if (!tsmux) {
                pr_err("Bad tsmux handle\n");
                return -EINVAL;
        }
	list_for_each_entry(filter, &tsmux->tsg_filters, obj.lh) {
		filter->last_DTS = TSMUX_DEFAULT_64BIT_UNSPECIFIED;
		filter->first_DTS = TSMUX_DEFAULT_64BIT_UNSPECIFIED;
		filter->num_buffers = 0;
		filter->state = TE_TSG_STATE_STOPPED;
		filter->stream_man_paused =  0;
	}
	return 0;
}

/*!
 * \brief Initialises a TE tsg filter object
 *
 * Initialises all the internal data structures in the tsg filter object to
 * their default values
 *
 * \param filter TSG filter object to initialise
 * \param tsmux Parent tsmux for tsg filter
 * \param name   Name of tsg filter
 * \param type   Type of tsg filter
 *
 * \retval 0       Success
 * \retval -EINVAL Bad parameter
 */
int te_tsg_filter_init(struct te_tsg_filter *filter,
		struct te_obj *tsmux_obj,
		char *name, enum te_obj_type_e type)

{
	int err = 0;
	struct te_tsmux *tsmux = NULL;

	tsmux = te_tsmux_from_obj(tsmux_obj);
	if (!tsmux) {
		pr_err("Bad tsmux handle\n");
		return -EINVAL;
	}


	/* Initialise embedded TE obj */
	err = te_obj_init(&filter->obj, type, name, tsmux_obj,
			&te_global.tsg_filter_class);
	if (err)
		goto err0;

	tsmux->num_tsg ++;
	if (tsmux->num_tsg > TE_MAX_TSG_PER_TSMUX) {
		pr_err("TSG filter limit reached\n");
		err =  -ENOSYS;
		goto err1;
	}

	list_add_tail(&filter->obj.lh, &tsmux->tsg_filters);

	/* Stream Parameters */
	filter->program_id = tsmux->program_id;
	filter->stream_id = te_tsmux_get_next_stream_id(tsmux, type);
	filter->include_pcr = 0;
	filter->stream_is_pes = 0;
	filter->include_rap = 1;
	filter->stream_pid = TSMUX_DEFAULT_UNSPECIFIED;
	filter->stream_type = STM_TE_TSG_FILTER_CONTROL_STREAM_TYPE_VIDEO_MPEG2;
	filter->pes_stream_id = TSMUX_DEFAULT_UNSPECIFIED;
	filter->stream_descriptor_size = 0;
	filter->dts_integrity_threshold = te_cfg.dts_integrity_threshold;
	filter->bit_buffer_size = 2*1024*1024;
	filter->multiplexaheadlimit = te_cfg.multiplex_ahead_limit;
	filter->dontwaitlimit = te_cfg.dont_wait_limit;
	filter->dtsduration = te_cfg.dts_duration;
	filter->input_buffers = TSMUX_DEFAULT_TSG_INPUT_BUFFERS;

	pr_debug("tsmux %p program_id %u stream_id %u type %u\n",
		tsmux_obj,
		filter->program_id,
		filter->stream_id,
		type);

	/* Internal parameters */
	filter->last_DTS = TSMUX_DEFAULT_64BIT_UNSPECIFIED;
	filter->first_DTS = TSMUX_DEFAULT_64BIT_UNSPECIFIED;
	filter->ftd_first_DTS = TSMUX_DEFAULT_64BIT_UNSPECIFIED;
	filter->num_buffers = 0;
	filter->dts_discont_cnt = TSMUX_MAX_DTS_DISCONTINUITY_COUNT;
	filter->full_discont_marker = false;
	filter->eos_detected = false;
	filter->dont_pause_other = false;
	filter->total_received_frames = 0;
	filter->total_accepted_frames = 0;

	INIT_LIST_HEAD(&filter->tsg_buffer_list);
	INIT_LIST_HEAD(&filter->tsg_mme_buffer_list);


	/* Add input filter attributes to registry */
	err = stm_registry_add_attribute(&filter->obj,
			"stream_type",
			STM_REGISTRY_ADDRESS, &filter->stream_type,
			sizeof(filter->stream_type));

	err = stm_registry_add_attribute(&filter->obj,
			"tsg_state",
			STM_REGISTRY_INT32, &filter->state,
			sizeof(filter->state));

	err = stm_registry_add_attribute(&filter->obj,
			"last_DTS",
			STM_REGISTRY_ADDRESS, &filter->last_DTS,
			sizeof(filter->last_DTS));

	err = stm_registry_add_attribute(&filter->obj,
			"dts_discont_cnt",
			STM_REGISTRY_UINT32, &filter->dts_discont_cnt,
			sizeof(filter->dts_discont_cnt));

	err = stm_registry_add_attribute(&filter->obj,
			"full_discont_marker",
			STM_REGISTRY_ADDRESS, &filter->full_discont_marker,
			sizeof(filter->full_discont_marker));

	err = stm_registry_add_attribute(&filter->obj,
			"num_of_buffers",
			STM_REGISTRY_INT32, &filter->num_buffers,
			sizeof(filter->num_buffers));

	err = stm_registry_add_attribute(&filter->obj,
			"native_DTS",
			STM_REGISTRY_ADDRESS, &filter->native_DTS,
			sizeof(filter->native_DTS));

	err = stm_registry_add_attribute(&filter->obj,
			"eos_detected",
			STM_REGISTRY_ADDRESS, &filter->eos_detected,
			sizeof(filter->eos_detected));

	err = stm_registry_add_attribute(&filter->obj,
			"total_received_frames",
			STM_REGISTRY_ADDRESS, &filter->total_received_frames,
			sizeof(filter->total_received_frames));

	err = stm_registry_add_attribute(&filter->obj,
			"total_accepted_frames",
			STM_REGISTRY_ADDRESS, &filter->total_accepted_frames,
			sizeof(filter->total_accepted_frames));

	err = stm_registry_add_attribute(&filter->obj,
			"dont_pause_other",
			STM_REGISTRY_ADDRESS, &filter->dont_pause_other,
			sizeof(filter->dont_pause_other));


	filter->state = TE_TSG_STATE_NEW;

	return 0;
err1:
	te_obj_deinit(&filter->obj);
err0:
	return err;
}

/*!
 * \brief Uninitializes a TE TSG filter object
 *
 * \param filter Pointer to the TSG filter object to destroy
 *
 * \retval 0 Success
 */
int te_tsg_filter_deinit(struct te_tsg_filter *filter)
{
	int err;

	struct te_tsmux *tsmux = NULL;

	tsmux = te_tsmux_from_obj(filter->obj.parent);
	if (!tsmux) {
		pr_err("Bad tsmux handle\n");
		return -EINVAL;
	}

	err = te_obj_deinit(&filter->obj);
	if (err)
		return err;

	list_del(&filter->obj.lh);
	tsmux->num_tsg --;

	return 0;
}

/*!
 * \brief Deletes a TE TSG filter object
 *
 * \param obj TE TSG filter object to delete
 *
 * \retval 0 Success
 */
static int te_tsg_filter_delete(struct te_obj *obj)
{
	int err;
	struct te_tsg_filter *tsg_filter = container_of(obj,
			struct te_tsg_filter, obj);

	err = te_tsg_filter_deinit(tsg_filter);
	if (err)
			return err;

#ifdef CONFIG_RELAY
	/* free st relay index */
	st_relayfs_freeindex(ST_RELAY_SOURCE_TSMUX_TSG_INJECTOR,
			tsg_filter->push_relayfs_index);
#endif

	kfree(tsg_filter);
	return err;
}

/*!
 * \brief Reads a control value from an tsg filter object
 *
 * \param obj     Point to the tsg filter object to read
 * \param control Control to read
 * \param buf     Buffer populated with the read value
 * \param size    Size of buf
 *
 * \retval 0       Success
 * \retval -ENOSYS Control not available
 * \retval -EINVAL Buffer size too small to read data
 */
static int te_tsg_filter_get_control(struct te_obj *obj, uint32_t control,
		void *buf, uint32_t size)
{
	struct te_tsg_filter *tsg_filter = container_of(obj,
			struct te_tsg_filter, obj);
	int err = 0;

	switch (control) {
	case STM_TE_TSG_FILTER_CONTROL_INCLUDE_PCR:
		err = GET_CONTROL(tsg_filter->include_pcr, buf, size);
		break;
	case STM_TE_TSG_FILTER_CONTROL_STREAM_PID:
		err = GET_CONTROL(tsg_filter->stream_pid, buf, size);
		break;
	case STM_TE_TSG_FILTER_CONTROL_STREAM_TYPE:
		err = GET_CONTROL(tsg_filter->stream_type, buf, size);
		break;
	case STM_TE_TSG_FILTER_CONTROL_STREAM_IS_PES:
		err = GET_CONTROL(tsg_filter->stream_is_pes, buf, size);
		break;
	case STM_TE_TSG_FILTER_CONTROL_PES_STREAM_ID:
		err = GET_CONTROL(tsg_filter->pes_stream_id, buf, size);
		break;
	case STM_TE_TSG_FILTER_CONTROL_BIT_BUFFER_SIZE:
		err = GET_CONTROL(tsg_filter->bit_buffer_size, buf, size);
		break;
	case STM_TE_TSG_FILTER_CONTROL_STREAM_DESCRIPTOR:
	{
		stm_te_tsmux_descriptor_t *desc =
				(stm_te_tsmux_descriptor_t *) buf;

		memcpy(desc->descriptor, tsg_filter->stream_descriptor,
			tsg_filter->stream_descriptor_size);
		desc->size = tsg_filter->stream_descriptor_size;
		err = 0;
		break;
	}
	case STM_TE_TSG_FILTER_CONTROL_INCLUDE_RAP:
		err = GET_CONTROL(tsg_filter->include_rap, buf, size);
		break;
	case STM_TE_TSG_FILTER_CONTROL_IGNORE_FIRST_DTS_CHECK:
		err = GET_CONTROL(tsg_filter->ignore_first_dts_check,
					buf, size);
		break;
	case STM_TE_TSG_FILTER_CONTROL_STREAM_MANUAL_PAUSED:
		err = GET_CONTROL(tsg_filter->stream_man_paused, buf, size);
		break;
	case STM_TE_TSG_FILTER_CONTROL_SCATTERED_STREAM:
		err = GET_CONTROL(tsg_filter->scattered_stream, buf, size);
		break;
	case STM_TE_TSG_FILTER_CONTROL_IGNORE_AUTO_PAUSE:
		err = GET_CONTROL(tsg_filter->ignore_stream_auto_pause,
				buf, size);
		break;
	case STM_TE_TSG_FILTER_CONTROL_DTS_INTEGRITY_THRESHOLD:
		err = GET_CONTROL(tsg_filter->dts_integrity_threshold,
						buf, size);
		break;
	case STM_TE_TSG_FILTER_CONTROL_MULTIPLEX_AHEAD_LIMIT:
		err = GET_CONTROL(tsg_filter->multiplexaheadlimit, buf, size);
		break;
	case STM_TE_TSG_FILTER_CONTROL_DONT_WAIT_LIMIT:
		err = GET_CONTROL(tsg_filter->dontwaitlimit, buf, size);
		break;
	case STM_TE_TSG_FILTER_CONTROL_DTS_DURATION:
		err = GET_CONTROL(tsg_filter->dtsduration, buf, size);
		break;
	case STM_TE_TSG_FILTER_CONTROL_NUM_INPUT_BUFFERS:
		err = GET_CONTROL(tsg_filter->input_buffers, buf, size);
		break;
	default:
		err = -ENOSYS;
	}
	return err;
}

/*!
 * \brief Sets a control value on an tsg filter object
 *
 * \param obj     Pointer to the tsg filter object to set the control on
 * \param control Control to set
 * \param buf     Buffer containing the new control value
 * \param size    Size of the new control value in buf
 *
 * \retval 0       Success
 * \retval -ENOSYS Control not available
 * \retval -EINVAL Incorrect size for this control
 */
static int te_tsg_filter_set_control(struct te_obj *obj, uint32_t control,
		const void *buf, uint32_t size)
{
	struct te_tsg_filter *tsg_filter = container_of(obj,
			struct te_tsg_filter, obj);
	int err = 0;
	/* Allow manual pause to be active on any stage of filter */
	if (control != STM_TE_TSG_FILTER_CONTROL_STREAM_MANUAL_PAUSED
			&& !(tsg_filter->state &
			(TE_TSG_STATE_NEW | TE_TSG_STATE_CONNECTED |
			 TE_TSG_STATE_STOPPED))) {
		pr_err("Can't change tsg filter controls once running\n");
		err = -EINVAL;
		goto out;
	}

	pr_debug("Setting control %d with size %d\n", control, size);
	/* TODO Checks on the values being assigned to the controls */
	switch (control) {
	case STM_TE_TSG_FILTER_CONTROL_INCLUDE_PCR:
		err = SET_CONTROL(tsg_filter->include_pcr, buf, size);
		break;
	case STM_TE_TSG_FILTER_CONTROL_STREAM_PID:
		err = SET_CONTROL(tsg_filter->stream_pid, buf, size);
		break;
	case STM_TE_TSG_FILTER_CONTROL_STREAM_TYPE:
		err = SET_CONTROL(tsg_filter->stream_type, buf, size);
		break;
	case STM_TE_TSG_FILTER_CONTROL_STREAM_IS_PES:
		err = SET_CONTROL(tsg_filter->stream_is_pes, buf, size);
		break;
	case STM_TE_TSG_FILTER_CONTROL_PES_STREAM_ID:
		err = SET_CONTROL(tsg_filter->pes_stream_id, buf, size);
		break;
	case STM_TE_TSG_FILTER_CONTROL_BIT_BUFFER_SIZE:
		err = SET_CONTROL(tsg_filter->bit_buffer_size, buf, size);
		break;
	case STM_TE_TSG_FILTER_CONTROL_STREAM_DESCRIPTOR:
	{
		stm_te_tsmux_descriptor_t *desc =
				(stm_te_tsmux_descriptor_t *) buf;

		if (!desc->size || desc->size > STM_TE_TSMUX_MAX_DESCRIPTOR) {
			pr_err("Invalid STM_TE_TSG_FILTER_CONTROL_STREAM_DESCRIPTOR size - %d\n",
					desc->size);
			err = -EINVAL;
			break;
		}
		memcpy(tsg_filter->stream_descriptor, desc->descriptor,
			desc->size);
		tsg_filter->stream_descriptor_size = desc->size;
		err = 0;
		break;
	}
	case STM_TE_TSG_FILTER_CONTROL_INCLUDE_RAP:
		err = SET_CONTROL(tsg_filter->include_rap, buf, size);
		break;
	case STM_TE_TSG_FILTER_CONTROL_IGNORE_FIRST_DTS_CHECK:
		err = SET_CONTROL(tsg_filter->ignore_first_dts_check,
					buf, size);
		break;
	case	STM_TE_TSG_FILTER_CONTROL_STREAM_MANUAL_PAUSED:
		err = SET_CONTROL(tsg_filter->stream_man_paused, buf, size);
		break;

	case	STM_TE_TSG_FILTER_CONTROL_SCATTERED_STREAM:
		err = SET_CONTROL(tsg_filter->scattered_stream, buf, size);
		break;
	case STM_TE_TSG_FILTER_CONTROL_IGNORE_AUTO_PAUSE:
		err = SET_CONTROL(tsg_filter->ignore_stream_auto_pause, buf, size);
		break;

	case STM_TE_TSG_FILTER_CONTROL_DTS_INTEGRITY_THRESHOLD:
		err = SET_CONTROL(tsg_filter->dts_integrity_threshold,
							buf, size);
		break;
	case STM_TE_TSG_FILTER_CONTROL_MULTIPLEX_AHEAD_LIMIT:
		err = SET_CONTROL(tsg_filter->multiplexaheadlimit, buf, size);
		break;
	case STM_TE_TSG_FILTER_CONTROL_DONT_WAIT_LIMIT:
		err = SET_CONTROL(tsg_filter->dontwaitlimit, buf, size);
		break;
	case STM_TE_TSG_FILTER_CONTROL_DTS_DURATION:
		err = SET_CONTROL(tsg_filter->dtsduration, buf, size);
		break;
	case STM_TE_TSG_FILTER_CONTROL_NUM_INPUT_BUFFERS:
		err = SET_CONTROL(tsg_filter->input_buffers, buf, size);
		break;
	default:
		pr_warn("%s Unknown control %d\n", __func__, control);
		err = -ENOSYS;
		break;
	}
out:
	return err;
}

/*!
 * \brief Attaches a tsg filter to a tsg index filter
 *
 * \param tsg_filter Pointer to input filter to attach
 * \param tsg_index_filter TE obj for tsg filter to attach to
 *
 * \retval 0       Success
 * \retval -EEXIST Filters are already connected
 */
static int te_tsg_filter_attach_tsg_index_filter(
		struct te_tsg_filter *tsg_filter,
		struct te_obj *tsg_index_filter)
{
	int err = 0;
	int i;
	char link_name[STM_REGISTRY_MAX_TAG_SIZE];

	snprintf(link_name, STM_REGISTRY_MAX_TAG_SIZE, "tsg(%p)->tsg_idx(%p)",
			tsg_filter, tsg_index_filter);

	/* Check if objects are already attached */
	for (i = 0; i < TE_MAX_TSG_INDEX_PER_TSG_FILTER; i++) {
		if (tsg_filter->tsg_index_filters[i] == tsg_index_filter) {
			pr_err("TSG Filters %p and %p are already attached\n",
					&tsg_filter->obj, tsg_index_filter);
			return -EEXIST;
		}
	}

	/* Check for free internal connections */
	for (i = 0; i < TE_MAX_TSG_INDEX_PER_TSG_FILTER; i++) {
		if (NULL == tsg_filter->tsg_index_filters[i])
			break;
	}
	if (TE_MAX_TSG_INDEX_PER_TSG_FILTER == i) {
		pr_err("tsg filter %p has too many connections\n",
				&tsg_filter->obj);
		goto error;
	}

	/* Inform registry of connections */
	err = stm_registry_add_connection(&tsg_filter->obj, link_name,
			tsg_index_filter);
	if (err) {
		pr_err("Failed to register connection (%d)\n", err);
		goto error;
	}

	/* Update filter states and connection */
	tsg_filter->obj.state = TE_OBJ_STARTED;
	tsg_index_filter->state = TE_OBJ_STARTED;
	tsg_filter->tsg_index_filters[i] = tsg_index_filter;


	pr_debug("Attached tsg filter %p -> tsg index filter %p\n",
			&tsg_filter->obj,
			tsg_index_filter);

	return 0;

error:
	if (stm_registry_remove_connection(&tsg_filter->obj, link_name))
		pr_warn("Cleanup:Error removing link %s\n", link_name);
	for (i = 0; i < TE_MAX_TSG_INDEX_PER_TSG_FILTER; i++) {
		if (tsg_filter->tsg_index_filters[i] == tsg_index_filter)
			tsg_filter->tsg_index_filters[i] = NULL;
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
static int te_tsg_filter_detach_tsg_index_filter(
		struct te_tsg_filter *tsg_filter,
		struct te_obj *tsg_index_filter)
{
	int err = 0;
	int i;
	bool connected = false;
	char link_name[STM_REGISTRY_MAX_TAG_SIZE];

	snprintf(link_name, STM_REGISTRY_MAX_TAG_SIZE, "tsg(%p)->tsg_idx(%p)",
			tsg_filter, tsg_index_filter);

	/* All errors in this function are treated as non-fatal: we
	 * print a warning but continue */

	/* Remove internal connections */
	for (i = 0; i < TE_MAX_TSG_INDEX_PER_TSG_FILTER; i++) {
		if (tsg_filter->tsg_index_filters[i] == tsg_index_filter) {
			tsg_filter->tsg_index_filters[i] = NULL;
			connected = true;
		}
	}

	if (!connected) {
		pr_err("tsg (0x%p) not connected to tsg index (0x%p)",
			tsg_filter,
			tsg_index_filter);
		return -EINVAL;
	}

	/* Unregister connection */
	err = stm_registry_remove_connection(&tsg_filter->obj, link_name);
	if (err) {
		pr_warn("Failed to remove connection %s (%d)\n", link_name,
				err);
	}
	/* If the input filter has no other connections, set it's state to
	 * stopped */
	for (i = 0; i < TE_MAX_TSG_INDEX_PER_TSG_FILTER; i++) {
		if (tsg_filter->tsg_index_filters[i] != NULL)
			break;
	}
	if (TE_MAX_TSG_INDEX_PER_TSG_FILTER == i)
		tsg_filter->obj.state = TE_OBJ_STOPPED;

	return err;
}

int te_tsg_filter_attach(struct te_obj *obj, stm_object_h target)
{
	int err = 0;
	struct te_obj *te_obj;
	struct te_tsg_index_filter *tsg_index_filter;
	struct te_tsg_filter *tsg_filter = te_tsg_filter_from_obj(obj);

	if (tsg_filter == NULL)
		goto error;

	err = te_obj_from_hdl(target, &te_obj);
	if (err == 0)
		tsg_index_filter = te_tsg_index_filter_from_obj(te_obj);
	else
		tsg_index_filter = NULL;

	if (err || tsg_index_filter == NULL)
		goto error;

	/* Try tsg -> tsg index filter connection */
	err = te_tsg_filter_attach_tsg_index_filter(tsg_filter,
		&tsg_index_filter->obj);
error:
	return err;
}

int te_tsg_filter_detach(struct te_obj *obj, stm_object_h target)
{
	int err = 0;
	struct te_obj *te_obj;
	struct te_tsg_index_filter *tsg_index_filter;
	struct te_tsg_filter *tsg_filter = te_tsg_filter_from_obj(obj);

	if (tsg_filter == NULL)
		goto error;

	err = te_obj_from_hdl(target, &te_obj);
	if (err == 0)
		tsg_index_filter = te_tsg_index_filter_from_obj(te_obj);
	else
		tsg_index_filter = NULL;

	if (err || tsg_index_filter == NULL)
		goto error;

	/* Try tsg -> tsg index filter connection */
	err = te_tsg_filter_detach_tsg_index_filter(tsg_filter,
		&tsg_index_filter->obj);
error:
	return err;
}

static struct te_obj_ops te_tsg_filter_ops = {
	.delete = &te_tsg_filter_delete,
	.set_control = &te_tsg_filter_set_control,
	.get_control = &te_tsg_filter_get_control,
	.attach = &te_tsg_filter_attach,
	.detach = &te_tsg_filter_detach,
};

/*!
 * \brief Creates a new TE TSG filter object
 *
 * \param demux      Parent tsmux for new TSG filter object
 * \param new_filter Set to point to the new TSG filter TE object on success
 *
 * \retval 0       Success
 * \retval -EINVAL A bad parameter was supplied
 * \retval -ENOMEM Insufficient resources to allocate the new filter
 */
int te_tsg_filter_new(struct te_obj *tsmux, struct te_obj **new_filter)
{
	struct te_tsg_filter *filter;
	struct te_tsmux *tsmux_p;
	int err = 0;
	char name[STM_REGISTRY_MAX_TAG_SIZE];

	tsmux_p = te_tsmux_from_obj(tsmux);
	if (!tsmux_p) {
		pr_err("Bad tsmux handle\n");
		return -EINVAL;
	}

	filter = kzalloc(sizeof(struct te_tsg_filter), GFP_KERNEL);
	if (!filter) {
		pr_err("Failed to allocate TSG filter\n");
		return -ENOMEM;
	}

	snprintf(name, STM_REGISTRY_MAX_TAG_SIZE, "TSG.Filter.%p",
			&filter->obj);

	err = te_tsg_filter_init(filter, tsmux, name,
			TE_OBJ_TSG_FILTER);
	if (err)
		goto error;
	filter->obj.ops = &te_tsg_filter_ops;

	*new_filter = &filter->obj;

#ifdef CONFIG_RELAY
	err = stm_registry_add_attribute(&filter->obj, "st_relayfs_id",
			STM_REGISTRY_UINT32, &filter->push_relayfs_index,
			sizeof(filter->push_relayfs_index));
	if (err) {
		pr_warn("Failed to add st_relayfs_id to obj %s (%d)\n", name,
				err);
		goto error;
	}
	filter->push_relayfs_index = st_relayfs_getindex(
			ST_RELAY_SOURCE_TSMUX_TSG_INJECTOR);
#endif

	return 0;
error:
	te_tsg_filter_deinit(filter);
	kfree(filter);
	return err;

}

/*
 * TSG Filter async pull data interface
 */
static int te_tsg_async_connect(stm_object_h src_object,
		stm_object_h sink_object,
		struct stm_te_async_data_interface_src *async_src)
{
	int err = 0;
	struct te_obj *tsg_obj;
	struct te_tsg_filter *tsg;
	struct te_tsmux *tsmux;

	stm_te_trace_in();

	err = te_tsg_filter_from_hdl(sink_object, &tsg_obj);
	if (err) {
		pr_err("Bad tsg filter handle %p\n", sink_object);
		goto out;
	}

	if (tsg_obj->type != TE_OBJ_TSG_FILTER) {
		pr_warn("attempt to connect non-tsg filter\n");
		err = -EINVAL;
		goto out;
	}

	tsg = te_tsg_filter_from_obj(tsg_obj);

	tsg->async_src_if = async_src;
	tsg->external_connection = src_object;

	tsmux = te_tsmux_from_obj(tsg->obj.parent);
	if (!tsmux) {
		pr_err("Bad tsg filter, invalid tsmux parent\n");
		err = -EINVAL;
		goto out;
	}
	err = te_tsmux_mme_tsg_connect(tsmux, tsg);
	if (err) {
		pr_err("Can't connect tsg filter %p\n", tsg);
		goto out;
	}
out:
	stm_te_trace_out_result(err);
	return err;
}

static int te_tsg_async_disconnect(stm_object_h src_object,
		stm_object_h sink_object)
{
	int err = 0;
	struct te_obj *tsg_obj;
	struct te_tsg_filter *tsg;
	struct te_tsmux *tsmux;

	stm_te_trace_in();

	err = te_tsg_filter_from_hdl(sink_object, &tsg_obj);
	if (err) {
		pr_err("Bad tsg filter handle %p\n", sink_object);
		goto out;
	}

	if (tsg_obj->type != TE_OBJ_TSG_FILTER) {
		pr_warn("attempt to disconnect non-tsg filter\n");
		err = -EINVAL;
		goto out;
	}

	tsg = te_tsg_filter_from_obj(tsg_obj);

	tsmux = te_tsmux_from_obj(tsg->obj.parent);
	if (!tsmux) {
		pr_err("Bad tsg filter, invalid tsmux parent\n");
		err = -EINVAL;
		goto out;
	}
	err = te_tsmux_mme_tsg_disconnect(tsmux, tsg);
	if (err) {
		pr_err("Can't disconnect tsg filter %p\n", tsg);
		goto out;
	}
	tsg->external_connection = NULL;


out:
	stm_te_trace_out_result(err);
	return err;
}

static int te_tsg_queue_buffer(stm_object_h sink_h,
		struct stm_se_compressed_frame_metadata_s *metadata,
		struct stm_data_block *block_list,
		uint32_t block_count,
		uint32_t *data_blocks)
{
	int err = 0;
	struct te_obj *tsg_obj;
	struct te_tsg_filter *tsg;
	struct te_tsmux *tsmx;
	int processed = 0;

#ifdef CONFIG_RELAY
	int i;
#endif

	stm_te_trace_in();

	err = te_tsg_filter_from_hdl(sink_h, &tsg_obj);
	if (err) {
		pr_err("Bad tsg filter handle %p\n", sink_h);
		goto out;
	}

	if (tsg_obj->type != TE_OBJ_TSG_FILTER) {
		pr_warn("attempt to queue on non-tsg filter\n");
		err = -EINVAL;
		goto out;
	}

	tsg = te_tsg_filter_from_obj(tsg_obj);
	tsmx = te_tsmux_from_obj(tsg_obj->parent);

#ifdef CONFIG_RELAY
	/* Dumping data to st relayfs */
	for (i = 0; i < block_count; i++) {
		st_relayfs_write(ST_RELAY_TYPE_TSMUX_TSG_INJECTOR +
				tsg->push_relayfs_index,
				ST_RELAY_SOURCE_TSMUX_TSG_INJECTOR +
				tsg->push_relayfs_index,
				block_list[i].data_addr,
				block_list[i].len, 0);
	}
#endif

	err = te_tsmux_mme_send_buffer(tsmx, tsg, metadata, block_list,
			block_count);
	if (err)
		pr_err("Couldn't send buffer to tsmux %p\n", tsmx);
	else
		processed = block_count;

out:
	*data_blocks = processed;
	return err;
}

struct stm_te_async_data_interface_sink stm_te_tsg_data_interface = {
		te_tsg_async_connect,
		te_tsg_async_disconnect,
		te_tsg_queue_buffer,
	KERNEL,
	STM_IOMODE_BLOCKING_IO,
	ALIGN_CACHE_LINE,
	PAGE_SIZE,
	0
};

