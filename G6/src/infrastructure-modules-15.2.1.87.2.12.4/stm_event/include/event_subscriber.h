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
 @File   event_subscriber.h
 @brief



*/


#ifndef _EVENT_SUBSCRIBER_H_
#define _EVENT_SUBSCRIBER_H_

#include "stm_event.h"
#include "infra_queue.h"
#include "event_types.h"
#include "evt_signal_async.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum evt_subscribe_wait_state_e {
	EVT_SUBSCRIBE_STATE_IDLE,
	EVT_SUBSCRIBE_STATE_SET_PARAM,
	EVT_SUBSCRIBE_STATE_WAIT,
	EVT_SUBSCRIBE_STATE_DELETE,
	EVT_SUBSCRIBE_STATE_EXIT
} evt_subscribe_wait_state_t;

typedef int (*evt_mng_term_subscriber_fp)(void *);

typedef struct event_subscriber_signaler_info_s {
	evt_mng_signaler_param_h	signaler_handle;
	infra_os_rwlock_t		lock_sig_info_node;
	stm_object_h			object;
	uint32_t			evt_mask;
	/*Place holder for recording occurence of events. At each index 1 means event occurred*/
	uint32_t			evt_occurence_mask;
	uint32_t			evt_occurence_arr[EVT_MNG_EVT_ARR_SIZE];
	void				*cookie;
	/*Place holder for storing the linked list's node info for this signaler*/
	evt_mng_signaler_info_q_t	q_p;
} evt_mng_subscriber_signaler_info_t;


typedef struct stm_event_subscription_s {
	evt_subscribe_wait_state_t	cur_state;
	evt_subscribe_wait_state_t	next_state;
	infra_error_code_t		api_error;
	uint32_t			handle; /*Refers to a single instance of stm_event_subscription_s. Used for sanity check.*/
	stm_event_handler_t	evt_handler_fp;
	infra_os_waitq_t		*waitq_p;
	infra_os_mutex_t		lock_subscriber_state_p;
	infra_os_rwlock_t		lock_subscriber_node;
	/*This sema will be used when subscriber will call the wait_evt with a timeout>0 val.*/
	infra_os_semaphore_t		wait_evt_sem_p;
	infra_os_semaphore_t		api_sem_p;
	infra_os_semaphore_t		no_active_usr_sem_p;
	bool				is_waiting;
	bool				interruptible;
	bool				enable_synchro;
	bool				deletion_started;
	evt_async_signaler_h		async_param_h;
	/*pointer to the first node of signaler's linked list for this subscriber*/
	evt_mng_signaler_q_t		*signaler_info_q_head_p;
	/*Place holder for storing the linked list's node info for this subscriber*/
	evt_mng_subscribe_q_t		q_node_p;
	/*Number of threads, accessing the subscriber control block*/
	uint32_t			usr_cnt;
} stm_event_subscription_t;

typedef struct evt_mng_subscriber_control_s {
	evt_mng_subscribe_q_t		*head_q_p;
	uint32_t			num_subscriber;
	infra_os_rwlock_t		lock_subscriber_q;
	evt_mng_term_subscriber_fp	subscriber_term_fp;
} evt_mng_subscriber_control_t;

void evt_mng_set_subscriber_control_param(evt_mng_subscriber_control_t *control_p);
evt_mng_subscriber_control_t *evt_mng_get_subscriber_control_param(void);

infra_error_code_t evt_mng_subscription_create(
					       stm_event_subscription_entry_t *subscription_entries,
					       uint32_t number_of_entries,
					       stm_event_subscription_h *subscription);

infra_error_code_t evt_mng_subscription_delete(stm_event_subscription_h subscription);

infra_error_code_t evt_mng_subscription_modify(
					       stm_event_subscription_h subscription,
					       stm_event_subscription_entry_t *subscription_entry,
					       stm_event_subscription_op_t operation);

bool evt_mng_subscriber_check_events(
				     stm_event_subscription_h subscription,
				     evt_mng_check_event_param_t *check_event_param_p);

/*This API will be called by the signaler to inform the subsribers, when an event occurs.*/
infra_error_code_t evt_mng_signal_subsciber(
					    stm_event_subscription_h subscriber,
					    stm_event_info_t *evt_info_p,
					    uint32_t number_of_events);

bool evt_mng_check_event_subscription(stm_event_subscription_h subscriber,
				      stm_event_info_t *evt_info_p,
				      evt_mng_subscriber_signaler_info_t **cur_signaler_info_p);

infra_error_code_t
evt_subscribe_handle_wait(stm_event_subscription_h subscriber,
			  evt_mng_check_event_param_t *check_event_param_p);
infra_error_code_t
evt_subscribe_check_entry(stm_event_subscription_h subscriber,
			  evt_subscribe_wait_state_t desired_state);
infra_error_code_t
evt_subscribe_do_exit(stm_event_subscription_h subscriber,
		      evt_subscribe_wait_state_t desired_state);

infra_error_code_t evtmng_dealloc_subscriber_param(
						   stm_event_subscription_t *subscription_p);
infra_error_code_t evt_mng_report_entry(
					stm_event_subscription_h subscriber,
					uint8_t do_removal);
infra_error_code_t evt_mng_report_exit(
				       stm_event_subscription_h subscriber);

ssize_t dump_subscriber(struct file *f, char __user *buf, size_t count, loff_t *ppos);

#ifdef __cplusplus
}
#endif

#endif /*_EVENT_SUBSCRIBER_H_*/
