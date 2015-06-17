/*
 * arch/sh/boards/st/mb671/setup.c
 *
 * Copyright (C) 2007 STMicroelectronics Limited
 * Author: Stuart Menefy (stuart.menefy@st.com)
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 * STMicroelectronics STx7200 Mboard support.
 */

#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/phy.h>
#include <linux/stm/pio.h>
#include <linux/stm/soc.h>
#include <linux/stm/emi.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/physmap.h>
#include <linux/mtd/partitions.h>
#include <linux/bpa2.h>
#include <asm/irq-ilc.h>
#include <asm/mb671/epld.h>
#include "../common/common.h"

static int ascs[2] __initdata = { 2 | (STASC_FLAG_NORTSCTS << 8), 3 };

const char *LMI_VID_partalias[] = { "BPA2_Region1", "coredisplay-video", "gfx-memory", "v4l2-video-buffers", NULL };
const char *LMI_SYS_partalias[] = { "BPA2_Region0", "bigphysarea", "v4l2-coded-video-buffers", NULL };

static struct bpa2_partition_desc bpa2_parts_table[] = {
	{
		.name  = "LMI_VID",
		.start = 0x81000000,
		.size  = 0x0f000000,
		.flags = 0,
		.aka   = LMI_VID_partalias
	},
	{
		.name  = "LMI_SYS",
		.start = 0,
		.size  = 0x0a000000,
		.flags = 0,
		.aka   = LMI_SYS_partalias
	}
};

static void __init mb671_setup(char **cmdline_p)
{
	printk(KERN_NOTICE "STMicroelectronics STx7200 Mboard "
			"initialisation\n");

	stx7200_early_device_init();
	stx7200_configure_asc(ascs, 2, 0);
	bpa2_init(bpa2_parts_table, ARRAY_SIZE(bpa2_parts_table));
}

static struct plat_stm_pwm_data pwm_private_info = {
	.flags		= PLAT_STM_PWM_OUT0,
};

static struct plat_ssc_data ssc_private_info = {
	.capability  = (
			ssc0_has(SSC_I2C_CAPABILITY | SSC_I2C_PIO) |
			ssc1_has(SSC_SPI_CAPABILITY) |
			ssc2_has(SSC_I2C_CAPABILITY | SSC_I2C_PIO) |
			ssc3_has(SSC_SPI_CAPABILITY) |
			ssc4_has(SSC_I2C_CAPABILITY | SSC_I2C_PIO)),
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

static void mtd_set_vpp(struct map_info *map, int vpp)
{
	/* Bit 0: VPP enable
	 * Bit 1: Reset (not used in later EPLD versions)
	 */

	if (vpp) {
		epld_write(3, EPLD_FLASH);
	} else {
		epld_write(2, EPLD_FLASH);
	}
}

static struct physmap_flash_data physmap_flash_data = {
	.width		= 2,
	.set_vpp	= mtd_set_vpp,
	.nr_parts	= ARRAY_SIZE(mtd_parts_table),
	.parts		= mtd_parts_table
};

static struct platform_device physmap_flash = {
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
		.platform_data	= &physmap_flash_data,
	},
};

static struct plat_stmmacphy_data phy_private_data[2] = {
	{
		/* MII0: SMSC LAN8700 */
		.bus_id = 0,
		.phy_addr = -1,
		.phy_mask = 0,
		.interface = PHY_INTERFACE_MODE_RMII,
	}, {
		/* MII1: MB539B connected to J2 */
		.bus_id = 1,
		.phy_addr = 0,
		.phy_mask = 0,
		.interface = PHY_INTERFACE_MODE_MII,
	}
};

static struct platform_device mb671_phy_devices[2] = {
	{
		.name		= "stmmacphy",
		.id		= 0,
		.num_resources	= 1,
		.resource	= (struct resource[]) {
			{
				.name	= "phyirq",
				/* This should be:
				 * .start = ILC_IRQ(93),
				 * .end = ILC_IRQ(93),
				 * but mode pins setup (MII0_RXD[3] pulled
				 * down) disables nINT pin of LAN8700, so
				 * we are unable to use it... */
				.start	= -1,
				.end	= -1,
				.flags	= IORESOURCE_IRQ,
			},
		},
		.dev = {
			.platform_data = &phy_private_data[0],
		}
	}, {
		.name		= "stmmacphy",
		.id		= 1,
		.num_resources	= 1,
		.resource	= (struct resource[]) {
			{
				.name	= "phyirq",
				.start	= ILC_IRQ(95),
				.end	= ILC_IRQ(95),
				.flags	= IORESOURCE_IRQ,
			},
		},
		.dev = {
			.platform_data = &phy_private_data[1],
		}
	}
};

static struct platform_device epld_device = {
	.name		= "epld",
	.id		= -1,
	.num_resources	= 1,
	.resource	= (struct resource[]) {
		{
			.start	= EPLD_BASE,
			.end	= EPLD_BASE + EPLD_SIZE - 1,
			.flags	= IORESOURCE_MEM,
		}
	},
	.dev.platform_data = &(struct plat_epld_data) {
		.opsize = 16,
	},
};

static struct platform_device *mb671_devices[] __initdata = {
	&epld_device,
	&physmap_flash,
	&mb671_phy_devices[0],
	&mb671_phy_devices[1],
};

static int __init device_init(void)
{
	unsigned int epld_rev;
	unsigned int pcb_rev;

	epld_rev = epld_read(EPLD_EPLDVER);
	pcb_rev = epld_read(EPLD_PCBVER);
	printk(KERN_NOTICE "mb671 PCB rev %X EPLD rev %dr%d\n",
			pcb_rev, epld_rev >> 4, epld_rev & 0xf);

	stx7200_configure_pwm(&pwm_private_info);
	stx7200_configure_ssc(&ssc_private_info);

	stx7200_configure_usb(0);
	stx7200_configure_usb(1);
	stx7200_configure_usb(2);

	stx7200_configure_sata(0);

#if 1 /* On-board PHY (MII0) in RMII mode, using MII_CLK */
	stx7200_configure_ethernet(0, 1, 0, 0);
#else /* External PHY board (MB539B) on MII1 in MII mode, using its own clock */
	stx7200_configure_ethernet(1, 0, 1, 1);
#endif

	stx7200_configure_lirc(NULL);

	return platform_add_devices(mb671_devices, ARRAY_SIZE(mb671_devices));
}
arch_initcall(device_init);

static void __iomem *mb671_ioport_map(unsigned long port, unsigned int size)
{
	/* However picking somewhere safe isn't as easy as you might think.
	 * I used to use external ROM, but that can cause problems if you are
	 * in the middle of updating Flash. So I'm now using the processor core
	 * version register, which is guaranted to be available, and non-writable.
	 */
	return (void __iomem *)CCN_PVR;
}

static void __init mb671_init_irq(void)
{
	epld_early_init(&epld_device);

#if defined(CONFIG_SH_ST_STEM)
	/* The off chip interrupts on the mb671 are a mess. The external
	 * EPLD priority encodes them, but because they pass through the ILC3
	 * there is no way to decode them.
	 *
	 * So here we bodge it as well. Only enable the STEM INTR0 signal,
	 * and hope nothing else goes active. This will result in
	 * SYS_ITRQ[3..0] = 0100.
	 *
	 * BTW. According to EPLD code author - "masking" interrupts
	 * means "enabling" them... Just to let you know... ;-)
	 */
	epld_write(0xff, EPLD_INTMASK0CLR);
	epld_write(0xff, EPLD_INTMASK1CLR);
	/* IntPriority(4) <= not STEM_notINTR0 */
	epld_write(1 << 4, EPLD_INTMASK0SET);
#endif
}

struct sh_machine_vector mv_mb671 __initmv = {
	.mv_name		= "mb671",
	.mv_setup		= mb671_setup,
	.mv_nr_irqs		= NR_IRQS,
	.mv_init_irq		= mb671_init_irq,
	.mv_ioport_map		= mb671_ioport_map,
};
