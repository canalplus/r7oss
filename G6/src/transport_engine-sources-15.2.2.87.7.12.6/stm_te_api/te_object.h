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

Source file name : te_object.h

Declares base TE object type and operators
******************************************************************************/

/*!
 * \file te_object.h
 *
 * Defines a set of functions for reading and writing attributes of stm_te
 * objects.
 *
 * Members of stm_te objects should not be accessed directly, except by the
 * functions declared in this header. This allows the implementation of the
 * stm_te internal object model to be changed, with only limited impact on the
 * stm_te codebase
 */
#ifndef __TE_OBJECT_H
#define __TE_OBJECT_H

#include <stm_common.h>
#include <stm_te.h>
#include <pti_hal_api.h>
#include <linux/errno.h>
#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/string.h>

/*!
 * \brief TE object types
 *
 * Note that these types deliberately map onto and extend the stm_te_filter_e
 * enum in the stm_te API to avoid the need for translation.
 */
enum te_obj_type_e {
	/* Values map onto stm_te API constants */
	TE_OBJ_PID_FILTER = STM_TE_PID_FILTER,
	TE_OBJ_TS_FILTER = STM_TE_TS_FILTER,
	TE_OBJ_PES_FILTER = STM_TE_PES_FILTER,
	TE_OBJ_SECTION_FILTER = STM_TE_SECTION_FILTER,
	TE_OBJ_PCR_FILTER = STM_TE_PCR_FILTER,
	TE_OBJ_TS_INDEX_FILTER = STM_TE_TS_INDEX_FILTER,
	TE_OBJ_TSG_FILTER = STM_TE_TSG_FILTER,
	TE_OBJ_TSG_SEC_FILTER = STM_TE_TSG_SEC_FILTER,
	TE_OBJ_TSG_INDEX_FILTER = STM_TE_TSG_INDEX_FILTER,

	/* Internal types - extend stm_te API constants */
	TE_OBJ_DEMUX = STM_TE_LAST_FILTER,
	TE_OBJ_TSMUX,
	TE_OBJ_PID_INS_FILTER,
	TE_OBJ_PID_REP_FILTER,
};

/*!
 * \brief TE object states
 */
enum te_obj_state_e {
	/*! An object in stopped state indicates HAL resources for that object
	 * have not been assigned */
	TE_OBJ_STOPPED = 0,
	/* Object in pause state */
	TE_OBJ_PAUSED,
	/*! An object in started state has associated HAL resources */
	TE_OBJ_STARTED,
};

/*!
 * \brief TE object classes
 */
enum te_obj_class_e {
	TE_OBJ_CLASS_DEMUX,
	TE_OBJ_CLASS_TSMUX,
	TE_OBJ_CLASS_INPUT_FILTER,
	TE_OBJ_CLASS_OUTPUT_FILTER,
	TE_OBJ_CLASS_TSG_FILTER,
	TE_OBJ_CLASS_TSG_INDEX_FILTER,
};

/*!
 * \brief Represents a single object class
 */
struct te_obj_class {
	enum te_obj_class_e class;
	uint32_t max;
	atomic_t count;
};

/*!
 * \brief Base te object type
 *
 * This struct is embedded in all TE objects, providing common attributes and
 * methods
 */
struct te_obj {
	enum te_obj_type_e type;
	struct te_obj_class *class_obj;
	enum te_obj_state_e state;
	struct te_obj *parent;
	char name[STM_REGISTRY_MAX_TAG_SIZE];
	struct list_head lh;
	struct mutex lock;
	struct te_obj_ops *ops;
};

/*!
 * \brief vtable of operations performed on all struct te_obj operations
 *
 * No function pointer in an instance of this vtable may be NULL
 */
struct te_obj_ops {
	int (*delete)(struct te_obj *obj);
	int (*set_control)(struct te_obj *obj, uint32_t ctrl, const void *buf,
			uint32_t size);
	int (*get_control)(struct te_obj *obj, uint32_t ctrl, void *buf,
			uint32_t size);
	int (*attach)(struct te_obj *obj, stm_object_h target);
	int (*detach)(struct te_obj *obj, stm_object_h target);
};

/* TE object operators */
int te_obj_init(struct te_obj *obj, enum te_obj_type_e type, char *name,
		struct te_obj *parent, struct te_obj_class *type_obj);
int te_obj_deinit(struct te_obj *obj);

/* TE HAL Null handle */
#define HAL_HDL_EQUAL(x, y) (x.word == y.word)
#define HAL_HDL_IS_NULL(x) (x.word == stptiOBJMAN_NullHandle.word)
#define HAL_HDL(x) (FullHandle_t)(x)


/* Conversion functions with object class checking */
int te_in_filter_from_hdl(stm_object_h hdl, struct te_obj **obj);
int te_tsg_filter_from_hdl(stm_object_h hdl, struct te_obj **obj);
int te_demux_from_hdl(stm_object_h hdl, struct te_obj **obj);
int te_tsmux_from_hdl(stm_object_h hdl, struct te_obj **obj);
int te_tsg_index_from_hdl(stm_object_h hdl, struct te_obj **obj);

/* Conversion functions with object type checking */
int te_valid_hdl(stm_object_h hdl);
int te_obj_from_hdl(stm_object_h hdl, struct te_obj **obj);
int te_hdl_from_obj(struct te_obj *obj, stm_object_h *hdl);
stm_te_object_h te_obj_to_hdl(struct te_obj *obj);

/* Set/get control helpers */
static inline int __set_control(void *ctrl, size_t ctrl_size, const void *buf,
		size_t buf_size)
{
	if (!buf)
		return -EINVAL;

	if (buf_size == ctrl_size)
		memcpy(ctrl, buf, ctrl_size);
	else
		return -EINVAL;
	return 0;
}
#define SET_CONTROL(ctrl, buf, size) \
	__set_control(&(ctrl), sizeof(ctrl), buf, size)

static inline int __get_control(void *ctrl, size_t ctrl_size, void *buf,
		size_t buf_size)
{
	if (!buf)
		return -EINVAL;

	if (buf_size >= ctrl_size)
		memcpy(buf, ctrl, ctrl_size);
	else
		return -EINVAL;
	return 0;
}
#define GET_CONTROL(ctrl, buf, size) \
	__get_control(&(ctrl), sizeof(ctrl), buf, size)

#endif
