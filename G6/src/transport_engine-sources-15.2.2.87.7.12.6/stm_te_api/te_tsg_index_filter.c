/******************************************************************************
Copyright (C) 2011, 2012, 2013 STMicroelectronics. All Rights Reserved.

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

Source file name : te_tsg_index_filter.c

Defines TSG index filter specific operations
******************************************************************************/

#include <linux/slab.h>
#include <linux/spinlock.h>
#include <stm_te_dbg.h>
#include <te_object.h>
#include <stm_data_interface.h>
#include "te_tsg_index_filter.h"
#include "te_tsmux.h"
#include "te_global.h"
#include "te_tsmux_mme.h"

struct te_tsg_index_filter *te_tsg_index_filter_from_obj(struct te_obj *obj)
{
	if (obj->type != TE_OBJ_TSG_INDEX_FILTER)
		return NULL;

	return container_of(obj, struct te_tsg_index_filter, obj);
}

static unsigned int get_tsg_index_size(struct te_tsg_index_filter *idx)
{
	unsigned int size = 0;

	if (idx->read_pointer ==
		idx->write_pointer) {
		size = 0;
	} else if (idx->read_pointer < idx->write_pointer) {
		size = (idx->write_pointer - idx->read_pointer) *
				sizeof(stm_te_tsg_index_data_t);
	} else {
		size = (idx->write_pointer +
			(idx->buffer_size-idx->read_pointer)) *
			sizeof(stm_te_tsg_index_data_t);
	}
	pr_debug("Testing index size %u bytes\n", size);
	return size;
}

static bool check_ready_size(struct te_tsg_index_filter *idx,
		struct stm_data_block *block_list, uint32_t block_count)
{
	int length = 0;
	int count = block_count;
	bool ready = false;

	if (count) {
		while (count--) {
			length += block_list->len;
			block_list++;
		}
	}

	if (mutex_lock_interruptible(&idx->obj.lock) != 0)
		return false;

	if (length <= get_tsg_index_size(idx) || idx->detach)
		ready = true;

	mutex_unlock(&idx->obj.lock);
	return ready;
}

static void te_tsg_index_filter_read(struct te_tsg_index_filter *idx,
		unsigned char *p, unsigned int size, unsigned int *bytes_copied)
{
	*bytes_copied = 0;
	do {
		if (idx->read_pointer ==
			idx->write_pointer) {
			break;
		} else if (size >= sizeof(stm_te_tsg_index_data_t)) {
			memcpy(p, &idx->index_buffer_p[idx->read_pointer++],
				sizeof(stm_te_tsg_index_data_t));
			idx->read_pointer %= idx->buffer_size;
			*bytes_copied += sizeof(stm_te_tsg_index_data_t);
			p += sizeof(stm_te_tsg_index_data_t);
			size -= sizeof(stm_te_tsg_index_data_t);
		} else
			break;
	} while (size);
}

static int te_tsg_index_filter_get_data(struct te_tsg_index_filter *idx,
		struct stm_data_block *block_list, uint32_t block_count,
		uint32_t *filled_blocks)
{
	int err  = 0;
	int total_read = 0;

	*filled_blocks = 0;

	/* Behaviour is based on io mode */
	if (idx->pull_intf.mode == STM_IOMODE_BLOCKING_IO) {
		if (!check_ready_size(idx, block_list, block_count)) {
			err = wait_event_interruptible(idx->reader_waitq,
				check_ready_size(idx, block_list, block_count));
			if (err) {
				goto error_no_unlock;
			} else if (idx->detach) {
				err = -EINTR;
				goto error_no_unlock;
			}
		}
	}
	/* Read out the data here */
	if (mutex_lock_interruptible(&idx->obj.lock) != 0){
		err = -EINTR;
		goto error_no_unlock;
	}

	while (block_count > 0) {
		unsigned int bytes_read = 0;

		te_tsg_index_filter_read(idx,
				block_list->data_addr,
				block_list->len,
				&bytes_read);
		if (err || 0 == bytes_read)
			break;

		block_list->len = bytes_read;
		(*filled_blocks)++;
		total_read += bytes_read;
		block_count--;
		block_list++;
	}
	err = total_read;
	mutex_unlock(&idx->obj.lock);
error_no_unlock:
	return err;
}

static int te_tsg_index_pull_data(stm_object_h filter,
		struct stm_data_block *block_list, uint32_t block_count,
		uint32_t *filled_blocks)
{
	int err;
	struct te_obj *te_obj;
	struct te_tsg_index_filter *tsg_index_filter;

	err = te_obj_from_hdl(filter, &te_obj);
	if (err == 0)
		tsg_index_filter = te_tsg_index_filter_from_obj(te_obj);
	else
		tsg_index_filter = NULL;

	if (err || tsg_index_filter == NULL)
		goto error;

	if (te_obj->state != TE_OBJ_STARTED) {
		pr_err("Request for data when tsg index is stopped\n");
		err = -EINVAL;
		goto error;
	}

	err = te_tsg_index_filter_get_data(tsg_index_filter, block_list,
			block_count,
			filled_blocks);

	if (err < 0 && err != -EAGAIN)
		pr_err("Couldn't get tsg index data from %p\n",
			tsg_index_filter);

error:
	return err;
}

static int te_tsg_index_pull_testfordata(stm_object_h filter, uint32_t *size)
{
	int err;
	struct te_obj *te_obj;
	struct te_tsg_index_filter *idx;

	*size = 0;

	err = te_obj_from_hdl(filter, &te_obj);
	if (err == 0)
		idx = te_tsg_index_filter_from_obj(te_obj);
	else
		idx = NULL;

	if (err || idx == NULL)
		goto error;

	if (te_obj->state != TE_OBJ_STARTED) {
		pr_err("Testing for data when tsg index is stopped\n");
		err = -EINVAL;
		goto error;
	}

	err = mutex_lock_interruptible(&idx->obj.lock);
	if (err) {
		pr_err("Couldn't lock tsg index filter from %p\n",
			idx);
		goto error;
	}
	*size = get_tsg_index_size(idx);

	mutex_unlock(&idx->obj.lock);
error:
	return err;
}

stm_data_interface_pull_src_t stm_te_tsg_index_pull_interface = {
	te_tsg_index_pull_data,
	te_tsg_index_pull_testfordata,
};

/*!
 * \brief Initialises a TE tsg index filter object
 *
 * Initialises all the internal data structures in the tsg filter object to
 * their default values
 *
 * \param filter TSG index filter object to initialise
 * \param tsmux Parent tsmux for tsg filter
 * \param name   Name of tsg filter
 * \param type   Type of tsg filter
 *
 * \retval 0       Success
 * \retval -EINVAL Bad parameter
 */
int te_tsg_index_filter_init(struct te_tsg_index_filter *filter,
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
			&te_global.tsg_index_filter_class);
	if (err)
		goto error;

	/* Add input filter attributes to registry */
	/* TODO Add any required attributes to registry */

	list_add_tail(&filter->obj.lh, &tsmux->tsg_index_filters);

	/* Set default output filter pull src interface */
	filter->pull_src_intf = &stm_te_tsg_index_pull_interface;

	filter->index_buffer_p = kzalloc(sizeof(stm_te_tsg_index_data_t) *
		te_cfg.tsg_index_buffer_size, GFP_KERNEL);
	if (!filter->index_buffer_p) {
		pr_err("Failed to allocate TSG Index filter buffer\n");
		err = -ENOMEM;
		goto error;
	};
	filter->buffer_size = te_cfg.tsg_index_buffer_size;
	filter->read_pointer = 0;
	filter->write_pointer = 0;
	filter->overflows = 0;
	filter->detach = false;
	init_waitqueue_head(&filter->reader_waitq);
	return 0;
error:
	te_obj_deinit(&filter->obj);
	return err;
}

/*!
 * \brief Uninitializes a TE TSG Index filter object
 *
 * \param filter Pointer to the TSG filter object to destroy
 *
 * \retval 0 Success
 */
int te_tsg_index_filter_deinit(struct te_tsg_index_filter *filter)
{
	int err;

	if (filter->index_buffer_p != NULL)
		kfree(filter->index_buffer_p);

	err = te_obj_deinit(&filter->obj);
	if (err)
		return err;

	list_del(&filter->obj.lh);
	return 0;
}

/*!
 * \brief Deletes a TE TSG index filter object
 *
 * \param obj TE TSG index filter object to delete
 *
 * \retval 0 Success
 */
static int te_tsg_index_filter_delete(struct te_obj *obj)
{
	int err;
	struct te_tsg_index_filter *tsg_index_filter = container_of(obj,
			struct te_tsg_index_filter, obj);

	err = te_tsg_index_filter_deinit(tsg_index_filter);
	if (err)
		return err;

	kfree(tsg_index_filter);
	return err;
}

/*!
 * \brief Reads a control value from an tsg index filter object
 *
 * \param obj     Point to the tsg index filter object to read
 * \param control Control to read
 * \param buf     Buffer populated with the read value
 * \param size    Size of buf
 *
 * \retval 0       Success
 * \retval -ENOSYS Control not available
 * \retval -EINVAL Buffer size too small to read data
 */
static int te_tsg_index_filter_get_control(struct te_obj *filter,
		uint32_t control,
		void *buf,
		uint32_t size)
{
	int err = 0;
	struct te_tsg_index_filter *tsg_index_filter
		= te_tsg_index_filter_from_obj(filter);

	pr_debug("Getting control %d with size %d\n", control, size);

	switch (control) {
	case STM_TE_TSG_INDEX_FILTER_CONTROL_INDEX:
		err = SET_CONTROL(tsg_index_filter->index_mask, buf, size);
		break;
	default:
		pr_warn("%s Unknown control %d\n", __func__, control);
		err = -ENOSYS;
		break;
	}

	return err;
}

/*!
 * \brief Sets a control value on an tsg index filter object
 *
 * \param obj     Pointer to the tsg index filter object to set the control on
 * \param control Control to set
 * \param buf     Buffer containing the new control value
 * \param size    Size of the new control value in buf
 *
 * \retval 0       Success
 * \retval -ENOSYS Control not available
 * \retval -EINVAL Incorrect size for this control
 */
static int te_tsg_index_filter_set_control(struct te_obj *filter,
		uint32_t control,
		const void *buf,
		uint32_t size)
{
	struct te_tsg_index_filter *tsg_index_filter
		= te_tsg_index_filter_from_obj(filter);
	int err = 0;

	pr_debug("Setting control %d with size %d\n", control, size);

	switch (control) {
	case STM_TE_TSG_INDEX_FILTER_CONTROL_INDEX:
		err = SET_CONTROL(tsg_index_filter->index_mask, buf, size);
		break;
	default:
		pr_warn("%s Unknown control %d\n", __func__, control);
		err = -ENOSYS;
		break;
	}

	return err;
}

static int te_tsg_index_filter_attach_to_pull_sink(struct te_obj *filter,
		stm_object_h target)
{
	int err;
	struct te_tsg_index_filter *tsg_index_filter
		= te_tsg_index_filter_from_obj(filter);

	pr_debug("Attaching tsg index filter %p to pull sink %p\n", filter,
			target);
	err = tsg_index_filter->pull_intf.connect(filter,
				target,
				tsg_index_filter->pull_src_intf);
	if (err) {
		pr_err("Failed to connect filter %p to pull sink %p (%d)\n",
				filter, target, err);
		goto out;
	}

	err = stm_registry_add_connection(filter, STM_DATA_INTERFACE_PULL,
			target);
	if (err) {
		pr_err("Failed to register connection %s (%d)\n",
				STM_DATA_INTERFACE_PULL, err);
		if (tsg_index_filter->pull_intf.disconnect(filter, target))
			pr_warn("Cleanup: disconnect failed\n");
	}
out:
	return err;
}

static int te_tsg_index_filter_detach_from_pull_sink(struct te_obj *filter,
		stm_object_h target)
{
	int err;
	int result = 0;

	struct te_tsg_index_filter *idx
		= te_tsg_index_filter_from_obj(filter);

	pr_debug("Detaching tsg index filter %p from pull sink %p\n", filter,
			target);

	err = stm_registry_remove_connection(filter, STM_DATA_INTERFACE_PULL);
	if (err) {
		pr_err("Failed to unregister connection %s (%d)\n",
				STM_DATA_INTERFACE_PULL, err);
		result = err;
	}

	if (mutex_lock_interruptible(&idx->obj.lock) != 0) {
		result = -EINTR;
		goto error;
	}

	idx->detach = true;
	wake_up_interruptible(&idx->reader_waitq);
	mutex_unlock(&idx->obj.lock);
	err = idx->pull_intf.disconnect(filter, target);
	if (err) {
		pr_err("Failed to disconnect %p from pull sink %p (%d)\n",
				filter, target, err);
		result = err;
	}
error:
	return result;
}

int te_tsg_index_filter_attach(struct te_obj *filter, stm_object_h target)
{
	int err;
	stm_object_h target_type;
	char type_tag[STM_REGISTRY_MAX_TAG_SIZE];
	uint32_t iface_size;
	struct te_tsg_index_filter *tsg_index_filter = NULL;

	if (!filter)
		return -EINVAL;

	tsg_index_filter = te_tsg_index_filter_from_obj(filter);
	if (!tsg_index_filter)
		return -EINVAL;

	/* Check if this tsg index filter is already attached to an external
	 * object */
	if (tsg_index_filter->external_connection) {
		pr_err("TSG Index filter %p already has an attachment\n",
			filter);
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
	 * 2. STM_DATA_INTERFACE_PUSH
	 */
	if (0 == stm_registry_get_attribute(target_type,
				STM_DATA_INTERFACE_PULL, type_tag,
				sizeof(stm_data_interface_pull_sink_t),
				&tsg_index_filter->pull_intf,
				&iface_size)) {

		pr_debug("attaching pull sink %p -> %p\n", filter, target);
		err = te_tsg_index_filter_attach_to_pull_sink(filter, target);
	} else if (0 == stm_registry_get_attribute(target_type,
				STM_DATA_INTERFACE_PUSH, type_tag,
				sizeof(stm_data_interface_push_sink_t),
				&tsg_index_filter->push_intf,
				&iface_size)) {
		pr_debug("attach push sink %p -> %p not supported\n",
			filter,
			target);
	} else {
		pr_err("Attach %p -> %p: no supported interfaces\n", filter,
				target);
		err = -EPERM;
	}

	if (err == 0 && tsg_index_filter->pull_intf.connect) {
		stm_data_interface_pull_sink_t tmp_sink;
		if (0 == stm_registry_get_attribute(target,
				STM_DATA_INTERFACE_PULL, type_tag,
				sizeof(stm_data_interface_pull_sink_t),
				&tmp_sink,
				&iface_size))
			tsg_index_filter->pull_intf.mode = tmp_sink.mode;
	}

	pr_debug("interfaces: push.connect %p, pull.connect %p\n",
			tsg_index_filter->push_intf.connect,
			tsg_index_filter->pull_intf.connect);


	if (!err)
		tsg_index_filter->external_connection = target;

	return err;
}

int te_tsg_index_filter_detach(struct te_obj *filter, stm_object_h target)
{
	int err;
	stm_object_h target_type;
	stm_object_h connected_obj = NULL;
	char type_tag[STM_REGISTRY_MAX_TAG_SIZE];
	uint32_t iface_size;
	struct te_tsg_index_filter *tsg_index_filter = NULL;

	if (!filter)
		return -EINVAL;

	tsg_index_filter = te_tsg_index_filter_from_obj(filter);
	if (!tsg_index_filter)
		return -EINVAL;

	if (tsg_index_filter->external_connection != target) {
		pr_err("tsg index filter %p not attached to %p\n",
			filter,
			target);
		return -EINVAL;
	}

	/* Check for pull or push data connection */
	err = stm_registry_get_object_type(target, &target_type);
	if (0 != err) {
		pr_err("unable to get type of target object %p\n", target);
		return err;
	}

	/* Look for supported interfaces of the target object.
	 * Currently looks for the following interfaces:
	 * 1. STM_DATA_INTERFACE_PULL
	 * 2. STM_DATA_INTERFACE_PUSH
	 */
	if (0 == stm_registry_get_attribute(target_type,
				STM_DATA_INTERFACE_PULL, type_tag,
				sizeof(stm_data_interface_pull_sink_t),
				&tsg_index_filter->pull_intf,
				&iface_size)) {
		pr_debug("detaching from pull sink %p\n", target);
		err = stm_registry_get_connection(te_obj_to_hdl(filter),
				STM_DATA_INTERFACE_PULL, &connected_obj);

		if (0 == err && connected_obj == target)
			err = te_tsg_index_filter_detach_from_pull_sink(filter,
					target);
	} else if (0 == stm_registry_get_attribute(target_type,
				STM_DATA_INTERFACE_PUSH, type_tag,
				sizeof(stm_data_interface_push_sink_t),
				&tsg_index_filter->push_intf,
				&iface_size)) {
		pr_debug("Attached to push sink %p not supported\n", target);
	} else {
		pr_err("Detach %p -> %p: no supported interfaces\n", filter,
				target);
		err = -EPERM;
	}

	if (!err)
		tsg_index_filter->external_connection = NULL;

	return err;
}

static struct te_obj_ops te_tsg_index_filter_ops = {
	.delete = &te_tsg_index_filter_delete,
	.set_control = &te_tsg_index_filter_set_control,
	.get_control = &te_tsg_index_filter_get_control,
	.attach = &te_tsg_index_filter_attach,
	.detach = &te_tsg_index_filter_detach,
};

/*!
 * \brief Creates a new TE TSG index filter object
 *
 * \param tsmux      Parent tsmux for new TSG index filter object
 * \param new_filter Set to point to the new TSG filter TE object on success
 *
 * \retval 0       Success
 * \retval -EINVAL A bad parameter was supplied
 * \retval -ENOMEM Insufficient resources to allocate the new filter
 */
int te_tsg_index_filter_new(struct te_obj *tsmux, struct te_obj **new_filter)
{
	struct te_tsg_index_filter *filter;
	struct te_tsmux *tsmux_p;
	int err = 0;
	char name[STM_REGISTRY_MAX_TAG_SIZE];

	tsmux_p = te_tsmux_from_obj(tsmux);
	if (!tsmux_p) {
		pr_err("Bad tsmux handle\n");
		return -EINVAL;
	}

	filter = kzalloc(sizeof(struct te_tsg_index_filter), GFP_KERNEL);
	if (!filter) {
		pr_err("Failed to allocate TSG Index filter\n");
		return -ENOMEM;
	}

	snprintf(name, STM_REGISTRY_MAX_TAG_SIZE, "TSG.IndexFilter.%p",
			&filter->obj);

	err = te_tsg_index_filter_init(filter, tsmux, name,
			TE_OBJ_TSG_INDEX_FILTER);
	if (err)
		goto error;
	filter->obj.ops = &te_tsg_index_filter_ops;

	*new_filter = &filter->obj;
	return 0;
error:
	te_tsg_index_filter_deinit(filter);
	kfree(filter);
	return err;

}
