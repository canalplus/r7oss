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

Source file name : te_object.c

Defines base te_object functions
******************************************************************************/

#include <stm_te_dbg.h>
#include <stm_registry.h>

#include <linux/string.h>
#include <linux/errno.h>

#include "te_object.h"
#include "te_global.h"
#include "te_output_filter.h"

int te_obj_init(struct te_obj *obj, enum te_obj_type_e type, char *name,
		struct te_obj *parent, struct te_obj_class *type_obj)
{
	int err;

	/* Check if we have reached the maximum number for this object type */
	if (atomic_read(&type_obj->count) >= type_obj->max) {
		pr_err("Maximum number of TE class %d reached\n",
				type_obj->class);
		err = -ENOMEM;
		goto error;
	}

	obj->type = type;
	obj->state = TE_OBJ_STOPPED;
	obj->parent = parent;
	obj->class_obj = type_obj;
	strncpy(obj->name, name, STM_REGISTRY_MAX_TAG_SIZE - 1);
	mutex_init(&obj->lock);

	/* Add object and core attributes to the registry */
	err = stm_registry_add_instance(
			parent ? parent : STM_REGISTRY_INSTANCES,
			type_obj, name, obj);
	if (err) {
		pr_err("Failed to add object %s to registry (%d)\n", name,
				err);
		goto err0;
	}

	err = stm_registry_add_attribute(obj, "type", STM_REGISTRY_UINT32,
			&obj->type, sizeof(obj->type));
	if (err) {
		pr_err("Failed to add type attr to obj %s (%d)\n", name, err);
		goto err1;
	}

	err = stm_registry_add_attribute(obj, "state", STM_REGISTRY_UINT32,
			&obj->state, sizeof(obj->state));
	if (err) {
		pr_err("Failed to add state attr to obj %s (%d)\n", name, err);
		goto err1;
	}

	/* Increment the total count of the object class (type object) */
	atomic_inc(&type_obj->count);

	return 0;

err1:
	if (stm_registry_remove_object(obj) != 0)
		pr_warn("couldn't remove object\n");
err0:
	mutex_destroy(&obj->lock);
error:
	return err;
}

stm_te_object_h te_obj_to_hdl(struct te_obj *obj)
{
	return (stm_te_object_h)obj;
}

int te_obj_deinit(struct te_obj *obj)
{
	int err;
	/* Remove object from the registry. Return error immediately, since
	 * this probably means that the object is busy and cannot be removed */
	err = stm_registry_remove_object(obj);
	if (err) {
		pr_err("Failed to remove object %s (%d)\n", obj->name, err);
		return err;
	}
	mutex_destroy(&obj->lock);
	atomic_dec(&obj->class_obj->count);

	return 0;
}

int te_valid_hdl(stm_object_h hdl)
{
	stm_object_h type_object;
	int err = stm_registry_get_object_type(hdl, &type_object);

	if (err == 0 && (type_object == &te_global.demux_class
			|| type_object == &te_global.tsmux_class
			|| type_object == &te_global.in_filter_class
			|| type_object == &te_global.out_filter_class
			|| type_object == &te_global.tsg_filter_class
			|| type_object == &te_global.tsg_index_filter_class)
	)
		return 0;

	return -EINVAL;
}
int te_obj_from_hdl(stm_object_h hdl, struct te_obj **obj)
{
	int err = te_valid_hdl(hdl);
	if (!err)
		*obj = (struct te_obj *)hdl;

	return err;
}

int te_hdl_from_obj(struct te_obj *obj, stm_object_h *hdl)
{
	*hdl = (stm_object_h)obj;
	return te_valid_hdl(*hdl);
}


int te_demux_from_hdl(stm_object_h hdl, struct te_obj **obj)
{
	int err;
	stm_object_h type_object;

	err = stm_registry_get_object_type(hdl, &type_object);
	if (!err) {
		if (type_object == &te_global.demux_class)
			*obj = (struct te_obj *)hdl;
		else
			err = -EINVAL;
	}
	return err;
}

int te_tsmux_from_hdl(stm_object_h hdl, struct te_obj **obj)
{
	int err;
	stm_object_h type_object;

	err = stm_registry_get_object_type(hdl, &type_object);
	if (!err) {
		if (type_object == &te_global.tsmux_class)
			*obj = (struct te_obj *)hdl;
		else
			err = -EINVAL;
	}
	return err;
}

int te_tsg_index_from_hdl(stm_object_h hdl, struct te_obj **obj)
{
	int err;
	stm_object_h type_object;

	err = stm_registry_get_object_type(hdl, &type_object);
	if (!err) {
		if (type_object == &te_global.tsg_index_filter_class)
			*obj = (struct te_obj *)hdl;
		else
			err = -EINVAL;
	}
	return err;
}

int te_in_filter_from_hdl(stm_object_h hdl, struct te_obj **obj)
{
	int err;
	stm_object_h type_object;

	err = stm_registry_get_object_type(hdl, &type_object);
	if (!err) {
		if (type_object == &te_global.in_filter_class)
			*obj = (struct te_obj *)hdl;
		else
			err = -EINVAL;
	}
	return err;
}

int te_tsg_filter_from_hdl(stm_object_h hdl, struct te_obj **obj)
{
	int err;
	stm_object_h type_object;

	err = stm_registry_get_object_type(hdl, &type_object);
	if (!err) {
		if (type_object == &te_global.tsg_filter_class)
			*obj = (struct te_obj *)hdl;
		else
			err = -EINVAL;
	}
	return err;
}
