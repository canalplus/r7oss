#include "stm_scr.h"
#include "scr_data_parsing.h"
#include "scr_internal.h"
#include "scr_utils.h"
#include "scr_io.h"

int stm_scr_new(uint32_t  scr_dev_id,
		stm_scr_device_type_t device_type,
		stm_scr_h *scr_p)
{
	int error = 0;

	if (scr_dev_id >= STM_SCR_MAX) {
		scr_error_print("<%s> :: Invalid Device Id\n", __func__);
		return -ENODEV;
	}
	if (!scr_p) {
		scr_error_print("<%s> :: scr_p cannot be null\n", __func__);
		return -EINVAL;
	}
	error = scr_internal_new(scr_dev_id,device_type,scr_p);
	if (error) {
		scr_error_print("<%s> :: scr_internal_new failed[%d]\n",
			__func__, error);
	}
	return error;
}

int stm_scr_delete(stm_scr_h  scr)
{
	scr_ctrl_t *scr_ctrl_p = (scr_ctrl_t *)scr;
	if (!scr) {
		scr_error_print("<%s> :: scr cannot be null\n", __func__);
		return -EINVAL;
	}
	if ((VALIDATE_HANDLE(scr_ctrl_p->magic_num)) != 0) {
		scr_error_print("<%s> :: <%d>\n", __func__, __LINE__);
		return -EINVAL;
	}
	return (scr_internal_delete(scr_ctrl_p));
}

int stm_scr_transfer(stm_scr_h  scr,
		stm_scr_transfer_params_t  *transfer_params_p)
{
	scr_ctrl_t *scr_ctrl_p = (scr_ctrl_t *)scr;
	if (!scr || !transfer_params_p) {
		scr_error_print("<%s> :: <%d>\n", __func__, __LINE__);
		return -EINVAL;
	}
	if ((VALIDATE_HANDLE(scr_ctrl_p->magic_num)) != 0) {
		scr_error_print("<%s> :: <%d>\n", __func__, __LINE__);
		return -EINVAL;
	}

	if (!scr_ctrl_p->reset_done) {
		scr_error_print("<%s> :: <%d>\n", __func__, __LINE__);
		scr_ctrl_p->last_error = SCR_ERROR_NOT_RESET;
		return -EPERM;
	}
	scr_send_message_and_wait(scr_ctrl_p,
			SCR_THREAD_STATE_PROCESS,
			SCR_PROCESS_CMD_TRANSFER,
			(void *)transfer_params_p,
			false);

	return scr_ctrl_p->last_status;
}

int stm_scr_reset(stm_scr_h  scr,
			stm_scr_reset_type_t  type,
			uint8_t  *atr_p,
			uint32_t  *atr_length_p)
{
	scr_ctrl_t *scr_ctrl_p = (scr_ctrl_t *)scr;
	if (!scr || !atr_p || !atr_length_p) {
		scr_error_print("<%s> :: <%d>\n", __func__, __LINE__);
		return -EINVAL;
	}
	if ((VALIDATE_HANDLE(scr_ctrl_p->magic_num)) != 0) {
		scr_error_print("<%s> :: <%d>\n", __func__, __LINE__);
		return -EINVAL;
	}

	scr_send_message_and_wait(scr_ctrl_p,
			SCR_THREAD_STATE_PROCESS,
			SCR_PROCESS_CMD_RESET,
			(void *)&type,
			false);
	if (!scr_ctrl_p->last_status) {
		*atr_length_p = scr_ctrl_p->atr_response.atr_size;
		memcpy(atr_p,
			&scr_ctrl_p->atr_response.raw_data,
			scr_ctrl_p->atr_response.atr_size);
	}
	return scr_ctrl_p->last_status;
}


int stm_scr_deactivate(stm_scr_h scr)
{
	scr_ctrl_t *scr_ctrl_p = (scr_ctrl_t *)scr;
	if (!scr) {
		scr_error_print("<%s> :: <%d>\n", __func__, __LINE__);
		return -EINVAL;
	}
	if ((VALIDATE_HANDLE(scr_ctrl_p->magic_num)) != 0) {
		scr_error_print("<%s> :: <%d>\n", __func__, __LINE__);
		return -EINVAL;
	}

	scr_asc_abort(scr_ctrl_p);

	scr_send_message_and_wait(scr_ctrl_p,
			SCR_THREAD_STATE_DEACTIVATE,
			SCR_PROCESS_CMD_INVALID,
			NULL,
			false);

	if (scr_ctrl_p->last_status == EINVAL)
		return scr_ctrl_p->last_status;
	else
		return 0;
}

int  stm_scr_abort(stm_scr_h scr)
{
	scr_ctrl_t *scr_ctrl_p = (scr_ctrl_t *)scr;
	if (!scr) {
		scr_error_print("<%s> :: <%d>\n", __func__, __LINE__);
		return -EINVAL;
	}
	if ((VALIDATE_HANDLE(scr_ctrl_p->magic_num)) != 0) {
		scr_error_print("<%s> :: <%d>\n", __func__, __LINE__);
		return -EINVAL;
	}
	scr_ctrl_p->last_status = scr_asc_abort(scr_ctrl_p);

	scr_send_message_and_wait(scr_ctrl_p,
			SCR_THREAD_STATE_PROCESS,
			SCR_PROCESS_CMD_ABORT,
			NULL,
			false);
	return scr_ctrl_p->last_status;
}

int stm_scr_protocol_negotiation(stm_scr_h scr,
			uint8_t *pps_data_p,
			uint8_t *pps_response_p)
{
	scr_ctrl_t *scr_ctrl_p = (scr_ctrl_t *)scr;
	if (!scr || !pps_data_p || !pps_response_p) {
		scr_error_print("<%s> :: <%d>\n", __func__, __LINE__);
		return -EINVAL;
	}
	if ((VALIDATE_HANDLE(scr_ctrl_p->magic_num)) != 0) {
		scr_error_print("<%s> :: <%d>\n", __func__, __LINE__);
		return -EINVAL;
	}
	if (!scr_ctrl_p->reset_done) {
		scr_error_print("<%s> :: <%d>\n", __func__, __LINE__);
		scr_ctrl_p->last_error = SCR_ERROR_NOT_RESET;
		return -EPERM;
	}

	scr_send_message_and_wait(scr_ctrl_p,
			SCR_THREAD_STATE_PROCESS,
			SCR_PROCESS_CMD_PPS,
			(void *)pps_data_p,
			false);
	/* Do not copy the PPSS and PCK bytes in the response*/
	memcpy(pps_response_p,
		&scr_ctrl_p->pps_data.pps_response[SCR_PPS0_INDEX],
		scr_ctrl_p->pps_data.pps_size - 2);
	return scr_ctrl_p->last_status;
}

int stm_scr_get_capabilities(stm_scr_h scr,
			stm_scr_capabilities_t *capabilities_p)

{
	scr_ctrl_t *scr_ctrl_p = (scr_ctrl_t *)scr;
	if (!scr || !capabilities_p) {
		scr_error_print("<%s> :: <%d>\n", __func__, __LINE__);
		return -EINVAL;
	}
	if ((VALIDATE_HANDLE(scr_ctrl_p->magic_num)) != 0) {
		scr_error_print("<%s> :: <%d>\n", __func__, __LINE__);
		return -EINVAL;
	}
	if (!scr_ctrl_p->reset_done) {
		scr_error_print("<%s> :: <%d>\n", __func__, __LINE__);
		return -EPERM;
	}
	memcpy((void *)capabilities_p,
		(void *)&scr_ctrl_p->scr_caps,
		sizeof(stm_scr_capabilities_t));
	return 0;
}

int stm_scr_get_card_status(stm_scr_h scr, bool *is_present)
{
	scr_ctrl_t *scr_ctrl_p = (scr_ctrl_t *)scr;
	if (!scr || !is_present) {
		scr_error_print("<%s> :: <%d>\n", __func__, __LINE__);
		return -EINVAL;
	}
	if ((VALIDATE_HANDLE(scr_ctrl_p->magic_num)) != 0) {
		scr_error_print("<%s> :: <%d>\n", __func__, __LINE__);
		return -EINVAL;
	}
	*is_present = scr_ctrl_p->card_present;
	return 0;
}

int stm_scr_get_last_error(stm_scr_h scr, uint32_t *last_error)
{
	scr_ctrl_t *scr_ctrl_p = (scr_ctrl_t *)scr;
	if (!scr || !last_error) {
		scr_error_print("<%s> :: <%d>\n", __func__, __LINE__);
		return -EINVAL;
	}
	if ((VALIDATE_HANDLE(scr_ctrl_p->magic_num)) != 0) {
		scr_error_print("<%s> :: <%d>\n", __func__, __LINE__);
		return -EINVAL;
	}
	*last_error = scr_ctrl_p->last_error;
	scr_ctrl_p->last_error = 0;
	return 0;
}

int stm_scr_get_control(stm_scr_h scr,
			stm_scr_ctrl_flags_t  ctrl_flag,
			uint32_t  *value_p)
{
	scr_ctrl_t *scr_ctrl_p = (scr_ctrl_t *)scr;
	if (!scr ||  !value_p) {
		scr_error_print("<%s> :: <%d>\n", __func__, __LINE__);
		return -EINVAL;
	}
	if ((VALIDATE_HANDLE(scr_ctrl_p->magic_num)) != 0) {
		scr_error_print("<%s> :: <%d>\n", __func__, __LINE__);
		return -EINVAL;
	}
	return scr_internal_get_ctrl(scr_ctrl_p, ctrl_flag, value_p);
}

int stm_scr_set_control(stm_scr_h scr,
			stm_scr_ctrl_flags_t ctrl_flag,
			uint32_t  value)
{
	scr_ctrl_t *scr_ctrl_p = (scr_ctrl_t *)scr;
	if (!scr) {
		scr_error_print("<%s> :: <%d>\n", __func__, __LINE__);
		return -EINVAL;
	}
	if ((VALIDATE_HANDLE(scr_ctrl_p->magic_num)) != 0) {
		scr_error_print("<%s> :: <%d>\n", __func__, __LINE__);
		return -EINVAL;
	}
	return scr_internal_set_ctrl(scr_ctrl_p, ctrl_flag, value);
}
