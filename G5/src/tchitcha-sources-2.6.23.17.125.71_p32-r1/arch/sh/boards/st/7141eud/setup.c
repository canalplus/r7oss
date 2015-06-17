/*
 * arch/sh/boards/st/7141eud/setup.c
 *
 * Copyright (C) 2009 STMicroelectronics Limited
 * Author: Stuart Menefy (stuart.menefy@st.com)
 * Author: Mose' Pernice <mose.pernice@st.com>
 * Author: Giuseppe Cavallaro <peppe.cavallaro@st.com>
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 * STMicroelectronics 7141EUD board support.
 * Note: this setup file comes from the 7141 Mboard (mb628) but,
 * obviously, it's been changed according to the new platform.
 */

#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/leds.h>
#include <linux/stm/pio.h>
#include <linux/stm/soc.h>
#include <linux/workqueue.h>
#include <linux/stm/emi.h>
#include <linux/stm/sysconf.h>
#include <linux/spi/spi.h>
#include <linux/spi/spi_bitbang.h>
#include <linux/spi/flash.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/physmap.h>
#include <linux/mtd/partitions.h>
#include <linux/bpa2.h>
#include <linux/phy.h>
#include <linux/lirc.h>
#include <linux/gpio_keys.h>
#include <linux/input.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <asm/irq-ilc.h>
#include <asm/irl.h>
#include "../common/common.h"

const char *LMI_VID_partalias[] = { "BPA2_Region1", "coredisplay-video", "gfx-memory", "v4l2-video-buffers", NULL };
const char *LMI_SYS_partalias[] = { "BPA2_Region0", "bigphysarea", "v4l2-coded-video-buffers", NULL };

static struct bpa2_partition_desc bpa2_parts_table[] = {
	{
		.name  = "LMI_VID",
		.start = 0x80800000,
		.size  = 0x07800000,
		.flags = 0,
		.aka   = LMI_VID_partalias
	},
	{
		.name  = "LMI_SYS",
		.start = 0x44000000,
		.size  = 0x04000000,
		.flags = 0,
		.aka   = LMI_SYS_partalias
	}
};


#ifdef CONFIG_STMMAC_DUAL_MAC
#define ENABLE_GMAC0
#endif

static int ascs[] __initdata = {(2 | (STASC_FLAG_NORTSCTS << 8))};
static void __init eud7141_setup(char **cmdline_p)
{

	printk(KERN_INFO "STMicroelectronics 7141EUD initialisation\n");

	stx7141_early_device_init();
	stx7141_configure_asc(ascs, ARRAY_SIZE(ascs), 0);
	bpa2_init(bpa2_parts_table, ARRAY_SIZE(bpa2_parts_table));
}

static struct plat_ssc_data ssc_private_info = {
	.capability  =
		ssc0_has(SSC_UNCONFIGURED)	/* SSC1 */	|
		ssc1_has(SSC_UNCONFIGURED)	/* SSC2 */	|
		ssc2_has(SSC_I2C_CAPABILITY)	/* SSC3 */	|
		ssc3_has(SSC_I2C_CAPABILITY)	/* SSC4 */	|
		ssc4_has(SSC_I2C_CAPABILITY)	/* SSC5 */	|
		ssc5_has(SSC_I2C_CAPABILITY)	/* SSC6 */	|
		ssc6_has(SSC_I2C_CAPABILITY),	/* SSC7 */
};

static struct mtd_partition mtd_parts_table[3] = {
	{
		.name = "Boot firmware",
		.size = 0x00060000,
		.offset = 0x00000000,
	}, {
		.name = "Kernel",
		.size = 0x00300000,
		.offset = 0x00060000,
	}, {
		.name = "Root FS",
		.size = MTDPART_SIZ_FULL,
		.offset = 0x00360000,
	}
};

static struct physmap_flash_data physmap_flash_data = {
	.width		= 2,
	.nr_parts	= ARRAY_SIZE(mtd_parts_table),
	.parts		= mtd_parts_table
};

static struct platform_device eud7141_physmap_flash = {
	.name		= "physmap-flash",
	.id		= -1,
	.num_resources	= 1,
	.resource	= (struct resource[]) {
		{
			.start		= 0x00000000,
			.end		= 64*1024*1024 - 1,
			.flags		= IORESOURCE_MEM,
		}
	},
	.dev		= {
		.platform_data	= &physmap_flash_data,
	},
};

static struct mtd_partition nand_partitions[] = {
	{
		.name	= "NAND root",
		.offset	= 0,
		.size 	= 0x00800000
	}, {
		.name	= "NAND home",
		.offset	=  0x00800000,
		.size	= MTDPART_SIZ_FULL
	},
};

static struct plat_stmnand_data eud7141_nand_config = {
	.emi_bank		= 2, /* boot from NOR default */
	.emi_withinbankoffset	= 0,

	/* Timings for NAND512W3A */
	.emi_timing_data = &(struct emi_timing_data) {
		.rd_cycle_time	 = 40,		 /* times in ns */
		.rd_oee_start	 = 0,
		.rd_oee_end	 = 10,
		.rd_latchpoint	 = 10,
		.busreleasetime  = 0,

		.wr_cycle_time	 = 40,
		.wr_oee_start	 = 0,
		.wr_oee_end	 = 10,

		.wait_active_low = 0,
	},

	.chip_delay		= 40,		/* time in us */
	.mtd_parts		= nand_partitions,
	.nr_parts		= ARRAY_SIZE(nand_partitions),
};

static int eud7141_phy_reset(void *bus)
{
	static struct stpio_pin *phy_reset;
	static int first = 1;

	/* Both PHYs share the same reset signal, only act on the first. */
	if (!first)
		return 1;
	first = 0;

	printk(KERN_DEBUG "Resetting PHY\n");

	phy_reset = stpio_request_pin(5, 3, "ResetMII", STPIO_OUT);
	stpio_set_pin(phy_reset, 1);

	udelay(10);

	stpio_set_pin(phy_reset, 0);
	mdelay(50);
	stpio_set_pin(phy_reset, 1);
	udelay(10);

	return 1;
}

static struct plat_stmmacphy_data phy_private_data[2] = {
{
	.bus_id = 0,
	.phy_addr = -1,
	.phy_mask = 0,
	.interface = PHY_INTERFACE_MODE_MII,
	.phy_reset = eud7141_phy_reset,
}, {
	/* Default MAC connected to the IC+ IP1001 PHY driver */
	.bus_id = 1,
	.phy_addr = -1,
	.phy_mask = 0,
	.interface = PHY_INTERFACE_MODE_GMII,
	.phy_reset = eud7141_phy_reset,
} };

static struct platform_device eud7141_phy_devices[2] = {
{
	.name		= "stmmacphy",
	.id		= 0,
	.num_resources	= 1,
	.resource	= (struct resource[]) {
		{
			.name	= "phyirq",
			.start	= -1,  /* ILC_IRQ(43) FIXME */
			.end	= -1,  /* ILC_IRQ(43) FIXME */
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
			.start	= -1,/* ILC_IRQ(42) but MODE pin clash*/
			.end	= -1,
			.flags	= IORESOURCE_IRQ,
		},
	},
	.dev = {
		.platform_data = &phy_private_data[1],
	}
} };

static struct platform_device *eud7141_devices[] __initdata = {
	&eud7141_physmap_flash,
	&eud7141_phy_devices[0],
	&eud7141_phy_devices[1],
};

/* Configuration based on Futarque-RC signals train. */
lirc_scd_t lirc_scd = {
	.code = 0x3FFFC028,
	.nomtime = 0x1f4,
	.noiserecov = 0,
};

static int __init device_init(void)
{
	u32 bank1_start = emi_bank_base(1);
	u32 bank2_start = emi_bank_base(2);
	struct sysconf_field *sc;
	u32 boot_mode;

	stx7141_configure_ssc(&ssc_private_info);
	stx7141_configure_usb(0);

	stx7141_configure_usb(1);

	if (cpu_data->cut_major > 1)
		stx7141_configure_sata();

#ifdef ENABLE_GMAC0
	/* TODO Never Tested */
	stx7141_configure_ethernet(0, 0, 0, 0);
#endif
	stx7141_configure_ethernet(1, 0, 0, 1);
	stx7141_configure_lirc(&(struct stx7141_lirc_config){
		.rx_mode = stx7141_lirc_rx_mode_ir,
		.scd = &lirc_scd, });

	/* Configure FLASH according to boot device mode pins */
	sc = sysconf_claim(SYS_STA, 1, 16, 17, "boot_mode");
	boot_mode = sysconf_read(sc);
	if (boot_mode == 0x0)
		/* Default configuration */
		pr_info("Configuring FLASH for boot-from-NOR\n");

		/* For boot from NOR place J2 in postion nearest to
		 * front panel
		 *     Fit R64 and remove R57 */

	else if (boot_mode == 0x1) {
		/* Swap NOR/NAND banks */
		pr_info("Configuring FLASH for boot-from-NAND\n");
		/* For boot from NAND place J2 in postion furthest from
		 *  front panel
		 *     Fit R57 qand remove R64 */
		 eud7141_physmap_flash.resource[0].start = bank1_start;
		 eud7141_physmap_flash.resource[0].end = bank2_start - 1;
		 eud7141_nand_config.emi_bank = 0;
	} else
		 pr_info("Unsupported boot mode\n");

	stx7141_configure_nand(&eud7141_nand_config);

	return platform_add_devices(eud7141_devices,
				    ARRAY_SIZE(eud7141_devices));
}
arch_initcall(device_init);

static void __iomem *eud7141_ioport_map(unsigned long port, unsigned int size)
{
	/*
	 * No IO ports on this device, but to allow safe probing pick
	 * somewhere safe to redirect all reads and writes.
	 */
	return (void __iomem *)CCN_PVR;
}

static void __init eud7141_init_irq(void)
{
}

struct sh_machine_vector mv_eud7141 __initmv = {
	.mv_name		= "eud7141",
	.mv_setup		= eud7141_setup,
	.mv_nr_irqs		= NR_IRQS,
	.mv_init_irq		= eud7141_init_irq,
	.mv_ioport_map		= eud7141_ioport_map,
};
