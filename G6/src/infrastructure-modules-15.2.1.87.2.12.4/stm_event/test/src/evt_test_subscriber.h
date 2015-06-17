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
 @File   evt_test_subscriber.h
 @brief



*/


#ifndef _EVT_TEST_SUBSCRIBE_H
#define _EVT_TEST_SUBSCRIBE_H

#include "infra_os_wrapper.h"
#include "stm_event.h"
#include "infra_test_interface.h"
#include "evt_test_signaler.h"
#include "evt_test.h"

#define EVT_TEST_SUBSCRIBE_OBJECT_MAX	4
#define EVT_TEST_SUBSCRIBE_EVT_MAX	30


typedef enum {
	EVT_TEST_SUB_TYPE_1 = 0,
	EVT_TEST_SUB_TYPE_2,
	EVT_TEST_SUB_TYPE_3,
	EVT_TEST_SUB_TYPE_MAX
} evt_test_subscriber_type_t;


typedef enum {
	EVT_TEST_SUB_TASK_STATE_INIT = 1,
	EVT_TEST_SUB_TASK_STATE_START,
	EVT_TEST_SUB_TASK_STATE_RUN,
	EVT_TEST_SUB_TASK_STATE_STOP,
	EVT_TEST_SUB_TASK_STATE_TERM
} evt_test_sub_task_state_t;

struct evt_test_subscriber_s {
	uint32_t			handle;
	infra_dev_name_t		dev_name;
	evt_test_subscriber_type_t	type;

	stm_event_subscription_entry_t	subscription_entry[EVT_TEST_SUBSCRIBE_OBJECT_MAX];
	uint32_t			max_subscription;
	stm_event_subscription_h	subscriber_handle;

	infra_os_thread_t		thread;
	evt_test_sub_task_state_t	cur_state;
	evt_test_sub_task_state_t	next_state;

	infra_os_mutex_t		lock_p;
	/*This sema will be used when subscriber
	 * will call the wait_evt with a timeout>0 val.
	 * */
	infra_os_semaphore_t		sem_p;
	infra_os_semaphore_t		cmd_sem_p;
	uint32_t			num_evt_received;
	stm_event_info_t		evt_info[EVT_TEST_SUBSCRIBE_EVT_MAX];

	bool				do_polling;
	infra_os_timeout_t		timeout_in_millisec;

	infra_os_waitq_t		queue;

	stm_event_handler_t		evt_handler;
};


infra_error_code_t evt_test_subscribe_entry(void);
void evt_test_sub_print_evt_info(stm_event_info_t *evt_info_p,
				 evt_test_sig_evt_id_string_param_t *param_p,
				 const char *func_name);
infra_error_code_t evt_test_subscribe_exit(void);
int evt_test_delete_subscription(struct evt_test_subscriber_s *);

#endif /*_STM_EVENT_H*/
