
#include "asc_internal.h"

ca_error_code_t asc_set_autoparity_rejection(asc_ctrl_t *asc_ctrl_p,
		uint8_t auto_parity_rejection)
{
	volatile asc_regs_t *asc_regs_p = (asc_regs_t *)asc_ctrl_p->device.base;
	struct asc_device *device = &asc_ctrl_p->device;

	if (auto_parity_rejection == 1)
		ASC_PARITYREJECT_ENABLE(asc_regs_p);
	else if (auto_parity_rejection == 0)
		ASC_PARITYREJECT_DISABLE(asc_regs_p);
	else {
		asc_error_print("%s@%dError:Invalid value\n",
			__func__, __LINE__);
		return -EINVAL;
	}
	device->params.autoparityrejection = auto_parity_rejection;
	return CA_NO_ERROR;
}

ca_error_code_t asc_set_flow_control(asc_ctrl_t *asc_ctrl_p,
		uint8_t  flow_control)
{
	volatile asc_regs_t *asc_regs_p = (asc_regs_t *)asc_ctrl_p->device.base;
	struct asc_device *device = &asc_ctrl_p->device;

	if (flow_control == 1)
		ASC_SCFLOWCTRL_ENABLE(asc_regs_p);
	else if (flow_control == 0)
		ASC_SCFLOWCTRL_DISABLE(asc_regs_p);
	else {
		asc_error_print("%s@%dError :Invalid value\n",
			__func__, __LINE__);
		return -EINVAL;
	}
	/* update the paramters in use from now*/
	device->params.flowcontrol = flow_control;
	return CA_NO_ERROR;
}

ca_error_code_t asc_set_nack(asc_ctrl_t *asc_ctrl_p, uint8_t  nack)
{
	volatile asc_regs_t *asc_regs_p = (asc_regs_t *)asc_ctrl_p->device.base;
	struct asc_device  *device = &asc_ctrl_p->device;

	if (nack == 1)
		ASC_NACK_ENABLE(asc_regs_p);
	else if (nack == 0)
		ASC_NACK_DISABLE(asc_regs_p);
	else {
		asc_error_print("%s@%dError :Invalid value\n",
			__func__, __LINE__);
		return -EINVAL;
	}

	device->params.nack = nack;
	return CA_NO_ERROR;
}

ca_error_code_t asc_set_databits(volatile asc_regs_t *asc_regs_p,
		stm_asc_scr_databits_t  databits)
{
	switch (databits) {
	case STM_ASC_SCR_8BITS_NO_PARITY:
		ASC_8BITS(asc_regs_p);
		break;

	case STM_ASC_SCR_8BITS_ODD_PARITY:
		ASC_8BITS_PARITY(asc_regs_p);
		ASC_PARITY_ODD(asc_regs_p);
		break;

	case STM_ASC_SCR_8BITS_EVEN_PARITY:
		ASC_8BITS_PARITY(asc_regs_p);
		ASC_PARITY_EVEN(asc_regs_p);
		break;

	case STM_ASC_SCR_9BITS_NO_PARITY:
		ASC_9BITS(asc_regs_p);
		break;

	case STM_ASC_SCR_9BITS_UNKNOWN_PARITY:
		ASC_9BITS(asc_regs_p);
		ASC_SMARTCARD_DISABLE(asc_regs_p);
		break;

	default:
		asc_error_print("%s@%dError :Invalid value\n",
			__func__, __LINE__);
		return -EINVAL;
	}
	return CA_NO_ERROR;
}

ca_error_code_t asc_set_stopbits(volatile asc_regs_t *asc_regs_p,
		stm_asc_scr_stopbits_t stopbits)
{
	switch (stopbits) {
	case STM_ASC_SCR_STOP_0_5:
		ASC_0_5_STOPBITS(asc_regs_p);
		break;

	case STM_ASC_SCR_STOP_1_0:
		ASC_1_0_STOPBITS(asc_regs_p);
		break;

	case STM_ASC_SCR_STOP_1_5:
		ASC_1_5_STOPBITS(asc_regs_p);
		break;

	case STM_ASC_SCR_STOP_2_0:
		ASC_2_0_STOPBITS(asc_regs_p);
		break;

	default:
		asc_error_print("%s@%dError :Invalid value\n",
			__func__, __LINE__);
		return -EINVAL;
	}
	return CA_NO_ERROR;
}

ca_error_code_t asc_set_baudrate(asc_ctrl_t *asc_ctrl_p, uint32_t  baud)
{
	uint8_t clockmode = 0;
	uint16_t  reload_value[2];
	int32_t  baud_error[2];
	volatile asc_regs_t *asc_regs_p = (asc_regs_t *)asc_ctrl_p->device.base;

	/* Compute reload value for 'decrement-trigger-zero' baud mode */
	reload_value[0] = (uint16_t)(((asc_ctrl_p->device.uartclk) +
				((16 * baud) / 2)) / (16 * baud));
	baud_error[0] = (int32_t)(baud - (asc_ctrl_p->device.uartclk /
				(16 * reload_value[0])));

	/* Compute reload value for 'multiply-carry-trigger' baud mode.
	   The BR calculation has been revised for higher BR support- DDTS 25068 */

	reload_value[1] = (uint16_t)(((baud * (2 << 9)) +
				(asc_ctrl_p->device.uartclk / (2 << 10))) /
				(asc_ctrl_p->device.uartclk / (2 << 9)));
	baud_error[1] = (int32_t)(baud - (reload_value[1] *
				(asc_ctrl_p->device.uartclk / (2 << 9)) /
				(2 << 9)));

	/* Select clock mode */
	if (abs(baud_error[0]) <= abs(baud_error[1]))
		clockmode = 0;          /* decrement-trigger-zero */
	else
		clockmode = 1;          /* multiply-carry-trigger */

	/* Set the required clock mode */
	switch (clockmode) {
	case 0:
		ASC_BAUD_TICKONZERO(asc_regs_p);
	break;
	case 1:
		ASC_BAUD_TICKONCARRY(asc_regs_p);
	break;
	}
	/* Invoke clock reload value */
	ASC_SET_BAUDRATE(asc_regs_p, reload_value[clockmode]);
	return CA_NO_ERROR;
}

ca_error_code_t asc_set_protocol(asc_ctrl_t *asc_ctrl_p,
		stm_asc_scr_protocol_t protocol)
{
	ca_error_code_t error = CA_NO_ERROR;
	volatile asc_regs_t *asc_regs_p = (asc_regs_t *)asc_ctrl_p->device.base;
	asc_ctrl_p->lpm_data.baudrate = protocol.baud;
	error = asc_set_databits(asc_regs_p, protocol.databits);
	error |= asc_set_stopbits(asc_regs_p, protocol.stopbits);
	error |= asc_set_baudrate(asc_ctrl_p, protocol.baud);
	return error;
}
