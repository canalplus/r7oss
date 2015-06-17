/*
 * arch/sh/boards/st/hdk7106/setup.c
 *
 * Copyright (C) 2009 STMicroelectronics Limited
 * Author: Pawel Moll (pawel.moll@st.com)
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 */

#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/phy.h>
#include <linux/gpio.h>
#include <linux/leds.h>
#include <linux/tm1668.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/physmap.h>
#include <linux/mtd/partitions.h>
#include <linux/bpa2.h>
#include <linux/mtd/nand.h>
#include <linux/spi/spi.h>
#include <linux/spi/flash.h>
#include <linux/stm/nand.h>
#include <linux/stm/emi.h>
#include <linux/stm/soc.h>
#include <linux/stm/soc_init.h>
#include <linux/stm/pio.h>
#include <linux/stm/sysconf.h>
#include "../common/common.h"

/*
 * FLASH setup depends on board boot configuration:
 *
 *				boot-from-
 * Mode Pins		NOR		NAND		SPI
 * ----------------     -----		-----		-----
 * JE8-1 (Mode5)	0 (E)		1 (W)		0 (E)
 * JE8-2 (Mode6)	0 (E)		0 (E)		1 (W)
 * JE6-1 (Mode7)	0 (W)		0 (W)		0 (W)
 * JE6-2 (Mode8)	0 (W)		1 (E)		1 (E)
 *
 * CS-Routing
 * ----------------
 * JE2			1-2 (N)		2-3 (S)		1-2 (N)
 * JE11			1-2 (N)		2-3 (S)		1-2 (N)
 *
 * Post-boot Access
 * ----------------
 * NOR (limit)		EMIA (64MB)	EMIB (8MB)	X
 * NAND			EMIB/FLEXB	FLEXA		EMIB/FLEXB
 * SPI			SPIFSM		SPIFSM		SPIFSM
 *
 * Switch positions given in terms of (N)orth, (E)ast, (S)outh, and (W)est,
 * viewing board with LED display South and power connector North.
 *
 */



#define HDK7106_GPIO_POWER_ON_ETHERNET stpio_to_gpio(4, 2)
#define HDK7106_GPIO_POWER_ON stpio_to_gpio(4, 3)

const char *player_buffers[] = { "BPA2_Region0", "BPA2_Region1", "v4l2-video-buffers", NULL };
const char *bigphys[]        = { "bigphysarea", "v4l2-coded-video-buffers", "coredisplay-video", NULL };

static int hdk7106_ascs[] __initdata = { 2 | (STASC_FLAG_NORTSCTS << 8) };

static struct bpa2_partition_desc bpa2_parts_table[] = {
	{
		.name  = "Player_Buffers", //96MB
		.start = 0,
		.size  = 0x07000000,
		.flags = 0,
		.aka   = player_buffers
	},
	{
		.name  = "Bigphys_Buffers", //64MB
		.start = 0,
		.size  = 0x04000000,      
		.flags = 0,
		.aka   = bigphys
	},
	{
		.name  = "gfx-memory-0",  //64MB
		.start = 0x58000000,
		.size  = 0x04000000,
		.flags = 0,
		.aka   = NULL
	},
	{
		.name  = "gfx-memory-1",  //64MB
		.start = 0x5c000000,
		.size  = 0x04000000,
		.flags = 0,
		.aka   = NULL
	}
};

static void __init hdk7106_setup(char **cmdline_p)
{
	printk(KERN_INFO "STMicroelectronics HDK7106 board initialisation\n");

	stx7105_early_device_init();
	stx7105_configure_asc(hdk7106_ascs, ARRAY_SIZE(hdk7106_ascs), 0);
	bpa2_init(bpa2_parts_table, ARRAY_SIZE(bpa2_parts_table));
}



static struct plat_ssc_data hdk7106_ssc_plat_data = {
	.capability =
		ssc0_has(SSC_I2C_CAPABILITY) |
		ssc1_has(SSC_I2C_CAPABILITY) |
		ssc2_has(SSC_UNCONFIGURED) |
		ssc3_has(SSC_I2C_CAPABILITY),
	.routing =
		SSC2_SCLK_PIO3_4 | SSC2_MTSR_PIO3_5 |
		SSC3_SCLK_PIO3_6 | SSC3_MTSR_PIO3_7,
};

static struct usb_init_data hdk7106_usb_init[2] __initdata = {
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



static struct platform_device hdk7106_leds = {
	.name = "leds-gpio",
	.id = -1,
	.dev.platform_data = &(struct gpio_led_platform_data) {
		.num_leds = 1,
		.leds = (struct gpio_led[]) {
			{
				.name = "FP green",
				.default_trigger = "heartbeat",
				.gpio = stpio_to_gpio(3, 1),
				.active_low = 1,
			},
		},
	},
};

static struct tm1668_key hdk7106_front_panel_keys[] = {
	{ 0x00001000, KEY_UP, "Up (SWF2)" },
	{ 0x00800000, KEY_DOWN, "Down (SWF7)" },
	{ 0x00008000, KEY_LEFT, "Left (SWF6)" },
	{ 0x00000010, KEY_RIGHT, "Right (SWF5)" },
	{ 0x00000080, KEY_ENTER, "Enter (SWF1)" },
	{ 0x00100000, KEY_ESC, "Escape (SWF4)" },
};

static struct tm1668_character hdk7106_front_panel_characters[] = {
	TM1668_7_SEG_HEX_DIGITS,
	TM1668_7_SEG_HEX_DIGITS_WITH_DOT,
	TM1668_7_SEG_SEGMENTS,
};

static struct platform_device hdk7106_front_panel = {
	.name = "tm1668",
	.id = -1,
	.dev.platform_data = &(struct tm1668_platform_data) {
		.gpio_dio = stpio_to_gpio(5, 1),
		.gpio_sclk = stpio_to_gpio(5, 0),
		.gpio_stb = stpio_to_gpio(5, 2),
		.config = tm1668_config_6_digits_12_segments,

		.keys_num = ARRAY_SIZE(hdk7106_front_panel_keys),
		.keys = hdk7106_front_panel_keys,
		.keys_poll_period = DIV_ROUND_UP(HZ, 5),

		.brightness = 8,
		.characters_num = ARRAY_SIZE(hdk7106_front_panel_characters),
		.characters = hdk7106_front_panel_characters,
		.text = "7106",
	},
};



/* PCI configuration */
static struct pci_config_data hdk7106_pci_config = {
	.pci_irq = {
		PCI_PIN_DEFAULT,
		PCI_PIN_DEFAULT,
		PCI_PIN_UNUSED,
		PCI_PIN_UNUSED
	},
	.serr_irq = PCI_PIN_UNUSED, /* SERR PIO is shared with MII1_CRS */
	.idsel_lo = 30,
	.idsel_hi = 30,
	.req_gnt = {
		PCI_PIN_DEFAULT,
		PCI_PIN_UNUSED,
		PCI_PIN_UNUSED,
		PCI_PIN_UNUSED
	},
	.pci_clk = 33333333,
	.pci_reset_gpio = stpio_to_gpio(15, 7)
};

int pcibios_map_platform_irq(struct pci_dev *dev, u8 slot, u8 pin)
{
	/* We can use the standard function on this board */
	return stx7105_pcibios_map_platform_irq(&hdk7106_pci_config, pin);
}



static int hdk7106_phy_reset(void *bus)
{
	gpio_set_value(HDK7106_GPIO_POWER_ON_ETHERNET, 0);
	udelay(10000); /* 10 miliseconds is enough for everyone ;-) */
	gpio_set_value(HDK7106_GPIO_POWER_ON_ETHERNET, 1);

	return 1;
}

static struct platform_device hdk7106_phy_devices[] = {
	{ /* On-board ICplus IP1001 */
		.name = "stmmacphy",
		.id = 0,
		.dev.platform_data = &(struct  plat_stmmacphy_data) {
			.bus_id = 0,
			.phy_addr = 1,
			.phy_mask = 0,
			.interface = PHY_INTERFACE_MODE_MII,
			.phy_reset = &hdk7106_phy_reset,
		},
	}, { /* MII connector JN6 */
		.name = "stmmacphy",
		.id = 1,
		.dev.platform_data = &(struct  plat_stmmacphy_data) {
			.bus_id = 1,
			.phy_addr = -1,
			.phy_mask = 0,
			.interface = PHY_INTERFACE_MODE_MII,
			.phy_reset = &hdk7106_phy_reset,
		},
	},
};



static struct mtd_partition hdk7106_nor_flash_parts[] = {
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

static struct platform_device hdk7106_nor_flash = {
	.name = "physmap-flash",
	.id = -1,
	.num_resources = 1,
	.resource = (struct resource[]) {
		{
			.start = 0, /* modified in device_init() */
			.end = (64 * 1024 * 1024) - 1,
			.flags = IORESOURCE_MEM,
		}
	},
	.dev.platform_data = &(struct physmap_flash_data) {
		.width = 2,
		.nr_parts = ARRAY_SIZE(hdk7106_nor_flash_parts),
		.parts = hdk7106_nor_flash_parts,
	},
};



/* Configuration for Serial Flash */
static struct mtd_partition hdk7106_spi_flash_parts[] = {
	{
		.name = "SFLASH_1",
		.size = 0x00080000,
		.offset = 0,
	}, {
		.name = "SFLASH_2",
		.size = MTDPART_SIZ_FULL,
		.offset = MTDPART_OFS_NXTBLK,
	},
};

static struct stm_plat_spifsm_data hdk7106_spifsm_data = {
	.name = "m25p32",
	.parts = hdk7106_spi_flash_parts,
	.nr_parts = ARRAY_SIZE(hdk7106_spi_flash_parts),
};

/* Configuration for NAND Flash */
static struct mtd_partition hdk7106_nand_flash_parts[] = {
	{
		.name = "NAND root",
		.offset = 0,
		.size = 0x00800000
	}, {
		.name = "NAND home",
		.offset = MTDPART_OFS_APPEND,
		.size = MTDPART_SIZ_FULL
	},
};

static struct plat_stmnand_data hdk7106_nand_flash_config = {
	/* STM_NAND_EMI data */
	.emi_withinbankoffset = 0,
	.rbn_port = -1,
	.rbn_pin = -1,

	.timing_data = &(struct nand_timing_data) {
		.sig_setup = 50, /* times in ns */
		.sig_hold = 50,
		.CE_deassert = 0,
		.WE_to_RBn = 100,
		.wr_on = 10,
		.wr_off = 40,
		.rd_on = 10,
		.rd_off = 40,
		.chip_delay = 50, /* in us */
	},
	.flex_rbn_connected = 1,
};

/* Platform data for STM_NAND_EMI/FLEX/AFM. (bank# may be updated later) */
static struct platform_device hdk7106_nand_flash =
STM_NAND_DEVICE("stm-nand-flex", 1, &hdk7106_nand_flash_config,
		hdk7106_nand_flash_parts, ARRAY_SIZE(hdk7106_nand_flash_parts),
		NAND_USE_FLASH_BBT);



static struct platform_device *hdk7106_devices[] __initdata = {
	&hdk7106_leds,
	&hdk7106_front_panel,
	&hdk7106_phy_devices[0],
	&hdk7106_phy_devices[1],
};

static int __init device_init(void)
{
	u32 boot_mode;
	struct sysconf_field *sc;

	/* Configure flash according to selected boot device */
	sc = sysconf_claim(SYS_STA, 1, 5, 8, "boot_mode");
	boot_mode = sysconf_read(sc);
	if (boot_mode == 0 || boot_mode == 5) {
		/* boot-from-NOR */
		printk(KERN_INFO "Configuring FLASH for boot-from-NOR\n");
		hdk7106_nor_flash.resource[0].start = 0x00000000;
		hdk7106_nor_flash.resource[0].end = emi_bank_base(1) - 1;
		hdk7106_nand_flash.id = 1;
		platform_device_register(&hdk7106_nor_flash);
	} else if ((boot_mode > 0 && boot_mode < 5) ||
			(boot_mode > 5 && boot_mode < 10)) {
		/* boot-from-NAND */
		printk(KERN_INFO "Configuring FLASH for boot-from-NAND\n");
		hdk7106_nor_flash.resource[0].start = emi_bank_base(1);
		hdk7106_nor_flash.resource[0].end = emi_bank_base(2) - 1;
		hdk7106_nand_flash.id = 0;
		platform_device_register(&hdk7106_nor_flash);
	} else if (boot_mode == 10 || boot_mode == 11) {
		/* boot-from-SPI */
		printk(KERN_INFO "Configuring FLASH for boot-from-SPI\n");
		hdk7106_nor_flash.resource[0].start = 0;
		hdk7106_nor_flash.resource[0].end = 0;
		hdk7106_nand_flash.id = 1;
	}
	sysconf_release(sc);

	/* The "POWER_ON_ETH" line should be rather called "PHY_RESET",
	 * but it isn't... ;-) */
	gpio_request(HDK7106_GPIO_POWER_ON_ETHERNET, "POWER_ON_ETHERNET");
	gpio_direction_output(HDK7106_GPIO_POWER_ON_ETHERNET, 0);

	/* Some of the peripherals are powered by regulators
	 * triggered by the following PIO line... */
	gpio_request(HDK7106_GPIO_POWER_ON, "POWER_ON");
	gpio_direction_output(HDK7106_GPIO_POWER_ON, 1);

	stx7105_configure_lirc(NULL);
	stx7105_configure_ssc(&hdk7106_ssc_plat_data);

	stx7105_configure_usb(0, &hdk7106_usb_init[0]);
	stx7105_configure_usb(1, &hdk7106_usb_init[1]);

	stx7105_configure_pci(&hdk7106_pci_config);

	stx7105_configure_sata(0);
	stx7105_configure_sata(1);

	stx7105_configure_audio_pins(0, 0, 1, 0);

#if 1
	stx7105_configure_ethernet(0, stx7105_ethernet_mii, 0, 0, 0, 0);
#else
	stx7105_configure_ethernet(1, stx7105_ethernet_mii, 1, 1, 0,
			STX7105_ETHERNET_MII1_MDIO_3_4 |
			STX7105_ETHERNET_MII1_MDC_3_5);
#endif

	stx7105_configure_nand(&hdk7106_nand_flash);
	stx7106_configure_spifsm(&hdk7106_spifsm_data);

	/* HW note:
	 * There is and error on the 7106-HDK MMC daughter board V 1.0
	 * schematics.
	 * The STMPS2151STR Pin 4 should be 'EN' and the Pin 5 should be
	 * 'Power_IN'. These are reverse.
	 */
	stx7106_configure_mmc();

	return platform_add_devices(hdk7106_devices,
			ARRAY_SIZE(hdk7106_devices));
}
arch_initcall(device_init);


static void __iomem *hdk7106_ioport_map(unsigned long port, unsigned int size)
{
	/* Shouldn't be here! */
	BUG();
	return NULL;
}

struct sh_machine_vector mv_hdk7106 __initmv = {
	.mv_name = "hdk7106",
	.mv_setup = hdk7106_setup,
	.mv_nr_irqs = NR_IRQS,
	.mv_ioport_map = hdk7106_ioport_map,
	STM_PCI_IO_MACHINE_VEC
};

