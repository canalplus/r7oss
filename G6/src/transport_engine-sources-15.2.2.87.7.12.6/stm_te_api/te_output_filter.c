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

Source file name : te_output_filter.c

Defines base output filter specific operations
******************************************************************************/

#include <stm_te_dbg.h>
#include <te_object.h>
#include <pti_hal_api.h>
#include <te_interface.h>
#include <linux/dma-direction.h>
#include <asm/cacheflush.h>

#include "te_object.h"
#include "te_global.h"
#include "te_output_filter.h"
#include "te_ts_filter.h"
#include "te_input_filter.h"
#include "te_pid_filter.h"
#include "te_ins_filter.h"
#include "te_rep_filter.h"
#include "te_ts_index_filter.h"
#include "te_pes_filter.h"
#include "te_section_filter.h"
#include "te_pcr_filter.h"
#include "te_demux.h"
#include "te_time.h"
#include "te_global.h"
#include <stm_memsrc.h>
#include <stm_event.h>
#include "te_sysfs.h"

#define TE_PAUSE 1
#define TE_RESUME 0

/* Data struct for storing queued data packets */
struct te_queued_data {
	struct list_head lh;
	uint8_t *data;
	uint32_t size;
	bool valid;
};

int te_out_filter_from_hdl(stm_object_h hdl, struct te_out_filter **output)
{
	int err;
	struct te_obj *object;

	err = te_obj_from_hdl(hdl, &object);
	if (err == 0)
		*output = te_out_filter_from_obj(object);
	else
		*output = NULL;
	return err;
}

static void te_inv_range(void *addr, size_t size)
{
	unsigned long phys = virt_to_phys(addr);
#if defined(CONFIG_CPU_SH4)
	invalidate_ioremap_region(phys, addr, 0, size);
#elif defined(CONFIG_ARM)
	dmac_unmap_area((const void *)(addr), size, DMA_FROM_DEVICE);
	outer_inv_range(phys, phys + size);
	smp_mb();
#else
#error "Unsupported arch"
#endif
}

static void te_out_filter_pull_work(struct te_out_filter *output,
		struct te_hal_obj *buf)
{
	ST_ErrorCode_t hal_err = ST_NO_ERROR;
	uint32_t wr_offs = 0;
	int err = 0;

	/* Update buffer write offset */
	err = mutex_lock_interruptible(&output->obj.lock);
	if (err < 0)
		return;

	hal_err = stptiHAL_call(Buffer.HAL_BufferGetWriteOffset, buf->hdl,
			&wr_offs);
	if (hal_err != ST_NO_ERROR) {
		pr_err("HAL_BufferGetWriteOffset returned 0x%x\n", hal_err);
		mutex_unlock(&output->obj.lock);
		return;
	}

	/* Write offset has not changed. Do nothing */
	if (output->wr_offs == wr_offs) {
		mutex_unlock(&output->obj.lock);
		return;
	}

	/* Update write offset */
	output->wr_offs = wr_offs;

	pr_debug("rd_offs %d wr_offs %d\n", output->rd_offs, output->wr_offs);

	/* New data is available
	 * 1. Notify pull sink
	 * 2. Wake up any sleeping reader thread
	 */
	if (output->pull_intf.notify) {
		err = output->pull_intf.notify(output->external_connection,
				STM_MEMSINK_EVENT_DATA_AVAILABLE);
		if (unlikely(err))
			pr_err_ratelimited("Notify sink %p failed (%d)\n",
					output->external_connection, err);
	}
	mutex_unlock(&output->obj.lock);

	/* Wake up any waiting read thread */
	wake_up_interruptible(&output->reader_waitq);
}

static int te_out_filter_attach_to_pull_sink(struct te_obj *filter,
		stm_object_h target)
{
	int err;
	struct te_out_filter *output = te_out_filter_from_obj(filter);
	struct te_demux *demux = te_demux_from_obj(filter->parent);

	pr_debug("Attaching out filter %p to pull sink %p\n", filter,
			target);
	err = output->pull_intf.connect(filter, target, output->pull_src_intf);
	if (err) {
		pr_err("Failed to connect filter %p to pull sink %p (%d)\n",
				filter, target, err);
		goto error;
	}

	if (output->hal_buffer) {
		pr_debug("registering buffer on sink connect\n");
		err = te_bwq_register(demux->bwq, &output->obj,
				output->hal_buffer, output->buffer_work_func);
		if (err != -EEXIST && err != 0) {
			pr_err("unable to register buffer\n");
			goto error;
		}
	}


	err = stm_registry_add_connection(filter, STM_DATA_INTERFACE_PULL,
			target);
	if (err) {
		pr_err("Failed to register connection %s (%d)\n",
				STM_DATA_INTERFACE_PULL, err);
		if (output->pull_intf.disconnect(filter, target))
			pr_warn("Cleanup: disconnect failed\n");
	}
	return 0;
error:
	if (output->hal_buffer)
		if (te_bwq_unregister(demux->bwq, output->hal_buffer))
			output->hal_buffer = NULL;
	return err;
}

static int te_out_filter_detach_from_pull_sink(struct te_obj *filter,
		stm_object_h target)
{
	int err;
	int result = 0;
	struct te_out_filter *output = te_out_filter_from_obj(filter);
	struct te_demux *demux = te_demux_from_obj(filter->parent);

	pr_debug("Detaching out filter %p from pull sink %p\n", filter,
			target);

	err = stm_registry_remove_connection(filter, STM_DATA_INTERFACE_PULL);
	if (err) {
		pr_err("Failed to unregister connection %s (%d)\n",
				STM_DATA_INTERFACE_PULL, err);
		result = err;
	}

	/* Check if the output filter is of type SECTION.
	 * Section filters are special as they share the same HAL buffer.
	 * For Section filters Work Q unregistration is only done when the
	 * section to be deleted is the last user on the same slot.
	 * This decision is taken during deattach of Input filter with the
	 * section filter.
	 * */
	if (filter->type != TE_OBJ_SECTION_FILTER)
		if (output->hal_buffer) {
			err = te_bwq_unregister(demux->bwq,
					output->hal_buffer);
			if (err)
				output->hal_buffer = NULL;
		}

	/* Wake up any thread currently blocked in pull_data */
	err = mutex_lock_interruptible(&filter->lock);
	if (err >= 0) {
		output->external_connection = NULL;
		mutex_unlock(&filter->lock);
		wake_up_interruptible(&output->reader_waitq);
	}

	err = output->pull_intf.disconnect(filter, target);
	if (err) {
		pr_err("Failed to disconnect %p from pull sink %p (%d)\n",
				filter, target, err);
		result = err;
	} else {
		memset(&output->pull_intf, 0, sizeof(output->pull_intf));
	}


	return result;
}

static void te_out_filter_push_work(struct te_out_filter *output,
		struct te_hal_obj *buf)
{
	ST_ErrorCode_t Error = ST_NO_ERROR;
	struct stm_data_block block[2];
	int read;
	int blocks = 0;
	int index;
	int err = 0;
	struct te_demux *demux = te_demux_from_obj(output->obj.parent);
	int alignment_mask;
	int len;
	int new_rd_offs;

	if (mutex_lock_interruptible(&output->obj.lock) != 0)
		return;

	/* Read the data from the buffer */
	Error = stptiHAL_call(Buffer.HAL_BufferGetWriteOffset, buf->hdl, &output->wr_offs);
	if (Error != ST_NO_ERROR) {
		pr_err("HAL_BufferGetWriteOffset returned 0x%x\n", Error);
		goto done;
	}

	if (output->rd_offs == output->wr_offs)
		goto done; /* nothing to do */

	pr_debug("rd_offs %d wr_offs %d\n", output->rd_offs, output->wr_offs);

	/* Compute alignment mask */
	if (output->push_intf.alignment)
		alignment_mask = ~(output->push_intf.alignment - 1);
	else
		alignment_mask = ~0;

	if (output->rd_offs < output->wr_offs) {
		len = (output->wr_offs - output->rd_offs) & alignment_mask;
		if (!len)
			goto done; /* nothing to do */
		block[0].data_addr = output->buf_start + output->rd_offs;
		block[0].len = len;
		blocks = 1;
		new_rd_offs = output->rd_offs + len;
	} else {
		len = (output->buf_size - output->rd_offs + output->wr_offs) &
			alignment_mask;
		if (!len)
			goto done; /* nothing to do */

		block[0].data_addr = output->buf_start + output->rd_offs;
		block[0].len = min((output->buf_size - output->rd_offs), len);
		blocks = 1;

		len -= block[0].len;

		if (!len) {
			new_rd_offs = output->rd_offs + (int)block[0].len;
			if (new_rd_offs == output->buf_size)
				new_rd_offs = 0;
		} else {
			block[1].data_addr = output->buf_start;
			block[1].len = len;
			blocks = 2;
			new_rd_offs = len;
		}
	}

	for (index = 0; index < blocks; index++)
		te_inv_range(block[index].data_addr, block[index].len);

	err = output->push_intf.push_data(output->external_connection, block,
					blocks, &read);
	if (err)
		pr_warn_ratelimited("Push_data failed %d\n", err);

	output->rd_offs = new_rd_offs;

	stptiHAL_call(Buffer.HAL_BufferSetReadOffset, buf->hdl, output->rd_offs);

	if (demux->pacing_filter == &output->obj) {
		if (demux->upstream_obj && demux->push_notify.notify) {
			err = demux->push_notify.notify(
					demux->upstream_obj,
					STM_MEMSRC_EVENT_CONTINUE_INJECTION);
			if (unlikely(err)) {
				pr_err_ratelimited("Notify src %p failed(%d)\n",
						demux->upstream_obj,
						err);
			}
		}
	}

done:
	mutex_unlock(&output->obj.lock);
}

void te_out_filter_work(struct te_obj *obj, struct te_hal_obj *buf)
{
	struct te_out_filter *output = te_out_filter_from_obj(obj);
	if (obj->state == TE_OBJ_PAUSED) {
		pr_debug("%s(): Buffer %s paused\n", __func__, obj->name);
		return;
	}

	if (output->pull_intf.connect)
		te_out_filter_pull_work(output, buf);

	if (output->push_intf.connect)
		te_out_filter_push_work(output, buf);
}

static int te_out_filter_attach_to_push_sink(struct te_obj *filter,
		stm_object_h target)
{
	int err;
	struct te_out_filter *output = te_out_filter_from_obj(filter);
	struct te_demux *demux = te_demux_from_obj(filter->parent);

	pr_debug("Attaching out filter %p to push sink %p\n", filter,
			target);
	err = output->push_intf.connect(filter, target);
	if (err) {
		pr_err("Failed to connect filter %p to push sink %p (%d)\n",
				filter, target, err);
		goto error;
	}

	if (output->hal_buffer) {
		pr_debug("registering buffer on sink connect\n");
		err = te_bwq_register(demux->bwq, &output->obj,
				output->hal_buffer, output->buffer_work_func);
		if (err != -EEXIST && err != 0) {
			pr_err("unable to register buffer\n");
			goto error;
		}
	}

	err = stm_registry_add_connection(filter, STM_DATA_INTERFACE_PUSH,
			target);
	if (err) {
		pr_err("Failed to register connection %s (%d)\n",
				STM_DATA_INTERFACE_PUSH, err);
		goto error;
	}
	return 0;
error:
	if (output->hal_buffer)
		if (te_bwq_unregister(demux->bwq, output->hal_buffer))
			output->hal_buffer = NULL;
	if (output->push_intf.disconnect(filter, target))
		pr_warn("Cleanup: disconnect failed\n");
	return err;
}

/*!
 * \brief Copies data from an output filter into a kernel buffer
 *
 * Reads data directly from the output filter's HAL buffer, ignoring data
 * quantisation
 *
 * \param output      Output filter to read from
 * \param buf         Kernel virtual buffer to copy data into
 * \param buf_size    Size of buffer pointed to by buf
 * \param bytes_read  Pointer to uint32_t set to the actual number of bytes
 *                    copied by this function
 *
 * \retval 0       Success
 * \retval -EINVAL HAL error occurrred
 */
int te_out_filter_read_bytes(struct te_out_filter *output, uint8_t *buf,
		uint32_t buf_size, uint32_t *bytes_read)
{
	uint32_t read_offs = stptiHAL_CURRENT_READ_OFFSET;
	ST_ErrorCode_t hal_err;
	FullHandle_t buf_hdl = output->hal_buffer->hdl;
	int err;

	*bytes_read = 0;

	err = mutex_lock_interruptible(&output->obj.lock);
	if (err < 0)
		return err;

	hal_err = stptiHAL_call(Buffer.HAL_BufferRead, buf_hdl,
			stptiHAL_READ_IGNORE_QUANTISATION,
			&read_offs, 0,
			buf, buf_size, NULL, 0, NULL, 0,
			&te_out_filter_memcpy, bytes_read);

	/* Update read pointer and resignal if necessary (intentionally
	 * ignore returned error) */
	stptiHAL_call(Buffer.HAL_BufferSetReadOffset, buf_hdl, read_offs);

	output->rd_offs = read_offs;

	mutex_unlock(&output->obj.lock);

	return te_hal_err_to_errno(hal_err);
}

/*!
 * \brief Determines how much data will be read by the next read call on this
 * output filter using the current HAL buffer read/write offsets
 *
 * \param output Output filter to test
 *
 * \return Size of data available to read in the next read call
 */
int32_t te_out_filter_next_read_size(struct te_out_filter *output)
{
	/* Currently we ignore any quantisation units so the amount of data
	 * that can be read is equal to the current amount data in the buffer
	 */
	int32_t size = 0;
	int err;

	err = mutex_lock_interruptible(&output->obj.lock);
	if (err < 0)
		return err;

	if (output->wr_offs >= output->rd_offs) {
		size = output->wr_offs - output->rd_offs;
	} else {
		size = output->buf_size - output->rd_offs;
		size += output->wr_offs;
	}

	mutex_unlock(&output->obj.lock);

	pr_debug("size=%u\n", size);
	return size;
}

static int te_out_filter_detach_from_push_sink(struct te_out_filter *output,
		stm_object_h target)
{
	int err, ret;

	struct te_demux *demux = te_demux_from_obj(output->obj.parent);

	pr_debug("Detaching out filter %p from push sink %p\n", &output->obj,
								target);

	err = stm_registry_remove_connection(&output->obj,
						STM_DATA_INTERFACE_PUSH);
	if (err) {
		pr_err("Failed to unregister connection %s (%d)\n",
				STM_DATA_INTERFACE_PUSH, err);
	}

	/* Disconnect the push interface before unregistering the WQ
	 * This is to ensure the push sink doesn't block any pending push
	 * and release it.
	 * Inside WQ de-registration we wait for all pending WORK before
	 * releasing HAL buffer.
	 * */
	err = output->push_intf.disconnect(&output->obj, target);
	if (err)
		pr_err("Failed to disconnect %p from %p\n",
				&output->obj,
				target);

	if (output->hal_buffer) {
		ret = te_bwq_unregister(demux->bwq, output->hal_buffer);
		if (ret)
			output->hal_buffer = NULL;
	}

	if (err == 0)
		memset(&output->push_intf, 0, sizeof(output->push_intf));

	return err;
}

int te_out_filter_attach(struct te_obj *filter, stm_object_h target)
{
	int err;
	stm_object_h target_type;
	char type_tag[STM_REGISTRY_MAX_TAG_SIZE];
	uint32_t iface_size;
	struct te_out_filter *output = NULL;

	if (filter)
		output = te_out_filter_from_obj(filter);
	else
		return -EINVAL;

	/* Check if this output filter is already attached to an external
	 * object */
	if (output->external_connection) {
		pr_err("Output filter %p already has an attachment\n", filter);
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
				&output->pull_intf,
				&iface_size)) {
		pr_debug("attaching pull sink %p -> %p\n", filter, target);
		err = te_out_filter_attach_to_pull_sink(filter, target);
	} else if (0 == stm_registry_get_attribute(target_type,
				STM_DATA_INTERFACE_PUSH, type_tag,
				sizeof(stm_data_interface_push_sink_t),
				&output->push_intf,
				&iface_size)) {
		pr_debug("attaching push sink %p -> %p\n", filter, target);
		err = te_out_filter_attach_to_push_sink(filter, target);
	} else {
		pr_err("Attach %p -> %p: no supported interfaces\n", filter,
				target);
		err = -EPERM;
	}

	if (err == 0 && output->pull_intf.connect) {
		stm_data_interface_pull_sink_t tmp_sink;
		if (0 == stm_registry_get_attribute(target,
				STM_DATA_INTERFACE_PULL, type_tag,
				sizeof(stm_data_interface_pull_sink_t),
				&tmp_sink,
				&iface_size))
			output->pull_intf.mode = tmp_sink.mode;
	}

	pr_debug("interfaces: push.connect %p, pull.connect %p\n",
			output->push_intf.connect, output->pull_intf.connect);


	if (!err)
		output->external_connection = target;

	return err;
}

int te_out_filter_detach(struct te_obj *filter, stm_object_h target)
{
	int err;
	stm_object_h target_type;
	stm_object_h connected_obj = NULL;
	char type_tag[STM_REGISTRY_MAX_TAG_SIZE];
	uint32_t iface_size;
	struct te_out_filter *output = NULL;

	if (filter)
		output = te_out_filter_from_obj(filter);
	else
		return -EINVAL;

	if (output->external_connection != target) {
		pr_err("Output filter %p not attached to %p\n", filter, target);
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
				&output->pull_intf,
				&iface_size)) {
		pr_debug("detaching from pull sink %p\n", target);
		err = stm_registry_get_connection(te_obj_to_hdl(filter),
				STM_DATA_INTERFACE_PULL, &connected_obj);
		if (0 == err && connected_obj == target)
			err = te_out_filter_detach_from_pull_sink(filter,
					target);
	} else if (0 == stm_registry_get_attribute(target_type,
				STM_DATA_INTERFACE_PUSH, type_tag,
				sizeof(stm_data_interface_push_sink_t),
				&output->push_intf,
				&iface_size)) {
		pr_debug("attached to push sink %p\n", target);
		err = stm_registry_get_connection(te_obj_to_hdl(filter),
				STM_DATA_INTERFACE_PUSH, &connected_obj);
		if (0 == err && connected_obj == target)
			err = te_out_filter_detach_from_push_sink(output,
					target);
	} else {
		pr_err("Detach %p -> %p: no supported interfaces\n", filter,
				target);
		err = -EPERM;
	}

	if (!err) {
		/* Ensure output filter external connection is unset */
		output->external_connection = NULL;
	}

	return err;
}

/*!
 * \brief Gets the size of the data unit at the current head of the queue for a
 * output filter
 *
 * \param output output filter to discard data from
 * \param size   Returned size of the data unit at the current head of the queue
 *
 * Note that when the output filter is in bytestream read mode then the this
 * will return the size of all of the data in the queue. Conversely when the
 * filter is in quantisation unit read mode, then this will return the size of
 * the item at the head of the the queue.
 *
 * \retval 0 Success
 * \retval -EINTR Interrupted by signal
 */
int te_out_filter_queue_peek(struct te_out_filter *output, uint32_t *size)
{
	int err;
	struct te_queued_data *q;

	*size = 0;

	err = mutex_lock_interruptible(&output->queue_lock);
	if (err < 0)
		return err;

	if (output->read_quantisation_units) {
		if (!list_empty(&output->queued_data)) {
			q = list_first_entry(&output->queued_data,
					struct te_queued_data, lh);
			*size = q->size;
		}
	} else {
		*size = output->total_queued_data;
	}

	mutex_unlock(&output->queue_lock);
	return err;
}

/*!
 * \brief Gets the current packet count for an output filter
 *
 * \param filter Filter to check
 * \param count  Pointer set with current packet count (or 0 if the filter is
 *               not started)
 *
 * \retval 0    Success
 * \retval -EIO HAL error
 */
int te_out_filter_get_pkt_count(struct te_obj *filter, uint32_t *count)
{
	int err = 0;
	ST_ErrorCode_t hal_err;
	struct te_out_filter *output = te_out_filter_from_obj(filter);

	if (filter->state == TE_OBJ_STARTED && (NULL != output->hal_buffer)) {
		hal_err = stptiHAL_call(Buffer.HAL_BufferStatus,
				output->hal_buffer->hdl,
				NULL, NULL, count, NULL, NULL, NULL, NULL);
		if (ST_NO_ERROR != hal_err) {
			pr_err("HAL_BufferStatus return 0x%x\n", hal_err);
			err = te_hal_err_to_errno(hal_err);
		}
	} else {
		*count = 0;
	}

	return err;
}

/*!
 * \brief Gets the current output filter statistics information
 *
 * \param obj Filter to check
 * \param stat  Pointer to stat
 *
 * \retval 0    Success
 * \retval -EIO HAL error
 */
static int te_out_filter_get_stat(struct te_obj *obj,
				stm_te_output_filter_stats_t *stat)
{
	int err = 0;
	ST_ErrorCode_t hal_err;
	BOOL buffer_overflows = false;
	struct timespec ts;
	ktime_t ktime;

	struct te_out_filter *output = te_out_filter_from_obj(obj);

	memset(stat, 0, sizeof(*stat));

	if (obj->state == TE_OBJ_STARTED && (output->hal_buffer != NULL)) {
		stat->crc_errors = 0;

		hal_err = stptiHAL_call(Buffer.HAL_BufferStatus,
				output->hal_buffer->hdl,
				NULL, &stat->bytes_in_buffer,
				&stat->packet_count, NULL, NULL, NULL,
				&buffer_overflows);
		if (ST_NO_ERROR != hal_err) {
			pr_err("HAL_BufferStatus return 0x%x\n", hal_err);
			err = te_hal_err_to_errno(hal_err);
			goto exit;
		}

		stat->buffer_overflows = (uint32_t)buffer_overflows;

	}

exit:
	getrawmonotonic(&ts);
	ktime = timespec_to_ktime(ts);
	stat->system_time = ktime_to_us(ktime);
	return err;
}

/*!
 * \brief Gets the default HAL buffer parameters for a given output filter type
 *
 * \param type Type of buffer to get HAL parameters for
 * \param hal_buf_params     Returned HAL buffer parameters
 */
void te_out_filter_get_buffer_params(struct te_out_filter *output,
				 stptiHAL_BufferConfigParams_t *hal_buf_params)
{

	memset(hal_buf_params, 0, sizeof(stptiHAL_BufferConfigParams_t));

	hal_buf_params->ManuallyAllocatedBuffer = FALSE;
	hal_buf_params->PhysicalAddressSupplied = FALSE;
	hal_buf_params->BufferStart_p = NULL;

	if (output->buf_size == 0) {
		switch (output->obj.type) {
		case TE_OBJ_TS_FILTER:
			hal_buf_params->BufferSize = te_cfg.ts_buffer_size;
			break;

		case TE_OBJ_PES_FILTER:
			hal_buf_params->BufferSize = te_cfg.pes_buffer_size;
			break;

		case TE_OBJ_SECTION_FILTER:
			hal_buf_params->BufferSize = te_cfg.section_buffer_size;
			break;

		case TE_OBJ_PCR_FILTER:
			hal_buf_params->BufferSize = te_cfg.pcr_buffer_size;
			break;

		case TE_OBJ_TS_INDEX_FILTER:
			hal_buf_params->BufferSize = te_cfg.index_buffer_size;
			break;
		/* if user passes in an input filter give them some default
		 * buffer params */
		default:
			hal_buf_params->BufferSize = te_cfg.ts_buffer_size;
			break;
		}
	} else {
		/* use user defined size */
		hal_buf_params->BufferSize = output->buf_size;
	}
}

/*!
 * \brief Allocates a HAL buffer
 *
 * The size of the HAL buffer depends on the output filter type
 *
 * \param filter            TE output filter object to allocate buffers fro
 * \param hal_buffer_handle Returned HAL buffer handle
 * \param hal_buf_params    Returned HAL buffer parameters used to allocate the
 *                          HAL buffer
 *
 * \retval 0       Success
 * \retval -ENOMEM Failed to allocate HAL buffer
 */
int te_out_filter_buffer_alloc(struct te_obj *filter,
			struct te_hal_obj **hal_buffer,
			stptiHAL_BufferConfigParams_t *params)
{
	struct te_demux *demux = te_demux_from_obj(filter->parent);
	struct te_out_filter *output = te_out_filter_from_obj(filter);

	te_out_filter_get_buffer_params(output, params);

	/* Reset pointers for the new buffer */
	output->wr_offs = 0;
	output->rd_offs = 0;

	if (te_hal_obj_alloc(hal_buffer, demux->hal_session,
				OBJECT_TYPE_BUFFER, params)) {
		pr_err("couldn't allocate buffer\n");
		return -ENOMEM;
	}
	return 0;
}

/*!
 * \briefs Gets the appropriate HAL slot type for an output filter
 *
 * \param object_type     TE object type to map
 * \param stpti_slot_type Returned HAL slot type
 *
 * \retval 0       Success
 * \retval -EINVAL Bad parameter or the object type does not map to a TE HAL
 *                 slot type
 */
static int te_out_filter_to_stpti_slot_type(enum te_obj_type_e type,
		stptiHAL_SlotMode_t *stpti_slot_type)
{
	int result = 0;

	if (stpti_slot_type == NULL) {
		result = -EINVAL;
	} else {
		switch (type) {
		case TE_OBJ_SECTION_FILTER:
			*stpti_slot_type = stptiHAL_SLOT_TYPE_SECTION;
			break;
		case TE_OBJ_PCR_FILTER:
			*stpti_slot_type = stptiHAL_SLOT_TYPE_PCR;
			break;

		case TE_OBJ_PES_FILTER:
			*stpti_slot_type = stptiHAL_SLOT_TYPE_PES;
			break;

		case TE_OBJ_TS_FILTER:
			*stpti_slot_type = stptiHAL_SLOT_TYPE_RAW;
			break;

		default:
			/* does not map to a stpti slot type */
			result = -EINVAL;
			break;
		}
	}
	return result;
}

/*!
 * \brief Creates and associates the necessary HAL objects to connect a pid
 * filter to an output filter
 *
 * \param out_flt TE Output filter to attach pid_filt to
 * \param in_flt TE PID filter object to attach
 *
 * \retval 0       Success
 * \retval -EEXIST The objects are already attached
 * \retval -EINVAL The supplied objects cannot be attached
 * \retval -ENOMEM Failed to allocate the required HAL resource
 */
int te_out_filter_connect_input(struct te_obj *out_flt, struct te_obj *in_flt)
{
	ST_ErrorCode_t Error = ST_NO_ERROR;
	stptiHAL_SlotConfigParams_t slot_params;
	int result = 0;

	struct te_hal_obj *hal_slot = NULL, *hal_filter = NULL;

	struct te_in_filter *input = te_in_filter_from_obj(in_flt);
	struct te_out_filter *output = te_out_filter_from_obj(out_flt);
	struct te_demux *demux = te_demux_from_obj(out_flt->parent);

	memset(&slot_params, 0, sizeof(stptiHAL_SlotConfigParams_t));

	pr_debug("Pid Filter 0x%p Output Filter 0x%p\n", in_flt, out_flt);

	if (!te_in_filter_has_slot_space(input)) {
		pr_err("maximum slots reached\n");
		return -ENOMEM;
	}
	pr_debug("free slots available\n");

	if ((in_flt->type == TE_OBJ_PID_INS_FILTER
	   || in_flt->type == TE_OBJ_PID_REP_FILTER)
	   && out_flt->type != TE_OBJ_TS_FILTER) {
		pr_err("pid ins/rep filter can only be connected to ts output filter\n");
		return -EINVAL;
	}

	/* Find out the slot type required from the output filter object */
	switch (out_flt->type) {
	case TE_OBJ_DEMUX:
		pr_debug("connect to demux\n");
		return -EINVAL;
		break;
	case TE_OBJ_SECTION_FILTER:
		pr_debug("connect to section\n");
		return attach_pid_to_sf(in_flt, out_flt);
		break;
	case TE_OBJ_PES_FILTER:
		pr_debug("connect to PES\n");
		slot_params.SlotMode = stptiHAL_SLOT_TYPE_PES;
		slot_params.SoftwareCDFifo = TRUE;
		break;
	case TE_OBJ_PCR_FILTER:
		pr_debug("connect to PCR\n");
		slot_params.SlotMode = stptiHAL_SLOT_TYPE_PCR;
		break;
	case TE_OBJ_TS_INDEX_FILTER:
		pr_debug("connect to TS_INDEX\n");
		return attach_pid_to_index(in_flt, out_flt);
		break;
	case TE_OBJ_TS_FILTER:
	default:
		pr_debug("connect to TS (and everything else)\n");

		slot_params.SlotMode = stptiHAL_SLOT_TYPE_RAW;

		if (in_flt->type == TE_OBJ_PID_INS_FILTER)
			return te_ins_filter_attach_out(in_flt, out_flt);

		if (in_flt->type == TE_OBJ_PID_REP_FILTER)
			return te_rep_filter_attach_out(in_flt, out_flt);

		break;
	}

	/* Only create the output filter's buffer handle if required */
	if (output->hal_buffer == NULL) {

		stptiHAL_BufferConfigParams_t params;

		pr_debug("creating HAL buffer\n");

		/* Ok lets allocate a buffer */
		te_out_filter_get_buffer_params(output, &params);

		/* Reset pointers for the new buffer */
		output->wr_offs = 0;
		output->rd_offs = 0;

		result = te_hal_obj_alloc(&output->hal_buffer,
					demux->hal_session,
					OBJECT_TYPE_BUFFER, &params);
		if (result != 0) {
			pr_err("couldn't allocate buffer\n");
			goto error;
		}

		if (out_flt->type == TE_OBJ_PES_FILTER) {
			stptiHAL_call(Buffer.HAL_BufferSetThreshold,
				      output->hal_buffer->hdl,
				      3 * params.BufferSize / 4); /* 75% threshold */

		} else if (out_flt->type == TE_OBJ_PCR_FILTER) {
			stptiHAL_call(Buffer.HAL_BufferSetThreshold,
					      output->hal_buffer->hdl, 1);

		}
		stptiHAL_call(Buffer.HAL_BufferSetOverflowControl,
					      output->hal_buffer->hdl,
					      output->overflow_behaviour);

		/* The HAL returns the BufferStart in params.BufferStart_p */
		output->buf_start = params.BufferStart_p;
		output->buf_size = params.BufferSize;
	} else {
		/* Reuse existing buffer, but increment refcount */
		pr_debug("reusing HAL buffer\n");
		te_hal_obj_inc(output->hal_buffer);
	}

	pr_debug("Output filter PTI buffer handle 0x%x\n",
			output->hal_buffer->hdl.word);

	/* Allocate a slot and link it to the buffer */
	result = te_hal_obj_alloc(&hal_slot, demux->hal_session,
					OBJECT_TYPE_SLOT, &slot_params);

	if (result != 0) {
		pr_err("couldn't allocate slot\n");
		goto error;
	}

	/* Store it in the list */
	list_add(&hal_slot->entry, &input->slots);

	if (TE_OBJ_TS_FILTER == out_flt->type) {
		BOOL dlna = FALSE;
		pr_debug("connecting TS filter\n");
		if (te_ts_filter_is_security_formatted(out_flt))
			/* Output: If no local scramble (LS) set,
			 * records input stream. If LS set, records
			 * scrambled stream.  Scrambled data, remains
			 * scrambled.
			 */
			Error = stptiHAL_call(
					Slot.HAL_SlotSetSecurePathOutputNode,
					hal_slot->hdl,
					stptiHAL_SECUREPATH_OUTPUT_NODE_SCRAMBLED);
		else
			Error = stptiHAL_call(
					Slot.HAL_SlotSetSecurePathOutputNode,
					hal_slot->hdl,
					stptiHAL_SECUREPATH_OUTPUT_NODE_CLEAR);

		if (Error != ST_NO_ERROR) {
			pr_err("HAL_SlotSetSecurePathOutputNode error 0x%x\n",
					Error);
			result = te_hal_err_to_errno(Error);
			goto error;
		}

		if (te_ts_filter_is_dlna_formatted(out_flt))
			dlna = TRUE;

		Error = stptiHAL_call(Slot.HAL_SlotFeatureEnable,
				hal_slot->hdl,
				stptiHAL_SLOT_OUTPUT_DNLA_TS_TAG,
				dlna);

		if (ST_NO_ERROR != Error) {
			result = te_hal_err_to_errno(Error);
			goto error;
		}
	}

	if (TE_OBJ_PES_FILTER == out_flt->type) {
		uint8_t stream_id = te_pes_filter_get_stream_id(out_flt);

		pr_debug("connecting PES filter, stream_id %d\n", stream_id);
		if (stream_id) {
			if (!output->hal_filter) {
				stptiHAL_FilterConfigParams_t params = {
					stptiHAL_PES_STREAMID_FILTER
				};

				result = te_hal_obj_alloc(&hal_filter,
						demux->hal_session,
						OBJECT_TYPE_FILTER,
						&params);
				if (result != 0) {
					pr_err("couldn't allocate HAL filter\n");
					goto error;
				}
				output->hal_filter = hal_filter;
			} else {
				te_hal_obj_inc(output->hal_filter);
				hal_filter = output->hal_filter;
			}

			/* Associate the filter to the slot */
			Error = stptiOBJMAN_AssociateObjects(hal_filter->hdl,
								hal_slot->hdl);
			if (ST_NO_ERROR != Error) {
				pr_err("couldn't associate hal filter to slot\n");
				result = te_hal_err_to_errno(Error);
				goto error;
			}
			pr_debug("STPTI Filter Associated 0x%x to slot 0x%x\n",
					 hal_filter->hdl.word,
					 hal_slot->hdl.word);

			/* Call filterset and enable to activate the
			 * filter */
			Error = stptiHAL_call(Filter.HAL_FilterUpdate,
					hal_filter->hdl,
					stptiHAL_PES_STREAMID_FILTER,
					FALSE, FALSE, &stream_id,
					NULL, NULL);

			if (ST_NO_ERROR != Error) {
				pr_err("Error calling HAL_FilterUpdate Error 0x%x\n",
					Error);
				/* deallocate the objects - they will
				 * automatically break the links */
				result = te_hal_err_to_errno(Error);
				goto error;
			}

			Error = stptiHAL_call(Filter.HAL_FilterEnable, hal_filter->hdl, TRUE);
			if (Error != ST_NO_ERROR) {
				pr_err("Error calling HAL_FilterEnable Error 0x%x\n",
						(unsigned int)Error);
				result = te_hal_err_to_errno(Error);
				goto error;
			}
		}
	}

	if (input->path_id) {
		Error = stptiHAL_call(Slot.HAL_SlotSetSecurePathID,
				hal_slot->hdl, input->path_id);
		if (ST_NO_ERROR != Error) {
			result = te_hal_err_to_errno(Error);
			goto error;
		}
	}

	/* Initialise time points for TS_INDEX/PCR filters */
	if (TE_OBJ_PCR_FILTER == out_flt->type ||
			TE_OBJ_TS_INDEX_FILTER == out_flt->type) {
		result = te_obj_init_time(out_flt);
		if (result)
			goto error;
	}

	pr_debug
	    ("Pid filter slot handle 0x%x, buffer handle 0x%x\n",
	     hal_slot->hdl.word, output->hal_buffer->hdl.word);

	Error = stptiOBJMAN_AssociateObjects(hal_slot->hdl,
					     output->hal_buffer->hdl);

	if (ST_NO_ERROR != Error) {
		pr_err("stptiOBJMAN_AssociateObjects error 0x%x\n",
				Error);
		result = te_hal_err_to_errno(Error);
		goto error;
	}

	pr_debug("Calling HAL_SlotSetPID\n");

	Error = stptiHAL_call(
			Slot.HAL_SlotSetPID,
			hal_slot->hdl,
			input->pid,
			FALSE);

	if (Error != ST_NO_ERROR) {
		pr_err("HAL_SlotSetPID error 0x%x\n", Error);
		result = te_hal_err_to_errno(Error);
		goto error;
	}

	/* Register buffer workqueue function */
	if (output->external_connection != NULL) {
		result = te_bwq_register(demux->bwq, out_flt,
				output->hal_buffer,
				output->buffer_work_func);
		if (result != 0) {
			pr_err("unable to register buffer work func\n");
			goto error;
		}
	}

	return result;

error:
	if (output->hal_buffer) {
		if (te_hal_obj_dec(output->hal_buffer))
			output->hal_buffer = NULL;
	}

	if (hal_slot)
		te_hal_obj_dec(hal_slot);

	if (output->hal_filter) {
		if (te_hal_obj_dec(output->hal_filter))
			output->hal_filter = NULL;
	}

	return result;
}

/*!
 * \brief Destroys the HAL objects created when the given pid and output
 * filters were attached
 *
 * \param out_flt TE Output filter to detach pid_filter_p from
 * \param in_flt TE PID filter object to detach
 *
 * \retval 0       Success
 * \retval -EINVAL Detachment of the specified filters is not supported
 */
int te_out_filter_disconnect_input(struct te_obj *out_flt,
		struct te_obj *in_flt)
{
	ST_ErrorCode_t Error = ST_NO_ERROR;
	stptiHAL_SlotConfigParams_t params;
	int j, num_objects;
	FullHandle_t index;
	struct te_hal_obj *slot = NULL, *tmp = NULL;
	U16 hal_slot_mode = 0;
	int err = 0;

	struct te_out_filter *output = te_out_filter_from_obj(out_flt);
	struct te_in_filter *input = te_in_filter_from_obj(in_flt);

	/* validate functions arguments */
	if (in_flt == NULL || out_flt == NULL) {
		pr_err("input or output filter is NULL\n");
		err = -EINVAL;
		goto done;
	} else if (in_flt->type != TE_OBJ_PID_FILTER
		&& in_flt->type != TE_OBJ_PID_INS_FILTER
		&& in_flt->type != TE_OBJ_PID_REP_FILTER) {
		pr_err("input is not a PID filter\n");
		err = -EINVAL;
		goto done;
	}

	if (in_flt->type == TE_OBJ_PID_INS_FILTER)
		return te_ins_filter_detach_out(in_flt, out_flt);

	if (in_flt->type == TE_OBJ_PID_REP_FILTER)
		return te_rep_filter_detach_out(in_flt, out_flt);

	memset(&params, 0, sizeof(stptiHAL_SlotConfigParams_t));

	/* Check to see if there are any HAL objects that need disassociating.
	 * Where there is a 1:1 mapping between hal objects
	 * (slots with buffers filters & signals etc)
	 * the HAL objects can be deallocated after disassociating.
	 * However if more than one association exists then we need to check
	 * it is safe to deallocate the HAL objects */

	switch (out_flt->type) {

		/* Objects that do not use their own slot all other filters do
		 * */
	case TE_OBJ_TS_INDEX_FILTER:
		index = output->hal_index->hdl;

		/* Find the RAW slot we are indexing on */
		list_for_each_entry(slot, &input->slots, entry) {

			Error = stptiHAL_call(Slot.HAL_SlotGetMode,
				slot->hdl,
				&hal_slot_mode);

			if (ST_NO_ERROR == Error
				&& hal_slot_mode == stptiHAL_SLOT_TYPE_RAW) {
				stptiOBJMAN_DisassociateObjects(slot->hdl,
					output->hal_index->hdl);
				te_hal_obj_dec(slot);
				break;
			}
		}

		if (te_hal_obj_dec(output->hal_index)) {
			output->hal_index = NULL;
			output->obj.state = TE_OBJ_STOPPED;
		}

		break;

		/* Objects where >1:1 associations are allowed */
	case TE_OBJ_TS_FILTER:
	case TE_OBJ_SECTION_FILTER:
	case TE_OBJ_PES_FILTER:
	case TE_OBJ_PCR_FILTER:
		/* Find out the slot type from the output filter object */
		err = te_out_filter_to_stpti_slot_type(
							out_flt->type,
							&params.SlotMode);
		if (err != 0) {
			pr_err("unable to determine slot type\n");
			goto done;
		}

		if (out_flt->type == TE_OBJ_SECTION_FILTER) {
			te_section_filter_disconnect(out_flt, in_flt);
		} else {
			FullHandle_t *assoc_handles;
			assoc_handles = kmalloc(sizeof(FullHandle_t) *
					MAX_INT_CONNECTIONS, GFP_KERNEL);
			if (!assoc_handles) {
				pr_err("Failed to allocassoc handles array\n");
				err = -ENOMEM;
				goto done;
			}

			/* Get the list of slots attached to this buffer.
			 * Search the list until you find one that matches. */
			num_objects = stptiOBJMAN_ReturnAssociatedObjects(
					output->hal_buffer->hdl, assoc_handles,
					MAX_INT_CONNECTIONS, OBJECT_TYPE_SLOT);

			if (num_objects == 0) {
				pr_err("stptiOBJMAN_ReturnAssociatedObjects failed\n");
				kfree(assoc_handles);
				goto done;
			}

			list_for_each_entry_safe(slot, tmp, &input->slots, entry) {
				for (j = 0; j < num_objects; j++) {
					if (slot->hdl.word == assoc_handles[j].word) {
						te_hal_obj_dec(slot);
						break;
					}
				}
			}
			if (TE_OBJ_PID_FILTER == in_flt->type)
				te_pid_filter_update(in_flt);

			/* Delete HAL filter if it exists (e.g. PES stream id filter) */
			if (output->hal_filter) {
				if (te_hal_obj_dec(output->hal_filter)) {
					output->hal_filter = NULL;
				}
			}

			if (te_hal_obj_dec(output->hal_buffer)) {
				output->hal_buffer = NULL;
				output->obj.state = TE_OBJ_STOPPED;
			} else {
				/* Buffer still exists, check + flush */
				if (output->flushing_behaviour
					== STM_TE_FILTER_CONTROL_FLUSH_ON_DETACH) {
					output->flush(output);
				}
			}

			kfree(assoc_handles);
		}
		break;
	default:
		pr_err("unsupported output filter type %d\n", out_flt->type);
		err = -EINVAL;
		goto done;
	}

done:
	return err;
}

static int te_out_filter_pull_testfordata(stm_object_h filter_h, uint32_t *size)
{
	int err = 0;
	struct te_out_filter *output;
	int32_t sz;

	*size = 0;

	err = te_out_filter_from_hdl(filter_h, &output);
	if (err) {
		pr_err("Bad filter handle %p\n", filter_h);
		return err;
	}

	/* output->next_read_size can return a negative errno if interrupted by
	 * a signal*/
	sz = output->next_read_size(output);
	if (sz >= 0)
		*size = sz;
	else
		err = sz;

	return err;
}

static int te_out_filter_pull_data(stm_object_h filter_h,
		struct stm_data_block *data_block,
		uint32_t block_count,
		uint32_t *filled_blocks)
{
	int err = 0;
	struct te_out_filter *output;
	int32_t total_read = 0;
	int32_t available_size = 0;
	int32_t read_size = 0;
	int32_t req_size = 0;
	int32_t min_read_size;
	int i;
	struct te_demux *demux ;
	*filled_blocks = 0;

	/* Check handle */
	err = te_out_filter_from_hdl(filter_h, &output);
	if (err) {
		pr_err("Bad filter handle %p\n", filter_h);
		return err;
	}

	demux = te_demux_from_obj(output->obj.parent);
	if (demux == NULL)
		return -ENODEV;

	/* Determine total requested size */
	for (i = 0; i < block_count; i++)
		req_size += data_block[i].len;

	/* Check attachment status */
	if (!output->external_connection) {
		pr_warn("Pull request on detached filter %s\n",
				output->obj.name);
		return -EPERM;
	}

	/* If the output filter is in quantisation unit reading mode we will
	 * return at most one quantisation unit, so regardless of req_size, the
	 * minimum read size is 1 byte.
	 * In byte-stream mode the minimum read size is the size of the buffer.
	 * This means that blocking pull requests will block until the whole
	 * buffer is filled. */
	if (output->read_quantisation_units)
		min_read_size = 1;
	else
		min_read_size = req_size;

	/* If output->next_read_size returns an error, it has been interrupted
	 * and we should return now */
	available_size = output->next_read_size(output);
	if (available_size < 0)
		return available_size;

	/* If we cannot satisfy the pull_request now force pull_work to happen
	 * now, so we have an up-to-date view of the output filter's buffer */
	if (available_size < min_read_size && output->hal_buffer) {
		te_hal_obj_inc(output->hal_buffer);
		output->buffer_work_func(&output->obj, output->hal_buffer);
		te_hal_obj_dec(output->hal_buffer);
		available_size = output->next_read_size(output);
		if (available_size < 0)
			return available_size;
	}


	/* If this output filter has enough data to satisty the pull request or
	 * we are in non-blocking mode populate the buffer
	 *
	 * Otherwise this task must wait until enough data becomes available
	 * (or the output filter is disconnected)
	 */
	while (available_size < min_read_size &&
			!(output->pull_intf.mode & STM_IOMODE_NON_BLOCKING_IO))
	{
		int wait_err;

		pr_debug("Requested %d (minimum=%d), %d available. Blocking\n",
				req_size, min_read_size, available_size);
		wait_err = wait_event_interruptible(output->reader_waitq,
				output->next_read_size(output) >= min_read_size
				|| output->external_connection == NULL);
		if (wait_err) {
			pr_warn("Pull request on %s interrupted\n",
					output->obj.name);
			return wait_err;
		}

		/* Check attachment status */
		if (!output->external_connection) {
			pr_warn("%s disconnected. Aborting pull request\n",
					output->obj.name);
			return -EPERM;
		}

		available_size = output->next_read_size(output);
		if (available_size < 0)
			return available_size;
	}

	read_size = min_t(uint32_t, available_size, req_size);

	pr_debug("Reading %d bytes (%d requested, %d available)\n", read_size,
			req_size, available_size);

	while (!err && block_count > 0 && read_size > 0) {
		uint32_t bytes_read = 0;
		err = output->read_bytes(output, data_block->data_addr,
				min_t(uint32_t, data_block->len, read_size),
				&bytes_read);
		if (err || 0 == bytes_read)
			break;

		data_block->len = bytes_read;
		read_size -= bytes_read;
		(*filled_blocks)++;
		total_read += bytes_read;
		block_count--;
		data_block++;
	}

	if (demux->pacing_filter == &output->obj) {
		if (demux->push_notify.notify) {
			err = demux->push_notify.notify(
					demux->upstream_obj,
					STM_MEMSRC_EVENT_CONTINUE_INJECTION);
			if (unlikely(err)) {
				pr_err_ratelimited("Notify src %p failed(%d)\n",
						demux->upstream_obj,
						err);
			}
		}
	}

	return total_read;
}

stm_data_interface_pull_src_t stm_te_pull_byte_interface = {
	te_out_filter_pull_data,
	te_out_filter_pull_testfordata,
};

/*!
 * \brief Gets stm_te control data for an stm_te output filter object
 *
 * \param filter  output filter object to interrogate
 * \param control Control to get
 * \param value Set with the current value of the control
 *
 * \retval 0       Success
 * \retval -ENOSYS Invalid control coptrol for this output filter
 */
int te_out_filter_get_control(struct te_obj *filter, uint32_t control,
				void *buf, uint32_t size)
{
	int err = 0;
	struct te_out_filter *output = te_out_filter_from_obj(filter);

	switch (control) {
	case STM_TE_OUTPUT_FILTER_CONTROL_BUFFER_SIZE:
		err = GET_CONTROL(output->buf_size, buf, size);
		break;
	case STM_TE_OUTPUT_FILTER_CONTROL_OVERFLOW_BEHAVIOUR:
		err = GET_CONTROL(output->overflow_behaviour, buf, size);
		break;
	case STM_TE_OUTPUT_FILTER_CONTROL_ERROR_RECOVERY:
		err = GET_CONTROL(output->error_recovery, buf, size);
		break;
	case STM_TE_OUTPUT_FILTER_CONTROL_FLUSHING_BEHAVIOUR:
		err = GET_CONTROL(output->flushing_behaviour, buf, size);
		break;
	case STM_TE_OUTPUT_FILTER_CONTROL_READ_IN_QUANTISATION_UNITS:
		err = GET_CONTROL(output->read_quantisation_units, buf, size);
		break;
	case __deprecated_STM_TE_FILTER_CONTROL_STATUS:
	case STM_TE_OUTPUT_FILTER_CONTROL_STATUS:
		err = te_out_filter_get_stat(filter,
					(stm_te_output_filter_stats_t *) buf);
		break;
	default:
		err = -ENOSYS;
	}

	return err;
}

static int te_out_filter_set_overflow_control(struct te_out_filter *filter)
{
	ST_ErrorCode_t hal_err;
	int err = 0;

	/* Grab the object's lock to prevent other threads updating output
		buffer overflow control */
	err = mutex_lock_interruptible(&filter->obj.lock);
	if (err < 0)
		return err;

	/* Set the overflow control for HAL buffer */
	if (filter->hal_buffer) {
		te_hal_obj_inc(filter->hal_buffer);
		hal_err = stptiHAL_call(Buffer.HAL_BufferSetOverflowControl,
				filter->hal_buffer->hdl,
				filter->overflow_behaviour);
		if (ST_NO_ERROR != hal_err) {
			pr_err("HAL_BufferSetOverflowControl return 0x%x\n",
				hal_err);
			err = te_hal_err_to_errno(hal_err);
		}
		te_hal_obj_dec(filter->hal_buffer);
	} else {
		err = -ENOSYS;
		pr_err("There is no allocated hal_buffer 0x%x\n", err);
	}
	mutex_unlock(&filter->obj.lock);

	return err;
}

/*!
 * \brief Sets stm_te control data for an stm_te output filter object
 *
 * \param filter  output filter object to update
 * \param control Control to set
 * \param value New value of the control
 *
 * \retval 0       Success
 * \retval -ENOSYS Invalid control control for this output filter
 */
int te_out_filter_set_control(struct te_obj *filter, uint32_t control,
				const void *buf, uint32_t size)
{
	int err = 0;
	unsigned int val;
	struct  te_out_filter *output = te_out_filter_from_obj(filter);

	switch (control) {
	case STM_TE_OUTPUT_FILTER_CONTROL_BUFFER_SIZE:
		if (filter->state == TE_OBJ_STARTED) {
			pr_err("Unable to change buffer size, output filter is already attached\n");
			err = -EINVAL;
		} else if (buf && (*((int *)buf) < DVB_PAYLOAD_SIZE)) {
			pr_err("Buffer size should be atleast equal to DVB payload size i.e. 184 bytes\n");
			err = -EINVAL;
		} else
			err = SET_CONTROL(output->buf_size, buf, size);
		break;
	case STM_TE_OUTPUT_FILTER_CONTROL_READ_IN_QUANTISATION_UNITS:
		err = SET_CONTROL(val, buf, size);
		if (!err)
			output->read_quantisation_units = (val != 0);
		break;
	case STM_TE_OUTPUT_FILTER_CONTROL_OVERFLOW_BEHAVIOUR:
		err = SET_CONTROL(output->overflow_behaviour, buf, size);
		if (err == 0)
			err = te_out_filter_set_overflow_control(output);
		break;

	case STM_TE_OUTPUT_FILTER_CONTROL_ERROR_RECOVERY:
		/* Setting these values is currently unsupported */
		err = -ENOSYS;
		break;
	case STM_TE_OUTPUT_FILTER_CONTROL_FLUSHING_BEHAVIOUR:
		err = SET_CONTROL(output->flushing_behaviour, buf, size);
		break;
	case STM_TE_OUTPUT_FILTER_CONTROL_FLUSH:
		err = output->flush(output);
		break;
	case STM_TE_OUTPUT_FILTER_CONTROL_PAUSE:
		if (*(int *)buf == TE_PAUSE && filter->state ==
						TE_OBJ_STARTED) {
			filter->state = TE_OBJ_PAUSED;
		} else if (*(int *)buf == TE_RESUME && filter->state ==
						TE_OBJ_PAUSED) {
			filter->state = TE_OBJ_STARTED;
		} else {
			pr_err("Invalid set control request - buf val %d"
				" filter_state %d\n", *(int *)buf,
						filter->state);
			err = -EINVAL;
		}
		break;
	default:
		err = -ENOSYS;
	}

	return err;
}

static int te_out_filter_flush(struct te_out_filter *filter)
{
	ST_ErrorCode_t hal_err;
	int err = 0;

	/* Grab the object's lock to prevent other threads updating output
	 * rd/wr offset */
	err = mutex_lock_interruptible(&filter->obj.lock);
	if (err < 0)
		return err;

	/* Flush the HAL buffer */
	if (filter->hal_buffer) {
		te_hal_obj_inc(filter->hal_buffer);

		hal_err = stptiHAL_call(Buffer.HAL_BufferFlush,
				filter->hal_buffer->hdl);
		if (ST_NO_ERROR != hal_err) {
			pr_err("BufferFlush return 0x%x\n", hal_err);
			err = te_hal_err_to_errno(hal_err);
		}

		hal_err = stptiHAL_call(Buffer.HAL_BufferGetWriteOffset,
				filter->hal_buffer->hdl, &filter->wr_offs);
		if (ST_NO_ERROR != hal_err) {
			pr_err("BufferGetWriteOffset return 0x%x\n", hal_err);
			err = te_hal_err_to_errno(hal_err);
		}


		if (!err) {
			filter->rd_offs = filter->wr_offs;
			stptiHAL_call(Buffer.HAL_BufferSetReadOffset,
					filter->hal_buffer->hdl,
					filter->rd_offs);
		}

		te_hal_obj_dec(filter->hal_buffer);
	}

	mutex_unlock(&filter->obj.lock);

	return err;
}

/*!
 * \brief stm_te output filter object constructor
 *
 * Initialises a new output filter.r default values
 *
 * \param filter pointer to the output filter to initialise
 * \param demux  pointer to the TE obj of the parent demux for this output
 *               filter
 * \param name   String name for this output filter
 * \param type   Type of this output filter
 *
 * \retval 0       Success
 * \retval -ENOMEM No memory for new object
 */
int te_out_filter_init(struct te_out_filter *filter, struct te_obj *demux_obj,
		char *name, enum te_obj_type_e type)
{
	int err = 0;
	struct te_demux *demux;
	u_long flags = 0;

	demux = te_demux_from_obj(demux_obj);
	if (!demux) {
		pr_err("Bad demux handle\n");
		return -EINVAL;
	}

	err = te_obj_init(&filter->obj, type, name, demux_obj,
			&te_global.out_filter_class);
	if (err)
		goto error;

	/* Add output filter registry attributes */
	err = te_sysfs_entries_add(&filter->obj);
	if (err) {
		pr_err("Failed to add sysfs entries for filter %p %s\n", filter,
			name);
		err = -ENOMEM;
		goto error;
	}
	err = stm_registry_add_attribute(&filter->obj,
			TE_SYSFS_NAME_HAL_HDL "buffer",
			TE_SYSFS_TAG_HAL_HDL,
			&filter->hal_buffer,
			sizeof(struct te_hal_obj *));
	if (err) {
		pr_err("Failed to add HAL buffer handle to obj %s (%d)\n",
				name, err);
		goto error;
	}
	err = stm_registry_add_attribute(&filter->obj,
			TE_SYSFS_NAME_HAL_HDL "filter",
			TE_SYSFS_TAG_HAL_HDL,
			&filter->hal_filter,
			sizeof(struct te_hal_obj *));
	if (err) {
		pr_err("Failed to add HAL filter handle to obj %s (%d)\n",
				name, err);
		goto error;
	}
	err = stm_registry_add_attribute(&filter->obj,
			TE_SYSFS_NAME_HAL_HDL "index",
			TE_SYSFS_TAG_HAL_HDL,
			&filter->hal_index,
			sizeof(struct te_hal_obj *));
	if (err) {
		pr_err("Failed to add HAL index handle to obj %s (%d)\n",
				name, err);
		goto error;
	}

	/*
	 * Initialise output filter fields with non-zero defaults to default
	 * values
	 */
	filter->flushing_behaviour  = TE_DEFAULT_OUTPUT_FILTER_FLUSHING;
	filter->overflow_behaviour = TE_DEFAULT_OUTPUT_FILTER_OVERFLOW;
	filter->buf_size = 0;

	/* Set output filter pull src interface */
	filter->pull_src_intf = &stm_te_pull_byte_interface;

	/* Set default read_bytes and next_read_size functions
	 * Sub-classes of output filter may override these */
	filter->read_bytes = &te_out_filter_read_bytes;
	filter->next_read_size = &te_out_filter_next_read_size;

	/* Set default buffer work function */
	filter->buffer_work_func = &te_out_filter_work;

	INIT_LIST_HEAD(&filter->queued_data);
	filter->total_queued_data = 0;
	filter->max_queued_data = te_cfg.max_queued_output_data;
	mutex_init(&filter->queue_lock);

	init_waitqueue_head(&filter->reader_waitq);

	write_lock_irqsave(&demux->out_flt_rw_lock, flags);
	list_add_tail(&filter->obj.lh, &demux->out_filters);
	write_unlock_irqrestore(&demux->out_flt_rw_lock, flags);

	/* Set default object functions */
	filter->flush = &te_out_filter_flush;

	return 0;

error:
	te_obj_deinit(&filter->obj);
	return err;
}

int te_out_filter_deinit(struct te_out_filter *filter)
{
	int err;
	struct te_demux *dmx;
	u_long flags = 0;

	err = te_sysfs_entries_remove(&filter->obj);
	if (err)
		pr_warn("Failed to remove sysfs entries\n");

	err = te_obj_deinit(&filter->obj);
	if (err)
		return err;

	/* Delete any queued data */
	te_out_filter_queue_empty(filter);
	mutex_destroy(&filter->queue_lock);
	dmx = te_demux_from_obj(filter->obj.parent);
	write_lock_irqsave(&dmx->out_flt_rw_lock, flags);
	list_del(&filter->obj.lh);
	write_unlock_irqrestore(&dmx->out_flt_rw_lock, flags);

	/* If this output filter is the pacing filter for this demux, unset the
	 * pacing filter */
	if (dmx && dmx->pacing_filter == &filter->obj) {
		pr_info("Demux %s pacing filter %s (%p) was deleted\n",
				dmx->obj.name, filter->obj.name, &filter->obj);
		dmx->pacing_filter = NULL;
	}

	return 0;
}

/*!
 * \brief object to output filter cast
 */
struct te_out_filter *te_out_filter_from_obj(struct te_obj *filter)
{
	return container_of(filter, struct te_out_filter, obj);
}

/*!
 * \brief Adds a single data unit to the tail of the queued data for this
 * output filter
 *
 * \param output output filter to discard data from
 * \param data   Data buffer to queue (will be copied by this function)
 * \param size   Size of data
 * \param bool   Validity flag stored with data. Used for recording section crc
 *               validity
 *
 * \retval 0       Success
 * \retval -ENOMEM Allocation failed, or too much data already stored for this
 *                 object
 * \retval -EINTR  Interrupted by signal
 */
int te_out_filter_queue_data(struct te_out_filter *output, uint8_t *data,
		uint32_t size, bool valid)
{
	int err;
	struct te_queued_data *q_data;

	err = mutex_lock_interruptible(&output->queue_lock);
	if (err < 0)
		return err;

	if (output->total_queued_data + size >
			output->max_queued_data) {
		pr_warn_ratelimited("Obj %p data queue overrun\n",
				&output->obj);
		err = -ENOMEM;
		goto out;
	}

	q_data = kmalloc(sizeof(*q_data) + size, GFP_KERNEL);
	if (!q_data) {
		err = -ENOMEM;
		goto out;
	}

	q_data->data = (uint8_t *)&q_data[1];
	q_data->size = size;
	q_data->valid = valid;
	memcpy(q_data->data, data, size);

	list_add_tail(&q_data->lh, &output->queued_data);
	output->total_queued_data += size;
	output->lifetime_queued_data += size;

out:
	mutex_unlock(&output->queue_lock);
	return err;
}

/*!
 * \brief Reads data from the head of the queued data for an output filter
 *
 * \remark When the output filter is in quantisation unit read mode this
 * function will either read one complete data unit or as much of that data
 * unit as will fit in the supplied buffer (whatever is smallest)
 *
 * \remark When the output filter is in bytestream read mode this function will
 * read as much data from the queue aswill fit in the supplied buffer
 *
 * \param output      output filter to discard data from
 * \param buffer      Buffer to read the data into
 * \param buffer_size Size of buffer
 * \param bytes_read  Returned size of data read
 * \param valid       Whether the data returned was marked as valid (normally
 *                    refers to section crc check). In bytestream mode this
 *                    indicates that ALL the data read came from valid
 *                    packets/sections.
 *
 * \retval 0      Success
 * \retval -EINTR Interrupted by signal
 */
int te_out_filter_queue_read(struct te_out_filter *output, uint8_t *buffer,
		uint32_t buffer_size, uint32_t *bytes_read, bool *valid)
{
	int err;
	struct te_queued_data *q, *q2;

	*valid = true;
	*bytes_read = 0;

	err = mutex_lock_interruptible(&output->queue_lock);
	if (err < 0)
		return err;

	if (output->read_quantisation_units) {
		if (list_empty(&output->queued_data))
			goto out;

		q = list_first_entry(&output->queued_data,
				struct te_queued_data, lh);

		*bytes_read = min(buffer_size, q->size);
		*valid = q->valid;
		memcpy(buffer, q->data, *bytes_read);

		q->size -= *bytes_read;
		q->data += *bytes_read;
		output->total_queued_data -= *bytes_read;
		if (q->size <= 0) {
			list_del(&q->lh);
			kfree(q);
		}
	} else {
		list_for_each_entry_safe(q, q2, &output->queued_data, lh) {
			uint32_t bytes_this_iter;

			/* Read data from this queue entry into read buffer */
			bytes_this_iter = min(buffer_size, q->size);
			*valid &= q->valid;
			memcpy(buffer, q->data, bytes_this_iter);

			/* Update queue entry. Discard this queue entry if all
			 * data has been read from it */
			q->size -= bytes_this_iter;
			q->data += bytes_this_iter;
			output->total_queued_data -= bytes_this_iter;
			if (q->size <= 0) {
				list_del(&q->lh);
				kfree(q);
			}

			/* Update read buffer pointer */
			buffer += bytes_this_iter;
			*bytes_read += bytes_this_iter;
			buffer_size -= bytes_this_iter;
			if (buffer_size <= 0)
				break;
		}
	}
out:
	mutex_unlock(&output->queue_lock);
	return err;
}

/*!
 * \brief Discards any queued data for this output filter
 *
 * \param output  output filter to discard data from
 *
 * \retval 0      Success
 * \retval -EINTR Interrupted by signal
 */
int te_out_filter_queue_empty(struct te_out_filter *output)
{
	struct te_queued_data *q, *q2;
	int err;

	err = mutex_lock_interruptible(&output->queue_lock);
	if (err)
		return err;

	list_for_each_entry_safe(q, q2, &output->queued_data,
			lh) {
		list_del(&q->lh);
		kfree(q);
	}
	output->total_queued_data = 0;
	mutex_unlock(&output->queue_lock);

	return err;
}

/*!
 * \brief Wait until the specified output filter's buffer has at least 25%
 * space
 * \param obj TE obj for output filter to wait upon
 *
 * \retval 0       Success
 * \retval -EINVAL Output filter is not valid
 * \retval -EIO    HAL error
 * \retval -EINTR  Interrupted by signal
 * \retval -EWOULDBLOCK Output filter is not started or has not connections
 */
int te_out_filter_wait(struct te_obj *obj)
{
	int err = 0;
	struct te_out_filter *filter = te_out_filter_from_obj(obj);
	ST_ErrorCode_t hal_err;
	struct te_hal_obj *hal_buffer = NULL;
	uint32_t buffer_size, bytes_in_buffer, units_in_buffer, free_space;
	struct te_demux *dmx;

	buffer_size = bytes_in_buffer = units_in_buffer = free_space = 0;

	/* Check if the pacing filter is valid whilst holding the global lock.
	 * If it is, retrieve a reference to the HAL buffer handle */
	err = mutex_lock_interruptible(&te_global.lock);
	if (err < 0)
		return err;

	err = te_valid_hdl(te_obj_to_hdl(obj));
	if (err) {
		pr_warn("Pacing filter handle %p is invalid\n", obj);
	} else if (!filter->hal_buffer) {
		pr_warn("Pacing filter %s is not started\n", obj->name);
		err = -EINVAL;
	} else if (!filter->external_connection) {
		pr_warn("Pacing filter %s has no downstream connections\n",
				obj->name);
		err = -EINVAL;
	} else {
		te_hal_obj_inc(filter->hal_buffer);
		hal_buffer = filter->hal_buffer;
	}
	mutex_unlock(&te_global.lock);

	if (err)
		return err;

	dmx = te_demux_from_obj(obj->parent);
	if (!dmx)
		return -EINVAL;

	/* If we got here without error, we have a valid HAL buffer to wait on */
	while (!err) {
		hal_err = stptiHAL_call(Buffer.HAL_BufferStatus,
				hal_buffer->hdl,
				&buffer_size, &bytes_in_buffer,
				&units_in_buffer, &free_space, NULL, NULL, NULL);
		if (ST_NO_ERROR != hal_err) {
			pr_err("HAL_BufferStatus return 0x%x\n", hal_err);
			err = te_hal_err_to_errno(hal_err);
			break;
		}

		/* If buffer is < 75% full break, otherwise wait */
		if (((bytes_in_buffer*100) / buffer_size) < 75)
			break;

		/* Wait for buffer to become empty */
		set_current_state(TASK_INTERRUPTIBLE);
		if (signal_pending(current) ||
				dmx->disconnect_pending ||
				schedule_timeout(msecs_to_jiffies(30))) {
			err = -EINTR;
			pr_info("TE inject pacing interrupted\n");
		}
	}
	if (hal_buffer)
		if (te_hal_obj_dec(hal_buffer)) {
			/* If this was the last reference to the HAL buffer, we
			 * need to check if the output filter still exists and
			 * if so we can set the HAL buffer pointer to NULL */
			err = mutex_lock_interruptible(&te_global.lock);
			if (err < 0)
				return err;
			if (0 == te_valid_hdl(te_obj_to_hdl(obj)) &&
					filter->hal_buffer == hal_buffer)
				filter->hal_buffer = NULL;

			mutex_unlock(&te_global.lock);
		}

	return err;
}

ST_ErrorCode_t te_out_filter_memcpy(void **dest_pp, const void *src_p,
		size_t size)
{
	/* Note: dest_pp is not a void *, but a void ** */
	memcpy(*dest_pp, src_p, size);
	*dest_pp = (void *)((uint8_t *)(*dest_pp) + size);
	return ST_NO_ERROR;
}

int te_out_filter_get_sizes(struct te_obj *obj, unsigned int *total_size,
				unsigned int *bytes_in_buffer,
				unsigned int *free_space)
{
	ST_ErrorCode_t hal_err;
	struct te_out_filter *filter = te_out_filter_from_obj(obj);
	struct te_hal_obj *hal_buffer = NULL;
	uint32_t units_in_buffer;
	int err;

	err = mutex_lock_interruptible(&te_global.lock);
	if (err < 0)
		return err;

	if (filter->hal_buffer == NULL) {
		pr_warn("Pacing filter %s is not started\n", obj->name);
		goto err_unlk;
	}

	if (!filter->external_connection) {
		pr_warn("Pacing filter %s has no downstream connections\n",
				obj->name);
		goto err_unlk;
	}

	te_hal_obj_inc(filter->hal_buffer);
	hal_buffer = filter->hal_buffer;

	mutex_unlock(&te_global.lock);

	hal_err = stptiHAL_call(Buffer.HAL_BufferStatus,
			hal_buffer->hdl,
			total_size, bytes_in_buffer,
			&units_in_buffer, free_space, NULL, NULL, NULL);

	te_hal_obj_dec(hal_buffer);

	if (ST_NO_ERROR != hal_err) {
		pr_err("HAL_BufferStatus return 0x%x\n", hal_err);
		return -EINVAL;
	}

	return 0;

err_unlk:
	mutex_unlock(&te_global.lock);
	return -EINVAL;
}

int te_out_filter_send_event(struct te_out_filter *output,
		stm_te_filter_event_t event)
{
	int ret = 0;
	stm_event_t evt;

	evt.event_id = event;
	evt.object = &output->obj;

	ret = stm_event_signal(&evt);
	if (ret)
		pr_err("OutputFilter:0x%p failed(%d) to signal event(0x%x)\n",
				output, ret, event);
	else
		pr_debug("OutputFilter:0x%p event(0x%x) signalled\n",
				output, event);
	return ret;
}


int te_out_filter_notify_pull_sink(struct te_out_filter *output,
				stm_memsink_event_t event)
{
	int ret = 0;

	if (!output->external_connection) {
		pr_err("Output filter 0x%p has no downstream connection\n",
			output);
		ret = -EINVAL;
		goto skip_event;
	}

	if (!output->pull_intf.notify) {
		pr_err("Output filter 0x%p does not have notify interface\n",
			output);
		ret = -EINVAL;
		goto skip_event;
	}

	ret = output->pull_intf.notify(output->external_connection, event);
	if (ret)
		pr_err_ratelimited("Notify sink %p failed (%d)\n",
				output->external_connection, ret);

skip_event:
	return ret;
}
