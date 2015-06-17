#ifdef CONFIG_ARCH_STI
#include <linux/gpio.h>
#include <linux/platform_device.h>
#else
#include <linux/stm/gpio.h>
#include <linux/stm/pio.h>
#include <linux/stm/pad.h>
#endif
#include <linux/irq.h>
#include <linux/kernel_stat.h>
#include "stm_ci.h"
#include "stm_event.h"
#include "ci_hal.h"

#ifdef CONFIG_ARCH_STI
static ca_error_code_t ci_power_on_micrel_pio(stm_ci_t *ci_p,
		ci_voltage_t voltage)
{
	ca_error_code_t error = CA_NO_ERROR;
	ci_signal_map vcc3en	= ci_p->sig_map[CI_SIGNAL_VCC_MICREL_Vcc3EN];
	ci_signal_map vcc5en	= ci_p->sig_map[CI_SIGNAL_VCC_MICREL_Vcc5EN];
	ci_signal_map en0	= ci_p->sig_map[CI_SIGNAL_VCC_MICREL_EN0];
	ci_signal_map en1	= ci_p->sig_map[CI_SIGNAL_VCC_MICREL_EN1];
	switch (voltage) {
	case CI_VOLT_3300:
		gpio_set_value(vcc5en.data.gpio, 0);
		gpio_set_value(vcc3en.data.gpio, 1);
		gpio_set_value(en0.data.gpio, 0);
		gpio_set_value(en1.data.gpio, 1);
	break;
	case CI_VOLT_5000:
		gpio_set_value(vcc5en.data.gpio, 1);
		gpio_set_value(vcc3en.data.gpio, 0);
		gpio_set_value(en0.data.gpio, 0);
		gpio_set_value(en1.data.gpio, 1);
	break;
	default:
		error = -EINVAL;
		ci_error_trace(CI_HAL_RUNTIME,
				"Invalid Voltage \n");
	break;
	}
	return error;
}
static ca_error_code_t ci_power_off_micrel_pio(stm_ci_t *ci_p)
{
	ca_error_code_t	error = CA_NO_ERROR;
	ci_signal_map vcc3en = ci_p->sig_map[CI_SIGNAL_VCC_MICREL_Vcc3EN];
	ci_signal_map vcc5en = ci_p->sig_map[CI_SIGNAL_VCC_MICREL_Vcc5EN];
	ci_signal_map en0 = ci_p->sig_map[CI_SIGNAL_VCC_MICREL_EN0];
	ci_signal_map en1 = ci_p->sig_map[CI_SIGNAL_VCC_MICREL_EN1];
	gpio_set_value(vcc5en.data.gpio, 0);
	gpio_set_value(vcc3en.data.gpio, 0);
	gpio_set_value(en0.data.gpio, 1);
	gpio_set_value(en1.data.gpio, 1);
	return error;
}
static ca_error_code_t ci_power_on_vcc_pio(stm_ci_t *ci_p,
						ci_voltage_t voltage)
{
	ca_error_code_t error = CA_NO_ERROR;
	ci_signal_map vcc_en = ci_p->sig_map[CI_SIGNAL_VCC_EN];
	ci_signal_map vcc_3_5en = ci_p->sig_map[CI_SIGNAL_VCC_3_5EN];
	switch (voltage) {
	case CI_VOLT_3300:
	gpio_set_value(vcc_en.data.gpio, 1);
	gpio_set_value(vcc_3_5en.data.gpio, 0);
	break;
	case CI_VOLT_5000:
	gpio_set_value(vcc_en.data.gpio, 1);
	gpio_set_value(vcc_3_5en.data.gpio, 1);
	break;
	default:
	error = -EINVAL;
	ci_error_trace(CI_HAL_RUNTIME,
			"Invalid Voltage\n");
	break;
	}
	return error;
}
static ca_error_code_t ci_power_off_vcc_pio(stm_ci_t	*ci_p)
{
	ca_error_code_t	error = CA_NO_ERROR;
	ci_signal_map vcc_en	= ci_p->sig_map[CI_SIGNAL_VCC_EN];
	ci_signal_map vcc_3_5en = ci_p->sig_map[CI_SIGNAL_VCC_3_5EN];
	gpio_set_value(vcc_en.data.gpio, 0);
	gpio_set_value(vcc_3_5en.data.gpio, 1);
	return error;
}
static ca_error_code_t ci_power_on_direct_pio(stm_ci_t *ci_p,
				ci_voltage_t voltage)
{
	ca_error_code_t error = CA_NO_ERROR;
	ci_signal_map vcc_en = ci_p->sig_map[CI_SIGNAL_VCC_EN];
	gpio_set_value(vcc_en.data.gpio,
			vcc_en.active_high);
	ci_debug_trace(CI_HAL_RUNTIME,
			"Direct mapped Vcc 0x%x\n",
			vcc_en.data.gpio);
	return error;
}
static ca_error_code_t ci_power_off_direct_pio(stm_ci_t *ci_p)
{
	ca_error_code_t error = CA_NO_ERROR;
	ci_signal_map vcc_en = ci_p->sig_map[CI_SIGNAL_VCC_EN];
	gpio_set_value(vcc_en.data.gpio,
			(~(vcc_en.active_high) & 0x01));
	return error;
}
static ca_error_code_t ci_power_on_alt_volt_pio(stm_ci_t *ci_p,
				ci_voltage_t voltage)
{
	ca_error_code_t error = CA_NO_ERROR;
	ci_signal_map vcc5en = ci_p->sig_map[CI_SIGNAL_VCC_MICREL_Vcc5EN];
	ci_signal_map vcc3en = ci_p->sig_map[CI_SIGNAL_VCC_MICREL_Vcc3EN];
	switch (voltage) {
	case CI_VOLT_3300:
		gpio_set_value(vcc5en.data.gpio, 0);
		gpio_set_value(vcc3en.data.gpio, 1);
		break;
	case CI_VOLT_5000:
		gpio_set_value(vcc5en.data.gpio, 1);
		gpio_set_value(vcc3en.data.gpio, 0);
		break;
	default:
		error = -EINVAL;
		ci_error_trace(CI_HAL_RUNTIME,
				"Invalid Voltage\n");
		break;
	}
	return error;
}
static ca_error_code_t ci_power_off_alt_volt_pio(stm_ci_t *ci_p)
{
	ca_error_code_t error = CA_NO_ERROR;
	ci_signal_map vcc5en = ci_p->sig_map[CI_SIGNAL_VCC_MICREL_Vcc5EN];
	ci_signal_map vcc3en = ci_p->sig_map[CI_SIGNAL_VCC_MICREL_Vcc3EN];
	gpio_set_value(vcc5en.data.gpio, 0);
	gpio_set_value(vcc3en.data.gpio, 0);
	return error;
}
static uint32_t	ci_initialise_interrupt(stm_ci_t *ci_p, uint32_t comp,
				uint32_t SigIndex)
{
	uint32_t error_code = 0, irq = 0;
	uint8_t	owner[32];
	if (SigIndex ==	CI_SIGNAL_CD1) {
		ci_signal_map cd1 = ci_p->sig_map[CI_SIGNAL_CD1];
		irq = gpio_to_irq(cd1.data.gpio);
		sprintf(owner, "%s-%d", "CI_CD1", CI_NO(ci_p->magic_num));
		CI_SET_IRQ_TYPE(irq, comp ?\
				IRQ_TYPE_LEVEL_LOW : IRQ_TYPE_LEVEL_HIGH);
		error_code = request_irq(irq, ci_detect_handler, IRQF_DISABLED ,
					(const char *)owner, (void *)ci_p);
		if (error_code) {
			ci_error_trace(CI_HAL_RUNTIME,
						"Interrupt request failed\n");
			return error_code;
		}
	} else if (SigIndex == CI_SIGNAL_IREQ) {
		ci_signal_map ireq = ci_p->sig_map[CI_SIGNAL_IREQ];
		irq = gpio_to_irq(ireq.data.gpio);
		sprintf(owner, "%s-%d", "CI_IRQ", CI_NO(ci_p->magic_num));
		CI_SET_IRQ_TYPE(irq, comp ?\
			IRQ_TYPE_LEVEL_LOW : IRQ_TYPE_LEVEL_HIGH);
		error_code = request_irq(irq, ci_data_handler, IRQF_DISABLED,
					(const char *)owner, (void *)ci_p);
		if (error_code) {
			ci_error_trace(CI_HAL_RUNTIME,
					"Interrupt request failed\n");
			return error_code;
		}
	}
	disable_irq_nosync(irq);
	return error_code;
}
ca_error_code_t ci_slot_reset_pio(struct ci_dvb_ca_en50221 *ca)
{
	stm_ci_t *ci_p = (stm_ci_t *)ca->data;
	ca_error_code_t error = CA_NO_ERROR;
	ci_signal_map rst = ci_p->sig_map[CI_SIGNAL_RST];
	gpio_set_value(rst.data.gpio, (rst.active_high & 0x01));
	ca_os_sleep_milli_sec(1);
	gpio_set_value(rst.data.gpio, ((~(rst.active_high)) & 0x01));
	return error;
}
ca_error_code_t ci_slot_status_pio(struct ci_dvb_ca_en50221 *ca,
		uint16_t *pin_value_p)
{
	stm_ci_t *ci_p = (stm_ci_t *)ca->data;
	ca_error_code_t error = CA_NO_ERROR;
	uint8_t pin_value = 0;
	ci_signal_map cd1 = ci_p->sig_map[CI_SIGNAL_CD1];
	pin_value = __gpio_get_value(cd1.data.gpio);
	ci_debug_trace(CI_HAL_RUNTIME,
			"Card detect Pio value = 0x%x Pin BitMask = 0x%x\n",
			pin_value,
			cd1.bit_mask);
	*pin_value_p = (uint8_t)pin_value;
	return error;
}
ca_error_code_t ci_slot_power_enable_pio(struct ci_dvb_ca_en50221 *ca,
					ci_voltage_t	voltage)
{
	ca_error_code_t error = CA_NO_ERROR;
	stm_ci_t *ci_p = (stm_ci_t *)ca->data;
	if (ci_p->vcc_type == CI_VCC)
		error =	ci_power_on_vcc_pio(ci_p, voltage);
	else if (ci_p->vcc_type == CI_VCC_MICREL)
		error =	ci_power_on_micrel_pio(ci_p, voltage);
	else if (ci_p->vcc_type == CI_VCC_DIRECT)
		error =	ci_power_on_direct_pio(ci_p, voltage);
	else if (ci_p->vcc_type == CI_VCC_ALT_VOLT_EN)
		error =	ci_power_on_alt_volt_pio(ci_p, voltage);
	return error;
}
ca_error_code_t ci_slot_power_disable_pio(struct ci_dvb_ca_en50221 *ca)
{
	ca_error_code_t error = CA_NO_ERROR;
	stm_ci_t *ci_p = (stm_ci_t *)ca->data;
	if (ci_p->vcc_type == CI_VCC)
		error =	ci_power_off_vcc_pio(ci_p);
	else if (ci_p->vcc_type == CI_VCC_MICREL)
		error =	ci_power_off_micrel_pio(ci_p);
	else if (ci_p->vcc_type == CI_VCC_DIRECT)
		error =	ci_power_off_direct_pio(ci_p);
	else if (ci_p->vcc_type == CI_VCC_ALT_VOLT_EN)
		error =	ci_power_off_alt_volt_pio(ci_p);
	return error;
}
ca_error_code_t ci_switch_access_mode_pio(struct ci_dvb_ca_en50221 *ca,
					ci_access_mode_t access_mode)
{
	stm_ci_t *ci_p = (stm_ci_t *)ca->data;
	ci_signal_map iois16 = ci_p->sig_map[CI_SIGNAL_IOIS16];
	gpio_set_value(iois16.data.gpio, iois16.active_high);
	ci_debug_trace(CI_HAL_RUNTIME,
			"IOIS16 Pio set to %d\n",
			(iois16.active_high) ? 1 : 0);
	return CA_NO_ERROR;
}
ca_error_code_t ci_slot_init_pio(struct ci_dvb_ca_en50221 *ca)
{
	uint32_t gpio, index = 0;
	uint8_t owner[32];
	stm_ci_t *ci_p = (stm_ci_t *)ca->data;
	struct platform_device *pdev = ci_p->ci_hw_data_p->pdev;
	for (index = 0; index < CI_SIGNAL_MAX; index++) {
		ci_signal_map signal =	ci_p->sig_map[index];
		if (signal.interface_type == CI_GPIO) {
			sprintf(owner, "%s-%d", "CI", CI_NO(ci_p->magic_num));
			gpio = signal.data.gpio;
			if (gpio_is_valid(gpio)) {
				if (devm_gpio_request(&pdev->dev, gpio, owner)) {
					ci_error_trace(CI_HAL_RUNTIME,
						"%d GPIO Request failed\n",
						index);
					return -EBUSY;
				}
			} else
				ci_debug_trace(CI_HAL_RUNTIME,
					"%d GPIO request passed\n",
					index);
		}
	}
	return CA_NO_ERROR;
}
ca_error_code_t ci_slot_shutdown_pio(struct ci_dvb_ca_en50221 *ca)
{
	uint32_t gpio, index = 0;
	stm_ci_t *ci_p = (stm_ci_t *)ca->data;
	struct platform_device *pdev = ci_p->ci_hw_data_p->pdev;
	for (index = 0; index < CI_SIGNAL_MAX; index++) {
		ci_signal_map signal =	ci_p->sig_map[index];
		if (signal.interface_type == CI_GPIO) {
			gpio = signal.data.gpio;
			if (gpio)
				devm_gpio_free(&pdev->dev, gpio);
		}
	}
	return CA_NO_ERROR;
}
ca_error_code_t ci_slot_enable_pio(struct ci_dvb_ca_en50221 *ca)
{
	stm_ci_t *ci_p = (stm_ci_t *)ca->data;
	ca_error_code_t error = CA_NO_ERROR;
	ci_signal_map sel = ci_p->sig_map[CI_SIGNAL_SEL];
	gpio_set_value(sel.data.gpio, sel.active_high);
	return error;
}
ca_error_code_t ci_slot_detect_irq_install_pio(struct ci_dvb_ca_en50221 *ca)
{
	ca_error_code_t error = CA_NO_ERROR;
	stm_ci_t *ci_p = (stm_ci_t *)ca->data;
	uint8_t pin_value = 0;
	ci_signal_map cd1 = ci_p->sig_map[CI_SIGNAL_CD1];
	pin_value = __gpio_get_value(cd1.data.gpio);
	error = ci_initialise_interrupt(ci_p, pin_value, CI_SIGNAL_CD1);
	if (error)
		ci_error_trace(CI_HAL_RUNTIME,
				"GPIO irq registration	failed\n");
	return error;
}
ca_error_code_t ci_slot_detect_irq_uninstall_pio(
				struct ci_dvb_ca_en50221 *ca)
{
	stm_ci_t *ci_p = (stm_ci_t *)ca->data;
	ci_signal_map cd1 = ci_p->sig_map[CI_SIGNAL_CD1];
	uint32_t irq = gpio_to_irq(cd1.data.gpio);
	free_irq(irq, (void *)ci_p);
	return CA_NO_ERROR;
}
ca_error_code_t ci_slot_detect_irq_enable_pio(struct ci_dvb_ca_en50221 *ca)
{
	uint32_t pin_value = 0;
	stm_ci_t *ci_p = (stm_ci_t *)ca->data;
	ci_signal_map cd1 = ci_p->sig_map[CI_SIGNAL_CD1];
	int irq	= gpio_to_irq(cd1.data.gpio);
	pin_value = __gpio_get_value(cd1.data.gpio);
	CI_SET_IRQ_TYPE(irq, pin_value ?\
			IRQ_TYPE_LEVEL_LOW : IRQ_TYPE_LEVEL_HIGH);
	enable_irq(irq);
	return CA_NO_ERROR;
}
ca_error_code_t ci_slot_detect_irq_disable_pio(struct ci_dvb_ca_en50221 *ca)
{
	stm_ci_t *ci_p = (stm_ci_t *)ca->data;
	ci_signal_map cd1 = ci_p->sig_map[CI_SIGNAL_CD1];
	int irq	= gpio_to_irq(cd1.data.gpio);
	disable_irq_nosync(irq);
	return CA_NO_ERROR;
}
ca_error_code_t ci_slot_data_irq_install_pio(struct	ci_dvb_ca_en50221 *ca)
{
	ca_error_code_t error = CA_NO_ERROR;
	stm_ci_t *ci_p = (stm_ci_t *)ca->data;
	ci_signal_map ireq = ci_p->sig_map[CI_SIGNAL_IREQ];
	uint8_t	pin_value;
	pin_value = __gpio_get_value(ireq.data.gpio);
	error = ci_initialise_interrupt(ci_p, pin_value,
			CI_SIGNAL_IREQ);
	if (error)
		ci_error_trace(CI_HAL_RUNTIME,
				"GPIO irq registration failed\n");
	return error;
}
ca_error_code_t ci_slot_data_irq_uninstall_pio(struct ci_dvb_ca_en50221 *ca)
{
	stm_ci_t *ci_p = (stm_ci_t *)ca->data;
	ci_signal_map ireq = ci_p->sig_map[CI_SIGNAL_IREQ];
	uint32_t irq = gpio_to_irq(ireq.data.gpio);
	free_irq(irq, (void *)ci_p);
	return CA_NO_ERROR;
}
ca_error_code_t ci_slot_data_irq_enable_pio(struct ci_dvb_ca_en50221 *ca)
{
	uint32_t pin_value = 0;
	stm_ci_t *ci_p = (stm_ci_t *)ca->data;
	ci_signal_map ireq = ci_p->sig_map[CI_SIGNAL_IREQ];
	uint32_t irq = gpio_to_irq(ireq.data.gpio);
	if (ci_p->device.data_irq_enabled == true)
		return CA_NO_ERROR;
	pin_value = __gpio_get_value(ireq.data.gpio);
	CI_SET_IRQ_TYPE(irq, pin_value ?\
			IRQ_TYPE_LEVEL_LOW : IRQ_TYPE_LEVEL_HIGH);
	enable_irq(irq);
	ci_p->device.data_irq_enabled = true;
	return CA_NO_ERROR;
}
ca_error_code_t ci_slot_data_irq_disable_pio(struct	ci_dvb_ca_en50221 *ca)
{
	stm_ci_t *ci_p = (stm_ci_t *)ca->data;
	ci_signal_map ireq = ci_p->sig_map[CI_SIGNAL_IREQ];
	uint32_t irq = gpio_to_irq(ireq.data.gpio);
	if (ci_p->device.data_irq_enabled == false)
		return CA_NO_ERROR;
	disable_irq_nosync(irq);
	ci_p->device.data_irq_enabled =	false;
	return CA_NO_ERROR;
}
#else
static ca_error_code_t ci_power_on_micrel_pio(stm_ci_t *ci_p,
		ci_voltage_t voltage)
{
	ca_error_code_t error = CA_NO_ERROR;
	ci_signal_map vcc3en	= ci_p->sig_map[CI_SIGNAL_VCC_MICREL_Vcc3EN];
	ci_signal_map vcc5en	= ci_p->sig_map[CI_SIGNAL_VCC_MICREL_Vcc5EN];
	ci_signal_map en0	= ci_p->sig_map[CI_SIGNAL_VCC_MICREL_EN0];
	ci_signal_map en1	= ci_p->sig_map[CI_SIGNAL_VCC_MICREL_EN1];

	switch (voltage) {
	case CI_VOLT_3300:
		gpio_set_value(vcc5en.data.pad_config->gpios[0].gpio, 0);
		gpio_set_value(vcc3en.data.pad_config->gpios[0].gpio, 1);
		gpio_set_value(en0.data.pad_config->gpios[0].gpio, 0);
		gpio_set_value(en1.data.pad_config->gpios[0].gpio, 1);
	break;

	case CI_VOLT_5000:
		gpio_set_value(vcc5en.data.pad_config->gpios[0].gpio, 1);
		gpio_set_value(vcc3en.data.pad_config->gpios[0].gpio, 0);
		gpio_set_value(en0.data.pad_config->gpios[0].gpio, 0);
		gpio_set_value(en1.data.pad_config->gpios[0].gpio, 1);
	break;

	default:
		error = -EINVAL;
		ci_error_trace(CI_HAL_RUNTIME,
				"Invalid Voltage \n");
	break;
	}
	return error;
}

static ca_error_code_t ci_power_off_micrel_pio(stm_ci_t *ci_p)
{
	ca_error_code_t	error = CA_NO_ERROR;
	ci_signal_map vcc3en = ci_p->sig_map[CI_SIGNAL_VCC_MICREL_Vcc3EN];
	ci_signal_map vcc5en = ci_p->sig_map[CI_SIGNAL_VCC_MICREL_Vcc5EN];
	ci_signal_map en0 = ci_p->sig_map[CI_SIGNAL_VCC_MICREL_EN0];
	ci_signal_map en1 = ci_p->sig_map[CI_SIGNAL_VCC_MICREL_EN1];

	gpio_set_value(vcc5en.data.pad_config->gpios[0].gpio, 0);
	gpio_set_value(vcc3en.data.pad_config->gpios[0].gpio, 0);
	gpio_set_value(en0.data.pad_config->gpios[0].gpio, 1);
	gpio_set_value(en1.data.pad_config->gpios[0].gpio, 1);

	return error;
}

/* Non-micrel mapped Vcc */
static ca_error_code_t ci_power_on_vcc_pio(stm_ci_t *ci_p,
						ci_voltage_t voltage)
{
	ca_error_code_t error = CA_NO_ERROR;
	ci_signal_map vcc_en = ci_p->sig_map[CI_SIGNAL_VCC_EN];
	ci_signal_map vcc_3_5en = ci_p->sig_map[CI_SIGNAL_VCC_3_5EN];

	switch (voltage) {
	case CI_VOLT_3300:
	gpio_set_value(vcc_en.data.pad_config->gpios[0].gpio, 1);
	gpio_set_value(vcc_3_5en.data.pad_config->gpios[0].gpio, 0);
	break;

	case CI_VOLT_5000:
	gpio_set_value(vcc_en.data.pad_config->gpios[0].gpio, 1);
	gpio_set_value(vcc_3_5en.data.pad_config->gpios[0].gpio, 1);
	break;

	default:
	error = -EINVAL;
	ci_error_trace(CI_HAL_RUNTIME,
			"Invalid Voltage\n");
	break;
	}
	return error;
}

static ca_error_code_t ci_power_off_vcc_pio(stm_ci_t	*ci_p)
{
	ca_error_code_t	error = CA_NO_ERROR;
	ci_signal_map vcc_en	= ci_p->sig_map[CI_SIGNAL_VCC_EN];
	ci_signal_map vcc_3_5en = ci_p->sig_map[CI_SIGNAL_VCC_3_5EN];

	gpio_set_value(vcc_en.data.pad_config->gpios[0].gpio, 0);
	gpio_set_value(vcc_3_5en.data.pad_config->gpios[0].gpio, 1);

	return error;
}

/* direct mapped Vcc */
static ca_error_code_t ci_power_on_direct_pio(stm_ci_t *ci_p,
				ci_voltage_t voltage)
{
	ca_error_code_t error = CA_NO_ERROR;
	ci_signal_map vcc_en = ci_p->sig_map[CI_SIGNAL_VCC_EN];

	gpio_set_value(vcc_en.data.pad_config->gpios[0].gpio,
			vcc_en.active_high);

	ci_debug_trace(CI_HAL_RUNTIME,
			"Direct mapped Vcc 0x%x\n",
			vcc_en.data.pad_config->gpios[0].gpio);

	return error;
}

static ca_error_code_t ci_power_off_direct_pio(stm_ci_t *ci_p)
{
	ca_error_code_t error = CA_NO_ERROR;
	ci_signal_map vcc_en = ci_p->sig_map[CI_SIGNAL_VCC_EN];

	gpio_set_value(vcc_en.data.pad_config->gpios[0].gpio,
			(~(vcc_en.active_high) & 0x01));

	return error;
}

static ca_error_code_t ci_power_on_alt_volt_pio(stm_ci_t *ci_p,
				ci_voltage_t voltage)
{
	ca_error_code_t error = CA_NO_ERROR;
	ci_signal_map vcc5en = ci_p->sig_map[CI_SIGNAL_VCC_MICREL_Vcc5EN];
	ci_signal_map vcc3en = ci_p->sig_map[CI_SIGNAL_VCC_MICREL_Vcc3EN];

	switch (voltage) {
	case CI_VOLT_3300:
		gpio_set_value(vcc5en.data.pad_config->gpios[0].gpio, 0);
		gpio_set_value(vcc3en.data.pad_config->gpios[0].gpio, 1);
		break;

	case CI_VOLT_5000:
		gpio_set_value(vcc5en.data.pad_config->gpios[0].gpio, 1);
		gpio_set_value(vcc3en.data.pad_config->gpios[0].gpio, 0);
		break;

	default:
		error = -EINVAL;
		ci_error_trace(CI_HAL_RUNTIME,
				"Invalid Voltage\n");
		break;
	}
	return error;
}

static ca_error_code_t ci_power_off_alt_volt_pio(stm_ci_t *ci_p)
{
	ca_error_code_t error = CA_NO_ERROR;
	ci_signal_map vcc5en = ci_p->sig_map[CI_SIGNAL_VCC_MICREL_Vcc5EN];
	ci_signal_map vcc3en = ci_p->sig_map[CI_SIGNAL_VCC_MICREL_Vcc3EN];

	gpio_set_value(vcc5en.data.pad_config->gpios[0].gpio, 0);
	gpio_set_value(vcc3en.data.pad_config->gpios[0].gpio, 0);

	return error;
}

static uint32_t	ci_initialise_interrupt(stm_ci_t *ci_p, uint32_t comp,
				uint32_t SigIndex)
{
	uint32_t error_code = 0, irq = 0;
	uint8_t	owner[32];

	if (SigIndex ==	CI_SIGNAL_CD1) {
		ci_signal_map cd1 = ci_p->sig_map[CI_SIGNAL_CD1];
		irq = gpio_to_irq(
			cd1.data.pad_config->gpios[CI_DETECT_GPIO].gpio);

		sprintf(owner, "%s-%d", "CI_CD1", CI_NO(ci_p->magic_num));
		CI_SET_IRQ_TYPE(irq, comp ?\
				IRQ_TYPE_LEVEL_LOW : IRQ_TYPE_LEVEL_HIGH);

		error_code = request_irq(irq, ci_detect_handler, IRQF_DISABLED ,
					(const char *)owner, (void *)ci_p);
		if (error_code) {
			ci_error_trace(CI_HAL_RUNTIME,
						"Interrupt request failed\n");
			return error_code;
		}

	} else if (SigIndex == CI_SIGNAL_IREQ) {
		ci_signal_map ireq = ci_p->sig_map[CI_SIGNAL_IREQ];
		irq = gpio_to_irq(
			ireq.data.pad_config->gpios[CI_IRQ_GPIO].gpio);

		sprintf(owner, "%s-%d", "CI_IRQ", CI_NO(ci_p->magic_num));

		CI_SET_IRQ_TYPE(irq, comp ?\
			IRQ_TYPE_LEVEL_LOW : IRQ_TYPE_LEVEL_HIGH);

		error_code = request_irq(irq, ci_data_handler, IRQF_DISABLED,
					(const char *)owner, (void *)ci_p);
		if (error_code) {
			ci_error_trace(CI_HAL_RUNTIME,
					"Interrupt request failed\n");
			return error_code;
		}
	}
	disable_irq_nosync(irq);

	return error_code;
}

/***************************************************************************/
/* exported Functions */
/***************************************************************************/
ca_error_code_t ci_slot_reset_pio(struct ci_dvb_ca_en50221 *ca)
{
	stm_ci_t *ci_p = (stm_ci_t *)ca->data;
	ca_error_code_t error = CA_NO_ERROR;
	ci_signal_map rst = ci_p->sig_map[CI_SIGNAL_RST];

	gpio_set_value(rst.data.pad_config->gpios[0].gpio,
			(rst.active_high & 0x01));
	ca_os_sleep_milli_sec(1);
	gpio_set_value(rst.data.pad_config->gpios[0].gpio,
			((~(rst.active_high)) & 0x01));

	return error;
}

ca_error_code_t ci_slot_status_pio(struct ci_dvb_ca_en50221 *ca,
		uint16_t *pin_value_p)
{
	stm_ci_t *ci_p = (stm_ci_t *)ca->data;

	ca_error_code_t error = CA_NO_ERROR;
	uint8_t pin_value = 0;

	ci_signal_map cd1 = ci_p->sig_map[CI_SIGNAL_CD1];
	pin_value = __gpio_get_value(
			cd1.data.pad_config->gpios[CI_DETECT_GPIO].gpio);
	ci_debug_trace(CI_HAL_RUNTIME,
			"Card detect Pio value = 0x%x Pin BitMask = 0x%x\n",
			pin_value,
			cd1.bit_mask);

	*pin_value_p = (uint8_t)pin_value;
	return error;
}

ca_error_code_t ci_slot_power_enable_pio(struct ci_dvb_ca_en50221 *ca,
					ci_voltage_t	voltage)
{
	ca_error_code_t error = CA_NO_ERROR;
	stm_ci_t *ci_p = (stm_ci_t *)ca->data;

	if (ci_p->vcc_type == CI_VCC)
		error =	ci_power_on_vcc_pio(ci_p, voltage);

	else if (ci_p->vcc_type == CI_VCC_MICREL)
		error =	ci_power_on_micrel_pio(ci_p, voltage);

	else if (ci_p->vcc_type == CI_VCC_DIRECT)
		error =	ci_power_on_direct_pio(ci_p, voltage);

	else if (ci_p->vcc_type == CI_VCC_ALT_VOLT_EN)
		error =	ci_power_on_alt_volt_pio(ci_p, voltage);

	return error;
}

ca_error_code_t ci_slot_power_disable_pio(struct ci_dvb_ca_en50221 *ca)
{
	ca_error_code_t error = CA_NO_ERROR;
	stm_ci_t *ci_p = (stm_ci_t *)ca->data;

	if (ci_p->vcc_type == CI_VCC)
		error =	ci_power_off_vcc_pio(ci_p);
	else if (ci_p->vcc_type == CI_VCC_MICREL)
		error =	ci_power_off_micrel_pio(ci_p);
	else if (ci_p->vcc_type == CI_VCC_DIRECT)
		error =	ci_power_off_direct_pio(ci_p);
	else if (ci_p->vcc_type == CI_VCC_ALT_VOLT_EN)
		error =	ci_power_off_alt_volt_pio(ci_p);

	return error;

}
ca_error_code_t ci_switch_access_mode_pio(struct ci_dvb_ca_en50221 *ca,
					ci_access_mode_t access_mode)
{
	stm_ci_t *ci_p = (stm_ci_t *)ca->data;
	ci_signal_map iois16 = ci_p->sig_map[CI_SIGNAL_IOIS16];
	gpio_set_value(iois16.data.pad_config->gpios[0].gpio,
				iois16.active_high);

	ci_debug_trace(CI_HAL_RUNTIME,
			"IOIS16 Pio set to %d\n",
			(iois16.active_high) ? 1 : 0);
	return CA_NO_ERROR;
}

ca_error_code_t ci_slot_init_pio(struct ci_dvb_ca_en50221 *ca)
{
	uint32_t index = 0;
	uint8_t owner[32];

	stm_ci_t *ci_p = (stm_ci_t *)ca->data;

	for (index = 0; index < CI_SIGNAL_MAX; index++) {
		ci_signal_map signal =	ci_p->sig_map[index];

		if (signal.interface_type == CI_GPIO) {
			sprintf(owner, "%s-%d", "CI", CI_NO(ci_p->magic_num));

			ci_p->ci_info[index].pad.pad_state = stm_pad_claim(
					signal.data.pad_config, owner);
			if (NULL == ci_p->ci_info[index].pad.pad_state)	{
				ci_error_trace(CI_HAL_RUNTIME,
						"%d Pad claim failed\n",
						index);
				return -EBUSY;
			} else {
				ci_debug_trace(CI_HAL_RUNTIME,
					"%d Pad claim passed signal\n",
					index);
			}
		}
	}
	return CA_NO_ERROR;
}

ca_error_code_t ci_slot_shutdown_pio(struct ci_dvb_ca_en50221 *ca)
{
	uint32_t index = 0;
	stm_ci_t *ci_p = (stm_ci_t *)ca->data;

	for (index = 0; index < CI_SIGNAL_MAX; index++) {
		ci_signal_map signal =	ci_p->sig_map[index];
		if (signal.interface_type == CI_GPIO) {
			if (ci_p->ci_info[index].pad.pad_state) {
				stm_pad_release(
					ci_p->ci_info[index].pad.pad_state);
				ci_p->ci_info[index].pad.pad_state = NULL;
			}
		}
	}
	return CA_NO_ERROR;
}

ca_error_code_t ci_slot_enable_pio(struct ci_dvb_ca_en50221 *ca)
{
	stm_ci_t *ci_p = (stm_ci_t *)ca->data;
	ca_error_code_t error = CA_NO_ERROR;
	ci_signal_map sel = ci_p->sig_map[CI_SIGNAL_SEL];

	/*	POD_SEL# or CI_EN# */
	gpio_set_value(sel.data.pad_config->gpios[0].gpio,
				sel.active_high);

	return error;
}

/* open	and install pio	*/
ca_error_code_t ci_slot_detect_irq_install_pio(struct ci_dvb_ca_en50221 *ca)
{
	ca_error_code_t error = CA_NO_ERROR;
	stm_ci_t *ci_p = (stm_ci_t *)ca->data;
	uint8_t pin_value = 0;
	ci_signal_map cd1 = ci_p->sig_map[CI_SIGNAL_CD1];

	pin_value = __gpio_get_value(
			cd1.data.pad_config->gpios[CI_DETECT_GPIO].gpio);

	error = ci_initialise_interrupt(ci_p, pin_value, CI_SIGNAL_CD1);
	if (error)
		ci_error_trace(CI_HAL_RUNTIME,
				"GPIO irq registration	failed\n");
	return error;
}

ca_error_code_t ci_slot_detect_irq_uninstall_pio(
				struct ci_dvb_ca_en50221 *ca)
{
	stm_ci_t *ci_p = (stm_ci_t *)ca->data;
	ci_signal_map cd1 = ci_p->sig_map[CI_SIGNAL_CD1];
	uint32_t irq = gpio_to_irq(
			cd1.data.pad_config->gpios[CI_DETECT_GPIO].gpio);

	free_irq(irq, (void *)ci_p);
	return CA_NO_ERROR;
}

ca_error_code_t ci_slot_detect_irq_enable_pio(struct ci_dvb_ca_en50221 *ca)
{
	uint32_t pin_value = 0;
	stm_ci_t *ci_p = (stm_ci_t *)ca->data;

	ci_signal_map cd1 = ci_p->sig_map[CI_SIGNAL_CD1];
	int irq	= gpio_to_irq(cd1.data.pad_config->gpios[CI_DETECT_GPIO].gpio);

	pin_value = __gpio_get_value(
			cd1.data.pad_config->gpios[CI_DETECT_GPIO].gpio);

	CI_SET_IRQ_TYPE(irq, pin_value ?\
			IRQ_TYPE_LEVEL_LOW : IRQ_TYPE_LEVEL_HIGH);
	enable_irq(irq);
	return CA_NO_ERROR;
}

ca_error_code_t ci_slot_detect_irq_disable_pio(struct ci_dvb_ca_en50221 *ca)
{
	stm_ci_t *ci_p = (stm_ci_t *)ca->data;
	ci_signal_map cd1 = ci_p->sig_map[CI_SIGNAL_CD1];
	int irq	= gpio_to_irq(cd1.data.pad_config->gpios[CI_DETECT_GPIO].gpio);
	disable_irq_nosync(irq);
	return CA_NO_ERROR;
}

ca_error_code_t ci_slot_data_irq_install_pio(struct	ci_dvb_ca_en50221 *ca)
{
	ca_error_code_t error = CA_NO_ERROR;
	stm_ci_t *ci_p = (stm_ci_t *)ca->data;
	ci_signal_map ireq = ci_p->sig_map[CI_SIGNAL_IREQ];
	uint8_t	pin_value;
	pin_value = __gpio_get_value(
			ireq.data.pad_config->gpios[CI_IRQ_GPIO].gpio);

	error = ci_initialise_interrupt(ci_p, pin_value,
			CI_SIGNAL_IREQ);
	if (error)
		ci_error_trace(CI_HAL_RUNTIME,
				"GPIO irq registration failed\n");

	return error;
}
ca_error_code_t ci_slot_data_irq_uninstall_pio(struct ci_dvb_ca_en50221 *ca)
{
	stm_ci_t *ci_p = (stm_ci_t *)ca->data;
	ci_signal_map ireq = ci_p->sig_map[CI_SIGNAL_IREQ];
	uint32_t irq = gpio_to_irq(
			ireq.data.pad_config->gpios[CI_IRQ_GPIO].gpio);
	free_irq(irq, (void *)ci_p);

	return CA_NO_ERROR;
}
ca_error_code_t ci_slot_data_irq_enable_pio(struct ci_dvb_ca_en50221 *ca)
{
	uint32_t pin_value = 0;
	stm_ci_t *ci_p = (stm_ci_t *)ca->data;
	ci_signal_map ireq = ci_p->sig_map[CI_SIGNAL_IREQ];
	uint32_t irq = gpio_to_irq(
			ireq.data.pad_config->gpios[CI_IRQ_GPIO].gpio);

	if (ci_p->device.data_irq_enabled == true)
		return CA_NO_ERROR;

	pin_value = __gpio_get_value(
			ireq.data.pad_config->gpios[CI_IRQ_GPIO].gpio);

	CI_SET_IRQ_TYPE(irq, pin_value ?\
			IRQ_TYPE_LEVEL_LOW : IRQ_TYPE_LEVEL_HIGH);

	enable_irq(irq);
	ci_p->device.data_irq_enabled = true;

	return CA_NO_ERROR;
}
ca_error_code_t ci_slot_data_irq_disable_pio(struct	ci_dvb_ca_en50221 *ca)
{
	stm_ci_t *ci_p = (stm_ci_t *)ca->data;
	ci_signal_map ireq = ci_p->sig_map[CI_SIGNAL_IREQ];
	uint32_t irq = gpio_to_irq(
			ireq.data.pad_config->gpios[CI_IRQ_GPIO].gpio);

	if (ci_p->device.data_irq_enabled == false)
		return CA_NO_ERROR;

	disable_irq_nosync(irq);
	ci_p->device.data_irq_enabled =	false;
	return CA_NO_ERROR;
}
#endif
