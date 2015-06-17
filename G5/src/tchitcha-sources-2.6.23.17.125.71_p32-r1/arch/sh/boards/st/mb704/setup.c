/*
 * arch/sh/boards/st/mb704/setup.c
 *
 * Copyright (C) 2008 STMicroelectronics Limited
 * Author: Stuart Menefy (stuart.menefy@st.com)
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 * STMicroelectronics STx5197 processor board support.
 */

#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/leds.h>
#include <linux/stm/pio.h>
#include <linux/stm/soc.h>
#include <linux/stm/emi.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/physmap.h>
#include <linux/mtd/partitions.h>
#include <linux/phy.h>
#include <linux/lirc.h>
#include <linux/spi/spi.h>
#include <linux/spi/eeprom.h>
#include <linux/io.h>
#include <asm/irq-ilc.h>
#include <asm/irl.h>
#include "../common/common.h"

static int ascs[2] __initdata = { 2, 3 };

static void __init mb704_setup(char **cmdline_p)
{
	printk(KERN_INFO "STMicroelectronics STx5197 Mboard initialisation\n");

	stx5197_early_device_init();
	stx5197_configure_asc(ascs, 2, 0);
}

static struct plat_stm_pwm_data pwm_private_info = {
	.flags		= PLAT_STM_PWM_OUT0,
};

/*
 * Configure SSC0 as SPI to drive serial Flash attached to SPI (requires
 * SSC driver SPI) or as I2C to drive I2C devices such as HDMI.
 *
 * To use I2C bus 0 (PIO1:1:0]) it is necessary to fit jumpers
 * J2-E, J2-G, J2-F and J2-H.
 *
 * I2C bus 1 can be routed to on chip QPSK block (setting routing to
 * SSC1_QPSK) or the pins QAM_SCLT/SDAT (setting SSC1_QAM_SCLT_SDAT).
 * Both require SSC driver.
 *
 * To use I2C bus 2 (PIO3[3:2]) it is necessary to remove jumpers
 * J7-A, J7-C, J6-H and J6-F and fit J7-B and J6-G.
 */
static struct plat_ssc_data ssc_private_info = {
	.capability =
		ssc0_has(SSC_SPI_CAPABILITY) |
		ssc1_has(SSC_I2C_CAPABILITY) |
		ssc2_has(SSC_I2C_CAPABILITY),
	.routing = SSC1_QPSK,
};

static struct platform_device mb704_leds = {
	.name = "leds-gpio",
	.id = -1,
	.dev.platform_data = &(struct gpio_led_platform_data) {
		.num_leds = 1,
		.leds = (struct gpio_led[]) {
			{
				.name = "HB",
				.default_trigger = "heartbeat",
				.gpio = stpio_to_gpio(3, 6),
				.active_low = 1,
			},
		},
	},
};

static struct plat_stmmacphy_data phy_private_data = {
	/* SMSC LAN 8700 on the mb762 */
	.bus_id = 0,
	.phy_addr = -1,
	.phy_mask = 0,
	.interface = PHY_INTERFACE_MODE_MII,
};

static struct platform_device mb704_phy_device = {
	.name		= "stmmacphy",
	.id		= 0,
	.num_resources	= 1,
	.resource	= (struct resource[]) {
		{
			.name	= "phyirq",
			.start	= -1,/* FIXME should be ILC_IRQ(25) */
			.end	= -1,
			.flags	= IORESOURCE_IRQ,
		},
	},
	.dev = {
		.platform_data = &phy_private_data,
	}
};

/* Note to use the mb704's SPI Flash device J10 needs to be in position 1-2. */

static struct spi_board_info mb704_spi_device = {
	.modalias       = "mtd_dataflash",
	.chip_select    = 0,
	.max_speed_hz   = 120000,
	.bus_num        = 0,
};

static struct platform_device *mb704_devices[] __initdata = {
	&mb704_leds,
	&mb704_phy_device,
};

/* Configuration based on Futarque-RC signals train. */
lirc_scd_t lirc_scd = {
	.code = 0x3FFFC028,
	.nomtime = 0x1f4,
	.noiserecov = 0,
};

static int __init device_init(void)
{
	/*
	 * By default we don't configure PWM as the mb704 has J2C fitted
	 * which resulsts in contention.
	 * stx5197_configure_pwm(&pwm_private_info);
	 */
	stx5197_configure_ssc(&ssc_private_info);
	stx5197_configure_usb();
	stx5197_configure_ethernet(0, 1, 0);
	stx5197_configure_lirc(&lirc_scd);

	spi_register_board_info(&mb704_spi_device, 1);

	return platform_add_devices(mb704_devices, ARRAY_SIZE(mb704_devices));
}
arch_initcall(device_init);

static void __iomem *mb704_ioport_map(unsigned long port, unsigned int size)
{
	/*
	 * However picking somewhere safe isn't as easy as you might
	 * think.  I used to use external ROM, but that can cause
	 * problems if you are in the middle of updating Flash. So I'm
	 * now using the processor core version register, which is
	 * guaranted to be available, and non-writable.
	 */
	return (void __iomem *)CCN_PVR;
}

static void __init mb704_init_irq(void)
{
}

struct sh_machine_vector mv_mb704 __initmv = {
	.mv_name		= "mb704",
	.mv_setup		= mb704_setup,
	.mv_nr_irqs		= NR_IRQS,
	.mv_init_irq		= mb704_init_irq,
	.mv_ioport_map		= mb704_ioport_map,
};
