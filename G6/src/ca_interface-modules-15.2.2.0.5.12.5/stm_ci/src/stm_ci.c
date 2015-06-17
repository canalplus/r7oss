#include "stm_ci.h"
#include "ci_internal.h"
#include "ci_utils.h"
#include "ci_hal.h"

int stm_ci_new(uint32_t	dev_id, stm_ci_h *device_p)
{
	int	error = CA_NO_ERROR;
	stm_ci_t *ci_p	= NULL;
	bool status = false;
	ci_enter_critical_section();
	error =	ci_validate_init_param(dev_id, device_p);

	if (error != CA_NO_ERROR)
		goto api_exit;

	error =	ci_alloc_control_param(dev_id, device_p);
	if (error != CA_NO_ERROR)
		goto api_exit;

	ci_p = *device_p;

	ci_fill_control_param(dev_id , ci_p);
	error =	ci_internal_new(dev_id , ci_p);
	if (error != CA_NO_ERROR)
		goto api_exi1;

	/* check card present or not */
	error =	ci_internal_get_slot_status(ci_p, &status);
	if (error != CA_NO_ERROR) {
		error =	ci_check_state(ci_p, CI_STATE_CARD_NOT_PRESENT);
		ci_p->magic_num	= MAGIC_NUM_MASK | dev_id;
		error =	CA_NO_ERROR;
		ci_debug_trace(CI_API_RUNTIME,
		"Link initialization required if card is inserted after	new\n");
		goto api_exit;
	}
	ci_check_state(ci_p, CI_STATE_CARD_PRESENT);
	error =	ci_internal_card_configure(ci_p);
	if (error == CA_NO_ERROR) {
		error = ci_internal_link_init(ci_p);
		if (error == CA_NO_ERROR)
			error =	ci_check_state(ci_p, CI_STATE_CARD_READY);
	}
	/* -EPERM will occur only if we	removed	the card from the slot
	* so do nothing :
	If we get any other error change state to CP	*/
	if ((-EPERM != error) && (error != CA_NO_ERROR)) {
		ci_check_state(ci_p, CI_STATE_CARD_PRESENT);

		ci_debug_trace(CI_API_RUNTIME,
		"Link initialization failed :Need to issue Link	reset\n");

		ci_p->magic_num	= MAGIC_NUM_MASK | dev_id;
		error =	CA_NO_ERROR;
		goto api_exit;
	}
	/* In case card	is removed :new	is considered success :error = -EPERM */
	if (error == -EPERM) {
		ci_debug_trace(CI_API_RUNTIME,
				"card has been removed = %d\n",
				ci_p->currstate);
		ci_p->magic_num	= MAGIC_NUM_MASK | dev_id;
		error =	CA_NO_ERROR;
		goto api_exit;
	}
	ci_p->magic_num = MAGIC_NUM_MASK | dev_id;
	ci_p->device.link_initialized =	true;
	ci_exit_critical_section();
	return error;
api_exi1:
	ci_dealloc_control_param(ci_p);
api_exit:
	ci_exit_critical_section();
	return error;
}
int stm_ci_delete(stm_ci_h ci)
{
	int	error =	CA_NO_ERROR;
	stm_ci_t *ci_p = (stm_ci_t *)ci;

	ci_enter_critical_section();
	error =	ci_validate_del_param(ci);
	if (error != CA_NO_ERROR)
		goto api_exit;

	error =	ci_check_state(ci_p, CI_STATE_CLOSE);
	if (error == CA_NO_ERROR) {
		ci_internal_delete(ci_p);
		error =	ci_dealloc_control_param(ci_p);
	}
api_exit:
	ci_exit_critical_section();
	return error;

}

int stm_ci_read(stm_ci_h ci,
		stm_ci_msg_params_t	*params_p)
{
	int error = CA_NO_ERROR;
	stm_ci_t *ci_p = (stm_ci_t *)ci;

	ci_enter_critical_section();
	if (!ci || !params_p) {
		ci_error_trace(CI_API_RUNTIME,
				"INVALID PARAM[handle=%p msg:%p]\n",
				ci,
				params_p);
		error =	-EINVAL;
		goto api_exit;
	}
	if ((CI_VALIDATE_HANDLE(ci_p->magic_num)) != 0) {
		ci_error_trace(CI_API_RUNTIME,
				"INVALID HANDLE[handle=%p ]\n",
				ci);
		error =	-ENODEV;
		goto api_exit;
	}
	error =	ci_check_state(ci_p, CI_STATE_PROCESS);
	if (error != CA_NO_ERROR) {
		ci_error_trace(CI_API_RUNTIME,
		"currstate state/status value not appropriate for read\n");
		goto api_exit;
	}
	error =	ci_internal_read(ci_p, params_p);
	if (error != CA_NO_ERROR) {
		ci_error_trace(CI_API_RUNTIME,
				"Read failed %d\n",
				error);
	}
	/* If Card has been removed, cuustate will be CNP so card state will not
	* change to Ready	:error not checked intentionally*/
	ci_check_state(ci_p, CI_STATE_CARD_READY);

api_exit:
	ci_exit_critical_section();
	return error;

}

int stm_ci_read_cis(stm_ci_h ci,
		unsigned int	buf_size,
		unsigned int	*actual_cis_size,
		unsigned char	*cis_buf_p)

{
	int error = 0;
	stm_ci_t *ci_p = (stm_ci_t *)ci;

	ci_enter_critical_section();

	if (!ci || !actual_cis_size || !cis_buf_p) {
		ci_error_trace(CI_API_RUNTIME,
		"INVALID PARAM[handle=%p actual_cis_size:%p cis_buf_p =%p\n",
				ci,
				actual_cis_size,
				cis_buf_p);
		error =	-EINVAL;
		goto api_exit;
	}
	if ((CI_VALIDATE_HANDLE(ci_p->magic_num)) != 0) {
		ci_error_trace(CI_API_RUNTIME,
				"INVALID HANDLE[handle=%p ]\n",
				ci_p);
		error =	-ENODEV;
		goto api_exit;
	}
	if (buf_size < ci_p->device.ci_caps.cis_size) {
		ci_error_trace(CI_API_RUNTIME,
				"SMALL BUFFER SIZE < %d\n",
				ci_p->device.ci_caps.cis_size);
		error =	-EIO;
		goto api_exit;
	}
	error =	ci_check_state(ci_p, CI_STATE_PROCESS);
	if (error != CA_NO_ERROR) {
		ci_error_trace(CI_INTERNAL_RUNTIME,
			"currstate state not appropriate for read cis\n");
		goto api_exit;
	}
	error =	ci_internal_read_cis(ci_p, actual_cis_size, cis_buf_p);
	/* If Card has been removed, cuustate will be CNP so card state will
	* not	change to Ready	:error not checked intentionally*/
	ci_check_state(ci_p, CI_STATE_CARD_READY);
api_exit:
	ci_exit_critical_section();
	return error;

}

int stm_ci_write(stm_ci_h ci,
		stm_ci_msg_params_t	*params_p)
{
	int error = 0;
	stm_ci_t *ci_p = (stm_ci_t *)ci;

	ci_enter_critical_section();

	if (!ci || !params_p) {
		ci_error_trace(CI_API_RUNTIME,
				"INVALID PARAM[handle=%p msg:%p]\n",
				ci,
				params_p);
		error =	-EINVAL;
		goto api_exit;
	}
	if ((CI_VALIDATE_HANDLE(ci_p->magic_num)) != 0) {
		ci_error_trace(CI_API_RUNTIME,
				"INVALID HANDLE[handle=%p]\n",
				ci_p);
		error =	-ENODEV;
		goto api_exit;
	}
	error =	ci_check_state(ci_p, CI_STATE_PROCESS);
	if (error != CA_NO_ERROR) {
		ci_error_trace(CI_API_RUNTIME,
				"currstate state not appropriate for write\n");
		goto api_exit;
	}
	error =	ci_internal_write(ci_p, params_p);
	if (error) {
		ci_error_trace(CI_API_RUNTIME,
				"write failed[%d]\n",
				error);
	}
	/* If Card has been removed cuustate will be CNP so card state will
	* not	change to Ready	:error not checked intentionally*/
	ci_check_state(ci_p, CI_STATE_CARD_READY);

api_exit:
	ci_exit_critical_section();
	return error;
}

int stm_ci_get_capabilities(stm_ci_h ci,
		stm_ci_capabilities_t *capabilities_p)

{
	int error = 0;
	stm_ci_t *ci_p = (stm_ci_t *)ci;
	ci_enter_critical_section();

	if (!ci || !capabilities_p) {
		ci_error_trace(CI_API_RUNTIME,
				"INVALID PARAM[handle=%p ctrl_data_p:%p]\n",
				ci,
				capabilities_p);
		error =	-EINVAL;
		goto api_exit;
	}
	if ((CI_VALIDATE_HANDLE(ci_p->magic_num)) != 0) {
		ci_error_trace(CI_API_RUNTIME,
				"INVALID HANDLE[handle=%p]\n",
				ci);
		error =	-ENODEV;
		goto api_exit;
	}
	/* no state check needed as cap	=null already */
	memcpy((void *)capabilities_p, (void *)&ci_p->device.ci_caps,
			sizeof(stm_ci_capabilities_t));
api_exit:
	ci_exit_critical_section();
	return error;
}

int stm_ci_get_slot_status(stm_ci_h ci, bool *is_present)
{
	int error = 0;
	stm_ci_t *ci_p = (stm_ci_t *)ci;

	ci_enter_critical_section();

	if (!ci || !is_present) {
		ci_error_trace(CI_API_RUNTIME,
				"INVALID PARAM[handle=%p is_present:%p]\n",
				ci,
				is_present);
		error =	-EINVAL;
		goto api_exit;
	}
	*is_present = false;
	if ((CI_VALIDATE_HANDLE(ci_p->magic_num)) != 0) {
		ci_error_trace(CI_API_RUNTIME,
				"INVALID HANDLE\n");
		error =	-ENODEV;
		goto api_exit;
	}
	error =	ci_internal_get_slot_status(ci_p, is_present);

api_exit:
	ci_exit_critical_section();
	return error;
}

int stm_ci_get_status(stm_ci_h		ci,
		stm_ci_status_t		status,
		uint8_t			*value_p)
{
	int error = 0;
	stm_ci_t *ci_p = (stm_ci_t *)ci;

	ci_enter_critical_section();

	if (!ci || !value_p) {
		ci_error_trace(CI_API_RUNTIME,
				"INVALID PARAM[handle=%p value_p:%p]\n",
				ci,
				value_p);
		error = -EINVAL;
		goto api_exit;
	}
	if ((CI_VALIDATE_HANDLE(ci_p->magic_num)) != 0) {
		ci_error_trace(CI_API_RUNTIME,
				"INVALID HANDLE[handle=%p ]\n",
				ci);
		error = -ENODEV;
		goto api_exit;
	}
	if (ci_p->currstate == CI_STATE_CARD_NOT_PRESENT) {
		ci_error_trace(CI_API_RUNTIME,
				"No card available");
		error =	-EPERM;
		goto api_exit;
	}
	error =	ci_internal_get_status(ci_p, status, value_p);
	if (error) {
		ci_error_trace(CI_API_RUNTIME,
				"ci_internal_get_status	failed[%d]\n",
				error);
	}
api_exit:
	ci_exit_critical_section();
	return error;
}

int stm_ci_set_command(stm_ci_h		ci,
		stm_ci_command_t	command,
		uint8_t		value)
{
	int error = 0;
	stm_ci_t *ci_p = (stm_ci_t *)ci;

	ci_enter_critical_section();

	if (!ci) {
		ci_error_trace(CI_API_RUNTIME,
				"INVALID PARAM[handle=%p\n",
				ci);
		error = -EINVAL;
		goto api_exit;
	}
	if ((CI_VALIDATE_HANDLE(ci_p->magic_num)) != 0) {
		ci_error_trace(CI_API_RUNTIME,
				"INVALID HANDLE[handle=%p ]\n",
				ci);
		error = -ENODEV;
		goto api_exit;
	}

	if (ci_p->device.ci_caps.cam_type != STM_CI_CAM_DVBCI_PLUS) {
		ci_error_trace(CI_API_RUNTIME,
				"INVALID CAM Type[=%d ]\n",
				ci_p->device.ci_caps.cam_type);
		error =	-EPERM;
		goto api_exit;
	}

	error =	ci_check_state(ci_p, CI_STATE_PROCESS);
	if (error != CA_NO_ERROR) {
		ci_error_trace(CI_INTERNAL_RUNTIME,
			"currstate state not appropriate for setting cmd\n");
		goto api_exit;
	}
	error =	ci_internal_set_command(ci_p, command, value);
	if (error) {
		ci_error_trace(CI_API_RUNTIME,
			"ci_internal_set_command failed[%d]\n",
			error);
	}

	/* If Card has been removed, currstate will be CNP so card state
	* * will not change	to Ready :error	not checked intentionally*/
	ci_check_state(ci_p, CI_STATE_CARD_READY);

api_exit:
	ci_exit_critical_section();
	return error;
}

int stm_ci_reset(stm_ci_h ci, stm_ci_reset_type_t type)
{
	int error = 0;
	stm_ci_t *ci_p = (stm_ci_t *)ci;

	ci_enter_critical_section();

	if (!ci) {
		ci_error_trace(CI_API_RUNTIME,
				"INVALID PARAM[handle=%p\n",
				ci);
		error =	-EINVAL;
		goto api_exit;
	}
	if ((CI_VALIDATE_HANDLE(ci_p->magic_num)) != 0) {
		ci_error_trace(CI_API_RUNTIME,
				"INVALID HANDLE[handle=%p ]\n",
				ci);
		error = -ENODEV;
		goto api_exit;
	}

	switch (type) {
	case STM_CI_RESET_SOFT:
		error =	ci_internal_soft_reset(ci_p);
		break;

	case STM_CI_RESET_LINK:
		/* check card present or not */
		if (ci_p->currstate != CI_STATE_CARD_PRESENT) {
			error =	-EPERM;
			ci_error_trace(CI_API_RUNTIME,
				"Link Reset not	required:error %d\n",
				error);
			goto api_exit;
		}
		/* CAM state will be CP	*/
		/* Invalidate old capabilities and device parameters */
		ci_p->device.ci_caps.cis_size = 0;
		ci_p->device.link_initialized = false;
		error =	ci_internal_card_configure(ci_p);
		if (error == CA_NO_ERROR) {
			error = ci_internal_link_init(ci_p);
			if (error == CA_NO_ERROR)
				error =	ci_check_state(ci_p,
					CI_STATE_CARD_READY);
		}
		/* -EPERM will occur only if we	removed	the card
		*from the slot so do nothing :
		If we get any other error change state to CP */
		if ((-EPERM != error) && (error != CA_NO_ERROR))
			ci_check_state(ci_p, CI_STATE_CARD_PRESENT);
		else
			ci_p->device.link_initialized = true;
		break;

	default:
		ci_error_trace(CI_API_RUNTIME,
				"wrong reset type %d\n",
				type);
			break;
	}
api_exit:
	ci_exit_critical_section();
	return error;
}

