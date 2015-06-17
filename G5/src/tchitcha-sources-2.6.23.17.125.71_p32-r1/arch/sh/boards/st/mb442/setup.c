/*
 * arch/sh/boards/st/mb442/setup.c
 *
 * Copyright (C) 2005 STMicroelectronics Limited
 * Author: Stuart Menefy (stuart.menefy@st.com)
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 * STMicroelectronics STb7100 Reference board support.
 */

#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/stm/pio.h>
#include <linux/stm/soc.h>
#include <linux/spi/spi.h>
#include <linux/delay.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/physmap.h>
#include <linux/mtd/partitions.h>
#include <linux/bpa2.h>
#include <linux/phy.h>
#include <linux/lirc.h>
#include <asm/irl.h>

static int ascs[2] __initdata = { 2, 3 };

const char *LMI_VID_partalias[] = { "BPA2_Region1", "coredisplay-video", "gfx-memory", "v4l2-video-buffers", NULL };
const char *LMI_SYS_partalias[] = { "BPA2_Region0", "bigphysarea", "v4l2-coded-video-buffers", NULL };

#ifdef CONFIG_32BIT
static struct bpa2_partition_desc bpa2_parts_table[] = {
	{
		.name  = "LMI_VID",
		.start = 0x60800000,
		.size  = 0x07800000,
		.flags = 0,
		.aka   = LMI_VID_partalias
	},
	{
		.name  = "LMI_SYS",
		.start = 0,
		.size  = 0x05000000,
		.flags = 0,
		.aka   = LMI_SYS_partalias
	}
};
#else
static struct bpa2_partition_desc bpa2_parts_table[] = {
	{
		.name  = "LMI_VID",
		.start = 0x10800000,
		.size  = 0x03800000,
		.flags = 0,
		.aka   = LMI_VID_partalias
	},
	{
		.name  = "LMI_SYS",
		.start = 0,
		.size  = 0x05000000,
		.flags = 0,
		.aka   = LMI_SYS_partalias
	}
};
#endif /* CONFIG_32BIT */

void __init mb442_setup(char** cmdline_p)
{
	printk("STMicroelectronics STb7100 Reference board initialisation\n");

	stx7100_early_device_init();
	stb7100_configure_asc(ascs, 2, 0);
	bpa2_init(bpa2_parts_table, ARRAY_SIZE(bpa2_parts_table));
}

static struct plat_stm_pwm_data pwm_private_info = {
	.flags		= PLAT_STM_PWM_OUT1,
};

static struct plat_ssc_data ssc_private_info = {
	.capability  =
		ssc0_has(SSC_I2C_CAPABILITY | SSC_I2C_PIO) |
		ssc1_has(SSC_SPI_CAPABILITY) |
		ssc2_has(SSC_I2C_CAPABILITY | SSC_I2C_PIO),
};

static struct resource smc91x_resources[] = {
	[0] = {
		.start	= 0x02000300,
		.end	= 0x02000300 + 0xff,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= IRL0_IRQ,
		.end	= IRL0_IRQ,
		.flags	= IORESOURCE_IRQ,
	},
};

static struct platform_device smc91x_device = {
	.name		= "smc91x",
	.id		= 0,
	.num_resources	= ARRAY_SIZE(smc91x_resources),
	.resource	= smc91x_resources,
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

static struct stpio_pin *vpp_pio;
static void set_vpp(struct map_info *info, int enable)
{
	stpio_set_pin(vpp_pio, enable);
}

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

static struct stpio_pin *phy_reset_pin;

static int mb442_phy_reset(void* bus)
{
	stpio_set_pin(phy_reset_pin, 1);
	udelay(1);
	stpio_set_pin(phy_reset_pin, 0);
	udelay(1);
	stpio_set_pin(phy_reset_pin, 1);

	return 1;
}

static struct plat_stmmacphy_data phy_private_data = {
	.bus_id = 0,
	.phy_addr = 14,
	.phy_mask = 1,
	.interface = PHY_INTERFACE_MODE_MII,
	.phy_reset = &mb442_phy_reset,
};

static struct platform_device mb442_phy_device = {
	.name		= "stmmacphy",
	.id		= 0,
	.num_resources	= 1,
	.resource	= (struct resource[]) {
                {
			.name	= "phyirq",
			.start	= IRL3_IRQ,
			.end	= IRL3_IRQ,
			.flags	= IORESOURCE_IRQ,
		},
	},
	.dev = {
		.platform_data = &phy_private_data,
	 }
};

static struct platform_device *mb442_devices[] __initdata = {
	&smc91x_device,
	&physmap_flash,
	&mb442_phy_device,
};

/* Configuration based on Futarque-RC signals train. */
lirc_scd_t lirc_scd = {
	.code = 0x3FFFC028,
	.nomtime = 0x1f4,
	.noiserecov = 0,
};

static int __init device_init(void)
{
	struct stpio_pin *smc91x_reset;

	stx7100_configure_sata();
	stx7100_configure_pwm(&pwm_private_info);
	stx7100_configure_ssc(&ssc_private_info);
	stx7100_configure_usb();
	stx7100_configure_lirc(&lirc_scd);
	stx7100_configure_pata(3, 1, IRL1_IRQ);

	vpp_pio = stpio_request_set_pin(2, 7, "flash_VPP", STPIO_OUT, 0);

	phy_reset_pin = stpio_request_set_pin(2, 4, "ste100p_reset",
					      STPIO_OUT, 1);
	stx7100_configure_ethernet(0, 0, 0);

	/* Reset the SMSC 91C111 Ethernet chip */
	smc91x_reset = stpio_request_set_pin(2, 6, "smc91x_reset",
					     STPIO_OUT, 0);
	udelay(1);
	stpio_set_pin(smc91x_reset, 1);
	udelay(1);
	stpio_set_pin(smc91x_reset, 0);

	return platform_add_devices(mb442_devices,
				    ARRAY_SIZE(mb442_devices));
}

device_initcall(device_init);
