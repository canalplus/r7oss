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

Source file name : stm_ce_osal.h

Declares OS-astraction macros for stm_ce driver
************************************************************************/

#ifndef __STM_CE_OSAL_H
#define __STM_CE_OSAL_H

/* When defined, trace is enabled */
#define CONFIG_STM_CE_TRACE

#if defined(__KERNEL__)
/*
 * Linux kernel OS calls
 */
#include <linux/dma-mapping.h>
#include <linux/freezer.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/platform_device.h>
#include <linux/rwsem.h>
#include <linux/scatterlist.h>
#include <linux/semaphore.h>
#include <linux/slab.h>
#include <linux/atomic.h>
#include <asm/page.h>


/* Memory management functions */
#define OS_malloc(size) kmalloc(size, GFP_KERNEL|__GFP_REPEAT)
#define OS_calloc(size) kzalloc(size, GFP_KERNEL)
#define OS_free(p) kfree(p)
#define OS_virt_addr_valid(p) virt_addr_valid(p)

/* Locking macros */
/* Note parameter usage - '&<param>' */
/* Allow multiple reader - Note, not interruptible. */
#define OS_lock_t struct rw_semaphore
#define OS_lock_init(lock) init_rwsem(&lock)
#define OS_lock_del(lock)
#define OS_wlock(lock) down_write(&lock)
#define OS_wunlock(lock) up_write(&lock)
#define OS_rlock(lock) down_read(&lock)
#define OS_runlock(lock) up_read(&lock)

/* Atomic operations */
#define OS_atomic_t atomic_t
#define OS_atomic_read(a) atomic_read(&a)
#define OS_atomic_set(a, v) atomic_set(&a, v)
#define OS_atomic_inc_return(a) atomic_inc_return(&a)
#define OS_atomic_inc(a) atomic_inc(&a)
#define OS_atomic_dec(a) atomic_dec(&a)

/* DMA flags */
#define OS_DMA_TO_DEVICE DMA_TO_DEVICE
#define OS_DMA_FROM_DEVICE DMA_FROM_DEVICE
#define OS_DMA_BIDIRECTIONAL DMA_BIDIRECTIONAL

/* Trace macros */
#ifdef CONFIG_STM_CE_TRACE
/* Trace controlled by module parameters */
extern int ce_trace_api_entry;
extern int ce_trace_api_exit;
extern int ce_trace_int_entry;
extern int ce_trace_int_exit;
extern int ce_trace_info;
extern int ce_trace_mme;
extern int ce_trace_error;
#define CE_ERROR(fmt, args...)						\
do {									\
	if (ce_trace_error)						\
		printk(KERN_ERR "[%s:%d] ERROR: " fmt,			\
				__func__, __LINE__, ## args);		\
} while (0)

#define CE_INFO(fmt, args...)						\
do {									\
	if (ce_trace_info)						\
		printk(KERN_INFO "[%s:%d] " fmt,			\
				__func__, __LINE__, ## args);		\
} while (0)

#define CE_MME(fmt, args...)						\
do {									\
	if (ce_trace_mme)						\
		printk(KERN_INFO "[CE MME] " fmt, ## args);		\
} while (0)

#define CE_API_ENTRY(fmt, args...)					\
do {									\
	if (ce_trace_api_entry)						\
		printk(KERN_INFO"[%s entry] " fmt, __func__, ## args);	\
} while (0)

#define CE_API_EXIT(fmt, args...)					\
do {									\
	if (ce_trace_api_exit)						\
		printk(KERN_INFO "[%s return] " fmt, __func__, ## args);\
} while (0)

#define CE_ENTRY(fmt, args...)						\
do {									\
	if (ce_trace_int_entry)						\
		printk(KERN_INFO"[%s entry] " fmt, __func__, ## args);	\
} while (0)

#define CE_EXIT(fmt, args...)						\
do {									\
	if (ce_trace_int_exit)						\
		printk(KERN_INFO "[%s return] " fmt, __func__, ## args);\
} while (0)

extern int ce_trace_hal_entry;
extern int ce_trace_hal_exit;
extern int ce_trace_hal_error;
extern int ce_trace_hal_info;
#define CE_HAL_ENTRY(fmt, args...)					\
do {									\
	if (ce_trace_hal_entry)						\
		printk(KERN_INFO "CE_HAL[%s entry] " fmt,		\
					__func__,  ## args);		\
} while (0)

#define CE_HAL_EXIT(fmt, args...)					\
do {									\
	if (ce_trace_hal_exit)						\
		printk(KERN_INFO "CE_HAL[%s return] " fmt,		\
					__func__, ## args);		\
} while (0)

#define CE_HAL_ERROR(fmt, args...)					\
do {									\
	if (ce_trace_hal_error)						\
		printk(KERN_ERR "CE_HAL[%s:%d] ERROR: " fmt,		\
					__func__, __LINE__, ## args);	\
} while (0)

#define CE_HAL_INFO(fmt, args...)					\
do {									\
	if (ce_trace_hal_info)						\
		printk(KERN_INFO "CE_HAL[%s:%d]: " fmt,			\
					__func__, __LINE__, ## args);	\
} while (0)

#else
#define CE_ERROR(fmt, args...)
#define CE_INFO(fmt, args...)
#define CE_MME(fmt, args...)
#define CE_API_ENTRY(fmt, args...)
#define CE_API_EXIT(fmt, args...)
#define CE_ENTRY(fmt, args...)
#define CE_EXIT(fmt, args...)
#define CE_HAL_ENTRY(fmt, args...)
#define CE_HAL_EXIT(fmt, args...)
#define CE_HAL_ERROR(fmt, args...)
#define CE_HAL_INFO(fmt, args...)
#endif

#elif defined(__SH4__) || defined(__ST200__)
/*
 * OS21 calls
 */
#include <os21.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include "list.h"

typedef int int32_t;
typedef unsigned int uint32_t;
typedef unsigned char uint8_t;

/* Memory management functions */
#define OS_malloc(size) memory_allocate(0, size);
#define OS_calloc(size) memory_allocate_clear(0, 1, size);
#define OS_free(p) memory_deallocate(0, p)

/* Locking macros */
#define OS_lock_t mutex_t *
#define OS_lock_init(lock) (lock = mutex_create_fifo())
#define OS_lock_del(lock) mutex_delete(lock)
#define OS_wlock(lock) mutex_lock(lock)
#define OS_wunlock(lock) mutex_release(lock)
#define OS_rlock(lock) mutex_lock(lock)
#define OS_runlock(lock) mutex_release(lock)

/* Trace macros */
#ifdef CONFIG_STM_CE_TRACE
#define CE_ERROR(fmt, args...) \
	printf("[%s:%d] ERROR: " fmt, __func__, __LINE__, ## args)
#define CE_INFO(fmt, args...) \
	printf("[%s:%d] " fmt, __func__, __LINE__, ## args)
#define CE_MME(fmt, args...) \
	printf("[CE MME] " fmt, ## args)
#define CE_API_ENTRY(fmt, args...) \
	printf("[%s entry] " fmt, __func__, ## args)
#define CE_API_EXIT(fmt, args...) \
	printf("[%s return] " fmt, __func__, ## args)
#define CE_ENTRY(fmt, args...) \
	printf("[%s entry] " fmt, __func__, ## args)
#define CE_EXIT(fmt, args...) \
	printf("[%s return] " fmt, __func__, ## args)
#define CE_HAL_ENTRY(fmt, args...) \
	printf("CE_HAL[%s entry] " fmt, __func__,  ## args)
#define CE_HAL_EXIT(fmt, args...) \
	printf("CE_HAL[%s return] " fmt, __func__,  ## args)
#define CE_HAL_ERROR(fmt, args...) \
	printf("CE_HAL[%s:%d] ERROR: " fmt, __func__, __LINE__, ## args)
#define CE_HAL_INFO(fmt, args...) \
	printf("CE_HAL[%s:%d]: " fmt, __func__, __LINE__, ## args)
#else
#define CE_ERROR(fmt, args...)
#define CE_INFO(fmt, args...)
#define CE_MME(fmt, args...)
#define CE_API_ENTRY(fmt, args...)
#define CE_API_EXIT(fmt, args...)
#define CE_ENTRY(fmt, args...)
#define CE_EXIT(fmt, args...)
#define CE_HAL_ENTRY(fmt, args...)
#define CE_HAL_EXIT(fmt, args...)
#define CE_HAL_ERROR(fmt, args...)
#define CE_HAL_INFO(fmt, args...)
#endif

#else
#error "No OS abstraction defined"
#endif

#endif
