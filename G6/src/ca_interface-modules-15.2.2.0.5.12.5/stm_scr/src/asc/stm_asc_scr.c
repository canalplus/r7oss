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
 @File   stm_asc_scr.h
 @brief
*/

#include "ca_os_wrapper.h"
#include "stm_asc_scr.h"
#include "asc_internal.h"
#include "asc_hal.h"

#define DRIVER_NAME "stm_asc_scr"

/*---- Forward function declarations---------------------------*/
static int32_t asc_check_control_param(stm_asc_scr_ctrl_flags_t ctrl_flag,
			void *value_p);
static int32_t asc_check_asc_param(asc_ctrl_t *asc_ctrl_p);
static int32_t asc_check_data_param(stm_asc_scr_data_params_t *params_p);

static int32_t asc_check_control_param(stm_asc_scr_ctrl_flags_t ctrl_flag,
		void *value_p)
{
	if (!value_p) {
		asc_error_print("%s@%d  invalid value\n",
			__func__, __LINE__);
		return -EINVAL;
	}
	if (ctrl_flag > STM_ASC_SCR_MAX) {
		asc_error_print("%s@%d  ctrl_flags out of range(%d)\n",
			__func__, __LINE__, ctrl_flag);
		return -EINVAL;
	}
	return 0;
}
static int asc_check_asc_param(asc_ctrl_t *asc_ctrl_p)
{
	if (!asc_ctrl_p) {
		asc_error_print("%s@%d asc handle is invalid 0x%x\n",
				__func__, __LINE__,
				(uint32_t)asc_ctrl_p);
		return -EINVAL;
	}

	if (asc_ctrl_p->handle != (void *)asc_ctrl_p) {
		asc_error_print("%s@%d asc invalid :asc asc 0x%x, asc 0x%x\n",
				__func__, __LINE__,
				(uint32_t)asc_ctrl_p->handle,
				(uint32_t)asc_ctrl_p);
		return -EINVAL;
	}
	return 0;
}

static int32_t asc_check_data_param(stm_asc_scr_data_params_t *params_p)
{
	if (!params_p) {
		asc_error_print("%s@%d invalid data params(%p)\n",
				__func__, __LINE__, params_p);
		return -EINVAL;
	}
	if ((params_p->buffer_p == NULL) || (params_p->size == 0)) {
		asc_error_print("%s@%d invalid params :buffer %p  size %d\n",
			__func__, __LINE__,
			params_p->buffer_p,
			params_p->size);
		return -EINVAL;
	}
	return 0;
}

int32_t stm_asc_scr_new(stm_asc_scr_new_params_t *newparams_p, stm_asc_h *asc_p)
{
	ca_error_code_t error = CA_NO_ERROR;

	/* FDMA not supported yet*/
	if ((newparams_p == NULL) ||
		(newparams_p->transfer_type >= STM_ASC_SCR_TRANSFER_FDMA) ||
		(newparams_p->transfer_type < STM_ASC_SCR_TRANSFER_CPU) ||
		(newparams_p->device_data == NULL)) {
		asc_error_print("%s@%d Error(%d)\n", __func__, __LINE__, error);
		return -EINVAL;
	}

	if (!asc_p) {
		asc_error_print("%s@%d asc invalid 0x%x\n", __func__, __LINE__,
				(uint32_t)asc_p);
		return -EINVAL;
	}

	error = asc_create_new(newparams_p, asc_p);

	if (error != CA_NO_ERROR)
		asc_error_print("%s@%d:Error(%d)\n", __func__, __LINE__, error);

	return error;
}

int32_t stm_asc_scr_delete(stm_asc_h  asc)
{
	ca_error_code_t	 error = CA_NO_ERROR;
	asc_ctrl_t	*asc_ctrl_p = NULL;

	asc_ctrl_p = (asc_ctrl_t*)asc;
	error = asc_check_asc_param(asc_ctrl_p);
	if (error == 0) {
		/* Abort all activity on the UART */
		asc_delete_intance(asc);
		asc_deallocate_platform_data(asc_ctrl_p);
		/*dealloc the asc scr port memory*/
		ca_os_free((void *)asc_ctrl_p);
	}

	return error;
}

int stm_asc_scr_pad_config(stm_asc_h  asc, bool release)
{
	ca_error_code_t	 error = CA_NO_ERROR;
	asc_ctrl_t	*asc_ctrl_p = NULL;

	asc_ctrl_p = (asc_ctrl_t *)asc;
	error = asc_check_asc_param(asc_ctrl_p);
	if (error == 0)
		error = asc_pad_config(asc_ctrl_p, release);
	return error;
}

int32_t stm_asc_scr_read(stm_asc_h  asc, stm_asc_scr_data_params_t *params_p)
{
	ca_error_code_t error = CA_NO_ERROR;
	asc_ctrl_t *asc_ctrl_p = NULL;

	asc_ctrl_p = (asc_ctrl_t*)asc;
	error = asc_check_asc_param(asc_ctrl_p);
	error |= asc_check_data_param(params_p);
	if (error != 0)
		return error;

	error = asc_read(asc_ctrl_p,params_p);

	return error;
}


int32_t stm_asc_scr_write(stm_asc_h  asc, stm_asc_scr_data_params_t *params_p)
{
	ca_error_code_t error = CA_NO_ERROR;
	asc_ctrl_t *asc_ctrl_p = NULL;

	asc_ctrl_p = (asc_ctrl_t*)asc;
	error = asc_check_asc_param(asc_ctrl_p);
	error |= asc_check_data_param(params_p);
	if (error != 0)
		return error;

	error = asc_write(asc_ctrl_p, params_p);

	return error;
}


int32_t stm_asc_scr_abort(stm_asc_h  asc)
{
	ca_error_code_t error = CA_NO_ERROR;
	asc_ctrl_t *asc_ctrl_p = NULL;

	asc_ctrl_p = (asc_ctrl_t*)asc;
	error = asc_check_asc_param(asc_ctrl_p);
	if (error != 0)
		return error;

	/* Abort all activity on the ASC */
	error = asc_abort(asc_ctrl_p);
	return error;
}

int32_t stm_asc_scr_flush(stm_asc_h  asc)
{
	ca_error_code_t error = CA_NO_ERROR;
	asc_ctrl_t *asc_ctrl_p = NULL;

	asc_ctrl_p = (asc_ctrl_t*)asc;
	error = asc_check_asc_param(asc_ctrl_p);
	if (error != 0)
		return error;

	/* Abort all activity on the ASC */
	error = asc_flush(asc);
	return error;
}

int32_t stm_asc_scr_set_control(stm_asc_h  asc,
		stm_asc_scr_ctrl_flags_t ctrl_flag, uint32_t value)
{
	ca_error_code_t error = CA_NO_ERROR;
	asc_ctrl_t *asc_ctrl_p = (asc_ctrl_t *)asc;

	error = asc_check_asc_param(asc_ctrl_p);
	error |= asc_check_control_param(ctrl_flag, &value);
	if (error == 0)
		error = asc_set_ctrl(asc_ctrl_p, ctrl_flag, value);

	return error;
}

int stm_asc_scr_get_control(stm_asc_h asc,
		stm_asc_scr_ctrl_flags_t ctrl_flag,
		uint32_t *value_p)
{
	asc_ctrl_t *asc_ctrl_p = (asc_ctrl_t *)asc;
	ca_error_code_t error = CA_NO_ERROR;

	error = asc_check_asc_param(asc_ctrl_p);
	error |= asc_check_control_param(ctrl_flag, value_p);
	if (error == 0)
		asc_get_ctrl(asc_ctrl_p, ctrl_flag, value_p);

	return error;
}

int32_t stm_asc_scr_set_compound_control(stm_asc_h  asc,
		stm_asc_scr_ctrl_flags_t ctrl_flag, void *value_p)
{
	asc_ctrl_t *asc_ctrl_p = (asc_ctrl_t *)asc;
	ca_error_code_t error = CA_NO_ERROR;

	error = asc_check_asc_param(asc_ctrl_p);
	error |= asc_check_control_param(ctrl_flag, value_p);
	if (error == 0)
		asc_set_compound_ctrl(asc_ctrl_p, ctrl_flag, value_p);
	return error;
}

int32_t stm_asc_scr_get_compound_control(stm_asc_h  asc,
		stm_asc_scr_ctrl_flags_t ctrl_flag, void *value_p)
{
	asc_ctrl_t *asc_ctrl_p = (asc_ctrl_t *)asc;
	ca_error_code_t error = CA_NO_ERROR;

	error = asc_check_asc_param(asc_ctrl_p);
	error |= asc_check_control_param(ctrl_flag, value_p);
	if (error == 0)
		asc_get_compound_ctrl(asc_ctrl_p, ctrl_flag, value_p);
	return error;
}

int32_t stm_asc_scr_freeze(stm_asc_h  asc)
{
	struct clk *clk;
	asc_ctrl_t *asc_ctrl_p = (asc_ctrl_t *)asc;
	struct platform_device *pdev = asc_ctrl_p->device.pdev;
	ca_error_code_t error = CA_NO_ERROR;
	asc_regs_t *asc_regs_p =
		(asc_regs_t *)asc_ctrl_p->device.base;
	asc_debug_print("<%s> [PM] ASC suspend\n", __func__);
	/* release PIOs */
	error = asc_pad_config(asc_ctrl_p, true);
	if (error) {
		asc_error_print("<%s> [PM] ASc pad release failed\n", __func__);
		return error ;
	}

#ifdef ASC_DEBUG
	dumpreg(asc_ctrl_p);
#endif
	/* save registers */
	asc_store_reg_set(asc_regs_p, &asc_ctrl_p->lpm_data.asc_regs);

#ifdef CONFIG_ARCH_STI
	clk = devm_clk_get(&pdev->dev, NULL);
#else
	clk = clk_get(&pdev->dev, "comms_clk");
#endif
	if (IS_ERR(clk)) {
		asc_error_print("<%s> [PM] Comms clock not found!%p\n",
				__func__,
				&pdev->dev);
		return -EINVAL;
	}
	clk_disable(clk);
	return 0;
}

int32_t stm_asc_scr_restore(stm_asc_h  asc)
{
	struct clk *clk;
	asc_ctrl_t *asc_ctrl_p = (asc_ctrl_t *)asc;
	struct platform_device *pdev = asc_ctrl_p->device.pdev;
	asc_regs_t *asc_regs_p =
			(asc_regs_t *)asc_ctrl_p->device.base;
	ca_error_code_t error = CA_NO_ERROR;
	asc_debug_print("<%s> [PM] ASC resume\n", __func__);
	/*reclaim PIOs*/
	error = asc_pad_config(asc_ctrl_p, false);
	if (error) {
		asc_error_print("<%s> [PM] ASc pad claim failed\n", __func__);
		return error ;
	}
	/*restore registers*/
	asc_load_reg_set(&asc_ctrl_p->lpm_data.asc_regs, asc_regs_p);
	asc_set_baudrate(asc_ctrl_p, asc_ctrl_p->lpm_data.baudrate);

#ifdef CONFIG_ARCH_STI
	clk = devm_clk_get(&pdev->dev, NULL);
#else
	clk = clk_get(&pdev->dev, "comms_clk");
#endif
	if (IS_ERR(clk)) {
		asc_error_print("<%s> [PM] Comms clock not found!%p\n",
				__func__,
				&pdev->dev);
		return -EINVAL;
	}
#ifdef ASC_DEBUG
	dumpreg(asc_ctrl_p);
#endif
	clk_enable(clk);
	return 0;
}
