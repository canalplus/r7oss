/************************************************************************
Copyright (C) 2011 STMicroelectronics. All Rights Reserved.

This file is part of the stm_fe Library.

stm_fe is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License version 2 as published by the
Free Software Foundation.

stm_fe is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with stm_fe; see the file COPYING.  If not, write to the Free Software
Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

The stm_fe Library may alternatively be licensed under a proprietary
license from ST.

Source file name : stm_cec.c
Author :           bharatj

API dedinitions for demodulation device

Date        Modification                                    Name
----        ------------                                    --------
05-Mar-12   Created                                         bharatj

************************************************************************/

#include "stm_cec.h"
#include "cec_core.h"
#include "cec_debug.h"

int stm_cec_new(uint32_t dev_id, stm_cec_h *device)
{
	int		error = INFRA_NO_ERROR;

	cec_enter_critical_section();
	error = cec_validate_init_param(dev_id, device);
	if (error) {
		cec_exit_critical_section();
		return error;
	}
	error = cec_alloc_control_param(dev_id, device);
	if (error) {
		cec_exit_critical_section();
		return error;
	}
	cec_exit_critical_section();
	error = cec_fill_control_param(dev_id , *device);
	if (error)
		return error;
	error = cec_do_hw_init(dev_id , *device);

	if (!error)
		error = cec_check_state(*device, CEC_STATE_QUERY);

	return error;
}
EXPORT_SYMBOL(stm_cec_new);

int stm_cec_delete(stm_cec_h device)
{
	int		error = INFRA_NO_ERROR;

	cec_enter_critical_section();
	error = cec_validate_del_param(device);
	if (error != 0)
		return error;

	cec_exit_critical_section();

	error = cec_check_state(device, CEC_STATE_CLOSE);
	if (error == 0) {
		cec_do_hw_term(device);
		cec_dealloc_control_param(device);
	}

	return error;
}
EXPORT_SYMBOL(stm_cec_delete);

int
stm_cec_send_msg(stm_cec_h  device,
		uint8_t retries,
		stm_cec_msg_t *cec_msg_p)
{
	infra_error_code_t error, retry_err;

	if (device == NULL || cec_msg_p == NULL) {
		cec_error_trace(CEC_API,
				"INVALID PARAM[handle=%p msg:%p]\n",
				device,
				cec_msg_p);
		return -EINVAL;
	}
	if ((CEC_VALIDATE_HANDLE(device->magic_num)) != 0) {
		cec_error_trace(CEC_API,
				"INVALID HANDLE[handle=%p ]\n",
				device);
		return -ENODEV;
	}

	if (retries == 0) {
		retry_err = -EINVAL;
		/* to continue in-spec in case of non-handled error */
		device->retries = 1;
	} else if (retries > 5) {
		retry_err = -EINVAL;
		/* to continue in-spec in case of non-handled error */
		device->retries = 5;
	} else {
		 /* Good Param */
		device->retries = retries;
	}

	if (device->logical_addr_set) {
		error = cec_check_state(device, CEC_STATE_BUSY);
		if (error == INFRA_NO_ERROR) {
			cec_debug_trace(CEC_API, "\n");
			error = cec_send_msg(device, cec_msg_p);
		}
		error |= cec_check_state(device, CEC_STATE_READY);
	} else {
		error = cec_send_msg(device, cec_msg_p);
	}
	return error;
}
EXPORT_SYMBOL(stm_cec_send_msg);

int
stm_cec_receive_msg(stm_cec_h  device, stm_cec_msg_t *cec_msg_p)
{
	infra_error_code_t	error = 0;

	if (device == NULL || cec_msg_p == NULL) {
		cec_error_trace(CEC_API,
				"INVALID PARAM[handle=%p msg:%p]\n",
				device,
				cec_msg_p);
		return -EINVAL;
	}
	if ((CEC_VALIDATE_HANDLE(device->magic_num)) != 0) {
		cec_error_trace(CEC_API,
				"INVALID HANDLE[handle=%p ]\n",
				device);
		return -ENODEV;
	}

	error = cec_retreive_message(device, cec_msg_p);
	return error;

}
EXPORT_SYMBOL(stm_cec_receive_msg);

int
stm_cec_set_compound_control(stm_cec_h  device,
				stm_cec_ctrl_flag_t ctrl_flag,
				stm_cec_ctrl_type_t *ctrl_data_p)
{
	infra_error_code_t	error = 0;
	if (device == NULL || ctrl_data_p == NULL) {
				cec_error_trace(CEC_API,
				"INVALID PARAM[handle=%p ctrl_data_p:%p]\n",
				device,
				ctrl_data_p);
		return -EINVAL;
	}

	if ((CEC_VALIDATE_HANDLE(device->magic_num)) != 0) {
			cec_error_trace(CEC_API,
					"INVALID HANDLE[handle=%p ]\n",
					device);
			return -ENODEV;
	}

	error = cec_core_set_ctrl(device, ctrl_flag, ctrl_data_p);
	return error;
}
EXPORT_SYMBOL(stm_cec_set_compound_control);

int
stm_cec_get_compound_control(stm_cec_h device,
				stm_cec_ctrl_flag_t ctrl_flag,
				stm_cec_ctrl_type_t *ctrl_data_p)
{
	infra_error_code_t			error = 0;
	if (device == NULL || ctrl_data_p == NULL) {
		cec_error_trace(CEC_API,
		"INVALID PARAM[handle=%p ctrl_data_p:%p]\n",
		device,
		ctrl_data_p);
		return -EINVAL;
	}
	if ((CEC_VALIDATE_HANDLE(device->magic_num)) != 0) {
		cec_error_trace(CEC_API,
				"INVALID HANDLE[handle=%p ]\n",
				device);
		return -EINVAL;
	}

	error = cec_core_get_ctrl(device, ctrl_flag, ctrl_data_p);
	return error;

}
EXPORT_SYMBOL(stm_cec_get_compound_control);
