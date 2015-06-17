#include <linux/irq.h>
#include <linux/kernel_stat.h>
#include "stm_ci.h"
#include "stm_event.h"
#include "ci_hal.h"

static ca_error_code_t ci_power_on_micrel_epld(stm_ci_t *ci_p,
			ci_voltage_t voltage)
{
	ca_error_code_t error = CA_NO_ERROR;
	uint32_t ci_ctrl_add = 0;
	uint16_t card_pwr_value = 0;

	ci_signal_map vcc3en	= ci_p->sig_map[CI_SIGNAL_VCC_MICREL_Vcc3EN];
	ci_signal_map vcc5en	= ci_p->sig_map[CI_SIGNAL_VCC_MICREL_Vcc5EN];
	ci_signal_map en0	= ci_p->sig_map[CI_SIGNAL_VCC_MICREL_EN0];
	ci_signal_map en1	= ci_p->sig_map[CI_SIGNAL_VCC_MICREL_EN1];

	ci_ctrl_add	= (uint32_t)(ci_p->hw_addr.epld_addr_v + \
					vcc3en.data.epld_config.addr);
	card_pwr_value = readl((const volatile void __iomem *)ci_ctrl_add);


	switch (voltage) {
	case CI_VOLT_3300:
		card_pwr_value &= ~((uint16_t)vcc3en.bit_mask |\
					(uint16_t)en0.bit_mask);
		card_pwr_value |= ((uint16_t)vcc5en.bit_mask |\
					(uint16_t)en1.bit_mask);
		break;

	case CI_VOLT_5000:
		card_pwr_value &= ~((uint16_t)vcc5en.bit_mask |\
					(uint16_t)en1.bit_mask);
		card_pwr_value |= ((uint16_t)vcc3en.bit_mask |\
					(uint16_t)en0.bit_mask);
		break;

	default:
		error = -EINVAL;
		ci_error_trace(CI_HAL_RUNTIME,
				"Invalid Voltage\n");
		break;
	}
	ci_debug_trace(CI_HAL_RUNTIME,
		"Micrel Power on value = 0x%x Pin BitMask = 0x%x\
			at addr = 0x%x\n",
			card_pwr_value,
			en0.bit_mask,
			ci_ctrl_add);

	if (error == CA_NO_ERROR)
		writel((uint32_t) card_pwr_value, (volatile void __iomem *)ci_ctrl_add);

	return error;
}

static ca_error_code_t ci_power_off_micrel_epld(stm_ci_t *ci_p)
{
	ca_error_code_t error = CA_NO_ERROR;
	ci_signal_map vcc3en = ci_p->sig_map[CI_SIGNAL_VCC_MICREL_Vcc3EN];
	ci_signal_map vcc5en = ci_p->sig_map[CI_SIGNAL_VCC_MICREL_Vcc5EN];
	ci_signal_map en0 = ci_p->sig_map[CI_SIGNAL_VCC_MICREL_EN0];
	ci_signal_map en1 = ci_p->sig_map[CI_SIGNAL_VCC_MICREL_EN1];
	uint32_t	ci_ctrl_add = 0;
	uint16_t	card_pwr_value = 0;

	ci_ctrl_add	= (uint32_t)(ci_p->hw_addr.epld_addr_v + \
					vcc3en.data.epld_config.addr);
	card_pwr_value = readl((const volatile void __iomem *)ci_ctrl_add);
	card_pwr_value &= ~((uint16_t)(vcc3en.bit_mask |\
			(uint16_t)en0.bit_mask) | ((uint16_t)vcc5en.bit_mask |\
			(uint16_t)en1.bit_mask));

	writel((uint32_t) card_pwr_value, (volatile void __iomem *)ci_ctrl_add);
	ci_debug_trace(CI_HAL_RUNTIME,
		"Power off value = 0x%x Pin BitMask = 0x%x at addr = 0x%x\n",
			card_pwr_value ,
			vcc3en.bit_mask,
			ci_ctrl_add);
	return error;
}

/* Non-micrel mapped Vcc */
static ca_error_code_t ci_power_on_vcc_epld(stm_ci_t	*ci_p,
					ci_voltage_t voltage)
{
	ca_error_code_t error = CA_NO_ERROR;
	ci_signal_map vcc_en = ci_p->sig_map[CI_SIGNAL_VCC_EN];
	ci_signal_map vcc_3_5en = ci_p->sig_map[CI_SIGNAL_VCC_3_5EN];
	uint32_t	ci_ctrl_add = 0;
	uint16_t	card_pwr_value = 0;

	ci_ctrl_add	= (uint32_t)(ci_p->hw_addr.epld_addr_v +\
					vcc_en.data.epld_config.addr);
	card_pwr_value = readl((const volatile void __iomem *)ci_ctrl_add);

	switch (voltage) {
	case CI_VOLT_3300:
		card_pwr_value &= ~((uint16_t)vcc_3_5en.bit_mask);
		card_pwr_value |= ((uint16_t)vcc_en.bit_mask);
		break;

	case CI_VOLT_5000:
		card_pwr_value &= ~((uint16_t)vcc_en.bit_mask);
		card_pwr_value |= ((uint16_t)vcc_3_5en.bit_mask);
		break;

	default:
		error = -EINVAL;
		ci_error_trace(CI_HAL_RUNTIME,
				"Invalid Voltage\n");
		break;
	}
	ci_debug_trace(CI_HAL_RUNTIME,
		"Power on value =0x%x Pin BitMask = 0x%x at addr = 0x%x\n",
			card_pwr_value,
			vcc_en.bit_mask,
			ci_ctrl_add);

	if (error == CA_NO_ERROR)
		writel((uint32_t)card_pwr_value, (volatile void __iomem *)ci_ctrl_add);
	return error;
}

static ca_error_code_t ci_power_off_vcc_epld(stm_ci_t *ci_p)
{
	ca_error_code_t error = CA_NO_ERROR;
	ci_signal_map vcc_en = ci_p->sig_map[CI_SIGNAL_VCC_EN];
	ci_signal_map vcc_3_5en = ci_p->sig_map[CI_SIGNAL_VCC_3_5EN];
	uint32_t ci_ctrl_add = 0;
	uint16_t card_pwr_value = 0;

	ci_ctrl_add	= (uint32_t)(ci_p->hw_addr.epld_addr_v +\
					vcc_en.data.epld_config.addr);
	card_pwr_value = readl((const volatile void __iomem *)ci_ctrl_add);

	card_pwr_value &= ~(((uint16_t)vcc_en.bit_mask) |\
				((uint16_t)vcc_3_5en.bit_mask));
	writel((uint32_t)card_pwr_value, (volatile void __iomem *)ci_ctrl_add);

	ci_debug_trace(CI_HAL_RUNTIME,
		"Power off value = 0x%x Pin BitMask =0x%x at addr =0x%x\n",
			card_pwr_value ,
			vcc_en.bit_mask,
			ci_ctrl_add);
	return error;
}

/* direct mapped Vcc */
static ca_error_code_t ci_power_on_direct_epld(stm_ci_t *ci_p,
				ci_voltage_t	voltage)
{
	ca_error_code_t	error = CA_NO_ERROR;
	ci_signal_map vcc_en = ci_p->sig_map[CI_SIGNAL_VCC_EN];
	uint32_t	ci_ctrl_add = 0;
	uint16_t	card_pwr_value = 0;

	ci_ctrl_add = (uint32_t)(ci_p->hw_addr.epld_addr_v +\
					vcc_en.data.epld_config.addr);
	card_pwr_value = readl((const volatile void __iomem *)ci_ctrl_add);

	if (vcc_en.active_high == true)
		card_pwr_value |= vcc_en.bit_mask;
	else
		card_pwr_value &= ~(vcc_en.bit_mask);

	writel((uint32_t)card_pwr_value, (volatile void __iomem *)ci_ctrl_add);
	ci_debug_trace(CI_HAL_RUNTIME,
	"Direct mapped Power value on = 0x%x Pin BitMask = 0x%x ataddr= 0x%x\n",
			card_pwr_value,
			vcc_en.bit_mask,
			ci_ctrl_add);

	return error;
}

static ca_error_code_t ci_power_off_direct_epld(stm_ci_t *ci_p)
{
	ca_error_code_t error = CA_NO_ERROR;
	ci_signal_map vcc_en = ci_p->sig_map[CI_SIGNAL_VCC_EN];
	uint32_t	ci_ctrl_add = 0;
	uint16_t	card_pwr_value = 0;

	ci_ctrl_add	= (uint32_t)(ci_p->hw_addr.epld_addr_v + \
					vcc_en.data.epld_config.addr);
	card_pwr_value = readl((const volatile void __iomem *)ci_ctrl_add);
	if (vcc_en.active_high == true)
		card_pwr_value &= ~(vcc_en.bit_mask);
	else
		card_pwr_value |= vcc_en.bit_mask;
	writel((uint32_t)card_pwr_value, (volatile void __iomem *)ci_ctrl_add);

	ca_os_sleep_milli_sec(5);
	ci_debug_trace(CI_HAL_RUNTIME,
	"Direct mapped Power value off = 0x%x Pin BitMask = 0x%x\
			at addr = 0x%x\n",
			card_pwr_value ,
			vcc_en.bit_mask,
			ci_ctrl_add);
	return error;
}

static uint32_t	ci_initialise_interrupt(stm_ci_t *ci_p,
		uint32_t comp, uint32_t SigIndex)
{
	uint32_t error_code = 0, irq = 0;
	uint8_t	owner[32];

	if (SigIndex ==	CI_SIGNAL_CD1) {
		ci_signal_map cd1 = ci_p->sig_map[CI_SIGNAL_CD1];

		irq = cd1.interrupt_num;
		sprintf(owner, "%s-%d", "CI_CD1", CI_NO(ci_p->magic_num));
		CI_SET_IRQ_TYPE(irq, comp ?\
				IRQ_TYPE_LEVEL_LOW : IRQ_TYPE_LEVEL_HIGH);
		error_code = request_irq(irq, ci_detect_handler,
					IRQF_DISABLED ,
					(const char *)owner,
					(void *)ci_p);
		if (error_code) {
			ci_error_trace(CI_HAL_RUNTIME,
					"Interrupt request failed\n");
			return error_code;
		}

	} else if (SigIndex == CI_SIGNAL_IREQ) {
		ci_signal_map ireq = ci_p->sig_map[CI_SIGNAL_IREQ];
		irq = ireq.interrupt_num;

		sprintf(owner, "%s-%d", "CI_IRQ", CI_NO(ci_p->magic_num));
		CI_SET_IRQ_TYPE(irq, comp ?\
				IRQ_TYPE_LEVEL_LOW : IRQ_TYPE_LEVEL_HIGH);
		error_code = request_irq(irq, ci_data_handler,
					IRQF_DISABLED,
					(const char *)owner,
					(void *)ci_p);
		if (error_code) {
			ci_error_trace(CI_HAL_RUNTIME,
					"Interrupt request failed\n");
			return error_code;
		}
	}
	if (irq != 0)
		disable_irq_nosync(irq);

	return error_code;
}

/***************************************************************************/
/* exported Functions */
/***************************************************************************/
ca_error_code_t ci_slot_reset_epld(struct ci_dvb_ca_en50221 *ca)
{
	stm_ci_t *ci_p = (stm_ci_t *)ca->data;
	ci_signal_map rst = ci_p->sig_map[CI_SIGNAL_RST];
	uint32_t ci_rst_set_add = (uint32_t)(ci_p->hw_addr.epld_addr_v +
			rst.data.epld_config.epld_int_reg.reset_set_addr);
	uint32_t ci_rst_clear_add = (uint32_t)(ci_p->hw_addr.epld_addr_v +
			rst.data.epld_config.epld_int_reg.reset_clear_addr);
	uint16_t reset_value = 0;

	ci_debug_trace(CI_HAL_RUNTIME,
		"Card Reset value = 0x%x Pin BitMask = 0x%x at addr = 0x%x\n",
			reset_value,
			rst.bit_mask,
			ci_rst_clear_add);
	reset_value	= rst.data.epld_config.epld_int_reg.reset_set_bitmask;
	writel((uint32_t)reset_value, (volatile void __iomem *)ci_rst_set_add);
	ca_os_sleep_milli_sec(1);

	reset_value	= rst.data.epld_config.epld_int_reg.reset_clear_bitmask;
	writel((uint32_t)reset_value,(volatile void __iomem *) ci_rst_clear_add);
	ca_os_sleep_milli_sec(1);

	reset_value	= rst.data.epld_config.epld_int_reg.reset_set_bitmask;
	writel((uint32_t)reset_value,(volatile void __iomem *) ci_rst_set_add);
	ca_os_sleep_milli_sec(1);

	reset_value = rst.data.epld_config.epld_int_reg.reset_clear_bitmask;
	writel((uint32_t)reset_value, (volatile void __iomem *)ci_rst_clear_add);
	ca_os_sleep_milli_sec(1);

	return CA_NO_ERROR;
}

ca_error_code_t ci_slot_status_epld(struct ci_dvb_ca_en50221 *ca,
					uint16_t *pin_value_p)
{
	uint16_t card_status = 0;
	uint32_t cd1_bitmask = 0;
	stm_ci_t *ci_p = (stm_ci_t *)ca->data;
	uint32_t ci_ctrl_add = (uint32_t)(ci_p->hw_addr.epld_addr_v +
				ci_p->device.epld_reg.cd_status_addr);

	card_status = (uint16_t)readl((const volatile void __iomem *)ci_ctrl_add);
	cd1_bitmask = ci_p->device.epld_reg.cd_status_bitmask;

	/* Assuming that CD1 and CD2 are always	mapped to same register	*/
	if ((card_status & cd1_bitmask) == cd1_bitmask)
		*pin_value_p = 1;
	else
		*pin_value_p = 0;	/* no module */

	ci_debug_trace(CI_HAL_RUNTIME,
		"Card detect value = 0x%x Pin BitMask = 0x%x at addr = 0x%x\
			Pin value = %d\n",
			card_status ,
			cd1_bitmask,
			ci_ctrl_add,
			*pin_value_p);
	return CA_NO_ERROR;
}

ca_error_code_t ci_slot_power_enable_epld(struct ci_dvb_ca_en50221 *ca,
						ci_voltage_t voltage)
{
	ca_error_code_t error = CA_NO_ERROR;
	stm_ci_t *ci_p = (stm_ci_t *)ca->data;

	if (ci_p->vcc_type == CI_VCC)
		error =	ci_power_on_vcc_epld(ci_p, voltage);
	else if (ci_p->vcc_type == CI_VCC_MICREL)
		error =	ci_power_on_micrel_epld(ci_p, voltage);
	else if (ci_p->vcc_type == CI_VCC_DIRECT)
		error =	ci_power_on_direct_epld(ci_p, voltage);

	return error;
}

ca_error_code_t ci_slot_power_disable_epld(struct ci_dvb_ca_en50221 *ca)
{
	ca_error_code_t error = CA_NO_ERROR;
	stm_ci_t *ci_p = (stm_ci_t *)ca->data;

	if (ci_p->vcc_type == CI_VCC)
		error =	ci_power_off_vcc_epld(ci_p);
	else if (ci_p->vcc_type == CI_VCC_MICREL)
		error =	ci_power_off_micrel_epld(ci_p);
	else if (ci_p->vcc_type == CI_VCC_DIRECT)
		error =	ci_power_off_direct_epld(ci_p);

	return error;
}
ca_error_code_t ci_switch_access_mode_epld(struct ci_dvb_ca_en50221 *ca,
					ci_access_mode_t access_mode)
{
	stm_ci_t *ci_p = (stm_ci_t *)ca->data;
	uint32_t ci_ctrl_add = 0;
	uint16_t card_reg_value = 0;
	ci_signal_map iois16 = ci_p->sig_map[CI_SIGNAL_IOIS16];

	ci_ctrl_add = (uint32_t)(ci_p->hw_addr.epld_addr_v +\
				iois16.data.epld_config.addr);
	card_reg_value = readl((const volatile void __iomem *)ci_ctrl_add);

	if (iois16.active_high == true)
		card_reg_value |= iois16.bit_mask;
	else
		card_reg_value &= ~(iois16.bit_mask);

	writel((uint32_t) card_reg_value, (volatile void __iomem *)ci_ctrl_add);

	ci_debug_trace(CI_HAL_RUNTIME,
			"IOIS16 Reg bit set to %d\n",
			(iois16.active_high) ? 1 : 0);
	return CA_NO_ERROR;
}
ca_error_code_t ci_slot_init_epld(struct ci_dvb_ca_en50221 *ca)
{
	uint32_t index = 0;
	stm_ci_t *ci_p = (stm_ci_t *)ca->data;

	for (index = 0; index < CI_SIGNAL_MAX; index++) {
		ci_signal_map signal = ci_p->sig_map[index];
		if (signal.interface_type == CI_EPLD) {
			ci_p->device.epld_reg =
				signal.data.epld_config.epld_int_reg;
			break;
		}
	}
	return CA_NO_ERROR;
}

ca_error_code_t ci_slot_shutdown_epld(struct ci_dvb_ca_en50221 *ca)
{
	stm_ci_t *ci_p = (stm_ci_t *)ca->data;
	memset(&ci_p->device.epld_reg, 0x00, sizeof(ci_epld_intr_reg));
	return CA_NO_ERROR;
}

ca_error_code_t ci_slot_enable_epld(struct ci_dvb_ca_en50221 *ca)
{
	stm_ci_t *ci_p = (stm_ci_t *)ca->data;
	ci_signal_map sel = ci_p->sig_map[CI_SIGNAL_SEL];
	uint32_t	ci_ctrl_add = 0;
	uint16_t	card_reg_value = 0;

	ci_ctrl_add	= (uint32_t)(ci_p->hw_addr.epld_addr_v + \
				sel.data.epld_config.addr);
	card_reg_value = readl((const volatile void __iomem *)ci_ctrl_add);

	if (sel.active_high == true)
		card_reg_value |= sel.bit_mask;
	else
		card_reg_value &= ~(sel.bit_mask);

	/*	POD_SEL# or CI_EN# */
	writel((uint32_t) card_reg_value,(volatile void __iomem *) ci_ctrl_add);

	ci_debug_trace(CI_HAL_RUNTIME,
			"sel Reg bit set to %d\n",
			(sel.active_high) ? 1 : 0);
	return CA_NO_ERROR;
}
/* open	and install int	*/
ca_error_code_t ci_slot_detect_irq_install_epld(struct ci_dvb_ca_en50221 *ca)
{
	ca_error_code_t error = 0;
	stm_ci_t *ci_p = (stm_ci_t *)ca->data;
	uint16_t pin_value = 0;
	ci_signal_map cd1 = ci_p->sig_map[CI_SIGNAL_CD1];
	int irq	= cd1.interrupt_num;
	uint32_t ci_ctrl_add = (uint32_t)(ci_p->hw_addr.epld_addr_v +
			cd1.data.epld_config.addr);

	/* Assuming that both CD1 and CD2 mapped to same EPLD register */
	pin_value = (uint16_t)readl((const volatile void __iomem *)ci_ctrl_add);
	if (irq != 0) {
		error = ci_initialise_interrupt(ci_p, pin_value, CI_SIGNAL_CD1);
		if (error)
			ci_error_trace(CI_HAL_RUNTIME,
					"EPLD irq registration	failed\n");
	}
	return error;
}
ca_error_code_t ci_slot_detect_irq_uninstall_epld(
			struct ci_dvb_ca_en50221 *ca)
{
	stm_ci_t *ci_p = (stm_ci_t *)ca->data;
	ci_signal_map cd1 = ci_p->sig_map[CI_SIGNAL_CD1];
	uint32_t irq = cd1.interrupt_num;
	if (irq != 0)
		free_irq(irq, (void *)ci_p);

	return CA_NO_ERROR;
}
ca_error_code_t ci_slot_detect_irq_enable_epld(struct ci_dvb_ca_en50221 *ca)
{
	uint32_t pin_value = 0;
	stm_ci_t *ci_p = (stm_ci_t *)ca->data;
	ci_signal_map cd1 = ci_p->sig_map[CI_SIGNAL_CD1];
	int irq	= cd1.interrupt_num;

	uint32_t ci_ctrl_add = (uint32_t)(ci_p->hw_addr.epld_addr_v +
	cd1.data.epld_config.addr);

	/* Assuming that both CD1 and CD2 mapped to same EPLD register */
	pin_value = (uint16_t)readl((const volatile void __iomem *)ci_ctrl_add);
	CI_SET_IRQ_TYPE(irq, pin_value ? \
			IRQ_TYPE_LEVEL_LOW : IRQ_TYPE_LEVEL_HIGH);
	if (irq != 0)
		enable_irq(irq);

	return CA_NO_ERROR;
}

ca_error_code_t ci_slot_detect_irq_disable_epld(
			struct ci_dvb_ca_en50221 *ca)
{
	stm_ci_t *ci_p = (stm_ci_t *)ca->data;
	ci_signal_map cd1 = ci_p->sig_map[CI_SIGNAL_CD1];
	int irq	= cd1.interrupt_num;
	if (irq != 0)
		disable_irq_nosync(irq);
	return CA_NO_ERROR;
}

ca_error_code_t ci_slot_data_irq_install_epld(struct ci_dvb_ca_en50221 *ca)
{
	ca_error_code_t error = 0;
	stm_ci_t *ci_p = (stm_ci_t *)ca->data;
	ci_signal_map ireq = ci_p->sig_map[CI_SIGNAL_IREQ];
	uint32_t irq = ireq.interrupt_num;
	uint16_t pin_value;

	uint32_t ci_ctrl_add = (uint32_t)(ci_p->hw_addr.epld_addr_v +\
					ireq.data.epld_config.addr);
	/* Assuming that both CD1 and CD2 mapped to same EPLD register */
	pin_value = (uint16_t)readl((const volatile void __iomem *)ci_ctrl_add);
	if (irq != 0) {
		/* Pin_value not needed	*/
		error = ci_initialise_interrupt(ci_p, pin_value,
				CI_SIGNAL_IREQ);
		if (error)
			ci_error_trace(CI_HAL_RUNTIME,
				"GPIO irq registration failed\n");
	}
	return error;
}
ca_error_code_t ci_slot_data_irq_uninstall_epld(struct ci_dvb_ca_en50221 *ca)
{
	stm_ci_t *ci_p = (stm_ci_t *)ca->data;
	ci_signal_map ireq = ci_p->sig_map[CI_SIGNAL_IREQ];
	uint32_t irq = ireq.interrupt_num;
	if (irq != 0)
		free_irq(irq, (void *)ci_p);

	return CA_NO_ERROR;
}
ca_error_code_t ci_slot_data_irq_enable_epld(struct	ci_dvb_ca_en50221 *ca)
{
	uint16_t pin_value = 0;
	stm_ci_t *ci_p = (stm_ci_t *)ca->data;
	ci_signal_map ireq = ci_p->sig_map[CI_SIGNAL_IREQ];
	uint32_t irq = ireq.interrupt_num;
	uint32_t ci_ctrl_add = (uint32_t)(ci_p->hw_addr.epld_addr_v +\
					ireq.data.epld_config.addr);
	/* Assuming that both CD1 and CD2 mapped to same EPLD register */
	pin_value = (uint16_t)readl((const volatile void __iomem *)ci_ctrl_add);
	if (irq != 0) {
		CI_SET_IRQ_TYPE(irq, pin_value ?\
				IRQ_TYPE_LEVEL_LOW : IRQ_TYPE_LEVEL_HIGH);
		enable_irq(irq);
	}
	return CA_NO_ERROR;

}
ca_error_code_t ci_slot_data_irq_disable_epld(struct ci_dvb_ca_en50221 *ca)
{
	stm_ci_t *ci_p = (stm_ci_t *)ca->data;
	ci_signal_map ireq = ci_p->sig_map[CI_SIGNAL_IREQ];

	uint32_t irq = ireq.interrupt_num;
	if (irq != 0)
		disable_irq_nosync(irq);
	return CA_NO_ERROR;
}

