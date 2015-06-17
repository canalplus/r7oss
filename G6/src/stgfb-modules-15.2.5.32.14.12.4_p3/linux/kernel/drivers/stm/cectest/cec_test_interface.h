/***********************************************************************
 *
 * File: linux/kernel/drivers/stm/cectest/cec_test_interface.h
 * Copyright (c) 2005-2014 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/


/*
   @file     cec_test_interface.h
   @brief

 */

#ifndef CEC_TEST_INTERFACE_H_
#define CEC_TEST_INTERFACE_H_

#include "cec_token.h"
#include <cec_core.h>

#define CEC_TEST_PRINT			1
#define CEC_TEST_DEBUG			0

#define CEC_TEST_MAX_STR_LEN			50
#define CEC_TEST_MAX_TOKENS			10
#define CEC_TEST_MAX_CMD_DESC_LEN		256

#if CEC_TEST_DEBUG
    #define cec_test_debug(fmt, ...) printk(fmt,  ##__VA_ARGS__)
#else
    #define cec_test_debug(fmt, ...) while(0)
#endif

#if CEC_TEST_PRINT
    #define cec_test_print(fmt, ...) printk(fmt,  ##__VA_ARGS__)
#else
    #define cec_test_print(fmt, ...) while(0)
#endif



typedef enum{
  CEC_TEST_INST_1 = 1,
  CEC_TEST_INST_2,
  CEC_TEST_INST_3,
  CEC_TEST_INST_4,
  CEC_TEST_INST_MAX
}cec_test_module_instance_t;

typedef enum{
  CEC_TEST_CMD_INIT = 1,
  CEC_TEST_CMD_START,
  CEC_TEST_CMD_RUN,
  CEC_TEST_CMD_SET_PARAM,
  CEC_TEST_CMD_STOP,
  CEC_TEST_CMD_TERM,
  CEC_TEST_CMD_RESERVED
}cec_test_cmd_type_t;

typedef struct cec_q_s		cec_test_interface_q_t;

typedef char cec_test_str_t[CEC_TEST_MAX_STR_LEN];
typedef char cec_test_info_t[CEC_TEST_MAX_CMD_DESC_LEN];

typedef void (*cec_test_func_t)(void *test_param_p, void * user_data_p);

typedef struct cec_test_reg_param_s{
	cec_test_str_t		func_str;
	char				info[CEC_TEST_MAX_CMD_DESC_LEN];
	cec_test_func_t		interface_func_p;
	void				*user_data_p;
}cec_test_reg_param_t;

typedef struct cec_test_param_s{
	cec_test_str_t		param_str;
	void				*user_data_p;
}cec_test_param_t;


typedef struct cec_test_interface_param_s{
	cec_test_str_t		func_str;
	cec_test_info_t		info;
	cec_test_func_t		interface_func_p;
	void				*user_data_p;
	cec_test_interface_q_t	interface_q_node;
}cec_test_interface_param_t;

typedef struct cec_test_cmd_param_s{
	cec_test_str_t		tokens[CEC_TEST_MAX_TOKENS];
	uint32_t			num_tokens;
	uint32_t			cur_index;
	cec_test_func_t		interface_func_fp;
}cec_test_cmd_param_t;

int cec_test_register_cmd(cec_test_reg_param_t *cmd_param_p);
int cec_test_deregister_cmd(cec_test_reg_param_t *cmd_param_p);
int cec_test_register_param(cec_test_param_t *int_param_p);
int cec_test_deregister_param(cec_test_param_t *int_param_p);
int cec_test_get_param_data(char *param_str_p,
					cec_type_t type,
					void *data_p,
					uint8_t size);
int  cec_init_test_module(void);
void cec_term_test_module(void);

void * cec_allocate (unsigned int size );
int cec_free(void *Address);
int cec_thread_create(struct task_struct  ** thread,
                   void (*task_entry)(void* param),
                   void * parameter,
                   const char *name,
                   int        *thread_settings);
int cec_wait_thread(struct task_struct ** task);
void cec_sleep_milli_sec( unsigned int Value );
int cec_q_remove_node(cec_q_t **head_p, uint32_t key, cec_q_t **node_p);
int cec_q_insert_node(cec_q_t **head_p, cec_q_t *node_p, uint32_t key, void *data_p);

#define CEC_TEST_REG_CMD(cmd_param, func_macro, func_p, info)		strncpy(cmd_param.func_str , func_macro, (CEC_TEST_MAX_STR_LEN-1)); \
									cmd_param.interface_func_p = (cec_test_func_t)func_p; \
									cec_test_register_cmd(&cmd_param)

#define CEC_TEST_DEREG_CMD(cmd_param, func_p)		        	cmd_param.interface_func_p = (cec_test_func_t)func_p; \
									cec_test_deregister_cmd(&cmd_param)




#define CEC_TEST_REG_DATA(int_param, param_macro, data, val)		strncpy(int_param.param_str , param_macro, (CEC_TEST_MAX_STR_LEN-1)); \
									val = data;\
									int_param.user_data_p = (void *)&val; \
									cec_test_register_param(&int_param)

#define CEC_TEST_DEREG_DATA(int_param, param_macro)		        strncpy(int_param.param_str , param_macro, (CEC_TEST_MAX_STR_LEN-1)); \
									cec_test_deregister_param(&int_param)
#endif
