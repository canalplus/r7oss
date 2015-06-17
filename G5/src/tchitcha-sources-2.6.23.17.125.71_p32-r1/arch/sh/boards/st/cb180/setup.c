/*
 * arch/sh/boards/st/cb180/setup.c
 *
 * Copyright (C) 2008 STMicroelectronics Limited
 * Author: Stuart Menefy (stuart.menefy@st.com)
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 * Fuba board cb180 support.
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
#include <linux/stm/sysconf.h>

/* define OVERRIDE_MTDPARTS only, if you want to fix the
 * flash partitions in kernel. In general this setting will
 * be controlled by bootloader in kernel command line */

/* #define OVERRIDE_MTDPARTS */


static int ascs[2] __initdata = { 2|((STASC_FLAG_NORTSCTS | STASC_FLAG_ASC2_PIO12 )<<8), 1|(STASC_FLAG_NORTSCTS<<8) };

/* taken from MB680 board setup */
const char *LMI_SYS_partalias[] = { "BPA2_Region0", "v4l2-coded-video-buffers",
		"BPA2_Region1", "v4l2-video-buffers" ,"coredisplay-video", NULL };

static struct bpa2_partition_desc bpa2_parts_table[] = {
	{
		.name  = "LMI_SYS",
		.start = 0x44400000,
		.size  = 0x04400000,
		.flags = 0,
		.aka   = LMI_SYS_partalias
	},
	{
		.name  = "bigphysarea",
		.start = 0x48800000,
		.size  = 0x01800000,
		.flags = 0,
		.aka   = NULL
	},
	{
		.name  = "gfx-memory-1",
		.start = 0x4a000000,
		.size  = 0x02000000,
		.flags = 0,
		.aka   = NULL
	},
	{
		.name  = "gfx-memory-0",
		.start = 0x4c000000,
		.size  = 0x04000000,
		.flags = 0,
		.aka   = NULL
	},
};

static void __init dk263790_7105_setup(char** cmdline_p)
{
	int uart2_pio12_enables = 3;
	struct sysconf_field *sc;

	printk("cb180 board initialisation\n");

	stx7105_early_device_init();
	stx7105_configure_asc(ascs, 2, 0);

	/* CFG07[1] = UART2_RXD_SRC_SELECT = 1 */
	/* CFG07[2] = UART2_CTS_SRC_SELECT = 1 */
//#ifndef TSOUT
	sc = sysconf_claim(SYS_CFG, 7, 1, 2, "UART2 Config");
	sysconf_write(sc, sysconf_read(sc) | uart2_pio12_enables);
	bpa2_init(bpa2_parts_table, ARRAY_SIZE(bpa2_parts_table));
}

static struct plat_ssc_data ssc_private_info = {
	.capability  =
		ssc0_has(SSC_SPI_CAPABILITY) |
		ssc1_has(SSC_I2C_CAPABILITY) |
		ssc2_has(SSC_UNCONFIGURED) |
		ssc3_has(SSC_I2C_CAPABILITY),
	.routing =
		SSC3_SCLK_PIO13_2 | SSC3_MTSR_PIO13_3 | SSC3_MRST_PIO13_3,
};

#ifdef OVERRIDE_MTDPARTS
static struct mtd_partition mtd_parts_table[9] = {
	{
		.name = "Boot firmware",
		.size = 0x00060000,
		.offset = 0x00000000,
	}, {
		.name = "bootld original",
		.size = 0x00080000,
		.offset = 0x00060000,
	}, {
		.name = "bootld copy",
		.size = 0x00080000,
		.offset = 0x000E0000,
	}, {
		.name = "Kernel",
		.size = 0x00320000,
		.offset = 0x00160000,
	}, {
		.name = "Root FS",
		.size = 0x00A00000,
		.offset = 0x00480000,
	}, {
		.name = "var",
		.size = 0x00400000,
		.offset = 0x00E80000,
	}, {
		.name = "user_data",
		.size = 0x00400000,
		.offset = 0x01280000,
	}, {
		.name = "delphi",
		.size = MTDPART_SIZ_FULL,
		.offset = 0x01680000,
	}, {
		.name = "full_flash",
		.size = 0x04000000,
		.offset = 0x00000000,
	}
};
#endif /* OVERRIDE_MTDPARTS */

static struct usb_init_data usb_init[2] __initdata = {
	{
		.oc_en = 0,
		.oc_actlow = 0,
		.oc_pinsel = 0,
		.pwr_en = 0,
		.pwr_pinsel = 0,
	}, {
		.oc_en = 0,
		.oc_actlow = 0,
		.oc_pinsel = USB1_OC_PIO14_6,
		.pwr_en = 0,
		.pwr_pinsel = USB1_PWR_PIO14_7,
	}
};

#ifdef OVERRIDE_MTDPARTS
static struct physmap_flash_data physmap_flash_data = {
	.width		= 2,
	.nr_parts	= ARRAY_SIZE(mtd_parts_table),
	.parts		= mtd_parts_table
};
#else
static struct physmap_flash_data physmap_flash_data = {
	.width		= 2,
	.nr_parts	= 0,
	.parts		= NULL
};
#endif

static struct platform_device physmap_flash = {
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
static struct stpio_pin *phy_reset_pin;

static int dk263790_7105_phy_reset(void* bus)
{
	stpio_set_pin(phy_reset_pin, 0);
	udelay(100);
	stpio_set_pin(phy_reset_pin, 1);

	return 1;
}

static struct plat_stmmacphy_data phy_private_data = {
	/* Micrel */
	.bus_id = 0,
	.phy_addr = 0,
	.phy_mask = 0,
	.interface = PHY_INTERFACE_MODE_MII,
	.phy_reset = &dk263790_7105_phy_reset,
};

static struct platform_device dk263790_7105_phy_device = {
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

static struct platform_device *dk263790_7105_devices[] __initdata = {
	&physmap_flash,
	&dk263790_7105_phy_device,
};

/* Configuration based on Futarque-RC signals train. */
lirc_scd_t lirc_scd = {
	.code = 0x3FFFC028,
	.codelen = 0x1e,
	.alt_codelen = 0,
	.nomtime = 0x1f4,
	.noiserecov = 0,
};

static struct {
	u8 sys_cfg;
	u8 alt_max;
} stx710x_pio_sysconf_data[] = {
	[0] = { 19, 5 },
	[1] = { 20, 4 },
	[2] = { 21, 4 },
	[3] = { 25, 4 },
	[4] = { 34, 4 },
	[5] = { 35, 4 },
	[6] = { 36, 4 },
	[7] = { 37, 4 },
	[8] = { 46, 3 },
	[9] = { 47, 4 },
	[10] = { 39, 2 },
	[11] = { 53, 4 },
	[12] = { 48, 5 },
	[13] = { 49, 5 },
	[14] = { 0, 1 },
	[15] = { 50, 4 },
	[16] = { 54, 2 },
};

static void __init stx710x_pio_sysconf(int port, int pin, int alt,
		const char *name)
{
	struct sysconf_field *sc;
	int sys_cfg, alt_max;

   //printk("stx710x_pio_sysconf(%d, %d, %d, '%s')\n", port, pin, alt, name);

	BUG_ON(port < 0 || port >= ARRAY_SIZE(stx710x_pio_sysconf_data));
	BUG_ON(pin < 0 || pin > 7);

	if (port == 14 && alt == 2)
		alt = 1;

	sys_cfg = stx710x_pio_sysconf_data[port].sys_cfg;
	alt_max = stx710x_pio_sysconf_data[port].alt_max;

	if ((sys_cfg == 0) || (alt == -1))
		return;

	BUG_ON(alt < 1 || alt > alt_max);

	alt--;

	if (alt_max > 1) {
		sc = sysconf_claim(SYS_CFG, sys_cfg, pin, pin, name);
		BUG_ON(!sc);
		sysconf_write(sc, alt & 1);
	}

	if (alt_max > 2) {
		sc = sysconf_claim(SYS_CFG, sys_cfg, pin + 8, pin + 8, name);
		BUG_ON(!sc);
		sysconf_write(sc, (alt >> 1) & 1);
	}

	if (alt_max > 4) {
		sc = sysconf_claim(SYS_CFG, sys_cfg, pin + 16, pin + 16, name);
		BUG_ON(!sc);
		sysconf_write(sc, (alt >> 2) & 1);
	}
}

static void __init configure_dvout(void)
{
	static struct stpio_pin *piopin;
	int pin;

	/* PIO 0 */
	for (pin=0; pin<8; pin++)
	{
		stx710x_pio_sysconf(0, pin, 2, "dvo_r");
		piopin = stpio_request_pin(0, pin, "dvo_r", STPIO_ALT_OUT);
	}
	/* PIO 3 */
	for (pin=4; pin<8; pin++)
	{
		stx710x_pio_sysconf(3, pin, 1, "dvo_g");
		piopin = stpio_request_pin(3, pin, "dvo_g", STPIO_ALT_OUT);
	}
	/* PIO 4 */
	for (pin=0; pin<4; pin++)
	{
		stx710x_pio_sysconf(4, pin, 1, "dvo_g");
		piopin = stpio_request_pin(4, pin, "dvo_g", STPIO_ALT_OUT);
	}
	for (pin=4; pin<8; pin++)
	{
		stx710x_pio_sysconf(4, pin, 1, "dvo_b");
		piopin = stpio_request_pin(4, pin, "dvo_b", STPIO_ALT_OUT);
	}
	/* PIO 5 */
	for (pin=0; pin<4; pin++)
	{
		stx710x_pio_sysconf(5, pin, 1, "dvo_b");
		piopin = stpio_request_pin(5, pin, "dvo_b", STPIO_ALT_OUT);
	}
	stx710x_pio_sysconf(5, 4, 1, "dvo_hsync");
	piopin = stpio_request_pin(5, 4, "dvo_hsync", STPIO_ALT_OUT);
	stx710x_pio_sysconf(5, 5, 1, "dvo_clk");
	piopin = stpio_request_pin(5, 5, "dvo_clk", STPIO_ALT_OUT);
	stx710x_pio_sysconf(5, 6, 1, "dvo_vsync");
	piopin = stpio_request_pin(5, 6, "dvo_vsync", STPIO_ALT_OUT);
	stx710x_pio_sysconf(5, 7, 1, "dvo_data");
	piopin = stpio_request_pin(5, 7, "dvo_data", STPIO_ALT_OUT);
}

static void __init configure_tsin(void)
{
	static struct stpio_pin *pin;
	struct sysconf_field *sc;
	
	stx710x_pio_sysconf(13, 4, 1, "tsin0serdata");	
	pin = stpio_request_pin(13, 4, "tsin0serdata", STPIO_IN);
	stx710x_pio_sysconf(13, 5, 1, "tsin0btclkin");	
	pin = stpio_request_pin(13, 5, "tsin0btclkin", STPIO_IN);
	stx710x_pio_sysconf(13, 6, 1, "tsin0valid");	
	pin = stpio_request_pin(13, 6, "tsin0valid", STPIO_IN);
	stx710x_pio_sysconf(13, 7, 1, "tsin0error");	
	pin = stpio_request_pin(13, 7, "tsin0error", STPIO_IN);
	stx710x_pio_sysconf(14, 0, 1, "tsin0pkclk");	
	pin = stpio_request_pin(14, 0, "tsin0pkclk", STPIO_IN);
	
	sc = sysconf_claim(SYS_CFG, 4, 10, 10, "tsin2sel");
	sysconf_write(sc, 1); /* TSin2 is from PIO14 */
	
	stx710x_pio_sysconf(14, 1, 2, "tsin1serdata");	
	pin = stpio_request_pin(14, 1, "tsin1serdata", STPIO_IN);
	stx710x_pio_sysconf(14, 2, 2, "tsin1btclkin");	
	pin = stpio_request_pin(14, 2, "tsin1btclkin", STPIO_IN);
	stx710x_pio_sysconf(14, 3, 2, "tsin1valid");	
	pin = stpio_request_pin(14, 3, "tsin1valid", STPIO_IN);
	stx710x_pio_sysconf(14, 4, 2, "tsin1error");	
	pin = stpio_request_pin(14, 4, "tsin1error", STPIO_IN);
	stx710x_pio_sysconf(14, 5, 2, "tsin1pkclk");	
	pin = stpio_request_pin(14, 5, "tsin1pkclk", STPIO_IN);
	
	stx710x_pio_sysconf(12, 5, 4, "tsin2serdata");	
	pin = stpio_request_pin(12, 5, "tsin2serdata", STPIO_IN);
	stx710x_pio_sysconf(12, 6, 4, "tsin2btclkin");	
	pin = stpio_request_pin(12, 6, "tsin2btclkin", STPIO_IN);
	stx710x_pio_sysconf(12, 7, 4, "tsin2valid");	
	pin = stpio_request_pin(12, 7, "tsin2valid", STPIO_IN);
	stx710x_pio_sysconf(13, 0, 4, "tsin2error");	
	pin = stpio_request_pin(13, 0, "tsin2error", STPIO_IN);
	stx710x_pio_sysconf(13, 1, 4, "tsin2pkclk");	
	pin = stpio_request_pin(13, 1, "tsin2pkclk", STPIO_IN);
//#ifdef TSOUT
   /* nur zum Test: TSOUT aktivieren! */
/*	sc = sysconf_claim(SYS_CFG, 7, 1, 2, "UART2 Config");
	sysconf_write(sc, 0);
	stx710x_pio_sysconf(12, 0, 2, "tsoutserdata");	
	pin = stpio_request_pin(12, 0, "tsoutserdata", STPIO_ALT_OUT);
	stx710x_pio_sysconf(12, 1, 2, "tsoutbtclkin");	
	pin = stpio_request_pin(12, 1, "tsoutbtclkin", STPIO_ALT_OUT);
	stx710x_pio_sysconf(12, 2, 2, "tsoutvalid");	
	pin = stpio_request_pin(12, 2, "tsoutvalid", STPIO_ALT_OUT);
	stx710x_pio_sysconf(12, 3, 2, "tsouterror");	
	pin = stpio_request_pin(12, 3, "tsouterror", STPIO_ALT_OUT);
	stx710x_pio_sysconf(12, 4, 2, "tsoutpkclk");	
	pin = stpio_request_pin(12, 4, "tsoutpkclk", STPIO_ALT_OUT);
*/
}

static int __init device_init(void)
{
	stx7105_configure_ssc(&ssc_private_info);

	/*
	 * Note that USB ports are located on mainboard
	 * and debugboard/daughterboard:
	 *		  PORT 0  SW		PORT 1	SW
	 *		+----------------------------------------
	 *		  Debugboard 		Mainboard
	 */

	stx7105_configure_usb(0, &usb_init[0]);
	stx7105_configure_usb(1, &usb_init[1]);

	phy_reset_pin = stpio_request_set_pin(7, 3, "eth_phy_reset",
					      STPIO_OUT, 1);
	stx7105_configure_ethernet(0, stx7105_ethernet_mii, 0, 0, 0, 0);
	stx7105_configure_lirc(&lirc_scd);

	stx7105_configure_audio_pins(3, 0, 1, 0);

	configure_tsin();

//	configure_dvout();

	return platform_add_devices(dk263790_7105_devices, ARRAY_SIZE(dk263790_7105_devices));
}
arch_initcall(device_init);

static void __iomem *dk263790_7105_ioport_map(unsigned long port, unsigned int size)
{
	/* Shouldn't be here! */
	BUG();
	return NULL;
}

struct sh_machine_vector mv_dk263790_7105 __initmv = {
	.mv_name		= "cb180",
	.mv_setup		= dk263790_7105_setup,
	.mv_nr_irqs		= NR_IRQS,
	.mv_ioport_map		= dk263790_7105_ioport_map,
	STM_PCI_IO_MACHINE_VEC
};
