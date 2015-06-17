/*
 * arch/sh/boards/st/5197cab/setup.c
 *
 * Copyright (C) 2008 STMicroelectronics Limited
 * Author: Stuart Menefy (stuart.menefy@st.com)
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 * STMicroelectronics 5197cab reference board support.
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
#include <linux/delay.h>
#include <linux/lirc.h>
#include <linux/spi/spi.h>
#include <linux/spi/eeprom.h>
#include <linux/io.h>
#include <asm/irq-ilc.h>
#include <asm/irl.h>
#include "../common/common.h"

static int ascs[1] __initdata = { 3 };

static void __init cab5197_setup(char **cmdline_p)
{
	printk(KERN_INFO
		"STMicroelectronics cab5197 reference board initialisation\n");

	stx5197_early_device_init();
	stx5197_configure_asc(ascs, 1, 0);
}

static struct plat_stm_pwm_data pwm_private_info = {
	.flags		= PLAT_STM_PWM_OUT0,
};

/*
 * Configure SSC0 as SPI to drive serial Flash attached to SPI (requires
 * SSC driver SPI) or as I2C to drive I2C devices such as HDMI.
 *
 */

static struct plat_ssc_data ssc_private_info = {
	.capability =
		ssc0_has(SSC_SPI_CAPABILITY) |
		ssc1_has(SSC_I2C_CAPABILITY) |
		ssc2_has(SSC_I2C_CAPABILITY),
	.routing = SSC1_QPSK,
};

static struct stpio_pin *phy_reset_pin;

static int cab_phy_reset(void *bus)
{
	phy_reset_pin = stpio_request_set_pin
		(1, 4, "phy_reset",  STPIO_OUT, 0);
	udelay(20);
	stpio_set_pin(phy_reset_pin, 1);
	return 1;
}

static struct plat_stmmacphy_data phy_private_data = {
	.bus_id = 0,
	.phy_addr = -1,
	.phy_mask = 0,
	.interface = PHY_INTERFACE_MODE_MII,
#if 1   /*
	 * Note only enable the phy reset if R526 is fitted on your board
	 * otherwise disable this #if and rely upon power on reset only
	 */
	.phy_reset = cab_phy_reset,
#endif
};

static struct platform_device cab5197_phy_device = {
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

/* Note to use the cab5197's SPI Flash device J10 needs to be in
 * position 1-2.
 */

static struct spi_board_info cab5197_spi_device = {
	.modalias       = "mtd_dataflash",
	.chip_select    = 0,
	.max_speed_hz   = 120000,
	.bus_num        = 0,
};

static struct platform_device *cab5197_devices[] __initdata = {
	&cab5197_phy_device,
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
	 * By default we don't configure PWM as the cab5197 has J2C fitted
	 * which resulsts in contention.
	 * stx5197_configure_pwm(&pwm_private_info);
	 */
	stx5197_configure_ssc(&ssc_private_info);
	stx5197_configure_usb();
	stx5197_configure_ethernet(0, 1, 0);
	stx5197_configure_lirc(&lirc_scd);

	spi_register_board_info(&cab5197_spi_device, 1);

	return platform_add_devices(cab5197_devices,
				ARRAY_SIZE(cab5197_devices));
}
arch_initcall(device_init);

static void __iomem *cab5197_ioport_map(unsigned long port, unsigned int size)
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

static void __init cab5197_init_irq(void)
{
}

struct sh_machine_vector mv_cab5197 __initmv = {
	.mv_name		= "5197cab",
	.mv_setup		= cab5197_setup,
	.mv_nr_irqs		= NR_IRQS,
	.mv_init_irq		= cab5197_init_irq,
	.mv_ioport_map		= cab5197_ioport_map,
};
