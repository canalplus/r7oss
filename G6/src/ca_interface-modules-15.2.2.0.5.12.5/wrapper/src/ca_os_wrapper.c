/*****************************************************************************/
/* COPYRIGHT (C) 2011 STMicroelectronics - All Rights Reserved               */
/* ST makes no warranty express or implied including but not limited to,     */
/* any warranty of                                                           */
/*                                                                           */
/*   (i)  merchantability or fitness for a particular purpose and/or         */
/*   (ii) requirements, for a particular purpose in relation to the LICENSED */
/*        MATERIALS, which is provided AS IS, WITH ALL FAULTS. ST does not */
/*        represent or warrant that the LICENSED MATERIALS provided here     */
/*        under is free of infringement of any third party patents,          */
/*        copyrights,trade secrets or other intellectual property rights.    */
/*        ALL WARRANTIES, CONDITIONS OR OTHER TERMS IMPLIED BY LAW ARE       */
/*        EXCLUDED TO THE FULLEST EXTENT PERMITTED BY LAW                    */
/*                                                                           */
/*****************************************************************************/
/**
 @File   ca_os_wrappper.c
 @brief



*/
#include "ca_os_wrapper.h"
#include "linux/debugfs.h"


/* --- Taskname table --- */

#define CA_OS_MAX_NAME_SIZE    32
#define CA_TIMER_GRANUALITY  ((1000/HZ) -1)  /* Timer granularity */

#define CA_OS_DEBUG_MEMORY_STATUS	0
#define CA_OS_DEBUG_EXTRA_META_BYTES	4
// --------------------------------------------------------------
// Malloc and free functions

#if CA_OS_DEBUG_MEMORY_STATUS
uint32_t		total_mem_allocated;
#endif

void *ca_os_malloc_d( unsigned int size, const char *func, uint32_t line)
{
	void *p;
#if CA_OS_DEBUG_MEMORY_STATUS
	if( size <= 128*1024 )
		p = (void *)kmalloc((size+CA_OS_DEBUG_EXTRA_META_BYTES),
				GFP_KERNEL);
	else
		p = vmalloc((size+CA_OS_DEBUG_EXTRA_META_BYTES));

	if(p){
		*((int *)p)=size;
		total_mem_allocated +=  *((int *)p);
		p = (void*)((uint8_t *)p + 4);
		memset(p,0,size);
	}
	printk(">%s@%d>,M:,%u, AS:,%d ,S:,%d,\n",
			func,
			line,
			(uint32_t)p,
			total_mem_allocated,
			size);
#else
	if( size <= 128*1024 )
		p = (void *)kmalloc( size, GFP_KERNEL );
	else
		p = vmalloc( size );

	if(p)
		memset(p,0,size);
#endif
	return p;
}


int ca_os_free_d(void *Address , const char *func, uint32_t line)
{
	unsigned long  Addr = (unsigned long)Address;
#if CA_OS_DEBUG_MEMORY_STATUS
	void * temp_addr = ((char *)Address) - CA_OS_DEBUG_EXTRA_META_BYTES;
	uint32_t actual_size= 0;
#endif

	if( Address == NULL ){
		printk( "ca_os_Free- Attempt to free null pointer>> %s@%d\n",
				func,
				line);
		return -EINVAL;
	}
#if CA_OS_DEBUG_MEMORY_STATUS
	total_mem_allocated -= *((int *)temp_addr);
	actual_size = *((int *)temp_addr);
	printk("<%s@%d<,M:,%u, AS:,%d, S:,%d,\n",
			func,
			line,
			(uint32_t)Address,
			total_mem_allocated,
			actual_size);
	if( (unsigned int)Addr >= VMALLOC_START &&
			(unsigned int)Addr < VMALLOC_END ){
		/* We found the address in the table,
		 * it is a vmalloc allocation */
		vfree(temp_addr);
	}
	else{
		/* Address not found... trying kfree ! */
		kfree(temp_addr);
	}
#else
	if( (unsigned int)Addr >= VMALLOC_START &&
			(unsigned int)Addr < VMALLOC_END ){
		/* We found the address in the table,
		 * it is a vmalloc allocation */
		vfree(Address);
	}
	else{
		/* Address not found... trying kfree ! */
		kfree(Address);
	}
#endif
	return 0;

}


inline bool ca_os_copy(void *des_p, void *src_p, uint32_t bytes_to_copy)
{
	memcpy(des_p, src_p, bytes_to_copy);
	return true;
}
// --------



/* The Semaphore functions
 */
int ca_os_sema_initialize(ca_os_semaphore_t  *sema,
		uint32_t     initial_count )
{
	*sema = (ca_os_semaphore_t)ca_os_malloc(
			sizeof(struct semaphore));
	if (*sema != NULL) {
		sema_init(*sema, initial_count);
		return 0;
	} else
		return -ENOMEM;
}

int ca_os_sema_terminate(ca_os_semaphore_t  sema)
{
	ca_os_free((void *)sema);
	return 0;
}


int ca_os_sema_wait_timeout(ca_os_semaphore_t sema,
		ca_os_timeout_t timeout)
{
	int ret = -1;
	unsigned long timeout_jiffies =0;

	if (CA_OS_IMMEDIATE == timeout) {
		/* down_trylock() returns 0 if we got the lock, 1 if not.
		 * We must return 0 if we got the lock, -1 if not.
		 */
		ret = down_trylock(sema) ? -1 : 0;
	} else if (CA_OS_INFINITE == timeout) {
		ret = ca_os_sema_wait(sema);
		/* No SEMA_ADD_TRACE since it it managed into semaphore_wait(Semaphore_p) */
	} else {

		timeout_jiffies = msecs_to_jiffies(timeout);

		if (timeout >= 0) {
			ret = down_timeout(sema, timeout_jiffies);
		} else {
			printk("\n\n\nWARNING!!!\nTimeout may have looped (%d) ...\n"\
					"Please check ...\n\n\n",(int)timeout);
			/* Absolute time has already expired, return error ... */
			ret = -EINVAL;
		}
	}

	return ret;
}
int ca_os_sema_wait(ca_os_semaphore_t  sema)
{
	int ret = -1;

	ret = down_interruptible(sema);
	return ret;
}

int ca_os_sema_signal(ca_os_semaphore_t  sema)
{
	up(sema);
	return 0;
}
#ifdef CONFIG_HIGH_RES_TIMERS
#define MS_TO_NS(x)     (x * 1000000)
enum hrtimer_restart hrtimer_callback(struct hrtimer *timer)
{
	unsigned long flags;
	ca_os_hrt_semaphore_t semaphore =
		container_of(timer,
				struct ca_os_hrt_semaphore_s,
				hr_timer);
	spin_lock_irqsave(&semaphore->lock, flags);
	semaphore->state = HRTIMER_STATE_CALLBACK;
	if (semaphore->state != HRTIMER_STATE_INACTIVE)
		up(&(semaphore->sema));
	spin_unlock_irqrestore(&semaphore->lock, flags);
	return HRTIMER_NORESTART;
}


int ca_os_hrt_sema_initialize(ca_os_hrt_semaphore_t  *semaphore,
		uint32_t     initial_count )
{
	*semaphore = (ca_os_hrt_semaphore_t)ca_os_malloc(
			sizeof(struct ca_os_hrt_semaphore_s));
	if (*semaphore != NULL) {
		sema_init(&((*semaphore)->sema), initial_count);
		hrtimer_init(&((*semaphore)->hr_timer),
				CLOCK_MONOTONIC,
				HRTIMER_MODE_REL);
		(*semaphore)->hr_timer.function = &hrtimer_callback;
		spin_lock_init(&((*semaphore)->lock));
		return 0;
	} else
		return -ENOMEM;
}

int ca_os_hrt_sema_terminate(ca_os_hrt_semaphore_t  semaphore)
{
	int32_t ret = 0;
	ret = hrtimer_cancel(&(semaphore->hr_timer));
	ca_os_free((void *)semaphore);
	return ret;
}

int ca_os_hrt_sema_wait_timeout(ca_os_hrt_semaphore_t semaphore,
		ca_os_timeout_t timeout)
{
	unsigned long sec, nsec;
	int ret = -1;
	ktime_t ktime;
	if (CA_OS_IMMEDIATE == timeout) {
		/* down_trylock() returns 0 if we got the lock, 1 if not.
		 * * We must return 0 if we got the lock, -1 if not.
		 **/
		ret = down_trylock(&(semaphore->sema)) ? -1 : 0;
	} else if (CA_OS_INFINITE == timeout) {
		ret = ca_os_sema_wait(&semaphore->sema);
		/* No SEMA_ADD_TRACE since it is
		 * managed into semaphore_wait(Semaphore_p)*/
	} else {
		unsigned long flags;

		sec = timeout / 1000;
		nsec = (timeout % 1000) * 1000000;
		ktime = ktime_set(sec, nsec);

		spin_lock_irqsave(&semaphore->lock, flags);
		semaphore->state = HRTIMER_STATE_ENQUEUED;
		hrtimer_start(&(semaphore->hr_timer), ktime, HRTIMER_MODE_REL);
		spin_unlock_irqrestore(&semaphore->lock, flags);

		ret = ca_os_sema_wait(&semaphore->sema);
		if (ret) {
			spin_lock_irqsave(&semaphore->lock, flags);
			hrtimer_cancel(&(semaphore->hr_timer));
			semaphore->state = HRTIMER_STATE_INACTIVE;
			spin_unlock_irqrestore(&semaphore->lock, flags);
		}

		spin_lock_irqsave(&semaphore->lock, flags);
		if (semaphore->state == HRTIMER_STATE_CALLBACK) {
			spin_unlock_irqrestore(&semaphore->lock, flags);
			return -ETIME;
		}
		spin_unlock_irqrestore(&semaphore->lock, flags);
	}
	return ret;
}

int ca_os_hrt_sema_wait(ca_os_hrt_semaphore_t  semaphore)
{
	int ret = -1;
	semaphore->state = HRTIMER_STATE_INACTIVE;
	ret = down_interruptible(&(semaphore->sema));
	return ret;
}

int ca_os_hrt_sema_signal(ca_os_hrt_semaphore_t  semaphore)
{
	unsigned long flags;
	spin_lock_irqsave(&semaphore->lock, flags);
	if (semaphore->state == HRTIMER_STATE_ENQUEUED) {
		hrtimer_cancel(&(semaphore->hr_timer));
		semaphore->state = HRTIMER_STATE_INACTIVE;
	}
	spin_unlock_irqrestore(&semaphore->lock, flags);
	up(&(semaphore->sema));
	return 0;
}
#endif

int ca_os_mutex_initialize(ca_os_mutex_t  *mutex)
{
	*mutex = ca_os_malloc( sizeof(**mutex) );
	if (*mutex != NULL) {
		rt_mutex_init(*mutex);
		return 0;
	} else
		return -ENOMEM;
}

int ca_os_mutex_terminate(ca_os_mutex_t  mutex)
{
	rt_mutex_destroy(mutex);
	ca_os_free((void*)mutex);
	return 0;
}


int ca_os_mutex_lock(ca_os_mutex_t  mutex)
{
	int ret;
	ret = rt_mutex_lock_interruptible(mutex, true);
	if (-EDEADLK == ret) {
		printk(KERN_ERR "%p will cause a deadlock\n", mutex);
		return ret;
	}
	return 0;
}

int ca_os_mutex_unlock(ca_os_mutex_t  mutex)
{
	rt_mutex_unlock(mutex);
	return 0;
}

int ca_os_wait_queue_wakeup(ca_os_waitq_t  *wait_q_p, bool interruptible)
{
	if (interruptible)
		wake_up_interruptible(wait_q_p);
	else
		wake_up( wait_q_p );


	return 0;
}

int ca_os_wait_queue_initialize(ca_os_waitq_t *wait_queue)
{
	if (wait_queue != NULL) {
		init_waitqueue_head(wait_queue);
		return 0;
	} else
		return -ENOMEM;
}

int ca_os_wait_queue_reinitialize(ca_os_waitq_t *wait_queue)
{
	init_waitqueue_head(wait_queue);
	return 0;
}

int ca_os_wait_queue_deinitialize(ca_os_waitq_t *wait_queue)
{
	return 0;
}

int ca_os_thread_create(ca_os_thread_t *thread,
		void (*task_entry)(void* param),
		ca_os_task_param_t parameter,
		const char         *name,
		ca_os_task_priority_t * priority)
{
	struct sched_param	param;
	struct task_struct		*taskp;

	/* create the task and leave it suspended */
	taskp = kthread_create((int(*)(void *))task_entry,
			parameter, "%s", name);
	if (IS_ERR(taskp))
		return -ENOMEM;

	taskp->flags |= PF_NOFREEZE;

	/* switch to real time scheduling (if requested) */
	if (priority[1]) {
		param.sched_priority = priority[1];
		if (0 != sched_setscheduler(taskp, priority[0], &param)) {
			printk(KERN_ERR "FAILED to set scheduling parameters to priority %d mode (%d)\n", priority[1],priority[0]);
		}
	}

	wake_up_process(taskp);

	*thread = taskp;

	return 0;
}


void   ca_os_thread_terminate(void)
{
	return;
}

int  ca_os_task_enter (void)
{
	yield();
	return 0;
}

int  ca_os_task_exit (void)
{
	while (!kthread_should_stop())
		schedule_timeout_interruptible(100); /* 100 jiffies */
	return 0;
}

int  ca_os_wait_thread(ca_os_thread_t* task)
{

	if (NULL == *task) {
		ca_error_print("FATAL ERROR: Task cannot be null\n");
		return -EINVAL;
	}
	kthread_stop(*task);
	*task = NULL;
	return 0;
}

unsigned int ca_os_get_time_in_sec( void )
{
	struct timeval  now;

#if !defined (CONFIG_USE_SYSTEM_CLOCK)
	ktime_t         ktime   = ktime_get();
	now                 = ktime_to_timeval(ktime);
#else
	do_gettimeofday( &now );
#endif

	return now.tv_sec;
}

unsigned int ca_os_get_time_in_milli_sec( void )
{
	return (unsigned int)(((jiffies )* 1000)/HZ);
}



void ca_os_sleep_milli_sec( unsigned int Value )
{
	sigset_t allset, oldset;
	unsigned int  Jiffies = ((Value*HZ)/1000)+1;

	/* Block all signals during the sleep (i.e. work correctly when called via ^C) */
	sigfillset(&allset);
	sigprocmask(SIG_BLOCK, &allset, &oldset);

	/* Sleep */
	set_current_state( TASK_INTERRUPTIBLE );
	schedule_timeout( Jiffies );

	/* Restore the original signal mask */
	sigprocmask(SIG_SETMASK, &oldset, NULL);
}

void ca_os_sleep_usec(unsigned long usecs_min, unsigned long usecs_max)
{
	/* SLEEPING FOR "A FEW" USECS :do busy wait */
	if (usecs_max <= 10)
		udelay(usecs_max);
	else
		usleep_range(usecs_min, usecs_max);
}

ca_os_time_t ca_os_time_now()
{
	/*currently not supported*/
	return 0;
}
