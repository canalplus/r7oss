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

Source file name : cec_core.c
Author :           bharatj

API dedinitions for demodulation device

Date        Modification                                    Name
----        ------------                                    --------
05-Mar-12   Created                                         bharatj

************************************************************************/
#include <linux/stm/pad.h>
#include <linux/clk.h>
#ifdef CONFIG_STM_LPM
#include <linux/stm/lpm.h>
#endif
#include "infra_platform.h"
#include "cec_core.h"
#include "stm_cec.h"
#include "cec_hal.h"
#include "stm_event.h"
#include "infra_debug.h"
#include "cec_debug.h"

/****************************LOCAL MACROS*************************************/
#define CEC_DEVICE_MAX 2
/*HACK: Need to be dynamic as per platform through platform_data*/
/*****************************************************************************/

/***************************LOCAL GLOBALS*************************************/
static stm_cec_t *g_cec_h_arr[CEC_DEVICE_MAX];
static struct cec_hw_data_s g_cec_hw_data[CEC_DEVICE_MAX];
static struct rt_mutex *g_lock;
static struct rt_mutex cec_global_lock;
/*****************************************************************************/

/*************************LOCAL FUNCTIONS*************************************/
static int32_t cec_pio_init(uint32_t dev_id , stm_cec_t *cec_p);

static bool  cec_hal_check_tx_status(stm_cec_t *cec_p,
				cec_hal_msg_status *tx_status_p);

static void cec_isr_bottom_half(struct work_struct *work);

static int cec_set_logical_addr(stm_cec_t *cec_p,
				stm_cec_ctrl_type_t *ctrl_data_p);

static int cec_get_logical_addr(stm_cec_t *cec_p,
				stm_cec_ctrl_type_t *ctrl_data_p);

static bool cec_core_is_ping_msg(stm_cec_t *cec_p,
				stm_cec_msg_t *msg_p);
/*****************************************************************************/


static int32_t cec_pio_init(uint32_t dev_id, stm_cec_t *cec_p)
{
	uint8_t owner[32];
	struct stm_pad_config *pad_config_p;

	pad_config_p = &(cec_p->cec_hw_data_p->platform_data->pad_config[0]);
	memset((void *)owner, 0, 32);
	sprintf(owner, "%s-%d", "CEC", dev_id);
	cec_p->pad_state = stm_pad_claim(pad_config_p, owner);
	if (NULL == cec_p->pad_state) {
		cec_error_trace(CEC_API,
				"PIO Pad claim failed %d\n",
				__LINE__);
		return -EACCES;
	}

	return INFRA_NO_ERROR;
}

inline void cec_enter_critical_section(void)
{
	infra_os_mutex_lock(g_lock);
}

inline void cec_exit_critical_section(void)
{
	infra_os_mutex_unlock(g_lock);
}

int cec_alloc_global_param(void)
{
	g_lock = &cec_global_lock;
	rt_mutex_init(g_lock);
	return  0;
}

int cec_dealloc_global_param(void)
{
	return 0;
}

int cec_get_hw_container(uint32_t dev_id,
			struct cec_hw_data_s **cec_hw_data_p)
{
	*cec_hw_data_p = &g_cec_hw_data[dev_id];
	return 0;
}

int cec_core_suspend(struct cec_hw_data_s *hw_data_p, bool may_wakeup)
{
	stm_cec_t		*cec_p;
#ifdef CONFIG_STM_LPM
	struct stm_lpm_cec_address cec_addr;
	int err;
#endif
	cec_p = (stm_cec_t *)hw_data_p->cec_ctrl_p;
	if (cec_p != NULL && hw_data_p->init == true) {
#ifdef CONFIG_STM_LPM
		cec_addr.logical_addr = cec_p->logical_addr_set;
		err = stm_lpm_set_cec_addr(&cec_addr);
		if (err < 0)
			cec_error_trace(CEC_API, "Firmware not responding %d\n",__LINE__);
#endif
		cec_hal_suspend(cec_p, may_wakeup);
	}
	return 0;
}

int cec_core_resume(struct cec_hw_data_s *hw_data_p)
{
	stm_cec_t		*cec_p;
	cec_p = (stm_cec_t *)hw_data_p->cec_ctrl_p;
	if (cec_p != NULL && hw_data_p->init == true)
		cec_hal_resume(cec_p);

	return 0;
}

int cec_validate_init_param(uint32_t dev_id, stm_cec_h *device)
{
	struct cec_hw_data_s *cec_hw_data_p;

	if (device == NULL) {
		cec_error_trace(CEC_API, "Invalid Params\n");
		return -EINVAL;
	}

	if ((g_cec_h_arr[dev_id]) != NULL) {
		cec_error_trace(CEC_API,
				"CEC Instance(%d) already initialized\n",
				dev_id);
		return -EBUSY;
	}

	cec_hw_data_p = &g_cec_hw_data[dev_id];
	if (cec_hw_data_p->base_address == NULL) {
		cec_error_trace(CEC_API,
				"CEC Device(%d) not supported\n",
				dev_id);
		return -ENODEV;
	}

	if (dev_id >= CEC_DEVICE_MAX) {
		cec_error_trace(CEC_API, "Invalid Device Id\n");
		return -ENODEV;
	}
	return 0;
}

int cec_validate_del_param(stm_cec_h device)
{
	uint8_t			dev_id;

	if (device == NULL) {
		cec_error_trace(CEC_API,
				"INVALID PARAM[handle=%p]\n",
				device);
		return -EINVAL;
	}

	if ((CEC_VALIDATE_HANDLE(device->magic_num)) != 0) {
		cec_error_trace(CEC_API,
				"INVALID HANDLE[handle=%p ]\n",
				device);
		return -ENODEV;
	}

	dev_id = device->magic_num & 0xF;
	if (dev_id >= CEC_DEVICE_MAX)
		return -EINVAL;


	if ((g_cec_h_arr[dev_id]) == NULL) {
		cec_error_trace(CEC_API,
		"CEC Instance(%d) already deleted\n", dev_id);
		return -EBUSY;
	}

	return 0;
}


int cec_alloc_control_param(uint32_t dev_id, stm_cec_h *device)
{
	stm_cec_t *cec_p;
	int error;

	cec_p = infra_os_malloc(sizeof(stm_cec_t));
	if (cec_p == NULL) {
		cec_error_trace(CEC_API, "CEC control block alloc failed\n");
		return -ENOMEM;
	}
	(g_cec_h_arr[dev_id]) = cec_p;
	*device =  cec_p;

	error = infra_os_sema_initialize(&(cec_p->tx_done_sema_p), 0);
	error |= infra_os_mutex_initialize(&(cec_p->lock_cec_param_p));
	spin_lock_init(&cec_p->intr_lock);

	INIT_WORK(&(cec_p->cec_isr_work), cec_isr_bottom_half);
	cec_p->w_q = create_workqueue(CEC_ISR_W_Q_NAME);

	return 0;
}

int cec_dealloc_control_param(stm_cec_t *cec_p)
{
	int	error = INFRA_NO_ERROR;
	uint8_t	dev_id;

	if (cec_p->lock_cec_param_p)
		error |= infra_os_mutex_terminate(cec_p->lock_cec_param_p);
	if (cec_p->tx_done_sema_p)
		error = infra_os_sema_terminate(cec_p->tx_done_sema_p);
	cancel_work_sync(&(cec_p->cec_isr_work));
	destroy_workqueue(cec_p->w_q);
	dev_id = cec_p->magic_num & 0xF;
	(g_cec_h_arr[dev_id]) = NULL;
	infra_os_free(cec_p);
	return 0;
}

int cec_fill_control_param(uint32_t dev_id , stm_cec_t *cec_p)
{
	int error = INFRA_NO_ERROR;

	cec_p->magic_num = MAGIC_NUM_MASK | dev_id;
	cec_p->cec_hw_data_p = &g_cec_hw_data[dev_id];
	cec_p->cur_state = CEC_STATE_OPEN;
	cec_p->next_state = CEC_STATE_OPEN;
	cec_p->logical_addr_set = 0;
	cec_p->cur_logical_addr = CEC_INAVLID_LOGICAL_ADDR;
	cec_p->prev_logical_addr = CEC_INAVLID_LOGICAL_ADDR;
	cec_p->status_arr[CEC_STATUS_MSG_BROADCAST] = false;
	cec_p->status_arr[CEC_STATUS_MSG_PING] = false;
	cec_p->status_arr[CEC_STATUS_TX_MSG_LOADED] = false;
	cec_p->frame_status = 0; /* Init CEC frame status as free*/
	cec_p->bus_status = CEC_HAL_BUS_FREE;
	cec_p->retries = 1; /*Need to try atleast once*/
	cec_p->trials = 0;
	cec_p->msg_list_head = -1;
	cec_p->msg_list_tail = 0;

	return error;
}


int cec_do_hw_init(uint32_t dev_id , stm_cec_t *cec_p)
{
	struct cec_hw_data_s		*hw_data_p;
	struct stm_cec_platform_data	*platform_data_p;
	struct resource			*base_address_info_p;
	struct resource			*irq_info_p;
	uint32_t			ip_mem_size;
	int				error = INFRA_NO_ERROR;

	hw_data_p = cec_p->cec_hw_data_p ;
	hw_data_p->cec_ctrl_p = (void *)cec_p;
	platform_data_p = hw_data_p->platform_data;
	base_address_info_p = hw_data_p->base_address;
	irq_info_p = &(hw_data_p->r_irq);


	ip_mem_size = (base_address_info_p->end -
			base_address_info_p->start +
			1);
	cec_p->base_addr_v = ioremap_nocache(
					base_address_info_p->start,
					ip_mem_size);
	if (!cec_p->base_addr_v) {
		cec_error_trace(CEC_API, "CEC HW memory mapping failed\n");
		return -EFAULT;
	}

	/* initialising the gpio */
	error = cec_pio_init(dev_id, cec_p);
	if (error != 0)
		goto ERROR_PIO;


	cec_debug_trace(CEC_API,
		"irq_info_p->start:%p hw_data_p->dev_name:%s cec_p:%p\n",
		(void *)irq_info_p->start,
		hw_data_p->dev_name,
		cec_p);
	if (request_irq(irq_info_p->start,
			cec_irq_isr,
			0,
			hw_data_p->dev_name,
			cec_p)) {
		cec_error_trace(CEC_API, "irq request failed\n");
		error = -EBUSY;
		goto ERROR_IRQ;
	}
	cec_p->auto_ack_for_broadcast_tx =
		platform_data_p->auto_ack_for_broadcast_tx;
	cec_p->clk_freq = clk_get_rate(hw_data_p->clk);
	/* PATCH to improve timing accuracy */
	cec_p->clk_freq -= platform_data_p->clk_err_correction;
	cec_hal_init(cec_p);
	hw_data_p->init = true;

	return error;

ERROR_IRQ:
	stm_pad_release(cec_p->pad_state);
	cec_p->pad_state = NULL;
ERROR_PIO:
	iounmap(cec_p->base_addr_v);

	return error;
}

int cec_do_hw_term(stm_cec_t *cec_p)
{
	struct cec_hw_data_s		*hw_data_p;
	struct resource			*irq_info_p;
	int				error = INFRA_NO_ERROR;


	hw_data_p = cec_p->cec_hw_data_p ;
	irq_info_p = &(hw_data_p->r_irq);

	free_irq(irq_info_p->start, cec_p);
	stm_pad_release(cec_p->pad_state);
	cec_p->pad_state = NULL;
	iounmap(cec_p->base_addr_v);
	hw_data_p->init = false;
	return error;
}

int cec_retreive_message(stm_cec_t *cec_p,
			stm_cec_msg_t *cec_msg_p)
{
	uint8_t		read_idx ;
	int8_t		rw_diff;

	spin_lock(&cec_p->intr_lock);
	rw_diff = (cec_p->msg_list_tail-1) - cec_p->msg_list_head;
	if (rw_diff > 0) {
		cec_p->msg_list_head++;
		read_idx = ((cec_p->msg_list_head)%CEC_MSG_LIST_SIZE);
		cec_debug_trace(CEC_HW,
				"tail:%d head:%d read_idx:%d\n",
				cec_p->msg_list_tail,
				cec_p->msg_list_head,
				read_idx);
	} else {
		spin_unlock(&cec_p->intr_lock);
		cec_error_trace(CEC_API, "No message to be read\n");
		return -ENODATA;
	}
	spin_unlock(&cec_p->intr_lock);
	infra_os_copy((void *)(cec_msg_p->msg),
	(void *)(cec_p->message_list[read_idx].msg),
	cec_p->message_list[read_idx].msg_len);
	cec_msg_p->msg_len = cec_p->message_list[read_idx].msg_len;

	return 0;
}

int cec_send_msg(stm_cec_t *cec_p,  stm_cec_msg_t *cec_msg_p)
{
	int		error = INFRA_NO_ERROR;
	struct cec_hal_isr_param_s	*isr_param_p;
	cec_hal_msg_status		tx_status;

	cec_p->trials = 0;
	cec_p->cur_logical_addr = CEC_GET_INIT_LOGICAL_ADDR(cec_msg_p->msg[0]);
	cec_core_is_ping_msg(cec_p, cec_msg_p);
	error = cec_hal_send_msg(cec_p, cec_msg_p);

	if (error == INFRA_NO_ERROR)
		error = infra_os_sema_wait(cec_p->tx_done_sema_p);


	/*Check the status of the send command*/
	isr_param_p = &(cec_p->isr_param);
	cec_hal_check_tx_status(cec_p, &tx_status);
	if (isr_param_p->notify &
		(CEC_HAL_EVT_TX_NOT_DONE | CEC_HAL_EVT_MSG_ERROR)) {
		error = -ECOMM;
	}
	if (isr_param_p->notify & CEC_HAL_EVT_TX_CANCELLED)
		error = -ECANCELED;


	isr_param_p->notify &= ~(CEC_HAL_EVT_TX_DONE
				|CEC_HAL_EVT_TX_NOT_DONE
				|CEC_HAL_EVT_MSG_ERROR
				|CEC_HAL_EVT_TX_CANCELLED);
	cec_p->prev_logical_addr = cec_p->cur_logical_addr;

	return error;
}


int cec_check_state(stm_cec_t *cec_p, cec_state_t desired_state)
{
	cec_state_t		cur_state;
	int	error = INFRA_NO_ERROR;

	infra_os_mutex_lock(cec_p->lock_cec_param_p);
	cur_state = cec_p->cur_state;
	infra_os_mutex_unlock(cec_p->lock_cec_param_p);

	switch (desired_state) {
	case CEC_STATE_OPEN:
			/*default state*/
			break;
	case CEC_STATE_QUERY:
			if ((cur_state != CEC_STATE_OPEN) &&
				(cur_state != CEC_STATE_READY))
				error = -EINVAL;

			break;
	case CEC_STATE_READY:
			if ((cur_state != CEC_STATE_QUERY) &&
				(cur_state != CEC_STATE_BUSY)) {
				error = -EINVAL;
			}
			break;
	case CEC_STATE_BUSY:
			if (cur_state != CEC_STATE_READY)
				error = -EINVAL;

			break;
	case CEC_STATE_CLOSE:
			if ((cur_state != CEC_STATE_QUERY) &&
			(cur_state != CEC_STATE_BUSY) &&
			(cur_state != CEC_STATE_READY))
				error = -EINVAL;

			if (cur_state == CEC_STATE_BUSY)
				/*release the waiting user*/
				cec_hal_abort_tx(cec_p);

			break;
	default:
			cec_debug_trace(CEC_API, "INVALID STATE\n");
			error = -EINVAL;
			break;
	}

	if (error == INFRA_NO_ERROR) {
		infra_os_mutex_lock(cec_p->lock_cec_param_p);
		cec_p->cur_state = desired_state;
		infra_os_mutex_unlock(cec_p->lock_cec_param_p);
	}

	return error;
}



static bool cec_hal_check_tx_status(stm_cec_t *cec_p,
				cec_hal_msg_status *tx_status_p)
{
	if (cec_p->status_arr[CEC_STATUS_TX_MSG_LOADED] == false) {
		if (GET_CEC_FRAME_STATUS(cec_p->frame_status,
		CEC_HAL_FRAME_TX_DONE))	{
			CLR_CEC_FRAME_STATUS(cec_p->frame_status,
			CEC_HAL_FRAME_TX_DONE);
		}
		return false;
	}

	if (GET_CEC_BUS_STATUS(cec_p->bus_status, CEC_HAL_BUS_TX_BUSY)) {
		/*Msg is transmitting*/
	} else if (GET_CEC_FRAME_STATUS(cec_p->frame_status,
					CEC_HAL_FRAME_TX_DONE)){

			CLR_CEC_FRAME_STATUS(cec_p->frame_status,
					CEC_HAL_FRAME_TX_DONE);

			if (GET_CEC_FRAME_STATUS(cec_p->frame_status,
						CEC_HAL_FRAME_TX_ACK)){
				*tx_status_p = CEC_HAL_TX_STATUS_SUCCESS;
			} else {
				if (GET_CEC_FRAME_STATUS(cec_p->frame_status,
						(CEC_HAL_FRAME_TX_ERROR|
						CEC_HAL_FRAME_TX_BUSY |
						CEC_HAL_FRAME_TX_ARBITER))){
						/*Notify the fail Busy*/
					*tx_status_p =
						CEC_HAL_TX_STATUS_FAIL_BUSY;
				} else {
					/* Notify ACK FAIL the Events*/
					*tx_status_p =
						CEC_HAL_TX_STATUS_FAIL_NACK;
				}
			}
			cec_p->status_arr[CEC_STATUS_TX_MSG_LOADED]  = false;
			return false;
	} else {
		/*waiting for Transmission*/
	}

	return false;
}


static void cec_isr_bottom_half(struct work_struct *work)
{
	stm_cec_t		*cec_p;
	struct cec_hal_isr_param_s *isr_param_p;
	stm_event_t		event;
	int			error = 0;

	cec_p = container_of(work, stm_cec_t, cec_isr_work);
	isr_param_p = &(cec_p->isr_param);
	cec_debug_trace(CEC_HW, "\n");
	spin_lock(&cec_p->intr_lock);
	event.event_id =
	(isr_param_p->notify &
	CEC_HAL_EVT_RX_DONE) ? STM_CEC_EVENT_MSG_RECEIVED
	: STM_CEC_EVENT_MSG_RECEIVED_ERROR;
	isr_param_p->notify &= ~(CEC_HAL_EVT_RX_DONE|CEC_HAL_EVT_RX_NOT_DONE);
	spin_unlock(&cec_p->intr_lock);

	event.object = (void *)cec_p;
	error = stm_event_signal(&event);
	if (error)
		cec_error_trace(CEC_UTILS,
				"stm_event_signal failed(%d)\n",
				error);


	return;
}

int cec_core_set_ctrl(stm_cec_t *cec_p,
		stm_cec_ctrl_flag_t ctrl_flag,
		stm_cec_ctrl_type_t *ctrl_data_p)
{
	int	error = INFRA_NO_ERROR;

	switch (ctrl_flag) {
	case STM_CEC_CTRL_FLAG_UPDATE_LOGICAL_ADDR:
			error = cec_set_logical_addr(cec_p, ctrl_data_p);
			break;
	default:
			cec_error_trace(CEC_API,
			"INVALID CONTROL REQUESTED\n");
			error = -EINVAL;
	}
	return error;
}

int cec_core_get_ctrl(stm_cec_t *cec_p,
		stm_cec_ctrl_flag_t ctrl_flag,
		stm_cec_ctrl_type_t *ctrl_data_p)
{
	int			error = INFRA_NO_ERROR;

	switch (ctrl_flag) {
	case STM_CEC_CTRL_FLAG_UPDATE_LOGICAL_ADDR:
			error = cec_get_logical_addr(cec_p, ctrl_data_p);
			break;
	default:
			cec_error_trace(CEC_API,
			"INVALID CONTROL REQUESTED\n");
			error = -EINVAL;
	}
	return error;
}


static int cec_set_logical_addr(stm_cec_t *cec_p,
			stm_cec_ctrl_type_t *ctrl_data_p)
{
	int		error = INFRA_NO_ERROR;
	cec_state_t	desired_state = CEC_STATE_OPEN;

	if (ctrl_data_p->logic_addr_param.enable) {
		if (!cec_p->logical_addr_set)
			desired_state = CEC_STATE_READY;

		cec_p->logical_addr_set =
			(1<<ctrl_data_p->logic_addr_param.logical_addr);
		cec_hal_set_logical_addr(cec_p,
				ctrl_data_p->logic_addr_param.logical_addr);
	} else {
		cec_p->logical_addr_set &=
		~(1<<ctrl_data_p->logic_addr_param.logical_addr);
		if (!cec_p->logical_addr_set)
			desired_state = CEC_STATE_QUERY;

		cec_hal_set_logical_addr(cec_p,
		ctrl_data_p->logic_addr_param.logical_addr);
	}
	if (desired_state != CEC_STATE_OPEN)
		error = cec_check_state(cec_p, desired_state);

	cec_debug_trace(CEC_UTILS, "Error:%d\n", error);
	return error;

}


static int cec_get_logical_addr(stm_cec_t *cec_p,
				stm_cec_ctrl_type_t *ctrl_data_p)
{
	int		error = INFRA_NO_ERROR;

	ctrl_data_p->logic_addr_param.logical_addr = cec_p->logical_addr_set;
	return error;

}

static bool cec_core_is_ping_msg(stm_cec_t *cec_p, stm_cec_msg_t *msg_p)
{
	uint8_t		int_addr, foll_addr;

	int_addr = CEC_GET_INIT_LOGICAL_ADDR(msg_p->msg[0]);
	foll_addr = CEC_GET_FOLL_LOGICAL_ADDR(msg_p->msg[0]);

	if ((int_addr == foll_addr) || (int_addr == CEC_BROADCAST_LOG_ADDR))
		cec_p->status_arr[CEC_STATUS_MSG_PING] = true;
	else
		cec_p->status_arr[CEC_STATUS_MSG_PING] = false;

	cec_p->status_arr[CEC_STATUS_MSG_BROADCAST] =
			(foll_addr == CEC_BROADCAST_LOG_ADDR) ? true : false;
	cec_debug_trace(CEC_HW,
			"Send Broadcast message?(%d)\n",
			cec_p->status_arr[CEC_STATUS_MSG_BROADCAST]);

	return cec_p->status_arr[CEC_STATUS_MSG_PING];
}
