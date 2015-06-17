/*****************************************************************************/
/* COPYRIGHT (C) 2009 STMicroelectronics - All Rights Reserved               */
/* ST makes no warranty express or implied including but not limited to,     */
/* any warranty of                                                           */
/*                                                                           */
/*   (i)  merchantability or fitness for a particular purpose and/or         */
/*   (ii) requirements, for a particular purpose in relation to the LICENSED */
/*        MATERIALS, which is provided AS IS, WITH ALL FAULTS. ST does not */
/*        represent or warrant that the LICENSED MATERIALS provided here     */
/*        under is free of infringement of any third party patents,          */
/*        copyrights,trade secrets or other intellectual property rights.    */
/*        ALL WARRANTIES, CONDITIONS OR OTHER TERMS IMPLIED BY LAW ARE       */
/*        EXCLUDED TO THE FULLEST EXTENT PERMITTED BY LAW                    */
/*                                                                           */
/*****************************************************************************/
/**
 @File   evt_test_subscriber.c
 @brief



*/
#include "evt_test_signaler.h"
#include "evt_test_subscriber.h"
#include "evt_test_sub_3.h"

#define TIMING_HACK		0

static void evt_test_subscribe_task_S3(void *);
int evt_test_sub_S3_handle_evt(struct evt_test_subscriber_s *control_p);


void evt_test_subscribe_task_S3(void *data_p)
{
	struct evt_test_subscriber_s		*control_p;
	uint32_t			timeout;
	control_p = (struct evt_test_subscriber_s *) data_p;

	timeout = 400;
	control_p->timeout_in_millisec = timeout;
	while (1) {
#if !(TIMING_HACK)
		infra_os_sema_wait_timeout(control_p->sem_p,
			      control_p->timeout_in_millisec);

#else
		infra_os_sleep_milli_sec(timeout);
#endif
		control_p->timeout_in_millisec = INFRA_OS_INFINITE;
		infra_os_mutex_lock(control_p->lock_p);
		if (control_p->cur_state != control_p->next_state)
			control_p->cur_state = control_p->next_state;

		infra_os_mutex_unlock(control_p->lock_p);
		evt_test_dtrace(EVT_TEST_L2,
			      " Cur:%d\n",
			      control_p->cur_state);

		switch (control_p->cur_state) {
		case EVT_TEST_SUB_TASK_STATE_INIT:
			break;
		case EVT_TEST_SUB_TASK_STATE_START:
			evt_test_dtrace(EVT_TEST_L1, " Cur:%d\n",
				      control_p->cur_state);
			infra_os_sema_signal(control_p->cmd_sem_p);
			break;
		case EVT_TEST_SUB_TASK_STATE_RUN:
			evt_test_sub_S3_handle_evt(control_p);
			control_p->timeout_in_millisec = timeout;
			break;
		case EVT_TEST_SUB_TASK_STATE_STOP:
			infra_os_sema_signal(control_p->cmd_sem_p);
			evt_test_etrace(EVT_TEST_L1,
				      " Stopping\n");
			break;
		case EVT_TEST_SUB_TASK_STATE_TERM:
			evt_test_etrace(EVT_TEST_L1,
				      " Terminating\n");
			infra_os_task_exit();
			return;
			break;
		default:
			evt_test_etrace(EVT_TEST_L1,
				      " invalid state\n");
		}
	}
}


int evt_test_subscribe_init_S3(struct evt_test_subscriber_s *control_p)
{
	int			error = INFRA_NO_ERROR;
	infra_os_task_priority_t task_priority[2] = {SCHED_RR, 50};

	control_p->do_polling = false;
	control_p->timeout_in_millisec = 100;
	control_p->handle = (uint32_t) control_p;
	control_p->cur_state = EVT_TEST_SUB_TASK_STATE_INIT;
	control_p->next_state = EVT_TEST_SUB_TASK_STATE_INIT;
	error = infra_os_sema_initialize(&(control_p->sem_p), 1);
	error = infra_os_sema_initialize(&(control_p->cmd_sem_p), 0);
	error |= infra_os_mutex_initialize(&(control_p->lock_p));

	infra_os_thread_create(&(control_p->thread),
		      evt_test_subscribe_task_S3,
		      (void *) control_p,
		      "INF-T-evtSub3",
		      task_priority);

	if (control_p->thread == NULL)
		return -ENOMEM;


	return INFRA_NO_ERROR;
}

int evt_test_set_subscription_S3(struct evt_test_subscriber_s *control_p)
{
	stm_object_h			object;
	stm_event_subscription_entry_t	*subscription_entry_p = NULL;
	stm_event_subscription_h		subscription;
	uint32_t				number_of_entries = 0;
	int			error = INFRA_NO_ERROR;

	subscription_entry_p = (stm_event_subscription_entry_t *)
		control_p->subscription_entry;

	evt_test_etrace(EVT_TEST_L1, " >>>>SUBSCRIPTION<<<<<\n");

	evt_test_sig_get_object(&object, "EVT_TEST_SIG_3");
	subscription_entry_p->cookie = control_p;
	subscription_entry_p->object = object;
	subscription_entry_p->event_mask = (EVT_TEST_ID_SIG_3_EVENT_A |
		EVT_TEST_ID_SIG_3_EVENT_C |
		EVT_TEST_ID_SIG_3_EVENT_E |
		EVT_TEST_ID_SIG_3_EVENT_H |
		EVT_TEST_ID_SIG_3_EVENT_J |
		EVT_TEST_ID_SIG_3_EVENT_K);

	evt_test_etrace(EVT_TEST_L1,
		      " subscription_entry_p:0x%x  object:0x%x Mask:0x%x Cook:0x%x\n",
		      (uint32_t) subscription_entry_p,
		      (uint32_t) object,
		      subscription_entry_p->event_mask,
		      (uint32_t) control_p);

	subscription_entry_p++;
	number_of_entries++;

	evt_test_sig_get_object(&object, "EVT_TEST_SIG_1");
	subscription_entry_p->cookie = control_p;
	subscription_entry_p->object = object;
	subscription_entry_p->event_mask = (EVT_TEST_ID_SIG_1_EVENT_B |
		EVT_TEST_ID_SIG_1_EVENT_D |
		EVT_TEST_ID_SIG_1_EVENT_H |
		EVT_TEST_ID_SIG_1_EVENT_G |
		EVT_TEST_ID_SIG_1_EVENT_I |
		EVT_TEST_ID_SIG_1_EVENT_K);

	evt_test_etrace(EVT_TEST_L1,
		      " subscription_entry_p:0x%x  object:0x%x Mask:0x%x Cook:0x%x\n",
		      (uint32_t) subscription_entry_p,
		      (uint32_t) object,
		      subscription_entry_p->event_mask,
		      (uint32_t) control_p);

	subscription_entry_p = (stm_event_subscription_entry_t *)
		control_p->subscription_entry;
	number_of_entries++;

	error = stm_event_subscription_create(subscription_entry_p,
		      number_of_entries,
		      &subscription);
	if (error != INFRA_NO_ERROR)
		evt_test_etrace(EVT_TEST_L1,
			      " stm_event_subscription_create failed, error:%d.\n",
			      error);

	control_p->subscriber_handle = subscription;

	evt_test_etrace(EVT_TEST_L1,
		      " <<<<<SUBSCRIPTION[0x%x @ 0x%x]<<<<<\n",
		      (uint32_t) control_p,
		      (uint32_t) subscription);

	return INFRA_NO_ERROR;
}


int evt_test_modify_subscription_S3(struct evt_test_subscriber_s *control_p)
{
	stm_object_h			object;
	stm_event_subscription_entry_t	*subscription_entry_p = NULL;
	int		error = INFRA_NO_ERROR;

	subscription_entry_p = (stm_event_subscription_entry_t *)
		control_p->subscription_entry;

	evt_test_sig_get_object(&object, "EVT_TEST_SIG_2");
	subscription_entry_p->cookie = control_p;
	subscription_entry_p->object = object;
	subscription_entry_p->event_mask = (EVT_TEST_ID_SIG_2_EVENT_B |
		EVT_TEST_ID_SIG_2_EVENT_D |
		EVT_TEST_ID_SIG_2_EVENT_H);

	error = stm_event_subscription_modify(control_p->subscriber_handle,
		      subscription_entry_p,
		      STM_EVENT_SUBSCRIPTION_OP_ADD);
	if (error != INFRA_NO_ERROR)
		evt_test_etrace(EVT_TEST_L1,
			      " stm_event_subscription_modify failed, error:%d.\n",
			      error);

	return error;
}

int evt_test_subscriber_S3_set_interface(
					 struct evt_test_subscriber_s *control_p)
{
	int			error = INFRA_NO_ERROR;
	return error;
}

int evt_test_sub_S3_handle_evt(struct evt_test_subscriber_s *control_p)
{
	uint32_t num_evt = 0;
	int32_t	timeout = 0;
	int32_t total_timeout = 0;
	int error;
	stm_event_info_t *evt_info_p;
	evt_test_sig_evt_id_string_param_t param;

	timeout = ((199 * INFRA_OS_GET_TICKS_PER_SEC) / 1000);
	timeout = 100;
#if !(TIMING_HACK)
	error = infra_os_sema_wait_timeout(control_p->sem_p, timeout);
	infra_os_sleep_milli_sec(timeout);
#else
	timeout = (time_now % 699);
	infra_os_sleep_milli_sec(timeout);
#endif
	total_timeout = -1;

	/*first lookup the events*/
	error = stm_event_wait(control_p->subscriber_handle,
		      total_timeout,
		      0,
		      NULL,
		      NULL);
	/*evt found*/
	if (error < 0)
		total_timeout = 0;

	error = stm_event_wait(control_p->subscriber_handle,
		      total_timeout,
		      EVT_TEST_SUBSCRIBE_EVT_MAX,
		      &(control_p->num_evt_received),
		      control_p->evt_info);

	evt_test_etrace(EVT_TEST_L1,
		      " %d EVENTS RECEIVED Err:%d\n",
		      control_p->num_evt_received, error);

	evt_info_p = control_p->evt_info;

	for (; num_evt < control_p->num_evt_received; num_evt++) {
		param.evt_id = evt_info_p->event.event_id;
		param.obj = evt_info_p->event.object;
		error = evt_test_sig_evt_id_to_string(&param);
		evt_test_sub_print_evt_info(evt_info_p, &param, __FUNCTION__);
		evt_info_p++;
	}
	return INFRA_NO_ERROR;
}
