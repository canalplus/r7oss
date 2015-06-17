/*****************************************************************************/
/* COPYRIGHT (C) 2009 STMicroelectronics - All Rights Reserved               */
/* ST makes no warranty express or implied including but not limited to,     */
/* any warranty of                                                           */
/*                                                                           */
/*   (i)  merchantability or fitness for a particular purpose and/or         */
/*   (ii) requirements, for a particular purpose in relation to the LICENSED */
/*        MATERIALS, which is provided �AS IS�, WITH ALL FAULTS. ST does not */
/*        represent or warrant that the LICENSED MATERIALS provided here     */
/*        under is free of infringement of any third party patents,          */
/*        copyrights,trade secrets or other intellectual property rights.    */
/*        ALL WARRANTIES, CONDITIONS OR OTHER TERMS IMPLIED BY LAW ARE       */
/*        EXCLUDED TO THE FULLEST EXTENT PERMITTED BY LAW                    */
/*                                                                           */
/*****************************************************************************/
/**
 @File   evt_test_signaler.h
 @brief



*/


#ifndef _EVT_TEST_SIGNALER_H
#define _EVT_TEST_SIGNALER_H

#include "infra_os_wrapper.h"
#include "stm_event.h"
#include "infra_test_interface.h"
#include "evt_test.h"

#define EVT_TEST_MAX_EVT_NAME	32


typedef enum {
	EVT_TEST_SIG_TYPE_1 = 0,
	EVT_TEST_SIG_TYPE_2,
	EVT_TEST_SIG_TYPE_3,
	EVT_TEST_SIG_TYPE_MAX
} evt_test_signaler_type_t;

typedef enum {
	EVT_TEST_SIG_TASK_STATE_INIT = 1,
	EVT_TEST_SIG_TASK_STATE_START,
	EVT_TEST_SIG_TASK_STATE_STOP,
	EVT_TEST_SIG_TASK_STATE_TERM
} evt_test_sig_task_state_t;

typedef char evt_test_evt_name_t[EVT_TEST_MAX_EVT_NAME];

typedef struct evt_test_sig_evt_id_string_param_s {
	stm_object_h		obj;
	uint32_t			evt_id;
	evt_test_evt_name_t	evt_name;
	evt_test_evt_name_t	sig_name;
} evt_test_sig_evt_id_string_param_t;


#define EVT_TEST_ID_SIG_1_EVENT_MAX		11
typedef enum {
	EVT_TEST_ID_SIG_1_EVENT_A = 0x1,
	EVT_TEST_ID_SIG_1_EVENT_B = (1 << 2),
	EVT_TEST_ID_SIG_1_EVENT_C = (1 << 3),
	EVT_TEST_ID_SIG_1_EVENT_D = (1 << 4),
	EVT_TEST_ID_SIG_1_EVENT_E = (1 << 5),
	EVT_TEST_ID_SIG_1_EVENT_G = (1 << 6),
	EVT_TEST_ID_SIG_1_EVENT_H = (1 << 7),
	EVT_TEST_ID_SIG_1_EVENT_I = (1 << 8),
	EVT_TEST_ID_SIG_1_EVENT_J = (1 << 9),
	EVT_TEST_ID_SIG_1_EVENT_K = (1 << 10),
	EVT_TEST_ID_SIG_1_IPV4_EVT = (1 << 11)
} evt_test_signaler1_events_t;

typedef struct evt_test_signaler_s {
	uint32_t				handle;
	infra_dev_name_t			dev_name;
	evt_test_signaler_type_t		type;
	uint8_t					sig_ip_evt;
	infra_os_timeout_t			timeout_in_millisec;
	infra_os_thread_t			thread;
	evt_test_sig_task_state_t		cur_state;
	evt_test_sig_task_state_t		next_state;
	infra_os_mutex_t			lock_p;
	/*This sema will be used when subscriber will call the wait_evt with a timeout>0 val.*/
	infra_os_semaphore_t		sem_p;
	infra_os_semaphore_t		cmd_sem_p;
} evt_test_signaler_t;

#define EVT_TEST_ID_SIG_2_EVENT_MAX		12
typedef enum {
	EVT_TEST_ID_SIG_2_EVENT_A = 0x1,
	EVT_TEST_ID_SIG_2_EVENT_B = (1 << 2),
	EVT_TEST_ID_SIG_2_EVENT_C = (1 << 3),
	EVT_TEST_ID_SIG_2_EVENT_D = (1 << 4),
	EVT_TEST_ID_SIG_2_EVENT_E = (1 << 5),
	EVT_TEST_ID_SIG_2_EVENT_G = (1 << 6),
	EVT_TEST_ID_SIG_2_EVENT_H = (1 << 7),
	EVT_TEST_ID_SIG_2_EVENT_I = (1 << 8),
	EVT_TEST_ID_SIG_2_EVENT_J = (1 << 9),
	EVT_TEST_ID_SIG_2_EVENT_K = (1 << 10),
	EVT_TEST_ID_SIG_2_EVENT_F = (1 << 11),
	EVT_TEST_ID_SIG_2_IPV4_EVT = (1 << 11)
} evt_test_signaler2_events_t;

#define EVT_TEST_ID_SIG_3_EVENT_MAX		11
typedef enum {
	EVT_TEST_ID_SIG_3_EVENT_A = 0x1,
	EVT_TEST_ID_SIG_3_EVENT_B = (1 << 2),
	EVT_TEST_ID_SIG_3_EVENT_C = (1 << 3),
	EVT_TEST_ID_SIG_3_EVENT_D = (1 << 4),
	EVT_TEST_ID_SIG_3_EVENT_E = (1 << 5),
	EVT_TEST_ID_SIG_3_EVENT_G = (1 << 6),
	EVT_TEST_ID_SIG_3_EVENT_H = (1 << 7),
	EVT_TEST_ID_SIG_3_EVENT_I = (1 << 8),
	EVT_TEST_ID_SIG_3_EVENT_J = (1 << 9),
	EVT_TEST_ID_SIG_3_EVENT_K = (1 << 10),
	EVT_TEST_ID_SIG_3_IPV4_EVT = (1 << 11)
} evt_test_signaler3_events_t;


infra_error_code_t evt_test_sig_get_object(stm_object_h *obj_p, infra_dev_name_t name);
evt_test_signaler_type_t evt_test_sig_get_sigtype(stm_object_h obj);
infra_error_code_t evt_test_sig_evt_id_to_string(evt_test_sig_evt_id_string_param_t *param_p);
infra_error_code_t evt_test_sig_set_cmd(evt_test_signaler_t *control_p, evt_test_sig_task_state_t state);
infra_error_code_t evt_test_signaler_entry(void);
infra_error_code_t evt_test_signaler_exit(void);

#endif	 /*_EVT_TEST_SIGNALER_H*/
