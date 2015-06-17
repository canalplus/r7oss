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

Source file name : stm_fe_os.h
Author :          Shobhit

Header for stm_fe_os.c

Date        Modification                                    Name
----        ------------                                    --------
16-Jun-11   Created                                    Shobhit

************************************************************************/
#ifndef _STM_FE_OS_H
#define _STM_FE_OS_H

#include <linux/semaphore.h>
#include <linux/mutex.h>
#include <linux/spinlock.h>
#include <linux/kfifo.h>

#define KMALLOC_MAX	(128 * 1024)
#define FE_WAIT_N_MS(X) schedule_timeout_interruptible(msecs_to_jiffies(X))
/* typedef struct semaphore		*os_semaphore_t; */
typedef struct semaphore fe_semaphore ;
typedef struct mutex fe_mutex;
typedef struct task_struct *fe_thread;
typedef struct i2c_adapter *fe_i2c_adapter;
typedef struct i2c_msg fe_i2c_msg;
typedef struct __wait_queue_head fe_wait_queue_head_t;

#define fe_wait_event_interruptible(q, condition) \
({\
int __ret;\
__ret = wait_event_interruptible(q, condition);\
__ret;\
})

#define fe_wait_event_interruptible_timeout(q, condition, t) \
({\
int __ret;\
__ret = wait_event_interruptible_timeout(q, condition, t);\
__ret;\
})

#define stm_fe_getclocks_persec()      (HZ)
#define stm_fe_thread_should_stop()      kthread_should_stop()
int stm_fe_thread_start(void);
int stm_fe_thread_exit(void);
int stm_fe_thread_create(fe_thread *thread,
			 int (*task_entry) (void *param),
			 void *data, const char *name, int *priority);
int stm_fe_free(void **add);

void *stm_fe_malloc(unsigned int size);
int stm_fe_sema_init(fe_semaphore *sema, uint32_t initialcount);
int stm_fe_sema_wait(fe_semaphore *sema);
int stm_fe_sema_wait_timeout(fe_semaphore *sema, long timeout);
void stm_fe_sema_signal(fe_semaphore *sema);

int stm_fe_mutex_init(fe_mutex *mtx);
int stm_fe_mutex_lock(fe_mutex *mtx);
int stm_fe_mutex_unlock(fe_mutex *mtx);
int stm_fe_mutex_trylock(fe_mutex *mtx);

int stm_fe_spin_lock(spinlock_t *sl);
int stm_fe_spin_unlock(spinlock_t *sl);

int stm_fe_spin_lock_bh(spinlock_t *sl);
int stm_fe_spin_unlock_bh(spinlock_t *sl);

int stm_fe_thread_stop(fe_thread *thread);
long stm_fe_delay_ms(uint16_t timeout);
unsigned long stm_fe_time_now(void);
unsigned long stm_fe_time_minus(unsigned long t1, unsigned long t2);
uint32_t stm_fe_jiffies_to_msecs(unsigned long jiffies);
void fe_wake_up_interruptible(fe_wait_queue_head_t *q);
void fe_init_waitqueue_head(fe_wait_queue_head_t *q);
int stm_fe_fifo_alloc(struct kfifo *fifo, uint16_t size);
int stm_fe_fifo_free(struct kfifo *fifo);
int stm_fe_fifo_out(struct kfifo *fifo, char *buff, int size);
int stm_fe_fifo_in(struct kfifo *fifo, char *buff, int size);
unsigned long stm_fe_time_plus(unsigned long t1, unsigned long t2);
#endif /* _STM_FE_OS_H */
