/*
 * arch/sh/boards/st/mb796/setup.c
 *
 * Copyright (C) 2009 STMicroelectronics Limited
 * Author: Pawel Moll (pawel.moll@st.com)
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 */

#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/leds.h>
#include <linux/delay.h>
#include <linux/phy.h>
#include <linux/gpio.h>
#include <linux/stm/pio.h>
#include <linux/stm/soc.h>
#include <linux/stm/emi.h>
#include "../common/common.h"

#define MB796_NOTPIORESETMII stpio_to_gpio(0, 6)

static int mb796_ascs[] __initdata = { 2, 3 };

static void __init mb796_setup(char **cmdline_p)
{
	printk(KERN_INFO "STMicroelectronics STi5289-MBOARD (mb796) "
			"initialisation\n");

	stx5206_early_device_init();
	stx5206_configure_asc(mb796_ascs, ARRAY_SIZE(mb796_ascs), 0);
}

static struct plat_ssc_data mb796_ssc_plat_data = {
	.capability =
		ssc0_has(SSC_I2C_CAPABILITY) | /* Internal I2C link */
		ssc1_has(SSC_I2C_CAPABILITY) |
		ssc2_has(SSC_I2C_CAPABILITY) |
		ssc3_has(SSC_I2C_CAPABILITY),
};



static struct platform_device mb796_led = {
	.name = "leds-gpio",
	.id = 0,
	.dev.platform_data = &(struct gpio_led_platform_data) {
		.num_leds = 1,
		.leds = (struct gpio_led[]) {
			{	/* J26-D fitted, J21-A 2-3 */
				.name = "LD15 (GREEN)",
				.default_trigger = "heartbeat",
				.gpio = stpio_to_gpio(3, 3),
			},
		},
	},
};



static int mb796_phy_reset(void *bus)
{
	gpio_set_value(MB796_NOTPIORESETMII, 0);
	udelay(15000);
	gpio_set_value(MB796_NOTPIORESETMII, 1);

	return 0;
}

static struct plat_stmmacphy_data mb796_phy_plat_data = {
	/* Micrel */
	.bus_id = 0,
	.phy_addr = -1,
	.phy_mask = 0,
	.interface = PHY_INTERFACE_MODE_MII,
	.phy_reset = &mb796_phy_reset,
};

static struct platform_device mb796_phy_device = {
	.name = "stmmacphy",
	.id = -1,
	.dev.platform_data = &mb796_phy_plat_data,
};

static struct platform_device *mb796_devices[] __initdata = {
	&mb796_led,
	&mb796_phy_device,
};

/* Configuration based on Futarque-RC signals train. */
static lirc_scd_t mb796_lirc_scd = {
	.code = 0x3fffc028,
	.nomtime = 0x1f4,
	.noiserecov = 0,
};

#ifdef CONFIG_SH_ST_MB705
static void mb705_epld_pci_reset(void)
{
	mb705_reset(EPLD_EMI_RESET_SW1, 1000);
	/* PCI spec says one second */
	mdelay(10);
}
#endif

/* PCI configuration */

static struct pci_config_data mb796_pci_config = {
	.pci_irq = {
		PCI_PIN_DEFAULT,
		PCI_PIN_DEFAULT, /* J20-C fitted, J20-D not fitted */
		PCI_PIN_DEFAULT,
		PCI_PIN_DEFAULT
	},
	.serr_irq = PCI_PIN_UNUSED,
	.idsel_lo = 30, /* J15 2-3 */
	.idsel_hi = 30,
	.req_gnt = {
		PCI_PIN_DEFAULT,
		PCI_PIN_UNUSED,
		PCI_PIN_UNUSED,
		PCI_PIN_UNUSED
	},
	.pci_clk = 33333333,
	/* Well, this board doesn't really provide any
	 * means of resetting the PCI devices... */
	.pci_reset_gpio = -EINVAL,
};

int pcibios_map_platform_irq(struct pci_dev *dev, u8 slot, u8 pin)
{
       /* We can use the standard function on this board */
       return stx5206_pcibios_map_platform_irq(&mb796_pci_config, pin);
}

static int __init mb796_device_init(void)
{
	gpio_request(MB796_NOTPIORESETMII, "notPioResetMii");
	gpio_direction_output(MB796_NOTPIORESETMII, 1);

	stx5206_configure_ssc(&mb796_ssc_plat_data);
	stx5206_configure_lirc(stx5206_lirc_rx_ir, 0, 0, &mb796_lirc_scd);

	stx5206_configure_pci(&mb796_pci_config);

	stx5206_configure_usb();

	stx5206_configure_ethernet(stx5206_ethernet_rmii, 0, 0);

	return platform_add_devices(mb796_devices, ARRAY_SIZE(mb796_devices));
}
arch_initcall(mb796_device_init);

static void __iomem *mb796_ioport_map(unsigned long port, unsigned int size)
{
	/* Shouldn't be here! */
	BUG();
	return NULL;
}

struct sh_machine_vector mv_mb796 __initmv = {
	.mv_name = "mb796",
	.mv_setup = mb796_setup,
	.mv_nr_irqs = NR_IRQS,
	.mv_ioport_map = mb796_ioport_map,
	STM_PCI_IO_MACHINE_VEC
};

