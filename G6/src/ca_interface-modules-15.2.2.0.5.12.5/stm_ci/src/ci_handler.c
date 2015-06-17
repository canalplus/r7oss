#include <linux/irq.h>
#include <linux/kernel_stat.h>
#include "stm_ci.h"
#include "stm_event.h"
#include "ci_hal.h"

irqreturn_t ci_detect_handler(int irq, void *dev)
{
	ca_error_code_t error = CA_NO_ERROR;
	uint16_t	card_status = 0;
	stm_event_t	event;

	stm_ci_t *ci_p = (stm_ci_t *)dev;
	ci_signal_map cd1 = ci_p->sig_map[CI_SIGNAL_CD1];

	ci_debug_trace(CI_HAL_RUNTIME, "Inside card detect handler\n");

	if (ci_p->ca.slot_detect_irq_disable != NULL)
		error = (*ci_p->ca.slot_detect_irq_disable)(&ci_p->ca);

	if (error != CA_NO_ERROR)
		goto irq_error;

	/*This	cannot be blocking*/
	if (ci_p->ca.slot_status != NULL)
		error =	(*ci_p->ca.slot_status)(&ci_p->ca, &card_status);

	if (error != CA_NO_ERROR)
		goto irq_error;

	/* to protect critical data */
	spin_lock(&ci_p->lock);
	/* card	status is not pio value	it is a	pin value */
	if (card_status == cd1.active_high) {
		ci_p->device.cam_present = true;
	} else{
		ci_p->device.cam_present = false;
		ci_p->device.daie_set =	false;
		ci_p->device.frie_set =	false;
		ci_p->device.link_initialized =	false;
		ci_p->device.cam_voltage = CI_VOLT_5000;
		memset(&ci_p->device.ci_caps, 0x00,
				sizeof(stm_ci_capabilities_t));
	}
	if (ci_p->device.cam_present == true) {
		ci_debug_trace(CI_HAL_RUNTIME,
				"card inserted 0x%x\n",
				card_status);
		ci_p->currstate	= CI_STATE_CARD_PRESENT;
		event.event_id = STM_CI_EVENT_CAM_INSERTED;
	} else{
		ci_debug_trace(CI_HAL_RUNTIME,
			"card removed 0x%x\n",
			card_status);
		ci_p->currstate	= CI_STATE_CARD_NOT_PRESENT;
		event.event_id = STM_CI_EVENT_CAM_REMOVED;
	}

	spin_unlock(&ci_p->lock);
	event.object = (void *)ci_p;
	error =	stm_event_signal(&event);
	if (error != CA_NO_ERROR)
		goto irq_error;

	if (ci_p->ca.slot_detect_irq_enable != NULL)
		error = (*ci_p->ca.slot_detect_irq_enable)(&ci_p->ca);

	if (error != CA_NO_ERROR)
		goto irq_error;

	return IRQ_HANDLED;
irq_error:
	return IRQ_HANDLED;/* TODO*/
}

static inline void cam_process_da_irq(stm_ci_t *ci_p)
{
	uint8_t		status;
	uint32_t	io_base_addr = (uint32_t) ci_p->hw_addr.io_addr_v;
	volatile uint8_t *io_reg_addr = (volatile uint8_t *)io_base_addr;

	status = io_reg_addr[CI_CMDSTATUS];
	if (status & CI_DA) {
		ci_p->device.daie_set = false;
		ci_debug_trace(CI_HAL_RUNTIME, "DA Interrupt receievd\n");
		if (ci_p->currstate == CI_STATE_PROCESS)
			ca_os_sema_signal(ci_p->ctrl_sem_p);
	}
	return;
}

static inline void cam_process_fr_irq(stm_ci_t *ci_p)
{
	uint8_t		status;
	uint32_t	io_base_addr = (uint32_t) ci_p->hw_addr.io_addr_v;
	volatile uint8_t *io_reg_addr = (volatile uint8_t *)io_base_addr;

	status = io_reg_addr[CI_CMDSTATUS];
	if ((status & CI_FR) && (status & CI_IIR)) {
		uint32_t count = 100;
		ci_debug_trace(CI_HAL_RUNTIME, "Interrupt receievd IIR\
			and FR set:STM_CI_RESET_SOFT has to be issued by\
			User \n");
		ci_p->device.frie_set =	false;
		ci_p->device.daie_set =	false;

		/* Make	the Reset bit high */
		*(io_reg_addr + CI_CMDSTATUS) = CI_SETRS;

		/* Give	busy loop delay	*/
		while (count > 0)
			count--;
		/* Make	the Reset bit low */
		*(io_reg_addr + CI_CMDSTATUS) = CI_RESET_COMMAND_REG;
		ci_read_status_register(ci_p, &status);
		if ((status & CI_FR) == CI_FR) {
			ci_debug_trace(CI_HAL_RUNTIME,
			"Inside data ISR :IIR	bit reset FR set\n");
		}
		/* MG check if state has to be changed to READY	here */
	} else if (status & CI_FR) {
		ci_p->device.frie_set = false;
		ci_debug_trace(CI_HAL_RUNTIME, "FR Interrupt receievd\n");
		if (ci_p->currstate == CI_STATE_PROCESS)
			ca_os_sema_signal(ci_p->ctrl_sem_p);
	}
}

static void validate_data_irq(stm_ci_t *ci_p)
{
	/* if not CI PLUS return */
	if ((ci_p->device.ci_caps.cam_type != STM_CI_CAM_DVBCI_PLUS) ||
			(ci_p->device.ci_caps.interrupt_supported != true) ||
			(ci_p->device.access_mode != CI_IO_ACCESS) ||
			(ci_p->device.cam_present != true)) {
		return;
	}

	if (ci_p->device.daie_set == true)
		cam_process_da_irq(ci_p);
	if (ci_p->device.frie_set == true)
		cam_process_fr_irq(ci_p);
	return;
}

irqreturn_t ci_data_handler(int irq, void *dev)
{
	ca_error_code_t error = CA_NO_ERROR;
	stm_ci_t *ci_p = (stm_ci_t *)dev;

	ci_debug_trace(CI_HAL_RUNTIME, "Inside data handler\n");

	if (ci_p->ca.slot_data_irq_disable != NULL)
		error = (*ci_p->ca.slot_data_irq_disable)(&ci_p->ca);

	if (error != CA_NO_ERROR)
		return IRQ_HANDLED;/*TODO*/
	spin_lock(&ci_p->lock);
	validate_data_irq(ci_p);
	spin_unlock(&ci_p->lock);
	/* data interrupt will be enabled when DAIE or FRIE is set */
	/*ci_slot_data_irq_enable_pio(&ci_p->ca);*/
	/*	if(ci_p->ca.slot_data_irq_enable != NULL){
	*	error = (*ci_p->ca.slot_data_irq_enable)(&ci_p->ca);
	*	}*/
	return IRQ_HANDLED;
}

