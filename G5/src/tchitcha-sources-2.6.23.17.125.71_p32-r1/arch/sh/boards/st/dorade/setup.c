/*
 * arch/sh/boards/st/dorade/setup.c
 *
 * Copyright (C) 2008 STMicroelectronics Limited
 * Author: Stuart Menefy (stuart.menefy@st.com)
 *
 * Copyright (C) 2009 WyPlay SAS.
 * frederic mazuel <fmazuel@wyplay.com>
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 * dorade support.
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
#include <linux/bpa2.h>
#include <sound/stm.h>
#include <linux/phy.h>
#include <linux/gpio_keys.h>
#include <linux/input.h>
#include <asm/irq-ilc.h>
#include <asm/irl.h>
#include <asm/io.h>
#include "../common/common.h"

static int ascs[3] __initdata = {
	1 | (STASC_FLAG_NORTSCTS << 8),
	2 | (STASC_FLAG_NORTSCTS << 8),
	3 | (STASC_FLAG_NORTSCTS << 8)
};

const char *player_buffers_512[] = {
	"BPA2_Region0",
	"BPA2_Region1",
	"v4l2-video-buffers",
	NULL
};

const char *bigphys_512[] = {
	"bigphysarea",
	"v4l2-coded-video-buffers",
	"coredisplay-video",
	NULL
};

static struct bpa2_partition_desc bpa2_parts_table_512[] = {
	{
		.name	= "Player_Buffers",	/* 96 + 3MiB */
		.start	= 0,
		.size	= 0x07000000 + 3 * 1024 * 1024,
		.flags	= 0,
		.aka	= player_buffers_512
	},
	{
		.name	= "Bigphys_Buffers",	/* 64MiB */
		.start	= 0,
		.size	= 0x04000000,
		.flags	= 0,
		.aka	= bigphys_512
	},
	{
		.name	= "gfx-memory-0",	/* 64MiB */
		.start = 0x58000000,
		.size	= 0x04000000,
		.flags	= 0,
		.aka	= NULL
	},
	{
		.name	= "gfx-memory-1",	/* 64MiB */
		.start	= 0x5c000000,
		.size	= 0x04000000,
		.flags	= 0,
		.aka	= NULL
	}
};

const char *LMI_SYS_partalias_256[] = {
	"BPA2_Region0",
	"v4l2-coded-video-buffers",
	"BPA2_Region1",
	"v4l2-video-buffers",
	"coredisplay-video",
	NULL
};

const char *bigphys_partalias_256[] = {
	"gfx-memory-1",
	NULL
};

static struct bpa2_partition_desc bpa2_parts_table_256[] = {
	{
		.name	= "LMI_SYS",		/* 85MiB */
		.start	= 0x45800000,
		.size	= 0x05500000,
		.flags	= 0,
		.aka	= LMI_SYS_partalias_256
	},
	{
		.name	= "bigphysarea",	/* 18MiB */
		.start	= 0x4ad00000,
		.size	= 0x01200000,
		.flags	= 0,
		.aka	= bigphys_partalias_256
	},
	{
		.name	= "gfx-memory-0",	/* 48MiB */
		.start	= 0x4c000000,
		.size	= 0x03000000,
		.flags	= 0,
		.aka	= NULL
	}
};

static __init void dorade_setup(char** cmdline_p)
{
	printk("DORADE board initialisation\n");

	stx7105_early_device_init();
	stx7105_configure_asc(ascs, 3, 0);

	printk("Use BPA2 partitions table for ");
	switch (memory_end - memory_start) {
	case 0x10000000:
		printk("256MiB DDR2\n");
		bpa2_init(bpa2_parts_table_256,
			  ARRAY_SIZE(bpa2_parts_table_256));
		break;
	case 0x20000000:
		printk("512MiB DDR2\n");
		bpa2_init(bpa2_parts_table_512,
			  ARRAY_SIZE(bpa2_parts_table_512));
		break;
	}
}

static struct plat_ssc_data ssc_private_info = {
	.capability =
		ssc0_has(SSC_I2C_CAPABILITY | SSC_I2C_PIO) |
		ssc1_has(SSC_I2C_CAPABILITY | SSC_I2C_PIO) |
		ssc2_has(SSC_I2C_CAPABILITY | SSC_I2C_PIO) |
		ssc3_has(SSC_I2C_CAPABILITY | SSC_I2C_PIO),
	.routing =
	SSC2_SCLK_PIO3_4 | SSC2_MTSR_PIO12_1 | SSC2_MRST_PIO3_5 |
	SSC3_SCLK_PIO13_2 | SSC3_MTSR_PIO13_3 | SSC3_MRST_PIO3_7,
};

static struct usb_init_data usb_init[2] __initdata = {
	{
		.oc_en = 0,
		.pwr_en = 0,
	}, {
		.oc_en = 0,
		.pwr_en = 0,
	}
};

static struct platform_device dorade_leds = {
	.name = "leds-gpio",
	.id = 0,
	.dev.platform_data = &(struct gpio_led_platform_data) {
		.num_leds = 4,
		.leds = (struct gpio_led[]) {
			{
				.name = "LED0",
				.gpio = stpio_to_gpio(11, 2),
			},
			{
				.name = "LED1",
				.gpio = stpio_to_gpio(11, 3),
			},
			{
				.name = "LED2",
				.gpio = stpio_to_gpio(11, 4),
			},
			{
				.name = "LED3",
				.gpio = stpio_to_gpio(11, 5),
			},
		},
	},
};

static struct plat_stmmacphy_data phy_private_data = {
	/* Marvell */
	.bus_id = 0,
	.phy_addr = 0x10,
	.phy_mask = 0,
	.interface = PHY_INTERFACE_MODE_MII,
};

static struct platform_device dorade_phy_device = {
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

static struct platform_device spi_pio_device[] = {
	{
		.name           = "spi_st_pio",
		.id             = 8,
		.num_resources  = 0,
		.dev            = {
			.platform_data =
			&(struct ssc_pio_t) {
				.pio = {{15, 0}, {15, 1}, {15, 3} },
			},
		},
	},
};

static struct platform_device *dorade_devices[] __initdata = {
	&dorade_leds,
	&dorade_phy_device,
	&spi_pio_device[0],
};

static unsigned lirc_enabled_pios[] = {
	[0] = 3 * 8 + 0
};

/* Configuration based on Futarque-RC signals train. */
static lirc_scd_t lirc_scd = {
        .code = 0x3FFFC028,
        .nomtime = 0x1f4,
        .noiserecov = 0,
};

static int __init device_init(void)
{
	struct stpio_pin *pio_ptr = NULL;

	stx7105_configure_sata(0);
	stx7105_configure_ssc(&ssc_private_info);

	/* Version control */
	pio_ptr = stpio_request_pin(4, 2, "VER1", STPIO_IN);
	stpio_free_pin(pio_ptr);
	pio_ptr = stpio_request_pin(4, 3, "VER2", STPIO_IN);
	stpio_free_pin(pio_ptr);
	pio_ptr = stpio_request_pin(4, 4, "VER3", STPIO_IN);
	stpio_free_pin(pio_ptr);

	stx7105_configure_usb(0, &usb_init[0]);
	stx7105_configure_usb(1, &usb_init[1]);

	/* FLASH_WP is shared by NOR and NAND.  However, since MTD NAND has no
	   concept of WP/VPP, we must permanently enable it*/
	stpio_request_set_pin(6, 4, "FLASH_WP", STPIO_OUT, 1);

	/*
	stx7105_configure_lirc(&lirc_scd, lirc_enabled_pios,
			       ARRAY_SIZE(lirc_enabled_pios));
	*/
	stpio_request_set_pin(15, 5, "eth_phy_reset",
			      STPIO_OUT, 1);
	stx7105_configure_ethernet(0, 0, 0, 0, 1, 0);

	//stx7105_configure_tsin(2, 1);
	//stx7105_configure_tsin(3, 1);

	return platform_add_devices(dorade_devices, ARRAY_SIZE(dorade_devices));
}
arch_initcall(device_init);

static void __iomem *dorade_ioport_map(unsigned long port, unsigned int size)
{
	/* However picking somewhere safe isn't as easy as you might think.
	 * I used to use external ROM, but that can cause problems if you are
	 * in the middle of updating Flash. So I'm now using the processor core
	 * version register, which is guaranted to be available, and non-writable.
	 */
	return (void __iomem *)CCN_PVR;
}

static void __init dorade_init_irq(void)
{
	/* Configure STEM interrupts as active low. */
	set_irq_type(ILC_EXT_IRQ(1), IRQ_TYPE_LEVEL_LOW);
	set_irq_type(ILC_EXT_IRQ(2), IRQ_TYPE_LEVEL_LOW);
}

struct sh_machine_vector mv_dorade __initmv = {
	.mv_name		= "dorade",
	.mv_setup		= dorade_setup,
	.mv_nr_irqs		= NR_IRQS,
	.mv_init_irq		= dorade_init_irq,
	.mv_ioport_map		= dorade_ioport_map,
};
