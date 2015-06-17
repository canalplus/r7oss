/***********************************************************************
 *
 * File: hdmirx/src/stm_hdmirx_os.c
 * Copyright (c) 2012 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

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
#include <linux/pm.h>
#include <linux/pm_runtime.h>

#include <vibe_debug.h>


#include "stm_hdmirx_os.h"
//#include "stddefs.h"

/*****************************************************************************
 *@Name        : stm_hdmirx_thread_start
 *@Description : yielding the thread

*****************************************************************************/
int stm_hdmirx_thread_start(void)
{
  yield();		/*yield the current processor to other threads */
  return 0;
}

/*****************************************************************************
 *@Name        : stm_hdmirx_memcpy
 *@Description : copy mem

*****************************************************************************/
int stm_hdmirx_memcpy(void *destination, const void *source,
                      unsigned int bytes_to_copy)
{
  memcpy(destination, source, bytes_to_copy);
  return 1;
}

/*****************************************************************************
 *@Name        : stm_hdmirx_task_lock
 *@Description : task lock

*****************************************************************************/
void stm_hdmirx_task_lock(void)
{
  //TBD
  //Linux function for task lock should be called
}

/*****************************************************************************
 *@Name        : stm_hdmirx_task_unlock
 *@Description : task unlcok

*****************************************************************************/
void stm_hdmirx_task_unlock(void)
{
  //TBD
}

/*****************************************************************************
 *@Name        : stm_hdmirx_thread_exit
 *@Description : Exit from the thread
*****************************************************************************/
int stm_hdmirx_thread_exit(void)
{
  while (!kthread_should_stop())
    {
      /*Set_current_state() here because schedule_timeout() calls
       * schedule() unconditionally. */
      schedule_timeout_interruptible(100);
    }

  return 0;
}

/*****************************************************************************
 *@Name        : stm_hdmirx_thread_create
 *@Description : creation of thread

*****************************************************************************/
int stm_hdmirx_thread_create(stm_hdmirx_thread *thread,
                             void (*task_entry) (void *param),
                             void (*hdmirx_handle),
                             const char *name, const int *thread_settings)
{

  int ret = 0;
  struct task_struct *taskp;
  int                sched_policy;
  struct sched_param sched_param;

  taskp = kthread_create((int (*)(void *))task_entry, hdmirx_handle, name);
  if (IS_ERR(taskp))
    {
      return -ENOMEM;
    }

  /* Set scheduling settings */
  sched_policy               = thread_settings[0];
  sched_param.sched_priority = thread_settings[1];
  if ( sched_setscheduler(taskp, sched_policy, &sched_param) )
    {
      TRC(TRC_ID_ERROR, "FAILED to set thread scheduling parameters: name=%s, policy=%d, priority=%d", \
          name, sched_policy, sched_param.sched_priority);
    }

  wake_up_process(taskp);

  *thread = taskp;

  return ret;
}

/*****************************************************************************
 *@Name        : stm_hdmirx_thread_wait
 *@Description :  for task wait (deletion)

*****************************************************************************/
int stm_hdmirx_thread_wait(stm_hdmirx_thread *thread)
{
  int ret = -1;
  ret = kthread_stop(*thread);
  return ret;
}

/*****************************************************************************
 *@Name        : stm_hdmirx_sema_init
 *@Description : creation of semaphore
		with initial count = 0 or 1
*****************************************************************************/
int stm_hdmirx_sema_init(stm_hdmirx_semaphore **sema, unsigned int initialcount)
{
  if(sema == NULL)
    return -1;
  *sema = (stm_hdmirx_semaphore *) stm_hdmirx_malloc(sizeof(stm_hdmirx_semaphore));
  if (*sema != NULL)
    sema_init(*sema, initialcount);
  else
    return -ENOMEM;
  return 0;
}

/*****************************************************************************
 *@Name        : stm_hdmirx_sema_wait
 *@Description : waiting for semaphore release

*****************************************************************************/
int stm_hdmirx_sema_wait(stm_hdmirx_semaphore *sema)
{
  int ret = 0;
  ret = down_interruptible(sema);
  return ret;
}

/*****************************************************************************
 *@Name        : stm_hdmirx_sema_waittimeout
 *@Description : semaphore wait with timeout value

*****************************************************************************/
int stm_hdmirx_sema_wait_timeout(stm_hdmirx_semaphore *sema, long timeout)
{
  int ret = -1;

  ret = down_timeout(sema, timeout);
  return ret;
}

/*****************************************************************************
 *@Name        : stm_hdmirx_sema_signal
 *@Description :  release the semaphore

*****************************************************************************/
void stm_hdmirx_sema_signal(stm_hdmirx_semaphore *sema)
{
  if (sema != NULL)
    up(sema);
}

/******************************************************************************
 *@Name        : stm_hdmirx_sema_delete
 *@Description :  termination of semaphore

*****************************************************************************/
int stm_hdmirx_sema_delete(stm_hdmirx_semaphore *sema)
{
  stm_hdmirx_sema_signal(sema);
  return stm_hdmirx_free(sema);

}

/*****************************************************************************
 *@Name        : stm_hdmirx_malloc
 *@Description : Allocation of memmory according to size

*****************************************************************************/
void *stm_hdmirx_malloc(unsigned int size)
{
  void *add_p;
  if (size <= 128 * 1024)
    add_p = (void *)kmalloc(size, GFP_KERNEL);
  else
    add_p = vmalloc(size);

  if (add_p)
    memset(add_p, 0, size);
  return add_p;
}

/*****************************************************************************
 *@Name        : stm_hdmirx_free
 *@Description :free the memory

*****************************************************************************/
int stm_hdmirx_free(void *add)
{

  unsigned long address = (unsigned long)add;

  if (add == NULL)
    {
      printk(KERN_ERR "%s:Attempted to free NULL pointer \n",
             __func__);
      return -EINVAL;
    }
  /* if it is a vmalloc allocation */
  if ((unsigned int)address >= VMALLOC_START
      && (unsigned int)address < VMALLOC_END)
    {

      vfree(add);
    }
  else
    {
      kfree(add);
    }
  add = NULL;
  return 0;
}

/*****************************************************************************
 *@Name        : stm_hdmirx_delay_ms
 *@Description :  delay in ms
*****************************************************************************/
void stm_hdmirx_delay_ms(uint16_t mDelay)
{
  mdelay(mDelay);
}

/*****************************************************************************
 *@Name        : stm_hdmirx_delay_us
 *@Description :  delay in us
*****************************************************************************/

void stm_hdmirx_delay_us(uint16_t uDelay)
{
  udelay(uDelay);
}

/*****************************************************************************
 *@Name        : stm_hdmirx_time_now
 *@Description :
*****************************************************************************/
unsigned long stm_hdmirx_time_now(void)
{
  return (unsigned long)jiffies;
}

/*****************************************************************************
 *@Name        : stm_hdmirx_time_minus
 *@Description :
*****************************************************************************/

unsigned long stm_hdmirx_time_minus(unsigned long t1, unsigned long t2)
{
  unsigned long retVal;
  retVal = (unsigned long)((unsigned long)t1 - (unsigned long)t2);
  return retVal;
}

/*****************************************************************************
 *@Name        : stm_hdmirx_convert_ms_to_jiffies
 *@Description :  converted ms to jiffies
*****************************************************************************/
long stm_hdmirx_convert_ms_to_jiffies(uint16_t timeout)
{
  return msecs_to_jiffies(timeout);
}

/*****************************************************************************
 *@Name        : stm_hdmirx_pm_runtime_get
 *@Description : resume device
*****************************************************************************/
void stm_hdmirx_pm_runtime_get(struct device *dev)
{
#ifdef CONFIG_PM_RUNTIME
  pm_runtime_get_sync(dev);
#endif
}

/*****************************************************************************
 *@Name        : stm_hdmirx_pm_runtime_get
 *@Description : suspend device
*****************************************************************************/
void stm_hdmirx_pm_runtime_put(struct device *dev)
{
#ifdef CONFIG_PM_RUNTIME
  pm_runtime_put_sync(dev);
#endif
}
