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
 @File   evt_signal_async.h
 @brief



*/

#ifndef _EVT_SIGNAL_ASYNC_H_
#define _EVT_SIGNAL_ASYNC_H_

#include "stm_event.h"
#include "infra_os_wrapper.h"

#define EVT_ASYNC_TASK_NAME			"INF-EvtAsyncCb"
#define EVT_ASYNC_TASK_PRIORITY			52
#define EVT_ASYNC_MSG_Q_MAX_SIZE		16
#define EVT_ASYNC_EVT_INFO_ARR_MAX_SIZE		10

typedef struct evt_async_signaler_s *evt_async_signaler_h;

typedef enum {
	EVT_ASYNC_SIGNALER_STATE_INIT = 1,
	EVT_ASYNC_SIGNALER_STATE_RUN,
	EVT_ASYNC_SIGNALER_STATE_TERM
} evt_async_signaler_state_t;

typedef struct evt_async_msg_q_s {
	uint32_t			subscriber;
	stm_event_handler_t		evt_handler_fp;
} evt_async_msg_q_t;

typedef struct evt_async_post_evt_param_s {
	uint32_t			subscriber;
	stm_event_handler_t		evt_handler_fp;
	stm_event_info_t		*evt_info_p;
} evt_async_post_evt_param_t;


typedef struct evt_async_signaler_s {
	uint32_t			handle;
	infra_os_timeout_t		timeout_in_millisec;
	infra_os_thread_t		thread_p;
	evt_async_signaler_state_t	cur_state;
	evt_async_signaler_state_t	next_state;
	spinlock_t		lock_async_param;
	infra_os_semaphore_t		task_state_sync_sema_p;
	uint8_t				msg_q_head;
	uint8_t				msg_q_tail;
	evt_async_msg_q_t		msg_q[EVT_ASYNC_MSG_Q_MAX_SIZE];
	stm_event_info_t			event_info_arr[EVT_ASYNC_EVT_INFO_ARR_MAX_SIZE];
} evt_async_signaler_t;

infra_error_code_t evt_async_signaler_init(void);
infra_error_code_t evt_async_signaler_term(void);
infra_error_code_t evt_async_post_event(evt_async_signaler_h async_handle, evt_async_post_evt_param_t *post_evt_param_p);
infra_error_code_t evt_async_get_params(evt_async_signaler_h *handle_p);
#endif
