/***********************************************************************
 *
 * File: hdmirx/include/stm_hdmirx_os.h
 * Copyright (c) 2005-2012 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/
#ifndef __HDMIRX_OS__

#define __HDMIRX_OS__
#include <linux/kernel.h>
#include <linux/semaphore.h>
#include <linux/delay.h>
#include <linux/kthread.h>
#include <linux/sched.h>
#include <linux/i2c.h>
#include <linux/mm.h>		/* for verify_area */
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <asm/param.h>
#include <asm/current.h>
#include <asm/uaccess.h>
#include <asm/unistd.h>
#include <asm/io.h>

#include <stddefs.h>

#define stm_hdmirx_getclocks_persec()      (HZ)

//typedef struct semaphore              *os_semaphore_t;
typedef struct semaphore stm_hdmirx_semaphore;
typedef struct task_struct *stm_hdmirx_thread;
typedef struct i2c_adapter *hdmirx_i2c_adapter;
typedef struct i2c_msg hdmirx_i2c_msg;

int stm_hdmirx_thread_start(void);
int stm_hdmirx_thread_exit(void);
int stm_hdmirx_thread_create(stm_hdmirx_thread *thread,
                             void (*task_entry) (void *param),
                             void (*hdmirx_handle),
                             const char *name, const int *thread_settings);
int stm_hdmirx_free(void *add);

void *stm_hdmirx_malloc(unsigned int size);
int stm_hdmirx_sema_init(stm_hdmirx_semaphore **sema,
                         unsigned int initialcount);
int stm_hdmirx_sema_wait(stm_hdmirx_semaphore *sema);
int stm_hdmirx_sema_wait_timeout(stm_hdmirx_semaphore *sema, long timeout);
void stm_hdmirx_sema_signal(stm_hdmirx_semaphore *sema);
int stm_hdmirx_sema_delete(stm_hdmirx_semaphore *sema);

int stm_hdmirx_thread_wait(stm_hdmirx_thread *thread);
void stm_hdmirx_delay_ms(unsigned short timeout);
void stm_hdmirx_delay_us(uint16_t uDelay);
unsigned long stm_hdmirx_time_now(void);
unsigned long stm_hdmirx_time_minus(unsigned long t1, unsigned long t2);
void stm_hdmirx_task_lock(void);
void stm_hdmirx_task_unlock(void);
int stm_hdmirx_memcpy(void *destination, const void *source, unsigned int num);
long stm_hdmirx_convert_ms_to_jiffies(uint16_t timeout);
void stm_hdmirx_pm_runtime_get(struct device *dev);
void stm_hdmirx_pm_runtime_put(struct device *dev);

#define ReadRegisterByte(Address)           ((uint32_t)readb((void __iomem *) Address))
#define ReadRegisterWord(Address)           ((uint32_t)readw((void __iomem *) Address ))
#define ReadRegisterLong(Address)           ((uint32_t)readl((void __iomem *) Address ))
#define ReadRegisterTriByte(Address)        ((uint32_t) ((readb((void __iomem *)((Address) + 2)) << 16 | (readw((void __iomem *) Address))))
#define WriteRegisterByte(Address,Value)    (writeb( (uint32_t)Value, (void __iomem *) Address ))
#define WriteRegisterWord(Address,Value)    (writew( Value, (void __iomem *) Address ))
#define WriteRegisterLong(Address,Value)     (writel( Value, (void __iomem *) Address ))
#define WriteRegisterTriByte(Address,Value)  writew((uint16_t)Value,(void __iomem *) Address);  \
                                                writeb((uint8_t)(((Value) & 0xFF0000) >> 16),(void __iomem *)(Address+2))
#endif
