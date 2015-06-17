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
   @file     cec_test_interface.h
   @brief

 */

#ifndef CEC_TEST_INTERFACE_H_
#define CEC_TEST_INTERFACE_H_

#include "infra_queue.h"
#include "infra_token.h"

#define INFRA_TEST_PRINT			1
#define INFRA_TEST_DEBUG			0

#define INFRA_TEST_MAX_STR_LEN			50
#define INFRA_TEST_MAX_TOKENS			10
#define INFRA_TEST_MAX_CMD_DESC_LEN		256

#if INFRA_TEST_DEBUG
    #define infra_test_debug(fmt, ...) pr_info(fmt,  ##__VA_ARGS__)
#else
    #define infra_test_debug(fmt, ...) while(0)
#endif

#if INFRA_TEST_PRINT
    #define infra_test_print(fmt, ...) pr_info(fmt,  ##__VA_ARGS__)
#else
    #define infra_test_print(fmt, ...) while(0)
#endif



typedef enum{
	INFRA_TEST_INST_1 = 1,
	INFRA_TEST_INST_2,
	INFRA_TEST_INST_3,
	INFRA_TEST_INST_4,
	INFRA_TEST_INST_MAX
}infra_test_module_instance_t;

typedef enum{
	INFRA_TEST_CMD_INIT = 1,
	INFRA_TEST_CMD_START,
	INFRA_TEST_CMD_RUN,
	INFRA_TEST_CMD_SET_PARAM,
	INFRA_TEST_CMD_STOP,
	INFRA_TEST_CMD_TERM,
	INFRA_TEST_CMD_RESERVED
}infra_test_cmd_type_t;

typedef struct infra_q_s		infra_test_interface_q_t;

typedef char infra_test_str_t[INFRA_TEST_MAX_STR_LEN];
typedef char infra_test_info_t[INFRA_TEST_MAX_CMD_DESC_LEN];

typedef void (*infra_test_func_t)(void 	*test_param_p, void * user_data_p);

typedef struct infra_test_reg_param_s{
	infra_test_str_t		func_str;
	char				info[INFRA_TEST_MAX_CMD_DESC_LEN];
	infra_test_func_t		interface_func_p;
	void				*user_data_p;
}infra_test_reg_param_t;

typedef struct infra_test_param_s{
	infra_test_str_t		param_str;
	void				*user_data_p;
}infra_test_param_t;


typedef struct infra_test_interface_param_s{
	infra_test_str_t		func_str;
	infra_test_info_t		info;
	infra_test_func_t		interface_func_p;
	void				*user_data_p;
	infra_test_interface_q_t	interface_q_node;
}infra_test_interface_param_t;

typedef struct infra_test_cmd_param_s{
	infra_test_str_t		tokens[INFRA_TEST_MAX_TOKENS];
	uint32_t			num_tokens;
	uint32_t			cur_index;
	infra_test_func_t		interface_func_fp;
}infra_test_cmd_param_t;




infra_error_code_t infra_test_register_cmd(infra_test_reg_param_t *cmd_param_p);
infra_error_code_t infra_test_deregister_cmd(infra_test_reg_param_t *cmd_param_p);
infra_error_code_t infra_test_register_param(infra_test_param_t *int_param_p);
infra_error_code_t infra_test_deregister_param(infra_test_param_t *int_param_p);
infra_error_code_t infra_test_get_param_data(char *param_str_p,
					infra_type_t type,
					void *data_p,
					uint8_t size);

infra_error_code_t infra_test_term_module(void);
infra_error_code_t infra_test_init_module(void);

#define INFRA_TEST_REG_CMD(cmd_param, func_macro, func_p, info)		strncpy(cmd_param.func_str , func_macro, (INFRA_TEST_MAX_STR_LEN-1)); \
									cmd_param.interface_func_p = (infra_test_func_t)func_p; \
									infra_test_register_cmd(&cmd_param)

#define INFRA_TEST_DEREG_CMD(cmd_param, func_p)		        	cmd_param.interface_func_p = (infra_test_func_t)func_p; \
									infra_test_deregister_cmd(&cmd_param)




#define INFRA_TEST_REG_DATA(int_param, param_macro, data, val)		strncpy(int_param.param_str , param_macro, (INFRA_TEST_MAX_STR_LEN-1)); \
									val = data;\
									int_param.user_data_p = (void *)&val; \
									infra_test_register_param(&int_param)

#define INFRA_TEST_DEREG_DATA(int_param, param_macro)		        strncpy(int_param.param_str , param_macro, (INFRA_TEST_MAX_STR_LEN-1)); \
									infra_test_deregister_param(&int_param)



#endif
