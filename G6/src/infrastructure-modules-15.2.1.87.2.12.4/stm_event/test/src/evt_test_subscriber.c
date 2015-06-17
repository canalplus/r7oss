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

#include "evt_test_subscriber.h"
#include "evt_test_sub_1.h"
#include "evt_test_sub_2.h"
#include "evt_test_sub_3.h"
#include "infra_token.h"

static infra_dev_name_t evt_subscriber_dev_name[] = {"EVT_TEST_SUB_1", "EVT_TEST_SUB_2", "EVT_TEST_SUB_3"};
static uint32_t evt_test_subscriber_handle_store[EVT_TEST_SIG_TYPE_MAX];

/******************************SUBSCRIBER***********************************/

static infra_error_code_t	evt_test_cmd_sub_set_subscription(void *, void *);
static infra_error_code_t	evt_test_cmd_sub_modify_subscription(void *, void *);
static infra_error_code_t	evt_test_cmd_sub_delete_subscription(void *, void *);
static infra_error_code_t	evt_test_cmd_sub_set_interface(void *, void *);

static infra_error_code_t	evt_test_cmd_init_sub(void *, void *);
static infra_error_code_t	evt_test_cmd_start_sub(void *, void *);
static infra_error_code_t	evt_test_cmd_run_sub(void *, void *);
static infra_error_code_t	evt_test_cmd_stop_sub(void *, void *);
static infra_error_code_t	evt_test_cmd_term_sub(void *, void *);

static int evt_test_sub_set_state(struct evt_test_subscriber_s *,
				  evt_test_sub_task_state_t);

static int evt_test_subscribe_term(struct evt_test_subscriber_s *control_p);

static int evt_test_subscribe_set_cmd(struct evt_test_subscriber_s *,
				      evt_test_sub_task_state_t);
static int evt_test_subscribe_interrupt_evt(void *data_p, void *user_data_p);
static int evt_test_modify_subscription(
					struct evt_test_subscriber_s *control_p);

/******************************SUBSCRIBER***********************************/

static infra_error_code_t	evt_test_cmd_sub_get_events(void *, void *);


infra_error_code_t evt_test_subscribe_init_all(void)
{
	infra_error_code_t error;
	struct evt_test_subscriber_s *control_p = NULL;

	control_p = (struct evt_test_subscriber_s *) infra_os_malloc(
		      sizeof(*control_p));
	if (NULL == control_p)
		return -ENOMEM;

	evt_test_subscriber_handle_store[0] = (uint32_t) control_p;
	strncpy(control_p->dev_name,
		      evt_subscriber_dev_name[0],
		      INFRA_MAX_DEV_NAME);
	error = evt_test_subscribe_init_S1(control_p);

	control_p = (struct evt_test_subscriber_s *) infra_os_malloc(
		      sizeof(*control_p));
	if (NULL == control_p)
		return -ENOMEM;

	evt_test_subscriber_handle_store[1] = (uint32_t) control_p;
	strncpy(control_p->dev_name,
		      evt_subscriber_dev_name[1],
		      INFRA_MAX_DEV_NAME);
	error = evt_test_subscribe_init_S2(control_p);

	control_p = (struct evt_test_subscriber_s *) infra_os_malloc(
		      sizeof(*control_p));
	if (NULL == control_p)
		return -ENOMEM;

	evt_test_subscriber_handle_store[2] = (uint32_t) control_p;
	strncpy(control_p->dev_name,
		      evt_subscriber_dev_name[2],
		      INFRA_MAX_DEV_NAME);

	error = evt_test_subscribe_init_S3(control_p);

	return error;
}

infra_error_code_t evt_test_subscribe_term_all(void)
{
	infra_error_code_t error = INFRA_NO_ERROR;
	struct evt_test_subscriber_s *control_p = NULL;

	control_p = (struct evt_test_subscriber_s *)
		evt_test_subscriber_handle_store[0];
	evt_test_subscribe_set_cmd(control_p,
		      EVT_TEST_SUB_TASK_STATE_STOP);
	evt_test_subscribe_set_cmd(control_p,
		      EVT_TEST_SUB_TASK_STATE_TERM);
	infra_os_free((void *) control_p);

	control_p = (struct evt_test_subscriber_s *)
		evt_test_subscriber_handle_store[1];
	evt_test_subscribe_set_cmd(control_p,
		      EVT_TEST_SUB_TASK_STATE_STOP);
	evt_test_subscribe_set_cmd(control_p,
		      EVT_TEST_SUB_TASK_STATE_TERM);
	infra_os_free((void *) control_p);

	control_p = (struct evt_test_subscriber_s *)
		evt_test_subscriber_handle_store[2];
	evt_test_subscribe_set_cmd(control_p,
		      EVT_TEST_SUB_TASK_STATE_STOP);
	evt_test_subscribe_set_cmd(control_p,
		      EVT_TEST_SUB_TASK_STATE_TERM);
	infra_os_free((void *) control_p);

	return error;
}

static int evt_test_subscribe_term(struct evt_test_subscriber_s *control_p)
{
	infra_error_code_t error;

	error = infra_os_sema_terminate(control_p->sem_p);
	error |= infra_os_mutex_terminate(control_p->lock_p);
	error = infra_os_sema_terminate(control_p->cmd_sem_p);

	switch (control_p->type) {
	case EVT_TEST_SUB_TYPE_2:
		error = infra_os_wait_queue_deinitialize(
			      &(control_p->queue));
		break;
	case EVT_TEST_SUB_TYPE_1:
	case EVT_TEST_SUB_TYPE_3:
		/*nothing to do*/
		break;
	default:
		break;
	}

	return INFRA_NO_ERROR;
}


void evt_test_sub_print_evt_info(stm_event_info_t *evt_info_p,
				 evt_test_sig_evt_id_string_param_t *param_p,
				 const char *func_name)
{
	infra_test_print("---------%s[START]---------\n", func_name);

	infra_test_print("Evt Name: %s  Id:%d SigName: %s Sig: 0x%x\n",
		      param_p->evt_name,
		      param_p->evt_id,
		      param_p->sig_name,
		      (uint32_t) param_p->obj);

	infra_test_print("TimeStamp: %u\n",
		      (uint32_t) evt_info_p->timestamp);

	infra_test_print("Events Missed(0/1): %d\n",
		      evt_info_p->events_missed);

	infra_test_print("---------%s[STOP]---------\n", func_name);

	return;
}

static int evt_test_subscribe_set_cmd(struct evt_test_subscriber_s *control_p,
				      evt_test_sub_task_state_t state)
{
	infra_error_code_t error = INFRA_NO_ERROR;

	switch (state) {
	case EVT_TEST_SUB_TASK_STATE_TERM:
		error = evt_test_sub_set_state(control_p, state);
		if (INFRA_NO_ERROR == error) {
			evt_test_etrace(EVT_TEST_L1,
				      "Thr:0x%x\n",
				      (uint32_t) control_p->thread);
			infra_os_wait_thread(&(control_p->thread));
		}
		evt_test_dtrace(EVT_TEST_L1,
			      "EVT_TEST_SUB_TASK_STATE_TERM\n");
		evt_test_subscribe_term(control_p);
		break;
	case EVT_TEST_SUB_TASK_STATE_STOP:
		evt_test_dtrace(EVT_TEST_L1,
			      "Thr:0x%x\n",
			      (uint32_t) control_p->thread);
		error = evt_test_sub_set_state(control_p, state);
		evt_test_dtrace(EVT_TEST_L1, "STOPPING\n");
		if (INFRA_NO_ERROR == error)
			infra_os_sema_wait(control_p->cmd_sem_p);

		break;
	case EVT_TEST_SUB_TASK_STATE_INIT:
		break;
	case EVT_TEST_SUB_TASK_STATE_START:
		error = evt_test_sub_set_state(control_p, state);
		evt_test_etrace(EVT_TEST_L1, " STARTING\n");
		if (INFRA_NO_ERROR == error)
			infra_os_sema_wait(control_p->cmd_sem_p);

		break;
	case EVT_TEST_SUB_TASK_STATE_RUN:
		error = evt_test_sub_set_state(control_p, state);
		evt_test_etrace(EVT_TEST_L1, " RUNNING\n");
		break;
	default:
		evt_test_etrace(EVT_TEST_L1, " Invalid Command\n");
		error = -EINVAL;
		break;
	}

	return error;
}


static int evt_test_sub_set_state(struct evt_test_subscriber_s *control_p,
				  evt_test_sub_task_state_t state)
{
	infra_error_code_t		error = INFRA_NO_ERROR;
	evt_test_sub_task_state_t	cur_state;

	infra_os_mutex_lock(control_p->lock_p);
	cur_state = control_p->cur_state;
	infra_os_mutex_unlock(control_p->lock_p);
	evt_test_dtrace(EVT_TEST_L1, " Cur:%d Desired:%d\n", cur_state, state);

	switch (state) {
	case EVT_TEST_SUB_TASK_STATE_INIT:
		if (cur_state != EVT_TEST_SUB_TASK_STATE_TERM) {
			evt_test_etrace(EVT_TEST_L1,
				      " Error(%d)\n",
				      error);
			error = -EINVAL;
		}
		break;
	case EVT_TEST_SUB_TASK_STATE_START:
		if ((cur_state != EVT_TEST_SUB_TASK_STATE_STOP) &&
			(cur_state != EVT_TEST_SUB_TASK_STATE_INIT)) {
			evt_test_etrace(EVT_TEST_L1,
				      " Error(%d)\n",
				      error);
			error = -EINVAL;
		}
		break;
	case EVT_TEST_SUB_TASK_STATE_RUN:
		if (cur_state != EVT_TEST_SUB_TASK_STATE_START) {
			evt_test_etrace(EVT_TEST_L1,
				      " Error(%d)\n",
				      error);
			error = -EINVAL;
		}
		break;
	case EVT_TEST_SUB_TASK_STATE_TERM:
		evt_test_etrace(EVT_TEST_L1, " Error(%d)\n", error);
		break;
	case EVT_TEST_SUB_TASK_STATE_STOP:
		if ((cur_state != EVT_TEST_SUB_TASK_STATE_START) &&
			(cur_state != EVT_TEST_SUB_TASK_STATE_RUN)) {
			evt_test_etrace(EVT_TEST_L1,
				      " Error(%d)\n",
				      error);
			error = -EINVAL;
		}
		break;
	default:
		evt_test_etrace(EVT_TEST_L1, " Error(%d)\n", error);
		error = -EINVAL;
		break;
	}

	if (error == INFRA_NO_ERROR) {
		evt_test_dtrace(EVT_TEST_L1, " Error(%d)\n", error);
		infra_os_mutex_lock(control_p->lock_p);
		control_p->next_state = state;
		infra_os_mutex_unlock(control_p->lock_p);
		infra_os_sema_signal(control_p->sem_p);
	}

	evt_test_dtrace(EVT_TEST_L1, " Error(%d)\n", error);
	return error;
}

static int evt_test_cmd_start_sub(void *data_p, void *user_data_p)
{
	struct evt_test_subscriber_s *control_p;
	int error = 0;
	int arg;

	evt_test_dtrace(EVT_TEST_L1, ">>>  >>>\n");

	error = get_arg(&data_p, INTEGER, (void *) &arg, sizeof(int));

	if (error < 0 || arg > EVT_TEST_SIG_TYPE_MAX) {
		pr_err("Error:%d Invalid Token(%d)\n",
			      error,
			      arg);
		return -EINVAL;
	}

	control_p = (struct evt_test_subscriber_s *)
		evt_test_subscriber_handle_store[arg-1];
	evt_test_subscribe_set_cmd(control_p, EVT_TEST_SUB_TASK_STATE_START);

	return error;
}

static int evt_test_cmd_run_sub(void *data_p, void *user_data_p)
{
	struct evt_test_subscriber_s *control_p;
	int error, arg;

	evt_test_dtrace(EVT_TEST_L1, ">>>  >>>\n");

	error = get_arg(&data_p, INTEGER, (void *) &arg, sizeof(int));
	if (error < 0 || arg > EVT_TEST_SIG_TYPE_MAX) {
		pr_err("Error:%d Invalid Token(%d)\n",
			      error,
			      arg);
		return -EINVAL;
	}

	control_p = (struct evt_test_subscriber_s *)
		evt_test_subscriber_handle_store[arg-1];
	evt_test_subscribe_set_cmd(control_p, EVT_TEST_SUB_TASK_STATE_RUN);

	return error;
}

static int evt_test_cmd_stop_sub(void *data_p, void *user_data_p)
{
	struct evt_test_subscriber_s *control_p;
	int arg, error;

	evt_test_dtrace(EVT_TEST_L1, ">>>  >>>\n");

	error = get_arg(&data_p, INTEGER, (void *) &arg, sizeof(int));
	if (error < 0 || arg > EVT_TEST_SIG_TYPE_MAX) {
		pr_err("Error:%d Invalid Token(%d)\n",
			      error,
			      arg);
		return -EINVAL;
	}
	control_p = (struct evt_test_subscriber_s *)
		evt_test_subscriber_handle_store[arg-1];
	evt_test_subscribe_set_cmd(control_p, EVT_TEST_SUB_TASK_STATE_STOP);

	return error;
}


static int evt_test_cmd_init_sub(void *data_p, void *user_data_p)
{
	struct evt_test_subscriber_s *control_p;
	int arg, error;

	evt_test_dtrace(EVT_TEST_L1, ">>>  >>>\n");

	error = get_arg(&data_p, INTEGER, (void *) &arg, sizeof(int));
	if (error < 0 || arg > EVT_TEST_SIG_TYPE_MAX) {
		pr_err("Error:%d Invalid Token(%d)\n",
			      error,
			      arg);
		return -EINVAL;
	}

	control_p = (struct evt_test_subscriber_s *)
		infra_os_malloc(sizeof(*control_p));
	if (NULL == control_p)
		return -ENOMEM;

	evt_test_subscriber_handle_store[arg-1] = (uint32_t) control_p;
	strncpy(control_p->dev_name,
		      evt_subscriber_dev_name[arg-1],
		      INFRA_MAX_DEV_NAME);

	switch (arg) {
	case 1:
		evt_test_subscribe_init_S1(control_p);
		break;

	case 2:
		evt_test_subscribe_init_S2(control_p);
		break;

	case 3:
		evt_test_subscribe_init_S3(control_p);
		break;
	}
	return error;
}


static int evt_test_cmd_term_sub(void *data_p, void *user_data_p)
{
	struct evt_test_subscriber_s *control_p;
	int arg, error;

	evt_test_dtrace(EVT_TEST_L1, ">>>  >>>\n");

	error = get_arg(&data_p, INTEGER, (void *) &arg, sizeof(int));
	if (error < 0 || arg > EVT_TEST_SIG_TYPE_MAX) {
		pr_err("Error:%d Invalid Token(%d)\n",
			      error,
			      arg);
		return -EINVAL;
	}

	control_p = (struct evt_test_subscriber_s *)
		evt_test_subscriber_handle_store[arg-1];
	evt_test_dtrace(EVT_TEST_L1, ">>> %s : Line : %d  >>>\n", __func__, __LINE__);
	evt_test_subscribe_set_cmd(control_p, EVT_TEST_SUB_TASK_STATE_TERM);

	return error;
}



static int evt_test_cmd_sub_set_subscription(void *data_p, void *user_data_p)
{
	struct evt_test_subscriber_s *control_p;
	int error = 0, arg;

	evt_test_dtrace(EVT_TEST_L1, ">>>  >>>\n");
	error = get_arg(&data_p, INTEGER, (void *) &arg, sizeof(int));
	if (error < 0 || arg > EVT_TEST_SIG_TYPE_MAX) {
		pr_err("Error:%d Invalid Token(%d)\n",
			      error,
			      arg);
		return -EINVAL;
	}

	control_p = (struct evt_test_subscriber_s *)
		evt_test_subscriber_handle_store[arg-1];

	switch (arg) {
	case 1:
		evt_test_set_subscription_S1(control_p);
		break;

	case 2:
		evt_test_set_subscription_S2(control_p);
		evt_test_dtrace(EVT_TEST_L1,
			      "\nSubscription Created\n\tDevice Name: %s\n",
			      control_p->dev_name);
		break;

	case 3:
		evt_test_set_subscription_S3(control_p);
		break;
	}
	evt_test_etrace(EVT_TEST_L1,
		      "Subscription(0x%p) created for Device(0x%p) Name:%s\n",
		      control_p->subscriber_handle,
		      control_p,
		      control_p->dev_name);

	evt_test_dtrace(EVT_TEST_L1,
		      "\nExiting from evt_test_cmd_sub_set_subscription(..)\n ");

	return error;
}

static int evt_test_cmd_sub_modify_subscription(void *data_p, void *user_data_p)
{
	struct evt_test_subscriber_s *control_p;
	int error = 0, arg;

	evt_test_dtrace(EVT_TEST_L1, ">>>  >>>\n");
	error = get_arg(&data_p, INTEGER, (void *) &arg, sizeof(int));
	if (error < 0 || arg > EVT_TEST_SIG_TYPE_MAX) {
		pr_err("Error:%d Invalid Token(%d)\n",
			      error,
			      arg);
		return -EINVAL;
	}

	control_p = (struct evt_test_subscriber_s *)
		evt_test_subscriber_handle_store[arg-1];

	switch (arg) {
	case 1:
		evt_test_modify_subscription_S1(control_p);
		break;

	case 2:
		evt_test_modify_subscription_S2(control_p);
		break;

	case 3:
		evt_test_modify_subscription_S3(control_p);
		break;
	}
	return error;
}

static int evt_test_cmd_sub_delete_subscription(void *data_p, void *user_data_p)
{
	struct evt_test_subscriber_s *control_p;
	int error = 0;
	int arg;

	evt_test_dtrace(EVT_TEST_L1, ">>>  >>>\n");
	error = get_arg(&data_p, INTEGER, (void *) &arg, sizeof(int));
	if (error < 0 || arg > EVT_TEST_SIG_TYPE_MAX) {
		pr_err("Error:%d Invalid Token(%d)\n",
			      error,
			      arg);
		return -EINVAL;
	}

	control_p = (struct evt_test_subscriber_s *)
		evt_test_subscriber_handle_store[arg-1];
	evt_test_delete_subscription(control_p);

	return 0;
}

static int evt_test_cmd_sub_set_interface(void *data_p, void *user_data_p)
{
	struct evt_test_subscriber_s *control_p;
	infra_error_code_t error = INFRA_NO_ERROR;
	int arg;

	evt_test_dtrace(EVT_TEST_L1, ">>>  >>>\n");
	error = get_arg(&data_p, INTEGER, (void *) &arg, sizeof(int));
	if (error < 0 || arg > EVT_TEST_SIG_TYPE_MAX) {
		pr_err("Error:%d Invalid Token(%d)\n",
			      error,
			      arg);
		return -EINVAL;
	}
	control_p = (struct evt_test_subscriber_s *)
		evt_test_subscriber_handle_store[arg-1];
	switch (arg) {
	case 1:
		evt_test_subscriber_S1_set_interface(control_p);
		break;

	case 2:
		evt_test_subscriber_S2_set_interface(control_p);
		break;

	case 3:
		evt_test_subscriber_S3_set_interface(control_p);
		break;
	}

	return error;
}



static int evt_test_cmd_sub_get_events(void *data_p, void *user_data_p)
{

	struct evt_test_subscriber_s *control_p;
	infra_error_code_t error = INFRA_NO_ERROR;
	int arg;

	evt_test_dtrace(EVT_TEST_L1, ">>>  >>>\n");
	error = get_arg(&data_p, INTEGER, (void *) &arg, sizeof(int));
	if (error < 0 || arg > EVT_TEST_SIG_TYPE_MAX) {
		pr_err("Error:%d Invalid Token(%d)\n",
			      error,
			      arg);
		return -EINVAL;
	}
	control_p = (struct evt_test_subscriber_s *)
		evt_test_subscriber_handle_store[arg-1];

	switch (arg) {
	case 1:
	case 2:
		pr_err("Error:%d Invalid Argument (2)\n",
			      -EINVAL);
		return -EINVAL;

	case 3:
		evt_test_sub_S3_handle_evt(control_p);
		break;
	}
	return error;
}

static int evt_test_modify_subscription(
					struct evt_test_subscriber_s *control_p)
{
	stm_object_h			object;
	stm_event_subscription_entry_t	*subscription_entry_p = NULL;
	int			error = INFRA_NO_ERROR;

	subscription_entry_p = (stm_event_subscription_entry_t *)
		control_p->subscription_entry;
	evt_test_sig_get_object(&object, "EVT_TEST_SIG_1");
	subscription_entry_p->cookie = control_p;
	subscription_entry_p->object = object;
	subscription_entry_p->event_mask = (EVT_TEST_ID_SIG_1_IPV4_EVT);

	evt_test_dtrace(EVT_TEST_L1,
		      " subscription_entry_p:0x%x  object:0x%x Mask:0x%x Cook:0x%x\n",
		      (uint32_t) subscription_entry_p,
		      (uint32_t) object,
		      subscription_entry_p->event_mask,
		      (uint32_t) control_p);

	error = stm_event_subscription_modify(control_p->subscriber_handle,
		      subscription_entry_p,
		      STM_EVENT_SUBSCRIPTION_OP_UPDATE);
	if (error != INFRA_NO_ERROR)
		error = stm_event_subscription_modify(
			      control_p->subscriber_handle,
			      subscription_entry_p,
			      STM_EVENT_SUBSCRIPTION_OP_ADD);

	if (error != 0)
		evt_test_etrace(EVT_TEST_L1,
			      " stm_event_subscription_modify failed, error:%d.\n",
			      error);

	return error;

}

static int evt_test_subscribe_interrupt_evt(void *data_p, void *user_data_p)
{

	struct evt_test_subscriber_s *control_p;
	infra_error_code_t error = INFRA_NO_ERROR;
	int arg;

	evt_test_etrace(EVT_TEST_L1, ">>>  >>>\n");
	error = get_arg(&data_p, INTEGER, (void *) &arg, sizeof(int));
	if (error < 0 || arg > EVT_TEST_SIG_TYPE_MAX) {
		pr_err("Error:%d Invalid Token(%d)\n",
			      error,
			      arg);
		pr_err("<%s:%d>", __func__, __LINE__);
		return -EINVAL;
	}
	control_p = (struct evt_test_subscriber_s *)
		evt_test_subscriber_handle_store[arg-1];
	if (control_p == NULL)
		return -EINVAL;
	switch (arg) {
	case 1:
	case 2:
	case 3:
		evt_test_modify_subscription(control_p);
		break;
	}
	return error;
}


int evt_test_delete_subscription(struct evt_test_subscriber_s *control_p)
{
	infra_error_code_t error = INFRA_NO_ERROR;

	error = stm_event_subscription_delete(control_p->subscriber_handle);
	if (error != INFRA_NO_ERROR)
		evt_test_etrace(EVT_TEST_L1,
			      "%s failed, Control:0x%p Sub:0x%p error:%d\n",
			      "stm_event_subscription_delete",
			      control_p,
			      control_p->subscriber_handle,
			      error);

	return error;
}


infra_error_code_t evt_test_subscribe_entry()
{
	infra_test_reg_param_t		cmd_param;

	INFRA_TEST_REG_CMD(cmd_param,
		      "SUB_INIT",
		      evt_test_cmd_init_sub,
		      "INIT SUBSCRIBER");
	INFRA_TEST_REG_CMD(cmd_param,
		      "SUB_START",
		      evt_test_cmd_start_sub,
		      "START SUBSCRIBER");
	INFRA_TEST_REG_CMD(cmd_param,
		      "SUB_STOP",
		      evt_test_cmd_stop_sub,
		      "STOP SUBSCRIBER");
	INFRA_TEST_REG_CMD(cmd_param,
		      "SUB_RUN",
		      evt_test_cmd_run_sub,
		      "RUN SUBSCRIBER");
	INFRA_TEST_REG_CMD(cmd_param,
		      "SUB_TERM",
		      evt_test_cmd_term_sub,
		      "TERM SUBSCRIBER");

	INFRA_TEST_REG_CMD(cmd_param,
		      "SUB_CRE_SUBSCRIP",
		      evt_test_cmd_sub_set_subscription,
		      "CREATE SUBSCRIPTION FOR SUBSCRIBER ");
	INFRA_TEST_REG_CMD(cmd_param,
		      "SUB_MOD_SUBSCRIP",
		      evt_test_cmd_sub_modify_subscription,
		      "MODIFY SUBSCRIPTION FOR SUBSCRIBER");
	INFRA_TEST_REG_CMD(cmd_param,
		      "SUB_DEL_SUBSCRIP",
		      evt_test_cmd_sub_delete_subscription,
		      "DELETE SUBSCRIPTION FOR SUBSCRIBER ");
	INFRA_TEST_REG_CMD(cmd_param,
		      "SUB_SET_INTER",
		      evt_test_cmd_sub_set_interface,
		      "SET_INTERFACE_FOR_SUBSCRIBER");
	INFRA_TEST_REG_CMD(cmd_param,
		      "SUB_GET_EVT",
		      evt_test_cmd_sub_get_events,
		      "GET EVT FOR FOR SUBSCRIBER");

	INFRA_TEST_REG_CMD(cmd_param,
		      "SUB_SUBSCRIBE_INTERRUPT_EVT",
		      evt_test_subscribe_interrupt_evt,
		      "subscribe for interrupt type evt");

	return INFRA_NO_ERROR;
}

infra_error_code_t evt_test_subscribe_exit()
{
	infra_test_reg_param_t		cmd_param;

	INFRA_TEST_DEREG_CMD(cmd_param, evt_test_cmd_init_sub);
	INFRA_TEST_DEREG_CMD(cmd_param, evt_test_cmd_start_sub);
	INFRA_TEST_DEREG_CMD(cmd_param, evt_test_cmd_run_sub);
	INFRA_TEST_DEREG_CMD(cmd_param, evt_test_cmd_stop_sub);
	INFRA_TEST_DEREG_CMD(cmd_param, evt_test_cmd_term_sub);

	INFRA_TEST_DEREG_CMD(cmd_param, evt_test_cmd_sub_set_subscription);
	INFRA_TEST_DEREG_CMD(cmd_param, evt_test_cmd_sub_modify_subscription);
	INFRA_TEST_DEREG_CMD(cmd_param, evt_test_cmd_sub_delete_subscription);
	INFRA_TEST_DEREG_CMD(cmd_param, evt_test_cmd_sub_set_interface);


	INFRA_TEST_DEREG_CMD(cmd_param, evt_test_cmd_sub_get_events);
	INFRA_TEST_DEREG_CMD(cmd_param, evt_test_subscribe_interrupt_evt);

	return INFRA_NO_ERROR;
}
