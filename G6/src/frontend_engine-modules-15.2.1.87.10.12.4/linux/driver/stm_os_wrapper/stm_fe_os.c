/************************************************************************
Copyright (C) 2011 STMicroelectronics. All Rights Reserved.

This file is part of the stm_fe Library.

stm_fe is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License version 2 as published by the
Free Software Foundation.

stm_fe is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with stm_fe; see the file COPYING.  If not, write to the Free Software
Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

The stm_fe Library may alternatively be licensed under a proprietary
license from ST.

Source file name : stm_fe_os.c
Author :           shobhit

OS wrapper for stm_fe

Date        Modification                                    Name
----        ------------                                    --------
25-Jun-11   Created                                         shobhit

************************************************************************/

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/semaphore.h>
#include <linux/delay.h>
#include <linux/kthread.h>
#include <linux/sched.h>
#include <linux/i2c.h>
#include <linux/mm.h>		/* for verify_area */
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/wait.h>
#include <linux/hardirq.h>
#include <linux/param.h>
#include <asm/current.h>
#include <linux/uaccess.h>
#include <linux/unistd.h>
#include <linux/platform_device.h>
#include <linux/stm/plat_dev.h>
#include "stm_fe_os.h"
static int os_wrapper_debug;
module_param_named(os_wrapper_debug, os_wrapper_debug, int, 0644);
MODULE_PARM_DESC(os_wrapper_debug,
		    "Turn on/off stmfe os wrapper debugging (default:off).");
#define dpr_info(x...) do { if (os_wrapper_debug) pr_info(x); } while (0)
#define dpr_err(x...) do { if (os_wrapper_debug) pr_err(x); } while (0)

/*****************************************************************************
 *@Name        : stm_fe_thread_start
 *@Description : yielding the thread

*****************************************************************************/
int stm_fe_thread_start(void)
{
	cond_resched();	     /*yield the current processor to other threads */
	dpr_info("%s: thread started\n", __func__);
	return 0;
}
EXPORT_SYMBOL(stm_fe_thread_start);

/*****************************************************************************
 *@Name        : stm_fe_thread_create
 *@Description : creation of thread

*****************************************************************************/
int stm_fe_thread_create(fe_thread *thread,
			 int (*task_entry) (void *param),
			 void *data, const char *name, int *priority)
{
	int ret = 0;
	struct sched_param param;
	struct task_struct *taskp;
	taskp = kthread_create(task_entry, data, "%s", name);
	if (IS_ERR(taskp)) {
		pr_err("%s: thread creation error\n", __func__);
		return PTR_ERR(taskp);
	}
	taskp->flags |= PF_NOFREEZE;

	/* switch to real time scheduling (if requested) */
	if (priority[1]) {
		param.sched_priority = priority[1];
		ret = sched_setscheduler(taskp, priority[0], &param);
		if (ret) {
			pr_err("%s: set prio %d failed\n", __func__,
								   priority[1]);
			return ret;
		}
	}
	wake_up_process(taskp);
	*thread = taskp;
	dpr_info("%s: thread created %s prio %d\n", __func__, name,
								   priority[1]);
	return ret;
}
EXPORT_SYMBOL(stm_fe_thread_create);

/*****************************************************************************
 *@Name        : stm_fe_thread_stop
 *@Description :  for task wait (deletion)

*****************************************************************************/
int stm_fe_thread_stop(fe_thread *thread)
{
	int ret = -1;
	ret = kthread_stop(*thread);
	dpr_info("%s: thread stopped\n", __func__);
	return ret;
}
EXPORT_SYMBOL(stm_fe_thread_stop);

/*****************************************************************************
 *@Name        : stm_fe_sema_init
 *@Description : creation of semaphore
		with initial count = 0 or 1
*****************************************************************************/

int stm_fe_sema_init(fe_semaphore *sema, uint32_t initialcount)
{
	if (sema) {
		sema_init(sema, initialcount);
		dpr_info("%s: semaphore init with count %u\n", __func__,
								  initialcount);
		return 0;
	} else {
		dpr_err("%s: init error occured, sema is NULL\n", __func__);
		return -ENOMEM;
	}
}
EXPORT_SYMBOL(stm_fe_sema_init);

/*****************************************************************************
 *@Name        : stm_fe_sema_wait
 *@Description : waiting for semaphore release

*****************************************************************************/
int stm_fe_sema_wait(fe_semaphore *sema)
{
	int ret = 0;
	ret = down_interruptible(sema);
	return ret;
}
EXPORT_SYMBOL(stm_fe_sema_wait);

/*****************************************************************************
 *@Name        : stm_fe_sema_waittimeout
 *@Description : semaphore wait with timeout value

*****************************************************************************/
int stm_fe_sema_wait_timeout(fe_semaphore *sema, long timeout)
{
	int ret = -1;

	ret = down_timeout(sema, timeout);

	return ret;
}
EXPORT_SYMBOL(stm_fe_sema_wait_timeout);

/*****************************************************************************
 *@Name        : stm_fe_sema_signal
 *@Description :  release the semaphore

*****************************************************************************/
void stm_fe_sema_signal(fe_semaphore *sema)
{

	up(sema);

}
EXPORT_SYMBOL(stm_fe_sema_signal);

/*****************************************************************************
 *@Name        : stm_fe_mutex_init
 *@Description : dynamically initialize a mutex

*****************************************************************************/
int stm_fe_mutex_init(fe_mutex *mtx)
{
	if (mtx) {
		mutex_init(mtx);
		dpr_info("%s: mtx initialized\n", __func__);
		return 0;
	} else {
		dpr_err("%s: init failed as mtx is NULL\n", __func__);
		return -ENOMEM;
	}
}
EXPORT_SYMBOL(stm_fe_mutex_init);

/*****************************************************************************
 *@Name        : stm_fe_spin_lock
 *@Description : spin lock api for stm_fe

*****************************************************************************/
int stm_fe_spin_lock(spinlock_t *sl)
{
	if (!sl) {
		dpr_err("%s: lock failed as spin lock is NULL\n", __func__);
		return -EINVAL;
	}
	spin_lock(sl);

	return 0;
}
EXPORT_SYMBOL(stm_fe_spin_lock);

/*****************************************************************************
 *@Name        : stm_fe_spin_unlock
 *@Description : Unlocks the given spin_lock

*****************************************************************************/
int stm_fe_spin_unlock(spinlock_t *sl)
{
	if (!sl) {
		dpr_err("%s: unlock failed as spin lock is NULL\n", __func__);
		return -EINVAL;
	}
	spin_unlock(sl);

	return 0;
}
EXPORT_SYMBOL(stm_fe_spin_unlock);

/*****************************************************************************
 *@Name        : stm_fe_spin_lock_bh
 *@Description : spin lock api for stm_fe and disables bottom halves

*****************************************************************************/
int stm_fe_spin_lock_bh(spinlock_t *sl)
{
	if (!sl) {
		dpr_err("%s: lock failed as spin lock is NULL\n", __func__);
		return -EINVAL;
	}
	spin_lock_bh(sl);

	return 0;
}
EXPORT_SYMBOL(stm_fe_spin_lock_bh);

/*****************************************************************************
 *@Name        : stm_fe_spin_unlock_bh
 *@Description : Unlocks the given spin_lock and enables bottom halves

*****************************************************************************/
int stm_fe_spin_unlock_bh(spinlock_t *sl)
{
	if (!sl) {
		dpr_err("%s: unlock failed as spin lock is NULL\n", __func__);
		return -EINVAL;
	}
	spin_unlock_bh(sl);

	return 0;
}
EXPORT_SYMBOL(stm_fe_spin_unlock_bh);


/*****************************************************************************
 *@Name        : stm_fe_mutex_lock
 *@Description : Locks the given mutex; sleeps if the lock is
		 unavailable

*****************************************************************************/
int stm_fe_mutex_lock(fe_mutex *mtx)
{
	if (!mtx) {
		dpr_err("%s: lock failed as mtx is NULL\n", __func__);
		return -EINVAL;
	}
	mutex_lock(mtx);

	return 0;
}
EXPORT_SYMBOL(stm_fe_mutex_lock);

/*****************************************************************************
 *@Name        : stm_fe_mutex_unlock
 *@Description : Unlocks the given mutex

*****************************************************************************/
int stm_fe_mutex_unlock(fe_mutex *mtx)
{
	if (!mtx) {
		dpr_err("%s: unlock failed as mtx is NULL\n", __func__);
		return -EINVAL;
	}
	mutex_unlock(mtx);

	return 0;
}
EXPORT_SYMBOL(stm_fe_mutex_unlock);

/*****************************************************************************
 *@Name        : stm_fe_mutex_trylock
 *@Description : try to acquire the mutex, without waiting

*****************************************************************************/
int stm_fe_mutex_trylock(fe_mutex *mtx)
{
	int ret = 0;

	if (!mtx) {
		dpr_err("%s: trylock failed as mtx is NULL\n", __func__);
		return -EINVAL;
	}
	ret = mutex_trylock(mtx);

	return ret;
}
EXPORT_SYMBOL(stm_fe_mutex_trylock);

/******************************************************************************
 *@Name        : fe_init_waitqueue_head
 *@Description : OS wrapper to initialize wait queue
*****************************************************************************/
void fe_init_waitqueue_head(fe_wait_queue_head_t *q)
{
	init_waitqueue_head(q);
}
EXPORT_SYMBOL(fe_init_waitqueue_head);

/******************************************************************************
 *@Name        : stm_fe_wake_up_interruptible
 *@Description : OS wrapper to wake up event in wait queue
*****************************************************************************/
void fe_wake_up_interruptible(fe_wait_queue_head_t *q)
{
	wake_up_interruptible(q);
}
EXPORT_SYMBOL(fe_wake_up_interruptible);

/*****************************************************************************
 *@Name        : stm_fe_malloc
 *@Description : Allocation of memmory according to size

*****************************************************************************/
void *stm_fe_malloc(unsigned int size)
{
	void *add_p;

	if (size <= KMALLOC_MAX)
		add_p = kmalloc(size, in_interrupt()
					 ? GFP_ATOMIC : GFP_KERNEL);
	else
		add_p = vmalloc(size);

	if (!add_p) {
		dpr_err("%s: failed due to insufficient memory", __func__);
	} else {
		memset(add_p, 0, size);
		dpr_info("%s: allocated memory of size %u\n", __func__, size);
	}
	return add_p;
}
EXPORT_SYMBOL(stm_fe_malloc);

/*****************************************************************************
 *@Name        : stm_fe_free
 *@Description :free the memory

*****************************************************************************/
int stm_fe_free(void **add)
{
	if (!*add) {
		pr_err("%s: request to free NULL pointer\n", __func__);
		return -EINVAL;
	}
	/* if it is a vmalloc allocation */
	if ((unsigned int)*add >= VMALLOC_START &&
		  (unsigned int)*add < VMALLOC_END) {
		vfree(*add);
	} else {
		kfree(*add);
	}
	*add = NULL;
	dpr_info("%s: freed up allocated memory\n", __func__);
	return 0;
}
EXPORT_SYMBOL(stm_fe_free);

/*****************************************************************************
 *@Name        : stm_fe_fifo_alloc
 *@Description : Memory allocation for kfifo

*****************************************************************************/
int stm_fe_fifo_alloc(struct kfifo *fifo, uint16_t size)
{
	int ret;
	ret = kfifo_alloc(fifo, size, GFP_KERNEL);
	if (ret < 0) {
		pr_err("%s: kfifo_alloc failed %d\n", __func__, ret);
		return ret;
	}
	dpr_info("%s: fifo allocated of size %u\n", __func__, size);
	return ret;
}
EXPORT_SYMBOL(stm_fe_fifo_alloc);

/*****************************************************************************
 *@Name        : stm_fe_fifo_free
 *@Description : Free memory for kfifo

*****************************************************************************/
int stm_fe_fifo_free(struct kfifo *fifo)
{
	int ret = 0;

	kfifo_free(fifo);
	dpr_info("%s: freed allocated memory for fifo\n", __func__);
	return ret;
}
EXPORT_SYMBOL(stm_fe_fifo_free);

/*****************************************************************************
 *@Name        : stm_fe_fifo_out
 *@Description : Get message from FIFO

*****************************************************************************/
int stm_fe_fifo_out(struct kfifo *fifo, char *buff, int size)
{
	int ret = 0;
	dpr_info("%s: getting buff of size %d from fifo\n", __func__, size);
	ret = kfifo_out(fifo, buff, size);
	if (ret < 0)
		pr_err("%s: kfifo_out : failed: %d\n", __func__, ret);
	dpr_info("%s: got buff of size  %d from fifo\n", __func__, ret);
	return ret;
}
EXPORT_SYMBOL(stm_fe_fifo_out);

/*****************************************************************************
 *@Name        : stm_fe_fifo_in
 *@Description : Put message in FIFO

*****************************************************************************/
int stm_fe_fifo_in(struct kfifo *fifo, char *buff, int size)
{
	int ret = 0;
	dpr_info("%s: pushing buff of size %d to fifo\n", __func__, size);
	ret = kfifo_in(fifo, buff, size);
	if (ret < 0)
		pr_err("%s: kfifo_in : failed: %d\n", __func__, ret);
	dpr_info("%s: pushed buff of size %d to fifo\n", __func__, ret);
	return ret;
}
EXPORT_SYMBOL(stm_fe_fifo_in);

/*****************************************************************************
 *@Name        : stm_fe_delay_ms
 *@Description :  converted ms to jiffies

*****************************************************************************/
long stm_fe_delay_ms(uint16_t timeout)
{
	return msecs_to_jiffies(timeout);
}
EXPORT_SYMBOL(stm_fe_delay_ms);

static int32_t __init stm_os_wrapper_init(void)
{
	pr_info("Loading os wrapper module...\n");

	return 0;
}

unsigned long stm_fe_time_now(void)
{
	return (unsigned long)jiffies;
}
EXPORT_SYMBOL(stm_fe_time_now);

unsigned long stm_fe_time_minus(unsigned long t1, unsigned long t2)
{
	unsigned long ret;
	ret = (unsigned long)((unsigned long)t1 - (unsigned long)t2);
	return ret;
}
EXPORT_SYMBOL(stm_fe_time_minus);

unsigned long stm_fe_time_plus(unsigned long t1, unsigned long t2)
{
	unsigned long ret;

	ret = (unsigned long)((unsigned long)t1 + (unsigned long)t2);

	return ret;
}
EXPORT_SYMBOL(stm_fe_time_plus);

uint32_t stm_fe_jiffies_to_msecs(unsigned long jiffies)
{
	return jiffies_to_msecs(jiffies);
}
EXPORT_SYMBOL(stm_fe_jiffies_to_msecs);



module_init(stm_os_wrapper_init);

static void __exit stm_os_wrapper_term(void)
{
	pr_info("Removing os wrapper module ...\n");

}

module_exit(stm_os_wrapper_term);

MODULE_DESCRIPTION("Frontend engine internal OS abstraction layer");
MODULE_AUTHOR("STMicroelectronics Limited");
MODULE_LICENSE("GPL");
