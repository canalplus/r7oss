/*
 * arch/sh/boards/st/hdref/setup.c
 *
 * Copyright (C) 2005 STMicroelectronics Limited
 * Author: Stuart Menefy (stuart.menefy@st.com)
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 * STMicroelectronics HDref Reference board support.
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
#include <linux/phy_fixed.h>
#include <asm/irl.h>

static int ascs[2] __initdata = {
    2 | (STASC_FLAG_NORTSCTS << 8),
    3 | (STASC_FLAG_NORTSCTS << 8),
};

void __init hdref_setup(char** cmdline_p)
{
	printk("STMicroelectronics HDref Reference board initialisation\n");

	stx7100_early_device_init();
	stb7100_configure_asc(ascs, 2, 0);
}

static struct plat_ssc_data ssc_private_info = {
	.capability  =
		ssc0_has(SSC_I2C_CAPABILITY) |
		ssc1_has(SSC_SPI_CAPABILITY) |
		ssc2_has(SSC_I2C_CAPABILITY),
};

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
	.set_vpp	= NULL,
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
        .phy_addr = -1,
        .phy_mask = 0,
        .interface = PHY_INTERFACE_MODE_MII,
};

static struct fixed_phy_status fixed_phy_status = {
	.link = 1,
	.speed = 100,
};

static struct platform_device *hdref_devices[] __initdata = {
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
	stx7100_configure_sata();
	stx7100_configure_ssc(&ssc_private_info);
	stx7100_configure_usb();
	stx7100_configure_lirc(&lirc_scd);
	stx7100_configure_pata(3, 1, IRL1_IRQ);

	BUG_ON(fixed_phy_add(PHY_POLL, 1, &fixed_phy_status));
	stx7100_configure_ethernet(0, 0, STMMAC_PACK_BUS_ID(0, 1));

	return platform_add_devices(hdref_devices,
				    ARRAY_SIZE(hdref_devices));
}

device_initcall(device_init);
