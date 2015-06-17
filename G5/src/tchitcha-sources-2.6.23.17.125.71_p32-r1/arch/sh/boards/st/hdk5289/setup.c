/*
 * arch/sh/boards/st/hdk5289/setup.c
 *
 * Copyright (C) 2009 STMicroelectronics Limited
 * Author: Pawel Moll (pawel.moll@st.com)
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 */

#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/phy.h>
#include <linux/gpio.h>
#include <linux/tm1668.h>
#include <linux/mtd/physmap.h>
#include <linux/mtd/partitions.h>
#include <linux/mtd/nand.h>
#include <linux/stm/nand.h>
#include <linux/stm/emi.h>
#include <linux/stm/sysconf.h>
#include <linux/stm/pio.h>
#include <linux/stm/soc.h>
#include <linux/stm/soc_init.h>
#include <linux/stm/emi.h>
#include "../common/common.h"

#define HDK5289_GPIO_RST stpio_to_gpio(2, 2)
#define HDK5289_POWER_ON stpio_to_gpio(2, 3)

static int hdk5289_ascs[] __initdata = { 2 | (STASC_FLAG_NORTSCTS << 8) };

/*
 * FLASH setup depends on board boot configuration:
 *
 *				boot-from-
 * Boot Device		NOR		NAND		SPI
 * ----------------     -----		-----		-----
 * JE3-2 (Mode15)	0 (W)		1 (E)		0 (W)
 * JE3-1 (Mode16)	0 (W)		0 (W)		1 (E)
 *
 * Word Size
 * ----------------
 * JE2-2 (Mode13)	0 (W)		1 (E)		x
 *
 *
 * CS-Routing
 * ----------------
 * JH1			NOR (N)		NAND (S)	NOR (N)
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

static void __init hdk5289_setup(char **cmdline_p)
{
	printk(KERN_INFO "STMicroelectronics HDK7105 board initialisation\n");

	stx5206_early_device_init();
	stx5206_configure_asc(hdk5289_ascs, ARRAY_SIZE(hdk5289_ascs), 0);
}

static struct plat_ssc_data hdk5289_ssc_plat_data = {
	.capability =
		ssc0_has(SSC_I2C_CAPABILITY) | /* Internal I2C link */
		ssc1_has(SSC_I2C_CAPABILITY) |
		ssc2_has(SSC_UNCONFIGURED) |
		ssc3_has(SSC_I2C_CAPABILITY),
};



static struct tm1668_key hdk5289_front_panel_keys[] = {
	{ 0x00001000, KEY_UP, "Up (SWF2)" },
	{ 0x00800000, KEY_DOWN, "Down (SWF7)" },
	{ 0x00008000, KEY_LEFT, "Left (SWF6)" },
	{ 0x00000010, KEY_RIGHT, "Right (SWF5)" },
	{ 0x00000080, KEY_ENTER, "Enter (SWF1)" },
	{ 0x00100000, KEY_ESC, "Escape (SWF4)" },
};

static struct tm1668_character hdk5289_front_panel_characters[] = {
	TM1668_7_SEG_HEX_DIGITS,
	TM1668_7_SEG_HEX_DIGITS_WITH_DOT,
	TM1668_7_SEG_SEGMENTS,
};

static struct platform_device hdk5289_front_panel = {
	.name = "tm1668",
	.id = -1,
	.dev.platform_data = &(struct tm1668_platform_data) {
		.gpio_dio = stpio_to_gpio(3, 5),
		.gpio_sclk = stpio_to_gpio(3, 4),
		.gpio_stb = stpio_to_gpio(2, 7),
		.config = tm1668_config_6_digits_12_segments,

		.keys_num = ARRAY_SIZE(hdk5289_front_panel_keys),
		.keys = hdk5289_front_panel_keys,
		.keys_poll_period = DIV_ROUND_UP(HZ, 5),

		.brightness = 8,
		.characters_num = ARRAY_SIZE(hdk5289_front_panel_characters),
		.characters = hdk5289_front_panel_characters,
		.text = "5289",
	},
};



static struct plat_stmmacphy_data hdk5289_phy_plat_data = {
	/* Micrel */
	.bus_id = 0,
	.phy_addr = -1,
	.phy_mask = 0,
	.interface = PHY_INTERFACE_MODE_MII,
};

static struct platform_device hdk5289_phy_device = {
	.name = "stmmacphy",
	.id = -1,
	.dev.platform_data = &hdk5289_phy_plat_data,
};


static struct mtd_partition hdk5289_physmap_flash_partitions[3] = {
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

static struct physmap_flash_data hdk5289_physmap_flash_data = {
	.width		= 2,
	.set_vpp	= NULL,
	.nr_parts	= ARRAY_SIZE(hdk5289_physmap_flash_partitions),
	.parts		= hdk5289_physmap_flash_partitions,
};

static struct platform_device hdk5289_physmap_flash = {
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
		.platform_data	= &hdk5289_physmap_flash_data,
	},
};

static struct mtd_partition hdk5289_spifsm_partitions[] = {
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

static struct stm_plat_spifsm_data hdk5289_spifsm_data = {
	.name = "w25q64",
	.parts = hdk5289_spifsm_partitions,
	.nr_parts = ARRAY_SIZE(hdk5289_spifsm_partitions),
};


static struct mtd_partition hdk5289_nand_flash_partitions[] = {
	{
		.name   = "NAND root",
		.offset = 0,
		.size   = 0x00800000
	}, {
		.name   = "NAND home",
		.offset = MTDPART_OFS_APPEND,
		.size   = MTDPART_SIZ_FULL
	},
};

static struct plat_stmnand_data hdk5289_nand_flash_data = {
	/* STM_NAND_EMI data */
	.emi_withinbankoffset   = 0,
	.rbn_port               = -1,
	.rbn_pin                = -1,

	.timing_data = &(struct nand_timing_data) {
		.sig_setup      = 50,           /* times in ns */
		.sig_hold       = 50,
		.CE_deassert    = 0,
		.WE_to_RBn      = 100,
		.wr_on          = 10,
		.wr_off         = 40,
		.rd_on          = 10,
		.rd_off         = 40,
		.chip_delay     = 50,           /* in us */
	},
	.flex_rbn_connected     = 1,
};

/* Platform data for STM_NAND_EMI/FLEX/AFM. (bank# may be updated later) */
static struct platform_device hdk5289_nand_flash_device =
STM_NAND_DEVICE("stm-nand-flex", 1, &hdk5289_nand_flash_data,
		hdk5289_nand_flash_partitions,
		ARRAY_SIZE(hdk5289_nand_flash_partitions),
		NAND_USE_FLASH_BBT);



static struct platform_device *hdk5289_devices[] __initdata = {
	&hdk5289_front_panel,
	&hdk5289_phy_device,
};

static int __init device_init(void)
{
	struct sysconf_field *sc;
	u32 boot_mode;

	/* Configure FLASH according to boot device mode pins */
	/*    Note, MODE[15-16] seems to be SYS_STA_1[16-17]! */
	sc = sysconf_claim(SYS_STA, 1, 16, 17, "boot_mode");
	boot_mode = sysconf_read(sc);
	sysconf_release(sc);
	switch (boot_mode) {
	case 0x0:
		/* Boot-from-NOR */
		pr_info("Configuring FLASH for boot-from-NOR\n");
		hdk5289_physmap_flash.resource[0].start = 0x00000000;
		hdk5289_physmap_flash.resource[0].end = emi_bank_base(1) - 1;
		hdk5289_nand_flash_device.id = 1;
		platform_device_register(&hdk5289_physmap_flash);
		break;
	case 0x1:
		/* Boot-from-NAND */
		pr_info("Configuring FLASH for boot-from-NAND\n");
		hdk5289_physmap_flash.resource[0].start = emi_bank_base(1);
		hdk5289_physmap_flash.resource[0].end = emi_bank_base(2) - 1;
		hdk5289_nand_flash_device.id = 0;
		platform_device_register(&hdk5289_physmap_flash);
		break;
	case 0x2:
		/* Boot-from-SPI */
		pr_info("Configuring FLASH for boot-from-SPI\n");
		hdk5289_physmap_flash.resource[0].start = 0;
		hdk5289_physmap_flash.resource[0].end = 0;
		hdk5289_nand_flash_device.id = 1;
		break;
	}


	/* This board has a one PIO line used to reset almost everything:
	 * ethernet PHY, HDMI transmitter chip, PCI... */
	gpio_request(HDK5289_GPIO_RST, "GPIO_RST#");
	gpio_direction_output(HDK5289_GPIO_RST, 0);
	udelay(1000);
	gpio_set_value(HDK5289_GPIO_RST, 1);

	/* Some of the peripherals are powered by regulators
	 * triggered by the following PIO line... */
	gpio_request(HDK5289_POWER_ON, "POWER_ON");
	gpio_direction_output(HDK5289_POWER_ON, 1);

	stx5206_configure_lirc(stx5206_lirc_rx_ir, 0, 0, NULL);
	stx5206_configure_ssc(&hdk5289_ssc_plat_data);

	stx5206_configure_usb();

	stx5206_configure_ethernet(stx5206_ethernet_mii, 0, 0);

	stx5206_configure_nand(&hdk5289_nand_flash_device);

	stx5206_configure_spifsm(&hdk5289_spifsm_data);

	stx5289_configure_mmc();

	return platform_add_devices(hdk5289_devices,
			ARRAY_SIZE(hdk5289_devices));
}
arch_initcall(device_init);

struct sh_machine_vector mv_hdk5289 __initmv = {
	.mv_name		= "hdk5289",
	.mv_setup		= hdk5289_setup,
	.mv_nr_irqs		= NR_IRQS,
};

