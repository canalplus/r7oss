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
 @File   event_signal_async.c
 @brief

**/

#include "infra_os_wrapper.h"
#include "evt_signal_async.h"
#include "event_types.h"
#include "event_subscriber.h"
#include "event_debug.h"

static evt_async_signaler_t		*evt_async_control_p;

static int thread_inf_evtasynccb[2] = {SCHED_RR, EVT_ASYNC_TASK_PRIORITY};
module_param_array_named(thread_INF_EvtAsyncCb, thread_inf_evtasynccb, int, NULL, 0644);
MODULE_PARM_DESC(thread_INF_EvtAsyncCb, "INF-EvtAsyncCb thread:s(Mode),p(Priority)");


static void evt_async_signaler_task(void *);
static int evt_async_pull_event(evt_async_signaler_t *control_p);

infra_error_code_t evt_async_signaler_init(void)
{

	infra_error_code_t		error = INFRA_NO_ERROR;
	evt_async_signaler_t		*control_p = NULL;

	control_p = (evt_async_signaler_t *) infra_os_malloc(sizeof(evt_async_signaler_t));
	if (NULL == control_p)
		return -ENOMEM;


	memset(control_p, 0, sizeof(*control_p));

	control_p->timeout_in_millisec = 100;
	control_p->handle = (uint32_t) control_p;
	control_p->cur_state = EVT_ASYNC_SIGNALER_STATE_INIT;
	control_p->next_state = EVT_ASYNC_SIGNALER_STATE_INIT;
	error = infra_os_sema_initialize(&(control_p->task_state_sync_sema_p), 1);
	infra_os_spin_lock_init(&control_p->lock_async_param);

	infra_os_thread_create(&(control_p->thread_p),
		      evt_async_signaler_task,
		      (void *) control_p,
		      (char *) EVT_ASYNC_TASK_NAME,
		      thread_inf_evtasynccb);

	if (control_p->thread_p == NULL)
		return -ENOMEM;


	evt_async_control_p = control_p;

	return INFRA_NO_ERROR;
}

infra_error_code_t evt_async_signaler_term()
{
	infra_error_code_t	error;
	evt_async_signaler_t	*control_p = NULL;

	control_p = evt_async_control_p;
	control_p->next_state = EVT_ASYNC_SIGNALER_STATE_TERM;
	infra_os_sema_signal(control_p->task_state_sync_sema_p);
	infra_os_wait_thread(&(control_p->thread_p));
	error = infra_os_sema_terminate(control_p->task_state_sync_sema_p);

	infra_os_free((void *) control_p);

	evt_debug_trace(EVENT_ASYNC_MANAGER, "\n");
	return INFRA_NO_ERROR;
}

void evt_async_signaler_task(void *data_p)
{
	evt_async_signaler_t	*control_p;
	control_p = (evt_async_signaler_t *) data_p;
	control_p->timeout_in_millisec = INFRA_OS_INFINITE;

	while (1) {
		infra_os_sema_wait(control_p->task_state_sync_sema_p);
		control_p->cur_state = control_p->next_state;

		switch (control_p->cur_state) {
		case EVT_ASYNC_SIGNALER_STATE_INIT:
			control_p->next_state =
				EVT_ASYNC_SIGNALER_STATE_RUN;
			break;
		case EVT_ASYNC_SIGNALER_STATE_RUN:
			evt_async_pull_event(control_p);
			break;
		case EVT_ASYNC_SIGNALER_STATE_TERM:
			infra_os_task_exit();
			return;
		default:
			evt_error_trace(EVENT_ASYNC_MANAGER,
				      " invalid state\n");
		}

	}
}


infra_error_code_t evt_async_post_event(evt_async_signaler_h async_handle, evt_async_post_evt_param_t *post_evt_param_p)
{
	uint8_t			tail;
	evt_async_msg_q_t	*msg_p;
	u_long flags = 0;

	infra_os_spin_lock_irqsave(&async_handle->lock_async_param, flags);
	tail = ((async_handle->msg_q_tail + 1) % EVT_ASYNC_MSG_Q_MAX_SIZE);

	if (tail == async_handle->msg_q_head) {
		infra_os_spin_unlock_irqrestore(
			      &async_handle->lock_async_param,
			      flags);
		evt_error_trace(EVENT_ASYNC_MANAGER, " MESSAGE Q FULL\n");
		return -ENOSPC;
	}

	tail = (async_handle->msg_q_tail % EVT_ASYNC_MSG_Q_MAX_SIZE);
	async_handle->msg_q_tail = (tail + 1) % EVT_ASYNC_MSG_Q_MAX_SIZE;
	infra_os_spin_unlock_irqrestore(
		      &async_handle->lock_async_param,
		      flags);

	msg_p = &(async_handle->msg_q[tail]);
	msg_p->subscriber = post_evt_param_p->subscriber;
	msg_p->evt_handler_fp = post_evt_param_p->evt_handler_fp;

	evt_debug_trace(EVENT_ASYNC_MANAGER,
		      " ASYCNRO: head:%d Tail:%d\n",
		      async_handle->msg_q_head,
		      async_handle->msg_q_tail);
	/*Trigger the task to process this event*/
	infra_os_sema_signal(async_handle->task_state_sync_sema_p);

	return INFRA_NO_ERROR;
}

infra_error_code_t evt_async_pull_event(evt_async_signaler_t *control_p)
{
	int				err = 0;
	uint8_t				head;
	evt_async_msg_q_t		*msg_p;
	evt_mng_check_event_param_t	check_event_param;
	evt_subscribe_wait_state_t	state;
	u_long flags = 0;

	infra_os_spin_lock_irqsave(&control_p->lock_async_param, flags);
	head = (control_p->msg_q_head % EVT_ASYNC_MSG_Q_MAX_SIZE);

	if (head == control_p->msg_q_tail) {
		infra_os_spin_unlock_irqrestore(&control_p->lock_async_param,
			      flags);
		evt_debug_trace(EVENT_ASYNC_MANAGER, "MESSAGE Q EMPTY\n");
		return -ENODATA;
	}
	evt_debug_trace(EVENT_ASYNC_MANAGER,
		      " ASYCNRO: head:%d Tail:%d\n",
		      control_p->msg_q_head,
		      control_p->msg_q_tail);
	msg_p = &(control_p->msg_q[head]);
	/*Update the head as the cur message has been consumed*/
	control_p->msg_q_head = ((head + 1) % EVT_ASYNC_MSG_Q_MAX_SIZE);
	infra_os_spin_unlock_irqrestore(&control_p->lock_async_param, flags);

	check_event_param.evt_info_mem_p =
		(uint8_t *) (&(control_p->event_info_arr[0]));
	check_event_param.num_evt_occured = 0;
	check_event_param.max_evt_space = EVT_ASYNC_EVT_INFO_ARR_MAX_SIZE;
	check_event_param.timeout = INFRA_OS_IMMEDIATE;
	check_event_param.do_evt_retrieval = 1;

	/*Check if Susbcriber has been deleted already*/
	err = evt_mng_report_entry(
		      (stm_event_subscription_h) msg_p->subscriber,
		      false);
	if (err != INFRA_NO_ERROR) {
		evt_error_trace(EVENT_ASYNC_MANAGER, " Subscriber(%d) has been \
			deleted\n", msg_p->subscriber);
		return err;
	}
	err = evt_subscribe_check_entry(
		      (stm_event_subscription_h) (msg_p->subscriber),
		      EVT_SUBSCRIBE_STATE_WAIT);

	if (err != INFRA_NO_ERROR) {
		/*This action is not allowed right now.*/
		evt_error_trace(EVENT_ASYNC_MANAGER, " Event lookup is not allowed\n");
		evt_mng_report_exit((stm_event_subscription_h) (msg_p->subscriber));
		return err;
	}

	err = evt_subscribe_handle_wait((stm_event_subscription_h) (msg_p->subscriber), &check_event_param);
	state = (err != -ECANCELED) ? EVT_SUBSCRIBE_STATE_IDLE : EVT_SUBSCRIBE_STATE_DELETE;
	evt_subscribe_do_exit((stm_event_subscription_h) (msg_p->subscriber), state);

	evt_debug_trace(EVENT_ASYNC_MANAGER,
		      " ASYCNRO: head:%d Tail:%d Evt:%d\n",
		      control_p->msg_q_head,
		      control_p->msg_q_tail,
		      check_event_param.num_evt_occured);
	if (err == 0 && check_event_param.num_evt_occured != 0) {
		msg_p->evt_handler_fp(check_event_param.num_evt_occured,
			      &(control_p->event_info_arr[0]));
	}
	evt_mng_report_exit((stm_event_subscription_h) (msg_p->subscriber));

	return INFRA_NO_ERROR;
}


infra_error_code_t evt_async_get_params(evt_async_signaler_h *handle_p)
{
	*handle_p = evt_async_control_p;
	return INFRA_NO_ERROR;
}
