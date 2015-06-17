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

Source file name : te_sysfs.h

Declares stm_te sysfs/statistic functions
******************************************************************************/
#ifndef __TE_SYSFS_H
#define __TE_SYSFS_H

struct te_sysfs_entry {
	const char *name;
	const char *tag;
	stm_registry_type_def_t func;
};

/* Macros to generate stm_registry "show" functions to display arbitrary
 * statistics by reading an object's control */
#define TE_SYSFS_DEMUX_STATS_ATTR(attr) \
	TE_SYSFS_STATS_ATTR(demux, stm_te_demux_stats_t, \
			STM_TE_DEMUX_CNTRL_STATUS, attr)

#define TE_SYSFS_INPUT_FILTER_STATS_ATTR(attr) \
	TE_SYSFS_STATS_ATTR(in_filter, stm_te_input_filter_stats_t, \
			STM_TE_INPUT_FILTER_CONTROL_STATUS, attr)

#define TE_SYSFS_OUTPUT_FILTER_STATS_ATTR(attr) \
	TE_SYSFS_STATS_ATTR(out_filter, stm_te_output_filter_stats_t, \
			STM_TE_OUTPUT_FILTER_CONTROL_STATUS, attr)

#define TE_SYSFS_SIMPLE_ATTR(type, name, attr_type, control_name) \
static int te_sysfs_show_##type##_##name(stm_object_h object, char *buf, \
				size_t size, char *user_buf, size_t user_size) \
{ \
	struct te_obj *obj; \
	int err; \
	attr_type val; \
	mutex_lock(&te_global.lock); \
	err = te_obj_from_hdl(object, &obj); \
	if (err) { \
		pr_err("Invalid object\n"); \
		goto error; \
	} \
	err = obj->ops->get_control(obj, control_name, \
		&val, sizeof(val)); \
	if (err) \
		goto error; \
	\
	mutex_unlock(&te_global.lock); \
	return snprintf(user_buf, user_size, "%u\n", val); \
\
error: \
	mutex_unlock(&te_global.lock);\
	return err; \
}

#define TE_SYSFS_STATS_ATTR(type, stats_struct, stats_control, attr) \
static int te_sysfs_show_##type##_##attr(stm_object_h object, char *buf, \
				size_t size, char *user_buf, size_t user_size) \
{ \
	struct te_obj *obj; \
	int err; \
	stats_struct stat; \
	if (mutex_lock_interruptible(&te_global.lock) != 0) \
		return -EINTR; \
	err = te_obj_from_hdl(object, &obj); \
	if (err) { \
		pr_err("Invalid object\n"); \
		goto error; \
	} \
	err = obj->ops->get_control(obj, stats_control, \
		&stat, sizeof(stats_struct)); \
	if (err) \
		goto error; \
	\
	mutex_unlock(&te_global.lock); \
	return snprintf(user_buf, user_size, "%u\n", stat.attr); \
\
error: \
	mutex_unlock(&te_global.lock);\
	return err; \
}

#define TE_SYSFS_STRINGIFY(x) (#x)

#define TE_SYSFS_ENTRY(type, attr) { \
	.name = #attr, \
	.tag = TE_SYSFS_STRINGIFY(te_##type##_##attr), \
	.func.print_handler = &te_sysfs_show_##type##_##attr, \
	.func.store_handler = NULL \
}

#define TE_SYSFS_TIME_STAT_ENTRY(demux, attr) { \
	.name = #attr, \
	.tag = TE_SYSFS_STRINGIFY(te_time_##attr), \
	.func.print_handler = &te_sysfs_show_time_##attr, \
	.func.store_handler = NULL \
}


#define TE_SYSFS_DEMUX_TIME_STATS_ATTR(attr) \
static int te_sysfs_show_time_##attr(stm_object_h object, char *buf, \
				size_t size, char *user_buf, size_t user_size) \
{ \
	struct te_obj *dmx_obj; \
	struct te_demux *dmx; \
	int err; \
	unsigned long time_stat;	\
	if (mutex_lock_interruptible(&te_global.lock) != 0) \
		return -EINTR; \
	 err = te_demux_from_hdl(object, &dmx_obj); \
	if (err) { \
		pr_err("Bad demux handle\n"); \
		goto error; \
	} \
	dmx = te_demux_from_obj(dmx_obj); \
	time_stat = dmx->push_time_stats.attr ; \
	mutex_unlock(&te_global.lock); \
	return snprintf(user_buf, user_size, "%lu\n", time_stat); \
error: \
	mutex_unlock(&te_global.lock);\
	return err; \
}

#define TE_SYSFS_NAME_HAL_HDL "hal_"
#define TE_SYSFS_TAG_HAL_HDL "te_hal_hdl"
#define TE_SYSFS_TAG_HAL_HDL_LIST "te_hal_hdl_list"

int te_sysfs_data_type_add(void);
int te_sysfs_data_type_remove(void);
int te_sysfs_entries_add(struct te_obj *obj);
int te_sysfs_entries_remove(struct te_obj *obj);

#endif
