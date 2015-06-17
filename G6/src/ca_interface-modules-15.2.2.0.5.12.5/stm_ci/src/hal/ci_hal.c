#include "ci_hal.h"

ca_error_code_t ci_hal_slot_init(stm_ci_t *ci_p)
{
	ca_error_code_t error = CA_NO_ERROR;

	/* Initialize all host interfaces present*/
	if (ci_p->ca.slot_init != NULL) {
		error =	(*ci_p->ca.slot_init)(&ci_p->ca);
		if (error)
			return error;
	}
	/* select or enable the	cam through SEL	signal */
	if (ci_p->ca.slot_enable != NULL) {
		error =	(*ci_p->ca.slot_enable)(&ci_p->ca);
		if (error)
			return error;
	}
	/* Install and Enable CD1 and IREQ interrupts */
	if (ci_p->ca.slot_detect_irq_install != NULL) {
		error =	(*ci_p->ca.slot_detect_irq_install)(&ci_p->ca);
		if (error)
			return error;
	}
	if (ci_p->ca.slot_detect_irq_enable != NULL) {
		error =	(*ci_p->ca.slot_detect_irq_enable)(&ci_p->ca);
		if (error)
			return error;
	}
	if (ci_p->ca.slot_data_irq_install != NULL) {
		error =	(*ci_p->ca.slot_data_irq_install)(&ci_p->ca);
		if (error)
			return error;
	}
	return error;
}

ca_error_code_t ci_hal_slot_shutdown(stm_ci_t *ci_p)
{
	ca_error_code_t	error = CA_NO_ERROR;

	/* Disable and unInstall CD1 and IREQ interrupts */
	if (ci_p->ca.slot_detect_irq_disable != NULL) {
		error =	(*ci_p->ca.slot_detect_irq_disable)(&ci_p->ca);
		if (error)
			return error;
	}
	if (ci_p->ca.slot_detect_irq_uninstall != NULL) {
		error =	(*ci_p->ca.slot_detect_irq_uninstall)(&ci_p->ca);
		if (error)
			return error;
	}
	if (ci_p->ca.slot_data_irq_disable != NULL) {
		error =	(*ci_p->ca.slot_data_irq_disable)(&ci_p->ca);
		if (error)
			return error;
	}
	if (ci_p->ca.slot_data_irq_uninstall != NULL) {
		error =	(*ci_p->ca.slot_data_irq_uninstall)(&ci_p->ca);
		if (error)
			return error;
	}
	if (ci_p->ca.slot_shutdown != NULL)
		error =	(*ci_p->ca.slot_shutdown)(&ci_p->ca);
	return error;
}

ca_error_code_t ci_hal_slot_status(stm_ci_t *ci_p)
{
	ca_error_code_t	error =	CA_NO_ERROR;
	uint16_t		card_status = 0;
	ci_signal_map cd1 = ci_p->sig_map[CI_SIGNAL_CD1];
	u_long flags;

	if (ci_p->ca.slot_status != NULL)
		error =	(*ci_p->ca.slot_status)(&ci_p->ca, &card_status);

	/* Assuming that CD1 and CD2 are always	mapped to same register	*/
	spin_lock_irqsave(&ci_p->lock, flags);
	if (card_status	== cd1.active_high) {
		ci_p->device.cam_present = true;
		ci_debug_trace(CI_HAL_RUNTIME,
				"Card present 0x%x\n",
				card_status);
		error =	CA_NO_ERROR;
	} else {
		ci_debug_trace(CI_HAL_RUNTIME,
				"Card not present 0x%x\n",
				card_status);
		/* No module */
		ci_p->device.cam_present = false;
		ci_p->device.daie_set = false;
		ci_p->device.frie_set = false;
		error =	-ENODEV;
	}
	spin_unlock_irqrestore(&ci_p->lock, flags);
	return error;
}

ca_error_code_t ci_hal_power_enable(stm_ci_t	*ci_p)
{
	ca_error_code_t	error =	CA_NO_ERROR;
	ci_debug_trace(CI_HAL_RUNTIME,
			"Card power enable:voltage %d\n",
			ci_p->device.cam_voltage);
	if (ci_p->ca.slot_power_enable != NULL)
		error =	(*ci_p->ca.slot_power_enable)(
				&ci_p->ca, ci_p->device.cam_voltage);
	if (error == CA_NO_ERROR)
		ca_os_sleep_milli_sec(1);/* Required	to settle vcc */

	return error;
}

ca_error_code_t ci_hal_power_disable(stm_ci_t *ci_p)
{
	ca_error_code_t	error =	CA_NO_ERROR;
	ci_debug_trace(CI_HAL_RUNTIME,
			"Card power disable\n");
	if (ci_p->ca.slot_power_disable != NULL)
		error =	(*ci_p->ca.slot_power_disable)(&ci_p->ca);
	if (error == CA_NO_ERROR)
		ca_os_sleep_milli_sec(1);/* Required to settle vcc */
	return error;
}

ca_error_code_t ci_hal_slot_reset(stm_ci_t *ci_p)
{
	ca_error_code_t	error =	CA_NO_ERROR;
	if (ci_p->ca.slot_reset != NULL)
		error =	(*ci_p->ca.slot_reset)(&ci_p->ca);
	return error;
}

