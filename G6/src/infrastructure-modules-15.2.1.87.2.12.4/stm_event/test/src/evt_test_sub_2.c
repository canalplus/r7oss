/*****************************************************************************/
/* COPYRIGHT (C) 2009 STMicroelectronics - All Rights Reserved               */
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
 @File   evt_test_subscriber.c
 @brief



*/

#include "evt_test_signaler.h"
#include "evt_test_subscriber.h"
#include "evt_test_sub_2.h"


static void evt_test_subscribe_task_S2(void *);
static int evt_test_sub_S2_handle_evt(struct evt_test_subscriber_s *control_p);


void evt_test_subscribe_task_S2(void *data_p)
{
	struct evt_test_subscriber_s *control_p;

	control_p = (struct evt_test_subscriber_s *) data_p;

	evt_test_dtrace(EVT_TEST_L1, " PID:0x%x\n", (uint32_t) current->pid);

	while (1) {
		infra_os_sema_wait(control_p->sem_p);
		infra_os_mutex_lock(control_p->lock_p);
		if (control_p->cur_state != control_p->next_state)
			control_p->cur_state = control_p->next_state;

		infra_os_mutex_unlock(control_p->lock_p);

		switch (control_p->cur_state) {
		case EVT_TEST_SUB_TASK_STATE_INIT:
			break;
		case EVT_TEST_SUB_TASK_STATE_START:
			infra_os_sema_signal(control_p->cmd_sem_p);
			break;
		case EVT_TEST_SUB_TASK_STATE_RUN:
			evt_test_sub_S2_handle_evt(control_p);
			break;
		case EVT_TEST_SUB_TASK_STATE_STOP:
			evt_test_dtrace(EVT_TEST_L1, " Stopping\n");
			evt_test_delete_subscription(control_p);
			infra_os_sema_signal(control_p->cmd_sem_p);
			evt_test_dtrace(EVT_TEST_L1, " Stopping\n");
			break;
		case EVT_TEST_SUB_TASK_STATE_TERM:
			evt_test_dtrace(EVT_TEST_L1, " Terminating\n");
			infra_os_task_exit();
			return;
			break;
		default:
			evt_test_dtrace(EVT_TEST_L1, " invalid state\n");
		}
	}
}

int evt_test_subscribe_init_S2(struct evt_test_subscriber_s *control_p)
{
	infra_error_code_t error = INFRA_NO_ERROR;
	infra_os_task_priority_t task_priority[2] = {SCHED_RR, 50};

	infra_os_wait_queue_initialize(&(control_p->queue));

	if ((&(control_p->queue)) == NULL)
		return -ENOMEM;

	control_p->handle = (uint32_t) control_p;
	control_p->type = EVT_TEST_SUB_TYPE_2;
	control_p->cur_state = EVT_TEST_SUB_TASK_STATE_INIT;
	control_p->next_state = EVT_TEST_SUB_TASK_STATE_INIT;
	error = infra_os_sema_initialize(&(control_p->sem_p), 1);
	error = infra_os_sema_initialize(&(control_p->cmd_sem_p), 0);
	error |= infra_os_mutex_initialize(&(control_p->lock_p));

	infra_os_thread_create(&(control_p->thread),
		      evt_test_subscribe_task_S2,
		      (void *) control_p,
		      "INF-T-evtSub2",
		      task_priority);

	if (control_p->thread == NULL)
		return -ENOMEM;


	return INFRA_NO_ERROR;
}

infra_error_code_t
evt_test_set_subscription_S2(struct evt_test_subscriber_s *control_p)
{
	stm_object_h			object;
	stm_event_subscription_entry_t	*subscription_entry_p = NULL;
	stm_event_subscription_h	subscription;
	uint32_t			number_of_entries = 0;
	infra_error_code_t		error = INFRA_NO_ERROR;

	subscription_entry_p = (stm_event_subscription_entry_t *)
		control_p->subscription_entry;

	evt_test_dtrace(EVT_TEST_L1, " >>>>SUBSCRIPTION>>>>\n");

	evt_test_sig_get_object(&object, "EVT_TEST_SIG_1");
	subscription_entry_p->cookie = control_p;
	subscription_entry_p->object = object;
	subscription_entry_p->event_mask = (EVT_TEST_ID_SIG_1_EVENT_A |
		EVT_TEST_ID_SIG_1_EVENT_C |
		EVT_TEST_ID_SIG_1_EVENT_E |
		EVT_TEST_ID_SIG_1_EVENT_H |
		EVT_TEST_ID_SIG_1_EVENT_J |
		EVT_TEST_ID_SIG_1_EVENT_K);

	evt_test_dtrace(EVT_TEST_L1,
		      "subscription_entry_p:0x%x  object:0x%x Mask:0x%x Cook:0x%x\n",
		      (uint32_t) subscription_entry_p,
		      (uint32_t) object,
		      subscription_entry_p->event_mask,
		      (uint32_t) control_p);

	subscription_entry_p++;
	number_of_entries++;

	evt_test_sig_get_object(&object, "EVT_TEST_SIG_2");
	subscription_entry_p->cookie = control_p;
	subscription_entry_p->object = object;
	subscription_entry_p->event_mask = (EVT_TEST_ID_SIG_2_EVENT_B |
		EVT_TEST_ID_SIG_2_EVENT_D |
		EVT_TEST_ID_SIG_2_EVENT_F |
		EVT_TEST_ID_SIG_2_EVENT_H |
		EVT_TEST_ID_SIG_2_EVENT_J |
		EVT_TEST_ID_SIG_2_EVENT_K);

	evt_test_dtrace(EVT_TEST_L1,
		      "subscription_entry_p:0x%x  object:0x%x Mask:0x%x Cook:0x%x\n",
		      (uint32_t) subscription_entry_p,
		      (uint32_t) object,
		      subscription_entry_p->event_mask,
		      (uint32_t) control_p);

	subscription_entry_p =
		(stm_event_subscription_entry_t *) control_p->subscription_entry;
	number_of_entries++;

	error = stm_event_subscription_create(subscription_entry_p,
		      number_of_entries,
		      &subscription);
	if (error != INFRA_NO_ERROR)
		evt_test_dtrace(EVT_TEST_L1, "stm_event_subscription \
					create failed, error:%d\n", error);

	control_p->subscriber_handle = subscription;

	evt_test_dtrace(EVT_TEST_L1, " <<<<<SUBSCRIPTION<<<<<\n");

	return INFRA_NO_ERROR;
}

int evt_test_modify_subscription_S2(struct evt_test_subscriber_s *control_p)
{
	stm_object_h			object;
	stm_event_subscription_entry_t	*subscription_entry_p = NULL;
	infra_error_code_t		error = INFRA_NO_ERROR;

	subscription_entry_p =
		(stm_event_subscription_entry_t *) control_p->subscription_entry;

	evt_test_sig_get_object(&object, "EVT_TEST_SIG_1");
	subscription_entry_p->cookie = control_p;
	subscription_entry_p->object = object;
	subscription_entry_p->event_mask = (EVT_TEST_ID_SIG_1_EVENT_A);

	evt_test_dtrace(EVT_TEST_L1,
		      " subscription_entry_p:0x%x  object:0x%x Mask:0x%x Cook:0x%x\n",
		      (uint32_t) subscription_entry_p,
		      (uint32_t) object,
		      subscription_entry_p->event_mask,
		      (uint32_t) control_p);

	error = stm_event_subscription_modify(control_p->subscriber_handle,
		      subscription_entry_p,
		      STM_EVENT_SUBSCRIPTION_OP_REMOVE);
	if (error != INFRA_NO_ERROR)
		evt_test_dtrace(EVT_TEST_L1,
			      " stm_event_subscription_modify failed, error:%d.\n",
			      error);

	return error;
}


int evt_test_subscriber_S2_set_interface(struct evt_test_subscriber_s *control_p)
{
	infra_error_code_t error = INFRA_NO_ERROR;
	infra_os_waitq_t *queue;
	bool interruptable;

	error = stm_event_set_wait_queue(control_p->subscriber_handle,
		      &(control_p->queue), true);
	if (error != INFRA_NO_ERROR)
		evt_test_dtrace(EVT_TEST_L1,
			      "stm_event_set_wait_queue failed, error:%d.\n",
			      error);

	error = stm_event_get_wait_queue(control_p->subscriber_handle,
		      &(queue), &interruptable);
	if (error != INFRA_NO_ERROR)
		evt_test_dtrace(EVT_TEST_L1,
			      "stm_event_get_wait_queue failed, error:%d.\n",
			      error);

	if (queue != &(control_p->queue))
		evt_test_dtrace(EVT_TEST_L1,
			      "stm_event_set_wait_queue failed, error:%d.\n",
			      error);


	return error;
}

static int evt_test_sub_S2_handle_evt(struct evt_test_subscriber_s *control_p)
{
	uint32_t				num_evt = 0;
	infra_error_code_t			error = INFRA_NO_ERROR;
	stm_event_info_t			*evt_info_p;
	evt_test_sig_evt_id_string_param_t	param;

	infra_os_wait_for_queue((control_p->queue),
		      (INFRA_NO_ERROR == stm_event_wait(control_p->subscriber_handle,
		      INFRA_OS_IMMEDIATE,
		      EVT_TEST_SUBSCRIBE_EVT_MAX,
		      &(control_p->num_evt_received),
		      control_p->evt_info)),
		      100);

	evt_test_dtrace(EVT_TEST_L1,
		      " %d EVENTS RECEIVED Error:%d\n",
		      control_p->num_evt_received,
		      error);

	evt_info_p = control_p->evt_info;

	for (; num_evt < control_p->num_evt_received; num_evt++) {
		param.evt_id = evt_info_p->event.event_id;
		param.obj = evt_info_p->event.object;
		error = evt_test_sig_evt_id_to_string(&param);
		evt_test_sub_print_evt_info(evt_info_p, &param, __FUNCTION__);
		evt_info_p++;
	}

	infra_os_sleep_milli_sec(500);

	/*Trigger the task for next wait for events*/
	infra_os_sema_signal(control_p->sem_p);

	return INFRA_NO_ERROR;
}
