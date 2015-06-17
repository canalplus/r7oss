/******************************************************************************
Copyright (C) 2012, 2013 STMicroelectronics. All Rights Reserved.

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

Source file name : te_work.h

Defines buffer work queue operations
******************************************************************************/

#include "te_buffer_work.h"

#include <stm_te_dbg.h>
#include <pti_driver.h>
#include <pti_hal_api.h>

#include <linux/workqueue.h>
#include <linux/radix-tree.h>
#include <linux/mutex.h>
#include <linux/kthread.h>
#include <linux/jiffies.h>

#define BWQ_IDLE_WAIT 30 /* msec */

#define BWQ_MAX_LOOKUP 128

static int thread_dmx_bufwd[2] = { SCHED_NORMAL, 0 };
module_param_array_named(thread_DMX_BufWd,thread_dmx_bufwd, int, NULL, 0644);
MODULE_PARM_DESC(thread_DMX_BufWd, "DMX-BufWd thread:s(Mode),p(Priority)");

struct te_buffer_work
{
	struct te_obj *obj;
	struct te_hal_obj *buffer;
	struct work_struct work;
	bool queued;
	TE_BWQ_WORK func;
};

struct te_buffer_work_queue
{
	struct workqueue_struct *wq;
	struct radix_tree_root root;
	struct mutex m;
	struct te_hal_obj *signal;
	struct task_struct *thd;
	unsigned long next_idle;
};

static void te_bwq_schedule_idle(struct te_buffer_work_queue *bwq)
{
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3, 4, 0))
	struct te_buffer_work *bw;
	unsigned int nr_slots = BWQ_MAX_LOOKUP;
	unsigned int i;

	void **slots[BWQ_MAX_LOOKUP];
	unsigned long last_index = 0;

	if (time_is_before_jiffies(bwq->next_idle)) {
		/* Idle delay has expired, iterate buffers and schedule */
		bwq->next_idle += msecs_to_jiffies(BWQ_IDLE_WAIT);

		if (mutex_lock_interruptible(&bwq->m) != 0)
			return;

		while (nr_slots == BWQ_MAX_LOOKUP) {

			nr_slots = radix_tree_gang_lookup_slot(
						&bwq->root,
						slots,
#if (LINUX_VERSION_CODE > KERNEL_VERSION(3, 0, 0))
						NULL,
#endif
						last_index,
						BWQ_MAX_LOOKUP);

			for (i = 0; i < nr_slots; i++) {
				bw = radix_tree_deref_slot(slots[i]);
				if (!bw->queued) {
					pr_debug("queued idle buffer 0x%08x\n",
							bw->buffer->hdl.word);
					bw->queued = true;
					queue_work(bwq->wq, &bw->work);
				}
				last_index = bw->buffer->hdl.word;
			}
		}
		mutex_unlock(&bwq->m);
	}
#else
	struct te_buffer_work *bw;
	struct radix_tree_iter iter;
	void **slot;

	if (time_is_before_jiffies(bwq->next_idle)) {
		/* Idle delay has expired, iterate buffers and schedule */

		bwq->next_idle += msecs_to_jiffies(BWQ_IDLE_WAIT);

		if (mutex_lock_interruptible(&bwq->m) != 0)
			return;

		radix_tree_for_each_slot(slot, &bwq->root, &iter, 0) {
			bw = radix_tree_deref_slot(slot);
			if (!bw->queued) {
				pr_debug("queued idle buffer 0x%08x\n", bw->buffer->hdl.word);
				bw->queued = true;
				queue_work(bwq->wq, &bw->work);
			}
		}
		mutex_unlock(&bwq->m);
	}
#endif
}

/*!
 * \brief monitor signal and despatch work
 */
static int te_bwq_task(void *data)
{
	struct te_buffer_work_queue *bwq = (struct te_buffer_work_queue *)data;
	ST_ErrorCode_t error;
	FullHandle_t buffer;
	struct te_buffer_work *bw;

	while (!kthread_should_stop()) {
		error = stptiHAL_call(Signal.HAL_SignalWait,
				bwq->signal->hdl,
				&buffer, BWQ_IDLE_WAIT);

		if (ST_NO_ERROR != error) {
			if (ST_ERROR_TIMEOUT != error)
				pr_warn("Ignoring HAL_SignalWait error 0x%x\n", error);
			else
				te_bwq_schedule_idle(bwq);
			continue;
		}

		if (HAL_HDL_IS_NULL(buffer))
			continue;

		if (mutex_lock_interruptible(&bwq->m) != 0)
			continue;

		bw =  radix_tree_lookup(&bwq->root, buffer.word);
		if (bw) {
			pr_debug("Found entry for buffer 0x%08x\n", buffer.word);
			bw->queued = true;
			queue_work(bwq->wq, &bw->work);
		} else {
			pr_debug("Buffer 0x%08x not in radix tree\n",
				buffer.word);
		}
		mutex_unlock(&bwq->m);

		te_bwq_schedule_idle(bwq);
	}

	return 0;
}

/*!
 * \brief despatch work to object handlers
 */
static void te_bwq_despatch(struct work_struct *work)
{
	struct te_buffer_work *bw
		= container_of(work, struct te_buffer_work, work);

	pr_debug("despatching buffer 0x%08x\n", bw->buffer->hdl.word);
	bw->func(bw->obj, bw->buffer);
	bw->queued = false;
	pr_debug("despatched buffer 0x%08x\n", bw->buffer->hdl.word);

}

/*!
 * \brief create a buffer work queue
 * \param bwq pointer to store created work queue in
 * \param q   the work queue to use (NULL to create new)
 * \returns 0 on success, otherwise errno
 */
int te_bwq_create(struct te_buffer_work_queue **bwq,
			struct te_hal_obj *signal,
			struct workqueue_struct *q)
{
	int err = 0;
	struct te_buffer_work_queue *new_bwq;

	struct sched_param	param;

	*bwq = NULL;

	if (signal == NULL || q == NULL) {
		pr_err("must provide signal and work queue\n");
		return -EINVAL;
	}

	te_hal_obj_inc(signal);

	new_bwq = kzalloc(sizeof(*new_bwq), GFP_KERNEL);
	if (new_bwq == NULL) {
		pr_err("no memory for buffer work queue\n");
		err = -ENOMEM;
		goto error;
	}

	new_bwq->wq = q;
	new_bwq->signal = signal;
	new_bwq->next_idle = jiffies + msecs_to_jiffies(BWQ_IDLE_WAIT);

	INIT_RADIX_TREE(&new_bwq->root, GFP_KERNEL);
	mutex_init(&new_bwq->m);

	new_bwq->thd = kthread_run(te_bwq_task, new_bwq, "DMX-BufWd-%01d.%02d",
			signal->hdl.member.pDevice,
			signal->hdl.member.vDevice);

	if (!new_bwq->thd) {
		pr_err("couldn't spin BWQ thread\n");
		err = -ENOMEM;
		goto error;
	}

	/* switch to real time scheduling (if requested) */
	if (thread_dmx_bufwd[1]) {
		param.sched_priority = thread_dmx_bufwd[1];
		if (0 != sched_setscheduler(new_bwq->thd, thread_dmx_bufwd[0], &param)) {
			pr_err("FAILED to set scheduling parameters to priority %d Mode :(%d)\n", thread_dmx_bufwd[1], thread_dmx_bufwd[0]);
		}
	}

	*bwq = new_bwq;

	return 0;

error:
	te_hal_obj_dec(signal);
	kfree(new_bwq);
	return err;
}

/*!
 * \brief destroy a buffer work queue
 * \param bwq to destroy
 */
int te_bwq_destroy(struct te_buffer_work_queue *bwq)
{
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3, 4, 0))
	struct te_buffer_work *bw;
	unsigned int nr_slots = BWQ_MAX_LOOKUP;
	unsigned int i;

	void **slots[BWQ_MAX_LOOKUP];

	if (!bwq)
		return -EINVAL;

	kthread_stop(bwq->thd);

	/* Iterate through the tree, disassociate buffers and cancel any work */
	while (nr_slots == BWQ_MAX_LOOKUP) {
		nr_slots = radix_tree_gang_lookup_slot(
					&bwq->root,
					slots,
#if (LINUX_VERSION_CODE > KERNEL_VERSION(3, 0, 0))
						NULL,
#endif
					0,
					BWQ_MAX_LOOKUP);

		for (i = 0; i < nr_slots; i ++) {
			bw = radix_tree_deref_slot(slots[i]);

			pr_debug("cancelling work for buffer 0x%08x\n",
				bw->buffer->hdl.word);

			cancel_work_sync(&bw->work);

			pr_debug("cancelled work for buffer 0x%08x\n",
				bw->buffer->hdl.word);

			te_hal_obj_dec(bw->buffer);
			kfree(bw);
		}
	}
	return 0;
#else
	struct te_buffer_work *bw;
	struct radix_tree_iter iter;
	void **slot;

	if (!bwq)
		return -EINVAL;

	kthread_stop(bwq->thd);

	/* Iterate through the tree, disassociate buffers and cancel any work */
	radix_tree_for_each_slot(slot, &bwq->root, &iter, 0) {
		bw = radix_tree_deref_slot(slot);

		pr_debug("cancelling work for buffer 0x%08x\n",
				bw->buffer->hdl.word);

		cancel_work_sync(&bw->work);

		pr_debug("cancelled work for buffer 0x%08x\n",
				bw->buffer->hdl.word);

		te_hal_obj_dec(bw->buffer);
		kfree(bw);
	}

	te_hal_obj_dec(bwq->signal);

	kfree(bwq);
	return 0;
#endif
}

/*!
 * \brief register an object, its buffer and the work callback
 * \param bwq the work queue to register on
 * \param obj the TE object associated with this work package
 * \param buffer the buffer to monitor
 * \param func the function that will be called when work is to be done
 */
int te_bwq_register(struct te_buffer_work_queue *bwq,
			struct te_obj *obj,
			struct te_hal_obj *buffer,
			TE_BWQ_WORK func)
{
	int err = 0;
	ST_ErrorCode_t Error = ST_NO_ERROR;
	struct te_buffer_work *bw;

	pr_debug("registering obj %p buffer %p (0x%08x) func %p\n",
			obj, buffer, buffer->hdl.word, func);

	/* Check if this buffer already has the same work associated. If so
	 * ignore. If the requested work is different print warning but keep
	 * original work function
	 *
	 * TODO: This shouldn't be needed when the connection model is reworked
	 * to prevent potential multiple associations of work with buffers */

	if (mutex_lock_interruptible(&bwq->m) != 0)
		return -EINTR;

	bw = radix_tree_lookup(&bwq->root, buffer->hdl.word);
	mutex_unlock(&bwq->m);
	if (bw) {
		if (bw->func != func) {
			pr_warn("Buffer 0x%x already has a work function\n",
					buffer->hdl.word);
			return -EEXIST;
		} else {
			pr_debug("Buffer 0x%x already has same work func\n",
					buffer->hdl.word);
			return 0;
		}
	}

	/* Create and initialise the buffer work struct */
	bw = kzalloc(sizeof(*bw), GFP_KERNEL);
	if (!bw) {
		pr_err("no memory for work\n");
		return -ENOMEM;
	}

	te_hal_obj_inc(buffer);

	bw->obj = obj;
	bw->buffer = buffer;
	bw->func = func;
	INIT_WORK(&bw->work, te_bwq_despatch);

	/* Store the work struct in the tree, and associate the buffer to signal */
	if (mutex_lock_interruptible(&bwq->m) != 0) {
		goto error_mtx;
	}
	err = radix_tree_insert(&bwq->root, buffer->hdl.word, bw);
	mutex_unlock(&bwq->m);

	if (err != 0) {
		pr_err("unable to insert in radix tree\n");
		goto error;
	}

	Error = stptiOBJMAN_AssociateObjects(
			bwq->signal->hdl,
			buffer->hdl);

	if (Error != ST_NO_ERROR) {
		pr_err("couldn't associate signal 0x%08x to buffer 0x%08x (%d)\n",
			bwq->signal->hdl.word, buffer->hdl.word, Error);
		err = -EIO;
		goto error;
	}

	pr_debug("associated signal 0x%08x to buffer 0x%08x\n",
			bwq->signal->hdl.word,
			buffer->hdl.word);

	return 0;

error:
	/*
	 * Not sure what to do if the mutex is interrupted...
	 * We'll leave a hanging entry in the list and an associated signal,
	 * but hopefully bwq destroy will look after it later.
	 */
	if (mutex_lock_interruptible(&bwq->m) == 0) {
		stptiOBJMAN_DisassociateObjects(
			bwq->signal->hdl,
			buffer->hdl);
		radix_tree_delete(&bwq->root, buffer->hdl.word);
		mutex_unlock(&bwq->m);
	}

error_mtx:
	kfree(bw);
	te_hal_obj_dec(buffer);

	return err;
}

/*!
 * \brief unregister a buffer and stop monitoring it
 * \param bwq the work queue to unregister from
 * \param buffer the buffer to unregister
 * \returns non-zero if buffer was freed
 */
int te_bwq_unregister(struct te_buffer_work_queue *bwq, struct te_hal_obj *buffer)
{
	struct te_buffer_work *bw = NULL;
	int ret = 0;

	if (mutex_lock_interruptible(&bwq->m) != 0)
		return 0; /* Return 0 on error due to buffer ref'ing */

	pr_debug("unregistering buffer %p (0x%08x)\n", buffer, buffer->hdl.word);

	bw = radix_tree_lookup(&bwq->root, buffer->hdl.word);
	if (bw) {
		stptiOBJMAN_DisassociateObjects(
			bwq->signal->hdl,
			buffer->hdl);
		cancel_work_sync(&bw->work);
		radix_tree_delete(&bwq->root, buffer->hdl.word);
		ret = te_hal_obj_dec(bw->buffer);
		kfree(bw);
		pr_debug("unregistered buffer 0x%08x\n", buffer->hdl.word);
	}
	mutex_unlock(&bwq->m);

	return ret;
}
