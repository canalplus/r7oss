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

Source file name : te_hal_obj.h

Declares TE HAL objects
******************************************************************************/
#ifndef __TE_HAL_OBJ_H
#define __TE_HAL_OBJ_H

#include <linux/kref.h>
#include <linux/list.h>
#include <pti_hal_api.h>

struct te_hal_obj {
	FullHandle_t hdl;

	struct kref refcount;
	void (*release)(struct kref *kref);

	/* entry is a user available list_head for storing objects in linked lists */
	struct list_head entry;

	/* pointer to the parent, so we can inc/dec the parents usage count */
	struct te_hal_obj *parent;

	/* We store a list of children for tracking object leaks during destruction */

	/* list of children */
	struct list_head children;

	/* entry for insertion into the parents list of children */
	struct list_head c_entry;
};

void te_hal_obj_print(struct te_hal_obj *obj, const char *pfx);

void te_hal_obj_dump_tree(struct te_hal_obj *obj);

/*!
 *\brief increment use count on object
 */
static inline void __te_hal_obj_inc(struct te_hal_obj *obj, const char *pfx)
{
	te_hal_obj_print(obj, pfx);
	kref_get(&obj->refcount);
}

/*!
 *\brief decrement use count on object, frees object if now unused.
 */
static inline int __te_hal_obj_dec(struct te_hal_obj *obj, const char *pfx)
{
	if (!obj)
		return 1;
	te_hal_obj_print(obj, pfx);
	return kref_put(&obj->refcount, obj->release);
}

int __te_hal_obj_alloc(struct te_hal_obj **obj, struct te_hal_obj *parent, ObjectType_t type, void *params, const char *pfx);

#define STRINGIZE(x) STRINGIZE2(x)
#define STRINGIZE2(x) #x
#define LINE_STRING STRINGIZE(__LINE__)

#define te_hal_obj_alloc(o, p, t, m) __te_hal_obj_alloc(o, p, t, m, __FILE__ ":" LINE_STRING " allocated")
#define te_hal_obj_inc(o)   __te_hal_obj_inc(o, __FILE__ ":" LINE_STRING " incrementing")
#define te_hal_obj_dec(o)   __te_hal_obj_dec(o, __FILE__ ":" LINE_STRING " decrementing")

/*!
 *\brief convert TE HAL error into linux errno
 */
static inline int te_hal_err_to_errno(ST_ErrorCode_t hal_err)
{
	switch (hal_err) {
	case ST_NO_ERROR:
		return 0;
	case ST_ERROR_BAD_PARAMETER:
		return -EINVAL;
	case ST_ERROR_NO_MEMORY:
		return -ENOMEM;
	case ST_ERROR_ALREADY_INITIALIZED:
		return -EAGAIN;
	default:
		return -EIO;
	}
}

#endif
