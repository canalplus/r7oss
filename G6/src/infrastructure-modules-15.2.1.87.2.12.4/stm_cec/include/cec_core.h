/************************************************************************
Copyright (C) 2011 STMicroelectronics. All Rights Reserved.

This file is part of the stm_fe Library.

stm_cec is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License version 2 as published by the
Free Software Foundation.

stm_cec is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with stm_fe; see the file COPYING.  If not, write to the Free Software
Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

The stm_fe Library may alternatively be licensed under a proprietary
license from ST.

Source file name : cec_core.h
Author :           bharatj

API dedinitions for demodulation device

Date        Modification                                    Name
----        ------------                                    --------
05-Mar-12   Created                                         bharatj

************************************************************************/

#ifndef _CEC_CORE_H_
#define _CEC_CORE_H_

#include "stm_cec.h"
#include "infra_os_wrapper.h"
#include "cec_hal.h"

#define CEC_INAVLID_LOGICAL_ADDR		0
#define CEC_MSG_LIST_SIZE			5
#define CEC_DEV_NAME_SIZE			20
#define MAGIC_NUM_MASK				0xaabbcc00
#define CEC_ISR_W_Q_NAME			"INF-cecWorkQ"

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

/*******************control block*************************************/
typedef struct stm_cec_s{
	uint32_t			magic_num;  /* To validate the correct handle*/
	uint32_t			clk_freq;
	cec_state_t			cur_state;
	cec_state_t			next_state;
	unsigned char __iomem 		*base_addr_v;
	spinlock_t			intr_lock; /* port lock */
	infra_os_semaphore_t		tx_done_sema_p;
	infra_os_mutex_t		lock_cec_param_p;
	struct workqueue_struct		*w_q;
	struct work_struct		cec_isr_work;
	struct cec_hw_data_s		*cec_hw_data_p;
	struct stm_pad_state 		*pad_state;
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

infra_error_code_t cec_get_hw_container(uint32_t dev_id, struct cec_hw_data_s **cec_hw_data_p);
infra_error_code_t cec_validate_init_param(uint32_t dev_id, stm_cec_h *device);
infra_error_code_t cec_validate_del_param(stm_cec_h device);
infra_error_code_t cec_alloc_control_param(uint32_t dev_id, stm_cec_h *device);
infra_error_code_t cec_alloc_global_param(void);
infra_error_code_t cec_dealloc_global_param(void);
void cec_enter_critical_section(void);
void cec_exit_critical_section(void);
infra_error_code_t cec_dealloc_control_param(stm_cec_h device);
infra_error_code_t cec_fill_control_param(uint32_t dev_id ,stm_cec_t * cec_p);
infra_error_code_t cec_do_hw_init(uint32_t dev_id ,stm_cec_t * cec_p);
infra_error_code_t cec_do_hw_term(stm_cec_t * cec_p);
infra_error_code_t cec_retreive_message(stm_cec_t * cec_p,  stm_cec_msg_t *cec_msg_p);
infra_error_code_t cec_send_msg(stm_cec_t * cec_p,  stm_cec_msg_t *cec_msg_p);
infra_error_code_t cec_check_state(stm_cec_t * cec_p, cec_state_t desired_state);
infra_error_code_t   cec_core_set_ctrl(stm_cec_t *cec_p, stm_cec_ctrl_flag_t ctrl_flag, stm_cec_ctrl_type_t *ctrl_data_p);
infra_error_code_t   cec_core_get_ctrl(stm_cec_t *cec_p, stm_cec_ctrl_flag_t ctrl_flag, stm_cec_ctrl_type_t *ctrl_data_p);
int cec_core_suspend(struct cec_hw_data_s *hw_data_p, bool may_wakeup);
int cec_core_resume(struct cec_hw_data_s *hw_data_p);
#endif
