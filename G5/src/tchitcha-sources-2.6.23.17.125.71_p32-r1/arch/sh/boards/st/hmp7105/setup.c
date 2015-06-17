/*
 * arch/sh/boards/st/hmp7105/setup.c
 *
 * Copyright (C) 2009 STMicroelectronics Limited
 * Author: Chris Smith <chris.smith@st.com>
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 * STMicroelectronics HMP7105 support.
 */

#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/leds.h>
#include <linux/stm/pio.h>
#include <linux/stm/soc.h>
#include <linux/stm/emi.h>
#include <linux/delay.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/physmap.h>
#include <linux/mtd/partitions.h>
#include <linux/mtd/nand.h>
#include <linux/stm/nand.h>
#include <linux/stm/soc_init.h>
#include <linux/phy.h>
#include <linux/gpio_keys.h>
#include <linux/input.h>
#include <linux/io.h>
#include <asm/irq-ilc.h>
#include <asm/irl.h>
#include "../common/common.h"

static int ascs[2] __initdata = { 2, 3 };

static void __init hmp7105_setup(char **cmdline_p)
{
	printk(KERN_INFO "STMicroelectronics HMP7105 board initialisation\n");

	stx7105_early_device_init();
	stx7105_configure_asc(ascs, 2, 0);
}

static struct plat_stm_pwm_data pwm_private_info = {
	.flags		= PLAT_STM_PWM_OUT0,
	.routing	= PWM_OUT0_PIO13_0,
};

static struct plat_ssc_data ssc_private_info = {
	.capability  =
		ssc0_has(SSC_I2C_CAPABILITY) |
		ssc1_has(SSC_I2C_CAPABILITY) |
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

static struct platform_device hmp7105_leds = {
	.name = "leds-gpio",
	.id = 0,
	.dev.platform_data = &(struct gpio_led_platform_data) {
		.num_leds = 4,
		.leds = (struct gpio_led[]) {
			{
				.name = "AmberLED0",
				.gpio = stpio_to_gpio(16, 1),
			},
			{
				.name = "AmberLED1",
				.default_trigger = "heartbeat",
				.gpio = stpio_to_gpio(16, 2),
				.active_low = 1,
			},
			{
				.name = "RedLED",
				.default_trigger = "heartbeat",
				.gpio = stpio_to_gpio(16, 3),
				.active_low = 1,
			},
			{
				.name = "GreenLED",
				.default_trigger = "heartbeat",
				.gpio = stpio_to_gpio(16, 4),
				.active_low = 1,
			},
		},
	},
};

static struct stpio_pin *phy_reset;

static int hmp7105_phy_reset(void *bus)
{
    stpio_set_pin(phy_reset, 0);
    udelay(100);
    stpio_set_pin(phy_reset, 1);

	return 0;
}

static struct plat_stmmacphy_data phy_private_data = {
	/* SMSC 8700 */
	.bus_id = 0,
	.phy_addr = -1,
	.phy_mask = 0,
	.interface = PHY_INTERFACE_MODE_MII,
	.phy_reset = &hmp7105_phy_reset,
};

static struct platform_device hmp7105_phy_device = {
	.name		= "stmmacphy",
	.id		= 0,
	.num_resources	= 1,
	.resource	= (struct resource[]) {
		{
			.name	= "phyirq",
			.start	= -1,/*FIXME, should be ILC_EXT_IRQ(6), */
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

static struct physmap_flash_data hmp7105_physmap_flash_data = {
	.width		= 2,
	.set_vpp	= NULL,
	.nr_parts	= ARRAY_SIZE(mtd_parts_table),
	.parts		= mtd_parts_table
};

static struct platform_device hmp7105_physmap_flash = {
	.name		= "physmap-flash",
	.id		= -1,
	.num_resources	= 1,
	.resource	= (struct resource[]) {
		{
			.start		= 0x00000000,
			.end		= 32*1024*1024 - 1,
			.flags		= IORESOURCE_MEM,
		}
	},
	.dev		= {
		.platform_data	= &hmp7105_physmap_flash_data,
	},
};

/* NAND Device */
static struct mtd_partition nand_parts[] = {
	{
		.name	= "NAND root",
		.offset	= 0,
		.size 	= 0x00800000
	}, {
		.name	= "NAND home",
		.offset	= MTDPART_OFS_APPEND,
		.size	= MTDPART_SIZ_FULL
	},
};

static struct plat_stmnand_data nand_config = {
	/* STM_NAND_EMI data */
	.emi_withinbankoffset	= 0,
	.rbn_port		= -1,
	.rbn_pin		= -1,

	.timing_data = &(struct nand_timing_data) {
		.sig_setup	= 50,		/* times in ns */
		.sig_hold	= 50,
		.CE_deassert	= 0,
		.WE_to_RBn	= 100,
		.wr_on		= 10,
		.wr_off		= 40,
		.rd_on		= 10,
		.rd_off		= 40,
		.chip_delay	= 30,		/* in us */
	},
	.flex_rbn_connected	= 0,
};

/* Platform data for STM_NAND_EMI/FLEX/AFM. (bank# may be updated later) */
static struct platform_device nand_device =
	STM_NAND_DEVICE("stm-nand-emi", 1, &nand_config,
			nand_parts, ARRAY_SIZE(nand_parts), NAND_USE_FLASH_BBT);

static struct platform_device *hmp7105_devices[] __initdata = {
	&hmp7105_leds,
	&hmp7105_physmap_flash,
	&hmp7105_phy_device,
};

/* Configuration based on Futarque-RC signals train. */
lirc_scd_t lirc_scd = {
	.code = 0x3FFFC028,
	.nomtime = 0x1f4,
	.noiserecov = 0,
};

static int __init device_init(void)
{
	stx7105_configure_sata(0);
	stx7105_configure_pwm(&pwm_private_info);
	stx7105_configure_ssc(&ssc_private_info);

	/*
	 * Note that USB port configuration depends on jumper
	 * settings:
	 *
	 *	  PORT 0	       		PORT 1
	 *	+-----------------------------------------------------------
	 * norm	|  4[4]	J5A:2-3			 4[6]	J10A:2-3
	 * alt	| 12[5]	J5A:1-2  J6F:open	14[6]	J10A:1-2  J11G:open
	 * norm	|  4[5]	J5B:2-3			 4[7]	J10B:2-3
	 * alt	| 12[6]	J5B:1-2  J6G:open	14[7]	J10B:1-2  J11H:open
	 */

	stx7105_configure_usb(0, &usb_init[0]);
	stx7105_configure_usb(1, &usb_init[1]);

	phy_reset = stpio_request_pin(11, 0, "ResetMII", STPIO_OUT);
	stx7105_configure_ethernet(0, stx7105_ethernet_mii, 1, 0, 1, 0);

	stx7105_configure_lirc(&lirc_scd);

	stpio_request_set_pin(10, 7, "NANDEnable", STPIO_OUT, 0);
	stx7105_configure_nand(&nand_device);

	return platform_add_devices(hmp7105_devices,
						ARRAY_SIZE(hmp7105_devices));
}
arch_initcall(device_init);


static void __iomem *hmp7105_ioport_map(unsigned long port, unsigned int size)
{
	/* Shouldn't be here! */
	BUG();
	return NULL;
}

struct sh_machine_vector mv_hmp7105 __initmv = {
	.mv_name = "hmp7105",
	.mv_setup = hmp7105_setup,
	.mv_nr_irqs = NR_IRQS,
	.mv_ioport_map = hmp7105_ioport_map,
};
