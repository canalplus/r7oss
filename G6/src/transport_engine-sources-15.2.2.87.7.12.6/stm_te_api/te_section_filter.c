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

Source file name : te_section_filter.c

Defines base section filter specific operations
******************************************************************************/

#include <stm_te_dbg.h>
#include <pti_hal_api.h>
#include "te_object.h"
#include "te_interface.h"
#include "te_output_filter.h"
#include "te_section_filter.h"
#include "te_input_filter.h"
#include "te_pid_filter.h"
#include "te_demux.h"
#include "stm_memsink.h"
#include "te_global.h"

/* Section filter constants */
#define MAX_FILTER_BYTES (16)

struct te_section_filter {
	struct te_out_filter output;

	uint8_t filter_bytes[MAX_FILTER_BYTES];
	uint8_t filter_masks[MAX_FILTER_BYTES];
	uint8_t pos_neg_pattern[MAX_FILTER_BYTES];
	uint8_t filter_size;
	uint8_t filter_hal_size;
	uint8_t filter_type;
	uint8_t *proprietary_data;
	uint8_t proprietary_size;

	bool section_filter_repeated;
	bool force_crc_check;
	bool discard_on_crc_error;
	bool pos_neg_filtering;
	bool version_not_match_filtering;
	bool proprietary_1_filtering;
	bool ecm_filter;

	uint8_t read_buffer[TE_MAX_SECTION_SIZE];

	/* statistics */
	uint32_t crc_error_count;
	uint32_t packet_count;
	uint32_t buffer_overflow_count;

	bool work_owner;
	struct te_section_filter *owner;
};

/* Local function prototypes */
static int te_section_filter_hal_update(struct te_section_filter *section);
static void translate_section_bytes(const uint8_t *pattern, uint32_t length,
		uint8_t *out_pattern, uint8_t *out_length);
static int te_section_filter_get_stat(struct te_obj *obj,
		stm_te_output_filter_stats_t *stat);

static struct te_section_filter *te_section_filter_from_obj(
		struct te_obj *filter)
{
	if (filter->type == TE_OBJ_SECTION_FILTER)
		return container_of(filter, struct te_section_filter,
				output.obj);
	else
		return NULL;
}

static void te_section_work(struct te_obj *obj, struct te_hal_obj *buffer);

/*!
 * \brief Sets the filter bytes for a te section filter object
 *
 * For non-proprietary filters the pattern is translated and stored in HAL
 * format
 *
 * \param filter te filter object to update
 * \param bytes  filter pattern to set
 * \param size   size of filter pattern to set
 */
static inline void set_filter_bytes(struct te_section_filter *section,
		const uint8_t *bytes, uint32_t size)
{
	if (!section->proprietary_1_filtering) {
		section->filter_size = size;
		translate_section_bytes(bytes, size, section->filter_bytes,
				&section->filter_hal_size);
	} else {
		kfree(section->proprietary_data);
		section->proprietary_data = kmalloc(size, GFP_KERNEL);
		if (!section->proprietary_data) {
			pr_err("Failed to allocate proprietary data buffer\n");
			return;
		}
		memcpy(section->proprietary_data, bytes, size);
		section->proprietary_size = size;
	}
}

/*!
 * \brief Sets the filter mask for a te section filter
 *
 * Mask bytes are translated and store in HAL format
 *
 * \param section te section filter to update
 * \param bytes   Filter mask pattern to set
 * \param size    Filter mask size
 */
static inline void set_filter_mask(struct te_section_filter *section,
		const uint8_t *bytes, uint32_t size)
{
	uint8_t mask_size = 0;
	if (!section->proprietary_1_filtering) {
		translate_section_bytes(bytes, size, section->filter_masks,
			&mask_size);
	}
}

/*!
 * \brief Returns a pointer to the filter mask for a te section filter object
 *
 * \param filter te filter object to interrogate
 */
static inline uint8_t *get_filter_mask(struct te_section_filter *section)
{
	return section->filter_masks;
}

/*!
 * \brief Returns a pointer to the filter pn pattern for a te section filter
 * object
 *
 * \param filter te filter object to interrogate
 *
 * \retval pointer to positive negative pattern or NULL if no pattern set
 */
static inline uint8_t *get_pn_pattern(struct te_section_filter *section)
{
	if (section->pos_neg_filtering)
		return section->pos_neg_pattern;
	return NULL;
}

/*!
 * \brief Sets the pn pattern for a te section filter object
 *
 * Filter bytes are translated and stored in HAL format
 *
 * \param filter te filter object to update
 * \param bytes  positive/negative pattern to set
 * \param size   size of pn pattern to set
 */
static inline void set_pn_pattern(struct te_section_filter *section,
					const uint8_t *bytes, uint32_t size)
{
	uint8_t pn_size = 0;
	translate_section_bytes(bytes, size, section->pos_neg_pattern,
			&pn_size);
}

/*!
 * \brief Returns a pointer to the filter pattern for a te section filter object
 *
 * \param filter te filter object to interrogate
 */
static inline uint8_t *get_filter_bytes(struct te_section_filter *section)
{
	if (!section->proprietary_1_filtering)
		return section->filter_bytes;
	else
		return section->proprietary_data;
}

int te_section_filter_get_control(struct te_obj *filter, uint32_t control,
					void *buf, uint32_t size)
{
	int err = 0;
	struct te_section_filter *section = te_section_filter_from_obj(filter);
	unsigned int val;

	switch (control) {
	case STM_TE_SECTION_FILTER_CONTROL_REPEATED:
		val = section->section_filter_repeated;
		err = GET_CONTROL(val, buf, size);
		break;
	case STM_TE_SECTION_FILTER_CONTROL_FORCE_CRC_CHECK:
		val = section->force_crc_check;
		err = GET_CONTROL(val, buf, size);
		break;
	case STM_TE_SECTION_FILTER_CONTROL_DISCARD_ON_CRC_ERROR:
		val = section->discard_on_crc_error;
		err = GET_CONTROL(val, buf, size);
		break;
	case STM_TE_SECTION_FILTER_CONTROL_VERSION_NOT_MATCH:
		val = section->version_not_match_filtering;
		err = GET_CONTROL(val, buf, size);
		break;
	case STM_TE_SECTION_FILTER_CONTROL_PROPRIETARY_1:
		val = section->proprietary_1_filtering;
		err = GET_CONTROL(val, buf, size);
		break;
	case STM_TE_SECTION_FILTER_CONTROL_POS_NEG_ENABLED:
		val = section->pos_neg_filtering;
		err = GET_CONTROL(val, buf, size);
		break;
	case STM_TE_SECTION_FILTER_CONTROL_ECM:
		val = section->ecm_filter;
		err = GET_CONTROL(val, buf, size);
		break;
	case __deprecated_STM_TE_FILTER_CONTROL_STATUS:
	case STM_TE_OUTPUT_FILTER_CONTROL_STATUS:
		err = te_section_filter_get_stat(filter,
					(stm_te_output_filter_stats_t *) buf);
		break;
	case STM_TE_OUTPUT_FILTER_CONTROL_BUFFER_SIZE:
		err = GET_CONTROL(section->output.max_queued_data, buf, size);
		break;
	case STM_TE_OUTPUT_FILTER_CONTROL_PAUSE:
                return -EINVAL;
	default:
		err = te_out_filter_get_control(filter, control, buf, size);
	}
	return err;
}

int te_section_filter_set_control(struct te_obj *filter, uint32_t control,
					const void *buf, uint32_t size)
{
	int err = 0;
	int do_update = 1;
	struct te_section_filter *section = te_section_filter_from_obj(filter);
	unsigned int val;

	switch (control) {
	case STM_TE_SECTION_FILTER_CONTROL_REPEATED:
		err = SET_CONTROL(val, buf, size);
		if (!err)
			section->section_filter_repeated = (val != 0);
		break;
	case STM_TE_SECTION_FILTER_CONTROL_FORCE_CRC_CHECK:
		err = SET_CONTROL(val, buf, size);
		if (!err)
			section->force_crc_check = (val != 0);
		break;
	case STM_TE_SECTION_FILTER_CONTROL_DISCARD_ON_CRC_ERROR:
		err = SET_CONTROL(val, buf, size);
		if (!err)
			section->discard_on_crc_error = (val != 0);
		break;
	case STM_TE_SECTION_FILTER_CONTROL_VERSION_NOT_MATCH:
		err = SET_CONTROL(val, buf, size);
		if (!err)
			section->version_not_match_filtering = (val != 0);
		break;
	case STM_TE_SECTION_FILTER_CONTROL_PROPRIETARY_1:
		err = SET_CONTROL(val, buf, size);
		if (!err) {
			section->proprietary_1_filtering = (val != 0);
			if (!section->filter_size)
				do_update = 0;
		}
		break;
	case STM_TE_SECTION_FILTER_CONTROL_POS_NEG_ENABLED:
		err = SET_CONTROL(val, buf, size);
		if (!err)
			section->pos_neg_filtering = (val != 0);
		break;
	case STM_TE_SECTION_FILTER_CONTROL_ECM:
		err = SET_CONTROL(val, buf, size);
		if (!err)
			section->ecm_filter = (val != 0);
		break;
	case STM_TE_SECTION_FILTER_CONTROL_PATTERN:
		set_filter_bytes(section, buf, size);
		break;
	case STM_TE_SECTION_FILTER_CONTROL_MASK:
		set_filter_mask(section, buf, section->filter_size);
		break;
	case STM_TE_SECTION_FILTER_CONTROL_POS_NEG_PATTERN:
		set_pn_pattern(section, buf, section->filter_size);
		break;
	case STM_TE_OUTPUT_FILTER_CONTROL_BUFFER_SIZE:
		/* Overload the set buffer call with local implementation
		 * Since sections share the same hal buffer attached to same
		 * PID filter. Hence we will modify the local buffer where the
		 * sectioncs are copied from shred hal buffer.
		 * */
		if (buf && *(int *)buf < TE_MAX_SECTION_SIZE)
			pr_warn("%d size is low, section(%p) buffer might overflow\n",
					*(int *)buf,
					filter->name);

		if (filter->state == TE_OBJ_STARTED) {
			pr_err("Unable to change buffer size, output filter is already attached\n");
			err = -EINVAL;
		} else if (buf && (*((int *)buf) < DVB_PAYLOAD_SIZE)) {
			pr_err("Buffer size should be atleast equal to DVB payload size i.e. 184 bytes\n");
			err = -EINVAL;
		} else
			err = SET_CONTROL(section->output.max_queued_data,
					buf, size);
		break;
	case STM_TE_OUTPUT_FILTER_CONTROL_PAUSE:
                return -EINVAL;
	default:
		do_update = 0;
		err = te_out_filter_set_control(filter, control, buf, size);
	}

	if (!err && do_update)
		err = te_section_filter_hal_update(section);
	return err;
}

/*!
 * \brief Flush stale sections on a started stm_te section filter
 *
 * Should be called whenever the filtering parameters change
 *
 * \param filter stm_te filter object to flsuh
 */
void te_section_flush(struct te_obj *filter)
{
	int err;
	ST_ErrorCode_t hal_err;

	struct te_section_filter *section = te_section_filter_from_obj(filter);

	FullHandle_t hal_filter = section->output.hal_filter->hdl;
	FullHandle_t hal_buffer = section->output.hal_buffer->hdl;

	/* Delete any sections that exist in the HAL circular buffer */
	hal_err = stptiHAL_call(Buffer.HAL_BufferFiltersFlush,
			hal_buffer,
			&hal_filter, 1);
	if (ST_NO_ERROR != hal_err)
		pr_warn("HAL_BufferFiltersFlush return 0x%x\n", hal_err);

	/* Empty any queued data for this filter */
	err = te_out_filter_queue_empty(&section->output);
	if (err)
		pr_warn("Failed to empty queue for section filter %p (%d)\n",
				filter, err);
}

/*!
 * \brief Copies section bytes from stm_te format to te HAL format
 *
 * stm_te format includes 2-byte section length at [1:2], te HAL format skips
 * these 2-bytes
 */
static void translate_section_bytes(const uint8_t *pattern, uint32_t length,
		uint8_t *out_pattern, uint8_t *out_length)
{
	int i;
	memset(out_pattern, 0, MAX_FILTER_BYTES);
	out_pattern[0] = pattern[0];
	*out_length = 1;
	for (i = 3; i < length && i < TE_MAX_SECTION_FILTER_LENGTH; i++) {
		out_pattern[i - 2] = pattern[i];
		(*out_length)++;
	}
}


static void print_section_info(struct te_section_filter *section)
{
	char print_filter_bytes[TE_MAX_SECTION_FILTER_LENGTH];
	char print_filter_mask[TE_MAX_SECTION_FILTER_LENGTH];
	char print_filter_pattern[TE_MAX_SECTION_FILTER_LENGTH];

	pr_debug("Section filter %s info:\n", section->output.obj.name);
	if (get_filter_bytes(section)) {
		hex_dump_to_buffer(get_filter_bytes(section),
				section->filter_hal_size,
				TE_MAX_SECTION_FILTER_LENGTH, 1,
				print_filter_bytes,
				sizeof(print_filter_bytes),
				false);
		pr_debug("Filter bytes........ %s\n", print_filter_bytes);
	}

	if (get_filter_mask(section)) {
		hex_dump_to_buffer(get_filter_mask(section),
				section->filter_hal_size,
				TE_MAX_SECTION_FILTER_LENGTH, 1,
				print_filter_mask,
				sizeof(print_filter_mask),
				false);
		pr_debug("Filter masks........ %s\n", print_filter_mask);
	}
	if (get_pn_pattern(section)) {
		hex_dump_to_buffer(get_pn_pattern(section),
				section->filter_hal_size,
				TE_MAX_SECTION_FILTER_LENGTH, 1,
				print_filter_pattern,
				sizeof(print_filter_pattern),
				false);
		pr_debug("Filter p/n pattern ........ %s\n",
					print_filter_pattern);
	}
}

static int te_section_filter_read_bytes(struct te_out_filter *filter,
		uint8_t *buffer, uint32_t buffer_size, uint32_t *bytesread)
{
	bool crc_valid;
	/* We need to support many SFs per pid. To limit the number of TP DMAs
	 * per pid all sections on a given pid are output into a single internal
	 * stpti circular buffer. This means we have many readers of a single
	 * buffer. When a read is done we check to see if the first section in
	 * the buffer matched the filter the reader is reading, if it does not
	 * we buffer this section in an array of linked lists (one list per SF)
	 * and try the next section in the circular buffer. This is repeated
	 * until either the correct section is found or the buffer is emptied
	 * into the linked lists. All subsequent reads first check in the linked
	 * lists before reading from the stpti circular buffer.
	 * Note we do not let the lists grow forever but limit the amound of
	 * memory that can be consumed in this way.
	 */
	return te_out_filter_queue_read(filter, buffer, buffer_size, bytesread,
			&crc_valid);
}

static int32_t te_section_filter_next_read_size(struct te_out_filter *filter)
{
	int32_t size = 0;

	if (te_out_filter_queue_peek(filter, &size))
		size = 0;

	pr_debug("size=%u\n", size);
	return size;
}

/*!
 * \brief print out the stpti filter type as a string
 *
 * \param hal_filter_type HAL filter type to print
 */
static void te_print_filter_type(stptiHAL_FilterType_t hal_filter_type)
{
	switch (hal_filter_type) {
	case stptiHAL_PNMM_VNMM_FILTER:
		pr_debug("Filter = stptiHAL_PNMM_VNMM_FILTER\n");
		break;
	case stptiHAL_PNMM_FILTER:
		pr_debug("Filter = stptiHAL_PNMM_FILTER\n");
		break;
	case stptiHAL_SHORT_VNMM_FILTER:
		pr_debug("Filter = stptiHAL_SHORT_VNMM_FILTER\n");
		break;
	case stptiHAL_SHORT_FILTER:
		pr_debug("Filter = stptiHAL_SHORT_FILTER\n");
		break;
	case stptiHAL_LONG_VNMM_FILTER:
		pr_debug("Filter = stptiHAL_LONG_VNMM_FILTER\n");
		break;
	case stptiHAL_LONG_FILTER:
		pr_debug("Filter = stptiHAL_LONG_FILTER\n");
		break;
	default:
		pr_debug("Filter = stptiHAL_NO_FILTER\n");
		break;
	}
}

/*!
 * \brief Determines the appropriate HAL filter type for a section filter
 *
 * \param length          Filter length
 * \param pos_neg_match   Is the filter in positive negative match mode
 * \param not_match_mode  Is the filter in not match mode
 * \param hal_filter_type Returned HAL filter type
 */
void te_compute_section_filter_type(uint8_t length,
				    bool pos_neg_match,
				    bool not_match_mode,
				    stptiHAL_FilterType_t *hal_filter_type)
{
	int remainder = (length % 8);
	int multipler = (int)(length / 8);

	*hal_filter_type = stptiHAL_NO_FILTER;

	if (remainder)
		multipler++;

	switch (multipler) {
	case 1:
		if (not_match_mode && pos_neg_match)
			*hal_filter_type = stptiHAL_PNMM_VNMM_FILTER;

		else if (pos_neg_match && !not_match_mode)
			*hal_filter_type = stptiHAL_PNMM_FILTER;

		else if (!pos_neg_match && not_match_mode)
			*hal_filter_type = stptiHAL_SHORT_VNMM_FILTER;

		else
			*hal_filter_type = stptiHAL_SHORT_FILTER;
		break;

	case 2:
		if (not_match_mode)
			*hal_filter_type = stptiHAL_LONG_VNMM_FILTER;

		else
			*hal_filter_type = stptiHAL_LONG_FILTER;
		break;

	default:
		break;
	}

	te_print_filter_type(*hal_filter_type);
	return;
}

struct te_section_filter *find_attached_section_filter(
		struct te_in_filter *input, struct te_section_filter *exclude)
{
	int i;
	struct te_obj *of;

	for (i = 0; i < MAX_INT_CONNECTIONS; i++) {
		of = input->output_filters[i];
		if (of != NULL && of != &exclude->output.obj) {
			if (of->type == TE_OBJ_SECTION_FILTER) {
				return te_section_filter_from_obj(of);
			}
		}
	}

	return NULL;
}

/*!
 * \brief Creates and associates HAL resource for attaching a PID filter to a s
 * section filter
 *
 * \param pid TE obj for pid filter to attach
 * \param sec TE obj for seciton filter to attach to
 *
 * \retval 0       Success
 * \retval -EEXIST Objects already connected
 * \retval -ENOMEM Insuffcient resource
 * \retval -EINVAL Bad parameter
 */
int attach_pid_to_sf(struct te_obj *pid, struct te_obj *sec)
{
	ST_ErrorCode_t Error = ST_NO_ERROR;
	int result = 0;

	struct te_section_filter *section = te_section_filter_from_obj(sec);
	struct te_in_filter *input = te_in_filter_from_obj(pid);
	struct te_demux *demux = te_demux_from_obj(section->output.obj.parent);

	stptiHAL_FilterConfigParams_t sf_params = { 0 };
	stptiHAL_BufferConfigParams_t buf_params = { 0 };
	stptiHAL_SlotConfigParams_t slot_params = { 0 };

	pr_debug("Pid Filter %p Section Filter %p\n", pid, sec);
	print_section_info(section);

	if (input->sf_slot == NULL) {
		/*
		 * This is the first SF on the PID Filter.
		 * Create the SF Buffer + Slot
		 */

		/* Alloc Buffer + attach to signal */
		te_out_filter_get_buffer_params(&section->output, &buf_params);

		/* Reset pointers for the new buffer */
		section->output.wr_offs = 0;
		section->output.rd_offs = 0;

		result = te_hal_obj_alloc(&section->output.hal_buffer,
						demux->hal_session,
						OBJECT_TYPE_BUFFER,
						&buf_params);

		if (result) {
			pr_err("unable to allocate buffer\n");
			goto error;
		}

		/* Store all the params in this SF */
		section->output.buf_start = buf_params.BufferStart_p;
		section->output.buf_size = buf_params.BufferSize;

		Error = stptiHAL_call(Buffer.HAL_BufferSetThreshold,
				section->output.hal_buffer->hdl, 1);
		if (Error != ST_NO_ERROR) {
			pr_err("HAL_BufferSetThreshold error 0x%x\n",
					Error);
			result = te_hal_err_to_errno(Error);
			goto error;
		}

		/* Register the buffer work. Note that since this buffer work is
		 * registered against the buffer FIRST, it will prevent the
		 * regular buffer work from being registered */
		te_bwq_register(demux->bwq, sec, section->output.hal_buffer,
				te_section_work);

		section->work_owner = true;
		section->owner = section;

		/* Alloc Slot */

		if (section->ecm_filter) {
			slot_params.SlotMode = stptiHAL_SLOT_TYPE_ECM;
		} else {
			if (!section->proprietary_1_filtering)
				slot_params.SlotMode =
					stptiHAL_SLOT_TYPE_SECTION;
			else
				slot_params.SlotMode = stptiHAL_SLOT_TYPE_EMM;
		}
		slot_params.SoftwareCDFifo = FALSE;
		result = te_hal_obj_alloc(&input->sf_slot, demux->hal_session,
						OBJECT_TYPE_SLOT, &slot_params);

		if (result) {
			pr_err("couldn't allocate slot\n");
			goto error;
		}

		list_add(&input->sf_slot->entry, &input->slots);

		Error = stptiOBJMAN_AssociateObjects(input->sf_slot->hdl,
					section->output.hal_buffer->hdl);
		if (ST_NO_ERROR != Error) {
			pr_err("stptiOBJMAN_AssociateObjects error 0x%x\n",
					Error);
			result = te_hal_err_to_errno(Error);
			goto error;
		}

		Error = stptiHAL_call(Slot.HAL_SlotSetPID, input->sf_slot->hdl,
				input->pid, FALSE);
		if (ST_NO_ERROR != Error) {
			pr_err("HAL_SlotSetPID error 0x%x\n", Error);
			result = te_hal_err_to_errno(Error);
			goto error;
		}
		if (pid->type == TE_OBJ_PID_FILTER)
			result = te_pid_filter_update(pid);
	} else {
		/*
		 * Reuse existing slots + buffers
		 */
		struct te_section_filter *friend =
				find_attached_section_filter(input, section);

		if (!friend) {
			pr_err("Section slot found, but no section filter\n");
			result = -ENOTRECOVERABLE;
			goto error;
		}
		te_hal_obj_inc(friend->output.hal_buffer);
		te_hal_obj_inc(input->sf_slot);
		section->output.hal_buffer = friend->output.hal_buffer;
		section->output.buf_start = friend->output.buf_start;
		section->output.buf_size = friend->output.buf_size;
		section->owner = friend;
	}

	if (0 != input->path_id) {
		stptiHAL_call(Slot.HAL_SlotSetSecurePathID,
			      input->sf_slot->hdl,
			      input->path_id);
	}

	/* We need to update this section to allow setting of filter data
	 * before the attach - currently assume this has not been done */
	/* create a HAL Section filter */

	if (section->proprietary_1_filtering) {
		sf_params.FilterType = stptiHAL_PROPRIETARY_FILTER;
	} else {
		te_compute_section_filter_type(
				section->filter_hal_size,
				section->pos_neg_filtering,
				section->version_not_match_filtering,
				&sf_params.FilterType);
	}

	section->filter_type = sf_params.FilterType;

	result = te_hal_obj_alloc(&section->output.hal_filter,
					demux->hal_session,
					OBJECT_TYPE_FILTER, &sf_params);

	if (result) {
		pr_err("unable to allocate HAL filter\n");
		goto error;
	}


	/* Associate the filter to the slot */
	Error = stptiOBJMAN_AssociateObjects(section->output.hal_filter->hdl,
						input->sf_slot->hdl);
	if (ST_NO_ERROR != Error) {
		pr_err("couldn't associate HAL filter to slot\n");
		result = te_hal_err_to_errno(Error);
		goto error;
	}

	if (sf_params.FilterType != stptiHAL_PROPRIETARY_FILTER) {
		pr_debug("Updating HAL for section filter %p\n", &section->output.obj);

		/* Call filterset and enable to activate the filter */
		Error = stptiHAL_call(Filter.HAL_FilterUpdate,
				section->output.hal_filter->hdl,
				sf_params.FilterType,
				section->section_filter_repeated ? 0 : 1,
				section->force_crc_check,
				section->filter_bytes,
				section->filter_masks,
				section->pos_neg_filtering ?
				section->pos_neg_pattern : NULL);
		if (ST_NO_ERROR != Error) {

			pr_err("HAL_FilterUpdate error 0x%x\n", (unsigned int)Error);
			result = te_hal_err_to_errno(Error);
			goto error;
		}

		/* always set HAL DiscardOnCRC to off, as we will discard CRC
		 * in TE instead of HAL */
		Error = stptiHAL_call(Filter.HAL_FilterSetDiscardOnCRCErr,
					section->output.hal_filter->hdl,
					FALSE);

		if (ST_NO_ERROR != Error) {
			pr_err("HAL_FilterSetDiscardOnCRCErr error 0x%x\n", Error);
			result = te_hal_err_to_errno(Error);
			goto error;
		}

		Error = stptiHAL_call(Filter.HAL_FilterEnable,
					section->output.hal_filter->hdl, TRUE);

		if (ST_NO_ERROR != Error) {
			pr_err("HAL_FilterEnable error 0x%x\n", Error);
			result = te_hal_err_to_errno(Error);
			goto error;
		}
	}

	return 0;

error:
	if (input->sf_slot) {
		if (te_hal_obj_dec(input->sf_slot))
			input->sf_slot = NULL;
	}

	if (section->output.hal_buffer) {
		if (section->work_owner)
			te_bwq_unregister(demux->bwq,
				section->output.hal_buffer);
		if (te_hal_obj_dec(section->output.hal_buffer))
			section->output.hal_buffer = NULL;
	}

	if (section->output.hal_filter) {
		if (te_hal_obj_dec(section->output.hal_filter))
			section->output.hal_filter = NULL;
	}

	return result;
}

void te_section_filter_disconnect(struct te_obj *out_flt, struct te_obj *in_flt)
{
	struct te_section_filter *section = te_section_filter_from_obj(out_flt);
	struct te_in_filter *input = te_in_filter_from_obj(in_flt);
	struct te_demux *demux = te_demux_from_obj(out_flt->parent);
	int err;

	/* Flush cached sections on detach */
	if (section->output.flushing_behaviour
		& STM_TE_FILTER_CONTROL_FLUSH_ON_DETACH)
		te_section_flush(out_flt);

	/* DisAssociate the filter from the slot */
	err = stptiOBJMAN_DisassociateObjects(section->output.hal_filter->hdl,
						input->sf_slot->hdl);
	if (ST_NO_ERROR != err)
		pr_warn("couldn't disassociate HAL filter from slot\n");

	/* If we destroy the slot then the buffer association
	 * will be broken automatically. Section filters share a
	 * single slot and buffer, so only deallocate if this is
	 * the last SF using this slot and buffer.
	 */
	if (te_hal_obj_dec(input->sf_slot)) {
		input->sf_slot = NULL;
		if (section->output.hal_buffer)
			te_bwq_unregister(demux->bwq,
				section->output.hal_buffer);
		if (in_flt->type == TE_OBJ_PID_FILTER)
			te_pid_filter_update(in_flt);
	} else {
		if (section->work_owner) {
			int i;
			struct te_obj *of;
			struct te_section_filter *current_sec;
			struct te_section_filter *friend =
				find_attached_section_filter(input, section);

			pr_debug("transfering work from %p to %p\n", out_flt,
				&friend->output.obj);
			te_bwq_unregister(demux->bwq,
					section->output.hal_buffer);
			/* Since buffer work queues are created along with HAL
			 * buffers, there is no need to check hal buffer ptr */
			for (i = 0; i < MAX_INT_CONNECTIONS; i++) {
				of = input->output_filters[i];
				if (of != NULL && of != &section->output.obj
					&& of->type == TE_OBJ_SECTION_FILTER) {
					current_sec =
						te_section_filter_from_obj(of);
					current_sec->owner = friend;
				}
			}
			te_bwq_register(demux->bwq, &friend->output.obj,
				section->output.hal_buffer,te_section_work);
			section->work_owner = false;
			friend->work_owner = true;
		}
	}

	if (te_hal_obj_dec(section->output.hal_buffer))
		section->output.hal_buffer = NULL;

	if (te_hal_obj_dec(section->output.hal_filter)) {
		section->output.hal_filter = NULL;
		section->output.obj.state = TE_OBJ_STOPPED;
	}
}

/*!
 * \brief Updates a the HAL objects for an existing te section filter object
 *
 * \param filter The section filter update
 *
 * \retval 0    Success
 * \retval -EIO An internal HAL call failed
 */
static int te_section_filter_hal_update(struct te_section_filter *section)
{
	int err = 0;
	uint32_t new_filter_type;

	ST_ErrorCode_t hal_err;

	struct te_demux *demux = te_demux_from_obj(section->output.obj.parent);

	if (TE_OBJ_STARTED != section->output.obj.state)
		return 0;

	pr_debug("Updating HAL for section filter %p\n", &section->output.obj);
	print_section_info(section);

	/* TODO: This function needs to cope if pattern/masks are not yet set
	 * */

	/* Check if the new HAL filter type is different from the existing type
	 */
	if (!section->proprietary_1_filtering) {
		te_compute_section_filter_type(section->filter_hal_size,
				section->pos_neg_filtering,
				section->version_not_match_filtering,
				&new_filter_type);
	} else {
		new_filter_type = stptiHAL_PROPRIETARY_FILTER;
	}

	if (new_filter_type == section->filter_type) {
		pr_debug("filter %p HAL type unchanged\n", &section->output.obj);
	} else {
		/* Whilst the HAL allows us to change the filter type between
		 * certain filter types, to keep things simple we always delete
		 * and re-create the filter if the type has changed.
		 *
		 * This means we need to store the existing slot associations
		 * of the HAL filter and re-associate with the new HAL filter
		 * */
		int slot_count;
		stptiHAL_FilterConfigParams_t params;
		int i;
		FullHandle_t *slot_array;
		slot_array = kmalloc(sizeof(FullHandle_t) * MAX_INT_CONNECTIONS,
				GFP_KERNEL);
		if (!slot_array) {
			pr_err("No memory for slot array\n");
			err = -ENOMEM;
			goto error;
		}

		pr_debug("filter %p HAL type change %d -> %d\n",
				&section->output.obj,
				section->filter_type,
				new_filter_type);

		/* Find the associated slots so that we can re-associate
		 * afterwards */
		slot_count = stptiOBJMAN_ReturnAssociatedObjects(
				section->output.hal_filter->hdl,
				slot_array, MAX_INT_CONNECTIONS,
				OBJECT_TYPE_SLOT);

		te_hal_obj_dec(section->output.hal_filter);

		params.FilterType = new_filter_type;

		err = te_hal_obj_alloc(&section->output.hal_filter, demux->hal_session,
					OBJECT_TYPE_FILTER, &params);

		if (err) {
			pr_err("couldn't reallocate HAL filter\n");
			kfree(slot_array);
			goto error;
		}

		for (i = 0; i < slot_count; i++) {
			hal_err = stptiOBJMAN_AssociateObjects(
					section->output.hal_filter->hdl,
					slot_array[i]);
			if (ST_NO_ERROR != hal_err) {
				pr_err("Failed to associate slot 0x%x with filter 0x%x (0x%x)\n",
						slot_array[i].word,
						section->output.hal_filter->hdl.word, hal_err);
				err = te_hal_err_to_errno(hal_err);
				kfree(slot_array);
				goto error;
			}
			pr_debug("Associated filter 0x%x to slot 0x%x\n",
						section->output.hal_filter->hdl.word,
						slot_array[i].word);
		}

		/* Store new HAL filter and type for this te object */
		section->filter_type = new_filter_type;

		kfree(slot_array);
	}


	/* Call HAL FilterUpdateProprietary if proprietary is set */
	if (!section->proprietary_1_filtering) {
		hal_err = stptiHAL_call(Filter.HAL_FilterUpdate,
				section->output.hal_filter->hdl,
				new_filter_type,
				section->section_filter_repeated ? 0 : 1,
				section->force_crc_check,
				get_filter_bytes(section),
				get_filter_mask(section),
				get_pn_pattern(section));
		if (ST_NO_ERROR == hal_err) {
			/* always set HAL DiscardOnCRC to off, as we will
			 * discard CRC in TE instead of HAL */
			hal_err = stptiHAL_call(
					Filter.HAL_FilterSetDiscardOnCRCErr,
					section->output.hal_filter->hdl,
					FALSE);
		}

	} else {
		hal_err = stptiHAL_call(
				Filter.HAL_FilterUpdateProprietaryFilter,
				section->output.hal_filter->hdl,
				new_filter_type,
				get_filter_bytes(section),
				section->proprietary_size);
	}
	if (ST_NO_ERROR != hal_err) {
		pr_err("HAL_FilterUpdate return 0x%x\n", hal_err);
		err = te_hal_err_to_errno(hal_err);
		goto error;
	}

	hal_err = stptiHAL_call(Filter.HAL_FilterEnable,
			section->output.hal_filter->hdl, TRUE);
	if (ST_NO_ERROR != hal_err) {
		pr_err("HAL_FilterEnable return 0x%x\n", hal_err);
		err = te_hal_err_to_errno(hal_err);
		goto error;
	}

	/* Discard on CRC error will be performed in TE, so we do not need to
	 * update HAL for this option
	 */

	/* Flush any queued sections for this filter */
	te_section_flush(&section->output.obj);

	return 0;

error:
	if (section->output.hal_filter) {
		if (te_hal_obj_dec(section->output.hal_filter))
			section->output.hal_filter = NULL;
	}

	return err;
}

static int te_section_filter_delete(struct te_obj *filter)
{
	struct te_section_filter *section = te_section_filter_from_obj(filter);
	int err;
	struct te_demux *dmx;

	dmx = te_demux_from_obj(filter->parent);

	/* Acquire the section lock for this vdevice.
	 * This is to ensure that we do not proceed for deletion
	 * until an active section work is completed.
	 * */
	if (mutex_lock_interruptible(&dmx->of_sec_lock) != 0)
		return -EINTR;
	err = te_out_filter_deinit(&section->output);
	if (err) {
		mutex_unlock(&dmx->of_sec_lock);
		return err;
	}

	kfree(section);
	mutex_unlock(&dmx->of_sec_lock);
	return 0;
}

static int te_section_filter_get_stat(struct te_obj *obj,
				stm_te_output_filter_stats_t *stat)
{
	int err = 0;
	struct timespec ts;
	ktime_t ktime;

	struct te_section_filter *section;

	section = te_section_filter_from_obj(obj);

	stat->crc_errors = section->crc_error_count;
	stat->packet_count = section->packet_count;
	stat->buffer_overflows = section->buffer_overflow_count;
	getrawmonotonic(&ts);
	ktime = timespec_to_ktime(ts);
	stat->system_time = ktime_to_us(ktime);
	return err;
}

static struct te_obj_ops te_section_filter_ops = {
	.attach = te_out_filter_attach,
	.detach = te_out_filter_detach,
	.set_control = te_section_filter_set_control,
	.get_control = te_section_filter_get_control,
	.delete = te_section_filter_delete,
};

static int te_section_filter_flush(struct te_out_filter *filter)
{
	ST_ErrorCode_t hal_err;
	int err = 0;

	/* Flush the HAL filter from the HAL buffer */
	if (filter->hal_buffer && filter->hal_filter) {
		te_hal_obj_inc(filter->hal_buffer);
		te_hal_obj_inc(filter->hal_filter);

		hal_err = stptiHAL_call(Buffer.HAL_BufferFiltersFlush,
				filter->hal_buffer->hdl,
				&filter->hal_filter->hdl, 1);
		if (ST_NO_ERROR != hal_err)
			pr_err("BufferFiltersFlush return 0x%x\n", hal_err);
		err = te_hal_err_to_errno(hal_err);

		te_hal_obj_dec(filter->hal_filter);
		te_hal_obj_dec(filter->hal_buffer);
	}

	/* Clear any queued sections */
	if (!err)
		err = te_out_filter_queue_empty(filter);

	return err;
}

int te_section_filter_new(struct te_obj *demux, struct te_obj **new_filter)
{
	int res = 0;
	char name[STM_REGISTRY_MAX_TAG_SIZE];

	struct te_section_filter *filter = kzalloc(sizeof(*filter), GFP_KERNEL);

	if (!filter) {
		pr_err("couldn't allocate TS Index filter object\n");
		res = -ENOMEM;
		goto error;
	}

	snprintf(name, STM_REGISTRY_MAX_TAG_SIZE, "SectionFilter.%p",
				&filter->output.obj);
	res = te_out_filter_init(&filter->output, demux, name,
			TE_OBJ_SECTION_FILTER);
	if (res != 0)
		goto error;

	/* Initialise TS Index filter data */
	filter->filter_hal_size = TE_DEFAULT_SECTION_FILTER_LEN;
	filter->section_filter_repeated = TE_DEFAULT_SECTION_FILTER_REPEAT;
	filter->discard_on_crc_error =
			TE_DEFAULT_DISCARD_ON_CRC_ERROR;
	filter->force_crc_check =
			TE_DEFAULT_SECTION_FILTER_FORCE_CRC_CHECK;
	filter->pos_neg_filtering =
			TE_DEFAULT_SECTION_FILTER_POS_NEG_MODE;
	filter->version_not_match_filtering =
			TE_DEFAULT_SECTION_FILTER_VERSION_NOT_MATCH;

	filter->output.obj.ops = &te_section_filter_ops;

	filter->output.flush = &te_section_filter_flush;

	/* Section filter uses queue implementation to "cache" sections in
	 * memory so the read_bytes/next_read_size functions differ from
	 * regular output filters */
	filter->output.read_bytes = &te_section_filter_read_bytes;
	filter->output.next_read_size = &te_section_filter_next_read_size;

	/* Section filter has a different buffer work function (only supports
	 * pull sink) */
	filter->output.buffer_work_func = &te_section_work;

	*new_filter = &filter->output.obj;
	return 0;

error:
	*new_filter = NULL;
	kfree(filter);

	return res;
}

/*!
 * \brief Retrieve a te section filter object from a hal section filter handle
 *
 * Scope of search is limited to children of a specified te demux object
 *
 * The demux object list must not change whilst this function is being called.
 * I.e. we must hold a global (TODO: or demux-level when it exists) read lock
 */
static void te_get_section_filter_by_hal_handle(struct te_obj *demux_obj,
		FullHandle_t hal_handle, struct te_section_filter **te_sf)
{
	struct te_out_filter *out_filter = NULL;
	struct te_demux *dmx = te_demux_from_obj(demux_obj);

	*te_sf = NULL;

	read_lock(&dmx->out_flt_rw_lock);
	list_for_each_entry(out_filter, &dmx->out_filters, obj.lh) {

		if (out_filter->hal_filter == NULL)
			continue;

		if (HAL_HDL_EQUAL(out_filter->hal_filter->hdl, hal_handle) &&
			TE_OBJ_SECTION_FILTER == out_filter->obj.type) {
			*te_sf = te_section_filter_from_obj(
					&out_filter->obj);
			break;
		}

	}
	read_unlock(&dmx->out_flt_rw_lock);
}

/*!
 * \brief Reads section data from HAL buffer handle and stores in section
 * lists
 *
 * Also notifies any pull sinks attached to matching section filters that data
 * is available
 **/
static void te_read_section_buffer(struct te_obj *demux, FullHandle_t buffer,
		uint8_t *local_buffer, uint32_t local_buffer_size)
{
	ST_ErrorCode_t error = ST_NO_ERROR;
	uint32_t read_offset = stptiHAL_CURRENT_READ_OFFSET;
	uint32_t bytes_read = 0;
	stptiHAL_SectionFilterMetaData_t metadata = { 0 };
	int i;
	bool buffer_empty = false;
	struct te_demux *dmx;

	dmx = te_demux_from_obj(demux);

	while (!buffer_empty) {
		error = stptiHAL_call(Buffer.HAL_BufferRead, buffer,
				stptiHAL_READ_AS_UNITS_NO_TRUNCATION,
				&read_offset, 0, local_buffer,
				local_buffer_size, NULL, 0, &metadata,
				sizeof(metadata), te_out_filter_memcpy,
				&bytes_read);
		switch (error) {
		case ST_NO_ERROR:
			break;
		case STPTI_ERROR_NO_PACKET:
			/* Buffer empty. Stop reading */
			buffer_empty = true;
			break;
		case STPTI_ERROR_NOT_ENOUGH_ROOM_TO_RETURN_DATA:
			/* Not enough room in local buffer. Print warning */
			pr_warn("Demux %s: not enough room for buf data 0x%x\n",
					demux->name, buffer.word);
			break;
		default:
			/* Error return code(s) */
			pr_err("Demux %s: buf read err 0x%x\n", demux->name,
					error);
			buffer_empty = true;
			break;
		}

		if (buffer_empty)
			continue;

		/* Acquire the lock before starting the processing.
		 * This is to ensure that we do not process any data while
		 * a delete is performed on a section filter
		 * */
		if (mutex_lock_interruptible(&dmx->of_sec_lock))
			return;

		/* For each matched filter:
		 * 1. Store the section in the queue for that
		 * filter
		 * 2. Notify the filter's pull sink if it has one
		 */
		for (i = 0; i < metadata.FiltersMatched; i++) {
			int err;
			struct te_section_filter *te_sf = NULL;
			if (HAL_HDL_IS_NULL(metadata.FilterHandles[i]))
				continue;

			te_get_section_filter_by_hal_handle(demux,
					metadata.FilterHandles[i], &te_sf);
			if (!te_sf) {
				pr_warn("Could not find section filter\n");
				continue;
			}

			/* Check whether section filter is attached or not to
			* input filter.This check protects the false trigger
			* from TP.
			* Basically during section filter detachment HAL has
			* updated TP about slot<-->SF detachment. But TP
			* has not read this info yet. So we might get a
			* false trigger because TP assumes this section
			* filter is still connected. So we ignore this trigger.
			* In next trigger, TP will update the association
			* correctly. */

			if (TE_OBJ_STOPPED == te_sf->output.obj.state) {
				pr_warn("section filter is disconnected from input filter");
				continue;
			}
			te_sf->output.rd_offs = read_offset;

			pr_debug("%s got section: %u bytes\n",
					te_sf->output.obj.name,
					bytes_read);

			if (metadata.CRC_OK == FALSE) {
				te_sf->crc_error_count++;
				te_out_filter_send_event(&te_sf->output,
						STM_TE_SECTION_FILTER_EVENT_CRC_ERROR);
			}

			if (metadata.CRC_OK == TRUE ||
			     te_sf->discard_on_crc_error == false){

				err = te_out_filter_queue_data(&te_sf->output,
						local_buffer, bytes_read,
						metadata.CRC_OK);
				if (err) {
					if (err == -ENOMEM) {
						te_sf->buffer_overflow_count++;
						te_out_filter_notify_pull_sink(&te_sf->output,
								STM_MEMSINK_EVENT_BUFFER_OVERFLOW);
					}
					continue;
				}

				te_sf->packet_count++;

				/* Notify pull sink that section is available
				 * (if pull sink notify function exists) */
				te_out_filter_notify_pull_sink(&te_sf->output,
					STM_MEMSINK_EVENT_DATA_AVAILABLE);

				/* Wake-up any queued reading thread for this
				 * filter */
				wake_up_interruptible(&te_sf->output.reader_waitq);
			}
		}
		mutex_unlock(&dmx->of_sec_lock);
	}

	/* Update Read Pointer for this buffer */
	error = stptiHAL_call(Buffer.HAL_BufferSetReadOffset, buffer,
			read_offset);
	if (ST_NO_ERROR != error) {
		pr_err("Demux %s: update buf %x read offset err 0x%x\n",
				demux->name, buffer.word, error);
	}
}

static void te_section_work(struct te_obj *obj, struct te_hal_obj *buffer)
{
	struct te_section_filter *sf, *sec;

	if (IS_ERR_OR_NULL(obj)) {
		pr_err("Invalid section filter obj 0x%p\n", obj);
		return;
	}

	sf = te_section_filter_from_obj(obj);
	if (IS_ERR_OR_NULL(sf)) {
		pr_err("Invalid section filter passed sf 0x%p\n", sf);
		return;
	}

	sec = sf->owner;
	if (!sec) {
		pr_err("Invalid section filter owner\n");
		return;
	}

	/* Make sure hal buffer still exists */
	if (stptiOBJMAN_CheckHandle(buffer->hdl, OBJECT_TYPE_BUFFER) !=
		ST_NO_ERROR) {
		pr_debug("Buffer 0x%x no longer exists\n", buffer->hdl.word);
		return;
	}

	if (mutex_lock_interruptible(&sec->output.obj.lock) < 0)
		return;

	/* Read all section data from the buffer into the section
	 * lists for this te demux object */
	te_read_section_buffer(obj->parent, buffer->hdl, sec->read_buffer,
			TE_MAX_SECTION_SIZE);

	mutex_unlock(&sec->output.obj.lock);
}

