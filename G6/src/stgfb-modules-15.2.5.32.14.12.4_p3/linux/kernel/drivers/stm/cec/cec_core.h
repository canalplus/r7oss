/***********************************************************************
 *
 * File: linux/kernel/drivers/stm/cec/cec_core.h
 * Copyright (c) 2005-2014 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef _CEC_CORE_H_
#define _CEC_CORE_H_
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

#include "stm_cec.h"
#include "cec_hal.h"


#define CEC_INAVLID_LOGICAL_ADDR		0
#define CEC_MSG_LIST_SIZE			5
#define CEC_DEV_NAME_SIZE			20
#define MAGIC_NUM_MASK				0xaabbcc00
#define CEC_ISR_W_Q_NAME			"VIBE-cecWorkQ"

#define CEC_VALIDATE_HANDLE(magic_num)		(magic_num>>8) ^ (0x00aabbcc)
#define CEC_GET_INIT_LOGICAL_ADDR(msg)		((msg>>4)&0xF)
#define CEC_GET_FOLL_LOGICAL_ADDR(msg)		(msg&0xF)

typedef enum{
	CEC_STATE_OPEN,
	CEC_STATE_QUERY,
	CEC_STATE_READY,
	CEC_STATE_BUSY,
	CEC_STATE_CLOSE
}cec_state_t;

typedef enum{
	CEC_STATUS_TX_MSG_LOADED,
	CEC_STATUS_TX_IN_PROCESS,
	CEC_STATUS_MSG_BROADCAST,
	CEC_STATUS_MSG_PING,
	CEC_STATE_MAX
}cec_status_flag_t;

struct cec_hw_data_s
{
	uint8_t				dev_name[CEC_DEV_NAME_SIZE];
	struct stm_cec_platform_data	*platform_data;
	struct resource			*base_address;
	struct resource			r_irq;
	struct clk			*clk;
	bool				init;
	struct cec_hw_data_s		*next_p;
	void				*cec_ctrl_p;
};

struct cec_hal_isr_param_s{
	uint16_t	isr_status;
	uint16_t	msg_status;
	uint8_t		exit;
	uint16_t	notify;
};

typedef struct cec_q_s{
	void			*data_p; /*Pointer to the actual node whose linked list is constructed*/
	uint32_t			key; /*key to locate any node. helps in searching and deltion*/
	struct cec_q_s		*next_p; /*Pointer to the next Q element*/
}cec_q_t;

#define CEC_Q_GET_DATA(q_p, type)		(type*)(q_p->data_p)
#define CEC_Q_SET_DATA(q_p, data_p)		q_p->node_data_p = (void*)data_p
#define CEC_Q_GET_NEXT(q_p)			q_p->next_p

#define     CEC_READ_REG_BYTE(addr)			readb((void __iomem *)addr)
#define     CEC_WRITE_REG_BYTE(addr, data)		writeb(data,(void __iomem *)addr)
#define     CEC_SET_REGBITS_BYTE(addr, data )        	do {uint8_t a;\
								a = readb((void __iomem *)addr);\
								a |= data;\
								writeb(a, (void __iomem *)addr); \
							} while(0)
#define     CEC_CLR_REGBITS_BYTE(addr,data)		do{ \
								uint8_t a;\
								a = readb((void __iomem *)addr);\
								a &= ~data;\
								writeb(a,(void __iomem *)addr); \
							}while(0)
#define     CEC_CLEAR_SET_REGBITS_BYTE(addr,data_clr,data_set)		do{ \
										uint8_t a;\
										a = readb((void __iomem *)addr);\
										a &= ~data_clr;\
										a |= data_set;\
										writeb(a, (void __iomem *)addr); \
									} while(0)

/*U16*/
#define     CEC_READ_REG_WORD(addr)			readl((void __iomem *)addr)
#define     CEC_WRITE_REG_WORD(addr,data)		writel(data, (void __iomem *)addr)
#define     CEC_SET_REGBITS_WORD(addr,data)		do{ \
								uint16_t a;\
								a = readl((void __iomem *)addr);\
								a |= data;\
								writel(a, (void __iomem *)addr); \
							}while(0);
#define     CEC_CLR_REGBITS_WORD(addr,data)		do{ \
								uint16_t a;\
								a = readl((void __iomem *)addr);\
								a &= ((uint16_t)~data);\
								writel(a, (void __iomem *)addr); \
							}while(0)
#define     CEC_CLR_SET_REGBITS_WORD(addr,data_clr,data_set)	 do{ \
										uint16_t a;\
										a = readl((void __iomem *)addr);\
										a &= ~data_clr;\
										a |= data_set;\
										writel(a, (void __iomem *)addr); \
									}while(0)
/*******************control block*************************************/
typedef struct stm_cec_s{
	uint32_t			magic_num;  /* To validate the correct handle*/
	uint32_t			clk_freq;
	cec_state_t			cur_state;
	cec_state_t			next_state;
	unsigned char __iomem 		*base_addr_v;
	spinlock_t			intr_lock; /* port lock */
	struct semaphore		*tx_done_sema_p;
	struct rt_mutex *		lock_cec_param_p;
	struct workqueue_struct		*w_q;
	struct work_struct		cec_isr_work;
	struct cec_hw_data_s		*cec_hw_data_p;
#ifndef CONFIG_ARCH_STI
	struct stm_pad_state		*pad_state;
#endif
	struct cec_hal_isr_param_s	isr_param;
	uint16_t			logical_addr_set;
	uint8_t				cur_logical_addr;
	uint8_t				prev_logical_addr;
	uint8_t				tx_msg_state;
	bool				status_arr[CEC_STATE_MAX];
	cec_hal_frame_status_t		frame_status;   /* CEC Frame status controls*/
	cec_hal_bus_status_t		bus_status;   /* CEC BUS Status controsl*/
	uint8_t				retries;
	uint8_t				trials;
	int32_t				msg_list_head;
	int32_t				msg_list_tail;
	stm_cec_msg_t			message_list[CEC_MSG_LIST_SIZE];
	uint8_t				auto_ack_for_broadcast_tx;
}stm_cec_t;

int cec_get_hw_container(uint32_t dev_id, struct cec_hw_data_s **cec_hw_data_p);
int cec_validate_init_param(uint32_t dev_id, stm_cec_h *device);
int cec_validate_del_param(stm_cec_h device);
int cec_alloc_control_param(uint32_t dev_id, stm_cec_h *device);
int cec_alloc_global_param(void);
int cec_dealloc_global_param(void);
void cec_enter_critical_section(void);
void cec_exit_critical_section(void);
int cec_dealloc_control_param(stm_cec_h device);
int cec_fill_control_param(uint32_t dev_id ,stm_cec_t * cec_p);
int cec_do_hw_init(uint32_t dev_id ,stm_cec_t * cec_p);
int cec_do_hw_term(stm_cec_t * cec_p);
int cec_retreive_message(stm_cec_t * cec_p,  stm_cec_msg_t *cec_msg_p);
int cec_send_msg(stm_cec_t * cec_p,  stm_cec_msg_t *cec_msg_p);
int cec_check_state(stm_cec_t * cec_p, cec_state_t desired_state);
int cec_core_set_ctrl(stm_cec_t *cec_p, stm_cec_ctrl_flag_t ctrl_flag, stm_cec_ctrl_type_t *ctrl_data_p);
int cec_core_get_ctrl(stm_cec_t *cec_p, stm_cec_ctrl_flag_t ctrl_flag, stm_cec_ctrl_type_t *ctrl_data_p);
int cec_core_suspend(struct cec_hw_data_s *hw_data_p, bool may_wakeup);
int cec_core_resume(struct cec_hw_data_s *hw_data_p);
void * cec_allocate (unsigned int size );
int cec_free(void *Address);
int cec_mutex_initialize(struct rt_mutex ** mutex);
int cec_mutex_terminate (struct rt_mutex  * mutex);
int cec_sema_initialize(struct semaphore  **sema, uint32_t InitialCount );
int cec_sema_terminate(struct semaphore  *sema);
int cec_sema_wait(struct semaphore  *sema);
int cec_sema_signal(struct semaphore   *sema);

#endif
