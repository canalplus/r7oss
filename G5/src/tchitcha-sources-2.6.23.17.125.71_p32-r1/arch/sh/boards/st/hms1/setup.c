/*
 * arch/sh/boards/st/hms1/setup.c
 *
 * Copyright (C) 2006 STMicroelectronics Limited
 * Author: Stuart Menefy (stuart.menefy@st.com)
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 * HMS1 board support.
 */

#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/stm/pio.h>
#include <linux/stm/soc.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/physmap.h>
#include <linux/mtd/partitions.h>
#include <asm/irl.h>
#include <linux/bpa2.h>
#include <linux/lirc.h>

static struct stpio_pin *vpp_pio;

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

void __init hms1_setup(char** cmdline_p)
{
	printk("HMS1 board initialisation\n");

	stx7100_early_device_init();
	stb7100_configure_asc(ascs, 2, 0);
	bpa2_init(bpa2_parts_table, ARRAY_SIZE(bpa2_parts_table));
}

static struct plat_stm_pwm_data pwm_private_info = {
	.flags		= PLAT_STM_PWM_OUT1,
};

static struct plat_ssc_data ssc_private_info = {
	.capability  =
		ssc0_has(SSC_I2C_CAPABILITY) |
		ssc1_has(SSC_I2C_CAPABILITY) |
		ssc2_has(SSC_I2C_CAPABILITY),
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

static struct platform_device smsc_lan9117 = {
	.name		= "smc911x",
	.id		= -1,
	.num_resources	= 4,
	.resource	= (struct resource []) {
		{
			.flags = IORESOURCE_MEM,
			.start = 0x01000000,
			.end   = 0x010000ff,
		},
		{
			.flags = IORESOURCE_IRQ,
			.start = IRL0_IRQ,
			.end   = IRL0_IRQ,
		},
		/* See end of "drivers/net/smsc_911x/smsc9118.c" file
		 * for description of two following resources. */
		{
			.flags = IORESOURCE_IRQ,
			.name  = "polarity",
			.start = 1,
			.end   = 1,
		},
		{
			.flags = IORESOURCE_IRQ,
			.name  = "type",
			.start = 1,
			.end   = 1,
		},
	},
};

static struct platform_device *hms1_devices[] __initdata = {
	&physmap_flash,
	&smsc_lan9117,
};

static int __init hms1_device_init(void)
{
	stx7100_configure_sata();
	stx7100_configure_pwm(&pwm_private_info);
	stx7100_configure_ssc(&ssc_private_info);
	stx7100_configure_usb();
	stx7100_configure_pata(3, 1, IRL1_IRQ);
	stx7100_configure_lirc(NULL);

	vpp_pio = stpio_request_pin(2,5, "VPP", STPIO_OUT);

	return platform_add_devices(hms1_devices, ARRAY_SIZE(hms1_devices));
}

arch_initcall(hms1_device_init);
