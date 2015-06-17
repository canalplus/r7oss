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
 @File   event_receiver.h
 @brief



*/

#include "event_subscriber.h"
#include "event_signaler.h"
#include "event_utils.h"
#include "event_debug.h"
#include "event_debugfs.h"

static evt_mng_subscriber_control_t	*evt_subscribe_control_p;

static infra_error_code_t evtmng_alloc_subscriber_param(
							stm_event_subscription_t *subscription_p);
static infra_error_code_t evtmng_delete_signaler_info(
						      stm_event_subscription_h subscription);
static infra_error_code_t evtmng_copy_signaler_info_to_subscriber(
								  stm_event_subscription_h subscription,
								  stm_event_subscription_entry_t *subscription_entry_p);
static infra_error_code_t evtmng_erase_signaler_info_from_subscriber(
								     stm_event_subscription_h subscription,
								     stm_object_h object);
static int evtmng_term_subscriber(void *);
static infra_error_code_t evt_mng_wait_for_active_users(
							stm_event_subscription_h subscriber);
static infra_error_code_t evtmng_update_signaler_info_for_subscriber(
								     stm_event_subscription_h subscription,
								     stm_event_subscription_entry_t *subscription_entry_p);

int evtmng_term_subscriber(void *data_p)
{
	infra_error_code_t		error = INFRA_NO_ERROR;
	stm_event_subscription_t	*subscribe_cur_p = NULL;
	evt_mng_subscribe_q_t		*subscribe_cur_q_p = NULL;
	evt_mng_subscriber_control_t	*control_p;

	control_p = (evt_mng_subscriber_control_t *) data_p;

	subscribe_cur_q_p = control_p->head_q_p;
	evt_debug_trace(EVENT_SUBSCRIBER,
		      KERN_ALERT " %s (pid %i)\n",
		      current->comm, current->pid);
	/*Traverse the signaler list and
	 * delete each reference for the this subscriber*/
	while (subscribe_cur_q_p != NULL) {
		subscribe_cur_p = INFRA_Q_GET_DATA(subscribe_cur_q_p,
			      stm_event_subscription_t);
		/*Calling the subscriber's list, */
		evt_mng_report_entry(subscribe_cur_p, true);
		evt_mng_report_exit(subscribe_cur_p);
		error = evt_mng_subscription_delete(subscribe_cur_p);

		error = evtmng_dealloc_subscriber_param(subscribe_cur_p);

		subscribe_cur_q_p = INFRA_Q_GET_NEXT((&(subscribe_cur_p->q_node_p)));
		evt_debug_trace(EVENT_SUBSCRIBER, " Subscriber:0x%x\n",
			      (uint32_t) subscribe_cur_p);
		/*dealloc the subscriber memory*/
		infra_os_free((void *) subscribe_cur_p);
		subscribe_cur_p = NULL;
	}

	return error;
}

static infra_error_code_t evtmng_delete_signaler_info(stm_event_subscription_h subscription)
{
	infra_error_code_t			error = INFRA_NO_ERROR;
	evt_mng_subscriber_signaler_info_t	*signaler_info_cur_p = NULL;
	evt_mng_signaler_q_t			*signaler_info_cur_q_p = NULL;

	signaler_info_cur_q_p = subscription->signaler_info_q_head_p;

	/*Traverse the signaler list and delete each reference
	 * for the this subscriber*/
	while (signaler_info_cur_q_p != NULL) {
		signaler_info_cur_p = INFRA_Q_GET_DATA(
			      signaler_info_cur_q_p,
			      evt_mng_subscriber_signaler_info_t);
		/*Calling the Signaler's list, to update its own subscriber list*/
		error = evt_mng_signaler_deattach_subscriber(
			      (uint32_t) (signaler_info_cur_p->signaler_handle),
			      (void *) subscription);
		signaler_info_cur_q_p = INFRA_Q_GET_NEXT((&(signaler_info_cur_p->q_p)));
		/*free this node created to store this obj info*/
		infra_os_free((void *) signaler_info_cur_p);
		signaler_info_cur_p = NULL;
	}

	return error;
}


static infra_error_code_t evtmng_copy_signaler_info_to_subscriber(
								  stm_event_subscription_h subscription,
								  stm_event_subscription_entry_t *subscription_entry_p)
{
	infra_error_code_t			error;
	evt_mng_subscriber_signaler_info_t	*signaler_info_p = NULL;
	uint32_t				signaler_handle;
	evt_mng_signaler_info_q_t		*signaler_info_q_p = NULL;
	u_long flags = 0;

	error = evt_mng_signaler_attach_subscriber(subscription_entry_p,
		      &signaler_handle,
		      (void *) (subscription));
	if (error != INFRA_NO_ERROR) {
		evt_debug_trace(EVENT_SUBSCRIBER,
			      " Error(%d) Signaler Not Subscribed\n",
			      error);
		return error;
	}

	infra_os_read_lock(&subscription->lock_subscriber_node);
	error = infra_q_search_node(subscription->signaler_info_q_head_p,
		      (uint32_t) (subscription_entry_p->object),
		      &signaler_info_q_p);
	infra_os_read_unlock(&subscription->lock_subscriber_node);

	/*is this signaler already present in our linked list?
	 * Error means NO, so alloc a new node*/
	if (error != INFRA_NO_ERROR) {
		evt_debug_trace(EVENT_SUBSCRIBER,
			      " Error(%d) Signaler Not Found\n",
			      error);
		/*reset the error*/
		error = INFRA_NO_ERROR;
		signaler_info_p = (evt_mng_subscriber_signaler_info_t *)
			infra_os_malloc(sizeof(*signaler_info_p));
		if (NULL == signaler_info_p) {
			evt_error_trace(EVENT_SUBSCRIBER, " Error(%d)\n", error);
			error = evt_mng_signaler_deattach_subscriber(signaler_handle,
				      (void *) (subscription));
			return -ENOMEM;
		}
		/*Insert this structure in the Q*/
		infra_os_write_lock_irqsave(&subscription->lock_subscriber_node, flags);
		infra_q_insert_node(&(subscription->signaler_info_q_head_p),
			      &(signaler_info_p->q_p),
			      (uint32_t) (subscription_entry_p->object),
			      (void *) signaler_info_p);
		infra_os_write_unlock_irqrestore(&subscription->lock_subscriber_node, flags);
	} else {
		evt_debug_trace(EVENT_SUBSCRIBER, " Error(%d) Signaler Found\n", error);
		/*Its already present*/
		signaler_info_p = INFRA_Q_GET_DATA(signaler_info_q_p,
			      evt_mng_subscriber_signaler_info_t);
	}

	/*Copy the internal reference to the object storge structure*/
	infra_os_write_lock_irqsave(&signaler_info_p->lock_sig_info_node, flags);
	signaler_info_p->signaler_handle = (evt_mng_signaler_param_h) signaler_handle;
	signaler_info_p->evt_mask = subscription_entry_p->event_mask;
	signaler_info_p->object = subscription_entry_p->object;
	signaler_info_p->cookie = subscription_entry_p->cookie;
	infra_os_write_unlock_irqrestore(&signaler_info_p->lock_sig_info_node, flags);

	evt_debug_trace(EVENT_SUBSCRIBER,
		      " Sig_p:0x%x EvtMask:0x%x Obj:0x%x Cookie:0x%x\n",
		      (uint32_t) signaler_handle,
		      subscription_entry_p->event_mask,
		      (uint32_t) subscription_entry_p->object,
		      (uint32_t) subscription_entry_p->cookie);

	return error;
}

static infra_error_code_t evtmng_update_signaler_info_for_subscriber(
								     stm_event_subscription_h subscription,
								     stm_event_subscription_entry_t *subscription_entry_p)
{
	infra_error_code_t			error;
	evt_mng_subscriber_signaler_info_t	*signaler_info_p = NULL;
	evt_mng_signaler_info_q_t		*signaler_info_q_p = NULL;
	u_long flags = 0;

	infra_os_read_lock(&subscription->lock_subscriber_node);
	error = infra_q_search_node(subscription->signaler_info_q_head_p,
		      (uint32_t) (subscription_entry_p->object),
		      &signaler_info_q_p);
	/*Is this signaler present in our linked list?*/
	if (error != INFRA_NO_ERROR) {
		infra_os_read_unlock(&subscription->lock_subscriber_node);
		evt_error_trace(EVENT_SUBSCRIBER,
			      " Error(%d) Signaler Not Found\n",
			      error);
		return error;
	} else {
		/*Its present, now get the signaler node*/
		signaler_info_p = INFRA_Q_GET_DATA(signaler_info_q_p,
			      evt_mng_subscriber_signaler_info_t);
	}

	infra_os_read_unlock(&subscription->lock_subscriber_node);
	/*Copy the internal reference to the object storge structure*/
	infra_os_write_lock_irqsave(&signaler_info_p->lock_sig_info_node, flags);
	signaler_info_p->evt_mask = subscription_entry_p->event_mask;
	signaler_info_p->cookie = subscription_entry_p->cookie;

	infra_os_write_unlock_irqrestore(&signaler_info_p->lock_sig_info_node, flags);
	evt_debug_trace(EVENT_SUBSCRIBER,
		      " Sig_p:0x%x EvtMask:0x%x Obj:0x%x Cookie:0x%x\n",
		      (uint32_t)signaler_info_p,
		      subscription_entry_p->event_mask,
		      (uint32_t) subscription_entry_p->object,
		      (uint32_t) subscription_entry_p->cookie);

	return error;
}


static infra_error_code_t evtmng_erase_signaler_info_from_subscriber(
								     stm_event_subscription_h subscription,
								     stm_object_h object)
{
	infra_error_code_t			error;
	evt_mng_subscriber_signaler_info_t	*signaler_info_p = NULL;
	uint32_t				signaler_handle;
	evt_mng_signaler_info_q_t		*signaler_info_q_p;
	u_long flags = 0;

	infra_os_write_lock_irqsave(&subscription->lock_subscriber_node,
		      flags);
	/*remove the reference of this signaler from the linked list*/
	error = infra_q_remove_node(&(subscription->signaler_info_q_head_p),
		      (uint32_t) object,
		      &signaler_info_q_p);
	infra_os_write_unlock_irqrestore(&subscription->lock_subscriber_node,
		      flags);
	evt_debug_trace(EVENT_SUBSCRIBER, " Error(%d)\n", error);
	/*If no error, then it means that the signaler was located and removed
	 * Now de-attach this subscriber also from this particular signaler.
	 */
	if (error == INFRA_NO_ERROR) {
		evt_debug_trace(EVENT_SUBSCRIBER, " Error(%d)\n", error);
		signaler_info_p = INFRA_Q_GET_DATA(signaler_info_q_p,
			      evt_mng_subscriber_signaler_info_t);
		signaler_handle = (uint32_t) (signaler_info_p->signaler_handle);
		/*Subsequently delete this subscriber
		 * from the signaler's database also*/
		error = evt_mng_signaler_deattach_subscriber(signaler_handle,
			      (void *) (subscription));
		evt_debug_trace(EVENT_SUBSCRIBER, " Error(%d)\n", error);
		infra_os_free((void *) signaler_info_p);
		signaler_info_p = NULL;
	}

	return error;
}


static infra_error_code_t evtmng_alloc_subscriber_param(stm_event_subscription_t *subscription_p)
{
	infra_error_code_t		error = INFRA_NO_ERROR;

	subscription_p->handle = (uint32_t) subscription_p;
	subscription_p->next_state = EVT_SUBSCRIBE_STATE_IDLE;
	subscription_p->cur_state = EVT_SUBSCRIBE_STATE_IDLE;
	subscription_p->api_error = INFRA_NO_ERROR;
	subscription_p->enable_synchro = false;
	error = infra_os_sema_initialize(&(subscription_p->wait_evt_sem_p), 0);
	error |= infra_os_sema_initialize(&(subscription_p->api_sem_p), 1);
	error |= infra_os_sema_initialize(&(subscription_p->no_active_usr_sem_p), 0);
	error |= infra_os_mutex_initialize(
		      &subscription_p->lock_subscriber_state_p);
	infra_os_rwlock_init(&subscription_p->lock_subscriber_node);

	return error;
}

infra_error_code_t evtmng_dealloc_subscriber_param(stm_event_subscription_t *subscription_p)
{
	infra_error_code_t		error = INFRA_NO_ERROR;

	if (subscription_p->wait_evt_sem_p)
		error = infra_os_sema_terminate(subscription_p->wait_evt_sem_p);

	if (subscription_p->api_sem_p)
		error = infra_os_sema_terminate(subscription_p->api_sem_p);
	if (subscription_p->no_active_usr_sem_p)
		error = infra_os_sema_terminate(subscription_p->no_active_usr_sem_p);
	if (subscription_p->lock_subscriber_state_p)
		error |= infra_os_mutex_terminate(
			      subscription_p->lock_subscriber_state_p);
	subscription_p->wait_evt_sem_p = NULL;
	subscription_p->api_sem_p = NULL;

	return error;
}

void evt_mng_set_subscriber_control_param(evt_mng_subscriber_control_t *control_p)
{
	control_p->subscriber_term_fp = evtmng_term_subscriber;
	evt_subscribe_control_p = control_p;

}

evt_mng_subscriber_control_t *evt_mng_get_subscriber_control_param(void)
{
	return evt_subscribe_control_p;
}

infra_error_code_t evt_mng_subscription_create(
					       stm_event_subscription_entry_t *subscription_entries,
					       uint32_t number_of_entries,
					       stm_event_subscription_h *subscription)
{
	infra_error_code_t error = INFRA_NO_ERROR;
	stm_event_subscription_t *subscription_p = NULL;
	stm_event_subscription_entry_t *subscription_entry_cur_p = NULL;
	uint32_t signaler_count = 0;
	evt_mng_subscriber_control_t *sub_control_p;
	u_long flags = 0;

	subscription_entry_cur_p = subscription_entries;
	sub_control_p = evt_subscribe_control_p;

	evt_debug_trace(EVENT_SUBSCRIBER, " Error(%d)>>>\n", error);

	subscription_p = (stm_event_subscription_t *)
		infra_os_malloc(sizeof(*subscription_p));
	if (NULL == subscription_p) {
		evt_error_trace(EVENT_SUBSCRIBER,
			      " Error(%d) in subscription allocation\n",
			      error);
		return -ENOMEM;
	}

	error = evtmng_alloc_subscriber_param(subscription_p);
	if (INFRA_NO_ERROR != error) {
		evt_error_trace(EVENT_SUBSCRIBER,
			      " Error(%d) in evtmng_alloc_subscriber_param\n", error);
		goto EVT_MNG_ERROR_SUBSCRIBER_PARAM;
	}

	evt_async_get_params(&(subscription_p->async_param_h));

	for (; signaler_count < number_of_entries; signaler_count++) {
		error = evtmng_copy_signaler_info_to_subscriber(
			      subscription_p,
			      subscription_entry_cur_p);
		if (error != INFRA_NO_ERROR) {
			/*since we had an error,
			 * delete the already created signaler_info nodes*/
			evt_error_trace(EVENT_SUBSCRIBER,
				      " Error(%d) in evtmng_copy_signaler_info_to_subscriber\n",
				      error);
			evtmng_delete_signaler_info(subscription_p);
			goto EVT_MNG_ERROR_SUBSCRIBER_PARAM;
		}
		subscription_entry_cur_p++;
	}

	infra_os_write_lock_irqsave(&sub_control_p->lock_subscriber_q,
		      flags);
	infra_q_insert_node(&(sub_control_p->head_q_p),
		      &(subscription_p->q_node_p),
		      (uint32_t) subscription_p,
		      (void *) subscription_p);
	sub_control_p->num_subscriber++;
	infra_os_write_unlock_irqrestore(&sub_control_p->lock_subscriber_q,
		      flags);

	/*Copy the subscription handle*/
	*subscription = subscription_p;

	evt_debug_trace(EVENT_SUBSCRIBER,
		      " Sub_p:0x%x Error(%d)<<<<\n",
		      (uint32_t) subscription_p,
		      error);

	return INFRA_NO_ERROR;

EVT_MNG_ERROR_SUBSCRIBER_PARAM:
	evtmng_dealloc_subscriber_param(subscription_p);
	/*dealloc the subscriber memory*/
	infra_os_free((void *) subscription_p);
	subscription_p = NULL;

	return error;

}

infra_error_code_t evt_mng_subscription_delete(
					       stm_event_subscription_h subscription)
{
	infra_error_code_t		error = INFRA_NO_ERROR;
	evt_mng_subscriber_control_t	*sub_control_p;

	sub_control_p = evt_subscribe_control_p;
	evt_mng_wait_for_active_users(subscription);

	/*Invalidate the handle before de-alloc.
	This will help in the subscription handle sanity check.
	*/
	subscription->handle = EVT_INAVLID_HANDLE;

	/*No active now for this node. Locking not required*/
	error = evtmng_delete_signaler_info(subscription);

	/* ensure exit from evt_mng_report_exit() */
	infra_os_read_lock(&subscription->lock_subscriber_node);
	infra_os_read_unlock(&subscription->lock_subscriber_node);
	return error;
}

infra_error_code_t evt_mng_subscription_modify(
					       stm_event_subscription_h subscription,
					       stm_event_subscription_entry_t *subscription_entry,
					       stm_event_subscription_op_t operation)
{
	infra_error_code_t			error = INFRA_NO_ERROR;
	evt_mng_subscriber_signaler_info_t	*signaler_info_p = NULL;
	uint32_t				signaler_handle;
	evt_mng_signaler_info_q_t		*signaler_info_q_p;

	switch (operation) {
	case STM_EVENT_SUBSCRIPTION_OP_ADD:
		evt_debug_trace(EVENT_SUBSCRIBER,
			      " STM_EVENT_SUBSCRIPTION_OP_ADD\n");
		error = evtmng_copy_signaler_info_to_subscriber(
			      subscription,
			      subscription_entry);
		break;
	case STM_EVENT_SUBSCRIPTION_OP_REMOVE:
		evt_debug_trace(EVENT_SUBSCRIBER,
			      " STM_EVENT_SUBSCRIPTION_OP_REMOVE\n");
		error = evtmng_erase_signaler_info_from_subscriber(
			      subscription,
			      subscription_entry->object);
		break;
	case STM_EVENT_SUBSCRIPTION_OP_UPDATE:
		infra_os_read_lock(&subscription->lock_subscriber_node);
		error = infra_q_search_node(
			      subscription->signaler_info_q_head_p,
			      (uint32_t) subscription_entry->object,
			      &signaler_info_q_p);
		infra_os_read_unlock(&subscription->lock_subscriber_node);
		evt_debug_trace(EVENT_SUBSCRIBER,
			      " STM_EVENT_SUBSCRIPTION_OP_UPDATE\n");
		if (INFRA_NO_ERROR == error) {
			signaler_info_p = INFRA_Q_GET_DATA(signaler_info_q_p,
				      evt_mng_subscriber_signaler_info_t);
			signaler_handle = (uint32_t) (signaler_info_p->signaler_handle);
			error = evt_mng_signaler_update_subscriber_info(
				      subscription_entry,
				      signaler_handle);
			error = evtmng_update_signaler_info_for_subscriber(
				      subscription,
				      subscription_entry);
		}
		break;
	}

	return error;
}

bool evt_mng_subscriber_check_events(
				     stm_event_subscription_h subscription,
				     evt_mng_check_event_param_t *check_event_param_p)
{
	bool					evt_occured = false;
	evt_mng_signaler_q_t			*signaler_info_cur_q_p = NULL;
	evt_mng_subscriber_signaler_info_t	*signaler_info_cur_p = NULL;
	stm_event_info_t			*cur_event_p;
	u_long flags = 0;

	signaler_info_cur_q_p = subscription->signaler_info_q_head_p;
	cur_event_p = (stm_event_info_t *) check_event_param_p->evt_info_mem_p;
	evt_debug_trace(EVENT_SUBSCRIBER, " Sub:0x%x cur_event_p:0x%x\n",
		      (uint32_t) subscription, (uint32_t) cur_event_p);

	/*Parse all the regsitered signalers for this suscriber*/
	while (signaler_info_cur_q_p != NULL) {
		signaler_info_cur_p = INFRA_Q_GET_DATA(signaler_info_cur_q_p,
			      evt_mng_subscriber_signaler_info_t);

		infra_os_read_lock(&signaler_info_cur_p->lock_sig_info_node);
		check_event_param_p->requested_evt_mask = signaler_info_cur_p->evt_mask;
		infra_os_copy(check_event_param_p->evt_occurence_info,
			      signaler_info_cur_p->evt_occurence_arr,
			      (sizeof(uint32_t) * EVT_MNG_EVT_ARR_SIZE));
		check_event_param_p->evt_occurence_mask = signaler_info_cur_p->evt_occurence_mask;
		check_event_param_p->cookie = signaler_info_cur_p->cookie;
		infra_os_read_unlock(&signaler_info_cur_p->lock_sig_info_node);
		/*Parse the obj list and check if an event had occurred*/
		evt_occured |= evt_mng_signaler_lookup_events(
			      (uint32_t) (signaler_info_cur_p->signaler_handle),
			      check_event_param_p);
		/*update the event counters and
		 * event mask because some events have been delivered*/
		infra_os_write_lock_irqsave(&signaler_info_cur_p->lock_sig_info_node, flags);
		infra_os_copy(signaler_info_cur_p->evt_occurence_arr,
			      check_event_param_p->evt_occurence_info,
			      (sizeof(uint32_t) * EVT_MNG_EVT_ARR_SIZE));
		signaler_info_cur_p->evt_occurence_mask = check_event_param_p->evt_occurence_mask;
		evt_debug_trace(EVENT_SUBSCRIBER,
			      " evt_occured:0x%x Sig:0x%x\n",
			      evt_occured,
			      (uint32_t) (signaler_info_cur_p->signaler_handle));
		infra_os_write_unlock_irqrestore(&signaler_info_cur_p->lock_sig_info_node,
			      flags);
		/*we have exceeded the max space to copy the events, so break now*/
		if (check_event_param_p->max_evt_space == check_event_param_p->num_evt_occured)
			break;

		/*user just wants to know if there is any event. so check and break*/
		if (check_event_param_p->do_evt_retrieval == 0 && evt_occured == true)
			break;

		infra_os_read_lock(&subscription->lock_subscriber_node);
		signaler_info_cur_q_p = INFRA_Q_GET_NEXT((&(signaler_info_cur_p->q_p)));
		infra_os_read_unlock(&subscription->lock_subscriber_node);
		evt_debug_trace(EVENT_SUBSCRIBER,
			      " signaler_info_cur_q_p:0x%x\n",
			      (uint32_t) signaler_info_cur_q_p);
	}

	return evt_occured;
}

/*
Checks if the event had been subscribed for?
*/
bool evt_mng_check_event_subscription(stm_event_subscription_h subscriber,
				      stm_event_info_t *evt_info_p,
				      evt_mng_subscriber_signaler_info_t **cur_signaler_info_p)
{
	infra_error_code_t		error = INFRA_NO_ERROR;
	evt_mng_signaler_q_t		*temp_q_p;
	bool				evt_subscribed = false;

	infra_os_read_lock(&subscriber->lock_subscriber_node);

	error = infra_q_search_node(subscriber->signaler_info_q_head_p,
		      (uint32_t) evt_info_p->event.object,
		      &temp_q_p);
	if (INFRA_NO_ERROR != error) {
		evt_error_trace(EVENT_SUBSCRIBER,
			      " Signaler:0x%x does not exsist\n",
			      (uint32_t) evt_info_p->event.object);
		infra_os_read_unlock(&subscriber->lock_subscriber_node);
		return false;
	}
	*cur_signaler_info_p = INFRA_Q_GET_DATA(temp_q_p,
		      evt_mng_subscriber_signaler_info_t);

	infra_os_read_unlock(&subscriber->lock_subscriber_node);
	/*Check if subscriber had requested for this event*/
	infra_os_read_lock(&(*cur_signaler_info_p)->lock_sig_info_node);
	if (((*cur_signaler_info_p)->evt_mask & evt_info_p->event.event_id) != 0) {
		evt_debug_trace(EVENT_SUBSCRIBER,
			      " Sub:0x%x EVENT SUBSCRIBED\n",
			      (uint32_t) subscriber);
		evt_subscribed = true;
	}

	infra_os_read_unlock(&(*cur_signaler_info_p)->lock_sig_info_node);

	return evt_subscribed;
}

infra_error_code_t evt_mng_report_entry(stm_event_subscription_h subscriber,
					uint8_t do_removal)
{
	int error = 0;
	evt_mng_subscribe_q_t		*subscriber_q_p;
	evt_mng_subscriber_control_t	*sub_control_p;
	u_long flags = 0;

	sub_control_p = evt_subscribe_control_p;

	infra_os_read_lock(&sub_control_p->lock_subscriber_q);
	/*Check if it has already been deleted.*/
	error = infra_q_search_node(sub_control_p->head_q_p,
		      (uint32_t) subscriber, &subscriber_q_p);

	if (error == 0) {
		infra_os_write_lock_irqsave(
			      &subscriber->lock_subscriber_node,
			      flags);
		if (subscriber->deletion_started == false)
			subscriber->usr_cnt++;
		else
			error = -ECANCELED;


		if (do_removal == true) {
			evt_debug_trace(EVENT_SUBSCRIBER,
				      "SUB:0x%p Usr:%d\n",
				      subscriber,
				      subscriber->usr_cnt);
			subscriber->deletion_started = true;
		}
		infra_os_write_unlock_irqrestore(
			      &subscriber->lock_subscriber_node,
			      flags);
	}
	infra_os_read_unlock(&sub_control_p->lock_subscriber_q);
	return error;
}

infra_error_code_t evt_mng_report_exit(stm_event_subscription_h subscriber)
{
	int error = 0;
	u_long flags = 0;

	infra_os_write_lock_irqsave(&subscriber->lock_subscriber_node,
		      flags);
	subscriber->usr_cnt -= (subscriber->usr_cnt > 0) ? 1 : 0;
	if (subscriber->deletion_started == true && subscriber->usr_cnt == 0) {
		evt_debug_trace(EVENT_SUBSCRIBER,
			      "SUB:0x%p Usr:%d\n",
			      subscriber,
			      subscriber->usr_cnt);
		infra_os_sema_signal(subscriber->no_active_usr_sem_p);
	}
	infra_os_write_unlock_irqrestore(&subscriber->lock_subscriber_node,
		      flags);
	return error;
}

infra_error_code_t evt_mng_wait_for_active_users(stm_event_subscription_h subscriber)
{
	evt_mng_subscriber_control_t	*sub_control_p;
	int error = 0;
	evt_mng_subscribe_q_t		*subscriber_q_p;
	u_long flags = 0;

	sub_control_p = evt_subscribe_control_p;

	infra_os_write_lock_irqsave(&sub_control_p->lock_subscriber_q, flags);

	/*Check if it has already been deleted.*/
	error = infra_q_remove_node(&(sub_control_p->head_q_p),
		      (uint32_t) subscriber, &subscriber_q_p);
	sub_control_p->num_subscriber--;
	if (subscriber->deletion_started == true && subscriber->usr_cnt == 0) {
		infra_os_write_unlock_irqrestore(
			      &sub_control_p->lock_subscriber_q,
			      flags);
		evt_debug_trace(EVENT_SUBSCRIBER, "SUB:0x%p Usr:%d\n",
			      subscriber,
			      subscriber->usr_cnt);
		return 0;
	} else {
		infra_os_write_unlock_irqrestore(
			      &sub_control_p->lock_subscriber_q,
			      flags);
		evt_debug_trace(EVENT_SUBSCRIBER, "SUB:0x%p Usr:%d\n",
			      subscriber,
			      subscriber->usr_cnt);
		infra_os_sema_wait(subscriber->no_active_usr_sem_p);
	}
	return 0;
}

infra_error_code_t evt_mng_signal_subsciber(stm_event_subscription_h subscriber,
					    stm_event_info_t *evt_info_p,
					    uint32_t number_of_events)
{
	evt_mng_subscriber_signaler_info_t	*cur_signaler_info_p = NULL;
	bool					flag = false;
	int8_t					bit_pos;
	evt_async_post_evt_param_t		post_evt_param;
	u_long flags = 0;

	/*First ceck the sanity of the handle.*/
	if (subscriber == NULL || subscriber->handle != (uint32_t) subscriber)
		return -EINVAL;


	flag = evt_mng_check_event_subscription(subscriber,
		      evt_info_p,
		      &cur_signaler_info_p);

	if (flag == false) { /*event not subscribed for*/
		return INFRA_NO_ERROR;
	}

	infra_os_read_lock(&cur_signaler_info_p->lock_sig_info_node);
	/*copy the cookie for this evt*/
	evt_info_p->cookie = cur_signaler_info_p->cookie;

	infra_os_read_unlock(&cur_signaler_info_p->lock_sig_info_node);
	bit_pos = evt_mng_get_bit_pos(evt_info_p->event.event_id);
	if (bit_pos == -1)
		return -EINVAL;


	infra_os_write_lock_irqsave(&cur_signaler_info_p->lock_sig_info_node, flags);
	evt_info_p->events_missed = false;
	cur_signaler_info_p->evt_occurence_arr[bit_pos]++;
	cur_signaler_info_p->evt_occurence_mask |= (1 << bit_pos);
	if (cur_signaler_info_p->evt_occurence_arr[bit_pos] > 1) {
		evt_debug_trace(EVENT_SUBSCRIBER,
			      " Bit:%d EventMissed @ %d\n",
			      bit_pos,
			      cur_signaler_info_p->evt_occurence_arr[bit_pos]);
		evt_info_p->events_missed = true;
	}
	infra_os_write_unlock_irqrestore(&cur_signaler_info_p->lock_sig_info_node, flags);

	infra_os_write_lock_irqsave(&subscriber->lock_subscriber_node,
		      flags);
	/*Check if this subscriber is waiting on a blocking wait*/
	if (subscriber->is_waiting) {
		evt_debug_trace(EVENT_SUBSCRIBER,
			      " Sub:0x%x sem_p:0x%x obj:0x%x\n",
			      (uint32_t) subscriber,
			      (uint32_t) (subscriber->wait_evt_sem_p),
			      (uint32_t) (evt_info_p->event.object));
		infra_os_sema_signal(subscriber->wait_evt_sem_p);
	}
	subscriber->is_waiting = false;

	/*check , if we have a handler or wait Q regsitered*/
	if (subscriber->evt_handler_fp) {
		evt_debug_trace(EVENT_SUBSCRIBER,
			      " Sub:0x%x handler:0x%x obj:0x%x\n",
			      (uint32_t) subscriber,
			      (uint32_t) (subscriber->evt_handler_fp),
			      (uint32_t) (evt_info_p->event.object));

		if (subscriber->enable_synchro) {
			/*Since this subscriber has been informed, so decrease an occurence.*/
			cur_signaler_info_p->evt_occurence_arr[bit_pos] = 0;
			cur_signaler_info_p->evt_occurence_mask &= ~(1 << bit_pos);
			subscriber->evt_handler_fp(number_of_events, evt_info_p);
			evt_debug_trace(EVENT_SUBSCRIBER, " SYNCRO\n");
		} else {
			post_evt_param.subscriber = (uint32_t) subscriber;
			post_evt_param.evt_handler_fp = subscriber->evt_handler_fp;
			post_evt_param.evt_info_p = evt_info_p;
			evt_debug_trace(EVENT_SUBSCRIBER, " ASYCNRO\n");
			evt_async_post_event(subscriber->async_param_h,
				      &(post_evt_param));
		}
		evt_debug_trace(EVENT_SUBSCRIBER,
			      " Sub:0x%x handler:0x%x obj:0x%x\n",
			      (uint32_t) subscriber,
			      (uint32_t) (subscriber->evt_handler_fp),
			      (uint32_t) (evt_info_p->event.object));
	}

	if (subscriber->waitq_p) {
		evt_debug_trace(EVENT_SUBSCRIBER,
			      " Sub:0x%x waitq_p:0x%x obj:0x%x\n",
			      (uint32_t) subscriber,
			      (uint32_t) (subscriber->waitq_p),
			      (uint32_t) (evt_info_p->event.object));
		infra_os_wait_queue_wakeup(subscriber->waitq_p,
			      subscriber->interruptible);
	}

	infra_os_write_unlock_irqrestore(&subscriber->lock_subscriber_node,
		      flags);

	return INFRA_NO_ERROR;
}


infra_error_code_t evt_subscribe_handle_wait(stm_event_subscription_h subscriber,
					     evt_mng_check_event_param_t *check_event_param_p)
{
	infra_error_code_t		error = INFRA_NO_ERROR;
	bool				    evt_found = false;
	int32_t				relative_timeout;

	/*First check if any of the event has occured*/
	evt_debug_trace(EVENT_SUBSCRIBER, " PID:0x%x Sub:0x%x\n", (uint32_t) current->pid, (uint32_t) subscriber);
	evt_found = evt_mng_subscriber_check_events(subscriber, check_event_param_p);

	/*we need to wait for the specified time if no evt found*/
	if (check_event_param_p->num_evt_occured == 0 && INFRA_OS_IMMEDIATE != check_event_param_p->timeout) {

		infra_os_read_lock(&subscriber->lock_subscriber_node);
		subscriber->is_waiting = true;
		infra_os_read_unlock(&subscriber->lock_subscriber_node);
		relative_timeout = check_event_param_p->timeout;
		error = infra_os_sema_wait_timeout(subscriber->wait_evt_sem_p, relative_timeout);
		infra_os_read_lock(&subscriber->lock_subscriber_node);
		if (subscriber->api_error != INFRA_NO_ERROR) {
			evt_debug_trace(EVENT_SUBSCRIBER,
				      " ERR:%u Sub:0x%x\n",
				      error,
				      (uint32_t) subscriber);
			error = subscriber->api_error;
			infra_os_read_unlock(&subscriber->lock_subscriber_node);
			return error;
		}
		infra_os_read_unlock(&subscriber->lock_subscriber_node);

		/*if sema is signalled, we need to check which evt occured */
		if (error == INFRA_NO_ERROR) {
			evt_debug_trace(EVENT_SUBSCRIBER, " ERR:0x%x Sub:0x%x\n", error, (uint32_t) subscriber);
			evt_found = evt_mng_subscriber_check_events(subscriber, check_event_param_p);
		}
	}

	error = (evt_found == true) ? INFRA_NO_ERROR : (-ETIMEDOUT);
	infra_os_read_lock(&subscriber->lock_subscriber_node);
	error = (subscriber->api_error != INFRA_NO_ERROR) ? subscriber->api_error : error;
	infra_os_read_unlock(&subscriber->lock_subscriber_node);
	return error;
}

infra_error_code_t evt_subscribe_do_exit(stm_event_subscription_h subscriber,
					 evt_subscribe_wait_state_t desired_state)
{
	infra_os_mutex_lock(subscriber->lock_subscriber_state_p);
	subscriber->cur_state = desired_state;
	infra_os_mutex_unlock(subscriber->lock_subscriber_state_p);

	/*Signal the API Sema, means that u have completed your work and now someone else can enter*/
	infra_os_sema_signal(subscriber->api_sem_p);

	return INFRA_NO_ERROR;
}


infra_error_code_t evt_subscribe_check_entry(stm_event_subscription_h subscriber,
					     evt_subscribe_wait_state_t desired_state)
{
	evt_subscribe_wait_state_t	cur_state;
	infra_error_code_t		error = INFRA_NO_ERROR;

	infra_os_mutex_lock(subscriber->lock_subscriber_state_p);
	cur_state = subscriber->cur_state;
	/*Check if a command has been issued but not executed i.e
	waiting on API sema, then dont overwrite the state.*/
	if (subscriber->next_state > desired_state) {
		infra_os_mutex_unlock(subscriber->lock_subscriber_state_p);
		return -EINVAL;
	}

	switch (desired_state) {
	case EVT_SUBSCRIBE_STATE_IDLE:
		if ((cur_state != EVT_SUBSCRIBE_STATE_SET_PARAM) &&
			(cur_state != EVT_SUBSCRIBE_STATE_WAIT)) {
			error = -EINVAL;
		}
		break;
	case EVT_SUBSCRIBE_STATE_SET_PARAM:
		if ((cur_state != EVT_SUBSCRIBE_STATE_IDLE) &&
			(cur_state != EVT_SUBSCRIBE_STATE_SET_PARAM)) {
			error = -EINVAL;
		}
		break;
	case EVT_SUBSCRIBE_STATE_WAIT:
		if (cur_state != EVT_SUBSCRIBE_STATE_IDLE)
			error = -EINVAL;

		break;
	case EVT_SUBSCRIBE_STATE_DELETE:
		if (cur_state == EVT_SUBSCRIBE_STATE_WAIT) {
			subscriber->api_error = -ECANCELED;
			infra_os_sema_signal(subscriber->wait_evt_sem_p);
		} else if ((cur_state != EVT_SUBSCRIBE_STATE_IDLE) &&
			(cur_state != EVT_SUBSCRIBE_STATE_SET_PARAM)) {
			error = -EINVAL;
		}
		break;
	case EVT_SUBSCRIBE_STATE_EXIT:
		if (cur_state != EVT_SUBSCRIBE_STATE_DELETE)
			error = -EINVAL;

		break;
	default:
		evt_debug_trace(EVENT_SUBSCRIBER, " INAVLID STATE\n");
		error = -EINVAL;
		break;
	}

	if (error == INFRA_NO_ERROR)
		subscriber->next_state = desired_state;

	infra_os_mutex_unlock(subscriber->lock_subscriber_state_p);

	if (error == INFRA_NO_ERROR) {
		/*Wait on this semaphore, means that pls wait if
		 * someone already accesing this subscription*/
		infra_os_sema_wait(subscriber->api_sem_p);
		infra_os_mutex_lock(subscriber->lock_subscriber_state_p);
		subscriber->cur_state = desired_state;
		subscriber->next_state = EVT_SUBSCRIBE_STATE_IDLE;
		infra_os_mutex_unlock(subscriber->lock_subscriber_state_p);
	} else {
		evt_error_trace(EVENT_SUBSCRIBER, " Error(%d)\n", error);
	}

	return error;
}

#if (defined SDK2_EVENT_ENABLE_DEBUGFS_STATISTICS)
/* This function dumps the all subscribers and their signalers for debug
 * purpose.
 */
ssize_t dump_subscriber(struct file *f, char __user *buf, size_t count, loff_t *ppos)
{
	stm_event_subscription_t *subscribe_cur_p = NULL;
	evt_mng_subscribe_q_t *subscribe_cur_q_p = NULL;
	evt_mng_signaler_q_t *signaler_cur_q_p;
	evt_mng_subscriber_signaler_info_t *signaler_cur_p;
	int32_t ii, jj;
	struct io_descriptor *io_desc;
	ssize_t nbread, size_copy;

	io_desc = f->private_data;

	if (*ppos >= io_desc->size_allocated && io_desc->size_allocated)
		goto done;

	if (*ppos == 0) {
		infra_os_read_lock(&evt_subscribe_control_p->lock_subscriber_q);

		nbread = snprintf(NULL, 0, "Dumping Subscriber List:Total subscribers %d\n",
		      evt_subscribe_control_p->num_subscriber);
		ii = 1;

		/*Traverse the subsriber list and
		* print each reference for the this subscriber*/
		subscribe_cur_q_p = evt_subscribe_control_p->head_q_p;
		/* first find the size */
		while (subscribe_cur_q_p != NULL) {
			subscribe_cur_p = INFRA_Q_GET_DATA(subscribe_cur_q_p,
				      stm_event_subscription_t);
			nbread += snprintf(NULL, 0, "subscriber %d->0x%x\n",
				      ii++, (uint32_t) subscribe_cur_p);
			infra_os_read_lock(&subscribe_cur_p->lock_subscriber_node);
			signaler_cur_q_p = subscribe_cur_p->signaler_info_q_head_p;
			jj = 1;
			while (signaler_cur_q_p) {
				signaler_cur_p = INFRA_Q_GET_DATA(signaler_cur_q_p,
					      evt_mng_subscriber_signaler_info_t);
				nbread += snprintf(NULL, 0, "\t\tsignaler: %d->0x%x object 0x%x evt_mask 0x%x\n",
					      jj++,
					      (uint32_t) (signaler_cur_p->signaler_handle),
					      (uint32_t) (signaler_cur_p->object),
					      signaler_cur_p->evt_mask);

				signaler_cur_q_p = INFRA_Q_GET_NEXT(signaler_cur_q_p);
			}
			infra_os_read_unlock(&subscribe_cur_p->lock_subscriber_node);
			subscribe_cur_q_p = INFRA_Q_GET_NEXT((&(subscribe_cur_p->q_node_p)));
		}

		io_desc->data = infra_os_malloc(nbread);
		if (io_desc->data == NULL) {
			infra_os_read_unlock(&evt_subscribe_control_p->lock_subscriber_q);
			return -ENOMEM;
		}

		io_desc->size_allocated = nbread;
		io_desc->size = nbread;

		nbread = sprintf(&((char *)io_desc->data)[0], "Dumping Subscriber List:Total subscribers %d\n",
		      evt_subscribe_control_p->num_subscriber);
		ii = 1;

		/* Actual copy of the list */
		subscribe_cur_q_p = evt_subscribe_control_p->head_q_p;
		while (subscribe_cur_q_p != NULL) {
			subscribe_cur_p = INFRA_Q_GET_DATA(subscribe_cur_q_p,
				      stm_event_subscription_t);
			nbread += sprintf(&((char *)io_desc->data)[nbread], "subscriber %d->0x%x\n",
				      ii++, (uint32_t) subscribe_cur_p);
			infra_os_read_lock(&subscribe_cur_p->lock_subscriber_node);
			signaler_cur_q_p = subscribe_cur_p->signaler_info_q_head_p;
			jj = 1;
			while (signaler_cur_q_p) {
				signaler_cur_p = INFRA_Q_GET_DATA(signaler_cur_q_p,
					      evt_mng_subscriber_signaler_info_t);
				nbread += sprintf(&((char *)io_desc->data)[nbread], "\t\tsignaler: %d->0x%x object 0x%x evt_mask 0x%x\n",
					      jj++,
					      (uint32_t) (signaler_cur_p->signaler_handle),
					      (uint32_t) (signaler_cur_p->object),
					      signaler_cur_p->evt_mask);

				signaler_cur_q_p = INFRA_Q_GET_NEXT(signaler_cur_q_p);
			}
			infra_os_read_unlock(&subscribe_cur_p->lock_subscriber_node);
			subscribe_cur_q_p = INFRA_Q_GET_NEXT((&(subscribe_cur_p->q_node_p)));
		}

		infra_os_read_unlock(&evt_subscribe_control_p->lock_subscriber_q);
	}

	size_copy = (count < io_desc->size) ? count : io_desc->size;

	if (copy_to_user(buf, io_desc->data + *ppos, size_copy))
		goto done;

	*ppos += size_copy;
	io_desc->size -= size_copy;
	return size_copy;

done:
	if (io_desc->data)
		infra_os_free(io_desc->data);
	io_desc->data = NULL;
	io_desc->size = 0;
	io_desc->size_allocated = 0;
	return 0;
}
#endif
