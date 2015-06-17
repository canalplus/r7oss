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
 @File   event_signaler.h
 @brief



*/


#ifndef _EVENT_SIGNALER_H
#define _EVENT_SIGNALER_H

#include "stm_event.h"
#include "infra_os_wrapper.h"
#include "infra_queue.h"
#include "event_types.h"

#ifdef __cplusplus
extern "C" {
#endif


typedef int (*evt_mng_term_signaler_fp)(void *);

/*Control Block of each Signaler*/
typedef struct evt_mng_signaler_param_s {
	stm_object_h			object;
	uint32_t			signaler_handle;
	/*Place holder for events, each index can store 1 evt data*/
	stm_event_info_t		evt_mng_evt_info_t[EVT_MNG_EVT_ARR_SIZE];
	uint32_t			evt_mask;
	uint32_t			evt_occured;
	wait_queue_head_t		delete_waitq;
	atomic_t			use_cnt;
	infra_os_rwlock_t		lock_signaler_node;
	/*Head of the Subscriber Linked List*/
	evt_mng_subscribe_q_t		*subcriber_head_p;
	/*Node Memory to create the Linked List*/
	evt_mng_signaler_q_t		signaler_q_p;
} evt_mng_signaler_param_t;



typedef struct evt_mng_signaler_control_s {
	evt_mng_signaler_q_t		*head;
	uint32_t			num_signaler;
	infra_os_rwlock_t		lock_sig_q_param_p;
} evt_mng_signaler_control_t;

void evt_mng_set_signaler_control_param(evt_mng_signaler_control_t *control_p);
evt_mng_signaler_control_t *evt_mng_get_signaler_control_param(void);

stm_object_h evt_mng_get_signaler_id(uint32_t signaler_handle);

infra_error_code_t evt_mng_add_signaler(stm_event_subscription_entry_t *subscription_entry,
					uint32_t *signaler_handle_p);

infra_error_code_t evt_mng_signaler_deattach_subscriber(uint32_t signaler_handle,
							void *subscriber_p);

infra_error_code_t evt_mng_signaler_update_subscriber_info(stm_event_subscription_entry_t *subscription_entry,
							   uint32_t signaler_handle);

infra_error_code_t evt_mng_signaler_attach_subscriber(stm_event_subscription_entry_t *subscription_entry,
						      uint32_t *signaler_handle_p,
						      void *subscriber_p);

bool evt_mng_signaler_lookup_events(uint32_t signaler_handle,
				    evt_mng_check_event_param_t *check_event_param_p);

infra_error_code_t evt_mng_handle_evt(stm_event_t *event);


ssize_t dump_signaler(struct file *f, char __user *buf, size_t count, loff_t *ppos);

#ifdef __cplusplus
}
#endif

#endif /*_EVENT_SIGNALER_H*/
