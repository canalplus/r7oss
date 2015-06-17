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
 @File   stm_event.c
 @brief



*/

#include "stm_event.h"
#include "event_types.h"
#include "event_subscriber.h"
#include "event_signaler.h"
#include "event_debug.h"

static int evt_mng_is_event_pending(struct stm_event_subscription_s *subscription);

int evt_mng_is_event_pending(struct stm_event_subscription_s *subscription)
{
	int error = 0;
	evt_mng_check_event_param_t	check_event_param;
	stm_event_info_t		dummy_event_info;
	bool				evt_found;

	check_event_param.evt_info_mem_p = (uint8_t *) &dummy_event_info;
	check_event_param.num_evt_occured = 0;
	check_event_param.max_evt_space = 1;
	check_event_param.do_evt_retrieval = 0;
	evt_found = evt_mng_subscriber_check_events(subscription,
		      &check_event_param);
	evt_subscribe_do_exit(subscription, EVT_SUBSCRIBE_STATE_IDLE);
	/*If event is found, return error as no space*/
	evt_debug_trace(EVENT_API, "PID:0x%x Evt:%d Evt:%d\n",
		      (uint32_t) current->pid,
		      evt_found,
		      check_event_param.num_evt_occured);
	error = (evt_found == true) ? (-ENOSPC) : (INFRA_NO_ERROR);
	evt_mng_report_exit(subscription);
	return error;
}


int stm_event_subscription_create(
				  stm_event_subscription_entry_t *subscription_entries,
				  uint32_t number_of_entries,
				  stm_event_subscription_h *subscription)
{
	infra_error_code_t	error = INFRA_NO_ERROR;

	if (subscription_entries == NULL || number_of_entries == 0)
		return -EINVAL;


	error = evt_mng_subscription_create(subscription_entries,
		      number_of_entries,
		      subscription);

	if (error != INFRA_NO_ERROR)
		evt_error_trace(EVENT_API, "Error(%d) in subscription creation\n", error);


	return error;

}


int stm_event_subscription_delete(stm_event_subscription_h subscription)
{
	infra_error_code_t	error = INFRA_NO_ERROR;

	/*Check the sanity of the handle*/
	if (subscription == NULL ||
		subscription->handle != (uint32_t) subscription) {
		return -EINVAL;
	}
	error = evt_mng_report_entry(subscription, true);
	if (error != 0) {
		evt_error_trace(EVENT_API,
			      "Invalid Subscription\n");
		return error;
	}

	error = evt_subscribe_check_entry(subscription,
		      EVT_SUBSCRIBE_STATE_DELETE);
	if (error != INFRA_NO_ERROR) {
		/*This action is not allowed right now.*/
		evt_error_trace(EVENT_API,
			      "Error(%d) in subscription deletion\n", error);
		return error;
	}

	evt_mng_report_exit(subscription);
	error = evt_mng_subscription_delete(subscription);
	/*dealloc the subscriber memory*/
	evtmng_dealloc_subscriber_param(subscription);
	infra_os_free((void *) subscription);

	if (error != INFRA_NO_ERROR) {
		evt_error_trace(EVENT_API,
			      "Error(%d) in subscription deletion\n", error);
	}

	return error;
}

int stm_event_subscription_modify(
				  stm_event_subscription_h subscription,
				  stm_event_subscription_entry_t *subscription_entry,
				  stm_event_subscription_op_t operation)
{

	int		error = STM_EVT_NO_ERROR;

	/*Check the sanity of the handle*/
	if (subscription == NULL ||
		subscription->handle != (uint32_t) subscription) {
		evt_error_trace(EVENT_API,
			      "Error(%d) in subscription modify\n",
			      error);
		return -EINVAL;
	}

	error = evt_mng_report_entry(subscription, false);
	if (error != 0) {
		evt_error_trace(EVENT_API,
			      "Invalid Subscription\n");
		return error;
	}

	error = evt_subscribe_check_entry(subscription,
		      EVT_SUBSCRIBE_STATE_SET_PARAM);
	if (error != INFRA_NO_ERROR) {
		/*This action is not allowed right now.*/
		evt_error_trace(EVENT_API,
			      "Error(%d) in subscription modify\n", error);
		return error;
	}

	error = evt_mng_subscription_modify(subscription,
		      subscription_entry,
		      operation);
	evt_subscribe_do_exit(subscription, EVT_SUBSCRIBE_STATE_IDLE);
	evt_mng_report_exit(subscription);

	return error;
}

int stm_event_wait(
		   stm_event_subscription_h subscription,
		   int32_t timeout,
		   uint32_t max_number_of_events,
		   uint32_t *number_of_events_p,
		   stm_event_info_t *events)
{

	infra_error_code_t		error = INFRA_NO_ERROR;
	evt_mng_check_event_param_t	check_event_param;
	u_long flags = 0;

	/*Check the sanity of the handle*/
	if (subscription == NULL ||
		subscription->handle != (uint32_t) subscription) {
		return -EINVAL;
	}

	error = evt_mng_report_entry(subscription, false);
	if (error != INFRA_NO_ERROR) {
		/*Either the subscription has been deleted or
		 * is in process of deletion.*/
		return error;
	}

	error = evt_subscribe_check_entry(subscription,
		      EVT_SUBSCRIBE_STATE_WAIT);
	if (error != INFRA_NO_ERROR) {
		/*This action is not allowed right now.*/
		evt_mng_report_exit(subscription);
		return error;
	}

	/*Check a special condition, a lookup for, if pending events exsist*/
	if (number_of_events_p == NULL &&
		events == NULL &&
		max_number_of_events == 0) {
		error = evt_mng_is_event_pending(subscription);
		return error;
	}

	infra_os_write_lock_irqsave(&subscription->lock_subscriber_node,
		      flags);
	subscription->api_error = INFRA_NO_ERROR;
	infra_os_write_unlock_irqrestore(&subscription->lock_subscriber_node,
		      flags);

	/*Check the sanity of the params*/
	if (number_of_events_p == NULL ||
		events == NULL ||
		max_number_of_events == 0) {
		evt_mng_report_exit(subscription);
		return -EINVAL;
	}

	check_event_param.evt_info_mem_p = (uint8_t *) events;
	check_event_param.num_evt_occured = 0;
	check_event_param.max_evt_space = max_number_of_events;
	check_event_param.timeout = timeout;
	check_event_param.do_evt_retrieval = 1;

	error = evt_subscribe_handle_wait(subscription, &check_event_param);

	/*Update the Evt Params.*/
	*number_of_events_p = check_event_param.num_evt_occured;
	if (error != -ECANCELED)
		evt_subscribe_do_exit(subscription, EVT_SUBSCRIBE_STATE_IDLE);
	else
		evt_subscribe_do_exit(subscription, EVT_SUBSCRIBE_STATE_DELETE);

	evt_debug_trace(EVENT_API, "PID:0x%x Err:%d Evt:%d\n",
		      (uint32_t) current->pid,
		      error,
		      check_event_param.num_evt_occured);
	evt_mng_report_exit(subscription);

	return error;

}

int stm_event_set_handler(
			  stm_event_subscription_h subscription,
			  stm_event_handler_t event_handler)
{
	int		error = STM_EVT_NO_ERROR;
	u_long flags = 0;

	/*Check the sanity of the handle*/
	if (subscription == NULL ||
		subscription->handle != (uint32_t) subscription) {
		evt_error_trace(EVENT_API,
			      "Error(INVALID PARAM) Sub:0x%x\n",
			      (uint32_t) subscription);
		return -EINVAL;
	}

	infra_os_write_lock_irqsave(&subscription->lock_subscriber_node,
		      flags);
	if (NULL == subscription->waitq_p) {
		evt_debug_trace(EVENT_API, "Sub:0x%x  evt_handler_fp:0x%x\n",
			      (uint32_t) subscription,
			      (uint32_t) event_handler);
		subscription->evt_handler_fp = event_handler;
	} else {
		evt_error_trace(EVENT_API,
			      "Error(INVALID PARAM) Sub:0x%x\n",
			      (uint32_t) subscription);
		error = -EINVAL;
	}
	infra_os_write_unlock_irqrestore(&subscription->lock_subscriber_node,
		      flags);

	return error;

}

int stm_event_get_handler(
			  stm_event_subscription_h subscription,
			  stm_event_handler_t *event_handler_p)
{
	u_long flags = 0;

	/*Check the sanity of the handle*/
	if (subscription == NULL ||
		subscription->handle != (uint32_t) subscription) {
		return -EINVAL;
	}

	infra_os_write_lock_irqsave(&subscription->lock_subscriber_node,
		      flags);
	*event_handler_p = subscription->evt_handler_fp;
	infra_os_write_unlock_irqrestore(&subscription->lock_subscriber_node,
		      flags);

	return STM_EVT_NO_ERROR;

}

int stm_event_set_wait_queue(
			     stm_event_subscription_h subscription,
			     wait_queue_head_t	*queue,
			     bool		interruptible)
{
	int		error = STM_EVT_NO_ERROR;
	u_long flags = 0;

	/*Check the sanity of the handle*/
	if (subscription == NULL || subscription->handle != (uint32_t) subscription)
		return -EINVAL;


	infra_os_write_lock_irqsave(&subscription->lock_subscriber_node,
		      flags);
	if (NULL == subscription->evt_handler_fp) {
		evt_debug_trace(EVENT_API, "Sub:0x%x  Wait_p:0x%x\n",
			      (uint32_t) subscription, (uint32_t) queue);
		subscription->waitq_p = queue;
		subscription->interruptible = interruptible;
	} else {
		evt_error_trace(EVENT_API, "Error(INVALID PARAM) Sub:0x%x\n",
			      (uint32_t) subscription);
		error = -EINVAL;
	}
	infra_os_write_unlock_irqrestore(&subscription->lock_subscriber_node,
		      flags);

	return error;

}


int stm_event_get_wait_queue(
			     stm_event_subscription_h subscription,
			     wait_queue_head_t	**queue_p,
			     bool		*interruptible_p)
{
	u_long flags = 0;

	/*Check the sanity of the handle*/
	if (subscription == NULL ||
		subscription->handle != (uint32_t) subscription) {
		return -EINVAL;
	}

	infra_os_write_lock_irqsave(&subscription->lock_subscriber_node,
		      flags);
	*queue_p = subscription->waitq_p;
	*interruptible_p = subscription->interruptible;
	infra_os_write_unlock_irqrestore(&subscription->lock_subscriber_node,
		      flags);

	return STM_EVT_NO_ERROR;

}

int stm_event_signal(stm_event_t *event)
{
	int		error = STM_EVT_NO_ERROR;

	/*Check the sanity of the handle*/
	if (event == NULL || event->object == NULL)
		return -EINVAL;


	error = evt_mng_handle_evt(event);

	return error;
}
