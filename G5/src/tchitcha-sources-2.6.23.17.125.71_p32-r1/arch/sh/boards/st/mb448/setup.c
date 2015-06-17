/*
 * arch/sh/boards/st/mb448/setup.c
 *
 * Copyright (C) 2005 STMicroelectronics Limited
 * Author: Stuart Menefy (stuart.menefy@st.com)
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 * STMicroelectronics STb7109E Reference board support.
 */

#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/stm/pio.h>
#include <linux/stm/soc.h>
#include <linux/delay.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/physmap.h>
#include <linux/mtd/partitions.h>
#include <linux/phy.h>
#include <asm/irl.h>

static struct stpio_pin *vpp_pio;

static int ascs[2] __initdata = { 2, 3 };

void __init mb448_setup(char** cmdline_p)
{
	printk("STMicroelectronics STb7109E Reference board initialisation\n");

	stx7100_early_device_init();
	stb7100_configure_asc(ascs, 2, 0);
}

static struct plat_ssc_data ssc_private_info = {
	.capability  =
		ssc0_has(SSC_I2C_CAPABILITY) |
		ssc1_has(SSC_SPI_CAPABILITY) |
		ssc2_has(SSC_I2C_CAPABILITY),
};

static struct resource smc91x_resources[] = {
	[0] = {
		.start	= 0xa2000300,
		.end	= 0xa2000300 + 0xff,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= IRL3_IRQ,
		.end	= IRL3_IRQ,
		.flags	= IORESOURCE_IRQ,
	},
};

static struct platform_device smc91x_device = {
	.name		= "smc91x",
	.id		= 0,
	.num_resources	= ARRAY_SIZE(smc91x_resources),
	.resource	= smc91x_resources,
};

static void set_vpp(struct map_info * info, int enable)
{
	stpio_set_pin(vpp_pio, enable);
}

static struct mtd_partition mtd_parts_table[3] = {
	{
		.name = "Boot firmware",
		.size = 0x00040000,
		.offset = 0x00000000,
	}, {
		.name = "Kernel",
		.size = 0x00100000,
		.offset = 0x00040000,
	}, {
		.name = "Root FS",
		.size = MTDPART_SIZ_FULL,
		.offset = 0x00140000,
	}
};

static struct physmap_flash_data physmap_flash_data = {
	.width		= 2,
	.set_vpp	= set_vpp,
	.nr_parts	= ARRAY_SIZE(mtd_parts_table),
	.parts		= mtd_parts_table
};

static struct resource physmap_flash_resource = {
	.start		= 0x00000000,
	.end		= 0x00800000 - 1,
	.flags		= IORESOURCE_MEM,
};

static struct platform_device physmap_flash = {
	.name		= "physmap-flash",
	.id		= -1,
	.dev		= {
		.platform_data	= &physmap_flash_data,
	},
	.num_resources	= 1,
	.resource	= &physmap_flash_resource,
};

static struct plat_stmmacphy_data phy_private_data = {
	.bus_id = 0,
	.phy_addr = 14,
	.phy_mask = 1,
	.interface = PHY_INTERFACE_MODE_MII,
	.phy_reset = NULL,
};

static struct platform_device mb448_phy_device = {
	.name		= "stmmacphy",
	.id		= 0,
	.num_resources	= 1,
	.resource	= (struct resource[]) {
                {
			.name	= "phyirq",
			.start	= IRL0_IRQ,
			.end	= IRL0_IRQ,
			.flags	= IORESOURCE_IRQ,
		},
	},
	.dev = {
		.platform_data = &phy_private_data,
	 }
};

static struct platform_device *mb448_devices[] __initdata = {
	&smc91x_device,
	&physmap_flash,
	&mb448_phy_device,
};

static int __init device_init(void)
{
	struct stpio_pin *smc91x_reset;

	stx7100_configure_sata();
	stx7100_configure_ssc(&ssc_private_info);
	stx7100_configure_usb();
	stx7100_configure_ethernet(0, 0, 0);

	vpp_pio = stpio_request_pin(2,7, "VPP", STPIO_OUT);

	/* Reset the SMSC 91C111 Ethernet chip */
	smc91x_reset = stpio_request_set_pin(2, 6, "smc91x_reset",
					     STPIO_OUT, 0);
	udelay(1);
	stpio_set_pin(smc91x_reset, 1);
	udelay(1);
	stpio_set_pin(smc91x_reset, 0);

	return platform_add_devices(mb448_devices,
				    ARRAY_SIZE(mb448_devices));
}

device_initcall(device_init);
