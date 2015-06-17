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
   @file     ca_test_interface.h
   @brief

 */

#ifndef CA_TEST_INTERFACE_H_
#define CA_TEST_INTERFACE_H_

#include "ca_queue.h"
#include "ca_token.h"

#define CA_TEST_PRINT			1
#define CA_TEST_DEBUG			0

#define CA_TEST_MAX_STR_LEN			50
#define CA_TEST_MAX_TOKENS			10
#define CA_TEST_MAX_CMD_DESC_LEN		256

#if CA_TEST_DEBUG
    #define ca_test_debug(fmt, ...) printk(fmt,  ##__VA_ARGS__)
#else
    #define ca_test_debug(fmt, ...) while(0)
#endif

#if CA_TEST_PRINT
    #define ca_test_print(fmt, ...) printk(fmt,  ##__VA_ARGS__)
#else
    #define ca_test_print(fmt, ...) while(0)
#endif



typedef enum{
	CA_TEST_INST_1 = 1,
	CA_TEST_INST_2,
	CA_TEST_INST_3,
	CA_TEST_INST_4,
	CA_TEST_INST_MAX
}ca_test_module_instance_t;

typedef enum{
	CA_TEST_CMD_INIT = 1,
	CA_TEST_CMD_START,
	CA_TEST_CMD_RUN,
	CA_TEST_CMD_SET_PARAM,
	CA_TEST_CMD_STOP,
	CA_TEST_CMD_TERM,
	CA_TEST_CMD_RESERVED
}ca_test_cmd_type_t;

typedef struct ca_q_s		ca_test_interface_q_t;

typedef char ca_test_str_t[CA_TEST_MAX_STR_LEN];
typedef char ca_test_info_t[CA_TEST_MAX_CMD_DESC_LEN];

typedef void (*ca_test_func_t)(void 	*test_param_p, void * user_data_p);

typedef struct ca_test_reg_param_s{
	ca_test_str_t		func_str;
	char			info[CA_TEST_MAX_CMD_DESC_LEN];
	ca_test_func_t		interface_func_p;
	void			*user_data_p;
}ca_test_reg_param_t;

typedef struct ca_test_param_s{
	ca_test_str_t		param_str;
	void			*user_data_p;
}ca_test_param_t;


typedef struct ca_test_interface_param_s{
	ca_test_str_t		func_str;
	ca_test_info_t		info;
	ca_test_func_t		interface_func_p;
	void			*user_data_p;
	ca_test_interface_q_t	interface_q_node;
}ca_test_interface_param_t;

typedef struct ca_test_cmd_param_s{
	ca_test_str_t		tokens[CA_TEST_MAX_TOKENS];
	uint32_t		num_tokens;
	uint32_t		cur_index;
	ca_test_func_t		interface_func_fp;
}ca_test_cmd_param_t;




int ca_test_register_cmd(ca_test_reg_param_t *cmd_param_p);
int ca_test_deregister_cmd(ca_test_reg_param_t *cmd_param_p);
int ca_test_register_param(ca_test_param_t *int_param_p);
int ca_test_deregister_param(ca_test_param_t *int_param_p);
int ca_test_get_param_data(char *param_str_p,
					ca_type_t type,
					void *data_p,
					uint8_t size);

int ca_test_term_module(void);
int ca_test_init_module(void);

#define CA_TEST_REG_CMD(cmd_param, func_macro, func_p, info)	\
do { \
	strncpy(cmd_param.func_str , func_macro, (CA_TEST_MAX_STR_LEN-1)); \
	cmd_param.interface_func_p = (ca_test_func_t)func_p; \
	ca_test_register_cmd(&cmd_param); \
} while (0)

#define CA_TEST_DEREG_CMD(cmd_param, func_p) \
do { \
	cmd_param.interface_func_p = (ca_test_func_t)func_p; \
	ca_test_deregister_cmd(&cmd_param); \
} while (0)

#define CA_TEST_REG_DATA(int_param, param_macro, data, val) \
do { \
	strncpy(int_param.param_str , param_macro, (CA_TEST_MAX_STR_LEN-1)); \
	val = data; \
	int_param.user_data_p = (void *)&val; \
	ca_test_register_param(&int_param); \
} while (0)

#define CA_TEST_DEREG_DATA(int_param, param_macro) \
do { \
	strncpy(int_param.param_str , param_macro, (CA_TEST_MAX_STR_LEN-1)); \
	ca_test_deregister_param(&int_param); \
} while (0)

#endif
