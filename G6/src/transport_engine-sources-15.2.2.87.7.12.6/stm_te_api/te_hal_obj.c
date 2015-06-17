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

Source file name : te_hal_obj.c

Declares TE HAL objects
******************************************************************************/
#include "te_hal_obj.h"
#include "te_internal_cfg.h"
#include "te_object.h"

#include <stm_te_dbg.h>

static void release_obj(struct kref *ref)
{
	struct te_hal_obj *obj = container_of(ref, struct te_hal_obj, refcount);

	te_hal_obj_print(obj, "destroying object");
	
	if (ST_NO_ERROR != stptiOBJMAN_DeallocateObject(obj->hdl, TRUE)) {
		pr_err("unable to deallocate handle 0x%08x\n", obj->hdl.word);
	}

	list_del(&obj->entry);
	list_del(&obj->c_entry);

	if (obj->parent)
		kref_put(&obj->parent->refcount, obj->parent->release);

	kfree(obj);
}

int __te_hal_obj_alloc(struct te_hal_obj **obj, struct te_hal_obj *parent, ObjectType_t type, void *params, const char *pfx)
{
	int res = 0;
	FullHandle_t parent_hdl = stptiOBJMAN_NullHandle;
	ST_ErrorCode_t hal_err;

	struct te_hal_obj *new_obj = kzalloc(sizeof(*new_obj), GFP_KERNEL);

	if (!new_obj) {
		pr_err("couldn't allocate object\n");
		res = -ENOMEM;
		goto error;
	}

	kref_init(&new_obj->refcount);

	new_obj->release = release_obj;

	if (parent)
		parent_hdl = parent->hdl;

	hal_err = stptiOBJMAN_AllocateObject(
			parent_hdl, type, &new_obj->hdl, params);

	if (ST_NO_ERROR != hal_err) {
		pr_err("failed to allocate device type %d (err 0x%x)\n",
			type, hal_err);
		res = te_hal_err_to_errno(hal_err);
		goto error;
	}

	if (HAL_HDL_IS_NULL(new_obj->hdl)) {
		pr_err("allocated returned NULL pointer but didn't fail\n");
		res = -ENOMEM;
		goto error;
	}

	INIT_LIST_HEAD(&new_obj->entry);
	INIT_LIST_HEAD(&new_obj->children);
	INIT_LIST_HEAD(&new_obj->c_entry);

	if (parent) {
		new_obj->parent = parent;
		kref_get(&parent->refcount);
		list_add(&new_obj->c_entry, &parent->children);
	} else {
		pr_warning("Orphan HAL object created!\n");
	}


	*obj = new_obj;
	te_hal_obj_print(new_obj, pfx);

	return 0;

error:
	*obj = NULL;
	if (new_obj)
		kfree(new_obj);
	return res;
}
void te_hal_obj_print(struct te_hal_obj *obj, const char *pfx)
{
	const char *type;

	if (!obj) {
		pr_debug("%s\nHAL obj NULL!\n", pfx);
		return;
	}

	switch (obj->hdl.member.ObjectType) {
	case OBJECT_TYPE_INVALID:
		type = "Invalid";
		break;
	case OBJECT_TYPE_PDEVICE:
		type = "pDevice";
		break;
	case OBJECT_TYPE_VDEVICE:
		type = "vDevice";
		break;
	case OBJECT_TYPE_SESSION:
		type = "session";
		break;
	case OBJECT_TYPE_SOFTWARE_INJECTOR:
		type = "sw injector";
		break;
	case OBJECT_TYPE_BUFFER:
		type = "buffer";
		break;
	case OBJECT_TYPE_FILTER:
		type = "filter";
		break;
	case OBJECT_TYPE_SIGNAL:
		type = "signal";
		break;
	case OBJECT_TYPE_INDEX:
		type = "index";
		break;
	case OBJECT_TYPE_DATA_ENTRY:
		type = "data entry";
		break;
	case OBJECT_TYPE_SLOT:
		type = "slot";
		break;
	case OBJECT_TYPE_CONTAINER:
		type = "container";
		break;

	case OBJECT_TYPE_ANY:
		type = "any";
		break;
    
	case STPTI_HANDLE_INVALID_OBJECT_TYPE:
		type = "invalid";
		break;
	default:
		type = "unknown";
		break;
	}

	pr_debug("%s\n"
		 "HAL obj 0x%p:\n"
		 "  handle:  0x%08x\n"
		 "  type:    %s\n"
		 "  num:     %d\n"
		 "  session: %d\n"
		 "  vDevice: %d\n"
		 "  pDevice: %d\n", pfx, obj, obj->hdl.word, type,
				    obj->hdl.member.Object,
				    obj->hdl.member.Session,
				    obj->hdl.member.vDevice,
				    obj->hdl.member.pDevice);
}
void te_hal_obj_dump_tree(struct te_hal_obj *obj)
{
	struct te_hal_obj *next;

	if (!obj) {
		pr_debug("HAL obj NULL!\n");
		return;
	}

	te_hal_obj_print(obj, "");

	list_for_each_entry(next, &obj->children, c_entry) {
		te_hal_obj_dump_tree(next); /* bad bad recursion */
	}
}

