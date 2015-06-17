/*
 * arch/sh/boards/st/mb839/setup.c
 *
 * Copyright (C) 2008 STMicroelectronics Limited
 * Author: Carl Shaw (carl.shaw@st.com)
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 * STMicroelectronics MB839 board support.
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
#include <linux/phy.h>
#include <linux/gpio_keys.h>
#include <linux/input.h>
#include <asm/irq-ilc.h>
#include <asm/irl.h>
#include <asm/io.h>
#include "../common/common.h"

/* smart card on PIO0, modem on PIO4, uart on PIO5 */
static int ascs[2] __initdata = { 2, 3 };

static void __init mb839_setup(char **cmdline_p)
{
	printk("STMicroelectronics MB839 initialisation\n");

	stx7105_early_device_init();
	stx7105_configure_asc(ascs, 2, 1);
}

static struct plat_stm_pwm_data pwm_private_info = {
	.flags = PLAT_STM_PWM_OUT0,
	.routing = PWM_OUT0_PIO13_0,
};

static struct plat_ssc_data ssc_private_info = {
	.capability =
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

static struct platform_device mb839_leds = {
	.name = "leds-gpio",
	.id = 0,
	.dev.platform_data = &(struct gpio_led_platform_data){
							      .num_leds = 2,
							      .leds =
							      (struct
							       gpio_led[]){
									   {
									    .
									    name
									    =
									    "LD1",
									    .
									    default_trigger
									    =
									    "heartbeat",
									    .
									    gpio
									    =
									    stpio_to_gpio
									    (2,
									     0),
									    .
									    active_low
									    = 1,
									    },
									   {
									    .
									    name
									    =
									    "LD2",
									    .
									    gpio
									    =
									    stpio_to_gpio
									    (2,
									     1),
									    .
									    active_low
									    = 1,
									    },
									   },
							      },
};

/*
 * On the MB839, the PHY is held in reset and needs enabling via PIO
 * pins.  Some e/net lines are also used as SoC mode pins and we need to
 * drive these high in order to get e/net to work.
 */
static struct stpio_pin *phy_reset, *phy_mode1, *phy_mode2, *phy_notintmode;

static int mb839_phy_reset(void *bus)
{
	stpio_set_pin(phy_notintmode, 1);
	stpio_set_pin(phy_mode1, 1);
	stpio_set_pin(phy_mode2, 1);

	stpio_set_pin(phy_reset, 1);
	udelay(1);
	stpio_set_pin(phy_reset, 0);
	udelay(100);
	stpio_set_pin(phy_reset, 1);
	udelay(1);

	return 0;
}

static struct plat_stmmacphy_data phy_private_data = {
	/* SMSC 8700 */
	.bus_id = 0,
	.phy_addr = -1,
	.phy_mask = 0,
	.interface = PHY_INTERFACE_MODE_MII,
	.phy_reset = &mb839_phy_reset,
};

static struct platform_device mb839_phy_device = {
	.name = "stmmacphy",
	.id = 0,
	.num_resources = 1,
	.resource = (struct resource[]){
					{
					 .name = "phyirq",
					 .start = -1,	/*FIXME, should be ILC_EXT_IRQ(6), */
					 .end = -1,
					 .flags = IORESOURCE_IRQ,
					 },
					},
	.dev = {
		.platform_data = &phy_private_data,
		}
};

static struct mtd_partition mtd_parts_table[] = {
	{
	 .name = "NOR FLASH",
	 .size = MTDPART_SIZ_FULL,
	 .offset = 0x00000000,
	 }
};

static struct physmap_flash_data physmap_flash_data = {
	.width = 2,
	.nr_parts = ARRAY_SIZE(mtd_parts_table),
	.parts = mtd_parts_table
};

static struct platform_device physmap_flash = {
	.name = "physmap-flash",
	.id = -1,
	.num_resources = 1,
	.resource = (struct resource[]){
					{
					 .start = 0x00000000,	/* Can be overridden */
					 .end = 128 * 1024 * 1024 - 1,
					 .flags = IORESOURCE_MEM,
					 }
					},
	.dev = {
		.platform_data = &physmap_flash_data,
		},
};

static struct platform_device *mb839_devices[] __initdata = {
	&mb839_leds,
	&mb839_phy_device,
	&physmap_flash,
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

	stx7105_configure_usb(0, &usb_init[0]);
	stx7105_configure_usb(1, &usb_init[1]);

	phy_notintmode = stpio_request_pin(7, 0, "PhyNotIntMode", STPIO_OUT);
	phy_mode1 = stpio_request_pin(7, 1, "PhyMode1Sel", STPIO_OUT);
	phy_mode2 = stpio_request_pin(7, 2, "PhyMode2Sel", STPIO_OUT);
	phy_reset = stpio_request_pin(7, 3, "ResetMII", STPIO_OUT);

	stx7105_configure_ethernet(0, stx7105_ethernet_mii, 1, 0, 0, 0);

	stx7105_configure_lirc(&lirc_scd);

	return platform_add_devices(mb839_devices, ARRAY_SIZE(mb839_devices));
}

arch_initcall(device_init);

static void __iomem *mb839_ioport_map(unsigned long port, unsigned int size)
{
	/* Shouldn't be here! */
	BUG();
	return NULL;
}

static void __init mb839_init_irq(void)
{
	/* Configure STEM interrupts as active low. */
	set_irq_type(ILC_EXT_IRQ(1), IRQ_TYPE_LEVEL_LOW);
	set_irq_type(ILC_EXT_IRQ(2), IRQ_TYPE_LEVEL_LOW);
}

struct sh_machine_vector mv_mb839 __initmv = {
	.mv_name = "mb839",
	.mv_setup = mb839_setup,
	.mv_nr_irqs = NR_IRQS,
	.mv_init_irq = mb839_init_irq,
	.mv_ioport_map = mb839_ioport_map,
};
