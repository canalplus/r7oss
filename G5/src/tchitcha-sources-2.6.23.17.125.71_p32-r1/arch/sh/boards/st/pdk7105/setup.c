/*
 * arch/sh/boards/st/pdk7105/setup.c
 *
 * Copyright (C) 2008 STMicroelectronics Limited
 * Author: Stuart Menefy (stuart.menefy@st.com)
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 * STMicroelectronics PDK7105-SDK support.
 */

#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/leds.h>
#include <linux/gpio.h>
#include <linux/tm1668.h>
#include <linux/stm/pio.h>
#include <linux/stm/soc.h>
#include <linux/stm/emi.h>
#include <linux/stm/sysconf.h>
#include <linux/delay.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/physmap.h>
#include <linux/mtd/partitions.h>
#include <linux/bpa2.h>
#include <sound/stm.h>
#include <linux/mtd/nand.h>
#include <linux/stm/nand.h>
#include <linux/spi/spi.h>
#include <linux/spi/flash.h>
#include <linux/stm/soc_init.h>
#include <linux/phy.h>
#include <asm/irq-ilc.h>
#include <asm/irl.h>
#include <asm/io.h>
#include "../common/common.h"

static int ascs[4] __initdata = { 0, 1, 2, 3 };

const char *player_buffers[] = { "BPA2_Region0", "BPA2_Region1", "v4l2-video-buffers", NULL };
const char *bigphys[]        = { "bigphysarea", "v4l2-coded-video-buffers", "coredisplay-video", NULL };

#define PDK7105_GPIO_PCI_SERR stpio_to_gpio(15, 4)


/*
 * Flash setup depends on boot-device...
 *
 * Jumper settings (board v1.2-011):
 *
 * boot-from-       | NOR                NAND	            SPI
 * ----------------------------------------------------------------------------
 * JE2 (CS routing) | 0 (EMIA->NOR_CS)   1 (EMIA->NAND_CS)  0
 *                  |   (EMIB->NOR_CS)     (EMIB->NOR_CS)     (EMIB->NOR_CS)
 *                  |   (EMIC->NAND_CS)    (EMIC->NOR_CS)     (EMIC->NAND_CS)
 * JE3 (data width) | 0 (16bit)          1 (8bit)           N/A
 * JE5 (mode 15)    | 0 (boot NOR)       1 (boot NAND)	    0 (boot SPI)
 * JE6 (mode 16)    | 0                  0                  1
 * -----------------------------------------------------------------------------
 *
 */

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

static void __init pdk7105_setup(char** cmdline_p)
{
	printk("STMicroelectronics PDK7105-SDK board initialisation\n");

	stx7105_early_device_init();
	stx7105_configure_asc(ascs, 4, 2);
	bpa2_init(bpa2_parts_table, ARRAY_SIZE(bpa2_parts_table));
}

static struct plat_stm_pwm_data pwm_private_info = {
	.flags		= PLAT_STM_PWM_OUT0,
	.routing	= PWM_OUT0_PIO13_0,
};

static struct plat_ssc_data ssc_private_info = {
	.capability  =
		ssc0_has(SSC_I2C_CAPABILITY | SSC_I2C_PIO) |
		ssc1_has(SSC_SPI_CAPABILITY)               |
		ssc2_has(SSC_I2C_CAPABILITY | SSC_I2C_PIO) |
		ssc3_has(SSC_I2C_CAPABILITY | SSC_I2C_PIO),
	.routing =
		SSC2_SCLK_PIO3_4 | SSC2_MTSR_PIO3_5 |
		SSC3_SCLK_PIO3_6 | SSC3_MTSR_PIO3_7,
	.spi_chipselects = {
		[1] = NULL,	/* Use default PIO chip_select */
	},
};

static struct usb_init_data usb_init[2] __initdata = {
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



static struct platform_device pdk7105_leds = {
	.name = "leds-gpio",
	.id = 0,
	.dev.platform_data = &(struct gpio_led_platform_data) {
		.num_leds = 2,
		.leds = (struct gpio_led[]) {
			/* The schematics actually describes these PIOs
			 * the other way round, but all tested boards
			 * had the bi-colour LED fitted like below... */
			{
				.name = "RED", /* This is also frontpanel LED */
				.gpio = stpio_to_gpio(7, 0),
				.active_low = 1,
			},
			{
				.name = "GREEN",
				.default_trigger = "heartbeat",
				.gpio = stpio_to_gpio(7, 1),
				.active_low = 1,
			},
		},
	},
};

static struct tm1668_key pdk7105_front_panel_keys[] = {
	{ 0x00001000, KEY_UP, "Up (SWF2)" },
	{ 0x00800000, KEY_DOWN, "Down (SWF7)" },
	{ 0x00008000, KEY_LEFT, "Left (SWF6)" },
	{ 0x00000010, KEY_RIGHT, "Right (SWF5)" },
	{ 0x00000080, KEY_ENTER, "Enter (SWF1)" },
	{ 0x00100000, KEY_ESC, "Escape (SWF4)" },
};

static struct tm1668_character pdk7105_front_panel_characters[] = {
	TM1668_7_SEG_HEX_DIGITS,
	TM1668_7_SEG_HEX_DIGITS_WITH_DOT,
	TM1668_7_SEG_SEGMENTS,
};

static struct platform_device pdk7105_front_panel = {
	.name = "tm1668",
	.id = -1,
	.dev.platform_data = &(struct tm1668_platform_data) {
		.gpio_dio = stpio_to_gpio(11, 2),
		.gpio_sclk = stpio_to_gpio(11, 3),
		.gpio_stb = stpio_to_gpio(11, 4),
		.config = tm1668_config_6_digits_12_segments,

		.keys_num = ARRAY_SIZE(pdk7105_front_panel_keys),
		.keys = pdk7105_front_panel_keys,
		.keys_poll_period = DIV_ROUND_UP(HZ, 5),

		.brightness = 8,
		.characters_num = ARRAY_SIZE(pdk7105_front_panel_characters),
		.characters = pdk7105_front_panel_characters,
		.text = "7105",
	},
};



static struct stpio_pin *phy_reset_pin;

static int pdk7105_phy_reset(void* bus)
{
	stpio_set_pin(phy_reset_pin, 0);
	udelay(100);
	stpio_set_pin(phy_reset_pin, 1);

	return 1;
}

static struct plat_stmmacphy_data phy_private_data = {
	/* Micrel */
	.bus_id = 0,
	.phy_addr = -1,
	.phy_mask = 0,
	.interface = PHY_INTERFACE_MODE_MII,
	.phy_reset = &pdk7105_phy_reset,
};

static struct platform_device pdk7105_phy_device = {
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

static struct mtd_partition mtd_parts_table[3] = {
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

static struct physmap_flash_data pdk7105_physmap_flash_data = {
	.width		= 2,
	.set_vpp	= NULL,
	.nr_parts	= ARRAY_SIZE(mtd_parts_table),
	.parts		= mtd_parts_table
};

static struct platform_device pdk7105_physmap_flash = {
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
		.platform_data	= &pdk7105_physmap_flash_data,
	},
};

/* Configuration for Serial Flash */
static struct mtd_partition serialflash_partitions[] = {
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

static struct flash_platform_data serialflash_data = {
	.name = "m25p80",
	.parts = serialflash_partitions,
	.nr_parts = ARRAY_SIZE(serialflash_partitions),
	.type = "m25p32",	/* Check device on individual board */
};

static struct spi_board_info spi_serialflash[] =  {
	{
		.modalias       = "m25p80",
		.bus_num        = 0,
		.chip_select    = spi_set_cs(2, 4),
		.max_speed_hz   = 5000000,
		.platform_data  = &serialflash_data,
		.mode           = SPI_MODE_3,
	},
};

/* Configuration for NAND Flash */
static struct mtd_partition nand_parts[] = {
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

static struct plat_stmnand_data nand_config = {
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
static struct platform_device nand_device =
STM_NAND_DEVICE("stm-nand-flex", 2, &nand_config,
		nand_parts, ARRAY_SIZE(nand_parts), NAND_USE_FLASH_BBT);



static struct platform_device *pdk7105_devices[] __initdata = {
	&pdk7105_physmap_flash,
	&pdk7105_leds,
	&pdk7105_front_panel,
	&pdk7105_phy_device,
};

/* Configuration based on Futarque-RC signals train. */
static lirc_scd_t lirc_scd = {
	.code = 0x3FFFC028,
	.nomtime = 0x1f4,
	.noiserecov = 0,
};

/* PCI configuration */
static struct pci_config_data pdk7105_pci_config = {
	.pci_irq = {
		PCI_PIN_DEFAULT,
		PCI_PIN_DEFAULT,
		PCI_PIN_UNUSED,
		PCI_PIN_UNUSED
	},
	.serr_irq = PCI_PIN_UNUSED, /* Modified in device_init() */
	.idsel_lo = 30,
	.idsel_hi = 30,
	.req_gnt = {
		PCI_PIN_DEFAULT,
		PCI_PIN_UNUSED,
		PCI_PIN_UNUSED,
		PCI_PIN_UNUSED
	},
	.pci_clk = 33333333,
	.pci_reset_gpio = stpio_to_gpio(15, 7),
};

int pcibios_map_platform_irq(struct pci_dev *dev, u8 slot, u8 pin)
{
	/* We can use the standard function on this board */
	return stx7105_pcibios_map_platform_irq(&pdk7105_pci_config, pin);
}

static int __init device_init(void)
{
	u32 bank1_start;
	u32 bank2_start;
	struct sysconf_field *sc;
	u32 boot_mode;

	bank1_start = emi_bank_base(1);
	bank2_start = emi_bank_base(2);

	/* Configure FLASH according to boot device mode pins */
	sc = sysconf_claim(SYS_STA, 1, 15, 16, "boot_mode");
	boot_mode = sysconf_read(sc);
	switch (boot_mode) {
	case 0x0:
		/* Boot-from-NOR */
		pr_info("Configuring FLASH for boot-from-NOR\n");
		pdk7105_physmap_flash.resource[0].start = 0x00000000;
		pdk7105_physmap_flash.resource[0].end = bank1_start - 1;
		nand_device.id = 2;
		break;
	case 0x1:
		/* Boot-from-NAND */
		pr_info("Configuring FLASH for boot-from-NAND\n");
		pdk7105_physmap_flash.resource[0].start = bank1_start;
		pdk7105_physmap_flash.resource[0].end = bank2_start - 1;
		nand_device.id = 0;
		break;
	case 0x2:
		/* Boot-from-SPI */
		pr_info("Configuring FLASH for boot-from-SPI\n");
		pdk7105_physmap_flash.resource[0].start = bank1_start;
		pdk7105_physmap_flash.resource[0].end = bank2_start - 1;
		nand_device.id = 2;
		break;
	}

	/* Setup the PCI_SERR# PIO */
	if (gpio_request(PDK7105_GPIO_PCI_SERR, "PCI_SERR#") == 0) {
		gpio_direction_input(PDK7105_GPIO_PCI_SERR);
		pdk7105_pci_config.serr_irq =
				gpio_to_irq(PDK7105_GPIO_PCI_SERR);
		set_irq_type(pdk7105_pci_config.serr_irq, IRQ_TYPE_LEVEL_LOW);
	} else {
		printk(KERN_WARNING "pdk7105: Failed to claim PCI SERR PIO!\n");
	}
	stx7105_configure_pci(&pdk7105_pci_config);

	stx7105_configure_sata(0);
	stx7105_configure_pwm(&pwm_private_info);
	stx7105_configure_ssc(&ssc_private_info);

	/*
	 * Note that USB port configuration depends on jumper
	 * settings:
	 *		  PORT 0  SW		PORT 1	SW
	 *		+----------------------------------------
	 * OC	normal	|  4[4]	J5A 2-3		 4[6]	J10A 2-3
	 *	alt	| 12[5]	J5A 1-2		14[6]	J10A 1-2
	 * PWR	normal	|  4[5]	J5B 2-3		 4[7]	J10B 2-3
	 *	alt	| 12[6]	J5B 1-2		14[7]	J10B 1-2
	 */

	stx7105_configure_usb(0, &usb_init[0]);
	stx7105_configure_usb(1, &usb_init[1]);

	phy_reset_pin = stpio_request_set_pin(15, 5, "eth_phy_reset",
					      STPIO_OUT, 1);
	stx7105_configure_ethernet(0, stx7105_ethernet_mii, 0, 0, 0, 0);
	stx7105_configure_lirc(&lirc_scd);
	stx7105_configure_audio_pins(0, 0, 1, 0);

	/*
	 * FLASH_WP is shared by NOR and NAND.  However, since MTD NAND has no
	 * concept of WP/VPP, we must permanently enable it
	 */
	stpio_request_set_pin(6, 4, "FLASH_WP", STPIO_OUT, 1);

	stx7105_configure_nand(&nand_device);

	/* The serial FLASH device may be driven by PIO15[0-3] (SPIboot
	 * interface) or PIO2[4-6] (SSC0).  The two sets of PIOs are connected
	 * via 3k3 resistor.  Here we are using the SSC.  Therefore to avoid
	 * drive contention, the SPIboot PIOs should be configured as inputs. */
	stpio_request_set_pin(15, 0, "SPIBoot CLK", STPIO_IN, 0);
	stpio_request_set_pin(15, 1, "SPIBoot DOUT", STPIO_IN, 0);
	stpio_request_set_pin(15, 2, "SPIBoot NOTCS", STPIO_IN, 0);
	stpio_request_set_pin(15, 3, "SPIBoot DIN", STPIO_IN, 0);

	spi_register_board_info(spi_serialflash, ARRAY_SIZE(spi_serialflash));
	
	return platform_add_devices(pdk7105_devices, ARRAY_SIZE(pdk7105_devices));
}
arch_initcall(device_init);

static void __iomem *pdk7105_ioport_map(unsigned long port, unsigned int size)
{
	/* Shouldn't be here! */
	BUG();
	return NULL;
}

struct sh_machine_vector mv_pdk7105 __initmv = {
	.mv_name = "pdk7105",
	.mv_setup = pdk7105_setup,
	.mv_nr_irqs = NR_IRQS,
	.mv_ioport_map = pdk7105_ioport_map,
	STM_PCI_IO_MACHINE_VEC
};
