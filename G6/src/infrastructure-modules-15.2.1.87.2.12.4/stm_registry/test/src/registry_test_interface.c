/*****************************************************************************/
/* COPYRIGHT (C) 2009 STMicroelectronics - All Rights Reserved               */
/* ST makes no warranty express or implied including but not limited to,     */
/* any warranty of                                                           */
/*                                                                           */
/*   (i)  merchantability or fitness for a particular purpose and/or         */
/*   (ii) requirements, for a particular purpose in relation to the LICENSED */
/*        MATERIALS, which is provided AS IS, WITH ALL FAULTS. ST does not   */
/*        represent or warrant that the LICENSED MATERIALS provided here     */
/*        under is free of infringement of any third party patents,          */
/*        copyrights, trade secrets or other intellectual property rights.   */
/*        ALL WARRANTIES, CONDITIONS OR OTHER TERMS IMPLIED BY LAW ARE       */
/*        EXCLUDED TO THE FULLEST EXTENT PERMITTED BY LAW                    */
/*                                                                           */
/*****************************************************************************/

/*
   @file    registry_test_interface.c
   @brief

 */

#include "infra_proc_interface.h"
#include "infra_queue.h"
#include "infra_os_wrapper.h"
#include "registry_test_interface.h"


static infra_test_interface_q_t		*interface_param_q_head_p;
static infra_os_mutex_t			lock_test_q_param_p;
static void				*infra_test_proc_control_p;


static int infra_test_proc_read(infra_proc_msg_interface_t *msg_p);
static void infra_test_proc_write(infra_proc_msg_interface_t *msg_p);


infra_error_code_t infra_test_deregister_cmd(infra_test_reg_param_t *cmd_param_p)
{
	infra_error_code_t			error = INFRA_NO_ERROR;
	infra_test_interface_q_t		*interface_q_param_p;
	infra_test_interface_param_t		*cur_interface_param_p;

	infra_test_debug(">>> %s@%d >>>\n", __FUNCTION__, __LINE__);
	infra_os_mutex_lock(lock_test_q_param_p);
	error = infra_q_remove_node(&(interface_param_q_head_p), (uint32_t) (cmd_param_p->interface_func_p), &interface_q_param_p);
	cur_interface_param_p = INFRA_Q_GET_DATA(interface_q_param_p, infra_test_interface_param_t);
	infra_os_free(cur_interface_param_p);
	infra_os_mutex_unlock(lock_test_q_param_p);
	infra_test_debug("<<< %s@%d <<<\n", __FUNCTION__, __LINE__);

	return error;
}


infra_error_code_t infra_test_register_cmd(infra_test_reg_param_t *cmd_param_p)
{
	infra_test_interface_param_t			*interface_param_p;

	infra_test_debug(">>> %s@%d >>>\n", __FUNCTION__, __LINE__);
	interface_param_p = (infra_test_interface_param_t *) infra_os_malloc(sizeof(*interface_param_p));
	if (NULL == interface_param_p) {
		infra_error_print("%s@%d: Error in command registration\n", __FUNCTION__, __LINE__);
		return -ENOMEM;
	}

	memset((void *) interface_param_p->func_str, '\0', INFRA_TEST_MAX_STR_LEN);
	memset((void *) interface_param_p->info, '\0', INFRA_TEST_MAX_CMD_DESC_LEN);
	strncpy(interface_param_p->func_str, cmd_param_p->func_str, (INFRA_TEST_MAX_STR_LEN - 1));
	strncpy(interface_param_p->info, cmd_param_p->info, (INFRA_TEST_MAX_CMD_DESC_LEN - 1));
	interface_param_p->interface_func_p = cmd_param_p->interface_func_p;
	interface_param_p->user_data_p = cmd_param_p->user_data_p;

	infra_test_debug("%15s:%d - %s\n", interface_param_p->func_str, strlen(interface_param_p->func_str), interface_param_p->info);

	infra_os_mutex_lock(lock_test_q_param_p);
	infra_q_insert_node(&(interface_param_q_head_p), &(interface_param_p->interface_q_node), (uint32_t) interface_param_p->interface_func_p, (void *) interface_param_p);
	infra_os_mutex_unlock(lock_test_q_param_p);

	infra_test_debug("<<< %s@%d <<<\n", __FUNCTION__, __LINE__);

	return INFRA_NO_ERROR;
}

infra_error_code_t infra_test_init_module(void)
{
	infra_proc_create_param_t		proc_create_param;

	infra_test_print(">>> %s@%d >>>\n", __FUNCTION__, __LINE__);

	infra_os_mutex_initialize(&(lock_test_q_param_p));
	infra_test_print(">>> %s@%d MUEX:0x%x\n", __FUNCTION__, __LINE__, (uint32_t) lock_test_q_param_p);
	strncpy(proc_create_param.dir_name[INFRA_PROC_DIR_PARENT], "registry", INFRA_MAX_PROC_DIR_NAME);
	strncpy(proc_create_param.dir_name[INFRA_PROC_NAME_SELF], "registry_test", INFRA_MAX_PROC_DIR_NAME);

	proc_create_param.msg_buffer_max_size = 30;
	proc_create_param.read_fp = infra_test_proc_read;
	proc_create_param.write_fp = infra_test_proc_write;

	infra_test_proc_control_p = infra_create_proc_entry(&proc_create_param);
	infra_test_print("<<< %s@%d <<<\n", __FUNCTION__, __LINE__);

	return INFRA_NO_ERROR;

}


infra_error_code_t infra_test_term_module(void)
{
	infra_error_code_t			error = INFRA_NO_ERROR;
	infra_test_interface_q_t		*interface_q_param_p;
	infra_test_interface_param_t		*cur_interface_param_p;

	infra_test_print(">>> %s@%d >>>\n", __FUNCTION__, __LINE__);

	infra_os_mutex_lock(lock_test_q_param_p);
	while (interface_param_q_head_p != NULL) {
		cur_interface_param_p = INFRA_Q_GET_DATA(interface_param_q_head_p, infra_test_interface_param_t);
		error = infra_q_remove_node(&(interface_param_q_head_p), (uint32_t) (cur_interface_param_p->interface_func_p), &interface_q_param_p);
		infra_os_free(cur_interface_param_p);
	}
	infra_os_mutex_unlock(lock_test_q_param_p);

	infra_os_mutex_terminate(lock_test_q_param_p);
	infra_remove_proc_entry(infra_test_proc_control_p);

	infra_test_print("<<< %s@%d <<<\n", __FUNCTION__, __LINE__);

	return error;

}

int infra_test_proc_read(infra_proc_msg_interface_t *msg_p)
{
	infra_test_interface_q_t		*cur_interface_q_param_p;
	infra_test_interface_param_t		*cur_interface_param_p;
	uint8_t					local_len = 0;

	cur_interface_q_param_p = interface_param_q_head_p;
	msg_p->msg_len = 0;
	while (cur_interface_q_param_p != NULL) {
		cur_interface_param_p = INFRA_Q_GET_DATA(cur_interface_q_param_p, infra_test_interface_param_t);
		local_len = sprintf(msg_p->msg_p, "%15s:\n", cur_interface_param_p->func_str);
		msg_p->msg_len += local_len;
		infra_test_debug("%15s - %s\n", cur_interface_param_p->func_str, cur_interface_param_p->info);
		cur_interface_q_param_p = INFRA_Q_GET_NEXT((&(cur_interface_param_p->interface_q_node)));
		msg_p->msg_p += local_len;
		local_len = 0;
	}

	return msg_p->msg_len;
}


void infra_test_proc_write(infra_proc_msg_interface_t *msg_p)
{
	infra_test_interface_q_t		*cur_interface_q_param_p;
	infra_test_interface_param_t		*cur_interface_param_p;
	infra_test_func_t			interface_func_p = NULL;
	void					*dummy_p = NULL;
	uint8_t					cmp_result = 0;

	cur_interface_q_param_p = interface_param_q_head_p;
	msg_p->msg_p[msg_p->msg_len-1] = '\0';
	while (cur_interface_q_param_p != NULL) {
		cur_interface_param_p = INFRA_Q_GET_DATA(cur_interface_q_param_p, infra_test_interface_param_t);
		cmp_result = strcmp(msg_p->msg_p, cur_interface_param_p->func_str);
		infra_test_debug("%s@%d Arg:%s:%d List:%s\n", __FUNCTION__, __LINE__, msg_p->msg_p, msg_p->msg_len, cur_interface_param_p->func_str);
		if (!(cmp_result)) {
			interface_func_p = (infra_test_func_t) cur_interface_param_p->interface_func_p;
			break;
		}
		cur_interface_q_param_p = INFRA_Q_GET_NEXT((&(cur_interface_param_p->interface_q_node)));
	}

	if (interface_func_p) {
		infra_test_debug(">>> %s@%d >>>\n", __FUNCTION__, __LINE__);
		interface_func_p(dummy_p, cur_interface_param_p->user_data_p);
	}

	return;
}
