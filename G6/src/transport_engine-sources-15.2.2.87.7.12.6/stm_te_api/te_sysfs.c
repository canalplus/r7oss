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

Source file name : te_sysfs.c

Defines stm_te sysfs related functions
******************************************************************************/

#include "te_object.h"
#include "te_demux.h"
#include "te_input_filter.h"
#include "te_output_filter.h"
#include "te_hal_obj.h"
#include "te_sysfs.h"
#include "te_global.h"

static int te_sysfs_store_demux_reset(stm_object_h object, char *buf,
		size_t size, const char *user_buf, size_t user_size);

static int te_sysfs_show_TPCycleCount(stm_object_h object, char *buf,
		size_t size, char *user_buf, size_t user_size);

static int te_sysfs_print_hal_hdl(stm_object_h object, char *buf,
		size_t size, char *user_buf, size_t user_size);

static int te_sysfs_print_hal_hdl_list(stm_object_h object, char *buf,
		size_t size, char *user_buf, size_t user_size);

/* Declare demux sysfs statistics attributes */
TE_SYSFS_DEMUX_STATS_ATTR(packet_count);
TE_SYSFS_DEMUX_STATS_ATTR(cc_errors);
TE_SYSFS_DEMUX_STATS_ATTR(tei_count);
TE_SYSFS_DEMUX_STATS_ATTR(input_packets);
TE_SYSFS_DEMUX_STATS_ATTR(crc_errors);
TE_SYSFS_DEMUX_STATS_ATTR(buffer_overflows);
TE_SYSFS_DEMUX_STATS_ATTR(output_packets);
TE_SYSFS_DEMUX_STATS_ATTR(utilization);

TE_SYSFS_DEMUX_TIME_STATS_ATTR(inject_input_latency) ;
TE_SYSFS_DEMUX_TIME_STATS_ATTR(inject_time_tp) ;

static struct te_sysfs_entry te_demux_stat_entry[] = {

	/* generic stats entries */
	TE_SYSFS_ENTRY(demux, packet_count),
	TE_SYSFS_ENTRY(demux, cc_errors),
	TE_SYSFS_ENTRY(demux, tei_count),
	TE_SYSFS_ENTRY(demux, input_packets),
	TE_SYSFS_ENTRY(demux, crc_errors),
	TE_SYSFS_ENTRY(demux, buffer_overflows),
	TE_SYSFS_ENTRY(demux, output_packets),
	TE_SYSFS_ENTRY(demux, utilization),

	/* reset entry */
	{
		.name = "reset",
		.tag = "te_reset",
		.func.print_handler = NULL,
		.func.store_handler = &te_sysfs_store_demux_reset
	},
	/* Cycle count entry */
	{
		.name = "TPCycleCount",
		.tag = "te_TPCycleCount",
		.func.print_handler = te_sysfs_show_TPCycleCount,
		.func.store_handler = NULL,
	},

	TE_SYSFS_TIME_STAT_ENTRY(demux, inject_input_latency),
	TE_SYSFS_TIME_STAT_ENTRY(demux, inject_time_tp),

};

/* Declare input filter sysfs statistics attributes */
TE_SYSFS_INPUT_FILTER_STATS_ATTR(packet_count);

static struct te_sysfs_entry te_in_filter_stat_entry[] = {
	TE_SYSFS_ENTRY(in_filter, packet_count),
};

/* Declare output filter sysfs statistics attributes */
TE_SYSFS_OUTPUT_FILTER_STATS_ATTR(packet_count);
TE_SYSFS_OUTPUT_FILTER_STATS_ATTR(crc_errors);
TE_SYSFS_OUTPUT_FILTER_STATS_ATTR(buffer_overflows);
TE_SYSFS_OUTPUT_FILTER_STATS_ATTR(bytes_in_buffer);
TE_SYSFS_SIMPLE_ATTR(out_filter, buffer_size, uint32_t,
		STM_TE_OUTPUT_FILTER_CONTROL_BUFFER_SIZE);

static struct te_sysfs_entry te_out_filter_stat_entry[] = {
	TE_SYSFS_ENTRY(out_filter, packet_count),
	TE_SYSFS_ENTRY(out_filter, crc_errors),
	TE_SYSFS_ENTRY(out_filter, buffer_overflows),
	TE_SYSFS_ENTRY(out_filter, bytes_in_buffer),
	TE_SYSFS_ENTRY(out_filter, buffer_size),
};

/* Miscellaneous data types used by TE objects */
static struct te_sysfs_entry te_misc_entry[] = {
	/* HAL object entry */
	{
		.name = TE_SYSFS_NAME_HAL_HDL,
		.tag = TE_SYSFS_TAG_HAL_HDL,
		.func.print_handler = &te_sysfs_print_hal_hdl,
	},
	/* HAL object list entry */
	{
		.name = TE_SYSFS_NAME_HAL_HDL,
		.tag = TE_SYSFS_TAG_HAL_HDL_LIST,
		.func.print_handler = &te_sysfs_print_hal_hdl_list,
	}
};

int te_sysfs_data_type_add(void)
{
	int err;
	int i;
	struct te_sysfs_entry *entry;
	uint32_t entry_size;

	entry = te_demux_stat_entry;
	entry_size = ARRAY_SIZE(te_demux_stat_entry);

	for (i = 0; i < entry_size; i++) {
		err = stm_registry_add_data_type(entry[i].tag, &entry[i].func);
		if (err) {
			pr_err("Failed to add data type %s\n", entry[i].tag);
			goto error;
		}
	}

	entry = te_in_filter_stat_entry;
	entry_size = ARRAY_SIZE(te_in_filter_stat_entry);
	for (i = 0; i < entry_size; i++) {
		err = stm_registry_add_data_type(entry[i].tag, &entry[i].func);
		if (err) {
			pr_err("Failed to add data type %s\n", entry[i].tag);
			goto error;
		}
	}

	entry = te_out_filter_stat_entry;
	entry_size = ARRAY_SIZE(te_out_filter_stat_entry);

	for (i = 0; i < entry_size; i++) {
		err = stm_registry_add_data_type(entry[i].tag, &entry[i].func);
		if (err) {
			pr_err("Failed to add data type %s\n", entry[i].tag);
			goto error;
		}
	}

	for (i = 0; i < ARRAY_SIZE(te_misc_entry); i++) {
		err = stm_registry_add_data_type(te_misc_entry[i].tag,
				&te_misc_entry[i].func);
		if (err) {
			pr_err("Failed to add data type %s\n",
					te_misc_entry[i].tag);
			goto error;
		}
	}

error:
	return err;
}

int te_sysfs_data_type_remove(void)
{
	int err;
	int i;
	struct te_sysfs_entry *entry;
	uint32_t entry_size;

	entry = te_demux_stat_entry;
	entry_size = ARRAY_SIZE(te_demux_stat_entry);

	for (i = 0; i < entry_size; i++) {
		err = stm_registry_remove_data_type(entry[i].tag);
		if (err) {
			pr_err("Failed to remove data type %s\n",
				entry[i].name);
			goto error;
		}
	}

	entry = te_in_filter_stat_entry;
	entry_size = ARRAY_SIZE(te_in_filter_stat_entry);
	for (i = 0; i < entry_size; i++) {
		err = stm_registry_remove_data_type(entry[i].tag);
		if (err) {
			pr_err("Failed to remove data type %s\n",
				entry[i].name);
			goto error;
		}
	}

	entry = te_out_filter_stat_entry;
	entry_size = ARRAY_SIZE(te_out_filter_stat_entry);

	for (i = 0; i < entry_size; i++) {
		err = stm_registry_remove_data_type(entry[i].tag);
		if (err) {
			pr_err("Failed to remove data type %s\n",
				entry[i].name);
			goto error;
		}
	}

	for (i = 0; i < ARRAY_SIZE(te_misc_entry); i++) {
		err = stm_registry_remove_data_type(te_misc_entry[i].tag);
		if (err) {
			pr_err("Failed to add data type %s\n",
					te_misc_entry[i].tag);
			goto error;
		}
	}

error:
	return err;
}

/*!
 * \brief Add new sysfs entries
 *
 * \param obj	   Pointer to TE object
 * \param entry	   Array of entries to be added
 * \param entry_size Size of array
 *
 * \retval 0       Success
 * \retval -EINVAL Bad handle
 * \retval -EIO    Hardware error
 */
int te_sysfs_entries_add(struct te_obj *obj)
{
	int err = 0;
	int i;
	struct te_sysfs_entry *entry;
	uint32_t nb_entries;

	switch (obj->type) {
	case TE_OBJ_DEMUX:
		entry = te_demux_stat_entry;
		nb_entries = ARRAY_SIZE(te_demux_stat_entry);
		break;
	case TE_OBJ_PES_FILTER:
	case TE_OBJ_SECTION_FILTER:
	case TE_OBJ_TS_FILTER:
	case TE_OBJ_TS_INDEX_FILTER:
	case TE_OBJ_PCR_FILTER:
		entry = te_out_filter_stat_entry;
		nb_entries = ARRAY_SIZE(te_out_filter_stat_entry);
		break;
	case TE_OBJ_PID_FILTER:
	case TE_OBJ_PID_INS_FILTER:
	case TE_OBJ_PID_REP_FILTER:
		entry = te_in_filter_stat_entry;
		nb_entries = ARRAY_SIZE(te_in_filter_stat_entry);
		break;
	default:
		return 0;
	}

	for (i = 0; i < nb_entries && !err; i++) {
		err = stm_registry_add_attribute(obj, entry[i].name,
						entry[i].tag,
						" ", 1);
		if (err)
			pr_err("Failed to add attribute %s\n",
				entry[i].name);
	}

	return err;
}

/*!
 * \brief Remove sysfs entries
 *
 * \param obj	   Pointer to TE object
 * \param entry	   Array of entries to removed
 * \param entry_size Size of array
 *
 * \retval 0       Success
 * \retval -EINVAL Bad handle
 * \retval -EIO    Hardware error
 */
int te_sysfs_entries_remove(struct te_obj *obj)
{
	int err = 0;
	int i;
	struct te_sysfs_entry *entry;
	uint32_t nb_entries;

	switch (obj->type) {
	case TE_OBJ_DEMUX:
		entry = te_demux_stat_entry;
		nb_entries = ARRAY_SIZE(te_demux_stat_entry);
		break;
	case TE_OBJ_PES_FILTER:
	case TE_OBJ_SECTION_FILTER:
	case TE_OBJ_TS_FILTER:
	case TE_OBJ_TS_INDEX_FILTER:
	case TE_OBJ_PCR_FILTER:
		entry = te_out_filter_stat_entry;
		nb_entries = ARRAY_SIZE(te_out_filter_stat_entry);
		break;
	case TE_OBJ_PID_FILTER:
	case TE_OBJ_PID_INS_FILTER:
	case TE_OBJ_PID_REP_FILTER:
		entry = te_in_filter_stat_entry;
		nb_entries = ARRAY_SIZE(te_in_filter_stat_entry);
		break;
	default:
		return 0;
	}

	for (i = 0; i < nb_entries && !err; i++) {
		err = stm_registry_remove_attribute(obj, entry[i].name);
		if (err)
			pr_err("Failed to remove attribute %s\n",
				entry[i].name);
	}

	return err;
}

/*!
 * \brief Sysfs store callback interface for demux stats reset
 *
 * \param object	object handle
 * \param buf		buffer
 * \param size		buffer size
 * \param user_buf	user buffer
 * \param user_size	user buffer size
 *
 * \retval user_size	Success
 * \retval -EINVAL	Bad handle
 * \retval -EIO	Hardware error
 * \retval -EINTR       Call interrupted
 */
static int te_sysfs_store_demux_reset(stm_object_h object, char *buf,
			size_t size, const char *user_buf, size_t user_size)
{
	struct te_obj *obj;
	int err;

	if (mutex_lock_interruptible(&te_global.lock) != 0)
		return -EINTR;

	err = te_obj_from_hdl(object, &obj);
	if (err) {
		pr_err("Invalid object\n");
		goto error;
	}

	err = te_demux_reset_stat(obj);
	if (err)
		goto error;

	mutex_unlock(&te_global.lock);

	return user_size;
error:
	mutex_unlock(&te_global.lock);
	return err;
}

/*!
 * \brief Sysfs show callback interface for demux cycle count
 *
 * \param object	object handle
 * \param buf		buffer
 * \param size		buffer size
 * \param user_buf	user buffer
 * \param user_size	user buffer size
 *
 * \retval user_size	Success
 * \retval -EINVAL	Bad handle
 * \retval -EIO	Hardware error
 */
static int te_sysfs_show_TPCycleCount(stm_object_h obj, char *buf,
				size_t size, char *user_buf, size_t user_size)
{
	unsigned int count = 0;
	ST_ErrorCode_t hal_err;
	struct te_demux *demux = te_demux_from_obj(obj);
	struct te_hal_obj *pDev;

	if (demux == NULL)
		return snprintf(user_buf, user_size, "invalid demux\n");

	if (demux->hal_vdevice == NULL)
		return snprintf(user_buf, user_size, "invalid vDevice\n");

	pDev = demux->hal_vdevice->parent;
	if (pDev == NULL)
		return snprintf(user_buf, user_size, "invalid pDevice\n");


	hal_err = stptiHAL_call(pDevice.HAL_pDeviceGetCycleCount, pDev->hdl,
				&count);

	if (hal_err != ST_NO_ERROR)
		return snprintf(user_buf, user_size, "Hal Error %d\n", hal_err);

	return snprintf(user_buf, user_size, "%u\n", count);
}

/*!
 * \brief Sysfs print callback to show a HAL hdl
 *
 * \param object	Object handle
 * \param buf		buffer
 * \param size		buffer size
 * \param user_buf	user buffer
 * \param user_size	user buffer size
 *
 * \return size of data written to user_buf
 */
static int te_sysfs_print_hal_hdl(stm_object_h object, char *buf,
		size_t size, char *user_buf, size_t user_size)
{
	struct te_hal_obj **hal_hdl_pp;
	struct te_hal_obj *hal_hdl;

	/* buf contains a pointer to a pointer to a struct te_hal_obj */
	if (!buf || size != sizeof(hal_hdl))
		return -EINVAL;
	hal_hdl_pp = (struct te_hal_obj **)buf;

	hal_hdl = *hal_hdl_pp;

	/* HAL handle not initialised */
	if (!hal_hdl)
		return snprintf(user_buf, user_size, "none\n");

	return snprintf(user_buf, user_size, "0x%.8x\n", hal_hdl->hdl.word);
}

/*!
 * \brief Sysfs print callback to show a list of HAL hdls
 *
 * \param object	Object handle
 * \param buf		buffer
 * \param size		buffer size
 * \param user_buf	user buffer
 * \param user_size	user buffer size
 *
 * \return size of data written to user_buf
 */
static int te_sysfs_print_hal_hdl_list(stm_object_h object, char *buf,
		size_t size, char *user_buf, size_t user_size)
{
	struct list_head *lh;
	struct te_hal_obj *hal_hdl;
	int ret = 0;
	int hdl_count = 0;

	/* buf contains a pointer to a pointer to a struct list_head */
	if (!buf || size != sizeof(struct list_head))
		return -EINVAL;
	lh = (struct list_head *)buf;

	/* Print list of Hal handles */
	ret += snprintf(user_buf, user_size, "[");

	list_for_each_entry(hal_hdl, lh, entry) {
		if (hdl_count > 0)
			ret += snprintf(&user_buf[ret], user_size - ret,
					", ");
		ret += snprintf(&user_buf[ret], user_size - ret, "0x%.8x",
				hal_hdl->hdl.word);
		hdl_count++;
	}

	ret += snprintf(&user_buf[ret], user_size - ret, "]\n");

	return ret;
}

