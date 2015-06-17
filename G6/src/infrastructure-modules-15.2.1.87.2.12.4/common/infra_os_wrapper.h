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
 @File   infra_os_wrappper.h
 @brief



*/

#ifndef INFRA_OS_WRAPPER_H_
#define INFRA_OS_WRAPPER_H_

#include <linux/version.h>

/* next line to check */
#include <linux/module.h>
#include <linux/kernel.h> /* Needed for KERN_ALERT */
#include <linux/init.h>   /* Needed for the macros */
/* end to check */

#include <linux/types.h>
#include <linux/fs.h>
#include <linux/mm.h>    /* for verify_area */
#include <linux/errno.h> /* for -EBUSY */

#include <asm/io.h>       /* for ioremap */
#include <asm/atomic.h>
#include <asm/system.h>

#include <linux/ioport.h> /* for ..._mem_region */

#include <linux/spinlock.h>

#include <linux/time.h>
#include <linux/jiffies.h>
#include <linux/delay.h>

#include <linux/irq.h>
#include <linux/signal.h>
#include <linux/sched.h>
#include <linux/interrupt.h>

#include <linux/wait.h>
#include <linux/poll.h>

#include <linux/slab.h>
#include <linux/vmalloc.h>

#include <linux/netdevice.h>    /* ??? SET_MODULE_OWNER ??? */
#include <linux/cdev.h>         /* cdev_alloc */

#include <asm/param.h>
#include <asm/current.h>
#include <asm/uaccess.h>
#include <asm/unistd.h>

#include <linux/rtmutex.h>
#include <linux/semaphore.h>
#include <linux/spinlock_types.h>

#include <linux/stddef.h>
#include <linux/types.h>
#include <linux/unistd.h>
#include <linux/string.h>

#include <linux/kthread.h>

#include <linux/hrtimer.h>
#include <linux/spinlock.h>

#include <linux/spinlock_types.h>
#include <linux/rwlock_types.h>
#include <linux/spinlock.h>


#define ERROR 1

#if ERROR
    #define infra_error_print(fmt, ...) pr_err(fmt,  ##__VA_ARGS__)
#else
    #define infra_error_print(fmt, ...)  while(0)
#endif

#define UNUSED_PARAMETER(x)		(void)(x)


#define INFRA_MAX_DEV_NAME               32

#define INFRA_NO_ERROR			0
#define INFRA_OS_NO_PRIORITY          0xffffffff
#define INFRA_OS_HIGHEST_PRIORITY     99
#define INFRA_OS_MID_PRIORITY         50
#define INFRA_OS_LOWEST_PRIORITY      0
#define INFRA_OS_INFINITE             ((infra_os_timeout_t)-1)
#define INFRA_OS_IMMEDIATE             ((infra_os_timeout_t)0)
#define INFRA_OS_INVALID_THREAD       ((infra_os_thread_t)0)
#define INFRA_OS_GET_TICKS_PER_SEC	HZ


/*uint8_t*/
#define     INFRA_READ_REG_BYTE(addr)			readb((void*)addr)
#define     INFRA_WRITE_REG_BYTE(addr, data)		writeb(data,(void*)addr)
#define     INFRA_SET_REGBITS_BYTE(addr, data )        	do {uint8_t a;\
								a = readb((void*)addr);\
								a |= data;\
								writeb(a, (void*)addr); \
							} while(0)
#define     INFRA_CLR_REGBITS_BYTE(addr,data)		do{ \
								uint8_t a;\
								a = readb((void*)addr);\
								a &= ~data;\
								writeb(a,(void*)addr); \
							}while(0)
#define     INFRA_CLEAR_SET_REGBITS_BYTE(addr,data_clr,data_set)		do{ \
										uint8_t a;\
										a = readb((void*)addr);\
										a &= ~data_clr;\
										a |= data_set;\
										writeb(a, (void*)addr); \
									} while(0)

/*U16*/
#define     INFRA_READ_REG_WORD(addr)			readl((void*)addr)
#define     INFRA_WRITE_REG_WORD(addr,data)		writel(data, (void*)addr)
#define     INFRA_SET_REGBITS_WORD(addr,data)		do{ \
								uint16_t a;\
								a = readl((void*)addr);\
								a |= data;\
								writel(a, (void*)addr); \
							}while(0);
#define     INFRA_CLR_REGBITS_WORD(addr,data)		do{ \
								uint16_t a;\
								a = readl((void*)addr);\
								a &= ((uint16_t)~data);\
								writel(a, (void*)addr); \
							}while(0)
#define     INFRA_CLR_SET_REGBITS_WORD(addr,data_clr,data_set)	 do{ \
										uint16_t a;\
										a = readl((void*)addr);\
										a &= ~data_clr;\
										a |= data_set;\
										writel(a, (void*)addr); \
									}while(0)



typedef char infra_dev_name_t[INFRA_MAX_DEV_NAME];
typedef int infra_error_code_t;

/* --- Useful type declarations --- */

/* --- not implemented in kernel version --- */
typedef unsigned int         	infra_os_MessageQueue_t;

//
typedef unsigned long		infra_os_time_t;
typedef int			infra_os_timeout_t;
typedef struct task_struct	*infra_os_thread_t;
typedef void			*infra_os_task_param_t;
typedef unsigned int		infra_os_task_priority_t;
typedef struct semaphore	*infra_os_semaphore_t;

#ifdef CONFIG_HIGH_RES_TIMERS
typedef struct infra_os_hrt_semaphore_s {
	struct semaphore sema;
	struct hrtimer hr_timer;
	uint32_t state;
	spinlock_t lock;
} *infra_os_hrt_semaphore_t;
#else
typedef struct semaphore        *infra_os_hrt_semaphore_t;
#endif

typedef struct rt_mutex		*infra_os_mutex_t;
typedef wait_queue_head_t	infra_os_waitq_t;
typedef spinlock_t		infra_os_spinlock_t;
typedef rwlock_t		infra_os_rwlock_t;


typedef void	(*infra_os_thread_fn_t)( void   *parameter );
typedef void 	*(*infra_os_task_entry_t)( void* parameter );

#define infra_os_task_entry( fn )   void  *fn( void* parameter )

/* --- Useful macro's --- */

#define strerror( x )                   "Unknown error"

typedef struct infra_os_thread_info_s {
	infra_os_thread_fn_t	thread;
	void			*args;
	unsigned int		priority;
}infra_os_thread_info_t;


#ifdef __cplusplus
extern "C" {
#endif

// --------------------------------------------------------------
//      The Memory functions

void			*infra_os_malloc_d( unsigned int Size , const char *, uint32_t);
infra_error_code_t	infra_os_free_d( void *Address , const char *, uint32_t);

#define infra_os_malloc(x)	infra_os_malloc_d(x,__FUNCTION__,__LINE__)
#define infra_os_free(x)	infra_os_free_d(x,__FUNCTION__,__LINE__)

void          infra_os_invalidate_cache_range(void *cpu_addr, uint32_t size );
void          infra_os_flush_cache_range(void *cpu_addr,  uint32_t size );
void          infra_os_purge_cache_range(void *cpu_addr,  uint32_t size );
void         *infra_os_peripheral_address(void *cpu_addr);

// --------------------------------------------------------------
//      The Semaphore functions

infra_error_code_t   infra_os_sema_initialize(infra_os_semaphore_t *sema,
				uint32_t initial_count);
infra_error_code_t   infra_os_sema_terminate(infra_os_semaphore_t semaphore);
infra_error_code_t   infra_os_sema_wait(infra_os_semaphore_t semaphore);
infra_error_code_t   infra_os_sema_wait_timeout(infra_os_semaphore_t semaphore,
				infra_os_timeout_t timeout);
infra_error_code_t   infra_os_sema_signal(infra_os_semaphore_t semaphore );

#ifdef CONFIG_HIGH_RES_TIMERS
infra_error_code_t   infra_os_hrt_sema_initialize(infra_os_hrt_semaphore_t *sema,
				uint32_t initial_count);
infra_error_code_t   infra_os_hrt_sema_terminate(infra_os_hrt_semaphore_t semaphore);
infra_error_code_t   infra_os_hrt_sema_wait_timeout(infra_os_hrt_semaphore_t semaphore,
				infra_os_timeout_t timeout);
infra_error_code_t   infra_os_hrt_sema_signal(infra_os_hrt_semaphore_t semaphore );
infra_error_code_t   infra_os_hrt_sema_wait(infra_os_hrt_semaphore_t semaphore);

#else

static inline int infra_os_hrt_sema_initialize(infra_os_hrt_semaphore_t *sema,
                                 uint32_t initial_count)
{
	return infra_os_sema_initialize(sema, initial_count);
}

static inline int infra_os_hrt_sema_terminate(infra_os_hrt_semaphore_t sema)
{
	return infra_os_sema_terminate(sema);
}

static inline int infra_os_hrt_sema_wait_timeout(infra_os_hrt_semaphore_t sema,
                                 infra_os_timeout_t timeout)
{
	return infra_os_sema_wait_timeout(sema, timeout);
}

static inline int  infra_os_hrt_sema_signal(infra_os_hrt_semaphore_t sema)
{
	return infra_os_sema_signal(sema);
}

static inline int infra_os_hrt_sema_wait(infra_os_hrt_semaphore_t sema)
{
	return infra_os_sema_wait(sema);
}

#endif
// --------------------------------------------------------------
//      The Mutex functions
infra_error_code_t   infra_os_mutex_initialize(infra_os_mutex_t  *mutex );
infra_error_code_t   infra_os_mutex_terminate(infra_os_mutex_t mutex );
infra_error_code_t   infra_os_mutex_lock(infra_os_mutex_t mutex );
infra_error_code_t   infra_os_mutex_unlock(infra_os_mutex_t mutex );


//WAIT Qs
#define infra_os_wait_for_queue(wait_q_p, condition, timeout)		\
{								\
    if (timeout == INFRA_OS_INFINITE)				\
	wait_event_interruptible(wait_q_p, condition );					\
    else								\
	wait_event_interruptible_timeout(wait_q_p, condition, msecs_to_jiffies (timeout) );	\
}



infra_error_code_t    infra_os_wait_queue_wakeup(infra_os_waitq_t  *wait_q_p , bool interruptible);
infra_error_code_t    infra_os_wait_queue_initialize(infra_os_waitq_t *wait_queue );
infra_error_code_t    infra_os_wait_queue_reinitialize(infra_os_waitq_t *wait_queue );
infra_error_code_t    infra_os_wait_queue_deinitialize(infra_os_waitq_t *wait_queue );

#define infra_os_spin_lock_init(lock)	spin_lock_init(lock)
#define infra_os_spin_lock_irqsave(lock, flag)	spin_lock_irqsave(lock, flag)
#define infra_os_spin_unlock_irqrestore(lock, flag)  \
					spin_unlock_irqrestore(lock, flag)


#define infra_os_rwlock_init(lock)	\
			rwlock_init(lock)

#define infra_os_write_lock_irqsave(lock, flag)	\
			write_lock_irqsave(lock, flag)

#define infra_os_write_unlock_irqrestore(lock, flag)	\
			write_unlock_irqrestore(lock, flag)

#define infra_os_read_lock(lock)	\
			read_lock(lock)

#define infra_os_read_unlock(lock)	\
			read_unlock(lock)




infra_error_code_t   infra_os_thread_create(	infra_os_thread_t *thread,
						infra_os_thread_fn_t	task_entry,
						infra_os_task_param_t	parameter,
						const char		*name,
						infra_os_task_priority_t *priority );
void infra_os_thread_terminate( void );
infra_error_code_t   infra_os_set_priority( infra_os_task_priority_t priority );
infra_error_code_t  infra_os_wait_thread ( infra_os_thread_t* task);
infra_error_code_t  infra_os_task_exit ( void );
bool infra_os_should_task_exit(void);

unsigned int  infra_os_get_time_in_sec( void );
unsigned int	infra_os_get_time_in_milli_sec( void );
void		infra_os_sleep_milli_sec( unsigned int Value );

void infra_os_sleep_usec(unsigned long usecs_min, unsigned long usecs_max);

bool	infra_os_copy(void *des_p, void *src_p, uint32_t bytes_to_copy);
infra_os_time_t infra_os_time_now(void);

#endif
