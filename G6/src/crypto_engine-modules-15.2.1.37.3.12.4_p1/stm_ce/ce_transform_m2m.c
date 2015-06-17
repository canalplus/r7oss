/************************************************************************
Copyright (C) 2011,2012 STMicroelectronics. All Rights Reserved.

This file is part of the Crypto_engine Library.

Crypto_engine is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License version 2 as published by the
Free Software Foundation.

Crypto_engine is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with Crypto_engine; see the file COPYING.  If not, write to the Free Software
Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

The Crypto_engine Library may alternatively be licensed under a proprietary
license from ST.

Source file name : ce_transform_m2m.c

Implements mem2mem functions for an stm_ce transform object
************************************************************************/
#include "stm_ce.h"
#include "stm_ce_objects.h"
#include "ce_hal.h"
#include "stm_ce_osal.h"
#include "stm_registry.h"

#define CE_M2M_MAX_TRANSFER (PAGE_SIZE * 64)

#define CE_TASK_TIMEOUT_MS 100

static int thread_dmx_cepush[2] = { SCHED_NORMAL, 0 };
module_param_array_named(thread_DMX_Cepush, thread_dmx_cepush, int, NULL, 0644);
MODULE_PARM_DESC(thread_DMX_Cepush, "DMX-CePush thread:s(Mode),p(Priority)");

static unsigned int ce_m2m_buf_size = CE_M2M_MAX_TRANSFER;
module_param(ce_m2m_buf_size, int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(ce_m2m_buf_size, "CE M2M internal buffer size");

static stm_data_interface_pull_src_t ce_data_pull_src_interface;

static int ce_push_task(void *params)
{
	struct ce_transform *tfm = (struct ce_transform *)params;
	stm_ce_buffer_t ce_dest_buffer;
	struct stm_data_block blk;
	uint32_t blks_sent;
	int err = 0;

	set_freezable();

	CE_INFO("Transform %p: Starting push task\n", tfm);

	tfm->mem.dest_buf = &ce_dest_buffer;
	tfm->mem.dest_count = 1;
	tfm->mem.dest_buf->size = tfm->mem.push_buffer_size;
	tfm->mem.dest_total_size = tfm->mem.push_buffer_size;

	/* Prepare push buffer for DMA */
	tfm->mem.dest_buf->data = (uint8_t *)dma_map_single(NULL,
						tfm->mem.push_buffer,
						tfm->mem.push_buffer_size,
						DMA_FROM_DEVICE);

	complete(&tfm->mem.dest_writeable);

	while (!kthread_should_stop()) {

		unsigned long ret;

		/* Wait for buffer to become readable */
		ret = wait_for_completion_interruptible_timeout(
				  &tfm->mem.dest_readable,
				  msecs_to_jiffies(CE_TASK_TIMEOUT_MS));
		if (ret <= 0) {
			try_to_freeze();
			continue;
		}
		dma_unmap_single(NULL, (dma_addr_t)tfm->mem.dest_buf->data,
					tfm->mem.dest_buf->size,
					DMA_FROM_DEVICE);

		/* Push to downstream consumer */
		blk.data_addr = tfm->mem.push_buffer;
		blk.len = tfm->mem.bytes_written;
		blk.next = NULL;
		CE_INFO("Tfm %p: Pushing %u bytes to sink %p\n", tfm,
				tfm->mem.bytes_written, tfm->mem.sink);
		err = (*tfm->mem.sink_u.push.push_data)(tfm->mem.sink, &blk,
							1, &blks_sent);

		/* Prepare push buffer ready for next DMA */
		tfm->mem.dest_buf->data = (uint8_t *)dma_map_single(NULL,
							tfm->mem.push_buffer,
				tfm->mem.push_buffer_size, DMA_FROM_DEVICE);

		complete(&tfm->mem.dest_writeable);

		try_to_freeze();
	}
	CE_INFO("Transform %p: Push task ending\n", tfm);

	dma_unmap_single(NULL, (dma_addr_t)tfm->mem.dest_buf->data,
				tfm->mem.dest_buf->size, DMA_FROM_DEVICE);

	return 0;
}

static int ce_push_sink_connect
(stm_object_h src_object, stm_object_h sink_object)
{
	struct ce_transform *tfm;
	int err = 0;

	CE_ENTRY("src=%p sink=%p\n", src_object, sink_object);

	/* Get transform object (checks validity of handle) */
	if (unlikely(ce_transform_from_handle(sink_object, &tfm)))
		return -EINVAL;

	/* Check this is a memory transform */
	if (unlikely((STM_CE_TRANSFORM_TYPE_MEMORY != tfm->type) &&
			(STM_CE_TRANSFORM_TYPE_MEMORY_CLEAR != tfm->type))) {
		CE_ERROR("Transform %p is not configured for m2m\n", tfm);
		return -EINVAL;
	}

	/* Already connected? */
	if (unlikely(tfm->mem.src)) {
		CE_ERROR("Transform %p already attached\n", tfm);
		return -EBUSY;
	}

	tfm->mem.src = src_object;

	CE_EXIT("%d\n", err);
	return err;
}

static int ce_push_sink_disconnect(stm_object_h src_object,
					stm_object_h sink_object)
{
	int err;
	struct ce_transform *tfm;

	CE_ENTRY("src=%p sink=%p\n", src_object, sink_object);

	/* Get transform object (checks validity of handle) */
	if (unlikely(ce_transform_from_handle(sink_object, &tfm)))
		return -EINVAL;

	/* Check this is a memory transform */
	if (unlikely((STM_CE_TRANSFORM_TYPE_MEMORY != tfm->type) &&
			(STM_CE_TRANSFORM_TYPE_MEMORY_CLEAR != tfm->type))) {
		CE_ERROR("Transform %p is not configured for m2m\n", tfm);
		err = -EINVAL;
	}

	if (unlikely(tfm->mem.src != src_object)) {
		CE_ERROR("Src %p has not been attached to transform %p\n",
				 src_object, tfm);
		return -ENODEV;
	}

	tfm->mem.src = NULL;

	CE_EXIT();
	return 0;
}

static int ce_push_data(stm_object_h sink_object,
			struct stm_data_block *block_list,
			uint32_t block_count,
			uint32_t *data_blocks)
{
	struct ce_transform *tfm;
	int err = 0;
	stm_ce_buffer_t *ce_buffer = NULL;
	uint32_t cur_block = 0;
	uint32_t offset = 0;
	uint32_t remaining_data = 0;
	int i;

	CE_ENTRY("transform=%p\n", sink_object);

	/* Get transform object (checks validity of handle) */
	if (unlikely(ce_transform_from_handle(sink_object, &tfm)))
		return -EINVAL;

	/* Check this is a memory transform */
	if (unlikely((STM_CE_TRANSFORM_TYPE_MEMORY != tfm->type) &&
			(STM_CE_TRANSFORM_TYPE_MEMORY_CLEAR != tfm->type))) {
		CE_ERROR("Transform %p is not configured for m2m\n",
				 tfm);
		return -EINVAL;
	}

	/* Allocate stm_ce_buffer list (for translating block_list) */
	ce_buffer = OS_malloc(sizeof(stm_ce_buffer_t) * block_count);
	if (unlikely(!ce_buffer))
		return -ENOMEM;

	/* Compute the amount of data to be sent */
	for (i = 0; i < block_count; i++)
		remaining_data += block_list[i].len;

	OS_rlock(tfm->lock);

	/* Currently, since there is no data tagging, we enforce in-order
	 * execution of DMA requests by only allowing one writer at a time */
	if (mutex_lock_interruptible(&tfm->mem.wlock)) {
		err = -ERESTARTSYS;
		goto clean;
	}

	tfm->mem.bytes_available = remaining_data;

	while (remaining_data && !err) {
		uint32_t dma_size = 0, dma_remain = 0;

		/* Wait for destination to be ready */
		if (wait_for_completion_interruptible(
				&tfm->mem.dest_writeable)) {
			err = -ERESTARTSYS;
			goto clean;
		}

		/* Compute the target dma size */
		dma_size = min(remaining_data, tfm->mem.dest_total_size);

		dma_remain = dma_size;

		/* Create a ce_buffer list with data to be computed */
		for (i = 0; dma_remain && (i < block_count - cur_block); i++) {
			/* Calculate the amount of data to be processed from
			 * this block */
			ce_buffer[i].size = min(block_list[cur_block + i].len -
						offset, dma_remain);
			dma_remain -= ce_buffer[i].size;

			/* Cache flush the piece of data to be processed */
			ce_buffer[i].data = (uint8_t *)dma_map_single(NULL,
				block_list[cur_block + i].data_addr + offset,
				block_list[cur_block + i].len - offset,
				DMA_TO_DEVICE);

			/* Update the offset pointer and reset if it reach the
			 * end of the block */
			offset += ce_buffer[i].size;
			if (offset == block_list[cur_block + i].len)
				offset = 0;
		}

		CE_INFO("Tfm %p: Starting DMA\n", tfm);

		/* Call the internal dma api */
		err = (*tfm->api->dma)(tfm, ce_buffer, i,
					tfm->mem.dest_buf,
					tfm->mem.dest_count);

		tfm->mem.bytes_written = dma_size;
		CE_INFO("Tfm %p: DMA sent %d bytes (%d blocks)to receiver\n"
				, tfm, dma_size, i);

		cur_block += (i - 1);

		while (i > 0) {
			dma_unmap_single(NULL, (dma_addr_t)ce_buffer[i-1].data,
						ce_buffer[i-1].size,
						DMA_TO_DEVICE);
			i--;
		}

		remaining_data -= dma_size;

		complete(&tfm->mem.dest_readable);
	}

	/* Return the number of processed block */
	*data_blocks = cur_block + 1;

clean:
	mutex_unlock(&tfm->mem.wlock);
	OS_runlock(tfm->lock);

	OS_free(ce_buffer);
	CE_EXIT("%d\n", err);
	return err;
}


static int ce_pull_src_data(stm_object_h src_object,
				struct stm_data_block *block_list,
				uint32_t block_count,
				uint32_t *data_blocks_filled)
{
	struct ce_transform *tfm;
	stm_ce_buffer_t *ce_buf = NULL;
	int ret = 0;
	int i;
	uint32_t dest_fill_size = 0;

	CE_ENTRY("transform=%p\n", src_object);

	*data_blocks_filled = 0;

	/* Get transform object (checks validity of handle) */
	if (unlikely(ce_transform_from_handle(src_object, &tfm)))
		return -EINVAL;

	/* Check this is a memory transform */
	if (unlikely((STM_CE_TRANSFORM_TYPE_MEMORY != tfm->type) &&
			(STM_CE_TRANSFORM_TYPE_MEMORY_CLEAR != tfm->type))) {
		CE_ERROR("Transform %p is not configured for m2m\n",
				 tfm);
		return -EINVAL;
	}

	/* Convert block list into array of ce buffers, pointing to physical
	 * addresses */
	ce_buf = OS_malloc(block_count * sizeof(stm_ce_buffer_t));
	if (unlikely(!ce_buf))
		return -ENOMEM;

	tfm->mem.dest_total_size = 0;
	for (i = 0; i < block_count; i++) {
		ce_buf[i].data = (uint8_t *)dma_map_single(NULL,
							block_list[i].data_addr,
							block_list[i].len,
							DMA_FROM_DEVICE);
		ce_buf[i].size = block_list[i].len;
		tfm->mem.dest_total_size += block_list[i].len;
	}

	/* Indicate destination is ready for DMA */
	tfm->mem.dest_buf = ce_buf;
	tfm->mem.dest_count = block_count;
	complete(&tfm->mem.dest_writeable);

	if (wait_for_completion_interruptible(&tfm->mem.dest_readable)) {
		ret = -ERESTARTSYS;
		goto clean;
	}

	for (i = 0; i < block_count; i++) {
		dma_unmap_single(NULL, (dma_addr_t)ce_buf[i].data,
						  ce_buf[i].size,
						DMA_FROM_DEVICE);

		/* Determine the amount of block filled */
		if (dest_fill_size < tfm->mem.bytes_written)
			(*data_blocks_filled)++;

		dest_fill_size += ce_buf[i].size;
	}
	ret = tfm->mem.bytes_written;
	tfm->mem.bytes_available -= tfm->mem.bytes_written;


	CE_INFO("Tfm %p: Pulled %u blocks (%u bytes)\n", tfm,
			*data_blocks_filled, tfm->mem.bytes_written);

clean:
	OS_free(ce_buf);
	return ret;
}

static int ce_pull_src_test_for_data(stm_object_h src_object, uint32_t *size)
{

	struct ce_transform *tfm;
	int ret = 0;
	*size = 0;

	/* Get transform object (checks validity of handle) */
	if (unlikely(ce_transform_from_handle(src_object, &tfm)))
		return -EINVAL;

	/* Check this is a memory transform */
	if (unlikely(STM_CE_TRANSFORM_TYPE_MEMORY != tfm->type)) {
		CE_ERROR("Transform %p is not configured for m2m\n",
				 tfm);
		return -EINVAL;
	}

	*size = tfm->mem.bytes_available;

	return ret;

}

int ce_transform_attach_pull_sink(struct ce_transform *tfm, stm_object_h sink)
{
	int err;
	stm_object_h sink_type;
	uint32_t size;

	/* Look for a data push interface on the sink object */
	err = stm_registry_get_object_type(sink, &sink_type);
	if (unlikely(err)) {
		CE_ERROR("stm_registry_get_object_type return %d obj=%p\n",
				 err, sink);
		return -ENODEV;
	}
	err = stm_registry_get_attribute(sink_type, STM_DATA_INTERFACE_PULL,
						STM_REGISTRY_ADDRESS,
						sizeof(tfm->mem.sink_u.pull),
						 &tfm->mem.sink_u.pull, &size);
	if (unlikely(err))
		return -ENODEV;

	/* Call sink's connect function */
	err = tfm->mem.sink_u.pull.connect(tfm, sink,
					&ce_data_pull_src_interface);
	if (unlikely(err)) {
		CE_ERROR("Connect transform=%p to pull sink %p, err=%d\n",
				 tfm, sink, err);
		return err;
	}

	err = stm_registry_add_connection(tfm, "pull-sink", sink);
	if (unlikely(err)) {
		CE_ERROR("stm_registry_add_connection %p->%p, err %d\n", tfm,
				 sink, err);
	} else {
		tfm->mem.sink = sink;
		CE_INFO("Transform %p attached to pull sink %p\n", tfm, sink);
	}

	return err;
}

int ce_transform_detach_pull_sink(struct ce_transform *tfm, stm_object_h sink)
{
	int err;

	err = stm_registry_remove_connection(tfm, "pull-sink");
	if (unlikely(err))
		return err;

	err = tfm->mem.sink_u.pull.disconnect(tfm, sink);
	if (unlikely(err)) {
		CE_ERROR("Transform %p, sink %p disconnect, err %d\n", tfm,
				 sink, err);
	}

	tfm->mem.sink = NULL;
	memset(&tfm->mem.sink_u.pull, 0, sizeof(tfm->mem.sink_u.pull));

	return err;
}

int ce_transform_attach_push_sink(struct ce_transform *tfm, stm_object_h sink)
{
	int err;
	stm_object_h sink_type;
	uint32_t size = 0;
	struct sched_param param;

	/* Look for a data push interface on the sink object */
	err = stm_registry_get_object_type(sink, &sink_type);
	if (unlikely(err)) {
		CE_ERROR("stm_registry_get_object_type return %d obj=%p\n",
				 err, sink);
		return -ENODEV;
	}
	err = stm_registry_get_attribute(sink_type, STM_DATA_INTERFACE_PUSH,
						STM_REGISTRY_ADDRESS,
						sizeof(tfm->mem.sink_u.push),
						&tfm->mem.sink_u.push, &size);
	if (unlikely(err))
		return -ENODEV;

	CE_INFO("sink %p has push interface, size=%d\n", sink, size);


	/* Call sink's connect function */
	err = tfm->mem.sink_u.push.connect(tfm, sink);
	if (unlikely(err)) {
		CE_ERROR("Connect transform=%p to push sink %p, err=%d\n",
				 tfm, sink, err);
		return err;
	}

	/* Allocate push sink buffer */
	tfm->mem.push_buffer = kmalloc(ce_m2m_buf_size, GFP_KERNEL | GFP_DMA);
	if (unlikely(!tfm->mem.push_buffer)) {
		CE_ERROR("Transform %p failed to allocate DMA buffer "\
				"(sz=%u)\n",
				 tfm, ce_m2m_buf_size);
		return -ENOMEM;
	}
	tfm->mem.push_buffer_size = ce_m2m_buf_size;

	/* Start push task */
	tfm->mem.push_task = kthread_run(ce_push_task, tfm, "DMX-CePush-%p",
									 tfm);
	if (unlikely(IS_ERR(tfm->mem.push_task))) {
		CE_ERROR("Failed to create push task\n");
		return -ENOMEM;
	}

	/* switch to real time scheduling (if requested) */
	if (thread_dmx_cepush[1]) {
		param.sched_priority = thread_dmx_cepush[1];
		if (0 != sched_setscheduler(tfm->mem.push_task,
						thread_dmx_cepush[0],
						&param)) {
			CE_ERROR("Failed to set scheduling parameters "\
					"to priority %d Mode :(%d)\n", \
			thread_dmx_cepush[1], thread_dmx_cepush[0]);
		}
	}

	/* Add registry connection */
	err = stm_registry_add_connection(tfm, "push-sink", sink);
	if (unlikely(err)) {
		kfree(tfm->mem.push_buffer);
		CE_ERROR("stm_registry_add_connection %p->%p, err %d\n", tfm,
				 sink, err);
	} else {
		tfm->mem.sink = sink;
		CE_INFO("Transform %p attached to push sink %p\n", tfm, sink);
	}

	return err;
}

int ce_transform_detach_push_sink(struct ce_transform *tfm, stm_object_h sink)
{
	int err;
	CE_ENTRY("tfm=%p, sink=%p\n", tfm, sink);

	if (tfm->mem.push_task) {
		kthread_stop(tfm->mem.push_task);
		tfm->mem.push_task = NULL;
	}

	err = stm_registry_remove_connection(tfm, "push-sink");
	if (unlikely(err))
		return err;

	err = tfm->mem.sink_u.push.disconnect(tfm, sink);
	if (unlikely(err)) {
		CE_ERROR("Transform %p, sink %p disconnect, err %d\n", tfm,
				 sink, err);
	}

	tfm->mem.sink = NULL;
	memset(&tfm->mem.sink_u.push, 0, sizeof(tfm->mem.sink_u.push));

	/* Free push buffer */
	kfree(tfm->mem.push_buffer);
	tfm->mem.push_buffer = NULL;
	tfm->mem.push_buffer_size = 0;

	CE_EXIT("%d\n", err);
	return err;
}

/* Memory transform pull source interface, given to an attached pull sink */
static stm_data_interface_pull_src_t ce_data_pull_src_interface = {
	.pull_data = ce_pull_src_data,
	.test_for_data = ce_pull_src_test_for_data,
};

/* Exported mem transform data push interface */
stm_data_interface_push_sink_t ce_data_push_interface = {
	.connect = &ce_push_sink_connect,
	.disconnect = &ce_push_sink_disconnect,
	.push_data = &ce_push_data,
	.mem_type = KERNEL,
	.mode = STM_IOMODE_BLOCKING_IO,
	.alignment = 16,
	.max_transfer = CE_M2M_MAX_TRANSFER,
	.paketized = 0,
};
