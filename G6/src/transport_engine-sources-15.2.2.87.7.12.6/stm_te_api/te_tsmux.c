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

Source file name : te_tsmux.c

Defines tsmux object-specific functions
******************************************************************************/

#include <linux/slab.h>
#include "te_object.h"
#include "te_global.h"
#include "te_tsmux.h"
#include "te_output_filter.h"
#include "te_internal_cfg.h"
#include "te_interface.h"
#include "te_hal_obj.h"
#include "te_tsmux_mme.h"

#include <stm_te_dbg.h>
#include <pti_driver.h>
#include <pti_hal_api.h>
#ifdef CONFIG_RELAY
#include "st_relay.h"
#endif
#include "te_sysfs.h"

/* Local functions */
static void te_tsmux_set_buffer_size(struct te_tsmux *tsmux_p)
{
	uint32_t bitsize = tsmux_p->bitrate / (90000 / tsmux_p->pcr_period);
	tsmux_p->buffer_size = (bitsize + 7) / 8;
}

static void te_tsmux_set_max_memory(struct te_tsmux *tsmux_p)
{
	/* Set max memory usage to 1 second at mux bitrate */
	tsmux_p->max_memory = (tsmux_p->bitrate + 7) / 8;
}

static void te_tsmux_reset_parameters_stop(struct te_tsmux *tsmux_p)
{
	struct te_tsg_filter *tsg;
        /* reset only parametere which might need reset */
	tsmux_p->restart_complete = false;
	/* reset only parameters which might need reset */
	list_for_each_entry(tsg, &tsmux_p->tsg_filters, obj.lh) {
		reinit_ftd_para(tsmux_p, tsg);
		tsg->eos_detected = false;
		tsg->dont_pause_other = false;
		tsg->queued_buf = false;
	}

	tsmux_p->last_PCR = TSMUX_DEFAULT_64BIT_UNSPECIFIED;
	return;
}

static void te_tsmux_set_defaults(struct te_tsmux *tsmux_p)
{
	/* Global Parameters */
	tsmux_p->output_type = STM_TE_OUTPUT_TYPE_DVB;
	tsmux_p->pcr_period = TSMUX_DEFAULT_PCR_PERIOD;
	tsmux_p->pcr_stream = 1;
	tsmux_p->pcr_pid = TSMUX_DEFAULT_UNSPECIFIED;
	tsmux_p->table_gen = STM_TE_TSMUX_CNTRL_TABLE_GEN_PAT_PMT |
			STM_TE_TSMUX_CNTRL_TABLE_GEN_SDT;
	tsmux_p->table_period = TSMUX_DEFAULT_UNSPECIFIED;
	tsmux_p->ts_id = TSMUX_DEFAULT_UNSPECIFIED;
	tsmux_p->bitrate_is_constant = 0;
	tsmux_p->bitrate = TSMUX_DEFAULT_BITRATE;
	tsmux_p->global_flags = 0;
	/* Program Parameters */
	tsmux_p->program_id = 1;
	tsmux_p->program_number = TSMUX_DEFAULT_UNSPECIFIED;
	tsmux_p->pmt_pid = TSMUX_DEFAULT_UNSPECIFIED;
	strncpy(tsmux_p->sdt_prov_name, "Unknown", STM_TE_TSMUX_MAX_DESCRIPTOR);
	strncpy(tsmux_p->sdt_serv_name, "TSMux 1", STM_TE_TSMUX_MAX_DESCRIPTOR);
	tsmux_p->pmt_descriptor_size = 0;
	/* TE control parameters */
	tsmux_p->stop_mode = STM_TE_TSMUX_CNTRL_STOP_MODE_COMPLETE;
	/* Internal parameters */
	tsmux_p->next_stream_id = 0;
	tsmux_p->next_sec_stream_id = 0;
	tsmux_p->min_DTS = TSMUX_DEFAULT_64BIT_UNSPECIFIED;
	tsmux_p->last_PCR = TSMUX_DEFAULT_64BIT_UNSPECIFIED;
	te_tsmux_set_buffer_size(tsmux_p);
	te_tsmux_set_max_memory(tsmux_p);
	tsmux_p->memory_used = 0;
	tsmux_p->total_buffers = 0;
	tsmux_p->max_buffers = 512;
	tsmux_p->full_discont_wait_time =
					TSMUX_DEFAULT_FULL_DISCONT_WAIT_TIME;
	tsmux_p->restarting = false;
	tsmux_p->restart_complete = false;
	tsmux_p->total_ts_packets = 0;

	return;
}

/*!
 * \brief stm_te tsmux object destructor
 *
 * Deletes an stm_te tsmux object and any resource allocated to it
 *
 * \param obj Pointer to the tsmux object to destroy. After this function
 * has returned this pointer should not be dereferenced
 *
 * \retval 0      TSMux successfully deleted
 * \retval -EBUSY TSMux busy - it cannot be deleted
 * \retval -EIO   MME error freeing resources (tsmux is still deleted in this
 *                case since such error is not recoverable here)
 */
static int te_tsmux_delete(struct te_obj *obj)
{
	int err;
	struct te_tsmux *tsmx = te_tsmux_from_obj(obj);

	/* Return any error here immediately, since it likely means that the
	 * tsmux has connections or children and cannot be deleted */
	err = te_obj_deinit(&tsmx->obj);
	if (err)
		return err;

	/* Free tsmux MME resources. Continue on error */
	/* Force the tsmux to stop */
	tsmx->stop_mode = STM_TE_TSMUX_CNTRL_STOP_MODE_FORCED;
	err = te_tsmux_mme_stop(tsmx);
	if (err)
		pr_warn("Failed to free tsmux MME resources\n");

	/* Stop the MME if this is the last tsmux */
	list_del(&tsmx->obj.lh);
	if (list_empty(&te_global.tsmuxes)) {
		err = te_tsmux_mme_term();
	}

#ifdef CONFIG_RELAY
	st_relayfs_freeindex(ST_RELAY_SOURCE_TSMUX_PULL,
				tsmx->pull_relayfs_index);
#endif

	kfree(tsmx);

	return err;
}

/*!
 * \brief Retrieves stm_te control for an stm_te tsmux object
 *
 * \param obj     TE object for tsmux to read the control value from
 * \param control Control to get
 * \param buf     Buffer populated with the read value
 * \param size    Size of buf
 *
 * \retval 0       Success
 * \retval -ENOSYS Control not available
 * \retval -EINVAL Buffer size too small to read data
 */
static int te_tsmux_get_control(struct te_obj *obj, uint32_t control, void *buf,
		uint32_t size)
{
	int err = 0;
	struct te_tsmux *tsmx = te_tsmux_from_obj(obj);
	uint32_t intval;

	switch (control) {
	case STM_TE_TSMUX_CNTRL_OUTPUT_TYPE:
		intval = tsmx->output_type;
		err = GET_CONTROL(intval, buf, size);
		break;
	case STM_TE_TSMUX_CNTRL_PCR_PERIOD:
		intval = tsmx->pcr_period;
		err = GET_CONTROL(intval, buf, size);
		break;
	case STM_TE_TSMUX_CNTRL_GEN_PCR_STREAM:
		intval = tsmx->pcr_stream;
		err = GET_CONTROL(intval, buf, size);
		break;
	case STM_TE_TSMUX_CNTRL_PCR_PID:
		intval = tsmx->pcr_pid;
		err = GET_CONTROL(intval, buf, size);
		break;
	case STM_TE_TSMUX_CNTRL_TABLE_GEN:
		intval = tsmx->table_gen;
		err = GET_CONTROL(intval, buf, size);
		break;
	case STM_TE_TSMUX_CNTRL_TABLE_PERIOD:
		intval = tsmx->table_period;
		err = GET_CONTROL(intval, buf, size);
		break;
	case STM_TE_TSMUX_CNTRL_TS_ID:
		intval = tsmx->ts_id;
		err = GET_CONTROL(intval, buf, size);
		break;
	case STM_TE_TSMUX_CNTRL_BIT_RATE_IS_CONSTANT:
		intval = tsmx->bitrate_is_constant;
		err = GET_CONTROL(intval, buf, size);
		break;
	case STM_TE_TSMUX_CNTRL_BIT_RATE:
		intval = tsmx->bitrate;
		err = GET_CONTROL(intval, buf, size);
		break;
	case STM_TE_TSMUX_CNTRL_PROGRAM_NUMBER:
		intval = tsmx->program_number;
		err = GET_CONTROL(intval, buf, size);
		break;
	case STM_TE_TSMUX_CNTRL_PMT_PID:
		intval = tsmx->pmt_pid;
		err = GET_CONTROL(intval, buf, size);
		break;
	case STM_TE_TSMUX_CNTRL_SDT_PROV_NAME:
		strncpy(buf, tsmx->sdt_prov_name, size);
		err = 0;
		break;
	case STM_TE_TSMUX_CNTRL_SDT_SERV_NAME:
		strncpy(buf, tsmx->sdt_serv_name, size);
		err = 0;
		break;
	case STM_TE_TSMUX_CNTRL_PMT_DESCRIPTOR:
	{
		stm_te_tsmux_descriptor_t *desc =
				(stm_te_tsmux_descriptor_t *) buf;

		memcpy(desc->descriptor, tsmx->pmt_descriptor,
			tsmx->pmt_descriptor_size);
		desc->size = tsmx->pmt_descriptor_size;
		err = 0;
		break;
	}
	case STM_TE_TSMUX_CNTRL_STOP_MODE:
		intval = tsmx->stop_mode;
		err = GET_CONTROL(intval, buf, size);
		break;
	case STM_TE_TSMUX_CNTRL_NEXT_STREAM_ID:
		intval = tsmx->next_stream_id;
		err = GET_CONTROL(intval, buf, size);
		break;
	case STM_TE_TSMUX_CNTRL_NEXT_SEC_STREAM_ID:
		intval = tsmx->next_sec_stream_id;
		err = GET_CONTROL(intval, buf, size);
		break;

	default:
		err = -ENOSYS;
	}
	return err;
}

/*!
 * \brief Stores stm_te control data for an stm_te tsmux object
 *
 * \param obj     TE object for the tsmux to set the control value on
 * \param control Control to set
 * \param buf     Buffer containing the control value to set
 * \param size    Size of the control value in buf
 *
 * \retval 0       Success
 * \retval -ENOSYS Control not available
 * \retval -EINVAL Buffer size too small for specified control
 */
static int te_tsmux_set_control(struct te_obj *obj, uint32_t control,
		const void *buf, uint32_t size)
{
	int err = 0;
	struct te_tsmux *tsmx = te_tsmux_from_obj(obj);
	uint32_t intval;

	if (!(tsmx->state &
		(TE_TSMUX_STATE_NEW | TE_TSMUX_STATE_STOPPED)) &&
		control != STM_TE_TSMUX_CNTRL_STOP_MODE) {
		pr_err("Can't change tsmux control once it is started\n");
		err = -EINVAL;
		goto out;
	}

	/* TODO Checks on the values being assigned to the controls */
	switch (control) {
	/* TSMUX Controls */
	case STM_TE_TSMUX_CNTRL_OUTPUT_TYPE:
		err = SET_CONTROL(intval, buf, size);
		if (!err)
			tsmx->output_type = intval;
		break;
	case STM_TE_TSMUX_CNTRL_PCR_PERIOD:
		err = SET_CONTROL(intval, buf, size);
		if (!err) {
			tsmx->pcr_period = intval;
			te_tsmux_set_buffer_size(tsmx);
		}
		break;
	case STM_TE_TSMUX_CNTRL_GEN_PCR_STREAM:
		err = SET_CONTROL(intval, buf, size);
		if (!err)
			tsmx->pcr_stream = intval;
		break;
	case STM_TE_TSMUX_CNTRL_PCR_PID:
		err = SET_CONTROL(intval, buf, size);
		if (!err)
			tsmx->pcr_pid = intval;
		break;
	case STM_TE_TSMUX_CNTRL_TABLE_GEN:
		err = SET_CONTROL(intval, buf, size);
		if (!err)
			tsmx->table_gen = intval;
		break;
	case STM_TE_TSMUX_CNTRL_TABLE_PERIOD:
		err = SET_CONTROL(intval, buf, size);
		if (!err)
			tsmx->table_period = intval;
		break;
	case STM_TE_TSMUX_CNTRL_TS_ID:
		err = SET_CONTROL(intval, buf, size);
		if (!err)
			tsmx->ts_id = intval;
		break;
	case STM_TE_TSMUX_CNTRL_BIT_RATE_IS_CONSTANT:
		err = SET_CONTROL(intval, buf, size);
		if (!err)
			tsmx->bitrate_is_constant = intval;
		break;
	case STM_TE_TSMUX_CNTRL_BIT_RATE:
		err = SET_CONTROL(intval, buf, size);
		if (!err) {
			tsmx->bitrate = intval;
			te_tsmux_set_buffer_size(tsmx);
			te_tsmux_set_max_memory(tsmx);
		}
		break;
	/* Program Controls */
	case STM_TE_TSMUX_CNTRL_PROGRAM_NUMBER:
		err = SET_CONTROL(intval, buf, size);
		if (!err)
			tsmx->program_number = intval;
		break;
	case STM_TE_TSMUX_CNTRL_PMT_PID:
		err = SET_CONTROL(intval, buf, size);
		if (!err)
			tsmx->pmt_pid = intval;
		break;
	case STM_TE_TSMUX_CNTRL_SDT_PROV_NAME:
		if (strlen(buf) > STM_TE_TSMUX_MAX_DESCRIPTOR) {
			pr_err("Invalid STM_TE_TSMUX_CNTRL_SDT_PROV_NAME, too long\n");
			err = -EINVAL;
			break;
		}
		strncpy(tsmx->sdt_prov_name, buf,
			STM_TE_TSMUX_MAX_DESCRIPTOR);
		if (tsmx->sdt_prov_name[STM_TE_TSMUX_MAX_DESCRIPTOR]) {
			pr_err("Invalid STM_TE_TSMUX_CNTRL_SDT_PROV_NAME, no null\n");
			tsmx->sdt_prov_name[STM_TE_TSMUX_MAX_DESCRIPTOR] = '\0';
			err = -EINVAL;
			break;
		}
		err = 0;
		break;
	case STM_TE_TSMUX_CNTRL_SDT_SERV_NAME:
		if (strlen(buf) > STM_TE_TSMUX_MAX_DESCRIPTOR) {
			pr_err("Invalid STM_TE_TSMUX_CNTRL_SDT_SERV_NAME, too long\n");
			err = -EINVAL;
			break;
		}
		strncpy(tsmx->sdt_serv_name, buf, STM_TE_TSMUX_MAX_DESCRIPTOR);
		if (tsmx->sdt_prov_name[STM_TE_TSMUX_MAX_DESCRIPTOR]) {
			pr_err("Invalid STM_TE_TSMUX_CNTRL_SDT_SERV_NAME, no null\n");
			tsmx->sdt_prov_name[STM_TE_TSMUX_MAX_DESCRIPTOR] = '\0';
			err = -EINVAL;
			break;
		}
		err = 0;
		break;
	case STM_TE_TSMUX_CNTRL_PMT_DESCRIPTOR:
	{
		stm_te_tsmux_descriptor_t *desc =
				(stm_te_tsmux_descriptor_t *) buf;

		if (!desc->size || desc->size > STM_TE_TSMUX_MAX_DESCRIPTOR) {
			pr_err("Invalid STM_TE_TSMUX_CNTRL_PMT_DESCRIPTOR size - %d\n",
					desc->size);
			err = -EINVAL;
			break;
		}
		memcpy(tsmx->pmt_descriptor, desc->descriptor, desc->size);
		tsmx->pmt_descriptor_size = desc->size;
		err = 0;
		break;
	}
	case STM_TE_TSMUX_CNTRL_STOP_MODE:
		err = SET_CONTROL(intval, buf, size);
		if (!err)
			tsmx->stop_mode = intval;
	case STM_TE_TSMUX_CNTRL_NEXT_STREAM_ID:
		err = SET_CONTROL(intval, buf, size);
		if (!err)
			tsmx->next_stream_id = intval;
	case STM_TE_TSMUX_CNTRL_NEXT_SEC_STREAM_ID:
		err = SET_CONTROL(intval, buf, size);
		if (!err)
			tsmx->next_sec_stream_id = intval;
		break;
	default:
		err = -ENOSYS;
		break;
	}
out:
	return err;
}

static int te_tsmux_attach_to_pull_sink(struct te_obj *obj,
		stm_object_h target)
{
	int err;
	struct te_tsmux *tsmx = te_tsmux_from_obj(obj);

	pr_debug("Attaching TSMux %p to pull sink %p\n", obj,
			target);
	err = tsmx->pull_intf.connect(obj, target, tsmx->pull_src_intf);
	if (err) {
		pr_err("Failed to connect tsmux %p to pull sink %p (%d)\n",
				obj, target, err);
		goto out;
	}

	err = stm_registry_add_connection(obj, STM_DATA_INTERFACE_PULL,
			target);
	if (err) {
		pr_err("Failed to register connection %s (%d)\n",
				STM_DATA_INTERFACE_PULL, err);
		if (tsmx->pull_intf.disconnect(obj, target))
			pr_warn("Cleanup: disconnect failed\n");
	}
out:
	return err;
}

static int te_tsmux_detach_from_pull_sink(struct te_obj *obj,
		stm_object_h target)
{
	int err;
	int result = 0;

	struct te_tsmux *tsmx = te_tsmux_from_obj(obj);

	pr_debug("Detaching tsmux %p from pull sink %p\n", obj,
			target);

	err = stm_registry_remove_connection(obj, STM_DATA_INTERFACE_PULL);
	if (err) {
		pr_err("Failed to unregister connection %s (%d)\n",
				STM_DATA_INTERFACE_PULL, err);
		result = err;
	}

	err = tsmx->pull_intf.disconnect(obj, target);
	if (err) {
		pr_err("Failed to disconnect %p from pull sink %p (%d)\n",
				obj, target, err);
		result = err;
	}
	return result;
}

static int te_tsmux_attach(struct te_obj *obj, stm_object_h target)
{
	int err;
	stm_object_h target_type;
	char type_tag[STM_REGISTRY_MAX_TAG_SIZE];
	uint32_t iface_size;
	struct te_tsmux *tsmx = te_tsmux_from_obj(obj);

	/* Check if this tsmux is already attached to an external
	 * object */
	if (tsmx->external_connection) {
		pr_err("TSMux %p already has an attachment\n", obj);
		return -EBUSY;
	}

	err = stm_registry_get_object_type(target, &target_type);
	if (0 != err) {
		pr_err("unable to get type of target object %p\n", target);
		return err;
	}

	/* Look for supported interfaces of the target object.
	 * Currently looks for the following interfaces:
	 * 1. STM_DATA_INTERFACE_PULL
	 */
	if (0 == stm_registry_get_attribute(target_type,
				STM_DATA_INTERFACE_PULL, type_tag,
				sizeof(stm_data_interface_pull_sink_t),
				&tsmx->pull_intf,
				&iface_size)) {
		err = te_tsmux_attach_to_pull_sink(obj, target);
	} else {
		pr_err("Attach %p -> %p: no supported interfaces\n", obj,
				target);
		err = -EPERM;
	}

	if (!err)
		tsmx->external_connection = target;

	return err;
}

static int te_tsmux_detach(struct te_obj *obj, stm_object_h target)
{
	int err;
	stm_object_h connected_obj = NULL;
	struct te_tsmux *tsmx = te_tsmux_from_obj(obj);

	if (tsmx->external_connection != target) {
		pr_err("TSMux %p not attached to %p\n", obj, target);
		return -EINVAL;
	}

	/* Check for pull data connection */
	err = stm_registry_get_connection(te_obj_to_hdl(obj),
			STM_DATA_INTERFACE_PULL, &connected_obj);
	if (0 == err && connected_obj == target)
		err = te_tsmux_detach_from_pull_sink(obj, target);
	else
		pr_err("Error getting tsmux connection\n");

	if (!err)
		tsmx->external_connection = NULL;

	return err;
}

static struct te_obj_ops te_tsmux_ops = {
	.delete = te_tsmux_delete,
	.set_control = te_tsmux_set_control,
	.get_control = te_tsmux_get_control,
	.attach = te_tsmux_attach,
	.detach = te_tsmux_detach,
};

static int te_tsmux_pull_data(stm_object_h tsmux_h,
		struct stm_data_block *block_list, uint32_t block_count,
		uint32_t *filled_blocks)
{
	int err;
	struct te_obj *te_obj;
	struct te_tsmux *tsmx;

#ifdef CONFIG_RELAY
	int i;
#endif

	err = te_tsmux_from_hdl(tsmux_h, &te_obj);
	if (err)
		goto error;
	tsmx = te_tsmux_from_obj(te_obj);

	if (te_obj->state != TE_OBJ_STARTED) {
		pr_err("Request for data when tsmux is stopped\n");
		err = -EINVAL;
		goto error;
	}
	err = te_tsmux_mme_get_data(tsmx, block_list, block_count,
			filled_blocks);
	if (err < 0 && err != -EAGAIN)
			pr_err("Couldn't get tsmux data from %p\n", tsmx);

#ifdef CONFIG_RELAY
	for (i = 0; i < block_count; i++) {
		st_relayfs_write(ST_RELAY_TYPE_TSMUX_PULL +
				tsmx->pull_relayfs_index,
				ST_RELAY_SOURCE_TSMUX_PULL +
				tsmx->pull_relayfs_index,
				block_list[i].data_addr,
				block_list[i].len, 0);
	}
#endif

error:
	return err;
}

static int te_tsmux_pull_testfordata(stm_object_h tsmux_h, uint32_t *size)
{
	int err;
	struct te_obj *te_obj;
	struct te_tsmux *tsmx;

	*size = 0;
	err = te_tsmux_from_hdl(tsmux_h, &te_obj);
	if (err)
		goto error;
	tsmx = te_tsmux_from_obj(te_obj);

	if (te_obj->state != TE_OBJ_STARTED) {
		pr_err("Testing for data when tsmux is stopped\n");
		err = -EINVAL;
		goto error;
	}
	err = te_tsmux_mme_testfordata(tsmx, size);
	if (err)
		pr_err("Couldn't test for tsmux data from %p\n", tsmx);

error:
	return err;
}

stm_data_interface_pull_src_t stm_te_tsmux_pull_interface = {
	te_tsmux_pull_data,
	te_tsmux_pull_testfordata,
};

/* Functions */

struct te_tsmux *te_tsmux_from_obj(struct te_obj *obj)
{
	if (obj->type != TE_OBJ_TSMUX)
		return NULL;

	return container_of(obj, struct te_tsmux, obj);
}

int te_tsmux_get_next_stream_id(struct te_tsmux *tsmux_p,
				enum te_obj_type_e type)
{
	if (type == TE_OBJ_TSG_SEC_FILTER)
		return tsmux_p->next_sec_stream_id++;
	else
		return tsmux_p->next_stream_id++;
}

/*!
 * \brief Starts a te tsmux object
 *
 * A tsmux object is started by telling the tsmux worker task to start.
 *
 * \param obj TE obj for tsmux to start
 *
 * \retval 0 Success
 * \retval -EINVAL Invalid tsmux object
 */
int te_tsmux_start(struct te_obj *obj)
{
	int err = 0;
	struct te_tsmux *tsmx = te_tsmux_from_obj(obj);

	/* Note: It is not an error to start a started tsmux */
	if (TE_OBJ_STOPPED == obj->state) {
		obj->state = TE_OBJ_STARTED;
		/* Allocate MME resources for this tsmux*/
		err = te_tsmux_mme_start(tsmx);
	}
	return err;
}

/*!
 * \brief Stops a te tsmux object
 *
 * A tsmux object is "stopped" by telling the tsmux worker to stop.
 *
 * \param obj TE obj for tsmux to stop
 *
 * \retval 0 Success
 * \retval -EINVAL Invalid object
 */
int te_tsmux_stop(struct te_obj *obj)
{
	int err = 0;
	struct te_tsmux *tsmx = te_tsmux_from_obj(obj);

	/* Note: It is not an error to stop a stopped tsmux */
	if (TE_OBJ_STOPPED != obj->state) {

		if (mutex_lock_interruptible(&tsmx->obj.lock) != 0) {
			err = -EINTR;
			goto out_unlock;
		}
		obj->state = TE_OBJ_STOPPED;
		/* Allocate MME resources for this tsmux*/
		err = te_tsmux_mme_stop(tsmx);
		if (!err)
			te_tsmux_reset_parameters_stop(tsmx);
out_unlock:
		mutex_unlock(&tsmx->obj.lock);
	}
	return err;
}

/*!
 * \brief stm_te tsmux object constructor
 *
 * Creates a new tsmux object. Initialises all the internal data structures in
 * the tsmux object to their default values
 *
 * \param name String name of the new tsmux object
 * \param new_tsmux Returned pointer to the new tsmux object
 *
 * \retval 0             Success
 * \retval -ENOMEM       No memory for new tsmux
 * \retval -EIO          Hardware error
 * \retval -ENAMETOOLONG String name too long
 */
int te_tsmux_new(char *name, struct te_obj **new_tsmux)
{
	struct te_tsmux *tsmx;
	int err;

	/* Sanity check name. The registry will do this for us later anyway.
	 * This check is so that we can comply with our documented return code
	 * of -ENAMETOOLONG */
	if (strnlen(name, STM_REGISTRY_MAX_TAG_SIZE) >
			STM_REGISTRY_MAX_TAG_SIZE - 1)
		return -ENAMETOOLONG;

	tsmx = kzalloc(sizeof(struct te_tsmux), GFP_KERNEL);
	if (!tsmx) {
		pr_err("No memory for new tsmux\n");
		return -ENOMEM;
	}

	err = te_obj_init(&tsmx->obj, TE_OBJ_TSMUX, name, NULL,
			&te_global.tsmux_class);
	if (err)
		goto err1;
	tsmx->obj.ops = &te_tsmux_ops;
	tsmx->pull_src_intf = &stm_te_tsmux_pull_interface;

	/* Init demux-specific data */
	INIT_LIST_HEAD(&tsmx->tsg_filters);
	INIT_LIST_HEAD(&tsmx->pid_ins_filters);
	INIT_LIST_HEAD(&tsmx->tsg_index_filters);

	/* Initialise the tsmux default parameters */
	te_tsmux_set_defaults(tsmx);
	/* Initialised the ready wait queue */
	init_waitqueue_head(&tsmx->ready_wq);

	/* Start the MME if this is the first demux */
	if (list_empty(&te_global.tsmuxes)) {
		err = te_tsmux_mme_init();
		if (err)
			goto err0;
	}

	err = stm_registry_add_attribute(&tsmx->obj,
			"mux_state",
			STM_REGISTRY_UINT32, &tsmx->state,
			sizeof(tsmx->state));

	err = stm_registry_add_attribute(&tsmx->obj,
			"number_of_tsg_filter",
			STM_REGISTRY_UINT32, &tsmx->num_tsg,
			sizeof(tsmx->num_tsg));

	err = stm_registry_add_attribute(&tsmx->obj, "last_PCR",
			STM_REGISTRY_ADDRESS, &tsmx->last_PCR,
			sizeof(tsmx->last_PCR));

	err = stm_registry_add_attribute(&tsmx->obj, "total_memory_used",
			STM_REGISTRY_UINT32, &tsmx->memory_used,
			sizeof(tsmx->memory_used));

	err = stm_registry_add_attribute(&tsmx->obj, "total_mux_buffers",
			STM_REGISTRY_UINT32, &tsmx->total_buffers,
			sizeof(tsmx->total_buffers));

	err = stm_registry_add_attribute(&tsmx->obj, "restarting",
			STM_REGISTRY_ADDRESS, &tsmx->restarting,
			sizeof(tsmx->restarting));

	err = stm_registry_add_attribute(&tsmx->obj, "PCR_of_output_packet",
			STM_REGISTRY_ADDRESS, &tsmx->transform_status.PCR,
			sizeof(TSMUX_MME_TIMESTAMP));

	err = stm_registry_add_attribute(&tsmx->obj, "CompletedBufferCount",
			STM_REGISTRY_UINT32,
			&tsmx->transform_status.CompletedBufferCount,
			sizeof(tsmx->transform_status.CompletedBufferCount));

	err = stm_registry_add_attribute(&tsmx->obj, "NumberOfIndexRecords",
			STM_REGISTRY_UINT32,
			&tsmx->transform_status.NumberOfIndexRecords,
			sizeof(tsmx->transform_status.NumberOfIndexRecords));

	err = stm_registry_add_attribute(&tsmx->obj, "IndexRecords",
			STM_REGISTRY_ADDRESS,
			&tsmx->transform_status.IndexRecords,
			sizeof(TSMUX_IndexRecord_t));

	err = stm_registry_add_attribute(&tsmx->obj, "TSPacketsOut",
			STM_REGISTRY_UINT32,
			&tsmx->transform_status.TSPacketsOut,
			sizeof(tsmx->transform_status.TSPacketsOut));

	err = stm_registry_add_attribute(&tsmx->obj, "BitRate",
			STM_REGISTRY_UINT32, &tsmx->transform_status.Bitrate,
			sizeof(tsmx->transform_status.Bitrate));

	err = stm_registry_add_attribute(&tsmx->obj, "Total_TS_Packets",
			STM_REGISTRY_UINT32, &tsmx->total_ts_packets,
			sizeof(tsmx->total_ts_packets));

	err = stm_registry_add_attribute(&tsmx->obj, "TransformDurationMs",
			STM_REGISTRY_UINT32,
			&tsmx->transform_status.TransformDurationMs,
			sizeof(tsmx->transform_status.TransformDurationMs));


#ifdef CONFIG_RELAY
	err = stm_registry_add_attribute(&tsmx->obj, "st_relayfs_id",
				STM_REGISTRY_UINT32, &tsmx->pull_relayfs_index,
				sizeof(tsmx->pull_relayfs_index));
	if (err) {
		pr_warn("Failed to add st_relayfs_id to obj %s (%d)\n", name,
				err);
		goto err0;
	}
	tsmx->pull_relayfs_index = st_relayfs_getindex(
			ST_RELAY_SOURCE_TSMUX_PULL);
#endif

	tsmx->state = TE_TSMUX_STATE_NEW;

	list_add_tail(&tsmx->obj.lh, &te_global.tsmuxes);
	*new_tsmux = &tsmx->obj;
	return 0;
err0:
	if (list_empty(&te_global.tsmuxes))
		te_tsmux_mme_term();
	te_obj_deinit(&tsmx->obj);
err1:
	kfree(tsmx);
	return err;
}
