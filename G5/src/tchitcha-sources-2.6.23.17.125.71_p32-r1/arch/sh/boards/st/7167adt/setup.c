/*
 * arch/sh/boards/st/7167adt/setup.c
 *
 * Copyright (C) 2010 Jean-Christophe PLAGNIOL-VILLARD <plagnioj@jcrosoft.com>
 *
 * Copyright (C) 2008 STMicroelectronics Limited
 * Author: Stuart Menefy (stuart.menefy@st.com)
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 * STMicroelectronics STi7162/7 ADT board support.
 */

#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/stm/pio.h>
#include <linux/stm/soc.h>
#include <linux/stm/emi.h>
#include <linux/stm/sysconf.h>
#include <linux/delay.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/physmap.h>
#include <linux/mtd/partitions.h>
#include <linux/mtd/nand.h>
#include <linux/stm/nand.h>
#include <linux/spi/spi.h>
#include <linux/spi/flash.h>
#include <linux/stm/soc_init.h>
#include <linux/phy.h>
#include <linux/gpio.h>
#include <linux/input.h>
#include <linux/io.h>
#include <asm/irq-ilc.h>
#include <asm/irl.h>
#include "../common/common.h"

#define ADT7167_GPIO_FLASH_WP		stpio_to_gpio(13, 6)
#define ADT7167_GPIO_POWER_ON_ETH	stpio_to_gpio(15, 5)
#define ADT7167_GPIO_SPI_BOOT_CLK	stpio_to_gpio(15, 0)
#define ADT7167_GPIO_SPI_BOOT_DOUT	stpio_to_gpio(15, 1)
#define ADT7167_GPIO_SPI_BOOT_NOTCS	stpio_to_gpio(15, 2)
#define ADT7167_GPIO_SPI_BOOT_DIN	stpio_to_gpio(15, 3)

static int ascs[] __initdata = { 2 };

static void __init adt7167_setup(char **cmdline_p)
{
	printk(KERN_INFO "STMicroelectronics STx7167 ADT board initialisation\n");

	stx7105_early_device_init();
	stx7105_configure_asc(ascs, ARRAY_SIZE(ascs), 0);
}

static struct plat_ssc_data ssc_private_info = {
	.capability =
		ssc0_has(SSC_I2C_CAPABILITY) |
		ssc1_has(SSC_SPI_CAPABILITY) |
		ssc2_has(SSC_I2C_CAPABILITY) |
		ssc3_has(SSC_I2C_CAPABILITY),
	.routing =
		SSC2_SCLK_PIO3_4 | SSC2_MTSR_PIO3_5 |
		SSC3_SCLK_PIO3_6 | SSC3_MTSR_PIO3_7,
};

static struct usb_init_data usb_init[2] __initdata = {
	{
		.oc_en = 1,
		.oc_actlow = 1,
		.oc_pinsel = USB0_OC_PIO4_4,
		.pwr_en = 1,
		.pwr_pinsel = USB0_PWR_PIO4_5,
	}, {
		.oc_en = 1,
		.oc_actlow = 1,
		.oc_pinsel = USB1_OC_PIO4_6,
		.pwr_en = 1,
		.pwr_pinsel = USB1_PWR_PIO4_7,
	}
};

static int adt7167_phy_reset(void *bus)
{
	gpio_set_value(ADT7167_GPIO_POWER_ON_ETH, 0);
	udelay(100);
	gpio_set_value(ADT7167_GPIO_POWER_ON_ETH, 1);
	udelay(1);

	return 1;
}

static struct plat_stmmacphy_data phy_private_data = {
	/* Micrel KSZ8041FTL */
	.bus_id = 0,
	.phy_addr = -1,
	.phy_mask = 0,
	.interface = PHY_INTERFACE_MODE_MII,
	.phy_reset = &adt7167_phy_reset,
};

static struct platform_device adt7167_phy_device = {
	.name		= "stmmacphy",
	.id		= 0,
	.num_resources	= 1,
	.resource	= (struct resource[]) {
		{
			.name	= "phyirq",
			.start	= -1, /* FIXME, should be ILC_EXT_IRQ(6) */
			.end	= -1,
			.flags	= IORESOURCE_IRQ,
		},
	},
	.dev = {
		.platform_data = &phy_private_data,
	}
};

static struct mtd_partition mtd_parts_table[3] = {
	{
		.name = "Boot firmware",
		.size = 0x00040000,
		.offset = 0x00000000,
	}, {
		.name = "Kernel",
		.size = 0x00200000,
		.offset = 0x00040000,
	}, {
		.name = "Root FS",
		.size = MTDPART_SIZ_FULL,
		.offset = 0x00240000,
	}
};

static struct physmap_flash_data adt7167_physmap_flash_data = {
	.width		= 2,
	.set_vpp	= NULL,
	.nr_parts	= ARRAY_SIZE(mtd_parts_table),
	.parts		= mtd_parts_table
};

static struct platform_device adt7167_physmap_flash = {
	.name		= "physmap-flash",
	.id		= -1,
	.num_resources	= 1,
	.resource	= (struct resource[]) {
		{
			.start		= 0x00000000,
			.end		= 32 * 1024 * 1024 - 1,
			.flags		= IORESOURCE_MEM,
		}
	},
	.dev		= {
		.platform_data	= &adt7167_physmap_flash_data,
	},
};

/* Configuration for Serial Flash */
static struct mtd_partition serialflash_partitions[] = {
	{
		.name = "SFLASH_1",
		.size = 0x00080000,
		.offset = 0,
	}, {
		.name = "SFLASH_2",
		.size = MTDPART_SIZ_FULL,
		.offset = MTDPART_OFS_NXTBLK,
	},
};

static struct flash_platform_data serialflash_data = {
	.name = "m25p80",
	.nr_parts = ARRAY_SIZE(serialflash_partitions),
	.parts = serialflash_partitions,
	.type = "m25p32",
};

static struct spi_board_info spi_serialflash[] = {
	{
		.modalias	= "m25p80",
		.bus_num	= 0,
		.chip_select	= spi_set_cs(2, 4),
		.max_speed_hz	= 5000000,
		.platform_data	= &serialflash_data,
		.mode		= SPI_MODE_3,
	},
};

static struct platform_device *adt7167_devices[] __initdata = {
	&adt7167_physmap_flash,
	&adt7167_phy_device,
};

/* Configuration based on Futarque-RC signals train. */
lirc_scd_t lirc_scd = {
	.code = 0x3FFFC028,
	.nomtime = 0x1f4,
	.noiserecov = 0,
};

static int __init device_init(void)
{
	/* Configure the SPI Boot as input */
	if (gpio_request(ADT7167_GPIO_SPI_BOOT_CLK, "SPI Boot CLK") == 0)
		gpio_direction_input(ADT7167_GPIO_SPI_BOOT_CLK);
	else
		printk(KERN_ERR "7167adt: Failed to claim SPI Boot CLK!\n");

	if (gpio_request(ADT7167_GPIO_SPI_BOOT_DOUT, "SPI Boot DOUT") == 0)
		gpio_direction_input(ADT7167_GPIO_SPI_BOOT_DOUT);
	else
		printk(KERN_ERR "7167adt: Failed to claim SPI Boot DOUT!\n");

	if (gpio_request(ADT7167_GPIO_SPI_BOOT_NOTCS, "SPI Boot NOTCS") == 0)
		gpio_direction_input(ADT7167_GPIO_SPI_BOOT_NOTCS);
	else
		printk(KERN_ERR "7167adt: Failed to claim SPI Boot NOTCS!\n");

	if (gpio_request(ADT7167_GPIO_SPI_BOOT_DIN, "SPI Boot DIN") == 0)
		gpio_direction_input(ADT7167_GPIO_SPI_BOOT_DIN);
	else
		printk(KERN_ERR "7167adt: Failed to claim SPI Boot DIN!\n");

	stx7105_configure_ssc(&ssc_private_info);

	stx7105_configure_usb(0, &usb_init[0]);
	/* USB1 on JD7 Connector */
	stx7105_configure_usb(1, &usb_init[1]);

	if (gpio_request(ADT7167_GPIO_POWER_ON_ETH, "POWER_ON_ETH") == 0)
		gpio_direction_output(ADT7167_GPIO_POWER_ON_ETH, 1);
	else
		printk(KERN_ERR "7167adt: Failed to claim POWER_ON_ETH "
				"PIO!\n");
	stx7105_configure_ethernet(0, stx7105_ethernet_mii, 0, 0, 0, 0);

	stx7105_configure_lirc(&lirc_scd);

	stx7105_configure_audio_pins(1, 0, 0, 0);

	/* just permanetly enable the flash*/
	if (gpio_request(ADT7167_GPIO_FLASH_WP, "FLASH_WP") == 0)
		gpio_direction_output(ADT7167_GPIO_FLASH_WP, 1);
	else
		printk(KERN_ERR "7167adt: Failed to claim FLASH_WP PIO!\n");

	spi_register_board_info(spi_serialflash, ARRAY_SIZE(spi_serialflash));

	return platform_add_devices(adt7167_devices,
				    ARRAY_SIZE(adt7167_devices));
}
arch_initcall(device_init);

static void __iomem *adt7167_ioport_map(unsigned long port,
					unsigned int size)
{
	/* Shouldn't be here! */
	BUG();
	return NULL;
}

struct sh_machine_vector mv_adt7167 __initmv = {
	.mv_name = "7167adt",
	.mv_setup = adt7167_setup,
	.mv_nr_irqs = NR_IRQS,
	.mv_ioport_map = adt7167_ioport_map,
	STM_PCI_IO_MACHINE_VEC
};
