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
 @File   evt_test_sub_1.c
 @brief



*/
#include "evt_test_signaler.h"
#include "evt_test_subscriber.h"
#include "evt_test_sub_1.h"
#include "evt_test.h"

static void evt_test_subscribe_task_S1(void *);
static void evt_test_subscribe_evt_handler_S1(uint32_t,
					      stm_event_info_t *);
int evt_test_sub_S1_handle_evt(struct evt_test_subscriber_s *control_p);



void evt_test_subscribe_task_S1(void *data_p)
{
	struct evt_test_subscriber_s		*control_p;

	control_p = (struct evt_test_subscriber_s *) data_p;

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
			evt_test_etrace(EVT_TEST_L1, " Starting\n");
			break;
		case EVT_TEST_SUB_TASK_STATE_RUN:
			evt_test_sub_S1_handle_evt(control_p);
			break;
		case EVT_TEST_SUB_TASK_STATE_STOP:
			evt_test_delete_subscription(control_p);
			infra_os_sema_signal(control_p->cmd_sem_p);
			evt_test_etrace(EVT_TEST_L1, " Stopping\n");
			break;
		case EVT_TEST_SUB_TASK_STATE_TERM:
			evt_test_etrace(EVT_TEST_L1, " Terminating\n");
			evt_test_etrace(EVT_TEST_L1,
				      " Thr:0x%x\n",
				      (uint32_t) control_p->thread);
			infra_os_task_exit();
			return;
			break;
		default:
			evt_test_etrace(EVT_TEST_L1, " invalid state\n");
		}
	}
}


int evt_test_subscribe_init_S1(struct evt_test_subscriber_s *control_p)
{
	int			error;
	infra_os_task_priority_t task_priority[2] = {SCHED_RR, 45};

	control_p->evt_handler = evt_test_subscribe_evt_handler_S1;
	control_p->handle = (uint32_t) control_p;
	control_p->type = EVT_TEST_SUB_TYPE_1;
	control_p->cur_state = EVT_TEST_SUB_TASK_STATE_INIT;
	control_p->next_state = EVT_TEST_SUB_TASK_STATE_INIT;
	error = infra_os_sema_initialize(&(control_p->sem_p), 1);
	error = infra_os_sema_initialize(&(control_p->cmd_sem_p), 0);
	error |= infra_os_mutex_initialize(&(control_p->lock_p));

	error = infra_os_thread_create(&(control_p->thread),
		      evt_test_subscribe_task_S1,
		      (void *) control_p,
		      "INF-T-evtSub1",
		      task_priority);

	if (control_p->thread == NULL)
		return -ENOMEM;


	return error;
}



static void evt_test_subscribe_evt_handler_S1(uint32_t number_of_events,
					      stm_event_info_t *events)
{
	struct evt_test_subscriber_s			*control_p;

	control_p = (struct evt_test_subscriber_s *) events->cookie;
	control_p->num_evt_received = number_of_events;
	evt_test_dtrace(EVT_TEST_L1,
		      " SUB_NAME:%s\n",
		      control_p->dev_name);
	evt_test_dtrace(EVT_TEST_L1,
		      " %d EVENTS RECEIVED\n",
		      number_of_events);
	evt_test_dtrace(EVT_TEST_L1,
		      " EVENT:0x%x\n",
		      (uint32_t) events);
	evt_test_dtrace(EVT_TEST_L1,
		      " Time:%u Cook:%u\n",
		      (uint32_t) (events->timestamp),
		      (uint32_t) (events->cookie));
	infra_os_copy((void *) control_p->evt_info,
		      (void *) events,
		      (sizeof(stm_event_info_t) * number_of_events));
	infra_os_sema_signal(control_p->sem_p);
	return;
}

int evt_test_set_subscription_S1(struct evt_test_subscriber_s *control_p)
{
	stm_object_h			object;
	stm_event_subscription_entry_t	*subscription_entry_p = NULL;
	stm_event_subscription_h	subscription;
	uint32_t			number_of_entries = 0;
	int		error = INFRA_NO_ERROR;

	subscription_entry_p = (stm_event_subscription_entry_t *)
		control_p->subscription_entry;

	evt_test_etrace(EVT_TEST_L1, " >>>>SUBSCRIPTION>>>>\n");

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

	evt_test_sig_get_object(&object, "EVT_TEST_SIG_2");
	subscription_entry_p->cookie = control_p;
	subscription_entry_p->object = object;
	subscription_entry_p->event_mask = (EVT_TEST_ID_SIG_2_EVENT_A |
		EVT_TEST_ID_SIG_2_EVENT_C |
		EVT_TEST_ID_SIG_2_EVENT_E |
		EVT_TEST_ID_SIG_2_EVENT_G |
		EVT_TEST_ID_SIG_2_EVENT_I |
		EVT_TEST_ID_SIG_2_EVENT_K);

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


int evt_test_modify_subscription_S1(struct evt_test_subscriber_s *control_p)
{
	stm_object_h			object;
	stm_event_subscription_entry_t	*subscription_entry_p = NULL;
	int			error = INFRA_NO_ERROR;

	subscription_entry_p = (stm_event_subscription_entry_t *)
		control_p->subscription_entry;

	evt_test_sig_get_object(&object, "EVT_TEST_SIG_3");
	subscription_entry_p->cookie = control_p;
	subscription_entry_p->object = object;
	subscription_entry_p->event_mask = (EVT_TEST_ID_SIG_1_EVENT_B |
		EVT_TEST_ID_SIG_1_EVENT_C |
		EVT_TEST_ID_SIG_1_EVENT_H);

	evt_test_etrace(EVT_TEST_L1,
		      " subscription_entry_p:0x%x  object:0x%x Mask:0x%x Cook:0x%x\n",
		      (uint32_t) subscription_entry_p,
		      (uint32_t) object,
		      subscription_entry_p->event_mask,
		      (uint32_t) control_p);


	error = stm_event_subscription_modify(control_p->subscriber_handle,
		      subscription_entry_p,
		      STM_EVENT_SUBSCRIPTION_OP_UPDATE);
	if (error != INFRA_NO_ERROR)
		evt_test_etrace(EVT_TEST_L1,
			      " stm_event_subscription_modify failed, error:%d.\n",
			      error);

	return error;
}


int evt_test_subscriber_S1_set_interface(
					 struct evt_test_subscriber_s *control_p)
{
	int			error = INFRA_NO_ERROR;
	stm_event_handler_t	evt_handler;

	error = stm_event_set_handler(control_p->subscriber_handle,
		      control_p->evt_handler);
	if (error != INFRA_NO_ERROR)
		evt_test_etrace(EVT_TEST_L1,
			      " stm_event_set_handler failed, error:%d.\n",
			      error);

	error = stm_event_get_handler(control_p->subscriber_handle,
		      &evt_handler);
	if (error != INFRA_NO_ERROR)
		evt_test_etrace(EVT_TEST_L1,
			      " stm_event_get_handler failed, error:%d.\n",
			      error);

	if (evt_handler != control_p->evt_handler)
		evt_test_etrace(EVT_TEST_L1,
			      " stm_event_set_handler failed, error:%d.\n",
			      error);

	return error;
}


int evt_test_sub_S1_handle_evt(struct evt_test_subscriber_s *control_p)
{
	uint32_t num_evt = 0;
	int error = 0;
	stm_event_info_t *evt_info_p;
	evt_test_sig_evt_id_string_param_t param;

	evt_info_p = control_p->evt_info;
	evt_test_etrace(EVT_TEST_L1, "\n");

	for (; num_evt < control_p->num_evt_received; num_evt++) {
		param.evt_id = evt_info_p->event.event_id;
		param.obj = evt_info_p->event.object;
		error = evt_test_sig_evt_id_to_string(&param);
		evt_test_sub_print_evt_info(evt_info_p, &param, __FUNCTION__);
		evt_info_p++;
	}

	return error;
}
