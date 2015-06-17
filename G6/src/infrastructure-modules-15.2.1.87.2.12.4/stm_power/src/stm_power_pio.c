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
#include <linux/gpio.h>
#include <linux/irq.h>
#include "lpm_def.h"

static int pio_port = 3;
static int pio_pin = 4;

module_param(pio_port, int, 0764);
MODULE_PARM_DESC(pio_port, "Pio Bank used for wakeup");
module_param(pio_pin, int, 0764);
MODULE_PARM_DESC(pio_pin, "Pio Pin used for wakeup");

#ifdef CONFIG_STM_LPM_DEBUG
#define lpm_debug pr_info
#else
#define lpm_debug(fmt,arg...);
#endif
static irqreturn_t gpio_handler(int irq, void *ptr)
{
	uint8_t pin_value;
	disable_irq_nosync(irq);
	pin_value =  __gpio_get_value(stm_gpio(pio_port, pio_pin));
	lpm_debug("gpio  intrrupt called %d\n",pin_value);
	irq_set_irq_type(irq, pin_value ? IRQ_TYPE_LEVEL_LOW : IRQ_TYPE_LEVEL_HIGH);
	enable_irq(irq);
	return IRQ_HANDLED;
}

int config_pio(void)
{
	int ret = 0;
	uint8_t pin_value;
	struct stm_lpm_pio_setting pio_setting = {
		.pio_bank = pio_port,
		.pio_pin = pio_pin,
		.pio_level =  STM_LPM_PIO_HIGH,
		.pio_use = STM_LPM_PIO_WAKEUP,
		.pio_direction = STM_LPM_PIO_INPUT,
		.interrupt_enabled = 1,
	};
	lpm_debug("config_pio pio_bank %d, pio_pin %d\n",
		pio_port, pio_pin);

	ret = gpio_request(stm_gpio(pio_port, pio_pin), "POWER_PIO");
	if (ret < 0)
		pr_err(" Gpio request failed!!!!!!\n");
	gpio_direction_input(stm_gpio(pio_port, pio_pin));
	pin_value =  __gpio_get_value(stm_gpio(pio_port, pio_pin));	
	/* Set IRQ type at which edge wakeup is required */;
	irq_set_irq_type(gpio_to_irq(stm_gpio(pio_port, pio_pin)),
		 (pin_value?IRQ_TYPE_LEVEL_LOW:IRQ_TYPE_LEVEL_HIGH));

	/* register own function with PIO ISR */
	ret = request_irq(gpio_to_irq(stm_gpio(pio_port, pio_pin)),
		gpio_handler, IRQF_DISABLED, "sbc_pio_irq", NULL);
	if (ret < 0) {
		gpio_free(stm_gpio(pio_port, pio_pin));
		pr_err("ERROR: Request irq Not done!\n");
		return ret;
	}
	enable_irq_wake(gpio_to_irq(stm_gpio(pio_port, pio_pin)));
	pio_setting.pio_level =
		(pin_value ? STM_LPM_PIO_HIGH : STM_LPM_PIO_LOW);
	lpm_debug(" pio value %d\n", pin_value);	
	stm_lpm_setup_pio(&pio_setting);
	return 0;
}

static int __init infra_gpio_wakeup_init(void)
{
	config_pio();
	return 0;
}

void __exit infra_gpio_wakeup_exit(void)
{
	free_irq(gpio_to_irq(stm_gpio(pio_port, pio_pin)), NULL);
	gpio_free(stm_gpio(pio_port, pio_pin));
}

module_init(infra_gpio_wakeup_init);
module_exit(infra_gpio_wakeup_exit);

MODULE_AUTHOR("STMicroelectronics  <www.st.com>");
MODULE_DESCRIPTION("GPIO setup as wake up device");
MODULE_LICENSE("GPL");
