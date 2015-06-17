/*---------------------------------------------------------------------------
* /drivers/stm/lpm.c
* Copyright (C) 2010 STMicroelectronics Limited
* Author:
* May be copied or modified under the terms of the GNU General Public
* License.  See linux/COPYING for more information.
*----------------------------------------------------------------------------
*/
#ifndef CONFIG_ARCH_STI
#include <linux/stm/lpm.h>
#endif
#include <linux/module.h>
#include <linux/moduleparam.h>

#include "infra_os_wrapper.h"
#include "infra_test_interface.h"
#include "infra_token.h"

MODULE_AUTHOR("STMicroelectronics  <www.st.com>");
MODULE_DESCRIPTION("setup for wakeup device");
MODULE_LICENSE("GPL");

#ifdef CONFIG_STM_LPM
infra_error_code_t pm_send_cec_user_msg(void *data_p, void *usr_data)
{
	union stm_lpm_cec_params cec_params;
	int arg, number_of_tokens, i;
	infra_error_code_t error = 0;
	number_of_tokens = get_number_of_tokens((char *)data_p );

	if (number_of_tokens == 0) {
		error = -EINVAL;
		pr_err("Error:%d No Operand Found.\n", error);
		return -EINVAL;
	}

	/* Get Instance from Argument 1 */
	error = get_arg(&data_p, INTEGER ,
			(void *)&cec_params.cec_msg.opcode, sizeof(int));

	if (error < 0) {
		pr_err("Error:%d Invalid Opcode\n",
				error);
		return -EINVAL;
	}

	/*Get Remaining Arguments */
	number_of_tokens = get_number_of_tokens((char *)data_p );

	if (number_of_tokens == 0) {
		error = -EINVAL;
		pr_err("Error:%d No Operand Found.", error);
		return -EINVAL;
	}
	for (i = 0; i < number_of_tokens; i++) {
		error = get_arg(&data_p,
				INTEGER,
				(void *)&arg,
				sizeof(int));
		if (error == -EINVAL) {
			pr_err("Error:%d Invalid i"\
					"Argument operand\n",
					error);
			return -EINVAL;
		}
		cec_params.cec_msg.oprand[i]= arg;
	}
	cec_params.cec_msg.size = number_of_tokens + 1;
	error = stm_lpm_cec_config(STM_LPM_CONFIG_CEC_WU_CUSTOM_MSG,
					&cec_params);
	if (error < 0)
		pr_err(" cec config failed\n");

	return 0;
}

infra_error_code_t pm_send_keyscan_user_msg(void *data_p, void *usr_data)
{
	short int key_data;
	int number_of_tokens;
	infra_error_code_t error = 0;
	number_of_tokens = get_number_of_tokens((char *)data_p );

	if (number_of_tokens == 0) {
		error = -EINVAL;
		pr_err("Error:%d No key Found.\n", error);
		return -EINVAL;
	}

	/* Get Instance from Argument 1 */
	error = get_arg(&data_p, INTEGER ,
			(void *)&key_data, sizeof(int));

	if (error < 0) {
		pr_err("Error:%d Invalid key code\n",
				error);
		return -EINVAL;
	}

	error = stm_lpm_setup_keyscan(key_data);
	if (error < 0)
		pr_err(" keyscan config failed\n");
	return 0;

}
#else
infra_error_code_t pm_send_cec_user_msg(void *data_p, void *usr_data)
{
	return 0;
}
infra_error_code_t pm_send_keyscan_user_msg(void *data_p, void *usr_data)
{
	return 0;
}
#endif

static int __init infra_wakeup_reason_init(void)
{
	infra_test_reg_param_t		cmd_param;
	INFRA_TEST_REG_CMD(cmd_param, "PM_SEND_CEC_USER_MSG",
			pm_send_cec_user_msg, "cec setup for sbc");
	INFRA_TEST_REG_CMD(cmd_param, "PM_SEND_KEYSCAN_USER_MSG",
			pm_send_keyscan_user_msg, "keyscan setup for sbc");
	return 0;
}

static void __exit infra_wakeup_reason_exit(void)
{
	infra_test_reg_param_t          cmd_param;
	INFRA_TEST_DEREG_CMD(cmd_param, pm_send_cec_user_msg);
	INFRA_TEST_DEREG_CMD(cmd_param, pm_send_keyscan_user_msg);
}

module_init(infra_wakeup_reason_init);
module_exit(infra_wakeup_reason_exit);

