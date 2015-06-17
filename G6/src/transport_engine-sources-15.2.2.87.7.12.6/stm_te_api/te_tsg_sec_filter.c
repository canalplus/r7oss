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

Source file name : te_tsg_sec_filter.c

Defines non-program section insertion filter
******************************************************************************/
#include <linux/mutex.h>

#include <stm_te_dbg.h>
#include <te_tsmux_mme.h>

#include "te_internal_cfg.h"
#include "te_object.h"
#include "te_tsg_filter.h"
#include "te_tsg_sec_filter.h"
#ifdef CONFIG_RELAY
#include "st_relay.h"
#endif

struct te_tsg_sec_filter *te_tsg_sec_filter_from_obj(struct te_obj *obj)
{
	if (obj->type != TE_OBJ_TSG_SEC_FILTER)
		return NULL;

	return container_of(obj, struct te_tsg_sec_filter, tsg.obj);
}

static int te_tsg_sec_filter_delete(struct te_obj *obj)
{
	int err;
	struct te_tsg_sec_filter *sec = te_tsg_sec_filter_from_obj(obj);

	if (sec == NULL)
		return -EINVAL;

	if (mutex_lock_interruptible(&sec->tsg.obj.lock) != 0)
		return -EINTR;

	err = te_tsg_filter_deinit(&sec->tsg);
	if (err)
		return err;

	if (sec->table)
		kfree(sec->table);

#ifdef CONFIG_RELAY
	/* free st relay index */
	st_relayfs_freeindex(ST_RELAY_SOURCE_TSMUX_TSG_SEC_INJECTOR,
			sec->push_sec_relayfs_index);
#endif

	mutex_unlock(&sec->tsg.obj.lock);

	kfree(sec);
	return 0;
}

static int te_tsg_sec_filter_copy_data(struct te_tsg_sec_filter *sec,
		const void *buf, uint32_t size)
{
	int err = 0;

	if (size > TE_MAX_SECTION_SIZE) {
		err = -EINVAL;
		goto done;
	}

	if (sec->table)
		kfree(sec->table);

	sec->table = kzalloc(size, GFP_KERNEL);

	if (sec->table) {
		sec->table_len = size;
		memcpy(sec->table, buf, size);
	} else {
		err = -ENOMEM;
	}

done:
	return err;
}

static int te_tsg_sec_filter_set_control(struct te_obj *obj, uint32_t control,
		const void *buf, uint32_t size)
{
	int err = 0;

	struct te_tsg_sec_filter *sec = te_tsg_sec_filter_from_obj(obj);
	struct te_tsmux *mux = te_tsmux_from_obj(obj->parent);

	if (!sec || !mux)
		return -EINVAL;

	switch (control) {

	case STM_TE_TSG_SEC_FILTER_CONTROL_DATA:

		if (mutex_lock_interruptible(&sec->tsg.obj.lock) != 0)
			return -EINTR;
		err = te_tsg_sec_filter_copy_data(sec, buf, size);
		mutex_unlock(&sec->tsg.obj.lock);

		break;

	case STM_TE_TSG_SEC_FILTER_CONTROL_FREQ:

		if (sec->tsg.state  != TE_TSG_STATE_RUNNING) {
			err = SET_CONTROL(sec->rep_ivl, buf, size);
			goto done;
		}

#ifdef CONFIG_RELAY
	/* Dumping data to st relayfs */
		st_relayfs_write(ST_RELAY_TYPE_TSMUX_TSG_SEC_INJECTOR +
				sec->push_sec_relayfs_index,
				ST_RELAY_SOURCE_TSMUX_TSG_SEC_INJECTOR +
				sec->push_sec_relayfs_index,
				sec->table,
				sec->table_len, 0);
#endif

		err = SET_CONTROL(sec->rep_ivl, buf, size);

		if (err == 0)
			err = te_tsmux_mme_submit_section(mux, sec);

		break;

	case STM_TE_TSG_FILTER_CONTROL_NUM_INPUT_BUFFERS:
		err = SET_CONTROL(sec->tsg.input_buffers, buf, size);
		break;
	case STM_TE_TSG_FILTER_CONTROL_STREAM_PID:
		if (!(sec->tsg.state &
			(TE_TSG_STATE_NEW | TE_TSG_STATE_CONNECTED))) {
			pr_err("Can't change tsg sec PID when running\n");
			err = -EINVAL;
		} else {
			err = SET_CONTROL(sec->tsg.stream_pid, buf, size);
		}
		break;

	default:
		err = -EINVAL;
	}
done:
	return err;
}
static int te_tsg_sec_filter_get_control(struct te_obj *obj, uint32_t control,
		void *buf, uint32_t size)
{
	int err = 0;
	struct te_tsg_sec_filter *sec = te_tsg_sec_filter_from_obj(obj);

	if (!sec)
		return -EINVAL;

	switch (control) {

	case STM_TE_TSG_SEC_FILTER_CONTROL_DATA:
		if (size >= sec->table_len && sec->table)
			memcpy(buf, sec->table, sec->table_len);
		else
			err = -EINVAL;
		break;

	case STM_TE_TSG_FILTER_CONTROL_NUM_INPUT_BUFFERS:
		err = GET_CONTROL(sec->tsg.input_buffers, buf, size);
		break;
	case STM_TE_TSG_SEC_FILTER_CONTROL_FREQ:
		err = GET_CONTROL(sec->rep_ivl, buf, size);
		break;

	case STM_TE_TSG_FILTER_CONTROL_STREAM_PID:
		err = GET_CONTROL(sec->tsg.stream_pid, buf, size);
		break;
	default:
		err = -EINVAL;
	}
	return err;
}

static struct te_obj_ops te_tsg_sec_filter_ops = {
	.delete = te_tsg_sec_filter_delete,
	.set_control = te_tsg_sec_filter_set_control,
	.get_control = te_tsg_sec_filter_get_control,
	.attach = te_tsg_filter_attach,
	.detach = te_tsg_filter_detach,
};

/*!
 * \brief Creates a new TE TSG Section filter object
 *
 * \param demux      Parent tsmux
 * \param new_filter Set to point to the new TSG Section filter
 *
 * \retval 0       Success
 * \retval -EINVAL A bad parameter was supplied
 * \retval -ENOMEM Insufficient resources to allocate the new filter
 */
int te_tsg_sec_filter_new(struct te_obj *tsmux, struct te_obj **new_filter)
{
	struct te_tsg_sec_filter *filter;
	struct te_tsmux *tsmux_p;
	int err = 0;
	char name[STM_REGISTRY_MAX_TAG_SIZE];

	tsmux_p = te_tsmux_from_obj(tsmux);
	if (!tsmux_p) {
		pr_err("Bad tsmux handle\n");
		return -EINVAL;
	}

	filter = kzalloc(sizeof(*filter), GFP_KERNEL);
	if (!filter) {
		pr_err("Failed to allocate TSG SEC filter\n");
		return -ENOMEM;
	}

	snprintf(name, STM_REGISTRY_MAX_TAG_SIZE, "TSG-SEC.Filter.%p",
			&filter->tsg.obj);

	err = te_tsg_filter_init(&filter->tsg, tsmux, name,
			TE_OBJ_TSG_SEC_FILTER);
	if (err)
		goto error;

	filter->tsg.program_id = tsmux_p->program_id + 1;
	filter->tsg.obj.ops = &te_tsg_sec_filter_ops;
	filter->tsg.stream_is_pes = true;
	filter->tsg.dts_integrity_threshold = 0;

	init_completion(&filter->done);

#ifdef CONFIG_RELAY
	err = stm_registry_add_attribute(&filter->tsg.obj, "st_relayfs_id",
			STM_REGISTRY_UINT32, &filter->push_sec_relayfs_index,
			sizeof(filter->push_sec_relayfs_index));
	if (err) {
		pr_warn("Failed to add st_relayfs_id to obj %s (%d)\n", name,
				err);
		goto error;
	}
	filter->push_sec_relayfs_index = st_relayfs_getindex(
			ST_RELAY_SOURCE_TSMUX_TSG_SEC_INJECTOR);
#endif

	*new_filter = &filter->tsg.obj;
	return 0;

error:
	kfree(filter);
	return err;

}
